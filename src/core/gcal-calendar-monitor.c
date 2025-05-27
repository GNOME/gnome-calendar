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
#include "gcal-core-macros.h"
#include "gcal-date-time-utils.h"
#include "gcal-debug.h"
#include "gcal-event.h"

#include <gio/gio.h>
#include <libecal/libecal.h>

typedef struct
{
  GcalCalendarMonitor *monitor;
  GPtrArray           *events;
  GPtrArray           *event_ids;
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

  const GcalCalendarMonitorListener *listener;
  gpointer                           listener_user_data;

  /*
   * These fields are only accessed on the monitor thread, and
   * never on the main thread.
   */
  struct {
    gboolean          populated;
    GPtrArray        *events_to_add;
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

static gboolean      add_events_to_timeline_in_idle_cb           (gpointer           user_data);
static gboolean      update_events_in_idle_cb                    (gpointer           user_data);
static gboolean      remove_events_from_timeline_in_idle_cb      (gpointer           user_data);
static gboolean      complete_in_idle_cb                         (gpointer           user_data);

G_DEFINE_TYPE (GcalCalendarMonitor, gcal_calendar_monitor, G_TYPE_OBJECT)

enum
{
  PROP_0,
  PROP_CALENDAR,
  PROP_COMPLETE,
  N_PROPS
};

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
    self->monitor_thread.events_to_add = g_ptr_array_new_with_free_func (g_object_unref);
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
  g_clear_pointer (&data->events, g_ptr_array_unref);
  g_clear_pointer (&data->event_ids, g_ptr_array_unref);
  g_free (data);
}

static void
add_events_in_idle (GcalCalendarMonitor *self,
                    GPtrArray           *events)
{
  IdleData *idle_data;

  g_assert (GCAL_IS_THREAD (self->thread));

  idle_data = g_new0 (IdleData, 1);
  idle_data->monitor = g_object_ref (self);
  idle_data->events = g_ptr_array_ref (events);

  g_main_context_invoke_full (self->main_context,
                              G_PRIORITY_DEFAULT_IDLE,
                              add_events_to_timeline_in_idle_cb,
                              idle_data,
                              (GDestroyNotify) idle_data_free);
}

static void
update_events_in_idle (GcalCalendarMonitor *self,
                       GPtrArray           *events)
{
  IdleData *idle_data;

  g_assert (GCAL_IS_THREAD (self->thread));

  idle_data = g_new0 (IdleData, 1);
  idle_data->monitor = g_object_ref (self);
  idle_data->events = g_ptr_array_ref (events);

  g_main_context_invoke_full (self->main_context,
                              G_PRIORITY_DEFAULT_IDLE,
                              update_events_in_idle_cb,
                              idle_data,
                              (GDestroyNotify) idle_data_free);
}

static void
remove_events_in_idle (GcalCalendarMonitor *self,
                       GPtrArray           *events)
{
  IdleData *idle_data;

  g_assert (GCAL_IS_THREAD (self->thread));

  idle_data = g_new0 (IdleData, 1);
  idle_data->monitor = g_object_ref (self);
  idle_data->event_ids = g_ptr_array_ref (events);

  g_main_context_invoke_full (self->main_context,
                              G_PRIORITY_DEFAULT_IDLE,
                              remove_events_from_timeline_in_idle_cb,
                              idle_data,
                              (GDestroyNotify) idle_data_free);
}

static void
set_complete_in_idle (GcalCalendarMonitor *self,
                      gboolean             complete)
{
  IdleData *idle_data;

  g_assert (GCAL_IS_THREAD (self->thread));

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

  g_assert (GCAL_IS_THREAD (self->thread));

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
  g_autoptr (GPtrArray) events_to_add = NULL;
  g_autoptr (GcalRange) range = NULL;
  const GSList *l;
  gint i;

  GCAL_ENTRY;

  g_assert (GCAL_IS_THREAD (self->thread));

  range = get_monitor_ranges (self);

  maybe_init_event_arrays (self);
  components_to_expand = g_ptr_array_new ();
  events_to_add = g_ptr_array_new_with_free_func (g_object_unref);

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
        g_ptr_array_add (self->monitor_thread.events_to_add, g_object_ref (event));
      else
        g_ptr_array_add (events_to_add, g_object_ref (event));
    }

  /* Recurrent events */
  if (components_to_expand->len > 0)
    {
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

          if (!self->monitor_thread.populated)
            recurrences_data.expanded_events = self->monitor_thread.events_to_add;
          else
            recurrences_data.expanded_events = events_to_add;

#if GCAL_ENABLE_TRACE
          old_size = recurrences_data.expanded_events->len;
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
                              recurrences_data.expanded_events->len - old_size,
                              range_str);
            }
