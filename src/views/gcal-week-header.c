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

#include "gcal-week-header.h"
#include "gcal-week-view.h"
#include "gcal-utils.h"
#include "gcal-view.h"
#include "gcal-event-widget.h"

#include <glib/gi18n.h>
#include <string.h>
#include <math.h>

#define COLUMN_PADDING       6

struct _GcalWeekHeader
{
  GtkGrid           parent;

  GtkWidget        *grid;
  GtkWidget        *draw_area;
  GtkWidget        *month_label;
  GtkWidget        *week_label;
  GtkWidget        *year_label;
  GtkWidget        *scrolledwindow;
  GtkWidget        *expand_button;
  GtkWidget        *expand_button_box;
  GtkWidget        *expand_button_image;

  GcalManager      *manager;

  /*
   * Stores the events as they come from the week-view
   * The list will later be iterated after the active date is changed
   * and the events will be placed
   */
  GList            *events[7];

  gint              first_weekday;

  /*
   * Used for checking if the header is in collapsed state or expand state
   * false is collapse state true is expand state
   */
  gboolean          expanded;

  gboolean          use_24h_format;

  icaltimetype     *active_date;
  icaltimetype     *current_date;

  GtkSizeGroup     *sizegroup;
};

static void           header_collapse                       (GcalWeekHeader *self);

static void           header_expand                         (GcalWeekHeader *self);

static void           on_expand_action_activated            (GcalWeekHeader *self,
                                                             gpointer        user_data);

static void           gcal_week_header_finalize             (GObject *object);

static void           gcal_week_header_get_property         (GObject    *object,
                                                             guint       prop_id,
                                                             GValue     *value,
                                                             GParamSpec *psec);

static void           gcal_week_header_set_property         (GObject      *object,
                                                             guint         prop_id,
                                                             const GValue *value,
                                                             GParamSpec   *pspec);

static void           gcal_week_header_size_allocate        (GtkWidget     *widget,
                                                             GtkAllocation *alloc);

static gboolean       gcal_week_header_draw                 (GcalWeekHeader *self,
                                                             cairo_t        *cr,
                                                             GtkWidget      *widget);

enum
{
  PROP_0,
  PROP_ACTIVE_DATE
};

typedef enum
{
  UP,
  DOWN
} MoveDirection;

G_DEFINE_TYPE (GcalWeekHeader, gcal_week_header, GTK_TYPE_GRID);

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

static GDateTime*
get_start_of_week (icaltimetype *date)
{
  icaltimetype *new_date;

  new_date = g_new0 (icaltimetype, 1);
  *new_date = icaltime_from_day_of_year (icaltime_start_doy_week (*(date), get_first_weekday () + 1),
                                         date->year);
  new_date->is_date = 0;
  new_date->hour = 0;
  new_date->minute = 0;
  new_date->second = 0;

  *new_date = icaltime_set_timezone (new_date, date->zone);

  return icaltime_to_datetime (new_date);
}

static GDateTime*
get_end_of_week (icaltimetype *date)
{
  GDateTime *week_start, *week_end;

  week_start = get_start_of_week (date);
  week_end = g_date_time_add_days (week_start, 7);

  g_clear_pointer (&week_start, g_date_time_unref);

  return week_end;
}

