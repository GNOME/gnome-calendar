/* gcal-window.c
 *
 * Copyright (C) 2015 Erick Pérez Castellanos <erickpc@gnome.org>
 * Copyright (C) 2014-2020 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#define G_LOG_DOMAIN "GcalWindow"

#include "gcal-agenda-view.h"
#include "gcal-calendar-list.h"
#include "gcal-calendar-management-dialog.h"
#include "gcal-calendar-navigation-button.h"
#include "config.h"
#include "gcal-date-chooser.h"
#include "gcal-debug.h"
#include "gcal-drop-overlay.h"
#include "gcal-event-editor-dialog.h"
#include "gcal-event-widget.h"
#include "gcal-context.h"
#include "gcal-manager.h"
#include "gcal-month-view.h"
#include "gcal-quick-add-popover.h"
#include "gcal-search-button.h"
#include "gcal-sync-indicator.h"
#include "gcal-timeline.h"
#include "gcal-timeline-subscriber.h"
#include "gcal-view.h"
#include "gcal-weather-settings.h"
#include "gcal-week-view.h"
#include "gcal-window.h"

#include "importer/gcal-import-dialog.h"

#include <glib/gi18n.h>
#include <libecal/libecal.h>

/**
 * SECTION:gcal-window
 * @short_description: Main window of GNOME Calendar
 * @title:GcalWindow
 * @stability:unstable
 * @image:gcal-window.png
 *
 * #GcalWindow is the main window of GNOME Calendar, and contains
 * the views, the source dialog, the edit dialog, and manages the
 * calendar toggler popover menu.
 *
 * Besides that, #GcalWindow is also responsible for #GcalQuickAddPopover,
 * and it responds to the #GcalView:create-event signal by positioning
 * the quick add popover at the requested position.
 *
 * ## Edit dialog
 *
 * When an event is clicked, the views send the `event-activated`
 * signal. #GcalWindow responds to this signal opening #GcalEventEditorDialog
 * with the clicked event.
 *
 * When #GcalEventEditorDialog sends a response, #GcalWindow reacts by
 * either propagating to gcal_manager_update_event(), or hiding
 * the delete event widgets from the views.
 *
 * ## Source dialog
 *
 * The interaction with the source dialog is almost none. #GcalWindow
 * just shows and hides it.
 */

typedef struct
{
  gint                x;
  gint                y;
  GcalRange          *range;
} NewEventData;

typedef struct
{
  GcalWindow         *window;
  gchar              *uuid;
} OpenEditDialogData;

typedef struct
{
  GList              *ics_files;
  GList              *invalid_files;
} FileFilterResult;

struct _GcalWindow
{
  AdwApplicationWindow parent;

  /* upper level widgets */
  GtkWidget              *header_bar;
  AdwToastOverlay        *overlay;
  AdwViewStack           *views_stack;
  GtkWidget              *week_view;
  GtkWidget              *month_view;
  GtkWidget              *agenda_view;
  GtkWidget              *date_chooser;
  GcalSyncIndicator      *sync_indicator;
  GtkWidget              *search_button;
  AdwNavigationSplitView *split_view;

  /* header_bar widgets */
  GtkWidget          *calendars_list;
  GtkWidget          *menu_button;

  GcalEventEditorDialog *event_editor;
  GtkWidget             *import_dialog;

  GtkWidget          *last_focused_widget;

  /* new event popover widgets */
  GtkWidget          *quick_add_popover;

  /* day, week, month, year, list */
  GtkWidget          *views [6];
  gboolean            subscribed;

  GcalContext        *context;
  GcalWindowView      active_view;

  GDateTime          *active_date;

  gboolean            rtl;

  /* states */
  gboolean            new_event_mode;

  NewEventData       *event_creation_data;

  AdwToast           *delete_event_toast;

  gint                open_edit_dialog_timeout_id;

  /* weather management */
  GcalWeatherSettings *weather_settings;

  /* temp to keep event_creation */
  gboolean            open_edit_dialog;

  /* handler for the searh_view */
  gint                click_outside_handler_id;

  /* CSS */
  GtkCssProvider     *colors_provider;

  /* Window states */
  gboolean            in_key_press;

  /* File drop */
  GcalDropOverlay    *drop_overlay;
  AdwStatusPage      *drop_status_page;
  GtkDropTarget      *drop_target;
};

enum
{
  PROP_0,
  PROP_ACTIVE_DATE,
  PROP_ACTIVE_VIEW,
  PROP_CONTEXT,
  PROP_NEW_EVENT_MODE,
  N_PROPS
};

G_DEFINE_TYPE (GcalWindow, gcal_window, ADW_TYPE_APPLICATION_WINDOW)

static GParamSpec* properties[N_PROPS] = { NULL, };


/*
 * Auxiliary methods
 */

static void
update_today_action_enabled (GcalWindow *window)
{
  g_autoptr (GDateTime) now = NULL;
  GSimpleAction *action;
  gboolean enabled;

  GCAL_ENTRY;

  now = g_date_time_new_now_local ();

  switch (window->active_view)
    {
    case GCAL_WINDOW_VIEW_WEEK:
    case GCAL_WINDOW_VIEW_MONTH:
    case GCAL_WINDOW_VIEW_AGENDA:
      enabled = g_date_time_get_year (window->active_date) != g_date_time_get_year (now) ||
                g_date_time_get_week_of_year (window->active_date) !=  g_date_time_get_week_of_year (now);

      GCAL_TRACE_MSG ("Active date's week is %d, current week is %d",
                      g_date_time_get_week_of_year (window->active_date),
                      g_date_time_get_week_of_year (now));
      break;

    default:
      enabled = TRUE;
      break;
    }

  action = G_SIMPLE_ACTION (g_action_map_lookup_action (G_ACTION_MAP (window), "today"));
  g_simple_action_set_enabled (action, enabled);

  GCAL_EXIT;
}

