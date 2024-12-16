/* gcal-time-zone-dialog-row.h
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

#pragma once

#include <adwaita.h>

#include "libgweather/gweather.h"

G_BEGIN_DECLS

#define GCAL_TYPE_TIME_ZONE_DIALOG_ROW (gcal_time_zone_dialog_row_get_type ())
G_DECLARE_FINAL_TYPE (GcalTimeZoneDialogRow, gcal_time_zone_dialog_row, GCAL, TIME_ZONE_DIALOG_ROW, AdwActionRow);

GtkWidget*           gcal_time_zone_dialog_row_new               (GWeatherLocation   *location);

GWeatherLocation*    gcal_time_zone_dialog_row_get_location      (GcalTimeZoneDialogRow *self);

G_END_DECLS
