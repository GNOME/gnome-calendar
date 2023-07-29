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

typedef struct
{
  GtkWidget          *event_widget;
  gboolean            visible;
  guint8              length;
  guint8              height;
  guint8              cell;
} GcalEventBlock;

struct _GcalMonthViewRow
{
  GtkWidget           parent;

  GtkWidget          *day_cells[7];

  GcalRange          *range;

  /*
   * Hash to keep children widgets (all of them, parent widgets and its parts if there's any),
   * uuid as key and a list of all the instances of the event as value. Here, the first widget
   * on the list is the master, and the rest are the parts. Note: the master is a part itself.
   */
  GHashTable         *children;

  /*
   * Hash containing single-cell events, day of the month, on month-view, month of the year on
   * year-view as key anda list of the events that belongs to this cell
   */
  GHashTable         *single_cell_children;

  /*
   * A sorted list containing multiday events. This one contains only parents events, to find out
   * its parts @children will be used.
   */
  GList              *multi_cell_children;

  /* Hash containing cells that who has overflow per list of hidden widgets */
  GHashTable         *overflow_cells;

  gboolean            needs_reallocation;
  gint                last_width;
  gint                last_height;

  GcalContext        *context;
};

G_DEFINE_FINAL_TYPE (GcalMonthViewRow, gcal_month_view_row, GTK_TYPE_WIDGET)

enum
{
  PROP_0,
  PROP_CONTEXT,
  N_PROPS,
};

static GParamSpec *properties [N_PROPS];



static void
setup_child_widget (GcalMonthViewRow *self,
                    GtkWidget        *widget)
{
  gtk_widget_insert_after (widget, GTK_WIDGET (self), self->day_cells[6]);

  // g_signal_connect_object (widget, "activate", G_CALLBACK (on_event_activated_cb), self, 0);
  // g_signal_connect_object (widget, "notify::visible", G_CALLBACK (on_event_widget_visibility_changed_cb), self, 0);
}

static inline guint
get_child_weekday (GcalMonthViewRow *self,
                   GcalEventWidget  *child)
{
  GcalEvent *event;
  GDateTime *dt;
  gint weekday;

  event = gcal_event_widget_get_event (child);
  dt = gcal_event_get_date_start (event);

  /* Don't adjust the date when the event is all day */
  if (gcal_event_get_all_day (event))
    {
      weekday = g_date_time_get_day_of_week (dt) % 7;
    }
  else
    {
      dt = g_date_time_to_local (dt);
      weekday = g_date_time_get_day_of_week (dt) % 7;

      g_clear_pointer (&dt, g_date_time_unref);
    }

  return weekday;
}

static void
add_event_widget (GcalMonthViewRow *self,
                  GtkWidget        *widget)
{
  GcalEvent *event;
  const gchar *uuid;
  GList *l = NULL;

  g_return_if_fail (GCAL_IS_EVENT_WIDGET (widget));
  g_return_if_fail (gtk_widget_get_parent (widget) == NULL);

  event = gcal_event_widget_get_event (GCAL_EVENT_WIDGET (widget));
  uuid = gcal_event_get_uid (event);

  /* inserting in all children hash */
  if (g_hash_table_contains (self->children, uuid))
    {
      g_warning ("Event with uuid: %s already added", uuid);
      g_object_unref (widget);
      return;
    }

  l = g_list_append (l, widget);
  g_hash_table_insert (self->children, g_strdup (uuid), l);

  if (gcal_event_is_multiday (event))
    {
      self->multi_cell_children = g_list_insert_sorted (self->multi_cell_children,
                                                        widget,
                                                        (GCompareFunc) gcal_event_widget_sort_events);
    }
  else
    {
      guint weekday;

      weekday = get_child_weekday (self, GCAL_EVENT_WIDGET (widget));

      l = g_hash_table_lookup (self->single_cell_children, GINT_TO_POINTER (weekday));
      l = g_list_insert_sorted (l, widget, (GCompareFunc) gcal_event_widget_compare_by_start_date);

      if (g_list_length (l) != 1)
        g_hash_table_steal (self->single_cell_children, GINT_TO_POINTER (weekday));

      g_hash_table_insert (self->single_cell_children, GINT_TO_POINTER (weekday), l);
    }

  setup_child_widget (self, widget);
}

