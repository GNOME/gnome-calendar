/*
 * gcal-month-view-row.c
 *
 * Copyright 2023 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#define G_LOG_DOMAIN "GcalMonthViewRow"

#include "config.h"
#include "gcal-date-time-utils.h"
#include "gcal-debug.h"
#include "gcal-event-widget.h"
#include "gcal-month-cell.h"
#include "gcal-month-view-row.h"
#include "gcal-range-tree.h"
#include "gcal-utils.h"

#define SEPARATOR_OFFSET -1

typedef struct
{
  GtkWidget          *event_widget;
  guint8              length;
  guint8              cell;

  /* These are updated during allocation */
  gboolean            visible;
  gint                height;
} GcalEventBlock;

struct _GcalMonthViewRow
{
  GtkWidget           parent;

  GtkWidget          *day_cells[N_WEEKDAYS];

  GcalRange          *range;

  GListModel         *events;

  GHashTable         *layout_blocks;
  gboolean            layout_blocks_valid;
};

static void          on_event_widget_activated_cb                (GcalEventWidget    *widget,
                                                                  GcalMonthViewRow   *self);

static void          on_cell_activated_cb                        (GcalMonthCell    *cell,
                                                                  GcalMonthViewRow *self);

static gboolean      widget_tick_cb                              (GtkWidget          *widget,
                                                                  GdkFrameClock      *frame_clock,
                                                                  gpointer            user_data);

G_DEFINE_FINAL_TYPE (GcalMonthViewRow, gcal_month_view_row, GTK_TYPE_WIDGET)

enum
{
  EVENT_ACTIVATED,
  CELL_ACTIVATED,
  SHOW_OVERFLOW,
  N_SIGNALS,
};

static guint signals[N_SIGNALS] = { 0, };


/*
 * FocusEventData
 */

typedef struct
{
  GtkWidget *widget;
  graphene_rect_t rect;
  gint focal_x;
} FocusEventData;

