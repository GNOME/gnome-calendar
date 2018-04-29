/* gcal-ediat-dialog.c
 *
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
#include "config.h"

#define G_LOG_DOMAIN "GcalEditDialog"

#include "gcal-date-selector.h"
#include "gcal-debug.h"
#include "gcal-edit-dialog.h"
#include "gcal-time-selector.h"
#include "gcal-utils.h"
#include "gcal-event.h"
#include "gcal-recurrence.h"

#include <libecal/libecal.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

/**
 * SECTION:gcal-edit-dialog
 * @short_description: Event editor dialog
 * @title:GcalEditDialog
 * @image:gcal-edit-dialog.png
 *
 * #GcalEditDialog is the event editor dialog of GNOME Calendar. It
 * allows the user to change the various aspects of the events, as
 * well as managing alarms.
 */

struct _GcalEditDialog
{
  GtkDialog         parent;

  gboolean          writable;

  GcalManager      *manager;

  /* titlebar */
  GtkWidget        *titlebar;
  GtkWidget        *title_label;
  GtkWidget        *subtitle_label;

  GtkWidget        *scrolled_window;

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

  GtkWidget        *alarms_listbox;

  GtkWidget        *repeat_combo;
  GtkWidget        *repeat_duration_combo;
  GtkWidget        *repeat_limits_box;

  /* Recurrence widgets */
  GtkWidget        *number_of_occurrences_spin;
  GtkWidget        *repeat_duration_stack;
  GtkWidget        *until_date_selector;

  /* Add Alarms popover buttons */
  GtkWidget        *five_minutes_button;
  GtkWidget        *ten_minutes_button;
  GtkWidget        *thirty_minutes_button;
  GtkWidget        *one_hour_button;
  GtkWidget        *one_day_button;
  GtkWidget        *two_days_button;
  GtkWidget        *three_days_button;
  GtkWidget        *one_week_button;

  /* actions */
  GMenu              *sources_menu;
  GSimpleActionGroup *action_group;

  /* new data holders */
  GcalEvent        *event;
  ESource          *selected_source;

  /* flags */
  GcalTimeFormat    time_format;
  gboolean          event_is_new;
  gboolean          recurrence_changed;
  gboolean          setting_event;
};

static void        on_calendar_selected                          (GSimpleAction     *menu_item,
                                                                  GVariant          *value,
                                                                  gpointer           user_data);

static void        add_alarm_button_clicked                      (GtkWidget         *button,
                                                                  GcalEditDialog    *self);

G_DEFINE_TYPE (GcalEditDialog, gcal_edit_dialog, GTK_TYPE_DIALOG)

enum
{
  PROP_0,
  PROP_EVENT,
  PROP_MANAGER,
  PROP_TIME_FORMAT,
  PROP_WRITABLE,
  N_PROPS
};

static GParamSpec* properties[N_PROPS] = { NULL, };

static const GActionEntry action_entries[] =
{
  { "select-calendar", on_calendar_selected, "s" },
};

static gint
sources_menu_sort_func (gconstpointer a,
                        gconstpointer b)
{
  ESource *source1, *source2;

  source1 = E_SOURCE (a);
  source2 = E_SOURCE (b);

  return g_ascii_strcasecmp (e_source_get_display_name (source1), e_source_get_display_name (source2));
}

static void
fill_sources_menu (GcalEditDialog *dialog)
{
  GList *list;
  GList *aux;

  if (dialog->manager == NULL)
    return;

  list = gcal_manager_get_sources (dialog->manager);
  dialog->sources_menu = g_menu_new ();

  list = g_list_sort (list, sources_menu_sort_func);

  for (aux = list; aux != NULL; aux = aux->next)
    {
      ESource *source;
      GMenuItem *item;
      GdkRGBA color;
      cairo_surface_t *surface;
      GdkPixbuf *pix;

      source = E_SOURCE (aux->data);

      /* retrieve color */
      get_color_name_from_source (source, &color);
      surface = get_circle_surface_from_color (&color, 16);
      pix = gdk_pixbuf_get_from_surface (surface, 0, 0, 16, 16);

      /* menu item */
      item = g_menu_item_new (e_source_get_display_name (source), "select-calendar");
      g_menu_item_set_icon (item, G_ICON (pix));

      /* set insensitive for read-only calendars */
      if (!gcal_manager_is_client_writable (dialog->manager, source))
        {
          g_menu_item_set_action_and_target_value (item, "select-calendar", NULL);
        }
      else
        {
          g_menu_item_set_action_and_target_value (item, "select-calendar",
                                                   g_variant_new_string (e_source_get_uid (source)));
        }

      g_menu_append_item (dialog->sources_menu, item);

      g_clear_pointer (&surface, cairo_surface_destroy);
      g_object_unref (pix);
      g_object_unref (item);
    }

  gtk_popover_bind_model (GTK_POPOVER (dialog->sources_popover), G_MENU_MODEL (dialog->sources_menu), "edit");

  /* HACK: show the popover menu icons */
  fix_popover_menu_icons (GTK_POPOVER (dialog->sources_popover));

  g_list_free (list);
}

