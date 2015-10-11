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

struct _GcalTimeSelectorPrivate
{
  GtkWidget *time_label;
  GtkWidget *hour_spin;
  GtkWidget *minute_spin;
  GtkWidget *period_combo;
  GtkWidget *grid;

  gboolean   format_24h;
};

enum
{
  MODIFIED,
  NUM_SIGNALS
};

enum
{
  AM,
  PM
};

static guint signals[NUM_SIGNALS] = { 0, };

static void     format_date_label                              (GcalTimeSelector     *selector);

static gboolean on_output                                      (GtkSpinButton        *button,
                                                                gpointer              user_data);

static void     period_changed                                 (GtkComboBox          *combo,
                                                                gpointer              user_data);

static void     time_changed                                   (GtkAdjustment        *adjustment,
                                                                gpointer              user_data);

static void     gcal_time_selector_constructed                 (GObject              *object);

G_DEFINE_TYPE_WITH_PRIVATE (GcalTimeSelector, gcal_time_selector, GTK_TYPE_MENU_BUTTON);

static void
format_date_label (GcalTimeSelector *selector)
{
  GcalTimeSelectorPrivate *priv;
  GtkAdjustment *hour_adj;
  GtkAdjustment *minute_adj;
  gchar *new_time;
  gint hour, minute;

  priv = gcal_time_selector_get_instance_private (selector);

  hour_adj = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (priv->hour_spin));
  minute_adj = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (priv->minute_spin));

  /* get current time */
  hour = (gint) gtk_adjustment_get_value (hour_adj);
  minute = (gint) gtk_adjustment_get_value (minute_adj);

  /* format time according to the system 12h/24h setting */
  if (priv->format_24h)
    {
      new_time = g_strdup_printf ("%.2d:%.2d", hour, minute);
    }
  else
    {
      gint period;

      period = gtk_combo_box_get_active (GTK_COMBO_BOX (priv->period_combo));

      /* FIXME: we shouldn't expose print formatting code to translators */
      if (period == AM)
        new_time = g_strdup_printf (_("%.2d:%.2d AM"), hour, minute);
      else
        new_time = g_strdup_printf (_("%.2d:%.2d PM"), hour, minute);
    }

  gtk_label_set_label (GTK_LABEL (priv->time_label), new_time);

  g_free (new_time);
}

static gboolean
on_output (GtkSpinButton *button,
           gpointer       user_data)
{
  gchar *text;
  gint value;

  value = (gint) gtk_adjustment_get_value (gtk_spin_button_get_adjustment (button));
  text = g_strdup_printf ("%02d", value);
  gtk_entry_set_text (GTK_ENTRY (button), text);

  g_free (text);

  return TRUE;
}

static void
period_changed (GtkComboBox *combo,
                gpointer     user_data)
{
  format_date_label (GCAL_TIME_SELECTOR (user_data));

  g_signal_emit (user_data, signals[MODIFIED], 0);
}

void
gcal_time_selector_set_time_format (GcalTimeSelector *selector,
                                    gboolean          format_24h)
{
  GcalTimeSelectorPrivate *priv;

  priv = gcal_time_selector_get_instance_private (selector);

  priv->format_24h = format_24h;
  gtk_widget_set_visible (priv->period_combo, !format_24h);

  if (format_24h)
    gtk_adjustment_set_upper (gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (priv->hour_spin)), 23.0);
  else
    gtk_adjustment_set_upper (gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (priv->hour_spin)), 12.0);
}

static void
time_changed (GtkAdjustment *adjustment,
              gpointer       user_data)
{
  format_date_label (GCAL_TIME_SELECTOR (user_data));

  g_signal_emit (user_data, signals[MODIFIED], 0);
}

static void
gcal_time_selector_class_init (GcalTimeSelectorClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->constructed = gcal_time_selector_constructed;

  signals[MODIFIED] = g_signal_new ("modified",
                                    GCAL_TYPE_TIME_SELECTOR,
                                    G_SIGNAL_RUN_LAST,
                                    G_STRUCT_OFFSET (GcalTimeSelectorClass,
                                                     modified),
                                    NULL, NULL,
                                    g_cclosure_marshal_VOID__VOID,
                                    G_TYPE_NONE,
                                    0);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/org/gnome/calendar/time-selector.ui");

  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GcalTimeSelector, time_label);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GcalTimeSelector, hour_spin);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GcalTimeSelector, minute_spin);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GcalTimeSelector, period_combo);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GcalTimeSelector, grid);

  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), on_output);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), period_changed);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), time_changed);
}

static void
gcal_time_selector_init (GcalTimeSelector *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
gcal_time_selector_constructed (GObject *object)
{
  GcalTimeSelectorPrivate *priv;

  priv = gcal_time_selector_get_instance_private (GCAL_TIME_SELECTOR (object));

  /* chaining up */
  G_OBJECT_CLASS (gcal_time_selector_parent_class)->constructed (object);

  gtk_widget_set_direction (priv->grid, GTK_TEXT_DIR_LTR);
}

/* Public API */
GtkWidget*
gcal_time_selector_new (void)
{
  return g_object_new (GCAL_TYPE_TIME_SELECTOR, NULL);
}

void
gcal_time_selector_set_time (GcalTimeSelector *selector,
                             gint              hours,
                             gint              minutes)
{
  GcalTimeSelectorPrivate *priv;
  GtkAdjustment *hour_adj;
  GtkAdjustment *minute_adj;

  g_return_if_fail (GCAL_IS_TIME_SELECTOR (selector));
  priv = gcal_time_selector_get_instance_private (selector);
  hour_adj = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (priv->hour_spin));
  minute_adj = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (priv->minute_spin));
  g_warn_if_fail (hours < 24);
  g_warn_if_fail (minutes < 60);

  /* setup spin buttons according to the format */
  if (priv->format_24h)
    {
      gtk_adjustment_set_value (hour_adj, hours < 24 ? hours : 0);
    }
  else
    {
      gint period;

      period = hours < 12? AM : PM;
      hours = hours % 12;
      if(hours == 0){
        hours = 12;
      }

      gtk_combo_box_set_active (GTK_COMBO_BOX (priv->period_combo), period);
      gtk_adjustment_set_value (hour_adj, hours);
    }

  gtk_adjustment_set_value (minute_adj, minutes < 60 ? minutes : 0);

  /* format time label accordingly */
  format_date_label (selector);
}

void
gcal_time_selector_get_time (GcalTimeSelector *selector,
                             gint             *hours,
                             gint             *minutes)
{
  GcalTimeSelectorPrivate *priv;
  GtkAdjustment *hour_adj;
  GtkAdjustment *minute_adj;

  g_return_if_fail (GCAL_IS_TIME_SELECTOR (selector));
  priv = gcal_time_selector_get_instance_private (selector);
  hour_adj = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (priv->hour_spin));
  minute_adj = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (priv->minute_spin));

  /* 24h, the easy one*/
  if (priv->format_24h)
    {
      *hours = (gint) gtk_adjustment_get_value (hour_adj);
    }

  /* 12h format must calculate the time
   * according to the AM/PM combo box */
  else
    {
      gint am_pm;

      am_pm = gtk_combo_box_get_active (GTK_COMBO_BOX (priv->period_combo));
      *hours = (gint) gtk_adjustment_get_value (hour_adj) % 12 + 12 * am_pm;
    }

  /* minute field isn't dependant on 12h/24h format */
  *minutes = (gint) gtk_adjustment_get_value (minute_adj);
}
