/* -*- mode: c; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gcal-window.c
 * Copyright (C) 2012 Erick Pérez Castellanos <erickpc@gnome.org>
 * Copyright (C) 2014 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#include "gcal-nav-bar.h"
#include "gcal-manager.h"
#include "gcal-view.h"
#include "gcal-month-view.h"
#include "gcal-week-view.h"
#include "gcal-year-view.h"
#include "gcal-event-widget.h"
#include "gcal-edit-dialog.h"
#include "gcal-enum-types.h"
#include "gcal-new-event-widget.h"

#include "e-cell-renderer-color.h"

#include <glib/gi18n.h>

#include <libecal/libecal.h>
#include <libical/icaltime.h>

struct _NewEventData
{
  gint               x;
  gint               y;
  icaltimetype      *start_date;
  icaltimetype      *end_date;
};

typedef struct _NewEventData NewEventData;

typedef struct
{
  /* timeout ids */
  guint                save_geometry_timeout_id;

  /* upper level widgets */
  GtkWidget           *main_box;

  GtkWidget           *header_bar;
  GtkWidget           *search_bar;
  GtkWidget           *nav_bar;
  GtkWidget           *views_overlay;
  GtkWidget           *views_stack;
  GtkWidget           *notification;
  GtkWidget           *new_event_widget;
  GtkWidget           *popover; /* short-lived */

  /* header_bar widets */
  GtkWidget           *today_button;
  GtkWidget           *search_entry;
  GtkWidget           *views_switcher;

  GtkWidget           *views [5]; /* day, week, month, year, list */
  GtkWidget           *edit_dialog;

  GcalManager         *manager;
  GcalWindowViewType   active_view;
  icaltimetype        *active_date;

  /* states */
  gboolean             new_event_mode;
  gboolean             search_mode;

  NewEventData        *event_creation_data;
  /* FIXME: Review to see if this are needed */
  /* temp to keep the will_delete event uuid */
  gchar               *event_to_delete;

  /* temp to keep event_creation */
  gboolean             open_edit_dialog;
} GcalWindowPrivate;

enum
{
  PROP_0,
  PROP_ACTIVE_VIEW,
  PROP_ACTIVE_DATE,
  PROP_NEW_EVENT_MODE,
  PROP_MANAGER
};

#define SAVE_GEOMETRY_ID_TIMEOUT 100 /* ms */

static gboolean       key_pressed                        (GtkWidget           *widget,
                                                          GdkEventKey         *event,
                                                          gpointer             user_data);

static void           date_updated                       (GtkButton           *buttton,
                                                          gpointer             user_data);

static void           load_geometry                      (GcalWindow          *window);

static gboolean       save_geometry                      (gpointer             user_data);

static void           update_view                        (GcalWindow          *window);

static void           view_changed                       (GObject             *object,
                                                          GParamSpec          *pspec,
                                                          gpointer             user_data);

static void           set_new_event_mode                 (GcalWindow          *window,
                                                          gboolean             enabled);

static void           prepare_new_event_widget           (GcalWindow          *window);

static void           show_new_event_widget              (GcalView            *view,
                                                          gpointer             start_span,
                                                          gpointer             end_span,
                                                          gdouble              x,
                                                          gdouble              y,
                                                          gpointer             user_data);

static gboolean       place_new_event_widget             (GtkWidget           *popover,
                                                          gint                 x,
                                                          gint                 y);

static void           close_new_event_widget             (GtkButton           *button,
                                                          gpointer             user_data);

static void           create_notification                (GcalWindow          *window);

static void           hide_notification                  (GtkWidget           *button,
                                                          gpointer             user_data);

/* handling events interaction */
static void           create_event                       (gpointer             user_data,
                                                          GtkWidget           *widget);

static void           event_activated                    (GcalView            *view,
                                                          GcalEventWidget     *event_widget,
                                                          gpointer             user_data);

static void           init_edit_dialog                   (GcalWindow          *window);

static void           edit_dialog_closed                 (GtkDialog           *dialog,
                                                          gint                 response,
                                                          gpointer             user_data);

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

static gboolean       gcal_window_configure_event        (GtkWidget           *widget,
                                                          GdkEventConfigure   *event);

static gboolean       gcal_window_state_event            (GtkWidget           *widget,
                                                          GdkEventWindowState *event);

static void           gcal_window_search_toggled         (GObject             *object,
                                                          GParamSpec          *pspec,
                                                          gpointer             user_data);

static void           gcal_window_search_changed         (GtkEditable         *editable,
                                                          gpointer             user_data);

/* GcalManager signal handling */
static void           gcal_window_event_created          (GcalManager         *manager,
                                                          const gchar         *source_uid,
                                                          const gchar         *event_uid,
                                                          gpointer             user_data);

static void           gcal_window_remove_event           (GtkWidget           *notification,
                                                          GParamSpec          *spec,
                                                          gpointer             user_data);

static void           gcal_window_undo_remove_event      (GtkButton           *button,
                                                          gpointer             user_data);

G_DEFINE_TYPE_WITH_PRIVATE (GcalWindow, gcal_window, GTK_TYPE_APPLICATION_WINDOW)

