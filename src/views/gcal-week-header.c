/* gcal-week-header.c
 *
 * Copyright (C) 2016 Vamsi Krishna Gollapudi <pandu.sonu@yahoo.com>
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

#define ALL_DAY_CELLS_HEIGHT 40

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
  GtkWidget        *expand_button_image;

  GcalManager      *manager;

  /*
   * Stores the events as they come from the week-view
   * The list will later be iterated after the active date is changed
   * and the events will be placed
   */
  GList            *events;

  gint              first_weekday;

  /*
   * Used for checking if the header is in collapsed state or expand state
   * false is collapse state true is expand state
   */
  gboolean          expanded;

  gboolean          use_24h_format;

  icaltimetype     *active_date;
  icaltimetype     *current_date;
};

static GDateTime*     get_start_of_week                     (GcalWeekHeader *self);

static gint           events_compare_func                   (GcalEvent *first,
                                                             GcalEvent *second);

static void           update_headers                        (GcalWeekHeader *self);

static void           place_events                          (GcalWeekHeader *self);

static void           clear_current_events                  (GcalWeekHeader *self);

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

G_DEFINE_TYPE (GcalWeekHeader, gcal_week_header, GTK_TYPE_GRID);

static GDateTime*
get_start_of_week (GcalWeekHeader *self)
{
  icaltimetype *new_date;

  g_return_val_if_fail (GCAL_IS_WEEK_HEADER (self), NULL);
  new_date = g_new0 (icaltimetype, 1);
  *new_date = icaltime_from_day_of_year (icaltime_start_doy_week (*(self->active_date),
                                                                  self->first_weekday + 1),
                                                                  self->active_date->year);
  new_date->is_date = 0;
  new_date->hour = 0;
  new_date->minute = 0;
  new_date->second = 0;

  *new_date = icaltime_set_timezone (new_date, self->active_date->zone);

  return icaltime_to_datetime (new_date);
}

static gint
events_compare_func (GcalEvent *first,
                     GcalEvent *second)
{
  gint retval;

  /* Multiday events should come before single day events */
  if (gcal_event_is_multiday (first) != gcal_event_is_multiday (second))
    return gcal_event_is_multiday (second) - gcal_event_is_multiday (first);

  /* Compare with respect to start day */
  retval = g_date_time_compare (gcal_event_get_date_start (first),
                                gcal_event_get_date_start (second));

  if (retval != 0)
    return retval;

  /* Compare with respect to end day effectively comparing with respect to duration */
  return g_date_time_compare (gcal_event_get_date_end (second), gcal_event_get_date_end (first));
}

