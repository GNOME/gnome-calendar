/* gcal-calendar-monitor.c
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

#define G_LOG_DOMAIN "GcalCalendarMonitor"

#include "gcal-calendar-monitor.h"
#include "gcal-date-time-utils.h"
#include "gcal-debug.h"
#include "gcal-event.h"

#include <gio/gio.h>
#include <libecal/libecal.h>

typedef struct
{
  GcalCalendarMonitor *monitor;
  GcalEvent           *event;
  gchar               *event_id;
  gboolean             complete;
} IdleData;

typedef enum
{
  INVALID_EVENT,
  CREATE_VIEW,
  REMOVE_VIEW,
  RANGE_UPDATED,
  FILTER_UPDATED,
  QUIT,
} MonitorThreadEvent;

struct _GcalCalendarMonitor
{
  GObject             parent;

  GThread            *thread;
  GCancellable       *cancellable;
  GMainContext       *thread_context;
  GMainContext       *main_context;

  GAsyncQueue        *messages;
  GcalCalendar       *calendar;
  gboolean            complete;

  /*
   * These fields are only accessed on the monitor thread, and
   * never on the main thread.
   */
  struct {
    gboolean          populated;
    GHashTable       *events_to_add;
    ECalClientView   *view;
  } monitor_thread;

  /*
   * Accessing any of the fields below must be happen inside
   * a locked muxed.
   */
  struct {
    GRWLock           lock;
    GHashTable       *events; /* gchar* -> GcalEvent* */
    GcalRange        *range;
    gchar            *filter;
  } shared;
};

static gboolean      add_event_to_timeline_in_idle_cb            (gpointer           user_data);
static gboolean      update_event_in_idle_cb                     (gpointer           user_data);
static gboolean      remove_event_from_timeline_in_idle_cb       (gpointer           user_data);
static gboolean      complete_in_idle_cb                         (gpointer           user_data);

G_DEFINE_TYPE (GcalCalendarMonitor, gcal_calendar_monitor, G_TYPE_OBJECT)

enum
{
  EVENT_ADDED,
  EVENT_UPDATED,
  EVENT_REMOVED,
  N_SIGNALS,
};

enum
{
  PROP_0,
  PROP_CALENDAR,
  PROP_COMPLETE,
  N_PROPS
};

static gulong signals[N_SIGNALS] = { 0, };
static GParamSpec *properties [N_PROPS] = { NULL, };

/*
 * Threads
 *
 * These methods must *never* be executed in the main thread.
 */

static gchar*
build_subscriber_filter (GcalRange   *range,
                         const gchar *filter)
{
  g_autoptr (GDateTime) inclusive_range_end = NULL;
  g_autoptr (GDateTime) utc_range_start = NULL;
  g_autoptr (GDateTime) utc_range_end = NULL;
  g_autoptr (GDateTime) range_start = NULL;
  g_autoptr (GDateTime) range_end = NULL;
  g_autofree gchar *start_str = NULL;
  g_autofree gchar *end_str = NULL;
  g_autofree gchar *result = NULL;

  /*
   * XXX: E-D-S ISO8601 parser is incomplete and doesn't accept the output of
   * g_date_time_format_iso8601().
   */

  range_start = gcal_range_get_start (range);
  utc_range_start = g_date_time_to_utc (range_start);
  start_str = g_date_time_format (utc_range_start, "%Y%m%dT%H%M%SZ");

  range_end = gcal_range_get_end (range);
  inclusive_range_end = g_date_time_add_seconds (range_end, -1);
  utc_range_end = g_date_time_to_utc (inclusive_range_end);
  end_str = g_date_time_format (utc_range_end, "%Y%m%dT%H%M%SZ");

  if (filter)
    {
      result = g_strdup_printf ("(and (occur-in-time-range? (make-time \"%s\") (make-time \"%s\")) %s)",
                                start_str,
                                end_str,
                                filter);
    }
  else
    {
      result = g_strdup_printf ("(occur-in-time-range? (make-time \"%s\") (make-time \"%s\"))",
                                start_str,
                                end_str);
    }

  return g_steal_pointer (&result);
}

static void
maybe_init_event_arrays (GcalCalendarMonitor *self)
{
  if (self->monitor_thread.populated)
    return;

  if (!self->monitor_thread.events_to_add)
    {
      self->monitor_thread.events_to_add = g_hash_table_new_full (g_str_hash,
                                                                  g_str_equal,
                                                                  g_free,
                                                                  g_object_unref);
    }
}

static GcalRange*
get_monitor_ranges (GcalCalendarMonitor  *self)
{
  g_autoptr (GcalRange) range = NULL;

  g_rw_lock_reader_lock (&self->shared.lock);
  range = gcal_range_copy (self->shared.range);
  g_rw_lock_reader_unlock (&self->shared.lock);

  return g_steal_pointer (&range);
}

static void
idle_data_free (IdleData *data)
{
  g_clear_object (&data->monitor);
  g_clear_object (&data->event);
  g_clear_pointer (&data->event_id, g_free);
  g_free (data);
}

