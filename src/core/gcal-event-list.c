/*
 * gcal-event-list.c
 *
 * Copyright 2025 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#define G_LOG_DOMAIN "GcalEventList"

#include "gcal-event.h"
#include "gcal-event-list.h"

#include <gio/gio.h>

struct _GcalEventList
{
  GObject            parent_instance;

  GSequence         *events;
  GHashTable        *event_to_iter;
  GHashTable        *uid_to_iter;
  guint              n_events;

  /* cache */
  guint             last_position;
  GSequenceIter    *last_iter;
  gboolean          last_position_valid;
};

static void          g_list_model_interface_init                 (GListModelInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (GcalEventList, gcal_event_list, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, g_list_model_interface_init))


/*
 * Auxiliary functions
 */

static void
gcal_event_list_items_changed (GcalEventList *self,
                               guint          position,
                               guint          removed,
                               guint          added)
{
  /* check if the iter cache may have been invalidated */
  if (position <= self->last_position)
    {
      self->last_iter = NULL;
      self->last_position = 0;
      self->last_position_valid = FALSE;
    }

  g_list_model_items_changed (G_LIST_MODEL (self), position, removed, added);
}


/*
 * GListModel interface
 */

static GType
gcal_event_list_get_item_type (GListModel *model G_GNUC_UNUSED)
{
  return GCAL_TYPE_EVENT;
}

static guint
gcal_event_list_get_n_items (GListModel *model)
{
  GcalEventList *self = (GcalEventList *) model;

  g_assert (GCAL_IS_EVENT_LIST (self));

  return g_hash_table_size (self->event_to_iter);
}

static gpointer
gcal_event_list_get_item (GListModel *model,
                          guint       position)
{
  GcalEventList *self = (GcalEventList *) model;
  GSequenceIter *it = NULL;

  g_assert (GCAL_IS_EVENT_LIST (self));

  if (self->last_position_valid)
    {
      if (position < G_MAXUINT && self->last_position == position + 1)
        it = g_sequence_iter_prev (self->last_iter);
      else if (position > 0 && self->last_position == position - 1)
        it = g_sequence_iter_next (self->last_iter);
      else if (self->last_position == position)
        it = self->last_iter;
    }

  if (it == NULL)
    it = g_sequence_get_iter_at_pos (self->events, position);

  self->last_iter = it;
  self->last_position = position;
  self->last_position_valid = TRUE;

  if (g_sequence_iter_is_end (it))
    return NULL;
  else
    return g_object_ref (g_sequence_get (it));
}

static void
g_list_model_interface_init (GListModelInterface *iface)
{
  iface->get_item_type = gcal_event_list_get_item_type;
  iface->get_n_items = gcal_event_list_get_n_items;
  iface->get_item = gcal_event_list_get_item;
}


/*
 * GObject overrides
 */

static void
gcal_event_list_finalize (GObject *object)
{
  GcalEventList *self = (GcalEventList *)object;

  g_clear_pointer (&self->event_to_iter, g_hash_table_destroy);
  g_clear_pointer (&self->events, g_sequence_free);

  G_OBJECT_CLASS (gcal_event_list_parent_class)->finalize (object);
}

static void
gcal_event_list_class_init (GcalEventListClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gcal_event_list_finalize;
}

static void
gcal_event_list_init (GcalEventList *self)
{
  self->events = g_sequence_new (g_object_unref);
  self->event_to_iter = g_hash_table_new (g_direct_hash, g_direct_equal);
}

/**
 * gcal_event_list_new:
 *
 */
GcalEventList *
gcal_event_list_new (void)
{
  return g_object_new (GCAL_TYPE_EVENT_LIST, NULL);
}

/**
 * gcal_event_list_add_event:
 *
 * Adds @event to the list. A reference is taken.
 * If @event is already part of the list, nothing
 * happens.
 */
