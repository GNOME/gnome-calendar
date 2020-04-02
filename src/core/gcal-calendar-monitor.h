/* gcal-calendar-monitor.h
 *
 * Copyright 2020 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#include <glib-object.h>

#include "gcal-types.h"

G_BEGIN_DECLS

#define GCAL_TYPE_CALENDAR_MONITOR (gcal_calendar_monitor_get_type())
G_DECLARE_FINAL_TYPE (GcalCalendarMonitor, gcal_calendar_monitor, GCAL, CALENDAR_MONITOR, GObject)

GcalCalendarMonitor* gcal_calendar_monitor_new                   (GcalCalendar        *calendar);

void                 gcal_calendar_monitor_set_range             (GcalCalendarMonitor *self,
                                                                  GDateTime           *range_start,
                                                                  GDateTime           *range_end);

GcalEvent*           gcal_calendar_monitor_get_cached_event      (GcalCalendarMonitor  *self,
                                                                  const gchar          *event_id);

void                 gcal_calendar_monitor_set_filter            (GcalCalendarMonitor *self,
                                                                  const gchar         *filter);

G_END_DECLS
