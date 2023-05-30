/* gcal-month-view.c
 *
 * Copyright © 2015 Erick Pérez Castellanos
 *             2017-2020 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#define G_LOG_DOMAIN "GcalMonthView"

#include "config.h"
#include "gcal-application.h"
#include "gcal-clock.h"
#include "gcal-context.h"
#include "gcal-debug.h"
#include "gcal-month-cell.h"
#include "gcal-month-popover.h"
#include "gcal-month-view.h"
#include "gcal-timeline-subscriber.h"
#include "gcal-utils.h"
#include "gcal-view-private.h"

#include <glib/gi18n.h>

typedef struct
{
  GtkWidget          *event_widget;
  gboolean            visible;
  guint8              length;
  guint8              height;
  guint8              cell;
} GcalEventBlock;

struct _GcalMonthView
{
  GtkWidget           parent;

  struct {
    GtkWidget        *popover;
    GtkWidget        *relative_to;
  } overflow;

  /* Header widgets */
  GtkWidget          *header;
  GtkWidget          *month_label;
  GtkWidget          *year_label;
  GtkWidget          *weekday_label[7];

  /* Grid widgets */
  GtkWidget          *grid;
  GtkWidget          *month_cell[6][7]; /* unowned */

  GtkEventController *motion_controller;

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

  /*
   * Hash containing cells that who has overflow per list of hidden widgets.
   */
  GHashTable         *overflow_cells;

  /*
   * the cell on which its drawn the first day of the month, in the first row, 0 for the first
   * cell, 1 for the second, and so on, this takes first_weekday into account already.
   */
  gint                days_delay;

  /*
   * The cell whose keyboard focus is on.
   */
  gint                keyboard_cell;

  /*
   * first day of the week according to user locale, being
   * 0 for Sunday, 1 for Monday and so on */
  gint                first_weekday;

  /*
   * The start & end dates of the selection. We use datetimes to allow the user to navigate between
   * months using the keyboard.
   */
  GDateTime          *start_mark_cell;
  GDateTime          *end_mark_cell;

  /* Storage for the accumulated scrolling */
  gdouble             scroll_value;
  guint               update_grid_id;

  /* property */
  GDateTime          *date;
  GcalContext        *context;

  gboolean            needs_reallocation;
  gint                last_width;
  gint                last_height;
};


static void          gcal_view_interface_init                    (GcalViewInterface  *iface);

static void          gtk_buildable_interface_init                (GtkBuildableIface  *iface);

static void          on_event_activated_cb                       (GcalEventWidget    *widget,
                                                                  GcalMonthView      *self);

static void          on_event_widget_visibility_changed_cb       (GtkWidget          *event_widget,
                                                                  GParamSpec         *pspec,
                                                                  GcalMonthView      *self);

static void          on_month_cell_show_overflow_popover_cb      (GcalMonthCell      *cell,
                                                                  GtkWidget          *button,
                                                                  GcalMonthView      *self);

static void          gcal_timeline_subscriber_interface_init     (GcalTimelineSubscriberInterface *iface);


G_DEFINE_TYPE_WITH_CODE (GcalMonthView, gcal_month_view, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GCAL_TYPE_VIEW, gcal_view_interface_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_buildable_interface_init)
                         G_IMPLEMENT_INTERFACE (GCAL_TYPE_TIMELINE_SUBSCRIBER,
                                                gcal_timeline_subscriber_interface_init));

static GtkBuildableIface *parent_buildable_iface;

enum
{
  PROP_0,
  PROP_DATE,
  PROP_CONTEXT,
  N_PROPS
};


/*
 * Auxiliary functions
 */

static inline void
cancel_selection (GcalMonthView *self)
{
  gtk_event_controller_set_propagation_phase (self->motion_controller, GTK_PHASE_NONE);
  g_clear_pointer (&self->start_mark_cell, g_date_time_unref);
  g_clear_pointer (&self->end_mark_cell, g_date_time_unref);
}

static void
activate_event (GcalMonthView   *self,
                GcalEventWidget *event_widget)
{
  cancel_selection (self);
  gcal_month_popover_popdown (GCAL_MONTH_POPOVER (self->overflow.popover));

  gcal_view_event_activated (GCAL_VIEW (self), event_widget);
}

static void
setup_child_widget (GcalMonthView *self,
                    GtkWidget     *widget)
{
  if (!gtk_widget_get_parent (widget))
    gtk_widget_insert_before (widget, GTK_WIDGET (self), self->overflow.popover);

  g_signal_connect_object (widget, "activate", G_CALLBACK (on_event_activated_cb), self, 0);
  g_signal_connect_object (widget, "notify::visible", G_CALLBACK (on_event_widget_visibility_changed_cb), self, 0);
}

static gboolean
emit_create_event (GcalMonthView *self)
{
  GDateTime *end_dt,*start_dt;
  GtkWidget *month_cell;
  gboolean should_clear_end;
  gdouble x, y;
  gint cell;

  GCAL_ENTRY;

  if (!self->start_mark_cell || !self->end_mark_cell)
    GCAL_RETURN (FALSE);

  should_clear_end = FALSE;
  start_dt = self->start_mark_cell;
  end_dt = self->end_mark_cell;

  /* Swap dates if start > end */
  if (g_date_time_compare (start_dt, end_dt) > 0)
    {
      GDateTime *aux;
      aux = start_dt;
      start_dt = end_dt;
      end_dt = aux;
    }

  /* Only setup an end date when days are different */
  if (!g_date_time_equal (start_dt, end_dt))
    {
      GDateTime *tmp_dt;

      tmp_dt = g_date_time_new_local (g_date_time_get_year (end_dt),
                                      g_date_time_get_month (end_dt),
                                      g_date_time_get_day_of_month (end_dt),
                                      0, 0, 0);
      end_dt = g_date_time_add_days (tmp_dt, 1);

      should_clear_end = TRUE;

      g_clear_pointer (&tmp_dt, g_date_time_unref);
    }

  /* Get the corresponding GcalMonthCell */
  cell = g_date_time_get_day_of_month (self->end_mark_cell) + self->days_delay - 1;
  month_cell = self->month_cell[cell / 7][cell % 7];

  x = gtk_widget_get_width (month_cell) / 2.0;
  y = gtk_widget_get_height (month_cell) / 2.0;

  gtk_widget_translate_coordinates (month_cell,
                                    GTK_WIDGET (self),
                                    x, y,
                                    &x, &y);

  gcal_view_create_event (GCAL_VIEW (self), start_dt, end_dt, x, y);

  if (should_clear_end)
    g_clear_pointer (&end_dt, g_date_time_unref);

  GCAL_RETURN (TRUE);
}

