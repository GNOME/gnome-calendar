/*
 * gcal-subscriber-view-private.h
 *
 * Copyright (C) 2014 - Erick Pérez Castellanos
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GCAL_SUBSCRIBER_VIEW_PRIVATE_H__
#define __GCAL_SUBSCRIBER_VIEW_PRIVATE_H____

#include <glib.h>

G_BEGIN_DECLS

struct _GcalSubscriberViewPrivate
{
  /**
   * Hash to keep children widgets (all of them, parent widgets and its parts if there's any),
   * uuid as key and a list of all the instances of the event as value. Here, the first widget on the list is
   * the master, and the rest are the parts. Note: that the master is a part itself, the first one
   */
  GHashTable     *children;

  /**
   * Hash containig single-day events, day of the month as key and a list of the events that belongs to this day
   */
  GHashTable     *single_cell_children;

  /**
   * An organizaed list containig multiday events
   * This one contains only parents events, to find out its parts @children will be used
   */
  GList          *multi_cell_children;

  /**
   * Hash containing cells that who has overflow per list of hidden widgets.
   */
  GHashTable     *overflow_cells;
  /**
   * Set containing the master widgets hidden for delete;
   */
  GHashTable     *hidden_as_overflow;
};

G_END_DECLS

#endif /* __GCAL_SUBSCRIBER_VIEW_PRIVATE_H____ */
