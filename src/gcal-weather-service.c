/* gcal-weather-service.c
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

#define DESKTOP_FILE_NAME  "org.gnome.Calendar"

#include <geocode-glib/geocode-glib.h>
#include <geoclue.h>

#include "gcal-weather-service.h"


G_BEGIN_DECLS

/* GcalWeatherService:
 *
 * @location_service:     Used to monitor location changes.
 *                        Initialized by gcal_weather_service_run(),
 *                        freed by gcal_weather_service_stop().
 * @location_cancellable: Used to deal with async location service construction.
 * @weather_info:         The weather info to query.
 *
 * This service listens to location and weather changes and reports them.
 *
 *  * Create a new instance with gcal_weather_service_new().
 *  * Connect to ::changed to catch weather information.
 *  * Use gcal_weather_service_start() to start the service.
 *  * Use gcal_weather_service_stop() when you are done.
 *
 *  Make sure to stop this service before destroying it.
 */
struct _GcalWeatherService
{
  GObjectClass parent;

  /* <public> */

  /* <private> */
  /* timer: */
  guint           check_interval;
  guint           timeout_id;

  /* locations: */
  GClueSimple    *location_service;     /* owned, nullable */
  GCancellable   *location_cancellable; /* owned, non-null */
  gboolean        location_service_running;

  /* weather: */
  GWeatherInfo   *weather_info;         /* owned, nullable */
  guint           max_days;
  gboolean        weather_service_running;
};



enum
{
  PROP_0,
  PROP_MAX_DAYS,
  PROP_CHECK_INTERVAL,
  PROP_NUM,
};

enum
{
  SIG_WEATHER_CHANGED,
  SIG_NUM,
};

static guint gcal_weather_service_signals[SIG_NUM] = { 0 };


G_DEFINE_TYPE (GcalWeatherService, gcal_weather_service, G_TYPE_OBJECT)


/* Internal location API and callbacks: */
static void     gcal_weather_service_update_location       (GcalWeatherService  *self,
                                                            GWeatherLocation    *location);

static void     gcal_weather_service_update_gclue_location (GcalWeatherService  *self,
                                                            GClueLocation       *location);

static char*    gcal_weather_service_get_location_name     (GClueLocation       *location);

static void     on_gclue_simple_creation                   (GClueSimple         *source,
                                                            GAsyncResult        *result,
                                                            GcalWeatherService  *data);

static void     on_gclue_location_changed                  (GClueLocation       *location,
                                                            GcalWeatherService  *self);

static void     on_gclue_client_activity_changed           (GClueClient         *client,
                                                            GcalWeatherService  *self);

static void     on_gclue_client_stop                       (GClueClient         *client,
                                                            GAsyncResult        *res,
                                                            GClueSimple         *simple);

/* Internal Wweather API */
static void     gcal_weather_service_set_max_days          (GcalWeatherService  *self,
                                                            guint                days);

static void     gcal_weather_service_update_weather        (GWeatherInfo       *info,
                                                            GcalWeatherService *self);

/* Internal weather update timer API and callbacks */
static void     gcal_weather_service_set_check_interval    (GcalWeatherService   *self,
                                                            guint                 check_interval);

static void     gcal_weather_service_timer_stop            (GcalWeatherService *self);

static void     gcal_weather_service_timer_start           (GcalWeatherService *self);

static gboolean on_timer_timeout                           (GcalWeatherService *self);


G_END_DECLS


/********************
 * < gobject setup >
 *******************/

static void
gcal_weather_service_finalize (GObject *object)
{
  GcalWeatherService *self; /* unowned */

  self = (GcalWeatherService *) object;

  if (self->location_service != NULL)
    g_clear_object (&self->location_service);

  g_cancellable_cancel (self->location_cancellable);
  g_clear_object (&self->location_cancellable);

  if (self->weather_info != NULL)
    g_clear_object (&self->weather_info);

  G_OBJECT_CLASS (gcal_weather_service_parent_class)->finalize (object);
}



static void
gcal_weather_service_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GcalWeatherService* self; /* unowned */

  self = GCAL_WEATHER_SERVICE (object);
  switch (prop_id)
  {
  case PROP_MAX_DAYS:
    g_value_set_uint (value, gcal_weather_service_get_max_days (self));
    break;
  case PROP_CHECK_INTERVAL:
    g_value_set_uint (value, gcal_weather_service_get_check_interval (self));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}