static GtkWidget*
get_month_cell_at_position (GcalMonthView *self,
                            gdouble        x,
                            gdouble        y,
                            gint          *cell)
{
  GtkWidget *widget;
  gint i = -1;

  widget = gtk_widget_pick (GTK_WIDGET (self), x, y, GTK_PICK_INSENSITIVE);

  if (!widget)
    goto out;

  widget = gtk_widget_get_ancestor (widget, GCAL_TYPE_MONTH_CELL);

  if (!widget)
    goto out;

  for (i = 0; i < 42; i++)
    {
      guint row, col;

      row = i / 7;
      col = i % 7;

      if (self->month_cell[row][col] == widget)
        break;
    }

  g_assert (i >= 0 && i < 42);

out:
  if (cell)
    *cell = i;

  return widget;
}

static void
calculate_event_cells (GcalMonthView *self,
                       GcalEvent     *event,
                       gint          *out_first_cell,
                       gint          *out_last_cell)
{
  gboolean all_day;

  all_day = gcal_event_get_all_day (event);

  /* Start date */
  if (out_first_cell)
    {
      g_autoptr (GDateTime) start_date = NULL;
      gint first_cell;

      first_cell = 1;
      start_date = gcal_event_get_date_start (event);
      start_date = all_day ? g_date_time_ref (start_date) : g_date_time_to_local (start_date);

      if (g_date_time_get_year (start_date) == g_date_time_get_year (self->date) &&
          g_date_time_get_month (start_date) == g_date_time_get_month (self->date))
        {
          first_cell = g_date_time_get_day_of_month (start_date);
        }

      first_cell += self->days_delay - 1;

      *out_first_cell = first_cell;
    }

  /*
   * The logic for the end date is the same, except that we have to check
   * if the event is all day or not.
   */
  if (out_last_cell)
    {
      g_autoptr (GDateTime) end_date = NULL;
      gint last_cell;

      last_cell = gcal_date_time_get_days_in_month (self->date);
      end_date = gcal_event_get_date_end (event);
      end_date = all_day ? g_date_time_ref (end_date) : g_date_time_to_local (end_date);

      if (g_date_time_get_year (end_date) == g_date_time_get_year (self->date) &&
          g_date_time_get_month (end_date) == g_date_time_get_month (self->date))
        {
          last_cell = g_date_time_get_day_of_month (end_date);

          /* If the event is all day, we have to subtract 1 to find the the real date */
          if (all_day)
            last_cell--;
        }

      last_cell += self->days_delay - 1;

      *out_last_cell = last_cell;
    }
}

static GPtrArray*
calculate_multiday_event_blocks (GcalMonthView *self,
                                 GtkWidget     *event_widget,
                                 gdouble       *vertical_cell_space,
                                 gdouble       *size_left,
                                 gint          *events_at_day,
                                 gint          *allocated_events_at_day)
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
        real_height -= gcal_month_cell_get_overflow_height (GCAL_MONTH_CELL (self->month_cell[i / 7][i % 7]));

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
remove_cell_border_and_padding (GtkWidget *cell,
                                gdouble   *x,
                                gdouble   *y,
                                gdouble   *width)
{
  GtkStyleContext *cell_context;
  GtkBorder cell_padding;
  GtkBorder cell_border;
  gint header_height;

  cell_context = gtk_widget_get_style_context (cell);
  gtk_style_context_get_border (cell_context, &cell_border);
  gtk_style_context_get_padding (cell_context, &cell_padding);

  header_height = gcal_month_cell_get_header_height (GCAL_MONTH_CELL (cell));

  if (x)
    *x += cell_border.left + cell_padding.left;

  if (y)
    *y += cell_border.top + cell_padding.top + header_height;

  if (width)
    {
      *width -= cell_border.left + cell_border.right;
      *width -= cell_padding.left + cell_padding.right;
    }
}

static void
count_events_per_day (GcalMonthView *self,
                      gint          *events_per_day)
{
  GHashTableIter iter;
  gpointer key;
  GList *children;
  GList *l;

  /* Multiday events */
  for (l = self->multi_cell_children; l; l = l->next)
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

      cell = GPOINTER_TO_INT (key) + self->days_delay - 1;

      for (l = children; l; l = l->next)
        {
          if (!gtk_widget_get_visible (l->data))
            continue;

          events_per_day[cell]++;
        }
    }
}

static inline gboolean
month_view_contains_event (GcalRange *month_range,
                           GcalEvent *event)
{
  GcalRangeOverlap overlap;

  overlap = gcal_range_calculate_overlap (month_range, gcal_event_get_range (event), NULL);
  return overlap != GCAL_RANGE_NO_OVERLAP;
}

