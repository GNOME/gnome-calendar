/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 2 -*- */
/*
 * gcal-main-toolbar.c
 * Copyright (C) 2012 Erick Pérez Castellanos <erickpc@gnome.org>
 *
 * gnome-calendar is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * gnome-calendar is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "gcal-main-toolbar.h"

#include <glib/gi18n.h>

struct _GcalMainToolbarPrivate
{
  GtkWidget *widget;
};

G_DEFINE_TYPE (GcalMainToolbar, gcal_main_toolbar, GTK_CLUTTER_TYPE_ACTOR);

static void
gcal_main_toolbar_init (GcalMainToolbar *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            GCAL_TYPE_MAIN_TOOLBAR,
                                            GcalMainToolbarPrivate);
}

static void
gcal_main_toolbar_constructed (GObject *object)
{
  GcalMainToolbarPrivate *priv;

  GtkWidget *button;
  GtkStyleContext *context;
  GtkToolItem *item;
  GtkToolItem *spacer;
  GtkWidget *views_box;
  GtkWidget *bin;

  priv = GCAL_MAIN_TOOLBAR (object)->priv;
  if (G_OBJECT_CLASS (gcal_main_toolbar_parent_class)->constructed != NULL)
    G_OBJECT_CLASS (gcal_main_toolbar_parent_class)->constructed (object);

  priv->widget = gtk_toolbar_new ();
  gtk_widget_set_hexpand (priv->widget, TRUE);
  gtk_widget_set_vexpand (priv->widget, TRUE);
  gtk_toolbar_set_icon_size (GTK_TOOLBAR (priv->widget),
                             GTK_ICON_SIZE_BUTTON);

  context = gtk_widget_get_style_context (priv->widget);
  gtk_style_context_add_class (context, "gcal-main-toolbar");

  /* sources */
  button = gtk_toggle_button_new ();
  gtk_container_add (GTK_CONTAINER (button),
      gtk_image_new_from_icon_name ("emblem-documents-symbolic",
                                    GTK_ICON_SIZE_MENU));

  context = gtk_widget_get_style_context (button);
  gtk_style_context_add_class (context, "raised");

  item = gtk_tool_item_new ();
  gtk_container_add (GTK_CONTAINER (item), button);
  gtk_toolbar_insert (GTK_TOOLBAR (priv->widget), item, 0);

  /* spacer */
  spacer = gtk_tool_item_new ();
  gtk_tool_item_set_expand (spacer, TRUE);
  gtk_toolbar_insert (GTK_TOOLBAR (priv->widget), spacer, -1);

  /* views_box */
  views_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_hexpand (views_box, TRUE);

  context = gtk_widget_get_style_context (views_box);
  gtk_style_context_add_class (context, "linked");

  item = gtk_tool_item_new ();
  gtk_container_add (GTK_CONTAINER (item), views_box);
  gtk_toolbar_insert (GTK_TOOLBAR (priv->widget), item, -1);

  /* day */
  button = gtk_button_new_with_label (_("Day"));
  gtk_widget_set_size_request (button, 80, -1);

  context = gtk_widget_get_style_context (button);
  gtk_style_context_add_class (context, "raised");

  gtk_container_add (GTK_CONTAINER (views_box), button);

  /* week */
  button = gtk_button_new_with_label (_("Week"));
  gtk_widget_set_size_request (button, 80, -1);

  context = gtk_widget_get_style_context (button);
  gtk_style_context_add_class (context, "raised");

  gtk_container_add (GTK_CONTAINER (views_box), button);

  /* month */
  button = gtk_button_new_with_label (_("Month"));
  gtk_widget_set_size_request (button, 80, -1);

  context = gtk_widget_get_style_context (button);
  gtk_style_context_add_class (context, "raised");

  gtk_container_add (GTK_CONTAINER (views_box), button);

  /* year */
  button = gtk_button_new_with_label (_("Year"));
  gtk_widget_set_size_request (button, 80, -1);

  context = gtk_widget_get_style_context (button);
  gtk_style_context_add_class (context, "raised");

  gtk_container_add (GTK_CONTAINER (views_box), button);

  /* list */
  button = gtk_button_new_with_label (_("List"));
  gtk_widget_set_size_request (button, 80, -1);

  context = gtk_widget_get_style_context (button);
  gtk_style_context_add_class (context, "raised");

  gtk_container_add (GTK_CONTAINER (views_box), button);

  /* spacer */
  spacer = gtk_tool_item_new ();
  gtk_tool_item_set_expand (spacer, TRUE);
  gtk_toolbar_insert (GTK_TOOLBAR (priv->widget), spacer, -1);

  /* add */
  button = gtk_toggle_button_new ();
  gtk_container_add (GTK_CONTAINER (button),
      gtk_image_new_from_icon_name ("list-add-symbolic",
                                    GTK_ICON_SIZE_MENU));

  context = gtk_widget_get_style_context (button);
  gtk_style_context_add_class (context, "raised");

  item = gtk_tool_item_new ();
  gtk_container_add (GTK_CONTAINER (item), button);
  gtk_toolbar_insert (GTK_TOOLBAR (priv->widget), item, -1);

  /* adding toolbar */
  bin = gtk_clutter_actor_get_widget (GTK_CLUTTER_ACTOR (object));
  gtk_container_add (GTK_CONTAINER (bin), priv->widget);

  gtk_widget_show_all (bin);

}

static void
gcal_main_toolbar_finalize (GObject *object)
{
  G_OBJECT_CLASS (gcal_main_toolbar_parent_class)->finalize (object);
}

static void
gcal_main_toolbar_class_init (GcalMainToolbarClass *klass)
{
  G_OBJECT_CLASS (klass)->constructed = gcal_main_toolbar_constructed;
  G_OBJECT_CLASS (klass)->finalize = gcal_main_toolbar_finalize;

  g_type_class_add_private ((gpointer) klass, sizeof(GcalMainToolbarPrivate));
}

ClutterActor*
gcal_main_toolbar_new (void)
{
  return g_object_new (gcal_main_toolbar_get_type (), NULL);
}
