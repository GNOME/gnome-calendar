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
#include "gcal-toolbar.h"
#include "gcal-month-view.h"
#include "gcal-week-view.h"
#include "gcal-view.h"
#include "gcal-event-widget.h"
#include "gcal-event-view.h"
#include "gcal-enum-types.h"
#include "gtk-notification.h"

#include <glib/gi18n.h>
#include <clutter/clutter.h>
#include <clutter-gtk/clutter-gtk.h>

#include <libedataserverui/libedataserverui.h>

#include <libical/icaltime.h>

struct _GcalWindowPrivate
{
  ClutterActor        *main_toolbar;
  ClutterActor        *contents_actor;
  ClutterActor        *notebook_actor;
  ClutterActor        *sources_actor;
  ClutterActor        *notification_actor;

  GtkWidget           *notebook;
  GtkWidget           *sources_view;
  GtkWidget           *views [5];
  GtkWidget           *add_view;

  GcalWindowViewType   active_view;
  icaltimetype        *active_date;

  /* temp to keep the will_delete event uuid */
  gchar               *event_to_delete;
};

enum
{
  PROP_0,
  PROP_ACTIVE_VIEW,
  PROP_ACTIVE_DATE
};

static void           gcal_window_constructed            (GObject             *object);

static void           gcal_window_finalize               (GObject             *object);

static void           gcal_window_set_property           (GObject             *object,
                                                          guint                property_id,
                                                          const GValue        *value,
                                                          GParamSpec          *pspec);

static void           gcal_window_get_property           (GObject             *object,
                                                          guint                property_id,
                                                          GValue              *value,
                                                          GParamSpec          *pspec);

static GcalManager*   gcal_window_get_manager            (GcalWindow          *window);

static void           gcal_window_set_active_view        (GcalWindow          *window,
                                                          GcalWindowViewType   view_type);

static void           gcal_window_set_sources_view       (GcalWindow          *window);

static void           gcal_window_init_event_view        (GcalWindow          *window);

static void           gcal_window_view_changed           (GcalToolbar         *main_toolbar,
                                                          GcalWindowViewType   view_type,
                                                          gpointer             user_data);

static void           gcal_window_sources_shown          (GcalToolbar         *main_toolbar,
                                                          gboolean             visible,
                                                          gpointer             user_data);

static void           gcal_window_add_event              (GcalToolbar         *main_toolbar,
                                                          gpointer             user_data);

static void           gcal_window_back_last_view         (GtkWidget           *widget,
                                                          gpointer             user_data);

static void           gcal_window_edit_event             (GcalToolbar         *main_toolbar,
                                                          gpointer             user_data);

static void           gcal_window_done_edit_event        (GcalToolbar         *main_toolbar,
                                                          gpointer             user_data);

static void           gcal_window_sources_row_activated  (GtkTreeView         *tree_view,
                                                          GtkTreePath         *path,
                                                          GtkTreeViewColumn   *column,
                                                          gpointer             user_data);

static void           gcal_window_events_added           (GcalManager         *manager,
                                                          gpointer             events_list,
                                                          gpointer             user_data);

static void           gcal_window_events_removed         (GcalManager         *manager,
                                                          gpointer             events_list,
                                                          gpointer             user_data);

static void           gcal_window_event_activated        (GcalEventWidget     *event_widget,
                                                          gpointer             user_data);

static void           gcal_window_will_remove_event      (GcalEventView       *view,
                                                          gchar               *event_uuid,
                                                          gpointer             user_data);

static void           gcal_window_remove_event           (GtkNotification     *notification,
                                                          gpointer             user_data);

static void           gcal_window_undo_remove_event      (GtkButton           *button,
                                                          gpointer             user_data);

G_DEFINE_TYPE(GcalWindow, gcal_window, GTK_TYPE_APPLICATION_WINDOW)