static void
gcal_weather_service_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GcalWeatherService* self; /* unowned */

  self = GCAL_WEATHER_SERVICE (object);
  switch (prop_id)
  {
  case PROP_MAX_DAYS:
    gcal_weather_service_set_max_days (self, g_value_get_uint (value));
    break;
  case PROP_CHECK_INTERVAL:
    gcal_weather_service_set_check_interval (self, g_value_get_uint (value));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}



static void
gcal_weather_service_class_init (GcalWeatherServiceClass *klass)
{
  GObjectClass *object_class; /* unowned */

  object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = gcal_weather_service_finalize;
  object_class->get_property = gcal_weather_service_get_property;
  object_class->set_property = gcal_weather_service_set_property;

  /**
   * GcalWeatherServiceClass:max-days:
   *
   * Maximal number of days to fetch forecasts for.
   */
  g_object_class_install_property
      (G_OBJECT_CLASS (klass),
       PROP_MAX_DAYS,
       g_param_spec_uint ("max-days", "max-days", "max-days",
                          1, G_MAXUINT, 3,
                          G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  /**
   * GcalWeatherServiceClass:check-interval:
   *
   * Amount of seconds to wait before re-fetching weather infos.
   * Use %0 to disable timers.
   */
  g_object_class_install_property
      (G_OBJECT_CLASS (klass),
       PROP_CHECK_INTERVAL,
       g_param_spec_uint ("check-interval", "check-interval", "check-interval",
                          0, G_MAXUINT, 0,
                          G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  /**
   * GcalWeatherService::weather-changed:
   * @sender: The #GcalWeatherService
   * @gwi:    A SList containing updated #GcalWeatherInfos.
   * @self:   data pointer.
   *
   * Triggered on weather changes.
   */
  gcal_weather_service_signals[SIG_WEATHER_CHANGED]
    = g_signal_new ("weather-changed",
                    GCAL_TYPE_WEATHER_SERVICE,
                    G_SIGNAL_RUN_LAST,
                    0,
                    NULL,
                    NULL,
                    g_cclosure_marshal_VOID__POINTER,
                    G_TYPE_NONE,
                    1,
                    G_TYPE_POINTER);
}



static void
gcal_weather_service_init (GcalWeatherService *self)
{
  self->check_interval = 0;
  self->timeout_id = 0;
  self->location_cancellable = g_cancellable_new ();
  self->location_service_running = FALSE;
  self->location_service = NULL;
  self->weather_info = NULL;
  self->weather_service_running = FALSE;
  self->max_days = 0;
}



/**************
 * < private >
 **************/

/**
 * gcal_weather_service_update_location:
 * @self:     The #GcalWeatherService instance.
 * @location: (nullable): The location we want weather information for.
 *
 * Registers the location to retrieve weather information from.
 */
static void
gcal_weather_service_update_location (GcalWeatherService  *self,
                                      GWeatherLocation    *location)
{
  g_return_if_fail (GCAL_IS_WEATHER_SERVICE (self));

  if (self->weather_info != NULL)
    {
      gcal_weather_service_timer_stop (self);
      g_clear_object (&self->weather_info);
    }

  if (location == NULL)
    {
      g_info ("Could not retrieve current location.");
      gcal_weather_service_update_weather (NULL, self);
    }
  else
    {
      self->weather_info = gweather_info_new (location, GWEATHER_FORECAST_ZONE);

      gweather_info_set_enabled_providers (self->weather_info, GWEATHER_PROVIDER_ALL);
      g_signal_connect (self->weather_info, "updated", (GCallback) gcal_weather_service_update_weather, self);
      gweather_info_update (self->weather_info);

      gcal_weather_service_timer_start (self);
    }
}



/**
 * gcal_weather_service_update_gclue_location:
 * @self:     The #GcalWeatherService instance.
 * @location: (nullable): The location we want weather information for.
 *
 * Registers the location to retrieve weather information from.
 */
static void
gcal_weather_service_update_gclue_location (GcalWeatherService  *self,
                                            GClueLocation       *location)
{
  GWeatherLocation *wlocation = NULL; /* owned */

  g_return_if_fail (GCAL_IS_WEATHER_SERVICE (self));
  g_return_if_fail (location == NULL || GCLUE_IS_LOCATION (location));

  if (location != NULL)
    {
      g_autofree gchar *loc_name = NULL;

      loc_name = gcal_weather_service_get_location_name (location);
      if (loc_name != NULL)
        {
          gdouble latitude;
          gdouble longitude;

          latitude = gclue_location_get_latitude (location);
          longitude = gclue_location_get_longitude (location);

          wlocation = gweather_location_new_detached (loc_name, NULL, latitude, longitude);
        }
    }


  gcal_weather_service_update_location (self, wlocation);

  if (wlocation != NULL)
    gweather_location_unref (wlocation);
}



/* gcal_weather_service_get_location_name:
 * @location: A #GClueLocation to get the city name from.
 *
 * Queries the city name for the given location or %NULL.
 *
 * @Returns: (transfer full) (nullable): City name or %NULL.
 */
static char*
gcal_weather_service_get_location_name (GClueLocation *location)
{
  g_autoptr (GeocodeLocation) glocation = NULL;
  g_autoptr (GeocodeReverse) greverse = NULL;
  g_autoptr (GeocodePlace) gplace = NULL;

  g_return_val_if_fail (GCLUE_IS_LOCATION (location), NULL);

  glocation = geocode_location_new (gclue_location_get_latitude (location),
                                    gclue_location_get_longitude (location),
                                    gclue_location_get_accuracy (location));

  greverse = geocode_reverse_new_for_location (glocation);

  gplace = geocode_reverse_resolve (greverse, NULL);
  if (gplace == NULL)
    return NULL;

  return g_strdup (geocode_place_get_town (gplace));
}



/* on_gclue_simple_creation:
 * @source:
 * @result:                Result of gclue_simple_new().
 * @self: (transfer full): A GcalWeatherService reference.
 *
 * Callback used in gcal_weather_service_run().
 */
static void
on_gclue_simple_creation (GClueSimple        *_source,
                          GAsyncResult       *result,
                          GcalWeatherService *self)
{
  GClueSimple *location_service;   /* owned */
  GClueLocation *location;         /* unowned */
  GClueClient *client;             /* unowned */
  g_autoptr (GError) error = NULL;

  g_return_if_fail (G_IS_ASYNC_RESULT (result));
  g_return_if_fail (GCAL_IS_WEATHER_SERVICE (self));

  /* make sure we do not touch self->location_service
   * if the current operation was cancelled.
   */
  location_service = gclue_simple_new_finish (result, &error);
  if (error != NULL)
    {
      g_assert (location_service == NULL);

      if (error->domain == G_IO_ERROR && error->code == G_IO_ERROR_CANCELLED)
        /* Cancelled during creation. Silently fail. */;
      else
        g_warning ("Could not create GCLueSimple: %s", error->message);

      g_object_unref (self);
      return;
    }

  g_assert (self->location_service == NULL);
  g_assert (location_service != NULL);

  self->location_service = g_steal_pointer (&location_service);

  location = gclue_simple_get_location (self->location_service);
  client = gclue_simple_get_client (self->location_service);

  if (location != NULL)
    {
      gcal_weather_service_update_gclue_location (self, location);

      g_signal_connect_object (location,
                               "notify::location",
                               G_CALLBACK (on_gclue_location_changed),
                               self,
                               0);
    }

  g_signal_connect_object (client,
                           "notify::active",
                           G_CALLBACK (on_gclue_client_activity_changed),
                           self,
                           0);

  g_object_unref (self);
}


/* on_gclue_location_changed:
 * @location: #GClueLocation owned by @self
 * @self: The #GcalWeatherService
 *
 * Handles location changes.
 */
static void
on_gclue_location_changed (GClueLocation       *location,
                           GcalWeatherService  *self)
{
  g_return_if_fail (GCLUE_IS_LOCATION (location));
  g_return_if_fail (GCAL_IS_WEATHER_SERVICE (self));

  gcal_weather_service_update_gclue_location (self, location);
}



/* on_gclue_client_activity_changed:
 * @client: The #GClueclient ownd by @self
 * @self: The #GcalWeatherService
 *
 * Handles location client activity changes.
 */
static void
on_gclue_client_activity_changed (GClueClient         *client,
                                  GcalWeatherService  *self)
{
  g_return_if_fail (GCLUE_IS_CLIENT (client));
  g_return_if_fail (GCAL_IS_WEATHER_SERVICE (self));

  /* Notify listeners about unknown locations: */
  gcal_weather_service_update_location (self, NULL);
}



/* on_gclue_client_stop:
 * @source_object: A #GClueClient.
 * @res:           Result of gclue_client_call_stop().
 * @simple:        (transfer full): A #GClueSimple.
 *
 * Helper-callback used in gcal_weather_service_stop().
 */
static void
on_gclue_client_stop (GClueClient  *client,
                      GAsyncResult *res,
                      GClueSimple  *simple)
{
  g_autoptr(GError) error = NULL; /* owned */
  gboolean stopped;

  g_return_if_fail (GCLUE_IS_CLIENT (client));
  g_return_if_fail (G_IS_ASYNC_RESULT (res));
  g_return_if_fail (GCLUE_IS_SIMPLE (simple));

  stopped = gclue_client_call_stop_finish (client,
                                           res,
                                           &error);
  if (error != NULL)
      g_warning ("Could not stop location service: %s", error->message);
  else if (!stopped)
      g_warning ("Could not stop location service");

  g_object_unref (simple);
}



/* gcal_weather_service_set_max_days:
 * @self: The #GcalWeatherService instance.
 * @days: Number of days.
 *
 * Setter for #GcalWeatherInfos:max-days.
 */
static void
gcal_weather_service_set_max_days (GcalWeatherService *self,
                                   guint               days)
{
  g_return_if_fail (GCAL_IS_WEATHER_SERVICE (self));

  self->max_days = days;

  g_object_notify ((GObject*) self, "max-days");
}



/* gcal_weather_service_update_weather:
 * @info: (nullable): Newly received weather information or %NULL.
 * @self: A #GcalWeatherService instance.
 *
 * Retrieves weather information for @location and triggers
 * #GcalWeatherService::weather-changed.
 */
static void
gcal_weather_service_update_weather (GWeatherInfo       *info,
                                     GcalWeatherService *self)
{
  GSList* infos = NULL;

  g_return_if_fail (GCAL_IS_WEATHER_SERVICE (self));

  if (info == NULL)
    {
      g_info ("Could not retrieve valid weather");
    }
  else if (gweather_info_is_valid (info))
    {
      // TODO
    }
  else
    {
      g_autofree gchar* location_name = gweather_info_get_location_name (info);
      g_info ("Could not retrieve valid weather for location '%s'", location_name);
    }

  g_signal_emit (self, gcal_weather_service_signals[SIG_WEATHER_CHANGED], 0, infos);
}



/* gcal_weather_service_set_max_days:
 * @self: The #GcalWeatherService instance.
 * @days: Number of days.
 *
 * Setter for GcalWeatherInfos:max-days.
 */
static void
gcal_weather_service_set_check_interval (GcalWeatherService *self,
                                         guint               interval)
{
  g_return_if_fail (GCAL_IS_WEATHER_SERVICE (self));

  self->check_interval = interval;

  g_object_notify ((GObject*) self, "check-interval");
}



/* gcal_weather_service_timer_stop:
 * @self: The #GcalWeatherService instance.
 *
 * Stops the internal timer. Does nothing if
 * timer is not running.
 *
 * This timer is used to update weather information
 * every :check-interval seconds
 */
static void
gcal_weather_service_timer_stop (GcalWeatherService *self)
{
  g_return_if_fail (GCAL_IS_WEATHER_SERVICE (self));

  if (self->timeout_id > 0)
    {
      g_source_remove (self->timeout_id);
      self->timeout_id = 0;
    }
}



/* gcal_weather_service_timer_start
 * @self: The #GcalWeatherService instance.
 *
 * Starts the internal timer. Does nothing if
 * timer is already running.
 *
 * Timer is used to update weather information
 * every :check-interval seconds.
 */
static void
gcal_weather_service_timer_start (GcalWeatherService *self)
{
  g_return_if_fail (GCAL_IS_WEATHER_SERVICE (self));

  if (self->check_interval == 0)
    /* timer is disabled */
    return;

  if (self->timeout_id > 0)
    /* timer is already running */
    return;

  self->timeout_id = g_timeout_add_seconds (self->check_interval,
                                            (GSourceFunc) on_timer_timeout,
                                            self);
}



/* on_timer_timeout
 * @self: A #GcalWeatherService.
 *
 * Handles scheduled weather report updates.
 *
 * Returns: %G_SOURCE_CONTINUE
 */
static gboolean
on_timer_timeout (GcalWeatherService *self)
{
  g_return_val_if_fail (GCAL_IS_WEATHER_SERVICE (self), G_SOURCE_REMOVE);

  if (self->weather_info != NULL)
    gweather_info_update (self->weather_info);

  return G_SOURCE_CONTINUE;
}



/*************
 * < public >
 *************/

/**
 * gcal_weather_service_new:
 * @max_days:       mumber of days to fetch forecasts for.
 * @check_interval: seconds between checks for new weather information or %0 to disable checks.
 *
 * Creates a new #GcalWeatherService. This service listens
 * to location and weather changes and reports them.
 *
 * Returns: (transfer full): A newly created #GcalWeatherService.
 */
GcalWeatherService *
gcal_weather_service_new (guint max_days,
                          guint check_interval)
{
  return g_object_new (GCAL_TYPE_WEATHER_SERVICE,
                       "max-days",
                       max_days,
                       "check-interval",
                       check_interval,
                       NULL);
}



/**
 * gcal_weather_service_run:
 * @self: The #GcalWeatherService instance.
 * @location: (nullable): A fixed location or %NULL to use Gclue.
 *
 * Starts to monitor location and weather changes.
 * Use ::weather-changed to catch responses.
 */
void
gcal_weather_service_run (GcalWeatherService *self,
                          GWeatherLocation   *location)
{
  g_return_if_fail (GCAL_IS_WEATHER_SERVICE (self));

  if (self->location_service_running || self->weather_service_running)
    gcal_weather_service_stop (self);

  if (location == NULL)
    {
      /* Start location and weather service: */
      self->location_service_running = TRUE;
      self->location_service_running = TRUE;
      g_cancellable_cancel (self->location_cancellable);
      g_cancellable_reset (self->location_cancellable);
      gclue_simple_new (DESKTOP_FILE_NAME,
                        GCLUE_ACCURACY_LEVEL_EXACT,
                        self->location_cancellable,
                        (GAsyncReadyCallback) on_gclue_simple_creation,
                        g_object_ref (self));
    }
  else
    {
      /* Use the given location to retrieve weather information: */
      self->location_service_running = FALSE;
      self->location_service_running = TRUE;

      gcal_weather_service_update_location (self, location);
      gcal_weather_service_timer_start (self);
    }
}



/**
 * gcal_weather_service_stop:
 * @self: The #GcalWeatherService instance.
 *
 * Stops the service. Returns gracefully if service is
 * not running.
 */
void
gcal_weather_service_stop (GcalWeatherService *self)
{
  g_return_if_fail (GCAL_IS_WEATHER_SERVICE (self));

  if (!self->location_service_running && !self->weather_service_running)
    return ;

  self->location_service_running = FALSE;
  self->weather_service_running = FALSE;

  gcal_weather_service_timer_stop (self);

  /* Notify all listeners about unknown location: */
  gcal_weather_service_update_location (self, NULL);

  if (self->location_service == NULL)
    {
      /* location service is under construction. Cancel creation. */
      g_cancellable_cancel (self->location_cancellable);
    }
  else
    {
      GClueClient *client; /* unowned */

      client = gclue_simple_get_client (self->location_service);

      gclue_client_call_stop (client,
                              NULL,
                              (GAsyncReadyCallback) on_gclue_client_stop,
                              g_steal_pointer (&self->location_service));
    }
}



/**
 * gcal_weather_service_get_max_days:
 * @self: The #GcalWeatherService instance.
 *
 * Getter for #GcalWeatherService:max-days.
 */
guint
gcal_weather_service_get_max_days (GcalWeatherService *self)
{
  g_return_val_if_fail (GCAL_IS_WEATHER_SERVICE (self), 0);

  return self->max_days;
}



/**
 * gcal_weather_service_get_max_days:
 * @self: The #GcalWeatherService instance.
 *
 * Getter for #GcalWeatherService:max-days.
 */
guint
gcal_weather_service_get_check_interval (GcalWeatherService *self)
{
  g_return_val_if_fail (GCAL_IS_WEATHER_SERVICE (self), 0);

  return self->check_interval;
}
