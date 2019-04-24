/* gcal-weather-settings.c
 *
 * Copyright © 2018 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#define G_LOG_DOMAIN "GcalWeatherSettings"

#include "gcal-context.h"
#include "gcal-debug.h"
#include "gcal-manager.h"
#include "gcal-utils.h"
#include "gcal-weather-service.h"
#include "gcal-weather-settings.h"

struct _GcalWeatherSettings
{
  GtkBox              parent;

  GtkSwitch          *show_weather_switch;
  GtkSwitch          *weather_auto_location_switch;
  GtkWidget          *weather_location_entry;

  GcalContext        *context;

  GcalWeatherService *weather_service;
};


static void          on_weather_location_searchbox_changed_cb    (GWeatherLocationEntry *entry,
                                                                  GcalWeatherSettings   *self);

static void          on_show_weather_changed_cb                  (GtkSwitch           *wswitch,
                                                                  GParamSpec          *pspec,
                                                                  GcalWeatherSettings *self);

static void          on_weather_auto_location_changed_cb         (GtkSwitch           *lswitch,
                                                                  GParamSpec          *pspec,
                                                                  GcalWeatherSettings *self);

G_DEFINE_TYPE (GcalWeatherSettings, gcal_weather_settings, GTK_TYPE_BOX)

enum
{
  PROP_0,
  PROP_CONTEXT,
  PROP_WEATHER_SERVICE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS] = { NULL, };


/*
 * Auxiliary methods
 */

static void
load_weather_settings (GcalWeatherSettings *self)
{
  g_autoptr (GVariant) location = NULL;
  g_autoptr (GVariant) value = NULL;
  g_autofree gchar *location_name = NULL;
  GcalManager *manager;
  GSettings *settings;
  gboolean show_weather;
  gboolean auto_location;

  GCAL_ENTRY;

  if (!self->context)
    GCAL_RETURN ();

  manager = gcal_context_get_manager (self->context);
  settings = gcal_manager_get_settings (manager);
  value = g_settings_get_value (settings, "weather-settings");

  g_variant_get (value, "(bbsmv)",
                 &show_weather,
                 &auto_location,
                 &location_name,
                 &location);

  g_signal_handlers_block_by_func (self->show_weather_switch, on_show_weather_changed_cb, self);
  g_signal_handlers_block_by_func (self->weather_auto_location_switch, on_weather_auto_location_changed_cb, self);
  g_signal_handlers_block_by_func (self->weather_location_entry, on_weather_location_searchbox_changed_cb, self);

  gtk_switch_set_active (self->show_weather_switch, show_weather);
  gtk_switch_set_active (self->weather_auto_location_switch, auto_location);

  if (!location && !auto_location)
    {
      GtkStyleContext *context;

      context = gtk_widget_get_style_context (GTK_WIDGET (self->weather_location_entry));
      gtk_entry_set_text (GTK_ENTRY (self->weather_location_entry), location_name);
      gtk_style_context_add_class (context, "error");
    }
  else
    {
      g_autoptr (GWeatherLocation) weather_location;
      GWeatherLocation *world;

      world = gweather_location_get_world ();
      weather_location = location ? gweather_location_deserialize (world, location) : NULL;

      gweather_location_entry_set_location (GWEATHER_LOCATION_ENTRY (self->weather_location_entry), weather_location);
    }

  g_signal_handlers_unblock_by_func (self->show_weather_switch, on_show_weather_changed_cb, self);
  g_signal_handlers_unblock_by_func (self->weather_auto_location_switch, on_weather_auto_location_changed_cb, self);
  g_signal_handlers_unblock_by_func (self->weather_location_entry, on_weather_location_searchbox_changed_cb, self);

  GCAL_EXIT;
}

static void
save_weather_settings (GcalWeatherSettings *self)
{
  g_autoptr (GWeatherLocation) location = NULL;
  GcalManager *manager;
  GSettings *settings;
  GVariant *value;
  GVariant *vlocation;
  gboolean res;

  GCAL_ENTRY;

  if (!self->context)
    GCAL_RETURN ();

  location = gweather_location_entry_get_location (GWEATHER_LOCATION_ENTRY (self->weather_location_entry));
  vlocation = location ? gweather_location_serialize (location) : NULL;

  manager = gcal_context_get_manager (self->context);
  settings = gcal_manager_get_settings (manager);
  value = g_variant_new ("(bbsmv)",
                         gtk_switch_get_active (self->show_weather_switch),
                         gtk_switch_get_active (self->weather_auto_location_switch),
                         gtk_entry_get_text (GTK_ENTRY (self->weather_location_entry)),
                         vlocation);

  res = g_settings_set_value (settings, "weather-settings", value);

  if (!res)
    g_warning ("Could not persist weather settings");

  GCAL_EXIT;
}

static void
update_menu_weather_sensitivity (GcalWeatherSettings *self)
{
  gboolean weather_enabled;
  gboolean autoloc_enabled;

  weather_enabled = gtk_switch_get_active (self->show_weather_switch);
  autoloc_enabled = gtk_switch_get_active (self->weather_auto_location_switch);

  gtk_widget_set_sensitive (GTK_WIDGET (self->weather_auto_location_switch), weather_enabled);
  gtk_widget_set_sensitive (GTK_WIDGET (self->weather_location_entry), weather_enabled && !autoloc_enabled);
}


