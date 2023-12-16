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
#include "gcal-debug.h"
#include "gcal-event-widget.h"
#include "gcal-month-cell.h"
#include "gcal-utils.h"
#include "gcal-weather-info.h"

struct _GcalMonthCell
{
  AdwBin              parent;

  GDateTime          *date;
  guint               n_overflow;

  GtkLabel           *day_label;
  GtkWidget          *header_box;
  GtkLabel           *month_name_label;
  GtkImage           *weather_icon;
  GtkLabel           *temp_label;

  GtkWidget          *overflow_button;
  GtkInscription     *overflow_inscription;
  GtkWidget          *overlay;

  gboolean            different_month;
  gboolean            selected;

  GcalContext        *context;

  GcalWeatherInfo    *weather_info;
};

G_DEFINE_TYPE (GcalMonthCell, gcal_month_cell, ADW_TYPE_BIN)

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
  g_autoptr (GDateTime) today = NULL;
  gint weekday;

  /* Today */
  today = g_date_time_new_now_local ();

  if (gcal_date_time_compare_date (self->date, today) == 0)
    gtk_widget_add_css_class (GTK_WIDGET (self), "today");
  else
    gtk_widget_remove_css_class (GTK_WIDGET (self), "today");

  weekday = g_date_time_get_day_of_week (self->date);
  if (is_workday (weekday))
    gtk_widget_add_css_class (GTK_WIDGET (self), "workday");
  else
    gtk_widget_remove_css_class (GTK_WIDGET (self), "workday");
}

static void
add_month_separators (GcalMonthCell *self)
{
  gint day_of_month;

  gtk_widget_remove_css_class (GTK_WIDGET (self), "separator-top");
  gtk_widget_remove_css_class (GTK_WIDGET (self), "separator-side");

  day_of_month = g_date_time_get_day_of_month (self->date);
  if (day_of_month > 1 && day_of_month <= 7)
    {
      gtk_widget_add_css_class (GTK_WIDGET (self), "separator-top");
    }
  else if (day_of_month == 1)
    {
      gtk_widget_add_css_class (GTK_WIDGET (self), "separator-side");
      gtk_widget_add_css_class (GTK_WIDGET (self), "separator-top");
    }
}

static void
move_event (GcalMonthCell         *self,
            GcalEvent             *event,
            GcalRecurrenceModType  mod_type)
{

  g_autoptr (GcalEvent) changed_event = NULL;
  g_autoptr (GDateTime) start_dt = NULL;
  GTimeSpan timespan = 0;
  GDateTime *end_dt;
  gint diff;
  gint start_month, current_month;
  gint start_year, current_year;

  GCAL_ENTRY;

  changed_event = gcal_event_new_from_event (event);

  /* Move the event's date */
  start_dt = gcal_event_get_date_start (changed_event);
  end_dt = gcal_event_get_date_end (changed_event);

  start_month = g_date_time_get_month (start_dt);
  start_year = g_date_time_get_year (start_dt);

  current_month = g_date_time_get_month (self->date);
  current_year = g_date_time_get_year (self->date);

  timespan = g_date_time_difference (end_dt, start_dt);

  start_dt = g_date_time_add_full (start_dt,
                                   current_year - start_year,
                                   current_month - start_month,
                                   0, 0, 0, 0);

  diff = gcal_date_time_compare_date (self->date, start_dt);

  if (diff != 0)
    {
      g_autoptr (GDateTime) new_start = g_date_time_add_days (start_dt, diff);

      gcal_event_set_date_start (changed_event, new_start);

      /* The event may have a NULL end date, so we have to check it here */
      if (end_dt)
        {
          g_autoptr (GDateTime) new_end = g_date_time_add (new_start, timespan);

          gcal_event_set_date_end (changed_event, new_end);
        }

      gcal_manager_update_event (gcal_context_get_manager (self->context), changed_event, mod_type);
    }
}

