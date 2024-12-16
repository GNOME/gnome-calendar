/* gcal-time-zone-dialog.h
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

#define GCAL_TYPE_TIME_ZONE_DIALOG (gcal_time_zone_dialog_get_type ())
G_DECLARE_FINAL_TYPE (GcalTimeZoneDialog, gcal_time_zone_dialog, GCAL, TIME_ZONE_DIALOG, AdwDialog);

GtkWidget*           gcal_time_zone_dialog_new                   (GDateTime          *date_time);

GTimeZone*           gcal_time_zone_dialog_get_time_zone         (GcalTimeZoneDialog *self);

G_END_DECLS
