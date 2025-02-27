#include <glib.h>
#include <locale.h>

#include "gui/gcal-tests.h"

int
main (int argc, char **argv)
{
  g_setenv ("TZ", "UTC", TRUE);

  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("https://gitlab.gnome.org/GNOME/gnome-calendar/-/issues/");

  gcal_tests_add_internals ();

  return g_test_run ();
}
