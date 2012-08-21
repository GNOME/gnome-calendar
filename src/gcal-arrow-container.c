/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 2 -*- */
/*
 * gcal-arrow-container.c
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

#include "gcal-arrow-container.h"
#include <math.h>

struct _GcalArrowContainerPrivate
{
  guint arrow_size;
};

static void     gcal_arrow_container_get_preferred_width            (GtkWidget       *widget,
                                                                     gint            *minimum_width,
                                                                     gint            *natural_width);

static void     gcal_arrow_container_get_preferred_height_for_width (GtkWidget       *widget,
                                                                     gint             width,
                                                                     gint            *minimum_height,
                                                                     gint            *natural_height);

static void     gcal_arrow_container_get_preferred_height           (GtkWidget       *widget,
                                                                     gint            *minimum_height,
                                                                     gint            *natural_height);

static void     gcal_arrow_container_get_preferred_width_for_height (GtkWidget       *widget,
                                                                     gint             height,
                                                                     gint            *minimum_width,
                                                                     gint            *natural_width);

static void     gcal_arrow_container_size_allocate                  (GtkWidget       *widget,
                                                                     GtkAllocation   *allocation);

static gboolean gcal_arrow_container_draw                           (GtkWidget       *widget,
                                                                     cairo_t         *cr);

static void     gcal_arrow_container_draw_arrow                     (GtkWidget       *widget,
                                                                     cairo_t         *cr);

static void     gcal_arrow_container_draw_shadow                    (GtkWidget       *widget,
                                                                     cairo_t         *cr);

static void     gcal_arrow_container_get_padding_and_border         (GtkWidget       *widget,
                                                                     GtkBorder       *border,
                                                                     guint           *border_width,
                                                                     guint           *arrow_size,
                                                                     guint           *shadow);

G_DEFINE_TYPE(GcalArrowContainer, gcal_arrow_container, GTK_TYPE_BIN)

static void
gcal_arrow_container_class_init (GcalArrowContainerClass *klass)
{
  GtkWidgetClass* widget_class;

  widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->get_preferred_width =
      gcal_arrow_container_get_preferred_width;
  widget_class->get_preferred_height_for_width =
      gcal_arrow_container_get_preferred_height_for_width;
  widget_class->get_preferred_height =
      gcal_arrow_container_get_preferred_height;
  widget_class->get_preferred_width_for_height =
      gcal_arrow_container_get_preferred_width_for_height;
  widget_class->size_allocate = gcal_arrow_container_size_allocate;
  widget_class->draw = gcal_arrow_container_draw;

  /* Style properties */
  gtk_widget_class_install_style_property (
      widget_class,
      g_param_spec_uint ("arrow-size",
                         "Arrow width and height",
                         "The width and height of the arrow",
                         0,
                         G_MAXUINT32,
                         0,
                         G_PARAM_READABLE));

  gtk_widget_class_install_style_property (
      widget_class,
      g_param_spec_uint ("shadow-span",
                         "Shadow span",
                         "Shadow span",
                         0,
                         G_MAXUINT32,
                         0,
                         G_PARAM_READABLE));

  g_type_class_add_private ((gpointer)klass, sizeof(GcalArrowContainerPrivate));
}

static void
gcal_arrow_container_init (GcalArrowContainer *self)
{
  gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            GCAL_TYPE_ARROW_CONTAINER,
                                            GcalArrowContainerPrivate);
}

static void
gcal_arrow_container_get_preferred_width (GtkWidget *widget,
                                          gint      *minimum_width,
                                          gint      *natural_width)
{
  gint child_min, child_nat;
  GtkWidget *child;
  GtkBorder padding;
  gint minimum, natural;
  guint shadow;

  gcal_arrow_container_get_padding_and_border (widget,
                                               &padding,
                                               NULL,
                                               NULL,
                                               &shadow);

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

  minimum += padding.left + padding.right + 2 * shadow;
  natural += padding.left + padding.right + 2 * shadow;

 if (minimum_width)
    *minimum_width = minimum;

  if (natural_width)
    *natural_width = natural;
}

