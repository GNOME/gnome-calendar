/*
 * gcal-month-view.h
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

#ifndef __GCAL_MONTH_VIEW_H__
#define __GCAL_MONTH_VIEW_H__

#include "gcal-manager.h"
#include "gcal-subscriber-view.h"

G_BEGIN_DECLS

#define GCAL_TYPE_MONTH_VIEW  (gcal_month_view_get_type ())

G_DECLARE_FINAL_TYPE (GcalMonthView, gcal_month_view, GCAL, MONTH_VIEW, GcalSubscriberView)


void           gcal_month_view_set_current_date   (GcalMonthView *month_view,
                                                   icaltimetype  *current_date);
void           gcal_month_view_set_first_weekday  (GcalMonthView *view,
                                                   gint           day_nr);
void           gcal_month_view_set_use_24h_format (GcalMonthView *view,
                                                   gboolean       use_24h);

void           gcal_month_view_set_manager        (GcalMonthView *view,
                                                   GcalManager   *manager);

G_END_DECLS

#endif /* __GCAL_MONTH_VIEW_H__ */
