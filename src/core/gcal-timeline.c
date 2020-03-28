/* gcal-timeline.c
 *
 * Copyright 2020 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "GcalTimeline"

#include "gcal-calendar.h"
#include "gcal-calendar-monitor.h"
#include "gcal-context.h"
#include "gcal-date-time-utils.h"
#include "gcal-debug.h"
#include "gcal-event.h"
#include "gcal-range-tree.h"
#include "gcal-timeline.h"
#include "gcal-timeline-subscriber.h"

#include <libedataserver/libedataserver.h>

typedef struct
{
  GDateTime          *range_start;
  GDateTime          *range_end;
} SubscriberData;

struct _GcalTimeline
{
  GObject             parent_instance;

  gulong              update_range_idle_id;
  GDateTime          *range_start;
  GDateTime          *range_end;

  GcalRangeTree      *events;
  gchar              *filter;

  GHashTable         *calendars; /* GcalCalendar* -> GcalCalendarMonitor* */
  gint                completed_calendars;

  GHashTable         *subscribers; /* GcalTimelineSubscriber* -> SubscriberData* */
  GcalRangeTree      *subscriber_ranges;

  GCancellable       *cancellable;

  GcalContext        *context;
};

G_DEFINE_TYPE (GcalTimeline, gcal_timeline, G_TYPE_OBJECT)

enum
{
  PROP_0,
  PROP_COMPLETE,
  PROP_CONTEXT,
  PROP_FILTER,
  N_PROPS
};

static GParamSpec *properties [N_PROPS] = { NULL, };


/*
 * Auxiliary methods
 */

static inline gboolean
is_timeline_complete (GcalTimeline *self)
{
  return self->completed_calendars == g_hash_table_size (self->calendars);
}

static void
reset_completed_calendars (GcalTimeline *self)
{
  gboolean was_complete = is_timeline_complete (self);

  self->completed_calendars = 0;
  if (is_timeline_complete (self) != was_complete)
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_COMPLETE]);
}

static void
increase_completed_calendars (GcalTimeline *self)
{
  self->completed_calendars++;

  g_assert (self->completed_calendars <= g_hash_table_size (self->calendars));

  if (is_timeline_complete (self))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_COMPLETE]);
}

static SubscriberData*
subscriber_data_new (GcalTimelineSubscriber *subscriber)
{
  SubscriberData *subscriber_data;

  subscriber_data = g_new0 (SubscriberData, 1);
  subscriber_data->range_start = gcal_timeline_subscriber_get_range_start (subscriber);
  subscriber_data->range_end = gcal_timeline_subscriber_get_range_end (subscriber);

  return g_steal_pointer (&subscriber_data);
}

static void
subscriber_data_free (SubscriberData *subscriber_data)
{
  gcal_clear_date_time (&subscriber_data->range_start);
  gcal_clear_date_time (&subscriber_data->range_end);
  g_free (subscriber_data);
}

static void
add_event_to_subscriber (GcalTimelineSubscriber *subscriber,
                         GcalEvent              *event)
{
  GCAL_TRACE_MSG ("Adding event %s to subscriber %s",
                  gcal_event_get_uid (event),
                  G_OBJECT_TYPE_NAME (subscriber));

  GCAL_TIMELINE_SUBSCRIBER_GET_IFACE (subscriber)->add_event (subscriber, event);
}

static void
update_subscriber_event (GcalTimelineSubscriber *subscriber,
                         GcalEvent              *event)
{
  GCAL_TRACE_MSG ("Updating event '%s' (%s) at subscriber %s",
                  gcal_event_get_summary (event),
                  gcal_event_get_uid (event),
                  G_OBJECT_TYPE_NAME (subscriber));

  GCAL_TIMELINE_SUBSCRIBER_GET_IFACE (subscriber)->update_event (subscriber, event);
}
static void
remove_event_from_subscriber (GcalTimelineSubscriber *subscriber,
                              GcalEvent              *event)
{
  GCAL_TRACE_MSG ("Removing event '%s' (%s) from subscriber %s",
                  gcal_event_get_summary (event),
                  gcal_event_get_uid (event),
                  G_OBJECT_TYPE_NAME (subscriber));

  GCAL_TIMELINE_SUBSCRIBER_GET_IFACE (subscriber)->remove_event (subscriber, event);
}

