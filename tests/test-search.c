/* test-search.c
 *
 * Copyright 2026 GNOME Calendar Contributors
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

#include <glib.h>
#include <glib/gi18n.h>

#include "libecal/libecal.h"

G_DEFINE_AUTOPTR_CLEANUP_FUNC (ECalComponent, g_object_unref)

#include "gcal-calendar-monitor.h"
#include "gcal-event.h"
#include "gcal-search-hit.h"
#include "gcal-search-hit-event.h"
#include "gcal-stub-calendar.h"
#include "gcal-timeline.h"

#define STUB_EVENT_RECURRING "BEGIN:VEVENT\n"             \
                             "SUMMARY:Standup\n"          \
                             "UID:recurring@uid\n"        \
                             "DTSTAMP:19970114T000000Z\n" \
                             "DTSTART:20180714T000000Z\n" \
                             "DTEND:20180714T010000Z\n"   \
                             "RRULE:FREQ=DAILY;COUNT=3\n" \
                             "END:VEVENT\n"

#define STUB_EVENT_SINGLE "BEGIN:VEVENT\n"             \
                          "SUMMARY:Doctor Visit\n"     \
                          "UID:single@uid\n"           \
                          "DTSTAMP:19970114T170000Z\n" \
                          "DTSTART:20180716T170000Z\n" \
                          "DTEND:20180716T180000Z\n"   \
                          "END:VEVENT\n"

static GcalEvent *
create_event_for_string (const gchar  *string,
                         GError      **error)
{
  g_autoptr (ECalComponent) component = NULL;
  g_autoptr (GcalCalendar) calendar = NULL;

  component = e_cal_component_new_from_string (string);
  calendar = gcal_stub_calendar_new (NULL, error);

  return component ? gcal_event_new (calendar, component, error) : NULL;
}

static void
test_timeline_expand_recurrences (void)
{
  g_autoptr (GcalTimeline) timeline_default = NULL;
  g_autoptr (GcalTimeline) timeline_search = NULL;
  g_autoptr (GcalCalendar) calendar = NULL;
  g_autoptr (GError) error = NULL;

  g_test_bug ("991");

  timeline_default = gcal_timeline_new ();
  g_assert_true (gcal_timeline_get_expand_recurrences (timeline_default));

  timeline_search = g_object_new (GCAL_TYPE_TIMELINE,
                                  "expand-recurrences", FALSE,
                                  NULL);
  g_assert_false (gcal_timeline_get_expand_recurrences (timeline_search));

  calendar = gcal_stub_calendar_new (NULL, &error);
  g_assert_no_error (error);

  gcal_timeline_add_calendar (timeline_search, calendar);
  g_assert_false (gcal_timeline_get_expand_recurrences (timeline_search));
}

static void
test_calendar_monitor_expand_recurrences (void)
{
  g_autoptr (GcalCalendar) calendar = NULL;
  g_autoptr (GcalCalendarMonitor) monitor = NULL;
  g_autoptr (GcalCalendarMonitor) monitor_no_recur = NULL;
  g_autoptr (GError) error = NULL;

  g_test_bug ("991");

  calendar = gcal_stub_calendar_new (NULL, &error);
  g_assert_no_error (error);

  monitor = gcal_calendar_monitor_new (calendar);
  g_assert_true (gcal_calendar_monitor_get_expand_recurrences (monitor));

  monitor_no_recur = g_object_new (GCAL_TYPE_CALENDAR_MONITOR,
                                   "calendar", calendar,
                                   "expand-recurrences", FALSE,
                                   NULL);
  g_assert_false (gcal_calendar_monitor_get_expand_recurrences (monitor_no_recur));
}

static void
test_search_hit_recurring_subtitle (void)
{
  g_autoptr (GcalEvent) event_recurring = NULL;
  g_autoptr (GcalEvent) event_single = NULL;
  g_autoptr (GcalSearchHitEvent) hit_recurring = NULL;
  g_autoptr (GcalSearchHitEvent) hit_single = NULL;
  g_autoptr (GError) error = NULL;
  const gchar *subtitle;

  g_test_bug ("991");

  event_recurring = create_event_for_string (STUB_EVENT_RECURRING, &error);
  g_assert_no_error (error);
  g_assert_true (gcal_event_has_recurrence (event_recurring));

  event_single = create_event_for_string (STUB_EVENT_SINGLE, &error);
  g_assert_no_error (error);
  g_assert_false (gcal_event_has_recurrence (event_single));

  hit_recurring = gcal_search_hit_event_new (event_recurring);
  subtitle = gcal_search_hit_get_subtitle (GCAL_SEARCH_HIT (hit_recurring));
  g_assert_nonnull (subtitle);
  g_assert_nonnull (strstr (subtitle, _ ("Recurring")));

  hit_single = gcal_search_hit_event_new (event_single);
  subtitle = gcal_search_hit_get_subtitle (GCAL_SEARCH_HIT (hit_single));
  g_assert_nonnull (subtitle);
  g_assert_null (strstr (subtitle, _ ("Recurring")));
}

gint
main (gint   argc,
      gchar *argv[])
{
  g_setenv ("TZ", "UTC", TRUE);
  g_setenv ("LC_ALL", "C", TRUE);
  g_setenv ("LANGUAGE", "C", TRUE);

  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("https://gitlab.gnome.org/GNOME/gnome-calendar/-/issues/");

  g_test_add_func ("/timeline/expand-recurrences", test_timeline_expand_recurrences);
  g_test_add_func ("/calendar-monitor/expand-recurrences", test_calendar_monitor_expand_recurrences);
  g_test_add_func ("/search/hit/recurring-subtitle", test_search_hit_recurring_subtitle);

  return g_test_run ();
}
