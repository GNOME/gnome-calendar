/* -*- mode: c; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gcal-month-view.c
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

#include "gcal-month-view.h"
#include "gcal-utils.h"
#include "gcal-view.h"
#include "gcal-event-widget.h"

#include <glib/gi18n.h>

#include <libecal/libecal.h>

#include <math.h>

typedef struct
{
  /**
   * This is where we keep the refs of the child widgets.
   * Every child added to the list placed in the position
   * of it corresponding month day.
   * The cell number of every month day is calculated elsewhere.
   */
  GList          *days [31];

  GdkWindow      *event_window;

  /* the day of week for first day of the month,
   * sun: 0, mon: 1, ... sat = 6 */
  gint            days_delay;

  /* button_down/up flag */
  gint            clicked_cell;
  gint            start_mark_cell;
  gint            end_mark_cell;

  /* property */
  icaltimetype   *date;
} GcalMonthViewPrivate;

enum
{
  PROP_0,
  PROP_DATE  /* active-date inherited property */
};

static void           gcal_view_interface_init              (GcalViewIface  *iface);

static void           gcal_month_view_set_property          (GObject        *object,
                                                             guint           property_id,
                                                             const GValue   *value,
                                                             GParamSpec     *pspec);

static void           gcal_month_view_get_property          (GObject        *object,
                                                             guint           property_id,
                                                             GValue         *value,
                                                             GParamSpec     *pspec);

static void           gcal_month_view_finalize              (GObject        *object);

static void           gcal_month_view_realize               (GtkWidget      *widget);

static void           gcal_month_view_unrealize             (GtkWidget      *widget);

static void           gcal_month_view_map                   (GtkWidget      *widget);

static void           gcal_month_view_unmap                 (GtkWidget      *widget);

static void           gcal_month_view_size_allocate         (GtkWidget      *widget,
                                                             GtkAllocation  *allocation);

static gboolean       gcal_month_view_draw                  (GtkWidget      *widget,
                                                             cairo_t        *cr);

static gboolean       gcal_month_view_button_press          (GtkWidget      *widget,
                                                             GdkEventButton *event);

static gboolean       gcal_month_view_motion_notify_event   (GtkWidget      *widget,
                                                             GdkEventMotion *event);

static gboolean       gcal_month_view_button_release        (GtkWidget      *widget,
                                                             GdkEventButton *event);

static void           gcal_month_view_add                   (GtkContainer   *constainer,
                                                             GtkWidget      *widget);

static void           gcal_month_view_remove                (GtkContainer   *constainer,
                                                             GtkWidget      *widget);

static void           gcal_month_view_forall                (GtkContainer   *container,
                                                             gboolean        include_internals,
                                                             GtkCallback     callback,
                                                             gpointer        callback_data);

static gdouble        gcal_month_view_get_start_grid_y      (GtkWidget      *widget);

static icaltimetype*  gcal_month_view_get_initial_date      (GcalView       *view);

static icaltimetype*  gcal_month_view_get_final_date        (GcalView       *view);

static void           gcal_month_view_mark_current_unit     (GcalView       *view,
                                                             gint           *x,
                                                             gint           *y);

static void           gcal_month_view_clear_marks           (GcalView       *view);

static gchar*         gcal_month_view_get_left_header       (GcalView       *view);

static gchar*         gcal_month_view_get_right_header      (GcalView       *view);

static gboolean       gcal_month_view_draw_event            (GcalView       *view,
                                                             icaltimetype   *start_date,
                                                             icaltimetype   *end_date);

static GtkWidget*     gcal_month_view_get_by_uuid           (GcalView       *view,
                                                             const gchar    *uuid);

static void           gcal_month_view_clear                 (GcalView       *view);

G_DEFINE_TYPE_WITH_CODE (GcalMonthView,
                         gcal_month_view,
                         GTK_TYPE_CONTAINER,
                         G_ADD_PRIVATE (GcalMonthView)
                         G_IMPLEMENT_INTERFACE (GCAL_TYPE_VIEW,
                                                gcal_view_interface_init));


static void
gcal_month_view_class_init (GcalMonthViewClass *klass)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->set_property = gcal_month_view_set_property;
  object_class->get_property = gcal_month_view_get_property;
  object_class->finalize = gcal_month_view_finalize;

  widget_class = GTK_WIDGET_CLASS (klass);
  widget_class->realize = gcal_month_view_realize;
  widget_class->unrealize = gcal_month_view_unrealize;
  widget_class->map = gcal_month_view_map;
  widget_class->unmap = gcal_month_view_unmap;
  widget_class->size_allocate = gcal_month_view_size_allocate;
  widget_class->draw = gcal_month_view_draw;
  widget_class->button_press_event = gcal_month_view_button_press;
  widget_class->motion_notify_event = gcal_month_view_motion_notify_event;
  widget_class->button_release_event = gcal_month_view_button_release;

  container_class = GTK_CONTAINER_CLASS (klass);
  container_class->add = gcal_month_view_add;
  container_class->remove = gcal_month_view_remove;
  container_class->forall = gcal_month_view_forall;

  g_object_class_override_property (object_class, PROP_DATE, "active-date");
}



