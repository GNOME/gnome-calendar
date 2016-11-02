/* gcal-week-grid.c
 *
 * Copyright (C) 2016 Vamsi Krishna Gollapudi <pandu.sonu@yahoo.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "gcal-week-grid.h"
#include "gcal-week-view.h"
#include "gcal-utils.h"
#include "gcal-view.h"
#include "gcal-event-widget.h"

#include <glib/gi18n.h>
#include <string.h>
#include <math.h>

static const double dashed [] =
{
  5.0,
  6.0
};

struct _GcalWeekGrid
{
  GtkContainer  parent;

  GtkWidget    *hours_sidebar;

  GdkWindow    *event_window;

  gint          first_weekday;

  gboolean      use_24h_format;

  icaltimetype *active_date;
  icaltimetype *current_date;

  GcalManager  *manager;
};

static void           gcal_week_grid_finalize               (GObject *object);

static void           gcal_week_grid_add                    (GtkContainer *container,
                                                             GtkWidget    *widget);

static void           gcal_week_grid_remove                 (GtkContainer *container,
                                                             GtkWidget    *widget);

static void           gcal_week_grid_forall                 (GtkContainer *container,
                                                             gboolean      include_internals,
                                                             GtkCallback   callback,
                                                             gpointer      callback_data);

static void           gcal_week_grid_get_property           (GObject    *object,
                                                             guint       prop_id,
                                                             GValue     *value,
                                                             GParamSpec *psec);

static void           gcal_week_grid_set_property           (GObject      *object,
                                                             guint         prop_id,
                                                             const GValue *value,
                                                             GParamSpec   *pspec);

static gboolean       gcal_week_grid_draw                   (GtkWidget *widget,
                                                             cairo_t   *cr);

G_DEFINE_TYPE (GcalWeekGrid, gcal_week_grid, GTK_TYPE_CONTAINER);

static void
gcal_week_grid_finalize (GObject *object)
{
  GcalWeekGrid *self = GCAL_WEEK_GRID (object);

  g_clear_pointer (&self->active_date, g_free);
  g_clear_pointer (&self->current_date, g_free);
}

static void
gcal_week_grid_add (GtkContainer *container,
                    GtkWidget    *widget)
{
}

static void
gcal_week_grid_remove (GtkContainer *container,
                       GtkWidget    *widget)
{
}

static void
gcal_week_grid_forall (GtkContainer *container,
                       gboolean      include_internals,
                       GtkCallback   callback,
                       gpointer      callback_data)
{
}

static void
gcal_week_grid_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
}

static void
gcal_week_grid_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
}

static void
gcal_week_grid_realize (GtkWidget *widget)
{
  GcalWeekGrid *self;
  GdkWindow *parent_window;
  GdkWindowAttr attributes;
  gint attributes_mask;
  GtkAllocation allocation;

  self = GCAL_WEEK_GRID (widget);
  parent_window = gtk_widget_get_parent_window (widget);

  gtk_widget_set_realized (widget, TRUE);
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
                            GDK_LEAVE_NOTIFY_MASK |
                            GDK_SCROLL_MASK |
                            GDK_SMOOTH_SCROLL_MASK);
  attributes_mask = GDK_WA_X | GDK_WA_Y;

  self->event_window = gdk_window_new (parent_window,
                                       &attributes,
                                       attributes_mask);
  gtk_widget_register_window (widget, self->event_window);
}

static void
gcal_week_grid_unrealize (GtkWidget *widget)
{
  GcalWeekGrid *self;

  self = GCAL_WEEK_GRID (widget);

  if (self->event_window)
    {
      gtk_widget_unregister_window (widget, self->event_window);
      gdk_window_destroy (self->event_window);
      self->event_window = NULL;
    }

  GTK_WIDGET_CLASS (gcal_week_grid_parent_class)->unrealize (widget);
}

static void
gcal_week_grid_map (GtkWidget *widget)
{
  GcalWeekGrid *self;

  self = GCAL_WEEK_GRID (widget);

  if (self->event_window)
    gdk_window_show (self->event_window);

  GTK_WIDGET_CLASS (gcal_week_grid_parent_class)->map (widget);
}

static void
gcal_week_grid_unmap (GtkWidget *widget)
{
  GcalWeekGrid *self;

  self = GCAL_WEEK_GRID (widget);

  if (self->event_window)
    gdk_window_hide (self->event_window);

  GTK_WIDGET_CLASS (gcal_week_grid_parent_class)->unmap (widget);
}

static gboolean
gcal_week_grid_draw (GtkWidget *widget,
                     cairo_t   *cr)
{
  GtkStyleContext *context;
  GtkStateFlags state;
  GtkBorder padding;
  GdkRGBA color;

  gint i, width, height;

  PangoLayout *layout;
  PangoFontDescription *font_desc;

  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);

  gtk_style_context_add_class (context, "hours");
  gtk_style_context_get_color (context, state, &color);
  gtk_style_context_get_padding (context, state, &padding);
  gtk_style_context_get (context, state, "font", &font_desc, NULL);

  layout = pango_cairo_create_layout (cr);
  pango_layout_set_font_description (layout, font_desc);
  gdk_cairo_set_source_rgba (cr, &color);

  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);

  cairo_set_line_width (cr, 0.65);

  for (i = 0; i < 7; i++)
    {
      cairo_move_to (cr, ((width) / 7) * i + 0.4, 0);
      cairo_rel_line_to (cr, 0, height);
    }

  for (i = 0; i < 24; i++)
    {
      cairo_move_to (cr, 0, (height / 24) * i + 0.4);
      cairo_rel_line_to (cr, width, 0);
    }

  cairo_stroke (cr);

  cairo_set_dash (cr, dashed, 2, 0);

  for (i = 0; i < 24; i++)
    {
      cairo_move_to (cr, 0, (height / 24) * i + (height / 48) + 0.4);
      cairo_rel_line_to (cr, width, 0);
    }

  cairo_stroke (cr);

  pango_font_description_free (font_desc);
  g_object_unref (layout);

  return FALSE;
}

static void
gcal_week_grid_size_allocate (GtkWidget     *widget,
                              GtkAllocation *allocation)
{
  GcalWeekGrid *self = GCAL_WEEK_GRID (widget);

  /* No need to relayout stuff if nothing changed */
  if (allocation->height == gtk_widget_get_allocated_height (widget) &&
      allocation->width == gtk_widget_get_allocated_width (widget))
    {
      return;
    }

  /* Allocate the widget */
  gtk_widget_set_allocation (widget, allocation);

  if (gtk_widget_get_realized (widget))
    {
      gdk_window_move_resize (self->event_window,
                              allocation->x,
                              allocation->y,
                              allocation->width,
                              allocation->height);
    }
}

