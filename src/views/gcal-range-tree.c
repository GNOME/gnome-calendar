/* gcal-range-tree.c
 *
 * Copyright (C) 2016 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#define G_LOG_DOMAIN "GcalRangeTree"

#include "gcal-range-tree.h"
#include "utils/gcal-date-time-utils.h"

/**
 * SECTION:gcal-range-tree
 * @short_description: Augmented AVL tree to handle ranges
 * @title:GcalRangeTree
 * @stability:stable
 *
 * #GcalRangeTree is an augmented AVL tree implemented to be
 * able to handle time ranges rather than point values. This
 * tree is used by @GcalWeekGrid to store events according to
 * their start and end times.
 *
 * By using #GDateTime to store the ranges, it supports a very
 * long time range.
 *
 * # Ranges
 *
 * Conforming to the overall standard of iCal and Calendar,
 * the start of a range is inclusive but the end of the range
 * is always exclusive.
 */

typedef struct _Node
{
  struct _Node       *left;
  struct _Node       *right;
  GDateTime          *start;
  GDateTime          *end;
  GDateTime          *max;
  guint16             hits;
  gint64              height;
  GPtrArray          *data_array;
} Node;

struct _GcalRangeTree
{
  guint               ref_count;

  Node               *root;
};

G_DEFINE_BOXED_TYPE (GcalRangeTree, gcal_range_tree, gcal_range_tree_ref, gcal_range_tree_unref)

/* Auxiliary methods */
static void
destroy_tree (Node *n)
{
  if (!n)
    return;

  destroy_tree (n->left);
  destroy_tree (n->right);

  gcal_clear_date_time (&n->start);
  gcal_clear_date_time (&n->end);
  gcal_clear_date_time (&n->max);

  g_ptr_array_unref (n->data_array);
  g_free (n);
}

static inline gint32
height (Node *n)
{
  return n ? n->height : 0;
}

static inline void
update_height (Node *n)
{
  n->height = MAX (height (n->left), height (n->right)) + 1;
}

static inline guint32
balance (Node *n)
{
  return n ? height (n->left) - height (n->right) : 0;
}

/*
 * Compares the ranges A [a_start, a_end] and B [b_start, b_end]. A is
 * less rangy than B when either it starts before B, or starts at the
 * same point but ends before B.
 *
 * Some examples:
 *
 * 1. [-----A----]     =  0
 *    [-----B----]
 *
 * 2. [----A----]      =  1
 *      [----B----]
 *
 * 3. [------A-----]   = -1
 *    [----B----]
 *
 * 4.   [----A----]    = -1
 *    [----B----]
 *
 * This is *not* comparing the sizes of the ranges.
 */
static inline gboolean
compare_intervals (GDateTime *a_start,
                   GDateTime *a_end,
                   GDateTime *b_start,
                   GDateTime *b_end)
{
  gint result = g_date_time_compare (b_start, a_start);

  if (result == 0)
    result = g_date_time_compare (b_end, a_end);

  return result;
}

static inline gboolean
is_interval_lower (Node      *n,
                   GDateTime *start,
                   GDateTime *end)
{
  gint a_compare = g_date_time_compare (start, n->start);
  return a_compare < 0 || (a_compare == 0 && g_date_time_compare (end, n->end) < 0);
}

static inline gboolean
is_interval_higher (Node      *n,
                    GDateTime *start,
                    GDateTime *end)
{
  gint a_compare = g_date_time_compare (start, n->start);
  return a_compare > 0 || (a_compare == 0 && g_date_time_compare (end, n->end) > 0);
}

static Node*
node_new (GDateTime *start,
          GDateTime *end,
          gpointer   data)
{
  Node *n;

  n = g_new0 (Node, 1);
  n->start = g_date_time_ref (start);
  n->end = g_date_time_ref (end);
  n->max = g_date_time_ref (end);
  n->height = 1;
  n->hits = 1;

  n->data_array = g_ptr_array_new ();
  g_ptr_array_add (n->data_array, data);

  return n;
}

/* AVL rotations */
static Node*
rotate_left (Node *n)
{
  Node *tmp;

  tmp = n->right;
  n->right = tmp->left;
  tmp->left = n;

  /* Update heights */
  update_height (n);
  update_height (tmp);

  return tmp;
}

