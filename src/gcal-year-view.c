/* -*- mode: c; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* gcal-year-view.c
 *
 * Copyright (C) 2015 Erick Pérez Castellanos <erick.red@gmail.com>
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

#include "gcal-year-view.h"
#include "gcal-view.h"
#include "gcal-utils.h"

#include <glib/gi18n.h>
#include <math.h>
#include <string.h>

#define NAVIGATOR_CELL_WIDTH (210 + 15)
#define NAVIGATOR_CELL_HEIGHT 210
#define SIDEBAR_PREFERRED_WIDTH 200
#define VISUAL_CLUES_SIDE 3.0

typedef struct
{
  /* month span from 0 to 11 */
  gint start_day, start_month;
  gint end_day, end_month;
} ButtonData;

typedef struct
{
  gdouble  box_side;
  GdkPoint coordinates [12];
} GridData;

struct _GcalYearView
{
  GtkBox        parent;

  /* composite, GtkBuilder's widgets */
  GtkWidget    *navigator;
  GtkWidget    *sidebar;
  GtkWidget    *events_sidebar;
  GtkWidget    *navigator_stack;
  GtkWidget    *no_events_title;
  GtkWidget    *navigator_sidebar;

  GtkWidget    *popover; /* Popover for popover_mode */

  /* manager singleton */
  GcalManager  *manager;

  /* range shown on the sidebar */
  icaltimetype *start_selected_date;
  icaltimetype *end_selected_date;

  /* geometry info */
  GridData     *navigator_grid;

  /* state flags */
  gboolean      popover_mode;
  gboolean      button_pressed;
  ButtonData   *selected_data;

  /**
   * first day of the week according to user locale, being
   * 0 for Sunday, 1 for Monday and so on */
  gint          first_weekday;

  /* first and last weeks of the year */
  guint         first_week_of_year;
  guint         last_week_of_year;

    /**
   * clock format from GNOME desktop settings
   */
  gboolean      use_24h_format;

  /* text direction factors */
  gint          k;

  /* weak ref to current date */
  icaltimetype *current_date;

  /* date property */
  icaltimetype *date;
};

enum {
  PROP_0,
  PROP_DATE,
  LAST_PROP
};

enum
{
  EVENT_ACTIVATED,
  NUM_SIGNALS
};

static guint signals[NUM_SIGNALS] = { 0, };

static void   reset_sidebar (GcalYearView *year_view);
static void   gcal_view_interface_init (GcalViewIface *iface);
static void   gcal_data_model_subscriber_interface_init (ECalDataModelSubscriberInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GcalYearView, gcal_year_view, GTK_TYPE_BOX,
                         G_IMPLEMENT_INTERFACE (GCAL_TYPE_VIEW, gcal_view_interface_init)
                         G_IMPLEMENT_INTERFACE (E_TYPE_CAL_DATA_MODEL_SUBSCRIBER,
                                                gcal_data_model_subscriber_interface_init));

static guint
get_last_week_of_year_dmy (gint       first_weekday,
                           GDateDay   day,
                           GDateMonth month,
                           GDateYear  year)
{
  GDate day_of_year;
  gint day_of_week;

  g_date_set_dmy (&day_of_year, day, month, year);
  day_of_week = g_date_get_weekday (&day_of_year) % 7;

  if (day_of_week >= first_weekday)
    g_date_add_days (&day_of_year, (6 - day_of_week) + first_weekday);
  else
    g_date_add_days (&day_of_year, first_weekday - day_of_week - 1);

  return g_date_get_iso8601_week_of_year (&day_of_year);
}

static void
update_date (GcalYearView *year_view,
             icaltimetype *new_date)
{
  if (year_view->date != NULL && year_view->date->year != new_date->year && year_view->start_selected_date->day != 0)
    reset_sidebar (year_view);

  g_clear_pointer (&year_view->date, g_free);
  year_view->date = new_date;

  year_view->first_week_of_year = get_last_week_of_year_dmy (year_view->first_weekday,
                                                             1, G_DATE_JANUARY,  year_view->date->year);;
  year_view->last_week_of_year = get_last_week_of_year_dmy (year_view->first_weekday,
                                                            31, G_DATE_DECEMBER, year_view->date->year);
}

static void
event_activated (GcalEventWidget *widget,
                 gpointer         user_data)
{
  GcalYearView *view = GCAL_YEAR_VIEW (user_data);

  if (view->popover_mode)
    gtk_widget_hide (view->popover);
  g_signal_emit (GCAL_YEAR_VIEW (user_data), signals[EVENT_ACTIVATED], 0, widget);
}

static void
order_selected_data (ButtonData *selected_data)
{
  gint swap;
  if (selected_data->end_month < selected_data->start_month)
    {
      swap = selected_data->start_month;
      selected_data->start_month = selected_data->end_month;
      selected_data->end_month = swap;

      swap = selected_data->start_day;
      selected_data->start_day = selected_data->end_day;
      selected_data->end_day = swap;
    }
  else if (selected_data->start_month == selected_data->end_month && selected_data->end_day < selected_data->start_day)
    {
      swap = selected_data->start_day;
      selected_data->start_day = selected_data->end_day;
      selected_data->end_day = swap;
    }
}

static void
update_selected_dates_from_button_data (GcalYearView *year_view)
{
  if (year_view->selected_data->start_day != 0)
    {
      ButtonData selected_data = *(year_view->selected_data);
      order_selected_data (&selected_data);

      year_view->start_selected_date->day = selected_data.start_day;
      year_view->start_selected_date->month = selected_data.start_month + 1;
      year_view->start_selected_date->year = year_view->date->year;
      year_view->end_selected_date->is_date = TRUE;

      year_view->end_selected_date->day = selected_data.end_day;
      year_view->end_selected_date->month = selected_data.end_month + 1;
      year_view->end_selected_date->year = year_view->date->year;
      year_view->end_selected_date->hour = 23;
      year_view->end_selected_date->minute = 59;
      year_view->end_selected_date->is_date = TRUE;
      *(year_view->end_selected_date) = icaltime_normalize (*(year_view->end_selected_date));
    }
  else
    {
      *(year_view->start_selected_date) = *(year_view->current_date);
      year_view->start_selected_date->hour = 0;
      year_view->start_selected_date->minute = 0;
      year_view->start_selected_date->second = 0;

      *(year_view->end_selected_date) = *(year_view->current_date);
      year_view->end_selected_date->hour = 23;
      year_view->end_selected_date->minute = 59;
      *(year_view->end_selected_date) = icaltime_normalize (*(year_view->end_selected_date));
    }

  if (year_view->end_selected_date->year != year_view->start_selected_date->year)
    {
      year_view->end_selected_date->day = 31;
      year_view->end_selected_date->month = 12;
      year_view->end_selected_date->year = year_view->start_selected_date->year;
    }
}

