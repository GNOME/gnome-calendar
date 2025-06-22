/* gcal-view.c
 *
 * Copyright (C) 2015 - Erick Pérez Castellanos
 *               2016 - Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#define G_LOG_DOMAIN "GcalView"

#include "gcal-debug.h"
#include "gcal-event-widget.h"
#include "gcal-view-private.h"
#include "gcal-utils.h"

#include <glib.h>


G_DEFINE_INTERFACE (GcalView, gcal_view, GTK_TYPE_WIDGET)

enum
{
  CREATE_EVENT,
  CREATE_EVENT_DETAILED,
  EVENT_ACTIVATED,
  N_SIGNALS,
};

static guint signals[N_SIGNALS] = { 0, };

static void
gcal_view_default_init (GcalViewInterface *iface)
{
  /**
   * GcalView:active-date:
   *
   * The active date of the view.
   */
  g_object_interface_install_property (iface,
                                       g_param_spec_boxed ("active-date",
                                                           "The active date",
                                                           "The active/selected date in the view",
                                                           G_TYPE_DATE_TIME,
                                                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GcalView:context:
   *
   * The context of the view.
   */
  g_object_interface_install_property (iface,
                                       g_param_spec_object ("context",
                                                            "The context",
                                                            "The context of the view",
                                                            GCAL_TYPE_CONTEXT,
                                                            G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  /**
   * GcalView:time-direction:
   *
   * Orientation determining in which direction time is going for the view
   */
  g_object_interface_install_property (iface,
                                       g_param_spec_enum ("time-direction",
                                                          NULL,
                                                          NULL,
                                                          GTK_TYPE_ORIENTATION,
                                                          GTK_ORIENTATION_VERTICAL,
                                                          G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  /**
   * GcalView::create-event:
   *
   * Emitted when the view wants to create an event.
   */
  signals[CREATE_EVENT] = g_signal_new ("create-event",
                                        GCAL_TYPE_VIEW,
                                        G_SIGNAL_RUN_LAST,
                                        G_STRUCT_OFFSET (GcalViewInterface, create_event),
                                        NULL, NULL, NULL,
                                        G_TYPE_NONE,
                                        3,
                                        GCAL_TYPE_RANGE,
                                        G_TYPE_DOUBLE,
                                        G_TYPE_DOUBLE);

  /**
   * GcalView::create-event-detailed:
   *
   * Emitted when the view wants to create an event and immediately
   * edit it.
   */
  signals[CREATE_EVENT_DETAILED] = g_signal_new ("create-event-detailed",
                                                 GCAL_TYPE_VIEW,
                                                 G_SIGNAL_RUN_LAST,
                                                 G_STRUCT_OFFSET (GcalViewInterface, create_event_detailed),
                                                 NULL, NULL, NULL,
                                                 G_TYPE_NONE, 1,
                                                 GCAL_TYPE_RANGE);

  /**
   * GcalView::event-activated:
   *
   * Emitted when an event widget inside the view is activated.
   */
  signals[EVENT_ACTIVATED] = g_signal_new ("event-activated",
                                           GCAL_TYPE_VIEW,
                                           G_SIGNAL_RUN_LAST,
                                           0, NULL, NULL, NULL,
                                           G_TYPE_NONE,
                                           1,
                                           GCAL_TYPE_EVENT_WIDGET);
}

/**
 * gcal_view_set_date:
 * @view: a #GcalView
 * @date: an #GDateTime
 *
 * Sets the date of @view.
 */
void
gcal_view_set_date (GcalView  *view,
                    GDateTime *date)
{
  g_return_if_fail (GCAL_IS_VIEW (view));
  g_return_if_fail (GCAL_VIEW_GET_IFACE (view)->set_date);

  GCAL_VIEW_GET_IFACE (view)->set_date (view, date);
}

/**
 * gcal_view_get_context:
 * @self: a #GcalView
 *
 * Retrieves the #GcalContext instance from @self.
 *
 * Returns: (transfer none): a #GcalContext
 */
GcalContext*
gcal_view_get_context (GcalView *self)
{
  GcalContext *context;

  g_return_val_if_fail (GCAL_IS_VIEW (self), NULL);

  g_object_get (self, "context", &context, NULL);
  g_object_unref (context);

  return context;
}

/**
 * gcal_view_get_time_direction:
 * @self: a #GcalView
 *
 * Retrieves the orientation of the direction in which the view visualises time.
 *
 * Returns: (transfer none): a #GtkOrientation
 */
GtkOrientation
gcal_view_get_time_direction (GcalView *self)
{
  GtkOrientation time_direction;

  g_return_val_if_fail (GCAL_IS_VIEW (self), GTK_ORIENTATION_VERTICAL);

  g_object_get (self, "time-direction", &time_direction, NULL);

  return time_direction;
}

/**
 * gcal_view_get_date:
 * @view: a #GcalView
 *
 * Retrieves the date of @view.
 *
 * Returns: (transfer none): an #GDateTime.
 */
GDateTime*
gcal_view_get_date (GcalView *view)
{
  g_return_val_if_fail (GCAL_IS_VIEW (view), NULL);
  g_return_val_if_fail (GCAL_VIEW_GET_IFACE (view)->get_date, NULL);

  return GCAL_VIEW_GET_IFACE (view)->get_date (view);
}

/**
 * gcal_view_clear_marks:
 * @view: a #GcalView
 *
 * Clear any marking the view had drawn
 **/
void
gcal_view_clear_marks (GcalView *view)
{
  g_return_if_fail (GCAL_IS_VIEW (view));
  g_return_if_fail (GCAL_VIEW_GET_IFACE (view)->clear_marks);

  GCAL_VIEW_GET_IFACE (view)->clear_marks (view);
}

/**
 * gcal_view_get_children_by_uuid:
 * @view: a #GcalView
 * @uuid: The unique id of an event
 *
 * Returns a list with every event that has the passed uuid
 *
 * Returns: (transfer full): a {@link GList} instance
 **/
GList*
gcal_view_get_children_by_uuid (GcalView              *view,
                                GcalRecurrenceModType  mod,
                                const gchar           *uuid)
{
  g_return_val_if_fail (GCAL_IS_VIEW (view), NULL);
  g_return_val_if_fail (GCAL_VIEW_GET_IFACE (view)->get_children_by_uuid, NULL);

  return GCAL_VIEW_GET_IFACE (view)->get_children_by_uuid (view, mod, uuid);
}

/**
 * gcal_view_get_next_date:
 * @self: a #GcalView
 *
 * Retrieves the next date from @self. Different views have
 * different time ranges (e.g. day view ranges 1 day, week
 * view ranges 1 week, etc) and pressing the next button
 * of the main window may advance the time in different steps.
 *
 * Returns: (transfer full): a #GDateTime
 */
GDateTime*
gcal_view_get_next_date (GcalView *self)
{
  g_autoptr (GDateTime) next_date = NULL;

  g_return_val_if_fail (GCAL_IS_VIEW (self), NULL);
  g_return_val_if_fail (GCAL_VIEW_GET_IFACE (self)->get_next_date, NULL);

  next_date = GCAL_VIEW_GET_IFACE (self)->get_next_date (self);

#ifdef GCAL_ENABLE_TRACE
    {
      g_autofree gchar *str = NULL;

      str = g_date_time_format (next_date, "%x %X %z");
      g_debug ("%s's next date: %s", G_OBJECT_TYPE_NAME (self), str);
    }
#endif

  return g_steal_pointer (&next_date);
}

/**
 * gcal_view_get_previous_date:
 * @self: a #GcalView
 *
 * Retrieves the previous date from @self.
 *
 * Returns: (transfer full): a #GDateTime
 */
GDateTime*
gcal_view_get_previous_date (GcalView *self)
{
  g_autoptr (GDateTime) previous_date = NULL;

  g_return_val_if_fail (GCAL_IS_VIEW (self), NULL);
  g_return_val_if_fail (GCAL_VIEW_GET_IFACE (self)->get_previous_date, NULL);

  previous_date = GCAL_VIEW_GET_IFACE (self)->get_previous_date (self);

#ifdef GCAL_ENABLE_TRACE
    {
      g_autofree gchar *str = NULL;

      str = g_date_time_format (previous_date, "%x %X %z");
      g_debug ("%s's previous date: %s", G_OBJECT_TYPE_NAME (self), str);
    }
#endif

  return g_steal_pointer (&previous_date);
}

void
gcal_view_create_event (GcalView  *self,
                        GcalRange *range,
                        gdouble    x,
                        gdouble    y)
{
  g_assert (GCAL_IS_VIEW (self));
  g_assert (range != NULL);

  g_signal_emit (self, signals[CREATE_EVENT], 0, range, x, y);
}

void
gcal_view_create_event_detailed (GcalView  *self,
                                 GcalRange *range)
{
  g_assert (GCAL_IS_VIEW (self));
  g_assert (range != NULL);

  g_signal_emit (self, signals[CREATE_EVENT_DETAILED], 0, range);
}

void
gcal_view_event_activated (GcalView        *self,
                           GcalEventWidget *event_widget)
{
  g_assert (GCAL_IS_VIEW (self));
  g_assert (GCAL_IS_EVENT_WIDGET (event_widget));

  g_signal_emit (self, signals[EVENT_ACTIVATED], 0, event_widget);
}
