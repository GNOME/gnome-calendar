/* gcal-search-model.c
 *
 * Copyright 2019-2020 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#include "gcal-application.h"
#include "gcal-context.h"
#include "gcal-debug.h"
#include "gcal-timeline-subscriber.h"
#include "gcal-search-hit.h"
#include "gcal-search-hit-event.h"
#include "gcal-search-model.h"
#include "gcal-utils.h"

#define MIN_RESULTS         5
#define WAIT_FOR_RESULTS_MS 0.150

struct _GcalSearchModel
{
  GObject             parent;

  GCancellable       *cancellable;
  GDateTime          *range_start;
  GDateTime          *range_end;

  GListModel         *model;

  GTimer             *timer;
  guint               idle_id;
};

static void          gcal_timeline_subscriber_interface_init     (GcalTimelineSubscriberInterface *iface);

static void g_list_model_interface_init                (GListModelInterface              *iface);

G_DEFINE_TYPE_WITH_CODE (GcalSearchModel, gcal_search_model, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GCAL_TYPE_TIMELINE_SUBSCRIBER,
                                                gcal_timeline_subscriber_interface_init)
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL,
                                                g_list_model_interface_init))

/*
 * Callbacks
 */

static gboolean
check_for_search_hits_cb (gpointer user_data)
{
  GcalSearchModel *self;
  GTask *task;

  task = G_TASK (user_data);
  self = GCAL_SEARCH_MODEL (g_task_get_source_object (task));

  if (g_task_return_error_if_cancelled (task))
    goto stop_idle;

  if (g_timer_elapsed (self->timer, NULL) >= WAIT_FOR_RESULTS_MS ||
      g_list_model_get_n_items (self->model) >= MIN_RESULTS)
    {
      g_task_return_boolean (task, TRUE);
      goto stop_idle;
    }

  return G_SOURCE_CONTINUE;

stop_idle:
  g_clear_pointer (&self->timer, g_timer_destroy);
  self->idle_id = 0;
  return G_SOURCE_REMOVE;
}

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

static gboolean
search_hits_equals_cb (gconstpointer a,
                       gconstpointer b)
{
  GcalSearchHit *search_hit_a = GCAL_SEARCH_HIT ((gpointer) a);
  GcalSearchHit *search_hit_b = GCAL_SEARCH_HIT ((gpointer) b);

  return gcal_search_hit_compare (search_hit_a, search_hit_b) == 0;
}

static void
on_search_model_items_changed_cb (GListModel      *model,
                                  guint            position,
                                  guint            removed,
                                  guint            added,
                                  GcalSearchModel *self)
{
  g_list_model_items_changed (G_LIST_MODEL (self), position, removed, added);
}


/*
 * GcalTimelineSubscriber interface
 */

static GcalRange*
gcal_search_model_get_range (GcalTimelineSubscriber *subscriber)
{
  GcalSearchModel *self = GCAL_SEARCH_MODEL (subscriber);

  return gcal_range_new (self->range_start, self->range_end, GCAL_RANGE_DEFAULT);
}

static void
gcal_search_model_add_event (GcalTimelineSubscriber *subscriber,
                             GcalEvent              *event)
{
  g_autoptr (GcalSearchHitEvent) search_hit = NULL;
  GcalSearchModel *self;
  gboolean found;

  self = GCAL_SEARCH_MODEL (subscriber);

  GCAL_TRACE_MSG ("Adding search hit '%s'", gcal_event_get_summary (event));

  search_hit = gcal_search_hit_event_new (event);

  found = g_list_store_find_with_equal_func (G_LIST_STORE (self->model),
                                             search_hit,
                                             search_hits_equals_cb,
                                             NULL);

  if (found)
    return;

  g_list_store_insert_sorted (G_LIST_STORE (self->model),
                              search_hit,
                              compare_search_hits_cb,
                              self);
}

static void
gcal_search_model_update_event (GcalTimelineSubscriber *subscriber,
                                GcalEvent              *old_event,
                                GcalEvent              *event)
{
}

static void
gcal_search_model_remove_event (GcalTimelineSubscriber *subscriber,
                                GcalEvent              *event)
{
}

static void
gcal_timeline_subscriber_interface_init (GcalTimelineSubscriberInterface *iface)
{
  iface->get_range = gcal_search_model_get_range;
  iface->add_event = gcal_search_model_add_event;
  iface->update_event = gcal_search_model_update_event;
  iface->remove_event = gcal_search_model_remove_event;
}


/*
 * GListModel interface
 */

static GType
gcal_search_model_get_item_type (GListModel *model)
{
  return GCAL_TYPE_SEARCH_HIT;
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

  g_assert (self->timer == NULL);
  g_assert (self->idle_id == 0);

  g_cancellable_cancel (self->cancellable);

  gcal_clear_date_time (&self->range_start);
  gcal_clear_date_time (&self->range_end);
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
  self->model = (GListModel*) g_list_store_new (GCAL_TYPE_SEARCH_HIT);
  g_signal_connect (self->model, "items-changed", G_CALLBACK (on_search_model_items_changed_cb), self);
}

GcalSearchModel *
gcal_search_model_new (GCancellable *cancellable,
                       GDateTime    *range_start,
                       GDateTime    *range_end)
{
  GcalSearchModel *model;

  model = g_object_new (GCAL_TYPE_SEARCH_MODEL, NULL);
  model->cancellable = cancellable ? g_object_ref (cancellable) : NULL;
  model->range_start = g_date_time_ref (range_start);
  model->range_end = g_date_time_ref (range_end);

  return model;
}

void
gcal_search_model_wait_for_hits (GcalSearchModel     *self,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;

  GCAL_ENTRY;

  g_assert (self->timer == NULL);
  g_assert (self->idle_id == 0);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, gcal_search_model_wait_for_hits);
  g_task_set_priority (task, G_PRIORITY_LOW);

  self->timer = g_timer_new ();
  self->idle_id = g_idle_add_full (G_PRIORITY_LOW,
                                   check_for_search_hits_cb,
                                   g_object_ref (task),
                                   g_object_unref);

  GCAL_EXIT;
}

gboolean
gcal_search_model_wait_for_hits_finish (GcalSearchModel  *self,
                                        GAsyncResult     *result,
                                        GError          **error)
{
  g_return_val_if_fail (GCAL_IS_SEARCH_MODEL (self), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, self), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}
