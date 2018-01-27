/* gcal-weather-info.c
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

#define G_LOG_DOMAIN "Weather"

#include <string.h>
#include "gcal-weather-info.h"


/* _GcalWeatherInfo:
 * @date:        (not nullable): The day this information belongs to.
 * @name:        (not nullable): Icon name if weather informations are available or NULL.
 * @temperature: (not nullable): Temperature string including right units.
 *
 * Data type holding weather information.
 *
 * Geometric information and @image should only be set
 * inside widgets.
 */
struct _GcalWeatherInfo {
  GObject           parent_instance;

  /* <private> */
  GDate             date;        /* owned, non-null */
  gchar            *icon_name;   /* owned, non-null */
  gchar            *temperature; /* owned, non-null */
};


static void          gcal_weather_info_set_date                  (GcalWeatherInfo    *self,
                                                                  GDate              *date);

static void          gcal_weather_info_set_temperature           (GcalWeatherInfo    *self,
                                                                  const gchar        *temperature);

static void          gcal_weather_info_set_icon_name             (GcalWeatherInfo    *self,
                                                                  const gchar        *icon_name);

G_DEFINE_TYPE (GcalWeatherInfo, gcal_weather_info, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_DATE,
  PROP_ICON_NAME,
  PROP_TEMPERATURE,
  PROP_NUM,
};



/* < gobject setup > */

static void
gcal_weather_info_finalize (GObject *object)
{
  GcalWeatherInfo *self; /* unowned */

  self = (GcalWeatherInfo *) object;

  g_date_clear (&self->date, 1);
  g_free (self->icon_name);
  g_free (self->temperature);

  G_OBJECT_CLASS (gcal_weather_info_parent_class)->finalize (object);
}

