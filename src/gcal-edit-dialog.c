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
  GcalEvent        *event;

  /* flags */
  gboolean          format_24h;
  gboolean          event_is_new;
  gboolean          setting_event;
};

static void        fill_sources_menu                      (GcalEditDialog    *dialog);

static void        on_calendar_selected                   (GtkWidget         *menu_item,
                                                           GVariant          *value,
                                                           gpointer           user_data);

static void        update_location                        (GtkEntry          *entry,
                                                           GParamSpec        *pspec,
                                                           gpointer           user_data);

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

G_DEFINE_TYPE (GcalEditDialog, gcal_edit_dialog, GTK_TYPE_DIALOG)

enum
{
  PROP_0,
  PROP_EVENT,
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

        gcal_event_set_source (dialog->event, source);
        gtk_header_bar_set_subtitle (GTK_HEADER_BAR (dialog->titlebar),
                                     e_source_get_display_name (source));
        break;
      }
    }

  g_free (uid);
  g_list_free (list);
}

static void
update_location (GtkEntry   *entry,
                 GParamSpec *pspec,
                 gpointer    user_data)
{
  GcalEditDialog *dialog;

  dialog = GCAL_EDIT_DIALOG (user_data);

  gcal_event_set_location (dialog->event, gtk_entry_get_text (entry));
}

static void
update_summary (GtkEntry   *entry,
                GParamSpec *pspec,
                gpointer    user_data)
{
  GcalEditDialog *dialog;

  dialog = GCAL_EDIT_DIALOG (user_data);

  gtk_widget_set_sensitive (dialog->done_button, gtk_entry_get_text_length (entry) > 0);
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
    case PROP_EVENT:
      g_value_set_object (value, self->event);
      break;

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
    case PROP_EVENT:
      gcal_edit_dialog_set_event (self, g_value_get_object (value));
      break;

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
   * GcalEditDialog::event:
   *
   * The #GcalEvent being edited.
   */
  g_object_class_install_property (object_class,
                                   PROP_EVENT,
                                   g_param_spec_object ("event",
                                                        "event of the dialog",
                                                        "The event being edited",
                                                        GCAL_TYPE_EVENT,
                                                        G_PARAM_READWRITE));

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
}

static void
gcal_edit_dialog_finalize (GObject *object)
{
  GcalEditDialog *dialog;

  dialog = GCAL_EDIT_DIALOG (object);

  g_clear_object (&dialog->action_group);
  g_clear_object (&dialog->action);
  g_clear_object (&dialog->manager);
  g_clear_object (&dialog->event);

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

  /* location */
  g_signal_handlers_block_by_func (dialog->location_entry,
                                   update_location,
                                   dialog);
  gtk_entry_set_text (GTK_ENTRY (dialog->location_entry), "");
  g_signal_handlers_unblock_by_func (dialog->location_entry,
                                     update_location,
                                     dialog);

  /* notes */
  gtk_text_buffer_set_text (
      gtk_text_view_get_buffer (GTK_TEXT_VIEW (dialog->notes_text)),
      "",
      -1);
}

static void
gcal_edit_dialog_action_button_clicked (GtkWidget *widget,
                                        gpointer   user_data)
{
  GcalEditDialog *dialog;

  dialog = GCAL_EDIT_DIALOG (user_data);

  if (widget == dialog->cancel_button || (widget == dialog->done_button && !dialog->writable))
    {
      gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);
    }
  else if (widget == dialog->delete_button)
    {
      gtk_dialog_response (GTK_DIALOG (dialog), GCAL_RESPONSE_DELETE_EVENT);
    }
  else
    {
      GDateTime *start_date, *end_date;
      gboolean all_day;
      gchar *note_text;

      /* Update summary */
      gcal_event_set_summary (dialog->event, gtk_entry_get_text (GTK_ENTRY (dialog->summary_entry)));

      /* Update description */
      g_object_get (G_OBJECT (gtk_text_view_get_buffer (GTK_TEXT_VIEW (dialog->notes_text))),
                    "text", &note_text,
                    NULL);

      gcal_event_set_description (dialog->event, note_text);
      g_free (note_text);

      /* Update all day */
      all_day = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->all_day_check));

      gcal_event_set_all_day (dialog->event, all_day);

      /* By definition, all day events are always UTC */
      if (all_day)
        {
          GTimeZone *utc = g_time_zone_new_utc ();

          gcal_event_set_timezone (dialog->event, utc);

          g_clear_pointer (&utc, g_time_zone_unref);
        }

      /*
       * Update start & end dates. The dates are already translated to the current
       * timezone.
       */
      start_date = gcal_edit_dialog_get_date_start (dialog);
      end_date = gcal_edit_dialog_get_date_end (dialog);

      gcal_event_set_date_start (dialog->event, start_date);
      gcal_event_set_date_end (dialog->event, end_date);

      g_clear_pointer (&start_date, g_date_time_unref);
      g_clear_pointer (&end_date, g_date_time_unref);

      /* Send the response */
      gtk_dialog_response (GTK_DIALOG (dialog),
                           dialog->event_is_new ? GCAL_RESPONSE_CREATE_EVENT : GCAL_RESPONSE_SAVE_EVENT);
    }

  gcal_edit_dialog_set_event (dialog, NULL);
}

static void
gcal_edit_dialog_all_day_changed (GtkWidget *widget,
                                  gpointer   user_data)
{
  GcalEditDialog *dialog;
  gboolean active;

  dialog = GCAL_EDIT_DIALOG (user_data);
  active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->all_day_check));

  gtk_widget_set_sensitive (dialog->start_time_selector, !active);
  gtk_widget_set_sensitive (dialog->end_time_selector, !active);
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

