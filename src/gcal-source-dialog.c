/* gcal-source-dialog.c
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

#include "gcal-source-dialog.h"

#include "e-source-local.h"
#include "gcal-utils.h"

#include <glib/gi18n.h>
#include <libedataserverui/libedataserverui.h>

typedef struct
{
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

  /* new source details */
  GtkWidget          *author_label;
  GtkWidget          *calendar_address_entry;
  GtkWidget          *web_author_label;
  GtkWidget          *web_details_frame;
  GtkWidget          *web_new_calendar_name_entry;
  GtkWidget          *web_source_grid;
  GtkWidget          *web_sources_listbox;
  GtkWidget          *web_sources_revealer;

  gint                validate_url_resource_id;
  gint                calendar_address_id;

  /* overview widgets */
  GtkWidget          *add_calendar_menu_button;
  GtkWidget          *calendars_listbox;
  GtkWidget          *online_accounts_listbox;

  /* flags */
  GcalSourceDialogMode mode;
  ESource            *source;
  ESource            *remote_source;
  ESource            *old_default_source;
  GBinding           *title_bind;
  gboolean           *prompt_password;

  /* manager */
  GcalManager        *manager;
} GcalSourceDialogPrivate;

struct _GcalSourceDialog
{
  GtkDialog parent;

  /*< private >*/
  GcalSourceDialogPrivate *priv;
};

#define ENTRY_PROGRESS_TIMEOUT            100

static void       add_source                             (GcalManager         *manager,
                                                          ESource             *source,
                                                          gboolean             enabled,
                                                          gpointer             user_data);

static void       action_widget_activated               (GtkWidget            *widget,
                                                         gpointer              user_data);

static void       back_button_clicked                   (GtkButton            *button,
                                                         gpointer              user_data);

static void       calendar_file_selected                (GtkFileChooserButton *button,
                                                         gpointer              user_data);

static void       calendar_listbox_row_activated        (GtkListBox          *box,
                                                         GtkListBoxRow       *row,
                                                         gpointer             user_data);

static void       calendar_visible_check_toggled        (GObject             *object,
                                                         GParamSpec          *pspec,
                                                         gpointer             user_data);

static void       clear_pages                           (GcalSourceDialog     *dialog);

static void       color_set                             (GtkColorButton       *button,
                                                         gpointer              user_data);

static void       default_check_toggled                 (GObject             *object,
                                                         GParamSpec          *pspec,
                                                         gpointer             user_data);

static gboolean   description_label_link_activated      (GtkWidget            *widget,
                                                         gchar                *uri,
                                                         gpointer              user_data);

static void       display_header_func                   (GtkListBoxRow        *row,
                                                         GtkListBoxRow        *before,
                                                         gpointer              user_data);

static gboolean   is_goa_source                         (GcalSourceDialog     *dialog,
                                                         ESource              *source);

static GtkWidget* make_row_from_source                  (GcalSourceDialog    *dialog,
                                                         ESource             *source);

static void       name_entry_text_changed               (GObject             *object,
                                                         GParamSpec          *pspec,
                                                         gpointer             user_data);

static void       new_name_entry_text_changed           (GObject             *object,
                                                         GParamSpec          *pspec,
                                                         gpointer             user_data);

static void       response_signal                       (GtkDialog           *dialog,
                                                         gint                 response_id,
                                                         gpointer             user_data);

static void       setup_source_details                  (GcalSourceDialog     *dialog,
                                                         ESource              *source);

static void       stack_visible_child_name_changed      (GObject             *object,
                                                         GParamSpec          *pspec,
                                                         gpointer             user_data);

static gboolean   pulse_web_entry                       (GcalSourceDialog    *dialog);

static void       url_entry_text_changed                (GObject             *object,
                                                         GParamSpec          *pspec,
                                                         gpointer             user_data);

static gboolean   validate_url_cb                       (GcalSourceDialog    *dialog);

static gint       prompt_credentials                    (GcalSourceDialog    *dialog,
                                                         gchar              **username,
                                                         gchar              **password);

static void       discover_sources_cb                   (GObject             *source,
                                                         GAsyncResult        *result,
                                                         gpointer             user_data);

G_DEFINE_TYPE_WITH_PRIVATE (GcalSourceDialog, gcal_source_dialog, GTK_TYPE_DIALOG)

static void
add_source (GcalManager *manager,
            ESource     *source,
            gboolean     enabled,
            gpointer     user_data)
{
  GcalSourceDialogPrivate *priv = GCAL_SOURCE_DIALOG (user_data)->priv;
  GList *children, *l;
  gboolean contains_source;

  children = gtk_container_get_children (GTK_CONTAINER (priv->calendars_listbox));
  contains_source = FALSE;

  for (l = children; l != NULL; l = l->next)
    {
      if (g_object_get_data (l->data, "source") == source)
        contains_source = TRUE;
    }

  if (!contains_source)
    {
      GtkWidget *row;

      row = make_row_from_source (GCAL_SOURCE_DIALOG (user_data), source);
      g_object_set_data (G_OBJECT (row), "source", source);

      gtk_container_add (GTK_CONTAINER (priv->calendars_listbox), row);
    }

  g_list_free (children);
}

/**
 * action_widget_activated:
 * @widget: the button which emited the signal.
 * @user_data: a {@link GcalSourceDialog} instance.
 *
 * Emit a response when action buttons
 * are clicked.
 *
 * Returns:
 */
