/* gcal-week-header.c
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

#include "config.h"

#define G_LOG_DOMAIN "GcalWeekHeader"

#include "gcal-application.h"
#include "gcal-context.h"
#include "gcal-clock.h"
#include "gcal-debug.h"
#include "gcal-event-widget.h"
#include "gcal-gui-utils.h"
#include "gcal-utils.h"
#include "gcal-view-private.h"
#include "gcal-week-header.h"
#include "gcal-week-view.h"
#include "gcal-week-view-common.h"

#include <glib/gi18n.h>
#include <string.h>
#include <math.h>

/* WeatherInfoDay:
 * @winfo: (nullable): Holds weather information for this week-day. All other fields are only valid if this one is not %NULL.
 * @icon_buf: (nullable): Buffered weather icon.
 * @x: X-position of weather indicators.
 * @y: Y-positon of weather indicators.
 * @width: Width of weather indicators.
 * @height: Height of weather indicators.
 *
 * Structure represents a weather indicator for a single day.
 *
 * Location information are stored for mouse-over events.
 */
typedef struct
{
  GcalWeatherInfo    *winfo;    /* owned */
  GtkIconPaintable   *icon_buf; /* owned */
} WeatherInfoDay;

typedef struct
{
  GtkWidget          *day_number_label;
  GtkWidget          *weekday_name_label;
} WeekdayHeader;

typedef struct
{
  GcalWeekHeader     *self;
  GcalEvent          *event;
  gint                drop_cell;
} DropData;

struct _GcalWeekHeader
{
  GtkWidget           parent;

  GtkGrid            *grid;
  GtkWidget          *scrolledwindow;
  GtkEventController *motion_controller;
  GtkBox             *weekdays_box;
  GtkWidget          *main_box;

  GcalContext        *context;

  /*
   * Stores the events as they come from the week-view
   * The list will later be iterated after the active date is changed
   * and the events will be placed
   */
  GList              *events[7];
  GtkWidget          *overflow_label[7];
  WeekdayHeader       weekday_header[7];

  gint                first_weekday;

  /*
   * Used for checking if the header is in collapsed state or expand state
   * false is collapse state true is expand state
   */
  gboolean            can_expand;
  gboolean            expanded;

  GDateTime          *active_date;

  struct {
    gint              start;
    gint              end;
    GtkWidget        *widget;
  } selection;

  struct {
    gint              cell;
    GtkWidget        *widget;
  } dnd;

  /* Array of nullable weather infos for each day, starting with Sunday. */
  WeatherInfoDay      weather_infos[7];
};

typedef enum
{
  UP,
  DOWN
} MoveDirection;

enum
{
  PROP_0,
  PROP_CAN_EXPAND,
  PROP_EXPANDED,
  N_PROPS,
};

enum
{
  EVENT_ACTIVATED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };
static GParamSpec *properties [N_PROPS];

G_DEFINE_TYPE (GcalWeekHeader, gcal_week_header, GTK_TYPE_WIDGET);


/* WeatherInfoDay methods */

/* wid_clear:
 *
 * Drops all internal references and resets
 * geometric information.
 */
static inline void
wid_clear (WeatherInfoDay *wid)
{
   g_return_if_fail (wid != NULL);

  g_clear_object (&wid->winfo);
  g_clear_object (&wid->icon_buf);
}

static gint
add_weather_infos (GcalWeekHeader *self,
                   GPtrArray      *weather_infos)
{
  g_autoptr (GDateTime) week_start_dt = NULL;
  GDate week_start;
  gint consumed = 0;
  guint i;

  if (!self->active_date)
    return 0;

  week_start_dt = gcal_date_time_get_start_of_week (self->active_date);
  g_date_set_dmy (&week_start,
                  g_date_time_get_day_of_month (week_start_dt),
                  g_date_time_get_month (week_start_dt),
                  g_date_time_get_year (week_start_dt));

  for (i = 0; weather_infos && i < weather_infos->len; i++)
    {
      GcalWeatherInfo *gwi; /* unowned */
      GDate gwi_date;
      gint day_diff;

      gwi = g_ptr_array_index (weather_infos, i);

      gcal_weather_info_get_date (gwi, &gwi_date);

      day_diff = g_date_days_between (&week_start, &gwi_date);
      if (day_diff >= 0 && day_diff < G_N_ELEMENTS (self->weather_infos))
        {
          wid_clear (&self->weather_infos[day_diff]);
          self->weather_infos[day_diff].winfo = g_object_ref (gwi);
          consumed++;
        }
    }

  if (consumed > 0)
    gtk_widget_queue_draw (GTK_WIDGET (self));

  return consumed;
}

