/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gcal-date-selector.c
 * Copyright (C) 2012 Erick Pérez Castellanos <erickpc@gnome.org>
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

struct _GcalDateSelectorPrivate
{
  gboolean  internal_skip;

  gint      day;
  gint      month;
  gint      year;

  gchar    *mask;
  guint     day_pos;
  guint     month_pos;
  guint     year_pos;
  gboolean  have_long_year;
};

enum
{
  MODIFIED,
  NUM_SIGNALS
};

static guint signals[NUM_SIGNALS] = { 0, };

static void     gcal_date_selector_constructed                    (GObject              *object);

G_DEFINE_TYPE_WITH_PRIVATE (GcalDateSelector, gcal_date_selector, GTK_TYPE_TOGGLE_BUTTON);

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
  gchar *have_y;

  gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);

  priv = gcal_date_selector_get_instance_private (GCAL_DATE_SELECTOR (self));;

  priv->internal_skip = FALSE;
  priv->day = 1;
  priv->month = 1;
  priv->year = 1970;

  setlocale (LC_ALL,"");
  priv->mask = nl_langinfo (D_FMT);

  priv->day_pos = - (priv->mask - g_strstr_len (priv->mask, -1, "%d"));
  priv->month_pos = - (priv->mask - g_strstr_len (priv->mask, -1, "%m"));
  if ((have_y = g_strstr_len (priv->mask, - 1, "%y")) != NULL)
    {
      priv->have_long_year = FALSE;
      priv->year_pos = - (priv->mask - have_y);
    }
  else
    {
      priv->have_long_year = TRUE;
      priv->year_pos = - (priv->mask - g_strstr_len (priv->mask, -1, "%Y"));
      if (priv->year_pos < priv->day_pos)
        priv->day_pos += 2;
      if (priv->year_pos < priv->month_pos)
        priv->month_pos += 2;
    }
}

static void
gcal_date_selector_constructed (GObject *object)
{
  GcalDateSelectorPrivate *priv;

  priv = gcal_date_selector_get_instance_private (GCAL_DATE_SELECTOR (object));

  /* chaining up */
  G_OBJECT_CLASS (gcal_date_selector_parent_class)->constructed (object);
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
  GcalDateSelectorPrivate *priv;

  g_return_if_fail (GCAL_IS_DATE_SELECTOR (selector));
  priv = gcal_date_selector_get_instance_private (selector);

  priv->day = day;
  priv->month = month;
  priv->year = year;
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
