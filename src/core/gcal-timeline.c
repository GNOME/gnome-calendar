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
#include "gcal-date-time-utils.h"
#include "gcal-debug.h"
#include "gcal-event.h"
#include "gcal-range-tree.h"
#include "gcal-timeline.h"
#include "gcal-timeline-subscriber.h"
#include "gcal-utils.h"

#include <libedataserver/libedataserver.h>

struct _GcalTimeline
{
  GObject             parent_instance;

  guint               update_range_idle_id;
  GcalRange          *range;

  GcalRangeTree      *events;
  gchar              *filter;

  GHashTable         *calendars; /* GcalCalendar* -> GcalCalendarMonitor* */
  gboolean            complete;

  GListStore         *calendar_monitors;
  GListModel         *events_model;

  GHashTable         *subscribers; /* GcalTimelineSubscriber* -> SubscriberData* */

  GCancellable       *cancellable;
};

G_DEFINE_TYPE (GcalTimeline, gcal_timeline, G_TYPE_OBJECT)

enum
{
  PROP_0,
  PROP_COMPLETE,
  PROP_FILTER,
  N_PROPS
};

static GParamSpec *properties [N_PROPS] = { NULL, };


/*
 * SubscriberData
 */

typedef struct
{
  GcalRange *range;
  GtkFilterListModel *events;
  GtkSortListModel *sorted_events;
} SubscriberData;

static void
subscriber_data_free (SubscriberData *data)
{
  gtk_filter_list_model_set_model (data->events, NULL);

  g_clear_pointer (&data->range, gcal_range_unref);
  g_clear_object (&data->sorted_events);
  g_clear_object (&data->events);
  g_clear_pointer (&data, g_free);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (SubscriberData, subscriber_data_free)

static gboolean
event_in_subscriber_range_func (gpointer item,
                                gpointer user_data)
{
  SubscriberData *data = user_data;

  g_assert (GCAL_IS_EVENT (item));

  if (!data->range)
    return FALSE;

  return gcal_event_overlaps (item, data->range);
}

static gint
compare_events_cb (gconstpointer a,
                   gconstpointer b,
                   gpointer      user_data)
{
  GcalEvent *event_a = (GcalEvent *) a;
  GcalEvent *event_b = (GcalEvent *) b;

  return gcal_range_compare (gcal_event_get_range (event_b), gcal_event_get_range (event_a));
}

static SubscriberData *
subscriber_data_new (GcalTimeline           *self,
                     GcalTimelineSubscriber *subscriber)
{
  g_autoptr (GtkCustomFilter) filter = NULL;
  g_autoptr (SubscriberData) data = NULL;

  data = g_new0 (SubscriberData, 1);
  data->range = gcal_timeline_subscriber_get_range (subscriber);

  filter = gtk_custom_filter_new (event_in_subscriber_range_func, data, NULL);
  data->events = gtk_filter_list_model_new (g_object_ref (self->events_model), GTK_FILTER (g_steal_pointer (&filter)));
  data->sorted_events = gtk_sort_list_model_new (G_LIST_MODEL (g_object_ref (data->events)),
                                                 GTK_SORTER (gtk_custom_sorter_new (compare_events_cb, NULL, NULL)));

  return g_steal_pointer (&data);
}

/*
 * Auxiliary methods
 */

static void
update_completed_calendars (GcalTimeline *self)
{
  GcalCalendarMonitor *calendar_monitor;
  GcalCalendar *calendar;
  GHashTableIter iter;
  gboolean was_complete;
  gboolean is_complete;

  GCAL_ENTRY;

  was_complete = self->complete;
  is_complete = TRUE;

  g_hash_table_iter_init (&iter, self->calendars);
  while (g_hash_table_iter_next (&iter, (gpointer*) &calendar, (gpointer*) &calendar_monitor))
    {
      if (gcal_calendar_get_visible (calendar))
        is_complete &= gcal_calendar_monitor_is_complete (calendar_monitor);
    }

  if (was_complete != is_complete)
    {
      GCAL_TRACE_MSG ("Timeline %p is %s", self, is_complete ? "complete" : "incomplete");

      self->complete = is_complete;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_COMPLETE]);
    }

  GCAL_EXIT;
}

