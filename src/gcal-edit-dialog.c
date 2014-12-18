/* -*- mode: c; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#include "gcal-time-selector.h"
#include "gcal-date-entry.h"
#include "gcal-utils.h"

#include <libecal/libecal.h>
#include <glib/gi18n.h>

typedef struct
{
  gchar            *source_uid;
  gchar            *event_uid;
  gchar            *event_rid;
  gboolean          writable;

  GcalManager      *manager;
  GtkTreeIter      *active_iter;

  GtkWidget        *titlebar;
  GtkWidget        *lock;
  GtkWidget        *source_image;
  GtkWidget        *source_label;

  GtkWidget        *delete_button;
  GtkWidget        *done_button;
  GtkWidget        *cancel_button;
  GtkWidget        *sources_button;

  GtkWidget        *summary_entry;

  GtkWidget        *start_date_entry;
  GtkWidget        *end_date_entry;
  GtkWidget        *all_day_check;
  GtkWidget        *start_time_selector;
  GtkWidget        *end_time_selector;
  GtkWidget        *location_entry;
  GtkWidget        *notes_text;

  /* actions */
  GMenu              *sources_menu;
  GSimpleAction      *action;
  GSimpleActionGroup *action_group;

  /* new data holders */
  ESource          *source; /* weak reference */
  ECalComponent    *component;

  /* flags */
  gboolean          event_is_new;
  gboolean          setting_event;
} GcalEditDialogPrivate;

static void        fill_sources_menu                      (GcalEditDialog    *dialog);

static void        on_source_activate                     (GcalManager       *manager,
                                                           ESource           *source,
                                                           gboolean           active,
                                                           gpointer           user_data);

static void        on_source_added                        (GcalManager       *manager,
                                                           ESource           *source,
                                                           gpointer           user_data);

static void        on_source_removed                      (GcalManager       *manager,
                                                           ESource           *source,
                                                           gpointer           user_data);

static void        on_calendar_selected                   (GtkWidget         *menu_item,
                                                           GVariant          *value,
                                                           gpointer           user_data);

static void        update_date                            (GtkEntry          *entry,
                                                           gpointer           user_data);

static void        update_location                        (GtkEntry          *entry,
                                                           GParamSpec        *pspec,
                                                           gpointer           user_data);

static void        update_notes                           (GtkTextBuffer     *buffer,
                                                           gpointer           user_data);

static void        update_summary                         (GtkEntry          *entry,
                                                           GParamSpec        *pspec,
                                                           gpointer           user_data);

static void        update_time                            (GtkEntry          *entry,
                                                           gpointer           user_data);

static void        gcal_edit_dialog_constructed           (GObject           *object);

static void        gcal_edit_dialog_finalize              (GObject           *object);

static void        gcal_edit_dialog_set_writable          (GcalEditDialog    *dialog,
                                                           gboolean           writable);

static void        gcal_edit_dialog_clear_data            (GcalEditDialog    *dialog);

static void        gcal_edit_dialog_action_button_clicked (GtkWidget         *widget,
                                                           gpointer           user_data);

static void        gcal_edit_dialog_all_day_changed       (GtkWidget         *widget,
                                                           gpointer           user_data);

G_DEFINE_TYPE_WITH_PRIVATE (GcalEditDialog, gcal_edit_dialog, GTK_TYPE_DIALOG)

static void
fill_sources_menu (GcalEditDialog *dialog)
{
  GcalEditDialogPrivate *priv;
  GList *list;
  GList *aux;

  priv = gcal_edit_dialog_get_instance_private (dialog);
  list = gcal_manager_get_sources (priv->manager);

  /* clear menu */
  if (priv->sources_menu)
    g_menu_remove_all (priv->sources_menu);

  for (aux = list; aux != NULL; aux = aux->next)
    {
      ESource *source;
      GMenuItem *item;
      ESourceSelectable *extension;
      GdkRGBA color;
      GdkPixbuf *pix;

      source = E_SOURCE (aux->data);

      /* retrieve color */
      extension = E_SOURCE_SELECTABLE (e_source_get_extension (source, E_SOURCE_EXTENSION_CALENDAR));
      gdk_rgba_parse (&color, e_source_selectable_get_color (E_SOURCE_SELECTABLE (extension)));
      pix = gcal_get_pixbuf_from_color (&color, 16);

      /* menu item */
      item = g_menu_item_new (e_source_get_display_name (source), "edit.select_calendar");
      g_menu_item_set_icon (item, G_ICON (pix));

      /* set insensitive for read-only calendars */
      if (gcal_manager_is_client_writable (priv->manager, source))
        {
          g_menu_item_set_action_and_target_value (item, "edit.select_calendar", NULL);
        }
      else
        {
          g_menu_item_set_action_and_target_value (item, "edit.select_calendar",
                                                   g_variant_new_string (e_source_get_uid (source)));
        }

      g_menu_append_item (priv->sources_menu, item);

      g_object_unref (pix);
      g_object_unref (item);
    }

  g_list_free (list);
}

