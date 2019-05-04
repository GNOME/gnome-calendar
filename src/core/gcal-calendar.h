/* gcal-calendar.h
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

#include <libecal/libecal.h>
#include <gdk/gdk.h>

G_BEGIN_DECLS

/**
 * GcalCalendarError:
 * @GCAL_CALENDAR_ERROR_NOT_CALENDAR: the #ESource is not a calendar
 *
 * Errors that #GcalCalendar can generate.
 */
typedef enum
{
  GCAL_CALENDAR_ERROR_NOT_CALENDAR,
} GcalCalendarError;

#define GCAL_CALENDAR_ERROR (gcal_calendar_error_quark ())


#define GCAL_TYPE_CALENDAR (gcal_calendar_get_type())
G_DECLARE_DERIVABLE_TYPE (GcalCalendar, gcal_calendar, GCAL, CALENDAR, GObject)

struct _GcalCalendarClass
{
  GObjectClass parent_class;
};

GQuark               gcal_calendar_error_quark                   (void);

void                 gcal_calendar_new                           (ESource             *source,
                                                                  GCancellable        *cancellable,
                                                                  GAsyncReadyCallback  callback,
                                                                  gpointer             user_data);

GcalCalendar*        gcal_calendar_new_finish                    (GAsyncResult       *result,
                                                                  GError            **error);

ECalClient*          gcal_calendar_get_client                    (GcalCalendar       *self);

const GdkRGBA*       gcal_calendar_get_color                     (GcalCalendar       *self);

void                 gcal_calendar_set_color                     (GcalCalendar       *self,
                                                                  const GdkRGBA      *color);

const gchar*         gcal_calendar_get_id                        (GcalCalendar       *self);

const gchar*         gcal_calendar_get_name                      (GcalCalendar       *self);

void                 gcal_calendar_set_name                      (GcalCalendar       *self,
                                                                  const gchar        *name);

gboolean             gcal_calendar_is_read_only                  (GcalCalendar       *self);

ESource*             gcal_calendar_get_source                    (GcalCalendar       *self);

gboolean             gcal_calendar_get_visible                   (GcalCalendar       *self);

void                 gcal_calendar_set_visible                   (GcalCalendar       *self,
                                                                  gboolean            visible);

G_END_DECLS