static void
gcal_window_class_init(GcalWindowClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->constructed = gcal_window_constructed;
  object_class->finalize = gcal_window_finalize;
  object_class->set_property = gcal_window_set_property;
  object_class->get_property = gcal_window_get_property;

  g_object_class_install_property (object_class,
                                   PROP_ACTIVE_VIEW,
                                   g_param_spec_enum ("active-view",
                                                      "Active View",
                                                      "The active view, eg: month, week, etc.",
                                                      GCAL_WINDOW_VIEW_TYPE,
                                                      GCAL_WINDOW_VIEW_MONTH,
                                                      G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_ACTIVE_DATE,
                                   g_param_spec_boxed ("active-date",
                                                       "Date",
                                                       "The active/selected date",
                                                       ICAL_TIME_TYPE,
                                                       G_PARAM_CONSTRUCT |
                                                       G_PARAM_READWRITE));

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
  ClutterLayoutManager *contents_layout_manager;
  GtkWidget *holder;

  GtkStyleContext *context;

  if (G_OBJECT_CLASS (gcal_window_parent_class)->constructed != NULL)
    G_OBJECT_CLASS (gcal_window_parent_class)->constructed (object);

  priv = GCAL_WINDOW (object)->priv;

  /* internal data init*/
  priv->event_to_delete = NULL;

  /* FIXME: here read the data from somewehere */
  priv->active_date = g_new (icaltimetype, 1);
  *(priv->active_date) = icaltime_from_timet (time (NULL), 0);

  /* ui init */
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
  priv->main_toolbar = gcal_toolbar_new ();
  clutter_box_layout_pack (CLUTTER_BOX_LAYOUT (body_layout_manager),
                           priv->main_toolbar,
                           FALSE,
                           TRUE,
                           FALSE,
                           CLUTTER_BOX_ALIGNMENT_START,
                           CLUTTER_BOX_ALIGNMENT_START);

  /* contents */
  contents_layout_manager =
    clutter_bin_layout_new (CLUTTER_BIN_ALIGNMENT_CENTER,
                            CLUTTER_BIN_ALIGNMENT_CENTER);
  priv->contents_actor = clutter_actor_new ();
  clutter_actor_set_layout_manager (priv->contents_actor,
                                    contents_layout_manager);
  clutter_box_layout_pack (CLUTTER_BOX_LAYOUT (body_layout_manager),
                           priv->contents_actor,
                           TRUE,
                           TRUE,
                           TRUE,
                           CLUTTER_BOX_ALIGNMENT_CENTER,
                           CLUTTER_BOX_ALIGNMENT_CENTER);

  /* notebook widget for holding views */
  holder = gtk_frame_new (NULL);

  priv->notebook = gtk_notebook_new ();
  gtk_notebook_set_show_tabs (GTK_NOTEBOOK (priv->notebook), FALSE);
  gtk_notebook_set_show_border (GTK_NOTEBOOK (priv->notebook), FALSE);

  gtk_widget_show (priv->notebook);

  gtk_container_add (GTK_CONTAINER (holder), priv->notebook);
  gtk_widget_show (holder);

  priv->notebook_actor = gtk_clutter_actor_new_with_contents (holder);

  clutter_bin_layout_add (CLUTTER_BIN_LAYOUT (contents_layout_manager),
                          priv->notebook_actor,
                          CLUTTER_BIN_ALIGNMENT_FILL,
                          CLUTTER_BIN_ALIGNMENT_FILL);
  context =
    gtk_widget_get_style_context (
        gtk_clutter_actor_get_widget (
          GTK_CLUTTER_ACTOR (priv->notebook_actor)));
  gtk_style_context_add_class (context, "contents");

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
  clutter_actor_hide (priv->sources_actor);

  context =
    gtk_widget_get_style_context (
        gtk_clutter_actor_get_widget (
          GTK_CLUTTER_ACTOR (priv->sources_actor)));
  gtk_style_context_add_class (context, "overlay");

  /* notifications manager */
  priv->notification_actor = gtk_clutter_actor_new ();
  clutter_bin_layout_add (CLUTTER_BIN_LAYOUT (contents_layout_manager),
                          priv->notification_actor,
                          CLUTTER_BIN_ALIGNMENT_CENTER,
                          CLUTTER_BIN_ALIGNMENT_START);

  context =
    gtk_widget_get_style_context (
        gtk_clutter_actor_get_widget (
          GTK_CLUTTER_ACTOR (priv->notification_actor)));
  gtk_style_context_add_class (context, "overlay");
  clutter_actor_hide (priv->notification_actor);

  /* signals connection/handling */
  g_signal_connect (priv->main_toolbar,
                    "view-changed",
                    G_CALLBACK (gcal_window_view_changed),
                    object);
  g_signal_connect (priv->main_toolbar,
                    "sources-shown",
                    G_CALLBACK (gcal_window_sources_shown),
                    object);
  g_signal_connect (priv->main_toolbar,
                    "add-event",
                    G_CALLBACK (gcal_window_add_event),
                    object);

  g_signal_connect (priv->main_toolbar,
                    "back",
                    G_CALLBACK (gcal_window_back_last_view),
                    object);

  g_signal_connect (priv->main_toolbar,
                    "edit-event",
                    G_CALLBACK (gcal_window_edit_event),
                    object);
  g_signal_connect (priv->main_toolbar,
                    "done-edit",
                    G_CALLBACK (gcal_window_done_edit_event),
                    object);

  gtk_widget_show (embed);
}

static void
gcal_window_finalize (GObject *object)
{
  GcalWindowPrivate *priv;

  g_return_if_fail (GCAL_IS_WINDOW (object));
  priv = GCAL_WINDOW (object)->priv;

  if (priv->active_date != NULL)
    g_free (priv->active_date);

  if (G_OBJECT_CLASS (gcal_window_parent_class)->finalize != NULL)
    G_OBJECT_CLASS (gcal_window_parent_class)->finalize (object);
}

static void
gcal_window_set_property (GObject      *object,
                          guint         property_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  GcalWindowPrivate *priv;

  g_return_if_fail (GCAL_IS_WINDOW (object));
  priv = GCAL_WINDOW (object)->priv;

  switch (property_id)
    {
    case PROP_ACTIVE_VIEW:
      gcal_window_set_active_view (GCAL_WINDOW (object),
                                   g_value_get_enum (value));
      return;
    case PROP_ACTIVE_DATE:
      priv->active_date = g_value_dup_boxed (value);
      return;
    }

  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
gcal_window_get_property (GObject    *object,
                          guint       property_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  GcalWindowPrivate *priv;

  priv = GCAL_WINDOW (object)->priv;

  switch (property_id)
    {
    case PROP_ACTIVE_VIEW:
      g_value_set_enum (value, priv->active_view);
      return;
    case PROP_ACTIVE_DATE:
      g_value_set_boxed (value, priv->active_date);
      return;
    }

  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static GcalManager*
gcal_window_get_manager (GcalWindow *window)
{
  GcalApplication *app;
  app = GCAL_APPLICATION (gtk_window_get_application (GTK_WINDOW (window)));

  return gcal_application_get_manager (app);
}

static void
gcal_window_set_active_view (GcalWindow         *window,
                             GcalWindowViewType  view_type)
{
  GcalWindowPrivate *priv;
  gint activated_page;

  gboolean update_range;
  icaltimetype *first_day;
  icaltimetype *last_day;

  g_return_if_fail (GCAL_IS_WINDOW (window));
  priv = window->priv;

  if ((activated_page = gtk_notebook_page_num (GTK_NOTEBOOK (priv->notebook),
                                               priv->views[view_type]))
      != -1)
    {
      gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook),
                                     activated_page);
      priv->active_view = view_type;
    }
  else
    {
      switch (view_type)
        {
          case GCAL_WINDOW_VIEW_WEEK:
            priv->views[GCAL_WINDOW_VIEW_WEEK] = gcal_week_view_new ();
            break;
          case GCAL_WINDOW_VIEW_MONTH:
            priv->views[GCAL_WINDOW_VIEW_MONTH] = gcal_month_view_new ();
            break;
          default:
            g_debug ("Unimplemented view yet");
            return;
        }
      g_object_bind_property (priv->views[view_type], "active-date",
                              window, "active-date",
                              G_BINDING_DEFAULT);

      priv->active_view = view_type;

      gtk_widget_show (priv->views[priv->active_view]);
      gtk_notebook_set_current_page (
          GTK_NOTEBOOK (priv->notebook),
          gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook),
                                    priv->views[priv->active_view],
                                    NULL));
    }

  g_object_notify (G_OBJECT (window), "active-view");

  update_range = ! gcal_view_contains (GCAL_VIEW (priv->views[priv->active_view]),
                                       priv->active_date);

  gcal_view_set_date (GCAL_VIEW (priv->views[priv->active_view]), priv->active_date);
  if (update_range)
    {
      first_day = gcal_view_get_initial_date (
          GCAL_VIEW (priv->views[priv->active_view]));
      last_day = gcal_view_get_final_date (
          GCAL_VIEW (priv->views[priv->active_view]));

      gcal_manager_set_new_range (
          gcal_window_get_manager (window),
          first_day,
          last_day);

      g_free (first_day);
      g_free (last_day);
    }
}

