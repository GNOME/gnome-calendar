/*
 * gcal-month-view.c
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

#define G_LOG_DOMAIN "GcalMonthView"

#include "config.h"
#include "gcal-context.h"
#include "gcal-debug.h"
#include "gcal-month-cell.h"
#include "gcal-month-popover.h"
#include "gcal-month-view.h"
#include "gcal-month-view-row.h"
#include "gcal-range-tree.h"
#include "gcal-timeline-subscriber.h"
#include "gcal-utils.h"
#include "gcal-view-private.h"

#include <math.h>
#include <adwaita.h>

#define N_ROWS_PER_PAGE 5
#define N_PAGES 5
#define N_TOTAL_ROWS (N_ROWS_PER_PAGE * N_PAGES)
#define FIRST_VISIBLE_ROW_INDEX (N_ROWS_PER_PAGE * (N_PAGES - 1) / 2)
#define LAST_VISIBLE_ROW_INDEX (FIRST_VISIBLE_ROW_INDEX + N_ROWS_PER_PAGE - 1)

// N_PAGES must be an odd number
G_STATIC_ASSERT (N_PAGES % 2 != 0);

struct _GcalMonthView
{
  GtkWidget          parent;

  GtkWidget          *header;
  GtkWidget          *weekday_label[7];

  GtkEventController *motion_controller;

  GPtrArray          *week_rows;
  GHashTable         *events;

  struct {
    GtkWidget        *popover;
    GcalMonthViewRow *row;
    GtkWidget        *relative_to;
  } overflow;

  struct {
    GDateTime        *start;
    GDateTime        *end;
  } selection;

  /* Ranges from [0.0, 1.0] */
  gdouble             row_offset;
  AdwAnimation       *row_offset_animation;

  AdwAnimation       *kinetic_scroll_animation;
  gdouble             last_velocity;

  GDateTime          *date;
  GcalContext        *context;
};

static void          gcal_view_interface_init                    (GcalViewInterface  *iface);

static void          gtk_buildable_interface_init                (GtkBuildableIface  *iface);

static void          gcal_timeline_subscriber_interface_init     (GcalTimelineSubscriberInterface *iface);


G_DEFINE_FINAL_TYPE_WITH_CODE (GcalMonthView, gcal_month_view, GTK_TYPE_WIDGET,
                               G_IMPLEMENT_INTERFACE (GCAL_TYPE_TIMELINE_SUBSCRIBER, gcal_timeline_subscriber_interface_init)
                               G_IMPLEMENT_INTERFACE (GCAL_TYPE_VIEW, gcal_view_interface_init)
                               G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, gtk_buildable_interface_init))

enum
{
  PROP_0,
  PROP_CONTEXT,
  PROP_DATE,
  PROP_TIME_DIRECTION,
  N_PROPS,
};


/*
 * Auxiliary methods
 */

static void
allocate_overflow_popover (GcalMonthView *self,
                           gint           width,
                           gint           height,
                           gint           baseline)
{
  graphene_point_t origin, end;
  GtkAllocation popover_allocation;
  GtkAllocation cell_allocation;
  gint popover_min_width;
  gint popover_nat_width;
  gint popover_height;
  gint popover_width;
  gint header_height;
  gint popover_x2;
  gint popover_y2;

  g_assert (self->overflow.relative_to != NULL);

  header_height = gtk_widget_get_height (self->header);

  if (!gtk_widget_compute_point (self->overflow.relative_to,
                                 GTK_WIDGET (self),
                                 &GRAPHENE_POINT_INIT (0, 0),
                                 &origin))
    g_assert_not_reached ();

  if (!gtk_widget_compute_point (self->overflow.relative_to,
                                 GTK_WIDGET (self),
                                 &GRAPHENE_POINT_INIT (gtk_widget_get_width (self->overflow.relative_to),
                                                       gtk_widget_get_height (self->overflow.relative_to)),
                                 &end))
    g_assert_not_reached ();

  cell_allocation = (GtkAllocation) {
    .x = origin.x,
    .y = origin.y,
    .width = end.x - origin.x,
    .height = end.y - origin.y,
  };

  gtk_widget_measure (GTK_WIDGET (self->overflow.popover),
                      GTK_ORIENTATION_HORIZONTAL,
                      -1,
                      &popover_min_width,
                      &popover_nat_width,
                      NULL, NULL);

  gtk_widget_measure (GTK_WIDGET (self->overflow.popover),
                      GTK_ORIENTATION_VERTICAL,
                      -1,
                      NULL,
                      &popover_height,
                      NULL, NULL);

  popover_width = CLAMP (popover_nat_width,
                         MAX (popover_min_width, cell_allocation.width * 1.25),
                         cell_allocation.width * 1.5);
  popover_height = CLAMP (popover_height, cell_allocation.height * 1.5, height);

  popover_allocation = (GtkAllocation) {
    .x = MAX (0, cell_allocation.x - (popover_width - cell_allocation.width) / 2.0),
    .y = MAX (header_height, cell_allocation.y - (popover_height - cell_allocation.height) / 2.0),
    .width = popover_width,
    .height = popover_height,
  };

  popover_x2 = popover_allocation.x + popover_allocation.width;
  if (popover_x2 > width)
    popover_allocation.x -= (popover_x2 - width);

  popover_y2 = popover_allocation.y + popover_allocation.height;
  if (popover_y2 > height)
    popover_allocation.y -= (popover_y2 - height);

  gtk_widget_size_allocate (self->overflow.popover, &popover_allocation, baseline);
}

static inline void
update_weekday_labels (GcalMonthView *self)
{
  const gint first_weekday = get_first_weekday ();

  for (gint i = 0; i < 7; i++)
    {
      g_autofree gchar *weekday_name = NULL;

      weekday_name = g_utf8_strup (gcal_get_weekday ((i + first_weekday) % 7), -1);

      gtk_label_set_label (GTK_LABEL (self->weekday_label[i]), weekday_name);
    }
}

static void
cancel_deceleration (GcalMonthView *self)
{
  if (self->kinetic_scroll_animation == 0)
    return;

  adw_animation_pause (self->kinetic_scroll_animation);
  g_clear_object (&self->kinetic_scroll_animation);
}

static void
cancel_row_offset_animation (GcalMonthView *self)
{
  if (!self->row_offset_animation)
    return;

  adw_animation_pause (self->row_offset_animation);
  g_clear_object (&self->row_offset_animation);
}

