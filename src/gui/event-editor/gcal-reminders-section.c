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
#include "gcal-event-editor-section.h"
#include "gcal-reminders-section.h"
#include "gcal-utils.h"

#include <libecal/libecal.h>

struct _GcalRemindersSection
{
  GtkBox              parent;

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

  GcalContext        *context;
  GcalEvent          *event;
  GPtrArray          *alarms;
};


static void          gcal_event_editor_section_iface_init        (GcalEventEditorSectionInterface *iface);

static void          on_remove_alarm_cb                          (GcalAlarmRow         *alarm_row,
                                                                  GcalRemindersSection *self);

static void          on_update_alarm_cb                          (GcalAlarmRow         *alarm_row,
                                                                  GcalRemindersSection *self);

G_DEFINE_TYPE_WITH_CODE (GcalRemindersSection, gcal_reminders_section, GTK_TYPE_BOX,
                         G_IMPLEMENT_INTERFACE (GCAL_TYPE_EVENT_EDITOR_SECTION, gcal_event_editor_section_iface_init))

enum
{
  PROP_0,
  PROP_CONTEXT,
  N_PROPS
};

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
  e_cal_component_alarm_set_action (alarm, E_CAL_COMPONENT_ALARM_AUDIO);

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

  g_ptr_array_set_size (self->alarms, 0);

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
  g_signal_connect_object (row, "update-alarm", G_CALLBACK (on_update_alarm_cb), self, 0);

  return row;
}

