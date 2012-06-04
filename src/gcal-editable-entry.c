/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 2 -*- */
/*
 * gcal-editable-entry.c
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

#include "gcal-editable-entry.h"

struct _GcalEditableEntryPrivate
{
  GtkWidget *edit_widget;

  gboolean   update_view;
};

static void     gcal_editable_entry_constructed             (GObject       *object);

static gchar*   gcal_editable_entry_update_view_widget      (GcalEditable  *editable);

static void     gcal_editable_entry_enter_edit_mode         (GcalEditable  *editable);

static gboolean gcal_editable_entry_key_pressed             (GtkWidget     *widget,
                                                             GdkEventKey   *event,
                                                             gpointer       user_data);

static void     gcal_editable_entry_activated               (GtkEntry      *entry,
                                                             gpointer       user_data);

G_DEFINE_TYPE (GcalEditableEntry, gcal_editable_entry, GCAL_TYPE_EDITABLE);

static void gcal_editable_entry_class_init (GcalEditableEntryClass *klass)
{
  GcalEditableClass *editable_class;
  GObjectClass *object_class;

  editable_class = GCAL_EDITABLE_CLASS (klass);
  editable_class->update_view_widget = gcal_editable_entry_update_view_widget;
  editable_class->enter_edit_mode = gcal_editable_entry_enter_edit_mode;
  editable_class->leave_edit_mode = NULL;

  object_class = G_OBJECT_CLASS (klass);
  object_class->constructed = gcal_editable_entry_constructed;

  g_type_class_add_private ((gpointer)klass, sizeof (GcalEditableEntryPrivate));
}



static void gcal_editable_entry_init (GcalEditableEntry *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            GCAL_TYPE_EDITABLE_ENTRY,
                                            GcalEditableEntryPrivate);
}

static void
gcal_editable_entry_constructed (GObject       *object)
{
  GcalEditableEntryPrivate *priv;

  priv = GCAL_EDITABLE_ENTRY (object)->priv;

  if (G_OBJECT_CLASS (gcal_editable_entry_parent_class)->constructed != NULL)
    G_OBJECT_CLASS (gcal_editable_entry_parent_class)->constructed (object);

  priv->edit_widget = gtk_entry_new ();
  gtk_widget_show (priv->edit_widget);
  gcal_editable_set_edit_widget (GCAL_EDITABLE (object), priv->edit_widget);

  g_signal_connect (priv->edit_widget,
                    "key-press-event",
                    G_CALLBACK (gcal_editable_entry_key_pressed),
                    object);
  g_signal_connect (priv->edit_widget,
                    "activate",
                    G_CALLBACK (gcal_editable_entry_activated),
                    object);

  priv->update_view = TRUE;
}

static gchar*
gcal_editable_entry_update_view_widget (GcalEditable *editable)
{
  GcalEditableEntryPrivate *priv;
  gchar *contents;

  g_return_val_if_fail (GCAL_IS_EDITABLE_ENTRY (editable), NULL);
  priv = GCAL_EDITABLE_ENTRY (editable)->priv;
  contents = g_strdup (gtk_entry_get_text (GTK_ENTRY (priv->edit_widget)));
  return priv->update_view ? contents : NULL;
}

static void
gcal_editable_entry_enter_edit_mode (GcalEditable *editable)
{
  GcalEditableEntryPrivate *priv;
  const gchar *contents;

  g_return_if_fail (GCAL_IS_EDITABLE_ENTRY (editable));
  priv = GCAL_EDITABLE_ENTRY (editable)->priv;

  contents = gcal_editable_get_view_contents (editable);
  gtk_entry_set_text (GTK_ENTRY (priv->edit_widget), contents);
  gtk_editable_select_region (GTK_EDITABLE (priv->edit_widget), 0, -1);

  gtk_widget_grab_focus (priv->edit_widget);
}

static gboolean
gcal_editable_entry_key_pressed (GtkWidget   *widget,
                                 GdkEventKey *event,
                                 gpointer     user_data)
{
  GcalEditableEntryPrivate *priv;

  g_return_val_if_fail (GCAL_IS_EDITABLE_ENTRY (user_data), FALSE);
  priv = GCAL_EDITABLE_ENTRY (user_data)->priv;

  if (event->keyval == GDK_KEY_Escape)
    {
      priv->update_view = FALSE;
      gcal_editable_leave_edit_mode (GCAL_EDITABLE (user_data));
      return TRUE;
    }
  return FALSE;
}

static void
gcal_editable_entry_activated (GtkEntry *entry,
                               gpointer  user_data)
{
  if (gtk_entry_get_text_length (entry) != 0)
    {
      gcal_editable_leave_edit_mode (GCAL_EDITABLE (user_data));
    }
}

/**
 * gcal_editable_entry_new:
 * @date:
 *
 * Since: 0.1
 * Returns: (transfer full):
 **/
GtkWidget*
gcal_editable_entry_new (void)
{
  return g_object_new (GCAL_TYPE_EDITABLE_ENTRY, NULL);
}