static void
update_no_events_page (GcalYearView *year_view)
{
  gchar *title;
  gboolean has_range;

  has_range = (year_view->start_selected_date->day != year_view->end_selected_date->day ||
               year_view->start_selected_date->month != year_view->end_selected_date->month);

  if (icaltime_compare_date (year_view->current_date, year_view->start_selected_date) == 0)
    {
      title = g_strdup_printf ("%s%s", _("Today"), has_range ? "…" : "");
    }
  else
    {
      title = g_strdup_printf ("%s %d%s",
                               gcal_get_month_name (year_view->start_selected_date->month - 1),
                               year_view->start_selected_date->day,
                               has_range ? "…" : "");
    }

  gtk_label_set_text (GTK_LABEL (year_view->no_events_title), title);
  g_free (title);
}

static void
add_event_to_day_array (GcalYearView  *year_view,
                        GcalEvent     *event,
                        GList        **days_widgets_array,
                        gint           days_span)
{
  GtkWidget *child_widget;

  GDateTime *dt_start, *dt_end;
  GDateTime *date, *second_date;
  GTimeZone *tz;

  gint i;
  gboolean child_widget_used = FALSE;

  child_widget = gcal_event_widget_new (event);
  gcal_event_widget_set_read_only (GCAL_EVENT_WIDGET (child_widget),
                                   !gcal_manager_is_client_writable (year_view->manager, gcal_event_get_source (event)));

  dt_start = gcal_event_get_date_start (event);
  dt_end = gcal_event_get_date_end (event);

  /* normalize date on each new event */
  date = icaltime_to_datetime (year_view->start_selected_date, &tz);
  second_date = g_date_time_new (tz,
                                 g_date_time_get_year (date),
                                 g_date_time_get_month (date),
                                 g_date_time_get_day_of_month (date) + 1,
                                 0, 0, 0);

  gcal_event_widget_set_date_start (GCAL_EVENT_WIDGET (child_widget), date);
  gcal_event_widget_set_date_end (GCAL_EVENT_WIDGET (child_widget), second_date);

  /* marking and cloning */
  for (i = 0; i < days_span; i++)
    {
      GDateTime *aux;
      GtkWidget *cloned_child;
      gint start_comparison, end_comparison;

      cloned_child = child_widget;
      start_comparison = datetime_compare_date (dt_start, date);

      if (start_comparison <= 0)
        {
          if (child_widget_used)
            {
              cloned_child = gcal_event_widget_clone (GCAL_EVENT_WIDGET (child_widget));
              gcal_event_widget_set_date_start (GCAL_EVENT_WIDGET (cloned_child), date);
              gcal_event_widget_set_date_end (GCAL_EVENT_WIDGET (cloned_child), second_date);
            }
          else
            {
              child_widget_used = TRUE;
            }
        }
      else
        {
          cloned_child = NULL;
        }

      if (cloned_child != NULL)
        {
          days_widgets_array[i] = g_list_insert_sorted (days_widgets_array[i],
                                                        cloned_child,
                                                        (GCompareFunc) gcal_event_widget_compare_for_single_day);

          end_comparison = g_date_time_compare (second_date, dt_end);

          if (end_comparison == 0)
            break;
        }

      /* Move the compared dates to the next day */
      aux = date;
      date = g_date_time_add_days (date, 1);
      g_clear_pointer (&aux, g_date_time_unref);

      aux = second_date;
      second_date = g_date_time_add_days (second_date, 1);
      g_clear_pointer (&aux, g_date_time_unref);
    }
}

static void
update_sidebar (GcalYearView *year_view)
{
  GtkWidget *child_widget;
  GList *events, *l;
  GList **days_widgets_array;
  gint i, days_span;

  update_selected_dates_from_button_data (year_view);

  gtk_container_foreach (GTK_CONTAINER (year_view->events_sidebar), (GtkCallback) gtk_widget_destroy, NULL);

  days_span = icaltime_day_of_year(*(year_view->end_selected_date)) - icaltime_day_of_year(*(year_view->start_selected_date)) + 1;
  days_widgets_array = g_new0 (GList*, days_span);

  events = gcal_manager_get_events (year_view->manager, year_view->start_selected_date, year_view->end_selected_date);
  if (events == NULL)
    {
      days_span = 0;
      update_no_events_page (year_view);
      gtk_stack_set_visible_child_name (GTK_STACK (year_view->navigator_stack), "no-events");
    }
  else
    {
      gtk_stack_set_visible_child_name (GTK_STACK (year_view->navigator_stack), "events-list");
    }

  for (l = events; l != NULL; l = g_list_next (l))
    add_event_to_day_array (year_view, l->data, days_widgets_array, days_span);

  for (i = 0; i < days_span; i++)
    {
      GList *current_day = days_widgets_array[i];
      for (l = current_day; l != NULL; l = g_list_next (l))
        {
          child_widget = l->data;
          gtk_widget_show (child_widget);
          g_signal_connect (child_widget, "activate", G_CALLBACK (event_activated), year_view);
          g_object_set_data (G_OBJECT (child_widget), "shift", GINT_TO_POINTER (i));
          gtk_container_add (GTK_CONTAINER (year_view->events_sidebar), child_widget);
        }

      g_list_free (current_day);
    }

  g_list_free_full (events, g_object_unref);
  g_free (days_widgets_array);
}

static void
reset_sidebar (GcalYearView *year_view)
{
  memset (year_view->selected_data, 0, sizeof (ButtonData));
  gtk_widget_queue_draw (GTK_WIDGET (year_view));

  update_sidebar (year_view);
}

