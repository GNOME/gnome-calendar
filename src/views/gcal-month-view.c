/* gcal-month-view.c
 *
 * Copyright © 2015 Erick Pérez Castellanos
 *             2017 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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
#include "e-cal-data-model-subscriber.h"
#include "gcal-application.h"
#include "gcal-clock.h"
#include "gcal-context.h"
#include "gcal-debug.h"
#include "gcal-month-cell.h"
#include "gcal-month-popover.h"
#include "gcal-month-view.h"
#include "gcal-utils.h"
#include "gcal-view.h"

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
  GtkContainer        parent;

  GcalMonthPopover   *overflow_popover;

  GdkWindow          *event_window;

  /* Header widgets */
  GtkWidget          *header;
  GtkWidget          *label_0;
  GtkWidget          *label_1;
  GtkWidget          *label_2;
  GtkWidget          *label_3;
  GtkWidget          *label_4;
  GtkWidget          *label_5;
  GtkWidget          *label_6;
  GtkWidget          *month_label;
  GtkWidget          *year_label;
  GtkWidget          *weekday_label[7];

  /* Grid widgets */
  GtkWidget          *grid;
  GtkWidget          *month_cell[6][7]; /* unowned */

  /*
   * Hash to keep children widgets (all of them, parent widgets and its parts if there's any),
   * uuid as key and a list of all the instances of the event as value. Here, the first widget
   * on the list is the master, and the rest are the parts. Note: the master is a part itself.
   */
  GHashTable         *children;

  /*
   * Hash containig single-cell events, day of the month, on month-view, month of the year on
   * year-view as key anda list of the events that belongs to this cell
   */
  GHashTable         *single_cell_children;

  /*
   * A sorted list containig multiday events. This one contains only parents events, to find out
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

  gboolean            pending_event_allocation;
};


static void          gcal_view_interface_init                    (GcalViewInterface  *iface);

static void          gtk_buildable_interface_init                (GtkBuildableIface  *iface);

static void          e_data_model_subscriber_interface_init      (ECalDataModelSubscriberInterface *iface);

static void          on_event_activated_cb                       (GcalEventWidget    *widget,
                                                                  GcalMonthView      *self);

static void          on_event_widget_visibility_changed_cb       (GtkWidget          *event_widget,
                                                                  GParamSpec         *pspec,
                                                                  GcalMonthView      *self);

static void          on_month_cell_show_overflow_popover_cb      (GcalMonthCell      *cell,
                                                                  GtkWidget          *button,
                                                                  GcalMonthView      *self);


G_DEFINE_TYPE_WITH_CODE (GcalMonthView, gcal_month_view, GTK_TYPE_CONTAINER,
                         G_IMPLEMENT_INTERFACE (GCAL_TYPE_VIEW, gcal_view_interface_init)
                         G_IMPLEMENT_INTERFACE (E_TYPE_CAL_DATA_MODEL_SUBSCRIBER,
                                                e_data_model_subscriber_interface_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_buildable_interface_init));

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
  g_clear_pointer (&self->start_mark_cell, g_date_time_unref);
  g_clear_pointer (&self->end_mark_cell, g_date_time_unref);
}

static void
activate_event (GcalMonthView   *self,
                GcalEventWidget *event_widget)
{
  cancel_selection (self);
  gcal_month_popover_popdown (self->overflow_popover);

  g_signal_emit_by_name (self, "event-activated", event_widget);
}

static void
setup_child_widget (GcalMonthView *self,
                    GtkWidget     *widget)
{
  if (!gtk_widget_get_parent (widget))
    gtk_widget_set_parent (widget, GTK_WIDGET (self));

  g_signal_connect_object (widget, "activate", G_CALLBACK (on_event_activated_cb), self, 0);
  g_signal_connect_object (widget, "notify::visible", G_CALLBACK (on_event_widget_visibility_changed_cb), self, 0);
}

static gboolean
emit_create_event (GcalMonthView *self)
{
  GtkAllocation alloc;
  GDateTime *end_dt,*start_dt;
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

  gtk_widget_get_allocation (self->month_cell[cell / 7][cell % 7], &alloc);

  x = alloc.x + alloc.width / 2.0;
  y = alloc.y + alloc.height / 2.0;

  g_signal_emit_by_name (self, "create-event", start_dt, end_dt, x, y);

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
  gint i;

  if (y < gtk_widget_get_allocated_height (self->header))
    return NULL;

  for (i = 0; i < 42; i++)
    {
      GtkAllocation alloc;
      guint row, col;

      row = i / 7;
      col = i % 7;

      gtk_widget_get_allocation (self->month_cell[row][col], &alloc);

      if (x >= alloc.x && x < alloc.x + alloc.width &&
          y >= alloc.y && y < alloc.y + alloc.height)
        {
          if (cell)
            *cell = i;

          return self->month_cell[row][col];
        }
    }

  if (cell)
    *cell = -1;

  return NULL;
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
      GtkStyleContext *context;
      GtkBorder margin;
      gboolean visible_at_range;
      gboolean different_row;
      gboolean will_overflow;
      gdouble real_height;
      gint remaining_events;
      gint minimum_height;

      real_height = size_left[i];

      /* Calculate the minimum event height */
      context = gtk_widget_get_style_context (GTK_WIDGET (event_widget));

      gtk_style_context_get_margin (context, gtk_style_context_get_state (context), &margin);
      gtk_widget_get_preferred_height (GTK_WIDGET (event_widget), &minimum_height, NULL);

      minimum_height += margin.top + margin.bottom;

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
              gtk_widget_show (event_widget);
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
cleanup_overflow_information (GcalMonthView *self)
{
  g_autoptr (GList) widgets = NULL;
  GList *aux = NULL;
  GList *l = NULL;

  /* Remove every widget' parts, but the master widget */
  widgets = g_hash_table_get_values (self->children);

  for (aux = widgets; aux; aux = g_list_next (aux))
    l = g_list_concat (l, g_list_copy (g_list_next (aux->data)));

  g_list_free_full (l, (GDestroyNotify) gtk_widget_destroy);

  /* Clean overflow information */
  g_hash_table_remove_all (self->overflow_cells);
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
  gtk_style_context_get_border (cell_context, gtk_style_context_get_state (cell_context), &cell_border);
  gtk_style_context_get_padding (cell_context, gtk_style_context_get_state (cell_context), &cell_padding);

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

static gdouble
get_real_event_widget_height (GtkWidget *widget)
{
  gint min_height;

  gtk_widget_get_preferred_height (widget, &min_height, NULL);

  min_height += gtk_widget_get_margin_top (widget);
  min_height += gtk_widget_get_margin_bottom (widget);

  return min_height;
}

static void
allocate_multiday_events (GcalMonthView *self,
                          gdouble       *vertical_cell_space,
                          gdouble       *size_left,
                          gint          *events_at_day,
                          gint          *allocated_events_at_day)
{
  GtkAllocation event_allocation;
  GtkBorder margin;
  GList *l;
  gboolean is_rtl;

  is_rtl = gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL;

  for (l = self->multi_cell_children; l; l = g_list_next (l))
    {
      g_autoptr (GPtrArray) blocks = NULL;
      GtkStyleContext *child_context;
      GtkWidget *child_widget;
      GcalEvent *event;
      gint block_idx;

      child_widget = (GtkWidget*) l->data;

      if (!gtk_widget_get_visible (child_widget))
        continue;

      event = gcal_event_widget_get_event (l->data);
      child_context = gtk_widget_get_style_context (l->data);

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
          child_context = gtk_widget_get_style_context (child_widget);

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

          /* Position and allocate the child widget */
          gtk_style_context_get_margin (gtk_widget_get_style_context (child_widget),
                                        gtk_style_context_get_state (child_context),
                                        &margin);


          pos_x = first_cell_allocation.x + margin.left;
          pos_y = first_cell_allocation.y + margin.top;
          width = last_cell_allocation.x + last_cell_allocation.width - first_cell_allocation.x;

          remove_cell_border_and_padding (month_cell, &pos_x, &pos_y, &width);

          /*
           * We can only get the minimum height after making all these calculations,
           * otherwise GTK complains about allocating without calling get_preferred_height.
           */
          minimum_height = get_real_event_widget_height (child_widget);

          event_allocation.x = pos_x;
          event_allocation.y = pos_y + vertical_cell_space[cell] - size_left[cell];
          event_allocation.width = width - (margin.left + margin.right);
          event_allocation.height = minimum_height;

          gtk_widget_size_allocate (child_widget, &event_allocation);

          /* update size_left */
          for (j = 0; j < length; j++)
            {
              size_left[cell + j] -= minimum_height;
              size_left[cell + j] -= margin.top + margin.bottom;
            }
        }
    }
}

