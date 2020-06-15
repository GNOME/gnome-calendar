/* gcal-event.h
 *
 * Copyright (C) 2016-2020 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#ifndef GCAL_EVENT_H
#define GCAL_EVENT_H

#include "gcal-calendar.h"
#include "gcal-range.h"
#include "gcal-recurrence.h"

#include <glib-object.h>
#include <gtk/gtk.h>
#include <libecal/libecal.h>
#include <libedataserver/libedataserver.h>

G_BEGIN_DECLS

/**
 * GcalEventError:
 * @GCAL_EVENT_ERROR_INVALID_START_DATE: indicated an invalid start date
 *
 * Errors that #GcalEvent can generate.
 */
typedef enum
{
  GCAL_EVENT_ERROR_INVALID_START_DATE
} GcalEventError;

#define GCAL_EVENT_ERROR gcal_event_error_quark ()

#define GCAL_TYPE_EVENT (gcal_event_get_type())

G_DECLARE_FINAL_TYPE (GcalEvent, gcal_event, GCAL, EVENT, GObject)

GQuark               gcal_event_error_quark                      (void);

GcalEvent*           gcal_event_new                              (GcalCalendar       *calendar,
                                                                  ECalComponent      *component,
                                                                  GError            **error);

GcalEvent*           gcal_event_new_from_event                   (GcalEvent          *self);

gboolean             gcal_event_get_all_day                      (GcalEvent          *self);

void                 gcal_event_set_all_day                      (GcalEvent          *self,
                                                                  gboolean            all_day);

GdkRGBA*             gcal_event_get_color                        (GcalEvent          *self);

void                 gcal_event_set_color                        (GcalEvent          *self,
                                                                  GdkRGBA            *color);

ECalComponent*       gcal_event_get_component                    (GcalEvent          *self);

GDateTime*           gcal_event_get_date_end                     (GcalEvent          *self);

void                 gcal_event_set_date_end                     (GcalEvent          *self,
                                                                  GDateTime          *dt);

GDateTime*           gcal_event_get_date_start                   (GcalEvent          *self);

void                 gcal_event_set_date_start                   (GcalEvent          *self,
                                                                  GDateTime          *dt);

GcalRange*           gcal_event_get_range                        (GcalEvent          *self);

const gchar*         gcal_event_get_description                  (GcalEvent          *self);

void                 gcal_event_set_description                  (GcalEvent          *self,
                                                                  const gchar        *description);

gboolean             gcal_event_has_recurrence                   (GcalEvent          *self);

gboolean             gcal_event_has_alarms                       (GcalEvent          *self);

GList*               gcal_event_get_alarms                       (GcalEvent          *self);

void                 gcal_event_remove_all_alarms                (GcalEvent          *self);

void                 gcal_event_add_alarm                        (GcalEvent          *self,
                                                                  ECalComponentAlarm *alarm);

const gchar*         gcal_event_get_location                     (GcalEvent          *self);

void                 gcal_event_set_location                     (GcalEvent          *self,
                                                                  const gchar        *location);

GcalCalendar*        gcal_event_get_calendar                     (GcalEvent          *self);

void                 gcal_event_set_calendar                     (GcalEvent          *self,
                                                                  GcalCalendar       *calendar);

const gchar*         gcal_event_get_summary                      (GcalEvent          *self);

void                 gcal_event_set_summary                      (GcalEvent          *self,
                                                                  const gchar        *summary);

const gchar*         gcal_event_get_uid                          (GcalEvent          *self);

/* Utilities */

gboolean             gcal_event_is_multiday                      (GcalEvent          *self);

gint                 gcal_event_compare                          (GcalEvent          *event1,
                                                                  GcalEvent          *event2);

gint                 gcal_event_compare_with_current             (GcalEvent          *event1,
                                                                  GcalEvent          *event2,
                                                                  time_t              current_time);

void                 gcal_event_set_recurrence                   (GcalEvent          *event,
                                                                  GcalRecurrence     *recur);

GcalRecurrence*      gcal_event_get_recurrence                   (GcalEvent          *self);

void                 gcal_event_save_original_timezones          (GcalEvent          *self);

void                 gcal_event_get_original_timezones           (GcalEvent          *self,
                                                                  GTimeZone         **start_tz,
                                                                  GTimeZone         **end_tz);

gchar*               gcal_event_format_date                      (GcalEvent          *self);

G_END_DECLS

#endif /* GCAL_EVENT_H */