static void
gcal_month_view_init (GcalMonthView *self)
{
  GcalMonthViewPrivate *priv;
  gint i;

  gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);

  priv = gcal_month_view_get_instance_private (self);

  priv->clicked_cell = -1;

  priv->start_mark_cell = -1;
  priv->end_mark_cell = -1;

  for (i = 0; i < 31; i++)
    {
      priv->days[i] = NULL;
    }

  gtk_style_context_add_class (
      gtk_widget_get_style_context (GTK_WIDGET (self)),
      "calendar-view");
}

static void
gcal_view_interface_init (GcalViewIface *iface)
{
  iface->get_initial_date = gcal_month_view_get_initial_date;
  iface->get_final_date = gcal_month_view_get_final_date;

  iface->mark_current_unit = gcal_month_view_mark_current_unit;
  iface->clear_marks = gcal_month_view_clear_marks;

  iface->get_left_header = gcal_month_view_get_left_header;
  iface->get_right_header = gcal_month_view_get_right_header;

  iface->draw_event = gcal_month_view_draw_event;
  iface->get_by_uuid = gcal_month_view_get_by_uuid;
  iface->clear = gcal_month_view_clear;
}

static void
gcal_month_view_set_property (GObject       *object,
                              guint          property_id,
                              const GValue  *value,
                              GParamSpec    *pspec)
{
  GcalMonthViewPrivate *priv;

  priv = gcal_month_view_get_instance_private (GCAL_MONTH_VIEW (object));

  switch (property_id)
    {
    case PROP_DATE:
      {
        icaltimetype *first_of_month;

        if (priv->date != NULL)
          g_free (priv->date);

        priv->date = g_value_dup_boxed (value);

        first_of_month = gcal_dup_icaltime (priv->date);
        first_of_month->day = 1;
        priv->days_delay = icaltime_day_of_week (*first_of_month) - 1;
        g_free (first_of_month);

        break;
      }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gcal_month_view_get_property (GObject       *object,
                              guint          property_id,
                              GValue        *value,
                              GParamSpec    *pspec)
{
  GcalMonthViewPrivate *priv;

  priv = gcal_month_view_get_instance_private (GCAL_MONTH_VIEW (object));

  switch (property_id)
    {
    case PROP_DATE:
      g_value_set_boxed (value, priv->date);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gcal_month_view_finalize (GObject       *object)
{
  GcalMonthViewPrivate *priv;
  priv = gcal_month_view_get_instance_private (GCAL_MONTH_VIEW (object));

  if (priv->date != NULL)
    g_free (priv->date);

  /* Chain up to parent's finalize() method. */
  G_OBJECT_CLASS (gcal_month_view_parent_class)->finalize (object);
}

static void
gcal_month_view_realize (GtkWidget *widget)
{
  GcalMonthViewPrivate *priv;
  GdkWindow *parent_window;
  GdkWindowAttr attributes;
  gint attributes_mask;
  GtkAllocation allocation;

  priv = gcal_month_view_get_instance_private (GCAL_MONTH_VIEW (widget));
  gtk_widget_set_realized (widget, TRUE);

  parent_window = gtk_widget_get_parent_window (widget);
  gtk_widget_set_window (widget, parent_window);
  g_object_ref (parent_window);

  gtk_widget_get_allocation (widget, &allocation);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.x = allocation.x;
  attributes.y = allocation.y;
  attributes.width = allocation.width;
  attributes.height = allocation.height;
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= (GDK_BUTTON_PRESS_MASK |
                            GDK_BUTTON_RELEASE_MASK |
                            GDK_BUTTON1_MOTION_MASK |
                            GDK_POINTER_MOTION_HINT_MASK |
                            GDK_POINTER_MOTION_MASK |
                            GDK_ENTER_NOTIFY_MASK |
                            GDK_LEAVE_NOTIFY_MASK);
  attributes_mask = GDK_WA_X | GDK_WA_Y;

  priv->event_window = gdk_window_new (parent_window,
                                       &attributes,
                                       attributes_mask);
  gdk_window_set_user_data (priv->event_window, widget);
}

static void
gcal_month_view_unrealize (GtkWidget *widget)
{
  GcalMonthViewPrivate *priv;

  priv = gcal_month_view_get_instance_private (GCAL_MONTH_VIEW (widget));
  if (priv->event_window != NULL)
    {
      gdk_window_set_user_data (priv->event_window, NULL);
      gdk_window_destroy (priv->event_window);
      priv->event_window = NULL;
    }

  GTK_WIDGET_CLASS (gcal_month_view_parent_class)->unrealize (widget);
}

static void
gcal_month_view_map (GtkWidget *widget)
{
  GcalMonthViewPrivate *priv;

  priv = gcal_month_view_get_instance_private (GCAL_MONTH_VIEW (widget));
  if (priv->event_window != NULL)
    gdk_window_show (priv->event_window);

  GTK_WIDGET_CLASS (gcal_month_view_parent_class)->map (widget);
}

static void
gcal_month_view_unmap (GtkWidget *widget)
{
  GcalMonthViewPrivate *priv;

  priv = gcal_month_view_get_instance_private (GCAL_MONTH_VIEW (widget));
  if (priv->event_window != NULL)
    gdk_window_hide (priv->event_window);

  GTK_WIDGET_CLASS (gcal_month_view_parent_class)->unmap (widget);
}

static void
gcal_month_view_size_allocate (GtkWidget     *widget,
                               GtkAllocation *allocation)
{
  GcalMonthViewPrivate *priv;
  gint i;
  GList *l;

  GtkBorder padding;
  PangoLayout *layout;
  PangoFontDescription *font_desc;

  gint font_height;
  gdouble added_height;

  gdouble start_grid_y;
  gdouble horizontal_block;
  gdouble vertical_block;
  gdouble vertical_cell_margin;

  gdouble days;
  gint shown_rows;
  gint february_gap;

  gdouble lines_gap;

  gdouble lines_gap_for_5;

  priv = gcal_month_view_get_instance_private (GCAL_MONTH_VIEW (widget));

  gtk_widget_set_allocation (widget, allocation);
  if (gtk_widget_get_realized (widget))
    {
      gdk_window_move_resize (priv->event_window,
                              allocation->x,
                              allocation->y,
                              allocation->width,
                              allocation->height);
    }

  gtk_style_context_get_padding (gtk_widget_get_style_context (widget),
                                 gtk_widget_get_state_flags (widget),
                                 &padding);
  layout = pango_layout_new (gtk_widget_get_pango_context (widget));

  gtk_style_context_get (gtk_widget_get_style_context (widget),
                         gtk_widget_get_state_flags (widget),
                         "font", &font_desc,
                         NULL);
  pango_layout_set_font_description (layout, font_desc);
  pango_layout_get_pixel_size (layout, NULL, &font_height);
  pango_font_description_free (font_desc);
  g_object_unref (layout);

  start_grid_y = gcal_month_view_get_start_grid_y (widget);
  horizontal_block = allocation->width / 7;
  vertical_block = (allocation->height - start_grid_y) / 6;

  days = priv->days_delay + icaltime_days_in_month (priv->date->month, priv->date->year);
  shown_rows = ceil (days / 7.0);
  february_gap = shown_rows == 4 ? 1 : 0;
  lines_gap = ((gdouble) (shown_rows + 1) / 2.0) + 0.5 - ceil (shown_rows / 2.0);
  lines_gap_for_5 = shown_rows == 5 ? lines_gap : 0;

  vertical_cell_margin = padding.top + font_height;
  for (i = 0; i < 31; i++)
    {
      added_height = 0;
      for (l = priv->days[i]; l != NULL; l = l->next)
        {
          GcalViewChild *child;
          gint pos_x;
          gint pos_y;
          gint min_height;
          gint natural_height;
          GtkAllocation child_allocation;

          child = (GcalViewChild*) l->data;

          pos_x = horizontal_block * ((i + priv->days_delay) % 7 );
          pos_y = vertical_block * (((i + priv->days_delay + 7 * february_gap ) / 7 ) + lines_gap_for_5);

          if ((! gtk_widget_get_visible (child->widget))
              && (! child->hidden))
            continue;

          gtk_widget_get_preferred_height (child->widget,
                                           &min_height,
                                           &natural_height);
          child_allocation.x = pos_x;
          child_allocation.y = start_grid_y + vertical_cell_margin + pos_y;
          child_allocation.width = horizontal_block;
          child_allocation.height = MIN (natural_height, vertical_block);
          if (added_height + vertical_cell_margin + child_allocation.height
              > vertical_block)
            {
              gtk_widget_hide (child->widget);
              child->hidden = TRUE;

              l = l->next;
              for (; l != NULL; l = l->next)
                {
                  child = (GcalViewChild*) l->data;

                  gtk_widget_hide (child->widget);
                  child->hidden = TRUE;
                }

              break;
            }
          else
            {
              gtk_widget_show (child->widget);
              child->hidden = FALSE;
              child_allocation.y = child_allocation.y + added_height;
              gtk_widget_size_allocate (child->widget, &child_allocation);
              added_height += child_allocation.height;
            }
        }
    }
}

static gboolean
gcal_month_view_draw (GtkWidget *widget,
                      cairo_t   *cr)
{
  GcalMonthViewPrivate *priv;

  GtkStyleContext *context;
  GtkStateFlags state;

  GtkBorder padding;
  GtkAllocation alloc;

  GdkRGBA color;
  GdkRGBA ligther_color;
  GdkRGBA selected_color;
  GdkRGBA background_selected_color;

  GtkBorder header_padding;

  gint i;
  gint font_width;
  gint font_height;

  gdouble start_grid_y;

  PangoLayout *layout;
  PangoFontDescription *font;
  PangoFontDescription *selected_font;
  PangoFontDescription *bold_font;

  gdouble days;
  gint shown_rows;

  gint february_gap;
  gint h_lines;
  gdouble lines_gap;
  gdouble lines_gap_for_5;

  gint lower_mark;
  gint upper_mark;

  gint pos_y;
  gint pos_x;

  priv = gcal_month_view_get_instance_private (GCAL_MONTH_VIEW (widget));

  /* fonts and colors selection */
  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);

  gtk_style_context_get_padding (context, state, &padding);
  gtk_widget_get_allocation (widget, &alloc);

  layout = pango_cairo_create_layout (cr);

  start_grid_y = gcal_month_view_get_start_grid_y (widget);
  gtk_style_context_get_color (context,
                               state | GTK_STATE_FLAG_SELECTED,
                               &selected_color);
  gtk_style_context_get (context, state, "font", &selected_font, NULL);

  gtk_style_context_get_background_color (context,
                                          state | GTK_STATE_FLAG_SELECTED,
                                          &background_selected_color);

  gtk_style_context_get_color (context,
                               state | GTK_STATE_FLAG_INSENSITIVE,
                               &ligther_color);
  gtk_style_context_get_color (context, state, &color);
  gtk_style_context_get (context, state, "font", &font, NULL);

  pango_layout_set_font_description (layout, font);

  gtk_style_context_save (context);
  gtk_style_context_add_region (context, "header", 0);

  gtk_style_context_get_padding (context, state, &header_padding);

  gtk_style_context_restore (context);

  /* calculations */
  days = priv->days_delay + icaltime_days_in_month (priv->date->month,
                                                    priv->date->year);
  shown_rows = ceil (days / 7.0);
  february_gap = shown_rows == 4 ? 1 : 0;

  h_lines = (shown_rows % 2) + 5;
  lines_gap = ((gdouble) (shown_rows + 1) / 2.0) + 0.5 - ceil (shown_rows / 2.0);

  lines_gap_for_5 = shown_rows == 5 ? lines_gap : 0;

  /* drawing grid text */
  bold_font = pango_font_description_copy (font);
  pango_font_description_set_weight (bold_font, PANGO_WEIGHT_SEMIBOLD);
  gdk_cairo_set_source_rgba (cr, &color);
  for (i = 0; i < 7; i++)
    {
      pango_layout_set_font_description (layout, bold_font);
      pango_layout_set_text (layout, gcal_get_weekday (i), -1);
      pango_cairo_update_layout (cr, layout);
      pango_layout_get_pixel_size (layout, &font_width, &font_height);

      cairo_move_to (cr,
                     (alloc.width / 7) * i + header_padding.left,
                     start_grid_y - padding.bottom - font_height);
      pango_cairo_show_layout (cr, layout);
    }

  g_debug ("drawing marks from %d => %d",
           priv->start_mark_cell,
           priv->end_mark_cell);

  if (priv->start_mark_cell != -1 &&
      priv->end_mark_cell != -1)
    {
      lower_mark = priv->start_mark_cell;
      upper_mark = priv->end_mark_cell;
      if (priv->start_mark_cell > priv->end_mark_cell)
        {
          lower_mark = priv->end_mark_cell;
          upper_mark = priv->start_mark_cell;
        }
    }
  else
    {
      lower_mark = 43;
      upper_mark = -2;
    }

  gdk_cairo_set_source_rgba (cr, &ligther_color);

  for (i = priv->days_delay + 7 * february_gap; i < days + 7 * february_gap; i++)
    {
      gchar *nr_day;
      gint column = i % 7;
      gint row = i / 7;
      gint nr_day_i = i - (priv->days_delay + 7 * february_gap) + 1;

      nr_day = g_strdup_printf ("%d", nr_day_i);

      /* drawing new-event marks */
      if (lower_mark <= i &&
          i <= upper_mark)
        {
          gdk_cairo_set_source_rgba (cr, &background_selected_color);
          pos_y = ((alloc.height - start_grid_y ) / 6) * (row + lines_gap_for_5) + start_grid_y;
          cairo_rectangle (cr,
                           (alloc.width / 7) * column,
                           pos_y + 0.3,
                           alloc.width / 7,
                           (alloc.height - start_grid_y ) / 6);
          cairo_fill (cr);
          gdk_cairo_set_source_rgba (cr, &ligther_color);

        }

      /* drawing selected_day */
      if (priv->date->day == nr_day_i)
        {
          gdk_cairo_set_source_rgba (cr, &selected_color);
          pango_layout_set_font_description ( layout, selected_font);
        }

      pango_layout_set_text (layout, nr_day, -1);
      pango_cairo_update_layout (cr, layout);
      pango_layout_get_pixel_size (layout, &font_width, &font_height);

      pos_y = ((alloc.height - start_grid_y ) / 6) * (row + lines_gap_for_5) + start_grid_y + padding.top;
      cairo_move_to (cr,
                     (alloc.width / 7) * column + header_padding.left,
                     pos_y + 0.3);

      pango_cairo_show_layout (cr, layout);

      if (priv->date->day == nr_day_i)
        {
          /* drawing current_unit cell */
          gdk_cairo_set_source_rgba (cr, &selected_color);

          /* Two pixel line on the selected day cell */
          cairo_set_line_width (cr, 2.0);
          pos_y = ((alloc.height - start_grid_y ) / 6) * (row + lines_gap_for_5) + start_grid_y;
          cairo_move_to (cr,
                         (alloc.width / 7) * column,
                         pos_y + 1);
          cairo_rel_line_to (cr, (alloc.width / 7), 0);
          cairo_stroke (cr);

          gdk_cairo_set_source_rgba (cr, &ligther_color);
          pango_layout_set_font_description (layout, font);
        }

      g_free (nr_day);
    }

  /* drawing grid skel */
  /* graying out month slices */
  gdk_cairo_set_source_rgba (cr, &background_selected_color);
  if (h_lines == 6)
    {
      cairo_rectangle (cr,
                       0, start_grid_y,
                       alloc.width, ((alloc.height - start_grid_y ) / 6) * 0.5);
      cairo_rectangle (cr,
                       0, start_grid_y + ((alloc.height - start_grid_y ) / 6) * (lines_gap + 5),
                       alloc.width, ((alloc.height - start_grid_y ) / 6) * 0.5);
    }
  /* graying out cells */
  /* initial gap */
  for (i = 0; i < priv->days_delay + 7 * february_gap; i++)
    {
      gint column = i % 7;
      gint row = i / 7;
      pos_y = ((alloc.height - start_grid_y ) / 6) * (row + lines_gap_for_5) + start_grid_y;
      cairo_rectangle (cr,
                       (alloc.width / 7) * column,
                       pos_y + 0.3,
                       alloc.width / 7, (alloc.height - start_grid_y ) / 6);
    }
  /* final gap */
  for (i = days + 7 * february_gap; i < 7 * (shown_rows == 5 ? 5 : 6); i++)
    {
      gint column = i % 7;
      gint row = i / 7;
      pos_y = ((alloc.height - start_grid_y ) / 6) * (row + lines_gap_for_5) + start_grid_y;
      cairo_rectangle (cr,
                       (alloc.width / 7) * column,
                       pos_y + 0.3,
                       alloc.width / 7, (alloc.height - start_grid_y ) / 6);
    }
  cairo_fill (cr);

  /* lines */
  gdk_cairo_set_source_rgba (cr, &ligther_color);
  cairo_set_line_width (cr, 0.4);

  /* vertical lines, the easy ones */
  for (i = 0; i < 6; i++)
    {
      pos_x = (alloc.width / 7) * (i + 1);
      cairo_move_to (cr, pos_x + 0.3, start_grid_y);
      cairo_rel_line_to (cr, 0, alloc.height - start_grid_y);
    }

  /* top and bottom horizontal lines */
  cairo_move_to (cr, 0, start_grid_y + 0.3);
  cairo_rel_line_to (cr, alloc.width, 0);

  /* drawing weeks lines */
  for (i = 0; i < h_lines; i++)
    {
      pos_y = ((alloc.height - start_grid_y) / 6) * (i + lines_gap) + start_grid_y;
      cairo_move_to (cr, 0, pos_y + 0.3);
      cairo_rel_line_to (cr, alloc.width, 0);
    }

  cairo_stroke (cr);

  pango_font_description_free (bold_font);
  pango_font_description_free (font);
  pango_font_description_free (selected_font);
  g_object_unref (layout);

  if (GTK_WIDGET_CLASS (gcal_month_view_parent_class)->draw != NULL)
    GTK_WIDGET_CLASS (gcal_month_view_parent_class)->draw (widget, cr);

  return FALSE;
}

static gboolean
gcal_month_view_button_press (GtkWidget      *widget,
                              GdkEventButton *event)
{
  GcalMonthViewPrivate *priv;
  gdouble x, y;
  gint width, height;
  gdouble start_grid_y;

  gdouble days;
  gint shown_rows;

  gdouble lines_gap;
  gdouble lines_gap_for_5;

  gdouble v_block;

  priv = gcal_month_view_get_instance_private (GCAL_MONTH_VIEW (widget));

  x = event->x;
  y = event->y;

  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);
  start_grid_y = gcal_month_view_get_start_grid_y (widget);

  y = y - start_grid_y;

  days = priv->days_delay + icaltime_days_in_month (priv->date->month, priv->date->year);
  shown_rows = ceil (days / 7.0);

  lines_gap = ((gdouble) (shown_rows + 1) / 2.0) + 0.5 - ceil (shown_rows / 2.0);
  lines_gap_for_5 = shown_rows == 5 ? lines_gap : 0;

  v_block = (height - start_grid_y) / 6.0;

  priv->clicked_cell = 7 * ( floor ( (y - (lines_gap_for_5 * v_block))  / (v_block) )) + floor (x / (width / 7));

  if (priv->clicked_cell <
      icaltime_days_in_month (priv->date->month,
                              priv->date->year))
    {
      priv->start_mark_cell = priv->clicked_cell;
    }

  g_debug ("pressed cell is: %d", priv->start_mark_cell);

  return TRUE;
}

/*
 * Here we update priv->end_cell_mark in order to update the drawing
 * belonging to the days involved in the event creation
 */
static gboolean
gcal_month_view_motion_notify_event (GtkWidget      *widget,
                                     GdkEventMotion *event)
{
  GcalMonthViewPrivate *priv;
  gint width, height;
  gint y;
  gdouble start_grid_y;

  gdouble days;
  gint shown_rows;

  gdouble lines_gap;
  gdouble lines_gap_for_5;

  gdouble v_block;

  gint new_end_cell;

  priv = gcal_month_view_get_instance_private (GCAL_MONTH_VIEW (widget));

  if (priv->start_mark_cell == -1 ||
      priv->clicked_cell == -1)
    return FALSE;

  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);
  start_grid_y = gcal_month_view_get_start_grid_y (widget);

  y = event->y - start_grid_y;
  if (y < 0)
    return FALSE;

  days = priv->days_delay + icaltime_days_in_month (priv->date->month, priv->date->year);
  shown_rows = ceil (days / 7.0);

  lines_gap = ((gdouble) (shown_rows + 1) / 2.0) + 0.5 - ceil (shown_rows / 2.0);
  lines_gap_for_5 = shown_rows == 5 ? lines_gap : 0;

  v_block = (height - start_grid_y) / 6.0;

  /* caching value */
  new_end_cell = 7 * ( floor ( (y - (lines_gap_for_5 * v_block))  / (v_block) )) + floor (event->x / (width / 7));
  if (priv->end_mark_cell != new_end_cell)
    {
      gtk_widget_queue_draw (widget);
    }

  priv->end_mark_cell = new_end_cell;

  g_debug ("move_notify: %d, %d, %d",
           priv->start_mark_cell,
           priv->end_mark_cell,
           new_end_cell);

  return TRUE;
}

static gboolean
gcal_month_view_button_release (GtkWidget      *widget,
                                GdkEventButton *event)
{
  GcalMonthViewPrivate *priv;
  gdouble x, y;
  gint width, height;
  gdouble start_grid_y;
  gint released;

  gdouble days;
  gint shown_rows;

  gdouble lines_gap;
  gdouble lines_gap_for_5;
  gint february_gap;

  gdouble v_block;

  icaltimetype *start_date;
  icaltimetype *end_date;

  priv = gcal_month_view_get_instance_private (GCAL_MONTH_VIEW (widget));

  if (priv->clicked_cell == -1)
    return FALSE;

  x = event->x;
  y = event->y;

  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);
  start_grid_y = gcal_month_view_get_start_grid_y (widget);

  y = y - start_grid_y;

  days = priv->days_delay + icaltime_days_in_month (priv->date->month,
                                                    priv->date->year);
  shown_rows = ceil (days / 7.0);

  lines_gap = ((gdouble) (shown_rows + 1) / 2.0) + 0.5 - ceil (shown_rows / 2.0);
  lines_gap_for_5 = shown_rows == 5 ? lines_gap : 0;
  february_gap = shown_rows == 4 ? 1 : 0;

  v_block = (height - start_grid_y) / 6.0;

  released = 7 * ( floor ( (y - (lines_gap_for_5 * v_block))  / (v_block) )) +
             floor (event->x / (width / 7));

  /* whether the event is out of the days of the month */
  if (released < priv->days_delay + 7 * february_gap ||
      released >= days)
    {
      priv->clicked_cell = -1;
      priv->start_mark_cell = -1;
      priv->end_mark_cell = -1;
      gtk_widget_queue_draw (widget);

      return TRUE;
    }

  priv->end_mark_cell = released;
  g_debug ("released button cell: %d", priv->end_mark_cell);

  gtk_widget_queue_draw (widget);

  x = (width / 7) * (( priv->end_mark_cell % 7) + 0.5);
  y = start_grid_y + v_block * (lines_gap_for_5 + ( priv->end_mark_cell / 7) + 0.5);

  start_date = gcal_dup_icaltime (priv->date);
  start_date->day = priv->start_mark_cell - (priv->days_delay + 7 * february_gap) + 1;
  start_date->is_date = 1;

  end_date = NULL;
  if (priv->start_mark_cell != priv->end_mark_cell)
    {
      end_date = gcal_dup_icaltime (priv->date);
      end_date->day = priv->end_mark_cell - (priv->days_delay + 7 * february_gap) + 1;
      end_date->is_date = 1;
    }

  if (priv->start_mark_cell > priv->end_mark_cell)
    {
      start_date->day = priv->end_mark_cell - (priv->days_delay + 7 * february_gap) + 1;
      end_date->day = priv->start_mark_cell - (priv->days_delay + 7 * february_gap) + 1;
    }

  g_debug ("[view] will emit signal on: %f, %f, with %s",
           x, y, icaltime_as_ical_string (*start_date));

  g_signal_emit_by_name (GCAL_VIEW (widget),
                         "create-event",
                         start_date, end_date,
                         x, y);

  g_free (start_date);

  if (end_date != NULL)
    g_free (end_date);

  priv->clicked_cell = -1;
  return TRUE;
}

