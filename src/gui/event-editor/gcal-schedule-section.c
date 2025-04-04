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


/*
 * Callbacks
 */

static void
on_schedule_type_changed_cb (GtkWidget           *widget,
                             GParamSpec          *pspec,
                             GcalScheduleSection *self)
{
  gboolean all_day = all_day_selected (self);

  gtk_widget_set_visible (GTK_WIDGET (self->start_date_group), all_day);
  gtk_widget_set_visible (GTK_WIDGET (self->end_date_group), all_day);
  gtk_widget_set_visible (GTK_WIDGET (self->start_date_time_chooser), !all_day);
  gtk_widget_set_visible (GTK_WIDGET (self->end_date_time_chooser), !all_day);

  block_date_signals (self);

  if (all_day)
    {
      g_autoptr (GDateTime) start_local = NULL;
      g_autoptr (GDateTime) end_local = NULL;

      GDateTime *start = gcal_date_time_chooser_get_date_time (self->start_date_time_chooser);
      GDateTime *end = gcal_date_time_chooser_get_date_time (self->end_date_time_chooser);

      start_local = g_date_time_to_local (start);
      end_local = g_date_time_to_local (end);

      gcal_date_chooser_row_set_date (self->start_date_row, start_local);
      gcal_date_chooser_row_set_date (self->end_date_row, end_local);
      gcal_date_time_chooser_set_date_time (self->start_date_time_chooser, start_local);
      gcal_date_time_chooser_set_date_time (self->end_date_time_chooser, end_local);
    }

  unblock_date_signals (self);
}

static void
on_start_date_changed_cb (GtkWidget           *widget,
                          GParamSpec          *pspec,
                          GcalScheduleSection *self)
{
  GDateTime *start, *end;

  GCAL_ENTRY;

  block_date_signals (self);

  start = gcal_date_chooser_row_get_date (self->start_date_row);
  end = gcal_date_chooser_row_get_date (self->end_date_row);

  if (g_date_time_compare (start, end) == 1)
    {
      gcal_date_chooser_row_set_date (self->end_date_row, start);
      gcal_date_time_chooser_set_date (self->end_date_time_chooser, start);
    }

  // Keep the date row and the date-time chooser in sync
  gcal_date_time_chooser_set_date (self->start_date_time_chooser, start);

  unblock_date_signals (self);

  GCAL_EXIT;
}

