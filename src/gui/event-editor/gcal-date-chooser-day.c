/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2015 Red Hat, Inc.
 * Copyright (C) 2022 Purism SPC
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

struct _GcalDateChooserDay
{
  GtkButton           parent;

  GtkWidget          *label;
  GDateTime          *date;
};

G_DEFINE_TYPE (GcalDateChooserDay, gcal_date_chooser_day, GTK_TYPE_BUTTON)

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
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = gcal_date_chooser_day_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/event-editor/gcal-date-chooser-day.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalDateChooserDay, label);
}

static void
gcal_date_chooser_day_init (GcalDateChooserDay *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
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
  GtkWidget *widget = GTK_WIDGET (self);

  if (G_UNLIKELY (selected))
    {
      gtk_widget_set_state_flags (widget, GTK_STATE_FLAG_SELECTED, FALSE);
      gtk_widget_remove_css_class (widget, "flat");
    }
  else
    {
      gtk_widget_unset_state_flags (widget, GTK_STATE_FLAG_SELECTED);
      gtk_widget_add_css_class (widget, "flat");
    }
}
