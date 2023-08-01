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
#include "gcal-debug.h"
#include "gcal-event.h"
#include "gcal-event-editor-section.h"
#include "gcal-recurrence.h"
#include "gcal-schedule-section.h"
#include "gcal-time-selector.h"
#include "gcal-utils.h"

#include <glib/gi18n.h>

struct _GcalScheduleSection
{
  GtkBox              parent;

  GtkSwitch          *all_day_switch;
  GtkWidget          *end_date_selector;
  GtkWidget          *end_time_selector;
  GtkLabel           *event_end_label;
  GtkLabel           *event_start_label;
  GtkWidget          *number_of_occurrences_spin;
  GtkWidget          *repeat_combo;
  GtkWidget          *repeat_duration_combo;
  GtkWidget          *start_date_selector;
  GtkWidget          *start_time_selector;
  GtkWidget          *until_date_selector;

  GcalContext        *context;
  GcalEvent          *event;

  GcalEventEditorFlags flags;
};

static void          gcal_event_editor_section_iface_init        (GcalEventEditorSectionInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GcalScheduleSection, gcal_schedule_section, GTK_TYPE_BOX,
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

static void
find_best_timezones_for_event (GcalScheduleSection  *self,
                               GTimeZone             **out_tz_start,
                               GTimeZone             **out_tz_end)
{
  gboolean was_all_day;
  gboolean all_day;
  gboolean new_event;

  /* Update all day */
  all_day = gtk_switch_get_active (self->all_day_switch);
  was_all_day = gcal_event_get_all_day (self->event);
  new_event = self->flags & GCAL_EVENT_EDITOR_FLAG_NEW_EVENT;

  GCAL_TRACE_MSG ("Finding best timezone with all_day=%d, was_all_day=%d, event_is_new=%d",
                  all_day,
                  was_all_day,
                  new_event);

  if (!new_event && was_all_day && !all_day)
    {
      GCAL_TRACE_MSG ("Using original event timezones");

      gcal_event_get_original_timezones (self->event, out_tz_start, out_tz_end);
      return;
    }

  if (all_day)
    {
      GCAL_TRACE_MSG ("Using UTC timezones");

      if (out_tz_start)
        *out_tz_start = g_time_zone_new_utc ();

      if (out_tz_end)
        *out_tz_end = g_time_zone_new_utc ();
    }
  else
    {
      g_autoptr (GTimeZone) tz_start = NULL;
      g_autoptr (GTimeZone) tz_end = NULL;

      if (new_event)
        {
          GCAL_TRACE_MSG ("Using the local timezone");

          tz_start = g_time_zone_new_local ();
          tz_end = g_time_zone_new_local ();
        }
      else
        {
          GCAL_TRACE_MSG ("Using the current timezones");

          tz_start = g_time_zone_ref (g_date_time_get_timezone (gcal_event_get_date_start (self->event)));
          tz_end = g_time_zone_ref (g_date_time_get_timezone (gcal_event_get_date_end (self->event)));
        }

      if (out_tz_start)
        *out_tz_start = g_steal_pointer (&tz_start);

      if (out_tz_end)
        *out_tz_end = g_steal_pointer (&tz_end);
    }
}

static GDateTime*
return_datetime_for_widgets (GcalScheduleSection *self,
                             GTimeZone           *timezone,
                             GcalDateSelector    *date_selector,
                             GcalTimeSelector    *time_selector)
{
  g_autoptr (GDateTime) date_in_local_tz = NULL;
  g_autoptr (GDateTime) date_in_best_tz = NULL;
  GDateTime *date;
  GDateTime *time;
  GDateTime *retval;
  gboolean all_day;

  all_day = gtk_switch_get_active (self->all_day_switch);
  date = gcal_date_selector_get_date (date_selector);
  time = gcal_time_selector_get_time (time_selector);

  date_in_local_tz = g_date_time_new_local (g_date_time_get_year (date),
                                            g_date_time_get_month (date),
                                            g_date_time_get_day_of_month (date),
                                            all_day ? 0 : g_date_time_get_hour (time),
                                            all_day ? 0 : g_date_time_get_minute (time),
                                            0);

  if (all_day)
    date_in_best_tz = g_date_time_ref (date_in_local_tz);
  else
    date_in_best_tz = g_date_time_to_timezone (date_in_local_tz, timezone);

  retval = g_date_time_new (timezone,
                            g_date_time_get_year (date_in_best_tz),
                            g_date_time_get_month (date_in_best_tz),
                            g_date_time_get_day_of_month (date_in_best_tz),
                            all_day ? 0 : g_date_time_get_hour (date_in_best_tz),
                            all_day ? 0 : g_date_time_get_minute (date_in_best_tz),
                            0);

  return retval;
}

static GDateTime*
get_date_start (GcalScheduleSection *self)
{
  g_autoptr (GTimeZone) start_tz = NULL;

  find_best_timezones_for_event (self, &start_tz, NULL);

  return return_datetime_for_widgets (self,
                                      start_tz,
                                      GCAL_DATE_SELECTOR (self->start_date_selector),
                                      GCAL_TIME_SELECTOR (self->start_time_selector));
}

static GDateTime*
get_date_end (GcalScheduleSection *self)
{
  g_autoptr (GTimeZone) end_tz = NULL;

  find_best_timezones_for_event (self, NULL, &end_tz);

  return return_datetime_for_widgets (self,
                                      end_tz,
                                      GCAL_DATE_SELECTOR (self->end_date_selector),
                                      GCAL_TIME_SELECTOR (self->end_time_selector));
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

static gchar*
format_datetime_for_display (GDateTime      *date,
                             GcalTimeFormat  format,
                             gboolean        all_day)
{
  g_autofree gchar *formatted_date = NULL;
  g_autoptr (GDateTime) local_dt = NULL;
  g_autoptr (GDateTime) now = NULL;
  GString *string;
  gint days_diff;

  string = g_string_new ("");

  now = g_date_time_new_now_local ();
  local_dt = all_day ? g_date_time_ref (date) : g_date_time_to_local (date);
  days_diff = gcal_date_time_compare_date (local_dt, now);

  switch (days_diff)
    {
    case -7 ... -2:
      /* Translators: %A is the weekday name (e.g. Sunday, Monday, etc) */
      formatted_date = g_date_time_format (local_dt, gcal_util_translate_time_string (_("Last %A")));
      break;

    case -1:
      formatted_date = g_strdup (_("Yesterday"));
      break;

    case 0:
      formatted_date = g_strdup (_("Today"));
      break;

    case 1:
      formatted_date = g_strdup (_("Tomorrow"));
      break;

    case 2 ... 6:
      /* Translators: %A is the weekday name (e.g. Sunday, Monday, etc) */
      formatted_date = g_date_time_format (local_dt, gcal_util_translate_time_string (_("This %A")));
      break;

    default:
      formatted_date = g_date_time_format (local_dt, "%x");
      break;
    }

  if (!all_day)
    {
      g_autofree gchar *formatted_time = NULL;

      switch (format)
        {
        case GCAL_TIME_FORMAT_12H:
          formatted_time = g_date_time_format (local_dt, "%I:%M %P");
          break;

        case GCAL_TIME_FORMAT_24H:
          formatted_time = g_date_time_format (local_dt, "%R");
          break;

        default:
          g_assert_not_reached ();
        }

      /*
       * Translators: %1$s is the formatted date (e.g. Today, Sunday, or even 2019-10-11) and %2$s is the
       * formatted time (e.g. 03:14 PM, or 21:29)
       */
      g_string_printf (string, _("%1$s, %2$s"), formatted_date, formatted_time);
    }
  else
    {
      g_string_printf (string, "%s", formatted_date);
    }

  return g_string_free (string, FALSE);
}

static void
update_date_labels (GcalScheduleSection *self)
{
  g_autofree gchar *start_label = NULL;
  g_autofree gchar *end_label = NULL;
  g_autoptr (GDateTime) start = NULL;
  g_autoptr (GDateTime) end = NULL;
  GcalTimeFormat time_format;
  gboolean all_day;

  time_format = gcal_context_get_time_format (self->context);

  all_day = gtk_switch_get_active (self->all_day_switch);
  start = get_date_start (self);
  end = get_date_end (self);
  start_label = format_datetime_for_display (start, time_format, all_day);
  end_label = format_datetime_for_display (end, time_format, all_day);

  gtk_label_set_label (self->event_start_label, start_label);
  gtk_label_set_label (self->event_end_label, end_label);
}

static void
sync_datetimes (GcalScheduleSection *self,
                GParamSpec          *pspec,
                GtkWidget           *widget)
{
  GDateTime *start, *end, *start_local, *end_local, *new_date;
  GtkWidget *date_widget, *time_widget;
  gboolean is_start, is_all_day;

  GCAL_ENTRY;

  is_start = (widget == self->start_time_selector || widget == self->start_date_selector);
  is_all_day = gtk_switch_get_active (self->all_day_switch);
  start = get_date_start (self);
  end = get_date_end (self);

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

  update_date_labels (self);

  GCAL_EXIT;
}


/*
 * Callbacks
 */

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
on_all_day_switch_active_changed_cb (GtkSwitch           *all_day_switch,
                                     GParamSpec          *pspec,
                                     GcalScheduleSection *self)
{
  gboolean active = gtk_switch_get_active (all_day_switch);

  gtk_widget_set_sensitive (self->start_time_selector, !active);
  gtk_widget_set_sensitive (self->end_time_selector, !active);

  update_date_labels (self);
}

static void
on_time_format_changed_cb (GcalScheduleSection *self)
{
  GcalTimeFormat time_format;

  time_format = gcal_context_get_time_format (self->context);

  gcal_time_selector_set_time_format (GCAL_TIME_SELECTOR (self->start_time_selector), time_format);
  gcal_time_selector_set_time_format (GCAL_TIME_SELECTOR (self->end_time_selector), time_format);
}


/*
 * GcalEventEditorSection interface
 */

static void
gcal_schedule_section_set_event (GcalEventEditorSection *section,
                                 GcalEvent              *event,
                                 GcalEventEditorFlags    flags)
{
  g_autoptr (GDateTime) date_start = NULL;
  g_autoptr (GDateTime) date_end = NULL;
  GcalRecurrenceLimitType limit_type;
  GcalRecurrenceFrequency frequency;
  GcalScheduleSection *self;
  GcalRecurrence *recur;
  gboolean all_day;

  GCAL_ENTRY;

  self = GCAL_SCHEDULE_SECTION (section);

  g_set_object (&self->event, event);
  self->flags = flags;

  if (!event)
    GCAL_RETURN ();

  /* Recurrences */
  recur = gcal_event_get_recurrence (event);
  frequency = recur ? recur->frequency : GCAL_RECURRENCE_NO_REPEAT;
  limit_type = recur ? recur->limit_type : GCAL_RECURRENCE_FOREVER;

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
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (self->number_of_occurrences_spin), recur->limit.count);
      break;

    case GCAL_RECURRENCE_UNTIL:
      gcal_date_selector_set_date (GCAL_DATE_SELECTOR (self->until_date_selector), recur->limit.until);
      break;

    case GCAL_RECURRENCE_FOREVER:
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (self->number_of_occurrences_spin), 0);
      break;
    }

  all_day = gcal_event_get_all_day (event);

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
  g_signal_handlers_block_by_func (self->end_date_selector, sync_datetimes, self);
  g_signal_handlers_block_by_func (self->start_date_selector, sync_datetimes, self);

  gcal_date_selector_set_date (GCAL_DATE_SELECTOR (self->start_date_selector), date_start);
  gcal_date_selector_set_date (GCAL_DATE_SELECTOR (self->end_date_selector), date_end);

  g_signal_handlers_unblock_by_func (self->start_date_selector, sync_datetimes, self);
  g_signal_handlers_unblock_by_func (self->end_date_selector, sync_datetimes, self);

  /* time */
  g_signal_handlers_block_by_func (self->end_time_selector, sync_datetimes, self);
  g_signal_handlers_block_by_func (self->start_time_selector, sync_datetimes, self);

  gcal_time_selector_set_time (GCAL_TIME_SELECTOR (self->start_time_selector), date_start);
  gcal_time_selector_set_time (GCAL_TIME_SELECTOR (self->end_time_selector), date_end);

  g_signal_handlers_unblock_by_func (self->start_time_selector, sync_datetimes, self);
  g_signal_handlers_unblock_by_func (self->end_time_selector, sync_datetimes, self);

  /* all_day  */
  gtk_switch_set_active (self->all_day_switch, all_day);

  update_date_labels (self);

  GCAL_EXIT;
}