static gboolean
key_pressed (GtkWidget   *widget,
             GdkEventKey *event,
             gpointer     user_data)
{
  GcalWindowPrivate *priv;

  priv = gcal_window_get_instance_private (GCAL_WINDOW (user_data));

  g_debug ("Catching key events");

  if (priv->new_event_mode &&
      event->keyval == GDK_KEY_Escape)
    {
      set_new_event_mode (GCAL_WINDOW (user_data), FALSE);
      return TRUE;
    }
  else
    {
      if (priv->new_event_mode)
        return FALSE;

      if ((event->state & GDK_META_MASK) != 0 ||
          (event->state & GDK_CONTROL_MASK) != 0 ||
          event->keyval == GDK_KEY_Control_L ||
          event->keyval == GDK_KEY_Control_R ||
          event->keyval == GDK_KEY_Meta_L ||
          event->keyval == GDK_KEY_Meta_R ||
          event->keyval == GDK_KEY_Alt_L ||
          event->keyval == GDK_KEY_Alt_R)
        {
          return FALSE;
        }

      if (priv->search_mode && event->keyval == GDK_KEY_Escape)
        gcal_window_set_search_mode (GCAL_WINDOW (widget), FALSE);
      if (!priv->search_mode)
        gcal_window_set_search_mode (GCAL_WINDOW (widget), TRUE);
    }

  return FALSE;
}

static void
date_updated (GtkButton  *button,
              gpointer    user_data)
{
  GcalWindowPrivate *priv;

  gboolean move_back, move_today;

  priv = gcal_window_get_instance_private (GCAL_WINDOW (user_data));

  move_today = priv->today_button == (GtkWidget*) button;
  move_back = gcal_nav_bar_get_prev_button (GCAL_NAV_BAR (priv->nav_bar)) == (GtkWidget*) button;

  if (move_today)
    {
      *(priv->active_date) =
        icaltime_current_time_with_zone (
            gcal_manager_get_system_timezone (priv->manager));
    }
  else
    {
      switch (priv->active_view)
        {
        case GCAL_WINDOW_VIEW_DAY:
          priv->active_date->day += 1 * (move_back ? -1 : 1);
          break;
        case GCAL_WINDOW_VIEW_WEEK:
          priv->active_date->day += 7 * (move_back ? -1 : 1);
          break;
        case GCAL_WINDOW_VIEW_MONTH:
          priv->active_date->month += 1 * (move_back ? -1 : 1);
          break;
        case GCAL_WINDOW_VIEW_YEAR:
          priv->active_date->year += 1 * (move_back ? -1 : 1);
          break;
        case GCAL_WINDOW_VIEW_LIST:
        case GCAL_WINDOW_VIEW_SEARCH:
          break;
        }
    }
  *(priv->active_date) = icaltime_normalize (*(priv->active_date));
  g_object_notify (user_data, "active-date");

  update_view (GCAL_WINDOW (user_data));
}

static void
load_geometry (GcalWindow *window)
{
  GcalApplication *app;
  GSettings *settings;
  GVariant *variant;
  gboolean maximized;
  const gint32 *position;
  const gint32 *size;
  gsize n_elements;

  app = GCAL_APPLICATION (gtk_window_get_application (GTK_WINDOW (window)));
  settings = gcal_application_get_settings (app);

  /* load window settings: size */
  variant = g_settings_get_value (settings,
                                  "window-size");
  size = g_variant_get_fixed_array (variant,
                                    &n_elements,
                                    sizeof (gint32));
  if (n_elements == 2)
    gtk_window_set_default_size (GTK_WINDOW (window),
                                 size[0],
                                 size[1]);
  g_variant_unref (variant);

  /* load window settings: position */
  variant = g_settings_get_value (settings,
                                  "window-position");
  position = g_variant_get_fixed_array (variant,
                                        &n_elements,
                                        sizeof (gint32));
  if (n_elements == 2)
    gtk_window_move (GTK_WINDOW (window),
                     position[0],
                     position[1]);

  g_variant_unref (variant);

  /* load window settings: state */
  maximized = g_settings_get_boolean (settings,
                                      "window-maximized");
  if (maximized)
    gtk_window_maximize (GTK_WINDOW (window));
}

static gboolean
save_geometry (gpointer user_data)
{
  GtkWindow *self;
  GdkWindow *window;
  GcalWindowPrivate *priv;
  GdkWindowState state;
  GcalApplication *app;
  GSettings *settings;
  gboolean maximized;
  GVariant *variant;
  gint32 size[2];
  gint32 position[2];

  self = GTK_WINDOW (user_data);

  window = gtk_widget_get_window (GTK_WIDGET (self));
  state = gdk_window_get_state (window);
  priv = gcal_window_get_instance_private (GCAL_WINDOW (self));

  app = GCAL_APPLICATION (gtk_window_get_application (self));
  settings = gcal_application_get_settings (app);

  /* save window's state */
  maximized = state & GDK_WINDOW_STATE_MAXIMIZED;
  g_settings_set_boolean (settings,
                          "window-maximized",
                          maximized);

  if (maximized)
    {
      priv->save_geometry_timeout_id = 0;
      return FALSE;
    }

  /* save window's size */
  gtk_window_get_size (self,
                       (gint *) &size[0],
                       (gint *) &size[1]);
  variant = g_variant_new_fixed_array (G_VARIANT_TYPE_INT32,
                                       size,
                                       2,
                                       sizeof (size[0]));
  g_settings_set_value (settings,
                        "window-size",
                        variant);

  /* save windows's position */
  gtk_window_get_position (self,
                           (gint *) &position[0],
                           (gint *) &position[1]);
  variant = g_variant_new_fixed_array (G_VARIANT_TYPE_INT32,
                                       position,
                                       2,
                                       sizeof (position[0]));
  g_settings_set_value (settings,
                        "window-position",
                        variant);

  priv->save_geometry_timeout_id = 0;

  return FALSE;
}

