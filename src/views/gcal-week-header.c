/* gcal-week-header.c
 *
 * Copyright (C) 2016 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#include "gcal-debug.h"
#include "gcal-event-widget.h"
#include "gcal-utils.h"
#include "gcal-view.h"
#include "gcal-week-header.h"
#include "gcal-week-view.h"

#include <glib/gi18n.h>
#include <string.h>
#include <math.h>

#define COLUMN_PADDING       6

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
  GcalWeatherInfo  *winfo;    /* owned */
  GdkPixbuf        *icon_buf; /* owned */
} WeatherInfoDay;



struct _GcalWeekHeader
{
  GtkGrid           parent;

  GtkWidget        *grid;
  GtkWidget        *month_label;
  GtkWidget        *week_label;
  GtkWidget        *year_label;
  GtkWidget        *scrolledwindow;
  GtkWidget        *expand_button;
  GtkWidget        *expand_button_box;
  GtkWidget        *expand_button_image;
  GtkWidget        *header_labels_box;

  GcalManager      *manager;

  /*
   * Stores the events as they come from the week-view
   * The list will later be iterated after the active date is changed
   * and the events will be placed
   */
  GList            *events[7];
  GtkWidget        *overflow_label[7];

  gint              first_weekday;

  /*
   * Used for checking if the header is in collapsed state or expand state
   * false is collapse state true is expand state
   */
  gboolean          expanded;

  gboolean          use_24h_format;

  icaltimetype     *active_date;

  gint              selection_start;
  gint              selection_end;
  gint              dnd_cell;

  GcalWeatherService *weather_service; /* unowned, nullable */
  /* Array of nullable weather infos for each day, starting with Sunday. */
  WeatherInfoDay      weather_infos[7];

  GtkSizeGroup     *sizegroup;
};

typedef enum
{
  UP,
  DOWN
} MoveDirection;

enum
{
  PROP_0,
  PROP_WEATHER_SERVICE,
  NUM_PROPS
};

enum
{
  EVENT_ACTIVATED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (GcalWeekHeader, gcal_week_header, GTK_TYPE_GRID);


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

  if (wid->winfo != NULL)
    g_clear_object (&wid->winfo);

  if (wid->icon_buf != NULL)
    g_clear_object (&wid->icon_buf);
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
  gtk_widget_set_margin_end (widget, 6);
  g_signal_connect (widget, "activate", G_CALLBACK (on_event_widget_activated), self);
}

static inline void
destroy_event_widget (GcalWeekHeader *self,
                      GtkWidget      *widget)
{
  g_signal_handlers_disconnect_by_func (widget, on_event_widget_activated, self);
  gtk_widget_destroy (widget);
}

/* Auxiliary methods */
static gboolean
on_button_pressed (GcalWeekHeader *self,
                   GdkEventButton *event,
                   GtkWidget      *widget)
{
  gboolean ltr;
  gdouble column_width;
  gint column;
  gint width;

  ltr = gtk_widget_get_direction (widget) != GTK_TEXT_DIR_RTL;
  width = gtk_widget_get_allocated_width (widget);
  column_width = width / 7.0;
  column = ltr ? (event->x / column_width) : (7  - event->x / column_width);

  self->selection_start = column;
  self->selection_end = column;

  gtk_widget_queue_draw (GTK_WIDGET (self));

  return GDK_EVENT_PROPAGATE;
}

static gboolean
on_motion_notify (GcalWeekHeader *self,
                  GdkEventMotion *event,
                  GtkWidget      *widget)
{
  gboolean ltr;
  gdouble column_width;
  gint column;
  gint width;

  if (!(event->state & GDK_BUTTON_PRESS_MASK))
    return GDK_EVENT_PROPAGATE;

  ltr = gtk_widget_get_direction (widget) != GTK_TEXT_DIR_RTL;
  width = gtk_widget_get_allocated_width (widget);
  column_width = width / 7.0;
  column = ltr ? (event->x / column_width) : (7  - event->x / column_width);

  self->selection_end = column;

  gtk_widget_queue_draw (GTK_WIDGET (self));

  return GDK_EVENT_STOP;
}

static gboolean
on_button_released (GcalWeekHeader *self,
                    GdkEventButton *event,
                    GtkWidget      *widget)
{
  g_autoptr (GDateTime) week_start, selection_start, selection_end;
  GtkWidget *weekview;
  gboolean ltr;
  gdouble column_width;
  gint out_x, out_y;
  gint column;
  gint width;
  gint start;
  gint end;

  ltr = gtk_widget_get_direction (widget) != GTK_TEXT_DIR_RTL;
  width = gtk_widget_get_allocated_width (widget);
  column_width = width / 7.0;
  column = ltr ? (event->x / column_width) : (7  - event->x / column_width);

  self->selection_end = column;

  gtk_widget_queue_draw (GTK_WIDGET (self));

  /* Fake the week view's event so we can control the X and Y values */
  weekview = gtk_widget_get_ancestor (widget, GCAL_TYPE_WEEK_VIEW);

  start = self->selection_start;
  end = self->selection_end;

  if (start > end)
    {
      start = start + end;
      end = start - end;
      start = start - end;
    }

  week_start = get_start_of_week (self->active_date);
  selection_start = g_date_time_add_days (week_start, start);
  selection_end = end == start ? g_date_time_ref (selection_start) : g_date_time_add_days (week_start, end + 1);

  out_x = ltr ? (column_width * (column + 0.5)) : (width - column_width * (column + 0.5));

  /* Translate X... */
  gtk_widget_translate_coordinates (widget, weekview, out_x, 0, &out_x, NULL);

  /* And Y */
  gtk_widget_translate_coordinates (GTK_WIDGET (self),
                                    weekview,
                                    0,
                                    gtk_widget_get_allocated_height (GTK_WIDGET (self)),
                                    NULL,
                                    &out_y);

  g_signal_emit_by_name (weekview,
                         "create-event",
                         selection_start,
                         selection_end,
                         (gdouble) out_x,
                         (gdouble) out_y);

  return GDK_EVENT_STOP;
}

