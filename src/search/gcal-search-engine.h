/* gcal-search-engine.h
 *
 * Copyright 2019 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#include <gio/gio.h>

#include "gcal-types.h"

G_BEGIN_DECLS

#define GCAL_TYPE_SEARCH_ENGINE (gcal_search_engine_get_type())
G_DECLARE_FINAL_TYPE (GcalSearchEngine, gcal_search_engine, GCAL, SEARCH_ENGINE, GObject)

GcalSearchEngine*    gcal_search_engine_new                      (GcalContext        *context);

void                 gcal_search_engine_search                   (GcalSearchEngine   *self,
                                                                  const gchar        *search_query,
                                                                  GCancellable       *cancellable,
                                                                  GAsyncReadyCallback callback,
                                                                  gpointer            user_data);

GListModel*         gcal_search_engine_search_finish             (GcalSearchEngine   *self,
                                                                  GAsyncResult       *result,
                                                                  GError            **error);

G_END_DECLS
