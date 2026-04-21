/* test-utils.c
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

#include "gcal-utils.h"

#define EVENT_STRING_FOR_DATE(dtstart,dtend) \
  "BEGIN:VEVENT\n"                           \
  "SUMMARY:Stub event\n"                     \
  "UID:example@uid\n"                        \
  "DTSTAMP:19970114T170000Z\n"               \
  "DTSTART"dtstart"\n"                       \
  "DTEND"dtend"\n"                           \
  "END:VEVENT\n"

#define STUB_CALENDAR_VTZ(dtstart,dtend)               \
  "BEGIN:VCALENDAR\n"                                  \
  "VERSION:2.0\n"                                      \
  "METHOD:PUBLISH\n"                                   \
  "BEGIN:VTIMEZONE\n"                                  \
  "TZID:StubTz\n"                                      \
  "BEGIN:STANDARD\n"                                   \
  "DTSTART:16011028T030000\n"                          \
  "RRULE:FREQ=YEARLY;BYDAY=-1SU;BYMONTH=10\n"          \
  "TZOFFSETFROM:+0200\n"                               \
  "TZOFFSETTO:+0100\n"                                 \
  "END:STANDARD\n"                                     \
  "BEGIN:DAYLIGHT\n"                                   \
  "DTSTART:16010325T020000\n"                          \
  "RRULE:FREQ=YEARLY;BYDAY=-1SU;BYMONTH=3\n"           \
  "TZOFFSETFROM:+0100\n"                               \
  "TZOFFSETTO:+0200\n"                                 \
  "END:DAYLIGHT\n"                                     \
  "END:VTIMEZONE\n"                                    \
  "BEGIN:VEVENT\n"                                     \
  "DTSTART;TZID=StubTz"dtstart"\n"                     \
  "DTEND;TZID=StubTz"dtend"\n"                         \
  "LOCATION:somewhere\n"                               \
  "SEQUENCE:0\n"                                       \
  "UID:389584207457478\n"                              \
  "DTSTAMP:20240608T184316Z\n"                         \
  "SUMMARY:Test time zone\n"                           \
  "CATEGORIES:\n"                                      \
  "END:VEVENT\n"                                       \
  "END:VCALENDAR\n"

/*********************************************************************************************************************/

static void
date_time_from_icaltime (void)
{
  struct
    {
      const gchar *string;
      const gchar *start;
      const gchar *end;
    }
  events[] = {
    {
      .string = EVENT_STRING_FOR_DATE (";TZID=America/New_York:20241209T170000",
                                       ";TZID=America/New_York:20241209T180000"),
      .start = "2024-12-09T17:00:00-05",
      .end = "2024-12-09T18:00:00-05",
    },
    {
      .string = EVENT_STRING_FOR_DATE (":20240709T170000Z", ":20240709T180000Z"),
      .start = "2024-07-09T17:00:00Z",
      .end = "2024-07-09T18:00:00Z",
    },
    {
      // Virtual time zone in standard time
      .string = STUB_CALENDAR_VTZ (":20241103T103000", ":20241103T114500"),
      .start = "2024-11-03T10:30:00+01",
      .end = "2024-11-03T11:45:00+01",
    },
    {
      // Virtual time zone with Daylight Saving Time
      .string = STUB_CALENDAR_VTZ (":20240603T103000", ":20240603T114500"),
      .start = "2024-06-03T10:30:00+02",
      .end = "2024-06-03T11:45:00+02",
    },
  };

  for (gsize i = 0; i < G_N_ELEMENTS (events); i++)
    {

      g_autoptr (GDateTime) gdt_start = NULL;
      g_autoptr (GDateTime) gdt_end = NULL;
      g_autoptr (ICalTime) dt_start = NULL;
      g_autoptr (ICalTime) dt_end = NULL;
      g_autofree gchar *start_str = NULL;
      g_autofree gchar *end_str = NULL;
      ICalComponent *comp = NULL;

      comp = i_cal_component_new_from_string (events[i].string);
      g_assert_nonnull (comp);

      dt_start = i_cal_component_get_dtstart (comp);
      g_assert_nonnull (dt_start);
      gdt_start = gcal_date_time_from_icaltime (dt_start);
      g_assert_nonnull (gdt_start);

      dt_end = i_cal_component_get_dtend (comp);
      g_assert_nonnull (dt_end);
      gdt_end = gcal_date_time_from_icaltime (dt_end);
      g_assert_nonnull (gdt_end);

      start_str = g_date_time_format_iso8601 (gdt_start);
      end_str = g_date_time_format_iso8601 (gdt_end);

      g_assert_cmpstr (start_str, ==, events[i].start);
      g_assert_cmpstr (end_str, ==, events[i].end);

      g_clear_object (&comp);
    }
}

