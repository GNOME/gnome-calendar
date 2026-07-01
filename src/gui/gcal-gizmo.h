/* gcal-gizmo.h
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

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GCAL_TYPE_GIZMO (gcal_gizmo_get_type ())

G_DECLARE_FINAL_TYPE (GcalGizmo, gcal_gizmo, GCAL, GIZMO, GtkWidget)

/**
 * GcalGizmoMeasureFunc:
 * @self: a #GcalGizmo
 * @orientation: the orientation to measure
 * @for_size: size for the opposite of @orientation, or -1 if unknown
 * @minimum: (out) (optional): location to store the minimum size
 * @natural: (out) (optional): location to store the natural size
 * @minimum_baseline: (out) (optional):
 *   location to store the baseline position for the minimum size, or -1 to report no baseline
 * @natural_baseline: (out) (optional):
 *   location to store the baseline position for the natural size, or -1 to report no baseline
 * @user_data: (nullable): user data
 *
 * Function wrapper for overriding [vfunc@Gtk.Widget.measure].
 */
typedef void (*GcalGizmoMeasureFunc) (GcalGizmo      *self,
                                      GtkOrientation  orientation,
                                      int             for_size,
                                      int            *minimum,
                                      int            *natural,
                                      int            *minimum_baseline,
                                      int            *natural_baseline,
                                      gpointer        user_data);

/**
 * GcalGizmoSizeAllocateFunc:
 * @self: a #GcalGizmo
 * @width: the width
 * @height: the height
 * @baseline: the baseline, or -1
 * @user_data: (nullable): user data
 *
 * Function wrapper for overriding [vfunc@Gtk.Widget.size_allocate].
 */
typedef void (*GcalGizmoSizeAllocateFunc) (GcalGizmo *self,
                                           int        width,
                                           int        height,
                                           int        baseline,
                                           gpointer   user_data);

/**
 * GcalGizmoFocusFunc:
 * @self: a #GcalGizmo
 * @direction: the direction
 * @user_data: (nullable): user data
 *
 * Function wrapper for overriding [vfunc@Gtk.Widget.focus].
 */
typedef gboolean (*GcalGizmoFocusFunc) (GcalGizmo        *self,
                                        GtkDirectionType  direction,
                                        gpointer          user_data);

/**
 * GcalGizmoSnapshotFunc:
 * @self: a #GcalGizmo
 * @snapshot: the snapshot
 * @user_data: (nullable): user data
 *
 * Function wrapper for overriding [vfunc@Gtk.Widget.snapshot].
 */
typedef void (*GcalGizmoSnapshotFunc) (GcalGizmo   *self,
                                       GtkSnapshot *snapshot,
                                       gpointer     user_data);

GcalGizmo *gcal_gizmo_new                    (void);
void       gcal_gizmo_set_measure_func       (GcalGizmo                 *self,
                                              GcalGizmoMeasureFunc       measure_func,
                                              gpointer                   user_data);
void       gcal_gizmo_set_size_allocate_func (GcalGizmo                 *self,
                                              GcalGizmoSizeAllocateFunc  size_allocate_func,
                                              gpointer                   user_data);
void       gcal_gizmo_set_snapshot_func      (GcalGizmo                 *self,
                                              GcalGizmoSnapshotFunc      snapshot_func,
                                              gpointer                   user_data);
void       gcal_gizmo_set_focus_func         (GcalGizmo                 *self,
                                              GcalGizmoFocusFunc         focus_func,
                                              gpointer                   user_data);

G_END_DECLS