static void
on_end_date_changed_cb (GtkWidget           *widget,
                        GParamSpec          *pspec,
                        GcalScheduleSection *self)
{
  GDateTime *start, *end;

  GCAL_ENTRY;

  block_date_signals (self);

  start = gcal_date_chooser_row_get_date (self->start_date_row);
  end = gcal_date_chooser_row_get_date (self->end_date_row);

  if (g_date_time_compare (start, end) == 1)
    {
      gcal_date_chooser_row_set_date (self->start_date_row, end);
      gcal_date_time_chooser_set_date (self->start_date_time_chooser, end);
    }

  // Keep the date row and the date-time chooser in sync
  gcal_date_time_chooser_set_date (self->end_date_time_chooser, end);

  unblock_date_signals (self);

  GCAL_EXIT;
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

  gcal_date_time_chooser_set_time_format (self->start_date_time_chooser, time_format);
  gcal_date_time_chooser_set_time_format (self->end_date_time_chooser, time_format);
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
  g_autoptr (GDateTime) date_time_start_utc = NULL;
  g_autoptr (GDateTime) date_time_end_utc = NULL;
  g_autoptr (GDateTime) date_time_start_local = NULL;
  g_autoptr (GDateTime) date_time_end_local = NULL;
  GcalScheduleSection *self;
  GDateTime* date_time_start = NULL;
  GDateTime* date_time_end = NULL;
  GcalRecurrenceLimitType limit_type;
  GcalRecurrenceFrequency frequency;
  GcalRecurrence *recur;
  gboolean all_day, new_event;

  GCAL_ENTRY;

  self = GCAL_SCHEDULE_SECTION (section);

  g_set_object (&self->event, event);
  self->flags = flags;

  if (!event)
    GCAL_RETURN ();

  all_day = gcal_event_get_all_day (event);
  new_event = flags & GCAL_EVENT_EDITOR_FLAG_NEW_EVENT;

  /* schedule type  */
  adw_toggle_group_set_active_name (self->schedule_type_toggle_group,
                                    all_day ? "all-day" : "time-slot");

  gtk_widget_set_visible (GTK_WIDGET (self->start_date_group), all_day);
  gtk_widget_set_visible (GTK_WIDGET (self->end_date_group), all_day);
  gtk_widget_set_visible (GTK_WIDGET (self->start_date_time_chooser), !all_day);
  gtk_widget_set_visible (GTK_WIDGET (self->end_date_time_chooser), !all_day);

  /* retrieve start and end date-times */
  date_time_start = gcal_event_get_date_start (event);
  date_time_end = gcal_event_get_date_end (event);
  /*
   * This is subtracting what has been added in action_button_clicked ().
   * See bug 769300.
   */
  date_time_end = all_day ? g_date_time_add_days (date_time_end, -1) : date_time_end;

  date_time_start_utc = g_date_time_new_utc (g_date_time_get_year (date_time_start),
                                             g_date_time_get_month (date_time_start),
                                             g_date_time_get_day_of_month (date_time_start),
                                             0, 0, 0);

  date_time_end_utc = g_date_time_new_utc (g_date_time_get_year (date_time_end),
                                           g_date_time_get_month (date_time_end),
                                           g_date_time_get_day_of_month (date_time_end),
                                           0, 0, 0);

  date_time_start_local = g_date_time_new_local (g_date_time_get_year (date_time_start),
                                                 g_date_time_get_month (date_time_start),
                                                 g_date_time_get_day_of_month (date_time_start),
                                                 g_date_time_get_hour (date_time_start),
                                                 g_date_time_get_minute (date_time_start),
                                                 0);

  date_time_end_local = g_date_time_new_local (g_date_time_get_year (date_time_end),
                                               g_date_time_get_month (date_time_end),
                                               g_date_time_get_day_of_month (date_time_end),
                                               g_date_time_get_hour (date_time_end),
                                               g_date_time_get_minute (date_time_end),
                                               0);

  block_date_signals (self);

  /* date */
  gcal_date_chooser_row_set_date (self->start_date_row, date_time_start_utc);
  gcal_date_chooser_row_set_date (self->end_date_row, date_time_end_utc);

  /* date-time */
  if (new_event || all_day)
    {
      gcal_date_time_chooser_set_date_time (self->start_date_time_chooser, date_time_start_local);
      gcal_date_time_chooser_set_date_time (self->end_date_time_chooser, date_time_end_local);
    }
  else
    {
      gcal_date_time_chooser_set_date_time (self->start_date_time_chooser, date_time_start);
      gcal_date_time_chooser_set_date_time (self->end_date_time_chooser, date_time_end);
    }

  unblock_date_signals (self);

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

  GCAL_EXIT;
}

static void
gcal_schedule_section_apply (GcalEventEditorSection *section)
{
  GDateTime *start_date, *end_date;
  GcalRecurrenceFrequency freq;
  GcalScheduleSection *self;
  GcalRecurrence *old_recur;
  gboolean all_day;

  GCAL_ENTRY;

  self = GCAL_SCHEDULE_SECTION (section);

  all_day = all_day_selected (self);

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

      end_date = fake_end_date;
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
  GcalScheduleSection *self;
  GDateTime *start_date, *end_date, *prev_start_date, *prev_end_date;
  GTimeZone *start_tz, *end_tz, *prev_start_tz, *prev_end_tz;
  gboolean was_all_day;
  gboolean all_day;

  GCAL_ENTRY;

  self = GCAL_SCHEDULE_SECTION (section);
  all_day = all_day_selected (self);
  was_all_day = gcal_event_get_all_day (self->event);

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
  prev_start_date = gcal_event_get_date_start (self->event);
  prev_end_date = gcal_event_get_date_end (self->event);

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

  if (!g_date_time_equal (end_date, gcal_event_get_date_end (self->event)))
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

  GCAL_RETURN (gcal_date_time_compare_date (start_date, gcal_event_get_date_start (self->event)) < 0 ||
               gcal_date_time_compare_date (end_date, gcal_event_get_date_end (self->event)) > 0);
}
