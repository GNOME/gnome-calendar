/*
 * Copyright (C) 2021 Purism SPC
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Author: Alice Mikhaylenko <alice.mikhaylenko@puri.sm>
 */

/* Copied from libadwaita 1.8 and adapted for use in gnome-calendar. */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GCAL_TYPE_FADING_LABEL (gcal_fading_label_get_type())

G_DECLARE_FINAL_TYPE (GcalFadingLabel, gcal_fading_label, GCAL, FADING_LABEL, GtkWidget)

const gchar *gcal_fading_label_get_label (GcalFadingLabel *self);
void         gcal_fading_label_set_label (GcalFadingLabel *self,
                                          const gchar     *label);
gfloat       gcal_fading_label_get_align (GcalFadingLabel *self);
void         gcal_fading_label_set_align (GcalFadingLabel *self,
                                          gfloat           align);

G_END_DECLS