static void
clear_weather_infos (GcalWeekHeader *self)
{
  g_return_if_fail (GCAL_IS_WEEK_HEADER (self));

  for (gint i = 0; i < G_N_ELEMENTS (self->weather_infos); i++)
    wid_clear (&self->weather_infos[i]);

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
update_weather_infos (GcalWeekHeader *self)
{
  GPtrArray *weather_infos;
  GcalWeatherService *service;

  g_return_if_fail (GCAL_IS_WEEK_HEADER (self));

  clear_weather_infos (self);

  g_assert (self->context);
  service = gcal_context_get_weather_service (self->context);

  weather_infos = gcal_weather_service_get_weather_infos (service);
  add_weather_infos (self, weather_infos);
}


/* Event activation methods */
static void
on_event_widget_activated (GcalEventWidget *widget,
                           GcalWeekHeader  *self)
{
  g_signal_emit (self, signals[EVENT_ACTIVATED], 0, widget);
}


static inline void
setup_event_widget (GcalWeekHeader *self,
                    GtkWidget      *widget)
{
  g_signal_connect_object (widget, "activate", G_CALLBACK (on_event_widget_activated), self, 0);
}

static inline void
destroy_event_widget (GcalWeekHeader *self,
                      GtkWidget      *widget)
{
  g_signal_handlers_disconnect_by_func (widget, on_event_widget_activated, self);
  gtk_grid_remove (self->grid, widget);
}

/* Auxiliary methods */
static void
on_button_pressed (GtkGestureClick *click_gesture,
                   gint             n_press,
                   gdouble          x,
                   gdouble          y,
                   GcalWeekHeader  *self)
{
  gboolean ltr;
  gdouble column_width;
  gint column;
  gint width;

  ltr = gtk_widget_get_direction (GTK_WIDGET (self)) != GTK_TEXT_DIR_RTL;
  width = gtk_widget_get_width (self->scrolledwindow);
  column_width = width / 7.0;
  column = ltr ? (x / column_width) : (7  - x / column_width);

  self->selection.start = column;
  self->selection.end = column;

  gtk_widget_set_visible (self->selection.widget, TRUE);

  gtk_event_controller_set_propagation_phase (self->motion_controller,
                                              GTK_PHASE_BUBBLE);
}

static void
on_motion_notify (GtkEventControllerMotion *motion_event,
                  gdouble                   x,
                  gdouble                   y,
                  GcalWeekHeader           *self)
{
  gboolean ltr;
  gdouble column_width;
  gint column;
  gint width;

  ltr = gtk_widget_get_direction (GTK_WIDGET (self)) != GTK_TEXT_DIR_RTL;
  width = gtk_widget_get_width (self->scrolledwindow);
  column_width = width / 7.0;
  column = ltr ? (x / column_width) : (7  - x / column_width);

  self->selection.end = column;

  gtk_widget_queue_allocate (GTK_WIDGET (self));
}

static void
on_button_released (GtkGestureClick *click_gesture,
                    gint             n_press,
                    gdouble          x,
                    gdouble          y,
                    GcalWeekHeader  *self)
{
  g_autoptr (GDateTime) selection_start = NULL;
  g_autoptr (GDateTime) selection_end = NULL;
  g_autoptr (GDateTime) week_start = NULL;
  g_autoptr (GcalRange) range = NULL;
  graphene_point_t out;
  GtkWidget *weekview;
  gboolean ltr;
  gdouble column_width;
  gdouble local_x;
  gint column;
  gint width;
  gint start;
  gint end;

  ltr = gtk_widget_get_direction (GTK_WIDGET (self)) != GTK_TEXT_DIR_RTL;
  width = gtk_widget_get_width (self->scrolledwindow);
  column_width = width / 7.0;
  column = ltr ? (x / column_width) : (7  - x / column_width);

  self->selection.end = column;
  gtk_widget_queue_allocate (GTK_WIDGET (self));

  /* Fake the week view's event so we can control the X and Y values */
  weekview = gtk_widget_get_ancestor (GTK_WIDGET (self), GCAL_TYPE_WEEK_VIEW);

  start = self->selection.start;
  end = self->selection.end;

  if (start > end)
    {
      start = start + end;
      end = start - end;
      start = start - end;
    }

  week_start = gcal_date_time_get_start_of_week (self->active_date);
  selection_start = g_date_time_add_days (week_start, start);
  selection_end = end == start ? g_date_time_ref (selection_start) : g_date_time_add_days (week_start, end + 1);

  local_x = ltr ? (column_width * (column + 0.5)) : (width - column_width * (column + 0.5));

  if (!gtk_widget_compute_point (GTK_WIDGET (self),
                                 weekview,
                                 &GRAPHENE_POINT_INIT (local_x, gtk_widget_get_height (GTK_WIDGET (self))),
                                 &out))
    g_assert_not_reached ();

  range = gcal_range_new (selection_start, selection_end, GCAL_RANGE_DATE_ONLY);
  gcal_view_create_event (GCAL_VIEW (weekview), range, out.x, out.y);

  gtk_event_controller_set_propagation_phase (self->motion_controller, GTK_PHASE_NONE);
}

static void
on_weather_update (GcalWeatherService *weather_service,
                   GcalWeekHeader     *self)
{
  g_assert (GCAL_IS_WEATHER_SERVICE (weather_service));
  g_assert (GCAL_IS_WEEK_HEADER (self));

  update_weather_infos (self);
}

static GcalEvent*
get_event_by_uuid (GcalWeekHeader *self,
                   const gchar    *uuid)
{
  gint weekday;

  for (weekday = 0; weekday < 7; weekday++)
    {
      GList *l;

      for (l = self->events[weekday]; l != NULL; l = l->next)
        {
          if (g_strcmp0 (gcal_event_get_uid (l->data), uuid) == 0)
            return l->data;
        }
    }

  return NULL;
}

static inline gint
get_today_column (GcalWeekHeader *self)
{
  g_autoptr (GDateTime) week_start = NULL;
  g_autoptr (GDateTime) today = NULL;
  gint days_diff;

  today = g_date_time_new_now_local ();
  week_start = gcal_date_time_get_start_of_week (self->active_date);
  days_diff = g_date_time_difference (today, week_start) / G_TIME_SPAN_DAY;

  /* Today is out of range */
  if (g_date_time_compare (today, week_start) < 0 || days_diff > 7)
    return -1;

  return days_diff;
}

static gint
compare_events_by_length (GcalEvent *event1,
                          GcalEvent *event2)
{
  /* Multiday events should come before single day events */
  if (gcal_event_is_multiday (event1) != gcal_event_is_multiday (event2))
    return gcal_event_is_multiday (event2) - gcal_event_is_multiday (event1);

  /* Compare with respect to start day */
  return gcal_event_compare (event1, event2);
}

static gint
add_event_to_weekday (GcalWeekHeader *self,
                      GcalEvent      *event,
                      gint            weekday)
{
  GList *l;

  l = self->events[weekday];
  l = g_list_insert_sorted (l, event, (GCompareFunc) compare_events_by_length);

  self->events[weekday] = l;

  return g_list_index (l, event);
}

static gboolean
is_event_visible (GcalWeekHeader *self,
                  gint            weekday,
                  gint            position)
{
  gboolean show_label;

  if (self->expanded)
    return TRUE;

  show_label = g_list_length (self->events[weekday]) > 3;

  return show_label ? position < 2 : position < 3;
}

/* Grid management */
static void
update_overflow (GcalWeekHeader *self)
{
  gboolean show_expand;
  gint i;

  show_expand = FALSE;

  for (i = 0; i < 7; i++)
    {
      GtkWidget *label;
      gboolean show_label;
      gint n_events;

      n_events = g_list_length (self->events[i]);
      show_label = n_events > 3;
      label = self->overflow_label[i];

      show_expand = show_expand || show_label;

      if (show_label && !self->expanded)
        {
          gchar *text;

          text = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE, "Other event", "Other %d events", n_events - 2), n_events - 2);

          /* TODO: use a button and show an overflow popover */
          if (!label)
            {
              label = gtk_label_new ("");
              gtk_grid_attach (self->grid,
                               label,
                               i,
                               3,
                               1,
                               1);

              self->overflow_label[i] = label;
            }

          gtk_label_set_label (GTK_LABEL (label), text);
          gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
          g_free (text);
        }
      else if (label)
        {
          gtk_grid_remove (self->grid, label);
          self->overflow_label[i] = NULL;
        }
    }

  if (self->can_expand != show_expand)
    {
      self->can_expand = show_expand;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CAN_EXPAND]);
    }
}

