/* gcal-event-popover.c
 *
 * Copyright 2020 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "GcalEventPopover"

#include "gcal-debug.h"
#include "gcal-event-popover.h"
#include "gcal-meeting-row.h"
#include "gcal-utils.h"

#include <glib/gi18n.h>

struct _GcalEventPopover
{
  GtkPopover          parent;

  GtkLabel           *date_time_label;
  GtkLabel           *description_label;
  GtkWidget          *edit_button;
  GtkWidget          *location_box;
  GtkLabel           *location_label;
  GtkListBox         *meetings_listbox;
  GtkLabel           *placeholder_label;
  GtkLabel           *summary_label;

  GcalContext        *context;
  GcalEvent          *event;
};

static void          on_join_meeting_cb                          (GcalMeetingRow     *meeting_row,
                                                                  const gchar        *url,
                                                                  GcalEventPopover   *self);

G_DEFINE_TYPE (GcalEventPopover, gcal_event_popover, GTK_TYPE_POPOVER)

enum
{
  PROP_0,
  PROP_CONTEXT,
  PROP_EVENT,
  N_PROPS
};

enum
{
  EDIT,
  NUM_SIGNALS
};

static guint signals[NUM_SIGNALS] = { 0, };
static GParamSpec *properties[N_PROPS] = { NULL, };


/*
 * Auxiliary methods
 */

static gint
get_number_of_days_from_today (GDateTime *now,
                               GDateTime *day)
{
  GDate event_day;
  GDate today;

  g_date_set_dmy (&today,
                  g_date_time_get_day_of_month (now),
                  g_date_time_get_month (now),
                  g_date_time_get_year (now));

  g_date_set_dmy (&event_day,
                  g_date_time_get_day_of_month (day),
                  g_date_time_get_month (day),
                  g_date_time_get_year (day));

  return g_date_days_between (&today, &event_day);
}

static gchar*
format_time (GcalEventPopover *self,
             GDateTime        *date)
{
  GcalTimeFormat time_format;

  time_format = gcal_context_get_time_format (self->context);

  switch (time_format)
    {
    case GCAL_TIME_FORMAT_12H:
      return g_date_time_format (date, "%I:%M %P");

    case GCAL_TIME_FORMAT_24H:
      return g_date_time_format (date, "%R");

    default:
      g_assert_not_reached ();
    }

  return NULL;
}

static const gchar*
get_month_name (gint month)
{
  const gchar *month_names[] = {
    N_("January"),
    N_("February"),
    N_("March"),
    N_("April"),
    N_("May"),
    N_("June"),
    N_("July"),
    N_("August"),
    N_("September"),
    N_("October"),
    N_("November"),
    N_("December"),
    NULL
  };

  return gettext (month_names[month]);
}