static void
allocate_multiday_events (GcalMonthView *self,
                          gdouble       *vertical_cell_space,
                          gdouble       *size_left,
                          gint          *events_at_day,
                          gint          *allocated_events_at_day,
                          gint           baseline)
{
  g_autoptr (GcalRange) month_range = NULL;
  GtkAllocation event_allocation;
  GList *l;
  gboolean is_rtl;
  gint header_height;

  is_rtl = gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL;
  month_range = gcal_timeline_subscriber_get_range (GCAL_TIMELINE_SUBSCRIBER (self));
  header_height = gtk_widget_get_height (self->header);

  for (l = self->multi_cell_children; l; l = g_list_next (l))
    {
      g_autoptr (GPtrArray) blocks = NULL;
      GtkWidget *child_widget;
      GcalEvent *event;
      gint block_idx;

      child_widget = (GtkWidget*) l->data;

      if (!gtk_widget_get_visible (child_widget))
        continue;

      event = gcal_event_widget_get_event (l->data);

      if (!month_view_contains_event (month_range, event))
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
          day = cell - self->days_delay + 1;

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
          month_cell = self->month_cell[cell / 7][cell % 7];
          last_month_cell = self->month_cell[last_block_cell / 7][last_block_cell % 7];

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
          dt_start = g_date_time_new (g_date_time_get_timezone (gcal_event_get_date_start (event)),
                                      g_date_time_get_year (self->date),
                                      g_date_time_get_month (self->date),
                                      day,
                                      0, 0, 0);

          /* FIXME: use end date's timezone here */
          dt_end = g_date_time_add_days (dt_start, length);

          gcal_event_widget_set_date_start (GCAL_EVENT_WIDGET (child_widget), dt_start);
          gcal_event_widget_set_date_end (GCAL_EVENT_WIDGET (child_widget), dt_end);

          pos_x = first_cell_allocation.x;
          pos_y = first_cell_allocation.y + header_height;
          width = last_cell_allocation.x + last_cell_allocation.width - first_cell_allocation.x;

          remove_cell_border_and_padding (month_cell, &pos_x, &pos_y, &width);

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
allocate_single_day_events (GcalMonthView *self,
                            gdouble       *vertical_cell_space,
                            gdouble       *size_left,
                            gint          *events_at_day,
                            gint          *allocated_events_at_day,
                            gint           baseline)
{
  g_autoptr (GcalRange) month_range = NULL;
  GHashTableIter iter;
  GtkAllocation event_allocation;
  GtkAllocation cell_allocation;
  gpointer key, value;
  gint header_height;

  month_range = gcal_timeline_subscriber_get_range (GCAL_TIMELINE_SUBSCRIBER (self));
  header_height = gtk_widget_get_height (self->header);

  g_hash_table_iter_init (&iter, self->single_cell_children);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      GtkWidget *month_cell;
      GList *aux;
      GList *l;
      gboolean will_overflow;
      gint cell;
      gint day;

      day = GPOINTER_TO_INT (key);
      cell = day + self->days_delay - 1;
      month_cell = self->month_cell[cell / 7][cell % 7];

      gtk_widget_get_allocation (month_cell, &cell_allocation);

      l = (GList*) value;
      for (aux = l; aux; aux = g_list_next (aux))
        {
          GcalEvent *event;
          GtkWidget *child_widget;
          gdouble real_height;
          gdouble pos_y;
          gdouble pos_x;
          gdouble width;
          gint remaining_events;
          gint minimum_height;

          child_widget = aux->data;
          event = gcal_event_widget_get_event (GCAL_EVENT_WIDGET (child_widget));

          if (!gtk_widget_get_visible (child_widget))
            continue;

          if (!month_view_contains_event (month_range, event))
            continue;

          gtk_widget_measure (GTK_WIDGET (child_widget),
                              GTK_ORIENTATION_VERTICAL,
                              -1,
                              &minimum_height,
                              NULL, NULL, NULL);

          /* Check for overflow */
          remaining_events = events_at_day[cell] - allocated_events_at_day[cell];
          will_overflow = remaining_events * minimum_height >= size_left[cell];
          real_height = size_left[cell];

          if (will_overflow)
            real_height -= gcal_month_cell_get_overflow_height (GCAL_MONTH_CELL (month_cell));

          /* No space left, add to the overflow and continue */
          if (real_height < minimum_height)
            {
              gtk_widget_set_child_visible (child_widget, FALSE);

              l = g_hash_table_lookup (self->overflow_cells, GINT_TO_POINTER (cell));
              l = g_list_append (l, child_widget);

              if (g_list_length (l) == 1)
                g_hash_table_insert (self->overflow_cells, GINT_TO_POINTER (cell), l);
              else
                g_hash_table_replace (self->overflow_cells, GINT_TO_POINTER (cell), g_list_copy (l));

              continue;
            }

          allocated_events_at_day[cell]++;

          gtk_widget_set_child_visible (child_widget, TRUE);

          pos_x = cell_allocation.x;
          pos_y = cell_allocation.y + header_height;
          width = cell_allocation.width;

          remove_cell_border_and_padding (month_cell, &pos_x, &pos_y, &width);

          event_allocation.x = pos_x;
          event_allocation.y = pos_y + vertical_cell_space[cell] - size_left[cell];
          event_allocation.width = width;
          event_allocation.height = minimum_height;

          gtk_widget_set_child_visible (child_widget, TRUE);
          gtk_widget_size_allocate (child_widget, &event_allocation, baseline);

          size_left[cell] -= minimum_height;
        }
    }
}

static void
allocate_overflow_popover (GcalMonthView *self,
                           gint           width,
                           gint           height,
                           gint           baseline)
{
  GtkAllocation popover_allocation;
  GtkAllocation allocation;
  gint popover_min_width;
  gint popover_nat_width;
  gint popover_height;
  gint popover_width;
  gint header_height;

  g_assert (self->overflow.relative_to != NULL);

  header_height = gtk_widget_get_height (self->header);

  gtk_widget_get_allocation (self->overflow.relative_to, &allocation);
  allocation.y += header_height;

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

  popover_width = CLAMP (popover_nat_width, popover_min_width, allocation.width * 1.5);
  popover_height = CLAMP (popover_height, allocation.height * 1.5, height);

  popover_allocation = (GtkAllocation) {
    .x = MAX (0, allocation.x - (popover_width - allocation.width) / 2.0),
    .y = MAX (header_height, allocation.y - (popover_height - allocation.height) / 2.0),
    .width = popover_width,
    .height = popover_height,
  };

  if (popover_allocation.x + popover_allocation.width > width)
    popover_allocation.x -= (popover_allocation.x + popover_allocation.width - width);

  if (popover_allocation.y + popover_allocation.height > height)
    popover_allocation.y -= (popover_allocation.y + popover_allocation.height - height);

  gtk_widget_size_allocate (self->overflow.popover, &popover_allocation, baseline);
}

static void
setup_header_widget (GcalMonthView *self,
                     GtkWidget     *widget)
{
  self->header = widget;
  gtk_widget_set_parent (widget, GTK_WIDGET (self));
}

static void
setup_month_grid (GcalMonthView *self,
                  GtkWidget     *widget)
{
  guint row, col;

  self->grid = widget;
  gtk_widget_set_parent (widget, GTK_WIDGET (self));

  for (row = 0; row < 6; row++)
    {
      for (col = 0; col < 7; col++)
        {
          GtkWidget *cell;

          cell = gcal_month_cell_new ();

          g_signal_connect_object (cell, "show-overflow", G_CALLBACK (on_month_cell_show_overflow_popover_cb), self, 0);

          self->month_cell[row][col] = cell;

          gtk_grid_attach (GTK_GRID (widget), cell, col, row, 1, 1);
        }
    }
}

static GcalWeatherInfo*
get_weather_info_for_cell (GcalMonthView *self,
                           guint          cell)
{
  GcalWeatherService *weather_service;
  GcalMonthCell *first_cell;
  GPtrArray *weather_infos;
  GDateTime *first_dt;
  GDate first;
  guint i;

  if (!self->date)
    return NULL;

  weather_service = gcal_context_get_weather_service (self->context);
  weather_infos = gcal_weather_service_get_weather_infos (weather_service);

  first_cell = GCAL_MONTH_CELL (self->month_cell[0][0]);
  first_dt = gcal_month_cell_get_date (first_cell);

  g_date_set_dmy (&first,
                  g_date_time_get_day_of_month (first_dt),
                  g_date_time_get_month (first_dt),
                  g_date_time_get_year (first_dt));


  for (i = 0; weather_infos && i < weather_infos->len; i++)
    {
      GcalWeatherInfo *info;
      GDate weather_date;
      gint day_difference;

      info = g_ptr_array_index (weather_infos, i);

      gcal_weather_info_get_date (info, &weather_date);
      day_difference = g_date_days_between (&first, &weather_date);

      if (day_difference == cell)
        return info;
    }

  return NULL;
}

static void
update_weather (GcalMonthView *self,
                gboolean       clear_old)
{
  guint row;
  guint col;

  g_return_if_fail (GCAL_IS_MONTH_VIEW (self));

  for (row = 0; row < 6; row++)
    {
      for (col = 0; col < 7; col++)
        {
          GcalMonthCell *cell = GCAL_MONTH_CELL (self->month_cell[row][col]);
          gcal_month_cell_set_weather (cell,  get_weather_info_for_cell (self, row * 7 + col));
        }
    }
}

static gboolean
update_month_cells (GcalMonthView *self)
{
  g_autoptr (GDateTime) dt = NULL;
  gboolean show_last_row;
  guint row, col;

  show_last_row = gcal_date_time_get_days_in_month (self->date) + self->days_delay > 35;
  dt = g_date_time_new_local (g_date_time_get_year (self->date),
                              g_date_time_get_month (self->date),
                              1, 0, 0, 0);

  for (row = 0; row < 6; row++)
    {
      for (col = 0; col < 7; col++)
        {
          g_autoptr (GDateTime) cell_date = NULL;
          GcalMonthCell *cell;
          GDateTime *selection_start;
          GDateTime *selection_end;
          GList *l;
          gboolean different_month;
          gboolean selected;
          guint day;

          cell = GCAL_MONTH_CELL (self->month_cell[row][col]);
          day = row * 7 + col;
          selected = FALSE;
          l = NULL;

          /* Cell date */
          cell_date = g_date_time_add_days (dt, row * 7 + col - self->days_delay);

          gcal_month_cell_set_date (cell, cell_date);

          /* Different month */
          different_month = day < self->days_delay ||
                            day - self->days_delay >= gcal_date_time_get_days_in_month (self->date);

          gcal_month_cell_set_different_month (cell, different_month);

          /* If the last row is empty, hide it */
          gtk_widget_set_visible (GTK_WIDGET (cell), show_last_row || row < 5);

          if (different_month)
            {
              gcal_month_cell_set_selected (cell, FALSE);
              gcal_month_cell_set_overflow (cell, 0);
              continue;
            }

          /* Overflow */
          if (g_hash_table_contains (self->overflow_cells, GINT_TO_POINTER (day)))
            l = g_hash_table_lookup (self->overflow_cells, GINT_TO_POINTER (day));

          gcal_month_cell_set_overflow (cell, l ? g_list_length (l) : 0);

          /* Selection */
          selection_start = self->start_mark_cell;
          selection_end = self->end_mark_cell;

          if (selection_start)
            {
              if (!selection_end)
                selection_end = selection_start;

              /* Swap dates if end is before start */
              if (gcal_date_time_compare_date (selection_start, selection_end) < 0)
                {
                  GDateTime *aux = selection_end;
                  selection_end = selection_start;
                  selection_start = aux;
                }

              selected = gcal_date_time_compare_date (selection_start, gcal_month_cell_get_date (cell)) >= 0 &&
                         gcal_date_time_compare_date (selection_end, gcal_month_cell_get_date (cell)) <= 0;
            }

          gcal_month_cell_set_selected (cell, selected);
        }
    }

  update_weather (self, FALSE);

  self->update_grid_id = 0;

  return G_SOURCE_REMOVE;
}

static void
queue_update_month_cells (GcalMonthView *self)
{
  if (self->update_grid_id > 0)
    return;

  self->update_grid_id = g_idle_add ((GSourceFunc) update_month_cells, self);
}

static void
update_header_labels (GcalMonthView *self)
{
  gchar year_str[10] = { 0, };

  g_snprintf (year_str, 10, "%d", g_date_time_get_year (self->date));

  gtk_label_set_label (GTK_LABEL (self->month_label), gcal_get_month_name (g_date_time_get_month (self->date) - 1));
  gtk_label_set_label (GTK_LABEL (self->year_label), year_str);
}

static inline void
update_weekday_labels (GcalMonthView *self)
{
  gint i;

  for (i = 0; i < 7; i++)
    {
      g_autofree gchar *weekday_name = NULL;

      weekday_name = g_utf8_strup (gcal_get_weekday ((i + self->first_weekday) % 7), -1);

      gtk_label_set_label (GTK_LABEL (self->weekday_label[i]), weekday_name);
    }
}

static inline guint
get_child_cell (GcalMonthView   *self,
                GcalEventWidget *child)
{
  GcalEvent *event;
  GDateTime *dt;
  gint cell;

  event = gcal_event_widget_get_event (child);
  dt = gcal_event_get_date_start (event);

  /* Don't adjust the date when the event is all day */
  if (gcal_event_get_all_day (event))
    {
      cell = g_date_time_get_day_of_month (dt);
    }
  else
    {
      dt = g_date_time_to_local (dt);
      cell = g_date_time_get_day_of_month (dt);

      g_clear_pointer (&dt, g_date_time_unref);
    }

  return cell;
}

static void
add_event_widget (GcalMonthView *self,
                  GtkWidget     *widget)
{
  GcalEvent *event;
  const gchar *uuid;
  GList *l = NULL;

  g_return_if_fail (GCAL_IS_EVENT_WIDGET (widget));
  g_return_if_fail (gtk_widget_get_parent (widget) == NULL);

  event = gcal_event_widget_get_event (GCAL_EVENT_WIDGET (widget));
  uuid = gcal_event_get_uid (event);

  /* inserting in all children hash */
  if (g_hash_table_lookup (self->children, uuid) != NULL)
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
      guint cell_idx;

      cell_idx = get_child_cell (self, GCAL_EVENT_WIDGET (widget));

      l = g_hash_table_lookup (self->single_cell_children, GINT_TO_POINTER (cell_idx));
      l = g_list_insert_sorted (l, widget, (GCompareFunc) gcal_event_widget_compare_by_start_date);

      if (g_list_length (l) != 1)
        g_hash_table_steal (self->single_cell_children, GINT_TO_POINTER (cell_idx));

      g_hash_table_insert (self->single_cell_children, GINT_TO_POINTER (cell_idx), l);
    }

  setup_child_widget (self, widget);
}

