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

#include <libecal/e-cal-time-util.h>

enum
{
  PROP_0,
  PROP_DATE  //active-date inherited property
};

struct _GcalViewChild
{
  GtkWidget *widget;
  gboolean   hidden_by_me;
};

typedef struct _GcalViewChild GcalViewChild;

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

  gint            days_delay;
  gint            header_size;
  gint            grid_header_size;
  gdouble         vertical_step;
  gdouble         horizontal_step;

  /* property */
  icaltimetype   *date;
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

static void           gcal_month_view_set_date              (GcalMonthView  *view,
                                                             icaltimetype   *date);

static void           gcal_month_view_draw_header           (GcalMonthView  *view,
                                                             cairo_t        *cr,
                                                             GtkAllocation  *alloc,
                                                             GtkBorder      *padding);

static void           gcal_month_view_draw_grid             (GcalMonthView  *view,
                                                             cairo_t        *cr,
                                                             GtkAllocation  *alloc,
                                                             GtkBorder      *padding);

static icaltimetype*  gcal_month_view_get_initial_date      (GcalView       *view);

static icaltimetype*  gcal_month_view_get_final_date        (GcalView       *view);

static gboolean       gcal_month_view_contains              (GcalView       *view,
                                                             icaltimetype   *date);

static void           gcal_month_view_remove_by_uuid        (GcalView       *view,
                                                             const gchar    *uuid);

static GtkWidget*     gcal_month_view_get_by_uuid           (GcalView       *view,
                                                             const gchar    *uuid);

G_DEFINE_TYPE_WITH_CODE (GcalMonthView,
                         gcal_month_view,
                         GTK_TYPE_CONTAINER,
                         G_IMPLEMENT_INTERFACE (GCAL_TYPE_VIEW,
                                                gcal_view_interface_init));


static void
gcal_month_view_class_init (GcalMonthViewClass *klass)
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
  object_class->set_property = gcal_month_view_set_property;
  object_class->get_property = gcal_month_view_get_property;
  object_class->finalize = gcal_month_view_finalize;

  g_object_class_override_property (object_class, PROP_DATE, "active-date");

  g_type_class_add_private ((gpointer)klass, sizeof (GcalMonthViewPrivate));
}



static void
gcal_month_view_init (GcalMonthView *self)
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

  gtk_style_context_add_class (
      gtk_widget_get_style_context (GTK_WIDGET (self)),
      "calendar-view");
}

static void
gcal_view_interface_init (GcalViewIface *iface)
{
  iface->get_initial_date = gcal_month_view_get_initial_date;
  iface->get_final_date = gcal_month_view_get_final_date;

  iface->contains = gcal_month_view_contains;
  iface->remove_by_uuid = gcal_month_view_remove_by_uuid;
  iface->get_by_uuid = gcal_month_view_get_by_uuid;
}

