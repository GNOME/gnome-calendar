/* test-server.c
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

#include <libsoup/soup.h>
#include <libedataserver/libedataserver.h>

#include "gcal-simple-server.h"

static GcalSimpleServer*
init_server (void)
{
  g_autoptr (GcalSimpleServer) server = NULL;

  server = gcal_simple_server_new ();
  gcal_simple_server_start (server);

  return g_steal_pointer (&server);
}

static gboolean
fail_authenticate_cb (SoupMessage *message,
                      SoupAuth    *auth,
                      gboolean     retrying,
                      gpointer     user_data)
{
  GcalSimpleServer *server = user_data;

  gcal_simple_server_stop (server);

  g_assert_not_reached ();

  return TRUE;
}

static gboolean
authenticate_cb (SoupMessage *message,
                 SoupAuth    *auth,
                 gboolean     retrying,
                 gpointer     user_data)
{
  g_debug ("Authenticating...");

  soup_auth_authenticate (auth, "unicorn", "iamnotahorse");

  return TRUE;
}

static gboolean
wrong_authenticate_cb (SoupMessage *message,
                       SoupAuth    *auth,
                       gboolean     retrying,
                       gpointer     user_data)
{
  g_debug ("Authenticating with wrong username...");

  if (retrying)
    return FALSE;

  soup_auth_authenticate (auth, "thyartismurder", "popmusic");

  return TRUE;
}

/*********************************************************************************************************************/

static void
server_init (void)
{
  g_autoptr (GcalSimpleServer) server = NULL;

  server = gcal_simple_server_new ();
  gcal_simple_server_start (server);
}

/*********************************************************************************************************************/

static void
server_request_no_auth_empty (void)
{
  g_autoptr (GcalSimpleServer) server = NULL;
  g_autoptr (SoupMessage) message = NULL;
  g_autoptr (SoupSession) session = NULL;
  g_autoptr (GInputStream) input_stream = NULL;
  g_autoptr (GUri) uri = NULL;
  g_autoptr (GError) error = NULL;

  server = init_server ();
  uri = gcal_simple_server_get_uri (server);

  session = gcal_create_soup_session ();

  message = soup_message_new_from_uri ("GET", uri);
  g_signal_connect (message, "authenticate", G_CALLBACK (fail_authenticate_cb), server);
  input_stream = soup_session_send (session, message, NULL, &error);
  g_assert_no_error (error);
  g_assert_nonnull (input_stream);
}

/*********************************************************************************************************************/

static void
server_request_no_auth_ics (void)
{
  g_autoptr (GcalSimpleServer) server = NULL;
  g_autoptr (SoupMessage) message = NULL;
  g_autoptr (SoupSession) session = NULL;
  g_autoptr (GInputStream) input_stream = NULL;
  g_autoptr (GUri) uri = NULL;
  g_autoptr (GError) error = NULL;

  server = init_server ();
  uri = gcal_simple_server_get_uri (server);
  e_util_change_uri_component (&uri, SOUP_URI_PATH, "/public/calendar.ics");

  session = gcal_create_soup_session ();

  message = soup_message_new_from_uri ("GET", uri);
  g_signal_connect (message, "authenticate", G_CALLBACK (fail_authenticate_cb), server);
  input_stream = soup_session_send (session, message, NULL, &error);
  g_assert_no_error (error);
  g_assert_nonnull (input_stream);
}

/*********************************************************************************************************************/

static void
server_request_no_auth_calendar (void)
{
  g_autoptr (GcalSimpleServer) server = NULL;
  g_autoptr (SoupMessage) message = NULL;
  g_autoptr (SoupSession) session = NULL;
  g_autoptr (GInputStream) input_stream = NULL;
  g_autoptr (GUri) uri = NULL;
  g_autoptr (GError) error = NULL;

  server = init_server ();
  uri = gcal_simple_server_get_uri (server);
  e_util_change_uri_component (&uri, SOUP_URI_PATH, "/public/calendar");

  session = gcal_create_soup_session ();

  message = soup_message_new_from_uri ("GET", uri);
  g_signal_connect (message, "authenticate", G_CALLBACK (fail_authenticate_cb), server);
  input_stream = soup_session_send (session, message, NULL, &error);
  g_assert_no_error (error);
  g_assert_nonnull (input_stream);
}

/*********************************************************************************************************************/

