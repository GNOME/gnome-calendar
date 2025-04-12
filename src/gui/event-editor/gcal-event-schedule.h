/* gcal-event-schedule.h - immutable struct for the date and recurrence values of an event
 *
 * Copyright 2025 Federico Mena Quintero <federico@gnome.org>
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

#include "gcal-enums.h"
#include "gcal-event.h"
#include "gcal-recurrence.h"

G_BEGIN_DECLS

/* Values from a GcalEvent that this widget can manipulate.
 *
 * We keep an immutable copy of the original event's values, and later generate
 * a new GcalScheduleValues with the updated data from the widgetry.
 */
typedef struct
{
  gboolean all_day;

  GDateTime *date_start;
  GDateTime *date_end;

  GcalRecurrence *recur;
} GcalScheduleValues;

typedef struct
{
  /* original values from event */
  GcalScheduleValues orig;

  /* current values, as modified by actions from widgets */
  GcalScheduleValues curr;

  /* copied from GcalContext to avoid a dependency on it */
  GcalTimeFormat time_format;
} GcalEventSchedule;

GcalScheduleValues  gcal_schedule_values_copy                   (const GcalScheduleValues *values);


GcalEventSchedule  *gcal_event_schedule_from_event              (GcalEvent               *event,
                                                                 GcalTimeFormat           time_format);
void                gcal_event_schedule_free                    (GcalEventSchedule       *values);

GcalEventSchedule  *gcal_event_schedule_set_all_day             (const GcalEventSchedule *values,
                                                                 gboolean                 all_day);
GcalEventSchedule  *gcal_event_schedule_set_time_format         (const GcalEventSchedule *values,
                                                                 GcalTimeFormat           time_format);
GcalEventSchedule  *gcal_event_schedule_set_start_date          (const GcalEventSchedule *values,
                                                                 GDateTime               *start);
GcalEventSchedule  *gcal_event_schedule_set_end_date            (const GcalEventSchedule *values,
                                                                 GDateTime               *end);
GcalEventSchedule  *gcal_event_schedule_set_start_date_time     (const GcalEventSchedule *values,
                                                                 GDateTime               *start);
GcalEventSchedule  *gcal_event_schedule_set_end_date_time       (const GcalEventSchedule *values,
                                                                 GDateTime               *end);
GcalEventSchedule  *gcal_event_schedule_set_recur_frequency     (const GcalEventSchedule *values,
                                                                 GcalRecurrenceFrequency  frequency);
GcalEventSchedule  *gcal_event_schedule_set_recur_limit_type    (const GcalEventSchedule *values,
                                                                 GcalRecurrenceLimitType  limit_type);
GcalEventSchedule  *gcal_event_schedule_set_recurrence_count    (const GcalEventSchedule *values,
                                                                 guint                    count);
GcalEventSchedule  *gcal_event_schedule_set_recurrence_until    (const GcalEventSchedule *values,
                                                                 GDateTime               *until);

GcalEventSchedule *gcal_event_schedule_with_date_times          (const char              *start,
                                                                 const char              *end,
                                                                 gboolean                 all_day);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GcalEventSchedule, gcal_event_schedule_free);

/* Tests */

void gcal_event_schedule_add_tests (void);

G_END_DECLS