static void
add_event_in_idle (GcalCalendarMonitor *self,
                   GcalEvent        *event)
{
  IdleData *idle_data;

  idle_data = g_new0 (IdleData, 1);
  idle_data->monitor = g_object_ref (self);
  idle_data->event = g_object_ref (event);

  g_main_context_invoke_full (self->main_context,
                              G_PRIORITY_DEFAULT_IDLE,
                              add_event_to_timeline_in_idle_cb,
                              idle_data,
                              (GDestroyNotify) idle_data_free);
}

static void
update_event_in_idle (GcalCalendarMonitor *self,
                      GcalEvent           *event)
{
  IdleData *idle_data;

  idle_data = g_new0 (IdleData, 1);
  idle_data->monitor = g_object_ref (self);
  idle_data->event = g_object_ref (event);

  g_main_context_invoke_full (self->main_context,
                              G_PRIORITY_DEFAULT_IDLE,
                              update_event_in_idle_cb,
                              idle_data,
                              (GDestroyNotify) idle_data_free);
}

static void
remove_event_in_idle (GcalCalendarMonitor *self,
                      const gchar         *event_id)
{
  IdleData *idle_data;

  idle_data = g_new0 (IdleData, 1);
  idle_data->monitor = g_object_ref (self);
  idle_data->event_id = g_strdup (event_id);

  g_main_context_invoke_full (self->main_context,
                              G_PRIORITY_DEFAULT_IDLE,
                              remove_event_from_timeline_in_idle_cb,
                              idle_data,
                              (GDestroyNotify) idle_data_free);
}

static void
set_complete_in_idle (GcalCalendarMonitor *self,
                      gboolean             complete)
{
  IdleData *idle_data;

  idle_data = g_new0 (IdleData, 1);
  idle_data->monitor = g_object_ref (self);
  idle_data->complete = complete;

  g_main_context_invoke_full (self->main_context,
                              G_PRIORITY_DEFAULT_IDLE,
                              complete_in_idle_cb,
                              idle_data,
                              (GDestroyNotify) idle_data_free);
}

typedef struct
{
  GcalCalendarMonitor *monitor;
  GPtrArray           *expanded_events;
} GenerateRecurrencesData;

static gboolean
client_instance_generated_cb (ICalComponent  *icomponent,
                              ICalTime       *instance_start,
                              ICalTime       *instance_end,
                              gpointer        user_data,
                              GCancellable   *cancellable,
                              GError        **error)
{
  g_autoptr (GcalEvent) event = NULL;
  g_autoptr (GError) local_error = NULL;
  GenerateRecurrencesData *data;
  GcalCalendarMonitor *self;
  ECalComponent *ecomponent;

  data = user_data;
  self = data->monitor;

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return FALSE;

  ecomponent = e_cal_component_new_from_icalcomponent (i_cal_component_clone (icomponent));
  if (!ecomponent)
    return TRUE;

  event = gcal_event_new (self->calendar, ecomponent, &local_error);
  g_clear_object (&ecomponent);
  if (local_error)
    {
      g_propagate_error (error, local_error);
      return TRUE;
    }

  g_ptr_array_add (data->expanded_events, g_steal_pointer (&event));

  return TRUE;
}

