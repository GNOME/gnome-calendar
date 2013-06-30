/*
 * gcal-all-day-grid.h
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

#ifndef __GCAL_ALL_DAY_GRID_H__
#define __GCAL_ALL_DAY_GRID_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GCAL_TYPE_ALL_DAY_GRID                       (gcal_all_day_grid_get_type ())
#define GCAL_ALL_DAY_GRID(obj)                       (G_TYPE_CHECK_INSTANCE_CAST((obj), GCAL_TYPE_ALL_DAY_GRID, GcalAllDayGrid))
#define GCAL_ALL_DAY_GRID_CLASS(klass)               (G_TYPE_CHECK_CLASS_CAST((klass), GCAL_TYPE_ALL_DAY_GRID, GcalAllDayGridClass))
#define GCAL_IS_ALL_DAY_GRID(obj)                    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GCAL_TYPE_ALL_DAY_GRID))
#define GCAL_IS_ALL_DAY_GRID_CLASS(klass)            (G_TYPE_CHECK_CLASS_TYPE((klass), GCAL_TYPE_ALL_DAY_GRID))
#define GCAL_ALL_DAY_GRID_GET_CLASS(obj)             (G_TYPE_INSTANCE_GET_CLASS((obj), GCAL_TYPE_ALL_DAY_GRID, GcalAllDayGridClass))

typedef struct _GcalAllDayGrid                       GcalAllDayGrid;
typedef struct _GcalAllDayGridClass                  GcalAllDayGridClass;

struct _GcalAllDayGrid
{
  GtkContainer  parent;
};

struct _GcalAllDayGridClass
{
  GtkContainerClass parent_class;
};

GType          gcal_all_day_grid_get_type            (void);

GtkWidget*     gcal_all_day_grid_new                 (guint           columns);

void           gcal_all_day_grid_set_column_headers  (GcalAllDayGrid *all_day,
						      ...);

void           gcal_all_day_grid_place               (GcalAllDayGrid *all_day,
						      GtkWidget      *widget,
						      guint           column_idx,
						      guint           column_span);

GtkWidget*     gcal_all_day_grid_get_by_uuid         (GcalAllDayGrid *all_day,
                                                      const gchar    *uuid);
G_END_DECLS

#endif /* __GCAL_ALL_DAY_GRID_H__ */
