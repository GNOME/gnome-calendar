/* -*- mode: c; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gcal-month-view.c
 *
 * Copyright (C) 2015 - Erick Pérez Castellanos
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define G_LOG_DOMAIN "GcalMonthView"

#include "e-cal-data-model-subscriber.h"

#include "gcal-debug.h"
#include "gcal-month-cell.h"
#include "gcal-month-view.h"
#include "gcal-utils.h"
#include "gcal-view.h"

#include <glib/gi18n.h>

#include <math.h>

#define LINE_WIDTH      0.5

struct _GcalMonthView
{
  GtkContainer        parent;

  GtkWidget          *events_box;
  GtkWidget          *overflow_popover;
  GtkWidget          *popover_title;

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
  GtkWidget          *month_cell[6][7];

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
   * Set containing the master widgets hidden for delete;
   */
  GHashTable         *hidden_as_overflow;

  /* state flags */
  gboolean            children_changed;

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

  /*
   * clock format from GNOME desktop settings
   */
  gboolean            use_24h_format;

  /* text direction factors */
  gboolean            k;

  /* The cell hovered during Drag and Drop */
  gint                dnd_cell;

  /* Storage for the accumulated scrolling */
  gdouble             scroll_value;

  /* property */
  icaltimetype       *date;
  GcalManager        *manager;
};

#define MIRROR(val,start,end) (start + (val / end) * end + (end - val % end))

static void          gcal_view_interface_init                    (GcalViewInterface  *iface);

static void          gtk_buildable_interface_init                (GtkBuildableIface  *iface);

static void          e_data_model_subscriber_interface_init      (ECalDataModelSubscriberInterface *iface);

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
  PROP_MANAGER,
  N_PROPS
};

enum
{
  EVENT_ACTIVATED,
  NUM_SIGNALS
};

static guint signals[NUM_SIGNALS] = { 0, };


/*
 * Auxiliary functions
 */

static inline gint
real_cell (gint     cell,
           gboolean rtl)
{
  if (cell < 0)
    return cell;

  return rtl ? MIRROR (cell, 0, 7) - 1 : cell;
}

static inline void
cancel_selection (GcalMonthView *self)
{
  g_clear_pointer (&self->start_mark_cell, g_date_time_unref);
  g_clear_pointer (&self->end_mark_cell, g_date_time_unref);
}

static void
event_activated (GcalEventWidget *widget,
                 GcalMonthView   *self)
{
  cancel_selection (self);
  gtk_widget_hide (self->overflow_popover);

  g_signal_emit (self, signals[EVENT_ACTIVATED], 0, widget);
}

static void
event_visibility_changed (GtkWidget     *widget,
                          GcalMonthView *self)
{
  self->children_changed = TRUE;
}