static void
update_active_date (GcalMonthView *self)
{
  g_autoptr (GcalRange) top_row_range = NULL;
  GtkWidget *top_row;

  top_row = g_ptr_array_index (self->week_rows, FIRST_VISIBLE_ROW_INDEX);
  top_row_range = gcal_month_view_row_get_range (GCAL_MONTH_VIEW_ROW (top_row));
  g_assert (top_row_range != NULL);

  gcal_view_set_date (GCAL_VIEW (self), gcal_range_get_start (top_row_range));
  g_object_notify (G_OBJECT (self), "active-date");
}

static void
add_cached_events_to_row (GcalMonthView    *self,
                          GcalMonthViewRow *row)
{
  g_autoptr (GcalRange) row_range = NULL;
  GHashTableIter iter;
  GcalEvent *event;

  row_range = gcal_month_view_row_get_range (row);

  g_hash_table_iter_init (&iter, self->events);
  while (g_hash_table_iter_next (&iter, (gpointer *) &event, NULL))
    {
      GcalRangeOverlap overlap = gcal_range_calculate_overlap (row_range, gcal_event_get_range (event), NULL);

      if (overlap == GCAL_RANGE_NO_OVERLAP)
        continue;

      gcal_month_view_row_add_event (row, event);
    }
}

static inline void
maybe_popdown_overflow_popover (GcalMonthView *self)
{
  uint32_t row_index;
  gboolean found;

  if (!self->overflow.row)
    return;

  found = g_ptr_array_find (self->week_rows, self->overflow.row, &row_index);
  g_assert (found);

  if (floor (row_index / (gdouble) N_ROWS_PER_PAGE) != floor (N_PAGES / 2.0))
    gcal_month_popover_popdown (GCAL_MONTH_POPOVER (self->overflow.popover));
}

static void
move_bottom_row_to_top (GcalMonthView *self)
{
  g_autoptr (GcalRange) first_row_range = NULL;
  g_autoptr (GcalRange) new_range = NULL;
  GDateTime *first_row_range_start;
  GtkWidget *first_row;
  GtkWidget *last_row;

  first_row = g_ptr_array_index (self->week_rows, 0);
  first_row_range = gcal_month_view_row_get_range (GCAL_MONTH_VIEW_ROW (first_row));
  g_assert (first_row_range != NULL);
  first_row_range_start = gcal_range_get_start (first_row_range);
  g_assert (first_row_range_start != NULL);

  new_range = gcal_range_new_take (g_date_time_add_weeks (first_row_range_start, -1),
                                   g_date_time_ref (first_row_range_start),
                                   GCAL_RANGE_DEFAULT);

#ifdef GCAL_ENABLE_TRACE
    {
      g_autofree gchar *first_row_range_str = gcal_range_to_string (first_row_range);
      g_autofree gchar *new_range_str = gcal_range_to_string (new_range);
      GCAL_TRACE_MSG ("Moved bottom row to top, new range: %s (first row range: %s)", new_range_str, first_row_range_str);
    }
#endif

  last_row = g_ptr_array_steal_index (self->week_rows, self->week_rows->len - 1);
  gcal_month_view_row_set_range (GCAL_MONTH_VIEW_ROW (last_row), new_range);
  g_ptr_array_insert (self->week_rows, 0, last_row);

  add_cached_events_to_row (self, GCAL_MONTH_VIEW_ROW (last_row));
}

static void
move_top_row_to_bottom (GcalMonthView *self)
{
  g_autoptr (GcalRange) last_row_range = NULL;
  g_autoptr (GcalRange) new_range = NULL;
  GDateTime *last_row_range_end;
  GtkWidget *first_row;
  GtkWidget *last_row;

  last_row = g_ptr_array_index (self->week_rows, self->week_rows->len - 1);
  last_row_range = gcal_month_view_row_get_range (GCAL_MONTH_VIEW_ROW (last_row));
  g_assert (last_row_range != NULL);
  last_row_range_end = gcal_range_get_end (last_row_range);
  g_assert (last_row_range_end != NULL);

  new_range = gcal_range_new_take (g_date_time_ref (last_row_range_end),
                                   g_date_time_add_weeks (last_row_range_end, 1),
                                   GCAL_RANGE_DEFAULT);

#ifdef GCAL_ENABLE_TRACE
    {
      g_autofree gchar *last_row_range_str = gcal_range_to_string (last_row_range);
      g_autofree gchar *new_range_str = gcal_range_to_string (new_range);
      GCAL_TRACE_MSG ("Moved top row to bottom, new range: %s (last row range: %s)", new_range_str, last_row_range_str);
    }
#endif

  first_row = g_ptr_array_steal_index (self->week_rows, 0);
  gcal_month_view_row_set_range (GCAL_MONTH_VIEW_ROW (first_row), new_range);
  g_ptr_array_insert (self->week_rows, -1, first_row);

  add_cached_events_to_row (self, GCAL_MONTH_VIEW_ROW (first_row));
}

static inline void
dump_row_ranges (GcalMonthView *self)
{
#if defined(GCAL_ENABLE_TRACE) && 0
  for (guint i = 0; i < self->week_rows->len; i++)
    {
      g_autoptr (GcalRange) range = NULL;
      g_autofree gchar *range_str = NULL;
      GtkWidget *row;

      row = g_ptr_array_index (self->week_rows, i);
      range = gcal_month_view_row_get_range (GCAL_MONTH_VIEW_ROW (row));
      range_str = gcal_range_to_string (range);

      GCAL_TRACE_MSG ("    Row %u: %s", i, range_str);
    }
#endif
}

static void
update_week_ranges (GcalMonthView *self,
                    GDateTime     *new_date)
{
  g_autoptr (GcalRange) current_range = NULL;
  g_autoptr (GDateTime) current_date = NULL;
  gint n_weeks_before;

  /*
   * The current date corresponds to the week of the first
   * visible row.
   */
  n_weeks_before = N_ROWS_PER_PAGE * (N_PAGES - 1) / 2;

  current_date = g_steal_pointer (&self->date);
  if (current_date)
    current_range = gcal_timeline_subscriber_get_range (GCAL_TIMELINE_SUBSCRIBER (self));

  gcal_set_date_time (&self->date, new_date);

  if (current_range && gcal_range_contains_datetime (current_range, new_date))
    {
      g_autoptr (GcalRange) row_range = NULL;
      GcalMonthViewRow *row;
      gint diff;

      diff = g_date_time_compare (new_date, current_date);
      row = g_ptr_array_index (self->week_rows, n_weeks_before);
      row_range = gcal_month_view_row_get_range (row);

      while (!gcal_range_contains_datetime (row_range, new_date))
        {
          if (diff > 0)
            move_top_row_to_bottom (self);
          else
            move_bottom_row_to_top (self);

          g_clear_pointer (&row_range, gcal_range_unref);

          row = g_ptr_array_index (self->week_rows, n_weeks_before);
          row_range = gcal_month_view_row_get_range (row);
        }
    }
  else
    {
      for (gint i = 0; i < self->week_rows->len; i++)
        {
          g_autoptr (GDateTime) week_start = NULL;
          g_autoptr (GDateTime) week_end = NULL;
          g_autoptr (GcalRange) range = NULL;
          g_autoptr (GDateTime) date = NULL;
          GcalMonthViewRow *row;

          date = g_date_time_add_weeks (self->date, i - n_weeks_before);
          week_start = gcal_date_time_get_start_of_week (date);
          week_end = g_date_time_add_weeks (week_start, 1);
          range = gcal_range_new (week_start, week_end, GCAL_RANGE_DEFAULT);

          row = g_ptr_array_index (self->week_rows, i);
          gcal_month_view_row_set_range (row, range);
        }
    }

  maybe_popdown_overflow_popover (self);
  dump_row_ranges (self);
}

