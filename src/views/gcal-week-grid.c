/* gcal-week-grid.c
 *
 * Copyright (C) 2016 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *                    Vamsi Krishna Gollapudi <pandu.sonu@yahoo.com>
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
#include "gcal-range-tree.h"

#include <glib/gi18n.h>
#include <string.h>
#include <math.h>

#define MINUTES_PER_DAY 1440
#define MAX_MINUTES     (7 * MINUTES_PER_DAY)

static const double dashed [] =
{
  5.0,
  6.0
};

typedef struct
{
  GtkWidget          *widget;
  guint16             start;
  guint16             end;
} ChildData;

struct _GcalWeekGrid
{
  GtkContainer        parent;

  GtkWidget          *hours_sidebar;

  GdkWindow          *event_window;

  gint                first_weekday;
  gboolean            use_24h_format : 1;

  icaltimetype       *active_date;
  icaltimetype       *current_date;

  GcalRangeTree      *events;

  gboolean            children_changed;

  GcalManager        *manager;
};

G_DEFINE_TYPE (GcalWeekGrid, gcal_week_grid, GTK_TYPE_CONTAINER);

enum
{
  PROP_0,
  PROP_ACTIVE_DATE,
  LAST_PROP
};

static GParamSpec* properties[LAST_PROP] = { NULL, };

/* ChildData methods */
static ChildData*
child_data_new (GtkWidget *widget,
                guint16    start,
                guint16    end)
{
  ChildData *data;

  data = g_new (ChildData, 1);
  data->widget = widget;
  data->start = start;
  data->end = end;

  return data;
}

/* Auxiliary methods */
static GDateTime*
get_start_of_week (icaltimetype *date)
{
  icaltimetype *new_date;
  GDateTime *dt;

  new_date = g_new0 (icaltimetype, 1);
  *new_date = icaltime_from_day_of_year (icaltime_start_doy_week (*date, get_first_weekday () + 1),
                                         date->year);
  new_date->is_date = 0;
  new_date->hour = 0;
  new_date->minute = 0;
  new_date->second = 0;

  dt = g_date_time_new_local (new_date->year,
                              new_date->month,
                              new_date->day,
                              0, 0, 0);

  g_clear_pointer (&new_date, g_free);

  return dt;
}

static void
get_event_range (GcalWeekGrid *self,
                 GcalEvent    *event,
                 guint16      *start,
                 guint16      *end)
{
  GDateTime *week_start;
  GTimeSpan diff;

  if (!self->active_date)
    return;

  week_start = get_start_of_week (self->active_date);

  if (start)
    {
      GDateTime *event_start;

      event_start = g_date_time_to_local (gcal_event_get_date_start (event));

      diff = g_date_time_difference (event_start, week_start);
      *start = CLAMP (diff / G_TIME_SPAN_MINUTE, 0, MAX_MINUTES);

      g_clear_pointer (&event_start, g_date_time_unref);
    }

  if (end)
    {

      GDateTime *event_end;

      event_end = g_date_time_to_local (gcal_event_get_date_end (event));

      diff = g_date_time_difference (event_end, week_start);
      *end = CLAMP (diff / G_TIME_SPAN_MINUTE, 0, MAX_MINUTES);

      g_clear_pointer (&event_end, g_date_time_unref);
    }

  g_clear_pointer (&week_start, g_date_time_unref);
}

static void
gcal_week_grid_finalize (GObject *object)
{
  GcalWeekGrid *self = GCAL_WEEK_GRID (object);

  g_clear_pointer (&self->events, gcal_range_tree_unref);
  g_clear_pointer (&self->active_date, g_free);
  g_clear_pointer (&self->current_date, g_free);

  G_OBJECT_CLASS (gcal_week_grid_parent_class)->finalize (object);
}

static void
gcal_week_grid_add (GtkContainer *container,
                    GtkWidget    *widget)
{
  if (!gtk_widget_get_parent (widget))
    gtk_widget_set_parent (widget, GTK_WIDGET (container));
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
  GcalWeekGrid *self;
  GPtrArray *widgets_data;
  guint i;

  self = GCAL_WEEK_GRID (container);
  widgets_data = gcal_range_tree_get_data_at_range (self->events, 0, MAX_MINUTES);

  for (i = 0; widgets_data && i < widgets_data->len; i++)
    {
      ChildData *data;

      data = g_ptr_array_index (widgets_data, i);

      callback (data->widget, callback_data);
    }

  g_clear_pointer (&widgets_data, g_ptr_array_unref);
}

