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

#include "gcal-month-view.h"
#include "gcal-subscriber-view-private.h"
#include "gcal-utils.h"
#include "gcal-view.h"

#include <glib/gi18n.h>

#include <math.h>

typedef struct
{
  GtkWidget      *overflow_popover;
  GtkWidget      *events_list_box;
  GtkWidget      *popover_title;

  GdkWindow      *event_window;

  /**
   * the cell on which its drawn the first day of the month, in the first row, 0 for the first cell, 1 for the second,
   * and so on, this takes first_weekday into account already.
   */
  gint            days_delay;

  /**
   * first day of the week according to user locale, being
   * 0 for Sunday, 1 for Monday and so on */
  gint            first_weekday;

  /**
   * button_down/up flag
   * These keeps the cell index, meaning 0 for left top, 1 for the next in the row from left to right, etc.
   * Note that this does not take into account the disabled row before the first active row.
   */
  gint            start_mark_cell;
  gint            end_mark_cell;

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
  gint            k;

  icaltimetype   *current_date;

  /* property */
  icaltimetype   *date;
} GcalMonthViewPrivate;

enum
{
  PROP_0,
  PROP_DATE,  /* active-date inherited property */
};

static gint           gather_button_event_data              (GcalMonthView  *view,
                                                             gdouble         x,
                                                             gdouble         y,
                                                             gboolean       *out_on_indicator,
                                                             gdouble        *out_x,
                                                             gdouble        *out_y);

static gdouble        get_start_grid_y                      (GtkWidget      *widget);

static gboolean       get_widget_parts                      (gint            first_cell,
                                                             gint            last_cell,
                                                             gint            natural_height,
                                                             gdouble         vertical_cell_space,
                                                             gdouble        *size_left,
                                                             GArray         *cells,
                                                             GArray         *lengths);

static void           rebuild_popover_for_day               (GcalMonthView  *view,
                                                             gint            day);

static void           update_list_box_headers               (GtkListBoxRow  *row,
                                                             GtkListBoxRow  *before,
                                                             gpointer        user_data);

static void           add_new_event_button_cb               (GtkWidget      *button,
                                                             gpointer        user_data);

static void           gcal_view_interface_init              (GcalViewIface  *iface);

static void           gcal_month_view_set_property          (GObject        *object,
                                                             guint           property_id,
                                                             const GValue   *value,
                                                             GParamSpec     *pspec);

static void           gcal_month_view_get_property          (GObject        *object,
                                                             guint           property_id,
                                                             GValue         *value,
                                                             GParamSpec     *pspec);

static void           gcal_month_view_finalize              (GObject        *object);

static void           gcal_month_view_realize               (GtkWidget      *widget);

static void           gcal_month_view_unrealize             (GtkWidget      *widget);

static void           gcal_month_view_map                   (GtkWidget      *widget);

static void           gcal_month_view_unmap                 (GtkWidget      *widget);

static void           gcal_month_view_size_allocate         (GtkWidget      *widget,
                                                             GtkAllocation  *allocation);

static gboolean       gcal_month_view_draw                  (GtkWidget      *widget,
                                                             cairo_t        *cr);

static gboolean       gcal_month_view_button_press          (GtkWidget      *widget,
                                                             GdkEventButton *event);

static gboolean       gcal_month_view_motion_notify_event   (GtkWidget      *widget,
                                                             GdkEventMotion *event);

static gboolean       gcal_month_view_button_release        (GtkWidget      *widget,
                                                             GdkEventButton *event);

static void           gcal_month_view_direction_changed     (GtkWidget        *widget,
                                                             GtkTextDirection  previous_direction);

static gboolean       gcal_month_view_is_child_multiday     (GcalSubscriberView  *subscriber,
                                                             GcalEventWidget     *child);

static guint          gcal_month_view_get_child_cell        (GcalSubscriberView *subscriber,
                                                             GcalEventWidget    *child);

static void           gcal_month_view_clear_state           (GcalSubscriberView *subscriber);

static icaltimetype*  gcal_month_view_get_initial_date      (GcalView       *view);

static icaltimetype*  gcal_month_view_get_final_date        (GcalView       *view);

static void           gcal_month_view_clear_marks           (GcalView       *view);

static GList*         gcal_month_view_get_children_by_uuid  (GcalView       *view,
                                                             const gchar    *uuid);

G_DEFINE_TYPE_WITH_CODE (GcalMonthView, gcal_month_view,GCAL_TYPE_SUBSCRIBER_VIEW,
                         G_ADD_PRIVATE (GcalMonthView)
                         G_IMPLEMENT_INTERFACE (GCAL_TYPE_VIEW, gcal_view_interface_init));

static gint
gather_button_event_data (GcalMonthView *view,
                          gdouble        x,
                          gdouble        y,
                          gboolean      *out_on_indicator,
                          gdouble       *out_x,
                          gdouble       *out_y)
{
  GcalMonthViewPrivate *priv;
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
  gint font_height, padding_bottom;

  priv = gcal_month_view_get_instance_private (view);
  widget = GTK_WIDGET (view);

  start_grid_y = get_start_grid_y (widget);

  shown_rows = ceil ((priv->days_delay + icaltime_days_in_month (priv->date->month, priv->date->year)) / 7.0);
  first_row_gap = (6 - shown_rows) * 0.5; /* invalid area before the actual rows */

  cell_width = gtk_widget_get_allocated_width (widget) / 7.0;
  cell_height = (gtk_widget_get_allocated_height (widget) - start_grid_y) / 6.0;

  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);
  gtk_style_context_save (context);
  gtk_style_context_add_class (context, "overflow");
  gtk_style_context_get (context, state, "font", &ofont_desc, "padding-bottom", &padding_bottom, NULL);

  overflow_layout = gtk_widget_create_pango_layout (widget, NULL);
  pango_layout_set_font_description (overflow_layout, ofont_desc);
  pango_layout_get_pixel_size (overflow_layout, NULL, &font_height);

  gtk_style_context_restore (context);
  pango_font_description_free (ofont_desc);
  g_object_unref (overflow_layout);

  y = y - start_grid_y - first_row_gap * cell_height;

  cell = 7 * (gint)(y / cell_height) + (gint)(x / cell_width);

  if (out_on_indicator != NULL)
    {
      gdouble upper_border = cell_height * ((gint) y / (gint) cell_height);
      y = y - upper_border;

      *out_on_indicator = (cell_height - font_height - padding_bottom < y && y < cell_height);
    }

  if (out_x != NULL)
    *out_x = cell_width * ((cell % 7) + 0.5);

  if (out_y != NULL)
    *out_y = cell_height * ((cell / 7) + first_row_gap + 0.5) + start_grid_y;

  return cell;
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
  state_flags = gtk_widget_get_state_flags (widget);

  layout = gtk_widget_create_pango_layout (widget, NULL);

  gtk_style_context_save (context);
  gtk_style_context_add_class (context, "first-view-header");
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