static void
update_sidebar_headers (GtkListBoxRow *row,
                        GtkListBoxRow *before,
                        gpointer user_data)
{
  GcalYearView *year_view;
  GtkWidget *row_child, *before_child = NULL, *row_header = NULL;
  GcalEvent *row_event, *before_event;
  GDateTime *row_date, *before_date = NULL;
  icaltimetype date;
  gint row_shift, before_shift =-1;

  year_view = GCAL_YEAR_VIEW (user_data);
  row_child = gtk_bin_get_child (GTK_BIN (row));

  if (row_child == NULL)
    return;

  row_event = gcal_event_widget_get_event (GCAL_EVENT_WIDGET (row_child));
  row_date = gcal_event_get_date_start (row_event);
  row_shift = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row_child), "shift"));

  if (before != NULL)
    {
      before_child = gtk_bin_get_child (GTK_BIN (before));
      before_event = gcal_event_widget_get_event (GCAL_EVENT_WIDGET (before_child));
      before_date = gcal_event_get_date_start (before_event);
      before_shift = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (before_child), "shift"));
    }

  if (before_shift == -1 || before_shift != row_shift)
    {
      GtkWidget *label;
      gchar *label_str;

      row_header = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      date = *(year_view->start_selected_date);
      icaltime_adjust (&date, row_shift, 0, 0, 0);

      if (icaltime_compare_date (&date, year_view->current_date) == 0)
        label_str = g_strdup (_("Today"));
      else
        label_str = g_strdup_printf ("%s %d", gcal_get_month_name (date.month  - 1), date.day);

      label = gtk_label_new (label_str);
      gtk_style_context_add_class (gtk_widget_get_style_context (label), "sidebar-header");
      g_object_set (label, "margin", 6, "halign", GTK_ALIGN_START, NULL);
      g_free (label_str);

      gtk_container_add (GTK_CONTAINER (row_header), label);
    }

  if (!gcal_event_is_multiday (row_event) &&
      !gcal_event_get_all_day (row_event) &&
      (before_date == NULL || g_date_time_get_hour (before_date) != g_date_time_get_hour (row_date)))
    {
      gchar *time;
      GtkWidget *label;
      gint hour;

      hour = g_date_time_get_hour (row_date);

      if (year_view->use_24h_format)
        time = g_strdup_printf ("%.2d:00", hour);
      else
        time = g_strdup_printf ("%.2d:00 %s", hour % 12, hour < 12 ? "AM" : "PM");

      label = gtk_label_new (time);
      gtk_style_context_add_class (gtk_widget_get_style_context (label), GTK_STYLE_CLASS_DIM_LABEL);
      g_object_set (label, "margin", 6, "halign", GTK_ALIGN_START, NULL);

      if (row_header == NULL)
        row_header = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

      gtk_container_add (GTK_CONTAINER (row_header), label);
      gtk_container_add (GTK_CONTAINER (row_header), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));

      g_free (time);
    }

  if (row_header != NULL)
    gtk_widget_show_all (row_header);
  gtk_list_box_row_set_header (row, row_header);
}

static gint
sidebar_sort_func (GtkListBoxRow *row1,
                   GtkListBoxRow *row2,
                   gpointer       user_data)
{
  GtkWidget *row1_child, *row2_child;
  gint result, row1_shift, row2_shift;

  row1_child = gtk_bin_get_child (GTK_BIN (row1));
  row2_child = gtk_bin_get_child (GTK_BIN (row2));
  row1_shift = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row1_child), "shift"));
  row2_shift = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row2_child), "shift"));

  result = row1_shift - row2_shift;
  if (result == 0)
    return gcal_event_widget_compare_for_single_day (GCAL_EVENT_WIDGET (row1_child), GCAL_EVENT_WIDGET (row2_child));

  return result;
}

static gboolean
calculate_day_month_for_coord (GcalYearView *year_view,
                               gdouble       x,
                               gdouble       y,
                               gint         *out_day,
                               gint         *out_month)
{
  gint row, column, i, sw, clicked_cell, day, month;
  gdouble box_side;

  row = -1;
  column = -1;
  box_side = year_view->navigator_grid->box_side;
  sw = 1 - 2 * year_view->k;

  /* y selection */
  for (i = 0; i < 4 && ((row == -1) || (column == -1)); i++)
    {
      if (row == -1 &&
          y > year_view->navigator_grid->coordinates[i * 4].y + box_side &&
          y < year_view->navigator_grid->coordinates[i * 4].y + box_side * 7)
        {
          row = i;
        }
      if (column == -1 &&
          x > year_view->navigator_grid->coordinates[i].x + box_side * 0.5 &&
          x < year_view->navigator_grid->coordinates[i].x + box_side * 7.5)
        {
          column = i;
        }
    }

  if (row == -1 || column == -1)
    return FALSE;

  month = row * 4 + column;
  row = (y - (year_view->navigator_grid->coordinates[month].y + box_side)) / box_side;
  column = (x - (year_view->navigator_grid->coordinates[month].x + box_side * 0.5)) / box_side;
  clicked_cell = row * 7 + column;
  day = 7 * ((clicked_cell + 7 * year_view->k) / 7) + sw * (clicked_cell % 7) + (1 - year_view->k);
  day -= ((time_day_of_week (1, month, year_view->date->year) - year_view->first_weekday + 7) % 7);

  if (day < 1 || day > time_days_in_month (year_view->date->year, month))
    return FALSE;

  *out_day = day;
  *out_month = month;
  return TRUE;
}