static void
focus_event_data_free (gpointer data)
{
  FocusEventData *focus_event_data = data;

  g_free (focus_event_data);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (FocusEventData, focus_event_data_free)

static FocusEventData *
focus_event_data_new (GtkWidget             *widget,
                      const graphene_rect_t *rect,
                      gint                   focal_x)
{
  g_autoptr (FocusEventData) focus_event_data = NULL;

  focus_event_data = g_new0 (FocusEventData, 1);
  focus_event_data->widget = widget;
  focus_event_data->focal_x = focal_x;
  graphene_rect_init_from_rect (&focus_event_data->rect, rect);

  return g_steal_pointer (&focus_event_data);
}

static inline void
swap_focus_event_data (FocusEventData **dest,
                       FocusEventData  *src)
{
  g_assert (dest != NULL);

  g_clear_pointer (dest, focus_event_data_free);
  *dest = g_steal_pointer (&src);
}


/*
 * Auxiliary methods
 */

static FocusEventData *
create_focus_event_data (GcalMonthViewRow *self,
                         GtkWidget        *child)
{
  graphene_rect_t rect;
  gboolean is_rtl;
  gint focal_x;

  g_assert (GTK_IS_WIDGET (child));

  if (!gtk_widget_compute_bounds (child, GTK_WIDGET (self), &rect))
    g_assert_not_reached ();

  is_rtl = gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL;
  focal_x = rect.origin.x + (is_rtl ? rect.size.width : 0);

  return focus_event_data_new (child, &rect, focal_x);
}

static FocusEventData *
create_focus_event_data_candidate (GcalMonthViewRow *self,
                                   GtkWidget        *child)
{
  g_autoptr (FocusEventData) candidate = NULL;

  g_assert (GTK_IS_WIDGET (child));

  if (GCAL_IS_EVENT_WIDGET (child))
    candidate = create_focus_event_data (self, child);
  else if (GCAL_IS_MONTH_CELL (child) && gcal_month_cell_get_overflow (GCAL_MONTH_CELL (child)))
    candidate = create_focus_event_data (self, gcal_month_cell_get_overflow_button (GCAL_MONTH_CELL (child)));
  else
    return FALSE;

  if (!gtk_widget_get_child_visible (candidate->widget))
    return NULL;

  return g_steal_pointer (&candidate);
}

static GtkWidget *
find_first_event_widget (GcalMonthViewRow *self)
{
  g_autoptr (FocusEventData) candidate = NULL;
  g_autoptr (FocusEventData) nearest = NULL;
  GtkWidget *child;
  gboolean is_rtl;

  is_rtl = gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL;

  for (child = gtk_widget_get_first_child (GTK_WIDGET (self));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      g_autoptr (FocusEventData) aux = NULL;

      if (!(aux = create_focus_event_data_candidate (self, child)))
        continue;

      swap_focus_event_data (&candidate, g_steal_pointer (&aux));
      g_assert (candidate != NULL);

      if (nearest == NULL)
        swap_focus_event_data (&nearest, g_steal_pointer (&candidate));
      else if ((is_rtl && candidate->focal_x > nearest->focal_x) ||
               (!is_rtl && candidate->focal_x < nearest->focal_x))
        swap_focus_event_data (&nearest, g_steal_pointer (&candidate));
      else if (candidate->focal_x == nearest->focal_x && candidate->rect.origin.y < nearest->rect.origin.y)
        swap_focus_event_data (&nearest, g_steal_pointer (&candidate));
    }

  g_assert (nearest == NULL || GTK_IS_WIDGET (nearest->widget));

  GCAL_RETURN (nearest ? g_steal_pointer (&nearest->widget) : NULL);
}

static GtkWidget *
find_last_event_widget (GcalMonthViewRow *self)
{
  g_autoptr (FocusEventData) candidate = NULL;
  g_autoptr (FocusEventData) nearest = NULL;
  GtkWidget *child;
  gboolean is_rtl;

  is_rtl = gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL;

  for (child = gtk_widget_get_first_child (GTK_WIDGET (self));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      g_autoptr (FocusEventData) aux = NULL;

      if (!(aux = create_focus_event_data_candidate (self, child)))
        continue;

      swap_focus_event_data (&candidate, g_steal_pointer (&aux));
      g_assert (candidate != NULL);

      if (nearest == NULL)
        swap_focus_event_data (&nearest, g_steal_pointer (&candidate));
      else if ((is_rtl && candidate->focal_x < nearest->focal_x) ||
               (!is_rtl && candidate->focal_x > nearest->focal_x))
        swap_focus_event_data (&nearest, g_steal_pointer (&candidate));
      else if (candidate->focal_x == nearest->focal_x && candidate->rect.origin.y > nearest->rect.origin.y)
        swap_focus_event_data (&nearest, g_steal_pointer (&candidate));
    }

  g_assert (nearest == NULL || GTK_IS_WIDGET (nearest->widget));

  GCAL_RETURN (nearest ? g_steal_pointer (&nearest->widget) : NULL);
}

static GtkWidget *
find_nearest_vertical_event_widget (GcalMonthViewRow  *self,
                                    FocusEventData    *focused,
                                    GtkDirectionType   direction)
{
  g_autoptr (FocusEventData) candidate = NULL;
  g_autoptr (FocusEventData) nearest = NULL;
  GtkWidget *child;
  gboolean downward, upward, forward_or_backward;
  gboolean is_rtl;

  GCAL_ENTRY;

  downward = direction == GTK_DIR_DOWN || direction == GTK_DIR_TAB_FORWARD;
  upward = !downward;

  is_rtl = gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL;
  forward_or_backward = direction == GTK_DIR_TAB_FORWARD || direction == GTK_DIR_TAB_BACKWARD;

  for (child = gtk_widget_get_first_child (GTK_WIDGET (self));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      g_autoptr (FocusEventData) aux = NULL;

      if (!(aux = create_focus_event_data_candidate (self, child)))
        continue;

      swap_focus_event_data (&candidate, g_steal_pointer (&aux));
      g_assert (candidate != NULL);

      if (candidate->widget == focused->widget)
        continue;

      if (!graphene_rect_intersection (&GRAPHENE_RECT_INIT (focused->rect.origin.x, 0, focused->rect.size.width, 1),
                                       &GRAPHENE_RECT_INIT (candidate->rect.origin.x, 0, candidate->rect.size.width, 1),
                                       NULL))
        continue;

      if (forward_or_backward && focused->focal_x != candidate->focal_x)
        continue;

      if ((downward && candidate->rect.origin.y < focused->rect.origin.y) ||
          (upward && candidate->rect.origin.y > focused->rect.origin.y))
        continue;

      if (nearest == NULL)
        swap_focus_event_data (&nearest, g_steal_pointer (&candidate));
      else if ((downward && candidate->rect.origin.y < nearest->rect.origin.y) ||
               (upward && candidate->rect.origin.y > nearest->rect.origin.y))
        swap_focus_event_data (&nearest, g_steal_pointer (&candidate));
      else if (downward && candidate->rect.origin.y == nearest->rect.origin.y &&
               ((is_rtl && candidate->focal_x > nearest->focal_x) ||
                (!is_rtl && candidate->focal_x < nearest->focal_x)))
        swap_focus_event_data (&nearest, g_steal_pointer (&candidate));
    }

  g_assert (nearest == NULL || nearest->widget != focused->widget);

  GCAL_RETURN (nearest ? g_steal_pointer (&nearest->widget) : NULL);
}

static GtkWidget *
find_nearest_horizontal_event_widget (GcalMonthViewRow  *self,
                                      FocusEventData    *focused,
                                      GtkDirectionType   direction)
{
  g_autoptr (FocusEventData) candidate = NULL;
  g_autoptr (FocusEventData) nearest = NULL;
  GtkWidget *child;
  gboolean forward_or_backward;
  GtkDirectionType lateral;
  gboolean is_rtl;

  GCAL_ENTRY;

  g_assert (focused != NULL);

  is_rtl = gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL;
  forward_or_backward = direction == GTK_DIR_TAB_FORWARD || direction == GTK_DIR_TAB_BACKWARD;

  switch (direction)
    {
    case GTK_DIR_TAB_FORWARD:
      lateral = is_rtl ? GTK_DIR_LEFT : GTK_DIR_RIGHT;
      break;

    case GTK_DIR_TAB_BACKWARD:
      lateral = is_rtl ? GTK_DIR_RIGHT : GTK_DIR_LEFT;
      break;

    default:
      g_assert (direction == GTK_DIR_LEFT || direction == GTK_DIR_RIGHT);
      lateral = direction;
      break;
    }

  for (child = gtk_widget_get_first_child (GTK_WIDGET (self));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      g_autoptr (FocusEventData) aux = NULL;
      gboolean candidate_intersects_vertically;
      gboolean candidate_in_opposite_direction;

      if (!(aux = create_focus_event_data_candidate (self, child)))
        continue;

      swap_focus_event_data (&candidate, g_steal_pointer (&aux));
      g_assert (candidate != NULL);

      if (candidate->widget == focused->widget)
        continue;

      if (forward_or_backward &&
          ((lateral == GTK_DIR_LEFT && candidate->focal_x >= focused->focal_x) ||
           (lateral == GTK_DIR_RIGHT && candidate->focal_x <= focused->focal_x)))
        continue;

      candidate_in_opposite_direction = ((lateral == GTK_DIR_LEFT && candidate->focal_x > focused->focal_x) ||
                                         (lateral == GTK_DIR_RIGHT && candidate->focal_x < focused->focal_x));

      candidate_intersects_vertically =
          graphene_rect_intersection (&GRAPHENE_RECT_INIT (0, focused->rect.origin.y, 1, focused->rect.size.height),
                                      &GRAPHENE_RECT_INIT (0, candidate->rect.origin.y, 1, candidate->rect.size.height),
                                      NULL);

      if (!forward_or_backward && (!candidate_intersects_vertically || candidate_in_opposite_direction))
        continue;

      if (nearest == NULL)
        swap_focus_event_data (&nearest, g_steal_pointer (&candidate));
      else if ((lateral == GTK_DIR_LEFT && candidate->focal_x > nearest->focal_x) ||
               (lateral == GTK_DIR_RIGHT && candidate->focal_x < nearest->focal_x))
        swap_focus_event_data (&nearest, g_steal_pointer (&candidate));
      else if (candidate->focal_x == nearest->focal_x &&
               ((direction == GTK_DIR_TAB_FORWARD && candidate->rect.origin.y < nearest->rect.origin.y) ||
                (direction == GTK_DIR_TAB_BACKWARD && candidate->rect.origin.y > nearest->rect.origin.y)))
        swap_focus_event_data (&nearest, g_steal_pointer (&candidate));
    }

  g_assert (nearest == NULL || nearest->widget != focused->widget);

  GCAL_RETURN (nearest ? g_steal_pointer (&nearest->widget) : NULL);
}

static inline gboolean
is_overflow_focused (GtkWidget *focused)
{
  GtkWidget *cell, *overflow;

  if ((cell = gtk_widget_get_ancestor (focused, GCAL_TYPE_MONTH_CELL)))
    overflow = gcal_month_cell_get_overflow_button (GCAL_MONTH_CELL (cell));
  else
    return FALSE;

  return gtk_widget_is_focus (overflow);
}

static inline gboolean
focus_vertical_cell (GcalMonthViewRow *self,
                     GtkWidget        *focused_cell)
{
  graphene_point_t point;
  GtkWidget *row, *cell;
  gdouble cell_width;

  if (gtk_widget_is_ancestor (focused_cell, GTK_WIDGET (self)))
    return FALSE;

  cell_width = (gdouble) gtk_widget_get_width (GTK_WIDGET (self)) / (float) N_WEEKDAYS;
  row = gtk_widget_get_ancestor (focused_cell, GCAL_TYPE_MONTH_VIEW_ROW);

  g_assert (GTK_WIDGET (self) != row);

  if (!gtk_widget_compute_point (focused_cell, row, &GRAPHENE_POINT_INIT (cell_width / 2, 0), &point))
    g_assert_not_reached ();

  cell = gcal_month_view_row_get_cell_at_x (self, point.x);

  return gtk_widget_grab_focus (cell);
}

static gboolean
focus_horizontal_cell (GcalMonthViewRow *self,
                       GtkWidget        *focused_cell,
                       GtkDirectionType  direction)
{
  gint width;
  GtkWidget *new_cell;
  graphene_point_t point;

  width = gtk_widget_get_width (focused_cell);

  if (!gtk_widget_compute_point (focused_cell, GTK_WIDGET (self),
                                 &GRAPHENE_POINT_INIT (width / 2, 0),
                                 &point))
    g_assert_not_reached ();

  if (direction == GTK_DIR_LEFT)
    point.x -= width;
  else
    point.x += width;

  new_cell = gcal_month_view_row_get_cell_at_x (self, point.x);

  if (new_cell)
    return gtk_widget_grab_focus (new_cell);
  else
    return FALSE;
}

static void
layout_block_free (gpointer data)
{
  GcalEventBlock *block = data;

  if (!block)
    return;

  g_clear_pointer (&block->event_widget, gtk_widget_unparent);
  g_free (block);
}

static void
setup_child_widget (GcalMonthViewRow *self,
                    GtkWidget        *widget)
{
  gtk_widget_insert_after (widget, GTK_WIDGET (self), self->day_cells[N_WEEKDAYS - 1]);

  g_signal_connect_object (widget, "activate", G_CALLBACK (on_event_widget_activated_cb), self, 0);
}

static void
calculate_event_cells (GcalMonthViewRow *self,
                       GcalEvent        *event,
                       gint             *out_first_cell,
                       gint             *out_last_cell)
{
  g_autoptr (GDateTime) range_start = NULL;
  g_autoptr (GDateTime) start_date = NULL;
  gboolean all_day;
  gint first_cell;

  g_assert (out_first_cell != NULL || out_last_cell != NULL);

  all_day = gcal_event_get_all_day (event);
  range_start = gcal_range_get_start (self->range);
  start_date = gcal_event_get_date_start (event);
  start_date = all_day ? g_date_time_ref (start_date) : g_date_time_to_local (start_date);
  first_cell = MAX (gcal_date_time_compare_date (start_date, range_start), 0);

  /* Start date */
  if (out_first_cell)
    *out_first_cell = first_cell;

  /*
   * The logic for the end date is the same, except that we have to check
   * if the event is all day or not.
   */
  if (out_last_cell)
    {
      g_autoptr (GDateTime) range_start = NULL;
      g_autoptr (GDateTime) end_date = NULL;
      gint last_cell = N_WEEKDAYS - 1;

      range_start = gcal_range_get_start (self->range);
      end_date = gcal_event_get_date_end (event);

      if (all_day)
        {
          end_date = g_date_time_ref (end_date);
        }
      else
        {
          g_autoptr (GDateTime) local_end_date = g_date_time_to_local (end_date);
          end_date = g_date_time_add_seconds (local_end_date, -1);
        }

      last_cell = gcal_date_time_compare_date (end_date, range_start);

      if (all_day)
        last_cell--;

      *out_last_cell = CLAMP (last_cell, first_cell, N_WEEKDAYS - 1);
    }
}

static void
prepare_layout_blocks (GcalMonthViewRow *self,
                       guint             overflows[N_WEEKDAYS])
{
  GPtrArray *blocks_per_day[N_WEEKDAYS];
  gboolean cell_will_overflow[N_WEEKDAYS] = { FALSE, };
  guint available_height_without_overflow[N_WEEKDAYS] = { 0, };
  guint available_height_with_overflow[N_WEEKDAYS] = { 0, };
  guint weekday_heights[N_WEEKDAYS] = { 0, };
  guint n_events;

  g_assert (self->layout_blocks_valid);

  n_events = g_list_model_get_n_items (self->events);

  for (guint i = 0; i < N_WEEKDAYS; i++)
    {
      gint overflow_height;
      gint content_space;

      content_space = gcal_month_cell_get_content_space (GCAL_MONTH_CELL (self->day_cells[i]));
      overflow_height = gcal_month_cell_get_overflow_height (GCAL_MONTH_CELL (self->day_cells[i]));

      available_height_without_overflow[i] = content_space;
      available_height_with_overflow[i] = content_space - overflow_height;
      blocks_per_day[i] = g_ptr_array_sized_new (n_events);
    }

  /* Build a matrix with weekdays x blocks */
  for (guint i = 0; i < n_events; i++)
    {
      g_autoptr (GcalEvent) event = NULL;
      GPtrArray *blocks;

      event = g_list_model_get_item (self->events, i);

      blocks = g_hash_table_lookup (self->layout_blocks, event);
      g_assert (blocks != NULL);

      for (guint block_index = 0; block_index < blocks->len; block_index++)
        {
          GcalEventBlock *block;

          block = g_ptr_array_index (blocks, block_index);
          block->visible = TRUE;

          gtk_widget_measure (GTK_WIDGET (block->event_widget),
                              GTK_ORIENTATION_VERTICAL,
                              -1,
                              &block->height,
                              NULL, NULL, NULL);

          for (guint j = 0; j < block->length; j++)
            {
              guint cell = block->cell + j;

              g_ptr_array_add (blocks_per_day[cell], block);
              weekday_heights[cell] += block->height;
              cell_will_overflow[cell] |= weekday_heights[cell] > available_height_without_overflow[cell];
            }
        }
    }

  /* Figure out which blocks will be visible */
  for (guint cell = 0; cell < N_WEEKDAYS; cell++)
    {
      for (guint block_index = 0;
           block_index < blocks_per_day[cell]->len;
           block_index++)
        {
          GcalEventBlock *block = g_ptr_array_index (blocks_per_day[cell], block_index);

          if (block->cell != cell)
            continue;

          /* Will this block be visible in all cells it spans? */
          for (guint block_cell = cell; block_cell < cell + block->length; block_cell++)
            {
              gint available_height;

              if (cell_will_overflow[block_cell])
                available_height = available_height_with_overflow[block_cell];
              else
                available_height = available_height_without_overflow[block_cell];

              block->visible &= available_height > block->height;
            }

          for (guint block_cell = cell; block_cell < cell + block->length; block_cell++)
            {
              if (block->visible)
                {
                  available_height_with_overflow[block_cell] -= block->height;
                  available_height_without_overflow[block_cell] -= block->height;
                }
              else
                {
                  overflows[block_cell]++;
                }
            }
        }
    }

  for (guint i = 0; i < N_WEEKDAYS; i++)
    g_clear_pointer (&blocks_per_day[i], g_ptr_array_unref);
}

static void
recalculate_layout_blocks (GcalMonthViewRow *self)
{
  g_autoptr (GDateTime) range_start = NULL;
  guint events_at_weekday[N_WEEKDAYS] = { 0, };
  guint n_events;

  GCAL_ENTRY;

  g_assert (!self->layout_blocks_valid);

  range_start = gcal_range_get_start (self->range);
  n_events = g_list_model_get_n_items (self->events);

  g_hash_table_remove_all (self->layout_blocks);

  for (guint i = 0; i < n_events; i++)
    {
      g_autoptr (GPtrArray) blocks = NULL;
      g_autoptr (GcalEvent) event = NULL;
      GcalEventBlock *block = NULL;
      gint first_cell;
      gint last_cell;

      event = g_list_model_get_item (self->events, i);

      calculate_event_cells (self, event, &first_cell, &last_cell);
      g_assert (last_cell >= first_cell);

      blocks = g_ptr_array_new_full (last_cell - first_cell + 1, layout_block_free);

      for (gint cell = first_cell; cell <= last_cell; cell++)
        {
          events_at_weekday[cell]++;

          if (!block)
            {
              GtkWidget *event_widget;

              event_widget = gcal_event_widget_new (event);
              if (!gcal_event_get_all_day (event) && !gcal_event_is_multiday (event))
                gcal_event_widget_set_timestamp_policy (GCAL_EVENT_WIDGET (event_widget), GCAL_TIMESTAMP_POLICY_START);
              setup_child_widget (self, event_widget);

              block = g_new (GcalEventBlock, 1);
              block->event_widget = g_steal_pointer (&event_widget);
              block->length = 1;
              block->cell = cell;

              g_ptr_array_add (blocks, block);
            }
          else if (cell > first_cell && events_at_weekday[cell] != events_at_weekday[cell - 1])
            {
              GtkWidget *new_event_widget;

              new_event_widget = gcal_event_widget_clone (GCAL_EVENT_WIDGET (block->event_widget));
              setup_child_widget (self, new_event_widget);

              block = g_new (GcalEventBlock, 1);
              block->event_widget = g_steal_pointer (&new_event_widget);
              block->length = 1;
              block->cell = cell;

              g_ptr_array_add (blocks, block);
            }
          else
            {
              block->length++;
            }
        }

      /* Adjust slanted edges of multiday events */
      if (gcal_event_is_multiday (event))
        {
          g_autoptr (GDateTime) adjusted_range_start = NULL;

          adjusted_range_start = g_date_time_new (g_date_time_get_timezone (gcal_event_get_date_start (event)),
                                                  g_date_time_get_year (range_start),
                                                  g_date_time_get_month (range_start),
                                                  g_date_time_get_day_of_month (range_start),
                                                  0, 0, 0);


          for (guint j = 0; j < blocks->len; j++)
            {
              g_autoptr (GDateTime) block_start = NULL;
              g_autoptr (GDateTime) block_end = NULL;

              block = g_ptr_array_index (blocks, j);
              block_start = g_date_time_add_days (adjusted_range_start, block->cell);
              block_end = g_date_time_add_days (block_start, block->length); /* FIXME: use end date's timezone here */

              gcal_event_widget_set_date_start (GCAL_EVENT_WIDGET (block->event_widget), block_start);
              gcal_event_widget_set_date_end (GCAL_EVENT_WIDGET (block->event_widget), block_end);
            }
        }

      g_hash_table_insert (self->layout_blocks, event, g_steal_pointer (&blocks));
    }

  self->layout_blocks_valid = TRUE;

  GCAL_EXIT;
}

static void
invalidate_layout_blocks (GcalMonthViewRow *self)
{
  GCAL_ENTRY;

  if (!self->layout_blocks_valid)
    GCAL_RETURN ();

  if (gtk_widget_get_mapped (GTK_WIDGET (self)))
    {
      gtk_widget_add_tick_callback (GTK_WIDGET (self), widget_tick_cb, self, NULL);
      gtk_widget_queue_allocate (GTK_WIDGET (self));
    }

  self->layout_blocks_valid = FALSE;

  GCAL_EXIT;
}


/*
 * Callbacks
 */

static gboolean
filter_by_range_cb (GcalEvent        *event,
                    GcalRange        *row_range,
                    GcalMonthViewRow *self)
{
  g_assert (GCAL_IS_EVENT (event));
  g_assert (GCAL_IS_MONTH_VIEW_ROW (self));
  g_assert (self->range == row_range);

  if (!self->range)
    return FALSE;

  return gcal_event_overlaps (event, self->range);
}

static void
events_changed_cb (GListModel       *model,
                   guint             position,
                   guint             removed,
                   guint             added,
                   GcalMonthViewRow *self)
{
  GCAL_ENTRY;

  invalidate_layout_blocks (self);

  GCAL_EXIT;
}

static void
on_event_widget_activated_cb (GcalEventWidget  *widget,
                              GcalMonthViewRow *self)
{
  g_signal_emit (self, signals[EVENT_ACTIVATED], 0, widget);
}

static void
on_cell_activated_cb (GcalMonthCell    *cell,
                      GcalMonthViewRow *self)
{
  g_signal_emit (self, signals[CELL_ACTIVATED], 0, cell);
}

static void
on_month_cell_show_overflow_cb (GcalMonthCell    *cell,
                                GtkWidget        *button,
                                GcalMonthViewRow *self)
{
  g_signal_emit (self, signals[SHOW_OVERFLOW], 0, cell);
}

static gboolean
widget_tick_cb (GtkWidget     *widget,
                GdkFrameClock *frame_clock,
                gpointer       user_data)
{
  GcalMonthViewRow *self = (GcalMonthViewRow *) widget;

  GCAL_ENTRY;

  g_assert (GCAL_IS_MONTH_VIEW_ROW (self));

  recalculate_layout_blocks (self);

  GCAL_RETURN (G_SOURCE_REMOVE);
}


/*
 * GtkWidget overrides
 */

static void
gcal_month_view_row_map (GtkWidget *widget)
{
  GcalMonthViewRow *self = (GcalMonthViewRow *) widget;

  g_assert (GCAL_IS_MONTH_VIEW_ROW (self));

  GTK_WIDGET_CLASS (gcal_month_view_row_parent_class)->map (widget);

  if (!self->layout_blocks_valid)
    recalculate_layout_blocks (self);
}

static gboolean
gcal_month_view_row_focus (GtkWidget        *widget,
                           GtkDirectionType  direction)
{
  GcalMonthViewRow *self = GCAL_MONTH_VIEW_ROW (widget);
  g_autoptr (FocusEventData) data = NULL;
  GtkWidget *focused, *new_focus, *cell;
  gboolean overflow_focused;
  GtkRoot *root;

  root = gtk_widget_get_root (widget);
  focused = gtk_root_get_focus (root);

  if (!focused)
    return GTK_WIDGET_CLASS (gcal_month_view_row_parent_class)->focus (widget, direction);

  if (!gtk_widget_is_ancestor (focused, widget))
    {
      switch (direction)
        {
        case GTK_DIR_TAB_FORWARD:
          if ((new_focus = find_first_event_widget (self)))
            return gtk_widget_grab_focus (new_focus);
          else
            return FALSE;

        case GTK_DIR_TAB_BACKWARD:
          if ((new_focus = find_last_event_widget (self)))
            return gtk_widget_grab_focus (new_focus);
          else
            return FALSE;

        default:
          break;
        }
    }

  overflow_focused = is_overflow_focused (focused);
  cell = gtk_widget_get_ancestor (focused, GCAL_TYPE_MONTH_CELL);

  if (cell && !overflow_focused)
    {
      if (direction == GTK_DIR_UP || direction == GTK_DIR_DOWN)
        return focus_vertical_cell (self, cell);
      else if (direction == GTK_DIR_LEFT || direction == GTK_DIR_RIGHT)
        return focus_horizontal_cell (self, cell, direction);
    }

  if (GCAL_IS_EVENT_WIDGET (focused) || overflow_focused)
    {
      g_autoptr (FocusEventData) focus_event_data = create_focus_event_data (self, focused);

      switch (direction)
        {
        case GTK_DIR_TAB_FORWARD:
        case GTK_DIR_TAB_BACKWARD:
          if ((new_focus = find_nearest_vertical_event_widget (self, focus_event_data, direction)))
            return gtk_widget_grab_focus (new_focus);

          if ((new_focus = find_nearest_horizontal_event_widget (self, focus_event_data, direction)))
            return gtk_widget_grab_focus (new_focus);

          return FALSE;

        case GTK_DIR_UP:
        case GTK_DIR_DOWN:
          if ((new_focus = find_nearest_vertical_event_widget (self, focus_event_data, direction)))
            return gtk_widget_grab_focus (new_focus);
          else
            return FALSE;

        case GTK_DIR_LEFT:
        case GTK_DIR_RIGHT:
          if ((new_focus = find_nearest_horizontal_event_widget (self, focus_event_data, direction)))
            return gtk_widget_grab_focus (new_focus);
          else
            return FALSE;
        }
    }

  return GTK_WIDGET_CLASS (gcal_month_view_row_parent_class)->focus (widget, direction);
}

static void
gcal_month_view_row_measure (GtkWidget      *widget,
                             GtkOrientation  orientation,
                             gint            for_size,
                             gint           *out_minimum,
                             gint           *out_natural,
                             gint           *out_minimum_baseline,
                             gint           *out_natural_baseline)
{
  GcalMonthViewRow *self = GCAL_MONTH_VIEW_ROW (widget);
  gint minimum = 0;
  gint natural = 0;

  for (guint i = 0; i < N_WEEKDAYS; i++)
    {
      gint child_minimum;
      gint child_natural;

      gtk_widget_measure (self->day_cells[i],
                          orientation,
                          for_size,
                          &child_minimum,
                          &child_natural,
                          NULL,
                          NULL);


      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          minimum += child_minimum;
          natural += child_natural;
        }
      else
        {
          minimum = MAX(minimum, child_minimum);
          natural = MAX(natural, child_natural);
        }
    }

  if (out_minimum)
    *out_minimum = minimum;

  if (out_natural)
    *out_natural = natural;

  if (out_minimum_baseline)
    *out_minimum_baseline = -1;

  if (out_natural_baseline)
    *out_natural_baseline = -1;
}

