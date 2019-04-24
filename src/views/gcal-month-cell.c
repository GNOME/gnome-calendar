/* gcal-month-cell.c
 *
 * Copyright (C) 2017 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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


#define G_LOG_DOMAIN "GcalMonthCell"

#include "config.h"
#include "gcal-application.h"
#include "gcal-clock.h"
#include "gcal-event-widget.h"
#include "gcal-utils.h"
#include "gcal-month-cell.h"

struct _GcalMonthCell
{
  GtkEventBox         parent;

  GDateTime          *date;
  guint               n_overflow;

  GtkLabel           *day_label;
  GtkWidget          *header_box;
  GtkImage           *weather_icon;
  GtkLabel           *temp_label;

  GtkWidget          *overflow_button;
  GtkWidget          *overflow_label;
  GtkWidget          *overlay;

  gboolean            different_month;
  gboolean            selected;

  /* internal fields */
  gboolean            hovered : 1;
  gboolean            pressed : 1;

  GcalContext        *context;

  GcalWeatherInfo    *weather_info;
};

G_DEFINE_TYPE (GcalMonthCell, gcal_month_cell, GTK_TYPE_EVENT_BOX)

enum
{
  SHOW_OVERFLOW,
  N_SIGNALS
};

enum
{
  PROP_0,
  PROP_CONTEXT,
  N_PROPS
};

static guint signals[N_SIGNALS] = { 0, };
static GParamSpec *properties[N_PROPS] = { 0, };


/*
 * Auxiliary methods
 */

static void
update_style_flags (GcalMonthCell *self)
{
  g_autoptr (GDateTime) today;
  GtkStyleContext *context;
  GtkStateFlags flags;
  GtkWidget *widget;

  widget = GTK_WIDGET (self);
  flags = gtk_widget_get_state_flags (widget);

  if (self->hovered)
    flags |= GTK_STATE_FLAG_PRELIGHT;
  else
    flags &= ~GTK_STATE_FLAG_PRELIGHT;

  gtk_widget_set_state_flags (widget, flags, TRUE);

  /* Today */
  today = g_date_time_new_now_local ();
  context = gtk_widget_get_style_context (GTK_WIDGET (self));

  if (datetime_compare_date (self->date, today) == 0)
    gtk_style_context_add_class (context, "today");
  else
    gtk_style_context_remove_class (context, "today");
}

static gboolean
should_propagate_button_event (GtkWidget      *widget,
                               GdkEventButton *event_button)
{
  GcalMonthCell *self;
  gdouble x, y;
  gint button_height, height;

  self = GCAL_MONTH_CELL (widget);
  height = gtk_widget_get_allocated_height (widget);
  button_height = gtk_widget_get_allocated_height (self->overflow_button);

  x = event_button->x;
  y = event_button->y;

  if (!gcal_translate_child_window_position (widget, event_button->window, x, y, &x, &y))
    return GDK_EVENT_PROPAGATE;

  if (self->n_overflow > 0 && y > height - button_height)
    return GDK_EVENT_STOP;

  return GDK_EVENT_PROPAGATE;
}


/*
 * Callbacks
 */

static void
day_changed_cb (GcalClock     *clock,
                GcalMonthCell *self)
{
  update_style_flags (self);
}

static gboolean
enter_notify_event_cb (GtkWidget        *widget,
                       GdkEventCrossing *event_crossing,
                       GcalMonthCell    *self)
{
  self->hovered = TRUE;

  update_style_flags (self);

  return GDK_EVENT_PROPAGATE;
}

static gboolean
leave_notify_event_cb (GtkWidget        *widget,
                       GdkEventCrossing *event_crossing,
                       GcalMonthCell    *self)
{
  self->hovered = FALSE;

  update_style_flags (self);

  return GDK_EVENT_PROPAGATE;
}

static void
overflow_button_clicked_cb (GtkWidget     *button,
                            GcalMonthCell *self)
{
  g_signal_emit (self, signals[SHOW_OVERFLOW], 0, button);
}

/*
 * GtkWidget overrides
 */
static gboolean
gcal_month_cell_drag_motion (GtkWidget      *widget,
                             GdkDragContext *context,
                             gint            x,
                             gint            y,
                             guint           time)
{
  GcalMonthCell *self;

  self = GCAL_MONTH_CELL (widget);

  if (self->different_month)
    gtk_drag_unhighlight (widget);
  else
    gtk_drag_highlight (widget);

  gdk_drag_status (context, self->different_month ? 0 : GDK_ACTION_MOVE, time);

  return !self->different_month;
}

