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
#include "gcal-view.h"
#include "gcal-event-widget.h"

#include <glib/gi18n.h>

enum
{
  PROP_0,
  PROP_DATE
};

struct _GcalMonthViewPrivate
{
  /**
   * This is where we keep the refs of the child widgets.
   * Every child added to the list placed in the position
   * of it corresponding cell number.
   * The cell number is calculated in _add method.
   */
  GList          *days [35];

  GdkWindow      *event_window;

  gint            clicked_cell;

  gint            selected_cell;
  icaltimetype   *date;

  gint            days_delay;
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

static void     gcal_view_interface_init                (GcalViewIface  *iface);

static void     gcal_month_view_constructed             (GObject        *object);

static void     gcal_month_view_set_property            (GObject        *object,
                                                         guint           property_id,
                                                         const GValue   *value,
                                                         GParamSpec     *pspec);

static void     gcal_month_view_get_property            (GObject        *object,
                                                         guint           property_id,
                                                         GValue         *value,
                                                         GParamSpec     *pspec);

static void     gcal_month_view_finalize                (GObject        *object);

static void     gcal_month_view_realize                 (GtkWidget      *widget);

static void     gcal_month_view_unrealize               (GtkWidget      *widget);

static void     gcal_month_view_map                     (GtkWidget      *widget);

static void     gcal_month_view_unmap                   (GtkWidget      *widget);

static void     gcal_month_view_size_allocate           (GtkWidget      *widget,
                                                         GtkAllocation  *allocation);

static gboolean gcal_month_view_draw                    (GtkWidget      *widget,
                                                         cairo_t        *cr);

static gboolean gcal_month_view_button_press            (GtkWidget      *widget,
                                                         GdkEventButton *event);

static gboolean gcal_month_view_button_release          (GtkWidget      *widget,
                                                         GdkEventButton *event);

static void     gcal_month_view_add                     (GtkContainer   *constainer,
                                                         GtkWidget      *widget);

static void     gcal_month_view_remove                  (GtkContainer   *constainer,
                                                         GtkWidget      *widget);

static void     gcal_month_view_forall                  (GtkContainer   *container,
                                                         gboolean        include_internals,
                                                         GtkCallback     callback,
                                                         gpointer        callback_data);

static void     gcal_month_view_draw_month_grid         (GcalMonthView  *mont_view,
                                                         cairo_t        *cr,
                                                         gint            x,
                                                         gint            y);

static gboolean gcal_month_view_is_in_range             (GcalView       *view,
                                                         icaltimetype   *date);


G_DEFINE_TYPE_WITH_CODE (GcalMonthView,
                         gcal_month_view,
                         GTK_TYPE_CONTAINER,
                         G_IMPLEMENT_INTERFACE (GCAL_TYPE_VIEW,
                                                gcal_view_interface_init));


static void gcal_month_view_class_init (GcalMonthViewClass *klass)
{
  GtkContainerClass *container_class;
  GtkWidgetClass *widget_class;
  GObjectClass *object_class;

  container_class = GTK_CONTAINER_CLASS (klass);
  container_class->add   = gcal_month_view_add;
  container_class->remove = gcal_month_view_remove;
  container_class->forall = gcal_month_view_forall;

  widget_class = GTK_WIDGET_CLASS (klass);
  widget_class->realize = gcal_month_view_realize;
  widget_class->unrealize = gcal_month_view_unrealize;
  widget_class->map = gcal_month_view_map;
  widget_class->unmap = gcal_month_view_unmap;
  widget_class->size_allocate = gcal_month_view_size_allocate;
  widget_class->draw = gcal_month_view_draw;
  widget_class->button_press_event = gcal_month_view_button_press;
  widget_class->button_release_event = gcal_month_view_button_release;

  object_class = G_OBJECT_CLASS (klass);
  object_class->constructed = gcal_month_view_constructed;
  object_class->set_property = gcal_month_view_set_property;
  object_class->get_property = gcal_month_view_get_property;
  object_class->finalize = gcal_month_view_finalize;

  g_object_class_install_property (object_class,
                                   PROP_DATE,
                                   g_param_spec_boxed ("date",
                                                       "Date",
                                                       _("Date"),
                                                       ICAL_TIME_TYPE,
                                                       G_PARAM_CONSTRUCT |
                                                       G_PARAM_READWRITE));

  g_type_class_add_private ((gpointer)klass, sizeof (GcalMonthViewPrivate));
}



static void gcal_month_view_init (GcalMonthView *self)
{
  GcalMonthViewPrivate *priv;
  gint i;

  gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            GCAL_TYPE_MONTH_VIEW,
                                            GcalMonthViewPrivate);
  priv = self->priv;

