/*
 * Copyright (C) 2021 Purism SPC
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

/* Copied from libadwaita 1.8 and adapted for use in gnome-calendar. */

#pragma once

#include <glib.h>
#include <pango/pango.h>

G_BEGIN_DECLS

PangoDirection gcal_find_base_dir (const gchar *text,
                                   gint         length);

G_END_DECLS
