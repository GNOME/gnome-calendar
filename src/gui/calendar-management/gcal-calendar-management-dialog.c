/* gcal-calendar-management-dialog.c
 *
 * Copyright (C) 2015 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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
 */

#define G_LOG_DOMAIN "GcalCalendarManagementDialog"

#include "gcal-context.h"
#include "gcal-debug.h"
#include "gcal-calendar-management-dialog.h"
#include "gcal-calendar-management-page.h"
#include "gcal-calendars-page.h"
#include "gcal-new-calendar-page.h"
#include "gcal-utils.h"

#include <glib/gi18n.h>
#include <goa/goa.h>
#include <libedataserverui/libedataserverui.h>
#include <libsoup/soup.h>

/**
 * SECTION:gcal-calendar-management-dialog
 * @short_description: Dialog to manage calendars
 * @title:GcalCalendarManagementDialog
 * @stability:unstable
 * @image:gcal-calendar-management-dialog.png
 *
 * #GcalCalendarManagementDialog is the calendar management widget of GNOME
 * Calendar. With it, users can create calendars from local files,
 * local calendars or even import calendars from the Web or their
 * online accounts.
 */

typedef enum
{
  GCAL_PAGE_CALENDARS,
  GCAL_PAGE_NEW_CALENDAR,
  N_PAGES,
} GcalPageType;

struct _GcalCalendarManagementDialog
{
  GtkDialog           parent;

  GtkWidget          *add_button;
  GtkWidget          *back_button;
  GtkWidget          *calendar_color_button;
  GtkWidget          *calendar_visible_check;
  GtkWidget          *cancel_button;
  GtkWidget          *default_check;
  GtkWidget          *edit_grid;
  GtkWidget          *headerbar;
  GtkWidget          *name_entry;
  GtkWidget          *notebook;
  GtkWidget          *remove_button;
  GtkWidget          *stack;
  GtkWidget          *web_source_grid;

  /* edit page widgets */
  GtkWidget          *account_box;
  GtkWidget          *account_label;
  GtkWidget          *account_dim_label;
  GtkWidget          *calendar_url_button;
  GtkWidget          *location_dim_label;
  GtkWidget          *settings_button;

  /* new source details */
  GtkWidget          *calendar_address_entry;
  GtkWidget          *web_sources_listbox;
  GtkWidget          *web_sources_revealer;

  /* credentials dialog */
  GtkWidget          *credentials_cancel_button;
  GtkWidget          *credentials_connect_button;
  GtkWidget          *credentials_dialog;
  GtkWidget          *credentials_password_entry;
  GtkWidget          *credentials_user_entry;

  gint                calendar_address_id;
  gint                validate_url_resource_id;
  gint                notification_timeout_id;

  /* flags */
  GcalCalendarManagementDialogMode mode;
  ESource            *source;
  GList              *remote_sources;
  GcalCalendar       *removed_calendar;
  ESource            *old_default_source;
  GBinding           *title_bind;
  gboolean            prompt_password;

  /* auxiliary */
  GSimpleActionGroup *action_group;

  GcalCalendarManagementPage *pages[N_PAGES];

  GcalContext        *context;
};

typedef enum
{
  GCAL_ACCOUNT_TYPE_EXCHANGE,
  GCAL_ACCOUNT_TYPE_GOOGLE,
  GCAL_ACCOUNT_TYPE_OWNCLOUD,
  GCAL_ACCOUNT_TYPE_NOT_SUPPORTED
} GcalAccountType;

#define ENTRY_PROGRESS_TIMEOUT            100

static void       add_button_clicked                    (GtkWidget            *button,
                                                         gpointer              user_data);

static void       action_widget_activated               (GtkWidget            *widget,
                                                         gpointer              user_data);

static void       back_button_clicked                   (GtkButton            *button,
                                                         gpointer              user_data);

static void       calendar_file_selected                (GtkFileChooser       *button,
                                                         gpointer              user_data);

static void       calendar_visible_check_toggled        (GObject             *object,
                                                         GParamSpec          *pspec,
                                                         gpointer             user_data);

static void       cancel_button_clicked                 (GtkWidget            *button,
                                                         gpointer              user_data);

static void       clear_pages                           (GcalCalendarManagementDialog     *dialog);

static void       color_set                             (GtkColorButton       *button,
                                                         gpointer              user_data);

static void       default_check_toggled                 (GObject             *object,
                                                         GParamSpec          *pspec,
                                                         gpointer             user_data);

static gboolean   description_label_link_activated      (GtkWidget            *widget,
                                                         gchar                *uri,
                                                         gpointer              user_data);

static gboolean   is_goa_source                         (GcalCalendarManagementDialog     *dialog,
                                                         ESource              *source);

static void       name_entry_text_changed               (GObject             *object,
                                                         GParamSpec          *pspec,
                                                         gpointer             user_data);

static void       on_file_activated                     (GSimpleAction       *action,
                                                         GVariant            *param,
                                                         gpointer             user_data);

static void       on_local_activated                    (GSimpleAction       *action,
                                                         GVariant            *param,
                                                         gpointer             user_data);

static void       on_web_activated                      (GSimpleAction       *action,
                                                         GVariant            *param,
                                                         gpointer             user_data);

static void       response_signal                       (GtkDialog           *dialog,
                                                         gint                 response_id,
                                                         gpointer             user_data);

static void       settings_button_clicked               (GtkWidget           *button,
                                                         gpointer             user_data);

static gboolean   pulse_web_entry                       (GcalCalendarManagementDialog    *dialog);

static void       url_entry_text_changed                (GObject             *object,
                                                         GParamSpec          *pspec,
                                                         gpointer             user_data);

static gboolean   validate_url_cb                       (GcalCalendarManagementDialog    *dialog);

static gint       prompt_credentials                    (GcalCalendarManagementDialog    *dialog,
                                                         gchar              **username,
                                                         gchar              **password);

static void       discover_sources_cb                   (GObject             *source,
                                                         GAsyncResult        *result,
                                                         gpointer             user_data);

G_DEFINE_TYPE (GcalCalendarManagementDialog, gcal_calendar_management_dialog, GTK_TYPE_DIALOG)

