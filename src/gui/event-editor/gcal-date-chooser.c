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

#define G_LOG_DOMAIN "GcalDateChooser"

#include "gcal-utils.h"
#include "gcal-date-chooser.h"
#include "gcal-date-chooser-day.h"
#include "gcal-multi-choice.h"
#include "gcal-range-tree.h"
#include "gcal-timeline-subscriber.h"
#include "gcal-view.h"

#include <glib/gi18n.h>
#include <langinfo.h>
#include <locale.h>
#include <stdlib.h>

#define ROWS 6
#define COLS 7
#define DAYS 42

struct _GcalDateChooser
{
  AdwBin              parent;

  GtkWidget          *month_choice;
  GtkWidget          *popover_month_choice;
  GtkWidget          *year_choice;
  GtkWidget          *combined_choice;
  GtkWidget          *grid;

  GtkWidget          *day_grid;
  GtkWidget          *corner;
  GtkWidget          *cols[COLS];
  GtkWidget          *rows[ROWS];
  GtkWidget          *days[ROWS][COLS];
  GtkWidget          *week[ROWS];

  GDateTime          *date;
  GcalContext        *context;

  gint                this_year;
  gint                week_start;

  gboolean            show_heading;
  gboolean            show_day_names;
  gboolean            show_week_numbers;
  gboolean            show_selected_week;
  gboolean            show_events;
  gboolean            split_month_year;

  GcalRangeTree      *events;

  gulong              update_indicators_idle_id;
};

static void          gcal_view_interface_init                    (GcalViewInterface  *iface);

static void          gcal_timeline_subscriber_interface_init     (GcalTimelineSubscriberInterface *iface);
static gboolean      update_event_indicators_in_idle_cb          (gpointer           data);

static void          gcal_date_chooser_set_date                  (GcalView  *view,
                                                                  GDateTime *date);

G_DEFINE_TYPE_WITH_CODE (GcalDateChooser, gcal_date_chooser, ADW_TYPE_BIN,
                         G_IMPLEMENT_INTERFACE (GCAL_TYPE_VIEW, gcal_view_interface_init)
                         G_IMPLEMENT_INTERFACE (GCAL_TYPE_TIMELINE_SUBSCRIBER,
                                                gcal_timeline_subscriber_interface_init));

enum
{
  DAY_SELECTED,
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_SHOW_HEADING,
  PROP_SHOW_DAY_NAMES,
  PROP_SHOW_WEEK_NUMBERS,
  PROP_SHOW_SELECTED_WEEK,
  PROP_SHOW_EVENTS,
  PROP_SPLIT_MONTH_YEAR,
  PROP_DATE,
  PROP_CONTEXT,
  NUM_PROPERTIES = PROP_SPLIT_MONTH_YEAR + 1,
};

static guint signals[LAST_SIGNAL] = { 0, };
static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

