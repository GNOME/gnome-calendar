/* gcal-agenda-view.c
 *
 * Copyright (C) 2022 Purism SPC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#define G_LOG_DOMAIN "GcalAgendaView"

#include "gcal-agenda-view.h"
#include "gcal-agenda-view-day.h"
#include "gcal-agenda-view-item.h"
#include "gcal-debug.h"
#include "gcal-enums.h"
#include "gcal-event-widget.h"
#include "gcal-range-tree.h"
#include "gcal-timeline-subscriber.h"
#include "gcal-utils.h"
#include "gcal-view-private.h"

#include <adwaita.h>
#include <glib/gi18n.h>

#include <math.h>

typedef struct
{
  GtkWidget          *widget;
  GcalEvent          *event;
  GcalAgendaView     *self;
} ChildData;

struct _GcalAgendaView
{
  GtkBox              parent;

  GtkWidget          *scrolled_window;
  GtkListView        *list_view;

  /* property */
  GDateTime          *date;
  GListStore         *days_model;
  GtkFilterListModel *filtered_days;
  GtkFlattenListModel *flatten_model;

  gint                events_on_date;
  gint                clicked_cell;
};

static void          gcal_view_interface_init                    (GcalViewInterface  *iface);

static void          gcal_timeline_subscriber_interface_init     (GcalTimelineSubscriberInterface *iface);

enum
{
  PROP_0,
  PROP_DATE,
  PROP_TIME_DIRECTION,
  N_PROPS,
};


G_DEFINE_TYPE_WITH_CODE (GcalAgendaView, gcal_agenda_view, GTK_TYPE_BOX,
                         G_IMPLEMENT_INTERFACE (GCAL_TYPE_VIEW, gcal_view_interface_init)
                         G_IMPLEMENT_INTERFACE (GCAL_TYPE_TIMELINE_SUBSCRIBER,
                                                gcal_timeline_subscriber_interface_init));

/*
 * Auxiliary methods
 */

static gchar *
new_date_header_string (GDateTime *date)
{
  g_autoptr (GDateTime) today = NULL;
  g_autoptr (GDateTime) tomorrow = NULL;
  g_autoptr (GDateTime) yesterday = NULL;

  if (date == NULL)
    return NULL;

  today = g_date_time_new_now_local ();
  tomorrow = g_date_time_add_days (today, 1);
  yesterday = g_date_time_add_days (today, -1);

  if (gcal_date_time_compare_date (date, today) == 0)
    return g_strdup (_("Today"));
  else if (gcal_date_time_compare_date (date, tomorrow) == 0)
    return g_strdup (_("Tomorrow"));
  else if (gcal_date_time_compare_date (date, yesterday) == 0)
    return g_strdup (_("Yesterday"));
  else
    /*
     * Translators: %A is the full day name, %B is the month name
     * and %d is the day of the month as a number between 0 and 31.
     * More formats can be found on the doc:
     * https://docs.gtk.org/glib/method.DateTime.format.html
     */
    return g_date_time_format (date, _("%A %B %d"));
}

/*
 * Callbacks
 */

static gboolean
n_items_and_date_to_boolean (GcalAgendaViewDay *day,
                             unsigned int       n_items,
                             GDateTime         *date,
                             gpointer           user_data)
{
  GcalAgendaView *self = (GcalAgendaView *) user_data;
  g_autoptr (GcalRange) view_range = NULL;

  g_assert (GCAL_IS_AGENDA_VIEW (self));
  g_assert (GCAL_IS_AGENDA_VIEW_DAY (day));
  g_assert (self->date != NULL);

  view_range = gcal_timeline_subscriber_get_range (GCAL_TIMELINE_SUBSCRIBER (self));
  if (!gcal_range_contains_datetime (view_range, date))
    return FALSE;

  if (n_items > 0)
    return TRUE;

  if (gcal_date_time_compare_date (date, self->date) == 0)
    return TRUE;

  return FALSE;
}

static GcalTimestampPolicy
timestamp_policy_from_event (GtkListItem *item,
                             GcalEvent   *event)
{
  gboolean is_multiday_or_all_day;

  if (!event)
    return GCAL_TIMESTAMP_POLICY_NONE;

  is_multiday_or_all_day = (gcal_event_get_all_day (event) || gcal_event_is_multiday (event));
  return is_multiday_or_all_day ? GCAL_TIMESTAMP_POLICY_END : GCAL_TIMESTAMP_POLICY_START;
}

static GtkOrientation
orientation_from_event (GtkListItem *item,
                        GcalEvent   *event)
{
  gboolean is_multiday_or_all_day = TRUE;

  if (event)
    is_multiday_or_all_day = (gcal_event_get_all_day (event) || gcal_event_is_multiday (event));

  return is_multiday_or_all_day ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL;
}