static void
gcal_window_set_sources_view (GcalWindow *window)
{
  GcalWindowPrivate *priv;
  GcalManager *manager;

  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;

  priv = window->priv;

  manager = gcal_window_get_manager (window);
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
                    G_CALLBACK (gcal_window_sources_row_activated),
                    window);
}

static void
gcal_window_init_event_view (GcalWindow *window)
{
  GcalWindowPrivate *priv;

  g_return_if_fail (GCAL_IS_WINDOW (window));
  priv = window->priv;

  priv->add_view = gcal_event_view_new_with_manager (
      gcal_window_get_manager (window));
  g_object_set (priv->add_view,
                "hexpand", TRUE,
                "vexpand", TRUE,
                "margin-top", 10,
                "margin-left", 20,
                "margin-right", 20,
                NULL);

  g_signal_connect (priv->add_view,
                    "will-delete",
                    G_CALLBACK (gcal_window_will_remove_event),
                    window);

  gtk_widget_show (priv->add_view);
  gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook),
                            priv->add_view,
                            NULL);
}

static void
gcal_window_view_changed (GcalToolbar         *main_toolbar,
                           GcalWindowViewType  view_type,
                           gpointer            user_data)
{
  GcalWindowPrivate *priv;

  priv = GCAL_WINDOW (user_data)->priv;
  priv->active_view = view_type;

  gcal_window_set_active_view (GCAL_WINDOW (user_data), view_type);
}