static void
action_widget_activated (GtkWidget *widget,
                         gpointer   user_data)
{
  GcalSourceDialogPrivate *priv = GCAL_SOURCE_DIALOG (user_data)->priv;
  gint response;

  response = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (widget), "response"));

  priv->old_default_source = NULL;

  gtk_dialog_response (GTK_DIALOG (user_data), response);
}

/**
 * back_button_clicked:
 *
 * Returns to the previous page.
 *
 * Returns:
 */
static void
back_button_clicked (GtkButton *button,
                     gpointer   user_data)
{
  GcalSourceDialogPrivate *priv = GCAL_SOURCE_DIALOG (user_data)->priv;

  /*
   * TODO: when editing a GOA calendar, it should
   * go back to the GOA calendar selection page.
   */

  if (g_strcmp0 (gtk_stack_get_visible_child_name (GTK_STACK (priv->stack)), "edit") == 0)
    {
      // Save the source before leaving
      gcal_manager_save_source (priv->manager, priv->source);

      // Release the source ref we acquired
      g_object_unref (priv->source);
      priv->source = NULL;

      gcal_source_dialog_set_mode (GCAL_SOURCE_DIALOG (user_data), GCAL_SOURCE_DIALOG_MODE_NORMAL);
    }
}

static void
calendar_visible_check_toggled (GObject    *object,
                                GParamSpec *pspec,
                                gpointer    user_data)
{
  GcalSourceDialogPrivate *priv = GCAL_SOURCE_DIALOG (user_data)->priv;

  g_assert (priv->source != NULL);

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (object)))
    gcal_manager_enable_source (priv->manager, priv->source);
  else
    gcal_manager_disable_source (priv->manager, priv->source);
}

/**
 * clear_pages:
 *
 * Clear local and web pages.
 *
 * Returns:
 */
static void
clear_pages (GcalSourceDialog *dialog)
{
  GcalSourceDialogPrivate *priv = dialog->priv;
  GList *list;

  gtk_entry_set_text (GTK_ENTRY (priv->calendar_address_entry), "");
  gtk_widget_set_sensitive (priv->add_button, FALSE);

  // Remove discovered web sources (if any)
  list = gtk_container_get_children (GTK_CONTAINER (priv->web_sources_listbox));
  g_list_free_full (list, (GDestroyNotify) gtk_widget_destroy);

  gtk_revealer_set_reveal_child (GTK_REVEALER (priv->web_sources_revealer), FALSE);

  gtk_widget_hide (priv->web_details_frame);
}

static void
color_set (GtkColorButton *button,
           gpointer        user_data)
{
  GcalSourceDialogPrivate *priv = GCAL_SOURCE_DIALOG (user_data)->priv;
  ESourceSelectable *extension;
  GdkRGBA color;

  gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (button), &color);

  extension = E_SOURCE_SELECTABLE (e_source_get_extension (priv->source, E_SOURCE_EXTENSION_CALENDAR));

  e_source_selectable_set_color (extension, gdk_rgba_to_string (&color));
}

static void
default_check_toggled (GObject    *object,
                       GParamSpec *pspec,
                       gpointer    user_data)
{
  GcalSourceDialogPrivate *priv = GCAL_SOURCE_DIALOG (user_data)->priv;

  /* Retrieve the current default source */
  if (priv->old_default_source == NULL)
    {
      priv->old_default_source = gcal_manager_get_default_source (priv->manager);
      g_object_unref (priv->old_default_source);
    }

  /**
   * Keeps toggling between the
   * current source and the previous
   * default source.
   */
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (object)))
    gcal_manager_set_default_source (priv->manager, priv->source);
  else
    gcal_manager_set_default_source (priv->manager, priv->old_default_source);
}


/**
 * description_label_link_activated:
 *
 * Show GNOME Control Center when
 * the label's link is pressed.
 *
 * Returns:
 */
static gboolean
description_label_link_activated (GtkWidget *widget,
                                  gchar     *uri,
                                  gpointer   user_data)
{
  gchar *command[] = {"gnome-control-center", "online-accounts", NULL};
  g_spawn_async (NULL, command, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);

  return TRUE;
}

/**
 * display_header_func:
 *
 * Shows a separator before each row.
 *
 */
static void
display_header_func (GtkListBoxRow *row,
                     GtkListBoxRow *before,
                     gpointer       user_data)
{
  if (before != NULL)
    {
      GtkWidget *header;

      header = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_widget_show (header);

      gtk_list_box_row_set_header (row, header);
    }
}

/**
 * is_goa_source:
 *
 * Checks whether the source comes from
 * a online account.
 *
 * Returns: %TRUE if the source came from a GOA account
 */
static gboolean
is_goa_source (GcalSourceDialog *dialog,
               ESource          *source)
{
  GcalSourceDialogPrivate *priv = dialog->priv;
  ESource *parent;
  gboolean is_goa;

  g_assert (source && E_IS_SOURCE (source));

  parent = gcal_manager_get_source (priv->manager, e_source_get_parent (source));

  is_goa = e_source_has_extension (parent, E_SOURCE_EXTENSION_GOA);

  g_object_unref (parent);

  return is_goa;
}

/**
 * make_row_from_source:
 *
 * Create a GtkListBoxRow for a given
 * ESource.
 *
 * Returns: (transfer full) the new row
 */
