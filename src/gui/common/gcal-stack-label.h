/*
 * gcal-stack-label.h
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 The GNOME Calendar authors
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GCAL_TYPE_STACK_LABEL (gcal_stack_label_get_type())

G_DECLARE_FINAL_TYPE (GcalStackLabel, gcal_stack_label, GCAL, STACK_LABEL, GtkWidget)

GcalStackLabel *gcal_stack_label_new            (void);
const char     *gcal_stack_label_get_label      (GcalStackLabel     *self);
void            gcal_stack_label_set_label      (GcalStackLabel     *self,
                                                 const char         *label);
void            gcal_stack_label_compute_labels (GcalStackLabel     *self,
                                                 GtkStringList      *labels);

G_END_DECLS
