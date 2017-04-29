/* -*- mode: c; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gcal-month-view.c
 *
 * Copyright (C) 2015 - Erick Pérez Castellanos
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
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

#include "gcal-debug.h"
#include "gcal-month-view.h"
#include "gcal-subscriber-view-private.h"
#include "gcal-utils.h"
#include "gcal-view.h"

#include <glib/gi18n.h>

#include <math.h>

#define LINE_WIDTH      0.5

struct _GcalMonthView
{
  GcalSubscriberView parent;

  GtkWidget      *overflow_popover;
  GtkWidget      *events_list_box;
  GtkWidget      *popover_title;

  GdkWindow      *event_window;

  /*
   * the cell on which its drawn the first day of the month, in the first row, 0 for the first cell, 1 for the second,
   * and so on, this takes first_weekday into account already.
   */
  gint            days_delay;

  /*
   * The cell whose keyboard focus is on.
   */
  gint            keyboard_cell;

  /*
   * first day of the week according to user locale, being
   * 0 for Sunday, 1 for Monday and so on */
  gint            first_weekday;

  /*
   * The start & end dates of the selection. We use datetimes to allow the user to navigate between months using
   * the keyboard.
   */
  GDateTime      *start_mark_cell;
  GDateTime      *end_mark_cell;

  /**
   * This flags signals when the cursor is over an overflow indicator and when once has been clicked
   */
  gint            pressed_overflow_indicator;
  gint            hovered_overflow_indicator;

  /**
   * clock format from GNOME desktop settings
   */
  gboolean        use_24h_format;

  /* text direction factors */
  gboolean        k;

  /* The cell hovered during Drag and Drop */
  gint            dnd_cell;

  /* Storage for the accumulated scrolling */
  gdouble         scroll_value;

  /* property */
  icaltimetype   *date;
  GcalManager    *manager;
};

#define MIRROR(val,start,end) (start + (val / end) * end + (end - val % end))

static void           gcal_view_interface_init              (GcalViewInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GcalMonthView, gcal_month_view, GCAL_TYPE_SUBSCRIBER_VIEW,
                         G_IMPLEMENT_INTERFACE (GCAL_TYPE_VIEW, gcal_view_interface_init));

enum
{
  PROP_0,
  PROP_DATE,  /* active-date inherited property */
};

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

static gdouble
get_start_grid_y (GtkWidget *widget)
{
  GtkStyleContext* context;
  GtkStateFlags state_flags;
  gint padding_top;

  PangoLayout *layout;
  PangoFontDescription *font_desc;
  gint font_height;
  gdouble start_grid_y;

  context = gtk_widget_get_style_context (widget);
  state_flags = gtk_style_context_get_state (context);

  layout = gtk_widget_create_pango_layout (widget, NULL);

  gtk_style_context_save (context);
  gtk_style_context_add_class (context, "start-header");
  gtk_style_context_get (context, state_flags, "padding-top", &padding_top, "font", &font_desc, NULL);

  pango_layout_set_font_description (layout, font_desc);
  pango_layout_get_pixel_size (layout, NULL, &font_height);

  pango_font_description_free (font_desc);
  gtk_style_context_restore (context);

  start_grid_y = 2 * padding_top + font_height;

  gtk_style_context_get (context, state_flags, "font", &font_desc, "padding-top", &padding_top, NULL);

  pango_layout_set_font_description (layout, font_desc);
  pango_layout_get_pixel_size (layout, NULL, &font_height);

  start_grid_y += padding_top + font_height;

  pango_font_description_free (font_desc);
  g_object_unref (layout);
  return start_grid_y;
}

static void
get_cell_position (GcalMonthView *self,
                   gint           cell,
                   gdouble       *out_x,
                   gdouble       *out_y)
{
  GtkWidget *widget;

  gdouble start_grid_y;

  gint shown_rows;
  gdouble first_row_gap;

  gdouble cell_width;
  gdouble cell_height;

  widget = GTK_WIDGET (self);

  start_grid_y = get_start_grid_y (widget);

  shown_rows = ceil ((self->days_delay + icaltime_days_in_month (self->date->month, self->date->year)) / 7.0);
  first_row_gap = (6 - shown_rows) * 0.5; /* invalid area before the actual rows */

  cell_width = gtk_widget_get_allocated_width (widget) / 7.0;
  cell_height = (gtk_widget_get_allocated_height (widget) - start_grid_y) / 6.0;

  if (out_x != NULL)
    *out_x = cell_width * ((cell % 7) + 0.5);

  if (out_y != NULL)
    *out_y = cell_height * ((cell / 7) + first_row_gap + 0.5) + start_grid_y;
}
static void
rebuild_popover_for_day (GcalMonthView *self,
                         gint           day)
{
  GcalSubscriberViewPrivate *ppriv;
  GList *l;
  GtkWidget *child_widget;
  GdkRectangle rect;

  gchar *label_title;
  /* placement helpers */
  gdouble start_grid_y, cell_width, cell_height;

  gint shown_rows;
  gint real_clicked_popover_cell;

  GtkStyleContext *context;
  GtkStateFlags state;

  PangoLayout *overflow_layout;
  PangoFontDescription *ofont_desc;
  gint font_height, padding_bottom;

  ppriv = GCAL_SUBSCRIBER_VIEW (self)->priv;

  label_title = g_strdup_printf ("%s %d", gcal_get_month_name (self->date->month - 1), day);
  gtk_label_set_text (GTK_LABEL (self->popover_title), label_title);
  g_free (label_title);

  /* Clean all the widgets */
  gtk_container_foreach (GTK_CONTAINER (self->events_list_box), (GtkCallback) gtk_widget_destroy, NULL);

  l = g_hash_table_lookup (ppriv->overflow_cells, GINT_TO_POINTER (self->pressed_overflow_indicator));

  /* Setup the start & end dates of the events as the begin & end of day */
  if (l)
    {
      for (; l != NULL; l = g_list_next (l))
        {
          GtkWidget *cloned_event;
          GDateTime *current_date;
          GDateTime *dt_start;
          GDateTime *dt_end;
          GcalEvent *event;
          GTimeZone *tz;

          event = gcal_event_widget_get_event (l->data);

          if (gcal_event_get_all_day (event))
            tz = g_time_zone_new_utc ();
          else
            tz = g_time_zone_new_local ();

          current_date = icaltime_to_datetime (self->date);
          dt_start = g_date_time_new (tz,
                                      g_date_time_get_year (current_date),
                                      g_date_time_get_month (current_date),
                                      day,
                                      0, 0, 0);

          dt_end = g_date_time_add_days (dt_start, 1);

          cloned_event = gcal_event_widget_clone (l->data);
          gcal_event_widget_set_date_start (GCAL_EVENT_WIDGET (cloned_event), dt_start);
          gcal_event_widget_set_date_end (GCAL_EVENT_WIDGET (cloned_event), dt_end);

          gtk_container_add (GTK_CONTAINER (self->events_list_box), cloned_event);
          _gcal_subscriber_view_setup_child (GCAL_SUBSCRIBER_VIEW (self), cloned_event);

          g_clear_pointer (&current_date, g_date_time_unref);
          g_clear_pointer (&dt_start, g_date_time_unref);
          g_clear_pointer (&dt_end, g_date_time_unref);
          g_clear_pointer (&tz, g_time_zone_unref);
        }
    }

  /* placement calculation */
  start_grid_y = get_start_grid_y (GTK_WIDGET (self));
  shown_rows = ceil ((self->days_delay + icaltime_days_in_month (self->date->month, self->date->year)) / 7.0);

  cell_width = gtk_widget_get_allocated_width (GTK_WIDGET (self)) / 7.0;
  cell_height = (gtk_widget_get_allocated_height (GTK_WIDGET (self)) - start_grid_y) / 6.0;

  context = gtk_widget_get_style_context (GTK_WIDGET (self));
  state = gtk_style_context_get_state (context);
  gtk_style_context_save (context);
  gtk_style_context_add_class (context, "overflow");
  gtk_style_context_get (context, state, "font", &ofont_desc, "padding-bottom", &padding_bottom, NULL);

  overflow_layout = gtk_widget_create_pango_layout (GTK_WIDGET (self), NULL);
  pango_layout_set_font_description (overflow_layout, ofont_desc);
  pango_layout_get_pixel_size (overflow_layout, NULL, &font_height);

  gtk_style_context_restore (context);
  pango_font_description_free (ofont_desc);
  g_object_unref (overflow_layout);

  real_clicked_popover_cell = real_cell (self->pressed_overflow_indicator, self->k);
  rect.y = cell_height * ((real_clicked_popover_cell / 7) + 1.0 + (6 - shown_rows) * 0.5) + start_grid_y - padding_bottom - font_height / 2;
  rect.width = 1;
  rect.height = 1;

  if (real_clicked_popover_cell % 7 < 3)
    {
      rect.x = cell_width * ((real_clicked_popover_cell % 7) + 1.0);
      gtk_popover_set_position (GTK_POPOVER (self->overflow_popover), self->k ? GTK_POS_LEFT : GTK_POS_RIGHT);
    }
  else if (real_clicked_popover_cell % 7 > 3)
    {
      rect.x = cell_width * ((real_clicked_popover_cell % 7));
      gtk_popover_set_position (GTK_POPOVER (self->overflow_popover), self->k ? GTK_POS_RIGHT : GTK_POS_LEFT);
    }
  else
    {
      rect.x = cell_width * ((real_clicked_popover_cell % 7) + 1.0 - self->k);
      gtk_popover_set_position (GTK_POPOVER (self->overflow_popover), GTK_POS_RIGHT);
    }
  gtk_popover_set_pointing_to (GTK_POPOVER (self->overflow_popover), &rect);

  /* sizing hack */
  child_widget = gtk_bin_get_child (GTK_BIN (self->overflow_popover));
  gtk_widget_set_size_request (child_widget, 200, -1);

  g_object_set_data (G_OBJECT (self->overflow_popover), "selected-day", GINT_TO_POINTER (day));
}

