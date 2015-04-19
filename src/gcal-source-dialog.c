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

  /* notification */
  GtkWidget          *notification;
  GtkWidget          *notification_label;

  /* edit page widgets */
  GtkWidget          *account_box;
  GtkWidget          *account_label;
  GtkWidget          *account_dim_label;
  GtkWidget          *calendar_url_button;
  GtkWidget          *location_dim_label;

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

  /* overview widgets */
  GtkWidget          *add_calendar_menu_button;
  GtkWidget          *calendars_listbox;
  GtkWidget          *online_accounts_listbox;

  /* flags */
  GcalSourceDialogMode mode;
  ESource            *source;
  GList              *remote_sources;
  ESource            *removed_source;
  ESource            *old_default_source;
  GBinding           *title_bind;
  gboolean            prompt_password;

  /* auxiliary */
  GSimpleActionGroup *action_group;

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

static void       add_button_clicked                    (GtkWidget            *button,
                                                         gpointer              user_data);

static void       add_source                             (GcalManager         *manager,
                                                          ESource             *source,
                                                          gboolean             enabled,
                                                          gpointer             user_data);

static void       action_widget_activated               (GtkWidget            *widget,
                                                         gpointer              user_data);

static void       back_button_clicked                   (GtkButton            *button,
                                                         gpointer              user_data);

static void       calendar_file_selected                (GtkFileChooser       *button,
                                                         gpointer              user_data);

static void       calendar_listbox_row_activated        (GtkListBox          *box,
                                                         GtkListBoxRow       *row,
                                                         gpointer             user_data);

static gint       calendar_listbox_sort_func             (GtkListBoxRow       *row1,
                                                          GtkListBoxRow       *row2,
                                                          gpointer             user_data);

static void       calendar_visible_check_toggled        (GObject             *object,
                                                         GParamSpec          *pspec,
                                                         gpointer             user_data);

static void       cancel_button_clicked                 (GtkWidget            *button,
                                                         gpointer              user_data);

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

static void       online_accounts_settings_button_clicked (GtkWidget         *button,
                                                          gpointer            user_data);

static void       on_file_activated                     (GSimpleAction       *action,
                                                         GVariant            *param,
                                                         gpointer             user_data);

static void       on_local_activated                    (GSimpleAction       *action,
                                                         GVariant            *param,
                                                         gpointer             user_data);

static void       on_web_activated                      (GSimpleAction       *action,
                                                         GVariant            *param,
                                                         gpointer             user_data);

static void       remove_source                         (GcalManager         *manager,
                                                         ESource             *source,
                                                         gpointer             user_data);

static void       response_signal                       (GtkDialog           *dialog,
                                                         gint                 response_id,
                                                         gpointer             user_data);

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

GActionEntry actions[] = {
  {"file",  on_file_activated,  NULL, NULL, NULL},
  {"local", on_local_activated, NULL, NULL, NULL},
  {"web",   on_web_activated,   NULL, NULL, NULL}
};


static void
add_button_clicked (GtkWidget *button,
                    gpointer   user_data)
{
  GcalSourceDialogPrivate *priv = GCAL_SOURCE_DIALOG (user_data)->priv;

  if (priv->source != NULL)
    {
      // Commit the new source
      gcal_manager_save_source (priv->manager, priv->source);

      priv->source = NULL;

      gcal_source_dialog_set_mode (GCAL_SOURCE_DIALOG (user_data), GCAL_SOURCE_DIALOG_MODE_NORMAL);
    }

  if (priv->remote_sources != NULL)
    {
      GList *l;

      for (l = priv->remote_sources; l != NULL; l = l->next)
        {
          // Commit each new remote source
          gcal_manager_save_source (priv->manager, l->data);
        }

      g_list_free (priv->remote_sources);

      priv->remote_sources = NULL;

      // Go back to overview
      gcal_source_dialog_set_mode (GCAL_SOURCE_DIALOG (user_data), GCAL_SOURCE_DIALOG_MODE_NORMAL);
    }
}

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
  const gchar *visible_child;

  visible_child = gtk_stack_get_visible_child_name (GTK_STACK (priv->stack));

  if (g_strcmp0 (visible_child, "edit") == 0)
    {
      // Save the source before leaving
      gcal_manager_save_source (priv->manager, priv->source);

      // Release the source ref we acquired
      g_object_unref (priv->source);
      priv->source = NULL;
    }

  gcal_source_dialog_set_mode (GCAL_SOURCE_DIALOG (user_data), GCAL_SOURCE_DIALOG_MODE_NORMAL);
}