static void
draw_month_grid (GcalYearView *year_view,
                 GtkWidget    *widget,
                 cairo_t      *cr,
                 gint          month_nr,
                 gint         *weeks_counter)
{
  GtkStyleContext *context;
  GtkStateFlags state_flags;

  PangoLayout *layout, *slayout;
  PangoFontDescription *font_desc, *sfont_desc;

  GdkRGBA color;
  gint layout_width, layout_height, i, j, sw;
  gint column, row;
  gdouble x, y, box_side, box_padding_top, box_padding_start;
  gint days_delay, days, shown_rows, sunday_idx;
  gchar *str, *nr_day, *nr_week;
  gboolean selected_day;
  GList *events;
  icaltimetype start_date, end_date;

  cairo_save (cr);
  context = gtk_widget_get_style_context (widget);
  state_flags = gtk_style_context_get_state (context);
  sw = 1 - 2 * year_view->k;
  box_side = year_view->navigator_grid->box_side;
  x = year_view->navigator_grid->coordinates[month_nr].x;
  y = year_view->navigator_grid->coordinates[month_nr].y;

  /* Get the font description */
  gtk_style_context_save (context);
  gtk_style_context_set_state (context, state_flags | GTK_STATE_FLAG_SELECTED);
  gtk_style_context_get (context, state_flags | GTK_STATE_FLAG_SELECTED, "font", &sfont_desc, NULL);
  gtk_style_context_restore (context);

  slayout = gtk_widget_create_pango_layout (widget, NULL);
  pango_layout_set_font_description (slayout, sfont_desc);

  /* header */
  gtk_style_context_save (context);
  gtk_style_context_add_class (context, "header");

  str = g_strdup (gcal_get_month_name (month_nr));
  gtk_style_context_get (context, state_flags, "font", &font_desc, NULL);

  layout = gtk_widget_create_pango_layout (widget, str);
  pango_layout_set_font_description (layout, font_desc);
  pango_layout_get_pixel_size (layout, &layout_width, &layout_height);
  gtk_render_layout (context, cr, x + (box_side * 8 - layout_width) / 2.0, y + (box_side - layout_height) / 2.0,
                     layout);

  gtk_render_background (context, cr,
                         x + (box_side * 8 - layout_width) / 2.0, y + (box_side - layout_height) / 2.0,
                         layout_width, layout_width);

  pango_font_description_free (font_desc);
  g_free (str);
  gtk_style_context_restore (context);

  /* separator line */
  gtk_style_context_save (context);
  gtk_style_context_add_class (context, "lines");

  gtk_style_context_get_color (context, state_flags, &color);
  cairo_set_line_width (cr, 0.2);
  gdk_cairo_set_source_rgba (cr, &color);
  cairo_move_to (cr, x + box_side / 2, y + box_side + 0.4);
  cairo_rel_line_to (cr, 7.0 * box_side, 0);
  cairo_stroke (cr);

  gtk_style_context_restore (context);

  /* days */
  gtk_style_context_save (context);
  gtk_style_context_add_class (context, "days");

  gtk_style_context_get (context, state_flags, "font", &font_desc, NULL);
  pango_layout_set_font_description (layout, font_desc);

  days_delay = (time_day_of_week (1, month_nr, year_view->date->year) - year_view->first_weekday + 7) % 7;
  days = days_delay + icaltime_days_in_month (month_nr + 1, year_view->date->year);
  shown_rows = ceil (days / 7.0);
  sunday_idx = year_view->k * 6 + sw * ((7 - year_view->first_weekday) % 7);

  start_date.day    = 1;
  start_date.month  = month_nr + 1;
  start_date.year   = year_view->date->year;
  start_date.hour   = 0;
  start_date.minute = 0;
  start_date.second = 0;
  end_date.day    = 1;
  end_date.month  = month_nr + 1;
  end_date.year   = year_view->date->year;
  end_date.hour   = 23;
  end_date.minute = 59;
  end_date.second = 59;

  for (i = 0; i < 7 * shown_rows; i++)
    {
      column = i % 7;
      row = i / 7;

      j = 7 * ((i + 7 * year_view->k) / 7) + sw * (i % 7) + (1 - year_view->k);
      if (j <= days_delay)
        continue;
      else if (j > days)
        continue;
      j -= days_delay;

      nr_day = g_strdup_printf ("%d", j);
      pango_layout_set_text (layout, nr_day, -1);
      pango_layout_get_pixel_size (layout, &layout_width, &layout_height);
      box_padding_top = (box_side - layout_height) / 2 > 0 ? (box_side - layout_height) / 2 : 0;
      box_padding_start = (box_side - layout_width) / 2 > 0 ? (box_side - layout_width) / 2 : 0;

      selected_day = FALSE;
      if (year_view->selected_data->start_day != 0)
        {
          ButtonData selected_data = *(year_view->selected_data);
          order_selected_data (&selected_data);
          if (month_nr > selected_data.start_month && month_nr < selected_data.end_month)
            {
              selected_day = TRUE;
            }
          else if (month_nr == selected_data.start_month && month_nr == selected_data.end_month)
            {
              selected_day = j >= selected_data.start_day && j <= selected_data.end_day;
            }
          else if (month_nr == selected_data.start_month && j >= selected_data.start_day)
            {
              selected_day = TRUE;
            }
          else if (month_nr == selected_data.end_month && j <= selected_data.end_day)
            {
              selected_day = TRUE;
            }
        }

      if (year_view->date->year == year_view->current_date->year && month_nr + 1 == year_view->current_date->month &&
          j == year_view->current_date->day)
        {
          PangoLayout *clayout;
          PangoFontDescription *cfont_desc;

          gtk_style_context_save (context);
          gtk_style_context_add_class (context, "current");

          clayout = gtk_widget_create_pango_layout (widget, nr_day);
          gtk_style_context_get (context, state_flags, "font", &cfont_desc, NULL);
          pango_layout_set_font_description (clayout, cfont_desc);
          pango_layout_get_pixel_size (clayout, &layout_width, &layout_height);
          box_padding_top = (box_side - layout_height) / 2 > 0 ? (box_side - layout_height) / 2 : 0;
          box_padding_start = (box_side - layout_width) / 2 > 0 ? (box_side - layout_width) / 2 : 0;

          /* FIXME: hardcoded padding of the number background */
          gtk_render_background (context, cr,
                                 box_side * (column + 0.5 + year_view->k) + x + sw * box_padding_start - year_view->k * layout_width - 2.0,
                                 box_side * (row + 1) + y + box_padding_top - 1.0,
                                 layout_width + 4.0, layout_height + 2.0);
          gtk_render_layout (context, cr,
                             box_side * (column + 0.5 + year_view->k) + x + sw * box_padding_start - year_view->k * layout_width,
                             box_side * (row + 1) + y + box_padding_top,
                             clayout);

          gtk_style_context_restore (context);
          pango_font_description_free (cfont_desc);
          g_object_unref (clayout);
        }
      else if (selected_day)
        {
          gtk_style_context_set_state (context, state_flags | GTK_STATE_FLAG_SELECTED);

          pango_layout_set_text (slayout, nr_day, -1);
          pango_layout_get_pixel_size (slayout, &layout_width, &layout_height);
          box_padding_top = (box_side - layout_height) / 2 > 0 ? (box_side - layout_height) / 2 : 0;
          box_padding_start = (box_side - layout_width) / 2 > 0 ? (box_side - layout_width) / 2 : 0;

          gtk_render_layout (context, cr,
                             box_side * (column + 0.5 + year_view->k) + x + sw * box_padding_start - year_view->k * layout_width,
                             box_side * (row + 1) + y + box_padding_top,
                             slayout);

          gtk_style_context_set_state (context, state_flags);
        }
      else if (column == sunday_idx)
        {
          gtk_style_context_save (context);
          gtk_style_context_add_class (context, "sunday");
          gtk_render_layout (context, cr,
                             box_side * (column + 0.5 + year_view->k) + x + sw * box_padding_start - year_view->k * layout_width,
                             box_side * (row + 1) + y + box_padding_top,
                             layout);
          gtk_style_context_restore (context);
        }
      else
        {
          gtk_render_layout (context, cr,
                             box_side * (column + 0.5 + year_view->k) + x + sw * box_padding_start - year_view->k * layout_width,
                             box_side * (row + 1) + y + box_padding_top,
                             layout);
        }

      start_date.day = end_date.day = j;
      events = gcal_manager_get_events (year_view->manager, &start_date, &end_date);
      if (events != NULL)
        {
          gtk_style_context_save (context);
          gtk_style_context_add_class (context, "with-events");
          if (selected_day)
            gtk_style_context_add_class (context, "with-events-selected");
          else if (column == sunday_idx)
            gtk_style_context_add_class (context, "with-events-sunday");
          box_padding_start = (box_side - VISUAL_CLUES_SIDE) / 2 > 0 ? (box_side - VISUAL_CLUES_SIDE) / 2 : 0;
          gtk_render_background (context, cr,
                                 box_side * (column + 0.5 + year_view->k) + x + sw * box_padding_start - year_view->k * VISUAL_CLUES_SIDE,
                                 box_side * (row + 1) + y + box_padding_top + layout_height + 2.0,
                                 VISUAL_CLUES_SIDE, VISUAL_CLUES_SIDE);
          gtk_style_context_restore (context);
          g_list_free_full (events, g_object_unref);
        }

      g_free (nr_day);
    }
  pango_font_description_free (font_desc);
  gtk_style_context_restore (context);

  /* week numbers */
  gtk_style_context_save (context);
  gtk_style_context_add_class (context, "week-numbers");

  gtk_style_context_get (context, state_flags, "font", &font_desc, NULL);
  pango_layout_set_font_description (layout, font_desc);

  for (i = 0; i < shown_rows; i++)
    {
      /* special condition for first and last weeks of the year */
      if (month_nr == 0 && year_view->first_week_of_year > 1 && i == 1)
        *weeks_counter = 1;
      else if (month_nr == 11 && (i + 1) == shown_rows)
        *weeks_counter = year_view->last_week_of_year;
      /* other weeks */
      else if (i == 0)
        {
          if (days_delay == 0 && month_nr != 0)
            *weeks_counter = *weeks_counter + 1;
        }
      else
        *weeks_counter = *weeks_counter + 1;

      nr_week = g_strdup_printf ("%d", *weeks_counter);

      pango_layout_set_text (layout, nr_week, -1);
      pango_layout_get_pixel_size (layout, &layout_width, &layout_height);
      box_padding_top = (box_side - layout_height) / 2 > 0 ? (box_side - layout_height) / 2 : 0;
      box_padding_start = ((box_side / 2) - layout_width) / 2 > 0 ? ((box_side / 2) - layout_width) / 2 : 0;

      gtk_render_layout (context, cr,
                         x + sw * box_padding_start + year_view->k * (8 * box_side - layout_width),
                         box_side * (i + 1) + y + box_padding_top,
                         layout);

      g_free (nr_week);
    }
  gtk_style_context_restore (context);

  pango_font_description_free (sfont_desc);
  g_object_unref (slayout);

  pango_font_description_free (font_desc);
  g_object_unref (layout);
  cairo_restore (cr);
}

