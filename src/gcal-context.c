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

struct _GcalContext
{
  GObject             parent;

  GcalClock          *clock;
  GoaClient          *goa_client;
  GcalManager        *manager;
  GSettings          *settings;
  GcalWeatherService *weather_service;
};

G_DEFINE_TYPE (GcalContext, gcal_context, G_TYPE_OBJECT)

enum
{
  PROP_0,
  PROP_CLOCK,
  PROP_GOA_CLIENT,
  PROP_MANAGER,
  PROP_SETTINGS,
  PROP_WEATHER_SERVICE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];


/*
 * GObject overrides
 */

static void
gcal_context_finalize (GObject *object)
{
  GcalContext *self = (GcalContext *)object;

  g_clear_object (&self->clock);
  g_clear_object (&self->goa_client);
  g_clear_object (&self->manager);
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

    case PROP_GOA_CLIENT:
      g_value_set_object (value, self->goa_client);
      break;

    case PROP_MANAGER:
      g_value_set_object (value, self->manager);
      break;

    case PROP_SETTINGS:
      g_value_set_object (value, self->settings);
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
    case PROP_GOA_CLIENT:
    case PROP_MANAGER:
    case PROP_SETTINGS:
    case PROP_WEATHER_SERVICE:
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_context_class_init (GcalContextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gcal_context_finalize;
  object_class->get_property = gcal_context_get_property;
  object_class->set_property = gcal_context_set_property;

  properties[PROP_CLOCK] = g_param_spec_object ("clock",
                                                "Wall clock",
                                                "Main clock driving the calendar timeline",
                                                GCAL_TYPE_CLOCK,
                                                G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  properties[PROP_GOA_CLIENT] = g_param_spec_object ("goa-client",
                                                     "Online Accounts client",
                                                     "Online Accounts client",
                                                     GOA_TYPE_CLIENT,
                                                     G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  properties[PROP_MANAGER] = g_param_spec_object ("manager",
                                                  "Data manager",
                                                  "Data manager of the application",
                                                  GCAL_TYPE_MANAGER,
                                                  G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  properties[PROP_SETTINGS] = g_param_spec_object ("settings",
                                                   "GNOME Calendar settings",
                                                   "GNOME Calendar settings",
                                                   G_TYPE_SETTINGS,
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
  self->clock = gcal_clock_new ();
  self->goa_client = goa_client_new_sync (NULL, NULL);
  self->manager = gcal_manager_new ();
  self->settings = g_settings_new ("org.gnome.calendar");
  self->weather_service = gcal_weather_service_new ();
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
 * gcal_context_get_goa_client:
 *
 * Retrieves the #GoaClient from @self.
 *
 * Returns: (transfer none): a #GoaClient
 */
GoaClient*
gcal_context_get_goa_client (GcalContext *self)
{
  g_return_val_if_fail (GCAL_IS_CONTEXT (self), NULL);

  return self->goa_client;
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
