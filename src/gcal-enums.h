/* gcal-enums.h
 *
 * Copyright © 2018 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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
 */

#pragma once

#include "gcal-enum-types.h"

/**
 * GcalTimeFormat:
 * @GCAL_TIME_FORMAT_12H: time is displayed in 12h format
 * @GCAL_TIME_FORMAT_24H: time is displayed in 24h format
 */
typedef enum
{
  GCAL_TIME_FORMAT_12H,
  GCAL_TIME_FORMAT_24H,
} GcalTimeFormat;

/**
 * GcalWindowViewType:
 * @GCAL_WINDOW_VIEW_WEEK: Week view
 * @GCAL_WINDOW_VIEW_MONTH: Month view
 * GCAL_WINDOW_VIEW_AGENDA: Agenda/list view
 *
 * Enum with the available views.
 */
typedef enum
{
  GCAL_WINDOW_VIEW_MONTH,
  GCAL_WINDOW_VIEW_WEEK,
  GCAL_WINDOW_VIEW_AGENDA,

  GCAL_WINDOW_VIEW_N_VIEWS,
} GcalWindowView;

typedef enum
{
  GCAL_WEEK_DAY_INVALID   = 0,
  GCAL_WEEK_DAY_SUNDAY    = 1 << 0,
  GCAL_WEEK_DAY_MONDAY    = 1 << 1,
  GCAL_WEEK_DAY_TUESDAY   = 1 << 2,
  GCAL_WEEK_DAY_WEDNESDAY = 1 << 3,
  GCAL_WEEK_DAY_THURSDAY  = 1 << 4,
  GCAL_WEEK_DAY_FRIDAY    = 1 << 5,
  GCAL_WEEK_DAY_SATURDAY  = 1 << 6
} GcalWeekDay;

typedef enum
{
  GCAL_TIMESTAMP_POLICY_NONE,
  GCAL_TIMESTAMP_POLICY_START,
  GCAL_TIMESTAMP_POLICY_END,
} GcalTimestampPolicy;
