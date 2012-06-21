/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 2 -*- */
/*
 * gcal-editable-combo.c
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

#include "gcal-editable-combo.h"

struct _GcalEditableComboPrivate
{
  GtkWidget *view_widget;
  GtkWidget *edit_widget;
};

static void     gcal_editable_combo_constructed             (GObject       *object);

static void     gcal_editable_combo_size_allocate           (GtkWidget     *widget,
                                                             GtkAllocation *allocation);

static void     gcal_editable_combo_enter_edit_mode         (GcalEditable  *editable);

static void     gcal_editable_combo_leave_edit_mode         (GcalEditable  *editable);

static void     gcal_editable_combo_clear                   (GcalEditable  *editable);

G_DEFINE_TYPE (GcalEditableCombo, gcal_editable_combo, GCAL_TYPE_EDITABLE);

static void gcal_editable_combo_class_init (GcalEditableComboClass *klass)
{
  GcalEditableClass *editable_class;
  GtkWidgetClass *widget_class;
  GObjectClass *object_class;

  editable_class = GCAL_EDITABLE_CLASS (klass);
  editable_class->enter_edit_mode = gcal_editable_combo_enter_edit_mode;
  editable_class->leave_edit_mode = gcal_editable_combo_leave_edit_mode;
  editable_class->clear = gcal_editable_combo_clear;

  widget_class = GTK_WIDGET_CLASS (klass);
  widget_class->size_allocate = gcal_editable_combo_size_allocate;

  object_class = G_OBJECT_CLASS (klass);
  object_class->constructed = gcal_editable_combo_constructed;

  g_type_class_add_private ((gpointer)klass, sizeof (GcalEditableComboPrivate));
}



static void gcal_editable_combo_init (GcalEditableCombo *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            GCAL_TYPE_EDITABLE_COMBO,
                                            GcalEditableComboPrivate);
}

static void
gcal_editable_combo_constructed (GObject *object)
{
  GcalEditableComboPrivate *priv;

  priv = GCAL_EDITABLE_COMBO (object)->priv;

  if (G_OBJECT_CLASS (gcal_editable_combo_parent_class)->constructed != NULL)
    G_OBJECT_CLASS (gcal_editable_combo_parent_class)->constructed (object);

  priv->view_widget = gtk_label_new (NULL);
  gtk_label_set_ellipsize (GTK_LABEL (priv->view_widget),
                           PANGO_ELLIPSIZE_END);
  gtk_widget_show (priv->view_widget);
  gtk_widget_set_valign (priv->view_widget, GTK_ALIGN_CENTER);
  gtk_widget_set_halign (priv->view_widget, GTK_ALIGN_START);
  gcal_editable_set_view_widget (GCAL_EDITABLE (object), priv->view_widget);

  priv->edit_widget = gtk_combo_box_new ();
  gtk_widget_show (priv->edit_widget);
  gcal_editable_set_edit_widget (GCAL_EDITABLE (object), priv->edit_widget);
}

static void
gcal_editable_combo_size_allocate (GtkWidget     *widget,
                                   GtkAllocation *allocation)
{
  GcalEditableComboPrivate *priv;
  GtkAllocation view_allocation;
  gint minimum;
  gint natural;

  g_return_if_fail (GCAL_IS_EDITABLE_COMBO (widget));
  priv = GCAL_EDITABLE_COMBO (widget)->priv;

  if (GTK_WIDGET_CLASS (gcal_editable_combo_parent_class)->size_allocate != NULL)
    GTK_WIDGET_CLASS (gcal_editable_combo_parent_class)->size_allocate (widget, allocation);

  //FIXME: hack to keep the label thin
  gtk_widget_get_preferred_height (priv->view_widget, &minimum, &natural);
  gtk_widget_get_allocation (priv->view_widget, &view_allocation);
  view_allocation.height = minimum;
  gtk_widget_size_allocate (priv->view_widget, &view_allocation);
}

static void
gcal_editable_combo_enter_edit_mode (GcalEditable *editable)
{
  GcalEditableComboPrivate *priv;
  gchar *uid;

  g_return_if_fail (GCAL_IS_EDITABLE_COMBO (editable));
  priv = GCAL_EDITABLE_COMBO (editable)->priv;

  if (gtk_combo_box_get_model (GTK_COMBO_BOX (priv->edit_widget)) != NULL)
    {
      uid = gcal_get_source_uid (
          gtk_combo_box_get_model (GTK_COMBO_BOX (priv->edit_widget)),
          gtk_label_get_text (GTK_LABEL (priv->view_widget)));

      gtk_combo_box_set_active_id (GTK_COMBO_BOX (priv->edit_widget), uid);
      g_free (uid);

      gtk_widget_grab_focus (priv->edit_widget);
    }
}

static void
gcal_editable_combo_leave_edit_mode (GcalEditable *editable)
{
  GcalEditableComboPrivate *priv;
  gchar *name;

  g_return_if_fail (GCAL_IS_EDITABLE_COMBO (editable));
  priv = GCAL_EDITABLE_COMBO (editable)->priv;

  if (gtk_combo_box_get_model (GTK_COMBO_BOX (priv->edit_widget)) == NULL)
    {
      gtk_label_set_text (GTK_LABEL (priv->view_widget), NULL);
    }
  else
    {
      name = gcal_get_source_name (
          gtk_combo_box_get_model (GTK_COMBO_BOX (priv->edit_widget)),
          gtk_combo_box_get_active_id (GTK_COMBO_BOX (priv->edit_widget)));

      gtk_label_set_text (GTK_LABEL (priv->view_widget), name);
      g_free (name);
    }
}

static void
gcal_editable_combo_clear (GcalEditable  *editable)
{
  GcalEditableComboPrivate *priv;

  g_return_if_fail (GCAL_IS_EDITABLE_COMBO (editable));
  priv = GCAL_EDITABLE_COMBO (editable)->priv;

  gtk_label_set_text (GTK_LABEL (priv->view_widget), NULL);
  gtk_combo_box_set_active (GTK_COMBO_BOX (priv->edit_widget), -1);
}

/**
 * gcal_editable_combo_new:
 *
 * Init the combo in EDIT_MODE
 *
 * Since: 0.1
 * Returns: (transfer full):
 **/