static gchar*
get_previous_date_icon (GcalWindow     *window,
                        GcalWindowView *view)
{
  switch (gcal_view_get_time_direction (GCAL_VIEW (view)))
    {
    case GTK_ORIENTATION_VERTICAL:
      return g_strdup ("go-up-symbolic");

    case GTK_ORIENTATION_HORIZONTAL:
      return g_strdup ("go-previous-symbolic");

    default:
      g_assert_not_reached ();
    }
}

static gchar*
get_previous_date_tooltip (GcalWindow  *window,
                           const gchar *view_name)
{
  if (g_str_equal (view_name, "week"))
    return g_strdup (_("Previous Week"));
  else if (g_str_equal (view_name, "month"))
    return g_strdup (_("Previous Month"));
  else if (g_str_equal (view_name, "agenda"))
    return g_strdup (_("Yesterday"));
  else
    g_assert_not_reached ();
}

static gchar*
get_next_date_icon (GcalWindow     *window,
                    GcalWindowView *view)
{
  switch (gcal_view_get_time_direction (GCAL_VIEW (view)))
    {
    case GTK_ORIENTATION_VERTICAL:
      return g_strdup ("go-down-symbolic");

    case GTK_ORIENTATION_HORIZONTAL:
      return g_strdup ("go-next-symbolic");

    default:
      g_assert_not_reached ();
    }
}

static gchar*
get_next_date_tooltip (GcalWindow  *window,
                       const gchar *view_name)
{
  if (g_str_equal (view_name, "week"))
    return g_strdup (_("Next Week"));
  else if (g_str_equal (view_name, "month"))
    return g_strdup (_("Next Month"));
  else if (g_str_equal (view_name, "agenda"))
    return g_strdup (_("Tomorrow"));
  else
    g_assert_not_reached ();
}

static void
maybe_add_subscribers_to_timeline (GcalWindow *self)
{
  GcalTimeline *timeline;

  if (self->subscribed)
    return;


  timeline = gcal_manager_get_timeline (gcal_context_get_manager (self->context));
  gcal_timeline_add_subscriber (timeline, GCAL_TIMELINE_SUBSCRIBER (self->week_view));
  gcal_timeline_add_subscriber (timeline, GCAL_TIMELINE_SUBSCRIBER (self->month_view));
  gcal_timeline_add_subscriber (timeline, GCAL_TIMELINE_SUBSCRIBER (self->agenda_view));
  gcal_timeline_add_subscriber (timeline, GCAL_TIMELINE_SUBSCRIBER (self->date_chooser));

  self->subscribed = TRUE;
}

static void
update_active_date (GcalWindow *window,
                    GDateTime  *date)
{
  g_autoptr (GDateTime) new_date = NULL;
  g_autofree gchar *new_date_string = NULL;
  GcalWindowView i;

  GCAL_ENTRY;

  new_date = g_date_time_new (g_date_time_get_timezone (date),
                              g_date_time_get_year (date),
                              g_date_time_get_month (date),
                              g_date_time_get_day_of_month (date),
                              0, 0, 0);

  new_date_string = g_date_time_format (new_date, "%x %X %z");
  g_debug ("Updating active date to %s", new_date_string);

  gcal_set_date_time (&window->active_date, new_date);

  for (i = 0; i < GCAL_WINDOW_VIEW_N_VIEWS; i++)
    gcal_view_set_date (GCAL_VIEW (window->views[i]), new_date);
  gcal_view_set_date (GCAL_VIEW (window->date_chooser), new_date);

  update_today_action_enabled (window);

  maybe_add_subscribers_to_timeline (window);

  GCAL_EXIT;
}

static void
recalculate_calendar_colors_css (GcalWindow *self)
{
  g_autoptr (GString) css_colors = NULL;
  g_autoptr (GList) calendars = NULL;
  GcalManager *manager;

  css_colors = g_string_new (NULL);
  manager = gcal_context_get_manager (self->context);
  calendars = gcal_manager_get_calendars (manager);
  for (GList *l = calendars; l; l = l->next)
    {
      g_autofree gchar* color_str = NULL;
      const GdkRGBA *color;
      GcalCalendar *calendar;
      GQuark color_id;

      calendar = GCAL_CALENDAR (l->data);

      color = gcal_calendar_get_color (calendar);
      color_str = gdk_rgba_to_string (color);
      color_id = g_quark_from_string (color_str);

      g_string_append_printf (css_colors, ".color-%u { --event-bg-color: %s; }\n", color_id, color_str);
    }

  gtk_css_provider_load_from_string (self->colors_provider, css_colors->str);
}

