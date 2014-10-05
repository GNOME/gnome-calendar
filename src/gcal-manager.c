/* -*- mode: c; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

typedef struct _RetryOpenData
{
  ESource      *source;
  GcalManager  *manager;
} RetryOpenData;

typedef struct _GcalManagerUnit
{
  ECalClient     *client;
  ECalClientView *view;

  gboolean        enabled;
  gboolean        connected;

  /* FIXME: remove me, stub code */
  GHashTable     *events;
} GcalManagerUnit;

typedef struct
{
  /**
   * The list of clients we are managing.
   * Each value is of type GCalStoreUnit
   * And each key is the source uid
   */
  GHashTable      *clients;

  ESourceRegistry *source_registry;

  /* The range of dates defining the query */
  icaltimetype    *initial_date;
  icaltimetype    *final_date;

  /* The active query */
  gchar           *query;

  GCancellable    *async_ops;

  /* timezone */
  icaltimezone    *system_timezone;

  /* uid of pending create event actions */
  gchar           *pending_event_uid;
  gchar           *pending_event_source;
} GcalManagerPrivate;

/* FIXME: review */
struct _DeleteEventData
{
  gchar           *event_uid;
  GcalManagerUnit *unit;
  GcalManager     *manager;
};

typedef struct _DeleteEventData DeleteEventData;

struct _MoveEventData
{
  gchar           *event_uid;
  GcalManagerUnit *unit;
  GcalManagerUnit *new_unit;
  ECalComponent   *new_component;
  GcalManager     *manager;
};

typedef struct _MoveEventData MoveEventData;

