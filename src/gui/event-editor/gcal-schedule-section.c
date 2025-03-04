/* gcal-schedule-section.c
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

#define G_LOG_DOMAIN "GcalScheduleSection"

#include "gcal-context.h"
#include "gcal-date-selector.h"
#include "gcal-date-chooser-row.h"
#include "gcal-date-time-chooser.h"
#include "gcal-debug.h"
#include "gcal-event.h"
#include "gcal-event-editor-section.h"
#include "gcal-recurrence.h"
#include "gcal-schedule-section.h"
#include "gcal-utils.h"

#include <glib/gi18n.h>

struct _GcalScheduleSection
{
  GtkBox               parent;

  GcalScheduleValues  *values;

  AdwToggleGroup      *schedule_type_toggle_group;
  AdwPreferencesGroup *start_date_group;
  GcalDateChooserRow  *start_date_row;
  AdwPreferencesGroup *end_date_group;
  GcalDateChooserRow  *end_date_row;
  GcalDateTimeChooser *start_date_time_chooser;
  GcalDateTimeChooser *end_date_time_chooser;
  GtkWidget           *number_of_occurrences_spin;
  GtkWidget           *repeat_combo;
  GtkWidget           *repeat_duration_combo;
  GtkWidget           *until_date_selector;

  GcalContext        *context;
  GcalEvent          *event;

  GcalEventEditorFlags flags;
};

/* Desired state of the widgets, computed from GcalScheduleValues.
 *
 * There is some subtle logic to decide how to update the widgets based on how
 * each of them causes the GcalScheduleValues to be modified.  We encapsulate
 * the desired state in this struct, so we can have tests for it.
 */
typedef struct
{
  gboolean schedule_type_all_day;
  gboolean date_widgets_visible;
  gboolean date_time_widgets_visible;
  GcalTimeFormat time_format;

  /* Note that the displayed date/time may not correspond to the real data in the
   * GcalScheduleValues.  See the comment in widget_state_from_values().
   */
  GDateTime *date_time_start;
  GDateTime *date_time_end;
} WidgetState;

static void widget_state_free (WidgetState *state);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (WidgetState, widget_state_free);

static void          gcal_event_editor_section_iface_init        (GcalEventEditorSectionInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GcalScheduleSection, gcal_schedule_section, GTK_TYPE_BOX,
                         G_IMPLEMENT_INTERFACE (GCAL_TYPE_EVENT_EDITOR_SECTION, gcal_event_editor_section_iface_init))

enum
{
  PROP_0,
  PROP_CONTEXT,
  N_PROPS
};

static void          on_start_date_changed_cb                    (GtkWidget          *widget,
                                                                  GParamSpec         *pspec,
                                                                  GcalScheduleSection *self);

static void          on_end_date_changed_cb                      (GtkWidget          *widget,
                                                                  GParamSpec         *pspec,
                                                                  GcalScheduleSection *self);

static void          on_start_date_time_changed_cb               (GtkWidget          *widget,
                                                                  GParamSpec         *pspec,
                                                                  GcalScheduleSection *self);

static void          on_end_date_time_changed_cb                 (GtkWidget          *widget,
                                                                  GParamSpec         *pspec,
                                                                  GcalScheduleSection *self);

static void          on_schedule_type_changed_cb                 (GtkWidget           *widget,
                                                                  GParamSpec          *pspec,
                                                                  GcalScheduleSection *self);

static GcalScheduleValues *
gcal_schedule_values_copy (const GcalScheduleValues *values)
{
  GcalScheduleValues *copy = g_new0 (GcalScheduleValues, 1);

  copy->all_day = values->all_day;
  copy->orig_date_start = values->orig_date_start ? g_date_time_ref (values->orig_date_start) : NULL;
  copy->orig_date_end = values->orig_date_end ? g_date_time_ref (values->orig_date_end) : NULL;
  copy->date_start = values->date_start ? g_date_time_ref (values->date_start) : NULL;
  copy->date_end = values->date_end ? g_date_time_ref (values->date_end) : NULL;
  copy->recur = values->recur ? gcal_recurrence_ref (values->recur) : NULL;
  copy->time_format = values->time_format;

  return copy;
}

static GcalScheduleValues *
gcal_schedule_values_set_all_day (const GcalScheduleValues *values, gboolean all_day)
{
  GcalScheduleValues *copy = gcal_schedule_values_copy (values);

  copy->all_day = all_day;
  return copy;
}

