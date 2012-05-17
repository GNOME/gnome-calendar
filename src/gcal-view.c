/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 2 -*- */
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

#include <glib.h>

static void
gcal_view_base_init (gpointer g_iface)
{
  static gboolean initialized = FALSE;

  if (!initialized)
    {
      /* create interface signals here. */
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

gboolean
gcal_view_is_in_range (GcalView     *view,
                       icaltimetype *date)
{
  g_return_val_if_fail (GCAL_IS_VIEW (view), FALSE);

  return GCAL_VIEW_GET_INTERFACE (view)->is_in_range (view, date);
}

void
gcal_view_add_event (GcalView  *view,
                     GtkWidget *event)
{
  g_return_if_fail (GCAL_IS_VIEW (view));

  GCAL_VIEW_GET_INTERFACE (view)->add_event (view, event);
}