static GtkWidget*
make_row_from_source (GcalSourceDialog *dialog,
                      ESource          *source)
{
  GcalSourceDialogPrivate *priv = dialog->priv;
  GtkWidget *bottom_label;
  GtkWidget *top_label;
  GdkPixbuf *pixbuf;
  GtkWidget *icon;
  GtkWidget *grid;
  GtkWidget *row;
  GdkRGBA color;
  gchar *parent_name;

  get_source_parent_name_color (priv->manager, source, &parent_name, NULL);
  row = gtk_list_box_row_new ();

  /* main box */
  grid = g_object_new (GTK_TYPE_GRID,
                       "border-width", 6,
                       "column-spacing", 12,
                       NULL);

  /* source color icon */
  gdk_rgba_parse (&color, get_color_name_from_source (source));
  pixbuf = get_circle_pixbuf_from_color (&color, 24);
  icon = gtk_image_new_from_pixbuf (pixbuf);

  /* source name label */
  top_label = g_object_new (GTK_TYPE_LABEL,
                            "label", e_source_get_display_name (source),
                            "xalign", 0.0,
                            "hexpand", TRUE,
                            NULL);

  /* parent source name label */
  bottom_label = g_object_new (GTK_TYPE_LABEL,
                               "label", parent_name,
                               "xalign", 0.0,
                               "hexpand", TRUE,
                               NULL);
  gtk_style_context_add_class (gtk_widget_get_style_context (bottom_label), "dim-label");


  gtk_grid_attach (GTK_GRID (grid), icon, 0, 0, 1, 2);
  gtk_grid_attach (GTK_GRID (grid), top_label, 1, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), bottom_label, 1, 1, 1, 1);
  gtk_container_add (GTK_CONTAINER (row), grid);

  gtk_widget_show_all (row);

  g_object_unref (pixbuf);
  g_free (parent_name);

  return row;
}

/**
 * name_entry_text_changed:
 *
 * Callend when the name entry's text
 * is edited. It changes the source's
 * display name, but wait's for the
 * calendar's ::response signal to
 * commit these changes.
 *
 * Returns:
 */
static void
name_entry_text_changed (GObject    *object,
                         GParamSpec *pspec,
                         gpointer    user_data)
{
  GcalSourceDialogPrivate *priv = GCAL_SOURCE_DIALOG (user_data)->priv;

  e_source_set_display_name (priv->source, gtk_entry_get_text (GTK_ENTRY (priv->name_entry)));
}

/**
 * new_name_entry_text_changed:
 *
 * Callend when the name entry of a
 * new to-be-added source is edited.
 * It changes the source's display
 * name, but wait's for the calendar's
 * 'response' signal to commit these
 * changes.
 *
 * Returns:
 */
static void
new_name_entry_text_changed (GObject    *object,
                             GParamSpec *pspec,
                             gpointer    user_data)
{
  GcalSourceDialogPrivate *priv = GCAL_SOURCE_DIALOG (user_data)->priv;
  ESource *source = NULL;

  if (GTK_WIDGET (object) == priv->web_new_calendar_name_entry)
    source = priv->remote_source;

  if (source != NULL)
    e_source_set_display_name (source, gtk_entry_get_text (GTK_ENTRY (object)));
}

/**
 * response_signal:
 *
 * Save the source when the dialog
 * is close.
 *
 * Returns:
 */
static void
response_signal (GtkDialog *dialog,
                 gint       response_id,
                 gpointer   user_data)
{
  GcalSourceDialogPrivate *priv = GCAL_SOURCE_DIALOG (dialog)->priv;

  /* save the source */
  if (priv->mode == GCAL_SOURCE_DIALOG_MODE_EDIT && priv->source != NULL)
    {
      gcal_manager_save_source (priv->manager, priv->source);

      g_object_unref (priv->source);
      priv->source = NULL;
    }

  /* commit the new source */
  if (priv->mode == GCAL_SOURCE_DIALOG_MODE_NORMAL && response_id == GTK_RESPONSE_APPLY)
    {
      /* save the current page's source */
      if (priv->remote_source != NULL)
        gcal_manager_save_source (priv->manager, priv->remote_source);
    }

  /* Destroy the source when the operation is cancelled */
  if (priv->mode == GCAL_SOURCE_DIALOG_MODE_NORMAL && response_id == GTK_RESPONSE_CANCEL)
    {
      if (priv->remote_source != NULL)
        g_clear_object (&(priv->remote_source));
    }
}

