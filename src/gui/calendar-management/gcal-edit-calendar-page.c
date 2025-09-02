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

#include <glib/gi18n.h>

struct _GcalEditCalendarPage
{
  AdwNavigationPage   parent;

  AdwActionRow       *account_row;
  GtkColorDialogButton *calendar_color_button;
  AdwSwitchRow       *calendar_visible_row;
  AdwActionRow       *calendar_url_row;
  AdwEntryRow        *name_entry;
  GtkWidget          *remove_group;

  GcalCalendar       *calendar;

  GcalContext        *context;
};

static void          gcal_calendar_management_page_iface_init    (GcalCalendarManagementPageInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GcalEditCalendarPage, gcal_edit_calendar_page, ADW_TYPE_NAVIGATION_PAGE,
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
  ESource *source;
  gboolean is_remote;
  gboolean is_file;
  gboolean is_goa;

  self->calendar = g_object_ref (calendar);

  is_goa = is_goa_calendar (self, calendar);
  source = gcal_calendar_get_source (calendar);
  is_file = e_source_has_extension (source, E_SOURCE_EXTENSION_LOCAL_BACKEND);
  is_remote = is_remote_source (source);

  GCAL_TRACE_MSG ("Calendar '%s' is GOA: %d; is file: %d; is remote: %d",
                  gcal_calendar_get_name (calendar),
                  is_goa,
                  is_file,
                  is_remote);

  gtk_widget_set_visible (GTK_WIDGET (self->account_row), is_goa);
  gtk_widget_set_visible (GTK_WIDGET (self->calendar_url_row), !is_goa);

  /* If it's a file, set the file path */
  if (is_file)
    {
      g_autofree gchar *basename = NULL;
      g_autofree gchar *text = NULL;
      g_autofree gchar *uri = NULL;
      ESourceLocal *local;
      GFile *file;

      local = e_source_get_extension (source, E_SOURCE_EXTENSION_LOCAL_BACKEND);
      file = e_source_local_get_custom_file (local);
      basename = g_file_get_basename (file);
      uri = g_file_get_uri (file);

      text = g_strdup_printf ("<a title=\"%1$s\" href=\"%2$s\">%1$s</a>", basename, uri);
      adw_action_row_set_subtitle (self->calendar_url_row, text);

      gtk_accessible_update_property (GTK_ACCESSIBLE (self->calendar_url_row),
                                      GTK_ACCESSIBLE_PROPERTY_LABEL, basename,
                                      -1);
    }

  /* If it's remote, build the uri */
  if (is_remote)
    {
      g_autoptr (GUri) guri = NULL;
      g_autofree gchar *text = NULL;
      g_autofree gchar *uri = NULL;
      ESourceWebdav *webdav;

      webdav = e_source_get_extension (source, E_SOURCE_EXTENSION_WEBDAV_BACKEND);
      guri = e_source_webdav_dup_uri (webdav);
      uri = g_uri_to_string_partial (guri, G_URI_HIDE_PASSWORD);
      text = g_strdup_printf ("<a title=\"%1$s\" href=\"%1$s\">%1$s</a>", uri);

      adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->calendar_url_row), _("Calendar URL"));
      adw_action_row_set_subtitle (self->calendar_url_row, text);

      gtk_accessible_update_property (GTK_ACCESSIBLE (self->calendar_url_row),
                                      GTK_ACCESSIBLE_PROPERTY_LABEL, uri,
                                      -1);
    }

  if (is_goa)
    {
      const gchar *parent_name = e_source_get_display_name (gcal_calendar_get_parent_source (calendar));

      adw_action_row_set_subtitle (self->account_row, parent_name);
    }

  if (!is_goa && !is_remote && !is_file)
    {
      adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->calendar_url_row), _("Location"));
      adw_action_row_set_subtitle (self->calendar_url_row, _("On This Computer"));
    }

  gtk_color_dialog_button_set_rgba (self->calendar_color_button, gcal_calendar_get_color (calendar));
  gtk_editable_set_text (GTK_EDITABLE (self->name_entry), gcal_calendar_get_name (calendar));
  adw_switch_row_set_active (self->calendar_visible_row, gcal_calendar_get_visible (calendar));

  gtk_widget_set_visible (self->remove_group, e_source_get_removable (source));
}