static void
load_css_providers (GcalWindow *self)
{
  GdkDisplay *display;

  display = gtk_widget_get_display (GTK_WIDGET (self));

  /* Calendar olors */
  self->colors_provider = gtk_css_provider_new ();
  gtk_style_context_add_provider_for_display (display,
                                              GTK_STYLE_PROVIDER (self->colors_provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 1);
}

static void
show_widget (GtkWidget *widget)
{
  gtk_widget_set_visible (widget, TRUE);
}

static void
hide_widget (GtkWidget *widget)
{
  gtk_widget_set_visible (widget, FALSE);
}

static char *
content_title (GcalWindow     *self,
               GcalWindowView  active_view)
{
  /* We check for NULL so we get the right page name during initialization */
  switch (active_view)
    {
    case GCAL_WINDOW_VIEW_WEEK:
      return g_strdup (_("Week"));

    case GCAL_WINDOW_VIEW_MONTH:
      return g_strdup (_("Month"));

    case GCAL_WINDOW_VIEW_AGENDA:
      return g_strdup (_("Agenda"));

    default:
      return g_strdup (_("Calendar"));
    }
}

static inline void
free_file_filter_result (FileFilterResult *self)
{
  g_list_free_full (self->ics_files, g_object_unref);
  g_list_free_full (self->invalid_files, g_object_unref);
}

static inline gboolean
is_ics_file (GFile *file)
{
  g_autoptr (GFileInfo) file_info = NULL;
  g_autoptr (GError) error = NULL;

  file_info = g_file_query_info (file,
                                 G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                                 G_FILE_QUERY_INFO_NONE,
                                 NULL,
                                 &error);

  if (error)
    return FALSE;

  return g_strcmp0 (g_file_info_get_content_type (file_info), "text/calendar") == 0;
}

static inline void
free_file_slist_full (GSList *files)
{
  g_slist_free_full (files, g_object_unref);
}

static void
filter_ics_files_thread (GTask        *task,
                         gpointer      source_object,
                         gpointer      task_data,
                         GCancellable *cancellable)
{
  FileFilterResult *result;
  GSList *files;

  files = task_data;
  result = g_new0 (FileFilterResult, 1);

  for (GSList *iter = files; iter != NULL; iter = iter->next)
    {
      GFile *file = G_FILE (iter->data);

      if (is_ics_file (file))
        result->ics_files = g_list_prepend (result->ics_files, g_object_ref (file));
      else
        result->invalid_files = g_list_prepend (result->invalid_files, g_object_ref (file));
    }

  g_task_return_pointer (task, result, NULL);
}

static FileFilterResult*
filter_ics_files_finish (GAsyncResult  *result,
                         GError       **error)
{
  g_return_val_if_fail (g_task_is_valid (result, NULL), NULL);
  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
filter_ics_files (GcalWindow          *self,
                  GSList              *files,
                  GAsyncReadyCallback  callback)
{
  g_autoslist (GFile) files_copy = NULL;
  g_autoptr (GTask) task = NULL;

  files_copy = g_slist_copy_deep (files, (GCopyFunc) g_object_ref, NULL);

  task = g_task_new (NULL, NULL, callback, self);
  g_task_set_task_data (task, g_steal_pointer (&files_copy), (GDestroyNotify) free_file_slist_full);
  g_task_set_source_tag (task, filter_ics_files);
  g_task_run_in_thread (task, filter_ics_files_thread);
}

/*
 * Callbacks
 */

static void
on_show_calendars_action_activated (GSimpleAction *action,
                                    GVariant      *param,
                                    gpointer       user_data)
{
  GcalWindow *self = GCAL_WINDOW (user_data);
  GcalCalendarManagementDialog *dialog = gcal_calendar_management_dialog_new (self->context);

  adw_dialog_present (ADW_DIALOG (dialog), GTK_WIDGET (self));
}

static void
on_view_action_activated (GSimpleAction *action,
                          GVariant      *param,
                          gpointer       user_data)
{
  GcalWindow *window = GCAL_WINDOW (user_data);
  gint32 view;

  GCAL_ENTRY;

  view = g_variant_get_int32 (param);

  /* -1 means next view, -2 means previous view */
  if (view == -1)
    view = window->active_view + 1;
  else if (view == -2)
    view = window->active_view - 1;

  view = CLAMP (view, 0, GCAL_WINDOW_VIEW_N_VIEWS - 1);

  if (adw_view_stack_page_get_visible (adw_view_stack_get_page (window->views_stack, window->views[view])))
    {
      window->active_view = view;
      adw_view_stack_set_visible_child (window->views_stack, window->views[window->active_view]);

      g_object_notify_by_pspec (G_OBJECT (user_data), properties[PROP_ACTIVE_VIEW]);
    }

  GCAL_EXIT;
}

static void
on_window_next_date_activated_cb (GSimpleAction *action,
                                  GVariant      *param,
                                  gpointer       user_data)
{
  g_autoptr (GDateTime) next_date = NULL;
  GcalWindow *self;
  GcalView *current_view;

  self = GCAL_WINDOW (user_data);
  current_view = GCAL_VIEW (self->views[self->active_view]);
  next_date = gcal_view_get_next_date (current_view);

  update_active_date (self, next_date);
}

static void
on_window_new_event_cb (GSimpleAction *action,
                        GVariant      *param,
                        gpointer       user_data)
{
  g_autoptr (ECalComponent) comp = NULL;
  g_autoptr (GDateTime) start = NULL;
  g_autoptr (GDateTime) end = NULL;
  GcalCalendar *default_calendar;
  GcalManager *manager;
  GcalWindow *self;
  GcalEvent *event;

  self = GCAL_WINDOW (user_data);
  start = g_date_time_new_utc (g_date_time_get_year (self->active_date),
                               g_date_time_get_month (self->active_date),
                               g_date_time_get_day_of_month (self->active_date),
                               0, 0, 0);
  end = g_date_time_add_days (start, 1);

  manager = gcal_context_get_manager (self->context);
  comp = build_component_from_details ("", start, end);
  default_calendar = gcal_manager_get_default_calendar (manager);
  event = gcal_event_new (default_calendar, comp, NULL);

  adw_dialog_present (ADW_DIALOG (self->event_editor), GTK_WIDGET (self));
  gcal_event_editor_dialog_set_event (self->event_editor, event, TRUE);
}

static void
on_window_open_date_time_settings_cb (GSimpleAction *action,
                                      GVariant      *param,
                                      gpointer       user_data)
{
  GDBusConnection *connection = g_application_get_dbus_connection (g_application_get_default ());

  gcal_utils_launch_gnome_settings (connection, "datetime", NULL);
}

static void
on_window_open_online_accounts_cb (GSimpleAction *action,
                                   GVariant      *param,
                                   gpointer       user_data)
{
  GDBusConnection *connection = g_application_get_dbus_connection (g_application_get_default ());

  gcal_utils_launch_gnome_settings (connection, "online-accounts", NULL);
}

static void
on_window_previous_date_activated_cb (GSimpleAction *action,
                                      GVariant      *param,
                                      gpointer       user_data)
{
  g_autoptr (GDateTime) previous_date = NULL;
  GcalWindow *self;
  GcalView *current_view;

  self = GCAL_WINDOW (user_data);
  current_view = GCAL_VIEW (self->views[self->active_view]);
  previous_date = gcal_view_get_previous_date (current_view);

  update_active_date (self, previous_date);
}

static void
on_window_today_activated_cb (GSimpleAction *action,
                              GVariant      *param,
                              gpointer       user_data)
{
  g_autoptr (GDateTime) today = NULL;
  GcalWindow *self;

  self = GCAL_WINDOW (user_data);
  today = g_date_time_new_now_local ();

  update_active_date (self, today);
}

static void
on_window_undo_delete_event_cb (GSimpleAction *action,
                                GVariant      *param,
                                gpointer       user_data)
{
  g_autoptr (GList) widgets = NULL;
  GcalRecurrenceModType modifier;
  GcalWindow *self;
  GcalEvent *event;

  GCAL_ENTRY;

  self = GCAL_WINDOW (user_data);

  if (!self->delete_event_toast)
    GCAL_RETURN ();

  event = g_object_get_data (G_OBJECT (self->delete_event_toast), "event");
  modifier = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (self->delete_event_toast), "modifier"));

  g_assert (event != NULL);

  /* Show the hidden to-be-deleted events */
  widgets = gcal_view_get_children_by_uuid (GCAL_VIEW (self->views[self->active_view]),
                                            modifier,
                                            gcal_event_get_uid (event));

  g_list_foreach (widgets, (GFunc) show_widget, NULL);

  g_clear_object (&self->delete_event_toast);

  GCAL_EXIT;
}