static gchar*
format_multiday_date (GcalEventPopover *self,
                      GDateTime        *dt,
                      gboolean          force_show_year,
                      gboolean          show_time)
{
  g_autoptr (GDateTime) now = NULL;
  gint n_days_from_dt;

  now = g_date_time_new_now_local ();
  n_days_from_dt = get_number_of_days_from_today (now, dt);

  if (show_time)
    {
      g_autofree gchar *hours = format_time (self, dt);

      if (n_days_from_dt == 0)
        {
          return g_strdup_printf (_("Today %s"), hours);
        }
      else if (n_days_from_dt == 1)
        {
          return g_strdup_printf (_("Tomorrow %s"), hours);
        }
      else if (n_days_from_dt == -1)
        {
          return g_strdup_printf (_("Yesterday %s"), hours);
        }
      else if (!force_show_year && g_date_time_get_year (now) == g_date_time_get_year (dt))
        {
          /*
           * Translators: %1$s is a month name (e.g. November), %2$d is the day
           * of month, and %3$ is the hour. This format string results in dates
           * like "November 21, 22:00".
           */
          return g_strdup_printf (_("%1$s %2$d, %3$s"),
                                  get_month_name (g_date_time_get_month (dt) - 1),
                                  g_date_time_get_day_of_month (dt),
                                  hours);
        }
      else
        {
          /*
           * Translators: %1$s is a month name (e.g. November), %2$d is the day
           * of month, %3$d is the year, and %4$s is the hour. This format string
           * results in dates like "November 21, 2020, 22:00".
           */
          return g_strdup_printf (_("%1$s %2$d, %3$d, %4$s"),
                                  get_month_name (g_date_time_get_month (dt) - 1),
                                  g_date_time_get_day_of_month (dt),
                                  g_date_time_get_year (dt),
                                  hours);
        }

    }
  else
    {
      if (n_days_from_dt == 0)
        {
          return g_strdup (_("Today"));
        }
      else if (n_days_from_dt == 1)
        {
          return g_strdup (_("Tomorrow"));
        }
      else if (n_days_from_dt == -1)
        {
          return g_strdup (_("Yesterday"));
        }
      else if (!force_show_year && g_date_time_get_year (now) == g_date_time_get_year (dt))
        {
          /*
           * Translators: %1$s is a month name (e.g. November), and %2$d is
           * the day of month. This format string results in dates like
           * "November 21".
           */
          return g_strdup_printf (_("%1$s %2$d"),
                                  get_month_name (g_date_time_get_month (dt) - 1),
                                  g_date_time_get_day_of_month (dt));
        }
      else
        {
          /*
           * Translators: %1$s is a month name (e.g. November), %2$d is the day
           * of month, and %3$d is the year. This format string results in dates
           * like "November 21, 2020".
           */
          return g_strdup_printf (_("%1$s %2$d, %3$d"),
                                  get_month_name (g_date_time_get_month (dt) - 1),
                                  g_date_time_get_day_of_month (dt),
                                  g_date_time_get_year (dt));
        }

    }

  g_assert_not_reached ();
}

static gchar*
format_single_day (GcalEventPopover *self,
                   GDateTime        *start_dt,
                   GDateTime        *end_dt,
                   gboolean          show_time)
{
  g_autoptr (GDateTime) now = NULL;
  gint n_days_from_dt;

  now = g_date_time_new_now_local ();
  n_days_from_dt = get_number_of_days_from_today (now, start_dt);

  if (show_time)
    {
      g_autofree gchar *start_hours = format_time (self, start_dt);
      g_autofree gchar *end_hours = format_time (self, end_dt);

      if (n_days_from_dt == 0)
        {
          /*
           * Translators: %1$s is the start hour, and %2$s is the end hour, for
           * example: "Today, 19:00 — 22:00"
           */
          return g_strdup_printf (_("Today, %1$s — %2$s"), start_hours, end_hours);
        }
      else if (n_days_from_dt == 1)
        {
          /*
           * Translators: %1$s is the start hour, and %2$s is the end hour, for
           * example: "Tomorrow, 19:00 — 22:00"
           */
          return g_strdup_printf (_("Tomorrow, %1$s – %2$s"), start_hours, end_hours);
        }
      else if (n_days_from_dt == -1)
        {
          /*
           * Translators: %1$s is the start hour, and %2$s is the end hour, for
           * example: "Tomorrow, 19:00 — 22:00"
           */
          return g_strdup_printf (_("Yesterday, %1$s – %2$s"), start_hours, end_hours);
        }
      else if (g_date_time_get_year (now) == g_date_time_get_year (start_dt))
        {
          /*
           * Translators: %1$s is a month name (e.g. November), %2$d is the day
           * of month, %3$s is the start hour, and %4$s is the end hour. This
           * format string results in dates like "November 21, 19:00 — 22:00".
           */
          return g_strdup_printf (_("%1$s %2$d, %3$s – %4$s"),
                                  get_month_name (g_date_time_get_month (start_dt) - 1),
                                  g_date_time_get_day_of_month (start_dt),
                                  start_hours,
                                  end_hours);
        }
      else
        {
          /*
           * Translators: %1$s is a month name (e.g. November), %2$d is the day
           * of month, %3$d is the year, %4$s is the start hour, and %5$s is the
           * end hour. This format string results in dates like:
           *
           * "November 21, 2021, 19:00 — 22:00".
           */
          return g_strdup_printf (_("%1$s %2$d, %3$d, %4$s – %5$s"),
                                  get_month_name (g_date_time_get_month (start_dt) - 1),
                                  g_date_time_get_day_of_month (start_dt),
                                  g_date_time_get_year (start_dt),
                                  start_hours,
                                  end_hours);
        }
    }
  else
    {
      if (n_days_from_dt == 0)
        {
          return g_strdup (_("Today"));
        }
      else if (n_days_from_dt == 1)
        {
          return g_strdup (_("Tomorrow"));
        }
      else if (n_days_from_dt == -1)
        {
          return g_strdup (_("Yesterday"));
        }
      else if (g_date_time_get_year (now) == g_date_time_get_year (start_dt))
        {
          /*
           * Translators: %1$s is a month name (e.g. November), and %2$d is
           * the day of month. This format string results in dates like
           * "November 21".
           */
          return g_strdup_printf (_("%1$s %2$d"),
                                  get_month_name (g_date_time_get_month (start_dt) - 1),
                                  g_date_time_get_day_of_month (start_dt));
        }
      else
        {
          /*
           * Translators: %1$s is a month name (e.g. November), %2$d is the day
           * of month, and %3$d is the year. This format string results in dates
           * like "November 21, 2020".
           */
          return g_strdup_printf (_("%1$s %2$d, %3$d"),
                                  get_month_name (g_date_time_get_month (start_dt) - 1),
                                  g_date_time_get_day_of_month (start_dt),
                                  g_date_time_get_year (start_dt));
        }
    }

  g_assert_not_reached ();
}

