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

#include "config.h"

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
  AdwNavigationPage   parent;

  GtkWidget          *add_button;
  GtkEntry           *calendar_address_entry;
  EntryState          calendar_address_entry_state;
  AdwMessageDialog   *credentials_dialog;
  GtkEntry           *credentials_password_entry;
  GtkEntry           *credentials_user_entry;
  GtkColorDialogButton *local_calendar_color_button;
  AdwEntryRow        *local_calendar_name_row;
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

static void          cancel_validation_discover                               (GcalNewCalendarPage *self);

static void          gcal_calendar_management_page_iface_init    (GcalCalendarManagementPageInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GcalNewCalendarPage, gcal_new_calendar_page, ADW_TYPE_NAVIGATION_PAGE,
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

static void
update_add_button (GcalNewCalendarPage *self)
{
  g_autofree gchar *add_button_label = NULL;
  gboolean valid;
  uint32_t n_calendars;

  valid = (self->local_source != NULL || self->remote_sources != NULL) &&
      self->calendar_address_entry_state != ENTRY_STATE_VALIDATING &&
      self->calendar_address_entry_state != ENTRY_STATE_INVALID;

  gtk_widget_set_sensitive (self->add_button, valid);

  n_calendars = 0;
  if (self->remote_sources)
    n_calendars += self->remote_sources->len;
  else if (self->local_source)
    n_calendars++;

  if (n_calendars > 0)
    {
      add_button_label = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE,
                                                       "Add Calendar",
                                                       "Add %1$u Calendars",
                                                       n_calendars),
                                          n_calendars);
    }
  else
    {
      add_button_label = g_strdup (C_("button", "Add Calendar"));
    }

  gtk_button_set_label (GTK_BUTTON (self->add_button), add_button_label);
}

static void
update_local_source (GcalNewCalendarPage *self)
{
  g_autofree gchar *calendar_name = NULL;

  g_clear_object (&self->local_source);

  calendar_name = g_strdup (gtk_editable_get_text (GTK_EDITABLE (self->local_calendar_name_row)));
  calendar_name = g_strstrip (calendar_name);

  if (calendar_name && g_utf8_strlen (calendar_name, -1) > 0)
    {
      g_autofree gchar *color_string = NULL;
      ESourceExtension *ext;
      const GdkRGBA *color;
      ESource *source;

      color = gtk_color_dialog_button_get_rgba (self->local_calendar_color_button);
      color_string = gdk_rgba_to_string (color);

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
  if (pulsing && self->calendar_address_id == 0)
    {
      gtk_entry_set_progress_fraction (self->calendar_address_entry, 0.1);
      self->calendar_address_id = g_timeout_add (ENTRY_PROGRESS_TIMEOUT, pulse_web_entry, self);
    }
  else if (!pulsing && self->calendar_address_id != 0)
    {
      gtk_entry_set_progress_fraction (self->calendar_address_entry, 0.0);
      g_clear_handle_id (&self->calendar_address_id, g_source_remove);
    }
}

static void
update_url_entry_state (GcalNewCalendarPage *self,
                        EntryState           state,
                        const gchar         *error_message)
{
  self->calendar_address_entry_state = state;

  if (state == ENTRY_STATE_INVALID)
    {
      gtk_widget_add_css_class (GTK_WIDGET (self->calendar_address_entry), "error");
      if (error_message)
        {
          g_autofree gchar *multiline_message = NULL;
          g_auto(GStrv) split_message = NULL;

          split_message = g_strsplit (error_message, ": ", -1);
          multiline_message = g_strjoinv (":\n", split_message);

          gtk_entry_set_icon_from_icon_name (self->calendar_address_entry,
                                             GTK_ENTRY_ICON_SECONDARY,
                                             "dialog-error");
          gtk_entry_set_icon_tooltip_text (self->calendar_address_entry,
                                           GTK_ENTRY_ICON_SECONDARY,
                                           multiline_message);
        }
    }
  else
    {
      gtk_widget_remove_css_class (GTK_WIDGET (self->calendar_address_entry), "error");
      gtk_entry_set_icon_from_icon_name (self->calendar_address_entry,
                                         GTK_ENTRY_ICON_SECONDARY,
                                         NULL);
      gtk_entry_set_icon_tooltip_text (self->calendar_address_entry,
                                       GTK_ENTRY_ICON_SECONDARY,
                                       NULL);
    }

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

  url = gtk_editable_get_text (GTK_EDITABLE (self->calendar_address_entry));
  username = gtk_editable_get_text (GTK_EDITABLE (self->credentials_user_entry));
  password = gtk_editable_get_text (GTK_EDITABLE (self->credentials_password_entry));

  gcal_discover_sources_from_uri (url,
                                  username,
                                  password,
                                  self->cancellable,
                                  sources_discovered_cb,
                                  self);
  update_url_entry_state (self, ENTRY_STATE_VALIDATING, NULL);

  GCAL_EXIT;
}

/*
 * Callbacks
 */

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
          GtkRoot *root;

          g_debug ("Unauthorized, asking for credentials...");

          root = gtk_widget_get_root (GTK_WIDGET (self));
          gtk_window_set_transient_for (GTK_WINDOW (self->credentials_dialog), GTK_WINDOW (root));
          gtk_window_present (GTK_WINDOW (self->credentials_dialog));
        }
      /*
       * If the operation was cancelled by the user, no error message
       * or error style class will be applied
       */
      else if (!g_error_matches (error,
                                 G_IO_ERROR,
                                 G_IO_ERROR_CANCELLED))
        {
          g_warning ("Error finding sources: %s", error->message);
          update_url_entry_state (self, ENTRY_STATE_INVALID, error->message);
        }
      GCAL_RETURN ();
    }

  g_debug ("Found %u sources", sources->len);

  self->remote_sources = g_steal_pointer (&sources);
  update_url_entry_state (self, ENTRY_STATE_VALID, NULL);

  GCAL_EXIT;
}