static void
on_breakpoint_changed (GObject *object,
                       GParamSpec *pspec,
                       GcalWindow *window)
{
  gint32 view = window->active_view;

  while (!adw_view_stack_page_get_visible (adw_view_stack_get_page (window->views_stack, window->views[view])))
    {
      view--;

      if (view < 0)
        view += GCAL_WINDOW_VIEW_N_VIEWS;

      if (view == window->active_view)
        return;
    }

  if (window->active_view != view)
    {
      window->active_view = view;
      adw_view_stack_set_visible_child (window->views_stack, window->views[window->active_view]);
      g_object_notify_by_pspec (G_OBJECT (window), properties[PROP_ACTIVE_VIEW]);
    }
}

static void
view_changed (GObject    *object,
              GParamSpec *pspec,
              gpointer    user_data)
{
  GcalWindow *window;
  GEnumClass *eklass;
  GEnumValue *eval;
  GcalWindowView view_type;

  window = GCAL_WINDOW (user_data);

  /* XXX: this is the destruction process */
  if (!gtk_widget_get_visible (GTK_WIDGET (window->views_stack)))
    return;

  /*
   * As there are no guarantees for the order of AdwBreakpoint setters,
   * it is possible that the agenda view disappears before the month and
   * week views reappears, and vice versa.
   */
  if (!adw_view_stack_get_visible_child_name (window->views_stack))
    return;

  eklass = g_type_class_ref (GCAL_TYPE_WINDOW_VIEW);
  eval = g_enum_get_value_by_nick (eklass, adw_view_stack_get_visible_child_name (window->views_stack));

  view_type = eval->value;

  g_type_class_unref (eklass);

  window->active_view = view_type;
  update_today_action_enabled (window);
  g_object_notify_by_pspec (G_OBJECT (user_data), properties[PROP_ACTIVE_VIEW]);
}

static void
set_new_event_mode (GcalWindow *window,
                    gboolean    enabled)
{
  GCAL_ENTRY;

  window->new_event_mode = enabled;
  g_object_notify_by_pspec (G_OBJECT (window), properties[PROP_NEW_EVENT_MODE]);

  if (!enabled && window->views[window->active_view])
    gcal_view_clear_marks (GCAL_VIEW (window->views[window->active_view]));

  /* XXX: here we could disable clicks from the views, yet */
  /* for now we relaunch the new-event widget */
  if (!enabled && gtk_widget_is_visible (window->quick_add_popover))
    {
      gtk_widget_set_visible (window->quick_add_popover, FALSE);
    }

  GCAL_EXIT;
}

/* new-event interaction: second variant */
static void
show_new_event_widget (GcalView   *view,
                       GcalRange  *range,
                       gdouble     x,
                       gdouble     y,
                       GcalWindow *window)
{
  graphene_point_t p;
  GdkRectangle rect;

  GCAL_ENTRY;

  g_assert (range != NULL);

  /* 1st and 2nd steps */
  set_new_event_mode (window, TRUE);

  if (window->event_creation_data != NULL)
    {
      g_clear_pointer (&window->event_creation_data->range, gcal_range_unref);
      g_clear_pointer (&window->event_creation_data, g_free);
    }

  window->event_creation_data = g_new0 (NewEventData, 1);
  window->event_creation_data->x = x;
  window->event_creation_data->y = y;
  window->event_creation_data->range = gcal_range_ref (range);

  g_debug ("Quick add popover position (%f, %f)", x, y);

  /* Setup the quick add popover's dates */
  gcal_quick_add_popover_set_range (GCAL_QUICK_ADD_POPOVER (window->quick_add_popover),
                                    window->event_creation_data->range);

  /* Position and place the quick add popover */
  if (!gtk_widget_compute_point (window->views[window->active_view],
                                 GTK_WIDGET (window),
                                 &GRAPHENE_POINT_INIT (x, y),
                                 &p))
    {
      g_assert_not_reached ();
    }

  /* Place popover over the given (x,y) position */
  rect.x = p.x;
  rect.y = p.y;
  rect.width = 1;
  rect.height = 1;

  gtk_popover_set_pointing_to (GTK_POPOVER (window->quick_add_popover), &rect);
  gtk_popover_popup (GTK_POPOVER (window->quick_add_popover));

  GCAL_EXIT;
}

static void
close_new_event_widget (GtkButton *button,
                        gpointer   user_data)
{
  set_new_event_mode (GCAL_WINDOW (user_data), FALSE);
}

static void
edit_event (GcalQuickAddPopover *popover,
            GcalEvent           *event,
            GcalWindow          *self)
{
  adw_dialog_present (ADW_DIALOG (self->event_editor), GTK_WIDGET (self));
  gcal_event_editor_dialog_set_event (self->event_editor, event, TRUE);
}