static void
allocate_single_day_events (GcalMonthView *self,
                            gdouble       *vertical_cell_space,
                            gdouble       *size_left,
                            gint          *events_at_day,
                            gint          *allocated_events_at_day)
{
  GHashTableIter iter;
  GtkAllocation event_allocation;
  GtkAllocation cell_allocation;
  GtkBorder margin;
  gpointer key, value;

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
          GtkStyleContext *child_context;
          GtkWidget *child_widget;
          gdouble real_height;
          gdouble pos_y;
          gdouble pos_x;
          gdouble width;
          gint remaining_events;
          gint minimum_height;

          child_widget = aux->data;

          if (!gtk_widget_get_visible (child_widget))
            continue;

          child_context = gtk_widget_get_style_context (child_widget);

          gtk_style_context_get_margin (child_context, gtk_style_context_get_state (child_context), &margin);
          minimum_height = get_real_event_widget_height (child_widget) + margin.top + margin.bottom;

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

          pos_x = cell_allocation.x + margin.left;
          pos_y = cell_allocation.y + margin.top;
          width = cell_allocation.width;

          remove_cell_border_and_padding (month_cell, &pos_x, &pos_y, &width);

          event_allocation.x = pos_x;
          event_allocation.y = pos_y + vertical_cell_space[cell] - size_left[cell];
          event_allocation.width = width - (margin.left + margin.right);
          event_allocation.height = minimum_height;

          gtk_widget_set_child_visible (child_widget, TRUE);
          gtk_widget_size_allocate (child_widget, &event_allocation);

          size_left[cell] -= minimum_height + margin.top + margin.bottom;
        }
    }
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
              if (gcal_date_time_compare_date (selection_end, selection_start) < 0)
                {
                  GDateTime *aux = selection_end;
                  selection_end = selection_start;
                  selection_start = aux;
                }

              selected = gcal_date_time_compare_date (gcal_month_cell_get_date (cell), selection_start) >= 0 &&
                         gcal_date_time_compare_date (gcal_month_cell_get_date (cell), selection_end) <= 0;
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
      g_autofree gchar *weekday_name;

      weekday_name = g_utf8_strup (gcal_get_weekday ((i + self->first_weekday) % 7), -1);

      gtk_label_set_label (GTK_LABEL (self->weekday_label[i]), weekday_name);
    }
}