static void
remove_event_widget (GcalMonthViewRow *self,
                     GtkWidget        *widget)
{
  GtkWidget *master_widget;
  GcalEvent *event;
  GList *l, *aux;
  const gchar *uuid;

  g_assert (gtk_widget_get_parent (widget) == GTK_WIDGET (self));

  if (!GCAL_IS_EVENT_WIDGET (widget))
    return;

  event = gcal_event_widget_get_event (GCAL_EVENT_WIDGET (widget));
  uuid = gcal_event_get_uid (event);

  l = g_hash_table_lookup (self->children, uuid);

  if (l)
    {
      gtk_widget_unparent (widget);

      master_widget = (GtkWidget*) l->data;
      if (widget == master_widget)
        {
          if (g_list_find (self->multi_cell_children, widget) != NULL)
            {
              self->multi_cell_children = g_list_remove (self->multi_cell_children, widget);

              aux = g_list_next (l);
              if (aux != NULL)
                {
                  l->next = NULL;
                  aux->prev = NULL;
                  g_list_foreach (aux, (GFunc) gtk_widget_unparent, NULL);
                  g_list_free (aux);
                }
            }
          else
            {
              GHashTableIter iter;
              gpointer key, value;

              /*
               * When an event is changed, we can't rely on it's old date
               * to remove the corresponding widget. Because of that, we have
               * to iter through all the widgets to see which one matches
               */
              g_hash_table_iter_init (&iter, self->single_cell_children);

              while (g_hash_table_iter_next (&iter, &key, &value))
                {
                  gboolean should_break;

                  should_break = FALSE;

                  for (aux = value; aux != NULL; aux = aux->next)
                    {
                      if (aux->data == widget)
                        {
                          aux = g_list_remove (g_list_copy (value), widget);

                          /*
                           * If we removed the event and there's no event left for
                           * the day, remove the key from the table. If there are
                           * events for that day, replace the list.
                           */
                          if (!aux)
                            g_hash_table_remove (self->single_cell_children, key);
                          else
                            g_hash_table_replace (self->single_cell_children, key, aux);

                          should_break = TRUE;

                          break;
                        }
                    }

                  if (should_break)
                    break;
                }
            }
        }

      l = g_list_remove (g_list_copy (l), widget);

      if (!l)
        g_hash_table_remove (self->children, uuid);
      else
        g_hash_table_replace (self->children, g_strdup (uuid), l);
    }

  gtk_widget_queue_resize (GTK_WIDGET (self));
}

static void
calculate_event_cells (GcalMonthViewRow *self,
                       GcalEvent        *event,
                       gint             *out_first_cell,
                       gint             *out_last_cell)
{
  gboolean all_day;

  all_day = gcal_event_get_all_day (event);

  /* Start date */
  if (out_first_cell)
    {
      g_autoptr (GDateTime) start_date = NULL;
      gint first_cell = 0;

      start_date = gcal_event_get_date_start (event);
      start_date = all_day ? g_date_time_ref (start_date) : g_date_time_to_local (start_date);

      if (gcal_range_contains_datetime (self->range, start_date))
        first_cell = g_date_time_get_day_of_week (start_date) % 7;

      *out_first_cell = first_cell;
    }

  /*
   * The logic for the end date is the same, except that we have to check
   * if the event is all day or not.
   */
  if (out_last_cell)
    {
      g_autoptr (GDateTime) end_date = NULL;
      gint last_cell = 6;

      end_date = gcal_event_get_date_end (event);
      end_date = all_day ? g_date_time_ref (end_date) : g_date_time_to_local (end_date);

      if (gcal_range_contains_datetime (self->range, end_date))
        {
          last_cell = g_date_time_get_day_of_week (end_date) % 7;

          /* If the event is all day, we have to subtract 1 to find the the real date */
          if (all_day)
            last_cell--;
        }

      *out_last_cell = last_cell;
    }
}

