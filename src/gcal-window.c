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
#include "gcal-manager.h"
#include "gcal-floating-container.h"
#include "gcal-main-toolbar.h"
#include "gcal-month-view.h"
#include "gcal-utils.h"

#include <clutter/clutter.h>
#include <clutter-gtk/clutter-gtk.h>

#include <libedataserverui/e-cell-renderer-color.h>

#include <libical/icaltime.h>

struct _GcalWindowPrivate
{
  ClutterActor *main_toolbar;
  ClutterActor *notebook_actor;
  ClutterActor *sources_actor;

  GtkWidget    *notebook;
  GtkWidget    *sources_view;
  GtkWidget    *views [5];
};

static void   gcal_window_constructed            (GObject         *object);

GcalManager*  _gcal_window_get_manager           (GcalWindow      *window);

static void   _gcal_window_set_sources_view      (GcalWindow      *window);

static void   _gcal_window_view_changed          (GcalMainToolbar *main_toolbar,
                                                  GcalViewType     view_type,
                                                  gpointer         user_data);

static void   _gcal_window_sources_shown         (GcalMainToolbar *main_toolbar,
                                                  gboolean         visible,
                                                  gpointer         user_data);

static void   _gcal_window_sources_row_activated (GtkTreeView       *tree_view,
                                                  GtkTreePath       *path,
                                                  GtkTreeViewColumn *column,
                                                  gpointer           user_data);

G_DEFINE_TYPE(GcalWindow, gcal_window, GTK_TYPE_APPLICATION_WINDOW)

static void
gcal_window_class_init(GcalWindowClass *klass)
{
  G_OBJECT_CLASS (klass)->constructed = gcal_window_constructed;

  g_type_class_add_private((gpointer)klass, sizeof(GcalWindowPrivate));
}

static void gcal_window_init(GcalWindow *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self,
                                           GCAL_TYPE_WINDOW,
                                           GcalWindowPrivate);
}

static void
gcal_window_constructed (GObject *object)
{
  GcalWindowPrivate *priv;
  GtkWidget *embed;
  ClutterActor *stage;
  ClutterActor *base;
  ClutterLayoutManager *base_layout_manager;
  ClutterActor *body_actor;
  ClutterLayoutManager *body_layout_manager;
  ClutterActor *contents_actor;
  ClutterLayoutManager *contents_layout_manager;
  GtkWidget *holder;

  GtkStyleContext *context;

  if (G_OBJECT_CLASS (gcal_window_parent_class)->constructed != NULL)
    G_OBJECT_CLASS (gcal_window_parent_class)->constructed (object);

  priv = GCAL_WINDOW (object)->priv;

  embed = gtk_clutter_embed_new ();
  gtk_container_add (GTK_CONTAINER (object), embed);
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

  /* main_toolbar */
  priv->main_toolbar = gcal_main_toolbar_new ();
  clutter_box_layout_pack (CLUTTER_BOX_LAYOUT (body_layout_manager),
                           priv->main_toolbar,
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
                           CLUTTER_BOX_ALIGNMENT_CENTER,
                           CLUTTER_BOX_ALIGNMENT_CENTER);

  /* notebook widget for holding views */
  priv->notebook = gtk_notebook_new ();
  gtk_notebook_set_show_tabs (GTK_NOTEBOOK (priv->notebook), FALSE);
  gtk_notebook_set_show_border (GTK_NOTEBOOK (priv->notebook), FALSE);
  gtk_widget_show (priv->notebook);

  priv->notebook_actor = gtk_clutter_actor_new_with_contents (priv->notebook);

  clutter_bin_layout_add (CLUTTER_BIN_LAYOUT (contents_layout_manager),
                          priv->notebook_actor,
                          CLUTTER_BIN_ALIGNMENT_FILL,
                          CLUTTER_BIN_ALIGNMENT_FILL);

  icaltimetype *date = g_new (icaltimetype, 1);
  *date = icaltime_today ();
  priv->views[GCAL_VIEW_TYPE_MONTHLY] = gcal_month_view_new (date);

  context = gtk_widget_get_style_context (priv->views[GCAL_VIEW_TYPE_MONTHLY]);
  gtk_style_context_add_class (context, "views");

  gtk_widget_show (priv->views[GCAL_VIEW_TYPE_MONTHLY]);
  gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook),
                            priv->views[GCAL_VIEW_TYPE_MONTHLY],
                            NULL);

  /* sources view */
  holder = gcal_floating_container_new ();

  context = gtk_widget_get_style_context (holder);
  gtk_style_context_add_class (context, "sources-views");

  gtk_widget_show_all (holder);

  priv->sources_actor = gtk_clutter_actor_new_with_contents (holder);
  clutter_actor_set_margin_left (priv->sources_actor, 10);
  clutter_actor_set_margin_top (priv->sources_actor, 10);
  clutter_actor_set_opacity (priv->sources_actor, 0);
  clutter_bin_layout_add (CLUTTER_BIN_LAYOUT (contents_layout_manager),
                          priv->sources_actor,
                          CLUTTER_BIN_ALIGNMENT_START,
                          CLUTTER_BIN_ALIGNMENT_START);

  /* signals connection/handling */
  g_signal_connect (priv->main_toolbar,
                    "view-changed",
                    G_CALLBACK (_gcal_window_view_changed),
                    object);
  g_signal_connect (priv->main_toolbar,
                    "sources-shown",
                    G_CALLBACK (_gcal_window_sources_shown),
                    object);

  gtk_widget_show (embed);
}

