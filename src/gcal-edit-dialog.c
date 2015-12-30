/* -*- mode: c; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gcal-ediat-dialog.c
 * Copyright (C) 2015 Erick Pérez Castellanos <erickpc@gnome.org>
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
#include "gcal-date-selector.h"
#include "gcal-utils.h"

#include <libecal/libecal.h>
#include <glib/gi18n.h>

struct _GcalEditDialog
{
  GtkDialog         parent;

  gchar            *source_uid;
  gchar            *event_uid;
  gchar            *event_rid;
  gboolean          writable;

  GcalManager      *manager;

  GtkWidget        *titlebar;
  GtkWidget        *lock;
  GtkWidget        *source_image;
  GtkWidget        *source_label;

  GtkWidget        *delete_button;
  GtkWidget        *done_button;
  GtkWidget        *cancel_button;
  GtkWidget        *sources_button;
  GtkWidget        *sources_popover;

  GtkWidget        *summary_entry;

  GtkWidget        *start_date_selector;
  GtkWidget        *end_date_selector;
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
  gboolean          format_24h;
  gboolean          event_is_new;
  gboolean          setting_event;
};

static void        fill_sources_menu                      (GcalEditDialog    *dialog);

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

G_DEFINE_TYPE (GcalEditDialog, gcal_edit_dialog, GTK_TYPE_DIALOG)

enum
{
  PROP_0,
  PROP_MANAGER,
  PROP_WRITABLE,
  LAST_PROP
};

static void
fill_sources_menu (GcalEditDialog *dialog)
{
  GList *list;
  GList *aux;

  if (dialog->manager == NULL)
    return;

  list = gcal_manager_get_sources (dialog->manager);
  dialog->sources_menu = g_menu_new ();

  for (aux = list; aux != NULL; aux = aux->next)
    {
      ESource *source;
      GMenuItem *item;
      GdkRGBA color;
      GdkPixbuf *pix;

      source = E_SOURCE (aux->data);

      /* retrieve color */
      get_color_name_from_source (source, &color);
      pix = gcal_get_pixbuf_from_color (&color, 16);;

      /* menu item */
      item = g_menu_item_new (e_source_get_display_name (source), "select_calendar");
      g_menu_item_set_icon (item, G_ICON (pix));

      /* set insensitive for read-only calendars */
      if (!gcal_manager_is_client_writable (dialog->manager, source))
        {
          g_menu_item_set_action_and_target_value (item, "select_calendar", NULL);
        }
      else
        {
          g_menu_item_set_action_and_target_value (item, "select_calendar",
                                                   g_variant_new_string (e_source_get_uid (source)));
        }

      g_menu_append_item (dialog->sources_menu, item);

      g_object_unref (pix);
      g_object_unref (item);
    }

  gtk_popover_bind_model (GTK_POPOVER (dialog->sources_popover), G_MENU_MODEL (dialog->sources_menu), "edit");

  /* HACK: show the popover menu icons */
  fix_popover_menu_icons (GTK_POPOVER (dialog->sources_popover));

  g_list_free (list);
}

