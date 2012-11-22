/* -*- mode: c; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#include "gcal-year-view.h"
#include "gcal-month-view.h"
#include "gcal-week-view.h"
#include "gcal-view.h"
#include "gcal-event-widget.h"
#include "gcal-edit-dialog.h"
#include "gcal-enum-types.h"
#include "gtk-notification.h"
#include "gcal-event-overlay.h"

#include <glib/gi18n.h>
#include <clutter/clutter.h>
#include <clutter-gtk/clutter-gtk.h>

#include <libedataserverui/libedataserverui.h>

#include <libical/icaltime.h>

struct _GcalWindowPrivate
{
  ClutterActor        *toolbar_actor;
  ClutterActor        *searchbar_actor;

  ClutterActor        *contents_actor;
  ClutterActor        *notebook_actor;
  ClutterActor        *sources_actor;
  ClutterActor        *notification_actor;

  ClutterActor        *new_event_actor;

  GtkWidget           *notebook;
  GtkWidget           *sources_view;
  GtkWidget           *views [5];
  GtkWidget           *edit_dialog;
  GtkWidget           *edit_widget;

  GcalWindowViewType   active_view;
  icaltimetype        *active_date;

  /* temp to keep the will_delete event uuid */
  gchar               *event_to_delete;

  /* temp to keep event_creation */
  gboolean             waiting_for_creation;
  gboolean             queue_open_edit_dialog;
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

static void           gcal_window_stage_notify_cb        (ClutterActor        *actor,
                                                          GParamSpec          *pspec,
                                                          gpointer             user_data);

static GcalManager*   gcal_window_get_manager            (GcalWindow          *window);

static void           gcal_window_set_active_view        (GcalWindow          *window,
                                                          GcalWindowViewType   view_type);

static void           gcal_window_set_sources_view       (GcalWindow          *window);

static void           gcal_window_init_edit_dialog       (GcalWindow          *window);

static void           gcal_window_view_changed           (GcalToolbar         *toolbar_actor,
                                                          GcalWindowViewType   view_type,
                                                          gpointer             user_data);

static void           gcal_window_sources_shown          (GcalToolbar         *toolbar_actor,
                                                          gboolean             visible,
                                                          gpointer             user_data);

static void           gcal_window_add_event              (GcalToolbar         *toolbar_actor,
                                                          gpointer             user_data);

static void           gcal_window_bring_searchbar        (GcalToolbar         *toolbar_actor,
                                                          gpointer             user_data);

static void           gcal_window_hide_searchbar         (GtkWidget           *button,
                                                          gpointer             user_data);

static void           gcal_window_back_last_view         (GtkWidget           *widget,
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

static void           gcal_window_events_modified        (GcalManager         *manager,
                                                          gpointer             events_list,
                                                          gpointer             user_data);

static void           gcal_window_event_created          (GcalManager         *manager,
                                                          const gchar         *source_uid,
                                                          const gchar         *event_uid,
                                                          gpointer             user_data);

static void           gcal_window_event_activated        (GcalEventWidget     *event_widget,
                                                          gpointer             user_data);

static void           gcal_window_remove_event           (GtkNotification     *notification,
                                                          gpointer             user_data);

static void           gcal_window_undo_remove_event      (GtkButton           *button,
                                                          gpointer             user_data);

static void           gcal_window_view_updated           (GcalView            *view,
                                                          gpointer             date,
                                                          gpointer             user_data);

static void           gcal_window_event_overlay_shown   (GcalView            *view,
                                                         gpointer             start_span,
                                                         gpointer             end_span,
                                                         gdouble              x,
                                                         gdouble              y,
                                                         gpointer             user_data);

static void           gcal_window_event_overlay_closed   (GcalEventOverlay    *widget,
                                                          gpointer             user_data);

static void           gcal_window_create_event           (GcalEventOverlay    *widget,
                                                          GcalNewEventData    *new_data,
                                                          gboolean             open_details,
                                                          gpointer             user_data);

static void           gcal_window_show_hide_actor_cb     (ClutterActor        *actor,
                                                          gchar               *name,
                                                          gboolean             is_finished,
                                                          gpointer             user_data);

static void           gcal_window_edit_dialog_responded  (GtkDialog           *dialog,
                                                          gint                 response,
                                                          gpointer             user_data);

static void           gcal_window_update_event_widget    (GcalManager         *manager,
                                                          const gchar         *source_uid,
                                                          const gchar         *event_uid,
                                                          GcalEventWidget     *widget);

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

  g_object_class_install_property (
      object_class,
      PROP_ACTIVE_VIEW,
      g_param_spec_enum ("active-view",
                         "Active View",
                         "The active view, eg: month, week, etc.",
                         GCAL_WINDOW_VIEW_TYPE,
                         GCAL_WINDOW_VIEW_MONTH,
                         G_PARAM_READWRITE));

  g_object_class_install_property (
      object_class,
      PROP_ACTIVE_DATE,
      g_param_spec_boxed ("active-date",
                          "Date",
                          "The active/selected date",
                          ICAL_TIME_TYPE,
                          G_PARAM_CONSTRUCT |
                          G_PARAM_READWRITE));

  g_type_class_add_private((gpointer)klass, sizeof(GcalWindowPrivate));
}

