/* gcal-event-schedule.c - immutable struct for the date and recurrence values of an event
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

#define G_LOG_DOMAIN "GcalEventSchedule"

#include "gcal-date-time-utils.h"
#include "gcal-debug.h"
#include "gcal-event.h"
#include "gcal-recurrence.h"
#include "gcal-event-schedule.h"

GcalScheduleValues
gcal_schedule_values_copy (const GcalScheduleValues *values)
{
  GcalScheduleValues copy;

  copy.all_day = values->all_day;
  copy.date_start = values->date_start ? g_date_time_ref (values->date_start) : NULL;
  copy.date_end = values->date_end ? g_date_time_ref (values->date_end) : NULL;
  copy.recur = values->recur ? gcal_recurrence_copy (values->recur) : NULL;

  return copy;
}

static GcalEventSchedule *
gcal_event_schedule_copy (const GcalEventSchedule *values)
{
  GcalEventSchedule *copy = g_new0 (GcalEventSchedule, 1);

  copy->orig = gcal_schedule_values_copy (&values->orig);
  copy->curr = gcal_schedule_values_copy (&values->curr);
  copy->time_format = values->time_format;

  return copy;
}

static void
gcal_schedule_values_free (GcalScheduleValues *values)
{
  values->all_day = FALSE;

  g_clear_pointer (&values->date_start, g_date_time_unref);
  g_clear_pointer (&values->date_end, g_date_time_unref);
  g_clear_pointer (&values->recur, gcal_recurrence_unref);
}

/**
 * gcal_event_schedule_free():
 *
 * Frees the contents of @values.
 */
void
gcal_event_schedule_free (GcalEventSchedule *values)
{
  gcal_schedule_values_free (&values->orig);
  gcal_schedule_values_free (&values->curr);
  g_free (values);
}

GcalEventSchedule*
gcal_event_schedule_set_all_day (const GcalEventSchedule *values, gboolean all_day)
{
  GcalEventSchedule *copy = gcal_event_schedule_copy (values);

  if (all_day == values->curr.all_day)
    {
      return copy;
    }

  if (all_day)
    {
      /* We are switching from a time-slot event to an all-day one.  If we had
       *
       *   date_start = $day1 + $time
       *   date_end   = $day2 + $time
       *
       * we want to switch to
       *
       *   date_start = $day1 + 00:00 (midnight)
       *   date_end   = $day2 + 1 day + 00:00 (midnight of the next day)
       */

      GDateTime *start = values->curr.date_start;
      GDateTime *end = values->curr.date_end;

      g_autoptr (GDateTime) new_start = g_date_time_new (g_date_time_get_timezone (start),
                                                         g_date_time_get_year (start),
                                                         g_date_time_get_month (start),
                                                         g_date_time_get_day_of_month (start),
                                                         0,
                                                         0,
                                                         0.0);

      g_autoptr (GDateTime) tmp = g_date_time_new (g_date_time_get_timezone (end),
                                                   g_date_time_get_year (end),
                                                   g_date_time_get_month (end),
                                                   g_date_time_get_day_of_month (end),
                                                   0,
                                                   0,
                                                   0.0);
      g_autoptr (GDateTime) new_end = g_date_time_add_days (tmp, 1);

      gcal_set_date_time (&copy->curr.date_start, new_start);
      gcal_set_date_time (&copy->curr.date_end, new_end);
    }
  else
    {
      /* We are switching from an all-day event to a time-slot one.  In this case,
       * we want to preserve the current dates, but restore the times of the original
       * event.
       */

      GDateTime *start = values->curr.date_start;
      GDateTime *end = values->curr.date_end;

      g_autoptr (GDateTime) new_start = g_date_time_new (g_date_time_get_timezone (start),
                                                         g_date_time_get_year (start),
                                                         g_date_time_get_month (start),
                                                         g_date_time_get_day_of_month (start),
                                                         g_date_time_get_hour (values->orig.date_start),
                                                         g_date_time_get_minute (values->orig.date_start),
                                                         g_date_time_get_seconds (values->orig.date_start));

      g_autoptr (GDateTime) tmp = g_date_time_new (g_date_time_get_timezone (end),
                                                   g_date_time_get_year (end),
                                                   g_date_time_get_month (end),
                                                   g_date_time_get_day_of_month (end),
                                                   g_date_time_get_hour (values->orig.date_end),
                                                   g_date_time_get_minute (values->orig.date_end),
                                                   g_date_time_get_seconds (values->orig.date_end));

      g_autoptr (GDateTime) new_end = g_date_time_add_days (tmp, -1);

      gcal_set_date_time (&copy->curr.date_start, new_start);
      gcal_set_date_time (&copy->curr.date_end, new_end);
    }

  copy->curr.all_day = all_day;
  return copy;
}

