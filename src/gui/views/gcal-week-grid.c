/* gcal-week-grid.c
 *
 * Copyright (C) 2016-2020 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *                    Vamsi Krishna Gollapudi <pandu.sonu@yahoo.com>
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

#define G_LOG_DOMAIN "GcalWeekGrid"

#include "gcal-application.h"
#include "gcal-clock.h"
#include "gcal-context.h"
#include "gcal-debug.h"
#include "gcal-gui-utils.h"
#include "gcal-week-grid.h"
#include "gcal-week-view.h"
#include "gcal-week-view-common.h"
#include "gcal-utils.h"
#include "gcal-view-private.h"
#include "gcal-event-widget.h"
#include "gcal-event-list.h"
#include "gcal-range-tree.h"

#include <glib/gi18n.h>
#include <string.h>
#include <math.h>

typedef struct
{
  GcalWeekGrid       *self;
  GcalEvent          *event;
  gint                drop_cell;
} DropData;

struct _GcalWeekGrid
{
  GtkWidget           parent;

  GtkWidget          *hours_sidebar;
  GtkWidget          *now_strip;
  GtkEventController *motion_controller;

  GDateTime          *active_date;

  GPtrArray          *layout_blocks;
  GtkFilterListModel *filter_model;
  GtkSortListModel   *sort_model;

  gboolean            layout_blocks_valid;

  /*
   * These fields are "cells" rather than minutes. Each cell
   * correspond to 30 minutes.
   */
  struct {
    gint              start;
    gint              end;
    GtkWidget        *widget;
  } selection;

  struct {
    gint              cell;
    GtkWidget        *widget;
    gint              event_minutes;
  } dnd;
};

static void     on_event_widget_activated_cb (GcalEventWidget *widget,
                                              GcalWeekGrid    *self);
static gboolean widget_tick_cb               (GtkWidget       *widget,
                                              GdkFrameClock   *frame_clock,
                                              gpointer         user_data);

G_DEFINE_TYPE (GcalWeekGrid, gcal_week_grid, GTK_TYPE_WIDGET);


enum
{
  EVENT_ACTIVATED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };


/*
 * ChildData
 */

typedef struct
{
  GtkWidget          *widget;
  GcalEvent          *event;
  uint32_t            n_columns;
} ChildData;

static ChildData*
child_data_new (GtkWidget *widget,
                GcalEvent *event)
{
  ChildData *data;

  data = g_new (ChildData, 1);
  data->widget = widget;
  data->event = g_object_ref (event);
  data->n_columns = 1;

  return data;
}

static void
child_data_free (gpointer data)
{
  ChildData *child_data = data;

  if (!child_data)
    return;

  g_clear_pointer (&child_data->widget, gtk_widget_unparent);
  g_clear_object (&child_data->event);
  g_free (child_data);
}


/*
 * LayoutBlock
 */

typedef struct
{
  GcalRange *range;
  GPtrArray *columns; /* GcalRangeTree<ChildData> */
} LayoutBlock;

