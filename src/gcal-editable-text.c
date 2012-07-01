/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 2 -*- */
/*
 * gcal-editable-text.c
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

#include "gcal-editable-text.h"

struct _GcalEditableTextPrivate
{
  GtkWidget *view_widget;

  GtkWidget *desc_frame;
  GtkWidget *scrolled_win;
  GtkWidget *edit_widget;
};

static void     gcal_editable_text_constructed             (GObject       *object);

static void     gcal_editable_text_enter_edit_mode         (GcalEditable  *editable);

static void     gcal_editable_text_leave_edit_mode         (GcalEditable  *editable);

static void     gcal_editable_text_clear                   (GcalEditable  *editable);

G_DEFINE_TYPE (GcalEditableText, gcal_editable_text, GCAL_TYPE_EDITABLE);

static void gcal_editable_text_class_init (GcalEditableTextClass *klass)
{
  GcalEditableClass *editable_class;
  GObjectClass *object_class;

  editable_class = GCAL_EDITABLE_CLASS (klass);
  editable_class->enter_edit_mode = gcal_editable_text_enter_edit_mode;
  editable_class->leave_edit_mode = gcal_editable_text_leave_edit_mode;
  editable_class->clear = gcal_editable_text_clear;

  object_class = G_OBJECT_CLASS (klass);
  object_class->constructed = gcal_editable_text_constructed;

  g_type_class_add_private ((gpointer)klass, sizeof (GcalEditableTextPrivate));
}



static void gcal_editable_text_init (GcalEditableText *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            GCAL_TYPE_EDITABLE_TEXT,
                                            GcalEditableTextPrivate);
}

static void
gcal_editable_text_constructed (GObject *object)
{
  GcalEditableTextPrivate *priv;

  priv = GCAL_EDITABLE_TEXT (object)->priv;

  if (G_OBJECT_CLASS (gcal_editable_text_parent_class)->constructed != NULL)
    G_OBJECT_CLASS (gcal_editable_text_parent_class)->constructed (object);

  priv->view_widget = gtk_label_new (NULL);
  gtk_label_set_ellipsize (GTK_LABEL (priv->view_widget),
                           PANGO_ELLIPSIZE_END);
  gtk_widget_show (priv->view_widget);
  gtk_widget_set_valign (priv->view_widget, GTK_ALIGN_START);
  gtk_widget_set_halign (priv->view_widget, GTK_ALIGN_START);
  gcal_editable_set_view_widget (GCAL_EDITABLE (object), priv->view_widget);

  priv->edit_widget = gtk_text_view_new ();
  gtk_widget_show (priv->edit_widget);

  priv->scrolled_win = gtk_scrolled_window_new (NULL, NULL);
  gtk_container_add (GTK_CONTAINER (priv->scrolled_win), priv->edit_widget);

  priv->desc_frame = gtk_frame_new (NULL);
  gtk_container_add (GTK_CONTAINER (priv->desc_frame), priv->scrolled_win);

  gtk_widget_show (priv->desc_frame);
  gcal_editable_set_edit_widget (GCAL_EDITABLE (object), priv->desc_frame);
}

static void
gcal_editable_text_enter_edit_mode (GcalEditable *editable)
{
  GcalEditableTextPrivate *priv;
  GtkTextBuffer *buffer;

  g_return_if_fail (GCAL_IS_EDITABLE_TEXT (editable));
  priv = GCAL_EDITABLE_TEXT (editable)->priv;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->edit_widget));
  gtk_text_buffer_set_text (buffer,
                            gtk_label_get_text (GTK_LABEL (priv->view_widget)),
                            -1);

  gtk_widget_grab_focus (priv->edit_widget);
}

static void
gcal_editable_text_leave_edit_mode (GcalEditable *editable)
{
  GcalEditableTextPrivate *priv;
  GtkTextBuffer *buffer;
  GtkTextIter start, end;
  gchar *text;

  g_return_if_fail (GCAL_IS_EDITABLE_TEXT (editable));
  priv = GCAL_EDITABLE_TEXT (editable)->priv;
  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->edit_widget));
  gtk_text_buffer_get_start_iter (buffer, &start);
  gtk_text_buffer_get_end_iter (buffer, &end);
  text = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
  gtk_label_set_text (GTK_LABEL (priv->view_widget), text);
  g_free (text);
}

static void
gcal_editable_text_clear (GcalEditable  *editable)
{
  GcalEditableTextPrivate *priv;
  GtkTextBuffer *buffer;

  g_return_if_fail (GCAL_IS_EDITABLE_TEXT (editable));
  priv = GCAL_EDITABLE_TEXT (editable)->priv;
  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->edit_widget));
  gtk_text_buffer_set_text (buffer, "", -1);
}

/* Public API */
/**
 * gcal_editable_text_new:
 *
 * Init the entry in EDIT_MODE
 *
 * Since: 0.1
 * Returns: (transfer full):
 **/
GtkWidget*
gcal_editable_text_new (void)
{
  GtkWidget *entry = g_object_new (GCAL_TYPE_EDITABLE_TEXT, NULL);
  gcal_editable_enter_edit_mode (GCAL_EDITABLE (entry));
  return entry;
}

/**
 * gcal_editable_text_new_with_content:
 *
 * Init the entry in VIEW_MODE
 *
 * Since 0.2
 * Returns: (transfer full)
 */
GtkWidget*
gcal_editable_text_new_with_content (const gchar* content)
{
  GtkWidget *entry;
  entry = g_object_new (GCAL_TYPE_EDITABLE_TEXT, NULL);
  gcal_editable_text_set_content (GCAL_EDITABLE_TEXT (entry), content);
  gcal_editable_leave_edit_mode (GCAL_EDITABLE (entry));
  return entry;
}

void
gcal_editable_text_set_content (GcalEditableText *entry,
                                 const gchar       *content)
{
  GcalEditableTextPrivate *priv;

  g_return_if_fail (GCAL_IS_EDITABLE_TEXT (entry));
  priv = entry->priv;

  gtk_label_set_text (GTK_LABEL (priv->view_widget), content);
}