static void
update_date_time_label (GcalEventPopover *self)
{
  g_autoptr (GDateTime) start_dt = NULL;
  g_autoptr (GDateTime) end_dt = NULL;
  g_autoptr (GString) string = NULL;
  gboolean show_hours;
  gboolean multiday;
  gboolean all_day;

  string = g_string_new ("");
  all_day = gcal_event_get_all_day (self->event);
  multiday = gcal_event_is_multiday (self->event);
  show_hours = !all_day;

  if (all_day)
    {
      start_dt = g_date_time_ref (gcal_event_get_date_start (self->event));
      end_dt = g_date_time_ref (gcal_event_get_date_end (self->event));
    }
  else
    {
      start_dt = g_date_time_to_local (gcal_event_get_date_start (self->event));
      end_dt = g_date_time_to_local (gcal_event_get_date_end (self->event));
    }

  if (multiday)
    {
      g_autofree gchar *start_str = NULL;
      g_autofree gchar *end_str = NULL;
      gboolean show_year;
      g_autoptr (GDateTime) real_end_dt = NULL;

      show_year = g_date_time_get_year (start_dt) != g_date_time_get_year (end_dt);
      start_str = format_multiday_date (self, start_dt, show_year, show_hours);

      if (!show_hours)
        real_end_dt = g_date_time_add_days (end_dt, -1);
      else
        real_end_dt = g_date_time_ref (end_dt);
      
      end_str = format_multiday_date (self, real_end_dt, show_year, show_hours);

      /* Translators: %1$s is the start date, and %2$s. For example: June 21 - November 29, 2022 */
      g_string_printf (string, _("%1$s — %2$s"), start_str, end_str);

    }
  else
    {
      g_autofree gchar *str = format_single_day (self, start_dt, end_dt, show_hours);
      g_string_append (string, str);
    }

  gtk_label_set_label (self->date_time_label, string->str);
}

static void
update_placeholder_label (GcalEventPopover *self)
{
  gboolean placeholder_visible = FALSE;

  placeholder_visible |= !gtk_widget_get_visible (GTK_WIDGET (self->location_box)) &&
                         !gtk_widget_get_visible (GTK_WIDGET (self->description_label));
  gtk_widget_set_visible (GTK_WIDGET (self->placeholder_label), placeholder_visible);
}

