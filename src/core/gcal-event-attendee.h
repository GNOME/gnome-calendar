/* gcal-event-attendee.h
 *
 * Copyright (C) 2025 Alen Galinec <mind.on.warp@gmail.com>
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

#pragma once

#include "gcal-enums.h"
#include <glib-object.h>
#include <libecal/libecal.h>

G_BEGIN_DECLS

#define GCAL_TYPE_EVENT_ATTENDEE (gcal_event_attendee_get_type ())
G_DECLARE_FINAL_TYPE (GcalEventAttendee, gcal_event_attendee, GCAL, EVENT_ATTENDEE, GObject)

GcalEventAttendee         *gcal_event_attendee_new                (ECalComponentAttendee *ecal_attendee);
const gchar               *gcal_event_attendee_get_name           (GcalEventAttendee     *self);
GcalEventAttendeeType      gcal_event_attendee_get_attendee_type  (GcalEventAttendee     *self);
const gchar               *gcal_event_attendee_get_delegated_from (GcalEventAttendee     *self);
const gchar               *gcal_event_attendee_get_delegated_to   (GcalEventAttendee     *self);
const gchar               *gcal_event_attendee_get_language       (GcalEventAttendee     *self);
const gchar               *gcal_event_attendee_get_member         (GcalEventAttendee     *self);
GcalEventAttendeePartStat  gcal_event_attendee_get_part_status    (GcalEventAttendee     *self);
GcalEventAttendeeRole      gcal_event_attendee_get_role           (GcalEventAttendee     *self);
gboolean                   gcal_event_attendee_get_requires_rsvp  (GcalEventAttendee     *self);
const gchar               *gcal_event_attendee_get_sent_by        (GcalEventAttendee     *self);
const gchar               *gcal_event_attendee_get_uri            (GcalEventAttendee     *self);

G_END_DECLS
