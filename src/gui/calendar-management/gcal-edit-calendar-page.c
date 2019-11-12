/* gcal-edit-calendar-page.c
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

#define G_LOG_DOMAIN "GcalEditCalendarPage"

#include "gcal-context.h"
#include "gcal-calendar-management-page.h"
#include "gcal-debug.h"
#include "gcal-edit-calendar-page.h"
#include "gcal-utils.h"

struct _GcalEditCalendarPage
{
  GtkBox              parent;

  GtkWidget          *account_box;
  GtkLabel           *account_label;
  GtkWidget          *account_dim_label;
  GtkWidget          *back_button;
  GtkColorChooser    *calendar_color_button;
  GtkToggleButton    *calendar_visible_check;
  GtkWidget          *calendar_url_button;
  GtkToggleButton    *default_check;
  GtkWidget          *location_dim_label;
  GtkEntry           *name_entry;
  GtkWidget          *remove_button;

  GcalCalendar       *calendar;

  GcalContext        *context;
};

static void          gcal_calendar_management_page_iface_init    (GcalCalendarManagementPageInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GcalEditCalendarPage, gcal_edit_calendar_page, GTK_TYPE_BOX,
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

static gboolean
is_goa_calendar (GcalEditCalendarPage *self,
                 GcalCalendar         *calendar)
{
  g_autoptr (ESource) parent = NULL;
  GcalManager *manager;
  ESource *source;

  g_assert (calendar && GCAL_IS_CALENDAR (calendar));

  manager = gcal_context_get_manager (self->context);
  source = gcal_calendar_get_source (calendar);
  parent = gcal_manager_get_source (manager, e_source_get_parent (source));

  return e_source_has_extension (parent, E_SOURCE_EXTENSION_GOA);
}

static gboolean
is_remote_source (ESource *source)
{
  gboolean has_webdav, has_auth;

  g_assert (E_IS_SOURCE (source));

  has_webdav = e_source_has_extension (source, E_SOURCE_EXTENSION_WEBDAV_BACKEND);
  has_auth = e_source_has_extension (source, E_SOURCE_EXTENSION_AUTHENTICATION);

  if (!has_webdav || !has_auth)
    return FALSE;

  if (has_auth)
    {
      ESourceAuthentication *auth;

      auth = e_source_get_extension (source, E_SOURCE_EXTENSION_AUTHENTICATION);

      /* No host is set, it's not a remote source */
      if (!e_source_authentication_get_host (auth))
        return FALSE;
    }

  if (has_webdav)
    {
      ESourceWebdav *webdav;

      webdav = e_source_get_extension (source, E_SOURCE_EXTENSION_WEBDAV_BACKEND);

      /* No resource path specified, not a remote source */
      if (!e_source_webdav_get_resource_path (webdav))
        return FALSE;
    }

  return TRUE;
}

