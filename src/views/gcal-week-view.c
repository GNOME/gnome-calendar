/* -*- mode: c; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gcal-week-view.c
 *
 * Copyright (C) 2015 - Erick Pérez Castellanos
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
#include "gcal-week-header.h"

#include <glib/gi18n.h>

#include <math.h>

#define ALL_DAY_CELLS_HEIGHT 40

static const double dashed [] =
{
  1.0,
  1.0
};

enum
{
  PROP_0,
  PROP_DATE
};

struct _GcalWeekViewChild
{
  GtkWidget *widget;
  gboolean   hidden_by_me;

  /* vertical index */
  gint       index;
};

typedef struct _GcalWeekViewChild GcalWeekViewChild;

typedef struct
{
  /**
   * This is where we keep the refs of the child widgets.
   * Every child added to the list placed in the position
   * of it corresponding cell number.
   * The cell number is calculated in _add method.
   * day[0] Represents the first_weekday according to locale
   */
  GList          *days [7];

  GdkWindow      *view_window;
  GdkWindow      *grid_window;

  GtkWidget      *vscrollbar;

  /**
   * first day of the week according to user locale, being
   * 0 for Sunday, 1 for Monday and so on */
  gint            first_weekday;

  /**
   * clock format from GNOME desktop settings
   */
  gboolean        use_24h_format;

  /* property */
  icaltimetype   *date;
  GcalManager    *manager; /* weak referenced */

  GdkWindow      *event_window;

  gint            clicked_cell;
} GcalWeekViewPrivate;

static void           event_opened                         (GcalEventWidget *event_widget,
                                                            gpointer         user_data);

static void           draw_header                          (GcalWeekView   *view,
                                                            cairo_t        *cr,
                                                            GtkAllocation  *alloc,
                                                            GtkBorder      *padding);

static void           draw_grid_window                     (GcalWeekView   *view,
                                                            cairo_t        *cr);

static void           gcal_week_view_constructed           (GObject        *object);

static void           gcal_week_view_finalize              (GObject        *object);

static void           gcal_week_view_set_property          (GObject        *object,
                                                            guint           property_id,
                                                            const GValue   *value,
                                                            GParamSpec     *pspec);

static void           gcal_week_view_get_property          (GObject        *object,
                                                            guint           property_id,
                                                            GValue         *value,
                                                            GParamSpec     *pspec);

static void           gcal_week_view_realize               (GtkWidget      *widget);

static void           gcal_week_view_unrealize             (GtkWidget      *widget);

static void           gcal_week_view_size_allocate         (GtkWidget      *widget,
                                                            GtkAllocation  *allocation);

static gboolean       gcal_week_view_draw                  (GtkWidget      *widget,
                                                            cairo_t        *cr);

static gboolean       gcal_week_view_scroll_event          (GtkWidget      *widget,
                                                            GdkEventScroll *event);

static gboolean       gcal_week_view_button_press_event    (GtkWidget      *widget,
                                                            GdkEventButton *event);

static gboolean       gcal_week_view_button_release_event  (GtkWidget      *widget,
                                                            GdkEventButton *event);

static void           gcal_week_view_add                   (GtkContainer   *constainer,
                                                            GtkWidget      *widget);

static void           gcal_week_view_remove                (GtkContainer   *constainer,
                                                            GtkWidget      *widget);

static void           gcal_week_view_forall                (GtkContainer   *container,
                                                            gboolean        include_internals,
                                                            GtkCallback     callback,
                                                            gpointer        callback_data);

static void           gcal_week_view_scroll_value_changed  (GtkAdjustment  *adjusment,
                                                            gpointer        user_data);

static icaltimetype*  gcal_week_view_get_final_date        (GcalView       *view);

static GtkWidget*     gcal_week_view_get_by_uuid           (GcalSubscriberView *view,
                                                            const gchar        *uuid);

static void           gcal_view_interface_init             (GcalViewInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GcalWeekView, gcal_week_view, GCAL_TYPE_SUBSCRIBER_VIEW,
                         G_ADD_PRIVATE (GcalWeekView)
                         G_IMPLEMENT_INTERFACE (GCAL_TYPE_VIEW, gcal_view_interface_init));

/**
 * gcal_week_view_get_start_grid_y:
 *
 * In GcalMonthView this method returns the height of the headers of the view
 * and the grid. Here this points just the place where the grid_window hides
 * behind the header
 * Here this height includes:
 *  - The big header of the view
 *  - The grid header dislaying weekdays
 *  - The cell containing all-day events.
 */
gint
gcal_week_view_get_start_grid_y (GtkWidget *widget)
{
  GtkStyleContext *context;
  GtkStateFlags flags;
  GtkBorder padding;

  PangoLayout *layout;
  PangoFontDescription *font_desc;
  gint font_height;
  gdouble start_grid_y;

  context = gtk_widget_get_style_context (widget);
  flags = gtk_widget_get_state_flags (widget);

  layout = gtk_widget_create_pango_layout (widget, NULL);

  /* init header values */
  gtk_style_context_get_padding (context, flags, &padding);

  gtk_style_context_get (context, flags, "font", &font_desc, NULL);

  pango_font_description_set_weight (font_desc, PANGO_WEIGHT_SEMIBOLD);
  pango_layout_set_font_description (layout, font_desc);
  pango_layout_get_pixel_size (layout, NULL, &font_height);
  pango_font_description_free (font_desc);

  /* 6: is padding around the header */
  start_grid_y = font_height + padding.bottom;

  /* for including the all-day cells */
  start_grid_y += ALL_DAY_CELLS_HEIGHT;

  g_object_unref (layout);
  return start_grid_y;
}

