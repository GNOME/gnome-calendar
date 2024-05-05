/* gcal-alarm-row.c
 *
 * Copyright 2019 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#define G_LOG_DOMAIN "GcalAlarmRow"

#include "config.h"

#include <glib/gi18n.h>

#include "gcal-alarm-row.h"

struct _GcalAlarmRow
{
  AdwActionRow        parent;

  GtkToggleButton    *volume_button;

  ECalComponentAlarm *alarm;
};

G_DEFINE_TYPE (GcalAlarmRow, gcal_alarm_row, ADW_TYPE_ACTION_ROW)

enum
{
  PROP_0,
  PROP_ALARM,
  N_PROPS
};

enum
{
  UPDATE_ALARM,
  REMOVE_ALARM,
  N_SIGNALS,
};

static guint signals [N_SIGNALS] = { 0, };
static GParamSpec *properties [N_PROPS] = { NULL, };

/*
 * Auxiliary methods
 */

static gchar*
format_alarm_duration (ICalDuration *duration)
{
  guint minutes;
  guint hours;
  guint days;

  days = i_cal_duration_get_weeks (duration) * 7 + i_cal_duration_get_days (duration);
  hours = i_cal_duration_get_hours (duration);
  minutes = i_cal_duration_get_minutes (duration);

  if (days > 0)
    {
      if (hours > 0)
        {
          if (minutes > 0)
            {
              /*
               * Translators: %1$u is days (in numbers), %2$u is hours (in numbers), and %3$u is minutes (in numbers).
               * The full sentence would be "X days, X hours, and X minutes before the event starts".
               */
              const gchar *format = g_dngettext (GETTEXT_PACKAGE,
                                                 g_dngettext (GETTEXT_PACKAGE,
                                                              g_dngettext (GETTEXT_PACKAGE,
                                                                           "%1$u day, %2$u hour, and %3$u minute before",
                                                                           "%1$u day, %2$u hour, and %3$u minutes before",
                                                                           minutes),
                                                              g_dngettext (GETTEXT_PACKAGE,
                                                                           "%1$u day, %2$u hours, and %3$u minute before",
                                                                           "%1$u day, %2$u hours, and %3$u minutes before",
                                                                           minutes),
                                                              hours),
                                                 g_dngettext (GETTEXT_PACKAGE,
                                                              g_dngettext (GETTEXT_PACKAGE,
                                                                           "%1$u days, %2$u hour, and %3$u minute before",
                                                                           "%1$u days, %2$u hour, and %3$u minutes before",
                                                                           minutes),
                                                              g_dngettext (GETTEXT_PACKAGE,
                                                                           "%1$u days, %2$u hours, and %3$u minute before",
                                                                           "%1$u days, %2$u hours, and %3$u minutes before",
                                                                           minutes),
                                                              hours),
                                                  days);
              return g_strdup_printf (format, days, hours, minutes);
            }
          else
            {
              /*
               * Translators: %1$u is days (in numbers) and %2$u is hours (in numbers). The full sentence would be "X
               * days and X hours before the event starts".
               */
              const gchar *format = g_dngettext (GETTEXT_PACKAGE,
                                                 g_dngettext (GETTEXT_PACKAGE,
                                                             "%1$u day and %2$u hour before",
                                                             "%1$u day and %2$u hours before",
                                                              hours),
                                                 g_dngettext (GETTEXT_PACKAGE,
                                                             "%1$u days and %2$u hour before",
                                                             "%1$u days and %2$u hours before",
                                                              hours),
                                                  days);
              return g_strdup_printf (format, days, hours);
            }
        }
      else
        {
          if (minutes > 0)
            {
              /* Translators: %1$u is days (in numbers) and %2$u is minutes (in numbers). The full sentence would be "X
               * days and X hours before the event starts".
               */
              const gchar *format = g_dngettext (GETTEXT_PACKAGE,
                                                 g_dngettext (GETTEXT_PACKAGE,
                                                             "%1$u day and %2$u minute before",
                                                             "%1$u day and %2$u minutes before",
                                                              minutes),
                                                 g_dngettext (GETTEXT_PACKAGE,
                                                             "%1$u days and %2$u minute before",
                                                             "%1$u days and %2$u minutes before",
                                                              minutes),
                                                  days);
              return g_strdup_printf (format, days, minutes);
            }
          else
            {
              /* Translators: %1$u is days (in numbers). The full sentence would be "X days before the event starts". */
              const gchar *format = g_dngettext (GETTEXT_PACKAGE,
                                                 "%1$u day before",
                                                 "%1$u days before",
                                                  days);
              return g_strdup_printf (format, days);
            }
        }
    }
  else
    {
      if (hours > 0)
        {
          if (minutes > 0)
            {
              /*
               * Translators: %1$u is hours (in numbers), and %2$u is minutes (in numbers). The full sentence would be
               * "X hours and X minutes before the event starts". */
              const gchar *format = g_dngettext (GETTEXT_PACKAGE,
                                                 g_dngettext (GETTEXT_PACKAGE,
                                                              "%1$u hour and %2$u minute before",
                                                              "%1$u hour and %2$u minutes before",
                                                              minutes),
                                                 g_dngettext (GETTEXT_PACKAGE,
                                                              "%1$u hours and %2$u minute before",
                                                              "%1$u hours and %2$u minutes before",
                                                              minutes),
                                                 hours);
              return g_strdup_printf (format, hours, minutes);
            }
          else
            {
              /* Translators: %1$u is hours (in numbers). The full sentence would be "X hours before the event starts". */
              const gchar *format = g_dngettext (GETTEXT_PACKAGE,
                                                 "%1$u hour before",
                                                 "%1$u hours before",
                                                  hours);
              return g_strdup_printf (format, hours);
            }
        }
      else
        {
          if (minutes > 0)
            {
              /* Translators: %1$u is minutes (in numbers). The full sentence would be "X minutes before the event starts". */
              const gchar *format = g_dngettext (GETTEXT_PACKAGE,
                                                 "%1$u minute before",
                                                 "%1$u minutes before",
                                                  minutes);
              return g_strdup_printf (format, minutes);
            }
          else
            {
              return g_strdup (_("Event start time"));
            }
        }
    }
}