/* Signal IDs */
enum
{
  EVENTS_ADDED,
  EVENTS_MODIFIED,
  EVENTS_REMOVED,
  EVENT_CREATED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

void            free_manager_unit_data                    (gpointer         data);

static void     load_source                               (GcalManager     *manager,
                                                           ESource         *source);

static void     on_client_connected                       (GObject         *source_object,
                                                           GAsyncResult    *result,
                                                           gpointer         user_data);

static gboolean retry_connect_on_timeout                  (gpointer         user_data);

static void     recreate_view                             (GcalManager     *manager,
                                                           GcalManagerUnit *unit);

static void     on_view_objects_added                     (ECalClientView  *view,
                                                           gpointer         objects,
                                                           gpointer         user_data);

static void     on_view_objects_removed                   (ECalClientView  *view,
                                                           gpointer         objects,
                                                           gpointer         user_data);

static void     on_view_objects_modified                  (ECalClientView  *view,
                                                           gpointer         objects,
                                                           gpointer         user_data);

static void     remove_source                             (GcalManager     *manager,
                                                           ESource         *source);

/* class_init && init vfuncs */

static void     gcal_manager_constructed                  (GObject         *object);

static void     gcal_manager_finalize                     (GObject         *object);

static void     gcal_manager_on_event_removed             (GObject         *source_object,
                                                           GAsyncResult    *result,
                                                           gpointer         user_data);

static void     gcal_manager_on_event_removed_for_move    (GObject         *source_object,
                                                           GAsyncResult    *result,
                                                           gpointer         user_data);

static void     gcal_manager_on_event_created             (GObject         *source_object,
                                                           GAsyncResult    *result,
                                                           gpointer         user_data);

G_DEFINE_TYPE_WITH_PRIVATE (GcalManager, gcal_manager, G_TYPE_OBJECT)

void
free_manager_unit_data (gpointer data)
{
  GcalManagerUnit *unit;

  unit = (GcalManagerUnit*) data;

  g_clear_object (&(unit->client));
  g_clear_object (&(unit->view));

  g_free (unit);
}

/**
 * load_source:
 * @manager: Manager instance
 * @source: Loaded source
 *
 * Create @GcalManagerUnit data, add it to internal hash of sources.
 * Open/connect to calendars available
 *
 **/
static void
load_source (GcalManager *manager,
             ESource     *source)
{
  GcalManagerPrivate *priv;

  priv = gcal_manager_get_instance_private (manager);

  if (g_hash_table_lookup (priv->clients, source) == NULL)
    {
      GcalManagerUnit *unit;

      unit = g_new0 (GcalManagerUnit, 1);

      g_hash_table_insert (priv->clients, source, unit);
    }
  else
    {
      g_warning ("Skipping already loaded source: %s",
                 e_source_get_uid (source));
    }

  /* NULL: because maybe the operation cannot be really cancelled */
  e_cal_client_connect (source,
                        E_CAL_CLIENT_SOURCE_TYPE_EVENTS, NULL,
                        on_client_connected,
                        manager);
}

static void
on_client_connected (GObject      *source_object,
                     GAsyncResult *result,
                     gpointer      user_data)
{
  ECalClient *client;
  GcalManagerPrivate *priv;
  GError *error;

  ESource *source;

  GcalManagerUnit *unit;

  priv = gcal_manager_get_instance_private (GCAL_MANAGER (user_data));
  source = e_client_get_source (E_CLIENT (source_object));

  error = NULL;
  client = E_CAL_CLIENT (e_cal_client_connect_finish (result, &error));
  if (error == NULL)
    {
      unit = (GcalManagerUnit*) g_hash_table_lookup (priv->clients,
                                                     source);
      unit->connected = TRUE;
      unit->client = client;

      /* FIXME: user should be able to disable sources */
      unit->enabled = FALSE;

      g_debug ("Source %s connected",
               e_source_get_display_name (source));

      /* setting view */
      recreate_view (GCAL_MANAGER (user_data), unit);
    }
  else
    {
      if (error != NULL)
        {
          g_warning ("%s", error->message);

          /* handling error */
          if (g_error_matches (error, E_CLIENT_ERROR,
                               E_CLIENT_ERROR_BUSY))
            {
              RetryOpenData *rod;

              rod = g_new0 (RetryOpenData, 1);
              rod->source = source;
              rod->manager = user_data;

              g_timeout_add_seconds (2, retry_connect_on_timeout, rod);

              g_error_free (error);
              return;
            }

          /* in any other case, remove it*/
          remove_source (GCAL_MANAGER (user_data), source);
          g_warning ("%s: Failed to open/connect '%s': %s",
                     G_STRFUNC,
                     e_source_get_display_name (source),
                     error->message);

          g_object_unref (source);
          g_error_free (error);
          return;
        }
    }
}

static gboolean
retry_connect_on_timeout (gpointer user_data)
{
  ESource *source;
  GcalManager *manager;

  RetryOpenData *rod = user_data;

  g_return_val_if_fail (rod != NULL, FALSE);
  g_return_val_if_fail (rod->source != NULL, FALSE);
  g_return_val_if_fail (rod->manager != NULL, FALSE);

  source = rod->source;
  manager = rod->manager;
  g_free (user_data);

  e_cal_client_connect (source,
                        E_CAL_CLIENT_SOURCE_TYPE_EVENTS, NULL,
                        on_client_connected,
                        manager);

  return FALSE;
}

static void
recreate_view (GcalManager     *manager,
               GcalManagerUnit *unit)
{
  GcalManagerPrivate *priv;
  GError *error;

  priv = gcal_manager_get_instance_private (manager);
  g_return_if_fail (priv->query != NULL);

  /* stopping */
  g_clear_object (&(unit->view));

  error = NULL;
  if (e_cal_client_get_view_sync (unit->client,
                                  priv->query,
                                  &(unit->view),
                                  priv->async_ops,
                                  &error))
    {
      /*hooking signals */
      g_signal_connect (unit->view,
                        "objects-added",
                        G_CALLBACK (on_view_objects_added),
                        manager);

      g_signal_connect (unit->view,
                        "objects-removed",
                        G_CALLBACK (on_view_objects_removed),
                        manager);

      g_signal_connect (unit->view,
                        "objects-modified",
                        G_CALLBACK (on_view_objects_modified),
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
 * on_view_objects_added
 * @view: the view emitting the signal
 * @objects: a #GSList of icalcomponent*
 * @user_data: the #GcalManager instance
 */
static void
on_view_objects_added (ECalClientView *view,
                       gpointer        objects,
                       gpointer        user_data)
{
  GcalManagerPrivate *priv;

  ESource *source;
  GcalManagerUnit *unit;

  GSList *l;
  GSList *events_data;

  ECalClient *client;

  priv = gcal_manager_get_instance_private (GCAL_MANAGER (user_data));
  events_data = NULL;

  client = e_cal_client_view_ref_client (view);
  source = e_client_get_source (E_CLIENT (client));

  unit = (GcalManagerUnit*) g_hash_table_lookup (priv->clients,
                                                 source);

  for (l = objects; l != NULL; l = l->next)
    {
      if (l->data != NULL && unit->enabled)
        {
          GcalEventData *data;

          data = g_new0 (GcalEventData, 1);
          data->source = source;
          data->event_component =
            e_cal_component_new_from_icalcomponent (
                 icalcomponent_new_clone (l->data));
          events_data = g_slist_append (events_data, data);
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

  g_object_unref (client);
}

/**
 * on_view_objects_removed
 * @view: the view emitting the signal
 * @objects: a GSList of ECalComponentId*
 * @user_data: a #GcalManager instance
 */
static void
on_view_objects_removed (ECalClientView *view,
                         gpointer        objects,
                         gpointer        user_data)
{
  GSList *l;
  GSList *events_data;

  ECalClient *client;
  const gchar *source_uid;

  events_data = NULL;
  client = e_cal_client_view_ref_client (view);
  source_uid = e_source_get_uid (e_client_get_source (E_CLIENT (client)));

  for (l = objects; l != NULL; l = l->next)
    {
      gchar *removed_event_uuid =
        g_strdup_printf ("%s:%s",
                         source_uid,
                         ((ECalComponentId*)(l->data))->uid);
      events_data = g_slist_append (events_data, removed_event_uuid);
    }

  if (events_data != NULL)
    {
      g_signal_emit (GCAL_MANAGER (user_data),
                     signals[EVENTS_REMOVED],
                     0,
                     events_data);

      g_slist_free_full (events_data, g_free);
    }

  g_object_unref (client);
}

/**
 * on_view_objects_modified
 * @view: the view emitting the signal
 * @objects: a GSList of icalcomponent*
 * @user_data: The data passed when connecting the signal, here GcalManager
 */
static void
on_view_objects_modified (ECalClientView *view,
                          gpointer        objects,
                          gpointer        user_data)
{
  GSList *l;
  GSList *events_data;

  ECalClient *client;
  const gchar *source_uid;

  events_data = NULL;
  client = e_cal_client_view_ref_client (view);
  source_uid = e_source_get_uid (e_client_get_source (E_CLIENT (client)));

  for (l = objects; l != NULL; l = l->next)
    {
      if (l->data != NULL)
        {
          gchar* event_uuid;
          event_uuid = g_strdup_printf ("%s:%s",
                                        source_uid,
                                        icalcomponent_get_uid (l->data));
          events_data = g_slist_append (events_data, event_uuid);
        }
    }

  if (events_data != NULL)
    {
      g_signal_emit (GCAL_MANAGER (user_data),
                     signals[EVENTS_MODIFIED],
                     0,
                     events_data);

      g_slist_free_full (events_data, g_free);
    }

  g_object_unref (client);
}

void
remove_source (GcalManager  *manager,
               ESource      *source)
{
  GcalManagerPrivate *priv;

  g_return_if_fail (GCAL_IS_MANAGER (manager));
  g_return_if_fail (E_IS_SOURCE (source));

  priv = gcal_manager_get_instance_private (manager);

  g_hash_table_remove (priv->clients, source);
}

static void
gcal_manager_class_init (GcalManagerClass *klass)
{
  G_OBJECT_CLASS (klass)->constructed = gcal_manager_constructed;
  G_OBJECT_CLASS (klass)->finalize = gcal_manager_finalize;

  /**
   * GcalManager::objects-added:
   *
   * The list passed to the signalis composed of #GcalEventData
   * structures keeping a volatile pointer to the source of the event
   * and an owned copy of an #EcalComponent
   *
   * @manager: the #GcalManager instance which emitted the signal
   * @objects: (type GSList) (transfer none) (element-type long):
   */
  signals[EVENTS_ADDED] =
    g_signal_new ("events-added",
                  GCAL_TYPE_MANAGER,
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GcalManagerClass,
                                   events_added),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__POINTER,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_POINTER);

  signals[EVENTS_MODIFIED] =
    g_signal_new ("events-modified",
                  GCAL_TYPE_MANAGER,
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GcalManagerClass,
                                   events_modified),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__POINTER,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_POINTER);

  signals[EVENTS_REMOVED] =
    g_signal_new ("events-removed",
                  GCAL_TYPE_MANAGER,
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GcalManagerClass,
                                   events_removed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__POINTER,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_POINTER);

  signals[EVENT_CREATED] =
    g_signal_new ("event-created",
                  GCAL_TYPE_MANAGER,
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GcalManagerClass,
                                   event_created),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  2,
                  G_TYPE_POINTER,
                  G_TYPE_POINTER);
}

static void
gcal_manager_init (GcalManager *self)
{
  GcalManagerPrivate *priv;

  priv = gcal_manager_get_instance_private (self);

  priv->system_timezone = e_cal_util_get_system_timezone ();

  priv->clients = g_hash_table_new_full ((GHashFunc) e_source_hash,
                                         (GEqualFunc) e_source_equal,
                                         NULL,
                                         free_manager_unit_data);
}

static void
gcal_manager_constructed (GObject *object)
{
  GcalManagerPrivate *priv;

  GError *error;
  GList *sources;
  GList *l;

  if (G_OBJECT_CLASS (gcal_manager_parent_class)->constructed != NULL)
    G_OBJECT_CLASS (gcal_manager_parent_class)->constructed (object);

  priv = gcal_manager_get_instance_private (GCAL_MANAGER (object));

  error = NULL;
  priv->source_registry = e_source_registry_new_sync (NULL, &error);
  if (priv->source_registry == NULL)
    {
      g_warning ("Failed to access calendar configuration: %s", error->message);
      g_error_free (error);
      return;
    }

  sources = e_source_registry_list_sources (priv->source_registry,
                                            E_SOURCE_EXTENSION_CALENDAR);

  for (l = sources; l != NULL; l = l->next)
    load_source (GCAL_MANAGER (object), l->data);

  g_list_free (sources);

  g_signal_connect_swapped (priv->source_registry,
                            "source-added",
                            G_CALLBACK (load_source),
                            object);

  g_signal_connect_swapped (priv->source_registry,
                            "source-removed",
                            G_CALLBACK (remove_source),
                            object);
}

static void
gcal_manager_finalize (GObject *object)
{
  GcalManagerPrivate *priv;

  priv = gcal_manager_get_instance_private (GCAL_MANAGER (object));

  g_hash_table_destroy (priv->clients);
}

static void
gcal_manager_on_event_removed (GObject      *source_object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  ECalClient *client;
  DeleteEventData *data;
  GError *error;

  client = E_CAL_CLIENT (source_object);
  data = (DeleteEventData*) user_data;

  error = NULL;
  if (e_cal_client_remove_object_finish (client, result, &error))
    {
      /* removing events from hash */
      /* FIXME: add notification to UI */
      g_debug ("Found and removed: %s", data->event_uid);
    }
  else
    {
      //FIXME: do something when there was some error
      ;
    }

  g_free (data->event_uid);
  g_free (data);
}

static void
gcal_manager_on_event_removed_for_move (GObject      *source_object,
                                        GAsyncResult *result,
                                        gpointer      user_data)
{
  GcalManagerPrivate *priv;
  ECalClient *client;
  MoveEventData *data;
  GError *error;

  client = E_CAL_CLIENT (source_object);
  data = (MoveEventData*) user_data;
  priv = gcal_manager_get_instance_private (data->manager);

  error = NULL;
  if (e_cal_client_remove_object_finish (client, result, &error))
    {
      icalcomponent *new_event_icalcomp;

      e_cal_component_commit_sequence (data->new_component);

      new_event_icalcomp =
        e_cal_component_get_icalcomponent (data->new_component);
      e_cal_client_create_object (data->new_unit->client,
                                  new_event_icalcomp,
                                  priv->async_ops,
                                  gcal_manager_on_event_created,
                                  data->manager);
      g_object_unref (data->new_component);
    }
  else
    {
      //FIXME: do something when there was some error
      ;
    }

  g_free (data->event_uid);
  g_free (data);
}

static void
gcal_manager_on_event_created (GObject      *source_object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  ECalClient *client;
  gchar *new_uid;
  GError *error;

  client = E_CAL_CLIENT (source_object);
  error = NULL;
  if (! e_cal_client_create_object_finish (client, result, &new_uid, &error))
    {
      /* Some error */
      g_warning ("Error creating object: %s", error->message);
      g_error_free (error);
    }
  g_free (new_uid);
}

static void
gcal_manager_on_event_modified (GObject      *source_object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  ECalClient *client;
  GError *error;

  /* If the view catch a signal of the newly modified object then
     dunno what's the purposes of this pieces of code, other than
     finish the async ops. */

  client = E_CAL_CLIENT (source_object);
  error = NULL;
  if (e_cal_client_modify_object_finish (client, result, &error))
    {
      g_debug ("Call to modify finished");
    }
  else
    {
      /* Some error */
      g_warning ("Error modifying object: %s", error->message);
      g_error_free (error);
    }
}

/* Public API */
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
  /* FIXME: stub code, since we don't keep a calendars list-store
   anymore */
  return NULL;
}

icaltimezone*
gcal_manager_get_system_timezone (GcalManager *manager)
{
  GcalManagerPrivate *priv;

  priv = gcal_manager_get_instance_private (manager);
  return priv->system_timezone;
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
                         const gchar *backend,
                         const gchar *color)
{
  GcalManagerPrivate *priv;
  ESource *source;
  ESourceCalendar *extension;
  GError *error;

  priv = gcal_manager_get_instance_private (manager);

  source = e_source_new (NULL, NULL, NULL);
  extension = E_SOURCE_CALENDAR (e_source_get_extension (source,
                                                         E_SOURCE_EXTENSION_CALENDAR));

  g_object_set (extension,
                "backend-name", backend,
                "color", color,
                NULL);

  e_source_set_display_name (source, name);

  error = NULL;
  e_source_registry_commit_source_sync (priv->source_registry,
                                        source,
                                        NULL,
                                        &error);
  if (error)
    {
      g_warning ("Failed to store calendar configuration: %s",
                 error->message);
      g_error_free (error);
      g_object_unref (source);
      return NULL;
    }

  load_source (manager, source);
  return e_source_dup_uid (source);
}

gchar*
gcal_manager_get_default_source (GcalManager *manager)
{
  GcalManagerPrivate *priv;

  ESource *edefault;
  gchar *source_uid;

  priv = gcal_manager_get_instance_private (manager);

  edefault = e_source_registry_ref_default_calendar (priv->source_registry);
  source_uid = e_source_dup_uid (edefault);

  g_object_unref (edefault);
  return source_uid;
}

gboolean
gcal_manager_get_source_readonly (GcalManager *manager,
                                  const gchar *source_uid)
{
  GcalManagerPrivate *priv;
  GcalManagerUnit *unit;

  priv = gcal_manager_get_instance_private (manager);

  unit = g_hash_table_lookup (priv->clients, source_uid);
  return e_client_is_readonly (E_CLIENT (unit->client));
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
 * In case the new range fits inside the old one,
 * then the method generate a fake ::events-added signal.
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
  gchar *since_iso8601;
  gchar *until_iso8601;
  GHashTableIter iter;
  gpointer key;
  gpointer value;

  priv = gcal_manager_get_instance_private (manager);

  /* rebuild query */
  since_iso8601 =
    isodate_from_time_t (icaltime_as_timet_with_zone (*initial_date,
                                                      priv->system_timezone));

  until_iso8601 =
    isodate_from_time_t (icaltime_as_timet_with_zone (*final_date,
                                                      priv->system_timezone));

  if (priv->query != NULL)
    g_free (priv->query);

  priv->query = g_strdup_printf ("occur-in-time-range? "
                                 "(make-time \"%s\") "
                                 "(make-time \"%s\")",
                                 since_iso8601,
                                 until_iso8601);

  g_free (since_iso8601);
  g_free (until_iso8601);
  g_debug ("Reload query %s", priv->query);

  /* redoing query */
  g_hash_table_iter_init (&iter, priv->clients);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      GcalManagerUnit *unit = (GcalManagerUnit*) value;
      if (unit->connected)
        recreate_view (manager, unit);
    }
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

  priv = gcal_manager_get_instance_private (manager);

  unit = g_hash_table_lookup (priv->clients, source_uid);
  event = g_hash_table_lookup (unit->events, event_uid);
  e_cal_component_get_dtstart (event, &dt);
  dtstart = gcal_dup_icaltime (dt.value);

  if (dtstart->is_date != 1)
    *dtstart = icaltime_convert_to_zone (*(dt.value), priv->system_timezone);

  e_cal_component_free_datetime (&dt);
  return dtstart;
}

/**
 * gcal_manager_get_event_end_date:
 * @manager: a #GcalManager object
 * @source_uid: the uid of the #ESource
 * @event_uid: the uid of the #ECalComponent
 *
 * Get the date/time end of a calendar event
 *
 * Returns: (transfer full) An #icaltimetype object, free with g_free().
 **/
icaltimetype*
gcal_manager_get_event_end_date (GcalManager *manager,
                                 const gchar *source_uid,
                                 const gchar *event_uid)
{
  GcalManagerPrivate *priv;
  GcalManagerUnit *unit;
  ECalComponent *event;
  ECalComponentDateTime dt;
  icaltimetype *dtend;

  priv = gcal_manager_get_instance_private (manager);

  unit = g_hash_table_lookup (priv->clients, source_uid);
  event = g_hash_table_lookup (unit->events, event_uid);
  e_cal_component_get_dtend (event, &dt);
  dtend = gcal_dup_icaltime (dt.value);

  if (dtend->is_date != 1)
    *dtend = icaltime_convert_to_zone (*(dt.value), priv->system_timezone);

  e_cal_component_free_datetime (&dt);
  return dtend;
}

/**
 * gcal_manager_get_event_summary:
 * @manager: a #GcalManager object
 * @source_uid: the uid of the #ESource
 * @event_uid: the uid of the #ECalComponent
 *
 * Returns the summary of a calendar event.
 *
 * Returns: (transfer full) a c-string to be freed with g_free()
 **/
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

