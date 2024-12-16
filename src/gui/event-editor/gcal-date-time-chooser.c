/* gcal-date-time-chooser.c
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

#define G_LOG_DOMAIN "GcalDateTimeChooser"

#include "gcal-date-chooser-row.h"
#include "gcal-date-time-chooser.h"
#include "gcal-date-time-utils.h"
#include "gcal-debug.h"
#include "gcal-time-zone-dialog.h"

#include <glib/gi18n.h>

struct _GcalDateTimeChooser
{
  AdwPreferencesGroup parent;

  GcalDateChooserRow *date_row;
  AdwEntryRow        *time_row;
  GtkPopover         *time_popover;
  AdwButtonContent   *time_zone_button_content;
  AdwToggleGroup     *period_toggle_group;

  GtkAdjustment *hour_adjustment;
  GtkAdjustment *minute_adjustment;

  GDateTime      *date_time;
  GcalTimeFormat  time_format;
};

G_DEFINE_TYPE (GcalDateTimeChooser, gcal_date_time_chooser, ADW_TYPE_PREFERENCES_GROUP);

enum
{
  PROP_0,
  PROP_DATE_TIME,
  PROP_DATE_LABEL,
  PROP_TIME_LABEL,
  N_PROPS
};

static GParamSpec *properties[N_PROPS] = {
  NULL,
};

static void on_spin_buttons_value_changed_cb (GcalDateTimeChooser *self);

static void on_period_toggle_group_active_changed_cb (GtkWidget           *widget,
                                                      GParamSpec          *pspec,
                                                      GcalDateTimeChooser *self);

/*
 * Auxiliary methods
 */

/*
 * Use the stored date_time of the GcalDateTimeChooser to format the content of the time entry.
 */
static void
normalize_time_entry (GcalDateTimeChooser *self)
{
  g_autoptr (GTimeZone) local_time_zone = g_time_zone_new_local ();
  g_autoptr (GString) label = NULL;
  const gchar *date_time_identifier;
  const gchar *local_identifier;
  gchar *date_time_str;

  GCAL_ENTRY;

  switch (self->time_format)
    {
    case GCAL_TIME_FORMAT_12H:
      date_time_str = g_date_time_format (self->date_time, "%I:%M %p");
      break;

    case GCAL_TIME_FORMAT_24H:
      date_time_str = g_date_time_format (self->date_time, "%R");
      break;

    default:
      g_assert_not_reached ();
    }

  label = g_string_new_take (date_time_str);

  date_time_identifier = g_time_zone_get_identifier (g_date_time_get_timezone (self->date_time));
  local_identifier = g_time_zone_get_identifier (local_time_zone);

  if (g_strcmp0 (date_time_identifier, local_identifier) != 0)
    {
      g_autofree gchar *time_zone_str = gcal_date_time_format_utc_offset (self->date_time);

      g_string_append (label, " (");
      g_string_append (label, time_zone_str);
      g_string_append (label, ")");
    }

  gtk_editable_set_text (GTK_EDITABLE (self->time_row), label->str);

  GCAL_EXIT;
}

/*
 * Use the timezone of the stored date_time of the GcalDateTimeChooser to format the label of the timezone button.
 */
static void
normalize_timezone_button_label (GcalDateTimeChooser *self)
{
  g_autofree gchar *formatted = NULL;
  g_autofree gchar *utc_str = NULL;
  const gchar *identifier;

  identifier = g_time_zone_get_identifier (g_date_time_get_timezone (self->date_time));

  if (g_strcmp0 (identifier, "UTC") == 0)
    {
      adw_button_content_set_label (self->time_zone_button_content, "UTC");
      return;
    }

  utc_str = gcal_date_time_format_utc_offset (self->date_time);

  /* Translators: "%1$s" is the timezone identifier (e.g. UTC, America/Sao_Paulo)
   * and "%2$s" is the timezone offset (e.g. UTC-3).
   */
  formatted = g_strdup_printf (_("%1$s (%2$s)"), identifier, utc_str);

  adw_button_content_set_label (self->time_zone_button_content, formatted);
}

