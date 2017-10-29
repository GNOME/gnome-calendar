/* -*- mode: c; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gcal-view.c
 *
 * Copyright (C) 2015 - Erick Pérez Castellanos
 *               2016 - Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#define G_LOG_DOMAIN "GcalView"

#include "gcal-view.h"
#include "gcal-utils.h"

#include <glib.h>


G_DEFINE_INTERFACE (GcalView, gcal_view, GTK_TYPE_WIDGET)

static void
gcal_view_default_init (GcalViewInterface *iface)
{
  /**
   * GcalView::active-date:
   *
   * The active date of the view.
   */
  g_object_interface_install_property (iface,
                                       g_param_spec_boxed ("active-date",
                                                           "The active date",
                                                           "The active/selecetd date in the view",
                                                           ICAL_TIME_TYPE,
                                                           G_PARAM_READWRITE));

  /**
   * GcalView::manager:
   *
   * The #GcalManager of the view.
   */
  g_object_interface_install_property (iface,
                                       g_param_spec_object ("manager",
                                                            "The manager",
                                                            "The manager of the view",
                                                            GCAL_TYPE_MANAGER,
                                                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GcalView::weather-service:
   *
   * The #GcalWeatherService of the view.
   */
  g_object_interface_install_property (iface,
                                       g_param_spec_object ("weather-service",
                                                            "The weather service",
                                                            "The weather service of the view",
                                                            GCAL_TYPE_WEATHER_SERVICE,
                                                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GcalView::create-event:
   *
   * Emitted when the view wants to create an event.
   */
  g_signal_new ("create-event",
                GCAL_TYPE_VIEW,
                G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET (GcalViewInterface, create_event),
                NULL, NULL, NULL,
                G_TYPE_NONE,
                4,
                G_TYPE_POINTER,
                G_TYPE_POINTER,
                G_TYPE_DOUBLE,
                G_TYPE_DOUBLE);

  /**
   * GcalView::create-event-detailed:
   *
   * Emitted when the view wants to create an event and immediately
   * edit it.
   */
  g_signal_new ("create-event-detailed",
                GCAL_TYPE_VIEW,
                G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET (GcalViewInterface, create_event_detailed),
                NULL, NULL, NULL,
                G_TYPE_NONE, 2, G_TYPE_POINTER, G_TYPE_POINTER);
}

/**
 * gcal_view_set_date:
 * @view: a #GcalView
 * @date: an #icaltimetype
 *
 * Sets the date of @view.
 */
void
gcal_view_set_date (GcalView     *view,
                    icaltimetype *date)
{
  g_return_if_fail (GCAL_IS_VIEW (view));
  g_return_if_fail (GCAL_VIEW_GET_IFACE (view)->set_date);

  GCAL_VIEW_GET_IFACE (view)->set_date (view, date);
}

/**
 * gcal_view_get_manager:
 * @self: a #GcalView
 *
 * Retrieves the #GcalManager instance from @self.
 *
 * Returns: (transfer none): a #GcalManager
 */
GcalManager*
gcal_view_get_manager (GcalView *self)
{
  GcalManager *manager;

  g_return_val_if_fail (GCAL_IS_VIEW (self), NULL);

  g_object_get (self, "manager", &manager, NULL);
  g_object_unref (manager);

  return manager;
}

/**
 * gcal_view_get_weather_service:
 * @self: The #GcalView instance.
 * @service: (nullable): The weather service to query.
 *
 * Sets the service to query for weather reports.
 */
GcalWeatherService*
gcal_view_get_weather_service (GcalView *self)
{
  GcalWeatherService *service; /* unowned */

  g_return_val_if_fail (GCAL_IS_VIEW (self), NULL);

  g_object_get (self, "weather-service", &service, NULL);
  g_object_unref (service);

  return service;
}

/**
 * gcal_view_set_weather_service:
 * @self: The #GcalView instance.
 *
 * Returns: (transfer none): The internal weather service object.
 */
void
gcal_view_set_weather_service (GcalView           *self,
                               GcalWeatherService *service)
{
  g_return_if_fail (GCAL_IS_VIEW (self));
  g_return_if_fail (service == NULL || GCAL_IS_WEATHER_SERVICE (service));

  g_object_set (self, "weather-service", &service, NULL);
}

/**
 * gcal_view_set_weather_service_impl_helper:
 * @old_service: The views weather service field.
 * @new_service: (nullable): The new weather service to use.
 * @update_func: The function to call after updating weather.
 * @weather_changed_cb: The views "weather-changed" handler
 * @data: The data to pass to @update_func and @weather_changed_cb
 *
 * Internal implementation helper for
 * gcal_view_set_weather_service().
 */
void
gcal_view_set_weather_service_impl_helper (GcalWeatherService  **old_service,
                                           GcalWeatherService   *new_service,
                                           GcalWeatherUpdateFunc update_func,
                                           GCallback             weather_changed_cb,
                                           GtkWidget            *data)
{
  g_return_if_fail (old_service != NULL);
  g_return_if_fail (*old_service == NULL || GCAL_IS_WEATHER_SERVICE (*old_service));
  g_return_if_fail (new_service == NULL || GCAL_IS_WEATHER_SERVICE (new_service));
  g_return_if_fail (update_func != NULL);
  g_return_if_fail (weather_changed_cb != NULL);
  g_return_if_fail (GTK_IS_WIDGET (data));

  if (*old_service != new_service)
    {
      if (*old_service != NULL)
        g_signal_handlers_disconnect_by_func (*old_service,
                                              (GCallback) weather_changed_cb,
                                              data);

      g_set_object (old_service, new_service);

      if (*old_service != NULL)
        g_signal_connect (*old_service,
                          "weather-changed",
                          (GCallback) weather_changed_cb,
                          data);

      update_func (data);
      g_object_notify (G_OBJECT (data), "weather-service");
    }
}

/**
 * gcal_view_get_date:
 * @view: a #GcalView
 *
 * Retrieves the date of @view.
 *
 * Returns: (transfer none): an #icaltimetype.
 */
icaltimetype*
gcal_view_get_date (GcalView *view)
{
  g_return_val_if_fail (GCAL_IS_VIEW (view), NULL);
  g_return_val_if_fail (GCAL_VIEW_GET_IFACE (view)->get_date, NULL);

  return GCAL_VIEW_GET_IFACE (view)->get_date (view);
}

/**
 * gcal_view_clear_marks:
 * @view: a #GcalView
 *
 * Clear any marking the view had drawn
 **/
void
gcal_view_clear_marks (GcalView *view)
{
  g_return_if_fail (GCAL_IS_VIEW (view));
  g_return_if_fail (GCAL_VIEW_GET_IFACE (view)->clear_marks);

  GCAL_VIEW_GET_IFACE (view)->clear_marks (view);
}

/**
 * gcal_view_get_children_by_uuid:
 * @view: a #GcalView
 * @uuid: The unique id of an event
 *
 * Returns a list with every event that has the passed uuid
 *
 * Returns: (transfer full): a {@link GList} instance
 **/
GList*
gcal_view_get_children_by_uuid (GcalView              *view,
                                GcalRecurrenceModType  mod,
                                const gchar           *uuid)
{
  g_return_val_if_fail (GCAL_IS_VIEW (view), NULL);
  g_return_val_if_fail (GCAL_VIEW_GET_IFACE (view)->get_children_by_uuid, NULL);

  return GCAL_VIEW_GET_IFACE (view)->get_children_by_uuid (view, mod, uuid);
}
