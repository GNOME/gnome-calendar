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
  COLUMN_UID = 0,         //corresponding source uid
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
   * And each key is the source uid
   */
  GHashTable    *clients;

  /**
   * System sources read from gconf
   */
  ESourceList   *system_sources;

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

void            gcal_manager_free_unit_data               (gpointer         data);

static void     gcal_manager_on_client_opened             (GObject         *source_object,
                                                           GAsyncResult    *result,
                                                           gpointer         user_data);

static gboolean gcal_manager_retry_open_on_timeout        (gpointer         user_data);

static void     gcal_manager_free_retry_open_data         (gpointer         user_data);

void            gcal_manager_remove_client                (GcalManager     *manager,
                                                           ECalClient      *client);

static void     gcal_manager_load_source                  (GcalManager     *manager,
                                                           ESource         *source);

static void     gcal_manager_reload_events                (GcalManager     *manager);

static void     gcal_manager_reload_view                  (GcalManager     *manager,
                                                           GcalManagerUnit *unit);

static void     gcal_manager_on_view_objects_added        (ECalClientView  *view,
                                                           gpointer         objects,
                                                           gpointer         user_data);

static void     gcal_manager_on_view_objects_removed      (ECalClientView  *view,
                                                           gpointer         objects,
                                                           gpointer         user_data);

static void     gcal_manager_on_view_objects_modified     (ECalClientView  *view,
                                                           gpointer         objects,
                                                           gpointer         user_data);

static void     gcal_manager_on_sources_row_changed       (GtkTreeModel    *store,
                                                           GtkTreePath     *path,
                                                           GtkTreeIter     *iter,
                                                           gpointer         user_data);

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

  g_signal_connect (priv->sources_model,
                    "row-changed",
                    G_CALLBACK (gcal_manager_on_sources_row_changed),
                    self);

  priv->initial_date = g_new(icaltimetype, 1);
  *(priv->initial_date) = icaltime_from_timet (time (NULL), 0);

  priv->final_date = g_new(icaltimetype, 1);
  *(priv->final_date) = icaltime_from_timet (time (NULL), 0);

  priv->clients = g_hash_table_new_full (g_str_hash,
                                         g_str_equal,
                                         g_free,
                                         gcal_manager_free_unit_data);
}

static void
gcal_manager_constructed (GObject *object)
{
  GcalManagerPrivate *priv;

  GError *error;
  GSList *l_groups;
  GSList *l;

  if (G_OBJECT_CLASS (gcal_manager_parent_class)->constructed != NULL)
    G_OBJECT_CLASS (gcal_manager_parent_class)->constructed (object);

  priv = GCAL_MANAGER (object)->priv;

  if (!e_cal_client_get_sources (&(priv->system_sources),
                                 E_CAL_CLIENT_SOURCE_TYPE_EVENTS,
                                 &error))
    {
     //FIXME maybe give more information of what happened
      g_warning ("%s", error->message);
    }

  for (l_groups = e_source_list_peek_groups (priv->system_sources);
       l_groups != NULL;
       l_groups = l_groups->next)
    {
      for (l = e_source_group_peek_sources (E_SOURCE_GROUP (l_groups->data));
           l != NULL;
           l = l->next)
        {
          gcal_manager_load_source (GCAL_MANAGER (object), l->data);
        }
    }
}

static void
gcal_manager_finalize (GObject *object)
{
  GcalManagerPrivate *priv;

  priv = GCAL_MANAGER (object)->priv;

  g_hash_table_destroy (priv->clients);
  g_clear_object (&(priv->sources_model));
}

void
gcal_manager_free_unit_data (gpointer data)
{
  GcalManagerUnit *unit;

  unit = (GcalManagerUnit*) data;
  g_clear_object (&(unit->source));
  g_clear_object (&(unit->client));
  g_clear_object (&(unit->view));
  g_hash_table_destroy (unit->events);

  g_free (unit);
}