static void
gcal_schedule_section_apply (GcalEventEditorSection *section)
{
  g_autoptr (GDateTime) start_date = NULL;
  g_autoptr (GDateTime) end_date = NULL;
  GcalRecurrenceFrequency freq;
  GcalScheduleSection *self;
  GcalRecurrence *old_recur;
  gboolean was_all_day;
  gboolean all_day;

  GCAL_ENTRY;

  self = GCAL_SCHEDULE_SECTION (section);
  all_day = gtk_switch_get_active (self->all_day_switch);
  was_all_day = gcal_event_get_all_day (self->event);

  if (!was_all_day && all_day)
    gcal_event_save_original_timezones (self->event);

  /*
   * Update start & end dates. The dates are already translated to the current
   * timezone (unless the event used to be all day, but no longer is).
   */
  start_date = get_date_start (self);
  end_date = get_date_end (self);

#ifdef GCAL_ENABLE_TRACE
    {
      g_autofree gchar *start_dt_string = g_date_time_format (start_date, "%x %X %z");
      g_autofree gchar *end_dt_string = g_date_time_format (end_date, "%x %X %z");

      g_debug ("New start date: %s", start_dt_string);
      g_debug ("New end date: %s", end_dt_string);
    }
#endif

  gcal_event_set_all_day (self->event, all_day);

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

  gcal_event_set_date_start (self->event, start_date);
  gcal_event_set_date_end (self->event, end_date);

  /* Check Repeat popover and set recurrence-rules accordingly */
  old_recur = gcal_event_get_recurrence (self->event);
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
      if (!gcal_recurrence_is_equal (old_recur, recur))
        {
          /* Remove the previous recurrence... */
          remove_recurrence_properties (self->event);

          /* ... and set the new one */
          gcal_event_set_recurrence (self->event, recur);
        }
    }
  else
    {
      /* When NO_REPEAT is set, make sure to remove the old recurrent */
      remove_recurrence_properties (self->event);
    }

  GCAL_EXIT;
}