static void
setup_alarms (GcalRemindersSection *self)
{
  g_autoptr (GList) alarms = NULL;
  GList *l;
  gsize i;

  GCAL_ENTRY;

  clear_alarms (self);

  if (!self->event)
    GCAL_RETURN ();

  /* We start by making all alarm buttons sensitive, and only make them insensitive when needed */
  for (i = 0; i < G_N_ELEMENTS (minutes_button); i++)
    gtk_widget_set_sensitive (WIDGET_FROM_OFFSET (minutes_button[i].button_offset), TRUE);

  alarms = gcal_event_get_alarms (self->event);
  for (l = alarms; l != NULL; l = l->next)
    g_ptr_array_add (self->alarms, l->data);

  for (i = 0; i < self->alarms->len; i++)
    {
      ECalComponentAlarm *alarm;
      GtkWidget *row;
      gint minutes;
      guint j;

      alarm = g_ptr_array_index (self->alarms, i);

      /* Make already-added alarm buttons insensitive */
      minutes = get_alarm_trigger_minutes (self->event, alarm);

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
  gint minutes_a;
  gint minutes_b;

  self = GCAL_REMINDERS_SECTION (user_data);

  if (a == self->new_alarm_row)
    return 1;
  else if (b == self->new_alarm_row)
    return -1;

  alarm_a = gcal_alarm_row_get_alarm (GCAL_ALARM_ROW (a));
  minutes_a = get_alarm_trigger_minutes (self->event, alarm_a);

  alarm_b = gcal_alarm_row_get_alarm (GCAL_ALARM_ROW (b));
  minutes_b = get_alarm_trigger_minutes (self->event, alarm_b);

  return minutes_a - minutes_b;
}

static void
on_update_alarm_cb (GcalAlarmRow         *alarm_row,
                    GcalRemindersSection *self)
{
  ECalComponentAlarm *alarm;
  gint trigger_minutes;
  gsize i;

  GCAL_ENTRY;

  alarm = gcal_alarm_row_get_alarm (alarm_row);
  trigger_minutes = get_alarm_trigger_minutes (self->event, alarm);

  /* Remove from the array */
  for (i = 0; i < self->alarms->len; i++)
    {
      ECalComponentAlarm *a = g_ptr_array_index (self->alarms, i);

      if (trigger_minutes == get_alarm_trigger_minutes (self->event, a))
        {
          GCAL_TRACE_MSG ("Updated alarm for %d minutes", trigger_minutes);

          g_ptr_array_remove_index (self->alarms, i);
          g_ptr_array_insert (self->alarms, i, e_cal_component_alarm_copy (alarm));

          break;
        }
    }

  GCAL_EXIT;
}

static void
on_remove_alarm_cb (GcalAlarmRow         *alarm_row,
                    GcalRemindersSection *self)
{
  ECalComponentAlarm *alarm;
  GtkWidget *alarm_button;
  gint trigger_minutes;
  gsize i;

  GCAL_ENTRY;

  alarm = gcal_alarm_row_get_alarm (alarm_row);
  trigger_minutes = get_alarm_trigger_minutes (self->event, alarm);

  /* Make the button sensitive again */
  alarm_button = get_row_for_alarm_trigger_minutes (self, trigger_minutes);

  if (alarm_button)
    gtk_widget_set_sensitive (alarm_button, TRUE);

  /* Remove from the array */
  for (i = 0; i < self->alarms->len; i++)
    {
      ECalComponentAlarm *a = g_ptr_array_index (self->alarms, i);

      if (trigger_minutes == get_alarm_trigger_minutes (self->event, a))
        {
          GCAL_TRACE_MSG ("Removed alarm for %d minutes", trigger_minutes);

          g_ptr_array_remove_index (self->alarms, i);
          break;
        }
    }

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

  g_ptr_array_add (self->alarms, alarm);

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
 * GcalEventEditorSection interface
 */

static void
gcal_reminders_section_set_event (GcalEventEditorSection *section,
                                  GcalEvent              *event,
                                  GcalEventEditorFlags    flags)
{
  GcalRemindersSection *self = GCAL_REMINDERS_SECTION (section);

  GCAL_ENTRY;

  g_set_object (&self->event, event);
  setup_alarms (self);

  GCAL_EXIT;
}

static void
gcal_reminders_section_apply (GcalEventEditorSection *section)
{
  GcalRemindersSection *self = GCAL_REMINDERS_SECTION (section);
  gint i;

  GCAL_ENTRY;

  gcal_event_remove_all_alarms (self->event);

  for (i = 0; i < self->alarms->len; i++)
    gcal_event_add_alarm (self->event, g_ptr_array_index (self->alarms, i));

  clear_alarms (self);

  GCAL_EXIT;
}

static gboolean
gcal_reminders_section_changed (GcalEventEditorSection *section)
{
  GcalRemindersSection *self;
  g_autoptr (GList) alarms = NULL;

  GCAL_ENTRY;

  self = GCAL_REMINDERS_SECTION (section);

  alarms = gcal_event_get_alarms (self->event);
  if (g_list_length (alarms) != self->alarms->len)
    GCAL_RETURN (TRUE);

  for (GList *l = alarms; l != NULL; l = l->next)
    {
      ECalComponentAlarm *other_alarm;
      ECalComponentAlarm *alarm;

      alarm = l->data;
      other_alarm = NULL;

      for (guint i = 0; i < self->alarms->len; i++)
        {
          ECalComponentAlarm *aux = g_ptr_array_index (self->alarms, i);

          if (get_alarm_trigger_minutes (self->event, alarm) == get_alarm_trigger_minutes (self->event, aux))
            {
              other_alarm = aux;

              if (e_cal_component_alarm_get_action (alarm) != e_cal_component_alarm_get_action (aux))
                GCAL_RETURN (TRUE);

              break;
            }
        }

      if (!other_alarm)
        GCAL_RETURN (TRUE);

      /* TODO: this certainly needs deeper comparisons! */
    }

  GCAL_RETURN (FALSE);
}

static void
gcal_event_editor_section_iface_init (GcalEventEditorSectionInterface *iface)
{
  iface->set_event = gcal_reminders_section_set_event;
  iface->apply = gcal_reminders_section_apply;
  iface->changed = gcal_reminders_section_changed;
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
gcal_reminders_section_finalize (GObject *object)
{
  GcalRemindersSection *self = (GcalRemindersSection *)object;

  g_clear_pointer (&self->alarms, g_ptr_array_unref);
  g_clear_object (&self->context);
  g_clear_object (&self->event);

  G_OBJECT_CLASS (gcal_reminders_section_parent_class)->finalize (object);
}

static void
gcal_reminders_section_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GcalRemindersSection *self = GCAL_REMINDERS_SECTION (object);

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
gcal_reminders_section_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GcalRemindersSection *self = GCAL_REMINDERS_SECTION (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_assert (self->context == NULL);
      self->context = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_reminders_section_class_init (GcalRemindersSectionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gcal_reminders_section_dispose;
  object_class->finalize = gcal_reminders_section_finalize;
  object_class->get_property = gcal_reminders_section_get_property;
  object_class->set_property = gcal_reminders_section_set_property;

  g_object_class_override_property (object_class, PROP_CONTEXT, "context");

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
  self->alarms = g_ptr_array_new_with_free_func (e_cal_component_alarm_free);

  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_list_box_set_sort_func (self->alarms_listbox, sort_alarms_func, self, NULL);
  gtk_widget_set_parent (GTK_WIDGET (self->alarms_popover), GTK_WIDGET (self->new_alarm_row));
}
