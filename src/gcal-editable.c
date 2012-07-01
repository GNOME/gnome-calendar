/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 2 -*- */
/*
 * gcal-editable.c
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

#include "gcal-editable.h"

struct _GcalEditablePrivate
{
  gboolean           locked;
  GcalEditableMode   mode : 1;
};

static void     gcal_editable_constructed             (GObject        *object);

G_DEFINE_TYPE (GcalEditable, gcal_editable, GTK_TYPE_NOTEBOOK);

static void gcal_editable_class_init (GcalEditableClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->constructed = gcal_editable_constructed;

  klass->enter_edit_mode = gcal_editable_enter_edit_mode;
  klass->leave_edit_mode = gcal_editable_leave_edit_mode;

  g_type_class_add_private ((gpointer)klass, sizeof (GcalEditablePrivate));
}



static void gcal_editable_init (GcalEditable *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            GCAL_TYPE_EDITABLE,
                                            GcalEditablePrivate);
}

static void
gcal_editable_constructed (GObject *object)
{
  GcalEditablePrivate *priv;

  priv = GCAL_EDITABLE (object)->priv;

  priv->locked = FALSE;
  priv->mode = GCAL_VIEW_MODE;

  gtk_notebook_set_show_tabs (GTK_NOTEBOOK (object), FALSE);
}

/* Public API */
void
gcal_editable_set_view_widget (GcalEditable *editable,
                               GtkWidget    *widget)
{
  g_return_if_fail (GCAL_IS_EDITABLE (editable));

  if (gtk_notebook_get_nth_page (GTK_NOTEBOOK (editable), 0) != NULL)
    gtk_notebook_remove_page (GTK_NOTEBOOK (editable), 0);

  gtk_notebook_insert_page (GTK_NOTEBOOK (editable), widget, NULL, 0);
}

void
gcal_editable_set_edit_widget (GcalEditable *editable,
                               GtkWidget    *widget)
{
  g_return_if_fail (GCAL_IS_EDITABLE (editable));

  if (gtk_notebook_get_nth_page (GTK_NOTEBOOK (editable), 1) != NULL)
    gtk_notebook_remove_page (GTK_NOTEBOOK (editable), 1);

  gtk_notebook_insert_page (GTK_NOTEBOOK (editable), widget, NULL, 1);
}

void
gcal_editable_enter_edit_mode (GcalEditable *editable)
{
  GcalEditablePrivate *priv;
  GtkWidget *view_widget;
  GtkWidget *edit_widget;

  g_return_if_fail (GCAL_IS_EDITABLE (editable));
  priv = editable->priv;

  if (! priv->locked && priv->mode == GCAL_VIEW_MODE)
    {
      edit_widget = gtk_notebook_get_nth_page (GTK_NOTEBOOK (editable), 1);
      gtk_widget_show_all (edit_widget);
      view_widget = gtk_notebook_get_nth_page (GTK_NOTEBOOK (editable), 0);
      gtk_widget_hide (view_widget);

      gtk_notebook_set_current_page (GTK_NOTEBOOK (editable), 1);

      if (GCAL_EDITABLE_GET_CLASS (editable)->enter_edit_mode != NULL)
        GCAL_EDITABLE_GET_CLASS (editable)->enter_edit_mode (editable);

      priv->mode = GCAL_EDIT_MODE;
    }
}

void
gcal_editable_leave_edit_mode (GcalEditable *editable)
{
  GcalEditablePrivate *priv;
  GtkWidget *view_widget;
  GtkWidget *edit_widget;

  g_return_if_fail (GCAL_IS_EDITABLE (editable));
  priv = editable->priv;

  if (! priv->locked && priv->mode == GCAL_EDIT_MODE)
    {
      edit_widget = gtk_notebook_get_nth_page (GTK_NOTEBOOK (editable), 1);
      gtk_widget_hide (edit_widget);
      view_widget = gtk_notebook_get_nth_page (GTK_NOTEBOOK (editable), 0);
      gtk_widget_show_all (view_widget);

      gtk_notebook_set_current_page (GTK_NOTEBOOK (editable), 0);

      if (GCAL_EDITABLE_GET_CLASS (editable)->leave_edit_mode != NULL)
        GCAL_EDITABLE_GET_CLASS (editable)->leave_edit_mode (editable);

      priv->mode = GCAL_VIEW_MODE;
    }
}

/**
 * gcal_editable_clear:
 * @editable: A GcalEditable widget
 *
 * Will clear the content of the widgets, and set it into GCAL_EDIT_MODE
 */
void
gcal_editable_clear (GcalEditable *editable)
{
  g_return_if_fail (GCAL_IS_EDITABLE (editable));

  if (GCAL_EDITABLE_GET_CLASS (editable)->clear != NULL)
    GCAL_EDITABLE_GET_CLASS (editable)->clear (editable);

  GCAL_EDITABLE_GET_CLASS (editable)->enter_edit_mode (editable);
}

GcalEditableMode
gcal_editable_get_mode (GcalEditable *editable)
{
  GcalEditablePrivate *priv;

  g_return_val_if_fail (GCAL_IS_EDITABLE (editable), GCAL_VIEW_MODE);
  priv = editable->priv;
  return priv->mode;
}

void
gcal_editable_lock (GcalEditable *editable)
{
  GcalEditablePrivate *priv;

  g_return_if_fail (GCAL_IS_EDITABLE (editable));
  priv = editable->priv;
  priv->locked = TRUE;
}

void
gcal_editable_unlock (GcalEditable *editable)
{
  GcalEditablePrivate *priv;

  g_return_if_fail (GCAL_IS_EDITABLE (editable));
  priv = editable->priv;
  priv->locked = FALSE;
}
