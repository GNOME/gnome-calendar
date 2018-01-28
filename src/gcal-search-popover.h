/* gcal-search-popover.h
 *
 * Copyright (C) 2014 - Erick Pérez Castellanos
 *               2018 - Georges Basile Stavracas Neto
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "gcal-event-widget.h"
#include "gcal-manager.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GCAL_TYPE_SEARCH_POPOVER (gcal_search_popover_get_type ())

G_DECLARE_FINAL_TYPE (GcalSearchPopover, gcal_search_popover, GCAL, SEARCH_POPOVER, GtkPopover)

void                 gcal_search_popover_set_time_format         (GcalSearchPopover  *self,
                                                                  gboolean            format_24h);

void                 gcal_search_popover_search                  (GcalSearchPopover  *self,
                                                                  const gchar        *field,
                                                                  const gchar        *query);

void                 gcal_search_popover_connect                 (GcalSearchPopover  *self,
                                                                  GcalManager        *manager);


G_END_DECLS
