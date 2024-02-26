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

#define G_LOG_DOMAIN      "GcalWeatherService"

#include <geoclue.h>
#include <string.h>
#include <math.h>

#include "config.h"
#include "gcal-debug.h"
#include "gcal-timer.h"
#include "gcal-weather-service.h"


#define GCAL_WEATHER_CHECK_INTERVAL_RENEW_DEFAULT (3 * 60 * 60)  /* seconds */
#define GCAL_WEATHER_CHECK_INTERVAL_NEW_DEFAULT   (5 * 60)       /* seconds */
#define GCAL_WEATHER_VALID_TIMESPAN_DEFAULT       (24 * 60 * 60) /* seconds */
#define GCAL_WEATHER_FORECAST_MAX_DAYS_DEFAULT     5

#define DAY_SECONDS (24 * 60 * 60)

/**
 * Internal structure used to manage known
 * weather icons.
 */
typedef struct
{
  gchar              *name;
  gboolean            night_support;
} GcalWeatherIconInfo;


/* GcalWeatherService:
 *
 * @time_zone:               The current time zone
 * @use_counter:             Number of active users of the service.
 * @check_interval_new:      Amount of seconds to wait before fetching weather infos.
 * @check_interval_renew:    Amount of seconds to wait before re-fetching weather information.
 * @duration_timer           Timer used to request weather report updates.
 * @midnight_timer           Timer used to update weather reports at midnight.
 * @network_changed_sid      "network-changed" signal ID.
 * @location:                Used for from where to display the weather. Current location if NULL.
 * @location_service:        Used to monitor location changes.
 *                           Initialized by gcal_weather_service_run(),
 *                           freed by gcal_weather_service_stop().
 * @location_cancellable:    Used to deal with async location service construction.
 * @locaton_running:         Whether location service is active.
 * @weather_infos:           List of #GcalWeatherInfo objects.
 * @weather_infos_upated:    The monotonic time @weather_info was set at.
 * @valid_timespan:          Amount of seconds weather information are considered valid.
 * @gweather_info:           The weather info to query.
 * @max_days:                Number of days we want weather information for.
 * @weather_service_active:  True if weather service is requested to be active, regardless of being in use.
 * @weather_service_running: True if weather service is active.
 * @weather_is_stale:        True if update was request without the service running.
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
  GObjectClass        parent;

  GTimeZone          *timezone;            /* owned, nullable */

  guint               use_counter;

  /* timer: */
  guint               check_interval_new;
  guint               check_interval_renew;
  GcalTimer          *duration_timer;
  GcalTimer          *midnight_timer;

  /* network monitoring */
  gulong              network_changed_sid;

  /* locations: */
  GWeatherLocation   *location;             /* owned, nullable */
  GClueSimple        *location_service;     /* owned, nullable */
  GCancellable       *location_cancellable; /* owned, non-null */
  gboolean            location_service_running;

  /* weather: */
  GPtrArray          *weather_infos;        /* owned[owned] */
  gint64              weather_infos_upated;
  gint64              valid_timespan;
  GWeatherInfo       *gweather_info;        /* owned, nullable */
  guint               max_days;
  gboolean            weather_service_active;
  gboolean            weather_service_running;
  gboolean            weather_is_stale;
};

static void          on_gweather_update_cb                       (GWeatherInfo       *info,
                                                                  GcalWeatherService *self);

G_DEFINE_TYPE (GcalWeatherService, gcal_weather_service, G_TYPE_OBJECT)

enum
{
  PROP_0,
  PROP_TIME_ZONE,
  PROP_LOCATION,
  PROP_NUM,
};

enum
{
  SIG_WEATHER_CHANGED,
  N_SIGNALS,
};

static guint signals[N_SIGNALS] = { 0 };


/*
 * Auxiliary methods
 */

static gssize
get_normalized_icon_name_len (const gchar *str)
{
  const gchar *suffix1 = "-symbolic";
  const gssize suffix1_len = strlen (suffix1);

  const gchar *suffix2 = "-night";
  const gssize suffix2_len = strlen (suffix2);

  gssize clean_len;
  gssize str_len;

  str_len = strlen (str);

  clean_len = str_len - suffix1_len;
  if (clean_len >= 0 && memcmp (suffix1, str + clean_len, suffix1_len) == 0)
    str_len = clean_len;

  clean_len = str_len - suffix2_len;
  if (clean_len >= 0 && memcmp (suffix2, str + clean_len, suffix2_len) == 0)
    str_len = clean_len;

  return str_len;
}

