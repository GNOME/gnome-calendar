/* -*- mode: c; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gcal-window.c
 * Copyright (C) 2015 Erick Pérez Castellanos <erickpc@gnome.org>
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

#include "gcal-manager.h"
#include "gcal-view.h"
#include "gcal-week-view.h"
#include "gcal-month-view.h"
#include "gcal-year-view.h"
#include "gcal-search-view.h"
#include "gcal-event-widget.h"
#include "gcal-edit-dialog.h"
#include "gcal-enum-types.h"
#include "gcal-quick-add-popover.h"
#include "gcal-source-dialog.h"

#include <glib/gi18n.h>

#include <libecal/libecal.h>
#include <libical/icaltime.h>

typedef struct
{
  gint               x;
  gint               y;
  GDateTime         *start_date;
  GDateTime         *end_date;
} NewEventData;

typedef struct
{
  GcalWindow *window;
  gchar *uuid;
} OpenEditDialogData;

struct _GcalWindow
{
  GtkApplicationWindow parent;

  /* timeout ids */
  guint                save_geometry_timeout_id;
  guint                notification_timeout;

  /* upper level widgets */
  GtkWidget           *main_box;

  GtkWidget           *header_bar;
  GtkWidget           *search_bar;
  GtkWidget           *views_overlay;
  GtkWidget           *views_stack;
  GtkWidget           *week_view;
  GtkWidget           *month_view;
  GtkWidget           *year_view;
  GtkWidget           *notification;
  GtkWidget           *notification_label;
  GtkWidget           *notification_action_button;
  GtkWidget           *notification_close_button;

  /* header_bar widets */
  GtkWidget           *menu_button;
  GtkWidget           *search_button;
  GtkWidget           *calendars_button;
  GtkWidget           *search_entry;
  GtkWidget           *back_button;
  GtkWidget           *today_button;
  GtkWidget           *forward_button;
  GtkWidget           *views_switcher;

  /* new event popover widgets */
  GtkWidget           *quick_add_popover;

  GtkWidget           *search_view;

  /* day, week, month, year, list */
  GtkWidget           *views [6];
  GtkWidget           *edit_dialog;

  GcalManager         *manager;
  GcalWindowViewType   active_view;
  icaltimetype        *active_date;

  icaltimetype        *current_date;
  gboolean             rtl;

  /* states */
  gboolean             new_event_mode;
  gboolean             search_mode;

  NewEventData        *event_creation_data;

  GcalEvent           *event_to_delete;

  /* calendar management */
  GtkWidget           *calendar_popover;
  GtkWidget           *calendar_listbox;
  GtkWidget           *source_dialog;
  gint                 refresh_timeout;
  gint                 refresh_timeout_id;
  gint                 open_edit_dialog_timeout_id;

  /* temp to keep event_creation */
  gboolean             open_edit_dialog;
};

enum
{
  PROP_0,
  PROP_ACTIVE_VIEW,
  PROP_MANAGER,
  PROP_ACTIVE_DATE,
  PROP_NEW_EVENT_MODE
};

#define SAVE_GEOMETRY_ID_TIMEOUT 100 /* ms */
#define FAST_REFRESH_TIMEOUT     900000 /* ms */
#define SLOW_REFRESH_TIMEOUT     3600000 /* ms */

#define gcal_window_add_accelerator(app,action,accel) {\
  const gchar *tmp[] = {accel, NULL};\
  gtk_application_set_accels_for_action (GTK_APPLICATION (app), action, tmp);\
}


static void           on_show_calendars_action_activated (GSimpleAction       *action,
                                                          GVariant            *param,
                                                          gpointer             user_data);

static void           on_date_action_activated           (GSimpleAction       *action,
                                                          GVariant            *param,
                                                          gpointer             user_data);

static void           on_view_action_activated           (GSimpleAction       *action,
                                                          GVariant            *param,
                                                          gpointer             user_data);

static gboolean       key_pressed                        (GtkWidget           *widget,
                                                          GdkEvent            *event,
                                                          gpointer             user_data);

static void           update_today_button_sensitive      (GcalWindow          *window);

static void           date_updated                       (GtkButton           *buttton,
                                                          gpointer             user_data);

static void           search_event_selected              (GcalSearchView      *search_view,
                                                          icaltimetype        *date,
                                                          gpointer             user_data);

static gint           calendar_listbox_sort_func         (GtkListBoxRow       *row1,
                                                          GtkListBoxRow       *row2,
                                                          gpointer             user_data);

static void           load_geometry                      (GcalWindow          *window);

static gboolean       save_geometry                      (gpointer             user_data);

static void           view_changed                       (GObject             *object,
                                                          GParamSpec          *pspec,
                                                          gpointer             user_data);

static void           set_new_event_mode                 (GcalWindow          *window,
                                                          gboolean             enabled);

static void           show_new_event_widget              (GcalView            *view,
                                                          gpointer             start_span,
                                                          gpointer             end_span,
                                                          gdouble              x,
                                                          gdouble              y,
                                                          GcalWindow          *window);

static void           close_new_event_widget             (GtkButton           *button,
                                                          gpointer             user_data);

static void           create_notification                (GcalWindow          *window,
                                                          gchar               *message,
                                                          gchar               *button_label);

static void           hide_notification                  (GcalWindow          *window,
                                                          GtkWidget           *button);

/* calendar management */
static void           add_source                         (GcalManager         *manager,
                                                          ESource             *source,
                                                          gboolean             enabled,
                                                          gpointer             user_data);


static GtkWidget*     make_row_from_source               (GcalWindow          *window,
                                                          ESource             *source);

static void           remove_source                      (GcalManager         *manager,
                                                          ESource             *source,
                                                          gpointer             user_data);

static void           source_row_activated               (GtkListBox          *listbox,
                                                          GtkListBoxRow       *row,
                                                          gpointer             user_data);

static void           on_calendar_toggled                (GObject             *object,
                                                          GParamSpec          *pspec,
                                                          gpointer             user_data);

static gboolean       refresh_sources                    (GcalWindow          *window);

static gboolean       window_state_changed               (GtkWidget           *window,
                                                          GdkEvent            *event,
                                                          gpointer             user_data);

/* handling events interaction */
static void           edit_event                         (GcalQuickAddPopover *popover,
                                                          GcalEvent           *event,
                                                          GcalWindow          *self);

