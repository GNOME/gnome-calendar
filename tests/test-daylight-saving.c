/* test-daylight-saving.c
 *
 * Copyright © 2023 Daniel García Moreno <daniel.garcia@suse.com>
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

#include <glib.h>
#include "gcal-utils.h"

/*********************************************************************************************************************/

static void
floating_minutes (void)
{
  g_autoptr (GDateTime) week_start = NULL;
  g_autoptr (GDateTime) start = NULL;
  g_autoptr (GDateTime) end = NULL;
  g_autoptr (GTimeZone) cest = NULL;

  int start_cell = 3 * 2; // three hours (daylight time starts at 3:00 AM)
  int end_cell = start_cell;

  g_test_bug ("482");

  cest = g_time_zone_new_identifier ("Europe/Madrid");

  // Without daylight saving change
  week_start = g_date_time_new (cest, 2023, 11, 5, 0, 0, 0);
  g_assert_cmpstr (g_date_time_format_iso8601 (week_start), ==, "2023-11-05T00:00:00+01");
  g_assert_false (g_date_time_is_daylight_savings (week_start));

  start = gcal_date_time_add_floating_minutes (week_start, start_cell * 30);
  end = gcal_date_time_add_floating_minutes (week_start, (end_cell + 1) * 30);

  g_assert_false (g_date_time_is_daylight_savings (start));
  g_assert_false (g_date_time_is_daylight_savings (end));

  g_assert_cmpstr (g_date_time_format_iso8601 (start), ==, "2023-11-05T03:00:00+01");
  g_assert_cmpstr (g_date_time_format_iso8601 (end), ==, "2023-11-05T03:30:00+01");
}


static void
floating_minutes_october (void)
{
  g_autoptr (GDateTime) week_start = NULL;
  g_autoptr (GDateTime) start = NULL;
  g_autoptr (GDateTime) end = NULL;
  g_autoptr (GTimeZone) cest = NULL;

  int start_cell = 3 * 2; // three hours (daylight time starts at 3:00 AM)
  int end_cell = start_cell;

  g_test_bug ("482");

  cest = g_time_zone_new_identifier ("Europe/Madrid");

  // With daylight saving change
  week_start = g_date_time_new (cest, 2023, 10, 29, 0, 0, 0);
  g_assert_cmpstr (g_date_time_format_iso8601 (week_start), ==, "2023-10-29T00:00:00+02");
  g_assert_true (g_date_time_is_daylight_savings (week_start));

  start = gcal_date_time_add_floating_minutes (week_start, start_cell * 30);
  end = gcal_date_time_add_floating_minutes (week_start, (end_cell + 1) * 30);

  g_assert_cmpstr (g_date_time_format_iso8601 (start), ==, "2023-10-29T03:00:00+01");
  g_assert_cmpstr (g_date_time_format_iso8601 (end), ==, "2023-10-29T03:30:00+01");
}

static void
floating_minutes_march (void)
{
  g_autoptr (GDateTime) week_start = NULL;
  g_autoptr (GDateTime) start = NULL;
  g_autoptr (GDateTime) end = NULL;
  g_autoptr (GTimeZone) cest = NULL;

  int start_cell = 3 * 2; // three hours (daylight time starts at 3:00 AM)
  int end_cell = start_cell;

  g_test_bug ("482");

  cest = g_time_zone_new_identifier ("Europe/Madrid");

  // With daylight saving change
  week_start = g_date_time_new (cest, 2023, 3, 26, 0, 0, 0);
  g_assert_cmpstr (g_date_time_format_iso8601 (week_start), ==, "2023-03-26T00:00:00+01");
  g_assert_false (g_date_time_is_daylight_savings (week_start));

  start = gcal_date_time_add_floating_minutes (week_start, start_cell * 30);
  end = gcal_date_time_add_floating_minutes (week_start, (end_cell + 1) * 30);

  g_assert_cmpstr (g_date_time_format_iso8601 (start), ==, "2023-03-26T03:00:00+02");
  g_assert_cmpstr (g_date_time_format_iso8601 (end), ==, "2023-03-26T03:30:00+02");
}

/*********************************************************************************************************************/

gint
main (gint   argc,
      gchar *argv[])
{
  g_setenv ("TZ", "UTC", TRUE);

  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("https://gitlab.gnome.org/GNOME/gnome-calendar/-/issues/");

  g_test_add_func ("/daylight/floating-minutes", floating_minutes);
  g_test_add_func ("/daylight/floating-minutes-october", floating_minutes_october);
  g_test_add_func ("/daylight/floating-minutes-march", floating_minutes_march);

  return g_test_run ();
}