GcalEvent*
gcal_edit_dialog_get_event (GcalEditDialog *dialog)
{
  g_return_val_if_fail (GCAL_IS_EDIT_DIALOG (dialog), NULL);

  return dialog->event;
}

void
gcal_edit_dialog_set_event (GcalEditDialog *dialog,
                            GcalEvent      *event)
{
  g_return_if_fail (GCAL_IS_EDIT_DIALOG (dialog));

  if (g_set_object (&dialog->event, event))
    {
      GDateTime *date_start;
      GDateTime *date_end;
      GdkPixbuf *pix;
      ESource *source;
      const gchar *summary;
      gboolean all_day;

      dialog->setting_event = TRUE;

      /* If we just set the event to NULL, simply send a property notify */
      if (!event)
        goto out;

      all_day = gcal_event_get_all_day (event);
      source = gcal_event_get_source (event);

      /* Clear event data */
      gcal_edit_dialog_clear_data (dialog);

      /* update sources list */
      if (dialog->sources_menu != NULL)
        g_menu_remove_all (dialog->sources_menu);

      fill_sources_menu (dialog);

      /* Load new event data */
      /* summary */
      summary = gcal_event_get_summary (event);

      if (g_strcmp0 (summary, "") == 0)
        gtk_entry_set_text (GTK_ENTRY (dialog->summary_entry), _("Unnamed event"));
      else
        gtk_entry_set_text (GTK_ENTRY (dialog->summary_entry), summary);

      /* dialog titlebar's title & subtitle */
      pix = gcal_get_pixbuf_from_color (gcal_event_get_color (event), 16);
      gtk_image_set_from_pixbuf (GTK_IMAGE (dialog->source_image), pix);
      g_object_unref (pix);

      gtk_header_bar_set_subtitle (GTK_HEADER_BAR (dialog->titlebar),
                                   e_source_get_display_name (source));

      /* retrieve start and end dates */
      date_start = gcal_event_get_date_start (event);
      date_start = all_day ? g_date_time_ref (date_start) : g_date_time_to_local (date_start);

      date_end = gcal_event_get_date_end (event);
      date_end = all_day ? g_date_time_ref (date_end) : g_date_time_to_local (date_end);

      /* date */
      gcal_date_selector_set_date (GCAL_DATE_SELECTOR (dialog->start_date_selector), date_start);
      gcal_date_selector_set_date (GCAL_DATE_SELECTOR (dialog->end_date_selector), date_end);

      /* time */
      gcal_time_selector_set_time (GCAL_TIME_SELECTOR (dialog->start_time_selector), date_start);
      gcal_time_selector_set_time (GCAL_TIME_SELECTOR (dialog->end_time_selector), date_end);

      /* all_day  */
      gtk_widget_set_sensitive (dialog->start_time_selector, dialog->writable && !all_day);
      gtk_widget_set_sensitive (dialog->end_time_selector, dialog->writable && !all_day);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->all_day_check), all_day);

      /* location */
      gtk_entry_set_text (GTK_ENTRY (dialog->location_entry), gcal_event_get_location (event));

      /* notes */
      gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (dialog->notes_text)),
                                gcal_event_get_description (event),
                                -1);

      gcal_edit_dialog_set_writable (dialog, gcal_manager_is_client_writable (dialog->manager, source));

      g_clear_pointer (&date_start, g_date_time_unref);
      g_clear_pointer (&date_end, g_date_time_unref);

out:
      g_object_notify (G_OBJECT (dialog), "event");

      dialog->setting_event = FALSE;
    }
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

static GDateTime*
return_datetime_for_widgets (GcalEditDialog   *dialog,
                             GcalDateSelector *date_selector,
                             GcalTimeSelector *time_selector)
{
  GTimeZone *tz;
  GDateTime *date;
  GDateTime *time;
  GDateTime *retval;
  gboolean all_day;

  /* Use UTC timezone for All Day events, otherwise use the event's timezone */
  all_day = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->all_day_check));
  tz = all_day ? g_time_zone_new_utc () : g_time_zone_new_local ();

  date = gcal_date_selector_get_date (date_selector);
  time = gcal_time_selector_get_time (time_selector);

  retval = g_date_time_new (tz,
                            g_date_time_get_year (date),
                            g_date_time_get_month (date),
                            g_date_time_get_day_of_month (date),
                            g_date_time_get_hour (time),
                            g_date_time_get_minute (time),
                            0);

  /*
   * If the event is not all day, the timezone may be different from UTC or
   * local. In any case, since we're editing the event in the current timezone,
   * we should always correct the timezone to the event's timezone.
   */
  if (!all_day)
    {
      GDateTime *aux = retval;

      retval = g_date_time_to_timezone (aux, gcal_event_get_timezone (dialog->event));

      g_clear_pointer (&aux, g_date_time_unref);
    }

  g_clear_pointer (&tz, g_time_zone_unref);

  return retval;
}

GDateTime*
gcal_edit_dialog_get_date_start (GcalEditDialog *dialog)
{

  g_return_val_if_fail (GCAL_IS_EDIT_DIALOG (dialog), NULL);

  return return_datetime_for_widgets (dialog,
                                      GCAL_DATE_SELECTOR (dialog->start_date_selector),
                                      GCAL_TIME_SELECTOR (dialog->start_time_selector));
}

GDateTime*
gcal_edit_dialog_get_date_end (GcalEditDialog *dialog)
{
  g_return_val_if_fail (GCAL_IS_EDIT_DIALOG (dialog), NULL);

  return return_datetime_for_widgets (dialog,
                                      GCAL_DATE_SELECTOR (dialog->end_date_selector),
                                      GCAL_TIME_SELECTOR (dialog->end_time_selector));
}