static void
cleanup_overflow_information (GcalMonthViewRow *self)
{
  g_autoptr (GList) widget_lists = NULL;
  GList *l = NULL;

  /* Remove every widget' parts, but the master widget */
  widget_lists = g_hash_table_get_values (self->children);

  for (l = widget_lists; l; l = l->next)
    {
      g_autoptr (GList) widgets = g_list_copy (l->data);
      GList *l2;

      for (l2 = widgets->next; l2; l2 = l2->next)
        remove_event_widget (self, l2->data);
    }

  /* Clean overflow information */
  g_hash_table_remove_all (self->overflow_cells);
}

static void
count_events_per_day (GcalMonthViewRow *self,
                      gint              events_per_day[7])
{
  GHashTableIter iter;
  gpointer key;
  GList *children;

  /* Multiday events */
  for (GList *l = self->multi_cell_children; l; l = l->next)
    {
      GcalEvent *event;
      gint first_cell;
      gint last_cell;
      gint i;

      if (!gtk_widget_get_visible (l->data))
        continue;

      event = gcal_event_widget_get_event (l->data);

      calculate_event_cells (self, event, &first_cell, &last_cell);

      for (i = first_cell; i <= last_cell; i++)
        events_per_day[i]++;
    }

  /* Single day events */
  g_hash_table_iter_init (&iter, self->single_cell_children);
  while (g_hash_table_iter_next (&iter, &key, (gpointer*) &children))
    {
      gint cell;

      cell = GPOINTER_TO_INT (key);

      for (GList *l = children; l; l = l->next)
        {
          if (!gtk_widget_get_visible (l->data))
            continue;

          events_per_day[cell]++;
        }
    }
}

static GPtrArray*
calculate_multiday_event_blocks (GcalMonthViewRow *self,
                                 GtkWidget        *event_widget,
                                 gdouble          *vertical_cell_space,
                                 gdouble          *size_left,
                                 gint             *events_at_day,
                                 gint             *allocated_events_at_day)
{
  GcalEventBlock *block;
  GcalEvent *event;
  GPtrArray *blocks;
  gboolean was_visible;
  gdouble old_y;
  gdouble y;
  gint first_cell;
  gint last_cell;
  gint i;

  GCAL_ENTRY;

  old_y  = -1.0;
  was_visible = FALSE;

  /* Get the event cells */
  event = gcal_event_widget_get_event (GCAL_EVENT_WIDGET (event_widget));
  calculate_event_cells (self, event, &first_cell, &last_cell);

  blocks = g_ptr_array_new_full (last_cell - first_cell + 1, g_free);
  block = NULL;

  GCAL_TRACE_MSG ("Positioning '%s' (multiday) from %d to %d", gcal_event_get_summary (event), first_cell, last_cell);

  for (i = first_cell; i <= last_cell; i++)
    {
      gboolean visible_at_range;
      gboolean different_row;
      gboolean will_overflow;
      gdouble real_height;
      gint remaining_events;
      gint minimum_height;

      real_height = size_left[i];

      /* Calculate the minimum event height */
      gtk_widget_measure (GTK_WIDGET (event_widget),
                          GTK_ORIENTATION_VERTICAL,
                          -1,
                          &minimum_height,
                          NULL, NULL, NULL);

      /* Count this event at this cell */
      different_row = i / 7 != (i - 1) / 7;
      remaining_events = events_at_day[i] - allocated_events_at_day[i];
      will_overflow = remaining_events * minimum_height > real_height;

      if (will_overflow)
        real_height -= gcal_month_cell_get_overflow_height (GCAL_MONTH_CELL (self->day_cells[i]));

      visible_at_range = real_height >= minimum_height;

      if (visible_at_range)
        allocated_events_at_day[i]++;

      y = vertical_cell_space[i] - size_left[i];

      /* At the first iteration, make was_visible and visible_at_range equal */
      if (i == first_cell)
        was_visible = visible_at_range;

      if (!block || y != old_y || different_row || was_visible != visible_at_range)
        {
          GCAL_TRACE_MSG ("Breaking event at cell %d", i);

          /* Only create a new event widget after the first one is consumed */
          if (block)
            {
              GcalEvent *event;
              GList *aux;

              GCAL_TRACE_MSG ("Cloning event widget");

              event = gcal_event_widget_get_event (GCAL_EVENT_WIDGET (event_widget));

              event_widget = gcal_event_widget_clone (GCAL_EVENT_WIDGET (event_widget));
              setup_child_widget (self, event_widget);

              aux = g_hash_table_lookup (self->children, gcal_event_get_uid (event));
              aux = g_list_append (aux, event_widget);
            }

          old_y = y;

          /* Add a new block */
          block = g_new (GcalEventBlock, 1);
          block->event_widget = event_widget;
          block->visible = visible_at_range;
          block->height = minimum_height;
          block->length = 1;
          block->cell = i;

          g_ptr_array_add (blocks, block);
        }
      else
        {
          block->length++;
        }

      was_visible = visible_at_range;
    }

  GCAL_RETURN (blocks);
}