static void
gcal_month_view_row_size_allocate (GtkWidget *widget,
                                   gint       width,
                                   gint       height,
                                   gint       baseline)
{
  GcalMonthViewRow *self = GCAL_MONTH_VIEW_ROW (widget);
  gboolean is_ltr;
  gdouble cell_width;

  is_ltr = gtk_widget_get_direction (widget) != GTK_TEXT_DIR_RTL;
  cell_width = width / (float) N_WEEKDAYS;

  /* Month cells */
  for (guint i = 0; i < N_WEEKDAYS; i++)
    {
      GtkAllocation allocation;
      GtkWidget *cell;

      allocation.x = round (cell_width * i);
      allocation.y = 0;
      allocation.width = round (cell_width * (i + 1)) - allocation.x;
      allocation.height = height;

      cell = is_ltr ? self->day_cells[i] : self->day_cells[N_WEEKDAYS - i - 1];
      gtk_widget_size_allocate (cell, &allocation, baseline);
    }

  /* Event widgets */
  if (self->layout_blocks_valid)
    {
      gdouble cell_y[N_WEEKDAYS];
      guint overflows[N_WEEKDAYS] = { 0, };
      guint n_events;

      prepare_layout_blocks (self, overflows);

      for (guint i = 0; i < N_WEEKDAYS; i++)
        cell_y[i] = gcal_month_cell_get_header_height (GCAL_MONTH_CELL (self->day_cells[i]));

      n_events = g_list_model_get_n_items (self->events);

      for (guint i = 0; i < n_events; i++)
        {
          g_autoptr (GcalEvent) event = NULL;
          GPtrArray *blocks;

          event = g_list_model_get_item (self->events, i);

          blocks = g_hash_table_lookup (self->layout_blocks, event);
          g_assert (blocks != NULL);

          for (guint block_index = 0; block_index < blocks->len; block_index++)
            {
              GcalEventBlock *block;
              GtkAllocation allocation;
              gint start_cell;
              gint end_cell;

              block = g_ptr_array_index (blocks, block_index);

              start_cell = is_ltr ? block->cell : N_WEEKDAYS - block->cell - block->length;
              end_cell = start_cell + block->length;

              allocation.x = round (start_cell * cell_width - (is_ltr ? 0 : SEPARATOR_OFFSET));
              allocation.y = cell_y[block->cell];
              allocation.width = round (end_cell * cell_width) - allocation.x + (is_ltr ? SEPARATOR_OFFSET : 0);
              allocation.height = block->height;

              gtk_widget_set_child_visible (block->event_widget, block->visible);
              gtk_widget_size_allocate (block->event_widget, &allocation, baseline);

              for (guint j = 0; j < block->length; j++)
                cell_y[block->cell + j] += block->height;
            }
        }

      for (guint i = 0; i < N_WEEKDAYS; i++)
        gcal_month_cell_set_overflow (GCAL_MONTH_CELL (self->day_cells[i]), overflows[i]);
    }
}


