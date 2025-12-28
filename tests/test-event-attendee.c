/* test-event-attendee.c
 *
 * Copyright (C) 2025 Alen Galinec <mind.on.warp@gmail.com>
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
#include <libecal/libecal.h>

#include "gcal-event-attendee.h"

/***************************************************************************/

#define STUB_URI "mailto:mind.on.warp@gmail.com"
#define STUB_MEMBER "mailto:mind.on.warp@gmail.com"
#define STUB_CUTYPE I_CAL_CUTYPE_INDIVIDUAL
#define STUB_ROLE I_CAL_ROLE_REQPARTICIPANT
#define STUB_PARTSTAT I_CAL_PARTSTAT_NEEDSACTION
#define STUB_RSVP TRUE
#define STUB_DELFROM "mailto:boss@gnome.calendar.org"
#define STUB_DELTO "mailto:other-boss@gnome.calendar.org"
#define STUB_SENTBY "mailto:whoami@web.de"
#define STUB_CN "Bugs Bunny"
#define STUB_LANGUAGE "Denglisch"

/**
 * Creates a stub ECalComponentAttendee
 *
 * Returns: an #ECalComponentAttendee stub
 */
ECalComponentAttendee *
stub_ecal_attendee (void)
{
  ECalComponentAttendee *ecal_attendee = NULL;

  ecal_attendee = e_cal_component_attendee_new_full (STUB_URI,
                                                     STUB_MEMBER,
                                                     STUB_CUTYPE,
                                                     STUB_ROLE,
                                                     STUB_PARTSTAT,
                                                     STUB_RSVP,
                                                     STUB_DELFROM,
                                                     STUB_DELTO,
                                                     STUB_SENTBY,
                                                     STUB_CN,
                                                     STUB_LANGUAGE);

  return ecal_attendee;
}

/***************************************************************************/

static void
event_attendee_new (void)
{
  ECalComponentAttendee *ecal_attendee = NULL;
  g_autoptr (GcalEventAttendee) our_attendee = NULL;

  ecal_attendee = stub_ecal_attendee ();
  our_attendee = gcal_event_attendee_new (ecal_attendee);

  g_assert_nonnull (our_attendee);
  g_assert_cmpstr (STUB_URI, ==, gcal_event_attendee_get_uri (our_attendee));
  g_assert_cmpstr (STUB_MEMBER, ==, gcal_event_attendee_get_member (our_attendee));
  g_assert_cmpuint (GCAL_EVENT_ATTENDEE_TYPE_INDIVIDUAL, ==, gcal_event_attendee_get_attendee_type (our_attendee));
  g_assert_cmpuint (GCAL_EVENT_ATTENDEE_ROLE_REQUIRED, ==, gcal_event_attendee_get_role (our_attendee));
  g_assert_cmpuint (GCAL_EVENT_ATTENDEE_PART_NEEDS_ACTION, ==, gcal_event_attendee_get_part_status (our_attendee));
  g_assert (STUB_RSVP == gcal_event_attendee_get_requires_rsvp (our_attendee));
  g_assert_cmpstr (STUB_DELFROM, ==, gcal_event_attendee_get_delegated_from (our_attendee));
  g_assert_cmpstr (STUB_DELTO, ==, gcal_event_attendee_get_delegated_to (our_attendee));
  g_assert_cmpstr (STUB_SENTBY, ==, gcal_event_attendee_get_sent_by (our_attendee));
  g_assert_cmpstr (STUB_CN, ==, gcal_event_attendee_get_name (our_attendee));
  g_assert_cmpstr (STUB_LANGUAGE, ==, gcal_event_attendee_get_language (our_attendee));

  e_cal_component_attendee_free (ecal_attendee);
}

/***************************************************************************/

