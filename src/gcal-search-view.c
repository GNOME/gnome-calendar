/* -*- mode: c; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gcal-search-view.c
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

#include "gcal-search-view.h"

#include "gcal-event-widget.h"
#include "gcal-utils.h"
#include "gcal-view.h"

#include <glib/gi18n.h>

typedef struct
{
  GtkWidget      *listbox;

  /* property */
  icaltimetype   *date;
  GcalManager    *manager; /* weak reference */
} GcalSearchViewPrivate;

enum
{
  PROP_0,
  PROP_DATE,  /* active-date inherited property */
  PROP_MANAGER  /* manager inherited property */
};

static void           gcal_data_model_subscriber_interface_init (ECalDataModelSubscriberInterface *iface);

static void           gcal_view_interface_init                  (GcalViewIface  *iface);

static void           gcal_search_view_constructed              (GObject        *object);

static void           gcal_search_view_set_property             (GObject        *object,
                                                                 guint           property_id,
                                                                 const GValue   *value,
                                                                 GParamSpec     *pspec);

static void           gcal_search_view_get_property             (GObject        *object,
                                                                 guint           property_id,
                                                                 GValue         *value,
                                                                 GParamSpec     *pspec);

static void           gcal_search_view_finalize                 (GObject        *object);

static void           gcal_search_view_component_added          (ECalDataModelSubscriber *subscriber,
                                                                 ECalClient              *client,
                                                                 ECalComponent           *comp);

static void           gcal_search_view_component_modified       (ECalDataModelSubscriber *subscriber,
                                                                 ECalClient              *client,
                                                                 ECalComponent           *comp);

static void           gcal_search_view_component_removed        (ECalDataModelSubscriber *subscriber,
                                                                 ECalClient              *client,
                                                                 const gchar             *uid,
                                                                 const gchar             *rid);

static void           gcal_search_view_freeze                   (ECalDataModelSubscriber *subscriber);

static void           gcal_search_view_thaw                     (ECalDataModelSubscriber *subscriber);

G_DEFINE_TYPE_WITH_CODE (GcalSearchView,
                         gcal_search_view,
                         GTK_TYPE_SCROLLED_WINDOW,
                         G_ADD_PRIVATE (GcalSearchView)
                         G_IMPLEMENT_INTERFACE (E_TYPE_CAL_DATA_MODEL_SUBSCRIBER,
                                                gcal_data_model_subscriber_interface_init)
                         G_IMPLEMENT_INTERFACE (GCAL_TYPE_VIEW, gcal_view_interface_init));

static void
gcal_search_view_class_init (GcalSearchViewClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->constructed = gcal_search_view_constructed;
  object_class->set_property = gcal_search_view_set_property;
  object_class->get_property = gcal_search_view_get_property;
  object_class->finalize = gcal_search_view_finalize;

  g_object_class_install_property (
      object_class,
      PROP_DATE,
      g_param_spec_boxed ("active-date",
                          "The active date",
                          "The active/selected date in the view",
                          ICAL_TIME_TYPE,
                          G_PARAM_READWRITE));

  g_object_class_install_property (
      object_class,
      PROP_MANAGER,
      g_param_spec_pointer ("manager",
                            "The manager object",
                            "A weak reference to the app manager object",
                            G_PARAM_CONSTRUCT_ONLY |
                            G_PARAM_READWRITE));
}

static void
gcal_view_interface_init (GcalViewIface *iface)
{
  iface->get_initial_date = NULL;
  iface->get_final_date = NULL;

  iface->clear_marks = NULL;

  iface->get_left_header = NULL;
  iface->get_right_header = NULL;

  iface->get_by_uuid = NULL;
}

static void
gcal_search_view_init (GcalSearchView *self)
{
}

static void
gcal_data_model_subscriber_interface_init (ECalDataModelSubscriberInterface *iface)
{
  iface->component_added = gcal_search_view_component_added;
  iface->component_modified = gcal_search_view_component_modified;
  iface->component_removed = gcal_search_view_component_removed;
  iface->freeze = gcal_search_view_freeze;
  iface->thaw = gcal_search_view_thaw;
}

