/* -*- mode: c; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gcal-manager.c
 * Copyright (C) 2015 Erick Pérez Castellanos <erickpc@gnome.org>
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

#include <libedataserverui/libedataserverui.h>

typedef struct
{
  ESource       *source;
  ECalComponent *component;
  GcalManager   *manager;
} AsyncOpsData;

typedef struct
{
  ECalDataModelSubscriber *subscriber;
  gchar                   *query;

  guint                    sources_left;
  gboolean                 passed_start;
  gboolean                 search_done;
} ViewStateData;

typedef struct
{
  ECalClient     *client;

  gboolean        enabled;
  gboolean        connected;
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
  ECredentialsPrompter *credentials_prompter;

  ECalDataModel   *e_data_model;
  ECalDataModel   *search_data_model;

  ECalDataModel   *shell_search_data_model;
  ViewStateData   *search_view_data;

  GCancellable    *async_ops;

  /* state flags */
  gchar          **disabled_sources;
  gint             sources_at_launch;

  /* timezone */
  icaltimezone    *system_timezone;

  /* property */
  GSettings       *settings;
} GcalManagerPrivate;

struct _MoveEventData
{
  gchar           *event_uid;
  GcalManagerUnit *unit;
  GcalManagerUnit *new_unit;
  ECalComponent   *new_component;
  GcalManager     *manager;
};

typedef struct _MoveEventData MoveEventData;

enum
{
  PROP_0,
  PROP_SETTINGS,
};

enum
{
  SOURCE_ACTIVATED,
  SOURCE_ADDED,
  SOURCE_REMOVED,
  LOAD_COMPLETED,
  QUERY_COMPLETED,
  NUM_SIGNALS
};

static guint signals[NUM_SIGNALS] = { 0, };

static void     free_async_ops_data                       (AsyncOpsData    *data);

static void     free_unit_data                            (GcalManagerUnit *data);

static void     load_source                               (GcalManager     *manager,
                                                           ESource         *source);

static void     on_client_connected                       (GObject         *source_object,
                                                           GAsyncResult    *result,
                                                           gpointer         user_data);

static void     on_client_readonly_changed                (EClient           *client,
                                                           GParamSpec        *pspec,
                                                           gpointer           user_data);

static void     on_client_refreshed                       (GObject         *source_object,
                                                           GAsyncResult    *result,
                                                           gpointer         user_data);

static void     on_event_created                          (GObject         *source_object,
                                                           GAsyncResult    *result,
                                                           gpointer         user_data);

static void     on_event_updated                         (GObject          *source_object,
                                                          GAsyncResult     *result,
                                                          gpointer          user_data);

static void     on_event_removed                          (GObject         *source_object,
                                                           GAsyncResult    *result,
                                                           gpointer         user_data);

static void     remove_source                             (GcalManager     *manager,
                                                           ESource         *source);

/* class_init && init vfuncs */

static void     gcal_manager_constructed                  (GObject         *object);

static void     gcal_manager_finalize                     (GObject         *object);

static void     gcal_manager_set_property                 (GObject         *object,
                                                           guint            property_id,
                                                           const GValue    *value,
                                                           GParamSpec      *pspec);

static void     gcal_manager_get_property                 (GObject         *object,
                                                           guint            property_id,
                                                           GValue          *value,
                                                           GParamSpec      *pspec);

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
        g_warning ("Job failed: %s\n", tjd->error->message);

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

static void
free_async_ops_data (AsyncOpsData *data)
{
  g_object_unref (data->component);
  g_free (data);
}

static void
free_unit_data (GcalManagerUnit *data)
{
  g_object_unref (data->client);
  g_free (data);
}