static gboolean
gcal_schedule_section_changed (GcalEventEditorSection *section)
{
  g_autoptr (GDateTime) start_date = NULL;
  g_autoptr (GDateTime) end_date = NULL;
  GcalScheduleSection *self;
  gboolean was_all_day;
  gboolean all_day;

  GCAL_ENTRY;

  self = GCAL_SCHEDULE_SECTION (section);
  all_day = gtk_switch_get_active (self->all_day_switch);
  was_all_day = gcal_event_get_all_day (self->event);

  /* All day */
  if (all_day != was_all_day)
    GCAL_RETURN (TRUE);

  /* Start date */
  start_date = get_date_start (self);
  if (!g_date_time_equal (start_date, gcal_event_get_date_start (self->event)))
    GCAL_RETURN (TRUE);

  /* End date */
  end_date = get_date_end (self);
  if (gtk_switch_get_active (self->all_day_switch))
    {
      g_autoptr (GDateTime) fake_end_date = g_date_time_add_days (end_date, 1);
      gcal_set_date_time (&end_date, fake_end_date);
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

  if (!g_date_time_equal (end_date, gcal_event_get_date_end (self->event)))
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
      on_time_format_changed_cb (self);
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

  object_class->finalize = gcal_schedule_section_finalize;
  object_class->get_property = gcal_schedule_section_get_property;
  object_class->set_property = gcal_schedule_section_set_property;

  g_object_class_override_property (object_class, PROP_CONTEXT, "context");

  g_type_ensure (GCAL_TYPE_DATE_SELECTOR);
  g_type_ensure (GCAL_TYPE_TIME_SELECTOR);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/event-editor/gcal-schedule-section.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalScheduleSection, all_day_switch);
  gtk_widget_class_bind_template_child (widget_class, GcalScheduleSection, start_time_selector);
  gtk_widget_class_bind_template_child (widget_class, GcalScheduleSection, start_date_selector);
  gtk_widget_class_bind_template_child (widget_class, GcalScheduleSection, end_time_selector);
  gtk_widget_class_bind_template_child (widget_class, GcalScheduleSection, end_date_selector);
  gtk_widget_class_bind_template_child (widget_class, GcalScheduleSection, event_start_label);
  gtk_widget_class_bind_template_child (widget_class, GcalScheduleSection, event_end_label);
  gtk_widget_class_bind_template_child (widget_class, GcalScheduleSection, number_of_occurrences_spin);
  gtk_widget_class_bind_template_child (widget_class, GcalScheduleSection, repeat_combo);
  gtk_widget_class_bind_template_child (widget_class, GcalScheduleSection, repeat_duration_combo);
  gtk_widget_class_bind_template_child (widget_class, GcalScheduleSection, until_date_selector);

  gtk_widget_class_bind_template_callback (widget_class, on_all_day_switch_active_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_repeat_duration_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_repeat_type_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, sync_datetimes);
}

