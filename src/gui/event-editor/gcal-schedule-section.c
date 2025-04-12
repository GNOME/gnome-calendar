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
#include "gcal-event-schedule.h"
#include "gcal-recurrence.h"
#include "gcal-schedule-section.h"
#include "gcal-utils.h"

#include <glib/gi18n.h>

struct _GcalScheduleSection
{
  GtkBox               parent;

  GcalEventSchedule *values;

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

/* Desired state of the widgets, computed from GcalEventSchedule.
 *
 * There is some subtle logic to decide how to update the widgets based on how
 * each of them causes the GcalEventSchedule to be modified.  We encapsulate
 * the desired state in this struct, so we can have tests for it.
 */
typedef struct
{
  gboolean schedule_type_all_day;
  gboolean date_widgets_visible;
  gboolean date_time_widgets_visible;

  GcalTimeFormat time_format;

  /* Note that the displayed date/time may not correspond to the real data in the
   * GcalEventSchedule.  See the comment in widget_state_from_values().
   */
  GDateTime *date_time_start;
  GDateTime *date_time_end;

  struct {
    gboolean duration_combo_visible;
    gboolean number_of_occurrences_visible;
    gboolean until_date_visible;
    GcalRecurrenceFrequency frequency;
    GcalRecurrenceLimitType limit_type;
    guint limit_count;
    GDateTime *limit_until;
  } recurrence;
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

static void          on_number_of_occurrences_changed_cb         (GcalScheduleSection *self);
static void          on_until_date_changed_cb                    (GcalScheduleSection *self);

static WidgetState *
widget_state_from_values (const GcalEventSchedule *values)
{
  WidgetState *state = g_new0 (WidgetState, 1);

  state->schedule_type_all_day = values->curr.all_day;
  state->date_widgets_visible = values->curr.all_day;
  state->date_time_widgets_visible = !values->curr.all_day;
  state->time_format = values->time_format;

  if (values->curr.date_start && values->curr.date_end)
    {
      state->date_time_start = g_date_time_ref (values->curr.date_start);

      if (values->curr.all_day)
        {
          /* While in iCalendar a single-day all-day event goes from $day to $day+1, we don't
           * want to show the user a single-day all-day event as starting on Feb 1 and ending
           * on Feb 2, for example.
           *
           * So, we maintain the "real" data in GcalEventSchedule with the $day+1 as iCalendar wants,
           * but for display purposes, we subtract a day from the end date to show the expected thing to the
           * user.  When the widget changes, we add the day back - see gcal_schedule_values_set_end_date().
           *
           * See https://bugzilla.gnome.org/show_bug.cgi?id=769300 for the original bug about this.
           *
           * Keep in sync with gcal_schedule_values_set_end_date().
           */
          state->date_time_end = g_date_time_add_days (values->curr.date_end, -1);
        }
      else
        {
          state->date_time_end = g_date_time_ref (values->curr.date_end);
        }
    }
  else
    {
      /* The event editor can be instantiated but not shown yet, and we may get a
       * notification that the time_format changed.  In that case, the event will not be
       * set yet, and so the dates will not be set either - hence the test above for
       * date_start and date_end not being NULL.  Handle that by setting NULL dates for
       * the widgets.
       */
      state->date_time_start = NULL;
      state->date_time_end = NULL;
    }

  if (values->curr.recur && values->curr.recur->frequency != GCAL_RECURRENCE_NO_REPEAT)
    {
      state->recurrence.duration_combo_visible = TRUE;
      state->recurrence.frequency = values->curr.recur->frequency;
      state->recurrence.limit_type = values->curr.recur->limit_type;

      switch (values->curr.recur->limit_type)
        {
        case GCAL_RECURRENCE_COUNT:
          state->recurrence.limit_count = values->curr.recur->limit.count;
          state->recurrence.number_of_occurrences_visible = TRUE;
          break;

        case GCAL_RECURRENCE_UNTIL:
          state->recurrence.until_date_visible = TRUE;
          state->recurrence.limit_until = g_date_time_ref (values->curr.recur->limit.until);
          break;

        case GCAL_RECURRENCE_FOREVER:
          state->recurrence.limit_count = 0;
          break;

        default:
          g_assert_not_reached ();
        }
    }
  else
    {
      state->recurrence.duration_combo_visible = FALSE;
      state->recurrence.number_of_occurrences_visible = FALSE;
      state->recurrence.until_date_visible = FALSE;
      state->recurrence.frequency = GCAL_RECURRENCE_NO_REPEAT;
      state->recurrence.limit_type = GCAL_RECURRENCE_FOREVER;
      state->recurrence.limit_count = 0;
      state->recurrence.limit_until = NULL;
    }

  return state;
}

static void
widget_state_free (WidgetState *state)
{
  g_date_time_unref (state->date_time_start);
  g_date_time_unref (state->date_time_end);

  if (state->recurrence.limit_until)
    {
      g_date_time_unref (state->recurrence.limit_until);
    }

  g_free (state);
}

/*
 * Auxiliary methods
 */

static gboolean
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
  g_signal_handlers_block_by_func (self->number_of_occurrences_spin, on_number_of_occurrences_changed_cb, self);
  g_signal_handlers_block_by_func (self->until_date_selector, on_until_date_changed_cb, self);
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
  if (state->date_time_start)
    {
      gcal_date_chooser_row_set_date (self->start_date_row, state->date_time_start);
      gcal_date_time_chooser_set_date_time (self->start_date_time_chooser, state->date_time_start);
    }

  if (state->date_time_end)
    {
      gcal_date_chooser_row_set_date (self->end_date_row, state->date_time_end);
      gcal_date_time_chooser_set_date_time (self->end_date_time_chooser, state->date_time_end);
    }

  /* Recurrences */
  gtk_widget_set_visible (self->repeat_duration_combo, state->recurrence.duration_combo_visible);

  adw_combo_row_set_selected (ADW_COMBO_ROW (self->repeat_combo), state->recurrence.frequency);
  adw_combo_row_set_selected (ADW_COMBO_ROW (self->repeat_duration_combo), state->recurrence.limit_type);

  gtk_widget_set_visible (self->until_date_selector, state->recurrence.until_date_visible);
  if (state->recurrence.until_date_visible)
    {
      gcal_date_selector_set_date (GCAL_DATE_SELECTOR (self->until_date_selector), state->recurrence.limit_until);
    }

  gtk_widget_set_visible (self->number_of_occurrences_spin, state->recurrence.number_of_occurrences_visible);
  if (state->recurrence.number_of_occurrences_visible)
    {
      g_print ("update widgets: limit_count = %u\n", state->recurrence.limit_count);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (self->number_of_occurrences_spin), state->recurrence.limit_count);
    }