static void
on_calendar_selected (GtkWidget *menu_item,
                      GVariant  *value,
                      gpointer   user_data)
{
  GcalEditDialog *dialog;
  GList *list;
  GList *aux;
  gchar *uid;

  dialog = GCAL_EDIT_DIALOG (user_data);
  list = gcal_manager_get_sources (dialog->manager);

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
        GdkPixbuf *pix;

        /* retrieve color */
        get_color_name_from_source (source, &color);

        pix = gcal_get_pixbuf_from_color (&color, 16);
        gtk_image_set_from_pixbuf (GTK_IMAGE (dialog->source_image), pix);
        g_object_unref (pix);

        g_set_object (&dialog->source, source);
        gtk_header_bar_set_subtitle (GTK_HEADER_BAR (dialog->titlebar),
                                     e_source_get_display_name (dialog->source));
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
  ECalComponentDateTime dtstart;
  ECalComponentDateTime dtend;
  GcalEditDialog *dialog;
  icaltimetype *start_date;
  icaltimetype *end_date;

  dialog = GCAL_EDIT_DIALOG (user_data);

  if (dialog->setting_event)
    return;

  start_date = gcal_edit_dialog_get_start_date (GCAL_EDIT_DIALOG (user_data));
  end_date = gcal_edit_dialog_get_end_date (GCAL_EDIT_DIALOG (user_data));

  /* check if the start & end dates are sane */
  if (icaltime_compare (*start_date, *end_date) != -1)
    {
      /* change the non-editing entry */
      if (GTK_WIDGET (entry) == dialog->start_date_selector)
        {
          end_date->day = start_date->day;
          end_date->month = start_date->month;
          end_date->year = start_date->year;

          if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->all_day_check)))
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

          if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->all_day_check)))
            {
              start_date->day -= 1;
              *start_date = icaltime_normalize (*start_date);
          }
        }

      /* update the entries with the new sane values */
      g_signal_handlers_block_by_func (dialog->start_date_selector,
                                       update_date,
                                       user_data);
      g_signal_handlers_block_by_func (dialog->end_date_selector,
                                       update_date,
                                       user_data);

      gcal_date_selector_set_date (GCAL_DATE_SELECTOR (dialog->start_date_selector),
                                   start_date->day,
                                   start_date->month,
                                   start_date->year);
      gcal_date_selector_set_date (GCAL_DATE_SELECTOR (dialog->end_date_selector),
                                   end_date->day,
                                   end_date->month,
                                   end_date->year);

      g_signal_handlers_unblock_by_func (dialog->start_date_selector,
                                         update_date,
                                         user_data);
      g_signal_handlers_unblock_by_func (dialog->end_date_selector,
                                         update_date,
                                         user_data);
    }

  /* update component */
  e_cal_component_get_dtstart (dialog->component, &dtstart);
  e_cal_component_get_dtend (dialog->component, &dtend);

  *(dtstart.value) = *start_date;
  if (dtstart.tzid != NULL)
    {
      icaltimezone* zone = icaltimezone_get_builtin_timezone_from_tzid (dtstart.tzid);
      *(dtstart.value) = icaltime_convert_to_zone (*start_date, zone);
    }

  *(dtend.value) = *end_date;
  if (dtend.tzid != NULL)
    {
      icaltimezone* zone = icaltimezone_get_builtin_timezone_from_tzid (dtend.tzid);
      *(dtend.value) = icaltime_convert_to_zone (*end_date, zone);
    }

  e_cal_component_set_dtstart (dialog->component, &dtstart);
  e_cal_component_set_dtend (dialog->component, &dtend);
  e_cal_component_commit_sequence (dialog->component);

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
  GcalEditDialog *dialog;

  dialog = GCAL_EDIT_DIALOG (user_data);

  e_cal_component_set_location (dialog->component, gtk_entry_get_text (entry));
  e_cal_component_commit_sequence (dialog->component);
}

static void
update_notes (GtkTextBuffer *buffer,
              gpointer       user_data)
{
  GcalEditDialog *dialog;
  GSList note;
  ECalComponentText text;
  gchar *note_text;

  dialog = GCAL_EDIT_DIALOG (user_data);

  g_object_get (G_OBJECT (buffer),
                "text", &note_text,
                NULL);

  text.value = note_text;
  text.altrep = NULL;
  note.data = &text;
  note.next = NULL;

  e_cal_component_set_description_list (dialog->component, &note);
  e_cal_component_commit_sequence (dialog->component);

  g_free (note_text);
}

static void
update_summary (GtkEntry   *entry,
                GParamSpec *pspec,
                gpointer    user_data)
{

  ECalComponentText summary;
  GcalEditDialog *dialog;

  dialog = GCAL_EDIT_DIALOG (user_data);

  summary.value = gtk_entry_get_text (entry);
  summary.altrep = NULL;

  e_cal_component_set_summary (dialog->component, &summary);
  e_cal_component_commit_sequence (dialog->component);
}