/*
 * Use the time of the stored date_time of the GcalDateTimeChooser to format the spinners and the am/pm toggle of the
 * time popover.
 */
static void
normalize_time_popover (GcalDateTimeChooser *self)
{
  gint new_hour, new_minute;

  GCAL_ENTRY;

  new_hour = g_date_time_get_hour (self->date_time);
  new_minute = g_date_time_get_minute (self->date_time);

  g_signal_handlers_block_by_func (self->hour_adjustment, on_spin_buttons_value_changed_cb, self);
  g_signal_handlers_block_by_func (self->minute_adjustment, on_spin_buttons_value_changed_cb, self);
  g_signal_handlers_block_by_func (self->period_toggle_group, on_period_toggle_group_active_changed_cb, self);

  adw_toggle_group_set_active_name (self->period_toggle_group, new_hour < 12 ? "am" : "pm");

  switch (self->time_format)
    {
    case GCAL_TIME_FORMAT_12H:
      if (new_hour == 0)
        gtk_adjustment_set_value (self->hour_adjustment, 12);
      else if (new_hour <= 12)
        gtk_adjustment_set_value (self->hour_adjustment, new_hour);
      else
        gtk_adjustment_set_value (self->hour_adjustment, new_hour - 12);
      break;

    case GCAL_TIME_FORMAT_24H:
      gtk_adjustment_set_value (self->hour_adjustment, new_hour);
      break;

    default:
      g_assert_not_reached ();
    }

  gtk_adjustment_set_value (self->minute_adjustment, new_minute);

  g_signal_handlers_unblock_by_func (self->hour_adjustment, on_spin_buttons_value_changed_cb, self);
  g_signal_handlers_unblock_by_func (self->minute_adjustment, on_spin_buttons_value_changed_cb, self);
  g_signal_handlers_unblock_by_func (self->period_toggle_group, on_period_toggle_group_active_changed_cb, self);

  GCAL_EXIT;
}

static GRegex *
get_12h_regex (void)
{
  static GRegex *regex = NULL;

  if (g_once_init_enter_pointer (&regex))
    {
      g_autoptr (GError) error = NULL;
      g_autofree gchar *str = NULL;

      str = g_strdup_printf ("^(?<hour>[0-9]{1,2})([\\-:./_,'a-z](?<minute>[0-9]{1,2}))? ?(?<ampm>%s|%s)",
                             _ ("AM"), _ ("PM"));

      regex = g_regex_new (str, G_REGEX_CASELESS, 0, &error);

      if (error)
        g_error ("Failed to compile regex: %s. Aborting...", error->message);
    }

  return regex;
}

static GRegex *
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

/*
 * Parse the string of the time entry.
 * If the string is not recognised as a time, it resets the entry to the formatted form of the last time.
 * If it is recognised, the stored date_time is updated and the entry is formatted accordingly.
 */
