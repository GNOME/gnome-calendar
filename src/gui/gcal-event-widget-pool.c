/*
 * gcal-event-widget-pool.c
 *
 * Copyright 2026 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#define G_LOG_DOMAIN "GcalEventWidgetPool"

#include "gcal-core-macros.h"
#include "gcal-debug.h"
#include "gcal-event-widget.h"
#include "gcal-event-widget-pool.h"

#define N_PREALLOCATED_EVENT_WIDGETS 600

#define GDK_ARRAY_TYPE_NAME GcalEventWidgets
#define GDK_ARRAY_NAME gcal_event_widgets
#define GDK_ARRAY_ELEMENT_TYPE GcalEventWidget*
#define GDK_ARRAY_PREALLOC N_PREALLOCATED_EVENT_WIDGETS
#define GDK_ARRAY_FREE_FUNC g_object_unref
#include "gdkarrayimpl.c"

G_STATIC_ASSERT (N_PREALLOCATED_EVENT_WIDGETS > 0);

/**
 * GcalEventWidgetPool:
 *
 * A pool of event widgets.
 *
 * Instantiating widgets with templates is expensive, and in Calendar's
 * case, we instantiate hundreds of GcalEventWidgets often. This takes
 * a considerable time and affects the performance of the application.
 *
 * When GcalEventWidgetPool is created, it pre-allocates a number of
 * GcalEventWidgets. Consumers can then take event widgets from the
 * pool, and then return these event widgets back when they're unused.
 */

struct _GcalEventWidgetPool
{
  GObject parent_instance;

  GcalEventWidgets event_widgets;
};

G_DEFINE_FINAL_TYPE (GcalEventWidgetPool, gcal_event_widget_pool, G_TYPE_OBJECT)


/*
 * GObject overrides
 */

static void
gcal_event_widget_pool_finalize (GObject *object)
{
  GcalEventWidgetPool *self = GCAL_EVENT_WIDGET_POOL (object);

  gcal_event_widgets_clear (&self->event_widgets);

  G_OBJECT_CLASS (gcal_event_widget_pool_parent_class)->finalize (object);
}

static void
gcal_event_widget_pool_class_init (GcalEventWidgetPoolClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gcal_event_widget_pool_finalize;
}

static void
gcal_event_widget_pool_init (GcalEventWidgetPool *self)
{
  g_autoptr (GTimer) timer = g_timer_new ();

  g_timer_start (timer);
  for (size_t i = 0; i < N_PREALLOCATED_EVENT_WIDGETS; i++)
    {
      g_autoptr (GcalEventWidget) event_widget = NULL;

      event_widget = g_object_new (GCAL_TYPE_EVENT_WIDGET, NULL);
      g_object_ref_sink (event_widget);

      gcal_event_widgets_append (&self->event_widgets, g_steal_pointer (&event_widget));
    }
  g_timer_stop (timer);

  g_debug ("Initialized %u event widgets in %lf seconds",
           N_PREALLOCATED_EVENT_WIDGETS,
           g_timer_elapsed (timer, NULL));
}

/**
 * gcal_event_widget_pool_new:
 *
 * Creates a new event widget pool.
 */
GcalEventWidgetPool *
gcal_event_widget_pool_new (void)
{
  g_assert (GCAL_IS_MAIN_THREAD ());

  return g_object_new (GCAL_TYPE_EVENT_WIDGET_POOL, NULL);
}

/**
 * gcal_event_widget_pool_take_or_create:
 * @event: (transfer none): an event
 *
 * Takes an event widget from pool, with @event applied to
 * it. This is akin to creating a new event widget, and
 * setting it to @event.
 *
 * The pool will allocate a new event widget if it is empty.
 *
 * Returns: (transfer full): an event widget
 */
GtkWidget *
gcal_event_widget_pool_take_or_create (GcalEventWidgetPool *self,
                                       GcalEvent           *event)
{
  GcalEventWidget *event_widget;
  size_t size = 0;

  g_assert (GCAL_IS_EVENT_WIDGET_POOL (self));
  g_assert (GCAL_IS_EVENT (event));
  g_assert (GCAL_IS_MAIN_THREAD ());

  size = gcal_event_widgets_get_size (&self->event_widgets);

  if (size == 0)
    {
      GtkWidget *new_event_widget;

      g_debug ("Pool %p is empty, creating new event widget for event %s", self, gcal_event_get_uid (event));

      new_event_widget = gcal_event_widget_new (event);
      g_object_ref_sink (new_event_widget);

      return  (GtkWidget *) g_steal_pointer (&new_event_widget);
    }

  event_widget = gcal_event_widgets_get (&self->event_widgets, size - 1);

  g_debug ("Taking cached event widget %p from pool for event %s", event_widget, gcal_event_get_uid (event));

  gcal_event_widgets_splice (&self->event_widgets, size - 1, 1, TRUE, NULL, 0);
  g_assert (GCAL_IS_EVENT_WIDGET (event_widget));

  gcal_event_widget_set_event (event_widget, event);

  return (GtkWidget *) g_steal_pointer (&event_widget);
}

/**
 * gcal_event_widget_pool_reclaim:
 * @event_widget: (transfer full): an event
 *
 * Reclaims @event_widget into the pool. The event widget is
 * reset, and stored for later usage. The pool takes full
 * ownserhip of the event widget, so the caller must *not*
 * destroy it.
 *
 * The @event_widget must have been removed from its parent.
 */
void
gcal_event_widget_pool_reclaim (GcalEventWidgetPool *self,
                                GtkWidget           *event_widget)
{
  g_assert (GCAL_IS_EVENT_WIDGET_POOL (self));
  g_assert (GCAL_IS_EVENT_WIDGET (event_widget));
  g_assert (gtk_widget_get_parent (event_widget) == NULL);
  g_assert (GCAL_IS_MAIN_THREAD ());

  g_debug ("Reclaiming event widget %p", event_widget);

  gcal_event_widget_set_event (GCAL_EVENT_WIDGET (event_widget), NULL);
  gcal_event_widget_set_timestamp_policy (GCAL_EVENT_WIDGET (event_widget), GCAL_TIMESTAMP_POLICY_NONE);

  gcal_event_widgets_append (&self->event_widgets, GCAL_EVENT_WIDGET (g_steal_pointer (&event_widget)));
}