static gint
calendar_listbox_sort_func (GtkListBoxRow *row1,
                            GtkListBoxRow *row2,
                            gpointer       user_data)
{
  GcalSourceDialogPrivate *priv = GCAL_SOURCE_DIALOG (user_data)->priv;
  ESource *source1;
  ESource *source2;
  gboolean is_goa1;
  gboolean is_goa2;
  gint retval;

  // first source
  source1 = g_object_get_data (G_OBJECT (row1), "source");
  is_goa1 = is_goa_source (GCAL_SOURCE_DIALOG (user_data), source1);

  // second source
  source2 = g_object_get_data (G_OBJECT (row2), "source");
  is_goa2 = is_goa_source (GCAL_SOURCE_DIALOG (user_data), source2);

  if (is_goa1 == is_goa2)
    {
      gchar *parent_name1 = NULL;
      gchar *parent_name2 = NULL;

      // Retrieve parent names
      get_source_parent_name_color (priv->manager, source1, &parent_name1, NULL);
      get_source_parent_name_color (priv->manager, source1, &parent_name2, NULL);

      retval =  g_strcmp0 (parent_name1, parent_name2);

      // If they have the same parent names, compare by the source display names
      if (retval == 0)
        retval = g_strcmp0 (e_source_get_display_name (source1), e_source_get_display_name (source2));

      if (parent_name1 != NULL)
        g_free (parent_name1);
      if (parent_name2 != NULL)
        g_free (parent_name2);
    }
  else
    {
      // If one is a GOA account and the other isn't, make the GOA one go first
      retval = is_goa1 ? -1 : 1;
    }

  return retval;
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

static void
cancel_button_clicked (GtkWidget *button,
                       gpointer   user_data)
{
  GcalSourceDialogPrivate *priv = GCAL_SOURCE_DIALOG (user_data)->priv;

  // Destroy the ongoing created source
  if (priv->source != NULL)
    {
      g_object_unref (priv->source);
      priv->source = NULL;
    }

  // Cleanup detected remote sources that weren't added
  if (priv->remote_sources != NULL)
    {
      g_list_free_full (priv->remote_sources, g_object_unref);
      priv->remote_sources = NULL;
    }

  gcal_source_dialog_set_mode (GCAL_SOURCE_DIALOG (user_data), GCAL_SOURCE_DIALOG_MODE_NORMAL);
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
  gboolean valid;

  valid = g_utf8_strlen (gtk_entry_get_text (GTK_ENTRY (object)), -1) > 0;

  gtk_widget_set_sensitive (priv->back_button, valid);
  gtk_widget_set_sensitive (priv->add_button, valid);

  if (valid)
    e_source_set_display_name (priv->source, gtk_entry_get_text (GTK_ENTRY (priv->name_entry)));
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
      if (priv->remote_sources != NULL)
        {
          GList *l;

          for (l = priv->remote_sources; l != NULL; l = l->next)
            {
              // Commit each new remote source
              gcal_manager_save_source (priv->manager, l->data);
            }

          g_list_free (priv->remote_sources);

          priv->remote_sources = NULL;
        }
    }

  /* Destroy the source when the operation is cancelled */
  if (priv->mode == GCAL_SOURCE_DIALOG_MODE_NORMAL && response_id == GTK_RESPONSE_CANCEL)
    {
      if (priv->remote_sources != NULL)
        {
          g_list_free_full (priv->remote_sources, g_object_unref);
          priv->remote_sources = NULL;
        }
    }
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

      // No host is set, it's not a remote source
      if (e_source_authentication_get_host (auth) == NULL)
        return FALSE;
    }

  if (has_webdav)
    {
      ESourceWebdav *webdav;

      webdav = e_source_get_extension (source, E_SOURCE_EXTENSION_WEBDAV_BACKEND);

      // No resource path specified, not a remote source
      if (e_source_webdav_get_resource_path (webdav) == NULL)
        return FALSE;
    }

  return TRUE;
}