static void
update_row_visuals (GcalMonthView *self)
{
  g_autoptr (GcalRange) first_visible_row_range = NULL;
  g_autoptr (GcalRange) last_visible_row_range = NULL;
  g_autoptr (GcalRange) union_range = NULL;
  g_autoptr (GDateTime) start = NULL;
  g_autoptr (GDateTime) middle = NULL;
  g_autoptr (GDateTime) end = NULL;
  GcalMonthViewRow *first_visible_row;
  GcalMonthViewRow *last_visible_row;

  first_visible_row = g_ptr_array_index (self->week_rows, FIRST_VISIBLE_ROW_INDEX);
  first_visible_row_range = gcal_month_view_row_get_range (first_visible_row);

  last_visible_row = g_ptr_array_index (self->week_rows, LAST_VISIBLE_ROW_INDEX);
  last_visible_row_range = gcal_month_view_row_get_range (last_visible_row);

  union_range = gcal_range_union (first_visible_row_range, last_visible_row_range);
  start = gcal_range_get_start (union_range);
  end = gcal_range_get_end (union_range);
  middle = g_date_time_add_days (start, gcal_date_time_compare_date (end, start) / 2);

  for (gint i = 0; i < self->week_rows->len; i++)
    {
      GcalMonthViewRow *row;
      gboolean can_focus;

      row = g_ptr_array_index (self->week_rows, i);
      can_focus = i >= FIRST_VISIBLE_ROW_INDEX && i <= LAST_VISIBLE_ROW_INDEX;

      gtk_widget_set_can_focus (GTK_WIDGET (row), can_focus);
    }
}

static inline gint
get_grid_height (GcalMonthView *self)
{
  return gtk_widget_get_height (GTK_WIDGET (self)) - gtk_widget_get_height (self->header);
}

static void
offset_and_shuffle_rows (GcalMonthView *self,
                         gdouble        dy)
{
  GCAL_ENTRY;

  self->row_offset += dy;

  if (fabs (self->row_offset) > 0.5)
    {
      gint rows_to_shuffle = round (fabs (self->row_offset) + 0.5);

      g_assert (rows_to_shuffle > 0);

      if (self->row_offset >= 0.0)
        {
          /* Positive offset: move top rows to the bottom */
          while (rows_to_shuffle--)
            move_top_row_to_bottom (self);
          self->row_offset = fmod (self->row_offset, 0.5) - 0.5;
        }
      else
        {
          /* Negative offset: move bottom rows to the top */
          while (rows_to_shuffle--)
            move_bottom_row_to_top (self);
          self->row_offset = fmod (self->row_offset, 0.5) + 0.5;
        }

      maybe_popdown_overflow_popover (self);
      update_active_date (self);
      dump_row_ranges (self);
    }

  gtk_widget_queue_allocate (GTK_WIDGET (self));

  GCAL_EXIT;
}

static void
offset_and_shuffle_rows_by_pixels (GcalMonthView *self,
                                   gdouble        dy_pixels)
{
  gdouble row_height;
  gint grid_height;

  GCAL_ENTRY;

  grid_height = get_grid_height (self);
  row_height = grid_height / (gdouble) N_ROWS_PER_PAGE;

  offset_and_shuffle_rows (self, dy_pixels / row_height);

  GCAL_EXIT;
}

static void
animate_row_offset_cb (gdouble  value,
                       gpointer user_data)
{
  GcalMonthView *self = GCAL_MONTH_VIEW (user_data);

  self->row_offset = value;
  gtk_widget_queue_allocate (GTK_WIDGET (self));
}

static void
on_row_offset_animation_done (AdwAnimation  *animation,
                              GcalMonthView *self)
{
  gtk_widget_remove_css_class (GTK_WIDGET (self), "scrolling");
  cancel_row_offset_animation (self);

  self->row_offset = 0.0;
  gtk_widget_queue_allocate (GTK_WIDGET (self));
}

static void
snap_to_top_row (GcalMonthView *self)
{
  g_autoptr (AdwAnimationTarget) animation_target = NULL;

  GCAL_ENTRY;

  update_active_date (self);
  dump_row_ranges (self);

  animation_target = adw_callback_animation_target_new (animate_row_offset_cb, self, NULL);

  g_clear_object (&self->row_offset_animation);
  self->row_offset_animation = adw_timed_animation_new (GTK_WIDGET (self),
                                                        self->row_offset,
                                                        0.0,
                                                        200, // ms
                                                        g_steal_pointer (&animation_target));
  adw_timed_animation_set_easing (ADW_TIMED_ANIMATION (self->row_offset_animation),
                                  ADW_EASE_OUT_QUAD);
  g_signal_connect (self->row_offset_animation,
                    "done",
                    G_CALLBACK (on_row_offset_animation_done),
                    self);

  adw_animation_play (self->row_offset_animation);

  GCAL_EXIT;
}

static void
animate_row_scroll_cb (gdouble  value,
                       gpointer user_data)
{
  GcalMonthView *self = GCAL_MONTH_VIEW (user_data);
  gdouble *last_offset_location;
  gdouble dy;

  last_offset_location = g_object_get_data (G_OBJECT (self->row_offset_animation), "last-offset");
  g_assert (last_offset_location != NULL);

  dy = value - *last_offset_location;

  offset_and_shuffle_rows (self, dy);

  *last_offset_location = value;
}