static void
on_event_widget_activated_cb (GcalEventWidget *event_widget,
                              GtkListItem     *item)
{
  GcalView *view = GCAL_VIEW (gtk_widget_get_ancestor (GTK_WIDGET (event_widget), GCAL_TYPE_VIEW));

  gcal_view_event_activated (view, event_widget);
}


static void
agenda_header_setup_cb (GcalAgendaView           *self,
                        GtkListHeader            *header,
                        GtkSignalListItemFactory *factory)
{
  GtkWidget *row;

  g_assert (GCAL_IS_AGENDA_VIEW (self));

  row = g_object_new (GTK_TYPE_LABEL,
                      "can-focus", FALSE,
                      "xalign", 0.0f,
                      "ellipsize", PANGO_ELLIPSIZE_END,
                      NULL);

  gtk_widget_add_css_class (row, "caption-heading");

  gtk_list_header_set_child (header, row);
}

static void
agenda_header_bind_cb (GcalAgendaView           *self,
                       GtkListHeader            *header,
                       GtkSignalListItemFactory *factory)
{
  gpointer day;

  g_assert (GCAL_IS_AGENDA_VIEW (self));

  day = gtk_flatten_list_model_get_model_for_item (self->flatten_model, gtk_list_header_get_start (header));
  g_assert (day == NULL || GCAL_IS_AGENDA_VIEW_DAY (day));

  if (day != NULL)
    {
      g_autofree char *header_label = NULL;
      GDateTime *date;
      GtkLabel *label;

      date = gcal_agenda_view_day_get_date (GCAL_AGENDA_VIEW_DAY (day));
      header_label = new_date_header_string (date);

      label = GTK_LABEL (gtk_list_header_get_child (header));

      gtk_label_set_label (label, g_strdup (header_label));
    }
}

/*
 * GcalView interface
 */

static GDateTime*
gcal_agenda_view_get_date (GcalView *view)
{
  GcalAgendaView *self = GCAL_AGENDA_VIEW (view);

  return self->date;
}

static void
gcal_agenda_view_set_date (GcalView  *view,
                           GDateTime *date)
{
  GcalAgendaView *self = GCAL_AGENDA_VIEW (view);

  GCAL_ENTRY;

  gcal_set_date_time (&self->date, date);

  for (unsigned int i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (self->days_model)); i++)
    {
      g_autoptr (GcalAgendaViewDay) day = NULL;
      g_autoptr (GDateTime) next_date = NULL;

      day = g_list_model_get_item (G_LIST_MODEL (self->days_model), i);
      next_date = g_date_time_add_days (date, i);

      gcal_agenda_view_day_set_date (day, next_date);
    }

  gtk_list_view_scroll_to (self->list_view, 0, GTK_LIST_SCROLL_FOCUS, NULL);

  gcal_timeline_subscriber_range_changed (GCAL_TIMELINE_SUBSCRIBER (view));

  GCAL_EXIT;
}

static GList*
gcal_agenda_view_get_children_by_uuid (GcalView              *view,
                                       GcalRecurrenceModType  mod,
                                       const gchar           *uuid)
{
  GCAL_ENTRY;

  /* FIXME Not sure what to do here. */

  GCAL_RETURN (NULL);
}

static void
gcal_agenda_view_clear_marks (GcalView *view)
{
  /* FIXME Not sure what to do here. */
}

static GDateTime*
gcal_agenda_view_get_next_date (GcalView *view)
{
  GcalAgendaView *self = GCAL_AGENDA_VIEW (view);

  g_assert (self->date != NULL);
  return g_date_time_add_weeks (self->date, 1);
}


static GDateTime*
gcal_agenda_view_get_previous_date (GcalView *view)
{
  GcalAgendaView *self = GCAL_AGENDA_VIEW (view);

  g_assert (self->date != NULL);
  return g_date_time_add_weeks (self->date, -1);
}

static void
gcal_agenda_view_first_weekday_changed (GcalView *view)
{
  GcalAgendaView *self = GCAL_AGENDA_VIEW (view);

  gtk_list_view_scroll_to (self->list_view, 0, GTK_LIST_SCROLL_FOCUS, NULL);
  gcal_timeline_subscriber_range_changed (GCAL_TIMELINE_SUBSCRIBER (view));
}

static void
gcal_view_interface_init (GcalViewInterface *iface)
{
  iface->get_date = gcal_agenda_view_get_date;
  iface->set_date = gcal_agenda_view_set_date;
  iface->get_children_by_uuid = gcal_agenda_view_get_children_by_uuid;
  iface->clear_marks = gcal_agenda_view_clear_marks;
  iface->get_next_date = gcal_agenda_view_get_next_date;
  iface->get_previous_date = gcal_agenda_view_get_previous_date;
  iface->first_weekday_changed = gcal_agenda_view_first_weekday_changed;
}


/*
 * GcalTimelineSubscriber iface
 */