static void
gcal_weather_info_get_property (GObject    *object,
                                guint       property_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GcalWeatherInfo *self; /* unowned */

  self = GCAL_WEATHER_INFO (object);
  switch (property_id)
    {
    case PROP_DATE:
      g_value_set_boxed (value, &self->date);
      break;
    case PROP_ICON_NAME:
      g_value_set_string (value, gcal_weather_info_get_icon_name (self));
      break;
    case PROP_TEMPERATURE:
      g_value_set_string (value, gcal_weather_info_get_temperature (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gcal_weather_info_set_property (GObject      *object,
                                guint         property_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GcalWeatherInfo *self; /* unowned */

  self = GCAL_WEATHER_INFO (object);
  switch (property_id)
    {
    case PROP_DATE:
      gcal_weather_info_set_date (self, g_value_get_boxed (value));
      break;

    case PROP_ICON_NAME:
      gcal_weather_info_set_icon_name (self, g_value_get_string (value));
      break;

    case PROP_TEMPERATURE:
      gcal_weather_info_set_temperature (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gcal_weather_info_class_init (GcalWeatherInfoClass *klass)
{
  GObjectClass *object_class; /* unowned */

  object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = gcal_weather_info_finalize;
  object_class->get_property = gcal_weather_info_get_property;
  object_class->set_property = gcal_weather_info_set_property;


  /**
   * GcalWeatherInfo:date:
   *
   * The non-nullable date weather information belongs to.
   */
  g_object_class_install_property
      (object_class,
       PROP_DATE,
       g_param_spec_boxed ("date", "date", "date", G_TYPE_DATE,
                           G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  /**
   * GcalWeatherInfo:icon-name:
   *
   * Non-nullable Icon name representing the weather.
   */
  g_object_class_install_property
      (object_class,
       PROP_ICON_NAME,
       g_param_spec_string ("icon-name", "icon-name", "icon-name", NULL,
                            G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  /**
   * GcalWeatherInfo:temperature:
   *
   * The temperature as string or %NULL.
   */
  g_object_class_install_property
      (object_class,
       PROP_TEMPERATURE,
       g_param_spec_string ("temperature", "temperature", "temperature", NULL,
                            G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static void
gcal_weather_info_init (GcalWeatherInfo *self)
{
  g_date_clear (&self->date, 1);
  self->icon_name = NULL;
  self->temperature = NULL;
}



/* < private > */

/* gcal_weather_info_set_date:
 * @self: A #GcalWeatherInfo instance.
 * @date: The date to set.
 *
 * Setter for :date.
 */
static void
gcal_weather_info_set_date (GcalWeatherInfo *self,
                            GDate           *date)
{
  g_return_if_fail (GCAL_IS_WEATHER_INFO (self));
  g_return_if_fail (date != NULL);

  self->date = *date;
}

/* gcal_weather_info_set_temperature:
 * @self:        A #GcalWeatherInfo instance.
 * @temperature: A weather string.
 *
 * Setter for #GcalWeatherInfo:temperature.
 */
static void
gcal_weather_info_set_temperature (GcalWeatherInfo *self,
                                   const gchar     *temperature)
{
  g_return_if_fail (GCAL_IS_WEATHER_INFO (self));
  g_return_if_fail (temperature != NULL);

  if (g_strcmp0 (self->temperature, temperature) != 0)
    {
      g_free (self->temperature);
      self->temperature = g_strdup (temperature);
      g_object_notify ((GObject*) self, "temperature");
    }
}

/* gcal_weather_info_set_icon_name:
 * @self:      A #GcalWeatherInfo instance.
 * @icon_name: The name of the icon to display.
 *
 * Setter for #GcalWeatherInfo:icon_name.
 */
static void
gcal_weather_info_set_icon_name (GcalWeatherInfo *self,
                                 const gchar     *icon_name)
{
  g_return_if_fail (GCAL_IS_WEATHER_INFO (self));
  g_return_if_fail (icon_name != NULL);

  if (g_strcmp0 (self->icon_name, icon_name) != 0)
    {
      g_free (self->icon_name);
      self->icon_name = g_strdup (icon_name);
      g_object_notify ((GObject*) self, "icon-name");
    }
}



/* < public > */

/**
 * gcal_weather_info_new:
 * @date: (not nullable):      The date weather infos belong to.
 * @icon_name: (not nullable): The icon name indicating the weather.
 * @temperature: (nullable):   The temperature to display.
 *
 * Creates a new #GcalWeatherInfo.
 *
 * @Returns: (transfer full): A newly allocated #GcalWeatherInfo.
 */
GcalWeatherInfo*
gcal_weather_info_new (GDate       *date,
                       const gchar *icon_name,
                       const gchar *temperature)
{
  GcalWeatherInfo *info; /* owned */

  g_return_val_if_fail (date != NULL, NULL);
  g_return_val_if_fail (icon_name != NULL, NULL);
  g_return_val_if_fail (temperature != NULL, NULL);

  info  = (GcalWeatherInfo*) g_object_new (GCAL_TYPE_WEATHER_INFO,
                                           "date", date,
                                           "icon-name", icon_name,
                                           "temperature", temperature,
                                           NULL);
  return info;
}

/**
 * gcal_weather_info_get_date:
 * @self: A #GcalWeatherInfo instance.
 * @date: (out) (not nullable): The associated day.
 *
 * Getter for #GcalWeatherInfo:date.
 */
void
gcal_weather_info_get_date (GcalWeatherInfo *self,
                            GDate           *date)
{
  g_return_if_fail (GCAL_IS_WEATHER_INFO (self));

  *date = self->date;
}

/**
 * gcal_weather_info_get_icon_name:
 * @self: A #GcalWeatherInfo instance.
 *
 * Getter for #GcalWeatherInfo:icon_name.
 */
const gchar*
gcal_weather_info_get_icon_name (GcalWeatherInfo *self)
{
  g_return_val_if_fail (GCAL_IS_WEATHER_INFO (self), NULL);

  return self->icon_name;
}

/**
 * gcal_weather_info_get_temperature:
 * @self: A #GcalWeatherInfo instance.
 *
 * Getter for #GcalWeatherInfo:temperature.
 */
const gchar*
gcal_weather_info_get_temperature (GcalWeatherInfo *self)
{
  g_return_val_if_fail (GCAL_IS_WEATHER_INFO (self), NULL);

  return self->temperature;
}
