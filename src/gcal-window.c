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

#include "gcal-nav-bar.h"
#include "gcal-manager.h"
#include "gcal-floating-container.h"
#include "gcal-view.h"
#include "gcal-day-view.h"
#include "gcal-month-view.h"
#include "gcal-week-view.h"
#include "gcal-year-view.h"
#include "gcal-event-widget.h"
#include "gcal-edit-dialog.h"
#include "gcal-enum-types.h"
#include "gcal-new-event-widget.h"

#include "e-cell-renderer-color.h"

#include <libgd/gd.h>

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
  /* upper level widgets */
  GtkWidget           *main_box;

  GtkWidget           *header_bar;
  GtkWidget           *search_bar;
  GtkWidget           *nav_bar;
  GtkWidget           *views_overlay;
  GtkWidget           *views_stack;
  GtkWidget           *noty; /* short-lived */
  GtkWidget           *new_event_widget; /* short-lived */

  /* header_bar widets */
  GtkWidget           *new_button;
  GtkWidget           *search_entry;
  GtkWidget           *views_switcher;

  GtkWidget           *views [5]; /* day, week, month, year, list */
  GtkWidget           *edit_dialog;

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
  PROP_NEW_EVENT_MODE
};

static gboolean       key_pressed                        (GtkWidget           *widget,
                                                          GdkEventKey         *event,
                                                          gpointer             user_data);

static void           date_updated                       (GtkButton           *buttton,
                                                          gpointer             user_data);

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

static gboolean       place_new_event_widget             (GtkOverlay          *overlay,
                                                          GtkWidget           *child,
                                                          GdkRectangle        *allocation,
                                                          gpointer             user_data);

static void           close_new_event_widget             (GtkButton           *button,
                                                          gpointer             user_data);

static GcalManager*   get_manager                        (GcalWindow          *window);

/* handling events interaction */
static void           create_event                       (gpointer             user_data,
                                                          GtkWidget           *widget);

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

static void           gcal_window_init_edit_dialog       (GcalWindow          *window);

/* GcalManager signal handling */
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

static void           gcal_window_edit_dialog_responded  (GtkDialog           *dialog,
                                                          gint                 response,
                                                          gpointer             user_data);

static void           gcal_window_update_event_widget    (GcalManager         *manager,
                                                          const gchar         *source_uid,
                                                          const gchar         *event_uid,
                                                          GcalEventWidget     *widget);

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

  return FALSE;
}

static void
date_updated (GtkButton  *button,
              gpointer    user_data)
{
  GcalWindowPrivate *priv;

  gboolean move_back;

  priv = gcal_window_get_instance_private (GCAL_WINDOW (user_data));

  move_back = gcal_nav_bar_get_prev_button (GCAL_NAV_BAR (priv->nav_bar)) == (GtkWidget*) button;

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
      break;
    }
  *(priv->active_date) = icaltime_normalize (*(priv->active_date));
  g_object_notify (user_data, "active-date");

  update_view (GCAL_WINDOW (user_data));
}

/**
 * update_view:
 * @window:
 *
 * Calling update view on the active view
 **/
static void
update_view (GcalWindow *window)
{
  GcalWindowPrivate *priv;

  GtkWidget *widget;

  icaltimetype *first_day;
  icaltimetype *last_day;
  gchar* header;

  priv = gcal_window_get_instance_private (window);

  widget = priv->views[priv->active_view];

  /* destroying old children */
  gcal_view_clear (GCAL_VIEW (widget));

  first_day = gcal_view_get_initial_date (GCAL_VIEW (widget));
  last_day = gcal_view_get_final_date (GCAL_VIEW (widget));

  gcal_manager_set_new_range (
      get_manager (window),
      first_day,
      last_day);

  g_free (first_day);
  g_free (last_day);

  header = gcal_view_get_left_header (GCAL_VIEW (widget));
  g_object_set (priv->nav_bar, "left-header", header, NULL);
  g_free (header);

  header = gcal_view_get_right_header (GCAL_VIEW (widget));
  g_object_set (priv->nav_bar, "right-header", header, NULL);
  g_free (header);
}

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

  /* Get view_type from widget, or widget-name */
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

  if (! enabled)
    gcal_view_clear_marks (GCAL_VIEW (priv->views[priv->active_view]));

  /* XXX: here we could disable clicks from the views, yet */
  /* for now we relaunch the new-event widget */
  if (priv->new_event_widget != NULL)
    {
      gtk_widget_destroy (priv->new_event_widget);
      priv->new_event_widget = NULL;
    }
}