static gint
get_event_start_weekday (GcalWeekHeader *self,
                         GcalEvent      *event)
{
  GDateTime *week_start, *event_start;
  gint start_weekday;

  week_start = get_start_of_week (self->current_date);
  event_start = gcal_event_get_date_start (event);

  /* Only calculate the start day if it starts in the current week */
  if (g_date_time_compare (event_start, week_start) >= 0)
    start_weekday = (g_date_time_get_day_of_week (event_start) - get_first_weekday ()) % 7;
  else
    start_weekday = 0;

  g_clear_pointer (&week_start, g_date_time_unref);

  return start_weekday;
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

static void
merge_events (GcalWeekHeader *self,
              GtkWidget      *event,
              GtkWidget      *to_be_removed)
{
  gint deleted_width, current_width;

  /* Retrieve the current sizes */
  gtk_container_child_get (GTK_CONTAINER (self->grid),
                           to_be_removed,
                           "width", &deleted_width,
                           NULL);

  gtk_container_child_get (GTK_CONTAINER (self->grid),
                           event,
                           "width", &current_width,
                           NULL);

  gtk_widget_destroy (to_be_removed);

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

              if (current_widget == to_be_removed)
                continue;

              merge_events (self, current_widget, to_be_removed);
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

      gtk_grid_attach (GTK_GRID (self->grid),
                       widget_before,
                       left_attach,
                       top_attach,
                       column - left_attach,
                       1);

      new_width = old_width - column + left_attach;
      old_width = new_width;
      left_attach = column;

      /* Update the current event position and size */
      gtk_container_child_set (GTK_CONTAINER (self->grid),
                               widget,
                               "left_attach", left_attach,
                               "width", new_width,
                               NULL);

      gtk_widget_show (widget_before);
    }

  /* Create a new widget after the current widget */
  if (create_after)
    {
      GtkWidget *widget_after;

      widget_after = gcal_event_widget_clone (GCAL_EVENT_WIDGET (widget));
      gcal_event_widget_set_date_start (GCAL_EVENT_WIDGET (widget_after), end_column_date);
      gcal_event_widget_set_date_end (GCAL_EVENT_WIDGET (widget_after),
                                      gcal_event_widget_get_date_end (GCAL_EVENT_WIDGET (widget)));

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

      gtk_widget_show (widget_after);
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
    }

  g_clear_pointer (&children, g_list_free);
}

static void
add_event_to_grid (GcalWeekHeader *self,
                   GcalEvent      *event,
                   gint            start,
                   gint            end)
{
  GDateTime *week_start, *week_end, *widget_end_dt;
  GtkWidget *widget;
  gint position;
  gint i;

  /* Add at least at the first weekday */
  position = add_event_to_weekday (self, event, start);

  move_events_at_column (self, DOWN, start, position);

  /* Add the event to the grid */
  widget = gcal_event_widget_new (event);
  gtk_widget_set_margin_end (widget, end == 6 ? 0 : 6);

  gtk_grid_attach (GTK_GRID (self->grid),
                   widget,
                   start,
                   position + 1,
                   1,
                   1);

  gtk_widget_show (widget);

  /* Single-day events are the simplest case, and the code above is enough to deal with them */
  if (!gcal_event_is_multiday (event))
    return;

  week_start = get_start_of_week (self->active_date);
  week_end = get_end_of_week (self->active_date);

  /* Setup the event's start date */
  widget_end_dt = g_date_time_add_days (week_start, 1);

  gcal_event_widget_set_date_start (GCAL_EVENT_WIDGET (widget), week_start);
  gcal_event_widget_set_date_end (GCAL_EVENT_WIDGET (widget), widget_end_dt);

  /*
   * In addition to moving the current column's events below, multiday
   * events must also move the events from ~all~ the columns it spans
   * below
   */
  for (i = start + 1; i <= end; i++)
    {
      GDateTime *new_widget_end_dt;
      gint new_position;

      new_widget_end_dt = g_date_time_add_days (widget_end_dt, 1);

      /* Add the event to that day */
      new_position = add_event_to_weekday (self, event, i);

      move_events_at_column (self, DOWN, i, new_position);

      /* Add the event to the grid */
      if (new_position == position)
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

          gtk_grid_attach (GTK_GRID (self->grid),
                           cloned_widget,
                           i,
                           new_position + 1,
                           1,
                           1);

          gcal_event_widget_set_date_start (GCAL_EVENT_WIDGET (cloned_widget), cloned_widget_start_dt);

          /* From now on, let's modify this widget */
          widget = cloned_widget;

          g_clear_pointer (&cloned_widget_start_dt, g_date_time_unref);
        }

      /* Update the widget's end date */
      gcal_event_widget_set_date_end (GCAL_EVENT_WIDGET (widget), new_widget_end_dt);

      g_clear_pointer (&widget_end_dt, g_date_time_unref);

      widget_end_dt = new_widget_end_dt;
    }

  gtk_widget_show (widget);

  g_clear_pointer (&widget_end_dt, g_date_time_unref);
  g_clear_pointer (&week_start, g_date_time_unref);
  g_clear_pointer (&week_end, g_date_time_unref);
}