static void
setup_child_widget (GcalMonthView *self,
                    GtkWidget     *widget)
{
  if (!gtk_widget_get_parent (widget))
    gtk_widget_set_parent (widget, GTK_WIDGET (self));

  g_signal_connect (widget, "activate", G_CALLBACK (event_activated), self);
  g_signal_connect (widget, "hide", G_CALLBACK (event_visibility_changed), self);
  g_signal_connect (widget, "show", G_CALLBACK (event_visibility_changed), self);
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

static gboolean
get_widget_parts (gint     first_cell,
                  gint     last_cell,
                  gint     natural_height,
                  gdouble *vertical_cell_space,
                  gdouble *size_left,
                  GArray  *cells,
                  GArray  *lengths)
{
  gdouble old_y;
  gdouble y;
  gint current_part_length;
  gint i;

  old_y  = -1.0;
  current_part_length = 0;

  if (last_cell < first_cell)
    {
      gint swap = last_cell;
      last_cell = first_cell;
      first_cell = swap;
    }

  for (i = first_cell; i <= last_cell; i++)
    {
      if (size_left[i] < natural_height)
        return FALSE;

      y = vertical_cell_space[i] - size_left[i];

      if (old_y == -1.0 || y != old_y)
        {
          current_part_length = 1;
          g_array_append_val (cells, i);
          g_array_append_val (lengths, current_part_length);
          old_y = y;
        }
      else
        {
          current_part_length++;
          g_array_index (lengths, gint, lengths->len - 1) = current_part_length;
        }
    }

  return TRUE;
}

static void
show_overflow_popover_cb (GcalMonthCell *cell,
                          GtkWidget     *button,
                          GcalMonthView *self)
{
  g_autofree gchar *label_title = NULL;
  GtkWidget *child_widget;
  GDateTime *dt;
  GList *l;
  gint day, cell_nr;

  dt = gcal_month_cell_get_date (cell);
  day = g_date_time_get_day_of_month (dt);
  cell_nr = day + self->days_delay - 1;

  if (!g_hash_table_contains (self->overflow_cells, GINT_TO_POINTER (cell_nr)))
    {
      g_critical ("No overflow events for day %d. Something is wrong.", cell_nr);
      return;
    }

  /* Popover title */
  label_title = g_date_time_format (dt, _("%B %d"));
  gtk_label_set_text (GTK_LABEL (self->popover_title), label_title);

  /* Clean all the widgets */
  gtk_container_foreach (GTK_CONTAINER (self->events_box), (GtkCallback) gtk_widget_destroy, NULL);

  l = g_hash_table_lookup (self->overflow_cells, GINT_TO_POINTER (cell_nr));

  /* Setup the start & end dates of the events as the begin & end of day */
  for (; l != NULL; l = g_list_next (l))
    {
      g_autoptr (GDateTime) current_date = NULL;
      g_autoptr (GDateTime) dt_start = NULL;
      g_autoptr (GDateTime) dt_end = NULL;
      g_autoptr (GTimeZone) tz = NULL;
      GtkWidget *cloned_event;
      GcalEvent *event;

      event = gcal_event_widget_get_event (l->data);

      if (gcal_event_get_all_day (event))
        tz = g_time_zone_new_utc ();
      else
        tz = g_time_zone_new_local ();

      current_date = icaltime_to_datetime (self->date);
      dt_start = g_date_time_new (tz,
                                  g_date_time_get_year (current_date),
                                  g_date_time_get_month (current_date),
                                  day, 0, 0, 0);

      dt_end = g_date_time_add_days (dt_start, 1);

      cloned_event = gcal_event_widget_clone (l->data);
      gcal_event_widget_set_date_start (GCAL_EVENT_WIDGET (cloned_event), dt_start);
      gcal_event_widget_set_date_end (GCAL_EVENT_WIDGET (cloned_event), dt_end);

      gtk_container_add (GTK_CONTAINER (self->events_box), cloned_event);
      setup_child_widget (self, cloned_event);
    }

  gtk_popover_set_relative_to (GTK_POPOVER (self->overflow_popover), button);

  /* sizing hack */
  child_widget = gtk_bin_get_child (GTK_BIN (self->overflow_popover));
  gtk_widget_set_size_request (child_widget, 200, -1);
  gtk_widget_show_all (child_widget);

  g_object_set_data (G_OBJECT (self->overflow_popover),
                     "selected-day",
                     GINT_TO_POINTER (day));

  gtk_popover_popup (GTK_POPOVER (self->overflow_popover));

  cancel_selection (self);
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

          g_signal_connect (cell,
                            "show-overflow",
                            G_CALLBACK (show_overflow_popover_cb),
                            self);

          self->month_cell[row][col] = cell;

          gtk_grid_attach (GTK_GRID (widget), cell, col, row, 1, 1);
        }
    }
}

static void
update_month_cells (GcalMonthView *self)
{
  g_autoptr (GDateTime) dt;
  guint row, col;

  dt = g_date_time_new_local (self->date->year, self->date->month, 1, 0, 0, 0);

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
                            day - self->days_delay >= icaltime_days_in_month (self->date->month, self->date->year);

          gcal_month_cell_set_different_month (cell, different_month);

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
              if (datetime_compare_date (selection_end, selection_start) < 0)
                {
                  GDateTime *aux = selection_end;
                  selection_end = selection_start;
                  selection_start = aux;
                }

              selected = datetime_compare_date (gcal_month_cell_get_date (cell), selection_start) >= 0 &&
                         datetime_compare_date (gcal_month_cell_get_date (cell), selection_end) <= 0;
            }

          gcal_month_cell_set_selected (cell, selected);

          /* Drag n' Drop */
          if (self->dnd_cell != row * 7 + col)
            gtk_drag_unhighlight (GTK_WIDGET (cell));

          gtk_widget_queue_draw (GTK_WIDGET (cell));
        }
    }
}

