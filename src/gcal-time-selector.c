/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gcal-time-selector.c
 * Copyright (C) 2015 Erick Pérez Castellanos <erickpc@gnome.org>
 *               2014 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#include "gcal-time-selector.h"

#include <glib/gi18n.h>

struct _GcalTimeSelector
{
  GtkMenuButton parent;

  GtkAdjustment *hour_adjustment;
  GtkAdjustment *minute_adjustment;

  GtkWidget *time_label;
  GtkWidget *hour_spin;
  GtkWidget *minute_spin;
  GtkWidget *period_combo;
  GtkWidget *grid;

  GDateTime *time;

  gboolean   format_24h;
};

enum
{
  PROP_0,
  PROP_TIME,
  LAST_PROP
};

enum
{
  AM,
  PM
};

static void     gcal_time_selector_constructed                 (GObject              *object);

G_DEFINE_TYPE (GcalTimeSelector, gcal_time_selector, GTK_TYPE_MENU_BUTTON);

static void
update_label (GcalTimeSelector *selector)
{
  gchar *new_label;

  if (selector->format_24h)
    {
      new_label = g_date_time_format (selector->time, "%H:%M");
    }
  else
    {
      gchar *time_str;
      gint hour, minute, period;

      hour = (gint) gtk_adjustment_get_value (selector->hour_adjustment);
      minute = (gint) gtk_adjustment_get_value (selector->minute_adjustment);


      period = gtk_combo_box_get_active (GTK_COMBO_BOX (selector->period_combo));
      time_str = g_strdup_printf ("%.2d:%.2d", hour, minute);

      if (period == AM)
        new_label = g_strdup_printf (_("%s AM"), time_str);
      else
        new_label = g_strdup_printf (_("%s PM"), time_str);

      g_free (time_str);
    }

  gtk_label_set_label (GTK_LABEL (selector->time_label), new_label);

  g_clear_pointer (&new_label, g_free);
}

static void
update_time (GcalTimeSelector *selector)
{
  GDateTime *now, *new_time;
  gint hour, minute;

  /* Retrieve current time */
  hour = (gint) gtk_adjustment_get_value (selector->hour_adjustment);
  minute = (gint) gtk_adjustment_get_value (selector->minute_adjustment);

  if (!selector->format_24h)
    {
      hour = hour % 12;

      if (gtk_combo_box_get_active (GTK_COMBO_BOX (selector->period_combo)) == PM)
        {
          g_signal_handlers_block_by_func (selector->period_combo, update_time, selector);

          gtk_combo_box_set_active (GTK_COMBO_BOX (selector->period_combo), hour >= 12);
          hour += 12;

          g_signal_handlers_unblock_by_func (selector->period_combo, update_time, selector);
        }
    }

  now = g_date_time_new_now_local ();
  new_time = g_date_time_new_local (g_date_time_get_year (now),
                                    g_date_time_get_month (now),
                                    g_date_time_get_day_of_month (now),
                                    hour, minute, 0);

  /* Set the new time */
  gcal_time_selector_set_time (selector, new_time);

  g_clear_pointer (&new_time, g_date_time_unref);
  g_clear_pointer (&now, g_date_time_unref);
}

static gboolean
on_output (GtkWidget        *widget,
           GcalTimeSelector *selector)
{
  GtkAdjustment *adjustment;
  gchar *text;
  gint value;

  adjustment = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (widget));
  value = (gint) gtk_adjustment_get_value (adjustment);
  text = g_strdup_printf ("%02d", value);
  gtk_entry_set_text (GTK_ENTRY (widget), text);

  g_free (text);

  return TRUE;
}

