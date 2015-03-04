/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gcal-date-selector.c
 * Copyright (C) 2014 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *
 * gnome-calendar is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * gnome-calendar is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "gcal-date-selector.h"

#include <locale.h>
#include <langinfo.h>
#include <stdlib.h>
#include <glib/gi18n.h>

enum
{
  DAY,
  MONTH,
  YEAR,
  NUM_ENTRIES
};

struct _GcalDateSelectorPrivate
{
  /* widgets */
  GtkWidget   *date_label;
  GtkWidget   *calendar;
  GtkWidget   *grid;

  GtkWidget   *entries[NUM_ENTRIES];

  /* date */
  gint         day;
  gint         month;
  gint         year;

  /* misc */
  gchar       *mask;

  /* index in the mask starting by 0 */
  guint        day_pos;
  guint        month_pos;
  guint        year_pos;
};

enum
{
  MODIFIED,
  NUM_SIGNALS
};

static guint signals[NUM_SIGNALS] = { 0, };

static void     calendar_day_selected                             (GtkCalendar          *calendar,
                                                                   gpointer              user_data);

static gboolean date_entry_focus_out                              (GtkWidget            *widget,
                                                                   GdkEvent             *event,
                                                                   gpointer              user_data);

static void     entry_activated                                   (GtkEntry             *entry,
                                                                   gpointer              user_data);

static void     parse_entries                                     (GcalDateSelector     *selector);

static void     text_inserted                                     (GtkEditable          *editable,
                                                                   gchar                *new_text,
                                                                   gint                  new_text_length,
                                                                   gint                 *position,
                                                                   gpointer              user_data);

static void     gcal_date_selector_constructed                    (GObject              *object);

G_DEFINE_TYPE_WITH_PRIVATE (GcalDateSelector, gcal_date_selector, GTK_TYPE_MENU_BUTTON);

static void
calendar_day_selected (GtkCalendar *calendar,
                       gpointer     user_data)
{
  GcalDateSelectorPrivate *priv;
  guint day, month, year;

  priv = gcal_date_selector_get_instance_private (GCAL_DATE_SELECTOR (user_data));
  gtk_calendar_get_date (calendar, &year, &month, &day);

  /**
   * Block signal handler to avoid an infinite
   * recursion, exploding the proccess stack.
   */
  g_signal_handlers_block_by_func (priv->calendar, calendar_day_selected, user_data);

  gcal_date_selector_set_date (GCAL_DATE_SELECTOR (user_data), day, month + 1, year);

  g_signal_handlers_unblock_by_func (priv->calendar, calendar_day_selected, user_data);
}

static gboolean
date_entry_focus_out (GtkWidget *widget,
                      GdkEvent  *event,
                      gpointer   user_data)
{
  parse_entries (GCAL_DATE_SELECTOR (user_data));
  return FALSE;
}

static void
entry_activated (GtkEntry *entry,
                 gpointer  user_data)
{
  parse_entries (GCAL_DATE_SELECTOR (user_data));
}

static void
parse_entries (GcalDateSelector *selector)
{
  GcalDateSelectorPrivate *priv;
  gint day, month, year;

  priv = gcal_date_selector_get_instance_private (selector);

  day = atoi (gtk_entry_get_text (GTK_ENTRY (priv->entries[DAY])));
  month = atoi (gtk_entry_get_text (GTK_ENTRY (priv->entries[MONTH])));
  year = atoi (gtk_entry_get_text (GTK_ENTRY (priv->entries[YEAR])));

  /* select the date */
  g_signal_handlers_block_by_func (priv->calendar, calendar_day_selected, selector);

  gcal_date_selector_set_date (selector, day, month, year);

  g_signal_handlers_unblock_by_func (priv->calendar, calendar_day_selected, selector);
}