static void
create_event_detailed_cb (GcalView   *view,
                          GcalRange  *range,
                          GcalWindow *self)
{
  g_autoptr (GDateTime) range_start = NULL;
  g_autoptr (GDateTime) range_end = NULL;
  GcalCalendar *default_calendar;
  GcalManager *manager;
  ECalComponent *comp;
  GcalEvent *event;

  manager = gcal_context_get_manager (self->context);
  range_start = gcal_range_get_start (range);
  range_end = gcal_range_get_end (range);
  comp = build_component_from_details ("", range_start, range_end);
  default_calendar = gcal_manager_get_default_calendar (manager);
  event = gcal_event_new (default_calendar, comp, NULL);

  adw_dialog_present (ADW_DIALOG (self->event_editor), GTK_WIDGET (self));
  gcal_event_editor_dialog_set_event (self->event_editor, event, TRUE);

  g_clear_object (&comp);
}

static void
event_preview_cb (GcalEventWidget        *event_widget,
                  GcalEventPreviewAction  action,
                  gpointer                user_data)
{
  GcalWindow *self = GCAL_WINDOW (user_data);
  GcalEvent *event;

  switch (action)
    {
    case GCAL_EVENT_PREVIEW_ACTION_EDIT:
      event = gcal_event_widget_get_event (event_widget);
      adw_dialog_present (ADW_DIALOG (self->event_editor), GTK_WIDGET (self));
      gcal_event_editor_dialog_set_event (self->event_editor, event, FALSE);
      break;

    case GCAL_EVENT_PREVIEW_ACTION_NONE:
    default:
      if (self->last_focused_widget)
        {
          gtk_root_set_focus (GTK_ROOT (self), self->last_focused_widget);
          g_clear_weak_pointer (&self->last_focused_widget);
        }
      break;
    }
}

static void
day_selected (GcalWindow *self)
{
  update_active_date (self, gcal_view_get_date (GCAL_VIEW (self->date_chooser)));
}

static GtkWidget*
find_first_focusable_widget (GtkWidget *widget)
{
  GtkWidget *aux = widget;

  while (aux && !gtk_widget_get_focusable (aux))
    aux = gtk_widget_get_parent (aux);

  g_assert (GTK_IS_WIDGET (aux));
  return aux;
}

static void
event_activated (GcalView        *view,
                 GcalEventWidget *event_widget,
                 gpointer         user_data)
{
  GcalWindow *self = GCAL_WINDOW (user_data);

  g_set_weak_pointer (&self->last_focused_widget, find_first_focusable_widget (GTK_WIDGET (event_widget)));

  gcal_event_widget_show_preview (event_widget, event_preview_cb, user_data);
}

static void
on_toast_dismissed_cb (AdwToast   *toast,
                       GcalWindow *self)
{
  GcalRecurrenceModType modifier;
  GcalManager *manager;
  GcalEvent *event;

  GCAL_ENTRY;

  /* If we undid the removal, the stored toast is gone at this point */
  if (!self->delete_event_toast)
    GCAL_RETURN();

  manager = gcal_context_get_manager (self->context);
  event = g_object_get_data (G_OBJECT (toast), "event");
  modifier = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (toast), "modifier"));

  g_assert (event != NULL);

  gcal_manager_remove_event (manager, event, modifier);
  g_clear_object (&self->delete_event_toast);

  GCAL_EXIT;
}

static void
on_event_editor_dialog_remove_event_cb (GcalEventEditorDialog *edit_dialog,
                                        GcalEvent             *event,
                                        GcalRecurrenceModType  modifier,
                                        GcalWindow            *self)
{
  g_autoptr (AdwToast) toast = NULL;
  g_autoptr (GList) widgets = NULL;
  g_autoptr (GList) agenda_view_widgets = NULL;
  g_autoptr (GList) date_chooser_widgets = NULL;
  GcalView *view;
  gboolean has_deleted_event;

  GCAL_ENTRY;

  has_deleted_event = self->delete_event_toast != NULL;
  if (self->delete_event_toast)
    adw_toast_dismiss (self->delete_event_toast);
  self->delete_event_toast = NULL;

  toast = adw_toast_new (has_deleted_event ? _("Another event deleted") : _("Event deleted"));
  adw_toast_set_timeout (toast, 5);
  adw_toast_set_button_label (toast, _("_Undo"));
  adw_toast_set_action_name (toast, "win.undo-delete-event");
  g_object_set_data_full (G_OBJECT (toast), "event", g_object_ref (event), g_object_unref);
  g_object_set_data (G_OBJECT (toast), "modifier", GINT_TO_POINTER (modifier));
  g_signal_connect (toast, "dismissed", G_CALLBACK (on_toast_dismissed_cb), self);

  adw_toast_overlay_add_toast (self->overlay, g_object_ref (toast));
  self->delete_event_toast = g_steal_pointer (&toast);

  /* hide widget of the event */
  view = GCAL_VIEW (self->views[self->active_view]);
  widgets = gcal_view_get_children_by_uuid (view, modifier, gcal_event_get_uid (event));
  agenda_view_widgets = gcal_view_get_children_by_uuid (GCAL_VIEW (self->agenda_view), modifier, gcal_event_get_uid (event));
  date_chooser_widgets = gcal_view_get_children_by_uuid (GCAL_VIEW (self->date_chooser), modifier, gcal_event_get_uid (event));

  g_list_foreach (widgets, (GFunc) hide_widget, NULL);
  g_list_foreach (agenda_view_widgets, (GFunc) hide_widget, NULL);
  g_list_foreach (date_chooser_widgets, (GFunc) hide_widget, NULL);

  GCAL_EXIT;
}

