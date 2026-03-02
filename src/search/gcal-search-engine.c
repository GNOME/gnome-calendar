/* gcal-search-engine.c
 *
 * Copyright 2019 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#define G_LOG_DOMAIN "GcalSearchEngine"

#include "gcal-utils.h"
#include "gcal-date-time-utils.h"
#include "gcal-debug.h"
#include "gcal-search-engine.h"
#include "gcal-search-model.h"
#include "gcal-timeline.h"
#include "gcal-timeline-subscriber.h"
#include "gcal-utils.h"

struct _GcalSearchEngine
{
  GObject             parent;

  GcalTimeline       *timeline;
};

G_DEFINE_TYPE (GcalSearchEngine, gcal_search_engine, G_TYPE_OBJECT)


/*
 * Callbacks
 */

static void
on_manager_calendar_added_cb (GcalManager      *manager,
                              GcalCalendar     *calendar,
                              GcalSearchEngine *self)
{
  g_debug ("Adding calendar %s to search results", gcal_calendar_get_id (calendar));

  gcal_timeline_add_calendar (self->timeline, calendar);
}

static void
on_manager_calendar_removed_cb (GcalManager      *manager,
                                GcalCalendar     *calendar,
                                GcalSearchEngine *self)
{
  g_debug ("Removing calendar %s from search results", gcal_calendar_get_id (calendar));

  gcal_timeline_remove_calendar (self->timeline, calendar);
}

static void
search_model_hits_cb (GObject      *source,
                      GAsyncResult *result,
                      gpointer      data)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (GTask) task = data;
  GcalSearchModel *search_model;

  GCAL_ENTRY;

  search_model = GCAL_SEARCH_MODEL (source);
  gcal_search_model_wait_for_hits_finish (search_model, result, &error);

  if (error)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      GCAL_RETURN ();
    }

  g_task_return_pointer (task, g_object_ref (search_model), NULL);

  GCAL_EXIT;
}


/*
 * GObject overrides
 */

static void
gcal_search_engine_finalize (GObject *object)
{
  GcalSearchEngine *self = (GcalSearchEngine *)object;

  g_clear_object (&self->timeline);

  G_OBJECT_CLASS (gcal_search_engine_parent_class)->finalize (object);
}

static void
gcal_search_engine_constructed (GObject *object)
{
  GcalSearchEngine *self = (GcalSearchEngine *)object;
  GcalContext *context;
  GcalManager *manager;

  G_OBJECT_CLASS (gcal_search_engine_parent_class)->constructed (object);

  /* Setup the data model */
  self->timeline = gcal_timeline_new ();

  context = gcal_application_get_context (GCAL_DEFAULT_APPLICATION);
  manager = gcal_context_get_manager (context);
  g_signal_connect_object (manager, "calendar-added", G_CALLBACK (on_manager_calendar_added_cb), self, 0);
  g_signal_connect_object (manager, "calendar-removed", G_CALLBACK (on_manager_calendar_removed_cb), self, 0);
}

static void
gcal_search_engine_class_init (GcalSearchEngineClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gcal_search_engine_finalize;
  object_class->constructed = gcal_search_engine_constructed;
}

static void
gcal_search_engine_init (GcalSearchEngine *self)
{
}

GcalSearchEngine *
gcal_search_engine_new (void)
{
  return g_object_new (GCAL_TYPE_SEARCH_ENGINE,
                       NULL);
}

void
gcal_search_engine_search (GcalSearchEngine    *self,
                           const gchar         *search_query,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  g_autoptr (GcalSearchModel) model = NULL;
  g_autoptr (GDateTime) range_start = NULL;
  g_autoptr (GDateTime) range_end = NULL;
  g_autoptr (GDateTime) now = NULL;
  g_autoptr (GTask) task = NULL;
  GcalContext *context;
  GTimeZone *timezone;

  g_return_if_fail (GCAL_IS_SEARCH_ENGINE (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  context = gcal_application_get_context (GCAL_DEFAULT_APPLICATION);
  timezone = gcal_context_get_timezone (context);
  now = g_date_time_new_now (timezone);
  range_start = g_date_time_add_months (now, -N_WEEKDAYS - 1);
  range_end = g_date_time_add_months (now, N_WEEKDAYS - 1);
  model = gcal_search_model_new (cancellable, range_start, range_end);

  gcal_timeline_set_filter (self->timeline, search_query);
  gcal_timeline_add_subscriber (self->timeline, GCAL_TIMELINE_SUBSCRIBER (model));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, gcal_search_engine_search);
  g_task_set_priority (task, G_PRIORITY_LOW);

  gcal_search_model_wait_for_hits (model, cancellable, search_model_hits_cb, g_object_ref (task));
}

GListModel*
gcal_search_engine_search_finish (GcalSearchEngine  *self,
                                  GAsyncResult      *result,
                                  GError           **error)
{
  g_return_val_if_fail (GCAL_IS_SEARCH_ENGINE (self), NULL);
  g_return_val_if_fail (G_IS_TASK (result), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}