/*
 * Callbacks
 */

static void
add_new_event_button_cb (GtkWidget *button,
                         gpointer   user_data)
{
  GcalMonthView *self;
  GDateTime *start_date;
  gint day;

  self = GCAL_MONTH_VIEW (user_data);

  gcal_month_popover_popdown (self->overflow_popover);

  day = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (self->overflow_popover), "selected-day"));
  start_date = g_date_time_new_local (g_date_time_get_year (self->date),
                                      g_date_time_get_month (self->date),
                                      day, 0, 0, 0);

  g_signal_emit_by_name (GCAL_VIEW (user_data), "create-event-detailed", start_date, NULL);

  g_date_time_unref (start_date);
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
  self->pending_event_allocation = TRUE;
  gtk_widget_queue_resize (GTK_WIDGET (self));
}

static void
on_month_cell_show_overflow_popover_cb (GcalMonthCell *cell,
                                        GtkWidget     *button,
                                        GcalMonthView *self)
{
  GcalMonthPopover *popover;

  popover = GCAL_MONTH_POPOVER (self->overflow_popover);

  cancel_selection (self);

  gcal_month_popover_set_relative_to (popover, GTK_WIDGET (cell));
  gcal_month_popover_set_date (popover, gcal_month_cell_get_date (cell));
  gcal_month_popover_popup (popover);
}

static void
on_month_popover_event_activated_cb (GcalMonthPopover *month_popover,
                                     GcalEventWidget  *event_widget,
                                     GcalMonthView    *self)
{
  activate_event (self, event_widget);
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

  GCAL_ENTRY;

  self = GCAL_MONTH_VIEW (view);

  gcal_set_date_time (&self->date, date);

  self->days_delay = (time_day_of_week (1, g_date_time_get_month (self->date) - 1, g_date_time_get_year (self->date)) - self->first_weekday + 7) % 7;
  self->keyboard_cell = self->days_delay + (g_date_time_get_day_of_month (self->date) - 1);

  new_date_string = g_date_time_format (date, "%x %X %z");
  GCAL_TRACE_MSG ("new date: %s", new_date_string);

  update_header_labels (self);
  update_month_cells (self);

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
  GHashTableIter iter;
  GcalMonthView *self;
  GList *children;
  GList *tmp;

  self = GCAL_MONTH_VIEW (view);
  children = NULL;

  g_hash_table_iter_init (&iter, self->children);

  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &tmp))
    children = g_list_concat (children, g_list_copy (tmp));

  return filter_event_list_by_uid_and_modtype (children, mod, uuid);
}

static void
gcal_month_view_update_subscription (GcalView *view)
{
  g_autoptr (GDateTime) date_start = NULL;
  g_autoptr (GDateTime) date_end = NULL;
  GcalMonthView *self;
  time_t range_start;
  time_t range_end;

  self = GCAL_MONTH_VIEW (view);

  g_assert (self->date != NULL);
  date_start = g_date_time_new_local (g_date_time_get_year (self->date),
                                      g_date_time_get_month (self->date),
                                      1, 0, 0, 0);
  range_start = g_date_time_to_unix (date_start);

  date_end = g_date_time_add_months (date_start, 1);
  range_end = g_date_time_to_unix (date_end);

  gcal_manager_set_subscriber (gcal_context_get_manager (self->context),
                               E_CAL_DATA_MODEL_SUBSCRIBER (self),
                               range_start,
                               range_end);
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
  iface->update_subscription = gcal_month_view_update_subscription;
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
    GTK_BUILDER_WARN_INVALID_CHILD_TYPE (buildable, type);
}

