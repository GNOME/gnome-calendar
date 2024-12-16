/* gcal-time-zone-dialog.c
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
#include "config.h"

#define G_LOG_DOMAIN "GcalTimeZoneDialog"

#include "gcal-date-time-utils.h"
#include "gcal-debug.h"
#include "gcal-time-zone-dialog.h"
#include "gcal-time-zone-dialog-row.h"

#include <glib/gi18n.h>

#define RESULT_COUNT_LIMIT 12

struct _GcalTimeZoneDialog
{
  AdwDialog parent;

  GtkStack          *stack;
  AdwStatusPage     *empty_search;
  GtkScrolledWindow *search_results;
  GtkSearchEntry    *location_entry;
  GtkListBox        *listbox;

  GListStore       *locations;
  GTimeZone        *time_zone;
  GDateTime        *date_time;
};

G_DEFINE_TYPE (GcalTimeZoneDialog, gcal_time_zone_dialog, ADW_TYPE_DIALOG)

enum
{
  TIME_ZONE_SELECTED,
  N_SIGNALS,
};

static guint signals [N_SIGNALS] = { 0, };


/*
 * Auxiliary methods
 */

static GtkWidget*
create_row (gpointer item,
            gpointer user_data)
{
  GcalTimeZoneDialog *self = (GcalTimeZoneDialog *) user_data;
  g_autoptr (GDateTime) date_time = NULL;
  g_autofree gchar *country_name = NULL;
  g_autofree gchar *utc_str = NULL;
  g_autofree gchar *label = NULL;
  GWeatherLocation *location;
  AdwActionRow *row;

  g_assert (GCAL_IS_TIME_ZONE_DIALOG (self));
  g_assert (GWEATHER_IS_LOCATION (item));

  location = GWEATHER_LOCATION (item);

  country_name = gweather_location_get_country_name (location);
  if (country_name)
    {
      /* Translators: "%1$s" is the city / region name, and
       * "%2$s" is the country name (already localized).
       */
      label = g_strdup_printf (_("%1$s, %2$s"),
                             gweather_location_get_name (location),
                             country_name);
    }
  else
    {
      label = g_strdup (gweather_location_get_name (location));
    }


  date_time = g_date_time_new (gweather_location_get_timezone (location),
                               g_date_time_get_year (self->date_time),
                               g_date_time_get_month (self->date_time),
                               g_date_time_get_day_of_month (self->date_time),
                               g_date_time_get_hour (self->date_time),
                               g_date_time_get_minute (self->date_time),
                               0);
  utc_str = gcal_date_time_format_utc_offset (date_time);

  row = ADW_ACTION_ROW (gcal_time_zone_dialog_row_new (location));
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), utc_str);
  adw_action_row_set_subtitle (row, label);

  return GTK_WIDGET (row);
}

static void
query_locations (GcalTimeZoneDialog *self,
                 GWeatherLocation   *location,
                 gchar              *search)
{
  gboolean contains_name;
  GWeatherLocation *loc;
  GTimeZone *timezone;
  const gchar *name, *country_name;

  timezone = gweather_location_get_timezone (location);

  if (timezone)
    {
      name = gweather_location_get_name (location);
      country_name = gweather_location_get_country_name (location);

      contains_name = name != NULL && g_strrstr (g_utf8_casefold (g_utf8_normalize (name, -1, G_NORMALIZE_ALL), -1), search) != NULL;
      contains_name = contains_name | (country_name != NULL && g_strrstr (g_utf8_casefold (g_utf8_normalize (country_name, -1, G_NORMALIZE_ALL), -1), search) != NULL);

      if (g_list_model_get_n_items (G_LIST_MODEL (self->locations)) >= RESULT_COUNT_LIMIT)
        return;

      switch (gweather_location_get_level (location))
        {
          case GWEATHER_LOCATION_CITY:
              if (contains_name)
                g_list_store_append (self->locations, location);
              return;

          case GWEATHER_LOCATION_NAMED_TIMEZONE:
              if (contains_name)
                g_list_store_append (self->locations, location);
              return;

          default:
              break;
      }
    }

  loc = gweather_location_next_child (location, NULL);
  while (loc)
    {
      query_locations (self, loc, search);
      if (g_list_model_get_n_items (G_LIST_MODEL (self->locations)) >= RESULT_COUNT_LIMIT)
          return;

      loc = gweather_location_next_child (location, loc);
    }
}


/*
 * Callbacks
 */

