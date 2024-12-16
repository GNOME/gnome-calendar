/* gcal-time-zone-dialog-row.c
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

#define G_LOG_DOMAIN "GcalTimeZoneDialogRow"

#include "gcal-time-zone-dialog-row.h"

struct _GcalTimeZoneDialogRow
{
  AdwActionRow parent;

  GWeatherLocation *location;
};

G_DEFINE_TYPE (GcalTimeZoneDialogRow, gcal_time_zone_dialog_row, ADW_TYPE_ACTION_ROW)


/*
 * Auxiliary methods
 */


/*
 * Callbacks
 */


/*
 * Gobject overrides
 */

static void
gcal_time_zone_dialog_row_class_init (GcalTimeZoneDialogRowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/event-editor/gcal-time-zone-dialog-row.ui");
}

static void
gcal_time_zone_dialog_row_init (GcalTimeZoneDialogRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

/**
 * gcal_time_zone_dialog_row_new:
 *
 * Creates a new #GcalTimeZoneDialogRow
 *
 * Returns: (transfer full): a #GcalTimeZoneDialogRow
 */
GtkWidget*
gcal_time_zone_dialog_row_new (GWeatherLocation *location)
{
  GtkWidget *widget = g_object_new (GCAL_TYPE_TIME_ZONE_DIALOG_ROW, NULL);

  (GCAL_TIME_ZONE_DIALOG_ROW (widget))->location = g_object_ref (location);

  return widget;
}

/**
 * gcal_time_zone_dialog_row_get_location:
 * @self: a #GcalTimeZoneDialogRow
 *
 * Returns: (transfer none): a #GWeatherLocation
 */
GWeatherLocation*
gcal_time_zone_dialog_row_get_location (GcalTimeZoneDialogRow *self)
{
  return self->location;
}