/**
 * update_view:
 * @window:
 *
 * Update the headers on the navbar. Everything else on the application
 * updates itself on the date change.
 **/
static void
update_view (GcalWindow *window)
{
  GcalWindowPrivate *priv;

  GtkWidget *widget;

  gchar* header;

  priv = gcal_window_get_instance_private (window);

  widget = priv->views[priv->active_view];

  header = gcal_view_get_left_header (GCAL_VIEW (widget));
  g_object_set (priv->nav_bar, "left-header", header, NULL);
  g_free (header);

  header = gcal_view_get_right_header (GCAL_VIEW (widget));
  g_object_set (priv->nav_bar, "right-header", header, NULL);
  g_free (header);
}

/**
 * view_changed:
 * @object:
 * @pspec:
 * @user_data:
 *
 * Retrieve the enum value representing the view, update internal
 * @active_view with it and call @update_view
 **/
static void
view_changed (GObject    *object,
              GParamSpec *pspec,
              gpointer    user_data)
{
  GcalWindowPrivate *priv;
  GEnumClass *eklass;
  GEnumValue *eval;
  GcalWindowViewType view_type;

  priv = gcal_window_get_instance_private (GCAL_WINDOW (user_data));

  /* XXX: this is the destruction process */
  if (!gtk_widget_get_visible (priv->views_stack))
    return;

  eklass = g_type_class_ref (gcal_window_view_type_get_type ());
  eval = g_enum_get_value_by_nick (
             eklass,
             gtk_stack_get_visible_child_name (GTK_STACK (priv->views_stack)));

  view_type = eval->value;

  g_type_class_unref (eklass);

  priv->active_view = view_type;
  g_object_notify (G_OBJECT (user_data), "active-view");

  update_view (GCAL_WINDOW (user_data));
}

static void
set_new_event_mode (GcalWindow *window,
                    gboolean    enabled)
{
  GcalWindowPrivate *priv;

  priv = gcal_window_get_instance_private (window);
  priv->new_event_mode = enabled;
  g_object_notify (G_OBJECT (window), "new-event-mode");

  if (! enabled && priv->views[priv->active_view] != NULL)
    gcal_view_clear_marks (GCAL_VIEW (priv->views[priv->active_view]));

  /* XXX: here we could disable clicks from the views, yet */
  /* for now we relaunch the new-event widget */
  if (!enabled &&
      gtk_widget_is_visible (priv->popover))
    {
      gtk_widget_set_visible (priv->popover, FALSE);
    }
}

static void
prepare_new_event_widget (GcalWindow *window)
{
  GcalWindowPrivate *priv;

  gchar *uid;

  struct tm tm_date;
  gchar start[64];
  gchar *title_date;

  GcalNewEventWidget *new_widget;
  GtkWidget *widget;

  priv = gcal_window_get_instance_private (window);
  new_widget = GCAL_NEW_EVENT_WIDGET (priv->new_event_widget);

  /* setting title */
  tm_date = icaltimetype_to_tm (priv->event_creation_data->start_date);
  e_utf8_strftime_fix_am_pm (start, 64, "%B %d", &tm_date);
  title_date = g_strdup_printf (_("New Event on %s"), start);

  gcal_new_event_widget_set_title (new_widget, title_date);
  g_free (title_date);

  /* FIXME: do this somehow since GcalManager doesn't keep
     a list store of calendars anymore, and this is a stub method */
  gcal_new_event_widget_set_calendars (
      new_widget,
      GTK_TREE_MODEL (gcal_manager_get_sources_model (priv->manager)));

  uid = gcal_manager_get_default_source (priv->manager);
  gcal_new_event_widget_set_default_calendar (new_widget, uid);
  g_free (uid);

  /* clear entry */
  widget = gcal_new_event_widget_get_entry (new_widget);
  gtk_entry_set_text (GTK_ENTRY (widget), "");

  /* FIXME: add signals handling */
  widget = GTK_WIDGET (priv->popover);
  g_signal_connect (widget, "closed",
                    G_CALLBACK (close_new_event_widget), window);
  widget = gcal_new_event_widget_get_create_button (new_widget);
  g_signal_connect_swapped (widget, "clicked",
                            G_CALLBACK (create_event), window);
  widget = gcal_new_event_widget_get_details_button (new_widget);
  g_signal_connect_swapped (widget, "clicked",
                            G_CALLBACK (create_event), window);
  widget = gcal_new_event_widget_get_entry (new_widget);
  g_signal_connect_swapped (widget, "activate",
                            G_CALLBACK (create_event), window);
  gtk_widget_show_all (priv->popover);
}

