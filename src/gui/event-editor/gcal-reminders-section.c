/* gcal-reminders-section.c
 *
 * Copyright 2020 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#define G_LOG_DOMAIN "GcalRemindersSection"

#include "gcal-alarm-row.h"
#include "gcal-debug.h"
#include "gcal-event-editor-dialog.h"
#include "gcal-event-editor-section.h"
#include "gcal-reminders-section.h"
#include "gcal-utils.h"

#include <libecal/libecal.h>

struct _GcalRemindersSection
{
  AdwPreferencesGroup parent;

  GtkListBox         *alarms_listbox;
  GtkWidget          *alarms_popover;
  GtkListBoxRow      *new_alarm_row;

  GtkWidget          *event_start_button;
  GtkWidget          *five_minutes_button;
  GtkWidget          *ten_minutes_button;
  GtkWidget          *fifteen_minutes_button;
  GtkWidget          *thirty_minutes_button;
  GtkWidget          *one_hour_button;
  GtkWidget          *one_day_button;
  GtkWidget          *two_days_button;
  GtkWidget          *three_days_button;
  GtkWidget          *one_week_button;
};


static void          on_remove_alarm_cb                          (GcalAlarmRow         *alarm_row,
                                                                  GcalRemindersSection *self);

G_DEFINE_FINAL_TYPE (GcalRemindersSection, gcal_reminders_section, ADW_TYPE_PREFERENCES_GROUP)

/*
 * Auxiliary methods
 */

#define OFFSET(x)             (G_STRUCT_OFFSET (GcalRemindersSection, x))
#define WIDGET_FROM_OFFSET(x) (G_STRUCT_MEMBER (GtkWidget*, self, x))

struct
{
  gint minutes;
  gint button_offset;
} minutes_button[] = {
    { 0,     OFFSET (event_start_button) },
    { 5,     OFFSET (five_minutes_button) },
    { 10,    OFFSET (ten_minutes_button) },
    { 15,    OFFSET (fifteen_minutes_button) },
    { 30,    OFFSET (thirty_minutes_button) },
    { 60,    OFFSET (one_hour_button) },
    { 1440,  OFFSET (one_day_button) },
    { 2880,  OFFSET (two_days_button) },
    { 4320,  OFFSET (three_days_button) },
    { 10080, OFFSET (one_week_button) }
};

static GtkWidget*
get_row_for_alarm_trigger_minutes (GcalRemindersSection *self,
                                   gint                  minutes)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (minutes_button); i++)
    {
      if (minutes_button[i].minutes == minutes)
        return WIDGET_FROM_OFFSET (minutes_button[i].button_offset);
    }

  return NULL;
}

static ECalComponentAlarm*
create_alarm (guint minutes)
{

  ECalComponentAlarmTrigger *trigger;
  ECalComponentAlarm *alarm;
  ICalDuration *duration;

  duration = i_cal_duration_new_null_duration ();
  i_cal_duration_set_is_neg (duration, TRUE);
  i_cal_duration_set_minutes (duration, minutes);

  trigger = e_cal_component_alarm_trigger_new_relative (E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_START, duration);

  g_clear_object (&duration);

  alarm = e_cal_component_alarm_new ();
  e_cal_component_alarm_take_trigger (alarm, trigger);

  /* ONLY create "DISPLAY" alarm types. Don't expose "AUDIO" as an alarm type possibility for users to create;
   * they're mutually independent from "DISPLAY" in RFC5545, and audio-only alarms are useless for users in practice.
   * Furthermore, some other applications don't even allow creating/viewing/editing audio-only alarms on events!
   * We should instead expect alarm notification systems to automatically play sounds when displaying notifications. */
  e_cal_component_alarm_set_action (alarm, E_CAL_COMPONENT_ALARM_DISPLAY);

  return alarm;
}

static void
update_new_alarm_icon (GcalRemindersSection *self)
{
  GtkListBoxRow *first_row = gtk_list_box_get_row_at_index (self->alarms_listbox, 0);

  if (first_row == GTK_LIST_BOX_ROW (self->new_alarm_row))
    adw_button_row_set_start_icon_name (ADW_BUTTON_ROW (self->new_alarm_row), "add-reminder-symbolic");
  else
    adw_button_row_set_start_icon_name (ADW_BUTTON_ROW (self->new_alarm_row), "list-add-symbolic");
}

static void
clear_alarms (GcalRemindersSection *self)
{
  GtkWidget *child;

  child = gtk_widget_get_first_child (GTK_WIDGET (self->alarms_listbox));
  while (child)
    {
      GtkWidget *next = gtk_widget_get_next_sibling (child);

      if (child != GTK_WIDGET (self->new_alarm_row))
        gtk_list_box_remove (self->alarms_listbox, child);

      child = next;
    }

  update_new_alarm_icon (self);
}