static gboolean
on_drop_target_accept_cb (GtkDropTarget *target,
                          GdkDrop       *drop,
                          gpointer       user_data)
{
  GdkDrag *drag;
  GdkContentFormats *formats;

  drag = gdk_drop_get_drag (drop);
  formats = gdk_drop_get_formats (drop);

  // If drag is NULL, it means we are not receiving the from an external application/window,
  // but from internal sources, i.e, dragging events
  gboolean is_external = drag == NULL;
  return is_external && gdk_content_formats_contain_gtype (formats, GDK_TYPE_FILE_LIST);
}

static void
on_ics_files_filtered_cb (GObject      *source_object,
                          GAsyncResult *result,
                          gpointer      data)
{
  g_autoptr (GError) error = NULL;
  g_autofree gchar* toast_title = NULL;
  AdwToast *error_toast = NULL;
  GcalWindow *self;
  FileFilterResult *filter_result;

  GCAL_ENTRY;

  self = GCAL_WINDOW (data);
  filter_result = filter_ics_files_finish (result, &error);

  if (error)
    return;

  // Creates an error toast if it's needed
  if (filter_result->invalid_files)
    {
      if (filter_result->invalid_files->next)
        {
          toast_title = g_strdup(_("Could not import some files: Not ICS files"));
        }
      else
        {
          g_autofree gchar *basename = g_file_get_basename (filter_result->invalid_files->data);
          toast_title = g_strdup_printf (_("Could not import “%s”: Not an ICS file"), basename);
        }
      error_toast = adw_toast_new (toast_title);
    }

  if (filter_result->ics_files)
    {
      g_clear_pointer (&self->import_dialog, gtk_widget_unparent);

      self->import_dialog = gcal_import_dialog_new_for_file_list (self->context, filter_result->ics_files);
      adw_dialog_present (ADW_DIALOG (self->import_dialog), GTK_WIDGET (self));

      g_object_add_weak_pointer (G_OBJECT (self->import_dialog), (gpointer *)&self->import_dialog);

      if (error_toast)
        gcal_import_dialog_add_toast (GCAL_IMPORT_DIALOG (self->import_dialog), error_toast);
    }
  else
    {
      adw_toast_overlay_add_toast (self->overlay, error_toast);
    }

  g_clear_pointer (&filter_result, free_file_filter_result);
  GCAL_EXIT;
}

static gboolean
on_drop_target_drop_cb (GtkDropTarget *target,
                        const GValue  *value,
                        gdouble        x,
                        gdouble        y,
                        gpointer       user_data)
{
  g_autoptr (GSList) files = NULL;
  GdkFileList *file_list;
  GcalWindow *self;

  GCAL_ENTRY;

  self = GCAL_WINDOW (user_data);
  file_list = g_value_get_boxed (value);
  files = gdk_file_list_get_files (file_list);
  filter_ics_files (self, files, on_ics_files_filtered_cb);

  GCAL_RETURN (TRUE);
}

