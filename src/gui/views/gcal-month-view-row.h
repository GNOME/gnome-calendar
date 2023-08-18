/*
 * gcal-month-view-row.h
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

#pragma once

#include <gtk/gtk.h>

#include "gcal-context.h"
#include "gcal-event.h"
#include "gcal-range.h"

G_BEGIN_DECLS

#define GCAL_TYPE_MONTH_VIEW_ROW (gcal_month_view_row_get_type())
G_DECLARE_FINAL_TYPE (GcalMonthViewRow, gcal_month_view_row, GCAL, MONTH_VIEW_ROW, GtkWidget)

GtkWidget *          gcal_month_view_row_new                     (void);

GcalContext *        gcal_month_view_row_get_context             (GcalMonthViewRow   *self);

void                 gcal_month_view_row_set_context             (GcalMonthViewRow   *self,
                                                                  GcalContext        *context);

GcalRange *          gcal_month_view_row_get_range               (GcalMonthViewRow   *self);

void                 gcal_month_view_row_set_range               (GcalMonthViewRow   *self,
                                                                  GcalRange          *range);

void                 gcal_month_view_row_add_event               (GcalMonthViewRow   *self,
                                                                  GcalEvent          *event);

void                 gcal_month_view_row_remove_event            (GcalMonthViewRow   *self,
                                                                  GcalEvent          *event);

void                 gcal_month_view_row_update_style_for_date   (GcalMonthViewRow   *self,
                                                                  GDateTime          *date);

GList*               gcal_month_view_row_get_children_by_uuid    (GcalMonthViewRow      *self,
                                                                  GcalRecurrenceModType  mod,
                                                                  const gchar           *uuid);

GtkWidget*           gcal_month_view_row_get_cell_at_x           (GcalMonthViewRow   *self,
                                                                  gdouble             x);

void                 gcal_month_view_row_update_selection       (GcalMonthViewRow    *self,
                                                                 GcalRange           *selection_range);
G_END_DECLS