static void
event_attendee_cutype_tests (void)
{
  ECalComponentAttendee *ecal_attendee = NULL;

  ecal_attendee = stub_ecal_attendee ();

  /* X -> NONE */
  {
    g_autoptr (GcalEventAttendee) our_attendee = NULL;
    e_cal_component_attendee_set_cutype (ecal_attendee, I_CAL_CUTYPE_X);
    our_attendee = gcal_event_attendee_new (ecal_attendee);
    g_assert_cmpuint (GCAL_EVENT_ATTENDEE_TYPE_NONE, ==, gcal_event_attendee_get_attendee_type (our_attendee));
  }

  /* INDIVIDUAL -> INDIVIDUAL */
  {
    g_autoptr (GcalEventAttendee) our_attendee = NULL;
    e_cal_component_attendee_set_cutype (ecal_attendee, I_CAL_CUTYPE_INDIVIDUAL);
    our_attendee = gcal_event_attendee_new (ecal_attendee);
    g_assert_cmpuint (GCAL_EVENT_ATTENDEE_TYPE_INDIVIDUAL, ==, gcal_event_attendee_get_attendee_type (our_attendee));
  }

  /* GROUP -> GROUP */
  {
    g_autoptr (GcalEventAttendee) our_attendee = NULL;
    e_cal_component_attendee_set_cutype (ecal_attendee, I_CAL_CUTYPE_GROUP);
    our_attendee = gcal_event_attendee_new (ecal_attendee);
    g_assert_cmpuint (GCAL_EVENT_ATTENDEE_TYPE_GROUP, ==, gcal_event_attendee_get_attendee_type (our_attendee));
  }

  /* RESOURCE -> RESOURCE */
  {
    g_autoptr (GcalEventAttendee) our_attendee = NULL;
    e_cal_component_attendee_set_cutype (ecal_attendee, I_CAL_CUTYPE_RESOURCE);
    our_attendee = gcal_event_attendee_new (ecal_attendee);
    g_assert_cmpuint (GCAL_EVENT_ATTENDEE_TYPE_RESOURCE, ==, gcal_event_attendee_get_attendee_type (our_attendee));
  }

  /* ROOM -> ROOM */
  {
    g_autoptr (GcalEventAttendee) our_attendee = NULL;
    e_cal_component_attendee_set_cutype (ecal_attendee, I_CAL_CUTYPE_ROOM);
    our_attendee = gcal_event_attendee_new (ecal_attendee);
    g_assert_cmpuint (GCAL_EVENT_ATTENDEE_TYPE_ROOM, ==, gcal_event_attendee_get_attendee_type (our_attendee));
  }

  /* UNKNOWN -> UNKNOWN */
  {
    g_autoptr (GcalEventAttendee) our_attendee = NULL;
    e_cal_component_attendee_set_cutype (ecal_attendee, I_CAL_CUTYPE_UNKNOWN);
    our_attendee = gcal_event_attendee_new (ecal_attendee);
    g_assert_cmpuint (GCAL_EVENT_ATTENDEE_TYPE_UNKNOWN, ==, gcal_event_attendee_get_attendee_type (our_attendee));
  }

  /* NONE -> NONE */
  {
    g_autoptr (GcalEventAttendee) our_attendee = NULL;
    e_cal_component_attendee_set_cutype (ecal_attendee, I_CAL_CUTYPE_NONE);
    our_attendee = gcal_event_attendee_new (ecal_attendee);
    g_assert_cmpuint (GCAL_EVENT_ATTENDEE_TYPE_NONE, ==, gcal_event_attendee_get_attendee_type (our_attendee));
  }

  e_cal_component_attendee_free (ecal_attendee);
}

/***************************************************************************/