static gboolean
schedule_open_edit_dialog_by_uuid (OpenEditDialogData *edit_dialog_data)
{
  GcalWindow *window;
  GList *widgets;

  /* FIXME Do that in the date chooser too? */

  window = edit_dialog_data->window;
  widgets = gcal_view_get_children_by_uuid (GCAL_VIEW (window->month_view),
                                            GCAL_RECURRENCE_MOD_THIS_ONLY,
                                            edit_dialog_data->uuid);
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


/*
 * GObject overrides
 */

static void
gcal_window_finalize (GObject *object)
{
  GcalWindow *window = GCAL_WINDOW (object);

  GCAL_ENTRY;

  gcal_clear_timeout (&window->open_edit_dialog_timeout_id);

  if (window->event_creation_data)
    {
      g_clear_pointer (&window->event_creation_data->range, gcal_range_unref);
      g_clear_pointer (&window->event_creation_data, g_free);
    }

  g_clear_object (&window->context);

  gcal_clear_date_time (&window->active_date);

  g_clear_object (&window->colors_provider);

  G_OBJECT_CLASS (gcal_window_parent_class)->finalize (object);

  GCAL_EXIT;
}

static void
gcal_window_dispose (GObject *object)
{
  GcalTimeline *timeline;
  GcalWindow *self;

  self = GCAL_WINDOW (object);

  timeline = gcal_manager_get_timeline (gcal_context_get_manager (self->context));
  gcal_timeline_remove_subscriber (timeline, GCAL_TIMELINE_SUBSCRIBER (self->week_view));
  gcal_timeline_remove_subscriber (timeline, GCAL_TIMELINE_SUBSCRIBER (self->month_view));
  gcal_timeline_remove_subscriber (timeline, GCAL_TIMELINE_SUBSCRIBER (self->agenda_view));
  gcal_timeline_remove_subscriber (timeline, GCAL_TIMELINE_SUBSCRIBER (self->date_chooser));

  g_clear_pointer (&self->quick_add_popover, gtk_widget_unparent);
  g_clear_pointer (&self->import_dialog, gtk_widget_unparent);

  g_clear_weak_pointer (&self->last_focused_widget);

  if (self->delete_event_toast)
    adw_toast_dismiss (self->delete_event_toast);

  G_OBJECT_CLASS (gcal_window_parent_class)->dispose (object);
}

static void
gcal_window_constructed (GObject *object)
{
  GcalWindow *self;
  GSettings *settings;
  gboolean maximized;
  gint height;
  gint width;

  GCAL_ENTRY;

  self = GCAL_WINDOW (object);

  G_OBJECT_CLASS (gcal_window_parent_class)->constructed (object);

  /* Load saved geometry *after* the construct-time properties are set */
  settings = gcal_context_get_settings (self->context);

  maximized = g_settings_get_boolean (settings, "window-maximized");
  g_settings_get (settings, "window-size", "(ii)", &width, &height);

  gtk_window_set_default_size (GTK_WINDOW (self), width, height);

  if (maximized)
    gtk_window_maximize (GTK_WINDOW (self));

  /*
   * FIXME: this is a hack around the issue that happens when trying to bind
   * these properties using the GtkBuilder .ui file.
   */
  g_object_bind_property (self, "context", self->calendars_list, "context", G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
  g_object_bind_property (self, "context", self->weather_settings, "context", G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
  g_object_bind_property (self, "context", self->week_view, "context", G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
  g_object_bind_property (self, "context", self->month_view, "context", G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
  g_object_bind_property (self, "context", self->agenda_view, "context", G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
  g_object_bind_property (self, "context", self->date_chooser, "context", G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
  g_object_bind_property (self, "context", self->event_editor, "context", G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
  g_object_bind_property (self, "context", self->quick_add_popover, "context", G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
  g_object_bind_property (self, "context", self->sync_indicator, "context", G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
  g_object_bind_property (self, "context", self->search_button, "context", G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

  /* CSS */
  load_css_providers (self);

  g_object_connect (gcal_context_get_manager (self->context),
                    "swapped-object-signal::calendar-added", recalculate_calendar_colors_css, self,
                    "swapped-object-signal::calendar-changed", recalculate_calendar_colors_css, self,
                    "swapped-object-signal::calendar-removed", recalculate_calendar_colors_css, self,
                    NULL);
  recalculate_calendar_colors_css (self);

  GCAL_EXIT;
}

static void
gcal_window_set_property (GObject      *object,
                          guint         property_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  GcalWindow *self = GCAL_WINDOW (object);

  GCAL_ENTRY;

  switch (property_id)
    {
    case PROP_ACTIVE_VIEW:
      self->active_view = g_value_get_enum (value);
      adw_view_stack_set_visible_child (self->views_stack, self->views[self->active_view]);
      break;

    case PROP_ACTIVE_DATE:
      update_active_date (GCAL_WINDOW (object), g_value_get_boxed (value));
      break;

    case PROP_NEW_EVENT_MODE:
      set_new_event_mode (GCAL_WINDOW (object), g_value_get_boolean (value));
      break;

    case PROP_CONTEXT:
      g_assert (self->context == NULL);
      self->context = g_value_dup_object (value);

      g_settings_bind (gcal_context_get_settings (self->context),
                       "active-view",
                       self,
                       "active-view",
                       G_SETTINGS_BIND_SET | G_SETTINGS_BIND_GET);

      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }

  GCAL_EXIT;
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
    case PROP_ACTIVE_DATE:
      g_value_set_boxed (value, self->active_date);
      break;

    case PROP_ACTIVE_VIEW:
      g_value_set_enum (value, self->active_view);
      break;

    case PROP_NEW_EVENT_MODE:
      g_value_set_boolean (value, self->new_event_mode);
      break;

    case PROP_CONTEXT:
      g_value_set_object (value, self->context);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}


/*
 * GtkWidget overrides
 */

static void
gcal_window_unmap (GtkWidget *widget)
{
  GcalWindow *self;
  GSettings *settings;
  gint height;
  gint width;

  GCAL_ENTRY;

  self = GCAL_WINDOW (widget);
  settings = gcal_context_get_settings (self->context);

  gtk_window_get_default_size (GTK_WINDOW (self), &width, &height);

  g_settings_set_boolean (settings, "window-maximized", gtk_window_is_maximized (GTK_WINDOW (self)));
  g_settings_set (settings, "window-size", "(ii)", width, height);

  GTK_WIDGET_CLASS (gcal_window_parent_class)->unmap (widget);

  GCAL_EXIT;
}

static void
gcal_window_class_init (GcalWindowClass *klass)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;

  g_type_ensure (GCAL_TYPE_AGENDA_VIEW);
  g_type_ensure (GCAL_TYPE_CALENDAR_LIST);
  g_type_ensure (GCAL_TYPE_CALENDAR_NAVIGATION_BUTTON);
  g_type_ensure (GCAL_TYPE_DATE_CHOOSER);
  g_type_ensure (GCAL_TYPE_EVENT_EDITOR_DIALOG);
  g_type_ensure (GCAL_TYPE_MANAGER);
  g_type_ensure (GCAL_TYPE_MONTH_VIEW);
  g_type_ensure (GCAL_TYPE_QUICK_ADD_POPOVER);
  g_type_ensure (GCAL_TYPE_SYNC_INDICATOR);
  g_type_ensure (GCAL_TYPE_SEARCH_BUTTON);
  g_type_ensure (GCAL_TYPE_WEATHER_SETTINGS);
  g_type_ensure (GCAL_TYPE_WEEK_VIEW);
  g_type_ensure (GCAL_TYPE_DROP_OVERLAY);

  object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = gcal_window_finalize;
  object_class->constructed = gcal_window_constructed;
  object_class->dispose = gcal_window_dispose;
  object_class->set_property = gcal_window_set_property;
  object_class->get_property = gcal_window_get_property;

  widget_class = GTK_WIDGET_CLASS (klass);
  widget_class->unmap = gcal_window_unmap;


  properties[PROP_ACTIVE_DATE] = g_param_spec_boxed ("active-date",
                                                     "Date",
                                                     "The active/selected date",
                                                     G_TYPE_DATE_TIME,
                                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_ACTIVE_VIEW] = g_param_spec_enum ("active-view",
                                                    "Active View",
                                                    "The active view, eg: month, week, etc.",
                                                    GCAL_TYPE_WINDOW_VIEW,
                                                    GCAL_WINDOW_VIEW_MONTH,
                                                    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_CONTEXT] = g_param_spec_object ("context",
                                                  "Context",
                                                  "Context",
                                                  GCAL_TYPE_CONTEXT,
                                                  G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  properties[PROP_NEW_EVENT_MODE] = g_param_spec_boolean ("new-event-mode",
                                                          "New Event mode",
                                                          "Whether the window is in new-event-mode or not",
                                                          FALSE,
                                                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/gui/gcal-window.ui");

  /* widgets */
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, agenda_view);
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, calendars_list);
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, date_chooser);
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, event_editor);
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, header_bar);
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, menu_button);
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, month_view);
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, quick_add_popover);
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, overlay);
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, split_view);
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, sync_indicator);
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, search_button);
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, views_stack);
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, weather_settings);
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, week_view);
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, drop_overlay);
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, drop_target);

  gtk_widget_class_bind_template_callback (widget_class, on_breakpoint_changed);
  gtk_widget_class_bind_template_callback (widget_class, view_changed);
  gtk_widget_class_bind_template_callback (widget_class, get_previous_date_icon);
  gtk_widget_class_bind_template_callback (widget_class, get_previous_date_tooltip);
  gtk_widget_class_bind_template_callback (widget_class, get_next_date_icon);
  gtk_widget_class_bind_template_callback (widget_class, get_next_date_tooltip);

  /* Event creation related */
  gtk_widget_class_bind_template_callback (widget_class, close_new_event_widget);
  gtk_widget_class_bind_template_callback (widget_class, content_title);
  gtk_widget_class_bind_template_callback (widget_class, create_event_detailed_cb);
  gtk_widget_class_bind_template_callback (widget_class, day_selected);
  gtk_widget_class_bind_template_callback (widget_class, edit_event);
  gtk_widget_class_bind_template_callback (widget_class, event_activated);
  gtk_widget_class_bind_template_callback (widget_class, show_new_event_widget);

  /* Edit dialog related */
  gtk_widget_class_bind_template_callback (widget_class, on_event_editor_dialog_remove_event_cb);

  /* Drop Target related */
  gtk_widget_class_bind_template_callback (widget_class, on_drop_target_accept_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_drop_target_drop_cb);
}