static void
allocate_multiday_events (GcalMonthViewRow *self,
                          gdouble          *vertical_cell_space,
                          gdouble          *size_left,
                          gint             *events_at_day,
                          gint             *allocated_events_at_day,
                          gint              baseline)
{
  GtkAllocation event_allocation;
  GList *l;
  gboolean is_rtl;

  is_rtl = gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL;

  for (l = self->multi_cell_children; l; l = g_list_next (l))
    {
      g_autoptr (GPtrArray) blocks = NULL;
      GtkWidget *child_widget;
      GcalEvent *event;
      gint block_idx;

      child_widget = (GtkWidget*) l->data;

      if (!gtk_widget_should_layout (child_widget))
        continue;

      event = gcal_event_widget_get_event (l->data);

      if (gcal_range_calculate_overlap (self->range, gcal_event_get_range (event), NULL) == GCAL_RANGE_NO_OVERLAP)
        continue;

      /*
       * Multiday events can "break" following these rules:
       *
       *  - Break when line changed
       *  - Break when the vertical position in the cells changed
       *  - Break when the previous part's visibility is different than the current
       */
      blocks = calculate_multiday_event_blocks (self,
                                                child_widget,
                                                vertical_cell_space,
                                                size_left,
                                                events_at_day,
                                                allocated_events_at_day);

      for (block_idx = 0; block_idx < blocks->len; block_idx++)
        {
          g_autoptr (GDateTime) range_start = NULL;
          g_autoptr (GDateTime) dt_start = NULL;
          g_autoptr (GDateTime) dt_end = NULL;
          GcalEventBlock *block;
          GtkAllocation first_cell_allocation;
          GtkAllocation last_cell_allocation;
          GtkWidget *last_month_cell;
          GtkWidget *month_cell;
          GList *aux;
          gdouble pos_x;
          gdouble pos_y;
          gdouble width;
          gint last_block_cell;
          gint minimum_height;
          gint length;
          gint cell;
          gint day;
          gint j;

          block = g_ptr_array_index (blocks, block_idx);
          length = block->length;
          cell = block->cell;
          last_block_cell = cell + length - 1;
          day = cell;

          child_widget = block->event_widget;

          /* No space left, add to the overflow and continue */
          if (!block->visible)
            {
              gint idx;

              gtk_widget_set_child_visible (child_widget, FALSE);

              for (idx = cell; idx < cell + length; idx++)
                {
                  aux = g_hash_table_lookup (self->overflow_cells, GINT_TO_POINTER (idx));
                  aux = g_list_append (aux, child_widget);

                  if (g_list_length (aux) == 1)
                    g_hash_table_insert (self->overflow_cells, GINT_TO_POINTER (idx), aux);
                  else
                    g_hash_table_replace (self->overflow_cells, GINT_TO_POINTER (idx), g_list_copy (aux));
                }

              continue;
            }


          /* Retrieve the cell widget. On RTL languages, swap the first and last cells */
          month_cell = self->day_cells[cell];
          last_month_cell = self->day_cells[last_block_cell];

          if (is_rtl)
            {
              GtkWidget *aux = month_cell;
              month_cell = last_month_cell;
              last_month_cell = aux;
            }

          gtk_widget_get_allocation (month_cell, &first_cell_allocation);
          gtk_widget_get_allocation (last_month_cell, &last_cell_allocation);

          gtk_widget_set_child_visible (child_widget, TRUE);

          /*
           * Setup the widget's start date as the first day of the row,
           * and the widget's end date as the last day of the row. We don't
           * have to worry about the dates, since GcalEventWidget performs
           * some checks and only applies the dates when it's valid.
           */
          range_start = gcal_range_get_start (self->range);
          dt_start = g_date_time_new (g_date_time_get_timezone (gcal_event_get_date_start (event)),
                                      g_date_time_get_year (range_start),
                                      g_date_time_get_month (range_start),
                                      g_date_time_get_day_of_month (range_start) + day,
                                      0, 0, 0);

          /* FIXME: use end date's timezone here */
          dt_end = g_date_time_add_days (dt_start, length);

          gcal_event_widget_set_date_start (GCAL_EVENT_WIDGET (child_widget), dt_start);
          gcal_event_widget_set_date_end (GCAL_EVENT_WIDGET (child_widget), dt_end);

          pos_x = first_cell_allocation.x;
          pos_y = first_cell_allocation.y + gcal_month_cell_get_header_height (GCAL_MONTH_CELL (month_cell));
          width = last_cell_allocation.x + last_cell_allocation.width - first_cell_allocation.x;

          /*
           * We can only get the minimum height after making all these calculations,
           * otherwise GTK complains about allocating without calling get_preferred_height.
           */
          gtk_widget_measure (GTK_WIDGET (child_widget),
                              GTK_ORIENTATION_VERTICAL,
                              -1,
                              &minimum_height,
                              NULL, NULL, NULL);

          event_allocation.x = pos_x;
          event_allocation.y = pos_y + vertical_cell_space[cell] - size_left[cell];
          event_allocation.width = width;
          event_allocation.height = minimum_height;

          gtk_widget_size_allocate (child_widget, &event_allocation, baseline);

          /* update size_left */
          for (j = 0; j < length; j++)
            size_left[cell + j] -= minimum_height;
        }
    }
}

