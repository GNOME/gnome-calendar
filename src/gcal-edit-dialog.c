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
#include "gcal-time-entry.h"
#include "gcal-date-entry.h"
#include "gcal-utils.h"

#include <libecal/libecal.h>
#include <glib/gi18n.h>

typedef struct _SimpleEventStore SimpleEventStore;

struct _SimpleEventStore
{
  gboolean       all_day;
  icaltimetype  *start_date;
  icaltimetype  *end_date;
  icaltime_span *time_span;

  gchar         *location;
  gchar         *description;
};

typedef struct
{
  gchar            *source_uid;
  gchar            *event_uid;
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

  GtkWidget        *summary_entry;

  GtkWidget        *start_date_entry;
  GtkWidget        *end_date_entry;
  GtkWidget        *all_day_check;
  GtkWidget        *start_time_entry;
  GtkWidget        *end_time_entry;
  GtkWidget        *location_entry;
  GtkWidget        *notes_text;

  SimpleEventStore *ev_store;

  /* new data holders */
  ESource          *source; /* weak reference */
  ECalComponent    *component;

  gboolean          setting_event;
} GcalEditDialogPrivate;

static void        update_summary                         (GtkEntry          *entry,
                                                           GParamSpec        *pspec,
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

static gboolean    gcal_edit_dialog_date_changed          (GcalEditDialog    *dialog,
                                                           gboolean           start_date);

static void        gcal_edit_dialog_date_entry_modified   (GtkWidget         *entry,
                                                           gpointer           user_data);

static gboolean    gcal_edit_dialog_source_changed        (GcalEditDialog    *dialog);

G_DEFINE_TYPE_WITH_PRIVATE (GcalEditDialog, gcal_edit_dialog, GTK_TYPE_DIALOG)

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
  /* Entries */
  gtk_widget_class_bind_template_child_private (widget_class, GcalEditDialog, summary_entry);
  gtk_widget_class_bind_template_child_private (widget_class, GcalEditDialog, start_time_entry);
  gtk_widget_class_bind_template_child_private (widget_class, GcalEditDialog, start_date_entry);
  gtk_widget_class_bind_template_child_private (widget_class, GcalEditDialog, end_time_entry);
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

  /* bind title & symmary */
  g_object_bind_property (priv->summary_entry,
                          "text",
                          priv->titlebar,
                          "title",
                          G_BINDING_DEFAULT);

  /* bind all-day check button & time entries */
  g_object_bind_property (priv->all_day_check,
                          "active",
                          priv->start_time_entry,
                          "sensitive",
                          G_BINDING_DEFAULT | G_BINDING_INVERT_BOOLEAN);
  g_object_bind_property (priv->all_day_check,
                          "active",
                          priv->end_time_entry,
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
                     GINT_TO_POINTER (GTK_RESPONSE_ACCEPT));
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

  g_signal_connect (priv->start_date_entry,
                    "modified",
                    G_CALLBACK (gcal_edit_dialog_date_entry_modified),
                    object);

  g_signal_connect (priv->end_date_entry,
                    "modified",
                    G_CALLBACK (gcal_edit_dialog_date_entry_modified),
                    object);

  g_signal_connect (priv->start_time_entry,
                    "modified",
                    G_CALLBACK (gcal_edit_dialog_date_entry_modified),
                    object);

  g_signal_connect (priv->end_time_entry,
                    "modified",
                    G_CALLBACK (gcal_edit_dialog_date_entry_modified),
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

  if (priv->ev_store != NULL)
    {
      g_free (priv->ev_store->start_date);
      g_free (priv->ev_store->end_date);
      g_free (priv->ev_store->time_span);

      g_free (priv->ev_store);
    }

  if (priv->component != NULL)
    g_object_unref (priv->component);

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
  gtk_editable_set_editable (GTK_EDITABLE (priv->start_time_entry), writable);
  gtk_editable_set_editable (GTK_EDITABLE (priv->end_time_entry), writable);
  gtk_editable_set_editable (GTK_EDITABLE (priv->end_time_entry), writable);
  gtk_editable_set_editable (GTK_EDITABLE (priv->location_entry), writable);

  gtk_text_view_set_editable (GTK_TEXT_VIEW (priv->notes_text), writable);

  gtk_widget_set_sensitive (priv->all_day_check, writable);

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
  gcal_time_entry_set_time (GCAL_TIME_ENTRY (priv->start_time_entry), 0, 0);
  gtk_entry_set_text (GTK_ENTRY (priv->end_date_entry), "");
  gcal_time_entry_set_time (GCAL_TIME_ENTRY (priv->end_time_entry), 0, 0);

  /* location */
  gtk_entry_set_text (GTK_ENTRY (priv->location_entry), "");

  /* notes */
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

static void
gcal_edit_dialog_all_day_changed (GtkWidget *widget,
                                  gpointer   user_data)
{
  GcalEditDialogPrivate *priv;

  priv = gcal_edit_dialog_get_instance_private (GCAL_EDIT_DIALOG (user_data));

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->all_day_check)))
    {
      gcal_time_entry_set_time (GCAL_TIME_ENTRY (priv->start_time_entry), 0, 0);
      gcal_time_entry_set_time (GCAL_TIME_ENTRY (priv->end_time_entry), 0, 0);
    }
}

