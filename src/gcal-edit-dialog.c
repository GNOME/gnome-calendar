/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 2 -*- */
/*
 * gcal-ediat-dialog.c
 * Copyright (C) 2012 Erick Pérez Castellanos <erickpc@gnome.org>
 *
 * gnome-calendar is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * gnome-calendar is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "gcal-edit-dialog.h"
#include "gcal-utils.h"

#include <glib/gi18n.h>

struct _GcalEditDialogPrivate
{
  gchar       *source_uid;
  gchar       *event_uid;

  GcalManager *manager;
  GtkTreeIter *active_iter;

  GtkWidget   *delete_button;
  GtkWidget   *summary_entry;
  GtkWidget   *calendar_button;
  GtkWidget   *start_date_entry;
  GtkWidget   *end_date_entry;
  GtkWidget   *all_day_check;
  GtkWidget   *start_time_entry;
  GtkWidget   *end_time_entry;
  GtkWidget   *location_entry;
  GtkWidget   *notes_text;
};

static void        gcal_edit_dialog_constructed           (GObject           *object);

static void        gcal_edit_dialog_finalize              (GObject           *object);

static void        gcal_edit_dialog_calendar_selected     (GtkWidget         *menu_item,
                                                           gpointer           user_data);

static void        gcal_edit_dialog_set_writable          (GcalEditDialog    *dialog,
                                                           gboolean           writable);

static void        gcal_edit_dialog_clear_data            (GcalEditDialog    *dialog);

static void        gcal_edit_dialog_action_button_clicked (GtkWidget         *widget,
                                                           gpointer           user_data);

G_DEFINE_TYPE(GcalEditDialog, gcal_edit_dialog, GTK_TYPE_DIALOG)

static void
gcal_edit_dialog_class_init (GcalEditDialogClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->constructed = gcal_edit_dialog_constructed;
  object_class->finalize = gcal_edit_dialog_finalize;

  g_type_class_add_private ((gpointer)klass, sizeof(GcalEditDialogPrivate));
}

static void
gcal_edit_dialog_init (GcalEditDialog *self)
{
  GcalEditDialogPrivate *priv;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            GCAL_TYPE_EDIT_DIALOG,
                                            GcalEditDialogPrivate);
  priv = self->priv;

  priv->source_uid = NULL;
  priv->event_uid = NULL;
}

static void
gcal_edit_dialog_constructed (GObject* object)
{
  GcalEditDialogPrivate *priv;
  GtkWidget *content_area;
  GtkWidget *grid;
  GtkWidget *child;
  GtkWidget *action_area;
  GtkWidget *button;

  priv = GCAL_EDIT_DIALOG (object)->priv;

  /* chaining up */
  G_OBJECT_CLASS (gcal_edit_dialog_parent_class)->constructed (object);

  gtk_window_set_title (GTK_WINDOW (object), _("Event Details"));
  gtk_window_set_modal (GTK_WINDOW (object), TRUE);
  gtk_window_set_destroy_with_parent (GTK_WINDOW (object), TRUE);
  gtk_window_set_resizable (GTK_WINDOW (object), FALSE);

  gtk_container_set_border_width (GTK_CONTAINER (object), 6);

  content_area = gtk_dialog_get_content_area (GTK_DIALOG (object));

  /* edit area, grid */
  grid = gtk_grid_new ();
  g_object_set (grid,
                "row-spacing", 24,
                "column-spacing", 12,
                "border-width", 6,
                NULL);

  /* Summary, title */
  child = gtk_label_new (_("Title"));
  gtk_widget_set_halign (child, GTK_ALIGN_END);
  gtk_grid_attach (GTK_GRID (grid), child, 0, 0, 1, 1);

  priv->summary_entry = gtk_entry_new ();
  gtk_widget_set_hexpand (priv->summary_entry, TRUE);
  gtk_grid_attach (GTK_GRID (grid), priv->summary_entry, 1, 0, 1, 1);

  /* Calendar, source */
  priv->calendar_button = gtk_menu_button_new ();
  child = gtk_image_new_from_icon_name ("x-office-calendar-symbolic",
                                        GTK_ICON_SIZE_MENU);
  g_object_set (priv->calendar_button,
                "vexpand", TRUE,
                "always-show-image", TRUE,
                "image", child,
                "menu", gtk_menu_new (),
                NULL);
  gtk_grid_attach (GTK_GRID (grid), priv->calendar_button, 2, 0, 1, 1);

  /* Start date and time  */
  child = gtk_label_new (_("Starts"));
  gtk_widget_set_halign (child, GTK_ALIGN_END);
  gtk_grid_attach (GTK_GRID (grid), child, 0, 2, 1, 1);

  child = gtk_grid_new ();
  g_object_set (child,
                "orientation", GTK_ORIENTATION_HORIZONTAL,
                "column-spacing", 12,
                "column-homogeneous", FALSE,
                NULL);
  priv->start_date_entry = gtk_entry_new ();
  gtk_entry_set_icon_from_icon_name (GTK_ENTRY (priv->start_date_entry),
                                     GTK_ENTRY_ICON_SECONDARY,
                                     "x-office-calendar-symbolic");
  gtk_container_add (GTK_CONTAINER (child), priv->start_date_entry);

  priv->start_time_entry = gtk_entry_new ();
  gtk_entry_set_width_chars (GTK_ENTRY (priv->start_time_entry), 8);
  gtk_entry_set_icon_from_icon_name (GTK_ENTRY (priv->start_time_entry),
                                     GTK_ENTRY_ICON_SECONDARY,
                                     "preferences-system-time-symbolic");
  gtk_container_add (GTK_CONTAINER (child), priv->start_time_entry);

  priv->all_day_check = gtk_check_button_new_with_label (_("All day"));
  gtk_container_add (GTK_CONTAINER (child), priv->all_day_check);
  gtk_grid_attach (GTK_GRID (grid), child, 1, 2, 2, 1);

  /* End date and time  */
  child = gtk_label_new (_("Ends"));
  gtk_widget_set_halign (child, GTK_ALIGN_END);
  gtk_grid_attach (GTK_GRID (grid), child, 0, 3, 1, 1);

  child = gtk_grid_new ();
  g_object_set (child,
                "orientation", GTK_ORIENTATION_HORIZONTAL,
                "column-spacing", 12,
                NULL);
  priv->end_date_entry = gtk_entry_new ();
  gtk_entry_set_icon_from_icon_name (GTK_ENTRY (priv->end_date_entry),
                                     GTK_ENTRY_ICON_SECONDARY,
                                     "x-office-calendar-symbolic");
  gtk_container_add (GTK_CONTAINER (child), priv->end_date_entry);

  priv->end_time_entry = gtk_entry_new ();
  gtk_entry_set_width_chars (GTK_ENTRY (priv->end_time_entry), 8);
  gtk_entry_set_icon_from_icon_name (GTK_ENTRY (priv->end_time_entry),
                                     GTK_ENTRY_ICON_SECONDARY,
                                     "preferences-system-time-symbolic");
  gtk_container_add (GTK_CONTAINER (child), priv->end_time_entry);
  gtk_grid_attach (GTK_GRID (grid), child, 1, 3, 2, 1);

  /* Location, location, location */
  child = gtk_label_new (_("Location"));
  gtk_widget_set_halign (child, GTK_ALIGN_END);
  gtk_grid_attach (GTK_GRID (grid), child, 0, 4, 1, 1);

  priv->location_entry = gtk_entry_new ();
  gtk_widget_set_hexpand (priv->location_entry, TRUE);
  gtk_entry_set_icon_from_icon_name (GTK_ENTRY (priv->location_entry),
                                     GTK_ENTRY_ICON_SECONDARY,
                                     "find-location-symbolic");
  gtk_grid_attach (GTK_GRID (grid), priv->location_entry, 1, 4, 2, 1);

  /* Notes, description */
  child = gtk_label_new (_("Notes"));
  gtk_widget_set_halign (child, GTK_ALIGN_END);
  gtk_widget_set_valign (child, GTK_ALIGN_START);
  gtk_widget_set_margin_top (child, 4);
  gtk_grid_attach (GTK_GRID (grid), child, 0, 5, 1, 1);

  child = gtk_frame_new (NULL);
  priv->notes_text = gtk_text_view_new ();
  gtk_container_add (GTK_CONTAINER (child), priv->notes_text);
  gtk_widget_set_hexpand (child, TRUE);
  gtk_widget_set_size_request (
      gtk_bin_get_child (GTK_BIN (child)),
      -1, 80);
  gtk_grid_attach (GTK_GRID (grid), child, 1, 5, 2, 1);

  gtk_container_add (GTK_CONTAINER (content_area), grid);
  gtk_widget_show_all (content_area);

  /* action area, buttons */
  action_area = gtk_dialog_get_action_area (GTK_DIALOG (object));
  gtk_box_set_spacing (GTK_BOX (action_area), 6);

  button = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
  gtk_widget_set_can_default (button, TRUE);
  g_object_set_data (G_OBJECT (button),
                     "response",
                     GINT_TO_POINTER (GTK_RESPONSE_CANCEL));
  g_signal_connect (button,
                    "clicked",
                    G_CALLBACK (gcal_edit_dialog_action_button_clicked),
                    object);
  gtk_box_pack_end (GTK_BOX (action_area), button, FALSE, TRUE, 0);
  gtk_button_box_set_child_secondary (GTK_BUTTON_BOX (action_area),
                                      button,
                                      TRUE);

  priv->delete_button = gtk_button_new ();
  gtk_container_add (
      GTK_CONTAINER (priv->delete_button),
      gtk_image_new_from_icon_name ("user-trash-symbolic",
                                    GTK_ICON_SIZE_MENU));
  gtk_widget_set_can_default (priv->delete_button, TRUE);
  g_object_set_data (G_OBJECT (priv->delete_button),
                     "response",
                     GINT_TO_POINTER (GCAL_RESPONSE_DELETE_EVENT));
  g_signal_connect (priv->delete_button,
                    "clicked",
                    G_CALLBACK (gcal_edit_dialog_action_button_clicked),
                    object);
  gtk_box_pack_end (GTK_BOX (action_area), priv->delete_button, TRUE, TRUE, 0);
  gtk_button_box_set_child_non_homogeneous (GTK_BUTTON_BOX (action_area),
                                            priv->delete_button,
                                            TRUE);

  /* done button */
  button = gtk_button_new_with_label (_("Done"));
  gtk_widget_set_can_default (button, TRUE);
  g_object_set_data (G_OBJECT (button),
                     "response",
                     GINT_TO_POINTER (GTK_RESPONSE_ACCEPT));
  g_signal_connect (button,
                    "clicked",
                    G_CALLBACK (gcal_edit_dialog_action_button_clicked),
                    object);
  gtk_style_context_add_class (
      gtk_widget_get_style_context (button),
      "suggested-action");
  gtk_box_pack_end (GTK_BOX (action_area), button, TRUE, TRUE, 0);

  gtk_widget_show_all (action_area);

  /* signals handlers */
}