static void
setup_calendar (GcalEditCalendarPage *self,
                GcalCalendar         *calendar)
{
  g_autofree gchar *parent_name = NULL;
  GcalCalendar *default_calendar;
  GcalManager *manager;
  ESource *default_source;
  ESource *source;
  gboolean is_remote;
  gboolean is_file;
  gboolean is_goa;

  self->calendar = g_object_ref (calendar);

  manager = gcal_context_get_manager (self->context);
  default_calendar = gcal_manager_get_default_calendar (manager);
  default_source = gcal_calendar_get_source (default_calendar);
  is_goa = is_goa_calendar (self, calendar);
  source = gcal_calendar_get_source (calendar);
  is_file = e_source_has_extension (source, E_SOURCE_EXTENSION_LOCAL_BACKEND);
  is_remote = is_remote_source (source);

  get_source_parent_name_color (manager, source, &parent_name, NULL);

  gtk_widget_set_visible (self->account_box, is_goa);
  gtk_widget_set_visible (self->calendar_url_button, !is_goa && (is_file || is_remote));

  /* If it's a file, set the file path */
  if (is_file)
    {
      g_autofree gchar *uri = NULL;
      ESourceLocal *local;
      GFile *file;

      local = e_source_get_extension (source, E_SOURCE_EXTENSION_LOCAL_BACKEND);
      file = e_source_local_get_custom_file (local);
      uri = g_file_get_uri (file);

      gtk_link_button_set_uri (GTK_LINK_BUTTON (self->calendar_url_button), uri);
      gtk_button_set_label (GTK_BUTTON (self->calendar_url_button), uri);
    }

  /* If it's remote, build the uri */
  if (is_remote)
    {
      ESourceAuthentication *auth;
      g_autoptr (SoupURI) soup = NULL;
      g_autofree gchar *uri = NULL;
      ESourceWebdav *webdav;

      auth = e_source_get_extension (source, E_SOURCE_EXTENSION_AUTHENTICATION);
      webdav = e_source_get_extension (source, E_SOURCE_EXTENSION_WEBDAV_BACKEND);
      soup = e_source_webdav_dup_soup_uri (webdav);
      uri = g_strdup_printf ("%s://%s:%d%s",
                             soup_uri_get_scheme (soup),
                             e_source_authentication_get_host (auth),
                             e_source_authentication_get_port (auth),
                             e_source_webdav_get_resource_path (webdav));

      gtk_link_button_set_uri (GTK_LINK_BUTTON (self->calendar_url_button), uri);
      gtk_button_set_label (GTK_BUTTON (self->calendar_url_button), uri);
    }

  if (is_goa)
    {
      g_autofree gchar *name = NULL;

      get_source_parent_name_color (manager, source, &name, NULL);
      gtk_label_set_label (self->account_label, name);
    }

  gtk_color_chooser_set_rgba (self->calendar_color_button, gcal_calendar_get_color (calendar));
  gtk_entry_set_text (self->name_entry, gcal_calendar_get_name (calendar));
  gtk_toggle_button_set_active (self->calendar_visible_check, gcal_calendar_get_visible (calendar));

  gtk_toggle_button_set_active (self->default_check, source == default_source);
  gtk_widget_set_visible (GTK_WIDGET (self->default_check), !gcal_calendar_is_read_only (calendar));
  gtk_widget_set_visible (self->remove_button, e_source_get_removable (source));
}

static void
update_calendar (GcalEditCalendarPage *self)
{
  GcalManager *manager;
  GdkRGBA color;

  GCAL_ENTRY;

  g_assert (self->calendar != NULL);

  manager = gcal_context_get_manager (self->context);
  gtk_color_chooser_get_rgba (self->calendar_color_button, &color);

  gcal_calendar_set_name (self->calendar, gtk_entry_get_text (self->name_entry));
  gcal_calendar_set_color (self->calendar, &color);

  if (gtk_toggle_button_get_active (self->default_check))
    gcal_manager_set_default_calendar (manager, self->calendar);

  GCAL_EXIT;
}


/*
 * Callbacks
 */

static void
on_back_button_clicked_cb (GtkButton            *back_button,
                           GcalEditCalendarPage *self)
{
  GcalCalendarManagementPage *page;

  GCAL_ENTRY;

  page = GCAL_CALENDAR_MANAGEMENT_PAGE (self);
  gcal_calendar_management_page_switch_page (page, "calendars", NULL);

  GCAL_EXIT;
}

static void
on_remove_button_clicked_cb (GtkButton            *button,
                             GcalEditCalendarPage *self)
{
  GcalCalendarManagementPage *page;

  GCAL_ENTRY;

  page = GCAL_CALENDAR_MANAGEMENT_PAGE (self);
  gcal_calendar_management_page_switch_page (page, "calendars", self->calendar);

  GCAL_EXIT;
}

static void
on_settings_button_clicked_cb (GtkWidget            *button,
                               GcalEditCalendarPage *self)
{
  g_autoptr (ESource) parent = NULL;
  GApplication *app;
  GcalManager *manager;
  ESourceGoa *goa;
  ESource *source;

  GCAL_ENTRY;

  manager = gcal_context_get_manager (self->context);
  source = gcal_calendar_get_source (self->calendar);
  parent = gcal_manager_get_source (manager, e_source_get_parent (source));

  g_assert (e_source_has_extension (parent, E_SOURCE_EXTENSION_GOA));
  goa = e_source_get_extension (parent, E_SOURCE_EXTENSION_GOA);

  app = g_application_get_default ();
  gcal_utils_launch_online_accounts_panel (g_application_get_dbus_connection (app),
                                           e_source_goa_get_account_id (goa),
                                           NULL);

  GCAL_EXIT;
}


