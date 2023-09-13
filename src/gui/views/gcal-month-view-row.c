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

  GtkWidget          *day_cells[7];

  GcalRange          *range;

  GListStore         *events;
  GHashTable         *layout_blocks;

  GcalContext        *context;
};

static gint          compare_events_cb                           (gconstpointer      a,
                                                                  gconstpointer      b,
                                                                  gpointer           user_data);

static void          on_event_widget_activated_cb                (GcalEventWidget    *widget,
                                                                  GcalMonthViewRow   *self);

G_DEFINE_FINAL_TYPE (GcalMonthViewRow, gcal_month_view_row, GTK_TYPE_WIDGET)

enum
{
  PROP_0,
  PROP_CONTEXT,
  N_PROPS,
};

enum
{
  EVENT_ACTIVATED,
  SHOW_OVERFLOW,
  N_SIGNALS,
};

static guint signals[N_SIGNALS] = { 0, };
static GParamSpec *properties [N_PROPS];


/*
 * Auxiliary methods
 */

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
  gtk_widget_insert_after (widget, GTK_WIDGET (self), self->day_cells[6]);

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
      gint last_cell = 6;

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

      *out_last_cell = CLAMP (last_cell, first_cell, 6);
    }
}

static void
prepare_layout_blocks (GcalMonthViewRow *self,
                       guint             overflows[7])
{
  GPtrArray *blocks_per_day[7];
  gboolean cell_will_overflow[7] = { FALSE, };
  guint available_height_without_overflow[7] = { 0, };
  guint available_height_with_overflow[7] = { 0, };
  guint weekday_heights[7] = { 0, };
  guint n_events;

  n_events = g_list_model_get_n_items (G_LIST_MODEL (self->events));

  for (guint i = 0; i < 7; i++)
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

      event = g_list_model_get_item (G_LIST_MODEL (self->events), i);

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
  for (guint cell = 0; cell < 7; cell++)
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

  for (guint i = 0; i < 7; i++)
    g_clear_pointer (&blocks_per_day[i], g_ptr_array_unref);
}