static gboolean
gather_components (ECalDataModel         *data_model,
                   ECalClient            *client,
                   const ECalComponentId *id,
                   ECalComponent         *comp,
                   time_t                 instance_start,
                   time_t                 instance_end,
                   gpointer               user_data)
{
  GList **result = user_data;
  GcalEventData *new_data = g_new0 (GcalEventData, 1);

  new_data->source = e_client_get_source (E_CLIENT (client));
  new_data->event_component = g_object_ref (comp);
  *result = g_list_append (*result, new_data);/* FIXME: add me sorted */

  return TRUE;
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
      /* NULL: because maybe the operation cannot be really cancelled */
      e_cal_client_connect (source,
                            E_CAL_CLIENT_SOURCE_TYPE_EVENTS, 1, NULL,
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

  priv->sources_at_launch--;
  if (priv->sources_at_launch == 0)
    g_signal_emit (user_data, signals[LOAD_COMPLETED], 0);

  error = NULL;
  client = E_CAL_CLIENT (e_cal_client_connect_finish (result, &error));
  if (error == NULL)
    {
      unit = g_new0 (GcalManagerUnit, 1);
      unit->connected = TRUE;
      unit->client = g_object_ref (client);

      g_hash_table_insert (priv->clients, source, unit);

      g_debug ("Source %s (%s) connected",
               e_source_get_display_name (source),
               e_source_get_uid (source));

      /* notify the readonly property */
      g_signal_connect (client, "notify::readonly", G_CALLBACK (on_client_readonly_changed), user_data);

      if (g_strv_contains ((const gchar * const *) priv->disabled_sources, e_source_get_uid (source)))
        {
          unit->enabled = FALSE;
        }
      else
        {
          unit->enabled = TRUE;

          e_cal_data_model_add_client (priv->e_data_model, client);
          e_cal_data_model_add_client (priv->search_data_model, client);
          if (priv->shell_search_data_model != NULL)
            e_cal_data_model_add_client (priv->shell_search_data_model, client);
        }

      /* refresh client when it's added */
      if (unit->enabled && e_client_check_refresh_supported (E_CLIENT (client)))
      {
        e_client_refresh (E_CLIENT (client), NULL, on_client_refreshed, user_data);
      }

      g_signal_emit (GCAL_MANAGER (user_data), signals[SOURCE_ADDED], 0, source, unit->enabled);

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
on_client_readonly_changed (EClient    *client,
                            GParamSpec *pspec,
                            gpointer    user_data)
{
  GcalManagerPrivate *priv;
  ESource *source;
  GcalManagerUnit *unit;

  priv = gcal_manager_get_instance_private (GCAL_MANAGER (user_data));
  source = e_client_get_source (client);

  unit = g_hash_table_lookup (priv->clients, source);
  if (unit->enabled)
    g_signal_emit (GCAL_MANAGER (user_data), signals[SOURCE_ACTIVATED], 0, source, !e_client_is_readonly (client));
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
      ESource *source = e_client_get_source (E_CLIENT (source_object));
      /* FIXME: add notification to UI */
      g_debug ("Client of source: %s refreshed succesfully", e_source_get_uid (source));
    }
  else
    {
      /* FIXME: do something when there was some error */
      g_warning ("Error synchronizing client");
    }
}

static void
on_event_created (GObject      *source_object,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  AsyncOpsData *data;
  ECalClient *client;
  gchar *new_uid;
  GError *error;

  data = (AsyncOpsData*) user_data;
  client = E_CAL_CLIENT (source_object);
  error = NULL;

  if (!e_cal_client_create_object_finish (client, result, &new_uid, &error))
    {
      /* Some error */
      g_warning ("Error creating object: %s", error->message);
      g_error_free (error);
    }
  g_debug ("Event: %s created successfully", new_uid);

  g_free (new_uid);
  free_async_ops_data (data);
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

/**
 * on_event_removed:
 * @source_object: {@link ECalClient} source
 * @result: result of the operation
 * @user_data: an {@link ECalComponent}
 *
 * Called when an component is removed. Currently, it only checks for
 * error, but a more sophisticated implementation will come in no time.*
 *
 **/
static void
on_event_removed (GObject      *source_object,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  ECalClient *client;
  GError *error;

  client = E_CAL_CLIENT (source_object);

  error = NULL;
  if (!e_cal_client_remove_object_finish (client, result, &error))
    {
      /* FIXME: Notify the user somehow */
      g_warning ("Error removing event: %s", error->message);
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
  e_cal_data_model_remove_client (priv->search_data_model,
                                  e_source_get_uid (source));
  g_hash_table_remove (priv->clients, source);
  g_signal_emit (manager, signals[SOURCE_REMOVED], 0, source);
}

static void
model_state_changed (GcalManager            *manager,
                     ECalClientView         *view,
                     ECalDataModelViewState  state,
                     guint                   percent,
                     const gchar            *message,
                     const GError           *error,
                     ECalDataModel          *data_model)
{
  GcalManagerPrivate *priv = gcal_manager_get_instance_private (manager);
  gchar *filter = e_cal_data_model_dup_filter (data_model);
  if (state == E_CAL_DATA_MODEL_VIEW_STATE_START && g_strcmp0 (priv->search_view_data->query, filter) == 0)
    {
      priv->search_view_data->passed_start = TRUE;
      goto out;
    }

  if (priv->search_view_data->passed_start && state == E_CAL_DATA_MODEL_VIEW_STATE_COMPLETE &&
      g_strcmp0 (priv->search_view_data->query, filter) == 0)
    {
      priv->search_view_data->sources_left--;
      priv->search_view_data->search_done = (priv->search_view_data->sources_left == 0);
      if (priv->search_view_data->search_done)
        g_signal_emit (manager, signals[QUERY_COMPLETED], 0);
    }

out:
    g_free (filter);
    return;
}

static void
show_source_error (const gchar  *where,
                   const gchar  *what,
                   ESource      *source,
                   const GError *error)
{
  if (!error || g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    return;

  /* TODO Show the error in UI, somehow */
  g_warning ("%s: %s '%s': %s", where, what, e_source_get_display_name (source), error->message);
}

static void
source_invoke_authenticate_cb (GObject      *source_object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  ESource *source = E_SOURCE (source_object);
  GError *error = NULL;

  if (!e_source_invoke_authenticate_finish (source, result, &error) &&
      !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
    show_source_error (G_STRFUNC, "Failed to invoke authenticate", source, error);
  }

  g_clear_error (&error);
}

static void
source_trust_prompt_done_cb (GObject      *source_object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  ETrustPromptResponse response = E_TRUST_PROMPT_RESPONSE_UNKNOWN;
  ESource *source = E_SOURCE (source_object);
  GError *error = NULL;

  if (!e_trust_prompt_run_for_source_finish (source, result, &response, &error)) {
    show_source_error (G_STRFUNC, "Failed to prompt for trust for", source, error);
  } else if (response == E_TRUST_PROMPT_RESPONSE_ACCEPT || response == E_TRUST_PROMPT_RESPONSE_ACCEPT_TEMPORARILY) {
    /* Use NULL credentials to reuse those from the last time. */
    e_source_invoke_authenticate (source, NULL, NULL /* cancellable */, source_invoke_authenticate_cb, NULL);
  }

  g_clear_error (&error);
}

static void
source_credentials_required_cb (ESourceRegistry         *registry,
                                ESource                 *source,
                                ESourceCredentialsReason reason,
                                const gchar             *certificate_pem,
                                GTlsCertificateFlags     certificate_errors,
                                const GError            *op_error,
                                GcalManager             *manager)
{
  GcalManagerPrivate *priv;
  ECredentialsPrompter *credentials_prompter;

  g_return_if_fail (GCAL_IS_MANAGER (manager));

  priv = gcal_manager_get_instance_private (manager);
  credentials_prompter = priv->credentials_prompter;

  if (e_credentials_prompter_get_auto_prompt_disabled_for (credentials_prompter, source))
    return;

  if (reason == E_SOURCE_CREDENTIALS_REASON_SSL_FAILED) {
    e_trust_prompt_run_for_source (e_credentials_prompter_get_dialog_parent (credentials_prompter),
                                   source, certificate_pem, certificate_errors, op_error ? op_error->message : NULL,
                                   TRUE /* allow_source_save */, NULL /* cancellable */, source_trust_prompt_done_cb, NULL);
  } else if (reason == E_SOURCE_CREDENTIALS_REASON_ERROR && op_error) {
    show_source_error (G_STRFUNC, "Failed to authenticate", source, op_error);
  }
}

static void
gcal_manager_class_init (GcalManagerClass *klass)
{
  G_OBJECT_CLASS (klass)->constructed = gcal_manager_constructed;
  G_OBJECT_CLASS (klass)->finalize = gcal_manager_finalize;
  G_OBJECT_CLASS (klass)->set_property = gcal_manager_set_property;
  G_OBJECT_CLASS (klass)->get_property = gcal_manager_get_property;

  /* properties */
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_SETTINGS,
                                   g_param_spec_object ("settings",
                                                        "Application settings",
                                                        "The settings of the application passed down from GcalApplication",
                                                        G_TYPE_SETTINGS,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

  /* signals */
  signals[SOURCE_ACTIVATED] = g_signal_new ("source-activated", GCAL_TYPE_MANAGER, G_SIGNAL_RUN_LAST,
                                           G_STRUCT_OFFSET (GcalManagerClass, source_activated),
                                           NULL, NULL, NULL,
                                           G_TYPE_NONE, 2, G_TYPE_POINTER, G_TYPE_BOOLEAN);

  signals[SOURCE_ADDED] = g_signal_new ("source-added", GCAL_TYPE_MANAGER, G_SIGNAL_RUN_LAST,
                                        G_STRUCT_OFFSET (GcalManagerClass, source_added),
                                        NULL, NULL, NULL,
                                        G_TYPE_NONE, 2, G_TYPE_POINTER, G_TYPE_BOOLEAN);

  signals[SOURCE_REMOVED] = g_signal_new ("source-removed", GCAL_TYPE_MANAGER, G_SIGNAL_RUN_LAST,
                                          G_STRUCT_OFFSET (GcalManagerClass, source_removed),
                                          NULL, NULL, NULL,
                                          G_TYPE_NONE, 1, G_TYPE_POINTER);

  signals[LOAD_COMPLETED] = g_signal_new ("load-completed", GCAL_TYPE_MANAGER, G_SIGNAL_RUN_LAST,
                                          G_STRUCT_OFFSET (GcalManagerClass, load_completed),
                                          NULL, NULL, NULL,
                                          G_TYPE_NONE, 0);

  signals[QUERY_COMPLETED] = g_signal_new ("query-completed", GCAL_TYPE_MANAGER, G_SIGNAL_RUN_LAST,
                                           G_STRUCT_OFFSET (GcalManagerClass, query_completed),
                                           NULL, NULL, NULL,
                                           G_TYPE_NONE, 0);
}

static void
gcal_manager_init (GcalManager *self)
{
  ;
}

static void
gcal_manager_constructed (GObject *object)
{
  GcalManagerPrivate *priv;

  GList *sources, *l;
  GError *error = NULL;
  ESourceCredentialsProvider *credentials_provider;

  G_OBJECT_CLASS (gcal_manager_parent_class)->constructed (object);

  priv = gcal_manager_get_instance_private (GCAL_MANAGER (object));

  priv->disabled_sources = g_settings_get_strv (priv->settings, "disabled-sources");
  priv->system_timezone = e_cal_util_get_system_timezone ();

  priv->clients = g_hash_table_new_full ((GHashFunc) e_source_hash, (GEqualFunc) e_source_equal,
                                         g_object_unref, (GDestroyNotify) free_unit_data);

  /* reading sources and schedule its connecting */
  priv->source_registry = e_source_registry_new_sync (NULL, &error);
  if (priv->source_registry == NULL)
    {
      g_warning ("Failed to access calendar configuration: %s", error->message);
      g_error_free (error);
      return;
    }

  priv->credentials_prompter = e_credentials_prompter_new (priv->source_registry);

  /* First disable credentials prompt for all but calendar sources... */
  sources = e_source_registry_list_sources (priv->source_registry, NULL);

  for (l = sources; l != NULL; l = g_list_next (l))
    {
      ESource *source = E_SOURCE (l->data);

      /* Mark for skip also currently disabled sources */
      if (!e_source_has_extension (source, E_SOURCE_EXTENSION_CALENDAR))
        e_credentials_prompter_set_auto_prompt_disabled_for (priv->credentials_prompter, source, TRUE);
    }

  g_list_free_full (sources, g_object_unref);

  credentials_provider = e_credentials_prompter_get_provider (priv->credentials_prompter);

  /* ...then enable credentials prompt for credential source of the calendar sources,
     which can be a collection source.  */
  sources = e_source_registry_list_sources (priv->source_registry, E_SOURCE_EXTENSION_CALENDAR);

  for (l = sources; l != NULL; l = g_list_next (l))
    {
      ESource *source = E_SOURCE (l->data), *cred_source;

      cred_source = e_source_credentials_provider_ref_credentials_source (credentials_provider, source);
      if (cred_source && !e_source_equal (source, cred_source))
        e_credentials_prompter_set_auto_prompt_disabled_for (priv->credentials_prompter, cred_source, FALSE);
      g_clear_object (&cred_source);
    }

  g_list_free_full (sources, g_object_unref);

  /* The eds_credentials_prompter responses to REQUIRED and REJECTED reasons,
     the SSL_FAILED should be handled elsewhere. */
  g_signal_connect (priv->source_registry, "credentials-required", G_CALLBACK (source_credentials_required_cb), object);

  e_credentials_prompter_process_awaiting_credentials (priv->credentials_prompter);

  g_signal_connect_swapped (priv->source_registry, "source-added", G_CALLBACK (load_source), object);
  g_signal_connect_swapped (priv->source_registry, "source-removed", G_CALLBACK (remove_source), object);

  /* create data model */
  priv->e_data_model = e_cal_data_model_new (submit_thread_job);
  priv->search_data_model = e_cal_data_model_new (submit_thread_job);

  e_cal_data_model_set_expand_recurrences (priv->e_data_model, TRUE);
  e_cal_data_model_set_timezone (priv->e_data_model, priv->system_timezone);
  e_cal_data_model_set_expand_recurrences (priv->search_data_model, TRUE);
  e_cal_data_model_set_timezone (priv->search_data_model, priv->system_timezone);

  sources = e_source_registry_list_enabled (priv->source_registry, E_SOURCE_EXTENSION_CALENDAR);
  priv->sources_at_launch = g_list_length (sources);

  for (l = sources; l != NULL; l = l->next)
    load_source (GCAL_MANAGER (object), l->data);
  g_list_free (sources);
}

static void
gcal_manager_finalize (GObject *object)
{
  GcalManagerPrivate *priv;

  priv = gcal_manager_get_instance_private (GCAL_MANAGER (object));

  if (priv->settings != NULL)
    g_object_unref (priv->settings);
  g_strfreev (priv->disabled_sources);

  if (priv->e_data_model != NULL)
    g_object_unref (priv->e_data_model);
  if (priv->search_data_model != NULL)
    g_object_unref (priv->search_data_model);

  if (priv->search_view_data != NULL)
    {
      g_free (priv->search_view_data->query);
      g_free (priv->search_view_data);
    }
  if (priv->shell_search_data_model != NULL)
    g_object_unref (priv->shell_search_data_model);

  g_hash_table_destroy (priv->clients);
}

static void
gcal_manager_set_property (GObject      *object,
                           guint         property_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  GcalManagerPrivate *priv;

  priv = gcal_manager_get_instance_private (GCAL_MANAGER (object));

  switch (property_id)
    {
    case PROP_SETTINGS:
      if (priv->settings != NULL)
        g_object_unref (priv->settings);
      priv->settings = g_value_dup_object (value);
      return;
    }

  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
gcal_manager_get_property (GObject    *object,
                           guint       property_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  GcalManagerPrivate *priv;

  priv = gcal_manager_get_instance_private (GCAL_MANAGER (object));

  switch (property_id)
    {
    case PROP_SETTINGS:
      g_value_set_object (value, priv->settings);
      return;
    }

  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

/* Public API */
/**
 * gcal_manager_new_with_settings:
 * @settings:
 *
 * Since: 0.0.1
 * Return value: the new #GcalManager
 * Returns: (transfer full):
 **/
GcalManager*
gcal_manager_new_with_settings (GSettings *settings)
{
  return GCAL_MANAGER (g_object_new (GCAL_TYPE_MANAGER, "settings", settings, NULL));
}

/**
 * gcal_manager_get_sources:
 * @manager:
 *
 * Retrieve a list of the enabled sources used in the application.
 *
 * Returns: (Transfer full) a {@link GList} object to be freed with g_list_free()
 **/
GList*
gcal_manager_get_sources (GcalManager *manager)
{
  GcalManagerPrivate *priv;
  GHashTableIter iter;
  gpointer key, value;
  GList *aux = NULL;

  priv = gcal_manager_get_instance_private (manager);

  g_hash_table_iter_init (&iter, priv->clients);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      GcalManagerUnit *unit = value;
      if (!unit->enabled)
        continue;
      aux = g_list_append (aux, key);
    }
  return aux;
}

/**
 * gcal_manager_get_sources_connected:
 * @manager:
 *
 * Returns a {@link GList} with every source connected on the app, whether they are enabled or not.
 *
 * Returns: (Transfer full) a {@link GList} object to be freed with g_list_free()
 **/
GList*
gcal_manager_get_sources_connected (GcalManager *manager)
{
  GcalManagerPrivate *priv;

  priv = gcal_manager_get_instance_private (manager);
  return g_hash_table_get_keys (priv->clients);
}

/**
 * gcal_manager_get_default_source:
 * @manager: App singleton {@link GcalManager} instance
 *
 * Returns: (Transfer full): an {@link ESource} object. Free with g_object_unref().
 **/
ESource*
gcal_manager_get_default_source (GcalManager *manager)
{
  GcalManagerPrivate *priv;

  priv = gcal_manager_get_instance_private (manager);
  return e_source_registry_ref_default_calendar (priv->source_registry);
}

/**
 * gcal_manager_set_default_source:
 * @manager: App singleton {@link GcalManager} instance
 * @source: the new default source.
 *
 * Returns:
 **/
void
gcal_manager_set_default_source (GcalManager *manager,
                                 ESource     *source)
{
  GcalManagerPrivate *priv;

  priv = gcal_manager_get_instance_private (manager);
  e_source_registry_set_default_calendar (priv->source_registry, source);
}

icaltimezone*
gcal_manager_get_system_timezone (GcalManager *manager)
{
  GcalManagerPrivate *priv;

  priv = gcal_manager_get_instance_private (manager);
  return priv->system_timezone;
}

void
gcal_manager_setup_shell_search (GcalManager             *manager,
                                 ECalDataModelSubscriber *subscriber)
{
  GcalManagerPrivate *priv;
  priv = gcal_manager_get_instance_private (manager);

  if (priv->shell_search_data_model == NULL)
    {
      priv->shell_search_data_model = e_cal_data_model_new (submit_thread_job);
      g_signal_connect_swapped (priv->shell_search_data_model, "view-state-changed", G_CALLBACK (model_state_changed), manager);

      e_cal_data_model_set_expand_recurrences (priv->shell_search_data_model, TRUE);
      e_cal_data_model_set_timezone (priv->shell_search_data_model, priv->system_timezone);

      priv->search_view_data = g_new0 (ViewStateData, 1);
      priv->search_view_data->subscriber = subscriber;
    }
}

/**
 * gcal_manager_set_shell_search_query:
 * @manager: A #GcalManager instance
 * @query: query string (an s-exp)
 *
 * Set the query terms of the #ECalDataModel used in the shell search
 **/
void
gcal_manager_set_shell_search_query (GcalManager *manager,
                                     const gchar *query)
{
  GcalManagerPrivate *priv = gcal_manager_get_instance_private (manager);

  priv->search_view_data->passed_start = FALSE;
  priv->search_view_data->search_done = FALSE;
  priv->search_view_data->sources_left = g_hash_table_size (priv->clients) - g_strv_length (priv->disabled_sources);

  if (priv->search_view_data->query != NULL)
    g_free (priv->search_view_data->query);
  priv->search_view_data->query = g_strdup (query);

  e_cal_data_model_set_filter (priv->shell_search_data_model, query);
}

void
gcal_manager_set_shell_search_subscriber (GcalManager             *manager,
                                          ECalDataModelSubscriber *subscriber,
                                          time_t                   range_start,
                                          time_t                   range_end)
{
  GcalManagerPrivate *priv;

  priv = gcal_manager_get_instance_private (manager);
  e_cal_data_model_subscribe (priv->shell_search_data_model, subscriber, range_start, range_end);
}

gboolean
gcal_manager_shell_search_done (GcalManager *manager)
{
  GcalManagerPrivate *priv = gcal_manager_get_instance_private (manager);
  return priv->search_view_data->search_done;
}

GList*
gcal_manager_get_shell_search_events (GcalManager *manager)
{
  GcalManagerPrivate *priv;
  time_t range_start, range_end;
  GList *list = NULL;

  priv = gcal_manager_get_instance_private (manager);

  e_cal_data_model_get_subscriber_range (priv->shell_search_data_model, priv->search_view_data->subscriber,
                                         &range_start, &range_end);
  e_cal_data_model_foreach_component (priv->shell_search_data_model, range_start, range_end, gather_components, &list);
  return list;
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

void
gcal_manager_set_search_subscriber (GcalManager             *manager,
                                    ECalDataModelSubscriber *subscriber,
                                    time_t                   range_start,
                                    time_t                   range_end)
{
  GcalManagerPrivate *priv;

  priv = gcal_manager_get_instance_private (manager);
  e_cal_data_model_subscribe (priv->search_data_model,
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
  e_cal_data_model_set_filter (priv->search_data_model,
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

/**
 * gcal_manager_enable_source:
 * @manager: a #GcalManager
 * @source: the target ESource
 *
 * Enable the given ESource.
 */
void
gcal_manager_enable_source (GcalManager *manager,
                            ESource     *source)
{
  GcalManagerPrivate *priv;
  GcalManagerUnit *unit;
  gchar **new_disabled_sources;
  gint i;

  priv = gcal_manager_get_instance_private (manager);
  unit = g_hash_table_lookup (priv->clients, source);

  if (unit->enabled)
    return;

  unit->enabled = TRUE;
  e_cal_data_model_add_client (priv->e_data_model, unit->client);
  e_cal_data_model_add_client (priv->search_data_model, unit->client);
  if (priv->shell_search_data_model != NULL)
    e_cal_data_model_add_client (priv->shell_search_data_model, unit->client);

  /* remove source's uid from disabled_sources array */
  new_disabled_sources = g_new0 (gchar*, g_strv_length (priv->disabled_sources));
  for (i = 0; i < g_strv_length (priv->disabled_sources); i++)
    {
      if (g_strcmp0 (priv->disabled_sources[i], e_source_get_uid (source)) == 0)
        continue;
      new_disabled_sources[i] = g_strdup (priv->disabled_sources[i]);
    }
  g_strfreev (priv->disabled_sources);
  priv->disabled_sources = new_disabled_sources;

  /* sync settings value */
  g_settings_set_strv (priv->settings, "disabled-sources", (const gchar * const *) priv->disabled_sources);
}

/**
 * gcal_manager_disable_source:
 * @manager: a #GcalManager
 * @source: the target ESource
 *
 * Disable the given ESource.
 */
void
gcal_manager_disable_source (GcalManager *manager,
                             ESource     *source)
{
  GcalManagerPrivate *priv;
  GcalManagerUnit *unit;
  gchar **new_disabled_sources;
  gint i;
  const gchar *source_uid;

  priv = gcal_manager_get_instance_private (manager);
  unit = g_hash_table_lookup (priv->clients, source);

  if (!unit->enabled)
    return;

  source_uid = e_source_get_uid (source);
  unit->enabled = FALSE;
  e_cal_data_model_remove_client (priv->e_data_model, source_uid);
  e_cal_data_model_remove_client (priv->search_data_model, source_uid);
  if (priv->shell_search_data_model != NULL)
    e_cal_data_model_remove_client (priv->shell_search_data_model, source_uid);

  /* add source's uid from disabled_sources array */
  new_disabled_sources = g_new0 (gchar*, g_strv_length (priv->disabled_sources) + 2);
  for (i = 0; i < g_strv_length (priv->disabled_sources); i++)
    new_disabled_sources[i] = g_strdup (priv->disabled_sources[i]);

  new_disabled_sources[g_strv_length (priv->disabled_sources)] = g_strdup (source_uid);

  g_strfreev (priv->disabled_sources);
  priv->disabled_sources = new_disabled_sources;

  /* sync settings value */
  g_settings_set_strv (priv->settings, "disabled-sources", (const gchar * const *) priv->disabled_sources);
}

gboolean
gcal_manager_source_enabled (GcalManager *manager,
                             ESource     *source)
{
  GcalManagerPrivate *priv;
  GcalManagerUnit *unit;

  priv = gcal_manager_get_instance_private (manager);
  unit = g_hash_table_lookup (priv->clients, source);
  if (unit == NULL)
    return FALSE;
  return unit->enabled;
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
      GcalManagerUnit *unit = l->data;

      if (!unit->connected || ! e_client_check_refresh_supported (E_CLIENT (unit->client)))
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

  if (unit == NULL)
    return FALSE;

  return unit->connected && e_client_is_readonly (E_CLIENT (unit->client));
}

void
gcal_manager_create_event (GcalManager        *manager,
                           ESource            *source,
                           ECalComponent      *component)
{
  GcalManagerPrivate *priv;

  GcalManagerUnit *unit;
  icalcomponent *new_event_icalcomp;
  AsyncOpsData *data;

  priv = gcal_manager_get_instance_private (manager);

  unit = g_hash_table_lookup (priv->clients, source);

  new_event_icalcomp = e_cal_component_get_icalcomponent (component);

  data = g_new0 (AsyncOpsData, 1);
  data->source = source;
  data->component = component;
  data->manager = manager;

  e_cal_client_create_object (unit->client,
                              new_event_icalcomp,
                              priv->async_ops,
                              on_event_created,
                              data);
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
gcal_manager_remove_event (GcalManager   *manager,
                           ESource       *source,
                           ECalComponent *component)
{
  GcalManagerPrivate *priv;
  GcalManagerUnit *unit;
  ECalComponentId *id;

  priv = gcal_manager_get_instance_private (manager);

  unit = g_hash_table_lookup (priv->clients, source);
  id = e_cal_component_get_id (component);

  e_cal_client_remove_object (unit->client,
                              id->uid,
                              id->rid,
                              E_CAL_OBJ_MOD_THIS,
                              priv->async_ops,
                              on_event_removed,
                              component);

  e_cal_component_free_id (id);
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

/**
 * gcal_manager_get_events:
 *
 * Returns a list with {@link GcalEventData} objects owned by the caller, the list and the objects.
 * The components inside the list are owned by the caller as well. So, don't ref the component.
 *
 * Returns: An {@link GList} object
 */
GList*
gcal_manager_get_events (GcalManager  *manager,
                         icaltimetype *start_date,
                         icaltimetype *end_date)
{
  GcalManagerPrivate *priv = gcal_manager_get_instance_private (manager);
  time_t range_start, range_end;
  GList *list = NULL;

  range_start = icaltime_as_timet_with_zone (*start_date, priv->system_timezone);
  range_end = icaltime_as_timet_with_zone (*end_date, priv->system_timezone);

  e_cal_data_model_foreach_component (priv->e_data_model, range_start, range_end, gather_components, &list);
  return list;
}

gboolean
gcal_manager_load_completed (GcalManager *manager)
{
  GcalManagerPrivate *priv = gcal_manager_get_instance_private (manager);
  return priv->sources_at_launch == 0;
}

GcalEventData*
gcal_manager_get_event_from_shell_search (GcalManager *manager,
                                          const gchar *uuid)
{
  GcalManagerPrivate *priv;
  time_t range_start, range_end;
  GList *l, *list = NULL;
  GcalEventData *data, *new_data = NULL;
  gchar *cuuid;

  priv = gcal_manager_get_instance_private (manager);

  e_cal_data_model_get_subscriber_range (priv->shell_search_data_model, priv->search_view_data->subscriber,
                                         &range_start, &range_end);
  e_cal_data_model_foreach_component (priv->shell_search_data_model, range_start, range_end, gather_components, &list);
  if (list != NULL)
    new_data = g_new0 (GcalEventData, 1);

  for (l = list; l != NULL; l = g_list_next (l))
    {
      data = l->data;
      cuuid = get_uuid_from_component (data->source, data->event_component);

      if (g_strcmp0 (cuuid, uuid) == 0)
        {
          new_data->source = data->source;
          new_data->event_component = g_object_ref (data->event_component);
        }

      g_object_unref (data->event_component);
      g_free (data);
      g_free (cuuid);
    }
  g_list_free (list);

  return new_data;
}
