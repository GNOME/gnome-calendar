/* gcal-weather-service.h
 *
 * Copyright (C) 2017 - Florian Brosch <flo.brosch@gmail.com>
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

#ifndef GCAL_WEATHER_SERVICE_H
#define GCAL_WEATHER_SERVICE_H

#include <libgweather/gweather.h>
#include <gtk/gtk.h>

#include "gcal-weather-info.h"

G_BEGIN_DECLS

#define GCAL_TYPE_WEATHER_SERVICE (gcal_weather_service_get_type())
G_DECLARE_FINAL_TYPE (GcalWeatherService, gcal_weather_service, GCAL, WEATHER_SERVICE, GObject)

GcalWeatherService*  gcal_weather_service_new                       (void);

GTimeZone*           gcal_weather_service_get_time_zone             (GcalWeatherService *self);

void                 gcal_weather_service_set_time_zone             (GcalWeatherService *self,
                                                                     GTimeZone          *value);

GWeatherLocation*   gcal_weather_service_get_location               (GcalWeatherService *self);

void                 gcal_weather_service_set_location              (GcalWeatherService *self,
                                                                     GWeatherLocation   *value);

GcalWeatherInfo*     gcal_weather_service_get_weather_info_for_date (GcalWeatherService *self,
                                                                     GDate              *date);

GPtrArray*           gcal_weather_service_get_weather_infos         (GcalWeatherService *self);

const gchar*         gcal_weather_service_get_attribution           (GcalWeatherService *self);

void                 gcal_weather_service_update                    (GcalWeatherService *self);

void                 gcal_weather_service_activate                  (GcalWeatherService *self);

void                 gcal_weather_service_deactivate                (GcalWeatherService *self);

void                 gcal_weather_service_start                     (GcalWeatherService *self);

void                 gcal_weather_service_stop                      (GcalWeatherService *self);

void                 gcal_weather_service_hold                      (GcalWeatherService *self);

void                 gcal_weather_service_release                   (GcalWeatherService *self);

G_END_DECLS

#endif /* GCAL_WEATHER_SERVICE_H */