GcalEventSchedule *
gcal_event_schedule_set_time_format (const GcalEventSchedule *values, GcalTimeFormat time_format)
{
  GcalEventSchedule *copy = gcal_event_schedule_copy (values);

  copy->time_format = time_format;
  return copy;
}

/* The start_date_row widget has changed.  We need to sync it to the values and
 * adjust the other values based on it.
 */
GcalEventSchedule *
gcal_event_schedule_set_start_date (const GcalEventSchedule *values, GDateTime *start)
{
  GcalEventSchedule *copy = gcal_event_schedule_copy (values);

  gcal_set_date_time (&copy->curr.date_start, start);

  if (g_date_time_compare (start, copy->curr.date_end) == 1)
    {
      gcal_set_date_time (&copy->curr.date_end, start);
    }

  return copy;
}

/* The end_date_row widget has changed.  We need to sync it to the values and
 * adjust the other values based on it.
 */
GcalEventSchedule *
gcal_event_schedule_set_end_date (const GcalEventSchedule *values, GDateTime *end)
{
  GcalEventSchedule *copy = gcal_event_schedule_copy (values);

  if (copy->curr.all_day)
    {
      /* See the comment "While in iCalendar..." in widget_state_from_values().  Here we
       * take the human-readable end-date, and turn it into the appropriate value for
       * iCalendar.
       */
      g_autoptr (GDateTime) fake_end_date = g_date_time_add_days (end, 1);
      gcal_set_date_time (&copy->curr.date_end, fake_end_date);
    }
  else
    {
      gcal_set_date_time (&copy->curr.date_end, end);
    }

  if (g_date_time_compare (copy->curr.date_start, end) == 1)
    {
      gcal_set_date_time (&copy->curr.date_start, end);
    }

  return copy;
}

GcalEventSchedule *
gcal_event_schedule_set_start_date_time (const GcalEventSchedule *values, GDateTime *start)
{
  GcalEventSchedule *copy = gcal_event_schedule_copy (values);

  gcal_set_date_time (&copy->curr.date_start, start);

  if (g_date_time_compare (start, copy->curr.date_end) == 1)
    {
      GTimeZone *end_tz = g_date_time_get_timezone (copy->curr.date_end);
      g_autoptr (GDateTime) new_end = g_date_time_add_hours (start, 1);
      g_autoptr (GDateTime) new_end_in_end_tz = g_date_time_to_timezone (new_end, end_tz);

      gcal_set_date_time (&copy->curr.date_end, new_end_in_end_tz);
    }

  return copy;
}

GcalEventSchedule *
gcal_event_schedule_set_end_date_time (const GcalEventSchedule *values, GDateTime *end)
{
  GcalEventSchedule *copy = gcal_event_schedule_copy (values);

  gcal_set_date_time (&copy->curr.date_end, end);

  if (g_date_time_compare (copy->curr.date_start, end) == 1)
    {
      GTimeZone *start_tz = g_date_time_get_timezone (copy->curr.date_start);
      g_autoptr (GDateTime) new_start = g_date_time_add_hours (end, -1);
      g_autoptr (GDateTime) new_start_in_start_tz = g_date_time_to_timezone (new_start, start_tz);

      gcal_set_date_time (&copy->curr.date_start, new_start_in_start_tz);
    }

  return copy;
}