static void
update_time (GtkEntry   *entry,
             gpointer    user_data)
{
  ECalComponentDateTime dtstart;
  ECalComponentDateTime dtend;
  GcalEditDialog *dialog;
  icaltimetype *start_date;
  icaltimetype *end_date;

  dialog = GCAL_EDIT_DIALOG (user_data);

  if (dialog->setting_event)
    return;

  start_date = gcal_edit_dialog_get_start_date (GCAL_EDIT_DIALOG (user_data));
  end_date = gcal_edit_dialog_get_end_date (GCAL_EDIT_DIALOG (user_data));

  /* check if the start & end dates are sane */
  if (icaltime_compare (*start_date, *end_date) > -1)
    {
      /* change the non-editing entry */
      if (GTK_WIDGET (entry) == dialog->start_time_selector)
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
      g_signal_handlers_block_by_func (dialog->start_time_selector,
                                       update_time,
                                       user_data);
      g_signal_handlers_block_by_func (dialog->end_time_selector,
                                       update_time,
                                       user_data);

      /* updates date as well, since hours can change the current date */
      gcal_time_selector_set_time (GCAL_TIME_SELECTOR (dialog->start_time_selector),
                                   start_date->hour,
                                   start_date->minute);
      gcal_date_selector_set_date (GCAL_DATE_SELECTOR (dialog->start_date_selector),
                                   start_date->day,
                                   start_date->month,
                                   start_date->year);
      gcal_time_selector_set_time (GCAL_TIME_SELECTOR (dialog->end_time_selector),
                                   end_date->hour,
                                   end_date->minute);
      gcal_date_selector_set_date (GCAL_DATE_SELECTOR (dialog->end_date_selector),
                                   end_date->day,
                                   end_date->month,
                                   end_date->year);

      g_signal_handlers_unblock_by_func (dialog->start_time_selector,
                                         update_time,
                                         user_data);
      g_signal_handlers_unblock_by_func (dialog->end_time_selector,
                                         update_time,
                                         user_data);
    }

  g_debug ("Updating date: %s -> %s",
           icaltime_as_ical_string (*start_date),
           icaltime_as_ical_string (*end_date));

  /* update component */
  e_cal_component_get_dtstart (dialog->component, &dtstart);
  e_cal_component_get_dtend (dialog->component, &dtend);

  *(dtstart.value) = *start_date;
  if (dtstart.tzid != NULL)
    {
      icaltimezone* zone = icaltimezone_get_builtin_timezone_from_tzid (dtstart.tzid);
      *(dtstart.value) = icaltime_convert_to_zone (*start_date, zone);
    }

  *(dtend.value) = *end_date;
  if (dtend.tzid != NULL)
    {
      icaltimezone* zone = icaltimezone_get_builtin_timezone_from_tzid (dtend.tzid);
      *(dtend.value) = icaltime_convert_to_zone (*end_date, zone);
    }

  e_cal_component_set_dtstart (dialog->component, &dtstart);
  e_cal_component_set_dtend (dialog->component, &dtend);
  e_cal_component_commit_sequence (dialog->component);

  g_free (start_date);
  g_free (end_date);
  e_cal_component_free_datetime (&dtstart);
  e_cal_component_free_datetime (&dtend);
}