static void
animate_row_scroll (GcalMonthView *self,
                    gint           n_rows)
{
  GCAL_ENTRY;

  g_assert (n_rows != 0);

  update_active_date (self);
  dump_row_ranges (self);

  if (!self->row_offset_animation)
    {
      g_autoptr (AdwAnimationTarget) animation_target = NULL;
      g_autofree gdouble *last_offset_location = NULL;
      gboolean animate;

      g_object_get (gtk_widget_get_settings (GTK_WIDGET (self)),
                    "gtk-enable-animations", &animate,
                    NULL);


      animation_target = adw_callback_animation_target_new (animate_row_scroll_cb, self, NULL);

      self->row_offset_animation = adw_timed_animation_new (GTK_WIDGET (self),
                                                            self->row_offset,
                                                            n_rows,
                                                            animate ? 150 : 100, // ms
                                                            g_steal_pointer (&animation_target));
      adw_timed_animation_set_easing (ADW_TIMED_ANIMATION (self->row_offset_animation),
                                      ADW_EASE_OUT_QUAD);
      adw_animation_set_follow_enable_animations_setting (self->row_offset_animation, FALSE);

      g_signal_connect (self->row_offset_animation,
                        "done",
                        G_CALLBACK (on_row_offset_animation_done),
                        self);

      last_offset_location = g_malloc (sizeof (gdouble));
      *last_offset_location = self->row_offset;
      g_object_set_data_full (G_OBJECT (self->row_offset_animation),
                              "last-offset",
                              g_steal_pointer (&last_offset_location),
                              g_free);

      adw_animation_play (self->row_offset_animation);
    }
  else
    {
      gdouble target_value;

      target_value = adw_timed_animation_get_value_to (ADW_TIMED_ANIMATION (self->row_offset_animation));

      adw_animation_pause (self->row_offset_animation);
      adw_timed_animation_set_value_to (ADW_TIMED_ANIMATION (self->row_offset_animation), target_value + n_rows);
      adw_animation_play (self->row_offset_animation);
    }

  GCAL_EXIT;
}

static void
update_selection_range (GcalMonthView *self)
{
  g_autoptr (GcalRange) selection_range = NULL;
  GDateTime *selection_start;
  GDateTime *selection_end;

  selection_start = self->selection.start;
  selection_end = self->selection.end;

  if (selection_start)
    {
      if (!selection_end)
        selection_end = selection_start;

      /* Swap dates if end is before start */
      if (gcal_date_time_compare_date (selection_start, selection_end) > 0)
        {
          GDateTime *aux = selection_end;
          selection_end = selection_start;
          selection_start = aux;
        }

      selection_range = gcal_range_new (selection_start, selection_end, GCAL_RANGE_DATE_ONLY);
    }
  else
    {
      g_assert (selection_end == NULL);
      selection_range = NULL;
    }

  for (gint i = 0; i < self->week_rows->len; i++)
    {
      GcalMonthViewRow *row = g_ptr_array_index (self->week_rows, i);
      gcal_month_view_row_update_selection (row, selection_range);
    }
}



/*
 * Callbacks
 */

static void
on_click_gesture_pressed_cb (GtkGestureClick *click_gesture,
                             gint             n_press,
                             gdouble          x,
                             gdouble          y,
                             GcalMonthView   *self)
{
  GtkWidget *widget_at_position;
  GtkWidget *day_cell;

  GCAL_ENTRY;

  widget_at_position = gtk_widget_pick (GTK_WIDGET (self), x, y, GTK_PICK_DEFAULT);
  day_cell = gtk_widget_get_ancestor (widget_at_position, GCAL_TYPE_MONTH_CELL);

  if (!day_cell)
    GCAL_RETURN ();

  g_assert (GCAL_IS_MONTH_CELL (day_cell));

  gcal_clear_date_time (&self->selection.start);
  self->selection.start = g_date_time_ref (gcal_month_cell_get_date (GCAL_MONTH_CELL (day_cell)));

  gtk_event_controller_set_propagation_phase (self->motion_controller, GTK_PHASE_BUBBLE);

  update_selection_range (self);

  GCAL_EXIT;
}

static void
on_click_gesture_released_cb (GtkGestureClick *click_gesture,
                              gint             n_press,
                              gdouble          x,
                              gdouble          y,
                              GcalMonthView   *self)
{
  g_autoptr (GcalRange) selection_range = NULL;
  g_autoptr (GDateTime) selection_start = NULL;
  g_autoptr (GDateTime) selection_end = NULL;
  GtkWidget *widget_at_position;

  GCAL_ENTRY;

  gtk_event_controller_set_propagation_phase (self->motion_controller, GTK_PHASE_NONE);

  widget_at_position = gtk_widget_pick (GTK_WIDGET (self), x, y, GTK_PICK_DEFAULT);

  if (!self->selection.start ||
      !widget_at_position ||
      !gtk_widget_get_ancestor (widget_at_position, GCAL_TYPE_MONTH_VIEW_ROW))
    {
      gcal_view_clear_marks (GCAL_VIEW (self));
      GCAL_RETURN ();
    }

  selection_start = self->selection.start;
  selection_end = self->selection.end ?: selection_start;

  /* Swap dates if end is before start */
  if (gcal_date_time_compare_date (selection_start, selection_end) > 0)
    {
      GDateTime *aux = selection_end;
      selection_end = selection_start;
      selection_start = aux;
    }

  selection_start = g_date_time_ref (selection_start);
  selection_end = g_date_time_add_days (selection_end, 1);

  selection_range = gcal_range_new (selection_start, selection_end, GCAL_RANGE_DATE_ONLY);
  gcal_view_create_event (GCAL_VIEW (self), selection_range, x, y);

  GCAL_EXIT;
}

static void
on_motion_controller_motion_cb (GtkEventControllerMotion *motion_controller,
                                gdouble                   x,
                                gdouble                   y,
                                GcalMonthView            *self)
{
  GtkWidget *widget_at_position;
  GtkWidget *day_cell;
  GtkWidget *row;

  GCAL_ENTRY;

  widget_at_position = gtk_widget_pick (GTK_WIDGET (self), x, y, GTK_PICK_DEFAULT);

  if (!widget_at_position)
    GCAL_RETURN ();

  row = gtk_widget_get_ancestor (widget_at_position, GCAL_TYPE_MONTH_VIEW_ROW);

  if (!row)
    GCAL_RETURN ();

  day_cell = gcal_month_view_row_get_cell_at_x (GCAL_MONTH_VIEW_ROW (row), x);

  g_assert (day_cell != NULL);
  g_assert (GCAL_IS_MONTH_CELL (day_cell));

  gcal_clear_date_time (&self->selection.end);
  self->selection.end = g_date_time_ref (gcal_month_cell_get_date (GCAL_MONTH_CELL (day_cell)));

  update_selection_range (self);

  GCAL_EXIT;
}