static void
allocate_single_day_events (GcalMonthViewRow *self,
                            gdouble          *vertical_cell_space,
                            gdouble          *size_left,
                            gint             *events_at_day,
                            gint             *allocated_events_at_day,
                            gint              baseline)
{
  GHashTableIter iter;
  GtkAllocation event_allocation;
  GtkAllocation cell_allocation;
  gpointer key, value;

  g_hash_table_iter_init (&iter, self->single_cell_children);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      GtkWidget *month_cell;
      gboolean will_overflow;
      gint day;

      day = GPOINTER_TO_INT (key);
      month_cell = self->day_cells[day];

      gtk_widget_get_allocation (month_cell, &cell_allocation);

      for (GList *l = (GList *)value; l; l = l->next)
        {
          GcalEvent *event;
          GtkWidget *child_widget;
          gdouble real_height;
          gdouble pos_y;
          gdouble pos_x;
          gdouble width;
          gint remaining_events;
          gint minimum_height;

          child_widget = l->data;
          event = gcal_event_widget_get_event (GCAL_EVENT_WIDGET (child_widget));

          if (!gtk_widget_should_layout (child_widget))
            continue;

          if (gcal_range_calculate_overlap (self->range, gcal_event_get_range (event), NULL) == GCAL_RANGE_NO_OVERLAP)
            continue;

          gtk_widget_measure (GTK_WIDGET (child_widget),
                              GTK_ORIENTATION_VERTICAL,
                              -1,
                              &minimum_height,
                              NULL, NULL, NULL);

          /* Check for overflow */
          remaining_events = events_at_day[day] - allocated_events_at_day[day];
          will_overflow = remaining_events * minimum_height >= size_left[day];
          real_height = size_left[day];

          if (will_overflow)
            real_height -= gcal_month_cell_get_overflow_height (GCAL_MONTH_CELL (month_cell));

          /* No space left, add to the overflow and continue */
          if (real_height < minimum_height)
            {
              gtk_widget_set_child_visible (child_widget, FALSE);

              l = g_hash_table_lookup (self->overflow_cells, GINT_TO_POINTER (day));
              l = g_list_append (l, child_widget);

              if (g_list_length (l) == 1)
                g_hash_table_insert (self->overflow_cells, GINT_TO_POINTER (day), l);
              else
                g_hash_table_replace (self->overflow_cells, GINT_TO_POINTER (day), g_list_copy (l));

              continue;
            }

          allocated_events_at_day[day]++;

          gtk_widget_set_child_visible (child_widget, TRUE);

          pos_x = cell_allocation.x;
          pos_y = cell_allocation.y + gcal_month_cell_get_header_height (GCAL_MONTH_CELL (month_cell));
          width = cell_allocation.width;

          event_allocation.x = pos_x;
          event_allocation.y = pos_y + vertical_cell_space[day] - size_left[day];
          event_allocation.width = width;
          event_allocation.height = minimum_height;

          gtk_widget_set_child_visible (child_widget, TRUE);
          gtk_widget_size_allocate (child_widget, &event_allocation, baseline);

          size_left[day] -= minimum_height;
        }
    }
}