static gboolean
validate_url_cb (gpointer data)
{
  GcalNewCalendarPage *self = data;
  g_autoptr (GUri) guri = NULL;

  GCAL_ENTRY;

  update_url_entry_state (self, ENTRY_STATE_VALIDATING, NULL);
  self->validate_url_resource_id = 0;

  guri = g_uri_parse (gtk_editable_get_text (GTK_EDITABLE (self->calendar_address_entry)), SOUP_HTTP_URI_FLAGS | G_URI_FLAGS_PARSE_RELAXED, NULL);

  if (guri != NULL && g_uri_get_scheme (guri) && g_uri_get_host (guri))
    {
      discover_sources (self);
    }
  else
    {
      update_url_entry_state (self, ENTRY_STATE_INVALID, _("The URL you have entered appears to be invalid."));
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

  /* Commit each new remote source */
  if (self->remote_sources)
    {
      guint i;

      for (i = 0; i < self->remote_sources->len; i++)
        gcal_manager_save_source (manager, g_ptr_array_index (self->remote_sources, i));

      g_clear_pointer (&self->remote_sources, g_ptr_array_unref);
    }

  else
    {
      gcal_manager_save_source (manager, self->local_source);
    }

  gcal_calendar_management_page_switch_page (GCAL_CALENDAR_MANAGEMENT_PAGE (self),
                                             "calendars",
                                             NULL);
}

static void
on_calendar_address_activated_cb (GtkEntry            *entry,
                                  GcalNewCalendarPage *self)
{
  cancel_validation_discover (self);
  validate_url_cb (self);
}

static void
on_credentials_dialog_response_cb (AdwMessageDialog    *dialog,
                                 const gchar         *response,
                                 GcalNewCalendarPage *self)
{
  if (g_str_equal (response, "connect"))
    discover_sources (self);
  else if (g_str_equal (response, "cancel"))
    update_url_entry_state (self, ENTRY_STATE_INVALID,
                            _("A valid user and password are required to connect to this calendar."));

  gtk_editable_set_text (GTK_EDITABLE (self->credentials_user_entry), "");
  gtk_editable_set_text (GTK_EDITABLE (self->credentials_password_entry), "");
}

static void
cancel_validation_discover (GcalNewCalendarPage *self)
{
  if (self->calendar_address_id != 0)
    {
      gtk_entry_set_progress_fraction (self->calendar_address_entry, 0);
      g_clear_handle_id (&self->calendar_address_id, g_source_remove);
    }

  /* Cleanup previous remote sources */
  g_clear_pointer (&self->remote_sources, g_ptr_array_unref);

  if (self->validate_url_resource_id != 0)
    g_clear_handle_id (&self->validate_url_resource_id, g_source_remove);

  /* Cancel a validation in case it is on-going */
  if (self->calendar_address_entry_state == ENTRY_STATE_VALIDATING)
    {
      g_cancellable_cancel (self->cancellable);
      g_clear_object (&self->cancellable);
      self->cancellable = g_cancellable_new ();
    }
}

static void
on_url_entry_text_changed_cb (GtkEntry            *entry,
                              GParamSpec          *pspec,
                              GcalNewCalendarPage *self)
{
  const gchar *text;

  GCAL_ENTRY;

  text = gtk_editable_get_text (GTK_EDITABLE (entry));

  cancel_validation_discover (self);

  if (text && g_utf8_strlen (text, -1) > 0)
    {
      /*
       * Only validate the text after the timeout, as we don't want to bother them
       * as soon as they type into the entry.
       */
      self->validate_url_resource_id = g_timeout_add (750, validate_url_cb, self);
    }
  else
    {
      gtk_entry_set_progress_fraction (self->calendar_address_entry, 0);
      update_url_entry_state (self, ENTRY_STATE_EMPTY, NULL);
    }

  GCAL_EXIT;
}

static void
on_local_calendar_name_row_text_changed_cb (AdwEntryRow         *entry_row,
                                            GParamSpec          *pspec,
                                            GcalNewCalendarPage *self)
{
  update_local_source (self);
}

static void
on_local_calendar_color_button_rgba_changed_cb (GtkColorChooser     *chooser,
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

  gcal_utils_launch_gnome_settings (connection, "online-accounts", NULL);
}


/*
 * GcalCalendarManagementPage iface
 */

static void
gcal_new_calendar_page_deactivate (GcalCalendarManagementPage *page)
{
  GcalNewCalendarPage *self;

  GCAL_ENTRY;

  self = GCAL_NEW_CALENDAR_PAGE (page);

  g_clear_object (&self->local_source);
  g_clear_pointer (&self->remote_sources, g_ptr_array_unref);
  update_add_button (self);

  gtk_editable_set_text (GTK_EDITABLE (self->local_calendar_name_row), "");
  gtk_editable_set_text (GTK_EDITABLE (self->calendar_address_entry), "");

  toggle_url_entry_pulsing (self, FALSE);

  GCAL_EXIT;
}

static void
gcal_calendar_management_page_iface_init (GcalCalendarManagementPageInterface *iface)
{
  iface->deactivate = gcal_new_calendar_page_deactivate;
}

/*
 * GObject overrides
 */

static void
gcal_new_calendar_page_dispose (GObject *object)
{
  GcalNewCalendarPage *self = (GcalNewCalendarPage *)object;

  gtk_widget_dispose_template (GTK_WIDGET (self), GCAL_TYPE_NEW_CALENDAR_PAGE);

  G_OBJECT_CLASS (gcal_new_calendar_page_parent_class)->dispose (object);
}

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

  object_class->dispose = gcal_new_calendar_page_dispose;
  object_class->finalize = gcal_new_calendar_page_finalize;
  object_class->get_property = gcal_new_calendar_page_get_property;
  object_class->set_property = gcal_new_calendar_page_set_property;

  g_object_class_override_property (object_class, PROP_CONTEXT, "context");

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/gui/calendar-management/gcal-new-calendar-page.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalNewCalendarPage, add_button);
  gtk_widget_class_bind_template_child (widget_class, GcalNewCalendarPage, calendar_address_entry);
  gtk_widget_class_bind_template_child (widget_class, GcalNewCalendarPage, credentials_dialog);
  gtk_widget_class_bind_template_child (widget_class, GcalNewCalendarPage, credentials_password_entry);
  gtk_widget_class_bind_template_child (widget_class, GcalNewCalendarPage, credentials_user_entry);
  gtk_widget_class_bind_template_child (widget_class, GcalNewCalendarPage, local_calendar_color_button);
  gtk_widget_class_bind_template_child (widget_class, GcalNewCalendarPage, local_calendar_name_row);
  gtk_widget_class_bind_template_child (widget_class, GcalNewCalendarPage, web_sources_listbox);
  gtk_widget_class_bind_template_child (widget_class, GcalNewCalendarPage, web_sources_revealer);

  gtk_widget_class_bind_template_callback (widget_class, on_add_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_calendar_address_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_credentials_dialog_response_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_local_calendar_name_row_text_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_local_calendar_color_button_rgba_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_url_entry_text_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_web_description_label_link_activated_cb);
}

static void
gcal_new_calendar_page_init (GcalNewCalendarPage *self)
{
  self->cancellable = g_cancellable_new ();

  gtk_widget_init_template (GTK_WIDGET (self));
}
