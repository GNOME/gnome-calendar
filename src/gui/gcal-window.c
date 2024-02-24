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

#include "css-code.h"
#include "gcal-agenda-view.h"
#include "gcal-calendar-management-dialog.h"
#include "gcal-calendar-button.h"
#include "config.h"
#include "gcal-date-chooser.h"
#include "gcal-debug.h"
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
#include "gcal-toolbar-end.h"
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

struct _GcalWindow
{
  AdwApplicationWindow parent;

  /* upper level widgets */
  GtkWidget              *main_box;

  GtkWidget              *header_bar;
  AdwToastOverlay        *overlay;
  AdwViewStack           *views_stack;
  GtkWidget              *week_view;
  GtkWidget              *month_view;
  GtkWidget              *agenda_view;
  GtkWidget              *date_chooser;
  GtkWidget              *action_bar;
  GcalSyncIndicator      *sync_indicator;
  GcalToolbarEnd         *toolbar_end;
  GcalToolbarEnd         *sidebar_toolbar_end;
  AdwNavigationSplitView *split_view;

  /* header_bar widgets */
  GtkWidget          *calendars_button;
  GtkWidget          *menu_button;
  GtkWidget          *views_switcher;

  GcalEventEditorDialog *event_editor;
  GtkWindow             *import_dialog;

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

  /* calendar management */
  GtkWidget          *calendar_management_dialog;

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
                    GDateTime  *new_date)
{
  g_autofree gchar *new_date_string = NULL;
  GcalWindowView i;

  GCAL_ENTRY;

  new_date_string = g_date_time_format (new_date, "%x %X %z");
  g_debug ("Updating active date to %s", new_date_string);

  gcal_set_date_time (&window->active_date, new_date);

  for (i = GCAL_WINDOW_VIEW_WEEK; i <= GCAL_WINDOW_VIEW_MONTH; i++)
    gcal_view_set_date (GCAL_VIEW (window->views[i]), new_date);
  gcal_view_set_date (GCAL_VIEW (window->agenda_view), new_date);
  gcal_view_set_date (GCAL_VIEW (window->date_chooser), new_date);

  update_today_action_enabled (window);

  maybe_add_subscribers_to_timeline (window);

  GCAL_EXIT;
}

static void
recalculate_calendar_colors_css (GcalWindow *self)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (GList) calendars = NULL;
  g_autofree gchar *new_css_data = NULL;
  g_auto (GStrv) new_css_snippets = NULL;
  GcalManager *manager;
  GQuark color_id;
  GList *l;
  gint arr_length;
  gint i = 0;

  manager = gcal_context_get_manager (self->context);
  calendars = gcal_manager_get_calendars (manager);
  arr_length = g_list_length (calendars);
  new_css_snippets = g_new0 (gchar*, arr_length + 2);
  for (l = calendars; l; l = l->next, i++)
    {
      g_autofree gchar* color_str = NULL;
      const GdkRGBA *color;
      GcalCalendar *calendar;

      calendar = GCAL_CALENDAR (l->data);

      color = gcal_calendar_get_color (calendar);
      color_str = gdk_rgba_to_string (color);
      color_id = g_quark_from_string (color_str);

      new_css_snippets[i] = g_strdup_printf (CSS_TEMPLATE, color_id, color_str);
    }

  new_css_data = g_strjoinv ("\n", new_css_snippets);
  gtk_css_provider_load_from_string (self->colors_provider, new_css_data);

  if (error)
    g_warning ("Error creating custom stylesheet. %s", error->message);
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
  if (active_view == GCAL_WINDOW_VIEW_WEEK)
    return g_strdup (_("Week"));
  else
    return g_strdup (_("Month"));
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

  adw_dialog_present (ADW_DIALOG (self->calendar_management_dialog), GTK_WIDGET (self));
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

  /* -1 means next view */
  if (view == -1)
    view = ++(window->active_view);
  else if (view == -2)
    view = --(window->active_view);

  window->active_view = CLAMP (view, GCAL_WINDOW_VIEW_WEEK, GCAL_WINDOW_VIEW_MONTH);
  adw_view_stack_set_visible_child (window->views_stack, window->views[window->active_view]);

  g_object_notify_by_pspec (G_OBJECT (user_data), properties[PROP_ACTIVE_VIEW]);

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
  start = g_date_time_new (gcal_context_get_timezone (self->context),
                           g_date_time_get_year (self->active_date),
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
      break;
    }
}