  unblock_date_signals (self);
  g_signal_handlers_unblock_by_func (self->until_date_selector, on_until_date_changed_cb, self);
  g_signal_handlers_unblock_by_func (self->number_of_occurrences_spin, on_number_of_occurrences_changed_cb, self);
  g_signal_handlers_unblock_by_func (self->schedule_type_toggle_group, on_schedule_type_changed_cb, self);
}

/* Recomputes and sets the widget state.  Assumes self->values has already been updated. */
static void
refresh (GcalScheduleSection *self)
{
  g_autoptr (WidgetState) state = widget_state_from_values (self->values);
  update_widgets (self, state);
}

/* Updates the widgets, and the current values, from the specified ones.
 *
 * This will free the current values and replace them with new ones.
 */
static void
update_from_event_schedule (GcalScheduleSection *self,
                            GcalEventSchedule *values)
{
  gcal_event_schedule_free (self->values);
  self->values = values;

  refresh (self);
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
  GcalEventSchedule *updated = gcal_event_schedule_set_all_day (self->values, all_day);
  update_from_event_schedule (self, updated);
}

static void
on_start_date_changed_cb (GtkWidget           *widget,
                          GParamSpec          *pspec,
                          GcalScheduleSection *self)
{
  GDateTime *start = gcal_date_chooser_row_get_date (self->start_date_row);
  GcalEventSchedule *updated = gcal_event_schedule_set_start_date (self->values, start);
  update_from_event_schedule (self, updated);
}

static void
on_end_date_changed_cb (GtkWidget           *widget,
                        GParamSpec          *pspec,
                        GcalScheduleSection *self)
{
  GDateTime *end = gcal_date_chooser_row_get_date (self->end_date_row);
  GcalEventSchedule *updated = gcal_event_schedule_set_end_date (self->values, end);
  update_from_event_schedule (self, updated);
}