static void
gcal_window_sources_shown (GcalToolbar *main_toolbar,
                            gboolean    visible,
                            gpointer    user_data)
{
  GcalWindowPrivate *priv;
  priv  = ((GcalWindow*) user_data)->priv;

  if (visible)
    {
      clutter_actor_show (priv->sources_actor);
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
                             "signal-swapped::completed", clutter_actor_hide, priv->sources_actor,
                             NULL);
    }
}

static void
gcal_window_add_event (GcalToolbar *main_toolbar,
                        gpointer    user_data)
{
  GcalWindowPrivate *priv;

  priv = GCAL_WINDOW (user_data)->priv;
  if (priv->add_view == NULL)
      gcal_window_init_event_view (GCAL_WINDOW (user_data));

  //TODO: Reload/clean/reinitialize status
  gtk_notebook_set_current_page (
      GTK_NOTEBOOK (priv->notebook),
      gtk_notebook_page_num (GTK_NOTEBOOK (priv->notebook), priv->add_view));

  gcal_toolbar_set_mode (GCAL_TOOLBAR (priv->main_toolbar),
                         GCAL_TOOLBAR_VIEW_EVENT);

  gcal_event_view_load_new (GCAL_EVENT_VIEW (priv->add_view));
}

/*
 * FIXME: Take into account the event we're viewing could be in edit-mode and we
 * need to advice the user and offers him to save/discard the changes
 */
static void
gcal_window_back_last_view (GtkWidget   *widget,
                            gpointer     user_data)
{
  GcalWindowPrivate *priv;
  gint activated_page;

  priv = GCAL_WINDOW (user_data)->priv;

  gcal_toolbar_set_mode (GCAL_TOOLBAR (priv->main_toolbar),
                         GCAL_TOOLBAR_OVERVIEW);

  if ((activated_page = gtk_notebook_page_num (GTK_NOTEBOOK (priv->notebook),
                                               priv->views[priv->active_view]))
      != -1)
    {
      gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook),
                                     activated_page);
    }
  else
    {
      //FIXME: there's something that needs to be done here.
      g_warning ("Your app has gone crazy");
    }
}

static void
gcal_window_edit_event (GcalToolbar *main_toolbar,
                        gpointer     user_data)
{
  GcalWindowPrivate *priv;

  priv = GCAL_WINDOW (user_data)->priv;
  if (priv->add_view == NULL)
      gcal_window_init_event_view (GCAL_WINDOW (user_data));

  //FIXME: check here, maybe this isn't need since edit-event
  //can be triggered only from add_view
  gtk_notebook_set_current_page (
      GTK_NOTEBOOK (priv->notebook),
      gtk_notebook_page_num (GTK_NOTEBOOK (priv->notebook), priv->add_view));
  gcal_event_view_enter_edit_mode (GCAL_EVENT_VIEW (priv->add_view));
}

static void
gcal_window_done_edit_event (GcalToolbar *main_toolbar,
                        gpointer     user_data)
{
  GcalWindowPrivate *priv;

  priv = GCAL_WINDOW (user_data)->priv;
  gcal_event_view_leave_edit_mode (GCAL_EVENT_VIEW (priv->add_view));
}