static void
gcal_month_view_add (GtkContainer *container,
                     GtkWidget    *widget)
{
  GcalMonthViewPrivate *priv;
  GList *l;
  icaltimetype *date;

  GcalViewChild *new_child;

  g_return_if_fail (GCAL_IS_EVENT_WIDGET (widget));
  g_return_if_fail (gtk_widget_get_parent (widget) == NULL);
  priv = gcal_month_view_get_instance_private (GCAL_MONTH_VIEW (container));

  /* Check if it's already added for date */
  date = gcal_event_widget_get_date (GCAL_EVENT_WIDGET (widget));

  for (l = priv->days[date->day - 1]; l != NULL; l = l->next)
    {
      GcalViewChild *child;

      child = (GcalViewChild*) l->data;
      if (g_strcmp0 (
            gcal_event_widget_peek_uuid (GCAL_EVENT_WIDGET (widget)),
            gcal_event_widget_peek_uuid (GCAL_EVENT_WIDGET (child->widget)))
          == 0)
        {
          //TODO: remove once the main-dev phase its over
          g_warning ("Trying to add an event with the same uuid to the view");
          return;
        }
    }

  new_child = g_new0 (GcalViewChild, 1);
  new_child->widget = widget;
  new_child->hidden = FALSE;

  priv->days[date->day - 1] =
    g_list_insert_sorted (priv->days[date->day - 1],
                          new_child,
                          gcal_compare_event_widget_by_date);
  gtk_widget_set_parent (widget, GTK_WIDGET (container));

  g_free (date);
}