#endif
        }
    }

  if (events_to_add->len > 0)
    add_events_in_idle (self, events_to_add);

  GCAL_EXIT;
}

static void
on_client_view_objects_modified_cb (ECalClientView      *view,
                                    const GSList        *objects,
                                    GcalCalendarMonitor *self)
{
  g_autoptr (GHashTable) events_to_remove = NULL;
  g_autoptr (GPtrArray) components_to_expand = NULL;
  g_autoptr (GPtrArray) event_ids_to_remove = NULL;
  g_autoptr (GPtrArray) events_to_update = NULL;
  g_autoptr (GcalRange) range = NULL;
  const GSList *l;

  GCAL_ENTRY;

  g_assert (GCAL_IS_THREAD (self->thread));

  if (!self->monitor_thread.populated && self->monitor_thread.events_to_add)
    {
      g_clear_pointer (&self->monitor_thread.events_to_add, g_ptr_array_unref);
      return;
    }

  G_RW_LOCK_READER_AUTO_LOCK (&self->shared.lock, reader_locker);
  components_to_expand = g_ptr_array_new ();
  events_to_remove = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  events_to_update = g_ptr_array_new_with_free_func (g_object_unref);
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
          GHashTableIter iter;
          const gchar *aux;

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

      g_ptr_array_add (events_to_update, g_steal_pointer (&event));
    }

  /* Recurrent events */
  if (components_to_expand->len > 0)
    {
      g_autoptr (GPtrArray) expanded_events = NULL;
      g_autoptr (GPtrArray) events_to_add = NULL;
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
      events_to_add = g_ptr_array_new_with_free_func (g_object_unref);

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
              g_hash_table_remove (events_to_remove, gcal_event_get_uid (event));
              g_ptr_array_add (events_to_update, g_object_ref (event));
            }
          else
            {
              g_ptr_array_add (events_to_add, g_object_ref (event));
            }
        }

      if (events_to_add->len > 0)
        add_events_in_idle (self, events_to_add);
    }

  if (events_to_update->len > 0)
    update_events_in_idle (self, events_to_update);

  /* Now remove lingering events */
  event_ids_to_remove = g_hash_table_steal_all_keys (events_to_remove);
  if (event_ids_to_remove->len > 0)
    remove_events_in_idle (self, event_ids_to_remove);

  GCAL_EXIT;
}

static void
on_client_view_objects_removed_cb (ECalClientView      *view,
                                   const GSList        *objects,
                                   GcalCalendarMonitor *self)
{
  g_autoptr (GPtrArray) event_ids = NULL;
  const GSList *l;

  GCAL_ENTRY;

  g_assert (GCAL_IS_THREAD (self->thread));

  event_ids = g_ptr_array_new_with_free_func (g_free);

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
          GHashTableIter iter;
          const gchar *aux;

          event_id = g_strdup_printf ("%s:%s",
                                      gcal_calendar_get_id (self->calendar),
                                      e_cal_component_id_get_uid (component_id));

          /*
           * If this is the main component, remove the expanded recurrency instances
           * as well.
           */
          G_RW_LOCK_READER_AUTO_LOCK (&self->shared.lock, reader_locker);

          g_hash_table_iter_init (&iter, self->shared.events);
          while (g_hash_table_iter_next (&iter, (gpointer*) &aux, NULL))
            {
              if (!g_str_equal (aux, event_id) && g_str_has_prefix (aux, event_id))
                g_ptr_array_add (event_ids, g_strdup (aux));
            }
        }

      g_ptr_array_add (event_ids, g_strdup (event_id));
    }

  if (event_ids->len > 0)
    remove_events_in_idle (self, event_ids);

  GCAL_EXIT;
}

static void
on_client_view_complete_cb (ECalClientView      *view,
                            const GError        *error,
                            GcalCalendarMonitor *self)
{
  g_autoptr (GPtrArray) events_to_add = NULL;

  GCAL_ENTRY;

  g_assert (GCAL_IS_THREAD (self->thread));
  g_assert (!self->monitor_thread.populated);

  events_to_add = g_steal_pointer (&self->monitor_thread.events_to_add);

  if (events_to_add)
    add_events_in_idle (self, events_to_add);

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

  g_assert (GCAL_IS_THREAD (self->thread));

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

  g_assert (GCAL_IS_THREAD (self->thread));

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

  /*
   * The prepare function can be called before g_thread_new()
   * returns, and that's valid. However, in that case, no messages
   * should be available in the queue, as the main thread didn't
   * return execution to the mainloop yet. Protect against that.
   */
  g_assert (GCAL_IS_THREAD (self->thread) ||
            (!self->thread && g_async_queue_length (self->messages) == 0));

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

  g_assert (GCAL_IS_THREAD (self->thread));

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
  g_assert (GCAL_IS_MAIN_THREAD ());

  g_async_queue_push (self->messages, GINT_TO_POINTER (event));
  g_main_context_wakeup (self->thread_context);
}

