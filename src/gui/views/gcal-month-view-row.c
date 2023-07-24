/*
 * gcal-month-view-row.c
 *
 * Copyright 2023 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "GcalMonthViewRow"

#include "config.h"
#include "gcal-date-time-utils.h"
#include "gcal-debug.h"
#include "gcal-month-cell.h"
#include "gcal-month-view-row.h"

struct _GcalMonthViewRow
{
  GtkWidget           parent;

  GtkWidget          *day_cells[7];

  GcalRange          *range;
};

G_DEFINE_FINAL_TYPE (GcalMonthViewRow, gcal_month_view_row, GTK_TYPE_WIDGET)

enum
{
  PROP_0,
  N_PROPS,
};

static GParamSpec *properties [N_PROPS];


/*
 * GtkWidget overrides
 */

static void
gcal_month_view_row_measure (GtkWidget      *widget,
                             GtkOrientation  orientation,
                             gint            for_size,
                             gint           *out_minimum,
                             gint           *out_natural,
                             gint           *out_minimum_baseline,
                             gint           *out_natural_baseline)
{
  GcalMonthViewRow *self = GCAL_MONTH_VIEW_ROW (widget);
  gint minimum = 0;
  gint natural = 0;

  for (guint i = 0; i < 7; i++)
    {
      gint child_minimum;
      gint child_natural;

      gtk_widget_measure (self->day_cells[i],
                          orientation,
                          for_size,
                          &child_minimum,
                          &child_natural,
                          NULL,
                          NULL);


      minimum += child_minimum;
      natural += child_natural;
    }

  if (out_minimum)
    *out_minimum = minimum;

  if (out_natural)
    *out_natural = natural;

  if (out_minimum_baseline)
    *out_minimum_baseline = -1;

  if (out_natural_baseline)
    *out_natural_baseline = -1;
}

static void
gcal_month_view_row_size_allocate (GtkWidget *widget,
                                   gint       width,
                                   gint       height,
                                   gint       baseline)
{
  GcalMonthViewRow *self = GCAL_MONTH_VIEW_ROW (widget);
  gboolean is_ltr;
  gdouble cell_width;

  is_ltr = gtk_widget_get_direction (widget) != GTK_TEXT_DIR_RTL;
  cell_width = width / 7.0;

  for (guint i = 0; i < 7; i++)
    {
      GtkAllocation allocation;
      GtkWidget *cell;

      allocation.x = round (cell_width * i);
      allocation.y = 0;
      allocation.width = round (cell_width * (i + 1)) - allocation.x;
      allocation.height = height;

      cell = is_ltr ? self->day_cells[i] : self->day_cells[7 - i + 1];
      gtk_widget_size_allocate (cell, &allocation, baseline);
    }
}

static void
gcal_month_view_row_snapshot (GtkWidget   *widget,
                              GtkSnapshot *snapshot)
{
  GcalMonthViewRow *self = GCAL_MONTH_VIEW_ROW (widget);

  for (guint i = 0; i < 7; i++)
    gtk_widget_snapshot_child (widget, self->day_cells[i], snapshot);
}


/*
 * GObject overrides
 */

static void
gcal_month_view_row_dispose (GObject *object)
{
  GcalMonthViewRow *self = (GcalMonthViewRow *)object;

  for (guint i = 0; i < 7; i++)
    g_clear_pointer (&self->day_cells[i], gtk_widget_unparent);

  G_OBJECT_CLASS (gcal_month_view_row_parent_class)->dispose (object);
}

static void
gcal_month_view_row_finalize (GObject *object)
{
  GcalMonthViewRow *self = (GcalMonthViewRow *)object;

  g_clear_pointer (&self->range, gcal_range_unref);

  G_OBJECT_CLASS (gcal_month_view_row_parent_class)->finalize (object);
}

static void
gcal_month_view_row_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GcalMonthViewRow *self = GCAL_MONTH_VIEW_ROW (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_month_view_row_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GcalMonthViewRow *self = GCAL_MONTH_VIEW_ROW (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_month_view_row_class_init (GcalMonthViewRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gcal_month_view_row_dispose;
  object_class->finalize = gcal_month_view_row_finalize;
  object_class->get_property = gcal_month_view_row_get_property;
  object_class->set_property = gcal_month_view_row_set_property;

  widget_class->measure = gcal_month_view_row_measure;
  widget_class->size_allocate = gcal_month_view_row_size_allocate;
  widget_class->snapshot = gcal_month_view_row_snapshot;

  gtk_widget_class_set_css_name (widget_class, "monthviewrow");
}

static void
gcal_month_view_row_init (GcalMonthViewRow *self)
{
  for (guint i = 0; i < 7; i++)
    {
      self->day_cells[i] = gcal_month_cell_new ();
      gcal_month_cell_set_overflow (GCAL_MONTH_CELL (self->day_cells[i]), 0);
      gtk_widget_set_parent (self->day_cells[i], GTK_WIDGET (self));
    }
}

GtkWidget *
gcal_month_view_row_new (void)
{
  return g_object_new (GCAL_TYPE_MONTH_VIEW_ROW, NULL);
}

GcalRange *
gcal_month_view_row_get_range (GcalMonthViewRow *self)
{
  g_return_val_if_fail (GCAL_IS_MONTH_VIEW_ROW (self), NULL);

  return self->range ? gcal_range_ref (self->range) : NULL;
}

void
gcal_month_view_row_set_range (GcalMonthViewRow *self,
                               GcalRange        *range)
{
  g_autoptr (GDateTime) start = NULL;

  g_return_if_fail (GCAL_IS_MONTH_VIEW_ROW (self));
  g_return_if_fail (range != NULL);

  GCAL_ENTRY;

  if (self->range && gcal_range_compare (self->range, range) == 0)
    GCAL_RETURN ();

  g_clear_pointer (&self->range, gcal_range_unref);
  self->range = gcal_range_ref (range);

  /* TODO: remove all widgets */

  start = gcal_range_get_start (range);
  for (guint i = 0; i < 7; i++)
    {
      g_autoptr (GDateTime) day = g_date_time_add_days (start, i);
      gcal_month_cell_set_date (GCAL_MONTH_CELL (self->day_cells[i]), day);
    }
}
