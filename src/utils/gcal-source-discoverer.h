/* gcal-source-discoverer.h
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

G_BEGIN_DECLS

/**
 * GcalDiscovererError:
 * @GCAL_SOURCE_DISCOVERER_ERROR_UNAUTHORIZED: indicated an invalid start date
 *
 * Errors that can generate.
 */
typedef enum
{
  GCAL_SOURCE_DISCOVERER_ERROR_UNAUTHORIZED,
} GcalSourceDiscovererError;

#define GCAL_SOURCE_DISCOVERER_ERROR (gcal_source_discoverer_error_quark ())

GQuark               gcal_source_discoverer_error_quark          (void);

void                 gcal_discover_sources_from_uri              (const gchar         *uri,
                                                                  const gchar         *username,
                                                                  const gchar         *password,
                                                                  GCancellable        *cancellable,
                                                                  GAsyncReadyCallback  callback,
                                                                  gpointer             user_data);

GPtrArray*           gcal_discover_sources_from_uri_finish       (GAsyncResult       *result,
                                                                  GError            **error);

G_END_DECLS