static gboolean
gcal_edit_dialog_date_changed (GcalEditDialog *dialog,
                               gboolean        start_date)
{
  GcalEditDialogPrivate *priv;
  gint day;
  gint month;
  gint year;
  gint hour;
  gint minute;

  GtkWidget *widget;
  icaltimetype *old_date;
  priv = gcal_edit_dialog_get_instance_private (dialog);

  if (start_date)
    {
      widget = priv->start_date_entry;
      old_date = priv->ev_store->start_date;
    }
  else
    {
      widget = priv->end_date_entry;
      old_date = priv->ev_store->end_date;
    }

  gcal_date_entry_get_date (GCAL_DATE_ENTRY (widget),
                            &day,
                            &month,
                            &year);
  if (day != old_date->day ||
      month != old_date->month ||
      year != old_date->year)
    {
      return TRUE;
    }

  widget = start_date ? priv->start_time_entry : priv->end_time_entry;
  gcal_time_entry_get_time (GCAL_TIME_ENTRY (widget),
                            &hour,
                            &minute);
  if (hour != old_date->hour ||
      minute != old_date->minute)
    {
      return TRUE;
    }

  return FALSE;
}

static void
gcal_edit_dialog_date_entry_modified (GtkWidget *entry,
                                      gpointer   user_data)
{
  GcalEditDialogPrivate *priv;
  icaltimetype start_date;
  icaltimetype end_date;

  priv = gcal_edit_dialog_get_instance_private (GCAL_EDIT_DIALOG (user_data));

  if (priv->setting_event)
    return;

  gcal_date_entry_get_date (GCAL_DATE_ENTRY (priv->start_date_entry),
                            &(start_date.day),
                            &(start_date.month),
                            &(start_date.year));
  gcal_date_entry_get_date (GCAL_DATE_ENTRY (priv->end_date_entry),
                            &(end_date.day),
                            &(end_date.month),
                            &(end_date.year));

  start_date.is_date = 0;
  end_date.is_date = 0;

  gcal_time_entry_get_time (GCAL_TIME_ENTRY (priv->start_time_entry),
                            &(start_date.hour),
                            &(start_date.minute));
  start_date.second = 0;
  gcal_time_entry_get_time (GCAL_TIME_ENTRY (priv->end_time_entry),
                            &(end_date.hour),
                            &(end_date.minute));
  end_date.second = 0;

  if (start_date.hour == 0 &&
      start_date.minute == 0 &&
      end_date.hour == 0 &&
      end_date.minute == 0)
    {
      start_date.is_date = 1;
      end_date.is_date = 1;

      /* block signaling */
      g_signal_handlers_block_by_func (priv->all_day_check,
                                       gcal_edit_dialog_all_day_changed,
                                       user_data);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->all_day_check), TRUE);
      g_signal_handlers_unblock_by_func (priv->all_day_check,
                                         gcal_edit_dialog_all_day_changed,
                                         user_data);
      /* unblock signaling */
    }
  else
    {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->all_day_check), FALSE);
    }

  icaltime_set_timezone (&start_date,
                         gcal_manager_get_system_timezone (priv->manager));
  icaltime_set_timezone (&end_date,
                         gcal_manager_get_system_timezone (priv->manager));


  if (icaltime_compare (start_date, end_date) != -1)
    {
      g_signal_handlers_block_by_func (priv->start_date_entry,
                                       gcal_edit_dialog_date_entry_modified,
                                       user_data);
      g_signal_handlers_block_by_func (priv->start_time_entry,
                                       gcal_edit_dialog_date_entry_modified,
                                       user_data);
      g_signal_handlers_block_by_func (priv->end_date_entry,
                                       gcal_edit_dialog_date_entry_modified,
                                       user_data);
      g_signal_handlers_block_by_func (priv->end_time_entry,
                                       gcal_edit_dialog_date_entry_modified,
                                       user_data);

      if (entry == priv->start_date_entry ||
          entry == priv->start_time_entry)
        {
          time_t to_end;
          time_t from_start = icaltime_as_timet (start_date);

          to_end = from_start + (priv->ev_store->time_span->end - priv->ev_store->time_span->start);
          end_date = icaltime_from_timet (to_end, start_date.is_date);

          gcal_date_entry_set_date (GCAL_DATE_ENTRY (priv->end_date_entry),
                                    end_date.day,
                                    end_date.month,
                                    end_date.year);
          if (start_date.is_date != 1)
            {
              gcal_time_entry_set_time (GCAL_TIME_ENTRY (priv->end_time_entry),
                                        end_date.hour,
                                        end_date.minute);
            }
        }
      else
        {
          time_t from_start;
          time_t to_end = icaltime_as_timet (end_date);

          from_start = to_end - (priv->ev_store->time_span->end - priv->ev_store->time_span->start);
          start_date = icaltime_from_timet (from_start, end_date.is_date);

          gcal_date_entry_set_date (GCAL_DATE_ENTRY (priv->start_date_entry),
                                    start_date.day,
                                    start_date.month,
                                    start_date.year);
          if (end_date.is_date != 1)
            {
              gcal_time_entry_set_time (GCAL_TIME_ENTRY (priv->start_time_entry),
                                        start_date.hour,
                                        start_date.minute);
            }
        }

      g_signal_handlers_unblock_by_func (priv->start_date_entry,
                                         gcal_edit_dialog_date_entry_modified,
                                         user_data);
      g_signal_handlers_unblock_by_func (priv->start_time_entry,
                                         gcal_edit_dialog_date_entry_modified,
                                         user_data);
      g_signal_handlers_unblock_by_func (priv->end_date_entry,
                                         gcal_edit_dialog_date_entry_modified,
                                         user_data);
      g_signal_handlers_unblock_by_func (priv->end_time_entry,
                                         gcal_edit_dialog_date_entry_modified,
                                         user_data);
    }
}