static void
add_meeting (GcalEventPopover *self,
             const gchar      *url)
{
  GtkWidget *row;

  row = gcal_meeting_row_new (url);
  g_signal_connect (row, "join-meeting", G_CALLBACK (on_join_meeting_cb), self);
  gtk_list_box_append (self->meetings_listbox, row);

  gtk_widget_set_visible (GTK_WIDGET (self->meetings_listbox), TRUE);
}

static void
setup_location_label (GcalEventPopover *self)
{
  g_autoptr (GUri) guri = NULL;
  g_autofree gchar *location = NULL;

  location = g_strdup (gcal_event_get_location (self->event));
  g_strstrip (location);

  guri = g_uri_parse (location, SOUP_HTTP_URI_FLAGS | G_URI_FLAGS_PARSE_RELAXED, NULL);
  if (guri)
    {
      GString *string;

      string = g_string_new (NULL);
      g_string_append (string, "<a href=\"");
      g_string_append (string, location);
      g_string_append (string, "\">");
      g_string_append (string, location);
      g_string_append (string, "</a>");

      add_meeting (self, location);

      g_clear_pointer (&location, g_free);
      location = g_string_free (string, FALSE);
    }

  gtk_widget_set_visible (self->location_box,
                          location && g_utf8_strlen (location, -1) > 0);
  gtk_label_set_markup (self->location_label, location);
}

static void
setup_description_label (GcalEventPopover *self)
{
  g_autofree gchar *description = NULL;
  g_autofree gchar *meeting_url = NULL;
  g_autoptr (GString) string = NULL;

  gcal_utils_extract_google_section (gcal_event_get_description (self->event),
                                     &description,
                                     &meeting_url);
  g_strstrip (description);

  if (meeting_url)
    add_meeting (self, meeting_url);

  string = g_string_new (description);
  g_string_replace (string, "<br>", "\n", 0);
  g_string_replace (string, "&nbsp;", " ", 0);

  gtk_label_set_markup (self->description_label, string->str);
  gtk_widget_set_visible (GTK_WIDGET (self->description_label), string->str && *string->str);
}

static void
set_event_internal (GcalEventPopover *self,
                    GcalEvent        *event)
{
  g_set_object (&self->event, event);

  gtk_label_set_label (self->summary_label, gcal_event_get_summary (event));

  setup_description_label (self);
  setup_location_label (self);
  update_placeholder_label (self);
  update_date_time_label (self);

  gtk_widget_grab_focus (self->edit_button);
}


/*
 * Callbacks
 */

static void
on_edit_button_clicked_cb (GtkButton        *edit_button,
                           GcalEventPopover *self)
{
  g_signal_emit (self, signals[EDIT], 0);
  gtk_popover_popdown (GTK_POPOVER (self));
}

static void
on_uri_launched_cb (GObject      *source_object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  g_autoptr (GcalEventPopover) self = user_data;
  g_autoptr (GError) error = NULL;

  if (!gtk_uri_launcher_launch_finish (GTK_URI_LAUNCHER (source_object), result, &error))
    {
      g_warning ("Error launching URI: %s", error->message);
      return;
    }

  gtk_popover_popdown (GTK_POPOVER (self));
}

static void
on_join_meeting_cb (GcalMeetingRow   *meeting_row,
                    const gchar      *url,
                    GcalEventPopover *self)
{
  g_autoptr (GtkUriLauncher) uri_launcher = NULL;
  GtkWindow *window;

  window = GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (self)));
  g_assert (window != NULL);

  uri_launcher = gtk_uri_launcher_new (url);
  gtk_uri_launcher_launch (uri_launcher,
                           window,
                           NULL,
                           on_uri_launched_cb,
                           g_object_ref (self));
}

