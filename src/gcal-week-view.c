/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 2 -*- */
/*
 * gcal-week-view.c
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

#include "gcal-week-view.h"
#include "gcal-utils.h"
#include "gcal-view.h"
#include "gcal-event-widget.h"

#include <glib/gi18n.h>

#include <libecal/e-cal-time-util.h>

enum
{
  PROP_0,
  PROP_DATE
};

struct _GcalViewChild
{
  GtkWidget *widget;
  gboolean   hidden_by_me;

  /* vertical index */
  gint       index;
};

typedef struct _GcalViewChild GcalViewChild;

struct _GcalWeekViewPrivate
{
  /**
   * This is where we keep the refs of the child widgets.
   * Every child added to the list placed in the position
   * of it corresponding cell number.
   * The cell number is calculated in _add method.
   */
  GList          *days [7];

  GdkWindow      *event_window;

  icaltimetype   *date;

  gint            days_delay;
  gint            header_size;
  gint            grid_header_size;
  gint            grid_sidebar_size;

  /* hours_steps is the number of cell I will show vertically */
  gint            hours_steps;
  gdouble         vertical_step;
  gdouble         horizontal_step;
};

static void         gcal_view_interface_init               (GcalViewIface  *iface);

static void         gcal_week_view_set_property            (GObject        *object,
                                                            guint           property_id,
                                                            const GValue   *value,
                                                            GParamSpec     *pspec);

static void         gcal_week_view_get_property            (GObject        *object,
                                                            guint           property_id,
                                                            GValue         *value,
                                                            GParamSpec     *pspec);

static void         gcal_week_view_finalize                (GObject        *object);

static void         gcal_week_view_realize                 (GtkWidget      *widget);

static void         gcal_week_view_unrealize               (GtkWidget      *widget);

static void         gcal_week_view_map                     (GtkWidget      *widget);

static void         gcal_week_view_unmap                   (GtkWidget      *widget);

static void         gcal_week_view_size_allocate           (GtkWidget      *widget,
                                                            GtkAllocation  *allocation);

static gboolean     gcal_week_view_draw                    (GtkWidget      *widget,
                                                            cairo_t        *cr);

static void         gcal_week_view_add                     (GtkContainer   *constainer,
                                                            GtkWidget      *widget);

static void         gcal_week_view_remove                  (GtkContainer   *constainer,
                                                            GtkWidget      *widget);

static void         gcal_week_view_forall                  (GtkContainer   *container,
                                                            gboolean        include_internals,
                                                            GtkCallback     callback,
                                                            gpointer        callback_data);

static void         gcal_week_view_draw_header             (GcalWeekView   *view,
                                                            cairo_t        *cr,
                                                            GtkAllocation  *alloc,
                                                            GtkBorder      *padding);

static void         gcal_week_view_draw_grid               (GcalWeekView   *view,
                                                            cairo_t        *cr,
                                                            GtkAllocation  *alloc,
                                                            GtkBorder      *padding);

static gboolean     gcal_week_view_is_in_range             (GcalView       *view,
                                                            icaltimetype   *date);

static void         gcal_week_view_remove_by_uuid          (GcalView       *view,
                                                            const gchar    *uuid);

static GtkWidget*   gcal_week_view_get_by_uuid             (GcalView       *view,
                                                            const gchar    *uuid);

G_DEFINE_TYPE_WITH_CODE (GcalWeekView,
                         gcal_week_view,
                         GTK_TYPE_CONTAINER,
                         G_IMPLEMENT_INTERFACE (GCAL_TYPE_VIEW,
                                                gcal_view_interface_init));


static void
gcal_week_view_class_init (GcalWeekViewClass *klass)
{
  GtkContainerClass *container_class;
  GtkWidgetClass *widget_class;
  GObjectClass *object_class;

  container_class = GTK_CONTAINER_CLASS (klass);
  container_class->add   = gcal_week_view_add;
  container_class->remove = gcal_week_view_remove;
  container_class->forall = gcal_week_view_forall;

  widget_class = GTK_WIDGET_CLASS (klass);
  widget_class->realize = gcal_week_view_realize;
  widget_class->unrealize = gcal_week_view_unrealize;
  widget_class->map = gcal_week_view_map;
  widget_class->unmap = gcal_week_view_unmap;
  widget_class->size_allocate = gcal_week_view_size_allocate;
  widget_class->draw = gcal_week_view_draw;

  object_class = G_OBJECT_CLASS (klass);
  object_class->set_property = gcal_week_view_set_property;
  object_class->get_property = gcal_week_view_get_property;
  object_class->finalize = gcal_week_view_finalize;

  g_object_class_install_property (object_class,
                                   PROP_DATE,
                                   g_param_spec_boxed ("date",
                                                       "Date",
                                                       _("Date"),
                                                       ICAL_TIME_TYPE,
                                                       G_PARAM_CONSTRUCT |
                                                       G_PARAM_READWRITE));

  g_type_class_add_private ((gpointer)klass, sizeof (GcalWeekViewPrivate));
}