static gboolean
gcal_edit_dialog_source_changed (GcalEditDialog *dialog)
{
  GcalEditDialogPrivate *priv;

  GtkListStore *sources_model;
  gchar* uid;

  priv = gcal_edit_dialog_get_instance_private (dialog);
  sources_model = gcal_manager_get_sources_model (priv->manager);
  gtk_tree_model_get (GTK_TREE_MODEL (sources_model), priv->active_iter,
                      0, &uid,
                      -1);

  if (g_strcmp0 (priv->source_uid, uid) != 0)
    {
      g_free (uid);
      return TRUE;
    }

  g_free (uid);
  return FALSE;
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
gcal_edit_dialog_set_event_data (GcalEditDialog *dialog,
                                 GcalEventData  *data)
{
  GcalEditDialogPrivate *priv;

  GdkRGBA color;
  ESourceSelectable *extension;

  const gchar *const_text = NULL;
  gboolean all_day;

  const gchar *uid = NULL;
  ECalComponentText e_summary;
  ECalComponentDateTime dt;

  priv = gcal_edit_dialog_get_instance_private (dialog);
  all_day = FALSE;

  priv->setting_event = TRUE;

  priv->source = data->source;
  if (priv->component != NULL)
    g_object_unref (priv->component);
  priv->component = e_cal_component_clone (data->event_component);

  if (priv->source_uid != NULL)
    g_free (priv->source_uid);
  priv->source_uid = g_strdup (e_source_get_uid (priv->source));

  if (priv->event_uid != NULL)
    g_free (priv->event_uid);
  e_cal_component_get_uid (priv->component, &uid);
  priv->event_uid = g_strdup (uid);

  /* Clear event data */
  gcal_edit_dialog_clear_data (dialog);

  if (priv->ev_store == NULL)
    {
      priv->ev_store = g_new0 (SimpleEventStore, 1);
      priv->ev_store->time_span = g_new0 (icaltime_span, 1);
    }
  else
    {
      g_free (priv->ev_store->start_date);
      g_free (priv->ev_store->end_date);
    }

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
  e_cal_component_get_dtstart (priv->component, &dt);
  priv->ev_store->start_date = gcal_dup_icaltime (dt.value);
  e_cal_component_free_datetime (&dt);

  gcal_date_entry_set_date (GCAL_DATE_ENTRY (priv->start_date_entry),
                            priv->ev_store->start_date->day,
                            priv->ev_store->start_date->month,
                            priv->ev_store->start_date->year);

  /* end date */
  e_cal_component_get_dtend (priv->component, &dt);
  priv->ev_store->end_date = gcal_dup_icaltime (dt.value);
  e_cal_component_free_datetime (&dt);

  gcal_date_entry_set_date (GCAL_DATE_ENTRY (priv->end_date_entry),
                            priv->ev_store->end_date->day,
                            priv->ev_store->end_date->month,
                            priv->ev_store->end_date->year);

  /* all_day  */
  priv->ev_store->all_day = (priv->ev_store->start_date->is_date == 1 &&
                             priv->ev_store->end_date->is_date == 1);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->all_day_check),
                                priv->ev_store->all_day);

  /* start time */
  if (all_day)
    {
      priv->ev_store->start_date->hour = 0;
      priv->ev_store->start_date->minute = 0;
    }
  gcal_time_entry_set_time (GCAL_TIME_ENTRY (priv->start_time_entry),
                            priv->ev_store->start_date->hour,
                            priv->ev_store->start_date->minute);
  /* end time */
  if (all_day)
    {
      priv->ev_store->end_date->hour = 0;
      priv->ev_store->end_date->minute = 0;
    }
  gcal_time_entry_set_time (GCAL_TIME_ENTRY (priv->end_time_entry),
                            priv->ev_store->end_date->hour,
                            priv->ev_store->end_date->minute);

  /* getting span before going on */
  *(priv->ev_store->time_span) = icaltime_span_new (*(priv->ev_store->start_date),
                                                    *(priv->ev_store->end_date),
                                                    0);

  /* location */
  e_cal_component_get_location (priv->component, &const_text);
  gtk_entry_set_text (GTK_ENTRY (priv->location_entry),
                      const_text != NULL ? const_text : "");
  priv->ev_store->location = const_text != NULL ? g_strdup (const_text) : "";

  /* notes */
  if (priv->ev_store->description != NULL)
    g_free (priv->ev_store->description);

  priv->ev_store->description =
    get_desc_from_component (priv->component,
                             "\n");
  if (priv->ev_store->description != NULL)
    {
      gtk_text_buffer_set_text (
          gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->notes_text)),
          priv->ev_store->description,
          -1);
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

  priv = gcal_edit_dialog_get_instance_private (dialog);

  if (priv->source_uid == NULL ||
      priv->event_uid == NULL)
    {
      return NULL;
    }
  return g_strdup_printf ("%s:%s", priv->source_uid, priv->event_uid);
}

