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

/**
 * SECTION:gcal-range-tree
 * @short_description: Augmented AVL tree to handle ranges
 * @title:GcalRangeTree
 * @stability:stable
 *
 * #GcalRangeTree is an augmented AVL tree implemented to be
 * able to handle ranges rather than point values. This tree
 * is used by @GcalWeekGrid to store events according to their
 * start and end minutes, and thus all the data structure was
 * optimized to handle a limited subset of values.
 *
 * In the future, we may need to extend support for broader
 * range values, and move away from guint16 in function params.
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
  guint16             start;
  guint16             end;
  guint16             max;
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

static inline gboolean
compare_intervals (guint16 a_start,
                   guint16 a_end,
                   guint16 b_start,
                   guint16 b_end)
{
  if (a_start != b_start)
    return b_start - a_start;

  if (a_end != b_end)
    return b_end - a_end;

  return 0;
}

static inline gboolean
is_interval_lower (Node    *n,
                   guint32  start,
                   guint32  end)
{
  return start < n->start || (start == n->start && end < n->end);
}

static inline gboolean
is_interval_higher (Node    *n,
                    guint32  start,
                    guint32  end)
{
  return start > n->start || (start == n->start && end > n->end);
}

static Node*
node_new (guint32  start,
          guint32  end,
          gpointer data)
{
  Node *n;

  n = g_new0 (Node, 1);
  n->start = start;
  n->end = end;
  n->max = end;
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
insert (Node     *n,
        guint32   start,
        guint32   end,
        gpointer  data)
{
  if (!n)
    return node_new (start, end, data);

  if (compare_intervals (n->start, n->end, start, end) < 0)
    n->left = insert (n->left, start, end, data);
  else if (compare_intervals (n->start, n->end, start, end) > 0)
    n->right = insert (n->right, start, end, data);
  else
    return hit_node (n, data);

  /* Update the current node's maximum subrange value */
  n->max = MAX (n->max, end);

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
remove (Node     *n,
        guint32   start,
        guint32   end,
        gpointer  data)
{
  if (!n)
    return NULL;

  if (is_interval_lower (n, start, end))
    n->left = remove (n->left, start, end, data);
  else if (is_interval_higher (n, start, end))
    n->right = remove (n->right, start, end, data);
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
    return FALSE;

  if (type == G_PRE_ORDER)
    {
      if (run_traverse_func (n, func, user_data))
        return TRUE;
    }

  if (traverse (n->left, type, func, user_data))
    return TRUE;

  if (type == G_IN_ORDER)
    {
      if (run_traverse_func (n, func, user_data))
        return TRUE;
    }

  if (traverse (n->right, type, func, user_data))
    return TRUE;

  if (type == G_POST_ORDER)
    {
      if (run_traverse_func (n, func, user_data))
        return TRUE;
    }

  return FALSE;
}

/* Internal traverse functions */
static inline gboolean
gather_data_at_range (guint16  start,
                      guint16  end,
                      gpointer data,
                      gpointer user_data)
{
  struct {
    guint16 start, end;
    GPtrArray **array;
  } *range = user_data;

  /* Since we're traversing in order, stop here */
  if (compare_intervals (range->start, range->end, start, end) > 0 &&
      start >= range->end)
    {
      return TRUE;
    }

  /*
   * If the current range doesn't overlap but we're before the desired range, keep
   * traversing the tree
   */
  if (range->start >= end)
    return FALSE;

  if (!*range->array)
    *range->array = g_ptr_array_new ();

  g_ptr_array_add (*range->array, data);

  return FALSE;
}

static inline gboolean
count_entries_at_range (guint16  start,
                        guint16  end,
                        gpointer data,
                        gpointer user_data)
{
  struct {
    guint16 start, end;
    guint64 counter;
  } *range = user_data;

  /* Since we're traversing in order, stop here */
  if (compare_intervals (range->start, range->end, start, end) > 0 &&
      start >= range->end)
    {
      return TRUE;
    }

  /*
   * If the current range doesn't overlap but we're before the desired range, keep
   * traversing the tree
   */
  if (range->start >= end)
    return FALSE;

  range->counter++;

  return FALSE;
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
                           guint16        start,
                           guint16        end,
                           gpointer       data)
{
  g_return_if_fail (self);
  g_return_if_fail (end >= start);

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
                              guint16        start,
                              guint16        end,
                              gpointer       data)
{
  g_return_if_fail (self);
  g_return_if_fail (end >= start);

  self->root = remove (self->root, start, end, data);
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
                                   guint16        start,
                                   guint16        end)
{
  GPtrArray *data;

  struct {
    guint16 start, end;
    GPtrArray **array;
  } range = { start, end, &data };

  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (end >= start, NULL);

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
                                        guint16        start,
                                        guint16        end)
{
  struct {
    guint16 start, end;
    guint64 counter;
  } range = { start, end, 0 };

  g_return_val_if_fail (self, 0);
  g_return_val_if_fail (end >= start, 0);

  gcal_range_tree_traverse (self, G_IN_ORDER, count_entries_at_range, &range);

  return range.counter;
}
