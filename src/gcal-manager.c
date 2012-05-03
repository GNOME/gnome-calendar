/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 2 -*- */
/*
 * gcal-manager.c
 * Copyright (C) 2012 Erick Pérez Castellanos <erickpc@gnome.org>
 *
 * gnome-calendar is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * gnome-calendar is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "gcal-manager.h"

#include "gcal-utils.h"

#include <glib/gi18n.h>

#include <libecal/e-cal-client.h>
#include <libecal/e-cal-client-view.h>
#include <libecal/e-cal-time-util.h>
#include <libedataserver/e-data-server-util.h>

enum
{
  COLUMN_UID,
  COLUMN_NAME,
  COLUMN_ACTIVE,
  COLUMN_COLOR,
  N_COLUMNS
};

struct _RetryOpenData
{
  EClient      *client;
  GcalManager    *manager;
  GCancellable *cancellable;
};

typedef struct _RetryOpenData RetryOpenData;

struct _GcalManagerUnit
{
  ESource        *source;
  ECalClient     *client;
  ECalClientView *view;

  gboolean        enabled;
};

typedef struct _GcalManagerUnit GcalManagerUnit;

struct _GcalManagerPrivate
{
  /**
   * The list of clients we are managing.
   * Each element is of type GCalStoreUnit
   */
  GList         *clients;

  /**
   * The list of source groups
   */
  GList         *source_groups;

  /* The store for keeping easily retrieved data about sources */
  GtkListStore  *sources_model;

  /* The range of dates defining the query */
  GDate         *initial_date;
  GDate         *final_date;

  /* The active query */
  gchar         *query;

  /* The events list */
  GList         *events;

  GCancellable  *loading_clients;
};

static void     gcal_manager_constructed                  (GObject         *object);

static void     gcal_manager_finalize                     (GObject         *object);

static void     gcal_manager_on_client_opened             (GObject         *source_object,
                                                           GAsyncResult    *result,
                                                           gpointer         user_data);

static gboolean gcal_manager_retry_open_on_timeout        (gpointer         user_data);

static void     gcal_manager_free_retry_open_data         (gpointer         user_data);

static void     gcal_manager_remove_unit                  (GcalManager     *manager,
                                                           GcalManagerUnit *unit_data);

void            gcal_manager_remove_client                (GcalManager      *manager,
                                                           ECalClient       *client);

static void     gcal_manager_on_object_list_received      (GObject         *source_object,
                                                           GAsyncResult    *result,
                                                           gpointer         user_data);

G_DEFINE_TYPE(GcalManager, gcal_manager, G_TYPE_OBJECT)

static void
gcal_manager_class_init (GcalManagerClass *klass)
{
  G_OBJECT_CLASS (klass)->constructed = gcal_manager_constructed;
  G_OBJECT_CLASS (klass)->finalize = gcal_manager_finalize;

  g_type_class_add_private ((gpointer) klass, sizeof(GcalManagerPrivate));
}

static void
gcal_manager_init (GcalManager *self)
{
  GcalManagerPrivate *priv;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            GCAL_TYPE_MANAGER,
                                            GcalManagerPrivate);
  priv = self->priv;
  priv->sources_model = gtk_list_store_new (N_COLUMNS,
                                            G_TYPE_STRING,
                                            G_TYPE_STRING,
                                            G_TYPE_BOOLEAN,
                                            GDK_TYPE_COLOR);
  g_object_ref_sink (priv->sources_model);
}

