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

#define G_LOG_DOMAIN "GcalYearView"

#include "gcal-debug.h"
#include "gcal-year-view.h"
#include "gcal-view.h"
#include "gcal-utils.h"

#include <glib/gi18n.h>
#include <math.h>
#include <string.h>

#define NAVIGATOR_MAX_GRID_SIZE 4
#define VISUAL_CLUES_SIDE 3.0
#define WEEK_NUMBER_MARGIN 3.0

typedef struct
{
  /* month span from 0 to 11 */
  gint start_day, start_month;
  gint end_day, end_month;
  gint hovered_day, hovered_month;
  gint dnd_day, dnd_month;
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
  GtkWidget    *scrolled_window;
  GtkLabel     *temp_label;   /* unowned */
  GtkImage     *weather_icon; /* unowned */

  GtkWidget    *popover; /* Popover for popover_mode */

  /* manager singleton */
  GcalManager  *manager;
  GcalWeatherService *weather_service;

  /* range shown on the sidebar */
  icaltimetype *start_selected_date;
  icaltimetype *end_selected_date;

  /* geometry info */
  GridData     *navigator_grid;
  guint         number_of_columns;
  guint         column_width;
  guint         row_height;
  guint         header_height;
  guint         sidebar_width;

  /* state flags */
  gboolean      popover_mode;
  gboolean      button_pressed;
  ButtonData   *selected_data;

  /**
   * first day of the week according to user locale, being
   * 0 for Sunday, 1 for Monday and so on
   */
  gint          first_weekday;

  /* first and last weeks of the year */
  guint         first_week_of_year;
  guint         last_week_of_year;

  /* Storage for the accumulated scrolling */
  gdouble         scroll_value;

  /**
   * clock format from GNOME desktop settings
   */
  gboolean      use_24h_format;

  /* show week numbers from GNOME Shell settings */
  GSettings    *calendar_settings;
  gboolean      show_week_numbers;

  /* text direction factors */
  gint          k;

  /* date property */
  icaltimetype *date;

  /*
   * Array with the events at every month. Events
   * that span multiple months are added multiple
   * times to the array.
   */
  GPtrArray    *events[12];
};

enum {
  PROP_0,
  PROP_DATE,
  PROP_MANAGER,
  PROP_WEATHER_SERVICE,
  PROP_SHOW_WEEK_NUMBERS,
  LAST_PROP
};

enum
{
  EVENT_ACTIVATED,
  NUM_SIGNALS
};

static guint signals[NUM_SIGNALS] = { 0, };
static gpointer year_view_parent_class = NULL;

static void   gcal_view_interface_init (GcalViewInterface *iface);
static void   gcal_data_model_subscriber_interface_init (ECalDataModelSubscriberInterface *iface);
static void   update_weather (GcalYearView *self);

G_DEFINE_TYPE_WITH_CODE (GcalYearView, gcal_year_view, GTK_TYPE_BOX,
                         G_IMPLEMENT_INTERFACE (GCAL_TYPE_VIEW, gcal_view_interface_init)
                         G_IMPLEMENT_INTERFACE (E_TYPE_CAL_DATA_MODEL_SUBSCRIBER,
                                                gcal_data_model_subscriber_interface_init));

static gint
compare_events (GcalEventWidget *widget1,
                GcalEventWidget *widget2)
{
  return gcal_event_compare (gcal_event_widget_get_event (widget1), gcal_event_widget_get_event (widget2));
}

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
event_activated (GcalEventWidget *widget,
                 gpointer         user_data)
{
  GcalYearView *view = GCAL_YEAR_VIEW (user_data);

  if (view->popover_mode)
    gtk_widget_hide (view->popover);
  g_signal_emit (GCAL_YEAR_VIEW (user_data), signals[EVENT_ACTIVATED], 0, widget);
}