static gboolean
draw_navigator (GcalYearView *year_view,
                cairo_t      *cr,
                GtkWidget    *widget)
{
  GtkStyleContext *context;
  GtkStateFlags state_flags;

  gint header_padding_left, header_padding_top, header_height, layout_width, layout_height;
  gint real_padding_left, real_padding_top, i, sw, weeks_counter;
  gdouble width, height, box_side;

  gchar *header_str;

  PangoLayout *header_layout;
  PangoFontDescription *font_desc;

  context = gtk_widget_get_style_context (GTK_WIDGET (year_view));
  state_flags = gtk_style_context_get_state (context);
  sw = 1 - 2 * year_view->k;
  width = gtk_widget_get_allocated_width (widget);

  /* read header from CSS code related to the view */
  gtk_style_context_save (context);
  gtk_style_context_add_class (context, "first-view-header");

  header_str = g_strdup_printf ("%d", year_view->date->year);
  gtk_style_context_get (context, state_flags,
                         "padding-left", &header_padding_left, "padding-top", &header_padding_top,
                         "font", &font_desc, NULL);

  gtk_style_context_restore (context);

  /* draw header on navigator */
  context = gtk_widget_get_style_context (widget);
  state_flags = gtk_style_context_get_state (context);

  header_layout = gtk_widget_create_pango_layout (widget, header_str);
  pango_layout_set_font_description (header_layout, font_desc);
  pango_layout_get_pixel_size (header_layout, &layout_width, &layout_height);
  /* XXX: here the color of the text isn't read from year-view but from navigator widget,
   * which has the same color on the CSS file */
  gtk_render_layout (context, cr,
                     year_view->k * (width - layout_width) + sw * header_padding_left, header_padding_top, header_layout);

  pango_font_description_free (font_desc);
  g_object_unref (header_layout);
  g_free (header_str);

  header_height = header_padding_top * 2 + layout_height;
  height = gtk_widget_get_allocated_height (widget) - header_height;

  if (((width / 4.0) / 8.0) < ((height / 3.0) / 7.0))
    box_side = (width / 4.0) / 8.0;
  else
    box_side = (height / 3.0) / 7.0;

  real_padding_left = (width - (8 * 4 * box_side)) / 5.0;
  real_padding_top = (height - (7 * 3 * box_side)) / 4.0;

  year_view->navigator_grid->box_side = box_side;
  weeks_counter = year_view->first_week_of_year;
  for (i = 0; i < 12; i++)
    {
      gint row = i / 4;
      gint column = year_view->k * 3 + sw * (i % 4);

      year_view->navigator_grid->coordinates[i].x = (column + 1) * real_padding_left + column * box_side * 8;
      year_view->navigator_grid->coordinates[i].y = (row + 1) * real_padding_top + row * box_side * 7 + header_height;
      draw_month_grid (year_view, widget, cr, i, &weeks_counter);
    }

  return FALSE;
}