static void
on_calendar_selected (GSimpleAction *action,
                      GVariant      *value,
                      gpointer       user_data)
{
  GcalEditDialog *self;
  GList *list;
  GList *aux;
  gchar *uid;

  GCAL_ENTRY;

  self = GCAL_EDIT_DIALOG (user_data);
  list = gcal_manager_get_sources (self->manager);

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
        cairo_surface_t *surface;

        /* retrieve color */
        get_color_name_from_source (source, &color);

        surface = get_circle_surface_from_color (&color, 16);
        gtk_image_set_from_surface (GTK_IMAGE (self->source_image), surface);

        self->selected_source = source;

        gtk_label_set_label (GTK_LABEL (self->subtitle_label), e_source_get_display_name (source));

        g_clear_pointer (&surface, cairo_surface_destroy);
        break;
      }
    }

  g_free (uid);
  g_list_free (list);

  GCAL_EXIT;
}

static void
gcal_edit_dialog_set_writable (GcalEditDialog *dialog,
                               gboolean        writable)
{
  gboolean all_day;

  if (dialog->writable == writable)
    return;

  all_day = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->all_day_check));

  gtk_widget_set_sensitive (dialog->start_time_selector, !all_day && writable);
  gtk_widget_set_sensitive (dialog->end_time_selector, !all_day && writable);

  gtk_button_set_label (GTK_BUTTON (dialog->done_button), writable ? _("Save") : _("Done"));

  dialog->writable = writable;

  g_object_notify_by_pspec (G_OBJECT (dialog), properties[PROP_WRITABLE]);
}

static void
remove_recurrence_properties (GcalEvent *event)
{
  ECalComponent *comp;
  icalcomponent *icalcomp;
  icalproperty *prop;

  comp = gcal_event_get_component (event);
  icalcomp = e_cal_component_get_icalcomponent (comp);

  e_cal_component_set_recurid (comp, NULL);

  prop = icalcomponent_get_first_property (icalcomp, ICAL_RRULE_PROPERTY);

  if (prop)
    {
      icalcomponent_remove_property (icalcomp, prop);
      icalproperty_free (prop);
    }

  e_cal_component_rescan (comp);
}

static void
transient_size_allocate_cb (GcalEditDialog *self)
{
  GtkAllocation alloc;
  GtkWindow *transient;

  transient = gtk_window_get_transient_for (GTK_WINDOW (self));
  gtk_widget_get_allocation (GTK_WIDGET (transient), &alloc);

  gtk_scrolled_window_set_max_content_height (GTK_SCROLLED_WINDOW (self->scrolled_window),
                                              MAX (400, (gint) (0.75 * alloc.height)));
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

static gint
sort_alarms_func (GtkListBoxRow *a,
                  GtkListBoxRow *b,
                  gpointer       user_data)
{
  ECalComponentAlarm *alarm_a, *alarm_b;
  GcalEvent *event_a, *event_b;
  gint minutes_a, minutes_b;

  alarm_a = g_object_get_data (G_OBJECT (a), "alarm");
  alarm_b = g_object_get_data (G_OBJECT (b), "alarm");
  event_a = g_object_get_data (G_OBJECT (a), "event");
  event_b = g_object_get_data (G_OBJECT (b), "event");

  minutes_a = get_alarm_trigger_minutes (event_a, alarm_a);
  minutes_b = get_alarm_trigger_minutes (event_b, alarm_b);

  return minutes_a - minutes_b;
}

static void
fix_reminders_label_height_cb (GtkWidget    *summary_label,
                               GdkRectangle *allocation,
                               GtkWidget    *reminders_label)
{
  gtk_widget_set_size_request (reminders_label, -1, allocation->height);
}

static void
sync_datetimes (GcalEditDialog *self,
                GParamSpec     *pspec,
                GtkWidget      *widget)
{
  GDateTime *start, *end, *start_local, *end_local, *new_date;
  GtkWidget *date_widget, *time_widget;
  gboolean is_start, is_all_day;

  GCAL_ENTRY;

  is_start = (widget == self->start_time_selector || widget == self->start_date_selector);
  is_all_day = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->all_day_check));
  start = gcal_edit_dialog_get_date_start (self);
  end = gcal_edit_dialog_get_date_end (self);

  /* The date is valid, no need to update the fields */
  if (g_date_time_compare (end, start) >= 0)
    GCAL_GOTO (out);

  start_local = g_date_time_to_local (start);
  end_local = g_date_time_to_local (end);

  /*
   * If the user is changing the start date or time, we change the end
   * date or time (and vice versa).
   */
  if (is_start)
    {
      new_date = is_all_day ? g_date_time_add_hours (start, 0) : g_date_time_add_hours (start_local, 1);

      date_widget = self->end_date_selector;
      time_widget = self->end_time_selector;
    }
  else
    {
      new_date = is_all_day ? g_date_time_add_hours (end, 0) : g_date_time_add_hours (end_local, -1);

      date_widget = self->start_date_selector;
      time_widget = self->start_time_selector;
    }

  g_signal_handlers_block_by_func (date_widget, sync_datetimes, self);
  g_signal_handlers_block_by_func (time_widget, sync_datetimes, self);

  gcal_date_selector_set_date (GCAL_DATE_SELECTOR (date_widget), new_date);
  gcal_time_selector_set_time (GCAL_TIME_SELECTOR (time_widget), new_date);

  g_signal_handlers_unblock_by_func (date_widget, sync_datetimes, self);
  g_signal_handlers_unblock_by_func (time_widget, sync_datetimes, self);

  g_clear_pointer (&start_local, g_date_time_unref);
  g_clear_pointer (&end_local, g_date_time_unref);
  g_clear_pointer (&new_date, g_date_time_unref);