static gchar*
get_normalized_icon_name (GWeatherInfo* wi,
                          gboolean      is_night_icon)
{
  const gchar night_pfx[] = "-night";
  const gsize night_pfx_size = G_N_ELEMENTS (night_pfx) - 1;

  const gchar sym_pfx[] = "-symbolic";
  const gsize sym_pfx_size = G_N_ELEMENTS (sym_pfx) - 1;

  const gchar *str; /* unowned */
  gssize normalized_size;
  gchar *buffer = NULL; /* owned */
  gchar *bufpos = NULL; /* unowned */
  gsize buffer_size;

  g_return_val_if_fail (wi != NULL, NULL);

  str = gweather_info_get_icon_name (wi);
  if (!str)
    return NULL;

  normalized_size = get_normalized_icon_name_len (str);
  g_return_val_if_fail (normalized_size >= 0, NULL);

  if (is_night_icon)
    buffer_size = normalized_size + night_pfx_size + sym_pfx_size + 1;
  else
    buffer_size = normalized_size + sym_pfx_size + 1;

  buffer = g_malloc (buffer_size);
  bufpos = buffer;

  memcpy (bufpos, str, normalized_size);
  bufpos = bufpos + normalized_size;

  if (is_night_icon)
    {
      memcpy (bufpos, night_pfx, night_pfx_size);
      bufpos = bufpos + night_pfx_size;
    }

  memcpy (bufpos, sym_pfx, sym_pfx_size);
  buffer[buffer_size - 1] = '\0';

  return buffer;
}

static gboolean
has_valid_weather_infos (GcalWeatherService *self)
{
  gint64 now;

  if (self->gweather_info == NULL || self->weather_infos_upated < 0)
    return FALSE;

  now = g_get_monotonic_time ();
  return (now - self->weather_infos_upated) / 1000000 <= self->valid_timespan;
}

static void
update_timeout_interval (GcalWeatherService *self)
{
  guint interval;

  if (has_valid_weather_infos (self))
    interval = self->check_interval_renew;
  else
    interval = self->check_interval_new;

  gcal_timer_set_default_duration (self->duration_timer, interval);
}

static void
schedule_midnight (GcalWeatherService  *self)
{
  g_autoptr (GTimeZone) zone = NULL;
  g_autoptr (GDateTime) now = NULL;
  g_autoptr (GDateTime) tom = NULL;
  g_autoptr (GDateTime) mid = NULL;
  gint64 real_now;
  gint64 real_mid;

  zone = !self->timezone ? g_time_zone_new_local () : g_time_zone_ref (self->timezone);

  now = g_date_time_new_now (zone);
  tom = g_date_time_add_days (now, 1);
  mid = g_date_time_new (zone,
                         g_date_time_get_year (tom),
                         g_date_time_get_month (tom),
                         g_date_time_get_day_of_month (tom),
                         0, 0, 0);

  real_mid = g_date_time_to_unix (mid);
  real_now = g_date_time_to_unix (now);

  gcal_timer_set_default_duration (self->midnight_timer,
                                   real_mid - real_now);
}

static void
start_timer (GcalWeatherService  *self)
{
  GNetworkMonitor *monitor;

  monitor = g_network_monitor_get_default ();

  if (g_network_monitor_get_network_available (monitor))
    {
      update_timeout_interval (self);
      gcal_timer_start (self->duration_timer);

      schedule_midnight (self);
      gcal_timer_start (self->midnight_timer);
    }
}

static void
stop_timer (GcalWeatherService  *self)
{
  gcal_timer_stop (self->duration_timer);
  gcal_timer_stop (self->midnight_timer);
}

static gint
get_icon_name_sortkey (const gchar *icon_name,
                       gboolean    *supports_night_icon)
{
  gssize normalized_name_len;
  guint i;

  const GcalWeatherIconInfo icons[] =
    { {"weather-clear",             TRUE},
      {"weather-few-clouds",        TRUE},
      {"weather-overcast",          FALSE},
      {"weather-fog",               FALSE},
      {"weather-showers-scattered", FALSE},
      {"weather-showers",           FALSE},
      {"weather-snow",              FALSE},
      {"weather-storm",             FALSE},
      {"weather-severe-alert",      FALSE}
    };

  g_return_val_if_fail (icon_name != NULL, -1);
  g_return_val_if_fail (supports_night_icon != NULL, -1);

  *supports_night_icon = FALSE;

  normalized_name_len = get_normalized_icon_name_len (icon_name);
  g_return_val_if_fail (normalized_name_len >= 0, -1);

  for (i = 0; i < G_N_ELEMENTS (icons); i++)
    {
      if (normalized_name_len == strlen (icons[i].name) &&
          strncmp (icon_name, icons[i].name, normalized_name_len) == 0)
        {
          *supports_night_icon = icons[i].night_support;
          return i;
        }
    }

  g_warning ("Unknown weather icon '%s'", icon_name);

  return -1;
}