static void
on_weather_update (GcalWeatherService *weather_service,
                   GcalWeekHeader     *self)
{
  g_return_if_fail (GCAL_IS_WEATHER_SERVICE (weather_service));
  g_return_if_fail (GCAL_IS_WEEK_HEADER (self));
  g_return_if_fail (self->weather_service == weather_service);

  gcal_week_header_update_weather_infos (self);
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
  g_autoptr(GDateTime) today, week_start;
  gint days_diff;

  today = g_date_time_new_now_local ();
  week_start = get_start_of_week (self->active_date);
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

          /* Show the button if not visible yet */
          if (!gtk_widget_get_visible (self->expand_button))
            gtk_widget_show (self->expand_button);

          /* TODO: use a button and show an overflow popover */
          if (!label)
            {
              label = gtk_label_new ("");
              gtk_grid_attach (GTK_GRID (self->grid),
                               label,
                               i,
                               3,
                               1,
                               1);

              self->overflow_label[i] = label;

              gtk_widget_show (label);
            }

          gtk_label_set_label (GTK_LABEL (label), text);
          g_free (text);
        }
      else if (label)
        {
          gtk_widget_destroy (label);
          self->overflow_label[i] = NULL;
        }
    }

  gtk_widget_set_visible (self->expand_button, show_expand);
}