out:
  g_clear_pointer (&start, g_date_time_unref);
  g_clear_pointer (&end, g_date_time_unref);

  GCAL_EXIT;
}

static void
action_button_clicked (GtkWidget *widget,
                       gpointer   user_data)
{
  GcalEditDialog *dialog;

  GCAL_ENTRY;

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
      GcalRecurrenceFrequency freq;
      GcalRecurrence *old_recur;
      GDateTime *start_date, *end_date;
      gboolean was_all_day;
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
      was_all_day = gcal_event_get_all_day (dialog->event);

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
       * timezone (unless the event used to be all day, but no longer is).
       */
      start_date = gcal_edit_dialog_get_date_start (dialog);
      end_date = gcal_edit_dialog_get_date_end (dialog);

      /*
       * The end date for multi-day events is exclusive, so we bump it by a day.
       * This fixes the discrepancy between the end day of the event and how it
       * is displayed in the month view. See bug 769300.
       */
      if (all_day)
        {
          GDateTime *fake_end_date = g_date_time_add_days (end_date, 1);

          g_clear_pointer (&end_date, g_date_time_unref);
          end_date = fake_end_date;
        }
      else if (!all_day && was_all_day)
        {
          /* When an all day event is changed to be not an all day event, we
           * need to correct for the fact that the event's timezone was until
           * now set to UTC. That means we need to change the timezone to
           * localtime now, or else it will be saved incorrectly.
           */
          GDateTime *localtime_date;

          localtime_date = g_date_time_to_local (start_date);
          g_clear_pointer (&start_date, g_date_time_unref);
          start_date = localtime_date;

          localtime_date = g_date_time_to_local (end_date);
          g_clear_pointer (&end_date, g_date_time_unref);
          end_date = localtime_date;
        }

      gcal_event_set_date_start (dialog->event, start_date);
      gcal_event_set_date_end (dialog->event, end_date);

      g_clear_pointer (&start_date, g_date_time_unref);
      g_clear_pointer (&end_date, g_date_time_unref);

      /* Check Repeat popover and set recurrence-rules accordingly */
      old_recur = gcal_event_get_recurrence (dialog->event);
      freq = gtk_combo_box_get_active (GTK_COMBO_BOX (dialog->repeat_combo));

      if (freq != GCAL_RECURRENCE_NO_REPEAT)
        {
          GcalRecurrence *recur;

          recur = gcal_recurrence_new ();
          recur->frequency = freq;
          recur->limit_type = gtk_combo_box_get_active (GTK_COMBO_BOX (dialog->repeat_duration_combo));

          if (recur->limit_type == GCAL_RECURRENCE_UNTIL)
            recur->limit.until = gcal_date_selector_get_date (GCAL_DATE_SELECTOR (dialog->until_date_selector));
          else if (recur->limit_type == GCAL_RECURRENCE_COUNT)
            recur->limit.count = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (dialog->number_of_occurrences_spin));

          /* Only apply the new recurrence if it's different from the old one */
          if (!gcal_recurrence_is_equal (old_recur, recur))
            {
              /* Remove the previous recurrence... */
              remove_recurrence_properties (dialog->event);

              /* ... and set the new one */
              gcal_event_set_recurrence (dialog->event, recur);

              dialog->recurrence_changed = TRUE;
            }

          g_clear_pointer (&recur, gcal_recurrence_unref);
        }
      else
        {
          /* When NO_REPEAT is set, make sure to remove the old recurrent */
          remove_recurrence_properties (dialog->event);

          /* If the recurrence from an recurrent event was removed, mark it as changed */
          if (old_recur && old_recur->frequency != GCAL_RECURRENCE_NO_REPEAT)
            dialog->recurrence_changed = TRUE;
        }

      /* Update the source if needed */
      if (dialog->selected_source &&
          gcal_event_get_source (dialog->event) != dialog->selected_source)
        {
          if (dialog->event_is_new)
            {
              gcal_event_set_source (dialog->event, dialog->selected_source);
            }
          else
            {
              gcal_manager_move_event_to_source (dialog->manager,
                                                 dialog->event,
                                                 dialog->selected_source);
            }
        }

      dialog->selected_source = NULL;

      /* Send the response */
      gtk_dialog_response (GTK_DIALOG (dialog),
                           dialog->event_is_new ? GCAL_RESPONSE_CREATE_EVENT : GCAL_RESPONSE_SAVE_EVENT);
    }

  GCAL_EXIT;
}


static void
repeat_duration_changed (GtkComboBox    *widget,
                         GcalEditDialog *self)
{
  switch (gtk_combo_box_get_active (widget))
    {
      case 0:
        gtk_widget_hide (self->repeat_duration_stack);
        break;

      case 1:
        gtk_widget_show (self->repeat_duration_stack);
        gtk_stack_set_visible_child (GTK_STACK (self->repeat_duration_stack), self->number_of_occurrences_spin);
        break;

      case 2:
        gtk_widget_show (self->repeat_duration_stack);
        gtk_stack_set_visible_child (GTK_STACK (self->repeat_duration_stack), self->until_date_selector);
        break;

      default:
        break;
    }
}