static void
gcal_edit_dialog_finalize (GObject *object)
{
  GcalEditDialogPrivate *priv;

  priv = GCAL_EDIT_DIALOG (object)->priv;

  if (priv->source_uid != NULL)
    g_free (priv->source_uid);

  if (priv->event_uid != NULL)
    g_free (priv->event_uid);

  G_OBJECT_CLASS (gcal_edit_dialog_parent_class)->finalize (object);
}

static void
gcal_edit_dialog_calendar_selected (GtkWidget *menu_item,
                                    gpointer   user_data)
{
  GcalEditDialogPrivate *priv;

  GtkListStore *sources_model;
  GtkTreeIter *iter;
  GdkColor *color;

  GdkPixbuf *pix;
  GtkWidget *cal_image;

  priv = GCAL_EDIT_DIALOG (user_data)->priv;
  iter = g_object_get_data (G_OBJECT (menu_item), "sources-iter");

  sources_model = gcal_manager_get_sources_model (priv->manager);
  gtk_tree_model_get (GTK_TREE_MODEL (sources_model), iter,
                      3, &color,
                      -1);

  if (priv->active_iter != NULL)
    gtk_tree_iter_free (priv->active_iter);
  priv->active_iter = gtk_tree_iter_copy (iter);

  pix = gcal_get_pixbuf_from_color (color);
  cal_image = gtk_image_new_from_pixbuf (pix);
  gtk_button_set_image (GTK_BUTTON (priv->calendar_button), cal_image);

  gdk_color_free (color);
  g_object_unref (pix);
}