static void
gcal_arrow_container_get_preferred_height_for_width (GtkWidget *widget,
                                                     gint       width,
                                                     gint      *minimum_height,
                                                     gint      *natural_height)
{
  gint child_min, child_nat, child_width;
  GtkWidget *child;
  GtkBorder padding;
  guint shadow;
  guint arrow_size;
  gint minimum;
  gint natural;

  gcal_arrow_container_get_padding_and_border (widget,
                                               &padding,
                                               NULL,
                                               &arrow_size,
                                               &shadow);

  minimum = 0;
  natural = 0;

  child = gtk_bin_get_child (GTK_BIN (widget));
  if (child && gtk_widget_get_visible (child))
    {
      child_width = width - padding.left - padding.top - 2 * shadow;

      gtk_widget_get_preferred_height_for_width (child, child_width,
                                                 &child_min, &child_nat);
      minimum = MAX (minimum, child_min);
      natural = MAX (natural, child_nat);
    }

  minimum += padding.top + padding.bottom + arrow_size + 2 * shadow;
  natural += padding.top + padding.bottom + arrow_size + 2 * shadow;

 if (minimum_height)
    *minimum_height = minimum;

  if (natural_height)
    *natural_height = natural;
}

static void
gcal_arrow_container_get_preferred_height (GtkWidget *widget,
                                           gint      *minimum_height,
                                           gint      *natural_height)
{
  gint child_min, child_nat;
  GtkWidget *child;
  GtkBorder padding;
  guint shadow;
  guint arrow_size;
  gint minimum;
  gint natural;

  gcal_arrow_container_get_padding_and_border (widget,
                                               &padding,
                                               NULL,
                                               &arrow_size,
                                               &shadow);

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

  minimum += padding.top + padding.bottom + arrow_size + 2 * shadow;
  natural += padding.top + padding.bottom + arrow_size + 2 * shadow;

 if (minimum_height)
    *minimum_height = minimum;

  if (natural_height)
    *natural_height = natural;
}

static void
gcal_arrow_container_get_preferred_width_for_height (GtkWidget *widget,
                                                     gint       height,
                                                     gint      *minimum_width,
                                                     gint      *natural_width)
{
  gint child_min, child_nat, child_height;
  GtkWidget *child;
  GtkBorder padding;
  guint shadow;
  gint minimum;
  gint natural;

  gcal_arrow_container_get_padding_and_border (widget,
                                               &padding,
                                               NULL,
                                               NULL,
                                               &shadow);

  minimum = 0;
  natural = 0;

  child_height = height - padding.top - padding.bottom - 2 * shadow;

  child = gtk_bin_get_child (GTK_BIN (widget));
  if (child && gtk_widget_get_visible (child))
    {
      gtk_widget_get_preferred_width_for_height (child, child_height,
                                                 &child_min, &child_nat);
      minimum += child_min;
      natural += child_nat;
    }

  minimum += padding.left + padding.right + 2 * shadow;
  natural += padding.left + padding.right + 2 * shadow;

 if (minimum_width)
    *minimum_width = minimum;

  if (natural_width)
    *natural_width = natural;
}

static void
gcal_arrow_container_size_allocate (GtkWidget     *widget,
                                    GtkAllocation *allocation)
{
  GtkAllocation child_allocation;
  GtkBorder padding;
  GtkWidget *child;
  guint arrow_size;
  guint shadow;

  gtk_widget_set_allocation (widget, allocation);

  gcal_arrow_container_get_padding_and_border (widget,
                                               &padding,
                                               NULL,
                                               &arrow_size,
                                               &shadow);

  if (gtk_widget_get_realized (widget))
    {
      gdk_window_move_resize (gtk_widget_get_window (widget),
                              allocation->x,
                              allocation->y,
                              allocation->width,
                              allocation->height);
    }

  child_allocation.x = padding.left + shadow;
  child_allocation.y = padding.top + shadow;
  child_allocation.height =
      MAX (1,
           allocation->height - padding.top - padding.bottom - 2 * shadow - arrow_size);

  child_allocation.width =
      MAX (1,
           allocation->width - padding.left - padding.right - 2 * shadow);

  child = gtk_bin_get_child (GTK_BIN (widget));
  if (child && gtk_widget_get_visible (child))
    gtk_widget_size_allocate (child, &child_allocation);
}