static void
repeat_type_changed (GtkComboBox    *combobox,
                     GcalEditDialog *self)
{
  GcalRecurrenceFrequency frequency;
  gboolean has_recurrence;

  frequency = gtk_combo_box_get_active (combobox);
  has_recurrence = frequency != GCAL_RECURRENCE_NO_REPEAT;

  gtk_widget_set_visible (self->repeat_limits_box, has_recurrence);

  if (has_recurrence)
    {
      gtk_combo_box_set_active (GTK_COMBO_BOX (self->repeat_duration_combo), GCAL_RECURRENCE_FOREVER);
      gtk_widget_show (self->repeat_duration_combo);
      gtk_widget_hide (self->repeat_duration_stack);
    }
}

static void
all_day_changed_cb (GtkToggleButton *button,
                    GcalEditDialog  *self)
{
  gboolean active = gtk_toggle_button_get_active (button);

  gtk_widget_set_sensitive (self->start_time_selector, !active);
  gtk_widget_set_sensitive (self->end_time_selector, !active);
}


static void
gcal_edit_dialog_finalize (GObject *object)
{
  GcalEditDialog *dialog;

  GCAL_ENTRY;

  dialog = GCAL_EDIT_DIALOG (object);

  g_clear_object (&dialog->action_group);
  g_clear_object (&dialog->manager);
  g_clear_object (&dialog->event);

  G_OBJECT_CLASS (gcal_edit_dialog_parent_class)->finalize (object);

  GCAL_EXIT;
}

static void
gcal_edit_dialog_constructed (GObject* object)
{
  GcalEditDialog *self;

  self = GCAL_EDIT_DIALOG (object);

  /* chaining up */
  G_OBJECT_CLASS (gcal_edit_dialog_parent_class)->constructed (object);

  gtk_window_set_title (GTK_WINDOW (object), "");

  /* titlebar */
  gtk_window_set_titlebar (GTK_WINDOW (object), self->titlebar);

  /* Actions */
  self->action_group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (self->action_group),
                                   action_entries,
                                   G_N_ELEMENTS (action_entries),
                                   self);

  gtk_widget_insert_action_group (GTK_WIDGET (self),
                                  "edit",
                                  G_ACTION_GROUP (self->action_group));

  /* Watch the main window and adapt the maximum size */
  g_signal_connect_swapped (gtk_window_get_transient_for (GTK_WINDOW (self)),
                            "size-allocate",
                            G_CALLBACK (transient_size_allocate_cb),
                            self);
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

    case PROP_TIME_FORMAT:
      g_value_set_enum (value, self->time_format);
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

    case PROP_TIME_FORMAT:
      gcal_edit_dialog_set_time_format (self, g_value_get_enum (value));
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
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  g_type_ensure (GCAL_TYPE_DATE_SELECTOR);
  g_type_ensure (GCAL_TYPE_TIME_SELECTOR);

  object_class->finalize = gcal_edit_dialog_finalize;
  object_class->constructed = gcal_edit_dialog_constructed;
  object_class->get_property = gcal_edit_dialog_get_property;
  object_class->set_property = gcal_edit_dialog_set_property;

  /**
   * GcalEditDialog::event:
   *
   * The #GcalEvent being edited.
   */
  properties[PROP_EVENT] = g_param_spec_object ("event",
                                                "event of the dialog",
                                                "The event being edited",
                                                GCAL_TYPE_EVENT,
                                                G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GcalEditDialog::manager:
   *
   * The #GcalManager of the dialog.
   */
  properties[PROP_MANAGER] = g_param_spec_object ("manager",
                                                  "Manager of the dialog",
                                                  "The manager of the dialog",
                                                  GCAL_TYPE_MANAGER,
                                                  G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GcalEditDialog::time-format:
   *
   * The time format.
   */
  properties[PROP_TIME_FORMAT] = g_param_spec_enum ("time-format",
                                                    "Manager of the dialog",
                                                    "The manager of the dialog",
                                                    GCAL_TYPE_TIME_FORMAT,
                                                    GCAL_TIME_FORMAT_24H,
                                                    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GcalEditDialog::writable:
   *
   * Whether the current event can be edited or not.
   */
  properties[PROP_WRITABLE] = g_param_spec_boolean ("writable",
                                                    "Whether the current event can be edited",
                                                    "Whether the current event can be edited or not",
                                                    TRUE,
                                                    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/edit-dialog.ui");

  /* Alarms */
  gtk_widget_class_bind_template_child (widget_class, GcalEditDialog, five_minutes_button);
  gtk_widget_class_bind_template_child (widget_class, GcalEditDialog, ten_minutes_button);
  gtk_widget_class_bind_template_child (widget_class, GcalEditDialog, thirty_minutes_button);
  gtk_widget_class_bind_template_child (widget_class, GcalEditDialog, one_hour_button);
  gtk_widget_class_bind_template_child (widget_class, GcalEditDialog, one_day_button);
  gtk_widget_class_bind_template_child (widget_class, GcalEditDialog, two_days_button);
  gtk_widget_class_bind_template_child (widget_class, GcalEditDialog, three_days_button);
  gtk_widget_class_bind_template_child (widget_class, GcalEditDialog, one_week_button);
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
  gtk_widget_class_bind_template_child (widget_class, GcalEditDialog, alarms_listbox);
  gtk_widget_class_bind_template_child (widget_class, GcalEditDialog, notes_text);
  gtk_widget_class_bind_template_child (widget_class, GcalEditDialog, all_day_check);
  gtk_widget_class_bind_template_child (widget_class, GcalEditDialog, titlebar);
  gtk_widget_class_bind_template_child (widget_class, GcalEditDialog, title_label);
  gtk_widget_class_bind_template_child (widget_class, GcalEditDialog, subtitle_label);
  gtk_widget_class_bind_template_child (widget_class, GcalEditDialog, lock);
  gtk_widget_class_bind_template_child (widget_class, GcalEditDialog, number_of_occurrences_spin);
  gtk_widget_class_bind_template_child (widget_class, GcalEditDialog, repeat_combo);
  gtk_widget_class_bind_template_child (widget_class, GcalEditDialog, repeat_duration_combo);
  gtk_widget_class_bind_template_child (widget_class, GcalEditDialog, repeat_duration_stack);
  gtk_widget_class_bind_template_child (widget_class, GcalEditDialog, repeat_limits_box);
  gtk_widget_class_bind_template_child (widget_class, GcalEditDialog, scrolled_window);
  gtk_widget_class_bind_template_child (widget_class, GcalEditDialog, source_image);
  gtk_widget_class_bind_template_child (widget_class, GcalEditDialog, sources_popover);
  gtk_widget_class_bind_template_child (widget_class, GcalEditDialog, until_date_selector);


  /* callbacks */
  gtk_widget_class_bind_template_callback (widget_class, action_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, add_alarm_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, fix_reminders_label_height_cb);
  gtk_widget_class_bind_template_callback (widget_class, all_day_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, repeat_duration_changed);
  gtk_widget_class_bind_template_callback (widget_class, repeat_type_changed);
  gtk_widget_class_bind_template_callback (widget_class, sync_datetimes);
  gtk_widget_class_bind_template_callback (widget_class, update_summary);
  gtk_widget_class_bind_template_callback (widget_class, update_location);
}

static void
gcal_edit_dialog_init (GcalEditDialog *self)
{
  self->writable = TRUE;

  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_list_box_set_sort_func (GTK_LIST_BOX (self->alarms_listbox),
                              sort_alarms_func,
                              self,
                              NULL);
}

static void
gcal_edit_dialog_clear_data (GcalEditDialog *dialog)
{
  /* summary */
  g_signal_handlers_block_by_func (dialog->summary_entry, update_summary, dialog);

  gtk_entry_set_text (GTK_ENTRY (dialog->summary_entry), "");

  g_signal_handlers_unblock_by_func (dialog->summary_entry, update_summary, dialog);

  /* location */
  g_signal_handlers_block_by_func (dialog->location_entry, update_location, dialog);

  gtk_entry_set_text (GTK_ENTRY (dialog->location_entry), "");

  g_signal_handlers_unblock_by_func (dialog->location_entry, update_location, dialog);

  /* notes */
  gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (dialog->notes_text)), "", -1);
}

/*
 * Alarm related functions
 */

#define OFFSET(x)             (G_STRUCT_OFFSET (GcalEditDialog, x))
#define WIDGET_FROM_OFFSET(x) (G_STRUCT_MEMBER (GtkWidget*, self, x))

struct
{
  gint minutes;
  gint button_offset;
} minutes_button[] = {
    { 5,     OFFSET (five_minutes_button) },
    { 10,    OFFSET (ten_minutes_button) },
    { 30,    OFFSET (thirty_minutes_button) },
    { 60,    OFFSET (one_hour_button) },
    { 1440,  OFFSET (one_day_button) },
    { 2880,  OFFSET (two_days_button) },
    { 4320,  OFFSET (three_days_button) },
    { 10080, OFFSET (one_week_button) }
};

static GtkWidget*
get_row_for_alarm_trigger_minutes (GcalEditDialog *self,
                                   gint            minutes)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (minutes_button); i++)
    {
      if (minutes_button[i].minutes == minutes)
        return WIDGET_FROM_OFFSET (minutes_button[i].button_offset);
    }

  return NULL;
}

