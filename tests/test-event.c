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

#include "utils/gcal-utils.h"
#include "gcal-event.h"

#define STUB_EVENT "BEGIN:VEVENT\n"             \
                   "SUMMARY:Stub event\n"       \
                   "UID:example@uid\n"          \
                   "DTSTAMP:19970114T170000Z\n" \
                   "DTSTART:20180714T170000Z\n" \
                   "DTEND:20180715T035959Z\n"   \
                   "END:VEVENT\n"

#define EVENT_STRING_FOR_DATE(dtstart,dtend)    \
                   "BEGIN:VEVENT\n"             \
                   "SUMMARY:Stub event\n"       \
                   "UID:example@uid\n"          \
                   "DTSTAMP:19970114T170000Z\n" \
                   "DTSTART:"dtstart"\n"        \
                   "DTEND:"dtend"\n"            \
                   "END:VEVENT\n"

#define EVENT_STRING_FOR_DATE_START(dtstart)    \
                   EVENT_STRING_FOR_DATE(dtstart, "20180715T035959Z")

#define EVENT_STRING_FOR_DATE_END(dtend)        \
                   EVENT_STRING_FOR_DATE("19970101T170000Z", dtend)



/*
 * Auxiliary methods
 */

static GcalEvent*
create_event_for_string (const gchar  *string,
                         GError      **error)
{
  g_autoptr (ECalComponent) component = NULL;
  g_autoptr (ESource) source = NULL;

  source = e_source_new_with_uid ("stub", NULL, error);
  component = e_cal_component_new_from_string (string);

  return component ? gcal_event_new (source, component, error) : NULL;
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
event_date_start (void)
{
  struct
    {
      const gchar *string;
      GDateTime   *datetime;
    }
  start_dates[] = {
    { EVENT_STRING_FOR_DATE_START ("20160229T000000Z"), g_date_time_new_utc (2016, 2, 29, 00, 00, 00.) },
    { EVENT_STRING_FOR_DATE_START ("20180228T170000Z"), g_date_time_new_utc (2018, 2, 28, 17, 00, 00.) },
    { EVENT_STRING_FOR_DATE_START ("20180714T170000Z"), g_date_time_new_utc (2018, 7, 14, 17, 00, 00.) },
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
    { EVENT_STRING_FOR_DATE_END ("20160229T000000Z"), g_date_time_new_utc (2016, 2, 29, 00, 00, 00.) },
    { EVENT_STRING_FOR_DATE_END ("20180228T170000Z"), g_date_time_new_utc (2018, 2, 28, 17, 00, 00.) },
    { EVENT_STRING_FOR_DATE_END ("20180714T170000Z"), g_date_time_new_utc (2018, 7, 14, 17, 00, 00.) },
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
    EVENT_STRING_FOR_DATE ("20160229T000000Z", "20160229T030000Z"),
    EVENT_STRING_FOR_DATE ("20160229T000000Z", "20160301T000000Z"),
    EVENT_STRING_FOR_DATE ("20160229T000000Z", "20160229T235959Z"),
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
    EVENT_STRING_FOR_DATE ("20160229T000000Z", "20160302T000001Z"),
    EVENT_STRING_FOR_DATE ("20160229T020000Z", "20160310T004500Z"),
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

gint
main (gint   argc,
      gchar *argv[])
{
  g_setenv ("TZ", "UTC", TRUE);

  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/event/new", event_new);
  g_test_add_func ("/event/uid", event_uid);
  g_test_add_func ("/event/summary", event_summary);
  g_test_add_func ("/event/date/start", event_date_start);
  g_test_add_func ("/event/date/end", event_date_end);
  g_test_add_func ("/event/date/singleday", event_date_singleday);
  g_test_add_func ("/event/date/multiday", event_date_multiday);

  return g_test_run ();
}