/*
 * GObject overrides
 */

static void
gcal_month_view_row_dispose (GObject *object)
{
  GcalMonthViewRow *self = (GcalMonthViewRow *)object;

  for (guint i = 0; i < N_WEEKDAYS; i++)
    g_clear_pointer (&self->day_cells[i], gtk_widget_unparent);

  g_clear_pointer (&self->layout_blocks, g_hash_table_destroy);
  g_clear_object (&self->events);

  G_OBJECT_CLASS (gcal_month_view_row_parent_class)->dispose (object);
}

static void
gcal_month_view_row_finalize (GObject *object)
{
  GcalMonthViewRow *self = (GcalMonthViewRow *)object;

  g_clear_pointer (&self->range, gcal_range_unref);

  G_OBJECT_CLASS (gcal_month_view_row_parent_class)->finalize (object);
}

static void
gcal_month_view_row_class_init (GcalMonthViewRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gcal_month_view_row_dispose;
  object_class->finalize = gcal_month_view_row_finalize;

  widget_class->map = gcal_month_view_row_map;
  widget_class->focus = gcal_month_view_row_focus;
  widget_class->measure = gcal_month_view_row_measure;
  widget_class->size_allocate = gcal_month_view_row_size_allocate;

  signals[EVENT_ACTIVATED] = g_signal_new ("event-activated",
                                           GCAL_TYPE_MONTH_VIEW_ROW,
                                           G_SIGNAL_RUN_FIRST,
                                           0,  NULL, NULL, NULL,
                                           G_TYPE_NONE,
                                           1,
                                           GCAL_TYPE_EVENT_WIDGET);

  signals[CELL_ACTIVATED] = g_signal_new ("cell-activated",
                                          GCAL_TYPE_MONTH_VIEW_ROW,
                                          G_SIGNAL_RUN_FIRST,
                                          0,  NULL, NULL, NULL,
                                          G_TYPE_NONE,
                                          1,
                                          GCAL_TYPE_MONTH_CELL);

  signals[SHOW_OVERFLOW] = g_signal_new ("show-overflow",
                                         GCAL_TYPE_MONTH_VIEW_ROW,
                                         G_SIGNAL_RUN_FIRST,
                                         0,  NULL, NULL, NULL,
                                         G_TYPE_NONE,
                                         1,
                                         GCAL_TYPE_MONTH_CELL);

  gtk_widget_class_set_css_name (widget_class, "monthviewrow");
}