static void
on_client_view_objects_added_cb (ECalClientView      *view,
                                 const GSList        *objects,
                                 GcalCalendarMonitor *self)
{
  g_autoptr (GPtrArray) components_to_expand = NULL;
  g_autoptr (GcalRange) range = NULL;
  const GSList *l;
  gint i;

  GCAL_ENTRY;

  range = get_monitor_ranges (self);

  maybe_init_event_arrays (self);
  components_to_expand = g_ptr_array_new ();

  for (l = objects; l; l = l->next)
    {
      g_autoptr (GcalEvent) event = NULL;
      g_autoptr (GError) error = NULL;
      ICalComponent *icomponent;
      ECalComponent *ecomponent;

      icomponent = l->data;

      if (g_cancellable_is_cancelled (self->cancellable))
        return;

      if (!icomponent || !i_cal_component_get_uid (icomponent))
        continue;

      /* Recurrent events will be processed later */
      if (e_cal_util_component_has_recurrences (icomponent) &&
          !e_cal_util_component_is_instance (icomponent))
        {
          GCAL_TRACE_MSG ("Component %s (%s) needs to be expanded",
                          i_cal_component_get_summary (icomponent),
                          i_cal_component_get_uid (icomponent));
          g_ptr_array_add (components_to_expand, icomponent);
          continue;
        }

      ecomponent = e_cal_component_new_from_icalcomponent (i_cal_component_clone (icomponent));

      if (!ecomponent)
        continue;

      event = gcal_event_new (self->calendar, ecomponent, &error);
      g_clear_object (&ecomponent);

      if (error)
        {
          g_warning ("Error creating event: %s", error->message);
          continue;
        }

      if (!self->monitor_thread.populated)
        {
          g_autofree gchar *event_id = NULL;

          event_id = g_strdup (gcal_event_get_uid (event));

          g_hash_table_insert (self->monitor_thread.events_to_add,
                               g_steal_pointer (&event_id),
                               g_object_ref (event));
        }
      else
        {
          add_event_in_idle (self, event);
        }
    }

  /* Recurrent events */
  if (components_to_expand->len > 0)
    {
      g_autoptr (GPtrArray) expanded_events = NULL;
      g_autoptr (GDateTime) range_start = NULL;
      g_autoptr (GDateTime) range_end = NULL;
      ECalClient *client;
      time_t range_start_time;
      time_t range_end_time;

      GCAL_TRACE_MSG ("Expanding recurrencies of %d events", components_to_expand->len);

      client = gcal_calendar_get_client (self->calendar);

      range_start = gcal_range_get_start (range);
      range_end = gcal_range_get_end (range);
      range_start_time = g_date_time_to_unix (range_start);
      range_end_time = g_date_time_to_unix (range_end) - 1;

      expanded_events = g_ptr_array_new_with_free_func (g_object_unref);

      /* Generate the instances */
      for (i = 0; i < components_to_expand->len; i++)
        {
          GenerateRecurrencesData recurrences_data;
          ICalComponent *icomponent;
#if GCAL_ENABLE_TRACE
          gint old_size;
#endif

          if (g_cancellable_is_cancelled (self->cancellable))
            return;

          icomponent = g_ptr_array_index (components_to_expand, i);

          recurrences_data.monitor = self;
          recurrences_data.expanded_events = expanded_events;

#if GCAL_ENABLE_TRACE
          old_size = expanded_events->len;
#endif

          e_cal_client_generate_instances_for_object_sync (client,
                                                           icomponent,
                                                           range_start_time,
                                                           range_end_time,
                                                           self->cancellable,
                                                           client_instance_generated_cb,
                                                           &recurrences_data);

#if GCAL_ENABLE_TRACE
            {
              g_autofree gchar *range_str = gcal_range_to_string (range);

              GCAL_TRACE_MSG ("Component %s (%s) added %d instance(s) between %s",
                              i_cal_component_get_summary (icomponent),
                              i_cal_component_get_uid (icomponent),
                              expanded_events->len - old_size,
                              range_str);
            }
#endif
        }

      /* Process all these instances */
      for (i = 0; i < expanded_events->len; i++)
        {
          GcalEvent *event = g_ptr_array_index (expanded_events, i);

          if (g_cancellable_is_cancelled (self->cancellable))
            return;

          if (!self->monitor_thread.populated)
            {
              g_autofree gchar *event_id = NULL;

              event_id = g_strdup (gcal_event_get_uid (event));

              g_hash_table_insert (self->monitor_thread.events_to_add,
                                   g_steal_pointer (&event_id),
                                   g_object_ref (event));
            }
          else
            {
              add_event_in_idle (self, event);
            }
        }
    }

  GCAL_EXIT;
}

