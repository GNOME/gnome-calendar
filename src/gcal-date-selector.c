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
  GtkWidget   *popover;
  GtkWidget   *entries[NUM_ENTRIES];
  GtkWidget   *calendar;

  /* date */
  gint         day;
  gint         month;
  gint         year;

  /* misc */
  gchar       *mask;
  guint        day_pos;
  guint        month_pos;
  guint        year_pos;
  gboolean     have_long_year;

  gboolean     format_24h;
};

enum
{
  MODIFIED,
  NUM_SIGNALS
};

static guint signals[NUM_SIGNALS] = { 0, };

static void     calendar_day_selected                             (GtkCalendar          *calendar,
                                                                   gpointer              user_data);

static void     text_inserted                                     (GtkEditable          *editable,
                                                                   gchar                *new_text,
                                                                   gint                  new_text_length,
                                                                   gint                 *position,
                                                                   gpointer              user_data);

static void     set_date                                          (GcalDateSelector     *selector,
                                                                   gint                  day,
                                                                   gint                  month,
                                                                   gint                  year);

static void     gcal_date_selector_constructed                    (GObject              *object);

G_DEFINE_TYPE_WITH_PRIVATE (GcalDateSelector, gcal_date_selector, GTK_TYPE_TOGGLE_BUTTON);

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
  g_signal_handlers_block_by_func (priv->calendar,
                                   calendar_day_selected,
                                   user_data);
  set_date (GCAL_DATE_SELECTOR (user_data), day, month + 1, year);

  g_signal_handlers_unblock_by_func (priv->calendar,
                                     calendar_day_selected,
                                     user_data);
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
set_date (GcalDateSelector *selector,
          gint              day,
          gint              month,
          gint              year)
{
  GcalDateSelectorPrivate *priv;
  GDateTime *dt;
  gchar *label;

  g_return_if_fail (GCAL_IS_DATE_SELECTOR (selector));
  priv = gcal_date_selector_get_instance_private (selector);
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
  g_object_set (priv->calendar, "day", day, "month", month, "year", year, NULL);

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
  label = g_strdup_printf ("%.2d", month + 1);

  gtk_entry_set_text (GTK_ENTRY (priv->entries[MONTH]), label);
  g_free (label);

  /* year entry */
  if (priv->have_long_year)
    label = g_strdup_printf ("%.4d", year);
  else
    label = g_strdup_printf ("%.2d", year % 100);

  gtk_entry_set_text (GTK_ENTRY (priv->entries[YEAR]), label);

  g_free (label);
  g_date_time_unref (dt);
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
}

static void
gcal_date_selector_init (GcalDateSelector *self)
{
  GcalDateSelectorPrivate *priv;
  gint i, d_index, max;

  gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);

  priv = gcal_date_selector_get_instance_private (GCAL_DATE_SELECTOR (self));;

  priv->day = 1;
  priv->month = 1;
  priv->year = 1970;
  priv->have_long_year = FALSE;

  setlocale (LC_ALL,"");
  priv->mask = nl_langinfo (D_FMT);
  g_debug ("Mask: %s", priv->mask);

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
            priv->have_long_year = TRUE;
            break;
        }
    }
}

static void
gcal_date_selector_constructed (GObject *object)
{
  GcalDateSelectorPrivate *priv;
  GtkWidget *grid;
  GtkBuilder *builder;

  GSettings *settings;
  gchar *clock_format;
  gchar *entry_name;

  priv = gcal_date_selector_get_instance_private (GCAL_DATE_SELECTOR (object));

  /* chaining up */
  G_OBJECT_CLASS (gcal_date_selector_parent_class)->constructed (object);

  /* 24h setting */
  settings = g_settings_new ("org.gnome.desktop.interface");
  clock_format = g_settings_get_string (settings, "clock-format");
  priv->format_24h = (g_strcmp0 (clock_format, "24h") == 0);

  g_free (clock_format);
  g_object_unref (settings);

  /* date label */
  priv->date_label = gtk_label_new (NULL);
  gtk_widget_show (priv->date_label);

  gtk_container_add (GTK_CONTAINER (object), priv->date_label);

  /* retrieve components from UI definition */
  builder = gtk_builder_new ();
  gtk_builder_add_from_resource (builder, "/org/gnome/calendar/date-selector.ui", NULL);

  /* popover */
  priv->popover = gtk_popover_new (GTK_WIDGET (object));
  gtk_popover_set_position (GTK_POPOVER (priv->popover), GTK_POS_BOTTOM);

  /* main grid */
  grid = (GtkWidget*) gtk_builder_get_object (builder, "grid");
  g_object_ref (grid);

  /* calendar */
  priv->calendar = (GtkWidget*) gtk_builder_get_object (builder, "calendar");
  g_object_ref (priv->calendar);

  /**
   * Date entries
   *
   * day entry 
   */
  entry_name = g_strdup_printf ("entry%d", priv->day_pos);

  priv->entries[DAY] = (GtkWidget*) gtk_builder_get_object (builder, entry_name);
  gtk_entry_set_max_length (GTK_ENTRY (priv->entries[DAY]), 2);
  g_object_ref (priv->entries[DAY]);
  g_free (entry_name);

  /* month entry */
  entry_name = g_strdup_printf ("entry%d", priv->month_pos);

  priv->entries[MONTH] = (GtkWidget*) gtk_builder_get_object (builder, entry_name);
  gtk_entry_set_max_length (GTK_ENTRY (priv->entries[MONTH]), 2);
  g_object_ref (priv->entries[MONTH]);
  g_free (entry_name);

  /* year entry */
  entry_name = g_strdup_printf ("entry%d", priv->year_pos);

  priv->entries[YEAR] = (GtkWidget*) gtk_builder_get_object (builder, entry_name);
  g_object_ref (priv->entries[YEAR]);

  if (! priv->have_long_year)
    gtk_entry_set_max_length (GTK_ENTRY (priv->entries[YEAR]), 2);

  g_free (entry_name);

  /* signals and properties */
  gtk_container_add (GTK_CONTAINER (priv->popover), grid);
  g_object_bind_property (priv->popover, "visible", object, "active", G_BINDING_BIDIRECTIONAL);

  g_signal_connect (priv->entries[DAY], "insert-text", G_CALLBACK (text_inserted), object);
  g_signal_connect (priv->entries[MONTH], "insert-text", G_CALLBACK (text_inserted), object);
  g_signal_connect (priv->entries[YEAR], "insert-text", G_CALLBACK (text_inserted), object);

  g_signal_connect (priv->calendar, "day-selected", G_CALLBACK (calendar_day_selected), object);

  g_object_unref (builder);
}

/* Public API */
GtkWidget*
gcal_date_selector_new (void)
{
  return g_object_new (GCAL_TYPE_DATE_SELECTOR, NULL);
}

void
gcal_date_selector_set_date (GcalDateSelector *selector,
                             gint              day,
                             gint              month,
                             gint              year)
{
  set_date (selector, day, month, year);
}

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
