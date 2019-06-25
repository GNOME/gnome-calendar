/* gcal-new-calendar-page.c
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

#define G_LOG_DOMAIN "GcalNewCalendarPage"

#include <glib/gi18n.h>

#include "gcal-context.h"
#include "gcal-calendar-management-page.h"
#include "gcal-debug.h"
#include "gcal-new-calendar-page.h"
#include "gcal-utils.h"

#define ENTRY_PROGRESS_TIMEOUT 100 // ms

struct _GcalNewCalendarPage
{
  GtkBox              parent;

  GtkWidget          *add_button;
  GtkEntry           *calendar_address_entry;
  GtkFileChooser     *calendar_file_chooser_button;
  GtkFileFilter      *calendar_file_filter;
  GtkWidget          *cancel_button;
  GtkWidget          *credentials_cancel_button;
  GtkWidget          *credentials_connect_button;
  GtkWidget          *credentials_dialog;
  GtkEntry           *credentials_password_entry;
  GtkEntry           *credentials_user_entry;
  GtkColorChooser    *local_calendar_color_button;
  GtkEntry           *local_calendar_name_entry;
  GtkWidget          *web_sources_listbox;
  GtkWidget          *web_sources_revealer;

  guint               calendar_address_id;
  gboolean            prompt_password;
  GList              *remote_sources;
  guint               validate_url_resource_id;

  ESource            *local_source;

  GcalContext        *context;
};


static gboolean      validate_url_cb                             (GcalNewCalendarPage *self);

static void          gcal_calendar_management_page_iface_init    (GcalCalendarManagementPageInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GcalNewCalendarPage, gcal_new_calendar_page, GTK_TYPE_BOX,
                         G_IMPLEMENT_INTERFACE (GCAL_TYPE_CALENDAR_MANAGEMENT_PAGE,
                                                gcal_calendar_management_page_iface_init))

enum
{
  PROP_0,
  PROP_CONTEXT,
  N_PROPS
};

/*
 * Auxiliary methods
 */

static ESource*
duplicate_source (ESource *source)
{
  ESourceExtension *ext;
  ESource *new_source;

  g_assert (source && E_IS_SOURCE (source));

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

      e_source_set_parent (new_source, "webcal-stub");
      e_source_backend_set_backend_name (E_SOURCE_BACKEND (ext), "webcal");
    }

  return new_source;
}

static gint
prompt_credentials (GcalNewCalendarPage  *self,
                    gchar               **username,
                    gchar               **password)
{
  gint response;

  /* Cleanup last credentials */
  gtk_entry_set_text (self->credentials_password_entry, "");
  gtk_entry_set_text (self->credentials_user_entry, "");

  gtk_widget_grab_focus (GTK_WIDGET (self->credentials_user_entry));

  /* Show the dialog, then destroy it */
  response = gtk_dialog_run (GTK_DIALOG (self->credentials_dialog));

  if (response == GTK_RESPONSE_OK)
    {
      if (username)
        *username = g_strdup (gtk_entry_get_text (self->credentials_user_entry));

      if (password)
        *password = g_strdup (gtk_entry_get_text (self->credentials_password_entry));
    }

  gtk_widget_hide (self->credentials_dialog);

  return response;
}

static gchar*
calendar_path_to_name_suggestion (GFile *file)
{
  g_autofree gchar *unencoded_basename = NULL;
  g_autofree gchar *basename = NULL;
  gchar *ext;
  guint i;

  const gchar*
  import_file_extensions[] = {
    ".ical",
    ".ics",
    ".ifb",
    ".icalendar",
    ".vcs"
  };

  g_return_val_if_fail (G_IS_FILE (file), NULL);

  unencoded_basename = g_file_get_basename (file);
  basename = g_filename_display_name (unencoded_basename);

  ext = strrchr (basename, '.');

  if (!ext)
    return NULL;

  for (i = 0; i < G_N_ELEMENTS(import_file_extensions); i++)
    {
      if (g_ascii_strcasecmp (import_file_extensions[i], ext) == 0)
        {
           *ext = '\0';
           break;
        }
    }

  g_strdelimit (basename, "-_", ' ');

  return g_steal_pointer (&basename);
}