static void
stack_visible_child_name_changed (GObject    *object,
                                  GParamSpec *pspec,
                                  gpointer    user_data)
{
  GcalSourceDialogPrivate *priv = GCAL_SOURCE_DIALOG (user_data)->priv;
  const gchar *visible_name;
  gboolean is_main;

  visible_name = gtk_stack_get_visible_child_name (GTK_STACK (object));
  is_main = g_strcmp0 (visible_name, "main") == 0;

  // Show the '<' button everywhere except "main" page
  gtk_widget_set_visible (priv->back_button, !is_main);

  if (is_main)
    {
      gtk_header_bar_set_title (GTK_HEADER_BAR (priv->headerbar), _("Calendar Settings"));
      gtk_header_bar_set_subtitle (GTK_HEADER_BAR (priv->headerbar), NULL);
    }

  // Update fields when it goes to the edit page.
  if (g_strcmp0 (visible_name, "edit") == 0 && priv->source != NULL)
    {
      ESource *default_source;
      gchar *parent_name;
      GdkRGBA color;

      default_source = gcal_manager_get_default_source (priv->manager);

      get_source_parent_name_color (priv->manager, priv->source, &parent_name, NULL);

      /* block signals */
      g_signal_handlers_block_by_func (priv->calendar_visible_check, calendar_visible_check_toggled, user_data);
      g_signal_handlers_block_by_func (priv->calendar_color_button, color_set, user_data);
      g_signal_handlers_block_by_func (priv->name_entry, name_entry_text_changed, user_data);

      /* color button */
      gdk_rgba_parse (&color, get_color_name_from_source (priv->source));
      gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (priv->calendar_color_button), &color);

      /* entry */
      gtk_entry_set_text (GTK_ENTRY (priv->name_entry), e_source_get_display_name (priv->source));

      // enabled check
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->calendar_visible_check),
                                    gcal_manager_source_enabled (priv->manager, priv->source));

      /* default source check button */
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->default_check), (priv->source == default_source));
      gtk_widget_set_visible (priv->default_check, !gcal_manager_is_client_writable (priv->manager, priv->source));

      /* title */
      gtk_header_bar_set_title (GTK_HEADER_BAR (priv->headerbar), e_source_get_display_name (priv->source));
      gtk_header_bar_set_subtitle (GTK_HEADER_BAR (priv->headerbar), parent_name);

      /* toggle the remove button */
      gtk_widget_set_visible (priv->remove_button, e_source_get_removable (priv->source));

      /* unblock signals */
      g_signal_handlers_unblock_by_func (priv->calendar_visible_check, calendar_visible_check_toggled, user_data);
      g_signal_handlers_unblock_by_func (priv->calendar_color_button, color_set, user_data);
      g_signal_handlers_unblock_by_func (priv->name_entry, name_entry_text_changed, user_data);

      g_object_unref (default_source);
      g_free (parent_name);
    }
}

/**
 * calendar_file_selected:
 *
 * Opens a file selector dialog and
 * parse the resulting selection.
 *
 * Returns:
 */
static void
calendar_file_selected (GtkFileChooserButton *button,
                        gpointer              user_data)
{
  GcalSourceDialogPrivate *priv = GCAL_SOURCE_DIALOG (user_data)->priv;
  ESourceExtension *ext;
  ESource *source;
  GFile *file;

  //file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (priv->select_file_button));

  if (file == NULL)
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
  e_source_set_display_name (source, g_file_get_basename (file));

  /* Update buttons */
  gtk_widget_set_sensitive (priv->add_button, source != NULL);
}

/**
 * calendar_listbox_row_activated:
 *
 * Edits the selected calendar for the
 * 'Calendars' listbox or goes to the
 * calendar selection for the Online
 * Accounts listbox.
 *
 * Returns:
 */
static void
calendar_listbox_row_activated (GtkListBox    *box,
                                GtkListBoxRow *row,
                                gpointer       user_data)
{
  GcalSourceDialogPrivate *priv = GCAL_SOURCE_DIALOG (user_data)->priv;

  g_assert (row != NULL);

  /*
   * For non-GOA calendars, show the edit page
   * directly.
   */
  if (GTK_WIDGET (box) == priv->calendars_listbox)
    {
      ESource *source = g_object_get_data (G_OBJECT (row), "source");

      gcal_source_dialog_set_source (GCAL_SOURCE_DIALOG (user_data), source);
      gcal_source_dialog_set_mode (GCAL_SOURCE_DIALOG (user_data), GCAL_SOURCE_DIALOG_MODE_EDIT);
    }
}

/**
 * setup_source_details:
 *
 * Setup the details frame of a given
 * calendar in proccess of creation.
 *
 * Returns:
 */
static void
setup_source_details (GcalSourceDialog *dialog,
                      ESource          *source)
{
  GcalSourceDialogPrivate *priv = dialog->priv;
  gchar *email;

  /* Calendar name */
  gtk_entry_set_text (GTK_ENTRY (priv->web_new_calendar_name_entry), e_source_get_display_name (source));

  /* Email field */
  email = gcal_manager_query_client_data (priv->manager, source, CAL_BACKEND_PROPERTY_CAL_EMAIL_ADDRESS);

  if (email != NULL)
    {
      gtk_label_set_markup (GTK_LABEL (priv->web_author_label), email);
    }
  else
    {
      gchar *text = g_strdup_printf ("<i><small>%s</small></i>", _("No author"));
      gtk_label_set_markup (GTK_LABEL (priv->web_author_label), text);

      g_free (text);
    }

  gtk_widget_show_all (priv->web_details_frame);
}

/**
 * pulse_web_entry:
 *
 * Update the url's entry with a pulse fraction.
 *
 * Returns: FALSE
 */
static gboolean
pulse_web_entry (GcalSourceDialog *dialog)
{
  GcalSourceDialogPrivate *priv = dialog->priv;

  gtk_entry_progress_pulse (GTK_ENTRY (priv->calendar_address_entry));

  priv->calendar_address_id = g_timeout_add (ENTRY_PROGRESS_TIMEOUT, (GSourceFunc) pulse_web_entry, dialog);

  return FALSE;
}

/**
 * url_entry_text_changed:
 *
 * Performs a validation of the URL
 * 1 second after the user inputs.
 *
 * Returns:
 */
