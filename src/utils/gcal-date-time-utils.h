/* gcal-date-time-utils.h
 *
 * Copyright 2019 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#include <glib.h>
#include <libecal/libecal.h>

G_BEGIN_DECLS

/**
 * gcal_clear_date_time:
 * @dt: location of a #GDateTime pointer
 *
 * Unreferences @dt if not %NULL, and set it to %NULL.
 */
#define gcal_clear_date_time(dt) g_clear_pointer (dt, g_date_time_unref)

gboolean             gcal_set_date_time                          (GDateTime         **dest,
                                                                  GDateTime          *src);

guint8               gcal_date_time_get_days_in_month            (GDateTime          *date);

GDateTime*           gcal_date_time_get_start_of_week            (GDateTime          *date);

GDateTime*           gcal_date_time_get_end_of_week              (GDateTime          *date);

gint                 gcal_date_time_compare_date                 (GDateTime          *dt1,
                                                                  GDateTime          *dt2);

ICalTime*            gcal_date_time_to_icaltime                  (GDateTime          *dt);

gboolean             gcal_date_time_is_date                      (GDateTime          *dt);

GDateTime*           gcal_date_time_from_icaltime                (const ICalTime     *date);

ICalTimezone*        gcal_timezone_to_icaltimezone               (GTimeZone          *tz);


GDateTime*           gcal_date_time_add_floating_minutes         (GDateTime          *initial,
                                                                  gint                minutes);

G_END_DECLS