enum
{
  PROP_0,
  PROP_CONTEXT,
  N_PROPS
};

static GParamSpec *properties[N_PROPS] = { NULL, };

GActionEntry actions[] = {
  {"file",  on_file_activated,  NULL, NULL, NULL},
  {"local", on_local_activated, NULL, NULL, NULL},
  {"web",   on_web_activated,   NULL, NULL, NULL}
};

const gchar*
import_file_extensions[] = {
  ".ical",
  ".ics",
  ".ifb",
  ".icalendar",
  ".vcs"
};

static void
add_button_clicked (GtkWidget *button,
                    gpointer   user_data)
{
  GcalCalendarManagementDialog *self = GCAL_CALENDAR_MANAGEMENT_DIALOG (user_data);
  GcalManager *manager;

  manager = gcal_context_get_manager (self->context);

  if (self->source != NULL)
    {
      /* Commit the new source */
      gcal_manager_save_source (manager, self->source);

      self->source = NULL;

      gcal_calendar_management_dialog_set_mode (GCAL_CALENDAR_MANAGEMENT_DIALOG (user_data),
                                                GCAL_CALENDAR_MANAGEMENT_MODE_NORMAL);
    }

  if (self->remote_sources != NULL)
    {
      GList *l;

      /* Commit each new remote source */
      for (l = self->remote_sources; l != NULL; l = l->next)
        gcal_manager_save_source (manager, l->data);

      g_list_free (self->remote_sources);
      self->remote_sources = NULL;

      /* Go back to overview */
      gcal_calendar_management_dialog_set_mode (GCAL_CALENDAR_MANAGEMENT_DIALOG (user_data),
                                                GCAL_CALENDAR_MANAGEMENT_MODE_NORMAL);
    }
}

static void
action_widget_activated (GtkWidget *widget,
                         gpointer   user_data)
{
  GcalCalendarManagementDialog *self = GCAL_CALENDAR_MANAGEMENT_DIALOG (user_data);
  gint response = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (widget), "response"));

  self->old_default_source = NULL;

  gtk_dialog_response (GTK_DIALOG (user_data), response);
}

static void
back_button_clicked (GtkButton *button,
                     gpointer   user_data)
{
  GcalCalendarManagementDialog *self = GCAL_CALENDAR_MANAGEMENT_DIALOG (user_data);
  GcalManager *manager;

  manager = gcal_context_get_manager (self->context);

  if (gtk_stack_get_visible_child (GTK_STACK (self->stack)) == self->edit_grid)
    {
      /* Save the source before leaving */
      gcal_manager_save_source (manager, self->source);

      /* Release the source ref we acquired */
      g_clear_object (&self->source);
    }

  gcal_calendar_management_dialog_set_mode (GCAL_CALENDAR_MANAGEMENT_DIALOG (user_data), GCAL_CALENDAR_MANAGEMENT_MODE_NORMAL);
}

static void
calendar_visible_check_toggled (GObject    *object,
                                GParamSpec *pspec,
                                gpointer    user_data)
{
  GcalCalendarManagementDialog *self = GCAL_CALENDAR_MANAGEMENT_DIALOG (user_data);
  GcalCalendar *calendar;
  GcalManager *manager;

  manager = gcal_context_get_manager (self->context);

  calendar = gcal_manager_get_calendar_from_source (manager, self->source);
  gcal_calendar_set_visible (calendar, gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (object)));
}

static void
cancel_button_clicked (GtkWidget *button,
                       gpointer   user_data)
{
  GcalCalendarManagementDialog *self = GCAL_CALENDAR_MANAGEMENT_DIALOG (user_data);

  /* Destroy the ongoing created source */
  g_clear_object (&self->source);

  /* Cleanup detected remote sources that weren't added */
  if (self->remote_sources != NULL)
    {
      g_list_free_full (self->remote_sources, g_object_unref);
      self->remote_sources = NULL;
    }

  gcal_calendar_management_dialog_set_mode (GCAL_CALENDAR_MANAGEMENT_DIALOG (user_data), GCAL_CALENDAR_MANAGEMENT_MODE_NORMAL);
}

static void
clear_pages (GcalCalendarManagementDialog *dialog)
{
  GList *list;

  gtk_entry_set_text (GTK_ENTRY (dialog->calendar_address_entry), "");
  gtk_widget_set_sensitive (dialog->add_button, FALSE);

  /* Remove discovered web sources (if any) */
  list = gtk_container_get_children (GTK_CONTAINER (dialog->web_sources_listbox));
  g_list_free_full (list, (GDestroyNotify) gtk_widget_destroy);

  gtk_revealer_set_reveal_child (GTK_REVEALER (dialog->web_sources_revealer), FALSE);
  gtk_widget_hide (dialog->web_sources_revealer);
}

static void
color_set (GtkColorButton *button,
           gpointer        user_data)
{
  GcalCalendarManagementDialog *self;
  ESourceSelectable *extension;
  GdkRGBA color;

  self = GCAL_CALENDAR_MANAGEMENT_DIALOG (user_data);
  gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (button), &color);

  extension = E_SOURCE_SELECTABLE (e_source_get_extension (self->source, E_SOURCE_EXTENSION_CALENDAR));
  e_source_selectable_set_color (extension, gdk_rgba_to_string (&color));
}

static void
default_check_toggled (GObject    *object,
                       GParamSpec *pspec,
                       gpointer    user_data)
{
  GcalCalendarManagementDialog *self = GCAL_CALENDAR_MANAGEMENT_DIALOG (user_data);
  GcalManager *manager;
  ESource *new_default_source;

  manager = gcal_context_get_manager (self->context);

  /* Retrieve the current default source */
  if (self->old_default_source == NULL)
    {
      GcalCalendar *default_calendar = gcal_manager_get_default_calendar (manager);
      self->old_default_source = gcal_calendar_get_source (default_calendar);
    }

  /**
   * Keeps toggling between the
   * current source and the previous
   * default source.
   */
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (object)))
    new_default_source = self->source;
  else
    new_default_source = self->old_default_source;

  gcal_manager_set_default_calendar (manager,
                                     gcal_manager_get_calendar_from_source (manager, new_default_source));
}

