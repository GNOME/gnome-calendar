/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 2 -*- */
/*
 * gcal-floating-container.c
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

#include "gcal-floating-container.h"

struct _GcalFloatingContainerPrivate
{
  guint border;
};

static void     gcal_floating_container_get_preferred_width            (GtkWidget       *widget,
                                                                        gint            *minimum_width,
                                                                        gint            *natural_width);

static void     gcal_floating_container_get_preferred_height_for_width (GtkWidget       *widget,
                                                                        gint             width,
                                                                        gint            *minimum_height,
                                                                        gint            *natural_height);

static void     gcal_floating_container_get_preferred_height           (GtkWidget       *widget,
                                                                        gint            *minimum_height,
                                                                        gint            *natural_height);

static void     gcal_floating_container_get_preferred_width_for_height (GtkWidget       *widget,
                                                                        gint             height,
                                                                        gint            *minimum_width,
                                                                        gint            *natural_width);

static void     gcal_floating_container_size_allocate                  (GtkWidget       *widget,
                                                                        GtkAllocation   *allocation);

static gboolean gcal_floating_container_draw                           (GtkWidget       *widget,
                                                                        cairo_t         *cr);

static void     gcal_floating_container_add                            (GtkContainer    *container,
                                                                        GtkWidget       *child);

static void     gcal_floating_container_get_padding_and_border         (GtkWidget       *widget,
                                                                        GtkBorder       *border);

G_DEFINE_TYPE(GcalFloatingContainer, gcal_floating_container, GTK_TYPE_BIN)

static void
gcal_floating_container_class_init (GcalFloatingContainerClass *klass)
{
  GtkWidgetClass* widget_class;
  GtkContainerClass *container_class;

  widget_class = GTK_WIDGET_CLASS (klass);
  container_class = GTK_CONTAINER_CLASS (klass);

  widget_class->get_preferred_width =
      gcal_floating_container_get_preferred_width;
  widget_class->get_preferred_height_for_width =
      gcal_floating_container_get_preferred_height_for_width;
  widget_class->get_preferred_height =
      gcal_floating_container_get_preferred_height;
  widget_class->get_preferred_width_for_height =
      gcal_floating_container_get_preferred_width_for_height;
  widget_class->size_allocate = gcal_floating_container_size_allocate;
  widget_class->draw = gcal_floating_container_draw;

  container_class->add = gcal_floating_container_add;

  g_type_class_add_private ((gpointer)klass, sizeof (GcalFloatingContainerPrivate));
}



static void
gcal_floating_container_init (GcalFloatingContainer *self)
{
  gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            GCAL_TYPE_FLOATING_CONTAINER,
                                            GcalFloatingContainerPrivate);
}

static void
gcal_floating_container_get_preferred_width (GtkWidget *widget,
                                             gint      *minimum_width,
                                             gint      *natural_width)
{
  gint child_min, child_nat;
  GtkWidget *child;
  GtkBorder padding;
  gint minimum, natural;

  gcal_floating_container_get_padding_and_border (widget, &padding);

  minimum = 0;
  natural = 0;

  child = gtk_bin_get_child (GTK_BIN (widget));
  if (child && gtk_widget_get_visible (child))
    {
      gtk_widget_get_preferred_width (child,
                                      &child_min, &child_nat);
      minimum += child_min;
      natural += child_nat;
    }

  minimum += padding.left + padding.right;
  natural += padding.left + padding.right;

 if (minimum_width)
    *minimum_width = minimum;

  if (natural_width)
    *natural_width = natural;
}

static void
gcal_floating_container_get_preferred_height_for_width (GtkWidget *widget,
                                                        gint       width,
                                                        gint      *minimum_height,
                                                        gint      *natural_height)
{
  gint child_min, child_nat, child_width;
  GtkWidget *child;
  GtkBorder padding;
  gint minimum;
  gint natural;

  gcal_floating_container_get_padding_and_border (widget, &padding);

  minimum = 0;
  natural = 0;

  child = gtk_bin_get_child (GTK_BIN (widget));
  if (child && gtk_widget_get_visible (child))
    {
      child_width = width - padding.left - padding.top;

      gtk_widget_get_preferred_height_for_width (child, child_width,
                                                 &child_min, &child_nat);
      minimum = MAX (minimum, child_min);
      natural = MAX (natural, child_nat);
    }

  minimum += padding.top + padding.top;
  natural += padding.top + padding.top;

 if (minimum_height)
    *minimum_height = minimum;

  if (natural_height)
    *natural_height = natural;
}

static void
gcal_floating_container_get_preferred_height (GtkWidget *widget,
                                              gint      *minimum_height,
                                              gint      *natural_height)
{
  gint child_min, child_nat;
  GtkWidget *child;
  GtkBorder padding;
  gint minimum;
  gint natural;

  gcal_floating_container_get_padding_and_border (widget, &padding);

  minimum = 0;
  natural = 0;

  child = gtk_bin_get_child (GTK_BIN (widget));
  if (child && gtk_widget_get_visible (child))
    {
      gtk_widget_get_preferred_height (child,
                                       &child_min, &child_nat);
      minimum = child_min;
      natural = child_nat;
    }

  minimum += padding.top + padding.top;
  natural += padding.top + padding.top;

 if (minimum_height)
    *minimum_height = minimum;

  if (natural_height)
    *natural_height = natural;
}

static void
gcal_floating_container_get_preferred_width_for_height (GtkWidget *widget,
                                                        gint       height,
                                                        gint      *minimum_width,
                                                        gint      *natural_width)
{
  gint child_min, child_nat, child_height;
  GtkWidget *child;
  GtkBorder padding;
  gint minimum;
  gint natural;

  gcal_floating_container_get_padding_and_border (widget, &padding);

  minimum = 0;
  natural = 0;

  child_height = height - padding.top - padding.bottom;

  child = gtk_bin_get_child (GTK_BIN (widget));
  if (child && gtk_widget_get_visible (child))
    {
      gtk_widget_get_preferred_width_for_height (child, child_height,
                                                 &child_min, &child_nat);
      minimum += child_min;
      natural += child_nat;
    }

  minimum += padding.left + padding.right;
  natural += padding.left + padding.right;

 if (minimum_width)
    *minimum_width = minimum;

  if (natural_width)
    *natural_width = natural;
}

static void
gcal_floating_container_size_allocate (GtkWidget     *widget,
                                       GtkAllocation *allocation)
{
  GtkAllocation child_allocation;
  GtkBorder padding;
  GtkWidget *child;

  gtk_widget_set_allocation (widget, allocation);

  gcal_floating_container_get_padding_and_border (widget, &padding);

  if (gtk_widget_get_realized (widget))
    {
      gdk_window_move_resize (gtk_widget_get_window (widget),
                              allocation->x,
                              allocation->y,
                              allocation->width,
                              allocation->height);
    }

  child_allocation.x = padding.left;
  child_allocation.y = padding.top;
  child_allocation.height =
      MAX (1,
           allocation->height - padding.top - padding.bottom);

  child_allocation.width =
      MAX (1,
           allocation->width - padding.left - padding.right);

  child = gtk_bin_get_child (GTK_BIN (widget));
  if (child && gtk_widget_get_visible (child))
    gtk_widget_size_allocate (child, &child_allocation);
}

static gboolean
gcal_floating_container_draw (GtkWidget *widget,
                              cairo_t   *cr)
{
  GtkStyleContext *context;
  guint border_width;

  context = gtk_widget_get_style_context (widget);
  border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));

  gtk_render_background (context,  cr,
                         border_width, border_width,
                         gtk_widget_get_allocated_width (widget) - 2 * border_width,
                         gtk_widget_get_allocated_height (widget) - 2 * border_width);
  gtk_render_frame (context, cr,
                    border_width, border_width,
                    gtk_widget_get_allocated_width (widget) - 2 * border_width,
                    gtk_widget_get_allocated_height (widget) - 2 * border_width);
  if (GTK_WIDGET_CLASS (gcal_floating_container_parent_class)->draw != NULL)
    GTK_WIDGET_CLASS (gcal_floating_container_parent_class)->draw (widget, cr);

  return FALSE;
}

static void
gcal_floating_container_add (GtkContainer *container,
                             GtkWidget    *child)
{
  g_return_if_fail (gtk_bin_get_child (GTK_BIN (container)) == NULL);

  GTK_CONTAINER_CLASS (gcal_floating_container_parent_class)->add (container, child);
}

static void
gcal_floating_container_get_padding_and_border (GtkWidget *widget,
                                                GtkBorder *border)
{
  GtkStyleContext *context;
  GtkStateFlags state;
  GtkBorder tmp;

  guint border_width;

  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);

  gtk_style_context_get_padding (context, state, border);
  border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));

  gtk_style_context_get_border (context, state, &tmp);
  border->top += tmp.top + border_width;
  border->right += tmp.right + border_width;
  border->bottom += tmp.bottom + border_width;
  border->left += tmp.left + border_width;
}

GtkWidget*
gcal_floating_container_new (void)
{
  return g_object_new (GCAL_TYPE_FLOATING_CONTAINER, NULL);
}
