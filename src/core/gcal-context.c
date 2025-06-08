/* gcal-context.c
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

#define G_LOG_DOMAIN "GcalContext"

#include "gcal-context.h"
#include "gcal-time-zone-monitor.h"

struct _GcalContext
{
  GObject             parent;

  GDBusProxy         *settings_portal;

  GcalClock          *clock;
  GcalManager        *manager;
  GcalSearchEngine   *search_engine;
  GSettings          *settings;
  GcalTimeFormat      time_format;
  GcalWeatherService *weather_service;

  GcalTimeZoneMonitor   *timezone_monitor;
};

G_DEFINE_TYPE (GcalContext, gcal_context, G_TYPE_OBJECT)

enum
{
  PROP_0,
  PROP_CLOCK,
  PROP_MANAGER,
  PROP_SEARCH_ENGINE,
  PROP_SETTINGS,
  PROP_TIME_FORMAT,
  PROP_TIMEZONE,
  PROP_WEATHER_SERVICE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];


/*
 * Auxiliary methods
 */

static void
set_time_format_from_variant (GcalContext *self,
                              GVariant    *variant)
{
  g_autofree gchar *enum_format = NULL;
  GcalTimeFormat  time_format;

  g_assert (g_variant_type_equal (g_variant_get_type (variant), "s"));

  if (g_strcmp0 (g_variant_get_string (variant, NULL), "12h") == 0)
    time_format = GCAL_TIME_FORMAT_12H;
  else
    time_format = GCAL_TIME_FORMAT_24H;

  if (self->time_format == time_format)
    return;

  self->time_format = time_format;

  enum_format = g_enum_to_string (GCAL_TYPE_TIME_FORMAT, self->time_format);
  g_debug ("Setting time format to %s", enum_format);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TIME_FORMAT]);
}

static gboolean
read_time_format (GcalContext *self)
{
  g_autoptr (GVariant) other_child = NULL;
  g_autoptr (GVariant) child = NULL;
  g_autoptr (GVariant) ret = NULL;
  g_autoptr (GError) error = NULL;

  ret = g_dbus_proxy_call_sync (self->settings_portal,
                                "Read",
                                g_variant_new ("(ss)", "org.gnome.desktop.interface", "clock-format"),
                                G_DBUS_CALL_FLAGS_NONE,
                                G_MAXINT,
                                NULL,
                                &error);

  if (error)
    {
      return FALSE;
    }

  g_variant_get (ret, "(v)", &child);
  g_variant_get (child, "v", &other_child);

  set_time_format_from_variant (self, other_child);

  return TRUE;
}


/*
 * Callbacks
 */

static void
on_portal_proxy_signal_cb (GDBusProxy  *proxy,
                           const gchar *sender_name,
                           const gchar *signal_name,
                           GVariant    *parameters,
                           GcalContext *self)
{
  g_autoptr (GVariant) value = NULL;
  const char *namespace;
  const char *name;

  if (g_strcmp0 (signal_name, "SettingChanged") != 0)
    return;

  g_variant_get (parameters, "(&s&sv)", &namespace, &name, &value);

  if (g_strcmp0 (namespace, "org.gnome.desktop.interface") == 0 &&
      g_strcmp0 (name, "clock-format") == 0)
    {
      set_time_format_from_variant (self, value);
    }
}

static void
on_timezone_changed_cb (GcalTimeZoneMonitor *timezone_monitor,
                        GParamSpec          *pspec,
                        GcalContext         *self)
{
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TIMEZONE]);
}


/*
 * GObject overrides
 */

static void
gcal_context_constructed (GObject *object)
{
  GcalContext *self = (GcalContext *)object;

  G_OBJECT_CLASS (gcal_context_parent_class)->constructed (object);

  self->manager = gcal_manager_new (self);
  self->search_engine = gcal_search_engine_new (self);
}

static void
gcal_context_finalize (GObject *object)
{
  GcalContext *self = (GcalContext *)object;

  gcal_weather_service_stop (self->weather_service);

  g_clear_object (&self->clock);
  g_clear_object (&self->settings_portal);
  g_clear_object (&self->manager);
  g_clear_object (&self->timezone_monitor);
  g_clear_object (&self->weather_service);

  G_OBJECT_CLASS (gcal_context_parent_class)->finalize (object);
}