static void
stack_visible_child_name_changed (GObject    *object,
                                  GParamSpec *pspec,
                                  gpointer    user_data)
{
  GcalSourceDialogPrivate *priv = GCAL_SOURCE_DIALOG (user_data)->priv;
  const gchar *visible_name;

  visible_name = gtk_stack_get_visible_child_name (GTK_STACK (object));

  if (g_strcmp0 (visible_name, "main") == 0)
    {
      gtk_header_bar_set_title (GTK_HEADER_BAR (priv->headerbar), _("Calendar Settings"));
      gtk_header_bar_set_subtitle (GTK_HEADER_BAR (priv->headerbar), NULL);
      gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (priv->headerbar), TRUE);
      gtk_widget_set_visible (priv->add_button, FALSE);
      gtk_widget_set_visible (priv->cancel_button, FALSE);
      gtk_widget_set_visible (priv->back_button, FALSE);
    }

  /*
   * Update fields when it goes to the edit page.
   * Here, only widgets that depends on the current
   * source are updated, while indenpendent widgets
   * are updated at #gcal_source_dialog_set_mode
   */
  if (g_strcmp0 (visible_name, "edit") == 0 && priv->source != NULL)
    {
      ESource *default_source;
      gchar *parent_name;
      GdkRGBA color;
      gboolean creation_mode;
      gboolean is_goa;
      gboolean is_file;
      gboolean is_remote;

      default_source = gcal_manager_get_default_source (priv->manager);
      creation_mode = (priv->mode == GCAL_SOURCE_DIALOG_MODE_CREATE ||
                       priv->mode == GCAL_SOURCE_DIALOG_MODE_CREATE_WEB);
      is_goa = is_goa_source (GCAL_SOURCE_DIALOG (user_data), priv->source);
      is_file = e_source_has_extension (priv->source, E_SOURCE_EXTENSION_LOCAL_BACKEND);
      is_remote = is_remote_source (priv->source);

      get_source_parent_name_color (priv->manager, priv->source, &parent_name, NULL);

      // update headerbar buttons
      gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (priv->headerbar), !creation_mode);
      gtk_widget_set_visible (priv->calendar_visible_check, !creation_mode);
      gtk_widget_set_visible (priv->back_button, !creation_mode);
      gtk_widget_set_visible (priv->add_button, creation_mode);
      gtk_widget_set_visible (priv->cancel_button, creation_mode);
      gtk_widget_set_visible (priv->account_box, is_goa);
      gtk_widget_set_visible (priv->calendar_url_button, !is_goa && (is_file || is_remote));

      // If it's a file, set the file path
      if (is_file)
        {
          ESourceLocal *local;
          GFile *file;
          gchar *uri;

          local = e_source_get_extension (priv->source, E_SOURCE_EXTENSION_LOCAL_BACKEND);
          file = e_source_local_get_custom_file (local);
          uri = g_file_get_uri (file);

          gtk_link_button_set_uri (GTK_LINK_BUTTON (priv->calendar_url_button), uri);
          gtk_button_set_label (GTK_BUTTON (priv->calendar_url_button), uri);

          g_free (uri);
        }

      // If it's remote, build the uri
      if (is_remote)
        {
          ESourceAuthentication *auth;
          ESourceWebdav *webdav;
          gchar *uri;

          auth = e_source_get_extension (priv->source, E_SOURCE_EXTENSION_AUTHENTICATION);
          webdav = e_source_get_extension (priv->source, E_SOURCE_EXTENSION_WEBDAV_BACKEND);
          uri = g_strdup_printf ("https://%s%s", e_source_authentication_get_host (auth),
                                 e_source_webdav_get_resource_path (webdav));

          gtk_link_button_set_uri (GTK_LINK_BUTTON (priv->calendar_url_button), uri);
          gtk_button_set_label (GTK_BUTTON (priv->calendar_url_button), uri);

          g_free (uri);
        }

      // TODO: setup GOA settings

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
      if (!creation_mode)
        {
          gtk_header_bar_set_title (GTK_HEADER_BAR (priv->headerbar), e_source_get_display_name (priv->source));
          gtk_header_bar_set_subtitle (GTK_HEADER_BAR (priv->headerbar), parent_name);
        }

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
calendar_file_selected (GtkFileChooser       *button,
                        gpointer              user_data)
{
  GcalSourceDialogPrivate *priv = GCAL_SOURCE_DIALOG (user_data)->priv;
  ESourceExtension *ext;
  ESource *source;
  GFile *file;

  file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (button));

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

  // Jump to the edit page
  gcal_source_dialog_set_source (GCAL_SOURCE_DIALOG (user_data), source);
  gcal_source_dialog_set_mode (GCAL_SOURCE_DIALOG (user_data), GCAL_SOURCE_DIALOG_MODE_CREATE);

  gtk_widget_set_sensitive (priv->add_button, TRUE);
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
 * online_accounts_settings_button_clicked:
 *
 * Spawns the GNOME Control Center app
 * with Online Accounts openned.
 *
 * Returns:
 */
static void
online_accounts_settings_button_clicked (GtkWidget *button,
                                         gpointer   user_data)
{
  gchar *command[] = {"gnome-control-center", "online-accounts", NULL};
  g_spawn_async (NULL, command, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);
}