static void
set_model_on_subscriber (GcalTimelineSubscriber *subscriber,
                         GListModel             *model)
{
  /* Optional while we transition away from add/update/remove_event */
  if (!GCAL_TIMELINE_SUBSCRIBER_GET_IFACE (subscriber)->set_model)
    return;

  GCAL_TRACE_MSG ("Setting event model %p on subscriber %s",
                  model,
                  G_OBJECT_TYPE_NAME (subscriber));

  GCAL_TIMELINE_SUBSCRIBER_GET_IFACE (subscriber)->set_model (subscriber, model);
}

static void
update_range (GcalTimeline *self)
{
  GcalTimelineSubscriber *subscriber;
  GHashTableIter iter;
  gboolean has_subscribers;
  gboolean range_changed;

  GCAL_ENTRY;

  range_changed = FALSE;
  has_subscribers = g_hash_table_size (self->subscribers) > 0;

  if (has_subscribers)
    {
      g_autoptr (GcalRange) new_range = NULL;

      g_hash_table_iter_init (&iter, self->subscribers);
      while (g_hash_table_iter_next (&iter, (gpointer*) &subscriber, NULL))
        {
          g_autoptr (GcalRange) subscriber_range = NULL;
          g_autoptr (GcalRange) union_range = NULL;

          subscriber_range = gcal_timeline_subscriber_get_range (subscriber);

          if (new_range)
            {
              union_range = gcal_range_union (subscriber_range, new_range);

              g_clear_pointer (&new_range, gcal_range_unref);
              new_range = g_steal_pointer (&union_range);
            }
          else
            {
              new_range = g_steal_pointer (&subscriber_range);
            }
        }

      if (!self->range || gcal_range_compare (self->range, new_range) != 0)
        {
          g_clear_pointer (&self->range, gcal_range_unref);
          self->range = g_steal_pointer (&new_range);
          range_changed = TRUE;
        }

    }
  else if (self->range)
    {
      g_clear_pointer (&self->range, gcal_range_unref);
      range_changed = TRUE;
    }

  if (range_changed)
    {
      GcalCalendarMonitor *monitor;

      g_hash_table_iter_init (&iter, self->calendars);
      while (g_hash_table_iter_next (&iter, NULL, (gpointer*) &monitor))
        gcal_calendar_monitor_set_range (monitor, self->range);
    }

  GCAL_EXIT;
}

static void
update_subscriber_range (GcalTimeline           *self,
                         GcalTimelineSubscriber *subscriber)
{
  g_autoptr (GcalRange) old_range = NULL;
  g_autoptr (GcalRange) new_range = NULL;
  SubscriberData *subscriber_data;
  GtkFilter *filter;

  GCAL_ENTRY;

  subscriber_data = g_hash_table_lookup (self->subscribers, subscriber);

  old_range = g_steal_pointer (&subscriber_data->range);
  g_assert (old_range != NULL);

  new_range = gcal_timeline_subscriber_get_range (subscriber);

  subscriber_data->range = gcal_range_ref (new_range);

  g_assert (old_range != NULL);
  g_assert (new_range != NULL);

  filter = gtk_filter_list_model_get_filter (subscriber_data->events);
  g_assert (GTK_IS_FILTER (filter));

  switch (gcal_range_calculate_overlap (new_range, old_range, NULL))
    {
    case GCAL_RANGE_NO_OVERLAP:
    case GCAL_RANGE_INTERSECTS:
      gtk_filter_changed (filter, GTK_FILTER_CHANGE_DIFFERENT);
      break;

    case GCAL_RANGE_SUBSET:
      gtk_filter_changed (filter, GTK_FILTER_CHANGE_MORE_STRICT);
      break;

    case GCAL_RANGE_EQUAL:
      /* No need to refilter */
      break;

    case GCAL_RANGE_SUPERSET:
      gtk_filter_changed (filter, GTK_FILTER_CHANGE_LESS_STRICT);
      break;

    default:
      g_assert_not_reached ();
    }

  GCAL_EXIT;
}