static const guint month_length[2][13] =
{
  { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
  { 0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
};

static gboolean
get_combined_choice_visible (gpointer user_data,
                             gboolean show_heading,
                             gboolean split_month_year)
{
  return show_heading && !split_month_year;
}

static gboolean
get_split_choice_visible (gpointer user_data,
                          gboolean show_heading,
                          gboolean split_month_year)
{
  return show_heading && split_month_year;
}

static gboolean
leap (guint year)
{
  return ((((year % 4) == 0) && ((year % 100) != 0)) || ((year % 400) == 0));
}

static void
calendar_compute_days (GcalDateChooser *self)
{
  GcalDateChooserDay *d;
  GDateTime *date;
  gint year, month, day;
  gint other_year, other_month;
  gint ndays_in_month;
  gint ndays_in_prev_month;
  gint first_day;
  gint row;
  gint col;

  g_date_time_get_ymd (self->date, &year, &month, NULL);

  ndays_in_month = month_length[leap (year)][month];

  date = g_date_time_new_local (year, month, 1, 1, 1, 1);
  first_day = g_date_time_get_day_of_week (date);
  g_date_time_unref (date);

  first_day = (first_day + COLS - self->week_start) % COLS;
  if (first_day == 0)
    first_day = COLS;

  /* Compute days of previous month */
  if (month > 1)
    ndays_in_prev_month = month_length[leap (year)][month - 1];
  else
    ndays_in_prev_month = month_length[leap (year - 1)][12];
  day = ndays_in_prev_month - first_day + 1;

  other_year = year;
  other_month = month - 1;
  if (other_month == 0)
    {
      other_month = 12;
      other_year -= 1;
    }

  for (col = 0; col < first_day; col++)
    {
      d = GCAL_DATE_CHOOSER_DAY (self->days[0][col]);
      date = g_date_time_new_local (other_year, other_month, day, 1, 1, 1);
      gcal_date_chooser_day_set_date (d, date);
      gcal_date_chooser_day_set_other_month (d, TRUE);
      g_date_time_unref (date);
      day++;
    }

  /* Compute days of current month */
  row = first_day / COLS;
  col = first_day % COLS;

  for (day = 1; day <= ndays_in_month; day++)
    {
      d = GCAL_DATE_CHOOSER_DAY (self->days[row][col]);
      date = g_date_time_new_local (year, month, day, 1, 1, 1);
      gcal_date_chooser_day_set_date (d, date);
      gcal_date_chooser_day_set_other_month (d, FALSE);
      g_date_time_unref (date);

      col++;
      if (col == COLS)
        {
          row++;
          col = 0;
        }
    }

  /* Compute days of next month */

  other_year = year;
  other_month = month + 1;
  if (other_month == 13)
    {
      other_month = 1;
      other_year += 1;
    }

  day = 1;
  for (; row < ROWS; row++)
    {
      for (; col < COLS; col++)
        {
          d = GCAL_DATE_CHOOSER_DAY (self->days[row][col]);
          date = g_date_time_new_local (other_year, other_month, day, 1, 1, 1);
          gcal_date_chooser_day_set_date (d, date);
          gcal_date_chooser_day_set_other_month (d, TRUE);
          g_date_time_unref (date);
          day++;
        }
      col = 0;
    }

  /* update week numbers */
  for (row = 0; row < ROWS; row++)
    {
      gchar *text;

      d = GCAL_DATE_CHOOSER_DAY (self->days[row][COLS - 1]);
      date = gcal_date_chooser_day_get_date (d);
      text = g_strdup_printf ("%d", g_date_time_get_week_of_year (date));
      gtk_label_set_label (GTK_LABEL (self->rows[row]), text);
      g_free (text);
    }
}

/* 0 == sunday */
static const char *
calendar_get_weekday_name (gint i)
{
  const char *locale = g_getenv ("LC_TIME");
  const char *res;
  locale_t old_loc;
  locale_t loc = (locale_t) 0;

  if (locale)
    loc = newlocale (LC_MESSAGES_MASK, locale, (locale_t) 0);

  old_loc = uselocale (loc);

  switch (i)
    {
    case 0:
      /* Translators: Calendar grid abbreviation for Sunday.
       *
       * NOTE: These grid abbreviations are always shown together
       * and in order, e.g. "S M T W T F S".
       *
       * Please make sure these abbreviations match GNOME Shell's
       * abbreviations for your language.
       */
      res = C_("grid sunday", "S");
      break;

    case 1:
      /* Translators: Calendar grid abbreviation for Monday */
      res = C_("grid monday", "M");
      break;

    case 2:
      /* Translators: Calendar grid abbreviation for Tuesday */
      res = C_("grid tuesday", "T");
      break;

    case 3:
      /* Translators: Calendar grid abbreviation for Wednesday */
      res = C_("grid wednesday", "W");
      break;

    case 4:
      /* Translators: Calendar grid abbreviation for Thursday */
      res = C_("grid thursday", "T");
      break;

    case 5:
      /* Translators: Calendar grid abbreviation for Friday */
      res = C_("grid friday", "F");
      break;

    case 6:
      /* Translators: Calendar grid abbreviation for Saturday */
      res = C_("grid saturday", "S");
      break;

    default:
      g_assert_not_reached ();
    }

  uselocale (old_loc);

  if (loc != (locale_t) 0)
    freelocale (loc);

  return res;
}

static void
calendar_init_weekday_display (GcalDateChooser *self)
{
  for (gint i = 0; i < COLS; i++)
    {
      const char *text = calendar_get_weekday_name ((i + self->week_start) % COLS);
      gtk_label_set_label (GTK_LABEL (self->cols[i]), text);
    }
}

static gchar *
format_month (GcalMultiChoice *choice,
              gint             value,
              gpointer         data)
{
  return g_strdup (gcal_get_month_name (value));
}

static gchar *
format_month_year (GcalMultiChoice *choice,
                   gint             value,
                   gpointer         data)
{
  GcalDateChooser *self = GCAL_DATE_CHOOSER (data);
  g_autoptr (GDateTime) now = g_date_time_new_now (g_date_time_get_timezone (self->date));
  gint year, month;

  year = value / 12;
  month = value % 12;

  if (g_date_time_get_year (now) == year)
    return g_strdup (gcal_get_month_name (month));
  else
    return g_strdup_printf ("%s %d", gcal_get_month_name (month), year);
}

static void
calendar_init_month_display (GcalDateChooser *self)
{
  gchar *months[13];
  gchar *month;
  gint i;

  for (i = 0; i < 12; i++)
    {
      month = gcal_get_month_name (i);
      months[i] = g_strdup (month);
    }

  months[12] = NULL;

  gcal_multi_choice_set_choices (GCAL_MULTI_CHOICE (self->month_choice),
                                (const gchar**) months);
  gcal_multi_choice_set_choices (GCAL_MULTI_CHOICE (self->popover_month_choice),
                                (const gchar**) months);

  for (i = 0; i < 12; i++)
    g_free (months[i]);

  gcal_multi_choice_set_format_callback (GCAL_MULTI_CHOICE (self->month_choice),
                                         format_month,
                                         self,
                                         NULL);
  gcal_multi_choice_set_format_callback (GCAL_MULTI_CHOICE (self->popover_month_choice),
                                         format_month,
                                         self,
                                         NULL);

  gcal_multi_choice_set_format_callback (GCAL_MULTI_CHOICE (self->combined_choice),
                                         format_month_year,
                                         self,
                                         NULL);
}

static void
calendar_update_selected_day_display (GcalDateChooser *self)
{
  GcalDateChooserDay *d;
  GDateTime *date;
  gint row, col;

  for (row = 0; row < ROWS; row++)
    {
      gboolean row_selected = FALSE;

      for (col = 0; col < COLS; col++)
      {
        gboolean day_selected;

        d = GCAL_DATE_CHOOSER_DAY (self->days[row][col]);
        date = gcal_date_chooser_day_get_date (d);
        day_selected = gcal_date_time_compare_date (date, self->date) == 0;
        gcal_date_chooser_day_set_selected (d, day_selected);
        row_selected |= day_selected;
      }

      gtk_widget_set_visible (self->week[row], row_selected && self->show_selected_week);
    }
}

static void
calendar_update_selected_day (GcalDateChooser *self)
{
  GDateTime *date;
  gint month_len;
  gint year, month, day;

  g_date_time_get_ymd (self->date, &year, &month, &day);

  month_len = month_length[leap (year)][month];

  if (month_len < day)
    {
      date = g_date_time_new_local (year, month, month_len, 1, 1, 1);
      gcal_date_chooser_set_date (GCAL_VIEW (self), date);
      g_date_time_unref (date);
    }
  else
    {
      calendar_update_selected_day_display (self);
    }
}

static void
update_event_indicators (GcalDateChooser *self)
{
  gint row, col;

  for (row = 0; row < ROWS; row++)
    {
      for (col = 0; col < COLS; col++)
      {
          GDateTime *date;
          g_autoptr (GcalRange) range = NULL;

          date = gcal_date_chooser_day_get_date (GCAL_DATE_CHOOSER_DAY (self->days[row][col]));
          range = gcal_range_new_take (g_date_time_ref (date),
                                       g_date_time_add_days (date, 1),
                                       GCAL_RANGE_DEFAULT);

          gcal_date_chooser_day_set_dot_visible (GCAL_DATE_CHOOSER_DAY (self->days[row][col]),
                                                 gcal_range_tree_count_entries_at_range (self->events, range) > 0);
      }
    }
}

static void
queue_update_event_indicators (GcalDateChooser *self)
{
  if (self->update_indicators_idle_id > 0)
    return;

  self->update_indicators_idle_id = g_idle_add_full (G_PRIORITY_LOW,
                                                     update_event_indicators_in_idle_cb,
                                                     self,
                                                     NULL);
}

static void
day_selected_cb (GcalDateChooserDay *d,
                 GcalDateChooser    *self)
{
  gcal_date_chooser_set_date (GCAL_VIEW (self), gcal_date_chooser_day_get_date (d));
}

static void
on_clock_day_changed_cb (GcalDateChooser *self)
{
  /* FIXME Update the widget to the new day. */
}

static gboolean
update_event_indicators_in_idle_cb (gpointer data)
{
  GcalDateChooser *self = GCAL_DATE_CHOOSER (data);

  update_event_indicators (self);
  self->update_indicators_idle_id = 0;

  return G_SOURCE_REMOVE;
}

/*
 * GcalView interface
 */

static void
gcal_date_chooser_set_date (GcalView  *view,
                            GDateTime *date)
{
  GcalDateChooser *self = GCAL_DATE_CHOOSER (view);
  gint y1, m1, d1, y2, m2, d2;

  g_object_freeze_notify (G_OBJECT (self));

  g_date_time_get_ymd (self->date, &y1, &m1, &d1);
  g_date_time_get_ymd (date, &y2, &m2, &d2);

  gcal_set_date_time (&self->date, date);

  if (y1 != y2 || m1 != m2)
    {
      gcal_multi_choice_set_value (GCAL_MULTI_CHOICE (self->year_choice), y2);
      gcal_multi_choice_set_value (GCAL_MULTI_CHOICE (self->month_choice), m2 - 1);
      gcal_multi_choice_set_value (GCAL_MULTI_CHOICE (self->combined_choice), y2 * 12 + m2 - 1);
      calendar_compute_days (self);
      gcal_timeline_subscriber_range_changed (GCAL_TIMELINE_SUBSCRIBER (self));
    }

  if (y1 != y2 || m1 != m2 || d1 != d2)
    {
      calendar_update_selected_day (self);
      g_signal_emit (self, signals[DAY_SELECTED], 0);
      g_object_notify (G_OBJECT (self), "active-date");
    }

  g_object_thaw_notify (G_OBJECT (self));
}

static GDateTime*
gcal_date_chooser_get_date (GcalView *view)
{
  GcalDateChooser *self = GCAL_DATE_CHOOSER (view);

  return self->date;
}

static GList*
gcal_date_chooser_get_children_by_uuid (GcalView              *view,
                                        GcalRecurrenceModType  mod,
                                        const gchar           *uuid)
{
  return NULL;
}

static GDateTime*
gcal_date_chooser_get_next_date (GcalView *view)
{
  GcalDateChooser *self = GCAL_DATE_CHOOSER (view);

  g_assert (self->date != NULL);
  return g_date_time_add_months (self->date, 1);
}

static GDateTime*
gcal_date_chooser_get_previous_date (GcalView *view)
{
  GcalDateChooser *self = GCAL_DATE_CHOOSER (view);

  g_assert (self->date != NULL);
  return g_date_time_add_months (self->date, -1);
}

static void
gcal_view_interface_init (GcalViewInterface *iface)
{
  iface->get_date = gcal_date_chooser_get_date;
  iface->set_date = gcal_date_chooser_set_date;
  iface->get_children_by_uuid = gcal_date_chooser_get_children_by_uuid;
  iface->get_next_date = gcal_date_chooser_get_next_date;
  iface->get_previous_date = gcal_date_chooser_get_previous_date;
}


/*
 * GcalTimelineSubscriber implementation
 */

static GcalRange*
gcal_date_chooser_get_range (GcalTimelineSubscriber *subscriber)
{
  GcalDateChooser *self;
  GDateTime *start;
  g_autoptr (GDateTime) end = NULL;

  self = GCAL_DATE_CHOOSER (subscriber);
  start = gcal_date_chooser_day_get_date (GCAL_DATE_CHOOSER_DAY (self->days[0][0]));
  end = g_date_time_add_days (start, G_N_ELEMENTS (self->days) * G_N_ELEMENTS (self->days[0]));

  return gcal_range_new (start, end, GCAL_RANGE_DEFAULT);
}

static void
gcal_date_chooser_add_event (GcalTimelineSubscriber *subscriber,
                             GcalEvent              *event)
{
  GcalDateChooser *self;

  self = GCAL_DATE_CHOOSER (subscriber);

  g_debug ("Caching event '%s' in %s", gcal_event_get_uid (event), G_OBJECT_TYPE_NAME (self));
  gcal_range_tree_add_range (self->events, gcal_event_get_range (event), g_object_ref (event));

  queue_update_event_indicators (self);
}

static void
gcal_date_chooser_remove_event (GcalTimelineSubscriber *subscriber,
                                GcalEvent              *event)
{
  GcalDateChooser *self;

  self = GCAL_DATE_CHOOSER (subscriber);

  g_debug ("Removing event '%s' from %s's cache", gcal_event_get_uid (event), G_OBJECT_TYPE_NAME (self));
  gcal_range_tree_remove_range (self->events, gcal_event_get_range (event), event);

  queue_update_event_indicators (self);
}

static void
gcal_date_chooser_update_event (GcalTimelineSubscriber *subscriber,
                                GcalEvent              *old_event,
                                GcalEvent              *event)
{
  gcal_date_chooser_remove_event (subscriber, old_event);
  gcal_date_chooser_add_event (subscriber, event);
}

static void
gcal_timeline_subscriber_interface_init (GcalTimelineSubscriberInterface *iface)
{
  iface->get_range = gcal_date_chooser_get_range;
  iface->add_event = gcal_date_chooser_add_event;
  iface->remove_event = gcal_date_chooser_remove_event;
  iface->update_event = gcal_date_chooser_update_event;
}

static void
calendar_set_property (GObject      *obj,
                       guint         property_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
  GcalDateChooser *self = GCAL_DATE_CHOOSER (obj);

  switch (property_id)
    {
    case PROP_DATE:
      gcal_date_chooser_set_date (GCAL_VIEW (self), g_value_get_boxed (value));
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

    case PROP_SHOW_HEADING:
      gcal_date_chooser_set_show_heading (self, g_value_get_boolean (value));
      break;

    case PROP_SHOW_DAY_NAMES:
      gcal_date_chooser_set_show_day_names (self, g_value_get_boolean (value));
      break;

    case PROP_SHOW_WEEK_NUMBERS:
      gcal_date_chooser_set_show_week_numbers (self, g_value_get_boolean (value));
      break;

    case PROP_SHOW_SELECTED_WEEK:
      gcal_date_chooser_set_show_selected_week (self, g_value_get_boolean (value));
      break;

    case PROP_SHOW_EVENTS:
      gcal_date_chooser_set_show_events (self, g_value_get_boolean (value));
      break;

    case PROP_SPLIT_MONTH_YEAR:
      gcal_date_chooser_set_split_month_year (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, property_id, pspec);
      break;
    }
}

static void
calendar_get_property (GObject    *obj,
                       guint       property_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
  GcalDateChooser *self = GCAL_DATE_CHOOSER (obj);

  switch (property_id)
    {
    case PROP_DATE:
      g_value_set_boxed (value, self->date);
      break;

    case PROP_CONTEXT:
      g_value_set_object (value, self->context);
      break;

    case PROP_SHOW_HEADING:
      g_value_set_boolean (value, self->show_heading);
      break;

    case PROP_SHOW_DAY_NAMES:
      g_value_set_boolean (value, self->show_day_names);
      break;

    case PROP_SHOW_WEEK_NUMBERS:
      g_value_set_boolean (value, self->show_week_numbers);
      break;

    case PROP_SHOW_SELECTED_WEEK:
      g_value_set_boolean (value, self->show_selected_week);
      break;

    case PROP_SHOW_EVENTS:
      g_value_set_boolean (value, self->show_events);
      break;

    case PROP_SPLIT_MONTH_YEAR:
      g_value_set_boolean (value, self->split_month_year);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, property_id, pspec);
      break;
    }
}

static void
multi_choice_changed (GcalDateChooser *self,
                      gint             year,
                      gint             month)
{
  g_autoptr (GDateTime) date = NULL;
  gint day;

  day = g_date_time_get_day_of_month (self->date);

  /* Make sure the day is valid at that month */
  day = MIN (day, month_length[leap (year)][month]);

  date = g_date_time_new_local (year, month, day, 1, 1, 1);
  gcal_date_chooser_set_date (GCAL_VIEW (self), date);
}

static void
combined_multi_choice_changed (GcalDateChooser *self)
{
  gint year, month;

  year = gcal_multi_choice_get_value (GCAL_MULTI_CHOICE (self->combined_choice)) / 12;
  month = gcal_multi_choice_get_value (GCAL_MULTI_CHOICE (self->combined_choice)) % 12 + 1;

  multi_choice_changed (self, year, month);
}

static void
split_multi_choice_changed (GcalDateChooser *self)
{
  gint year, month;

  year = gcal_multi_choice_get_value (GCAL_MULTI_CHOICE (self->year_choice));
  month = gcal_multi_choice_get_value (GCAL_MULTI_CHOICE (self->month_choice)) + 1;

  multi_choice_changed (self, year, month);
}

static gboolean
on_drop_target_drop_cb (GtkDropTarget   *target,
                        const GValue    *value,
                        double           x,
                        double           y,
                        GcalDateChooser *self)
{
  g_autoptr (GDateTime) date = NULL;
  g_autoptr (GDate) gdate = NULL;
  const gchar *text;
  gint year, month, day;

  gdate = g_date_new ();
  text = g_value_get_string (value);

  if (text)
    g_date_set_parse (gdate, text);

  if (!g_date_valid (gdate))
    return FALSE;

  year = g_date_get_year (gdate);
  month = g_date_get_month (gdate);
  day = g_date_get_day (gdate);

  if (!self->show_heading)
    g_date_time_get_ymd (self->date, &year, &month, NULL);

  date = g_date_time_new_local (year, month, day, 1, 1, 1);
  gcal_date_chooser_set_date (GCAL_VIEW (self), date);

  return TRUE;
}

static void
gcal_date_chooser_finalize (GObject *object)
{
  GcalDateChooser *self = GCAL_DATE_CHOOSER (object);

  g_clear_object (&self->context);
  g_clear_pointer (&self->date, g_date_time_unref);
  g_clear_pointer (&self->events, gcal_range_tree_unref);

  G_OBJECT_CLASS (gcal_date_chooser_parent_class)->finalize (object);
}

static void
gcal_date_chooser_dispose (GObject *object)
{
  G_OBJECT_CLASS (gcal_date_chooser_parent_class)->dispose (object);
}

static void
gcal_date_chooser_class_init (GcalDateChooserClass *class)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  g_type_ensure (GCAL_TYPE_MULTI_CHOICE);

  object_class->dispose = gcal_date_chooser_dispose;
  object_class->finalize = gcal_date_chooser_finalize;
  object_class->set_property = calendar_set_property;
  object_class->get_property = calendar_get_property;

  properties[PROP_SHOW_HEADING] = g_param_spec_boolean ("show-heading",
                                                        "Show Heading",
                                                        "If TRUE, a heading is displayed",
                                                        TRUE,
                                                        G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_SHOW_DAY_NAMES] = g_param_spec_boolean ("show-day-names",
                                                          "Show Day Names",
                                                          "If TRUE, day names are displayed",
                                                          TRUE,
                                                          G_PARAM_READWRITE);

  properties[PROP_SHOW_WEEK_NUMBERS] = g_param_spec_boolean ("show-week-numbers",
                                                             "Show Week Numbers",
                                                             "If TRUE, week numbers are displayed",
                                                             TRUE,
                                                             G_PARAM_READWRITE);

  /**
   * GcalDateChooser:show-selected-week:
   *
   * Whether the selected week is highlighted or not.
   */
  properties[PROP_SHOW_SELECTED_WEEK] = g_param_spec_boolean ("show-selected-week",
                                                              "Show Selected Week",
                                                              "If TRUE, week numbers are displayed",
                                                              TRUE,
                                                              G_PARAM_READWRITE);

  properties[PROP_SHOW_EVENTS] = g_param_spec_boolean ("show-events",
                                                       "Show Events",
                                                       "If TRUE, events are displayed",
                                                       TRUE,
                                                       G_PARAM_READWRITE);

  properties[PROP_SPLIT_MONTH_YEAR] = g_param_spec_boolean ("split-month-year",
                                                            "Split Month Year",
                                                            "If TRUE, the month and year selectors are split",
                                                            TRUE,
                                                            G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);

  g_object_class_override_property (object_class, PROP_DATE, "active-date");
  g_object_class_override_property (object_class, PROP_CONTEXT, "context");

  signals[DAY_SELECTED] = g_signal_new ("day-selected",
                                        G_OBJECT_CLASS_TYPE (object_class),
                                        G_SIGNAL_RUN_FIRST,
                                        0,
                                        NULL, NULL,
                                        NULL,
                                        G_TYPE_NONE, 0);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/event-editor/gcal-date-chooser.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalDateChooser, combined_choice);
  gtk_widget_class_bind_template_child (widget_class, GcalDateChooser, month_choice);
  gtk_widget_class_bind_template_child (widget_class, GcalDateChooser, popover_month_choice);
  gtk_widget_class_bind_template_child (widget_class, GcalDateChooser, year_choice);
  gtk_widget_class_bind_template_child (widget_class, GcalDateChooser, grid);

  gtk_widget_class_bind_template_callback (widget_class, combined_multi_choice_changed);
  gtk_widget_class_bind_template_callback (widget_class, get_combined_choice_visible);
  gtk_widget_class_bind_template_callback (widget_class, get_split_choice_visible);
  gtk_widget_class_bind_template_callback (widget_class, split_multi_choice_changed);

  gtk_widget_class_set_css_name (widget_class, "datechooser");
}


