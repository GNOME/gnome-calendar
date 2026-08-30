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

#include "gcal-application.h"
#include "gcal-date-time-utils.h"
#include "gcal-manager.h"

#include <gtk/gtk.h>
#include <libecal/libecal.h>
#include <libgweather/gweather.h>

#define GCAL_N_WEEKDAYS 7
#define GCAL_ALIGNED(x)      (round (x) + 0.5)
#define GCAL_MINUTES_PER_DAY 1440
#define GCAL_MAX_MINUTES     (GCAL_N_WEEKDAYS * GCAL_MINUTES_PER_DAY)

#define GCAL_DEFAULT_APPLICATION GCAL_APPLICATION (g_application_get_default ())

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

gchar*               gcal_util_get_weekday                       (gint                i);

gchar*               gcal_util_get_month_name                    (gint                i);

GdkPaintable*        gcal_util_get_circle_paintable_from_color   (const GdkRGBA      *color,
                                                                  gint                size);

gchar*               gcal_util_get_desc_from_component           (ECalComponent      *component,
                                                                  const gchar        *joint_char);

gint                 gcal_util_get_first_weekday_iso             (void);

ECalComponent*       gcal_util_build_component_from_details      (const gchar        *summary,
                                                                  GDateTime          *initial_date,
                                                                  GDateTime          *final_date);

/* code brought from evolution */
gchar*               gcal_util_format_utc_offset                 (gint64              offset);

gint                 gcal_util_get_alarm_trigger_minutes         (GcalEvent          *event,
                                                                  ECalComponentAlarm *alarm);

gboolean             gcal_util_is_workday                        (guint               day);

GList*               gcal_util_filter_children_by_uid_and_modtype (GtkWidget             *widget,
                                                                   GcalRecurrenceModType  mod,
                                                                   const gchar           *uid);

void                 gcal_util_launch_gnome_settings             (GDBusConnection *connection,
                                                                  const gchar     *panel_id,
                                                                  const gchar     *action);

gchar*               gcal_util_format_filename_for_display       (const gchar         *filename);

void                 gcal_util_extract_meeting_url               (const char         *description,
                                                                  char              **out_description,
                                                                  char              **out_meeting_url);

void                 gcal_util_ask_recurrence_modification_type  (GtkWidget                 *parent,
                                                                  GcalEvent                 *event,
                                                                  gboolean                   show_mod_all,
                                                                  GcalAskRecurrenceCallback  callback,
                                                                  gpointer                   user_data);

GTimeZone *          gcal_util_get_app_timezone_or_local         (void);

gboolean             gcal_util_is_valid_event_name               (const gchar          *event_name);

const gchar*         gcal_util_get_service_name_from_url         (const gchar        *url);

SoupSession *        gcal_util_create_soup_session               (void);

GListModel *         gcal_util_create_writable_calendars_model   (GcalManager        *manager);

const gchar *        gcal_util_get_email_from_mailto_uri         (const gchar        *mailto_uri);

guint                gcal_util_create_activate_signal_and_shortcuts (GtkWidgetClass *widget_class,
                                                                     GType           widget_type);
#endif /* __GCAL_UTILS_H__ */
