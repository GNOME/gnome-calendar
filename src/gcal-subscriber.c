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
#include "gcal-subscriber-private.h"

#include "gcal-view.h"
#include "gcal-event-widget.h"

static void           gcal_data_model_subscriber_interface_init (ECalDataModelSubscriberInterface *iface);

static void           gcal_subscriber_finalize                  (GObject                          *object);

static void           gcal_subscriber_add                       (GtkContainer                     *container,
                                                                 GtkWidget                        *widget);

static void           gcal_subscriber_remove                    (GtkContainer                     *container,
                                                                 GtkWidget                        *widget);

static void           gcal_subscriber_forall                    (GtkContainer                     *container,
                                                                 gboolean                          include_internals,
                                                                 GtkCallback                       callback,
                                                                 gpointer                          callback_data);

static gboolean       gcal_subscriber_is_child_multicell        (GcalSubscriber                   *subscriber,
                                                                 GcalEventWidget                  *child);

static guint          gcal_subscriber_get_child_cell            (GcalSubscriber                   *subscriber,
                                                                 GcalEventWidget                  *child);

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

G_DEFINE_TYPE_WITH_CODE (GcalSubscriber, gcal_subscriber, GTK_TYPE_CONTAINER,
                         G_ADD_PRIVATE (GcalSubscriber)
                         G_IMPLEMENT_INTERFACE (E_TYPE_CAL_DATA_MODEL_SUBSCRIBER,
                                                gcal_data_model_subscriber_interface_init));


static void
gcal_subscriber_class_init (GcalSubscriberClass *klass)
{
  GtkContainerClass *container_class;

  klass->is_child_multicell = gcal_subscriber_is_child_multicell;
  klass->get_child_cell = gcal_subscriber_get_child_cell;

  G_OBJECT_CLASS (klass)->finalize = gcal_subscriber_finalize;

  container_class = GTK_CONTAINER_CLASS (klass);
  container_class->add = gcal_subscriber_add;
  container_class->remove = gcal_subscriber_remove;
  container_class->forall = gcal_subscriber_forall;
}

static void
gcal_subscriber_init (GcalSubscriber *self)
{
  GcalSubscriberPrivate *priv;

  priv = gcal_subscriber_get_instance_private (self);
  self->priv = priv;

  priv->children = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_list_free);
  priv->single_cell_children = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, (GDestroyNotify) g_list_free);
  priv->overflow_cells = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, (GDestroyNotify) g_list_free);
  priv->hidden_as_overflow = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
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

static void
gcal_subscriber_finalize (GObject *object)
{
  GcalSubscriberPrivate *priv;

  priv = gcal_subscriber_get_instance_private (GCAL_SUBSCRIBER (object));

  g_hash_table_destroy (priv->children);
  g_hash_table_destroy (priv->single_cell_children);
  g_hash_table_destroy (priv->overflow_cells);
  g_hash_table_destroy (priv->hidden_as_overflow);

  if (priv->multi_cell_children != NULL)
    g_list_free (priv->multi_cell_children);

  /* Chain up to parent's finalize() method. */
  G_OBJECT_CLASS (gcal_subscriber_parent_class)->finalize (object);
}

static void
gcal_subscriber_add (GtkContainer *container,
                     GtkWidget    *widget)
{
  GcalSubscriberPrivate *priv;
  const gchar *uuid;
  GList *l = NULL;

  g_return_if_fail (GCAL_IS_EVENT_WIDGET (widget));
  g_return_if_fail (gtk_widget_get_parent (widget) == NULL);
  priv = gcal_subscriber_get_instance_private (GCAL_SUBSCRIBER (container));
  uuid = gcal_event_widget_peek_uuid (GCAL_EVENT_WIDGET (widget));

  /* inserting in all children hash */
  if (g_hash_table_lookup (priv->children, uuid) != NULL)
    {
      g_warning ("Event with uuid: %s already added", uuid);
      gtk_widget_destroy (widget);
      return;
    }
  l = g_list_append (l, widget);
  g_hash_table_insert (priv->children, g_strdup (uuid), l);

  if (gcal_subscriber_is_child_multicell (GCAL_SUBSCRIBER (container), GCAL_EVENT_WIDGET (widget)))
    {
      priv->multi_cell_children = g_list_insert_sorted (priv->multi_cell_children, widget,
                                                       (GCompareFunc) gcal_event_widget_compare_by_length);
    }
  else
    {
      guint cell_idx = gcal_subscriber_get_child_cell (GCAL_SUBSCRIBER (container), GCAL_EVENT_WIDGET (widget));
      l = g_hash_table_lookup (priv->single_cell_children, GINT_TO_POINTER (cell_idx));
      l = g_list_insert_sorted (l, widget, (GCompareFunc) gcal_event_widget_compare_by_start_date);

      if (g_list_length (l) != 1)
        {
          g_hash_table_steal (priv->single_cell_children, GINT_TO_POINTER (cell_idx));
        }
      g_hash_table_insert (priv->single_cell_children, GINT_TO_POINTER (cell_idx), l);
    }

  /* setup child */
  gtk_widget_set_parent (widget, GTK_WIDGET (container));
  //g_signal_connect (widget, "activate", G_CALLBACK (event_opened), container);
}