static void
on_source_activate (GcalManager *manager,
                    ESource     *source,
                    gboolean     active,
                    gpointer     user_data)
{
  fill_sources_menu (GCAL_EDIT_DIALOG (user_data));
}

static void
on_source_added (GcalManager *manager,
                 ESource     *source,
                 gpointer     user_data)
{
  fill_sources_menu (GCAL_EDIT_DIALOG (user_data));
}

static void
on_source_removed (GcalManager *manager,
                   ESource     *source,
                   gpointer     user_data)
{
  fill_sources_menu (GCAL_EDIT_DIALOG (user_data));
}

static void
on_calendar_selected (GtkWidget *menu_item,
                      GVariant  *value,
                      gpointer   user_data)
{
  GcalEditDialogPrivate *priv;
  GList *list;
  GList *aux;
  gchar *uid;

  priv = gcal_edit_dialog_get_instance_private (GCAL_EDIT_DIALOG (user_data));
  list = gcal_manager_get_sources (priv->manager);

  /* retrieve selected calendar uid */
  g_variant_get (value, "s", &uid);

  /* search for any source with the given UID */
  for (aux = list; aux != NULL; aux = aux->next)
    {
      ESource *source;
      source = E_SOURCE (aux->data);

      if (g_strcmp0 (e_source_get_uid (source), uid) == 0)
      {
        GdkRGBA color;
        ESourceSelectable *extension;
        GdkPixbuf *pix;

        /* retrieve color */
        extension = E_SOURCE_SELECTABLE (e_source_get_extension (source, E_SOURCE_EXTENSION_CALENDAR));
        gdk_rgba_parse (&color, e_source_selectable_get_color (E_SOURCE_SELECTABLE (extension)));

        pix = gcal_get_pixbuf_from_color (&color, 16);
        gtk_image_set_from_pixbuf (GTK_IMAGE (priv->source_image), pix);
        g_object_unref (pix);

        priv->source = source;
        gtk_header_bar_set_subtitle (GTK_HEADER_BAR (priv->titlebar),
                                     e_source_get_display_name (priv->source));
        break;
      }
    }

  g_free (uid);
  g_list_free (list);
}

static void
update_date (GtkEntry   *entry,
             gpointer    user_data)
{
  GcalEditDialogPrivate *priv;
  ECalComponentDateTime dtstart;
  ECalComponentDateTime dtend;
  icaltimetype *start_date;
  icaltimetype *end_date;

  priv = gcal_edit_dialog_get_instance_private (GCAL_EDIT_DIALOG (user_data));

  if (priv->setting_event)
    return;

  start_date = gcal_edit_dialog_get_start_date (GCAL_EDIT_DIALOG (user_data));
  end_date = gcal_edit_dialog_get_end_date (GCAL_EDIT_DIALOG (user_data));

  /* check if the start & end dates are sane */
  if (icaltime_compare (*start_date, *end_date) != -1)
    {
      /* change the non-editing entry */
      if (GTK_WIDGET (entry) == priv->start_date_entry)
        {
          end_date->day = start_date->day;
          end_date->month = start_date->month;
          end_date->year = start_date->year;

          if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->all_day_check)))
            {
              end_date->day += 1;
              *end_date = icaltime_normalize (*end_date);
          }
        }
      else
        {
          start_date->day = end_date->day;
          start_date->month = end_date->month;
          start_date->year = end_date->year;

          if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->all_day_check)))
            {
              start_date->day -= 1;
              *start_date = icaltime_normalize (*start_date);
          }
        }

      /* update the entries with the new sane values */
      g_signal_handlers_block_by_func (priv->start_date_entry,
                                       update_date,
                                       user_data);
      g_signal_handlers_block_by_func (priv->end_date_entry,
                                       update_date,
                                       user_data);

      gcal_date_entry_set_date (GCAL_DATE_ENTRY (priv->start_date_entry),
                                start_date->day,
                                start_date->month,
                                start_date->year);
      gcal_date_entry_set_date (GCAL_DATE_ENTRY (priv->end_date_entry),
                                end_date->day,
                                end_date->month,
                                end_date->year);

      g_signal_handlers_unblock_by_func (priv->start_date_entry,
                                       update_date,
                                       user_data);
      g_signal_handlers_unblock_by_func (priv->end_date_entry,
                                       update_date,
                                       user_data);
    }

  /* update component */
  e_cal_component_get_dtstart (priv->component, &dtstart);
  e_cal_component_get_dtend (priv->component, &dtend);

  *(dtstart.value) = *start_date;
  *(dtend.value) = *end_date;

  e_cal_component_set_dtstart (priv->component, &dtstart);
  e_cal_component_set_dtend (priv->component, &dtend);
  e_cal_component_commit_sequence (priv->component);

  g_free (start_date);
  g_free (end_date);
  e_cal_component_free_datetime (&dtstart);
  e_cal_component_free_datetime (&dtend);
}