static void           create_event_detailed_cb           (GcalView            *view,
                                                          gpointer             start_span,
                                                          gpointer             end_span,
                                                          gpointer             user_data);

static void           event_activated                    (GcalView            *view,
                                                          GcalEventWidget     *event_widget,
                                                          gpointer             user_data);

static void           edit_dialog_closed                 (GtkDialog           *dialog,
                                                          gint                 response,
                                                          gpointer             user_data);

static void           search_toggled                     (GObject             *object,
                                                          GParamSpec          *pspec,
                                                          gpointer             user_data);

static void           search_changed                     (GtkEditable         *editable,
                                                          gpointer             user_data);

static void           remove_event                       (GtkWidget           *notification,
                                                          GParamSpec          *spec,
                                                          gpointer             user_data);

static void           undo_remove_action                 (GtkButton           *button,
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

G_DEFINE_TYPE (GcalWindow, gcal_window, GTK_TYPE_APPLICATION_WINDOW)

static const GActionEntry actions[] = {
  {"next",     on_date_action_activated },
  {"previous", on_date_action_activated },
  {"today",    on_date_action_activated },
  {"change-view", on_view_action_activated, "i" },
  {"show-calendars", on_show_calendars_action_activated },
};

static void
on_show_calendars_action_activated (GSimpleAction *action,
                                    GVariant      *param,
                                    gpointer       user_data)
{
  GcalWindow *window = GCAL_WINDOW (user_data);

  gcal_source_dialog_set_mode (GCAL_SOURCE_DIALOG (window->source_dialog), GCAL_SOURCE_DIALOG_MODE_NORMAL);

  gtk_widget_hide (window->calendar_popover);

  gtk_widget_show (window->source_dialog);
}

static void
on_date_action_activated (GSimpleAction *action,
                          GVariant      *param,
                          gpointer       user_data)
{
  GcalWindow *window;
  const gchar *action_name;

  g_return_if_fail (GCAL_IS_WINDOW (user_data));

  window = GCAL_WINDOW (user_data);
  action_name = g_action_get_name (G_ACTION (action));

  if (g_strcmp0 (action_name, "next") == 0)
    date_updated (GTK_BUTTON (window->forward_button), user_data);
  else if (g_strcmp0 (action_name, "previous") == 0)
    date_updated (GTK_BUTTON (window->back_button), user_data);
  else if (g_strcmp0 (action_name, "today") == 0)
    date_updated (GTK_BUTTON (window->today_button), user_data);
}

static void
on_view_action_activated (GSimpleAction *action,
                          GVariant      *param,
                          gpointer       user_data)
{
  GcalWindow *window = GCAL_WINDOW (user_data);
  guint view;

  view = g_variant_get_int32 (param);

  // -1 means next view
  if (view == -1)
    view = ++(window->active_view);
  else if (view == -2)
    view = --(window->active_view);

  window->active_view = CLAMP (view, GCAL_WINDOW_VIEW_MONTH, GCAL_WINDOW_VIEW_YEAR);
  gtk_stack_set_visible_child (GTK_STACK (window->views_stack), window->views[window->active_view]);

  g_object_notify (G_OBJECT (user_data), "active-view");
}

static gboolean
key_pressed (GtkWidget *widget,
             GdkEvent  *event,
             gpointer   user_data)
{
  GcalWindow *window = GCAL_WINDOW (user_data);

  /* special case: creating an event */
  if (window->new_event_mode)
    return GDK_EVENT_PROPAGATE;

  return gtk_search_bar_handle_event (GTK_SEARCH_BAR (window->search_bar),
                                      event);
}

static void
update_active_date (GcalWindow   *window,
                    icaltimetype *new_date)
{
  GDateTime *date_start, *date_end;
  time_t range_start, range_end;
  icaltimetype *previous_date;
  GDate old_week, new_week;

  previous_date = window->active_date;
  window->active_date = new_date;
  g_object_notify (G_OBJECT (window), "active-date");

  /* year_view */
  if (previous_date->year != new_date->year)
    {
      date_start = g_date_time_new_local (new_date->year, 1, 1, 0, 0, 0);
      range_start = g_date_time_to_unix (date_start);

      date_end = g_date_time_add_years (date_start, 1);
      range_end = g_date_time_to_unix (date_end);

      gcal_manager_set_subscriber (window->manager, E_CAL_DATA_MODEL_SUBSCRIBER (window->year_view), range_start, range_end);

      g_clear_pointer (&date_start, g_date_time_unref);
      g_clear_pointer (&date_end, g_date_time_unref);
    }

  /* month_view */
  if (previous_date->month != new_date->month || previous_date->year != new_date->year)
    {
      date_start = g_date_time_new_local (new_date->year, new_date->month, 1, 0, 0, 0);
      range_start = g_date_time_to_unix (date_start);

      date_end = g_date_time_add_months (date_start, 1);
      range_end = g_date_time_to_unix (date_end);

      gcal_manager_set_subscriber (window->manager, E_CAL_DATA_MODEL_SUBSCRIBER (window->month_view), range_start, range_end);

      g_clear_pointer (&date_start, g_date_time_unref);
      g_clear_pointer (&date_end, g_date_time_unref);
    }

  /* week_view */
  g_date_set_dmy (&old_week, previous_date->day, previous_date->month, previous_date->year);
  g_date_set_dmy (&new_week, new_date->day, new_date->month, new_date->year);

  if (previous_date->year != new_date->year ||
      g_date_get_iso8601_week_of_year (&old_week) != g_date_get_iso8601_week_of_year(&new_week))
    {
      date_start = g_date_time_new_local (new_date->year, new_date->month, new_date->day, 0, 0, 0);
      date_start = g_date_time_add_days (date_start, (get_first_weekday() - g_date_time_get_day_of_week (date_start) + 7) % 7 - 8);
      range_start = g_date_time_to_unix (date_start);

      date_end = g_date_time_add_days (date_start, 6);
      range_end = g_date_time_to_unix (date_end);

      gcal_manager_set_subscriber (window->manager, E_CAL_DATA_MODEL_SUBSCRIBER (window->week_view), range_start, range_end);

      g_clear_pointer (&date_start, g_date_time_unref);
      g_clear_pointer (&date_end, g_date_time_unref);
    }

  update_today_button_sensitive (window);

  g_free (previous_date);
}

static gboolean
update_current_date (GcalWindow *window)
{
  guint seconds;

  if (window->current_date == NULL)
    window->current_date = g_new0 (icaltimetype, 1);

  *(window->current_date) = icaltime_current_time_with_zone (gcal_manager_get_system_timezone (window->manager));
  *(window->current_date) = icaltime_set_timezone (window->current_date, gcal_manager_get_system_timezone (window->manager));

  gcal_month_view_set_current_date (GCAL_MONTH_VIEW (window->month_view), window->current_date);
  gcal_year_view_set_current_date (GCAL_YEAR_VIEW (window->year_view), window->current_date);

  seconds = 24 * 60 * 60 - (icaltime_as_timet (*(window->current_date)) % (24 * 60 * 60));
  g_timeout_add_seconds (seconds, (GSourceFunc) update_current_date, window);
  return FALSE;
}

static void
update_today_button_sensitive (GcalWindow *window)
{
  gboolean sensitive;

  switch (window->active_view)
    {
    case GCAL_WINDOW_VIEW_DAY:
      sensitive = window->active_date->year != window->current_date->year ||
                  window->active_date->month != window->current_date->month ||
                  window->active_date->day != window->current_date->day;
      break;

    case GCAL_WINDOW_VIEW_WEEK:
      /* TODO: determine when the today button would be sensitive on week view */
      sensitive = TRUE;
      break;

    case GCAL_WINDOW_VIEW_MONTH:
      sensitive = window->active_date->year != window->current_date->year ||
                  window->active_date->month != window->current_date->month;
      break;

    case GCAL_WINDOW_VIEW_YEAR:
      sensitive = window->active_date->year != window->current_date->year;
      break;

    case GCAL_WINDOW_VIEW_LIST:
    case GCAL_WINDOW_VIEW_SEARCH:
    default:
      sensitive = TRUE;
      break;
    }

  gtk_widget_set_sensitive (window->today_button, sensitive);
}

static void
date_updated (GtkButton  *button,
              gpointer    user_data)
{
  GcalWindow *window = GCAL_WINDOW (user_data);

  icaltimetype *new_date;
  gboolean move_back, move_today;
  gint factor;

  factor = window->rtl ? - 1 : 1;

  move_today = window->today_button == (GtkWidget*) button;
  move_back = window->back_button == (GtkWidget*) button;

  new_date = gcal_dup_icaltime (window->active_date);

  if (move_today)
    {
      *new_date = *(window->current_date);
    }
  else
    {
      switch (window->active_view)
        {
        case GCAL_WINDOW_VIEW_DAY:
          new_date->day += 1 * factor * (move_back ? -1 : 1);
          break;
        case GCAL_WINDOW_VIEW_WEEK:
          new_date->day += 7 * factor * (move_back ? -1 : 1);
          break;
        case GCAL_WINDOW_VIEW_MONTH:
          new_date->day = 1;
          new_date->month += 1 * factor * (move_back ? -1 : 1);
          break;
        case GCAL_WINDOW_VIEW_YEAR:
          new_date->year += 1 * factor * (move_back ? -1 : 1);
          break;
        case GCAL_WINDOW_VIEW_LIST:
        case GCAL_WINDOW_VIEW_SEARCH:
        default:
          break;
        }

      *new_date = icaltime_normalize (*new_date);
    }

  update_active_date (user_data, new_date);
}

static void
search_event_selected (GcalSearchView *search_view,
                       icaltimetype   *date,
                       gpointer        user_data)
{
  g_object_set (user_data, "active-date", date, NULL);
  gcal_window_set_search_mode (GCAL_WINDOW (user_data), FALSE);
}

static gint
calendar_listbox_sort_func (GtkListBoxRow *row1,
                            GtkListBoxRow *row2,
                            gpointer       user_data)
{
  ESource *source1, *source2;

  source1 = g_object_get_data (G_OBJECT (row1), "source");
  source2 = g_object_get_data (G_OBJECT (row2), "source");

  if (source1 == NULL && source2 == NULL)
    return 0;

  return g_strcmp0 (e_source_get_display_name (source1), e_source_get_display_name (source2));
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
  GcalWindow *window;
  GtkWindow *self;
  GdkWindow *win;
  GdkWindowState state;
  GcalApplication *app;
  GSettings *settings;
  gboolean maximized;
  GVariant *variant;
  gint32 size[2];
  gint32 position[2];

  self = GTK_WINDOW (user_data);
  window = GCAL_WINDOW (self);
  win = gtk_widget_get_window (GTK_WIDGET (self));
  state = gdk_window_get_state (win);


  app = GCAL_APPLICATION (gtk_window_get_application (self));
  settings = gcal_application_get_settings (app);

  /* save window's state */
  maximized = state & GDK_WINDOW_STATE_MAXIMIZED;
  g_settings_set_boolean (settings,
                          "window-maximized",
                          maximized);

  if (maximized)
    {
      window->save_geometry_timeout_id = 0;
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

  window->save_geometry_timeout_id = 0;

  return FALSE;
}

/**
 * view_changed:
 * @object:
 * @pspec:
 * @user_data:
 *
 * Called every time the user activate the stack-switcher
 * Retrieve the enum value representing the view, update internal
 * @active_view with it
 **/
static void
view_changed (GObject    *object,
              GParamSpec *pspec,
              gpointer    user_data)
{
  GcalWindow *window;
  GEnumClass *eklass;
  GEnumValue *eval;
  GcalWindowViewType view_type;

  window = GCAL_WINDOW (user_data);

  /* XXX: this is the destruction process */
  if (!gtk_widget_get_visible (window->views_stack))
    return;

  eklass = g_type_class_ref (gcal_window_view_type_get_type ());
  eval = g_enum_get_value_by_nick (
             eklass,
             gtk_stack_get_visible_child_name (GTK_STACK (window->views_stack)));

  view_type = eval->value;

  g_type_class_unref (eklass);

  if (view_type == GCAL_WINDOW_VIEW_SEARCH)
    return;

  window->active_view = view_type;
  update_today_button_sensitive (window);
  g_object_notify (G_OBJECT (user_data), "active-view");
}

static void
set_new_event_mode (GcalWindow *window,
                    gboolean    enabled)
{
  window->new_event_mode = enabled;
  g_object_notify (G_OBJECT (window), "new-event-mode");

  if (! enabled && window->views[window->active_view] != NULL)
    gcal_view_clear_marks (GCAL_VIEW (window->views[window->active_view]));

  /* XXX: here we could disable clicks from the views, yet */
  /* for now we relaunch the new-event widget */
  if (!enabled && gtk_widget_is_visible (window->quick_add_popover))
    {
      gtk_widget_set_visible (window->quick_add_popover, FALSE);
    }
}

/* new-event interaction: second variant */
static void
show_new_event_widget (GcalView   *view,
                       gpointer    start_span,
                       gpointer    end_span,
                       gdouble     x,
                       gdouble     y,
                       GcalWindow *window)
{
  GdkRectangle rect;
  gint out_x, out_y;

  /* 1st and 2nd steps */
  set_new_event_mode (window, TRUE);

  if (window->event_creation_data != NULL)
    {
      g_clear_pointer (&window->event_creation_data->start_date, g_date_time_unref);
      g_clear_pointer (&window->event_creation_data->end_date, g_date_time_unref);
      g_clear_pointer (&window->event_creation_data, g_free);
    }

  window->event_creation_data = g_new0 (NewEventData, 1);
  window->event_creation_data->x = x;
  window->event_creation_data->y = y;
  window->event_creation_data->start_date = g_date_time_ref (start_span);
  if (end_span != NULL)
    window->event_creation_data->end_date = g_date_time_ref (end_span);
  g_debug ("[show_new_event] position (%f, %f)", x, y);

  /* Setup the quick add popover's dates */
  gcal_quick_add_popover_set_date_start (GCAL_QUICK_ADD_POPOVER (window->quick_add_popover), start_span);
  gcal_quick_add_popover_set_date_end (GCAL_QUICK_ADD_POPOVER (window->quick_add_popover), end_span);

  /* Position and place the quick add popover */
  gtk_widget_translate_coordinates (window->views[window->active_view], window->views_stack, x, y, &out_x, &out_y);

  /* Place popover over the given (x,y) position */
  rect.x = out_x;
  rect.y = out_y;
  rect.width = 1;
  rect.height = 1;

  gtk_popover_set_pointing_to (GTK_POPOVER (window->quick_add_popover), &rect);
  gtk_widget_show (window->quick_add_popover);
}

static void
close_new_event_widget (GtkButton *button,
                        gpointer   user_data)
{
  set_new_event_mode (GCAL_WINDOW (user_data), FALSE);
}

/**
 * create_notification: Internal method for creating a notification
 * @window:
 * @message: The label it goes into the message part
 * @button_label: (allow-none): The label of the actionable button
 *
 **/
static void
create_notification (GcalWindow *window,
                     gchar      *message,
                     gchar      *button_label)
{
  /* notification content */
  gtk_label_set_markup (GTK_LABEL (window->notification_label), message);
  gtk_widget_show_all (window->notification);

  if (button_label != NULL)
    {
      gtk_button_set_label (GTK_BUTTON (window->notification_action_button),
                            button_label);
      gtk_widget_show (window->notification_action_button);
    }
  else
    {
      gtk_widget_hide (window->notification_action_button);
    }
}

static void
hide_notification (GcalWindow *window,
                   GtkWidget  *button)
{
  gtk_revealer_set_reveal_child (GTK_REVEALER (window->notification), FALSE);
  window->notification_timeout = 0;
}

static gboolean
hide_notification_scheduled (gpointer window)
{
  hide_notification (GCAL_WINDOW (window), NULL);
  return FALSE;
}

static void
add_source (GcalManager *manager,
            ESource     *source,
            gboolean     enabled,
            gpointer     user_data)
{
  GcalWindow *window;
  GtkWidget *row;

  window = GCAL_WINDOW (user_data);

  row = make_row_from_source (GCAL_WINDOW (user_data), source);

  gtk_container_add (GTK_CONTAINER (window->calendar_listbox), row);
}

/**
 * make_row_from_source:
 *
 * Create a GtkListBoxRow for a given
 * ESource.
 *
 * Returns: (transfer full) the new row
 */
static GtkWidget*
make_row_from_source (GcalWindow *window,
                      ESource    *source)
{
  GtkWidget *label, *icon, *checkbox, *box, *row;
  GtkStyleContext *context;
  GdkPixbuf *pixbuf;
  GdkRGBA color;

  row = gtk_list_box_row_new ();

  /* apply some nice styling */
  context = gtk_widget_get_style_context (row);
  gtk_style_context_add_class (context, "button");
  gtk_style_context_add_class (context, "flat");
  gtk_style_context_add_class (context, "menuitem");

  /* main box */
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_container_set_border_width (GTK_CONTAINER (box), 6);

  /* source color icon */
  get_color_name_from_source (source, &color);
  pixbuf = get_circle_pixbuf_from_color (&color, 16);
  icon = gtk_image_new_from_pixbuf (pixbuf);

  gtk_style_context_add_class (gtk_widget_get_style_context (icon), "calendar-color-image");

  /* source name label */
  label = gtk_label_new (e_source_get_display_name (source));
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_widget_set_hexpand (label, TRUE);

  /* checkbox */
  checkbox = gtk_check_button_new ();
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbox), gcal_manager_source_enabled (window->manager, source));
  g_signal_connect (checkbox, "notify::active", G_CALLBACK (on_calendar_toggled), window);

  gtk_container_add (GTK_CONTAINER (box), icon);
  gtk_container_add (GTK_CONTAINER (box), label);
  gtk_container_add (GTK_CONTAINER (box), checkbox);
  gtk_container_add (GTK_CONTAINER (row), box);

  g_object_set_data (G_OBJECT (row), "check", checkbox);
  g_object_set_data (G_OBJECT (row), "source", source);

  gtk_widget_show_all (row);

  g_object_unref (pixbuf);

  return row;
}

