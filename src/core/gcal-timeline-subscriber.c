/* gcal-timeline-subscriber.c
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

#include "gcal-timeline-subscriber.h"

G_DEFINE_INTERFACE (GcalTimelineSubscriber, gcal_timeline_subscriber, G_TYPE_OBJECT)

enum
{
  RANGE_CHANGED,
  N_SIGNALS,
};

static guint signals[N_SIGNALS] = { 0, };

static void
gcal_timeline_subscriber_default_init (GcalTimelineSubscriberInterface *iface)
{
  /**
   * GcalTimelineSubscriber::range-changed:
   *
   * Emitted when the subscriber range is changed.
   */
  signals[RANGE_CHANGED] = g_signal_new ("range-changed",
                                         GCAL_TYPE_TIMELINE_SUBSCRIBER,
                                         G_SIGNAL_RUN_LAST,
                                         0,
                                         NULL, NULL, NULL,
                                         G_TYPE_NONE,
                                         0);
}

/**
 * gcal_timeline_subscriber_get_range:
 * @self: a #GcalTimelineSubscriber
 *
 * Retrieves the range of @self.
 *
 * Returns: (transfer full): a #GcalRange
 */
GcalRange*
gcal_timeline_subscriber_get_range (GcalTimelineSubscriber *self)
{
  g_return_val_if_fail (GCAL_IS_TIMELINE_SUBSCRIBER (self), NULL);

  return GCAL_TIMELINE_SUBSCRIBER_GET_IFACE (self)->get_range (self);
}

/**
 * gcal_timeline_subscriber_range_changed:
 * @self: a #GcalTimelineSubscriber
 *
 * Emits the "range-changed" signal for @self. Only
 * implementations of #GcalTimelineSubscriber are
 * expected to use this method.
 */
void
gcal_timeline_subscriber_range_changed (GcalTimelineSubscriber *self)
{
  g_return_if_fail (GCAL_IS_TIMELINE_SUBSCRIBER (self));

  g_signal_emit (self, signals[RANGE_CHANGED], 0);
}