static void
gtk_buildable_interface_init (GtkBuildableIface *iface)
{
  iface->add_child = gcal_month_view_add_child;
}


/*
 * ECalDataModelSubscriber interface
 */

static void
gcal_month_view_component_added (ECalDataModelSubscriber *subscriber,
                                 ECalClient              *client,
                                 ECalComponent           *comp)
{
  g_autoptr (GcalEvent) event = NULL;
  GcalMonthView *self;
  GcalCalendar *calendar;
  GtkWidget *event_widget;
  GError *error;

  GCAL_ENTRY;

  error = NULL;
  self = GCAL_MONTH_VIEW (subscriber);
  calendar = gcal_manager_get_calendar_from_source (gcal_context_get_manager (self->context),
                                                    e_client_get_source (E_CLIENT (client)));
  event = gcal_event_new (calendar, comp, &error);

  if (error)
    {
      g_warning ("Error creating event: %s", error->message);
      g_clear_error (&error);
      GCAL_RETURN ();
    }

  event_widget = gcal_event_widget_new (self->context, event);
  gcal_event_widget_set_read_only (GCAL_EVENT_WIDGET (event_widget), e_client_is_readonly (E_CLIENT (client)));

  gtk_widget_show (event_widget);
  gtk_container_add (GTK_CONTAINER (subscriber), event_widget);

  self->pending_event_allocation = TRUE;

  GCAL_EXIT;
}

static void
gcal_month_view_component_modified (ECalDataModelSubscriber *subscriber,
                                    ECalClient              *client,
                                    ECalComponent           *comp)
{
  g_autoptr (GcalEvent) event = NULL;
  GcalMonthView *self;
  GcalCalendar *calendar;
  GtkWidget *new_widget;
  GError *error;
  GList *l;

  GCAL_ENTRY;

  error = NULL;
  self = GCAL_MONTH_VIEW (subscriber);
  calendar = gcal_manager_get_calendar_from_source (gcal_context_get_manager (self->context),
                                                    e_client_get_source (E_CLIENT (client)));
  event = gcal_event_new (calendar, comp, &error);

  if (error)
    {
      g_warning ("Error creating event: %s", error->message);
      g_clear_error (&error);
      GCAL_RETURN ();
    }

  new_widget = gcal_event_widget_new (self->context, event);

  l = g_hash_table_lookup (self->children, gcal_event_get_uid (event));

  if (l)
    {
      gtk_widget_destroy (l->data);

      gtk_widget_show (new_widget);
      gtk_container_add (GTK_CONTAINER (subscriber), new_widget);
    }
  else
    {
      g_warning ("%s: Widget with uuid: %s not found in view: %s",
                 G_STRFUNC, gcal_event_get_uid (event),
                 gtk_widget_get_name (GTK_WIDGET (subscriber)));
      gtk_widget_destroy (new_widget);
    }

  self->pending_event_allocation = TRUE;

  GCAL_EXIT;
}

static void
gcal_month_view_component_removed (ECalDataModelSubscriber *subscriber,
                                   ECalClient              *client,
                                   const gchar             *uid,
                                   const gchar             *rid)
{
  GcalMonthView *self;
  g_autofree gchar *uuid = NULL;
  const gchar *sid;
  GList *l;

  GCAL_ENTRY;

  self = GCAL_MONTH_VIEW (subscriber);
  sid = e_source_get_uid (e_client_get_source (E_CLIENT (client)));

  if (rid != NULL)
    uuid = g_strdup_printf ("%s:%s:%s", sid, uid, rid);
  else
    uuid = g_strdup_printf ("%s:%s", sid, uid);

  l = g_hash_table_lookup (self->children, uuid);

  if (!l)
    {
      g_warning ("%s: Widget with uuid: %s not found in view: %s",
                 G_STRFUNC,
                 uuid,
                 gtk_widget_get_name (GTK_WIDGET (subscriber)));
      GCAL_RETURN ();
    }

  gtk_widget_destroy (l->data);

  self->pending_event_allocation = TRUE;

  GCAL_EXIT;
}

static void
gcal_month_view_freeze (ECalDataModelSubscriber *subscriber)
{
}

static void
gcal_month_view_thaw (ECalDataModelSubscriber *subscriber)
{
}


