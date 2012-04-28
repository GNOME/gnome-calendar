/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 2 -*- */
/*
 * gcal-window.c
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

#include "gcal-window.h"
#include "gcal-main-toolbar.h"
#include "gcal-month-view.h"
#include "gcal-utils.h"

#include <clutter/clutter.h>
#include <clutter-gtk/clutter-gtk.h>

struct _GcalWindowPrivate
{
  ClutterActor *main_toolbar;

  GtkWidget    *notebook;
  GtkWidget    *views [5];
};

static void _gcal_window_view_changed  (GcalMainToolbar *main_toolbar,
                                        GcalViewType     view_type,
                                        gpointer         user_data);
static void _gcal_window_sources_shown (GcalMainToolbar *main_toolbar,
                                        gboolean         visible,
                                        gpointer         user_data);

G_DEFINE_TYPE(GcalWindow, gcal_window, GTK_TYPE_APPLICATION_WINDOW)

static void
gcal_window_class_init(GcalWindowClass *klass)
{
  g_type_class_add_private((gpointer)klass, sizeof(GcalWindowPrivate));
}

static void gcal_window_init(GcalWindow *self)
{
  GtkWidget *embed;
  ClutterActor *stage;
  ClutterActor *base;
  ClutterLayoutManager *base_layout_manager;
  ClutterActor *body_actor;
  ClutterLayoutManager *body_layout_manager;
  ClutterActor *main_toolbar;
  ClutterActor *contents_actor;
  ClutterLayoutManager *contents_layout_manager;
  ClutterActor *notebook_actor;

  GtkStyleContext *context;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self,
                                           GCAL_TYPE_WINDOW,
                                           GcalWindowPrivate);

  embed = gtk_clutter_embed_new ();
  gtk_container_add (GTK_CONTAINER (self), embed);
  stage = gtk_clutter_embed_get_stage (GTK_CLUTTER_EMBED (embed));

  /* base */
  base_layout_manager = clutter_box_layout_new ();
  clutter_box_layout_set_vertical (CLUTTER_BOX_LAYOUT (base_layout_manager),
                                   TRUE);
  base  = clutter_actor_new ();
  clutter_actor_set_layout_manager (base, base_layout_manager);
  clutter_actor_add_constraint (base,
                                clutter_bind_constraint_new (stage,
                                                             CLUTTER_BIND_SIZE,
                                                             0));
  clutter_actor_add_child (stage, base);

  /* body_actor */
  body_layout_manager = clutter_box_layout_new ();
  clutter_box_layout_set_vertical (CLUTTER_BOX_LAYOUT (body_layout_manager),
                                   TRUE);

  body_actor = clutter_actor_new ();
  clutter_actor_set_layout_manager (body_actor, body_layout_manager);
  clutter_box_layout_pack (CLUTTER_BOX_LAYOUT (base_layout_manager),
                           body_actor,
                           TRUE,
                           TRUE,
                           TRUE,
                           CLUTTER_BOX_ALIGNMENT_CENTER,
                           CLUTTER_BOX_ALIGNMENT_CENTER);

  main_toolbar = gcal_main_toolbar_new ();
  clutter_box_layout_pack (CLUTTER_BOX_LAYOUT (body_layout_manager),
                           main_toolbar,
                           FALSE,
                           TRUE,
                           FALSE,
                           CLUTTER_BOX_ALIGNMENT_START,
                           CLUTTER_BOX_ALIGNMENT_START);

  /* contents */
  contents_layout_manager = clutter_bin_layout_new (CLUTTER_BIN_ALIGNMENT_CENTER,
                                                    CLUTTER_BIN_ALIGNMENT_CENTER);
  contents_actor = clutter_actor_new ();
  clutter_actor_set_layout_manager (contents_actor, contents_layout_manager);
  clutter_box_layout_pack (CLUTTER_BOX_LAYOUT (body_layout_manager),
                           contents_actor,
                           TRUE,
                           TRUE,
                           TRUE,
                           CLUTTER_BOX_ALIGNMENT_START,
                           CLUTTER_BOX_ALIGNMENT_START);

  self->priv->notebook = gtk_notebook_new ();
  gtk_notebook_set_show_tabs (GTK_NOTEBOOK (self->priv->notebook), FALSE);
  gtk_notebook_set_show_border (GTK_NOTEBOOK (self->priv->notebook), FALSE);
  gtk_widget_show (self->priv->notebook);

  notebook_actor = gtk_clutter_actor_new_with_contents (self->priv->notebook);

  clutter_bin_layout_add (CLUTTER_BIN_LAYOUT (contents_layout_manager),
                                              notebook_actor,
                                              CLUTTER_BIN_ALIGNMENT_FILL,
                                              CLUTTER_BIN_ALIGNMENT_FILL);

  GDate *date = g_date_new ();
  g_date_set_time_t (date, time (NULL));
  self->priv->views[GCAL_VIEW_TYPE_MONTHLY] = gcal_month_view_new (date);

  context = gtk_widget_get_style_context (self->priv->views[GCAL_VIEW_TYPE_MONTHLY]);
  gtk_style_context_add_class (context, "views");

  gtk_widget_show (self->priv->views[GCAL_VIEW_TYPE_MONTHLY]);
  gtk_notebook_append_page (GTK_NOTEBOOK (self->priv->notebook),
                            self->priv->views[GCAL_VIEW_TYPE_MONTHLY],
                            NULL);

  g_signal_connect (main_toolbar,
                    "view-changed",
                    G_CALLBACK (_gcal_window_view_changed),
                    self);
  g_signal_connect (main_toolbar,
                    "sources-shown",
                    G_CALLBACK (_gcal_window_sources_shown),
                    self);

  gtk_widget_show (embed);
}

static void
_gcal_window_view_changed (GcalMainToolbar *main_toolbar,
                           GcalViewType     view_type,
                           gpointer         user_data)
{
  g_print ("GcalViewType in GcalWindow %d\n", view_type);
}

static void
_gcal_window_sources_shown (GcalMainToolbar *main_toolbar,
                            gboolean         visible,
                            gpointer         user_data)
{
  g_print ("Visible: %s\n", visible ? "yes" : "no");
}

GtkWidget*
gcal_window_new (void)
{
  return g_object_new (GCAL_TYPE_WINDOW, NULL);
}
