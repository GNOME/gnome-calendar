/* -*- mode: c; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gcal-subscriber.c
 *
 * Copyright (C) 2014 - Erick Pérez Castellanos
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
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

#include "gcal-subscriber.h"

#include "gcal-view.h"
#include "gcal-event-widget.h"

static void           gcal_data_model_subscriber_interface_init (ECalDataModelSubscriberInterface *iface);

static void           gcal_subscriber_component_added           (ECalDataModelSubscriber *subscriber,
                                                                 ECalClient              *client,
                                                                 ECalComponent           *comp);

static void           gcal_subscriber_component_modified        (ECalDataModelSubscriber *subscriber,
                                                                 ECalClient              *client,
                                                                 ECalComponent           *comp);

static void           gcal_subscriber_component_removed         (ECalDataModelSubscriber *subscriber,
                                                                 ECalClient              *client,
                                                                 const gchar             *uid,
                                                                 const gchar             *rid);

static void           gcal_subscriber_freeze                    (ECalDataModelSubscriber *subscriber);

static void           gcal_subscriber_thaw                      (ECalDataModelSubscriber *subscriber);

G_DEFINE_TYPE_WITH_CODE (GcalSubscriber,
                         gcal_subscriber,
                         GTK_TYPE_CONTAINER,
                         G_IMPLEMENT_INTERFACE (E_TYPE_CAL_DATA_MODEL_SUBSCRIBER,
                                                gcal_data_model_subscriber_interface_init));


static void
gcal_subscriber_class_init (GcalSubscriberClass *klass)
{
  ;
}

static void
gcal_subscriber_init (GcalSubscriber *self)
{
  ;
}

static void
gcal_data_model_subscriber_interface_init (ECalDataModelSubscriberInterface *iface)
{
  iface->component_added = gcal_subscriber_component_added;
  iface->component_modified = gcal_subscriber_component_modified;
  iface->component_removed = gcal_subscriber_component_removed;
  iface->freeze = gcal_subscriber_freeze;
  iface->thaw = gcal_subscriber_thaw;
}

/* ECalDataModelSubscriber interface API */
static void
gcal_subscriber_component_added (ECalDataModelSubscriber *subscriber,
                                 ECalClient              *client,
                                 ECalComponent           *comp)
{
  GtkWidget *event;
  GcalEventData *data;

  data = g_new0 (GcalEventData, 1);
  data->source = e_client_get_source (E_CLIENT (client));
  data->event_component = e_cal_component_clone (comp);

  event = gcal_event_widget_new_from_data (data);
  gcal_event_widget_set_read_only (GCAL_EVENT_WIDGET (event), e_client_is_readonly (E_CLIENT (client)));

  g_free (data);

  gtk_widget_show (event);
  gtk_container_add (GTK_CONTAINER (subscriber), event);
}

static void
gcal_subscriber_component_modified (ECalDataModelSubscriber *subscriber,
                                    ECalClient              *client,
                                    ECalComponent           *comp)
{
  GtkWidget *event;
  GtkWidget *widget;
  GcalEventData *data;

  data = g_new0 (GcalEventData, 1);
  data->source = e_client_get_source (E_CLIENT (client));
  data->event_component = e_cal_component_clone (comp);

  event = gcal_event_widget_new_from_data (data);
  g_free (data);

  widget =
    gcal_view_get_by_uuid (
        GCAL_VIEW (subscriber),
        gcal_event_widget_peek_uuid (GCAL_EVENT_WIDGET (event)));

  if (widget != NULL)
    {
      gtk_widget_destroy (widget);

      gtk_widget_show (event);
      gtk_container_add (GTK_CONTAINER (subscriber), event);
    }
}

static void
gcal_subscriber_component_removed (ECalDataModelSubscriber *subscriber,
                                   ECalClient              *client,
                                   const gchar             *uid,
                                   const gchar             *rid)
{
  GtkWidget *widget;
  const gchar *sid;
  gchar *uuid;

  sid = e_source_get_uid (e_client_get_source (E_CLIENT (client)));

  if (rid != NULL)
      uuid = g_strdup_printf ("%s:%s:%s", sid, uid, rid);
  else
    uuid = g_strdup_printf ("%s:%s", sid, uid);

  widget = gcal_view_get_by_uuid (GCAL_VIEW (subscriber), uuid);
  if (widget != NULL)
    gtk_widget_destroy (widget);
  else
    g_warning ("%s: Widget with uuid: %s not found",
               G_STRFUNC, uuid);

  g_free (uuid);
}

static void
gcal_subscriber_freeze (ECalDataModelSubscriber *subscriber)
{
}

static void
gcal_subscriber_thaw (ECalDataModelSubscriber *subscriber)
{
}
