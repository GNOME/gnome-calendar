/* gcal-time-chooser-row.h
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

#include "gcal-enums.h"

#include <adwaita.h>

G_BEGIN_DECLS

#define GCAL_TYPE_TIME_CHOOSER_ROW (gcal_time_chooser_row_get_type ())

G_DECLARE_FINAL_TYPE (GcalTimeChooserRow, gcal_time_chooser_row, GCAL, TIME_CHOOSER_ROW, AdwEntryRow)

GcalTimeFormat   gcal_time_chooser_row_get_time_format (GcalTimeChooserRow *self);
void             gcal_time_chooser_row_set_time_format (GcalTimeChooserRow *self,
                                                        GcalTimeFormat      time_format);

GDateTime*       gcal_time_chooser_row_get_time        (GcalTimeChooserRow *self);
void             gcal_time_chooser_row_set_time        (GcalTimeChooserRow *self,
                                                        GDateTime          *date);

G_END_DECLS