static gboolean
gcal_month_cell_drag_drop (GtkWidget      *widget,
                           GdkDragContext *context,
                           gint            x,
                           gint            y,
                           guint           time)
{
  GcalRecurrenceModType mod;
  GcalMonthCell *self;
  GtkWidget *event_widget;
  GDateTime *start_dt, *end_dt;
  GcalEvent *event;
  ESource *source;
  gint diff;
  gint start_month, current_month;
  gint start_year, current_year;
  GTimeSpan timespan = 0;

  self = GCAL_MONTH_CELL (widget);
  event_widget = gtk_drag_get_source_widget (context);
  mod = GCAL_RECURRENCE_MOD_THIS_ONLY;

  if (!GCAL_IS_EVENT_WIDGET (event_widget))
    return FALSE;

  if (self->different_month)
    return FALSE;

  event = gcal_event_widget_get_event (GCAL_EVENT_WIDGET (event_widget));
  source = gcal_event_get_source (event);

  if (gcal_event_has_recurrence (event) &&
      !ask_recurrence_modification_type (widget, &mod, source))
    {
      goto out;
    }

  /* Move the event's date */
  start_dt = gcal_event_get_date_start (event);
  end_dt = gcal_event_get_date_end (event);

  start_month = g_date_time_get_month (start_dt);
  start_year = g_date_time_get_year (start_dt);

  current_month = g_date_time_get_month (self->date);
  current_year = g_date_time_get_year (self->date);

  timespan = g_date_time_difference (end_dt, start_dt);

  start_dt = g_date_time_add_full (start_dt,
                                   current_year - start_year,
                                   current_month - start_month,
                                   0, 0, 0, 0);

  diff = g_date_time_get_day_of_month (self->date) - g_date_time_get_day_of_month (start_dt);

  if (diff != 0 ||
      current_month != start_month ||
      current_year != start_year)
    {
      g_autoptr (GDateTime) new_start;

       new_start = g_date_time_add_days (start_dt, diff);

      gcal_event_set_date_start (event, new_start);

      /* The event may have a NULL end date, so we have to check it here */
      if (end_dt)
        {
          GDateTime *new_end = g_date_time_add (new_start, timespan);

          gcal_event_set_date_end (event, new_end);
          g_clear_pointer (&new_end, g_date_time_unref);
        }

      gcal_manager_update_event (gcal_context_get_manager (self->context), event, mod);
    }

  g_clear_pointer (&start_dt, g_date_time_unref);

out:
  /* Cancel the DnD */
  gtk_drag_unhighlight (widget);
  gtk_drag_finish (context, TRUE, FALSE, time);

  return TRUE;
}

static void
gcal_month_cell_drag_leave (GtkWidget      *widget,
                            GdkDragContext *context,
                            guint           time)
{
  gtk_drag_unhighlight (widget);
}

static gboolean
gcal_month_cell_button_press_event (GtkWidget      *widget,
                                    GdkEventButton *event_button)
{
  GcalMonthCell *self = GCAL_MONTH_CELL (widget);

  self->pressed = TRUE;

  return should_propagate_button_event (widget, event_button);
}

static gboolean
gcal_month_cell_button_release_event (GtkWidget      *widget,
                                      GdkEventButton *event_button)
{
  GcalMonthCell *self = GCAL_MONTH_CELL (widget);

  if (!self->pressed)
    return GDK_EVENT_STOP;

  self->pressed = FALSE;

  return should_propagate_button_event (widget, event_button);
}

static gboolean
gcal_month_cell_draw (GtkWidget *widget,
                      cairo_t   *cr)
{
  GtkStyleContext *context;
  gint width, height;

  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);
  context = gtk_widget_get_style_context (widget);

  gtk_render_background (context, cr, 0, 0, width, height);
  gtk_render_frame (context, cr, 0, 0, width, height);

  if (gtk_widget_has_focus (widget))
    gtk_render_focus (context, cr, 0, 0, width, height);

  return GTK_WIDGET_CLASS (gcal_month_cell_parent_class)->draw (widget, cr);
}


/*
 * GObject overrides
 */

static void
gcal_month_cell_finalize (GObject *object)
{
  GcalMonthCell *self = (GcalMonthCell *)object;

  gcal_clear_datetime (&self->date);
  g_clear_object (&self->context);

  G_OBJECT_CLASS (gcal_month_cell_parent_class)->finalize (object);
}


