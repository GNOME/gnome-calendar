/* -*- mode: c; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gcal-overflow-bin-.h
 * Copyright (C) 2022 Purism SPC
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

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GCAL_TYPE_OVERFLOW_BIN (gcal_overflow_bin_get_type ())

G_DECLARE_DERIVABLE_TYPE (GcalOverflowBin, gcal_overflow_bin, GCAL, OVERFLOW_BIN, GtkWidget)

struct _GcalOverflowBinClass
{
  GtkWidgetClass parent_class;
};

GtkWidget *gcal_overflow_bin_new (void) G_GNUC_WARN_UNUSED_RESULT;

GtkWidget *gcal_overflow_bin_get_child (GcalOverflowBin *self);
void       gcal_overflow_bin_set_child (GcalOverflowBin *self,
                                        GtkWidget       *child);

void gcal_overflow_bin_set_request_mode (GcalOverflowBin    *self,
                                         GtkSizeRequestMode  request_mode);

G_END_DECLS