static void
gcal_month_view_row_init (GcalMonthViewRow *self)
{
  self->events = G_LIST_MODEL (gtk_filter_list_model_new (NULL,
                                                          GTK_FILTER (gtk_custom_filter_new (event_in_range_func, self, NULL))));
  gtk_filter_list_model_set_incremental (GTK_FILTER_LIST_MODEL (self->events), TRUE);
  g_signal_connect (self->events, "items-changed", G_CALLBACK (events_changed_cb), self);

  self->layout_blocks = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, (GDestroyNotify) g_ptr_array_unref);
  self->layout_blocks_valid = TRUE;

  for (guint i = 0; i < N_WEEKDAYS; i++)
    {
      self->day_cells[i] = gcal_month_cell_new ();
      gcal_month_cell_set_overflow (GCAL_MONTH_CELL (self->day_cells[i]), 0);
      gtk_widget_set_parent (self->day_cells[i], GTK_WIDGET (self));
      g_signal_connect (self->day_cells[i], "show-overflow", G_CALLBACK (on_month_cell_show_overflow_cb), self);
      g_signal_connect (self->day_cells[i], "activate", G_CALLBACK (on_cell_activated_cb), self);
      gtk_accessible_update_relation (GTK_ACCESSIBLE (self->day_cells[i]),
                                      GTK_ACCESSIBLE_RELATION_COL_INDEX, i + 1,
                                      -1);
    }
}