static void
maybe_spawn_view_thread (GcalCalendarMonitor *self)
{
  g_autofree gchar *thread_name = NULL;

  g_assert (GCAL_IS_MAIN_THREAD ());

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
  g_autoptr (GPtrArray) events_to_remove = NULL;
  GHashTableIter iter;
  GcalEvent *event;

  g_assert (GCAL_IS_MAIN_THREAD ());
  g_assert (range != NULL);

  GCAL_TRACE_MSG ("Removing events outside range from monitor");

  G_RW_LOCK_WRITER_AUTO_LOCK (&self->shared.lock, writer_locker);

  events_to_remove = g_ptr_array_new_full (g_hash_table_size (self->shared.events),
                                           g_object_unref);

  g_hash_table_iter_init (&iter, self->shared.events);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer*) &event))
    {
      GcalRange *event_range = gcal_event_get_range (event);

      if (gcal_range_calculate_overlap (range, event_range, NULL) != GCAL_RANGE_NO_OVERLAP)
        continue;

      g_ptr_array_add (events_to_remove, event);
      g_hash_table_iter_steal (&iter);
    }

  if (events_to_remove->len > 0)
    self->listener->remove_events (self, events_to_remove, self->listener_user_data);
}

static void
remove_all_events (GcalCalendarMonitor *self)
{
  g_autoptr (GPtrArray) events_to_remove = NULL;

  g_assert (GCAL_IS_MAIN_THREAD ());

  GCAL_TRACE_MSG ("Removing all events from view");

  G_RW_LOCK_WRITER_AUTO_LOCK (&self->shared.lock, writer_locker);

  events_to_remove = g_hash_table_steal_all_values (self->shared.events);

  if (events_to_remove->len > 0)
    self->listener->remove_events (self, events_to_remove, self->listener_user_data);
}

