/* gcal-week-grid.h
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

#ifndef GCAL_WEEK_GRID_H
#define GCAL_WEEK_GRID_H

#include "gcal-context.h"
#include "gcal-event-widget.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GCAL_TYPE_WEEK_GRID (gcal_week_grid_get_type())

G_DECLARE_FINAL_TYPE (GcalWeekGrid, gcal_week_grid, GCAL, WEEK_GRID, GtkWidget)

void                 gcal_week_grid_set_context                  (GcalWeekGrid       *week_grid,
                                                                  GcalContext        *context);

void                 gcal_week_grid_set_first_weekday            (GcalWeekGrid       *week_grid,
                                                                  gint                nr_day);

void                 gcal_week_grid_add_event                    (GcalWeekGrid       *self,
                                                                  GcalEvent          *event);

void                 gcal_week_grid_remove_event                 (GcalWeekGrid       *self,
                                                                  const gchar        *uid);

GList*               gcal_week_grid_get_children_by_uuid         (GcalWeekGrid          *self,
                                                                  GcalRecurrenceModType  mod,
                                                                  const gchar           *uid);

void                 gcal_week_grid_clear_marks                  (GcalWeekGrid       *self);

void                 gcal_week_grid_set_date                     (GcalWeekGrid       *self,
                                                                  GDateTime          *date);

G_END_DECLS

#endif /* GCAL_WEEK_GRID_H */