GcalManager*
_gcal_window_get_manager (GcalWindow *window)
{
  GcalApplication *app;
  app = GCAL_APPLICATION (gtk_window_get_application (GTK_WINDOW (window)));

  return gcal_application_get_manager (app);
}

static void
_gcal_window_set_sources_view (GcalWindow *window)
{
  GcalWindowPrivate *priv;
  GcalManager *manager;

  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;

  priv = window->priv;

  manager = _gcal_window_get_manager (window);
  priv->sources_view = gtk_tree_view_new_with_model (
      GTK_TREE_MODEL (gcal_manager_get_sources_model (manager)));
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (priv->sources_view),
                                     FALSE);
  gtk_tree_selection_set_mode (
      gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->sources_view)),
      GTK_SELECTION_NONE);
  gcal_gtk_tree_view_set_activate_on_single_click (
      GTK_TREE_VIEW (priv->sources_view),
      TRUE);

  renderer = gtk_cell_renderer_toggle_new ();
  gtk_cell_renderer_toggle_set_radio (GTK_CELL_RENDERER_TOGGLE (renderer),
                                      TRUE);
  column = gtk_tree_view_column_new_with_attributes ("",
                                                     renderer,
                                                     "active",
                                                     2,
                                                     NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (priv->sources_view),
                               column);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("",
                                                     renderer,
                                                     "text",
                                                     1,
                                                     NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (priv->sources_view),
                               column);

  renderer = e_cell_renderer_color_new ();
  column = gtk_tree_view_column_new_with_attributes ("",
                                                     renderer,
                                                     "color",
                                                     3,
                                                     NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (priv->sources_view),
                               column);

  gtk_style_context_add_class (
      gtk_widget_get_style_context (priv->sources_view),
      "osd");
  gtk_container_add (
      GTK_CONTAINER (gtk_clutter_actor_get_contents (
                        GTK_CLUTTER_ACTOR (priv->sources_actor))),
      priv->sources_view);
  gtk_widget_show (priv->sources_view);

  /* signals */
  g_signal_connect (priv->sources_view,
                    "row-activated",
                    G_CALLBACK (_gcal_window_sources_row_activated),
                    window);
}

static void
_gcal_window_view_changed (GcalMainToolbar *main_toolbar,
                           GcalViewType     view_type,
                           gpointer         user_data)
{
  GcalWindow *window;
  GcalManager *manager;

  /* FIXME: demo code */
  icaltimetype *first_day;
  icaltimetype *last_day;
  first_day = g_new0 (icaltimetype, 1);
  last_day = g_new0 (icaltimetype, 1);
  *first_day = icaltime_from_timet (time (NULL), 0);
  first_day->day = 1;
  *last_day = *first_day;
  last_day->day = 28;

  window = GCAL_WINDOW (user_data);
  manager = _gcal_window_get_manager (window);
  gcal_manager_set_new_range (manager, first_day, last_day);
  g_debug ("GcalViewType in GcalWindow %d", view_type);
  g_free (first_day);
  g_free (last_day);
}

static void
_gcal_window_sources_shown (GcalMainToolbar *main_toolbar,
                            gboolean         visible,
                            gpointer         user_data)
{
  GcalWindowPrivate *priv;
  priv  = ((GcalWindow*) user_data)->priv;

  if (visible)
    {
      clutter_actor_animate (priv->sources_actor,
                             CLUTTER_LINEAR, 150,
                             "opacity", 255,
                             NULL);
    }
  else
    {
      clutter_actor_animate (priv->sources_actor,
                             CLUTTER_LINEAR, 150,
                             "opacity", 0,
                             NULL);
    }
}

static void
_gcal_window_sources_row_activated (GtkTreeView       *tree_view,
                                    GtkTreePath       *path,
                                    GtkTreeViewColumn *column,
                                    gpointer           user_data)
{
  GcalWindowPrivate *priv;
  GtkTreeIter iter;
  gboolean active;

  priv  = ((GcalWindow*) user_data)->priv;

  gtk_tree_model_get_iter (
      gtk_tree_view_get_model (GTK_TREE_VIEW (priv->sources_view)),
      &iter,
      path);
  gtk_tree_model_get (
      gtk_tree_view_get_model (GTK_TREE_VIEW (priv->sources_view)),
      &iter,
      2, &active,
      -1);

  active ^= 1;
  gtk_list_store_set (
      GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (priv->sources_view))),
      &iter,
      2, active,
      -1);
}

GtkWidget*
gcal_window_new (GcalApplication *app)
{
  GcalWindow *win =  g_object_new (GCAL_TYPE_WINDOW,
                                   "application",
                                   GTK_APPLICATION (app),
                                   NULL);
  _gcal_window_set_sources_view (win);
  return GTK_WIDGET (win);
}