static void
on_time_format_changed_cb (GcalEventPopover *self)
{
  GCAL_ENTRY;

  update_date_time_label (self);

  GCAL_EXIT;
}


/*
 * GtkWidget overrides
 */

static void
gcal_event_popover_map (GtkWidget *widget)
{
  GcalEventPopover *self = (GcalEventPopover *) widget;
  GtkListBoxRow *first_meeting_row;

  GTK_WIDGET_CLASS (gcal_event_popover_parent_class)->map (widget);

  first_meeting_row = gtk_list_box_get_row_at_index (self->meetings_listbox, 0);

  if (first_meeting_row)
    gtk_widget_grab_focus (GTK_WIDGET (first_meeting_row));
  else
    gtk_widget_grab_focus (self->edit_button);
}


/*
 * GObject overrides
 */

static void
gcal_event_popover_finalize (GObject *object)
{
  GcalEventPopover *self = (GcalEventPopover *)object;

  g_clear_object (&self->context);
  g_clear_object (&self->event);

  G_OBJECT_CLASS (gcal_event_popover_parent_class)->finalize (object);
}

static void
gcal_event_popover_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GcalEventPopover *self = GCAL_EVENT_POPOVER (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_value_set_object (value, self->context);
      break;

    case PROP_EVENT:
      g_value_set_object (value, self->event);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_event_popover_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GcalEventPopover *self = GCAL_EVENT_POPOVER (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_assert (self->context == NULL);
      self->context = g_value_dup_object (value);
      g_signal_connect_object (self->context,
                               "notify::time-format",
                               G_CALLBACK (on_time_format_changed_cb),
                               self,
                               G_CONNECT_SWAPPED);
      break;

    case PROP_EVENT:
      g_assert (self->event == NULL);
      set_event_internal (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_event_popover_class_init (GcalEventPopoverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gcal_event_popover_finalize;
  object_class->get_property = gcal_event_popover_get_property;
  object_class->set_property = gcal_event_popover_set_property;

  widget_class->map = gcal_event_popover_map;

  /**
   * GcalEventPopover::context:
   *
   * The context of the event popover.
   */
  properties[PROP_CONTEXT] = g_param_spec_object ("context",
                                                  "Context",
                                                  "Context",
                                                  GCAL_TYPE_CONTEXT,
                                                  G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  /**
   * GcalEventPopover::event:
   *
   * The event this popover represents.
   */
  properties[PROP_EVENT] = g_param_spec_object ("event",
                                                "Event",
                                                "The event this popover represents",
                                                GCAL_TYPE_EVENT,
                                                G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals[EDIT] = g_signal_new ("edit",
                                GCAL_TYPE_EVENT_POPOVER,
                                G_SIGNAL_RUN_LAST,
                                0,
                                NULL, NULL,
                                g_cclosure_marshal_VOID__VOID,
                                G_TYPE_NONE,
                                0);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/gui/gcal-event-popover.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalEventPopover, date_time_label);
  gtk_widget_class_bind_template_child (widget_class, GcalEventPopover, description_label);
  gtk_widget_class_bind_template_child (widget_class, GcalEventPopover, edit_button);
  gtk_widget_class_bind_template_child (widget_class, GcalEventPopover, location_box);
  gtk_widget_class_bind_template_child (widget_class, GcalEventPopover, location_label);
  gtk_widget_class_bind_template_child (widget_class, GcalEventPopover, meetings_listbox);
  gtk_widget_class_bind_template_child (widget_class, GcalEventPopover, placeholder_label);
  gtk_widget_class_bind_template_child (widget_class, GcalEventPopover, summary_label);

  gtk_widget_class_bind_template_callback (widget_class, on_edit_button_clicked_cb);
}

static void
gcal_event_popover_init (GcalEventPopover *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget*
gcal_event_popover_new (GcalContext *context,
                        GcalEvent   *event)
{
  return g_object_new (GCAL_TYPE_EVENT_POPOVER,
                       "context", context,
                       "event", event,
                       NULL);
}
