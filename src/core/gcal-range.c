/* gcal-range.c
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

#include "gcal-range.h"

#include "gcal-date-time-utils.h"

struct _GcalRange
{
  gatomicrefcount     ref_count;

  GDateTime          *range_start;
  GDateTime          *range_end;
  GcalRangeType       range_type;
};

G_DEFINE_BOXED_TYPE (GcalRange, gcal_range, gcal_range_ref, gcal_range_unref)

typedef int (*CompareDateTimeFunc) (GDateTime *a,
                                    GDateTime *b);

static CompareDateTimeFunc
get_compare_func (GcalRange *a,
                  GcalRange *b)
{
  if (a->range_type == GCAL_RANGE_DATE_ONLY || b->range_type == GCAL_RANGE_DATE_ONLY)
    return gcal_date_time_compare_date;
  else
    return (CompareDateTimeFunc) g_date_time_compare;
}

/**
 * gcal_range_copy:
 * @self: a #GcalRange
 *
 * Makes a deep copy of a #GcalRange.
 *
 * Returns: (transfer full): A newly created #GcalRange with the same
 *   contents as @self
 */
GcalRange *
gcal_range_copy (GcalRange *self)
{
  GcalRange *copy;

  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (!g_atomic_ref_count_compare (&self->ref_count, 0), NULL);

  copy = gcal_range_new (self->range_start, self->range_end, self->range_type);

  return copy;
}

static void
gcal_range_free (GcalRange *self)
{
  g_assert (self);
  g_assert (g_atomic_ref_count_compare (&self->ref_count, 0));

  gcal_clear_date_time (&self->range_start);
  gcal_clear_date_time (&self->range_end);
  g_free (self);
}

/**
 * gcal_range_ref:
 * @self: A #GcalRange
 *
 * Increments the reference count of @self by one.
 *
 * Returns: (transfer full): @self
 */
GcalRange *
gcal_range_ref (GcalRange *self)
{
  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (!g_atomic_ref_count_compare (&self->ref_count, 0), NULL);

  g_atomic_ref_count_inc (&self->ref_count);

  return self;
}

/**
 * gcal_range_unref:
 * @self: A #GcalRange
 *
 * Decrements the reference count of @self by one, freeing the structure when
 * the reference count reaches zero.
 */
void
gcal_range_unref (GcalRange *self)
{
  g_return_if_fail (self);
  g_return_if_fail (!g_atomic_ref_count_compare (&self->ref_count, 0));

  if (g_atomic_ref_count_dec (&self->ref_count))
    gcal_range_free (self);
}

/**
 * gcal_range_new:
 *
 * Creates a new #GcalRange.
 *
 * Returns: (transfer full): A newly created #GcalRange
 */
GcalRange*
gcal_range_new (GDateTime     *range_start,
                GDateTime     *range_end,
                GcalRangeType  range_type)
{
  g_return_val_if_fail (range_start, NULL);
  g_return_val_if_fail (range_end, NULL);
  g_return_val_if_fail (g_date_time_compare (range_start, range_end) <= 0, NULL);

  return gcal_range_new_take (g_date_time_ref (range_start),
                              g_date_time_ref (range_end),
                              range_type);
}


/**
 * gcal_range_new_take:
 * @range_start: (transfer full): a #GDateTime
 * @range_end: (transfer full): a #GDateTime
 *
 * Creates a new #GcalRange and takes ownership of
 * @range_start and @range_end.
 *
 * Returns: (transfer full): A newly created #GcalRange
 */
GcalRange*
gcal_range_new_take (GDateTime     *range_start,
                     GDateTime     *range_end,
                     GcalRangeType  range_type)
{
  GcalRange *self;

  g_return_val_if_fail (range_start, NULL);
  g_return_val_if_fail (range_end, NULL);
  g_return_val_if_fail (g_date_time_compare (range_start, range_end) <= 0, NULL);

  self = g_new0 (GcalRange, 1);
  g_atomic_ref_count_init (&self->ref_count);

  self->range_start = range_start;
  self->range_end = range_end;
  self->range_type = range_type;

  return self;
}

/**
 * gcal_range_get_start:
 * @self: a #GcalRange
 *
 * Retrieves a copy of @self's range start.
 *
 * Returns: (transfer full): a #GDateTime
 */
GDateTime*
gcal_range_get_start (GcalRange *self)
{
  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (!g_atomic_ref_count_compare (&self->ref_count, 0), NULL);

  return g_date_time_ref (self->range_start);
}

/**
 * gcal_range_get_end:
 * @self: a #GcalRange
 *
 * Retrieves a copy of @self's range end.
 *
 * Returns: (transfer full): a #GDateTime
 */
GDateTime*
gcal_range_get_end (GcalRange *self)
{
  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (!g_atomic_ref_count_compare (&self->ref_count, 0), NULL);

  return g_date_time_ref (self->range_end);
}

/**
 * gcal_range_get_range_type:
 * @self: a #GcalRange
 *
 * Retrieves the range type of @self.
 *
 * Returns: the range type of @self
 */
