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
  COLUMN_UID,         //corresponding source uid
  COLUMN_NAME,
  COLUMN_ACTIVE,
  COLUMN_COLOR,
  N_COLUMNS
};

struct _RetryOpenData
{
  EClient      *client;
  GcalManager  *manager;
  GCancellable *cancellable;
};

typedef struct _RetryOpenData RetryOpenData;

struct _GcalManagerUnit
{
  ESource        *source;
  ECalClient     *client;
  ECalClientView *view;

  /**
   * The hash containing events retrieved.
   * Keys are the uid of the component
   * Values are the component itself
   */
  GHashTable     *events;

  gboolean        enabled;
};

typedef struct _GcalManagerUnit GcalManagerUnit;

struct _GcalManagerPrivate
{
  /**
   * The list of clients we are managing.
   * Each value is of type GCalStoreUnit
   * And is key is uid of the source
   */
  GHashTable    *clients;

  /**
   * The list of source groups
   */
  GList         *source_groups;

  /* The store for keeping easily retrieved data about sources */
  GtkListStore  *sources_model;

  /* The range of dates defining the query */
  icaltimetype  *initial_date;
  icaltimetype  *final_date;

  /* The active query */
  gchar         *query;

  GCancellable  *loading_clients;
};
/* Signal IDs */
enum
{
  EVENTS_ADDED,
  EVENTS_MODIFIED,
  EVENTS_REMOVED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void     gcal_manager_constructed                  (GObject         *object);

static void     gcal_manager_finalize                     (GObject         *object);

void            _gcal_manager_free_unit_data              (gpointer         data);

static void     _gcal_manager_on_client_opened            (GObject         *source_object,
                                                           GAsyncResult    *result,
                                                           gpointer         user_data);

static gboolean _gcal_manager_retry_open_on_timeout       (gpointer         user_data);

static void     _gcal_manager_free_retry_open_data        (gpointer         user_data);

void            _gcal_manager_remove_client               (GcalManager      *manager,
                                                           ECalClient       *client);

static void     _gcal_manager_on_object_list_received     (GObject         *source_object,
                                                           GAsyncResult    *result,
                                                           gpointer         user_data);

static void     _gcal_manager_reload_events               (GcalManager     *manager);

G_DEFINE_TYPE(GcalManager, gcal_manager, G_TYPE_OBJECT)

static void
gcal_manager_class_init (GcalManagerClass *klass)
{
  G_OBJECT_CLASS (klass)->constructed = gcal_manager_constructed;
  G_OBJECT_CLASS (klass)->finalize = gcal_manager_finalize;

  signals[EVENTS_ADDED] = g_signal_new ("events-added",
                                         GCAL_TYPE_MANAGER,
                                         G_SIGNAL_RUN_FIRST,
                                         G_STRUCT_OFFSET (GcalManagerClass,
                                                          events_added),
                                         NULL, NULL,
                                         g_cclosure_marshal_VOID__POINTER,
                                         G_TYPE_NONE,
                                         1,
                                         G_TYPE_POINTER);

  signals[EVENTS_MODIFIED] = g_signal_new ("events-modified",
                                           GCAL_TYPE_MANAGER,
                                           G_SIGNAL_RUN_FIRST,
                                           G_STRUCT_OFFSET (GcalManagerClass,
                                                            events_modified),
                                           NULL, NULL,
                                           g_cclosure_marshal_VOID__POINTER,
                                           G_TYPE_NONE,
                                           1,
                                           G_TYPE_POINTER);
  signals[EVENTS_REMOVED] = g_signal_new ("evens-removed",
                                          GCAL_TYPE_MANAGER,
                                          G_SIGNAL_RUN_FIRST,
                                          G_STRUCT_OFFSET (GcalManagerClass,
                                                           events_removed),
                                          NULL, NULL,
                                          g_cclosure_marshal_VOID__POINTER,
                                          G_TYPE_NONE,
                                          1,
                                          G_TYPE_POINTER);

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

  priv->initial_date = g_new(icaltimetype, 1);
  *(priv->initial_date) = icaltime_from_timet (time (NULL), 0);

  priv->final_date = g_new(icaltimetype, 1);
  *(priv->final_date) = icaltime_from_timet (time (NULL), 0);

  priv->clients = g_hash_table_new_full (g_str_hash,
                                         g_str_equal,
                                         g_free,
                                         _gcal_manager_free_unit_data);
}

static void
gcal_manager_constructed (GObject *object)
{
  GcalManagerPrivate *priv;

  if (G_OBJECT_CLASS (gcal_manager_parent_class)->constructed != NULL)
    G_OBJECT_CLASS (gcal_manager_parent_class)->constructed (object);

  priv = GCAL_MANAGER (object)->priv;
  priv->query = NULL;
}

static void
gcal_manager_finalize (GObject *object)
{
  GcalManagerPrivate *priv;

  priv = GCAL_MANAGER (object)->priv;

  g_hash_table_destroy (priv->clients);
  g_clear_object (priv->sources_model);
}

void
_gcal_manager_free_unit_data (gpointer data)
{
  GcalManagerUnit *unit;

  unit = (GcalManagerUnit*) data;
  g_clear_object (unit->source);
  g_clear_object (unit->client);
  g_clear_object (unit->view);
  g_hash_table_destroy (unit->events);

  g_free (unit);
}

static void
_gcal_manager_on_client_opened (GObject      *source_object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  ECalClient *client;
  GcalManagerPrivate *priv;
  GError *error;
  ESource *source;

  client = E_CAL_CLIENT (source_object);
  priv = ((GcalManager*) user_data)->priv;
  error = NULL;
  if (e_client_open_finish (E_CLIENT (client), result, &error))
    {
      /* get_object_list */
      if (priv->query != NULL)
        {
          e_cal_client_get_object_list (client,
                                        priv->query,
                                        priv->loading_clients,
                                        _gcal_manager_on_object_list_received,
                                        user_data);
        }
    }
  else
    {
      if (error != NULL)
        {
          /* handling error */
          g_warning ("%s", error->message);

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
              rod->manager = user_data;
              rod->cancellable = g_object_ref (priv->loading_clients);

              /* postpone for 1/2 of a second, backend is busy now */
              g_timeout_add_full (G_PRIORITY_DEFAULT,
                                  500,
                                  _gcal_manager_retry_open_on_timeout,
                                  rod,
                                  _gcal_manager_free_retry_open_data);

              g_error_free (error);

              return;
            }

          /* in any other case, remove it*/
          source = e_client_get_source (E_CLIENT (client));
          _gcal_manager_remove_client (GCAL_MANAGER (user_data), client);
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
_gcal_manager_retry_open_on_timeout (gpointer user_data)
{
  RetryOpenData *rod = user_data;

  g_return_val_if_fail (rod != NULL, FALSE);
  g_return_val_if_fail (rod->client != NULL, FALSE);
  g_return_val_if_fail (rod->manager != NULL, FALSE);

  e_client_open (rod->client,
                 TRUE,
                 rod->cancellable,
                 _gcal_manager_on_client_opened,
                 rod->manager);

  return FALSE;
}

static void
_gcal_manager_free_retry_open_data (gpointer user_data)
{
  RetryOpenData *rod = user_data;

  if (rod == NULL)
    return;

  g_object_unref (rod->client);
  g_object_unref (rod->cancellable);
  g_free (rod);
}

void
_gcal_manager_remove_client (GcalManager  *manager,
                             ECalClient *client)
{
  GcalManagerPrivate *priv;

  g_return_if_fail (GCAL_IS_MANAGER (manager));
  g_return_if_fail (E_IS_CAL_CLIENT (client));

  priv = manager->priv;
  g_hash_table_remove (
      priv->clients,
      e_source_peek_uid (e_client_get_source (E_CLIENT (client))));
}

static void
_gcal_manager_on_object_list_received (GObject      *source_object,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  ECalClient *client;
  GcalManagerPrivate *priv;

  GSList *event_list;
  GError *error;

  ECalComponentText summary;
  ECalComponentDateTime start;
  GSList *l;

  const gchar* uid;
  GcalManagerUnit *unit;
  const gchar* event_uid;

  client = E_CAL_CLIENT (source_object);
  priv = ((GcalManager*) user_data)->priv;

  event_list = NULL;
  error = NULL;

  if (e_cal_client_get_object_list_finish (client,
                                           result,
                                           &event_list,
                                           &error))
    {
      uid = e_source_peek_uid (e_client_get_source (E_CLIENT (client)));

      g_debug ("Obtained list: of length %d of events from source %s",
               event_list == NULL ? 0 : g_slist_length (event_list),
               uid);

      unit = (GcalManagerUnit*) g_hash_table_lookup (priv->clients, uid);

      for (l = event_list; l != NULL; l = l->next)
        {
          icalcomponent *ical = l->data;
          ECalComponent *event = e_cal_component_new ();
          e_cal_component_set_icalcomponent (event, ical);

          e_cal_component_get_uid (event, &event_uid);
          g_debug ("event_uid: %s", event_uid);

          g_hash_table_insert (unit->events,
                               g_strdup (event_uid),
                               event);

          e_cal_component_get_summary (event, &summary);
          if (summary.value != NULL)
            g_debug ("summary: %s", summary.value);

          e_cal_component_get_dtstart (event, &start);
          if (start.value != NULL)
            {
              struct tm glib_tm;
              gchar buffer[64];
              glib_tm = icaltimetype_to_tm (start.value);
              e_utf8_strftime (buffer, sizeof (buffer), "%a, %d %b %Y", &glib_tm);
              g_debug ("start date: %s", buffer);
            }
        }
      e_cal_client_free_icalcomp_slist (event_list);
    }
  else
    {
      if (error != NULL)
        {
          g_warning ("%s", error->message);

          g_error_free (error);
          return;
        }
    }
}

static void
_gcal_manager_reload_events (GcalManager *manager)
{
  GcalManagerPrivate *priv;
  GcalManagerUnit *unit_data;
  GHashTableIter iter;
  gpointer key;
  gpointer value;

  priv = manager->priv;
  g_hash_table_iter_init (&iter, priv->clients);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      unit_data = (GcalManagerUnit*) value;
      e_cal_client_get_object_list (unit_data->client,
                                    priv->query,
                                    priv->loading_clients,
                                    _gcal_manager_on_object_list_received,
                                    manager);
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
 * @color: a string representing a color parseable for gdk_color_parse
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
gcal_manager_add_source (GcalManager *manager,
                         const gchar *name,
                         const gchar *base_uri,
                         const gchar *relative_uri,
                         const gchar *color)
{
  GcalManagerPrivate *priv;
  GcalManagerUnit *unit_data;

  ESourceGroup *selected_group;
  GList *l;
  GHashTableIter hash_iter;
  gpointer key;
  gpointer value;

  GError *error;

  ESource *source;
  ECalClient *new_client;

  GtkTreeIter iter;
  GdkColor gdk_color;

  priv = manager->priv;
  selected_group = NULL;

  g_return_val_if_fail (GCAL_IS_MANAGER (manager), FALSE);

  for (l = priv->source_groups; l != NULL; l = l->next)
    {
      ESourceGroup *source_group = (ESourceGroup *) l->data;
      if (g_strcmp0 (base_uri, e_source_group_peek_base_uri (source_group)) == 0)
        {
          selected_group = source_group;

          g_hash_table_iter_init (&hash_iter, priv->clients);
          while (g_hash_table_iter_next (&hash_iter, &key, &value))
            {
              GcalManagerUnit *unit_data = (GcalManagerUnit*) value;
              if (g_strcmp0 (e_source_peek_relative_uri (unit_data->source),
                    relative_uri) == 0)
                {
                  return NULL;
                }
            }
          break;
        }
    }

  error = NULL;
  /* building stuff */
  if (selected_group == NULL)
    {
      selected_group = e_source_group_new (gcal_get_group_name (base_uri),
                                           base_uri);
      priv->source_groups = g_list_append (priv->source_groups, selected_group);
    }

  source = e_source_new (name, relative_uri);
  e_source_group_add_source (selected_group, source, -1);

  new_client = e_cal_client_new (source, E_CAL_CLIENT_SOURCE_TYPE_EVENTS, &error);

  unit_data = g_new0 (GcalManagerUnit, 1);
  unit_data->client = g_object_ref (new_client);
  unit_data->source = source;
  unit_data->events = g_hash_table_new_full (g_str_hash,
                                             g_str_equal,
                                             g_free,
                                             g_object_unref);

  g_hash_table_insert (priv->clients,
                       g_strdup (e_source_peek_uid (source)),
                       unit_data);

  /* filling store */
  gdk_color_parse (color, &gdk_color);
  gtk_list_store_append (priv->sources_model, &iter);
  gtk_list_store_set (priv->sources_model, &iter,
                      COLUMN_UID, e_source_peek_uid (source),
                      COLUMN_NAME, name,
                      COLUMN_ACTIVE, TRUE,
                      COLUMN_COLOR, &gdk_color,
                      -1);

  e_client_open (E_CLIENT (unit_data->client),
                 TRUE,
                 priv->loading_clients,
                 _gcal_manager_on_client_opened,
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
gcal_manager_set_new_range (GcalManager        *manager,
                            const icaltimetype *initial_date,
                            const icaltimetype *final_date)
{
  GcalManagerPrivate *priv;
  gboolean refresh_events;

  g_return_if_fail (manager != NULL);
  g_return_if_fail (initial_date != NULL);
  g_return_if_fail (final_date != NULL);

  priv = manager->priv;
  refresh_events = FALSE;

  /* updating query range */
  if ((icaltime_compare (*(priv->initial_date), *initial_date)) == 1)
    {
      /* switch dates */
      *(priv->initial_date) = *initial_date;
      refresh_events = TRUE;
    }
  if (icaltime_compare (*(priv->final_date), *final_date) == -1)
    {
      /* switch dates */
      *(priv->final_date) = *final_date;
      refresh_events = TRUE;
    }

  if (refresh_events)
    {
      /* rebuild query */
      gchar* since_iso8601 =
        isodate_from_time_t (icaltime_as_timet (*(priv->initial_date)));

      gchar* until_iso8601 =
        isodate_from_time_t (icaltime_as_timet (*(priv->final_date)));

      if (priv->query != NULL)
        g_free (priv->query);
      priv->query = g_strdup_printf (
          "occur-in-time-range? (make-time \"%s\") "
          "(make-time \"%s\")",
          since_iso8601,
          until_iso8601);

      g_debug ("Query %s", priv->query);

      /* redoing query */
      _gcal_manager_reload_events (manager);
    }
}
