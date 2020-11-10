/* gcal-expandable-entry.h
 *
 * Copyright (C) 2017 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GCAL_TYPE_EXPANDABLE_ENTRY (gcal_expandable_entry_get_type())
G_DECLARE_FINAL_TYPE (GcalExpandableEntry, gcal_expandable_entry, GCAL, EXPANDABLE_ENTRY, GtkEntry)

GtkWidget*           gcal_expandable_entry_new                   (void);

gboolean             gcal_expandable_entry_get_propagate_natural_width (GcalExpandableEntry *self);

void                 gcal_expandable_entry_set_propagate_natural_width (GcalExpandableEntry *self,
                                                                        gboolean            propagate_natural_width);

G_END_DECLS
