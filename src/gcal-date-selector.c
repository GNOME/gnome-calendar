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
  GtkEntry     parent;

  /* widgets */
  GtkWidget   *date_chooser;
  GtkWidget   *date_selector_popover;
};

enum
{
  PROP_0,
  PROP_DATE,
  LAST_PROP
};

G_DEFINE_TYPE (GcalDateSelector, gcal_date_selector, GTK_TYPE_ENTRY);

static void
update_text (GcalDateSelector *self)
{
  GDateTime *date;
  gchar *label;

  date = gcal_date_chooser_get_date (GCAL_DATE_CHOOSER (self->date_chooser));

  /* rebuild the date label */
  label = g_date_time_format (date, "%x");

  gtk_entry_set_text (GTK_ENTRY (self), label);
  g_free (label);
}

static void
calendar_day_selected (GcalDateSelector *self)
{
  update_text (self);

  g_object_notify (G_OBJECT (self), "date");
}

static void
parse_date (GcalDateSelector *self)
{
  GDateTime *new_date;
  GDate parsed_date;

  g_date_clear (&parsed_date, 1);
  g_date_set_parse (&parsed_date, gtk_entry_get_text (GTK_ENTRY (self)));

  if (!g_date_valid (&parsed_date))
    {
      update_text (self);
      return;
    }

  new_date = g_date_time_new_local (g_date_get_year (&parsed_date),
                                    g_date_get_month (&parsed_date),
                                    g_date_get_day (&parsed_date),
                                    0, 0, 0);

  gcal_date_selector_set_date (self, new_date);

  g_clear_pointer (&new_date, g_date_time_unref);
}

static void
icon_pressed_cb (GcalDateSelector     *self,
                 GtkEntryIconPosition  position,
                 GdkEvent             *event)
{
  GdkRectangle icon_bounds;

  gtk_entry_get_icon_area (GTK_ENTRY (self), position, &icon_bounds);

  /* HACK: seems like the popover is 2.5 px misplaced */
  icon_bounds.x += 3;

  gtk_popover_set_relative_to (GTK_POPOVER (self->date_selector_popover), GTK_WIDGET (self));
  gtk_popover_set_pointing_to (GTK_POPOVER (self->date_selector_popover), &icon_bounds);

  gtk_widget_show (self->date_selector_popover);
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

static gboolean
gcal_date_selector_focus_in_event (GtkWidget     *widget,
                                   GdkEventFocus *event)
{
  parse_date (GCAL_DATE_SELECTOR (widget));

  return GTK_WIDGET_CLASS (gcal_date_selector_parent_class)->focus_in_event (widget, event);
}

static gboolean
gcal_date_selector_focus_out_event (GtkWidget     *widget,
                                    GdkEventFocus *event)
{
  parse_date (GCAL_DATE_SELECTOR (widget));

  return GTK_WIDGET_CLASS (gcal_date_selector_parent_class)->focus_out_event (widget, event);
}

static void
gcal_date_selector_activate (GtkEntry *entry)
{
  parse_date (GCAL_DATE_SELECTOR (entry));
}

static void
gcal_date_selector_class_init (GcalDateSelectorClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkEntryClass *entry_class = GTK_ENTRY_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = gcal_date_selector_get_property;
  object_class->set_property = gcal_date_selector_set_property;

  widget_class->focus_in_event = gcal_date_selector_focus_in_event;
  widget_class->focus_out_event = gcal_date_selector_focus_out_event;

  entry_class->activate = gcal_date_selector_activate;

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

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/date-selector.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalDateSelector, date_chooser);
  gtk_widget_class_bind_template_child (widget_class, GcalDateSelector, date_selector_popover);

  gtk_widget_class_bind_template_callback (widget_class, calendar_day_selected);
  gtk_widget_class_bind_template_callback (widget_class, icon_pressed_cb);
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