static void
gcal_edit_dialog_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GcalEditDialog *self = GCAL_EDIT_DIALOG (object);

  switch (prop_id)
    {
    case PROP_MANAGER:
      g_value_set_object (value, self->manager);
      break;

    case PROP_WRITABLE:
      g_value_set_boolean (value, self->writable);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_edit_dialog_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GcalEditDialog *self = GCAL_EDIT_DIALOG (object);

  switch (prop_id)
    {
    case PROP_MANAGER:
      gcal_edit_dialog_set_manager (self, g_value_get_object (value));
      break;

    case PROP_WRITABLE:
      gcal_edit_dialog_set_writable (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_edit_dialog_class_init (GcalEditDialogClass *klass)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->get_property = gcal_edit_dialog_get_property;
  object_class->set_property = gcal_edit_dialog_set_property;
  object_class->constructed = gcal_edit_dialog_constructed;
  object_class->finalize = gcal_edit_dialog_finalize;

  widget_class = GTK_WIDGET_CLASS (klass);
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/edit-dialog.ui");

  /**
   * GcalEditDialog::manager:
   *
   * The #GcalManager of the dialog.
   */
  g_object_class_install_property (object_class,
                                   PROP_MANAGER,
                                   g_param_spec_object ("manager",
                                                        "Manager of the dialog",
                                                        "The manager of the dialog",
                                                        GCAL_TYPE_MANAGER,
                                                        G_PARAM_READWRITE));

  /**
   * GcalEditDialog::writable:
   *
   * Whether the current event can be edited or not.
   */
  g_object_class_install_property (object_class,
                                   PROP_WRITABLE,
                                   g_param_spec_boolean ("writable",
                                                         "Whether the current event can be edited",
                                                         "Whether the current event can be edited or not",
                                                         TRUE,
                                                         G_PARAM_READWRITE));

  /* Buttons */
  gtk_widget_class_bind_template_child (widget_class, GcalEditDialog, done_button);
  gtk_widget_class_bind_template_child (widget_class, GcalEditDialog, cancel_button);
  gtk_widget_class_bind_template_child (widget_class, GcalEditDialog, delete_button);
  gtk_widget_class_bind_template_child (widget_class, GcalEditDialog, sources_button);
  /* Entries */
  gtk_widget_class_bind_template_child (widget_class, GcalEditDialog, summary_entry);
  gtk_widget_class_bind_template_child (widget_class, GcalEditDialog, start_time_selector);
  gtk_widget_class_bind_template_child (widget_class, GcalEditDialog, start_date_selector);
  gtk_widget_class_bind_template_child (widget_class, GcalEditDialog, end_time_selector);
  gtk_widget_class_bind_template_child (widget_class, GcalEditDialog, end_date_selector);
  gtk_widget_class_bind_template_child (widget_class, GcalEditDialog, location_entry);
  /* Other */
  gtk_widget_class_bind_template_child (widget_class, GcalEditDialog, notes_text);
  gtk_widget_class_bind_template_child (widget_class, GcalEditDialog, all_day_check);
  gtk_widget_class_bind_template_child (widget_class, GcalEditDialog, titlebar);
  gtk_widget_class_bind_template_child (widget_class, GcalEditDialog, lock);
  gtk_widget_class_bind_template_child (widget_class, GcalEditDialog, source_image);
  gtk_widget_class_bind_template_child (widget_class, GcalEditDialog, sources_popover);

  /* callbacks */
  gtk_widget_class_bind_template_callback (widget_class, gcal_edit_dialog_action_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, gcal_edit_dialog_all_day_changed);
  gtk_widget_class_bind_template_callback (widget_class, update_summary);
  gtk_widget_class_bind_template_callback (widget_class, update_location);
  gtk_widget_class_bind_template_callback (widget_class, update_date);
  gtk_widget_class_bind_template_callback (widget_class, update_time);
}

static void
gcal_edit_dialog_init (GcalEditDialog *self)
{
  self->writable = TRUE;

  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
gcal_edit_dialog_constructed (GObject* object)
{
  GcalEditDialog *dialog;

  dialog = GCAL_EDIT_DIALOG (object);

  /* chaining up */
  G_OBJECT_CLASS (gcal_edit_dialog_parent_class)->constructed (object);

  gtk_window_set_title (GTK_WINDOW (object), "");

  /* titlebar */
  gtk_window_set_titlebar (GTK_WINDOW (object), dialog->titlebar);

  /* actions */
  dialog->action_group = g_simple_action_group_new ();
  gtk_widget_insert_action_group (GTK_WIDGET (object),
                                  "edit",
                                  G_ACTION_GROUP (dialog->action_group));

  dialog->action = g_simple_action_new ("select_calendar", G_VARIANT_TYPE_STRING);
  g_signal_connect (dialog->action,
                    "activate",
                    G_CALLBACK (on_calendar_selected), object);

  g_action_map_add_action (G_ACTION_MAP (dialog->action_group), G_ACTION (dialog->action));

  /* action area, buttons */
  g_object_set_data (G_OBJECT (dialog->cancel_button),
                     "response",
                     GINT_TO_POINTER (GTK_RESPONSE_CANCEL));
  g_object_set_data (G_OBJECT (dialog->delete_button),
                     "response",
                     GINT_TO_POINTER (GCAL_RESPONSE_DELETE_EVENT));
  g_object_set_data (G_OBJECT (dialog->done_button),
                     "response",
                     GINT_TO_POINTER (GCAL_RESPONSE_SAVE_EVENT));

  g_signal_connect (gtk_text_view_get_buffer (GTK_TEXT_VIEW (dialog->notes_text)), "changed", G_CALLBACK (update_notes),
                    object);

}

static void
gcal_edit_dialog_finalize (GObject *object)
{
  GcalEditDialog *dialog;

  dialog = GCAL_EDIT_DIALOG (object);

  g_clear_pointer (&dialog->source_uid, g_free);
  g_clear_pointer (&dialog->event_uid, g_free);
  g_clear_object (&dialog->action_group);
  g_clear_object (&dialog->action);
  g_clear_object (&dialog->component);
  g_clear_object (&dialog->manager);
  g_clear_object (&dialog->source);

  G_OBJECT_CLASS (gcal_edit_dialog_parent_class)->finalize (object);
}

static void
gcal_edit_dialog_set_writable (GcalEditDialog *dialog,
                               gboolean        writable)
{
  if (dialog->writable != writable)
    {
      dialog->writable = writable;

      gtk_button_set_label (GTK_BUTTON (dialog->done_button), writable ? _("Save") : _("Done"));

      if (!writable || (writable && gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->all_day_check))))
        {
          gtk_widget_set_sensitive (dialog->start_time_selector, FALSE);
          gtk_widget_set_sensitive (dialog->end_time_selector, FALSE);
        }
      else
        {
          gtk_widget_set_sensitive (dialog->start_time_selector, TRUE);
          gtk_widget_set_sensitive (dialog->end_time_selector, TRUE);
        }

      g_object_notify (G_OBJECT (dialog), "writable");
    }
}

static void
gcal_edit_dialog_clear_data (GcalEditDialog *dialog)
{
  /* summary */
  g_signal_handlers_block_by_func (dialog->summary_entry,
                                   update_summary,
                                   dialog);
  gtk_entry_set_text (GTK_ENTRY (dialog->summary_entry), "");
  g_signal_handlers_unblock_by_func (dialog->summary_entry,
                                     update_summary,
                                     dialog);

  /* date and time */
  gcal_date_selector_set_date (GCAL_DATE_SELECTOR (dialog->start_date_selector), 0, 0, 0);
  gcal_time_selector_set_time (GCAL_TIME_SELECTOR (dialog->start_time_selector), 0, 0);
  gcal_date_selector_set_date (GCAL_DATE_SELECTOR (dialog->end_date_selector), 0, 0, 0);
  gcal_time_selector_set_time (GCAL_TIME_SELECTOR (dialog->end_time_selector), 0, 0);

  /* location */
  g_signal_handlers_block_by_func (dialog->location_entry,
                                   update_location,
                                   dialog);
  gtk_entry_set_text (GTK_ENTRY (dialog->location_entry), "");
  g_signal_handlers_unblock_by_func (dialog->location_entry,
                                     update_location,
                                     dialog);

  /* notes */
  g_signal_handlers_block_by_func (gtk_text_view_get_buffer (GTK_TEXT_VIEW (dialog->notes_text)),
                                   update_notes,
                                   dialog);
  gtk_text_buffer_set_text (
      gtk_text_view_get_buffer (GTK_TEXT_VIEW (dialog->notes_text)),
      "",
      -1);
  g_signal_handlers_unblock_by_func (gtk_text_view_get_buffer (GTK_TEXT_VIEW (dialog->notes_text)),
                                     update_notes,
                                     dialog);
}

static void
gcal_edit_dialog_action_button_clicked (GtkWidget *widget,
                                        gpointer   user_data)
{
  GcalEditDialog *dialog;
  gint response;

  dialog = GCAL_EDIT_DIALOG (user_data);

  response = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (widget),
                                                  "response"));

  if (response == GCAL_RESPONSE_SAVE_EVENT &&
      dialog->event_is_new)
    {
      response = GCAL_RESPONSE_CREATE_EVENT;
    }

  gtk_dialog_response (GTK_DIALOG (user_data), response);
}