static gboolean
gcal_arrow_container_draw (GtkWidget *widget,
                           cairo_t   *cr)
{
  GtkBorder border;
  guint border_width;
  guint width;
  guint height;
  guint arrow_size;
  guint shadow;

  gtk_style_context_get_border (
      gtk_widget_get_style_context (widget),
      gtk_widget_get_state_flags (widget),
      &border);

  gcal_arrow_container_get_padding_and_border (widget,
                                               NULL,
                                               &border_width,
                                               &arrow_size,
                                               &shadow);

  width = gtk_widget_get_allocated_width (widget) - 2 * border_width - 2 * shadow;
  height = gtk_widget_get_allocated_height (widget) - 2 * border_width - arrow_size - 2 * shadow;

  gcal_arrow_container_draw_shadow (widget, cr);

  gtk_render_background (gtk_widget_get_style_context (widget),
                         cr,
                         border_width + shadow, border_width + shadow,
                         width, height);
  gtk_render_frame_gap (gtk_widget_get_style_context (widget),
                        cr,
                        border_width + shadow, border_width + shadow,
                        width, height,
                        GTK_POS_BOTTOM,
                        width / 2 - arrow_size - border.bottom,
                        width / 2 + arrow_size + border.bottom);

  gcal_arrow_container_draw_arrow (widget, cr);

  GTK_WIDGET_CLASS (gcal_arrow_container_parent_class)->draw (widget, cr);

  return FALSE;
}

static void
gcal_arrow_container_draw_arrow (GtkWidget *widget,
                                 cairo_t   *cr)
{
  GdkRGBA color;
  GtkBorder border;
  guint arrow_size;
  guint border_width;
  guint shadow;
  guint width;
  guint height;

  gtk_style_context_get_border (
      gtk_widget_get_style_context (widget),
      gtk_widget_get_state_flags (widget),
      &border);

  gcal_arrow_container_get_padding_and_border (widget,
                                               NULL,
                                               &border_width,
                                               &arrow_size,
                                               &shadow);

  width = gtk_widget_get_allocated_width (widget) - 2 * border_width - 2 * shadow;
  height = gtk_widget_get_allocated_height (widget) - 2 * border_width - arrow_size - 2 * shadow;

  cairo_save (cr);
  cairo_move_to (cr,
                 border_width + shadow + width / 2 - arrow_size,
                 border_width + shadow + height - border.bottom);
  cairo_rel_line_to (cr, 0, border.bottom);
  cairo_rel_line_to (cr, arrow_size, arrow_size);
  cairo_rel_line_to (cr, arrow_size, - arrow_size);
  cairo_rel_line_to (cr, 0, - border.bottom);

  gtk_style_context_get_background_color (
      gtk_widget_get_style_context (widget),
      gtk_widget_get_state_flags (widget),
      &color);
  cairo_set_source_rgba (cr, color.red, color.green, color.blue, color.alpha);
  cairo_fill (cr);

  cairo_set_line_width (cr, border.bottom);
  cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
  cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND);
  cairo_move_to (cr,
                 border_width + shadow + width / 2 - arrow_size,
                 border_width + shadow + height);
  cairo_rel_line_to (cr, arrow_size, arrow_size);
  cairo_rel_line_to (cr, arrow_size, - arrow_size);

  gtk_style_context_get_border_color (
      gtk_widget_get_style_context (widget),
      gtk_widget_get_state_flags (widget),
      &color);
  cairo_set_source_rgb (cr, color.red, color.green, color.blue);
  cairo_stroke (cr);
  cairo_restore (cr);
}

