/* gcal-night-light-monitor.c
 *
 * Copyright 2018 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#include "gcal-debug.h"
#include "gcal-night-light-monitor.h"

struct _GcalNightLightMonitor
{
  GObject             parent;

  GcalContext        *context;

  GDBusProxy         *night_light_proxy;
};

G_DEFINE_TYPE (GcalNightLightMonitor, gcal_night_light_monitor, G_TYPE_OBJECT)

enum
{
  PROP_0,
  PROP_CONTEXT,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];


/*
 * Callbacks
 */

static void
on_night_light_proxy_properties_changed_cb (GcalNightLightMonitor *self,
                                            GVariant              *properties,
                                            const gchar * const   *invalidated,
                                            GDBusProxy            *proxy)
{
  GSettings *settings;

  g_assert (GCAL_IS_NIGHT_LIGHT_MONITOR (self));
  g_assert (G_IS_DBUS_PROXY (proxy));

  settings = gcal_context_get_settings (self->context);

  if (g_settings_get_boolean (settings, "follow-night-light"))
    {
      g_autoptr (GVariant) activev = NULL;
      gboolean active;

      activev = g_dbus_proxy_get_cached_property (proxy, "NightLightActive");
      active = g_variant_get_boolean (activev);

      if (active)
        hdy_style_manager_set_color_scheme (hdy_style_manager_get_default (),
                                            HDY_COLOR_SCHEME_PREFER_DARK);
      else
        hdy_style_manager_set_color_scheme (hdy_style_manager_get_default (),
                                            HDY_COLOR_SCHEME_PREFER_LIGHT);
    }
}


/*
 * GObject overrides
 */

static void
gcal_night_light_monitor_finalize (GObject *object)
{
  GcalNightLightMonitor *self = (GcalNightLightMonitor *)object;

  g_clear_object (&self->context);
  g_clear_object (&self->night_light_proxy);

  G_OBJECT_CLASS (gcal_night_light_monitor_parent_class)->finalize (object);
}

static void
gcal_night_light_monitor_constructed (GObject *object)
{
  GcalNightLightMonitor *self = (GcalNightLightMonitor *)object;
  g_autoptr (GDBusConnection) connection = NULL;
  g_autoptr (GDBusProxy) proxy = NULL;

  GCAL_ENTRY;

  G_OBJECT_CLASS (gcal_night_light_monitor_parent_class)->constructed (object);

  connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);

  if (!connection)
    GCAL_RETURN ();

  proxy = g_dbus_proxy_new_sync (connection,
                                 G_DBUS_PROXY_FLAGS_GET_INVALIDATED_PROPERTIES,
                                 NULL,
                                 "org.gnome.SettingsDaemon.Color",
                                 "/org/gnome/SettingsDaemon/Color",
                                 "org.gnome.SettingsDaemon.Color",
                                 NULL,
                                 NULL);

  if (!proxy)
    GCAL_RETURN ();

  g_debug ("Created Night Light monitor");

  g_signal_connect_object (proxy,
                           "g-properties-changed",
                           G_CALLBACK (on_night_light_proxy_properties_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  self->night_light_proxy = g_steal_pointer (&proxy);

  on_night_light_proxy_properties_changed_cb (self, NULL, NULL, self->night_light_proxy);

  GCAL_EXIT;
}

static void
gcal_night_light_monitor_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  GcalNightLightMonitor *self = GCAL_NIGHT_LIGHT_MONITOR (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_value_set_object (value, self->context);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_night_light_monitor_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  GcalNightLightMonitor *self = GCAL_NIGHT_LIGHT_MONITOR (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_assert (self->context == NULL);
      self->context = g_value_dup_object (value);
      g_object_notify_by_pspec (object, pspec);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_night_light_monitor_class_init (GcalNightLightMonitorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gcal_night_light_monitor_finalize;
  object_class->constructed = gcal_night_light_monitor_constructed;
  object_class->get_property = gcal_night_light_monitor_get_property;
  object_class->set_property = gcal_night_light_monitor_set_property;

  properties[PROP_CONTEXT] = g_param_spec_object ("context",
                                                  "Context",
                                                  "Context",
                                                  GCAL_TYPE_CONTEXT,
                                                  G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gcal_night_light_monitor_init (GcalNightLightMonitor *self)
{
}

GcalNightLightMonitor*
gcal_night_light_monitor_new (GcalContext *context)
{
  return g_object_new (GCAL_TYPE_NIGHT_LIGHT_MONITOR,
                       "context", context,
                       NULL);
}