static void
gcal_search_view_constructed (GObject *object)
{
  GcalSearchViewPrivate *priv;

  priv =
    gcal_search_view_get_instance_private (GCAL_SEARCH_VIEW (object));

  priv->listbox = gtk_list_box_new ();
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (priv->listbox),
                                   GTK_SELECTION_NONE);
  gtk_widget_show (priv->listbox);

  gtk_container_add (GTK_CONTAINER (object), priv->listbox);


  gcal_manager_set_search_subscriber (
      priv->manager,
      E_CAL_DATA_MODEL_SUBSCRIBER (object),
      0, 0);
}

static void
gcal_search_view_set_property (GObject       *object,
                               guint          property_id,
                               const GValue  *value,
                               GParamSpec    *pspec)
{
  GcalSearchViewPrivate *priv;

  priv =
    gcal_search_view_get_instance_private (GCAL_SEARCH_VIEW (object));

  switch (property_id)
    {
    case PROP_DATE:
      {
        if (priv->date != NULL)
          g_free (priv->date);

        priv->date = g_value_dup_boxed (value);
        break;
      }
    case PROP_MANAGER:
      {
        priv->manager = g_value_get_pointer (value);
        break;
      }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gcal_search_view_get_property (GObject       *object,
                               guint          property_id,
                               GValue        *value,
                               GParamSpec    *pspec)
{
  GcalSearchViewPrivate *priv;

  priv = gcal_search_view_get_instance_private (GCAL_SEARCH_VIEW (object));

  switch (property_id)
    {
    case PROP_DATE:
      g_value_set_boxed (value, priv->date);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gcal_search_view_finalize (GObject       *object)
{
  GcalSearchViewPrivate *priv;
  priv = gcal_search_view_get_instance_private (GCAL_SEARCH_VIEW (object));

  if (priv->date != NULL)
    g_free (priv->date);

  /* Chain up to parent's finalize() method. */
  G_OBJECT_CLASS (gcal_search_view_parent_class)->finalize (object);
}

/* ECalDataModelSubscriber interface API */
static void
gcal_search_view_component_added (ECalDataModelSubscriber *subscriber,
                                  ECalClient              *client,
                                  ECalComponent           *comp)
{
  GcalSearchViewPrivate *priv;

  GtkWidget *event;
  GcalEventData *data;

  priv =
    gcal_search_view_get_instance_private (GCAL_SEARCH_VIEW (subscriber));

  data = g_new0 (GcalEventData, 1);
  data->source = e_client_get_source (E_CLIENT (client));
  data->event_component = e_cal_component_clone (comp);

  event = gcal_event_widget_new_from_data (data);
  gcal_event_widget_set_read_only (GCAL_EVENT_WIDGET (event), e_client_is_readonly (E_CLIENT (client)));
  g_free (data);

  gtk_widget_set_margin_start (event, 96);
  gtk_widget_set_margin_end (event, 96);
  gtk_widget_show (event);
  gtk_container_add (GTK_CONTAINER (priv->listbox), event);
}

static void
gcal_search_view_component_modified (ECalDataModelSubscriber *subscriber,
                                     ECalClient              *client,
                                     ECalComponent           *comp)
{
  ;
}

static void
gcal_search_view_component_removed (ECalDataModelSubscriber *subscriber,
                                    ECalClient              *client,
                                    const gchar             *uid,
                                    const gchar             *rid)
{
  GcalSearchViewPrivate *priv;

  priv =
    gcal_search_view_get_instance_private (GCAL_SEARCH_VIEW (subscriber));

  gtk_container_foreach (GTK_CONTAINER (priv->listbox),
                         (GtkCallback) gtk_widget_destroy, NULL);
}

static void
gcal_search_view_freeze (ECalDataModelSubscriber *subscriber)
{
  ;
}

static void
gcal_search_view_thaw (ECalDataModelSubscriber *subscriber)
{
}

/* Public API */
/**
 * gcal_search_view_new:
 * @manager: App singleton #GcalManager instance
 *
 * Since: 0.1
 * Create a new month view widget
 *
 * Returns: (transfer full):
 **/
GtkWidget*
gcal_search_view_new (GcalManager *manager)
{
  return g_object_new (GCAL_TYPE_SEARCH_VIEW, "manager", manager, NULL);
}
