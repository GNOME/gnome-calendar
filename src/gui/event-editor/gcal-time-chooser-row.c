/* gcal-time-chooser-row.c
 *
 * Copyright (C) 2024 Titouan Real <titouan.real@gmail.com>
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

#define G_LOG_DOMAIN "GcalTimeChooserRow"

#include "gcal-time-chooser-row.h"
#include "gcal-time-selector.h"

#include <locale.h>
#include <langinfo.h>
#include <stdlib.h>
#include <glib/gi18n.h>

struct _GcalTimeChooserRow
{
  AdwEntryRow         parent;

  /* widgets */
  GcalTimeSelector   *time_selector;

  GcalTimeFormat      time_format;
};

G_DEFINE_TYPE (GcalTimeChooserRow, gcal_time_chooser_row, ADW_TYPE_ENTRY_ROW);

enum
{
  PROP_0,
  PROP_TIME_FORMAT,
  PROP_TIME,
  N_PROPS
};

static GParamSpec* properties[N_PROPS] = { NULL, };


/*
 * Auxiliary methods
 */

static void
update_entry (GcalTimeChooserRow *self)
{
  g_autofree gchar *label = NULL;
  GDateTime *time;

  time = gcal_time_selector_get_time (self->time_selector);

  switch (self->time_format)
    {
    case GCAL_TIME_FORMAT_12H:
      label = g_date_time_format (time, "%I:%M %p");
      break;

    case GCAL_TIME_FORMAT_24H:
      label = g_date_time_format (time, "%R");
      break;

    default:
      g_assert_not_reached ();
    }

  gtk_editable_set_text (GTK_EDITABLE (self), label);
}

static GRegex*
get_12h_regex (void)
{
  static GRegex *regex = NULL;

  if (g_once_init_enter_pointer (&regex))
    {
      g_autoptr (GError) error = NULL;
      g_autofree gchar *str = NULL;

      str = g_strdup_printf ("^(?<hour>[0-9]{1,2})([\\-:./_,'a-z](?<minute>[0-9]{1,2}))? ?(?<ampm>%s|%s)",
                             _("AM"), _("PM"));

      regex = g_regex_new (str, G_REGEX_CASELESS, 0, &error);

      if (error)
        g_error ("Failed to compile regex: %s. Aborting...", error->message);
    }

  return regex;
}

static GRegex*
get_24h_regex (void)
{
  static GRegex *regex = NULL;

  if (g_once_init_enter_pointer (&regex))
    {
      g_autoptr (GError) error = NULL;

      regex = g_regex_new ("^(?<hour>[0-9]{1,2})([\\-:./_,'a-z](?<minute>[0-9]{1,2}))?", G_REGEX_CASELESS, 0, &error);

      if (error)
        g_error ("Failed to compile regex: %s. Aborting...", error->message);
    }

  return regex;
}

static void
parse_time (GcalTimeChooserRow *self)
{
  g_autoptr (GDateTime) now = NULL;
  const gchar *text;

  text = gtk_editable_get_text (GTK_EDITABLE (self));
  now = g_date_time_new_now_local ();

  switch (self->time_format)
    {
    case GCAL_TIME_FORMAT_12H:
      {
        g_autoptr (GMatchInfo) match_info = NULL;
        g_autoptr (GDateTime) time = NULL;
        g_autofree gchar *minute_str = NULL;
        g_autofree gchar *hour_str = NULL;
        g_autofree gchar *ampm_str = NULL;
        gboolean am;
        gint64 hour, minute;

        if (!g_regex_match (get_12h_regex (), text, 0, &match_info))
          {
            update_entry (self);
            break;
          }

        hour_str = g_match_info_fetch_named (match_info, "hour");
        minute_str = g_match_info_fetch_named (match_info, "minute");
        ampm_str = g_match_info_fetch_named (match_info, "ampm");

        hour = g_ascii_strtoll (hour_str, NULL, 10);
        minute = g_ascii_strtoll (minute_str, NULL, 10);
        am = (strcmp (g_ascii_strup (ampm_str, -1), _("AM")) == 0);

        /* Allow 12AM and 12PM */
        if ((hour > 11 || minute > 59) && !(hour == 12 && minute == 0))
          {
            update_entry (self);
            break;
          }

        if (hour == 12)
          {
            g_assert (minute == 0);
            hour = 0;
          }

        if (!am)
          hour += 12;

        now = g_date_time_new_now_local ();
        time = g_date_time_new_local (g_date_time_get_year (now),
                                      g_date_time_get_month (now),
                                      g_date_time_get_day_of_month (now),
                                      hour, minute, 0);

        gcal_time_chooser_row_set_time (self, time);

      }
      break;

    case GCAL_TIME_FORMAT_24H:
      {
        g_autoptr (GMatchInfo) match_info = NULL;
        g_autoptr (GDateTime) time = NULL;
        g_autofree gchar *minute_str = NULL;
        g_autofree gchar *hour_str = NULL;
        g_autofree gchar *ampm_str = NULL;
        gint64 hour, minute;

        if (!g_regex_match (get_24h_regex (), text, 0, &match_info))
          {
            update_entry (self);
            break;
          }

        hour_str = g_match_info_fetch_named (match_info, "hour");
        minute_str = g_match_info_fetch_named (match_info, "minute");

        hour = g_ascii_strtoll (hour_str, NULL, 10);
        minute = g_ascii_strtoll (minute_str, NULL, 10);

        if (hour > 23 || minute > 59)
          {
            update_entry (self);
            break;
          }

        time = g_date_time_new_local (g_date_time_get_year (now),
                                      g_date_time_get_month (now),
                                      g_date_time_get_day_of_month (now),
                                      hour, minute, 0);

        gcal_time_chooser_row_set_time (self, time);

      }
      break;

    default:
      g_assert_not_reached ();
    }
}


