/* gcal-date-time-chooser.h
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

#include "gcal-enums.h"

G_BEGIN_DECLS

#define GCAL_TYPE_DATE_TIME_CHOOSER (gcal_date_time_chooser_get_type ())

G_DECLARE_FINAL_TYPE (GcalDateTimeChooser, gcal_date_time_chooser, GCAL, DATE_TIME_CHOOSER, AdwPreferencesGroup)

GtkWidget*           gcal_date_time_chooser_new                  (void);

void                 gcal_date_time_chooser_set_time_format      (GcalDateTimeChooser *chooser,
                                                                  GcalTimeFormat      time_format);

GDateTime*           gcal_date_time_chooser_get_date_time        (GcalDateTimeChooser *chooser);

void                 gcal_date_time_chooser_set_date_time        (GcalDateTimeChooser *chooser,
                                                                  GDateTime          *date_time);

GDateTime*           gcal_date_time_chooser_get_date             (GcalDateTimeChooser *chooser);

void                 gcal_date_time_chooser_set_date             (GcalDateTimeChooser *chooser,
                                                                  GDateTime          *date_time);

const gchar*         gcal_date_time_chooser_get_date_label       (GcalDateTimeChooser *chooser);

void                 gcal_date_time_chooser_set_date_label       (GcalDateTimeChooser *chooser,
                                                                  const gchar        *date_label);

const gchar*          gcal_date_time_chooser_get_time_label       (GcalDateTimeChooser *chooser);

void                 gcal_date_time_chooser_set_time_label       (GcalDateTimeChooser *chooser,
                                                                  const gchar        *time_label);

G_END_DECLS
