/* gcal-date-time-utils.c
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

#include "gcal-date-time-utils.h"
#include "gcal-utils.h"

/**
 * gcal_set_datetime:
 * @dest: location to a #GDateTime pointer
 * @src: (nullable): a #GDateTime
 *
 * Performs the same of g_set_object(), but with #GDateTime
 * instances.
 *
 * Returns: %TRUE if the value of @dest changed, %FALSE otherwise
 */
gboolean
gcal_set_date_time (GDateTime **dest,
                    GDateTime  *src)
{
  gboolean changed = *dest != src;

  gcal_clear_datetime (dest);

  if (src)
    *dest = g_date_time_ref (src);

  return changed;
}

/**
 * gcal_date_time_get_days_in_month:
 * @date: a #GDateTime
 *
 * Retrieves the number of days in the month and year
 * represented by @date.
 *
 * Returns: number of days in month during the year of @date
 */
guint8
gcal_date_time_get_days_in_month (GDateTime *date)
{
  return g_date_get_days_in_month (g_date_time_get_month (date),
                                   g_date_time_get_year (date));
}

/**
 * gcal_date_time_get_start_of_week:
 * @date: a #GDateTime
 *
 * Retrieves the first day of the week @date is in, at 00:00
 * of the local timezone.
 *
 * This date is inclusive.
 *
 * Returns: (transfer full): a #GDateTime
 */
GDateTime*
gcal_date_time_get_start_of_week (GDateTime *date)
{
  g_autoptr (GDateTime) start_of_week = NULL;
  gint n_days_after_week_start;
  gint first_weekday;
  gint weekday;

  g_assert (date != NULL);

  first_weekday = get_first_weekday ();
  weekday = g_date_time_get_day_of_week (date) - 1;
  n_days_after_week_start = (weekday - first_weekday) % 7;

  start_of_week = g_date_time_add_days (date, -n_days_after_week_start);

  return g_date_time_new_local (g_date_time_get_year (start_of_week),
                                g_date_time_get_month (start_of_week),
                                g_date_time_get_day_of_month (start_of_week),
                                0, 0, 0);
}

/**
 * gcal_date_time_get_end_of_week:
 * @date: a #GDateTime
 *
 * Retrieves the last day of the week @date is in, at 23:59:59
 * of the local timezone.
 *
 * Because this date is exclusive, it actually is start of the
 * next week.
 *
 * Returns: (transfer full): a #GDateTime
 */
GDateTime*
gcal_date_time_get_end_of_week (GDateTime *date)
{
  g_autoptr (GDateTime) week_start = NULL;

  week_start = gcal_date_time_get_start_of_week (date);
  return g_date_time_add_weeks (week_start, 1);
}
