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

#include "gcal-simple-server.h"

static GcalSimpleServer*
init_server (void)
{
  g_autoptr (GcalSimpleServer) server = NULL;

  server = gcal_simple_server_new ();
  gcal_simple_server_start (server);

  return g_steal_pointer (&server);
}

static void
fail_authenticate_cb (SoupSession *session,
                      SoupMessage *message,
                      SoupAuth    *auth,
                      gboolean     retrying,
                      gpointer     user_data)
{
  GcalSimpleServer *server = user_data;

  gcal_simple_server_stop (server);

  g_assert_not_reached ();
}

static void
authenticate_cb (SoupSession *session,
                 SoupMessage *message,
                 SoupAuth    *auth,
                 gboolean     retrying,
                 gpointer     user_data)
{
  g_debug ("Authenticating...");

  soup_auth_authenticate (auth, "unicorn", "iamnotahorse");
}

static void
wrong_authenticate_cb (SoupSession *session,
                       SoupMessage *message,
                       SoupAuth    *auth,
                       gboolean     retrying,
                       gpointer     user_data)
{
  g_debug ("Authenticating with wrong username...");

  if (!retrying)
    soup_auth_authenticate (auth, "thyartismurder", "popmusic");
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
  g_autoptr (SoupURI) uri = NULL;
  g_autoptr (GError) error = NULL;

  server = init_server ();
  uri = gcal_simple_server_get_uri (server);

  session = soup_session_new ();
  g_signal_connect (session, "authenticate", G_CALLBACK (fail_authenticate_cb), server);

  message = soup_message_new_from_uri ("GET", uri);
  soup_session_send (session, message, NULL, &error);
  g_assert_no_error (error);
}

/*********************************************************************************************************************/

static void
server_request_no_auth_ics (void)
{
  g_autoptr (GcalSimpleServer) server = NULL;
  g_autoptr (SoupMessage) message = NULL;
  g_autoptr (SoupSession) session = NULL;
  g_autoptr (SoupURI) uri = NULL;
  g_autoptr (GError) error = NULL;

  server = init_server ();
  uri = gcal_simple_server_get_uri (server);
  soup_uri_set_path (uri, "/public/calendar.ics");

  session = soup_session_new ();
  g_signal_connect (session, "authenticate", G_CALLBACK (fail_authenticate_cb), server);

  message = soup_message_new_from_uri ("GET", uri);
  soup_session_send (session, message, NULL, &error);
  g_assert_no_error (error);
}

/*********************************************************************************************************************/

static void
server_request_no_auth_calendar (void)
{
  g_autoptr (GcalSimpleServer) server = NULL;
  g_autoptr (SoupMessage) message = NULL;
  g_autoptr (SoupSession) session = NULL;
  g_autoptr (SoupURI) uri = NULL;
  g_autoptr (GError) error = NULL;

  server = init_server ();
  uri = gcal_simple_server_get_uri (server);
  soup_uri_set_path (uri, "/public/calendar");

  session = soup_session_new ();
  g_signal_connect (session, "authenticate", G_CALLBACK (fail_authenticate_cb), server);

  message = soup_message_new_from_uri ("GET", uri);
  soup_session_send (session, message, NULL, &error);
  g_assert_no_error (error);
}

/*********************************************************************************************************************/

static void
server_request_auth_empty (void)
{
  g_autoptr (GcalSimpleServer) server = NULL;
  g_autoptr (SoupMessage) message = NULL;
  g_autoptr (SoupSession) session = NULL;
  g_autoptr (SoupURI) uri = NULL;
  g_autoptr (GError) error = NULL;

  server = init_server ();
  uri = gcal_simple_server_get_uri (server);
  soup_uri_set_path (uri, "/secret-area");

  session = soup_session_new ();
  g_signal_connect (session, "authenticate", G_CALLBACK (authenticate_cb), server);

  message = soup_message_new_from_uri ("GET", uri);
  soup_session_send (session, message, NULL, &error);
  g_assert_no_error (error);
}

/*********************************************************************************************************************/

static void
server_request_auth_calendar (void)
{
  g_autoptr (GcalSimpleServer) server = NULL;
  g_autoptr (SoupMessage) message = NULL;
  g_autoptr (SoupSession) session = NULL;
  g_autoptr (SoupURI) uri = NULL;
  g_autoptr (GError) error = NULL;

  server = init_server ();
  uri = gcal_simple_server_get_uri (server);
  soup_uri_set_path (uri, "/secret-area/calendar");

  session = soup_session_new ();
  g_signal_connect (session, "authenticate", G_CALLBACK (authenticate_cb), server);

  message = soup_message_new_from_uri ("GET", uri);
  soup_session_send (session, message, NULL, &error);
  g_assert_no_error (error);
}

/*********************************************************************************************************************/

static void
server_request_auth_ics (void)
{
  g_autoptr (GcalSimpleServer) server = NULL;
  g_autoptr (SoupMessage) message = NULL;
  g_autoptr (SoupSession) session = NULL;
  g_autoptr (SoupURI) uri = NULL;
  g_autoptr (GError) error = NULL;

  server = init_server ();
  uri = gcal_simple_server_get_uri (server);
  soup_uri_set_path (uri, "/secret-area/calendar.ics");

  session = soup_session_new ();
  g_signal_connect (session, "authenticate", G_CALLBACK (authenticate_cb), server);

  message = soup_message_new_from_uri ("GET", uri);
  soup_session_send (session, message, NULL, &error);
  g_assert_no_error (error);
}

/*********************************************************************************************************************/

static void
server_request_auth_wrong (void)
{
  g_autoptr (GcalSimpleServer) server = NULL;
  g_autoptr (SoupMessage) message = NULL;
  g_autoptr (SoupSession) session = NULL;
  g_autoptr (SoupURI) uri = NULL;
  g_autoptr (GError) error = NULL;

  server = init_server ();
  uri = gcal_simple_server_get_uri (server);
  soup_uri_set_path (uri, "/secret-area");

  session = soup_session_new ();
  g_signal_connect (session, "authenticate", G_CALLBACK (wrong_authenticate_cb), server);

  message = soup_message_new_from_uri ("GET", uri);
  soup_session_send (session, message, NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpuint (message->status_code, ==, SOUP_STATUS_UNAUTHORIZED);
}

/*********************************************************************************************************************/

gint
main (gint   argc,
      gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);

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
