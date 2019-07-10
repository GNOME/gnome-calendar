/* gcal-simple-server.c
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

#define G_LOG_DOMAIN "GcalSimpleServer"

#include "gcal-simple-server.h"

struct _GcalSimpleServer
{
  GObject             parent;

  /* Only valid while running */
  GThread            *thread;
  GMainLoop          *thread_mainloop;
  SoupServer         *server;
  gchar              *uri;

  GMutex              running_mutex;
  GCond               running_cond;
  gboolean            running;
};

G_DEFINE_TYPE (GcalSimpleServer, gcal_simple_server, G_TYPE_OBJECT)


/*
 * Auxiliary methods
 */

static void
process_get (SoupMessage *message,
             const gchar *prefix,
             const gchar *path)
{
  g_autofree gchar *calendar_path = g_strdup_printf ("%s/calendar", prefix);
  g_autofree gchar *calendar_file = g_strdup_printf ("%s/calendar.ics", prefix);

  if (g_strcmp0 (path, calendar_path) == 0)
    {
      g_debug ("Serving empty calendar");
      soup_message_set_response (message,
                                 "text/calendar",
                                 SOUP_MEMORY_STATIC,
                                 GCAL_TEST_SERVER_EMPTY_CALENDAR,
                                 strlen (GCAL_TEST_SERVER_EMPTY_CALENDAR));
    }

  if (g_strcmp0 (path, calendar_file) == 0)
    {
      g_autoptr (SoupBuffer) buffer = NULL;
      GMappedFile *mapping;

      g_debug ("Serving calendar.ics");

      mapping = g_mapped_file_new (path, FALSE, NULL);
      if (!mapping)
        {
          soup_message_set_status (message, SOUP_STATUS_INTERNAL_SERVER_ERROR);
          return;
        }

      buffer = soup_buffer_new_with_owner (g_mapped_file_get_contents (mapping),
                                           g_mapped_file_get_length (mapping),
                                           mapping,
                                           (GDestroyNotify)g_mapped_file_unref);
      soup_message_body_append_buffer (message->response_body, buffer);
    }
}

static void
process_caldav (SoupMessage *message,
                const gchar *path)
{
  g_message ("Processing CalDAV");
}


/*
 * Callbacks
 */

static gboolean
idle_quit_server_cb (gpointer user_data)
{
  GcalSimpleServer *self = user_data;

  g_main_loop_quit (self->thread_mainloop);

  return G_SOURCE_REMOVE;
}

static void
no_auth_handler_cb (SoupServer        *server,
                    SoupMessage       *message,
                    const gchar       *path,
                    GHashTable        *query,
                    SoupClientContext *client,
                    gpointer           user_data)
{
  g_debug ("No authenticaton needed");

  if (message->method == SOUP_METHOD_GET)
    process_get (message, "/public", path);
  else if (message->method == SOUP_METHOD_PROPFIND)
    process_caldav (message, path);
  else
    soup_message_set_status (message, SOUP_STATUS_NOT_IMPLEMENTED);
}

static void
auth_handler_cb (SoupServer        *server,
                 SoupMessage       *message,
                 const gchar       *path,
                 GHashTable        *query,
                 SoupClientContext *client,
                 gpointer           user_data)
{
  g_debug ("Needs authenticaton");

  if (message->method == SOUP_METHOD_GET)
    process_get (message, "/secret-area", path);
  else if (message->method == SOUP_METHOD_PROPFIND)
    process_caldav (message, path);
  else
    soup_message_set_status (message, SOUP_STATUS_NOT_IMPLEMENTED);
}

static gboolean
authorize_cb (SoupAuthDomain *domain,
              SoupMessage    *message,
              const char     *username,
              const char     *password,
              gpointer        user_data)
{
  const struct {
    const gchar *username;
    const gchar *password;
  } valid_credentials[] = {
    { "unicorn", "iamnotahorse" },
    { "feaneron", "idonotmaintainanything" },
    { "purism", "FINISHTHEPHONE!" },
  };
  guint i;

  g_debug ("Authorizing username: '%s', password: '%s'", username, password);

  if (!username && !password)
    return FALSE;

  for (i = 0; i < G_N_ELEMENTS (valid_credentials); i++)
    {
      if (g_strcmp0 (valid_credentials[i].username, username) == 0 &&
          g_strcmp0 (valid_credentials[i].password, password) == 0)
        {
          return TRUE;
        }
    }

  return FALSE;
}