static void
gcal_edit_dialog_all_day_changed (GtkWidget *widget,
                                  gpointer   user_data)
{
  GcalEditDialog *dialog;

  dialog = GCAL_EDIT_DIALOG (user_data);

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->all_day_check)))
    {
      gcal_time_selector_set_time (GCAL_TIME_SELECTOR (dialog->start_time_selector), 0, 0);
      gcal_time_selector_set_time (GCAL_TIME_SELECTOR (dialog->end_time_selector), 0, 0);
    }
}

/* Public API */
GtkWidget*
gcal_edit_dialog_new (gboolean format_24h)
{
  GcalEditDialog *dialog;

  dialog = g_object_new (GCAL_TYPE_EDIT_DIALOG, NULL);

  dialog->format_24h = format_24h;

  gcal_time_selector_set_time_format (GCAL_TIME_SELECTOR (dialog->start_time_selector), format_24h);
  gcal_time_selector_set_time_format (GCAL_TIME_SELECTOR (dialog->end_time_selector), format_24h);

  return GTK_WIDGET (dialog);
}

void
gcal_edit_dialog_set_event_is_new (GcalEditDialog *dialog,
                                   gboolean        event_is_new)
{
  dialog->event_is_new = event_is_new;

  gtk_widget_set_visible (dialog->delete_button, !event_is_new);

  /* FIXME: implement moving events to other sources */
  gtk_widget_set_sensitive (dialog->sources_button, event_is_new);
  gtk_button_set_relief (GTK_BUTTON (dialog->sources_button), event_is_new ? GTK_RELIEF_NORMAL : GTK_RELIEF_NONE);
}

