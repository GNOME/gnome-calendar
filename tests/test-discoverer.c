/* test-discoverer.c
 *
 * Copyright 2019 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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
#include <libecal/libecal.h>

#include "gcal-simple-server.h"
#include "gcal-source-discoverer.h"

static GcalSimpleServer*
init_server (void)
{
  g_autoptr (GcalSimpleServer) server = NULL;

  server = gcal_simple_server_new ();
  gcal_simple_server_start (server);

  return g_steal_pointer (&server);
}

/*********************************************************************************************************************/

static void
discovered_file_cb (GObject      *source_object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  g_autoptr (GPtrArray) sources = NULL;
  g_autoptr (GError) error = NULL;
  GMainLoop *mainloop = user_data;

  sources = gcal_discover_sources_from_uri_finish (result, &error);
  g_assert_no_error (error);
  g_assert_cmpuint (sources->len, ==, 1);

  g_main_loop_quit (mainloop);
}

static void
discoverer_file (void)
{
  g_autoptr (GcalSimpleServer) server = NULL;
  g_autoptr (GMainLoop) mainloop = NULL;
  g_autoptr (SoupURI) uri = NULL;
  g_autofree gchar *uri_str = NULL;

  server = init_server ();
  uri = gcal_simple_server_get_uri (server);
  soup_uri_set_path (uri, "/public/calendar");

  uri_str = soup_uri_to_string (uri, FALSE);

  mainloop = g_main_loop_new (NULL, FALSE);

  gcal_discover_sources_from_uri (uri_str,
                                  NULL,
                                  NULL,
                                  NULL,
                                  discovered_file_cb,
                                  mainloop);

  g_main_loop_run (mainloop);
}

/*********************************************************************************************************************/

static void
discovered_webdav_unauthorized_cb (GObject      *source_object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  g_autoptr (GPtrArray) sources = NULL;
  g_autoptr (GError) error = NULL;
  GMainLoop *mainloop = user_data;

  sources = gcal_discover_sources_from_uri_finish (result, &error);
  g_assert_error (error, GCAL_SOURCE_DISCOVERER_ERROR, GCAL_SOURCE_DISCOVERER_ERROR_UNAUTHORIZED);

  g_main_loop_quit (mainloop);
}

static void
discoverer_webdav_unauthorized (void)
{
  g_autoptr (GcalSimpleServer) server = NULL;
  g_autoptr (GMainLoop) mainloop = NULL;
  g_autoptr (SoupURI) uri = NULL;
  g_autofree gchar *uri_str = NULL;

  server = init_server ();
  uri = gcal_simple_server_get_uri (server);
  soup_uri_set_path (uri, "/secret-area/calendar");

  uri_str = soup_uri_to_string (uri, FALSE);

  mainloop = g_main_loop_new (NULL, FALSE);

  gcal_discover_sources_from_uri (uri_str,
                                  NULL,
                                  NULL,
                                  NULL,
                                  discovered_webdav_unauthorized_cb,
                                  mainloop);

  g_main_loop_run (mainloop);
}

/*********************************************************************************************************************/

#if 0

// TODO: Implement raw CalDAV server in GcalSimpleServer

static void
discoverer_webdav_auth_cb (GObject      *source_object,
                           GAsyncResult *result,
                           gpointer      user_data)
{
  g_autoptr (GPtrArray) sources = NULL;
  g_autoptr (GError) error = NULL;
  GMainLoop *mainloop = user_data;

  sources = gcal_discover_sources_from_uri_finish (result, &error);
  g_assert_no_error (error);

  g_main_loop_quit (mainloop);
}

static void
discoverer_webdav_auth (void)
{
  g_autoptr (GcalSimpleServer) server = NULL;
  g_autoptr (GMainLoop) mainloop = NULL;
  g_autoptr (SoupURI) uri = NULL;
  g_autofree gchar *uri_str = NULL;

  server = init_server ();
  uri = gcal_simple_server_get_uri (server);
  soup_uri_set_path (uri, "/secret-area/dav");

  uri_str = soup_uri_to_string (uri, FALSE);

  mainloop = g_main_loop_new (NULL, FALSE);

  gcal_discover_sources_from_uri (uri_str,
                                  "feaneron",
                                  "idonotmaintainanything",
                                  NULL,
                                  discoverer_webdav_auth_cb,
                                  mainloop);

  g_main_loop_run (mainloop);
}

#endif

/*********************************************************************************************************************/

gint
main (gint   argc,
      gchar *argv[])
{
  g_setenv ("TZ", "UTC", TRUE);

  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/discoverer/file", discoverer_file);
  g_test_add_func ("/discoverer/webdav/unauthorized", discoverer_webdav_unauthorized);

  return g_test_run ();
}