static GcalRecurrence *
recur_change_frequency (GcalRecurrence *old_recur, GcalRecurrenceFrequency frequency)
{
  if (frequency == GCAL_RECURRENCE_NO_REPEAT)
    {
      /* Invariant: GCAL_RECURRENCE_NO_REPEAT is reduced to a NULL recurrence */
      return NULL;
    }
  else
    {
      GcalRecurrence *new_recur;

      if (old_recur)
        {
          new_recur = gcal_recurrence_copy (old_recur);
        }
      else
        {
          new_recur = gcal_recurrence_new ();
        }

      new_recur->frequency = frequency;

      return new_recur;
    }
}

static GcalRecurrence *
recur_change_limit_type (GcalRecurrence *old_recur, GcalRecurrenceLimitType limit_type, GDateTime *date_start)
{
  /* An old recurrence would already be present with something other than GCAL_RECURRENCE_NO_REPEAT */
  g_assert (old_recur != NULL);
  g_assert (old_recur->frequency != GCAL_RECURRENCE_NO_REPEAT);

  GcalRecurrence *new_recur = gcal_recurrence_copy (old_recur);

  if (limit_type == old_recur->limit_type)
    {
      return new_recur;
    }
  else
    {
      new_recur->limit_type = limit_type;

      switch (limit_type)
        {
        case GCAL_RECURRENCE_FOREVER:
          break;

        case GCAL_RECURRENCE_COUNT:
          /* Other policies are possible.  This is just "more than once". */
          new_recur->limit.count = 2;
          break;

        case GCAL_RECURRENCE_UNTIL:
          /* Again, other policies are possible.  For now, leave the decision to the user. */
          g_assert (date_start != NULL);
          new_recur->limit.until = g_date_time_ref (date_start);
          break;

        default:
          g_assert_not_reached ();
        }
    }

  return new_recur;
}

GcalEventSchedule *
gcal_event_schedule_set_recur_frequency (const GcalEventSchedule *values,
                                         GcalRecurrenceFrequency frequency)
{
  GcalEventSchedule *copy = gcal_event_schedule_copy (values);
  GcalRecurrence *new_recur = recur_change_frequency (copy->curr.recur, frequency);
  g_clear_pointer (&copy->curr.recur, gcal_recurrence_unref);
  copy->curr.recur = new_recur;

  return copy;
}

GcalEventSchedule *
gcal_event_schedule_set_recur_limit_type (const GcalEventSchedule *values,
                                          GcalRecurrenceLimitType limit_type)
{
  GcalEventSchedule *copy = gcal_event_schedule_copy (values);
  GcalRecurrence *new_recur = recur_change_limit_type (copy->curr.recur, limit_type, values->curr.date_start);
  g_clear_pointer (&copy->curr.recur, gcal_recurrence_unref);
  copy->curr.recur = new_recur;

  return copy;
}

GcalEventSchedule *
gcal_event_schedule_set_recurrence_count (const GcalEventSchedule *values,
                                          guint count)
{
  GcalEventSchedule *copy = gcal_event_schedule_copy (values);

  /* An old recurrence would already be present */
  g_assert (copy->curr.recur != NULL);
  g_assert (copy->curr.recur->frequency != GCAL_RECURRENCE_NO_REPEAT);
  g_assert (copy->curr.recur->limit_type == GCAL_RECURRENCE_COUNT);

  copy->curr.recur->limit.count = count;

  return copy;
}

GcalEventSchedule *
gcal_event_schedule_set_recurrence_until (const GcalEventSchedule *values,
                                          GDateTime *until)
{
  GcalEventSchedule *copy = gcal_event_schedule_copy (values);

  /* An old recurrence would already be present */
  g_assert (copy->curr.recur != NULL);
  g_assert (copy->curr.recur->frequency != GCAL_RECURRENCE_NO_REPEAT);
  g_assert (copy->curr.recur->limit_type == GCAL_RECURRENCE_UNTIL);

  gcal_set_date_time (&copy->curr.recur->limit.until, until);

  return copy;
}

