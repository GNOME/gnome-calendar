/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2015 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gcal-date-chooser-day.h"

#include <stdlib.h>
#include <langinfo.h>

enum {
  SELECTED,
  LAST_DAY_SIGNAL
};

static guint signals[LAST_DAY_SIGNAL] = { 0, };

struct _GcalDateChooserDay
{
  GtkBin              parent;

  GtkWidget          *label;
  GDateTime          *date;
  GdkWindow          *event_window;
  GtkGesture         *multipress_gesture;
};

G_DEFINE_TYPE (GcalDateChooserDay, gcal_date_chooser_day, GTK_TYPE_BIN)

static void
day_pressed (GtkGestureMultiPress *gesture,
             gint                  n_press,
             gdouble               x,
             gdouble               y,
             GcalDateChooserDay   *self)
{
  gint button;

  button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));

  if (button == GDK_BUTTON_PRIMARY)
    {
      if (n_press == 1)
        g_signal_emit (self, signals[SELECTED], 0);
    }
}

static gboolean
gcal_date_chooser_day_key_press (GtkWidget   *widget,
                                 GdkEventKey *event)
{
  GcalDateChooserDay *self = GCAL_DATE_CHOOSER_DAY (widget);

  if (event->keyval == GDK_KEY_space ||
      event->keyval == GDK_KEY_Return ||
      event->keyval == GDK_KEY_ISO_Enter||
      event->keyval == GDK_KEY_KP_Enter ||
      event->keyval == GDK_KEY_KP_Space)
    {
      g_signal_emit (self, signals[SELECTED], 0);
      return TRUE;
    }

  if (GTK_WIDGET_CLASS (gcal_date_chooser_day_parent_class)->key_press_event (widget, event))
    return TRUE;

 return FALSE;
}

static void
gcal_date_chooser_day_dispose (GObject *object)
{
  GcalDateChooserDay *self = GCAL_DATE_CHOOSER_DAY (object);

  g_clear_object (&self->multipress_gesture);

  G_OBJECT_CLASS (gcal_date_chooser_day_parent_class)->dispose (object);
}

static gboolean
gcal_date_chooser_day_draw (GtkWidget *widget,
                            cairo_t   *cr)
{
  GtkStyleContext *context;
  GtkStateFlags state;
  gint x, y, width, height;

  context = gtk_widget_get_style_context (widget);
  state = gtk_style_context_get_state (context);

  x = 0;
  y = 0;
  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);

  gtk_render_background (context, cr, x, y, width, height);
  gtk_render_frame (context, cr, x, y, width, height);

  GTK_WIDGET_CLASS (gcal_date_chooser_day_parent_class)->draw (widget, cr);

  if (gtk_widget_has_visible_focus (widget))
    {
      GtkBorder border;

      gtk_style_context_get_border (context, state, &border);
      gtk_render_focus (context, cr, border.left, border.top,
                        gtk_widget_get_allocated_width (widget) - border.left - border.right,
                        gtk_widget_get_allocated_height (widget) - border.top - border.bottom);
    }

  return FALSE;
}

static void
gcal_date_chooser_day_map (GtkWidget *widget)
{
  GcalDateChooserDay *self = GCAL_DATE_CHOOSER_DAY (widget);

  GTK_WIDGET_CLASS (gcal_date_chooser_day_parent_class)->map (widget);

  gdk_window_show (self->event_window);
}

static void
gcal_date_chooser_day_unmap (GtkWidget *widget)
{
  GcalDateChooserDay *self = GCAL_DATE_CHOOSER_DAY (widget);

  gdk_window_hide (self->event_window);

  GTK_WIDGET_CLASS (gcal_date_chooser_day_parent_class)->unmap (widget);
}

static void
gcal_date_chooser_day_realize (GtkWidget *widget)
{
  GcalDateChooserDay *self = GCAL_DATE_CHOOSER_DAY (widget);
  GtkAllocation allocation;
  GdkWindow *window;
  GdkWindowAttr attributes;
  gint attributes_mask;

  gtk_widget_get_allocation (widget, &allocation);
  gtk_widget_set_realized (widget, TRUE);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = allocation.x;
  attributes.y = allocation.y;
  attributes.width = allocation.width;
  attributes.height = allocation.height;
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= GDK_BUTTON_PRESS_MASK
                           | GDK_BUTTON_RELEASE_MASK
                           | GDK_TOUCH_MASK
                           | GDK_ENTER_NOTIFY_MASK
                           | GDK_LEAVE_NOTIFY_MASK;

  attributes_mask = GDK_WA_X | GDK_WA_Y;

  window = gtk_widget_get_parent_window (widget);
  gtk_widget_set_window (widget, window);
  g_object_ref (window);

  self->event_window = gdk_window_new (window, &attributes, attributes_mask);
  gtk_widget_register_window (widget, self->event_window);
}