static Node*
rotate_right (Node *n)
{
  Node *tmp;

  tmp = n->left;
  n->left = tmp->right;
  tmp->right = n;

  /* Update heights */
  update_height (n);
  update_height (tmp);

  return tmp;
}

static inline Node*
hit_node (Node     *n,
          gpointer  data)
{
  n->hits++;
  g_ptr_array_add (n->data_array, data);
  return n;
}

static inline Node*
rebalance (Node *n)
{
  gint32 node_balance;

  update_height (n);

  /* Rotate the tree */
  node_balance = balance (n);

  if (node_balance > 1)
    {
      if (n->left && height (n->left->right) > height (n->left->left))
        n->left = rotate_left (n->left);

      return rotate_right (n);
    }
  else if (node_balance < -1)
    {
      if (n->right && height (n->right->right) < height (n->right->left))
        n->right = rotate_right (n->right);

      return rotate_left (n);
    }

  return n;
}

static Node*
insert (Node      *n,
        GDateTime *start,
        GDateTime *end,
        gpointer   data)
{
  gint result;

  if (!n)
    return node_new (start, end, data);

  result = compare_intervals (n->start, n->end, start, end);

  if (result < 0)
    n->left = insert (n->left, start, end, data);
  else if (result > 0)
    n->right = insert (n->right, start, end, data);
  else
    return hit_node (n, data);

  /* Update the current node's maximum subrange value */
  if (!n->max || g_date_time_compare (end, n->max) > 0)
    {
      gcal_clear_date_time (&n->max);
      n->max = g_date_time_ref (end);
    }

  return rebalance (n);
}

/* Remove */
static Node*
find_minimum (Node *n)
{
  if (n->left)
    return find_minimum (n->left);

  return n;
}

static Node*
delete_minimum (Node *n)
{
  if (!n->left)
    return n->right;

  n->left = delete_minimum (n->left);

  return rebalance (n);
}

static inline Node*
delete_node (Node     *n,
             gpointer  data)
{
  Node *left, *right, *min;

  n->hits--;
  g_ptr_array_remove (n->data_array, data);

  /* Only remove the node when the hit count reaches zero */
  if (n->hits > 0)
    return n;

  left = n->left;
  right = n->right;

  g_ptr_array_unref (n->data_array);
  g_free (n);

  if (!right)
    return left;

  min = find_minimum (right);
  min->right = delete_minimum (right);
  min->left = left;

  return rebalance (min);
}

static Node*
remove_node (Node      *n,
             GDateTime *start,
             GDateTime *end,
             gpointer   data)
{
  if (!n)
    return NULL;

  if (is_interval_lower (n, start, end))
    n->left = remove_node (n->left, start, end, data);
  else if (is_interval_higher (n, start, end))
    n->right = remove_node (n->right, start, end, data);
  else
    return delete_node (n, data);

  return rebalance (n);
}

/* Traverse */
static inline gboolean
run_traverse_func (Node                  *n,
                   GcalRangeTraverseFunc  func,
                   gpointer               user_data)
{
  guint i;

  for (i = 0; i < n->hits; i++)
    {
      if (func (n->start, n->end, g_ptr_array_index (n->data_array, i), user_data))
        return TRUE;
    }

  return FALSE;
}

static gboolean
traverse (Node                  *n,
          GTraverseType          type,
          GcalRangeTraverseFunc  func,
          gpointer               user_data)
{
  if (!n)
    return GCAL_TRAVERSE_CONTINUE;

  if (type == G_PRE_ORDER)
    {
      if (run_traverse_func (n, func, user_data))
        return GCAL_TRAVERSE_STOP;
    }

  if (traverse (n->left, type, func, user_data))
    return GCAL_TRAVERSE_STOP;

  if (type == G_IN_ORDER)
    {
      if (run_traverse_func (n, func, user_data))
        return GCAL_TRAVERSE_STOP;
    }

  if (traverse (n->right, type, func, user_data))
    return GCAL_TRAVERSE_STOP;

  if (type == G_POST_ORDER)
    {
      if (run_traverse_func (n, func, user_data))
        return GCAL_TRAVERSE_STOP;
    }

  return GCAL_TRAVERSE_CONTINUE;
}