static void
gcal_arrow_container_draw_shadow (GtkWidget *widget,
                                  cairo_t   *cr)
{
  cairo_pattern_t *pattern;
  gdouble inner_alpha;
  gint width;
  gint height;
  guint arrow_size;
  guint shadow_span;
  guint border_width;
  gdouble shadow_span_tan;

  gcal_arrow_container_get_padding_and_border (widget,
                                               NULL,
                                               &border_width,
                                               &arrow_size,
                                               &shadow_span);

  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);
  shadow_span_tan = shadow_span / tan (G_PI_4 / 2 + G_PI_4);

  inner_alpha = 0.8;

  cairo_save (cr);

  /* Top border */
  pattern = cairo_pattern_create_linear (0, border_width,
                                        0, border_width + 3 * shadow_span);

  cairo_pattern_add_color_stop_rgba (pattern, 0.0, 0, 0, 0, 0.0);
  cairo_pattern_add_color_stop_rgba (pattern, 1.0, 0, 0, 0, inner_alpha);

  cairo_set_source (cr, pattern);
  cairo_pattern_destroy (pattern);

  cairo_rectangle (cr,
                  border_width + shadow_span, border_width,
                  width - 2 * border_width - 2 * shadow_span, shadow_span);
  cairo_fill (cr);

  /* Left border */
  pattern = cairo_pattern_create_linear (border_width, 0,
                                        border_width + 3 * shadow_span, 0);

  cairo_pattern_add_color_stop_rgba (pattern, 0.0, 0, 0, 0, 0.0);
  cairo_pattern_add_color_stop_rgba (pattern, 1.0, 0, 0, 0, inner_alpha);

  cairo_set_source (cr, pattern);
  cairo_pattern_destroy (pattern);

  cairo_rectangle (cr,
                  border_width, border_width + shadow_span,
                  shadow_span, height - 2 * border_width - 2 * shadow_span - arrow_size);
  cairo_fill (cr);

  /* Right border */
  pattern = cairo_pattern_create_linear (width - (border_width + 3 * shadow_span), 0,
                                        width - border_width, 0);

  cairo_pattern_add_color_stop_rgba (pattern, 0.0, 0, 0, 0, inner_alpha);
  cairo_pattern_add_color_stop_rgba (pattern, 1.0, 0, 0, 0, 0.0);

  cairo_set_source (cr, pattern);
  cairo_pattern_destroy (pattern);

  cairo_rectangle (cr,
                  width - (border_width + shadow_span), border_width + shadow_span,
                  shadow_span, height - 2 * border_width - 2 * shadow_span - arrow_size);
  cairo_fill (cr);

  /* Bottom border */
  pattern = cairo_pattern_create_linear (0, height - border_width - 3 * shadow_span - arrow_size,
                                        0, height - border_width - arrow_size);
  cairo_pattern_add_color_stop_rgba (pattern, 0.0, 0, 0, 0, inner_alpha);
  cairo_pattern_add_color_stop_rgba (pattern, 1.0, 0, 0, 0, 0.0);

  cairo_set_source (cr, pattern);
  cairo_pattern_destroy (pattern);

  cairo_rectangle (cr,
                  border_width + shadow_span, height - border_width - shadow_span - arrow_size,
                  width / 2 - arrow_size - border_width - shadow_span - shadow_span_tan, shadow_span);
  cairo_rectangle (cr,
                  width / 2 + arrow_size + shadow_span_tan, height - border_width - shadow_span - arrow_size,
                  width / 2 - arrow_size - border_width - shadow_span - shadow_span_tan, shadow_span);
  cairo_fill (cr);

  /* NW corner */
  pattern = cairo_pattern_create_radial (
      border_width + shadow_span, border_width + shadow_span, 0,
      border_width + shadow_span, border_width + shadow_span, shadow_span);

  cairo_pattern_add_color_stop_rgba (pattern, 0.0, 0, 0, 0, inner_alpha / 3);
  cairo_pattern_add_color_stop_rgba (pattern, 1.0, 0, 0, 0, 0.0);

  cairo_set_source (cr, pattern);
  cairo_pattern_destroy (pattern);

  cairo_rectangle (cr,
                  border_width, border_width,
                  shadow_span, shadow_span);
  cairo_fill (cr);

  /* SE corner */
  pattern = cairo_pattern_create_radial (
      border_width + shadow_span, height - border_width - shadow_span - arrow_size, 0,
      border_width + shadow_span, height - border_width - shadow_span - arrow_size, shadow_span);

  cairo_pattern_add_color_stop_rgba (pattern, 0.0, 0, 0, 0, inner_alpha / 3);
  cairo_pattern_add_color_stop_rgba (pattern, 1.0, 0, 0, 0, 0.0);

  cairo_set_source (cr, pattern);
  cairo_pattern_destroy (pattern);

  cairo_rectangle (cr,
                  border_width, height - border_width - shadow_span - arrow_size,
                  shadow_span, shadow_span);
  cairo_fill (cr);

  /* SW corner */
  pattern = cairo_pattern_create_radial (
      width - border_width - shadow_span, height - border_width - shadow_span - arrow_size, 0,
      width - border_width - shadow_span, height - border_width - shadow_span - arrow_size, shadow_span);

  cairo_pattern_add_color_stop_rgba (pattern, 0.0, 0, 0, 0, inner_alpha / 3);
  cairo_pattern_add_color_stop_rgba (pattern, 1.0, 0, 0, 0, 0.0);

  cairo_set_source (cr, pattern);
  cairo_pattern_destroy (pattern);

  cairo_rectangle (cr,
                  width - border_width - shadow_span, height - border_width - shadow_span - arrow_size,
                  shadow_span, shadow_span);
  cairo_fill (cr);

  /* Arrow left joint */
  pattern = cairo_pattern_create_radial (
      width / 2 - arrow_size - shadow_span_tan, height - border_width - arrow_size, 0,
      width / 2 - arrow_size - shadow_span_tan, height - border_width - arrow_size, shadow_span);

  cairo_pattern_add_color_stop_rgba (pattern, 0.0, 0, 0, 0, 0.0);
  cairo_pattern_add_color_stop_rgba (pattern, 1.0, 0, 0, 0, inner_alpha / 3);

  cairo_set_source (cr, pattern);
  cairo_pattern_destroy (pattern);

  cairo_move_to (cr,
                 width / 2 - arrow_size - shadow_span_tan,
                 height - border_width - arrow_size);
  cairo_arc (cr,
             width / 2 - arrow_size - shadow_span_tan,
             height - border_width - arrow_size,
             shadow_span + 2,
             G_PI_2 + G_PI,
             2 * G_PI - G_PI_4);

  cairo_fill (cr);

  /* Arrow right joint */
  pattern = cairo_pattern_create_radial (
      width / 2 + arrow_size + shadow_span_tan, height - border_width - arrow_size, 0,
      width / 2 + arrow_size + shadow_span_tan, height - border_width - arrow_size, shadow_span);

  cairo_pattern_add_color_stop_rgba (pattern, 0.0, 0, 0, 0, 0.0);
  cairo_pattern_add_color_stop_rgba (pattern, 1.0, 0, 0, 0, inner_alpha / 3);

  cairo_set_source (cr, pattern);
  cairo_pattern_destroy (pattern);

  cairo_move_to (cr,
                 width / 2 + arrow_size + shadow_span_tan,
                 height - border_width - arrow_size);
  cairo_arc (cr,
             width / 2 + arrow_size + shadow_span_tan,
             height - border_width - arrow_size,
             shadow_span + 2,
             G_PI_4 + G_PI,
             G_PI + G_PI_2);

  cairo_fill (cr);

  /* Arrow left side */
  cairo_save (cr);
  cairo_translate (cr,
                   width / 2 - arrow_size,
                   height - border_width - shadow_span - arrow_size);
  cairo_rotate(cr, G_PI_4);

  pattern = cairo_pattern_create_linear (
      0, 0,
      0, shadow_span);

  cairo_pattern_add_color_stop_rgba (pattern, 0.0, 0, 0, 0, inner_alpha / 3);
  cairo_pattern_add_color_stop_rgba (pattern, 1.0, 0, 0, 0, 0.0);

  cairo_set_source (cr, pattern);
  cairo_pattern_destroy (pattern);

  cairo_rectangle (cr,
                   shadow_span_tan, 0,
                   arrow_size * G_SQRT2 - shadow_span_tan, shadow_span);

  cairo_fill (cr);
  cairo_restore (cr);

  /* Arrow right side */
  cairo_save (cr);
  cairo_translate (cr,
                   width / 2,
                   height - border_width - shadow_span);
  cairo_rotate (cr, - G_PI_4);

  pattern = cairo_pattern_create_linear (
      0, 0,
      0, shadow_span);

  cairo_pattern_add_color_stop_rgba (pattern, 0.0, 0, 0, 0, inner_alpha / 3);
  cairo_pattern_add_color_stop_rgba (pattern, 1.0, 0, 0, 0, 0.0);

  cairo_set_source (cr, pattern);
  cairo_pattern_destroy (pattern);

  cairo_rectangle (cr,
                   0,
                   0,
                   arrow_size * G_SQRT2 - shadow_span_tan,
                   shadow_span);

  cairo_fill (cr);
  cairo_restore (cr);

  /* Arrow point */
  pattern = cairo_pattern_create_radial (
      width / 2, height - border_width - shadow_span, 0,
      width / 2, height - border_width - shadow_span, shadow_span);

  cairo_pattern_add_color_stop_rgba (pattern, 0.0, 0, 0, 0, inner_alpha / 3);
  cairo_pattern_add_color_stop_rgba (pattern, 1.0, 0, 0, 0, 0.0);

  cairo_set_source (cr, pattern);
  cairo_pattern_destroy (pattern);

  cairo_move_to (cr,
                 width / 2,
                 height - border_width - shadow_span);
  cairo_arc (cr,
             width / 2,
             height - border_width - shadow_span,
             shadow_span,
             G_PI_4,
             G_PI_2 + G_PI_4);
  cairo_fill (cr);

  cairo_restore (cr);
}

