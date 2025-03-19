/*
 * gcal-gui-utils.c
 *
 * Copyright 2025 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "gcal-gui-utils.h"

#include <adwaita.h>

GtkWidget*
gcal_create_selection_widget (void)
{
  return g_object_new (ADW_TYPE_BIN,
                       "css-name", "selection",
                       NULL);
}

GtkWidget*
gcal_create_drop_target_widget (void)
{
  return g_object_new (ADW_TYPE_BIN,
                       "css-name", "droptarget",
                       NULL);
}
