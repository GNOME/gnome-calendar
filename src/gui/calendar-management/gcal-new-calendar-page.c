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
#include "gcal-source-discoverer.h"
#include "gcal-utils.h"

#define ENTRY_PROGRESS_TIMEOUT 100 // ms

typedef enum
{
  ENTRY_STATE_EMPTY,
  ENTRY_STATE_VALIDATING,
  ENTRY_STATE_VALID,
  ENTRY_STATE_INVALID
} EntryState;

struct _GcalNewCalendarPage
{
  GtkBox              parent;

  GtkWidget          *add_button;
  GtkEntry           *calendar_address_entry;
  EntryState          calendar_address_entry_state;
  GtkFileChooser     *calendar_file_chooser_button;
  GtkFileFilter      *calendar_file_filter;
  GtkWidget          *cancel_button;
  GtkWidget          *credentials_cancel_button;
  GtkWidget          *credentials_connect_button;
  GtkEntry           *credentials_password_entry;
  GtkPopover         *credentials_popover;
  GtkEntry           *credentials_user_entry;
  GtkColorChooser    *local_calendar_color_button;
  GtkEntry           *local_calendar_name_entry;
  GtkWidget          *web_sources_listbox;
  GtkWidget          *web_sources_revealer;

  guint               calendar_address_id;
  GPtrArray          *remote_sources;
  guint               validate_url_resource_id;

  GCancellable       *cancellable;

  ESource            *local_source;

  GcalContext        *context;
};

static gboolean      pulse_web_entry                             (gpointer           data);

static void          sources_discovered_cb                       (GObject           *source_object,
                                                                  GAsyncResult      *result,
                                                                  gpointer           user_data);

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

  valid = (self->local_source != NULL || self->remote_sources != NULL) &&
      self->calendar_address_entry_state != ENTRY_STATE_VALIDATING &&
      self->calendar_address_entry_state != ENTRY_STATE_INVALID;

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

static void
toggle_url_entry_pulsing (GcalNewCalendarPage *self,
                          gboolean             pulsing)
{
  gtk_entry_set_progress_fraction (self->calendar_address_entry, 0);

  if (pulsing && self->calendar_address_id == 0)
    self->calendar_address_id = g_timeout_add (ENTRY_PROGRESS_TIMEOUT, pulse_web_entry, self);
  else if (!pulsing && self->calendar_address_id != 0)
    g_clear_handle_id (&self->calendar_address_id, g_source_remove);
}

static void
update_url_entry_state (GcalNewCalendarPage *self,
                        EntryState           state)
{
  GtkStyleContext *context;

  self->calendar_address_entry_state = state;

  context = gtk_widget_get_style_context (GTK_WIDGET (self->calendar_address_entry));
  if (state == ENTRY_STATE_INVALID)
    gtk_style_context_add_class (context, "error");
  else
    gtk_style_context_remove_class (context, "error");

  update_add_button (self);
  toggle_url_entry_pulsing (self, state == ENTRY_STATE_VALIDATING);
}

static void
discover_sources (GcalNewCalendarPage *self)
{
  const gchar *username;
  const gchar *password;
  const gchar *url;

  GCAL_ENTRY;

  self->validate_url_resource_id = 0;

  url = gtk_entry_get_text (self->calendar_address_entry);
  username = gtk_entry_get_text (self->credentials_user_entry);
  password = gtk_entry_get_text (self->credentials_password_entry);

  gcal_discover_sources_from_uri (url,
                                  username,
                                  password,
                                  self->cancellable,
                                  sources_discovered_cb,
                                  self);
  update_url_entry_state (self, ENTRY_STATE_VALIDATING);

  GCAL_EXIT;
}