static void
gcal_month_view_set_property (GObject       *object,
                              guint          property_id,
                              const GValue  *value,
                              GParamSpec    *pspec)
{
  g_return_if_fail (GCAL_IS_MONTH_VIEW (object));

  switch (property_id)
    {
    case PROP_DATE:
      gcal_month_view_set_date (GCAL_MONTH_VIEW (object),
                                g_value_dup_boxed (value));
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
  GcalMonthViewPrivate *priv;

  g_return_if_fail (GCAL_IS_MONTH_VIEW (object));
  priv = GCAL_MONTH_VIEW (object)->priv;

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
  if (priv->event_window)
    gdk_window_show (priv->event_window);

  GTK_WIDGET_CLASS (gcal_month_view_parent_class)->map (widget);
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

  /* init header values */
  gtk_style_context_save (context);
  gtk_style_context_add_region (context, "header", 0);

  pango_layout_set_font_description (layout,
                                     gtk_style_context_get_font (context,
                                                                 state));
  pango_layout_get_pixel_size (layout, NULL, &font_height);

  /* 6: is padding around the header */
  priv->header_size = font_height + 6;
  gtk_style_context_remove_region (context, "header");
  gtk_style_context_restore (context);

  /* init grid values */
  pango_layout_set_font_description (layout,
                                     gtk_style_context_get_font (context,
                                                                 state));
  pango_layout_get_pixel_size (layout, NULL, &font_height);
  /* 6: is padding around the header */
  priv->grid_header_size = font_height + 6;

  priv->horizontal_step =
    (allocation->width - (allocation->x + padding.left + padding.right)) / 7;
  priv->vertical_step =
    (allocation->height - (allocation->y + padding.top + padding.bottom + priv->header_size + priv->grid_header_size)) / 5;

  for (i = 0; i < 35; i++)
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

          pos_x = ( i % 7 ) * priv->horizontal_step;
          pos_y = ( i / 7 ) * priv->vertical_step;

          if ((! gtk_widget_get_visible (child->widget)) && (! child->hidden_by_me))
            continue;

          gtk_widget_get_preferred_height (child->widget,
                                           &min_height,
                                           &natural_height);
          child_allocation.x = pos_x + allocation->x + padding.left;
          child_allocation.y = pos_y + font_height + allocation->y
            + priv->header_size + priv->grid_header_size + padding.top;
          child_allocation.width = priv->horizontal_step;
          child_allocation.height = MIN (natural_height, priv->vertical_step);
          if (added_height + font_height + child_allocation.height
              > priv->vertical_step)
            {
              gtk_widget_hide (child->widget);
              child->hidden_by_me = TRUE;
            }
          else
            {
              gtk_widget_show (child->widget);
              child->hidden_by_me = FALSE;
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
  GtkStyleContext *context;
  GtkStateFlags state;
  GtkBorder padding;
  GtkAllocation alloc;

  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);

  gtk_style_context_get_padding (context, state, &padding);
  gtk_widget_get_allocation (widget, &alloc);

  gcal_month_view_draw_header (GCAL_MONTH_VIEW (widget), cr, &alloc, &padding);
  gcal_month_view_draw_grid (GCAL_MONTH_VIEW (widget), cr, &alloc, &padding);

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
  y = y - ( padding.top + priv->grid_header_size );
  priv->clicked_cell = 7 * ( (gint ) ( y  / priv->vertical_step )) + ((gint) ( x / priv->horizontal_step ));

  return TRUE;
}

static gboolean
gcal_month_view_button_release (GtkWidget      *widget,
                                GdkEventButton *event)
{
  GcalMonthViewPrivate *priv;
  GtkStyleContext *context;
  GtkStateFlags state;
  GtkBorder padding;
  gint x, y;
  gint released;

  icaltimetype *new_date;

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
  y = y - ( padding.top + priv->grid_header_size );
  released = 7 * ( (gint ) ( y  / priv->vertical_step )) + ((gint) ( x / priv->horizontal_step ));

  if (priv->clicked_cell == released)
    {
      priv->selected_cell = priv->clicked_cell;
      priv->clicked_cell = -1;
      gtk_widget_queue_resize (widget);

      new_date = gcal_dup_icaltime (priv->date);
      new_date->day = priv->selected_cell + priv->days_delay;
      gcal_view_set_date (GCAL_VIEW (widget), new_date);
      g_free (new_date);
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

  GcalViewChild *new_child;

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
      GcalViewChild *child;

      child = (GcalViewChild*) l->data;
      if (g_strcmp0 (
            gcal_event_widget_peek_uuid (GCAL_EVENT_WIDGET (widget)),
            gcal_event_widget_peek_uuid (GCAL_EVENT_WIDGET (child->widget))) == 0)
        {
          g_warning ("Trying to add an event with the same uuid to the view");
          return;
        }
    }

  new_child = g_new0 (GcalViewChild, 1);
  new_child->widget = widget;
  new_child->hidden_by_me = FALSE;

  priv->days[day] = g_list_append (priv->days[day], new_child);
  gtk_widget_set_parent (widget, GTK_WIDGET (container));
}

static void
gcal_month_view_remove (GtkContainer *container,
                        GtkWidget    *widget)
{
  GcalMonthViewPrivate *priv;
  icaltimetype *date;
  gint day;
  GList *l;

  g_return_if_fail (GCAL_IS_MONTH_VIEW (container));
  g_return_if_fail (gtk_widget_get_parent (widget) == GTK_WIDGET (container));
  priv = GCAL_MONTH_VIEW (container)->priv;

  date = gcal_event_widget_get_date (GCAL_EVENT_WIDGET (widget));
  day = date->day + ( - priv->days_delay);
  g_free (date);

  for (l = priv->days[day]; l != NULL; l = l->next)
    {
      GcalViewChild *child;

      child = (GcalViewChild*) l->data;
      if (child->widget == widget)
        {
          priv->days[day] = g_list_remove (priv->days[day], child);
          g_free (child);
          break;
        }
    }
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
          GcalViewChild *child;

          child = (GcalViewChild*) l->data;
          (* callback) (child->widget, callback_data);
        }
    }
}