static void
merge_events (GcalWeekHeader *self,
              GtkWidget      *event,
              GtkWidget      *to_be_removed)
{
  GDateTime *end_date;
  gint deleted_width, current_width;

  /* Setup the new end date of the merged event */
  end_date = gcal_event_widget_get_date_end (GCAL_EVENT_WIDGET (to_be_removed));
  gcal_event_widget_set_date_end (GCAL_EVENT_WIDGET (event), end_date);

  /* Retrieve the current sizes */
  gtk_container_child_get (GTK_CONTAINER (self->grid),
                           to_be_removed,
                           "width", &deleted_width,
                           NULL);

  gtk_container_child_get (GTK_CONTAINER (self->grid),
                           event,
                           "width", &current_width,
                           NULL);

  destroy_event_widget (self, to_be_removed);

  /* Update the event's size */
  gtk_container_child_set (GTK_CONTAINER (self->grid),
                           event,
                           "width", current_width + deleted_width,
                           NULL);
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
          i = 0;

          for (i = 0; i < events_to_merge; i++)
            {
              GtkWidget *current_widget, *to_be_removed;

              current_widget = gtk_grid_get_child_at (GTK_GRID (self->grid), weekday + i, index + 1);
              to_be_removed = gtk_grid_get_child_at (GTK_GRID (self->grid), weekday + i + 1, index + 1);

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
  GDateTime *week_start, *column_date, *end_column_date;
  gboolean create_before;
  gboolean create_after;
  gint left_attach;
  gint top_attach;
  gint new_width;
  gint old_width;

  week_start = get_start_of_week (self->active_date);
  column_date = g_date_time_add_days (week_start, column);
  end_column_date = g_date_time_add_days (column_date, 1);

  gtk_container_child_get (GTK_CONTAINER (self->grid),
                           widget,
                           "top_attach", &top_attach,
                           "left_attach", &left_attach,
                           "width", &old_width,
                           NULL);

  create_before = column > 0 && left_attach < column;
  create_after = column < 6 && old_width > 1 && left_attach + old_width > column + 1;

  if (create_before)
    {
      GtkWidget *widget_before;

      widget_before = gcal_event_widget_clone (GCAL_EVENT_WIDGET (widget));
      gcal_event_widget_set_date_end (GCAL_EVENT_WIDGET (widget_before), column_date);

      setup_event_widget (self, widget_before);

      gtk_grid_attach (GTK_GRID (self->grid),
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
      gtk_container_child_set (GTK_CONTAINER (self->grid),
                               widget,
                               "left_attach", left_attach,
                               "width", new_width,
                               NULL);

      gcal_event_widget_set_date_start (GCAL_EVENT_WIDGET (widget), column_date);
      gtk_widget_set_visible (widget, is_event_visible (self, left_attach, top_attach - 1));
    }

  /* Create a new widget after the current widget */
  if (create_after)
    {
      g_autoptr (GDateTime) event_end;
      GtkWidget *widget_after;

      event_end = g_date_time_to_local (gcal_event_widget_get_date_end (GCAL_EVENT_WIDGET (widget)));

      widget_after = gcal_event_widget_clone (GCAL_EVENT_WIDGET (widget));
      gcal_event_widget_set_date_start (GCAL_EVENT_WIDGET (widget_after), end_column_date);
      gcal_event_widget_set_date_end (GCAL_EVENT_WIDGET (widget_after), event_end);

      setup_event_widget (self, widget_after);

      gtk_grid_attach (GTK_GRID (self->grid),
                       widget_after,
                       column + 1,
                       top_attach,
                       old_width - 1,
                       1);

      new_width = column - left_attach + 1;

      /* Only update the current widget's width */
      gtk_container_child_set (GTK_CONTAINER (self->grid),
                               widget,
                               "width", new_width,
                               NULL);

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
  GList *children, *l;

  children = gtk_container_get_children (GTK_CONTAINER (self->grid));

  /* First, lets find the widgets at this column */
  for (l = children; l != NULL; l = l->next)
    {
      gint top_attach, left_attach, width;

      /* Get the widget's current position... */
      gtk_container_child_get (GTK_CONTAINER (self->grid),
                               l->data,
                               "top_attach", &top_attach,
                               "left_attach", &left_attach,
                               "width", &width,
                               NULL);

      if (left_attach != column || start_at > top_attach - 1 || !GCAL_IS_EVENT_WIDGET (l->data))
        continue;

      /* If this is a multiday event, break it */
      if (width > 1)
        split_event_widget_at_column (self, l->data, column);

      top_attach = top_attach + (direction == DOWN ? 1 : -1);

      /* And move it to position + 1 */
      gtk_container_child_set (GTK_CONTAINER (self->grid),
                               l->data,
                               "top_attach", top_attach,
                               NULL);

      gtk_widget_set_visible (l->data, is_event_visible (self, left_attach, top_attach - 1));
    }

  g_clear_pointer (&children, g_list_free);
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

  child = gtk_grid_get_child_at (GTK_GRID (self->grid), weekday, 3);

  split_event_widget_at_column (self, child, weekday);
  gtk_widget_hide (child);
}

static void
add_event_to_grid (GcalWeekHeader *self,
                   GcalEvent      *event,
                   gint            start,
                   gint            end)
{
  g_autoptr (GDateTime) week_start = NULL;
  g_autoptr (GDateTime) week_end = NULL;
  GtkWidget *widget;
  gboolean is_visible, was_visible;
  gint position;
  gint i;

  /* Add at least at the first weekday */
  position = add_event_to_weekday (self, event, start);

  move_events_at_column (self, DOWN, start, position);

  /* Add the event to the grid */
  widget = gcal_event_widget_new (event);
  setup_event_widget (self, widget);

  gtk_grid_attach (GTK_GRID (self->grid),
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

  week_start = get_start_of_week (self->active_date);
  week_end = get_end_of_week (self->active_date);

  gcal_event_widget_set_date_start (GCAL_EVENT_WIDGET (widget), week_start);
  gcal_event_widget_set_date_end (GCAL_EVENT_WIDGET (widget), week_end);

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
          gtk_container_child_set (GTK_CONTAINER (self->grid),
                                   widget,
                                   "width", i - start + 1,
                                   NULL);
        }
      else
        {
          GDateTime *cloned_widget_start_dt;
          GtkWidget *cloned_widget;

          cloned_widget_start_dt = g_date_time_add_days (week_start, i);
          cloned_widget = gcal_event_widget_clone (GCAL_EVENT_WIDGET (widget));
          setup_event_widget (self, cloned_widget);

          gtk_grid_attach (GTK_GRID (self->grid),
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
                         icaltimetype   *new_icaldt)
{
  g_autoptr (GDateTime) new_week_start, new_week_end;
  g_autoptr (GDateTime) utc_week_start, utc_week_end;
  GList *events_to_update, *l;
  gint weekday;

  events_to_update = NULL;

  new_week_start = get_start_of_week (new_icaldt);
  new_week_end = get_end_of_week (new_icaldt);

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
          g_autoptr (GDateTime) event_start, event_end;
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
  GDateTime *week_start, *week_end, *week_mid;
  gchar *year_label, *month_label, *week_label;

  if(!self->active_date)
    return;

  week_start = get_start_of_week (self->active_date);
  week_end = g_date_time_add_days (week_start, 6);
  week_mid = g_date_time_add_days (week_start, 3);

  if (g_date_time_get_month (week_start) == g_date_time_get_month (week_end))
    {
      month_label = g_strdup_printf ("%s", gcal_get_month_name (g_date_time_get_month (week_start) - 1));
    }
  else
    {
      month_label = g_strdup_printf("%s - %s ",
                                    gcal_get_month_name (g_date_time_get_month (week_start) -1),
                                    gcal_get_month_name (g_date_time_get_month (week_end) - 1));
    }

  if (g_date_time_get_year (week_start) == g_date_time_get_year (week_end))
    {
      year_label = g_strdup_printf ("%d", g_date_time_get_year (week_start));
    }
  else
    {
      year_label = g_strdup_printf ("%d - %d",
                                    g_date_time_get_year (week_start),
                                    g_date_time_get_year (week_end));
    }

  week_label = g_strdup_printf (_("week %d"), g_date_time_get_week_of_year (week_mid));

  gtk_label_set_label (GTK_LABEL (self->month_label), month_label);
  gtk_label_set_label (GTK_LABEL (self->week_label), week_label);
  gtk_label_set_label (GTK_LABEL (self->year_label), year_label);

  g_clear_pointer (&week_start, g_date_time_unref);
  g_clear_pointer (&week_end, g_date_time_unref);
  g_clear_pointer (&week_mid, g_date_time_unref);
  g_clear_pointer (&month_label, g_free);
  g_clear_pointer (&week_label, g_free);
  g_clear_pointer (&year_label, g_free);
}

static void
header_collapse (GcalWeekHeader *self)
{
  GList *children, *l;

  children = gtk_container_get_children (GTK_CONTAINER (self->grid));

  self->expanded = FALSE;

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (self->scrolledwindow),
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_NEVER);
  gtk_scrolled_window_set_max_content_height (GTK_SCROLLED_WINDOW (self->scrolledwindow), -1);
  gtk_image_set_from_icon_name (GTK_IMAGE (self->expand_button_image), "go-down-symbolic", 4);

  for (l = children; l != NULL; l = l->next)
    {
      gint top_attach, left_attach;

      gtk_container_child_get (GTK_CONTAINER (self->grid),
                               l->data,
                               "top-attach", &top_attach,
                               "left_attach", &left_attach,
                               NULL);

      gtk_widget_set_visible (l->data, is_event_visible (self, left_attach, top_attach - 1));

      apply_overflow_at_weekday (self, left_attach);
    }

  update_overflow (self);

  g_clear_pointer (&children, g_list_free);
}

static void
header_expand (GcalWeekHeader *self)
{
  GtkWidget *week_view;
  GList *children, *l;

  children = gtk_container_get_children (GTK_CONTAINER (self->grid));
  week_view = gtk_widget_get_ancestor (GTK_WIDGET (self), GCAL_TYPE_WEEK_VIEW);

  self->expanded = TRUE;

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (self->scrolledwindow),
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_AUTOMATIC);

  /* TODO: animate this transition */
  gtk_scrolled_window_set_max_content_height (GTK_SCROLLED_WINDOW (self->scrolledwindow),
                                              gtk_widget_get_allocated_height (week_view) / 2);

  gtk_image_set_from_icon_name (GTK_IMAGE (self->expand_button_image), "go-up-symbolic", 4);

  for (l = children; l != NULL; l = l->next)
    {
      /* Remove any remaining labels */
      if (GTK_IS_LABEL (l->data))
        {
          gint left_attach;

          gtk_container_child_get (GTK_CONTAINER (self->grid),
                                   l->data,
                                   "left_attach", &left_attach,
                                   NULL);

          self->overflow_label[left_attach] = NULL;
          gtk_widget_destroy (l->data);
        }
      else
        {
          gtk_widget_show (l->data);
        }
    }

  /* Merge events that were broken because of the overflow label */
  check_mergeable_events (self);

  g_clear_pointer (&children, g_list_free);
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

/* Drawing area content and size */
static gdouble
get_weekday_names_height (GtkWidget *widget)
{
  PangoFontDescription *font_desc;
  GtkStyleContext* context;
  GtkStateFlags state_flags;
  PangoLayout *layout;
  GtkBorder padding;
  gint final_height;
  gint font_height;

  context = gtk_widget_get_style_context (widget);
  state_flags = gtk_style_context_get_state (context);

  layout = gtk_widget_create_pango_layout (widget, "A");

  gtk_style_context_save (context);
  gtk_style_context_add_class (context, "week-dates");
  gtk_style_context_get (context, state_flags, "font", &font_desc, NULL);
  gtk_style_context_get_padding (context, state_flags, &padding);

  pango_layout_set_font_description (layout, font_desc);
  pango_layout_get_pixel_size (layout, NULL, &font_height);

  pango_font_description_free (font_desc);
  gtk_style_context_restore (context);

  final_height = padding.top + font_height + padding.bottom;

  gtk_style_context_save (context);
  gtk_style_context_add_class (context, "week-names");
  gtk_style_context_get (context, state_flags, "font", &font_desc, NULL);
  gtk_style_context_get_padding (context, state_flags, &padding);

  pango_layout_set_font_description (layout, font_desc);
  pango_layout_get_pixel_size (layout, NULL, &font_height);

  pango_font_description_free (font_desc);
  gtk_style_context_restore (context);

  final_height += padding.top + font_height + padding.bottom;

  return final_height;
}

static void
gcal_week_header_finalize (GObject *object)
{
  GcalWeekHeader *self = GCAL_WEEK_HEADER (object);
  gint i;

  g_clear_pointer (&self->active_date, g_free);

  for (i = 0; i < 7; i++)
    g_list_free (self->events[i]);

  if (self->weather_service != NULL)
    {
      g_signal_handlers_disconnect_by_func (self->weather_service,
                                            (GCallback) on_weather_update,
                                            self);
      g_clear_object (&self->weather_service);
    }

  for (i = 0; i < G_N_ELEMENTS (self->weather_infos); i++)
    wid_clear (&self->weather_infos[i]);
}

static void
gcal_week_header_get_property (GObject    *object,
                               guint       property_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GcalWeekHeader *self = GCAL_WEEK_HEADER (object);

  switch (property_id)
    {
    case PROP_WEATHER_SERVICE:
      g_value_set_boxed (value, self->weather_service);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gcal_week_header_set_property (GObject      *object,
                               guint         property_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GcalWeekHeader *self = GCAL_WEEK_HEADER (object);

  switch (property_id)
    {
    case PROP_WEATHER_SERVICE:
        gcal_week_header_set_weather_service (self, GCAL_WEATHER_SERVICE (g_value_get_object (value)));
        break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gcal_week_header_size_allocate (GtkWidget     *widget,
                                GtkAllocation *alloc)
{
  GcalWeekHeader *self = GCAL_WEEK_HEADER (widget);
  gint min_header_height;

  min_header_height = get_weekday_names_height (widget);

  gtk_widget_set_margin_top (self->scrolledwindow, min_header_height);

  GTK_WIDGET_CLASS (gcal_week_header_parent_class)->size_allocate (widget, alloc);
}

static gboolean
gcal_week_header_draw (GtkWidget      *widget,
                       cairo_t        *cr)
{
  GtkStyleContext *context;
  GcalWeekHeader *self;
  GtkStateFlags state;
  GdkRGBA color;
  GtkAllocation alloc;
  GtkBorder padding;

  GDateTime *week_start, *week_end;

  PangoLayout *layout;
  PangoFontDescription *bold_font;

  gdouble cell_width;
  gint i, day_abv_font_height, current_cell, today_column;
  gint start_x, start_y;

  gboolean ltr;

  cairo_save (cr);

  /* Fonts and colour selection */
  self = GCAL_WEEK_HEADER (widget);
  context = gtk_widget_get_style_context (widget);
  state = gtk_style_context_get_state (context);
  ltr = gtk_widget_get_direction (widget) != GTK_TEXT_DIR_RTL;

  start_x = ltr ? gtk_widget_get_allocated_width (self->expand_button_box) : 0;
  start_y = gtk_widget_get_allocated_height (self->header_labels_box);

  gtk_style_context_get_padding (context, state, &padding);
  gtk_widget_get_allocation (widget, &alloc);

  if (!ltr)
    alloc.width -= gtk_widget_get_allocated_width (self->expand_button_box);

  gtk_style_context_get_color (context, state, &color);
  gdk_cairo_set_source_rgba (cr, &color);

  layout = pango_cairo_create_layout (cr);
  gtk_style_context_get (context, state, "font", &bold_font, NULL);
  pango_font_description_set_weight (bold_font, PANGO_WEIGHT_MEDIUM);
  pango_layout_set_font_description (layout, bold_font);

  week_start = get_start_of_week (self->active_date);
  week_end = g_date_time_add_days (week_start, 6);
  current_cell = icaltime_day_of_week (*(self->active_date)) - 1;
  current_cell = (7 + current_cell - self->first_weekday) % 7;
  today_column = get_today_column (self);

  cell_width = (alloc.width - start_x) / 7.0;

  /* Drag and Drop highlight */
  if (self->dnd_cell != -1)
    {
      gtk_drag_highlight (widget);
      gtk_render_background (context,
                             cr,
                             start_x + self->dnd_cell * cell_width,
                             start_y,
                             cell_width,
                             alloc.height - start_y + 6);

      gtk_drag_unhighlight (widget);
    }

  /* Draw the selection background */
  if (self->selection_start != -1 && self->selection_end != -1)
    {
      gint selection_width;
      gint selection_x;
      gint start, end;

      start = self->selection_start;
      end = self->selection_end;

      if (start > end)
        {
          start = start + end;
          end = start - end;
          start = start - end;
        }

      gtk_style_context_save (context);
      gtk_style_context_set_state (context, state | GTK_STATE_FLAG_SELECTED);

      selection_width = (end - start + 1) * cell_width;
      selection_x = ltr ? (start * cell_width) : (alloc.width - (start * cell_width + selection_width));

      gtk_render_background (context,
                             cr,
                             ALIGNED (start_x + selection_x) + 0.33,
                             start_y - 6,
                             ALIGNED (selection_width + 1),
                             alloc.height - start_y + 6);

      gtk_render_frame (context,
                        cr,
                        ALIGNED (start_x + selection_x) + 0.33,
                        start_y - 6,
                        ALIGNED (selection_width + 1),
                        alloc.height - start_y + 6);

      gtk_style_context_restore (context);
    }

  pango_layout_get_pixel_size (layout, NULL, &day_abv_font_height);

  for (i = 0; i < 7; i++)
    {
      WeatherInfoDay *wdinfo; /* unowned */
      gchar *weekday_date, *weekday_abv, *weekday;
      gdouble x;
      gint day_num_font_height, day_num_font_baseline;
      gint font_width;
      gint n_day;

      n_day = g_date_time_get_day_of_month (week_start) + i;

      if (n_day > g_date_get_days_in_month (g_date_time_get_month (week_start), g_date_time_get_year (week_start)))
        n_day = n_day - g_date_get_days_in_month (g_date_time_get_month (week_start), g_date_time_get_year (week_start));

      /* Draws the date of days in the week */
      weekday_date = g_strdup_printf ("%d", n_day);

      gtk_style_context_save (context);
      gtk_style_context_add_class (context, "week-dates");
      gtk_style_context_get (context, state, "font", &bold_font, NULL);

      if (i == today_column)
        gtk_style_context_add_class (context, "today");

      pango_layout_set_font_description (layout, bold_font);
      pango_layout_set_text (layout, weekday_date, -1);

      pango_layout_get_pixel_size (layout, &font_width, &day_num_font_height);
      day_num_font_baseline = pango_layout_get_baseline (layout) / PANGO_SCALE;

      if (ltr)
        x = padding.left + cell_width * i + COLUMN_PADDING + start_x;
      else
        x = alloc.width - (cell_width * i + font_width + COLUMN_PADDING + start_x);

      gtk_render_layout (context,
                         cr,
                         x,
                         day_abv_font_height + padding.bottom + start_y,
                         layout);

      gtk_style_context_restore (context);

      /* Draws the days name */
      weekday = g_utf8_strup (gcal_get_weekday ((i + self->first_weekday) % 7), -1);
      weekday_abv = g_strdup (weekday);
      g_free (weekday);

      gtk_style_context_save (context);
      gtk_style_context_add_class (context, "week-names");
      gtk_style_context_get (context, state, "font", &bold_font, NULL);

      if (i == today_column)
        gtk_style_context_add_class (context, "today");

      pango_layout_set_font_description (layout, bold_font);
      pango_layout_set_text (layout, weekday_abv, -1);

      pango_layout_get_pixel_size (layout, &font_width, NULL);

      if (ltr)
        x = padding.left + cell_width * i + COLUMN_PADDING + start_x;
      else
        x = alloc.width - (cell_width * i + font_width + COLUMN_PADDING + start_x);

      gtk_render_layout (context,
                         cr,
                         x,
                         start_y,
                         layout);

      gtk_style_context_restore (context);

      /* Draws weather icon if given */
      wdinfo = &self->weather_infos[ltr? i : 6 - i];
      if (wdinfo->winfo != NULL)
        {
          const gchar *weather_icon_name = gcal_weather_info_get_icon_name (wdinfo->winfo);
          const gchar *weather_temp = gcal_weather_info_get_temperature (wdinfo->winfo);

          /* Imagine a box around weather indicators with length MAX(imgW,tempW)
           * We compute its width and position in this section.
           * Its height is derived from day name and numbers. The icon sticks on
           * the very top and the temperature on its base.
           */
          gint icon_size = day_num_font_height - 3; /* available space - space between temperature and icon */
          gint temp_width = 0;
          gint temp_height = 0;
          gint temp_baseline = 0;

          gdouble wibox_width;
          gdouble wibox_x;

          if (weather_icon_name != NULL && wdinfo->icon_buf == NULL)
            {
              GtkIconTheme *theme; /* unowned */
              g_autoptr (GError) err = NULL;
              gint icon_flags;

              theme = gtk_icon_theme_get_default ();
              /* TODO: catch icon theme changes */

              icon_flags = ltr? GTK_ICON_LOOKUP_FORCE_SIZE | GTK_ICON_LOOKUP_DIR_LTR
                              : GTK_ICON_LOOKUP_FORCE_SIZE | GTK_ICON_LOOKUP_DIR_RTL;
              wdinfo->icon_buf = gtk_icon_theme_load_icon (theme, weather_icon_name, icon_size, icon_flags, &err);
              if (err != NULL)
                {
                  g_assert (wdinfo->icon_buf == NULL);
                  g_warning ("Could not load icon %s: %s", weather_icon_name, err->message);
                }
            }

          if (G_LIKELY (weather_temp != NULL))
            {
              gtk_style_context_save (context);
              gtk_style_context_add_class (context, "week-temperature");
              gtk_style_context_get (context, state, "font", &bold_font, NULL);

              pango_layout_set_font_description (layout, bold_font);
              pango_layout_set_text (layout, weather_temp, -1);

              pango_layout_get_pixel_size (layout, &temp_width, &temp_height);
              temp_baseline = pango_layout_get_baseline (layout) / PANGO_SCALE;
            }

          wibox_width = MAX(icon_size, temp_width);
          wibox_x = ltr ? alloc.width - (cell_width * (6 - i) + wibox_width + COLUMN_PADDING)
                        : padding.left + cell_width * i + COLUMN_PADDING + start_x;

          /* Actually draw weather indicator: */
          if (G_LIKELY (weather_temp != NULL))
            {
              gtk_render_layout (context,
                                 cr,
                                 wibox_x + (wibox_width - temp_width) / 2,
                                 day_abv_font_height + padding.bottom + start_y + day_num_font_baseline - temp_baseline,
                                 layout);
              gtk_style_context_restore (context);
            }

          if (G_LIKELY (wdinfo->icon_buf != NULL))
            {
              gdk_cairo_set_source_pixbuf (cr,
                                           wdinfo->icon_buf,
                                           wibox_x + (wibox_width - icon_size) / 2,
                                           start_y);
              cairo_paint (cr);
            }
        }


      /* Draws the lines after each day of the week */
      gtk_style_context_save (context);
      gtk_style_context_add_class (context, "lines");

      gtk_style_context_get_color (context, state, &color);
      gdk_cairo_set_source_rgba (cr, &color);

      cairo_set_line_width (cr, 0.25);

      cairo_move_to (cr,
                     ALIGNED (ltr ? (cell_width * i + start_x) : (alloc.width - (cell_width * i + start_x))),
                     day_abv_font_height + padding.bottom + start_y);

      cairo_rel_line_to (cr,
                         0.0,
                         gtk_widget_get_allocated_height (widget) - day_abv_font_height - start_y + padding.bottom);
      cairo_stroke (cr);

      gtk_style_context_restore (context);

      g_free (weekday_date);
      g_free (weekday_abv);
    }

  cairo_restore (cr);

  pango_font_description_free (bold_font);
  g_object_unref (layout);

  GTK_WIDGET_CLASS (gcal_week_header_parent_class)->draw (widget, cr);

  g_clear_pointer (&week_start, g_date_time_unref);
  g_clear_pointer (&week_end, g_date_time_unref);

  return FALSE;
}


static gint
get_dnd_cell (GtkWidget *widget,
              gint       x,
              gint       y)
{
  gdouble column_width;

  column_width = gtk_widget_get_allocated_width (widget) / 7.0;

  return x / column_width;
}

static gboolean
gcal_week_header_drag_motion (GtkWidget      *widget,
                              GdkDragContext *context,
                              gint            x,
                              gint            y,
                              guint           time)
{
  GcalWeekHeader *self;

  self = GCAL_WEEK_HEADER (widget);
  self->dnd_cell = get_dnd_cell (widget, x, y);

  /*
   * Sets the status of the drag - if it fails, sets the action to 0 and
   * aborts the drag with FALSE.
   */
  gdk_drag_status (context,
                   self->dnd_cell == -1 ? 0 : GDK_ACTION_MOVE,
                   time);

  gtk_widget_queue_draw (widget);

  return self->dnd_cell != -1;
}

static gboolean
gcal_week_header_drag_drop (GtkWidget      *widget,
                            GdkDragContext *context,
                            gint            x,
                            gint            y,
                            guint           time)
{
  GcalWeekHeader *self;
  g_autoptr (GDateTime) week_start;
  g_autoptr (GDateTime) dnd_date;
  g_autoptr (GDateTime) new_end;
  g_autoptr (GDateTime) tmp_dt;
  GDateTime *start_date;
  GDateTime *end_date;
  GTimeSpan difference;
  GtkWidget *event_widget;
  GcalEvent *event;
  gboolean turn_all_day;
  gboolean ltr;
  gint drop_cell;

  GCAL_ENTRY;

  self = GCAL_WEEK_HEADER (widget);
  ltr = gtk_widget_get_direction (widget) != GTK_TEXT_DIR_RTL;
  drop_cell = get_dnd_cell (widget, x, y);
  event_widget = gtk_drag_get_source_widget (context);

  week_start = NULL;
  dnd_date = NULL;
  new_end = NULL;
  tmp_dt = NULL;

  if (!GCAL_IS_EVENT_WIDGET (event_widget))
    return FALSE;

  /* RTL languages swap the drop cell column */
  if (!ltr)
    drop_cell = 6 - drop_cell;

  event = gcal_event_widget_get_event (GCAL_EVENT_WIDGET (event_widget));
  start_date = gcal_event_get_date_start (event);
  end_date = gcal_event_get_date_end (event);
  week_start = get_start_of_week (self->active_date);

  turn_all_day = !gcal_event_is_multiday (event) || gcal_event_get_all_day (event);

  if (!turn_all_day)
    {
      /*
       * The only case where we don't touch the timezone is for
       * timed, multiday events.
       */
      tmp_dt = g_date_time_new (gcal_event_get_timezone (event),
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
  dnd_date = g_date_time_add_days (tmp_dt, drop_cell);

  /* End date */
  difference = turn_all_day ? 24 : g_date_time_difference (end_date, start_date) / G_TIME_SPAN_HOUR;

  new_end = g_date_time_add_hours (dnd_date, difference);
  gcal_event_set_date_end (event, new_end);

  /*
   * Set the start date ~after~ the end date, so we can compare
   * the event's start and end dates above
   */
  gcal_event_set_date_start (event, dnd_date);

  if (turn_all_day)
    gcal_event_set_all_day (event, TRUE);

  /* Commit the changes */
  gcal_manager_update_event (self->manager, event, GCAL_RECURRENCE_MOD_THIS_ONLY);

  /* Cancel the DnD */
  self->dnd_cell = -1;

  gtk_drag_finish (context, TRUE, FALSE, time);

  gtk_widget_queue_draw (widget);

  GCAL_RETURN (TRUE);
}

static void
gcal_week_header_drag_leave (GtkWidget      *widget,
                             GdkDragContext *context,
                             guint           time)
{
  GcalWeekHeader *self = GCAL_WEEK_HEADER (widget);

  /* Cancel the drag */
  self->dnd_cell = -1;

  gtk_widget_queue_draw (widget);
}

static void
gcal_week_header_class_init (GcalWeekHeaderClass *kclass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (kclass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (kclass);

  object_class->finalize = gcal_week_header_finalize;
  object_class->get_property = gcal_week_header_get_property;
  object_class->set_property = gcal_week_header_set_property;

  widget_class->draw = gcal_week_header_draw;
  widget_class->size_allocate = gcal_week_header_size_allocate;
  widget_class->drag_motion = gcal_week_header_drag_motion;
  widget_class->drag_leave = gcal_week_header_drag_leave;
  widget_class->drag_drop = gcal_week_header_drag_drop;

  /**
   * GcalWeekHeader::weather-service:
   *
   * The #GcalWeatherService of the view.
   */
  g_object_class_install_property (G_OBJECT_CLASS (kclass),
                                   PROP_WEATHER_SERVICE,
                                   g_param_spec_object ("weather-service",
                                                        "The weather service",
                                                        "The weather service of the view",
                                                        GCAL_TYPE_WEATHER_SERVICE,
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  signals[EVENT_ACTIVATED] = g_signal_new ("event-activated",
                                           GCAL_TYPE_WEEK_HEADER,
                                           G_SIGNAL_RUN_FIRST,
                                           0,  NULL, NULL, NULL,
                                           G_TYPE_NONE,
                                           1,
                                           GCAL_TYPE_EVENT_WIDGET);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/week-header.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalWeekHeader, expand_button);
  gtk_widget_class_bind_template_child (widget_class, GcalWeekHeader, expand_button_box);
  gtk_widget_class_bind_template_child (widget_class, GcalWeekHeader, expand_button_image);
  gtk_widget_class_bind_template_child (widget_class, GcalWeekHeader, grid);
  gtk_widget_class_bind_template_child (widget_class, GcalWeekHeader, header_labels_box);
  gtk_widget_class_bind_template_child (widget_class, GcalWeekHeader, month_label);
  gtk_widget_class_bind_template_child (widget_class, GcalWeekHeader, scrolledwindow);
  gtk_widget_class_bind_template_child (widget_class, GcalWeekHeader, sizegroup);
  gtk_widget_class_bind_template_child (widget_class, GcalWeekHeader, week_label);
  gtk_widget_class_bind_template_child (widget_class, GcalWeekHeader, year_label);

  gtk_widget_class_bind_template_callback (widget_class, on_button_pressed);
  gtk_widget_class_bind_template_callback (widget_class, on_button_released);
  gtk_widget_class_bind_template_callback (widget_class, on_expand_action_activated);
  gtk_widget_class_bind_template_callback (widget_class, on_motion_notify);

  gtk_widget_class_set_css_name (widget_class, "calendar-view");
}

static void
gcal_week_header_init (GcalWeekHeader *self)
{
  self->expanded = FALSE;
  self->selection_start = -1;
  self->selection_end = -1;
  self->dnd_cell = -1;
  self->weather_service = NULL;
  memset (self->weather_infos, 0, sizeof(self->weather_infos));
  gtk_widget_init_template (GTK_WIDGET (self));

  /* This is to avoid stray lines when adding and removing events */
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_container_set_reallocate_redraws (GTK_CONTAINER (self), TRUE);
G_GNUC_END_IGNORE_DEPRECATIONS

  /* Setup the week header as a drag n' drop destination */
  gtk_drag_dest_set (GTK_WIDGET (self),
                     0,
                     NULL,
                     0,
                     GDK_ACTION_MOVE);
}

/* Private API */

/* gcal_week_header_add_weather_infos:
 * @self:  (not nullable): The #GcalWeekHeader instance.
 * @winfo: List of #GcalWeatherInfos to add to @self.
 *
 * Adds weather information to this header. @self
 * only adds weather information that can be
 * displayed with current settings.
 *
 * Return: Number of consumed weather information
 *         objects.
 */
static gint
gcal_week_header_add_weather_infos (GcalWeekHeader *self,
                                    GSList         *winfos)
{
  g_autoptr (GDateTime) _week_start = NULL;
  GSList *iter; /* unowned */
  GDate week_start;
  int consumed = 0;

  g_return_val_if_fail (self != NULL, 0);

  _week_start = get_start_of_week (self->active_date);
  g_date_set_dmy (&week_start,
                  g_date_time_get_day_of_month (_week_start),
                  g_date_time_get_month (_week_start),
                  g_date_time_get_year (_week_start));

  for (iter = winfos; iter != NULL; iter = iter->next)
    {
      GcalWeatherInfo *gwi; /* unowned */
      GDate gwi_date;
      gint day_diff;

      gwi = GCAL_WEATHER_INFO (iter->data);
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

/* gcal_week_header_clear_weather_infos:
 * @self: The #GcalWeekHeader instance.
 *
 * Removes all registered weather information objects from
 * this widget.
 */
static void
gcal_week_header_clear_weather_infos (GcalWeekHeader *self)
{
  g_return_if_fail (GCAL_IS_WEEK_HEADER (self));

  for (gint i = 0; i < G_N_ELEMENTS (self->weather_infos); i++)
    wid_clear (&self->weather_infos[i]);

  gtk_widget_queue_draw (GTK_WIDGET (self));
}


/* Public API */

/**
 * gcal_week_header_set_weather_service:
 * @self:    The #GcalWeekHeader instance.
 * @service: (nullable): The weather service to query.
 *
 * Note that #GcalWeekHeader does not hold a strong reference
 * to its weather service.
 */
void
gcal_week_header_set_weather_service (GcalWeekHeader     *self,
                                      GcalWeatherService *service)
{
  g_return_if_fail (GCAL_IS_WEEK_HEADER (self));
  g_return_if_fail (service == NULL || GCAL_IS_WEATHER_SERVICE (service));

  gcal_view_set_weather_service_impl_helper (&self->weather_service,
                                             service,
                                             (GcalWeatherUpdateFunc) gcal_week_header_update_weather_infos,
                                             (GCallback) on_weather_update,
                                             GTK_WIDGET (self));
}

/**
 * gcal_week_header_update_weather_infos:
 * @self: The #GcalWeekHeader instance.
 *
 * Retrieves latest weather information from registered
 * weather service and displays it.
 */
void
gcal_week_header_update_weather_infos (GcalWeekHeader *self)
{

  g_return_if_fail (GCAL_IS_WEEK_HEADER (self));

  gcal_week_header_clear_weather_infos (self);

  if (self->weather_service != NULL)
    {
      GSList* weather_infos = NULL; /* unowned */

      weather_infos = gcal_weather_service_get_weather_infos (self->weather_service);
      gcal_week_header_add_weather_infos (self, weather_infos);
    }
}

void
gcal_week_header_set_manager (GcalWeekHeader *self,
                              GcalManager    *manager)
{
  g_return_if_fail (GCAL_IS_WEEK_HEADER (self));

  self->manager = manager;

  g_signal_connect_swapped (gcal_manager_get_clock (manager),
                            "day-changed",
                            G_CALLBACK (gtk_widget_queue_draw),
                            self);
}

void
gcal_week_header_set_first_weekday (GcalWeekHeader *self,
                                    gint            nr_day)
{
  g_return_if_fail (GCAL_IS_WEEK_HEADER (self));

  self->first_weekday = nr_day;
}

void
gcal_week_header_set_use_24h_format (GcalWeekHeader *self,
                                     gboolean        use_24h_format)
{
  g_return_if_fail (GCAL_IS_WEEK_HEADER (self));

  self->use_24h_format = use_24h_format;
}

void
gcal_week_header_add_event (GcalWeekHeader *self,
                            GcalEvent      *event)
{
  g_autoptr (GDateTime) start_date = NULL;
  g_autoptr (GDateTime) end_date = NULL;
  g_autoptr (GDateTime) week_start = NULL;
  g_autoptr (GDateTime) week_end = NULL;
  gboolean all_day;
  gint start, end;

  g_return_if_fail (GCAL_IS_WEEK_HEADER (self));

  all_day = gcal_event_get_all_day (event);
  week_start = get_start_of_week (self->active_date);
  week_end = get_end_of_week (self->active_date);

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

      start_date = g_date_time_ref (gcal_event_get_date_start (event));
      end_date = g_date_time_ref (gcal_event_get_date_end (event));

      /*
       * Switch the week start and end by the UTC variants, in
       * order to correctly compare all-day events.
       */
      aux = week_start;
      week_start = utc_week_start;
      gcal_clear_datetime (&aux);

      aux = week_end;
      week_end = utc_week_end;
      gcal_clear_datetime (&aux);

    }
  else
    {
      start_date = g_date_time_to_local (gcal_event_get_date_start (event));
      end_date = g_date_time_to_local (gcal_event_get_date_end (event));
    }

  /* Start position */
  if (datetime_compare_date (start_date, week_start) >= 0)
    start = floor (g_date_time_difference (start_date, week_start) / G_TIME_SPAN_DAY);
  else
    start = 0;

  /* End position */
  if (g_date_time_compare (end_date, week_end) <= 0)
    end = floor (g_date_time_difference (end_date, week_start) / G_TIME_SPAN_DAY) - all_day;
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
  GcalEvent *removed_event;
  GList *children, *l;
  gint weekday;

  g_return_if_fail (GCAL_IS_WEEK_HEADER (self));

  removed_event = get_event_by_uuid (self, uuid);

  if (!removed_event)
    return;

  children = gtk_container_get_children (GTK_CONTAINER (self->grid));

  for (l = children; l != NULL; l = l->next)
    {
      GcalEventWidget *child_widget;
      GcalEvent *event;

      if (!GCAL_IS_EVENT_WIDGET (l->data))
        continue;

      child_widget = GCAL_EVENT_WIDGET (l->data);
      event = gcal_event_widget_get_event (child_widget);

      if (g_strcmp0 (uuid, gcal_event_get_uid (event)) == 0)
        destroy_event_widget (self, l->data);
    }

  /* Remove from the weekday's GList */
  for (weekday = 0; weekday < 7; weekday++)
    {
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

  g_clear_pointer (&children, g_list_free);
}

GList*
gcal_week_header_get_children_by_uuid (GcalWeekHeader        *self,
                                       GcalRecurrenceModType  mod,
                                       const gchar           *uuid)
{
  GList *children, *result;

  GCAL_ENTRY;

  children = gtk_container_get_children (GTK_CONTAINER (self->grid));
  result = filter_event_list_by_uid_and_modtype (children, mod, uuid);

  g_list_free (children);

  GCAL_RETURN (result);
}

GtkSizeGroup*
gcal_week_header_get_sidebar_size_group (GcalWeekHeader *self)
{
  g_return_val_if_fail (GCAL_IS_WEEK_HEADER (self), NULL);

  return self->sizegroup;
}

void
gcal_week_header_clear_marks (GcalWeekHeader *self)
{
  g_return_if_fail (GCAL_IS_WEEK_HEADER (self));

  self->selection_start = -1;
  self->selection_end = -1;

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

void
gcal_week_header_set_date (GcalWeekHeader *self,
                           icaltimetype   *date)
{
  icaltimetype *old_date, *new_date;

  old_date = self->active_date;
  new_date = gcal_dup_icaltime (date);

  /*
   * If the active date changed, but we're still in the same week,
   * there's no need to recalculate visible events.
   */
  if (old_date && new_date &&
      old_date->year == new_date->year &&
      icaltime_week_number (*old_date) == icaltime_week_number (*new_date))
    {
      g_free (new_date);
      return;
    }

  self->active_date = new_date;

  update_title (self);
  gtk_widget_queue_draw (GTK_WIDGET (self));

  if (old_date)
    update_unchanged_events (self, self->active_date);

  gcal_week_header_update_weather_infos (self);

  g_clear_pointer (&old_date, g_free);
}

/**
 * gcal_week_header_get_weather_infos:
 * @self: The #GcalWeekHeader instance.
 *
 * Returns a list of shown weather informations.
 *
 * @Returns: (transfer container):
 *           A GSList. The callee is responsible for freeing it.
 *           Elements are owned by the widget. Do not modify.
 */
GSList*
gcal_week_header_get_shown_weather_infos (GcalWeekHeader  *self)
{
  g_return_val_if_fail (GCAL_IS_WEEK_HEADER (self), NULL);
  GSList* lst = NULL; /* owned[unowned] */

  for (int i = 0; i < G_N_ELEMENTS (self->weather_infos); i++)
    {
      if (self->weather_infos[i].winfo != NULL)
        lst = g_slist_prepend (lst, self->weather_infos[i].winfo);
    }

   return lst;
}