static void
gcal_month_view_remove (GtkContainer *container,
                        GtkWidget    *widget)
{
  GcalMonthViewPrivate *priv;
  gint i;
  GList *l;

  g_return_if_fail (gtk_widget_get_parent (widget) == GTK_WIDGET (container));
  priv = gcal_month_view_get_instance_private (GCAL_MONTH_VIEW (container));

  for (i = 0; i < 31; i++)
    {
      for (l = priv->days[i]; l != NULL; l = l->next)
        {
          GcalViewChild *child;

          child = (GcalViewChild*) l->data;
          if (child->widget == widget)
            {
              gboolean was_visible;

              was_visible = gtk_widget_get_visible (widget);
              gtk_widget_unparent (widget);

              priv->days[i] = g_list_remove (priv->days[i], child);
              g_free (child);

              if (was_visible)
                gtk_widget_queue_resize (GTK_WIDGET (container));

              return;
            }
        }
    }
}

static void
gcal_month_view_forall (GtkContainer *container,
                        gboolean      include_internals,
                        GtkCallback   callback,
                        gpointer      callback_data)
{
  GcalMonthViewPrivate *priv;
  gint i;
  GList *l;

  priv = gcal_month_view_get_instance_private (GCAL_MONTH_VIEW (container));

  for (i = 0; i < 31; i++)
    {
      l = priv->days[i];

      while (l)
        {
          GcalViewChild *child;

          child = (GcalViewChild*) l->data;
          l  = l->next;

          (* callback) (child->widget, callback_data);
        }
    }
}