static void
remove_source (GcalManager *manager,
               ESource     *source,
               gpointer     user_data)
{
  GcalWindow *window = GCAL_WINDOW (user_data);
  GList *children, *aux;

  children = gtk_container_get_children (GTK_CONTAINER (window->calendar_listbox));

  for (aux = children; aux != NULL; aux = aux->next)
    {
      ESource *child_source = g_object_get_data (G_OBJECT (aux->data), "source");

      if (child_source != NULL && child_source == source)
        {
          gtk_widget_destroy (aux->data);
          break;
        }
    }

  g_list_free (children);
}

static void
source_row_activated (GtkListBox    *listbox,
                      GtkListBoxRow *row,
                      gpointer       user_data)
{
  GtkWidget *check;

  check = g_object_get_data (G_OBJECT (row), "check");

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check),
                                !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check)));
}

static void
source_enabled (GcalWindow *self,
                ESource    *source,
                gboolean    enabled)
{
  GList *children, *aux;

  children = gtk_container_get_children (GTK_CONTAINER (self->calendar_listbox));

  for (aux = children; aux != NULL; aux = aux->next)
    {
      ESource *child_source = g_object_get_data (G_OBJECT (aux->data), "source");

      if (child_source != NULL && child_source == source)
        {
          GtkToggleButton *button = g_object_get_data (G_OBJECT (aux->data), "check");

          g_signal_handlers_block_by_func (button, on_calendar_toggled, self);
          gtk_toggle_button_set_active (button, enabled);
          g_signal_handlers_unblock_by_func (button, on_calendar_toggled, self);

          break;
        }
    }

  g_list_free (children);
}