static void
event_attendee_participation_tests (void)
{
  ECalComponentAttendee *ecal_attendee = NULL;

  ecal_attendee = stub_ecal_attendee ();

  /* X -> NONE */
  {
    g_autoptr (GcalEventAttendee) our_attendee = NULL;
    e_cal_component_attendee_set_partstat (ecal_attendee, I_CAL_PARTSTAT_X);
    our_attendee = gcal_event_attendee_new (ecal_attendee);
    g_assert_cmpuint (GCAL_EVENT_ATTENDEE_PART_NONE, ==, gcal_event_attendee_get_part_status (our_attendee));
  }

  /* NEEDSACTION -> NEEDSACTION */
  {
    g_autoptr (GcalEventAttendee) our_attendee = NULL;
    e_cal_component_attendee_set_partstat (ecal_attendee, I_CAL_PARTSTAT_NEEDSACTION);
    our_attendee = gcal_event_attendee_new (ecal_attendee);
    g_assert_cmpuint (GCAL_EVENT_ATTENDEE_PART_NEEDS_ACTION, ==, gcal_event_attendee_get_part_status (our_attendee));
  }

  /* ACCEPTED -> ACCEPTED */
  {
    g_autoptr (GcalEventAttendee) our_attendee = NULL;
    e_cal_component_attendee_set_partstat (ecal_attendee, I_CAL_PARTSTAT_ACCEPTED);
    our_attendee = gcal_event_attendee_new (ecal_attendee);
    g_assert_cmpuint (GCAL_EVENT_ATTENDEE_PART_ACCEPTED, ==, gcal_event_attendee_get_part_status (our_attendee));
  }

  /* DECLINED -> DECLINED */
  {
    g_autoptr (GcalEventAttendee) our_attendee = NULL;
    e_cal_component_attendee_set_partstat (ecal_attendee, I_CAL_PARTSTAT_DECLINED);
    our_attendee = gcal_event_attendee_new (ecal_attendee);
    g_assert_cmpuint (GCAL_EVENT_ATTENDEE_PART_DECLINED, ==, gcal_event_attendee_get_part_status (our_attendee));
  }

  /* TENTATIVE -> TENTATIVE */
  {
    g_autoptr (GcalEventAttendee) our_attendee = NULL;
    e_cal_component_attendee_set_partstat (ecal_attendee, I_CAL_PARTSTAT_TENTATIVE);
    our_attendee = gcal_event_attendee_new (ecal_attendee);
    g_assert_cmpuint (GCAL_EVENT_ATTENDEE_PART_TENTATIVE, ==, gcal_event_attendee_get_part_status (our_attendee));
  }

  /* DELEGATED -> DELEGATED */
  {
    g_autoptr (GcalEventAttendee) our_attendee = NULL;
    e_cal_component_attendee_set_partstat (ecal_attendee, I_CAL_PARTSTAT_DELEGATED);
    our_attendee = gcal_event_attendee_new (ecal_attendee);
    g_assert_cmpuint (GCAL_EVENT_ATTENDEE_PART_DELEGATED, ==, gcal_event_attendee_get_part_status (our_attendee));
  }

  /* COMPLETED -> COMPLETED */
  {
    g_autoptr (GcalEventAttendee) our_attendee = NULL;
    e_cal_component_attendee_set_partstat (ecal_attendee, I_CAL_PARTSTAT_COMPLETED);
    our_attendee = gcal_event_attendee_new (ecal_attendee);
    g_assert_cmpuint (GCAL_EVENT_ATTENDEE_PART_COMPLETED, ==, gcal_event_attendee_get_part_status (our_attendee));
  }

  /* INPROCESS -> INPROCESS */
  {
    g_autoptr (GcalEventAttendee) our_attendee = NULL;
    e_cal_component_attendee_set_partstat (ecal_attendee, I_CAL_PARTSTAT_INPROCESS);
    our_attendee = gcal_event_attendee_new (ecal_attendee);
    g_assert_cmpuint (GCAL_EVENT_ATTENDEE_PART_INPROCESS, ==, gcal_event_attendee_get_part_status (our_attendee));
  }

  /* FAILED -> FAILED */
  {
    g_autoptr (GcalEventAttendee) our_attendee = NULL;
    e_cal_component_attendee_set_partstat (ecal_attendee, I_CAL_PARTSTAT_FAILED);
    our_attendee = gcal_event_attendee_new (ecal_attendee);
    g_assert_cmpuint (GCAL_EVENT_ATTENDEE_PART_FAILED, ==, gcal_event_attendee_get_part_status (our_attendee));
  }

  /* NONE -> NONE */
  {
    g_autoptr (GcalEventAttendee) our_attendee = NULL;
    e_cal_component_attendee_set_partstat (ecal_attendee, I_CAL_PARTSTAT_NONE);
    our_attendee = gcal_event_attendee_new (ecal_attendee);
    g_assert_cmpuint (GCAL_EVENT_ATTENDEE_PART_NONE, ==, gcal_event_attendee_get_part_status (our_attendee));
  }

  e_cal_component_attendee_free (ecal_attendee);
}