static void
gcal_edit_dialog_set_writable (GcalEditDialog *dialog,
                               gboolean        writable)
{
  GcalEditDialogPrivate *priv;

  priv = dialog->priv;

  gtk_widget_set_sensitive (priv->summary_entry, writable);
  gtk_widget_set_sensitive (priv->calendar_button, writable);
  gtk_widget_set_sensitive (priv->start_date_entry, writable);
  gtk_widget_set_sensitive (priv->start_time_entry, writable);
  gtk_widget_set_sensitive (priv->all_day_check, writable);
  gtk_widget_set_sensitive (priv->end_date_entry, writable);
  gtk_widget_set_sensitive (priv->end_time_entry, writable);
  gtk_widget_set_sensitive (priv->location_entry, writable);
  gtk_widget_set_sensitive (priv->notes_text, writable);

  /* add delete_button here */
  gtk_widget_set_sensitive (priv->delete_button, writable);
}

static void
gcal_edit_dialog_clear_data (GcalEditDialog *dialog)
{
  GcalEditDialogPrivate *priv;

  priv = dialog->priv;

  /* FIXME: add clear for the rest of the fields when I define loading code for
   * those below */
  gtk_entry_set_text (GTK_ENTRY (priv->summary_entry), "");
  gtk_entry_set_text (GTK_ENTRY (priv->location_entry), "");

  gtk_text_buffer_set_text (
      gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->notes_text)),
      "",
      -1);
}