static void
event_opened (GcalEventWidget *event_widget,
              gpointer         user_data)
{
  g_signal_emit_by_name (GCAL_VIEW (user_data),
                         "event-activated",
                         event_widget);
}

static void
draw_header (GcalWeekView  *view,
             cairo_t       *cr,
             GtkAllocation *alloc,
             GtkBorder     *padding)
{
  GcalWeekViewPrivate *priv;
  GtkWidget *widget;
  GtkStyleContext *context;
  GtkStateFlags state;
  GdkRGBA color;
  /* GdkRGBA current_color; */

  PangoLayout *layout;
  PangoFontDescription *bold_font;

  gint i;
  gint pos_i;
  gint start_grid_y;
  gint font_height;
  gdouble sidebar_width;
  gdouble cell_width;
  icaltimetype *start_of_week;
  gint current_cell;

  cairo_pattern_t *pattern;

  priv = gcal_week_view_get_instance_private (view);
  widget = GTK_WIDGET (view);

  cairo_save (cr);
  start_grid_y = gcal_week_view_get_start_grid_y (widget);
  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);

  /* adding shadow */
  pattern = cairo_pattern_create_linear(0, start_grid_y - 18,
                                        0, start_grid_y + 6);

  cairo_pattern_add_color_stop_rgba(pattern, 0.0, 0, 0, 0, 0.6);
  cairo_pattern_add_color_stop_rgba(pattern, 1.0, 0, 0, 0, 0.0);

  cairo_set_source(cr, pattern);
  cairo_pattern_destroy(pattern);

  cairo_rectangle(cr, 0, start_grid_y, alloc->width, 6);
  cairo_fill(cr);

  gtk_style_context_get_color (context, state, &color);
  gdk_cairo_set_source_rgba (cr, &color);

  /* grid header */
  layout = pango_cairo_create_layout (cr);
  gtk_style_context_get (context, state, "font", &bold_font, NULL);
  pango_font_description_set_weight (bold_font, PANGO_WEIGHT_SEMIBOLD);
  pango_layout_set_font_description (layout, bold_font);

  start_of_week = gcal_week_view_get_initial_date (GCAL_VIEW (view));
  current_cell = icaltime_day_of_week (*(priv->date)) - 1;
  current_cell = (7 + current_cell - priv->first_weekday) % 7;

  sidebar_width = gcal_week_view_get_sidebar_width (widget);
  cell_width = (alloc->width - sidebar_width) / 7;
  pango_layout_get_pixel_size (layout, NULL, &font_height);

  gtk_style_context_save (context);
  gtk_style_context_add_class (context, "current");
  gtk_render_background (context, cr,
                         cell_width * current_cell + sidebar_width,
                         font_height + padding->bottom,
                         cell_width,
                         ALL_DAY_CELLS_HEIGHT);
  gtk_style_context_remove_class (context, "current");
  gtk_style_context_restore (context);

  for (i = 0; i < 7; i++)
    {
      gchar *weekday_header;
      gchar *weekday_abv;
      gint n_day;

      n_day = start_of_week->day + i;
      if (n_day > icaltime_days_in_month (start_of_week->month,
                                          start_of_week->year))
        {
          n_day = n_day - icaltime_days_in_month (start_of_week->month,
                                                  start_of_week->year);
        }

      weekday_abv = gcal_get_weekday ((i + priv->first_weekday) % 7);
      weekday_header = g_strdup_printf ("%s %d", weekday_abv, n_day);

      pango_layout_set_text (layout, weekday_header, -1);
      cairo_move_to (cr,
                     padding->left + cell_width * i + sidebar_width,
                     0.0);
      pango_cairo_show_layout (cr, layout);

      cairo_save (cr);
      cairo_move_to (cr,
                     cell_width * i + sidebar_width + 0.3,
                     font_height + padding->bottom);
      cairo_rel_line_to (cr, 0.0, ALL_DAY_CELLS_HEIGHT);
      cairo_stroke (cr);
      cairo_restore (cr);

      g_free (weekday_header);
    }

  /* horizontal line */
  gtk_style_context_get_color (context,
                               state | GTK_STATE_FLAG_INSENSITIVE,
                               &color);
  gdk_cairo_set_source_rgba (cr, &color);
  pos_i = font_height + padding->bottom;
  cairo_move_to (cr, sidebar_width, pos_i + 0.3);
  cairo_rel_line_to (cr, alloc->width - sidebar_width, 0);

  cairo_stroke (cr);

  cairo_restore (cr);

  g_free (start_of_week);
  pango_font_description_free (bold_font);
  g_object_unref (layout);
}