static void
set_complete (GcalCalendarMonitor *self,
              gboolean             complete)
{
  g_assert (GCAL_IS_MAIN_THREAD ());

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
add_events_to_timeline_in_idle_cb (gpointer user_data)
{
  g_autoptr (GPtrArray) events_to_add = NULL;
  GcalCalendarMonitor *self;
  GPtrArray *events;
  IdleData *idle_data;

  GCAL_ENTRY;

  g_assert (GCAL_IS_MAIN_THREAD ());

  idle_data = user_data;
  self = idle_data->monitor;
  events = idle_data->events;
  g_assert (idle_data->event_ids == NULL);

  events_to_add = g_ptr_array_sized_new (events->len);

  G_RW_LOCK_WRITER_AUTO_LOCK (&self->shared.lock, writer_locker);
  for (guint i = 0; i < events->len; i++)
    {
      GcalEvent *event;
      const gchar *uid;

      event = g_ptr_array_index (events, i);
      uid = gcal_event_get_uid (event);

      if (!g_hash_table_contains (self->shared.events, uid))
        {
          g_hash_table_insert (self->shared.events, g_strdup (uid), g_object_ref (event));
          g_ptr_array_add (events_to_add, event);
        }
    }

  if (events_to_add->len > 0)
    self->listener->add_events (self, events_to_add, self->listener_user_data);

  GCAL_RETURN (G_SOURCE_REMOVE);
}

static gboolean
update_events_in_idle_cb (gpointer user_data)
{
  g_autoptr (GPtrArray) old_events = NULL;
  g_autoptr (GPtrArray) new_events = NULL;
  g_autoptr (GPtrArray) events = NULL;
  GcalCalendarMonitor *self;
  IdleData *idle_data;

  GCAL_ENTRY;

  g_assert (GCAL_IS_MAIN_THREAD ());

  idle_data = user_data;
  self = idle_data->monitor;
  events = g_steal_pointer (&idle_data->events);
  g_assert (idle_data->event_ids == NULL);

  old_events = g_ptr_array_new_full (events->len, g_object_unref);
  new_events = g_ptr_array_sized_new (events->len);

  G_RW_LOCK_WRITER_AUTO_LOCK (&self->shared.lock, writer_locker);
  for (guint i = 0; i < events->len; i++)
    {
      g_autoptr (GcalEvent) old_event = NULL;
      g_autofree gchar *old_event_id = NULL;
      GcalEvent *event;
      const gchar *event_id;

      event = g_ptr_array_index (events, i);
      event_id = gcal_event_get_uid (event);

      if (g_hash_table_steal_extended (self->shared.events,
                                       event_id,
                                       (gpointer*) &old_event_id,
                                       (gpointer*) &old_event))
        {
          g_hash_table_insert (self->shared.events, g_strdup (event_id), g_object_ref (event));
          g_ptr_array_add (old_events, g_steal_pointer (&old_event));
          g_ptr_array_add (new_events, event);
        }
    }

  g_assert (old_events->len == new_events->len);

  if (new_events->len > 0)
    self->listener->update_events (self, old_events, new_events, self->listener_user_data);

  GCAL_RETURN (G_SOURCE_REMOVE);
}

static gboolean
remove_events_from_timeline_in_idle_cb (gpointer user_data)
{
  g_autoptr (GPtrArray) events_to_remove = NULL;
  g_autoptr (GPtrArray) event_ids = NULL;
  GcalCalendarMonitor *self;
  IdleData *idle_data;

  GCAL_ENTRY;

  g_assert (GCAL_IS_MAIN_THREAD ());

  idle_data = user_data;
  self = idle_data->monitor;
  event_ids = g_steal_pointer (&idle_data->event_ids);
  g_assert (idle_data->events == NULL);

  events_to_remove = g_ptr_array_new_full (event_ids->len, g_object_unref);

  G_RW_LOCK_WRITER_AUTO_LOCK (&self->shared.lock, writer_locker);

  for (guint i = 0; i < event_ids->len; i++)
    {
      const gchar *event_id;
      GcalEvent *event;

      event_id = g_ptr_array_index (event_ids, i);
      event = g_hash_table_lookup (self->shared.events, event_id);

      if (event)
        {
          /* Keep the event alive until the listener process it*/
          g_ptr_array_add (events_to_remove, g_object_ref (event));
          g_hash_table_remove (self->shared.events, event_id);
        }
    }

  if (events_to_remove->len > 0)
    self->listener->remove_events (self, events_to_remove, self->listener_user_data);

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

  g_assert (GCAL_IS_MAIN_THREAD ());

  idle_data = user_data;
  self = idle_data->monitor;
  g_assert (idle_data->events == NULL);
  g_assert (idle_data->event_ids == NULL);

  set_complete (self, idle_data->complete);

  GCAL_RETURN (G_SOURCE_REMOVE);
}


/*
 * GObject overrides
 */

static void
gcal_calendar_monitor_dispose (GObject *object)
{
  GcalCalendarMonitor *self = (GcalCalendarMonitor *)object;

  g_cancellable_cancel (self->cancellable);
  notify_view_thread (self, QUIT);

  if (self->thread)
    {
      g_thread_join (self->thread);
      self->thread = NULL;
    }

  remove_all_events (self);

  g_clear_object (&self->cancellable);

  G_OBJECT_CLASS (gcal_calendar_monitor_parent_class)->dispose (object);
}

static void
gcal_calendar_monitor_finalize (GObject *object)
{
  GcalCalendarMonitor *self = (GcalCalendarMonitor *)object;

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

  object_class->dispose = gcal_calendar_monitor_dispose;
  object_class->finalize = gcal_calendar_monitor_finalize;
  object_class->get_property = gcal_calendar_monitor_get_property;
  object_class->set_property = gcal_calendar_monitor_set_property;

  properties[PROP_CALENDAR] = g_param_spec_object ("calendar", NULL, NULL,
                                                   GCAL_TYPE_CALENDAR,
                                                   G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  properties[PROP_COMPLETE] = g_param_spec_boolean ("complete", NULL, NULL,
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
gcal_calendar_monitor_new (GcalCalendar                      *calendar,
                           const GcalCalendarMonitorListener *listener,
                           gpointer                           user_data)
{
  g_autoptr (GcalCalendarMonitor) monitor = NULL;

  g_assert (calendar != NULL && GCAL_IS_CALENDAR (calendar));
  g_assert (listener != NULL);

  monitor = g_object_new (GCAL_TYPE_CALENDAR_MONITOR,
                          "calendar", calendar,
                          NULL);

  monitor->listener = listener;
  monitor->listener_user_data = user_data;

  return g_steal_pointer (&monitor);
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

  if (range)
    remove_events_outside_range (self, range);
  else
    remove_all_events (self);

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
