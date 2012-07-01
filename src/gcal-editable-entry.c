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
  GtkWidget *view_widget;
  GtkWidget *edit_widget;
};

static void     gcal_editable_entry_constructed             (GObject       *object);

static void     gcal_editable_entry_size_allocate           (GtkWidget     *widget,
                                                             GtkAllocation *allocation);

static void     gcal_editable_entry_enter_edit_mode         (GcalEditable  *editable);

static void     gcal_editable_entry_leave_edit_mode         (GcalEditable  *editable);

static void     gcal_editable_entry_clear                   (GcalEditable  *editable);

G_DEFINE_TYPE (GcalEditableEntry, gcal_editable_entry, GCAL_TYPE_EDITABLE);

static void gcal_editable_entry_class_init (GcalEditableEntryClass *klass)
{
  GcalEditableClass *editable_class;
  GtkWidgetClass *widget_class;
  GObjectClass *object_class;

  editable_class = GCAL_EDITABLE_CLASS (klass);
  editable_class->enter_edit_mode = gcal_editable_entry_enter_edit_mode;
  editable_class->leave_edit_mode = gcal_editable_entry_leave_edit_mode;
  editable_class->clear = gcal_editable_entry_clear;

  widget_class = GTK_WIDGET_CLASS (klass);
  widget_class->size_allocate = gcal_editable_entry_size_allocate;

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
gcal_editable_entry_constructed (GObject *object)
{
  GcalEditableEntryPrivate *priv;

  priv = GCAL_EDITABLE_ENTRY (object)->priv;

  if (G_OBJECT_CLASS (gcal_editable_entry_parent_class)->constructed != NULL)
    G_OBJECT_CLASS (gcal_editable_entry_parent_class)->constructed (object);

  priv->view_widget = gtk_label_new (NULL);
  gtk_label_set_ellipsize (GTK_LABEL (priv->view_widget),
                           PANGO_ELLIPSIZE_END);
  gtk_widget_show (priv->view_widget);
  gtk_widget_set_valign (priv->view_widget, GTK_ALIGN_CENTER);
  gtk_widget_set_halign (priv->view_widget, GTK_ALIGN_START);
  gcal_editable_set_view_widget (GCAL_EDITABLE (object), priv->view_widget);
  priv->edit_widget = gtk_entry_new ();
  gtk_widget_show (priv->edit_widget);
  gcal_editable_set_edit_widget (GCAL_EDITABLE (object), priv->edit_widget);
}

static void
gcal_editable_entry_size_allocate (GtkWidget     *widget,
                                   GtkAllocation *allocation)
{
  GcalEditableEntryPrivate *priv;
  GtkAllocation view_allocation;
  gint minimum;
  gint natural;

  g_return_if_fail (GCAL_IS_EDITABLE_ENTRY (widget));
  priv = GCAL_EDITABLE_ENTRY (widget)->priv;

  if (GTK_WIDGET_CLASS (gcal_editable_entry_parent_class)->size_allocate != NULL)
    GTK_WIDGET_CLASS (gcal_editable_entry_parent_class)->size_allocate (widget, allocation);

  //FIXME: hack to keep the label thin
  gtk_widget_get_preferred_height (priv->view_widget, &minimum, &natural);
  gtk_widget_get_allocation (priv->view_widget, &view_allocation);
  view_allocation.height = minimum;
  gtk_widget_size_allocate (priv->view_widget, &view_allocation);
}

static void
gcal_editable_entry_enter_edit_mode (GcalEditable *editable)
{
  GcalEditableEntryPrivate *priv;

  g_return_if_fail (GCAL_IS_EDITABLE_ENTRY (editable));
  priv = GCAL_EDITABLE_ENTRY (editable)->priv;

  gtk_entry_set_text (GTK_ENTRY (priv->edit_widget),
                      gtk_label_get_text (GTK_LABEL (priv->view_widget)));
  gtk_editable_select_region (GTK_EDITABLE (priv->edit_widget), 0, -1);

  gtk_widget_grab_focus (priv->edit_widget);
}

static void
gcal_editable_entry_leave_edit_mode (GcalEditable *editable)
{
  GcalEditableEntryPrivate *priv;

  g_return_if_fail (GCAL_IS_EDITABLE_ENTRY (editable));
  priv = GCAL_EDITABLE_ENTRY (editable)->priv;

  gtk_label_set_text (GTK_LABEL (priv->view_widget),
                      gtk_entry_get_text (GTK_ENTRY (priv->edit_widget)));
}

static void
gcal_editable_entry_clear (GcalEditable  *editable)
{
  GcalEditableEntryPrivate *priv;

  g_return_if_fail (GCAL_IS_EDITABLE_ENTRY (editable));
  priv = GCAL_EDITABLE_ENTRY (editable)->priv;

  gtk_label_set_text (GTK_LABEL (priv->view_widget), "");
  gtk_entry_set_text (GTK_ENTRY (priv->edit_widget), "");
}

/**
 * gcal_editable_entry_new:
 *
 * Init the entry in EDIT_MODE
 *
 * Since: 0.1
 * Returns: (transfer full):
 **/
GtkWidget*
gcal_editable_entry_new (void)
{
  GtkWidget *entry = g_object_new (GCAL_TYPE_EDITABLE_ENTRY, NULL);
  gcal_editable_enter_edit_mode (GCAL_EDITABLE (entry));
  return entry;
}

/**
 * gcal_editable_entry_new_with_content:
 *
 * Init the entry in VIEW_MODE
 *
 * Since 0.2
 * Returns: (transfer full)
 */
GtkWidget*
gcal_editable_entry_new_with_content (const gchar* content)
{
  GtkWidget *entry;
  entry = g_object_new (GCAL_TYPE_EDITABLE_ENTRY, NULL);
  gcal_editable_entry_set_content (GCAL_EDITABLE_ENTRY (entry), content, FALSE);
  gcal_editable_leave_edit_mode (GCAL_EDITABLE (entry));
  return entry;
}

void
gcal_editable_entry_set_content (GcalEditableEntry *entry,
                                 const gchar       *content,
                                 gboolean           use_markup)
{
  GcalEditableEntryPrivate *priv;

  g_return_if_fail (GCAL_IS_EDITABLE_ENTRY (entry));
  priv = entry->priv;

  if (use_markup)
    gtk_label_set_markup (GTK_LABEL (priv->view_widget), content);
  else
    gtk_label_set_text (GTK_LABEL (priv->view_widget), content);
}