static gboolean
get_time_day_start (GcalWeatherService *self,
                    GDate              *ret_date,
                    gint64             *ret_unix,
                    gint64             *ret_unix_exact)
{
  g_autoptr (GTimeZone) zone = NULL;
  g_autoptr (GDateTime) now = NULL;
  g_autoptr (GDateTime) day = NULL;

  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (ret_date != NULL, FALSE);
  g_return_val_if_fail (ret_unix != NULL, FALSE);
  g_return_val_if_fail (ret_unix_exact != NULL, FALSE);

  zone = !self->timezone ? g_time_zone_new_local () : g_time_zone_ref (self->timezone);

  now = g_date_time_new_now (zone);
  day = g_date_time_new (zone,
                         g_date_time_get_year (now),
                         g_date_time_get_month (now),
                         g_date_time_get_day_of_month (now),
                         0, 0, 0);

  g_date_set_dmy (ret_date,
                  g_date_time_get_day_of_month (day),
                  g_date_time_get_month (day),
                  g_date_time_get_year (day));

  *ret_unix = g_date_time_to_unix (day);
  *ret_unix_exact = g_date_time_to_unix (now);

  return TRUE;
}

static inline gboolean
get_gweather_temperature (GWeatherInfo *gwi,
                          gdouble      *temp)
{
  gboolean valid;
  gdouble  value;

  *temp = NAN;

  g_return_val_if_fail (gwi != NULL, FALSE);
  g_return_val_if_fail (temp != NULL, FALSE);

  valid = gweather_info_get_value_temp (gwi,
                                        GWEATHER_TEMP_UNIT_DEFAULT,
                                        &value);

  /* TODO: Extract temperatures in Celsius and catch implausible cases */
  if (valid)
    {
      *temp = value;
      return TRUE;
    }
  else
    {
      *temp = NAN;
      return FALSE;
    }
}

static gboolean
compute_weather_info_data (GSList    *samples,
                           gboolean   is_today,
                           gchar    **icon_name,
                           gchar    **temperature)
{
  GWeatherInfo *phenomenon_gwi = NULL;
  GWeatherInfo *temp_gwi = NULL;
  GSList *iter;
  gboolean phenomenon_supports_night_icon;
  gboolean has_daytime;
  gdouble temp_val;
  gint phenomenon_val;

  temp_val = NAN;
  has_daytime = FALSE;
  phenomenon_val = -1;
  phenomenon_supports_night_icon = FALSE;

  /*
   * Note: I checked three different gweather consumers
   *   and they all pick different values. So here is my
   *   take: I pick up the worst weather for icons and
   *   the highest temperature. I basically want to know
   *   whether I need my umbrella for my appointment.
   *   Not sure about the right temperature. It is probably
   *   better to pick-up the median of all predictions
   *   during daytime.
   */

  for (iter = samples; iter; iter = iter->next)
    {
      GWeatherInfo  *gwi;
      const gchar *icon_name;
      gboolean supports_night_icon;
      gboolean valid_temp;
      gdouble temp;
      gint phenomenon;

      gwi = GWEATHER_INFO (iter->data);
      phenomenon = -1;

      icon_name = gweather_info_get_icon_name (gwi);
      if (icon_name)
        phenomenon = get_icon_name_sortkey (icon_name, &supports_night_icon);

      valid_temp = get_gweather_temperature (gwi, &temp);

      if (phenomenon >= 0 && (phenomenon_gwi == NULL || phenomenon > phenomenon_val))
        {
          phenomenon_supports_night_icon = supports_night_icon;
          phenomenon_val = phenomenon;
          phenomenon_gwi = gwi;
        }

      if (valid_temp && (!temp_gwi || temp > temp_val))
        {
          temp_val = temp;
          temp_gwi = gwi;
        }

      if (gweather_info_is_daytime (gwi))
        has_daytime = TRUE;
    }

  if (phenomenon_gwi && temp_gwi)
    {
      *icon_name = get_normalized_icon_name (phenomenon_gwi, is_today && !has_daytime && phenomenon_supports_night_icon);
      *temperature = gweather_info_get_temp (temp_gwi);

      return TRUE;
    }
  else
    {
      /* empty list */
      *icon_name = NULL;
      *temperature = NULL;

      return FALSE;
    }
}