static void
gcal_window_sources_row_activated (GtkTreeView        *tree_view,
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

static void
gcal_window_events_added (GcalManager *manager,
                           gpointer    events_list,
                           gpointer    user_data)
{
  GcalWindowPrivate *priv;
  GSList *l;

  gchar **tokens;
  gchar *source_uid;
  gchar *event_uid;

  GtkWidget *event;
  icaltimetype *starting_date;
  gchar *summary;
  GdkRGBA *color;

  priv = GCAL_WINDOW (user_data)->priv;

  for (l = events_list; l != NULL; l = l->next)
    {

      tokens = g_strsplit ((gchar*) l->data, ":", -1);
      source_uid  = tokens[0];
      event_uid = tokens[1];
      starting_date = gcal_manager_get_event_start_date (manager,
                                                         source_uid,
                                                         event_uid);
      if (gcal_view_contains (
            GCAL_VIEW (priv->views[priv->active_view]),
            starting_date)
          && gcal_view_get_by_uuid (
              GCAL_VIEW (priv->views[priv->active_view]),
              (gchar*)l->data) == NULL)
        {
          summary = gcal_manager_get_event_summary (manager,
                                                    source_uid,
                                                    event_uid);
          color = gcal_manager_get_event_color (manager,
                                                source_uid,
                                                event_uid);
          event = gcal_event_widget_new ((gchar*) l->data);
          gcal_event_widget_set_summary (GCAL_EVENT_WIDGET (event), summary);
          gcal_event_widget_set_color (GCAL_EVENT_WIDGET (event), color);
          gcal_event_widget_set_date (GCAL_EVENT_WIDGET (event),
                                      starting_date);
          gcal_event_widget_set_all_day (
              GCAL_EVENT_WIDGET (event),
              gcal_manager_get_event_all_day (manager,
                                              source_uid,
                                              event_uid));
          gtk_widget_show (event);

          //FIXME: add event to every instantiated view
          gtk_container_add (
              GTK_CONTAINER (priv->views[priv->active_view]),
              event);

          g_signal_connect (event,
                            "activated",
                            G_CALLBACK (gcal_window_event_activated),
                            user_data);

          g_free (summary);
          gdk_rgba_free (color);
        }

      g_free (starting_date);
      g_strfreev (tokens);
    }
}

static void
gcal_window_events_removed (GcalManager *manager,
                            gpointer     events_list,
                            gpointer     user_data)
{
  GcalWindowPrivate *priv;
  GSList *l;
  GtkWidget *widget;

  g_return_if_fail (GCAL_IS_WINDOW (user_data));
  priv = GCAL_WINDOW (user_data)->priv;

  for (l = events_list; l != NULL; l = l->next)
    {
      gint i;
      for (i = 0; i < 5; i++)
        {
          if (priv->views[i] != NULL)
            {
              widget = gcal_view_get_by_uuid (GCAL_VIEW (priv->views[i]),
                                              (gchar*) l->data);
              gtk_widget_destroy (widget);
            }
        }
    }
}

static void
gcal_window_event_activated (GcalEventWidget *event_widget,
                             gpointer         user_data)
{
  GcalWindowPrivate *priv;

  g_return_if_fail (GCAL_IS_WINDOW (user_data));
  priv = GCAL_WINDOW (user_data)->priv;

  if (priv->add_view == NULL)
    gcal_window_init_event_view (GCAL_WINDOW (user_data));

  gtk_notebook_set_current_page (
    GTK_NOTEBOOK (priv->notebook),
    gtk_notebook_page_num (GTK_NOTEBOOK (priv->notebook), priv->add_view));

  gcal_event_view_load_event (GCAL_EVENT_VIEW (priv->add_view),
                              gcal_event_widget_peek_uuid (event_widget));

  gcal_toolbar_set_mode (GCAL_TOOLBAR (priv->main_toolbar),
                         GCAL_TOOLBAR_VIEW_EVENT);
}

static void
gcal_window_will_remove_event (GcalEventView *view,
                               gchar         *event_uuid,
                               gpointer      user_data)
{
  GcalWindowPrivate *priv;
  gint activated_page;

  GtkWidget *noty;
  GtkWidget *grid;
  GtkWidget *undo_button;

  GtkWidget *event_widget;

  priv = GCAL_WINDOW (user_data)->priv;

  gcal_toolbar_set_mode (GCAL_TOOLBAR (priv->main_toolbar),
                         GCAL_TOOLBAR_OVERVIEW);

  if ((activated_page = gtk_notebook_page_num (GTK_NOTEBOOK (priv->notebook),
                                               priv->views[priv->active_view]))
      != -1)
    {
      gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook),
                                     activated_page);
    }
  else
    {
      //FIXME: there's something that needs to be done here.
      g_warning ("Your app has gone crazy");
    }

  noty = gtk_notification_new ();
  grid = gtk_grid_new ();
  gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
  gtk_container_add (GTK_CONTAINER (grid), gtk_label_new (_("Event deleted")));

  undo_button = gtk_button_new_from_stock (GTK_STOCK_UNDO);
  gtk_container_add (GTK_CONTAINER (grid), undo_button);

  gtk_container_add (GTK_CONTAINER (noty), grid);
  gtk_widget_show_all (noty);
  gcal_window_show_notification (GCAL_WINDOW (user_data), noty);

  g_signal_connect (noty,
                    "dismissed",
                    G_CALLBACK (gcal_window_remove_event),
                    user_data);
  g_signal_connect (undo_button,
                    "clicked",
                    G_CALLBACK (gcal_window_undo_remove_event),
                    user_data);

  priv->event_to_delete = g_strdup (event_uuid);

  /* hide widget of the event */
  event_widget =
    gcal_view_get_by_uuid (GCAL_VIEW (priv->views[priv->active_view]),
                           priv->event_to_delete);
  gtk_widget_hide (event_widget);
}

