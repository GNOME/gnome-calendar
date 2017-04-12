/* -*- mode: c; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gcal-window.h
 * Copyright (C) 2015 Erick Pérez Castellanos <erickpc@gnome.org>
 *
 * gnome-calendar is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * gnome-calendar is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GCAL_UTILS_H__
#define __GCAL_UTILS_H__

#include "gcal-manager.h"
#include <gtk/gtk.h>
#include <libecal/libecal.h>
#include <libical/icaltime.h>

#define ICAL_TIME_TYPE (icaltime_get_type ())

#define ALIGNED(x)      (round (x) + 0.5)
#define MINUTES_PER_DAY 1440
#define MAX_MINUTES     (7 * MINUTES_PER_DAY)

#define gcal_clear_datetime(dt) g_clear_pointer (dt, g_date_time_unref)

/**
 * GcalWindowViewType:
 * @GCAL_WINDOW_VIEW_DAY: Day view (not implemented)
 * @GCAL_WINDOW_VIEW_WEEK: Week view
 * @GCAL_WINDOW_VIEW_MONTH: Month view
 * @GCAL_WINDOW_VIEW_YEAR: Year view
 * @GCAL_WINDOW_VIEW_LIST: List (not implemented)
 * @GCAL_WINDOW_VIEW_SEARCH: Search view (partially implemented)
 *
 * Enum with the available views.
 */
typedef enum
{
  GCAL_WINDOW_VIEW_DAY,
  GCAL_WINDOW_VIEW_WEEK,
  GCAL_WINDOW_VIEW_MONTH,
  GCAL_WINDOW_VIEW_YEAR,
  GCAL_WINDOW_VIEW_LIST,
  GCAL_WINDOW_VIEW_SEARCH,
} GcalWindowViewType;

GType           icaltime_get_type                               (void)            G_GNUC_CONST;

gint            datetime_compare_date                           (GDateTime             *dt1,
                                                                 GDateTime             *dt2);

icaltimetype*   datetime_to_icaltime                            (GDateTime             *dt);

gboolean        datetime_is_date                                (GDateTime             *dt);

GDateTime*      icaltime_to_datetime                            (const icaltimetype    *date);

icaltimetype*   gcal_dup_icaltime                               (const icaltimetype    *date);

gchar*          gcal_get_weekday                                (gint                   i);

gchar*          gcal_get_month_name                             (gint                   i);

cairo_surface_t*gcal_get_surface_from_color                     (GdkRGBA               *color,
                                                                 gint                   size);

cairo_surface_t*get_circle_surface_from_color                   (GdkRGBA               *color,
                                                                 gint                   size);

void            get_color_name_from_source                      (ESource               *source,
                                                                 GdkRGBA               *out_color);

gchar*          get_desc_from_component                         (ECalComponent         *component,
                                                                 const gchar           *joint_char);

gchar*          get_uuid_from_component                         (ESource               *source,
                                                                 ECalComponent         *component);

gint            get_first_weekday                               (void);

ECalComponent*  build_component_from_details                    (const gchar           *summary,
                                                                 GDateTime             *initial_date,
                                                                 GDateTime             *final_date);

gint            icaltime_compare_date                           (const icaltimetype    *date1,
                                                                 const icaltimetype    *date2);

gint            icaltime_compare_with_current                   (const icaltimetype    *date1,
                                                                 const icaltimetype    *date2,
                                                                 time_t                *current_time_t);

GDateTime*      get_start_of_week                               (icaltimetype          *date);

GDateTime*      get_end_of_week                                 (icaltimetype          *date);

gboolean        is_clock_format_24h                             (void);

/* code brought from evolution */
gsize           e_strftime_fix_am_pm                            (gchar                 *str,
                                                                 gsize                  max,
                                                                 const gchar           *fmt,
                                                                 const struct tm       *tm);
gsize           e_utf8_strftime_fix_am_pm                       (gchar                 *str,

                                                                 gsize                  max,
                                                                 const gchar           *fmt,
                                                                 const struct tm       *tm);

void            fix_popover_menu_icons                          (GtkPopover            *popover);

gboolean        uri_get_fields                                  (const gchar           *uri,
                                                                 gchar                **schema,
                                                                 gchar                **host,
                                                                 gchar                **path);

void            get_source_parent_name_color                    (GcalManager           *manager,
                                                                 ESource               *source,
                                                                 gchar                **name,
                                                                 gchar                **color);

gchar*          format_utc_offset                               (gint64                 offset);

gint            get_alarm_trigger_minutes                       (GcalEvent             *event,
                                                                 ECalComponentAlarm    *alarm);

gboolean        should_change_date_for_scroll                   (gdouble               *scroll_value,
                                                                 GdkEventScroll        *scroll_event);

gboolean        is_source_enabled                               (ESource               *source);

#endif // __GCAL_UTILS_H__