static void
update_location (GtkEntry   *entry,
                 GParamSpec *pspec,
                 gpointer    user_data)
{
  GcalEditDialogPrivate *priv;

  priv = gcal_edit_dialog_get_instance_private (GCAL_EDIT_DIALOG (user_data));

  e_cal_component_set_location (priv->component, gtk_entry_get_text (entry));
  e_cal_component_commit_sequence (priv->component);
}

static void
update_notes (GtkTextBuffer *buffer,
              gpointer       user_data)
{
  GcalEditDialogPrivate *priv;
  GSList note;
  ECalComponentText text;
  gchar *note_text;

  priv = gcal_edit_dialog_get_instance_private (GCAL_EDIT_DIALOG (user_data));

  g_object_get (G_OBJECT (buffer),
                "text", &note_text,
                NULL);

  text.value = note_text;
  text.altrep = NULL;
  note.data = &text;
  note.next = NULL;

  e_cal_component_set_description_list (priv->component, &note);
  e_cal_component_commit_sequence (priv->component);

  g_free (note_text);
}

static void
update_summary (GtkEntry   *entry,
                GParamSpec *pspec,
                gpointer    user_data)
{
  GcalEditDialogPrivate *priv;

  ECalComponentText summary;

  priv = gcal_edit_dialog_get_instance_private (GCAL_EDIT_DIALOG (user_data));

  summary.value = g_strdup (gtk_entry_get_text (entry));
  summary.altrep = summary.value;

  e_cal_component_set_summary (priv->component, &summary);
  e_cal_component_commit_sequence (priv->component);
}

static void
update_time (GtkEntry   *entry,
             gpointer    user_data)
{
  GcalEditDialogPrivate *priv;
  ECalComponentDateTime dtstart;
  ECalComponentDateTime dtend;
  icaltimetype *start_date;
  icaltimetype *end_date;

  priv = gcal_edit_dialog_get_instance_private (GCAL_EDIT_DIALOG (user_data));

  if (priv->setting_event)
    return;

  start_date = gcal_edit_dialog_get_start_date (GCAL_EDIT_DIALOG (user_data));
  end_date = gcal_edit_dialog_get_end_date (GCAL_EDIT_DIALOG (user_data));

  /* check if the start & end dates are sane */
  if (icaltime_compare (*start_date, *end_date) > -1)
    {
      /* change the non-editing entry */
      if (GTK_WIDGET (entry) == priv->start_time_selector)
        {
          end_date->hour = start_date->hour + 1;
          end_date->minute = start_date->minute;
          *end_date = icaltime_normalize (*end_date);
        }
      else
        {
          start_date->hour = end_date->hour - 1;
          start_date->minute = end_date->minute;
          *start_date = icaltime_normalize (*start_date);
        }

      /* update the entries with the new sane values */
      g_signal_handlers_block_by_func (priv->start_time_selector,
                                       update_time,
                                       user_data);
      g_signal_handlers_block_by_func (priv->end_time_selector,
                                       update_time,
                                       user_data);

      /* updates date as well, since hours can change the current date */
      gcal_time_selector_set_time (GCAL_TIME_SELECTOR (priv->start_time_selector),
                                start_date->hour,
                                start_date->minute);
      gcal_date_entry_set_date (GCAL_DATE_ENTRY (priv->start_date_entry),
                                start_date->day,
                                start_date->month,
                                start_date->year);
      gcal_time_selector_set_time (GCAL_TIME_SELECTOR (priv->end_time_selector),
                                end_date->hour,
                                end_date->minute);
      gcal_date_entry_set_date (GCAL_DATE_ENTRY (priv->end_date_entry),
                                end_date->day,
                                end_date->month,
                                end_date->year);

      g_signal_handlers_unblock_by_func (priv->start_time_selector,
                                       update_time,
                                       user_data);
      g_signal_handlers_unblock_by_func (priv->end_time_selector,
                                       update_date,
                                       user_data);
    }

  g_debug ("Updating date: %s -> %s",
           icaltime_as_ical_string (*start_date),
           icaltime_as_ical_string (*end_date));

  /* update component */
  e_cal_component_get_dtstart (priv->component, &dtstart);
  e_cal_component_get_dtend (priv->component, &dtend);

  *(dtstart.value) = *start_date;
  *(dtend.value) = *end_date;

  e_cal_component_set_dtstart (priv->component, &dtstart);
  e_cal_component_set_dtend (priv->component, &dtend);
  e_cal_component_commit_sequence (priv->component);

  g_free (start_date);
  g_free (end_date);
  e_cal_component_free_datetime (&dtstart);
  e_cal_component_free_datetime (&dtend);
}