static void
source_changed (GcalWindow *window,
                ESource    *source)
{
  GList *children, *aux;

  children = gtk_container_get_children (GTK_CONTAINER (window->calendar_listbox));

  for (aux = children; aux != NULL; aux = aux->next)
    {
      ESource *child_source = g_object_get_data (G_OBJECT (aux->data), "source");

      /*
       * Here we destroy the row and re-add it because there is no
       * way to actually bind the ::enabled state of source. It behaves
       * like a construct-only property, and we need to re-create the
       * row if there's any changes.
       */
      if (child_source != NULL && child_source == source)
        {
          gtk_widget_destroy (aux->data);
          add_source (window->manager, source, gcal_manager_source_enabled (window->manager, source), window);
          break;
        }
    }

  g_list_free (children);
}

static void
on_calendar_toggled (GObject    *object,
                     GParamSpec *pspec,
                     gpointer    user_data)
{
  GcalWindow *window;
  gboolean active;
  GtkWidget *row;
  ESource *source;

  window = GCAL_WINDOW (user_data);
  active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (object));
  row = gtk_widget_get_parent (gtk_widget_get_parent (GTK_WIDGET (object)));
  source = g_object_get_data (G_OBJECT (row), "source");

  if (source == NULL)
    return;

  /* Enable/disable the toggled calendar */
  if (active)
    gcal_manager_enable_source (window->manager, source);
  else
    gcal_manager_disable_source (window->manager, source);
}