static void
remove_button_clicked (GtkButton *button,
                       GtkWidget *row)
{
  ECalComponentAlarm *alarm;
  GcalEditDialog *self;
  GtkWidget *alarm_button;
  GcalEvent *event;
  gint trigger_minutes;

  self = GCAL_EDIT_DIALOG (gtk_widget_get_toplevel (row));
  alarm = g_object_get_data (G_OBJECT (row), "alarm");
  event = g_object_get_data (G_OBJECT (row), "event");
  trigger_minutes = get_alarm_trigger_minutes (event, alarm);

  /*
   * Make the button sensitive again
   */
  alarm_button = get_row_for_alarm_trigger_minutes (self, trigger_minutes);

  if (alarm_button)
    gtk_widget_set_sensitive (alarm_button, TRUE);

  gcal_event_remove_alarm (event, trigger_minutes);

  gcal_manager_update_event (self->manager, event, GCAL_RECURRENCE_MOD_THIS_ONLY);

  gtk_widget_destroy (row);

  /*
   * In order to not allocate a spacing between the listbox and the
   * add alarms button, we should always keep the listbox:visible property
   * updated.
   */
  gtk_widget_set_visible (self->alarms_listbox, gcal_event_has_alarms (self->event));
}

static void
sound_toggle_changed (GtkToggleButton *button,
                      GtkWidget       *row)
{
  ECalComponentAlarmAction action;
  ECalComponentAlarm *alarm;
  GtkWidget *image;
  gboolean has_sound;

  alarm = g_object_get_data (G_OBJECT (row), "alarm");
  image = gtk_bin_get_child (GTK_BIN (button));
  has_sound = gtk_toggle_button_get_active (button);

  /* Setup the alarm action */
  action = has_sound ? E_CAL_COMPONENT_ALARM_AUDIO : E_CAL_COMPONENT_ALARM_DISPLAY;

  e_cal_component_alarm_set_action (alarm, action);

  /* Update the volume icon */
  gtk_image_set_from_icon_name (GTK_IMAGE (image),
                                has_sound ? "audio-volume-high-symbolic" : "audio-volume-muted-symbolic",
                                GTK_ICON_SIZE_BUTTON);

}

