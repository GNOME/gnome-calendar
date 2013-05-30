/* -*- mode: c; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gcal-view.c
 *
 * Copyright (C) 2012 - Erick Pérez Castellanos
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
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

#include "gcal-view.h"
#include "gcal-utils.h"

#include <glib.h>

static void
gcal_view_base_init (gpointer g_iface)
{
  static gboolean initialized = FALSE;

  if (!initialized)
    {
      /* create interface signals && properties here. */
      g_object_interface_install_property (
          g_iface,
          g_param_spec_boxed ("active-date",
                              "The active date",
                              "The active/selecetd date in the view",
                              ICAL_TIME_TYPE,
                              G_PARAM_READWRITE));

      g_signal_new ("create-event",
                    GCAL_TYPE_VIEW,
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (GcalViewIface,
                                     create_event),
                    NULL, NULL, NULL,
                    G_TYPE_NONE,
                    4,
                    G_TYPE_POINTER,
                    G_TYPE_POINTER,
                    G_TYPE_DOUBLE,
                    G_TYPE_DOUBLE);

      g_signal_new ("updated",
                    GCAL_TYPE_VIEW,
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (GcalViewIface,
                                     updated),
                    NULL, NULL,
                    g_cclosure_marshal_VOID__POINTER,
                    G_TYPE_NONE,
                    1,
                    G_TYPE_POINTER);

      initialized = TRUE;
    }
}

GType
gcal_view_get_type (void)
{
  static GType type = 0;
  if (type == 0)
    {
      const GTypeInfo info =
      {
        sizeof (GcalViewIface),
        gcal_view_base_init,   /* base_init */
        NULL,   /* base_finalize */
        NULL,   /* class_init */
        NULL,   /* class_finalize */
        NULL,   /* class_data */
        0,
        0,      /* n_preallocs */
        NULL    /* instance_init */
      };
      type = g_type_register_static (G_TYPE_INTERFACE,
                                     "GcalView",
                                     &info,
                                     0);
      g_type_interface_add_prerequisite (type,
                                        G_TYPE_OBJECT);
    }
  return type;
}

void
gcal_view_set_date (GcalView     *view,
                    icaltimetype *date)
{
  g_return_if_fail (GCAL_IS_VIEW (view));

  g_object_set (view, "active-date", date, NULL);
}

icaltimetype*
gcal_view_get_date (GcalView *view)
{
  icaltimetype *date;

  g_return_val_if_fail (GCAL_IS_VIEW (view), NULL);

  g_object_get (view, "active-date", &date, NULL);
  return date;
}

icaltimetype*
gcal_view_get_initial_date (GcalView *view)
{
  g_return_val_if_fail (GCAL_IS_VIEW (view), NULL);

  return GCAL_VIEW_GET_INTERFACE (view)->get_initial_date (view);
}

icaltimetype*
gcal_view_get_final_date (GcalView *view)
{
  g_return_val_if_fail (GCAL_IS_VIEW (view), NULL);

  return GCAL_VIEW_GET_INTERFACE (view)->get_final_date (view);
}

gboolean
gcal_view_contains (GcalView     *view,
                       icaltimetype *date)
{
  g_return_val_if_fail (GCAL_IS_VIEW (view), FALSE);

  return GCAL_VIEW_GET_INTERFACE (view)->contains (view, date);
}

void
gcal_view_remove_by_uuid (GcalView    *view,
                          const gchar *uuid)
{
  g_return_if_fail (GCAL_IS_VIEW (view));

  GCAL_VIEW_GET_INTERFACE (view)->remove_by_uuid (view, uuid);
}

GtkWidget*
gcal_view_get_by_uuid (GcalView    *view,
                       const gchar *uuid)
{
  g_return_val_if_fail (GCAL_IS_VIEW (view), NULL);

  return GCAL_VIEW_GET_INTERFACE (view)->get_by_uuid (view, uuid);
}

void
gcal_view_reposition_child (GcalView    *view,
                            const gchar *uuid)
{
  g_return_if_fail (GCAL_IS_VIEW (view));

  GCAL_VIEW_GET_INTERFACE (view)->reposition_child (view, uuid);
}

void
gcal_view_clear_selection (GcalView *view)
{
  g_return_if_fail (GCAL_IS_VIEW (view));

  GCAL_VIEW_GET_INTERFACE (view)->clear_selection (view);
}

void
gcal_view_create_event_on_current_unit (GcalView *view)
{
  g_return_if_fail (GCAL_IS_VIEW (view));

  g_return_if_fail (GCAL_VIEW_GET_INTERFACE (view)->create_event_on_current_unit);

  GCAL_VIEW_GET_INTERFACE (view)->create_event_on_current_unit (view);
}

void
gcal_view_mark_current_unit (GcalView *view)
{
  g_return_if_fail (GCAL_IS_VIEW (view));
  g_return_if_fail (GCAL_VIEW_GET_INTERFACE (view)->mark_current_unit);

  GCAL_VIEW_GET_INTERFACE (view)->mark_current_unit (view);
}

void
gcal_view_clear_mark (GcalView *view)
{
  g_return_if_fail (GCAL_IS_VIEW (view));
  g_return_if_fail (GCAL_VIEW_GET_INTERFACE (view)->clear_mark);

  GCAL_VIEW_GET_INTERFACE (view)->clear_mark (view);
}

void
gcal_view_move_back (GcalView *view,
                     gint      steps)
{
  g_return_if_fail (GCAL_IS_VIEW (view));
  g_return_if_fail (GCAL_VIEW_GET_INTERFACE (view)->move_back);

  GCAL_VIEW_GET_INTERFACE (view)->move_back (view, steps);
}

void
gcal_view_move_forward (GcalView *view,
                        gint      steps)
{
  g_return_if_fail (GCAL_IS_VIEW (view));
  g_return_if_fail (GCAL_VIEW_GET_INTERFACE (view)->move_forward);

  GCAL_VIEW_GET_INTERFACE (view)->move_forward (view, steps);
}