/**
 * Extracts the values from @event that are needed to populate #GcalScheduleSection.
 *
 * Returns: a #GcalEventSchedule ready for use.  Free it with
 * gcal_event_schedule_free().
 */
GcalEventSchedule *
gcal_event_schedule_from_event (GcalEvent      *event,
                                GcalTimeFormat  time_format)
{
  GcalScheduleValues values;
  memset (&values, 0, sizeof (values));

  if (event)
    {
      GcalRecurrence *recur = gcal_event_get_recurrence (event);

      values.all_day = gcal_event_get_all_day (event);
      values.date_start = g_date_time_ref (gcal_event_get_date_start (event));
      values.date_end = g_date_time_ref (gcal_event_get_date_end (event));

      if (recur)
        {
          values.recur = gcal_recurrence_copy (recur);
        }
      else
        {
          values.recur = NULL;
        }
    }

  GcalEventSchedule *section_values = g_new0 (GcalEventSchedule, 1);

  *section_values = (GcalEventSchedule) {
    .orig = gcal_schedule_values_copy (&values),
    .curr = values,
    .time_format = time_format,
  };

  return section_values;
}

/* Builds a new GcalscheduleSectgionValues from two ISO 8601 date-times; be sure to
 * include your timezone if you need it.  This function is just to be used from tests.
 **/
static GcalScheduleValues
values_with_date_times (const char *start, const char *end, gboolean all_day)
{
  g_autoptr (GDateTime) start_date = g_date_time_new_from_iso8601 (start, NULL);
  g_assert (start_date != NULL);

  g_autoptr (GDateTime) end_date = g_date_time_new_from_iso8601 (end, NULL);
  g_assert (end_date != NULL);

  g_assert (g_date_time_compare (start_date, end_date) == -1);

  GcalScheduleValues values = {
    .all_day = all_day,
    .date_start = g_date_time_ref (start_date),
    .date_end = g_date_time_ref (end_date),
    .recur = NULL,
  };

  return values;
}

/* This is just for writing tests */
GcalEventSchedule *
gcal_event_schedule_with_date_times (const char *start, const char *end, gboolean all_day)
{
  GcalScheduleValues values = values_with_date_times (start, end, all_day);

  GcalEventSchedule *section_values = g_new0 (GcalEventSchedule, 1);

  *section_values = (GcalEventSchedule) {
    .orig = gcal_schedule_values_copy (&values),
    .curr = values,
    .time_format = GCAL_TIME_FORMAT_24H,
  };

  return section_values;
}


static void
test_setting_start_date_after_end_date_resets_end_date (void)
{
  /* We start with
   *   start = 10:00
   *   end   = 11:00
   *
   * Then we set start to 12:00
   *
   * We want to test that end becomes 12:00 as well.
   */
  g_autoptr (GcalEventSchedule) values = gcal_event_schedule_with_date_times("20250303T10:00:00-06:00",
                                                                             "20250303T11:00:00-06:00",
                                                                             FALSE);

  g_autoptr (GDateTime) two_hours_later = g_date_time_new_from_iso8601 ("20250303T12:00:00-06:00", NULL);

  g_autoptr (GcalEventSchedule) new_values = gcal_event_schedule_set_start_date (values, two_hours_later);
  g_assert (g_date_time_equal (new_values->curr.date_start, two_hours_later));
  g_assert (g_date_time_equal (new_values->curr.date_end, two_hours_later));
}

static void
test_setting_end_date_before_start_date_resets_start_date (void)
{
  /* We start with
   *   start = 10:00
   *   end   = 11:00
   *
   * Then we set end to 09:00
   *
   * We want to test that start becomes 09:00 as well.
   */
  g_autoptr (GcalEventSchedule) values = gcal_event_schedule_with_date_times("20250303T10:00:00-06:00",
                                                                             "20250303T11:00:00-06:00",
                                                                             FALSE);

  g_autoptr (GDateTime) two_hours_earlier = g_date_time_new_from_iso8601 ("20250303T09:00:00-06:00", NULL);

  g_autoptr (GcalEventSchedule) new_values = gcal_event_schedule_set_end_date (values, two_hours_earlier);
  g_assert (g_date_time_equal (new_values->curr.date_start, two_hours_earlier));
  g_assert (g_date_time_equal (new_values->curr.date_end, two_hours_earlier));
}

