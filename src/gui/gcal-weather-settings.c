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
  GtkEditable        *weather_location_entry;

  GWeatherLocation   *location;

  GcalContext        *context;
};


static void          on_weather_location_searchbox_changed_cb    (GtkEntry            *entry,
                                                                  GcalWeatherSettings *self);

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
  GSettings *settings;
  gboolean show_weather;
  gboolean auto_location;

  GCAL_ENTRY;

  if (!self->context)
    GCAL_RETURN ();

  settings = gcal_context_get_settings (self->context);
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
      gtk_editable_set_text (self->weather_location_entry, location_name);
      gtk_widget_add_css_class (GTK_WIDGET (self->weather_location_entry), "error");
    }
  else
    {
      g_autoptr (GWeatherLocation) weather_location = NULL;
      GWeatherLocation *world;

      world = gweather_location_get_world ();
      weather_location = location ? gweather_location_deserialize (world, location) : NULL;

      self->location = weather_location ? g_object_ref (weather_location) : NULL;
      gtk_editable_set_text (self->weather_location_entry,
                             self->location ? gweather_location_get_name (self->location) : "");
    }

  g_signal_handlers_unblock_by_func (self->show_weather_switch, on_show_weather_changed_cb, self);
  g_signal_handlers_unblock_by_func (self->weather_auto_location_switch, on_weather_auto_location_changed_cb, self);
  g_signal_handlers_unblock_by_func (self->weather_location_entry, on_weather_location_searchbox_changed_cb, self);

  GCAL_EXIT;
}

static void
save_weather_settings (GcalWeatherSettings *self)
{
  GSettings *settings;
  GVariant *value;
  GVariant *vlocation;
  gboolean res;

  GCAL_ENTRY;

  if (!self->context)
    GCAL_RETURN ();

  vlocation = self->location ? gweather_location_serialize (self->location) : NULL;

  settings = gcal_context_get_settings (self->context);
  value = g_variant_new ("(bbsmv)",
                         gtk_switch_get_active (self->show_weather_switch),
                         gtk_switch_get_active (self->weather_auto_location_switch),
                         gtk_editable_get_text (self->weather_location_entry),
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
  /*
   * NOTE: This check feels shabby. However, I couldn't find a better
   * one without iterating the model. has-custom-text does not work
   * properly. Lets go with it for now.
   */
  if (self->location && gweather_location_get_name (self->location))
    return g_object_ref (self->location);

  return NULL;
}

static void
manage_weather_service (GcalWeatherSettings *self)
{
  GcalWeatherService *weather_service;

  GCAL_ENTRY;

  weather_service = gcal_context_get_weather_service (self->context);

  if (gtk_switch_get_active (self->show_weather_switch))
    {
      g_autoptr (GWeatherLocation) location = NULL;

      if (!gtk_switch_get_active (self->weather_auto_location_switch))
        {
          location = get_checked_fixed_location (self);

          if (!location)
            g_warning ("Unknown location '%s' selected", gtk_editable_get_text (self->weather_location_entry));
        }

      gcal_weather_service_set_location (weather_service, location);
      gcal_weather_service_activate (weather_service);
    }
  else
    {
      gcal_weather_service_deactivate (weather_service);
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
on_weather_location_searchbox_changed_cb (GtkEntry            *entry,
                                          GcalWeatherSettings *self)
{
  GWeatherLocation *location;
  gboolean auto_location;

  save_weather_settings (self);

  auto_location = gtk_switch_get_active (self->weather_auto_location_switch);
  location = get_checked_fixed_location (self);

  if (!location && !auto_location)
    {
      gtk_widget_add_css_class (GTK_WIDGET (self->weather_location_entry), "error");
    }
  else
    {
      gtk_widget_remove_css_class (GTK_WIDGET (self->weather_location_entry), "error");
      manage_weather_service (self);
      g_object_unref (location);
    }
}


/*
 * GObject overrides
 */

static void
gcal_weather_settings_finalize (GObject *object)
{
  GcalWeatherSettings *self = (GcalWeatherSettings *)object;

  g_clear_object (&self->location);
  g_clear_object (&self->context);

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
      g_assert (self->context == NULL);
      self->context = g_value_dup_object (value);

      load_weather_settings (self);
      update_menu_weather_sensitivity (self);
      manage_weather_service (self);
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
                                                  G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/gui/gcal-weather-settings.ui");

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