static GtkWidget *
create_alarm_row (GcalRemindersSection *self,
                  ECalComponentAlarm    *alarm)
{
  GtkWidget *row;

  row = gcal_alarm_row_new (alarm);
  g_signal_connect_object (row, "remove-alarm", G_CALLBACK (on_remove_alarm_cb), self, 0);

  return row;
}

static void
setup_alarms (GcalRemindersSection *self)
{
  g_autoptr (GList) alarms = NULL;
  GtkWidget *dialog;
  GcalEvent *event;
  GList *l;

  GCAL_ENTRY;

  dialog = gtk_widget_get_ancestor (GTK_WIDGET (self), GCAL_TYPE_EVENT_EDITOR_DIALOG);

  g_assert (GCAL_IS_EVENT_EDITOR_DIALOG (dialog));

  event = gcal_event_editor_dialog_get_event (GCAL_EVENT_EDITOR_DIALOG (dialog));

  clear_alarms (self);

  if (!event)
    GCAL_RETURN ();

  alarms = gcal_event_get_alarms (event);

  for (l = alarms; l != NULL; l = l->next)
    {
      ECalComponentAlarm *alarm;
      GtkWidget *row;
      gint minutes;
      guint j;

      alarm = l->data;

      /* Make already-added alarm buttons insensitive */
      minutes = get_alarm_trigger_minutes (event, alarm);

      for (j = 0; j < G_N_ELEMENTS (minutes_button); j++)
        {
          if (minutes_button[j].minutes == minutes)
            gtk_widget_set_sensitive (WIDGET_FROM_OFFSET (minutes_button[j].button_offset), FALSE);
        }

      GCAL_TRACE_MSG ("Adding alarm for %u minutes", minutes);

      /* Add the row */
      row = create_alarm_row (self, alarm);
      gtk_list_box_append (self->alarms_listbox, row);
    }

  update_new_alarm_icon (self);

  GCAL_EXIT;
}


/*
 * Callbacks
 */

static gint
sort_alarms_func (GtkListBoxRow *a,
                  GtkListBoxRow *b,
                  gpointer       user_data)
{
  ECalComponentAlarm *alarm_a;
  ECalComponentAlarm *alarm_b;
  GcalRemindersSection *self;
  GtkWidget *dialog;
  GcalEvent *event;
  gint minutes_a;
  gint minutes_b;

  self = GCAL_REMINDERS_SECTION (user_data);

  dialog = gtk_widget_get_ancestor (GTK_WIDGET (self), GCAL_TYPE_EVENT_EDITOR_DIALOG);

  g_assert (GCAL_IS_EVENT_EDITOR_DIALOG (dialog));

  event = gcal_event_editor_dialog_get_event (GCAL_EVENT_EDITOR_DIALOG (dialog));

  if (a == self->new_alarm_row)
    return 1;
  else if (b == self->new_alarm_row)
    return -1;

  alarm_a = gcal_alarm_row_get_alarm (GCAL_ALARM_ROW (a));
  minutes_a = get_alarm_trigger_minutes (event, alarm_a);

  alarm_b = gcal_alarm_row_get_alarm (GCAL_ALARM_ROW (b));
  minutes_b = get_alarm_trigger_minutes (event, alarm_b);

  return minutes_a - minutes_b;
}

static void
on_remove_alarm_cb (GcalAlarmRow         *alarm_row,
                    GcalRemindersSection *self)
{
  ECalComponentAlarm *alarm;
  GtkWidget *alarm_button;
  gint trigger_minutes;
  GtkWidget *dialog;
  GcalEvent *event;

  GCAL_ENTRY;

  dialog = gtk_widget_get_ancestor (GTK_WIDGET (self), GCAL_TYPE_EVENT_EDITOR_DIALOG);

  g_assert (GCAL_IS_EVENT_EDITOR_DIALOG (dialog));

  event = gcal_event_editor_dialog_get_event (GCAL_EVENT_EDITOR_DIALOG (dialog));

  alarm = gcal_alarm_row_get_alarm (alarm_row);
  trigger_minutes = get_alarm_trigger_minutes (event, alarm);
  gcal_event_remove_alarm (event, trigger_minutes);

  /* Make the button sensitive again */
  alarm_button = get_row_for_alarm_trigger_minutes (self, trigger_minutes);

  if (alarm_button)
    gtk_widget_set_sensitive (alarm_button, TRUE);

  gtk_list_box_remove (self->alarms_listbox, GTK_WIDGET (alarm_row));

  update_new_alarm_icon (self);

  GCAL_EXIT;
}