static void
gcal_week_view_init (GcalWeekView *self)
{
  GcalWeekViewPrivate *priv;
  gint i;

  gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            GCAL_TYPE_WEEK_VIEW,
                                            GcalWeekViewPrivate);
  priv = self->priv;

  for (i = 0; i < 7; i++)
    {
      priv->days[i] = NULL;
    }
}

static void
gcal_view_interface_init (GcalViewIface *iface)
{
  iface->is_in_range = gcal_week_view_is_in_range;
  iface->remove_by_uuid = gcal_week_view_remove_by_uuid;
  iface->get_by_uuid = gcal_week_view_get_by_uuid;
}

static void
gcal_week_view_set_property (GObject       *object,
                              guint          property_id,
                              const GValue  *value,
                              GParamSpec    *pspec)
{
  GcalWeekViewPrivate *priv = GCAL_WEEK_VIEW (object)->priv;

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
gcal_week_view_get_property (GObject       *object,
                              guint          property_id,
                              GValue        *value,
                              GParamSpec    *pspec)
{
  GcalWeekViewPrivate *priv = GCAL_WEEK_VIEW (object)->priv;

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
gcal_week_view_finalize (GObject       *object)
{
  GcalWeekViewPrivate *priv = GCAL_WEEK_VIEW (object)->priv;

  if (priv->date != NULL)
    g_free (priv->date);

  /* Chain up to parent's finalize() method. */
  G_OBJECT_CLASS (gcal_week_view_parent_class)->finalize (object);
}

static void
gcal_week_view_realize (GtkWidget *widget)
{
  GcalWeekViewPrivate *priv;
  GdkWindow *parent_window;
  GdkWindowAttr attributes;
  gint attributes_mask;
  GtkAllocation allocation;

  priv = GCAL_WEEK_VIEW (widget)->priv;
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
gcal_week_view_unrealize (GtkWidget *widget)
{
  GcalWeekViewPrivate *priv;

  priv = GCAL_WEEK_VIEW (widget)->priv;
  if (priv->event_window != NULL)
    {
      gdk_window_set_user_data (priv->event_window, NULL);
      gdk_window_destroy (priv->event_window);
      priv->event_window = NULL;
    }

  GTK_WIDGET_CLASS (gcal_week_view_parent_class)->unrealize (widget);
}

static void
gcal_week_view_map (GtkWidget *widget)
{
  GcalWeekViewPrivate *priv;

  priv = GCAL_WEEK_VIEW (widget)->priv;
  if (priv->event_window)
    gdk_window_show (priv->event_window);

  GTK_WIDGET_CLASS (gcal_week_view_parent_class)->map (widget);
}

static void
gcal_week_view_unmap (GtkWidget *widget)
{
  GcalWeekViewPrivate *priv;

  priv = GCAL_WEEK_VIEW (widget)->priv;
  if (priv->event_window)
    gdk_window_hide (priv->event_window);

  GTK_WIDGET_CLASS (gcal_week_view_parent_class)->unmap (widget);
}

static void
gcal_week_view_size_allocate (GtkWidget     *widget,
                              GtkAllocation *allocation)
{
  GcalWeekViewPrivate *priv;
  gint i;
  GList *l;

  GtkStyleContext *context;
  GtkStateFlags state;
  GtkBorder padding;

  PangoLayout *layout;
  gint font_width;
  gint font_height;

  priv = GCAL_WEEK_VIEW (widget)->priv;

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
  pango_layout_set_text (layout, _("All day"), -1);
  pango_layout_get_pixel_size (layout, &font_width, &font_height);

  /* init values */
  /* FIXME: these values should be extracted from the theme font size */
  priv->header_size = font_height + 4;
  priv->grid_header_size = font_height + 4;
  priv->grid_sidebar_size = font_width + 8;

  priv->horizontal_step = (allocation->width - (allocation->x + padding.left + padding.right + priv->grid_sidebar_size)) / 7;
  priv->vertical_step =
    (allocation->height - (allocation->y + padding.top + padding.bottom + priv->header_size + priv->grid_header_size)) / 11;
  priv->hours_steps = 1;
  if (priv->vertical_step < 2 * font_height)
    {
      priv->hours_steps = 2;
      priv->vertical_step =
        (allocation->height - (allocation->y + padding.top + padding.bottom + priv->header_size + priv->grid_header_size)) / 6;
    }

  for (i = 0; i < 7; i++)
    {
      for (l = priv->days[i]; l != NULL; l = l->next)
        {
          GcalViewChild *child;
          gdouble added_height [11];

          gint pos_x;
          gint pos_y;
          gint min_height;
          gint natural_height;
          GtkAllocation child_allocation;

          child = (GcalViewChild*) l->data;
          pos_x = i * priv->horizontal_step;
          pos_y = ((child->index + (priv->hours_steps - 1)) / (priv->hours_steps)) * priv->vertical_step;

          if ((! gtk_widget_get_visible (child->widget)) && (! child->hidden_by_me))
            continue;

          gtk_widget_get_preferred_height (child->widget,
                                           &min_height,
                                           &natural_height);

          child_allocation.x = pos_x + allocation->x + padding.left
            + priv->grid_sidebar_size;
          child_allocation.y = pos_y + allocation->y + padding.top
            + priv->header_size + priv->grid_header_size;
          child_allocation.width = priv->horizontal_step;
          child_allocation.height = MIN (natural_height, priv->vertical_step);
          if (added_height[child->index] + child_allocation.height
              > priv->vertical_step)
            {
              gtk_widget_hide (child->widget);
              child->hidden_by_me = TRUE;
            }
          else
            {
              gtk_widget_show (child->widget);
              child->hidden_by_me = FALSE;
              child_allocation.y = child_allocation.y + added_height[child->index];
              gtk_widget_size_allocate (child->widget, &child_allocation);
              added_height[child->index] += child_allocation.height;
            }
        }
    }
}

static gboolean
gcal_week_view_draw (GtkWidget *widget,
                      cairo_t   *cr)
{
  GtkStyleContext *context;
  GtkStateFlags state;
  GtkBorder padding;
  GtkAllocation alloc;

  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);

  /* getting padding and allocation */
  gtk_style_context_get_padding (context, state, &padding);
  gtk_widget_get_allocation (widget, &alloc);

  gcal_week_view_draw_header (GCAL_WEEK_VIEW (widget), cr, &alloc, &padding);
  gcal_week_view_draw_grid (GCAL_WEEK_VIEW (widget), cr, &alloc, &padding);

  if (GTK_WIDGET_CLASS (gcal_week_view_parent_class)->draw != NULL)
    GTK_WIDGET_CLASS (gcal_week_view_parent_class)->draw (widget, cr);

  return FALSE;
}

static void
gcal_week_view_add (GtkContainer *container,
                     GtkWidget    *widget)
{
  GcalWeekViewPrivate *priv;
  GList *l;

  gint day;
  icaltimetype *date;

  GcalViewChild *new_child;

  g_return_if_fail (GCAL_IS_WEEK_VIEW (container));
  g_return_if_fail (GCAL_IS_EVENT_WIDGET (widget));
  g_return_if_fail (gtk_widget_get_parent (widget) == NULL);
  priv = GCAL_WEEK_VIEW (container)->priv;

  /* Check if it's already added for date */
  date = gcal_event_widget_get_date (GCAL_EVENT_WIDGET (widget));
  day = icaltime_day_of_week (*date);

  for (l = priv->days[day - 1]; l != NULL; l = l->next)
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

  if (gcal_event_widget_get_all_day (GCAL_EVENT_WIDGET (widget)))
    new_child->index = 0;
  else
    new_child->index = date->hour + 1 - 8;

  priv->days[day - 1] = g_list_append (priv->days[day - 1], new_child);
  gtk_widget_set_parent (widget, GTK_WIDGET (container));

  g_free (date);
}

static void
gcal_week_view_remove (GtkContainer *container,
                        GtkWidget    *widget)
{
  GcalWeekViewPrivate *priv;
  GList *l;
  icaltimetype *date;
  gint day;

  g_return_if_fail (GCAL_IS_WEEK_VIEW (container));
  g_return_if_fail (gtk_widget_get_parent (widget) == GTK_WIDGET (container));
  priv = GCAL_WEEK_VIEW (container)->priv;

  date = gcal_event_widget_get_date (GCAL_EVENT_WIDGET (widget));
  day = icaltime_day_of_week (*date);

  for (l = priv->days[day - 1]; l != NULL; l = l->next)
    {
      GcalViewChild *child;

      child = (GcalViewChild*) l->data;
      if (child->widget == widget)
        {
          priv->days[day - 1] = g_list_remove (priv->days[day - 1], child);
          g_free (child);
          break;
        }
    }

  gtk_widget_unparent (widget);

  g_free (date);
}

static void
gcal_week_view_forall (GtkContainer *container,
                        gboolean      include_internals,
                        GtkCallback   callback,
                        gpointer      callback_data)
{
  GcalWeekViewPrivate *priv;
  gint i;
  GList *l;

  priv = GCAL_WEEK_VIEW (container)->priv;

  for (i = 0; i < 7; i++)
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
gcal_week_view_draw_header (GcalWeekView  *view,
                            cairo_t       *cr,
                            GtkAllocation *alloc,
                            GtkBorder     *padding)
{
  GtkStyleContext *context;
  GtkStateFlags state;
  GdkRGBA color;

  PangoLayout *layout;
  gint layout_width;

  gchar *left_header;
  gchar *right_header;
  gchar str_date[64];

  icaltimetype *start_of_week;
  struct tm tm_date;

  g_return_if_fail (GCAL_IS_WEEK_VIEW (view));

  context = gtk_widget_get_style_context (GTK_WIDGET (view));
  state = gtk_widget_get_state_flags (GTK_WIDGET (view));

  gtk_style_context_get_color (context, state, &color);
  cairo_set_source_rgba (cr, color.red, color.green, color.blue, color.alpha);

  layout = pango_cairo_create_layout (cr);
  pango_layout_set_font_description (layout,
                                     gtk_style_context_get_font (context,
                                                                 state));

  start_of_week = gcal_week_view_get_initial_date (view);
  tm_date = icaltimetype_to_tm (start_of_week);
  e_utf8_strftime_fix_am_pm (str_date, 64, "%B %d", &tm_date);

  left_header = g_strdup_printf ("%s %s", _("Week of"), str_date);
  right_header = g_strdup_printf ("%s %d",
                                  _("Week"),
                                  icaltime_week_number (*start_of_week));

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

  g_free (start_of_week);
  g_free (left_header);
  g_free (right_header);
  g_object_unref (layout);
}

static void
gcal_week_view_draw_grid (GcalWeekView  *view,
                          cairo_t       *cr,
                          GtkAllocation *alloc,
                          GtkBorder     *padding)
{
  GcalWeekViewPrivate *priv;
  GtkWidget *widget;

  GtkStyleContext *context;
  GtkStateFlags state;
  GdkRGBA color;

  gint i;
  gint font_width;
  gint font_height;

  PangoLayout *layout;

  icaltimetype *start_of_week;

  priv = view->priv;
  widget = GTK_WIDGET (view);
  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);

  /* drawing grid header */
  state = gtk_widget_get_state_flags (widget);
  gtk_style_context_get_color (context, state, &color);
  cairo_set_source_rgba (cr, color.red, color.green, color.blue, color.alpha);

  layout = pango_cairo_create_layout (cr);
  pango_layout_set_font_description (layout,
                                     gtk_style_context_get_font (context,
                                                                 state));

  start_of_week = gcal_week_view_get_initial_date (view);
  for (i = 0; i < 7; i++)
    {
      gchar *weekday_header;
      gint n_day;

      n_day = start_of_week->day + i;
      if (n_day > icaltime_days_in_month (start_of_week->month, start_of_week->year))
        n_day = n_day - icaltime_days_in_month (start_of_week->month, start_of_week->year);

      weekday_header = g_strdup_printf ("%s %d",gcal_get_weekday (i), n_day);
      pango_layout_set_text (layout, weekday_header, -1);
      pango_cairo_update_layout (cr, layout);
      pango_layout_get_pixel_size (layout, &font_width, &font_height);
      cairo_move_to (cr,
                     alloc->x + padding->left + priv->grid_sidebar_size + priv->horizontal_step * i + 1 + ((priv->horizontal_step - font_width) / 2),
                     alloc->y + padding->top + priv->header_size);
      pango_cairo_show_layout (cr, layout);

      g_free (weekday_header);
    }
  g_free (start_of_week);

  /* draw grid_sidebar */
  pango_layout_set_text (layout, _("All day"), -1);
  pango_cairo_update_layout (cr, layout);
  pango_layout_get_pixel_size (layout, &font_width, &font_height);
  cairo_move_to (cr,
                 alloc->x + padding->left + (( priv->grid_sidebar_size  - font_width) / 2),
                 alloc->y + padding->top + priv->header_size + priv->grid_header_size + ((priv->vertical_step - font_height) / 2));
  pango_cairo_show_layout (cr, layout);

  for (i = 0; i < 10 / priv->hours_steps; i++)
    {
      gchar *hours;
      gint n_hour;

      n_hour = 8 + i * priv->hours_steps;
      if (n_hour < 12)
        hours = g_strdup_printf ("%d %s", n_hour, _("AM"));
      else if (n_hour == 12)
        hours = g_strdup_printf (_("Noon"));
      else
        hours = g_strdup_printf ("%d %s", n_hour - 12, _("PM"));

      pango_layout_set_text (layout, hours, -1);
      pango_cairo_update_layout (cr, layout);
      pango_layout_get_pixel_size (layout, &font_width, &font_height);
      cairo_move_to (cr,
                     alloc->x + padding->left + (( priv->grid_sidebar_size  - font_width) / 2),
                     alloc->y + padding->top + priv->header_size + priv->grid_header_size + priv->vertical_step * (i + 1) + ((priv->vertical_step - font_height) / 2));
      pango_cairo_show_layout (cr, layout);

      g_free (hours);
    }

  /* grid, vertical lines first */
  for (i = 0; i < 8; i++)
    {
      cairo_move_to (cr,
                     alloc->x + padding->left + priv->grid_sidebar_size + priv->horizontal_step * i,
                     alloc->y + padding->top + priv->header_size + priv->grid_header_size);
      cairo_line_to (cr,
                     alloc->x + padding->left + priv->grid_sidebar_size + priv->horizontal_step * i,
                     alloc->y + padding->top + priv->header_size + priv->grid_header_size + priv->vertical_step *  ((10 / priv->hours_steps) + 1));
    }

  /* rest of the lines */
  for (i = 0; i <= 10 / priv->hours_steps + 1; i++)
    {
      cairo_move_to (cr,
                     alloc->x + padding->left + priv->grid_sidebar_size,
                     alloc->y + padding->top + priv->header_size + priv->grid_header_size + priv->vertical_step * i);
      cairo_line_to (cr,
                     alloc->x + padding->left + priv->grid_sidebar_size + priv->horizontal_step * 7,
                     alloc->y + padding->top + priv->header_size + priv->grid_header_size + priv->vertical_step * i);
    }

  cairo_stroke (cr);

  g_object_unref (layout);
}

static gboolean
gcal_week_view_is_in_range (GcalView     *view,
                             icaltimetype *date)
{
  //FIXME: Add implementation here.
  // as it should return TRUE all the time.
  return TRUE;
}

static void
gcal_week_view_remove_by_uuid (GcalView    *view,
                                const gchar *uuid)
{
  GcalWeekViewPrivate *priv;
  gint i;
  GList *l;

  g_return_if_fail (GCAL_IS_WEEK_VIEW (view));
  priv = GCAL_WEEK_VIEW (view)->priv;

  for (i = 0; i < 7; i++)
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
              i = 8;
              break;
            }
        }
    }
}