static void
e_data_model_subscriber_interface_init (ECalDataModelSubscriberInterface *iface)
{
  iface->component_added = gcal_month_view_component_added;
  iface->component_modified = gcal_month_view_component_modified;
  iface->component_removed = gcal_month_view_component_removed;
  iface->freeze = gcal_month_view_freeze;
  iface->thaw = gcal_month_view_thaw;
}


/*
 * GtkContainer overrides
 */

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
gcal_month_view_add (GtkContainer *container,
                     GtkWidget    *widget)
{
  GcalMonthView *self;
  GcalEvent *event;
  const gchar *uuid;
  GList *l = NULL;

  g_return_if_fail (GCAL_IS_EVENT_WIDGET (widget));
  g_return_if_fail (gtk_widget_get_parent (widget) == NULL);

  self = GCAL_MONTH_VIEW (container);
  event = gcal_event_widget_get_event (GCAL_EVENT_WIDGET (widget));
  uuid = gcal_event_get_uid (event);

  /* inserting in all children hash */
  if (g_hash_table_lookup (self->children, uuid) != NULL)
    {
      g_warning ("Event with uuid: %s already added", uuid);
      gtk_widget_destroy (widget);
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
gcal_month_view_remove (GtkContainer *container,
                        GtkWidget    *widget)
{
  GcalMonthView *self;
  GtkWidget *master_widget;
  GcalEvent *event;
  GList *l, *aux;
  const gchar *uuid;

  g_return_if_fail (gtk_widget_get_parent (widget) == GTK_WIDGET (container));

  if (!GCAL_IS_EVENT_WIDGET (widget))
    goto out;

  self = GCAL_MONTH_VIEW (container);
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
  gtk_widget_queue_resize (GTK_WIDGET (container));
}

static void
gcal_month_view_forall (GtkContainer *container,
                        gboolean      include_internals,
                        GtkCallback   callback,
                        gpointer      callback_data)
{
  GcalMonthView *self;
  GList *l, *l2, *aux = NULL;

  self = GCAL_MONTH_VIEW (container);

  /* Header */
  if (self->header)
    (*callback) (self->header, callback_data);

  /* Grid */
  if (self->grid)
    (*callback) (self->grid, callback_data);

  /* Event widgets */
  l2 = g_hash_table_get_values (self->children);

  for (l = l2; l != NULL; l = g_list_next (l))
    aux = g_list_concat (aux, g_list_reverse (g_list_copy (l->data)));

  g_list_free (l2);

  l = aux;
  while (aux)
    {
      GtkWidget *widget = (GtkWidget*) aux->data;
      aux = aux->next;

      (*callback) (widget, callback_data);
    }

  g_list_free (l);
}


/*
 * GtkWidget overrides
 */

static void
gcal_month_view_realize (GtkWidget *widget)
{
  GcalMonthView *self;
  GdkWindow *parent_window;
  GdkWindowAttr attributes;
  gint attributes_mask;
  GtkAllocation allocation;

  self = GCAL_MONTH_VIEW (widget);
  gtk_widget_set_realized (widget, TRUE);

  parent_window = gtk_widget_get_parent_window (widget);
  gtk_widget_set_window (widget, g_object_ref (parent_window));

  gtk_widget_get_allocation (widget, &allocation);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.x = allocation.x;
  attributes.y = allocation.y;
  attributes.width = allocation.width;
  attributes.height = allocation.height;
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= (GDK_BUTTON_PRESS_MASK |
                            GDK_BUTTON_RELEASE_MASK |
                            GDK_BUTTON1_MOTION_MASK |
                            GDK_POINTER_MOTION_HINT_MASK |
                            GDK_POINTER_MOTION_MASK |
                            GDK_ENTER_NOTIFY_MASK |
                            GDK_LEAVE_NOTIFY_MASK |
                            GDK_SCROLL_MASK |
                            GDK_SMOOTH_SCROLL_MASK);
  attributes_mask = GDK_WA_X | GDK_WA_Y;

  self->event_window = gdk_window_new (parent_window,
                                       &attributes,
                                       attributes_mask);
  gtk_widget_register_window (widget, self->event_window);
}

static void
gcal_month_view_unrealize (GtkWidget *widget)
{
  GcalMonthView *self = GCAL_MONTH_VIEW (widget);

  if (self->event_window)
    {
      gtk_widget_unregister_window (widget, self->event_window);
      gdk_window_destroy (self->event_window);
      self->event_window = NULL;
    }

  GTK_WIDGET_CLASS (gcal_month_view_parent_class)->unrealize (widget);
}

static void
gcal_month_view_map (GtkWidget *widget)
{
  GcalMonthView *self = GCAL_MONTH_VIEW (widget);

  if (self->event_window)
    gdk_window_show (self->event_window);

  GTK_WIDGET_CLASS (gcal_month_view_parent_class)->map (widget);
}

static void
gcal_month_view_unmap (GtkWidget *widget)
{
  GcalMonthView *self = GCAL_MONTH_VIEW (widget);

  if (self->event_window)
    gdk_window_hide (self->event_window);

  GTK_WIDGET_CLASS (gcal_month_view_parent_class)->unmap (widget);
}

static void
gcal_month_view_size_allocate (GtkWidget     *widget,
                               GtkAllocation *allocation)
{
  GtkAllocation child_allocation;
  GtkAllocation old_alloc;
  GcalMonthView *self;
  gint header_height;
  gint grid_height;
  gint i;

  GCAL_ENTRY;

  self = GCAL_MONTH_VIEW (widget);

  /* Allocate the widget */
  gtk_widget_get_allocation (widget, &old_alloc);
  gtk_widget_set_allocation (widget, allocation);

  if (gtk_widget_get_realized (widget))
    gdk_window_move_resize (self->event_window, allocation->x, allocation->y, allocation->width, allocation->height);

  /* Header */
  gtk_widget_get_preferred_height (self->header, &header_height, NULL);

  child_allocation.x = allocation->x;
  child_allocation.y = allocation->y;
  child_allocation.width = allocation->width;
  child_allocation.height = header_height;

  gtk_widget_size_allocate (self->header, &child_allocation);

  /* Grid */
  gtk_widget_get_preferred_height (self->grid, &grid_height, NULL);

  child_allocation.x = allocation->x;
  child_allocation.y = allocation->y + header_height;
  child_allocation.width = allocation->width;
  child_allocation.height = MAX (allocation->height - header_height, grid_height);

  gtk_widget_size_allocate (self->grid, &child_allocation);

  /*
   * At this point, the internal widgets (grid and header) already received the allocation they
   * asked for, and are happy. Now it comes the tricky part: when GTK is allocating sizes (here),
   * we cannot show or hide or add or remove widgets. Doing that can potentially trigger an infinite
   * allocation cycle, and this function will keep running forever nonstop, the CPU will melt, the
   * fans will scream and users will cry in anger.
   *
   * Thus, to avoid the infinite allocation cycle, we *only* update the event widgets when something
   * actually changed - either a new event widget was added (pending_event_allocation) or the size
   * of the Month view changed.
   *
   * The following code can be read as the pseudo-code:
   *
   *   if (something changed)
   *     recalculate and recreate event widgets;
   */
  if (self->pending_event_allocation ||
      allocation->width != old_alloc.width ||
      allocation->height != old_alloc.height)
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

      /* Allocate multidays events before single day events, as they have a higher priority */
      allocate_multiday_events (self, vertical_cell_space, size_left, events_at_day, allocated_events_at_day);
      allocate_single_day_events (self, vertical_cell_space, size_left, events_at_day, allocated_events_at_day);
    }

  queue_update_month_cells (self);

  self->pending_event_allocation = FALSE;

  GCAL_EXIT;
}

static gboolean
gcal_month_view_button_press (GtkWidget      *widget,
                              GdkEventButton *event)
{
  GcalMonthView *self;
  gdouble x, y;
  gint days, clicked_cell;

  GCAL_ENTRY;

  self = GCAL_MONTH_VIEW (widget);
  days = self->days_delay + gcal_date_time_get_days_in_month (self->date);

  /* The event may have come from a child widget, so make it relative to the month view */
  if (!gcal_translate_child_window_position (widget, event->window, event->x, event->y, &x, &y))
    return GDK_EVENT_PROPAGATE;

  get_month_cell_at_position (self, x, y, &clicked_cell);

  if (clicked_cell >= self->days_delay && clicked_cell < days)
    {
      g_clear_pointer (&self->start_mark_cell, g_date_time_unref);

      self->keyboard_cell = clicked_cell;
      self->start_mark_cell = g_date_time_new_local (g_date_time_get_year (self->date),
                                                     g_date_time_get_month (self->date),
                                                     self->keyboard_cell - self->days_delay + 1,
                                                     0, 0, 0);

      update_month_cells (self);
    }

  GCAL_RETURN (GDK_EVENT_PROPAGATE);
}

static gboolean
gcal_month_view_motion_notify_event (GtkWidget      *widget,
                                     GdkEventMotion *event)
{
  GcalMonthView *self;
  gdouble x, y;
  gint days;
  gint new_end_cell;

  GCAL_ENTRY;

  self = GCAL_MONTH_VIEW (widget);
  days = self->days_delay + gcal_date_time_get_days_in_month (self->date);

  if (!gcal_translate_child_window_position (widget, event->window, event->x, event->y, &x, &y))
    GCAL_RETURN (GDK_EVENT_PROPAGATE);

  get_month_cell_at_position (self, x, y, &new_end_cell);

  if (self->start_mark_cell)
    {
      if (!(event->state & GDK_BUTTON_PRESS_MASK))
        GCAL_RETURN (GDK_EVENT_STOP);

      if (new_end_cell < self->days_delay || new_end_cell >= days)
        GCAL_RETURN (GDK_EVENT_PROPAGATE);

      /* Let the keyboard focus track the pointer */
      self->keyboard_cell = new_end_cell;

      g_clear_pointer (&self->end_mark_cell, g_date_time_unref);
      self->end_mark_cell = g_date_time_new_local (g_date_time_get_year (self->date),
                                                   g_date_time_get_month (self->date),
                                                   new_end_cell - self->days_delay + 1,
                                                   0, 0, 0);

      update_month_cells (self);

      GCAL_RETURN (GDK_EVENT_STOP);
    }
  else
    {
      if (gtk_widget_is_visible (GTK_WIDGET (self->overflow_popover)))
        GCAL_RETURN (GDK_EVENT_PROPAGATE);
    }

  GCAL_RETURN (GDK_EVENT_PROPAGATE);
}

static gboolean
gcal_month_view_button_release (GtkWidget      *widget,
                                GdkEventButton *event)
{
  GcalMonthView *self;
  gdouble x, y;
  gint days, current_day;

  GCAL_ENTRY;

  self = GCAL_MONTH_VIEW (widget);
  days = self->days_delay + gcal_date_time_get_days_in_month (self->date);

  if (!gcal_translate_child_window_position (widget, event->window, event->x, event->y, &x, &y))
    GCAL_RETURN (GDK_EVENT_PROPAGATE);

  get_month_cell_at_position (self, x, y, &current_day);

  if (current_day >= self->days_delay && current_day < days)
    {
      g_autoptr (GDateTime) new_active_date = NULL;
      gboolean valid;

      self->keyboard_cell = current_day;
      new_active_date = g_date_time_new_local (g_date_time_get_year (self->date),
                                               g_date_time_get_month (self->date),
                                               current_day - self->days_delay + 1,
                                               0, 0, 0);

      gcal_set_date_time (&self->end_mark_cell, new_active_date);
      gcal_set_date_time (&self->date, new_active_date);

      /* First, make sure to show the popover */
      valid = emit_create_event (self);

      update_month_cells (self);

      /* Then update the active date */
      g_object_notify (G_OBJECT (self), "active-date");

      GCAL_RETURN (valid);
    }
  else
    {
      /* If the button is released over an invalid cell, entirely cancel the selection */
      cancel_selection (GCAL_MONTH_VIEW (widget));

      gtk_widget_queue_resize (widget);

      GCAL_RETURN (GDK_EVENT_PROPAGATE);
    }
}

static void
gcal_month_view_direction_changed (GtkWidget        *widget,
                                   GtkTextDirection  previous_direction)
{
  GcalMonthView *self = GCAL_MONTH_VIEW (widget);

  self->pending_event_allocation = TRUE;

  gtk_widget_queue_resize (widget);
}

static gboolean
gcal_month_view_key_press (GtkWidget   *widget,
                           GdkEventKey *event)
{
  GcalMonthView *self;
  gboolean create_event;
  gboolean selection;
  gboolean valid_key;
  gboolean is_ltr;
  gint days_in_month;
  gint min, max, diff, month_change, current_day;
  gint row, col;

  g_return_val_if_fail (GCAL_IS_MONTH_VIEW (widget), FALSE);

  self = GCAL_MONTH_VIEW (widget);
  selection = event->state & GDK_SHIFT_MASK;
  create_event = FALSE;
  valid_key = FALSE;
  diff = 0;
  month_change = 0;
  days_in_month = gcal_date_time_get_days_in_month (self->date);
  current_day = self->keyboard_cell - self->days_delay + 1;
  min = self->days_delay;
  max = self->days_delay + days_in_month - 1;
  is_ltr = gtk_widget_get_direction (widget) != GTK_TEXT_DIR_RTL;

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

  switch (event->keyval)
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
      cancel_selection (GCAL_MONTH_VIEW (widget));
      break;

    default:
      return GDK_EVENT_PROPAGATE;
    }

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
       * month or last day of preceeding month and overload value depending on
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

  current_day = self->keyboard_cell - self->days_delay + 1;
  //self->date->day = current_day;

  /*
   * We can only emit the :create-event signal ~after~ grabbing the focus, otherwise
   * the popover is instantly hidden.
   */
  if (create_event)
    emit_create_event (self);

  g_object_notify (G_OBJECT (widget), "active-date");

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
      cancel_selection (GCAL_MONTH_VIEW (widget));
    }

  return GDK_EVENT_STOP;
}