static void
merge_events (GcalWeekHeader *self,
              GtkWidget      *event,
              GtkWidget      *to_be_removed)
{
  GtkLayoutManager *layout_manager;
  GtkLayoutChild *layout_child;
  GDateTime *end_date;
  gint deleted_width, current_width;

  /* Setup the new end date of the merged event */
  end_date = gcal_event_widget_get_date_end (GCAL_EVENT_WIDGET (to_be_removed));
  gcal_event_widget_set_date_end (GCAL_EVENT_WIDGET (event), end_date);

  /* Retrieve the current sizes */
  layout_manager = gtk_widget_get_layout_manager (GTK_WIDGET (self->grid));
  layout_child = gtk_layout_manager_get_layout_child (layout_manager, to_be_removed);
  deleted_width = gtk_grid_layout_child_get_column_span (GTK_GRID_LAYOUT_CHILD (layout_child));

  layout_child = gtk_layout_manager_get_layout_child (layout_manager, event);
  current_width = gtk_grid_layout_child_get_column_span (GTK_GRID_LAYOUT_CHILD (layout_child));

  destroy_event_widget (self, to_be_removed);

  /* Update the event's size */
  gtk_grid_layout_child_set_column_span (GTK_GRID_LAYOUT_CHILD (layout_child),
                                         current_width + deleted_width);
}

static void
check_mergeable_events (GcalWeekHeader *self)
{
  GList *checked_events[7] = { NULL, };
  gint weekday;

  /* We don't need to check the last column */
  for (weekday = 0; weekday < 6; weekday++)
    {
      GList *l;
      gint index;

      index = 0;

      for (l = self->events[weekday]; l != NULL; l = l->next)
        {
          GcalEvent *current_event;
          gint events_to_merge, i;

          current_event = l->data;
          events_to_merge = 0;

          if (g_list_find (checked_events[weekday], current_event))
            {
              index++;
              continue;
            }
          else
            {
              checked_events[weekday] = g_list_prepend (checked_events[weekday], current_event);
            }

          /*
           * Horizontally check if the next cells have the same event
           * than the current cell.
           */
          for (i = 1; i < 7 - weekday; i++)
            {
              GcalEvent *next_event;

              next_event = g_list_nth_data (self->events[weekday + i], index);

              if (next_event != current_event)
                break;

              events_to_merge++;

              /* Add to the list of checked days so we don't check it more times than necessary */
              checked_events[weekday + i] = g_list_prepend (checked_events[weekday + i], current_event);
            }

          /* We found events to merge. Lets merge them */

          for (i = 0; i < events_to_merge; i++)
            {
              GtkWidget *current_widget, *to_be_removed;

              current_widget = gtk_grid_get_child_at (self->grid, weekday + i, index + 1);
              to_be_removed = gtk_grid_get_child_at (self->grid, weekday + i + 1, index + 1);

              /*
               * We don't want to merge:
               *  - Overflow labels
               *  - The same widget
               *  - Widgets with different visibilities (i.e. broken by overflow
               */
              if (!GCAL_IS_EVENT_WIDGET (current_widget) ||
                  !GCAL_IS_EVENT_WIDGET (to_be_removed) ||
                  current_widget == to_be_removed ||
                  gtk_widget_get_visible (current_widget) != gtk_widget_get_visible (to_be_removed))
                {
                  continue;
                }

              merge_events (self, current_widget, to_be_removed);

              gtk_widget_set_visible (current_widget, is_event_visible (self, weekday + i, index));
            }

          index++;
        }

      /* We can get rid of the checked list here */
      g_list_free (checked_events[weekday]);
    }
}

static void
split_event_widget_at_column (GcalWeekHeader *self,
                              GtkWidget      *widget,
                              gint            column)
{
  GtkLayoutManager *layout_manager;
  GtkLayoutChild *layout_child;
  GDateTime *week_start, *column_date, *end_column_date;
  gboolean create_before;
  gboolean create_after;
  gint left_attach;
  gint top_attach;
  gint new_width;
  gint old_width;

  week_start = gcal_date_time_get_start_of_week (self->active_date);
  column_date = g_date_time_add_days (week_start, column);
  end_column_date = g_date_time_add_days (column_date, 1);

  layout_manager = gtk_widget_get_layout_manager (GTK_WIDGET (self->grid));
  layout_child = gtk_layout_manager_get_layout_child (layout_manager, widget);
  top_attach = gtk_grid_layout_child_get_row (GTK_GRID_LAYOUT_CHILD (layout_child));
  left_attach = gtk_grid_layout_child_get_column (GTK_GRID_LAYOUT_CHILD (layout_child));
  old_width = gtk_grid_layout_child_get_column_span (GTK_GRID_LAYOUT_CHILD (layout_child));

  create_before = column > 0 && left_attach < column;
  create_after = column < 6 && old_width > 1 && left_attach + old_width > column + 1;

  if (create_before)
    {
      GtkWidget *widget_before;

      widget_before = gcal_event_widget_clone (GCAL_EVENT_WIDGET (widget));
      gcal_event_widget_set_date_end (GCAL_EVENT_WIDGET (widget_before), column_date);

      setup_event_widget (self, widget_before);

      gtk_grid_attach (self->grid,
                       widget_before,
                       left_attach,
                       top_attach,
                       column - left_attach,
                       1);

      gtk_widget_set_visible (widget_before, is_event_visible (self, left_attach, top_attach - 1));

      new_width = old_width - column + left_attach;
      old_width = new_width;
      left_attach = column;

      /* Update the current event position, size and start date */
      gtk_grid_layout_child_set_column (GTK_GRID_LAYOUT_CHILD (layout_child), left_attach);
      gtk_grid_layout_child_set_column_span (GTK_GRID_LAYOUT_CHILD (layout_child), new_width);

      gcal_event_widget_set_date_start (GCAL_EVENT_WIDGET (widget), column_date);
      gtk_widget_set_visible (widget, is_event_visible (self, left_attach, top_attach - 1));
    }

  /* Create a new widget after the current widget */
  if (create_after)
    {
      g_autoptr (GDateTime) event_end = NULL;
      GtkWidget *widget_after;

      event_end = g_date_time_to_local (gcal_event_widget_get_date_end (GCAL_EVENT_WIDGET (widget)));

      widget_after = gcal_event_widget_clone (GCAL_EVENT_WIDGET (widget));
      gcal_event_widget_set_date_start (GCAL_EVENT_WIDGET (widget_after), end_column_date);
      gcal_event_widget_set_date_end (GCAL_EVENT_WIDGET (widget_after), event_end);

      setup_event_widget (self, widget_after);

      gtk_grid_attach (self->grid,
                       widget_after,
                       column + 1,
                       top_attach,
                       old_width - 1,
                       1);

      new_width = column - left_attach + 1;

      /* Only update the current widget's width */
      gtk_grid_layout_child_set_column_span (GTK_GRID_LAYOUT_CHILD (layout_child), new_width);

      gtk_widget_set_visible (widget_after, is_event_visible (self, column + 1, top_attach - 1));
    }

  g_clear_pointer (&end_column_date, g_date_time_unref);
  g_clear_pointer (&column_date, g_date_time_unref);
  g_clear_pointer (&week_start, g_date_time_unref);
}