static void
gcal_week_grid_get_preferred_height (GtkWidget *widget,
                                     gint      *minimum_height,
                                     gint      *natural_height)
{
  GtkStyleContext *context;
  GtkStateFlags state;
  GtkBorder padding;

  PangoLayout *layout;
  PangoFontDescription *font_desc;

  gint hours_12_height, hours_24_height, cell_height, height;

  context = gtk_widget_get_style_context (widget);
  state = gtk_style_context_get_state (context);

  gtk_style_context_save (context);
  gtk_style_context_add_class (context, "hours");

  gtk_style_context_get (context, state,
                         "font", &font_desc,
                         NULL);
  gtk_style_context_get_padding (context, state, &padding);

  layout = pango_layout_new (gtk_widget_get_pango_context (widget));
  pango_layout_set_font_description (layout, font_desc);

  pango_layout_set_text (layout, _("00 AM"), -1);
  pango_layout_get_pixel_size (layout, NULL, &hours_12_height);

  pango_layout_set_text (layout, _("00:00"), -1);
  pango_layout_get_pixel_size (layout, NULL, &hours_24_height);

  cell_height = MAX (hours_12_height, hours_24_height) + padding.top + padding.bottom;
  height = cell_height * 48;

  gtk_style_context_restore (context);

  pango_font_description_free (font_desc);
  g_object_unref (layout);

  /* Report the height */
  if (minimum_height)
    *minimum_height = height;

  if (natural_height)
    *natural_height = height;
}

static void
gcal_week_grid_class_init (GcalWeekGridClass *klass)
{
  GtkContainerClass *contianer_class = GTK_CONTAINER_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  contianer_class->add = gcal_week_grid_add;
  contianer_class->remove = gcal_week_grid_remove;
  contianer_class->forall = gcal_week_grid_forall;

  object_class->finalize = gcal_week_grid_finalize;
  object_class->get_property = gcal_week_grid_get_property;
  object_class->set_property = gcal_week_grid_set_property;

  widget_class->draw = gcal_week_grid_draw;
  widget_class->size_allocate = gcal_week_grid_size_allocate;
  widget_class->realize = gcal_week_grid_realize;
  widget_class->unrealize = gcal_week_grid_unrealize;
  widget_class->map = gcal_week_grid_map;
  widget_class->unmap = gcal_week_grid_unmap;
  widget_class->get_preferred_height = gcal_week_grid_get_preferred_height;

  gtk_widget_class_set_css_name (widget_class, "calendar-view");
}

static void
gcal_week_grid_init (GcalWeekGrid *self)
{
  gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);
}

/* Public API */
void
gcal_week_grid_set_manager (GcalWeekGrid *self,
                            GcalManager  *manager)
{
  g_return_if_fail (GCAL_IS_WEEK_GRID (self));

  self->manager = manager;
}

void
gcal_week_grid_set_first_weekday (GcalWeekGrid *self,
                                  gint          nr_day)
{
  g_return_if_fail (GCAL_IS_WEEK_GRID (self));

  self->first_weekday = nr_day;
}

void
gcal_week_grid_set_use_24h_format (GcalWeekGrid *self,
                                     gboolean    use_24h_format)
{
  g_return_if_fail (GCAL_IS_WEEK_GRID (self));

  self->use_24h_format = use_24h_format;
}

void
gcal_week_grid_set_current_date (GcalWeekGrid *self,
                                 icaltimetype *current_date)
{
  g_return_if_fail (GCAL_IS_WEEK_GRID (self));

  self->current_date = gcal_dup_icaltime (current_date);

  gtk_widget_queue_draw (GTK_WIDGET (self));
}
