/* gcal-date-time-utils.c
 *
 * Copyright 2019-2020 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

  gcal_clear_date_time (dest);

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
  weekday = g_date_time_get_day_of_week (date) % 7;
  n_days_after_week_start = (7 + weekday - first_weekday) % 7;

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

/**
 * gcal_date_time_compare_date:
 * @dt1: (nullable): a #GDateTime
 * @dt2: (nullable): a #GDateTime
 *
 * Compares the dates of @dt1 and @dt2. The times are
 * ignored.
 *
 * Returns: negative, 0 or positive
 */
gint
gcal_date_time_compare_date (GDateTime *dt1,
                             GDateTime *dt2)
{
  GDate d1, d2;

  if (!dt1 && !dt2)
    return 0;
  else if (!dt1)
    return -1;
  else if (!dt2)
    return 1;

  g_date_set_dmy (&d1,
                  g_date_time_get_day_of_month (dt1),
                  g_date_time_get_month (dt1),
                  g_date_time_get_year (dt1));

  g_date_set_dmy (&d2,
                  g_date_time_get_day_of_month (dt2),
                  g_date_time_get_month (dt2),
                  g_date_time_get_year (dt2));

  return g_date_days_between (&d2, &d1);
}

/**
 * gcal_date_time_to_icaltime:
 * @dt: a #GDateTime
 *
 * Converts the #GDateTime's @dt to an #ICalTime.
 *
 * Returns: (transfer full): a #ICalTime.
 */
ICalTime*
gcal_date_time_to_icaltime (GDateTime *dt)
{
  ICalTime *idt;

  if (!dt)
    return NULL;

  idt = i_cal_time_new_null_time ();

  i_cal_time_set_date (idt,
                       g_date_time_get_year (dt),
                       g_date_time_get_month (dt),
                       g_date_time_get_day_of_month (dt));
  i_cal_time_set_time (idt,
                       g_date_time_get_hour (dt),
                       g_date_time_get_minute (dt),
                       g_date_time_get_seconds (dt));
  i_cal_time_set_is_date (idt,
                  i_cal_time_get_hour (idt) == 0 &&
                  i_cal_time_get_minute (idt) == 0 &&
                  i_cal_time_get_second (idt) == 0);

  return idt;

}

/**
 * gcal_date_time_is_date:
 * @dt: a #GDateTime
 *
 * Checks if @dt represents a date. A pure date
 * has the values of hour, minutes and seconds set
 * to 0.
 *
 * Returns: %TRUE if @dt is a date, %FALSE if it's a
 * timed datetime.
 */
gboolean
gcal_date_time_is_date (GDateTime *dt)
{
  return g_date_time_get_hour (dt) == 0 &&
         g_date_time_get_minute (dt) == 0 &&
         g_date_time_get_seconds (dt) == 0;
}

/**
 * gcal_date_time_from_icaltime:
 * @date: an #ICalTime
 *
 * Creates a #GDateTime from @date. The timezone is preserved.
 *
 * Returns: (transfer full): a #GDateTime.
 */
GDateTime*
gcal_date_time_from_icaltime (const ICalTime *date)
{
  g_autoptr (GTimeZone) tz = NULL;
  g_autoptr (GDateTime) dt = NULL;
  const gchar *identifier;
  ICalTimezone *zone;

  zone = i_cal_time_get_timezone (date);
  identifier = zone ? i_cal_timezone_get_location (zone) : NULL;
  tz = identifier ? g_time_zone_new_identifier (identifier) : g_time_zone_new_utc ();
  dt = g_date_time_new (tz,
                        i_cal_time_get_year (date),
                        i_cal_time_get_month (date),
                        i_cal_time_get_day (date),
                        i_cal_time_is_date (date) ? 0 : i_cal_time_get_hour (date),
                        i_cal_time_is_date (date) ? 0 : i_cal_time_get_minute (date),
                        i_cal_time_is_date (date) ? 0 : i_cal_time_get_second (date));

  return g_steal_pointer (&dt);
}

/**
 * @tz: a #GTimezone
 *
 * Returns an #ICalTimezone corresponding to @tz.
 *
 * Returns: (transfer none): an #ICalTimezone.
 */
ICalTimezone*
gcal_timezone_to_icaltimezone (GTimeZone *tz)
{
  ICalTimezone *ical_tz;
  const gchar *tzid;

  tzid = g_time_zone_get_identifier (tz);
  ical_tz = i_cal_timezone_get_builtin_timezone (tzid);

  if (!ical_tz)
    ical_tz = i_cal_timezone_get_utc_timezone ();

  return ical_tz;
}