static void
on_start_date_time_changed_cb (GtkWidget           *widget,
                               GParamSpec          *pspec,
                               GcalScheduleSection *self)
{
  GDateTime *start = gcal_date_time_chooser_get_date_time (self->start_date_time_chooser);
  GcalEventSchedule *updated = gcal_event_schedule_set_start_date_time (self->values, start);
  update_from_event_schedule (self, updated);
}

static void
on_end_date_time_changed_cb (GtkWidget           *widget,
                             GParamSpec          *pspec,
                             GcalScheduleSection *self)
{
  GDateTime *end = gcal_date_time_chooser_get_date_time (self->end_date_time_chooser);
  GcalEventSchedule *updated = gcal_event_schedule_set_end_date_time (self->values, end);
  update_from_event_schedule (self, updated);
}

static void
on_number_of_occurrences_changed_cb (GcalScheduleSection *self)
{
  guint count = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (self->number_of_occurrences_spin));
  GcalEventSchedule *updated = gcal_event_schedule_set_recurrence_count (self->values, count);
  update_from_event_schedule (self, updated);
}

static void
on_until_date_changed_cb (GcalScheduleSection *self)
{
  GDateTime *until = gcal_date_selector_get_date (GCAL_DATE_SELECTOR (self->until_date_selector));
  GcalEventSchedule *updated = gcal_event_schedule_set_recurrence_until (self->values, until);
  update_from_event_schedule (self, updated);
}

static GcalRecurrenceLimitType
get_recurence_limit_type (GcalScheduleSection *self)
{
  guint item = adw_combo_row_get_selected (ADW_COMBO_ROW (self->repeat_duration_combo));

  g_assert (item >= GCAL_RECURRENCE_FOREVER && item <= GCAL_RECURRENCE_UNTIL);

  return (GcalRecurrenceLimitType) item;
}

static void
on_repeat_duration_changed_cb (GtkWidget           *widget,
                               GParamSpec          *pspec,
                               GcalScheduleSection *self)
{
  GcalRecurrenceLimitType limit_type = get_recurence_limit_type (self);
  GcalEventSchedule *updated = gcal_event_schedule_set_recur_limit_type (self->values, limit_type);
  update_from_event_schedule (self, updated);
}

static GcalRecurrenceFrequency
get_recurrence_frequency (GcalScheduleSection *self)
{
  guint item = adw_combo_row_get_selected (ADW_COMBO_ROW (self->repeat_combo));

  g_assert (item >= GCAL_RECURRENCE_NO_REPEAT && item <= GCAL_RECURRENCE_OTHER);

  return (GcalRecurrenceFrequency) item;
}

static void
on_repeat_type_changed_cb (GtkWidget           *widget,
                           GParamSpec          *pspec,
                           GcalScheduleSection *self)
{
  GcalRecurrenceFrequency frequency = get_recurrence_frequency (self);
  GcalEventSchedule *updated = gcal_event_schedule_set_recur_frequency (self->values, frequency);
  update_from_event_schedule (self, updated);
}

static void
on_time_format_changed_cb (GcalScheduleSection *self)
{
  if (self->event)
    {
      /* Apparently we can be notified that the time format changed, even when
       * there has not been an event set yet.  This breaks the code downstream.
       */
      GcalTimeFormat time_format = gcal_context_get_time_format (self->context);

      GcalEventSchedule *updated = gcal_event_schedule_set_time_format (self->values, time_format);

      update_from_event_schedule (self, updated);
    }
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
  GcalTimeFormat time_format;

  GCAL_ENTRY;

  self = GCAL_SCHEDULE_SECTION (section);

  g_set_object (&self->event, event);
  self->flags = flags;

  time_format = gcal_context_get_time_format (self->context);
  self->values = gcal_event_schedule_from_event (event, time_format);

  if (!event)
    GCAL_RETURN ();

  refresh (self);

  GCAL_EXIT;
}

