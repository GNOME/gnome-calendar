/* gcal-timeline.h
 *
 * Copyright 2020 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#include "gcal-types.h"

#include <glib-object.h>

G_BEGIN_DECLS

#define GCAL_TYPE_TIMELINE (gcal_timeline_get_type())
G_DECLARE_FINAL_TYPE (GcalTimeline, gcal_timeline, GCAL, TIMELINE, GObject)

GcalTimeline*        gcal_timeline_new                           (GcalContext        *context);

void                 gcal_timeline_add_calendar                  (GcalTimeline       *self,
                                                                  GcalCalendar       *calendar);

void                 gcal_timeline_remove_calendar               (GcalTimeline       *self,
                                                                  GcalCalendar       *calendar);

void                 gcal_timeline_add_subscriber                (GcalTimeline           *self,
                                                                  GcalTimelineSubscriber *subscriber);

void                 gcal_timeline_remove_subscriber             (GcalTimeline           *self,
                                                                  GcalTimelineSubscriber *subscriber);

GPtrArray*           gcal_timeline_get_events_at_range           (GcalTimeline       *self,
                                                                  GDateTime          *range_start,
                                                                  GDateTime          *range_end);

const gchar*         gcal_timeline_get_filter                    (GcalTimeline       *self);

void                 gcal_timeline_set_filter                    (GcalTimeline       *self,
                                                                  const gchar        *filter);

gboolean             gcal_timeline_is_complete                   (GcalTimeline       *self);

G_END_DECLS
