/* -*- mode: c; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gcal-searchbar.c
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

#include "gcal-searchbar.h"
#include "gcal-utils.h"

#include <glib/gi18n.h>

#include <math.h>

struct _GcalSearchbarPrivate
{
  GtkWidget           *widget;

  GtkToolItem         *left_item;
  GtkToolItem         *central_item;

  /* widgets */
  GtkWidget           *done_button;
  GtkWidget           *search_entry;

  GtkSizeGroup        *hsize_group;
};

enum
{
  /* From overview mode */
  DONE = 1,

  NUM_SIGNALS
};

static guint signals[NUM_SIGNALS] = { 0, };

static void gcal_searchbar_constructed            (GObject      *object);

static void gcal_searchbar_set_widgets            (GcalSearchbar  *searchbar);

static void gcal_searchbar_done_clicked           (GtkWidget    *button,
                                                   gpointer      user_data);

G_DEFINE_TYPE (GcalSearchbar, gcal_searchbar, GTK_CLUTTER_TYPE_ACTOR);

static void
gcal_searchbar_class_init (GcalSearchbarClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = gcal_searchbar_constructed;

  signals[DONE] = g_signal_new ("done",
                                GCAL_TYPE_SEARCHBAR,
                                G_SIGNAL_RUN_LAST,
                                G_STRUCT_OFFSET (GcalSearchbarClass,
                                                 done),
                                NULL, NULL,
                                g_cclosure_marshal_VOID__VOID,
                                G_TYPE_NONE,
                                0);

  g_type_class_add_private ((gpointer) klass, sizeof(GcalSearchbarPrivate));
}

static void
gcal_searchbar_init (GcalSearchbar *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            GCAL_TYPE_SEARCHBAR,
                                            GcalSearchbarPrivate);
}

static void
gcal_searchbar_constructed (GObject *object)
{
  GcalSearchbarPrivate *priv;

  GtkStyleContext *context;
  GtkToolItem *spacer;
  GtkToolItem *dummy_spacer;
  GtkWidget *bin;

  priv = GCAL_SEARCHBAR (object)->priv;
  if (G_OBJECT_CLASS (gcal_searchbar_parent_class)->constructed != NULL)
    G_OBJECT_CLASS (gcal_searchbar_parent_class)->constructed (object);

  priv->widget = gtk_toolbar_new ();
  gtk_widget_set_hexpand (priv->widget, TRUE);
  gtk_widget_set_vexpand (priv->widget, TRUE);
  gtk_toolbar_set_icon_size (GTK_TOOLBAR (priv->widget),
                             GTK_ICON_SIZE_BUTTON);

  context = gtk_widget_get_style_context (priv->widget);
  gtk_style_context_add_class (context, "main-toolbar");

  /* adding searchbar */
  bin = gtk_clutter_actor_get_widget (GTK_CLUTTER_ACTOR (object));
  gtk_container_add (GTK_CONTAINER (bin), priv->widget);

  priv->hsize_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

  /* adding sections */
  /* left */
  priv->left_item = gtk_tool_item_new ();
  gtk_toolbar_insert (GTK_TOOLBAR (priv->widget), priv->left_item, 0);
  gtk_size_group_add_widget (priv->hsize_group, GTK_WIDGET (priv->left_item));

  /* spacer */
  spacer = gtk_tool_item_new ();
  gtk_tool_item_set_expand (spacer, TRUE);
  gtk_toolbar_insert (GTK_TOOLBAR (priv->widget), spacer, -1);

  /* central */
  priv->central_item = gtk_tool_item_new ();
  gtk_toolbar_insert (GTK_TOOLBAR (priv->widget), priv->central_item, -1);

  /* spacer */
  spacer = gtk_tool_item_new ();
  gtk_tool_item_set_expand (spacer, TRUE);
  gtk_toolbar_insert (GTK_TOOLBAR (priv->widget), spacer, -1);

  /* right spacer */
  dummy_spacer = gtk_tool_item_new ();
  gtk_toolbar_insert (GTK_TOOLBAR (priv->widget), dummy_spacer, -1);
  gtk_size_group_add_widget (priv->hsize_group, GTK_WIDGET (dummy_spacer));

  gcal_searchbar_set_widgets (GCAL_SEARCHBAR (object));
  gtk_widget_show_all (bin);
}

static void
gcal_searchbar_set_widgets (GcalSearchbar *searchbar)
{
  GcalSearchbarPrivate *priv;
  GtkStyleContext *context;

  g_return_if_fail (GCAL_IS_SEARCHBAR (searchbar));
  priv = searchbar->priv;

  /* done */
  priv->done_button = gtk_button_new_with_label (_("Done"));
  gtk_widget_set_valign (priv->done_button, GTK_ALIGN_CENTER);
  gtk_widget_set_vexpand (priv->done_button, FALSE);

  gtk_widget_set_size_request (priv->done_button, 100, -1);
  g_object_set (gtk_bin_get_child (GTK_BIN (priv->done_button)),
                "margin", get_icon_margin (),
                NULL);

  context = gtk_widget_get_style_context (priv->done_button);
  gtk_style_context_add_class (context, "suggested-action");

  g_signal_connect (priv->done_button,
                    "clicked",
                    G_CALLBACK (gcal_searchbar_done_clicked),
                    searchbar);
  gtk_container_add (GTK_CONTAINER (priv->left_item), priv->done_button);
  gtk_widget_show_all (priv->done_button);

  /* search_entry */
  priv->search_entry = gtk_search_entry_new ();
  gtk_entry_set_placeholder_text (GTK_ENTRY (priv->search_entry), _("Search..."));
  gtk_widget_set_size_request (priv->search_entry, 450, -1);

  gtk_container_add (GTK_CONTAINER (priv->central_item), priv->search_entry);
  gtk_widget_show_all (priv->search_entry);
}

static void
gcal_searchbar_done_clicked (GtkWidget *button,
                             gpointer   user_data)
{
  GcalSearchbar *searchbar;

  searchbar = GCAL_SEARCHBAR (user_data);
  g_signal_emit (searchbar, signals[DONE], 0);
}

/* Public API */
ClutterActor*
gcal_searchbar_new (void)
{
  return g_object_new (gcal_searchbar_get_type (), NULL);
}