static GPtrArray*
preprocess_gweather_reports (GcalWeatherService *self,
                             GSList             *samples)
{
  GWeatherInfo *first_tomorrow = NULL; /* unowned */
  GPtrArray *result = NULL;
  GSList **days = NULL;   /* owned[owned[unowned]] */
  GSList *iter = NULL;    /* unowned */
  GDate cur_gdate;
  glong first_tomorrow_dtime = -1;
  gint64 today_unix;
  gint64 unix_now;
  guint i;

  /*
   * This function basically separates samples by date and calls compute_weather_info_data
   * for every bucket to build weather infos. Each bucket represents a single day.
   *
   * All gweather consumers I reviewed presume sorted samples. However, there is no documented
   * order. Lets assume the worst.
   */

  if (self->max_days <= 0)
    return NULL;

  if (!get_time_day_start (self, &cur_gdate, &today_unix, &unix_now))
    return NULL;

  result = g_ptr_array_new_full (self->max_days, g_object_unref);
  days = g_malloc0 (sizeof (GSList*) * self->max_days);

  /* Split samples to max_days buckets: */
  for (iter = samples; iter != NULL; iter = iter->next)
    {
      GWeatherInfo *gwi; /* unowned */
      gboolean valid_date;
      glong gwi_dtime;
      gsize bucket;

      gwi = GWEATHER_INFO (iter->data);
      valid_date = gweather_info_get_value_update (gwi, &gwi_dtime);
      if (!valid_date)
        continue;

      #if PRINT_WEATHER_DATA
      {
        g_autofree gchar* dbg_str = gwc2str (gwi);
        g_message ("WEATHER READING POINT: %s", dbg_str);
      }
      #endif

      if (gwi_dtime >= 0 && gwi_dtime >= today_unix)
        {
          bucket = (gwi_dtime - today_unix) / DAY_SECONDS;
          if (bucket < self->max_days)
            days[bucket] = g_slist_prepend (days[bucket], gwi);

          if (bucket == 1 && (first_tomorrow == NULL || first_tomorrow_dtime > gwi_dtime))
            {
              first_tomorrow_dtime = gwi_dtime;
              first_tomorrow = gwi;
            }
        }
      else
        {
          g_debug ("Encountered historic weather information");
        }
    }

  if (!days[0] && first_tomorrow)
    {

      glong secs_left_today;
      glong secs_between;

      /* There is no data point left for today. Lets borrow one. */
      secs_left_today = DAY_SECONDS - (unix_now - today_unix);
      secs_between = first_tomorrow_dtime - unix_now;

      if (secs_left_today < 90 * 60 && secs_between <= 180 * 60)
        days[0] = g_slist_prepend (days[0], first_tomorrow);
    }

  /* Produce GcalWeatherInfo for each bucket: */
  for (i = 0; i < self->max_days; i++)
    {
      g_autofree gchar *temperature = NULL;
      g_autofree gchar *icon_name = NULL;

      if (compute_weather_info_data (days[i], i == 0, &icon_name, &temperature))
        g_ptr_array_add (result, gcal_weather_info_new (&cur_gdate, icon_name, temperature));

      g_date_add_days (&cur_gdate, 1);
    }

  /* Cleanup */
  for (i = 0; i < self->max_days; i++)
    g_slist_free (days[i]);
  g_free (days);

  return result;
}

static void
update_weather (GcalWeatherService *self,
                GWeatherInfo       *info,
                gboolean            reuse_old_on_error)
{
  GSList *gwforecast = NULL; /* unowned */

  /* Compute a list of newly received weather infos. */
  if (!info)
    {
      g_debug ("Could not retrieve valid weather");
    }
  else if (gweather_info_is_valid (info))
    {
      g_debug ("Received valid weather information");
      gwforecast = gweather_info_get_forecast_list (info);
    }
  else
    {
      g_autofree gchar* location_name = gweather_info_get_location_name (info);
      g_debug ("Could not retrieve valid weather for location '%s'", location_name);
    }

  if (!gwforecast && self->weather_infos_upated >= 0)
    {
      if (!reuse_old_on_error || !has_valid_weather_infos (self))
        {
          g_clear_pointer (&self->weather_infos, g_ptr_array_unref);
          self->weather_infos_upated = -1;

          g_signal_emit (self, signals[SIG_WEATHER_CHANGED], 0);
        }
    }
  else if (gwforecast)
    {
      g_clear_pointer (&self->weather_infos, g_ptr_array_unref);
      self->weather_infos = preprocess_gweather_reports (self, gwforecast);
      self->weather_infos_upated = g_get_monotonic_time ();

      g_signal_emit (self, signals[SIG_WEATHER_CHANGED], 0);
    }
}