static gboolean
show_week_number_to_column_cb (GBinding     *binding,
                               const GValue *from_value,
                               GValue       *to_value,
                               gpointer      user_data)
{
  g_value_set_int (to_value, g_value_get_boolean (from_value) ? -1 : 0);

  return TRUE;
}


static gboolean
show_week_number_to_column_span_cb (GBinding     *binding,
                                    const GValue *from_value,
                                    GValue       *to_value,
                                    gpointer      user_data)
{
  g_value_set_int (to_value, g_value_get_boolean (from_value) ? COLS + 1 : COLS);

  return TRUE;
}


static void
gcal_date_chooser_init (GcalDateChooser *self)
{
  GtkDropTarget *drop_target;
  GtkWidget *label;
  gint row, col;
  gint year, month;
  GtkLayoutManager *layout_manager;

  self->show_heading = TRUE;
  self->show_day_names = TRUE;
  self->show_week_numbers = TRUE;
  self->show_selected_week = TRUE;
  self->show_events = TRUE;
  self->split_month_year = TRUE;

  self->date = g_date_time_new_now_local ();
  g_date_time_get_ymd (self->date, &self->this_year, NULL, NULL);

  self->week_start = get_first_weekday ();

  self->events = gcal_range_tree_new_with_free_func (g_object_unref);

  gtk_widget_init_template (GTK_WIDGET (self));

  for (col = 0; col < COLS; col++)
    {
      self->cols[col] = gtk_label_new ("");

      g_object_bind_property (self,
                              "show-day-names",
                              self->cols[col],
                              "visible",
                              G_BINDING_SYNC_CREATE);

      gtk_widget_add_css_class (self->cols[col], "weekday");

      gtk_grid_attach (GTK_GRID (self->grid), self->cols[col], col, -1, 1, 1);
    }

  layout_manager = gtk_widget_get_layout_manager (self->grid);

  for (row = 0; row < ROWS; row++)
    {
      GtkLayoutChild *layout_child;

      self->rows[row] = gtk_label_new ("");

      g_object_bind_property (self,
                              "show-week-numbers",
                              self->rows[row],
                              "visible",
                              G_BINDING_SYNC_CREATE);

      gtk_widget_add_css_class (self->rows[row], "weeknum");
      gtk_grid_attach (GTK_GRID (self->grid), self->rows[row], -1, row, 1, 1);

      self->week[row] = adw_bin_new ();
      gtk_widget_set_visible (self->week[row], FALSE);
      gtk_widget_add_css_class (self->week[row], "current-week");
      gtk_grid_attach (GTK_GRID (self->grid), self->week[row], -1, row, 8, 1);

      layout_child = gtk_layout_manager_get_layout_child (layout_manager, self->week[row]);

      g_object_bind_property_full (self,
                                   "show-week-numbers",
                                   layout_child,
                                   "column",
                                   G_BINDING_SYNC_CREATE,
                                   show_week_number_to_column_cb,
                                   NULL,
                                   NULL,
                                   NULL);

      g_object_bind_property_full (self,
                                   "show-week-numbers",
                                   layout_child,
                                   "column-span",
                                   G_BINDING_SYNC_CREATE,
                                   show_week_number_to_column_span_cb,
                                   NULL,
                                   NULL,
                                   NULL);
    }

  /* We are using a stack here to keep the week number column from shrinking
   * when all the weeks are single-digit
   */
  self->corner = gtk_stack_new ();
  gtk_grid_attach (GTK_GRID (self->grid), self->corner, -1, -1, 1, 1);

  label = gtk_label_new ("");
  gtk_widget_add_css_class (label, "weekday");
  gtk_stack_add_named (GTK_STACK (self->corner), label, "weekday");

  label = gtk_label_new ("99");
  gtk_widget_add_css_class (label, "weeknum");
  gtk_stack_add_named (GTK_STACK (self->corner), label, "weeknum");

  self->day_grid = g_object_new (GTK_TYPE_GRID,
                                 "valign", GTK_ALIGN_FILL,
                                 "halign", GTK_ALIGN_FILL,
                                 "row-spacing", 2,
                                 "column-spacing", 2,
                                 "visible", TRUE,
                                 NULL);
  gtk_grid_attach (GTK_GRID (self->grid), self->day_grid, 0, 0, COLS, ROWS);

  for (row = 0; row < ROWS; row++)
    {
      for (col = 0; col < COLS; col++)
        {
          self->days[row][col] = gcal_date_chooser_day_new ();

          g_object_bind_property (self,
                                  "show-events",
                                  self->days[row][col],
                                  "has-dot",
                                  G_BINDING_SYNC_CREATE);

          g_signal_connect_object (self->days[row][col],
                                   "clicked",
                                   G_CALLBACK (day_selected_cb),
                                   self,
                                   0);

          gtk_grid_attach (GTK_GRID (self->day_grid), self->days[row][col], col, row, 1, 1);
        }
    }

  calendar_init_month_display (self);
  calendar_init_weekday_display (self);

  calendar_compute_days (self);
  g_date_time_get_ymd (self->date, &year, &month, NULL);
  gcal_multi_choice_set_value (GCAL_MULTI_CHOICE (self->year_choice), year);
  gcal_multi_choice_set_value (GCAL_MULTI_CHOICE (self->month_choice), month - 1);
  gcal_multi_choice_set_value (GCAL_MULTI_CHOICE (self->combined_choice), year * 12 + month - 1);
  calendar_update_selected_day_display (self);

  drop_target = gtk_drop_target_new (G_TYPE_STRING, GDK_ACTION_COPY);
  gtk_drop_target_set_preload (drop_target, TRUE);
  g_signal_connect (drop_target, "drop", G_CALLBACK (on_drop_target_drop_cb), NULL);
  gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (drop_target));
}