static void
on_event_widget_activated_cb (GcalMonthViewRow *row,
                              GcalEventWidget  *event_widget,
                              GcalMonthView    *self)
{
  gcal_view_event_activated (GCAL_VIEW (self), event_widget);
}

static void
on_scroll_controller_scroll_begin_cb (GtkEventControllerScroll *scroll_controller,
                                      GcalMonthView            *self)
{
  GdkEvent *event;

  GCAL_ENTRY;

  event = gtk_event_controller_get_current_event (GTK_EVENT_CONTROLLER (scroll_controller));
  if (gdk_event_get_event_type (event) != GDK_TOUCHPAD_HOLD ||
      gdk_touchpad_event_get_n_fingers (event) > 1)
    {
      gtk_widget_add_css_class (GTK_WIDGET (self), "scrolling");
      cancel_row_offset_animation (self);
      cancel_deceleration (self);
    }

  GCAL_EXIT;
}

static gboolean
on_scroll_controller_scroll_cb (GtkEventControllerScroll *scroll_controller,
                                gdouble                   dx,
                                gdouble                   dy,
                                GcalMonthView            *self)
{
  GdkEvent *current_event;

  GCAL_ENTRY;

  current_event = gtk_event_controller_get_current_event (GTK_EVENT_CONTROLLER (scroll_controller));

  switch (gdk_scroll_event_get_direction (current_event))
    {
    case GDK_SCROLL_SMOOTH:
      cancel_row_offset_animation (self);
      cancel_deceleration (self);
      offset_and_shuffle_rows_by_pixels (self, dy);
      GCAL_RETURN (GDK_EVENT_STOP);

    default:
      GCAL_RETURN (GDK_EVENT_PROPAGATE);
    }
}

static gboolean
on_discrete_scroll_controller_scroll_cb (GtkEventControllerScroll *scroll_controller,
                                         gdouble                   dx,
                                         gdouble                   dy,
                                         GcalMonthView            *self)
{
  GdkEvent *current_event;
  gint n_rows = 0;

  GCAL_ENTRY;

  current_event = gtk_event_controller_get_current_event (GTK_EVENT_CONTROLLER (scroll_controller));

  switch (gdk_scroll_event_get_direction (current_event))
    {
    case GDK_SCROLL_UP:
      n_rows = -1;
      break;

    case GDK_SCROLL_DOWN:
      n_rows = 1;
      break;

    default:
      GCAL_RETURN (GDK_EVENT_PROPAGATE);
    }

  gtk_widget_add_css_class (GTK_WIDGET (self), "scrolling");

  maybe_popdown_overflow_popover (self);
  cancel_row_offset_animation (self);
  cancel_deceleration (self);

  animate_row_scroll (self, n_rows);

  GCAL_RETURN (GDK_EVENT_STOP);
}

static void
on_scroll_controller_scroll_end_cb (GtkEventControllerScroll *scroll_controller,
                                    GcalMonthView            *self)
{
  GCAL_ENTRY;

  snap_to_top_row (self);

  GCAL_EXIT;
}

static void
decelerate_scroll_cb (gdouble  value,
                      gpointer user_data)
{
  GcalMonthView *self = GCAL_MONTH_VIEW (user_data);
  gdouble dy;

  dy = self->last_velocity - value;
  self->last_velocity = value;

  if (fabs (value) > (get_grid_height (self) / (gdouble) N_ROWS_PER_PAGE / 5.0))
    offset_and_shuffle_rows_by_pixels (self, dy);
  else
    adw_animation_skip (self->kinetic_scroll_animation);
}

static void
on_kinetic_scroll_done_cb (AdwAnimation  *animation,
                           GcalMonthView *self)
{
  GCAL_ENTRY;

  snap_to_top_row (self);

  GCAL_EXIT;
}

static void
on_scroll_controller_decelerate_cb (GtkEventControllerScroll *scroll_controller,
                                    gdouble                   velocity_x,
                                    gdouble                   velocity_y,
                                    GcalMonthView            *self)
{
  g_autoptr (AdwAnimationTarget) animation_target = NULL;
  gdouble row_height;
  gdouble duration;
  gint grid_height;

  GCAL_ENTRY;

  /* XXX: I don't understand, but this just feels better */
  velocity_y /= 2.0;

  cancel_row_offset_animation (self);
  cancel_deceleration (self);

  grid_height = get_grid_height (self);
  if (fabs (velocity_y) < (grid_height / (gdouble) N_ROWS_PER_PAGE / 5.0))
    {
      snap_to_top_row (self);
      GCAL_RETURN ();
    }

  duration = fabs (velocity_y) / (gdouble) gtk_widget_get_height (GTK_WIDGET (self));
  animation_target = adw_callback_animation_target_new (decelerate_scroll_cb, self, NULL);

  /* Hijack the velocity so that it always ends in an actual row */
  row_height = grid_height / (gdouble) N_ROWS_PER_PAGE;
  velocity_y -= fmod (velocity_y, row_height) + self->row_offset * row_height;

  g_assert (self->kinetic_scroll_animation == NULL);
  self->kinetic_scroll_animation = adw_timed_animation_new (GTK_WIDGET (self),
                                                            velocity_y,
                                                            0.0,
                                                            duration * 1000, // ms
                                                            g_steal_pointer (&animation_target));
  self->last_velocity = velocity_y;
  adw_timed_animation_set_easing (ADW_TIMED_ANIMATION (self->kinetic_scroll_animation),
                                  ADW_EASE_OUT_EXPO);
  adw_animation_set_follow_enable_animations_setting (self->kinetic_scroll_animation, FALSE);

  g_signal_connect (self->kinetic_scroll_animation,
                    "done",
                    G_CALLBACK (on_kinetic_scroll_done_cb),
                    self);

  adw_animation_play (self->kinetic_scroll_animation);

  GCAL_EXIT;
}

static void
on_month_row_show_overflow_cb (GcalMonthViewRow *row,
                               GcalMonthCell    *cell,
                               GcalMonthView    *self)
{
  GcalMonthPopover *popover;

  GCAL_ENTRY;

  popover = GCAL_MONTH_POPOVER (self->overflow.popover);

  self->overflow.row = row;
  self->overflow.relative_to = GTK_WIDGET (cell);

  gcal_month_popover_set_date (popover, gcal_month_cell_get_date (cell));
  gcal_month_popover_popup (popover);

  /* FIXME: here we forcefully disable selection, but I wonder if there's a better
   * way to do it.
   */
  gtk_event_controller_set_propagation_phase (self->motion_controller, GTK_PHASE_NONE);
  gcal_view_clear_marks (GCAL_VIEW (self));

  GCAL_EXIT;
}