static void
url_entry_text_changed (GObject    *object,
                        GParamSpec *pspec,
                        gpointer    user_data)
{
  GcalSourceDialogPrivate *priv = GCAL_SOURCE_DIALOG (user_data)->priv;
  const gchar* text;

  text = gtk_entry_get_text (GTK_ENTRY (priv->calendar_address_entry));

  if (priv->calendar_address_id != 0)
    {
      g_source_remove (priv->calendar_address_id);
      priv->calendar_address_id = 0;

      gtk_entry_set_progress_fraction (GTK_ENTRY (priv->calendar_address_entry), 0);
    }

  if (priv->validate_url_resource_id != 0)
    {
      g_source_remove (priv->validate_url_resource_id);
      priv->validate_url_resource_id = 0;
    }

  if (g_utf8_strlen (text, -1) != 0)
    {
      // Remove any previous unreleased resource
      if (priv->validate_url_resource_id != 0)
        g_source_remove (priv->validate_url_resource_id);

      /*
       * At first, don't bother the user with
       * the login prompt. Only prompt it when
       * it fails.
       */
      priv->prompt_password = FALSE;

      priv->validate_url_resource_id = g_timeout_add (500, (GSourceFunc) validate_url_cb, user_data);
    }
  else
    {
      gtk_entry_set_progress_fraction (GTK_ENTRY (priv->calendar_address_entry), 0);
      gtk_revealer_set_reveal_child (GTK_REVEALER (priv->web_sources_revealer), FALSE);
    }
}

/**
 * validate_url_cb:
 *
 * Query the given URL for possible
 * calendar data.
 *
 * Returns:FALSE
 */
static gboolean
validate_url_cb (GcalSourceDialog *dialog)
{
  GcalSourceDialogPrivate *priv = dialog->priv;
  ESourceAuthentication *auth;
  ESourceExtension *ext;
  ESourceWebdav *webdav;
  ESource *source;
  gchar *host, *path;
  gboolean uri_valid;

  priv->validate_url_resource_id = 0;
  host = path = NULL;

  /**
   * Remove any reminescent ESources
   * cached before.
   */
  if (priv->remote_source != NULL)
    g_clear_pointer (&(priv->remote_source), g_object_unref);

  // Get the hostname and file path from the server
  uri_valid = uri_get_fields (gtk_entry_get_text (GTK_ENTRY (priv->calendar_address_entry)), NULL, &host, &path);

  g_debug ("[source-dialog] host: '%s', path: '%s'", host, path);

  if (host == NULL || !uri_valid)
    goto out;

  /**
   * Create the new source and add the needed
   * extensions.
   */
  source = e_source_new (NULL, NULL, NULL);
  e_source_set_parent (source, "webcal-stub");

  ext = e_source_get_extension (source, E_SOURCE_EXTENSION_CALENDAR);
  e_source_backend_set_backend_name (E_SOURCE_BACKEND (ext), "webcal");

  // Authentication
  auth = e_source_get_extension (source, E_SOURCE_EXTENSION_AUTHENTICATION);
  e_source_authentication_set_host (auth, host);

  // Webdav
  webdav = e_source_get_extension (source, E_SOURCE_EXTENSION_WEBDAV_BACKEND);
  e_source_webdav_set_resource_path (webdav, path);

  /*
   * If we're dealing with an absolute file path,
   * there is no need to check the server for more
   * sources.
   */
  if (g_str_has_suffix (path, ".ics"))
    {
      // Set the private source so it saves at closing
      priv->remote_source = source;

      // Update buttons
      gtk_widget_set_sensitive (priv->add_button, source != NULL);
      setup_source_details (dialog, source);
    }
  else
    {
      // Pulse the entry while it performs the check
      priv->calendar_address_id = g_timeout_add (ENTRY_PROGRESS_TIMEOUT, (GSourceFunc) pulse_web_entry, dialog);

      /*
       * Try to retrieve the sources without prompting
       * username and password. If we get any error,
       * then it prompts and retry.
       */
      if (priv->prompt_password)
        {
          gint response;
          gchar *user, *password;

          response = prompt_credentials (dialog, &user, &password);

          /*
           * User entered username and password, let's try
           * with it.
           */
          if (response == GTK_RESPONSE_OK)
            {
              ENamedParameters *credentials = e_named_parameters_new ();
              e_named_parameters_set (credentials, E_SOURCE_CREDENTIAL_USERNAME, user);
              e_named_parameters_set (credentials, E_SOURCE_CREDENTIAL_PASSWORD, password);

              e_webdav_discover_sources (source, gtk_entry_get_text (GTK_ENTRY (priv->calendar_address_entry)),
                                         E_WEBDAV_DISCOVER_SUPPORTS_EVENTS, credentials, NULL, discover_sources_cb,
                                         dialog);

              e_named_parameters_free (credentials);
            }

          if (user)
            g_free (user);
          if (password)
            g_free (password);
        }
      else
        {
          g_debug ("[source-dialog] Trying to connect without credentials...");
          e_webdav_discover_sources (source, gtk_entry_get_text (GTK_ENTRY (priv->calendar_address_entry)),
                                     E_WEBDAV_DISCOVER_SUPPORTS_EVENTS, NULL, NULL, discover_sources_cb, dialog);
        }
    }

out:
  if (host)
    g_free (host);

  if (path)
    g_free (path);

  return FALSE;
}

static void
credential_entry_activate (GtkEntry *entry,
                           gpointer  user_data)
{
  gtk_dialog_response (GTK_DIALOG (user_data), GTK_RESPONSE_OK);
}