void
gcal_event_list_add_event (GcalEventList *self,
                           GcalEvent     *event)
{
  GcalEvent *events[2] = { };

  g_assert (GCAL_IS_EVENT_LIST (self));
  g_assert (GCAL_IS_EVENT (event));

  events[0] = event;
  events[1] = NULL;

  gcal_event_list_add_events (self, events);
}

/**
 * gcal_event_list_add_events:
 * @events: a NULL-terminated list of events
 *
 * Adds @events to the list. A reference is taken.
 * Events that are already part of the list are
 * ignored.
 */
void
gcal_event_list_add_events (GcalEventList  *self,
                            GcalEvent     **events)
{
  unsigned int position = 0;
  unsigned int n_added = 0;

  g_assert (GCAL_IS_EVENT_LIST (self));
  g_assert (events != NULL);

  position = g_hash_table_size (self->event_to_iter);

  for (gsize i = 0; events[i]; i++)
    {
      GSequenceIter *iter;

      if (g_hash_table_contains (self->event_to_iter, events[i]))
        continue;

      iter = g_sequence_append (self->events, g_object_ref (events[i]));

      g_hash_table_insert (self->event_to_iter, events[i], iter);
      n_added++;
    }

  if (n_added > 0)
      gcal_event_list_items_changed (self, position, 0, n_added);
}

/**
 * gcal_event_list_remove_event:
 *
 * Removes @event from the list. If the event is not part of
 * the list, nothing happens.
 */
void
gcal_event_list_remove_event (GcalEventList *self,
                              GcalEvent     *event)
{
  GcalEvent *events[2] = { };

  g_assert (GCAL_IS_EVENT_LIST (self));
  g_assert (GCAL_IS_EVENT (event));

  events[0] = event;
  events[1] = NULL;

  gcal_event_list_remove_events (self, events);
}

/**
 * gcal_event_list_remove_events:
 * @events: a NULL-terminated list of events
 *
 * Removes @events from the list. Events that are not part of
 * the list are ignored.
 */
void
gcal_event_list_remove_events (GcalEventList  *self,
                               GcalEvent     **events)
{
  g_autoptr (GtkBitset) bitset = NULL;
  g_autoptr (GPtrArray) iters = NULL;

  g_assert (GCAL_IS_EVENT_LIST (self));
  g_assert (events != NULL);

  bitset = gtk_bitset_new_empty ();
  iters = g_ptr_array_new ();

  for (gsize i = 0; events[i]; i++)
    {
      GSequenceIter *iter = g_hash_table_lookup (self->event_to_iter, events[i]);

      if (!iter)
        continue;

      g_ptr_array_add (iters, iter);
      gtk_bitset_add (bitset, g_sequence_iter_get_position (iter));

      g_hash_table_remove (self->event_to_iter, events[i]);
    }

  for (gsize i = 0; i < iters->len; i++)
    {
      GSequenceIter *iter = g_ptr_array_index (iters, i);

      g_assert (iter != NULL);

      g_sequence_remove (iter);
    }

  if (!gtk_bitset_is_empty (bitset))
    {
      guint first = gtk_bitset_get_minimum (bitset);
      guint last = gtk_bitset_get_maximum (bitset);
      guint range_size = last - first + 1;
      guint n_removed = gtk_bitset_get_size_in_range (bitset, first, last);

      gcal_event_list_items_changed (self, first, range_size, range_size - n_removed);
    }
}

/**
 * gcal_event_list_remove_all_events:
 *
 * Removes all events from the list.
 */
void
gcal_event_list_remove_all_events (GcalEventList  *self)
{
  unsigned int n_removed = 0;

  g_assert (GCAL_IS_EVENT_LIST (self));

  n_removed = g_hash_table_size (self->event_to_iter);

  g_hash_table_remove_all (self->event_to_iter);
  g_sequence_remove_range (g_sequence_get_begin_iter (self->events),
                           g_sequence_get_end_iter (self->events));

  if (n_removed > 0)
      gcal_event_list_items_changed (self, 0, n_removed, 0);
}