static gdouble
gcal_month_view_get_start_grid_y (GtkWidget *widget)
{
  GtkBorder padding;

  PangoLayout *layout;
  PangoFontDescription *font_desc;
  gint font_height;
  gdouble start_grid_y;

  gtk_style_context_get_padding (gtk_widget_get_style_context (widget),
                                 gtk_widget_get_state_flags (widget),
                                 &padding);

  gtk_style_context_get (gtk_widget_get_style_context (widget),
                         gtk_widget_get_state_flags(widget),
                         "font", &font_desc,
                         NULL);

  layout = pango_layout_new (gtk_widget_get_pango_context (widget));
  pango_layout_set_font_description (layout, font_desc);
  pango_layout_get_pixel_size (layout, NULL, &font_height);


  start_grid_y = padding.top + font_height;

  pango_font_description_free (font_desc);
  g_object_unref (layout);
  return start_grid_y;
}

/* GcalView Interface API */
/**
 * gcal_month_view_get_initial_date:
 *
 * Since: 0.1
 * Return value: the first day of the month
 * Returns: (transfer full): Release with g_free()
 **/
static icaltimetype*
gcal_month_view_get_initial_date (GcalView *view)
{
  //FIXME to retrieve the 35 days range
  GcalMonthViewPrivate *priv;
  icaltimetype *new_date;

  g_return_val_if_fail (GCAL_IS_MONTH_VIEW (view), NULL);
  priv = gcal_month_view_get_instance_private (GCAL_MONTH_VIEW (view));

  new_date = gcal_dup_icaltime (priv->date);
  new_date->day = 1;

  return new_date;
}