static GcalScheduleValues *
gcal_schedule_values_set_time_format (const GcalScheduleValues *values, GcalTimeFormat time_format)
{
  GcalScheduleValues *copy = gcal_schedule_values_copy (values);

  copy->time_format = time_format;
  return copy;
}

/* The start_date_row widget has changed.  We need to sync it to the values and
 * adjust the other values based on it.
 */
static GcalScheduleValues *
gcal_schedule_values_set_start_date (const GcalScheduleValues *values, GDateTime *start)
{
  GcalScheduleValues *copy = gcal_schedule_values_copy (values);

  gcal_set_date_time (&copy->date_start, start);

  if (g_date_time_compare (start, copy->date_end) == 1)
    {
      gcal_set_date_time (&copy->date_end, start);
    }

  return copy;
}

/* The end_date_row widget has changed.  We need to sync it to the values and
 * adjust the other values based on it.
 */
static GcalScheduleValues *
gcal_schedule_values_set_end_date (const GcalScheduleValues *values, GDateTime *end)
{
  GcalScheduleValues *copy = gcal_schedule_values_copy (values);

  gcal_set_date_time (&copy->date_end, end);

  if (g_date_time_compare (copy->date_start, end) == 1)
    {
      gcal_set_date_time (&copy->date_start, end);
    }

  return copy;
}

static WidgetState *
widget_state_from_values (const GcalScheduleValues *values)
{
  WidgetState *state = g_new0 (WidgetState, 1);

  state->schedule_type_all_day = values->all_day;
  state->date_widgets_visible = values->all_day;
  state->date_time_widgets_visible = !values->all_day;
  state->time_format = values->time_format;

  g_assert (values->date_start);
  state->date_time_start = g_date_time_ref (values->date_start);

  if (values->all_day)
    {
      /* While in iCalendar a single-day all-day event goes from $day to $day+1, we don't want
       * to show a single-day all-day event as starting on Feb 1 and ending on Feb 2, for
       * example.  So, we subtract a day from the end date to show the expected thing to the
       * user.  We restore this when setting the date-time back on the event.
       *
       * See https://bugzilla.gnome.org/show_bug.cgi?id=769300 for the original bug about this.
       */
      state->date_time_end = g_date_time_add_days (values->date_end, -1);
    }
  else
    {
      g_assert (values->date_end);
      state->date_time_end = g_date_time_ref (values->date_end);
    }

  return state;
}

static void
widget_state_free (WidgetState *state)
{
  g_date_time_unref (state->date_time_start);
  g_date_time_unref (state->date_time_end);
  g_free (state);
}

/*
 * Auxiliary methods
 */

static inline gboolean
all_day_selected (GcalScheduleSection *self)
{
  const gchar *active = adw_toggle_group_get_active_name (self->schedule_type_toggle_group);
  return g_strcmp0 (active, "all-day") == 0;
}


static void
block_date_signals (GcalScheduleSection *self)
{
  g_signal_handlers_block_by_func (self->start_date_row, on_start_date_changed_cb, self);
  g_signal_handlers_block_by_func (self->end_date_row, on_end_date_changed_cb, self);

  g_signal_handlers_block_by_func (self->start_date_time_chooser, on_start_date_time_changed_cb, self);
  g_signal_handlers_block_by_func (self->end_date_time_chooser, on_end_date_time_changed_cb, self);
}

static void
unblock_date_signals (GcalScheduleSection *self)
{
  g_signal_handlers_unblock_by_func (self->end_date_time_chooser, on_end_date_time_changed_cb, self);
  g_signal_handlers_unblock_by_func (self->start_date_time_chooser, on_start_date_time_changed_cb, self);

  g_signal_handlers_unblock_by_func (self->end_date_row, on_end_date_changed_cb, self);
  g_signal_handlers_unblock_by_func (self->start_date_row, on_start_date_changed_cb, self);
}

static void
remove_recurrence_properties (GcalEvent *event)
{
  ECalComponent *comp;

  comp = gcal_event_get_component (event);

  e_cal_component_set_recurid (comp, NULL);
  e_cal_component_set_rrules (comp, NULL);
  e_cal_component_commit_sequence (comp);
}