GList*
gcal_edit_dialog_get_modified_properties (GcalEditDialog *dialog)
{
  GcalEditDialogPrivate *priv;
  gchar *desc;
  GList *res;

  priv = gcal_edit_dialog_get_instance_private (dialog);

  if (! priv->writable)
    return NULL;

  res = NULL;

  if (gcal_edit_dialog_date_changed (dialog, TRUE))
    {
      res = g_list_append (res, GINT_TO_POINTER (EVENT_START_DATE));
    }

  if (gcal_edit_dialog_date_changed (dialog, FALSE))
    {
      res = g_list_append (res, GINT_TO_POINTER (EVENT_END_DATE));
    }

  if (g_strcmp0 (priv->ev_store->location,
                 gtk_entry_get_text (GTK_ENTRY (priv->location_entry))) != 0)
    {
      res = g_list_append (res, GINT_TO_POINTER (EVENT_LOCATION));
    }

  /* getting desc */
  desc = gcal_edit_dialog_get_event_description (dialog);
  if (g_strcmp0 (priv->ev_store->description, desc) != 0)
    {
      res = g_list_append (res, GINT_TO_POINTER (EVENT_DESCRIPTION));
    }
  g_free (desc);

  /* and the bigger one is the calendar, the source switched */
  if (gcal_edit_dialog_source_changed (dialog))
    {
      res = g_list_append (res, GINT_TO_POINTER (EVENT_SOURCE));
    }

  return res;
}

