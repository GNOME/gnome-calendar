/*
 * gcal-dazzling-month-view.c
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

#define G_LOG_DOMAIN "GcalDazzlingMonthView"

#include "config.h"
#include "gcal-context.h"
#include "gcal-dazzling-month-view.h"
#include "gcal-debug.h"
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

// N_PAGES must be an odd number
G_STATIC_ASSERT (N_PAGES % 2 != 0);

struct _GcalDazzlingMonthView
{
  GtkWidget          parent;

  GtkWidget          *header;
  GtkWidget          *month_label;
  GtkWidget          *year_label;
  GtkWidget          *weekday_label[7];

  GPtrArray          *week_rows;
  GHashTable         *events;

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


G_DEFINE_FINAL_TYPE_WITH_CODE (GcalDazzlingMonthView, gcal_dazzling_month_view, GTK_TYPE_WIDGET,
                               G_IMPLEMENT_INTERFACE (GCAL_TYPE_TIMELINE_SUBSCRIBER, gcal_timeline_subscriber_interface_init)
                               G_IMPLEMENT_INTERFACE (GCAL_TYPE_VIEW, gcal_view_interface_init)
                               G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, gtk_buildable_interface_init))

enum
{
  PROP_0,
  PROP_CONTEXT,
  PROP_DATE,
  N_PROPS,
};


/*
 * Auxiliary methods
 */

static void
update_header_labels (GcalDazzlingMonthView *self)
{
  gchar year_str[10] = { 0, };

  g_snprintf (year_str, 10, "%d", g_date_time_get_year (self->date));

  gtk_label_set_label (GTK_LABEL (self->month_label), gcal_get_month_name (g_date_time_get_month (self->date) - 1));
  gtk_label_set_label (GTK_LABEL (self->year_label), year_str);
}

static inline void
update_weekday_labels (GcalDazzlingMonthView *self)
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
cancel_deceleration (GcalDazzlingMonthView *self)
{
  if (self->kinetic_scroll_animation == 0)
    return;

  adw_animation_pause (self->kinetic_scroll_animation);
  g_clear_object (&self->kinetic_scroll_animation);
}

static void
cancel_row_offset_animation (GcalDazzlingMonthView *self)
{
  if (!self->row_offset_animation)
    return;

  adw_animation_pause (self->row_offset_animation);
  g_clear_object (&self->row_offset_animation);
}

static void
update_active_date (GcalDazzlingMonthView *self)
{
  g_autoptr (GcalRange) top_row_range = NULL;
  GtkWidget *top_row;

  top_row = g_ptr_array_index (self->week_rows, N_ROWS_PER_PAGE * (N_PAGES - 1) / 2);
  top_row_range = gcal_month_view_row_get_range (GCAL_MONTH_VIEW_ROW (top_row));
  g_assert (top_row_range != NULL);

  gcal_view_set_date (GCAL_VIEW (self), gcal_range_get_start (top_row_range));
  g_object_notify (G_OBJECT (self), "active-date");
}

static void
move_bottom_row_to_top (GcalDazzlingMonthView *self)
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
                                   GCAL_RANGE_DATE_ONLY);

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
}

static void
move_top_row_to_bottom (GcalDazzlingMonthView *self)
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
                                   GCAL_RANGE_DATE_ONLY);

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
}

static inline void
dump_row_ranges (GcalDazzlingMonthView *self)
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
update_week_ranges (GcalDazzlingMonthView *self,
                    GDateTime             *new_date)
{
  g_autoptr (GcalRange) current_range = NULL;
  g_autoptr (GDateTime) current_date = NULL;
  gint n_weeks_before;

  /*
   * The current date corresponds to the week of the first
   * visible row.
   */
  n_weeks_before = N_ROWS_PER_PAGE * (N_PAGES - 1) / 2;

  current_range = gcal_timeline_subscriber_get_range (GCAL_TIMELINE_SUBSCRIBER (self));
  current_date = g_steal_pointer (&self->date);

  gcal_set_date_time (&self->date, new_date);

  if (gcal_range_contains_datetime (current_range, new_date))
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
          week_end = gcal_date_time_get_end_of_week (date);
          range = gcal_range_new (week_start, week_end, GCAL_RANGE_DATE_ONLY);

          row = g_ptr_array_index (self->week_rows, i);
          gcal_month_view_row_set_range (row, range);
        }
    }

  dump_row_ranges (self);
}