static void
update_weather (GcalMonthCell *self)
{
  GcalWeatherService *weather_service;
  GcalWeatherInfo *weather_info;
  GDate date;

  if (!self->context)
    return;

  g_date_set_dmy (&date,
                  g_date_time_get_day_of_month (self->date),
                  g_date_time_get_month (self->date),
                  g_date_time_get_year (self->date));

  weather_service = gcal_context_get_weather_service (self->context);
  weather_info = gcal_weather_service_get_weather_info_for_date (weather_service, &date);

  g_assert (!weather_info || GCAL_IS_WEATHER_INFO (weather_info));

  if (self->weather_info == weather_info)
    return;

  self->weather_info = weather_info;

  if (weather_info)
    {
      const gchar *icon_name; /* unowned */
      const gchar *temp_str;  /* unwoned */

      icon_name = gcal_weather_info_get_icon_name (weather_info);
      temp_str = gcal_weather_info_get_temperature (weather_info);

      gtk_image_set_from_icon_name (self->weather_icon, icon_name);
      gtk_label_set_text (self->temp_label, temp_str);
    }
  else
    {
      gtk_image_clear (self->weather_icon);
      gtk_label_set_text (self->temp_label, "");
    }
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

static void
overflow_button_clicked_cb (GtkWidget     *button,
                            GcalMonthCell *self)
{
  g_signal_emit (self, signals[SHOW_OVERFLOW], 0, button);
}

static gboolean
on_drop_target_accept_cb (GtkDropTarget *drop_target,
                          GdkDrop       *drop,
                          GcalMonthCell *self)
{
  GCAL_ENTRY;

  if ((gdk_drop_get_actions (drop) & gtk_drop_target_get_actions (drop_target)) == 0)
    GCAL_RETURN (FALSE);

  if (!gdk_content_formats_contain_gtype (gdk_drop_get_formats (drop), GCAL_TYPE_EVENT_WIDGET))
    GCAL_RETURN (FALSE);

  GCAL_RETURN (TRUE);
}

static void
on_ask_recurrence_response_cb (GcalEvent             *event,
                               GcalRecurrenceModType  mod_type,
                               gpointer               user_data)
{
  GcalMonthCell *self = GCAL_MONTH_CELL (user_data);

  if (mod_type != GCAL_RECURRENCE_MOD_NONE)
    move_event (self, event, mod_type);
}

static gboolean
on_drop_target_drop_cb (GtkDropTarget *drop_target,
                        const GValue  *value,
                        gdouble        x,
                        gdouble        y,
                        GcalMonthCell *self)
{
  GcalEventWidget *event_widget;
  GcalEvent *event;

  GCAL_ENTRY;

  if (!G_VALUE_HOLDS (value, GCAL_TYPE_EVENT_WIDGET))
    GCAL_RETURN (FALSE);

  event_widget = g_value_get_object (value);

  event = gcal_event_widget_get_event (event_widget);

  if (gcal_event_has_recurrence (event))
    {
      gcal_utils_ask_recurrence_modification_type (GTK_WIDGET (self),
                                                   event,
                                                   FALSE,
                                                   on_ask_recurrence_response_cb,
                                                   self);
    }
  else
    {
      move_event (self, event, GCAL_RECURRENCE_MOD_THIS_ONLY);
    }

  GCAL_RETURN (TRUE);
}

static void
on_weather_service_weather_changed_cb (GcalWeatherService *weather_service,
                                       GcalMonthCell      *self)
{
  update_weather (self);
}


/*
 * GObject overrides
 */

static void
gcal_month_cell_finalize (GObject *object)
{
  GcalMonthCell *self = (GcalMonthCell *)object;

  GcalWeatherService *weather_service = gcal_context_get_weather_service (self->context);
  gcal_weather_service_release (weather_service);

  gcal_clear_date_time (&self->date);
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

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/views/gcal-month-cell.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalMonthCell, day_label);
  gtk_widget_class_bind_template_child (widget_class, GcalMonthCell, header_box);
  gtk_widget_class_bind_template_child (widget_class, GcalMonthCell, month_name_label);
  gtk_widget_class_bind_template_child (widget_class, GcalMonthCell, overflow_button);
  gtk_widget_class_bind_template_child (widget_class, GcalMonthCell, overflow_inscription);
  gtk_widget_class_bind_template_child (widget_class, GcalMonthCell, overlay);
  gtk_widget_class_bind_template_child (widget_class, GcalMonthCell, temp_label);
  gtk_widget_class_bind_template_child (widget_class, GcalMonthCell, weather_icon);

  gtk_widget_class_bind_template_callback (widget_class, overflow_button_clicked_cb);

  gtk_widget_class_set_css_name (widget_class, "monthcell");
}