static gboolean
get_widget_parts (gint     first_cell,
                  gint     last_cell,
                  gint     natural_height,
                  gdouble  vertical_cell_space,
                  gdouble *size_left,
                  GArray  *cells,
                  GArray  *lengths)
{
  gint i;
  gint current_part_length;
  gdouble y, old_y = - 1.0;

  if (last_cell < first_cell)
    {
      gint swap = last_cell;
      last_cell = first_cell;
      first_cell = swap;
    }

  for (i = first_cell; i <= last_cell; i++)
    {
      if (size_left[i] < natural_height)
        {
          return FALSE;
        }
      else
        {
          y = vertical_cell_space - size_left[i];
          if (y != old_y)
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
    }

  return TRUE;
}

static void
rebuild_popover_for_day (GcalMonthView *view,
                         gint           day)
{
  GcalSubscriberViewPrivate *ppriv;
  GcalMonthViewPrivate *priv;
  GList *l;
  GtkWidget *child_widget;
  GdkRectangle rect;

  gchar *label_title;

  icaltimetype date, second_date;
  const icaltimetype *dt_start, *dt_end;
  gint start_comparison, end_comparison;

  /* placement helpers */
  gdouble start_grid_y, cell_width, cell_height;

  gint shown_rows;

  GtkStyleContext *context;
  GtkStateFlags state;

  PangoLayout *overflow_layout;
  PangoFontDescription *ofont_desc;
  gint font_height, padding_bottom;

  priv = gcal_month_view_get_instance_private (view);
  ppriv = GCAL_SUBSCRIBER_VIEW (view)->priv;

  label_title = g_strdup_printf ("%s %d", gcal_get_month_name (priv->date->month - 1), day);
  gtk_label_set_text (GTK_LABEL (priv->popover_title), label_title);
  g_free (label_title);

  /* Clean all the widgets */
  gtk_container_foreach (GTK_CONTAINER (priv->events_list_box), (GtkCallback) gtk_widget_destroy, NULL);

  l = g_hash_table_lookup (ppriv->overflow_cells, GINT_TO_POINTER (priv->pressed_overflow_indicator));
  if (l != NULL)
    {
      date = *(priv->date);
      date.day = day;

      second_date = date;

      date.hour = 0;
      date.minute = 0;
      second_date.hour = 23;
      second_date.minute =59;
    }
  for (; l != NULL; l = g_list_next (l))
    {
      child_widget = gcal_event_widget_clone (GCAL_EVENT_WIDGET (l->data));
      gtk_container_add (GTK_CONTAINER (priv->events_list_box), child_widget);
      _gcal_subscriber_view_setup_child (GCAL_SUBSCRIBER_VIEW (view), child_widget);

      dt_start = gcal_event_widget_peek_start_date (GCAL_EVENT_WIDGET (child_widget));
      dt_end = gcal_event_widget_peek_end_date (GCAL_EVENT_WIDGET (child_widget));

      date.is_date = dt_start->is_date;
      start_comparison = icaltime_compare (*dt_start, date);
      end_comparison = icaltime_compare (second_date, *dt_end);
      if (start_comparison == -1 && end_comparison == -1)
        gtk_style_context_add_class (gtk_widget_get_style_context (child_widget), "slanted");
      else if (start_comparison == -1)
        gtk_style_context_add_class (gtk_widget_get_style_context (child_widget), "slanted-start");
      else if (end_comparison == -1)
        gtk_style_context_add_class (gtk_widget_get_style_context (child_widget), "slanted-end");
    }

  /* placement calculation */
  start_grid_y = get_start_grid_y (GTK_WIDGET (view));
  shown_rows = ceil ((priv->days_delay + icaltime_days_in_month (priv->date->month, priv->date->year)) / 7.0);

  cell_width = gtk_widget_get_allocated_width (GTK_WIDGET (view)) / 7.0;
  cell_height = (gtk_widget_get_allocated_height (GTK_WIDGET (view)) - start_grid_y) / 6.0;

  context = gtk_widget_get_style_context (GTK_WIDGET (view));
  state = gtk_widget_get_state_flags (GTK_WIDGET (view));
  gtk_style_context_save (context);
  gtk_style_context_add_class (context, "overflow");
  gtk_style_context_get (context, state, "font", &ofont_desc, "padding-bottom", &padding_bottom, NULL);

  overflow_layout = gtk_widget_create_pango_layout (GTK_WIDGET (view), NULL);
  pango_layout_set_font_description (overflow_layout, ofont_desc);
  pango_layout_get_pixel_size (overflow_layout, NULL, &font_height);

  gtk_style_context_restore (context);
  pango_font_description_free (ofont_desc);
  g_object_unref (overflow_layout);

  rect.y = cell_height * ((priv->pressed_overflow_indicator / 7) + 1.0 + (6 - shown_rows) * 0.5) + start_grid_y - padding_bottom - font_height / 2;
  rect.width = 1;
  rect.height = 1;

  if (priv->pressed_overflow_indicator % 7 < 3)
    {
      rect.x = cell_width * ((priv->pressed_overflow_indicator % 7) + 1.0);
      gtk_popover_set_position (GTK_POPOVER (priv->overflow_popover), priv->k == 0 ? GTK_POS_RIGHT : GTK_POS_LEFT);
    }
  else if (priv->pressed_overflow_indicator % 7 > 3)
    {
      rect.x = cell_width * ((priv->pressed_overflow_indicator % 7));
      gtk_popover_set_position (GTK_POPOVER (priv->overflow_popover), priv->k == 0 ? GTK_POS_LEFT : GTK_POS_RIGHT);
    }
  else
    {
      rect.x = cell_width * ((priv->pressed_overflow_indicator % 7) + 1.0 - priv->k);
      gtk_popover_set_position (GTK_POPOVER (priv->overflow_popover), GTK_POS_RIGHT);
    }
  gtk_popover_set_pointing_to (GTK_POPOVER (priv->overflow_popover), &rect);

  /* sizing hack */
  child_widget = gtk_bin_get_child (GTK_BIN (priv->overflow_popover));
  gtk_widget_set_size_request (child_widget, 200, -1);

  g_object_set_data (G_OBJECT (priv->overflow_popover), "selected-day", GINT_TO_POINTER (day));
}

static void
overflow_popover_hide (GtkWidget *widget,
                       gpointer   user_data)
{
  GcalMonthViewPrivate *priv;

  priv = gcal_month_view_get_instance_private (GCAL_MONTH_VIEW (user_data));
  priv->hovered_overflow_indicator = -1;

  gtk_widget_queue_draw (GTK_WIDGET (user_data));
}

static void
update_list_box_headers (GtkListBoxRow *row,
                         GtkListBoxRow *before,
                         gpointer user_data)
{
  GcalMonthViewPrivate *priv;
  GtkWidget *row_child, *before_child = NULL;
  const icaltimetype *row_date, *before_date = NULL;

  priv = gcal_month_view_get_instance_private (GCAL_MONTH_VIEW (user_data));
  row_child = gtk_bin_get_child (GTK_BIN (row));
  row_date = gcal_event_widget_peek_start_date (GCAL_EVENT_WIDGET (row_child));
  if (before != NULL)
    {
      before_child = gtk_bin_get_child (GTK_BIN (before));
      before_date = gcal_event_widget_peek_start_date (GCAL_EVENT_WIDGET (before_child));
    }

  if (!gcal_event_widget_is_multiday (GCAL_EVENT_WIDGET (row_child)) &&
      !gcal_event_widget_get_all_day (GCAL_EVENT_WIDGET (row_child)) &&
      (before_date == NULL || before_date->hour != row_date->hour))
    {
      gchar *time;
      GtkWidget *label, *vbox;

      if (priv->use_24h_format)
        time = g_strdup_printf ("%.2d:00", row_date->hour);
      else
        time = g_strdup_printf ("%.2d:00 %s", row_date->hour % 12, row_date->hour < 12 ? "AM" : "PM");

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
}

static void
add_new_event_button_cb (GtkWidget *button,
                         gpointer   user_data)
{
  GcalMonthViewPrivate *priv;
  gint day;
  icaltimetype *start_date;

  priv = gcal_month_view_get_instance_private (GCAL_MONTH_VIEW (user_data));

  gtk_widget_hide (priv->overflow_popover);

  day = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (priv->overflow_popover), "selected-day"));
  start_date = gcal_dup_icaltime (priv->date);
  start_date->day = day;
  start_date->is_date = 1;

  g_signal_emit_by_name (GCAL_VIEW (user_data), "create-event-detailed", start_date, NULL);
  g_free (start_date);
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

  subscriber_view_class = GCAL_SUBSCRIBER_VIEW_CLASS (klass);
  subscriber_view_class->is_child_multicell = gcal_month_view_is_child_multiday;
  subscriber_view_class->get_child_cell = gcal_month_view_get_child_cell;
  subscriber_view_class->clear_state = gcal_month_view_clear_state;

  g_object_class_override_property (object_class, PROP_DATE, "active-date");
}