void
gcal_edit_dialog_set_event_data (GcalEditDialog *dialog,
                                 GcalEventData  *data)
{

  GdkRGBA color;
  GdkPixbuf *pix;

  const gchar *const_text = NULL;
  gboolean all_day;
  gchar *description;

  ECalComponentId *id;
  ECalComponentText e_summary;
  ECalComponentDateTime dtstart;
  ECalComponentDateTime dtend;

  all_day = FALSE;

  dialog->setting_event = TRUE;

  g_set_object (&dialog->source, data->source);

  g_clear_object (&dialog->component);
  dialog->component = e_cal_component_clone (data->event_component);

  g_clear_pointer (&dialog->source_uid, g_free);
  dialog->source_uid = e_source_dup_uid (dialog->source);

  /* Setup UID */
  id = e_cal_component_get_id (dialog->component);

  g_clear_pointer (&(dialog->event_uid), g_free);
  dialog->event_uid = g_strdup (id->uid);

  g_clear_pointer (&(dialog->event_rid), g_free);
  dialog->event_rid = g_strdup (id->rid);

  e_cal_component_free_id (id);

  /* Clear event data */
  gcal_edit_dialog_clear_data (dialog);

  /* update sources list */
  if (dialog->sources_menu != NULL)
    g_menu_remove_all (dialog->sources_menu);

  fill_sources_menu (dialog);

  /* Load new event data */
  /* summary */
  e_cal_component_get_summary (dialog->component, &e_summary);
  if (e_summary.value == NULL || g_strcmp0 (e_summary.value, "") == 0)
    gtk_entry_set_text (GTK_ENTRY (dialog->summary_entry), _("Unnamed event"));
  else
    gtk_entry_set_text (GTK_ENTRY (dialog->summary_entry), e_summary.value);

  /* dialog titlebar's title & subtitle */
  get_color_name_from_source (data->source, &color);

  pix = gcal_get_pixbuf_from_color (&color, 16);
  gtk_image_set_from_pixbuf (GTK_IMAGE (dialog->source_image), pix);
  g_object_unref (pix);

  gtk_header_bar_set_subtitle (GTK_HEADER_BAR (dialog->titlebar),
                               e_source_get_display_name (data->source));

  /* retrieve start and end dates */
  e_cal_component_get_dtstart (dialog->component, &dtstart);
  e_cal_component_get_dtend (dialog->component, &dtend);

  /* check if it's an all-day event */
  all_day = (dtstart.value->is_date == 1 && (dtend.value != NULL ? dtend.value->is_date == 1 : FALSE));

  /* start date */
  gcal_date_selector_set_date (GCAL_DATE_SELECTOR (dialog->start_date_selector),
                               dtstart.value->day,
                               dtstart.value->month,
                               dtstart.value->year);

  /* start time */
  if (!all_day)
    {
      icaltimetype *date = gcal_dup_icaltime (dtstart.value);
      if (dtstart.tzid != NULL)
        {
          dtstart.value->zone =
            icaltimezone_get_builtin_timezone_from_tzid (dtstart.tzid);
        }
      *date = icaltime_convert_to_zone (*(dtstart.value),
                                        e_cal_util_get_system_timezone ());
      gcal_time_selector_set_time (GCAL_TIME_SELECTOR (dialog->start_time_selector),
                                   date->hour, date->minute);
      g_free (date);
    }

  /* end date */
  if (dtend.value != NULL)
    {
      gcal_date_selector_set_date (GCAL_DATE_SELECTOR (dialog->end_date_selector),
                                   dtend.value->day, dtend.value->month, dtend.value->year);

      if (!all_day)
        {
          icaltimetype *date = gcal_dup_icaltime (dtend.value);
          if (dtend.tzid != NULL)
            {
              dtend.value->zone =
                icaltimezone_get_builtin_timezone_from_tzid (dtend.tzid);
            }
          *date = icaltime_convert_to_zone (*(dtend.value), e_cal_util_get_system_timezone ());
          gcal_time_selector_set_time (GCAL_TIME_SELECTOR (dialog->end_time_selector),
                                       date->hour, date->minute);
          g_free (date);
        }
    }
  else
    {
      gcal_date_selector_set_date (GCAL_DATE_SELECTOR (dialog->end_date_selector),
                                   dtstart.value->day, dtstart.value->month, dtstart.value->year);
      gcal_time_selector_set_time (GCAL_TIME_SELECTOR (dialog->end_time_selector),
                                   dtstart.value->hour, dtstart.value->minute);
    }

  e_cal_component_free_datetime (&dtstart);
  e_cal_component_free_datetime (&dtend);

  /* all_day  */
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->all_day_check), all_day);


  /* location */
  e_cal_component_get_location (dialog->component, &const_text);
  gtk_entry_set_text (GTK_ENTRY (dialog->location_entry),
                      const_text != NULL ? const_text : "");

  /* notes */
  description = get_desc_from_component (dialog->component, "\n");

  if (description != NULL)
    {
      gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (dialog->notes_text)),
                                description,
                                -1);

      g_free (description);
    }

  gcal_edit_dialog_set_writable (dialog, gcal_manager_is_client_writable (dialog->manager, dialog->source));

  dialog->setting_event = FALSE;
}

