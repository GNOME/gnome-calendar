/*
 * gcal-days-grid.h
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

#ifndef __GCAL_DAYS_GRID_H__
#define __GCAL_DAYS_GRID_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GCAL_TYPE_DAYS_GRID                       (gcal_days_grid_get_type ())
#define GCAL_DAYS_GRID(obj)                       (G_TYPE_CHECK_INSTANCE_CAST((obj), GCAL_TYPE_DAYS_GRID, GcalDaysGrid))
#define GCAL_DAYS_GRID_CLASS(klass)               (G_TYPE_CHECK_CLASS_CAST((klass), GCAL_TYPE_DAYS_GRID, GcalDaysGridClass))
#define GCAL_IS_DAYS_GRID(obj)                    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GCAL_TYPE_DAYS_GRID))
#define GCAL_IS_DAYS_GRID_CLASS(klass)            (G_TYPE_CHECK_CLASS_TYPE((klass), GCAL_TYPE_DAYS_GRID))
#define GCAL_DAYS_GRID_GET_CLASS(obj)             (G_TYPE_INSTANCE_GET_CLASS((obj), GCAL_TYPE_DAYS_GRID, GcalDaysGridClass))

typedef struct _GcalDaysGrid                       GcalDaysGrid;
typedef struct _GcalDaysGridClass                  GcalDaysGridClass;

struct _GcalDaysGrid
{
  GtkContainer  parent;
};

struct _GcalDaysGridClass
{
  GtkContainerClass parent_class;
};

GType          gcal_days_grid_get_type                  (void);

GtkWidget*     gcal_days_grid_new                       (guint           columns);

void           gcal_days_grid_set_preferred_cell_height (GcalDaysGrid   *days_grid,
							 guint           cell_height);

guint          gcal_days_grid_get_scale_width           (GcalDaysGrid   *days_grid);

void           gcal_days_grid_place                     (GcalDaysGrid   *days_grid,
							 GtkWidget      *widget,
							 guint           column_idx,
							 guint           start_cell,
							 guint           end_cell);

GtkWidget*     gcal_days_grid_get_by_uuid               (GcalDaysGrid   *days_grid,
							 const gchar    *uuid);

G_END_DECLS

#endif /* __GCAL_DAYS_GRID_H__ */