static void
gcal_month_view_set_date (GcalMonthView *view,
                          icaltimetype  *date)
{
  GcalMonthViewPrivate *priv;
  gboolean will_resize;
  icaltimetype *first_of_month;

  gint i;
  GList *l;
  GList *to_remove;

  priv = view->priv;
  will_resize = FALSE;

  /* if span_updated: queue_resize */
  will_resize = ! gcal_view_contains (GCAL_VIEW (view), date);

  if (priv->date != NULL)
    g_free (priv->date);

  priv->date = date;

  first_of_month = gcal_dup_icaltime (priv->date);
  first_of_month->day = 1;
  priv->days_delay =  - icaltime_day_of_week (*first_of_month) + 2;
  g_free (first_of_month);

  /* if have_selection: yes this one have */
  priv->selected_cell = priv->date->day - priv->days_delay;

  if (will_resize)
    {
      to_remove = NULL;

      for (i = 0; i < 34; i++)
        {
          for (l = priv->days[i]; l != NULL; l = l->next)
            {
              GcalViewChild *child;
              icaltimetype *child_date;

              child = (GcalViewChild*) l->data;
              child_date = gcal_event_widget_get_date (GCAL_EVENT_WIDGET (child->widget));
              if (! gcal_view_contains (GCAL_VIEW (view), child_date))
                to_remove = g_list_append (to_remove, child->widget);
            }
        }
      g_list_foreach (to_remove, (GFunc) gtk_widget_destroy, NULL);

      gtk_widget_queue_resize (GTK_WIDGET (view));
    }
}

static void
gcal_month_view_draw_header (GcalMonthView  *view,
                             cairo_t       *cr,
                             GtkAllocation *alloc,
                             GtkBorder     *padding)
{
  GcalMonthViewPrivate *priv;
  GtkStyleContext *context;
  GtkStateFlags state;
  GdkRGBA color;

  PangoLayout *layout;
  gint layout_width;

  gchar *left_header;
  gchar *right_header;
  gchar str_date[64];

  struct tm tm_date;

  g_return_if_fail (GCAL_IS_MONTH_VIEW (view));
  priv = view->priv;

  context = gtk_widget_get_style_context (GTK_WIDGET (view));
  state = gtk_widget_get_state_flags (GTK_WIDGET (view));

  gtk_style_context_save (context);
  gtk_style_context_add_region (context, "header", 0);

  gtk_style_context_get_color (context, state, &color);
  cairo_set_source_rgba (cr, color.red, color.green, color.blue, color.alpha);

  layout = pango_cairo_create_layout (cr);
  pango_layout_set_font_description (layout,
                                     gtk_style_context_get_font (context,
                                                                 state));
  gtk_style_context_remove_region (context, "header");
  gtk_style_context_restore (context);

  tm_date = icaltimetype_to_tm (priv->date);
  e_utf8_strftime_fix_am_pm (str_date, 64, "%B", &tm_date);

  left_header = g_strdup_printf ("%s", str_date);
  right_header = g_strdup_printf ("%d", priv->date->year);

  pango_layout_set_text (layout, left_header, -1);
  pango_cairo_update_layout (cr, layout);

  cairo_move_to (cr, alloc->x + padding->left, alloc->y + padding->top);
  pango_cairo_show_layout (cr, layout);

  state |= GTK_STATE_FLAG_INSENSITIVE;
  gtk_style_context_get_color (context, state, &color);
  cairo_set_source_rgba (cr, color.red, color.green, color.blue, color.alpha);

  pango_layout_set_text (layout, right_header, -1);
  pango_cairo_update_layout (cr, layout);
  pango_layout_get_pixel_size (layout, &layout_width, NULL);

  cairo_move_to (cr,
                 alloc->width - padding->right - layout_width,
                 alloc->y + padding->top);
  pango_cairo_show_layout (cr, layout);

  g_free (left_header);
  g_free (right_header);
  g_object_unref (layout);
}