GtkWidget*
gcal_editable_combo_new (void)
{
  GtkWidget *combo = g_object_new (GCAL_TYPE_EDITABLE_COMBO, NULL);
  gcal_editable_enter_edit_mode (GCAL_EDITABLE (combo));
  return combo;
}

/**
 * gcal_editable_combo_new_with_content:
 *
 * Init the combo in VIEW_MODE
 *
 * Since 0.2
 * Returns: (transfer full)
 */
GtkWidget*
gcal_editable_combo_new_with_content (const gchar* content)
{
  GtkWidget *combo;
  combo = g_object_new (GCAL_TYPE_EDITABLE_COMBO, NULL);
  gcal_editable_combo_set_content (GCAL_EDITABLE_COMBO (combo), content);
  gcal_editable_leave_edit_mode (GCAL_EDITABLE (combo));
  return combo;
}

void
gcal_editable_combo_set_model (GcalEditableCombo *combo,
                               GtkTreeModel      *model)
{
  GcalEditableComboPrivate *priv;
  GtkCellRenderer *renderer;

  g_return_if_fail (GCAL_IS_EDITABLE_COMBO (combo));
  priv = combo->priv;

  gtk_combo_box_set_model (GTK_COMBO_BOX (priv->edit_widget), model);
  gtk_combo_box_set_id_column (GTK_COMBO_BOX (priv->edit_widget), 0);
  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (priv->edit_widget),
                              renderer,
                              TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (priv->edit_widget),
                                  renderer,
                                  "text",
                                  1,
                                  NULL);
  gtk_combo_box_set_active (GTK_COMBO_BOX (priv->edit_widget), 0);
}

void
gcal_editable_combo_set_content (GcalEditableCombo *combo,
                                 const gchar       *content)
{
  GcalEditableComboPrivate *priv;

  g_return_if_fail (GCAL_IS_EDITABLE_COMBO (combo));
  priv = combo->priv;

  gtk_label_set_text (GTK_LABEL (priv->view_widget), content);
}