static void
layout_block_free (gpointer data)
{
  LayoutBlock *layout_block = data;

  if (!layout_block)
    return;

  g_clear_pointer (&layout_block->range, gcal_range_unref);
  g_clear_pointer (&layout_block->columns, g_ptr_array_unref);
  g_free (layout_block);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (LayoutBlock, layout_block_free)

static ChildData *
layout_block_add_event (LayoutBlock *layout_block,
                        GcalEvent   *event)
{
  GcalRange *event_range = NULL;
  ChildData *child_data = NULL;
  size_t column = 0;

  g_assert (layout_block != NULL);
  g_assert (GCAL_IS_EVENT (event));

  event_range = gcal_event_get_range (event);

  do
    {
      GcalRangeTree *column_data = NULL;

      if (column >= layout_block->columns->len)
        {
          column_data = gcal_range_tree_new_with_free_func (child_data_free);
          g_ptr_array_insert (layout_block->columns, column, column_data);
        }
      else
        {
          column_data = g_ptr_array_index (layout_block->columns, column);
        }

      g_assert (column_data != NULL);

      if (!gcal_range_tree_has_entries_at_range (column_data, event_range))
        {
          g_autoptr (GcalRange) new_block_range = NULL;

          child_data = child_data_new (NULL, event);
          gcal_range_tree_add_range (column_data, event_range, child_data);

          new_block_range = gcal_range_union (layout_block->range, event_range);
          g_clear_pointer (&layout_block->range, gcal_range_unref);
          layout_block->range = g_steal_pointer (&new_block_range);

          break;
        }

      column++;
    }
  while (TRUE);

  g_assert (child_data != NULL);

  return child_data;
}


/*
 * Auxiliary methods
 */

static void
add_event_to_block (GcalWeekGrid *self,
                    LayoutBlock  *layout_block,
                    GcalEvent    *event)
{
  ChildData *child_data;

  g_assert (GCAL_IS_WEEK_GRID (self));
  g_assert (layout_block != NULL);
  g_assert (GCAL_IS_EVENT (event));

  child_data = layout_block_add_event (layout_block, event);

  child_data->widget = g_object_new (GCAL_TYPE_EVENT_WIDGET,
                                     "event", event,
                                     "orientation", GTK_ORIENTATION_VERTICAL,
                                     "timestamp-policy", GCAL_TIMESTAMP_POLICY_START,
                                     NULL);

  g_signal_connect (child_data->widget, "activate", G_CALLBACK (on_event_widget_activated_cb), self);

  gtk_widget_set_parent (child_data->widget, GTK_WIDGET (self));
}

static void
expand_event_widgets_in_blocks (GcalWeekGrid *self)
{

  for (size_t block_index = 0; block_index < self->layout_blocks->len; block_index++)
    {
      g_autoptr (GDateTime) block_range_start = NULL;
      LayoutBlock *layout_block = NULL;

      layout_block = g_ptr_array_index (self->layout_blocks, block_index);
      g_assert (layout_block != NULL);
      g_assert (layout_block->columns != NULL);

      for (size_t column_index = 0; column_index < layout_block->columns->len; column_index++)
        {
          g_autoptr (GPtrArray) children_data = NULL;
          GcalRangeTree *block_column;

          block_column = g_ptr_array_index (layout_block->columns, column_index);
          g_assert (block_column != NULL);

          children_data = gcal_range_tree_get_all_data (block_column);
          g_assert (children_data != NULL);

          for (size_t child_data_index = 0; child_data_index < children_data->len; child_data_index++)
            {
              GcalRange *event_range;
              ChildData *child_data;

              child_data = g_ptr_array_index (children_data, child_data_index);
              event_range = gcal_event_get_range (child_data->event);

              for (size_t next_column_index = column_index + 1;
                   next_column_index < layout_block->columns->len;
                   next_column_index++)
                {
                  GcalRangeTree *next_column = g_ptr_array_index (layout_block->columns, next_column_index);

                  if (gcal_range_tree_has_entries_at_range (next_column, event_range))
                    break;

                  child_data->n_columns++;
                }
            }
        }
    }
}

static void
recalculate_layout_blocks (GcalWeekGrid *self)
{
  g_autoptr (GcalRangeTree) blocks_by_range = NULL;
  g_autoptr (GDateTime) week_start = NULL;
  g_autoptr (GDateTime) week_end = NULL;
  g_autoptr (GcalRange) range = NULL;
  size_t n_events = 0;

  GCAL_ENTRY;

  g_assert (GCAL_IS_WEEK_GRID (self));
  g_assert (self->layout_blocks != NULL);

  blocks_by_range = gcal_range_tree_new ();

  week_start = gcal_date_time_get_start_of_week (self->active_date);
  week_end = g_date_time_add_weeks (week_start, 1);
  range = gcal_range_new (week_start, week_end, GCAL_RANGE_DEFAULT);

  /* Remove all blocks */
  g_ptr_array_set_size (self->layout_blocks, 0);

  /* First pass: create all layout blocks, all event widgets will have 1 column of width */
  n_events = g_list_model_get_n_items (G_LIST_MODEL (self->sort_model));
  for (size_t i = 0; i < n_events; i++)
    {
      g_autoptr (GPtrArray) blocks_at_range = NULL;
      g_autoptr (GcalRange) old_block_range = NULL;
      g_autoptr (GcalRange) new_block_range = NULL;
      g_autoptr (GcalEvent) event = NULL;
      LayoutBlock *layout_block = NULL;
      GcalRange *event_range;

      event = g_list_model_get_item (G_LIST_MODEL (self->sort_model), i);
      g_assert (GCAL_IS_EVENT (event));

      if (!gcal_event_overlaps (event, range))
        continue;

      event_range = gcal_event_get_range (event);

      blocks_at_range = gcal_range_tree_get_data_at_range (blocks_by_range, event_range);
      g_assert (blocks_at_range == NULL || blocks_at_range->len == 1);

      if (!blocks_at_range)
        {
          layout_block = g_new0 (LayoutBlock, 1);
          layout_block->range = gcal_range_ref (event_range);
          layout_block->columns = g_ptr_array_new_with_free_func ((GDestroyNotify) gcal_range_tree_unref);

          gcal_range_tree_add_range (blocks_by_range, layout_block->range, layout_block);
          g_ptr_array_add (self->layout_blocks, layout_block);
        }
      else
        {
          layout_block = g_ptr_array_index (blocks_at_range, 0);
        }
      g_assert (layout_block != NULL);

      old_block_range = gcal_range_ref (layout_block->range);

      add_event_to_block (self, layout_block, event);

      new_block_range = gcal_range_ref (layout_block->range);

      if (gcal_range_calculate_overlap (old_block_range, new_block_range, NULL) != GCAL_RANGE_EQUAL)
        {
          gcal_range_tree_remove_range (blocks_by_range, old_block_range, layout_block);
          gcal_range_tree_add_range (blocks_by_range, new_block_range, layout_block);
        }
    }

  /* Second pass: expand event widgets to fill in empty space */
  expand_event_widgets_in_blocks (self);

  self->layout_blocks_valid = TRUE;

  GCAL_EXIT;
}

static void
invalidate_layout_blocks (GcalWeekGrid *self)
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
widget_tick_cb (GtkWidget     *widget,
                GdkFrameClock *frame_clock,
                gpointer       user_data)
{
  GcalWeekGrid *self = (GcalWeekGrid *) widget;

  GCAL_ENTRY;

  g_assert (GCAL_IS_WEEK_GRID (self));

  recalculate_layout_blocks (self);

  GCAL_RETURN (G_SOURCE_REMOVE);
}

static int
compare_events_cb (gconstpointer a,
                   gconstpointer b,
                   gpointer      user_data)
{
  GDateTime *event_a_start;
  GDateTime *event_b_start;
  GDateTime *event_a_end;
  GDateTime *event_b_end;
  GcalEvent *event_a;
  GcalEvent *event_b;
  int result;

  event_a = (GcalEvent *) a;
  event_b = (GcalEvent *) b;

  event_a_start = gcal_event_get_date_start (event_a);
  event_b_start = gcal_event_get_date_start (event_b);
  result = g_date_time_compare (event_a_start, event_b_start);
  if (result != 0)
    return result;

  event_a_end = gcal_event_get_date_end (event_a);
  event_b_end = gcal_event_get_date_end (event_b);
  result = g_date_time_compare (event_a_end, event_b_end);
  if (result != 0)
    return -result;

  return 0;
}

static gboolean
event_is_timed_single_day_func (gpointer item,
                                gpointer user_data)
{
  GcalEvent *event = (GcalEvent *) item;

  g_assert (GCAL_IS_EVENT (event));

  return !gcal_event_get_all_day (event) && !gcal_event_is_multiday (event);
}

static void
events_changed_cb (GListModel   *model,
                   unsigned int  position,
                   unsigned int  removed,
                   unsigned int  added,
                   GcalWeekGrid *self)
{
  invalidate_layout_blocks (self);
}

static void
on_click_gesture_pressed_cb (GtkGestureClick *click_gesture,
                             gint             n_press,
                             gdouble          x,
                             gdouble          y,
                             GcalWeekGrid    *self)
{
  gdouble minute_height;
  gint column_width;
  gint column;
  gint minute;

  g_assert (self->selection.start == -1);
  g_assert (self->selection.end == -1);

  minute_height = (gdouble) gtk_widget_get_height (GTK_WIDGET (self)) / MINUTES_PER_DAY;
  column_width = floor (gtk_widget_get_width (GTK_WIDGET (self)) / N_WEEKDAYS);
  column = (gint) x / column_width;
  minute = y / minute_height;
  minute = minute - (minute % 30);

  self->selection.start = (column * MINUTES_PER_DAY + minute) / 30;
  self->selection.end = self->selection.start;

  gtk_widget_set_visible (self->selection.widget, TRUE);

  gtk_event_controller_set_propagation_phase (self->motion_controller, GTK_PHASE_BUBBLE);
}

static void
on_event_widget_activated_cb (GcalEventWidget *widget,
                              GcalWeekGrid    *self)
{
  g_signal_emit (self, signals[EVENT_ACTIVATED], 0, widget);
}

static void
on_motion_controller_motion_cb (GtkEventControllerMotion *motion_controller,
                                gdouble                   x,
                                gdouble                   y,
                                GcalWeekGrid             *self)
{
  gdouble minute_height;
  gint column;
  gint minute;

  minute_height = (gdouble) gtk_widget_get_height (GTK_WIDGET (self)) / MINUTES_PER_DAY;
  column = self->selection.start * 30 / MINUTES_PER_DAY;
  minute = y / minute_height;
  minute = minute - (minute % 30);

  self->selection.end = (column * MINUTES_PER_DAY + minute) / 30;

  gtk_widget_queue_allocate (GTK_WIDGET (self));
}

static void
on_click_gesture_released_cb (GtkGestureClick *click_gesture,
                              gint             n_press,
                              gdouble          x,
                              gdouble          y,
                              GcalWeekGrid    *self)
{
  g_autoptr (GDateTime) week_start = NULL;
  g_autoptr (GcalRange) range = NULL;
  g_autoptr (GDateTime) start = NULL;
  g_autoptr (GDateTime) end = NULL;
  graphene_point_t out;
  GtkWidget *weekview;
  gboolean ltr;
  gdouble minute_height;
  gdouble local_x;
  gdouble local_y;
  gint column;
  gint minute;
  gint start_cell;
  gint end_cell;

  ltr = gtk_widget_get_direction (GTK_WIDGET (self)) != GTK_TEXT_DIR_RTL;

  minute_height = (gdouble) gtk_widget_get_height (GTK_WIDGET (self)) / MINUTES_PER_DAY;
  column = self->selection.start * 30 / MINUTES_PER_DAY;
  minute = y / minute_height;
  minute = minute - (minute % 30);

  self->selection.end = (column * MINUTES_PER_DAY + minute) / 30;

  start_cell = self->selection.start;
  end_cell = self->selection.end;

  if (start_cell > end_cell)
    {
      start_cell = start_cell + end_cell;
      end_cell = start_cell - end_cell;
      start_cell = start_cell - end_cell;
    }

  gtk_widget_queue_allocate (GTK_WIDGET (self));

  /* Fake the week view's event so we can control the X and Y values */
  weekview = gtk_widget_get_ancestor (GTK_WIDGET (self), GCAL_TYPE_WEEK_VIEW);
  week_start = gcal_date_time_get_start_of_week (self->active_date);

  if (ltr)
    {
      start = gcal_date_time_add_floating_minutes (week_start, start_cell * 30);
      end = gcal_date_time_add_floating_minutes (week_start, (end_cell + 1) * 30);
    }
  else
    {
      guint rtl_start_cell, rtl_end_cell, rtl_column;

      /* Fix the minute */
      rtl_column = N_WEEKDAYS - 1 - column;
      rtl_start_cell = start_cell + (rtl_column - column) * 48;
      rtl_end_cell = (rtl_column * MINUTES_PER_DAY + minute) / 30;

      start = gcal_date_time_add_floating_minutes (week_start, rtl_start_cell * 30);
      end = gcal_date_time_add_floating_minutes (week_start, (rtl_end_cell + 1) * 30);
    }

  local_x = round ((column + 0.5) * (gtk_widget_get_width (GTK_WIDGET (self)) / (float) N_WEEKDAYS));
  local_y = (minute + 15) * minute_height;

  if (!gtk_widget_compute_point (GTK_WIDGET (self),
                                 weekview,
                                 &GRAPHENE_POINT_INIT (local_x, local_y),
                                 &out))
    g_assert_not_reached ();

  range = gcal_range_new (start, end, GCAL_RANGE_DEFAULT);
  gcal_view_create_event (GCAL_VIEW (weekview), range, out.x, out.y);

  gtk_event_controller_set_propagation_phase (self->motion_controller, GTK_PHASE_NONE);
}

static gint
get_dnd_cell (GcalWeekGrid *self,
              gdouble       x,
              gdouble       y)
{
  gdouble column_width, cell_height;
  gint column, row;

  column_width = gtk_widget_get_width (GTK_WIDGET (self)) / (float) N_WEEKDAYS;
  cell_height = gtk_widget_get_height (GTK_WIDGET (self)) / 48.0;
  column = floor (x / column_width);
  row = y / cell_height;

  return column * 48 + row;
}

static void
move_event_to_cell (GcalWeekGrid          *self,
                    GcalEvent             *event,
                    guint                  cell,
                    GcalRecurrenceModType  mod_type)
{
  GcalContext *context = gcal_application_get_context (GCAL_DEFAULT_APPLICATION);
  g_autoptr (GDateTime) week_start = NULL;
  g_autoptr (GDateTime) dnd_date = NULL;
  g_autoptr (GDateTime) new_end = NULL;
  g_autoptr (GcalEvent) changed_event = NULL;
  GTimeSpan timespan = 0;

  /* RTL languages swap the drop cell column */
  if (gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL)
    {
      gint column, row;

      column = cell / (MINUTES_PER_DAY / 30);
      row = cell - column * 48;

      cell = (N_WEEKDAYS - 1 - column) * 48 + row;
    }

  changed_event = gcal_event_new_from_event (event);
  week_start = gcal_date_time_get_start_of_week (self->active_date);
  dnd_date = gcal_date_time_add_floating_minutes (week_start, cell * 30);

  /*
   * Calculate the diff between the dropped cell and the event's start date,
   * so we can update the end date accordingly.
   */
  timespan = g_date_time_difference (gcal_event_get_date_end (changed_event), gcal_event_get_date_start (changed_event));

  /*
   * Set the event's start and end dates. Since the event may have a
   * NULL end date, so we have to check it here
   */
  gcal_event_set_all_day (changed_event, FALSE);
  gcal_event_set_date_start (changed_event, dnd_date);

  /* Setup the new end date */
  new_end = g_date_time_add (dnd_date, timespan);
  gcal_event_set_date_end (changed_event, new_end);

  /* Commit the changes */
  gcal_manager_update_event (gcal_context_get_manager (context), changed_event, mod_type);
}

static void
on_ask_recurrence_response_cb (GcalEvent             *event,
                               GcalRecurrenceModType  mod_type,
                               gpointer               user_data)
{
  DropData *data = user_data;

  if (mod_type != GCAL_RECURRENCE_MOD_NONE)
    move_event_to_cell (data->self, data->event, data->drop_cell, mod_type);

  g_clear_object (&data->event);
  g_clear_pointer (&data, g_free);
}

static gboolean
on_drop_target_drop_cb (GtkDropTarget *drop_target,
                        const GValue  *value,
                        gdouble        x,
                        gdouble        y,
                        GcalWeekGrid  *self)
{
  GcalEventWidget *event_widget;
  GcalEvent *event;
  gint cell;

  GCAL_ENTRY;

  if (!G_VALUE_HOLDS (value, GCAL_TYPE_EVENT_WIDGET))
    GCAL_RETURN (FALSE);

  cell = get_dnd_cell (self, x, y);
  event_widget = g_value_get_object (value);
  event = gcal_event_widget_get_event (event_widget);

  if (gcal_event_has_recurrence (event))
    {
      DropData *data;

      data = g_new0 (DropData, 1);
      data->self = self;
      data->event = g_object_ref (event);
      data->drop_cell = cell;

      gcal_utils_ask_recurrence_modification_type (GTK_WIDGET (self),
                                                   event,
                                                   FALSE,
                                                   on_ask_recurrence_response_cb,
                                                   data);
    }
  else
    {
      move_event_to_cell (self, event, cell, GCAL_RECURRENCE_MOD_THIS_ONLY);
    }

  GCAL_RETURN (TRUE);
}

static GdkDragAction
on_drop_target_enter_cb (GtkDropTarget *drop_target,
                         gdouble        x,
                         gdouble        y,
                         GcalWeekGrid  *self)
{

  GCAL_ENTRY;

  self->dnd.cell = get_dnd_cell (self, x, y);
  gtk_widget_set_visible (self->dnd.widget, TRUE);

  GCAL_RETURN (GDK_ACTION_COPY);
}

static void
on_drop_target_leave_cb (GtkDropTarget *drop_target,
                         GcalWeekGrid  *self)
{
  GCAL_ENTRY;

  self->dnd.cell = -1;
  gtk_widget_set_visible (self->dnd.widget, FALSE);

  GCAL_EXIT;
}

static GdkDragAction
on_drop_target_motion_cb (GtkDropTarget *drop_target,
                          gdouble        x,
                          gdouble        y,
                          GcalWeekGrid  *self)
{
  GCAL_ENTRY;
  GcalEventWidget *event_widget;
  const GValue *value;
  GDateTime *start, *end;

  self->dnd.cell = get_dnd_cell (self, x, y);
  value = gtk_drop_target_get_value (drop_target);
  event_widget = g_value_get_object (value);

  start = gcal_event_widget_get_date_start (event_widget);
  end = gcal_event_widget_get_date_end (event_widget);

  self->dnd.event_minutes = g_date_time_difference (end, start) / G_TIME_SPAN_MINUTE;

  gtk_widget_queue_allocate (GTK_WIDGET (self));

  GCAL_RETURN (GDK_ACTION_COPY);
}

static void
gcal_week_grid_dispose (GObject *object)
{
  GcalWeekGrid *self = GCAL_WEEK_GRID (object);

  g_clear_pointer (&self->layout_blocks, g_ptr_array_unref);
  g_clear_pointer (&self->dnd.widget, gtk_widget_unparent);
  g_clear_pointer (&self->now_strip, gtk_widget_unparent);
  g_clear_pointer (&self->selection.widget, gtk_widget_unparent);

  G_OBJECT_CLASS (gcal_week_grid_parent_class)->dispose (object);
}

static void
gcal_week_grid_finalize (GObject *object)
{
  GcalWeekGrid *self = GCAL_WEEK_GRID (object);

  gcal_clear_date_time (&self->active_date);
  g_clear_object (&self->filter_model);
  g_clear_object (&self->sort_model);

  G_OBJECT_CLASS (gcal_week_grid_parent_class)->finalize (object);
}

static void
gcal_week_grid_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
}