static void
move_events_at_column (GcalWeekHeader *self,
                       MoveDirection   direction,
                       gint            column,
                       gint            start_at)
{
  GtkLayoutManager *layout_manager;
  GtkWidget *child;

  layout_manager = gtk_widget_get_layout_manager (GTK_WIDGET (self->grid));

  /* First, lets find the widgets at this column */
  for (child = gtk_widget_get_first_child (GTK_WIDGET (self->grid));
       child;
       child = gtk_widget_get_next_sibling (child))
    {
      gint top_attach, left_attach, width;
      GtkLayoutChild *layout_child;

      /* Get the widget's current position... */
      layout_child = gtk_layout_manager_get_layout_child (layout_manager, child);
      top_attach = gtk_grid_layout_child_get_row (GTK_GRID_LAYOUT_CHILD (layout_child));
      left_attach = gtk_grid_layout_child_get_column (GTK_GRID_LAYOUT_CHILD (layout_child));
      width = gtk_grid_layout_child_get_column_span (GTK_GRID_LAYOUT_CHILD (layout_child));

      if (left_attach != column || start_at > top_attach - 1 || !GCAL_IS_EVENT_WIDGET (child))
        continue;

      /* If this is a multiday event, break it */
      if (width > 1)
        split_event_widget_at_column (self, child, column);

      top_attach = top_attach + (direction == DOWN ? 1 : -1);

      /* And move it to position + 1 */
      gtk_grid_layout_child_set_row (GTK_GRID_LAYOUT_CHILD (layout_child), top_attach);

      gtk_widget_set_visible (child, is_event_visible (self, left_attach, top_attach - 1));
    }
}

static void
apply_overflow_at_weekday (GcalWeekHeader *self,
                           guint           weekday)
{
  GtkWidget *child;

  /*
   * If we don't need overflow, or we already applied the overflow,
   * we don't need to do anything els.
   */
  if (self->expanded || self->overflow_label[weekday] || g_list_length (self->events[weekday]) < 4)
    return;

  child = gtk_grid_get_child_at (self->grid, weekday, 3);

  if (!child)
    return;

  split_event_widget_at_column (self, child, weekday);
  gtk_widget_set_visible (child, FALSE);
}

static void
add_event_to_grid (GcalWeekHeader *self,
                   GcalEvent      *event,
                   gint            start,
                   gint            end)
{
  g_autoptr (GDateTime) week_start = NULL;
  g_autoptr (GDateTime) week_end = NULL;
  GtkLayoutManager *layout_manager;
  GtkWidget *widget;
  gboolean is_visible, was_visible;
  gint position;
  gint i;

  /* Take a reference to the event */
  g_object_ref (event);

  /* Add at least at the first weekday */
  position = add_event_to_weekday (self, event, start);

  move_events_at_column (self, DOWN, start, position);

  /* Add the event to the grid */
  widget = gcal_event_widget_new (self->context, event);
  setup_event_widget (self, widget);

  gtk_grid_attach (self->grid,
                   widget,
                   start,
                   position + 1,
                   1,
                   1);

  /* Setup event visibility */
  is_visible = is_event_visible (self, start, position);

  gtk_widget_set_visible (widget, is_visible);

  /* Eventually apply the overflow rules */
  apply_overflow_at_weekday (self, start);

  /* Single-day events are the simplest case, and the code above is enough to deal with them */
  if (!gcal_event_is_multiday (event))
    return;

  week_start = gcal_date_time_get_start_of_week (self->active_date);
  week_end = gcal_date_time_get_end_of_week (self->active_date);

  gcal_event_widget_set_date_start (GCAL_EVENT_WIDGET (widget), week_start);
  gcal_event_widget_set_date_end (GCAL_EVENT_WIDGET (widget), week_end);

  layout_manager = gtk_widget_get_layout_manager (GTK_WIDGET (self->grid));

  /*
   * In addition to moving the current column's events below, multiday
   * events must also move the events from ~all~ the columns it spans
   * below
   */
  for (i = start + 1; i <= end; i++)
    {
      gint new_position;

      /* Add the event to that day */
      new_position = add_event_to_weekday (self, event, i);

      was_visible = is_visible;
      is_visible = is_event_visible (self, i, new_position);

      move_events_at_column (self, DOWN, i, new_position);

      /* Add the event to the grid */
      if (new_position == position && was_visible == is_visible)
        {
          GtkLayoutChild *layout_child;

          layout_child = gtk_layout_manager_get_layout_child (layout_manager, widget);
          gtk_grid_layout_child_set_column_span (GTK_GRID_LAYOUT_CHILD (layout_child),
                                                 i - start + 1);
        }
      else
        {
          GDateTime *cloned_widget_start_dt;
          GtkWidget *cloned_widget;

          cloned_widget_start_dt = g_date_time_add_days (week_start, i);
          cloned_widget = gcal_event_widget_clone (GCAL_EVENT_WIDGET (widget));
          setup_event_widget (self, cloned_widget);

          gtk_grid_attach (self->grid,
                           cloned_widget,
                           i,
                           new_position + 1,
                           1,
                           1);

          gcal_event_widget_set_date_end (GCAL_EVENT_WIDGET (widget), cloned_widget_start_dt);
          gcal_event_widget_set_date_start (GCAL_EVENT_WIDGET (cloned_widget), cloned_widget_start_dt);

          /* From now on, let's modify this widget */
          widget = cloned_widget;

          gtk_widget_set_visible (widget, is_visible);

          g_clear_pointer (&cloned_widget_start_dt, g_date_time_unref);
        }

      /* Eventually apply the overflow rules */
      apply_overflow_at_weekday (self, i);

      /* Update the widget's end date */
      gcal_event_widget_set_date_end (GCAL_EVENT_WIDGET (widget), week_end);
    }
}