static gboolean
navigator_button_press_cb (GcalYearView   *year_view,
                           GdkEventButton *event,
                           GtkWidget      *widget)
{
  gint day, month;
  if (!calculate_day_month_for_coord (year_view, event->x, event->y, &day, &month))
    return FALSE;

  year_view->button_pressed = TRUE;
  year_view->selected_data->start_day = day;
  year_view->selected_data->start_month = month;

  return TRUE;
}

static gboolean
navigator_button_release_cb (GcalYearView   *year_view,
                             GdkEventButton *event,
                             GtkWidget      *widget)
{
  gint day, month;

  if (!year_view->button_pressed)
    return FALSE;

  if (!calculate_day_month_for_coord (year_view, event->x, event->y, &day, &month))
    goto fail;

  year_view->button_pressed = FALSE;
  year_view->selected_data->end_day = day;
  year_view->selected_data->end_month = month;

  /* update date and notify */
  year_view->date->day = day;
  year_view->date->month = month + 1;
  g_object_notify (G_OBJECT (year_view), "active-date");

  gtk_widget_queue_draw (widget);

  if (year_view->popover_mode)
    {
      GdkRectangle rect;
      GtkWidget *box;

      /* sizing */
      box = gtk_bin_get_child (GTK_BIN (year_view->popover));
      gtk_widget_set_size_request (box, 200, year_view->navigator_grid->box_side * 2 * 7);

      /* FIXME: improve rect */
      rect.x = event->x;
      rect.y = event->y;
      rect.width = rect.height = 1;
      gtk_popover_set_pointing_to (GTK_POPOVER (year_view->popover), &rect);

      /* FIXME: do no show over selected days */
      gtk_popover_set_position (GTK_POPOVER (year_view->popover), GTK_POS_RIGHT);
      gtk_widget_show (year_view->popover);
    }

  update_sidebar (year_view);
  return TRUE;

fail:
  year_view->button_pressed = FALSE;
  reset_sidebar (year_view);
  return TRUE;
}

static gboolean
navigator_motion_notify_cb (GcalYearView   *year_view,
                            GdkEventMotion *event,
                            GtkWidget      *widget)
{
  gint day, month;

  if (!year_view->button_pressed)
    return FALSE;

  if (!calculate_day_month_for_coord (year_view, event->x, event->y, &day, &month))
    goto fail;

  year_view->selected_data->end_day = day;
  year_view->selected_data->end_month = month;
  gtk_widget_queue_draw (widget);
  return TRUE;

fail:
  year_view->selected_data->end_day = year_view->selected_data->start_day;
  year_view->selected_data->end_month = year_view->selected_data->start_month;
  gtk_widget_queue_draw (widget);
  return TRUE;
}

static void
add_event_clicked_cb (GcalYearView *year_view,
                      GtkButton    *button)
{
  icaltimetype *start_date, *end_date = NULL;

  if (year_view->start_selected_date->day == 0)
    {
      start_date = gcal_dup_icaltime (year_view->current_date);
    }
  else
    {
      start_date = gcal_dup_icaltime (year_view->start_selected_date);
      end_date = gcal_dup_icaltime (year_view->end_selected_date);
      end_date->day += 1;
      *end_date = icaltime_normalize (*end_date);
      end_date->is_date = 1;
    }

  if (year_view->popover_mode)
    gtk_widget_hide (year_view->popover);

  start_date->is_date = 1;
  g_signal_emit_by_name (GCAL_VIEW (year_view), "create-event-detailed", start_date, end_date);
  g_free (start_date);
  if (end_date != NULL)
    g_free (end_date);
}

static void
popover_closed_cb (GcalYearView *year_view,
                   GtkPopover   *popover)
{
  reset_sidebar (year_view);
}

static void
gcal_year_view_finalize (GObject *object)
{
  GcalYearView *year_view = GCAL_YEAR_VIEW (object);

  g_free (year_view->navigator_grid);
  g_free (year_view->selected_data);

  g_free (year_view->start_selected_date);
  g_free (year_view->end_selected_date);

  g_clear_pointer (&year_view->date, g_free);

  G_OBJECT_CLASS (gcal_year_view_parent_class)->finalize (object);
}