static void
on_client_view_objects_modified_cb (ECalClientView      *view,
                                    const GSList        *objects,
                                    GcalCalendarMonitor *self)
{
  g_autoptr (GRWLockReaderLocker) reader_locker = NULL;
  g_autoptr (GHashTable) events_to_remove = NULL;
  g_autoptr (GPtrArray) components_to_expand = NULL;
  g_autoptr (GcalRange) range = NULL;
  GHashTableIter iter;
  const GSList *l;
  const gchar *aux;

  GCAL_ENTRY;

  if (!self->monitor_thread.populated && self->monitor_thread.events_to_add)
    {
      g_clear_pointer (&self->monitor_thread.events_to_add, g_hash_table_destroy);
      return;
    }

  reader_locker = g_rw_lock_reader_locker_new (&self->shared.lock);
  components_to_expand = g_ptr_array_new ();
  events_to_remove = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  range = gcal_range_copy (self->shared.range);

  for (l = objects; l; l = l->next)
    {
      g_autoptr (GcalEvent) event = NULL;
      g_autoptr (GError) error = NULL;
      g_autofree gchar *event_id = NULL;
      ECalComponentId *component_id;
      ICalComponent *icomponent;
      ECalComponent *ecomponent;
      gboolean recurrence_main;

      icomponent = l->data;

      if (!icomponent || !i_cal_component_get_uid (icomponent))
        continue;

      ecomponent = e_cal_component_new_from_icalcomponent (i_cal_component_clone (icomponent));

      if (!ecomponent)
        continue;

      recurrence_main = e_cal_util_component_has_recurrences (icomponent) &&
                        !e_cal_util_component_is_instance (icomponent);

      /*
       * If the event has no recurrence id, it is either a main event, or a
       * regular non-recurring event. If we're receiving the 'objects-modified'
       * signal because this event went from recurring to non-recurring, the
       * instances of the old recurrency are still here. They need to be removed.
       */
      component_id = e_cal_component_get_id (ecomponent);

      event_id = g_strdup_printf ("%s:%s",
                                  gcal_calendar_get_id (self->calendar),
                                  e_cal_component_id_get_uid (component_id));

      if (!e_cal_component_id_get_rid (component_id))
        {
          g_hash_table_iter_init (&iter, self->shared.events);
          while (g_hash_table_iter_next (&iter, (gpointer*) &aux, NULL))
            {
              if (!g_str_has_prefix (aux, event_id))
                continue;

              if (recurrence_main || !g_str_equal (aux, event_id))
                g_hash_table_add (events_to_remove, g_strdup (aux));
            }
        }
      g_clear_pointer (&component_id, e_cal_component_id_free);

      /* Recurrent events will be processed later */
      if (recurrence_main)
        {
          GCAL_TRACE_MSG ("Component %s (%s) needs to be expanded",
                          i_cal_component_get_summary (icomponent),
                          i_cal_component_get_uid (icomponent));
          g_ptr_array_add (components_to_expand, icomponent);
          continue;
        }

      event = gcal_event_new (self->calendar, ecomponent, &error);
      g_clear_object (&ecomponent);

      if (error)
        {
          g_warning ("Error creating event: %s", error->message);
          continue;
        }

      update_event_in_idle (self, event);
    }

  /* Recurrent events */
  if (components_to_expand->len > 0)
    {
      g_autoptr (GPtrArray) expanded_events = NULL;
      g_autoptr (GDateTime) range_start = NULL;
      g_autoptr (GDateTime) range_end = NULL;
      ECalClient *client;
      time_t range_start_time;
      time_t range_end_time;

      GCAL_TRACE_MSG ("Expanding recurrencies of %d events", components_to_expand->len);

      client = gcal_calendar_get_client (self->calendar);

      range_start = gcal_range_get_start (range);
      range_end = gcal_range_get_end (range);
      range_start_time = g_date_time_to_unix (range_start);
      range_end_time = g_date_time_to_unix (range_end) - 1;

      expanded_events = g_ptr_array_new_with_free_func (g_object_unref);

      /* Generate the instances */
      for (guint i = 0; i < components_to_expand->len; i++)
        {
          GenerateRecurrencesData recurrences_data;
          ICalComponent *icomponent;
#if GCAL_ENABLE_TRACE
          gint old_size;
#endif

          if (g_cancellable_is_cancelled (self->cancellable))
            return;

          icomponent = g_ptr_array_index (components_to_expand, i);

          recurrences_data.monitor = self;
          recurrences_data.expanded_events = expanded_events;

#if GCAL_ENABLE_TRACE
          old_size = expanded_events->len;
#endif

          e_cal_client_generate_instances_for_object_sync (client,
                                                           icomponent,
                                                           range_start_time,
                                                           range_end_time,
                                                           self->cancellable,
                                                           client_instance_generated_cb,
                                                           &recurrences_data);

#if GCAL_ENABLE_TRACE
            {
              g_autofree gchar *range_str = gcal_range_to_string (range);

              GCAL_TRACE_MSG ("Component %s (%s) added %d instance(s) between %s",
                              i_cal_component_get_summary (icomponent),
                              i_cal_component_get_uid (icomponent),
                              expanded_events->len - old_size,
                              range_str);
            }
#endif
        }

      for (guint i = 0; i < expanded_events->len; i++)
        {
          GcalEvent *event = g_ptr_array_index (expanded_events, i);
          GcalEvent *old_event;

          if (g_cancellable_is_cancelled (self->cancellable))
            return;

          old_event = g_hash_table_lookup (self->shared.events, gcal_event_get_uid (event));
          if (old_event)
            {
              update_event_in_idle (self, event);
              g_hash_table_remove (events_to_remove, gcal_event_get_uid (event));
            }
          else
            {
              add_event_in_idle (self, event);
            }
        }
    }

  /* Now remove lingering events */
  g_hash_table_iter_init (&iter, events_to_remove);
  while (g_hash_table_iter_next (&iter, (gpointer*) &aux, NULL))
    remove_event_in_idle (self, aux);

  GCAL_EXIT;
}

static void
on_client_view_objects_removed_cb (ECalClientView      *view,
                                   const GSList        *objects,
                                   GcalCalendarMonitor *self)
{
  const GSList *l;

  GCAL_ENTRY;

  for (l = objects; l; l = l->next)
    {
      g_autofree gchar *event_id = NULL;
      ECalComponentId *component_id;

      if (g_cancellable_is_cancelled (self->cancellable))
        return;

      component_id = l->data;

      if (e_cal_component_id_get_rid (component_id))
        {
          event_id = g_strdup_printf ("%s:%s:%s",
                                      gcal_calendar_get_id (self->calendar),
                                      e_cal_component_id_get_uid (component_id),
                                      e_cal_component_id_get_rid (component_id));
        }
      else
        {
          g_autoptr (GRWLockReaderLocker) reader_locker = NULL;
          GHashTableIter iter;
          const gchar *aux;

          event_id = g_strdup_printf ("%s:%s",
                                      gcal_calendar_get_id (self->calendar),
                                      e_cal_component_id_get_uid (component_id));

          /*
           * If this is the main component, remove the expanded recurrency instances
           * as well.
           */
          reader_locker = g_rw_lock_reader_locker_new (&self->shared.lock);

          g_hash_table_iter_init (&iter, self->shared.events);
          while (g_hash_table_iter_next (&iter, (gpointer*) &aux, NULL))
            {
              if (!g_str_equal (aux, event_id) && g_str_has_prefix (aux, event_id))
                remove_event_in_idle (self, aux);
            }
        }

      remove_event_in_idle (self, event_id);
    }

  GCAL_EXIT;
}