/*
 * GcalCalendarManagementPage iface
 */

static const gchar*
gcal_edit_calendar_page_get_name (GcalCalendarManagementPage *page)
{
  return "edit-calendar";
}

static const gchar*
gcal_edit_calendar_page_get_title (GcalCalendarManagementPage *page)
{
  GcalEditCalendarPage *self = GCAL_EDIT_CALENDAR_PAGE (page);

  return self->calendar ? gcal_calendar_get_name (self->calendar) : "";
}

static void
gcal_edit_calendar_page_activate (GcalCalendarManagementPage *page,
                                  GcalCalendar               *calendar)
{
  GcalEditCalendarPage *self;
  GtkHeaderBar *headerbar;

  self = GCAL_EDIT_CALENDAR_PAGE (page);
  headerbar = gcal_calendar_management_page_get_titlebar (page);
  gtk_header_bar_pack_start (headerbar, self->back_button);

  setup_calendar (self, calendar);
}

static void
gcal_edit_calendar_page_deactivate (GcalCalendarManagementPage *page)
{
  GcalEditCalendarPage *self;
  GtkHeaderBar *headerbar;
  GcalManager *manager;

  GCAL_ENTRY;

  self = GCAL_EDIT_CALENDAR_PAGE (page);
  headerbar = gcal_calendar_management_page_get_titlebar (page);
  gtk_container_remove (GTK_CONTAINER (headerbar), self->back_button);

  update_calendar (self);

  manager = gcal_context_get_manager (self->context);
  gcal_manager_save_source (manager, gcal_calendar_get_source (self->calendar));

  g_clear_object (&self->calendar);

  GCAL_EXIT;
}

static void
gcal_calendar_management_page_iface_init (GcalCalendarManagementPageInterface *iface)
{
  iface->get_name = gcal_edit_calendar_page_get_name;
  iface->get_title = gcal_edit_calendar_page_get_title;
  iface->activate = gcal_edit_calendar_page_activate;
  iface->deactivate = gcal_edit_calendar_page_deactivate;
}


/*
 * GObject overrides
 */

static void
gcal_edit_calendar_page_finalize (GObject *object)
{
  GcalEditCalendarPage *self = (GcalEditCalendarPage *)object;

  g_clear_object (&self->calendar);
  g_clear_object (&self->context);

  G_OBJECT_CLASS (gcal_edit_calendar_page_parent_class)->finalize (object);
}

static void
gcal_edit_calendar_page_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  GcalEditCalendarPage *self = GCAL_EDIT_CALENDAR_PAGE (object);

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
gcal_edit_calendar_page_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  GcalEditCalendarPage *self = GCAL_EDIT_CALENDAR_PAGE (object);

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
gcal_edit_calendar_page_class_init (GcalEditCalendarPageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gcal_edit_calendar_page_finalize;
  object_class->get_property = gcal_edit_calendar_page_get_property;
  object_class->set_property = gcal_edit_calendar_page_set_property;

  g_object_class_override_property (object_class, PROP_CONTEXT, "context");

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/gui/calendar-management/gcal-edit-calendar-page.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalEditCalendarPage, account_box);
  gtk_widget_class_bind_template_child (widget_class, GcalEditCalendarPage, account_label);
  gtk_widget_class_bind_template_child (widget_class, GcalEditCalendarPage, back_button);
  gtk_widget_class_bind_template_child (widget_class, GcalEditCalendarPage, calendar_color_button);
  gtk_widget_class_bind_template_child (widget_class, GcalEditCalendarPage, calendar_url_button);
  gtk_widget_class_bind_template_child (widget_class, GcalEditCalendarPage, calendar_visible_check);
  gtk_widget_class_bind_template_child (widget_class, GcalEditCalendarPage, default_check);
  gtk_widget_class_bind_template_child (widget_class, GcalEditCalendarPage, location_dim_label);
  gtk_widget_class_bind_template_child (widget_class, GcalEditCalendarPage, name_entry);
  gtk_widget_class_bind_template_child (widget_class, GcalEditCalendarPage, remove_button);

  gtk_widget_class_bind_template_callback (widget_class, on_back_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_remove_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_settings_button_clicked_cb);
}

static void
gcal_edit_calendar_page_init (GcalEditCalendarPage *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