static void
gcal_time_selector_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GcalTimeSelector *self = (GcalTimeSelector*) object;

  switch (prop_id)
    {
    case PROP_TIME:
      g_value_set_boxed (value, self->time);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_time_selector_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GcalTimeSelector *self = (GcalTimeSelector*) object;

  switch (prop_id)
    {
    case PROP_TIME:
      gcal_time_selector_set_time (self, g_value_get_boxed (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

void
gcal_time_selector_set_time_format (GcalTimeSelector *selector,
                                    gboolean          format_24h)
{
  g_return_if_fail (GCAL_IS_TIME_SELECTOR (selector));

  selector->format_24h = format_24h;
  gtk_widget_set_visible (selector->period_combo, !format_24h);

  if (format_24h)
    {
      gtk_adjustment_set_upper (gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (selector->hour_spin)), 0.0);
      gtk_adjustment_set_upper (gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (selector->hour_spin)), 23.0);
    }
  else
    {
      gtk_adjustment_set_lower (gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (selector->hour_spin)), 1.0);
      gtk_adjustment_set_upper (gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (selector->hour_spin)), 12.0);
    }
}

static void
gcal_time_selector_class_init (GcalTimeSelectorClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->constructed = gcal_time_selector_constructed;
  object_class->get_property = gcal_time_selector_get_property;
  object_class->set_property = gcal_time_selector_set_property;

  /**
   * GcalTimeSelector::time:
   *
   * The current time of the selector.
   */
  g_object_class_install_property (object_class,
                                   PROP_TIME,
                                   g_param_spec_boxed ("time",
                                                       "Time of the selector",
                                                       "The current time of the selector",
                                                       G_TYPE_DATE_TIME,
                                                       G_PARAM_READWRITE));

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/org/gnome/calendar/time-selector.ui");

  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GcalTimeSelector, time_label);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GcalTimeSelector, hour_adjustment);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GcalTimeSelector, hour_spin);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GcalTimeSelector, minute_adjustment);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GcalTimeSelector, minute_spin);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GcalTimeSelector, period_combo);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GcalTimeSelector, grid);

  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), on_output);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), update_time);
}

static void
gcal_time_selector_init (GcalTimeSelector *self)
{
  self->time = g_date_time_new_now_local ();

  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
gcal_time_selector_constructed (GObject *object)
{
  GcalTimeSelector *selector = GCAL_TIME_SELECTOR (object);

  /* chaining up */
  G_OBJECT_CLASS (gcal_time_selector_parent_class)->constructed (object);

  gtk_widget_set_direction (selector->grid, GTK_TEXT_DIR_LTR);
}

/* Public API */
GtkWidget*
gcal_time_selector_new (void)
{
  return g_object_new (GCAL_TYPE_TIME_SELECTOR, NULL);
}

void
gcal_time_selector_set_time (GcalTimeSelector *selector,
                             GDateTime        *time)
{
  g_return_if_fail (GCAL_IS_TIME_SELECTOR (selector));

  if (selector->time != time)
    {
      gint hour, minute;

      g_clear_pointer (&selector->time, g_date_time_unref);
      selector->time = g_date_time_new_local (g_date_time_get_year (time),
                                              g_date_time_get_month (time),
                                              g_date_time_get_day_of_month (time),
                                              g_date_time_get_hour (time),
                                              g_date_time_get_minute (time),
                                              0);

      /* Update the spinners */
      g_signal_handlers_block_by_func (selector->hour_adjustment, update_time, selector);
      g_signal_handlers_block_by_func (selector->minute_adjustment, update_time, selector);

      hour = g_date_time_get_hour (time);
      minute = g_date_time_get_minute (time);

      if (!selector->format_24h)
        {
          g_signal_handlers_block_by_func (selector->period_combo, update_time, selector);

          gtk_combo_box_set_active (GTK_COMBO_BOX (selector->period_combo), hour >= 12);
          hour =  hour % 12;
          hour = (hour == 0)? 12 : hour;

          g_signal_handlers_unblock_by_func (selector->period_combo, update_time, selector);
        }

      gtk_adjustment_set_value (selector->hour_adjustment, hour);
      gtk_adjustment_set_value (selector->minute_adjustment, minute);

      update_label (selector);

      g_signal_handlers_unblock_by_func (selector->hour_adjustment, update_time, selector);
      g_signal_handlers_unblock_by_func (selector->minute_adjustment, update_time, selector);

      g_object_notify (G_OBJECT (selector), "time");
    }
}

GDateTime*
gcal_time_selector_get_time (GcalTimeSelector *selector)
{
  g_return_val_if_fail (GCAL_IS_TIME_SELECTOR (selector), NULL);

  return selector->time;
}