static void
gcal_edit_dialog_class_init (GcalEditDialogClass *klass)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->constructed = gcal_edit_dialog_constructed;
  object_class->finalize = gcal_edit_dialog_finalize;

  widget_class = GTK_WIDGET_CLASS (klass);
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/edit-dialog.ui");

  /* Buttons */
  gtk_widget_class_bind_template_child_private (widget_class, GcalEditDialog, done_button);
  gtk_widget_class_bind_template_child_private (widget_class, GcalEditDialog, cancel_button);
  gtk_widget_class_bind_template_child_private (widget_class, GcalEditDialog, delete_button);
  gtk_widget_class_bind_template_child_private (widget_class, GcalEditDialog, sources_button);
  /* Entries */
  gtk_widget_class_bind_template_child_private (widget_class, GcalEditDialog, summary_entry);
  gtk_widget_class_bind_template_child_private (widget_class, GcalEditDialog, start_time_selector);
  gtk_widget_class_bind_template_child_private (widget_class, GcalEditDialog, start_date_entry);
  gtk_widget_class_bind_template_child_private (widget_class, GcalEditDialog, end_time_selector);
  gtk_widget_class_bind_template_child_private (widget_class, GcalEditDialog, end_date_entry);
  gtk_widget_class_bind_template_child_private (widget_class, GcalEditDialog, location_entry);
  /* Other */
  gtk_widget_class_bind_template_child_private (widget_class, GcalEditDialog, notes_text);
  gtk_widget_class_bind_template_child_private (widget_class, GcalEditDialog, all_day_check);
  gtk_widget_class_bind_template_child_private (widget_class, GcalEditDialog, titlebar);
  gtk_widget_class_bind_template_child_private (widget_class, GcalEditDialog, lock);
  gtk_widget_class_bind_template_child_private (widget_class, GcalEditDialog, source_image);
}