static void
gcal_manager_on_client_opened (GObject      *source_object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  ECalClient *client;
  GcalManagerPrivate *priv;
  GError *error;
  ESource *source;

  const gchar *uid;
  GcalManagerUnit *unit;

  client = E_CAL_CLIENT (source_object);
  priv = ((GcalManager*) user_data)->priv;
  error = NULL;
  if (e_client_open_finish (E_CLIENT (client), result, &error))
    {
      /* get_object_list */
      if (priv->query != NULL)
        {
          uid = e_source_peek_uid (e_client_get_source (E_CLIENT (client)));
          unit = (GcalManagerUnit*) g_hash_table_lookup (priv->clients, uid);

          /* setting view */
          gcal_manager_reload_view (GCAL_MANAGER (user_data), unit);
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
                                  gcal_manager_retry_open_on_timeout,
                                  rod,
                                  gcal_manager_free_retry_open_data);

              g_error_free (error);

              return;
            }

          /* in any other case, remove it*/
          source = e_client_get_source (E_CLIENT (client));
          gcal_manager_remove_client (GCAL_MANAGER (user_data), client);
          g_warning ("%s: Failed to open '%s': %s",
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

void
gcal_manager_remove_client (GcalManager  *manager,
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
gcal_manager_load_source (GcalManager *manager,
                          ESource     *source)
{
  GcalManagerPrivate *priv;
  ECalClient *new_client;
  GcalManagerUnit *unit;
  GError *error;

  GtkTreeIter iter;
  GdkColor gdk_color;

  priv = manager->priv;
  error = NULL;
  new_client = e_cal_client_new (source, E_CAL_CLIENT_SOURCE_TYPE_EVENTS, &error);
  if (error != NULL)
    {
      g_warning ("Couldn't create ECalClient. Error: %s", error->message);
      g_error_free (error);
      return;
    }

  unit = g_new0 (GcalManagerUnit, 1);
  unit->client = g_object_ref (new_client);
  unit->source = source;
  unit->events = g_hash_table_new_full (g_str_hash,
                                        g_str_equal,
                                        g_free,
                                        g_object_unref);

  g_hash_table_insert (priv->clients,
                       g_strdup (e_source_peek_uid (source)),
                       unit);

  /* filling store */
  gdk_color_parse (e_source_peek_color_spec (source), &gdk_color);
  gtk_list_store_append (priv->sources_model, &iter);
  gtk_list_store_set (priv->sources_model, &iter,
                      COLUMN_UID, e_source_peek_uid (source),
                      COLUMN_NAME, e_source_peek_name (source),
                      COLUMN_ACTIVE, TRUE,
                      COLUMN_COLOR, &gdk_color,
                      -1);

  e_client_open (E_CLIENT (unit->client),
                 TRUE,
                 priv->loading_clients,
                 gcal_manager_on_client_opened,
                 manager);
}

/**
 * gcal_manager_reload_events:
 *
 * @manager: Self
 *
 * This executes every time a new query has been set.
 * So, there are a bunch of stuff to be done here:
 * <itemizedlist>
 * <listitem><para>
 *   Retrieving Object list with the new query.
 * </para></listitem>
 * <listitem><para>
 *   Releasing the old view, desconnecting the callbacks
 * </para></listitem>
 * <listitem><para>
 *   Creating a new view, connecting callbacks
 * </para></listitem>
 * </itemizedlist>
 */
static void
gcal_manager_reload_events (GcalManager *manager)
{
  GcalManagerPrivate *priv;
  GHashTableIter iter;
  gpointer key;
  gpointer value;

  priv = manager->priv;
  g_hash_table_iter_init (&iter, priv->clients);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      gcal_manager_reload_view (manager, (GcalManagerUnit*) value);
    }
}

static void
gcal_manager_reload_view (GcalManager     *manager,
                          GcalManagerUnit *unit)
{
  GcalManagerPrivate *priv;
  GError *error;

  priv = manager->priv;
  g_return_if_fail (priv->query != NULL);

  /* stopping */
  if (unit->view != NULL)
    g_clear_object (&(unit->view));

  error = NULL;
  if (e_cal_client_get_view_sync (unit->client,
                                  priv->query,
                                  &(unit->view),
                                  priv->loading_clients,
                                  &error))
    {
      /*hooking signals */
      g_signal_connect (unit->view,
                        "objects-added",
                        G_CALLBACK (gcal_manager_on_view_objects_added),
                        manager);

      g_signal_connect (unit->view,
                        "objects-removed",
                        G_CALLBACK (gcal_manager_on_view_objects_removed),
                        manager);

      g_signal_connect (unit->view,
                        "objects-modified",
                        G_CALLBACK (gcal_manager_on_view_objects_modified),
                        manager);

      error = NULL;
      e_cal_client_view_set_fields_of_interest (unit->view, NULL, &error);

      error = NULL;
      e_cal_client_view_start (unit->view, &error);

      if (error != NULL)
        {
          g_clear_object (&(unit->view));
          g_warning ("%s", error->message);
        }
    }
  else
    {
      g_warning ("%s", error->message);
    }
}

/**
 * gcal_manager_on_view_objects_added
 * @view: the view emitting the signal
 * @objects: a GSList of icalcomponent*
 * @user_data: The data passed when connecting the signal, here GcalManager
 */
static void
gcal_manager_on_view_objects_added (ECalClientView *view,
                                    gpointer        objects,
                                    gpointer        user_data)
{
  GcalManagerPrivate *priv;
  GcalManagerUnit *unit;

  GSList *l;
  GSList *events_data;

  ECalClient *client;
  const gchar *source_uid;
  gchar *event_uuid;

  priv = GCAL_MANAGER (user_data)->priv;
  events_data = NULL;

  client = e_cal_client_view_get_client (view);
  source_uid = e_source_peek_uid (e_client_get_source (E_CLIENT (client)));
  unit = (GcalManagerUnit*) g_hash_table_lookup (priv->clients, source_uid);

  for (l = objects; l != NULL; l = l->next)
    {
      if (l->data != NULL)
        {
          ECalComponent *component;

          if (! g_hash_table_lookup_extended (unit->events,
                                              icalcomponent_get_uid (l->data),
                                              NULL,
                                              NULL))
            {
              component = e_cal_component_new_from_icalcomponent (
                  icalcomponent_new_clone (l->data));
              g_hash_table_insert (unit->events,
                                   g_strdup (icalcomponent_get_uid (l->data)),
                                   component);
              event_uuid = g_strdup_printf ("%s:%s",
                                           source_uid,
                                           icalcomponent_get_uid (l->data));
              events_data = g_slist_append (events_data, event_uuid);
            }
        }
    }

  if (events_data != NULL)
    {
      g_signal_emit (GCAL_MANAGER (user_data),
                     signals[EVENTS_ADDED],
                     0,
                     events_data);

      g_slist_free_full (events_data, g_free);
    }
}

/**
 * gcal_manager_on_view_objects_removed
 * @view: the view emitting the signal
 * @objects: a GSList of ECalComponentId*
 * @user_data: The data passed when connecting the signal, here GcalManager
 */
static void
gcal_manager_on_view_objects_removed (ECalClientView *view,
                                      gpointer        objects,
                                      gpointer        user_data)
{
}

/**
 * gcal_manager_on_view_objects_modified
 * @view: the view emitting the signal
 * @objects: a GSList of icalcomponent*
 * @user_data: The data passed when connecting the signal, here GcalManager
 */
static void
gcal_manager_on_view_objects_modified (ECalClientView *view,
                                       gpointer        objects,
                                       gpointer        user_data)
{
}

static void
gcal_manager_on_sources_row_changed (GtkTreeModel *store,
                                     GtkTreePath  *path,
                                     GtkTreeIter  *iter,
                                     gpointer      user_data)
{
  GcalManagerPrivate *priv;
  GcalManagerUnit *unit;
  gchar *source_uid;
  gboolean active;

  priv = GCAL_MANAGER (user_data)->priv;
  gtk_tree_model_get (store,
                      iter,
                      COLUMN_UID,
                      &source_uid,
                      COLUMN_ACTIVE,
                      &active,
                      -1);
  unit = g_hash_table_lookup (priv->clients, source_uid);
  unit->enabled = active;

  g_free (source_uid);
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
  GSList *groups;
  ESourceGroup *selected_group;
  GHashTableIter iter;
  gpointer key;
  gpointer value;
  ESource *source;

  g_return_val_if_fail (GCAL_IS_MANAGER (manager), NULL);
  priv = manager->priv;
  g_return_val_if_fail (GCAL_IS_MANAGER (manager), FALSE);

  selected_group = NULL;
  for (groups = e_source_list_peek_groups (priv->system_sources);
       groups != NULL;
       groups = groups->next)
    {
      if (g_strcmp0 (
            base_uri,
            e_source_group_peek_base_uri (E_SOURCE_GROUP (groups->data))) == 0)
        {
          selected_group = (ESourceGroup*) groups->data;

          g_hash_table_iter_init (&iter, priv->clients);
          while (g_hash_table_iter_next (&iter, &key, &value))
            {
              GcalManagerUnit *unit = (GcalManagerUnit*) value;
              if (g_strcmp0 (
                    e_source_peek_relative_uri (unit->source),
                    relative_uri) == 0)
                {
                  return NULL;
                }
            }
          break;
        }
    }

  if (selected_group == NULL)
    {
      selected_group = e_source_group_new (gcal_get_group_name (base_uri),
                                           base_uri);
      e_source_list_add_group (priv->system_sources, selected_group, -1);
    }

  source = e_source_new (name, relative_uri);
  e_source_group_add_source (selected_group, source, -1);

  gcal_manager_load_source (manager, source);

  return g_strdup (e_source_peek_uid (source));
}

const gchar*
gcal_manager_get_source_name (GcalManager *manager,
                              const gchar *source_uid)
{
  GcalManagerPrivate *priv;
  GcalManagerUnit *unit;

  g_return_val_if_fail (GCAL_IS_MANAGER (manager), NULL);
  priv = manager->priv;

  unit = g_hash_table_lookup (priv->clients, source_uid);
  return e_source_peek_name (unit->source);
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

  g_return_if_fail (GCAL_IS_MANAGER (manager));
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
      gcal_manager_reload_events (manager);
    }
}

gboolean
gcal_manager_exists_event (GcalManager *manager,
                           const gchar *source_uid,
                           const gchar *event_uid)
{
  GcalManagerPrivate *priv;
  GcalManagerUnit *unit;

  g_return_val_if_fail (GCAL_IS_MANAGER (manager), FALSE);
  priv = manager->priv;

  if ((unit = g_hash_table_lookup (priv->clients, source_uid)) != NULL)
    return g_hash_table_lookup (unit->events, event_uid) != NULL;
  return FALSE;
}

icaltimetype*
gcal_manager_get_event_start_date (GcalManager *manager,
                                   const gchar *source_uid,
                                   const gchar *event_uid)
{
  GcalManagerPrivate *priv;
  GcalManagerUnit *unit;
  ECalComponent *event;
  ECalComponentDateTime dt;
  icaltimetype *dtstart;

  g_return_val_if_fail (GCAL_IS_MANAGER (manager), NULL);
  priv = manager->priv;

  unit = g_hash_table_lookup (priv->clients, source_uid);
  event = g_hash_table_lookup (unit->events, event_uid);
  e_cal_component_get_dtstart (event, &dt);
  dtstart = gcal_dup_icaltime (dt.value);

  e_cal_component_free_datetime (&dt);
  return dtstart;
}

gchar*
gcal_manager_get_event_summary (GcalManager *manager,
                                const gchar *source_uid,
                                const gchar *event_uid)
{
  GcalManagerPrivate *priv;
  GcalManagerUnit *unit;
  ECalComponent *event;
  ECalComponentText e_summary;
  gchar *summary;

  g_return_val_if_fail (GCAL_IS_MANAGER (manager), NULL);
  priv = manager->priv;

  unit = g_hash_table_lookup (priv->clients, source_uid);
  event = g_hash_table_lookup (unit->events, event_uid);
  e_cal_component_get_summary (event, &e_summary);
  summary = g_strdup (e_summary.value);

  return summary;
}

gchar**
gcal_manager_get_event_organizer (GcalManager *manager,
                                  const gchar *source_uid,
                                  const gchar *event_uid)
{
  GcalManagerPrivate *priv;
  GcalManagerUnit *unit;
  ECalComponent *event;
  ECalComponentOrganizer e_organizer;
  gchar** values;

  g_return_val_if_fail (GCAL_IS_MANAGER (manager), NULL);
  priv = manager->priv;

  unit = g_hash_table_lookup (priv->clients, source_uid);
  event = g_hash_table_lookup (unit->events, event_uid);
  e_cal_component_get_organizer (event, &e_organizer);

  if (e_organizer.cn == NULL)
    return NULL;

  values = g_new (gchar*, 3);
  values[2] = NULL;
  values[0] = g_strdup (e_organizer.cn);
  values[1] = g_strdup (e_organizer.value);

  return values;
}

gchar*
gcal_manager_get_event_date (GcalManager *manager,
                             const gchar *source_uid,
                             const gchar *event_uid)
{
  GcalManagerPrivate *priv;
  GcalManagerUnit *unit;
  ECalComponent *event;
  ECalComponentDateTime dt;
  icaltimetype *dtstart;
  icaltimetype *dtend;
  struct tm tm_date;
  gchar *date;
  gchar since [128];
  gchar until [128];
  gchar *all_day;

  g_return_val_if_fail (GCAL_IS_MANAGER (manager), NULL);
  priv = manager->priv;

  unit = g_hash_table_lookup (priv->clients, source_uid);
  event = g_hash_table_lookup (unit->events, event_uid);

  e_cal_component_get_dtstart (event, &dt);
  dtstart = gcal_dup_icaltime (dt.value);
  e_cal_component_free_datetime (&dt);

  e_cal_component_get_dtend (event, &dt);
  dtend = gcal_dup_icaltime (dt.value);
  e_cal_component_free_datetime (&dt);

  if (dtstart->is_date == 1 && dtend->is_date == 1)
    {
      all_day = g_strdup (_(" (All day)"));

      tm_date = icaltimetype_to_tm (dtstart);
      e_utf8_strftime_fix_am_pm (since, 128, "%b, %d", &tm_date);
      tm_date = icaltimetype_to_tm (dtstart);
      e_utf8_strftime_fix_am_pm (until, 128, "%b, %d", &tm_date);
    }
  else
    {
      all_day = NULL;

      tm_date = icaltimetype_to_tm (dtstart);
      e_utf8_strftime_fix_am_pm (since, 128, "%b, %d, %l:%M %p", &tm_date);
      tm_date = icaltimetype_to_tm (dtend);
      e_utf8_strftime_fix_am_pm (until, 128, "%b, %d, %l:%M %p", &tm_date);
    }

  date = g_strdup_printf ("%s - %s%s",
                          since,
                          until,
                          all_day == NULL ? "": all_day);

  g_free (all_day);
  g_free (dtstart);
  g_free (dtend);
  return date;
}

const gchar*
gcal_manager_get_event_location (GcalManager *manager,
                                 const gchar *source_uid,
                                 const gchar *event_uid)
{
  GcalManagerPrivate *priv;
  GcalManagerUnit *unit;
  ECalComponent *event;
  const gchar* location;

  g_return_val_if_fail (GCAL_IS_MANAGER (manager), NULL);
  priv = manager->priv;

  unit = g_hash_table_lookup (priv->clients, source_uid);
  event = g_hash_table_lookup (unit->events, event_uid);
  e_cal_component_get_location (event, &location);
  return location;
}

gchar*
gcal_manager_get_event_description (GcalManager *manager,
                                    const gchar *source_uid,
                                    const gchar *event_uid)
{
  GcalManagerPrivate *priv;
  GcalManagerUnit *unit;
  ECalComponent *event;
  GSList *text_list;
  GSList *l;

  gchar *desc;

  g_return_val_if_fail (GCAL_IS_MANAGER (manager), NULL);
  priv = manager->priv;
  desc = NULL;

  unit = g_hash_table_lookup (priv->clients, source_uid);
  event = g_hash_table_lookup (unit->events, event_uid);
  e_cal_component_get_description_list (event, &text_list);

  for (l = text_list; l != NULL; l = l->next)
    {
      if (l->data != NULL)
        {
          ECalComponentText *text;
          gchar *carrier;
          text = l->data;

          if (desc != NULL)
            {
              carrier = g_strconcat (desc, text->value, NULL);
              g_free (desc);
              desc = carrier;
            }
          else
            {
              desc = g_strdup (text->value);
            }
        }
    }

  e_cal_component_free_text_list (text_list);
  return desc;
}

GdkRGBA*
gcal_manager_get_event_color (GcalManager *manager,
                              const gchar *source_uid,
                              const gchar *event_uid)
{
  GcalManagerPrivate *priv;
  GcalManagerUnit *unit;
  GdkRGBA *color;
  GdkColor gdk_color;

  g_return_val_if_fail (GCAL_IS_MANAGER (manager), NULL);
  priv = manager->priv;
  color = g_new0 (GdkRGBA, 1);

  unit = g_hash_table_lookup (priv->clients, source_uid);
  gdk_color_parse (e_source_peek_color_spec (unit->source), &gdk_color);

  color->red = (gdouble) gdk_color.red / 65535;
  color->green = (gdouble) gdk_color.green / 65535;
  color->blue = (gdouble) gdk_color.blue / 65535;
  color->alpha = 1;

  return color;
}

GList*
gcal_manager_get_event_reminders (GcalManager *manager,
                                  const gchar *source_uid,
                                  const gchar *event_uid)
{
  GcalManagerPrivate *priv;
  GcalManagerUnit *unit;
  ECalComponent *event;
  GList *reminders;

  g_return_val_if_fail (GCAL_IS_MANAGER (manager), NULL);
  priv = manager->priv;
  reminders = NULL;

  unit = g_hash_table_lookup (priv->clients, source_uid);
  event = g_hash_table_lookup (unit->events, event_uid);

  if (e_cal_component_has_alarms (event))
    {
      GList *alarms;
      GList *l;

      alarms = e_cal_component_get_alarm_uids (event);
      for (l = alarms; l != NULL; l = l->next)
        {
          gchar *type;
          gint number;
          gchar *time;
          ECalComponentAlarm *alarm;
          ECalComponentAlarmAction action;
          ECalComponentAlarmTrigger trigger;

          type = NULL;
          time = NULL;
          alarm = e_cal_component_get_alarm (event, l->data);

          e_cal_component_alarm_get_action (alarm, &action);
          /* here, this are the tricky bits.
           * We assume a bunch of differents AlarmAction(s) as one:
           * "Notification". In case we need to add extra-refinemen. This is the
           * place to change stuff t*/
          if (action == E_CAL_COMPONENT_ALARM_AUDIO
              || action == E_CAL_COMPONENT_ALARM_DISPLAY)
            type = g_strdup (_("Notification"));
          else if (action == E_CAL_COMPONENT_ALARM_EMAIL)
            type = g_strdup (_("Email"));

          e_cal_component_alarm_get_trigger (alarm, &trigger);
          /* Second part of tricky bits. We will ignore triggers types other
           * than E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_START, and other than
           * before. In case of extra tweaking (almost sure it will be needed)
           * this is the place of doing it. */
          if (trigger.type == E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_START)
            {
              /* cut/pasted/tweaking chunk of code from
               * evo:calendar/gui/e-alarm-list.c +435 */
              if (trigger.u.rel_duration.is_neg && trigger.u.rel_duration.days >= 1)
                {
                  number = trigger.u.rel_duration.days;
                  time = g_strdup (_("days"));
                }

              if (trigger.u.rel_duration.is_neg && trigger.u.rel_duration.weeks >= 1)
                {
                  number = trigger.u.rel_duration.weeks;
                  time = g_strdup (_("weeks"));
                }

              if (trigger.u.rel_duration.is_neg && trigger.u.rel_duration.hours >= 1)
                {
                  number = trigger.u.rel_duration.hours;
                  time = g_strdup (_("hours"));
                }

              if (trigger.u.rel_duration.is_neg && trigger.u.rel_duration.minutes >= 1)
                {
                  number = trigger.u.rel_duration.minutes;
                  time = g_strdup (_("minutes"));
                }
            }

          if (type != NULL && time != NULL)
            {
              gchar *one_reminder;

              one_reminder = g_strdup_printf ("%s %d %s %s",
                                              type,
                                              number,
                                              time,
                                              _("before"));
              reminders = g_list_prepend (reminders, one_reminder);

              g_free (type);
              g_free (time);
            }

          e_cal_component_alarm_free (alarm);
        }
      cal_obj_uid_list_free (alarms);
    }

  reminders = g_list_reverse (reminders);
  return reminders;
}
