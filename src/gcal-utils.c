/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 2 -*- */
/*
 * gcal-utils.c
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

#include <string.h>

#include <libedataserverui/e-cell-renderer-color.h>

#include "gcal-utils.h"

/* taken from eel/eel-gtk-extensions.c */
static gboolean
tree_view_button_press_callback (GtkWidget *tree_view,
         GdkEventButton *event,
         gpointer data)
{
  GtkTreePath *path;
  GtkTreeViewColumn *column;

  if (event->button == 1 && event->type == GDK_BUTTON_PRESS) {
    if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (tree_view),
             event->x, event->y,
             &path,
             &column,
             NULL,
             NULL)) {
      gtk_tree_view_row_activated
        (GTK_TREE_VIEW (tree_view), path, column);
    }
  }

  return FALSE;
}

void
gcal_gtk_tree_view_set_activate_on_single_click (GtkTreeView *tree_view,
                                                 gboolean should_activate)
{
  guint button_press_id;

  button_press_id = GPOINTER_TO_UINT
    (g_object_get_data (G_OBJECT (tree_view),
          "gd-tree-view-activate"));

  if (button_press_id && !should_activate) {
    g_signal_handler_disconnect (tree_view, button_press_id);
    g_object_set_data (G_OBJECT (tree_view),
         "gd-tree-view-activate",
         NULL);
  } else if (!button_press_id && should_activate) {
    button_press_id = g_signal_connect
      (tree_view,
       "button_press_event",
       G_CALLBACK  (tree_view_button_press_callback),
       NULL);
    g_object_set_data (G_OBJECT (tree_view),
         "gd-tree-view-activate",
         GUINT_TO_POINTER (button_press_id));
  }
}

const gchar*
gcal_get_group_name (const gchar *base_uri)
{
  if (g_strcmp0 (base_uri, "local:") == 0)
    return "Local";
  else
    return "External";
}

void
gcal_print_date (gchar *str,
                 GDate *date)
{
  gchar s [200];
  g_date_strftime (s, sizeof (s), "%a, %d %b %Y", date);
  g_print ("%s %s\n", str, s);
}
