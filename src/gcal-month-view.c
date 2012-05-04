/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 2 -*- */
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

#include <glib/gi18n.h>

enum
{
  PROP_0,
  PROP_DATE
};

struct _GcalMonthViewPrivate
{
  icaltimetype   *date;

  gint            header_font_size;

  gdouble         vertical_step;
  gdouble         horizontal_step;
};

/* this will need translation */
const char* weekdays [] =
 {
   "Sun",
   "Mon",
   "Tue",
   "Wed",
   "Thu",
   "Fri",
   "Sat"
  };

static void     gcal_month_view_set_property            (GObject       *object,
                                                         guint          property_id,
                                                         const GValue  *value,
                                                         GParamSpec    *pspec);

static void     gcal_month_view_get_property            (GObject       *object,
                                                         guint          property_id,
                                                         GValue        *value,
                                                         GParamSpec    *pspec);

static gboolean gcal_month_view_draw                    (GtkWidget     *widget,
                                                         cairo_t       *cr);

static void     gcal_month_view_finalize                (GObject       *object);

static void     _gcal_month_view_draw_month_grid        (GcalMonthView *mont_view,
                                                         cairo_t       *cr,
                                                         gint           x,
                                                         gint           y,
                                                         gint           width,
                                                         gint           height);

G_DEFINE_TYPE(GcalMonthView, gcal_month_view, GTK_TYPE_WIDGET)

static void gcal_month_view_class_init (GcalMonthViewClass *klass)
{
  GtkWidgetClass* widget_class;
  GObjectClass *object_class;

  widget_class = GTK_WIDGET_CLASS (klass);
  widget_class->draw = gcal_month_view_draw;

  object_class = G_OBJECT_CLASS (klass);
  object_class->set_property = gcal_month_view_set_property;
  object_class->get_property = gcal_month_view_get_property;
  object_class->finalize = gcal_month_view_finalize;

  g_object_class_install_property (object_class, PROP_DATE,
                                   g_param_spec_pointer ("date",
                                                         "date",
                                                         _("Date"),
                                                         G_PARAM_CONSTRUCT |
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_STATIC_NAME |
                                                         G_PARAM_STATIC_BLURB |
                                                         G_PARAM_STATIC_NICK));

  g_type_class_add_private ((gpointer)klass, sizeof (GcalMonthViewPrivate));
}



static void gcal_month_view_init (GcalMonthView *self)
{
  gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            GCAL_TYPE_MONTH_VIEW,
                                            GcalMonthViewPrivate);
}