static GWeatherLocation*
get_checked_fixed_location (GcalWeatherSettings *self)
{
  g_autoptr (GWeatherLocation) location;

  location = gweather_location_entry_get_location (GWEATHER_LOCATION_ENTRY (self->weather_location_entry));

  /*
   * NOTE: This check feels shabby. However, I couldn't find a better
   * one without iterating the model. has-custom-text does not work
   * properly. Lets go with it for now.
   */
  if (location && gweather_location_get_name (location))
    return g_steal_pointer (&location);

  return NULL;
}

static void
manage_weather_service (GcalWeatherSettings *self)
{
  GCAL_ENTRY;

  if (gtk_switch_get_active (self->show_weather_switch))
    {
      g_autoptr (GWeatherLocation) location = NULL;

      if (!gtk_switch_get_active (self->weather_auto_location_switch))
        {
          location = get_checked_fixed_location (self);

          if (!location)
            g_warning ("Unknown location '%s' selected", gtk_entry_get_text (GTK_ENTRY (self->weather_location_entry)));
        }

      gcal_weather_service_run (self->weather_service, location);
    }
  else
    {
      gcal_weather_service_stop (self->weather_service);
    }

  GCAL_EXIT;
}


/*
 * Callbacks
 */

static void
on_show_weather_changed_cb (GtkSwitch           *wswitch,
                            GParamSpec          *pspec,
                            GcalWeatherSettings *self)
{
  save_weather_settings (self);
  update_menu_weather_sensitivity (self);
  manage_weather_service (self);
}


static void
on_weather_auto_location_changed_cb (GtkSwitch           *lswitch,
                                     GParamSpec          *pspec,
                                     GcalWeatherSettings *self)
{
  save_weather_settings (self);
  update_menu_weather_sensitivity (self);
  manage_weather_service (self);
}

static void
on_weather_location_searchbox_changed_cb (GWeatherLocationEntry *entry,
                                          GcalWeatherSettings   *self)
{
  GtkStyleContext  *context;
  GWeatherLocation *location;
  gboolean auto_location;

  save_weather_settings (self);

  context = gtk_widget_get_style_context (self->weather_location_entry);
  auto_location = gtk_switch_get_active (self->weather_auto_location_switch);
  location = get_checked_fixed_location (self);

  if (!location && !auto_location)
    {
      gtk_style_context_add_class (context, "error");
    }
  else
    {
      gtk_style_context_remove_class (context, "error");
      manage_weather_service (self);
      gweather_location_unref (location);
    }
}



/*
 * GObject overrides
 */

static void
gcal_weather_settings_finalize (GObject *object)
{
  GcalWeatherSettings *self = (GcalWeatherSettings *)object;

  g_clear_object (&self->weather_service);

  G_OBJECT_CLASS (gcal_weather_settings_parent_class)->finalize (object);
}

static void
gcal_weather_settings_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  GcalWeatherSettings *self = GCAL_WEATHER_SETTINGS (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_value_set_object (value, self->context);
      break;

    case PROP_WEATHER_SERVICE:
      g_value_set_object (value, self->weather_service);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_weather_settings_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  GcalWeatherSettings *self = GCAL_WEATHER_SETTINGS (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      if (g_set_object (&self->context, g_value_get_object (value)))
        {
          load_weather_settings (self);
          update_menu_weather_sensitivity (self);
          manage_weather_service (self);

          g_object_notify_by_pspec (object, properties[PROP_CONTEXT]);
        }
      break;

    case PROP_WEATHER_SERVICE:
      gcal_weather_settings_set_weather_service (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_weather_settings_class_init (GcalWeatherSettingsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gcal_weather_settings_finalize;
  object_class->get_property = gcal_weather_settings_get_property;
  object_class->set_property = gcal_weather_settings_set_property;

  properties[PROP_CONTEXT] = g_param_spec_object ("context",
                                                  "Context",
                                                  "Context",
                                                  GCAL_TYPE_CONTEXT,
                                                  G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_WEATHER_SERVICE] = g_param_spec_object ("weather-service",
                                                          "The weather service object",
                                                          "The weather service object",
                                                          GCAL_TYPE_WEATHER_SERVICE,
                                                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/weather-settings.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalWeatherSettings, show_weather_switch);
  gtk_widget_class_bind_template_child (widget_class, GcalWeatherSettings, weather_auto_location_switch);
  gtk_widget_class_bind_template_child (widget_class, GcalWeatherSettings, weather_location_entry);

  gtk_widget_class_bind_template_callback (widget_class, on_show_weather_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_weather_auto_location_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_weather_location_searchbox_changed_cb);
}

static void
gcal_weather_settings_init (GcalWeatherSettings *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GcalWeatherService*
gcal_weather_settings_get_weather_service (GcalWeatherSettings *self)
{
  g_return_val_if_fail (GCAL_IS_WEATHER_SETTINGS (self), NULL);

  return self->weather_service;
}

void
gcal_weather_settings_set_weather_service (GcalWeatherSettings *self,
                                           GcalWeatherService  *service)
{
  g_return_if_fail (GCAL_IS_WEATHER_SETTINGS (self));

  GCAL_ENTRY;

  if (!g_set_object (&self->weather_service, service))
    GCAL_RETURN ();

  /* Recover weather settings */
  load_weather_settings (self);
  update_menu_weather_sensitivity (self);
  manage_weather_service (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_WEATHER_SERVICE]);

  GCAL_EXIT;
}