static void
gcal_arrow_container_get_padding_and_border (GtkWidget *widget,
                                             GtkBorder *border,
                                             guint     *border_width,
                                             guint     *arrow_size,
                                             guint     *shadow)
{
  GtkBorder tmp;
  guint bd_width;

  bd_width = gtk_container_get_border_width (GTK_CONTAINER (widget));
  if (border != NULL)
    {
      gtk_style_context_get_padding (
          gtk_widget_get_style_context (widget),
          gtk_widget_get_state_flags (widget),
          border);

      gtk_style_context_get_border (
          gtk_widget_get_style_context (widget),
          gtk_widget_get_state_flags (widget),
          &tmp);

      border->top += tmp.top + bd_width;
      border->right += tmp.right + bd_width;
      border->bottom += tmp.bottom + bd_width;
      border->left += tmp.left + bd_width;
    }

  if (arrow_size != NULL)
    {
      gtk_style_context_get_style (
          gtk_widget_get_style_context (widget),
          "arrow-size", arrow_size,
          NULL);
    }

  if (border_width != NULL)
    *border_width = bd_width;

  if (shadow != NULL)
    {
      gtk_style_context_get_style (
          gtk_widget_get_style_context (widget),
          "shadow-span", shadow,
          NULL);
    }
}

GtkWidget*
gcal_arrow_container_new (void)
{
  return g_object_new (GCAL_TYPE_ARROW_CONTAINER, NULL);
}