static void
on_search_changed (GtkSearchEntry     *entry,
                   GcalTimeZoneDialog *self)
{
  gchar *temp, *search;
  GWeatherLocation *world_location;

  g_list_store_remove_all (self->locations);

  if (g_strcmp0 (gtk_editable_get_text (GTK_EDITABLE (self->location_entry)), "") == 0)
    {
      gtk_stack_set_visible_child (self->stack, GTK_WIDGET (self->empty_search));
      return;
    }

  temp = g_utf8_normalize (gtk_editable_get_text (GTK_EDITABLE (self->location_entry)), -1, G_NORMALIZE_ALL);
  search = g_utf8_casefold (temp, -1);
  g_free(temp);

  world_location = gweather_location_get_world ();
  if (!world_location)
    return;

  query_locations (self, world_location, search);

  if (g_list_model_get_n_items (G_LIST_MODEL (self->locations)) == 0)
    {
      gtk_stack_set_visible_child (self->stack, GTK_WIDGET (self->empty_search));
      return;
    }

  gtk_stack_set_visible_child (self->stack, GTK_WIDGET (self->search_results));
}

static void
on_stop_search_cb (GtkSearchEntry     *entry,
                   GcalTimeZoneDialog *self)
{
  adw_dialog_close (ADW_DIALOG (self));
}

static void
on_row_activated_cb (GtkListBox *list_box,
                     GtkListBoxRow *list_box_row,
                     GcalTimeZoneDialog *self)
{
  GcalTimeZoneDialogRow *row;
  GWeatherLocation *location;
  GTimeZone *time_zone;

  GCAL_ENTRY;

  row = GCAL_TIME_ZONE_DIALOG_ROW (list_box_row);

  location = gcal_time_zone_dialog_row_get_location (row);
  time_zone = gweather_location_get_timezone (location);

  // The dialog only shows locations that have a timezone.
  g_assert (time_zone != NULL);

  self->time_zone = g_time_zone_new_identifier (g_time_zone_get_identifier (time_zone));

  g_signal_emit (self, signals[TIME_ZONE_SELECTED], 0);

  adw_dialog_close (ADW_DIALOG (self));

  GCAL_EXIT;
}


/*
 * Gobject overrides
 */

static void
gcal_time_zone_dialog_class_init (GcalTimeZoneDialogClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  signals[TIME_ZONE_SELECTED] = g_signal_new ("timezone-selected",
                                             GCAL_TYPE_TIME_ZONE_DIALOG,
                                             G_SIGNAL_RUN_LAST,
                                             0,
                                             NULL, NULL,
                                             NULL,
                                             G_TYPE_NONE,
                                             0);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/event-editor/gcal-time-zone-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalTimeZoneDialog, stack);
  gtk_widget_class_bind_template_child (widget_class, GcalTimeZoneDialog, empty_search);
  gtk_widget_class_bind_template_child (widget_class, GcalTimeZoneDialog, search_results);
  gtk_widget_class_bind_template_child (widget_class, GcalTimeZoneDialog, location_entry);
  gtk_widget_class_bind_template_child (widget_class, GcalTimeZoneDialog, listbox);

  gtk_widget_class_bind_template_callback (widget_class, on_search_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_stop_search_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_row_activated_cb);
}

static void
gcal_time_zone_dialog_init (GcalTimeZoneDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_search_entry_set_key_capture_widget (self->location_entry, GTK_WIDGET (self));

  self->locations = g_list_store_new (GWEATHER_TYPE_LOCATION);

  gtk_list_box_bind_model (self->listbox,
                           G_LIST_MODEL (self->locations),
                           create_row,
                           self,
                           NULL);
}

/**
 * gcal_time_zone_dialog_new:
 * @date_time: a #GDateTime
 *
 * Creates a new #GcalTimeZoneDialog
 *
 * Returns: (transfer full): a #GcalTimeZoneDialog
 */
GtkWidget*
gcal_time_zone_dialog_new (GDateTime *date_time)
{
  GcalTimeZoneDialog *dialog = g_object_new (GCAL_TYPE_TIME_ZONE_DIALOG, NULL);

  dialog->date_time = g_date_time_ref (date_time);

  return GTK_WIDGET (dialog);
}

/**
 * gcal_time_zone_dialog_get_time_zone:
 * @self: a #GcalTimeZoneDialog
 *
 * Get the value of the selected timezone
 *
 * Returns: (transfer none): the timezone selected in the dialog.
 */
GTimeZone*
gcal_time_zone_dialog_get_time_zone (GcalTimeZoneDialog *self)
{
  return self->time_zone;
}
