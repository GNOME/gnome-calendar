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


/**
 * GcalWindowViewType:
 * @GCAL_WINDOW_VIEW_DAY: Day view (not implemented)
 * @GCAL_WINDOW_VIEW_WEEK: Week view
 * @GCAL_WINDOW_VIEW_MONTH: Month view
 * @GCAL_WINDOW_VIEW_YEAR: Year view
 * @GCAL_WINDOW_VIEW_LIST: List (not implemented)
 * @GCAL_WINDOW_VIEW_SEARCH: Search view (partially implemented)
 *
 * Enum with the available views.
 */
typedef enum
{
  GCAL_WINDOW_VIEW_DAY,
  GCAL_WINDOW_VIEW_WEEK,
  GCAL_WINDOW_VIEW_MONTH,
  GCAL_WINDOW_VIEW_YEAR,
  GCAL_WINDOW_VIEW_LIST,
  GCAL_WINDOW_VIEW_SEARCH,
} GcalWindowViewType;

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