static gboolean
subscriber_contains_event (GcalTimelineSubscriber *subscriber,
                           GcalEvent              *event)
{
  g_autoptr (GDateTime) subscriber_range_start = NULL;
  g_autoptr (GDateTime) subscriber_range_end = NULL;

  subscriber_range_start = gcal_timeline_subscriber_get_range_start (subscriber);
  subscriber_range_end = gcal_timeline_subscriber_get_range_end (subscriber);

  return gcal_event_is_within_range (event, subscriber_range_start, subscriber_range_end);
}

static void
update_range (GcalTimeline *self)
{
  g_autoptr (GDateTime) new_range_start = NULL;
  g_autoptr (GDateTime) new_range_end = NULL;
  GcalTimelineSubscriber *subscriber;
  GHashTableIter iter;
  gboolean has_subscribers;
  gboolean range_changed;

  GCAL_ENTRY;

  range_changed = FALSE;
  has_subscribers = g_hash_table_size (self->subscribers) > 0;

  if (has_subscribers)
    {
      g_hash_table_iter_init (&iter, self->subscribers);
      while (g_hash_table_iter_next (&iter, (gpointer*) &subscriber, NULL))
        {
          g_autoptr (GDateTime) subscriber_start = NULL;
          g_autoptr (GDateTime) subscriber_end = NULL;

          subscriber_start = gcal_timeline_subscriber_get_range_start (subscriber);
          subscriber_end = gcal_timeline_subscriber_get_range_end (subscriber);

          if (!new_range_start || g_date_time_compare (subscriber_start, new_range_start) < 0)
            {
              gcal_clear_date_time (&new_range_start);
              new_range_start = g_steal_pointer (&subscriber_start);
            }

          if (!new_range_end || g_date_time_compare (subscriber_end, new_range_end) > 0)
            {
              gcal_clear_date_time (&new_range_end);
              new_range_end = g_steal_pointer (&subscriber_end);
            }
        }

      if (!self->range_start || g_date_time_compare (self->range_start, new_range_start) != 0)
        {
          gcal_clear_date_time (&self->range_start);
          self->range_start = g_steal_pointer (&new_range_start);
          range_changed = TRUE;
        }

      if (!self->range_end || g_date_time_compare (self->range_end, new_range_end) != 0)
        {
          gcal_clear_date_time (&self->range_end);
          self->range_end = g_steal_pointer (&new_range_end);
          range_changed = TRUE;
        }

    }
  else if (self->range_start || self->range_end)
    {
      gcal_clear_date_time (&self->range_start);
      gcal_clear_date_time (&self->range_end);
      range_changed = TRUE;
    }

  if (range_changed)
    {
      GcalCalendarMonitor *monitor;

      reset_completed_calendars (self);

      g_hash_table_iter_init (&iter, self->calendars);
      while (g_hash_table_iter_next (&iter, NULL, (gpointer*) &monitor))
        gcal_calendar_monitor_set_range (monitor, self->range_start, self->range_end);
    }

  GCAL_EXIT;
}