GtkWidget *
gcal_month_view_row_new (void)
{
  return g_object_new (GCAL_TYPE_MONTH_VIEW_ROW, NULL);
}

GcalRange *
gcal_month_view_row_get_range (GcalMonthViewRow *self)
{
  g_return_val_if_fail (GCAL_IS_MONTH_VIEW_ROW (self), NULL);

  return self->range ? gcal_range_ref (self->range) : NULL;
}

void
gcal_month_view_row_set_range (GcalMonthViewRow *self,
                               GcalRange        *range)
{
  g_autoptr (GDateTime) start = NULL;
  GtkFilter *filter;

  g_return_if_fail (GCAL_IS_MONTH_VIEW_ROW (self));
  g_return_if_fail (range != NULL);

  GCAL_ENTRY;

  if (self->range && gcal_range_compare (self->range, range) == 0)
    GCAL_RETURN ();

  g_clear_pointer (&self->range, gcal_range_unref);
  self->range = gcal_range_ref (range);

  /* Refilter */
  filter = gtk_filter_list_model_get_filter (GTK_FILTER_LIST_MODEL (self->events));
  g_assert (GTK_IS_CUSTOM_FILTER (filter));
  gtk_filter_changed (filter, GTK_FILTER_CHANGE_DIFFERENT);

  start = gcal_range_get_start (range);
  for (guint i = 0; i < N_WEEKDAYS; i++)
    {
      g_autoptr (GDateTime) day = g_date_time_add_days (start, i);
      gcal_month_cell_set_date (GCAL_MONTH_CELL (self->day_cells[i]), day);
    }

  GCAL_EXIT;
}