static void
server_request_auth_empty (void)
{
  g_autoptr (GcalSimpleServer) server = NULL;
  g_autoptr (SoupMessage) message = NULL;
  g_autoptr (SoupSession) session = NULL;
  g_autoptr (GInputStream) input_stream = NULL;
  g_autoptr (GUri) uri = NULL;
  g_autoptr (GError) error = NULL;

  server = init_server ();
  uri = gcal_simple_server_get_uri (server);
  e_util_change_uri_component (&uri, SOUP_URI_PATH, "/secret-area");

  session = gcal_create_soup_session ();

  message = soup_message_new_from_uri ("GET", uri);
  g_signal_connect (message, "authenticate", G_CALLBACK (authenticate_cb), server);
  input_stream = soup_session_send (session, message, NULL, &error);
  g_assert_no_error (error);
  g_assert_nonnull (input_stream);
}

/*********************************************************************************************************************/

static void
server_request_auth_calendar (void)
{
  g_autoptr (GcalSimpleServer) server = NULL;
  g_autoptr (SoupMessage) message = NULL;
  g_autoptr (SoupSession) session = NULL;
  g_autoptr (GInputStream) input_stream = NULL;
  g_autoptr (GUri) uri = NULL;
  g_autoptr (GError) error = NULL;

  server = init_server ();
  uri = gcal_simple_server_get_uri (server);
  e_util_change_uri_component (&uri, SOUP_URI_PATH, "/secret-area/calendar");

  session = gcal_create_soup_session ();

  message = soup_message_new_from_uri ("GET", uri);
  g_signal_connect (message, "authenticate", G_CALLBACK (authenticate_cb), server);
  input_stream = soup_session_send (session, message, NULL, &error);
  g_assert_no_error (error);
  g_assert_nonnull (input_stream);
}

/*********************************************************************************************************************/

static void
server_request_auth_ics (void)
{
  g_autoptr (GcalSimpleServer) server = NULL;
  g_autoptr (SoupMessage) message = NULL;
  g_autoptr (SoupSession) session = NULL;
  g_autoptr (GInputStream) input_stream = NULL;
  g_autoptr (GUri) uri = NULL;
  g_autoptr (GError) error = NULL;

  server = init_server ();
  uri = gcal_simple_server_get_uri (server);
  e_util_change_uri_component (&uri, SOUP_URI_PATH, "/secret-area/calendar.ics");

  session = gcal_create_soup_session ();

  message = soup_message_new_from_uri ("GET", uri);
  g_signal_connect (message, "authenticate", G_CALLBACK (authenticate_cb), server);
  input_stream = soup_session_send (session, message, NULL, &error);
  g_assert_no_error (error);
  g_assert_nonnull (input_stream);
}

/*********************************************************************************************************************/

static void
server_request_auth_wrong (void)
{
  g_autoptr (GcalSimpleServer) server = NULL;
  g_autoptr (SoupMessage) message = NULL;
  g_autoptr (SoupSession) session = NULL;
  g_autoptr (GInputStream) input_stream = NULL;
  g_autoptr (GUri) uri = NULL;
  g_autoptr (GError) error = NULL;

  server = init_server ();
  uri = gcal_simple_server_get_uri (server);
  e_util_change_uri_component (&uri, SOUP_URI_PATH, "/secret-area");

  session = gcal_create_soup_session ();

  message = soup_message_new_from_uri ("GET", uri);
  g_signal_connect (message, "authenticate", G_CALLBACK (wrong_authenticate_cb), server);

  input_stream = soup_session_send (session, message, NULL, &error);
  g_assert_no_error (error);
  g_assert_nonnull (input_stream);
  g_assert_cmpuint (soup_message_get_status (message), ==, SOUP_STATUS_UNAUTHORIZED);
}

/*********************************************************************************************************************/

gint
main (gint   argc,
      gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("https://gitlab.gnome.org/GNOME/gnome-calendar/-/issues/");

  g_test_add_func ("/server/init", server_init);
  g_test_add_func ("/server/no-auth/empty", server_request_no_auth_empty);
  g_test_add_func ("/server/no-auth/calendar", server_request_no_auth_calendar);
  g_test_add_func ("/server/no-auth/ics", server_request_no_auth_ics);
  g_test_add_func ("/server/auth/empty", server_request_auth_empty);
  g_test_add_func ("/server/auth/calendar", server_request_auth_calendar);
  g_test_add_func ("/server/auth/ics", server_request_auth_ics);
  g_test_add_func ("/server/auth/wrong", server_request_auth_wrong);

  return g_test_run ();
}
