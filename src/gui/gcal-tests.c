#include <glib.h>

#include "gcal-tests.h"
#include "event-editor/gcal-event-schedule.h"
#include "event-editor/gcal-schedule-section.h"

/* Adds all the tests for the internals of the GUI.
 *
 * To avoid exporting lots of little test functions from each source file, add a single
 * gcal_foo_add_tests() that in turn adds unit tests for that file.
 */
void
gcal_tests_add_internals (void)
{
  gcal_event_schedule_add_tests ();
  gcal_schedule_section_add_tests ();
}