static void
gcal_window_remove_event (GtkNotification *notification,
                          gpointer         user_data)
{
  GcalWindowPrivate *priv;
  GcalManager *manager;
  gchar **tokens;

  g_return_if_fail (GCAL_IS_WINDOW (user_data));
  priv = GCAL_WINDOW (user_data)->priv;

  if (priv->event_to_delete != NULL)
    {
      manager = gcal_window_get_manager (GCAL_WINDOW (user_data));
      tokens = g_strsplit (priv->event_to_delete, ":", -1);

      gcal_manager_remove_event (manager, tokens[0], tokens[1]);

      g_strfreev (tokens);
      g_free (priv->event_to_delete);
      priv->event_to_delete = NULL;
    }

  clutter_actor_hide (priv->notification_actor);
}

static void
gcal_window_undo_remove_event (GtkButton *button,
                               gpointer   user_data)
{
  GcalWindowPrivate *priv;
  GtkWidget *event_widget;

  g_return_if_fail (GCAL_IS_WINDOW (user_data));
  priv = GCAL_WINDOW (user_data)->priv;

  if (priv->event_to_delete != NULL)
    {
      event_widget = gcal_view_get_by_uuid (
          GCAL_VIEW (priv->views[priv->active_view]),
          priv->event_to_delete);
      gtk_widget_show (event_widget);

      g_free (priv->event_to_delete);
      priv->event_to_delete = NULL;

      gcal_window_hide_notification (GCAL_WINDOW (user_data));
    }

}

/* Public API */
GtkWidget*
gcal_window_new_with_view (GcalApplication *app,
                           GcalWindowViewType view_type)
{
  GcalWindow *win;
  GcalManager *manager;

  win  =  g_object_new (GCAL_TYPE_WINDOW,
                        "application",
                        GTK_APPLICATION (app),
                        NULL);
  gcal_window_set_sources_view (win);

  /* hooking signals */
  manager = gcal_window_get_manager (win);
  g_signal_connect (manager,
                    "events-added",
                    G_CALLBACK (gcal_window_events_added),
                    win);

  g_signal_connect (manager,
                    "events-removed",
                    G_CALLBACK (gcal_window_events_removed),
                    win);

  gcal_toolbar_set_active_view (GCAL_TOOLBAR (win->priv->main_toolbar),
                                view_type);
  return GTK_WIDGET (win);
}

void
gcal_window_show_notification (GcalWindow *window,
                               GtkWidget  *notification)
{
  GcalWindowPrivate *priv;

  g_return_if_fail (GCAL_IS_WINDOW (window));
  priv = window->priv;

  gtk_container_add (
      GTK_CONTAINER (gtk_clutter_actor_get_widget (GTK_CLUTTER_ACTOR (priv->notification_actor))),
      notification);
  gtk_widget_show_all (notification);
  clutter_actor_show (priv->notification_actor);
}

void
gcal_window_hide_notification (GcalWindow *window)
{
  GcalWindowPrivate *priv;
  GtkWidget *noty;

  g_return_if_fail (GCAL_IS_WINDOW (window));
  priv = window->priv;

  noty = gtk_clutter_actor_get_contents (
      GTK_CLUTTER_ACTOR (priv->notification_actor));
  gtk_notification_dismiss (GTK_NOTIFICATION (noty));
}
