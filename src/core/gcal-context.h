/* gcal-context.h
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

#include "gcal-clock.h"
#include "gcal-enums.h"
#include "gcal-manager.h"
#include "gcal-search-engine.h"
#include "weather/gcal-weather-service.h"

#include <glib-object.h>

G_BEGIN_DECLS

#define GCAL_TYPE_CONTEXT (gcal_context_get_type())
G_DECLARE_FINAL_TYPE (GcalContext, gcal_context, GCAL, CONTEXT, GObject)

GcalContext*         gcal_context_new                            (void);

GcalClock*           gcal_context_get_clock                      (GcalContext        *self);

GcalManager*         gcal_context_get_manager                    (GcalContext        *self);

GcalSearchEngine*    gcal_context_get_search_engine              (GcalContext        *self);

GSettings*           gcal_context_get_settings                   (GcalContext        *self);

GcalTimeFormat       gcal_context_get_time_format                (GcalContext        *self);

GTimeZone*           gcal_context_get_timezone                   (GcalContext        *self);

GcalWeatherService*  gcal_context_get_weather_service            (GcalContext        *self);

void                 gcal_context_startup                        (GcalContext        *self);

G_END_DECLS
