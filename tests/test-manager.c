/* test-manager.c
 *
 * Copyright (C) 2017 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#include "gcal-manager.h"

/*********************************************************************************************************************/

static void
manager_new_with_settings (void)
{
  g_autoptr (GcalManager) manager;
  g_autoptr (GSettings) settings;

  settings = g_settings_new ("org.gnome.calendar");
  manager = gcal_manager_new_with_settings (settings);

  g_assert_nonnull (manager);
}

/*********************************************************************************************************************/

gint
main (gint   argc,
      gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/manager/new_with_settings", manager_new_with_settings);

  return g_test_run ();
}

