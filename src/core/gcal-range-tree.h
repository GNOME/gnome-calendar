/* gcal-range-tree.h
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

#pragma once

#include "gcal-range.h"

#include <glib-object.h>

G_BEGIN_DECLS

#define GCAL_TYPE_RANGE_TREE (gcal_range_tree_get_type())

typedef struct _GcalRangeTree GcalRangeTree;

/**
 * GcalRangeTraverseFunc:
 * @start: #GDateTime with the start of range of the entry
 * @end: #GDateTime with the end of range of the entry
 * @data: (nullable): the data of the entry
 * @user_data: (closure): user data passed to the function
 *
 * Function type to be called when traversing a #GcalRangeTree.
 *
 * Returns: %TRUE to stop traversing, %FALSE to continue traversing
 */
typedef gboolean    (*GcalRangeTraverseFunc)                     (GcalRange          *range,
                                                                  gpointer            data,
                                                                  gpointer            user_data);

#define GCAL_TRAVERSE_CONTINUE FALSE;
#define GCAL_TRAVERSE_STOP     TRUE;

GType                gcal_range_tree_get_type                    (void) G_GNUC_CONST;

GcalRangeTree*       gcal_range_tree_new                         (void);

GcalRangeTree*       gcal_range_tree_new_with_free_func          (GDestroyNotify      destroy_func);

GcalRangeTree*       gcal_range_tree_copy                        (GcalRangeTree      *self);

GcalRangeTree*       gcal_range_tree_ref                         (GcalRangeTree      *self);

void                 gcal_range_tree_unref                       (GcalRangeTree      *self);

void                 gcal_range_tree_add_range                   (GcalRangeTree      *self,
                                                                  GcalRange          *range,
                                                                  gpointer            data);

void                 gcal_range_tree_remove_range                (GcalRangeTree      *self,
                                                                  GcalRange          *range,
                                                                  gpointer            data);

void                 gcal_range_tree_remove_data                 (GcalRangeTree      *self,
                                                                  gpointer            data);

void                 gcal_range_tree_traverse                    (GcalRangeTree      *self,
                                                                  GTraverseType       type,
                                                                  GcalRangeTraverseFunc func,
                                                                  gpointer           user_data);

GPtrArray*           gcal_range_tree_get_all_data                (GcalRangeTree      *self);

GPtrArray*           gcal_range_tree_get_data_at_range           (GcalRangeTree      *self,
                                                                  GcalRange          *range);

guint64              gcal_range_tree_count_entries_at_range      (GcalRangeTree      *self,
                                                                  GcalRange          *range);

void                 gcal_range_tree_print                       (GcalRangeTree      *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GcalRangeTree, gcal_range_tree_unref)

G_END_DECLS
