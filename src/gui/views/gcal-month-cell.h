/* gcal-month-cell.h
 *
 * Copyright (C) 2017 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#ifndef GCAL_MONTH_CELL_H
#define GCAL_MONTH_CELL_H

#include "gcal-context.h"

#include <adwaita.h>

G_BEGIN_DECLS

#define GCAL_TYPE_MONTH_CELL (gcal_month_cell_get_type())

G_DECLARE_FINAL_TYPE (GcalMonthCell, gcal_month_cell, GCAL, MONTH_CELL, AdwBin)

GtkWidget*           gcal_month_cell_new                         (void);

GDateTime*           gcal_month_cell_get_date                    (GcalMonthCell      *self);

void                 gcal_month_cell_set_date                    (GcalMonthCell      *self,
                                                                  GDateTime          *date);

gboolean             gcal_month_cell_get_different_month         (GcalMonthCell      *self);

void                 gcal_month_cell_set_different_month         (GcalMonthCell      *self,
                                                                  gboolean            out);

GcalContext*         gcal_month_cell_get_context                 (GcalMonthCell      *self);

void                 gcal_month_cell_set_context                 (GcalMonthCell      *self,
                                                                  GcalContext        *context);

guint                gcal_month_cell_get_overflow                (GcalMonthCell      *self);

void                 gcal_month_cell_set_overflow                (GcalMonthCell      *self,
                                                                  guint               n_overflow);

gint                 gcal_month_cell_get_content_space           (GcalMonthCell      *self);

gint                 gcal_month_cell_get_header_height           (GcalMonthCell      *self);

gint                 gcal_month_cell_get_overflow_height         (GcalMonthCell      *self);

gboolean             gcal_month_cell_get_selected                (GcalMonthCell      *self);

void                 gcal_month_cell_set_selected                (GcalMonthCell      *self,
                                                                  gboolean            selected);

G_END_DECLS

#endif /* GCAL_MONTH_VIEW_CELL_H */