static void
gcal_week_grid_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GcalWeekGrid *self = GCAL_WEEK_GRID (object);

  switch (prop_id)
    {
    case PROP_ACTIVE_DATE:
      g_value_set_boxed (value, self->active_date);
      return;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_week_grid_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GcalWeekGrid *self = GCAL_WEEK_GRID (object);

  switch (prop_id)
    {
    case PROP_ACTIVE_DATE:
      g_clear_pointer (&self->active_date, g_free);
      self->active_date = g_value_dup_boxed (value);

      gtk_widget_queue_resize (GTK_WIDGET (self));
      return;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
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

  /* Vertical lines */
  for (i = 0; i < 7; i++)
    {
      cairo_move_to (cr, ((width) / 7) * i + 0.4, 0);
      cairo_rel_line_to (cr, 0, height);
    }

  /* Horizontal lines */
  for (i = 1; i < 24; i++)
    {
      cairo_move_to (cr, 0, (height / 24) * i + 0.4);
      cairo_rel_line_to (cr, width, 0);
    }

  cairo_stroke (cr);

  /* Dashed lines between the vertical lines */
  cairo_set_dash (cr, dashed, 2, 0);

  for (i = 0; i < 24; i++)
    {
      cairo_move_to (cr, 0, (height / 24) * i + (height / 48) + 0.4);
      cairo_rel_line_to (cr, width, 0);
    }

  cairo_stroke (cr);

  pango_font_description_free (font_desc);
  g_object_unref (layout);

  GTK_WIDGET_CLASS (gcal_week_grid_parent_class)->draw (widget, cr);

  return FALSE;
}

static void
gcal_week_grid_size_allocate (GtkWidget     *widget,
                              GtkAllocation *allocation)
{
  GcalWeekGrid *self = GCAL_WEEK_GRID (widget);
  gdouble minutes_height;
  gdouble column_width;
  guint i;

  /* No need to relayout stuff if nothing changed */
  if (allocation->height == gtk_widget_get_allocated_height (widget) &&
      allocation->width == gtk_widget_get_allocated_width (widget) &&
      !self->children_changed)
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


  /* Preliminary calculations */
  minutes_height = (gdouble) allocation->height / MINUTES_PER_DAY;
  column_width = (gdouble) allocation->width / 7.0;

  /*
   * Iterate through weekdays; we don't have to worry about events that
   * jump between days because they're already handled by GcalWeekHeader.
   */
  for (i = 0; i < 7; i++)
    {
      GPtrArray *widgets_data;
      guint16 day_start;
      guint16 day_end;
      guint j;

      day_start = i * MINUTES_PER_DAY;
      day_end = day_start + MINUTES_PER_DAY;
      widgets_data = gcal_range_tree_get_data_at_range (self->events, day_start, day_end);

      for (j = 0; widgets_data && j < widgets_data->len; j++)
        {
          GtkStyleContext *context;
          GtkAllocation child_allocation;
          GtkWidget *event_widget;
          ChildData *data;
          GtkBorder margin;
          guint64 events_at_range;
          gint natural_height;
          gint height;
          gint width;

          data =  g_ptr_array_index (widgets_data, j);
          event_widget = data->widget;
          context = gtk_widget_get_style_context (event_widget);

          events_at_range = gcal_range_tree_count_entries_at_range (self->events,
                                                                    data->start,
                                                                    data->end);

          /* Gtk complains about that */
          gtk_widget_get_preferred_height (event_widget, NULL, &natural_height);

          /* Consider the margins of the child */
          gtk_style_context_get_margin (context,
                                        gtk_style_context_get_state (context),
                                        &margin);

          width = column_width / events_at_range - margin.left - margin.right;
          height = MAX ((data->end - data->start) * minutes_height - margin.top - margin.bottom, natural_height);

          /* Setup the child position and size */
          child_allocation.x = column_width * i + allocation->x + margin.left + 1;
          child_allocation.y = (data->start % MINUTES_PER_DAY) * minutes_height + margin.top;
          child_allocation.width = width;
          child_allocation.height = height;

          gtk_widget_size_allocate (event_widget, &child_allocation);
        }

      g_clear_pointer (&widgets_data, g_ptr_array_unref);
    }

  self->children_changed = FALSE;
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
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  container_class->add = gcal_week_grid_add;
  container_class->remove = gcal_week_grid_remove;
  container_class->forall = gcal_week_grid_forall;

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

  properties[PROP_ACTIVE_DATE] = g_param_spec_boxed ("active-date",
                                                     "Date",
                                                     "The active selected date",
                                                     ICAL_TIME_TYPE,
                                                     G_PARAM_CONSTRUCT | G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);

  gtk_widget_class_set_css_name (widget_class, "weekgrid");
}

static void
gcal_week_grid_init (GcalWeekGrid *self)
{
  gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);

  self->events = gcal_range_tree_new ();
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

  g_clear_pointer (&self->current_date, g_free);
  self->current_date = gcal_dup_icaltime (current_date);

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

void
gcal_week_grid_add_event (GcalWeekGrid *self,
                          GcalEvent    *event)
{
  GtkWidget *widget;
  guint16 start, end;

  g_return_if_fail (GCAL_IS_WEEK_GRID (self));

  widget = gcal_event_widget_new (event);
  start = 0;
  end = 0;

  self->children_changed = TRUE;

  get_event_range (self, event, &start, &end);

  gcal_range_tree_add_range (self->events,
                             start,
                             end,
                             child_data_new (widget, start, end));

  gtk_widget_show (widget);

  gtk_container_add (GTK_CONTAINER (self), widget);
}

void
gcal_week_grid_remove_event (GcalWeekGrid *self,
                             const gchar  *uid)
{
  GPtrArray *widgets;
  guint i;

  g_return_if_fail (GCAL_IS_WEEK_GRID (self));

  widgets = gcal_range_tree_get_data_at_range (self->events, 0, MAX_MINUTES);

  for (i = 0; widgets && i < widgets->len; i++)
    {
      ChildData *data;
      GcalEvent *event;
      guint16 event_start;
      guint16 event_end;

      data = g_ptr_array_index (widgets, i);
      event = gcal_event_widget_get_event (GCAL_EVENT_WIDGET (data->widget));

      if (g_strcmp0 (gcal_event_get_uid (event), uid) != 0)
        continue;

      self->children_changed = TRUE;

      get_event_range (self, event, &event_start, &event_end);

      gcal_range_tree_remove_range (self->events, data->start, data->end, data);
      gtk_widget_destroy (data->widget);
      g_free (data);
    }

  g_clear_pointer (&widgets, g_ptr_array_unref);
}