static gint
prompt_credentials (GcalSourceDialog  *dialog,
                    gchar            **username,
                    gchar            **password)
{
  GtkWidget *password_entry;
  GtkWidget *prompt_dialog;
  GtkWidget *name_entry;
  GtkWidget *grid, *label;
  GtkWidget *button;
  gint response;

  // Name entry
  name_entry = gtk_entry_new ();
  gtk_widget_set_hexpand (name_entry, TRUE);

  // Password entry
  password_entry = g_object_new (GTK_TYPE_ENTRY,
                                 "visibility", FALSE,
                                 "hexpand", TRUE,
                                 NULL);

  prompt_dialog = gtk_dialog_new_with_buttons (_("Enter your username and password"), GTK_WINDOW (dialog),
                                               GTK_DIALOG_MODAL | GTK_DIALOG_USE_HEADER_BAR,
                                               _("Cancel"), GTK_RESPONSE_CANCEL,
                                               _("Connect"), GTK_RESPONSE_OK, NULL);

  // Set the "Connect" button style
  button = gtk_dialog_get_widget_for_response (GTK_DIALOG (prompt_dialog), GTK_RESPONSE_OK);
  gtk_style_context_add_class (gtk_widget_get_style_context (button), "suggested-action");

  // Add some labels
  grid = gtk_grid_new ();
  g_object_set (grid,
                "border-width", 12,
                "column-spacing", 12,
                "row-spacing", 6,
                "expand", TRUE,
                NULL);

  label = gtk_label_new (_("User"));
  gtk_style_context_add_class (gtk_widget_get_style_context (label), "dim-label");
  gtk_label_set_xalign (GTK_LABEL (label), 1.0);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);

  label = gtk_label_new (_("Password"));
  gtk_style_context_add_class (gtk_widget_get_style_context (label), "dim-label");
  gtk_label_set_xalign (GTK_LABEL (label), 1.0);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 1, 1, 1);

  // Add entries
  gtk_grid_attach (GTK_GRID (grid), name_entry, 1, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), password_entry, 1, 1, 1, 1);

  // Insert into the dialog
  gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (prompt_dialog))), grid);
  gtk_widget_show_all (grid);

  g_signal_connect (name_entry, "activate", G_CALLBACK (credential_entry_activate), prompt_dialog);
  g_signal_connect (password_entry, "activate", G_CALLBACK (credential_entry_activate), prompt_dialog);

  // Show the dialog, then destroy it
  response = gtk_dialog_run (GTK_DIALOG (prompt_dialog));

  if (username)
    *username = g_strdup (gtk_entry_get_text (GTK_ENTRY (name_entry)));

  if (password)
    *password = g_strdup (gtk_entry_get_text (GTK_ENTRY (password_entry)));

  gtk_widget_destroy (prompt_dialog);

  return response;
}