static void
on_file_activated (GSimpleAction *action,
                   GVariant      *param,
                   gpointer       user_data)
{
  GtkWidget *dialog;
  GtkFileFilter *filter;

  // Dialog
  dialog = gtk_file_chooser_dialog_new (_("Select a calendar file"),
                                        GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (user_data))),
                                        GTK_FILE_CHOOSER_ACTION_SAVE,
                                        _("Cancel"), GTK_RESPONSE_CANCEL,
                                        _("Open"), GTK_RESPONSE_OK,
                                        NULL);

  g_signal_connect (dialog, "file-activated", G_CALLBACK (calendar_file_selected), user_data);

  // File filter
  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("Calendar files"));
  gtk_file_filter_add_mime_type (filter, "text/calendar");

  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

  gtk_dialog_run (GTK_DIALOG (dialog));

  gtk_widget_destroy (dialog);
}

/**
 * on_local_activated:
 *
 * Creates a new local calendar, and let
 * the user adjust the settings after.
 *
 * Returns:
 */
static void
on_local_activated (GSimpleAction *action,
                    GVariant      *param,
                    gpointer       user_data)
{
  GcalSourceDialogPrivate *priv = GCAL_SOURCE_DIALOG (user_data)->priv;
  ESourceExtension *ext;
  ESource *source;

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

  // Jump to the edit page
  gcal_source_dialog_set_source (GCAL_SOURCE_DIALOG (user_data), source);
  gcal_source_dialog_set_mode (GCAL_SOURCE_DIALOG (user_data), GCAL_SOURCE_DIALOG_MODE_CREATE);

  gtk_widget_set_sensitive (priv->add_button, TRUE);
}

/**
 * on_web_activated:
 *
 * Redirect to the web calendar creation
 * page
 *
 * Returns:
 */
static void
on_web_activated (GSimpleAction *action,
                  GVariant      *param,
                  gpointer       user_data)
{
  gcal_source_dialog_set_mode (GCAL_SOURCE_DIALOG (user_data), GCAL_SOURCE_DIALOG_MODE_CREATE_WEB);
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
  if (priv->remote_sources != NULL)
    {
      g_list_free_full (priv->remote_sources, g_object_unref);
      priv->remote_sources = NULL;
    }

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
      priv->remote_sources = g_list_append (priv->remote_sources, source);

      // Update buttons
      gtk_widget_set_sensitive (priv->add_button, source != NULL);
    }
  else
    {
      ENamedParameters *credentials;

      // Pulse the entry while it performs the check
      priv->calendar_address_id = g_timeout_add (ENTRY_PROGRESS_TIMEOUT, (GSourceFunc) pulse_web_entry, dialog);

      /*
       * Try to retrieve the sources without prompting
       * username and password. If we get any error,
       * then it prompts and retry.
       */
      credentials = e_named_parameters_new ();

      if (!priv->prompt_password)
        {
          g_debug ("[source-dialog] Trying to connect without credentials...");

          // NULL credentials
          e_named_parameters_set (credentials, E_SOURCE_CREDENTIAL_USERNAME, NULL);
          e_named_parameters_set (credentials, E_SOURCE_CREDENTIAL_PASSWORD, NULL);

          e_webdav_discover_sources (source, gtk_entry_get_text (GTK_ENTRY (priv->calendar_address_entry)),
                                     E_WEBDAV_DISCOVER_SUPPORTS_EVENTS, credentials, NULL, discover_sources_cb,
                                     dialog);
        }
      else
        {
          gint response;
          gchar *user, *password;

          g_debug ("[source-dialog] No credentials failed, retrying with user credentials...");

          user = password = NULL;
          response = prompt_credentials (dialog, &user, &password);

          /*
           * User entered username and password, let's try
           * with it.
           */
          if (response == GTK_RESPONSE_OK)
            {
              // User inputted credentials
              e_named_parameters_set (credentials, E_SOURCE_CREDENTIAL_USERNAME, user);
              e_named_parameters_set (credentials, E_SOURCE_CREDENTIAL_PASSWORD, password);

              e_webdav_discover_sources (source, gtk_entry_get_text (GTK_ENTRY (priv->calendar_address_entry)),
                                         E_WEBDAV_DISCOVER_SUPPORTS_EVENTS, credentials, NULL, discover_sources_cb,
                                         dialog);
            }

          if (user)
            g_free (user);
          if (password)
            g_free (password);
        }

      e_named_parameters_free (credentials);
    }

out:
  if (host)
    g_free (host);

  if (path)
    g_free (path);

  return FALSE;
}

