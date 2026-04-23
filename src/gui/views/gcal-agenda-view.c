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
#include "gcal-agenda-view-day-row.h"
#include "gcal-debug.h"
#include "gcal-enums.h"
#include "gcal-event-widget.h"
#include "gcal-range-tree.h"
#include "gcal-timeline-subscriber.h"
#include "gcal-utils.h"
#include "gcal-view-private.h"

#include <adwaita.h>

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

  /* property */
  GDateTime          *date;
  GListStore         *days_model;
  GtkFilterListModel *filtered_days;

  guint               scroll_grid_timeout_id;
  gulong              stack_page_changed_id;

  gint                events_on_date;
  gint                clicked_cell;
};

static void          schedule_position_scroll                    (GcalAgendaView       *self);

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


/*
 * Callbacks
 */

static void
stack_visible_child_changed_cb (AdwViewStack   *stack,
                                GParamSpec     *pspec,
                                GcalAgendaView *self)
{
  if (adw_view_stack_get_visible_child (stack) != (GtkWidget*) self)
    return;

  schedule_position_scroll (self);

  g_clear_signal_handler (&self->stack_page_changed_id, stack);
}

static gboolean
update_grid_scroll_position (GcalAgendaView *self)
{
  g_autoptr(GDateTime) week_start = NULL;
  g_autoptr(GDateTime) week_end = NULL;
  g_autoptr(GDateTime) now = NULL;
  GtkAdjustment *vadjustment;
  gdouble minutes, real_value;
  gdouble max, page, page_increment, value;
  gboolean dummy;

  /* While the scrolled window is not mapped, we keep waiting */
  if (!gtk_widget_get_realized (self->scrolled_window) ||
      !gtk_widget_get_mapped (self->scrolled_window))
    {
      if (self->stack_page_changed_id == 0)
        {
          GtkWidget *stack = gtk_widget_get_ancestor (GTK_WIDGET (self), ADW_TYPE_VIEW_STACK);

          self->stack_page_changed_id = g_signal_connect_object (stack,
                                                                 "notify::visible-child",
                                                                 G_CALLBACK (stack_visible_child_changed_cb),
                                                                 self,
                                                                 0);
        }

      self->scroll_grid_timeout_id = 0;

      GCAL_RETURN (G_SOURCE_REMOVE);
    }

  now = g_date_time_new_now_local ();
  week_start = gcal_date_time_get_start_of_week (self->date);
  week_end = gcal_date_time_get_end_of_week (self->date);

  /* Don't animate when not today */
  if (gcal_date_time_compare_date (now, week_start) < 0 || gcal_date_time_compare_date (now, week_end) >= 0)
    GCAL_GOTO (out);

  vadjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (self->scrolled_window));
  minutes = g_date_time_get_hour (now) * 60 + g_date_time_get_minute (now);
  page = gtk_adjustment_get_page_size (vadjustment);
  max = gtk_adjustment_get_upper (vadjustment);

  real_value = max / MINUTES_PER_DAY * minutes - (page / 2.0);
  page_increment = gtk_adjustment_get_page_increment (vadjustment);
  value = gtk_adjustment_get_value (vadjustment);

  gtk_adjustment_set_page_increment (vadjustment, real_value - value);

  g_signal_emit_by_name (self->scrolled_window,
                         "scroll-child",
                         GTK_SCROLL_PAGE_FORWARD,
                         FALSE,
                         &dummy);

  gtk_adjustment_set_page_increment (vadjustment, page_increment);

out:
  self->scroll_grid_timeout_id = 0;
  GCAL_RETURN (G_SOURCE_REMOVE);
}

static void
schedule_position_scroll (GcalAgendaView *self)
{
  /* Nothing is scheduled, we should do it now */
  if (self->scroll_grid_timeout_id > 0)
    g_source_remove (self->scroll_grid_timeout_id);

  self->scroll_grid_timeout_id = g_timeout_add (200,
                                                (GSourceFunc) update_grid_scroll_position,
                                                self);
}

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

  schedule_position_scroll (self);

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
gcal_view_interface_init (GcalViewInterface *iface)
{
  iface->get_date = gcal_agenda_view_get_date;
  iface->set_date = gcal_agenda_view_set_date;
  iface->get_children_by_uuid = gcal_agenda_view_get_children_by_uuid;
  iface->clear_marks = gcal_agenda_view_clear_marks;
  iface->get_next_date = gcal_agenda_view_get_next_date;
  iface->get_previous_date = gcal_agenda_view_get_previous_date;
}


/*
 * GcalTimelineSubscriber iface
 */

static void
on_day_row_event_activated_cb (GcalAgendaViewDayRow *day_row,
                               GcalEventWidget      *event_widget,
                               gpointer              user_data)
{
  GcalAgendaView *self;

  self = GCAL_AGENDA_VIEW (gtk_widget_get_ancestor (GTK_WIDGET (day_row), GCAL_TYPE_AGENDA_VIEW));
  g_assert (GCAL_IS_AGENDA_VIEW (self));

  gcal_view_event_activated (GCAL_VIEW (self), event_widget);
}

static GcalRange*
gcal_agenda_view_get_range (GcalTimelineSubscriber *subscriber)
{
  GcalAgendaView *self = GCAL_AGENDA_VIEW (subscriber);

  return gcal_range_new_take (g_date_time_ref (self->date),
                              gcal_date_time_get_end_of_week (self->date),
                              GCAL_RANGE_DEFAULT);
}

static void
gcal_agenda_view_add_event (GcalTimelineSubscriber *subscriber,
                            GcalEvent              *event)
{
}

static void
gcal_agenda_view_remove_event (GcalTimelineSubscriber *subscriber,
                               GcalEvent              *event)
{
}

static void
gcal_agenda_view_update_event (GcalTimelineSubscriber *subscriber,
                               GcalEvent              *old_event,
                               GcalEvent              *event)
{
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
  iface->add_event = gcal_agenda_view_add_event;
  iface->update_event = gcal_agenda_view_update_event;
  iface->remove_event = gcal_agenda_view_remove_event;
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

  g_type_ensure (GCAL_TYPE_AGENDA_VIEW_DAY_ROW);

  object_class->dispose = gcal_agenda_view_dispose;
  object_class->finalize = gcal_agenda_view_finalize;
  object_class->set_property = gcal_agenda_view_set_property;
  object_class->get_property = gcal_agenda_view_get_property;

  g_object_class_override_property (object_class, PROP_DATE, "active-date");
  g_object_class_override_property (object_class, PROP_TIME_DIRECTION, "time-direction");

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/views/gcal-agenda-view.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalAgendaView, filtered_days);
  gtk_widget_class_bind_template_child (widget_class, GcalAgendaView, scrolled_window);

  gtk_widget_class_bind_template_callback (widget_class, n_items_and_date_to_boolean);
  gtk_widget_class_bind_template_callback (widget_class, on_day_row_event_activated_cb);

  gtk_widget_class_set_css_name (widget_class, "agenda-view");
}

static void
gcal_agenda_view_init (GcalAgendaView *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->date = g_date_time_new_now_local ();
  self->days_model = g_list_store_new (GCAL_TYPE_AGENDA_VIEW_DAY);

  for (size_t i = 0; i < N_WEEKDAYS; i++)
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