static void
gcal_month_view_set_property (GObject       *object,
                              guint          property_id,
                              const GValue  *value,
                              GParamSpec    *pspec)
{
  GcalMonthViewPrivate *priv = GCAL_MONTH_VIEW (object)->priv;

  switch (property_id)
    {
    case PROP_DATE:
      if (priv->date != NULL)
        g_free (priv->date);

      priv->date = g_value_get_pointer (value);
      break;
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
  GcalMonthViewPrivate *priv = GCAL_MONTH_VIEW (object)->priv;

  switch (property_id)
    {
    case PROP_DATE:
      g_value_set_pointer (value, priv->date);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
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
  GdkRGBA color;
  GtkAllocation alloc;

  priv = GCAL_MONTH_VIEW (widget)->priv;

  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);

  gtk_style_context_get_padding (context, state, &padding);

  gtk_style_context_get_color (context, state, &color);

  /* header_font_size is found through pango_get_size_in_pixels */
  priv->header_font_size = 12;

  /* getting allocation, and so on */
  gtk_widget_get_allocation (widget, &alloc);

  _gcal_month_view_draw_month_grid (
      GCAL_MONTH_VIEW (widget),
      cr,
      alloc.x + padding.left,
      alloc.y + padding.top + priv->header_font_size,
      alloc.width - (alloc.x + padding.left + padding.right),
      alloc.height - (alloc.y + padding.top + padding.bottom + priv->header_font_size));

  return FALSE;
}

static void
gcal_month_view_finalize (GObject       *object)
{
  GcalMonthViewPrivate *priv = GCAL_MONTH_VIEW (object)->priv;

  if (priv->date != NULL)
    g_free (priv->date);

  /* Chain up to parent's finalize() method. */
  G_OBJECT_CLASS (gcal_month_view_parent_class)->finalize (object);
}

static void
_gcal_month_view_draw_month_grid (GcalMonthView *month_view,
                                  cairo_t       *cr,
                                  gint           x,
                                  gint           y,
                                  gint           width,
                                  gint           height)
{
  GcalMonthViewPrivate *priv;
  GtkWidget *widget;

  GtkStyleContext *context;
  GtkStateFlags state;
  GdkRGBA color;

  gint i, j;
  gint font_width;
  gint font_height;

  guint8 n_days_in_month;
  icaltimetype *first_of_month;
  gint space;

  PangoLayout *layout;
  gchar *day;

  priv = month_view->priv;
  widget = GTK_WIDGET (month_view);
  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);

  gtk_style_context_get_color (context, state, &color);
  cairo_set_source_rgba (cr, color.red, color.green, color.blue, color.alpha);

  layout = pango_cairo_create_layout (cr);
  pango_layout_set_font_description (layout,
                                     gtk_style_context_get_font (context,
                                                                 state));

  priv->vertical_step = (gdouble) height / 5;
  priv->horizontal_step= (gdouble) width / 7;

  n_days_in_month = icaltime_days_in_month (priv->date->month,
                                            priv->date->year);

  first_of_month = gcal_dup_icaltime (priv->date);
  first_of_month->day = 1;

  space =  - icaltime_day_of_week (*first_of_month) + 2;
  g_free (first_of_month);

  for (i = 0; i < 7; i++)
    {
      pango_layout_set_text (layout, weekdays[i], -1);
      pango_cairo_update_layout (cr, layout);
      pango_layout_get_pixel_size (layout, &font_width, &font_height);
      cairo_move_to (cr,
                     x + priv->horizontal_step * i + 1,
                     y - font_height - 1);
      pango_cairo_show_layout (cr, layout);

      for (j = 0; j < 5; j++)
        {
          gint n_day = j * 7 + i + space;
          if (n_day <= 0 ||
              n_day > n_days_in_month)
            continue;
          day = g_strdup_printf ("%d", n_day);
          pango_layout_set_text (layout, day, -1);
          pango_cairo_update_layout (cr, layout);
          pango_layout_get_pixel_size (layout, &font_width, &font_height);
          cairo_move_to (cr,
                         x + priv->horizontal_step * i + 2,
                         y + priv->vertical_step * j + 1);
          pango_cairo_show_layout (cr, layout);
          g_free (day);
        }
    }
  /* free the layout object */
  g_object_unref (layout);

  for (i = 0; i < 6; i++) {
      cairo_move_to (cr, x, y + priv->vertical_step * i);
      cairo_line_to (cr,
                    x + priv->horizontal_step * 7,
                    y + priv->vertical_step * i);
  }

  for (i = 0; i < 8; i++) {
      cairo_move_to (cr, x + priv->horizontal_step * i, y);
      cairo_line_to (cr, x + priv->horizontal_step * i, y + height);
  }

  cairo_stroke (cr);
}

/**
 * gcal_month_view_new:
 * @date:
 *
 * Since: 0.1
 * Return value: the new month view widget
 * Returns: (transfer full):
 **/
GtkWidget*
gcal_month_view_new (icaltimetype *date)
{
  return g_object_new (GCAL_TYPE_MONTH_VIEW, "date", date, NULL);
}

/**
 * gcal_month_view_get_initial_date:
 *
 * Since: 0.1
 * Return value: the first day of the month
 * Returns: (transfer full):
 **/
icaltimetype*
gcal_month_view_get_initial_date (GcalMonthView *view)
{
  GcalMonthViewPrivate *priv;
  icaltimetype *new_date;

  priv = view->priv;
  new_date = gcal_dup_icaltime (priv->date);
  new_date->day = 1;

  return new_date;
}

/**
 * gcal_month_view_get_final_date:
 *
 * Since: 0.1
 * Return value: the last day of the month
 * Returns: (transfer full):
 **/
icaltimetype*
gcal_month_view_get_final_date (GcalMonthView *view)
{
  GcalMonthViewPrivate *priv;
  icaltimetype *new_date;

  priv = view->priv;
  new_date = gcal_dup_icaltime (priv->date);
  new_date->day = icaltime_days_in_month (priv->date->month, priv->date->year);
  return new_date;
}