/**
 * gcal_month_view_get_final_date:
 *
 * Since: 0.1
 * Return value: the last day of the month
 * Returns: (transfer full): Release with g_free()
 **/
static icaltimetype*
gcal_month_view_get_final_date (GcalView *view)
{
  //FIXME to retrieve the 35 days range
  GcalMonthViewPrivate *priv;
  icaltimetype *new_date;

  g_return_val_if_fail (GCAL_IS_MONTH_VIEW (view), NULL);
  priv = gcal_month_view_get_instance_private (GCAL_MONTH_VIEW (view));

  new_date = gcal_dup_icaltime (priv->date);
  new_date->day = icaltime_days_in_month (priv->date->month, priv->date->year);
  return new_date;
}

static void
gcal_month_view_mark_current_unit (GcalView *view,
                                   gint     *x,
                                   gint     *y)
{
  GcalMonthViewPrivate *priv;

  gdouble horizontal_block;
  gdouble vertical_block;

  gint days;
  gint shown_rows;
  gint february_gap;
  gdouble lines_gap;
  gdouble lines_gap_for_5;

  gint x_pos, y_pos;
  gdouble start_grid_y;

  priv = gcal_month_view_get_instance_private (GCAL_MONTH_VIEW (view));
  start_grid_y = gcal_month_view_get_start_grid_y (GTK_WIDGET (view));
  horizontal_block = (gdouble) gtk_widget_get_allocated_width (GTK_WIDGET (view)) / 7.0;
  vertical_block = (gdouble) (gtk_widget_get_allocated_height (GTK_WIDGET (view)) - start_grid_y) / 6.0;

  days = priv->days_delay + icaltime_days_in_month (priv->date->month, priv->date->year);
  shown_rows = ceil (days / 7.0);
  february_gap = shown_rows == 4 ? 1 : 0;
  lines_gap = ((gdouble) (shown_rows + 1) / 2.0) + 0.5 - ceil (shown_rows / 2.0);
  lines_gap_for_5 = shown_rows == 5 ? lines_gap : 0;

  priv->start_mark_cell = priv->date->day + 7 * february_gap + priv->days_delay - 1;
  priv->end_mark_cell = priv->start_mark_cell;

  x_pos = horizontal_block * (( priv->end_mark_cell % 7) + 0.5);
  y_pos = start_grid_y + vertical_block * (lines_gap_for_5 + ( priv->end_mark_cell / 7) + 0.5);

  gtk_widget_queue_draw (GTK_WIDGET (view));

  if (x != NULL)
    *x = x_pos;
  if (y != NULL)
    *y = y_pos;
}