static GtkWidget*
create_row_for_alarm (GcalEvent          *event,
                      ECalComponentAlarm *alarm)
{
  ECalComponentAlarmAction action;
  GtkBuilder *builder;
  GtkWidget *label, *main_box, *row, *remove_button;
  GtkWidget *volume_button, *volume_icon;
  gboolean has_sound;
  gchar *text;
  gint trigger_minutes;

  trigger_minutes = get_alarm_trigger_minutes (event, alarm);

  /* Something bad happened */
  if (trigger_minutes < 0)
    return NULL;

  if (trigger_minutes < 60)
    {
      text = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE,
                                           "%d minute before",
                                           "%d minutes before",
                                           trigger_minutes),
                              trigger_minutes);
    }
  else if (trigger_minutes < 1440)
    {
      text = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE,
                                           "%d hour before",
                                           "%d hours before",
                                           trigger_minutes / 60),
                              trigger_minutes / 60);
    }
  else if (trigger_minutes < 10080)
    {
      text = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE,
                                           "%d day before",
                                           "%d days before",
                                           trigger_minutes / 1440),
                              trigger_minutes / 1440);
    }
  else
    {
      text = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE,
                                           "%d week before",
                                           "%d weeks before",
                                           trigger_minutes / 10080),
                              trigger_minutes / 10080);
    }

  /* The row */
  row = gtk_list_box_row_new ();
  gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), FALSE);

  g_object_set_data (G_OBJECT (row), "alarm", alarm);
  g_object_set_data (G_OBJECT (row), "event", event);

  /* Build the UI */
  builder = gtk_builder_new_from_resource ("/org/gnome/calendar/alarm-row.ui");

#define WID(x) (GTK_WIDGET (gtk_builder_get_object (builder, x)))

  label = WID ("label");
  gtk_label_set_label (GTK_LABEL (label), text);

  /* Retrieves the actions associated to the alarm */
  e_cal_component_alarm_get_action (alarm, &action);

  /* Updates the volume button to match the action */
  has_sound = action == E_CAL_COMPONENT_ALARM_AUDIO;

  volume_button = WID ("volume_button");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (volume_button), has_sound);

  volume_icon = WID ("volume_icon");
  gtk_image_set_from_icon_name (GTK_IMAGE (volume_icon),
                                has_sound ? "audio-volume-high-symbolic" : "audio-volume-muted-symbolic",
                                GTK_ICON_SIZE_BUTTON);

  g_signal_connect (volume_button,
                    "toggled",
                    G_CALLBACK (sound_toggle_changed),
                    row);

  /* Remove button */
  remove_button = WID ("remove_button");

  g_signal_connect (remove_button,
                    "clicked",
                    G_CALLBACK (remove_button_clicked),
                    row);

  main_box = WID ("main_box");
  gtk_container_add (GTK_CONTAINER (row), main_box);

  gtk_widget_show_all (row);

  g_clear_object (&builder);
  g_free (text);

#undef WID

  return row;
}

static void
setup_alarms (GcalEditDialog *self)
{
  GList *alarms, *l;
  guint i;

  gtk_widget_set_visible (self->alarms_listbox, gcal_event_has_alarms (self->event));

  alarms = gcal_event_get_alarms (self->event);

  /* Remove previous alarms */
  gtk_container_foreach (GTK_CONTAINER (self->alarms_listbox),
                         (GtkCallback) gtk_widget_destroy,
                         NULL);

  /*
   * We start by making all alarm buttons sensitive,
   * and only make them insensitive when needed.
   */
  for (i = 0; i < G_N_ELEMENTS (minutes_button); i++)
    gtk_widget_set_sensitive (WIDGET_FROM_OFFSET (minutes_button[i].button_offset), TRUE);

  for (l = alarms; l != NULL; l = l->next)
    {
      GtkWidget *row;
      gint minutes;

      row = create_row_for_alarm (self->event, l->data);

      if (!row)
        continue;

      /* Make already-added alarm buttons insensitive */
      minutes = get_alarm_trigger_minutes (self->event, l->data);

      for (i = 0; i < G_N_ELEMENTS (minutes_button); i++)
        {
          if (minutes_button[i].minutes == minutes)
            gtk_widget_set_sensitive (WIDGET_FROM_OFFSET (minutes_button[i].button_offset), FALSE);
        }

      /* Add the row */
      gtk_container_add (GTK_CONTAINER (self->alarms_listbox), row);
    }

  g_list_free (alarms);
}