static void
remove_event_widget (GcalMonthView *self,
                     GtkWidget     *widget)
{
  GtkWidget *master_widget;
  GcalEvent *event;
  GList *l, *aux;
  const gchar *uuid;

  g_return_if_fail (gtk_widget_get_parent (widget) == GTK_WIDGET (self));

  if (!GCAL_IS_EVENT_WIDGET (widget))
    goto out;

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

out:
  gtk_widget_queue_resize (GTK_WIDGET (self));
}

static void
cleanup_overflow_information (GcalMonthView *self)
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


/*
 * Callbacks
 */

static void
add_new_event_button_cb (GtkWidget *button,
                         gpointer   user_data)
{
  g_autoptr(GDateTime) start_date = NULL;
  GcalMonthView *self;
  gint day;

  self = GCAL_MONTH_VIEW (user_data);

  gcal_month_popover_popdown (GCAL_MONTH_POPOVER (self->overflow.popover));

  day = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (self->overflow.popover), "selected-day"));
  start_date = g_date_time_new_local (g_date_time_get_year (self->date),
                                      g_date_time_get_month (self->date),
                                      day, 0, 0, 0);

  gcal_view_create_event_detailed (GCAL_VIEW (self), start_date, NULL);
}

static void
on_click_gesture_pressed_cb (GtkGestureClick *click_gesture,
                             gint             n_press,
                             gdouble          x,
                             gdouble          y,
                             GcalMonthView   *self)
{
  gint days, clicked_cell;

  GCAL_ENTRY;

  days = self->days_delay + gcal_date_time_get_days_in_month (self->date);

  get_month_cell_at_position (self, x, y, &clicked_cell);

  if (clicked_cell >= self->days_delay && clicked_cell < days)
    {
      gtk_event_controller_set_propagation_phase (self->motion_controller, GTK_PHASE_BUBBLE);

      g_clear_pointer (&self->start_mark_cell, g_date_time_unref);

      self->keyboard_cell = clicked_cell;
      self->start_mark_cell = g_date_time_new_local (g_date_time_get_year (self->date),
                                                     g_date_time_get_month (self->date),
                                                     self->keyboard_cell - self->days_delay + 1,
                                                     0, 0, 0);

      update_month_cells (self);
    }
}