static gboolean
refresh_sources (GcalWindow *window)
{
  static gint current_timeout = FAST_REFRESH_TIMEOUT;

  /* refresh sources */
  gcal_manager_refresh (window->manager);

  /* check window state */
  if (current_timeout != window->refresh_timeout)
    {
      current_timeout = window->refresh_timeout;

      window->refresh_timeout_id = g_timeout_add (window->refresh_timeout, (GSourceFunc) refresh_sources, window);

      return G_SOURCE_REMOVE;
    }

  return G_SOURCE_CONTINUE;
}

static gboolean
window_state_changed (GtkWidget *widget,
                      GdkEvent  *event,
                      gpointer   user_data)
{
  GcalWindow *window;
  GdkEventWindowState *state;
  gboolean active;

  window = GCAL_WINDOW (widget);
  state = (GdkEventWindowState*) event;
  active = (state->new_window_state & GDK_WINDOW_STATE_FOCUSED);

  /* update timeout time according to the state */
  window->refresh_timeout = (active ? FAST_REFRESH_TIMEOUT : SLOW_REFRESH_TIMEOUT);

  return FALSE;
}

static void
edit_event (GcalQuickAddPopover *popover,
            GcalEvent           *event,
            GcalWindow          *window)
{
  gcal_edit_dialog_set_event_is_new (GCAL_EDIT_DIALOG (window->edit_dialog), TRUE);
  gcal_edit_dialog_set_event (GCAL_EDIT_DIALOG (window->edit_dialog), event);

  gtk_widget_show (window->edit_dialog);
}

static void
create_event_detailed_cb (GcalView *view,
                          gpointer  start_span,
                          gpointer  end_span,
                          gpointer  user_data)
{
  GcalWindow *window = GCAL_WINDOW (user_data);
  ECalComponent *comp;
  GcalEvent *event;

  comp = build_component_from_details ("", start_span, end_span);
  event = gcal_event_new (gcal_manager_get_default_source (window->manager), comp, NULL);

  gcal_edit_dialog_set_event_is_new (GCAL_EDIT_DIALOG (window->edit_dialog), TRUE);
  gcal_edit_dialog_set_event (GCAL_EDIT_DIALOG (window->edit_dialog), event);

  gtk_widget_show (window->edit_dialog);

  g_clear_object (&comp);
}

static void
event_activated (GcalView        *view,
                 GcalEventWidget *event_widget,
                 gpointer         user_data)
{
  GcalWindow *window = GCAL_WINDOW (user_data);
  GcalEvent *event;

  event = gcal_event_widget_get_event (event_widget);
  gcal_edit_dialog_set_event_is_new (GCAL_EDIT_DIALOG (window->edit_dialog), FALSE);
  gcal_edit_dialog_set_event (GCAL_EDIT_DIALOG (window->edit_dialog), event);

  gtk_widget_show (window->edit_dialog);
}