static void
update_location (GcalWeatherService  *self,
                 GWeatherLocation    *location)
{
  if (gcal_timer_is_running (self->duration_timer))
    stop_timer (self);

  if (self->gweather_info != NULL)
    {
      g_signal_handlers_disconnect_by_data (self->gweather_info, self);
      g_clear_object (&self->gweather_info);
    }

  if (!location)
    {
      g_debug ("Could not retrieve current location");
      update_weather (self, NULL, FALSE);
    }
  else
    {
      g_debug ("Got new weather service location: '%s'",
               !location ? "<null>" : gweather_location_get_name (location));

      self->gweather_info = gweather_info_new (location);
      gweather_info_set_contact_info (self->gweather_info,
                                      "https://gitlab.gnome.org/GNOME/gnome-calendar/-/raw/master/gnome-calendar.doap");

      gweather_info_set_enabled_providers (self->gweather_info,
                                           GWEATHER_PROVIDER_METAR |
                                           GWEATHER_PROVIDER_MET_NO |
                                           GWEATHER_PROVIDER_OWM);
      g_signal_connect_object (self->gweather_info, "updated", (GCallback) on_gweather_update_cb, self, 0);

      /*
       * gweather_info_update might or might not trigger a
       * GWeatherInfo::update() signal. Therefore, we have to
       * remove weather information before querying new one.
       * This might result in icon flickering on screen.
       * We probably want to introduce a "unknown" or "loading"
       * state in gweather-info to soften the effect.
       */
      update_weather (self, NULL, FALSE);
      gweather_info_update (self->gweather_info);

      start_timer (self);
    }
}

static void
update_gclue_location (GcalWeatherService  *self,
                       GClueLocation       *location)
{
  g_autoptr (GWeatherLocation) wlocation = NULL; /* owned */

  if (location)
    {
      GWeatherLocation *wworld; /* unowned */
      gdouble latitude;
      gdouble longitude;

      latitude = gclue_location_get_latitude (location);
      longitude = gclue_location_get_longitude (location);

      /* nearest-city works more closely to gnome weather. */
      wworld = gweather_location_get_world ();
      wlocation = gweather_location_find_nearest_city (wworld, latitude, longitude);
    }

  update_location (self, wlocation);
}

static void
start_or_stop_weather_service (GcalWeatherService *self)
{
  if (self->use_counter > 0 && self->weather_service_active)
    gcal_weather_service_start (self);
  else
    gcal_weather_service_stop (self);
}


/*
 * Callbacks
 */

static void
on_network_changed_cb (GNetworkMonitor    *monitor,
                       gboolean            available,
                       GcalWeatherService *self)
{
  gboolean is_running;

  is_running = gcal_timer_is_running (self->duration_timer);

  if (available && !is_running)
    {
      if (self->gweather_info)
        gweather_info_update (self->gweather_info);

      start_timer (self);
    }
  else if (!available && is_running)
    {
      stop_timer (self);
    }
}

static void
on_gclue_location_changed_cb (GClueLocation      *location,
                              GcalWeatherService *self)
{
  update_gclue_location (self, location);
}

static void
on_gclue_client_activity_changed_cb (GClueClient        *client,
                                     GcalWeatherService *self)
{
  /* Notify listeners about unknown locations: */
  update_location (self, NULL);
}

static void
on_gclue_simple_creation_cb (GClueSimple        *_source,
                             GAsyncResult       *result,
                             GcalWeatherService *self)
{
  g_autoptr (GError) error = NULL;
  GClueLocation *location;
  GClueClient *client;

  GCAL_ENTRY;

  self->location_service = gclue_simple_new_finish (result, &error);

  if (error)
    {
      g_assert_null (self->location_service);

      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED) &&
          !(g_dbus_error_is_remote_error (error) && strcmp (g_dbus_error_get_remote_error (error), "org.freedesktop.DBus.Error.AccessDenied") == 0))
        g_warning ("Could not create GCLueSimple: %s", error->message);

      GCAL_RETURN ();
    }

  location = gclue_simple_get_location (self->location_service);
  client = gclue_simple_get_client (self->location_service);

  if (location)
    {
      update_gclue_location (self, location);

      g_signal_connect_object (location,
                               "notify::location",
                               G_CALLBACK (on_gclue_location_changed_cb),
                               self,
                               0);
    }
  if (client)
    {
      g_signal_connect_object (client,
                               "notify::active",
                               G_CALLBACK (on_gclue_client_activity_changed_cb),
                               self,
                               0);
    }
  GCAL_EXIT;
}

static void
on_gclue_client_stopped_cb (GClueClient  *client,
                            GAsyncResult *res,
                            GClueSimple  *simple)
{
  g_autoptr (GError) error = NULL;
  gboolean stopped;

  stopped = gclue_client_call_stop_finish (client, res, &error);

  if (error)
    g_warning ("Could not stop location service: %s", error->message);
  else if (!stopped)
    g_warning ("Could not stop location service");

  g_object_unref (simple);
}

static void
on_gweather_update_cb (GWeatherInfo       *info,
                       GcalWeatherService *self)
{
  update_weather (self, info, TRUE);
}

static void
on_duration_timer_timeout (GcalTimer          *timer,
                           GcalWeatherService *self)
{
  if (self->gweather_info)
    gweather_info_update (self->gweather_info);
}