static void
on_month_popover_event_activated_cb (GcalMonthPopover *month_popover,
                                     GcalEventWidget  *event_widget,
                                     GcalMonthViewRow *self)
{
  gcal_view_event_activated (GCAL_VIEW (self), event_widget);
}


/*
 * GcalTimelineSubscriber iface
 */

static GcalRange*
gcal_month_view_get_range (GcalTimelineSubscriber *subscriber)
{
  g_autoptr (GcalRange) first_row_range = NULL;
  g_autoptr (GcalRange) last_row_range = NULL;
  GcalMonthView *self;

  self = GCAL_MONTH_VIEW (subscriber);

  first_row_range = gcal_month_view_row_get_range (g_ptr_array_index (self->week_rows, 0));
  last_row_range = gcal_month_view_row_get_range (g_ptr_array_index (self->week_rows,
                                                                     self->week_rows->len - 1));

  return gcal_range_union (first_row_range, last_row_range);
}

static void
gcal_month_view_add_event (GcalTimelineSubscriber *subscriber,
                           GcalEvent              *event)
{
  GcalMonthView *self;

  GCAL_ENTRY;

  g_assert (event != NULL);

  self = GCAL_MONTH_VIEW (subscriber);

  if (!g_hash_table_add (self->events, g_object_ref (event)))
    {
      g_warning ("Event with uuid: %s already added", gcal_event_get_uid (event));
      GCAL_RETURN ();
    }

  for (guint i = 0; i < self->week_rows->len; i++)
    {
      g_autoptr (GcalRange) row_range = NULL;
      GcalRangePosition position;
      GcalRangeOverlap overlap;
      GcalMonthViewRow *row;

      row = g_ptr_array_index (self->week_rows, i);
      row_range = gcal_month_view_row_get_range (row);

      overlap = gcal_range_calculate_overlap (row_range, gcal_event_get_range (event), &position);
      if (overlap != GCAL_RANGE_NO_OVERLAP)
        {
          gcal_month_view_row_add_event (row, event);
        }
      else
        {
          if (position == GCAL_RANGE_AFTER)
            break;
        }
    }

  GCAL_EXIT;
}

static void
gcal_month_view_remove_event (GcalTimelineSubscriber *subscriber,
                              GcalEvent              *event)
{
  g_autoptr (GcalEvent) G_GNUC_UNUSED owned_event = NULL;
  GcalMonthView *self;

  GCAL_ENTRY;

  g_assert (event != NULL);

  self = GCAL_MONTH_VIEW (subscriber);

  /* Keep event alive while removing it */
  owned_event = g_object_ref (event);

  if (!g_hash_table_remove (self->events, event))
    {
      g_warning ("Event with uuid: %s not in %s", gcal_event_get_uid (event), G_OBJECT_TYPE_NAME (self));
      GCAL_RETURN ();
    }

  for (guint i = 0; i < self->week_rows->len; i++)
    {
      g_autoptr (GcalRange) row_range = NULL;
      GcalRangePosition position;
      GcalRangeOverlap overlap;
      GcalMonthViewRow *row;

      row = g_ptr_array_index (self->week_rows, i);
      row_range = gcal_month_view_row_get_range (row);

      overlap = gcal_range_calculate_overlap (row_range, gcal_event_get_range (event), &position);
      if (overlap != GCAL_RANGE_NO_OVERLAP)
        {
          gcal_month_view_row_remove_event (row, event);
        }
      else
        {
          if (position == GCAL_RANGE_AFTER)
            break;
        }
    }

  GCAL_EXIT;
}

static void
gcal_month_view_update_event (GcalTimelineSubscriber *subscriber,
                              GcalEvent              *old_event,
                              GcalEvent              *event)
{
  GCAL_ENTRY;

  g_assert (event != NULL);

  gcal_month_view_remove_event (subscriber, old_event);
  gcal_month_view_add_event (subscriber, event);

  GCAL_EXIT;
}

static void
gcal_timeline_subscriber_interface_init (GcalTimelineSubscriberInterface *iface)
{
  iface->get_range = gcal_month_view_get_range;
  iface->add_event = gcal_month_view_add_event;
  iface->update_event = gcal_month_view_update_event;
  iface->remove_event = gcal_month_view_remove_event;
}


/*
 * GcalView interface
 */

static GDateTime*
gcal_month_view_get_date (GcalView *view)
{
  GcalMonthView *self = GCAL_MONTH_VIEW (view);

  return self->date;
}

static void
gcal_month_view_set_date (GcalView  *view,
                          GDateTime *date)
{
  GcalMonthView *self;
  gboolean week_changed;

  GCAL_ENTRY;

  self = GCAL_MONTH_VIEW (view);

  week_changed = !self->date ||
                 !date ||
                 g_date_time_get_month (self->date) != g_date_time_get_month (date) ||
                 g_date_time_get_week_of_year (self->date) != g_date_time_get_week_of_year (date);

  if (!week_changed)
    GCAL_RETURN ();

#ifdef GCAL_ENABLE_TRACE
  {
    g_autofree gchar *new_date_string = g_date_time_format (date, "%x %X %z");
    GCAL_TRACE_MSG ("New date: %s", new_date_string);
  }
#endif

  update_week_ranges (self, date);
  update_row_visuals (self);

  gcal_timeline_subscriber_range_changed (GCAL_TIMELINE_SUBSCRIBER (view));

  GCAL_EXIT;
}

static void
gcal_month_view_clear_marks (GcalView *view)
{
  GcalMonthView *self;

  GCAL_ENTRY;

  self = GCAL_MONTH_VIEW (view);

  gcal_clear_date_time (&self->selection.start);
  gcal_clear_date_time (&self->selection.end);

  update_selection_range (self);

  GCAL_EXIT;
}

static GList*
gcal_month_view_get_children_by_uuid (GcalView              *view,
                                      GcalRecurrenceModType  mod,
                                      const gchar           *uuid)
{
  g_autoptr (GList) children = NULL;
  GcalMonthView *self;

  GCAL_ENTRY;

  self = GCAL_MONTH_VIEW (view);

  for (guint i = 0; i < self->week_rows->len; i++)
    {
      g_autoptr (GList) row_children = NULL;
      GcalMonthViewRow *row;

      row = g_ptr_array_index (self->week_rows, i);
      row_children = gcal_month_view_row_get_children_by_uuid (row, mod, uuid);

      children = g_list_concat (children, g_steal_pointer (&row_children));
    }

  GCAL_RETURN (g_steal_pointer (&children));
}