static void
gcal_manager_constructed (GObject *object)
{
  GcalManagerPrivate *priv;

  if (G_OBJECT_CLASS (gcal_manager_parent_class)->constructed != NULL)
    G_OBJECT_CLASS (gcal_manager_parent_class)->constructed (object);

  priv = GCAL_MANAGER (object)->priv;
  /* demo data */
  GtkTreeIter iter;
  GdkColor color;

  gdk_color_parse ("red", &color);
  gtk_list_store_append (priv->sources_model, &iter);
  gtk_list_store_set (priv->sources_model, &iter,
                      COLUMN_UID, "local://personal",
                      COLUMN_NAME, "System",
                      COLUMN_ACTIVE, TRUE,
                      COLUMN_COLOR, &color,
                      -1);

  gdk_color_parse ("blue", &color);
  gtk_list_store_append (priv->sources_model, &iter);
  gtk_list_store_set (priv->sources_model, &iter,
                      COLUMN_UID, "webcal:googleplus",
                      COLUMN_NAME, "G+",
                      COLUMN_ACTIVE, TRUE,
                      COLUMN_COLOR, &color,
                      -1);
}

static void
gcal_manager_finalize (GObject *object)
{
  GcalManagerPrivate *priv;

  priv = GCAL_MANAGER (object)->priv;

  g_clear_object (priv->sources_model);
}