static void
update_headers (GcalWeekHeader *self)
{
  GDateTime *week_start, *week_end;
  gchar *year_label, *month_label, *week_label;

  if(!self->active_date)
    return;

  week_start = get_start_of_week (self);
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
place_events (GcalWeekHeader *self)
{
  GList *l, *iter;
  GDateTime *week_start, *week_end;
  gint i;
  gint hidden_events[7];

  if (!self->active_date)
    return;

  week_start = get_start_of_week (self);
  week_end = g_date_time_add_days (week_start, 6);
  iter = g_list_copy (self->events);

  for (i = 0; i < 7; i++)
    hidden_events[i] = 0;

  iter = g_list_sort (iter, (GCompareFunc) events_compare_func);

  for (l = iter; l != NULL; l = l->next)
    {
      GcalEvent *event;
      GtkWidget *widget;
      GDateTime *event_start, *event_end;
      gint start, end;
      gboolean placed;

      start = -1;
      end = -1;
      placed = FALSE;
      event = GCAL_EVENT (l->data);
      widget = gcal_event_widget_new (event);
      event_start = gcal_event_get_date_start (event);
      event_end = gcal_event_get_date_end (event);

      gtk_widget_set_visible (widget, TRUE);

      for (i = 0; i < 7; i++)
        {
          GDateTime *day_start, *day_end;

          day_start = g_date_time_add_days (week_start, i);
          day_end = g_date_time_add_days (day_start, 1);

          /* Checks if the event is present on the i-th day of the week */
          if ((g_date_time_compare (event_start, day_start) < 0 && g_date_time_compare (event_end, day_end) >= 0) ||
              (g_date_time_compare (event_start, day_start) >= 0 && g_date_time_compare (event_end, day_end) < 0) ||
              (g_date_time_compare (event_start, day_start) >= 0 && g_date_time_compare (event_start, day_end) < 0) ||
              (g_date_time_compare (event_end, day_start) > 0 && g_date_time_compare (event_end, day_end) < 0))
            {
              if (start == -1)
                start = i;

              end = i;
            }
        }

      self->events = g_list_remove_all (self->events, l->data);

      if (start == -1)
        continue;

      for (i = 1 ; ; i++)
        {
          gint j;
          gboolean present;

          if (!(self->expanded) && i >=4)
            break;

          present = FALSE;

          /*
           * Checks if the columns required for the event
           * are free in the i-th row
           */
          for (j = start; j < end + 1; j++)
            present = present || gtk_grid_get_child_at (GTK_GRID (self->grid), j, i);

          if (!present)
            {
              gcal_event_widget_set_date_start (GCAL_EVENT_WIDGET (widget),
                                                MAX (week_start, event_start));
              gcal_event_widget_set_date_end (GCAL_EVENT_WIDGET (widget),
                                              MIN(week_end, event_end));

              gtk_grid_attach (GTK_GRID (self->grid), widget, start, i, end - start + 1, 1);
              placed = TRUE;
              break;
            }
        }

      if (!placed)
        {
          for (i = start; i <= end; i++)
            hidden_events[i]++;

          self->events = g_list_append (self->events, event);
        }
    }

    if (!(self->expanded))
      {
        gtk_grid_remove_row (GTK_GRID (self->grid), 4);

        for (i = 0; i < 7; i++)
          {
            if (hidden_events[i])
              {
                GtkWidget *widget;

                widget = gtk_label_new (g_strdup_printf (g_dngettext (GETTEXT_PACKAGE, "Other event", "Other %d events", hidden_events[i]),
                                                         hidden_events[i]));

                gtk_widget_set_visible (widget, TRUE);
                gtk_grid_attach (GTK_GRID (self->grid), widget, i, 4, 1, 1);
              }
          }
      }

    g_list_free (iter);
}

static void
clear_current_events (GcalWeekHeader *self)
{
  GList *children, *l;

  children = g_list_copy (gtk_container_get_children (GTK_CONTAINER (self->grid)));

  for (l = children; l != NULL; l = l->next)
    {
      if (!GCAL_IS_EVENT_WIDGET (l->data))
        continue;

      /* Add current events to events list to add later if necessary */
      self->events = g_list_append (self->events,
                                    gcal_event_widget_get_event (l->data));

      gtk_widget_destroy (l->data);
    }

  g_list_free (children);
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

            self->events = g_list_append (self->events, gcal_event_widget_get_event (widget));

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

          widget = gtk_label_new (g_strdup_printf (g_dngettext (GETTEXT_PACKAGE, "Other event", "Other %d events", hidden_events[i]),
                                                   hidden_events[i]));

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
  place_events (self);
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

  g_clear_pointer (&self->current_date, g_free);
  g_list_free (self->events);
}

void
gcal_week_header_add_event (GcalWeekHeader *self,
                            GcalEvent      *event)
{
  self->events = g_list_append (self->events, event);
}

void
gcal_week_header_remove_event (GcalWeekHeader *self,
                               gchar          *uuid)
{
  GList *children, *l, *iter;

  children = gtk_container_get_children (GTK_CONTAINER (self->grid));

  for (l = children; l != NULL; l = g_list_next (l))
    {
      GcalEventWidget *child_widget;
      GcalEvent *event;

      if (!GCAL_IS_EVENT_WIDGET(l->data))
        continue;

      child_widget = GCAL_EVENT_WIDGET (l->data);
      event = gcal_event_widget_get_event (child_widget);

      if(child_widget != NULL && g_strcmp0 (uuid, gcal_event_get_uid (event)) == 0)
        gtk_widget_destroy (GTK_WIDGET (l->data));
    }

  iter = g_list_copy (self->events);

  for (l = iter; l != NULL; l = g_list_next (l))
    {
      if (!GCAL_IS_EVENT (l->data))
        continue;

      if (l->data != NULL && g_strcmp0 (uuid, gcal_event_get_uid (l->data)) == 0)
        self->events = g_list_remove_all (self->events, l->data);
    }

  g_list_free (children);
  g_list_free (iter);
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

  switch (prop_id)
    {
    case PROP_ACTIVE_DATE:
      g_clear_pointer (&self->active_date, g_free);
      self->active_date = g_value_dup_boxed (value);

      update_headers (self);
      clear_current_events (self);
      place_events (self);

      gtk_widget_queue_draw (self->draw_area);

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
  GtkStyleContext *context;
  GtkStateFlags state;
  GtkAllocation draw_alloc;

  gdouble sidebar_width, cell_width, button_width;

  PangoFontDescription *bold_font;

  context = gtk_widget_get_style_context (self->draw_area);
  state = gtk_widget_get_state_flags (self->draw_area);
  sidebar_width = gcal_week_view_get_sidebar_width (self->draw_area);
  button_width = gtk_widget_get_allocated_width (self->expand_button);

  gtk_widget_get_allocation (self->draw_area, &draw_alloc);

  cell_width = (draw_alloc.width - sidebar_width) / 7;

  gtk_style_context_get (context, state, "font", &bold_font, NULL);
  pango_font_description_set_weight (bold_font, PANGO_WEIGHT_SEMIBOLD);

  gtk_widget_set_margin_start (self->scrolledwindow,
                               gcal_week_view_get_sidebar_width (self->draw_area) + 1 - button_width);

  gtk_widget_set_margin_end (self->scrolledwindow,
                             gtk_widget_get_allocated_width (self->draw_area) - cell_width * 7 - sidebar_width + 7);

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

  gint i, pos_i, start_grid_y;
  gint font_height;
  gdouble sidebar_width, cell_width;
  icaltimetype *start_of_week, *end_of_week;

  cairo_pattern_t *pattern;

  cairo_save(cr);
  start_grid_y = gcal_week_view_get_start_grid_y (widget);

  /* Fonts and colour selection */
  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);

  gtk_style_context_get_padding (context, state, &padding);
  gtk_widget_get_allocation (widget, &alloc);

  pattern = cairo_pattern_create_linear (0, start_grid_y - 18,
                                         0, start_grid_y + 6);

  cairo_pattern_add_color_stop_rgba (pattern, 0.0, 0, 0, 0, 0.6);
  cairo_pattern_add_color_stop_rgba (pattern, 1.0, 0, 0, 0, 0.0);

  cairo_set_source (cr, pattern);
  cairo_pattern_destroy (pattern);

  gtk_style_context_get_color (context, state, &color);
  gdk_cairo_set_source_rgba (cr, &color);

  layout = pango_cairo_create_layout (cr);
  gtk_style_context_get (context, state, "font", &bold_font, NULL);
  pango_font_description_set_weight (bold_font, PANGO_WEIGHT_MEDIUM);
  pango_layout_set_font_description (layout, bold_font);

  week_start = get_start_of_week (self);
  week_end = g_date_time_add_days (week_start, 6);
  current_cell = icaltime_day_of_week (*(self->active_date)) - 1;
  current_cell = (7 + current_cell - self->first_weekday) % 7;

  sidebar_width = gcal_week_view_get_sidebar_width (widget);
  cell_width = (alloc.width - sidebar_width) / 7;
  pango_layout_get_pixel_size (layout, NULL, &font_height);

  gtk_style_context_save (context);
  gtk_style_context_add_class (context, "current");
  gtk_render_background (context, cr,
                         cell_width * current_cell + sidebar_width,
                         font_height + padding.bottom,
                         cell_width,
                         ALL_DAY_CELLS_HEIGHT);
  gtk_style_context_remove_class (context, "current");
  gtk_style_context_restore (context);

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
                     padding.left + cell_width * i + sidebar_width,
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
                     padding.left + cell_width * i + sidebar_width,
                     0);
      pango_cairo_show_layout (cr, layout);

      gtk_style_context_restore (context);

      /* Draws the lines after each day of the week */
      cairo_save (cr);
      cairo_move_to (cr,
                     cell_width * i + sidebar_width - 3,
                     font_height + padding.bottom + 3);
      cairo_set_line_width (cr, 0.25);
      cairo_rel_line_to (cr, 0.0, gtk_widget_get_allocated_height (self->draw_area));
      cairo_stroke (cr);
      cairo_restore (cr);

      g_free (weekday_date);
      g_free (weekday_abv);
    }

  cairo_save (cr);
  cairo_move_to (cr,
                 cell_width * 7 + sidebar_width - 3,
                 font_height + padding.bottom + 3);
  cairo_set_line_width (cr, 0.25);
  cairo_rel_line_to (cr, 0.0, gtk_widget_get_allocated_height (self->draw_area));
  cairo_stroke (cr);
  cairo_restore (cr);

  gtk_style_context_add_class (context, "margin");

  gtk_style_context_get_color (context, state, &color);
  gdk_cairo_set_source_rgba (cr, &color);
  cairo_move_to (cr, 0, gtk_widget_get_allocated_height (self->draw_area));
  cairo_rel_line_to (cr, gtk_widget_get_allocated_width (self->draw_area), 0);

  cairo_stroke (cr);

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

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/week-header.ui");

  g_object_class_install_property (object_class,
                                   PROP_ACTIVE_DATE,
                                   g_param_spec_boxed ("active-date",
                                                       "Date",
                                                       "The active selected date",
                                                       ICAL_TIME_TYPE,
                                                       G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  gtk_widget_class_bind_template_child (widget_class, GcalWeekHeader, grid);
  gtk_widget_class_bind_template_child (widget_class, GcalWeekHeader, month_label);
  gtk_widget_class_bind_template_child (widget_class, GcalWeekHeader, year_label);
  gtk_widget_class_bind_template_child (widget_class, GcalWeekHeader, week_label);
  gtk_widget_class_bind_template_child (widget_class, GcalWeekHeader, draw_area);
  gtk_widget_class_bind_template_child (widget_class, GcalWeekHeader, scrolledwindow);
  gtk_widget_class_bind_template_child (widget_class, GcalWeekHeader, expand_button);
  gtk_widget_class_bind_template_child (widget_class, GcalWeekHeader, expand_button_image);

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
gcal_week_header_set_current_date (GcalWeekHeader *self,
                                   icaltimetype   *current_date)
{
  g_return_if_fail (GCAL_IS_WEEK_HEADER (self));

  g_clear_pointer (&self->current_date, g_free);
  self->current_date = gcal_dup_icaltime (current_date);

  update_headers (self);
  clear_current_events (self);
  place_events (self);

  gtk_widget_queue_draw (self->draw_area);
}