static GDateTime*
gcal_month_view_get_next_date (GcalView *view)
{
  GcalMonthView *self = GCAL_MONTH_VIEW (view);

  g_assert (self->date != NULL);
  return g_date_time_add_months (self->date, 1);
}


static GDateTime*
gcal_month_view_get_previous_date (GcalView *view)
{
  GcalMonthView *self = GCAL_MONTH_VIEW (view);

  g_assert (self->date != NULL);
  return g_date_time_add_months (self->date, -1);
}

static void
gcal_view_interface_init (GcalViewInterface *iface)
{
  iface->get_date = gcal_month_view_get_date;
  iface->set_date = gcal_month_view_set_date;
  iface->clear_marks = gcal_month_view_clear_marks;
  iface->get_children_by_uuid = gcal_month_view_get_children_by_uuid;
  iface->get_next_date = gcal_month_view_get_next_date;
  iface->get_previous_date = gcal_month_view_get_previous_date;
}


/*
 * GtkBuildable interface
 */

static GtkBuildableIface *parent_buildable_iface;

static void
gcal_month_view_add_child (GtkBuildable *buildable,
                           GtkBuilder   *builder,
                           GObject      *child,
                           const gchar  *type)
{
  GcalMonthView *self = GCAL_MONTH_VIEW (buildable);

  if (g_strcmp0 (type, "header") == 0)
    {
      self->header = GTK_WIDGET (child);
      gtk_widget_set_parent (self->header, GTK_WIDGET (self));
    }
  else
    {
      parent_buildable_iface->add_child (buildable, builder, child, type);
    }
}

static void
gtk_buildable_interface_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);

  iface->add_child = gcal_month_view_add_child;
}


/*
 * GtkWidget overrides
 */

static gboolean
gcal_month_view_focus (GtkWidget        *widget,
                       GtkDirectionType  direction)
{
  GtkRoot *root;
  GtkWidget *focused;
  gboolean forward_or_backward, is_event_widget, is_ancestor;

  root = gtk_widget_get_root (widget);
  focused = gtk_root_get_focus (root);

  is_event_widget = GCAL_IS_EVENT_WIDGET (focused);
  is_ancestor = (focused && gtk_widget_is_ancestor (focused, widget));

  forward_or_backward = direction == GTK_DIR_TAB_FORWARD || direction == GTK_DIR_TAB_BACKWARD;

  if ((is_event_widget && forward_or_backward) || !(is_ancestor || forward_or_backward))
    return FALSE;

  GTK_WIDGET_CLASS (gcal_month_view_parent_class)->focus (widget, direction);
  return TRUE;
}

static void
gcal_month_view_measure (GtkWidget      *widget,
                         GtkOrientation  orientation,
                         gint            for_size,
                         gint           *minimum,
                         gint           *natural,
                         gint           *minimum_baseline,
                         gint           *natural_baseline)
{
  GcalMonthView *self;
  gint natural_header_size;
  gint minimum_header_size;
  gint minimum_row_size = 0;
  gint natural_row_size = 0;

  self = GCAL_MONTH_VIEW (widget);

  gtk_widget_measure (self->header,
                      orientation,
                      for_size,
                      &minimum_header_size,
                      &natural_header_size,
                      NULL,
                      NULL);

  for (guint i = 0; i < self->week_rows->len; i++)
    {
      GtkWidget *row;
      gint row_minimum;
      gint row_natural;

      row = g_ptr_array_index (self->week_rows, i);

      gtk_widget_measure (row,
                          orientation,
                          for_size,
                          &row_minimum,
                          &row_natural,
                          NULL,
                          NULL);

      if (i == 0)
        {
          minimum_row_size = row_minimum;
          natural_row_size = row_natural;
        }
      else
        {
          minimum_row_size = MIN (minimum_row_size, row_minimum);
          natural_row_size = MIN (natural_row_size, row_natural);
        }
    }


  if (orientation == GTK_ORIENTATION_VERTICAL)
    {
      if (minimum)
        *minimum = minimum_header_size + minimum_row_size;

      if (natural)
        *natural = natural_header_size + natural_row_size;
    }
  else
    {
      if (minimum)
        *minimum = MAX(minimum_header_size, minimum_row_size);

      if (natural)
        *natural = MAX(natural_header_size, natural_row_size);
    }

  if (minimum_baseline)
    *minimum_baseline = -1;

  if (natural_baseline)
    *natural_baseline = -1;
}

static void
gcal_month_view_size_allocate (GtkWidget *widget,
                               gint       width,
                               gint       height,
                               gint       baseline)
{
  GcalMonthView *self;
  gdouble row_scroll_offset;
  gdouble row_height;
  gdouble y_offset;
  gint header_height;
  gint grid_height;

  self = GCAL_MONTH_VIEW (widget);

  gtk_widget_measure (self->header, GTK_ORIENTATION_VERTICAL, width, &header_height, NULL, NULL, NULL);
  gtk_widget_allocate (self->header, width, header_height, baseline, NULL);

  grid_height = height - header_height + 1;
  row_height = grid_height / (gdouble) N_ROWS_PER_PAGE;
  row_scroll_offset = self->row_offset * row_height;
  y_offset = header_height - row_scroll_offset - (grid_height * ((N_PAGES - 1) / 2.0));

  for (guint i = 0; i < self->week_rows->len; i++)
    {
      GtkAllocation row_allocation;
      GtkWidget *row;

      row = g_ptr_array_index (self->week_rows, i);

#define ROW_Y(_i) (row_height * (_i) + y_offset)

      row_allocation.x = 0;
      row_allocation.y = round (ROW_Y (i));
      row_allocation.width = width;
      row_allocation.height = round (ROW_Y (i + 1)) - row_allocation.y;

#undef ROW_Y

      gtk_widget_size_allocate (row, &row_allocation, baseline);
    }

  /* Overflow popover */
  if (gtk_widget_should_layout (self->overflow.popover))
    allocate_overflow_popover (self, width, height, baseline);
}

