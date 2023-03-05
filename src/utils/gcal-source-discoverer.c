/* gcal-source-discoverer.c
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

#define G_LOG_DOMAIN "GcalSourceDiscoverer"

#include <libecal/libecal.h>
#include <libsoup/soup.h>

#include "gcal-debug.h"
#include "gcal-source-discoverer.h"
#include "gcal-utils.h"

#define ICAL_HEADER_STRING "BEGIN:VCALENDAR\r\n"

G_DEFINE_QUARK (GcalSourceDiscoverer, gcal_source_discoverer_error);

typedef struct
{
  gchar              *uri;
  gchar              *username;
  gchar              *password;
} DiscovererData;


/*
 * Auxiliary methods
 */

static void
discoverer_data_free (gpointer data)
{
  DiscovererData *discoverer_data = data;

  if (!discoverer_data)
    return;

  g_free (discoverer_data->uri);
  g_free (discoverer_data->username);
  g_free (discoverer_data->password);
  g_free (discoverer_data);
}

static ESource*
create_source_for_uri (DiscovererData  *data)
{
  ESourceAuthentication *auth;
  g_autoptr (GUri) guri = NULL;
  g_autofree gchar *display_name = NULL;
  g_autofree gchar *basename = NULL;
  ESourceExtension *ext;
  ESourceWebdav *webdav;
  ESource *source;
  const gchar *host;
  const gchar *path;

  guri = g_uri_parse (data->uri, SOUP_HTTP_URI_FLAGS | G_URI_FLAGS_PARSE_RELAXED, NULL);
  if (!guri)
    GCAL_RETURN (NULL);

  host = g_uri_get_host (guri);

  /* Create the new source and add the needed extensions */
  source = e_source_new (NULL, NULL, NULL);
  e_source_set_parent (source, "webcal-stub");

  /* Display name */
  path = g_uri_get_path (guri);
  basename = g_path_get_basename (path);
  display_name = gcal_utils_format_filename_for_display (basename);
  e_source_set_display_name (source, display_name);

  ext = e_source_get_extension (source, E_SOURCE_EXTENSION_CALENDAR);
  e_source_backend_set_backend_name (E_SOURCE_BACKEND (ext), "webcal");

  /* Authentication */
  auth = e_source_get_extension (source, E_SOURCE_EXTENSION_AUTHENTICATION);
  e_source_authentication_set_host (auth, host);

  /* Webdav */
  webdav = e_source_get_extension (source, E_SOURCE_EXTENSION_WEBDAV_BACKEND);
  e_source_webdav_set_uri (webdav, guri);

  /* Security */
  if (g_strcmp0 (g_uri_get_scheme (guri), "https") == 0)
    {
      ESourceSecurity *security = e_source_get_extension (source, E_SOURCE_EXTENSION_SECURITY);
      e_source_security_set_secure (security, TRUE);
    }

  return source;
}