static gpointer
run_server_in_thread (gpointer data)
{
  g_autoptr (SoupAuthDomain) domain = NULL;
  g_autoptr (GMainContext) context = NULL;
  g_autoptr (SoupServer) server = NULL;
  g_autoptr (GMainLoop) mainloop = NULL;
  g_autoslist (SoupURI) uris = NULL;
  g_autoptr (GError) error = NULL;
  GcalSimpleServer *self = data;
  g_autofree gchar *uri = NULL;

  context = g_main_context_new ();
  g_main_context_push_thread_default (context);
  mainloop = g_main_loop_new (context, FALSE);
  self->thread_mainloop = mainloop;

  server = soup_server_new (SOUP_SERVER_SERVER_HEADER, "gcal-simple-server",
                            NULL);

  /* Anything under /secret-area and /private requires authentication */
  domain = soup_auth_domain_basic_new (SOUP_AUTH_DOMAIN_REALM, "GcalSimpleServer",
                                       SOUP_AUTH_DOMAIN_BASIC_AUTH_CALLBACK, authorize_cb,
                                       SOUP_AUTH_DOMAIN_BASIC_AUTH_DATA, self,
                                       SOUP_AUTH_DOMAIN_ADD_PATH, "/secret-area",
                                       NULL);
  soup_server_add_auth_domain (server, domain);

  soup_server_listen_local (server, 0, SOUP_SERVER_LISTEN_IPV4_ONLY, &error);
  if (error)
    {
      g_critical ("Error starting up server: %s", error->message);
      return NULL;
    }

  soup_server_add_handler (server, NULL, no_auth_handler_cb, NULL, NULL);
  soup_server_add_handler (server, "/public", no_auth_handler_cb, NULL, NULL);
  soup_server_add_handler (server, "/secret-area", auth_handler_cb, NULL, NULL);

  uris = soup_server_get_uris (server);
  g_assert_cmpint (g_slist_length (uris), ==, 1);

  uri = soup_uri_to_string (uris->data, FALSE);
  self->uri = g_strdup (uri);

  g_debug ("Listening on %s", uri);

  g_mutex_lock (&self->running_mutex);
  self->running = TRUE;
  g_cond_signal (&self->running_cond);
  g_mutex_unlock (&self->running_mutex);

  g_main_loop_run (mainloop);

  g_debug ("Stopping");

  soup_server_disconnect (server);

  g_main_context_pop_thread_default (context);

  return NULL;
}


/*
 * GObject overrides
 */

static void
gcal_simple_server_finalize (GObject *object)
{
  GcalSimpleServer *self = (GcalSimpleServer *)object;

  if (self->thread)
    gcal_simple_server_stop (self);

  g_clear_pointer (&self->uri, g_free);

  G_OBJECT_CLASS (gcal_simple_server_parent_class)->finalize (object);
}

static void
gcal_simple_server_class_init (GcalSimpleServerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gcal_simple_server_finalize;
}

static void
gcal_simple_server_init (GcalSimpleServer *self)
{
}

GcalSimpleServer *
gcal_simple_server_new (void)
{
  return g_object_new (GCAL_TYPE_SIMPLE_SERVER, NULL);
}

void
gcal_simple_server_start (GcalSimpleServer *self)
{
  g_return_if_fail (GCAL_IS_SIMPLE_SERVER (self));

  if (self->thread)
    {
      g_warning ("Server already started");
      return;
    }

  self->thread = g_thread_new ("gcal_simple_server_thread", run_server_in_thread, self);

  g_mutex_lock (&self->running_mutex);
  while (!self->running)
    g_cond_wait (&self->running_cond, &self->running_mutex);
  g_mutex_unlock (&self->running_mutex);
}

void
gcal_simple_server_stop (GcalSimpleServer *self)
{
  GMainContext *context;

  g_return_if_fail (GCAL_IS_SIMPLE_SERVER (self));

  if (!self->thread)
    {
      g_warning ("Server not running");
      return;
    }

  context = g_main_loop_get_context (self->thread_mainloop);
  soup_add_completion (context, idle_quit_server_cb, self);

  g_mutex_lock (&self->running_mutex);
  self->running = FALSE;
  g_cond_signal (&self->running_cond);
  g_mutex_unlock (&self->running_mutex);

  g_thread_join (self->thread);
  self->thread = NULL;
}

SoupURI*
gcal_simple_server_get_uri (GcalSimpleServer *self)
{
  g_return_val_if_fail (GCAL_IS_SIMPLE_SERVER (self), NULL);
  g_return_val_if_fail (self->thread != NULL, NULL);

  return soup_uri_new (self->uri);
}