static GtkWidget*
gcal_week_view_get_by_uuid (GcalView    *view,
                            const gchar *uuid)
{
  GcalWeekViewPrivate *priv;
  gint i;
  GList *l;

  g_return_val_if_fail (GCAL_IS_WEEK_VIEW (view), NULL);
  priv = GCAL_WEEK_VIEW (view)->priv;

  for (i = 0; i < 7; i++)
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
 * gcal_week_view_new:
 * @date:
 *
 * Since: 0.1
 * Return value: the new month view widget
 * Returns: (transfer full):
 **/
GtkWidget*
gcal_week_view_new (icaltimetype *date)
{
  return g_object_new (GCAL_TYPE_WEEK_VIEW, "date", date, NULL);
}

/**
 * gcal_week_view_get_initial_date:
 *
 * Since: 0.1
 * Return value: the first day of the month
 * Returns: (transfer full): Release with g_free
 **/
icaltimetype*
gcal_week_view_get_initial_date (GcalWeekView *view)
{
  GcalWeekViewPrivate *priv;
  icaltimetype *new_date;

  priv = view->priv;
  new_date = g_new0 (icaltimetype, 1);
  *new_date = icaltime_from_day_of_year (
      icaltime_day_of_year (*(priv->date)) - icaltime_day_of_week (*(priv->date)) + 1,
      priv->date->year);

  return new_date;
}

/**
 * gcal_week_view_get_final_date:
 *
 * Since: 0.1
 * Return value: the last day of the month
 * Returns: (transfer full): Release with g_free
 **/
icaltimetype*
gcal_week_view_get_final_date (GcalWeekView *view)
{
  GcalWeekViewPrivate *priv;
  icaltimetype *new_date;

  priv = view->priv;
  new_date = g_new0 (icaltimetype, 1);
  *new_date = icaltime_from_day_of_year (
      icaltime_day_of_year (*(priv->date)) + 7 - icaltime_day_of_week (*(priv->date)),
      priv->date->year);

  return new_date;
}