static void
gcal_week_grid_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
}

static inline gint
get_today_column (GcalWeekGrid *self)
{
  g_autoptr(GDateTime) week_start = NULL;
  g_autoptr(GDateTime) today = NULL;
  gint days_diff;

  today = g_date_time_new_now_local ();
  week_start = gcal_date_time_get_start_of_week (self->active_date);
  days_diff = g_date_time_difference (today, week_start) / G_TIME_SPAN_DAY;

  /* Today is out of range */
  if (g_date_time_compare (today, week_start) < 0 || days_diff > N_WEEKDAYS)
    return -1;

  return days_diff;
}

static void
gcal_week_grid_map (GtkWidget *widget)
{
  GcalWeekGrid *self = (GcalWeekGrid *) widget;

  g_assert (GCAL_IS_WEEK_GRID (self));

  GTK_WIDGET_CLASS (gcal_week_grid_parent_class)->map (widget);

  if (!self->layout_blocks_valid)
    recalculate_layout_blocks (self);
}

static void
gcal_week_grid_measure (GtkWidget      *widget,
                        GtkOrientation  orientation,
                        gint            for_size,
                        gint           *minimum,
                        gint           *natural,
                        gint           *minimum_baseline,
                        gint           *natural_baseline)
{
  if (minimum)
    *minimum = 1;
  if (natural)
    *natural = 1;
}