static void
text_inserted (GtkEditable *editable,
               gchar       *new_text,
               gint         new_text_length,
               gint        *position,
               gpointer     user_data)
{
  GtkEntryBuffer *buffer;
  gchar* new_label;
  gint i;
  gint current_length, max_length;
  gboolean valid;

  valid = TRUE;
  buffer = gtk_entry_get_buffer (GTK_ENTRY (editable));
  current_length = gtk_entry_buffer_get_length (buffer);
  max_length = gtk_entry_get_max_length (GTK_ENTRY (editable));

  /* honor max length property */
  if (current_length + new_text_length > max_length)
    return;

  /* stop the default implementation */
  g_signal_stop_emission (editable, g_signal_lookup ("insert-text", GTK_TYPE_ENTRY), 0);

  for (i = 0; i < new_text_length; i++)
    {
      gchar c;
      c = *(new_text + i);

      /* trying to insert a non-numeric char */
      if (c < '0' || c > '9')
        {
          valid = FALSE;
          break;
        }
    }

  if (!valid)
    return;

  new_label = g_strdup_printf ("%s%s", gtk_entry_buffer_get_text (buffer), new_text);
  gtk_entry_buffer_set_text (buffer, new_label, current_length + new_text_length);
  *position = *position + new_text_length;

  g_free (new_label);
}

static void
gcal_date_selector_class_init (GcalDateSelectorClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->constructed = gcal_date_selector_constructed;

  signals[MODIFIED] = g_signal_new ("modified",
                                    GCAL_TYPE_DATE_SELECTOR,
                                    G_SIGNAL_RUN_LAST,
                                    G_STRUCT_OFFSET (GcalDateSelectorClass, modified),
                                    NULL, NULL, NULL,
                                    G_TYPE_NONE, 0);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/org/gnome/calendar/date-selector.ui");

  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GcalDateSelector, date_label);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GcalDateSelector, calendar);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GcalDateSelector, grid);

  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), text_inserted);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), date_entry_focus_out);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), entry_activated);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), calendar_day_selected);
}

static void
gcal_date_selector_init (GcalDateSelector *self)
{
  GcalDateSelectorPrivate *priv;
  gint i, d_index, max;

  gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);

  priv = gcal_date_selector_get_instance_private (GCAL_DATE_SELECTOR (self));;

  gtk_widget_init_template (GTK_WIDGET (self));

  priv->day = 1;
  priv->month = 1;
  priv->year = 1970;

  priv->mask = nl_langinfo (D_FMT);

  /**
   * Select the day, month and year indexes. This will
   * be used later on to map the date entries to the
   * corresponding indexes.
   */
  max = strlen (priv->mask);
  d_index = 0;

  for (i = 0; i < max; i++)
    {
      gchar c;

      c = *(priv->mask + i);

      /* No need to check these common separators */
      if (c == '%' || c == '-' || c == '/' || c == '.')
        continue;

      switch (c)
        {
          /* day */
          case 'a':
          case 'A':
          case 'd':
          case 'e':
          case 'j':
          case 'u':
          case 'w':
            priv->day_pos = d_index++;
            break;

          /* month */
          case 'b':
          case 'B':
          case 'm':
            priv->month_pos = d_index++;
            break;

          /* year */
          case 'y':
            priv->year_pos = d_index++;
            break;

          case 'Y':
            priv->year_pos = d_index++;
            break;
        }
    }
}

static void
gcal_date_selector_constructed (GObject *object)
{
  GcalDateSelectorPrivate *priv;

  GtkWidget *label, *box;
  GList *l, *aux;

  priv = gcal_date_selector_get_instance_private (GCAL_DATE_SELECTOR (object));

  /* chaining up */
  G_OBJECT_CLASS (gcal_date_selector_parent_class)->constructed (object);

  /* set labels, on the first row */
  label = gtk_grid_get_child_at (GTK_GRID (priv->grid), priv->day_pos, 0);
  gtk_label_set_text (GTK_LABEL (label), _("Day"));
  label = gtk_grid_get_child_at (GTK_GRID (priv->grid), priv->month_pos, 0);
  gtk_label_set_text (GTK_LABEL (label), _("Month"));
  label = gtk_grid_get_child_at (GTK_GRID (priv->grid), priv->year_pos, 0);
  gtk_label_set_text (GTK_LABEL (label), _("Year"));

  /* retrieve components from UI definition: entries */
  box = gtk_grid_get_child_at (GTK_GRID (priv->grid), 0, 1);
  aux = gtk_container_get_children (GTK_CONTAINER (box));
  for (l = aux; l != NULL; l = g_list_next (l))
    {
      gint position;
      gtk_container_child_get (GTK_CONTAINER (box), l->data, "position", &position, NULL);
      if (position == priv->day_pos)
        priv->entries[DAY] = l->data;
      if (position == priv->month_pos)
        priv->entries[MONTH] = l->data;
      if (position == priv->year_pos)
        priv->entries[YEAR] = l->data;

      if (position == priv->day_pos || position == priv->month_pos)
        gtk_entry_set_max_length (GTK_ENTRY (l->data), 2);
    }
  g_list_free (aux);

  gtk_widget_set_direction (box, GTK_TEXT_DIR_LTR);
  gtk_widget_set_direction (priv->grid, GTK_TEXT_DIR_LTR);
}

