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

  GtkWidget          *list_box;
  GtkWidget          *no_events_row;
  GtkWidget          *scrolled_window;

  /* property */
  GDateTime          *date;
  GcalContext        *context;

  GcalRangeTree      *events;
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
  PROP_CONTEXT,
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

static ChildData*
child_data_new (GtkWidget      *widget,
                GcalEvent      *event,
                GcalAgendaView *self)
{
  ChildData *data;

  data = g_new (ChildData, 1);
  data->widget = widget;
  data->event = g_object_ref (event);
  data->self = self;

  return data;
}

static void
child_data_free (gpointer data)
{
  ChildData *child_data = data;

  if (!child_data)
    return;

  if (child_data->widget && child_data->self)
    {
      gtk_list_box_remove (GTK_LIST_BOX (child_data->self->list_box), child_data->widget);
      child_data->widget = NULL;
      child_data->self = NULL;
    }
  g_clear_object (&child_data->event);
  g_free (child_data);
}

static GDateTime *
get_start_date_for_row (GcalAgendaView *self,
                        GtkListBoxRow  *row)
{
  GtkWidget *widget = gtk_list_box_row_get_child (row);

  if (GCAL_IS_EVENT_WIDGET (widget))
    {
      GDateTime *date = gcal_event_widget_get_date_start (GCAL_EVENT_WIDGET (widget));

      /* Pretend events start at least on the selected date */
      if (gcal_date_time_compare_date (date, self->date) > 0)
        return date;
      else
        return self->date;
    }


  return self->date;
}

static GDateTime *
get_end_date_for_row (GcalAgendaView *self,
                      GtkListBoxRow  *row)
{
  GtkWidget *widget = gtk_list_box_row_get_child (row);

  if (GCAL_IS_EVENT_WIDGET (widget))
    return gcal_event_widget_get_date_end (GCAL_EVENT_WIDGET (widget));

  return self->date;
}

static gboolean
get_row_is_multiday (GtkListBoxRow *row)
{
  GtkWidget *widget = gtk_list_box_row_get_child (row);

  if (GCAL_IS_EVENT_WIDGET (widget))
    {
      GcalEvent *event = gcal_event_widget_get_event (GCAL_EVENT_WIDGET (widget));

      return gcal_event_is_multiday (event);
    }

  return FALSE;
}

static gboolean
get_row_is_all_day (GtkListBoxRow *row)
{
  GtkWidget *widget = gtk_list_box_row_get_child (row);

  if (GCAL_IS_EVENT_WIDGET (widget))
    {
      GcalEvent *event = gcal_event_widget_get_event (GCAL_EVENT_WIDGET (widget));

      return gcal_event_get_all_day (event);
    }

  return FALSE;
}

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