GtkWidget*
gcal_date_chooser_new (void)
{
  return g_object_new (GCAL_TYPE_DATE_CHOOSER, NULL);
}

void
gcal_date_chooser_set_show_heading (GcalDateChooser *self,
                                    gboolean         setting)
{
  if (self->show_heading == setting)
    return;

  self->show_heading = setting;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SHOW_HEADING]);
}

gboolean
gcal_date_chooser_get_show_heading (GcalDateChooser *self)
{
  return self->show_heading;
}

void
gcal_date_chooser_set_show_day_names (GcalDateChooser *self,
                                      gboolean         setting)
{
  if (self->show_day_names == setting)
    return;

  self->show_day_names = setting;

  gtk_widget_set_visible (self->corner, self->show_day_names && self->show_week_numbers);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SHOW_DAY_NAMES]);
}

gboolean
gcal_date_chooser_get_show_day_names (GcalDateChooser *self)
{
  return self->show_day_names;
}

void
gcal_date_chooser_set_show_week_numbers (GcalDateChooser *self,
                                         gboolean         setting)
{
  if (self->show_week_numbers == setting)
    return;

  self->show_week_numbers = setting;

  gtk_widget_set_visible (self->corner, self->show_day_names && self->show_week_numbers);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SHOW_WEEK_NUMBERS]);
}