static void
on_click_gesture_released_cb (GtkGestureClick *click_gesture,
                              gint             n_press,
                              gdouble          x,
                              gdouble          y,
                              GcalMonthView   *self)
{
  gint days, current_day;

  GCAL_ENTRY;

  days = self->days_delay + gcal_date_time_get_days_in_month (self->date);

  get_month_cell_at_position (self, x, y, &current_day);

  if (current_day >= self->days_delay && current_day < days)
    {
      g_autoptr (GDateTime) new_active_date = NULL;

      self->keyboard_cell = current_day;
      new_active_date = g_date_time_new_local (g_date_time_get_year (self->date),
                                               g_date_time_get_month (self->date),
                                               current_day - self->days_delay + 1,
                                               0, 0, 0);

      gcal_set_date_time (&self->end_mark_cell, new_active_date);
      gcal_set_date_time (&self->date, new_active_date);

      /* First, make sure to show the popover */
      emit_create_event (self);

      update_month_cells (self);

      /* Then update the active date */
      g_object_notify (G_OBJECT (self), "active-date");
    }
  else
    {
      /* If the button is released over an invalid cell, entirely cancel the selection */
      cancel_selection (self);
      gtk_widget_queue_resize (GTK_WIDGET (self));
    }

  gtk_event_controller_set_propagation_phase (self->motion_controller, GTK_PHASE_NONE);
}

static void
on_event_activated_cb (GcalEventWidget *widget,
                       GcalMonthView   *self)
{
  activate_event (self, widget);
}

static void
on_event_widget_visibility_changed_cb (GtkWidget     *event_widget,
                                       GParamSpec    *pspec,
                                       GcalMonthView *self)
{
  self->needs_reallocation = TRUE;
}

static gboolean
on_key_controller_key_pressed_cb (GtkEventControllerKey *key_controller,
                                  guint                  keyval,
                                  guint                  keycode,
                                  GdkModifierType        state,
                                  GcalMonthView         *self)
{
  g_autoptr (GDateTime) new_date = NULL;
  gboolean create_event;
  gboolean selection;
  gboolean valid_key;
  gboolean is_ltr;
  gint days_in_month;
  gint min, max, diff, month_change, current_day;
  gint row, col;

  GCAL_ENTRY;

  selection = state & GDK_SHIFT_MASK;
  create_event = FALSE;
  valid_key = FALSE;
  diff = 0;
  month_change = 0;
  days_in_month = gcal_date_time_get_days_in_month (self->date);
  current_day = self->keyboard_cell - self->days_delay + 1;
  min = self->days_delay;
  max = self->days_delay + days_in_month - 1;
  is_ltr = gtk_widget_get_direction (GTK_WIDGET (self)) != GTK_TEXT_DIR_RTL;

  /*
   * If it's starting the selection right now, it should mark the current keyboard
   * focused cell as the start, and then update the end mark after updating the
   * focused cell.
   */
  if (selection && self->start_mark_cell == NULL)
    {
      self->start_mark_cell = g_date_time_new_local (g_date_time_get_year (self->date),
                                                     g_date_time_get_month (self->date),
                                                     current_day,
                                                     0, 0, 0);
    }

  switch (keyval)
    {
    case GDK_KEY_Up:
      valid_key = TRUE;
      diff = -7;
      break;

    case GDK_KEY_Down:
      valid_key = TRUE;
      diff = 7;
      break;

    case GDK_KEY_Left:
      valid_key = TRUE;
      diff = is_ltr ? -1 : 1;
      break;

    case GDK_KEY_Right:
      valid_key = TRUE;
      diff = is_ltr ? 1 : -1;
      break;

    case GDK_KEY_Return:
      /*
       * If it's not on the selection mode (i.e. shift is not pressed), we should
       * simulate it by changing the start & end selected cells = keyboard cell.
       */
      if (!selection && !self->start_mark_cell && !self->end_mark_cell)
        {
          g_autoptr (GDateTime) new_mark = NULL;

          new_mark = g_date_time_new_local (g_date_time_get_year (self->date),
                                            g_date_time_get_month (self->date),
                                            current_day,
                                            0, 0, 0);
          self->start_mark_cell = g_object_ref (new_mark);
          self->end_mark_cell = g_object_ref (new_mark);
        }

      create_event = TRUE;
      break;

    case GDK_KEY_Escape:
      cancel_selection (self);
      break;

    default:
      GCAL_RETURN (GDK_EVENT_PROPAGATE);
    }

  g_message ("Keyboard cell: %d, diff: %d (min: %d, max: %d)", self->keyboard_cell, diff, min, max);

  if (self->keyboard_cell + diff <= max && self->keyboard_cell + diff >= min)
    {
      self->keyboard_cell += diff;
    }
  else
    {
      g_autoptr (GDateTime) new_month = NULL;

      month_change = self->keyboard_cell + diff > max ? 1 : -1;
      new_month = g_date_time_add_months (self->date, month_change);

      self->days_delay = (time_day_of_week (1, g_date_time_get_month (new_month) - 1, g_date_time_get_year (new_month)) - self->first_weekday + 7) % 7;

      /*
       * Set keyboard cell value to the sum or difference of days delay of successive
       * month or last day of preceding month and overload value depending on
       * month_change. Overload value is the equal to the deviation of the value
       * of keboard_cell from the min or max value of the current month depending
       * on the overload point.
       */
      if (month_change == 1)
        self->keyboard_cell = self->days_delay + self->keyboard_cell + diff - max - 1;
      else
        self->keyboard_cell = self->days_delay + gcal_date_time_get_days_in_month (new_month) - min + self->keyboard_cell + diff;
    }

  /* Focus the selected month cell */
  row = self->keyboard_cell / 7;
  col = self->keyboard_cell % 7;

  gtk_widget_grab_focus (self->month_cell[row][col]);

  /* Update the active date */
  new_date = g_date_time_add_days (self->date, diff);
  gcal_set_date_time (&self->date, new_date);

  /*
   * We can only emit the :create-event signal ~after~ grabbing the focus, otherwise
   * the popover is instantly hidden.
   */
  if (create_event)
    emit_create_event (self);

  g_object_notify (G_OBJECT (self), "active-date");

  if (selection)
    {
      self->end_mark_cell = g_date_time_new_local (g_date_time_get_year (self->date),
                                                   g_date_time_get_month (self->date),
                                                   current_day,
                                                   0, 0, 0);
    }
  else if (!selection && valid_key)
    {
      /* Cancel selection if SHIFT is not pressed */
      cancel_selection (self);
    }

  GCAL_RETURN (GDK_EVENT_STOP);
}

