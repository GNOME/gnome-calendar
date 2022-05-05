/* -*- mode: c; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gcal-overflow-bin-layout.h
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

#define GCAL_TYPE_OVERFLOW_BIN_LAYOUT (gcal_overflow_bin_layout_get_type ())

G_DECLARE_FINAL_TYPE (GcalOverflowBinLayout, gcal_overflow_bin_layout, GCAL, OVERFLOW_BIN_LAYOUT, GtkLayoutManager)

void gcal_overflow_bin_layout_set_request_mode (GcalOverflowBinLayout *self,
                                                GtkSizeRequestMode     request_mode);

G_END_DECLS