static void
gcal_edit_dialog_init (GcalEditDialog *self)
{
  GcalEditDialogPrivate *priv;

  priv = gcal_edit_dialog_get_instance_private (self);

  priv->source_uid = NULL;
  priv->event_uid = NULL;
  priv->writable = TRUE;

  priv->setting_event = FALSE;

  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
gcal_edit_dialog_constructed (GObject* object)
{
  GcalEditDialogPrivate *priv;

  priv = gcal_edit_dialog_get_instance_private (GCAL_EDIT_DIALOG (object));

  /* chaining up */
  G_OBJECT_CLASS (gcal_edit_dialog_parent_class)->constructed (object);

  gtk_window_set_title (GTK_WINDOW (object), "");

  /* titlebar */
  gtk_window_set_titlebar (GTK_WINDOW (object), priv->titlebar);

  /* actions */
  priv->action_group = g_simple_action_group_new ();
  gtk_widget_insert_action_group (GTK_WIDGET (object),
                                  "edit",
                                  G_ACTION_GROUP (priv->action_group));

  priv->action = g_simple_action_new ("select_calendar", G_VARIANT_TYPE_STRING);
  g_signal_connect (priv->action,
                    "activate",
                    G_CALLBACK (on_calendar_selected), object);

  g_action_map_add_action (G_ACTION_MAP (priv->action_group), G_ACTION (priv->action));

  /* sources menu */
  priv->sources_menu = g_menu_new ();
  gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (priv->sources_button), G_MENU_MODEL (priv->sources_menu));

  /* bind title & symmary */
  g_object_bind_property (priv->summary_entry,
                          "text",
                          priv->titlebar,
                          "title",
                          G_BINDING_DEFAULT);

  /* bind all-day check button & time entries */
  g_object_bind_property (priv->all_day_check,
                          "active",
                          priv->start_time_selector,
                          "sensitive",
                          G_BINDING_DEFAULT | G_BINDING_INVERT_BOOLEAN);
  g_object_bind_property (priv->all_day_check,
                          "active",
                          priv->end_time_selector,
                          "sensitive",
                          G_BINDING_DEFAULT | G_BINDING_INVERT_BOOLEAN);

  /* action area, buttons */
  g_object_set_data (G_OBJECT (priv->cancel_button),
                     "response",
                     GINT_TO_POINTER (GTK_RESPONSE_CANCEL));
  g_signal_connect (priv->cancel_button,
                    "clicked",
                    G_CALLBACK (gcal_edit_dialog_action_button_clicked),
                    object);
  g_object_set_data (G_OBJECT (priv->delete_button),
                     "response",
                     GINT_TO_POINTER (GCAL_RESPONSE_DELETE_EVENT));
  g_signal_connect (priv->delete_button,
                    "clicked",
                    G_CALLBACK (gcal_edit_dialog_action_button_clicked),
                    object);
  g_object_set_data (G_OBJECT (priv->done_button),
                     "response",
                     GINT_TO_POINTER (GCAL_RESPONSE_SAVE_EVENT));
  g_signal_connect (priv->done_button,
                    "clicked",
                    G_CALLBACK (gcal_edit_dialog_action_button_clicked),
                    object);

  /* signals handlers */
  g_signal_connect (priv->all_day_check,
                    "toggled",
                    G_CALLBACK (gcal_edit_dialog_all_day_changed),
                    object);

  g_signal_connect (priv->summary_entry,
                    "notify::text",
                    G_CALLBACK (update_summary),
                    object);

  g_signal_connect (gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->notes_text)),
                    "changed",
                    G_CALLBACK (update_notes),
                    object);

  g_signal_connect (priv->location_entry,
                    "notify::text",
                    G_CALLBACK (update_location),
                    object);

  g_signal_connect (priv->start_date_entry,
                    "modified",
                    G_CALLBACK (update_date),
                    object);

  g_signal_connect (priv->end_date_entry,
                    "modified",
                    G_CALLBACK (update_date),
                    object);

  g_signal_connect (priv->start_time_selector,
                    "modified",
                    G_CALLBACK (update_time),
                    object);

  g_signal_connect (priv->end_time_selector,
                    "modified",
                    G_CALLBACK (update_time),
                    object);
}

static void
gcal_edit_dialog_finalize (GObject *object)
{
  GcalEditDialogPrivate *priv;

  priv = gcal_edit_dialog_get_instance_private (GCAL_EDIT_DIALOG (object));

  if (priv->source_uid != NULL)
    g_free (priv->source_uid);

  if (priv->event_uid != NULL)
    g_free (priv->event_uid);

  if (priv->source != NULL)
    g_object_unref (priv->source);

  if (priv->component != NULL)
    g_object_unref (priv->component);

  if (priv->action_group != NULL)
    g_object_unref (priv->action_group);

  if (priv->action != NULL)
    g_object_unref (priv->action);

  G_OBJECT_CLASS (gcal_edit_dialog_parent_class)->finalize (object);
}

static void
gcal_edit_dialog_set_writable (GcalEditDialog *dialog,
                               gboolean        writable)
{
  GcalEditDialogPrivate *priv;

  priv = gcal_edit_dialog_get_instance_private (dialog);

  priv->writable = writable;

  if (! writable)
    {
      gtk_widget_show (priv->lock);
    }
  else
    {
      gtk_widget_hide (priv->lock);
    }

  gtk_editable_set_editable (GTK_EDITABLE (priv->summary_entry), writable);
  gtk_editable_set_editable (GTK_EDITABLE (priv->start_date_entry), writable);
  gtk_editable_set_editable (GTK_EDITABLE (priv->end_date_entry), writable);
  gtk_editable_set_editable (GTK_EDITABLE (priv->location_entry), writable);

  gtk_text_view_set_editable (GTK_TEXT_VIEW (priv->notes_text), writable);

  gtk_widget_set_sensitive (priv->all_day_check, writable);

  if (!writable || (writable && gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->all_day_check))))
    {
      gtk_widget_set_sensitive (priv->start_time_selector, FALSE);
      gtk_widget_set_sensitive (priv->end_time_selector, FALSE);
    }
  else
    {
      gtk_widget_set_sensitive (priv->start_time_selector, TRUE);
      gtk_widget_set_sensitive (priv->end_time_selector, TRUE);
    }

  gtk_button_set_label (GTK_BUTTON (priv->done_button),
                        writable ? _("Save") : _("Done"));

  /* add delete_button here */
  gtk_widget_set_sensitive (priv->delete_button, writable);
}