static void
on_month_cell_show_overflow_popover_cb (GcalMonthCell *cell,
                                        GtkWidget     *button,
                                        GcalMonthView *self)
{
  GcalMonthPopover *popover;

  GCAL_ENTRY;

  popover = GCAL_MONTH_POPOVER (self->overflow.popover);

  self->overflow.relative_to = GTK_WIDGET (cell);

  gcal_month_popover_set_date (popover, gcal_month_cell_get_date (cell));
  gcal_month_popover_popup (popover);

  gcal_view_clear_marks (GCAL_VIEW (self));

  gcal_set_date_time (&self->date, gcal_month_cell_get_date (cell));
  g_object_notify (G_OBJECT (self), "active-date");

  GCAL_EXIT;
}

static void
on_month_popover_event_activated_cb (GcalMonthPopover *month_popover,
                                     GcalEventWidget  *event_widget,
                                     GcalMonthView    *self)
{
  cancel_selection (self);
  gcal_view_event_activated (GCAL_VIEW (self), event_widget);
}

static void
on_motion_controller_motion_cb (GtkEventControllerMotion *motion_controller,
                                gdouble                   x,
                                gdouble                   y,
                                GcalMonthView            *self)
{
  gint days;
  gint new_end_cell;

  GCAL_ENTRY;

  days = self->days_delay + gcal_date_time_get_days_in_month (self->date);

  get_month_cell_at_position (self, x, y, &new_end_cell);

  if (self->start_mark_cell)
    {
      if (new_end_cell < self->days_delay || new_end_cell >= days)
        return;

      /* Let the keyboard focus track the pointer */
      self->keyboard_cell = new_end_cell;

      g_clear_pointer (&self->end_mark_cell, g_date_time_unref);
      self->end_mark_cell = g_date_time_new_local (g_date_time_get_year (self->date),
                                                   g_date_time_get_month (self->date),
                                                   new_end_cell - self->days_delay + 1,
                                                   0, 0, 0);

      update_month_cells (self);
    }

  GCAL_EXIT;
}

static gboolean
on_scroll_controller_scrolled_cb (GtkEventControllerScroll *scroll_controller,
                                  gdouble                   dx,
                                  gdouble                   dy,
                                  GcalMonthView            *self)
{
  g_autoptr (GDateTime) new_date = NULL;
  GdkEvent *current_event;
  gint diff;

  current_event = gtk_event_controller_get_current_event (GTK_EVENT_CONTROLLER (scroll_controller));

  if (!should_change_date_for_scroll (&self->scroll_value, current_event))
    return GDK_EVENT_PROPAGATE;

  diff = self->scroll_value > 0.0 ? 1.0 : -1.0;
  new_date = g_date_time_add_months (self->date, diff);

  self->scroll_value = 0.0;

  gcal_view_set_date (GCAL_VIEW (self), new_date);

  g_object_notify (G_OBJECT (self), "active-date");

  return GDK_EVENT_STOP;
}


static void
on_weather_service_weather_changed_cb (GcalWeatherService *weather_service,
                                       GcalMonthView      *self)
{
  update_weather (self, TRUE);
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
  g_autofree gchar *new_date_string = NULL;
  GcalMonthView *self;
  gboolean month_changed;

  GCAL_ENTRY;

  self = GCAL_MONTH_VIEW (view);

  month_changed = !self->date ||
                  !date ||
                  g_date_time_get_month (self->date) != g_date_time_get_month (date) ||
                  g_date_time_get_year (self->date) != g_date_time_get_year (date);

  gcal_set_date_time (&self->date, date);

  if (!month_changed)
    GCAL_RETURN ();

  self->days_delay = (time_day_of_week (1, g_date_time_get_month (self->date) - 1, g_date_time_get_year (self->date)) - self->first_weekday + 7) % 7;
  self->keyboard_cell = self->days_delay + (g_date_time_get_day_of_month (self->date) - 1);

  new_date_string = g_date_time_format (date, "%x %X %z");
  GCAL_TRACE_MSG ("new date: %s", new_date_string);

  update_header_labels (self);
  update_month_cells (self);

  gcal_timeline_subscriber_range_changed (GCAL_TIMELINE_SUBSCRIBER (view));

  GCAL_EXIT;
}

