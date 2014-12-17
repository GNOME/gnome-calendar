/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gcal-time-selector.c
 * Copyright (C) 2012 Erick Pérez Castellanos <erickpc@gnome.org>
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
  GtkWidget *popover;
  GtkWidget *hour_spin;
  GtkWidget *minute_spin;
  GtkWidget *period_check;
  GtkWidget *period_combo;

  gboolean   format_24h;
};

enum
{
  MODIFIED,
  NUM_SIGNALS
};

static guint signals[NUM_SIGNALS] = { 0, };

static gboolean on_output                                      (GtkSpinButton        *button,
                                                                gpointer              user_data);

static void     gcal_time_selector_constructed                 (GObject              *object);

G_DEFINE_TYPE_WITH_PRIVATE (GcalTimeSelector, gcal_time_selector, GTK_TYPE_TOGGLE_BUTTON);

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

}

static void
gcal_time_selector_init (GcalTimeSelector *self)
{
  GcalTimeSelectorPrivate *priv;

  priv = gcal_time_selector_get_instance_private (self);

  priv->time_label = NULL;
  priv->popover = NULL;
  priv->hour_spin = NULL;
  priv->minute_spin = NULL;
}

static void
gcal_time_selector_constructed (GObject *object)
{
  GcalTimeSelectorPrivate *priv;
  GtkWidget *grid;
  GtkBuilder *builder;

  GSettings *settings;
  gchar *clock_format;

  priv = gcal_time_selector_get_instance_private (GCAL_TIME_SELECTOR (object));

  /* chaining up */
  G_OBJECT_CLASS (gcal_time_selector_parent_class)->constructed (object);

  gtk_widget_set_hexpand (GTK_WIDGET (object), TRUE);

  /* 24h setting */
  settings = g_settings_new ("org.gnome.desktop.interface");
  clock_format = g_settings_get_string (settings, "clock-format");
  priv->format_24h = (g_strcmp0 (clock_format, "24h") == 0);

  g_free (clock_format);
  g_object_unref (settings);

  /* time label */
  priv->time_label = gtk_label_new (NULL);
  gtk_label_set_label (GTK_LABEL (priv->time_label), "00:00");
  gtk_widget_show (priv->time_label);

  gtk_container_add (GTK_CONTAINER (object), priv->time_label);

  /* popover */
  builder = gtk_builder_new ();
  gtk_builder_add_from_resource (builder, "/org/gnome/calendar/time-selector.ui", NULL);

  priv->popover = gtk_popover_new (GTK_WIDGET (object));

  grid = (GtkWidget*) gtk_builder_get_object (builder, "grid");
  g_object_ref (grid);

  priv->hour_spin = (GtkWidget*) gtk_builder_get_object (builder, "hour_spin");
  g_object_ref (priv->hour_spin);

  priv->minute_spin = (GtkWidget*) gtk_builder_get_object (builder, "minute_spin");
  g_object_ref (priv->minute_spin);

  priv->period_combo = (GtkWidget*) gtk_builder_get_object (builder, "period_combo");
  gtk_widget_set_visible (priv->period_combo, !priv->format_24h);
  g_object_ref (priv->period_combo);

  g_object_unref (builder);

  gtk_container_add (GTK_CONTAINER (priv->popover), grid);
  g_object_bind_property (priv->popover, "visible", object, "active", G_BINDING_BIDIRECTIONAL);

  /* signals */
  g_signal_connect (priv->hour_spin, "output", G_CALLBACK (on_output), object);
  g_signal_connect (priv->minute_spin, "output", G_CALLBACK (on_output), object);
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
  gchar *new_time;

  g_return_if_fail (GCAL_IS_TIME_SELECTOR (selector));
  priv = gcal_time_selector_get_instance_private (selector);
  hour_adj = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (priv->hour_spin));
  minute_adj = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (priv->minute_spin));
  g_warn_if_fail (hours < 24);
  g_warn_if_fail (minutes < 60);

  gtk_adjustment_set_value (hour_adj, hours < 24 ? hours : 0);
  gtk_adjustment_set_value (minute_adj, minutes < 60 ? minutes : 0);

  if (priv->format_24h)
    {
      new_time = g_strdup_printf ("%.2d:%.2d", hours, minutes);
    }
  else
    {
      new_time = g_strdup_printf ("%.2d:%.2d", hours, minutes);
    }

  gtk_label_set_label (GTK_LABEL (priv->time_label), new_time);

  g_free (new_time);
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

  *hours = (gint) gtk_adjustment_get_value (hour_adj);
  *minutes = (gint) gtk_adjustment_get_value (minute_adj);
}
