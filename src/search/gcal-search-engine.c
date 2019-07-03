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

#include "gcal-context.h"
#include "gcal-date-time-utils.h"
#include "gcal-search-engine.h"
#include "gcal-search-model.h"
#include "gcal-thread-utils.h"

#include <dazzle.h>

typedef struct
{
  GcalSearchEngine   *engine;
  gchar              *query;
  gint                max_results;
  GDateTime          *range_start;
  GDateTime          *range_end;
} SearchData;

struct _GcalSearchEngine
{
  GObject             parent;

  ECalDataModel      *data_model;

  GcalContext        *context;
};

G_DEFINE_TYPE (GcalSearchEngine, gcal_search_engine, G_TYPE_OBJECT)

enum
{
  PROP_0,
  PROP_CONTEXT,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];


/*
 * Auxiliary methods
 */

static void
search_data_free (gpointer data)
{
  SearchData *search_data = data;

  if (!data)
    return;

  gcal_clear_date_time (&search_data->range_start);
  gcal_clear_date_time (&search_data->range_end);
  g_clear_pointer (&search_data->query, g_free);
  g_clear_object (&search_data->engine);
  g_slice_free (SearchData, data);
}


/*
 * Callbacks
 */

static void
search_func (GTask        *task,
             gpointer      source_object,
             gpointer      task_data,
             GCancellable *cancellable)
{
  g_autoptr (GcalSearchModel) model = NULL;
  GcalSearchEngine *self;
  SearchData *data;
  time_t start;
  time_t end;

  self = GCAL_SEARCH_ENGINE (source_object);
  data = (SearchData*) task_data;
  start = g_date_time_to_unix (data->range_start);
  end = g_date_time_to_unix (data->range_end);

  model = gcal_search_model_new (cancellable, data->max_results);
  e_cal_data_model_set_filter (self->data_model, data->query);

  e_cal_data_model_subscribe (self->data_model,
                              E_CAL_DATA_MODEL_SUBSCRIBER (model),
                              start,
                              end);

  g_task_return_pointer (task, g_steal_pointer (&model), g_object_unref);
}

static void
on_manager_calendar_added_cb (GcalManager      *manager,
                              GcalCalendar     *calendar,
                              GcalSearchEngine *self)
{
  ECalClient *client;

  g_debug ("Adding source %s to search results", gcal_calendar_get_id (calendar));

  client = gcal_calendar_get_client (calendar);

  if (gcal_calendar_get_visible (calendar))
    e_cal_data_model_add_client (self->data_model, client);
}

static void
on_manager_calendar_changed_cb (GcalManager      *manager,
                                GcalCalendar     *calendar,
                                GcalSearchEngine *self)
{
  g_debug ("Changing source %s from search results", gcal_calendar_get_id (calendar));

  if (gcal_calendar_get_visible (calendar))
    e_cal_data_model_add_client (self->data_model, gcal_calendar_get_client (calendar));
  else
    e_cal_data_model_remove_client (self->data_model, gcal_calendar_get_id (calendar));
}

static void
on_manager_calendar_removed_cb (GcalManager      *manager,
                                GcalCalendar     *calendar,
                                GcalSearchEngine *self)
{
  g_debug ("Removing source %s from search results", gcal_calendar_get_id (calendar));

  e_cal_data_model_remove_client (self->data_model, gcal_calendar_get_id (calendar));
}

static void
on_timezone_changed_cb (GcalContext      *context,
                        GParamSpec       *pspec,
                        GcalSearchEngine *self)
{
  g_debug ("Timezone changed");
}


/*
 * GObject overrides
 */

static void
gcal_search_engine_finalize (GObject *object)
{
  GcalSearchEngine *self = (GcalSearchEngine *)object;

  g_clear_object (&self->context);
  g_clear_object (&self->data_model);

  G_OBJECT_CLASS (gcal_search_engine_parent_class)->finalize (object);
}

static void
gcal_search_engine_constructed (GObject *object)
{
  GcalSearchEngine *self = (GcalSearchEngine *)object;
  GcalManager *manager;
  ICalTimezone *tz;

  G_OBJECT_CLASS (gcal_search_engine_parent_class)->constructed (object);

  /* Setup the data model */
  self->data_model = e_cal_data_model_new (gcal_thread_submit_job);
  e_cal_data_model_set_expand_recurrences (self->data_model, TRUE);
  tz = e_cal_util_get_system_timezone ();
  if (tz != NULL)
    e_cal_data_model_set_timezone (self->data_model, tz);


  manager = gcal_context_get_manager (self->context);
  g_signal_connect_object (manager, "calendar-added", G_CALLBACK (on_manager_calendar_added_cb), self, 0);
  g_signal_connect_object (manager, "calendar-changed", G_CALLBACK (on_manager_calendar_changed_cb), self, 0);
  g_signal_connect_object (manager, "calendar-removed", G_CALLBACK (on_manager_calendar_removed_cb), self, 0);

  g_signal_connect_object (self->context,
                           "notify::timezone",
                           G_CALLBACK (on_timezone_changed_cb),
                           self,
                           0);
}

static void
gcal_search_engine_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GcalSearchEngine *self = GCAL_SEARCH_ENGINE (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_value_set_object (value, self->context);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_search_engine_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GcalSearchEngine *self = GCAL_SEARCH_ENGINE (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_assert (self->context == NULL);
      self->context = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_search_engine_class_init (GcalSearchEngineClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gcal_search_engine_finalize;
  object_class->constructed = gcal_search_engine_constructed;
  object_class->get_property = gcal_search_engine_get_property;
  object_class->set_property = gcal_search_engine_set_property;

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

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gcal_search_engine_init (GcalSearchEngine *self)
{
}

GcalSearchEngine *
gcal_search_engine_new (GcalContext *context)
{
  return g_object_new (GCAL_TYPE_SEARCH_ENGINE,
                       "context", context,
                       NULL);
}

void
gcal_search_engine_search (GcalSearchEngine    *self,
                           const gchar         *search_query,
                           gint                 max_results,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  g_autoptr (GDateTime) now = NULL;
  g_autoptr (GTask) task = NULL;
  SearchData *data = NULL;
  GTimeZone *timezone;

  g_return_if_fail (GCAL_IS_SEARCH_ENGINE (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  timezone = gcal_context_get_timezone (self->context);
  now = g_date_time_new_now (timezone);

  data = g_slice_new0 (SearchData);
  data->engine = g_object_ref (self);
  data->query = g_strdup (search_query);
  data->max_results = max_results;
  data->range_start = g_date_time_add_weeks (now, -1);
  data->range_end = g_date_time_add_weeks (now, 3);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, gcal_search_engine_search);
  g_task_set_priority (task, G_PRIORITY_LOW);
  g_task_set_task_data (task, data, (GDestroyNotify) search_data_free);

  g_task_run_in_thread (task, search_func);
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
