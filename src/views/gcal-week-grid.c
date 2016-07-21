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
  1.0,
  1.0
};

struct _GcalWeekGrid
{
  GtkContainer  parent;

  GtkWidget    *hours_sidebar;

  GcalManager  *manager;

  gint          first_weekday;

  gboolean      use_24h_format;

  icaltimetype *active_date;
  icaltimetype *current_date;
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
  GcalWeekGrid *self = GCAL_WEEK_GRID (object);

  switch (prop_id)
    {
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
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
   }
}

static gboolean
gcal_week_grid_draw (GtkWidget *widget,
                     cairo_t   *cr)
{
  GcalWeekGrid *self;
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

  self = GCAL_WEEK_GRID (widget);

  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);

  gtk_style_context_get_color (context, state, &color);
  gtk_style_context_get_padding (context, state, &padding);
  gtk_style_context_get (context, state, "font", &font_desc, NULL);

  layout = pango_cairo_create_layout (cr);
  pango_layout_set_font_description (layout, font_desc);
  gdk_cairo_set_source_rgba (cr, &color);

  sidebar_width = gcal_week_view_get_sidebar_width (widget);
  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);

  current_cell = icaltime_day_of_week (*(self->current_date)) - 1;
  current_cell = (7 + current_cell - self->first_weekday) % 7;

  gtk_style_context_save (context);
  gtk_style_context_add_class (context, "current");
  gtk_render_background (context, cr,
                         ((width - sidebar_width)/ 7.0) * current_cell + sidebar_width,
                         0,
                         ((width  - sidebar_width)/ 7.0),
                         height);
  gtk_style_context_remove_class (context, "current");
  gtk_style_context_restore (context);

  for (i = 0; i < 7; i++)
    {
      cairo_move_to (cr,
                     sidebar_width + ((width - sidebar_width) / 7) * i + 0.4,
                     0);
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
      cairo_move_to (cr, sidebar_width, (height / 24) * i + (height / 48) + 0.4);
      cairo_rel_line_to (cr, width - sidebar_width, 0);
    }

  cairo_stroke (cr);

  pango_font_description_free (font_desc);
  g_object_unref (layout);

  return FALSE;
}

static void
gcal_week_grid_class_init (GcalWeekGridClass *klass)
{
  GtkContainerClass *contianer_class = GTK_CONTAINER_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  contianer_class->add = gcal_week_grid_add;
  contianer_class->remove = gcal_week_grid_remove;
  contianer_class->forall = gcal_week_grid_forall;

  object_class->finalize = gcal_week_grid_finalize;
  object_class->get_property = gcal_week_grid_get_property;
  object_class->set_property = gcal_week_grid_set_property;

  widget_class->draw = gcal_week_grid_draw;

  gtk_widget_class_set_css_name (widget_class, "calendar-view");
}

static void
gcal_week_grid_init (GcalWeekGrid *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
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