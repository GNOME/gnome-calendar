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

#include <stdlib.h>
#include <langinfo.h>

struct _GcalDateChooser
{
  GtkBin              parent;

  GtkWidget          *month_choice;
  GtkWidget          *year_choice;
  GtkWidget          *grid;

  GtkWidget          *day_grid;
  GtkWidget          *corner;
  GtkWidget          *cols[7];
  GtkWidget          *rows[6];
  GtkWidget          *days[6][7];

  GDateTime          *date;

  gint                this_year;
  gint                week_start;

  gboolean            show_heading;
  gboolean            show_day_names;
  gboolean            show_week_numbers;
  gboolean            no_month_change;

  GcalDateChooserDayOptionsCallback day_options_cb;
  gpointer            day_options_data;
  GDestroyNotify      day_options_destroy;
};

G_DEFINE_TYPE (GcalDateChooser, gcal_date_chooser, GTK_TYPE_BIN)

enum
{
  MONTH_CHANGED,
  DAY_SELECTED,
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_DATE,
  PROP_SHOW_HEADING,
  PROP_SHOW_DAY_NAMES,
  PROP_SHOW_WEEK_NUMBERS,
  PROP_NO_MONTH_CHANGE,
  NUM_PROPERTIES
};

static guint signals[LAST_SIGNAL] = { 0, };
static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