static void
gcal_manager_on_client_opened (GObject      *source_object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  ECalClient *client;
  GcalManager *manager;
  GError *error;
  ESource *source;

  client = E_CAL_CLIENT (source_object);
  manager = (GcalManager*) user_data;
  error = NULL;
  if (e_client_open_finish (E_CLIENT (client), result, &error))
    {
      /* get_object_list */
      e_cal_client_get_object_list (client,
                                    manager->priv->query,
                                    manager->priv->loading_clients,
                                    gcal_manager_on_object_list_received,
                                    manager);
    }
  else
    {
      if (error != NULL)
        {
          /* handling error */
          g_warning ("%s\n", error->message);

          if (g_error_matches (error, E_CLIENT_ERROR, E_CLIENT_ERROR_CANCELLED)
              || g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
            {
              g_error_free (error);
              return;
            }

          if (g_error_matches (error, E_CLIENT_ERROR, E_CLIENT_ERROR_BUSY))
            {
              RetryOpenData *rod;

              rod = g_new0 (RetryOpenData, 1);
              rod->client = g_object_ref (client);
              rod->manager = manager;
              rod->cancellable = g_object_ref (manager->priv->loading_clients);

              /* postpone for 1/2 of a second, backend is busy now */
              g_timeout_add_full (G_PRIORITY_DEFAULT,
                                  500,
                                  gcal_manager_retry_open_on_timeout,
                                  rod,
                                  gcal_manager_free_retry_open_data);

              g_error_free (error);

              return;
            }

          /* in any other case, remove it*/
          source = e_client_get_source (E_CLIENT (client));
          gcal_manager_remove_client (manager, client);
          g_warning (
                  "%s: Failed to open '%s': %s",
                  G_STRFUNC,
                  e_source_peek_name (source),
                  error->message);

          g_error_free (error);
          return;
        }
    }

//  /* to have them ready for later use */
//  e_client_retrieve_capabilities (
//          E_CLIENT (client), manager->priv->loading_clients,
//          cal_model_retrieve_capabilies_cb, model);
}

static gboolean
gcal_manager_retry_open_on_timeout (gpointer user_data)
{
  RetryOpenData *rod = user_data;

  g_return_val_if_fail (rod != NULL, FALSE);
  g_return_val_if_fail (rod->client != NULL, FALSE);
  g_return_val_if_fail (rod->manager != NULL, FALSE);

  e_client_open (rod->client,
                 TRUE,
                 rod->cancellable,
                 gcal_manager_on_client_opened,
                 rod->manager);

  return FALSE;
}

static void
gcal_manager_free_retry_open_data (gpointer user_data)
{
  RetryOpenData *rod = user_data;

  if (rod == NULL)
    return;

  g_object_unref (rod->client);
  g_object_unref (rod->cancellable);
  g_free (rod);
}

static void
gcal_manager_remove_unit (GcalManager     *manager,
                          GcalManagerUnit *unit_data)
{
  /* FIXME: whenever we have a default client
   * we must be sure not allow someone remove it */
  /* Remove the client from the list */
  manager->priv->clients = g_list_remove (manager->priv->clients, unit_data);

  /* free all remaining memory */
  g_object_unref (unit_data->client);
  if (unit_data->view)
    g_object_unref (unit_data->view);
  g_free (unit_data);
}

void
gcal_manager_remove_client (GcalManager  *manager,
                            ECalClient *client)
{
  GcalManagerUnit *unit_data;
  GList *l;

  unit_data = NULL;
  g_return_if_fail (GCAL_IS_MANAGER (manager));
  g_return_if_fail (E_IS_CAL_CLIENT (client));

  for (l = manager->priv->clients; l != NULL; l = l->next)
    {
      unit_data = (GcalManagerUnit *) l->data;
      if (unit_data->client == client)
        break;
    }
  if (unit_data)
    gcal_manager_remove_unit (manager, unit_data);
}

static void
gcal_manager_on_object_list_received (GObject      *source_object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
  ECalClient *client;
  GcalManager *manager;

  GSList *event_list;
  GError *error;

  const gchar *uid;
  ECalComponentText summary;
  ECalComponentDateTime start;
  GSList *l;

  client = E_CAL_CLIENT (source_object);
  manager = (GcalManager*) user_data;

  event_list = NULL;
  error = NULL;

  if (e_cal_client_get_object_list_finish (client, result, &event_list, &error))
    {
      g_print ("Obtained list: of length %d\n", event_list == NULL ? 0 : g_slist_length (event_list));

      for (l = event_list; l != NULL; l = l->next)
        {
          icalcomponent *ical = l->data;
          ECalComponent *event = e_cal_component_new ();
          e_cal_component_set_icalcomponent (event, ical);

          e_cal_component_get_uid (event, &uid);
          g_print ("uid: %s\n", uid);

          e_cal_component_get_summary (event,&summary);
          if (summary.value != NULL)
            g_print ("summary: %s\n", summary.value);

          e_cal_component_get_dtstart (event, &start);
          if (start.value != NULL)
            {
              struct tm glib_tm;
              gchar buffer[64];
              glib_tm = icaltimetype_to_tm (start.value);
              e_utf8_strftime (buffer, sizeof (buffer), "%a, %d %b %Y", &glib_tm);
              g_print ("start date: %s\n", buffer);
            }

          manager->priv->events = g_list_append (manager->priv->events, event);
        }
      e_cal_client_free_icalcomp_slist (event_list);
    }
  else
    {
      if (error != NULL)
        {
          g_warning ("%s\n", error->message);

          g_error_free (error);
          return;
        }
    }
}

/**
 * gcal_manager_new:
 *
 * Since: 0.0.1
 * Return value: the new #GcalManager
 * Returns: (transfer full):
 **/
GcalManager*
gcal_manager_new (void)
{
  return GCAL_MANAGER (g_object_new (GCAL_TYPE_MANAGER, NULL));
}

GtkListStore*
gcal_manager_get_sources_model (GcalManager *manager)
{
  g_return_val_if_fail (GCAL_IS_MANAGER (manager), NULL);
  return manager->priv->sources_model;
}

/**
 * gcal_manager_add_source:
 * @manager: a #GcalManager
 * @base_uri: URI defining the ESourceGroup the client will belongs
 * @relative_uri: URI, relative to the base URI
 *
 * Add a new calendar by its URI.
 * The calendar is enabled by default
 *
 * Return value: The UID of calendar's source, NULL in other case
 * Returns: (transfer full):
 *
 * Since: 0.0.1
 */
gchar*
gcal_manager_add_source (GcalManager   *manager,
                         const gchar *name,
                         const gchar *base_uri,
                         const gchar *relative_uri)
{
  GcalManagerPrivate *priv;
  GcalManagerUnit *unit_data;

  ESourceGroup *selected_group;
  GList *l;

  GError *error;

  ESource *source;
  ECalClient *new_client;

  priv = manager->priv;
  selected_group = NULL;

  g_return_val_if_fail (GCAL_IS_MANAGER (manager), FALSE);

  for (l = priv->source_groups; l != NULL; l = l->next)
    {
      ESourceGroup *source_group = (ESourceGroup *) l->data;
      if (g_strcmp0 (base_uri, e_source_group_peek_base_uri (source_group)) == 0)
        {
          selected_group = source_group;

          for (l = priv->clients; l != NULL; l = l->next)
            {
              GcalManagerUnit *unit_data = (GcalManagerUnit*) l->data;
              if (g_strcmp0 (relative_uri, e_source_peek_relative_uri (unit_data->source)) == 0)
                return NULL;
            }

          break;
        }
    }

  error = NULL;
  /* building stuff */
  selected_group = e_source_group_new (gcal_get_group_name (base_uri), base_uri);
  priv->source_groups = g_list_append (priv->source_groups, selected_group);

  source = e_source_new (name, relative_uri);
  e_source_group_add_source (selected_group, source, -1);

  new_client = e_cal_client_new (source, E_CAL_CLIENT_SOURCE_TYPE_EVENTS, &error);

  unit_data = g_new0 (GcalManagerUnit, 1);
  unit_data->client = g_object_ref (new_client);
  unit_data->source = source;

  priv->clients = g_list_append (priv->clients, unit_data);

  e_client_open (E_CLIENT (unit_data->client),
                 TRUE,
                 priv->loading_clients,
                 gcal_manager_on_client_opened,
                 manager);

  return g_strdup (e_source_peek_uid (source));
}

/**
 * gcal_manager_set_new_range:
 * @manager: a #GcalManager
 * @initial_date: starting date of the new range
 * @final_date: final date of the new range
 *
 * Sets a new range of date to look for events.
 * This method compares the dates to find out
 * the larger range, between the new one and the old.
 * It signals once the range it's been set and the
 * events retrieved for it. This methods triggers
 * an update of the internal list of events for every
 * opened calendar; a signal would be emitted
 * when the updated it's done.
 *
 * Return value: void
 *
 * Since: 0.0.1
 */
void
gcal_manager_set_new_range (GcalManager   *manager,
                            const GDate *initial_date,
                            const GDate *final_date)
{
  gboolean refresh_events = TRUE;

//  /* updating query range */
//  if (g_date_compare (manager->priv->initial_date, initial_date) > 0)
//    {
//      /* switch dates */
//      refresh_events = TRUE;
//    }
//  if (g_date_compare (manager->priv->final_date, final_date) < 0)
//    {
//      /* switch dates */
//      refresh_events = TRUE;
//    }

  if (refresh_events)
    {
      /* rebuild query */
      GDateTime * first_datetime = g_date_time_new_local(g_date_get_year (initial_date),
                                                         g_date_get_month (initial_date),
                                                         g_date_get_day (initial_date),
                                                         0,
                                                         0,
                                                         0);
      time_t since = g_date_time_to_unix (first_datetime);
      gchar* since_iso8601 = isodate_from_time_t (since);

      GDateTime * last_datetime = g_date_time_new_local(g_date_get_year (final_date),
                                                        g_date_get_month (final_date),
                                                        g_date_get_day (final_date),
                                                        23,
                                                        59,
                                                        59);
      gchar* until_iso8601 = isodate_from_time_t (
          g_date_time_to_unix (last_datetime));

      manager->priv->query = g_strdup_printf (
          "occur-in-time-range? (make-time \"%s\") "
          "(make-time \"%s\")",
          since_iso8601,
          until_iso8601);

      g_date_time_unref (first_datetime);
      g_date_time_unref (last_datetime);

      g_print ("Loading events since %s until %s\n",
                   since_iso8601,
                   until_iso8601);
    }
}