static void
gcal_schedule_section_apply_to_event (GcalScheduleSection *self,
                                      GcalEvent           *event)
{
  GCAL_ENTRY;

  gcal_event_set_all_day (event, self->values->curr.all_day);
  gcal_event_set_date_start (event, self->values->curr.date_start);
  gcal_event_set_date_end (event, self->values->curr.date_end);

  /* Only apply the new recurrence if it's different from the old one.
   * We don't unconditionally set the recurrence since remove_recurrence_properties()
   * will actually remove all the rrules, not just *a* unique one.
   *
   * That is, here we allow for future support for more than one rrule
   * in the event, given the current capabilities of gnome-calendar.
   *
   */
  if (!gcal_recurrence_is_equal (self->values->curr.recur, gcal_event_get_recurrence (event)))
    {
      remove_recurrence_properties (event);

      if (self->values->curr.recur)
        {
          gcal_event_set_recurrence (event, self->values->curr.recur);
        }
    }

  GCAL_EXIT;
}

static void
gcal_schedule_section_apply (GcalEventEditorSection *section)
{
  GcalScheduleSection *self = GCAL_SCHEDULE_SECTION (section);

  GCAL_ENTRY;

  gcal_schedule_section_apply_to_event (self, self->event);

  gcal_event_schedule_free (self->values);

  GcalTimeFormat time_format = gcal_context_get_time_format (self->context);
  self->values = gcal_event_schedule_from_event (self->event, time_format);

  GCAL_EXIT;
}

static gboolean
recurrence_changed (const GcalEventSchedule *values)
{
  return !gcal_recurrence_is_equal (values->orig.recur, values->curr.recur);
}

static gboolean
day_changed (const GcalEventSchedule *values)
{
  return (gcal_date_time_compare_date (values->curr.date_start, values->orig.date_start) < 0 ||
          gcal_date_time_compare_date (values->curr.date_end, values->orig.date_end) > 0);
}

static gboolean
values_changed (const GcalEventSchedule *values)
{
  const GcalScheduleValues *orig = &values->orig;
  const GcalScheduleValues *curr = &values->curr;

  if (orig->all_day != curr->all_day)
    return TRUE;

  GTimeZone *orig_start_tz, *orig_end_tz, *start_tz, *end_tz;
  orig_start_tz = g_date_time_get_timezone (orig->date_start);
  orig_end_tz = g_date_time_get_timezone (orig->date_end);
  start_tz = g_date_time_get_timezone (curr->date_start);
  end_tz = g_date_time_get_timezone (curr->date_end);

  if (!g_date_time_equal (orig->date_start, curr->date_start))
    return TRUE;

  if (g_strcmp0 (g_time_zone_get_identifier (start_tz), g_time_zone_get_identifier (orig_start_tz)) != 0)
    return TRUE;

  if (!g_date_time_equal (orig->date_end, curr->date_end))
    return TRUE;

  if (g_strcmp0 (g_time_zone_get_identifier (end_tz), g_time_zone_get_identifier (orig_end_tz)) != 0)
    return TRUE;

  return recurrence_changed (values);
}

static gboolean
gcal_schedule_section_changed (GcalEventEditorSection *section)
{
  GcalScheduleSection *self = GCAL_SCHEDULE_SECTION (section);

  GCAL_ENTRY;

  GCAL_RETURN (values_changed (self->values));
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

  g_clear_pointer (&self->values, gcal_event_schedule_free);
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
  gtk_widget_class_bind_template_callback (widget_class, on_number_of_occurrences_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_until_date_changed_cb);
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
  g_return_val_if_fail (GCAL_IS_SCHEDULE_SECTION (self), FALSE);

  return recurrence_changed (self->values);
}

gboolean
gcal_schedule_section_day_changed (GcalScheduleSection *self)
{
  g_return_val_if_fail (GCAL_IS_SCHEDULE_SECTION (self), FALSE);

  return day_changed (self->values);
}