GcalRangeType
gcal_range_get_range_type (GcalRange *self)
{
  g_return_val_if_fail (self, GCAL_RANGE_DEFAULT);
  g_return_val_if_fail (!g_atomic_ref_count_compare (&self->ref_count, 0), GCAL_RANGE_DEFAULT);

  return self->range_type;
}

/**
 * gcal_range_calculate_overlap:
 * @a: a #GcalRange
 * @b: a #GcalRange
 * @out_position: (direction out)(nullable): return location for a #GcalRangePosition
 *
 * Calculates how @a and @b overlap.
 *
 * The position returned at @out_position is always relative to @a. For example,
 * %GCAL_RANGE_AFTER means @a is after @b. The heuristic for the position is:
 *
 *  1. If @a begins before @b, @a comes before @b.
 *  2. If @a and @b begin at precisely the same moment, but @a ends before @b,
 *     @a comes before @b.
 *  3. Otherwise, @b comes before @a.
 *
 * Returns: the overlap result between @a and @b
 */
GcalRangeOverlap
gcal_range_calculate_overlap (GcalRange         *a,
                              GcalRange         *b,
                              GcalRangePosition *out_position)
{
  CompareDateTimeFunc compare_func;
  GcalRangePosition position;
  GcalRangeOverlap overlap;
  gint a_start_b_start_diff;
  gint a_end_b_end_diff;

  g_return_val_if_fail (a && b, GCAL_RANGE_NO_OVERLAP);

  /*
   * There are 11 cases that we need to take care of:
   *
   * 1. Equal
   *
   *   A |------------------------|
   *   B |------------------------|
   *
   * 2. Superset
   *
   * i.
   *   A |------------------------|
   *   B |-------------------|
   *
   * ii.
   *   A |------------------------|
   *   B   |-------------------|
   *
   * iii.
   *   A |------------------------|
   *   B      |-------------------|
   *
   * 3. Subset
   *
   * i.
   *   A |-------------------|
   *   B |------------------------|
   *
   * ii.
   *   A   |-------------------|
   *   B |------------------------|
   *
   * iii.
   *   A      |-------------------|
   *   B |------------------------|
   *
   * 4. Intersection
   *
   * i.
   *   A |--------------------|
   *   B     |--------------------|
   *
   * ii.
   *   A     |--------------------|
   *   B |--------------------|
   *
   * 5) No Overlap
   *
   * i.
   *   A             |------------|
   *   B |-----------|
   *
   * ii.
   *   A |------------|
   *   B              |-----------|
   *
   */

  compare_func = get_compare_func (a, b);
  a_start_b_start_diff = compare_func (a->range_start, b->range_start);
  a_end_b_end_diff = compare_func (a->range_end, b->range_end);

  if (a_start_b_start_diff == 0 && a_end_b_end_diff == 0)
    {
      /* Case 1, the easiest */
      overlap = GCAL_RANGE_EQUAL;
      position = GCAL_RANGE_MATCH;
    }
  else
    {
      if (a_start_b_start_diff == 0)
        {
          if (a_end_b_end_diff > 0)
            {
              /* Case 2.i */
              overlap = GCAL_RANGE_SUPERSET;
              position = GCAL_RANGE_AFTER;
            }
          else /* a_end_b_end_diff < 0 */
            {
              /* Case 3.i */
              overlap = GCAL_RANGE_SUBSET;
              position = GCAL_RANGE_BEFORE;
            }
        }
      else if (a_end_b_end_diff == 0)
        {
          gint a_start_b_end_diff;
          gint a_end_b_start_diff;

          a_start_b_end_diff = compare_func (a->range_start, b->range_end);
          a_end_b_start_diff = compare_func (a->range_end, b->range_start);

          if (a_start_b_end_diff >= 0)
            {
              /* Case 5.i for zero length A */
              overlap = GCAL_RANGE_NO_OVERLAP;
              position = GCAL_RANGE_AFTER;
            }
          else if (a_end_b_start_diff <= 0)
            {
              /* Case 5.ii for zero length B */
              overlap = GCAL_RANGE_NO_OVERLAP;
              position = GCAL_RANGE_BEFORE;
            }
          else if (a_start_b_start_diff < 0)
            {
              /* Case 2.iii */
              overlap = GCAL_RANGE_SUPERSET;
              position = GCAL_RANGE_BEFORE;
            }
          else /* a_start_b_start_diff > 0 */
            {
              /* Case 3.iii */
              overlap = GCAL_RANGE_SUBSET;
              position = GCAL_RANGE_AFTER;
            }
        }
      else /* a_start_b_start_diff != 0 && a_end_b_end_diff != 0 */
        {
          if (a_start_b_start_diff < 0 && a_end_b_end_diff > 0)
            {
              /* Case 2.ii */
              overlap = GCAL_RANGE_SUPERSET;
              position = GCAL_RANGE_BEFORE;
            }
          else if (a_start_b_start_diff > 0 && a_end_b_end_diff < 0)
            {
              /* Case 3.ii */
              overlap = GCAL_RANGE_SUBSET;
              position = GCAL_RANGE_AFTER;
            }
          else
            {
              gint a_start_b_end_diff;
              gint a_end_b_start_diff;

              a_start_b_end_diff = compare_func (a->range_start, b->range_end);
              a_end_b_start_diff = compare_func (a->range_end, b->range_start);

              /* No overlap cases */
              if (a_start_b_end_diff >= 0)
                {
                  /* Case 5.i */
                  overlap = GCAL_RANGE_NO_OVERLAP;
                  position = GCAL_RANGE_AFTER;
                }
              else if (a_end_b_start_diff <= 0)
                {
                  /* Case 5.ii */
                  overlap = GCAL_RANGE_NO_OVERLAP;
                  position = GCAL_RANGE_BEFORE;
                }
              else
                {
                  /* Intersection cases */
                  if (a_start_b_start_diff < 0 && a_end_b_start_diff > 0 && a_end_b_end_diff < 0)
                    {
                      /* Case 4.i */
                      overlap = GCAL_RANGE_INTERSECTS;
                      position = GCAL_RANGE_BEFORE;
                    }
                  else if (a_start_b_start_diff > 0 && a_start_b_end_diff < 0 && a_end_b_end_diff > 0)
                    {
                      /* Case 4.ii */
                      overlap = GCAL_RANGE_INTERSECTS;
                      position = GCAL_RANGE_AFTER;
                    }
                  else
                    {
                      g_assert_not_reached ();
                    }
                }
            }
        }
    }

  if (out_position)
    *out_position = position;

  return overlap;
}