static void
gcal_window_init (GcalWindow *self)
{
  static const GActionEntry actions[] = {
    {"change-view", on_view_action_activated, "i" },
    {"next-date", on_window_next_date_activated_cb },
    {"new-event", on_window_new_event_cb },
    {"open-date-time-settings", on_window_open_date_time_settings_cb },
    {"open-online-accounts", on_window_open_online_accounts_cb },
    {"previous-date", on_window_previous_date_activated_cb },
    {"show-calendars", on_show_calendars_action_activated },
    {"today", on_window_today_activated_cb },
    {"undo-delete-event", on_window_undo_delete_event_cb },
  };

  /* Setup actions */
  g_action_map_add_action_entries (G_ACTION_MAP (self),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   self);

  gtk_widget_init_template (GTK_WIDGET (self));

  self->views[GCAL_WINDOW_VIEW_WEEK] = self->week_view;
  self->views[GCAL_WINDOW_VIEW_MONTH] = self->month_view;
  self->views[GCAL_WINDOW_VIEW_AGENDA] = self->agenda_view;

  self->active_date = g_date_time_new_from_unix_local (0);
  self->rtl = gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL;

  /* devel styling */
  if (g_strcmp0 (PROFILE, "Devel") == 0)
    gtk_widget_add_css_class (GTK_WIDGET (self), "devel");

  gtk_widget_set_parent (self->quick_add_popover, GTK_WIDGET (self));

  GType drop_types[] = { GDK_TYPE_FILE_LIST };
  gtk_drop_target_set_gtypes (self->drop_target, drop_types,
                              G_N_ELEMENTS(drop_types));

  gcal_drop_overlay_set_drop_target(self->drop_overlay, self->drop_target);
}

/**
 * gcal_window_new_with_date:
 * @app: a #GcalApplication
 * @date: the active date
 *
 * Creates a #GcalWindow, positions it at @date.
 *
 * Returns: (transfer full): a #GcalWindow
 */
GtkWidget*
gcal_window_new_with_date (GcalApplication *app,
                           GDateTime       *date)
{
  return g_object_new (GCAL_TYPE_WINDOW,
                       "application", GTK_APPLICATION (app),
                       "context", gcal_application_get_context (app),
                       "active-date", date,
                       NULL);
}

/**
 * gcal_window_set_search_query:
 * @self: a #GcalWindow
 * @query: sets the search query of @self
 *
 * Sets the search query of the search. #GcalWindow only
 * propagates this to the search bar, which ends up
 * triggering the search.
 */
void
gcal_window_set_search_query (GcalWindow  *self,
                              const gchar *query)
{
  g_return_if_fail (GCAL_IS_WINDOW (self));

  gcal_search_button_search (GCAL_SEARCH_BUTTON (self->search_button), query);
}

/**
 * gcal_window_open_event_by_uuid:
 * @self: a #GcalWindow
 * @uuid: the unique identifier of the event to be opened
 *
 * Tells @self to open the event with @uuid. When it fails to
 * open the event, it waits for 2 seconds before trying again.
 */
void
gcal_window_open_event_by_uuid (GcalWindow  *self,
                                const gchar *uuid)
{
  GList *widgets;

  /* XXX: show events on month view */
  adw_view_stack_set_visible_child (self->views_stack, self->month_view);

  widgets = gcal_view_get_children_by_uuid (GCAL_VIEW (self->month_view),
                                            GCAL_RECURRENCE_MOD_THIS_ONLY,
                                            uuid);

  if (widgets)
    {
      event_activated (NULL, widgets->data, self);
      g_list_free (widgets);
    }
  else
    {
      OpenEditDialogData *edit_dialog_data = g_new0 (OpenEditDialogData, 1);
      edit_dialog_data->window = self;
      edit_dialog_data->uuid = g_strdup (uuid);
      self->open_edit_dialog_timeout_id = g_timeout_add_seconds (2,
                                                                 (GSourceFunc) schedule_open_edit_dialog_by_uuid,
                                                                 edit_dialog_data);
    }
}

void
gcal_window_import_files (GcalWindow  *self,
                          GFile      **files,
                          gint         n_files)
{
  g_return_if_fail (GCAL_IS_WINDOW (self));

  g_clear_pointer (&self->import_dialog, gtk_widget_unparent);

  self->import_dialog = gcal_import_dialog_new_for_files (self->context, files, n_files);
  adw_dialog_present (ADW_DIALOG (self->import_dialog), GTK_WIDGET (self));

  g_object_add_weak_pointer (G_OBJECT (self->import_dialog), (gpointer *)&self->import_dialog);
}