static void
discover_sources_cb (GObject      *source,
                     GAsyncResult *result,
                     gpointer      user_data)
{
  GcalSourceDialogPrivate *priv = GCAL_SOURCE_DIALOG (user_data)->priv;
  EWebDAVDiscoveredSource *src;
  GSList *discovered_sources, *user_adresses, *aux;
  GError *error;
  gint n_sources, counter;

  error = NULL;

  // Stop the pulsing entry
  if (priv->calendar_address_id != 0)
    {
      gtk_entry_set_progress_fraction (GTK_ENTRY (priv->calendar_address_entry), 0);
      g_source_remove (priv->calendar_address_id);
      priv->calendar_address_id = 0;
    }

  if (!e_webdav_discover_sources_finish (E_SOURCE (source), result, NULL, NULL, &discovered_sources, &user_adresses,
                                        &error))
    {
      // Don't add an source with errors
      gtk_widget_set_sensitive (priv->add_button, FALSE);

      /*
       * If it's the first try and things went wrong,
       * retry with the user credentials. Also, it
       * checks for the error code, since we don't
       * really want to retry things on unavailable
       * servers.
       */
      if (!priv->prompt_password && error->code == 14)
        {
          g_debug ("[source-dialog] No credentials failed, retrying with user credentials...");

          priv->prompt_password = TRUE;

          validate_url_cb (GCAL_SOURCE_DIALOG (user_data));
        }

      g_error_free (error);
      return;
    }

  n_sources = g_slist_length (discovered_sources);
  counter = 0;

  if (n_sources > 1)
    {
      // Remove previous results
      g_list_free_full (gtk_container_get_children (GTK_CONTAINER (priv->web_sources_listbox)),
                        (GDestroyNotify) gtk_widget_destroy);

      // Show the list of calendars
      gtk_revealer_set_reveal_child (GTK_REVEALER (priv->web_sources_revealer), TRUE);

      /* TODO: show a list of calendars */
      for (aux = discovered_sources; aux != NULL; aux = aux->next)
        {
          GtkWidget *row;
          GtkWidget *grid;
          GtkWidget *name_label, *url_label;
          GdkRGBA rgba;

          src = discovered_sources->data;
          row = gtk_list_box_row_new ();

          grid = g_object_new (GTK_TYPE_GRID,
                               "column_spacing", 12,
                               "border_width", 10,
                               NULL);

          name_label = gtk_label_new (src->display_name);
          gtk_label_set_xalign (GTK_LABEL (name_label), 0.0);
          gtk_label_set_ellipsize (GTK_LABEL (name_label), PANGO_ELLIPSIZE_END);

          url_label = gtk_label_new (src->href);
          gtk_style_context_add_class (gtk_widget_get_style_context (url_label), "dim-label");
          gtk_label_set_xalign (GTK_LABEL (url_label), 0.0);
          gtk_label_set_ellipsize (GTK_LABEL (url_label), PANGO_ELLIPSIZE_END);

          if (src->color != NULL)
            {
              GtkWidget *color_image;

              gdk_rgba_parse (&rgba, src->color);
              color_image = gtk_image_new_from_pixbuf (get_circle_pixbuf_from_color (&rgba, 16));

              gtk_grid_attach (GTK_GRID (grid), color_image, 0, 0, 1, 2);
            }

          gtk_grid_attach (GTK_GRID (grid), name_label, 1, 0, 1, 1);
          gtk_grid_attach (GTK_GRID (grid), url_label, 1, 1, 1, 1);

          gtk_container_add (GTK_CONTAINER (priv->web_sources_listbox), row);
          gtk_container_add (GTK_CONTAINER (row), grid);

          g_object_set_data (G_OBJECT (row), "source", source);
          g_object_set_data (G_OBJECT (row), "source-url", g_strdup (src->href));
          g_object_set_data (G_OBJECT (row), "source-color", g_strdup (src->color));
          g_object_set_data (G_OBJECT (row), "source-display-name", g_strdup (src->display_name));
          g_object_set_data (G_OBJECT (row), "source-email", g_strdup (g_slist_nth_data (user_adresses, counter)));

          gtk_widget_show_all (row);

          counter++;
        }
    }
  else if (n_sources == 1)
    {
      gboolean uri_valid;
      gchar *resource_path;

      src = discovered_sources->data;

      // Get the new resource path from the uri
      uri_valid = uri_get_fields (src->href, NULL, NULL, &resource_path);

      // Update the ESourceWebdav extension's resource path
      if (uri_valid)
        {
          ESourceWebdav *webdav = e_source_get_extension (E_SOURCE (source), E_SOURCE_EXTENSION_WEBDAV_BACKEND);

          e_source_webdav_set_resource_path (webdav, resource_path);
          e_source_webdav_set_display_name (webdav, src->display_name);
          e_source_webdav_set_email_address (webdav, user_adresses->data);

          e_source_set_display_name (E_SOURCE (source), src->display_name);

          // Update button sensivity, etc
          gtk_widget_set_sensitive (priv->add_button, source != NULL);
          setup_source_details (GCAL_SOURCE_DIALOG (user_data), E_SOURCE (source));
        }

      if (resource_path)
        g_free (resource_path);
    }

  // Free things up
  e_webdav_discover_free_discovered_sources (discovered_sources);
  g_slist_free_full (user_adresses, g_free);
}

GcalSourceDialog *
gcal_source_dialog_new (void)
{
  return g_object_new (GCAL_TYPE_SOURCE_DIALOG, NULL);
}

static void
gcal_source_dialog_constructed (GObject *object)
{
  GcalSourceDialog *self = (GcalSourceDialog *)object;
  GcalSourceDialogPrivate *priv = gcal_source_dialog_get_instance_private (self);
  GtkFileFilter *filter;

  G_OBJECT_CLASS (gcal_source_dialog_parent_class)->constructed (object);

  /* widget responses */
  gtk_dialog_set_default_response (GTK_DIALOG (object), GTK_RESPONSE_CANCEL);

  g_object_set_data (G_OBJECT (priv->add_button), "response", GINT_TO_POINTER (GTK_RESPONSE_APPLY));
  g_object_set_data (G_OBJECT (priv->cancel_button), "response", GINT_TO_POINTER (GTK_RESPONSE_CANCEL));
  g_object_set_data (G_OBJECT (priv->remove_button), "response", GINT_TO_POINTER (GCAL_RESPONSE_REMOVE_SOURCE));

  // Setup listbox header functions
  gtk_list_box_set_header_func (GTK_LIST_BOX (priv->calendars_listbox), display_header_func, NULL, NULL);
  gtk_list_box_set_header_func (GTK_LIST_BOX (priv->online_accounts_listbox), display_header_func, NULL, NULL);

  /* File filter */
  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("Calendar files"));
  gtk_file_filter_add_mime_type (filter, "text/calendar");

  //gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (priv->select_file_button), filter);

  /* setup titlebar */
  gtk_window_set_titlebar (GTK_WINDOW (object), priv->headerbar);
}

