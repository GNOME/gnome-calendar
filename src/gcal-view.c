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
        sizeof (GcalViewInterface),
        NULL,   /* base_init */
        NULL,   /* base_finalize */
        NULL,   /* class_init */
        NULL,   /* class_finalize */
        NULL,   /* class_data */
        0,
        0,      /* n_preallocs */
        NULL    /* instance_init */
      };
      const GInterfaceInfo view_info =
      {
        (GInterfaceInitFunc) gcal_view_base_init,
        NULL,               /* interface_finalize */
        NULL          /* interface_data */
      };
      type = g_type_register_static (G_TYPE_OBJECT,
                                     "GcalViewType",
                                     &info,
                                     0);
      g_type_add_interface_static (type,
                                   GCAL_TYPE_VIEW,
                                   &view_info);
    }
  return type;
}

gboolean
gcal_view_is_in_range (GcalView     *view,
                       icaltimetype *date)
{
  return GCAL_VIEW_GET_INTERFACE (view)->is_in_range (view, date);
}