static gboolean
description_label_link_activated (GtkWidget *widget,
                                  gchar     *uri,
                                  gpointer   user_data)
{
  GApplication *app = g_application_get_default ();
  gcal_utils_launch_online_accounts_panel (g_application_get_dbus_connection (app), NULL, NULL);
  return TRUE;
}

static gboolean
is_goa_source (GcalCalendarManagementDialog *dialog,
               ESource          *source)
{
  ESource *parent;
  gboolean is_goa;
  GcalManager *manager;

  g_assert (source && E_IS_SOURCE (source));

  manager = gcal_context_get_manager (dialog->context);
  parent = gcal_manager_get_source (manager, e_source_get_parent (source));
  is_goa = e_source_has_extension (parent, E_SOURCE_EXTENSION_GOA);
  g_object_unref (parent);

  return is_goa;
}

static void
name_entry_text_changed (GObject    *object,
                         GParamSpec *pspec,
                         gpointer    user_data)
{
  GcalCalendarManagementDialog *self = GCAL_CALENDAR_MANAGEMENT_DIALOG (user_data);
  gboolean valid = gtk_entry_get_text_length (GTK_ENTRY (object)) > 0;

  /*
   * Callend when the name entry's text
   * is edited. It changes the source's
   * display name, but wait's for the
   * calendar's ::response signal to
   * commit these changes.
   */

  gtk_widget_set_sensitive (self->back_button, valid);
  gtk_widget_set_sensitive (self->add_button, valid);

  if (valid)
    e_source_set_display_name (self->source, gtk_entry_get_text (GTK_ENTRY (self->name_entry)));
}

static void
response_signal (GtkDialog *dialog,
                 gint       response_id,
                 gpointer   user_data)
{
  GcalCalendarManagementDialog *self = GCAL_CALENDAR_MANAGEMENT_DIALOG (dialog);
  GcalManager *manager;

  manager = gcal_context_get_manager (self->context);

  /* Save the source */
  if (self->mode == GCAL_CALENDAR_MANAGEMENT_MODE_EDIT && self->source != NULL)
    {
      gcal_manager_save_source (manager, self->source);
      g_clear_object (&self->source);
    }

  /* Commit the new source; save the current page's source */
  if (self->mode == GCAL_CALENDAR_MANAGEMENT_MODE_NORMAL && response_id == GTK_RESPONSE_APPLY && self->remote_sources != NULL)
      {
        GList *l;

        /* Commit each new remote source */
        for (l = self->remote_sources; l != NULL; l = l->next)
          gcal_manager_save_source (manager, l->data);

        g_list_free (self->remote_sources);
        self->remote_sources = NULL;
      }

  /* Destroy the source when the operation is cancelled */
  if (self->mode == GCAL_CALENDAR_MANAGEMENT_MODE_NORMAL && response_id == GTK_RESPONSE_CANCEL && self->remote_sources != NULL)
    {
      g_list_free_full (self->remote_sources, g_object_unref);
      self->remote_sources = NULL;
    }

  gtk_widget_hide (GTK_WIDGET (dialog));
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
      if (e_source_authentication_get_host (auth) == NULL)
        return FALSE;
    }

  if (has_webdav)
    {
      ESourceWebdav *webdav;

      webdav = e_source_get_extension (source, E_SOURCE_EXTENSION_WEBDAV_BACKEND);

      /* No resource path specified, not a remote source */
      if (e_source_webdav_get_resource_path (webdav) == NULL)
        return FALSE;
    }

  return TRUE;
}

static void
settings_button_clicked (GtkWidget *button,
                         gpointer   user_data)
{
  GcalCalendarManagementDialog *self;
  GApplication *app;
  const gchar *account_id;

  self = GCAL_CALENDAR_MANAGEMENT_DIALOG (user_data);
  /* Selects the account to open */
  account_id = g_object_get_data (G_OBJECT (self->account_label), "account-id");

  app = g_application_get_default ();
  gcal_utils_launch_online_accounts_panel (g_application_get_dbus_connection (app),
                                           (gchar*) account_id,
                                           NULL);
}

