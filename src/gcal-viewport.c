/* -*- mode: c; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gcal-viewport.c
 *
 * Copyright (C) 2012 - Erick Pérez Castellanos
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "gcal-viewport.h"

#include <glib/gi18n.h>

#include <string.h>

struct _GcalViewportPrivate
{
  /* property */
  GtkWidget      *viewport;
  GtkWidget      *hscrollbar;
  GtkWidget      *vscrollbar;
};

static void       gcal_viewport_constructed          (GObject      *object);

static void       gcal_viewport_get_preferred_width  (GtkWidget    *widget,
                                                      gint         *minimum,
                                                      gint         *natural);

static void       gcal_viewport_get_preferred_height (GtkWidget    *widget,
                                                      gint         *minimum,
                                                      gint         *natural);

static void       gcal_viewport_child_allocated      (GtkWidget    *widget,
                                                      GdkRectangle *allocation,
                                                      gpointer      user_data);

G_DEFINE_TYPE (GcalViewport,gcal_viewport, GTK_TYPE_OVERLAY);

static void
gcal_viewport_class_init (GcalViewportClass *klass)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->constructed = gcal_viewport_constructed;

  widget_class = GTK_WIDGET_CLASS (klass);
  widget_class->get_preferred_width = gcal_viewport_get_preferred_width;
  widget_class->get_preferred_height = gcal_viewport_get_preferred_height;

  /* Setup the template GtkBuilder xml for this class */
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/viewport.ui");

  /* Bind internals widgets */
  gtk_widget_class_bind_child (widget_class, GcalViewportPrivate, hscrollbar);
  gtk_widget_class_bind_child (widget_class, GcalViewportPrivate, vscrollbar);
  gtk_widget_class_bind_child (widget_class, GcalViewportPrivate, viewport);

  g_type_class_add_private ((gpointer)klass, sizeof (GcalViewportPrivate));
}



static void
gcal_viewport_init (GcalViewport *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            GCAL_TYPE_VIEWPORT,
                                            GcalViewportPrivate);

  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
gcal_viewport_constructed (GObject *object)
{
  GcalViewportPrivate *priv;

  GtkAdjustment *adj;

  priv = GCAL_VIEWPORT (object)->priv;

  adj = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (priv->viewport));
  gtk_range_set_adjustment (GTK_RANGE (priv->hscrollbar), adj);

  adj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (priv->viewport));
  gtk_range_set_adjustment (GTK_RANGE (priv->vscrollbar), adj);
}

static void
gcal_viewport_get_preferred_width (GtkWidget *widget,
                                   gint      *minimum,
                                   gint      *natural)
{
  if (minimum != NULL)
    *minimum = 12;
  if (natural != NULL)
    *natural = 12;
}

static void
gcal_viewport_get_preferred_height (GtkWidget *widget,
                                    gint      *minimum,
                                    gint      *natural)
{
  if (minimum != NULL)
    *minimum = 12;
  if (natural != NULL)
    *natural = 12;
}

static void
gcal_viewport_child_allocated (GtkWidget    *widget,
                               GdkRectangle *allocation,
                               gpointer      user_data)
{
  GcalViewportPrivate *priv;
  gint width, height;

  priv = GCAL_VIEWPORT (user_data)->priv;

  width = gtk_widget_get_allocated_width (GTK_WIDGET (user_data));
  height = gtk_widget_get_allocated_height (GTK_WIDGET (user_data));

  if (width >= allocation->width)
    gtk_widget_hide (priv->hscrollbar);
  else
    gtk_widget_show (priv->hscrollbar);

  if (height >= allocation->height)
    gtk_widget_hide (priv->vscrollbar);
  else
    gtk_widget_show (priv->vscrollbar);
}

/* Public API */
/**
 * gcal_viewport_new:
 *
 * Since: 0.1
 * Return value: A new #GcalViewport
 * Returns: (transfer full):
 **/
GtkWidget*
gcal_viewport_new ()
{
  return g_object_new (GCAL_TYPE_VIEWPORT, NULL);
}

void
gcal_viewport_add (GcalViewport *viewport,
                   GtkWidget    *widget)
{
  GcalViewportPrivate *priv;

  priv = viewport->priv;
  gtk_container_add (GTK_CONTAINER (priv->viewport), widget);

  /* signals handlers */
  g_signal_connect_after (widget, "size-allocate",
                          G_CALLBACK (gcal_viewport_child_allocated), viewport);
}