static void
update_header_labels (GcalMonthView *self)
{
  gchar year_str[10] = { 0, };

  g_snprintf (year_str, 10, "%d", self->date->year);

  gtk_label_set_label (GTK_LABEL (self->month_label), gcal_get_month_name (self->date->month - 1));
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
 * GtkWidget overrides
 */

static gboolean
gcal_month_view_key_press (GtkWidget   *widget,
                           GdkEventKey *event)
{
  GcalMonthView *self;
  gboolean create_event;
  gboolean selection;
  gboolean valid_key;
  gboolean is_ltr;
  gint min, max, diff, month_change, current_day;
  gint row, col;

  g_return_val_if_fail (GCAL_IS_MONTH_VIEW (widget), FALSE);

  self = GCAL_MONTH_VIEW (widget);
  selection = event->state & GDK_SHIFT_MASK;
  create_event = FALSE;
  valid_key = FALSE;
  diff = 0;
  month_change = 0;
  current_day = self->keyboard_cell - self->days_delay + 1;
  min = self->days_delay;
  max = self->days_delay + icaltime_days_in_month (self->date->month, self->date->year) - 1;
  is_ltr = gtk_widget_get_direction (widget) != GTK_TEXT_DIR_RTL;

  /*
   * If it's starting the selection right now, it should mark the current keyboard
   * focused cell as the start, and then update the end mark after updating the
   * focused cell.
   */
  if (selection && self->start_mark_cell == NULL)
      self->start_mark_cell = g_date_time_new_local (self->date->year, self->date->month, current_day, 0, 0, 0);

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
        self->start_mark_cell = self->end_mark_cell = g_date_time_new_local (self->date->year, self->date->month, current_day, 0, 0, 0);

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
      month_change = self->keyboard_cell + diff > max ? 1 : -1;
      self->date->month += month_change;
      *(self->date) = icaltime_normalize(*(self->date));

      self->days_delay = (time_day_of_week (1, self->date->month - 1, self->date->year) - self->first_weekday + 7) % 7;

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
        self->keyboard_cell = self->days_delay + icaltime_days_in_month (self->date->month, self->date->year) - min + self->keyboard_cell + diff;
    }

  /* Focus the selected month cell */
  row = self->keyboard_cell / 7;
  col = self->keyboard_cell % 7;

  gtk_widget_grab_focus (self->month_cell[row][col]);

  current_day = self->keyboard_cell - self->days_delay + 1;
  self->date->day = current_day;

  /*
   * We can only emit the :create-event signal ~after~ grabbing the focus, otherwise
   * the popover is instantly hidden.
   */
  if (create_event)
    emit_create_event (self);

  g_object_notify (G_OBJECT (widget), "active-date");

  if (selection)
    {
      self->end_mark_cell = g_date_time_new_local (self->date->year, self->date->month, current_day, 0, 0, 0);
    }
  else if (!selection && valid_key)
    {
      /* Cancel selection if SHIFT is not pressed */
      cancel_selection (GCAL_MONTH_VIEW (widget));
    }

  return GDK_EVENT_STOP;
}

static gboolean
cancel_dnd_from_overflow_popover (GtkPopover *popover)
{
  GCAL_ENTRY;

  gtk_popover_popdown (popover);

  GCAL_RETURN (FALSE);
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
      self->date->month += self->scroll_value > 0 ? 1 : -1;
      *self->date = icaltime_normalize (*self->date);
      self->scroll_value = 0;

      gtk_widget_queue_draw (widget);

      g_object_notify (G_OBJECT (widget), "active-date");
    }

  return GDK_EVENT_STOP;
}

static void
add_new_event_button_cb (GtkWidget *button,
                         gpointer   user_data)
{
  GcalMonthView *self;
  gint day;
  GDateTime *start_date;

  self = GCAL_MONTH_VIEW (user_data);

  gtk_widget_hide (self->overflow_popover);

  day = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (self->overflow_popover), "selected-day"));
  start_date = g_date_time_new_local (self->date->year, self->date->month, day, 0, 0, 0);

  g_signal_emit_by_name (GCAL_VIEW (user_data), "create-event-detailed", start_date, NULL);

  g_date_time_unref (start_date);
}

/* GcalView Interface API */
static icaltimetype*
gcal_month_view_get_date (GcalView *view)
{
  GcalMonthView *self = GCAL_MONTH_VIEW (view);

  return self->date;
}