static const guint month_length[2][13] =
{
  { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
  { 0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
};

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

  first_day = (first_day + 7 - self->week_start) % 7;
  if (first_day == 0)
    first_day = 7;

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
  row = first_day / 7;
  col = first_day % 7;

  for (day = 1; day <= ndays_in_month; day++)
    {
      d = GCAL_DATE_CHOOSER_DAY (self->days[row][col]);
      date = g_date_time_new_local (year, month, day, 1, 1, 1);
      gcal_date_chooser_day_set_date (d, date);
      gcal_date_chooser_day_set_other_month (d, FALSE);
      g_date_time_unref (date);

      col++;
      if (col == 7)
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
  for (; row < 6; row++)
    {
      for (; col < 7; col++)
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
  for (row = 0; row < 6; row++)
    {
      gchar *text;

      d = GCAL_DATE_CHOOSER_DAY (self->days[row][6]);
      date = gcal_date_chooser_day_get_date (d);
      text = g_strdup_printf ("%d", g_date_time_get_week_of_year (date));
      gtk_label_set_label (GTK_LABEL (self->rows[row]), text);
      g_free (text);
    }

  gcal_date_chooser_invalidate_day_options (self);
}

/* 0 == sunday */
static gchar *
calendar_get_weekday_name (gint i)
{
  GDateTime *date;
  gchar *formatted_date;
  gchar *upcased_date;
  gchar *text;

  date = g_date_time_new_local (2015, 1, 4 + i, 1, 1, 1);
  formatted_date = g_date_time_format (date, "%a");
  g_date_time_unref (date);

  upcased_date = g_utf8_strup (formatted_date, -1);
  g_free (formatted_date);

  text = g_utf8_substring (upcased_date, 0, 1);
  g_free (upcased_date);

  return text;
}

/* 0 = january */
static gchar *
calendar_get_month_name (gint i)
{
  GDateTime *date;
  gchar *text;

  date = g_date_time_new_local (2015, i + 1, 1, 1, 1, 1);
  text = g_date_time_format (date, "%B");
  g_date_time_unref (date);

  return text;
}

static void
calendar_init_weekday_display (GcalDateChooser *self)
{
  gint i;
  gchar *text;

  for (i = 0; i < 7; i++)
    {
      text = calendar_get_weekday_name ((i + self->week_start) % 7);
      gtk_label_set_label (GTK_LABEL (self->cols[i]), text);
      g_free (text);
    }
}

static gchar *
format_month (GcalMultiChoice *choice,
              gint             value,
              gpointer         data)
{
  return calendar_get_month_name (value);
}

static void
calendar_init_month_display (GcalDateChooser *self)
{
  gchar *months[13];
  gchar *month;
  gint i;

  for (i = 0; i < 12; i++)
    {
      month = calendar_get_month_name (i);
      months[i] = g_strdup_printf ("%s", month);
      g_free (month);
    }

  months[12] = NULL;

  gcal_multi_choice_set_choices (GCAL_MULTI_CHOICE (self->month_choice),
                                (const gchar**) months);

  for (i = 0; i < 12; i++)
    g_free (months[i]);

  gcal_multi_choice_set_format_callback (GCAL_MULTI_CHOICE (self->month_choice),
                                         format_month,
                                         self,
                                         NULL);
}

static void
calendar_update_selected_day_display (GcalDateChooser *self)
{
  GcalDateChooserDay *d;
  GDateTime *date;
  gint row, col;

  for (row = 0; row < 6; row++)
    {
      for (col = 0; col < 7; col++)
      {
        d = GCAL_DATE_CHOOSER_DAY (self->days[row][col]);
        date = gcal_date_chooser_day_get_date (d);
        gcal_date_chooser_day_set_selected (d, datetime_compare_date (date, self->date) == 0);
      }
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
      gcal_date_chooser_set_date (self, date);
      g_date_time_unref (date);
    }
  else
    {
      calendar_update_selected_day_display (self);
    }
}

static void
day_selected_cb (GcalDateChooserDay *d,
                 GcalDateChooser    *self)
{
  gcal_date_chooser_set_date (self, gcal_date_chooser_day_get_date (d));
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
      gcal_date_chooser_set_date (self, g_value_get_boxed (value));
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

    case PROP_NO_MONTH_CHANGE:
      gcal_date_chooser_set_no_month_change (self, g_value_get_boolean (value));
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

    case PROP_SHOW_HEADING:
      g_value_set_boolean (value, self->show_heading);
      break;

    case PROP_SHOW_DAY_NAMES:
      g_value_set_boolean (value, self->show_day_names);
      break;

    case PROP_SHOW_WEEK_NUMBERS:
      g_value_set_boolean (value, self->show_week_numbers);
      break;

    case PROP_NO_MONTH_CHANGE:
      g_value_set_boolean (value, self->no_month_change);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, property_id, pspec);
      break;
    }
}

static void
multi_choice_changed (GcalDateChooser *self)
{
  GDateTime *date;
  gint year, month, day, month_days;

  year = gcal_multi_choice_get_value (GCAL_MULTI_CHOICE (self->year_choice));
  month = gcal_multi_choice_get_value (GCAL_MULTI_CHOICE (self->month_choice)) + 1;
  month_days = month_length [0][month];
  day = 1;

  if (leap (year))
    month_days = month_length [1][month];

  if (self->date)
    g_date_time_get_ymd (self->date, NULL, NULL, &day);

  if (day > month_days)
    day = month_days;

  date = g_date_time_new_local (year, month, day, 1, 1, 1);
  gcal_date_chooser_set_date (self, date);
  g_date_time_unref (date);
}

static void
calendar_drag_data_received (GtkWidget        *widget,
                             GdkDragContext   *context,
                             gint              x,
                             gint              y,
                             GtkSelectionData *selection_data,
                             guint             info,
                             guint             time)
{
  GcalDateChooser *self = GCAL_DATE_CHOOSER (widget);
  gchar *text;
  GDate *gdate;
  gint year, month, day;
  GDateTime *date;

  gdate = g_date_new ();
  text = (gchar *)gtk_selection_data_get_text (selection_data);
  if (text)
    {
      g_date_set_parse (gdate, text);
      g_free (text);
    }
  if (!g_date_valid (gdate))
    {
      g_date_free (gdate);
      gtk_drag_finish (context, FALSE, FALSE, time);
      return;
    }

  year = g_date_get_year (gdate);
  month = g_date_get_month (gdate);
  day = g_date_get_day (gdate);

  g_date_free (gdate);

  gtk_drag_finish (context, TRUE, FALSE, time);

  if (!self->show_heading || self->no_month_change)
    g_date_time_get_ymd (self->date, &year, &month, NULL);

  date = g_date_time_new_local (year, month, day, 1, 1, 1);
  gcal_date_chooser_set_date (self, date);
  g_date_time_unref (date);
}

static void
gcal_date_chooser_finalize (GObject *object)
{
  GcalDateChooser *self = GCAL_DATE_CHOOSER (object);

  g_clear_pointer (&self->date, g_date_time_unref);
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

  widget_class->drag_data_received = calendar_drag_data_received;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/date-chooser.ui");

  properties[PROP_DATE] = g_param_spec_boxed ("date",
                                              "Date",
                                              "The selected date",
                                              G_TYPE_DATE_TIME,
                                              G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

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

  properties[PROP_NO_MONTH_CHANGE] = g_param_spec_boolean ("no-month-change",
                                                           "No Month Change",
                                                           "If TRUE, the selected month cannot be changed",
                                                           FALSE,
                                                           G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);

  signals[MONTH_CHANGED] = g_signal_new ("month-changed",
                                         G_OBJECT_CLASS_TYPE (object_class),
                                         G_SIGNAL_RUN_FIRST,
                                         0,
                                         NULL, NULL,
                                         NULL,
                                         G_TYPE_NONE, 0);

  signals[DAY_SELECTED] = g_signal_new ("day-selected",
                                        G_OBJECT_CLASS_TYPE (object_class),
                                        G_SIGNAL_RUN_FIRST,
                                        0,
                                        NULL, NULL,
                                        NULL,
                                        G_TYPE_NONE, 0);

  gtk_widget_class_bind_template_child (widget_class, GcalDateChooser, month_choice);
  gtk_widget_class_bind_template_child (widget_class, GcalDateChooser, year_choice);
  gtk_widget_class_bind_template_child (widget_class, GcalDateChooser, grid);

  gtk_widget_class_bind_template_callback (widget_class, multi_choice_changed);

  gtk_widget_class_set_css_name (widget_class, "datechooser");
}


static void
gcal_date_chooser_init (GcalDateChooser *self)
{
  GtkWidget *label;
  gint row, col;
  gint year, month;

  self->show_heading = TRUE;
  self->show_day_names = TRUE;
  self->show_week_numbers = TRUE;
  self->no_month_change = FALSE;

  self->date = g_date_time_new_now_local ();
  g_date_time_get_ymd (self->date, &self->this_year, NULL, NULL);

  self->week_start = get_first_weekday ();

  gtk_widget_init_template (GTK_WIDGET (self));

  for (col = 0; col < 7; col++)
    {
      self->cols[col] = gtk_label_new ("");

      g_object_bind_property (self,
                              "show-day-names",
                              self->cols[col],
                              "visible",
                              G_BINDING_SYNC_CREATE);

      gtk_style_context_add_class (gtk_widget_get_style_context (self->cols[col]), "weekday");

      gtk_grid_attach (GTK_GRID (self->grid), self->cols[col], col, -1, 1, 1);
    }

  for (row = 0; row < 6; row++)
    {
      self->rows[row] = gtk_label_new ("");

      g_object_bind_property (self,
                              "show-week-numbers",
                              self->rows[row],
                              "visible",
                              G_BINDING_SYNC_CREATE);

      gtk_widget_show (self->rows[row]);
      gtk_style_context_add_class (gtk_widget_get_style_context (self->rows[row]), "weeknum");
      gtk_grid_attach (GTK_GRID (self->grid), self->rows[row], -1, row, 1, 1);
    }

  /* We are using a stack here to keep the week number column from shrinking
   * when all the weeks are single-digit
   */
  self->corner = gtk_stack_new ();
  gtk_grid_attach (GTK_GRID (self->grid), self->corner, -1, -1, 1, 1);
  label = gtk_label_new ("");
  gtk_widget_show (label);
  gtk_style_context_add_class (gtk_widget_get_style_context (label), "weekday");
  gtk_container_add (GTK_CONTAINER (self->corner), label);

  label = gtk_label_new ("99");
  gtk_style_context_add_class (gtk_widget_get_style_context (label), "weeknum");
  gtk_container_add (GTK_CONTAINER (self->corner), label);

  self->day_grid = g_object_new (GTK_TYPE_GRID,
                                 "valign", GTK_ALIGN_FILL,
                                 "halign", GTK_ALIGN_FILL,
                                 "row-spacing", 2,
                                 "column-spacing", 2,
                                 "visible", TRUE,
                                 NULL);
  gtk_grid_attach (GTK_GRID (self->grid), self->day_grid, 0, 0, 7, 6);

  for (row = 0; row < 6; row++)
    {
      for (col = 0; col < 7; col++)
        {
          self->days[row][col] = gcal_date_chooser_day_new ();

          g_signal_connect (self->days[row][col],
                            "selected",
                            G_CALLBACK (day_selected_cb),
                            self);

          gtk_widget_show (self->days[row][col]);
          gtk_grid_attach (GTK_GRID (self->day_grid), self->days[row][col], col, row, 1, 1);
        }
    }

  calendar_init_month_display (self);
  calendar_init_weekday_display (self);

  calendar_compute_days (self);
  g_date_time_get_ymd (self->date, &year, &month, NULL);
  gcal_multi_choice_set_value (GCAL_MULTI_CHOICE (self->year_choice), year);
  gcal_multi_choice_set_value (GCAL_MULTI_CHOICE (self->month_choice), month - 1);
  calendar_update_selected_day_display (self);

  gtk_drag_dest_set (GTK_WIDGET (self), GTK_DEST_DEFAULT_ALL, NULL, 0, GDK_ACTION_COPY);
  gtk_drag_dest_add_text_targets (GTK_WIDGET (self));
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

void
gcal_date_chooser_set_no_month_change (GcalDateChooser *self,
                                       gboolean         setting)
{
  if (self->no_month_change == setting)
    return;

  self->no_month_change = setting;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_NO_MONTH_CHANGE]);
}

gboolean
gcal_date_chooser_get_no_month_change (GcalDateChooser *self)
{
  return self->no_month_change;
}

void
gcal_date_chooser_set_date (GcalDateChooser *self,
                            GDateTime       *date)
{
  gint y1, m1, d1, y2, m2, d2;

  g_object_freeze_notify (G_OBJECT (self));

  g_date_time_get_ymd (self->date, &y1, &m1, &d1);
  g_date_time_get_ymd (date, &y2, &m2, &d2);

  g_date_time_unref (self->date);
  self->date = g_date_time_ref (date);

  if (y1 != y2 || m1 != m2)
    {
      gcal_multi_choice_set_value (GCAL_MULTI_CHOICE (self->year_choice), y2);
      gcal_multi_choice_set_value (GCAL_MULTI_CHOICE (self->month_choice), m2 - 1);
      calendar_compute_days (self);
    }

  if (y1 != y2 || m1 != m2 || d1 != d2)
    {
      calendar_update_selected_day (self);
      g_signal_emit (self, signals[DAY_SELECTED], 0);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DATE]);
    }

  g_object_thaw_notify (G_OBJECT (self));
}