static void
gcal_subscriber_remove (GtkContainer *container,
                        GtkWidget    *widget)
{
  GcalSubscriberPrivate *priv;
  const gchar *uuid;

  GList *l, *aux;
  gboolean was_visible = FALSE;
  GtkWidget *master_widget;

  g_return_if_fail (gtk_widget_get_parent (widget) == GTK_WIDGET (container));
  priv = gcal_subscriber_get_instance_private (GCAL_SUBSCRIBER (container));

  uuid = gcal_event_widget_peek_uuid (GCAL_EVENT_WIDGET (widget));

  l = g_hash_table_lookup (priv->children, uuid);
  if (l != NULL)
    {
      master_widget = (GtkWidget*) l->data;

      was_visible = gtk_widget_get_visible (widget);
      gtk_widget_unparent (widget);

      if (widget == master_widget)
        {
          if (gcal_subscriber_is_child_multicell (GCAL_SUBSCRIBER (container), GCAL_EVENT_WIDGET (widget)))
            {
              priv->multi_cell_children = g_list_remove (priv->multi_cell_children, widget);

              aux = g_list_next (l);
              if (aux != NULL)
                {
                  l->next = NULL;
                  aux->prev = NULL;
                  g_list_foreach (aux, (GFunc) gtk_widget_unparent, NULL);
                  g_list_free (aux);
                }
            }
          else
            {
              guint cell_idx = gcal_subscriber_get_child_cell (GCAL_SUBSCRIBER (container), GCAL_EVENT_WIDGET (widget));

              aux = g_hash_table_lookup (priv->single_cell_children, GINT_TO_POINTER (cell_idx));
              aux = g_list_remove (g_list_copy (aux), widget);
              if (aux == NULL)
                g_hash_table_remove (priv->single_cell_children, GINT_TO_POINTER (cell_idx));
              else
                g_hash_table_replace (priv->single_cell_children, GINT_TO_POINTER (cell_idx), aux);
            }
        }

      l = g_list_remove (g_list_copy (l), widget);
      if (l == NULL)
        g_hash_table_remove (priv->children, uuid);
      else
        g_hash_table_replace (priv->children, g_strdup (uuid), l);

      g_hash_table_remove (priv->hidden_as_overflow, uuid);

      if (was_visible)
        gtk_widget_queue_resize (GTK_WIDGET (container));
    }
}

static void
gcal_subscriber_forall (GtkContainer *container,
                        gboolean      include_internals,
                        GtkCallback   callback,
                        gpointer      callback_data)
{
  GcalSubscriberPrivate *priv;
  GList *l, *l2, *aux = NULL;

  priv = gcal_subscriber_get_instance_private (GCAL_SUBSCRIBER (container));

  l2 = g_hash_table_get_values (priv->children);
  for (l = l2; l != NULL; l = g_list_next (l))
    aux = g_list_concat (aux, g_list_reverse (g_list_copy (l->data)));
  g_list_free (l2);

  l = aux;
  while (aux != NULL)
    {
      GtkWidget *widget = (GtkWidget*) aux->data;
      aux = aux->next;

      (*callback) (widget, callback_data);
    }
  g_list_free (l);
}

static gboolean
gcal_subscriber_is_child_multicell (GcalSubscriber  *subscriber,
                                    GcalEventWidget *child)
{
  GcalSubscriberClass *klass;

  g_return_val_if_fail (GCAL_IS_SUBSCRIBER (subscriber), FALSE);
  g_return_val_if_fail (GCAL_IS_EVENT_WIDGET (child), FALSE);

  klass = GCAL_SUBSCRIBER_GET_CLASS (subscriber);

  if (klass->is_child_multicell)
    return klass->is_child_multicell (subscriber, child);

  return FALSE;
}

static guint
gcal_subscriber_get_child_cell (GcalSubscriber  *subscriber,
                                GcalEventWidget *child)
{
  GcalSubscriberClass *klass;

  g_return_val_if_fail (GCAL_IS_SUBSCRIBER (subscriber), 0);
  g_return_val_if_fail (GCAL_IS_EVENT_WIDGET (child), 0);

  klass = GCAL_SUBSCRIBER_GET_CLASS (subscriber);

  if (klass->get_child_cell)
    return klass->get_child_cell (subscriber, child);

  return 0;
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