static void
gcal_edit_dialog_action_button_clicked (GtkWidget *widget,
                                        gpointer   user_data)
{
  gint response;

  response = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (widget),
                                                  "response"));

  gtk_dialog_response (GTK_DIALOG (user_data), response);
}

/* Public API */
GtkWidget*
gcal_edit_dialog_new (void)
{
  return g_object_new (GCAL_TYPE_EDIT_DIALOG, NULL);
}

void
gcal_edit_dialog_set_event (GcalEditDialog *dialog,
                            const gchar    *source_uid,
                            const gchar    *event_uid)
{
  GcalEditDialogPrivate *priv;

  gchar *text;
  const gchar *const_text;
  gboolean all_day;

  priv = dialog->priv;
  all_day = FALSE;

  if (priv->source_uid != NULL)
    g_free (priv->source_uid);
  priv->source_uid = g_strdup (source_uid);

  if (priv->event_uid != NULL)
    g_free (priv->event_uid);
  priv->event_uid = g_strdup (event_uid);

  /* Load event data */
  gcal_edit_dialog_clear_data (dialog);

  /* summary */
  text = gcal_manager_get_event_summary (priv->manager,
                                         priv->source_uid,
                                         priv->event_uid);
  gtk_entry_set_text (GTK_ENTRY (priv->summary_entry),
                      text != NULL ? text : "");
  g_free (text);

  /* start date */

  /* all_day  */
  all_day = gcal_manager_get_event_all_day (priv->manager,
                                            priv->source_uid,
                                            priv->event_uid);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->all_day_check),
                                all_day);

  /* start time */
  if (all_day)
    {
      /* FIXME: set time to 00:00 */
    }

  /* end date */

  /* end time */
  if (all_day)
    {
      /* FIXME: set time to 00:00 */
    }

  /* location */
  const_text = gcal_manager_get_event_location (priv->manager,
                                          priv->source_uid,
                                          priv->event_uid);
  gtk_entry_set_text (GTK_ENTRY (priv->location_entry),
                      const_text != NULL ? const_text : "");

  /* notes */
  text = gcal_manager_get_event_description (priv->manager,
                                             priv->source_uid,
                                             priv->event_uid);
  gtk_text_buffer_set_text (
      gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->notes_text)),
      text != NULL ? text : "",
      -1);
  g_free (text);

  /* FIXME: this is for now, I will have to ask GcalManager */
  gcal_edit_dialog_set_writable (
      dialog,
      ! gcal_manager_get_source_readonly (priv->manager,
                                          priv->source_uid));
}

void
gcal_edit_dialog_set_manager (GcalEditDialog *dialog,
                              GcalManager    *manager)
{
  GcalEditDialogPrivate *priv;

  GtkMenu *menu;
  GtkWidget *item;
  GList *children;
  GtkListStore *sources_model;
  gboolean valid;
  GtkTreeIter iter;

  g_return_if_fail (GCAL_IS_MANAGER (manager));
  priv = dialog->priv;

  priv->manager = manager;

  /* Loading calendars */


  sources_model = gcal_manager_get_sources_model (priv->manager);
  valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (sources_model),
                                         &iter);
  if (! valid)
    return;

  menu = gtk_menu_button_get_popup (GTK_MENU_BUTTON (priv->calendar_button));
  children = gtk_container_get_children (GTK_CONTAINER (menu));
  g_list_foreach (children, (GFunc) gtk_widget_destroy, NULL);

  while (valid)
    {
      /* Walk through the list, reading each row */
      gchar *name;
      GdkColor *color;
      GtkWidget *cal_image;
      GdkPixbuf *pix;

      gtk_tree_model_get (GTK_TREE_MODEL (sources_model), &iter,
                          1, &name,
                          3, &color,
                          -1);

      item = gtk_image_menu_item_new ();
      g_object_set_data_full (G_OBJECT (item),
                              "sources-iter",
                              gtk_tree_iter_copy (&iter),
                              (GDestroyNotify) gtk_tree_iter_free);
      g_signal_connect (item,
                        "activate",
                        G_CALLBACK (gcal_edit_dialog_calendar_selected),
                        dialog);

      pix = gcal_get_pixbuf_from_color (color);

      cal_image = gtk_image_new_from_pixbuf (pix);
      g_object_set (item,
                    "always-show-image", TRUE,
                    "image", cal_image,
                    "label", name,
                    NULL);

      gtk_container_add (GTK_CONTAINER (menu), item);

      g_object_unref (pix);
      g_free (name);
      gdk_color_free (color);

      valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (sources_model),
                                        &iter);
    }

  gtk_widget_show_all (GTK_WIDGET (menu));
}