static void
day_selected (GcalWindow *self)
{
  update_active_date (self, gcal_view_get_date (GCAL_VIEW (self->date_chooser)));
}

static void
event_activated (GcalView        *view,
                 GcalEventWidget *event_widget,
                 gpointer         user_data)
{
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
  g_clear_object (&window->views_switcher);

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
  g_object_bind_property (self, "context", self->calendars_button, "context", G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
  g_object_bind_property (self, "context", self->weather_settings, "context", G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
  g_object_bind_property (self, "context", self->calendar_management_dialog, "context", G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
  g_object_bind_property (self, "context", self->week_view, "context", G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
  g_object_bind_property (self, "context", self->month_view, "context", G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
  g_object_bind_property (self, "context", self->agenda_view, "context", G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
  g_object_bind_property (self, "context", self->date_chooser, "context", G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
  g_object_bind_property (self, "context", self->event_editor, "context", G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
  g_object_bind_property (self, "context", self->quick_add_popover, "context", G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
  g_object_bind_property (self, "context", self->sync_indicator, "context", G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
  g_object_bind_property (self, "context", self->toolbar_end, "context", G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
  g_object_bind_property (self, "context", self->sidebar_toolbar_end, "context", G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

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
      gtk_widget_set_visible (self->views[self->active_view], TRUE);
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
  g_type_ensure (GCAL_TYPE_CALENDAR_MANAGEMENT_DIALOG);
  g_type_ensure (GCAL_TYPE_CALENDAR_BUTTON);
  g_type_ensure (GCAL_TYPE_DATE_CHOOSER);
  g_type_ensure (GCAL_TYPE_EVENT_EDITOR_DIALOG);
  g_type_ensure (GCAL_TYPE_MANAGER);
  g_type_ensure (GCAL_TYPE_MONTH_VIEW);
  g_type_ensure (GCAL_TYPE_QUICK_ADD_POPOVER);
  g_type_ensure (GCAL_TYPE_SYNC_INDICATOR);
  g_type_ensure (GCAL_TYPE_TOOLBAR_END);
  g_type_ensure (GCAL_TYPE_WEATHER_SETTINGS);
  g_type_ensure (GCAL_TYPE_WEEK_VIEW);

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
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, calendars_button);
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, date_chooser);
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, event_editor);
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, header_bar);
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, main_box);
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, menu_button);
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, month_view);
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, action_bar);
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, quick_add_popover);
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, calendar_management_dialog);
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, overlay);
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, sidebar_toolbar_end);
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, split_view);
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, sync_indicator);
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, toolbar_end);
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, views_stack);
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, views_switcher);
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, weather_settings);
  gtk_widget_class_bind_template_child (widget_class, GcalWindow, week_view);

  gtk_widget_class_bind_template_callback (widget_class, view_changed);

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

  self->active_date = g_date_time_new_from_unix_local (0);
  self->rtl = gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL;

  /* devel styling */
  if (g_strcmp0 (PROFILE, "Devel") == 0)
    gtk_widget_add_css_class (GTK_WIDGET (self), "devel");

  gtk_widget_set_parent (self->quick_add_popover, GTK_WIDGET (self));
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
  GtkWidget *search_button;
  GcalToolbarEnd *toolbar_end;

  g_return_if_fail (GCAL_IS_WINDOW (self));

  if (adw_navigation_split_view_get_collapsed (self->split_view))
    toolbar_end = self->sidebar_toolbar_end;
  else
    toolbar_end = self->toolbar_end;

  search_button = gcal_toolbar_end_get_search_button (toolbar_end);
  gcal_search_button_search (GCAL_SEARCH_BUTTON (search_button), query);
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

  g_clear_pointer (&self->import_dialog, gtk_window_destroy);

  self->import_dialog = GTK_WINDOW (gcal_import_dialog_new_for_files (self->context, files, n_files));
  gtk_window_set_transient_for (self->import_dialog, GTK_WINDOW (self));
  gtk_window_present (self->import_dialog);

  g_object_add_weak_pointer (G_OBJECT (self->import_dialog), (gpointer *)&self->import_dialog);
}