  priv = gcal_manager_get_instance_private (manager);

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

  priv = gcal_manager_get_instance_private (manager);

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

const gchar*
gcal_manager_get_event_location (GcalManager *manager,
                                 const gchar *source_uid,
                                 const gchar *event_uid)
{
  GcalManagerPrivate *priv;
  GcalManagerUnit *unit;
  ECalComponent *event;
  const gchar* location;

  priv = gcal_manager_get_instance_private (manager);

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

  priv = gcal_manager_get_instance_private (manager);
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
  ESource *source;
  ESourceSelectable *extension;
  GdkRGBA *color;

  priv = gcal_manager_get_instance_private (manager);
  source = e_source_registry_ref_source (priv->source_registry,
                                         source_uid);
  color = NULL;

  if (source != NULL)
    {
      color = g_new0 (GdkRGBA, 1);

      extension =
        E_SOURCE_SELECTABLE (e_source_get_extension (source,
                                                     E_SOURCE_EXTENSION_CALENDAR));
      gdk_rgba_parse (color, e_source_selectable_get_color (extension));

      g_object_unref (source);
    }

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

  priv = gcal_manager_get_instance_private (manager);
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
	  number = 0;
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

gboolean
gcal_manager_has_event_reminders (GcalManager *manager,
                                  const gchar *source_uid,
                                  const gchar *event_uid)
{
  GcalManagerPrivate *priv;
  GcalManagerUnit *unit;
  ECalComponent *event;

  priv = gcal_manager_get_instance_private (manager);

  unit = g_hash_table_lookup (priv->clients, source_uid);
  event = g_hash_table_lookup (unit->events, event_uid);
  return e_cal_component_has_alarms (event);
}

gboolean
gcal_manager_get_event_all_day (GcalManager *manager,
                                const gchar *source_uid,
                                const gchar *event_uid)
{
  GcalManagerPrivate *priv;
  GcalManagerUnit *unit;
  ECalComponent *event;

  ECalComponentDateTime dt;
  icaltimetype *dtstart;
  icaltimetype *dtend;

  gboolean all_day;

  priv = gcal_manager_get_instance_private (manager);

  unit = g_hash_table_lookup (priv->clients, source_uid);
  event = g_hash_table_lookup (unit->events, event_uid);

  e_cal_component_get_dtstart (event, &dt);
  dtstart = gcal_dup_icaltime (dt.value);
  e_cal_component_free_datetime (&dt);

  e_cal_component_get_dtend (event, &dt);
  dtend = gcal_dup_icaltime (dt.value);
  e_cal_component_free_datetime (&dt);

  all_day = (dtstart->is_date == 1) && (dtend->is_date == 1);

  g_free (dtstart);
  g_free (dtend);
  return all_day;
}

void
gcal_manager_remove_event (GcalManager *manager,
                           const gchar *source_uid,
                           const gchar *event_uid)
{
  GcalManagerPrivate *priv;
  GcalManagerUnit *unit;
  ECalComponent *event;

  priv = gcal_manager_get_instance_private (manager);

  unit = g_hash_table_lookup (priv->clients, source_uid);
  event = g_hash_table_lookup (unit->events, event_uid);
  if (event != NULL)
    {
      //FIXME: here sends notifications to everyone.
      //FIXME: reload the events in all the views, etc, etc, etc
      DeleteEventData *data;

      data = g_new0 (DeleteEventData, 1);
      data->event_uid = g_strdup (event_uid);
      data->unit = unit;
      data->manager = manager;
      e_cal_client_remove_object (unit->client,
                                  event_uid,
                                  NULL,
                                  E_CAL_OBJ_MOD_ALL,
                                  priv->async_ops,
                                  gcal_manager_on_event_removed,
                                  data);
    }
}

void
gcal_manager_create_event (GcalManager        *manager,
                           const gchar        *source_uid,
                           const gchar        *summary,
                           const icaltimetype *initial_date,
                           const icaltimetype *final_date)
{
  GcalManagerPrivate *priv;
  GcalManagerUnit *unit;
  ECalComponent *event;
  ECalComponentDateTime dt;
  ECalComponentText summ;
  icalcomponent *new_event_icalcomp;
  icaltimetype *dt_start;

  priv = gcal_manager_get_instance_private (manager);

  unit = g_hash_table_lookup (priv->clients, source_uid);

  event = e_cal_component_new ();
  e_cal_component_set_new_vtype (event, E_CAL_COMPONENT_EVENT);

  dt_start = gcal_dup_icaltime (initial_date);
  dt.value = dt_start;
  dt.tzid = NULL;
  e_cal_component_set_dtstart (event, &dt);

  if (final_date != NULL)
    {
      *dt.value = *final_date;
      e_cal_component_set_dtend (event, &dt);
    }
  else
    {
      icaltime_adjust (dt_start, 1, 0, 0, 0);
      *dt.value = *dt_start;
      e_cal_component_set_dtend (event, &dt);
    }

  summ.altrep = NULL;
  summ.value = summary;
  e_cal_component_set_summary (event, &summ);

  e_cal_component_commit_sequence (event);

  new_event_icalcomp = e_cal_component_get_icalcomponent (event);

  priv->pending_event_source = g_strdup (source_uid);
  priv->pending_event_uid =
    g_strdup (icalcomponent_get_uid (new_event_icalcomp));

  e_cal_client_create_object (unit->client,
                              new_event_icalcomp,
                              priv->async_ops,
                              gcal_manager_on_event_created,
                              manager);

  g_object_unref (event);
}

void
gcal_manager_set_event_start_date (GcalManager        *manager,
                                   const gchar        *source_uid,
                                   const gchar        *event_uid,
                                   const icaltimetype *initial_date)
{
  GcalManagerPrivate *priv;
  GcalManagerUnit *unit;
  ECalComponent *event;
  ECalComponentDateTime dt;
  icaltimetype *dt_start;

  priv = gcal_manager_get_instance_private (manager);

  unit = g_hash_table_lookup (priv->clients, source_uid);
  event = g_hash_table_lookup (unit->events, event_uid);

  dt_start = gcal_dup_icaltime (initial_date);
  dt.value = dt_start;
  dt.tzid = NULL;
  e_cal_component_set_dtstart (event, &dt);

  e_cal_component_commit_sequence (event);

  e_cal_client_modify_object (unit->client,
                              e_cal_component_get_icalcomponent (event),
                              E_CAL_OBJ_MOD_ALL,
                              NULL,
                              gcal_manager_on_event_modified,
                              manager);
}

void
gcal_manager_set_event_end_date (GcalManager        *manager,
                                 const gchar        *source_uid,
                                 const gchar        *event_uid,
                                 const icaltimetype *initial_date)
{
  GcalManagerPrivate *priv;
  GcalManagerUnit *unit;
  ECalComponent *event;
  ECalComponentDateTime dt;
  icaltimetype *dt_start;

  priv = gcal_manager_get_instance_private (manager);

  unit = g_hash_table_lookup (priv->clients, source_uid);
  event = g_hash_table_lookup (unit->events, event_uid);

  dt_start = gcal_dup_icaltime (initial_date);
  dt.value = dt_start;
  dt.tzid = NULL;
  e_cal_component_set_dtend (event, &dt);

  e_cal_component_commit_sequence (event);

  e_cal_client_modify_object (unit->client,
                              e_cal_component_get_icalcomponent (event),
                              E_CAL_OBJ_MOD_ALL,
                              NULL,
                              gcal_manager_on_event_modified,
                              manager);
}

void
gcal_manager_set_event_summary (GcalManager *manager,
                                const gchar *source_uid,
                                const gchar *event_uid,
                                const gchar *summary)
{
  GcalManagerPrivate *priv;
  GcalManagerUnit *unit;
  ECalComponent *event;
  ECalComponentText e_summary;

  priv = gcal_manager_get_instance_private (manager);

  unit = g_hash_table_lookup (priv->clients, source_uid);
  event = g_hash_table_lookup (unit->events, event_uid);

  e_summary.altrep = NULL;
  e_summary.value = summary;
  e_cal_component_set_summary (event, &e_summary);

  e_cal_component_commit_sequence (event);

  e_cal_client_modify_object (unit->client,
                              e_cal_component_get_icalcomponent (event),
                              E_CAL_OBJ_MOD_ALL,
                              NULL,
                              gcal_manager_on_event_modified,
                              manager);
}

void
gcal_manager_set_event_location (GcalManager *manager,
                                 const gchar *source_uid,
                                 const gchar *event_uid,
                                 const gchar *location)
{
  GcalManagerPrivate *priv;
  GcalManagerUnit *unit;
  ECalComponent *event;

  priv = gcal_manager_get_instance_private (manager);

  unit = g_hash_table_lookup (priv->clients, source_uid);
  event = g_hash_table_lookup (unit->events, event_uid);

  e_cal_component_set_location (event, location);

  e_cal_component_commit_sequence (event);

  e_cal_client_modify_object (unit->client,
                              e_cal_component_get_icalcomponent (event),
                              E_CAL_OBJ_MOD_ALL,
                              NULL,
                              gcal_manager_on_event_modified,
                              manager);
}

void
gcal_manager_set_event_description (GcalManager *manager,
                                    const gchar *source_uid,
                                    const gchar *event_uid,
                                    const gchar *description)
{
  GcalManagerPrivate *priv;
  GcalManagerUnit *unit;
  ECalComponent *event;
  GSList l;
  ECalComponentText desc;

  priv = gcal_manager_get_instance_private (manager);

  unit = g_hash_table_lookup (priv->clients, source_uid);
  event = g_hash_table_lookup (unit->events, event_uid);

  desc.value = description;
  desc.altrep = NULL;
  l.data = &desc;
  l.next = NULL;

  e_cal_component_set_description_list (event, &l);

  e_cal_component_commit_sequence (event);

  e_cal_client_modify_object (unit->client,
                              e_cal_component_get_icalcomponent (event),
                              E_CAL_OBJ_MOD_ALL,
                              NULL,
                              gcal_manager_on_event_modified,
                              manager);
}

void
gcal_manager_move_event_to_source (GcalManager *manager,
                                   const gchar *source_uid,
                                   const gchar *event_uid,
                                   const gchar *new_source_uid)
{
  GcalManagerPrivate *priv;
  GcalManagerUnit *unit;
  GcalManagerUnit *new_unit;
  ECalComponent *old_event;
  ECalComponent *new_event;

  priv = gcal_manager_get_instance_private (manager);

  unit = g_hash_table_lookup (priv->clients, source_uid);
  old_event = g_hash_table_lookup (unit->events, event_uid);
  new_unit = g_hash_table_lookup (priv->clients, new_source_uid);
  if (old_event != NULL)
    {
      //FIXME: here sends notifications to everyone.
      //FIXME: reload the events in all the views, etc, etc, etc
      MoveEventData *data;

      new_event = e_cal_component_clone (old_event);
      data = g_new0 (MoveEventData, 1);
      data->event_uid = g_strdup (event_uid);
      data->unit = unit;
      data->new_unit = new_unit;
      data->new_component = new_event;
      data->manager = manager;
      e_cal_client_remove_object (unit->client,
                                  event_uid,
                                  NULL,
                                  E_CAL_OBJ_MOD_ALL,
                                  priv->async_ops,
                                  gcal_manager_on_event_removed_for_move,
                                  data);
    }
}