static void
draw_grid_window (GcalWeekView  *view,
                  cairo_t       *cr)
{
  GcalWeekViewPrivate *priv;
  GtkWidget *widget;

  GtkStyleContext *context;
  GtkStateFlags state;
  GtkBorder padding;
  GdkRGBA color;

  gint i;
  gint width;
  gint height;
  gdouble sidebar_width;
  gint current_cell;

  PangoLayout *layout;
  PangoFontDescription *font_desc;

  priv = gcal_week_view_get_instance_private (view);
  widget = GTK_WIDGET (view);

  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);

  gtk_style_context_get_color (context, state, &color);
  gtk_style_context_get_padding (context, state, &padding);
  gtk_style_context_get (context, state, "font", &font_desc, NULL);

  layout = pango_cairo_create_layout (cr);
  pango_layout_set_font_description (layout, font_desc);
  gdk_cairo_set_source_rgba (cr, &color);

  sidebar_width = gcal_week_view_get_sidebar_width (widget);
  width = gdk_window_get_width (priv->grid_window);
  height = gdk_window_get_height (priv->grid_window);

  current_cell = icaltime_day_of_week (*(priv->date)) - 1;
  current_cell = (7 + current_cell - priv->first_weekday) % 7;

  gtk_cairo_transform_to_window (cr, widget, priv->grid_window);

  gtk_style_context_save (context);
  gtk_style_context_add_class (context, "current");
  gtk_render_background (
      context, cr,
      ((width - sidebar_width)/ 7.0) * current_cell + sidebar_width,
      0,
      ((width  - sidebar_width)/ 7.0),
      height);
  gtk_style_context_remove_class (context, "current");
  gtk_style_context_restore (context);

  /* grid, sidebar hours */
  for (i = 0; i < 24; i++)
    {
      gchar *hours;
      if (priv->use_24h_format)
        {
          hours = g_strdup_printf ("%02d:00", i);
          pango_layout_set_text (layout, hours, -1);
        }
      else
        {
          hours = g_strdup_printf ("%02d:00 %s",
                                   i % 12,
                                   i < 12 ? _("AM") : _("PM"));

          if (i == 0)
            pango_layout_set_text (layout, _("Midnight"), -1);
          else if (i == 12)
            pango_layout_set_text (layout, _("Noon"), -1);
          else
            pango_layout_set_text (layout, hours, -1);
        }

      cairo_move_to (cr, padding.left, padding.top + (height / 24) * i);
      pango_cairo_show_layout (cr, layout);

      g_free (hours);
    }

  /* grid, vertical lines first */
  for (i = 0; i < 7; i++)
    {
      cairo_move_to (cr,
                     sidebar_width + ((width - sidebar_width) / 7) * i + 0.4,
                     0);
      cairo_rel_line_to (cr, 0, height);
    }

  /* rest of the lines */
  for (i = 0; i < 24; i++)
    {
      /* hours lines */
      cairo_move_to (cr, 0, (height / 24) * i + 0.4);
      cairo_rel_line_to (cr, width, 0);
    }

  cairo_stroke (cr);

  cairo_set_dash (cr, dashed, 2, 0);
  for (i = 0; i < 24; i++)
    {
      /* 30 minutes lines */
      cairo_move_to (cr, sidebar_width, (height / 24) * i + (height / 48) + 0.4);
      cairo_rel_line_to (cr, width - sidebar_width, 0);
    }

  cairo_stroke (cr);

  pango_font_description_free (font_desc);
  g_object_unref (layout);
}

gint
gcal_week_view_get_sidebar_width (GtkWidget *widget)
{
  GtkStyleContext *context;
  GtkBorder padding;

  PangoLayout *layout;
  PangoFontDescription *font_desc;
  gint mid_width;
  gint noon_width;
  gint hours_width;
  gint sidebar_width;

  context = gtk_widget_get_style_context (widget);

  gtk_style_context_get_padding (
      gtk_widget_get_style_context (widget),
      gtk_widget_get_state_flags (widget),
      &padding);

  layout = pango_layout_new (gtk_widget_get_pango_context (widget));
  gtk_style_context_get (context,
                         gtk_widget_get_state_flags(widget),
                         "font", &font_desc,
                         NULL);
  pango_layout_set_font_description (layout, font_desc);

  pango_layout_set_text (layout, _("Midnight"), -1);
  pango_layout_get_pixel_size (layout, &mid_width, NULL);

  pango_layout_set_text (layout, _("Noon"), -1);
  pango_layout_get_pixel_size (layout, &noon_width, NULL);

  pango_layout_set_text (layout, _("00:00 PM"), -1);
  pango_layout_get_pixel_size (layout, &hours_width, NULL);

  sidebar_width = noon_width > mid_width ? noon_width : mid_width;
  sidebar_width = sidebar_width > hours_width ? 0 : hours_width;
  sidebar_width += padding.left + padding.right;

  pango_font_description_free (font_desc);
  g_object_unref (layout);

  return sidebar_width;
}