/* Public API */
GtkWidget*
gcal_date_selector_new (void)
{
  return g_object_new (GCAL_TYPE_DATE_SELECTOR, NULL);
}

/**
 * gcal_date_selector_set_date:
 * @selector:
 * @day:  (nullable): The day number.
 * @month: (nullable): The month number starting with 1
 * @year: (nullable): The year number
 *
 * Set the value of the date shown
 **/
void
gcal_date_selector_set_date (GcalDateSelector *selector,
                             gint              day,
                             gint              month,
                             gint              year)
{
  GcalDateSelectorPrivate *priv;
  GDateTime *dt;
  gchar *label;

  g_return_if_fail (GCAL_IS_DATE_SELECTOR (selector));
  priv = gcal_date_selector_get_instance_private (selector);
  day = CLAMP (day, 1, 31);
  month = CLAMP (month, 1, 12);
  /* since we're dealing only with the date, the tz shouldn't be a problem */
  dt = g_date_time_new_local (year, month, day, 0, 0, 0);

  /**
   * When it fails to be instances, it's
   * because edit dialog is cleaning it's
   * data. Thus, we should stop here.
   */
  if (dt == NULL)
    return;

  priv->day = day;
  priv->month = month;
  priv->year = year;

  month =  CLAMP (month - 1, 0, 11);

  /* set calendar's date */
  g_signal_handlers_block_by_func (priv->calendar, calendar_day_selected, selector);
  g_object_set (priv->calendar, "day", day, "month", month, "year", year, NULL);
  g_signal_handlers_unblock_by_func (priv->calendar, calendar_day_selected, selector);

  /* rebuild the date label */
  label = g_date_time_format (dt, priv->mask);

  gtk_label_set_label (GTK_LABEL (priv->date_label), label);
  g_free (label);

  /* set date entries' text */
  /* day entry */
  label = g_strdup_printf ("%.2d", day);

  gtk_entry_set_text (GTK_ENTRY (priv->entries[DAY]), label);
  g_free (label);

  /* month entry */
  label = g_strdup_printf ("%.2d", priv->month);

  gtk_entry_set_text (GTK_ENTRY (priv->entries[MONTH]), label);
  g_free (label);

  /* year entry */
  label = g_strdup_printf ("%.4d", year);

  gtk_entry_set_text (GTK_ENTRY (priv->entries[YEAR]), label);

  /* emit the MODIFIED signal */
  g_signal_emit (selector, signals[MODIFIED], 0);

  g_free (label);
  g_date_time_unref (dt);
}

/**
 * gcal_date_selector_get_date:
 * @selector:
 * @day:  (nullable): An out argument to hold the day number
 * @month: (nullable): An out argument to hold the month number starting with 1
 * @year: (nullable): An out argument to hold the year number
 *
 * Get the value of the date shown
 **/
void
gcal_date_selector_get_date (GcalDateSelector *selector,
                             gint             *day,
                             gint             *month,
                             gint             *year)
{
  GcalDateSelectorPrivate *priv;

  g_return_if_fail (GCAL_IS_DATE_SELECTOR (selector));
  priv = gcal_date_selector_get_instance_private (selector);

  if (day != NULL)
    *day = priv->day;
  if (month != NULL)
    *month = priv->month;
  if (year != NULL)
    *year = priv->year;
}