/* new-event interaction: second variant */
static void
show_new_event_widget (GcalView *view,
                       gpointer  start_span,
                       gpointer  end_span,
                       gdouble   x,
                       gdouble   y,
                       gpointer  user_data)
{
  GcalWindowPrivate *priv;

  g_return_if_fail (user_data);
  priv = gcal_window_get_instance_private (GCAL_WINDOW (user_data));

  /* 1st and 2nd steps */
  set_new_event_mode (GCAL_WINDOW (user_data), TRUE);

  if (priv->event_creation_data != NULL)
    {
      g_free (priv->event_creation_data->start_date);
      g_free (priv->event_creation_data->end_date);
      g_free (priv->event_creation_data);
    }

  priv->event_creation_data = g_new0 (NewEventData, 1);
  priv->event_creation_data->x = x;
  priv->event_creation_data->y = y;
  priv->event_creation_data->start_date = gcal_dup_icaltime (start_span);
  if (end_span != NULL)
    priv->event_creation_data->end_date = gcal_dup_icaltime (end_span);
  g_debug ("[show_new_event] position (%f, %f)", x, y);

  /* Setup new event widget data */
  prepare_new_event_widget (GCAL_WINDOW (user_data));

  place_new_event_widget (priv->popover, x, y);
}

static gboolean
place_new_event_widget (GtkWidget    *popover,
                        gint          x,
                        gint          y)
{
  GdkRectangle rect;

  /* Place popover over the given (x,y) position */
  rect.x = x;
  rect.y = y;
  rect.width = 1;
  rect.height = 1;

  gtk_popover_set_pointing_to (GTK_POPOVER (popover), &rect);

  return TRUE;
}

static void
close_new_event_widget (GtkButton *button,
                        gpointer   user_data)
{
  set_new_event_mode (GCAL_WINDOW (user_data), FALSE);
}