static void
test_all_day_displays_sensible_dates_and_roundtrips (void)
{
  /* Start with this, per iCalendar's convention for a single-day all-day event:
   *
   *   start = midnight
   *   end = next midnight
   *   all_day = true
   *
   * Then, pass that on to the widgets, and check that they display the same date for start/end,
   * since users see "all day, start=2025/03/03, end=2025/03/03" and think sure, a single all-day.
   *
   * Then, change the end date to one day later, to get a two-day event.
   *
   * Check that the values gets back end = (start + 2 days)
   */

  g_autoptr (GcalEventSchedule) values = gcal_event_schedule_with_date_times ("20250303T00:00:00-06:00",
                                                                              "20250304T00:00:00-06:00",
                                                                              TRUE);

  /* Compute the widget state from the values; check the displayed dates (should be the same) */

  g_autoptr (WidgetState) state = widget_state_from_values (values);

  g_assert_true (state->schedule_type_all_day);
  g_assert_true (state->date_widgets_visible);
  g_assert_false (state->date_time_widgets_visible);

  g_assert_cmpint (g_date_time_get_year (state->date_time_start), ==, 2025);
  g_assert_cmpint (g_date_time_get_month (state->date_time_start), ==, 3);
  g_assert_cmpint (g_date_time_get_day_of_month (state->date_time_start), ==, 3);

  g_assert_cmpint (g_date_time_get_year (state->date_time_end), ==, 2025);
  g_assert_cmpint (g_date_time_get_month (state->date_time_end), ==, 3);
  g_assert_cmpint (g_date_time_get_day_of_month (state->date_time_end), ==, 3);

  /* Add a day to the end date */

  g_autoptr (GDateTime) displayed_end_date_plus_one_day = g_date_time_new_from_iso8601 ("20250304T00:00:00-06:00", NULL);
  g_assert (displayed_end_date_plus_one_day != NULL);
  g_autoptr (GcalEventSchedule) new_values = gcal_event_schedule_set_end_date (values, displayed_end_date_plus_one_day);

  /* Check the real iCalendar value for the new date, should be different from the displayed value */

  g_assert_cmpint (g_date_time_get_day_of_month (new_values->curr.date_end), ==, 5);

  /* Roundtrip back to widgets and check the displayed value */

  g_autoptr (WidgetState) new_state = widget_state_from_values (new_values);
  g_assert_cmpint (g_date_time_get_day_of_month (new_state->date_time_end), ==, 4);
}

static void
test_turning_on_recurrence_count_turns_on_its_widget (void)
{
  /* Start with some configuration */
  g_autoptr (GcalEventSchedule) values = gcal_event_schedule_with_date_times("20250411T10:00:00-06:00",
                                                                             "20250411T11:30:00-06:00",
                                                                             FALSE);

  /* Turn on recurrence */
  g_autoptr (GcalEventSchedule) with_recur =
    gcal_event_schedule_set_recur_frequency (values, GCAL_RECURRENCE_DAILY);

  /* Set it to "count" */
  g_autoptr (GcalEventSchedule) with_count =
    gcal_event_schedule_set_recur_limit_type (with_recur, GCAL_RECURRENCE_COUNT);

  g_autoptr (WidgetState) state = widget_state_from_values (with_count);
  g_assert_true (state->recurrence.duration_combo_visible);
  g_assert_true (state->recurrence.number_of_occurrences_visible);
  g_assert_false (state->recurrence.until_date_visible);
}

static void
test_turning_on_recurrence_until_turns_on_its_widget (void)
{
  /* Start with some configuration */
  g_autoptr (GcalEventSchedule) values = gcal_event_schedule_with_date_times("20250411T10:00:00-06:00",
                                                                             "20250411T11:30:00-06:00",
                                                                             FALSE);

  /* Turn on recurrence */
  g_autoptr (GcalEventSchedule) with_recur =
    gcal_event_schedule_set_recur_frequency (values, GCAL_RECURRENCE_DAILY);

  /* Set it to "count" */
  g_autoptr (GcalEventSchedule) with_until =
    gcal_event_schedule_set_recur_limit_type (with_recur, GCAL_RECURRENCE_UNTIL);

  g_autoptr (WidgetState) state = widget_state_from_values (with_until);
  g_assert_true (state->recurrence.duration_combo_visible);
  g_assert_false (state->recurrence.number_of_occurrences_visible);
  g_assert_true (state->recurrence.until_date_visible);
}

void
gcal_schedule_section_add_tests (void)
{
  g_test_add_func ("/event_editor/schedule_section/all_day_displays_sensible_dates_and_roundtrips",
                   test_all_day_displays_sensible_dates_and_roundtrips);
  g_test_add_func ("/event_editor/schedule_section/turning_on_recurrence_count_turns_on_its_widget",
                   test_turning_on_recurrence_count_turns_on_its_widget);
  g_test_add_func ("/event_editor/schedule_section/turning_on_recurrence_until_turns_on_its_widget",
                   test_turning_on_recurrence_until_turns_on_its_widget);
}
