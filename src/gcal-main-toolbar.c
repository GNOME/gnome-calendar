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
#include "gcal-utils.h"

#include <glib/gi18n.h>

struct _GcalMainToolbarPrivate
{
  GtkWidget *widget;
};

enum
{
  /* From overview mode */
  VIEW_CHANGED = 1,
  SOURCES_SHOWN,
  ADD_EVENT,

  NUM_SIGNALS
};

static guint signals[NUM_SIGNALS] = { 0, };

static void gcal_main_toolbar_constructed    (GObject   *object);

static void gcal_main_toolbar_finalize       (GObject   *object);

static void _gcal_main_toolbar_view_changed  (GtkWidget *button,
                                              gpointer   user_data);

static void _gcal_main_toolbar_sources_shown (GtkWidget *button,
                                              gpointer   user_data);

static void _gcal_main_toolbar_add_event     (GtkWidget *button,
                                              gpointer   user_data);

G_DEFINE_TYPE (GcalMainToolbar, gcal_main_toolbar, GTK_CLUTTER_TYPE_ACTOR);

static void
gcal_main_toolbar_class_init (GcalMainToolbarClass *klass)
{
  G_OBJECT_CLASS (klass)->constructed = gcal_main_toolbar_constructed;
  G_OBJECT_CLASS (klass)->finalize = gcal_main_toolbar_finalize;

  signals[VIEW_CHANGED] = g_signal_new ("view-changed",
                                        GCAL_TYPE_MAIN_TOOLBAR,
                                        G_SIGNAL_RUN_LAST,
                                        G_STRUCT_OFFSET (GcalMainToolbarClass,
                                                         view_changed),
                                        NULL, NULL,
                                        g_cclosure_marshal_VOID__UINT,
                                        G_TYPE_NONE,
                                        1,
                                        G_TYPE_UINT);

  signals[SOURCES_SHOWN] = g_signal_new ("sources-shown",
                                         GCAL_TYPE_MAIN_TOOLBAR,
                                         G_SIGNAL_RUN_LAST,
                                         G_STRUCT_OFFSET (GcalMainToolbarClass,
                                                          sources_shown),
                                         NULL, NULL,
                                         g_cclosure_marshal_VOID__BOOLEAN,
                                         G_TYPE_NONE,
                                         1,
                                         G_TYPE_BOOLEAN);

  signals[ADD_EVENT] = g_signal_new ("add-event",
                                     GCAL_TYPE_MAIN_TOOLBAR,
                                     G_SIGNAL_RUN_LAST,
                                     G_STRUCT_OFFSET (GcalMainToolbarClass,
                                                      add_event),
                                     NULL, NULL,
                                     g_cclosure_marshal_VOID__VOID,
                                     G_TYPE_NONE,
                                     0);

  g_type_class_add_private ((gpointer) klass, sizeof(GcalMainToolbarPrivate));
}

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
  gtk_container_add (
      GTK_CONTAINER (button),
      gtk_image_new_from_icon_name ("emblem-documents-symbolic",
                                    GTK_ICON_SIZE_MENU));

  context = gtk_widget_get_style_context (button);
  gtk_style_context_add_class (context, "raised");

  g_signal_connect (button,
                    "clicked",
                    G_CALLBACK (_gcal_main_toolbar_sources_shown),
                    GCAL_MAIN_TOOLBAR (object));

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

  g_object_set_data (G_OBJECT (button),
                    "view-type",
                    GUINT_TO_POINTER (GCAL_VIEW_TYPE_DAILY));
  g_signal_connect (button,
                    "clicked",
                    G_CALLBACK (_gcal_main_toolbar_view_changed),
                    GCAL_MAIN_TOOLBAR (object));

  /* week */
  button = gtk_button_new_with_label (_("Week"));
  gtk_widget_set_size_request (button, 80, -1);

  context = gtk_widget_get_style_context (button);
  gtk_style_context_add_class (context, "raised");

  gtk_container_add (GTK_CONTAINER (views_box), button);

  g_object_set_data (G_OBJECT (button),
                    "view-type",
                    GUINT_TO_POINTER (GCAL_VIEW_TYPE_WEEKLY));
  g_signal_connect (button,
                    "clicked",
                    G_CALLBACK (_gcal_main_toolbar_view_changed),
                    GCAL_MAIN_TOOLBAR (object));

  /* month */
  button = gtk_button_new_with_label (_("Month"));
  gtk_widget_set_size_request (button, 80, -1);

  context = gtk_widget_get_style_context (button);
  gtk_style_context_add_class (context, "raised");

  gtk_container_add (GTK_CONTAINER (views_box), button);

  g_object_set_data (G_OBJECT (button),
                    "view-type",
                    GUINT_TO_POINTER (GCAL_VIEW_TYPE_MONTHLY));
  g_signal_connect (button,
                    "clicked",
                    G_CALLBACK (_gcal_main_toolbar_view_changed),
                    GCAL_MAIN_TOOLBAR (object));

  /* year */
  button = gtk_button_new_with_label (_("Year"));
  gtk_widget_set_size_request (button, 80, -1);

  context = gtk_widget_get_style_context (button);
  gtk_style_context_add_class (context, "raised");

  gtk_container_add (GTK_CONTAINER (views_box), button);

  g_object_set_data (G_OBJECT (button),
                    "view-type",
                    GUINT_TO_POINTER (GCAL_VIEW_TYPE_YEARLY));
  g_signal_connect (button,
                    "clicked",
                    G_CALLBACK (_gcal_main_toolbar_view_changed),
                    GCAL_MAIN_TOOLBAR (object));

  /* list */
  button = gtk_button_new_with_label (_("List"));
  gtk_widget_set_size_request (button, 80, -1);

  context = gtk_widget_get_style_context (button);
  gtk_style_context_add_class (context, "raised");

  gtk_container_add (GTK_CONTAINER (views_box), button);

  g_object_set_data (G_OBJECT (button),
                    "view-type",
                    GUINT_TO_POINTER (GCAL_VIEW_TYPE_LIST));
  g_signal_connect (button,
                    "clicked",
                    G_CALLBACK (_gcal_main_toolbar_view_changed),
                    GCAL_MAIN_TOOLBAR (object));

  /* spacer */
  spacer = gtk_tool_item_new ();
  gtk_tool_item_set_expand (spacer, TRUE);
  gtk_toolbar_insert (GTK_TOOLBAR (priv->widget), spacer, -1);

  /* add */
  button = gtk_button_new ();
  gtk_container_add (
      GTK_CONTAINER (button),
      gtk_image_new_from_icon_name ("list-add-symbolic",
                                    GTK_ICON_SIZE_MENU));

  context = gtk_widget_get_style_context (button);
  gtk_style_context_add_class (context, "raised");

  g_signal_connect (button,
                    "clicked",
                    G_CALLBACK (_gcal_main_toolbar_add_event),
                    GCAL_MAIN_TOOLBAR (object));

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
_gcal_main_toolbar_view_changed (GtkWidget *button,
                                 gpointer   user_data)
{
  GcalMainToolbar *toolbar;
  guint view_type;

  toolbar = GCAL_MAIN_TOOLBAR (user_data);
  view_type = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (button),
                                                   "view-type"));

  g_signal_emit (toolbar, signals[VIEW_CHANGED], 0, view_type);
}

static void
_gcal_main_toolbar_sources_shown (GtkWidget *button,
                                  gpointer   user_data)
{
  GcalMainToolbar *toolbar;

  toolbar = GCAL_MAIN_TOOLBAR (user_data);
  g_signal_emit (toolbar,
                 signals[SOURCES_SHOWN],
                 0,
                 gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)));
}

static void
_gcal_main_toolbar_add_event (GtkWidget *button,
                              gpointer   user_data)
{
  GcalMainToolbar *toolbar;

  toolbar = GCAL_MAIN_TOOLBAR (user_data);
  g_signal_emit (toolbar, signals[ADD_EVENT], 0);
}

ClutterActor*
gcal_main_toolbar_new (void)
{
  return g_object_new (gcal_main_toolbar_get_type (), NULL);
}