static void
update_add_button (GcalNewCalendarPage *self)
{
  gboolean valid;

  valid = self->local_source != NULL || self->remote_sources != NULL;
  gtk_widget_set_sensitive (self->add_button, valid);
}

static void
update_local_source (GcalNewCalendarPage *self)
{
  g_autofree gchar *calendar_name = NULL;

  g_clear_object (&self->local_source);

  calendar_name = g_strdup (gtk_entry_get_text (self->local_calendar_name_entry));
  calendar_name = g_strstrip (calendar_name);

  if (calendar_name && g_utf8_strlen (calendar_name, -1) > 0)
    {
      g_autofree gchar *color_string = NULL;
      ESourceExtension *ext;
      ESource *source;
      GdkRGBA color;

      gtk_color_chooser_get_rgba (self->local_calendar_color_button, &color);
      color_string = gdk_rgba_to_string (&color);

      /* Create the new source and add the needed extensions */
      source = e_source_new (NULL, NULL, NULL);
      e_source_set_parent (source, "local-stub");
      e_source_set_display_name (source, calendar_name);

      ext = e_source_get_extension (source, E_SOURCE_EXTENSION_CALENDAR);
      e_source_backend_set_backend_name (E_SOURCE_BACKEND (ext), "local");
      e_source_selectable_set_color (E_SOURCE_SELECTABLE (ext), color_string);

      e_source_backend_set_backend_name (E_SOURCE_BACKEND (ext), "local");

      self->local_source = source;
    }

  update_add_button (self);
}


/*
 * Callbacks
 */

static void
on_file_chooser_button_file_set_cb (GtkFileChooser      *chooser,
                                    GcalNewCalendarPage *self)
{
  g_autofree gchar *display_name = NULL;
  g_autoptr (ESource) source = NULL;
  g_autoptr (GFile) file = NULL;
  ESourceExtension *ext;

  GCAL_ENTRY;

  file = gtk_file_chooser_get_file (chooser);

  if (!file)
    GCAL_RETURN ();

  /* Create the new source and add the needed extensions */
  source = e_source_new (NULL, NULL, NULL);
  e_source_set_parent (source, "local-stub");

  ext = e_source_get_extension (source, E_SOURCE_EXTENSION_CALENDAR);
  e_source_backend_set_backend_name (E_SOURCE_BACKEND (ext), "local");

  ext = e_source_get_extension (source, E_SOURCE_EXTENSION_LOCAL_BACKEND);
  e_source_local_set_custom_file (E_SOURCE_LOCAL (ext), file);

  /* update the source properties */
  display_name = calendar_path_to_name_suggestion (file);
  e_source_set_display_name (source, display_name);

  /* TODO: report errors */
  gcal_manager_save_source (gcal_context_get_manager (self->context), source);

  gcal_calendar_management_page_switch_page (GCAL_CALENDAR_MANAGEMENT_PAGE (self),
                                             "calendars",
                                             NULL);

  gtk_file_chooser_unselect_all (self->calendar_file_chooser_button);

  GCAL_EXIT;
}

static gboolean
pulse_web_entry (GcalNewCalendarPage *self)
{
  gtk_entry_progress_pulse (self->calendar_address_entry);

  self->calendar_address_id = g_timeout_add (ENTRY_PROGRESS_TIMEOUT, (GSourceFunc) pulse_web_entry, self);

  return G_SOURCE_CONTINUE;
}

static void
on_check_activated_cb (GtkWidget           *check,
                       GParamSpec          *spec,
                       GcalNewCalendarPage *self)
{
  GtkWidget *row;
  ESource *source;

  row = gtk_widget_get_parent (check);
  source = g_object_get_data (G_OBJECT (row), "source");

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check)))
    self->remote_sources = g_list_prepend (self->remote_sources, source);
  else
    self->remote_sources = g_list_remove (self->remote_sources, source);

  update_add_button (self);
}