/* Auxiliary methods */
static void
update_no_events_row (GcalAgendaView *self)
{
  gboolean visible = !self->events_on_date;
  gboolean was_visible = gtk_widget_get_visible (self->no_events_row);

  if (visible != was_visible)
    {
      gtk_widget_set_visible (self->no_events_row, visible);
      gtk_list_box_invalidate_headers (GTK_LIST_BOX (self->list_box));
    }
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

static int
sort_func (GtkListBoxRow *row1,
           GtkListBoxRow *row2,
           gpointer       user_data)
{
  GcalAgendaView *self = GCAL_AGENDA_VIEW (user_data);
  gboolean multiday1, multiday2;
  gboolean all_day1, all_day2;
  int result;

  result = g_date_time_compare (get_start_date_for_row (self, row1),
                                get_start_date_for_row (self, row2));

  /* Sort multi day events before multi single day events of the same day. */
  if (result != 0)
    return result;

  multiday1 = get_row_is_multiday (row1);
  multiday2 = get_row_is_multiday (row2);

  if (multiday1 != multiday2)
    return multiday1 ? -1 : 1;

  all_day1 = get_row_is_all_day (row1);
  all_day2 = get_row_is_all_day (row2);

  if (all_day1 != all_day2)
    return all_day1 ? -1 : 1;

  return g_date_time_compare (get_end_date_for_row (self, row1),
                              get_end_date_for_row (self, row2));
}

static void
update_header (GtkListBoxRow *row,
               GtkListBoxRow *before,
               gpointer       user_data)
{
  GcalAgendaView *self = GCAL_AGENDA_VIEW (user_data);
  GDateTime *start = get_start_date_for_row (self, row);
  GDateTime *end = get_end_date_for_row (self, row);
  GtkWidget *widget = gtk_list_box_row_get_child (row);
  GcalEvent *event = NULL;
  g_autofree gchar *label = NULL;
  GtkWidget *header;
  g_autoptr (GDateTime) today = NULL;
  g_autoptr (GDateTime) end_midnight = NULL;

  if (GCAL_IS_EVENT_WIDGET (widget))
    event = gcal_event_widget_get_event (GCAL_EVENT_WIDGET (widget));

  today = g_date_time_new_now_local ();

  if (before)
    {
      GDateTime *before_start = get_start_date_for_row (self, before);
      GDateTime *before_end = get_end_date_for_row (self, before);
      gboolean before_all_day = get_row_is_all_day (before);
      gboolean event_all_day = get_row_is_all_day (row);

      if (get_row_is_multiday (row) &&
          get_row_is_multiday (before) &&
          gcal_date_time_compare_date (start, before_start) == 0 &&
          gcal_date_time_compare_date (start, today) == 0)
        {
          gtk_list_box_row_set_header (row, NULL);
          return;
        }

      end_midnight = event_all_day ? g_date_time_ref (end) : g_date_time_add_seconds (end, -1);

      if (gcal_date_time_compare_date (start, before_start) == 0 &&
          gcal_date_time_compare_date (end_midnight, before_end) == 0)
        {
          gtk_list_box_row_set_header (row, NULL);
          return;
        }

      /* All day events don't need to check for end date */
      if (event &&
          (before_all_day || event_all_day) &&
          !get_row_is_multiday (before) &&
          gcal_date_time_compare_date (start, before_start) == 0)
        {
          gtk_list_box_row_set_header (row, NULL);
          return;
        }
    }

  if (get_row_is_multiday (row))
    {
      g_autofree gchar *start_label = NULL;
      g_autofree gchar *end_label = NULL;
      if (gcal_date_time_compare_date (start, today) == 0)
        {
          label = g_strdup (_("On-going"));
        }
      else
        {
          start_label = new_date_header_string (start);
          end_label = new_date_header_string (end);

          label = g_strdup_printf ("%s – %s", start_label, end_label);
        }
    }
  else
    {
      label = new_date_header_string (start);
    }

  header = g_object_new (GTK_TYPE_LABEL,
                         "label", label,
                         "xalign", 0.0f,
                         NULL);
  gtk_widget_add_css_class (header, "caption-heading");

  gtk_list_box_row_set_header (row, header);
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

static inline gboolean
count_events_on_data (GcalRange *range,
                      gpointer   data,
                      gpointer   user_data)
{
  ChildData *child_data = data;
  GcalAgendaView *self = user_data;

  if (gcal_date_time_compare_date (self->date, gcal_event_get_date_start (child_data->event)) == 0)
    self->events_on_date++;

  return FALSE;
}

static void
gcal_agenda_view_set_date (GcalView  *view,
                           GDateTime *date)
{
  GcalAgendaView *self = GCAL_AGENDA_VIEW (view);

  GCAL_ENTRY;

  gcal_set_date_time (&self->date, date);

  schedule_position_scroll (self);

  self->events_on_date = 0;
  gcal_range_tree_traverse (self->events, G_IN_ORDER, count_events_on_data, self);
  update_no_events_row (self);

  gcal_timeline_subscriber_range_changed (GCAL_TIMELINE_SUBSCRIBER (view));

  if (self->events_on_date == 0)
    gtk_list_box_invalidate_sort (GTK_LIST_BOX (self->list_box));

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
on_event_widget_activated_cb (GcalEventWidget *event_widget,
                              GcalAgendaView  *self)
{
  gcal_view_event_activated (GCAL_VIEW (self), event_widget);
}

static void
on_list_box_row_activated_cb (GtkListBox     *list_box,
                              GtkListBoxRow  *row,
                              GcalAgendaView *self)
{
  GtkWidget *child = gtk_list_box_row_get_child (row);

  g_assert (GCAL_IS_EVENT_WIDGET (child));

  gcal_view_event_activated (GCAL_VIEW (self), GCAL_EVENT_WIDGET (child));
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
  GcalAgendaView *self = GCAL_AGENDA_VIEW (subscriber);
  GtkWidget *widget, *row;
  GcalTimestampPolicy timestamp_policy;

  GCAL_ENTRY;

  /* FIXME Check gcal_event_is_multiday (event) and gcal_event_get_all_day (event) */

  g_object_ref (event);

  if (gcal_date_time_compare_date (self->date, gcal_event_get_date_start (event)) == 0)
    {
      self->events_on_date++;
      update_no_events_row (self);
    }

  /* Create and add the new event widget */
  if (gcal_event_get_all_day (event) || gcal_event_is_multiday (event))
    timestamp_policy = GCAL_TIMESTAMP_POLICY_END;
  else
    timestamp_policy = GCAL_TIMESTAMP_POLICY_START;

  widget = g_object_new (GCAL_TYPE_EVENT_WIDGET,
                         "context", self->context,
                         "event", event,
                         "orientation", GTK_ORIENTATION_VERTICAL,
                         "timestamp-policy", timestamp_policy,
                         "focusable", FALSE,
                         NULL);

  row = g_object_new (GTK_TYPE_LIST_BOX_ROW,
                      "child", widget,
                      NULL);

  gcal_range_tree_add_range (self->events,
                             gcal_event_get_range (event),
                             child_data_new (row, event, self));

  gtk_accessible_update_property (GTK_ACCESSIBLE (row),
                                  GTK_ACCESSIBLE_PROPERTY_HAS_POPUP, TRUE,
                                  -1);

  gtk_accessible_update_relation (GTK_ACCESSIBLE (row),
                                  GTK_ACCESSIBLE_RELATION_LABELLED_BY, widget, NULL,
                                  -1);

  g_signal_connect (widget, "activate", G_CALLBACK (on_event_widget_activated_cb), self);

  gtk_list_box_append (GTK_LIST_BOX (self->list_box), row);

  gtk_list_box_invalidate_headers (GTK_LIST_BOX (self->list_box));

  GCAL_EXIT;
}

static void
gcal_agenda_view_remove_event (GcalTimelineSubscriber *subscriber,
                               GcalEvent              *event)
{
  GcalAgendaView *self = GCAL_AGENDA_VIEW (subscriber);
  const gchar *uid = gcal_event_get_uid (event);
  GPtrArray *widgets;
  guint i;

  GCAL_ENTRY;

  widgets = gcal_range_tree_get_all_data (self->events);

  if (gcal_date_time_compare_date (self->date, gcal_event_get_date_start (event)) == 0)
    {
      self->events_on_date--;
      update_no_events_row (self);
    }

  for (i = 0; widgets && i < widgets->len; i++)
    {
      ChildData *data;
      GtkWidget *widget;
      GcalEvent *event;

      data = g_ptr_array_index (widgets, i);
      widget = gtk_list_box_row_get_child (GTK_LIST_BOX_ROW (data->widget));
      event = gcal_event_widget_get_event (GCAL_EVENT_WIDGET (widget));

      if (g_strcmp0 (gcal_event_get_uid (event), uid) != 0)
        continue;

      gcal_range_tree_remove_range (self->events, gcal_event_get_range (event), data);
      gtk_widget_queue_allocate (GTK_WIDGET (self));
    }

  g_clear_pointer (&widgets, g_ptr_array_unref);

  gtk_list_box_invalidate_headers (GTK_LIST_BOX (self->list_box));

  GCAL_EXIT;
}

static void
gcal_agenda_view_update_event (GcalTimelineSubscriber *subscriber,
                               GcalEvent              *old_event,
                               GcalEvent              *event)
{
  GCAL_ENTRY;

  gcal_agenda_view_remove_event (subscriber, old_event);
  gcal_agenda_view_add_event (subscriber, event);

  GCAL_EXIT;
}

static void
gcal_timeline_subscriber_interface_init (GcalTimelineSubscriberInterface *iface)
{
  iface->get_range = gcal_agenda_view_get_range;
  iface->add_event = gcal_agenda_view_add_event;
  iface->update_event = gcal_agenda_view_update_event;
  iface->remove_event = gcal_agenda_view_remove_event;
}


/*
 * GObject overrides
 */

static void
gcal_agenda_view_dispose (GObject *object)
{
  GcalAgendaView *self;

  self = GCAL_AGENDA_VIEW (object);

  g_clear_pointer (&self->events, gcal_range_tree_unref);

  /* Chain up to parent's dispose() method. */
  G_OBJECT_CLASS (gcal_agenda_view_parent_class)->dispose (object);
}

static void
gcal_agenda_view_finalize (GObject       *object)
{
  GcalAgendaView *self;

  self = GCAL_AGENDA_VIEW (object);

  g_clear_pointer (&self->date, g_date_time_unref);

  g_clear_object (&self->context);

  /* Chain up to parent's finalize() method. */
  G_OBJECT_CLASS (gcal_agenda_view_parent_class)->finalize (object);
}

static void
gcal_agenda_view_set_property (GObject      *object,
                               guint         property_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GcalAgendaView *self = (GcalAgendaView *) object;

  switch (property_id)
    {
    case PROP_DATE:
      gcal_view_set_date (GCAL_VIEW (object), g_value_get_boxed (value));
      break;

    case PROP_CONTEXT:
      g_assert (self->context == NULL);
      self->context = g_value_dup_object (value);
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

    case PROP_CONTEXT:
      g_value_set_object (value, self->context);
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

  object_class->dispose = gcal_agenda_view_dispose;
  object_class->finalize = gcal_agenda_view_finalize;
  object_class->set_property = gcal_agenda_view_set_property;
  object_class->get_property = gcal_agenda_view_get_property;

  g_object_class_override_property (object_class, PROP_DATE, "active-date");
  g_object_class_override_property (object_class, PROP_CONTEXT, "context");
  g_object_class_override_property (object_class, PROP_TIME_DIRECTION, "time-direction");

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/views/gcal-agenda-view.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalAgendaView, list_box);
  gtk_widget_class_bind_template_child (widget_class, GcalAgendaView, no_events_row);
  gtk_widget_class_bind_template_child (widget_class, GcalAgendaView, scrolled_window);

  gtk_widget_class_bind_template_callback (widget_class, on_list_box_row_activated_cb);

  gtk_widget_class_set_css_name (widget_class, "agenda-view");
}

static void
gcal_agenda_view_init (GcalAgendaView *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->events = gcal_range_tree_new_with_free_func (child_data_free);

  gtk_list_box_set_sort_func (GTK_LIST_BOX (self->list_box), sort_func, self, NULL);
  gtk_list_box_set_header_func (GTK_LIST_BOX (self->list_box), update_header, self, NULL);
}