#if 0
static void
stack_visible_child_name_changed (GObject    *object,
                                  GParamSpec *pspec,
                                  gpointer    user_data)
{
  GcalCalendarManagementDialog *self;
  GcalCalendar *calendar;
  GcalManager *manager;
  GtkWidget *visible_child;

  self = GCAL_CALENDAR_MANAGEMENT_DIALOG (user_data);
  manager = gcal_context_get_manager (self->context);
  calendar = gcal_manager_get_calendar_from_source (manager, self->source);
  visible_child = gtk_stack_get_visible_child (GTK_STACK (object));

  if (visible_child == self->main_scrolledwindow)
    {
      gtk_header_bar_set_title (GTK_HEADER_BAR (self->headerbar), _("Calendar Settings"));
      gtk_header_bar_set_subtitle (GTK_HEADER_BAR (self->headerbar), NULL);
      gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (self->headerbar), TRUE);
      gtk_widget_set_visible (self->add_button, FALSE);
      gtk_widget_set_visible (self->cancel_button, FALSE);
      gtk_widget_set_visible (self->back_button, FALSE);
    }

  /*
   * Update fields when it goes to the edit page.
   * Here, only widgets that depends on the current
   * source are updated, while indenpendent widgets
   * are updated at #gcal_calendar_management_dialog_set_mode
   */
  if (visible_child == self->edit_grid && self->source != NULL)
    {
      GcalCalendar *default_calendar;
      ESource *default_source;
      gchar *parent_name;
      gboolean creation_mode, is_goa, is_file, is_remote;

      default_calendar = gcal_manager_get_default_calendar (manager);
      default_source = gcal_calendar_get_source (default_calendar);
      creation_mode = (self->mode == GCAL_CALENDAR_MANAGEMENT_MODE_CREATE ||
                       self->mode == GCAL_CALENDAR_MANAGEMENT_MODE_CREATE_WEB);
      is_goa = is_goa_source (GCAL_CALENDAR_MANAGEMENT_DIALOG (user_data), self->source);
      is_file = e_source_has_extension (self->source, E_SOURCE_EXTENSION_LOCAL_BACKEND);
      is_remote = is_remote_source (self->source);

      get_source_parent_name_color (manager, self->source, &parent_name, NULL);

      /* update headerbar buttons */
      gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (self->headerbar), !creation_mode);
      gtk_widget_set_visible (self->calendar_visible_check, !creation_mode);
      gtk_widget_set_visible (self->back_button, !creation_mode);
      gtk_widget_set_visible (self->add_button, creation_mode);
      gtk_widget_set_visible (self->cancel_button, creation_mode);
      gtk_widget_set_visible (self->account_box, is_goa);
      gtk_widget_set_visible (self->calendar_url_button, !is_goa && (is_file || is_remote));

      /* If it's a file, set the file path */
      if (is_file)
        {
          ESourceLocal *local;
          GFile *file;
          gchar *uri;

          local = e_source_get_extension (self->source, E_SOURCE_EXTENSION_LOCAL_BACKEND);
          file = e_source_local_get_custom_file (local);
          uri = g_file_get_uri (file);

          gtk_link_button_set_uri (GTK_LINK_BUTTON (self->calendar_url_button), uri);
          gtk_button_set_label (GTK_BUTTON (self->calendar_url_button), uri);

          g_free (uri);
        }

      /* If it's remote, build the uri */
      if (is_remote)
        {
          ESourceAuthentication *auth;
          ESourceWebdav *webdav;
          g_autoptr (SoupURI) soup;
          g_autofree gchar *uri;

          auth = e_source_get_extension (self->source, E_SOURCE_EXTENSION_AUTHENTICATION);
          webdav = e_source_get_extension (self->source, E_SOURCE_EXTENSION_WEBDAV_BACKEND);
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
          gchar *name;

          get_source_parent_name_color (manager, self->source, &name, NULL);
          gtk_label_set_label (GTK_LABEL (self->account_label), name);
        }

      /* block signals */
      g_signal_handlers_block_by_func (self->calendar_visible_check, calendar_visible_check_toggled, user_data);
      g_signal_handlers_block_by_func (self->calendar_color_button, color_set, user_data);
      g_signal_handlers_block_by_func (self->name_entry, name_entry_text_changed, user_data);

      /* color button */
      gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (self->calendar_color_button),
                                  gcal_calendar_get_color (calendar));

      /* entry */
      gtk_entry_set_text (GTK_ENTRY (self->name_entry), e_source_get_display_name (self->source));

      /* enabled check */
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->calendar_visible_check),
                                    gcal_calendar_get_visible (calendar));

      /* default source check button */
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->default_check), (self->source == default_source));
      gtk_widget_set_visible (self->default_check, !gcal_calendar_is_read_only (calendar));

      /* title */
      if (!creation_mode)
        {
          gtk_header_bar_set_title (GTK_HEADER_BAR (self->headerbar), gcal_calendar_get_name (calendar));
          gtk_header_bar_set_subtitle (GTK_HEADER_BAR (self->headerbar), parent_name);
        }

      /* toggle the remove button */
      gtk_widget_set_visible (self->remove_button, e_source_get_removable (self->source));

      /* unblock signals */
      g_signal_handlers_unblock_by_func (self->calendar_visible_check, calendar_visible_check_toggled, user_data);
      g_signal_handlers_unblock_by_func (self->calendar_color_button, color_set, user_data);
      g_signal_handlers_unblock_by_func (self->name_entry, name_entry_text_changed, user_data);

      g_object_unref (default_source);
      g_free (parent_name);
    }
}
#endif

/* calendar_path_to_name_suggestion:
 * @file: a calendar file reference.
 *
 * Returns: (transfer full): A calendar name.
 */
static gchar*
calendar_path_to_name_suggestion (GFile *file)
{
  g_autofree gchar *unencoded_basename = NULL;
  g_autofree gchar *basename = NULL;
  gchar *ext;
  guint i;

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
calendar_file_selected (GtkFileChooser *button,
                        gpointer        user_data)
{
  g_autofree gchar *display_name = NULL;
  g_autoptr (ESource) source = NULL;
  g_autoptr (GFile) file = NULL;
  GcalCalendarManagementDialog *self;
  ESourceExtension *ext;

  self = GCAL_CALENDAR_MANAGEMENT_DIALOG (user_data);
  file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (button));

  if (!file)
    return;

  /**
   * Create the new source and add the needed
   * extensions.
   */
  source = e_source_new (NULL, NULL, NULL);
  e_source_set_parent (source, "local-stub");

  ext = e_source_get_extension (source, E_SOURCE_EXTENSION_CALENDAR);
  e_source_backend_set_backend_name (E_SOURCE_BACKEND (ext), "local");

  ext = e_source_get_extension (source, E_SOURCE_EXTENSION_LOCAL_BACKEND);
  e_source_local_set_custom_file (E_SOURCE_LOCAL (ext), file);

  /* update the source properties */
  display_name = calendar_path_to_name_suggestion (file);
  e_source_set_display_name (source, display_name);

  /* Jump to the edit page */
  gcal_calendar_management_dialog_set_source (GCAL_CALENDAR_MANAGEMENT_DIALOG (user_data), source);
  gcal_calendar_management_dialog_set_mode (GCAL_CALENDAR_MANAGEMENT_DIALOG (user_data), GCAL_CALENDAR_MANAGEMENT_MODE_CREATE);

  gtk_widget_set_sensitive (self->add_button, TRUE);
}

static gboolean
pulse_web_entry (GcalCalendarManagementDialog *dialog)
{
  gtk_entry_progress_pulse (GTK_ENTRY (dialog->calendar_address_entry));

  dialog->calendar_address_id = g_timeout_add (ENTRY_PROGRESS_TIMEOUT, (GSourceFunc) pulse_web_entry, dialog);

  return FALSE;
}

