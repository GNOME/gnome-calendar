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

#include "gcal-application.h"
#include "gcal-clock.h"
#include "gcal-context.h"
#include "gcal-debug.h"
#include "gcal-range-tree.h"
#include "gcal-timeline-subscriber.h"
#include "gcal-year-view.h"
#include "gcal-view.h"
#include "gcal-utils.h"

#include <glib/gi18n.h>

#define NAVIGATOR_MAX_GRID_SIZE 4
#define VISUAL_CLUES_SIDE 3.0
#define WEEK_NUMBER_MARGIN 3.0

typedef struct
{
  GtkWidget         *week_number[6];
  GtkWidget         *day[31];
  GtkWidget         *grid;
} MonthData;

struct _GcalYearView
{
  GtkBox              parent;

  GtkFlowBox         *flowbox;
  GtkLabel           *year_label;

  /**
   * first day of the week according to user locale, being
   * 0 for Sunday, 1 for Monday and so on
   */
  gint                first_weekday;

  GSettings          *calendar_settings;

  MonthData           months[12];

  GDateTime          *date;
  GcalContext        *context;
  gulong              update_indicators_idle_id;

  GcalRangeTree      *events;
};

enum {
  PROP_0,
  PROP_DATE,
  PROP_CONTEXT,
  LAST_PROP
};

static void          gcal_view_interface_init                    (GcalViewInterface  *iface);

static void          gcal_timeline_subscriber_interface_init     (GcalTimelineSubscriberInterface *iface);

static gboolean      update_event_indicators_in_idle_cb          (gpointer           data);

G_DEFINE_TYPE_WITH_CODE (GcalYearView, gcal_year_view, GTK_TYPE_BOX,
                         G_IMPLEMENT_INTERFACE (GCAL_TYPE_VIEW, gcal_view_interface_init)
                         G_IMPLEMENT_INTERFACE (GCAL_TYPE_TIMELINE_SUBSCRIBER,
                                                gcal_timeline_subscriber_interface_init));

