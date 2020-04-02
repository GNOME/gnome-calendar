/* test-range-tree.c
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

#include "gcal-range-tree.h"

/*********************************************************************************************************************/

static void
range_tree_new (void)
{
  g_autoptr (GcalRangeTree) range_tree = NULL;

  range_tree = gcal_range_tree_new ();
  g_assert_nonnull (range_tree);
}

/*********************************************************************************************************************/

static void
range_tree_insert (void)
{
  g_autoptr (GcalRangeTree) range_tree = NULL;
  g_autoptr (GDateTime) start = NULL;
  g_autoptr (GDateTime) end = NULL;

  range_tree = gcal_range_tree_new ();
  g_assert_nonnull (range_tree);

  start = g_date_time_new_local (2020, 3, 17, 9, 30, 0);
  end = g_date_time_add_hours (start, 1);

  gcal_range_tree_add_range (range_tree, start, end, (gpointer) 0xdeadbeef);
  g_assert_cmpint (gcal_range_tree_count_entries_at_range (range_tree, start, end), ==, 1);

  gcal_range_tree_add_range (range_tree, start, end, (gpointer) 0x8badf00d);
  g_assert_cmpint (gcal_range_tree_count_entries_at_range (range_tree, start, end), ==, 2);

  gcal_range_tree_add_range (range_tree, start, end, (gpointer) 0x0badcafe);
  g_assert_cmpint (gcal_range_tree_count_entries_at_range (range_tree, start, end), ==, 3);
}

/*********************************************************************************************************************/


static struct {
  const gchar *start;
  const gchar *end;
} ranges[] = {
  { "2020-03-05T00:00:00", "2020-03-10T00:00:00" },
  { "2020-03-05T00:00:00", "2020-03-10T00:00:00" },
  { "2020-03-18T00:00:00", "2020-03-20T00:00:00" },
  { "2020-03-05T00:00:00", "2020-03-20T00:00:00" },
  { "2020-03-05T00:00:00", "2020-03-28T00:00:00" },
  { "2020-03-12T00:00:00", "2020-03-20T00:00:00" },
  { "2020-03-22T00:00:00", "2020-03-28T00:00:00" },
};

static gboolean
traverse_func (GDateTime *start,
               GDateTime *end,
               gpointer   data,
               gpointer   user_data)
{
  g_assert_cmpint (GPOINTER_TO_INT (data), >=, 0);
  g_assert_cmpint (GPOINTER_TO_INT (data), <, G_N_ELEMENTS (ranges));
  return GCAL_TRAVERSE_CONTINUE;
}

static void
range_tree_traverse (void)
{
  g_autoptr (GcalRangeTree) range_tree = NULL;
  g_autoptr (GTimeZone) utc = NULL;
  gint i;

  range_tree = gcal_range_tree_new ();
  g_assert_nonnull (range_tree);

  utc = g_time_zone_new_utc ();
  for (i = 0; i < G_N_ELEMENTS (ranges); i++)
    {
      g_autoptr (GDateTime) start = NULL;
      g_autoptr (GDateTime) end = NULL;

      start = g_date_time_new_from_iso8601 (ranges[i].start, utc);
      end = g_date_time_new_from_iso8601 (ranges[i].end, utc);

      gcal_range_tree_add_range (range_tree, start, end, GINT_TO_POINTER (i));
    }

  gcal_range_tree_traverse (range_tree, G_PRE_ORDER, traverse_func, NULL);
  gcal_range_tree_traverse (range_tree, G_IN_ORDER, traverse_func, NULL);
  gcal_range_tree_traverse (range_tree, G_POST_ORDER, traverse_func, NULL);
}

/*********************************************************************************************************************/

static void
range_tree_smaller_range (void)
{
  g_autoptr (GcalRangeTree) range_tree = NULL;
  g_autoptr (GDateTime) range_start = NULL;
  g_autoptr (GDateTime) range_end = NULL;
  g_autoptr (GDateTime) start = NULL;
  g_autoptr (GDateTime) end = NULL;

  range_tree = gcal_range_tree_new ();
  g_assert_nonnull (range_tree);

  start = g_date_time_new_local (2020, 3, 17, 9, 30, 0);
  end = g_date_time_add_hours (start, 1);

  gcal_range_tree_add_range (range_tree, start, end, (gpointer) 0xdeadbeef);
  g_assert_cmpint (gcal_range_tree_count_entries_at_range (range_tree, start, end), ==, 1);

  range_start = g_date_time_new_local (2020, 3, 20, 9, 30, 0);
  range_end = g_date_time_add_hours (range_start, 1);

  g_assert_cmpint (gcal_range_tree_count_entries_at_range (range_tree, range_start, range_end), ==, 0);
}

/*********************************************************************************************************************/

gint
main (gint   argc,
      gchar *argv[])
{
  g_setenv ("TZ", "UTC", TRUE);

  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/range-tree/new", range_tree_new);
  g_test_add_func ("/range-tree/insert", range_tree_insert);
  g_test_add_func ("/range-tree/traverse", range_tree_traverse);
  g_test_add_func ("/range-tree/smaller-range", range_tree_smaller_range);

  return g_test_run ();
}