static void
discover_sources_cb (GObject      *source,
                     GAsyncResult *result,
                     gpointer      user_data)
{
  GcalNewCalendarPage  *self;
  GSList *discovered_sources;
  GSList *user_addresses;
  GSList *aux;
  GError *error;

  self = GCAL_NEW_CALENDAR_PAGE (user_data);
  error = NULL;

  /* Stop the pulsing entry */
  if (self->calendar_address_id != 0)
    {
      gtk_entry_set_progress_fraction (GTK_ENTRY (self->calendar_address_entry), 0);
      g_source_remove (self->calendar_address_id);
      self->calendar_address_id = 0;
    }

  if (!e_webdav_discover_sources_finish (E_SOURCE (source), result, NULL, NULL, &discovered_sources, &user_addresses,
                                        &error))
    {
      /* Don't add an source with errors */
      gtk_widget_set_sensitive (self->add_button, FALSE);

      /*
       * If it's the first try and things went wrong, retry with the user
       * credentials. Also, it checks for the error code, since we don't
       * really want to retry things on unavailable servers.
       */
      if (!self->prompt_password &&
          (error->code == 14 ||
           error->code == 401 ||
           error->code == 403 ||
           error->code == 405))
        {
          self->prompt_password = TRUE;
          validate_url_cb (self);
        }
      else
        {
          g_debug ("Error: %s", error->message);
        }

      g_error_free (error);
      return;
    }

  /* Add a success icon to the entry */
  gtk_entry_set_icon_from_icon_name (GTK_ENTRY (self->calendar_address_entry), GTK_ENTRY_ICON_SECONDARY,
                                     "emblem-ok-symbolic");

  /* Remove previous results */
  g_list_free_full (gtk_container_get_children (GTK_CONTAINER (self->web_sources_listbox)),
                    (GDestroyNotify) gtk_widget_destroy);

  /* Show the list of calendars */
  gtk_revealer_set_reveal_child (GTK_REVEALER (self->web_sources_revealer), TRUE);
  gtk_widget_show (self->web_sources_revealer);

  /* TODO: show a list of calendars */
  for (aux = discovered_sources; aux != NULL; aux = aux->next)
    {
      g_autoptr (SoupURI) soup_uri = NULL;
      EWebDAVDiscoveredSource *discovered_source;
      const gchar *resource_path = NULL;

      discovered_source = aux->data;

      soup_uri = soup_uri_new (discovered_source->href);

      /* Get the new resource path from the uri */
      resource_path = soup_uri_get_path (soup_uri);

      if (soup_uri)
        {
          ESourceSelectable *selectable;
          ESourceWebdav *webdav;
          GtkWidget *row, *check;
          ESource *new_source;


          /* build up the new source */
          new_source = duplicate_source (E_SOURCE (source));
          e_source_set_display_name (E_SOURCE (new_source), discovered_source->display_name);

          webdav = e_source_get_extension (new_source, E_SOURCE_EXTENSION_WEBDAV_BACKEND);
          e_source_webdav_set_resource_path (webdav, resource_path);
          e_source_webdav_set_display_name (webdav, discovered_source->display_name);

          if (user_addresses)
            e_source_webdav_set_email_address (webdav, user_addresses->data);

          /* Setup the color */
          selectable = e_source_get_extension (new_source, E_SOURCE_EXTENSION_CALENDAR);
          e_source_selectable_set_color (selectable, discovered_source->color);

          /* create the new row */
          row = gtk_list_box_row_new ();

          check = gtk_check_button_new ();
          gtk_button_set_label (GTK_BUTTON (check), discovered_source->display_name);
          g_signal_connect (check, "notify::active", G_CALLBACK (on_check_activated_cb), self);

          gtk_container_add (GTK_CONTAINER (row), check);
          gtk_container_add (GTK_CONTAINER (self->web_sources_listbox), row);

          g_object_set_data (G_OBJECT (row), "parent-source", source);
          g_object_set_data (G_OBJECT (row), "source", new_source);

          gtk_widget_show_all (row);
        }
    }

  /* Free things up */
  e_webdav_discover_free_discovered_sources (discovered_sources);
  g_slist_free_full (user_addresses, g_free);
}