/* Internal traverse functions */
static inline gboolean
gather_all_data (GDateTime *start,
                 GDateTime *end,
                 gpointer   data,
                 gpointer   user_data)
{
  GPtrArray *array = user_data;

  g_ptr_array_add (array, data);

  return FALSE;
}

static inline gboolean
gather_data_at_range (GDateTime *start,
                      GDateTime *end,
                      gpointer   data,
                      gpointer   user_data)
{
  struct {
    GDateTime *start, *end;
    GPtrArray **array;
  } *range = user_data;

  /* Since we're traversing in order, stop here */
  if (compare_intervals (range->start, range->end, start, end) > 0 &&
      g_date_time_compare (start, range->end) >= 0)
    {
      return GCAL_TRAVERSE_STOP;
    }

  /*
   * If the current range doesn't overlap but we're before the desired range, keep
   * traversing the tree
   */
  if (g_date_time_compare (range->start, end) >= 0)
    return GCAL_TRAVERSE_CONTINUE;

  if (!*range->array)
    *range->array = g_ptr_array_new ();

  g_ptr_array_add (*range->array, data);

  return GCAL_TRAVERSE_CONTINUE;
}

static inline gboolean
count_entries_at_range (GDateTime *start,
                        GDateTime *end,
                        gpointer   data,
                        gpointer   user_data)
{
  struct {
    GDateTime *start, *end;
    guint64 counter;
  } *range = user_data;

  /* Since we're traversing in order, stop here */
  if (compare_intervals (range->start, range->end, start, end) > 0 &&
      g_date_time_compare (start, range->end) >= 0)
    {
      return GCAL_TRAVERSE_STOP;
    }

  /*
   * If the current range doesn't overlap but we're before the desired range, keep
   * traversing the tree
   */
  if (g_date_time_compare (range->start, end) >= 0)
    return GCAL_TRAVERSE_CONTINUE;

  range->counter++;

  return GCAL_TRAVERSE_CONTINUE;
}
static void
gcal_range_tree_free (GcalRangeTree *self)
{
  g_assert (self);
  g_assert_cmpint (self->ref_count, ==, 0);

  destroy_tree (self->root);

  g_slice_free (GcalRangeTree, self);
}

/**
 * gcal_range_tree_new:
 *
 * Creates a new range tree
 *
 * Returns: (transfer full): a newly created #GcalRangeTree.
 * Free with gcal_range_tree_unref() when done.
 */
GcalRangeTree*
gcal_range_tree_new (void)
{
  GcalRangeTree *self;

  self = g_slice_new0 (GcalRangeTree);
  self->ref_count = 1;

  return self;
}

/**
 * gcal_range_tree_copy:
 * @self: a #GcalRangeTree
 *
 * Copies @self into a new range tree.
 *
 * Returns: (transfer full): a newly created #GcalRangeTree.
 * Free with gcal_range_tree_unref() when done.
 */
GcalRangeTree*
gcal_range_tree_copy (GcalRangeTree *self)
{
  GcalRangeTree *copy;

  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (self->ref_count, NULL);

  copy = gcal_range_tree_new ();

  return copy;
}

/**
 * gcal_range_tree_ref:
 * @self: a #GcalRangeTree
 *
 * Increases the reference count of @self.
 *
 * Returns: (transfer full): pointer to the just-referenced tree.
 */
GcalRangeTree*
gcal_range_tree_ref (GcalRangeTree *self)
{
  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (self->ref_count, NULL);

  g_atomic_int_inc (&self->ref_count);

  return self;
}

/**
 * gcal_range_tree_unref:
 * @self: a #GcalRangeTree
 *
 * Decreases the reference count of @self, and frees it when
 * if the reference count reaches zero.
 */
void
gcal_range_tree_unref (GcalRangeTree *self)
{
  g_return_if_fail (self);
  g_return_if_fail (self->ref_count);

  if (g_atomic_int_dec_and_test (&self->ref_count))
    gcal_range_tree_free (self);
}