static void
gcal_month_cell_set_property (GObject       *object,
                              guint          property_id,
                              const GValue  *value,
                              GParamSpec    *pspec)
{
  GcalMonthCell *self = (GcalMonthCell *) object;

  switch (property_id)
    {
    case PROP_CONTEXT:
      gcal_month_cell_set_context (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gcal_month_cell_get_property (GObject       *object,
                              guint          property_id,
                              GValue        *value,
                              GParamSpec    *pspec)
{
  GcalMonthCell *self = (GcalMonthCell *) object;

  switch (property_id)
    {
    case PROP_CONTEXT:
      g_value_set_object (value, self->context);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gcal_month_cell_class_init (GcalMonthCellClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gcal_month_cell_finalize;
  object_class->set_property = gcal_month_cell_set_property;
  object_class->get_property = gcal_month_cell_get_property;

  widget_class->button_press_event = gcal_month_cell_button_press_event;
  widget_class->button_release_event = gcal_month_cell_button_release_event;
  widget_class->drag_motion = gcal_month_cell_drag_motion;
  widget_class->drag_drop = gcal_month_cell_drag_drop;
  widget_class->drag_leave = gcal_month_cell_drag_leave;
  widget_class->draw = gcal_month_cell_draw;

  signals[SHOW_OVERFLOW] = g_signal_new ("show-overflow",
                                         GCAL_TYPE_MONTH_CELL,
                                         G_SIGNAL_RUN_LAST,
                                         0, NULL, NULL, NULL,
                                         G_TYPE_NONE,
                                         1,
                                         GTK_TYPE_WIDGET);

  properties[PROP_CONTEXT] = g_param_spec_object ("context",
                                                  "Context",
                                                  "The GcalContext of the application",
                                                  GCAL_TYPE_CONTEXT,
                                                  G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/month-cell.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalMonthCell, day_label);
  gtk_widget_class_bind_template_child (widget_class, GcalMonthCell, header_box);
  gtk_widget_class_bind_template_child (widget_class, GcalMonthCell, overflow_button);
  gtk_widget_class_bind_template_child (widget_class, GcalMonthCell, overflow_label);
  gtk_widget_class_bind_template_child (widget_class, GcalMonthCell, overlay);
  gtk_widget_class_bind_template_child (widget_class, GcalMonthCell, temp_label);
  gtk_widget_class_bind_template_child (widget_class, GcalMonthCell, weather_icon);

  gtk_widget_class_bind_template_callback (widget_class, enter_notify_event_cb);
  gtk_widget_class_bind_template_callback (widget_class, leave_notify_event_cb);
  gtk_widget_class_bind_template_callback (widget_class, overflow_button_clicked_cb);

  gtk_widget_class_set_css_name (widget_class, "monthcell");
}

static void
gcal_month_cell_init (GcalMonthCell *self)
{
  GcalApplication *application;

  gtk_widget_init_template (GTK_WIDGET (self));

  /* Setup the month cell as a drag n' drop destination */
  gtk_drag_dest_set (GTK_WIDGET (self),
                     0,
                     NULL,
                     0,
                     GDK_ACTION_MOVE);

  /* Connect to the wall clock */
  application = GCAL_APPLICATION (g_application_get_default ());

  g_assert (application != NULL);

  g_signal_connect_object (gcal_application_get_clock (application),
                           "day-changed",
                           G_CALLBACK (day_changed_cb),
                           self,
                           0);

}

GtkWidget*
gcal_month_cell_new (void)
{
  return g_object_new (GCAL_TYPE_MONTH_CELL, NULL);
}


GDateTime*
gcal_month_cell_get_date (GcalMonthCell *self)
{
  g_return_val_if_fail (GCAL_IS_MONTH_CELL (self), NULL);

  return self->date;
}

void
gcal_month_cell_set_date (GcalMonthCell *self,
                          GDateTime     *date)
{
  g_autofree gchar *text = NULL;

  g_return_if_fail (GCAL_IS_MONTH_CELL (self));

  if (self->date && date && datetime_compare_date (self->date, date) == 0)
    return;

  gcal_clear_datetime (&self->date);
  self->date = g_date_time_ref (date);

  /* Day label */
  text = g_strdup_printf ("%d", g_date_time_get_day_of_month (date));

  gtk_label_set_text (self->day_label, text);
  update_style_flags (self);
}

/**
 * gcal_month_cell_set_weather:
 * @self: The #GcalMonthCell instance.
 * @info: (nullable): The weather information to display.
 *
 * Sets the weather information to display for this day.
 *
 * Note that this function does not check dates nor
 * manages weather information changes on its own.
 */
void
gcal_month_cell_set_weather (GcalMonthCell   *self,
                             GcalWeatherInfo *info)
{
  g_return_if_fail (GCAL_IS_MONTH_CELL (self));
  g_return_if_fail (!info || GCAL_IS_WEATHER_INFO (info));

  if (self->weather_info == info)
    return;

  self->weather_info = info;

  if (!info)
    {
      gtk_image_clear (self->weather_icon);
      gtk_label_set_text (self->temp_label, "");
    }
  else
    {
      const gchar* icon_name; /* unowned */
      const gchar* temp_str;  /* unwoned */

      icon_name = gcal_weather_info_get_icon_name (info);
      temp_str = gcal_weather_info_get_temperature (info);

      gtk_image_set_from_icon_name (self->weather_icon, icon_name, GTK_ICON_SIZE_SMALL_TOOLBAR);
      gtk_label_set_text (self->temp_label, temp_str);
    }
}

gboolean
gcal_month_cell_get_different_month (GcalMonthCell *self)
{
  g_return_val_if_fail (GCAL_IS_MONTH_CELL (self), FALSE);

  return self->different_month;
}

void
gcal_month_cell_set_different_month (GcalMonthCell *self,
                                     gboolean       different)
{
  GtkStyleContext *context;

  g_return_if_fail (GCAL_IS_MONTH_CELL (self));

  if (self->different_month == different)
    return;

  self->different_month = different;

  context = gtk_widget_get_style_context (GTK_WIDGET (self));

  if (different)
    gtk_style_context_add_class (context, "out-of-month");
  else
    gtk_style_context_remove_class (context, "out-of-month");
}

GcalContext*
gcal_month_cell_get_context (GcalMonthCell *self)
{
  g_return_val_if_fail (GCAL_IS_MONTH_CELL (self), NULL);

  return self->context;
}

void
gcal_month_cell_set_context (GcalMonthCell *self,
                             GcalContext   *context)
{
  g_return_if_fail (GCAL_IS_MONTH_CELL (self));

  if (g_set_object (&self->context, context))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CONTEXT]);
}

guint
gcal_month_cell_get_overflow (GcalMonthCell *self)
{
  g_return_val_if_fail (GCAL_IS_MONTH_CELL (self), 0);

  return self->n_overflow;
}

void
gcal_month_cell_set_overflow (GcalMonthCell *self,
                              guint          n_overflow)
{
  g_autofree gchar *text = NULL;

  g_return_if_fail (GCAL_IS_MONTH_CELL (self));

  gtk_widget_set_visible (self->overflow_label, n_overflow > 0);
  gtk_widget_set_sensitive (self->overflow_button, n_overflow > 0);

  if (self->n_overflow == n_overflow)
    return;

  self->n_overflow = n_overflow;

  text = g_strdup_printf ("+%d", n_overflow);
  gtk_label_set_text (GTK_LABEL (self->overflow_label), text);
}

gint
gcal_month_cell_get_content_space (GcalMonthCell *self)
{
  GtkStyleContext *context;
  GtkBorder padding;
  GtkBorder border;

  g_return_val_if_fail (GCAL_IS_MONTH_CELL (self), -1);

  context = gtk_widget_get_style_context (GTK_WIDGET (self));
  gtk_style_context_get_padding (context, gtk_style_context_get_state (context), &padding);
  gtk_style_context_get_border (context, gtk_style_context_get_state (context), &border);

  return gtk_widget_get_allocated_height (GTK_WIDGET (self)) -
         gcal_month_cell_get_header_height (self) -
         padding.top - padding.bottom -
         border.top - border.bottom;
}

gint
gcal_month_cell_get_header_height (GcalMonthCell *self)
{
  GtkStyleContext *context;
  GtkBorder padding;
  GtkBorder border;

  g_return_val_if_fail (GCAL_IS_MONTH_CELL (self), -1);

  context = gtk_widget_get_style_context (GTK_WIDGET (self->header_box));
  gtk_style_context_get_padding (context, gtk_style_context_get_state (context), &padding);
  gtk_style_context_get_border (context, gtk_style_context_get_state (context), &border);

  return gtk_widget_get_allocated_height (self->header_box) +
         gtk_widget_get_margin_top (self->header_box) +
         gtk_widget_get_margin_bottom (self->header_box) +
         padding.top + padding.bottom +
         border.top + border.bottom;
}

gint
gcal_month_cell_get_overflow_height (GcalMonthCell *self)
{
  g_return_val_if_fail (GCAL_IS_MONTH_CELL (self), -1);

  return gtk_widget_get_allocated_height (self->overflow_button);
}

gboolean
gcal_month_cell_get_selected (GcalMonthCell *self)
{
  g_return_val_if_fail (GCAL_IS_MONTH_CELL (self), FALSE);

  return self->selected;
}

void
gcal_month_cell_set_selected (GcalMonthCell *self,
                              gboolean       selected)
{
  GtkStateFlags flags;

  g_return_if_fail (GCAL_IS_MONTH_CELL (self));

  if (self->selected == selected)
    return;

  self->selected = selected;

  /* Update style context state */
  flags = gtk_widget_get_state_flags (GTK_WIDGET (self));

  if (selected)
    flags |= GTK_STATE_FLAG_SELECTED;
  else
    flags &= ~GTK_STATE_FLAG_SELECTED;

  gtk_widget_set_state_flags (GTK_WIDGET (self), flags, TRUE);

  gtk_widget_queue_resize (GTK_WIDGET (self));
}