static void
url_entry_text_changed (GObject    *object,
                        GParamSpec *pspec,
                        gpointer    user_data)
{
  GcalCalendarManagementDialog *self;
  const gchar* text;

  self = GCAL_CALENDAR_MANAGEMENT_DIALOG (user_data);
  text = gtk_entry_get_text (GTK_ENTRY (self->calendar_address_entry));

  if (self->calendar_address_id != 0)
    {
      g_source_remove (self->calendar_address_id);
      self->calendar_address_id = 0;

      gtk_entry_set_progress_fraction (GTK_ENTRY (self->calendar_address_entry), 0);
    }

  if (self->validate_url_resource_id != 0)
    {
      g_source_remove (self->validate_url_resource_id);
      self->validate_url_resource_id = 0;
    }

  if (g_utf8_strlen (text, -1) != 0)
    {
      /* Remove any previous unreleased resource */
      if (self->validate_url_resource_id != 0)
        g_source_remove (self->validate_url_resource_id);

      /*
       * At first, don't bother the user with
       * the login prompt. Only prompt it when
       * it fails.
       */
      self->prompt_password = FALSE;

      self->validate_url_resource_id = g_timeout_add (500, (GSourceFunc) validate_url_cb, user_data);
    }
  else
    {
      gtk_entry_set_progress_fraction (GTK_ENTRY (self->calendar_address_entry), 0);
    }
}

static void
on_file_activated (GSimpleAction *action,
                   GVariant      *param,
                   gpointer       user_data)
{
  GtkWidget *dialog;
  GtkFileFilter *filter;
  gint response;

  /* Dialog */
  dialog = gtk_file_chooser_dialog_new (_("Select a calendar file"),
                                        GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (user_data))),
                                        GTK_FILE_CHOOSER_ACTION_OPEN,
                                        _("Cancel"), GTK_RESPONSE_CANCEL,
                                        _("Open"), GTK_RESPONSE_OK,
                                        NULL);

  g_signal_connect (dialog, "file-activated", G_CALLBACK (calendar_file_selected), user_data);

  /* File filter */
  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("Calendar files"));
  gtk_file_filter_add_mime_type (filter, "text/calendar");

  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

  response = gtk_dialog_run (GTK_DIALOG (dialog));

  if (response == GTK_RESPONSE_OK)
    calendar_file_selected (GTK_FILE_CHOOSER (dialog), user_data);

  gtk_widget_destroy (dialog);
}

static void
on_local_activated (GSimpleAction *action,
                    GVariant      *param,
                    gpointer       user_data)
{
  GcalCalendarManagementDialog *self;
  ESourceExtension *ext;
  ESource *source;

  self = GCAL_CALENDAR_MANAGEMENT_DIALOG (user_data);
  /**
   * Create the new source and add the needed
   * extensions.
   */
  source = e_source_new (NULL, NULL, NULL);
  e_source_set_parent (source, "local-stub");

  ext = e_source_get_extension (source, E_SOURCE_EXTENSION_CALENDAR);
  e_source_backend_set_backend_name (E_SOURCE_BACKEND (ext), "local");

  /* update the source properties */
  e_source_set_display_name (source, _("Unnamed Calendar"));

  /* Jump to the edit page */
  gcal_calendar_management_dialog_set_source (GCAL_CALENDAR_MANAGEMENT_DIALOG (user_data), source);
  gcal_calendar_management_dialog_set_mode (GCAL_CALENDAR_MANAGEMENT_DIALOG (user_data), GCAL_CALENDAR_MANAGEMENT_MODE_CREATE);

  gtk_widget_set_sensitive (self->add_button, TRUE);
}

static void
on_web_activated (GSimpleAction *action,
                  GVariant      *param,
                  gpointer       user_data)
{
  gcal_calendar_management_dialog_set_mode (GCAL_CALENDAR_MANAGEMENT_DIALOG (user_data), GCAL_CALENDAR_MANAGEMENT_MODE_CREATE_WEB);
}

static void
calendar_address_activated (GtkEntry *entry,
                            gpointer  user_data)
{
  g_assert (GCAL_IS_CALENDAR_MANAGEMENT_DIALOG (user_data));

  validate_url_cb (GCAL_CALENDAR_MANAGEMENT_DIALOG (user_data));
}

static gboolean
validate_url_cb (GcalCalendarManagementDialog *dialog)
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

  dialog->validate_url_resource_id = 0;
  soup_uri = NULL;
  host = path = NULL;
  is_file = FALSE;

  /**
   * Remove any reminescent ESources
   * cached before.
   */
  if (dialog->remote_sources != NULL)
    {
      g_list_free_full (dialog->remote_sources, g_object_unref);
      dialog->remote_sources = NULL;
    }

  /* Remove previous results */
  g_list_free_full (gtk_container_get_children (GTK_CONTAINER (dialog->web_sources_listbox)),
                    (GDestroyNotify) gtk_widget_destroy);
  gtk_revealer_set_reveal_child (GTK_REVEALER (dialog->web_sources_revealer), FALSE);

  /* Clear the entry icon */
  gtk_entry_set_icon_from_icon_name (GTK_ENTRY (dialog->calendar_address_entry), GTK_ENTRY_ICON_SECONDARY, NULL);

  /* Get the hostname and file path from the server */
  uri = gtk_entry_get_text (GTK_ENTRY (dialog->calendar_address_entry));
  soup_uri = soup_uri_new (uri);
  if (!soup_uri)
    return FALSE;

  host = soup_uri_get_host (soup_uri);
  path = soup_uri_get_path (soup_uri);

  if (soup_uri_get_scheme (soup_uri) == SOUP_URI_SCHEME_FILE)
    is_file = TRUE;

  g_debug ("Detected host: '%s', path: '%s'", host, path);

  /**
   * Create the new source and add the needed
   * extensions.
   */
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
   * If we're dealing with an absolute file path,
   * there is no need to check the server for more
   * sources.
   */
  if (is_file)
    {
      /* Set the private source so it saves at closing */
      dialog->remote_sources = g_list_append (dialog->remote_sources, source);

      /* Update buttons */
      gtk_widget_set_sensitive (dialog->add_button, source != NULL);

      return FALSE;
    }

  /* Pulse the entry while it performs the check */
  dialog->calendar_address_id = g_timeout_add (ENTRY_PROGRESS_TIMEOUT, (GSourceFunc) pulse_web_entry, dialog);

  /*
   * Try to retrieve the sources without prompting
   * username and password. If we get any error,
   * then it prompts and retry.
   */
  credentials = e_named_parameters_new ();

  if (!dialog->prompt_password)
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
                                 dialog);
    }
  else
    {
      gint response;
      gchar *user, *password;

      g_debug ("No credentials failed, retrying with user credentials...");

      user = password = NULL;
      response = prompt_credentials (dialog, &user, &password);

      /*
       * User entered username and password, let's try
       * with it.
       */
      if (response == GTK_RESPONSE_OK)
        {
          /* User inputted credentials */
          e_named_parameters_set (credentials, E_SOURCE_CREDENTIAL_USERNAME, user);
          e_named_parameters_set (credentials, E_SOURCE_CREDENTIAL_PASSWORD, password);

          e_webdav_discover_sources (source,
                                     uri,
                                     E_WEBDAV_DISCOVER_SUPPORTS_EVENTS,
                                     credentials,
                                     NULL,
                                     discover_sources_cb,
                                     dialog);
        }

      g_free (user);
      g_free (password);
    }

  e_named_parameters_free (credentials);

  return FALSE;
}