static void
on_client_view_complete_cb (ECalClientView      *view,
                            const GError        *error,
                            GcalCalendarMonitor *self)
{
  g_autoptr (GHashTable) events_to_add = NULL;

  GCAL_ENTRY;

  g_assert (!self->monitor_thread.populated);

  events_to_add = g_steal_pointer (&self->monitor_thread.events_to_add);

  /* Process all these instances */
  if (events_to_add)
    {
      GHashTableIter iter;
      GcalEvent *event;

      g_hash_table_iter_init (&iter, events_to_add);
      while (g_hash_table_iter_next (&iter, NULL, (gpointer*) &event))
        {
          if (g_cancellable_is_cancelled (self->cancellable))
            return;

          GCAL_TRACE_MSG ("Sending event '%s' (%s) to main thead's idle",
                          gcal_event_get_summary (event),
                          gcal_event_get_uid (event));

          add_event_in_idle (self, event);
        }
    }

  self->monitor_thread.populated = TRUE;

  set_complete_in_idle (self, TRUE);

  g_debug ("Finished initial loading of calendar '%s'", gcal_calendar_get_name (self->calendar));

  GCAL_EXIT;
}

static void
create_view (GcalCalendarMonitor *self)
{
  g_autoptr (GRWLockReaderLocker) reader_locker = NULL;
  g_autofree gchar *filter = NULL;
  g_autoptr (GError) error = NULL;
  ECalClientView *view;
  ECalClient *client;

  GCAL_ENTRY;

  reader_locker = g_rw_lock_reader_locker_new (&self->shared.lock);

  g_assert (self->cancellable == NULL);
  self->cancellable = g_cancellable_new ();

  if (!self->shared.range)
    GCAL_RETURN ();

  filter = build_subscriber_filter (self->shared.range, self->shared.filter);

  g_clear_pointer (&reader_locker, g_rw_lock_reader_locker_free);

  if (!gcal_calendar_get_visible (self->calendar))
    GCAL_RETURN ();

  client = gcal_calendar_get_client (self->calendar);
  e_cal_client_get_view_sync (client, filter, &view, self->cancellable, &error);

  if (error)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Error creating view: %s", error->message);
      GCAL_RETURN ();
    }

  GCAL_TRACE_MSG ("Initialized ECalClientView with query \"%s\"", filter);

  g_signal_connect (view, "objects-added", G_CALLBACK (on_client_view_objects_added_cb), self);
  g_signal_connect (view, "objects-modified", G_CALLBACK (on_client_view_objects_modified_cb), self);
  g_signal_connect (view, "objects-removed", G_CALLBACK (on_client_view_objects_removed_cb), self);
  g_signal_connect (view, "complete", G_CALLBACK (on_client_view_complete_cb), self);

  if (g_cancellable_is_cancelled (self->cancellable))
    GCAL_RETURN ();

  g_debug ("Starting ECalClientView for calendar '%s'", gcal_calendar_get_name (self->calendar));

  e_cal_client_view_start (view, &error);

  if (error)
    {
      g_warning ("Error starting up view: %s", error->message);
      g_clear_object (&view);
      GCAL_RETURN ();
    }

  self->monitor_thread.view = g_steal_pointer (&view);

  set_complete_in_idle (self, FALSE);

  GCAL_EXIT;
}

static void
remove_view (GcalCalendarMonitor *self)
{
  g_autoptr (GError) error = NULL;

  GCAL_ENTRY;

  g_clear_object (&self->cancellable);

  if (!self->monitor_thread.view)
    GCAL_RETURN ();

  g_debug ("Tearing down view for calendar %s", gcal_calendar_get_id (self->calendar));

  e_cal_client_view_stop (self->monitor_thread.view, &error);

  if (error)
    {
      g_warning ("Error stopping view: %s", error->message);
      GCAL_RETURN ();
    }

  g_clear_object (&self->monitor_thread.view);
  self->monitor_thread.populated = FALSE;

  GCAL_EXIT;
}

typedef struct
{
  GSource              parent;
  GcalCalendarMonitor *monitor;
  GMainLoop           *mainloop;
} MessageQueueSource;

static gboolean
message_queue_source_prepare (GSource *source,
                              gint    *timeout)
{
  GcalCalendarMonitor *self;
  MessageQueueSource *queue_source;

  queue_source = (MessageQueueSource*) source;
  self = queue_source->monitor;

  return g_async_queue_length (self->messages) > 0;
}

