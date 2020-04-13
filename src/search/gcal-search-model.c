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

#include <dazzle.h>

#define MIN_RESULTS         5
#define WAIT_FOR_RESULTS_MS 0.150

struct _GcalSearchModel
{
  GObject             parent;

  GCancellable       *cancellable;
  gint                max_results;
  GDateTime          *range_start;
  GDateTime          *range_end;

  GListModel         *model;
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
 * GcalTimelineSubscriber interface
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

  self = GCAL_SEARCH_MODEL (subscriber);

  if (g_list_model_get_n_items (self->model) > self->max_results)
    return;

  GCAL_TRACE_MSG ("Adding search hit '%s'", gcal_event_get_summary (event));

  search_hit = gcal_search_hit_event_new (event);

  g_list_store_insert_sorted (G_LIST_STORE (self->model),
                              search_hit,
                              compare_search_hits_cb,
                              self);
}

static void
gcal_search_model_update_event (GcalTimelineSubscriber *subscriber,
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
  self->model = (GListModel*) g_list_store_new (DZL_TYPE_SUGGESTION);
  g_signal_connect_object (self->model, "items-changed", G_CALLBACK (on_model_items_changed_cb), self, 0);
}

GcalSearchModel *
gcal_search_model_new (GCancellable *cancellable,
                       gint          max_results,
                       GDateTime    *range_start,
                       GDateTime    *range_end)
{
  GcalSearchModel *model;

  model = g_object_new (GCAL_TYPE_SEARCH_MODEL, NULL);
  model->max_results = max_results;
  model->cancellable = cancellable ? g_object_ref (cancellable) : NULL;
  model->range_start = g_date_time_ref (range_start);
  model->range_end = g_date_time_ref (range_end);

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