static void
credential_button_clicked (GtkWidget *button,
                           gpointer   user_data)
{
  GcalSourceDialogPrivate *priv = GCAL_SOURCE_DIALOG(user_data)->priv;

  if (button == priv->credentials_cancel_button)
    gtk_dialog_response (GTK_DIALOG (priv->credentials_dialog), GTK_RESPONSE_CANCEL);
  else
    gtk_dialog_response (GTK_DIALOG (priv->credentials_dialog), GTK_RESPONSE_OK);
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
  GcalSourceDialogPrivate *priv = dialog->priv;
  gint response;

  // Cleanup last credentials
  gtk_entry_set_text (GTK_ENTRY (priv->credentials_password_entry), "");
  gtk_entry_set_text (GTK_ENTRY (priv->credentials_user_entry), "");

  gtk_widget_grab_focus (priv->credentials_user_entry);

  // Show the dialog, then destroy it
  response = gtk_dialog_run (GTK_DIALOG (priv->credentials_dialog));

  if (response == GTK_RESPONSE_OK)
    {
      if (username)
        *username = g_strdup (gtk_entry_get_text (GTK_ENTRY (priv->credentials_user_entry)));

      if (password)
        *password = g_strdup (gtk_entry_get_text (GTK_ENTRY (priv->credentials_password_entry)));
    }

  gtk_widget_hide (priv->credentials_dialog);

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

  // Copy Authentication data
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

  // Copy Webdav data
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
  GcalSourceDialogPrivate *priv = GCAL_SOURCE_DIALOG (user_data)->priv;
  GtkWidget *row;
  ESource *source;

  g_assert (user_data && GCAL_IS_SOURCE_DIALOG (user_data));

  row = gtk_widget_get_parent (check);
  source = g_object_get_data (G_OBJECT (row), "source");

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check)))
    {
      priv->remote_sources = g_list_append (priv->remote_sources, source);
    }
  else
    {
      priv->remote_sources = g_list_remove (priv->remote_sources, source);
    }

  gtk_widget_set_sensitive (priv->add_button, g_list_length (priv->remote_sources) > 0);
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
          priv->prompt_password = TRUE;

          validate_url_cb (GCAL_SOURCE_DIALOG (user_data));
        }
      else
        {
          g_debug ("[source-dialog] error: %s", error->message);
        }

      g_error_free (error);
      return;
    }

  // Remove previous results
  g_list_free_full (gtk_container_get_children (GTK_CONTAINER (priv->web_sources_listbox)),
                    (GDestroyNotify) gtk_widget_destroy);

  // Show the list of calendars
  gtk_revealer_set_reveal_child (GTK_REVEALER (priv->web_sources_revealer), TRUE);

  /* TODO: show a list of calendars */
  for (aux = discovered_sources; aux != NULL; aux = aux->next)
    {
      gchar *resource_path = NULL;
      gboolean uri_valid;

      src = aux->data;

      // Get the new resource path from the uri
      uri_valid = uri_get_fields (src->href, NULL, NULL, &resource_path);

      if (uri_valid)
        {
          ESourceWebdav *webdav;
          GtkWidget *row;
          GtkWidget *check;
          ESource *new_source;


          /* build up the new source */
          new_source = duplicate_source (E_SOURCE (source));
          e_source_set_display_name (E_SOURCE (new_source), src->display_name);

          webdav = e_source_get_extension (new_source, E_SOURCE_EXTENSION_WEBDAV_BACKEND);
          e_source_webdav_set_resource_path (webdav, resource_path);
          e_source_webdav_set_display_name (webdav, src->display_name);
          e_source_webdav_set_email_address (webdav, user_adresses->data);


          /* create the new row */
          row = gtk_list_box_row_new ();

          check = gtk_check_button_new ();
          gtk_button_set_label (GTK_BUTTON (check), src->display_name);
          g_signal_connect (check, "notify::active", G_CALLBACK (check_activated_cb), user_data);

          gtk_container_add (GTK_CONTAINER (row), check);
          gtk_container_add (GTK_CONTAINER (priv->web_sources_listbox), row);

          g_object_set_data (G_OBJECT (row), "parent-source", source);
          g_object_set_data (G_OBJECT (row), "source", new_source);
          g_object_set_data (G_OBJECT (row), "source-url", g_strdup (src->href));
          g_object_set_data (G_OBJECT (row), "source-color", g_strdup (src->color));
          g_object_set_data (G_OBJECT (row), "source-display-name", g_strdup (src->display_name));
          //g_object_set_data (G_OBJECT (row), "source-email", g_strdup (g_slist_nth_data (user_adresses, counter)));

          gtk_widget_show_all (row);
        }

      if (resource_path)
        g_free (resource_path);
    }

  // Free things up
  e_webdav_discover_free_discovered_sources (discovered_sources);
  g_slist_free_full (user_adresses, g_free);
}