static void
edit_dialog_closed (GtkDialog *dialog,
                    gint       response,
                    gpointer   user_data)
{
  GcalWindow *window;
  GcalEditDialog *edit_dialog;
  GcalEvent *event;
  GcalView *view;
  GList *widgets;

  window = GCAL_WINDOW (user_data);
  edit_dialog = GCAL_EDIT_DIALOG (dialog);
  event = gcal_edit_dialog_get_event (edit_dialog);
  view = GCAL_VIEW (window->views[window->active_view]);

  gtk_widget_hide (GTK_WIDGET (dialog));

  switch (response)
    {
    case GCAL_RESPONSE_CREATE_EVENT:
      gcal_manager_create_event (window->manager, event);
      break;

    case GCAL_RESPONSE_SAVE_EVENT:
      gcal_manager_update_event (window->manager, event);
      break;

    case GCAL_RESPONSE_DELETE_EVENT:
      if (window->event_to_delete != NULL)
        {
          gcal_manager_remove_event (window->manager, window->event_to_delete);
          g_clear_object (&window->event_to_delete);

          create_notification (GCAL_WINDOW (user_data), _("Another event deleted"), _("Undo"));
        }
      else
        {
          create_notification (GCAL_WINDOW (user_data), _("Event deleted"), _("Undo"));
        }

      gtk_revealer_set_reveal_child (GTK_REVEALER (window->notification), TRUE);

      if (window->notification_timeout != 0)
        g_source_remove (window->notification_timeout);

      window->notification_timeout = g_timeout_add_seconds (5, hide_notification_scheduled, user_data);

      g_set_object (&window->event_to_delete, event);

      /* hide widget of the event */
      widgets = gcal_view_get_children_by_uuid (view, gcal_event_get_uid (event));

      g_list_foreach (widgets, (GFunc) gtk_widget_hide, NULL);
      g_list_free (widgets);
      break;

    case GTK_RESPONSE_CANCEL:
    default:
      break;

    }
}

static void
search_toggled (GObject    *object,
                GParamSpec *pspec,
                gpointer    user_data)
{
  GcalWindow *window = GCAL_WINDOW (user_data);

  /* update header_bar widget */
  if (gtk_search_bar_get_search_mode (GTK_SEARCH_BAR (window->search_bar)))
    gcal_search_view_search (GCAL_SEARCH_VIEW (window->search_view), NULL, NULL);
}

static void
search_changed (GtkEditable *editable,
                gpointer     user_data)
{
  GcalWindow *window = GCAL_WINDOW (user_data);

  if (gtk_search_bar_get_search_mode (GTK_SEARCH_BAR (window->search_bar)))
    {
      /* perform the search */
      gcal_search_view_search (GCAL_SEARCH_VIEW (window->search_view),
                               "summary", gtk_entry_get_text (GTK_ENTRY (window->search_entry)));
    }
}

static void
remove_event (GtkWidget  *notification,
              GParamSpec *spec,
              gpointer    user_data)
{
  GcalWindow *window = GCAL_WINDOW (user_data);

  if (gtk_revealer_get_child_revealed (GTK_REVEALER (notification)))
      return;

  if (window->event_to_delete != NULL)
    {
      gcal_manager_remove_event (window->manager, window->event_to_delete);
      g_clear_object (&window->event_to_delete);
    }
}

static void
undo_remove_action (GtkButton *button,
                    gpointer   user_data)
{
  GcalWindow *window;
  GList *widgets;

  window = GCAL_WINDOW (user_data);
  widgets = gcal_view_get_children_by_uuid (GCAL_VIEW (window->views[window->active_view]),
                                            gcal_event_get_uid (window->event_to_delete));

  /* Show the hidden to-be-deleted events */
  g_list_foreach (widgets, (GFunc) gtk_widget_show, NULL);

  /* Clear the event to delete */
  g_clear_object (&window->event_to_delete);

  /* Hide the notification */
  hide_notification (window, NULL);

  g_list_free (widgets);
}

static gboolean
schedule_open_edit_dialog_by_uuid (OpenEditDialogData *edit_dialog_data)
{
  GcalWindow *window;
  GList *widgets;

  window = edit_dialog_data->window;
  widgets = gcal_view_get_children_by_uuid (GCAL_VIEW (window->month_view), edit_dialog_data->uuid);
  if (widgets != NULL)
    {
      window->open_edit_dialog_timeout_id = 0;

      event_activated (NULL, widgets->data, edit_dialog_data->window);
      g_list_free (widgets);
      g_free (edit_dialog_data->uuid);
      g_free (edit_dialog_data);

      return G_SOURCE_REMOVE;
    }

  return G_SOURCE_CONTINUE;
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
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/window.ui");

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
      PROP_MANAGER,
      g_param_spec_object ("manager",
                           "The manager object",
                           "A weak reference to the app manager object",
                           GCAL_TYPE_MANAGER,
                           G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

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

  /* widgets */
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, edit_dialog);
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, header_bar);
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, main_box);
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, menu_button);
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, search_bar);
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, search_button);
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, calendars_button);
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, calendar_listbox);
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, calendar_popover);
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, source_dialog);
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, search_entry);
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, back_button);
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, today_button);
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, forward_button);
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, views_overlay);
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, views_stack);
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, week_view);
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, month_view);
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, year_view);
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, views_switcher);
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, quick_add_popover);
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, search_view);

  gtk_widget_class_bind_template_child (widget_class, GcalWindow, notification);
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, notification_label);
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, notification_action_button);
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, notification_close_button);

  gtk_widget_class_bind_template_callback (widget_class, source_row_activated);

  gtk_widget_class_bind_template_callback (widget_class, key_pressed);
  gtk_widget_class_bind_template_callback (widget_class, search_toggled);
  gtk_widget_class_bind_template_callback (widget_class, search_changed);
  gtk_widget_class_bind_template_callback (widget_class, view_changed);
  gtk_widget_class_bind_template_callback (widget_class, date_updated);

  /* Event removal related */
  gtk_widget_class_bind_template_callback (widget_class, hide_notification);
  gtk_widget_class_bind_template_callback (widget_class, remove_event);
  gtk_widget_class_bind_template_callback (widget_class, undo_remove_action);

  /* Event creation related */
  gtk_widget_class_bind_template_callback (widget_class, edit_event);
  gtk_widget_class_bind_template_callback (widget_class, create_event_detailed_cb);
  gtk_widget_class_bind_template_callback (widget_class, show_new_event_widget);
  gtk_widget_class_bind_template_callback (widget_class, close_new_event_widget);
  gtk_widget_class_bind_template_callback (widget_class, event_activated);

  /* Syncronization related */
  gtk_widget_class_bind_template_callback (widget_class, window_state_changed);

  /* search related */
  gtk_widget_class_bind_template_callback (widget_class, search_event_selected);

  /* Edit dialog related */
  gtk_widget_class_bind_template_callback (widget_class, edit_dialog_closed);
}