static void
update_unchanged_events (GcalWeekHeader *self,
                         GDateTime      *new_date)
{
  g_autoptr (GDateTime) new_week_start = NULL;
  g_autoptr (GDateTime) utc_week_start = NULL;
  g_autoptr (GDateTime) new_week_end = NULL;
  g_autoptr (GDateTime) utc_week_end = NULL;
  GList *events_to_update, *l;
  gint weekday;

  events_to_update = NULL;

  new_week_start = gcal_date_time_get_start_of_week (new_date);
  new_week_end = gcal_date_time_get_end_of_week (new_date);

  utc_week_start = g_date_time_new_utc (g_date_time_get_year (new_week_start),
                                        g_date_time_get_month (new_week_start),
                                        g_date_time_get_day_of_month (new_week_start),
                                        0, 0, 0);
  utc_week_end = g_date_time_add_days (utc_week_start, 7);

  for (weekday = 0; weekday < 7; weekday++)
    {
      GList *events;

      events = self->events[weekday];

      for (l = events; l != NULL; l = l->next)
        {
          g_autoptr (GDateTime) event_start = NULL;
          g_autoptr (GDateTime) event_end = NULL;
          GDateTime *week_start, *week_end;

          /*
           * When the event is all day, we must be careful to compare its dates
           * against the UTC variants of the week start and end dates.
           */
          if (gcal_event_get_all_day (l->data))
            {
              event_start = g_date_time_ref (gcal_event_get_date_start (l->data));
              event_end = g_date_time_ref (gcal_event_get_date_end (l->data));

              week_start = utc_week_start;
              week_end = utc_week_end;
            }
          else
            {
              event_start = g_date_time_to_local (gcal_event_get_date_start (l->data));
              event_end = g_date_time_to_local (gcal_event_get_date_end (l->data));

              week_start = new_week_start;
              week_end = new_week_end;
            }

          /*
           * Check if the event must be updated. If we're going to the future, updatable events
           * are events that started somewhere in the past, and are still present. If we're
           * going to the past, updatable events are events that started
           */
          if (g_date_time_compare (event_start, week_end) < 0 &&
              g_date_time_compare (event_end, week_start) > 0 &&
              !g_list_find (events_to_update, l->data))
            {
              events_to_update = g_list_append (events_to_update, l->data);
            }
        }
    }

  for (l = events_to_update; l != NULL; l = l->next)
    {
      gcal_week_header_remove_event (self, gcal_event_get_uid (l->data));
      gcal_week_header_add_event (self, l->data);
    }

  g_clear_pointer (&events_to_update, g_list_free);
}

/* Header */
static void
update_title (GcalWeekHeader *self)
{
  g_autoptr (GDateTime) week_start = NULL;
  gint today_column;

  if(!self->active_date)
    return;

  week_start = gcal_date_time_get_start_of_week (self->active_date);
  today_column = get_today_column (self);

  for (gint i = 0; i < 7; i++)
    {
      g_autoptr (GDateTime) day = NULL;
      g_autofree gchar *weekday_date = NULL;
      g_autofree gchar *weekday_abv = NULL;
      g_autofree gchar *weekday = NULL;
      WeekdayHeader *header;
      gint n_day;

      day = g_date_time_add_days (week_start, i);
      n_day = g_date_time_get_day_of_month (day);

      if (n_day > g_date_get_days_in_month (g_date_time_get_month (week_start), g_date_time_get_year (week_start)))
        n_day = n_day - g_date_get_days_in_month (g_date_time_get_month (week_start), g_date_time_get_year (week_start));

      header = &self->weekday_header[i];

      if (i == today_column)
        {
          gtk_widget_add_css_class (header->weekday_name_label, "accent");
          gtk_widget_add_css_class (header->day_number_label, "accent");
          gtk_widget_remove_css_class (header->weekday_name_label, "dimmed");
          gtk_widget_remove_css_class (header->day_number_label, "dimmed");
        }
      else
        {
          gtk_widget_remove_css_class (header->weekday_name_label, "accent");
          gtk_widget_remove_css_class (header->day_number_label, "accent");
          gtk_widget_add_css_class (header->weekday_name_label, "dimmed");
          gtk_widget_add_css_class (header->day_number_label, "dimmed");
        }

      weekday_date = g_strdup_printf ("%d", n_day);
      gtk_label_set_label (GTK_LABEL (header->day_number_label), weekday_date);

      weekday = g_date_time_format (day, "%a");
      weekday_abv = g_utf8_strup (weekday, -1);
      gtk_label_set_label (GTK_LABEL (header->weekday_name_label), weekday_abv);
    }
}

static void
header_collapse (GcalWeekHeader *self)
{
  GtkLayoutManager *layout_manager;
  GtkWidget *child;

  self->expanded = FALSE;

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (self->scrolledwindow),
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_NEVER);
  gtk_scrolled_window_set_max_content_height (GTK_SCROLLED_WINDOW (self->scrolledwindow), -1);

  layout_manager = gtk_widget_get_layout_manager (GTK_WIDGET (self->grid));

  for (child = gtk_widget_get_first_child (GTK_WIDGET (self->grid));
       child;
       child = gtk_widget_get_next_sibling (child))
    {
      GtkLayoutChild *layout_child;
      gint top_attach, left_attach;

      layout_child = gtk_layout_manager_get_layout_child (layout_manager, child);
      left_attach = gtk_grid_layout_child_get_column (GTK_GRID_LAYOUT_CHILD (layout_child));
      top_attach = gtk_grid_layout_child_get_row (GTK_GRID_LAYOUT_CHILD (layout_child));

      gtk_widget_set_visible (child, is_event_visible (self, left_attach, top_attach - 1));

      apply_overflow_at_weekday (self, left_attach);
    }

  update_overflow (self);
}

static void
header_expand (GcalWeekHeader *self)
{
  GtkLayoutManager *layout_manager;
  GtkWidget *week_view;
  GtkWidget *child;

  week_view = gtk_widget_get_ancestor (GTK_WIDGET (self), GCAL_TYPE_WEEK_VIEW);
  layout_manager = gtk_widget_get_layout_manager (GTK_WIDGET (self->grid));

  self->expanded = TRUE;

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (self->scrolledwindow),
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_AUTOMATIC);

  /* TODO: animate this transition */
  gtk_scrolled_window_set_max_content_height (GTK_SCROLLED_WINDOW (self->scrolledwindow),
                                              gtk_widget_get_height (week_view) / 2);

  child = gtk_widget_get_first_child (GTK_WIDGET (self->grid));
  while (child)
    {
      GtkWidget *next = gtk_widget_get_next_sibling (child);

      /* Remove any remaining labels */
      if (GTK_IS_LABEL (child))
        {
          GtkLayoutChild *layout_child;
          gint column;

          layout_child = gtk_layout_manager_get_layout_child (layout_manager, child);
          column = gtk_grid_layout_child_get_column (GTK_GRID_LAYOUT_CHILD (layout_child));

          self->overflow_label[column] = NULL;
          gtk_grid_remove (self->grid, child);
        }
      else
        {
          gtk_widget_set_visible (child, TRUE);
        }

      child = next;
    }

  /* Merge events that were broken because of the overflow label */
  check_mergeable_events (self);
}

static void
on_expand_action_activated (GcalWeekHeader *self,
                            gpointer        user_data)
{
  if (self->expanded)
    header_collapse (self);
  else
    header_expand (self);
}

static gint
get_dnd_cell (GcalWeekHeader *self,
              gint            x,
              gint            y)
{
  gdouble column_width;

  column_width = gtk_widget_get_width (GTK_WIDGET (self)) / 7.0;

  return x / column_width;
}