static gboolean
message_queue_source_dispatch (GSource     *source,
                               GSourceFunc  callback,
                               gpointer     user_data)
{
  GcalCalendarMonitor *self;
  MessageQueueSource *queue_source;
  MonitorThreadEvent event;

  GCAL_ENTRY;

  queue_source = (MessageQueueSource*) source;
  self = queue_source->monitor;
  event = GPOINTER_TO_INT (g_async_queue_pop (self->messages));

  switch (event)
    {
    case FILTER_UPDATED:
    case RANGE_UPDATED:
      remove_view (self);
      create_view (self);
      break;

    case CREATE_VIEW:
      create_view (self);
      break;

    case REMOVE_VIEW:
      remove_view (self);
      break;

    case INVALID_EVENT:
    case QUIT:
      remove_view (self);
      g_main_loop_quit (queue_source->mainloop);
      GCAL_RETURN (G_SOURCE_REMOVE);
    }

  GCAL_RETURN (G_SOURCE_CONTINUE);
}

static GSourceFuncs monitor_queue_source_funcs =
{
  message_queue_source_prepare,
  NULL,
  message_queue_source_dispatch,
  NULL,
};

static MessageQueueSource*
views_monitor_source_new (GcalCalendarMonitor *self,
                          GMainLoop           *mainloop)
{
  MessageQueueSource *queue_source;
  GSource *source;

  /* Prepare the message queue source */
  source = g_source_new (&monitor_queue_source_funcs, sizeof (MessageQueueSource));
  queue_source = (MessageQueueSource*) source;
  queue_source->monitor = self;
  queue_source->mainloop = mainloop;
  g_source_set_name (source, "Message Queue Source");

  return queue_source;
}

static gpointer
calendar_view_thread_func (gpointer data)
{
  g_autoptr (GMainLoop) mainloop = NULL;
  MessageQueueSource *queue_source;
  GcalCalendarMonitor *self;

  GCAL_ENTRY;

  self = data;
  mainloop = g_main_loop_new (self->thread_context, FALSE);
  g_main_context_push_thread_default (self->thread_context);

  /* Events source */
  queue_source = views_monitor_source_new (self, mainloop);
  g_source_attach ((GSource*) queue_source, self->thread_context);

  g_main_loop_run (mainloop);
  g_main_loop_quit (mainloop);

  g_main_context_pop_thread_default (self->thread_context);

  GCAL_RETURN (NULL);
}

/*
 * Auxiliary methods
 */

static void
notify_view_thread (GcalCalendarMonitor *self,
                    MonitorThreadEvent   event)
{
  g_async_queue_push (self->messages, GINT_TO_POINTER (event));
  g_main_context_wakeup (self->thread_context);
}

static void
maybe_spawn_view_thread (GcalCalendarMonitor *self)
{
  g_autofree gchar *thread_name = NULL;

  if (self->thread || !self->shared.range)
    return;

  thread_name = g_strdup_printf ("GcalCalendarMonitor (%s)", gcal_calendar_get_id (self->calendar));
  self->thread = g_thread_new (thread_name, calendar_view_thread_func, self);

  g_debug ("Spawning thread %s", thread_name);
}

static void
remove_events_outside_range (GcalCalendarMonitor *self,
                             GcalRange           *range)
{
  g_autoptr (GRWLockWriterLocker) writer_locker = NULL;
  GHashTableIter iter;
  GcalEvent *event;

  GCAL_TRACE_MSG ("Removing events outside range from monitor");

  writer_locker = g_rw_lock_writer_locker_new (&self->shared.lock);

  g_hash_table_iter_init (&iter, self->shared.events);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer*) &event))
    {
      g_object_ref (event);

      g_hash_table_iter_remove (&iter);
      g_signal_emit (self, signals[EVENT_REMOVED], 0, event);

      g_object_unref (event);
    }
}

static void
remove_all_events (GcalCalendarMonitor *self)
{
  g_autoptr (GRWLockWriterLocker) writer_locker = NULL;
  GHashTableIter iter;
  GcalEvent *event;

  GCAL_TRACE_MSG ("Removing all events from view");

  writer_locker = g_rw_lock_writer_locker_new (&self->shared.lock);

  g_hash_table_iter_init (&iter, self->shared.events);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer*) &event))
    {
      g_hash_table_iter_remove (&iter);
      g_signal_emit (self, signals[EVENT_REMOVED], 0, event);
    }
}

static void
set_complete (GcalCalendarMonitor *self,
              gboolean             complete)
{
  if (self->complete == complete)
    return;

  GCAL_TRACE_MSG ("Setting complete to %s", complete ? "TRUE" : "FALSE");

  self->complete = complete;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_COMPLETE]);
}


/*
 * Callbacks
 */

static gboolean
add_event_to_timeline_in_idle_cb (gpointer user_data)
{
  g_autoptr (GRWLockWriterLocker) writer_locker = NULL;
  GcalCalendarMonitor *self;
  GcalEvent *event;
  IdleData *idle_data;
  const gchar *event_id;

  GCAL_ENTRY;

  idle_data = user_data;
  self = idle_data->monitor;
  event = idle_data->event;
  g_assert (idle_data->event_id == NULL);

  event_id = gcal_event_get_uid (event);

  writer_locker = g_rw_lock_writer_locker_new (&self->shared.lock);
  if (g_hash_table_insert (self->shared.events, g_strdup (event_id), g_object_ref (event)))
    g_signal_emit (self, signals[EVENT_ADDED], 0, event);

  GCAL_RETURN (G_SOURCE_REMOVE);
}