static void
gcal_month_view_init (GcalMonthView *self)
{
  GcalMonthViewPrivate *priv;
  GtkWidget *grid;
  GtkWidget *button;

  gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);

  priv = gcal_month_view_get_instance_private (self);

  priv->start_mark_cell = -1;
  priv->end_mark_cell = -1;

  priv->pressed_overflow_indicator = -1;
  priv->hovered_overflow_indicator = -1;

  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (self)), "calendar-view");

  if (gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_LTR)
    priv->k = 0;
  else if (gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL)
    priv->k = 1;

  priv->overflow_popover = gtk_popover_new (GTK_WIDGET (self));
  gtk_style_context_add_class (gtk_widget_get_style_context (priv->overflow_popover), "events");
  g_signal_connect (priv->overflow_popover, "hide", G_CALLBACK (overflow_popover_hide), self);

  grid = gtk_grid_new ();
  g_object_set (grid, "row-spacing", 6, "orientation", GTK_ORIENTATION_VERTICAL, NULL);
  gtk_container_add (GTK_CONTAINER (priv->overflow_popover), grid);

  priv->popover_title = gtk_label_new (NULL);
  gtk_style_context_add_class (gtk_widget_get_style_context (priv->popover_title), "sidebar-header");
  g_object_set (priv->popover_title, "margin", 6, "halign", GTK_ALIGN_START, NULL);
  priv->events_list_box = gtk_list_box_new ();
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (priv->events_list_box), GTK_SELECTION_NONE);
  gtk_list_box_set_header_func (GTK_LIST_BOX (priv->events_list_box), update_list_box_headers, self, NULL);
  button = gtk_button_new_with_label (_("Add Event…"));
  g_object_set (button, "hexpand", TRUE, NULL);
  g_signal_connect (button, "clicked", G_CALLBACK (add_new_event_button_cb), self);

  gtk_container_add (GTK_CONTAINER (grid), priv->popover_title);
  gtk_container_add (GTK_CONTAINER (grid), priv->events_list_box);
  gtk_container_add (GTK_CONTAINER (grid), button);
}

static void
gcal_view_interface_init (GcalViewIface *iface)
{
  iface->get_initial_date = gcal_month_view_get_initial_date;
  iface->get_final_date = gcal_month_view_get_final_date;

  iface->clear_marks = gcal_month_view_clear_marks;

  iface->get_children_by_uuid = gcal_month_view_get_children_by_uuid;
}