static void
test_setting_start_datetime_preserves_end_timezone (void)
{
  /* Start with
   *   start = 10:00, UTC-6
   *   end   = 11:30, UTC-5  (note the different timezone; event is 30 minutes long)
   *
   * Set the start date to 11:30, UTC-6
   *
   * End date should be 13:30, UTC-5 (i.e. end_date was set to the same as start_date, but in a different tz)
   *
   * (imagine driving for one hour while crossing timezones)
   */
  g_autoptr (GcalEventSchedule) values = gcal_event_schedule_with_date_times("20250303T10:00:00-06:00",
                                                                             "20250303T11:30:00-05:00",
                                                                             FALSE);
  g_assert (g_date_time_compare (values->curr.date_start, values->curr.date_end) == -1);

  g_autoptr (GDateTime) new_start = g_date_time_new_from_iso8601 ("20250303T11:30:00-06:00", NULL);
  g_autoptr (GDateTime) expected_end = g_date_time_new_from_iso8601 ("20250303T13:30:00-05:00", NULL);

  g_autoptr (GcalEventSchedule) new_values = gcal_event_schedule_set_start_date_time (values, new_start);
  g_assert (g_date_time_equal (new_values->curr.date_start, new_start));

  g_assert (g_date_time_equal (new_values->curr.date_end, expected_end));
}

static void
test_setting_end_datetime_preserves_start_timezone (void)
{
  /* Start with
   *   start = 10:00, UTC-6
   *   end   = 11:30, UTC-5  (note the different timezone; event is 30 minutes long)
   *
   * Set the end date to 09:30, UTC-5
   *
   * Start date should be 07:30, UTC-6 (set to one hour earlier than the end time, with the original start's tz)
   *
   * (imagine driving for one hour while crossing timezones)
   */
  g_autoptr (GcalEventSchedule) values = gcal_event_schedule_with_date_times("20250303T10:00:00-06:00",
                                                                             "20250303T11:30:00-05:00",
                                                                             FALSE);
  g_assert (g_date_time_compare (values->curr.date_start, values->curr.date_end) == -1);

  g_autoptr (GDateTime) new_end = g_date_time_new_from_iso8601 ("20250303T09:30:00-05:00", NULL);
  g_autoptr (GDateTime) expected_start = g_date_time_new_from_iso8601 ("20250303T07:30:00-06:00", NULL);

  g_autoptr (GcalEventSchedule) new_values = gcal_event_schedule_set_end_date_time (values, new_end);
  g_assert (g_date_time_equal (new_values->curr.date_end, new_end));

  g_assert (g_date_time_equal (new_values->curr.date_start, expected_start));
}

static void
test_43_switching_to_all_day_preserves_timezones (void)
{
  g_test_bug ("43");
  /* https://gitlab.gnome.org/GNOME/gnome-calendar/-/issues/43
   *
   * Given an event that is not all-day, e.g. from $day1-12:00 to $day2-13:00, if one turns it to an
   * all-day event, we want to switch the dates like this:
   *
   *   start: $day1's 00:00
   *   end: $day2's 24:00 (e.g. ($day2 + 1)'s 00:00)
   *
   * Also test that after that, changing the dates and turning off all-day restores the
   * original times.
   */

  g_autoptr (GcalEventSchedule) values = gcal_event_schedule_with_date_times ("20250303T12:00:00-06:00",
                                                                              "20250304T13:00:00-06:00",
                                                                              FALSE);

  /* set all-day and check that this modified the times to midnight */

  g_autoptr (GcalEventSchedule) all_day_values = gcal_event_schedule_set_all_day (values, TRUE);

  g_autoptr (GDateTime) expected_start_date = g_date_time_new_from_iso8601 ("20250303T00:00:00-06:00", NULL);
  g_autoptr (GDateTime) expected_end_date = g_date_time_new_from_iso8601 ("20250305T00:00:00-06:00", NULL);

  g_assert (g_date_time_equal (all_day_values->curr.date_start, expected_start_date));
  g_assert (g_date_time_equal (all_day_values->curr.date_end, expected_end_date));

  /* turn off all-day; check that times were restored */

  g_autoptr (GcalEventSchedule) not_all_day_values = gcal_event_schedule_set_all_day (all_day_values, FALSE);

  g_autoptr (GDateTime) expected_start_date2 = g_date_time_new_from_iso8601 ("20250303T12:00:00-06:00", NULL);
  g_autoptr (GDateTime) expected_end_date2 = g_date_time_new_from_iso8601 ("20250304T13:00:00-06:00", NULL);

  g_assert (g_date_time_equal (not_all_day_values->curr.date_start, expected_start_date2));
  g_assert (g_date_time_equal (not_all_day_values->curr.date_end, expected_end_date2));
}