static GcalRange*
gcal_agenda_view_get_range (GcalTimelineSubscriber *subscriber)
{
  GcalAgendaView *self = GCAL_AGENDA_VIEW (subscriber);

  return gcal_range_new_take (g_date_time_ref (self->date),
                              gcal_date_time_get_end_of_week (self->date),
                              GCAL_RANGE_DEFAULT);
}

static void
gcal_agenda_view_set_model (GcalTimelineSubscriber *subscriber,
                            GListModel             *model)
{
  GcalAgendaView *self;

  GCAL_ENTRY;

  self = GCAL_AGENDA_VIEW (subscriber);

  for (unsigned int i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (self->days_model)); i++)
    {
      g_autoptr (GcalAgendaViewDay) day = g_list_model_get_item (G_LIST_MODEL (self->days_model), i);

      gcal_agenda_view_day_set_model (day, model);
    }

  GCAL_EXIT;
}

static void
gcal_timeline_subscriber_interface_init (GcalTimelineSubscriberInterface *iface)
{
  iface->get_range = gcal_agenda_view_get_range;
  iface->set_model = gcal_agenda_view_set_model;
}


/*
 * GObject overrides
 */

static void
gcal_agenda_view_dispose (GObject *object)
{
  GcalAgendaView *self = GCAL_AGENDA_VIEW (object);

  gtk_widget_dispose_template (GTK_WIDGET (self), GCAL_TYPE_AGENDA_VIEW);

  G_OBJECT_CLASS (gcal_agenda_view_parent_class)->dispose (object);
}

static void
gcal_agenda_view_finalize (GObject       *object)
{
  GcalAgendaView *self = GCAL_AGENDA_VIEW (object);

  g_clear_pointer (&self->date, g_date_time_unref);

  /* Chain up to parent's finalize() method. */
  G_OBJECT_CLASS (gcal_agenda_view_parent_class)->finalize (object);
}

static void
gcal_agenda_view_set_property (GObject      *object,
                               guint         property_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  switch (property_id)
    {
    case PROP_DATE:
      gcal_view_set_date (GCAL_VIEW (object), g_value_get_boxed (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gcal_agenda_view_get_property (GObject    *object,
                               guint       property_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GcalAgendaView *self;

  g_return_if_fail (GCAL_IS_AGENDA_VIEW (object));
  self = GCAL_AGENDA_VIEW (object);

  switch (property_id)
    {
    case PROP_DATE:
      g_value_set_boxed (value, self->date);
      break;

    case PROP_TIME_DIRECTION:
      g_value_set_enum (value, GTK_ORIENTATION_VERTICAL);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gcal_agenda_view_class_init (GcalAgendaViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  g_type_ensure (GCAL_TYPE_AGENDA_VIEW_ITEM);

  object_class->dispose = gcal_agenda_view_dispose;
  object_class->finalize = gcal_agenda_view_finalize;
  object_class->set_property = gcal_agenda_view_set_property;
  object_class->get_property = gcal_agenda_view_get_property;

  g_object_class_override_property (object_class, PROP_DATE, "active-date");
  g_object_class_override_property (object_class, PROP_TIME_DIRECTION, "time-direction");

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/views/gcal-agenda-view.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalAgendaView, flatten_model);
  gtk_widget_class_bind_template_child (widget_class, GcalAgendaView, filtered_days);
  gtk_widget_class_bind_template_child (widget_class, GcalAgendaView, scrolled_window);
  gtk_widget_class_bind_template_child (widget_class, GcalAgendaView, list_view);

  gtk_widget_class_bind_template_callback (widget_class, agenda_header_setup_cb);
  gtk_widget_class_bind_template_callback (widget_class, agenda_header_bind_cb);
  gtk_widget_class_bind_template_callback (widget_class, n_items_and_date_to_boolean);
  gtk_widget_class_bind_template_callback (widget_class, on_event_widget_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, orientation_from_event);
  gtk_widget_class_bind_template_callback (widget_class, timestamp_policy_from_event);

  gtk_widget_class_set_css_name (widget_class, "agenda-view");
}

static void
gcal_agenda_view_init (GcalAgendaView *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->date = g_date_time_new_now_local ();
  self->days_model = g_list_store_new (GCAL_TYPE_AGENDA_VIEW_DAY);

  for (size_t i = 0; i < GCAL_N_WEEKDAYS; i++)
    {
      g_autoptr (GcalAgendaViewDay) day = NULL;
      g_autoptr (GDateTime) date = NULL;

      date = g_date_time_add_days (self->date, i);

      day = gcal_agenda_view_day_new ();
      gcal_agenda_view_day_set_date (day, date);

      g_list_store_append (self->days_model, day);
    }

  gtk_filter_list_model_set_model (self->filtered_days, G_LIST_MODEL (self->days_model));
}
