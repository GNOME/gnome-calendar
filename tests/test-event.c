/* test-event.c
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

#include <glib.h>

#include "gcal-event.h"
#include "gcal-stub-calendar.h"
#include "gcal-utils.h"

#define STUB_EVENT "BEGIN:VEVENT\n"             \
                   "SUMMARY:Stub event\n"       \
                   "UID:example@uid\n"          \
                   "DTSTAMP:19970114T170000Z\n" \
                   "DTSTART:20180714T170000Z\n" \
                   "DTEND:20180715T035959Z\n"   \
                   "END:VEVENT\n"

#define STUB_EVENT_DTEND "BEGIN:VEVENT\n"                \
                         "SUMMARY:Stub all day\n"        \
                         "UID:example@uid\n"             \
                         "DTSTART;VALUE=DATE:20231108\n" \
                         "DTEND;VALUE=DATE:20231109\n"   \
                         "END:VEVENT\n"

#define STUB_EVENT_NODTEND "BEGIN:VEVENT\n"                \
                           "SUMMARY:Stub all day\n"        \
                           "UID:example@uid\n"             \
                           "DTSTART;VALUE=DATE:20231108\n" \
                           "END:VEVENT\n"

#define EVENT_STRING_FOR_DATE(dtstart,dtend)    \
                   "BEGIN:VEVENT\n"             \
                   "SUMMARY:Stub event\n"       \
                   "UID:example@uid\n"          \
                   "DTSTAMP:19970114T170000Z\n" \
                   "DTSTART"dtstart"\n"        \
                   "DTEND"dtend"\n"            \
                   "END:VEVENT\n"

#define EVENT_STRING_FOR_DATE_START(dtstart)    \
                   EVENT_STRING_FOR_DATE(dtstart, ":20180715T035959Z")

#define EVENT_STRING_FOR_DATE_END(dtend)        \
                   EVENT_STRING_FOR_DATE(":19970101T170000Z", dtend)



/*
 * Auxiliary methods
 */

static GcalEvent*
create_event_for_string (const gchar  *string,
                         GError      **error)
{
  g_autoptr (ECalComponent) component = NULL;
  g_autoptr (GcalCalendar) calendar = NULL;

  component = e_cal_component_new_from_string (string);
  calendar = gcal_stub_calendar_new (NULL, error);

  return component ? gcal_event_new (calendar, component, error) : NULL;
}


/*********************************************************************************************************************/

static void
event_new (void)
{
  g_autoptr (GcalEvent) event = NULL;
  g_autoptr (GError) error = NULL;

  event = create_event_for_string (STUB_EVENT, &error);

  g_assert_no_error (error);
  g_assert_nonnull (event);
}

/*********************************************************************************************************************/

static void
event_clone (void)
{
  g_autoptr (GcalEvent) clone1 = NULL;
  g_autoptr (GcalEvent) clone2 = NULL;
  g_autoptr (GcalEvent) event = NULL;
  g_autoptr (GError) error = NULL;

  event = create_event_for_string (STUB_EVENT, &error);

  g_assert_no_error (error);
  g_assert_nonnull (event);

  clone1 = gcal_event_new_from_event (event);
  g_assert_nonnull (clone1);

  gcal_event_set_summary (event, "Another summary");

  clone2 = gcal_event_new_from_event (event);
  g_assert_nonnull (clone2);
}


/*********************************************************************************************************************/

static void
event_uid (void)
{
  g_autoptr (GcalEvent) event = NULL;

  event = create_event_for_string (STUB_EVENT, NULL);

  g_assert_cmpstr (gcal_event_get_uid (event), ==, "stub:example@uid");
}

/*********************************************************************************************************************/

static void
event_summary (void)
{
  g_autoptr (GcalEvent) event = NULL;

  event = create_event_for_string (STUB_EVENT, NULL);

  g_assert_cmpstr (gcal_event_get_summary (event), ==, "Stub event");
}

/*********************************************************************************************************************/

static void
event_no_dtend (void)
{
  g_autoptr (GcalEvent) event_dtend = NULL;
  g_autoptr (GcalEvent) event_nodtend = NULL;
  GDateTime *end_dtend = NULL;
  GDateTime *end_nodtend = NULL;

  g_test_bug ("327");

  event_dtend = create_event_for_string (STUB_EVENT_DTEND, NULL);
  event_nodtend = create_event_for_string (STUB_EVENT_NODTEND, NULL);
  end_dtend = gcal_event_get_date_end (event_dtend);
  end_nodtend = gcal_event_get_date_end (event_nodtend);

  // assert event with DTEND is working correctly
  g_assert_true (gcal_event_get_all_day (event_dtend));
  g_assert_true (end_dtend != NULL);

  // assert event without DTEND is working correctly, has a date_end assigned
  // for all day
  g_assert_true (gcal_event_get_all_day (event_nodtend));
  g_assert_true (end_nodtend != NULL);

  // The event without dtend should be all day and should have the same
  // date_end than the event that explicitly specified it.
  g_assert_true (g_date_time_equal (end_dtend, end_nodtend));
}