static void
validate_and_normalize_time_entry (GcalDateTimeChooser *self)
{
  const gchar *text;

  text = gtk_editable_get_text (GTK_EDITABLE (self->time_row));

  switch (self->time_format)
    {
    case GCAL_TIME_FORMAT_12H:
      {
        g_autoptr (GMatchInfo) match_info = NULL;
        g_autoptr (GDateTime) date_time = NULL;
        g_autofree gchar *minute_str = NULL;
        g_autofree gchar *hour_str = NULL;
        g_autofree gchar *ampm_str = NULL;
        gboolean am;
        gint64 hour, minute;

        if (!g_regex_match (get_12h_regex (), text, 0, &match_info))
          {
            normalize_time_entry (self);
            break;
          }

        hour_str = g_match_info_fetch_named (match_info, "hour");
        minute_str = g_match_info_fetch_named (match_info, "minute");
        ampm_str = g_match_info_fetch_named (match_info, "ampm");

        hour = g_ascii_strtoll (hour_str, NULL, 10);
        minute = g_ascii_strtoll (minute_str, NULL, 10);
        am = (strcmp (g_ascii_strup (ampm_str, -1), _ ("AM")) == 0);

        if (hour == 0 || hour > 12 || minute > 59)
          normalize_time_entry (self);

        if (hour == 12)
          hour = 0;

        if (!am)
          hour += 12;

        date_time = g_date_time_new (g_date_time_get_timezone (self->date_time),
                                     g_date_time_get_year (self->date_time),
                                     g_date_time_get_month (self->date_time),
                                     g_date_time_get_day_of_month (self->date_time),
                                     hour, minute, 0);

        gcal_date_time_chooser_set_date_time (self, date_time);
      }
      break;

    case GCAL_TIME_FORMAT_24H:
      {
        g_autoptr (GMatchInfo) match_info = NULL;
        g_autoptr (GDateTime) date_time = NULL;
        g_autofree gchar *minute_str = NULL;
        g_autofree gchar *hour_str = NULL;
        g_autofree gchar *ampm_str = NULL;
        gint64 hour, minute;

        if (!g_regex_match (get_24h_regex (), text, 0, &match_info))
          {
            normalize_time_entry (self);
            break;
          }

        hour_str = g_match_info_fetch_named (match_info, "hour");
        minute_str = g_match_info_fetch_named (match_info, "minute");

        hour = g_ascii_strtoll (hour_str, NULL, 10);
        minute = g_ascii_strtoll (minute_str, NULL, 10);

        if (hour > 23 || minute > 59)
          {
            normalize_time_entry (self);
            break;
          }

        date_time = g_date_time_new (g_date_time_get_timezone (self->date_time),
                                     g_date_time_get_year (self->date_time),
                                     g_date_time_get_month (self->date_time),
                                     g_date_time_get_day_of_month (self->date_time),
                                     hour, minute, 0);

        gcal_date_time_chooser_set_date_time (self, date_time);
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
on_date_changed_cb (GcalDateTimeChooser *self,
                    GParamSpec          *pspec,
                    GtkWidget           *widget)
{
  g_autoptr (GDateTime) new_date_time = NULL;
  GDateTime *date_chooser_row_date;

  GCAL_ENTRY;

  date_chooser_row_date = gcal_date_chooser_row_get_date (self->date_row);
  new_date_time = g_date_time_new (g_date_time_get_timezone (self->date_time),
                                   g_date_time_get_year (date_chooser_row_date),
                                   g_date_time_get_month (date_chooser_row_date),
                                   g_date_time_get_day_of_month (date_chooser_row_date),
                                   g_date_time_get_hour (self->date_time),
                                   g_date_time_get_minute (self->date_time),
                                   0);

  gcal_date_time_chooser_set_date_time (self, new_date_time);

  GCAL_EXIT;
}

static void
on_time_zone_selected_cb (GcalTimeZoneDialog  *dialog,
                          GcalDateTimeChooser *self)
{
  GTimeZone *time_zone;
  GDateTime *new;

  GCAL_ENTRY;

  time_zone = gcal_time_zone_dialog_get_time_zone (dialog);

  new = g_date_time_new (time_zone,
                         g_date_time_get_year (self->date_time),
                         g_date_time_get_month (self->date_time),
                         g_date_time_get_day_of_month (self->date_time),
                         g_date_time_get_hour (self->date_time),
                         g_date_time_get_minute (self->date_time),
                         0);

  gcal_date_time_chooser_set_date_time (self, new);

  GCAL_EXIT;
}

static void
on_time_zone_button_clicked_cb (GtkButton           *button,
                                GcalDateTimeChooser *self)
{
  AdwDialog *dialog;

  validate_and_normalize_time_entry (self);

  dialog = ADW_DIALOG (gcal_time_zone_dialog_new (self->date_time));
  g_signal_connect (dialog, "timezone-selected", G_CALLBACK (on_time_zone_selected_cb), self);
  gtk_popover_popdown (self->time_popover);
  adw_dialog_present (dialog, GTK_WIDGET (self));
}

static void
on_time_row_contains_focus_changed_cb (GtkEventControllerFocus *focus_controller,
                                       GParamSpec              *pspec,
                                       GcalDateTimeChooser     *self)
{
  validate_and_normalize_time_entry (self);
}

static void
on_time_popover_shown_cb (GtkPopover          *popover,
                          GcalDateTimeChooser *self)
{
  validate_and_normalize_time_entry (self);
}

static void
on_spin_button_output_cb (GtkWidget           *widget,
                          GcalDateTimeChooser *self)
{
  g_autofree gchar *text = NULL;
  GtkAdjustment *adjustment;
  gint hour;

  adjustment = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (widget));
  hour = gtk_adjustment_get_value (adjustment);

  text = g_strdup_printf ("%02d", hour);
  gtk_editable_set_text (GTK_EDITABLE (widget), text);
}

static void
on_spin_buttons_value_changed_cb (GcalDateTimeChooser *self)
{
  g_autoptr (GDateTime) new_date_time = NULL;
  gint hour, minute;

  GCAL_ENTRY;

  hour = gtk_adjustment_get_value (self->hour_adjustment);
  minute = gtk_adjustment_get_value (self->minute_adjustment);

  switch (self->time_format)
    {
    case GCAL_TIME_FORMAT_12H:
      if (hour == 12)
        hour = 0;
      if (g_strcmp0 (adw_toggle_group_get_active_name (self->period_toggle_group), "pm") == 0)
        hour += 12;
      break;

    case GCAL_TIME_FORMAT_24H:
      break;

    default:
      g_assert_not_reached ();
    }

  new_date_time = g_date_time_new (g_date_time_get_timezone (self->date_time),
                                   g_date_time_get_year (self->date_time),
                                   g_date_time_get_month (self->date_time),
                                   g_date_time_get_day_of_month (self->date_time),
                                   hour, minute, 0);

  gcal_date_time_chooser_set_date_time (self, new_date_time);

  GCAL_EXIT;
}

static void
on_hour_spin_button_wrapped_cb (GtkSpinButton       *spin_button,
                                GcalDateTimeChooser *self)
{
  if (g_strcmp0 (adw_toggle_group_get_active_name (self->period_toggle_group), "am") == 0)
    adw_toggle_group_set_active_name (self->period_toggle_group, "pm");
  else
    adw_toggle_group_set_active_name (self->period_toggle_group, "am");
}

static void
on_period_toggle_group_active_changed_cb (GtkWidget           *widget,
                                          GParamSpec          *pspec,
                                          GcalDateTimeChooser *self)
{
  g_autoptr (GDateTime) new_date_time = NULL;

  if (g_strcmp0 (adw_toggle_group_get_active_name (self->period_toggle_group), "am") == 0)
    new_date_time = g_date_time_add_hours (self->date_time, -12);
  else
    new_date_time = g_date_time_add_hours (self->date_time, 12);

  gcal_date_time_chooser_set_date_time (self, new_date_time);
}

/*
 * GObject overrides
 */

static void
gcal_date_time_chooser_dispose (GObject *object)
{
  GcalDateTimeChooser *self = GCAL_DATE_TIME_CHOOSER (object);

  gcal_clear_date_time (&self->date_time);

  G_OBJECT_CLASS (gcal_date_time_chooser_parent_class)->dispose (object);
}

static void
gcal_date_time_chooser_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GcalDateTimeChooser *self = GCAL_DATE_TIME_CHOOSER (object);

  switch (prop_id)
    {
    case PROP_DATE_TIME:
      g_value_set_boxed (value, gcal_date_time_chooser_get_date_time (self));
      break;

    case PROP_DATE_LABEL:
      g_value_set_string (value, gcal_date_time_chooser_get_date_label (self));
      break;

    case PROP_TIME_LABEL:
      g_value_set_string (value, gcal_date_time_chooser_get_time_label (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_date_time_chooser_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GcalDateTimeChooser *self = GCAL_DATE_TIME_CHOOSER (object);

  switch (prop_id)
    {
    case PROP_DATE_TIME:
      gcal_date_time_chooser_set_date_time (self, g_value_get_boxed (value));
      break;

    case PROP_DATE_LABEL:
      gcal_date_time_chooser_set_date_label (self, g_value_get_string (value));
      break;

    case PROP_TIME_LABEL:
      gcal_date_time_chooser_set_time_label (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_date_time_chooser_class_init (GcalDateTimeChooserClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_ensure (GCAL_TYPE_DATE_CHOOSER_ROW);

  object_class->dispose = gcal_date_time_chooser_dispose;
  object_class->get_property = gcal_date_time_chooser_get_property;
  object_class->set_property = gcal_date_time_chooser_set_property;

  /**
   * GcalDateTimeChooser::date-time:
   *
   * The current date-time of the chooser.
   */
  properties[PROP_DATE_TIME] = g_param_spec_boxed ("date-time",
                                                   "DateTime of the date-time chooser",
                                                   "The current date-time of the chooser",
                                                   G_TYPE_DATE_TIME,
                                                   G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GcalDateTimeChooser::date-label:
   *
   * The current date label of the date row.
   */
  properties[PROP_DATE_LABEL] = g_param_spec_string ("date-label",
                                                     NULL,
                                                     NULL,
                                                     "",
                                                     G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GcalDateTimeChooser::time-label:
   *
   * The current time label of the time row.
   */
  properties[PROP_TIME_LABEL] = g_param_spec_string ("time-label",
                                                     NULL,
                                                     NULL,
                                                     "",
                                                     G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/event-editor/gcal-date-time-chooser.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalDateTimeChooser, date_row);
  gtk_widget_class_bind_template_child (widget_class, GcalDateTimeChooser, time_row);
  gtk_widget_class_bind_template_child (widget_class, GcalDateTimeChooser, time_popover);
  gtk_widget_class_bind_template_child (widget_class, GcalDateTimeChooser, time_zone_button_content);
  gtk_widget_class_bind_template_child (widget_class, GcalDateTimeChooser, period_toggle_group);
  gtk_widget_class_bind_template_child (widget_class, GcalDateTimeChooser, hour_adjustment);
  gtk_widget_class_bind_template_child (widget_class, GcalDateTimeChooser, minute_adjustment);

  gtk_widget_class_bind_template_callback (widget_class, on_date_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_time_zone_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_time_row_contains_focus_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_time_popover_shown_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_spin_button_output_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_spin_buttons_value_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_hour_spin_button_wrapped_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_period_toggle_group_active_changed_cb);
}

static void
gcal_date_time_chooser_init (GcalDateTimeChooser *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->date_time = g_date_time_new_now_local ();
}

/* Public API */
GtkWidget *
gcal_date_time_chooser_new (void)
{
  return g_object_new (GCAL_TYPE_DATE_TIME_CHOOSER, NULL);
}

void
gcal_date_time_chooser_set_time_format (GcalDateTimeChooser *self,
                                        GcalTimeFormat       time_format)
{
  g_return_if_fail (GCAL_IS_DATE_TIME_CHOOSER (self));

  self->time_format = time_format;

  gtk_widget_set_visible (GTK_WIDGET (self->period_toggle_group), time_format == GCAL_TIME_FORMAT_12H);

  switch (self->time_format)
    {
    case GCAL_TIME_FORMAT_12H:
      gtk_adjustment_set_lower (self->hour_adjustment, 1);
      gtk_adjustment_set_upper (self->hour_adjustment, 12);
      break;

    case GCAL_TIME_FORMAT_24H:
      gtk_adjustment_set_lower (self->hour_adjustment, 0);
      gtk_adjustment_set_upper (self->hour_adjustment, 23);
      break;

    default:
      g_assert_not_reached ();
    }

  normalize_time_entry (self);
}

/**
 * gcal_date_time_chooser_get_date_time:
 * @self: a #GcalDateTimeChooser
 *
 * Get the value of the date-time shown
 *
 * Returns: (transfer none): the date-time of the chooser.
 */
GDateTime *
gcal_date_time_chooser_get_date_time (GcalDateTimeChooser *self)
{
  g_assert (GCAL_IS_DATE_TIME_CHOOSER (self));

  return g_date_time_ref (self->date_time);
}

/**
 * gcal_date_time_chooser_set_date_time:
 * @self: a #GcalDateTimeChooser
 * @date_time: a valid #GDateTime
 *
 * Set the value of the shown date-time.
 */
void
gcal_date_time_chooser_set_date_time (GcalDateTimeChooser *self,
                                      GDateTime           *date_time)
{
  GCAL_ENTRY;

  gcal_set_date_time (&self->date_time, date_time);

  g_signal_handlers_block_by_func (self->date_row, on_date_changed_cb, self);
  gcal_date_chooser_row_set_date (self->date_row, date_time);
  g_signal_handlers_unblock_by_func (self->date_row, on_date_changed_cb, self);

  normalize_time_entry (self);
  normalize_timezone_button_label (self);
  normalize_time_popover (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DATE_TIME]);

  GCAL_EXIT;
}

/**
 * gcal_date_time_chooser_get_date:
 * @self: a #GcalDateTimeChooser
 *
 * Get the value of the date shown.
 * The timezone, hours, minutes and seconds are discarded and replaced with UTC, 0, 0 and 0 respectively.
 *
 * Returns: (transfer full): the date of the chooser.
 */
GDateTime *
gcal_date_time_chooser_get_date (GcalDateTimeChooser *self)
{
  g_assert (GCAL_IS_DATE_TIME_CHOOSER (self));

  return g_date_time_new_utc (g_date_time_get_year (self->date_time),
                              g_date_time_get_month (self->date_time),
                              g_date_time_get_day_of_month (self->date_time),
                              0, 0, 0);
}

/**
 * gcal_date_time_chooser_set_date:
 * @self: a #GcalDateTimeChooser
 * @date_time: a valid #GDateTime
 *
 * Set the value of the shown date.
 * The timezone, hours, minutes and seconds are ignored.
 */
void
gcal_date_time_chooser_set_date (GcalDateTimeChooser *self,
                                 GDateTime           *date_time)
{
  g_autoptr (GDateTime) new_date_time = NULL;

  g_assert (GCAL_IS_DATE_TIME_CHOOSER (self));

  GCAL_ENTRY;

  new_date_time = g_date_time_new (g_date_time_get_timezone (self->date_time),
                                   g_date_time_get_year (date_time),
                                   g_date_time_get_month (date_time),
                                   g_date_time_get_day_of_month (date_time),
                                   g_date_time_get_hour (self->date_time),
                                   g_date_time_get_minute (self->date_time),
                                   0);

  gcal_date_time_chooser_set_date_time (self, new_date_time);

  GCAL_EXIT;
}

/**
 * gcal_date_time_chooser_get_date_label:
 * @self: a #GcalDateTimeChooser
 *
 * Get the value of the date row label
 *
 * Returns: (transfer none): the label of the date row.
 */
const gchar*
gcal_date_time_chooser_get_date_label (GcalDateTimeChooser *self)
{
  g_assert (GCAL_IS_DATE_TIME_CHOOSER (self));

  return adw_preferences_row_get_title (ADW_PREFERENCES_ROW (self->date_row));
}

/**
 * gcal_date_time_chooser_set_date_label:
 * @self: a #GcalDateTimeChooser
 * @date_label: The label
 *
 * Set the value of the date row label
 */
void
gcal_date_time_chooser_set_date_label (GcalDateTimeChooser *self,
                                       const gchar         *date_label)
{
  g_assert (GCAL_IS_DATE_TIME_CHOOSER (self));

  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->date_row), date_label);
}

/**
 * gcal_date_time_chooser_get_time_label:
 * @self: a #GcalDateTimeChooser
 *
 * Get the value of the time row label
 *
 * Returns: (transfer none): the label of the time row.
 */
const gchar*
gcal_date_time_chooser_get_time_label (GcalDateTimeChooser *self)
{
  g_assert (GCAL_IS_DATE_TIME_CHOOSER (self));

  return adw_preferences_row_get_title (ADW_PREFERENCES_ROW (self->time_row));
}

/**
 * gcal_date_time_chooser_set_time_label:
 * @self: a #GcalDateTimeChooser
 * @time_label: The label
 *
 * Set the value of the time row label
 */
void
gcal_date_time_chooser_set_time_label (GcalDateTimeChooser *self,
                                       const gchar         *time_label)
{
  g_assert (GCAL_IS_DATE_TIME_CHOOSER (self));

  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->time_row), time_label);
}
