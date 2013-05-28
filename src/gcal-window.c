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

#include "e-cell-renderer-color.h"
#include "gcal-window.h"
#include "gcal-manager.h"
#include "gcal-floating-container.h"
#include "gcal-year-view.h"
#include "gcal-month-view.h"
#include "gcal-week-view.h"
#include "gcal-view.h"
#include "gcal-event-widget.h"
#include "gcal-edit-dialog.h"
#include "gcal-enum-types.h"
#include "gcal-event-overlay.h"

#include <libgd/gd.h>

#include <glib/gi18n.h>
#include <clutter/clutter.h>

#include <libical/icaltime.h>

struct _GcalWindowPrivate
{
  /* FIXME: For building: TO REMOVE */
  ClutterActor        *new_event_actor;

  /* upper level widgets */
  GtkWidget           *main_box;

  GtkWidget           *header_bar;
  GtkWidget           *search_bar;
  GtkWidget           *views_overlay;
  GtkWidget           *views_stack;
  GtkWidget           *noty; /* short-lived */

  /* header_bar widets */
  GtkWidget           *new_button;
  GtkWidget           *search_entry;
  GtkWidget           *views_switcher;

  GtkWidget           *sources_view;
  GtkWidget           *views [5];
  GtkWidget           *edit_dialog;

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

static void           gcal_window_search_toggled         (GObject             *object,
                                                          GParamSpec          *pspec,
                                                          gpointer             user_data);

static void           gcal_window_search_changed         (GtkEditable         *editable,
                                                          gpointer             user_data);

static void           gcal_window_set_active_view        (GcalWindow          *window,
                                                          GcalWindowViewType   view_type);

static GcalManager*   gcal_window_get_manager            (GcalWindow          *window);

static void           gcal_window_set_sources_view       (GcalWindow          *window);

static void           gcal_window_init_edit_dialog       (GcalWindow          *window);

static void           gcal_window_add_event              (GtkButton           *new_button,
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

static void           gcal_window_remove_event           (GdNotification      *notification,
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

  g_type_class_add_private((gpointer)klass, sizeof (GcalWindowPrivate));
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

  GtkWidget *box;
  GtkWidget *search_button;
  GtkWidget *menu_button;

  if (G_OBJECT_CLASS (gcal_window_parent_class)->constructed != NULL)
    G_OBJECT_CLASS (gcal_window_parent_class)->constructed (object);

  priv = GCAL_WINDOW (object)->priv;

  /* internal data init */
  priv->active_date = g_new (icaltimetype, 1);

  /* ui construction */
  priv->main_box = gtk_grid_new ();
  gtk_orientable_set_orientation (GTK_ORIENTABLE (priv->main_box),
                                  GTK_ORIENTATION_VERTICAL);

  /* header_bar */
  priv->header_bar = gtk_header_bar_new ();

  /* header_bar: new */
  priv->new_button = gd_header_simple_button_new ();
  gd_header_button_set_label (GD_HEADER_BUTTON (priv->new_button),
                              _("New Event"));
  gtk_style_context_add_class (
      gtk_widget_get_style_context (priv->new_button),
      "suggested-action");
  /* FIXME: gtk_actionable_set_action_name (GTK_ACTIONABLE (forward_button), "win.new-event"); */
  gtk_header_bar_pack_start (GTK_HEADER_BAR (priv->header_bar), priv->new_button);

  /* header_bar: views. Temporarily, since this will be made of GdStackSwitcher */
  priv->views_switcher = gtk_image_new_from_icon_name ("face-wink-symbolic", GTK_ICON_SIZE_MENU);
  g_object_ref_sink (priv->views_switcher);
  gtk_header_bar_set_custom_title (GTK_HEADER_BAR (priv->header_bar),
                                   priv->views_switcher);

  /* header_bar: search */
  search_button = gd_header_toggle_button_new ();
  gd_header_button_set_symbolic_icon_name (GD_HEADER_BUTTON (search_button),
                                           "edit-find-symbolic");
  gtk_header_bar_pack_end (GTK_HEADER_BAR (priv->header_bar), search_button);

  /* header_bar: menu */
  menu_button = gd_header_menu_button_new ();
  gd_header_button_set_label (GD_HEADER_BUTTON (menu_button),
                              _("Settings"));
  gd_header_button_set_symbolic_icon_name (GD_HEADER_BUTTON (menu_button),
                                           "emblem-system-symbolic");
  gtk_header_bar_pack_end (GTK_HEADER_BAR (priv->header_bar), menu_button);

  gtk_widget_set_hexpand (priv->header_bar, TRUE);

  gtk_window_set_titlebar (GTK_WINDOW (object), priv->header_bar);

  /* search_bar */
  priv->search_entry = gtk_search_entry_new ();
  g_object_set (priv->search_entry,
                "width-request", 500,
                "hexpand", TRUE,
                "halign", GTK_ALIGN_CENTER,
                NULL);

  box = gtk_grid_new ();
  gtk_container_add (GTK_CONTAINER (box), priv->search_entry);

  priv->search_bar = gtk_search_bar_new (GTK_ENTRY (priv->search_entry));
  gtk_widget_set_hexpand (priv->search_bar, TRUE);
  g_object_bind_property (search_button, "active",
                          priv->search_bar, "search-mode",
                          G_BINDING_BIDIRECTIONAL);
  gtk_container_add (GTK_CONTAINER (priv->search_bar), box);
  gtk_container_add (GTK_CONTAINER (priv->main_box), priv->search_bar);

  /* overlay */
  priv->views_overlay = gtk_overlay_new ();
  gtk_widget_set_hexpand (priv->views_overlay, TRUE);
  gtk_widget_set_vexpand (priv->views_overlay, TRUE);
  gtk_container_add (GTK_CONTAINER (priv->main_box), priv->views_overlay);

  /* stack widget for holding views */
  priv->views_stack = gtk_stack_new ();
  gtk_widget_set_vexpand (priv->views_stack, TRUE);
  gtk_widget_set_hexpand (priv->views_stack, TRUE);
  gtk_container_add (GTK_CONTAINER (priv->views_overlay), priv->views_stack);

  gtk_style_context_add_class (
      gtk_widget_get_style_context (priv->views_stack),
      "views");

  /* signals connection/handling */
  g_signal_connect (priv->new_button, "clicked",
                    G_CALLBACK (gcal_window_add_event), object);
  g_signal_connect (priv->search_bar, "notify::search-mode",
                    G_CALLBACK (gcal_window_search_toggled), object);
  g_signal_connect (priv->search_entry, "changed",
                    G_CALLBACK (gcal_window_search_changed), object);

  gtk_container_add (GTK_CONTAINER (object), priv->main_box);
  gtk_widget_show_all (priv->main_box);
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
gcal_window_search_toggled (GObject    *object,
                            GParamSpec *pspec,
                            gpointer    user_data)
{
  GcalWindowPrivate *priv;
  gboolean search_mode;

  priv = GCAL_WINDOW (user_data)->priv;

  g_object_get (object, "search-mode", &search_mode, NULL);
  if (search_mode)
    {
      g_debug ("Entering search mode");

      /* update headder_bar widget */
      gtk_widget_hide (priv->new_button);
      gtk_header_bar_set_custom_title (GTK_HEADER_BAR (priv->header_bar),
                                       NULL);
      /* _prepare_for_search */
    }
  else
    {
      g_debug ("Leaving search mode");
      /* update header_bar */
      gtk_widget_show (priv->new_button);
      gtk_header_bar_set_custom_title (GTK_HEADER_BAR (priv->header_bar),
                                       priv->views_switcher);
      /* return to last active_view */
      gtk_stack_set_visible_child (GTK_STACK (priv->views_stack),
                                   priv->views[priv->active_view]);
    }
}

static void
gcal_window_search_changed (GtkEditable *editable,
                            gpointer     user_data)
{
  GcalWindowPrivate *priv;
  gboolean search_mode;

  priv = GCAL_WINDOW (user_data)->priv;

  g_object_get (priv->search_bar, "search-mode", &search_mode, NULL);
  if (search_mode)
    {
      GtkWidget *entry;
      gchar *title;

      entry = gtk_search_bar_get_entry (GTK_SEARCH_BAR (priv->search_bar));

      if (gtk_entry_get_text_length (GTK_ENTRY (entry)) != 0)
        {
          title = g_strdup_printf ("Results for \"%s\"",
                                   gtk_entry_get_text (GTK_ENTRY (entry)));
          gtk_header_bar_set_title (GTK_HEADER_BAR (priv->header_bar),
                                    title);
        }
      else
        {
          gtk_header_bar_set_title (GTK_HEADER_BAR (priv->header_bar),
                                    "");
        }
    }
}

static void
gcal_window_set_active_view (GcalWindow         *window,
                             GcalWindowViewType  view_type)
{
  GcalWindowPrivate *priv;

  gboolean update_range;
  icaltimetype *first_day;
  icaltimetype *last_day;

  g_return_if_fail (GCAL_IS_WINDOW (window));
  priv = window->priv;

  if (priv->views[view_type] != NULL)
    {
      gtk_stack_set_visible_child (GTK_STACK (priv->views_stack),
                                   priv->views[view_type]);
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
      gtk_container_add (GTK_CONTAINER (priv->views_stack),
                         priv->views[view_type]);
      gtk_stack_set_visible_child (GTK_STACK (priv->views_stack),
                                   priv->views[view_type]);
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

static GcalManager*
gcal_window_get_manager (GcalWindow *window)
{
  GcalApplication *app;
  app = GCAL_APPLICATION (gtk_window_get_application (GTK_WINDOW (window)));

  return gcal_application_get_manager (app);
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
  /* FIXME: decide on this */
  /* gtk_container_add ( */
  /*     GTK_CONTAINER (gtk_clutter_actor_get_contents ( */
  /*                       GTK_CLUTTER_ACTOR (priv->sources_actor))), */
  /*     priv->sources_view); */
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
gcal_window_add_event (GtkButton *new_button,
                       gpointer   user_data)
{
  GcalWindowPrivate *priv;

  priv = GCAL_WINDOW (user_data)->priv;
  gcal_view_create_event_on_current_unit (GCAL_VIEW (priv->views[priv->active_view]));
}

static void
gcal_window_sources_row_activated (GtkTreeView        *tree_view,
                                   GtkTreePath        *path,
                                   GtkTreeViewColumn  *column,
                                   gpointer            user_data)
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
gcal_window_remove_event (GdNotification  *notification,
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

  /* since this is called when the notification is dismissed is safe to do here: */
  priv->noty = NULL;
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
        if (priv->noty != NULL)
          g_clear_object (&(priv->noty));

        priv->noty = gd_notification_new ();
        grid = gtk_grid_new ();
        gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
        gtk_container_add (GTK_CONTAINER (grid),
                           gtk_label_new (_("Event deleted")));

        undo_button = gtk_button_new_from_stock (GTK_STOCK_UNDO);
        gtk_container_add (GTK_CONTAINER (grid), undo_button);

        gtk_container_add (GTK_CONTAINER (priv->noty), grid);
        gcal_window_show_notification (GCAL_WINDOW (user_data));

        g_signal_connect (priv->noty,
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
gcal_window_new_with_view (GcalApplication   *app,
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

  return GTK_WIDGET (win);
}

void
gcal_window_show_notification (GcalWindow *window)
{
  GcalWindowPrivate *priv;

  g_return_if_fail (GCAL_IS_WINDOW (window));
  priv = window->priv;

  gtk_overlay_add_overlay (GTK_OVERLAY (priv->views_overlay),
                           priv->noty);
  gtk_widget_show_all (priv->noty);
}

void
gcal_window_hide_notification (GcalWindow *window)
{
  GcalWindowPrivate *priv;

  g_return_if_fail (GCAL_IS_WINDOW (window));
  priv = window->priv;

  gd_notification_dismiss (GD_NOTIFICATION (priv->noty));
}