static void
gcal_month_view_clear_marks (GcalView *view)
{
  GcalMonthViewPrivate *priv;

  priv = gcal_month_view_get_instance_private (GCAL_MONTH_VIEW (view));

  priv->start_mark_cell = -1;
  priv->end_mark_cell = -1;
  gtk_widget_queue_draw (GTK_WIDGET (view));
}

static gchar*
gcal_month_view_get_left_header (GcalView *view)
{
  GcalMonthViewPrivate *priv;

  gchar str_date[64];

  struct tm tm_date;

  priv = gcal_month_view_get_instance_private (GCAL_MONTH_VIEW (view));

  tm_date = icaltimetype_to_tm (priv->date);
  e_utf8_strftime_fix_am_pm (str_date, 64, "%B", &tm_date);

  return g_strdup_printf ("%s", str_date);
}

static gchar*
gcal_month_view_get_right_header (GcalView *view)
{
  GcalMonthViewPrivate *priv;

  priv = gcal_month_view_get_instance_private (GCAL_MONTH_VIEW (view));

  return g_strdup_printf ("%d", priv->date->year);
}

static gboolean
gcal_month_view_draw_event (GcalView     *view,
                            icaltimetype *start_date,
                            icaltimetype *end_date)
{
  GcalMonthViewPrivate *priv;

  g_return_val_if_fail (GCAL_IS_MONTH_VIEW (view), FALSE);
  priv = gcal_month_view_get_instance_private (GCAL_MONTH_VIEW (view));

  if (priv->date == NULL)
    return FALSE;

  return (priv->date->month == start_date->month &&
          priv->date->year == start_date->year);
}

