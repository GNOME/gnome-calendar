/*
 * gcal-search-view.h
 *
 * Copyright (C) 2014 - Erick Pérez Castellanos
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
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

#ifndef __GCAL_SEARCH_VIEW_H__
#define __GCAL_SEARCH_VIEW_H__

#include "gcal-event-widget.h"
#include "gcal-manager.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GCAL_TYPE_SEARCH_VIEW                       (gcal_search_view_get_type ())

G_DECLARE_FINAL_TYPE (GcalSearchView, gcal_search_view, GCAL, SEARCH_VIEW, GtkPopover)

void           gcal_search_view_set_time_format  (GcalSearchView *view,
                                                  gboolean        format_24h);

void           gcal_search_view_search           (GcalSearchView *view,
                                                  const gchar    *field,
                                                  const gchar    *query);

GtkWidget*     gcal_search_view_new              (void);

void           gcal_search_view_connect          (GcalSearchView *search_view,
                                                  GcalManager    *manager);


G_END_DECLS

#endif /* __GCAL_SEARCH_VIEW_H__ */