static void
gcal_month_view_set_property (GObject       *object,
                              guint          property_id,
                              const GValue  *value,
                              GParamSpec    *pspec)
{
  GcalMonthViewPrivate *priv;

  priv = gcal_month_view_get_instance_private (GCAL_MONTH_VIEW (object));

  switch (property_id)
    {
    case PROP_DATE:
      {
        if (priv->date != NULL)
          g_free (priv->date);

        priv->date = g_value_dup_boxed (value);
        priv->days_delay = (time_day_of_week (1, priv->date->month - 1, priv->date->year) - priv->first_weekday + 7) % 7;
        gtk_widget_queue_draw (GTK_WIDGET (object));
        break;
      }
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
  GcalMonthViewPrivate *priv;

  priv = gcal_month_view_get_instance_private (GCAL_MONTH_VIEW (object));

  switch (property_id)
    {
    case PROP_DATE:
      g_value_set_boxed (value, priv->date);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gcal_month_view_finalize (GObject *object)
{
  GcalMonthViewPrivate *priv;
  priv = gcal_month_view_get_instance_private (GCAL_MONTH_VIEW (object));

  if (priv->date != NULL)
    g_free (priv->date);

  /* Chain up to parent's finalize() method. */
  G_OBJECT_CLASS (gcal_month_view_parent_class)->finalize (object);
}

static void
gcal_month_view_realize (GtkWidget *widget)
{
  GcalMonthViewPrivate *priv;
  GdkWindow *parent_window;
  GdkWindowAttr attributes;
  gint attributes_mask;
  GtkAllocation allocation;

  priv = gcal_month_view_get_instance_private (GCAL_MONTH_VIEW (widget));
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
                            GDK_LEAVE_NOTIFY_MASK);
  attributes_mask = GDK_WA_X | GDK_WA_Y;

  priv->event_window = gdk_window_new (parent_window,
                                       &attributes,
                                       attributes_mask);
  gtk_widget_register_window (widget, priv->event_window);
}

static void
gcal_month_view_unrealize (GtkWidget *widget)
{
  GcalMonthViewPrivate *priv;

  priv = gcal_month_view_get_instance_private (GCAL_MONTH_VIEW (widget));
  if (priv->event_window != NULL)
    {
      gtk_widget_unregister_window (widget, priv->event_window);
      gdk_window_destroy (priv->event_window);
      priv->event_window = NULL;
    }

  GTK_WIDGET_CLASS (gcal_month_view_parent_class)->unrealize (widget);
}

static void
gcal_month_view_map (GtkWidget *widget)
{
  GcalMonthViewPrivate *priv;

  priv = gcal_month_view_get_instance_private (GCAL_MONTH_VIEW (widget));
  if (priv->event_window != NULL)
    gdk_window_show (priv->event_window);

  GTK_WIDGET_CLASS (gcal_month_view_parent_class)->map (widget);
}

static void
gcal_month_view_unmap (GtkWidget *widget)
{
  GcalMonthViewPrivate *priv;

  priv = gcal_month_view_get_instance_private (GCAL_MONTH_VIEW (widget));
  if (priv->event_window != NULL)
    gdk_window_hide (priv->event_window);

  GTK_WIDGET_CLASS (gcal_month_view_parent_class)->unmap (widget);
}

static void
gcal_month_view_size_allocate (GtkWidget     *widget,
                               GtkAllocation *allocation)
{
  GcalSubscriberViewPrivate *ppriv;
  GcalMonthViewPrivate *priv;

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

  priv = gcal_month_view_get_instance_private (GCAL_MONTH_VIEW (widget));
  ppriv = GCAL_SUBSCRIBER_VIEW (widget)->priv;

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

  gtk_widget_set_allocation (widget, allocation);
  if (gtk_widget_get_realized (widget))
    gdk_window_move_resize (priv->event_window, allocation->x, allocation->y, allocation->width, allocation->height);

  gtk_style_context_get (gtk_widget_get_style_context (widget), gtk_widget_get_state_flags (widget),
                         "font", &font_desc, "padding-bottom", &padding_bottom, NULL);

  layout = gtk_widget_create_pango_layout (widget, _("Other events"));
  pango_layout_set_font_description (layout, font_desc);
  pango_layout_get_pixel_size (layout, NULL, &font_height);
  pango_font_description_free (font_desc);
  g_object_unref (layout);

  start_grid_y = get_start_grid_y (widget);
  cell_width = allocation->width / 7.0;
  cell_height = (allocation->height - start_grid_y) / 6.0;
  vertical_cell_space = cell_height - (padding_bottom + font_height);

  for (i = 0; i < 42; i++)
    size_left[i] = vertical_cell_space;

  shown_rows = ceil ((priv->days_delay + icaltime_days_in_month (priv->date->month, priv->date->year)) / 7.0);
  first_row_gap = (6 - shown_rows) * 0.5;
  sw = 1 - 2 * priv->k;

  /* allocate multidays events */
  for (l = ppriv->multi_cell_children; l != NULL; l = g_list_next (l))
    {
      gint first_cell, last_cell, first_row, last_row, start, end;
      gboolean visible, start_before = TRUE, end_after = TRUE;

      const icaltimetype *date;
      GArray *cells, *lengths;

      child_widget = (GtkWidget*) l->data;
      uuid = gcal_event_widget_peek_uuid (GCAL_EVENT_WIDGET (child_widget));
      if (!gtk_widget_is_visible (child_widget) && !g_hash_table_contains (ppriv->hidden_as_overflow, uuid))
        continue;

      gtk_widget_show (child_widget);
      gtk_widget_get_preferred_height (child_widget, NULL, &natural_height);

      j = 1;
      date = gcal_event_widget_peek_start_date (GCAL_EVENT_WIDGET (child_widget));
      if (date->month == priv->date->month)
        {
          j = date->day;
          start_before = FALSE;
        }
      j += priv->days_delay;
      first_cell = 7 * ((j - 1) / 7)+ 6 * priv->k + sw * ((j - 1) % 7);

      j = icaltime_days_in_month (priv->date->month, priv->date->year);
      date = gcal_event_widget_peek_end_date (GCAL_EVENT_WIDGET (child_widget));
      if (gcal_event_widget_get_all_day (GCAL_EVENT_WIDGET (child_widget)))
        {
          if (date->month == priv->date->month)
            {
              j = date->day - 1;
              end_after = FALSE;
            }
        }
      else
        {
          if (date->month == priv->date->month)
            {
              j = date->day;
              end_after = FALSE;
            }
        }
      j += priv->days_delay;
      last_cell = 7 * ((j - 1) / 7)+ 6 * priv->k + sw * ((j - 1) % 7);

      first_row = first_cell / 7;
      last_row = last_cell / 7;
      visible = TRUE;
      cells = g_array_sized_new (TRUE, TRUE, sizeof (gint), 16);
      lengths = g_array_sized_new (TRUE, TRUE, sizeof (gint), 16);
      for (i = first_row; i <= last_row && visible; i++)
        {
          start = i * 7 + priv->k * 6;
          end = i * 7 + (1 - priv->k) * 6;

          if (i == first_row)
            {
              start = first_cell;
            }
          if (i == last_row)
            {
              end = last_cell;
            }

          visible = get_widget_parts (start, end, natural_height, vertical_cell_space, size_left, cells, lengths);
        }

      if (visible)
        {
          for (i = 0; i < cells->len; i++)
            {
              gint cell_idx = g_array_index (cells, gint, i);
              gint row = cell_idx / 7;
              gint column = cell_idx % 7;

              if (i != 0)
                {
                  child_widget = gcal_event_widget_clone (GCAL_EVENT_WIDGET (child_widget));

                  _gcal_subscriber_view_setup_child (GCAL_SUBSCRIBER_VIEW (widget), child_widget);
                  gtk_widget_show (child_widget);

                  if (i != cells->len - 1 || end_after)
                    gtk_style_context_add_class (gtk_widget_get_style_context (child_widget), "slanted");
                  else
                    gtk_style_context_add_class (gtk_widget_get_style_context (child_widget), "slanted-start");

                  aux = g_hash_table_lookup (ppriv->children, uuid);
                  aux = g_list_append (aux, child_widget);
                }
              else if (i == 0 && cells->len == 1)
                {
                  if (start_before)
                    gtk_style_context_add_class (gtk_widget_get_style_context (child_widget), "slanted-start");
                  if (end_after)
                    gtk_style_context_add_class (gtk_widget_get_style_context (child_widget), "slanted-end");
                }
              else if (i != cells->len - 1)
                {
                  if (start_before)
                    gtk_style_context_add_class (gtk_widget_get_style_context (child_widget), "slanted");
                  else
                    gtk_style_context_add_class (gtk_widget_get_style_context (child_widget), "slanted-end");
                }

              gtk_style_context_get_margin (gtk_widget_get_style_context (child_widget),
                                            gtk_widget_get_state_flags (child_widget),
                                            &margin);

              pos_x = cell_width * column + margin.left;
              pos_y = cell_height * (row + first_row_gap) + start_grid_y + margin.top;

              child_allocation.x = pos_x;
              child_allocation.y = pos_y + vertical_cell_space - size_left[cell_idx];
              child_allocation.width = cell_width * g_array_index (lengths, gint, i) - (margin.left + margin.right);
              child_allocation.height = natural_height;

              gtk_widget_size_allocate (child_widget, &child_allocation);
              g_hash_table_remove (ppriv->hidden_as_overflow, uuid);

              /* update size_left */
              for (j = 0; j < g_array_index (lengths, gint, i); j++)
                size_left[cell_idx + j] -= natural_height + margin.top + margin.bottom;
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

  g_hash_table_iter_init (&iter, ppriv->single_cell_children);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      j = GPOINTER_TO_INT (key);
      j += priv->days_delay;
      i = 7 * ((j - 1) / 7)+ 6 * priv->k + sw * ((j - 1) % 7);

      l = (GList*) value;
      for (aux = l; aux != NULL; aux = g_list_next (aux))
        {
          child_widget = (GtkWidget*) aux->data;

          uuid = gcal_event_widget_peek_uuid (GCAL_EVENT_WIDGET (child_widget));
          if (!gtk_widget_is_visible (child_widget) && !g_hash_table_contains (ppriv->hidden_as_overflow, uuid))
            continue;

          gtk_widget_show (child_widget);
          gtk_widget_get_preferred_height (child_widget, NULL, &natural_height);

          if (size_left[i] > natural_height)
            {
              gtk_style_context_get_margin (gtk_widget_get_style_context (child_widget),
                                            gtk_widget_get_state_flags (child_widget),
                                            &margin);

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
  GcalMonthViewPrivate *priv;

  GtkStyleContext *context;
  GtkStateFlags state;

  GtkBorder padding;
  GtkAllocation alloc;

  GdkRGBA color;

  gchar *header_str;
  PangoLayout *layout;
  PangoFontDescription *font_desc, *sfont_desc;

  gint font_width, font_height, pos_x, pos_y, shown_rows;
  gint i, j, sw, lower_mark = 43, upper_mark = -2;
  gdouble start_grid_y, cell_width, cell_height, first_row_gap = 0.0;
  gdouble days;

  priv = gcal_month_view_get_instance_private (GCAL_MONTH_VIEW (widget));
  ppriv = GCAL_SUBSCRIBER_VIEW (widget)->priv;

  /* fonts and colors selection */
  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);

  gtk_style_context_get (context, state | GTK_STATE_FLAG_SELECTED, "font", &sfont_desc, NULL);

  gtk_widget_get_allocation (widget, &alloc);
  start_grid_y = get_start_grid_y (widget);
  cell_width = alloc.width / 7.0;
  cell_height = (alloc.height - start_grid_y) / 6.0;

  layout = gtk_widget_create_pango_layout (widget, NULL);

  /* calculations */
  days = priv->days_delay + icaltime_days_in_month (priv->date->month, priv->date->year);
  shown_rows = ceil (days / 7.0);

  first_row_gap = (6 - shown_rows) * 0.5; /* invalid area before the actual rows */
  sw = 1 - 2 * priv->k;

  /* active cells */
  if (priv->start_mark_cell != -1 && priv->end_mark_cell != -1)
    {
      lower_mark = 7 * ((priv->start_mark_cell + 7 * priv->k) / 7) + sw * (priv->start_mark_cell % 7) + (1 - priv->k);
      upper_mark = 7 * ((priv->end_mark_cell + 7 * priv->k) / 7) + sw * (priv->end_mark_cell % 7) + (1 - priv->k);
      if (lower_mark > upper_mark)
        {
          gint cell;
          cell = lower_mark;
          lower_mark = upper_mark;
          upper_mark = cell;
        }

      lower_mark -= priv->days_delay;
      upper_mark -= priv->days_delay;
    }

  /* view headers */
  gtk_style_context_save (context);
  gtk_style_context_add_class (context, "first-view-header");

  gtk_style_context_get (context, state, "font", &font_desc, NULL);
  gtk_style_context_get_padding (context, state, &padding);

  header_str = g_strdup_printf ("%s", gcal_get_month_name (priv->date->month - 1));
  pango_layout_set_text (layout, header_str, -1);
  pango_layout_set_font_description (layout, font_desc);
  pango_layout_get_pixel_size (layout, &font_width, NULL);

  gtk_render_layout (context, cr, priv->k * (alloc.width - font_width) + sw * padding.left, padding.top, layout);

  pango_font_description_free (font_desc);
  g_free (header_str);
  gtk_style_context_restore (context);

  gtk_style_context_save (context);
  gtk_style_context_add_class (context, "second-view-header");

  gtk_style_context_get (context, state, "font", &font_desc, NULL);
  gtk_style_context_get_padding (context, state, &padding);

  header_str = g_strdup_printf ("%d", priv->date->year);
  pango_layout_set_text (layout, header_str, -1);
  pango_layout_set_font_description (layout, font_desc);
  pango_layout_get_pixel_size (layout, &font_width, NULL);

  gtk_render_layout (context, cr, (1 - priv->k) * (alloc.width - font_width) - sw * padding.left, padding.top, layout);

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

      j = 6 * priv->k + sw * i;
      upcased_day = g_utf8_strup (gcal_get_weekday ((j + priv->first_weekday) % 7), -1);
      pango_layout_set_text (layout, upcased_day, -1);
      pango_layout_get_pixel_size (layout, &font_width, &font_height);

      gtk_render_layout (context, cr,
                         cell_width * (i + priv->k) + (sw * padding.left) - priv->k * font_width,
                         start_grid_y - padding.bottom - font_height,
                         layout);
      g_free (upcased_day);
    }

  pango_font_description_free (font_desc);
  gtk_style_context_restore (context);

  /* grid numbers */
  gtk_style_context_get (context, state, "font", &font_desc, NULL);
  pango_layout_set_font_description (layout, font_desc);
  for (i = 0; i < 7 * shown_rows; i++)
    {
      gchar *nr_day;
      gint column = i % 7;
      gint row = i / 7;

      j = 7 * ((i + 7 * priv->k) / 7) + sw * (i % 7) + (1 - priv->k);
      if (j <= priv->days_delay)
        continue;
      else if (j > days)
        continue;
      j -= priv->days_delay;

      nr_day = g_strdup_printf ("%d", j);

      if (g_hash_table_contains (ppriv->overflow_cells, GINT_TO_POINTER (i)))
        {
          GList *l;
          PangoLayout *overflow_layout;
          PangoFontDescription *ofont_desc;
          gchar *overflow_str;
          gdouble y_value;

          l = g_hash_table_lookup (ppriv->overflow_cells, GINT_TO_POINTER (i));

          /* TODO: Warning in some languages this string can be too long and may overlap wit the number */
          overflow_str = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE, "Other event", "Other %d events", g_list_length (l)),
                                          g_list_length (l));

          gtk_style_context_save (context);
          gtk_style_context_add_class (context, "overflow");
          if (priv->hovered_overflow_indicator == i)
            {
              gtk_style_context_set_state (context, state | GTK_STATE_FLAG_PRELIGHT);
              gtk_style_context_get (context, state | GTK_STATE_FLAG_PRELIGHT, "font", &ofont_desc, NULL);
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

          if ((!gtk_widget_is_visible (priv->overflow_popover) && priv->hovered_overflow_indicator == i) ||
               gtk_widget_is_visible (priv->overflow_popover))
            {
              gtk_render_background (context, cr, cell_width * column, y_value - padding.bottom,
                                     cell_width, font_height + padding.bottom * 2);
            }

          gtk_render_layout (context, cr, cell_width * column, y_value, overflow_layout);

          gtk_style_context_set_state (context, state);
          gtk_style_context_remove_class (context, "overflow");
          gtk_style_context_restore (context);
          g_free (overflow_str);
          pango_font_description_free (ofont_desc);
          g_object_unref (overflow_layout);
        }

      if (priv->date->year == priv->current_date->year && priv->date->month == priv->current_date->month &&
          j == priv->current_date->day)
        {
          PangoLayout *clayout;
          PangoFontDescription *cfont_desc;

          gtk_style_context_save (context);
          gtk_style_context_add_class (context, "current");

          clayout = gtk_widget_create_pango_layout (widget, nr_day);
          gtk_style_context_get (context, state, "font", &cfont_desc, NULL);
          pango_layout_set_font_description (clayout, cfont_desc);
          pango_layout_get_pixel_size (clayout, &font_width, &font_height);

          /* FIXME: hardcoded padding of the number background */
          gtk_render_background (context, cr,
                                 cell_width * (column + 1 - priv->k) - sw * padding.right + (priv->k - 1) * font_width - 2.0,
                                 cell_height * (row + 1 + first_row_gap) - font_height - padding.bottom + start_grid_y,
                                 font_width + 4, font_height + 2);
          gtk_render_layout (context, cr,
                             cell_width * (column + 1 - priv->k) - sw * padding.right + (priv->k - 1) * font_width,
                             cell_height * (row + 1 + first_row_gap) - font_height - padding.bottom + start_grid_y,
                             clayout);

          gtk_style_context_restore (context);
          pango_font_description_free (cfont_desc);
          g_object_unref (clayout);
        }
      else if (lower_mark <= j && j <= upper_mark)
        {
          gtk_style_context_save (context);
          gtk_style_context_set_state (context, state | GTK_STATE_FLAG_SELECTED);

          pango_layout_set_text (layout, nr_day, -1);
          pango_layout_set_font_description (layout, sfont_desc);
          pango_layout_get_pixel_size (layout, &font_width, &font_height);

          gtk_render_layout (context, cr,
                             cell_width * (column + 1 - priv->k) - sw * padding.right + (priv->k - 1) * font_width,
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
                             cell_width * (column + 1 - priv->k) - sw * padding.right + (priv->k - 1) * font_width,
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
  cairo_set_line_width (cr, 0.2);
  gdk_cairo_set_source_rgba (cr, &color);
  /* vertical lines, the easy ones */
  for (i = 0; i < 6; i++)
    {
      pos_x = cell_width * (i + 1);
      cairo_move_to (cr, pos_x + 0.4, start_grid_y);
      cairo_rel_line_to (cr, 0, alloc.height - start_grid_y);
    }

  /* top horizontal line */
  cairo_move_to (cr, 0, start_grid_y + 0.4);
  cairo_rel_line_to (cr, alloc.width, 0);

  /* drawing weeks lines */
  for (i = 0; i < (shown_rows % 2) + 5; i++)
    {
      pos_y = cell_height * (i + 0.5 * (2.0 - (shown_rows % 2))) + start_grid_y;
      cairo_move_to (cr, 0, pos_y + 0.4);
      cairo_rel_line_to (cr, alloc.width, 0);
    }
  cairo_stroke (cr);
  gtk_style_context_restore (context);

  /* selection borders */
  if (priv->start_mark_cell != -1 && priv->end_mark_cell != -1)
   {
     gint first_mark, last_mark;
     gint pos_x2, pos_y2;

     first_mark = priv->start_mark_cell;
     last_mark = priv->end_mark_cell;
     if (priv->start_mark_cell > priv->end_mark_cell)
       {
         last_mark = priv->start_mark_cell;
         first_mark = priv->end_mark_cell;
       }

     gtk_style_context_get_color (context, state | GTK_STATE_FLAG_SELECTED, &color);
     gdk_cairo_set_source_rgba (cr, &color);
     cairo_set_line_width (cr, 0.4);

     /* horizontals */
     if ((first_mark / 7) == (last_mark / 7))
       {
         gint row = first_mark / 7;
         gint first_column = first_mark % 7;
         gint last_column = last_mark % 7;

         pos_x = cell_width * (first_column);
         pos_x2 = cell_width * (last_column + 1);
         pos_y = cell_height * (row + first_row_gap) + start_grid_y;
         pos_y2 = cell_height * (row + 1.0 + first_row_gap) + start_grid_y;
         cairo_rectangle (cr, pos_x + 0.3, pos_y + 0.3, pos_x2 - pos_x + 0.6, pos_y2 - pos_y + 0.6);
       }
     else
       {
         gint first_row = first_mark / 7;
         gint last_row = last_mark / 7;
         gint first_column = first_mark % 7;
         gint last_column = last_mark % 7;
         gdouble end;

         for (i = first_row; i <= last_row; i++)
           {
             if (i == first_row)
               {
                 pos_x = cell_width * (first_column + priv->k);
                 end = (alloc.width * (1 - priv->k)) - pos_x;
               }
             else if (i == last_row)
               {
                 pos_x = cell_width * (last_column + 1 - priv->k);
                 end = (alloc.width * priv->k) - pos_x;
               }
             else
               {
                 pos_x = 0;
                 end = alloc.width;
               }

             pos_y = cell_height * (i + first_row_gap) + start_grid_y;
             pos_y2 = cell_height * (i + 1.0 + first_row_gap) + start_grid_y;
             cairo_rectangle (cr, pos_x + 0.3, pos_y + 0.3, ((gint)end) + 0.6, pos_y2 - pos_y + 0.6);
           }
       }

     cairo_stroke (cr);
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
  GcalMonthViewPrivate *priv;

  gint days, j, sw, clicked_cell;
  gboolean pressed_indicator = FALSE;

  priv = gcal_month_view_get_instance_private (GCAL_MONTH_VIEW (widget));
  ppriv = GCAL_SUBSCRIBER_VIEW (widget)->priv;

  days = priv->days_delay + icaltime_days_in_month (priv->date->month, priv->date->year);
  sw = 1 - 2 * priv->k;

  clicked_cell = gather_button_event_data (GCAL_MONTH_VIEW (widget), event->x, event->y,
                                                 &pressed_indicator, NULL, NULL);

  j = 7 * ((clicked_cell + 7 * priv->k) / 7) + sw * (clicked_cell % 7) + (1 - priv->k);

  if (j > priv->days_delay && j <= days)
    priv->start_mark_cell = clicked_cell;

  if (pressed_indicator && g_hash_table_contains (ppriv->overflow_cells, GINT_TO_POINTER (clicked_cell)))
    priv->pressed_overflow_indicator = clicked_cell;

  return TRUE;
}

/**
 * gcal_month_view_motion_notify_event:
 * @widget:
 * @event:
 *
 * Update priv->end_cell_mark in order to update the drawing
 * belonging to the days involved in the event creation
 *
 * Returns:
 **/
static gboolean
gcal_month_view_motion_notify_event (GtkWidget      *widget,
                                     GdkEventMotion *event)
{
  GcalMonthViewPrivate *priv;

  gint days;
  gint j, sw;
  gint new_end_cell, new_hovered_cell;
  gboolean hovered_indicator = FALSE;

  priv = gcal_month_view_get_instance_private (GCAL_MONTH_VIEW (widget));

  days = priv->days_delay + icaltime_days_in_month (priv->date->month, priv->date->year);
  sw = 1 - 2 * priv->k;

  new_end_cell = gather_button_event_data (GCAL_MONTH_VIEW (widget), event->x, event->y, &hovered_indicator, NULL, NULL);

  j = 7 * ((new_end_cell + 7 * priv->k) / 7) + sw * (new_end_cell % 7) + (1 - priv->k);

  if (priv->start_mark_cell != -1)
    {
      if (j > priv->days_delay && j <= days)
        {
          if (priv->end_mark_cell != priv->start_mark_cell)
            priv->hovered_overflow_indicator = -1;
          if (priv->end_mark_cell != new_end_cell)
            gtk_widget_queue_draw (widget);

          priv->end_mark_cell = new_end_cell;
          return TRUE;
        }
    }
  else
    {
      if (gtk_widget_is_visible (priv->overflow_popover))
        return FALSE;

      if (hovered_indicator)
        new_hovered_cell = new_end_cell;
      else
        new_hovered_cell = -1;

      if (priv->hovered_overflow_indicator != new_hovered_cell)
        {
          priv->hovered_overflow_indicator = new_hovered_cell;
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
  GcalSubscriberViewPrivate *ppriv;
  GcalMonthViewPrivate *priv;

  gint days;

  gint j, sw;
  gint released;
  gboolean released_indicator = FALSE;

  gdouble x, y;

  icaltimetype *start_date;
  icaltimetype *end_date = NULL;

  priv = gcal_month_view_get_instance_private (GCAL_MONTH_VIEW (widget));
  ppriv = GCAL_SUBSCRIBER_VIEW (widget)->priv;

  if (priv->start_mark_cell == -1)
    return FALSE;

  days = priv->days_delay + icaltime_days_in_month (priv->date->month, priv->date->year);
  sw = 1 - 2 * priv->k;

  released = gather_button_event_data (GCAL_MONTH_VIEW (widget), event->x, event->y, &released_indicator, &x, &y);

  j = 7 * ((released + 7 * priv->k) / 7) + sw * (released % 7) + (1 - priv->k);

  if (j <= priv->days_delay || j > days)
    {
      priv->pressed_overflow_indicator = -1;
      priv->start_mark_cell = -1;
      priv->end_mark_cell = -1;
      gtk_widget_queue_draw (widget);

      return TRUE;
    }

  priv->end_mark_cell = released;

  if (priv->pressed_overflow_indicator != -1 && priv->start_mark_cell == priv->end_mark_cell &&
      g_hash_table_contains (ppriv->overflow_cells, GINT_TO_POINTER (priv->pressed_overflow_indicator)))
    {
      priv->hovered_overflow_indicator = priv->pressed_overflow_indicator;

      rebuild_popover_for_day (GCAL_MONTH_VIEW (widget), j - priv->days_delay);
      gtk_widget_show_all (priv->overflow_popover);

      gtk_widget_queue_draw (widget);
      priv->pressed_overflow_indicator = -1;
      priv->start_mark_cell = -1;
      priv->end_mark_cell = -1;
      return TRUE;
    }
  else
    {
      start_date = gcal_dup_icaltime (priv->date);
      start_date->day = 7 * ((priv->start_mark_cell + 7 * priv->k) / 7) + sw * (priv->start_mark_cell % 7) + (1 - priv->k);
      start_date->day -= priv->days_delay;
      start_date->is_date = 1;

      end_date = gcal_dup_icaltime (priv->date);
      end_date->day = j - priv->days_delay;
      end_date->is_date = 1;

      if (start_date->day > end_date->day)
        {
          gint day = start_date->day;
          start_date->day = end_date->day;
          end_date->day = day;
        }
      end_date->day += 1 ;

      g_signal_emit_by_name (GCAL_VIEW (widget), "create-event", start_date, end_date, x, y);

      g_free (start_date);
      g_free (end_date);
    }

  gtk_widget_queue_draw (widget);
  priv->pressed_overflow_indicator = -1;
  return TRUE;
}

static void
gcal_month_view_direction_changed (GtkWidget        *widget,
                                   GtkTextDirection  previous_direction)
{
  GcalMonthViewPrivate *priv;
  priv = gcal_month_view_get_instance_private (GCAL_MONTH_VIEW (widget));

  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR)
    priv->k = 0;
  else if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
    priv->k = 1;
}

static gboolean
gcal_month_view_is_child_multiday (GcalSubscriberView *subscriber,
                                     GcalEventWidget    *child)
{
  return gcal_event_widget_is_multiday (child);
}

static guint
gcal_month_view_get_child_cell (GcalSubscriberView *subscriber,
                                GcalEventWidget    *child)
{
  const icaltimetype *dt_start = gcal_event_widget_peek_start_date (child);
  return dt_start->day;
}

static void
gcal_month_view_clear_state (GcalSubscriberView *subscriber)
{
  GcalMonthViewPrivate *priv;

  priv = gcal_month_view_get_instance_private (GCAL_MONTH_VIEW (subscriber));

  priv->start_mark_cell = -1;
  priv->end_mark_cell = -1;
  priv->pressed_overflow_indicator = -1;

  gtk_widget_hide (priv->overflow_popover);
}

/* GcalView Interface API */
/**
 * gcal_month_view_get_initial_date:
 *
 * Since: 0.1
 * Return value: the first day of the month
 * Returns: (transfer full): Release with g_free()
 **/
static icaltimetype*
gcal_month_view_get_initial_date (GcalView *view)
{
  //FIXME to retrieve the 35 days range
  GcalMonthViewPrivate *priv;
  icaltimetype *new_date;

  g_return_val_if_fail (GCAL_IS_MONTH_VIEW (view), NULL);
  priv = gcal_month_view_get_instance_private (GCAL_MONTH_VIEW (view));

  new_date = gcal_dup_icaltime (priv->date);
  new_date->day = 1;
  new_date->is_date = 0;
  new_date->hour = 0;
  new_date->minute = 0;
  new_date->second = 0;

  return new_date;
}

/**
 * gcal_month_view_get_final_date:
 *
 * Since: 0.1
 * Return value: the last day of the month
 * Returns: (transfer full): Release with g_free()
 **/
static icaltimetype*
gcal_month_view_get_final_date (GcalView *view)
{
  //FIXME to retrieve the 35 days range
  GcalMonthViewPrivate *priv;
  icaltimetype *new_date;

  g_return_val_if_fail (GCAL_IS_MONTH_VIEW (view), NULL);
  priv = gcal_month_view_get_instance_private (GCAL_MONTH_VIEW (view));

  new_date = gcal_dup_icaltime (priv->date);
  new_date->day = icaltime_days_in_month (priv->date->month, priv->date->year);
  new_date->is_date = 0;
  new_date->hour = 23;
  new_date->minute = 59;
  new_date->second = 0;

  return new_date;
}

static void
gcal_month_view_clear_marks (GcalView *view)
{
  GcalMonthViewPrivate *priv;

  priv = gcal_month_view_get_instance_private (GCAL_MONTH_VIEW (view));

  priv->start_mark_cell = -1;
  priv->end_mark_cell = -1;
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

/* Public API */
void
gcal_month_view_set_current_date (GcalMonthView *month_view,
                                  icaltimetype  *current_date)
{
  GcalMonthViewPrivate *priv = gcal_month_view_get_instance_private (month_view);

  priv->current_date = current_date;
  gtk_widget_queue_draw (GTK_WIDGET (month_view));
}

/**
 * gcal_month_view_set_first_weekday:
 * @view: A #GcalMonthView instance
 * @day_nr: Integer representing the first day of the week
 *
 * Set the first day of the week according to the locale, being
 * 0 for Sunday, 1 for Monday and so on.
 **/
void
gcal_month_view_set_first_weekday (GcalMonthView *view,
                                   gint           day_nr)
{
  GcalMonthViewPrivate *priv = gcal_month_view_get_instance_private (view);

  priv->first_weekday = day_nr;
  /* update days_delay */
  if (priv->date != NULL)
    priv->days_delay = (time_day_of_week (1, priv->date->month - 1, priv->date->year) - priv->first_weekday + 7) % 7;
}

/**
 * gcal_month_view_set_use_24h_format:
 * @view:
 * @use_24h:
 *
 * Whether the view will show time using 24h or 12h format
 **/
void
gcal_month_view_set_use_24h_format (GcalMonthView *view,
                                    gboolean       use_24h)
{
  GcalMonthViewPrivate *priv;

  priv = gcal_month_view_get_instance_private (view);
  priv->use_24h_format = use_24h;
}