/*
 * GtkWidget overrides
 */

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

  for (guint i = 0; i < 7; i++)
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


      minimum += child_minimum;
      natural += child_natural;
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
  cell_width = width / 7.0;

  /* Month cells */
  for (guint i = 0; i < 7; i++)
    {
      GtkAllocation allocation;
      GtkWidget *cell;

      allocation.x = round (cell_width * i);
      allocation.y = 0;
      allocation.width = round (cell_width * (i + 1)) - allocation.x;
      allocation.height = height;

      cell = is_ltr ? self->day_cells[i] : self->day_cells[7 - i + 1];
      gtk_widget_size_allocate (cell, &allocation, baseline);
    }

  /* Event widgets */
  if (self->needs_reallocation || self->last_width != width || self->last_height != height)
    {
      gdouble vertical_cell_space [7];
      gdouble size_left [7];
      gint allocated_events_at_day [7] = { 0, };
      gint events_at_day [7] = { 0, };

      /* Remove every widget' parts, but the main widget */
      cleanup_overflow_information (self);

      /* Event widgets */
      count_events_per_day (self, events_at_day);

      for (guint i = 0; i < 7; i++)
        {
          gint content_space  = gcal_month_cell_get_content_space (GCAL_MONTH_CELL (self->day_cells[i]));

          vertical_cell_space[i] = content_space;
          size_left[i] = content_space;
        }

      allocate_multiday_events (self, vertical_cell_space, size_left, events_at_day, allocated_events_at_day, baseline);
      allocate_single_day_events (self, vertical_cell_space, size_left, events_at_day, allocated_events_at_day, baseline);
    }
  else
    {
      GtkWidget *child;

      for (child = gtk_widget_get_first_child (widget);
           child;
           child = gtk_widget_get_next_sibling (child))
        {
          GtkAllocation allocation;

          if (!GCAL_IS_EVENT_WIDGET (child))
            continue;

          gtk_widget_get_allocation (child, &allocation);
          gtk_widget_size_allocate (child, &allocation, baseline);
        }
    }

  self->last_width = width;
  self->last_height = height;
  self->needs_reallocation = FALSE;
}


/*
 * GObject overrides
 */