void
gcal_edit_dialog_set_manager (GcalEditDialog *dialog,
                              GcalManager    *manager)
{
  g_return_if_fail (GCAL_IS_EDIT_DIALOG (dialog));
  g_return_if_fail (GCAL_IS_MANAGER (manager));

  if (g_set_object (&dialog->manager, manager))
    g_object_notify (G_OBJECT (dialog), "manager");
}

ECalComponent*
gcal_edit_dialog_get_component (GcalEditDialog *dialog)
{
  g_return_val_if_fail (GCAL_IS_EDIT_DIALOG (dialog), NULL);

  g_object_ref (dialog->component);
  return dialog->component;
}

ESource*
gcal_edit_dialog_get_source (GcalEditDialog *dialog)
{
  g_return_val_if_fail (GCAL_IS_EDIT_DIALOG (dialog), NULL);

  return dialog->source;
}

const gchar*
gcal_edit_dialog_peek_source_uid (GcalEditDialog *dialog)
{
  g_return_val_if_fail (GCAL_IS_EDIT_DIALOG (dialog), NULL);

  return dialog->source_uid;
}

const gchar*
gcal_edit_dialog_peek_event_uid (GcalEditDialog *dialog)
{
  g_return_val_if_fail (GCAL_IS_EDIT_DIALOG (dialog), NULL);

  return dialog->event_uid;
}

gchar*
gcal_edit_dialog_get_event_uuid (GcalEditDialog *dialog)
{
  gchar *uuid;

  if (dialog->source_uid == NULL ||
      dialog->event_uid == NULL)
    {
      return NULL;
    }

  if (dialog->event_rid != NULL)
    {
      uuid = g_strdup_printf ("%s:%s:%s",
                              dialog->source_uid,
                              dialog->event_uid,
                              dialog->event_rid);
    }
  else
    {
      uuid = g_strdup_printf ("%s:%s",
                              dialog->source_uid,
                              dialog->event_uid);
   }

  return uuid;
}

icaltimetype*
gcal_edit_dialog_get_start_date (GcalEditDialog *dialog)
{
  icaltimetype *date;

  gint value1;
  gint value2;
  gint value3;

  date = g_new0 (icaltimetype, 1);

  icaltime_set_timezone (date, gcal_manager_get_system_timezone (dialog->manager));

  gcal_date_selector_get_date (GCAL_DATE_SELECTOR (dialog->start_date_selector),
                               &value1,
                               &value2,
                               &value3);
  date->day = value1;
  date->month = value2;
  date->year = value3;

  value1 = value2 = 0;
  gcal_time_selector_get_time (GCAL_TIME_SELECTOR (dialog->start_time_selector),
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
  icaltimetype *date;

  gint value1;
  gint value2;
  gint value3;

  date = g_new0 (icaltimetype, 1);

  icaltime_set_timezone (date,
                         gcal_manager_get_system_timezone (dialog->manager));

  gcal_date_selector_get_date (GCAL_DATE_SELECTOR (dialog->end_date_selector),
                               &value1,
                               &value2,
                               &value3);
  date->day = value1;
  date->month = value2;
  date->year = value3;

  value1 = value2 = 0;
  gcal_time_selector_get_time (GCAL_TIME_SELECTOR (dialog->end_time_selector),
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