void
gcal_month_view_row_set_model (GcalMonthViewRow *self,
                               GListModel       *model)
{
  g_return_if_fail (GCAL_IS_MONTH_VIEW_ROW (self));

  gtk_filter_list_model_set_model (GTK_FILTER_LIST_MODEL (self->events), model);
  invalidate_layout_blocks (self);
}

GList*
gcal_month_view_row_get_children_by_uuid (GcalMonthViewRow      *self,
                                          GcalRecurrenceModType  mod,
                                          const gchar           *uuid)
{
  g_assert (GCAL_IS_MONTH_VIEW_ROW (self));

  return filter_children_by_uid_and_modtype (GTK_WIDGET (self), mod, uuid);
}

GtkWidget*
gcal_month_view_row_get_cell_at_x (GcalMonthViewRow *self,
                                   gdouble           x)
{
  gint column;
  gint width;

  g_assert (GCAL_IS_MONTH_VIEW_ROW (self));

  width = gtk_widget_get_width (GTK_WIDGET (self));
  if (x < 0.0 || x > width)
    return NULL;

  column = floor ((float) N_WEEKDAYS * x / (gdouble) width);

  if (gtk_widget_get_direction (GTK_WIDGET (self)) != GTK_TEXT_DIR_RTL)
    return self->day_cells[column];
  else
    return self->day_cells[N_WEEKDAYS - column - 1];
}

void
gcal_month_view_row_update_selection (GcalMonthViewRow *self,
                                      GcalRange        *selection_range)
{
  g_assert (GCAL_IS_MONTH_VIEW_ROW (self));

  for (guint i = 0; i < N_WEEKDAYS; i++)
    {
      GcalMonthCell *month_cell;
      GDateTime *cell_date;
      gboolean selected;

      month_cell = GCAL_MONTH_CELL (self->day_cells[i]);
      cell_date = gcal_month_cell_get_date (month_cell);
      selected = selection_range && gcal_range_contains_datetime (selection_range, cell_date);

      gcal_month_cell_set_selected (month_cell, selected);
    }
}

gboolean
gcal_month_view_row_focus_adjacent_cell (GcalMonthViewRow *self,
                                         GtkWidget        *widget)
{
  GtkWidget *cell;
  g_autoptr (FocusEventData) data = NULL;

  g_assert (GCAL_IS_MONTH_VIEW_ROW (self));
  g_assert (gtk_widget_is_ancestor (widget, GTK_WIDGET (self)));

  data = create_focus_event_data (self, widget);

  cell = gcal_month_view_row_get_cell_at_x (self, data->focal_x);

  return gtk_widget_grab_focus (cell);
}