static void
popdown_credentials_popover (GcalNewCalendarPage *self)
{
  gtk_entry_set_text (self->credentials_user_entry, "");
  gtk_entry_set_text (self->credentials_password_entry, "");

  gtk_popover_popdown (self->credentials_popover);
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
pulse_web_entry (gpointer data)
{
  GcalNewCalendarPage *self = data;

  gtk_entry_progress_pulse (self->calendar_address_entry);
  return G_SOURCE_CONTINUE;
}

static void
sources_discovered_cb (GObject      *source_object,
                       GAsyncResult *result,
                       gpointer      user_data)
{
  GcalNewCalendarPage *self;
  g_autoptr (GPtrArray) sources = NULL;
  g_autoptr (GError) error = NULL;

  GCAL_ENTRY;

  self = GCAL_NEW_CALENDAR_PAGE (user_data);
  sources = gcal_discover_sources_from_uri_finish (result, &error);

  if (error)
    {
      if (g_error_matches (error,
                           GCAL_SOURCE_DISCOVERER_ERROR,
                           GCAL_SOURCE_DISCOVERER_ERROR_UNAUTHORIZED))
        {
          g_debug ("Unauthorized, asking for credentials...");
          gtk_popover_popup (self->credentials_popover);
        }
      else
        {
          g_warning ("Error finding sources: %s", error->message);
          update_url_entry_state (self, ENTRY_STATE_INVALID);
        }
      GCAL_RETURN ();
    }

  g_debug ("Found %u sources", sources->len);

  self->remote_sources = g_steal_pointer (&sources);
  update_url_entry_state (self, ENTRY_STATE_VALID);

  GCAL_EXIT;
}

static gboolean
validate_url_cb (gpointer data)
{
  GcalNewCalendarPage *self = data;
  g_autoptr (SoupURI) uri = NULL;

  GCAL_ENTRY;

  self->validate_url_resource_id = 0;

  uri = soup_uri_new (gtk_entry_get_text (self->calendar_address_entry));

  if (uri != NULL && SOUP_URI_IS_VALID (uri))
    {
      discover_sources (self);
    }
  else
    {
      update_url_entry_state (self, ENTRY_STATE_INVALID);
      g_debug ("Invalid URL passed");
    }

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
      guint i;

      for (i = 0; i < self->remote_sources->len; i++)
        gcal_manager_save_source (manager, g_ptr_array_index (self->remote_sources, i));

      g_clear_pointer (&self->remote_sources, g_ptr_array_unref);
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
  if (button == self->credentials_connect_button)
    discover_sources (self);
  else
    update_url_entry_state (self, ENTRY_STATE_INVALID);

  popdown_credentials_popover (self);
}

static void
on_credential_entry_activate_cb (GtkEntry            *entry,
                                 GcalNewCalendarPage *self)
{
  discover_sources (self);
  popdown_credentials_popover (self);
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

  /* Cleanup previous remote sources */
  g_clear_pointer (&self->remote_sources, g_ptr_array_unref);

  if (self->validate_url_resource_id != 0)
    g_clear_handle_id (&self->validate_url_resource_id, g_source_remove);

  if (text && g_utf8_strlen (text, -1) > 0)
    {
      /*
       * At first, don't bother the user with the login prompt. Only prompt it when
       * it fails.
       */
      self->validate_url_resource_id = g_timeout_add (500, validate_url_cb, self);
      update_url_entry_state (self, ENTRY_STATE_VALIDATING);
    }
  else
    {
      gtk_entry_set_progress_fraction (self->calendar_address_entry, 0);
      update_url_entry_state (self, ENTRY_STATE_EMPTY);
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
on_local_calendar_color_button_color_changed_cb (GtkColorChooser     *chooser,
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
                                 GcalCalendar               *calendar)
{
  GcalNewCalendarPage *self;
  HdyHeaderBar *headerbar;

  GCAL_ENTRY;

  self = GCAL_NEW_CALENDAR_PAGE (page);
  headerbar = gcal_calendar_management_page_get_titlebar (page);

  hdy_header_bar_pack_start (headerbar, self->cancel_button);
  hdy_header_bar_pack_end (headerbar, self->add_button);
  hdy_header_bar_set_show_close_button (headerbar, FALSE);

  GCAL_EXIT;
}
static void
gcal_new_calendar_page_deactivate (GcalCalendarManagementPage *page)
{
  GcalNewCalendarPage *self;
  HdyHeaderBar *headerbar;

  GCAL_ENTRY;

  self = GCAL_NEW_CALENDAR_PAGE (page);
  headerbar = gcal_calendar_management_page_get_titlebar (page);

  gtk_container_remove (GTK_CONTAINER (headerbar), self->cancel_button);
  gtk_container_remove (GTK_CONTAINER (headerbar), self->add_button);
  hdy_header_bar_set_show_close_button (headerbar, TRUE);

  g_clear_object (&self->local_source);
  g_clear_pointer (&self->remote_sources, g_ptr_array_unref);
  update_add_button (self);

  gtk_entry_set_text (self->local_calendar_name_entry, "");
  gtk_entry_set_text (self->calendar_address_entry, "");

  toggle_url_entry_pulsing (self, FALSE);

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

  g_clear_handle_id (&self->calendar_address_id, g_source_remove);
  g_clear_handle_id (&self->validate_url_resource_id, g_source_remove);

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);
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

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/gui/calendar-management/gcal-new-calendar-page.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalNewCalendarPage, add_button);
  gtk_widget_class_bind_template_child (widget_class, GcalNewCalendarPage, calendar_address_entry);
  gtk_widget_class_bind_template_child (widget_class, GcalNewCalendarPage, calendar_file_chooser_button);
  gtk_widget_class_bind_template_child (widget_class, GcalNewCalendarPage, calendar_file_filter);
  gtk_widget_class_bind_template_child (widget_class, GcalNewCalendarPage, cancel_button);
  gtk_widget_class_bind_template_child (widget_class, GcalNewCalendarPage, credentials_cancel_button);
  gtk_widget_class_bind_template_child (widget_class, GcalNewCalendarPage, credentials_connect_button);
  gtk_widget_class_bind_template_child (widget_class, GcalNewCalendarPage, credentials_password_entry);
  gtk_widget_class_bind_template_child (widget_class, GcalNewCalendarPage, credentials_popover);
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
  gtk_widget_class_bind_template_callback (widget_class, on_local_calendar_color_button_color_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_url_entry_text_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_web_description_label_link_activated_cb);
}

static void
gcal_new_calendar_page_init (GcalNewCalendarPage *self)
{
  self->cancellable = g_cancellable_new ();

  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_file_filter_set_name (self->calendar_file_filter, _("Calendar files"));
}