static void
prepare_new_event_widget (GcalWindow *window)
{
  GcalWindowPrivate *priv;

  GcalManager *manager;
  gchar *uid;

  struct tm tm_date;
  gchar start[64];
  gchar *title_date;

  GcalNewEventWidget *new_widget;
  GtkWidget *widget;

  priv = gcal_window_get_instance_private (window);

  /* FIXME: ensure destruction or singleton pattern */
  priv->new_event_widget = gcal_new_event_widget_new ();
  new_widget = GCAL_NEW_EVENT_WIDGET (priv->new_event_widget);

  /* setting title */
  tm_date = icaltimetype_to_tm (priv->event_creation_data->start_date);
  e_utf8_strftime_fix_am_pm (start, 64, "%B %d", &tm_date);
  title_date = g_strdup_printf (_("New Event on %s"), start);

  gcal_new_event_widget_set_title (new_widget, title_date);
  g_free (title_date);

  manager = get_manager (window);

  gcal_new_event_widget_set_calendars (new_widget,
                                       GTK_TREE_MODEL (gcal_manager_get_sources_model (manager)));

  uid = gcal_manager_get_default_source (manager);
  gcal_new_event_widget_set_default_calendar (new_widget, uid);
  g_free (uid);

  /* FIXME: add signals handling */
  widget = gcal_new_event_widget_get_close_button (new_widget);
  g_signal_connect (widget, "clicked",
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

  gtk_widget_show_all (priv->new_event_widget);
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

  prepare_new_event_widget (GCAL_WINDOW (user_data));

  gtk_overlay_add_overlay (GTK_OVERLAY (priv->views_overlay),
                           priv->new_event_widget);
}

static gboolean
place_new_event_widget (GtkOverlay   *overlay,
                        GtkWidget    *child,
                        GdkRectangle *allocation,
                        gpointer      user_data)
{
  GcalWindowPrivate *priv;

  gint nat_width;
  gint nat_height;
  gint nav_bar_height;

  priv = gcal_window_get_instance_private (GCAL_WINDOW (user_data));

  if (! priv->new_event_mode)
    return FALSE;

  gtk_widget_get_preferred_width (priv->new_event_widget,
                                  NULL,
                                  &nat_width);
  gtk_widget_get_preferred_height_for_width (priv->new_event_widget,
                                             nat_width,
                                             NULL,
                                             &nat_height);
  nav_bar_height = gtk_widget_get_allocated_height (priv->nav_bar);

  g_debug ("[allocate-child] incoming value (%d, %d) - (%d, %d)",
           allocation->x,
           allocation->y,
           allocation->width,
           allocation->height);
  g_debug ("[allocate-child] natural size (%d, %d)",
           nat_width, nat_height);
  allocation->x = priv->event_creation_data->x - nat_width / 2;
  allocation->y = priv->event_creation_data->y + nav_bar_height - nat_height;
  allocation->width = nat_width;
  allocation->height = nat_height;

  g_debug ("[allocate-child]: outgoing value (%d, %d) - (%d, %d)",
           allocation->x,
           allocation->y,
           allocation->width,
           allocation->height);

  gtk_widget_grab_focus (gcal_new_event_widget_get_entry (
                             GCAL_NEW_EVENT_WIDGET (priv->new_event_widget)));

  return TRUE;
}

static void
close_new_event_widget (GtkButton *button,
                        gpointer   user_data)
{
  set_new_event_mode (GCAL_WINDOW (user_data), FALSE);
}

static GcalManager*
get_manager (GcalWindow *window)
{
  GcalApplication *app;
  app = GCAL_APPLICATION (gtk_window_get_application (GTK_WINDOW (window)));

  return gcal_application_get_manager (app);
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
  gcal_manager_create_event (get_manager (GCAL_WINDOW (user_data)),
                             uid, summary,
                             priv->event_creation_data->start_date,
                             priv->event_creation_data->end_date);

  g_free (uid);
  g_free (summary);
  /* reset and hide */
  set_new_event_mode (GCAL_WINDOW (user_data), FALSE);
}

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

  g_object_class_install_property (
      object_class,
      PROP_NEW_EVENT_MODE,
      g_param_spec_boolean ("new-event-mode",
                            "New Event mode",
                            "Whether the window is in new-event-mode or not",
                            FALSE,
                            G_PARAM_CONSTRUCT |
                            G_PARAM_READWRITE));
}