static void
update_widgets (GcalScheduleSection *self,
                WidgetState         *state)
{
  g_signal_handlers_block_by_func (self->schedule_type_toggle_group, on_schedule_type_changed_cb, self);
  block_date_signals (self);

  adw_toggle_group_set_active_name (self->schedule_type_toggle_group,
                                    state->schedule_type_all_day ? "all-day" : "time-slot");

  gtk_widget_set_visible (GTK_WIDGET (self->start_date_group), state->date_widgets_visible);
  gtk_widget_set_visible (GTK_WIDGET (self->end_date_group), state->date_widgets_visible);
  gtk_widget_set_visible (GTK_WIDGET (self->start_date_time_chooser), state->date_time_widgets_visible);
  gtk_widget_set_visible (GTK_WIDGET (self->end_date_time_chooser), state->date_time_widgets_visible);

  gcal_date_time_chooser_set_time_format (self->start_date_time_chooser, state->time_format);
  gcal_date_time_chooser_set_time_format (self->end_date_time_chooser, state->time_format);

  /* date */
  gcal_date_chooser_row_set_date (self->start_date_row, state->date_time_start);
  gcal_date_chooser_row_set_date (self->end_date_row, state->date_time_end);

  /* date-time */
  gcal_date_time_chooser_set_date_time (self->start_date_time_chooser, state->date_time_start);
  gcal_date_time_chooser_set_date_time (self->end_date_time_chooser, state->date_time_end);

  unblock_date_signals (self);
  g_signal_handlers_unblock_by_func (self->schedule_type_toggle_group, on_schedule_type_changed_cb, self);
}

/* Updates the widgets, and the current values, from the specified ones.
 *
 * This will free the current values and replace them with new ones.
 */
static void
update_from_values (GcalScheduleSection *self,
                    GcalScheduleValues *values)
{
  g_autoptr (WidgetState) state = widget_state_from_values (values);

  update_widgets (self, state);

  gcal_schedule_values_free (self->values);
  self->values = values;
}

/*
 * Callbacks
 */

static void
on_schedule_type_changed_cb (GtkWidget           *widget,
                             GParamSpec          *pspec,
                             GcalScheduleSection *self)
{
  gboolean all_day = all_day_selected (self);
  GcalScheduleValues *updated = gcal_schedule_values_set_all_day (self->values, all_day);
  update_from_values (self, updated);
}

static void
on_start_date_changed_cb (GtkWidget           *widget,
                          GParamSpec          *pspec,
                          GcalScheduleSection *self)
{
  GDateTime *start = gcal_date_chooser_row_get_date (self->start_date_row);
  GcalScheduleValues *updated = gcal_schedule_values_set_start_date (self->values, start);
  update_from_values (self, updated);
}

static void
on_end_date_changed_cb (GtkWidget           *widget,
                        GParamSpec          *pspec,
                        GcalScheduleSection *self)
{
  GDateTime *end = gcal_date_chooser_row_get_date (self->end_date_row);
  GcalScheduleValues *updated = gcal_schedule_values_set_end_date (self->values, end);
  update_from_values (self, updated);
}

static void
on_start_date_time_changed_cb (GtkWidget           *widget,
                               GParamSpec          *pspec,
                               GcalScheduleSection *self)
{
  GDateTime *start, *end;

  GCAL_ENTRY;

  block_date_signals (self);

  start = gcal_date_time_chooser_get_date_time (self->start_date_time_chooser);
  end = gcal_date_time_chooser_get_date_time (self->end_date_time_chooser);

  if (g_date_time_compare (start, end) == 1)
    {
      GTimeZone *end_tz = g_date_time_get_timezone (end);
      g_autoptr (GDateTime) new_end = g_date_time_add_hours (start, 1);
      g_autoptr (GDateTime) new_end_in_end_tz = g_date_time_to_timezone (new_end, end_tz);

      gcal_date_time_chooser_set_date_time (self->end_date_time_chooser, new_end_in_end_tz);
    }

  unblock_date_signals (self);

  GCAL_EXIT;
}

static void
on_end_date_time_changed_cb (GtkWidget           *widget,
                             GParamSpec          *pspec,
                             GcalScheduleSection *self)
{
  GDateTime *start, *end;

  GCAL_ENTRY;

  block_date_signals (self);

  start = gcal_date_time_chooser_get_date_time (self->start_date_time_chooser);
  end = gcal_date_time_chooser_get_date_time (self->end_date_time_chooser);

  if (g_date_time_compare (start, end) == 1)
    {
      GTimeZone *start_tz = g_date_time_get_timezone (start);
      g_autoptr (GDateTime) new_start = g_date_time_add_hours (end, -1);
      g_autoptr (GDateTime) new_start_in_start_tz = g_date_time_to_timezone (new_start, start_tz);

      gcal_date_time_chooser_set_date_time (self->start_date_time_chooser, new_start_in_start_tz);
    }

  unblock_date_signals (self);

  GCAL_EXIT;
}