static ESource*
create_discovered_source (ESource                 *source,
                          GSList                  *user_addresses,
                          EWebDAVDiscoveredSource *discovered_source)
{
  g_autoptr (GUri) guri = NULL;
  ESourceSelectable *selectable;
  ESourceExtension *ext;
  ESourceWebdav *webdav;
  ESource *new_source;
  const gchar *resource_path;

  guri = g_uri_parse (discovered_source->href, SOUP_HTTP_URI_FLAGS | G_URI_FLAGS_PARSE_RELAXED, NULL);
  resource_path = g_uri_get_path (guri);

  new_source = e_source_new (NULL, NULL, NULL);
  e_source_set_parent (new_source, "local");

  ext = e_source_get_extension (new_source, E_SOURCE_EXTENSION_CALENDAR);
  e_source_backend_set_backend_name (E_SOURCE_BACKEND (ext), "local");

  /* Copy Authentication data */
  if (e_source_has_extension (source, E_SOURCE_EXTENSION_AUTHENTICATION))
    {
      ESourceAuthentication *new_auth, *parent_auth;

      parent_auth = e_source_get_extension (source, E_SOURCE_EXTENSION_AUTHENTICATION);
      new_auth = e_source_get_extension (new_source, E_SOURCE_EXTENSION_AUTHENTICATION);

      e_source_authentication_set_host (new_auth, e_source_authentication_get_host (parent_auth));
      e_source_authentication_set_method (new_auth, e_source_authentication_get_method (parent_auth));
      e_source_authentication_set_port (new_auth, e_source_authentication_get_port (parent_auth));
      e_source_authentication_set_user (new_auth, e_source_authentication_get_user (parent_auth));
      e_source_authentication_set_proxy_uid (new_auth, e_source_authentication_get_proxy_uid (parent_auth));
    }

  /* Copy Webdav data */
  if (e_source_has_extension (source, E_SOURCE_EXTENSION_WEBDAV_BACKEND))
    {
      ESourceWebdav *new_webdav, *parent_webdav;

      parent_webdav = e_source_get_extension (source, E_SOURCE_EXTENSION_WEBDAV_BACKEND);
      new_webdav = e_source_get_extension (new_source, E_SOURCE_EXTENSION_WEBDAV_BACKEND);

      e_source_webdav_set_display_name (new_webdav, e_source_webdav_get_display_name (parent_webdav));
      e_source_webdav_set_resource_path (new_webdav, e_source_webdav_get_resource_path (parent_webdav));
      e_source_webdav_set_resource_query (new_webdav, e_source_webdav_get_resource_query (parent_webdav));
      e_source_webdav_set_email_address (new_webdav, e_source_webdav_get_email_address (parent_webdav));
      e_source_webdav_set_ssl_trust (new_webdav, e_source_webdav_get_ssl_trust (parent_webdav));

      e_source_set_parent (new_source, "caldav-stub");
      e_source_backend_set_backend_name (E_SOURCE_BACKEND (ext), "caldav");
    }

  /* Security */
  if (e_source_has_extension (source, E_SOURCE_EXTENSION_SECURITY))
    {
      ESourceSecurity *new_security, *parent_security;
      const gchar *method;

      parent_security = e_source_get_extension (source, E_SOURCE_EXTENSION_SECURITY);
      new_security = e_source_get_extension (new_source, E_SOURCE_EXTENSION_SECURITY);

      method = e_source_security_get_method (parent_security);

      GCAL_TRACE_MSG ("New source '%s' has security method \"%s\"", e_source_get_display_name (new_source), method);

      e_source_security_set_method (new_security, method);
    }

  /* build up the new source */
  e_source_set_display_name (new_source, discovered_source->display_name);

  webdav = e_source_get_extension (new_source, E_SOURCE_EXTENSION_WEBDAV_BACKEND);
  e_source_webdav_set_resource_path (webdav, resource_path);
  e_source_webdav_set_display_name (webdav, discovered_source->display_name);

  if (user_addresses)
    e_source_webdav_set_email_address (webdav, user_addresses->data);

  /* Setup the color */
  selectable = e_source_get_extension (new_source, E_SOURCE_EXTENSION_CALENDAR);
  e_source_selectable_set_color (selectable, discovered_source->color);

  return new_source;
}

static gboolean
is_authentication_error (gint code)
{
  switch (code)
    {
    case SOUP_STATUS_UNAUTHORIZED:
    case SOUP_STATUS_FORBIDDEN:
    case SOUP_STATUS_METHOD_NOT_ALLOWED:
      return TRUE;
    }

  return FALSE;
}

static GUri *
create_and_validate_uri (const gchar  *uri,
                         GError      **error)
{
  g_autoptr (GUri) guri = NULL;

  guri = g_uri_parse (uri, SOUP_HTTP_URI_FLAGS | G_URI_FLAGS_PARSE_RELAXED, error);

  if (!guri)
    GCAL_RETURN (NULL);

  if (!g_uri_get_host (guri) || g_uri_get_host (guri)[0] == '\0')
    {
      g_set_error (error, G_URI_ERROR, G_URI_ERROR_FAILED, "Invalid URI");
      return NULL;
    }

  return g_steal_pointer (&guri);
}