static void
calculate_changed_events (GcalTimeline            *self,
                          GcalTimelineSubscriber  *subscriber,
                          GDateTime               *old_range_start,
                          GDateTime               *old_range_end,
                          GDateTime               *new_range_start,
                          GDateTime               *new_range_end)
{
  g_autoptr (GPtrArray) events_to_remove = NULL;
  g_autoptr (GPtrArray) events_to_add = NULL;
  gint range_diff;
  gint i;

  events_to_add = g_ptr_array_new ();
  events_to_remove = g_ptr_array_new ();

  /* Start ranges diff */
  range_diff = g_date_time_compare (old_range_start, new_range_start);
  if (range_diff != 0)
    {
      g_autoptr (GPtrArray) events = NULL;

      if (range_diff < 0)
        {
          /* Removed */
          events = gcal_range_tree_get_data_at_range (self->events, old_range_start, new_range_start);
          if (events)
            g_ptr_array_extend (events_to_remove, events, NULL, NULL);
        }
      else if (range_diff > 0)
        {
          /* Added */
          events = gcal_range_tree_get_data_at_range (self->events, new_range_start, old_range_start);
          if (events)
            g_ptr_array_extend (events_to_add, events, NULL, NULL);
        }
    }

  /* End ranges diff */
  range_diff = g_date_time_compare (old_range_end, new_range_end);
  if (range_diff != 0)
    {
      g_autoptr (GPtrArray) events = NULL;

      if (range_diff < 0)
        {
          events = gcal_range_tree_get_data_at_range (self->events, old_range_end, new_range_end);
          if (events)
            g_ptr_array_extend (events_to_add, events, NULL, NULL);
        }
      else if (range_diff > 0)
        {
          events = gcal_range_tree_get_data_at_range (self->events, new_range_end, old_range_end);
          if (events)
            g_ptr_array_extend (events_to_remove, events, NULL, NULL);
        }
    }

  for (i = 0; i < events_to_remove->len; i++)
    {
      GcalEvent *event = g_ptr_array_index (events_to_remove, i);

      if (!gcal_event_is_within_range (event, old_range_start, old_range_end))
        continue;

      remove_event_from_subscriber (subscriber, event);
    }

  for (i = 0; i < events_to_add->len; i++)
    {
      GcalEvent *event = g_ptr_array_index (events_to_add, i);

      if (!gcal_event_is_within_range (event, new_range_start, new_range_end))
        continue;

      add_event_to_subscriber (subscriber, event);
    }
}

static void
update_subscriber_range (GcalTimeline           *self,
                         GcalTimelineSubscriber *subscriber)
{
  g_autoptr (GDateTime) new_range_start = NULL;
  g_autoptr (GDateTime) new_range_end = NULL;
  SubscriberData *subscriber_data;


  GCAL_ENTRY;

  subscriber_data = g_hash_table_lookup (self->subscribers, subscriber);
  g_assert (subscriber_data != NULL);

  new_range_start = gcal_timeline_subscriber_get_range_start (subscriber);
  new_range_end = gcal_timeline_subscriber_get_range_end (subscriber);

  /* Diff new and old event ranges */
  calculate_changed_events (self,
                            subscriber,
                            subscriber_data->range_start,
                            subscriber_data->range_end,
                            new_range_start,
                            new_range_end);

  /* Update the subscriber range */
  gcal_range_tree_remove_data (self->subscriber_ranges, subscriber);
  gcal_range_tree_add_range (self->subscriber_ranges,
                             new_range_start,
                             new_range_end,
                             subscriber);

  gcal_set_date_time (&subscriber_data->range_start, new_range_start);
  gcal_set_date_time (&subscriber_data->range_end, new_range_end);

  GCAL_EXIT;
}

static void
update_calendar_monitor_filters (GcalTimeline *self)
{
  GcalCalendarMonitor *monitor;
  GHashTableIter iter;

  reset_completed_calendars (self);

  g_hash_table_iter_init (&iter, self->calendars);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer*) &monitor))
    gcal_calendar_monitor_set_filter (monitor, self->filter);
}


/*
 * Callbacks
 */

static void
on_calendar_monitor_event_added_cb (GcalCalendarMonitor *monitor,
                                    GcalEvent           *event,
                                    GcalTimeline        *self)
{
  g_autoptr (GPtrArray) subscribers_at_range = NULL;
  GDateTime *event_start;
  GDateTime *event_end;
  gint i;

  GCAL_ENTRY;

  event_start = gcal_event_get_date_start (event);
  event_end = gcal_event_get_date_end (event);

  gcal_range_tree_add_range (self->events, event_start, event_end, g_object_ref (event));

  /* Add to all subscribers within the event range */
  subscribers_at_range = gcal_range_tree_get_data_at_range (self->subscriber_ranges, event_start, event_end);
  for (i = 0; subscribers_at_range && i < subscribers_at_range->len; i++)
    {
      GcalTimelineSubscriber *subscriber = g_ptr_array_index (subscribers_at_range, i);

      if (!subscriber_contains_event (subscriber, event))
        continue;

      add_event_to_subscriber (subscriber, event);
    }

  GCAL_EXIT;
}