static void
on_add_alarm_button_clicked_cb (GtkWidget            *button,
                                GcalRemindersSection *self)
{
  ECalComponentAlarm *alarm;
  GtkWidget *row;
  guint i, minutes;
  GtkWidget *dialog;
  GcalEvent *event;

  dialog = gtk_widget_get_ancestor (GTK_WIDGET (self), GCAL_TYPE_EVENT_EDITOR_DIALOG);

  g_assert (GCAL_IS_EVENT_EDITOR_DIALOG (dialog));

  event = gcal_event_editor_dialog_get_event (GCAL_EVENT_EDITOR_DIALOG (dialog));

  /* Search for the button minute */
  minutes = G_MAXUINT;

  for (i = 0; i < G_N_ELEMENTS (minutes_button); i++)
    {
      if (WIDGET_FROM_OFFSET (minutes_button[i].button_offset) == button)
        {
          minutes = minutes_button[i].minutes;
          break;
        }
    }

  if (minutes == G_MAXUINT)
    return;

  alarm = create_alarm (minutes);

  row = create_alarm_row (self, alarm);
  gtk_list_box_append (self->alarms_listbox, row);
  gcal_event_add_alarm (event, alarm);

  gtk_widget_set_sensitive (button, FALSE);

  update_new_alarm_icon (self);
}

static void
on_alarms_listbox_row_activated_cb (GtkListBox           *alarms_listbox,
                                    GtkListBoxRow        *row,
                                    GcalRemindersSection *self)
{
  if (row == self->new_alarm_row)
    gtk_popover_popup (GTK_POPOVER (self->alarms_popover));
}


/*
 * GtkWidget overrides
 */

static void
gcal_reminders_section_map (GtkWidget *widget)
{
  GcalRemindersSection *self = GCAL_REMINDERS_SECTION (widget);

  setup_alarms (self);

  GTK_WIDGET_CLASS (gcal_reminders_section_parent_class)->map (widget);
}

static void
gcal_reminders_section_unmap (GtkWidget *widget)
{
  GcalRemindersSection *self = GCAL_REMINDERS_SECTION (widget);

  clear_alarms (self);

  for (gsize i = 0; i < G_N_ELEMENTS (minutes_button); i++)
    gtk_widget_set_sensitive (WIDGET_FROM_OFFSET (minutes_button[i].button_offset), TRUE);

  GTK_WIDGET_CLASS (gcal_reminders_section_parent_class)->unmap (widget);
}


/*
 * GObject overrides
 */

static void
gcal_reminders_section_dispose (GObject *object)
{
  GcalRemindersSection *self = (GcalRemindersSection *)object;

  g_clear_pointer (&self->alarms_popover, gtk_widget_unparent);

  G_OBJECT_CLASS (gcal_reminders_section_parent_class)->dispose (object);
}

static void
gcal_reminders_section_class_init (GcalRemindersSectionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gcal_reminders_section_dispose;

  widget_class->map = gcal_reminders_section_map;
  widget_class->unmap = gcal_reminders_section_unmap;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/event-editor/gcal-reminders-section.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalRemindersSection, alarms_listbox);
  gtk_widget_class_bind_template_child (widget_class, GcalRemindersSection, alarms_popover);
  gtk_widget_class_bind_template_child (widget_class, GcalRemindersSection, event_start_button);
  gtk_widget_class_bind_template_child (widget_class, GcalRemindersSection, five_minutes_button);
  gtk_widget_class_bind_template_child (widget_class, GcalRemindersSection, new_alarm_row);
  gtk_widget_class_bind_template_child (widget_class, GcalRemindersSection, one_day_button);
  gtk_widget_class_bind_template_child (widget_class, GcalRemindersSection, one_hour_button);
  gtk_widget_class_bind_template_child (widget_class, GcalRemindersSection, one_week_button);
  gtk_widget_class_bind_template_child (widget_class, GcalRemindersSection, ten_minutes_button);
  gtk_widget_class_bind_template_child (widget_class, GcalRemindersSection, fifteen_minutes_button);
  gtk_widget_class_bind_template_child (widget_class, GcalRemindersSection, thirty_minutes_button);
  gtk_widget_class_bind_template_child (widget_class, GcalRemindersSection, three_days_button);
  gtk_widget_class_bind_template_child (widget_class, GcalRemindersSection, two_days_button);

  gtk_widget_class_bind_template_callback (widget_class, on_add_alarm_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_alarms_listbox_row_activated_cb);
}

static void
gcal_reminders_section_init (GcalRemindersSection *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_list_box_set_sort_func (self->alarms_listbox, sort_alarms_func, self, NULL);
  gtk_widget_set_parent (GTK_WIDGET (self->alarms_popover), GTK_WIDGET (self->new_alarm_row));
}
