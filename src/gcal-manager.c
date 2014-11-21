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

typedef struct _GcalManagerUnit
{
  ECalClient     *client;

  gboolean        enabled;
  gboolean        connected;

  /* FIXME: add GdkRGBA colors */
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

  ECalDataModel   *e_data_model;

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
  EVENT_CREATED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void     load_source                               (GcalManager     *manager,
                                                           ESource         *source);

static void     on_client_connected                       (GObject         *source_object,
                                                           GAsyncResult    *result,
                                                           gpointer         user_data);

static void     on_client_refreshed                       (GObject         *source_object,
                                                           GAsyncResult    *result,
                                                           gpointer         user_data);

static void     on_event_updated                         (GObject          *source_object,
                                                          GAsyncResult     *result,
                                                          gpointer          user_data);

static void     remove_source                             (GcalManager     *manager,
                                                           ESource         *source);

/* class_init && init vfuncs */

static void     gcal_manager_finalize                     (GObject         *object);

static void     gcal_manager_on_event_removed             (GObject         *source_object,
                                                           GAsyncResult    *result,
                                                           gpointer         user_data);

static void     gcal_manager_on_event_created             (GObject         *source_object,
                                                           GAsyncResult    *result,
                                                           gpointer         user_data);

G_DEFINE_TYPE_WITH_PRIVATE (GcalManager, gcal_manager, G_TYPE_OBJECT)

/* -- start: threading related code provided by Milan Crha */
typedef struct {
  EThreadJobFunc func;
  gpointer user_data;
  GDestroyNotify free_user_data;

  GCancellable *cancellable;
  GError *error;
} ThreadJobData;

static void
thread_job_data_free (gpointer ptr)
{
  ThreadJobData *tjd = ptr;

  if (tjd != NULL)
    {
      /* This should go to UI with more info/description,
       * if it is not G_IOI_ERROR_CANCELLED */
      if (tjd->error != NULL)
        g_warning ("Job faile: %s\n", tjd->error->message);

      if (tjd->free_user_data != NULL)
        tjd->free_user_data (tjd->user_data);

      g_clear_object (&tjd->cancellable);
      g_clear_error (&tjd->error);
      g_free (tjd);
    }
}

static gpointer
thread_job_thread (gpointer user_data)
{
  ThreadJobData *tjd = user_data;

  g_return_val_if_fail (tjd != NULL, NULL);

  if (tjd->func != NULL)
    tjd->func (tjd->user_data, tjd->cancellable, &tjd->error);

  thread_job_data_free (tjd);

  return g_thread_self ();
}

static GCancellable *
submit_thread_job (EThreadJobFunc func,
                   gpointer user_data,
                   GDestroyNotify free_user_data)
{
  ThreadJobData *tjd;
  GThread *thread;
  GCancellable *cancellable;

  cancellable = g_cancellable_new ();

  tjd = g_new0 (ThreadJobData, 1);
  tjd->func = func;
  tjd->user_data = user_data;
  tjd->free_user_data = free_user_data;
  /* user should be able to cancel this cancellable somehow */
  tjd->cancellable = g_object_ref (cancellable);
  tjd->error = NULL;

  thread = g_thread_try_new (NULL,
                             thread_job_thread, tjd,
                             &tjd->error);
  if (thread != NULL)
    {
      g_thread_unref (thread);
    }
  else
    {
      thread_job_data_free (tjd);
      g_clear_object (&cancellable);
    }

  return cancellable;
}
/* -- end: threading related code provided by Milan Crha -- */

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

      /* NULL: because maybe the operation cannot be really cancelled */
      e_cal_client_connect (source,
                            E_CAL_CLIENT_SOURCE_TYPE_EVENTS, NULL,
                            on_client_connected,
                            manager);
    }
  else
    {
      g_warning ("%s: Skipping already loaded source: %s",
                 G_STRFUNC,
                 e_source_get_uid (source));
    }
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
      unit->enabled = TRUE;

      g_debug ("Source %s (%s) connected",
               e_source_get_display_name (source),
               e_source_get_uid (source));

      e_cal_data_model_add_client (priv->e_data_model, client);
      g_clear_object (&client);
    }
  else
    {
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


static void
on_client_refreshed (GObject      *source_object,
                     GAsyncResult *result,
                     gpointer      user_data)
{
  GError *error;

  error = NULL;
  if (e_client_refresh_finish (E_CLIENT (source_object), result, &error))
    {
      /* FIXME: add notification to UI */
      ;
    }
  else
    {
      /* FIXME: do something when there was some error */
      g_warning ("Error synchronizing client");
    }
}

/**
 * on_component_updated:
 * @source_object: {@link ECalClient} source
 * @result: result of the operation
 * @user_data: manager instance
 *
 * Called when an component is modified. Currently, it only checks for
 * error, but a more sophisticated implementation will come in no time.
 *
 **/
static void
on_event_updated (GObject      *source_object,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  GError *error;

  error = NULL;
  if (! e_cal_client_modify_object_finish (E_CAL_CLIENT (source_object),
                                           result,
                                           &error))
    {
      g_warning ("Error updating component: %s", error->message);
      g_error_free (error);
    }
  g_object_unref (E_CAL_COMPONENT (user_data));
}

void
remove_source (GcalManager  *manager,
               ESource      *source)
{
  GcalManagerPrivate *priv;

  g_return_if_fail (GCAL_IS_MANAGER (manager));
  g_return_if_fail (E_IS_SOURCE (source));

  priv = gcal_manager_get_instance_private (manager);

  e_cal_data_model_remove_client (priv->e_data_model,
                                  e_source_get_uid (source));
  g_hash_table_remove (priv->clients, source);
}

static void
gcal_manager_class_init (GcalManagerClass *klass)
{
  G_OBJECT_CLASS (klass)->finalize = gcal_manager_finalize;

  /* FIXME: check if this is really needed */
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

  GError *error;
  GList *sources;
  GList *l;

  priv = gcal_manager_get_instance_private (self);

  priv->system_timezone = e_cal_util_get_system_timezone ();

  priv->clients = g_hash_table_new_full ((GHashFunc) e_source_hash,
                                         (GEqualFunc) e_source_equal,
                                         g_object_unref,
                                         g_free);

  /* reading sources and schedule its connecting */
  error = NULL;
  priv->source_registry = e_source_registry_new_sync (NULL, &error);
  if (priv->source_registry == NULL)
    {
      g_warning ("Failed to access calendar configuration: %s", error->message);
      g_error_free (error);
      return;
    }

  sources = e_source_registry_list_enabled (priv->source_registry,
                                            E_SOURCE_EXTENSION_CALENDAR);

  for (l = sources; l != NULL; l = l->next)
    load_source (self, l->data);

  g_list_free (sources);

  g_signal_connect_swapped (priv->source_registry,
                            "source-added",
                            G_CALLBACK (load_source),
                            self);

  g_signal_connect_swapped (priv->source_registry,
                            "source-removed",
                            G_CALLBACK (remove_source),
                            self);

  /* create data model */
  priv->e_data_model = e_cal_data_model_new (submit_thread_job);

  e_cal_data_model_set_expand_recurrences (priv->e_data_model, TRUE);
  e_cal_data_model_set_timezone (priv->e_data_model,
                                 priv->system_timezone);
}

static void
gcal_manager_finalize (GObject *object)
{
  GcalManagerPrivate *priv;

  priv = gcal_manager_get_instance_private (GCAL_MANAGER (object));

  if (priv->e_data_model != NULL)
    g_object_unref (priv->e_data_model);

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

void
gcal_manager_set_subscriber (GcalManager             *manager,
                             ECalDataModelSubscriber *subscriber,
                             time_t                   range_start,
                             time_t                   range_end)
{
  GcalManagerPrivate *priv;

  priv = gcal_manager_get_instance_private (manager);
  e_cal_data_model_subscribe (priv->e_data_model,
                              subscriber,
                              range_start, range_end);
}

/**
 * gcal_manager_set_query:
 * @manager: A #GcalManager instance
 * @query: (nullable): query terms or %NULL
 *
 * Set the query terms of the #ECalDataModel or clear it if %NULL is
 * passed
 *
 **/
void
gcal_manager_set_query (GcalManager *manager,
                        const gchar *query)
{
  GcalManagerPrivate *priv;

  priv = gcal_manager_get_instance_private (manager);
  e_cal_data_model_set_filter (priv->e_data_model,
                               query != NULL ? query : "#t");
}

/**
 * gcal_manager_add_source:
 * @manager: a #GcalManager
 * @base_uri: URI defining the ESourceGroup the client will belongs
 * @relative_uri: URI, relative to the base URI
 * @color: a string representing a color parseable for gdk_rgba_parse
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

void
gcal_manager_refresh (GcalManager *manager)
{
  GcalManagerPrivate *priv;
  GList *clients;
  GList *l;

  priv = gcal_manager_get_instance_private (manager);
  clients = g_hash_table_get_values (priv->clients);

  /* refresh clients */
  for (l = clients; l != NULL; l = l->next)
    {
      GcalManagerUnit *unit;

      unit = (GcalManagerUnit*) l->data;

      if (! e_client_check_refresh_supported (E_CLIENT (unit->client)))
        continue;

      e_client_refresh (E_CLIENT (unit->client),
                        NULL,
                        on_client_refreshed,
                        manager);
    }

  g_list_free (clients);
}

gboolean
gcal_manager_is_client_writable (GcalManager *manager,
                                 ESource     *source)
{
  GcalManagerPrivate *priv;
  GcalManagerUnit *unit;

  priv = gcal_manager_get_instance_private (manager);

  unit = g_hash_table_lookup (priv->clients, source);
  return e_client_is_readonly (E_CLIENT (unit->client));
}

void
gcal_manager_remove_event (GcalManager *manager,
                           const gchar *source_uid,
                           const gchar *event_uid)
{
  GcalManagerPrivate *priv;
  GcalManagerUnit *unit;
  DeleteEventData *data;

  priv = gcal_manager_get_instance_private (manager);

  unit = g_hash_table_lookup (priv->clients, source_uid);

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
gcal_manager_update_event (GcalManager   *manager,
                           ESource       *source,
                           ECalComponent *component)
{
  GcalManagerPrivate *priv;
  GcalManagerUnit *unit;

  priv = gcal_manager_get_instance_private (manager);
  unit = (GcalManagerUnit*) g_hash_table_lookup (priv->clients, source);

  e_cal_client_modify_object (unit->client,
                              e_cal_component_get_icalcomponent (component),
                              E_CAL_OBJ_MOD_THIS,
                              NULL,
                              on_event_updated,
                              component);
}

void
gcal_manager_move_event_to_source (GcalManager *manager,
                                   const gchar *source_uid,
                                   const gchar *event_uid,
                                   const gchar *new_source_uid)
{
  /* FIXME: add code, fix stub method  */
  ;
}