static gboolean
update_event_in_idle_cb (gpointer user_data)
{
  g_autoptr (GRWLockWriterLocker) writer_locker = NULL;
  GcalCalendarMonitor *self;
  GcalEvent *old_event;
  GcalEvent *event;
  IdleData *idle_data;
  const gchar *event_id;

  GCAL_ENTRY;

  idle_data = user_data;
  self = idle_data->monitor;
  event = idle_data->event;
  g_assert (idle_data->event_id == NULL);

  event_id = gcal_event_get_uid (event);

  writer_locker = g_rw_lock_writer_locker_new (&self->shared.lock);
  old_event = g_hash_table_lookup (self->shared.events, event_id);
  if (old_event)
    {
      g_hash_table_insert (self->shared.events, g_strdup (event_id), g_object_ref (event));
      g_signal_emit (self, signals[EVENT_UPDATED], 0, old_event, event);
    }

  GCAL_RETURN (G_SOURCE_REMOVE);
}

static gboolean
remove_event_from_timeline_in_idle_cb (gpointer user_data)
{
  g_autoptr (GRWLockWriterLocker) writer_locker = NULL;
  g_autoptr (GcalEvent) event = NULL;
  GcalCalendarMonitor *self;
  IdleData *idle_data;
  gchar *event_id;

  GCAL_ENTRY;

  idle_data = user_data;
  self = idle_data->monitor;
  g_assert (idle_data->event == NULL);
  event_id = idle_data->event_id;

  writer_locker = g_rw_lock_writer_locker_new (&self->shared.lock);
  event = g_hash_table_lookup (self->shared.events, event_id);

  if (event)
    {
      /* Keep the event alive until the signal is emitted */
      g_object_ref (event);

      g_hash_table_remove (self->shared.events, event_id);
      g_signal_emit (self, signals[EVENT_REMOVED], 0, event);
    }

  GCAL_RETURN (G_SOURCE_REMOVE);
}

static void
on_calendar_visible_changed_cb (GcalCalendar        *calendar,
                                GParamSpec          *pspec,
                                GcalCalendarMonitor *self)
{
  if (gcal_calendar_get_visible (calendar))
    {
      notify_view_thread (self, CREATE_VIEW);
    }
  else
    {
      remove_all_events (self);
      notify_view_thread (self, REMOVE_VIEW);
    }
}

static gboolean
complete_in_idle_cb (gpointer user_data)
{
  GcalCalendarMonitor *self;
  IdleData *idle_data;

  GCAL_ENTRY;

  idle_data = user_data;
  self = idle_data->monitor;
  g_assert (idle_data->event == NULL);
  g_assert (idle_data->event_id == NULL);

  set_complete (self, idle_data->complete);

  GCAL_RETURN (G_SOURCE_REMOVE);
}


/*
 * GObject overrides
 */

static void
gcal_calendar_monitor_finalize (GObject *object)
{
  GcalCalendarMonitor *self = (GcalCalendarMonitor *)object;

  g_cancellable_cancel (self->cancellable);
  notify_view_thread (self, QUIT);
  g_clear_pointer (&self->thread, g_thread_join);

  remove_all_events (self);

  g_clear_object (&self->calendar);
  g_clear_pointer (&self->thread_context, g_main_context_unref);
  g_clear_pointer (&self->main_context, g_main_context_unref);
  g_clear_pointer (&self->messages, g_async_queue_unref);
  g_clear_pointer (&self->shared.events, g_hash_table_destroy);
  g_clear_pointer (&self->shared.filter, g_free);
  g_clear_pointer (&self->shared.range, gcal_range_unref);

  G_OBJECT_CLASS (gcal_calendar_monitor_parent_class)->finalize (object);
}

