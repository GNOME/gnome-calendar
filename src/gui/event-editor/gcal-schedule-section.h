/* gcal-schedule-section.h
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

#include <adwaita.h>
#include <gtk/gtk.h>

#include "gcal-enums.h"
#include "gcal-event.h"
#include "gcal-recurrence.h"

G_BEGIN_DECLS

#define GCAL_TYPE_SCHEDULE_SECTION (gcal_schedule_section_get_type())
G_DECLARE_FINAL_TYPE (GcalScheduleSection, gcal_schedule_section, GCAL, SCHEDULE_SECTION, GtkBox)

/* Values from a GcalEvent that this widget can manipulate.
 *
 * We keep an immutable copy of the original event's values, and later generate
 * a new GcalScheduleValues with the updated data from the widgetry.
 */
typedef struct
{
  gboolean all_day;

  /* Original times from the GcalEvent.  We keep these around to be able to reconstruct
   * the event's duration in case the all-day toggle gets turned on and off repeatedly.
   */
  GDateTime *orig_date_start;
  GDateTime *orig_date_end;

  GDateTime *date_start;
  GDateTime *date_end;

  GcalRecurrence *recur;
  GcalTimeFormat time_format;
} GcalScheduleValues;

gboolean             gcal_schedule_section_recurrence_changed    (GcalScheduleSection *self);

gboolean             gcal_schedule_section_day_changed           (GcalScheduleSection *self);

GcalScheduleValues  *gcal_schedule_values_from_event             (GcalEvent          *event,
                                                                  GcalTimeFormat      time_format);
void                 gcal_schedule_values_free                   (GcalScheduleValues *values);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GcalScheduleValues, gcal_schedule_values_free);

/* Tests */

void gcal_schedule_section_add_tests (void);

G_END_DECLS
