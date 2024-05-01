/* gcal-calendar-management-page.h
 *
 * Copyright 2019 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#include "gcal-calendar.h"

G_BEGIN_DECLS

#define GCAL_TYPE_CALENDAR_MANAGEMENT_PAGE (gcal_calendar_management_page_get_type ())

G_DECLARE_INTERFACE (GcalCalendarManagementPage, gcal_calendar_management_page, GCAL, CALENDAR_MANAGEMENT_PAGE, AdwNavigationPage)

struct _GcalCalendarManagementPageInterface
{
  GTypeInterface parent;

  void               (*activate)                                 (GcalCalendarManagementPage *self,
                                                                  GcalCalendar               *calendar);

  void               (*deactivate)                               (GcalCalendarManagementPage *self);

  void               (*switch_page)                              (GcalCalendarManagementPage *self,
                                                                  const gchar                *page_name,
                                                                  GcalCalendar               *calendar);
};

void                 gcal_calendar_management_page_activate      (GcalCalendarManagementPage *self,
                                                                  GcalCalendar               *calendar);

void                 gcal_calendar_management_page_deactivate    (GcalCalendarManagementPage *self);

void                 gcal_calendar_management_page_switch_page   (GcalCalendarManagementPage *self,
                                                                  const gchar                *page_name,
                                                                  GcalCalendar               *calendar);

G_END_DECLS