static void
setup_alarm (GcalAlarmRow *self)
{
  g_autofree gchar *formatted_duration = NULL;
  ECalComponentAlarmTrigger *trigger;
  ECalComponentAlarmAction action;
  ICalDuration *duration;

  trigger = e_cal_component_alarm_get_trigger (self->alarm);
  duration = e_cal_component_alarm_trigger_get_duration (trigger);
  formatted_duration = format_alarm_duration (duration);

  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self), formatted_duration);

  action = e_cal_component_alarm_get_action (self->alarm);
  gtk_toggle_button_set_active (self->volume_button, action == E_CAL_COMPONENT_ALARM_AUDIO);
}


/*
 * Callbacks
 */

static void
on_remove_button_clicked_cb (GtkButton    *button,
                             GcalAlarmRow *self)
{
  g_signal_emit (self, signals[REMOVE_ALARM], 0);
}

static void
on_sound_toggle_changed_cb (GtkToggleButton *button,
                            GParamSpec      *pspec,
                            GcalAlarmRow    *self)
{
  ECalComponentAlarmAction action;
  gboolean has_sound;

  has_sound = gtk_toggle_button_get_active (button);

  /* Setup the alarm action */
  action = has_sound ? E_CAL_COMPONENT_ALARM_AUDIO : E_CAL_COMPONENT_ALARM_DISPLAY;

  e_cal_component_alarm_set_action (self->alarm, action);

  /* Update the volume icon */
  gtk_button_set_icon_name (GTK_BUTTON (self->volume_button),
                            has_sound ? "audio-volume-high-symbolic" : "audio-volume-muted-symbolic");

  g_signal_emit (self, signals[UPDATE_ALARM], 0);
}


/*
 * GObject overrides
 */

static void
gcal_alarm_row_finalize (GObject *object)
{
  GcalAlarmRow *self = (GcalAlarmRow *)object;

  g_clear_pointer (&self->alarm, e_cal_component_alarm_free);

  G_OBJECT_CLASS (gcal_alarm_row_parent_class)->finalize (object);
}

static void
gcal_alarm_row_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GcalAlarmRow *self = GCAL_ALARM_ROW (object);

  switch (prop_id)
    {
    case PROP_ALARM:
      g_value_set_pointer (value, self->alarm);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_alarm_row_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GcalAlarmRow *self = GCAL_ALARM_ROW (object);

  switch (prop_id)
    {
    case PROP_ALARM:
      g_assert (self->alarm == NULL);
      self->alarm = e_cal_component_alarm_copy (g_value_get_pointer (value));
      setup_alarm (self);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_alarm_row_class_init (GcalAlarmRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gcal_alarm_row_finalize;
  object_class->get_property = gcal_alarm_row_get_property;
  object_class->set_property = gcal_alarm_row_set_property;

  properties[PROP_ALARM] = g_param_spec_pointer ("alarm",
                                                 "Alarm",
                                                 "Alarm",
                                                 G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals[REMOVE_ALARM] = g_signal_new ("remove-alarm",
                                        GCAL_TYPE_ALARM_ROW,
                                        G_SIGNAL_RUN_LAST,
                                        0,
                                        NULL, NULL,
                                        g_cclosure_marshal_VOID__VOID,
                                        G_TYPE_NONE,
                                        0);
  signals[UPDATE_ALARM] = g_signal_new ("update-alarm",
                                        GCAL_TYPE_ALARM_ROW,
                                        G_SIGNAL_RUN_LAST,
                                        0,
                                        NULL, NULL,
                                        g_cclosure_marshal_VOID__VOID,
                                        G_TYPE_NONE,
                                        0);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/event-editor/gcal-alarm-row.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalAlarmRow, volume_button);

  gtk_widget_class_bind_template_callback (widget_class, on_remove_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_sound_toggle_changed_cb);
}

static void
gcal_alarm_row_init (GcalAlarmRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget*
gcal_alarm_row_new (ECalComponentAlarm *alarm)
{
  return g_object_new (GCAL_TYPE_ALARM_ROW,
                       "alarm", alarm,
                       NULL);
}

ECalComponentAlarm*
gcal_alarm_row_get_alarm (GcalAlarmRow *self)
{
  g_return_val_if_fail (GCAL_IS_ALARM_ROW (self), NULL);

  return self->alarm;
}