static void
gcal_context_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  GcalContext *self = GCAL_CONTEXT (object);

  switch (prop_id)
    {
    case PROP_CLOCK:
      g_value_set_object (value, self->clock);
      break;

    case PROP_MANAGER:
      g_value_set_object (value, self->manager);
      break;

    case PROP_SEARCH_ENGINE:
      g_value_set_object (value, self->search_engine);
      break;

    case PROP_SETTINGS:
      g_value_set_object (value, self->settings);
      break;

    case PROP_TIME_FORMAT:
      g_value_set_enum (value, self->time_format);
      break;

    case PROP_TIMEZONE:
      g_value_set_boxed (value, gcal_time_zone_monitor_get_timezone (self->timezone_monitor));
      break;

    case PROP_WEATHER_SERVICE:
      g_value_set_object (value, self->weather_service);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_context_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  switch (prop_id)
    {
    case PROP_CLOCK:
    case PROP_MANAGER:
    case PROP_SEARCH_ENGINE:
    case PROP_SETTINGS:
    case PROP_TIME_FORMAT:
    case PROP_TIMEZONE:
    case PROP_WEATHER_SERVICE:
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_context_class_init (GcalContextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = gcal_context_constructed;
  object_class->finalize = gcal_context_finalize;
  object_class->get_property = gcal_context_get_property;
  object_class->set_property = gcal_context_set_property;

  properties[PROP_CLOCK] = g_param_spec_object ("clock",
                                                "Wall clock",
                                                "Main clock driving the calendar timeline",
                                                GCAL_TYPE_CLOCK,
                                                G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  properties[PROP_MANAGER] = g_param_spec_object ("manager",
                                                  "Data manager",
                                                  "Data manager of the application",
                                                  GCAL_TYPE_MANAGER,
                                                  G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  properties[PROP_SEARCH_ENGINE] = g_param_spec_object ("search-engine",
                                                        "Search engine",
                                                        "Search engine",
                                                        GCAL_TYPE_SEARCH_ENGINE,
                                                        G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  properties[PROP_SETTINGS] = g_param_spec_object ("settings",
                                                   "GNOME Calendar settings",
                                                   "GNOME Calendar settings",
                                                   G_TYPE_SETTINGS,
                                                   G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  properties[PROP_TIME_FORMAT] = g_param_spec_enum ("time-format",
                                                    "Time format",
                                                    "System time format",
                                                    GCAL_TYPE_TIME_FORMAT,
                                                    GCAL_TIME_FORMAT_24H,
                                                    G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  properties[PROP_TIMEZONE] = g_param_spec_boxed ("timezone",
                                                  "Timezone",
                                                  "System timezone",
                                                  G_TYPE_TIME_ZONE,
                                                  G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  properties[PROP_WEATHER_SERVICE] = g_param_spec_object ("weather-service",
                                                          "Weather service",
                                                          "Weather service",
                                                          GCAL_TYPE_WEATHER_SERVICE,
                                                          G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gcal_context_init (GcalContext *self)
{
  g_autoptr (GError) error = NULL;

  self->clock = gcal_clock_new ();
  self->settings = g_settings_new ("org.gnome.calendar");
  self->weather_service = gcal_weather_service_new ();
  self->time_format = GCAL_TIME_FORMAT_24H;

  self->timezone_monitor = gcal_time_zone_monitor_new ();
  g_signal_connect_object (self->timezone_monitor,
                           "notify::timezone",
                           G_CALLBACK (on_timezone_changed_cb),
                           self,
                           0);

  /* Time format */
  self->settings_portal = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                         G_DBUS_PROXY_FLAGS_NONE,
                                                         NULL,
                                                         "org.freedesktop.portal.Desktop",
                                                         "/org/freedesktop/portal/desktop",
                                                         "org.freedesktop.portal.Settings",
                                                         NULL,
                                                         &error);

  if (error)
    g_error ("Failed to load portals: %s. Aborting...", error->message);
}

/**
 * gcal_context_new: (skip)
 *
 * Create a new GcalContext. Only one context must be
 * available during the application lifetime.
 *
 * Returns: (transfer full): a #GcalContext
 */
GcalContext*
gcal_context_new (void)
{
  return g_object_new (GCAL_TYPE_CONTEXT, NULL);
}

/**
 * gcal_context_get_clock:
 *
 * Retrieves the #GcalClock from @self.
 *
 * Returns: (transfer none): a #GcalClock
 */
GcalClock*
gcal_context_get_clock (GcalContext *self)
{
  g_return_val_if_fail (GCAL_IS_CONTEXT (self), NULL);

  return self->clock;
}

/**
 * gcal_context_get_manager:
 *
 * Retrieves the #GcalManager from @self.
 *
 * Returns: (transfer none): a #GcalManager
 */
GcalManager*
gcal_context_get_manager (GcalContext *self)
{
  g_return_val_if_fail (GCAL_IS_CONTEXT (self), NULL);

  return self->manager;
}

/**
 * gcal_context_get_search_engine:
 *
 * Retrieves the #GcalSearchEngine from @self.
 *
 * Returns: (transfer none): a #GcalSearchEngine
 */
GcalSearchEngine*
gcal_context_get_search_engine (GcalContext *self)
{
  g_return_val_if_fail (GCAL_IS_CONTEXT (self), NULL);

  return self->search_engine;
}

/**
 * gcal_context_get_settings:
 *
 * Retrieves the #GSettings from @self.
 *
 * Returns: (transfer none): a #GSettings
 */
GSettings*
gcal_context_get_settings (GcalContext *self)
{
  g_return_val_if_fail (GCAL_IS_CONTEXT (self), NULL);

  return self->settings;
}

/**
 * gcal_context_get_time_format:
 *
 * Retrieves the #GcalTimeFormat from @self.
 *
 * Returns: a #GcalTimeFormat
 */
GcalTimeFormat
gcal_context_get_time_format (GcalContext *self)
{
  g_return_val_if_fail (GCAL_IS_CONTEXT (self), 0);

  return self->time_format;
}

/**
 * gcal_context_get_timezone:
 *
 * Retrieves the system timezone.
 *
 * Returns: (transfer none): a #GTimeZone
 */
GTimeZone*
gcal_context_get_timezone (GcalContext *self)
{
  g_return_val_if_fail (GCAL_IS_CONTEXT (self), NULL);

  return gcal_time_zone_monitor_get_timezone (self->timezone_monitor);
}

/**
 * gcal_context_get_weather_service:
 *
 * Retrieves the #GcalWeatherService from @self.
 *
 * Returns: (transfer none): a #GcalWeatherService
 */
GcalWeatherService*
gcal_context_get_weather_service (GcalContext *self)
{
  g_return_val_if_fail (GCAL_IS_CONTEXT (self), NULL);

  return self->weather_service;
}

void
gcal_context_startup (GcalContext *self)
{
  g_return_if_fail (GCAL_IS_CONTEXT (self));

  if (read_time_format (self))
    {
      g_signal_connect (self->settings_portal,
                        "g-signal",
                        G_CALLBACK (on_portal_proxy_signal_cb),
                        self);
    }

  gcal_manager_startup (self->manager);
}