/**
 * remove_source:
 *
 * Removes the given source from the source
 * list.
 *
 * Returns:
 */
static void
remove_source (GcalManager *manager,
               ESource     *source,
               gpointer     user_data)
{
  GcalSourceDialogPrivate *priv = GCAL_SOURCE_DIALOG (user_data)->priv;
  GtkWidget *row;
  GList *children, *aux;

  children = gtk_container_get_children (GTK_CONTAINER (priv->calendars_listbox));
  row = NULL;

  for (aux = children; aux != NULL; aux = aux->next)
    {
      ESource *child_source = g_object_get_data (G_OBJECT (aux->data), "source");

      if (child_source != NULL && child_source == source)
        {
          row = aux->data;
          break;
        }
    }

  if (row != NULL)
    gtk_widget_destroy (row);

  g_list_free (children);
}

/**
 * notification_child_revealed_changed:
 *
 * Remove the source after the notification
 * is hidden.
 *
 * Returns:
 */
static void
notification_child_revealed_changed (GtkWidget  *notification,
                                     GParamSpec *spec,
                                     gpointer    user_data)
{
  GcalSourceDialogPrivate *priv = GCAL_SOURCE_DIALOG (user_data)->priv;

  if (gtk_revealer_get_child_revealed (GTK_REVEALER (notification)))
      return;

  /* If we have any removed source, delete it */
  if (priv->removed_source != NULL)
    {
      GError *error = NULL;

      /* We don't really want to remove non-removable sources */
      if (!e_source_get_removable (priv->removed_source))
        return;

      // Enable the source again to remove it's name from disabled list
      gcal_manager_enable_source (priv->manager, priv->removed_source);

      e_source_remove_sync (priv->removed_source, NULL, &error);

      /**
       * If something goes wrong, throw
       * an alert and add the source back.
       */
      if (error != NULL)
        {
          g_warning ("[source-dialog] Error removing source: %s", error->message);

          add_source (priv->manager, priv->removed_source,
                      gcal_manager_source_enabled (priv->manager, priv->removed_source), user_data);

          gcal_manager_enable_source (priv->manager, priv->removed_source);

          g_error_free (error);
        }
    }
}

/**
 * undo_remove_action:
 *
 * Readd the removed source.
 *
 * Returns:
 */
static void
undo_remove_action (GtkButton *button,
                    gpointer   user_data)
{
  GcalSourceDialogPrivate *priv = GCAL_SOURCE_DIALOG (user_data)->priv;

  /* if there's any set source, unremove it */
  if (priv->removed_source != NULL)
    {
      // Enable the source before adding it again
      gcal_manager_enable_source (priv->manager, priv->removed_source);

      add_source (priv->manager, priv->removed_source,
                  gcal_manager_source_enabled (priv->manager, priv->removed_source), user_data);

      /*
       * Don't clear the pointer, since we don't
       * want to erase the source at all.
       */
      priv->removed_source = NULL;

      // Hide notification
      gtk_revealer_set_reveal_child (GTK_REVEALER (priv->notification), FALSE);
    }
}

/**
 * hide_notification:
 *
 * Helper function that hides the
 * notification, causing the removal
 * of the source (when valid).
 *
 * Returns:
 */
static void
hide_notification (GcalSourceDialog *dialog,
                   GtkWidget        *button)
{
  GcalSourceDialogPrivate *priv = dialog->priv;
  gtk_revealer_set_reveal_child (GTK_REVEALER (priv->notification), FALSE);
  priv->notification_timeout_id = 0;
}

/**
 * hide_notification_scheduled:
 *
 * Limit the ammount of time that
 * the notification is shown.
 *
 * Returns: %FALSE
 */
static gboolean
hide_notification_scheduled (gpointer dialog)
{
  hide_notification (GCAL_SOURCE_DIALOG (dialog), NULL);
  return FALSE;
}

/**
 * remove_button_clicked:
 *
 * Trigger the source removal
 * logic.
 *
 * Returns:
 */