static void
gcal_window_init (GcalWindow *self)
{
  /* Setup actions */
  g_action_map_add_action_entries (G_ACTION_MAP (self),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   self);

  gtk_widget_init_template (GTK_WIDGET (self));

  /* source dialog */
  g_object_bind_property (self, "application", self->source_dialog, "application",
                          G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

  self->active_date = g_new0 (icaltimetype, 1);
  self->rtl = gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL;
}

static void
gcal_window_constructed (GObject *object)
{
  GcalWindow *window = GCAL_WINDOW (object);

  GtkBuilder *builder;
  GMenuModel *winmenu;

  GSettings *helper_settings;
  gchar *clock_format;
  gboolean use_24h_format;

  if (G_OBJECT_CLASS (gcal_window_parent_class)->constructed != NULL)
    G_OBJECT_CLASS (gcal_window_parent_class)->constructed (object);

  helper_settings = g_settings_new ("org.gnome.desktop.interface");
  clock_format = g_settings_get_string (helper_settings, "clock-format");
  use_24h_format = (g_strcmp0 (clock_format, "24h") == 0);
  g_free (clock_format);
  g_object_unref (helper_settings);

  /* header_bar: menu */
  builder = gtk_builder_new ();
  gtk_builder_add_from_resource (builder,
                                 "/org/gnome/calendar/gtk/menus.ui",
                                 NULL);

  winmenu = (GMenuModel *)gtk_builder_get_object (builder, "win-menu");
  gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (window->menu_button),
                                  winmenu);

  g_object_unref (builder);

  window->views[GCAL_WINDOW_VIEW_WEEK] = window->week_view;
  window->views[GCAL_WINDOW_VIEW_MONTH] = window->month_view;
  window->views[GCAL_WINDOW_VIEW_YEAR] = window->year_view;

  gcal_edit_dialog_set_time_format (GCAL_EDIT_DIALOG (window->edit_dialog), use_24h_format);

  gcal_week_view_set_first_weekday (GCAL_WEEK_VIEW (window->views[GCAL_WINDOW_VIEW_WEEK]), get_first_weekday ());
  gcal_week_view_set_use_24h_format (GCAL_WEEK_VIEW (window->views[GCAL_WINDOW_VIEW_WEEK]), use_24h_format);

  gcal_month_view_set_first_weekday (GCAL_MONTH_VIEW (window->views[GCAL_WINDOW_VIEW_MONTH]), get_first_weekday ());
  gcal_month_view_set_use_24h_format (GCAL_MONTH_VIEW (window->views[GCAL_WINDOW_VIEW_MONTH]), use_24h_format);

  gcal_year_view_set_first_weekday (GCAL_YEAR_VIEW (window->views[GCAL_WINDOW_VIEW_YEAR]), get_first_weekday ());
  gcal_year_view_set_use_24h_format (GCAL_YEAR_VIEW (window->views[GCAL_WINDOW_VIEW_YEAR]), use_24h_format);

  /* search view */
  gcal_search_view_connect (GCAL_SEARCH_VIEW (window->search_view), window->manager);
  gcal_search_view_set_time_format (GCAL_SEARCH_VIEW (window->search_view), use_24h_format);

  /* refresh timeout, first is fast */
  window->refresh_timeout_id = g_timeout_add (FAST_REFRESH_TIMEOUT, (GSourceFunc) refresh_sources, object);

  /* calendars popover */
  gtk_list_box_set_sort_func (GTK_LIST_BOX (window->calendar_listbox), (GtkListBoxSortFunc) calendar_listbox_sort_func,
                              object, NULL);

  if (gcal_manager_load_completed (window->manager))
    {
      GList *sources, *l;
      sources = gcal_manager_get_sources_connected (window->manager);
      for (l = sources; l != NULL; l = g_list_next (l))
        add_source (window->manager, l->data, gcal_manager_source_enabled (window->manager, l->data), object);

      g_list_free (sources);
    }
}

static void
gcal_window_finalize (GObject *object)
{
  GcalWindow *window = GCAL_WINDOW (object);

  if (window->save_geometry_timeout_id > 0)
    {
      g_source_remove (window->save_geometry_timeout_id);
      window->save_geometry_timeout_id = 0;
    }

  if (window->open_edit_dialog_timeout_id > 0)
    {
      g_source_remove (window->open_edit_dialog_timeout_id);
      window->open_edit_dialog_timeout_id = 0;
    }

  /* If we have a queued event to delete, remove it now */
  if (window->event_to_delete)
    {
      gcal_manager_remove_event (window->manager, window->event_to_delete);
      g_clear_object (&window->event_to_delete);
    }

  if (window->event_creation_data != NULL)
    {
      g_clear_pointer (&window->event_creation_data->start_date, g_date_time_unref);
      g_clear_pointer (&window->event_creation_data->end_date, g_date_time_unref);
      g_clear_pointer (&window->event_creation_data, g_free);
    }

  g_clear_object (&window->manager);
  g_clear_object (&window->views_switcher);

  g_free (window->active_date);
  g_free (window->current_date);

  G_OBJECT_CLASS (gcal_window_parent_class)->finalize (object);
}