static void
move_event_to_cell (GcalWeekHeader        *self,
                    GcalEvent             *event,
                    guint                  cell,
                    GcalRecurrenceModType  mod_type)
{
  g_autoptr (GDateTime) week_start = NULL;
  g_autoptr (GDateTime) dnd_date = NULL;
  g_autoptr (GDateTime) new_end = NULL;
  g_autoptr (GDateTime) tmp_dt = NULL;
  g_autoptr (GcalEvent) changed_event = NULL;
  GDateTime *start_date;
  GDateTime *end_date;
  GTimeSpan difference;

  GCAL_ENTRY;

  /* RTL languages swap the drop cell column */
  if (gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL)
    cell = 6 - cell;

  changed_event = gcal_event_new_from_event (event);
  start_date = gcal_event_get_date_start (changed_event);
  end_date = gcal_event_get_date_end (changed_event);
  week_start = gcal_date_time_get_start_of_week (self->active_date);

  if (!gcal_event_get_all_day (changed_event))
    {
      /*
       * The only case where we don't touch the timezone is for
       * timed, multiday events.
       */
      tmp_dt = g_date_time_new (g_date_time_get_timezone (start_date),
                                g_date_time_get_year (week_start),
                                g_date_time_get_month (week_start),
                                g_date_time_get_day_of_month (week_start),
                                g_date_time_get_hour (start_date),
                                g_date_time_get_minute (start_date),
                                0);
    }
  else
    {
      tmp_dt = g_date_time_new_utc (g_date_time_get_year (week_start),
                                    g_date_time_get_month (week_start),
                                    g_date_time_get_day_of_month (week_start),
                                    0, 0, 0);
    }
  dnd_date = g_date_time_add_days (tmp_dt, cell);

  /* End date */
  difference = g_date_time_difference (end_date, start_date) / G_TIME_SPAN_MINUTE;

  new_end = g_date_time_add_minutes (dnd_date, difference);
  gcal_event_set_date_end (changed_event, new_end);

  /*
   * Set the start date ~after~ the end date, so we can compare
   * the event's start and end dates above
   */
  gcal_event_set_date_start (changed_event, dnd_date);

  /* Commit the changes */
  gcal_manager_update_event (gcal_context_get_manager (self->context), changed_event, mod_type);
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
on_drop_target_drop_cb (GtkDropTarget  *drop_target,
                        const GValue   *value,
                        gdouble         x,
                        gdouble         y,
                        GcalWeekHeader *self)
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
on_drop_target_enter_cb (GtkDropTarget  *drop_target,
                         gdouble         x,
                         gdouble         y,
                         GcalWeekHeader *self)
{

  GCAL_ENTRY;

  self->dnd.cell = get_dnd_cell (self, x, y);
  gtk_widget_set_visible (self->dnd.widget, self->dnd.cell != -1);

  GCAL_RETURN (self->dnd.cell != -1 ? GDK_ACTION_COPY : 0);
}

static void
on_drop_target_leave_cb (GtkDropTarget  *drop_target,
                         GcalWeekHeader *self)
{
  GCAL_ENTRY;

  self->dnd.cell = -1;
  gtk_widget_set_visible (self->dnd.widget, FALSE);

  GCAL_EXIT;
}

static GdkDragAction
on_drop_target_motion_cb (GtkDropTarget  *drop_target,
                          gdouble         x,
                          gdouble         y,
                          GcalWeekHeader *self)
{
  GCAL_ENTRY;

  self->dnd.cell = get_dnd_cell (self, x, y);
  gtk_widget_set_visible (self->dnd.widget, self->dnd.cell != -1);
  gtk_widget_queue_allocate (GTK_WIDGET (self));

  GCAL_RETURN (self->dnd.cell != -1 ? GDK_ACTION_COPY : 0);
}

/* Drawing area content and size */

static void
gcal_week_header_measure (GtkWidget      *widget,
                          GtkOrientation  orientation,
                          gint            for_size,
                          gint           *minimum,
                          gint           *natural,
                          gint           *minimum_baseline,
                          gint           *natural_baseline)
{
  GcalWeekHeader *self = (GcalWeekHeader *) widget;

  g_assert (GCAL_IS_WEEK_HEADER (self));

  gtk_widget_measure (self->main_box,
                      orientation,
                      for_size,
                      minimum, natural,
                      minimum_baseline,
                      natural_baseline);
}

static void
gcal_week_header_size_allocate (GtkWidget *widget,
                                gint       width,
                                gint       height,
                                gint       baseline)
{
  GcalWeekHeader *self = (GcalWeekHeader *) widget;
  gboolean ltr;
  gdouble cell_width;

  g_assert (GCAL_IS_WEEK_HEADER (self));

  gtk_widget_allocate (self->main_box, width, height, baseline, NULL);

  ltr = gtk_widget_get_direction (widget) != GTK_TEXT_DIR_RTL;
  cell_width = width / 7.0;

  if (gtk_widget_should_layout (self->selection.widget))
    {
      gint selection_width;
      gint selection_x;
      gint start, end;

      g_assert (self->selection.start != -1);
      g_assert (self->selection.end != -1);

      start = self->selection.start;
      end = self->selection.end;

      if (start > end)
        {
          start = start + end;
          end = start - end;
          start = start - end;
        }

      selection_width = (end - start + 1) * cell_width;
      selection_x = ltr ? (start * cell_width) : (width - (start * cell_width + selection_width));


      gtk_widget_size_allocate (self->selection.widget,
                                &(GtkAllocation) {
                                  .x = ALIGNED (selection_x),
                                  .y = -6,
                                  .width = ALIGNED (selection_width + 1),
                                  .height = height + 6,
                                },
                                baseline);
    }

  if (gtk_widget_should_layout (self->dnd.widget))
    {
      gtk_widget_size_allocate (self->dnd.widget,
                                &(GtkAllocation) {
                                  .x = self->dnd.cell * cell_width,
                                  .y = -6,
                                  .width = cell_width,
                                  .height = height + 6,
                                },
                                baseline);
    }
}

static void
gcal_week_header_snapshot (GtkWidget   *widget,
                           GtkSnapshot *snapshot)
{
  GcalWeekHeader *self;
  gboolean ltr;
  gint height;
  gint width;
  gint x;

  /* Fonts and colour selection */
  self = GCAL_WEEK_HEADER (widget);
  ltr = gtk_widget_get_direction (widget) != GTK_TEXT_DIR_RTL;

  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_height (widget);
  x = ALIGNED (ltr ? 0 : width);

  gtk_snapshot_save (snapshot);
  gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (x, 0));
  gcal_week_view_common_snapshot_hour_lines (widget, snapshot, GTK_ORIENTATION_HORIZONTAL, width, height);
  gtk_snapshot_restore (snapshot);

  gtk_widget_snapshot_child (widget, self->selection.widget, snapshot);
  gtk_widget_snapshot_child (widget, self->dnd.widget, snapshot);
  gtk_widget_snapshot_child (widget, self->main_box, snapshot);
}


