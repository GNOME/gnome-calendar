/*
 * gcal-week-view.h
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

#ifndef __GCAL_WEEK_VIEW_H__
#define __GCAL_WEEK_VIEW_H__

#include "gcal-manager.h"
#include "gcal-view.h"

G_BEGIN_DECLS

#define GCAL_TYPE_WEEK_VIEW (gcal_week_view_get_type ())

G_DECLARE_FINAL_TYPE (GcalWeekView, gcal_week_view, GCAL, WEEK_VIEW, GtkBox)

GtkWidget*     gcal_week_view_new                (void);
void           gcal_week_view_set_manager        (GcalWeekView *view,
                                                  GcalManager  *manager);
void           gcal_week_view_set_first_weekday  (GcalWeekView *view,
                                                  gint          day_nr);
void           gcal_week_view_set_use_24h_format (GcalWeekView *view,
                                                  gboolean      use_24h);
void           gcal_week_view_set_current_date   (GcalWeekView *week_header,
                                                  icaltimetype *current_date);

G_END_DECLS

#endif /* __GCAL_WEEK_VIEW_H__ */