static void
create_notification (GcalWindow *window)
{
  GcalWindowPrivate *priv;

  GtkWidget *notification_frame;
  GtkWidget *notification_grid;
  GtkWidget *undo_button;
  GtkWidget *hide_button;

  priv = gcal_window_get_instance_private (window);

  priv->notification = gtk_revealer_new ();
  gtk_revealer_set_transition_type (
      GTK_REVEALER (priv->notification),
      GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
  gtk_revealer_set_transition_duration (GTK_REVEALER (priv->notification), 100);
  gtk_widget_set_halign (priv->notification, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (priv->notification, GTK_ALIGN_START);

  gtk_overlay_add_overlay (GTK_OVERLAY (priv->views_overlay),
                           priv->notification);

  notification_frame = gtk_frame_new (NULL);
  gtk_style_context_add_class (
      gtk_widget_get_style_context (notification_frame),
      "app-notification");

  /* notification content */
  notification_grid = gtk_grid_new ();
  gtk_grid_set_column_spacing (GTK_GRID (notification_grid), 20);

  undo_button = gtk_button_new_with_label (_("Undo"));
  gtk_widget_set_valign (undo_button, GTK_ALIGN_CENTER);

  hide_button = gtk_button_new_from_icon_name ("window-close-symbolic", GTK_ICON_SIZE_MENU);
  gtk_button_set_relief (GTK_BUTTON (hide_button), GTK_RELIEF_NONE);
  gtk_style_context_add_class (
      gtk_widget_get_style_context (hide_button),
      "image-button");

  gtk_container_add (GTK_CONTAINER (notification_grid), gtk_label_new (_("Event deleted")));
  gtk_container_add (GTK_CONTAINER (notification_grid), undo_button);
  gtk_container_add (GTK_CONTAINER (notification_grid), hide_button);

  gtk_container_add (GTK_CONTAINER (notification_frame), notification_grid);
  gtk_container_add (GTK_CONTAINER (priv->notification), notification_frame);

  gtk_widget_show_all (priv->notification);

  g_signal_connect (hide_button,
                    "clicked",
                    G_CALLBACK (hide_notification),
                    window);
  g_signal_connect (priv->notification,
                    "notify::child-revealed",
                    G_CALLBACK (gcal_window_remove_event),
                    window);
  g_signal_connect (undo_button,
                    "clicked",
                    G_CALLBACK (gcal_window_undo_remove_event),
                    window);
}

static void
hide_notification (GtkWidget *button,
                   gpointer   user_data)
{
  gcal_window_hide_notification (GCAL_WINDOW (user_data));
}

static void
create_event (gpointer   user_data,
              GtkWidget *widget)
{
  GcalWindowPrivate *priv;

  GcalNewEventWidget *new_widget;

  gchar *uid;
  gchar *summary;

  priv = gcal_window_get_instance_private (GCAL_WINDOW (user_data));
  new_widget = GCAL_NEW_EVENT_WIDGET (priv->new_event_widget);

  if (widget == gcal_new_event_widget_get_details_button (new_widget))
    priv->open_edit_dialog = TRUE;

  uid = gcal_new_event_widget_get_calendar_uid (new_widget);
  summary = gcal_new_event_widget_get_summary (new_widget);

  /* create the event */
  gcal_manager_create_event (priv->manager,
                             uid, summary,
                             priv->event_creation_data->start_date,
                             priv->event_creation_data->end_date);

  g_free (uid);
  g_free (summary);
  /* reset and hide */
  set_new_event_mode (GCAL_WINDOW (user_data), FALSE);
}

static void
event_activated (GcalView        *view,
                 GcalEventWidget *event_widget,
                 gpointer         user_data)
{
  GcalWindowPrivate *priv;
  GcalEventData *data;

  priv = gcal_window_get_instance_private (GCAL_WINDOW (user_data));

  if (priv->edit_dialog == NULL)
    init_edit_dialog (GCAL_WINDOW (user_data));

  data = gcal_event_widget_get_data (event_widget);
  gcal_edit_dialog_set_event_data (
      GCAL_EDIT_DIALOG (priv->edit_dialog),
      data);
  g_free (data);

  gtk_dialog_run (GTK_DIALOG (priv->edit_dialog));
}

static void
init_edit_dialog (GcalWindow *window)
{
  GcalWindowPrivate *priv;

  priv = gcal_window_get_instance_private (window);

  priv->edit_dialog = gcal_edit_dialog_new ();
  gtk_window_set_transient_for (GTK_WINDOW (priv->edit_dialog),
                                GTK_WINDOW (window));
  gcal_edit_dialog_set_manager (GCAL_EDIT_DIALOG (priv->edit_dialog),
                                priv->manager);

  g_signal_connect (priv->edit_dialog,
                    "response", G_CALLBACK (edit_dialog_closed),
                    window);
}

static void
edit_dialog_closed (GtkDialog *dialog,
                    gint       response,
                    gpointer   user_data)
{
  GcalWindowPrivate *priv;

  GcalEditDialog *edit_dialog;
  ECalComponent *component;
  GcalView *view;

  priv = gcal_window_get_instance_private (GCAL_WINDOW (user_data));

  gtk_widget_hide (GTK_WIDGET (dialog));

  view = GCAL_VIEW (priv->views[priv->active_view]);
  edit_dialog = GCAL_EDIT_DIALOG (dialog);

  switch (response)
    {
    case GCAL_RESPONSE_SAVE_EVENT:
      /* retrieve the component from the dialog*/
      component = gcal_edit_dialog_get_component (edit_dialog);

      gcal_manager_update_event (priv->manager,
                                 gcal_edit_dialog_get_source (edit_dialog),
                                 component);

      break;

    case GCAL_RESPONSE_DELETE_EVENT:
      /* delete the event */
      if (priv->notification != NULL)
        g_clear_object (&(priv->notification));

      create_notification (GCAL_WINDOW (user_data));
      gcal_window_show_notification (GCAL_WINDOW (user_data));

      priv->event_to_delete =
        gcal_edit_dialog_get_event_uuid (edit_dialog);

      /* hide widget of the event */
      gtk_widget_hide (gcal_view_get_by_uuid (view,
                                              priv->event_to_delete));

      break;

    case GTK_RESPONSE_CANCEL:
      break;

    }
}

static void
gcal_window_class_init(GcalWindowClass *klass)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->constructed = gcal_window_constructed;
  object_class->finalize = gcal_window_finalize;
  object_class->set_property = gcal_window_set_property;
  object_class->get_property = gcal_window_get_property;

  widget_class = GTK_WIDGET_CLASS (klass);
  widget_class->configure_event = gcal_window_configure_event;
  widget_class->window_state_event = gcal_window_state_event;

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

  g_object_class_install_property (
      object_class,
      PROP_NEW_EVENT_MODE,
      g_param_spec_boolean ("new-event-mode",
                            "New Event mode",
                            "Whether the window is in new-event-mode or not",
                            FALSE,
                            G_PARAM_READWRITE));

  g_object_class_install_property (
      object_class,
      PROP_MANAGER,
      g_param_spec_pointer ("manager",
                            "The manager object",
                            "A weak reference to the app manager object",
                            G_PARAM_CONSTRUCT_ONLY |
                            G_PARAM_READWRITE));
}

static void
gcal_window_init (GcalWindow *self)
{
  GcalWindowPrivate *priv;

  priv = gcal_window_get_instance_private (self);

  /* states */
  priv->search_mode = FALSE;

  priv->event_creation_data = NULL;

  /* FIXME: Review real need of this */
  priv->save_geometry_timeout_id = 0;
  priv->event_to_delete = NULL;
  priv->open_edit_dialog = FALSE;
}

static void
gcal_window_constructed (GObject *object)
{
  GcalWindowPrivate *priv;

  GtkWidget *box;
  GtkWidget *search_button;

  GtkWidget *menu_button;
  GtkBuilder *builder;
  GMenuModel *winmenu;

  gint i;

  if (G_OBJECT_CLASS (gcal_window_parent_class)->constructed != NULL)
    G_OBJECT_CLASS (gcal_window_parent_class)->constructed (object);

  priv = gcal_window_get_instance_private (GCAL_WINDOW (object));

  /* ui construction */
  priv->main_box = gtk_grid_new ();
  gtk_orientable_set_orientation (GTK_ORIENTABLE (priv->main_box),
                                  GTK_ORIENTATION_VERTICAL);

  /* header_bar */
  priv->header_bar = gtk_header_bar_new ();

  /* header_bar: new */
  priv->today_button = gtk_button_new_with_label (_("Today"));
  gtk_header_bar_pack_start (GTK_HEADER_BAR (priv->header_bar), priv->today_button);

  priv->views_switcher = gtk_stack_switcher_new ();
  g_object_ref_sink (priv->views_switcher);
  gtk_header_bar_set_custom_title (GTK_HEADER_BAR (priv->header_bar),
                                   priv->views_switcher);

  /* header_bar: menu */
  menu_button = gtk_menu_button_new ();
  gtk_menu_button_set_use_popover (GTK_MENU_BUTTON (menu_button),
                                   TRUE);
  gtk_button_set_image (
      GTK_BUTTON (menu_button),
      gtk_image_new_from_icon_name ("open-menu-symbolic",
                                    GTK_ICON_SIZE_MENU));
  gtk_header_bar_pack_end (GTK_HEADER_BAR (priv->header_bar),
                           menu_button);

  builder = gtk_builder_new ();
  gtk_builder_add_from_resource (builder,
                                 "/org/gnome/calendar/menus.ui",
                                 NULL);

  winmenu = (GMenuModel *)gtk_builder_get_object (builder, "winmenu");
  gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (menu_button),
                                  winmenu);

  g_object_unref (builder);

  /* header_bar: search */
  search_button = gtk_toggle_button_new ();
  gtk_container_add (
      GTK_CONTAINER (search_button),
      gtk_image_new_from_icon_name ("edit-find-symbolic",
                                    GTK_ICON_SIZE_MENU));
  gtk_header_bar_pack_end (GTK_HEADER_BAR (priv->header_bar),
                           search_button);

  gtk_header_bar_set_show_close_button (
      GTK_HEADER_BAR (priv->header_bar),
      TRUE);

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

  priv->search_bar = gtk_search_bar_new ();
  gtk_search_bar_connect_entry (GTK_SEARCH_BAR (priv->search_bar),
                                GTK_ENTRY (priv->search_entry));
  gtk_widget_set_hexpand (priv->search_bar, TRUE);
  g_object_bind_property (search_button, "active",
                          priv->search_bar, "search-mode-enabled",
                          G_BINDING_BIDIRECTIONAL);
  gtk_container_add (GTK_CONTAINER (priv->search_bar), box);
  gtk_container_add (GTK_CONTAINER (priv->main_box), priv->search_bar);

  /* overlay */
  priv->views_overlay = gtk_overlay_new ();
  gtk_container_add (GTK_CONTAINER (priv->main_box), priv->views_overlay);

  box = gtk_grid_new ();
  gtk_orientable_set_orientation (GTK_ORIENTABLE (box),
                                  GTK_ORIENTATION_VERTICAL);
  gtk_container_add (GTK_CONTAINER (priv->views_overlay), box);

  /* nav_bar */
  priv->nav_bar = gcal_nav_bar_new ();
  gtk_container_add (GTK_CONTAINER (box), priv->nav_bar);

  /* stack widget for holding views */
  priv->views_stack = gtk_stack_new ();
  g_object_set (priv->views_stack,
                "vexpand", TRUE,
                "hexpand", TRUE,
                "transition-type", GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT,
                "transition-duration", 500,
                NULL);
  gtk_container_add (GTK_CONTAINER (box), priv->views_stack);

  gtk_style_context_add_class (
      gtk_widget_get_style_context (priv->views_stack),
      "views");

  priv->views[GCAL_WINDOW_VIEW_WEEK] =
    gcal_week_view_new (priv->manager);
  gtk_stack_add_titled (GTK_STACK (priv->views_stack),
                        priv->views[GCAL_WINDOW_VIEW_WEEK],
                        "week", _("Week"));
  g_object_bind_property (GCAL_WINDOW (object), "active-date",
                          priv->views[GCAL_WINDOW_VIEW_WEEK], "active-date",
                          G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

  priv->views[GCAL_WINDOW_VIEW_MONTH] =
    gcal_month_view_new (priv->manager);
  gtk_stack_add_titled (GTK_STACK (priv->views_stack),
                        priv->views[GCAL_WINDOW_VIEW_MONTH],
                        "month", _("Month"));
  g_object_bind_property (GCAL_WINDOW (object), "active-date",
                          priv->views[GCAL_WINDOW_VIEW_MONTH], "active-date",
                          G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

  priv->views[GCAL_WINDOW_VIEW_YEAR] =
    gcal_year_view_new (priv->manager);
  gtk_stack_add_titled (GTK_STACK (priv->views_stack),
                        priv->views[GCAL_WINDOW_VIEW_YEAR],
                        "year", _("Year"));
  g_object_bind_property (GCAL_WINDOW (object), "active-date",
                          priv->views[GCAL_WINDOW_VIEW_YEAR], "active-date",
                          G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

  gtk_stack_switcher_set_stack (GTK_STACK_SWITCHER (priv->views_switcher),
                                GTK_STACK (priv->views_stack));

  gtk_container_add (GTK_CONTAINER (object), priv->main_box);
  gtk_widget_show_all (priv->main_box);

  gtk_style_context_add_class (
      gtk_widget_get_style_context (GTK_WIDGET (object)),
      "views");

  /* popover and content */
  priv->popover = gtk_popover_new (GTK_WIDGET(priv->views_stack));

  priv->new_event_widget = gcal_new_event_widget_new ();

  gtk_container_add (GTK_CONTAINER(priv->popover),
                     GTK_WIDGET(priv->new_event_widget));

  /* signals connection/handling */
  g_signal_connect (object, "key-press-event",
                    G_CALLBACK (key_pressed), object);

  for (i = 0; i < 4; ++i)
    {
      if (priv->views[i] != NULL)
        {
          g_signal_connect (priv->views[i], "create-event",
                            G_CALLBACK (show_new_event_widget), object);
          g_signal_connect (priv->views[i], "event-activated",
                            G_CALLBACK (event_activated), object);
        }
    }

  g_signal_connect (priv->search_bar, "notify::search-mode-enabled",
                    G_CALLBACK (gcal_window_search_toggled), object);
  g_signal_connect (priv->search_entry, "changed",
                    G_CALLBACK (gcal_window_search_changed), object);

  g_signal_connect (priv->views_stack, "notify::visible-child",
                    G_CALLBACK (view_changed), object);

  g_signal_connect (priv->today_button, "clicked",
                    G_CALLBACK (date_updated), object);

  g_signal_connect (gcal_nav_bar_get_prev_button (GCAL_NAV_BAR (priv->nav_bar)),
                    "clicked", G_CALLBACK (date_updated), object);
  g_signal_connect (gcal_nav_bar_get_next_button (GCAL_NAV_BAR (priv->nav_bar)),
                    "clicked", G_CALLBACK (date_updated), object);
}

static void
gcal_window_finalize (GObject *object)
{
  GcalWindowPrivate *priv;

  priv = gcal_window_get_instance_private (GCAL_WINDOW (object));

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

  priv = gcal_window_get_instance_private (GCAL_WINDOW (object));

  switch (property_id)
    {
    case PROP_ACTIVE_VIEW:
      priv->active_view = g_value_get_enum (value);
      gtk_stack_set_visible_child (GTK_STACK (priv->views_stack),
                                   priv->views[priv->active_view]);


      update_view (GCAL_WINDOW (object));
      return;
    case PROP_ACTIVE_DATE:
      if (priv->active_date != NULL)
        g_free (priv->active_date);
      priv->active_date = g_value_dup_boxed (value);
      return;
    case PROP_NEW_EVENT_MODE:
      set_new_event_mode (GCAL_WINDOW (object), g_value_get_boolean (value));
      return;
    case PROP_MANAGER:
      priv->manager = g_value_get_pointer (value);
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

  priv = gcal_window_get_instance_private (GCAL_WINDOW (object));

  switch (property_id)
    {
    case PROP_ACTIVE_VIEW:
      g_value_set_enum (value, priv->active_view);
      return;
    case PROP_ACTIVE_DATE:
      g_value_set_boxed (value, priv->active_date);
      return;
    case PROP_NEW_EVENT_MODE:
      g_value_set_boolean (value, priv->new_event_mode);
      return;
    }

  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static gboolean
gcal_window_configure_event (GtkWidget         *widget,
                             GdkEventConfigure *event)
{
  GcalWindow *window;
  GcalWindowPrivate *priv;
  gboolean retval;

  window = GCAL_WINDOW (widget);
  priv = gcal_window_get_instance_private (window);

  if (priv->save_geometry_timeout_id != 0)
    {
      g_source_remove (priv->save_geometry_timeout_id);
      priv->save_geometry_timeout_id = 0;
    }

  priv->save_geometry_timeout_id = g_timeout_add (SAVE_GEOMETRY_ID_TIMEOUT,
                                                  save_geometry,
                                                  window);

  retval = GTK_WIDGET_CLASS (gcal_window_parent_class)->configure_event (widget, event);

  return retval;
}

static gboolean
gcal_window_state_event (GtkWidget           *widget,
                         GdkEventWindowState *event)
{
  GcalWindow *window;
  GcalWindowPrivate *priv;
  gboolean retval;

  window = GCAL_WINDOW (widget);
  priv = gcal_window_get_instance_private (window);

  if (priv->save_geometry_timeout_id != 0)
    {
      g_source_remove (priv->save_geometry_timeout_id);
      priv->save_geometry_timeout_id = 0;
    }

  priv->save_geometry_timeout_id = g_timeout_add (SAVE_GEOMETRY_ID_TIMEOUT,
                                                  save_geometry,
                                                  window);

  retval = GTK_WIDGET_CLASS (gcal_window_parent_class)->window_state_event (widget, event);

  return retval;
}

static void
gcal_window_search_toggled (GObject    *object,
                            GParamSpec *pspec,
                            gpointer    user_data)
{
  GcalWindowPrivate *priv;

  priv = gcal_window_get_instance_private (GCAL_WINDOW (user_data));

  if (gtk_search_bar_get_search_mode (GTK_SEARCH_BAR (priv->search_bar)))
    {
      g_debug ("Entering search mode");

      /* update headder_bar widget */
      gtk_widget_hide (priv->today_button);
      gtk_header_bar_set_custom_title (GTK_HEADER_BAR (priv->header_bar),
                                       NULL);
      /* _prepare_for_search */
    }
  else
    {
      g_debug ("Leaving search mode");
      /* update header_bar */
      gtk_widget_show (priv->today_button);
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

  priv = gcal_window_get_instance_private (GCAL_WINDOW (user_data));

  if (gtk_search_bar_get_search_mode (GTK_SEARCH_BAR (priv->search_bar)))
    {
      gchar *title;

      if (gtk_entry_get_text_length (GTK_ENTRY (priv->search_entry)) != 0)
        {
          title = g_strdup_printf ("Results for \"%s\"",
                                   gtk_entry_get_text (GTK_ENTRY (priv->search_entry)));
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
gcal_window_event_created (GcalManager *manager,
                           const gchar *source_uid,
                           const gchar *event_uid,
                           gpointer     user_data)
{
  GcalWindowPrivate *priv;

  priv = gcal_window_get_instance_private (GCAL_WINDOW (user_data));

  if (! priv->open_edit_dialog)
    return;

  priv->open_edit_dialog = FALSE;

  if (priv->edit_dialog == NULL)
    init_edit_dialog (GCAL_WINDOW (user_data));

  /* FIXME: use new GcalEditDialog API */
  /* gcal_edit_dialog_set_event (GCAL_EDIT_DIALOG (priv->edit_dialog), */
  /*                             source_uid, */
  /*                             event_uid); */

  gtk_dialog_run (GTK_DIALOG (priv->edit_dialog));
}

static void
gcal_window_remove_event (GtkWidget       *notification,
                          GParamSpec      *spec,
                          gpointer         user_data)
{
  GcalWindowPrivate *priv;
  gchar **tokens;

  priv = gcal_window_get_instance_private (GCAL_WINDOW (user_data));

  if (gtk_revealer_get_child_revealed (GTK_REVEALER (notification)))
      return;

  if (priv->event_to_delete != NULL)
    {
      tokens = g_strsplit (priv->event_to_delete, ":", 2);

      gcal_manager_remove_event (priv->manager, tokens[0], tokens[1]);

      g_strfreev (tokens);
      g_free (priv->event_to_delete);
      priv->event_to_delete = NULL;
    }

  /* since this is called when the notification is dismissed is safe to do here: */
  gtk_widget_destroy (priv->notification);
}

static void
gcal_window_undo_remove_event (GtkButton *button,
                               gpointer   user_data)
{
  GcalWindowPrivate *priv;
  GtkWidget *event_widget;

  priv = gcal_window_get_instance_private (GCAL_WINDOW (user_data));

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
gcal_window_new_with_view (GcalApplication   *app,
                           GcalWindowViewType view_type)
{
  GcalWindow *win;
  GcalManager *manager;
  icaltimetype date;

  manager = gcal_application_get_manager (GCAL_APPLICATION (app));
  /* FIXME: here read the initial date from somewehere */
  date = icaltime_current_time_with_zone (gcal_manager_get_system_timezone (manager));
  date = icaltime_set_timezone (&date,
                                gcal_manager_get_system_timezone (manager));

  win  =  g_object_new (GCAL_TYPE_WINDOW,
                        "application",
                        GTK_APPLICATION (app),
                        "active-date",
                        &date,
                        "manager",
                        manager,
                        NULL);

  /* hooking signals */
  /* FIXME: remember to check if this is really needed */
  g_signal_connect (manager,
                    "event-created",
                    G_CALLBACK (gcal_window_event_created),
                    win);

  /* init hack */
  load_geometry (win);

  if (view_type == GCAL_WINDOW_VIEW_DAY)
    view_changed (NULL, NULL, win);

  return GTK_WIDGET (win);
}

/* new-event interaction: first variant */
void
gcal_window_new_event (GcalWindow *window)
{
  GcalWindowPrivate *priv;
  gint x, y;

  priv = gcal_window_get_instance_private (window);

  /* 1st and 2nd steps */
  set_new_event_mode (window, TRUE);

  gcal_view_mark_current_unit (GCAL_VIEW (priv->views[priv->active_view]),
                               &x, &y);

  if (priv->event_creation_data != NULL)
    {
      g_free (priv->event_creation_data->start_date);
      g_free (priv->event_creation_data->end_date);
      g_free (priv->event_creation_data);
    }

  priv->event_creation_data = g_new0 (NewEventData, 1);
  priv->event_creation_data->x = x;
  priv->event_creation_data->y = y;
  priv->event_creation_data->start_date = gcal_dup_icaltime (priv->active_date);
  priv->event_creation_data->end_date = gcal_dup_icaltime (priv->active_date);

  /* adjusting dates according to the actual view */
  switch (priv->active_view)
    {
    case GCAL_WINDOW_VIEW_DAY:
    case GCAL_WINDOW_VIEW_WEEK:
      priv->event_creation_data->end_date->hour += 1;
      *(priv->event_creation_data->end_date) =
        icaltime_normalize (*(priv->event_creation_data->end_date));
      break;
    case GCAL_WINDOW_VIEW_YEAR:
      priv->event_creation_data->start_date->day = 1;
      priv->event_creation_data->end_date->day =
        icaltime_days_in_month (priv->event_creation_data->end_date->month,
                                priv->event_creation_data->end_date->year);
      break;
    case GCAL_WINDOW_VIEW_MONTH:
      priv->event_creation_data->start_date->is_date = 1;
      priv->event_creation_data->end_date->is_date = 1;
      break;
    case GCAL_WINDOW_VIEW_LIST:
    case GCAL_WINDOW_VIEW_SEARCH:
      break;
    }

  prepare_new_event_widget (GCAL_WINDOW (window));

  place_new_event_widget (priv->popover, x, y);
}

void
gcal_window_set_search_mode (GcalWindow *window,
                             gboolean    enabled)
{
  GcalWindowPrivate *priv;

  priv = gcal_window_get_instance_private (window);
  gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (priv->search_bar),
                                  enabled);
}

void
gcal_window_show_notification (GcalWindow *window)
{
  GcalWindowPrivate *priv;

  priv = gcal_window_get_instance_private (window);

  gtk_revealer_set_reveal_child (GTK_REVEALER (priv->notification),
                                 TRUE);
}

void
gcal_window_hide_notification (GcalWindow *window)
{
  GcalWindowPrivate *priv;

  priv = gcal_window_get_instance_private (window);

  gtk_revealer_set_reveal_child (GTK_REVEALER (priv->notification),
                                 FALSE);
}