static void
add_alarm_button_clicked (GtkWidget      *button,
                          GcalEditDialog *self)
{
  guint i, minutes;

  /* Search for the button minute */
  minutes = 0;

  for (i = 0; i < G_N_ELEMENTS (minutes_button); i++)
    {
      if (WIDGET_FROM_OFFSET (minutes_button[i].button_offset) == button)
        {
          minutes = minutes_button[i].minutes;
          break;
        }
    }

  if (minutes == 0)
    return;

  /* Add the alarm */
  gcal_event_add_alarm (self->event, minutes, FALSE);

  /*
   * Instead of manually handling stuff, simply remove all alarms and
   * add back again.
   */
  setup_alarms (self);

  /*
   * Since we don't allow more than 1 alarm per time, set the button
   * to insensitive so it cannot be triggered anymore.
   */
  gtk_widget_set_sensitive (button, FALSE);
}

/* Public API */

/**
 * gcal_edit_dialog_new:
 *
 * Creates a new #GcalEditDialog
 *
 * Returns: (transfer full): a #GcalEditDialog
 */
GtkWidget*
gcal_edit_dialog_new (void)
{
  return g_object_new (GCAL_TYPE_EDIT_DIALOG, NULL);
}

/**
 * gcal_edit_dialog_set_time_format:
 * @dialog: a #GcalDialog
 * @use_24h_format: %TRUE to use 24h format, %FALSE otherwise
 *
 * Sets the time format to be used by @dialog.
 */
void
gcal_edit_dialog_set_time_format (GcalEditDialog *dialog,
                                  GcalTimeFormat  time_format)
{
  g_return_if_fail (GCAL_IS_EDIT_DIALOG (dialog));

  dialog->time_format = time_format;

  gcal_time_selector_set_time_format (GCAL_TIME_SELECTOR (dialog->start_time_selector), dialog->time_format);
  gcal_time_selector_set_time_format (GCAL_TIME_SELECTOR (dialog->end_time_selector), dialog->time_format);

  g_object_notify_by_pspec (G_OBJECT (dialog), properties[PROP_TIME_FORMAT]);
}

/**
 * gcal_edit_dialog_set_event_is_new:
 * @dialog: a #GcalDialog
 * @event_is_new: %TRUE if the event is new, %FALSE otherwise
 *
 * Sets whether the currently edited event is a new event, or not.
 * The @dialog will adapt it's UI elements to reflect that.
 */
void
gcal_edit_dialog_set_event_is_new (GcalEditDialog *dialog,
                                   gboolean        event_is_new)
{
  dialog->event_is_new = event_is_new;

  gtk_widget_set_visible (dialog->delete_button, !event_is_new);
}

/**
 * gcal_edit_dialog_get_event:
 * @dialog: a #GcalDialog
 *
 * Retrieves the current event being edited by the @dialog.
 *
 * Returns: (transfer none)(nullable): a #GcalEvent
 */
GcalEvent*
gcal_edit_dialog_get_event (GcalEditDialog *dialog)
{
  g_return_val_if_fail (GCAL_IS_EDIT_DIALOG (dialog), NULL);

  return dialog->event;
}

/**
 * gcal_edit_dialog_set_event:
 * @dialog: a #GcalDialog
 * @event: (nullable): a #GcalEvent
 *
 * Sets the event of the @dialog. When @event is
 * %NULL, the current event information is unset.
 */