static gboolean
show_popover_for_position (GcalMonthView *self,
                           gdouble        x,
                           gdouble        y,
                           gboolean       on_indicator)
{
  GcalSubscriberViewPrivate *ppriv;
  GtkWidget *widget;
  GDateTime *end_dt,*start_dt;

  widget = GTK_WIDGET (self);
  ppriv = GCAL_SUBSCRIBER_VIEW (self)->priv;
  start_dt = self->start_mark_cell;
  end_dt = self->end_mark_cell;

  if (self->start_mark_cell == NULL)
    return FALSE;

  if (self->pressed_overflow_indicator != -1 &&
      g_date_time_equal (start_dt, end_dt) &&
      g_hash_table_contains (ppriv->overflow_cells, GINT_TO_POINTER (self->pressed_overflow_indicator)))
    {
      self->hovered_overflow_indicator = self->pressed_overflow_indicator;

      rebuild_popover_for_day (GCAL_MONTH_VIEW (widget), g_date_time_get_day_of_month (end_dt));
      gtk_widget_show_all (self->overflow_popover);

      gtk_widget_queue_draw (widget);
      self->pressed_overflow_indicator = -1;

      cancel_selection (GCAL_MONTH_VIEW (widget));
      return TRUE;
    }
  else
    {
      gboolean should_clear_end = FALSE;

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

      g_signal_emit_by_name (GCAL_VIEW (widget), "create-event", start_dt, end_dt, x, y);

      if (should_clear_end)
        g_clear_pointer (&end_dt, g_date_time_unref);
    }

  gtk_widget_queue_draw (widget);
  self->pressed_overflow_indicator = -1;

  return FALSE;
}

static gint
gather_button_event_data (GcalMonthView *self,
                          gdouble        x,
                          gdouble        y,
                          gboolean      *out_on_indicator,
                          gdouble       *out_x,
                          gdouble       *out_y)
{
  GtkWidget *widget;

  gdouble start_grid_y;

  gint shown_rows;
  gdouble first_row_gap;

  gdouble cell_width;
  gdouble cell_height;

  gint cell;

  GtkStyleContext *context;
  GtkStateFlags state;

  PangoLayout *overflow_layout;
  PangoFontDescription *ofont_desc;
  gint font_height, padding_bottom, padding_top;

  widget = GTK_WIDGET (self);

  start_grid_y = get_start_grid_y (widget);

  shown_rows = ceil ((self->days_delay + icaltime_days_in_month (self->date->month, self->date->year)) / 7.0);
  first_row_gap = (6 - shown_rows) * 0.5; /* invalid area before the actual rows */

  cell_width = gtk_widget_get_allocated_width (widget) / 7.0;
  cell_height = (gtk_widget_get_allocated_height (widget) - start_grid_y) / 6.0;

  context = gtk_widget_get_style_context (widget);
  state = gtk_style_context_get_state (context);
  gtk_style_context_save (context);
  gtk_style_context_add_class (context, "overflow");
  gtk_style_context_get (context, state,
                         "font", &ofont_desc,
                         "padding-bottom", &padding_bottom,
                         "padding-top", &padding_top,
                         NULL);

  overflow_layout = gtk_widget_create_pango_layout (widget, NULL);
  pango_layout_set_font_description (overflow_layout, ofont_desc);
  pango_layout_get_pixel_size (overflow_layout, NULL, &font_height);

  gtk_style_context_restore (context);
  pango_font_description_free (ofont_desc);
  g_object_unref (overflow_layout);

  y = y - start_grid_y - first_row_gap * cell_height;

  /* When we're dealing with 5-row months, the first_row_gap
   * is never big enought, and (gint)(y / cell_height) always
   * rounds to 0, resulting in a valid cell number when it is
   * actually invalid.
   */
  cell = y < 0 ? -1 : 7 * (gint)(y / cell_height) + (gint)(x / cell_width);

  if (out_on_indicator != NULL)
    {
      gdouble upper_border = cell_height * ((gint) y / (gint) cell_height);
      y = y - upper_border;

      *out_on_indicator = (cell_height - font_height - padding_bottom - padding_top < y && y < cell_height);
    }

  if (out_x != NULL)
    *out_x = cell_width * ((cell % 7) + 0.5);

  if (out_y != NULL)
    *out_y = cell_height * ((cell / 7) + first_row_gap + 0.5) + start_grid_y;

  return cell;
}

static gboolean
get_widget_parts (gint     first_cell,
                  gint     last_cell,
                  gint     natural_height,
                  gdouble  vertical_cell_space,
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

      y = vertical_cell_space - size_left[i];

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
overflow_popover_hide (GtkWidget *widget,
                       gpointer   user_data)
{
  GcalMonthView *self = GCAL_MONTH_VIEW (widget);

  self->hovered_overflow_indicator = -1;
  gtk_widget_queue_draw (widget);
}

static void
update_list_box_headers (GtkListBoxRow *row,
                         GtkListBoxRow *before,
                         gpointer user_data)
{
  GcalMonthView *self;
  GtkWidget *row_child, *before_child = NULL;
  GDateTime *row_date, *before_date = NULL;
  GcalEvent *row_event;

  self = GCAL_MONTH_VIEW (user_data);
  row_child = gtk_bin_get_child (GTK_BIN (row));
  row_event = gcal_event_widget_get_event (GCAL_EVENT_WIDGET (row_child));
  row_date = gcal_event_widget_get_date_start (GCAL_EVENT_WIDGET (row_child));
  row_date = g_date_time_to_local (row_date);
  if (before != NULL)
    {
      before_child = gtk_bin_get_child (GTK_BIN (before));
      before_date = gcal_event_widget_get_date_start (GCAL_EVENT_WIDGET (before_child));
      before_date = g_date_time_to_local (before_date);
    }

  if (!gcal_event_is_multiday (row_event) &&
      !gcal_event_get_all_day (row_event) &&
      (before_date == NULL || g_date_time_get_hour (before_date) != g_date_time_get_hour (row_date)))
    {
      gchar *time;
      GtkWidget *label, *vbox;
      gint hour;

      hour = g_date_time_get_hour (row_date);

      if (self->use_24h_format)
        time = g_strdup_printf ("%.2d:00", hour);
      else
        time = g_strdup_printf ("%.2d:00 %s", hour % 12, hour < 12 ? "AM" : "PM");

      label = gtk_label_new (time);
      gtk_style_context_add_class (gtk_widget_get_style_context (label), GTK_STYLE_CLASS_DIM_LABEL);
      g_object_set (label, "margin-start", 6, "halign", GTK_ALIGN_START, NULL);

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_container_add (GTK_CONTAINER (vbox), label);
      gtk_container_add (GTK_CONTAINER (vbox), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));
      gtk_widget_show_all (vbox);

      gtk_list_box_row_set_header (row, vbox);
      g_free (time);
    }

  g_clear_pointer (&before_date, g_date_time_unref);
  g_clear_pointer (&row_date, g_date_time_unref);
}