static gboolean
validate_url_cb (GcalNewCalendarPage *self)
{
  ESourceAuthentication *auth;
  ENamedParameters *credentials;
  ESourceExtension *ext;
  ESourceWebdav *webdav;
  ESource *source;
  g_autoptr (SoupURI) soup_uri;
  const gchar *uri;
  const gchar *host, *path;
  gboolean is_file;

  GCAL_ENTRY;

  self->validate_url_resource_id = 0;
  soup_uri = NULL;
  host = path = NULL;
  is_file = FALSE;

  /* Remove any reminescent ESources cached before */
  if (self->remote_sources)
    {
      g_list_free_full (self->remote_sources, g_object_unref);
      self->remote_sources = NULL;
    }

  /* Remove previous results */
  g_list_free_full (gtk_container_get_children (GTK_CONTAINER (self->web_sources_listbox)),
                    (GDestroyNotify) gtk_widget_destroy);
  gtk_revealer_set_reveal_child (GTK_REVEALER (self->web_sources_revealer), FALSE);

  /* Clear the entry icon */
  gtk_entry_set_icon_from_icon_name (self->calendar_address_entry, GTK_ENTRY_ICON_SECONDARY, NULL);

  /* Get the hostname and file path from the server */
  uri = gtk_entry_get_text (self->calendar_address_entry);
  soup_uri = soup_uri_new (uri);
  if (!soup_uri)
    GCAL_RETURN (G_SOURCE_REMOVE);

  host = soup_uri_get_host (soup_uri);
  path = soup_uri_get_path (soup_uri);

  if (soup_uri_get_scheme (soup_uri) == SOUP_URI_SCHEME_FILE)
    is_file = TRUE;

  g_debug ("Detected host: '%s', path: '%s'", host, path);

  /* Create the new source and add the needed extensions */
  source = e_source_new (NULL, NULL, NULL);
  e_source_set_parent (source, "webcal-stub");

  ext = e_source_get_extension (source, E_SOURCE_EXTENSION_CALENDAR);
  e_source_backend_set_backend_name (E_SOURCE_BACKEND (ext), "webcal");

  /* Authentication */
  auth = e_source_get_extension (source, E_SOURCE_EXTENSION_AUTHENTICATION);
  e_source_authentication_set_host (auth, host);

  /* Webdav */
  webdav = e_source_get_extension (source, E_SOURCE_EXTENSION_WEBDAV_BACKEND);
  e_source_webdav_set_soup_uri (webdav, soup_uri);

  /*
   * If we're dealing with an absolute file path, there is no need
   * to check the server for more sources.
   */
  if (is_file)
    {
      self->remote_sources = g_list_append (self->remote_sources, source);
      gtk_widget_set_sensitive (self->add_button, source != NULL);
      GCAL_RETURN (G_SOURCE_REMOVE);
    }

  /* Pulse the entry while it performs the check */
  self->calendar_address_id = g_timeout_add (ENTRY_PROGRESS_TIMEOUT, (GSourceFunc) pulse_web_entry, self);

  /*
   * Try to retrieve the sources without prompting username and password. If
   * we get any error, then it prompts and retry.
   */
  credentials = e_named_parameters_new ();

  if (!self->prompt_password)
    {
      g_debug ("Trying to connect without credentials...");

      /* NULL credentials */
      e_named_parameters_set (credentials, E_SOURCE_CREDENTIAL_USERNAME, NULL);
      e_named_parameters_set (credentials, E_SOURCE_CREDENTIAL_PASSWORD, NULL);

      e_webdav_discover_sources (source,
                                 uri,
                                 E_WEBDAV_DISCOVER_SUPPORTS_EVENTS,
                                 credentials,
                                 NULL,
                                 discover_sources_cb,
                                 self);
    }
  else
    {
      g_autofree gchar *password = NULL;
      g_autofree gchar *user = NULL;
      gint response;

      g_debug ("No credentials failed, retrying with user credentials...");

      response = prompt_credentials (self, &user, &password);

      g_message ("lalala");

      if (response == GTK_RESPONSE_OK)
        {
          e_named_parameters_set (credentials, E_SOURCE_CREDENTIAL_USERNAME, user);
          e_named_parameters_set (credentials, E_SOURCE_CREDENTIAL_PASSWORD, password);

          e_webdav_discover_sources (source,
                                     uri,
                                     E_WEBDAV_DISCOVER_SUPPORTS_EVENTS,
                                     credentials,
                                     NULL,
                                     discover_sources_cb,
                                     self);
        }
    }

  e_named_parameters_free (credentials);

  GCAL_RETURN (G_SOURCE_REMOVE);
}