static void
update_calendar_monitor_filters (GcalTimeline *self)
{
  GcalCalendarMonitor *monitor;
  GHashTableIter iter;

  g_hash_table_iter_init (&iter, self->calendars);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer*) &monitor))
    gcal_calendar_monitor_set_filter (monitor, self->filter);
}


/*
 * Callbacks
 */

static void
on_calendar_monitor_completed_cb (GcalCalendarMonitor *monitor,
                                  GParamSpec          *pspec,
                                  GcalTimeline        *self)
{
  GCAL_ENTRY;

  update_completed_calendars (self);

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

  g_clear_handle_id (&self->update_range_idle_id, g_source_remove);

  g_clear_pointer (&self->events, gcal_range_tree_unref);
  g_clear_pointer (&self->calendars, g_hash_table_destroy);
  g_clear_pointer (&self->subscribers, g_hash_table_destroy);

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
      g_value_set_boolean (value, self->complete);
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

  self->calendar_monitors = g_list_store_new (GCAL_TYPE_CALENDAR_MONITOR);
  self->events_model = G_LIST_MODEL (gtk_flatten_list_model_new (g_object_ref (G_LIST_MODEL (self->calendar_monitors))));
}

/**
 * gcal_timeline_new:
 *
 * Creates a new #GcalTimeline.
 *
 * Returns: (transfer full): a #GcalTimeline
 */
GcalTimeline*
gcal_timeline_new (void)
{
  return g_object_new (GCAL_TYPE_TIMELINE,
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
  g_signal_connect (monitor, "notify::complete", G_CALLBACK (on_calendar_monitor_completed_cb), self);
  g_hash_table_insert (self->calendars, calendar, g_object_ref (monitor));
  g_list_store_append (self->calendar_monitors, monitor);

  if (self->range)
    gcal_calendar_monitor_set_range (monitor, self->range);

  update_completed_calendars (self);

  GCAL_EXIT;
}

void
gcal_timeline_remove_calendar (GcalTimeline *self,
                               GcalCalendar *calendar)
{
  g_autoptr (GcalCalendarMonitor) calendar_monitor = NULL;

  g_return_if_fail (GCAL_IS_TIMELINE (self));
  g_return_if_fail (GCAL_IS_CALENDAR (calendar));

  GCAL_ENTRY;

  g_object_ref (calendar);

  if (g_hash_table_steal_extended (self->calendars, calendar, NULL, (gpointer *) &calendar_monitor))
    {
      guint position = 0;

      GCAL_TRACE_MSG ("Removing calendar '%s' from timeline %p", gcal_calendar_get_name (calendar), self);

      if (g_list_store_find (self->calendar_monitors, calendar_monitor, &position))
        g_list_store_remove (self->calendar_monitors, position);

      update_completed_calendars (self);
    }

  g_object_unref (calendar);

  GCAL_EXIT;
}

void
gcal_timeline_add_subscriber (GcalTimeline           *self,
                              GcalTimelineSubscriber *subscriber)
{
  g_autoptr (SubscriberData) subscriber_data = NULL;

  g_return_if_fail (GCAL_IS_TIMELINE (self));
  g_return_if_fail (GCAL_IS_TIMELINE_SUBSCRIBER (subscriber));

  GCAL_ENTRY;

  if (g_hash_table_contains (self->subscribers, subscriber))
    GCAL_RETURN ();

  g_debug ("Adding subscriber %s to timeline %p", G_OBJECT_TYPE_NAME (subscriber), self);

  subscriber_data = subscriber_data_new (self, subscriber);

  set_model_on_subscriber (subscriber, G_LIST_MODEL (subscriber_data->sorted_events));

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
  g_autoptr (GcalRange) range = NULL;

  g_return_val_if_fail (GCAL_IS_TIMELINE (self), NULL);
  g_return_val_if_fail (range_start != NULL, NULL);
  g_return_val_if_fail (range_end != NULL, NULL);

  range = gcal_range_new (range_start, range_end, GCAL_RANGE_DEFAULT);
  events_at_range = gcal_range_tree_get_data_at_range (self->events, range);

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

  return self->complete;
}
