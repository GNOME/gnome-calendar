/* gcal-gizmo.c
 *
 * Copyright (C) 2020 Purism SPC
 * Copyright 2025 Hari Rana <theevilskeleton@riseup.net>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "config.h"

#include "gcal-gizmo.h"

/**
 * GcalGizmo:
 *
 * A very basic widget for setting virtual functions affecting layout.
 *
 * This is only meant to be used for custom widgets that need child widgets with custom
 * functions that contribute to layout.
 */

struct _GcalGizmo
{
  GtkWidget                  parent_instance;

  GcalGizmoMeasureFunc       measure_func;
  gpointer                   measure_user_data;

  GcalGizmoSizeAllocateFunc  size_allocate_func;
  gpointer                   size_allocate_user_data;

  GcalGizmoSnapshotFunc      snapshot_func;
  gpointer                   snapshot_user_data;

  GcalGizmoFocusFunc         focus_func;
  gpointer                   focus_user_data;
};

G_DEFINE_FINAL_TYPE (GcalGizmo, gcal_gizmo, GTK_TYPE_WIDGET)


/*
 * GtkWidget overrides
 */

static void
gcal_gizmo_real_measure (GtkWidget      *widget,
                         GtkOrientation  orientation,
                         int             for_size,
                         int            *minimum,
                         int            *natural,
                         int            *minimum_baseline,
                         int            *natural_baseline)
{
  GcalGizmo *self = (GcalGizmo *) widget;

  g_assert (GCAL_IS_GIZMO (self));

  if (self->measure_func)
    {
      self->measure_func (self,
                          orientation,
                          for_size,
                          minimum, natural,
                          minimum_baseline, natural_baseline,
                          self->measure_user_data);
    }
}

static void
gcal_gizmo_real_size_allocate (GtkWidget *widget,
                               int        width,
                               int        height,
                               int        baseline)
{
  GcalGizmo *self = (GcalGizmo *) widget;

  g_assert (GCAL_IS_GIZMO (self));

  if (self->size_allocate_func)
    self->size_allocate_func (self, width, height, baseline, self->size_allocate_user_data);
}

static void
gcal_gizmo_real_snapshot (GtkWidget   *widget,
                          GtkSnapshot *snapshot)
{
  GcalGizmo *self = (GcalGizmo *) widget;

  g_assert (GCAL_IS_GIZMO (self));

  if (self->snapshot_func)
    self->snapshot_func (self, snapshot, self->snapshot_user_data);
  else
    GTK_WIDGET_CLASS (gcal_gizmo_parent_class)->snapshot (widget, snapshot);
}

static gboolean
gcal_gizmo_real_focus (GtkWidget        *widget,
                       GtkDirectionType  direction)
{
  GcalGizmo *self = (GcalGizmo *) widget;

  g_assert (GCAL_IS_GIZMO (self));

  if (self->focus_func)
    return self->focus_func (self, direction, self->focus_user_data);

  return FALSE;
}


/*
 * GObject overrides
 */

static void
gcal_gizmo_dispose (GObject *object)
{
  GtkWidget *widget = GTK_WIDGET (object);
  GtkWidget *child;

  while ((child = gtk_widget_get_first_child (widget)))
    gtk_widget_unparent (child);

  G_OBJECT_CLASS (gcal_gizmo_parent_class)->dispose (object);
}


/*
 * Initialization
 */

static void
gcal_gizmo_class_init (GcalGizmoClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gcal_gizmo_dispose;

  widget_class->measure = gcal_gizmo_real_measure;
  widget_class->size_allocate = gcal_gizmo_real_size_allocate;
  widget_class->snapshot = gcal_gizmo_real_snapshot;
  widget_class->focus = gcal_gizmo_real_focus;
}

static void
gcal_gizmo_init (GcalGizmo *self)
{
}


/**
 * gcal_gizmo_new:
 *
 * Create a new #GcalGizmo.
 *
 * Returns: (transfer full): a newly created #GcalGizmo
 */
GcalGizmo *
gcal_gizmo_new (void)
{
  return g_object_new (GCAL_TYPE_GIZMO, NULL);
}

/**
 * gcal_gizmo_set_measure_func:
 * @self: a #GcalGizmo
 * @measure_func: (scope call): the function to set
 *
 * Set the function overriding [vfunc@Gtk.Widget.measure].
 */
void
gcal_gizmo_set_measure_func (GcalGizmo            *self,
                             GcalGizmoMeasureFunc  measure_func,
                             gpointer              user_data)
{
  g_assert (GCAL_IS_GIZMO (self));

  self->measure_func = measure_func;
  self->measure_user_data = user_data;

  gtk_widget_queue_resize (GTK_WIDGET (self));
}

/**
 * gcal_gizmo_set_size_allocate_func:
 * @self: a #GcalGizmo
 * @size_allocate_func: (scope call): the function to set
 *
 * Set the function overriding [vfunc@Gtk.Widget.size_allocate].
 */
void
gcal_gizmo_set_size_allocate_func (GcalGizmo                 *self,
                                   GcalGizmoSizeAllocateFunc  size_allocate_func,
                                   gpointer                   user_data)
{
  g_assert (GCAL_IS_GIZMO (self));

  self->size_allocate_func = size_allocate_func;
  self->size_allocate_user_data = user_data;

  gtk_widget_queue_allocate (GTK_WIDGET (self));
}

/**
 * gcal_gizmo_set_snapshot_func:
 * @self: a #GcalGizmo
 * @snapshot_func: (scope call): the function to set
 *
 * Set the function overriding [vfunc@Gtk.Widget.snapshot].
 */
void
gcal_gizmo_set_snapshot_func (GcalGizmo             *self,
                              GcalGizmoSnapshotFunc  snapshot_func,
                              gpointer               user_data)
{
  g_assert (GCAL_IS_GIZMO (self));

  self->snapshot_func = snapshot_func;
  self->snapshot_user_data = user_data;

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

/**
 * gcal_gizmo_set_focus_func:
 * @self: a #GcalGizmo
 * @focus_func: (scope call): the function to set
 *
 * Set the function overriding [vfunc@Gtk.Widget.focus].
 */
void
gcal_gizmo_set_focus_func (GcalGizmo          *self,
                           GcalGizmoFocusFunc  focus_func,
                           gpointer            user_data)
{
  g_assert (GCAL_IS_GIZMO (self));

  self->focus_func = focus_func;
  self->focus_user_data = user_data;
}