static void
gcal_date_chooser_day_unrealize (GtkWidget *widget)
{
  GcalDateChooserDay *self = GCAL_DATE_CHOOSER_DAY (widget);

  if (self->event_window)
    {
      gtk_widget_unregister_window (widget, self->event_window);
      gdk_window_destroy (self->event_window);
      self->event_window = NULL;
    }

  GTK_WIDGET_CLASS (gcal_date_chooser_day_parent_class)->unrealize (widget);
}

static void
gcal_date_chooser_day_size_allocate (GtkWidget     *widget,
                                     GtkAllocation *allocation)
{
  GcalDateChooserDay *self = GCAL_DATE_CHOOSER_DAY (widget);

  GTK_WIDGET_CLASS (gcal_date_chooser_day_parent_class)->size_allocate (widget, allocation);

  if (gtk_widget_get_realized (widget))
    {
      gdk_window_move_resize (self->event_window,
                              allocation->x,
                              allocation->y,
                              allocation->width,
                              allocation->height);
    }
}

static void
gcal_date_chooser_day_drag_data_get (GtkWidget        *widget,
                                     GdkDragContext   *context,
                                     GtkSelectionData *selection_data,
                                     guint             info,
                                     guint             time)
{
  GcalDateChooserDay *self = GCAL_DATE_CHOOSER_DAY (widget);
  gchar *text;

  text = g_date_time_format (self->date, "%x");
  gtk_selection_data_set_text (selection_data, text, -1);
  g_free (text);
}

static void
gcal_date_chooser_day_get_preferred_width (GtkWidget *widget,
                                           gint      *minimum,
                                           gint      *natural)
{
  GcalDateChooserDay *self;
  GtkStyleContext *context;
  GtkStateFlags flags;
  GtkBorder border, margin, padding;
  gint label_min, label_nat;
  gint min_width, min, nat;

  self = GCAL_DATE_CHOOSER_DAY (widget);
  context = gtk_widget_get_style_context (widget);
  flags = gtk_style_context_get_state (context);

  gtk_style_context_get_border (context, flags, &border);
  gtk_style_context_get_margin (context, flags, &margin);
  gtk_style_context_get_padding (context, flags, &padding);

  gtk_style_context_get (context,
                         flags,
                         "min-width", &min_width,
                         NULL);

  gtk_widget_get_preferred_width (self->label, &label_min, &label_nat);

  min = label_min + margin.left + border.left + padding.left + margin.right + border.right + padding.right;
  nat = label_nat + margin.left + border.left + padding.left + margin.right + border.right + padding.right;

  if (minimum)
    *minimum = MAX (min, min_width);

  if (natural)
    *natural = MAX (nat, min_width);
}

static void
gcal_date_chooser_day_get_preferred_height (GtkWidget *widget,
                                            gint      *minimum,
                                            gint      *natural)
{
  GcalDateChooserDay *self;
  GtkStyleContext *context;
  GtkStateFlags flags;
  GtkBorder border, margin, padding;
  gint label_min, label_nat;
  gint min_height, min, nat;

  self = GCAL_DATE_CHOOSER_DAY (widget);
  context = gtk_widget_get_style_context (widget);
  flags = gtk_style_context_get_state (context);

  gtk_style_context_get_border (context, flags, &border);
  gtk_style_context_get_margin (context, flags, &margin);
  gtk_style_context_get_padding (context, flags, &padding);

  gtk_style_context_get (context,
                         flags,
                         "min-height", &min_height,
                         NULL);

  gtk_widget_get_preferred_height (self->label, &label_min, &label_nat);

  min = label_min + margin.top + border.top + padding.top + margin.bottom + border.bottom + padding.bottom;
  nat = label_nat + margin.top + border.top + padding.top + margin.bottom + border.bottom + padding.bottom;

  if (minimum)
    *minimum = MAX (min, min_height);

  if (natural)
    *natural = MAX (nat, min_height);
}

static gboolean
gcal_date_chooser_day_enter_notify_event (GtkWidget        *widget,
                                          GdkEventCrossing *event)
{
  GtkStyleContext *context;
  GtkStateFlags state;

  context = gtk_widget_get_style_context (widget);
  state = gtk_style_context_get_state (context);

  gtk_style_context_set_state (context, state | GTK_STATE_FLAG_PRELIGHT);

  return GDK_EVENT_PROPAGATE;
}

static gboolean
gcal_date_chooser_day_leave_notify_event (GtkWidget        *widget,
                                          GdkEventCrossing *event)
{
  GtkStyleContext *context;
  GtkStateFlags state;

  context = gtk_widget_get_style_context (widget);
  state = gtk_style_context_get_state (context);

  gtk_style_context_set_state (context, state & ~GTK_STATE_FLAG_PRELIGHT);

  return GDK_EVENT_PROPAGATE;
}

