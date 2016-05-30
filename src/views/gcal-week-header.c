/* gcal-week-header.c
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


#include "gcal-week-header.h"
#include "gcal-week-view.h"
#include "gcal-utils.h"
#include "gcal-view.h"
#include "gcal-event-widget.h"

#include <glib/gi18n.h>
#include <string.h>
#include <math.h>

#define ALL_DAY_CELLS_HEIGHT 40

struct _GcalWeekHeader
{
  GtkScrolledWindow parent;

  GtkWidget        *grid;

  GcalManager      *manager;

  gint              first_weekday;

  gboolean          use_24h_format;

  icaltimetype     *current_date;
};

static void           gcal_week_header_finalize             (GObject *object);

static void           gcal_week_header_get_property         (GObject    *object,
                                                             guint       prop_id,
                                                             GValue     *value,
                                                             GParamSpec *psec);

static void           gcal_week_header_set_property         (GObject      *object,
                                                             guint         prop_id,
                                                             const GValue *value,
                                                             GParamSpec   *pspec);

static gboolean       gcal_week_header_draw                 (GcalWeekHeader *self,
                                                             cairo_t        *cr,
                                                             GtkWidget      *widget);

G_DEFINE_TYPE (GcalWeekHeader, gcal_week_header, GTK_TYPE_SCROLLED_WINDOW);

static void
gcal_week_header_finalize (GObject *object)
{
  GcalWeekHeader *self = GCAL_WEEK_HEADER (object);

  g_clear_pointer (&self->current_date, g_free);
}

static void
gcal_week_header_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GcalWeekHeader *self = GCAL_WEEK_HEADER (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_week_header_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GcalWeekHeader *self = GCAL_WEEK_HEADER (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static gboolean
gcal_week_header_draw (GcalWeekHeader *self,
                       cairo_t        *cr,
                       GtkWidget      *widget)
{
  GtkStyleContext *context;
  GtkStateFlags state;
  GdkRGBA color;
  GtkAllocation *alloc;
  GtkBorder *padding;

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

  cairo_save(cr);
  start_grid_y = gcal_week_view_get_start_grid_y (widget);

  /* Fonts and colour selection */
  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);

  gtk_style_context_get_padding (context, state, padding);
  gtk_widget_get_allocation (widget, alloc);

  pattern = cairo_pattern_create_linear (0, start_grid_y - 18,
                                         0, start_grid_y + 6);

  cairo_pattern_add_color_stop_rgba (pattern, 0.0, 0, 0, 0, 0.6);
  cairo_pattern_add_color_stop_rgba (pattern, 1.0, 0, 0, 0, 0.0);

  cairo_set_source (cr, pattern);
  cairo_pattern_destroy (pattern);

  cairo_rectangle (cr, 0, start_grid_y, alloc->width, 6);
  cairo_fill (cr);

  gtk_style_context_get_color (context, state, &color);
  gdk_cairo_set_source_rgba (cr, &color);

  layout = pango_cairo_create_layout (cr);
  gtk_style_context_get (context, state, "font", &bold_font, NULL);
  pango_font_description_set_weight (bold_font, PANGO_WEIGHT_SEMIBOLD);
  pango_layout_set_font_description (layout, bold_font);

  start_of_week = gcal_week_view_get_initial_date (GCAL_VIEW (widget));
  current_cell = icaltime_day_of_week (*(self->current_date)) - 1;
  current_cell = (7 + current_cell - self->first_weekday) % 7;

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

      /* Draw the week days with dates */
      weekday_abv = gcal_get_weekday ((i + self->first_weekday) % 7);
      weekday_header = g_strdup_printf ("%s %d", weekday_abv, n_day);

      pango_layout_set_text (layout, weekday_header, -1);
      cairo_move_to (cr,
                     padding->left + cell_width * i + sidebar_width,
                     0.0);
      pango_cairo_show_layout (cr, layout);

      /* Draws the lines after each day of the week */
      cairo_save (cr);
      cairo_move_to (cr,
                     cell_width * i + sidebar_width + 0.3,
                     font_height + padding->bottom);
      cairo_rel_line_to (cr, 0.0, ALL_DAY_CELLS_HEIGHT);
      cairo_stroke (cr);
      cairo_restore (cr);

      g_free (weekday_header);
    }

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

  return FALSE;
}

static void
gcal_week_header_class_init (GcalWeekHeaderClass *kclass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (kclass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (kclass);

  object_class->finalize = gcal_week_header_finalize;
  object_class->get_property = gcal_week_header_get_property;
  object_class->set_property = gcal_week_header_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/week-header.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalWeekHeader, grid);

  gtk_widget_class_bind_template_callback (widget_class, gcal_week_header_draw);

  gtk_widget_class_set_css_name (widget_class, "calendar-view");
}

static void
gcal_week_header_init (GcalWeekHeader *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

/* Public API */
void
gcal_week_header_set_manager (GcalWeekHeader *self,
                              GcalManager    *manager)
{
  g_return_if_fail (GCAL_IS_WEEK_HEADER (self));

  self->manager = manager;
}

void
gcal_week_header_set_first_weekday (GcalWeekHeader *self,
                                    gint            nr_day)
{
  g_return_if_fail (GCAL_IS_WEEK_HEADER (self));

  self->first_weekday = nr_day;
}

void
gcal_week_header_set_use_24h_format (GcalWeekHeader *self,
                                     gboolean        use_24h_format)
{
  g_return_if_fail (GCAL_IS_WEEK_HEADER (self));

  self->use_24h_format = use_24h_format;
}

void
gcal_week_header_set_current_date (GcalWeekHeader *self,
                                   icaltimetype   *current_date)
{
  g_return_if_fail (GCAL_IS_WEEK_HEADER (self));

  self->current_date = current_date;

  gtk_widget_queue_draw (GTK_WIDGET (self));
}