static void
gcal_month_view_snapshot (GtkWidget   *widget,
                          GtkSnapshot *snapshot)
{
  GcalMonthView *self;
  gint header_height;
  gint height;
  gint width;

  self = GCAL_MONTH_VIEW (widget);

  /* Snapshot rows with a clip */
  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_height (widget);
  header_height = gtk_widget_get_height (self->header);

  gtk_snapshot_push_clip (snapshot,
                          &GRAPHENE_RECT_INIT (0, header_height, width, height - header_height));
  for (guint i = 0; i < self->week_rows->len; i++)
    gtk_widget_snapshot_child (widget, g_ptr_array_index (self->week_rows, i), snapshot);

  gtk_snapshot_pop (snapshot);

  /* Snapshot header */
  gtk_widget_snapshot_child (widget, self->header, snapshot);

  /* Overflow popover */
  gtk_widget_snapshot_child (widget, self->overflow.popover, snapshot);
}


/*
 * GObject overrides
 */

static void
gcal_month_view_dispose (GObject *object)
{
  GcalMonthView *self = GCAL_MONTH_VIEW (object);

  GCAL_ENTRY;

  g_clear_pointer (&self->events, g_hash_table_destroy);

  g_clear_object (&self->kinetic_scroll_animation);
  g_clear_object (&self->row_offset_animation);

  g_clear_pointer (&self->overflow.popover, gtk_widget_unparent);
  g_clear_pointer (&self->header, gtk_widget_unparent);
  g_clear_pointer (&self->week_rows, g_ptr_array_unref);

  G_OBJECT_CLASS (gcal_month_view_parent_class)->dispose (object);

  GCAL_EXIT;
}

static void
gcal_month_view_finalize (GObject *object)
{
  GcalMonthView *self = (GcalMonthView *)object;

  gcal_clear_date_time (&self->selection.start);
  gcal_clear_date_time (&self->selection.end);
  gcal_clear_date_time (&self->date);
  g_clear_object (&self->context);

  G_OBJECT_CLASS (gcal_month_view_parent_class)->finalize (object);
}

static void
gcal_month_view_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GcalMonthView *self = GCAL_MONTH_VIEW (object);

  switch (prop_id)
    {
    case PROP_DATE:
      g_value_set_boxed (value, self->date);
      break;

    case PROP_CONTEXT:
      g_value_set_object (value, self->context);
      break;

    case PROP_TIME_DIRECTION:
      g_value_set_enum (value, GTK_ORIENTATION_VERTICAL);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_month_view_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GcalMonthView *self = GCAL_MONTH_VIEW (object);

  switch (prop_id)
    {
    case PROP_DATE:
      gcal_view_set_date (GCAL_VIEW (self), g_value_get_boxed (value));
      break;

    case PROP_CONTEXT:
      g_assert (self->context == NULL);
      self->context = g_value_dup_object (value);

      for (gint i = 0; i < N_TOTAL_ROWS; i++)
        gcal_month_view_row_set_context (g_ptr_array_index (self->week_rows, i), self->context);

      g_object_notify (object, "context");
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_month_view_class_init (GcalMonthViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gcal_month_view_dispose;
  object_class->finalize = gcal_month_view_finalize;
  object_class->get_property = gcal_month_view_get_property;
  object_class->set_property = gcal_month_view_set_property;

  widget_class->focus = gcal_month_view_focus;
  widget_class->measure = gcal_month_view_measure;
  widget_class->size_allocate = gcal_month_view_size_allocate;
  widget_class->snapshot = gcal_month_view_snapshot;

  g_object_class_override_property (object_class, PROP_CONTEXT, "context");
  g_object_class_override_property (object_class, PROP_DATE, "active-date");
  g_object_class_override_property (object_class, PROP_TIME_DIRECTION, "time-direction");

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/views/gcal-month-view.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalMonthView, motion_controller);

  gtk_widget_class_bind_template_child_full (widget_class, "label_0", FALSE, G_STRUCT_OFFSET (GcalMonthView, weekday_label[0]));
  gtk_widget_class_bind_template_child_full (widget_class, "label_1", FALSE, G_STRUCT_OFFSET (GcalMonthView, weekday_label[1]));
  gtk_widget_class_bind_template_child_full (widget_class, "label_2", FALSE, G_STRUCT_OFFSET (GcalMonthView, weekday_label[2]));
  gtk_widget_class_bind_template_child_full (widget_class, "label_3", FALSE, G_STRUCT_OFFSET (GcalMonthView, weekday_label[3]));
  gtk_widget_class_bind_template_child_full (widget_class, "label_4", FALSE, G_STRUCT_OFFSET (GcalMonthView, weekday_label[4]));
  gtk_widget_class_bind_template_child_full (widget_class, "label_5", FALSE, G_STRUCT_OFFSET (GcalMonthView, weekday_label[5]));
  gtk_widget_class_bind_template_child_full (widget_class, "label_6", FALSE, G_STRUCT_OFFSET (GcalMonthView, weekday_label[6]));

  gtk_widget_class_bind_template_callback (widget_class, on_click_gesture_pressed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_click_gesture_released_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_discrete_scroll_controller_scroll_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_motion_controller_motion_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_scroll_controller_scroll_begin_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_scroll_controller_scroll_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_scroll_controller_scroll_end_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_scroll_controller_decelerate_cb);

  gtk_widget_class_set_css_name (widget_class, "calendar-view");
}

static void
gcal_month_view_init (GcalMonthView *self)
{
  g_autoptr (GDateTime) now = NULL;

  self->events = g_hash_table_new_full (g_direct_hash, g_direct_equal, g_object_unref, NULL);

  gtk_widget_init_template (GTK_WIDGET (self));
  update_weekday_labels (self);

  self->week_rows = g_ptr_array_new_full (N_TOTAL_ROWS, (GDestroyNotify) gtk_widget_unparent);
  for (gint i = 0; i < N_TOTAL_ROWS; i++)
    {
      GtkWidget *row = gcal_month_view_row_new ();
      g_signal_connect (row, "event-activated", G_CALLBACK (on_event_widget_activated_cb), self);
      g_signal_connect (row, "show-overflow", G_CALLBACK (on_month_row_show_overflow_cb), self);
      gtk_widget_set_parent (row, GTK_WIDGET (self));
      g_ptr_array_add (self->week_rows, row);
    }

  gtk_widget_insert_before (self->header, GTK_WIDGET (self), NULL);

  /* Overflow popover */
  self->overflow.popover = gcal_month_popover_new ();
  g_object_bind_property (self, "context", self->overflow.popover, "context", G_BINDING_DEFAULT);
  g_signal_connect (self->overflow.popover, "event-activated", G_CALLBACK (on_month_popover_event_activated_cb), self);

  gtk_widget_set_parent (self->overflow.popover, GTK_WIDGET (self));

  now = g_date_time_new_now_local ();
  gcal_view_set_date (GCAL_VIEW (self), now);
}