static void
gcal_month_view_set_date (GcalView     *view,
                          icaltimetype *date)
{
  GcalMonthView *self;

  GCAL_ENTRY;

  self = GCAL_MONTH_VIEW (view);

  g_clear_pointer (&self->date, g_free);

  self->date = gcal_dup_icaltime (date);
  self->days_delay = (time_day_of_week (1, self->date->month - 1, self->date->year) - self->first_weekday + 7) % 7;
  self->keyboard_cell = self->days_delay + (self->date->day - 1);

  GCAL_TRACE_MSG ("new date: %s", icaltime_as_ical_string (*date));

  self->children_changed = TRUE;

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
gcal_view_interface_init (GcalViewInterface *iface)
{
  iface->get_date = gcal_month_view_get_date;
  iface->set_date = gcal_month_view_set_date;
  iface->clear_marks = gcal_month_view_clear_marks;
  iface->get_children_by_uuid = gcal_month_view_get_children_by_uuid;
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
  GtkWidget *event_widget;
  GcalEvent *event;
  GError *error;

  error = NULL;
  event = gcal_event_new (e_client_get_source (E_CLIENT (client)), comp, &error);

  if (error)
    {
      g_warning ("Error creating event: %s", error->message);
      g_clear_error (&error);
      return;
    }

  event_widget = gcal_event_widget_new (event);
  gcal_event_widget_set_read_only (GCAL_EVENT_WIDGET (event_widget), e_client_is_readonly (E_CLIENT (client)));

  gtk_widget_show (event_widget);
  gtk_container_add (GTK_CONTAINER (subscriber), event_widget);

  update_month_cells (GCAL_MONTH_VIEW (subscriber));

  g_clear_object (&event);
}

static void
gcal_month_view_component_modified (ECalDataModelSubscriber *subscriber,
                                    ECalClient              *client,
                                    ECalComponent           *comp)
{
  GcalMonthView *self;
  GtkWidget *new_widget;
  GcalEvent *event;
  GError *error;
  GList *l;

  error = NULL;
  self = GCAL_MONTH_VIEW (subscriber);
  event = gcal_event_new (e_client_get_source (E_CLIENT (client)), comp, &error);

  if (error)
    {
      g_warning ("Error creating event: %s", error->message);
      g_clear_error (&error);
      return;
    }

  new_widget = gcal_event_widget_new (event);

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

  update_month_cells (self);

  g_clear_object (&event);
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
      return;
    }

  gtk_widget_destroy (l->data);
  update_month_cells (self);
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

  self->children_changed = TRUE;
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
      self->children_changed = TRUE;

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

      g_hash_table_remove (self->hidden_as_overflow, uuid);
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

  /* Header */
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

    case PROP_MANAGER:
      self->manager = g_value_dup_object (value);

      g_signal_connect_swapped (gcal_manager_get_clock (self->manager),
                                "day-changed",
                                G_CALLBACK (gtk_widget_queue_draw),
                                self);

      for (i = 0; i < 42; i++)
        gcal_month_cell_set_manager (GCAL_MONTH_CELL (self->month_cell[i / 7][i % 7]), self->manager);

      g_object_notify (object, "manager");
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

    case PROP_MANAGER:
      g_value_set_object (value, self->manager);
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

  g_clear_pointer (&self->date, g_free);
  g_clear_pointer (&self->children, g_hash_table_destroy);
  g_clear_pointer (&self->single_cell_children, g_hash_table_destroy);
  g_clear_pointer (&self->overflow_cells, g_hash_table_destroy);
  g_clear_pointer (&self->hidden_as_overflow, g_hash_table_destroy);
  g_clear_pointer (&self->multi_cell_children, g_list_free);

  g_clear_object (&self->manager);

  /* Chain up to parent's finalize() method. */
  G_OBJECT_CLASS (gcal_month_view_parent_class)->finalize (object);
}

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
remove_cell_border_and_padding (GtkWidget *cell,
                                gdouble   *x,
                                gdouble   *y,
                                gdouble   *width)
{
  GtkStyleContext *cell_context;
  GtkBorder cell_padding;
  GtkBorder cell_border;

  cell_context = gtk_widget_get_style_context (cell);
  gtk_style_context_get_border (cell_context, gtk_style_context_get_state (cell_context), &cell_border);
  gtk_style_context_get_padding (cell_context, gtk_style_context_get_state (cell_context), &cell_padding);

  if (x)
    *x += cell_border.left + cell_padding.left;

  if (y)
    *y += cell_border.top + cell_padding.top;

  if (width)
    {
      *width -= cell_border.left + cell_border.right;
      *width -= cell_padding.left + cell_padding.right;
    }
}