static void
on_midnight_timer_timeout (GcalTimer          *timer,
                           GcalWeatherService *self)
{
  if (self->gweather_info)
    gweather_info_update (self->gweather_info);

  if (gcal_timer_is_running (self->duration_timer))
    gcal_timer_reset (self->duration_timer);

  schedule_midnight (self);
}

#if PRINT_WEATHER_DATA
static gchar*
gwc2str (GWeatherInfo *gwi)
{
    g_autoptr (GDateTime) date = NULL;
    g_autofree gchar *date_str = NULL;
    glong update;

    gchar *icon_name; /* unowned */
    gdouble temp;

    if (!gweather_info_get_value_update (gwi, &update))
      return g_strdup ("<null>");

    date = g_date_time_new_from_unix_local (update);
    date_str = g_date_time_format (date, "%F %T"),

    get_gweather_temperature (gwi, &temp);
    icon_name = gweather_info_get_symbolic_icon_name (gwi);

    return g_strdup_printf ("(%s: t:%f, w:%s)",
                            date_str,
                            temp,
                            icon_name);
}
#endif


/*
 * GObject overrides
 */

static void
gcal_weather_service_finalize (GObject *object)
{
  GcalWeatherService *self = (GcalWeatherService *) object;

  g_cancellable_cancel (self->location_cancellable);

  g_clear_pointer (&self->duration_timer, gcal_timer_free);
  g_clear_pointer (&self->midnight_timer, gcal_timer_free);
  g_clear_pointer (&self->timezone, g_time_zone_unref);
  g_clear_pointer (&self->weather_infos, g_ptr_array_unref);

  g_clear_object (&self->gweather_info);
  g_clear_object (&self->location_service);
  g_clear_object (&self->location_cancellable);

  if (self->network_changed_sid > 0)
    g_signal_handler_disconnect (g_network_monitor_get_default (), self->network_changed_sid);

  G_OBJECT_CLASS (gcal_weather_service_parent_class)->finalize (object);
}

