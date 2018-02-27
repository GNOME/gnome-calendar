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

#include "gcal-utils.h"
#include "gcal-event.h"

#define STUB_EVENT "BEGIN:VEVENT \n"             \
                   "SUMMARY:Stub event \n"       \
                   "UID:example@uid \n"          \
                   "DTSTAMP:20180714T170000Z \n" \
                   "DTSTART:20180714T170000Z \n" \
                   "DTEND:20180715T035959Z \n"   \
                   "END:VEVENT \n"

/*********************************************************************************************************************/

static void
event_new (void)
{
  g_autoptr (ECalComponent) component = NULL;
  g_autoptr (GcalEvent) event = NULL;
  g_autoptr (ESource) source = NULL;
  g_autoptr (GError) error = NULL;

  component = e_cal_component_new_from_string (STUB_EVENT);
  source = e_source_new_with_uid ("stub", NULL, &error);

  g_assert_no_error (error);

  event = gcal_event_new (source, component, &error);

  g_assert_no_error (error);
  g_assert_nonnull (event);
}

/*********************************************************************************************************************/

gint
main (gint   argc,
      gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/event/new", event_new);

  return g_test_run ();
}