static void
credential_button_clicked (GtkWidget *button,
                           gpointer   user_data)
{
  GcalCalendarManagementDialog *self = GCAL_CALENDAR_MANAGEMENT_DIALOG(user_data);

  if (button == self->credentials_cancel_button)
    gtk_dialog_response (GTK_DIALOG (self->credentials_dialog), GTK_RESPONSE_CANCEL);
  else
    gtk_dialog_response (GTK_DIALOG (self->credentials_dialog), GTK_RESPONSE_OK);
}

static void
credential_entry_activate (GtkEntry *entry,
                           gpointer  user_data)
{
  gtk_dialog_response (GTK_DIALOG (user_data), GTK_RESPONSE_OK);
}

static gint
prompt_credentials (GcalCalendarManagementDialog  *dialog,
                    gchar            **username,
                    gchar            **password)
{
  gint response;

  /* Cleanup last credentials */
  gtk_entry_set_text (GTK_ENTRY (dialog->credentials_password_entry), "");
  gtk_entry_set_text (GTK_ENTRY (dialog->credentials_user_entry), "");

  gtk_widget_grab_focus (dialog->credentials_user_entry);

  /* Show the dialog, then destroy it */
  response = gtk_dialog_run (GTK_DIALOG (dialog->credentials_dialog));

  if (response == GTK_RESPONSE_OK)
    {
      if (username != NULL)
        *username = g_strdup (gtk_entry_get_text (GTK_ENTRY (dialog->credentials_user_entry)));

      if (password != NULL)
        *password = g_strdup (gtk_entry_get_text (GTK_ENTRY (dialog->credentials_password_entry)));
    }

  gtk_widget_hide (dialog->credentials_dialog);

  return response;
}

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

static void
check_activated_cb (GtkWidget  *check,
                    GParamSpec *spec,
                    gpointer    user_data)
{
  GcalCalendarManagementDialog *self;
  GtkWidget *row;
  ESource *source;

  self = GCAL_CALENDAR_MANAGEMENT_DIALOG (user_data);

  row = gtk_widget_get_parent (check);
  source = g_object_get_data (G_OBJECT (row), "source");

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check)))
    self->remote_sources = g_list_prepend (self->remote_sources, source);
  else
    self->remote_sources = g_list_remove (self->remote_sources, source);

  gtk_widget_set_sensitive (self->add_button, g_list_length (self->remote_sources) > 0);
}