static void
on_calendar_monitor_event_updated_cb (GcalCalendarMonitor *monitor,
                                      GcalEvent           *old_event,
                                      GcalEvent           *event,
                                      GcalTimeline        *self)
{
  g_autoptr (GPtrArray) subscribers_at_range = NULL;
  GDateTime *old_event_start;
  GDateTime *old_event_end;
  GDateTime *event_start;
  GDateTime *event_end;
  gint i;

  GCAL_ENTRY;

  /* Remove the old event */
  old_event_start = gcal_event_get_date_start (old_event);
  old_event_end = gcal_event_get_date_end (old_event);

  gcal_range_tree_remove_range (self->events, old_event_start, old_event_end, old_event);

  /* Add the updated event to the (potentially new) range */
  event_start = gcal_event_get_date_start (event);
  event_end = gcal_event_get_date_end (event);

  gcal_range_tree_add_range (self->events, event_start, event_end, g_object_ref (event));

  /* Add to all subscribers within the event range */
  subscribers_at_range = gcal_range_tree_get_data_at_range (self->subscriber_ranges, event_start, event_end);
  for (i = 0; subscribers_at_range && i < subscribers_at_range->len; i++)
    {
      GcalTimelineSubscriber *subscriber = g_ptr_array_index (subscribers_at_range, i);

      if (!subscriber_contains_event (subscriber, event))
        continue;

      update_subscriber_event (subscriber, event);
    }

  GCAL_EXIT;
}

static void
on_calendar_monitor_event_removed_cb (GcalCalendarMonitor *monitor,
                                      GcalEvent           *event,
                                      GcalTimeline        *self)
{
  g_autoptr (GPtrArray) subscribers_at_range = NULL;
  GDateTime *event_start;
  GDateTime *event_end;
  gint i;

  GCAL_ENTRY;

  event_start = gcal_event_get_date_start (event);
  event_end = gcal_event_get_date_end (event);

  /* Add to all subscribers within the event range */
  subscribers_at_range = gcal_range_tree_get_data_at_range (self->subscriber_ranges, event_start, event_end);
  for (i = 0; subscribers_at_range && i < subscribers_at_range->len; i++)
    {
      GcalTimelineSubscriber *subscriber = g_ptr_array_index (subscribers_at_range, i);

      if (!subscriber_contains_event (subscriber, event))
        continue;

      remove_event_from_subscriber (subscriber, event);
    }

  gcal_range_tree_remove_range (self->events, event_start, event_end, event);

  GCAL_EXIT;
}

static void
on_calendar_monitor_completed_cb (GcalCalendarMonitor *monitor,
                                  GcalTimeline        *self)
{
  GCAL_ENTRY;

  increase_completed_calendars (self);

  GCAL_EXIT;
}

static gboolean
update_timeline_range_in_idle_cb (gpointer user_data)
{
  GcalTimeline *self;

  GCAL_ENTRY;

  self = GCAL_TIMELINE (user_data);

  update_range (self);

  self->update_range_idle_id = 0;

  GCAL_RETURN (G_SOURCE_REMOVE);
}

static void
on_subscriber_range_changed_cb (GcalTimelineSubscriber *subscriber,
                                GcalTimeline           *self)
{
  update_subscriber_range (self, subscriber);

  if (self->update_range_idle_id)
    return;

  self->update_range_idle_id = g_idle_add (update_timeline_range_in_idle_cb, self);
}


/*
 * GObject overrides
 */

static void
gcal_timeline_finalize (GObject *object)
{
  GcalTimeline *self = (GcalTimeline *)object;

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  if (self->update_range_idle_id > 0)
    {
      g_source_remove (self->update_range_idle_id);
      self->update_range_idle_id = 0;
    }

  g_clear_pointer (&self->events, gcal_range_tree_unref);
  g_clear_pointer (&self->calendars, g_hash_table_destroy);
  g_clear_pointer (&self->subscribers, g_hash_table_destroy);
  g_clear_pointer (&self->subscriber_ranges, gcal_range_tree_unref);

  G_OBJECT_CLASS (gcal_timeline_parent_class)->finalize (object);
}