/*
 * Callbacks
 */

static gboolean
on_soup_message_authenticate_cb (SoupMessage *message,
                                 SoupAuth    *auth,
                                 gboolean     retrying,
                                 gpointer     user_data)
{
  DiscovererData *data = user_data;

  if (data->username && data->password)
    soup_auth_authenticate (auth, data->username, data->password);

  return TRUE;
}

typedef GPtrArray* (*DiscoverFunc) (DiscovererData  *data,
                                    GCancellable    *cancellable,
                                    GError         **error);

static GPtrArray*
discover_file_in_thread (DiscovererData  *data,
                         GCancellable    *cancellable,
                         GError         **error)
{
  g_autoptr (GInputStream) input_stream = NULL;
  g_autoptr (SoupMessage) message = NULL;
  g_autoptr (SoupSession) session = NULL;
  g_autoptr (GPtrArray) source = NULL;
  g_autoptr (GUri) guri = NULL;
  g_autofree gchar *uri_str = NULL;
  g_autofree gchar *header_string = NULL;
  gsize header_string_read_len;
  gboolean is_calendar_header;
  const gchar *content_type;
  gboolean is_calendar_content_type;

  GCAL_ENTRY;

  guri = create_and_validate_uri (data->uri, error);

  if (!guri)
    GCAL_RETURN (NULL);

  if (g_strcmp0 (g_uri_get_scheme (guri), "webcal") == 0)
    e_util_change_uri_component (&guri, SOUP_URI_SCHEME, "https");

  uri_str = g_uri_to_string_partial (guri, G_URI_HIDE_PASSWORD);
  GCAL_TRACE_MSG ("Creating request for %s", uri_str);

  session = soup_session_new_with_options ("timeout", 10, NULL);

  message = soup_message_new_from_uri ("GET", guri);
  g_signal_connect (message, "authenticate", G_CALLBACK (on_soup_message_authenticate_cb), data);

  input_stream = soup_session_send (session, message, cancellable, error);

  if (!input_stream)
    GCAL_RETURN (NULL);

  header_string = g_malloc0 (sizeof (gchar) * (strlen (ICAL_HEADER_STRING) + 1));
  g_input_stream_read_all (input_stream,
                           header_string,
                           strlen (ICAL_HEADER_STRING),
                           &header_string_read_len,
                           cancellable,
                           NULL);

  is_calendar_header = header_string_read_len == strlen (ICAL_HEADER_STRING) &&
                       g_strcmp0 (ICAL_HEADER_STRING, header_string) == 0;

  g_input_stream_close (input_stream, cancellable, error);

  content_type = soup_message_headers_get_content_type (soup_message_get_response_headers (message), NULL);
  is_calendar_content_type = content_type && g_strstr_len (content_type, -1, "text/calendar") != NULL;

  GCAL_TRACE_MSG ("Message retrieved, content type: %s, status code: %u", content_type, soup_message_get_status (message));

  if (is_authentication_error (soup_message_get_status (message)))
    {
      g_set_error (error,
                   GCAL_SOURCE_DISCOVERER_ERROR,
                   GCAL_SOURCE_DISCOVERER_ERROR_UNAUTHORIZED,
                   "%s",
                   soup_message_get_reason_phrase (message));

      GCAL_RETURN (NULL);
    }

  if (is_calendar_header || is_calendar_content_type)
    {
      source = g_ptr_array_new_full (1, g_object_unref);
      g_ptr_array_add (source, create_source_for_uri (data));
    }

  GCAL_RETURN (g_steal_pointer (&source));
}