static void
gcal_month_view_size_allocate (GtkWidget     *widget,
                               GtkAllocation *allocation)
{
  GtkStyleContext *child_context;
  GHashTableIter iter;
  GtkAllocation child_allocation;
  GtkAllocation cell_alloc;
  GcalMonthView *self;
  GtkWidget *child_widget;
  GtkWidget *month_cell;
  GtkBorder margin;
  gpointer key, value;
  GList *widgets, *aux, *l = NULL;
  const gchar *uuid;
  gdouble vertical_cell_space [42];
  gdouble size_left [42];
  gdouble pos_x, pos_y;
  gint minimum_height;
  gint header_height;
  gint grid_height;
  gint i, j, sw;

  self = gcal_month_view_get_instance_private (GCAL_MONTH_VIEW (widget));

  /* No need to relayout stuff if nothing changed */
  if (!self->children_changed &&
      allocation->height == gtk_widget_get_allocated_height (widget) &&
      allocation->width == gtk_widget_get_allocated_width (widget))
    {
      return;
    }

  /* Remove every widget' parts, but the master widget */
  widgets = g_hash_table_get_values (self->children);

  for (aux = widgets; aux != NULL; aux = g_list_next (aux))
    l = g_list_concat (l, g_list_copy (g_list_next (aux->data)));

  g_list_free (widgets);

  g_list_free_full (l, (GDestroyNotify) gtk_widget_destroy);

  /* Clean overflow information */
  g_hash_table_remove_all (self->overflow_cells);

  /* Allocate the widget */
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

  /* Event widgets */
  for (i = 0; i < 42; i++)
    {
      gint h = gcal_month_cell_get_real_height (GCAL_MONTH_CELL (self->month_cell[i / 7][i % 7]));

      vertical_cell_space[i] = h;
      size_left[i] = h;
    }

  sw = 1 - 2 * self->k;

  /*
   * Allocate multidays events before single day events, as they have a
   * higher priority. Multiday events have a master event, and clones
   * to represent the multiple sections of the event.
   */
  for (l = self->multi_cell_children; l != NULL; l = g_list_next (l))
    {
      GDateTime *date;
      GcalEvent *event;
      GArray *cells, *lengths;
      gboolean visible, all_day;
      gint first_cell, last_cell, first_row, last_row, start, end;

      child_widget = (GtkWidget*) l->data;
      event = gcal_event_widget_get_event (l->data);
      uuid = gcal_event_get_uid (event);
      all_day = gcal_event_get_all_day (event);
      child_context = gtk_widget_get_style_context (l->data);

      if (!gtk_widget_is_visible (child_widget) && !g_hash_table_contains (self->hidden_as_overflow, uuid))
        continue;

      gtk_widget_show (child_widget);
      gtk_widget_get_preferred_height (child_widget, &minimum_height, NULL);

      /*
       * If the month of the event's start date is equal to the month
       * we're visualizing, the first cell is the event's day of the
       * month. Otherwise, the first cell is the 1st day of the month.
       */
      j = 1;
      date = gcal_event_get_date_start (event);
      date = all_day ? g_date_time_ref (date) : g_date_time_to_local (date);

      if (g_date_time_get_year (date) == self->date->year &&
          g_date_time_get_month (date) == self->date->month)
        {
          j = g_date_time_get_day_of_month (date);
        }

      j += self->days_delay;

      g_clear_pointer (&date, g_date_time_unref);

      /*
       * Calculate the first cell position according to the locale
       * and the start date.
       */
      first_cell = 7 * ((j - 1) / 7)+ 6 * self->k + sw * ((j - 1) % 7);

      /*
       * The logic for the end date is the same, except that we have to check
       * if the event is all day or not.
       */
      j = icaltime_days_in_month (self->date->month, self->date->year);
      date = gcal_event_get_date_end (event);
      date = all_day ? g_date_time_ref (date) : g_date_time_to_local (date);

      if (g_date_time_get_month (date) == self->date->month)
        {
          j = g_date_time_get_day_of_month (date);

          /* If the event is all day, we have to subtract 1 to find the the real date */
          if (all_day)
            j--;
        }

      j += self->days_delay;

      g_clear_pointer (&date, g_date_time_unref);

      last_cell = 7 * ((j - 1) / 7)+ 6 * self->k + sw * ((j - 1) % 7);

      /*
       * Now that we found the start & end dates, we have to find out
       * whether the event is visible or not.
       */
      first_row = first_cell / 7;
      last_row = last_cell / 7;
      visible = TRUE;
      cells = g_array_sized_new (TRUE, TRUE, sizeof (gint), 16);
      lengths = g_array_sized_new (TRUE, TRUE, sizeof (gint), 16);

      for (i = first_row; i <= last_row && visible; i++)
        {
          start = i * 7 + self->k * 6;
          end = i * 7 + (1 - self->k) * 6;

          if (i == first_row)
            start = first_cell;
          if (i == last_row)
            end = last_cell;

          visible = get_widget_parts (start, end, minimum_height, vertical_cell_space, size_left, cells, lengths);
        }

      if (visible)
        {
          for (i = 0; i < cells->len; i++)
            {
              GDateTime *dt_start, *dt_end;
              gdouble width;
              gint cell_idx = g_array_index (cells, gint, i);
              gint length = g_array_index (lengths, gint, i);
              gint row = cell_idx / 7;
              gint column = cell_idx % 7;
              gint day = cell_idx - self->days_delay + 1;

              /* Day number is calculated differently on RTL languages */
              if (self->k)
                day = 7 * row + MIRROR (cell_idx % 7, 0, 7) - self->days_delay - length + 1;

              if (i != 0)
                {
                  child_widget = gcal_event_widget_clone (GCAL_EVENT_WIDGET (child_widget));

                  setup_child_widget (self, child_widget);
                  gtk_widget_show (child_widget);

                  child_context = gtk_widget_get_style_context (child_widget);

                  aux = g_hash_table_lookup (self->children, uuid);
                  aux = g_list_append (aux, child_widget);
                }

              /* Retrieve the cell widget */
              if (self->k)
                month_cell = self->month_cell[row][6 - column];
              else
                month_cell = self->month_cell[row][column];

              gtk_widget_get_allocation (month_cell, &cell_alloc);

              gtk_widget_get_preferred_height (child_widget, &minimum_height, NULL);

              /*
               * Setup the widget's start date as the first day of the row,
               * and the widget's end date as the last day of the row. We don't
               * have to worry about the dates, since GcalEventWidget performs
               * some checks and only applies the dates when it's valid.
               */
              dt_start = g_date_time_new (gcal_event_get_timezone (event),
                                          self->date->year,
                                          self->date->month,
                                          day,
                                          0, 0, 0);

              dt_end = g_date_time_add_days (dt_start, length);

              gcal_event_widget_set_date_start (GCAL_EVENT_WIDGET (child_widget), dt_start);
              gcal_event_widget_set_date_end (GCAL_EVENT_WIDGET (child_widget), dt_end);

              /* Position and allocate the child widget */
              gtk_style_context_get_margin (gtk_widget_get_style_context (child_widget),
                                            gtk_style_context_get_state (child_context),
                                            &margin);


              pos_x = cell_alloc.x + margin.left;
              pos_y = cell_alloc.y + margin.top;
              width = cell_alloc.width * length;

              remove_cell_border_and_padding (month_cell, &pos_x, &pos_y, &width);

              child_allocation.x = pos_x;
              child_allocation.y = pos_y + vertical_cell_space[cell_idx] - size_left[cell_idx];
              child_allocation.width = width - (margin.left + margin.right);
              child_allocation.height = minimum_height;

              gtk_widget_size_allocate (child_widget, &child_allocation);
              g_hash_table_remove (self->hidden_as_overflow, uuid);

              /* update size_left */
              for (j = 0; j < length; j++)
                {
                  size_left[cell_idx + j] -= minimum_height;
                  size_left[cell_idx + j] -= margin.top + margin.bottom;
                }

              g_clear_pointer (&dt_start, g_date_time_unref);
              g_clear_pointer (&dt_end, g_date_time_unref);
            }
        }
      else
        {
          gtk_widget_hide (child_widget);
          g_hash_table_add (self->hidden_as_overflow, g_strdup (uuid));

          for (i = first_cell; i <= last_cell; i++)
            {
              aux = g_hash_table_lookup (self->overflow_cells, GINT_TO_POINTER (i));
              aux = g_list_append (aux, child_widget);

              if (g_list_length (aux) == 1)
                g_hash_table_insert (self->overflow_cells, GINT_TO_POINTER (i), aux);
              else
                g_hash_table_replace (self->overflow_cells, GINT_TO_POINTER (i), g_list_copy (aux));
            }
        }

      g_array_free (cells, TRUE);
      g_array_free (lengths, TRUE);
    }

  /*
   * Allocate single day events after multiday ones. For single
   * day children, there's no need to calculate the start & end
   * dates of the widgets.
   */
  g_hash_table_iter_init (&iter, self->single_cell_children);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      j = GPOINTER_TO_INT (key);
      j += self->days_delay;
      i = 7 * ((j - 1) / 7) + 6 * self->k + sw * ((j - 1) % 7);

      if (self->k)
        month_cell = self->month_cell[i / 7][6 - i % 7];
      else
        month_cell = self->month_cell[i / 7][i % 7];

      gtk_widget_get_allocation (month_cell, &cell_alloc);

      l = (GList*) value;
      for (aux = l; aux != NULL; aux = g_list_next (aux))
        {
          GcalEvent *event;
          gdouble width;

          child_widget = aux->data;
          event = gcal_event_widget_get_event (aux->data);
          uuid = gcal_event_get_uid (event);

          if (!gtk_widget_is_visible (child_widget) && !g_hash_table_contains (self->hidden_as_overflow, uuid))
            continue;

          gtk_widget_show (child_widget);
          gtk_widget_get_preferred_height (child_widget, &minimum_height, NULL);

          if (size_left[i] > minimum_height)
            {
              child_context = gtk_widget_get_style_context (child_widget);

              gtk_style_context_get_margin (child_context, gtk_style_context_get_state (child_context), &margin);

              pos_x = cell_alloc.x + margin.left;
              pos_y = cell_alloc.y + margin.top;
              width = cell_alloc.width;

              remove_cell_border_and_padding (month_cell, &pos_x, &pos_y, &width);

              child_allocation.x = pos_x;
              child_allocation.y = pos_y + vertical_cell_space[i] - size_left[i];
              child_allocation.width = width - (margin.left + margin.right);
              child_allocation.height = minimum_height;
              gtk_widget_show (child_widget);
              gtk_widget_size_allocate (child_widget, &child_allocation);
              g_hash_table_remove (self->hidden_as_overflow, uuid);

              size_left[i] -= minimum_height + margin.top + margin.bottom;
            }
          else
            {
              gtk_widget_hide (child_widget);
              g_hash_table_add (self->hidden_as_overflow, g_strdup (uuid));

              l = g_hash_table_lookup (self->overflow_cells, GINT_TO_POINTER (i));
              l = g_list_append (l, child_widget);

              if (g_list_length (l) == 1)
                g_hash_table_insert (self->overflow_cells, GINT_TO_POINTER (i), l);
              else
                g_hash_table_replace (self->overflow_cells, GINT_TO_POINTER (i), g_list_copy (l));
            }
        }
    }

  update_month_cells (self);

  self->children_changed = FALSE;
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
  days = self->days_delay + icaltime_days_in_month (self->date->month, self->date->year);

  /* The event may have come from a child widget, so make it relative to the month view */
  if (!gcal_translate_child_window_position (widget, event->window, event->x, event->y, &x, &y))
    return GDK_EVENT_PROPAGATE;

  get_month_cell_at_position (self, x, y, &clicked_cell);

  clicked_cell = real_cell (clicked_cell, self->k);

  if (clicked_cell >= self->days_delay && clicked_cell < days)
    {
      g_clear_pointer (&self->start_mark_cell, g_date_time_unref);

      self->keyboard_cell = clicked_cell;
      self->start_mark_cell = g_date_time_new_local (self->date->year, self->date->month,
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
  days = self->days_delay + icaltime_days_in_month (self->date->month, self->date->year);

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
      self->end_mark_cell = g_date_time_new_local (self->date->year,
                                                   self->date->month,
                                                   new_end_cell - self->days_delay + 1,
                                                   0, 0, 0);

      update_month_cells (self);

      GCAL_RETURN (GDK_EVENT_STOP);
    }
  else
    {
      if (gtk_widget_is_visible (self->overflow_popover))
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
  days = self->days_delay + icaltime_days_in_month (self->date->month, self->date->year);

  if (!gcal_translate_child_window_position (widget, event->window, event->x, event->y, &x, &y))
    GCAL_RETURN (GDK_EVENT_PROPAGATE);

  get_month_cell_at_position (self, x, y, &current_day);

  if (current_day >= self->days_delay && current_day < days)
    {
      gboolean valid;

      g_clear_pointer (&self->end_mark_cell, g_date_time_unref);

      self->keyboard_cell = current_day;
      self->end_mark_cell = g_date_time_new_local (self->date->year, self->date->month, current_day - self->days_delay + 1, 0, 0, 0);

      self->date->day = g_date_time_get_day_of_month (self->end_mark_cell);

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

      gtk_widget_queue_draw (widget);

      GCAL_RETURN (GDK_EVENT_PROPAGATE);
    }
}

static void
gcal_month_view_direction_changed (GtkWidget        *widget,
                                   GtkTextDirection  previous_direction)
{
  GcalMonthView *self = GCAL_MONTH_VIEW (widget);

  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR)
    self->k = 0;
  else if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
    self->k = 1;
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
  g_object_class_override_property (object_class, PROP_MANAGER, "manager");

  signals[EVENT_ACTIVATED] = g_signal_new ("event-activated",
                                           GCAL_TYPE_MONTH_VIEW,
                                           G_SIGNAL_RUN_LAST,
                                           0, NULL, NULL, NULL,
                                           G_TYPE_NONE,
                                           1,
                                           GCAL_TYPE_EVENT_WIDGET);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/month-view.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalMonthView, events_box);
  gtk_widget_class_bind_template_child (widget_class, GcalMonthView, label_0);
  gtk_widget_class_bind_template_child (widget_class, GcalMonthView, label_1);
  gtk_widget_class_bind_template_child (widget_class, GcalMonthView, label_2);
  gtk_widget_class_bind_template_child (widget_class, GcalMonthView, label_3);
  gtk_widget_class_bind_template_child (widget_class, GcalMonthView, label_4);
  gtk_widget_class_bind_template_child (widget_class, GcalMonthView, label_5);
  gtk_widget_class_bind_template_child (widget_class, GcalMonthView, label_6);
  gtk_widget_class_bind_template_child (widget_class, GcalMonthView, month_label);
  gtk_widget_class_bind_template_child (widget_class, GcalMonthView, overflow_popover);
  gtk_widget_class_bind_template_child (widget_class, GcalMonthView, popover_title);
  gtk_widget_class_bind_template_child (widget_class, GcalMonthView, year_label);

  gtk_widget_class_bind_template_callback (widget_class, add_new_event_button_cb);
  gtk_widget_class_bind_template_callback (widget_class, cancel_dnd_from_overflow_popover);

  gtk_widget_class_set_css_name (widget_class, "calendar-view");
}