static void
gcal_edit_dialog_clear_data (GcalEditDialog *dialog)
{
  GcalEditDialogPrivate *priv;

  priv = gcal_edit_dialog_get_instance_private (dialog);

  /* summary */
  g_signal_handlers_block_by_func (priv->summary_entry,
                                   update_summary,
                                   dialog);
  gtk_entry_set_text (GTK_ENTRY (priv->summary_entry), "");
  g_signal_handlers_unblock_by_func (priv->summary_entry,
                                     update_summary,
                                     dialog);

  /* calendar button */
  if (priv->active_iter != NULL)
    {
      gtk_tree_iter_free (priv->active_iter);
      priv->active_iter = NULL;
    }

  /* date and time */
  gtk_entry_set_text (GTK_ENTRY (priv->start_date_entry), "");
  gcal_time_selector_set_time (GCAL_TIME_SELECTOR (priv->start_time_selector), 0, 0);
  gtk_entry_set_text (GTK_ENTRY (priv->end_date_entry), "");
  gcal_time_selector_set_time (GCAL_TIME_SELECTOR (priv->end_time_selector), 0, 0);

  /* location */
  g_signal_handlers_block_by_func (priv->location_entry,
                                   update_location,
                                   dialog);
  gtk_entry_set_text (GTK_ENTRY (priv->location_entry), "");
  g_signal_handlers_unblock_by_func (priv->location_entry,
                                     update_location,
                                     dialog);

  /* notes */
  g_signal_handlers_block_by_func (gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->notes_text)),
                                   update_notes,
                                   dialog);
  gtk_text_buffer_set_text (
      gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->notes_text)),
      "",
      -1);
  g_signal_handlers_unblock_by_func (gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->notes_text)),
                                     update_notes,
                                     dialog);
}

static void
gcal_edit_dialog_action_button_clicked (GtkWidget *widget,
                                        gpointer   user_data)
{
  GcalEditDialogPrivate *priv;
  gint response;

  priv = gcal_edit_dialog_get_instance_private (GCAL_EDIT_DIALOG (user_data));
  response = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (widget),
                                                  "response"));

  if (response == GCAL_RESPONSE_SAVE_EVENT &&
      priv->event_is_new)
    {
      response = GCAL_RESPONSE_CREATE_EVENT;
    }

  gtk_dialog_response (GTK_DIALOG (user_data), response);
}

static void
gcal_edit_dialog_all_day_changed (GtkWidget *widget,
                                  gpointer   user_data)
{
  GcalEditDialogPrivate *priv;

  priv = gcal_edit_dialog_get_instance_private (GCAL_EDIT_DIALOG (user_data));

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->all_day_check)))
    {
      gcal_time_selector_set_time (GCAL_TIME_SELECTOR (priv->start_time_selector), 0, 0);
      gcal_time_selector_set_time (GCAL_TIME_SELECTOR (priv->end_time_selector), 0, 0);
    }
}

/* Public API */
GtkWidget*
gcal_edit_dialog_new (void)
{
  GtkWidget *dialog;

  dialog = g_object_new (GCAL_TYPE_EDIT_DIALOG, NULL);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

  return dialog;
}

void
gcal_edit_dialog_set_event_is_new (GcalEditDialog *dialog,
                                   gboolean        event_is_new)
{
  GcalEditDialogPrivate *priv;

  priv = gcal_edit_dialog_get_instance_private (dialog);
  priv->event_is_new = event_is_new;
  gtk_widget_set_visible (GTK_WIDGET (priv->delete_button), !event_is_new);

  /* FIXME: implement moving events to other sources */
  gtk_widget_set_sensitive (GTK_WIDGET (priv->sources_button), event_is_new);
  gtk_button_set_relief (GTK_BUTTON (priv->sources_button), event_is_new ? GTK_RELIEF_NORMAL : GTK_RELIEF_NONE);
}

