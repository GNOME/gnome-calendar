/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 2 -*- */
/*
 * gcal-editable-date.c
 *
 * Copyright (C) 2012 - Erick Pérez Castellanos
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
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

#include "gcal-editable-date.h"

#include <glib/gi18n.h>

struct _GcalEditableDatePrivate
{
  GtkWidget *view_widget;

  GtkWidget *edit_widget;
  GtkWidget *since_entry;
  GtkWidget *until_entry;
  GtkWidget *all_day_check;
};

static void     gcal_editable_date_constructed             (GObject       *object);

static void     gcal_editable_date_enter_edit_mode         (GcalEditable  *editable);

static void     gcal_editable_date_leave_edit_mode         (GcalEditable  *editable);

static void     gcal_editable_date_clear                   (GcalEditable  *editable);

static void     gcal_editable_date_split_set               (GcalEditable  *editable);

G_DEFINE_TYPE (GcalEditableDate, gcal_editable_date, GCAL_TYPE_EDITABLE);

static void gcal_editable_date_class_init (GcalEditableDateClass *klass)
{
  GcalEditableClass *editable_class;
  GObjectClass *object_class;

  editable_class = GCAL_EDITABLE_CLASS (klass);
  editable_class->enter_edit_mode = gcal_editable_date_enter_edit_mode;
  editable_class->leave_edit_mode = gcal_editable_date_leave_edit_mode;
  editable_class->clear = gcal_editable_date_clear;

  object_class = G_OBJECT_CLASS (klass);
  object_class->constructed = gcal_editable_date_constructed;

  g_type_class_add_private ((gpointer)klass, sizeof (GcalEditableDatePrivate));
}



static void gcal_editable_date_init (GcalEditableDate *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            GCAL_TYPE_EDITABLE_DATE,
                                            GcalEditableDatePrivate);
}

static void
gcal_editable_date_constructed (GObject *object)
{
  GcalEditableDatePrivate *priv;

  priv = GCAL_EDITABLE_DATE (object)->priv;

  if (G_OBJECT_CLASS (gcal_editable_date_parent_class)->constructed != NULL)
    G_OBJECT_CLASS (gcal_editable_date_parent_class)->constructed (object);

  priv->view_widget = gtk_label_new (NULL);
  gtk_widget_show (priv->view_widget);
  gtk_widget_set_valign (priv->view_widget, GTK_ALIGN_START);
  gtk_widget_set_halign (priv->view_widget, GTK_ALIGN_START);
  gcal_editable_set_view_widget (GCAL_EDITABLE (object), priv->view_widget);

  priv->edit_widget = gtk_grid_new ();

  priv->since_entry = gtk_entry_new ();
  priv->until_entry = gtk_entry_new ();
  priv->all_day_check = gtk_check_button_new_with_label (_("All day"));

  gtk_grid_attach (GTK_GRID (priv->edit_widget),
                   priv->since_entry,
                   0, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (priv->edit_widget),
                   gtk_label_new (_("to")),
                   1, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (priv->edit_widget),
                   priv->until_entry,
                   2, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (priv->edit_widget),
                   priv->all_day_check,
                   0, 1, 3, 1);

  gtk_grid_set_column_spacing (GTK_GRID (priv->edit_widget), 6);
  gtk_widget_show_all (priv->edit_widget);

  gcal_editable_set_edit_widget (GCAL_EDITABLE (object), priv->edit_widget);
}

static void
gcal_editable_date_enter_edit_mode (GcalEditable *editable)
{
  GcalEditableDatePrivate *priv;

  g_return_if_fail (GCAL_IS_EDITABLE_DATE (editable));
  priv = GCAL_EDITABLE_DATE (editable)->priv;

  gcal_editable_date_split_set (editable);
  gtk_widget_grab_focus (priv->since_entry);
}

static void
gcal_editable_date_leave_edit_mode (GcalEditable *editable)
{
  GcalEditableDatePrivate *priv;
  gchar *built;

  g_return_if_fail (GCAL_IS_EDITABLE_DATE (editable));
  priv = GCAL_EDITABLE_DATE (editable)->priv;
  built = NULL;

  if (g_strcmp0 (gtk_entry_get_text (GTK_ENTRY (priv->since_entry)), "") != 0)
    built = g_strdup (gtk_entry_get_text (GTK_ENTRY (priv->since_entry)));

  if (g_strcmp0 (gtk_entry_get_text (GTK_ENTRY (priv->until_entry)), "") != 0)
    {
      if (built != NULL)
        {
          gchar *c;
          c = built;
          built = g_strconcat (c,
                               " - ",
                               gtk_entry_get_text (GTK_ENTRY (priv->until_entry)),
                               NULL);
          g_free (c);
        }
      else
        {
          built = g_strdup (gtk_entry_get_text (GTK_ENTRY (priv->until_entry)));
        }
    }

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->all_day_check))
      && built != NULL)
    {
      gchar *c;
      c = built;
      built = g_strconcat (c, _(" (All day)"), NULL);
      g_free (c);
    }

  gtk_label_set_text (GTK_LABEL (priv->view_widget), built);
  g_free (built);
}

static void
gcal_editable_date_clear (GcalEditable  *editable)
{
  GcalEditableDatePrivate *priv;

  g_return_if_fail (GCAL_IS_EDITABLE_DATE (editable));
  priv = GCAL_EDITABLE_DATE (editable)->priv;

  gtk_label_set_text (GTK_LABEL (priv->view_widget), NULL);
  gtk_entry_set_text (GTK_ENTRY (priv->since_entry), NULL);
  gtk_entry_set_text (GTK_ENTRY (priv->until_entry), NULL);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->all_day_check), FALSE);
}

static void
gcal_editable_date_split_set (GcalEditable  *editable)
{
  GcalEditableDatePrivate *priv;
  gchar **tokens;

  g_return_if_fail (GCAL_IS_EDITABLE_DATE (editable));
  priv = GCAL_EDITABLE_DATE (editable)->priv;

  if (g_strcmp0 (gtk_label_get_text (GTK_LABEL (priv->view_widget)), "") != 0)
    {
      gchar *label_text;
      if (g_str_has_suffix (
            gtk_label_get_text (GTK_LABEL (priv->view_widget)),
            _(" (All day)")))
        {
          const gchar *c;
          c = gtk_label_get_text (GTK_LABEL (priv->view_widget));
          label_text = g_strndup (c, g_strrstr (c, _(" (All day)")) - c);

          gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->all_day_check),
                                        TRUE);
        }
      else
        {
          label_text =
            g_strdup (gtk_label_get_text (GTK_LABEL (priv->view_widget)));
          gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->all_day_check),
                                        FALSE);
        }

      tokens = g_strsplit (label_text, "-", -1);

      if (tokens[0] != NULL)
        {
          tokens[0] = g_strstrip (tokens[0]);
          gtk_entry_set_text (GTK_ENTRY (priv->since_entry), tokens[0]);
        }
      if (tokens[1] != NULL)
        {
          tokens[1] = g_strstrip (tokens[1]);
          gtk_entry_set_text (GTK_ENTRY (priv->until_entry), tokens[1]);
        }

      g_strfreev (tokens);
    }
}

/**
 * gcal_editable_date_new:
 *
 * Init the date widget in EDIT_MODE
 *
 * Since: 0.1
 * Returns: (transfer full):
 **/
GtkWidget*
gcal_editable_date_new (void)
{
  GtkWidget *date = g_object_new (GCAL_TYPE_EDITABLE_DATE, NULL);
  gcal_editable_enter_edit_mode (GCAL_EDITABLE (date));
  return date;
}

/**
 * gcal_editable_date_new_with_content:
 *
 * Init the date widget in VIEW_MODE
 *
 * Since 0.2
 * Returns: (transfer full)
 */
GtkWidget*
gcal_editable_date_new_with_content (const gchar* content)
{
  GtkWidget *date;
  date = g_object_new (GCAL_TYPE_EDITABLE_DATE, NULL);
  gcal_editable_date_set_content (GCAL_EDITABLE_DATE (date), content);
  gcal_editable_leave_edit_mode (GCAL_EDITABLE (date));
  return date;
}

void
gcal_editable_date_set_content (GcalEditableDate *date,
                                const gchar      *content)
{
  GcalEditableDatePrivate *priv;

  g_return_if_fail (GCAL_IS_EDITABLE_DATE (date));
  priv = date->priv;

  gtk_label_set_text (GTK_LABEL (priv->view_widget), content);
}