static void
on_repeat_duration_changed_cb (GtkWidget           *widget,
                               GParamSpec          *pspec,
                               GcalScheduleSection *self)
{
  gint active = adw_combo_row_get_selected (ADW_COMBO_ROW (widget));

  gtk_widget_set_visible (self->number_of_occurrences_spin, active == 1);
  gtk_widget_set_visible (self->until_date_selector, active == 2);
}

static void
on_repeat_type_changed_cb (GtkWidget           *widget,
                           GParamSpec          *pspec,
                           GcalScheduleSection *self)
{
  GcalRecurrenceFrequency frequency;

  frequency = adw_combo_row_get_selected (ADW_COMBO_ROW (widget));

  if (frequency == GCAL_RECURRENCE_NO_REPEAT)
    adw_combo_row_set_selected (ADW_COMBO_ROW (self->repeat_duration_combo), GCAL_RECURRENCE_FOREVER);

  gtk_widget_set_visible (self->repeat_duration_combo, frequency != GCAL_RECURRENCE_NO_REPEAT);
}

static void
on_time_format_changed_cb (GcalScheduleSection *self)
{
  GcalTimeFormat time_format = gcal_context_get_time_format (self->context);

  GcalScheduleValues *updated = gcal_schedule_values_set_time_format (self->values, time_format);

  WidgetState *state = widget_state_from_values (updated);
  update_widgets (self, state);
  widget_state_free (state);
}


/*
 * GcalEventEditorSection interface
 */

/*
 * Sets the event of the schedule section.
 *
 * If the event is not a new event, the event date start and date end timezones are kept and displayed to the user.
 *
 * If the event is a new event, the event date start and date end timezones are forgotten. The results of
 * g_date_time_get_year, g_date_time_get_month, g_date_time_get_day_of_month, g_date_time_get_hour and
 * g_date_time_get_minute are used for the date row and the date time chooser.
 * The date row timezone is set to UTC (as all day events use the UTC timezone).
 * The date time chooser timezone is set to local.
 */
static void
gcal_schedule_section_set_event (GcalEventEditorSection *section,
                                 GcalEvent              *event,
                                 GcalEventEditorFlags    flags)
{
  GcalScheduleSection *self;
  GcalRecurrenceLimitType limit_type;
  GcalRecurrenceFrequency frequency;
  GcalTimeFormat time_format;
  GcalScheduleValues *values;
  g_autoptr (WidgetState) state = NULL;

  GCAL_ENTRY;

  self = GCAL_SCHEDULE_SECTION (section);

  g_set_object (&self->event, event);
  self->flags = flags;

  time_format = gcal_context_get_time_format (self->context);
  self->values = gcal_schedule_values_from_event (event, time_format);
  values = self->values; /* alias to avoid "self->values" everywhere */

  if (!event)
    GCAL_RETURN ();

  state = widget_state_from_values (values);
  update_widgets (self, state);

  /* Recurrences */
  if (values->recur)
    {
      frequency = values->recur->frequency;
      limit_type = values->recur->limit_type;
    }
  else
    {
      frequency = GCAL_RECURRENCE_NO_REPEAT;
      limit_type = GCAL_RECURRENCE_FOREVER;
    }

  adw_combo_row_set_selected (ADW_COMBO_ROW (self->repeat_combo), frequency);
  adw_combo_row_set_selected (ADW_COMBO_ROW (self->repeat_duration_combo), limit_type);

  if (frequency == GCAL_RECURRENCE_NO_REPEAT)
    {
      gtk_widget_set_visible (self->repeat_duration_combo, FALSE);
    }
  else
    {
      gtk_widget_set_visible (self->repeat_duration_combo, TRUE);
      gtk_widget_set_visible (self->repeat_duration_combo, TRUE);
    }

  switch (limit_type)
    {
    case GCAL_RECURRENCE_COUNT:
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (self->number_of_occurrences_spin), values->recur->limit.count);
      break;

    case GCAL_RECURRENCE_UNTIL:
      gcal_date_selector_set_date (GCAL_DATE_SELECTOR (self->until_date_selector), values->recur->limit.until);
      break;

    case GCAL_RECURRENCE_FOREVER:
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (self->number_of_occurrences_spin), 0);
      break;
    }

  GCAL_EXIT;
}