static void
remove_button_clicked (GtkWidget *button,
                       gpointer   user_data)
{
  GcalSourceDialogPrivate *priv = GCAL_SOURCE_DIALOG (user_data)->priv;

  if (priv->source != NULL)
    {
      GList *children, *l;
      gchar *str;

      priv->removed_source = priv->source;
      priv->source = NULL;
      children = gtk_container_get_children (GTK_CONTAINER (priv->calendars_listbox));

      gtk_revealer_set_reveal_child (GTK_REVEALER (priv->notification), TRUE);

      // Remove the listbox entry (if any)
      for (l = children; l != NULL; l = l->next)
        {
          if (g_object_get_data (l->data, "source") == priv->removed_source)
            {
              gtk_widget_destroy (l->data);
              break;
            }
        }

      // Update notification label
      str = g_strdup_printf (_("Calendar <b>%s</b> removed"), e_source_get_display_name (priv->removed_source));
      gtk_label_set_markup (GTK_LABEL (priv->notification_label), str);

      // Remove old notifications
      if (priv->notification_timeout_id != 0)
        g_source_remove (priv->notification_timeout_id);

      priv->notification_timeout_id = g_timeout_add_seconds (5, hide_notification_scheduled, user_data);

      // Disable the source, so it gets hidden
      gcal_manager_disable_source (priv->manager, priv->removed_source);

      g_list_free (children);
      g_free (str);
    }

  gcal_source_dialog_set_mode (GCAL_SOURCE_DIALOG (user_data), GCAL_SOURCE_DIALOG_MODE_NORMAL);
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
  GtkBuilder *builder;
  GMenuModel *menu;

  G_OBJECT_CLASS (gcal_source_dialog_parent_class)->constructed (object);

  /* widget responses */
  gtk_dialog_set_default_response (GTK_DIALOG (object), GTK_RESPONSE_CANCEL);

  g_object_set_data (G_OBJECT (priv->remove_button), "response", GINT_TO_POINTER (GCAL_RESPONSE_REMOVE_SOURCE));

  // Setup listbox header functions
  gtk_list_box_set_header_func (GTK_LIST_BOX (priv->calendars_listbox), display_header_func, NULL, NULL);
  gtk_list_box_set_sort_func (GTK_LIST_BOX (priv->calendars_listbox), (GtkListBoxSortFunc) calendar_listbox_sort_func,
                              object, NULL);

  gtk_list_box_set_header_func (GTK_LIST_BOX (priv->online_accounts_listbox), display_header_func, NULL, NULL);

  // Action group
  priv->action_group = g_simple_action_group_new ();
  gtk_widget_insert_action_group (GTK_WIDGET (object), "source", G_ACTION_GROUP (priv->action_group));

  g_action_map_add_action_entries (G_ACTION_MAP (priv->action_group), actions, G_N_ELEMENTS (actions), object);

  // Load the "Add" button menu
  builder = gtk_builder_new_from_resource ("/org/gnome/calendar/menus.ui");

  menu = G_MENU_MODEL (gtk_builder_get_object (builder, "add-source-menu"));
  gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (priv->add_calendar_menu_button), menu);

  g_object_unref (builder);

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

  gtk_widget_class_bind_template_child_private (widget_class, GcalSourceDialog, account_box);
  gtk_widget_class_bind_template_child_private (widget_class, GcalSourceDialog, account_label);
  gtk_widget_class_bind_template_child_private (widget_class, GcalSourceDialog, add_button);
  gtk_widget_class_bind_template_child_private (widget_class, GcalSourceDialog, add_calendar_menu_button);
  gtk_widget_class_bind_template_child_private (widget_class, GcalSourceDialog, back_button);
  gtk_widget_class_bind_template_child_private (widget_class, GcalSourceDialog, calendar_address_entry);
  gtk_widget_class_bind_template_child_private (widget_class, GcalSourceDialog, calendar_color_button);
  gtk_widget_class_bind_template_child_private (widget_class, GcalSourceDialog, calendar_url_button);
  gtk_widget_class_bind_template_child_private (widget_class, GcalSourceDialog, calendar_visible_check);
  gtk_widget_class_bind_template_child_private (widget_class, GcalSourceDialog, calendars_listbox);
  gtk_widget_class_bind_template_child_private (widget_class, GcalSourceDialog, cancel_button);
  gtk_widget_class_bind_template_child_private (widget_class, GcalSourceDialog, credentials_cancel_button);
  gtk_widget_class_bind_template_child_private (widget_class, GcalSourceDialog, credentials_connect_button);
  gtk_widget_class_bind_template_child_private (widget_class, GcalSourceDialog, credentials_dialog);
  gtk_widget_class_bind_template_child_private (widget_class, GcalSourceDialog, credentials_password_entry);
  gtk_widget_class_bind_template_child_private (widget_class, GcalSourceDialog, credentials_user_entry);
  gtk_widget_class_bind_template_child_private (widget_class, GcalSourceDialog, default_check);
  gtk_widget_class_bind_template_child_private (widget_class, GcalSourceDialog, edit_grid);
  gtk_widget_class_bind_template_child_private (widget_class, GcalSourceDialog, headerbar);
  gtk_widget_class_bind_template_child_private (widget_class, GcalSourceDialog, location_dim_label);
  gtk_widget_class_bind_template_child_private (widget_class, GcalSourceDialog, name_entry);
  gtk_widget_class_bind_template_child_private (widget_class, GcalSourceDialog, notification);
  gtk_widget_class_bind_template_child_private (widget_class, GcalSourceDialog, notification_label);
  gtk_widget_class_bind_template_child_private (widget_class, GcalSourceDialog, online_accounts_listbox);
  gtk_widget_class_bind_template_child_private (widget_class, GcalSourceDialog, remove_button);
  gtk_widget_class_bind_template_child_private (widget_class, GcalSourceDialog, stack);
  gtk_widget_class_bind_template_child_private (widget_class, GcalSourceDialog, web_sources_listbox);
  gtk_widget_class_bind_template_child_private (widget_class, GcalSourceDialog, web_sources_revealer);

  gtk_widget_class_bind_template_callback (widget_class, add_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, action_widget_activated);
  gtk_widget_class_bind_template_callback (widget_class, back_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, calendar_file_selected);
  gtk_widget_class_bind_template_callback (widget_class, calendar_listbox_row_activated);
  gtk_widget_class_bind_template_callback (widget_class, calendar_visible_check_toggled);
  gtk_widget_class_bind_template_callback (widget_class, cancel_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, credential_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, credential_entry_activate);
  gtk_widget_class_bind_template_callback (widget_class, color_set);
  gtk_widget_class_bind_template_callback (widget_class, default_check_toggled);
  gtk_widget_class_bind_template_callback (widget_class, description_label_link_activated);
  gtk_widget_class_bind_template_callback (widget_class, hide_notification);
  gtk_widget_class_bind_template_callback (widget_class, name_entry_text_changed);
  gtk_widget_class_bind_template_callback (widget_class, notification_child_revealed_changed);
  gtk_widget_class_bind_template_callback (widget_class, online_accounts_settings_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, remove_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, response_signal);
  gtk_widget_class_bind_template_callback (widget_class, stack_visible_child_name_changed);
  gtk_widget_class_bind_template_callback (widget_class, undo_remove_action);
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
  g_signal_connect (priv->manager, "source-removed", G_CALLBACK (remove_source), dialog);
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

  priv->mode = mode;

  // Cleanup old data
  clear_pages (dialog);

  switch (mode)
    {
    case GCAL_SOURCE_DIALOG_MODE_CREATE:
      gtk_header_bar_set_title (GTK_HEADER_BAR (priv->headerbar), _("Add Calendar"));
      gtk_header_bar_set_subtitle (GTK_HEADER_BAR (priv->headerbar), NULL);
      gtk_stack_set_visible_child_name (GTK_STACK (priv->stack), "edit");
      break;

    case GCAL_SOURCE_DIALOG_MODE_CREATE_WEB:
      gtk_header_bar_set_title (GTK_HEADER_BAR (priv->headerbar), _("Add Calendar"));
      gtk_header_bar_set_subtitle (GTK_HEADER_BAR (priv->headerbar), NULL);
      gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (priv->headerbar), FALSE);
      gtk_stack_set_visible_child_name (GTK_STACK (priv->stack), "create");
      gtk_widget_set_visible (priv->add_button, TRUE);
      gtk_widget_set_visible (priv->cancel_button, TRUE);
      break;

    case GCAL_SOURCE_DIALOG_MODE_EDIT:
      // Bind title
      if (priv->title_bind == NULL)
        {
          priv->title_bind = g_object_bind_property (priv->name_entry, "text", priv->headerbar, "title",
                                                     G_BINDING_DEFAULT);
        }

      gtk_stack_set_visible_child_name (GTK_STACK (priv->stack), "edit");
      break;

    case GCAL_SOURCE_DIALOG_MODE_NORMAL:
      /* Free any bindings left behind */
      if (priv->title_bind != NULL)
        {
          g_binding_unbind (priv->title_bind);
          priv->title_bind = NULL;
        }

      gtk_header_bar_set_title (GTK_HEADER_BAR (priv->headerbar), _("Calendar Settings"));
      gtk_header_bar_set_subtitle (GTK_HEADER_BAR (priv->headerbar), NULL);
      gtk_stack_set_visible_child_name (GTK_STACK (priv->stack), "main");
      break;

    default:
      g_assert_not_reached ();
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