const gchar*
gcal_edit_dialog_peek_summary (GcalEditDialog *dialog)
{
  GcalEditDialogPrivate *priv;

  priv = gcal_edit_dialog_get_instance_private (dialog);

  return gtk_entry_get_text (GTK_ENTRY (priv->summary_entry));
}

const gchar*
gcal_edit_dialog_peek_location (GcalEditDialog *dialog)
{
  GcalEditDialogPrivate *priv;

  priv = gcal_edit_dialog_get_instance_private (dialog);

  return gtk_entry_get_text (GTK_ENTRY (priv->location_entry));
}

gchar*
gcal_edit_dialog_get_event_description (GcalEditDialog *dialog)
{
  GcalEditDialogPrivate *priv;
  GtkTextBuffer *buffer;
  GtkTextIter start_iter;
  GtkTextIter end_iter;
  gchar *desc;

  priv = gcal_edit_dialog_get_instance_private (dialog);
  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->notes_text));
  gtk_text_buffer_get_start_iter (buffer, &start_iter);
  gtk_text_buffer_get_end_iter (buffer, &end_iter);
  desc = gtk_text_buffer_get_text (buffer, &start_iter, &end_iter, FALSE);
  return desc;
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
  gcal_time_entry_get_time (GCAL_TIME_ENTRY (priv->start_time_entry),
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
  gcal_time_entry_get_time (GCAL_TIME_ENTRY (priv->end_time_entry),
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

gchar*
gcal_edit_dialog_get_new_source_uid (GcalEditDialog *dialog)
{
  GcalEditDialogPrivate *priv;

  GtkListStore *sources_model;
  gchar* uid;

  priv = gcal_edit_dialog_get_instance_private (dialog);
  sources_model = gcal_manager_get_sources_model (priv->manager);
  gtk_tree_model_get (GTK_TREE_MODEL (sources_model), priv->active_iter,
                      0, &uid,
                      -1);

  return uid;
}
