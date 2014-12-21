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

static void     set_date                                          (GcalDateSelector     *selector,
                                                                   gint                  day,
                                                                   gint                  month,
                                                                   gint                  year);

static void     gcal_date_selector_constructed                    (GObject              *object);

G_DEFINE_TYPE_WITH_PRIVATE (GcalDateSelector, gcal_date_selector, GTK_TYPE_TOGGLE_BUTTON);

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

  g_date_time_unref (dt);
  g_free (label);
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
  GtkWidget *grid;
  GtkBuilder *builder;

  GSettings *settings;
  gchar *clock_format;

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

  /* signals and properties */
  gtk_container_add (GTK_CONTAINER (priv->popover), grid);
  g_object_bind_property (priv->popover, "visible", object, "active", G_BINDING_BIDIRECTIONAL);
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