static GtkWidget*
create_day_label (guint day)
{
  g_autofree gchar *day_str = NULL;
  GtkWidget *overlay;
  GtkWidget *label;
  GtkWidget *dot;

  day_str = g_strdup_printf ("%u", day);
  label = gtk_label_new (day_str);
  gtk_widget_set_halign (label, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
  gtk_widget_add_css_class (label, "days");
  gtk_widget_add_css_class (label, "numeric");

  dot = g_object_new (ADW_TYPE_BIN,
                      "css-name", "dot",
                      "valign", GTK_ALIGN_END,
                      "halign", GTK_ALIGN_CENTER,
                      NULL);

  overlay = gtk_overlay_new ();
  gtk_overlay_set_child (GTK_OVERLAY (overlay), label);
  gtk_overlay_add_overlay (GTK_OVERLAY (overlay), dot);
  g_object_set_data (G_OBJECT (overlay), "dot", dot);

  return overlay;
}

static void
set_dot_visible (GtkWidget *day_widget,
                 gboolean   visible)
{
  GtkWidget *dot = g_object_get_data (G_OBJECT (day_widget), "dot");
  gtk_widget_set_visible (dot, visible);
}

static void
init_month_data (GcalYearView *self,
                 guint         month,
                 MonthData    *month_data)
{
  GtkWidget *month_name;
  GtkWidget *separator;
  guint i;

  month_data->grid = g_object_new (GTK_TYPE_GRID,
                                   "column-spacing", 6,
                                   "row-spacing", 6,
                                   "column-spacing", 6,
                                   "hexpand", TRUE,
                                   "vexpand", TRUE,
                                   NULL);
  gtk_widget_add_css_class (month_data->grid, "month");
  gtk_widget_add_css_class (month_data->grid, "card");
  gtk_widget_add_css_class (month_data->grid, "activatable");
  gtk_flow_box_append (self->flowbox, month_data->grid);

  month_name = gtk_label_new (gcal_get_month_name (month));
  gtk_widget_set_hexpand (month_name, TRUE);
  gtk_widget_add_css_class (month_name, "accent");
  gtk_widget_add_css_class (month_name, "title-4");
  gtk_grid_attach (GTK_GRID (month_data->grid), month_name, 0, 0, 8, 1);

  separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_grid_attach (GTK_GRID (month_data->grid), separator, 0, 1, 8, 1);

  for (i = 0; i < 6; i++)
    {
      month_data->week_number[i] = gtk_label_new (NULL);
      gtk_widget_set_halign (month_data->week_number[i], GTK_ALIGN_CENTER);
      gtk_widget_set_valign (month_data->week_number[i], GTK_ALIGN_CENTER);
      gtk_widget_add_css_class (month_data->week_number[i], "week-number");
      gtk_grid_attach (GTK_GRID (month_data->grid), month_data->week_number[i], 0, i + 2, 1, 1);
    }

  for (i = 0; i < 31; i++)
    {
      month_data->day[i] = create_day_label (i + 1);

      gtk_grid_attach (GTK_GRID (month_data->grid),
                       month_data->day[i],
                       1 + i % 7,
                       2 + i / 7,
                       1,
                       1);
    }
}

static void
update_weekday_widgets (GcalYearView *self)
{
  gboolean show_weekdate;
  guint month;

  GCAL_ENTRY;

  show_weekdate = g_settings_get_boolean (self->calendar_settings, "show-weekdate");

  for (month = 0; month < 12; month++)
    {
      g_autoptr (GDateTime) week_date = NULL;
      GDate date;
      gint days_delay;
      gint week;

      g_date_set_dmy (&date, 1, month + 1, g_date_time_get_year (self->date));
      days_delay = 7 - (7 - ((g_date_get_weekday (&date) % 7) - (self->first_weekday % 7)));

      /* Week numbers */
      week_date = g_date_time_new_local (g_date_time_get_year (self->date),
                                         month + 1,
                                         1, 0, 0, 0);

      for (week = 0; week < 6; week++)
        {
          g_autoptr (GDateTime) next_week_date = NULL;
          GtkWidget *label;
          gboolean valid_date;

          valid_date = g_date_valid_dmy (MAX (week * 7 + 1 - days_delay, 1),
                                         month + 1,
                                         g_date_time_get_year (self->date));

          label = self->months[month].week_number[week];
          gtk_widget_set_visible (label, valid_date && show_weekdate);

          if (valid_date)
            {
              g_autofree gchar *week_number = NULL;

              week_number = g_strdup_printf ("%d", g_date_time_get_week_of_year (week_date));
              gtk_label_set_label (GTK_LABEL (label), week_number);
            }

          next_week_date = g_date_time_add_days (week_date, 7);
          gcal_set_date_time (&week_date, next_week_date);
        }
    }

  GCAL_EXIT;
}

static void
update_year_label (GcalYearView *self)
{
  g_autofree gchar *year = NULL;

  year = g_date_time_format (self->date, "%Y");
  gtk_label_set_label (self->year_label, year);
}

static void
update_event_indicators (GcalYearView *self)
{
  guint month;

  GCAL_ENTRY;

  for (month = 0; month < 12; month++)
    {
      guint day;

      for (day = 0; day < 31; day++)
        {
          g_autoptr (GcalRange) range = NULL;
          g_autoptr (GDateTime) date = NULL;

          if (!g_date_valid_dmy (day + 1, month + 1, g_date_time_get_year (self->date)))
            continue;

          date = g_date_time_new_local (g_date_time_get_year (self->date),
                                        month + 1,
                                        day + 1,
                                        0, 0, 0);
          range = gcal_range_new_take (g_date_time_ref (date),
                                       g_date_time_add_days (date, 1),
                                       GCAL_RANGE_DEFAULT);

          set_dot_visible (self->months[month].day[day],
                           gcal_range_tree_count_entries_at_range (self->events, range) > 0);
        }
    }

  GCAL_EXIT;
}

static void
update_month_grid (GcalYearView *self)
{
  g_autoptr (GDateTime) now = NULL;
  gboolean show_weekdate;
  guint month;

  GCAL_ENTRY;

  now = g_date_time_new_now_local ();
  show_weekdate = g_settings_get_boolean (self->calendar_settings, "show-weekdate");

  for (month = 0; month < 12; month++)
    {
      g_autoptr (GDateTime) week_date = NULL;
      GDate date;
      guint day;
      gint days_delay;
      gint column;
      gint week;
      gint row;

      g_date_set_dmy (&date, 1, month + 1, g_date_time_get_year (self->date));
      days_delay = 7 - (7 - ((g_date_get_weekday (&date) % 7) - (self->first_weekday % 7)));

      column = days_delay;
      row = 2;

      /* Week numbers */
      week_date = g_date_time_new_local (g_date_time_get_year (self->date),
                                         month + 1,
                                         1, 0, 0, 0);

      for (week = 0; week < 6; week++)
        {
          g_autoptr (GDateTime) next_week_date = NULL;
          GtkWidget *label;
          gboolean valid_date;

          valid_date = g_date_valid_dmy (MAX (week * 7 + 1 - days_delay, 1),
                                         month + 1,
                                         g_date_time_get_year (self->date));

          label = self->months[month].week_number[week];
          gtk_widget_set_visible (label, valid_date && show_weekdate);

          if (valid_date)
            {
              g_autofree gchar *week_number = NULL;

              week_number = g_strdup_printf ("%d", g_date_time_get_week_of_year (week_date));
              gtk_label_set_label (GTK_LABEL (label), week_number);
            }

          next_week_date = g_date_time_add_days (week_date, 7);
          gcal_set_date_time (&week_date, next_week_date);
        }

      /* Month days */
      for (day = 0; day < 31; day++)
        {
          g_autoptr (GDateTime) day_datetime = NULL;
          GtkLayoutManager *layout_manager;
          GtkLayoutChild *layout_child;
          GtkWidget *label;
          gboolean valid_date;
          gint weekday;

          label = self->months[month].day[day];
          valid_date = g_date_valid_dmy (day + 1, month + 1, g_date_time_get_year (self->date));

          gtk_widget_set_visible (label, valid_date);

          if (!valid_date)
            continue;

          day_datetime = g_date_time_new_local (g_date_time_get_year (self->date),
                                                month + 1,
                                                day + 1,
                                                0, 0, 0);

          g_date_set_dmy (&date, day + 1, month + 1, g_date_time_get_year (self->date));
          weekday = g_date_get_weekday (&date) % 7;

          if (!is_workday (weekday))
            gtk_widget_add_css_class (label, "non-workday");
          else
            gtk_widget_remove_css_class (label, "non-workday");

          if (g_date_time_get_month (now) == month + 1 &&
              g_date_time_get_day_of_month (now) == day + 1 &&
              g_date_time_get_year (now) == g_date_time_get_year (self->date))
            gtk_widget_add_css_class (label, "current");
          else
            gtk_widget_remove_css_class (label, "current");

          layout_manager = gtk_widget_get_layout_manager (GTK_WIDGET (self->months[month].grid));
          layout_child = gtk_layout_manager_get_layout_child (layout_manager, label);

          gtk_grid_layout_child_set_column (GTK_GRID_LAYOUT_CHILD (layout_child), column + 1);
          gtk_grid_layout_child_set_row (GTK_GRID_LAYOUT_CHILD (layout_child), row);

          column++;
          if (column >= 7)
            row++;
          column %= 7;
        }
    }

  GCAL_EXIT;
}

static void
queue_update_event_indicators (GcalYearView *self)
{
  if (self->update_indicators_idle_id > 0)
    return;

  self->update_indicators_idle_id = g_idle_add_full (G_PRIORITY_LOW,
                                                     update_event_indicators_in_idle_cb,
                                                     self,
                                                     NULL);
}


/*
 * Callbacks
 */

static gboolean
update_event_indicators_in_idle_cb (gpointer data)
{
  GcalYearView *self = GCAL_YEAR_VIEW (data);

  update_event_indicators (self);

  self->update_indicators_idle_id = 0;
  return G_SOURCE_REMOVE;
}

static void
on_clock_day_changed_cb (GcalClock    *clock,
                         GcalYearView *self)
{
  GCAL_ENTRY;

  update_month_grid (self);

  GCAL_EXIT;
}

static void
on_flowbox_child_activated_cb (GtkFlowBox      *flowbox,
                               GtkFlowBoxChild *child,
                               GcalYearView    *self)
{
  g_autoptr (GDateTime) date = NULL;
  gint child_index;

  GCAL_ENTRY;

  child_index = gtk_flow_box_child_get_index (child);
  date = g_date_time_new_local (g_date_time_get_year (self->date),
                                child_index + 1,
                                1, 0, 0, 0);

  gcal_set_date_time (&self->date, date);
  g_object_notify (G_OBJECT (self), "active-date");

  gtk_widget_activate_action (GTK_WIDGET (self), "win.change-view", "i", 1);

  GCAL_EXIT;
}

static void
on_show_weekdate_changed_cb (GSettings    *settings,
                             gchar        *key,
                             GcalYearView *self)
{
  GCAL_ENTRY;

  update_weekday_widgets (self);

  GCAL_EXIT;
}


/*
 * GcalTimelineSubscriber implementation
 */

static GcalRange*
gcal_year_view_get_range (GcalTimelineSubscriber *subscriber)
{
  g_autoptr (GDateTime) start = NULL;
  g_autoptr (GDateTime) end = NULL;
  GcalYearView *self;

  self = GCAL_YEAR_VIEW (subscriber);
  start = g_date_time_new_local (g_date_time_get_year (self->date), 1, 1, 0, 0, 0);
  end = g_date_time_add_years (start, 1);

  return gcal_range_new (start, end, GCAL_RANGE_DEFAULT);
}

static void
gcal_year_view_add_event (GcalTimelineSubscriber *subscriber,
                          GcalEvent              *event)
{
  GcalYearView *self;

  GCAL_ENTRY;

  self = GCAL_YEAR_VIEW (subscriber);

  g_debug ("Caching event '%s' in Year view", gcal_event_get_uid (event));
  gcal_range_tree_add_range (self->events, gcal_event_get_range (event), g_object_ref (event));

  queue_update_event_indicators (self);

  GCAL_EXIT;
}

static void
gcal_year_view_remove_event (GcalTimelineSubscriber *subscriber,
                             GcalEvent              *event)
{
  GcalYearView *self;

  GCAL_ENTRY;

  self = GCAL_YEAR_VIEW (subscriber);

  g_debug ("Removing event '%s' from Year view's cache", gcal_event_get_uid (event));
  gcal_range_tree_remove_range (self->events, gcal_event_get_range (event), event);

  queue_update_event_indicators (self);

  GCAL_EXIT;
}

static void
gcal_year_view_update_event (GcalTimelineSubscriber *subscriber,
                             GcalEvent              *event)
{
  GCAL_ENTRY;

  gcal_year_view_remove_event (subscriber, event);
  gcal_year_view_add_event (subscriber, event);

  GCAL_EXIT;
}

static void
gcal_timeline_subscriber_interface_init (GcalTimelineSubscriberInterface *iface)
{
  iface->get_range = gcal_year_view_get_range;
  iface->add_event = gcal_year_view_add_event;
  iface->remove_event = gcal_year_view_remove_event;
  iface->update_event = gcal_year_view_update_event;
}


/*
 * GcalView interface
 */

static GDateTime*
gcal_year_view_get_date (GcalView *view)
{
  GcalYearView *self = GCAL_YEAR_VIEW (view);

  return self->date;
}

static void
gcal_year_view_set_date (GcalView  *view,
                         GDateTime *date)
{
  GcalYearView *self;
  gboolean year_changed;

  GCAL_ENTRY;

  self = GCAL_YEAR_VIEW (view);

  year_changed = !self->date || g_date_time_get_year (self->date) != g_date_time_get_year (date);

  gcal_set_date_time (&self->date, date);

  if (year_changed)
    gcal_timeline_subscriber_range_changed (GCAL_TIMELINE_SUBSCRIBER (view));

  update_weekday_widgets (self);
  update_month_grid (self);
  update_year_label (self);

  GCAL_EXIT;
}

static GList*
gcal_year_view_get_children_by_uuid (GcalView              *view,
                                     GcalRecurrenceModType  mod,
                                     const gchar           *uuid)
{
  return NULL;
}

static GDateTime*
gcal_year_view_get_next_date (GcalView *view)
{
  GcalYearView *self = GCAL_YEAR_VIEW (view);

  g_assert (self->date != NULL);
  return g_date_time_add_years (self->date, 1);
}


static GDateTime*
gcal_year_view_get_previous_date (GcalView *view)
{
  GcalYearView *self = GCAL_YEAR_VIEW (view);

  g_assert (self->date != NULL);
  return g_date_time_add_years (self->date, -1);
}

static void
gcal_view_interface_init (GcalViewInterface *iface)
{
  iface->get_date = gcal_year_view_get_date;
  iface->set_date = gcal_year_view_set_date;
  iface->get_children_by_uuid = gcal_year_view_get_children_by_uuid;
  iface->get_next_date = gcal_year_view_get_next_date;
  iface->get_previous_date = gcal_year_view_get_previous_date;
}

static void
gcal_year_view_finalize (GObject *object)
{
  GcalYearView *year_view = GCAL_YEAR_VIEW (object);

  g_clear_pointer (&year_view->date, g_date_time_unref);
  g_clear_pointer (&year_view->events, gcal_range_tree_unref);
  g_clear_object (&year_view->calendar_settings);

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

    case PROP_CONTEXT:
      g_value_set_object (value, self->context);
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

    case PROP_CONTEXT:
      g_assert (self->context == NULL);
      self->context = g_value_dup_object (value);

      g_signal_connect_object (gcal_context_get_clock (self->context),
                               "day-changed",
                               G_CALLBACK (on_clock_day_changed_cb),
                               self,
                               G_CONNECT_SWAPPED);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_year_view_class_init (GcalYearViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gcal_year_view_finalize;
  object_class->get_property = gcal_year_view_get_property;
  object_class->set_property = gcal_year_view_set_property;

  g_object_class_override_property (object_class, PROP_DATE, "active-date");
  g_object_class_override_property (object_class, PROP_CONTEXT, "context");

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/views/gcal-year-view.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalYearView, flowbox);
  gtk_widget_class_bind_template_child (widget_class, GcalYearView, year_label);

  gtk_widget_class_bind_template_callback (widget_class, on_flowbox_child_activated_cb);

  gtk_widget_class_set_css_name (widget_class, "calendar-view");
}

static void
gcal_year_view_init (GcalYearView *self)
{
  guint i;

  gtk_widget_init_template (GTK_WIDGET (self));

  self->first_weekday = get_first_weekday ();
  self->events = gcal_range_tree_new_with_free_func (g_object_unref);

  /* bind GNOME Shell' show week numbers property to GNOME Calendar's one */
  self->calendar_settings = g_settings_new ("org.gnome.desktop.calendar");
  g_signal_connect (self->calendar_settings, "changed::show-weekdate", G_CALLBACK (on_show_weekdate_changed_cb), self);

  for (i = 0; i < 12; i++)
    init_month_data (self, i, &self->months[i]);
}