static void
weather_changed (GcalWeatherService *weather_service,
                 GcalYearView       *self)
{
  g_return_if_fail (GCAL_IS_WEATHER_SERVICE (weather_service));
  g_return_if_fail (GCAL_IS_YEAR_VIEW (self));

  update_weather (self);
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
      g_autofree icaltimetype *current_date;
      g_autoptr (GDateTime) now;

      now = g_date_time_new_now_local ();
      current_date = datetime_to_icaltime (now);

      if (year_view->date)
        *(year_view->start_selected_date) = *year_view->date;
      else
        *(year_view->start_selected_date) = *current_date;

      year_view->start_selected_date->hour = 0;
      year_view->start_selected_date->minute = 0;
      year_view->start_selected_date->second = 0;

      *(year_view->end_selected_date) = *(year_view->start_selected_date);
      year_view->end_selected_date->hour = 23;
      year_view->end_selected_date->minute = 59;
      *(year_view->end_selected_date) = icaltime_normalize (*(year_view->end_selected_date));

      year_view->selected_data->start_day = year_view->start_selected_date->day;
      year_view->selected_data->start_month = year_view->start_selected_date->month - 1;
      year_view->selected_data->end_day = year_view->end_selected_date->day;
      year_view->selected_data->end_month = year_view->end_selected_date->month -1;
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
  g_autoptr (GDateTime) start_selected_date, now;
  gboolean has_range;
  gchar *title;

  now = g_date_time_new_now_local ();
  start_selected_date = icaltime_to_datetime (year_view->start_selected_date);

  has_range = (year_view->start_selected_date->day != year_view->end_selected_date->day ||
               year_view->start_selected_date->month != year_view->end_selected_date->month);

  if (datetime_compare_date (now, start_selected_date) == 0)
    {
      title = g_strdup_printf ("%s%s", _("Today"), has_range ? "…" : "");
    }
  else
    {
      if (has_range)
        {
          /* Translators: This is a date format in the sidebar of the year
           * view when the selection starts at the specified day and the
           * end is unspecified.  */
          title = g_date_time_format (start_selected_date, _("%B %d…"));
        }
      else
        {
          /* Translators: This is a date format in the sidebar of the year
           * view when there is only one specified day selected.  */
          title = g_date_time_format (start_selected_date, _("%B %d"));
        }
    }

  gtk_label_set_text (GTK_LABEL (year_view->no_events_title), title);
  update_weather (year_view);
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

  gint i;

  child_widget = gcal_event_widget_new (event);
  gcal_event_widget_set_read_only (GCAL_EVENT_WIDGET (child_widget),
                                   !gcal_manager_is_client_writable (year_view->manager, gcal_event_get_source (event)));

  dt_start = gcal_event_get_date_start (event);
  dt_end = gcal_event_get_date_end (event);

  /* normalize date on each new event */
  date = icaltime_to_datetime (year_view->start_selected_date);
  second_date = g_date_time_add_days (date, 1);

  /* marking and cloning */
  for (i = 0; i < days_span; i++)
    {
      GtkWidget *cloned_child;
      GDateTime *aux;
      gint start_comparison;

      cloned_child = child_widget;
      start_comparison = datetime_compare_date (dt_start, date);

      if (start_comparison > 0)
        {
          /*
           * The start date of the event is ahead of the current
           * date being comparent, so we don't add it now.
           */
          cloned_child = NULL;
        }
      else if (start_comparison == 0)
        {
          /*
           * This is the first day the event happens, so we just use
           * the previously created event widget.
           */
          cloned_child = child_widget;
        }
      else
        {
          /*
           * We're in the middle of the event and the dates are between
           * the event start date > current date > event end date, so
           * we keep cloning the event widget until we reach the end of
           * the event.
           */
          cloned_child = gcal_event_widget_clone (GCAL_EVENT_WIDGET (child_widget));
        }

      if (cloned_child != NULL)
        {
          gint end_comparison;

          /*
           * Setup the event widget's custom dates, so the slanted edges are
           * applied properly
           */
          if (gcal_event_is_multiday (event))
            {
              gcal_event_widget_set_date_start (GCAL_EVENT_WIDGET (cloned_child), date);
              gcal_event_widget_set_date_end (GCAL_EVENT_WIDGET (cloned_child), second_date);
            }

          days_widgets_array[i] = g_list_insert_sorted (days_widgets_array[i],
                                                        cloned_child,
                                                        (GCompareFunc) compare_events);

          end_comparison = datetime_compare_date (second_date, dt_end);

          /*
           * If the end date surpassed the event's end date, we reached the
           * end of the event's time span and shall stop adding the event to
           * the list.
           */
          if (end_comparison >= 0)
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

  g_clear_pointer (&second_date, g_date_time_unref);
  g_clear_pointer (&date, g_date_time_unref);
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

  gtk_widget_queue_draw (year_view->navigator);

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

  if (!row_child)
    return;

  row_event = gcal_event_widget_get_event (GCAL_EVENT_WIDGET (row_child));
  row_date = gcal_event_get_date_start (row_event);
  row_date = g_date_time_to_local (row_date);
  row_shift = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row_child), "shift"));

  if (before)
    {
      before_child = gtk_bin_get_child (GTK_BIN (before));
      before_event = gcal_event_widget_get_event (GCAL_EVENT_WIDGET (before_child));
      before_date = gcal_event_get_date_start (before_event);
      before_date = g_date_time_to_local (before_date);
      before_shift = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (before_child), "shift"));
    }

  if (before_shift == -1 || before_shift != row_shift)
    {
      g_autoptr (GDateTime) now, dt;
      GtkWidget *label;
      gchar *label_str;

      row_header = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      now = g_date_time_new_now_local ();

      date = *(year_view->start_selected_date);
      icaltime_adjust (&date, row_shift, 0, 0, 0);

      dt = icaltime_to_datetime (&date);

      if (datetime_compare_date (dt, now) == 0)
        label_str = g_strdup (_("Today"));
      else
        /* Translators: This is a date format in the sidebar of the year view. */
        label_str = g_date_time_format (dt, _("%B %d"));

      label = gtk_label_new (label_str);
      gtk_style_context_add_class (gtk_widget_get_style_context (label), "sidebar-header");
      g_object_set (label, "margin", 6, "halign", GTK_ALIGN_START, NULL);
      g_free (label_str);

      gtk_container_add (GTK_CONTAINER (row_header), label);
    }

  if (row_header != NULL)
    gtk_widget_show_all (row_header);
  gtk_list_box_row_set_header (row, row_header);

  g_clear_pointer (&before_date, g_date_time_unref);
  g_clear_pointer (&row_date, g_date_time_unref);
}

static void
update_date (GcalYearView *year_view,
             icaltimetype *new_date)
{
  gboolean needs_reset = FALSE;
  if (year_view->date != NULL && icaltime_compare_date (year_view->date, new_date) && year_view->start_selected_date->day != 0)
    needs_reset = TRUE;

  g_clear_pointer (&year_view->date, g_free);
  year_view->date = gcal_dup_icaltime (new_date);

  year_view->first_week_of_year = get_last_week_of_year_dmy (year_view->first_weekday,
                                                             1, G_DATE_JANUARY,  year_view->date->year);;
  year_view->last_week_of_year = get_last_week_of_year_dmy (year_view->first_weekday,
                                                            31, G_DATE_DECEMBER, year_view->date->year);

  if (needs_reset)
    reset_sidebar (year_view);
}

static void
calculate_coord_for_date (GcalYearView *year_view,
                          gint          day,
                          gint          month,
                          gboolean      is_title,
                          GdkRectangle *rect)
{
  gint cell_x, cell_y, clicked_cell, sw;

  if (is_title ||
      year_view->selected_data->start_day != year_view->selected_data->end_day ||
      year_view->selected_data->start_month != year_view->selected_data->end_month)
    {
      rect->x = year_view->navigator_grid->coordinates[month].x;
      rect->y = year_view->navigator_grid->coordinates[month].y;
      rect->width = year_view->column_width;
      rect->height =  year_view->row_height;

      return;
    }

  /* else */
  sw = 1 - 2 * year_view->k;
  clicked_cell = day + ((time_day_of_week (1, month, year_view->date->year) - year_view->first_weekday + 7) % 7) - 1;

  cell_x = (clicked_cell % 7 + year_view->k + year_view->show_week_numbers) * year_view->navigator_grid->box_side * sw;
  cell_x += (year_view->k * year_view->navigator_grid->box_side * (7 + year_view->show_week_numbers));
  cell_y = ((clicked_cell / 7 + 1) * year_view->navigator_grid->box_side);

  rect->x = cell_x + year_view->navigator_grid->coordinates[month].x;
  rect->y = cell_y + year_view->navigator_grid->coordinates[month].y;
  rect->width = rect->height =  year_view->navigator_grid->box_side;
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
    return compare_events (GCAL_EVENT_WIDGET (row1_child), GCAL_EVENT_WIDGET (row2_child));

  return result;
}


