/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 2 -*- */
/*
 * gcal-event-view.c
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

#include "gcal-event-view.h"
#include "gcal-editable-entry.h"

#include <glib/gi18n.h>

struct _GcalEventViewPrivate
{
  gboolean   edit_mode;

  GtkWidget *e_what;
  GtkWidget *e_host;

  GtkWidget   *e_since;
  GtkWidget   *e_until;
  GtkWidget   *c_all_day;

  GtkWidget   *e_where;

  GtkWidget   *cb_cal;

  GtkWidget   *t_desc;

  GtkWidget   *attending_yes;
  GtkWidget   *attending_maybe;
  GtkWidget   *attending_no;

  GcalManager *manager;
};

static void     gcal_event_view_constructed          (GObject        *object);

static void     gcal_event_view_set_combo_box        (GcalEventView  *view);

G_DEFINE_TYPE(GcalEventView, gcal_event_view, GTK_TYPE_GRID)

static void
gcal_event_view_class_init(GcalEventViewClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->constructed = gcal_event_view_constructed;

  g_type_class_add_private((gpointer)klass, sizeof(GcalEventViewPrivate));
}

static void gcal_event_view_init(GcalEventView *self)
{
  GtkStyleContext *context;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self,
                                           GCAL_TYPE_EVENT_VIEW,
                                           GcalEventViewPrivate);

  context = gtk_widget_get_style_context (GTK_WIDGET (self));
  gtk_style_context_add_class (context, "event-view");
}

static void
gcal_event_view_constructed (GObject *object)
{
  GcalEventViewPrivate *priv;
  GtkWidget *what;
  GtkWidget *host;
  GtkWidget *when;
  GtkWidget *to;
  GtkWidget *calendar;
  GtkWidget *where;
  GtkWidget *desc;
  GtkWidget *desc_frame;
  GtkWidget *attending;
  GtkWidget *attending_box;

  priv = GCAL_EVENT_VIEW (object)->priv;
  if (G_OBJECT_CLASS (gcal_event_view_parent_class)->constructed != NULL)
    G_OBJECT_CLASS (gcal_event_view_parent_class)->constructed (object);

  priv->e_what = gcal_editable_entry_new ();
  gcal_editable_enter_edit_mode (GCAL_EDITABLE (priv->e_what));
  priv->e_host = gcal_editable_entry_new ();
  gcal_editable_enter_edit_mode (GCAL_EDITABLE (priv->e_host));

  priv->e_since = gtk_entry_new ();
  priv->e_until = gtk_entry_new ();
  priv->c_all_day = gtk_check_button_new_with_label (_("All day"));

  priv->e_where = gtk_entry_new ();

  priv->cb_cal = gtk_combo_box_new ();
  gtk_widget_set_halign (priv->cb_cal, GTK_ALIGN_START);

  priv->t_desc = gtk_text_view_new ();

  priv->attending_yes = gtk_toggle_button_new_with_label (_("Yes"));
  priv->attending_maybe = gtk_toggle_button_new_with_label (_("Maybe"));
  priv->attending_no = gtk_toggle_button_new_with_label (_("No"));

  attending_box = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_box_set_spacing (GTK_BOX (attending_box), 2);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (attending_box), GTK_BUTTONBOX_SPREAD);
  gtk_widget_set_halign (attending_box, GTK_ALIGN_START);
  gtk_container_add (GTK_CONTAINER (attending_box), priv->attending_yes);
  gtk_container_add (GTK_CONTAINER (attending_box), priv->attending_maybe);
  gtk_container_add (GTK_CONTAINER (attending_box), priv->attending_no);

  what = gtk_label_new (_("What"));
  gtk_widget_set_halign (what, GTK_ALIGN_END);
  gtk_grid_attach (GTK_GRID (object), what, 0, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (object), priv->e_what, 1, 0, 3, 1);

  host = gtk_label_new (_("Host"));
  gtk_widget_set_halign (host, GTK_ALIGN_END);
  gtk_grid_attach (GTK_GRID (object), host, 4, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (object), priv->e_host, 5, 0, 1, 1);

  when = gtk_label_new (_("When"));
  gtk_widget_set_halign (when, GTK_ALIGN_END);
  gtk_grid_attach (GTK_GRID (object), when, 0, 1, 1, 1);
  gtk_grid_attach (GTK_GRID (object), priv->e_since, 1, 1, 1, 1);
  to = gtk_label_new (_("to"));
  gtk_grid_attach (GTK_GRID (object), to, 2, 1, 1, 1);
  gtk_grid_attach (GTK_GRID (object), priv->e_until, 3, 1, 1, 1);
  gtk_grid_attach (GTK_GRID (object), priv->c_all_day, 1, 2, 1, 1);

  where = gtk_label_new (_("Where"));
  gtk_widget_set_halign (where, GTK_ALIGN_END);
  gtk_grid_attach (GTK_GRID (object), where, 0, 3, 1, 1);
  gtk_grid_attach (GTK_GRID (object), priv->e_where, 1, 3, 3, 1);

  calendar = gtk_label_new (_("Calendar"));
  gtk_widget_set_halign (calendar, GTK_ALIGN_END);
  gtk_grid_attach (GTK_GRID (object), calendar, 0, 4, 1, 1);
  gtk_grid_attach (GTK_GRID (object), priv->cb_cal, 1, 4, 3, 1);

  desc = gtk_label_new (_("Description"));
  gtk_widget_set_halign (desc, GTK_ALIGN_END);
  gtk_widget_set_valign (desc, GTK_ALIGN_START);
  gtk_grid_attach (GTK_GRID (object), desc, 0, 5, 1, 1);
  desc_frame = gtk_frame_new (NULL);
  gtk_container_add (GTK_CONTAINER (desc_frame), priv->t_desc);
  gtk_widget_set_size_request (desc_frame, -1, 200);
  gtk_grid_attach (GTK_GRID (object), desc_frame, 1, 5, 3, 1);

  attending = gtk_label_new (_("Attending"));
  gtk_widget_set_halign (attending, GTK_ALIGN_END);
  gtk_grid_attach (GTK_GRID (object), attending, 0, 6, 1, 1);
  gtk_grid_attach (GTK_GRID (object), attending_box, 1, 6, 3, 1);

  gtk_grid_set_row_spacing (GTK_GRID (object), 10);
  gtk_grid_set_column_spacing (GTK_GRID (object), 6);
  gtk_widget_show_all (GTK_WIDGET (object));
}

static void
gcal_event_view_set_combo_box (GcalEventView *view)
{
  GcalEventViewPrivate *priv;
  GtkCellRenderer *renderer;

  priv = view->priv;

  gtk_combo_box_set_model (
      GTK_COMBO_BOX (priv->cb_cal),
      GTK_TREE_MODEL (gcal_manager_get_sources_model (priv->manager)));
  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (priv->cb_cal), renderer, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (priv->cb_cal),
                                  renderer,
                                  "text",
                                  1,
                                  NULL);
  gtk_combo_box_set_active (GTK_COMBO_BOX (priv->cb_cal), 0);
}

GtkWidget*
gcal_event_view_new_with_manager (GcalManager *manager)
{
  GtkWidget *view;
  GcalEventViewPrivate *priv;

  view = g_object_new (GCAL_TYPE_EVENT_VIEW, NULL);
  priv = GCAL_EVENT_VIEW (view)->priv;
  priv->manager = manager;
  gcal_event_view_set_combo_box (GCAL_EVENT_VIEW (view));

  return view;
}