static void
gcal_source_dialog_class_init (GcalSourceDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class;

  /**
   * Since we cannot guarantee that the
   * type system registered ESourceLocal,
   * it must be ensured at least here.
   */
  g_type_ensure (E_TYPE_SOURCE_LOCAL);

  object_class->constructed = gcal_source_dialog_constructed;

  widget_class = GTK_WIDGET_CLASS (klass);

  /* bind things for/from the template class */
  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/org/gnome/calendar/source-dialog.ui");

  gtk_widget_class_bind_template_child_private (widget_class, GcalSourceDialog, add_button);
  gtk_widget_class_bind_template_child_private (widget_class, GcalSourceDialog, add_calendar_menu_button);
  gtk_widget_class_bind_template_child_private (widget_class, GcalSourceDialog, back_button);
  gtk_widget_class_bind_template_child_private (widget_class, GcalSourceDialog, calendar_address_entry);
  gtk_widget_class_bind_template_child_private (widget_class, GcalSourceDialog, calendar_color_button);
  gtk_widget_class_bind_template_child_private (widget_class, GcalSourceDialog, calendar_visible_check);
  gtk_widget_class_bind_template_child_private (widget_class, GcalSourceDialog, calendars_listbox);
  gtk_widget_class_bind_template_child_private (widget_class, GcalSourceDialog, cancel_button);
  gtk_widget_class_bind_template_child_private (widget_class, GcalSourceDialog, default_check);
  gtk_widget_class_bind_template_child_private (widget_class, GcalSourceDialog, edit_grid);
  gtk_widget_class_bind_template_child_private (widget_class, GcalSourceDialog, headerbar);
  gtk_widget_class_bind_template_child_private (widget_class, GcalSourceDialog, name_entry);
  gtk_widget_class_bind_template_child_private (widget_class, GcalSourceDialog, online_accounts_listbox);
  gtk_widget_class_bind_template_child_private (widget_class, GcalSourceDialog, remove_button);
  gtk_widget_class_bind_template_child_private (widget_class, GcalSourceDialog, stack);
  gtk_widget_class_bind_template_child_private (widget_class, GcalSourceDialog, web_details_frame);
  gtk_widget_class_bind_template_child_private (widget_class, GcalSourceDialog, web_new_calendar_name_entry);
  gtk_widget_class_bind_template_child_private (widget_class, GcalSourceDialog, web_source_grid);
  gtk_widget_class_bind_template_child_private (widget_class, GcalSourceDialog, web_sources_listbox);
  gtk_widget_class_bind_template_child_private (widget_class, GcalSourceDialog, web_sources_revealer);

  gtk_widget_class_bind_template_callback (widget_class, action_widget_activated);
  gtk_widget_class_bind_template_callback (widget_class, back_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, calendar_file_selected);
  gtk_widget_class_bind_template_callback (widget_class, calendar_listbox_row_activated);
  gtk_widget_class_bind_template_callback (widget_class, calendar_visible_check_toggled);
  gtk_widget_class_bind_template_callback (widget_class, color_set);
  gtk_widget_class_bind_template_callback (widget_class, default_check_toggled);
  gtk_widget_class_bind_template_callback (widget_class, description_label_link_activated);
  gtk_widget_class_bind_template_callback (widget_class, name_entry_text_changed);
  gtk_widget_class_bind_template_callback (widget_class, new_name_entry_text_changed);
  gtk_widget_class_bind_template_callback (widget_class, response_signal);
  gtk_widget_class_bind_template_callback (widget_class, stack_visible_child_name_changed);
  gtk_widget_class_bind_template_callback (widget_class, url_entry_text_changed);
}

static void
gcal_source_dialog_init (GcalSourceDialog *self)
{
  self->priv = gcal_source_dialog_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));
}

/**
 * gcal_source_dialog_set_manager:
 *
 * Setup the {@link GcalManager} singleton
 * instance of the application.
 *
 * Returns:
 */
void
gcal_source_dialog_set_manager (GcalSourceDialog *dialog,
                                GcalManager      *manager)
{
  GcalSourceDialogPrivate *priv = dialog->priv;

  priv->manager = manager;

  if (gcal_manager_load_completed (priv->manager))
    {
      GList *sources, *l;

      sources = gcal_manager_get_sources_connected (priv->manager);

      for (l = sources; l != NULL; l = l->next)
        add_source (priv->manager, l->data, gcal_manager_source_enabled (priv->manager, l->data), dialog);
    }

  g_signal_connect (priv->manager, "source-added", G_CALLBACK (add_source), dialog);
  /* TODO: connect ::source-removed signals */
}

/**
 * gcal_source_dialog_set_mode:
 *
 * Set the source dialog mode. Creation
 * mode means that a new calendar will
 * be created, while edit mode means a
 * calendar will be edited.
 *
 * Returns:
 */
void
gcal_source_dialog_set_mode (GcalSourceDialog    *dialog,
                             GcalSourceDialogMode mode)
{
  GcalSourceDialogPrivate *priv = dialog->priv;
  gboolean edit_mode;

  priv->mode = mode;
  edit_mode = (mode == GCAL_SOURCE_DIALOG_MODE_EDIT);

  gtk_stack_set_visible_child_name (GTK_STACK (priv->stack), edit_mode ? "edit" : "main");

  if (!edit_mode)
    {
      /* Free any bindings left behind */
      if (priv->title_bind != NULL)
        {
          g_binding_unbind (priv->title_bind);
          priv->title_bind = NULL;
        }

      gtk_header_bar_set_title (GTK_HEADER_BAR (priv->headerbar), _("Calendar Settings"));

      clear_pages (dialog);
    }
  else
    {
      /* bind title when nothing is binded */
      if (priv->title_bind == NULL)
        {
          priv->title_bind = g_object_bind_property (priv->name_entry, "text", priv->headerbar, "title",
                                                     G_BINDING_DEFAULT);
        }
    }
}

/**
 * gcal_source_dialog_set_source:
 *
 * Sets the source to be edited by the user.
 *
 * Returns:
 */
void
gcal_source_dialog_set_source (GcalSourceDialog *dialog,
                               ESource          *source)
{
  GcalSourceDialogPrivate *priv = dialog->priv;

  g_assert (source && E_IS_SOURCE (source));

  g_object_ref (source);

  priv->source = source;
}