static gboolean
gcal_month_view_key_press (GtkWidget   *widget,
                           GdkEventKey *event)
{
  GcalMonthView *self;
  gboolean selection, valid_key;
  gdouble x, y;
  gint min, max, diff, month_change, current_day;

  g_return_val_if_fail (GCAL_IS_MONTH_VIEW (widget), FALSE);

  self = GCAL_MONTH_VIEW (widget);
  selection = event->state & GDK_SHIFT_MASK;
  valid_key = FALSE;
  diff = 0;
  month_change = 0;
  current_day = self->keyboard_cell - self->days_delay + 1;
  min = self->days_delay;
  max = self->days_delay + icaltime_days_in_month (self->date->month, self->date->year) - 1;

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
      diff = self->k ? 1 : -1;
      break;

    case GDK_KEY_Right:
      valid_key = TRUE;
      diff = self->k ? -1 : 1;
      break;

    case GDK_KEY_Return:
      /*
       * If it's not on the selection mode (i.e. shift is not pressed), we should
       * simulate it by changing the start & end selected cells = keyboard cell.
       */
      if (!selection && self->start_mark_cell == NULL && self->end_mark_cell == NULL)
        self->start_mark_cell = self->end_mark_cell = g_date_time_new_local (self->date->year, self->date->month, current_day, 0, 0, 0);

      get_cell_position (GCAL_MONTH_VIEW (widget), real_cell (current_day + self->days_delay - 1, self->k), &x, &y);
      show_popover_for_position (GCAL_MONTH_VIEW (widget), x, y, FALSE);
      break;

    case GDK_KEY_Escape:
      cancel_selection (GCAL_MONTH_VIEW (widget));
      break;

    default:
      return FALSE;
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

  current_day = self->keyboard_cell - self->days_delay + 1;
  self->date->day = current_day;
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

  gtk_widget_queue_draw (widget);

  return TRUE;
}

/*
 * Drag and Drop functions
 */
static gint
get_dnd_cell (GtkWidget *widget,
              gint       x,
              gint       y)
{
  GcalMonthView *self;
  gint cell;

  self = GCAL_MONTH_VIEW (widget);
  cell = gather_button_event_data (GCAL_MONTH_VIEW (widget), x, y, NULL, NULL, NULL);
  cell = real_cell (cell, self->k);

  if (cell >= self->days_delay &&
      cell < g_date_get_days_in_month (self->date->month, self->date->year) + self->days_delay)
    {
      return cell;
    }

  return -1;
}