static void
recalculate_layout_blocks (GcalMonthViewRow *self)
{
  g_autoptr (GDateTime) range_start = NULL;
  guint events_at_weekday[7] = { 0, };
  guint n_events;

  range_start = gcal_range_get_start (self->range);
  n_events = g_list_model_get_n_items (G_LIST_MODEL (self->events));

  g_hash_table_remove_all (self->layout_blocks);

  for (guint i = 0; i < n_events; i++)
    {
      g_autoptr (GPtrArray) blocks = NULL;
      g_autoptr (GcalEvent) event = NULL;
      GcalEventBlock *block = NULL;
      gint first_cell;
      gint last_cell;

      event = g_list_model_get_item (G_LIST_MODEL (self->events), i);

      calculate_event_cells (self, event, &first_cell, &last_cell);
      g_assert (last_cell >= first_cell);

      blocks = g_ptr_array_new_full (last_cell - first_cell + 1, layout_block_free);

      for (gint cell = first_cell; cell <= last_cell; cell++)
        {
          events_at_weekday[cell]++;

          if (!block)
            {
              GtkWidget *event_widget;

              event_widget = gcal_event_widget_new (self->context, event);
              // TODO: gcal_event_widget_set_read_only (GCAL_EVENT_WIDGET (event_widget), gcal_calendar_is_read_only (calendar));
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
}


/*
 * Callbacks
 */

static gint
compare_events_cb (gconstpointer a,
                   gconstpointer b,
                   gpointer      user_data)
{
  GcalEvent *event_a = (GcalEvent *)a;
  GcalEvent *event_b = (GcalEvent *)b;

  if (gcal_event_is_multiday (event_a) != gcal_event_is_multiday (event_b))
    return gcal_event_is_multiday (event_b) - gcal_event_is_multiday (event_a);

  return gcal_event_compare (event_a, event_b);
}

static void
on_event_widget_activated_cb (GcalEventWidget  *widget,
                              GcalMonthViewRow *self)
{
  g_signal_emit (self, signals[EVENT_ACTIVATED], 0, widget);
}

static void
on_month_cell_show_overflow_cb (GcalMonthCell    *cell,
                                GtkWidget        *button,
                                GcalMonthViewRow *self)
{
  g_signal_emit (self, signals[SHOW_OVERFLOW], 0, cell);
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

      cell = is_ltr ? self->day_cells[i] : self->day_cells[7 - i - 1];
      gtk_widget_size_allocate (cell, &allocation, baseline);
    }

  /* Event widgets */
    {
      gdouble cell_y[7];
      guint overflows[7] = { 0, };
      guint n_events;

      prepare_layout_blocks (self, overflows);

      for (guint i = 0; i < 7; i++)
        cell_y[i] = gcal_month_cell_get_header_height (GCAL_MONTH_CELL (self->day_cells[i]));

      n_events = g_list_model_get_n_items (G_LIST_MODEL (self->events));

      for (guint i = 0; i < n_events; i++)
        {
          g_autoptr (GcalEvent) event = NULL;
          GPtrArray *blocks;

          event = g_list_model_get_item (G_LIST_MODEL (self->events), i);

          blocks = g_hash_table_lookup (self->layout_blocks, event);
          g_assert (blocks != NULL);

          for (guint block_index = 0; block_index < blocks->len; block_index++)
            {
              GcalEventBlock *block;
              GtkAllocation allocation;

              block = g_ptr_array_index (blocks, block_index);

              allocation.x = (is_ltr ? block->cell : 7 - block->cell - block->length) * cell_width;
              allocation.y = cell_y[block->cell];
              allocation.width = block->length * cell_width;
              allocation.height = block->height;

              gtk_widget_set_child_visible (block->event_widget, block->visible);
              gtk_widget_size_allocate (block->event_widget, &allocation, baseline);

              for (guint j = 0; j < block->length; j++)
                cell_y[block->cell + j] += block->height;
            }
        }

      for (guint i = 0; i < 7; i++)
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

  for (guint i = 0; i < 7; i++)
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

  signals[EVENT_ACTIVATED] = g_signal_new ("event-activated",
                                           GCAL_TYPE_MONTH_VIEW_ROW,
                                           G_SIGNAL_RUN_FIRST,
                                           0,  NULL, NULL, NULL,
                                           G_TYPE_NONE,
                                           1,
                                           GCAL_TYPE_EVENT_WIDGET);

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
  self->events = g_list_store_new (GCAL_TYPE_EVENT);
  self->layout_blocks = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, (GDestroyNotify) g_ptr_array_unref);

  for (guint i = 0; i < 7; i++)
    {
      self->day_cells[i] = gcal_month_cell_new ();
      gcal_month_cell_set_overflow (GCAL_MONTH_CELL (self->day_cells[i]), 0);
      gtk_widget_set_parent (self->day_cells[i], GTK_WIDGET (self));
      g_signal_connect (self->day_cells[i], "show-overflow", G_CALLBACK (on_month_cell_show_overflow_cb), self);
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

  g_return_if_fail (GCAL_IS_MONTH_VIEW_ROW (self));
  g_return_if_fail (range != NULL);

  GCAL_ENTRY;

  if (self->range && gcal_range_compare (self->range, range) == 0)
    GCAL_RETURN ();

  g_clear_pointer (&self->range, gcal_range_unref);
  self->range = gcal_range_ref (range);

  /* Preemptively remove all event widgets */
  g_list_store_remove_all (self->events);
  g_hash_table_remove_all (self->layout_blocks);

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
  g_assert (GCAL_IS_MONTH_VIEW_ROW (self));
  g_assert (GCAL_IS_EVENT (event));

  g_list_store_insert_sorted (self->events, event, compare_events_cb, self);
  recalculate_layout_blocks (self);
}

void
gcal_month_view_row_remove_event (GcalMonthViewRow *self,
                                  GcalEvent        *event)
{
  guint position;

  g_assert (GCAL_IS_MONTH_VIEW_ROW (self));
  g_assert (GCAL_IS_EVENT (event));

  GCAL_ENTRY;

  if (!g_list_store_find (self->events, event, &position))
    {
      g_warning ("%s: Widget with uuid: %s not found in view: %s",
                 G_STRFUNC,
                 gcal_event_get_uid (event),
                 G_OBJECT_TYPE_NAME (self));
      GCAL_RETURN ();
    }

  g_list_store_remove (self->events, position);
  recalculate_layout_blocks (self);

  GCAL_EXIT;
}

void
gcal_month_view_row_update_style_for_date   (GcalMonthViewRow *self,
                                             GDateTime        *date)
{
  g_autoptr (GDateTime) range_start = NULL;

  g_assert (GCAL_IS_MONTH_VIEW_ROW (self));
  g_assert (date != NULL);
  g_assert (self->range != NULL);

  range_start = gcal_range_get_start (self->range);

  for (guint i = 0; i < 7; i++)
    {
      g_autoptr (GDateTime) cell_date = NULL;
      gboolean different_month;

      cell_date = g_date_time_add_days (range_start, i);
      different_month = g_date_time_get_year (cell_date) != g_date_time_get_year (date) ||
                        g_date_time_get_month (cell_date) != g_date_time_get_month (date);

      gcal_month_cell_set_different_month (GCAL_MONTH_CELL (self->day_cells[i]), different_month);
    }
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

  column = floor (7.0 * x / (gdouble) width);

  if (gtk_widget_get_direction (GTK_WIDGET (self)) != GTK_TEXT_DIR_RTL)
    return self->day_cells[column];
  else
    return self->day_cells[7 - column - 1];
}

void
gcal_month_view_row_update_selection (GcalMonthViewRow *self,
                                      GcalRange        *selection_range)
{
  g_assert (GCAL_IS_MONTH_VIEW_ROW (self));

  for (guint i = 0; i < 7; i++)
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