static void
gcal_window_init(GcalWindow *self)
{
  GcalWindowPrivate *priv;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self,
                                           GCAL_TYPE_WINDOW,
                                           GcalWindowPrivate);
  priv = self->priv;

  priv->event_to_delete = NULL;
  priv->waiting_for_creation = FALSE;
  priv->queue_open_edit_dialog = FALSE;
}

static void
gcal_window_constructed (GObject *object)
{
  GcalWindowPrivate *priv;

  GtkWidget *embed;
  ClutterActor *stage;
  GtkWidget *holder;
  GtkWidget *button;
  GtkWidget *entry;

  GtkStyleContext *context;

  if (G_OBJECT_CLASS (gcal_window_parent_class)->constructed != NULL)
    G_OBJECT_CLASS (gcal_window_parent_class)->constructed (object);

  priv = GCAL_WINDOW (object)->priv;

  /* internal data init */
  priv->event_to_delete = NULL;

  priv->active_date = g_new (icaltimetype, 1);

  /* ui init */
  embed = gtk_clutter_embed_new ();
  gtk_container_add (GTK_CONTAINER (object), embed);
  stage = gtk_clutter_embed_get_stage (GTK_CLUTTER_EMBED (embed));

  /* toolbar_actor */
  priv->toolbar_actor = gcal_toolbar_new ();
  clutter_actor_add_child (stage, priv->toolbar_actor);
  clutter_actor_set_position (priv->toolbar_actor, 0, 0);
  clutter_actor_add_constraint_with_name (
      priv->toolbar_actor,
      "width-bind",
      clutter_bind_constraint_new (stage,
                                   CLUTTER_BIND_WIDTH,
                                   0.0));

  /* searchbar_actor */
  /* FIXME: demo code */
  holder = gtk_grid_new ();
  button = gtk_button_new_with_label (_("Done"));
  gtk_widget_set_size_request (button, 100, -1);
  gtk_widget_set_hexpand (button, FALSE);
  gtk_widget_set_halign (button, GTK_ALIGN_START);
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  gtk_style_context_add_class (
      gtk_widget_get_style_context (button),
      "suggested-action");
  gtk_style_context_add_class (
      gtk_widget_get_style_context (button),
      "toolbar-button");

  entry = gtk_search_entry_new ();
  gtk_entry_set_placeholder_text (GTK_ENTRY (entry), _("Search ..."));
  gtk_widget_set_hexpand (entry, TRUE);
  gtk_widget_set_vexpand (entry, TRUE);
  gtk_widget_set_halign (entry, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (entry, GTK_ALIGN_CENTER);
  gtk_widget_set_size_request (entry, 450, -1);

  gtk_container_set_border_width (GTK_CONTAINER (holder), 12);
  gtk_container_add (GTK_CONTAINER (holder), button);
  gtk_container_add (GTK_CONTAINER (holder), entry);
  gtk_widget_show_all (holder);

  priv->searchbar_actor = gtk_clutter_actor_new_with_contents (holder);
  clutter_actor_add_child (stage, priv->searchbar_actor);
  clutter_actor_set_x (priv->searchbar_actor, 0);
  clutter_actor_add_constraint_with_name (
      priv->searchbar_actor,
      "width-bind",
      clutter_bind_constraint_new (stage,
                                   CLUTTER_BIND_WIDTH,
                                   0.0));
  clutter_actor_add_constraint_with_name (
      priv->searchbar_actor,
      "height-bind",
      clutter_bind_constraint_new (priv->toolbar_actor,
                                   CLUTTER_BIND_HEIGHT,
                                   0.0));
  clutter_actor_add_constraint_with_name (
      priv->searchbar_actor,
      "bottom-snap",
      clutter_snap_constraint_new (priv->toolbar_actor,
                                   CLUTTER_SNAP_EDGE_BOTTOM,
                                   CLUTTER_SNAP_EDGE_TOP,
                                   0.0));
  /* contents_actor */
  priv->contents_actor = clutter_actor_new ();
  clutter_actor_set_layout_manager (
      priv->contents_actor,
      clutter_bin_layout_new (CLUTTER_BIN_ALIGNMENT_CENTER,
                              CLUTTER_BIN_ALIGNMENT_CENTER));
  clutter_actor_set_x (priv->contents_actor, 0);
  clutter_actor_add_constraint_with_name (
      priv->contents_actor,
      "width-bind",
      clutter_bind_constraint_new (stage,
                                   CLUTTER_BIND_WIDTH,
                                   0.0));
  clutter_actor_add_constraint_with_name (
      priv->contents_actor,
      "y-bind",
      clutter_bind_constraint_new (
          stage,
          CLUTTER_BIND_Y,
          0.0));
  clutter_actor_add_constraint_with_name (
      priv->contents_actor,
      "bottom-snap",
      clutter_snap_constraint_new (stage,
                                   CLUTTER_SNAP_EDGE_BOTTOM,
                                   CLUTTER_SNAP_EDGE_BOTTOM,
                                   0.0));
  clutter_actor_add_child (stage, priv->contents_actor);


  /* notebook widget for holding views */
  holder = gtk_frame_new (NULL);

  priv->notebook = gtk_notebook_new ();
  gtk_notebook_set_show_tabs (GTK_NOTEBOOK (priv->notebook), FALSE);
  gtk_notebook_set_show_border (GTK_NOTEBOOK (priv->notebook), FALSE);

  gtk_widget_show (priv->notebook);

  gtk_container_add (GTK_CONTAINER (holder), priv->notebook);
  gtk_widget_show (holder);

  priv->notebook_actor = gtk_clutter_actor_new_with_contents (holder);
  g_object_set (priv->notebook_actor,
                "x-expand", TRUE,
                "y-expand", TRUE,
                "x-align", CLUTTER_ACTOR_ALIGN_FILL,
                "y-align", CLUTTER_ACTOR_ALIGN_FILL,
                NULL);
  clutter_actor_add_child (priv->contents_actor, priv->notebook_actor);

  context =
    gtk_widget_get_style_context (
        gtk_clutter_actor_get_widget (
          GTK_CLUTTER_ACTOR (priv->notebook_actor)));
  gtk_style_context_add_class (context, "contents");

  /* sources view */
  holder = gtk_frame_new (NULL);
  gtk_widget_show (holder);

  priv->sources_actor = gtk_clutter_actor_new_with_contents (holder);
  g_object_set (priv->sources_actor,
                "margin-right", 10.0,
                "margin-top", 10.0,
                "opacity", 0,
                "x-expand", TRUE,
                "y-expand", TRUE,
                "x-align", CLUTTER_ACTOR_ALIGN_END,
                "y-align", CLUTTER_ACTOR_ALIGN_START,
                NULL);
  clutter_actor_add_child (priv->contents_actor, priv->sources_actor);

  clutter_actor_hide (priv->sources_actor);
  clutter_actor_set_background_color (
      priv->sources_actor,
      clutter_color_get_static (CLUTTER_COLOR_ALUMINIUM_2));

  context =
    gtk_widget_get_style_context (
        gtk_clutter_actor_get_widget (
          GTK_CLUTTER_ACTOR (priv->sources_actor)));
  gtk_style_context_add_class (context, "overlay");

  /* notifications manager */
  priv->notification_actor = gtk_clutter_actor_new ();
  g_object_set (priv->notification_actor,
                "x-expand", TRUE,
                "y-expand", TRUE,
                "x-align", CLUTTER_ACTOR_ALIGN_CENTER,
                "y-align", CLUTTER_ACTOR_ALIGN_START,
                NULL);
  clutter_actor_add_child (priv->contents_actor, priv->notification_actor);

  context =
    gtk_widget_get_style_context (
        gtk_clutter_actor_get_widget (
          GTK_CLUTTER_ACTOR (priv->notification_actor)));
  gtk_style_context_add_class (context, "overlay");
  clutter_actor_hide (priv->notification_actor);

  /* event-overlay */
  priv->new_event_actor = gcal_event_overlay_new ();
  g_object_set (priv->new_event_actor,
                "opacity", 0,
                "x-align", CLUTTER_ACTOR_ALIGN_FILL,
                "y-align", CLUTTER_ACTOR_ALIGN_FILL,
                NULL);

  context =
    gtk_widget_get_style_context (
        gtk_clutter_actor_get_widget (
          GTK_CLUTTER_ACTOR (priv->new_event_actor)));
  gtk_style_context_add_class (context, "overlay");

  clutter_actor_add_child (priv->contents_actor, priv->new_event_actor);
  clutter_actor_hide (priv->new_event_actor);

  /* signals connection/handling */
  g_signal_connect (stage,
                    "notify::allocation",
                    G_CALLBACK (gcal_window_stage_notify_cb),
                    object);
  g_signal_connect (button,
                    "clicked",
                    G_CALLBACK (gcal_window_hide_searchbar),
                    object);

  g_signal_connect (priv->toolbar_actor,
                    "view-changed",
                    G_CALLBACK (gcal_window_view_changed),
                    object);
  g_signal_connect (priv->toolbar_actor,
                    "sources-shown",
                    G_CALLBACK (gcal_window_sources_shown),
                    object);
  g_signal_connect (priv->toolbar_actor,
                    "add-event",
                    G_CALLBACK (gcal_window_add_event),
                    object);
  g_signal_connect (priv->toolbar_actor,
                    "search-events",
                    G_CALLBACK (gcal_window_bring_searchbar),
                    object);

  g_signal_connect (priv->toolbar_actor,
                    "back",
                    G_CALLBACK (gcal_window_back_last_view),
                    object);

  g_signal_connect (priv->new_event_actor,
                    "cancelled",
                    G_CALLBACK (gcal_window_event_overlay_closed),
                    object);

  g_signal_connect (priv->new_event_actor,
                    "created",
                    G_CALLBACK (gcal_window_create_event),
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

static void
gcal_window_stage_notify_cb (ClutterActor        *actor,
                             GParamSpec          *pspec,
                             gpointer             user_data)
{
  GcalWindowPrivate *priv;
  ClutterConstraint *constraint;

  priv = GCAL_WINDOW (user_data)->priv;

  /* Updating constraints */
  constraint = clutter_actor_get_constraint (priv->contents_actor,
                                             "y-bind");
  clutter_bind_constraint_set_offset (
      CLUTTER_BIND_CONSTRAINT (constraint),
      clutter_actor_get_height (priv->toolbar_actor));
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
        case GCAL_WINDOW_VIEW_YEAR:
          priv->views[GCAL_WINDOW_VIEW_YEAR] = gcal_year_view_new ();
          break;
        default:
            g_debug ("Unimplemented view yet");
            return;
        }

      /* Bindings properties and signals */
      g_object_bind_property (priv->views[view_type], "active-date",
                              window, "active-date",
                              G_BINDING_DEFAULT);

      g_signal_connect (priv->views[view_type],
                        "create-event",
                        G_CALLBACK (gcal_window_event_overlay_shown),
                        window);

      g_signal_connect (priv->views[view_type],
                        "updated",
                        G_CALLBACK (gcal_window_view_updated),
                        window);

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

  gcal_view_set_date (GCAL_VIEW (priv->views[priv->active_view]),
                      priv->active_date);
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
gcal_window_init_edit_dialog (GcalWindow *window)
{
  GcalWindowPrivate *priv;

  g_return_if_fail (GCAL_IS_WINDOW (window));
  priv = window->priv;

  priv->edit_dialog = gcal_edit_dialog_new ();
  gtk_window_set_transient_for (GTK_WINDOW (priv->edit_dialog),
                                GTK_WINDOW (window));
  gcal_edit_dialog_set_manager (GCAL_EDIT_DIALOG (priv->edit_dialog),
                                gcal_window_get_manager (window));

  g_signal_connect (priv->edit_dialog,
                    "response",
                    G_CALLBACK (gcal_window_edit_dialog_responded),
                    window);
}

static void
gcal_window_view_changed (GcalToolbar         *toolbar_actor,
                          GcalWindowViewType   view_type,
                          gpointer             user_data)
{
  GcalWindowPrivate *priv;

  priv = GCAL_WINDOW (user_data)->priv;
  priv->active_view = view_type;

  gcal_window_set_active_view (GCAL_WINDOW (user_data), view_type);
}

static void
gcal_window_sources_shown (GcalToolbar *toolbar_actor,
                           gboolean     visible,
                           gpointer     user_data)
{
  GcalWindowPrivate *priv;
  priv  = GCAL_WINDOW (user_data)->priv;

  if (visible)
    {
      clutter_actor_show (priv->sources_actor);
      clutter_actor_save_easing_state (priv->sources_actor);
      clutter_actor_set_opacity (priv->sources_actor, 255);
      clutter_actor_restore_easing_state (priv->sources_actor);
    }
  else
    {
      clutter_actor_save_easing_state (priv->sources_actor);
      clutter_actor_set_opacity (priv->sources_actor, 0);
      clutter_actor_restore_easing_state (priv->sources_actor);

      g_signal_connect (priv->sources_actor,
                        "transition-stopped::opacity",
                        G_CALLBACK (gcal_window_show_hide_actor_cb),
                        user_data);
    }
}

static void
gcal_window_add_event (GcalToolbar *toolbar_actor,
                       gpointer     user_data)
{
  g_debug ("Create new-event here");
  /* FIXME: bring new-event-overlay up front on the actual unit */
}

static void
gcal_window_bring_searchbar (GcalToolbar *toolbar_actor,
                             gpointer     user_data)
{
  GcalWindowPrivate *priv;

  priv = GCAL_WINDOW (user_data)->priv;

  clutter_actor_save_easing_state (priv->toolbar_actor);
  clutter_actor_set_y (priv->toolbar_actor,
                       clutter_actor_get_height (priv->toolbar_actor));
  clutter_actor_set_easing_duration (priv->toolbar_actor, 500);
  clutter_actor_restore_easing_state (priv->toolbar_actor);
}

static void
gcal_window_hide_searchbar (GtkWidget *button,
                            gpointer   user_data)
{
  GcalWindowPrivate *priv;

  priv = GCAL_WINDOW (user_data)->priv;

  clutter_actor_save_easing_state (priv->toolbar_actor);
  clutter_actor_set_y (priv->toolbar_actor,
                       0);
  clutter_actor_set_easing_duration (priv->toolbar_actor, 500);
  clutter_actor_restore_easing_state (priv->toolbar_actor);
  /*clutter_actor_save_easing_state (priv->searchbar_actor);*/
  /*clutter_actor_set_y (priv->searchbar_actor, 0);*/
  /*clutter_actor_set_easing_duration (priv->searchbar_actor, 500);*/
  /*clutter_actor_restore_easing_state (priv->searchbar_actor);*/
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

  gcal_toolbar_set_mode (GCAL_TOOLBAR (priv->toolbar_actor),
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
      /* FIXME: there's something that needs to be done here. */
      g_warning ("Your app has gone crazy");
    }
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
                          gpointer     events_list,
                          gpointer     user_data)
{
  GcalWindowPrivate *priv;
  GSList *l;

  gchar **tokens;
  gchar *source_uid;
  gchar *event_uid;

  GtkWidget *event;
  icaltimetype *starting_date;

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
            starting_date) &&
          gcal_view_get_by_uuid (
            GCAL_VIEW (priv->views[priv->active_view]),
            (gchar*)l->data) == NULL)
        {
          event = gcal_event_widget_new ((gchar*) l->data);

          gcal_window_update_event_widget (manager,
                                           source_uid,
                                           event_uid,
                                           GCAL_EVENT_WIDGET (event));
          gtk_widget_show (event);

          /* FIXME: add event-widget to every instantiated view, not pretty sure
           * about this. */
          gtk_container_add (
              GTK_CONTAINER (priv->views[priv->active_view]),
              event);

          g_signal_connect (event,
                            "activate",
                            G_CALLBACK (gcal_window_event_activated),
                            user_data);
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
gcal_window_events_modified (GcalManager *manager,
                             gpointer     events_list,
                             gpointer     user_data)
{
  GcalWindowPrivate *priv;
  GSList *l;

  gchar **tokens;
  gchar *source_uid;
  gchar *event_uid;

  GtkWidget *widget;

  priv = GCAL_WINDOW (user_data)->priv;

  for (l = events_list; l != NULL; l = l->next)
    {
      gint i;

      tokens = g_strsplit ((gchar*) l->data, ":", -1);
      source_uid  = tokens[0];
      event_uid = tokens[1];

      g_debug ("Object modified from UI on calendar: %s event: %s",
               source_uid,
               event_uid);

      for (i = 0; i < 5; i++)
        {
          if (priv->views[i] != NULL)
            {
              widget = gcal_view_get_by_uuid (GCAL_VIEW (priv->views[i]),
                                              (gchar*) l->data);
              gcal_window_update_event_widget (manager,
                                               source_uid,
                                               event_uid,
                                               GCAL_EVENT_WIDGET (widget));
              gcal_view_reposition_child (GCAL_VIEW (priv->views[i]),
                                          (gchar*) l->data);
            }
        }

      g_strfreev (tokens);
    }
}

static void
gcal_window_event_created (GcalManager *manager,
                           const gchar *source_uid,
                           const gchar *event_uid,
                           gpointer     user_data)
{
  GcalWindowPrivate *priv;

  priv = GCAL_WINDOW (user_data)->priv;

  if (! priv->waiting_for_creation)
    return;

  priv->waiting_for_creation = FALSE;

  if (priv->edit_dialog == NULL)
    gcal_window_init_edit_dialog (GCAL_WINDOW (user_data));

  gcal_edit_dialog_set_event (GCAL_EDIT_DIALOG (priv->edit_dialog),
                              source_uid,
                              event_uid);

  if (CLUTTER_ACTOR_IS_VISIBLE (priv->new_event_actor))
    {
      priv->queue_open_edit_dialog = TRUE;
      return;
    }

  gtk_dialog_run (GTK_DIALOG (priv->edit_dialog));
}

static void
gcal_window_event_activated (GcalEventWidget *event_widget,
                             gpointer         user_data)
{
  GcalWindowPrivate *priv;
  gchar **tokens;

  g_return_if_fail (GCAL_IS_WINDOW (user_data));
  priv = GCAL_WINDOW (user_data)->priv;

  if (priv->edit_dialog == NULL)
    gcal_window_init_edit_dialog (GCAL_WINDOW (user_data));

  tokens = g_strsplit (gcal_event_widget_peek_uuid (event_widget), ":", -1);
  gcal_edit_dialog_set_event (GCAL_EDIT_DIALOG (priv->edit_dialog),
                              tokens[0],
                              tokens[1]);
  g_strfreev (tokens);

  gtk_dialog_run (GTK_DIALOG (priv->edit_dialog));
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

static void
gcal_window_view_updated (GcalView *view,
                          gpointer  date,
                          gpointer  user_data)
{
  GcalWindowPrivate *priv;
  icaltimetype *first_day;
  icaltimetype *last_day;

  priv = GCAL_WINDOW (user_data)->priv;

  gcal_view_set_date (GCAL_VIEW (priv->views[priv->active_view]),
                      date);

  first_day = gcal_view_get_initial_date (
      GCAL_VIEW (priv->views[priv->active_view]));
  last_day = gcal_view_get_final_date (
          GCAL_VIEW (priv->views[priv->active_view]));

  gcal_manager_set_new_range (
      gcal_window_get_manager (GCAL_WINDOW (user_data)),
      first_day,
      last_day);

  g_free (first_day);
  g_free (last_day);
}

static void
gcal_window_event_overlay_shown (GcalView *view,
                                 gpointer  start_span,
                                 gpointer  end_span,
                                 gdouble   x,
                                 gdouble   y,
                                 gpointer  user_data)
{
  GcalWindowPrivate *priv;
  GcalManager *manager;
  gint width, height;

  g_return_if_fail (user_data);
  priv = GCAL_WINDOW (user_data)->priv;

  manager = gcal_window_get_manager (GCAL_WINDOW (user_data));
  /* squeezed in here, reload on every show */
  gcal_event_overlay_set_sources_model (
      GCAL_EVENT_OVERLAY (priv->new_event_actor),
      gcal_manager_get_sources_model (manager));

  if (start_span != NULL)
    {
      gcal_event_overlay_set_span (GCAL_EVENT_OVERLAY (priv->new_event_actor),
                                   (icaltimetype*) start_span,
                                   (icaltimetype*) end_span);
    }

  clutter_actor_show (priv->new_event_actor);

  width = clutter_actor_get_width (priv->new_event_actor);
  height = clutter_actor_get_height (priv->new_event_actor);

  x = x - width / 2;
  y = y - height;

  clutter_actor_set_x (priv->new_event_actor, x);
  clutter_actor_set_y (priv->new_event_actor, y);

  clutter_actor_save_easing_state (priv->new_event_actor);
  clutter_actor_set_opacity (priv->new_event_actor, 255);
  clutter_actor_restore_easing_state (priv->new_event_actor);
}

static void
gcal_window_event_overlay_closed (GcalEventOverlay *widget,
                                  gpointer          user_data)
{
  GcalWindowPrivate *priv;

  g_return_if_fail (user_data);
  priv = GCAL_WINDOW (user_data)->priv;

  /* reset and hide */
  gcal_view_clear_selection (GCAL_VIEW (priv->views[priv->active_view]));
  gcal_event_overlay_reset (GCAL_EVENT_OVERLAY (priv->new_event_actor));

  clutter_actor_save_easing_state (priv->new_event_actor);
  clutter_actor_set_opacity (priv->new_event_actor, 0);
  clutter_actor_restore_easing_state (priv->new_event_actor);

  g_signal_connect (priv->new_event_actor,
                    "transition-stopped::opacity",
                    G_CALLBACK (gcal_window_show_hide_actor_cb),
                    user_data);
}

static void
gcal_window_create_event (GcalEventOverlay    *widget,
                          GcalNewEventData    *new_data,
                          gboolean             open_details,
                          gpointer             user_data)
{
  GcalWindowPrivate *priv;
  GcalManager *manager;

  priv = GCAL_WINDOW (user_data)->priv;

  if (open_details)
    {
      priv->waiting_for_creation = TRUE;
    }

  /* create the event */
  manager = gcal_window_get_manager (GCAL_WINDOW (user_data));
  gcal_manager_create_event (manager,
                             new_data->calendar_uid,
                             new_data->summary,
                             new_data->start_date,
                             new_data->end_date);

  /* reset and hide */
  gcal_window_event_overlay_closed (widget, user_data);
}

static void
gcal_window_show_hide_actor_cb (ClutterActor *actor,
                                gchar        *name,
                                gboolean      is_finished,
                                gpointer      user_data)
{
  GcalWindowPrivate *priv;

  priv = GCAL_WINDOW (user_data)->priv;

  if (CLUTTER_ACTOR_IS_VISIBLE (actor) &&
      clutter_actor_get_opacity (actor) == 0)
    clutter_actor_hide (actor);

  if (priv->queue_open_edit_dialog)
    {
      gtk_widget_show_all (priv->edit_dialog);
      priv->queue_open_edit_dialog = FALSE;
    }

  /* disconnect so we don't get multiple notifications */
  g_signal_handlers_disconnect_by_func (actor,
                                        gcal_window_show_hide_actor_cb,
                                        NULL);
}

static void
gcal_window_edit_dialog_responded (GtkDialog *dialog,
                                   gint       response,
                                   gpointer   user_data)
{
  GcalWindowPrivate *priv;

  GcalManager *manager;
  GList *changed_props;
  GList *l;

  GtkWidget *event_widget;
  GtkWidget *noty;
  GtkWidget *grid;
  GtkWidget *undo_button;

  priv = GCAL_WINDOW (user_data)->priv;

  gtk_widget_hide (priv->edit_dialog);

  switch (response)
    {
      case GTK_RESPONSE_CANCEL:
        /* do nothing, nor edit, nor nothing */
        break;
      case GTK_RESPONSE_ACCEPT:
        /* save changes if editable */
        manager = gcal_window_get_manager (GCAL_WINDOW (user_data));
        changed_props =
          gcal_edit_dialog_get_modified_properties (GCAL_EDIT_DIALOG (dialog));
        for (l = changed_props; l != NULL; l = l->next)
          {
            EventEditableProperty prop = (EventEditableProperty) l->data;

            switch (prop)
              {
              case EVENT_SUMMARY:
                g_debug ("Will change summary");
                gcal_manager_set_event_summary (
                    manager,
                    gcal_edit_dialog_peek_source_uid (GCAL_EDIT_DIALOG (dialog)),
                    gcal_edit_dialog_peek_event_uid (GCAL_EDIT_DIALOG (dialog)),
                    gcal_edit_dialog_peek_summary (GCAL_EDIT_DIALOG (dialog)));
                break;
              case EVENT_START_DATE:
                {
                  icaltimetype *date;

                  date = gcal_edit_dialog_get_start_date (GCAL_EDIT_DIALOG (dialog));

                  g_debug ("Will change start_date");
                  gcal_manager_set_event_start_date (
                       manager,
                       gcal_edit_dialog_peek_source_uid (GCAL_EDIT_DIALOG (dialog)),
                       gcal_edit_dialog_peek_event_uid (GCAL_EDIT_DIALOG (dialog)),
                       date);
                  g_free (date);
                  break;
                }
              case EVENT_END_DATE:
                {
                  icaltimetype *date;

                  date = gcal_edit_dialog_get_end_date (GCAL_EDIT_DIALOG (dialog));

                  g_debug ("Will change end_date");
                  gcal_manager_set_event_end_date (
                       manager,
                       gcal_edit_dialog_peek_source_uid (GCAL_EDIT_DIALOG (dialog)),
                       gcal_edit_dialog_peek_event_uid (GCAL_EDIT_DIALOG (dialog)),
                       date);
                  g_free (date);
                  break;
                }
              case EVENT_LOCATION:
                g_debug ("Will change location");
                gcal_manager_set_event_location (
                    manager,
                    gcal_edit_dialog_peek_source_uid (GCAL_EDIT_DIALOG (dialog)),
                    gcal_edit_dialog_peek_event_uid (GCAL_EDIT_DIALOG (dialog)),
                    gcal_edit_dialog_peek_location (GCAL_EDIT_DIALOG (dialog)));
                break;
              case EVENT_DESCRIPTION:
                {
                  gchar *new_desc;

                  new_desc = gcal_edit_dialog_get_event_description (GCAL_EDIT_DIALOG (dialog));
                  gcal_manager_set_event_description (
                       manager,
                       gcal_edit_dialog_peek_source_uid (GCAL_EDIT_DIALOG (dialog)),
                       gcal_edit_dialog_peek_event_uid (GCAL_EDIT_DIALOG (dialog)),
                       new_desc);
                  g_free (new_desc);
                  break;
                }
              case EVENT_SOURCE:
                {
                  gchar *new_uid;

                  new_uid = gcal_edit_dialog_get_new_source_uid (GCAL_EDIT_DIALOG (dialog));
                  gcal_manager_move_event_to_source (
                       manager,
                       gcal_edit_dialog_peek_source_uid (GCAL_EDIT_DIALOG (dialog)),
                       gcal_edit_dialog_peek_event_uid (GCAL_EDIT_DIALOG (dialog)),
                       new_uid);
                  g_free (new_uid);
                  break;
                }
              default:
                break;
              }
          }

        break;
      case GCAL_RESPONSE_DELETE_EVENT:
        /* delete the event */
        noty = gtk_notification_new ();
        grid = gtk_grid_new ();
        gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
        gtk_container_add (GTK_CONTAINER (grid),
                           gtk_label_new (_("Event deleted")));

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

        priv->event_to_delete =
          gcal_edit_dialog_get_event_uuid (GCAL_EDIT_DIALOG (priv->edit_dialog));

        event_widget =
          gcal_view_get_by_uuid (GCAL_VIEW (priv->views[priv->active_view]),
                                 priv->event_to_delete);

        /* hide widget of the event */
        gtk_widget_hide (GTK_WIDGET (event_widget));
        break;

      default:
        /* do nothing */
        break;
    }
}

static void
gcal_window_update_event_widget (GcalManager     *manager,
                                 const gchar     *source_uid,
                                 const gchar     *event_uid,
                                 GcalEventWidget *widget)
{
  gchar *summary;
  GdkRGBA *color;
  icaltimetype *starting_date;

  summary = gcal_manager_get_event_summary (manager,
                                            source_uid,
                                            event_uid);

  color = gcal_manager_get_event_color (manager,
                                        source_uid,
                                        event_uid);
  starting_date = gcal_manager_get_event_start_date (manager,
                                                     source_uid,
                                                     event_uid);
  gcal_event_widget_set_summary (widget, summary);
  gcal_event_widget_set_color (widget, color);
  gcal_event_widget_set_date (widget, starting_date);
  gcal_event_widget_set_all_day (
      widget,
      gcal_manager_get_event_all_day (manager, source_uid, event_uid));
  gcal_event_widget_set_has_reminders (
      widget,
      gcal_manager_has_event_reminders (manager, source_uid, event_uid));

  g_free (summary);
  gdk_rgba_free (color);
  g_free (starting_date);
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

  manager = gcal_window_get_manager (win);

  /* FIXME: here read the initial date from somewehere */
  *(win->priv->active_date) =
    icaltime_current_time_with_zone (gcal_manager_get_system_timezone (manager));

  /* hooking signals */
  g_signal_connect (manager,
                    "events-added",
                    G_CALLBACK (gcal_window_events_added),
                    win);

  g_signal_connect (manager,
                    "events-removed",
                    G_CALLBACK (gcal_window_events_removed),
                    win);

  g_signal_connect (manager,
                    "events-modified",
                    G_CALLBACK (gcal_window_events_modified),
                    win);

  g_signal_connect (manager,
                    "event-created",
                    G_CALLBACK (gcal_window_event_created),
                    win);

  gcal_toolbar_set_active_view (GCAL_TOOLBAR (win->priv->toolbar_actor),
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