static void
gcal_window_init(GcalWindow *self)
{
  GcalWindowPrivate *priv;

  priv = gcal_window_get_instance_private (self);

  /* states */
  priv->search_mode = FALSE;

  priv->event_creation_data = NULL;

  /* FIXME: Review real need of this */
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
  priv->new_button = gd_header_simple_button_new ();
  gd_header_button_set_label (GD_HEADER_BUTTON (priv->new_button),
                              _("New Event"));
  gtk_style_context_add_class (
      gtk_widget_get_style_context (priv->new_button),
      "suggested-action");
  gtk_header_bar_pack_start (GTK_HEADER_BAR (priv->header_bar), priv->new_button);

  priv->views_switcher = gtk_stack_switcher_new ();
  g_object_ref_sink (priv->views_switcher);
  gtk_header_bar_set_custom_title (GTK_HEADER_BAR (priv->header_bar),
                                   priv->views_switcher);
  g_object_bind_property (object,
                          "new-event-mode",
                          priv->views_switcher,
                          "sensitive",
                          G_BINDING_DEFAULT | G_BINDING_INVERT_BOOLEAN);

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
  gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (priv->header_bar),
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

  priv->views[GCAL_WINDOW_VIEW_DAY] = gcal_day_view_new ();
  gtk_stack_add_titled (GTK_STACK (priv->views_stack),
                        priv->views[GCAL_WINDOW_VIEW_DAY],
                        "day", _("Day"));
  g_object_bind_property (GCAL_WINDOW (object), "active-date",
                          priv->views[GCAL_WINDOW_VIEW_DAY], "active-date",
                          G_BINDING_DEFAULT);

  priv->views[GCAL_WINDOW_VIEW_WEEK] = gcal_week_view_new ();
  gtk_stack_add_titled (GTK_STACK (priv->views_stack),
                        priv->views[GCAL_WINDOW_VIEW_WEEK],
                        "week", _("Week"));
  g_object_bind_property (GCAL_WINDOW (object), "active-date",
                          priv->views[GCAL_WINDOW_VIEW_WEEK], "active-date",
                          G_BINDING_DEFAULT);

  priv->views[GCAL_WINDOW_VIEW_MONTH] = gcal_month_view_new ();
  gtk_stack_add_titled (GTK_STACK (priv->views_stack),
                        priv->views[GCAL_WINDOW_VIEW_MONTH],
                        "month", _("Month"));
  g_object_bind_property (GCAL_WINDOW (object), "active-date",
                          priv->views[GCAL_WINDOW_VIEW_MONTH], "active-date",
                          G_BINDING_DEFAULT);

  priv->views[GCAL_WINDOW_VIEW_YEAR] = gcal_year_view_new ();
  gtk_stack_add_titled (GTK_STACK (priv->views_stack),
                        priv->views[GCAL_WINDOW_VIEW_YEAR],
                        "year", _("Year"));
  g_object_bind_property (GCAL_WINDOW (object), "active-date",
                          priv->views[GCAL_WINDOW_VIEW_YEAR], "active-date",
                          G_BINDING_DEFAULT);

  gtk_stack_switcher_set_stack (GTK_STACK_SWITCHER (priv->views_switcher),
                                GTK_STACK (priv->views_stack));

  gtk_container_add (GTK_CONTAINER (object), priv->main_box);
  gtk_widget_show_all (priv->main_box);

  gtk_style_context_add_class (
      gtk_widget_get_style_context (GTK_WIDGET (object)),
      "views");

  /* signals connection/handling */
  g_signal_connect (object, "key-press-event",
                    G_CALLBACK (key_pressed), object);

  g_signal_connect_swapped (priv->new_button, "clicked",
                            G_CALLBACK (gcal_window_new_event), object);
  for (i = 0; i < 4; ++i)
    {
      g_signal_connect (priv->views[i], "create-event",
                        G_CALLBACK (show_new_event_widget), object);
    }
  g_signal_connect (priv->views_overlay, "get-child-position",
                    G_CALLBACK (place_new_event_widget), object);

  g_signal_connect (priv->search_bar, "notify::search-mode-enabled",
                    G_CALLBACK (gcal_window_search_toggled), object);
  g_signal_connect (priv->search_entry, "changed",
                    G_CALLBACK (gcal_window_search_changed), object);

  g_signal_connect (priv->views_stack, "notify::visible-child",
                    G_CALLBACK (view_changed), object);
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
      gtk_stack_set_visible_child (GTK_STACK (priv->views_stack),
                                   priv->views[g_value_get_enum (value)]);

      return;
    case PROP_ACTIVE_DATE:
      if (priv->active_date != NULL)
        g_free (priv->active_date);
      priv->active_date = g_value_dup_boxed (value);
      return;
    case PROP_NEW_EVENT_MODE:
      set_new_event_mode (GCAL_WINDOW (object), g_value_get_boolean (value));
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
gcal_window_init_edit_dialog (GcalWindow *window)
{
  GcalWindowPrivate *priv;

  priv = gcal_window_get_instance_private (window);

  priv->edit_dialog = gcal_edit_dialog_new ();
  gtk_window_set_transient_for (GTK_WINDOW (priv->edit_dialog),
                                GTK_WINDOW (window));
  gcal_edit_dialog_set_manager (GCAL_EDIT_DIALOG (priv->edit_dialog),
                                get_manager (window));

  g_signal_connect (priv->edit_dialog,
                    "response",
                    G_CALLBACK (gcal_window_edit_dialog_responded),
                    window);
}

/* GcalManager signal handling */
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

  GcalView *view;
  GtkWidget *event;
  icaltimetype *start_date;
  icaltimetype *end_date;

  priv = gcal_window_get_instance_private (GCAL_WINDOW (user_data));
  view = GCAL_VIEW (priv->views[priv->active_view]);

  for (l = events_list; l != NULL; l = l->next)
    {
      tokens = g_strsplit ((gchar*) l->data, ":", 2);
      source_uid  = tokens[0];
      event_uid = tokens[1];
      start_date = gcal_manager_get_event_start_date (manager,
                                                      source_uid,
                                                      event_uid);
      end_date = gcal_manager_get_event_end_date (manager,
                                                  source_uid,
                                                  event_uid);

      /* FIXME: erase me */
      /* g_debug ("add: %s with date %s", */
      /*          (gchar*) l->data, */
      /*          icaltime_as_ical_string (*start_date)); */

      if (gcal_view_draw_event (view, start_date, end_date) &&
          gcal_view_get_by_uuid (view, (gchar*)l->data) == NULL)
        {
          event = gcal_event_widget_new ((gchar*) l->data);
          gcal_window_update_event_widget (manager,
                                           source_uid,
                                           event_uid,
                                           GCAL_EVENT_WIDGET (event));
          gtk_widget_show (event);
          gtk_container_add (GTK_CONTAINER (view), event);

          g_signal_connect (event,
                            "activate",
                            G_CALLBACK (gcal_window_event_activated),
                            user_data);
        }

      g_free (start_date);
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

  priv = gcal_window_get_instance_private (GCAL_WINDOW (user_data));

  for (l = events_list; l != NULL; l = l->next)
    {
      gint i;
      for (i = 0; i < 5; i++)
        {
          if (priv->views[i] != NULL)
            {
              gtk_widget_destroy (
                  gcal_view_get_by_uuid (GCAL_VIEW (priv->views[i]),
                                         (gchar*) l->data));
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
  icaltimetype *start_date;
  icaltimetype *end_date;

  priv = gcal_window_get_instance_private (GCAL_WINDOW (user_data));

  for (l = events_list; l != NULL; l = l->next)
    {
      gint i;

      tokens = g_strsplit ((gchar*) l->data, ":", 2);
      source_uid  = tokens[0];
      event_uid = tokens[1];

      g_debug ("Object modified from UI on calendar: %s event: %s",
               source_uid,
               event_uid);

      for (i = 0; i < 5; i++)
        {
          if (priv->views[i] == NULL)
            continue;

          widget = gcal_view_get_by_uuid (GCAL_VIEW (priv->views[i]),
                                          (gchar*) l->data);

          if (widget != NULL)
            {
              start_date = gcal_manager_get_event_start_date (manager,
                                                              source_uid,
                                                              event_uid);
              end_date = gcal_manager_get_event_end_date (manager,
                                                          source_uid,
                                                          event_uid);

              gtk_widget_destroy (widget);

              if (gcal_view_draw_event (GCAL_VIEW (priv->views[i]),
                                        start_date, end_date))
                {
                  GtkWidget *event;
                  event = gcal_event_widget_new ((gchar*) l->data);
                  gcal_window_update_event_widget (manager,
                                                   source_uid,
                                                   event_uid,
                                                   GCAL_EVENT_WIDGET (event));
                  gtk_widget_show (event);
                  gtk_container_add (GTK_CONTAINER (priv->views[i]),
                                     event);
                  g_signal_connect (event,
                                    "activate",
                                    G_CALLBACK (gcal_window_event_activated),
                                    user_data);
                }

              g_free (start_date);
              g_free (end_date);
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

  priv = gcal_window_get_instance_private (GCAL_WINDOW (user_data));

  if (! priv->open_edit_dialog)
    return;

  priv->open_edit_dialog = FALSE;

  if (priv->edit_dialog == NULL)
    gcal_window_init_edit_dialog (GCAL_WINDOW (user_data));

  gcal_edit_dialog_set_event (GCAL_EDIT_DIALOG (priv->edit_dialog),
                              source_uid,
                              event_uid);

  gtk_dialog_run (GTK_DIALOG (priv->edit_dialog));
}

static void
gcal_window_event_activated (GcalEventWidget *event_widget,
                             gpointer         user_data)
{
  GcalWindowPrivate *priv;
  gchar **tokens;

  priv = gcal_window_get_instance_private (GCAL_WINDOW (user_data));

  if (priv->edit_dialog == NULL)
    gcal_window_init_edit_dialog (GCAL_WINDOW (user_data));

  tokens = g_strsplit (gcal_event_widget_peek_uuid (event_widget), ":", 2);
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

  priv = gcal_window_get_instance_private (GCAL_WINDOW (user_data));

  if (priv->event_to_delete != NULL)
    {
      manager = get_manager (GCAL_WINDOW (user_data));
      tokens = g_strsplit (priv->event_to_delete, ":", 2);

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

static void
gcal_window_edit_dialog_responded (GtkDialog *dialog,
                                   gint       response,
                                   gpointer   user_data)
{
  GcalWindowPrivate *priv;

  GcalManager *manager;
  GList *changed_props;
  GList *l;

  GtkWidget *grid;
  GtkWidget *undo_button;

  priv = gcal_window_get_instance_private (GCAL_WINDOW (user_data));

  gtk_widget_hide (priv->edit_dialog);

  switch (response)
    {
      case GTK_RESPONSE_CANCEL:
        /* do nothing, nor edit, nor nothing */
        break;
      case GTK_RESPONSE_ACCEPT:
        /* save changes if editable */
        manager = get_manager (GCAL_WINDOW (user_data));
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

        undo_button = gtk_button_new_with_label (_("Undo"));
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

        /* hide widget of the event */
        gtk_widget_hide (
            gcal_view_get_by_uuid (GCAL_VIEW (priv->views[priv->active_view]),
                                   priv->event_to_delete));
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
  icaltimetype *date;

  summary = gcal_manager_get_event_summary (manager,
                                            source_uid,
                                            event_uid);
  gcal_event_widget_set_summary (widget, summary);

  color = gcal_manager_get_event_color (manager,
                                        source_uid,
                                        event_uid);
  gcal_event_widget_set_color (widget, color);

  date = gcal_manager_get_event_start_date (manager,
                                            source_uid,
                                            event_uid);
  gcal_event_widget_set_date (widget, date);

  g_free (date);
  date = gcal_manager_get_event_end_date (manager,
                                          source_uid,
                                          event_uid);
  gcal_event_widget_set_end_date (widget, date);

  gcal_event_widget_set_all_day (
      widget,
      gcal_manager_get_event_all_day (manager, source_uid, event_uid));

  gcal_event_widget_set_has_reminders (
      widget,
      gcal_manager_has_event_reminders (manager, source_uid, event_uid));

  g_free (summary);
  gdk_rgba_free (color);
  g_free (date);
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

  win  =  g_object_new (GCAL_TYPE_WINDOW,
                        "application",
                        GTK_APPLICATION (app),
                        "active-date",
                        &date,
                        NULL);

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

  /* init hack */
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
      break;
    }

  prepare_new_event_widget (GCAL_WINDOW (window));

  gtk_overlay_add_overlay (GTK_OVERLAY (priv->views_overlay),
                           priv->new_event_widget);
}

void
gcal_window_set_search_mode (GcalWindow *window,
                             gboolean    enabled)
{
  GcalWindowPrivate *priv;

  priv = gcal_window_get_instance_private (window);
  gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (priv->search_bar), enabled);
}

void
gcal_window_show_notification (GcalWindow *window)
{
  GcalWindowPrivate *priv;

  priv = gcal_window_get_instance_private (window);
  gtk_overlay_add_overlay (GTK_OVERLAY (priv->views_overlay),
                           priv->noty);
  gtk_widget_show_all (priv->noty);
}

void
gcal_window_hide_notification (GcalWindow *window)
{
  GcalWindowPrivate *priv;

  priv = gcal_window_get_instance_private (window);
  gd_notification_dismiss (GD_NOTIFICATION (priv->noty));
}