static void
gcal_timeline_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GcalTimeline *self = GCAL_TIMELINE (object);

  switch (prop_id)
    {
    case PROP_COMPLETE:
      g_value_set_boolean (value, is_timeline_complete (self));
      break;

    case PROP_CONTEXT:
      g_value_set_object (value, self->context);
      break;

    case PROP_FILTER:
      g_value_set_string (value, self->filter);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_timeline_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GcalTimeline *self = GCAL_TIMELINE (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_assert (self->context == NULL);
      self->context = g_value_get_object (value);
      break;

    case PROP_FILTER:
      gcal_timeline_set_filter (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_timeline_class_init (GcalTimelineClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gcal_timeline_finalize;
  object_class->get_property = gcal_timeline_get_property;
  object_class->set_property = gcal_timeline_set_property;

  /**
   * GcalTimeline::complete:
   *
   * Whether all calendars in this timeline are completed.
   */
  properties[PROP_COMPLETE] = g_param_spec_boolean ("complete",
                                                    "Complete",
                                                    "Complete",
                                                    FALSE,
                                                    G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GcalSearchEngine::context:
   *
   * The #GcalContext of the application.
   */
  properties[PROP_CONTEXT] = g_param_spec_object ("context",
                                                  "Data context",
                                                  "Data context",
                                                  GCAL_TYPE_CONTEXT,
                                                  G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GcalTimeline::filter:
   *
   * The search filter.
   */
  properties[PROP_FILTER] = g_param_spec_string ("filter",
                                                 "Filter",
                                                 "Filter",
                                                 NULL,
                                                 G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gcal_timeline_init (GcalTimeline *self)
{
  self->cancellable = g_cancellable_new ();
  self->events = gcal_range_tree_new_with_free_func (g_object_unref);
  self->calendars = g_hash_table_new_full (NULL, NULL, NULL, g_object_unref);
  self->subscribers = g_hash_table_new_full (NULL, NULL, g_object_unref, (GDestroyNotify) subscriber_data_free);
  self->subscriber_ranges = gcal_range_tree_new ();
}

/**
 * gcal_timeline_new:
 * @context: a #GcalContext
 *
 * Creates a new #GcalTimeline.
 *
 * Returns: (transfer full): a #GcalTimeline
 */
GcalTimeline*
gcal_timeline_new (GcalContext *context)
{
  return g_object_new (GCAL_TYPE_TIMELINE,
                       "context", context,
                       NULL);
}

/**
 * gcal_timeline_add_calendar:
 * @self: a #GcalTimeline
 * @calendar: a #GcalCalendar
 *
 * Adds @calendar to @self. The events in @calendar will be added
 * respectively to
 *
 */
void
gcal_timeline_add_calendar (GcalTimeline *self,
                            GcalCalendar *calendar)
{
  g_autoptr (GcalCalendarMonitor) monitor = NULL;

  g_return_if_fail (GCAL_IS_TIMELINE (self));
  g_return_if_fail (GCAL_IS_CALENDAR (calendar));

  GCAL_ENTRY;

  if (g_hash_table_contains (self->calendars, calendar))
    return;

  GCAL_TRACE_MSG ("Adding calendar '%s' to timeline %p", gcal_calendar_get_name (calendar), self);

  monitor = gcal_calendar_monitor_new (calendar);
  g_signal_connect (monitor, "event-added", G_CALLBACK (on_calendar_monitor_event_added_cb), self);
  g_signal_connect (monitor, "event-updated", G_CALLBACK (on_calendar_monitor_event_updated_cb), self);
  g_signal_connect (monitor, "event-removed", G_CALLBACK (on_calendar_monitor_event_removed_cb), self);
  g_signal_connect (monitor, "completed", G_CALLBACK (on_calendar_monitor_completed_cb), self);
  g_hash_table_insert (self->calendars, calendar, g_object_ref (monitor));

  if (self->range_start && self->range_end)
    gcal_calendar_monitor_set_range (monitor, self->range_start, self->range_end);

  GCAL_EXIT;
}

void
gcal_timeline_remove_calendar (GcalTimeline *self,
                               GcalCalendar *calendar)
{
  g_return_if_fail (GCAL_IS_TIMELINE (self));
  g_return_if_fail (GCAL_IS_CALENDAR (calendar));

  GCAL_ENTRY;

  g_object_ref (calendar);

  if (g_hash_table_remove (self->calendars, calendar))
    GCAL_TRACE_MSG ("Removing calendar '%s' from timeline %p", gcal_calendar_get_name (calendar), self);

  g_object_unref (calendar);

  GCAL_EXIT;
}

void
gcal_timeline_add_subscriber (GcalTimeline           *self,
                              GcalTimelineSubscriber *subscriber)
{
  SubscriberData *subscriber_data;

  g_return_if_fail (GCAL_IS_TIMELINE (self));
  g_return_if_fail (GCAL_IS_TIMELINE_SUBSCRIBER (subscriber));

  GCAL_ENTRY;

  if (g_hash_table_contains (self->subscribers, subscriber))
    GCAL_RETURN ();

  g_debug ("Adding subscriber %s to timeline %p", G_OBJECT_TYPE_NAME (subscriber), self);

  subscriber_data = subscriber_data_new (subscriber);

  g_hash_table_insert (self->subscribers, g_object_ref (subscriber), g_steal_pointer (&subscriber_data));
  g_signal_connect_object (subscriber,
                           "range-changed",
                           G_CALLBACK (on_subscriber_range_changed_cb),
                           self,
                           0);

  update_subscriber_range (self, subscriber);
  update_range (self);

  GCAL_EXIT;
}

void
gcal_timeline_remove_subscriber (GcalTimeline           *self,
                                 GcalTimelineSubscriber *subscriber)
{
  g_return_if_fail (GCAL_IS_TIMELINE (self));
  g_return_if_fail (GCAL_IS_TIMELINE_SUBSCRIBER (subscriber));

  GCAL_ENTRY;

  if (!g_hash_table_contains (self->subscribers, subscriber))
    GCAL_RETURN ();

  g_debug ("Removing subscriber %s from timeline %p", G_OBJECT_TYPE_NAME (subscriber), self);

  g_signal_handlers_disconnect_by_func (subscriber, on_subscriber_range_changed_cb, self);
  g_hash_table_remove (self->subscribers, subscriber);

  update_range (self);

  GCAL_EXIT;
}

GPtrArray*
gcal_timeline_get_events_at_range (GcalTimeline *self,
                                   GDateTime    *range_start,
                                   GDateTime    *range_end)
{
  g_autoptr (GPtrArray) events_at_range = NULL;

  g_return_val_if_fail (GCAL_IS_TIMELINE (self), NULL);
  g_return_val_if_fail (range_start != NULL, NULL);
  g_return_val_if_fail (range_end != NULL, NULL);
  g_return_val_if_fail (g_date_time_compare (range_start, range_end) < 0, NULL);

  events_at_range = gcal_range_tree_get_data_at_range (self->events, range_start, range_end);

  return g_steal_pointer (&events_at_range);
}

const gchar*
gcal_timeline_get_filter (GcalTimeline *self)
{
  g_return_val_if_fail (GCAL_IS_TIMELINE (self), NULL);

  return self->filter;
}

void
gcal_timeline_set_filter (GcalTimeline *self,
                          const gchar  *filter)
{
  g_return_if_fail (GCAL_IS_TIMELINE (self));

  GCAL_ENTRY;

  if (g_strcmp0 (self->filter, filter) == 0)
    GCAL_RETURN ();

  g_debug ("Setting timeline filter to \"%s\"", filter);

  g_clear_pointer (&self->filter, g_free);
  self->filter = g_strdup (filter);

  update_calendar_monitor_filters (self);

  GCAL_EXIT;
}

gboolean
gcal_timeline_is_complete (GcalTimeline *self)
{
  g_return_val_if_fail (GCAL_IS_TIMELINE (self), FALSE);

  return is_timeline_complete (self);
}