static void
gcal_week_grid_snapshot (GtkWidget   *widget,
                         GtkSnapshot *snapshot)
{
  GcalWeekGrid *self;
  gint width, height;

  self = GCAL_WEEK_GRID (widget);
  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_height (widget);

  gcal_week_view_common_snapshot_hour_lines (widget, snapshot, GTK_ORIENTATION_HORIZONTAL, width, height);
  gcal_week_view_common_snapshot_hour_lines (widget, snapshot, GTK_ORIENTATION_VERTICAL, width, height);

  /* First, draw the selection */
  gtk_widget_snapshot_child (widget, self->selection.widget, snapshot);
  gtk_widget_snapshot_child (widget, self->dnd.widget, snapshot);

  /* Then, event widgets */
  for (GtkWidget *child = gtk_widget_get_first_child (widget);
       child;
       child = gtk_widget_get_next_sibling (child))
    {
      if (child == self->selection.widget ||
          child == self->dnd.widget ||
          child == self->now_strip)
        {
          continue;
        }

      gtk_widget_snapshot_child (widget, child, snapshot);
    }

  gtk_widget_snapshot_child (widget, self->now_strip, snapshot);
}

static void
gcal_week_grid_size_allocate (GtkWidget *widget,
                              gint       width,
                              gint       height,
                              gint       baseline)
{
  GcalWeekGrid *self = GCAL_WEEK_GRID (widget);
  g_autoptr (GDateTime) week_start = NULL;
  gboolean ltr;
  gdouble minutes_height;
  gdouble column_width;
  gint today_column;

  ltr = gtk_widget_get_direction (widget) != GTK_TEXT_DIR_RTL;

  /* Preliminary calculations */
  minutes_height = (gdouble) height / MINUTES_PER_DAY;
  column_width = (gdouble) width / (float) N_WEEKDAYS;

  /* Selection */
  if (gtk_widget_should_layout (self->selection.widget))
    {
      gint selection_height;
      gint start_row;
      gint end_row;
      gint column;
      gint start;
      gint end;
      gint x;
      gint y;

      g_assert (self->selection.start != -1);
      g_assert (self->selection.end != -1);

      start = self->selection.start;
      end = self->selection.end;

      /* Swap cells if needed */
      if (start > end)
        {
          start = start + end;
          end = start - end;
          start = start - end;
        }

      column = (int) floor (start * 30 / (double) MINUTES_PER_DAY);

#define Y_POSITION(row_) ((gint) round (((row_) * 30) * minutes_height))

      start_row = start - column * 48;
      end_row = (end + 1) - column * 48;

      x = column * column_width;
      y = Y_POSITION (start_row);
      selection_height = Y_POSITION (end_row) - y;

#undef Y_POSITION

      gtk_widget_size_allocate (self->selection.widget,
                                &(GtkAllocation) {
                                  .x = x,
                                  .y = y,
                                  .width = column_width - 1,
                                  .height = selection_height,
                                },
                                baseline);
    }

  if (gtk_widget_should_layout (self->dnd.widget))
    {
      gdouble cell_height, event_height;
      gint column, row;

      g_assert (self->dnd.cell != -1);

      cell_height = minutes_height * 30;
      event_height = minutes_height * self->dnd.event_minutes;
      column = self->dnd.cell / (MINUTES_PER_DAY / 30);
      row = self->dnd.cell - column * 48;

      gtk_widget_size_allocate (self->dnd.widget,
                                &(GtkAllocation) {
                                  .x = column * column_width,
                                  .y = row * cell_height,
                                  .width = column_width,
                                  .height = event_height,
                                },
                                baseline);
    }

  week_start = gcal_date_time_get_start_of_week (self->active_date);

  /* Event widgets */
  for (size_t block_index = 0; block_index < self->layout_blocks->len; block_index++)
    {
      g_autoptr (GDateTime) block_range_start = NULL;
      LayoutBlock *layout_block = NULL;
      double block_column_width;
      int block_weekday;

      layout_block = g_ptr_array_index (self->layout_blocks, block_index);
      g_assert (layout_block != NULL);
      g_assert (layout_block->columns != NULL);

      block_range_start = gcal_range_get_start (layout_block->range);
      block_weekday = gcal_date_time_compare_date (block_range_start, week_start);
      g_assert (block_weekday >= 0 && block_weekday < 7);

      block_column_width = column_width / (double) layout_block->columns->len - 1;

      for (size_t block_column_index = 0; block_column_index < layout_block->columns->len; block_column_index++)
        {
          g_autoptr (GPtrArray) children_data = NULL;
          GcalRangeTree *block_column;

          block_column = g_ptr_array_index (layout_block->columns, block_column_index);
          g_assert (block_column != NULL);

          children_data = gcal_range_tree_get_all_data (block_column);
          g_assert (children_data != NULL);

          for (size_t child_data_index = 0; child_data_index < children_data->len; child_data_index++)
            {
              g_autoptr (GDateTime) event_start = NULL;
              g_autoptr (GDateTime) event_end = NULL;
              GtkAllocation child_allocation;
              GtkWidget *event_widget;
              ChildData *child_data;
              gint event_minutes;
              gint minimum_width;
              gint minimum_height;
              gint event_height;
              gint event_width;
              gint x;
              gint y;

              child_data = g_ptr_array_index (children_data, child_data_index);
              event_widget = child_data->widget;

              if (!gtk_widget_should_layout (event_widget))
                continue;

              event_start = g_date_time_to_local (gcal_event_get_date_start (child_data->event));
              event_end = g_date_time_to_local (gcal_event_get_date_end (child_data->event));
              event_minutes = g_date_time_difference (event_end, event_start) / G_TIME_SPAN_MINUTE;

              /* Compute the height of the widget */
              gtk_widget_measure (event_widget, GTK_ORIENTATION_VERTICAL, -1, &minimum_height, NULL, NULL, NULL);
              event_height = event_minutes * minutes_height;
              event_height = MAX (minimum_height, event_height);

              /* Compute the width of the widget */
              gtk_widget_measure (event_widget, GTK_ORIENTATION_HORIZONTAL, event_height, &minimum_width, NULL, NULL, NULL);
              event_width = MAX (minimum_width, block_column_width * child_data->n_columns);

              x = column_width * block_weekday + block_column_width * block_column_index + 1;
              if (!ltr)
                x = width - (x + event_width);
              y = (g_date_time_get_hour (event_start) * 60 + g_date_time_get_minute (event_start)) * minutes_height;

              /* Setup the child position and size */
              child_allocation.x = x;
              child_allocation.y = y;
              child_allocation.width = event_width;
              child_allocation.height = event_height;

              gtk_widget_size_allocate (event_widget, &child_allocation, baseline);
            }
        }
    }

  /* Today column */
  today_column = get_today_column (GCAL_WEEK_GRID (widget));

  gtk_widget_set_child_visible (self->now_strip, today_column != -1);

  if (today_column != -1)
    {
      g_autoptr (GDateTime) now = NULL;
      GtkAllocation allocation;
      guint minutes_from_midnight;
      gint now_strip_height;
      gint x;

      now = g_date_time_new_now_local ();
      minutes_from_midnight = g_date_time_get_hour (now) * 60 + g_date_time_get_minute (now);

      gtk_widget_measure (self->now_strip,
                          GTK_ORIENTATION_VERTICAL,
                          -1,
                          &now_strip_height,
                          NULL,
                          NULL,
                          NULL);

      if (ltr)
        x = today_column * column_width;
      else
        x = width - (today_column * column_width) - column_width;

      allocation.x = x;
      allocation.y = round (minutes_from_midnight * ((gdouble) height / MINUTES_PER_DAY));
      allocation.width = column_width;
      allocation.height = MAX (1, now_strip_height);

      gtk_widget_size_allocate (self->now_strip, &allocation, baseline);
    }
}

