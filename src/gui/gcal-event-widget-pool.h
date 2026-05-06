/*
 * gcal-event-widget-pool.h
 *
 * Copyright 2026 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#pragma once

#include <gtk/gtk.h>

#include "gcal-event.h"

G_BEGIN_DECLS

#define GCAL_TYPE_EVENT_WIDGET_POOL (gcal_event_widget_pool_get_type())
G_DECLARE_FINAL_TYPE (GcalEventWidgetPool, gcal_event_widget_pool, GCAL, EVENT_WIDGET_POOL, GObject)

GcalEventWidgetPool *gcal_event_widget_pool_new            (void);
GtkWidget           *gcal_event_widget_pool_take_or_create (GcalEventWidgetPool *self,
                                                            GcalEvent           *event);
void                 gcal_event_widget_pool_reclaim        (GcalEventWidgetPool *self,
                                                            GtkWidget           *event_widget);

G_END_DECLS
