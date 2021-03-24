/* test-range.c
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

#include "gcal-range.h"

/*********************************************************************************************************************/

static void
range_new (void)
{
  g_autoptr (GcalRange) range = NULL;
  g_autoptr (GDateTime) start = NULL;
  g_autoptr (GDateTime) end = NULL;

  start = g_date_time_new_now_utc ();
  end = g_date_time_add_seconds (start, 1);

  range = gcal_range_new (start, end, GCAL_RANGE_DEFAULT);
  g_assert_nonnull (range);
}

/*********************************************************************************************************************/

static void
range_invalid (void)
{

  g_autoptr (GcalRange) range = NULL;
  g_autoptr (GDateTime) start = NULL;
  g_autoptr (GDateTime) end = NULL;

  start = g_date_time_new_now_utc ();
  end = g_date_time_add_seconds (start, 1);

  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "gcal_range_new: assertion 'range_start' failed");
  range = gcal_range_new (NULL, NULL, GCAL_RANGE_DEFAULT);
  g_test_assert_expected_messages ();
  g_assert_null (range);

  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "gcal_range_new: assertion 'range_start' failed");
  range = gcal_range_new (NULL, end, GCAL_RANGE_DEFAULT);
  g_test_assert_expected_messages ();
  g_assert_null (range);

  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "gcal_range_new: assertion 'range_end' failed");
  range = gcal_range_new (start, NULL, GCAL_RANGE_DEFAULT);
  g_test_assert_expected_messages ();
  g_assert_null (range);

  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "gcal_range_new: assertion 'g_date_time_compare (range_start, range_end) <= 0' failed");
  range = gcal_range_new (end, start, GCAL_RANGE_DEFAULT);
  g_test_assert_expected_messages ();
  g_assert_null (range);
}

/*********************************************************************************************************************/

typedef struct
{
  const gchar *start;
  const gchar *end;
} TextRange;

static struct {
  TextRange first;
  TextRange second;
  GcalRangeOverlap expected_overlap;
  GcalRangePosition expected_position;
} ranges[] = {
  /* Equal */
  {
    { "2020-03-05T00:00:00", "2020-03-10T00:00:00" },
    { "2020-03-05T00:00:00", "2020-03-10T00:00:00" },
    GCAL_RANGE_EQUAL,
    GCAL_RANGE_MATCH,
  },

  /* Superset */
  {
    { "2020-03-05T00:00:00", "2020-03-10T00:00:00" },
    { "2020-03-05T00:00:00", "2020-03-08T00:00:00" },
    GCAL_RANGE_SUPERSET,
    GCAL_RANGE_AFTER,
  },
  {
    { "2020-03-05T00:00:00", "2020-03-10T00:00:00" },
    { "2020-03-07T00:00:00", "2020-03-08T00:00:00" },
    GCAL_RANGE_SUPERSET,
    GCAL_RANGE_BEFORE,
  },
  {
    { "2020-03-05T00:00:00", "2020-03-10T00:00:00" },
    { "2020-03-07T00:00:00", "2020-03-10T00:00:00" },
    GCAL_RANGE_SUPERSET,
    GCAL_RANGE_BEFORE,
  },

  /* Subset */
  {
    { "2020-03-05T00:00:00", "2020-03-08T00:00:00" },
    { "2020-03-05T00:00:00", "2020-03-10T00:00:00" },
    GCAL_RANGE_SUBSET,
    GCAL_RANGE_BEFORE,
  },
  {
    { "2020-03-07T00:00:00", "2020-03-08T00:00:00" },
    { "2020-03-05T00:00:00", "2020-03-10T00:00:00" },
    GCAL_RANGE_SUBSET,
    GCAL_RANGE_AFTER,
  },
  {
    { "2020-03-07T00:00:00", "2020-03-10T00:00:00" },
    { "2020-03-05T00:00:00", "2020-03-10T00:00:00" },
    GCAL_RANGE_SUBSET,
    GCAL_RANGE_AFTER,
  },

  /* Intersection */
  {
    { "2020-03-05T00:00:00", "2020-03-08T00:00:00" },
    { "2020-03-07T00:00:00", "2020-03-10T00:00:00" },
    GCAL_RANGE_INTERSECTS,
    GCAL_RANGE_BEFORE,
  },
  {
    { "2020-03-07T00:00:00", "2020-03-10T00:00:00" },
    { "2020-03-05T00:00:00", "2020-03-08T00:00:00" },
    GCAL_RANGE_INTERSECTS,
    GCAL_RANGE_AFTER,
  },

  /* No Overlap */
  {
    { "2020-03-05T00:00:00", "2020-03-10T00:00:00" },
    { "2020-03-10T00:00:00", "2020-03-15T00:00:00" },
    GCAL_RANGE_NO_OVERLAP,
    GCAL_RANGE_BEFORE,
  },
  {
    { "2020-03-10T00:00:00", "2020-03-15T00:00:00" },
    { "2020-03-05T00:00:00", "2020-03-10T00:00:00" },
    GCAL_RANGE_NO_OVERLAP,
    GCAL_RANGE_AFTER,
  },

  /* Zero length */
  {
    { "2020-03-10T00:00:00", "2020-03-15T00:00:00" },
    { "2020-03-15T00:00:00", "2020-03-15T00:00:00" },
    GCAL_RANGE_NO_OVERLAP,
    GCAL_RANGE_BEFORE,
  },
  {
    { "2020-03-10T00:00:00", "2020-03-15T00:00:00" },
    { "2020-03-10T00:00:00", "2020-03-10T00:00:00" },
    GCAL_RANGE_SUPERSET,
    GCAL_RANGE_AFTER,
  },
  {
    { "2020-03-15T00:00:00", "2020-03-15T00:00:00" },
    { "2020-03-10T00:00:00", "2020-03-15T00:00:00" },
    GCAL_RANGE_NO_OVERLAP,
    GCAL_RANGE_AFTER,
  },
  {
    { "2020-03-10T00:00:00", "2020-03-10T00:00:00" },
    { "2020-03-10T00:00:00", "2020-03-15T00:00:00" },
    GCAL_RANGE_SUBSET,
    GCAL_RANGE_BEFORE,
  },
  {
    { "2020-03-10T00:00:00", "2020-03-10T00:00:00" },
    { "2020-03-10T00:00:00", "2020-03-10T00:00:00" },
    GCAL_RANGE_EQUAL,
    GCAL_RANGE_MATCH,
  },
};

