/* gcal-event-organizer.h
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

#include "libecal/libecal.h"
#include <glib-object.h>

G_BEGIN_DECLS

#define GCAL_TYPE_EVENT_ORGANIZER (gcal_event_organizer_get_type ())
G_DECLARE_FINAL_TYPE (GcalEventOrganizer, gcal_event_organizer, GCAL, EVENT_ORGANIZER, GObject)

GcalEventOrganizer *gcal_event_organizer_new          (ECalComponentOrganizer *ecal_organizer);
const gchar        *gcal_event_organizer_get_name     (GcalEventOrganizer     *self);
const gchar        *gcal_event_organizer_get_uri      (GcalEventOrganizer     *self);
const gchar        *gcal_event_organizer_get_sent_by  (GcalEventOrganizer     *self);
const gchar        *gcal_event_organizer_get_language (GcalEventOrganizer     *self);

G_END_DECLS
