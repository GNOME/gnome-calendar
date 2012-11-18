/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gcal-date-entry.c
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

#include "gcal-date-entry.h"

#include <locale.h>
#include <langinfo.h>
#include <glib/gi18n.h>

struct _GcalDateEntryPrivate
{
  gboolean  internal_skip;

  gint      day;              /* 1 - 28 || 29 || 30 || 31 */
  gint      month;            /* 1 - 12 */
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

static void     gtk_editable_iface_init                        (GtkEditableInterface *iface);

static void     gcal_date_entry_constructed                    (GObject              *object);

static void     gcal_date_entry_icon_press                     (GtkEntry             *entry,
                                                                GtkEntryIconPosition  icon_pos,
                                                                GdkEvent             *event,
                                                                gpointer              user_data);

static void     gcal_date_entry_insert_text                    (GtkEditable          *editable,
                                                                const gchar          *new_text,
                                                                gint                  new_text_length,
                                                                gint                 *position);

static void     gcal_date_entry_delete_text                    (GtkEditable          *editable,
                                                                gint                  start_pos,
                                                                gint                  end_pos);

static void     gcal_date_entry_validate_and_insert            (GcalDateEntry        *entry);

G_DEFINE_TYPE_WITH_CODE (GcalDateEntry,
                         gcal_date_entry,
                         GTK_TYPE_ENTRY,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_EDITABLE,
                                                gtk_editable_iface_init));

static void
gcal_date_entry_class_init (GcalDateEntryClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->constructed = gcal_date_entry_constructed;

  signals[MODIFIED] = g_signal_new ("modified",
                                    GCAL_TYPE_DATE_ENTRY,
                                    G_SIGNAL_RUN_LAST,
                                    G_STRUCT_OFFSET (GcalDateEntryClass,
                                                     modified),
                                    NULL, NULL,
                                    g_cclosure_marshal_VOID__VOID,
                                    G_TYPE_NONE,
                                    0);

  g_type_class_add_private ((gpointer)klass, sizeof(GcalDateEntryPrivate));
}

static void
gcal_date_entry_init (GcalDateEntry *self)
{
  GcalDateEntryPrivate *priv;
  gchar *have_y;

  gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            GCAL_TYPE_DATE_ENTRY,
                                            GcalDateEntryPrivate);
  priv = self->priv;

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
gtk_editable_iface_init (GtkEditableInterface *iface)
{
  iface->insert_text = gcal_date_entry_insert_text;
  iface->delete_text = gcal_date_entry_delete_text;
}

static void
gcal_date_entry_constructed (GObject *object)
{
  GcalDateEntryPrivate *priv;

  priv = GCAL_DATE_ENTRY (object)->priv;

  /* chaining up */
  G_OBJECT_CLASS (gcal_date_entry_parent_class)->constructed (object);

  gtk_entry_set_width_chars (
      GTK_ENTRY (object),
      strlen (priv->mask) + (priv->have_long_year ? 4 : 2));
  gtk_entry_set_alignment (GTK_ENTRY (object), 0.5);

  gtk_entry_set_icon_from_icon_name (GTK_ENTRY (object),
                                     GTK_ENTRY_ICON_SECONDARY,
                                     "x-office-calendar-symbolic");
  gtk_entry_set_icon_tooltip_text (GTK_ENTRY (object),
                                   GTK_ENTRY_ICON_SECONDARY,
                                   _("Change the date"));

  /* default text */
  gcal_date_entry_validate_and_insert (GCAL_DATE_ENTRY (object));

  /* setting signals */
  g_signal_connect (object,
                    "icon-press",
                    G_CALLBACK (gcal_date_entry_icon_press),
                    object);
}

static void
gcal_date_entry_icon_press (GtkEntry             *entry,
                            GtkEntryIconPosition  icon_pos,
                            GdkEvent             *event,
                            gpointer              user_data)
{
  g_debug ("Icon pressed");
}