/**
 * gcal_range_compare:
 * @a: a #GcalRange
 * @b: a #GcalRange
 *
 * Compares @a and @b. See gcal_range_calculate_overlap() for the
 * rules of when a range comes before or after.
 *
 * Returns: -1 is @a comes before @b, 0 if they're equal, or 1 if
 * @a comes after @b.
 */
gint
gcal_range_compare (GcalRange *a,
                    GcalRange *b)
{
  CompareDateTimeFunc compare_func;
  gint result;

  g_return_val_if_fail (a && b, 0);

  compare_func = get_compare_func (a, b);
  result = compare_func (a->range_start, b->range_start);

  if (result == 0)
    result = compare_func (a->range_end, b->range_end);

  return result;
}

/**
 * gcal_range_union:
 * @a: a #GcalRange
 * @b: a #GcalRange
 *
 * Creates a new #GcalRange with the union of @a and @b.
 *
 * Returns: (transfer full): a #GcalRange.
 */
GcalRange*
gcal_range_union (GcalRange *a,
                  GcalRange *b)
{
  CompareDateTimeFunc compare_func;
  GcalRangeType range_type;
  GDateTime *start;
  GDateTime *end;

  g_return_val_if_fail (a != NULL, NULL);
  g_return_val_if_fail (b != NULL, NULL);

  compare_func = get_compare_func (a, b);

  if (compare_func (a->range_start, b->range_start) < 0)
    start = a->range_start;
  else
    start = b->range_start;

  if (compare_func (a->range_end, b->range_end) > 0)
    end = a->range_end;
  else
    end = b->range_end;

  if (a->range_type == GCAL_RANGE_DATE_ONLY || b->range_type == GCAL_RANGE_DATE_ONLY)
    range_type = GCAL_RANGE_DATE_ONLY;
  else
    range_type = GCAL_RANGE_DEFAULT;

  return gcal_range_new (start, end, range_type);
}

/**
 * gcal_range_to_string:
 * @self: a #GcalRange
 *
 * Formats @self using ISO8601 dates. This is only useful
 * for debugging purposes.
 *
 * Returns: (transfer full): a string representation of @self
 */
gchar*
gcal_range_to_string (GcalRange *self)
{
  g_autofree gchar *start_string = NULL;
  g_autofree gchar *end_string = NULL;

  g_return_val_if_fail (self, NULL);

  start_string = g_date_time_format_iso8601 (self->range_start);
  end_string = g_date_time_format_iso8601 (self->range_end);

  return g_strdup_printf ("[%s | %s)", start_string, end_string);
}

/**
 * gcal_range_contains_datetime:
 * @self: a #GcalRange
 * @datetime: a #GDateTime
 *
 * Checks if @datetime is contained within @self.
 *
 * Returns: %TRUE is @datetime is within the range described
 * by @self, %FALSE otherwise
 */
gboolean
gcal_range_contains_datetime (GcalRange *self,
                              GDateTime *datetime)
{
  g_return_val_if_fail (self, FALSE);
  g_return_val_if_fail (datetime, FALSE);

  switch (self->range_type)
    {
    case GCAL_RANGE_DEFAULT:
      return g_date_time_compare (datetime, self->range_start) >= 0 &&
             g_date_time_compare (datetime, self->range_end) < 0;

    case GCAL_RANGE_DATE_ONLY:
      return gcal_date_time_compare_date (datetime, self->range_start) >= 0 &&
             gcal_date_time_compare_date (datetime, self->range_end) <= 0;
    default:
      g_assert_not_reached ();
    }
}