static void
gcal_year_view_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GcalYearView *self = GCAL_YEAR_VIEW (object);

  switch (prop_id)
    {
    case PROP_DATE:
      g_value_set_boxed (value, self->date);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_year_view_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  switch (prop_id)
    {
    case PROP_DATE:
      update_date (GCAL_YEAR_VIEW (object), g_value_dup_boxed (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_year_view_get_preferred_width (GtkWidget *widget,
                                    gint      *minimum,
                                    gint      *natural)
{
  GcalYearView *year_view = GCAL_YEAR_VIEW (widget);
  GtkStyleContext *context;
  gint padding_left;

  context = gtk_widget_get_style_context (year_view->navigator);

  gtk_style_context_get (context,
                         gtk_style_context_get_state (context),
                         "padding-left", &padding_left, NULL);

  if (minimum != NULL)
    *minimum = NAVIGATOR_CELL_WIDTH * 4 + padding_left * 8;
  if (natural != NULL)
    *natural = NAVIGATOR_CELL_WIDTH * 4 + padding_left * 8 + SIDEBAR_PREFERRED_WIDTH;
}

static void
gcal_year_view_get_preferred_height_for_width (GtkWidget *widget,
                                               gint       width,
                                               gint      *minimum,
                                               gint      *natural)
{
  GcalYearView *year_view = GCAL_YEAR_VIEW (widget);
  GtkStyleContext *context;
  gint padding_top;

  context = gtk_widget_get_style_context (year_view->navigator);

  gtk_style_context_get (context,
                         gtk_style_context_get_state (context),
                         "padding-top", &padding_top, NULL);

  if (minimum != NULL)
    *minimum = NAVIGATOR_CELL_HEIGHT * 3 + padding_top * 6;
  if (natural != NULL)
    *natural = NAVIGATOR_CELL_HEIGHT * 3 + padding_top * 6;
}

static void
gcal_year_view_size_allocate (GtkWidget     *widget,
                              GtkAllocation *alloc)
{
  GcalYearView *year_view = GCAL_YEAR_VIEW (widget);
  GtkStyleContext *context;
  gint padding_left;

  context = gtk_widget_get_style_context (widget);
  gtk_style_context_save (context);
  gtk_style_context_add_class (context, "year-navigator");
  gtk_style_context_get (context, gtk_style_context_get_state (context), "padding-left", &padding_left, NULL);
  gtk_style_context_restore (context);

  year_view->popover_mode = (alloc->width < NAVIGATOR_CELL_WIDTH * 4 + padding_left * 8 + SIDEBAR_PREFERRED_WIDTH);
  if (year_view->popover_mode && !gtk_widget_is_ancestor (year_view->events_sidebar, year_view->popover))
    {
      g_object_ref (year_view->sidebar);

      gtk_container_remove (GTK_CONTAINER (widget), year_view->sidebar);
      gtk_container_add (GTK_CONTAINER (year_view->popover), year_view->sidebar);

      g_object_unref (year_view->sidebar);

      gtk_widget_show_all (year_view->sidebar);
      popover_closed_cb (GCAL_YEAR_VIEW (widget), GTK_POPOVER (year_view->popover));
    }
  else if (!year_view->popover_mode && gtk_widget_is_ancestor (year_view->events_sidebar, year_view->popover))
    {
      g_object_ref (year_view->sidebar);

      gtk_container_remove (GTK_CONTAINER (year_view->popover), year_view->sidebar);
      gtk_box_pack_end (GTK_BOX (widget), year_view->sidebar, FALSE, TRUE, 0);

      g_object_unref (year_view->sidebar);

      gtk_widget_show (year_view->sidebar);
      g_signal_handlers_block_by_func (year_view->popover, popover_closed_cb, widget);
      gtk_widget_hide (year_view->popover);
      g_signal_handlers_unblock_by_func (year_view->popover, popover_closed_cb, widget);
    }

  GTK_WIDGET_CLASS (gcal_year_view_parent_class)->size_allocate (widget, alloc);
}

static void
gcal_year_view_direction_changed (GtkWidget        *widget,
                                  GtkTextDirection  previous_direction)
{
  GcalYearView *year_view = GCAL_YEAR_VIEW (widget);

  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR)
    year_view->k = 0;
  else if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
    year_view->k = 1;
}

static GList*
gcal_year_view_get_children_by_uuid (GcalView    *view,
                                     const gchar *uuid)
{
  GcalYearView *year_view = GCAL_YEAR_VIEW (view);
  GList *children, *l, *result = NULL;

  children = gtk_container_get_children (GTK_CONTAINER (year_view->events_sidebar));
  for (l = children; l != NULL; l = g_list_next (l))
    {
      GcalEventWidget *child_widget;
      GcalEvent *event;

      child_widget = GCAL_EVENT_WIDGET (gtk_bin_get_child (GTK_BIN (l->data)));
      event = gcal_event_widget_get_event (child_widget);


      if (child_widget != NULL && g_strcmp0 (uuid, gcal_event_get_uid (event)) == 0)
        result = g_list_append (result, child_widget);
    }
  g_list_free (children);

  return result;
}

static void
gcal_year_view_component_added (ECalDataModelSubscriber *subscriber,
                                ECalClient              *client,
                                ECalComponent           *comp)
{
  GcalYearView *year_view = GCAL_YEAR_VIEW (subscriber);

  GcalEvent *event;
  GList **days_widgets_array;
  GList *l;
  gint i, days_span;

  ECalComponentDateTime date;
  time_t event_start, event_end, range_start, range_end;
  icaltimezone *zone;

  update_selected_dates_from_button_data (year_view);
  days_span = icaltime_day_of_year(*(year_view->end_selected_date)) - icaltime_day_of_year(*(year_view->start_selected_date)) + 1;
  days_widgets_array = g_new0 (GList*, days_span);

  event = gcal_event_new (e_client_get_source (E_CLIENT (client)), comp);

  /* check if event belongs to range */
  zone = gcal_manager_get_system_timezone (year_view->manager);
  range_start = icaltime_as_timet_with_zone (*(year_view->start_selected_date), zone);
  range_end = icaltime_as_timet_with_zone (*(year_view->end_selected_date), zone);

  e_cal_component_get_dtstart (comp, &date);
  event_start = icaltime_as_timet_with_zone (*(date.value), date.value->zone != NULL ? date.value->zone : zone);
  e_cal_component_free_datetime (&date);

  e_cal_component_get_dtend (comp, &date);
  if (date.value != NULL)
    event_end = icaltime_as_timet_with_zone (*(date.value), date.value->zone != NULL ? date.value->zone : zone);
  else
    event_end = event_start;
  e_cal_component_free_datetime (&date);

  if (!((event_start <= range_start && event_end >= range_end) ||
        (event_start >= range_start && event_end <= range_end) ||
        (event_start >= range_start && event_start <= range_end) ||
        (event_end >= range_start && event_end <= range_end)))
    {
      goto out;
    }

  add_event_to_day_array (year_view, event, days_widgets_array, days_span);
  gtk_stack_set_visible_child_name (GTK_STACK (year_view->navigator_stack), "events-list");

  for (i = 0; i < days_span; i++)
    {
      GList *current_day = days_widgets_array[i];
      for (l = current_day; l != NULL; l = g_list_next (l))
        {
          GtkWidget *child_widget = l->data;
          gtk_widget_show (child_widget);
          g_signal_connect (child_widget, "activate", G_CALLBACK (event_activated), year_view);
          g_object_set_data (G_OBJECT (child_widget), "shift", GINT_TO_POINTER (i));
          gtk_container_add (GTK_CONTAINER (year_view->events_sidebar), child_widget);
        }

      g_list_free (current_day);
    }

out:
  g_clear_object (&event);
  g_free (days_widgets_array);
}

static void
gcal_year_view_component_changed (ECalDataModelSubscriber *subscriber,
                                  ECalClient              *client,
                                  ECalComponent           *comp)
{
  GcalYearView *year_view = GCAL_YEAR_VIEW (subscriber);
  GList *children, *l;
  gchar *uuid;

  uuid = get_uuid_from_component (e_client_get_source (E_CLIENT (client)), comp);
  children = gtk_container_get_children (GTK_CONTAINER (year_view->events_sidebar));
  for (l = children; l != NULL; l = g_list_next (l))
    {
      GcalEventWidget *child_widget;
      GcalEvent *event;

      child_widget = GCAL_EVENT_WIDGET (gtk_bin_get_child (GTK_BIN (l->data)));
      event = gcal_event_widget_get_event (child_widget);

      if (child_widget != NULL && g_strcmp0 (uuid, gcal_event_get_uid (event)) == 0)
        gtk_widget_destroy (GTK_WIDGET (l->data));
    }
  g_list_free (children);
  g_free (uuid);

  gcal_year_view_component_added (subscriber, client, comp);
}

static void
gcal_year_view_component_removed (ECalDataModelSubscriber *subscriber,
                                  ECalClient              *client,
                                  const gchar             *uid,
                                  const gchar             *rid)
{
  GcalYearView *year_view = GCAL_YEAR_VIEW (subscriber);
  GList *children, *l;
  ESource *source;
  gchar *uuid;
  gboolean update_sidebar_needed = FALSE;

  source = e_client_get_source (E_CLIENT (client));
  if (rid != NULL)
    uuid = g_strdup_printf ("%s:%s:%s", e_source_get_uid (source), uid, rid);
  else
    uuid = g_strdup_printf ("%s:%s", e_source_get_uid (source), uid);

  children = gtk_container_get_children (GTK_CONTAINER (year_view->events_sidebar));
  for (l = children; l != NULL; l = g_list_next (l))
    {
      GcalEventWidget *child_widget;
      GcalEvent *event;

      child_widget = GCAL_EVENT_WIDGET (gtk_bin_get_child (GTK_BIN (l->data)));
      event = gcal_event_widget_get_event (child_widget);


      if (child_widget != NULL && g_strcmp0 (uuid, gcal_event_get_uid (event)) == 0)
        {
          if (g_list_length (children) == 1)
            update_sidebar_needed = TRUE;
          gtk_widget_destroy (GTK_WIDGET (l->data));
        }
    }
  g_list_free (children);
  g_free (uuid);

  if (update_sidebar_needed)
    {
      update_no_events_page (GCAL_YEAR_VIEW (subscriber));
      gtk_stack_set_visible_child_name (GTK_STACK (year_view->navigator_stack), "no-events");
    }
}

static void
gcal_year_view_freeze (ECalDataModelSubscriber *subscriber)
{
  ;
}

static void
gcal_year_view_thaw (ECalDataModelSubscriber *subscriber)
{
}

static void
gcal_year_view_class_init (GcalYearViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gcal_year_view_finalize;
  object_class->get_property = gcal_year_view_get_property;
  object_class->set_property = gcal_year_view_set_property;

  widget_class->get_preferred_width = gcal_year_view_get_preferred_width;
  widget_class->get_preferred_height_for_width = gcal_year_view_get_preferred_height_for_width;
  widget_class->size_allocate = gcal_year_view_size_allocate;
  widget_class->direction_changed = gcal_year_view_direction_changed;

  g_object_class_override_property (object_class, PROP_DATE, "active-date");

  /* FIXME: it will problably go back to GcalView */
  signals[EVENT_ACTIVATED] = g_signal_new ("event-activated", GCAL_TYPE_YEAR_VIEW, G_SIGNAL_RUN_LAST,
                                           0,
                                           NULL, NULL, NULL,
                                           G_TYPE_NONE, 1, GCAL_TYPE_EVENT_WIDGET);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/year-view.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalYearView, navigator);
  gtk_widget_class_bind_template_child (widget_class, GcalYearView, sidebar);
  gtk_widget_class_bind_template_child (widget_class, GcalYearView, events_sidebar);
  gtk_widget_class_bind_template_child (widget_class, GcalYearView, navigator_stack);
  gtk_widget_class_bind_template_child (widget_class, GcalYearView, navigator_sidebar);
  gtk_widget_class_bind_template_child (widget_class, GcalYearView, no_events_title);
  gtk_widget_class_bind_template_child (widget_class, GcalYearView, popover);

  gtk_widget_class_bind_template_callback (widget_class, draw_navigator);
  gtk_widget_class_bind_template_callback (widget_class, navigator_button_press_cb);
  gtk_widget_class_bind_template_callback (widget_class, navigator_button_release_cb);
  gtk_widget_class_bind_template_callback (widget_class, navigator_motion_notify_cb);
  gtk_widget_class_bind_template_callback (widget_class, add_event_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, popover_closed_cb);

  gtk_widget_class_set_css_name (widget_class, "calendar-view");
}

static void
gcal_year_view_init (GcalYearView *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  if (gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_LTR)
    self->k = 0;
  else if (gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL)
    self->k = 1;

  self->navigator_grid = g_new0 (GridData, 1);
  self->selected_data = g_new0 (ButtonData, 1);

  self->start_selected_date = g_new0 (icaltimetype, 1);
  self->start_selected_date->zone = e_cal_util_get_system_timezone ();
  self->end_selected_date = g_new0 (icaltimetype, 1);
  self->end_selected_date->zone = e_cal_util_get_system_timezone ();

  gtk_list_box_set_header_func (GTK_LIST_BOX (self->events_sidebar), update_sidebar_headers, self, NULL);
  gtk_list_box_set_sort_func (GTK_LIST_BOX (self->events_sidebar), sidebar_sort_func, NULL, NULL);
}

static void
gcal_view_interface_init (GcalViewIface *iface)
{
  /* FIXME: implement what's needed */
  iface->get_children_by_uuid = gcal_year_view_get_children_by_uuid;
}

static void
gcal_data_model_subscriber_interface_init (ECalDataModelSubscriberInterface *iface)
{
  iface->component_added = gcal_year_view_component_added;
  iface->component_modified = gcal_year_view_component_changed;
  iface->component_removed = gcal_year_view_component_removed;
  iface->freeze = gcal_year_view_freeze;
  iface->thaw = gcal_year_view_thaw;
}

/* Public API */
void
gcal_year_view_set_manager (GcalYearView *year_view,
                            GcalManager  *manager)
{
  year_view->manager = manager;
}

void
gcal_year_view_set_first_weekday (GcalYearView *year_view,
                                  gint          nr_day)
{
  year_view->first_weekday = nr_day;
}

void
gcal_year_view_set_use_24h_format (GcalYearView *year_view,
                                   gboolean      use_24h_format)
{
  year_view->use_24h_format = use_24h_format;
}

void
gcal_year_view_set_current_date (GcalYearView *year_view,
                                 icaltimetype *current_date)
{
  year_view->current_date = current_date;

  /* Launches update */
  gtk_widget_queue_draw (GTK_WIDGET (year_view));
  update_sidebar (year_view);
}
