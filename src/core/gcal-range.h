/* gcal-range.h
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

G_BEGIN_DECLS

#define GCAL_TYPE_RANGE (gcal_range_get_type ())
typedef struct _GcalRange GcalRange;

/**
 * GcalRangeOverlap:
 *
 * @GCAL_RANGE_NO_OVERLAP: the ranges don't overlap
 * @GCAL_RANGE_INTERSECTS: the ranges intersect, but have non-intersected areas
 * @GCAL_RANGE_SUBSET: the first range is a subset of the second range
 * @GCAL_RANGE_EQUAL: the ranges are exactly equal
 * @GCAL_RANGE_SUPERSET: the first range is a superset of the second range
 *
 * The possible results of comparing two ranges.
 */
typedef enum
{
  GCAL_RANGE_NO_OVERLAP,
  GCAL_RANGE_INTERSECTS,
  GCAL_RANGE_SUBSET,
  GCAL_RANGE_EQUAL,
  GCAL_RANGE_SUPERSET,
} GcalRangeOverlap;

/**
 * GcalRangePosition:
 *
 * @GCAL_RANGE_BEFORE: range @a is before @b
 * @GCAL_RANGE_AFTER: range @a is after @b
 *
 * The position of @a relative to @b. When the ranges are exactly equal, it
 * is undefined.
 */
typedef enum
{
  GCAL_RANGE_BEFORE = -1,
  GCAL_RANGE_MATCH  = 0,
  GCAL_RANGE_AFTER  = 1,
} GcalRangePosition;

/**
 * GcalRangeType:
 *
 * @GCAL_RANGE_DEFAULT: the default (date and time) range type
 * @GCAL_RANGE_DATE_ONLY: date-only range
 *
 * The type of the range. When comparing ranges, if either one of
 * them is date-only, times are not considered by the comparison.
 */
typedef enum
{
  GCAL_RANGE_DEFAULT,
  GCAL_RANGE_DATE_ONLY,
} GcalRangeType;

GType                gcal_range_get_type                         (void) G_GNUC_CONST;

GcalRange*           gcal_range_new                              (GDateTime          *range_start,
                                                                  GDateTime          *range_end,
                                                                  GcalRangeType       range_type);
GcalRange*           gcal_range_new_take                         (GDateTime          *range_start,
                                                                  GDateTime          *range_end,
                                                                  GcalRangeType       range_type);
GcalRange*           gcal_range_copy                             (GcalRange          *self);
GcalRange*           gcal_range_ref                              (GcalRange          *self);
void                 gcal_range_unref                            (GcalRange          *self);

GDateTime*           gcal_range_get_start                        (GcalRange          *self);
GDateTime*           gcal_range_get_end                          (GcalRange          *self);
GcalRangeType        gcal_range_get_range_type                   (GcalRange          *self);

GcalRangeOverlap     gcal_range_calculate_overlap                (GcalRange          *a,
                                                                  GcalRange          *b,
                                                                  GcalRangePosition  *out_position);

gint                 gcal_range_compare                          (GcalRange          *a,
                                                                  GcalRange          *b);

GcalRange*           gcal_range_union                            (GcalRange          *a,
                                                                  GcalRange          *b);

gchar*               gcal_range_to_string                        (GcalRange          *self);

gboolean             gcal_range_contains_datetime                (GcalRange          *self,
                                                                  GDateTime          *datetime);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GcalRange, gcal_range_unref)

G_END_DECLS