static inline gint
get_grid_height (GcalDazzlingMonthView *self)
{
  return gtk_widget_get_height (GTK_WIDGET (self)) - gtk_widget_get_height (self->header);
}

static void
offset_and_shuffle_rows (GcalDazzlingMonthView *self,
                         gdouble                dy)
{
  gdouble row_height;
  gint grid_height;

  GCAL_ENTRY;

  grid_height = get_grid_height (self);
  row_height = grid_height / (gdouble) N_ROWS_PER_PAGE;
  self->row_offset += dy / row_height;

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

      update_active_date (self);
      dump_row_ranges (self);
    }

  gtk_widget_queue_allocate (GTK_WIDGET (self));

  GCAL_EXIT;
}

static void
animate_row_offset_cb (gdouble  value,
                       gpointer user_data)
{
  GcalDazzlingMonthView *self = GCAL_DAZZLING_MONTH_VIEW (user_data);

  self->row_offset = value;
  gtk_widget_queue_allocate (GTK_WIDGET (self));
}

static void
on_row_offset_animation_done (AdwAnimation          *animation,
                              GcalDazzlingMonthView *self)
{
  gtk_widget_remove_css_class (GTK_WIDGET (self), "scrolling");
  cancel_row_offset_animation (self);
}

static void
snap_to_top_row (GcalDazzlingMonthView *self)
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


/*
 * Callbacks
 */

static void
on_event_widget_activated_cb (GcalMonthViewRow      *row,
                              GcalEventWidget       *event_widget,
                              GcalDazzlingMonthView *self)
{
  gcal_view_event_activated (GCAL_VIEW (self), event_widget);
}

static void
on_scroll_controller_scroll_begin_cb (GtkEventControllerScroll *scroll_controller,
                                      GcalDazzlingMonthView    *self)
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
                                GcalDazzlingMonthView    *self)
{
  GdkEvent *current_event;

  GCAL_ENTRY;

  current_event = gtk_event_controller_get_current_event (GTK_EVENT_CONTROLLER (scroll_controller));

  switch (gdk_scroll_event_get_direction (current_event))
    {
    case GDK_SCROLL_SMOOTH:
      cancel_row_offset_animation (self);
      cancel_deceleration (self);
      offset_and_shuffle_rows (self, dy);
      GCAL_RETURN (GDK_EVENT_STOP);

    default:
      GCAL_RETURN (GDK_EVENT_PROPAGATE);
    }
}

static gboolean
on_discrete_scroll_controller_scroll_cb (GtkEventControllerScroll *scroll_controller,
                                         gdouble                   dx,
                                         gdouble                   dy,
                                         GcalDazzlingMonthView    *self)
{
  GdkEvent *current_event;

  GCAL_ENTRY;

  current_event = gtk_event_controller_get_current_event (GTK_EVENT_CONTROLLER (scroll_controller));

  switch (gdk_scroll_event_get_direction (current_event))
    {
    case GDK_SCROLL_UP:
      move_bottom_row_to_top (self);
      break;

    case GDK_SCROLL_DOWN:
      move_top_row_to_bottom (self);
      break;

    default:
      GCAL_RETURN (GDK_EVENT_PROPAGATE);
    }

  gtk_widget_remove_css_class (GTK_WIDGET (self), "scrolling");
  cancel_row_offset_animation (self);
  cancel_deceleration (self);
  update_active_date (self);
  dump_row_ranges (self);

  self->row_offset = 0.0;

  gtk_widget_queue_allocate (GTK_WIDGET (self));

  GCAL_RETURN (GDK_EVENT_STOP);
}

static void
on_scroll_controller_scroll_end_cb (GtkEventControllerScroll *scroll_controller,
                                    GcalDazzlingMonthView    *self)
{
  GCAL_ENTRY;

  snap_to_top_row (self);

  GCAL_EXIT;
}