/**
 * gcal_range_tree_add_range:
 * @self: a #GcalRangeTree
 * @start: start of the interval
 * @end: end of the interval
 * @data: user data
 *
 * Adds the range into @self, and attach @data do it. It is
 * possible to have multiple entries in the same interval,
 * in which case the interval will have a reference counting like
 * behavior.
 */
void
gcal_range_tree_add_range (GcalRangeTree *self,
                           GDateTime     *start,
                           GDateTime     *end,
                           gpointer       data)
{
  g_return_if_fail (self);
  g_return_if_fail (end && start);
  g_return_if_fail (g_date_time_compare (end, start) >= 0);

  self->root = insert (self->root, start, end, data);
}

/**
 * gcal_range_tree_remove_range:
 * @self: a #GcalRangeTree
 * @start: the start of the range
 * @end: the end of the range
 * @data: user data
 *
 * Removes (or decreases the reference count) of the given interval.
 */
void
gcal_range_tree_remove_range (GcalRangeTree *self,
                              GDateTime     *start,
                              GDateTime     *end,
                              gpointer       data)
{
  g_return_if_fail (self);
  g_return_if_fail (end && start);
  g_return_if_fail (g_date_time_compare (end, start) >= 0);

  self->root = remove_node (self->root, start, end, data);
}

/**
 * gcal_range_tree_traverse:
 * @self: a #GcalRangeTree
 * @type: the type of traverse to perform
 * @func: the function to call on each traverse iteration
 * @user_data: user data for @func
 *
 * Traverse @self calling @func according to the @type specified.
 */
void
gcal_range_tree_traverse (GcalRangeTree         *self,
                          GTraverseType          type,
                          GcalRangeTraverseFunc  func,
                          gpointer               user_data)
{
  g_return_if_fail (self);

  traverse (self->root, type, func, user_data);
}

/**
 * gcal_range_tree_get_all_data:
 * @self: a #GcalRangeTree
 *
 * Retrieves all the data currently stored in @self.
 *
 * Returns: (transfer full): a #GPtrArray with the stored elements
 */
GPtrArray*
gcal_range_tree_get_all_data (GcalRangeTree *self)
{
  g_autoptr (GPtrArray) data = NULL;

  g_return_val_if_fail (self, NULL);

  data = g_ptr_array_new ();

  gcal_range_tree_traverse (self, G_IN_ORDER, gather_all_data, data);

  return g_steal_pointer (&data);
}

/**
 * gcal_range_tree_get_data_at_range:
 * @self: a #GcalRangeTree
 * @start: inclusive start of the range
 * @end: exclusive end of the range
 *
 * Retrieves the data of every node between @start and @end.
 *
 * Returns: (transfer full): a #GPtrArray. Unref with g_ptr_array_unref()
 * when finished.
 */
GPtrArray*
gcal_range_tree_get_data_at_range (GcalRangeTree *self,
                                   GDateTime     *start,
                                   GDateTime     *end)
{
  GPtrArray *data;

  struct {
    GDateTime *start, *end;
    GPtrArray **array;
  } range = { start, end, &data };

  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (end && start, NULL);
  g_return_val_if_fail (g_date_time_compare (end, start) >= 0, NULL);

  data = NULL;

  gcal_range_tree_traverse (self, G_IN_ORDER, gather_data_at_range, &range);

  return data;
}

/**
 * gcal_range_tree_count_entries_at_range:
 * @self: a #GcalRangeTree
 * @start: start of the range
 * @end: end of the range
 *
 * Counts the number of entries available in the given
 * range.
 *
 * Returns: the number of entries in the given range
 */
guint64
gcal_range_tree_count_entries_at_range (GcalRangeTree *self,
                                        GDateTime     *start,
                                        GDateTime     *end)
{
  struct {
    GDateTime *start, *end;
    guint64 counter;
  } range = { start, end, 0 };

  g_return_val_if_fail (self, 0);
  g_return_val_if_fail (end && start, 0);
  g_return_val_if_fail (g_date_time_compare (end, start) >= 0, 0);

  gcal_range_tree_traverse (self, G_IN_ORDER, count_entries_at_range, &range);

  return range.counter;
}