static void
gcal_schedule_section_apply_to_event (GcalScheduleSection *self,
                                      GcalEvent           *event)
{
  g_autoptr (GDateTime) start_date = NULL;
  g_autoptr (GDateTime) end_date = NULL;
  GcalRecurrenceFrequency freq;
  gboolean all_day;

  GCAL_ENTRY;

  all_day = all_day_selected (self);

  if (!all_day)
    {
      start_date = gcal_date_time_chooser_get_date_time (self->start_date_time_chooser);
      end_date = gcal_date_time_chooser_get_date_time (self->end_date_time_chooser);
    }
  else
    {
      start_date = g_date_time_ref (gcal_date_chooser_row_get_date (self->start_date_row));
      end_date = g_date_time_ref (gcal_date_chooser_row_get_date (self->end_date_row));
    }

#ifdef GCAL_ENABLE_TRACE
    {
      g_autofree gchar *start_dt_string = g_date_time_format (start_date, "%x %X %z");
      g_autofree gchar *end_dt_string = g_date_time_format (end_date, "%x %X %z");

      g_debug ("New start date: %s", start_dt_string);
      g_debug ("New end date: %s", end_dt_string);
    }
#endif

  gcal_event_set_all_day (event, all_day);

  if (all_day)
    {
      /* See the comment "While in iCalendar" elsewhere in this file.  Here we restore the
       * correct date-time for the event.
       *
       * See https://bugzilla.gnome.org/show_bug.cgi?id=769300 for the original bug about this.
       */
      GDateTime *fake_end_date = g_date_time_add_days (end_date, 1);

      end_date = fake_end_date;
    }

  gcal_event_set_date_start (event, start_date);
  gcal_event_set_date_end (event, end_date);

  /* Check Repeat popover and set recurrence-rules accordingly */

  freq = adw_combo_row_get_selected (ADW_COMBO_ROW (self->repeat_combo));

  if (freq != GCAL_RECURRENCE_NO_REPEAT)
    {
      g_autoptr (GcalRecurrence) recur = NULL;

      recur = gcal_recurrence_new ();
      recur->frequency = freq;
      recur->limit_type = adw_combo_row_get_selected (ADW_COMBO_ROW (self->repeat_duration_combo));

      if (recur->limit_type == GCAL_RECURRENCE_UNTIL)
        recur->limit.until = g_date_time_ref (gcal_date_selector_get_date (GCAL_DATE_SELECTOR (self->until_date_selector)));
      else if (recur->limit_type == GCAL_RECURRENCE_COUNT)
        recur->limit.count = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (self->number_of_occurrences_spin));

      /* Only apply the new recurrence if it's different from the old one */
      if (!gcal_recurrence_is_equal (self->values->recur, recur))
        {
          /* Remove the previous recurrence... */
          remove_recurrence_properties (event);

          /* ... and set the new one */
          gcal_event_set_recurrence (event, recur);
        }
    }
  else
    {
      /* When NO_REPEAT is set, make sure to remove the old recurrent */
      remove_recurrence_properties (event);
    }

  GCAL_EXIT;
}

static void
gcal_schedule_section_apply (GcalEventEditorSection *section)
{
  GcalScheduleSection *self = GCAL_SCHEDULE_SECTION (section);

  GCAL_ENTRY;

  gcal_schedule_section_apply_to_event (self, self->event);

  gcal_schedule_values_free (self->values);

  GcalTimeFormat time_format = gcal_context_get_time_format (self->context);
  self->values = gcal_schedule_values_from_event (self->event, time_format);

  GCAL_EXIT;
}