static void
discover_sources_cb (GObject      *source,
                     GAsyncResult *result,
                     gpointer      user_data)
{
  GcalCalendarManagementDialog *self;
  GSList *discovered_sources, *user_addresses, *aux;
  GError *error;

  self = GCAL_CALENDAR_MANAGEMENT_DIALOG (user_data);
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
       * If it's the first try and things went wrong,
       * retry with the user credentials. Also, it
       * checks for the error code, since we don't
       * really want to retry things on unavailable
       * servers.
       */
      if (!self->prompt_password &&
          (error->code == 14 ||
           error->code == 401 ||
           error->code == 403 ||
           error->code == 405))
        {
          self->prompt_password = TRUE;

          validate_url_cb (GCAL_CALENDAR_MANAGEMENT_DIALOG (user_data));
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
          g_signal_connect (check, "notify::active", G_CALLBACK (check_activated_cb), user_data);

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

#if 0
static void
remove_button_clicked (GtkWidget *button,
                       gpointer   user_data)
{
  GcalCalendarManagementDialog *self = GCAL_CALENDAR_MANAGEMENT_DIALOG (user_data);
  GcalManager *manager;

  manager = gcal_context_get_manager (self->context);

  if (self->source != NULL)
    {
      ESource *removed_source;
      GList *children, *l;
      gchar *str;

      removed_source = self->source;
      self->removed_calendar = gcal_manager_get_calendar_from_source (manager, removed_source);
      self->source = NULL;
      children = gtk_container_get_children (GTK_CONTAINER (self->calendars_listbox));

      gtk_revealer_set_reveal_child (GTK_REVEALER (self->notification), TRUE);

      /* Remove the listbox entry (if any) */
      for (l = children; l != NULL; l = l->next)
        {
          if (g_object_get_data (l->data, "calendar") == self->removed_calendar)
            {
              gtk_widget_destroy (l->data);
              break;
            }
        }

      /* Update notification label */
      str = g_markup_printf_escaped (_("Calendar <b>%s</b> removed"), gcal_calendar_get_name (self->removed_calendar));
      gtk_label_set_markup (GTK_LABEL (self->notification_label), str);

      /* Remove old notifications */
      if (self->notification_timeout_id != 0)
        g_source_remove (self->notification_timeout_id);

      self->notification_timeout_id = g_timeout_add_seconds (5, hide_notification_scheduled, user_data);

      gcal_calendar_set_visible (self->removed_calendar, FALSE);

      g_list_free (children);
      g_free (str);
    }

  gcal_calendar_management_dialog_set_mode (GCAL_CALENDAR_MANAGEMENT_DIALOG (user_data), GCAL_CALENDAR_MANAGEMENT_MODE_NORMAL);
}
#endif

static void
set_page (GcalCalendarManagementDialog *self,
          const gchar                  *page_name,
          gpointer                      page_data)
{
  GcalCalendarManagementPage *current_page;
  guint i;

  GCAL_TRACE_MSG ("Switching to page '%s'", page_name);

  current_page = (GcalCalendarManagementPage*) gtk_stack_get_visible_child (GTK_STACK (self->stack));

  for (i = 0; i < N_PAGES; i++)
    {
      GcalCalendarManagementPage *page = self->pages[i];

      if (g_strcmp0 (page_name, gcal_calendar_management_page_get_name (page)) != 0)
        continue;

      gtk_stack_set_visible_child (GTK_STACK (self->stack), GTK_WIDGET (page));
      gtk_header_bar_set_title (GTK_HEADER_BAR (self->headerbar),
                                gcal_calendar_management_page_get_title (page));
      gcal_calendar_management_page_activate (page, page_data);
      break;
    }

  gcal_calendar_management_page_deactivate (current_page);
}

/*
 * Callbacks
 */

static void
on_page_switched_cb (GcalCalendarManagementPage   *page,
                     const gchar                  *next_page,
                     gpointer                      page_data,
                     GcalCalendarManagementDialog *self)
{
  GCAL_ENTRY;

  set_page (self, next_page, page_data);

  GCAL_EXIT;
}

static void
setup_context (GcalCalendarManagementDialog *self)
{
  const struct {
    GcalPageType page_type;
    GType        gtype;
  } pages[] = {
    { GCAL_PAGE_CALENDARS, GCAL_TYPE_CALENDARS_PAGE },
    { GCAL_PAGE_NEW_CALENDAR, GCAL_TYPE_NEW_CALENDAR_PAGE },
  };
  gint i;

  GCAL_ENTRY;

  for (i = 0; i < N_PAGES; i++)
    {
      GcalCalendarManagementPage *page;

      page = g_object_new (pages[i].gtype,
                           "context", self->context,
                           NULL);
      gtk_widget_show (GTK_WIDGET (page));

      gtk_stack_add_titled (GTK_STACK (self->stack),
                            GTK_WIDGET (page),
                            gcal_calendar_management_page_get_name (page),
                            gcal_calendar_management_page_get_title (page));

      g_signal_connect_object (page,
                               "switch-page",
                               G_CALLBACK (on_page_switched_cb),
                               self,
                               0);

      self->pages[i] = page;
    }

  set_page (self, "calendars", NULL);

  GCAL_EXIT;
}

static void
gcal_calendar_management_dialog_constructed (GObject *object)
{
  GcalCalendarManagementDialog *self;

  self = GCAL_CALENDAR_MANAGEMENT_DIALOG (object);

  G_OBJECT_CLASS (gcal_calendar_management_dialog_parent_class)->constructed (object);

  /* widget responses */
  gtk_dialog_set_default_response (GTK_DIALOG (object), GTK_RESPONSE_CANCEL);

  g_object_set_data (G_OBJECT (self->remove_button), "response", GINT_TO_POINTER (GCAL_RESPONSE_REMOVE_SOURCE));

  /* Action group */
  self->action_group = g_simple_action_group_new ();
  gtk_widget_insert_action_group (GTK_WIDGET (object), "source", G_ACTION_GROUP (self->action_group));

  g_action_map_add_action_entries (G_ACTION_MAP (self->action_group), actions, G_N_ELEMENTS (actions), object);

  /* setup titlebar */
  gtk_window_set_titlebar (GTK_WINDOW (object), self->headerbar);
}

static void
gcal_calendar_management_dialog_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GcalCalendarManagementDialog *self = (GcalCalendarManagementDialog *) object;

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
gcal_calendar_management_dialog_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GcalCalendarManagementDialog *self = (GcalCalendarManagementDialog *) object;

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_assert (self->context == NULL);
      self->context = g_value_dup_object (value);
      setup_context (self);
      g_object_notify_by_pspec (object, properties[PROP_CONTEXT]);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_calendar_management_dialog_class_init (GcalCalendarManagementDialogClass *klass)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;

  /**
   * Since we cannot guarantee that the
   * type system registered ESourceLocal,
   * it must be ensured at least here.
   */
  g_type_ensure (E_TYPE_SOURCE_LOCAL);

  object_class = G_OBJECT_CLASS (klass);
  object_class->constructed = gcal_calendar_management_dialog_constructed;
  object_class->get_property = gcal_calendar_management_dialog_get_property;
  object_class->set_property = gcal_calendar_management_dialog_set_property;

  properties[PROP_CONTEXT] = g_param_spec_object ("context",
                                                  "Context",
                                                  "The context object of the application",
                                                  GCAL_TYPE_CONTEXT,
                                                  G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  widget_class = GTK_WIDGET_CLASS (klass);

  /* bind things for/from the template class */
  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/org/gnome/calendar/calendar-management-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalCalendarManagementDialog, account_box);
  gtk_widget_class_bind_template_child (widget_class, GcalCalendarManagementDialog, account_label);
  gtk_widget_class_bind_template_child (widget_class, GcalCalendarManagementDialog, add_button);
  gtk_widget_class_bind_template_child (widget_class, GcalCalendarManagementDialog, back_button);
  gtk_widget_class_bind_template_child (widget_class, GcalCalendarManagementDialog, calendar_address_entry);
  gtk_widget_class_bind_template_child (widget_class, GcalCalendarManagementDialog, calendar_color_button);
  gtk_widget_class_bind_template_child (widget_class, GcalCalendarManagementDialog, calendar_url_button);
  gtk_widget_class_bind_template_child (widget_class, GcalCalendarManagementDialog, calendar_visible_check);
  gtk_widget_class_bind_template_child (widget_class, GcalCalendarManagementDialog, cancel_button);
  gtk_widget_class_bind_template_child (widget_class, GcalCalendarManagementDialog, credentials_cancel_button);
  gtk_widget_class_bind_template_child (widget_class, GcalCalendarManagementDialog, credentials_connect_button);
  gtk_widget_class_bind_template_child (widget_class, GcalCalendarManagementDialog, credentials_dialog);
  gtk_widget_class_bind_template_child (widget_class, GcalCalendarManagementDialog, credentials_password_entry);
  gtk_widget_class_bind_template_child (widget_class, GcalCalendarManagementDialog, credentials_user_entry);
  gtk_widget_class_bind_template_child (widget_class, GcalCalendarManagementDialog, default_check);
  gtk_widget_class_bind_template_child (widget_class, GcalCalendarManagementDialog, edit_grid);
  gtk_widget_class_bind_template_child (widget_class, GcalCalendarManagementDialog, headerbar);
  gtk_widget_class_bind_template_child (widget_class, GcalCalendarManagementDialog, location_dim_label);
  gtk_widget_class_bind_template_child (widget_class, GcalCalendarManagementDialog, name_entry);
  gtk_widget_class_bind_template_child (widget_class, GcalCalendarManagementDialog, remove_button);
  gtk_widget_class_bind_template_child (widget_class, GcalCalendarManagementDialog, settings_button);
  gtk_widget_class_bind_template_child (widget_class, GcalCalendarManagementDialog, stack);
  gtk_widget_class_bind_template_child (widget_class, GcalCalendarManagementDialog, web_source_grid);
  gtk_widget_class_bind_template_child (widget_class, GcalCalendarManagementDialog, web_sources_listbox);
  gtk_widget_class_bind_template_child (widget_class, GcalCalendarManagementDialog, web_sources_revealer);

  gtk_widget_class_bind_template_callback (widget_class, add_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, action_widget_activated);
  gtk_widget_class_bind_template_callback (widget_class, back_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, calendar_address_activated);
  gtk_widget_class_bind_template_callback (widget_class, calendar_file_selected);
  gtk_widget_class_bind_template_callback (widget_class, calendar_visible_check_toggled);
  gtk_widget_class_bind_template_callback (widget_class, cancel_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, credential_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, credential_entry_activate);
  gtk_widget_class_bind_template_callback (widget_class, color_set);
  gtk_widget_class_bind_template_callback (widget_class, default_check_toggled);
  gtk_widget_class_bind_template_callback (widget_class, description_label_link_activated);
  gtk_widget_class_bind_template_callback (widget_class, name_entry_text_changed);
  gtk_widget_class_bind_template_callback (widget_class, response_signal);
  gtk_widget_class_bind_template_callback (widget_class, settings_button_clicked);
  //gtk_widget_class_bind_template_callback (widget_class, stack_visible_child_name_changed);
  gtk_widget_class_bind_template_callback (widget_class, url_entry_text_changed);
}

static void
gcal_calendar_management_dialog_init (GcalCalendarManagementDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

/**
 * gcal_calendar_management_dialog_set_mode:
 * @dialog: a #GcalCalendarManagementDialog
 * @mode: a #GcalCalendarManagementDialogMode
 *
 * Set the source dialog mode. Creation mode means that a new
 * calendar will be created, while edit mode means a calendar
 * will be edited.
 */
void
gcal_calendar_management_dialog_set_mode (GcalCalendarManagementDialog     *dialog,
                                          GcalCalendarManagementDialogMode  mode)
{
  GcalCalendarManagementDialogMode previous_mode = dialog->mode;

  dialog->mode = mode;

  /* Cleanup old data */
  clear_pages (dialog);

  switch (mode)
    {
    case GCAL_CALENDAR_MANAGEMENT_MODE_CREATE:
      gtk_header_bar_set_title (GTK_HEADER_BAR (dialog->headerbar), _("Add Calendar"));
      gtk_header_bar_set_subtitle (GTK_HEADER_BAR (dialog->headerbar), NULL);
      gtk_stack_set_visible_child (GTK_STACK (dialog->stack), dialog->edit_grid);
      break;

    case GCAL_CALENDAR_MANAGEMENT_MODE_CREATE_WEB:
      gtk_header_bar_set_title (GTK_HEADER_BAR (dialog->headerbar), _("Add Calendar"));
      gtk_header_bar_set_subtitle (GTK_HEADER_BAR (dialog->headerbar), NULL);
      gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (dialog->headerbar), FALSE);
      gtk_stack_set_visible_child (GTK_STACK (dialog->stack), dialog->web_source_grid);
      gtk_widget_set_visible (dialog->add_button, TRUE);
      gtk_widget_set_visible (dialog->cancel_button, TRUE);
      break;

    case GCAL_CALENDAR_MANAGEMENT_MODE_EDIT:
      /* Bind title */
      if (dialog->title_bind == NULL)
        {
          dialog->title_bind = g_object_bind_property (dialog->name_entry, "text", dialog->headerbar, "title",
                                                       G_BINDING_DEFAULT);
        }

      gtk_stack_set_visible_child (GTK_STACK (dialog->stack), dialog->edit_grid);
      break;

    case GCAL_CALENDAR_MANAGEMENT_MODE_NORMAL:
      /* Free any bindings left behind */
      g_clear_pointer (&dialog->title_bind, g_binding_unbind);

      gtk_header_bar_set_title (GTK_HEADER_BAR (dialog->headerbar), _("Calendar Settings"));
      gtk_header_bar_set_subtitle (GTK_HEADER_BAR (dialog->headerbar), NULL);
      gtk_stack_set_visible_child (GTK_STACK (dialog->stack), GTK_WIDGET (dialog->pages[GCAL_PAGE_CALENDARS]));
      break;

    default:
      g_assert_not_reached ();
    }

  //if (previous_mode == mode)
  //  stack_visible_child_name_changed (G_OBJECT (dialog->stack), NULL, dialog);
}

/**
 * gcal_calendar_management_dialog_set_source:
 * @dialog: a #GcalCalendarManagementDialog
 * @source: an #ESource
 *
 * Sets the source to be edited by the user.
 */
void
gcal_calendar_management_dialog_set_source (GcalCalendarManagementDialog *dialog,
                                            ESource                      *source)
{
  g_return_if_fail (source && E_IS_SOURCE (source));

  g_set_object (&dialog->source, source);
}