static void
gcal_window_set_property (GObject      *object,
                          guint         property_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  GcalWindow *self = GCAL_WINDOW (object);

  switch (property_id)
    {
    case PROP_ACTIVE_VIEW:
      self->active_view = g_value_get_enum (value);
      gtk_widget_show (self->views[self->active_view]);
      gtk_stack_set_visible_child (GTK_STACK (self->views_stack),
                                   self->views[self->active_view]);
      return;
    case PROP_ACTIVE_DATE:
      update_active_date (GCAL_WINDOW (object), g_value_dup_boxed (value));
      return;
    case PROP_NEW_EVENT_MODE:
      set_new_event_mode (GCAL_WINDOW (object), g_value_get_boolean (value));
      return;
    case PROP_MANAGER:
      if (g_set_object (&self->manager, g_value_get_object (value)))
        {
          g_signal_connect (self->manager, "source-added", G_CALLBACK (add_source), object);
          g_signal_connect (self->manager, "source-removed", G_CALLBACK (remove_source), object);
          g_signal_connect_swapped (self->manager, "source-enabled", G_CALLBACK (source_enabled), object);
          g_signal_connect_swapped (self->manager, "source-changed", G_CALLBACK (source_changed), object);

          gcal_edit_dialog_set_manager (GCAL_EDIT_DIALOG (self->edit_dialog), self->manager);
          gcal_week_view_set_manager (GCAL_WEEK_VIEW (self->week_view), self->manager);
          gcal_month_view_set_manager (GCAL_MONTH_VIEW (self->month_view), self->manager);
          gcal_year_view_set_manager (GCAL_YEAR_VIEW (self->year_view), self->manager);
          gcal_quick_add_popover_set_manager (GCAL_QUICK_ADD_POPOVER (self->quick_add_popover), self->manager);
          gcal_source_dialog_set_manager (GCAL_SOURCE_DIALOG (self->source_dialog), self->manager);
          update_current_date (GCAL_WINDOW (object));

          g_object_notify (object, "manager");
        }
      return;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
gcal_window_get_property (GObject    *object,
                          guint       property_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  GcalWindow *self = GCAL_WINDOW (object);

  switch (property_id)
    {
    case PROP_ACTIVE_VIEW:
      g_value_set_enum (value, self->active_view);
      return;
    case PROP_MANAGER:
      g_value_set_object (value, self->manager);
      return;
    case PROP_ACTIVE_DATE:
      g_value_set_boxed (value, self->active_date);
      return;
    case PROP_NEW_EVENT_MODE:
      g_value_set_boolean (value, self->new_event_mode);
      return;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static gboolean
gcal_window_configure_event (GtkWidget         *widget,
                             GdkEventConfigure *event)
{
  GcalWindow *window;
  gboolean retval;

  window = GCAL_WINDOW (widget);

  if (window->save_geometry_timeout_id != 0)
    {
      g_source_remove (window->save_geometry_timeout_id);
      window->save_geometry_timeout_id = 0;
    }

  window->save_geometry_timeout_id = g_timeout_add (SAVE_GEOMETRY_ID_TIMEOUT,
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
  gboolean retval;

  window = GCAL_WINDOW (widget);

  if (window->save_geometry_timeout_id != 0)
    {
      g_source_remove (window->save_geometry_timeout_id);
      window->save_geometry_timeout_id = 0;
    }

  window->save_geometry_timeout_id = g_timeout_add (SAVE_GEOMETRY_ID_TIMEOUT,
                                                    save_geometry,
                                                    window);

  retval = GTK_WIDGET_CLASS (gcal_window_parent_class)->window_state_event (widget, event);

  return retval;
}

/* Public API */
GtkWidget*
gcal_window_new_with_view_and_date (GcalApplication   *app,
                                    GcalWindowViewType view_type,
                                    icaltimetype      *date)
{
  GcalWindow *win;
  GcalManager *manager;

  manager = gcal_application_get_manager (GCAL_APPLICATION (app));

  win  =  g_object_new (GCAL_TYPE_WINDOW, "application", GTK_APPLICATION (app), "manager", manager, "active-date", date,
                        NULL);

  /* setup accels */
  gcal_window_add_accelerator (app, "win.next",     "<Alt>Right");
  gcal_window_add_accelerator (app, "win.previous", "<Alt>Left");
  gcal_window_add_accelerator (app, "win.today",    "<Alt>Down");
  gcal_window_add_accelerator (app, "win.today",    "<Ctrl>t");

  gcal_window_add_accelerator (app, "win.change-view(-1)", "<Ctrl>Page_Down");
  gcal_window_add_accelerator (app, "win.change-view(-2)", "<Ctrl>Page_Up");
  gcal_window_add_accelerator (app, "win.change-view(2)",  "<Ctrl>2");
  gcal_window_add_accelerator (app, "win.change-view(3)",  "<Ctrl>3");

  /* loading size */
  load_geometry (win);

  if (view_type == GCAL_WINDOW_VIEW_DAY)
    view_changed (NULL, NULL, win);

  return GTK_WIDGET (win);
}

/* new-event interaction: first variant */
void
gcal_window_new_event (GcalWindow *window)
{
  GDateTime *start_date, *end_date;
  icaltimetype date;

  /* 1st and 2nd steps */
  set_new_event_mode (window, TRUE);

  date = *window->current_date;
  date.is_date = 1;

  start_date = icaltime_to_datetime (&date);
  end_date = icaltime_to_datetime (&date);

  /* adjusting dates according to the actual view */
  switch (window->active_view)
    {
    case GCAL_WINDOW_VIEW_DAY:
    case GCAL_WINDOW_VIEW_WEEK:
      end_date = g_date_time_add_hours (start_date, 1);
      break;
    case GCAL_WINDOW_VIEW_MONTH:
    case GCAL_WINDOW_VIEW_YEAR:
      end_date = g_date_time_add_days (start_date, 1);
      break;
    case GCAL_WINDOW_VIEW_LIST:
    case GCAL_WINDOW_VIEW_SEARCH:
    default:
      break;
    }

  create_event_detailed_cb (NULL, start_date, end_date, window);
}

void
gcal_window_set_search_mode (GcalWindow *window,
                             gboolean    enabled)
{
  g_return_if_fail (GCAL_IS_WINDOW (window));

  window->search_mode = enabled;
  gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (window->search_bar), enabled);
}

void
gcal_window_set_search_query (GcalWindow  *window,
                              const gchar *query)
{
  g_return_if_fail (GCAL_IS_WINDOW (window));

  gtk_entry_set_text (GTK_ENTRY (window->search_entry), query);
}

void
gcal_window_open_event_by_uuid (GcalWindow  *window,
                                const gchar *uuid)
{
  GList *widgets;

  /* XXX: show events on month view */
  gtk_stack_set_visible_child (GTK_STACK (window->views_stack), window->month_view);
  widgets = gcal_view_get_children_by_uuid (GCAL_VIEW (window->month_view), uuid);
  if (widgets != NULL)
    {
      event_activated (NULL, widgets->data, window);
      g_list_free (widgets);
    }
  else
    {
      OpenEditDialogData *edit_dialog_data = g_new0 (OpenEditDialogData, 1);
      edit_dialog_data->window = window;
      edit_dialog_data->uuid = g_strdup (uuid);
      window->open_edit_dialog_timeout_id = g_timeout_add_seconds (2,
                                                                   (GSourceFunc) schedule_open_edit_dialog_by_uuid,
                                                                   edit_dialog_data);
    }
}