static void
gcal_month_cell_init (GcalMonthCell *self)
{
  GtkDropTarget *drop_target;

  gtk_widget_init_template (GTK_WIDGET (self));

  drop_target = gtk_drop_target_new (GCAL_TYPE_EVENT_WIDGET, GDK_ACTION_COPY);
  g_signal_connect (drop_target, "accept", G_CALLBACK (on_drop_target_accept_cb), self);
  g_signal_connect (drop_target, "drop", G_CALLBACK (on_drop_target_drop_cb), self);
  gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (drop_target));
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
  gint day_of_month;

  g_return_if_fail (GCAL_IS_MONTH_CELL (self));

  if (self->date && date && gcal_date_time_compare_date (self->date, date) == 0)
    return;

  gcal_clear_date_time (&self->date);
  self->date = g_date_time_ref (date);

  /* Day label */
  text = g_strdup_printf ("%d", g_date_time_get_day_of_month (date));

  gtk_label_set_text (self->day_label, text);
  update_style_flags (self);
  add_month_separators (self);

  /* Month name */
  day_of_month = g_date_time_get_day_of_month (date);
  gtk_widget_set_visible (GTK_WIDGET (self->month_name_label), day_of_month == 1);
  if (g_date_time_get_day_of_month (date) == 1)
    {
      g_autofree gchar *month_name = g_date_time_format (date, "%b");
      gtk_label_set_text (self->month_name_label, month_name);
    }

  update_weather (self);
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
  g_return_if_fail (GCAL_IS_MONTH_CELL (self));

  if (self->different_month == different)
    return;

  self->different_month = different;

  if (different)
    gtk_widget_add_css_class (GTK_WIDGET (self), "out-of-month");
  else
    gtk_widget_remove_css_class (GTK_WIDGET (self), "out-of-month");
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

  if (!g_set_object (&self->context, context))
    return;

  g_signal_connect_object (gcal_context_get_clock (context),
                           "day-changed",
                           G_CALLBACK (day_changed_cb),
                           self,
                           0);

  g_signal_connect_object (gcal_context_get_weather_service (self->context),
                           "weather-changed",
                           G_CALLBACK (on_weather_service_weather_changed_cb),
                           self,
                           0);

  GcalWeatherService *weather_service = gcal_context_get_weather_service (self->context);
  gcal_weather_service_hold (weather_service);

  update_weather (self);

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
  g_return_if_fail (GCAL_IS_MONTH_CELL (self));

  if (self->n_overflow == n_overflow)
    return;

  self->n_overflow = n_overflow;

  gtk_widget_set_sensitive (self->overflow_button, n_overflow > 0);

  if (n_overflow > 0)
    {
      g_autofree gchar *text = g_strdup_printf ("+%d", n_overflow);
      gtk_inscription_set_text (self->overflow_inscription, text);
    }
  else
    {
      gtk_inscription_set_text (self->overflow_inscription, "");
    }
}

gint
gcal_month_cell_get_content_space (GcalMonthCell *self)
{
  GtkStyleContext *context;
  GtkBorder padding;
  GtkBorder border;

  g_return_val_if_fail (GCAL_IS_MONTH_CELL (self), -1);

  context = gtk_widget_get_style_context (GTK_WIDGET (self));
  gtk_style_context_get_padding (context, &padding);
  gtk_style_context_get_border (context, &border);

  return gtk_widget_get_height (GTK_WIDGET (self)) -
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
  gtk_style_context_get_padding (context, &padding);
  gtk_style_context_get_border (context, &border);

  return gtk_widget_get_height (self->header_box) +
         gtk_widget_get_margin_top (self->header_box) +
         gtk_widget_get_margin_bottom (self->header_box) +
         padding.top + padding.bottom +
         border.top + border.bottom;
}

gint
gcal_month_cell_get_overflow_height (GcalMonthCell *self)
{
  g_return_val_if_fail (GCAL_IS_MONTH_CELL (self), -1);

  return gtk_widget_get_height (self->overflow_button);
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