static void
gcal_month_view_clear_marks (GcalView *view)
{
  cancel_selection (GCAL_MONTH_VIEW (view));
  update_month_cells (GCAL_MONTH_VIEW (view));

  gtk_widget_queue_allocate (GTK_WIDGET (view));
}

static GList*
gcal_month_view_get_children_by_uuid (GcalView              *view,
                                      GcalRecurrenceModType  mod,
                                      const gchar           *uuid)
{
  return filter_children_by_uid_and_modtype (GTK_WIDGET (view), mod, uuid);
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

static void
gcal_month_view_add_child (GtkBuildable *buildable,
                           GtkBuilder   *builder,
                           GObject      *child,
                           const gchar  *type)
{
  GcalMonthView *self = GCAL_MONTH_VIEW (buildable);

  if (type && strcmp (type, "header") == 0)
    setup_header_widget  (self, GTK_WIDGET (child));
  else if (type && strcmp (type, "grid") == 0)
    setup_month_grid  (self, GTK_WIDGET (child));
  else
    parent_buildable_iface->add_child (buildable, builder, child, type);
}

static void
gtk_buildable_interface_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);

  iface->add_child = gcal_month_view_add_child;
}


/*
 * GcalTimelineSubscriber iface
 */

static GcalRange*
gcal_month_view_get_range (GcalTimelineSubscriber *subscriber)
{
  g_autoptr (GDateTime) month_start = NULL;
  g_autoptr (GDateTime) month_end = NULL;
  GcalMonthView *self;

  self = GCAL_MONTH_VIEW (subscriber);
  month_start = g_date_time_new_local (g_date_time_get_year (self->date),
                                       g_date_time_get_month (self->date),
                                       1, 0, 0, 0);
  month_end = g_date_time_add_months (month_start, 1);

  return gcal_range_new (month_start, month_end, GCAL_RANGE_DEFAULT);
}

static void
gcal_month_view_add_event (GcalTimelineSubscriber *subscriber,
                           GcalEvent              *event)
{
  GcalMonthView *self;
  GcalCalendar *calendar;
  GtkWidget *event_widget;

  self = GCAL_MONTH_VIEW (subscriber);
  calendar = gcal_event_get_calendar (event);

  event_widget = gcal_event_widget_new (self->context, event);
  gcal_event_widget_set_read_only (GCAL_EVENT_WIDGET (event_widget), gcal_calendar_is_read_only (calendar));
  if (!gcal_event_get_all_day (event) && !gcal_event_is_multiday (event))
    gcal_event_widget_set_timestamp_policy (GCAL_EVENT_WIDGET (event_widget), GCAL_TIMESTAMP_POLICY_START);

  add_event_widget (self, event_widget);

  self->needs_reallocation = TRUE;
}

static void
gcal_month_view_update_event (GcalTimelineSubscriber *subscriber,
                              GcalEvent              *event)
{
  GcalMonthView *self;
  GtkWidget *new_widget;
  GList *l;

  GCAL_ENTRY;

  self = GCAL_MONTH_VIEW (subscriber);

  l = g_hash_table_lookup (self->children, gcal_event_get_uid (event));

  if (!l)
    {
      g_warning ("%s: Widget with uuid: %s not found in view: %s",
                 G_STRFUNC,
                 gcal_event_get_uid (event),
                 gtk_widget_get_name (GTK_WIDGET (subscriber)));
      return;
    }

  /* Destroy the old event widget (split event widgets will be destroyed too) */
  remove_event_widget (self, l->data);

  /* Create and add the new event widget */
  new_widget = gcal_event_widget_new (self->context, event);
  if (!gcal_event_get_all_day (event) && !gcal_event_is_multiday (event))
    gcal_event_widget_set_timestamp_policy (GCAL_EVENT_WIDGET (new_widget), GCAL_TIMESTAMP_POLICY_START);
  add_event_widget (self, new_widget);

  self->needs_reallocation = TRUE;

  GCAL_EXIT;
}