static void
update_unchanged_events (GcalWeekHeader *self,
                         icaltimetype   *old_icaldt,
                         icaltimetype   *new_icaldt)
{
  GDateTime *old_week_start, *old_week_end;
  GDateTime *new_week_start, *new_week_end;
  GList *events_to_update, *l;
  gint weekday;

  events_to_update = NULL;

  old_week_start = get_start_of_week (old_icaldt);
  old_week_end = get_end_of_week (old_icaldt);

  new_week_start = get_start_of_week (new_icaldt);
  new_week_end = get_end_of_week (new_icaldt);

  for (weekday = 0; weekday < 7; weekday++)
    {
      GList *events;

      events = self->events[weekday];

      for (l = events; l != NULL; l = l->next)
        {
          GDateTime *event_start, *event_end;

          event_start = gcal_event_get_date_start (l->data);
          event_end = gcal_event_get_date_end (l->data);

          /*
           * Check if the event must be updated. If we're going to the future, updatable events
           * are events that started somewhere in the past, and are still present. If we're
           * going to the past, updatable events are events that started
           */
          if (g_date_time_compare (event_start, new_week_end) <= 0 &&
              g_date_time_compare (event_end, new_week_start) > 0 &&
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
  g_clear_pointer (&new_week_start, g_date_time_unref);
  g_clear_pointer (&old_week_start, g_date_time_unref);
  g_clear_pointer (&new_week_end, g_date_time_unref);
  g_clear_pointer (&old_week_end, g_date_time_unref);
}

static void
update_title (GcalWeekHeader *self)
{
  GDateTime *week_start, *week_end;
  gchar *year_label, *month_label, *week_label;

  if(!self->active_date)
    return;

  week_start = get_start_of_week (self->active_date);
  week_end = g_date_time_add_days (week_start, 6);

  if (g_date_time_get_month (week_start) == g_date_time_get_month (week_end))
    {
      month_label = g_strdup_printf ("%s ", gcal_get_month_name (g_date_time_get_month (week_start) - 1));
    }
  else
    {
      month_label = g_strdup_printf("%s - %s ",
                                    gcal_get_month_name (g_date_time_get_month (week_start) -1),
                                    gcal_get_month_name (g_date_time_get_month (week_end) - 1));
    }

  if (g_date_time_get_year (week_start) == g_date_time_get_year (week_end))
    {
      week_label = g_strdup_printf ("week %d", g_date_time_get_week_of_year (week_start));
      year_label = g_strdup_printf ("%d", g_date_time_get_year (week_start));
    }
  else
    {
      week_label = g_strdup_printf ("week %d/%d",
                                    g_date_time_get_week_of_year (week_start),
                                    g_date_time_get_week_of_year (week_end));
      year_label = g_strdup_printf ("%d-%d",
                                    g_date_time_get_year (week_start),
                                    g_date_time_get_year (week_end));
    }

  gtk_label_set_label (GTK_LABEL (self->month_label), month_label);
  gtk_label_set_label (GTK_LABEL (self->week_label), week_label);
  gtk_label_set_label (GTK_LABEL (self->year_label), year_label);

  g_clear_pointer (&week_start, g_date_time_unref);
  g_clear_pointer (&week_end, g_date_time_unref);
  g_clear_pointer (&month_label, g_free);
  g_clear_pointer (&week_label, g_free);
  g_clear_pointer (&year_label, g_free);
}

static void
header_collapse (GcalWeekHeader *self)
{
  gint hidden_events[7], row, i;

  self->expanded = FALSE;

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (self->scrolledwindow),
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_NEVER);
  gtk_scrolled_window_set_max_content_height (GTK_SCROLLED_WINDOW (self->scrolledwindow), -1);
  gtk_image_set_from_icon_name (GTK_IMAGE (self->expand_button_image), "go-down-symbolic", 4);

  for (i = 0; i < 7; i++)
    hidden_events[i] = 0;

  /* Gets count of number of extra events in each column */
  for (row = 4; ; row++)
    {
      gboolean empty;

      empty = TRUE;

      for (i = 0; i < 7; i++)
        {
          if (gtk_grid_get_child_at (GTK_GRID (self->grid), i, row))
            {
              hidden_events[i]++;
              empty = FALSE;
            }
        }

      if (empty)
        break;
    }

  /* Removes the excess events and adds them to the object list of events */
  for ( ; row >= 4; row--)
    {
      for (i = 0; i < 7; i++)
      {
        if (gtk_grid_get_child_at (GTK_GRID (self->grid), i, row))
          {
            GcalEventWidget *widget;

            widget = GCAL_EVENT_WIDGET (gtk_grid_get_child_at (GTK_GRID (self->grid), i, row));

            gtk_widget_destroy (GTK_WIDGET (widget));
          }
      }
    }

  /* Adds labels in the last row */
  for (i = 0; i < 7; i++)
    {
      if (hidden_events[i])
        {
          GtkWidget *widget;
          gchar *other_events_label;

          other_events_label = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE, "Other event", "Other %d events", hidden_events[i]),
                                                hidden_events[i]);
          widget = gtk_label_new (other_events_label);

          gtk_widget_set_visible (widget, TRUE);
          gtk_grid_attach (GTK_GRID (self->grid), widget, i, 4, 1, 1);
        }
    }
}

