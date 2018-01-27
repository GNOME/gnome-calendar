/* gcal-weather-info.h
 *
 * Copyright (C) 2017 Florian Brosch <flo.brosch@gmail.com>
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

#ifndef __GCAL_WEATHER_INFO_H__
#define __GCAL_WEATHER_INFO_H__

#include <glib-object.h>
#include <glib.h>


G_BEGIN_DECLS

#define GCAL_TYPE_WEATHER_INFO (gcal_weather_info_get_type())

G_DECLARE_FINAL_TYPE (GcalWeatherInfo, gcal_weather_info, GCAL, WEATHER_INFO, GObject)


GcalWeatherInfo*     gcal_weather_info_new                       (GDate              *date,
                                                                  const gchar        *icon_name,
                                                                  const gchar        *temperature);

void                 gcal_weather_info_get_date                  (GcalWeatherInfo    *self,
                                                                  GDate              *date);

void                 gcal_weather_info_get_date                  (GcalWeatherInfo    *self,
                                                                  GDate              *date);

const gchar*         gcal_weather_info_get_icon_name             (GcalWeatherInfo    *self);

const gchar*         gcal_weather_info_get_temperature           (GcalWeatherInfo    *self);


G_END_DECLS

#endif /* __GCAL_WEATHER_INFO_H__ */


