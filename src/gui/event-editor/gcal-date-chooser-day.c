/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2015 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#define G_LOG_DOMAIN "GcalDateChooserDay"

#include "config.h"

#include "gcal-date-chooser-day.h"

#include <stdlib.h>
#include <langinfo.h>

enum {
  SELECTED,
  LAST_DAY_SIGNAL
};

static guint signals[LAST_DAY_SIGNAL] = { 0, };

struct _GcalDateChooserDay
{
  AdwBin              parent;

  GtkWidget          *label;
  GDateTime          *date;
};

G_DEFINE_TYPE (GcalDateChooserDay, gcal_date_chooser_day, ADW_TYPE_BIN)

static void
day_pressed (GtkGestureClick    *click_gesture,
             gint                n_press,
             gdouble             x,
             gdouble             y,
             GcalDateChooserDay *self)
{
  if (n_press == 1)
    g_signal_emit (self, signals[SELECTED], 0);
}

static void
gcal_date_chooser_day_dispose (GObject *object)
{
  GcalDateChooserDay *self = GCAL_DATE_CHOOSER_DAY (object);

  g_clear_pointer (&self->date, g_date_time_unref);

  G_OBJECT_CLASS (gcal_date_chooser_day_parent_class)->dispose (object);
}

static void
gcal_date_chooser_day_class_init (GcalDateChooserDayClass *class)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = gcal_date_chooser_day_dispose;

  signals[SELECTED] = g_signal_new ("selected",
                                    GCAL_TYPE_DATE_CHOOSER_DAY,
                                    G_SIGNAL_RUN_FIRST,
                                    0,
                                    NULL,
                                    NULL,
                                    NULL,
                                    G_TYPE_NONE, 0);

  gtk_widget_class_set_css_name (widget_class, "day");

  gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_space, 0, "selected", NULL);
  gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_Return, 0, "selected", NULL);
  gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_ISO_Enter, 0, "selected", NULL);
  gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_KP_Enter, 0, "selected", NULL);
  gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_KP_Space, 0, "selected", NULL);
}

static void
gcal_date_chooser_day_init (GcalDateChooserDay *self)
{
  GtkGesture *click_gesture;
  GtkWidget *widget = GTK_WIDGET (self);

  gtk_widget_set_halign (widget, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);

  gtk_widget_set_can_focus (widget, TRUE);
  gtk_widget_add_css_class (widget, "day");

  self->label = gtk_label_new ("");
  gtk_widget_set_halign (self->label, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (self->label, GTK_ALIGN_CENTER);
  gtk_widget_set_hexpand (self->label, TRUE);
  gtk_widget_set_vexpand (self->label, TRUE);

  adw_bin_set_child (ADW_BIN (self), self->label);

  click_gesture = gtk_gesture_click_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (click_gesture), 0);
  gtk_widget_add_controller (widget, GTK_EVENT_CONTROLLER (click_gesture));

  g_signal_connect_object (click_gesture,
                           "pressed",
                           G_CALLBACK (day_pressed),
                           self,
                           0);
}

GtkWidget*
gcal_date_chooser_day_new (void)
{
  return g_object_new (GCAL_TYPE_DATE_CHOOSER_DAY, NULL);
}

void
gcal_date_chooser_day_set_date (GcalDateChooserDay *self,
                                GDateTime          *date)
{
  gchar *text;

  g_clear_pointer (&self->date, g_date_time_unref);
  self->date = g_date_time_ref (date);

  text = g_strdup_printf ("%d", g_date_time_get_day_of_month (date));
  gtk_label_set_label (GTK_LABEL (self->label), text);
  g_free (text);
}

GDateTime*
gcal_date_chooser_day_get_date (GcalDateChooserDay *self)
{
  return self->date;
}

void
gcal_date_chooser_day_set_other_month (GcalDateChooserDay *self,
                                       gboolean            other_month)
{
  if (other_month)
    gtk_widget_add_css_class (GTK_WIDGET (self), "other-month");
  else
    gtk_widget_remove_css_class (GTK_WIDGET (self), "other-month");
}

void
gcal_date_chooser_day_set_selected (GcalDateChooserDay *self,
                                    gboolean            selected)
{
  if (selected)
    gtk_widget_set_state_flags (GTK_WIDGET (self), GTK_STATE_FLAG_SELECTED, FALSE);
  else
    gtk_widget_unset_state_flags (GTK_WIDGET (self), GTK_STATE_FLAG_SELECTED);
}