static void
header_expand (GcalWeekHeader *self)
{
  GtkWidget *week_view;

  week_view = gtk_widget_get_ancestor (GTK_WIDGET (self), GCAL_TYPE_WEEK_VIEW);

  self->expanded = TRUE;

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (self->scrolledwindow),
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_max_content_height (GTK_SCROLLED_WINDOW (self->scrolledwindow),
                                              gtk_widget_get_allocated_height (week_view)/2);
  gtk_image_set_from_icon_name (GTK_IMAGE (self->expand_button_image), "go-up-symbolic", 4);

  gtk_grid_remove_row (GTK_GRID (self->grid), 4);
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

static void
gcal_week_header_finalize (GObject *object)
{
  GcalWeekHeader *self = GCAL_WEEK_HEADER (object);
  gint i;

  g_clear_pointer (&self->current_date, g_free);

  for (i = 0; i < 7; i++)
    g_list_free (self->events[i]);
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
    case PROP_ACTIVE_DATE:
      g_value_set_boxed (value, self->active_date);
      return;

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
  icaltimetype *old_date;

  switch (prop_id)
    {
    case PROP_ACTIVE_DATE:
      old_date = self->active_date;
      self->active_date = g_value_dup_boxed (value);

      update_title (self);
      gtk_widget_queue_draw (self->draw_area);

      if (old_date)
        update_unchanged_events (self, old_date, self->active_date);

      g_clear_pointer (&old_date, g_free);
      return;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_week_header_size_allocate (GtkWidget     *widget,
                                GtkAllocation *alloc)
{
  GcalWeekHeader *self = GCAL_WEEK_HEADER (widget);
  PangoFontDescription *bold_font;
  GtkStyleContext *context;
  GtkStateFlags state;
  GtkAllocation draw_alloc;

  context = gtk_widget_get_style_context (self->draw_area);
  state = gtk_widget_get_state_flags (self->draw_area);

  gtk_widget_get_allocation (self->draw_area, &draw_alloc);

  gtk_style_context_get (context, state, "font", &bold_font, NULL);
  pango_font_description_set_weight (bold_font, PANGO_WEIGHT_SEMIBOLD);

  gtk_widget_set_margin_top (self->scrolledwindow,
                             (4 * pango_font_description_get_size (bold_font)) / PANGO_SCALE);

  GTK_WIDGET_CLASS (gcal_week_header_parent_class)->size_allocate (widget, alloc);
}

static gboolean
gcal_week_header_draw (GcalWeekHeader *self,
                       cairo_t        *cr,
                       GtkWidget      *widget)
{
  GtkStyleContext *context;
  GtkStateFlags state;
  GdkRGBA color;
  GtkAllocation alloc;
  GtkBorder padding;

  GDateTime *week_start, *week_end;

  PangoLayout *layout;
  PangoFontDescription *bold_font;

  gint i, font_height, current_cell;
  gdouble cell_width;

  cairo_save(cr);

  /* Fonts and colour selection */
  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);

  gtk_style_context_get_padding (context, state, &padding);
  gtk_widget_get_allocation (widget, &alloc);

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

  cell_width = alloc.width / 7;
  pango_layout_get_pixel_size (layout, NULL, &font_height);

  for (i = 0; i < 7; i++)
    {
      gchar *weekday_date, *weekday_abv;
      gint n_day;

      n_day = g_date_time_get_day_of_month (week_start) + i;

      if (n_day > g_date_get_days_in_month (g_date_time_get_month (week_start), g_date_time_get_year (week_start)))
        n_day = n_day - g_date_get_days_in_month (g_date_time_get_month (week_start), g_date_time_get_year (week_start));

      /* Draws the date of days in the week */
      weekday_date = g_strdup_printf ("%d", n_day);

      gtk_style_context_save (context);
      gtk_style_context_add_class (context, "week-dates");
      gtk_style_context_get (context, state, "font", &bold_font, NULL);
      gtk_style_context_get_color (context, state, &color);
      gdk_cairo_set_source_rgba (cr, &color);

      pango_layout_set_font_description (layout, bold_font);
      pango_layout_set_text (layout, weekday_date, -1);
      cairo_move_to (cr,
                     padding.left + cell_width * i + COLUMN_PADDING,
                     font_height + padding.bottom);
      pango_cairo_show_layout (cr,layout);

      gtk_style_context_restore (context);

      /* Draws the days name */
      weekday_abv = g_strdup_printf ("%s", g_utf8_strup (gcal_get_weekday ((i + self->first_weekday) % 7), -1));

      gtk_style_context_save (context);
      gtk_style_context_add_class (context, "week-names");
      gtk_style_context_get (context, state, "font", &bold_font, NULL);

      pango_layout_set_font_description (layout, bold_font);
      pango_layout_set_text (layout, weekday_abv, -1);
      cairo_move_to (cr,
                     padding.left + cell_width * i + COLUMN_PADDING,
                     0);
      pango_cairo_show_layout (cr, layout);

      gtk_style_context_restore (context);

      /* Draws the lines after each day of the week */
      cairo_move_to (cr,
                     cell_width * i + 0.5,
                     font_height + padding.bottom);

      cairo_set_line_width (cr, 0.25);
      cairo_rel_line_to (cr, 0.0, gtk_widget_get_allocated_height (self->draw_area) - font_height + padding.bottom);
      cairo_stroke (cr);

      g_free (weekday_date);
      g_free (weekday_abv);
    }

  cairo_restore (cr);

  pango_font_description_free (bold_font);
  g_object_unref (layout);

  g_clear_pointer (&week_start, g_date_time_unref);
  g_clear_pointer (&week_end, g_date_time_unref);

  return FALSE;
}

static void
gcal_week_header_class_init (GcalWeekHeaderClass *kclass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (kclass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (kclass);

  object_class->finalize = gcal_week_header_finalize;
  object_class->get_property = gcal_week_header_get_property;
  object_class->set_property = gcal_week_header_set_property;

  widget_class->size_allocate = gcal_week_header_size_allocate;

  g_object_class_install_property (object_class,
                                   PROP_ACTIVE_DATE,
                                   g_param_spec_boxed ("active-date",
                                                       "Date",
                                                       "The active selected date",
                                                       ICAL_TIME_TYPE,
                                                       G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/week-header.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalWeekHeader, draw_area);
  gtk_widget_class_bind_template_child (widget_class, GcalWeekHeader, expand_button);
  gtk_widget_class_bind_template_child (widget_class, GcalWeekHeader, expand_button_box);
  gtk_widget_class_bind_template_child (widget_class, GcalWeekHeader, expand_button_image);
  gtk_widget_class_bind_template_child (widget_class, GcalWeekHeader, grid);
  gtk_widget_class_bind_template_child (widget_class, GcalWeekHeader, month_label);
  gtk_widget_class_bind_template_child (widget_class, GcalWeekHeader, scrolledwindow);
  gtk_widget_class_bind_template_child (widget_class, GcalWeekHeader, sizegroup);
  gtk_widget_class_bind_template_child (widget_class, GcalWeekHeader, week_label);
  gtk_widget_class_bind_template_child (widget_class, GcalWeekHeader, year_label);

  gtk_widget_class_bind_template_callback (widget_class, gcal_week_header_draw);
  gtk_widget_class_bind_template_callback (widget_class, on_expand_action_activated);

  gtk_widget_class_set_css_name (widget_class, "calendar-view");
}

static void
gcal_week_header_init (GcalWeekHeader *self)
{
  self->expanded = FALSE;

  gtk_widget_init_template (GTK_WIDGET (self));
}

/* Public API */
void
gcal_week_header_set_manager (GcalWeekHeader *self,
                              GcalManager    *manager)
{
  g_return_if_fail (GCAL_IS_WEEK_HEADER (self));

  self->manager = manager;
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
  GDateTime *start_date, *end_date, *week_start, *week_end;
  gint weekday;
  gint width;

  g_return_if_fail (GCAL_IS_WEEK_HEADER (self));

  week_start = get_start_of_week (self->active_date);
  week_end = get_end_of_week (self->active_date);
  start_date = gcal_event_get_date_start (event);
  end_date = gcal_event_get_date_end (event);
  weekday = get_event_start_weekday (self, event);

  /* Calculate the real width of this event */
  width = ceil (g_date_time_difference (end_date, start_date) / G_TIME_SPAN_DAY);

  if (g_date_time_compare (start_date, week_start) < 0)
    width -= ceil (g_date_time_difference (week_start, start_date) / G_TIME_SPAN_DAY);

  if (g_date_time_compare (week_end, end_date) < 0)
    width -= ceil (g_date_time_difference (end_date, week_end) / G_TIME_SPAN_DAY);

  add_event_to_grid (self, event, weekday, weekday + width - 1);

  /* Check if we eventually can merge events */
  check_mergeable_events (self);

  g_clear_pointer (&week_end, g_date_time_unref);
  g_clear_pointer (&week_start, g_date_time_unref);
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
        gtk_widget_destroy (l->data);
    }

  /* Remove from the weekday's GList */
  for (weekday = 0; weekday < 7; weekday++)
    {
      gint event_position;

      l = self->events[weekday];
      event_position = g_list_index (self->events[weekday], removed_event);

      if (event_position == -1)
        continue;

      /* Move remaining events up */
      move_events_at_column (self, UP, weekday, event_position);

      /* Remove from the current weekday */
      l = g_list_remove (l, removed_event);
      self->events[weekday] = l;

    }

  /* Check if we eventually can merge events */
  check_mergeable_events (self);

  g_clear_pointer (&children, g_list_free);
}

void
gcal_week_header_set_current_date (GcalWeekHeader *self,
                                   icaltimetype   *current_date)
{
  g_return_if_fail (GCAL_IS_WEEK_HEADER (self));

  g_clear_pointer (&self->current_date, g_free);
  self->current_date = gcal_dup_icaltime (current_date);

  update_title (self);

  gtk_widget_queue_draw (self->draw_area);
}

GtkSizeGroup*
gcal_week_header_get_sidebar_size_group (GcalWeekHeader *self)
{
  g_return_val_if_fail (GCAL_IS_WEEK_HEADER (self), NULL);

  return self->sizegroup;
}