static void
test_recur_changes_to_no_repeat (void)
{
  g_autoptr (GcalRecurrence) old_recur = gcal_recurrence_new ();
  old_recur->frequency = GCAL_RECURRENCE_WEEKLY;

  g_autoptr (GcalRecurrence) new_recur = recur_change_frequency (old_recur, GCAL_RECURRENCE_NO_REPEAT);
  g_assert_null (new_recur);
}

static void
test_recur_changes_limit_type_to_count (void)
{
  g_autoptr (GcalRecurrence) old_recur = gcal_recurrence_new ();
  old_recur->frequency = GCAL_RECURRENCE_WEEKLY;

  g_autoptr (GcalRecurrence) new_recur = recur_change_limit_type (old_recur, GCAL_RECURRENCE_COUNT, NULL);
  g_assert_cmpint (new_recur->limit_type, ==, GCAL_RECURRENCE_COUNT);
  g_assert_cmpint (new_recur->limit.count, ==, 2);
}

static void
test_recur_changes_limit_type_to_until (void)
{
  g_autoptr (GcalRecurrence) old_recur = gcal_recurrence_new ();
  old_recur->frequency = GCAL_RECURRENCE_WEEKLY;

  g_autoptr (GDateTime) date = g_date_time_new_from_iso8601 ("20250411T00:00:00-06:00", NULL);

  g_autoptr (GcalRecurrence) new_recur = recur_change_limit_type (old_recur, GCAL_RECURRENCE_UNTIL, date);
  g_assert_cmpint (new_recur->limit_type, ==, GCAL_RECURRENCE_UNTIL);
  g_assert (g_date_time_equal (new_recur->limit.until, date));
}

void
gcal_event_schedule_add_tests (void)
{
  g_test_add_func ("/event_editor/event_schedule/setting_start_date_after_end_date_resets_end_date",
                   test_setting_start_date_after_end_date_resets_end_date);
  g_test_add_func ("/event_editor/event_schedule/setting_end_date_before_start_date_resets_start_date",
                   test_setting_end_date_before_start_date_resets_start_date);
  g_test_add_func ("/event_editor/event_schedule/setting_start_datetime_preserves_end_timezone",
                   test_setting_start_datetime_preserves_end_timezone);
  g_test_add_func ("/event_editor/event_schedule/setting_end_datetime_preserves_start_timezone",
                   test_setting_end_datetime_preserves_start_timezone);
  g_test_add_func ("/event_editor/event_schedule/43_switching_to_all_day_preserves_timezones",
                   test_43_switching_to_all_day_preserves_timezones);
  g_test_add_func ("/event_editor/event_schedule/recur_changes_to_no_repeat",
                   test_recur_changes_to_no_repeat);
  g_test_add_func ("/event_editor/event_schedule/recur_changes_limit_type_to_count",
                   test_recur_changes_limit_type_to_count);
  g_test_add_func ("/event_editor/event_schedule/recur_changes_limit_type_to_until",
                   test_recur_changes_limit_type_to_until);
}