static void
gcal_weather_service_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GcalWeatherService *self = (GcalWeatherService *) object;

  switch (prop_id)
    {
    case PROP_TIME_ZONE:
      g_value_set_pointer (value, gcal_weather_service_get_time_zone (self));
      break;

    case PROP_LOCATION:
      g_value_set_pointer (value, gcal_weather_service_get_location (self));
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
  GcalWeatherService *self = (GcalWeatherService *) object;

  switch (prop_id)
    {
    case PROP_TIME_ZONE:
      gcal_weather_service_set_time_zone (self, g_value_get_pointer (value));
      break;

    case PROP_LOCATION:
      gcal_weather_service_set_location (self, g_value_get_pointer (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gcal_weather_service_class_init (GcalWeatherServiceClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = gcal_weather_service_finalize;
  object_class->get_property = gcal_weather_service_get_property;
  object_class->set_property = gcal_weather_service_set_property;

  /**
   * GcalWeatherService:time-zone:
   *
   * The time zone to use.
   */
  g_object_class_install_property (object_class,
                                   PROP_TIME_ZONE,
                                   g_param_spec_pointer ("time-zone",
                                                         "time-zone",
                                                         "time-zone",
                                                         G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE));

  /**
   * GcalWeatherService:location:
   *
   * The location to use, automatic if NULL.
   */
  g_object_class_install_property (object_class,
                                   PROP_LOCATION,
                                   g_param_spec_object ("location",
                                                        "location",
                                                        "location",
                                                        GWEATHER_TYPE_LOCATION,
                                                        G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE));

  /**
   * GcalWeatherService::weather-changed:
   * @sender: The #GcalWeatherService
   * @self:   data pointer.
   *
   * Triggered on weather changes. Call
   * gcal_weather_service_get_weather_infos() to
   * retrieve predictions.
   */
  signals[SIG_WEATHER_CHANGED] = g_signal_new ("weather-changed",
                                               GCAL_TYPE_WEATHER_SERVICE,
                                               G_SIGNAL_RUN_LAST,
                                               0,
                                               NULL,
                                               NULL,
                                               g_cclosure_marshal_VOID__VOID,
                                               G_TYPE_NONE,
                                               0);
}

static void
gcal_weather_service_init (GcalWeatherService *self)
{
  self->max_days = GCAL_WEATHER_FORECAST_MAX_DAYS_DEFAULT;
  self->check_interval_new = GCAL_WEATHER_CHECK_INTERVAL_NEW_DEFAULT;
  self->check_interval_renew = GCAL_WEATHER_CHECK_INTERVAL_RENEW_DEFAULT;
  self->location_cancellable = g_cancellable_new ();
  self->weather_infos_upated = -1;
  self->valid_timespan = GCAL_WEATHER_VALID_TIMESPAN_DEFAULT;

  self->duration_timer = gcal_timer_new (GCAL_WEATHER_CHECK_INTERVAL_NEW_DEFAULT);
  gcal_timer_set_callback (self->duration_timer, (GCalTimerFunc) on_duration_timer_timeout, self, NULL);

  self->midnight_timer = gcal_timer_new (24 * 60 * 60);
  gcal_timer_set_callback (self->midnight_timer, (GCalTimerFunc) on_midnight_timer_timeout, self, NULL);

  self->network_changed_sid = g_signal_connect (g_network_monitor_get_default (),
                                                "network-changed",
                                                G_CALLBACK (on_network_changed_cb),
                                                self);
}

/**
 * gcal_weather_service_new:
 *
 * Creates a new #GcalWeatherService. This service listens
 * to location and weather changes and reports them.
 *
 * Returns: (transfer full): A newly created #GcalWeatherService.
 */
GcalWeatherService*
gcal_weather_service_new (void)
{
  return g_object_new (GCAL_TYPE_WEATHER_SERVICE, NULL);
}

/**
 * gcal_weather_service_get_location:
 * @self: The #GcalWeatherService instance.
 *
 * Getter for #GcalWeatherService:location.
 */
GWeatherLocation*
gcal_weather_service_get_location (GcalWeatherService *self)
{
  g_return_val_if_fail (GCAL_IS_WEATHER_SERVICE (self), NULL);

  return self->location;
}

/**
 * gcal_weather_service_set_location:
 * @self: The #GcalWeatherService instance.
 * @value: new location, nullable.
 *
 * Setter for #GcalWeatherInfos:location.
 */
void
gcal_weather_service_set_location (GcalWeatherService *self,
                                   GWeatherLocation   *value)
{
  g_return_if_fail (GCAL_IS_WEATHER_SERVICE (self));

  if (self->location == value)
    return;

  self->location = value;

  gcal_weather_service_start (self);

  g_object_notify (G_OBJECT (self), "location");
}

/**
 * gcal_weather_service_get_time_zone:
 * @self: The #GcalWeatherService instance.
 *
 * Getter for #GcalWeatherService:time-zone.
 */
GTimeZone*
gcal_weather_service_get_time_zone (GcalWeatherService *self)
{
  g_return_val_if_fail (GCAL_IS_WEATHER_SERVICE (self), NULL);

  return self->timezone;
}

/**
 * gcal_weather_service_set_time_zone:
 * @self: The #GcalWeatherService instance.
 * @days: Number of days.
 *
 * Setter for #GcalWeatherInfos:time-zone.
 */
void
gcal_weather_service_set_time_zone (GcalWeatherService *self,
                                    GTimeZone          *value)
{
  g_return_if_fail (GCAL_IS_WEATHER_SERVICE (self));

  if (self->timezone == value)
    return;

  g_clear_pointer (&self->timezone, g_time_zone_unref);
  self->timezone = value ? g_time_zone_ref (value) : NULL;

  /* make sure we provide correct weather infos */
  gweather_info_update (self->gweather_info);

  /* make sure midnight is timed correctly: */
  schedule_midnight (self);

  g_object_notify (G_OBJECT (self), "time-zone");
}

/**
 * gcal_weather_service_get_weather_info_for_date:
 * @self: The #GcalWeatherService instance.
 * @date: The #GDate for which to get the current forecast.
 *
 * Retrieves the available weather information for @date.
 *
 * Returns: (transfer none)(nullable): a #GcalWeatherInfo
 */
GcalWeatherInfo*
gcal_weather_service_get_weather_info_for_date (GcalWeatherService *self,
                                                GDate              *date)
{
  g_return_val_if_fail (GCAL_IS_WEATHER_SERVICE (self), NULL);

  for (guint i = 0; self->weather_infos && i < self->weather_infos->len; i++)
    {
      GcalWeatherInfo *info;
      GDate weather_date;

      info = g_ptr_array_index (self->weather_infos, i);

      gcal_weather_info_get_date (info, &weather_date);

      if (g_date_compare (&weather_date, date) == 0)
        return info;
    }

  return NULL;
}

/**
 * gcal_weather_service_get_weather_infos:
 * @self: The #GcalWeatherService instance.
 *
 * Returns: (transfer none): list of known weather reports.
 */
GPtrArray*
gcal_weather_service_get_weather_infos (GcalWeatherService *self)
{
  g_return_val_if_fail (GCAL_IS_WEATHER_SERVICE (self), NULL);

  return self->weather_infos;
}

/**
 * gcal_weather_service_get_attribution:
 * @self: The #GcalWeatherService instance.
 *
 * Returns weather service attribution.
 *
 * Returns: (nullable) (transfer none): Text to display.
 */
const gchar*
gcal_weather_service_get_attribution (GcalWeatherService *self)
{
  g_return_val_if_fail (GCAL_IS_WEATHER_SERVICE (self), NULL);

  if (self->gweather_info)
    return gweather_info_get_attribution (self->gweather_info);

  return NULL;
}

/**
 * gcal_weather_service_update:
 * @self: The #GcalWeatherService instance.
 *
 * Tries to update weather reports.
 */
void
gcal_weather_service_update (GcalWeatherService *self)
{
  g_return_if_fail (GCAL_IS_WEATHER_SERVICE (self));

  if (!self->weather_service_running)
    {
      self->weather_is_stale = TRUE;
      return;
    }
  else
    {
      self->weather_is_stale = FALSE;
    }

  if (self->gweather_info)
    {
      gweather_info_update (self->gweather_info);
      update_timeout_interval (self);

      if (gcal_timer_is_running (self->duration_timer))
        gcal_timer_reset (self->duration_timer);
    }
}


/**
 * gcal_weather_service_activate:
 * @self: The #GcalWeatherService instance.
 *
 * Activates weather service if needed.
 * Use ::weather-changed to catch responses.
 */
void
gcal_weather_service_activate (GcalWeatherService *self)
{
  g_return_if_fail (GCAL_IS_WEATHER_SERVICE (self));

  self->weather_service_active = TRUE;

  start_or_stop_weather_service (self);
}

/**
 * gcal_weather_service_deactivate:
 * @self: The #GcalWeatherService instance.
 *
 * Stops the service.
 */
void
gcal_weather_service_deactivate (GcalWeatherService *self)
{
  g_return_if_fail (GCAL_IS_WEATHER_SERVICE (self));

  self->weather_service_active = FALSE;

  start_or_stop_weather_service (self);
}

/**
 * gcal_weather_service_start:
 * @self: The #GcalWeatherService instance.
 *
 * Starts to monitor location and weather changes.
 * Use ::weather-changed to catch responses.
 */
void
gcal_weather_service_start (GcalWeatherService *self)
{
  g_return_if_fail (GCAL_IS_WEATHER_SERVICE (self));

  if (self->location_service_running && self->weather_service_running)
    return;

  g_debug ("Starting weather service");

  self->weather_service_running = TRUE;

  if (!self->location)
    {
      /* Start location and weather service: */
      self->location_service_running = TRUE;

      g_cancellable_cancel (self->location_cancellable);
      g_cancellable_reset (self->location_cancellable);

      gclue_simple_new (APPLICATION_ID,
                        GCLUE_ACCURACY_LEVEL_CITY,
                        self->location_cancellable,
                        (GAsyncReadyCallback) on_gclue_simple_creation_cb,
                        self);
    }
  else
    {
      self->location_service_running = FALSE;

      /* TODO: stop running location service */

      /*_update_location starts timer if necessary */
      update_location (self, self->location);
    }

  if (self->weather_is_stale)
    {
      gcal_weather_service_update (self);
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
  GCAL_ENTRY;
  GClueClient *client;

  g_return_if_fail (GCAL_IS_WEATHER_SERVICE (self));

  if (!self->location_service_running && !self->weather_service_running)
    GCAL_RETURN ();

  g_debug ("Stopping weather service");

  self->location_service_running = FALSE;
  self->weather_service_running = FALSE;

  /* Notify all listeners about unknown location */
  update_location (self, NULL);

  if (!self->location_service)
    {
      /* location service is under construction. Cancel creation. */
      g_cancellable_cancel (self->location_cancellable);
    }
  else
    {
      client = gclue_simple_get_client (self->location_service);
      if (client)
        {
          gclue_client_call_stop (client,
                                  self->location_cancellable,
                                  (GAsyncReadyCallback) on_gclue_client_stopped_cb,
                                  self->location_service);
        }
      g_clear_object (&self->location_service);
    }

  GCAL_EXIT;
}

/**
 * gcal_weather_service_hold:
 * @self: The #GcalWeatherService instance.
 *
 * Mark the service as in use by increasing its use counter.
 **/
void
gcal_weather_service_hold (GcalWeatherService *self)
{
  g_return_if_fail (GCAL_IS_WEATHER_SERVICE (self));

  self->use_counter++;

  start_or_stop_weather_service (self);
}

/**
 * gcal_weather_service_release:
 * @self: The #GcalWeatherService instance.
 *
 * Mark the use the of service to be over by decreasing its use counter.
 * A call to this function is expected to be paired with a previous
 * call to gcal_weather_service_hold.
 **/
void
gcal_weather_service_release (GcalWeatherService *self)
{
  g_return_if_fail (GCAL_IS_WEATHER_SERVICE (self));
  g_return_if_fail (self->use_counter > 0);

  self->use_counter--;

  start_or_stop_weather_service (self);
}