static void
on_add_button_clicked_cb (GtkWidget           *button,
                          GcalNewCalendarPage *self)
{
  GcalManager *manager;

  manager = gcal_context_get_manager (self->context);

  /* Commit the new source */
  if (self->local_source)
    gcal_manager_save_source (manager, self->local_source);

  /* Commit each new remote source */
  if (self->remote_sources)
    {
      GList *l;

      for (l = self->remote_sources; l; l = l->next)
        gcal_manager_save_source (manager, l->data);

      g_list_free_full (self->remote_sources, g_object_unref);
      self->remote_sources = NULL;
    }

  gcal_calendar_management_page_switch_page (GCAL_CALENDAR_MANAGEMENT_PAGE (self),
                                             "calendars",
                                             NULL);
}

static void
on_calendar_address_activated_cb (GtkEntry            *entry,
                                  GcalNewCalendarPage *self)
{
  validate_url_cb (self);
}

static void
on_credential_button_clicked_cb (GtkWidget           *button,
                                 GcalNewCalendarPage *self)
{
  if (button == self->credentials_cancel_button)
    gtk_dialog_response (GTK_DIALOG (self->credentials_dialog), GTK_RESPONSE_CANCEL);
  else
    gtk_dialog_response (GTK_DIALOG (self->credentials_dialog), GTK_RESPONSE_OK);
}

static void
on_credential_entry_activate_cb (GtkEntry  *entry,
                                 GtkDialog *dialog)
{
  gtk_dialog_response (dialog, GTK_RESPONSE_OK);
}

static void
on_url_entry_text_changed_cb (GtkEntry            *entry,
                              GParamSpec          *pspec,
                              GcalNewCalendarPage *self)
{
  const gchar* text;

  GCAL_ENTRY;

  text = gtk_entry_get_text (entry);

  if (self->calendar_address_id != 0)
    {
      gtk_entry_set_progress_fraction (entry, 0);
      g_clear_handle_id (&self->calendar_address_id, g_source_remove);
    }

  if (self->validate_url_resource_id != 0)
    g_clear_handle_id (&self->validate_url_resource_id, g_source_remove);

  if (g_utf8_strlen (text, -1) > 0)
    {
      /*
       * At first, don't bother the user with the login prompt. Only prompt it when
       * it fails.
       */
      self->prompt_password = FALSE;
      self->validate_url_resource_id = g_timeout_add (500, (GSourceFunc) validate_url_cb, self);
    }
  else
    {
      gtk_entry_set_progress_fraction (self->calendar_address_entry, 0);
    }

  GCAL_EXIT;
}

static void
on_cancel_button_clicked_cb (GtkWidget                  *button,
                             GcalCalendarManagementPage *page)
{
  gcal_calendar_management_page_switch_page (page, "calendars", NULL);
}

static void
on_local_calendar_name_entry_text_changed_cb (GtkEntry            *entry,
                                              GParamSpec          *pspec,
                                              GcalNewCalendarPage *self)
{
  update_local_source (self);
}

static void
on_web_description_label_link_activated_cb (GtkLabel            *label,
                                            gchar               *uri,
                                            GcalNewCalendarPage *self)
{
  GDBusConnection *connection = g_application_get_dbus_connection (g_application_get_default ());

  gcal_utils_launch_online_accounts_panel (connection, NULL, NULL);
}


/*
 * GcalCalendarManagementPage iface
 */

static const gchar*
gcal_new_calendar_page_get_name (GcalCalendarManagementPage *page)
{
  return "new-calendar";
}

static const gchar*
gcal_new_calendar_page_get_title (GcalCalendarManagementPage *page)
{
  return _("New Calendar");
}