/*********************************************************************************************************************/

static void
event_date_start (void)
{
  struct
    {
      const gchar *string;
      GDateTime   *datetime;
    }
  start_dates[] = {
    { EVENT_STRING_FOR_DATE_START (":20160229T000000Z"), g_date_time_new_utc (2016, 2, 29, 00, 00, 00.) },
    { EVENT_STRING_FOR_DATE_START (":20180228T170000Z"), g_date_time_new_utc (2018, 2, 28, 17, 00, 00.) },
    { EVENT_STRING_FOR_DATE_START (":20180714T170000Z"), g_date_time_new_utc (2018, 7, 14, 17, 00, 00.) },
  };

  guint i;

  for (i = 0; i < G_N_ELEMENTS (start_dates); i++)
    {
      g_autoptr (GDateTime) dtstart = NULL;
      g_autoptr (GcalEvent) event = NULL;
      g_autoptr (GError) error = NULL;

      event = create_event_for_string (start_dates[i].string, &error);
      dtstart = start_dates[i].datetime;

      g_assert_no_error (error);
      g_assert_true (g_date_time_equal (gcal_event_get_date_start (event), dtstart));
    }
}

/*********************************************************************************************************************/

static void
event_date_end (void)
{
  struct
    {
      const gchar *string;
      GDateTime   *datetime;
    }
  end_dates[] = {
    { EVENT_STRING_FOR_DATE_END (":20160229T000000Z"), g_date_time_new_utc (2016, 2, 29, 00, 00, 00.) },
    { EVENT_STRING_FOR_DATE_END (":20180228T170000Z"), g_date_time_new_utc (2018, 2, 28, 17, 00, 00.) },
    { EVENT_STRING_FOR_DATE_END (":20180714T170000Z"), g_date_time_new_utc (2018, 7, 14, 17, 00, 00.) },
  };

  guint i;

  for (i = 0; i < G_N_ELEMENTS (end_dates); i++)
    {
      g_autoptr (GDateTime) dtend = NULL;
      g_autoptr (GcalEvent) event = NULL;
      g_autoptr (GError) error = NULL;

      event = create_event_for_string (end_dates[i].string, &error);
      dtend = end_dates[i].datetime;

      g_assert_no_error (error);
      g_assert_true (g_date_time_equal (gcal_event_get_date_end (event), dtend));
    }
}

/*********************************************************************************************************************/

static void
event_date_singleday (void)
{
  const gchar * const events[] = {
    EVENT_STRING_FOR_DATE (":20160229T000000Z", ":20160229T030000Z"),
    EVENT_STRING_FOR_DATE (":20160229T000000Z", ":20160301T000000Z"),
    EVENT_STRING_FOR_DATE (":20160229T000000Z", ":20160229T235959Z"),
  };

  guint i;

  for (i = 0; i < G_N_ELEMENTS (events); i++)
    {
      g_autoptr (GcalEvent) event = NULL;
      g_autoptr (GError) error = NULL;

      event = create_event_for_string (events[i], &error);

      g_assert_no_error (error);
      g_assert_false (gcal_event_is_multiday (event));
    }
}

/*********************************************************************************************************************/

static void
event_date_multiday (void)
{
  const gchar * const events[] = {
    EVENT_STRING_FOR_DATE (":20160229T000000Z", ":20160302T000001Z"),
    EVENT_STRING_FOR_DATE (":20160229T020000Z", ":20160310T004500Z"),
  };

  guint i;

  for (i = 0; i < G_N_ELEMENTS (events); i++)
    {
      g_autoptr (GcalEvent) event = NULL;
      g_autoptr (GError) error = NULL;

      event = create_event_for_string (events[i], &error);

      g_assert_no_error (error);
      g_assert_true (gcal_event_is_multiday (event));
    }
}

/*********************************************************************************************************************/