static void
gcal_month_view_remove_event (GcalTimelineSubscriber *subscriber,
                              GcalEvent              *event)
{
  GcalMonthView *self;
  const gchar *uuid;
  GList *l;

  GCAL_ENTRY;

  self = GCAL_MONTH_VIEW (subscriber);
  uuid = gcal_event_get_uid (event);
  l = g_hash_table_lookup (self->children, uuid);

  if (!l)
    {
      g_warning ("%s: Widget with uuid: %s not found in view: %s",
                 G_STRFUNC,
                 uuid,
                 gtk_widget_get_name (GTK_WIDGET (subscriber)));
      GCAL_RETURN ();
    }

  remove_event_widget (self, l->data);

  self->needs_reallocation = TRUE;

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
 * GtkWidget overrides
 */

static void
gcal_month_view_direction_changed (GtkWidget        *widget,
                                   GtkTextDirection  direction)
{
  GcalMonthView *self = GCAL_MONTH_VIEW (widget);

  self->needs_reallocation = TRUE;
}

static void
gcal_month_view_size_allocate (GtkWidget *widget,
                               gint       width,
                               gint       height,
                               gint       baseline)
{
  GtkAllocation child_allocation;
  GcalMonthView *self;
  gint header_height;
  gint grid_height;
  gint i;

  GCAL_ENTRY;

  self = GCAL_MONTH_VIEW (widget);

  /* Header */
  gtk_widget_measure (GTK_WIDGET (self->header),
                      GTK_ORIENTATION_VERTICAL,
                      -1,
                      &header_height,
                      NULL, NULL, NULL);

  child_allocation.x = 0;
  child_allocation.y = 0;
  child_allocation.width = width;
  child_allocation.height = header_height;

  gtk_widget_size_allocate (self->header, &child_allocation, baseline);

  /* Grid */
  gtk_widget_measure (GTK_WIDGET (self->grid),
                      GTK_ORIENTATION_VERTICAL,
                      -1,
                      &grid_height,
                      NULL, NULL, NULL);

  child_allocation.x = 0;
  child_allocation.y = header_height;
  child_allocation.width = width;
  child_allocation.height = MAX (height - header_height, grid_height);

  gtk_widget_size_allocate (self->grid, &child_allocation, baseline);

  if (self->needs_reallocation || self->last_width != width || self->last_height != height)
    {
      gdouble vertical_cell_space [42];
      gdouble size_left [42];
      gint allocated_events_at_day [42] = { 0, };
      gint events_at_day [42] = { 0, };

      /* Remove every widget' parts, but the master widget */
      cleanup_overflow_information (self);

      /* Event widgets */
      count_events_per_day (self, events_at_day);

      for (i = 0; i < 42; i++)
        {
          gint h = gcal_month_cell_get_content_space (GCAL_MONTH_CELL (self->month_cell[i / 7][i % 7]));

          vertical_cell_space[i] = h;
          size_left[i] = h;
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

  queue_update_month_cells (self);

  if (gtk_widget_should_layout (self->overflow.popover))
    allocate_overflow_popover (self, width, height, baseline);

  self->last_width = width;
  self->last_height = height;
  self->needs_reallocation = FALSE;

  GCAL_EXIT;
}


/*
 * GObject overrides
 */

static void
gcal_month_view_set_property (GObject       *object,
                              guint          property_id,
                              const GValue  *value,
                              GParamSpec    *pspec)
{
  GcalMonthView *self = (GcalMonthView *) object;
  gint i;

  switch (property_id)
    {
    case PROP_DATE:
      gcal_view_set_date (GCAL_VIEW (object), g_value_get_boxed (value));
      break;

    case PROP_CONTEXT:
      g_assert (self->context == NULL);
      self->context = g_value_dup_object (value);

      for (i = 0; i < 42; i++)
        gcal_month_cell_set_context (GCAL_MONTH_CELL (self->month_cell[i / 7][i % 7]), self->context);

      g_signal_connect_object (gcal_context_get_clock (self->context),
                               "day-changed",
                               G_CALLBACK (update_month_cells),
                               self,
                               G_CONNECT_SWAPPED);

      g_signal_connect_object (gcal_context_get_weather_service (self->context),
                               "weather-changed",
                               G_CALLBACK (on_weather_service_weather_changed_cb),
                               self,
                               0);
      update_weather (self, TRUE);
      g_object_notify (object, "context");
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gcal_month_view_get_property (GObject       *object,
                              guint          property_id,
                              GValue        *value,
                              GParamSpec    *pspec)
{
  GcalMonthView *self = GCAL_MONTH_VIEW (object);

  switch (property_id)
    {
    case PROP_DATE:
      g_value_set_boxed (value, self->date);
      break;

    case PROP_CONTEXT:
      g_value_set_object (value, self->context);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gcal_month_view_finalize (GObject *object)
{
  GcalMonthView *self = GCAL_MONTH_VIEW (object);

  gcal_clear_date_time (&self->date);
  g_clear_object (&self->context);
  g_clear_handle_id (&self->update_grid_id, g_source_remove);

  /* Chain up to parent's finalize() method. */
  G_OBJECT_CLASS (gcal_month_view_parent_class)->finalize (object);
}

static void
gcal_month_view_dispose (GObject *object)
{
  GcalMonthView *self = GCAL_MONTH_VIEW (object);
  GtkWidget *child;

  GCAL_ENTRY;

  g_clear_pointer (&self->overflow.popover, gtk_widget_unparent);
  g_clear_pointer (&self->header, gtk_widget_unparent);
  g_clear_pointer (&self->grid, gtk_widget_unparent);

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (self))) != NULL)
    remove_event_widget (self, child);

  g_clear_pointer (&self->children, g_hash_table_destroy);
  g_clear_pointer (&self->single_cell_children, g_hash_table_destroy);
  g_clear_pointer (&self->overflow_cells, g_hash_table_destroy);
  g_clear_pointer (&self->multi_cell_children, g_list_free);

  G_OBJECT_CLASS (gcal_month_view_parent_class)->dispose (object);

  GCAL_EXIT;
}

static void
gcal_month_view_class_init (GcalMonthViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);;
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gcal_month_view_dispose;
  object_class->finalize = gcal_month_view_finalize;
  object_class->set_property = gcal_month_view_set_property;
  object_class->get_property = gcal_month_view_get_property;

  widget_class->direction_changed = gcal_month_view_direction_changed;
  widget_class->size_allocate = gcal_month_view_size_allocate;

  g_object_class_override_property (object_class, PROP_DATE, "active-date");
  g_object_class_override_property (object_class, PROP_CONTEXT, "context");

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/views/gcal-month-view.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalMonthView, month_label);
  gtk_widget_class_bind_template_child (widget_class, GcalMonthView, motion_controller);
  gtk_widget_class_bind_template_child (widget_class, GcalMonthView, year_label);

  gtk_widget_class_bind_template_child_full (widget_class, "label_0", FALSE, G_STRUCT_OFFSET (GcalMonthView, weekday_label[0]));
  gtk_widget_class_bind_template_child_full (widget_class, "label_1", FALSE, G_STRUCT_OFFSET (GcalMonthView, weekday_label[1]));
  gtk_widget_class_bind_template_child_full (widget_class, "label_2", FALSE, G_STRUCT_OFFSET (GcalMonthView, weekday_label[2]));
  gtk_widget_class_bind_template_child_full (widget_class, "label_3", FALSE, G_STRUCT_OFFSET (GcalMonthView, weekday_label[3]));
  gtk_widget_class_bind_template_child_full (widget_class, "label_4", FALSE, G_STRUCT_OFFSET (GcalMonthView, weekday_label[4]));
  gtk_widget_class_bind_template_child_full (widget_class, "label_5", FALSE, G_STRUCT_OFFSET (GcalMonthView, weekday_label[5]));
  gtk_widget_class_bind_template_child_full (widget_class, "label_6", FALSE, G_STRUCT_OFFSET (GcalMonthView, weekday_label[6]));

  gtk_widget_class_bind_template_callback (widget_class, add_new_event_button_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_click_gesture_pressed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_click_gesture_released_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_key_controller_key_pressed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_motion_controller_motion_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_scroll_controller_scrolled_cb);

  gtk_widget_class_set_css_name (widget_class, "calendar-view");

  g_type_ensure (GCAL_TYPE_MONTH_POPOVER);
}

static void
gcal_month_view_init (GcalMonthView *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->last_width = -1;
  self->last_height = -1;
  self->children = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_list_free);
  self->single_cell_children = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, (GDestroyNotify) g_list_free);
  self->overflow_cells = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, (GDestroyNotify) g_list_free);

  /* First weekday */
  self->first_weekday = get_first_weekday ();

  update_weekday_labels (self);

  /* Overflow popover */
  self->overflow.popover = gcal_month_popover_new ();
  gtk_widget_set_parent (self->overflow.popover, GTK_WIDGET (self));

  g_object_bind_property (self,
                          "context",
                          self->overflow.popover,
                          "context",
                          G_BINDING_DEFAULT);

  g_signal_connect_object (self->overflow.popover,
                           "event-activated",
                           G_CALLBACK (on_month_popover_event_activated_cb),
                           self,
                           0);
}