static gboolean
calculate_day_month_for_coord (GcalYearView *year_view,
                               gdouble       x,
                               gdouble       y,
                               gint         *out_day,
                               gint         *out_month,
                               gboolean     *is_title)
{
  GdkPoint *coordinates;
  gint row, column, i, sw, clicked_cell, day, month, columns_or_rows;
  gint number_of_rows;
  gdouble box_side;

  row = -1;
  column = -1;
  box_side = year_view->navigator_grid->box_side;
  number_of_rows = ceil (12.0 / year_view->number_of_columns);
  sw = 1 - 2 * year_view->k;
  coordinates = year_view->navigator_grid->coordinates;

  *is_title = FALSE;
  columns_or_rows = year_view->number_of_columns > number_of_rows ? year_view->number_of_columns : number_of_rows;

  for (i = 0; i < columns_or_rows && ((row == -1) || (column == -1)); i++)
    {
      if (row == -1 &&
          y > coordinates[i * year_view->number_of_columns].y &&
          y < coordinates[i * year_view->number_of_columns].y + year_view->row_height)
        {
          if (y < coordinates[i * year_view->number_of_columns].y + box_side)
            *is_title = TRUE;
          row = i;
        }
      if (column == -1 &&
          x > coordinates[i].x + box_side * year_view->show_week_numbers * (1 - year_view->k) &&
          x < coordinates[i].x + year_view->column_width - box_side * year_view->show_week_numbers * year_view->k)
        {
          column = i;
        }
    }

  if (row == -1 || column == -1)
    return FALSE;

  month = row * year_view->number_of_columns + column;

  if (month < 0 || month > 11)
    return FALSE;

  *out_month = month;

  if (*is_title)
    return TRUE;

  row = (y - (year_view->navigator_grid->coordinates[month].y + box_side)) / box_side;
  column = (x - (year_view->navigator_grid->coordinates[month].x + box_side * year_view->show_week_numbers * (1 - year_view->k))) / box_side;
  clicked_cell = row * 7 + column;
  day = 7 * ((clicked_cell + 7 * year_view->k) / 7) + sw * (clicked_cell % 7) + (1 - year_view->k);
  day -= ((time_day_of_week (1, month, year_view->date->year) - year_view->first_weekday + 7) % 7);

  if (day < 1 || day > time_days_in_month (year_view->date->year, month))
    return FALSE;

  *out_day = day;
  return TRUE;
}
static guint
count_events_at_day (GcalYearView *self,
                     icaltimetype *today)
{
  g_autoptr (GDateTime) today_start, today_end;
  GPtrArray *events;
  guint i, n_events;

  events = self->events[today->month - 1];
  n_events = 0;
  today_start = icaltime_to_datetime (today);
  today_end = g_date_time_add_days (today_start, 1);

  for (i = 0; i < events->len; i++)
    {
      g_autoptr (GDateTime) event_start, event_end;
      GcalEvent *event;

      event = g_ptr_array_index (events, i);

      event_start = g_date_time_ref (gcal_event_get_date_start (event));

      if (gcal_event_get_all_day (event))
          event_end = g_date_time_add_days (gcal_event_get_date_end (event), -1);
      else
          event_end = g_date_time_ref (gcal_event_get_date_end (event));


      if (datetime_compare_date (event_start, today_end) >= 0 ||
          datetime_compare_date (event_end, today_start) < 0)
        {
          continue;
        }

      n_events++;
    }

  return n_events;
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

  g_autoptr (GDateTime) now;

  GdkRGBA color;
  gint layout_width, layout_height, i, j, sw;
  gint column, row;
  gboolean column_is_workday;
  gdouble x, y, box_side, box_padding_top, box_padding_start;
  gint days_delay, days, shown_rows;
  gchar *str, *nr_day, *nr_week;
  gboolean selected_day;
  icaltimetype today;

  now = g_date_time_new_now_local ();

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
  gtk_render_layout (context, cr, x + (year_view->column_width - layout_width) / 2.0, y + (box_side - layout_height) / 2.0,
                     layout);

  gtk_render_background (context, cr,
                         x + (year_view->column_width - layout_width) / 2.0, y + (box_side - layout_height) / 2.0,
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

  cairo_move_to (cr, x + box_side * year_view->show_week_numbers * (1 - year_view->k), y + box_side + 0.4);
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

  today = icaltime_today ();
  today.year = year_view->date->year;
  today.month = month_nr + 1;
  today.day = 1;
  today.hour = 0;
  today.minute = 0;
  today.second = 0;
  today.zone = gcal_manager_get_system_timezone (year_view->manager);

  for (i = 0; i < 7 * shown_rows; i++)
    {
      column = (i % 7) + (year_view->k || year_view->show_week_numbers);
      column_is_workday = is_workday ((7 * year_view->k + sw * (column - (year_view->show_week_numbers * !year_view->k) + (sw * year_view->first_weekday))) % 7);
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

      /* Draw the hover background */
      if (year_view->selected_data->hovered_day == j &&
          year_view->selected_data->hovered_month == month_nr)
        {
          gint hover_x, hover_y;

          hover_x = box_side * column + x + box_padding_start / 2.0 - year_view->k * box_side - 2.0;
          hover_y = box_side * (row + 1) + y + box_padding_top - 1.0;

          gtk_style_context_set_state (context, state_flags | GTK_STATE_FLAG_PRELIGHT);

          gtk_render_background (context,
                                 cr,
                                 hover_x,
                                 hover_y,
                                 box_side - box_padding_start / 2.0,
                                 box_side);

          gtk_style_context_set_state (context, state_flags);
        }

      /* Draw the Drag n' Drop indicator */
      if (year_view->selected_data->dnd_day == j &&
          year_view->selected_data->dnd_month == month_nr)
        {
          gint dnd_x, dnd_y;

          dnd_x = box_side * column + x + box_padding_start / 2.0 - year_view->k * box_side - 2.0;
          dnd_y = box_side * (row + 1) + y + box_padding_top - 1.0;

          gtk_style_context_save (context);
          gtk_style_context_add_class (context, "dnd");

          gtk_render_background (context,
                                 cr,
                                 dnd_x,
                                 dnd_y,
                                 box_side - box_padding_start / 2.0,
                                 box_side);

          gtk_style_context_restore (context);
        }

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

      if (year_view->date->year == g_date_time_get_year (now) &&
          month_nr + 1 == g_date_time_get_month (now) &&
          j == g_date_time_get_day_of_month (now))
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
                                 box_side * column + x + sw * box_padding_start - year_view->k * layout_width - 2.0,
                                 box_side * (row + 1) + y + box_padding_top - 1.0,
                                 layout_width + 4.0, layout_height + 2.0);
          gtk_render_layout (context, cr,
                             box_side * column + x + sw * box_padding_start - year_view->k * layout_width,
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
                             box_side * column + x + sw * box_padding_start - year_view->k * layout_width,
                             box_side * (row + 1) + y + box_padding_top,
                             slayout);

          gtk_style_context_set_state (context, state_flags);
        }
      else if (!column_is_workday)
        {
          gtk_style_context_save (context);
          gtk_style_context_add_class (context, "non-workday");
          gtk_render_layout (context, cr,
                             box_side * column + x + sw * box_padding_start - year_view->k * layout_width,
                             box_side * (row + 1) + y + box_padding_top,
                             layout);
          gtk_style_context_restore (context);
        }
      else
        {
          gtk_render_layout (context, cr,
                             box_side * column + x + sw * box_padding_start - year_view->k * layout_width,
                             box_side * (row + 1) + y + box_padding_top,
                             layout);
        }

      /* Update the current day, so we can count the events at this day */
      today.day = j;

      if (count_events_at_day (year_view, &today) > 0)
        {
          gtk_style_context_save (context);
          gtk_style_context_add_class (context, "with-events");

          if (selected_day)
            gtk_style_context_add_class (context, "with-events-selected");
          else if (!column_is_workday)
            gtk_style_context_add_class (context, "with-events-non-workday");

          box_padding_start = MAX (0, (box_side - VISUAL_CLUES_SIDE) / 2);
          gtk_render_background (context, cr,
                                 box_side * column + x + sw * box_padding_start - year_view->k * VISUAL_CLUES_SIDE,
                                 box_side * (row + 1) + y + box_padding_top + layout_height + 2.0,
                                 VISUAL_CLUES_SIDE, VISUAL_CLUES_SIDE);
          gtk_style_context_restore (context);
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

      if (year_view->show_week_numbers)
        {
          nr_week = g_strdup_printf ("%d", *weeks_counter);

          pango_layout_set_text (layout, nr_week, -1);
          pango_layout_get_pixel_size (layout, &layout_width, &layout_height);
          box_padding_top = (box_side - layout_height) / 2.0 > 0 ? (box_side - layout_height) / 2.0 : 0;
          box_padding_start = (box_side - layout_width) / 2.0 > 0 ? (box_side - layout_width) / 2.0 : 0;

          gtk_render_background (context, cr,
                                 x + sw * box_padding_top + year_view->k * (8 * box_side - layout_height) - WEEK_NUMBER_MARGIN,
                                 box_side * (i + 1) + y + box_padding_top - WEEK_NUMBER_MARGIN,
                                 layout_height + WEEK_NUMBER_MARGIN * 2, layout_height + WEEK_NUMBER_MARGIN * 2);

          gtk_render_layout (context, cr,
                             x + sw * box_padding_start + year_view->k * (8 * box_side - layout_width),
                             box_side * (i + 1) + y + box_padding_top,
                             layout);

          g_free (nr_week);
        }
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
  GtkBorder padding;

  gint header_height, layout_width, layout_height;
  gint real_padding_left, real_padding_top, i, sw, weeks_counter, number_of_rows;
  gdouble width, height;

  gchar *header_str;

  PangoLayout *header_layout;
  PangoFontDescription *font_desc;

  context = gtk_widget_get_style_context (widget);
  state_flags = gtk_style_context_get_state (context);
  sw = 1 - 2 * year_view->k;
  width = gtk_widget_get_allocated_width (widget);
  number_of_rows = ceil (12.0 / year_view->number_of_columns);

  /* read header from CSS code related to the view */
  gtk_style_context_save (context);
  gtk_style_context_add_class (context, "primary-label");

  header_str = g_strdup_printf ("%d", year_view->date->year);
  gtk_style_context_get (context, state_flags, "font", &font_desc, NULL);
  gtk_style_context_get_padding (context, state_flags, &padding);

  gtk_style_context_restore (context);

  header_layout = gtk_widget_create_pango_layout (widget, header_str);
  pango_layout_set_font_description (header_layout, font_desc);
  pango_layout_get_pixel_size (header_layout, &layout_width, &layout_height);
  /* XXX: here the color of the text isn't read from year-view but from navigator widget,
   * which has the same color on the CSS file */
  gtk_render_layout (context, cr,
                     year_view->k * (width - layout_width) + sw * padding.left,
                     padding.top,
                     header_layout);

  pango_font_description_free (font_desc);
  g_object_unref (header_layout);
  g_free (header_str);

  header_height = padding.top * 2 + layout_height;
  height = gtk_widget_get_allocated_height (widget) - header_height;

  real_padding_left = (width - year_view->column_width * year_view->number_of_columns) / (year_view->number_of_columns + 1);
  real_padding_top = (height - year_view->row_height * number_of_rows) / number_of_rows;

  if (real_padding_top < 0)
    real_padding_top = 0;

  weeks_counter = year_view->first_week_of_year;
  for (i = 0; i < 12; i++)
    {
      gint row = i / year_view->number_of_columns;
      gint column = year_view->k * (year_view->number_of_columns - 1) + sw * (i % year_view->number_of_columns);

      year_view->navigator_grid->coordinates[i].x = (column + 1) * real_padding_left + column * year_view->column_width;
      year_view->navigator_grid->coordinates[i].y = row * real_padding_top + row * year_view->row_height + header_height;
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
  gboolean is_title = FALSE;

  if (!calculate_day_month_for_coord (year_view, event->x, event->y, &day, &month, &is_title))
    return GDK_EVENT_PROPAGATE;

  if (is_title)
    day = 1;

  year_view->button_pressed = TRUE;
  year_view->selected_data->start_day = day;
  year_view->selected_data->start_month = month;

  return GDK_EVENT_PROPAGATE;
}

static gboolean
navigator_button_release_cb (GcalYearView   *year_view,
                             GdkEventButton *event,
                             GtkWidget      *widget)
{
  gint day, month;
  gboolean is_title = FALSE;

  if (!year_view->button_pressed)
    return FALSE;

  if (!calculate_day_month_for_coord (year_view, event->x, event->y, &day, &month, &is_title))
    goto fail;

  if (is_title)
    day = g_date_get_days_in_month (month + 1, year_view->date->year);

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

      calculate_coord_for_date (year_view, day, month, is_title, &rect);
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
  gint day = 0, month = 0;
  gboolean is_title = FALSE;

  /* Cancel the hover when selecting a date range */
  year_view->selected_data->hovered_day = -1;
  year_view->selected_data->hovered_month = -1;

  if (calculate_day_month_for_coord (year_view, event->x, event->y, &day, &month, &is_title))
    {
      if (year_view->button_pressed)
        {
          if (is_title)
            day = g_date_get_days_in_month (month + 1, year_view->date->year);

          year_view->selected_data->end_day = day;
          year_view->selected_data->end_month = month;
          gtk_widget_queue_draw (widget);

          return GDK_EVENT_STOP;
        }
      else
        {
          /* We only deal with the hover effect when the button is not pressed */
          year_view->selected_data->hovered_day = day;
          year_view->selected_data->hovered_month = month;
          gtk_widget_queue_draw (widget);

          return GDK_EVENT_PROPAGATE;
        }
    }

  gtk_widget_queue_draw (widget);

  return GDK_EVENT_STOP;
}

static void
navigator_edge_overshot_cb (GcalYearView    *self,
                            GtkPositionType  position_type)
{
  GtkAdjustment *adjustment;

  /* Ignore horizontal scrolling (and the year view never scrolls horizontally anyway) */
  if (position_type == GTK_POS_LEFT || position_type == GTK_POS_RIGHT)
    return;

  adjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (self->scrolled_window));

  self->date->year += position_type == GTK_POS_BOTTOM ? 1 : -1;
  *self->date = icaltime_normalize (*self->date);

  gtk_adjustment_set_value (adjustment, 0.0);
  gtk_widget_queue_draw (self->navigator);

  g_object_notify (G_OBJECT (self), "active-date");
}

static gboolean
navigator_scroll_event_cb (GcalYearView   *self,
                           GdkEventScroll *scroll_event)
{
  GtkWidget *vscrollbar;

  vscrollbar = gtk_scrolled_window_get_vscrollbar (GTK_SCROLLED_WINDOW (self->scrolled_window));

  /*
   * If the vertical scrollbar is visible, delegate the year changing check
   * to GtkScrolledWindow:edge-overshot callback.
   */
  if (gtk_widget_get_child_visible (vscrollbar))
    return GDK_EVENT_PROPAGATE;

  /*
   * If we accumulated enough scrolling, change the month. Otherwise, we'd scroll
   * waaay too fast.
   */
  if (should_change_date_for_scroll (&self->scroll_value, scroll_event))
    {
      self->date->year += self->scroll_value > 0 ? 1 : -1;
      *self->date = icaltime_normalize (*self->date);
      self->scroll_value = 0;

      gtk_widget_queue_draw (self->navigator);

      g_object_notify (G_OBJECT (self), "active-date");
    }

  return GDK_EVENT_STOP;
}

static void
add_event_clicked_cb (GcalYearView *year_view,
                      GtkButton    *button)
{
  GDateTime *start_date, *end_date = NULL;

  if (year_view->start_selected_date->day == 0)
    {
      start_date = g_date_time_new_now_local ();
    }
  else
    {
      icaltimetype *dtstart, *dtend;
      GDateTime *tmp_date;

      dtstart = year_view->start_selected_date;
      dtend = year_view->end_selected_date;

      start_date = g_date_time_new_local (dtstart->year, dtstart->month, dtstart->day, 0, 0, 0);
      tmp_date = g_date_time_new_local (dtend->year, dtend->month, dtend->day, 0, 0, 0);
      end_date = g_date_time_add_days (tmp_date, 1);

      g_clear_pointer (&tmp_date, g_date_time_unref);
    }

  if (year_view->popover_mode)
    gtk_widget_hide (year_view->popover);

  g_signal_emit_by_name (GCAL_VIEW (year_view), "create-event-detailed", start_date, end_date);

  g_clear_pointer (&start_date, g_date_time_unref);
  g_clear_pointer (&end_date, g_date_time_unref);
}

static void
popover_closed_cb (GcalYearView *year_view,
                   GtkPopover   *popover)
{
  reset_sidebar (year_view);
}

static void
calculate_grid_sizes (GcalYearView *self)
{
  /* 1 column for week number and 7 for days */
  if (self->show_week_numbers)
    self->column_width = self->navigator_grid->box_side * 8;
  else
    self->column_width = self->navigator_grid->box_side * 7;

  /* 1 line for month header and 6 lines for weeks */
  self->row_height = self->navigator_grid->box_side * 7;
}

static void
calculate_sizes (GcalYearView *self)
{
  GtkStyleContext *context;
  PangoLayout *layout;
  PangoFontDescription *font;

  gint padding_left, padding_right, padding_top, padding_bottom;
  gint width, height, box_width, box_height, natural;
  gchar *test_str;

  /* get header info from CSS */
  context = gtk_widget_get_style_context (GTK_WIDGET (self));
  gtk_style_context_save (context);
  gtk_style_context_add_class (context, "primary-label");
  gtk_style_context_get (context, gtk_style_context_get_state (context),
                         "padding-top", &padding_top,
                         "padding-bottom", &padding_bottom,
                         "font", &font, NULL);
  gtk_style_context_restore (context);

  /* measure header height */
  test_str = g_strdup_printf ("8888");
  layout = gtk_widget_create_pango_layout (self->navigator, test_str);
  pango_layout_set_font_description (layout, font);
  pango_layout_get_pixel_size (layout, &width, &height);

  pango_font_description_free (font);
  g_object_unref (layout);
  g_free (test_str);

  self->header_height = padding_top + height + padding_bottom;

  /* get day info from CSS */
  context = gtk_widget_get_style_context (GTK_WIDGET (self->navigator));
  gtk_style_context_save (context);
  gtk_style_context_add_class (context, "days");
  gtk_style_context_get (context, gtk_style_context_get_state (context),
                         "padding-left", &padding_left,
                         "padding-right", &padding_right,
                         "padding-top", &padding_top,
                         "padding-bottom", &padding_bottom,
                         "font", &font, NULL);
  gtk_style_context_restore (context);

  /* measure box size */
  test_str = g_strdup_printf ("88");
  layout = gtk_widget_create_pango_layout (self->navigator, test_str);
  pango_layout_set_font_description (layout, font);
  pango_layout_get_pixel_size (layout, &width, &height);

  pango_font_description_free (font);
  g_object_unref (layout);
  g_free (test_str);

  box_width = padding_left + width + padding_right;
  box_height = padding_top + height + padding_bottom;

  self->navigator_grid->box_side = (box_width > box_height) ? box_width : box_height;

  /* get sidebar width */
  gtk_widget_get_preferred_width (self->sidebar, &width, &natural);
  self->sidebar_width = width;

  calculate_grid_sizes (self);
}

/*
 * Drag and Drop functions
 */
static gboolean
navigator_drag_motion_cb (GcalYearView   *self,
                          GdkDragContext *context,
                          gint            x,
                          gint            y,
                          guint           time,
                          GtkWidget      *navigator)
{
  gint day, month;
  gboolean is_title, retval;

  retval = FALSE;

  self->selected_data->dnd_day = -1;
  self->selected_data->dnd_month = -1;

  if (calculate_day_month_for_coord (self, x, y, &day, &month, &is_title))
    {
      /* For now, don't allow dropping events on the month title */
      if (!is_title)
        {
          self->selected_data->dnd_day = day;
          self->selected_data->dnd_month = month;

          gdk_drag_status (context, GDK_ACTION_MOVE, time);

          retval = TRUE;
        }
    }
  else
    {
      gdk_drag_status (context, 0, time);
    }

  gtk_widget_queue_draw (self->navigator);

  return retval;
}

static gboolean
navigator_drag_drop_cb (GcalYearView   *self,
                        GdkDragContext *context,
                        gint            x,
                        gint            y,
                        guint           time,
                        GtkWidget      *navigator)
{
  gint day, month;
  gboolean is_title;

  if (calculate_day_month_for_coord (self, x, y, &day, &month, &is_title))
    {
      if (!GCAL_IS_EVENT_WIDGET (gtk_drag_get_source_widget (context)))
        return FALSE;

      if (!is_title)
        {
          GcalRecurrenceModType mod;
          GcalEventWidget *event_widget;
          GcalEvent *event;
          GDateTime *start_dt, *end_dt;
          GDateTime *drop_date;
          ESource *source;

          event_widget = GCAL_EVENT_WIDGET (gtk_drag_get_source_widget (context));
          event = gcal_event_widget_get_event (event_widget);
          source = gcal_event_get_source (event);
          mod = GCAL_RECURRENCE_MOD_THIS_ONLY;

          if (gcal_event_has_recurrence (event) &&
              !ask_recurrence_modification_type (GTK_WIDGET (self), &mod, source))
            {
              goto out;
            }

          start_dt = gcal_event_get_date_start (event);
          end_dt = gcal_event_get_date_end (event);

          drop_date = g_date_time_add_full (start_dt,
                                            self->date->year - g_date_time_get_year (start_dt),
                                            (month + 1) - g_date_time_get_month (start_dt),
                                            day - g_date_time_get_day_of_month (start_dt),
                                            0, 0, 0);

          if (!g_date_time_equal (start_dt, drop_date))
            {
              GTimeSpan diff = g_date_time_difference (drop_date, start_dt);
              GDateTime *new_start;

              new_start = g_date_time_add (start_dt, diff);
              gcal_event_set_date_start (event, new_start);

              /* The event may have a NULL end date, so we have to check it here */
              if (end_dt)
                {
                  GDateTime *new_end;

                  new_end = g_date_time_add (end_dt, diff);
                  gcal_event_set_date_end (event, new_end);

                  g_clear_pointer (&new_end, g_date_time_unref);
                }

              gcal_manager_update_event (self->manager, event, mod);

              g_clear_pointer (&new_start, g_date_time_unref);
            }

          g_clear_pointer (&drop_date, g_date_time_unref);
        }
    }

out:
  /* Cancel the DnD after the event is dropped */
  self->selected_data->dnd_day = -1;
  self->selected_data->dnd_month = -1;

  gtk_drag_finish (context, TRUE, FALSE, time);

  gtk_widget_queue_draw (navigator);

  return TRUE;
}

static void
navigator_drag_leave_cb (GcalYearView   *self,
                         GdkDragContext *context,
                         guint           time,
                         GtkWidget      *navigator)
{
  /* Cancel the DnD after the event is dropped */
  self->selected_data->dnd_day = -1;
  self->selected_data->dnd_month = -1;

  gtk_widget_queue_draw (navigator);
}

/* GcalView implementation */
static icaltimetype*
gcal_year_view_get_date (GcalView *view)
{
  GcalYearView *self = GCAL_YEAR_VIEW (view);

  return self->date;
}

static void
gcal_year_view_set_date (GcalView     *view,
                         icaltimetype *date)
{
  GCAL_ENTRY;

  update_date (GCAL_YEAR_VIEW (view), date);

  GCAL_EXIT;
}

static GList*
gcal_year_view_get_children_by_uuid (GcalView              *view,
                                     GcalRecurrenceModType  mod,
                                     const gchar           *uuid)
{
  GcalYearView *self = GCAL_YEAR_VIEW (view);
  GList *result, *children;

  result = NULL;

  children = gtk_container_get_children (GTK_CONTAINER (self->sidebar));
  result = filter_event_list_by_uid_and_modtype (children, mod, uuid);

  g_list_free (children);

  return result;
}

static void
gcal_view_interface_init (GcalViewInterface *iface)
{
  /* FIXME: implement what's needed */
  iface->get_date = gcal_year_view_get_date;
  iface->set_date = gcal_year_view_set_date;
  iface->get_children_by_uuid = gcal_year_view_get_children_by_uuid;
}

/* Called when self is destroyed.
 *
 * WARNING: This is a workaround. This is one of the views where
 * reference counts are off. We overwrite destroy to avoid dangling
 * pointers. This issue needs to be addressed and this function removed.
 */
static void
gcal_year_view_destroyed (GtkWidget *widget)
{
  GcalYearView *self = GCAL_YEAR_VIEW (widget);

  if (self->weather_service != NULL)
    {
      g_signal_handlers_disconnect_by_func (self->weather_service,
                                            (GCallback) weather_changed,
                                            self);
      g_clear_object (&self->weather_service);
    }

  GTK_WIDGET_CLASS (year_view_parent_class)->destroy ((GtkWidget*) G_TYPE_CHECK_INSTANCE_CAST (self, GTK_TYPE_BOX, GtkGrid));
}

static void
gcal_year_view_finalize (GObject *object)
{
  GcalYearView *year_view = GCAL_YEAR_VIEW (object);
  guint i;

  if (year_view->weather_service != NULL)
    {
      g_signal_connect (year_view->weather_service,
                        "weather-changed",
                        (GCallback) weather_changed,
                        year_view);
      g_clear_object (&year_view->weather_service);
    }

  g_free (year_view->navigator_grid);
  g_free (year_view->selected_data);

  g_free (year_view->start_selected_date);
  g_free (year_view->end_selected_date);

  g_clear_pointer (&year_view->date, g_free);

  g_clear_object (&year_view->calendar_settings);

  for (i = 0; i < 12; i++)
    g_clear_pointer (&year_view->events[i], g_ptr_array_unref);

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

    case PROP_MANAGER:
      g_value_set_object (value, self->manager);
      break;

    case PROP_WEATHER_SERVICE:
      g_value_set_object (value, gcal_year_view_get_weather_service (self));
      break;

    case PROP_SHOW_WEEK_NUMBERS:
      g_value_set_boolean (value, self->show_week_numbers);
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
  GcalYearView *self = GCAL_YEAR_VIEW (object);

  switch (prop_id)
    {
    case PROP_DATE:
      gcal_view_set_date (GCAL_VIEW (self), g_value_get_boxed (value));
      break;

    case PROP_MANAGER:
      self->manager = g_value_dup_object (value);

      g_signal_connect_swapped (gcal_manager_get_clock (self->manager),
                                "day-changed",
                                G_CALLBACK (gtk_widget_queue_draw),
                                self->navigator);

      g_object_notify (object, "manager");
      break;

    case PROP_WEATHER_SERVICE:
      gcal_year_view_set_weather_service (self, g_value_get_object (value));
      break;

    case PROP_SHOW_WEEK_NUMBERS:
      if (self->show_week_numbers != g_value_get_boolean (value))
        {
          self->show_week_numbers = g_value_get_boolean (value);
          calculate_grid_sizes (self);
          g_object_notify (object, "show-week-numbers");
          gtk_widget_queue_allocate (GTK_WIDGET (self));
        }
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
  gint padding_left, padding_right, hpadding;

  context = gtk_widget_get_style_context (year_view->navigator);

  gtk_style_context_get (context,
                         gtk_style_context_get_state (context),
                         "padding-left", &padding_left,
                         "padding-right", &padding_right, NULL);

  hpadding = padding_left + padding_right;
  if (minimum != NULL)
    *minimum = hpadding + year_view->column_width;
  if (natural != NULL)
    *natural = hpadding + year_view->column_width + year_view->sidebar_width;
}

static void
gcal_year_view_size_allocate (GtkWidget     *widget,
                              GtkAllocation *alloc)
{
  GcalYearView *year_view = GCAL_YEAR_VIEW (widget);
  GtkStyleContext *context;
  gint padding_left, padding_right, padding_top, padding_bottom, hpadding, vpadding;
  gint number_of_rows;

  context = gtk_widget_get_style_context (widget);
  gtk_style_context_save (context);
  gtk_style_context_add_class (context, "year-navigator");
  gtk_style_context_get (context,
                         gtk_style_context_get_state (context),
                         "padding-left", &padding_left,
                         "padding-right", &padding_right,
                         "padding-top", &padding_top,
                         "padding-bottom", &padding_bottom, NULL);
  gtk_style_context_restore (context);

  hpadding = padding_left + padding_right;
  vpadding = padding_top + padding_bottom;

  year_view->popover_mode = (alloc->width < (hpadding + year_view->column_width * 2 + year_view->sidebar_width));

  year_view->number_of_columns = (alloc->width - hpadding - year_view->sidebar_width * !year_view->popover_mode) / year_view->column_width;

  if (year_view->number_of_columns > NAVIGATOR_MAX_GRID_SIZE)
    year_view->number_of_columns = NAVIGATOR_MAX_GRID_SIZE;

  number_of_rows = ceil (12.0 / year_view->number_of_columns);

  gtk_widget_set_size_request (year_view->navigator,
                               hpadding + year_view->column_width * year_view->number_of_columns,
                               vpadding + year_view->row_height * number_of_rows + year_view->header_height);

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

static void
gcal_year_view_component_added (ECalDataModelSubscriber *subscriber,
                                ECalClient              *client,
                                ECalComponent           *comp)
{
  GcalYearView *self = GCAL_YEAR_VIEW (subscriber);
  GDateTime *event_start, *event_end;
  GcalEvent *event;
  GError *error;
  guint i, start_month, end_month;

  error = NULL;
  event = gcal_event_new (e_client_get_source (E_CLIENT (client)), comp, &error);

  if (error)
    {
      g_warning ("Error creating event: %s", error->message);
      g_clear_error (&error);
      return;
    }

  event_start = gcal_event_get_date_start (event);
  event_end = gcal_event_get_date_end (event);

  /* Calculate the start & end months */
  start_month = g_date_time_get_month (event_start) - 1;
  end_month = g_date_time_get_month (event_end) - 1;

  if (g_date_time_get_year (event_start) < self->date->year)
    start_month = 0;

  if (g_date_time_get_year (event_end) > self->date->year)
    end_month = 11;

  /* Add the event to the cache */
  for (i = start_month; i <= end_month; i++)
    g_ptr_array_add (self->events[i], g_object_ref (event));

  update_sidebar (self);

  gtk_widget_queue_draw (GTK_WIDGET (self->navigator));
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
  guint i;
  gint number_of_children;

  source = e_client_get_source (E_CLIENT (client));
  if (rid != NULL)
    uuid = g_strdup_printf ("%s:%s:%s", e_source_get_uid (source), uid, rid);
  else
    uuid = g_strdup_printf ("%s:%s", e_source_get_uid (source), uid);

  children = gtk_container_get_children (GTK_CONTAINER (year_view->events_sidebar));
  number_of_children = g_list_length (children);

  for (l = children; l != NULL; l = g_list_next (l))
    {
      GcalEventWidget *child_widget;
      GcalEvent *event;

      child_widget = GCAL_EVENT_WIDGET (gtk_bin_get_child (GTK_BIN (l->data)));
      event = gcal_event_widget_get_event (child_widget);

      if (g_strcmp0 (uuid, gcal_event_get_uid (event)) == 0)
        {
          gtk_widget_destroy (GTK_WIDGET (l->data));
          number_of_children--;
        }
    }

  /*
   * No children left visible, all the events were removed and now we have to show the
   * 'No Events' placeholder.
   */
  if (number_of_children == 0)
    {
      update_no_events_page (GCAL_YEAR_VIEW (subscriber));
      gtk_stack_set_visible_child_name (GTK_STACK (year_view->navigator_stack), "no-events");
    }

  /* Also remove from the cached list of events */
  for (i = 0; i < 12; i++)
    {
      GPtrArray *events;
      guint j;

      events = year_view->events[i];

      for (j = 0; j < events->len; j++)
        {
          GcalEvent *event;

          event = g_ptr_array_index (events, j);

          if (g_strcmp0 (gcal_event_get_uid (event), uuid) != 0)
            continue;

          g_ptr_array_remove (events, event);
        }
    }

  gtk_widget_queue_draw (GTK_WIDGET (year_view->navigator));

  g_list_free (children);
  g_free (uuid);
}

static void
gcal_year_view_component_changed (ECalDataModelSubscriber *subscriber,
                                  ECalClient              *client,
                                  ECalComponent           *comp)
{
  ECalComponentId *id;

  id = e_cal_component_get_id (comp);

  gcal_year_view_component_removed (subscriber, client, id->uid, id->rid);
  gcal_year_view_component_added (subscriber, client, comp);

  g_clear_pointer (&id, e_cal_component_free_id);
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
  widget_class->size_allocate = gcal_year_view_size_allocate;
  widget_class->direction_changed = gcal_year_view_direction_changed;

  g_object_class_override_property (object_class, PROP_DATE, "active-date");
  g_object_class_override_property (object_class, PROP_MANAGER, "manager");

  /**
   * GcalWeekView:weather-service:
   *
   * Sets the weather service to use.
   */
  g_object_class_install_property
      (object_class,
       PROP_WEATHER_SERVICE,
       g_param_spec_object ("weather-service", "weather-service", "weather-service",
                            GCAL_TYPE_WEATHER_SERVICE,
                            G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_SHOW_WEEK_NUMBERS,
                                   g_param_spec_boolean ("show-week-numbers",
                                                         "Show Week Numbers",
                                                         "Show Week Numbers Column",
                                                         FALSE,
                                                         G_PARAM_READWRITE));

  /* FIXME: it will problably go back to GcalView */
  signals[EVENT_ACTIVATED] = g_signal_new ("event-activated",
                                           GCAL_TYPE_YEAR_VIEW,
                                           G_SIGNAL_RUN_LAST,
                                           0, NULL, NULL, NULL,
                                           G_TYPE_NONE,
                                           1,
                                           GCAL_TYPE_EVENT_WIDGET);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/year-view.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalYearView, navigator);
  gtk_widget_class_bind_template_child (widget_class, GcalYearView, sidebar);
  gtk_widget_class_bind_template_child (widget_class, GcalYearView, events_sidebar);
  gtk_widget_class_bind_template_child (widget_class, GcalYearView, navigator_stack);
  gtk_widget_class_bind_template_child (widget_class, GcalYearView, navigator_sidebar);
  gtk_widget_class_bind_template_child (widget_class, GcalYearView, no_events_title);
  gtk_widget_class_bind_template_child (widget_class, GcalYearView, popover);
  gtk_widget_class_bind_template_child (widget_class, GcalYearView, scrolled_window);
  gtk_widget_class_bind_template_child (widget_class, GcalYearView, temp_label);
  gtk_widget_class_bind_template_child (widget_class, GcalYearView, weather_icon);

  gtk_widget_class_bind_template_callback (widget_class, draw_navigator);
  gtk_widget_class_bind_template_callback (widget_class, navigator_button_press_cb);
  gtk_widget_class_bind_template_callback (widget_class, navigator_button_release_cb);
  gtk_widget_class_bind_template_callback (widget_class, navigator_drag_drop_cb);
  gtk_widget_class_bind_template_callback (widget_class, navigator_drag_leave_cb);
  gtk_widget_class_bind_template_callback (widget_class, navigator_drag_motion_cb);
  gtk_widget_class_bind_template_callback (widget_class, navigator_edge_overshot_cb);
  gtk_widget_class_bind_template_callback (widget_class, navigator_motion_notify_cb);
  gtk_widget_class_bind_template_callback (widget_class, navigator_scroll_event_cb);
  gtk_widget_class_bind_template_callback (widget_class, add_event_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, popover_closed_cb);

  gtk_widget_class_set_css_name (widget_class, "calendar-view");

  /* Hack to deal with broken reference counts: */
  year_view_parent_class = g_type_class_peek_parent (klass);
  ((GtkWidgetClass *) klass)->destroy = (void (*) (GtkWidget *)) gcal_year_view_destroyed;
}

static void
gcal_year_view_init (GcalYearView *self)
{
  guint i;

  self->weather_service = NULL;

  for (i = 0; i < 12; i++)
    self->events[i] = g_ptr_array_new_with_free_func (g_object_unref);

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

  /* bind GNOME Shell' show week numbers property to GNOME Calendar's one */
  self->calendar_settings = g_settings_new ("org.gnome.desktop.calendar");
  g_settings_bind (self->calendar_settings, "show-weekdate", self, "show-week-numbers", G_SETTINGS_BIND_DEFAULT);
  g_signal_connect_swapped (self->calendar_settings, "changed::show-weekdate", G_CALLBACK (gtk_widget_queue_draw), self);

  /* layout */
  self->number_of_columns = 4;
  calculate_sizes (self);

  gtk_list_box_set_header_func (GTK_LIST_BOX (self->events_sidebar), update_sidebar_headers, self, NULL);
  gtk_list_box_set_sort_func (GTK_LIST_BOX (self->events_sidebar), sidebar_sort_func, NULL, NULL);

  /* The year navigator is a DnD destination */
  gtk_drag_dest_set (self->navigator,
                     0,
                     NULL,
                     0,
                     GDK_ACTION_MOVE);
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

static void
update_weather (GcalYearView *self)
{
  gboolean updated = FALSE;

  g_return_if_fail (GCAL_IS_YEAR_VIEW (self));


  if (self->weather_service != NULL && self->date != NULL)
    {
      GSList *witer;  /* unowned */
      GDate   date;

      g_date_set_dmy (&date, self->date->day, self->date->month, self->date->year);

      witer = gcal_weather_service_get_weather_infos (self->weather_service);
      for (; witer != NULL; witer = witer->next)
        {
          GcalWeatherInfo *info; /* unowned */
          GDate wdate;

          info = GCAL_WEATHER_INFO (witer->data);
          gcal_weather_info_get_date (info, &wdate);
          if (g_date_compare (&date, &wdate) == 0)
            {
              const gchar *temp_str; /* unowned */
              const gchar *icon_nm;  /* unowned */

              temp_str = gcal_weather_info_get_temperature (info);
              icon_nm = gcal_weather_info_get_icon_name (info);

              gtk_label_set_text (self->temp_label, temp_str);
              gtk_image_set_from_icon_name (self->weather_icon, icon_nm, GTK_ICON_SIZE_SMALL_TOOLBAR);
              updated = TRUE;
              break;
            }
        }
    }

  if (!updated)
    {
      gtk_label_set_text (self->temp_label, "");
      gtk_image_clear (self->weather_icon);
    }
}

/* Public API */
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

/**
 * gcal_year_view_set_weather_service:
 * @self: The #GcalYearView instance.
 * @service: (nullable): The weather service to query.
 *
 * Sets the service to query for weather reports.
 */
void
gcal_year_view_set_weather_service (GcalYearView       *self,
                                    GcalWeatherService *service)
{
  g_return_if_fail (GCAL_IS_YEAR_VIEW (self));
  g_return_if_fail (service == NULL || GCAL_IS_WEATHER_SERVICE (service));

  if (self->weather_service != service)
    {
      if (self->weather_service != NULL)
        g_signal_handlers_disconnect_by_func (self->weather_service,
                                              (GCallback) weather_changed,
                                              self);

      g_set_object (&self->weather_service, service);

      if (self->weather_service != NULL)
        g_signal_connect (self->weather_service,
                          "weather-changed",
                          (GCallback) weather_changed,
                          self);

      update_weather (self);
      g_object_notify (G_OBJECT (self), "weather-service");
    }
}

/**
 * gcal_year_view_get_weather_service:
 * @self: The #GcalYearView instance.
 *
 * Returns: (transfer none): The internal weather service object.
 */
GcalWeatherService*
gcal_year_view_get_weather_service (GcalYearView *self)
{
  g_return_val_if_fail (GCAL_IS_YEAR_VIEW (self), NULL);

  return self->weather_service;
}