void
gcal_edit_dialog_set_event_data (GcalEditDialog *dialog,
                                 GcalEventData  *data)
{
  GcalEditDialogPrivate *priv;

  GdkRGBA color;
  ESourceSelectable *extension;

  const gchar *const_text = NULL;
  gboolean all_day;
  gchar *description;

  ECalComponentId *id;
  ECalComponentText e_summary;
  ECalComponentDateTime dtstart;
  ECalComponentDateTime dtend;

  priv = gcal_edit_dialog_get_instance_private (dialog);
  all_day = FALSE;

  priv->setting_event = TRUE;

  if (priv->source != NULL)
    g_clear_object (&(priv->source));
  priv->source = g_object_ref (data->source);

  if (priv->component != NULL)
    g_object_unref (priv->component);
  priv->component = e_cal_component_clone (data->event_component);

  if (priv->source_uid != NULL)
    g_free (priv->source_uid);
  priv->source_uid = g_strdup (e_source_get_uid (priv->source));

  id = e_cal_component_get_id (priv->component);

  if (priv->event_uid != NULL)
    g_free (priv->event_uid);
  priv->event_uid = g_strdup (id->uid);

  if (priv->event_rid != NULL)
    g_clear_pointer (&(priv->event_rid), g_free);
  if (id->rid != NULL)
    {
      priv->event_rid = g_strdup (id->rid);
    }

  e_cal_component_free_id (id);

  /* Clear event data */
  gcal_edit_dialog_clear_data (dialog);

  /* Load new event data */
  /* summary */
  e_cal_component_get_summary (priv->component, &e_summary);
  gtk_entry_set_text (GTK_ENTRY (priv->summary_entry),
                      e_summary.value != NULL ? e_summary.value : "");

  /* dialog titlebar's title & subtitle */
  extension = E_SOURCE_SELECTABLE (e_source_get_extension (data->source, E_SOURCE_EXTENSION_CALENDAR));
  gdk_rgba_parse (
      &color,
      e_source_selectable_get_color (E_SOURCE_SELECTABLE (extension)));

  gtk_image_set_from_pixbuf (GTK_IMAGE (priv->source_image),
                             gcal_get_pixbuf_from_color (&color, 16));

  gtk_header_bar_set_subtitle (GTK_HEADER_BAR (priv->titlebar),
                               e_source_get_display_name (data->source));

  /* start date */
  e_cal_component_get_dtstart (priv->component, &dtstart);

  gcal_date_entry_set_date (GCAL_DATE_ENTRY (priv->start_date_entry),
                            dtstart.value->day,
                            dtstart.value->month,
                            dtstart.value->year);

  /* start time */
  if (all_day)
    {
      dtstart.value->hour = 0;
      dtstart.value->minute = 0;
    }
  gcal_time_selector_set_time (GCAL_TIME_SELECTOR (priv->start_time_selector), dtstart.value->hour, dtstart.value->minute);

  /* end date */
  e_cal_component_get_dtend (priv->component, &dtend);
  if (dtend.value != NULL)
    {
      gcal_date_entry_set_date (GCAL_DATE_ENTRY (priv->end_date_entry),
                                dtend.value->day, dtend.value->month, dtend.value->year);
      all_day = (dtstart.value->is_date == 1 && dtend.value->is_date == 1);

      if (!all_day)
        gcal_time_selector_set_time (GCAL_TIME_SELECTOR (priv->end_time_selector), dtend.value->hour, dtend.value->minute);
    }
  else
    {
      gcal_date_entry_set_date (GCAL_DATE_ENTRY (priv->end_date_entry),
                                dtstart.value->day, dtstart.value->month, dtstart.value->year);
      gcal_time_selector_set_time (GCAL_TIME_SELECTOR (priv->end_time_selector), dtstart.value->hour, dtstart.value->minute);
      all_day = FALSE;
    }

  e_cal_component_free_datetime (&dtstart);
  e_cal_component_free_datetime (&dtend);

  /* all_day  */
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->all_day_check), all_day);


  /* location */
  e_cal_component_get_location (priv->component, &const_text);
  gtk_entry_set_text (GTK_ENTRY (priv->location_entry),
                      const_text != NULL ? const_text : "");

  /* notes */
  description = get_desc_from_component (priv->component, "\n");

  if (description != NULL)
    {
      gtk_text_buffer_set_text (
          gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->notes_text)),
          description,
          -1);

      g_free (description);
    }

  gcal_edit_dialog_set_writable (
      dialog,
      ! gcal_manager_is_client_writable (priv->manager,
                                         priv->source));

  priv->setting_event = FALSE;
}

