/* gcal-search-model.c
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

#define G_LOG_DOMAIN "GcalSearchModel"

#include "e-cal-data-model.h"
#include "gcal-application.h"
#include "gcal-context.h"
#include "gcal-debug.h"
#include "gcal-search-hit.h"
#include "gcal-search-hit-event.h"
#include "gcal-search-model.h"
#include "gcal-utils.h"

#include <dazzle.h>

#define MIN_RESULTS         5
#define WAIT_FOR_RESULTS_MS 0.150

struct _GcalSearchModel
{
  GObject             parent;

  GCancellable       *cancellable;
  gint                max_results;

  GListModel         *model;
};

static void e_cal_data_model_subscriber_interface_init (ECalDataModelSubscriberInterface *iface);

static void g_list_model_interface_init                (GListModelInterface              *iface);

G_DEFINE_TYPE_WITH_CODE (GcalSearchModel, gcal_search_model, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (E_TYPE_CAL_DATA_MODEL_SUBSCRIBER,
                                                e_cal_data_model_subscriber_interface_init)
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL,
                                                g_list_model_interface_init))

/*
 * Callbacks
 */

static void
on_model_items_changed_cb (GListModel      *model,
                           guint            position,
                           guint            removed,
                           guint            added,
                           GcalSearchModel *self)
{
  g_list_model_items_changed (G_LIST_MODEL (self), position, removed, added);
}


/*
 * ECalDataModelSubscriber interface
 */

static gint
compare_search_hits_cb (gconstpointer a,
                        gconstpointer b,
                        gpointer      user_data)
{
  GcalSearchHit *hit_a, *hit_b;
  gint result;

  hit_a = (GcalSearchHit *) a;
  hit_b = (GcalSearchHit *) b;

  /* Priority */
  result = gcal_search_hit_get_priority (hit_a) - gcal_search_hit_get_priority (hit_b);

  if (result != 0)
    return result;

  /* Compare func */
  return gcal_search_hit_compare (hit_a, hit_b);
}

static void
gcal_search_model_component_added (ECalDataModelSubscriber *subscriber,
                                   ECalClient              *client,
                                   ECalComponent           *component)
{
  g_autoptr (GcalSearchHitEvent) search_hit = NULL;
  g_autoptr (GcalEvent) event = NULL;
  g_autoptr (GError) error = NULL;
  GcalSearchModel *self;
  GcalCalendar *calendar;
  GcalContext *context;
  ESource *source;

  self = GCAL_SEARCH_MODEL (subscriber);

  if (g_list_model_get_n_items (self->model) > self->max_results)
    return;

  /* FIXME: propagate context to the model properly */
  context = gcal_application_get_context (GCAL_APPLICATION (g_application_get_default ()));
  source = e_client_get_source (E_CLIENT (client));
  calendar = gcal_manager_get_calendar_from_source (gcal_context_get_manager (context), source);
  event = gcal_event_new (calendar, component, &error);

  if (error)
    {
      g_warning ("Error adding event to search results: %s", error->message);
      return;
    }

  GCAL_TRACE_MSG ("Adding search hit '%s'", gcal_event_get_summary (event));

  search_hit = gcal_search_hit_event_new (event);

  g_list_store_insert_sorted (G_LIST_STORE (self->model),
                              search_hit,
                              compare_search_hits_cb,
                              self);
}

static void
gcal_search_model_component_modified (ECalDataModelSubscriber *subscriber,
                                      ECalClient              *client,
                                      ECalComponent           *comp)
{
}

static void
gcal_search_model_component_removed (ECalDataModelSubscriber *subscriber,
                                     ECalClient              *client,
                                     const gchar             *uid,
                                     const gchar             *rid)
{
}

static void
gcal_search_model_freeze (ECalDataModelSubscriber *subscriber)
{
}

static void
gcal_search_model_thaw (ECalDataModelSubscriber *subscriber)
{
}

static void
e_cal_data_model_subscriber_interface_init (ECalDataModelSubscriberInterface *iface)
{
  iface->component_added = gcal_search_model_component_added;
  iface->component_modified = gcal_search_model_component_modified;
  iface->component_removed = gcal_search_model_component_removed;
  iface->freeze = gcal_search_model_freeze;
  iface->thaw = gcal_search_model_thaw;
}


/*
 * GListModel interface
 */

static GType
gcal_search_model_get_item_type (GListModel *model)
{
  return DZL_TYPE_SUGGESTION;
}

static guint
gcal_search_model_get_n_items (GListModel *model)
{
  GcalSearchModel *self = (GcalSearchModel *)model;
  return g_list_model_get_n_items (self->model);
}

static gpointer
gcal_search_model_get_item (GListModel *model,
                            guint       position)
{
  GcalSearchModel *self = (GcalSearchModel *)model;
  return g_list_model_get_item (self->model, position);
}

static void
g_list_model_interface_init (GListModelInterface *iface)
{
  iface->get_item_type = gcal_search_model_get_item_type;
  iface->get_n_items = gcal_search_model_get_n_items;
  iface->get_item = gcal_search_model_get_item;
}


/*
 * GObject overrides
 */

static void
gcal_search_model_finalize (GObject *object)
{
  GcalSearchModel *self = (GcalSearchModel *)object;

  g_cancellable_cancel (self->cancellable);

  g_clear_object (&self->cancellable);
  g_clear_object (&self->model);

  G_OBJECT_CLASS (gcal_search_model_parent_class)->finalize (object);
}

static void
gcal_search_model_class_init (GcalSearchModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gcal_search_model_finalize;
}

static void
gcal_search_model_init (GcalSearchModel *self)
{
  self->model = (GListModel*) g_list_store_new (DZL_TYPE_SUGGESTION);
  g_signal_connect_object (self->model, "items-changed", G_CALLBACK (on_model_items_changed_cb), self, 0);
}

GcalSearchModel *
gcal_search_model_new (GCancellable *cancellable,
                       gint          max_results)
{
  GcalSearchModel *model;

  model = g_object_new (GCAL_TYPE_SEARCH_MODEL, NULL);
  model->max_results = max_results;
  model->cancellable = cancellable ? g_object_ref (cancellable) : NULL;

  return model;
}

void
gcal_search_model_wait_for_hits (GcalSearchModel *self,
                                 GCancellable    *cancellable)
{
  g_autoptr (GMainContext) thread_context = NULL;
  g_autoptr (GTimer) timer = NULL;

  GCAL_ENTRY;

  g_return_if_fail (GCAL_IS_SEARCH_MODEL (self));

  thread_context = g_main_context_ref_thread_default ();
  timer = g_timer_new ();

  g_timer_start (timer);

  while (g_list_model_get_n_items (self->model) < MIN_RESULTS &&
         g_timer_elapsed (timer, NULL) < WAIT_FOR_RESULTS_MS)
    {
      if (g_cancellable_is_cancelled (cancellable))
        break;

      g_main_context_iteration (thread_context, FALSE);
    }

  GCAL_EXIT;
}