static void
decelerate_scroll_cb (gdouble  value,
                      gpointer user_data)
{
  GcalDazzlingMonthView *self = GCAL_DAZZLING_MONTH_VIEW (user_data);
  gdouble dy;

  dy = self->last_velocity - value;
  self->last_velocity = value;

  if (fabs (value) > (get_grid_height (self) / (gdouble) N_ROWS_PER_PAGE / 5.0))
    offset_and_shuffle_rows (self, dy);
  else
    adw_animation_skip (self->kinetic_scroll_animation);
}

static void
on_kinetic_scroll_done_cb (AdwAnimation          *animation,
                           GcalDazzlingMonthView *self)
{
  GCAL_ENTRY;

  snap_to_top_row (self);

  GCAL_EXIT;
}

static void
on_scroll_controller_decelerate_cb (GtkEventControllerScroll *scroll_controller,
                                    gdouble                   velocity_x,
                                    gdouble                   velocity_y,
                                    GcalDazzlingMonthView    *self)
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

  g_signal_connect (self->kinetic_scroll_animation,
                    "done",
                    G_CALLBACK (on_kinetic_scroll_done_cb),
                    self);

  adw_animation_play (self->kinetic_scroll_animation);

  GCAL_EXIT;
}


/*
 * GcalTimelineSubscriber iface
 */

static GcalRange*
gcal_dazzling_month_view_get_range (GcalTimelineSubscriber *subscriber)
{
  g_autoptr (GcalRange) first_row_range = NULL;
  g_autoptr (GcalRange) last_row_range = NULL;
  GcalDazzlingMonthView *self;

  self = GCAL_DAZZLING_MONTH_VIEW (subscriber);

  first_row_range = gcal_month_view_row_get_range (g_ptr_array_index (self->week_rows, 0));
  last_row_range = gcal_month_view_row_get_range (g_ptr_array_index (self->week_rows,
                                                                     self->week_rows->len - 1));

  return gcal_range_union (first_row_range, last_row_range);
}