/*
 * GObject overrides
 */

static void
gcal_week_header_dispose (GObject *object)
{
  GcalWeekHeader *self = (GcalWeekHeader *) object;

  g_assert (GCAL_IS_WEEK_HEADER (self));

  g_clear_pointer (&self->dnd.widget, gtk_widget_unparent);
  g_clear_pointer (&self->selection.widget, gtk_widget_unparent);

  gtk_widget_dispose_template (GTK_WIDGET (self), GCAL_TYPE_WEEK_HEADER);

  G_OBJECT_CLASS (gcal_week_header_parent_class)->dispose (object);
}

static void
gcal_week_header_finalize (GObject *object)
{
  GcalWeekHeader *self = GCAL_WEEK_HEADER (object);
  gint i;

  gcal_clear_date_time (&self->active_date);

  for (i = 0; i < 7; i++)
    g_list_free (self->events[i]);

  for (i = 0; i < G_N_ELEMENTS (self->weather_infos); i++)
    wid_clear (&self->weather_infos[i]);

  G_OBJECT_CLASS (gcal_week_header_parent_class)->finalize (object);
}

static void
gcal_week_header_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GcalWeekHeader *self = GCAL_WEEK_HEADER (object);

  switch (prop_id)
    {
    case PROP_CAN_EXPAND:
      g_value_set_boolean (value, self->can_expand);
      break;

    case PROP_EXPANDED:
      g_value_set_boolean (value, self->expanded);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_week_header_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GcalWeekHeader *self = GCAL_WEEK_HEADER (object);

  switch (prop_id)
    {
    case PROP_EXPANDED:
      gcal_week_header_set_expanded (self, g_value_get_boolean (value));
      break;

    case PROP_CAN_EXPAND:
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_week_header_class_init (GcalWeekHeaderClass *kclass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (kclass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (kclass);

  object_class->dispose = gcal_week_header_dispose;
  object_class->finalize = gcal_week_header_finalize;
  object_class->get_property = gcal_week_header_get_property;
  object_class->set_property = gcal_week_header_set_property;

  widget_class->measure = gcal_week_header_measure;
  widget_class->size_allocate = gcal_week_header_size_allocate;
  widget_class->snapshot = gcal_week_header_snapshot;

  properties[PROP_CAN_EXPAND] = g_param_spec_boolean ("can-expand", NULL, NULL,
                                                      FALSE,
                                                      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_EXPANDED] = g_param_spec_boolean ("expanded", NULL, NULL,
                                                    FALSE,
                                                    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals[EVENT_ACTIVATED] = g_signal_new ("event-activated",
                                           GCAL_TYPE_WEEK_HEADER,
                                           G_SIGNAL_RUN_FIRST,
                                           0,  NULL, NULL, NULL,
                                           G_TYPE_NONE,
                                           1,
                                           GCAL_TYPE_EVENT_WIDGET);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/views/gcal-week-header.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalWeekHeader, grid);
  gtk_widget_class_bind_template_child (widget_class, GcalWeekHeader, main_box);
  gtk_widget_class_bind_template_child (widget_class, GcalWeekHeader, motion_controller);
  gtk_widget_class_bind_template_child (widget_class, GcalWeekHeader, scrolledwindow);
  gtk_widget_class_bind_template_child (widget_class, GcalWeekHeader, weekdays_box);

  gtk_widget_class_bind_template_callback (widget_class, on_button_pressed);
  gtk_widget_class_bind_template_callback (widget_class, on_button_released);
  gtk_widget_class_bind_template_callback (widget_class, on_expand_action_activated);
  gtk_widget_class_bind_template_callback (widget_class, on_motion_notify);

  gtk_widget_class_set_css_name (widget_class, "weekheader");
}

static void
gcal_week_header_init (GcalWeekHeader *self)
{
  GtkDropTarget *drop_target;
  gint i;

  self->expanded = FALSE;

  self->selection.start = -1;
  self->selection.end = -1;
  self->selection.widget = gcal_create_selection_widget ();
  gtk_widget_set_visible (self->selection.widget, FALSE);
  gtk_widget_set_parent (self->selection.widget, GTK_WIDGET (self));

  self->dnd.cell = -1;
  self->dnd.widget = gcal_create_drop_target_widget ();
  gtk_widget_set_visible (self->dnd.widget, FALSE);
  gtk_widget_set_parent (self->dnd.widget, GTK_WIDGET (self));

  self->first_weekday = get_first_weekday ();

  gtk_widget_init_template (GTK_WIDGET (self));

  for (i = 0; i < 7; i++)
    {
      WeekdayHeader *header = &self->weekday_header[i];
      GtkWidget *box;

      box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_widget_set_margin_start (box, 6);

      header->weekday_name_label = gtk_label_new ("");
      gtk_widget_set_hexpand (header->weekday_name_label, TRUE);
      gtk_widget_add_css_class (header->weekday_name_label, "heading");
      gtk_widget_add_css_class (header->weekday_name_label, "dimmed");
      gtk_label_set_xalign (GTK_LABEL (header->weekday_name_label), 0.0);
      gtk_box_append (GTK_BOX (box), header->weekday_name_label);

      header->day_number_label = gtk_label_new ("");
      gtk_widget_set_hexpand (header->day_number_label, TRUE);
      gtk_widget_add_css_class (header->day_number_label, "title-2");
      gtk_widget_add_css_class (header->day_number_label, "dimmed");
      gtk_label_set_xalign (GTK_LABEL (header->day_number_label), 0.0);
      gtk_box_append (GTK_BOX (box), header->day_number_label);

      gtk_box_append (self->weekdays_box, box);

      /* Add 7 empty widget to the grid to ensure proper spacing */
      gtk_grid_attach (self->grid, gtk_box_new (GTK_ORIENTATION_VERTICAL, 0), i, 0, 1, 1);
    }

  drop_target = gtk_drop_target_new (GCAL_TYPE_EVENT_WIDGET, GDK_ACTION_COPY);
  g_signal_connect (drop_target, "drop", G_CALLBACK (on_drop_target_drop_cb), self);
  g_signal_connect (drop_target, "enter", G_CALLBACK (on_drop_target_enter_cb), self);
  g_signal_connect (drop_target, "leave", G_CALLBACK (on_drop_target_leave_cb), self);
  g_signal_connect (drop_target, "motion", G_CALLBACK (on_drop_target_motion_cb), self);
  gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (drop_target));
}

void
gcal_week_header_set_context (GcalWeekHeader *self,
                              GcalContext    *context)
{
  g_return_if_fail (GCAL_IS_WEEK_HEADER (self));

  self->context = context;

  g_signal_connect_object (gcal_context_get_clock (context),
                           "day-changed",
                           G_CALLBACK (gtk_widget_queue_draw),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (gcal_context_get_weather_service (self->context),
                           "weather-changed",
                           G_CALLBACK (on_weather_update),
                           self,
                           0);

  update_weather_infos (self);
}

void
gcal_week_header_add_event (GcalWeekHeader *self,
                            GcalEvent      *event)
{
  g_autoptr (GDateTime) event_start = NULL;
  g_autoptr (GDateTime) event_end = NULL;
  g_autoptr (GDateTime) week_start = NULL;
  g_autoptr (GDateTime) week_end = NULL;
  gboolean all_day;
  gint start, end;

  g_return_if_fail (GCAL_IS_WEEK_HEADER (self));

  all_day = gcal_event_get_all_day (event);
  week_start = gcal_date_time_get_start_of_week (self->active_date);
  week_end = gcal_date_time_get_end_of_week (self->active_date);

  /* Retrieve the real start and end dates */
  if (all_day)
    {
      GDateTime *utc_week_start, *utc_week_end, *aux;

      utc_week_start = g_date_time_new_utc (g_date_time_get_year (week_start),
                                            g_date_time_get_month (week_start),
                                            g_date_time_get_day_of_month (week_start),
                                            0, 0, 0);
      utc_week_end = g_date_time_new_utc (g_date_time_get_year (week_end),
                                          g_date_time_get_month (week_end),
                                          g_date_time_get_day_of_month (week_end),
                                          0, 0, 0);

      event_start = g_date_time_ref (gcal_event_get_date_start (event));
      event_end = g_date_time_ref (gcal_event_get_date_end (event));

      /*
       * Switch the week start and end by the UTC variants, in
       * order to correctly compare all-day events.
       */
      aux = week_start;
      week_start = utc_week_start;
      gcal_clear_date_time (&aux);

      aux = week_end;
      week_end = utc_week_end;
      gcal_clear_date_time (&aux);

    }
  else
    {
      event_start = g_date_time_to_local (gcal_event_get_date_start (event));
      event_end = g_date_time_to_local (gcal_event_get_date_end (event));
    }

  /* Start position */
  if (g_date_time_compare (event_start, week_start) >= 0)
    start = floor (g_date_time_difference (event_start, week_start) / G_TIME_SPAN_DAY);
  else
    start = 0;

  /* End position */
  if (g_date_time_compare (event_end, week_end) <= 0)
    end = floor (g_date_time_difference (event_end, week_start) / G_TIME_SPAN_DAY) - all_day;
  else
    end = 6;

  /* Sanity checks */
  if (start > end || start > 6 || end < 0)
    {
      g_warning ("Error adding event '%s' to the week header", gcal_event_get_summary (event));
      return;
    }

  /* Add the event widget to the grid */
  add_event_to_grid (self, event, start, end);

  /* Check if we eventually can merge events */
  check_mergeable_events (self);

  /* And also update the overflow labels */
  update_overflow (self);
}

void
gcal_week_header_remove_event (GcalWeekHeader *self,
                               const gchar    *uuid)
{
  g_autoptr (GcalEvent) removed_event = NULL;
  GtkWidget *child;
  gint weekday;

  g_return_if_fail (GCAL_IS_WEEK_HEADER (self));

  removed_event = get_event_by_uuid (self, uuid);

  if (!removed_event)
    return;

  child = gtk_widget_get_first_child (GTK_WIDGET (self->grid));
  while (child)
    {
      GtkWidget *next = gtk_widget_get_next_sibling (child);

      if (GCAL_IS_EVENT_WIDGET (child))
        {
          GcalEventWidget *child_widget;
          GcalEvent *event;

          child_widget = GCAL_EVENT_WIDGET (child);
          event = gcal_event_widget_get_event (child_widget);

          if (g_strcmp0 (uuid, gcal_event_get_uid (event)) == 0)
            destroy_event_widget (self, child);
        }

      child = next;
    }

  /* Remove from the weekday's GList */
  for (weekday = 0; weekday < 7; weekday++)
    {
      GList *l;
      gint event_position;

      l = self->events[weekday];
      event_position = g_list_index (self->events[weekday], removed_event);

      if (event_position == -1)
        continue;

      /* Remove from the current weekday */
      l = g_list_remove (l, removed_event);
      self->events[weekday] = l;

      /* Move remaining events up */
      move_events_at_column (self, UP, weekday, event_position);
    }

  /* Check if we eventually can merge events */
  check_mergeable_events (self);

  /* And also update the overflow labels */
  update_overflow (self);
}

GList*
gcal_week_header_get_children_by_uuid (GcalWeekHeader        *self,
                                       GcalRecurrenceModType  mod,
                                       const gchar           *uuid)
{
  g_autolist (GcalEventWidget) result = NULL;

  GCAL_ENTRY;

  result = filter_children_by_uid_and_modtype (GTK_WIDGET (self->grid), mod, uuid);

  GCAL_RETURN (g_steal_pointer (&result));
}

void
gcal_week_header_clear_marks (GcalWeekHeader *self)
{
  g_return_if_fail (GCAL_IS_WEEK_HEADER (self));

  self->selection.start = -1;
  self->selection.end = -1;
  gtk_widget_set_visible (self->selection.widget, FALSE);
}

void
gcal_week_header_set_date (GcalWeekHeader *self,
                           GDateTime      *date)
{
  gboolean had_date;

  /*
   * If the active date changed, but we're still in the same week,
   * there's no need to recalculate visible events.
   */
  if (self->active_date && date &&
      g_date_time_get_year (self->active_date) == g_date_time_get_year (date) &&
      g_date_time_get_week_of_year (self->active_date) == g_date_time_get_week_of_year (date))
    {
      return;
    }

  had_date = self->active_date != NULL;

  gcal_set_date_time (&self->active_date, date);

  update_title (self);
  gtk_widget_queue_draw (GTK_WIDGET (self));

  if (had_date)
    update_unchanged_events (self, self->active_date);

  update_weather_infos (self);
}

gboolean
gcal_week_header_get_can_expand (GcalWeekHeader *self)
{
  g_assert (GCAL_IS_WEEK_HEADER (self));

  return self->can_expand;
}

gboolean
gcal_week_header_get_expanded (GcalWeekHeader *self)
{
  g_assert (GCAL_IS_WEEK_HEADER (self));

  return self->expanded;
}

void
gcal_week_header_set_expanded (GcalWeekHeader *self,
                               gboolean        expanded)
{
  g_assert (GCAL_IS_WEEK_HEADER (self));

  if (self->expanded == expanded)
    return;

  if (expanded)
    header_expand (self);
  else
    header_collapse (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_EXPANDED]);
}