static void
gcal_week_view_class_init (GcalWeekViewClass *klass)
{
  GcalSubscriberViewClass *subscriber_view_class;
  GtkContainerClass *container_class;
  GtkWidgetClass *widget_class;
  GObjectClass *object_class;

  subscriber_view_class = GCAL_SUBSCRIBER_VIEW_CLASS (klass);
  subscriber_view_class->get_child_by_uuid = gcal_week_view_get_by_uuid;

  container_class = GTK_CONTAINER_CLASS (klass);
  container_class->add = gcal_week_view_add;
  container_class->remove = gcal_week_view_remove;
  container_class->forall = gcal_week_view_forall;
  gtk_container_class_handle_border_width (container_class);

  widget_class = GTK_WIDGET_CLASS (klass);
  widget_class->realize = gcal_week_view_realize;
  widget_class->unrealize = gcal_week_view_unrealize;
  widget_class->size_allocate = gcal_week_view_size_allocate;
  widget_class->draw = gcal_week_view_draw;
  widget_class->scroll_event = gcal_week_view_scroll_event;
  widget_class->button_press_event = gcal_week_view_button_press_event;
  widget_class->button_release_event = gcal_week_view_button_release_event;

  object_class = G_OBJECT_CLASS (klass);
  object_class->constructed = gcal_week_view_constructed;
  object_class->finalize = gcal_week_view_finalize;
  object_class->set_property = gcal_week_view_set_property;
  object_class->get_property = gcal_week_view_get_property;

  g_object_class_override_property (object_class,
                                    PROP_DATE, "active-date");
}

static void
gcal_week_view_init (GcalWeekView *self)
{
  gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);
  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (self)), "calendar-view");
}

static void
gcal_view_interface_init (GcalViewInterface *iface)
{
  iface->get_initial_date = gcal_week_view_get_initial_date;
  iface->get_final_date = gcal_week_view_get_final_date;

  /* iface->clear_marks = gcal_week_view_clear_marks; */
}

static void
gcal_week_view_constructed (GObject *object)
{
  GcalWeekViewPrivate *priv;

  g_return_if_fail (GCAL_IS_WEEK_VIEW (object));
  priv = gcal_week_view_get_instance_private (GCAL_WEEK_VIEW (object));

  if (G_OBJECT_CLASS (gcal_week_view_parent_class)->constructed != NULL)
      G_OBJECT_CLASS (gcal_week_view_parent_class)->constructed (object);

  priv->vscrollbar = gtk_scrollbar_new (GTK_ORIENTATION_VERTICAL, NULL);

  gtk_widget_set_parent (priv->vscrollbar, GTK_WIDGET (object));
  gtk_widget_show (priv->vscrollbar);

  g_signal_connect (gtk_range_get_adjustment (GTK_RANGE (priv->vscrollbar)),
                    "value-changed",
                    G_CALLBACK (gcal_week_view_scroll_value_changed),
                    object);

  g_object_ref (priv->vscrollbar);
}

static void
gcal_week_view_finalize (GObject       *object)
{
  GcalWeekViewPrivate *priv;

  priv = gcal_week_view_get_instance_private (GCAL_WEEK_VIEW (object));

  if (priv->date != NULL)
    g_free (priv->date);

  if (priv->vscrollbar != NULL)
    {
      gtk_widget_unparent (priv->vscrollbar);
      g_clear_object (&(priv->vscrollbar));
    }

  /* Chain up to parent's finalize() method. */
  G_OBJECT_CLASS (gcal_week_view_parent_class)->finalize (object);
}