static gboolean
gcal_month_view_drag_motion (GtkWidget      *widget,
                             GdkDragContext *context,
                             gint            x,
                             gint            y,
                             guint           time)
{
  GcalMonthView *self;

  self = GCAL_MONTH_VIEW (widget);
  self->dnd_cell = get_dnd_cell (widget, x, y);

  /* Setup the drag highlight */
  if (self->dnd_cell != -1)
    {
      gtk_drag_highlight (widget);
      gtk_widget_hide (self->overflow_popover);
    }
  else
    {
      gtk_drag_unhighlight (widget);
    }

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
gcal_month_view_drag_drop (GtkWidget      *widget,
                           GdkDragContext *context,
                           gint            x,
                           gint            y,
                           guint           time)
{
  GcalSubscriberViewPrivate *ppriv = GCAL_SUBSCRIBER_VIEW (widget)->priv;
  GcalMonthView *self = GCAL_MONTH_VIEW (widget);
  GtkWidget *event_widget;
  GDateTime *start_dt, *end_dt, *current_dt;
  GcalEvent *event;
  gint cell, diff;
  gint start_month, current_month;
  gint start_year, current_year;
  GTimeSpan timespan = 0;

  cell = get_dnd_cell (widget, x, y);
  event_widget = gtk_drag_get_source_widget (context);

  if (!GCAL_IS_EVENT_WIDGET (event_widget))
    return FALSE;

  event = gcal_event_widget_get_event (GCAL_EVENT_WIDGET (event_widget));

  /* Move the event's date */
  start_dt = gcal_event_get_date_start (event);
  end_dt = gcal_event_get_date_end (event);

  start_month = g_date_time_get_month (start_dt);
  start_year = g_date_time_get_year (start_dt);

  current_dt = icaltime_to_datetime (self->date);

  current_month = g_date_time_get_month (current_dt);
  current_year = g_date_time_get_year (current_dt);

  g_clear_pointer (&current_dt, g_date_time_unref);

  if (end_dt)
    timespan = g_date_time_difference (end_dt, start_dt);

  start_dt = g_date_time_add_full (start_dt,
                                   current_year - start_year,
                                   current_month - start_month,
                                   0, 0, 0, 0);

  diff = cell - self->days_delay - g_date_time_get_day_of_month (start_dt) + 1;
  if (diff != 0 ||
      current_month != start_month ||
      current_year != start_year)
    {
      GDateTime *new_start = g_date_time_add_days (start_dt, diff);

      gcal_event_set_date_start (event, new_start);

      /* The event may have a NULL end date, so we have to check it here */
      if (end_dt)
        {
          GDateTime *new_end = g_date_time_add (new_start, timespan);

          gcal_event_set_date_end (event, new_end);
          g_clear_pointer (&new_end, g_date_time_unref);
        }

      gcal_manager_update_event (self->manager, event);

      g_clear_pointer (&new_start, g_date_time_unref);

      ppriv->children_changed = TRUE;
    }

  g_clear_pointer (&start_dt, g_date_time_unref);

  /* Cancel the DnD */
  self->dnd_cell = -1;
  gtk_drag_unhighlight (widget);

  gtk_drag_finish (context, TRUE, FALSE, time);

  gtk_widget_queue_draw (widget);

  return TRUE;
}

static void
gcal_month_view_drag_leave (GtkWidget      *widget,
                            GdkDragContext *context,
                            guint           time)
{
  GcalMonthView *self = GCAL_MONTH_VIEW (widget);

  /* Cancel the drag */
  self->dnd_cell = -1;
  gtk_drag_unhighlight (widget);

  gtk_widget_queue_draw (widget);
}

static gboolean
cancel_dnd_from_overflow_popover (GtkWidget *popover)
{
  gtk_widget_hide (popover);

  return FALSE;
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
  GcalSubscriberViewPrivate *ppriv;
  GcalMonthView *self;

  GCAL_ENTRY;

  self = GCAL_MONTH_VIEW (view);
  ppriv = GCAL_SUBSCRIBER_VIEW (view)->priv;

  g_clear_pointer (&self->date, g_free);

  self->date = gcal_dup_icaltime (date);
  self->days_delay = (time_day_of_week (1, self->date->month - 1, self->date->year) - self->first_weekday + 7) % 7;
  self->keyboard_cell = self->days_delay + (self->date->day - 1);

  GCAL_TRACE_MSG ("new date: %s", icaltime_as_ical_string (*date));

  ppriv->children_changed = TRUE;
  gtk_widget_queue_resize (GTK_WIDGET (view));

  GCAL_EXIT;
}

static void
gcal_month_view_clear_marks (GcalView *view)
{
  cancel_selection (GCAL_MONTH_VIEW (view));

  gtk_widget_queue_draw (GTK_WIDGET (view));
}

static GList*
gcal_month_view_get_children_by_uuid (GcalView    *view,
                                      const gchar *uuid)
{
  GcalSubscriberViewPrivate *ppriv = GCAL_SUBSCRIBER_VIEW (view)->priv;
  GList *l;

  l = g_hash_table_lookup (ppriv->children, uuid);
  if (l != NULL)
    return g_list_reverse (g_list_copy (l));

  return NULL;
}

static void
gcal_view_interface_init (GcalViewInterface *iface)
{
  iface->get_date = gcal_month_view_get_date;
  iface->set_date = gcal_month_view_set_date;
  iface->clear_marks = gcal_month_view_clear_marks;
  iface->get_children_by_uuid = gcal_month_view_get_children_by_uuid;
}

static void
gcal_month_view_set_property (GObject       *object,
                              guint          property_id,
                              const GValue  *value,
                              GParamSpec    *pspec)
{
  switch (property_id)
    {
    case PROP_DATE:
      gcal_view_set_date (GCAL_VIEW (object), g_value_get_boxed (value));
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
  gtk_widget_set_window (widget, parent_window);
  g_object_ref (parent_window);

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

  if (self->event_window != NULL)
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

  if (self->event_window != NULL)
    gdk_window_show (self->event_window);

  GTK_WIDGET_CLASS (gcal_month_view_parent_class)->map (widget);
}

static void
gcal_month_view_unmap (GtkWidget *widget)
{
  GcalMonthView *self = GCAL_MONTH_VIEW (widget);

  if (self->event_window != NULL)
    gdk_window_hide (self->event_window);

  GTK_WIDGET_CLASS (gcal_month_view_parent_class)->unmap (widget);
}

static void
gcal_month_view_size_allocate (GtkWidget     *widget,
                               GtkAllocation *allocation)
{
  GcalSubscriberViewPrivate *ppriv;
  GtkStyleContext *context, *child_context;
  GcalMonthView *self;

  gint i, j, sw, shown_rows;

  GtkBorder margin;
  gint padding_bottom, font_height;
  PangoLayout *layout;
  PangoFontDescription *font_desc;

  gdouble start_grid_y, first_row_gap, cell_width, cell_height, vertical_cell_space;
  gdouble pos_x, pos_y, size_left [42];

  const gchar *uuid;
  GtkWidget *child_widget;
  GtkAllocation child_allocation;
  gint natural_height;

  GList *widgets, *aux, *l = NULL;
  GHashTableIter iter;
  gpointer key, value;

  self = gcal_month_view_get_instance_private (GCAL_MONTH_VIEW (widget));
  context = gtk_widget_get_style_context (widget);
  ppriv = GCAL_SUBSCRIBER_VIEW (widget)->priv;

  /* No need to relayout stuff if nothing changed */
  if (!ppriv->children_changed &&
      allocation->height == gtk_widget_get_allocated_height (widget) &&
      allocation->width == gtk_widget_get_allocated_width (widget))
    {
      return;
    }

  /* remove every widget' parts, but the master widget */
  widgets = g_hash_table_get_values (ppriv->children);
  for (aux = widgets; aux != NULL; aux = g_list_next (aux))
    l = g_list_concat (l, g_list_copy (g_list_next (aux->data)));
  g_list_free (widgets);

  g_list_free_full (l, (GDestroyNotify) gtk_widget_destroy);

  /* clean overflow information */
  g_hash_table_remove_all (ppriv->overflow_cells);

  /* Allocate the widget */
  gtk_widget_set_allocation (widget, allocation);

  if (gtk_widget_get_realized (widget))
    gdk_window_move_resize (self->event_window, allocation->x, allocation->y, allocation->width, allocation->height);

  gtk_style_context_get (context,
                         gtk_style_context_get_state (context),
                         "font", &font_desc,
                         "padding-bottom", &padding_bottom,
                         NULL);

  layout = gtk_widget_create_pango_layout (widget, _("Other events"));
  pango_layout_set_font_description (layout, font_desc);
  pango_layout_get_pixel_size (layout, NULL, &font_height);
  pango_font_description_free (font_desc);
  g_object_unref (layout);

  start_grid_y = get_start_grid_y (widget);
  cell_width = allocation->width / 7.0;
  cell_height = (allocation->height - start_grid_y) / 6.0;
  vertical_cell_space = cell_height - (padding_bottom * 2 + font_height);

  for (i = 0; i < 42; i++)
    size_left[i] = vertical_cell_space;

  shown_rows = ceil ((self->days_delay + icaltime_days_in_month (self->date->month, self->date->year)) / 7.0);
  first_row_gap = (6 - shown_rows) * 0.5;
  sw = 1 - 2 * self->k;

  /*
   * Allocate multidays events before single day events, as they have a
   * higher priority. Multiday events have a master event, and clones
   * to represent the multiple sections of the event.
   */
  for (l = ppriv->multi_cell_children; l != NULL; l = g_list_next (l))
    {
      gint first_cell, last_cell, first_row, last_row, start, end;
      gboolean visible, all_day;

      GDateTime *date;
      GcalEvent *event;
      GArray *cells, *lengths;

      child_widget = (GtkWidget*) l->data;
      event = gcal_event_widget_get_event (l->data);
      uuid = gcal_event_get_uid (event);
      all_day = gcal_event_get_all_day (event);
      child_context = gtk_widget_get_style_context (l->data);

      if (!gtk_widget_is_visible (child_widget) && !g_hash_table_contains (ppriv->hidden_as_overflow, uuid))
        continue;

      gtk_widget_show (child_widget);
      gtk_widget_get_preferred_height (child_widget, NULL, &natural_height);

      /*
       * If the month of the event's start date is equal to the month
       * we're visualizing, the first cell is the event's day of the
       * month. Otherwise, the first cell is the 1st day of the month.
       */
      j = 1;
      date = gcal_event_get_date_start (event);
      date = all_day ? g_date_time_ref (date) : g_date_time_to_local (date);

      if (g_date_time_get_month (date) == self->date->month)
        j = g_date_time_get_day_of_month (date);

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

          visible = get_widget_parts (start, end, natural_height, vertical_cell_space, size_left, cells, lengths);
        }

      if (visible)
        {
          for (i = 0; i < cells->len; i++)
            {
              GDateTime *dt_start, *dt_end;
              gint cell_idx = g_array_index (cells, gint, i);
              gint length = g_array_index (lengths, gint, i);
              gint row = cell_idx / 7;
              gint column = cell_idx % 7;
              gint day = cell_idx - self->days_delay + 1;

              /* Day number is calculated differently on RTL languages */
              if (self->k)
                day = 7 * row + MIRROR (day % 7, 0, 7) - length;

              if (i != 0)
                {
                  child_widget = gcal_event_widget_clone (GCAL_EVENT_WIDGET (child_widget));

                  _gcal_subscriber_view_setup_child (GCAL_SUBSCRIBER_VIEW (widget), child_widget);
                  gtk_widget_show (child_widget);

                  child_context = gtk_widget_get_style_context (child_widget);

                  aux = g_hash_table_lookup (ppriv->children, uuid);
                  aux = g_list_append (aux, child_widget);
                }

              /* XXX: silence Gtk+ complaining about preferred size */
              gtk_widget_get_preferred_height (child_widget, NULL, &natural_height);

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

              pos_x = cell_width * column + margin.left;
              pos_y = cell_height * (row + first_row_gap) + start_grid_y + margin.top;

              child_allocation.x = pos_x;
              child_allocation.y = pos_y + vertical_cell_space - size_left[cell_idx];
              child_allocation.width = cell_width * length - (margin.left + margin.right);
              child_allocation.height = natural_height;

              gtk_widget_size_allocate (child_widget, &child_allocation);
              g_hash_table_remove (ppriv->hidden_as_overflow, uuid);

              /* update size_left */
              for (j = 0; j < length; j++)
                size_left[cell_idx + j] -= natural_height + margin.top + margin.bottom;

              g_clear_pointer (&dt_start, g_date_time_unref);
              g_clear_pointer (&dt_end, g_date_time_unref);
            }
        }
      else
        {
          gtk_widget_hide (child_widget);
          g_hash_table_add (ppriv->hidden_as_overflow, g_strdup (uuid));

          for (i = first_cell; i <= last_cell; i++)
            {
              aux = g_hash_table_lookup (ppriv->overflow_cells, GINT_TO_POINTER (i));
              aux = g_list_append (aux, child_widget);

              if (g_list_length (aux) == 1)
                g_hash_table_insert (ppriv->overflow_cells, GINT_TO_POINTER (i), aux);
              else
                g_hash_table_replace (ppriv->overflow_cells, GINT_TO_POINTER (i), g_list_copy (aux));
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
  g_hash_table_iter_init (&iter, ppriv->single_cell_children);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      j = GPOINTER_TO_INT (key);
      j += self->days_delay;
      i = 7 * ((j - 1) / 7)+ 6 * self->k + sw * ((j - 1) % 7);

      l = (GList*) value;
      for (aux = l; aux != NULL; aux = g_list_next (aux))
        {
          GcalEvent *event;

          child_widget = aux->data;
          event = gcal_event_widget_get_event (aux->data);
          uuid = gcal_event_get_uid (event);

          if (!gtk_widget_is_visible (child_widget) && !g_hash_table_contains (ppriv->hidden_as_overflow, uuid))
            continue;

          gtk_widget_show (child_widget);
          gtk_widget_get_preferred_height (child_widget, NULL, &natural_height);

          if (size_left[i] > natural_height)
            {
              child_context = gtk_widget_get_style_context (child_widget);

              gtk_style_context_get_margin (child_context, gtk_style_context_get_state (child_context), &margin);

              pos_x = cell_width * (i % 7) + margin.left;
              pos_y = cell_height * ((i / 7) + first_row_gap) + start_grid_y + margin.top;

              child_allocation.x = pos_x;
              child_allocation.y = pos_y + vertical_cell_space - size_left[i];
              child_allocation.width = cell_width - (margin.left + margin.right);
              child_allocation.height = natural_height;
              gtk_widget_show (child_widget);
              gtk_widget_size_allocate (child_widget, &child_allocation);
              g_hash_table_remove (ppriv->hidden_as_overflow, uuid);

              size_left[i] -= natural_height + margin.top + margin.bottom;
            }
          else
            {
              gtk_widget_hide (child_widget);
              g_hash_table_add (ppriv->hidden_as_overflow, g_strdup (uuid));

              l = g_hash_table_lookup (ppriv->overflow_cells, GINT_TO_POINTER (i));
              l = g_list_append (l, child_widget);

              if (g_list_length (l) == 1)
                g_hash_table_insert (ppriv->overflow_cells, GINT_TO_POINTER (i), l);
              else
                g_hash_table_replace (ppriv->overflow_cells, GINT_TO_POINTER (i), g_list_copy (l));
            }
        }
    }

  if (g_hash_table_size (ppriv->overflow_cells) != 0)
    gtk_widget_queue_draw (widget);

  ppriv->children_changed = FALSE;
}

static gboolean
gcal_month_view_draw (GtkWidget *widget,
                      cairo_t   *cr)
{
  GcalSubscriberViewPrivate *ppriv;
  GcalMonthView *self;

  GtkStyleContext *context;
  GtkStateFlags state;

  GtkBorder padding;
  GtkAllocation alloc;

  GdkRGBA color;

  gchar *header_str;
  PangoLayout *layout;
  PangoFontDescription *font_desc, *sfont_desc;

  g_autoptr (GDateTime) today;

  gint font_width, font_height, pos_x, pos_y, shown_rows;
  gint i, j, sw, lower_mark = 43, upper_mark = -2;
  gint cell_width, cell_height;
  gdouble start_grid_y, first_row_gap = 0.0;
  gdouble days;

  self = GCAL_MONTH_VIEW (widget);
  ppriv = GCAL_SUBSCRIBER_VIEW (widget)->priv;

  today = g_date_time_new_now_local ();

  /* fonts and colors selection */
  context = gtk_widget_get_style_context (widget);
  state = gtk_style_context_get_state (context);

  /* Get font description */
  gtk_style_context_save (context);
  gtk_style_context_set_state (context, state | GTK_STATE_FLAG_SELECTED);
  gtk_style_context_get (context, state | GTK_STATE_FLAG_SELECTED, "font", &sfont_desc, NULL);
  gtk_style_context_restore (context);

  gtk_widget_get_allocation (widget, &alloc);
  start_grid_y = get_start_grid_y (widget);
  cell_width = alloc.width / 7.0;
  cell_height = (alloc.height - start_grid_y) / 6.0;

  layout = gtk_widget_create_pango_layout (widget, NULL);

  /* calculations */
  days = self->days_delay + icaltime_days_in_month (self->date->month, self->date->year);
  shown_rows = ceil (days / 7.0);

  first_row_gap = (6 - shown_rows) * 0.5; /* invalid area before the actual rows */
  sw = 1 - 2 * self->k;

  /* active cells */
  if (self->start_mark_cell && self->end_mark_cell)
    {
      gint start_year, end_year, start_month, end_month;

      upper_mark = g_date_time_get_day_of_month(self->end_mark_cell);

      start_year = g_date_time_get_year (self->start_mark_cell);
      end_year = g_date_time_get_year (self->end_mark_cell);
      start_month = g_date_time_get_month (self->start_mark_cell);
      end_month = g_date_time_get_month (self->end_mark_cell);

      if (start_year == end_year && start_month == end_month)
        {
          lower_mark = g_date_time_get_day_of_month(self->start_mark_cell);
        }
      else
        {
          if (start_year != end_year)
            lower_mark = start_year < end_year ? 1 : icaltime_days_in_month (end_month, end_year);
          else
            lower_mark = start_month < end_month ? 1 : icaltime_days_in_month (end_month, end_year);
        }

      if (lower_mark > upper_mark)
        {
          gint cell;
          cell = lower_mark;
          lower_mark = upper_mark;
          upper_mark = cell;
        }
    }

  /* view headers */
  gtk_style_context_save (context);
  gtk_style_context_add_class (context, "start-header");

  gtk_style_context_get (context, state, "font", &font_desc, NULL);
  gtk_style_context_get_padding (context, state, &padding);

  header_str = g_strdup_printf ("%s", gcal_get_month_name (self->date->month - 1));
  pango_layout_set_text (layout, header_str, -1);
  pango_layout_set_font_description (layout, font_desc);
  pango_layout_get_pixel_size (layout, &font_width, NULL);

  gtk_render_layout (context, cr, self->k * (alloc.width - font_width) + sw * padding.left, padding.top, layout);

  pango_font_description_free (font_desc);
  g_free (header_str);
  gtk_style_context_restore (context);

  gtk_style_context_save (context);
  gtk_style_context_add_class (context, "end-header");

  gtk_style_context_get (context, state, "font", &font_desc, NULL);
  gtk_style_context_get_padding (context, state, &padding);

  header_str = g_strdup_printf ("%d", self->date->year);
  pango_layout_set_text (layout, header_str, -1);
  pango_layout_set_font_description (layout, font_desc);
  pango_layout_get_pixel_size (layout, &font_width, NULL);

  gtk_render_layout (context, cr, (1 - self->k) * (alloc.width - font_width) - sw * padding.left, padding.top, layout);

  pango_font_description_free (font_desc);
  g_free (header_str);
  gtk_style_context_restore (context);

  /*same padding for the rest of the view */
  gtk_style_context_get_padding (context, state, &padding);

  /* grid header */
  gtk_style_context_save (context);
  gtk_style_context_add_class (context, "header");

  gtk_style_context_get (context, state, "font", &font_desc, NULL);
  pango_layout_set_font_description (layout, font_desc);
  for (i = 0; i < 7; i++)
    {
      gchar *upcased_day;

      j = 6 * self->k + sw * i;
      upcased_day = g_utf8_strup (gcal_get_weekday ((j + self->first_weekday) % 7), -1);
      pango_layout_set_text (layout, upcased_day, -1);
      pango_layout_get_pixel_size (layout, &font_width, &font_height);

      gtk_render_layout (context, cr,
                         cell_width * (i + self->k) + (sw * padding.left) - self->k * font_width,
                         start_grid_y - padding.bottom - font_height,
                         layout);
      g_free (upcased_day);
    }

  pango_font_description_free (font_desc);
  gtk_style_context_restore (context);

  /* Drag and Drop highlight */
  if (self->dnd_cell != -1)
    {
      gtk_render_background (context,
                             cr,
                             floor (cell_width * real_cell (self->dnd_cell % 7, self->k)),
                             floor (cell_height * (self->dnd_cell / 7 + first_row_gap) + start_grid_y),
                             cell_width + 1,
                             cell_height + 1);
    }

  /* grid numbers */
  gtk_style_context_get (context, state, "font", &font_desc, NULL);
  pango_layout_set_font_description (layout, font_desc);
  for (i = 0; i < 7 * shown_rows; i++)
    {
      gchar *nr_day;
      gint column = i % 7;
      gint row = i / 7;
      gint real_i = real_cell (i, self->k);

      j = 7 * ((i + 7 * self->k) / 7) + sw * (i % 7) + (1 - self->k);
      if (j <= self->days_delay)
        continue;
      else if (j > days)
        continue;
      j -= self->days_delay;

      nr_day = g_strdup_printf ("%d", j);

      if (g_hash_table_contains (ppriv->overflow_cells, GINT_TO_POINTER (real_i)))
        {
          GList *l;
          PangoLayout *overflow_layout;
          PangoFontDescription *ofont_desc;
          gchar *overflow_str;
          gdouble y_value;

          l = g_hash_table_lookup (ppriv->overflow_cells, GINT_TO_POINTER (real_i));

          /* TODO: Warning: in some languages this string can be too long and may overlap with the number */
          overflow_str = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE, "Other event", "Other %d events", g_list_length (l)),
                                          g_list_length (l));

          gtk_style_context_save (context);
          gtk_style_context_add_class (context, "overflow");
          if (self->hovered_overflow_indicator == real_i)
            {
              gtk_style_context_set_state (context, GTK_STATE_FLAG_PRELIGHT);
              gtk_style_context_get (context, GTK_STATE_FLAG_PRELIGHT, "font", &ofont_desc, NULL);
            }
          else
            {
              gtk_style_context_get (context, state, "font", &ofont_desc, NULL);
            }

          overflow_layout = gtk_widget_create_pango_layout (widget, overflow_str);

          pango_layout_set_font_description (overflow_layout, ofont_desc);
          pango_layout_set_width (overflow_layout, pango_units_from_double (cell_width));
          pango_layout_set_alignment (overflow_layout, PANGO_ALIGN_CENTER);
          pango_layout_get_pixel_size (overflow_layout, &font_width, &font_height);
          y_value = cell_height * (row + 1 + first_row_gap) - font_height - padding.bottom + start_grid_y;

          if ((!gtk_widget_is_visible (self->overflow_popover) && self->hovered_overflow_indicator == real_i) ||
               gtk_widget_is_visible (self->overflow_popover))
            {
              gtk_render_background (context, cr, cell_width * column, y_value - padding.bottom,
                                     cell_width, font_height + padding.bottom * 2);
            }

          gtk_render_layout (context, cr, cell_width * column, y_value, overflow_layout);

          gtk_style_context_remove_class (context, "overflow");
          gtk_style_context_restore (context);
          g_free (overflow_str);
          pango_font_description_free (ofont_desc);
          g_object_unref (overflow_layout);
        }

      if (self->date->year == g_date_time_get_year (today) &&
          self->date->month == g_date_time_get_month (today) &&
          j == g_date_time_get_day_of_month (today))
        {
          PangoFontDescription *cfont_desc;
          PangoLayout *clayout;

          gtk_style_context_save (context);
          gtk_style_context_add_class (context, "current");

          /*
           * If the current day is not under a DnD, it's actually not possible
           * to *not* show the DnD indicator using only CSS. So we have to fake
           * the "I'm not under DnD" state.
           */
          if (j != self->dnd_cell)
            gtk_style_context_set_state (context, state & ~GTK_STATE_FLAG_DROP_ACTIVE);

          clayout = gtk_widget_create_pango_layout (widget, nr_day);
          gtk_style_context_get (context, state, "font", &cfont_desc, NULL);
          pango_layout_set_font_description (clayout, cfont_desc);
          pango_layout_get_pixel_size (clayout, &font_width, &font_height);

          /* FIXME: hardcoded padding of the number background */
          gtk_render_background (context, cr,
                                 cell_width * (column - self->k),
                                 cell_height * (row + first_row_gap) + start_grid_y,
                                 cell_width, cell_height);
          gtk_render_layout (context, cr,
                             cell_width * (column + 1 - self->k) - sw * padding.right + (self->k - 1) * font_width,
                             cell_height * (row + 1 + first_row_gap) - font_height - padding.bottom + start_grid_y,
                             clayout);

          gtk_style_context_restore (context);
          pango_font_description_free (cfont_desc);
          g_object_unref (clayout);
        }
      else if (lower_mark <= j && j <= upper_mark)
        {
          gtk_style_context_save (context);
          gtk_style_context_set_state (context, GTK_STATE_FLAG_SELECTED);

          pango_layout_set_text (layout, nr_day, -1);
          pango_layout_set_font_description (layout, sfont_desc);
          pango_layout_get_pixel_size (layout, &font_width, &font_height);

          gtk_render_layout (context, cr,
                             cell_width * (column + 1 - self->k) - sw * padding.right + (self->k - 1) * font_width,
                             cell_height * (row + 1 + first_row_gap) - font_height - padding.bottom + start_grid_y,
                             layout);

          gtk_style_context_restore (context);
          pango_layout_set_font_description (layout, font_desc);
        }
      else
        {
          pango_layout_set_text (layout, nr_day, -1);
          pango_layout_get_pixel_size (layout, &font_width, &font_height);

          gtk_render_layout (context, cr,
                             cell_width * (column + 1 - self->k) - sw * padding.right + (self->k - 1) * font_width,
                             cell_height * (row + 1 + first_row_gap) - font_height - padding.bottom + start_grid_y,
                             layout);
        }

      g_free (nr_day);
    }
  pango_font_description_free (sfont_desc);
  pango_font_description_free (font_desc);

  /* lines */
  gtk_style_context_save (context);
  gtk_style_context_add_class (context, "lines");

  gtk_style_context_get_color (context, state, &color);
  cairo_set_line_width (cr, LINE_WIDTH);
  gdk_cairo_set_source_rgba (cr, &color);
  /* vertical lines, the easy ones */
  for (i = 0; i < 6; i++)
    {
      pos_x = cell_width * (i + 1);
      cairo_move_to (cr, pos_x + (LINE_WIDTH / 2), start_grid_y);
      cairo_rel_line_to (cr, 0, alloc.height - start_grid_y);
    }

  /* top horizontal line */
  cairo_move_to (cr, 0, start_grid_y + (LINE_WIDTH / 2));
  cairo_rel_line_to (cr, alloc.width, 0);

  /* drawing weeks lines */
  for (i = 0; i < (shown_rows % 2) + 5; i++)
    {
      pos_y = cell_height * (i + 0.5 * (2.0 - (shown_rows % 2))) + start_grid_y;
      cairo_move_to (cr, 0, pos_y + (LINE_WIDTH / 2));
      cairo_rel_line_to (cr, alloc.width, 0);
    }
  cairo_stroke (cr);
  gtk_style_context_restore (context);

  /* selection borders */
  if (self->start_mark_cell && self->end_mark_cell)
   {
     gint first_mark, last_mark;
     gint pos_x2, pos_y2;
     gint start_year, end_year, start_month, end_month;

     start_year = g_date_time_get_year (self->start_mark_cell);
     end_year = g_date_time_get_year (self->end_mark_cell);
     start_month = g_date_time_get_month (self->start_mark_cell);
     end_month = g_date_time_get_month (self->end_mark_cell);

     last_mark = real_cell (g_date_time_get_day_of_month (self->end_mark_cell) + self->days_delay - 1, self->k);

     if (start_year == end_year && start_month == end_month)
      {
        first_mark = real_cell (g_date_time_get_day_of_month (self->start_mark_cell) + self->days_delay - 1, self->k);
      }
     else
      {
        gint start_cell, end_cell;

        start_cell = self->days_delay;
        end_cell = icaltime_days_in_month (end_month, end_year) + self->days_delay - 1;

        if (start_year != end_year)
          first_mark = start_year < end_year ? real_cell (start_cell, self->k) : real_cell(end_cell, self->k);
        else
          first_mark = start_month < end_month ? real_cell (start_cell, self->k) : real_cell (end_cell, self->k);
      }

     if (first_mark > last_mark)
       {
         gint tmp = first_mark;
         first_mark = last_mark;
         last_mark = tmp;
       }

     /* Retrieve the selection color */
     gtk_style_context_save (context);
     gtk_style_context_set_state (context, state | GTK_STATE_FLAG_SELECTED);
     gtk_style_context_get_color (context, state | GTK_STATE_FLAG_SELECTED, &color);
     gtk_style_context_restore (context);

     gdk_cairo_set_source_rgba (cr, &color);
     cairo_set_line_width (cr, LINE_WIDTH);

     if ((first_mark / 7) == (last_mark / 7))
       {
         /* selection in 1 row with 1 rectangular outline */
         gint row = first_mark / 7;
         gint first_column = first_mark % 7;
         gint last_column = last_mark % 7;

         pos_x = cell_width * (first_column);
         pos_x2 = cell_width * (last_column + 1);
         pos_y = cell_height * (row + first_row_gap) + start_grid_y;
         pos_y2 = cell_height * (row + 1.0 + first_row_gap) + start_grid_y;
         cairo_rectangle (cr, pos_x + (LINE_WIDTH / 2), pos_y + (LINE_WIDTH / 2),
                          pos_x2 - pos_x + (LINE_WIDTH / 2), pos_y2 - pos_y + (LINE_WIDTH / 2));
       }
     else if ((first_mark / 7) == ((last_mark / 7) - 1) && (first_mark % 7) > ((last_mark % 7) + 1))
       {
         /* selection in 2 rows with 2 seperate rectangular outlines */
         gint first_row = first_mark / 7;
         gint last_row = last_mark / 7;
         gint first_column = first_mark % 7;
         gint last_column = last_mark % 7;
         gdouble end;

         for (i = first_row; i <= last_row; i++)
           {
             if (i == first_row)
               {
                 pos_x = cell_width * (first_column + self->k);
                 end = (alloc.width * (1 - self->k)) - pos_x;
               }
             else if (i == last_row)
               {
                 pos_x = cell_width * (last_column + 1 - self->k);
                 end = (alloc.width * self->k) - pos_x;
               }
             else
               {
                 pos_x = 0;
                 end = alloc.width;
               }

             pos_y = cell_height * (i + first_row_gap) + start_grid_y;
             pos_y2 = cell_height * (i + 1.0 + first_row_gap) + start_grid_y;
             cairo_rectangle (cr, pos_x + (LINE_WIDTH / 2), pos_y + (LINE_WIDTH / 2),
                              (gint)end + (LINE_WIDTH / 2), pos_y2 - pos_y + (LINE_WIDTH / 2));
           }
       }
     else
       {
         /* multi-row selection with a single outline */
         gint first_row = first_mark / 7;
         gint last_row = last_mark / 7;
         gint first_column = first_mark % 7;
         gint last_column = last_mark % 7;
         gdouble start_x = (cell_width * first_column) + (LINE_WIDTH / 2);
         gdouble start_y = (cell_height * (first_row + first_row_gap)) + start_grid_y + (LINE_WIDTH / 2);
         gdouble end_x = (cell_width * (last_column + 1.0)) + (LINE_WIDTH / 2);
         gdouble end_y = (cell_height * (last_row + first_row_gap + 1.0)) + start_grid_y + (LINE_WIDTH / 2);
         gdouble max_x = alloc.width - (LINE_WIDTH / 2);

         /* Draw the irregular shaped selection box starting from the
          * top-left corner of the start date (start_{x,y}) clock-wise
          * to the right-bottom corner of the end date (end_{x,y}) and
          * on finishing at the start date again.
          */
         cairo_move_to (cr, start_x, start_y);
         cairo_line_to (cr, max_x, start_y);

         /* Skip intermediate drawing steps if end_{x,y} are at the
          * last column in the row, this makes sure we always draw the
          * outline within the visible drawing area.
          */
         if (last_column == 6)
           {
             cairo_line_to (cr, max_x, end_y);
           }
         else
           {
             cairo_line_to (cr, max_x, end_y - cell_height);
             cairo_line_to (cr, end_x, end_y - cell_height);
             cairo_line_to (cr, end_x, end_y);
           }

         cairo_line_to (cr, (LINE_WIDTH / 2), end_y);
         cairo_line_to (cr, (LINE_WIDTH / 2), start_y + cell_height);
         cairo_line_to (cr, start_x, start_y + cell_height);
         cairo_line_to (cr, start_x, start_y);
       }

     cairo_stroke_preserve (cr);
     /* selection background fill */
     cairo_set_source_rgba(cr, color.red, color.green, color.blue, 0.1);
     cairo_fill(cr);
  }

  /* Draw focus rectangle */
  if (self->keyboard_cell > self->days_delay - 1)
    {
      gint localized_cell;
      gint x;
      gint y;

      localized_cell = real_cell (self->keyboard_cell % 7, self->k);

      x = cell_width * (localized_cell);
      y = cell_height * (self->keyboard_cell / 7 + first_row_gap) + start_grid_y;

      gtk_render_focus (context, cr, x, y, cell_width, cell_height);
    }

  g_object_unref (layout);

  if (GTK_WIDGET_CLASS (gcal_month_view_parent_class)->draw != NULL)
    GTK_WIDGET_CLASS (gcal_month_view_parent_class)->draw (widget, cr);

  return FALSE;
}

static gboolean
gcal_month_view_button_press (GtkWidget      *widget,
                              GdkEventButton *event)
{
  GcalSubscriberViewPrivate *ppriv;
  GcalMonthView *self;

  gint days, clicked_cell;
  gboolean pressed_indicator = FALSE;

  self = GCAL_MONTH_VIEW (widget);
  ppriv = GCAL_SUBSCRIBER_VIEW (widget)->priv;

  days = self->days_delay + icaltime_days_in_month (self->date->month, self->date->year);

  clicked_cell = gather_button_event_data (GCAL_MONTH_VIEW (widget), event->x, event->y,
                                                 &pressed_indicator, NULL, NULL);

  clicked_cell = real_cell (clicked_cell, self->k);

  if (clicked_cell >= self->days_delay && clicked_cell < days)
    {
      g_clear_pointer (&self->start_mark_cell, g_date_time_unref);

      self->keyboard_cell = clicked_cell;
      self->start_mark_cell = g_date_time_new_local (self->date->year, self->date->month,
                                                     self->keyboard_cell - self->days_delay + 1,
                                                     0, 0, 0);
      gtk_widget_queue_draw (widget);
    }

  if (pressed_indicator && g_hash_table_contains (ppriv->overflow_cells, GINT_TO_POINTER (clicked_cell)))
    self->pressed_overflow_indicator = clicked_cell;

  return TRUE;
}

/**
 * gcal_month_view_motion_notify_event:
 * @widget:
 * @event:
 *
 * Update self->end_cell_mark in order to update the drawing
 * belonging to the days involved in the event creation
 *
 * Returns:
 **/
static gboolean
gcal_month_view_motion_notify_event (GtkWidget      *widget,
                                     GdkEventMotion *event)
{
  GcalMonthView *self;

  gint days;
  gint new_end_cell, new_hovered_cell;
  gboolean hovered_indicator = FALSE;

  self = GCAL_MONTH_VIEW (widget);

  days = self->days_delay + icaltime_days_in_month (self->date->month, self->date->year);

  new_end_cell = gather_button_event_data (GCAL_MONTH_VIEW (widget), event->x, event->y, &hovered_indicator, NULL, NULL);
  new_end_cell = real_cell (new_end_cell, self->k);

  if (self->start_mark_cell)
    {
      if (!(event->state & GDK_BUTTON_PRESS_MASK))
        return TRUE;

      if (new_end_cell >= self->days_delay && new_end_cell < days)
        {
          gint previous_end_cell;

          previous_end_cell = new_end_cell;

          /* Let the keyboard focus track the pointer */
          self->keyboard_cell = new_end_cell;

          /*
           * When there is a previously set end mark, it should be
           * cleared before assigning a new one.
           */
          if (self->end_mark_cell)
            {
              previous_end_cell = g_date_time_get_day_of_month (self->end_mark_cell) - 1;

              g_clear_pointer (&self->end_mark_cell, g_date_time_unref);
            }

          if (self->start_mark_cell &&
              self->end_mark_cell &&
              !g_date_time_equal (self->start_mark_cell, self->end_mark_cell))
            {
              self->hovered_overflow_indicator = -1;
            }

          if (previous_end_cell != new_end_cell)
            gtk_widget_queue_draw (widget);

          self->end_mark_cell = g_date_time_new_local (self->date->year,
                                                       self->date->month,
                                                       new_end_cell - self->days_delay + 1,
                                                       0, 0, 0);

          return TRUE;
        }
    }
  else
    {
      if (gtk_widget_is_visible (self->overflow_popover))
        return FALSE;

      if (hovered_indicator)
        new_hovered_cell = new_end_cell;
      else
        new_hovered_cell = -1;

      if (self->hovered_overflow_indicator != new_hovered_cell)
        {
          self->hovered_overflow_indicator = new_hovered_cell;
          gtk_widget_queue_draw (widget);
          return TRUE;
        }
    }

  return FALSE;
}

static gboolean
gcal_month_view_button_release (GtkWidget      *widget,
                                GdkEventButton *event)
{
  GcalMonthView *self;
  gdouble x, y;
  gint days, current_day;
  gboolean released_indicator = FALSE;

  self = GCAL_MONTH_VIEW (widget);
  days = self->days_delay + icaltime_days_in_month (self->date->month, self->date->year);

  current_day = gather_button_event_data (GCAL_MONTH_VIEW (widget), event->x, event->y, &released_indicator, &x, &y);
  current_day = real_cell (current_day, self->k);

  if (current_day >= self->days_delay && current_day < days)
    {
      gboolean valid;

      g_clear_pointer (&self->end_mark_cell, g_date_time_unref);

      self->keyboard_cell = current_day;
      self->end_mark_cell = g_date_time_new_local (self->date->year, self->date->month, current_day - self->days_delay + 1, 0, 0, 0);

      self->date->day = g_date_time_get_day_of_month (self->end_mark_cell);

      /* First, make sure to show the popover */
      valid = show_popover_for_position (GCAL_MONTH_VIEW (widget), x, y, released_indicator);

      /* Then update the active date */
      g_object_notify (G_OBJECT (self), "active-date");

      return valid;
    }
  else
    {
      /* If the button is released over an invalid cell, entirely cancel the selection */
      cancel_selection (GCAL_MONTH_VIEW (widget));

      gtk_widget_queue_draw (widget);

      return FALSE;
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

static guint
gcal_month_view_get_child_cell (GcalSubscriberView *subscriber,
                                GcalEventWidget    *child)
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
gcal_month_view_clear_state (GcalSubscriberView *subscriber)
{
  GcalMonthView *self = GCAL_MONTH_VIEW (subscriber);

  cancel_selection (GCAL_MONTH_VIEW (subscriber));

  self->pressed_overflow_indicator = -1;

  gtk_widget_hide (self->overflow_popover);
}

static void
gcal_month_view_class_init (GcalMonthViewClass *klass)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GcalSubscriberViewClass *subscriber_view_class;

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
  widget_class->draw = gcal_month_view_draw;
  widget_class->button_press_event = gcal_month_view_button_press;
  widget_class->motion_notify_event = gcal_month_view_motion_notify_event;
  widget_class->button_release_event = gcal_month_view_button_release;
  widget_class->direction_changed = gcal_month_view_direction_changed;
  widget_class->key_press_event = gcal_month_view_key_press;
  widget_class->drag_motion = gcal_month_view_drag_motion;
  widget_class->drag_drop = gcal_month_view_drag_drop;
  widget_class->drag_leave = gcal_month_view_drag_leave;
  widget_class->scroll_event = gcal_month_view_scroll_event;

  subscriber_view_class = GCAL_SUBSCRIBER_VIEW_CLASS (klass);
  subscriber_view_class->get_child_cell = gcal_month_view_get_child_cell;
  subscriber_view_class->clear_state = gcal_month_view_clear_state;

  g_object_class_override_property (object_class, PROP_DATE, "active-date");

  gtk_widget_class_set_css_name (widget_class, "calendar-view");
}

static void
gcal_month_view_init (GcalMonthView *self)
{
  GtkWidget *grid;
  GtkWidget *button;

  gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);

  self = gcal_month_view_get_instance_private (self);

  cancel_selection (self);

  self->pressed_overflow_indicator = -1;
  self->hovered_overflow_indicator = -1;

  self->k = gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL;

  self->overflow_popover = gtk_popover_new (GTK_WIDGET (self));
  gtk_style_context_add_class (gtk_widget_get_style_context (self->overflow_popover), "events");
  g_signal_connect_swapped (self->overflow_popover, "hide", G_CALLBACK (overflow_popover_hide), self);
  g_signal_connect_swapped (self->overflow_popover,
                            "drag-motion",
                            G_CALLBACK (cancel_dnd_from_overflow_popover),
                            self->overflow_popover);

  grid = gtk_grid_new ();
  g_object_set (grid, "row-spacing", 6, "orientation", GTK_ORIENTATION_VERTICAL, NULL);
  gtk_container_add (GTK_CONTAINER (self->overflow_popover), grid);

  self->popover_title = gtk_label_new (NULL);
  gtk_style_context_add_class (gtk_widget_get_style_context (self->popover_title), "sidebar-header");
  g_object_set (self->popover_title, "margin", 6, "halign", GTK_ALIGN_START, NULL);
  self->events_list_box = gtk_list_box_new ();
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (self->events_list_box), GTK_SELECTION_NONE);
  gtk_list_box_set_header_func (GTK_LIST_BOX (self->events_list_box), update_list_box_headers, self, NULL);
  button = gtk_button_new_with_label (_("Add Event…"));
  g_object_set (button, "hexpand", TRUE, NULL);
  g_signal_connect (button, "clicked", G_CALLBACK (add_new_event_button_cb), self);

  gtk_container_add (GTK_CONTAINER (grid), self->popover_title);
  gtk_container_add (GTK_CONTAINER (grid), self->events_list_box);
  gtk_container_add (GTK_CONTAINER (grid), button);

  /* Setup the month view as a drag n' drop destination */
  gtk_drag_dest_set (GTK_WIDGET (self),
                     0,
                     NULL,
                     0,
                     GDK_ACTION_MOVE);

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
}

/* Public API */

/**
 * gcal_month_view_set_first_weekday:
 * @view: A #GcalMonthView instance
 * @day_nr: Integer representing the first day of the week
 *
 * Set the first day of the week according to the locale, being
 * 0 for Sunday, 1 for Monday and so on.
 **/
void
gcal_month_view_set_first_weekday (GcalMonthView *self,
                                   gint           day_nr)
{
  g_return_if_fail (GCAL_IS_MONTH_VIEW (self));

  self->first_weekday = day_nr;

  /* update days_delay */
  if (self->date)
    self->days_delay = (time_day_of_week (1, self->date->month - 1, self->date->year) - self->first_weekday + 7) % 7;
}

/**
 * gcal_month_view_set_use_24h_format:
 * @view:
 * @use_24h:
 *
 * Whether the view will show time using 24h or 12h format
 **/
void
gcal_month_view_set_use_24h_format (GcalMonthView *self,
                                    gboolean       use_24h)
{
  self->use_24h_format = use_24h;
}

void
gcal_month_view_set_manager (GcalMonthView *self,
                             GcalManager   *manager)
{
  g_return_if_fail (GCAL_IS_MONTH_VIEW (self));

  self->manager = manager;
}