static void
gcal_schedule_section_init (GcalScheduleSection *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

gboolean
gcal_schedule_section_recurrence_changed (GcalScheduleSection *self)
{
  g_autoptr (GcalRecurrence) recurrence = NULL;
  GcalRecurrenceFrequency freq;

  g_return_val_if_fail (GCAL_IS_SCHEDULE_SECTION (self), FALSE);

  freq = adw_combo_row_get_selected (ADW_COMBO_ROW (self->repeat_combo));
  if (freq == GCAL_RECURRENCE_NO_REPEAT && !gcal_event_get_recurrence (self->event))
    GCAL_RETURN (FALSE);

  recurrence = gcal_recurrence_new ();
  recurrence->frequency = adw_combo_row_get_selected (ADW_COMBO_ROW (self->repeat_combo));
  recurrence->limit_type = adw_combo_row_get_selected (ADW_COMBO_ROW (self->repeat_duration_combo));
  if (recurrence->limit_type == GCAL_RECURRENCE_UNTIL)
    recurrence->limit.until = g_date_time_ref (gcal_date_selector_get_date (GCAL_DATE_SELECTOR (self->until_date_selector)));
  else if (recurrence->limit_type == GCAL_RECURRENCE_COUNT)
    recurrence->limit.count = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (self->number_of_occurrences_spin));

  GCAL_RETURN (!gcal_recurrence_is_equal (recurrence, gcal_event_get_recurrence (self->event)));
}

gboolean
gcal_schedule_section_day_changed (GcalScheduleSection *self)
{
  g_autoptr (GDateTime) start_date = NULL;
  g_autoptr (GDateTime) end_date = NULL;

  g_return_val_if_fail (GCAL_IS_SCHEDULE_SECTION (self), FALSE);

  GCAL_ENTRY;

  start_date = get_date_start (self);
  end_date = get_date_end (self);

  GCAL_RETURN (gcal_date_time_compare_date (start_date, gcal_event_get_date_start (self->event)) < 0 ||
               gcal_date_time_compare_date (end_date, gcal_event_get_date_end (self->event)) > 0);
}
