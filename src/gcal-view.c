/* -*- mode: c; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gcal-view.c
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

#include "gcal-view.h"
#include "gcal-utils.h"

#include <glib.h>


G_DEFINE_INTERFACE (GcalView, gcal_view, GTK_TYPE_WIDGET)

static void
gcal_view_default_init (GcalViewInterface *iface)
{
  /**
   * GcalView::active-date:
   *
   * The active date of the view.
   */
  g_object_interface_install_property (iface,
                                       g_param_spec_boxed ("active-date",
                                                           "The active date",
                                                           "The active/selecetd date in the view",
                                                           ICAL_TIME_TYPE,
                                                           G_PARAM_READWRITE));

  /**
   * GcalView::create-event:
   *
   * Emitted when the view wants to create an event.
   */
  g_signal_new ("create-event",
                GCAL_TYPE_VIEW,
                G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET (GcalViewInterface, create_event),
                NULL, NULL, NULL,
                G_TYPE_NONE,
                4,
                G_TYPE_POINTER,
                G_TYPE_POINTER,
                G_TYPE_DOUBLE,
                G_TYPE_DOUBLE);

  /**
   * GcalView::create-event-detailed:
   *
   * Emitted when the view wants to create an event and immediately
   * edit it.
   */
  g_signal_new ("create-event-detailed",
                GCAL_TYPE_VIEW,
                G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET (GcalViewInterface, create_event_detailed),
                NULL, NULL, NULL,
                G_TYPE_NONE, 2, G_TYPE_POINTER, G_TYPE_POINTER);
}

/**
 * gcal_view_set_date:
 * @view: a #GcalView
 * @date: an #icaltimetype
 *
 * Sets the date of @view.
 */
void
gcal_view_set_date (GcalView     *view,
                    icaltimetype *date)
{
  g_return_if_fail (GCAL_IS_VIEW (view));
  g_return_if_fail (GCAL_VIEW_GET_IFACE (view)->set_date);

  GCAL_VIEW_GET_IFACE (view)->set_date (view, date);
}

/**
 * gcal_view_get_date:
 * @view: a #GcalView
 *
 * Retrieves the date of @view.
 *
 * Returns: (transfer none): an #icaltimetype.
 */
icaltimetype*
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
