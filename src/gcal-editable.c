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
#include "gcal-utils.h"

struct _GcalEditablePrivate
{
  GtkWidget         *view_widget;
  GtkWidget         *event_box;

  GcalEditableMode   mode : 1;
};

static void     gcal_editable_constructed             (GObject        *object);

static gboolean gcal_editable_button_pressed          (GtkWidget      *widget,
                                                       GdkEventButton *event,
                                                       gpointer        user_data);

G_DEFINE_TYPE (GcalEditable, gcal_editable, GTK_TYPE_NOTEBOOK);

static void gcal_editable_class_init (GcalEditableClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->constructed = gcal_editable_constructed;

  klass->update_view_widget = NULL;
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
gcal_editable_constructed (GObject       *object)
{
  GcalEditablePrivate *priv;

  priv = GCAL_EDITABLE (object)->priv;

  priv->mode = GCAL_VIEW_MODE;
  priv->view_widget = gtk_label_new (NULL);
  priv->event_box = gtk_event_box_new ();
  gtk_container_add (GTK_CONTAINER (priv->event_box),
                     priv->view_widget);
  gtk_widget_show_all (priv->event_box);
  gtk_notebook_insert_page (GTK_NOTEBOOK (object),
                            priv->event_box,
                            NULL,
                            0);

  gtk_notebook_set_current_page (GTK_NOTEBOOK (object), 0);
  gtk_notebook_set_show_tabs (GTK_NOTEBOOK (object), FALSE);

  g_signal_connect (priv->event_box,
                    "button-press-event",
                    G_CALLBACK (gcal_editable_button_pressed), object);
}

static gboolean
gcal_editable_button_pressed (GtkWidget      *widget,
                              GdkEventButton *event,
                              gpointer        user_data)
{
  if (event->type == GDK_2BUTTON_PRESS)
    gcal_editable_enter_edit_mode (user_data);

  return TRUE;
}

/* Public API */
void
gcal_editable_set_view_contents (GcalEditable *editable,
                                 const gchar  *contents)
{
  GcalEditablePrivate *priv;

  g_return_if_fail (GCAL_IS_EDITABLE (editable));
  priv = editable->priv;

  gtk_label_set_text (GTK_LABEL (priv->view_widget), contents);
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

const gchar*
gcal_editable_get_view_contents (GcalEditable *editable)
{
  GcalEditablePrivate *priv;

  g_return_val_if_fail (GCAL_IS_EDITABLE (editable), NULL);
  priv = GCAL_EDITABLE (editable)->priv;
  return gtk_label_get_text (GTK_LABEL (priv->view_widget));
}

void
gcal_editable_enter_edit_mode (GcalEditable *editable)
{
  GcalEditablePrivate *priv;

  g_return_if_fail (GCAL_IS_EDITABLE (editable));
  priv = editable->priv;

  if (priv->mode == GCAL_VIEW_MODE)
    {
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
  gchar *str;

  g_return_if_fail (GCAL_IS_EDITABLE (editable));
  priv = editable->priv;

  if (priv->mode == GCAL_EDIT_MODE)
    {
      str = GCAL_EDITABLE_GET_CLASS (editable)->update_view_widget (editable);
      if (str != NULL)
        {
          gtk_label_set_text (GTK_LABEL (priv->view_widget), str);
          g_free (str);
        }

      gtk_notebook_set_current_page (GTK_NOTEBOOK (editable), 0);

      if (GCAL_EDITABLE_GET_CLASS (editable)->leave_edit_mode != NULL)
        GCAL_EDITABLE_GET_CLASS (editable)->leave_edit_mode (editable);

      priv->mode = GCAL_VIEW_MODE;
    }
}