static void
gcal_date_chooser_day_class_init (GcalDateChooserDayClass *class)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = gcal_date_chooser_day_dispose;

  widget_class->draw = gcal_date_chooser_day_draw;
  widget_class->realize = gcal_date_chooser_day_realize;
  widget_class->unrealize = gcal_date_chooser_day_unrealize;
  widget_class->map = gcal_date_chooser_day_map;
  widget_class->unmap = gcal_date_chooser_day_unmap;
  widget_class->key_press_event = gcal_date_chooser_day_key_press;
  widget_class->size_allocate = gcal_date_chooser_day_size_allocate;
  widget_class->drag_data_get = gcal_date_chooser_day_drag_data_get;
  widget_class->get_preferred_width = gcal_date_chooser_day_get_preferred_width;
  widget_class->get_preferred_height = gcal_date_chooser_day_get_preferred_height;
  widget_class->enter_notify_event = gcal_date_chooser_day_enter_notify_event;
  widget_class->leave_notify_event = gcal_date_chooser_day_leave_notify_event;

  signals[SELECTED] = g_signal_new ("selected",
                                    GCAL_TYPE_DATE_CHOOSER_DAY,
                                    G_SIGNAL_RUN_FIRST,
                                    0,
                                    NULL,
                                    NULL,
                                    NULL,
                                    G_TYPE_NONE, 0);

  gtk_widget_class_set_css_name (widget_class, "day");
}

static void
gcal_date_chooser_day_init (GcalDateChooserDay *self)
{
  GtkWidget *widget = GTK_WIDGET (self);

  gtk_widget_set_halign (widget, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);

  gtk_widget_set_can_focus (widget, TRUE);
  gtk_style_context_add_class (gtk_widget_get_style_context (widget), "day");

  self->label = gtk_label_new ("");
  gtk_widget_show (self->label);
  gtk_widget_set_halign (self->label, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (self->label, GTK_ALIGN_CENTER);
  gtk_widget_set_hexpand (self->label, TRUE);
  gtk_widget_set_vexpand (self->label, TRUE);

  gtk_container_add (GTK_CONTAINER (self), self->label);

  self->multipress_gesture = gtk_gesture_multi_press_new (widget);
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (self->multipress_gesture), 0);

  g_signal_connect (self->multipress_gesture,
                    "pressed",
                    G_CALLBACK (day_pressed),
                    self);
}

GtkWidget*
gcal_date_chooser_day_new (void)
{
  return g_object_new (GCAL_TYPE_DATE_CHOOSER_DAY, NULL);
}

void
gcal_date_chooser_day_set_date (GcalDateChooserDay *self,
                                GDateTime          *date)
{
  gchar *text;

  g_clear_pointer (&self->date, g_date_time_unref);
  self->date = g_date_time_ref (date);

  text = g_strdup_printf ("%d", g_date_time_get_day_of_month (date));
  gtk_label_set_label (GTK_LABEL (self->label), text);
  g_free (text);
}

GDateTime*
gcal_date_chooser_day_get_date (GcalDateChooserDay *self)
{
  return self->date;
}

void
gcal_date_chooser_day_set_other_month (GcalDateChooserDay *self,
                                       gboolean            other_month)
{
  GtkStyleContext *context;

  context = gtk_widget_get_style_context (GTK_WIDGET (self));

  if (other_month)
    {
      gtk_style_context_add_class (context, "other-month");
      gtk_drag_source_unset (GTK_WIDGET (self));
    }
  else
    {
      gtk_style_context_remove_class (context, "other-month");
      gtk_drag_source_set (GTK_WIDGET (self),
                           GDK_BUTTON1_MASK | GDK_BUTTON3_MASK,
                           NULL, 0,
                           GDK_ACTION_COPY);
      gtk_drag_source_add_text_targets (GTK_WIDGET (self));
    }
}

void
gcal_date_chooser_day_set_selected (GcalDateChooserDay *self,
                                    gboolean            selected)
{
  if (selected)
    gtk_widget_set_state_flags (GTK_WIDGET (self), GTK_STATE_FLAG_SELECTED, FALSE);
  else
    gtk_widget_unset_state_flags (GTK_WIDGET (self), GTK_STATE_FLAG_SELECTED);
}

void
gcal_date_chooser_day_set_options (GcalDateChooserDay        *self,
                                   GcalDateChooserDayOptions  options)
{
  GtkStyleContext *context;

  context = gtk_widget_get_style_context (GTK_WIDGET (self));

  if (options & GCAL_DATE_CHOOSER_DAY_WEEKEND)
    gtk_style_context_add_class (context, "weekend");
  else
    gtk_style_context_remove_class (context, "weekend");

  if (options & GCAL_DATE_CHOOSER_DAY_HOLIDAY)
    gtk_style_context_add_class (context, "holiday");
  else
    gtk_style_context_remove_class (context, "holiday");

  if (options & GCAL_DATE_CHOOSER_DAY_MARKED)
    gtk_style_context_add_class (context, "marked");
  else
    gtk_style_context_remove_class (context, "marked");
}