static void
gcal_week_grid_class_init (GcalWeekGridClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gcal_week_grid_dispose;
  object_class->finalize = gcal_week_grid_finalize;
  object_class->get_property = gcal_week_grid_get_property;
  object_class->set_property = gcal_week_grid_set_property;

  widget_class->map = gcal_week_grid_map;
  widget_class->measure = gcal_week_grid_measure;
  widget_class->size_allocate = gcal_week_grid_size_allocate;
  widget_class->snapshot = gcal_week_grid_snapshot;

  signals[EVENT_ACTIVATED] = g_signal_new ("event-activated",
                                           GCAL_TYPE_WEEK_GRID,
                                           G_SIGNAL_RUN_FIRST,
                                           0,  NULL, NULL, NULL,
                                           G_TYPE_NONE,
                                           1,
                                           GCAL_TYPE_EVENT_WIDGET);

  gtk_widget_class_set_css_name (widget_class, "weekgrid");
}

static void
gcal_week_grid_init (GcalWeekGrid *self)
{
  GcalContext *context = gcal_application_get_context (GCAL_DEFAULT_APPLICATION);
  GtkCustomFilter *filter;
  GtkDropTarget *drop_target;
  GtkGesture *click_gesture;
  AdwStyleManager *manager;

  self->selection.start = -1;
  self->selection.end = -1;
  self->selection.widget = gcal_create_selection_widget ();
  gtk_widget_set_visible (self->selection.widget, FALSE);
  gtk_widget_set_parent (self->selection.widget, GTK_WIDGET (self));

  self->dnd.cell = -1;
  self->dnd.widget = gcal_create_drop_target_widget ();
  gtk_widget_set_visible (self->dnd.widget, FALSE);
  gtk_widget_set_parent (self->dnd.widget, GTK_WIDGET (self));

  self->layout_blocks = g_ptr_array_new_with_free_func (layout_block_free);

  filter = gtk_custom_filter_new (event_is_timed_single_day_func, NULL, NULL);
  self->filter_model = gtk_filter_list_model_new (NULL, GTK_FILTER (g_steal_pointer (&filter)));
  self->sort_model = gtk_sort_list_model_new (G_LIST_MODEL (g_object_ref (self->filter_model)),
                                              GTK_SORTER (gtk_custom_sorter_new (compare_events_cb, NULL, NULL)));
  g_signal_connect (self->sort_model, "items-changed", G_CALLBACK (events_changed_cb), self);

  self->now_strip = adw_bin_new ();
  gtk_widget_add_css_class (self->now_strip, "now-strip");
  gtk_widget_set_can_target (self->now_strip, FALSE);
  gtk_widget_set_parent (self->now_strip, GTK_WIDGET (self));

  click_gesture = gtk_gesture_click_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (click_gesture), GDK_BUTTON_PRIMARY);
  g_signal_connect (click_gesture, "pressed", G_CALLBACK (on_click_gesture_pressed_cb), self);
  g_signal_connect (click_gesture, "released", G_CALLBACK (on_click_gesture_released_cb), self);
  gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (click_gesture));

  self->motion_controller = gtk_event_controller_motion_new ();
  g_signal_connect (self->motion_controller, "motion", G_CALLBACK (on_motion_controller_motion_cb), self);
  gtk_event_controller_set_propagation_phase (self->motion_controller, GTK_PHASE_NONE);
  gtk_widget_add_controller (GTK_WIDGET (self), self->motion_controller);

  drop_target = gtk_drop_target_new (GCAL_TYPE_EVENT_WIDGET, GDK_ACTION_COPY);
  gtk_drop_target_set_preload (drop_target, TRUE);
  g_signal_connect (drop_target, "drop", G_CALLBACK (on_drop_target_drop_cb), self);
  g_signal_connect (drop_target, "enter", G_CALLBACK (on_drop_target_enter_cb), self);
  g_signal_connect (drop_target, "leave", G_CALLBACK (on_drop_target_leave_cb), self);
  g_signal_connect (drop_target, "motion", G_CALLBACK (on_drop_target_motion_cb), self);
  gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (drop_target));

  g_signal_connect_object (gcal_context_get_clock (context),
                           "minute-changed",
                           G_CALLBACK (gtk_widget_queue_resize),
                           self->now_strip,
                           G_CONNECT_SWAPPED);

  manager = adw_style_manager_get_default ();
  g_signal_connect_object (manager,
                           "notify::high-contrast",
                           G_CALLBACK (gtk_widget_queue_draw),
                           self,
                           G_CONNECT_SWAPPED);
}

void
gcal_week_grid_set_model (GcalWeekGrid *self,
                          GListModel   *model)
{
  g_assert (GCAL_IS_WEEK_GRID (self));

  gtk_filter_list_model_set_model (self->filter_model, model);
}

GList*
gcal_week_grid_get_children_by_uuid (GcalWeekGrid          *self,
                                     GcalRecurrenceModType  mod,
                                     const gchar           *uid)
{
  g_autoptr (GList) result = NULL;

  GCAL_ENTRY;

  result = filter_children_by_uid_and_modtype (GTK_WIDGET (self), mod, uid);

  GCAL_RETURN (g_steal_pointer (&result));
}

void
gcal_week_grid_clear_marks (GcalWeekGrid *self)
{
  g_return_if_fail (GCAL_IS_WEEK_GRID (self));

  self->selection.start = -1;
  self->selection.end = -1;
  gtk_widget_set_visible (self->selection.widget, FALSE);
}

void
gcal_week_grid_set_date (GcalWeekGrid *self,
                         GDateTime    *date)
{
  gcal_set_date_time (&self->active_date, date);

  invalidate_layout_blocks (self);
}