static GtkWidget*
gcal_month_view_get_by_uuid (GcalView    *view,
                             const gchar *uuid)
{
  GcalMonthViewPrivate *priv;
  gint i;
  GList *l;

  g_return_val_if_fail (GCAL_IS_MONTH_VIEW (view), NULL);
  priv = gcal_month_view_get_instance_private (GCAL_MONTH_VIEW (view));

  for (i = 0; i < 31; i++)
    {
      for (l = priv->days[i]; l != NULL; l = l->next)
        {
          GcalViewChild *child;
          const gchar* widget_uuid;

          child = (GcalViewChild*) l->data;
          widget_uuid = gcal_event_widget_peek_uuid (GCAL_EVENT_WIDGET (child->widget));
          if (g_strcmp0 (uuid, widget_uuid) == 0)
            return child->widget;
        }
    }
  return NULL;
}

static void
gcal_month_view_clear (GcalView *view)
{
  gtk_container_foreach (GTK_CONTAINER (view),
                         (GtkCallback) gtk_widget_destroy, NULL);
}

/* Public API */
/**
 * gcal_month_view_new:
 *
 * Since: 0.1
 * Return value: the new month view widget
 * Returns: (transfer full):
 **/
GtkWidget*
gcal_month_view_new (void)
{
  return g_object_new (GCAL_TYPE_MONTH_VIEW, NULL);
}