gboolean
gcal_date_chooser_get_show_week_numbers (GcalDateChooser *self)
{
  return self->show_week_numbers;
}

/**
 * gcal_date_chooser_set_show_selected_week:
 * @self: a #GcalDateChooser
 * @setting: whether the selected week is highlighted or not.
 *
 * Sets the selected week is highlighted or not.
 */
void
gcal_date_chooser_set_show_selected_week (GcalDateChooser *self,
                                          gboolean         setting)
{
  if (self->show_selected_week == setting)
    return;

  self->show_selected_week = setting;

  calendar_update_selected_day_display (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SHOW_SELECTED_WEEK]);
}

/**
 * gcal_date_chooser_get_show_selected_week:
 * @self: a #GcalDateChooser
 *
 * Returns: whether the selected week is highlighted or not.
 */
gboolean
gcal_date_chooser_get_show_selected_week (GcalDateChooser *self)
{
  return self->show_selected_week;
}

void
gcal_date_chooser_set_show_events (GcalDateChooser *self,
                                   gboolean         setting)
{
  if (self->show_events == setting)
    return;

  self->show_events = setting;

  calendar_update_selected_day_display (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SHOW_EVENTS]);
}

gboolean
gcal_date_chooser_get_show_events (GcalDateChooser *self)
{
  return self->show_events;
}

void
gcal_date_chooser_set_split_month_year (GcalDateChooser *self,
                                        gboolean         setting)
{
  if (self->split_month_year == setting)
    return;

  self->split_month_year = setting;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SPLIT_MONTH_YEAR]);
}

gboolean
gcal_date_chooser_get_split_month_year (GcalDateChooser *self)
{
  return self->split_month_year;
}