static gboolean
gcal_schedule_section_changed (GcalEventEditorSection *section)
{
  GcalScheduleSection *self;
  GDateTime *start_date, *end_date, *prev_start_date, *prev_end_date;
  GTimeZone *start_tz, *end_tz, *prev_start_tz, *prev_end_tz;
  gboolean was_all_day;
  gboolean all_day;

  GCAL_ENTRY;

  self = GCAL_SCHEDULE_SECTION (section);
  all_day = all_day_selected (self);
  was_all_day = self->values->all_day;

  /* All day */
  if (all_day != was_all_day)
    GCAL_RETURN (TRUE);

  if (!all_day)
  {
    start_date = gcal_date_time_chooser_get_date_time (self->start_date_time_chooser);
    end_date = gcal_date_time_chooser_get_date_time (self->end_date_time_chooser);
  }
  else
  {
    start_date = gcal_date_chooser_row_get_date (self->start_date_row);
    end_date = gcal_date_chooser_row_get_date (self->end_date_row);
  }
  prev_start_date = self->values->date_start;
  prev_end_date = self->values->date_end;

  start_tz = g_date_time_get_timezone (start_date);
  end_tz = g_date_time_get_timezone (end_date);
  prev_start_tz = g_date_time_get_timezone (prev_start_date);
  prev_end_tz = g_date_time_get_timezone (prev_end_date);

  /* Start date */
  if (!g_date_time_equal (start_date, prev_start_date))
    GCAL_RETURN (TRUE);
  if (g_strcmp0 (g_time_zone_get_identifier (start_tz), g_time_zone_get_identifier (prev_start_tz)) != 0)
    GCAL_RETURN (TRUE);

  /* End date */
  if (all_day)
    {
      GDateTime *fake_end_date = g_date_time_add_days (end_date, 1);
      end_date = fake_end_date;
    }

  if (!g_date_time_equal (end_date, self->values->date_end))
    GCAL_RETURN (TRUE);
  if (g_strcmp0 (g_time_zone_get_identifier (end_tz), g_time_zone_get_identifier (prev_end_tz)) != 0)
    GCAL_RETURN (TRUE);

  /* Recurrency */
  GCAL_RETURN (gcal_schedule_section_recurrence_changed (self));
}

static void
gcal_event_editor_section_iface_init (GcalEventEditorSectionInterface *iface)
{
  iface->set_event = gcal_schedule_section_set_event;
  iface->apply = gcal_schedule_section_apply;
  iface->changed = gcal_schedule_section_changed;
}


/*
 * GObject overrides
 */

static void
gcal_schedule_section_finalize (GObject *object)
{
  GcalScheduleSection *self = (GcalScheduleSection *)object;

  g_clear_pointer (&self->values, gcal_schedule_values_free);
  g_clear_object (&self->context);
  g_clear_object (&self->event);

  G_OBJECT_CLASS (gcal_schedule_section_parent_class)->finalize (object);
}

static void
gcal_schedule_section_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  GcalScheduleSection *self = GCAL_SCHEDULE_SECTION (object);

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
gcal_schedule_section_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  GcalScheduleSection *self = GCAL_SCHEDULE_SECTION (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_assert (self->context == NULL);
      self->context = g_value_dup_object (value);
      g_signal_connect_object (self->context,
                               "notify::time-format",
                               G_CALLBACK (on_time_format_changed_cb),
                               self,
                               G_CONNECT_SWAPPED);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_schedule_section_class_init (GcalScheduleSectionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  g_type_ensure (GCAL_TYPE_DATE_TIME_CHOOSER);

  object_class->finalize = gcal_schedule_section_finalize;
  object_class->get_property = gcal_schedule_section_get_property;
  object_class->set_property = gcal_schedule_section_set_property;

  g_object_class_override_property (object_class, PROP_CONTEXT, "context");

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/event-editor/gcal-schedule-section.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalScheduleSection, schedule_type_toggle_group);
  gtk_widget_class_bind_template_child (widget_class, GcalScheduleSection, start_date_group);
  gtk_widget_class_bind_template_child (widget_class, GcalScheduleSection, start_date_row);
  gtk_widget_class_bind_template_child (widget_class, GcalScheduleSection, end_date_group);
  gtk_widget_class_bind_template_child (widget_class, GcalScheduleSection, end_date_row);
  gtk_widget_class_bind_template_child (widget_class, GcalScheduleSection, start_date_time_chooser);
  gtk_widget_class_bind_template_child (widget_class, GcalScheduleSection, end_date_time_chooser);
  gtk_widget_class_bind_template_child (widget_class, GcalScheduleSection, number_of_occurrences_spin);
  gtk_widget_class_bind_template_child (widget_class, GcalScheduleSection, repeat_combo);
  gtk_widget_class_bind_template_child (widget_class, GcalScheduleSection, repeat_duration_combo);
  gtk_widget_class_bind_template_child (widget_class, GcalScheduleSection, until_date_selector);

  gtk_widget_class_bind_template_callback (widget_class, on_start_date_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_end_date_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_start_date_time_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_end_date_time_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_repeat_duration_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_repeat_type_changed_cb);
}

static void
gcal_schedule_section_init (GcalScheduleSection *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect (self->schedule_type_toggle_group,
                    "notify::active",
                    G_CALLBACK (on_schedule_type_changed_cb),
                    self);
}