static gboolean
gcal_month_view_scroll_event (GtkWidget      *widget,
                              GdkEventScroll *scroll_event)
{
  GcalMonthView *self = GCAL_MONTH_VIEW (widget);

  /*
   * If we accumulated enough scrolling, change the month. Otherwise, we'd scroll
   * waaay too fast.
   */
  if (should_change_date_for_scroll (&self->scroll_value, scroll_event))
    {
      g_autoptr (GDateTime) new_date = NULL;
      gint diff;

      diff = self->scroll_value > 0 ? 1 : -1;
      new_date = g_date_time_add_months (self->date, diff);

      gcal_clear_date_time (&self->date);
      self->date = g_steal_pointer (&new_date);
      self->scroll_value = 0;

      gtk_widget_queue_draw (widget);

      g_object_notify (G_OBJECT (widget), "active-date");
    }

  return GDK_EVENT_STOP;
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
  g_clear_pointer (&self->children, g_hash_table_destroy);
  g_clear_pointer (&self->single_cell_children, g_hash_table_destroy);
  g_clear_pointer (&self->overflow_cells, g_hash_table_destroy);
  g_clear_pointer (&self->multi_cell_children, g_list_free);

  g_clear_object (&self->context);

  if (self->update_grid_id > 0)
    {
      g_source_remove (self->update_grid_id);
      self->update_grid_id = 0;
    }

  /* Chain up to parent's finalize() method. */
  G_OBJECT_CLASS (gcal_month_view_parent_class)->finalize (object);
}