/***************************************************************************/

static void
event_attendee_role_tests (void)
{
  ECalComponentAttendee *ecal_attendee = NULL;

  ecal_attendee = stub_ecal_attendee ();

  /* X -> NONE */
  {
    g_autoptr (GcalEventAttendee) our_attendee = NULL;
    e_cal_component_attendee_set_role (ecal_attendee, I_CAL_ROLE_X);
    our_attendee = gcal_event_attendee_new (ecal_attendee);
    g_assert_cmpuint (GCAL_EVENT_ATTENDEE_ROLE_NONE, ==, gcal_event_attendee_get_role (our_attendee));
  }

  /* CHAIR -> CHAIR */
  {
    g_autoptr (GcalEventAttendee) our_attendee = NULL;
    e_cal_component_attendee_set_role (ecal_attendee, I_CAL_ROLE_CHAIR);
    our_attendee = gcal_event_attendee_new (ecal_attendee);
    g_assert_cmpuint (GCAL_EVENT_ATTENDEE_ROLE_CHAIR, ==, gcal_event_attendee_get_role (our_attendee));
  }

  /* REQPARTICIPANT -> REQUIRED */
  {
    g_autoptr (GcalEventAttendee) our_attendee = NULL;
    e_cal_component_attendee_set_role (ecal_attendee, I_CAL_ROLE_REQPARTICIPANT);
    our_attendee = gcal_event_attendee_new (ecal_attendee);
    g_assert_cmpuint (GCAL_EVENT_ATTENDEE_ROLE_REQUIRED, ==, gcal_event_attendee_get_role (our_attendee));
  }

  /* OPTPARTICIPANT -> OPTIONAL */
  {
    g_autoptr (GcalEventAttendee) our_attendee = NULL;
    e_cal_component_attendee_set_role (ecal_attendee, I_CAL_ROLE_OPTPARTICIPANT);
    our_attendee = gcal_event_attendee_new (ecal_attendee);
    g_assert_cmpuint (GCAL_EVENT_ATTENDEE_ROLE_OPTIONAL, ==, gcal_event_attendee_get_role (our_attendee));
  }

  /* NONPARTICIPANT -> NONPARTICIPANT */
  {
    g_autoptr (GcalEventAttendee) our_attendee = NULL;
    e_cal_component_attendee_set_role (ecal_attendee, I_CAL_ROLE_NONPARTICIPANT);
    our_attendee = gcal_event_attendee_new (ecal_attendee);
    g_assert_cmpuint (GCAL_EVENT_ATTENDEE_ROLE_NONPARTICIPANT, ==, gcal_event_attendee_get_role (our_attendee));
  }

  /* NONE -> NONE */
  {
    g_autoptr (GcalEventAttendee) our_attendee = NULL;
    e_cal_component_attendee_set_role (ecal_attendee, I_CAL_ROLE_NONE);
    our_attendee = gcal_event_attendee_new (ecal_attendee);
    g_assert_cmpuint (GCAL_EVENT_ATTENDEE_ROLE_NONE, ==, gcal_event_attendee_get_role (our_attendee));
  }

  e_cal_component_attendee_free (ecal_attendee);
}

/***************************************************************************/

gint
main (gint argc,
      gchar *argv[])
{
  g_setenv ("TZ", "UTC", TRUE);

  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("https://gitlab.gnome.org/GNOME/gnome-calendar/-/issues/");

  g_test_add_func ("/event-attendee/new", event_attendee_new);
  g_test_add_func ("/event-attendee/cutype-tests", event_attendee_cutype_tests);
  g_test_add_func ("/event-attendee/participation-tests", event_attendee_participation_tests);
  g_test_add_func ("/event-attendee/role-tests", event_attendee_role_tests);

  return g_test_run ();
}