gboolean
gcal_schedule_section_recurrence_changed (GcalScheduleSection *self)
{
  g_autoptr (GcalRecurrence) recurrence = NULL;
  GcalRecurrenceFrequency freq;

  g_return_val_if_fail (GCAL_IS_SCHEDULE_SECTION (self), FALSE);

  freq = adw_combo_row_get_selected (ADW_COMBO_ROW (self->repeat_combo));
  if (freq == GCAL_RECURRENCE_NO_REPEAT && !self->values->recur)
    GCAL_RETURN (FALSE);

  recurrence = gcal_recurrence_new ();
  recurrence->frequency = adw_combo_row_get_selected (ADW_COMBO_ROW (self->repeat_combo));
  recurrence->limit_type = adw_combo_row_get_selected (ADW_COMBO_ROW (self->repeat_duration_combo));
  if (recurrence->limit_type == GCAL_RECURRENCE_UNTIL)
    recurrence->limit.until = g_date_time_ref (gcal_date_selector_get_date (GCAL_DATE_SELECTOR (self->until_date_selector)));
  else if (recurrence->limit_type == GCAL_RECURRENCE_COUNT)
    recurrence->limit.count = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (self->number_of_occurrences_spin));

  GCAL_RETURN (!gcal_recurrence_is_equal (recurrence, self->values->recur));
}

gboolean
gcal_schedule_section_day_changed (GcalScheduleSection *self)
{
  GDateTime *start_date, *end_date;
  gboolean all_day;

  g_return_val_if_fail (GCAL_IS_SCHEDULE_SECTION (self), FALSE);

  GCAL_ENTRY;

  all_day = all_day_selected (self);

   if (all_day)
    {
      start_date = gcal_date_time_chooser_get_date_time (self->start_date_time_chooser);
      end_date = gcal_date_time_chooser_get_date_time (self->end_date_time_chooser);
    }
  else
    {
      start_date = gcal_date_chooser_row_get_date (self->start_date_row);
      end_date = gcal_date_chooser_row_get_date (self->end_date_row);
    }

  GCAL_RETURN (gcal_date_time_compare_date (start_date, self->values->date_start) < 0 ||
               gcal_date_time_compare_date (end_date, self->values->date_end) > 0);
}

/**
 * Extracts the values from @event that are needed to populate #GcalScheduleSection.
 *
 * Returns: a #GcalscheduleValues ready for use.  Free it with
 * gcal_schedule_values_free().
 */
GcalScheduleValues *
gcal_schedule_values_from_event (GcalEvent      *event,
                                 GcalTimeFormat  time_format)
{
  GcalScheduleValues *values = g_new0 (GcalScheduleValues, 1);

  if (event)
    {
      GcalRecurrence *recur = gcal_event_get_recurrence (event);

      values->all_day = gcal_event_get_all_day (event);

      values->orig_date_start = g_date_time_ref (gcal_event_get_date_start (event));
      values->date_start = g_date_time_ref (values->orig_date_start);

      values->orig_date_end = g_date_time_ref (gcal_event_get_date_end (event));
      values->date_end = g_date_time_ref (values->orig_date_end);

      if (recur)
        {
          values->recur = gcal_recurrence_ref (recur);
        }
    }

  values->time_format = time_format;

  return values;
}

/**
 * Frees the contents of @values.  Does not free the @values pointer itself;
 * this structure is meant to be allocated on the stack.
 */
void
gcal_schedule_values_free (GcalScheduleValues *values)
{
  values->all_day = FALSE;

  g_clear_pointer (&values->date_start, g_date_time_unref);
  g_clear_pointer (&values->date_end, g_date_time_unref);
  g_clear_pointer (&values->recur, gcal_recurrence_unref);

  g_free (values);
}

static void
test_setting_start_date_after_end_date_resets_end_date (void)
{
  /* We start with
   *   start = 10:00
   *   end   = 11:00
   *
   * Then we set start to 12:00
   *
   * We want to test that end becomes 12:00 as well.
   */
  g_autoptr (GDateTime) date = g_date_time_new_from_iso8601 ("20250303T10:00:00-06:00", NULL);
  g_assert (date != NULL);

  g_autoptr (GDateTime) one_hour_later = g_date_time_new_from_iso8601 ("20250303T11:00:00-06:00", NULL);
  g_assert (one_hour_later != NULL);

  g_autoptr (GcalScheduleValues) values = g_new (GcalScheduleValues, 1);

  *values = (GcalScheduleValues) {
    .all_day = FALSE,
    .orig_date_start = g_date_time_ref (date),
    .orig_date_end = g_date_time_ref (one_hour_later),
    .date_start = g_date_time_ref (date),
    .date_end = g_date_time_ref (one_hour_later),
    .recur = NULL,
    .time_format = GCAL_TIME_FORMAT_24H,
  };

  g_autoptr (GDateTime) two_hours_later = g_date_time_new_from_iso8601 ("20250303T12:00:00-06:00", NULL);

  g_autoptr (GcalScheduleValues) new_values = gcal_schedule_values_set_start_date (values, two_hours_later);
  g_assert (g_date_time_equal (new_values->date_start, two_hours_later));
  g_assert (g_date_time_equal (new_values->date_end, two_hours_later));
}