static void
gcal_month_view_class_init (GcalMonthViewClass *klass)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->set_property = gcal_month_view_set_property;
  object_class->get_property = gcal_month_view_get_property;
  object_class->finalize = gcal_month_view_finalize;

  widget_class = GTK_WIDGET_CLASS (klass);
  widget_class->realize = gcal_month_view_realize;
  widget_class->unrealize = gcal_month_view_unrealize;
  widget_class->map = gcal_month_view_map;
  widget_class->unmap = gcal_month_view_unmap;
  widget_class->size_allocate = gcal_month_view_size_allocate;
  widget_class->button_press_event = gcal_month_view_button_press;
  widget_class->motion_notify_event = gcal_month_view_motion_notify_event;
  widget_class->button_release_event = gcal_month_view_button_release;
  widget_class->direction_changed = gcal_month_view_direction_changed;
  widget_class->key_press_event = gcal_month_view_key_press;
  widget_class->scroll_event = gcal_month_view_scroll_event;

  container_class = GTK_CONTAINER_CLASS (klass);
  container_class->add = gcal_month_view_add;
  container_class->remove = gcal_month_view_remove;
  container_class->forall = gcal_month_view_forall;

  g_object_class_override_property (object_class, PROP_DATE, "active-date");
  g_object_class_override_property (object_class, PROP_CONTEXT, "context");

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/month-view.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalMonthView, label_0);
  gtk_widget_class_bind_template_child (widget_class, GcalMonthView, label_1);
  gtk_widget_class_bind_template_child (widget_class, GcalMonthView, label_2);
  gtk_widget_class_bind_template_child (widget_class, GcalMonthView, label_3);
  gtk_widget_class_bind_template_child (widget_class, GcalMonthView, label_4);
  gtk_widget_class_bind_template_child (widget_class, GcalMonthView, label_5);
  gtk_widget_class_bind_template_child (widget_class, GcalMonthView, label_6);
  gtk_widget_class_bind_template_child (widget_class, GcalMonthView, month_label);
  gtk_widget_class_bind_template_child (widget_class, GcalMonthView, year_label);

  gtk_widget_class_bind_template_callback (widget_class, add_new_event_button_cb);

  gtk_widget_class_set_css_name (widget_class, "calendar-view");

  g_type_ensure (GCAL_TYPE_MONTH_POPOVER);
}

static void
gcal_month_view_init (GcalMonthView *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);

  self->children = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_list_free);
  self->single_cell_children = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, (GDestroyNotify) g_list_free);
  self->overflow_cells = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, (GDestroyNotify) g_list_free);
  self->pending_event_allocation = FALSE;

  /* First weekday */
  self->first_weekday = get_first_weekday ();

  /* Weekday header labels */
  self->weekday_label[0] = self->label_0;
  self->weekday_label[1] = self->label_1;
  self->weekday_label[2] = self->label_2;
  self->weekday_label[3] = self->label_3;
  self->weekday_label[4] = self->label_4;
  self->weekday_label[5] = self->label_5;
  self->weekday_label[6] = self->label_6;

  update_weekday_labels (self);

  /* Overflow popover */
  self->overflow_popover = (GcalMonthPopover*) gcal_month_popover_new ();

  g_object_bind_property (self,
                          "context",
                          self->overflow_popover,
                          "context",
                          G_BINDING_DEFAULT);

  g_signal_connect_object (self->overflow_popover, "event-activated", G_CALLBACK (on_month_popover_event_activated_cb), self, 0);
}

