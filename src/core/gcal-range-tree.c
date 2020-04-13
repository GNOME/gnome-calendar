/* gcal-range-tree.c
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
  GcalRange          *range;
  GDateTime          *max;
  guint16             hits;
  gint64              height;
  GPtrArray          *data_array;
} Node;

struct _GcalRangeTree
{
  guint               ref_count;

  GDestroyNotify      destroy_func;
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

  g_clear_pointer (&n->range, gcal_range_unref);
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

static Node*
node_new (GcalRange      *range,
          gpointer        data,
          GDestroyNotify  destroy_func)
{
  Node *n;

  n = g_new0 (Node, 1);
  n->range = gcal_range_ref (range);
  n->max = gcal_range_get_end (range);
  n->height = 1;
  n->hits = 1;

  n->data_array = g_ptr_array_new_with_free_func (destroy_func);
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
insert (Node           *n,
        GcalRange      *range,
        gpointer        data,
        GDestroyNotify  destroy_func)
{
  g_autoptr (GDateTime) range_end = NULL;
  gint result;

  if (!n)
    return node_new (range, data, destroy_func);

  result = gcal_range_compare (range, n->range);

  if (result < 0)
    n->left = insert (n->left, range, data, destroy_func);
  else if (result > 0)
    n->right = insert (n->right, range, data, destroy_func);
  else
    return hit_node (n, data);

  range_end = gcal_range_get_end (range);

  /* Update the current node's maximum subrange value */
  if (!n->max || g_date_time_compare (range_end, n->max) > 0)
    gcal_set_date_time (&n->max, range_end);

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
             GcalRange *range,
             gpointer   data)
{
  GcalRangePosition position;

  if (!n)
    return NULL;

  gcal_range_calculate_overlap (range, n->range, &position);

  switch (position)
    {
    case GCAL_RANGE_BEFORE:
      n->left = remove_node (n->left, range, data);
      break;

    case GCAL_RANGE_MATCH:
      return delete_node (n, data);

    case GCAL_RANGE_AFTER:
      n->right = remove_node (n->right, range, data);
      break;

    default:
      g_assert_not_reached ();
    }

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
      if (func (n->range, g_ptr_array_index (n->data_array, i), user_data))
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
gather_all_data (GcalRange *range,
                 gpointer   data,
                 gpointer   user_data)
{
  GPtrArray *array = user_data;

  g_ptr_array_add (array, data);

  return FALSE;
}

static inline gboolean
gather_data_at_range (GcalRange *range,
                      gpointer   data,
                      gpointer   user_data)
{
  GcalRangePosition position;
  GcalRangeOverlap overlap;

  struct {
    GcalRange *range;
    GPtrArray **array;
  } *gather_data = user_data;

  overlap = gcal_range_calculate_overlap (range, gather_data->range, &position);

  if (overlap == GCAL_RANGE_NO_OVERLAP)
    {
      if (position == GCAL_RANGE_BEFORE)
        return GCAL_TRAVERSE_CONTINUE;
      if (position == GCAL_RANGE_AFTER)
        return GCAL_TRAVERSE_STOP;
    }

  if (!*gather_data->array)
    *gather_data->array = g_ptr_array_new ();

  g_ptr_array_add (*gather_data->array, data);

  return GCAL_TRAVERSE_CONTINUE;
}

static inline gboolean
count_entries_at_range (GcalRange *range,
                        gpointer   data,
                        gpointer   user_data)
{
  GcalRangePosition position;
  GcalRangeOverlap overlap;

  struct {
    GcalRange *range;
    guint64 counter;
  } *gather_data = user_data;

  overlap = gcal_range_calculate_overlap (range, gather_data->range, &position);

  if (overlap == GCAL_RANGE_NO_OVERLAP)
    {
      if (position == GCAL_RANGE_BEFORE)
        return GCAL_TRAVERSE_CONTINUE;
      if (position == GCAL_RANGE_AFTER)
        return GCAL_TRAVERSE_STOP;
    }

  gather_data->counter++;

  return GCAL_TRAVERSE_CONTINUE;
}

static inline gboolean
remove_data_func (GcalRange *range,
                  gpointer   data,
                  gpointer   user_data)
{
  struct {
    GcalRangeTree *range_tree;
    gpointer      *data;
  } *remove_data = user_data;

  if (remove_data->data == data)
    {
      gcal_range_tree_remove_range (remove_data->range_tree, range, data);
      return GCAL_TRAVERSE_STOP;
    }

  return GCAL_TRAVERSE_CONTINUE;
}

static void
recursively_print_node_to_string (Node    *n,
                                  GString *string,
                                  gint     depth)
{
  g_autofree gchar *range = NULL;
  gint64 i;

  for (i = 0; i < depth * 2; i++)
    g_string_append (string, "  ");

  if (!n)
    {
      g_string_append (string, "(null)\n");
      return;
    }

  range = gcal_range_to_string (n->range);
  g_string_append_printf (string, "Node %s (hits: %d)\n", range, n->hits);

  recursively_print_node_to_string (n->left, string, depth + 1);
  recursively_print_node_to_string (n->right, string, depth + 1);
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
 * gcal_range_tree_new_with_free_func:
 * @destroy_func: (nullable): a function to free elements with
 *
 * Creates a new range tree with @destroy_func as the function to
 * destroy elements when removing them.
 *
 * Returns: (transfer full): a newly created #GcalRangeTree.
 * Free with gcal_range_tree_unref() when done.
 */
GcalRangeTree*
gcal_range_tree_new_with_free_func (GDestroyNotify destroy_func)
{
  GcalRangeTree *self;

  self = g_slice_new0 (GcalRangeTree);
  self->ref_count = 1;
  self->destroy_func = destroy_func;

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
 * @range: a #GcalRange
 * @data: user data
 *
 * Adds the range into @self, and attach @data do it. It is
 * possible to have multiple entries in the same interval,
 * in which case the interval will have a reference counting like
 * behavior.
 */
void
gcal_range_tree_add_range (GcalRangeTree *self,
                           GcalRange     *range,
                           gpointer       data)
{
  g_return_if_fail (self);
  g_return_if_fail (range);

  self->root = insert (self->root, range, data, self->destroy_func);
}

/**
 * gcal_range_tree_remove_range:
 * @self: a #GcalRangeTree
 * @range: a #GcalRange
 * @data: user data
 *
 * Removes (or decreases the reference count) of the given interval.
 */
void
gcal_range_tree_remove_range (GcalRangeTree *self,
                              GcalRange     *range,
                              gpointer       data)
{
  g_return_if_fail (self);
  g_return_if_fail (range);

  self->root = remove_node (self->root, range, data);
}

/**
 * gcal_range_tree_remove_data:
 * @self: a #GcalRangeTree
 * @data: user data
 *
 * Removes the first instance of @data found in the range tree.
 */
void
gcal_range_tree_remove_data (GcalRangeTree *self,
                             gpointer       data)
{
  struct {
    GcalRangeTree *range_tree;
    gpointer      *data;
  } remove_data = { self, data };

  g_return_if_fail (self);

  gcal_range_tree_traverse (self, G_IN_ORDER, remove_data_func, &remove_data);
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
 * @range: a #GcalRange
 *
 * Retrieves the data of every node within @range.
 *
 * Returns: (transfer full): a #GPtrArray. Unref with g_ptr_array_unref()
 * when finished.
 */
GPtrArray*
gcal_range_tree_get_data_at_range (GcalRangeTree *self,
                                   GcalRange     *range)
{
  GPtrArray *data;

  struct {
    GcalRange *range;
    GPtrArray **array;
  } gather_data = { range, &data };

  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (range, NULL);

  data = NULL;

  gcal_range_tree_traverse (self, G_IN_ORDER, gather_data_at_range, &gather_data);

  return data;
}

/**
 * gcal_range_tree_count_entries_at_range:
 * @self: a #GcalRangeTree
 * @range: a #GcalRange
 *
 * Counts the number of entries available in the given
 * range.
 *
 * Returns: the number of entries in the given range
 */
guint64
gcal_range_tree_count_entries_at_range (GcalRangeTree *self,
                                        GcalRange     *range)
{
  struct {
    GcalRange *range;
    guint64 counter;
  } gather_data = { range, 0 };

  g_return_val_if_fail (self, 0);
  g_return_val_if_fail (range, 0);

  gcal_range_tree_traverse (self, G_IN_ORDER, count_entries_at_range, &gather_data);

  return gather_data.counter;
}

/**
 * gcal_range_tree_print:
 * @self: a #GcalRangeTree
 *
 * Prints @self. This is only useful for debugging purposes. There
 * are no guarantees about the format, except that it's human-readable.
 */
void
gcal_range_tree_print (GcalRangeTree *self)
{
  g_autoptr (GString) string = NULL;

  g_return_if_fail (self);

  string = g_string_new ("");
  recursively_print_node_to_string (self->root, string, 0);

  g_print ("%s", string->str);
}