static void
gcal_month_view_init (GcalMonthView *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);

  self->dnd_cell = -1;
  self->children = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_list_free);
  self->single_cell_children = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, (GDestroyNotify) g_list_free);
  self->overflow_cells = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, (GDestroyNotify) g_list_free);
  self->hidden_as_overflow = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  self->k = gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL;

  /*
   * Also set the overflow popover as a drop destination, so we can hide
   * it when the user starts dragging an event that is inside the overflow
   * list.
   */
  gtk_drag_dest_set (GTK_WIDGET (self->overflow_popover),
                     0,
                     NULL,
                     0,
                     GDK_ACTION_MOVE);

  /* Weekday header labels */
  self->weekday_label[0] = self->label_0;
  self->weekday_label[1] = self->label_1;
  self->weekday_label[2] = self->label_2;
  self->weekday_label[3] = self->label_3;
  self->weekday_label[4] = self->label_4;
  self->weekday_label[5] = self->label_5;
  self->weekday_label[6] = self->label_6;

  update_weekday_labels (self);
}

/* Public API */

/**
 * gcal_month_view_set_first_weekday:
 * @view: A #GcalMonthView instance
 * @day_nr: Integer representing the first day of the week
 *
 * Set the first day of the week according to the locale, being
 * 0 for Sunday, 1 for Monday and so on.
 */
void
gcal_month_view_set_first_weekday (GcalMonthView *self,
                                   gint           day_nr)
{
  g_return_if_fail (GCAL_IS_MONTH_VIEW (self));

  self->first_weekday = day_nr;

  /* update days_delay */
  if (self->date)
    self->days_delay = (time_day_of_week (1, self->date->month - 1, self->date->year) - self->first_weekday + 7) % 7;

  update_weekday_labels (self);
}

/**
 * gcal_month_view_set_use_24h_format:
 * @view:
 * @use_24h:
 *
 * Whether the view will show time using 24h or 12h format
 */
void
gcal_month_view_set_use_24h_format (GcalMonthView *self,
                                    gboolean       use_24h)
{
  self->use_24h_format = use_24h;
}