static void
gcal_week_view_set_property (GObject       *object,
                             guint          property_id,
                             const GValue  *value,
                             GParamSpec    *pspec)
{
  GcalWeekViewPrivate *priv;

  priv = gcal_week_view_get_instance_private (GCAL_WEEK_VIEW (object));

  switch (property_id)
    {
    case PROP_DATE:
      {
        time_t range_start, range_end;
        icaltimetype *date;
        icaltimezone* default_zone;

        if (priv->date != NULL)
          g_free (priv->date);

        priv->date = g_value_dup_boxed (value);

        default_zone =
          gcal_manager_get_system_timezone (priv->manager);
        date = gcal_view_get_initial_date (GCAL_VIEW (object));
        range_start = icaltime_as_timet_with_zone (*date,
                                                   default_zone);
        g_free (date);
        date = gcal_view_get_final_date (GCAL_VIEW (object));
        range_end = icaltime_as_timet_with_zone (*date,
                                                 default_zone);
        g_free (date);
        gcal_manager_set_subscriber (priv->manager,
                                     E_CAL_DATA_MODEL_SUBSCRIBER (object),
                                     range_start,
                                     range_end);
        gtk_widget_queue_draw (GTK_WIDGET (object));
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
  GcalWeekViewPrivate *priv;

  g_return_if_fail (GCAL_IS_WEEK_VIEW (object));
  priv = gcal_week_view_get_instance_private (GCAL_WEEK_VIEW (object));

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
gcal_week_view_realize (GtkWidget *widget)
{
  GcalWeekViewPrivate *priv;
  GdkWindow *parent_window;
  GdkWindowAttr attributes;
  gint attributes_mask;
  GtkAllocation allocation;

  gint i;
  GList *l;

  priv = gcal_week_view_get_instance_private (GCAL_WEEK_VIEW (widget));
  gtk_widget_set_realized (widget, TRUE);

  gtk_widget_get_allocation (widget, &allocation);

  parent_window = gtk_widget_get_parent_window (widget);
  gtk_widget_set_window (widget, parent_window);
  g_object_ref (parent_window);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.x = allocation.x;
  attributes.y = allocation.y;
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= (GDK_BUTTON_PRESS_MASK |
                            GDK_BUTTON_RELEASE_MASK);
  attributes_mask = GDK_WA_X | GDK_WA_Y;

  priv->event_window = gdk_window_new (parent_window,
                                       &attributes,
                                       attributes_mask);
  gtk_widget_register_window (widget, priv->event_window);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.x = allocation.x;
  attributes.y = allocation.y;
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= (GDK_EXPOSURE_MASK |
                            GDK_BUTTON_PRESS_MASK |
                            GDK_BUTTON_RELEASE_MASK |
                            GDK_BUTTON_MOTION_MASK |
                            GDK_POINTER_MOTION_MASK |
                            GDK_SCROLL_MASK |
                            GDK_TOUCH_MASK |
                            GDK_SMOOTH_SCROLL_MASK);
  attributes.visual = gtk_widget_get_visual (widget);
  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;

  priv->view_window = gdk_window_new (parent_window,
                                      &attributes,
                                      attributes_mask);
  gtk_widget_register_window (widget, priv->view_window);

  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= GDK_EXPOSURE_MASK;
  priv->grid_window = gdk_window_new (priv->view_window,
                                      &attributes,
                                      attributes_mask);
  gtk_widget_register_window (widget, priv->grid_window);

  gdk_window_show (priv->event_window);
  gdk_window_show (priv->grid_window);
  gdk_window_show (priv->view_window);

  /* setting parent_window for every event child */
  for (i = 0; i < 7; i++)
    {
      for (l = priv->days[i]; l != NULL; l = l->next)
        {
          GcalWeekViewChild *child;

          child = (GcalWeekViewChild*) l->data;
          gtk_widget_set_parent_window (child->widget,
                                        priv->grid_window);
        }
    }
}

static void
gcal_week_view_unrealize (GtkWidget *widget)
{
  GcalWeekViewPrivate *priv;

  priv = gcal_week_view_get_instance_private (GCAL_WEEK_VIEW (widget));
  if (priv->view_window != NULL)
    {
      gtk_widget_unregister_window (widget, priv->view_window);
      gdk_window_destroy (priv->view_window);
      priv->view_window = NULL;
    }

  if (priv->grid_window != NULL)
    {
      gtk_widget_unregister_window (widget, priv->grid_window);
      gdk_window_destroy (priv->grid_window);
      priv->grid_window = NULL;
    }
  if (priv->event_window != NULL)
    {
      gtk_widget_unregister_window (widget, priv->event_window);
      gdk_window_destroy (priv->event_window);
      priv->event_window = NULL;
    }

  GTK_WIDGET_CLASS (gcal_week_view_parent_class)->unrealize (widget);
}

static void
gcal_week_view_size_allocate (GtkWidget     *widget,
                              GtkAllocation *allocation)
{
  GcalWeekViewPrivate *priv;

  GtkBorder padding;
  PangoLayout *layout;
  PangoFontDescription *font_desc;
  gint font_height;

  gint start_grid_y;
  gint sidebar_width;

  GtkAllocation scroll_allocation;
  gint min;
  gint natural;

  gdouble horizontal_block;
  gdouble vertical_block;
  gdouble adj_value;

  gint i;
  GList *l;

  gboolean scroll_needed;
  gint grid_height;
  gint view_height;

  priv = gcal_week_view_get_instance_private (GCAL_WEEK_VIEW (widget));

  gtk_widget_set_allocation (widget, allocation);

  if (! gtk_widget_get_realized (widget))
    return;

  gtk_style_context_get_padding (
      gtk_widget_get_style_context (widget),
      gtk_widget_get_state_flags (widget),
      &padding);

  start_grid_y = gcal_week_view_get_start_grid_y (widget);

  gdk_window_move_resize (priv->event_window,
                          allocation->x,
                          allocation->y,
                          allocation->width,
                          start_grid_y);

  /* Estimating cell height */
  layout = pango_layout_new (gtk_widget_get_pango_context (widget));
  gtk_style_context_get (gtk_widget_get_style_context (widget),
                         gtk_widget_get_state_flags (widget),
                         "font", &font_desc,
                         NULL);
  pango_layout_set_font_description (layout, font_desc);
  pango_layout_get_pixel_size (layout, NULL, &font_height);
  pango_font_description_free (font_desc);

  gtk_widget_get_preferred_width (priv->vscrollbar, &min, &natural);

  view_height = allocation->height - start_grid_y;
  grid_height = 24 * (padding.top + padding.bottom + 3 * font_height);
  scroll_needed = grid_height > allocation->height - start_grid_y ? TRUE : FALSE;
  grid_height = scroll_needed ? grid_height : allocation->height - start_grid_y;

  if (scroll_needed)
    {
      adj_value = gtk_adjustment_get_value (
          gtk_range_get_adjustment (GTK_RANGE (priv->vscrollbar)));
    }
  else
    {
      adj_value = 0;
    }

  gdk_window_move_resize (priv->view_window,
                          allocation->x,
                          allocation->y + start_grid_y,
                          allocation->width,
                          allocation->height - start_grid_y);
  gdk_window_move_resize (priv->grid_window,
                          allocation->x,
                          - adj_value,
                          allocation->width,
                          grid_height);

  if (scroll_needed)
    {
      scroll_allocation.x = allocation->width - natural;
      scroll_allocation.y = start_grid_y;
      scroll_allocation.width = natural;
      scroll_allocation.height = view_height;
      gtk_widget_size_allocate (priv->vscrollbar, &scroll_allocation);

      gtk_adjustment_set_page_size (
          gtk_range_get_adjustment (GTK_RANGE (priv->vscrollbar)),
          view_height * view_height / grid_height);
      gtk_adjustment_set_upper (
          gtk_range_get_adjustment (GTK_RANGE (priv->vscrollbar)),
          grid_height - view_height + (view_height * view_height / grid_height));
    }
  else
    {
      gtk_widget_hide (priv->vscrollbar);
    }

  sidebar_width = gcal_week_view_get_sidebar_width (widget);
  horizontal_block = (allocation->width - sidebar_width) / 7.0;
  vertical_block = gdk_window_get_height (priv->grid_window) / 24.0;

  for (i = 0; i < 7; i++)
    {
      gdouble *added_height;
      added_height = g_malloc0 (sizeof (gdouble) * 25);

      for (l = priv->days[i]; l != NULL; l = l->next)
        {
          GcalWeekViewChild *child;
          gint min_height;
          gint natural_height;
          gdouble vertical_span;
          GtkAllocation child_allocation;

          child = (GcalWeekViewChild*) l->data;

          if ((! gtk_widget_get_visible (child->widget)) &&
              (! child->hidden_by_me))
            {
              continue;
            }

          gtk_widget_get_preferred_height (child->widget,
                                           &min_height,
                                           &natural_height);

          child_allocation.x = i * horizontal_block + sidebar_width;
          if (child->index == -1)
            {
              vertical_span = 2 * font_height;
              child_allocation.y = start_grid_y - vertical_span;
            }
          else
            {
              vertical_span = vertical_block;
              child_allocation.y = child->index * vertical_block;
            }

          child_allocation.width = horizontal_block;
          child_allocation.height = MIN (natural_height, vertical_span);
          if (added_height[child->index + 1] + child_allocation.height > vertical_span)
            {
              gtk_widget_hide (child->widget);
              child->hidden_by_me = TRUE;
            }
          else
            {
              gtk_widget_show (child->widget);
              child->hidden_by_me = FALSE;
              child_allocation.y = child_allocation.y + added_height[child->index + 1];
              gtk_widget_size_allocate (child->widget, &child_allocation);
              added_height[child->index + 1] += child_allocation.height;
            }
        }
      g_free (added_height);
    }
}

static gboolean
gcal_week_view_draw (GtkWidget *widget,
                     cairo_t   *cr)
{
  GcalWeekViewPrivate *priv;
  GtkStyleContext *context;
  GtkStateFlags state;
  GtkBorder padding;
  GtkAllocation alloc;

  g_return_val_if_fail (GCAL_IS_WEEK_VIEW (widget), FALSE);
  priv = gcal_week_view_get_instance_private (GCAL_WEEK_VIEW (widget));

  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);

  /* getting padding and allocation */
  gtk_style_context_get_padding (context, state, &padding);
  gtk_widget_get_allocation (widget, &alloc);

  if (gtk_cairo_should_draw_window (cr, gtk_widget_get_window (widget)))
    {
      cairo_save (cr);
      cairo_set_line_width (cr, 0.4);
      draw_header (GCAL_WEEK_VIEW (widget), cr, &alloc, &padding);
      cairo_restore (cr);
    }

  if (priv->view_window != NULL &&
      gtk_cairo_should_draw_window (cr, priv->view_window))
    {
      gint y;

      gdk_window_get_position (priv->view_window, NULL, &y);
      cairo_rectangle (cr,
                       alloc.x,
                       y,
                       alloc.width,
                       gdk_window_get_height (priv->view_window));
      cairo_clip (cr);
    }

  if (priv->grid_window != NULL &&
      gtk_cairo_should_draw_window (cr, priv->grid_window))
    {
      cairo_save (cr);
      cairo_set_line_width (cr, 0.3);
      draw_grid_window (GCAL_WEEK_VIEW (widget), cr);
      cairo_restore (cr);
    }

  GTK_WIDGET_CLASS (gcal_week_view_parent_class)->draw (widget, cr);

  return FALSE;
}

static gboolean
gcal_week_view_scroll_event (GtkWidget      *widget,
                             GdkEventScroll *event)
{
  GcalWeekViewPrivate *priv;

  gdouble delta_y;
  gdouble delta;

  g_return_val_if_fail (GCAL_IS_WEEK_VIEW (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  priv = gcal_week_view_get_instance_private (GCAL_WEEK_VIEW (widget));

  if (gdk_event_get_scroll_deltas ((GdkEvent *) event, NULL, &delta_y))
    {
      if (delta_y != 0.0 &&
          priv->vscrollbar  != NULL &&
          gtk_widget_get_visible (priv->vscrollbar))
        {
          GtkAdjustment *adj;
          gdouble new_value;
          gdouble page_size;
          gdouble scroll_unit;

          adj = gtk_range_get_adjustment (GTK_RANGE (priv->vscrollbar));
          page_size = gtk_adjustment_get_page_size (adj);
          scroll_unit = pow (page_size, 2.0 / 3.0);

          new_value = CLAMP (gtk_adjustment_get_value (adj) + delta_y * scroll_unit,
                             gtk_adjustment_get_lower (adj),
                             gtk_adjustment_get_upper (adj) -
                             gtk_adjustment_get_page_size (adj));

          gtk_adjustment_set_value (adj, new_value);

          return TRUE;
        }
    }
  else
    {
      GtkWidget *range = NULL;

      if (event->direction == GDK_SCROLL_UP ||
          event->direction == GDK_SCROLL_DOWN)
        {
          range = priv->vscrollbar;
        }

      if (range != NULL && gtk_widget_get_visible (range))
        {
          GtkAdjustment *adj;
          gdouble new_value;
          gdouble page_size;

          adj = gtk_range_get_adjustment (GTK_RANGE (range));
          page_size = gtk_adjustment_get_page_size (adj);
          delta = pow (page_size, 2.0 / 3.0);

          new_value = CLAMP (gtk_adjustment_get_value (adj) + delta,
                             gtk_adjustment_get_lower (adj),
                             gtk_adjustment_get_upper (adj) -
                             gtk_adjustment_get_page_size (adj));

          gtk_adjustment_set_value (adj, new_value);

          return TRUE;
        }
    }

  return FALSE;
}

static gboolean
gcal_week_view_button_press_event (GtkWidget      *widget,
                                   GdkEventButton *event)
{
  gdouble y, start_grid_y;

  y = event->y;

  start_grid_y = gcal_week_view_get_start_grid_y (widget);

  if (y - start_grid_y < 0)
    {
      /* FIXME: click on the header should create all day events */
      return FALSE;
    }

  return FALSE;
}

static gboolean
gcal_week_view_button_release_event (GtkWidget      *widget,
                                     GdkEventButton *event)
{
  GcalWeekViewPrivate *priv;
  gdouble y, start_grid_y;

  priv = gcal_week_view_get_instance_private (GCAL_WEEK_VIEW (widget));

  y = event->y;

  start_grid_y = gcal_week_view_get_start_grid_y (widget);

  if (y - start_grid_y < 0)
    {
      /* FIXME: click on the header should create all day events */
      priv->clicked_cell = -1;
      return FALSE;
    }

  priv->clicked_cell = -1;
  return FALSE;
}

static void
gcal_week_view_add (GtkContainer *container,
                     GtkWidget    *widget)
{
  GcalWeekViewPrivate *priv;
  GList *l;

  gint day;

  GcalWeekViewChild *new_child;
  GDateTime *date;
  GcalEvent *event;

  g_return_if_fail (GCAL_IS_WEEK_VIEW (container));
  g_return_if_fail (GCAL_IS_EVENT_WIDGET (widget));
  g_return_if_fail (gtk_widget_get_parent (widget) == NULL);
  priv = gcal_week_view_get_instance_private (GCAL_WEEK_VIEW (container));

  /* Check if it's already added for date */
  event = gcal_event_widget_get_event (GCAL_EVENT_WIDGET (widget));
  date = gcal_event_get_date_start (event);
  day = (g_date_time_get_day_of_week (date) - priv->first_weekday + 6) % 7;

  for (l = priv->days[day]; l != NULL; l = l->next)
    {
      GtkWidget *event_widget;

      event_widget = GTK_WIDGET (((GcalWeekViewChild*) l->data)->widget);
      if (gcal_event_widget_equal (GCAL_EVENT_WIDGET (widget),
                                   GCAL_EVENT_WIDGET (event_widget)))
        {
          //TODO: remove once the main-dev phase its over
          g_warning ("Trying to add an event with the same uuid to the view");
          g_free (date);
          gtk_widget_destroy (widget);
          return;
        }
    }

  new_child = g_new0 (GcalWeekViewChild, 1);
  new_child->widget = widget;
  new_child->hidden_by_me = FALSE;

  if (gcal_event_get_all_day (event))
    {
      new_child->index = -1;
      if (gtk_widget_get_window (widget) != NULL)
        gtk_widget_set_parent_window (widget,
                                      gtk_widget_get_window (widget));
    }
  else
    {
      new_child->index = g_date_time_get_hour (date);
      if (priv->grid_window != NULL)
        gtk_widget_set_parent_window (widget, priv->grid_window);
    }

  priv->days[day] = g_list_append (priv->days[day], new_child);
  gtk_widget_set_parent (widget, GTK_WIDGET (container));

  g_signal_connect (widget,
                    "activate",
                    G_CALLBACK (event_opened),
                    container);
}

static void
gcal_week_view_remove (GtkContainer *container,
                        GtkWidget    *widget)
{
  GcalWeekViewPrivate *priv;
  GcalEvent *event;
  GDateTime *date;
  GList *l;
  gint day;
  gboolean was_visible;

  g_return_if_fail (GCAL_IS_WEEK_VIEW (container));
  g_return_if_fail (gtk_widget_get_parent (widget) == GTK_WIDGET (container));
  priv = gcal_week_view_get_instance_private (GCAL_WEEK_VIEW (container));
  event = gcal_event_widget_get_event (GCAL_EVENT_WIDGET (widget));
  date = gcal_event_get_date_start (event);
  day = (g_date_time_get_day_of_week (date) - priv->first_weekday + 6) % 7;

  for (l = priv->days[day]; l != NULL; l = l->next)
    {
      GcalWeekViewChild *child;

      child = (GcalWeekViewChild*) l->data;
      if (child->widget == widget)
        {
          priv->days[day] = g_list_remove (priv->days[day], child);
          g_free (child);
          break;
        }
    }


  was_visible = gtk_widget_get_visible (widget);
  gtk_widget_unparent (widget);

  if (was_visible)
    gtk_widget_queue_resize (GTK_WIDGET (container));

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

  priv = gcal_week_view_get_instance_private (GCAL_WEEK_VIEW (container));

  for (i = 0; i < 7; i++)
    {
      l = priv->days[i];

      while (l)
        {
          GcalWeekViewChild *child;

          child = (GcalWeekViewChild*) l->data;
          l  = l->next;

          (* callback) (child->widget, callback_data);
        }
    }

  if (include_internals && priv->vscrollbar != NULL)
    (* callback) (priv->vscrollbar, callback_data);
}

static void
gcal_week_view_scroll_value_changed (GtkAdjustment *adjusment,
                                     gpointer       user_data)
{
  GcalWeekViewPrivate *priv;

  g_return_if_fail (GCAL_IS_WEEK_VIEW (user_data));
  priv = gcal_week_view_get_instance_private (GCAL_WEEK_VIEW (user_data));

  if (priv->grid_window != NULL)
    {
      gdk_window_move (priv->grid_window,
                       0,
                       - gtk_adjustment_get_value (adjusment));
    }
}

/* GcalView Interface API */
/**
 * gcal_week_view_get_initial_date:
 *
 * Since: 0.1
 * Return value: the first day of the week
 * Returns: (transfer full): Release with g_free()
 **/
icaltimetype*
gcal_week_view_get_initial_date (GcalView *view)
{
  GcalWeekViewPrivate *priv;
  icaltimetype *new_date;

  g_return_val_if_fail (GCAL_IS_WEEK_VIEW (view), NULL);
  priv = gcal_week_view_get_instance_private (GCAL_WEEK_VIEW(view));
  new_date = g_new0 (icaltimetype, 1);
  *new_date = icaltime_from_day_of_year (
                  icaltime_start_doy_week (*(priv->date), priv->first_weekday + 1),
                  priv->date->year);
  new_date->is_date = 0;
  new_date->hour = 0;
  new_date->minute = 0;
  new_date->second = 0;
  *new_date = icaltime_set_timezone (new_date, priv->date->zone);
  return new_date;
}

/**
 * gcal_week_view_get_final_date:
 *
 * Since: 0.1
 * Return value: the last day of the week
 * Returns: (transfer full): Release with g_free()
 **/
static icaltimetype*
gcal_week_view_get_final_date (GcalView *view)
{
  GcalWeekViewPrivate *priv;
  icaltimetype *new_date;

  g_return_val_if_fail (GCAL_IS_WEEK_VIEW (view), NULL);
  priv = gcal_week_view_get_instance_private (GCAL_WEEK_VIEW(view));
  new_date = g_new0 (icaltimetype, 1);
  *new_date = icaltime_from_day_of_year (
                  icaltime_start_doy_week (*(priv->date), priv->first_weekday + 1) + 6,
                  priv->date->year);
  new_date->is_date = 0;
  new_date->hour = 23;
  new_date->minute = 59;
  new_date->second = 0;
  *new_date = icaltime_set_timezone (new_date, priv->date->zone);
  return new_date;
}

static GtkWidget*
gcal_week_view_get_by_uuid (GcalSubscriberView *subscriber_view,
                            const gchar        *uuid)
{
  GcalWeekViewPrivate *priv;
  gint i;
  GList *l;

  g_return_val_if_fail (GCAL_IS_WEEK_VIEW (subscriber_view), NULL);
  priv = gcal_week_view_get_instance_private (GCAL_WEEK_VIEW(subscriber_view));

  for (i = 0; i < 7; i++)
    {
      for (l = priv->days[i]; l != NULL; l = l->next)
        {
          GcalWeekViewChild *child;
          GcalEvent *event;
          const gchar* widget_uuid;

          child = (GcalWeekViewChild*) l->data;
          event = gcal_event_widget_get_event (GCAL_EVENT_WIDGET (child->widget));
          widget_uuid = gcal_event_get_uid (event);

          if (g_strcmp0 (uuid, widget_uuid) == 0)
            return child->widget;
        }
    }
  return NULL;
}

/* Public API */
/**
 * gcal_week_view_new:
 *
 * Create a week-view widget
 *
 * Returns: (transfer full):
 **/
GtkWidget*
gcal_week_view_new (void)
{
  return g_object_new (GCAL_TYPE_WEEK_VIEW, NULL);
}

void
gcal_week_view_set_manager (GcalWeekView *week_view,
                            GcalManager  *manager)
{
  GcalWeekViewPrivate *priv = gcal_week_view_get_instance_private (week_view);

  priv->manager = manager;
}

/**
 * gcal_week_view_set_first_weekday:
 * @view:
 * @day_nr:
 *
 * Set the first day of the week according to the locale, being
 * 0 for Sunday, 1 for Monday and so on.
 **/
void
gcal_week_view_set_first_weekday (GcalWeekView *view,
                                  gint          day_nr)
{
  GcalWeekViewPrivate *priv;

  priv = gcal_week_view_get_instance_private (view);
  priv->first_weekday = day_nr;
}

/**
 * gcal_week_view_set_use_24h_format:
 * @view:
 * @use_24h:
 *
 * Whether the view will show time using 24h or 12h format
 **/
void
gcal_week_view_set_use_24h_format (GcalWeekView *view,
                                   gboolean      use_24h)
{
  GcalWeekViewPrivate *priv;

  priv = gcal_week_view_get_instance_private (view);
  priv->use_24h_format = use_24h;
}