static void
gcal_date_entry_insert_text (GtkEditable *editable,
                             const gchar *new_text,
                             gint         new_text_length,
                             gint        *position)
{
  GcalDateEntryPrivate *priv;
  GtkEditableInterface *parent_editable_iface;
  gboolean new_string_set;
  gchar new_string [16];
  gint i;

  priv = GCAL_DATE_ENTRY (editable)->priv;
  parent_editable_iface = g_type_interface_peek (gcal_date_entry_parent_class,
                                                 GTK_TYPE_EDITABLE);

  if (priv->internal_skip)
    {
      parent_editable_iface->insert_text (editable,
                                          new_text,
                                          new_text_length,
                                          position);
      *position = 0;
      return;
    }

  new_string_set = FALSE;
  memset (new_string, 0, 16);
  /* Is it day */
  if (*position >= priv->day_pos &&
      *position < priv->day_pos + 2)
    {
      //trim string, decide what to remove
      for (i = 0; i < priv->day_pos + 2 - *position; i++)
        {
          if (g_ascii_isdigit (new_text[i]))
            {
              new_string_set = TRUE;
              new_string[i] = new_text[i];
            }
          else
            {
              break;
            }
        }
    }

  /* Is it month */
  if (*position >= priv->month_pos &&
      *position < priv->month_pos + 2)
    {
      //trim string, decide what to remove
      for (i = 0; i < priv->month_pos + 2 - *position; i++)
        {
          if (g_ascii_isdigit (new_text[i]))
            {
              new_string_set = TRUE;
              new_string[i] = new_text[i];
            }
          else
            {
              break;
            }
        }
    }

  /* Is it year */
  if (*position >= priv->year_pos &&
      *position < priv->year_pos + (priv->have_long_year ? 4 : 2))
    {
      //trim string, decide what to remove
      for (i = 0;
           i < priv->year_pos + (priv->have_long_year ? 4 : 2) - *position;
           i++)
        {
          if (g_ascii_isdigit (new_text[i]))
            {
              new_string_set = TRUE;
              new_string[i] = new_text[i];
            }
          else
            {
              break;
            }
        }
    }

  if (new_string_set)
    {
      gchar* unit_string;
      const gchar* text;

      gint new_day;
      gint new_month;
      gint new_year;

      /* making space for new_string */
      priv->internal_skip = TRUE;
      gtk_editable_delete_text (editable,
                                *position,
                                *position + strlen (new_string));
      priv->internal_skip = FALSE;

      parent_editable_iface->insert_text (editable,
                                          new_string,
                                          strlen (new_string),
                                          position);

      /* Updating internal data and validating */
      text = gtk_entry_get_text (GTK_ENTRY (editable));
      unit_string = g_strndup (text + priv->day_pos, 2);
      new_day = (gint) g_ascii_strtoull (unit_string, NULL, 10);
      g_free (unit_string);

      unit_string = g_strndup (text + priv->month_pos, 2);
      new_month = (gint) g_ascii_strtoull (unit_string, NULL, 10);
      g_free (unit_string);

      unit_string = g_strndup (text + priv->year_pos, (priv->have_long_year ? 4 : 2));
      new_year = (gint) g_ascii_strtoull (unit_string, NULL, 10);
      g_free (unit_string);

      if (priv->day != new_day ||
          priv->month != new_month ||
          priv->year != new_year)
        {
          priv->day = new_day;
          priv->month = new_month;
          priv->year = new_year;

          gcal_date_entry_validate_and_insert (GCAL_DATE_ENTRY (editable));
        }
    }
}

static void
gcal_date_entry_delete_text (GtkEditable *editable,
                             gint         start_pos,
                             gint         end_pos)
{
  /* do nothing, unless we mark it */
  GcalDateEntryPrivate *priv;
  GtkEditableInterface *parent_editable_iface;

  priv = GCAL_DATE_ENTRY (editable)->priv;
  parent_editable_iface = g_type_interface_peek (gcal_date_entry_parent_class,
                                                 GTK_TYPE_EDITABLE);

  if (priv->internal_skip)
    {
      parent_editable_iface->delete_text (editable,
                                          start_pos,
                                          end_pos);
    }
}

static void
gcal_date_entry_validate_and_insert (GcalDateEntry *entry)
{
  GcalDateEntryPrivate *priv;
  gchar *tmp;
  gchar *tmp1;
  gchar **tokens;
  gchar *unit;

  priv = entry->priv;

  if (priv->month > 12)
    priv->month = 12;
  if (priv->day > g_date_days_in_month (priv->month, priv->year))
    priv->day = g_date_days_in_month (priv->month, priv->year);

  tokens = g_strsplit (priv->mask, "%d", -1);
  unit = g_strdup_printf ("%.2d", priv->day);
  tmp = g_strjoinv (unit, tokens);
  g_free (unit);
  g_strfreev (tokens);

  tokens = g_strsplit (tmp, "%m", -1);
  unit = g_strdup_printf ("%.2d", priv->month);
  tmp1 = g_strjoinv (unit, tokens);
  g_free (unit);
  g_strfreev (tokens);

  g_free (tmp);
  if (priv->have_long_year)
    {
      tokens = g_strsplit (tmp1, "%Y", -1);
      unit = g_strdup_printf ("%.4d", priv->year);
      tmp = g_strjoinv (unit, tokens);
    }
  else
    {
      tokens = g_strsplit (tmp1, "%y", -1);
      unit = g_strdup_printf ("%.2d", priv->year);
      tmp = g_strjoinv (unit, tokens);
    }

  g_free (unit);
  g_strfreev (tokens);

  priv->internal_skip = TRUE;
  gtk_entry_set_text (GTK_ENTRY (entry), tmp);
  priv->internal_skip = FALSE;

  g_signal_emit (entry, signals[MODIFIED], 0);

  g_free (tmp);
  g_free (tmp1);
}

/* Public API */
GtkWidget*
gcal_date_entry_new (void)
{
  return g_object_new (GCAL_TYPE_DATE_ENTRY, NULL);
}

void
gcal_date_entry_set_date (GcalDateEntry *entry,
                          gint           day,
                          gint           month,
                          gint           year)
{
  GcalDateEntryPrivate *priv;

  g_return_if_fail (GCAL_IS_DATE_ENTRY (entry));
  priv = entry->priv;

  priv->day = day;
  priv->month = month;
  priv->year = year;

  gcal_date_entry_validate_and_insert (entry);
}

void
gcal_date_entry_get_date (GcalDateEntry *entry,
                          gint          *day,
                          gint          *month,
                          gint          *year)
{
  GcalDateEntryPrivate *priv;

  g_return_if_fail (GCAL_IS_DATE_ENTRY (entry));
  priv = entry->priv;

  if (day != NULL)
    *day = priv->day;
  if (month != NULL)
    *month = priv->month;
  if (year != NULL)
    *year = priv->year;
}
