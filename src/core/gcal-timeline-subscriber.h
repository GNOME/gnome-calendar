/* gcal-timeline-subscriber.h
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

#include <glib-object.h>

#define LIBICAL_GLIB_UNSTABLE_API
#include <libical-glib/libical-glib.h>

#include "gcal-range.h"
#include "gcal-types.h"

G_BEGIN_DECLS

#define GCAL_TYPE_TIMELINE_SUBSCRIBER (gcal_timeline_subscriber_get_type ())

G_DECLARE_INTERFACE (GcalTimelineSubscriber, gcal_timeline_subscriber, GCAL, TIMELINE_SUBSCRIBER, GObject)

struct _GcalTimelineSubscriberInterface
{
  GTypeInterface parent;

  GcalRange*         (*get_range)                                (GcalTimelineSubscriber *self);

  void               (*add_event)                                (GcalTimelineSubscriber *self,
                                                                  GcalEvent              *event);

  void               (*update_event)                             (GcalTimelineSubscriber *self,
                                                                  GcalEvent              *old_event,
                                                                  GcalEvent              *event);

  void               (*remove_event)                             (GcalTimelineSubscriber *self,
                                                                  GcalEvent              *event);
};

GcalRange*           gcal_timeline_subscriber_get_range            (GcalTimelineSubscriber  *self);

void                 gcal_timeline_subscriber_range_changed        (GcalTimelineSubscriber *self);

G_END_DECLS