GDateTime *
gcal_date_chooser_get_date (GcalDateChooser *self)
{
  return self->date;
}

void
gcal_date_chooser_set_day_options_callback (GcalDateChooser                   *self,
                                            GcalDateChooserDayOptionsCallback  callback,
                                            gpointer                           data,
                                            GDestroyNotify                     destroy)
{
  if (self->day_options_destroy)
    self->day_options_destroy (self->day_options_data);

  self->day_options_cb = callback;
  self->day_options_data = data;
  self->day_options_destroy = destroy;

  gcal_date_chooser_invalidate_day_options (self);
}


void
gcal_date_chooser_invalidate_day_options (GcalDateChooser *self)
{
  GcalDateChooserDayOptions options;
  GcalDateChooserDay *d;
  GDateTime *date;
  gint row, col;

  for (row = 0; row < 6; row++)
    {
      for (col = 0; col < 7; col++)
        {
          d = GCAL_DATE_CHOOSER_DAY (self->days[row][col]);
          date = gcal_date_chooser_day_get_date (d);

          if (self->day_options_cb)
            options = self->day_options_cb (self, date, self->day_options_data);
          else
            options = GCAL_DATE_CHOOSER_DAY_NONE;

          gcal_date_chooser_day_set_options (d, options);
        }
    }
}