static void
gcal_calendar_monitor_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  GcalCalendarMonitor *self = GCAL_CALENDAR_MONITOR (object);

  switch (prop_id)
    {
    case PROP_CALENDAR:
      g_value_set_object (value, self->calendar);
      break;

    case PROP_COMPLETE:
      g_value_set_boolean (value, self->complete);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_calendar_monitor_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  GcalCalendarMonitor *self = GCAL_CALENDAR_MONITOR (object);

  switch (prop_id)
    {
    case PROP_CALENDAR:
      g_assert (self->calendar == NULL);
      self->calendar = g_value_dup_object (value);
      g_signal_connect (self->calendar,
                        "notify::visible",
                        G_CALLBACK (on_calendar_visible_changed_cb),
                        self);
      break;

    case PROP_COMPLETE:
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_calendar_monitor_class_init (GcalCalendarMonitorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gcal_calendar_monitor_finalize;
  object_class->get_property = gcal_calendar_monitor_get_property;
  object_class->set_property = gcal_calendar_monitor_set_property;

  signals[EVENT_ADDED] = g_signal_new ("event-added",
                                       GCAL_TYPE_CALENDAR_MONITOR,
                                       G_SIGNAL_RUN_FIRST,
                                       0, NULL, NULL, NULL,
                                       G_TYPE_NONE,
                                       1,
                                       GCAL_TYPE_EVENT);

  signals[EVENT_UPDATED] = g_signal_new ("event-updated",
                                         GCAL_TYPE_CALENDAR_MONITOR,
                                         G_SIGNAL_RUN_FIRST,
                                         0, NULL, NULL, NULL,
                                         G_TYPE_NONE,
                                         2,
                                         GCAL_TYPE_EVENT,
                                         GCAL_TYPE_EVENT);

  signals[EVENT_REMOVED] = g_signal_new ("event-removed",
                                         GCAL_TYPE_CALENDAR_MONITOR,
                                         G_SIGNAL_RUN_FIRST,
                                         0, NULL, NULL, NULL,
                                         G_TYPE_NONE,
                                         1,
                                         GCAL_TYPE_EVENT);

  properties[PROP_CALENDAR] = g_param_spec_object ("calendar",
                                                   "Calendar",
                                                   "Calendar to be monitored",
                                                   GCAL_TYPE_CALENDAR,
                                                   G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);


  properties[PROP_COMPLETE] = g_param_spec_boolean ("complete",
                                                    "Complete",
                                                    "Whether",
                                                    FALSE,
                                                    G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gcal_calendar_monitor_init (GcalCalendarMonitor *self)
{
  self->shared.events = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
  self->thread_context = g_main_context_new ();
  self->main_context = g_main_context_ref_thread_default ();
  self->messages = g_async_queue_new ();
  self->complete = FALSE;

  g_rw_lock_init (&self->shared.lock);
}

GcalCalendarMonitor*
gcal_calendar_monitor_new (GcalCalendar *calendar)
{
  return g_object_new (GCAL_TYPE_CALENDAR_MONITOR,
                       "calendar", calendar,
                       NULL);
}

/**
 * gcal_calendar_monitor_set_range:
 * @self: a #GcalCalendarMonitor
 * @range: a #GcalRange
 *
 * Updates the range of @self. This usually will result in
 * @self creating a new view, and gathering the events from
 * the events server.
 */
void
gcal_calendar_monitor_set_range (GcalCalendarMonitor *self,
                                 GcalRange           *range)
{
  g_autoptr (GRWLockWriterLocker) writer_locker = NULL;
  gboolean range_changed;

  g_return_if_fail (GCAL_IS_CALENDAR_MONITOR (self));

  GCAL_ENTRY;

  writer_locker = g_rw_lock_writer_locker_new (&self->shared.lock);

  range_changed =
    !self->shared.range ||
    !range ||
    gcal_range_calculate_overlap (self->shared.range, range, NULL) != GCAL_RANGE_EQUAL;

  if (!range_changed)
    GCAL_RETURN ();

  g_clear_pointer (&self->shared.range, gcal_range_unref);
  self->shared.range = range ? gcal_range_copy (range) : NULL;

  g_clear_pointer (&writer_locker, g_rw_lock_writer_locker_free);

  maybe_spawn_view_thread (self);
  remove_events_outside_range (self, range);

  g_cancellable_cancel (self->cancellable);

  if (gcal_calendar_get_visible (self->calendar))
    notify_view_thread (self, RANGE_UPDATED);

  GCAL_EXIT;
}

/**
 * gcal_calendar_monitor_get_cached_event:
 * @self: a #GcalCalendarMonitor
 * @event_id: the id of the #GcalEvent
 *
 * Returns: (transfer full)(nullable): a #GcalEvent, or %NULL
 */
GcalEvent*
gcal_calendar_monitor_get_cached_event (GcalCalendarMonitor *self,
                                        const gchar         *event_id)
{
  GcalEvent *event;

  g_return_val_if_fail (GCAL_IS_CALENDAR_MONITOR (self), NULL);
  g_return_val_if_fail (event_id, NULL);

  g_rw_lock_reader_lock (&self->shared.lock);
  event = g_hash_table_lookup (self->shared.events, event_id);
  g_rw_lock_reader_unlock (&self->shared.lock);

  return event ? g_object_ref (event) : NULL;
}

void
gcal_calendar_monitor_set_filter (GcalCalendarMonitor *self,
                                  const gchar         *filter)
{
  g_return_if_fail (GCAL_IS_CALENDAR_MONITOR (self));

  g_rw_lock_writer_lock (&self->shared.lock);

  g_clear_pointer (&self->shared.filter, g_free);
  self->shared.filter = g_strdup (filter);

  g_rw_lock_writer_unlock (&self->shared.lock);

  remove_all_events (self);

  if (gcal_calendar_get_visible (self->calendar))
    notify_view_thread (self, FILTER_UPDATED);
}

gboolean
gcal_calendar_monitor_is_complete (GcalCalendarMonitor *self)
{
  g_return_val_if_fail (GCAL_IS_CALENDAR_MONITOR (self), FALSE);

  return self->complete;
}
