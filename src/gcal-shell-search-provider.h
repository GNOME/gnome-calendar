/* gcal-shell-search-provider.h
 *
 * Copyright (C) 2015 Erick Pérez Castellanos <erick.red@gmail.com>
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

#ifndef GCAL_SHELL_SEARCH_PROVIDER_H
#define GCAL_SHELL_SEARCH_PROVIDER_H

#include <glib-object.h>
#include <gio/gio.h>

#include "gcal-context.h"

G_BEGIN_DECLS

#define GCAL_TYPE_SHELL_SEARCH_PROVIDER (gcal_shell_search_provider_get_type())

G_DECLARE_FINAL_TYPE (GcalShellSearchProvider, gcal_shell_search_provider, GCAL, SHELL_SEARCH_PROVIDER, GObject)

GcalShellSearchProvider* gcal_shell_search_provider_new           (GcalContext             *context);

gboolean                 gcal_shell_search_provider_dbus_export   (GcalShellSearchProvider *search_provider,
                                                                   GDBusConnection         *connection,
                                                                   const gchar             *object_path,
                                                                   GError                 **error);

void                     gcal_shell_search_provider_dbus_unexport (GcalShellSearchProvider *search_provider,
                                                                   GDBusConnection         *connection,
                                                                   const gchar             *object_path);

G_END_DECLS

#endif /* GCAL_SHELL_SEARCH_PROVIDER_H */
