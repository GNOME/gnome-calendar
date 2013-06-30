/* -*- mode: c; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gcal-new-event-widget.c
 * Copyright (C) 2012 Erick Pérez Castellanos <erickpc@gnome.org>
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

#include "gcal-new-event-widget.h"
#include "gcal-arrow-bin.h"

typedef struct
{
  GtkWidget     *title_label;
  GtkWidget     *what_entry;
  GtkWidget     *calendar_button;

  GtkWidget     *create_button;
  GtkWidget     *details_button;

  GtkWidget     *close_button;
} GcalNewEventWidgetPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GcalNewEventWidget, gcal_new_event_widget, GTK_TYPE_OVERLAY)

static void
gcal_new_event_widget_class_init (GcalNewEventWidgetClass *klass)
{
  GtkWidgetClass *widget_class;

  widget_class = GTK_WIDGET_CLASS (klass);

  /* Setup the template GtkBuilder xml for this class */
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/new-event.ui");

  /* Bind internals widgets */
  gtk_widget_class_bind_child (widget_class, GcalNewEventWidgetPrivate, title_label);
  gtk_widget_class_bind_child (widget_class, GcalNewEventWidgetPrivate, what_entry);
  gtk_widget_class_bind_child (widget_class, GcalNewEventWidgetPrivate, calendar_button);
  gtk_widget_class_bind_child (widget_class, GcalNewEventWidgetPrivate, create_button);
  gtk_widget_class_bind_child (widget_class, GcalNewEventWidgetPrivate, details_button);
  gtk_widget_class_bind_child (widget_class, GcalNewEventWidgetPrivate, close_button);
}

static void
gcal_new_event_widget_init (GcalNewEventWidget *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

/* Public API */
GtkWidget*
gcal_new_event_widget_new (void)
{
  return g_object_new (GCAL_TYPE_NEW_EVENT_WIDGET, NULL);
}

void
gcal_new_event_widget_set_title (GcalNewEventWidget *widget,
                                 const gchar        *title)
{
  GcalNewEventWidgetPrivate *priv;

  priv = gcal_new_event_widget_get_instance_private (widget);
  gtk_label_set_text (GTK_LABEL (priv->title_label), title);
}

/**
 * gcal_new_event_widget_get_entry:
 * @widget:
 *
 * Get a reference to the summanry entry
 *
 * Returns: (transfer none) a #GtkWidget
 **/
GtkWidget*
gcal_new_event_widget_get_entry (GcalNewEventWidget *widget)
{
  GcalNewEventWidgetPrivate *priv;

  priv = gcal_new_event_widget_get_instance_private (widget);
  return priv->what_entry;
}

GtkWidget*
gcal_new_event_widget_get_create_button (GcalNewEventWidget *widget)
{
  GcalNewEventWidgetPrivate *priv;

  priv = gcal_new_event_widget_get_instance_private (widget);
  return priv->create_button;
}

GtkWidget*
gcal_new_event_widget_get_details_button (GcalNewEventWidget *widget)
{
  GcalNewEventWidgetPrivate *priv;

  priv = gcal_new_event_widget_get_instance_private (widget);
  return priv->details_button;
}

/**
 * gcal_new_event_widget_get_close_button:
 * @widget: a #GcalNewEventWidget
 *
 * Get a pointer a to the close button in the widget.
 *
 * Returns: (transfer none) a #GtkWidget
 **/
GtkWidget*
gcal_new_event_widget_get_close_button (GcalNewEventWidget *widget)
{
  GcalNewEventWidgetPrivate *priv;

  priv = gcal_new_event_widget_get_instance_private (widget);
  return priv->close_button;
}