void
gcal_edit_dialog_set_manager (GcalEditDialog *dialog,
                              GcalManager    *manager)
{
  GcalEditDialogPrivate *priv;

  g_return_if_fail (GCAL_IS_MANAGER (manager));
  priv = gcal_edit_dialog_get_instance_private (dialog);

  priv->manager = manager;

  g_signal_connect (manager, "source-activated",
                    G_CALLBACK (on_source_activate), dialog);
  g_signal_connect (manager, "source-added",
                    G_CALLBACK (on_source_added), dialog);
  g_signal_connect (manager, "source-removed",
                    G_CALLBACK (on_source_removed), dialog);
}

ECalComponent*
gcal_edit_dialog_get_component (GcalEditDialog *dialog)
{
  GcalEditDialogPrivate *priv;

  priv = gcal_edit_dialog_get_instance_private (dialog);
  g_object_ref (priv->component);
  return priv->component;
}

ESource*
gcal_edit_dialog_get_source (GcalEditDialog *dialog)
{
  GcalEditDialogPrivate *priv;

  priv = gcal_edit_dialog_get_instance_private (dialog);
  return priv->source;
}

const gchar*
gcal_edit_dialog_peek_source_uid (GcalEditDialog *dialog)
{
  GcalEditDialogPrivate *priv;

  priv = gcal_edit_dialog_get_instance_private (dialog);
  return priv->source_uid;
}

const gchar*
gcal_edit_dialog_peek_event_uid (GcalEditDialog *dialog)
{
  GcalEditDialogPrivate *priv;

  priv = gcal_edit_dialog_get_instance_private (dialog);
  return priv->event_uid;
}

gchar*
gcal_edit_dialog_get_event_uuid (GcalEditDialog *dialog)
{
  GcalEditDialogPrivate *priv;
  gchar *uuid;

  priv = gcal_edit_dialog_get_instance_private (dialog);

  if (priv->source_uid == NULL ||
      priv->event_uid == NULL)
    {
      return NULL;
    }

  if (priv->event_rid != NULL)
    {
      uuid = g_strdup_printf ("%s:%s:%s",
                              priv->source_uid,
                              priv->event_uid,
                              priv->event_rid);
    }
  else
    {
      uuid = g_strdup_printf ("%s:%s",
                              priv->source_uid,
                              priv->event_uid);
   }

  return uuid;
}

icaltimetype*
gcal_edit_dialog_get_start_date (GcalEditDialog *dialog)
{
  GcalEditDialogPrivate *priv;
  icaltimetype *date;

  gint value1;
  gint value2;
  gint value3;

  priv = gcal_edit_dialog_get_instance_private (dialog);

  date = g_new0 (icaltimetype, 1);

  icaltime_set_timezone (date,
                         gcal_manager_get_system_timezone (priv->manager));

  gcal_date_entry_get_date (GCAL_DATE_ENTRY (priv->start_date_entry),
                            &value1,
                            &value2,
                            &value3);
  date->day = value1;
  date->month = value2;
  date->year = value3;

  value1 = value2 = 0;
  gcal_time_selector_get_time (GCAL_TIME_SELECTOR (priv->start_time_selector),
                            &value1,
                            &value2);
  date->hour = value1;
  date->minute = value2;

  if (date->hour == 0 &&
      date->minute == 0)
    {
      date->is_date = 1;
    }

  return date;
}

icaltimetype*
gcal_edit_dialog_get_end_date (GcalEditDialog *dialog)
{
  GcalEditDialogPrivate *priv;
  icaltimetype *date;

  gint value1;
  gint value2;
  gint value3;

  priv = gcal_edit_dialog_get_instance_private (dialog);

  date = g_new0 (icaltimetype, 1);

  icaltime_set_timezone (date,
                         gcal_manager_get_system_timezone (priv->manager));

  gcal_date_entry_get_date (GCAL_DATE_ENTRY (priv->end_date_entry),
                            &value1,
                            &value2,
                            &value3);
  date->day = value1;
  date->month = value2;
  date->year = value3;

  value1 = value2 = 0;
  gcal_time_selector_get_time (GCAL_TIME_SELECTOR (priv->end_time_selector),
                            &value1,
                            &value2);
  date->hour = value1;
  date->minute = value2;

  if (date->hour == 0 &&
      date->minute == 0)
    {
      date->is_date = 1;
    }

  return date;
}