static void
gcal_month_view_draw_grid (GcalMonthView *view,
                           cairo_t       *cr,
                           GtkAllocation *alloc,
                           GtkBorder     *padding)
{
  GcalMonthViewPrivate *priv;
  GtkWidget *widget;

  GtkStyleContext *context;
  GtkStateFlags state;
  GdkRGBA color;

  gint i, j;
  gint font_width;
  gint font_height;

  gdouble start_grid_x;
  gdouble start_grid_y;

  guint8 n_days_in_month;

  PangoLayout *layout;
  gchar *day;

  priv = view->priv;
  widget = GTK_WIDGET (view);

  start_grid_x = alloc->x + padding->left;
  start_grid_y = alloc->y + padding->top + priv->header_size + priv->grid_header_size;

  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);

  /* drawing selected_cell */
  //FIXME: to draw date in the selected cell if it's not in the month
  state |= GTK_STATE_FLAG_SELECTED;
  gtk_style_context_get_background_color (context, state, &color);
  cairo_set_source_rgba (cr, color.red, color.green, color.blue, 0.6);
  cairo_rectangle (cr,
                   start_grid_x + priv->horizontal_step * ( priv->selected_cell % 7),
                   start_grid_y + priv->vertical_step * ( priv->selected_cell / 7),
                   priv->horizontal_step,
                   priv->vertical_step);
  cairo_fill (cr);

  /* drawing grid */
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
      pango_layout_set_text (layout, gcal_get_weekday (i), -1);
      pango_cairo_update_layout (cr, layout);
      pango_layout_get_pixel_size (layout, &font_width, &font_height);
      /* 6: is padding around the grid-header */
      cairo_move_to (cr,
                     start_grid_x + priv->horizontal_step * i + 6,
                     start_grid_y - priv->grid_header_size);
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
          /* 2: is margin-left and 1: is margin_top for numbers */
          cairo_move_to (cr,
                         start_grid_x + priv->horizontal_step * i + 2,
                         start_grid_y + priv->vertical_step * j + 1);
          pango_cairo_show_layout (cr, layout);
          g_free (day);
        }
    }
  /* free the layout object */
  g_object_unref (layout);

  for (i = 0; i < 6; i++)
    {
      cairo_move_to (cr, start_grid_x, start_grid_y + priv->vertical_step * i);
      cairo_line_to (cr,
                     start_grid_x + priv->horizontal_step * 7,
                     start_grid_y + priv->vertical_step * i);
    }

  for (i = 0; i < 8; i++)
    {
      cairo_move_to (cr, start_grid_x + priv->horizontal_step * i, start_grid_y);
      cairo_line_to (cr,
                     start_grid_x + priv->horizontal_step * i,
                     start_grid_y + priv->vertical_step * 5);
    }

  cairo_stroke (cr);
}

/* GcalView Interface API */
/**
 * gcal_month_view_get_initial_date:
 *
 * Since: 0.1
 * Return value: the first day of the month
 * Returns: (transfer full): Release with g_free
 **/
static icaltimetype*
gcal_month_view_get_initial_date (GcalView *view)
{
  //FIXME to retrieve the 35 days range
  GcalMonthViewPrivate *priv;
  icaltimetype *new_date;

  g_return_val_if_fail (GCAL_IS_MONTH_VIEW (view), NULL);
  priv = GCAL_MONTH_VIEW (view)->priv;
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
static icaltimetype*
gcal_month_view_get_final_date (GcalView *view)
{
  //FIXME to retrieve the 35 days range
  GcalMonthViewPrivate *priv;
  icaltimetype *new_date;

  g_return_val_if_fail (GCAL_IS_MONTH_VIEW (view), NULL);
  priv = GCAL_MONTH_VIEW (view)->priv;
  new_date = gcal_dup_icaltime (priv->date);
  new_date->day = icaltime_days_in_month (priv->date->month, priv->date->year);
  return new_date;
}

static gboolean
gcal_month_view_contains (GcalView     *view,
                          icaltimetype *date)
{
  GcalMonthViewPrivate *priv;

  g_return_val_if_fail (GCAL_IS_MONTH_VIEW (view), FALSE);
  priv = GCAL_MONTH_VIEW (view)->priv;

  if (priv->date == NULL)
    return FALSE;
  if (priv->date->month == date->month
      || priv->date->year == date->year)
    {
      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

static void
gcal_month_view_remove_by_uuid (GcalView    *view,
                                const gchar *uuid)
{
  GcalMonthViewPrivate *priv;
  gint i;
  GList *l;

  g_return_if_fail (GCAL_IS_MONTH_VIEW (view));
  priv = GCAL_MONTH_VIEW (view)->priv;

  for (i = 0; i < 35; i++)
    {
      for (l = priv->days[i]; l != NULL; l = l->next)
        {
          GcalViewChild *child;
          const gchar* widget_uuid;

          child = (GcalViewChild*) l->data;
          widget_uuid = gcal_event_widget_peek_uuid (GCAL_EVENT_WIDGET (child->widget));
          if (g_strcmp0 (uuid, widget_uuid) == 0)
            {
              gtk_widget_destroy (child->widget);
              i = 36;
              break;
            }
        }
    }
}

static GtkWidget*
gcal_month_view_get_by_uuid (GcalView    *view,
                             const gchar *uuid)
{
  GcalMonthViewPrivate *priv;
  gint i;
  GList *l;

  g_return_val_if_fail (GCAL_IS_MONTH_VIEW (view), NULL);
  priv = GCAL_MONTH_VIEW (view)->priv;

  for (i = 0; i < 35; i++)
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