static GPtrArray*
discover_webdav_in_thread (DiscovererData  *data,
                           GCancellable    *cancellable,
                           GError         **error)
{
  g_autoptr (ENamedParameters) credentials = NULL;
  g_autoptr (GPtrArray) sources = NULL;
  g_autoptr (ESource) source = NULL;
  g_autoptr (GError) local_error = NULL;
  g_autofree gchar *certificate_pem = NULL;
  g_autoptr (GUri) guri = NULL;
  GTlsCertificateFlags flags;
  GSList *discovered_sources = NULL;
  GSList *user_addresses = NULL;
  GSList *l;

  GCAL_ENTRY;

  guri = create_and_validate_uri (data->uri, error);

  if (!guri)
    GCAL_RETURN (NULL);

  credentials = e_named_parameters_new ();
  e_named_parameters_set (credentials, E_SOURCE_CREDENTIAL_USERNAME, data->username);
  e_named_parameters_set (credentials, E_SOURCE_CREDENTIAL_PASSWORD, data->password);

  source = create_source_for_uri (data);

  e_webdav_discover_sources_sync (source,
                                  data->uri,
                                  E_WEBDAV_DISCOVER_SUPPORTS_EVENTS,
                                  credentials,
                                  &certificate_pem,
                                  &flags,
                                  &discovered_sources,
                                  &user_addresses,
                                  cancellable,
                                  &local_error);

  if (local_error)
    {
      if (local_error->domain == E_SOUP_SESSION_ERROR &&
          is_authentication_error (local_error->code))
        {
          g_set_error (error,
                       GCAL_SOURCE_DISCOVERER_ERROR,
                       GCAL_SOURCE_DISCOVERER_ERROR_UNAUTHORIZED,
                       "%s",
                       soup_status_get_phrase (local_error->code));
        }
      else
        {
          g_propagate_error (error, g_steal_pointer (&local_error));
        }

      GCAL_RETURN (NULL);
    }

  sources = g_ptr_array_new_full (g_slist_length (discovered_sources), g_object_unref);
  for (l = discovered_sources; l; l = l->next)
    {
      EWebDAVDiscoveredSource *discovered_source;
      ESource *new_source;

      discovered_source = l->data;
      new_source = create_discovered_source (source, user_addresses, discovered_source);

      if (!new_source)
        continue;

      g_ptr_array_add (sources, new_source);
    }

  g_clear_pointer (&discovered_sources, e_webdav_discover_free_discovered_sources);
  g_slist_free_full (user_addresses, g_free);

  GCAL_RETURN (g_steal_pointer (&sources));
}

static void
discover_sources_in_thread_cb (GTask        *task,
                               gpointer      source_object,
                               gpointer      task_data,
                               GCancellable *cancellable)
{
  g_autoptr (GError) error = NULL;
  DiscovererData *data = task_data;
  guint i;

  const DiscoverFunc discover_funcs[] = {
    discover_file_in_thread,
    discover_webdav_in_thread,
  };

  for (i = 0; i < G_N_ELEMENTS (discover_funcs); i++)
    {
      g_autoptr (GPtrArray) sources = NULL;
      g_autoptr (GError) local_error = NULL;

      sources = discover_funcs[i] (data, cancellable, &local_error);

      if (sources)
        {
          g_task_return_pointer (task, g_steal_pointer (&sources), NULL);
          return;
        }
      else if (local_error)
        {
          g_clear_error (&error);
          g_propagate_error (&error, g_steal_pointer (&local_error));
        }
    }

  if (error)
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_UNKNOWN, "Unknown error");
}

void
gcal_discover_sources_from_uri (const gchar         *uri,
                                const gchar         *username,
                                const gchar         *password,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;
  DiscovererData *data;

  g_assert (uri != NULL);

  data = g_new0 (DiscovererData, 1);
  data->uri = g_strdup (uri);
  data->username = g_strdup (username);
  data->password = g_strdup (password);

  task = g_task_new (NULL, cancellable, callback, user_data);
  g_task_set_source_tag (task, gcal_discover_sources_from_uri);
  g_task_set_task_data (task, data, discoverer_data_free);

  g_task_run_in_thread (task, discover_sources_in_thread_cb);
}

GPtrArray*
gcal_discover_sources_from_uri_finish (GAsyncResult  *result,
                                       GError       **error)
{
  g_return_val_if_fail (g_task_is_valid (result, NULL), NULL);
  g_return_val_if_fail (!error || !*error, NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}