void
gcal_edit_dialog_set_event (GcalEditDialog *dialog,
                            GcalEvent      *event)
{
  GcalRecurrenceLimitType limit_type;
  GcalRecurrenceFrequency frequency;
  GcalRecurrence *recur;
  GtkAdjustment *count_adjustment;
  GDateTime *date_start;
  GDateTime *date_end;
  cairo_surface_t *surface;
  ESource *source;
  const gchar *summary;
  gboolean all_day;

  GCAL_ENTRY;

  g_return_if_fail (GCAL_IS_EDIT_DIALOG (dialog));

  g_set_object (&dialog->event, event);

  dialog->setting_event = TRUE;

  count_adjustment = gtk_adjustment_new (0, 2, G_MAXDOUBLE, 1, 1, 10);

  gtk_spin_button_set_adjustment (GTK_SPIN_BUTTON (dialog->number_of_occurrences_spin), count_adjustment);

  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (dialog->number_of_occurrences_spin), TRUE);

  /* If we just set the event to NULL, simply send a property notify */
  if (!event)
    GCAL_GOTO (out);

  /* Recurrences */
  recur = gcal_event_get_recurrence (event);
  frequency = recur ? recur->frequency : GCAL_RECURRENCE_NO_REPEAT;
  limit_type = recur ? recur->limit_type : GCAL_RECURRENCE_FOREVER;

  gtk_combo_box_set_active (GTK_COMBO_BOX (dialog->repeat_combo), frequency);
  gtk_combo_box_set_active (GTK_COMBO_BOX (dialog->repeat_duration_combo), limit_type);

  if (frequency == GCAL_RECURRENCE_NO_REPEAT)
    {
      gtk_widget_hide (dialog->repeat_limits_box);
    }
  else
    {
      gtk_widget_show (dialog->repeat_limits_box);
      gtk_widget_show (dialog->repeat_duration_combo);
    }

  switch (limit_type)
    {
    case GCAL_RECURRENCE_COUNT:
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (dialog->number_of_occurrences_spin), recur->limit.count);
      break;

    case GCAL_RECURRENCE_UNTIL:
      gcal_date_selector_set_date (GCAL_DATE_SELECTOR (dialog->until_date_selector), recur->limit.until);
      break;

    case GCAL_RECURRENCE_FOREVER:
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (dialog->number_of_occurrences_spin), 0);
      break;
    }

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
  surface = get_circle_surface_from_color (gcal_event_get_color (event), 10);
  gtk_image_set_from_surface (GTK_IMAGE (dialog->source_image), surface);
  g_clear_pointer (&surface, cairo_surface_destroy);

  gtk_label_set_label (GTK_LABEL (dialog->subtitle_label), e_source_get_display_name (source));

  /* retrieve start and end dates */
  date_start = gcal_event_get_date_start (event);
  date_start = all_day ? g_date_time_ref (date_start) : g_date_time_to_local (date_start);

  date_end = gcal_event_get_date_end (event);
  /*
   * This is subtracting what has been added in action_button_clicked ().
   * See bug 769300.
   */
  date_end = all_day ? g_date_time_add_days (date_end, -1) : g_date_time_to_local (date_end);

  /* date */
  g_signal_handlers_block_by_func (dialog->end_date_selector, sync_datetimes, dialog);
  g_signal_handlers_block_by_func (dialog->start_date_selector, sync_datetimes, dialog);

  gcal_date_selector_set_date (GCAL_DATE_SELECTOR (dialog->start_date_selector), date_start);
  gcal_date_selector_set_date (GCAL_DATE_SELECTOR (dialog->end_date_selector), date_end);

  g_signal_handlers_unblock_by_func (dialog->start_date_selector, sync_datetimes, dialog);
  g_signal_handlers_unblock_by_func (dialog->end_date_selector, sync_datetimes, dialog);

  /* time */
  g_signal_handlers_block_by_func (dialog->end_time_selector, sync_datetimes, dialog);
  g_signal_handlers_block_by_func (dialog->start_time_selector, sync_datetimes, dialog);

  gcal_time_selector_set_time (GCAL_TIME_SELECTOR (dialog->start_time_selector), date_start);
  gcal_time_selector_set_time (GCAL_TIME_SELECTOR (dialog->end_time_selector), date_end);

  g_signal_handlers_unblock_by_func (dialog->start_time_selector, sync_datetimes, dialog);
  g_signal_handlers_unblock_by_func (dialog->end_time_selector, sync_datetimes, dialog);

  /* all_day  */
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->all_day_check), all_day);

  /* recurrence_changed */
  dialog->recurrence_changed = FALSE;

  /* location */
  gtk_entry_set_text (GTK_ENTRY (dialog->location_entry), gcal_event_get_location (event));

  /* notes */
  gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (dialog->notes_text)),
                            gcal_event_get_description (event),
                            -1);

  gcal_edit_dialog_set_writable (dialog, gcal_manager_is_client_writable (dialog->manager, source));

  g_clear_pointer (&date_start, g_date_time_unref);
  g_clear_pointer (&date_end, g_date_time_unref);

  /* Setup the alarms */
  setup_alarms (dialog);

out:
  g_object_notify_by_pspec (G_OBJECT (dialog), properties[PROP_EVENT]);

  dialog->setting_event = FALSE;

  GCAL_EXIT;
}

/**
 * gcal_edit_dialog_set_manager:
 * @dialog: a #GcalEditDialog
 * @manager: a #GcalManager
 *
 * Sets the #GcalManager instance of the @dialog.
 */
void
gcal_edit_dialog_set_manager (GcalEditDialog *dialog,
                              GcalManager    *manager)
{
  g_return_if_fail (GCAL_IS_EDIT_DIALOG (dialog));
  g_return_if_fail (GCAL_IS_MANAGER (manager));

  if (g_set_object (&dialog->manager, manager))
    g_object_notify_by_pspec (G_OBJECT (dialog), properties[PROP_MANAGER]);
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

/**
 * gcal_edit_dialog_get_date_start:
 * @dialog: a #GcalEditDialog
 *
 * Retrieves the start date of the edit dialog.
 *
 * Returns: (transfer full): a #GDateTime
 */
GDateTime*
gcal_edit_dialog_get_date_start (GcalEditDialog *dialog)
{

  g_return_val_if_fail (GCAL_IS_EDIT_DIALOG (dialog), NULL);

  return return_datetime_for_widgets (dialog,
                                      GCAL_DATE_SELECTOR (dialog->start_date_selector),
                                      GCAL_TIME_SELECTOR (dialog->start_time_selector));
}

/**
 * gcal_edit_dialog_get_date_end:
 * @dialog: a #GcalEditDialog
 *
 * Retrieves the end date of the edit dialog.
 *
 * Returns: (transfer full): a #GDateTime
 */
GDateTime*
gcal_edit_dialog_get_date_end (GcalEditDialog *dialog)
{
  g_return_val_if_fail (GCAL_IS_EDIT_DIALOG (dialog), NULL);

  return return_datetime_for_widgets (dialog,
                                      GCAL_DATE_SELECTOR (dialog->end_date_selector),
                                      GCAL_TIME_SELECTOR (dialog->end_time_selector));
}


gboolean
gcal_edit_dialog_get_recurrence_changed (GcalEditDialog *self)
{
  g_return_val_if_fail (GCAL_IS_EDIT_DIALOG (self), FALSE);

  return self->recurrence_changed;
}