  for (i = 0; i < 35; i++)
    {
      priv->days[i] = NULL;
    }
}

static void
gcal_view_interface_init (GcalViewIface *iface)
{
  iface->is_in_range = gcal_month_view_is_in_range;
}

static void
gcal_month_view_constructed (GObject       *object)
{
  GcalMonthViewPrivate *priv;

  priv = GCAL_MONTH_VIEW (object)->priv;
  priv->selected_cell = priv->date->day + ( - priv->days_delay);
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
      {
        icaltimetype *first_of_month;

        if (priv->date != NULL)
          g_free (priv->date);

        priv->date = g_value_dup_boxed (value);

        first_of_month = gcal_dup_icaltime (priv->date);
        first_of_month->day = 1;
        priv->days_delay =  - icaltime_day_of_week (*first_of_month) + 2;
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
  GcalMonthViewPrivate *priv = GCAL_MONTH_VIEW (object)->priv;

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
  GcalMonthViewPrivate *priv = GCAL_MONTH_VIEW (object)->priv;

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

  priv = GCAL_MONTH_VIEW (widget)->priv;
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

  priv = GCAL_MONTH_VIEW (widget)->priv;
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

  priv = GCAL_MONTH_VIEW (widget)->priv;
  GTK_WIDGET_CLASS (gcal_month_view_parent_class)->map (widget);

  if (priv->event_window)
    gdk_window_show (priv->event_window);
}

static void
gcal_month_view_unmap (GtkWidget *widget)
{
  GcalMonthViewPrivate *priv;

  priv = GCAL_MONTH_VIEW (widget)->priv;
  if (priv->event_window)
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

  GtkStyleContext *context;
  GtkStateFlags state;
  GtkBorder padding;
  PangoLayout *layout;
  gint font_height;
  gdouble added_height;

  priv = GCAL_MONTH_VIEW (widget)->priv;

  gtk_widget_set_allocation (widget, allocation);
  if (gtk_widget_get_realized (widget))
    {
      gdk_window_move_resize (priv->event_window,
                              allocation->x,
                              allocation->y,
                              allocation->width,
                              allocation->height);
    }

  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);
  gtk_style_context_get_padding (context, state, &padding);

  layout = pango_layout_new (gtk_widget_get_pango_context (widget));
  pango_layout_set_font_description (layout,
                                     gtk_style_context_get_font (context,
                                                                 state));
  pango_layout_get_pixel_size (layout, NULL, &font_height);

  /* init values */
  priv->header_font_size = 12;
  priv->horizontal_step =
    (allocation->width - (allocation->x + padding.left + padding.right)) / 7;
  priv->vertical_step =
    (allocation->height - (allocation->y + padding.top + padding.bottom + priv->header_font_size)) / 5;

  for (i = 0; i < 35; i++)
    {
      if (priv->days[i] == NULL)
        continue;
      added_height = 0;
      for (l = priv->days[i]; l != NULL; l = l->next)
        {
          gint pos_x;
          gint pos_y;
          gint min_height;
          gint natural_height;
          GtkAllocation child_allocation;

          pos_x = ( i % 7 ) * priv->horizontal_step;
          pos_y = ( i / 7 ) * priv->vertical_step;

          gtk_widget_get_preferred_height (GTK_WIDGET (l->data),
                                           &min_height,
                                           &natural_height);
          child_allocation.x = pos_x + allocation->x + padding.top;
          child_allocation.y = pos_y + font_height + allocation->y
            + priv->header_font_size + padding.left;
          child_allocation.width = priv->horizontal_step;
          child_allocation.height = MIN (natural_height, priv->vertical_step);
          if (added_height + font_height + child_allocation.height
              > priv->vertical_step)
            {
              gtk_widget_hide (GTK_WIDGET (l->data));
            }
          else
            {
              gtk_widget_show (GTK_WIDGET (l->data));
              child_allocation.y = child_allocation.y + added_height;
              gtk_widget_size_allocate (GTK_WIDGET (l->data), &child_allocation);
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

  gcal_month_view_draw_month_grid (
      GCAL_MONTH_VIEW (widget),
      cr,
      alloc.x + padding.left,
      alloc.y + padding.top + priv->header_font_size);

  if (GTK_WIDGET_CLASS (gcal_month_view_parent_class)->draw != NULL)
    GTK_WIDGET_CLASS (gcal_month_view_parent_class)->draw (widget, cr);

  return FALSE;
}

static gboolean
gcal_month_view_button_press (GtkWidget      *widget,
                              GdkEventButton *event)
{
  GcalMonthViewPrivate *priv;
  GtkStyleContext *context;
  GtkStateFlags state;
  GtkBorder padding;
  gint x, y;

  priv = GCAL_MONTH_VIEW (widget)->priv;
  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);

  gtk_style_context_get_padding (context, state, &padding);

  x = event->x;
  y = event->y;
  if (event->window != priv->event_window)
    {
      gpointer source_widget;
      gdk_window_get_user_data (event->window, &source_widget);
      gtk_widget_translate_coordinates (GTK_WIDGET (source_widget),
                                        widget,
                                        event->x,
                                        event->y,
                                        &x,
                                        &y);
    }

  x = x - padding.left;
  y = y - ( padding.top + priv->header_font_size );
  priv->clicked_cell = 7 * ( (gint ) ( y  / priv->vertical_step )) + ((gint) ( x / priv->horizontal_step ));

  return TRUE;
}

static gboolean gcal_month_view_button_release (GtkWidget      *widget,
                                                GdkEventButton *event)
{
  GcalMonthViewPrivate *priv;
  GtkStyleContext *context;
  GtkStateFlags state;
  GtkBorder padding;
  gint x, y;
  gint released;

  priv = GCAL_MONTH_VIEW (widget)->priv;
  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);

  gtk_style_context_get_padding (context, state, &padding);

  x = event->x;
  y = event->y;
  if (event->window != priv->event_window)
    {
      gpointer source_widget;
      gdk_window_get_user_data (event->window, &source_widget);
      gtk_widget_translate_coordinates (GTK_WIDGET (source_widget),
                                        widget,
                                        event->x,
                                        event->y,
                                        &x,
                                        &y);
    }

  x = x - padding.left;
  y = y - ( padding.top + priv->header_font_size );
  released = 7 * ( (gint ) ( y  / priv->vertical_step )) + ((gint) ( x / priv->horizontal_step ));

  if (priv->clicked_cell == released)
    {
      priv->selected_cell = priv->clicked_cell;
      priv->clicked_cell = -1;
      gtk_widget_queue_draw (widget);
    }
  return TRUE;
}

static void
gcal_month_view_add (GtkContainer *container,
                     GtkWidget    *widget)
{
  GcalMonthViewPrivate *priv;
  GList *l;
  gint day;
  icaltimetype *date;

  g_return_if_fail (GCAL_IS_MONTH_VIEW (container));
  g_return_if_fail (GCAL_IS_EVENT_WIDGET (widget));
  g_return_if_fail (gtk_widget_get_parent (widget) == NULL);
  priv = GCAL_MONTH_VIEW (container)->priv;

  /* Check if it's already added for date */
  date = gcal_event_widget_get_date (GCAL_EVENT_WIDGET (widget));
  day = date->day + ( - priv->days_delay);
  g_free (date);

  for (l = priv->days[day]; l != NULL; l = l->next)
    {
      if (g_strcmp0 (
            gcal_event_widget_peek_uuid (GCAL_EVENT_WIDGET (widget)),
            gcal_event_widget_peek_uuid (GCAL_EVENT_WIDGET (l->data))) == 0)
        {
          g_warning ("Trying to add an event with the same uuid to the view");
          return;
        }
    }
  priv->days[day] = g_list_append (priv->days[day], widget);
  gtk_widget_set_parent (widget, GTK_WIDGET (container));
}

static void
gcal_month_view_remove (GtkContainer *container,
                        GtkWidget    *widget)
{
  GcalMonthViewPrivate *priv;
  icaltimetype *date;
  gint day;

  g_return_if_fail (GCAL_IS_MONTH_VIEW (container));
  g_return_if_fail (gtk_widget_get_parent (widget) == GTK_WIDGET (container));
  priv = GCAL_MONTH_VIEW (container)->priv;

  date = gcal_event_widget_get_date (GCAL_EVENT_WIDGET (widget));
  day = date->day + ( - priv->days_delay);
  g_free (date);

  priv->days[day] = g_list_remove (priv->days[day], widget);
  gtk_widget_unparent (widget);
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

  priv = GCAL_MONTH_VIEW (container)->priv;

  for (i = 0; i < 35; i++)
    {
      for (l = priv->days[i]; l != NULL; l = l->next)
        {
          (* callback) (l->data, callback_data);
        }
    }
}

static void
gcal_month_view_draw_month_grid (GcalMonthView *month_view,
                                 cairo_t       *cr,
                                 gint           x,
                                 gint           y)
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

  PangoLayout *layout;
  gchar *day;

  priv = month_view->priv;
  widget = GTK_WIDGET (month_view);
  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);

  state |= GTK_STATE_FLAG_SELECTED;
  gtk_style_context_get_background_color (context, state, &color);
  cairo_set_source_rgba (cr, color.red, color.green, color.blue, 0.8);
  cairo_rectangle (cr,
                   x + priv->horizontal_step * ( priv->selected_cell % 7),
                   y + priv->vertical_step * ( priv->selected_cell / 7),
                   priv->horizontal_step,
                   priv->vertical_step);
  cairo_fill (cr);

  state = gtk_widget_get_state_flags (widget);
  gtk_style_context_get_color (context, state, &color);
  cairo_set_source_rgba (cr, color.red, color.green, color.blue, color.alpha);

  layout = pango_cairo_create_layout (cr);
  pango_layout_set_font_description (layout,
                                     gtk_style_context_get_font (context,
                                                                 state));

  n_days_in_month = icaltime_days_in_month (priv->date->month,
                                            priv->date->year);

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
          gint n_day = j * 7 + i + priv->days_delay;
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

  for (i = 0; i < 6; i++)
    {
      cairo_move_to (cr, x, y + priv->vertical_step * i);
      cairo_line_to (cr,
                     x + priv->horizontal_step * 7,
                     y + priv->vertical_step * i);
    }

  for (i = 0; i < 8; i++)
    {
      cairo_move_to (cr, x + priv->horizontal_step * i, y);
      cairo_line_to (cr,
                     x + priv->horizontal_step * i,
                     y + priv->vertical_step * 5);
    }

  cairo_stroke (cr);

}

static gboolean
gcal_month_view_is_in_range (GcalView     *view,
                             icaltimetype *date)
{
  g_debug ("Implementation of is_in_range called");
  return TRUE;
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
 * Returns: (transfer full): Release with g_free
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
 * Returns: (transfer full): Release with g_free
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
