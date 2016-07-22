/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gcal-date-selector.c
 * Copyright (C) 2014 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#include "gcal-date-chooser.h"
#include "gcal-date-selector.h"

#include <locale.h>
#include <langinfo.h>
#include <stdlib.h>
#include <glib/gi18n.h>

struct _GcalDateSelector
{
  GtkMenuButton parent;

  /* widgets */
  GtkWidget   *date_chooser;
  GtkWidget   *date_label;
};

enum
{
  PROP_0,
  PROP_DATE,
  LAST_PROP
};

G_DEFINE_TYPE (GcalDateSelector, gcal_date_selector, GTK_TYPE_MENU_BUTTON);

static void
update_label (GcalDateSelector *self)
{
  GDateTime *date;
  gchar *label;

  date = gcal_date_chooser_get_date (GCAL_DATE_CHOOSER (self->date_chooser));

  /* rebuild the date label */
  label = g_date_time_format (date, "%x");

  gtk_label_set_label (GTK_LABEL (self->date_label), label);
  g_free (label);
}

static void
calendar_day_selected (GcalDateSelector *self)
{
  update_label (self);

  g_object_notify (G_OBJECT (self), "date");
}

static void
gcal_date_selector_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GcalDateSelector *self = (GcalDateSelector*) object;

  switch (prop_id)
    {
    case PROP_DATE:
      g_value_set_boxed (value, gcal_date_chooser_get_date (GCAL_DATE_CHOOSER (self->date_chooser)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_date_selector_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GcalDateSelector *self = (GcalDateSelector*) object;

  switch (prop_id)
    {
    case PROP_DATE:
      gcal_date_selector_set_date (self, g_value_get_boxed (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_date_selector_class_init (GcalDateSelectorClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->get_property = gcal_date_selector_get_property;
  object_class->set_property = gcal_date_selector_set_property;

  /**
   * GcalDateSelector::date:
   *
   * The current date of the selector.
   */
  g_object_class_install_property (object_class,
                                   PROP_DATE,
                                   g_param_spec_boxed ("date",
                                                       "Date of the selector",
                                                       "The current date of the selector",
                                                       G_TYPE_DATE_TIME,
                                                       G_PARAM_READWRITE));

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/org/gnome/calendar/date-selector.ui");

  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GcalDateSelector, date_chooser);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GcalDateSelector, date_label);

  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), calendar_day_selected);
}

static void
gcal_date_selector_init (GcalDateSelector *self)
{

  gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);

  gtk_widget_init_template (GTK_WIDGET (self));
}

/* Public API */
GtkWidget*
gcal_date_selector_new (void)
{
  return g_object_new (GCAL_TYPE_DATE_SELECTOR, NULL);
}

/**
 * gcal_date_selector_set_date:
 * @selector: a #GcalDateSelector
 * @date: a valid #GDateTime
 *
 * Set the value of the shown date.
 */
void
gcal_date_selector_set_date (GcalDateSelector *selector,
                             GDateTime        *date)
{
  g_return_if_fail (GCAL_IS_DATE_SELECTOR (selector));

  /* set calendar's date */
  gcal_date_chooser_set_date (GCAL_DATE_CHOOSER (selector->date_chooser), date);

  /* emit the MODIFIED signal */
  g_object_notify (G_OBJECT (selector), "date");
}

/**
 * gcal_date_selector_get_date:
 * @selector: a #GcalDateSelector
 *
 * Get the value of the date shown
 *
 * Returns: (transfer none): the date of the selector.
 */
GDateTime*
gcal_date_selector_get_date (GcalDateSelector *selector)
{
  g_return_val_if_fail (GCAL_IS_DATE_SELECTOR (selector), NULL);

  return gcal_date_chooser_get_date (GCAL_DATE_CHOOSER (selector->date_chooser));
}
