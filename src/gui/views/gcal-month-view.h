/* gcal-month-view.h
 *
 * Copyright (C) 2012 - Erick Pérez Castellanos
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
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
#include "gcal-weather-service.h"

G_BEGIN_DECLS

#define GCAL_TYPE_MONTH_VIEW  (gcal_month_view_get_type ())

G_DECLARE_FINAL_TYPE (GcalMonthView, gcal_month_view, GCAL, MONTH_VIEW, GtkWidget)

G_END_DECLS

#endif /* __GCAL_MONTH_VIEW_H__ */