/*********************************************************************************************************************/

#define GOOGLE_SAMPLE_DESCRIPTION \
"-::~:~::~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~::~:~::-\n" \
"Entrar com o Google Meet: https://meet.google.com/abc-defg-hij\n"                          \
"\n"                                                                                        \
"Saiba mais sobre o Meet em: https://support.google.com/a/users/answer/9282720\n"           \
"\n"                                                                                        \
"Não edite esta seção.\n"                                                                   \
"-::~:~::~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~::~:~::-"

#define GOOGLE_SAMPLE_DESCRIPTION_WITH_COMMENT \
"TROLOLO\n"                                                                                 \
"-::~:~::~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~::~:~::-\n" \
"Entrar com o Google Meet: https://meet.google.com/abc-defg-hij\n"                          \
"\n"                                                                                        \
"Saiba mais sobre o Meet em: https://support.google.com/a/users/answer/9282720\n"           \
"\n"                                                                                        \
"Não edite esta seção.\n"                                                                   \
"-::~:~::~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~:~::~:~::-"

#define TEAMS_SAMPLE_DESCRIPTION \
"________________________________________________________________________________\n" \
"Réunion Microsoft Teams\n" \
"Joindre : https://teams.microsoft.com/meet/12345678912345?p=0AbCdEfGjKLm1nOjTj\n" \
"Numéro de réunion : 123 456 789 123 45\n" \
"Code secret : abcdefgh\n" \
"________________________________\n" \
"Besoin d’aide?<https://aka.ms/JoinTeamsMeeting?omkt=fr-CA> | Référence système<https://teams.microsoft.com/l/meetup-join/19%3ameeting_KLMNTABC02UJKL5OC00ZWJhLThhNDgtYmJlYTI2ZTE5YzM0%40abc.v2/0?context=%7b%22Tid%22%3a%3f5f042f-920a-4035-8dc7-477b5dce3c73%22%2c%22Oid%22%3a%aa4dd47c-47a1-4a00-b766-6e5f83e7d367%22%7d>\n" \
"Pour les organisateurs : Options de réunion<https://teams.microsoft.com/meetingOptions/?organizerId=690add55-d0e3-4605-ac28-a3a08193c212&tenantId=43743467-968a-4ded-8250-afd003a4a407&threadId=19_meeting_Yjk5NTI0M2UtNjA5OC00ZWJhLThhNDgtYmJlYTI2ZTE5YzM0&messageId=100&language=fr-CA>\n" \
"________________________________________________________________________________"


static void
extract_meeting_url (void)
{
  g_autofree char *description = NULL;
  g_autofree char *meeting_url = NULL;

  gcal_utils_extract_meeting_url (GOOGLE_SAMPLE_DESCRIPTION, &description, &meeting_url);
  g_assert_cmpstr (description, ==, "");
  g_assert_cmpstr (meeting_url, ==, "https://meet.google.com/abc-defg-hij");

  g_clear_pointer (&description, g_free);
  g_clear_pointer (&meeting_url, g_free);

  gcal_utils_extract_meeting_url (GOOGLE_SAMPLE_DESCRIPTION_WITH_COMMENT, &description, &meeting_url);
  g_assert_cmpstr (description, ==, "TROLOLO\n");
  g_assert_cmpstr (meeting_url, ==, "https://meet.google.com/abc-defg-hij");

  g_clear_pointer (&description, g_free);
  g_clear_pointer (&meeting_url, g_free);

  gcal_utils_extract_meeting_url (TEAMS_SAMPLE_DESCRIPTION, &description, &meeting_url);
  g_assert_cmpstr (description, ==, "");
  g_assert_cmpstr (meeting_url, ==, "https://teams.microsoft.com/meet/12345678912345?p=0AbCdEfGjKLm1nOjTj");
}

/*********************************************************************************************************************/

gint
main (gint   argc,
      gchar *argv[])
{
  g_setenv ("TZ", "UTC", TRUE);

  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("https://gitlab.gnome.org/GNOME/gnome-calendar/-/issues/");

  g_test_add_func ("/utils/date-time/date_time_from_icaltime", date_time_from_icaltime);
  g_test_add_func ("/utils/misc/extract_meeting_url", extract_meeting_url);

  return g_test_run ();
}