/*
 * Callbacks
 */

static void
on_contains_focus_changed_cb (GtkEventControllerFocus *focus_controller,
                              GParamSpec              *pspec,
                              GcalTimeChooserRow      *self)
{
  parse_time (self);
}

static void
on_time_selected_changed_cb (GcalTimeSelector   *selector,
                             GParamSpec         *pspec,
                             GcalTimeChooserRow *self)
{
  update_entry (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TIME]);
}

static void
on_time_popover_shown_cb (GtkPopover         *popover,
                          GcalTimeChooserRow *self)
{
  parse_time (self);
}


/*
 * GObject overrides
 */

static void
gcal_time_chooser_row_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  GcalTimeChooserRow *self = GCAL_TIME_CHOOSER_ROW (object);

  switch (prop_id)
    {
    case PROP_TIME_FORMAT:
      g_value_set_enum (value, self->time_format);
      break;

    case PROP_TIME:
      g_value_set_boxed (value, gcal_time_chooser_row_get_time (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_time_chooser_row_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  GcalTimeChooserRow *self = GCAL_TIME_CHOOSER_ROW (object);

  switch (prop_id)
    {
    case PROP_TIME_FORMAT:
      gcal_time_chooser_row_set_time_format (self, g_value_get_enum (value));
      break;

    case PROP_TIME:
      gcal_time_chooser_row_set_time (self, g_value_get_boxed (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_time_chooser_row_class_init (GcalTimeChooserRowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = gcal_time_chooser_row_get_property;
  object_class->set_property = gcal_time_chooser_row_set_property;

  properties[PROP_TIME_FORMAT] = g_param_spec_enum ("time-format", NULL, NULL,
                                                    GCAL_TYPE_TIME_FORMAT,
                                                    GCAL_TIME_FORMAT_24H,
                                                    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  properties[PROP_TIME] = g_param_spec_boxed ("time", NULL, NULL,
                                              G_TYPE_DATE_TIME,
                                              G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/event-editor/gcal-time-chooser-row.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalTimeChooserRow, time_selector);

  gtk_widget_class_bind_template_callback (widget_class, on_contains_focus_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_time_selected_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_time_popover_shown_cb);
}

static void
gcal_time_chooser_row_init (GcalTimeChooserRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->time_format = GCAL_TIME_FORMAT_24H;
}

GcalTimeFormat
gcal_time_chooser_row_get_time_format (GcalTimeChooserRow *self)
{
  g_return_val_if_fail (GCAL_IS_TIME_CHOOSER_ROW (self), 0);

  return self->time_format;
}

void
gcal_time_chooser_row_set_time_format (GcalTimeChooserRow *self,
                                       GcalTimeFormat      time_format)
{
  g_return_if_fail (GCAL_IS_TIME_CHOOSER_ROW (self));

  if (self->time_format == time_format)
    return;

  self->time_format = time_format;

  gcal_time_selector_set_time_format (self->time_selector, time_format);
  update_entry (self);
}

/**
 * gcal_time_chooser_row_set_time:
 * @selector: a #GcalTimeChooserRow
 * @date: a valid #GDateTime
 *
 * Set the value of the shown time.
 */
void
gcal_time_chooser_row_set_time (GcalTimeChooserRow *self,
                                GDateTime          *time)
{
  g_return_if_fail (GCAL_IS_TIME_CHOOSER_ROW (self));

  gcal_time_selector_set_time (self->time_selector, time);
  update_entry (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TIME]);
}

/**
 * gcal_time_chooser_row_get_time:
 * @selector: a #GcalTimeChooserRow
 *
 * Get the value of the time shown
 *
 * Returns: (transfer none): the time of the selector.
 */
GDateTime*
gcal_time_chooser_row_get_time (GcalTimeChooserRow *self)
{
  g_return_val_if_fail (GCAL_IS_TIME_CHOOSER_ROW (self), NULL);

  return gcal_time_selector_get_time (self->time_selector);
}
