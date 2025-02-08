/* gcal-calendar-navigation-button.h
 *
 * Copyright 2025 Markus Göllnitz <camelcasenick@bewares.it>
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

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define GCAL_TYPE_CALENDAR_NAVIGATION_BUTTON (gcal_calendar_navigation_button_get_type ())
G_DECLARE_FINAL_TYPE (GcalCalendarNavigationButton, gcal_calendar_navigation_button, GCAL, CALENDAR_NAVIGATION_BUTTON, AdwBin)

G_END_DECLS
