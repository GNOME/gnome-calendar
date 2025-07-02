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

#include "gcal-date-time-utils.h"
#include "gcal-manager.h"

#include <gtk/gtk.h>
#include <libecal/libecal.h>
#include <libgweather/gweather.h>

#define ALIGNED(x)      (round (x) + 0.5)
#define MINUTES_PER_DAY 1440
#define MAX_MINUTES     (7 * MINUTES_PER_DAY)

#define gcal_clear_timeout(pp) { if (pp && *pp) { g_source_remove (*pp); *pp = 0; } }
#define gcal_clear_signal_handler(pp,instance) { if (pp && *pp > 0) { g_signal_handler_disconnect (instance, *pp); *pp = 0; } }

#if !EDS_CHECK_VERSION (3, 31, 90)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ESource, g_object_unref)
#endif

G_DEFINE_AUTOPTR_CLEANUP_FUNC (ECalComponent, g_object_unref)

#if !GWEATHER_CHECK_VERSION(3, 39, 0)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (GWeatherLocation, gweather_location_unref)
#endif

typedef void (*GcalAskRecurrenceCallback) (GcalEvent             *event,
                                           GcalRecurrenceModType  modtype,
                                           gpointer               user_data);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (ICalTime, g_object_unref)

gchar*               gcal_get_weekday                            (gint                i);

gchar*               gcal_get_month_name                         (gint                i);

GdkPaintable*        gcal_get_paintable_from_color               (const GdkRGBA      *color,
                                                                  gint                size);

GdkPaintable*        get_circle_paintable_from_color             (const GdkRGBA      *color,
                                                                  gint                size);

void                 get_color_name_from_source                  (ESource            *source,
                                                                  GdkRGBA            *out_color);

gchar*               get_desc_from_component                     (ECalComponent      *component,
                                                                  const gchar        *joint_char);

gchar*               get_uuid_from_component                     (ESource            *source,
                                                                  ECalComponent      *component);

gint                 get_first_weekday                           (void);

ECalComponent*       build_component_from_details                (const gchar        *summary,
                                                                  GDateTime          *initial_date,
                                                                  GDateTime          *final_date);

gint                 icaltime_compare_date                       (const ICalTime *date1,
                                                                  const ICalTime *date2);

gint                 icaltime_compare_with_current               (const ICalTime *date1,
                                                                  const ICalTime *date2,
                                                                  time_t         *current_time_t);

/* code brought from evolution */
gsize                e_strftime_fix_am_pm                        (gchar              *str,
                                                                  gsize               max,
                                                                  const gchar        *fmt,
                                                                  const struct tm    *tm);

gsize                e_utf8_strftime_fix_am_pm                   (gchar              *str,
                                                                  gsize               max,
                                                                  const gchar        *fmt,
                                                                  const struct tm    *tm);

gchar*               format_utc_offset                           (gint64              offset);

gint                 get_alarm_trigger_minutes                   (GcalEvent          *event,
                                                                  ECalComponentAlarm *alarm);

gboolean             should_change_date_for_scroll               (gdouble            *scroll_value,
                                                                  GdkEvent           *scroll_event);

gboolean             is_source_enabled                           (ESource            *source);

gboolean             is_workday                                  (guint	              day);

GList*               filter_children_by_uid_and_modtype          (GtkWidget             *widget,
                                                                  GcalRecurrenceModType  mod,
                                                                  const gchar           *uid);

void                 gcal_utils_launch_gnome_settings            (GDBusConnection *connection,
                                                                  const gchar     *panel_id,
                                                                  const gchar     *action);

gchar*               gcal_utils_format_filename_for_display      (const gchar         *filename);

void                 gcal_utils_extract_google_section           (const gchar        *description,
                                                                  gchar             **out_description,
                                                                  gchar             **out_meeting_url);

void                 gcal_utils_ask_recurrence_modification_type (GtkWidget                 *parent,
                                                                  GcalEvent                 *event,
                                                                  gboolean                   show_mod_all,
                                                                  GcalAskRecurrenceCallback  callback,
                                                                  gpointer                   user_data);

const gchar *        gcal_util_translate_time_string             (const gchar          *str);

GTimeZone *          gcal_util_get_app_timezone_or_local         ();

gboolean             gcal_is_valid_event_name                    (const gchar          *event_name);

const gchar*         gcal_get_service_name_from_url              (const gchar        *url);

SoupSession *        gcal_create_soup_session                    (void);

GListModel *         gcal_create_writable_calendars_model        (GcalManager        *manager);

#endif /* __GCAL_UTILS_H__ */
