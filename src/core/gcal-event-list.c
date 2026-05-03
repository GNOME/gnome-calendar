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

#define GDK_ARRAY_TYPE_NAME GcalEventArray
#define GDK_ARRAY_NAME gcal_event_array
#define GDK_ARRAY_ELEMENT_TYPE GcalEvent*
#define GDK_ARRAY_PREALLOC 50
#define GDK_ARRAY_FREE_FUNC g_object_unref
#include "gdkarrayimpl.c"

struct _GcalEventList
{
  GObject            parent_instance;

  GcalEventArray     event_array;
  GHashTable        *events;
};

static void          g_list_model_interface_init                 (GListModelInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (GcalEventList, gcal_event_list, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, g_list_model_interface_init))


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

  return gcal_event_array_get_size (&self->event_array);
}

static gpointer
gcal_event_list_get_item (GListModel *model,
                          guint       position)
{
  GcalEventList *self = (GcalEventList *) model;

  g_assert (GCAL_IS_EVENT_LIST (self));

  if (position < gcal_event_array_get_size (&self->event_array))
    return g_object_ref (gcal_event_array_get (&self->event_array, position));
  else
    return NULL;
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

  gcal_event_array_clear (&self->event_array);
  g_clear_pointer (&self->events, g_hash_table_destroy);

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
  gcal_event_array_init (&self->event_array);
  self->events = g_hash_table_new (g_direct_hash, g_direct_equal);
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

  position = gcal_event_array_get_size (&self->event_array);

  for (gsize i = 0; events[i]; i++)
    {
      if (g_hash_table_contains (self->events, events[i]))
        continue;

      gcal_event_array_append (&self->event_array, g_object_ref (events[i]));
      g_hash_table_add (self->events, events[i]);
      n_added++;
    }

  if (n_added > 0)
      g_list_model_items_changed (G_LIST_MODEL (self), position, 0, n_added);
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
  g_autoptr (GHashTable) contained_events = NULL;
  g_autoptr (GtkBitset) bitset = NULL;
  GtkBitsetIter iter;

  g_assert (GCAL_IS_EVENT_LIST (self));
  g_assert (events != NULL);

  bitset = gtk_bitset_new_empty ();
  contained_events = g_hash_table_new (g_direct_hash, g_direct_equal);

  for (size_t i = 0; events[i]; i++)
    {
      if (g_hash_table_contains (self->events, events[i]))
        g_hash_table_add (contained_events, events[i]);
    }

  for (size_t i = 0; i < gcal_event_array_get_size (&self->event_array); i++)
    {
      GcalEvent *event = gcal_event_array_get (&self->event_array, i);

      if (g_hash_table_contains (contained_events, event))
        {
          g_hash_table_remove (contained_events, event);
          g_hash_table_remove (self->events, event);
          gtk_bitset_add (bitset, i);
        }

      if (g_hash_table_size (contained_events) == 0)
        break;
    }

  for (gtk_bitset_iter_init_last (&iter, bitset, NULL);
       gtk_bitset_iter_is_valid (&iter);
       gtk_bitset_iter_previous (&iter, NULL))
    {
      gcal_event_array_splice (&self->event_array,
                               gtk_bitset_iter_get_value (&iter),
                               1,
                               FALSE,
                               NULL,
                               0);
    }

  if (!gtk_bitset_is_empty (bitset))
    {
      guint first = gtk_bitset_get_minimum (bitset);
      guint last = gtk_bitset_get_maximum (bitset);
      guint range_size = last - first + 1;
      guint n_removed = gtk_bitset_get_size_in_range (bitset, first, last);

      g_list_model_items_changed (G_LIST_MODEL (self), first, range_size, range_size - n_removed);
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

  n_removed = gcal_event_array_get_size (&self->event_array);

  g_hash_table_remove_all (self->events);
  gcal_event_array_clear (&self->event_array);

  if (n_removed > 0)
      g_list_model_items_changed (G_LIST_MODEL (self), 0, n_removed, 0);
}
