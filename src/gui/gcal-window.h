/* gcal-window.h
 *
 * Copyright (C) 2015 Erick Pérez Castellanos <erickpc@gnome.org>
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

#include "gcal-application.h"
#include "gcal-enums.h"
#include "gcal-utils.h"

G_BEGIN_DECLS


#define GCAL_TYPE_WINDOW                    (gcal_window_get_type ())

G_DECLARE_FINAL_TYPE (GcalWindow, gcal_window, GCAL, WINDOW, AdwApplicationWindow)

GtkWidget*           gcal_window_new_with_date                  (GcalApplication     *app,
                                                                 GDateTime           *date);

void                 gcal_window_set_search_query               (GcalWindow          *self,
                                                                const gchar          *query);

void                 gcal_window_open_event_by_uuid             (GcalWindow          *self,
                                                                 const gchar         *uuid);

void                 gcal_window_import_files                   (GcalWindow          *self,
                                                                 GFile             **files,
                                                                 gint                n_files);

G_END_DECLS