static void
gcal_month_view_row_dispose (GObject *object)
{
  GcalMonthViewRow *self = (GcalMonthViewRow *)object;
  GtkWidget *child;

  for (guint i = 0; i < 7; i++)
    g_clear_pointer (&self->day_cells[i], gtk_widget_unparent);

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (self))) != NULL)
    remove_event_widget (self, child);

  g_clear_pointer (&self->children, g_hash_table_destroy);
  g_clear_pointer (&self->single_cell_children, g_hash_table_destroy);
  g_clear_pointer (&self->overflow_cells, g_hash_table_destroy);
  g_clear_pointer (&self->multi_cell_children, g_list_free);

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
gcal_month_view_row_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GcalMonthViewRow *self = GCAL_MONTH_VIEW_ROW (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_value_set_object (value, self->context);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_month_view_row_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GcalMonthViewRow *self = GCAL_MONTH_VIEW_ROW (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      gcal_month_view_row_set_context (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_month_view_row_class_init (GcalMonthViewRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gcal_month_view_row_dispose;
  object_class->finalize = gcal_month_view_row_finalize;
  object_class->get_property = gcal_month_view_row_get_property;
  object_class->set_property = gcal_month_view_row_set_property;

  widget_class->measure = gcal_month_view_row_measure;
  widget_class->size_allocate = gcal_month_view_row_size_allocate;

  properties[PROP_CONTEXT] = g_param_spec_object ("context",
                                                  "Context",
                                                  "The GcalContext of the application",
                                                  GCAL_TYPE_CONTEXT,
                                                  G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_css_name (widget_class, "monthviewrow");
}

static void
gcal_month_view_row_init (GcalMonthViewRow *self)
{
  self->last_width = -1;
  self->last_height = -1;
  self->children = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_list_free);
  self->single_cell_children = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, (GDestroyNotify) g_list_free);
  self->overflow_cells = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, (GDestroyNotify) g_list_free);

  for (guint i = 0; i < 7; i++)
    {
      self->day_cells[i] = gcal_month_cell_new ();
      gcal_month_cell_set_overflow (GCAL_MONTH_CELL (self->day_cells[i]), 0);
      gtk_widget_set_parent (self->day_cells[i], GTK_WIDGET (self));
    }
}

GtkWidget *
gcal_month_view_row_new (void)
{
  return g_object_new (GCAL_TYPE_MONTH_VIEW_ROW, NULL);
}

GcalContext*
gcal_month_view_row_get_context (GcalMonthViewRow *self)
{
  g_assert (GCAL_IS_MONTH_VIEW_ROW (self));

  return self->context;
}

void
gcal_month_view_row_set_context (GcalMonthViewRow *self,
                                 GcalContext      *context)
{
  g_assert (GCAL_IS_MONTH_VIEW_ROW (self));

  if (g_set_object (&self->context, context))
    {
      for (guint i = 0; i < 7; i++)
        gcal_month_cell_set_context (GCAL_MONTH_CELL (self->day_cells[i]), context);

      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CONTEXT]);
    }
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
  GtkWidget *child;

  g_return_if_fail (GCAL_IS_MONTH_VIEW_ROW (self));
  g_return_if_fail (range != NULL);

  GCAL_ENTRY;

  if (self->range && gcal_range_compare (self->range, range) == 0)
    GCAL_RETURN ();

  g_clear_pointer (&self->range, gcal_range_unref);
  self->range = gcal_range_ref (range);

  /* Preemptively remove all event widgets */
  child = gtk_widget_get_first_child (GTK_WIDGET (self));
  while (child)
    {
      GtkWidget *next = gtk_widget_get_next_sibling (child);

      if (GCAL_IS_EVENT_WIDGET (child))
        remove_event_widget (self, child);

      child = next;
    }

  start = gcal_range_get_start (range);
  for (guint i = 0; i < 7; i++)
    {
      g_autoptr (GDateTime) day = g_date_time_add_days (start, i);
      gcal_month_cell_set_date (GCAL_MONTH_CELL (self->day_cells[i]), day);
    }
}

void
gcal_month_view_row_add_event (GcalMonthViewRow *self,
                               GcalEvent        *event)
{
  GcalCalendar *calendar;
  GtkWidget *event_widget;

  g_assert (GCAL_IS_MONTH_VIEW_ROW (self));
  g_assert (GCAL_IS_EVENT (event));

  calendar = gcal_event_get_calendar (event);

  event_widget = gcal_event_widget_new (self->context, event);
  gcal_event_widget_set_read_only (GCAL_EVENT_WIDGET (event_widget), gcal_calendar_is_read_only (calendar));
  if (!gcal_event_get_all_day (event) && !gcal_event_is_multiday (event))
    gcal_event_widget_set_timestamp_policy (GCAL_EVENT_WIDGET (event_widget), GCAL_TIMESTAMP_POLICY_START);

  add_event_widget (self, event_widget);

  self->needs_reallocation = TRUE;
}

void
gcal_month_view_row_remove_event (GcalMonthViewRow *self,
                                  GcalEvent        *event)
{
  const gchar *uid;
  GList *l;

  g_assert (GCAL_IS_MONTH_VIEW_ROW (self));
  g_assert (GCAL_IS_EVENT (event));

  GCAL_ENTRY;

  uid = gcal_event_get_uid (event);
  l = g_hash_table_lookup (self->children, uid);

  if (!l)
    {
      g_warning ("%s: Widget with uuid: %s not found in view: %s",
                 G_STRFUNC,uid, G_OBJECT_TYPE_NAME (self));
      GCAL_RETURN ();
    }

  remove_event_widget (self, l->data);

  self->needs_reallocation = TRUE;

  GCAL_EXIT;
}