static void
gcal_dazzling_month_view_add_event (GcalTimelineSubscriber *subscriber,
                                    GcalEvent              *event)
{
  GcalDazzlingMonthView *self;

  GCAL_ENTRY;

  g_assert (event != NULL);

  self = GCAL_DAZZLING_MONTH_VIEW (subscriber);

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
gcal_dazzling_month_view_update_event (GcalTimelineSubscriber *subscriber,
                                       GcalEvent              *event)
{
}

static void
gcal_dazzling_month_view_remove_event (GcalTimelineSubscriber *subscriber,
                                       GcalEvent              *event)
{
  g_autoptr (GcalEvent) owned_event = NULL;
  GcalDazzlingMonthView *self;

  GCAL_ENTRY;

  g_assert (event != NULL);

  self = GCAL_DAZZLING_MONTH_VIEW (subscriber);

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
gcal_timeline_subscriber_interface_init (GcalTimelineSubscriberInterface *iface)
{
  iface->get_range = gcal_dazzling_month_view_get_range;
  iface->add_event = gcal_dazzling_month_view_add_event;
  iface->update_event = gcal_dazzling_month_view_update_event;
  iface->remove_event = gcal_dazzling_month_view_remove_event;
}


/*
 * GcalView interface
 */

static GDateTime*
gcal_dazzling_month_view_get_date (GcalView *view)
{
  GcalDazzlingMonthView *self = GCAL_DAZZLING_MONTH_VIEW (view);

  return self->date;
}

static void
gcal_dazzling_month_view_set_date (GcalView  *view,
                                   GDateTime *date)
{
  GcalDazzlingMonthView *self;
  gboolean month_changed;

  GCAL_ENTRY;

  self = GCAL_DAZZLING_MONTH_VIEW (view);

  month_changed = !self->date ||
                  !date ||
                  g_date_time_get_month (self->date) != g_date_time_get_month (date) ||
                  g_date_time_get_year (self->date) != g_date_time_get_year (date);

  if (!month_changed)
    GCAL_RETURN ();

#ifdef GCAL_ENABLE_TRACE
  {
    g_autofree gchar *new_date_string = g_date_time_format (date, "%x %X %z");
    GCAL_TRACE_MSG ("New date: %s", new_date_string);
  }
#endif

  update_week_ranges (self, date);
  update_header_labels (self);

  gcal_timeline_subscriber_range_changed (GCAL_TIMELINE_SUBSCRIBER (view));

  GCAL_EXIT;
}

static void
gcal_dazzling_month_view_clear_marks (GcalView *view)
{
  GCAL_TODO ("gcal_dazzling_month_view_clear_marks");
}

static GList*
gcal_dazzling_month_view_get_children_by_uuid (GcalView              *view,
                                               GcalRecurrenceModType  mod,
                                               const gchar           *uuid)
{
  return filter_children_by_uid_and_modtype (GTK_WIDGET (view), mod, uuid);
}

static GDateTime*
gcal_dazzling_month_view_get_next_date (GcalView *view)
{
  GcalDazzlingMonthView *self = GCAL_DAZZLING_MONTH_VIEW (view);

  g_assert (self->date != NULL);
  return g_date_time_add_months (self->date, 1);
}


static GDateTime*
gcal_dazzling_month_view_get_previous_date (GcalView *view)
{
  GcalDazzlingMonthView *self = GCAL_DAZZLING_MONTH_VIEW (view);

  g_assert (self->date != NULL);
  return g_date_time_add_months (self->date, -1);
}

static void
gcal_view_interface_init (GcalViewInterface *iface)
{
  iface->get_date = gcal_dazzling_month_view_get_date;
  iface->set_date = gcal_dazzling_month_view_set_date;
  iface->clear_marks = gcal_dazzling_month_view_clear_marks;
  iface->get_children_by_uuid = gcal_dazzling_month_view_get_children_by_uuid;
  iface->get_next_date = gcal_dazzling_month_view_get_next_date;
  iface->get_previous_date = gcal_dazzling_month_view_get_previous_date;
}


/*
 * GtkBuildable interface
 */

static GtkBuildableIface *parent_buildable_iface;

static void
gcal_dazzling_month_view_add_child (GtkBuildable *buildable,
                           GtkBuilder   *builder,
                           GObject      *child,
                           const gchar  *type)
{
  GcalDazzlingMonthView *self = GCAL_DAZZLING_MONTH_VIEW (buildable);

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

  iface->add_child = gcal_dazzling_month_view_add_child;
}


/*
 * GtkWidget overrides
 */

static void
gcal_dazzling_month_view_measure (GtkWidget      *widget,
                                  GtkOrientation  orientation,
                                  gint            for_size,
                                  gint           *minimum,
                                  gint           *natural,
                                  gint           *minimum_baseline,
                                  gint           *natural_baseline)
{
  GcalDazzlingMonthView *self;
  gint natural_header_size;
  gint minimum_header_size;
  gint minimum_row_size;
  gint natural_row_size;

  self = GCAL_DAZZLING_MONTH_VIEW (widget);

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

  if (minimum)
    *minimum = minimum_header_size + minimum_row_size;

  if (natural)
    *natural = natural_header_size + natural_row_size;

  if (minimum_baseline)
    *minimum_baseline = -1;

  if (natural_baseline)
    *natural_baseline = -1;
}

static void
gcal_dazzling_month_view_size_allocate (GtkWidget *widget,
                                        gint       width,
                                        gint       height,
                                        gint       baseline)
{
  GcalDazzlingMonthView *self;
  gdouble row_scroll_offset;
  gdouble row_height;
  gdouble y_offset;
  gint header_height;
  gint grid_height;

  self = GCAL_DAZZLING_MONTH_VIEW (widget);

  gtk_widget_measure (self->header, GTK_ORIENTATION_VERTICAL, width, &header_height, NULL, NULL, NULL);
  gtk_widget_allocate (self->header, width, header_height, baseline, NULL);

  grid_height = height - header_height;
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
}

static void
gcal_dazzling_month_view_snapshot (GtkWidget   *widget,
                                   GtkSnapshot *snapshot)
{
  GcalDazzlingMonthView *self;
  gint header_height;
  gint height;
  gint width;

  self = GCAL_DAZZLING_MONTH_VIEW (widget);

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
}


/*
 * GObject overrides
 */

static void
gcal_dazzling_month_view_dispose (GObject *object)
{
  GcalDazzlingMonthView *self = GCAL_DAZZLING_MONTH_VIEW (object);

  GCAL_ENTRY;

  g_clear_pointer (&self->events, g_hash_table_destroy);

  g_clear_object (&self->kinetic_scroll_animation);
  g_clear_object (&self->row_offset_animation);

  g_clear_pointer (&self->header, gtk_widget_unparent);
  g_clear_pointer (&self->week_rows, g_ptr_array_unref);

  G_OBJECT_CLASS (gcal_dazzling_month_view_parent_class)->dispose (object);

  GCAL_EXIT;
}

static void
gcal_dazzling_month_view_finalize (GObject *object)
{
  GcalDazzlingMonthView *self = (GcalDazzlingMonthView *)object;

  gcal_clear_date_time (&self->date);
  g_clear_object (&self->context);

  G_OBJECT_CLASS (gcal_dazzling_month_view_parent_class)->finalize (object);
}

static void
gcal_dazzling_month_view_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  GcalDazzlingMonthView *self = GCAL_DAZZLING_MONTH_VIEW (object);

  switch (prop_id)
    {
    case PROP_DATE:
      g_value_set_boxed (value, self->date);
      break;

    case PROP_CONTEXT:
      g_value_set_object (value, self->context);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_dazzling_month_view_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  GcalDazzlingMonthView *self = GCAL_DAZZLING_MONTH_VIEW (object);

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
gcal_dazzling_month_view_class_init (GcalDazzlingMonthViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gcal_dazzling_month_view_dispose;
  object_class->finalize = gcal_dazzling_month_view_finalize;
  object_class->get_property = gcal_dazzling_month_view_get_property;
  object_class->set_property = gcal_dazzling_month_view_set_property;

  widget_class->measure = gcal_dazzling_month_view_measure;
  widget_class->size_allocate = gcal_dazzling_month_view_size_allocate;
  widget_class->snapshot = gcal_dazzling_month_view_snapshot;

  g_object_class_override_property (object_class, PROP_CONTEXT, "context");
  g_object_class_override_property (object_class, PROP_DATE, "active-date");

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/views/gcal-dazzling-month-view.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalDazzlingMonthView, month_label);
  gtk_widget_class_bind_template_child (widget_class, GcalDazzlingMonthView, year_label);

  gtk_widget_class_bind_template_child_full (widget_class, "label_0", FALSE, G_STRUCT_OFFSET (GcalDazzlingMonthView, weekday_label[0]));
  gtk_widget_class_bind_template_child_full (widget_class, "label_1", FALSE, G_STRUCT_OFFSET (GcalDazzlingMonthView, weekday_label[1]));
  gtk_widget_class_bind_template_child_full (widget_class, "label_2", FALSE, G_STRUCT_OFFSET (GcalDazzlingMonthView, weekday_label[2]));
  gtk_widget_class_bind_template_child_full (widget_class, "label_3", FALSE, G_STRUCT_OFFSET (GcalDazzlingMonthView, weekday_label[3]));
  gtk_widget_class_bind_template_child_full (widget_class, "label_4", FALSE, G_STRUCT_OFFSET (GcalDazzlingMonthView, weekday_label[4]));
  gtk_widget_class_bind_template_child_full (widget_class, "label_5", FALSE, G_STRUCT_OFFSET (GcalDazzlingMonthView, weekday_label[5]));
  gtk_widget_class_bind_template_child_full (widget_class, "label_6", FALSE, G_STRUCT_OFFSET (GcalDazzlingMonthView, weekday_label[6]));

  gtk_widget_class_bind_template_callback (widget_class, on_discrete_scroll_controller_scroll_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_scroll_controller_scroll_begin_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_scroll_controller_scroll_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_scroll_controller_scroll_end_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_scroll_controller_decelerate_cb);

  gtk_widget_class_set_css_name (widget_class, "calendar-view");
}

static void
gcal_dazzling_month_view_init (GcalDazzlingMonthView *self)
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
      gtk_widget_set_parent (row, GTK_WIDGET (self));
      g_ptr_array_add (self->week_rows, row);
    }

  gtk_widget_insert_after (self->header, GTK_WIDGET (self), NULL);

  now = g_date_time_new_now_local ();
  gcal_view_set_date (GCAL_VIEW (self), now);
}