static void
gcal_new_calendar_page_activate (GcalCalendarManagementPage *page,
                                 gpointer                    page_data)
{
  GcalNewCalendarPage *self;
  GtkHeaderBar *headerbar;

  GCAL_ENTRY;

  self = GCAL_NEW_CALENDAR_PAGE (page);
  headerbar = gcal_calendar_management_page_get_titlebar (page);

  gtk_header_bar_pack_start (headerbar, self->cancel_button);
  gtk_header_bar_pack_end (headerbar, self->add_button);
  gtk_header_bar_set_show_close_button (headerbar, FALSE);

  GCAL_EXIT;
}
static void
gcal_new_calendar_page_deactivate (GcalCalendarManagementPage *page)
{
  GcalNewCalendarPage *self;
  GtkHeaderBar *headerbar;

  GCAL_ENTRY;

  self = GCAL_NEW_CALENDAR_PAGE (page);
  headerbar = gcal_calendar_management_page_get_titlebar (page);

  gtk_container_remove (GTK_CONTAINER (headerbar), self->cancel_button);
  gtk_container_remove (GTK_CONTAINER (headerbar), self->add_button);
  gtk_header_bar_set_show_close_button (headerbar, TRUE);

  if (self->remote_sources)
    {
      g_list_free_full (self->remote_sources, g_object_unref);
      self->remote_sources = NULL;
    }

  gtk_entry_set_text (self->local_calendar_name_entry, "");
  gtk_entry_set_text (self->calendar_address_entry, "");

  GCAL_EXIT;
}

static void
gcal_calendar_management_page_iface_init (GcalCalendarManagementPageInterface *iface)
{
  iface->get_name = gcal_new_calendar_page_get_name;
  iface->get_title = gcal_new_calendar_page_get_title;
  iface->activate = gcal_new_calendar_page_activate;
  iface->deactivate = gcal_new_calendar_page_deactivate;
}

/*
 * GObject overrides
 */

static void
gcal_new_calendar_page_finalize (GObject *object)
{
  GcalNewCalendarPage *self = (GcalNewCalendarPage *)object;

  g_clear_object (&self->context);

  G_OBJECT_CLASS (gcal_new_calendar_page_parent_class)->finalize (object);
}

static void
gcal_new_calendar_page_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GcalNewCalendarPage *self = GCAL_NEW_CALENDAR_PAGE (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_value_set_object (value, self->context);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_new_calendar_page_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GcalNewCalendarPage *self = GCAL_NEW_CALENDAR_PAGE (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      self->context = g_value_dup_object (value);
      g_assert (self->context != NULL);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_new_calendar_page_class_init (GcalNewCalendarPageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gcal_new_calendar_page_finalize;
  object_class->get_property = gcal_new_calendar_page_get_property;
  object_class->set_property = gcal_new_calendar_page_set_property;

  g_object_class_override_property (object_class, PROP_CONTEXT, "context");

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/new-calendar-page.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalNewCalendarPage, add_button);
  gtk_widget_class_bind_template_child (widget_class, GcalNewCalendarPage, calendar_address_entry);
  gtk_widget_class_bind_template_child (widget_class, GcalNewCalendarPage, calendar_file_chooser_button);
  gtk_widget_class_bind_template_child (widget_class, GcalNewCalendarPage, calendar_file_filter);
  gtk_widget_class_bind_template_child (widget_class, GcalNewCalendarPage, cancel_button);
  gtk_widget_class_bind_template_child (widget_class, GcalNewCalendarPage, credentials_cancel_button);
  gtk_widget_class_bind_template_child (widget_class, GcalNewCalendarPage, credentials_connect_button);
  gtk_widget_class_bind_template_child (widget_class, GcalNewCalendarPage, credentials_dialog);
  gtk_widget_class_bind_template_child (widget_class, GcalNewCalendarPage, credentials_password_entry);
  gtk_widget_class_bind_template_child (widget_class, GcalNewCalendarPage, credentials_user_entry);
  gtk_widget_class_bind_template_child (widget_class, GcalNewCalendarPage, local_calendar_color_button);
  gtk_widget_class_bind_template_child (widget_class, GcalNewCalendarPage, local_calendar_name_entry);
  gtk_widget_class_bind_template_child (widget_class, GcalNewCalendarPage, web_sources_listbox);
  gtk_widget_class_bind_template_child (widget_class, GcalNewCalendarPage, web_sources_revealer);

  gtk_widget_class_bind_template_callback (widget_class, on_add_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_calendar_address_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_cancel_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_credential_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_credential_entry_activate_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_file_chooser_button_file_set_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_local_calendar_name_entry_text_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_url_entry_text_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_web_description_label_link_activated_cb);
}

static void
gcal_new_calendar_page_init (GcalNewCalendarPage *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_file_filter_set_name (self->calendar_file_filter, _("Calendar files"));
}