/*
 * Callbacks
 */

static void
on_calendar_color_changed_cb (GtkColorDialogButton *button,
                              GParamSpec           *pspec,
                              GcalEditCalendarPage *self)
{
  gcal_calendar_set_color (self->calendar, gtk_color_dialog_button_get_rgba (button));
}

static void
on_calendar_visibility_changed_cb (AdwSwitchRow         *row,
                                   GParamSpec           *pspec,
                                   GcalEditCalendarPage *self)
{
  gcal_calendar_set_visible (self->calendar, adw_switch_row_get_active (row));
}

static void
on_remove_button_row_activated_cb (GtkButton            *button,
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
  gcal_utils_launch_gnome_settings (g_application_get_dbus_connection (app),
                                    "online-accounts",
                                    e_source_goa_get_account_id (goa));

  GCAL_EXIT;
}


/*
 * GcalCalendarManagementPage iface
 */

static void
gcal_edit_calendar_page_activate (GcalCalendarManagementPage *page,
                                  GcalCalendar               *calendar)
{
  GCAL_ENTRY;

  setup_calendar (GCAL_EDIT_CALENDAR_PAGE (page), calendar);

  GCAL_EXIT;
}

static void
gcal_edit_calendar_page_deactivate (GcalCalendarManagementPage *page)
{
  GcalEditCalendarPage *self;
  GcalManager *manager;

  GCAL_ENTRY;

  self = GCAL_EDIT_CALENDAR_PAGE (page);

  gcal_calendar_set_name (self->calendar, gtk_editable_get_text (GTK_EDITABLE (self->name_entry)));

  manager = gcal_context_get_manager (self->context);
  gcal_manager_save_source (manager, gcal_calendar_get_source (self->calendar));

  g_clear_object (&self->calendar);

  GCAL_EXIT;
}

static void
gcal_calendar_management_page_iface_init (GcalCalendarManagementPageInterface *iface)
{
  iface->activate = gcal_edit_calendar_page_activate;
  iface->deactivate = gcal_edit_calendar_page_deactivate;
}


/*
 * GObject overrides
 */

static void
gcal_edit_calendar_page_dispose (GObject *object)
{
  GcalEditCalendarPage *self = GCAL_EDIT_CALENDAR_PAGE (object);

  if (self->calendar)
    gcal_calendar_set_name (self->calendar, gtk_editable_get_text (GTK_EDITABLE (self->name_entry)));

  G_OBJECT_CLASS (gcal_edit_calendar_page_parent_class)->dispose (object);
}

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

  object_class->dispose = gcal_edit_calendar_page_dispose;
  object_class->finalize = gcal_edit_calendar_page_finalize;
  object_class->get_property = gcal_edit_calendar_page_get_property;
  object_class->set_property = gcal_edit_calendar_page_set_property;

  g_object_class_override_property (object_class, PROP_CONTEXT, "context");

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/gui/calendar-management/gcal-edit-calendar-page.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalEditCalendarPage, account_row);
  gtk_widget_class_bind_template_child (widget_class, GcalEditCalendarPage, calendar_color_button);
  gtk_widget_class_bind_template_child (widget_class, GcalEditCalendarPage, calendar_url_row);
  gtk_widget_class_bind_template_child (widget_class, GcalEditCalendarPage, calendar_visible_row);
  gtk_widget_class_bind_template_child (widget_class, GcalEditCalendarPage, name_entry);
  gtk_widget_class_bind_template_child (widget_class, GcalEditCalendarPage, remove_group);

  gtk_widget_class_bind_template_callback (widget_class, on_calendar_color_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_calendar_visibility_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_remove_button_row_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_settings_button_clicked_cb);
}

static void
gcal_edit_calendar_page_init (GcalEditCalendarPage *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