static void
event_date_create_tzid (void)
{
  const gchar *event_str = EVENT_STRING_FOR_DATE (";TZID=Europe/London:20170818T010000", ";TZID=Europe/London:20170818T020000");

  g_autoptr (GcalEvent) event = NULL;
  g_autoptr (GError) error = NULL;
  g_autoptr (GDateTime) datetime = NULL;
  g_autoptr (GTimeZone) tz = NULL;
  g_autofree gchar *ical_string = NULL;

  ECalComponent *comp = NULL;
  ICalComponent *ical_comp = NULL;
  ECalComponentDateTime *dt_end = NULL;

  g_test_bug ("172");

  event = create_event_for_string (event_str, &error);
  g_assert_no_error (error);

  comp = gcal_event_get_component (event);
  dt_end = e_cal_component_get_dtend (comp);
  g_assert_cmpstr (e_cal_component_datetime_get_tzid (dt_end), ==, "Europe/London");
  e_cal_component_datetime_free (dt_end);

  ical_comp = e_cal_component_get_icalcomponent (comp);

  ical_string = i_cal_component_as_ical_string (ical_comp);
  g_assert_true (strstr (ical_string, "DTSTART;TZID=Europe/London:20170818T010000\r\n") != NULL);
  g_assert_true (strstr (ical_string, "DTEND;TZID=Europe/London:20170818T020000\r\n") != NULL);

  // Edit the event to check that the modification is stored correctly
  g_setenv ("TZ", "Europe/London", TRUE);

  tz = g_time_zone_new_identifier ("Europe/London");
  datetime = g_date_time_new (tz, 2017, 8, 18, 3, 00, 00.);
  gcal_event_set_date_end (event, datetime);

  ical_string = i_cal_component_as_ical_string (ical_comp);
  g_assert_true (strstr (ical_string, "DTSTART;TZID=Europe/London:20170818T010000\r\n") != NULL);
  g_assert_true (strstr (ical_string, "DTEND;TZID=/freeassociation.sourceforge.net/Europe/London:\r\n 20170818T030000\r\n") != NULL);

  g_setenv ("TZ", "UTC", TRUE);
}

/*********************************************************************************************************************/

static void
event_date_edit_tzid (void)
{
  struct {
    const gchar *string;
    const gchar *tz;
  } events[] = {
    {
      EVENT_STRING_FOR_DATE (";TZID=Europe/Madrid:20170818T130000", ";TZID=Europe/Madrid:20170818T140000"),
      "Europe/Madrid"
    },
    {
      // https://gitlab.gnome.org/GNOME/gnome-calendar/-/issues/170
      EVENT_STRING_FOR_DATE (";TZID=Europe/London:20170818T130000", ";TZID=Europe/London:20170818T140000"),
      "Europe/London"
    },
    {
      EVENT_STRING_FOR_DATE (";TZID=America/Costa_Rica:20170818T130000", ";TZID=America/Costa_Rica:20170818T140000"),
      "America/Costa_Rica"
    },
  };

  g_test_bug ("170");

  for (size_t i = 0; i < G_N_ELEMENTS (events); i++)
    {
      g_autoptr (GDateTime) datetime = NULL;
      g_autoptr (GTimeZone) timezone = NULL;
      g_autoptr (GcalEvent) event = NULL;
      g_autoptr (GError) error = NULL;

      ECalComponentDateTime *dt_end = NULL;

      event = create_event_for_string (events[i].string, &error);
      g_assert_no_error (error);

      // modify the event and check the tz in the result
      timezone = g_time_zone_new_identifier (events[i].tz);
      datetime = g_date_time_new (timezone, 2017, 8, 18, 15, 00, 00.);
      gcal_event_set_date_end (event, datetime);

      dt_end = e_cal_component_get_dtend (gcal_event_get_component (event));
      g_assert_cmpstr (e_cal_component_datetime_get_tzid (dt_end), ==, "UTC");
      e_cal_component_datetime_free (dt_end);
    }
}

/*********************************************************************************************************************/

gint
main (gint   argc,
      gchar *argv[])
{
  g_setenv ("TZ", "UTC", TRUE);

  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("https://gitlab.gnome.org/GNOME/gnome-calendar/-/issues/");

  g_test_add_func ("/event/new", event_new);
  g_test_add_func ("/event/clone", event_clone);
  g_test_add_func ("/event/uid", event_uid);
  g_test_add_func ("/event/summary", event_summary);
  g_test_add_func ("/event/nodtend", event_no_dtend);
  g_test_add_func ("/event/date/start", event_date_start);
  g_test_add_func ("/event/date/end", event_date_end);
  g_test_add_func ("/event/date/singleday", event_date_singleday);
  g_test_add_func ("/event/date/multiday", event_date_multiday);
  g_test_add_func ("/event/date/edit-tzid", event_date_edit_tzid);
  g_test_add_func ("/event/date/create-tzid", event_date_create_tzid);

  return g_test_run ();
}