static void
range_calculate_overlap (void)
{
  g_autoptr (GTimeZone) utc = NULL;
  gint i;


  utc = g_time_zone_new_utc ();
  for (i = 0; i < G_N_ELEMENTS (ranges); i++)
    {
      g_autoptr (GDateTime) second_start = NULL;
      g_autoptr (GDateTime) second_end = NULL;
      g_autoptr (GDateTime) first_start = NULL;
      g_autoptr (GDateTime) first_end = NULL;
      g_autoptr (GcalRange) range_a = NULL;
      g_autoptr (GcalRange) range_b = NULL;
      GcalRangePosition position;
      GcalRangeOverlap overlap;

      first_start = g_date_time_new_from_iso8601 (ranges[i].first.start, utc);
      first_end = g_date_time_new_from_iso8601 (ranges[i].first.end, utc);
      second_start = g_date_time_new_from_iso8601 (ranges[i].second.start, utc);
      second_end = g_date_time_new_from_iso8601 (ranges[i].second.end, utc);

      range_a = gcal_range_new (first_start, first_end, GCAL_RANGE_DEFAULT);
      g_assert_nonnull (range_a);

      range_b = gcal_range_new (second_start, second_end, GCAL_RANGE_DEFAULT);
      g_assert_nonnull (range_b);

      overlap = gcal_range_calculate_overlap (range_a, range_b, &position);
      g_assert_cmpint (overlap, ==, ranges[i].expected_overlap);
      g_assert_cmpint (position, ==, ranges[i].expected_position);
    }
}

/*********************************************************************************************************************/

gint
main (gint   argc,
      gchar *argv[])
{
  g_setenv ("TZ", "UTC", TRUE);

  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/range/new", range_new);
  g_test_add_func ("/range/invalid-ranges", range_invalid);
  g_test_add_func ("/range/calculate-overlap", range_calculate_overlap);

  return g_test_run ();
}
