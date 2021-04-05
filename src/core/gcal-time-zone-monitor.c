/* gcal-time-zone-monitor.c
 *
 * Copyright 2019-2020 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#define G_LOG_DOMAIN "GcalTimeZoneMonitor"

#include "gcal-debug.h"
#include "gcal-time-zone-monitor.h"

#include <gio/gio.h>

struct _GcalTimeZoneMonitor
{
  GObject             parent;

  GTimeZone          *timezone;

  GDBusProxy         *timedate1_proxy;
};

G_DEFINE_TYPE (GcalTimeZoneMonitor, gcal_time_zone_monitor, G_TYPE_OBJECT)

enum
{
  PROP_0,
  PROP_TIMEZONE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];


/*
 * Callbacks
 */

static void
on_timedate1_proxy_properties_changed_cb (GcalTimeZoneMonitor *self,
                                          GVariant            *props,
                                          const gchar * const *invalidated,
                                          GDBusProxy          *proxy)
{
  g_autoptr (GVariant) timezone_variant = NULL;
  const gchar *timezone_identifier = NULL;

  g_assert (GCAL_IS_TIME_ZONE_MONITOR (self));
  g_assert (G_IS_DBUS_PROXY (proxy));

  timezone_variant = g_dbus_proxy_get_cached_property (proxy, "Timezone");

  /*
   * NULL timezone_variant means NULL timezone_identifier, which falls back
   * to regularly checking /etc/localtime
   */
  if (timezone_variant)
    timezone_identifier = g_variant_get_string (timezone_variant, NULL);

  g_clear_pointer (&self->timezone, g_time_zone_unref);
  self->timezone = g_time_zone_new_identifier (timezone_identifier);

  g_debug ("System timezone is %s", g_time_zone_get_identifier (self->timezone));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TIMEZONE]);
}


/*
 * GObject overrides
 */

static void
gcal_time_zone_monitor_finalize (GObject *object)
{
  GcalTimeZoneMonitor *self = (GcalTimeZoneMonitor *)object;

  g_clear_pointer (&self->timezone, g_time_zone_unref);
  g_clear_object (&self->timedate1_proxy);

  G_OBJECT_CLASS (gcal_time_zone_monitor_parent_class)->finalize (object);
}

static void
gcal_time_zone_monitor_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GcalTimeZoneMonitor *self = GCAL_TIME_ZONE_MONITOR (object);

  switch (prop_id)
    {
    case PROP_TIMEZONE:
      g_value_set_boxed (value, self->timezone);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_time_zone_monitor_class_init (GcalTimeZoneMonitorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gcal_time_zone_monitor_finalize;
  object_class->get_property = gcal_time_zone_monitor_get_property;

  properties[PROP_TIMEZONE] = g_param_spec_boxed ("timezone",
                                                  "Timezone",
                                                  "System timezone",
                                                  G_TYPE_TIME_ZONE,
                                                  G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gcal_time_zone_monitor_init (GcalTimeZoneMonitor *self)
{
  g_autoptr (GDBusConnection) connection = NULL;
  g_autoptr (GDBusProxy) proxy = NULL;

  GCAL_ENTRY;

  self->timezone = g_time_zone_new_local ();

  connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, NULL);

  if (!connection)
    GCAL_RETURN ();

  proxy = g_dbus_proxy_new_sync (connection,
                                 G_DBUS_PROXY_FLAGS_GET_INVALIDATED_PROPERTIES,
                                 NULL,
                                 "org.freedesktop.timedate1",
                                 "/org/freedesktop/timedate1",
                                 "org.freedesktop.timedate1",
                                 NULL,
                                 NULL);

  if (!proxy)
    GCAL_RETURN ();

  g_debug ("Created Time Zone monitor");

  g_signal_connect_object (proxy,
                           "g-properties-changed",
                           G_CALLBACK (on_timedate1_proxy_properties_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  self->timedate1_proxy = g_steal_pointer (&proxy);

  on_timedate1_proxy_properties_changed_cb (self, NULL, NULL, self->timedate1_proxy);

  GCAL_EXIT;
}

GcalTimeZoneMonitor*
gcal_time_zone_monitor_new (void)
{
  return g_object_new (GCAL_TYPE_TIME_ZONE_MONITOR, NULL);
}

GTimeZone*
gcal_time_zone_monitor_get_timezone (GcalTimeZoneMonitor *self)
{
  g_return_val_if_fail (GCAL_IS_TIME_ZONE_MONITOR (self), NULL);

  return self->timezone;
}