static void
test_setting_end_date_before_start_date_resets_start_date (void)
{
  /* We start with
   *   start = 10:00
   *   end   = 11:00
   *
   * Then we set end to 09:00
   *
   * We want to test that start becomes 09:00 as well.
   */
  g_autoptr (GDateTime) date = g_date_time_new_from_iso8601 ("20250303T10:00:00-06:00", NULL);
  g_assert (date != NULL);

  g_autoptr (GDateTime) one_hour_later = g_date_time_new_from_iso8601 ("20250303T11:00:00-06:00", NULL);
  g_assert (one_hour_later != NULL);

  g_autoptr (GcalScheduleValues) values = g_new (GcalScheduleValues, 1);

  *values = (GcalScheduleValues) {
    .all_day = FALSE,
    .orig_date_start = g_date_time_ref (date),
    .orig_date_end = g_date_time_ref (one_hour_later),
    .date_start = g_date_time_ref (date),
    .date_end = g_date_time_ref (one_hour_later),
    .recur = NULL,
    .time_format = GCAL_TIME_FORMAT_24H,
  };

  g_autoptr (GDateTime) two_hours_earlier = g_date_time_new_from_iso8601 ("20250303T09:00:00-06:00", NULL);

  g_autoptr (GcalScheduleValues) new_values = gcal_schedule_values_set_end_date (values, two_hours_earlier);
  g_assert (g_date_time_equal (new_values->date_start, two_hours_earlier));
  g_assert (g_date_time_equal (new_values->date_end, two_hours_earlier));
}

static void
test_turning_all_day_on_displays_sensible_dates (void)
{
  g_autoptr (GDateTime) date = g_date_time_new_from_iso8601 ("20250303T12:34:56-06:00", NULL);
  g_assert (date != NULL);

  g_autoptr (GDateTime) one_hour_later = g_date_time_new_from_iso8601 ("20250303T13:34:56-06:00", NULL);
  g_assert (one_hour_later != NULL);

  g_autoptr (GcalScheduleValues) values = g_new0 (GcalScheduleValues, 1);

  *values = (GcalScheduleValues) {
    .all_day = FALSE,
    .orig_date_start = g_date_time_ref (date),
    .orig_date_end = g_date_time_ref (one_hour_later),
    .date_start = g_date_time_ref (date),
    .date_end = g_date_time_ref (one_hour_later),
    .recur = NULL,
    .time_format = GCAL_TIME_FORMAT_24H,
  };

  g_autoptr (GcalScheduleValues) new_values = gcal_schedule_values_set_all_day (values, TRUE);

  g_autoptr (WidgetState) state = widget_state_from_values (new_values);

  g_assert_true (state->schedule_type_all_day);
  g_assert_true (state->date_widgets_visible);
  g_assert_false (state->date_time_widgets_visible);

  g_assert_cmpint (g_date_time_get_year (state->date_time_start), ==, 2025);
  g_assert_cmpint (g_date_time_get_month (state->date_time_start), ==, 3);
  g_assert_cmpint (g_date_time_get_day_of_month (state->date_time_start), ==, 3);

  g_assert_cmpint (g_date_time_get_year (state->date_time_end), ==, 2025);
  g_assert_cmpint (g_date_time_get_month (state->date_time_end), ==, 3);
  /* test failure:
   *   g_assert_cmpint (g_date_time_get_day_of_month (state->date_time_end), ==, 3);
   * this is because we subtract a day from the all_day end_date
   */
}

void
gcal_schedule_section_add_tests (void)
{
  g_test_add_func ("/event_editor/schedule_section/setting_start_date_after_end_date_resets_end_date",
                   test_setting_start_date_after_end_date_resets_end_date);
  g_test_add_func ("/event_editor/schedule_section/setting_end_date_before_start_date_resets_start_date",
                   test_setting_end_date_before_start_date_resets_start_date);
  g_test_add_func ("/event_editor/schedule_section/turning_all_day_on_displays_sensible_dates",
                   test_turning_all_day_on_displays_sensible_dates);
}
