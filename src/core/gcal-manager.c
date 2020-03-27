/* gcal-manager.c
 *
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

#define G_LOG_DOMAIN "GcalManager"

#include "gcal-application.h"
#include "gcal-context.h"
#include "gcal-debug.h"
#include "gcal-manager.h"
#include "gcal-timeline.h"
#include "gcal-timeline-subscriber.h"
#include "gcal-utils.h"

#include <libedataserverui/libedataserverui.h>

/**
 * SECTION:gcal-manager
 * @short_description: The backend of GNOME Calendar
 * @title:GcalManager
 * @stability:unstable
 *
 * #GcalManager is the backend of GNOME Calendar. It sets everything
 * up, connects to the Online Accounts daemon, and manages the events
 * and calendars.
 */

typedef struct
{
  GcalEvent          *event;
  GcalManager        *manager;
} AsyncOpsData;

typedef struct
{
  ECalDataModelSubscriber *subscriber;
  gchar              *query;

  guint               sources_left;
  gboolean            passed_start;
  gboolean            search_done;
} ViewStateData;

typedef struct
{
  gchar              *event_uid;
  GcalCalendar       *calendar;
  GcalCalendar       *new_calendar;
  ECalComponent      *new_component;
  GcalManager        *manager;
} MoveEventData;

typedef struct
{
  GcalManager        *manager;
  GList              *events;
} GatherEventData;

struct _GcalManager
{
  GObject             parent;

  /**
   * The list of clients we are managing.
   * Each value is of type GCalStoreUnit
   * And each key is the source uid
   */
  GHashTable         *clients;

  ESourceRegistry    *source_registry;
  ECredentialsPrompter *credentials_prompter;

  ECalDataModel      *e_data_model;

  ECalDataModel      *shell_search_data_model;
  ViewStateData      *search_view_data;

  GCancellable       *async_ops;

  gint                clients_synchronizing;

  GcalTimeline       *timeline;

  GcalContext        *context;
};

G_DEFINE_TYPE (GcalManager, gcal_manager, G_TYPE_OBJECT)

enum
{
  PROP_0,
  PROP_CONTEXT,
  PROP_DEFAULT_CALENDAR,
  PROP_SYNCHRONIZING,
  NUM_PROPS
};

enum
{
  CALENDAR_ADDED,
  CALENDAR_CHANGED,
  CALENDAR_REMOVED,
  QUERY_COMPLETED,
  NUM_SIGNALS
};

static guint       signals[NUM_SIGNALS] = { 0, };
static GParamSpec *properties[NUM_PROPS] = { NULL, };

static void
free_async_ops_data (AsyncOpsData *data)
{
  g_object_unref (data->event);
  g_free (data);
}

static gboolean
gather_events (ECalDataModel         *data_model,
               ECalClient            *client,
               const ECalComponentId *id,
               ECalComponent         *comp,
               time_t                 instance_start,
               time_t                 instance_end,
               gpointer               user_data)
{
  GatherEventData *data = user_data;
  GcalCalendar *calendar;
  GcalEvent *event;
  GError *error;

  error = NULL;
  calendar = g_hash_table_lookup (data->manager->clients, e_client_get_source (E_CLIENT (client)));
  event = gcal_event_new (calendar, comp, &error);

  if (error)
    {
      g_warning ("Error: %s", error->message);
      g_clear_error (&error);
      return TRUE;
    }

  data->events = g_list_append (data->events, event);/* FIXME: add me sorted */

  return TRUE;
}

static void
remove_source (GcalManager  *self,
               ESource      *source)
{
  g_autoptr (GcalCalendar) calendar = NULL;

  GCAL_ENTRY;

  g_return_if_fail (GCAL_IS_MANAGER (self));
  g_return_if_fail (E_IS_SOURCE (source));

  calendar = g_hash_table_lookup (self->clients, source);

  if (!calendar)
    return;

  GCAL_TRACE_MSG ("Removing calendar %s (%s)",
                  e_source_get_display_name (source),
                  e_source_get_uid (source));

  g_object_ref (calendar);

  e_cal_data_model_remove_client (self->e_data_model,
                                  e_source_get_uid (source));

  gcal_timeline_remove_calendar (self->timeline, calendar);
  g_hash_table_remove (self->clients, source);
  g_signal_emit (self, signals[CALENDAR_REMOVED], 0, calendar);

  GCAL_EXIT;
}

static void
source_changed (GcalManager *self,
                ESource     *source)
{
  GcalCalendar *calendar;

  GCAL_ENTRY;

  calendar = g_hash_table_lookup (self->clients, source);

  if (!calendar)
    GCAL_RETURN ();

  if (gcal_calendar_get_visible (calendar))
    {
      ECalClient *client = gcal_calendar_get_client (calendar);

      e_cal_data_model_add_client (self->e_data_model, client);
      if (self->shell_search_data_model)
        e_cal_data_model_add_client (self->shell_search_data_model, client);
    }
  else
    {
      const gchar *id = gcal_calendar_get_id (calendar);

      e_cal_data_model_remove_client (self->e_data_model, id);
      if (self->shell_search_data_model)
        e_cal_data_model_remove_client (self->shell_search_data_model, id);
    }

  g_signal_emit (self, signals[CALENDAR_CHANGED], 0, calendar);

  GCAL_EXIT;
}

static void
on_client_refreshed (GObject      *source_object,
                     GAsyncResult *result,
                     gpointer      user_data)
{
  GcalManager *self = GCAL_MANAGER (user_data);
  GError *error = NULL;

  GCAL_ENTRY;

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

  self->clients_synchronizing--;
  if (self->clients_synchronizing == 0)
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SYNCHRONIZING]);

  GCAL_EXIT;
}

static void
on_calendar_created_cb (GObject      *source_object,
                        GAsyncResult *result,
                        gpointer      user_data)
{
  g_autoptr (GError) error = NULL;
  ESourceRefresh *refresh_extension;
  ESourceOffline *offline_extension;
  GcalCalendar *calendar;
  GcalManager *self;
  ECalClient *client;
  ESource *source;
  gboolean visible;

  self = GCAL_MANAGER (user_data);
  calendar = gcal_calendar_new_finish (result, &error);

  if (error)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED) &&
          !g_error_matches (error, GCAL_CALENDAR_ERROR, GCAL_CALENDAR_ERROR_NOT_CALENDAR))
        {
          g_warning ("Failed to open/connect to calendar: %s", error->message);
        }
      return;
    }

  g_debug ("Source %s (%s) connected",
           gcal_calendar_get_name (calendar),
           gcal_calendar_get_id (calendar));

  visible = gcal_calendar_get_visible (calendar);
  client = gcal_calendar_get_client (calendar);
  source = gcal_calendar_get_source (calendar);

  g_hash_table_insert (self->clients, g_object_ref (source), calendar);

  gcal_timeline_add_calendar (self->timeline, calendar);

  if (visible)
    {
      e_cal_data_model_add_client (self->e_data_model, client);
      if (self->shell_search_data_model != NULL)
        e_cal_data_model_add_client (self->shell_search_data_model, client);
    }

  /* refresh client when it's added */
  if (visible && e_client_check_refresh_supported (E_CLIENT (client)))
    {
      e_client_refresh (E_CLIENT (client), NULL, on_client_refreshed, user_data);

      self->clients_synchronizing++;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SYNCHRONIZING]);
    }

  /* Cache all the online calendars, so the user can see them offline */
  offline_extension = e_source_get_extension (source, E_SOURCE_EXTENSION_OFFLINE);
  e_source_offline_set_stay_synchronized (offline_extension, TRUE);

  /* And also make sure the source is periodically updated */
  refresh_extension = e_source_get_extension (source, E_SOURCE_EXTENSION_REFRESH);
  e_source_refresh_set_enabled (refresh_extension, TRUE);
  e_source_refresh_set_interval_minutes (refresh_extension, 30);

  e_source_registry_commit_source (self->source_registry,
                                   source,
                                   NULL,
                                   NULL,
                                   NULL);

  g_signal_emit (self, signals[CALENDAR_ADDED], 0, calendar);
}

static void
load_source (GcalManager *self,
             ESource     *source)
{
  GCAL_ENTRY;

  if (g_hash_table_contains (self->clients, source))
    {
      g_warning ("%s: Skipping already loaded source: %s",
                 G_STRFUNC,
                 e_source_get_uid (source));
      return;
    }

  gcal_calendar_new (source, self->async_ops, on_calendar_created_cb, self);

  GCAL_EXIT;
}

static gboolean
transform_e_source_to_gcal_calendar_cb (GBinding     *binding,
                                        const GValue *from_value,
                                        GValue       *to_value,
                                        gpointer      user_data)
{
  GcalCalendar *calendar;
  GcalManager *self;
  ESource *source;

  self = GCAL_MANAGER (user_data);
  source = g_value_get_object (from_value);

  calendar = g_hash_table_lookup (self->clients, source);
  g_value_set_object (to_value, calendar);

  return TRUE;
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

  GCAL_ENTRY;

  data = (AsyncOpsData*) user_data;
  client = E_CAL_CLIENT (source_object);
  new_uid = NULL;
  error = NULL;

  if (!e_cal_client_create_object_finish (client, result, &new_uid, &error))
    {
      /* Some error */
      g_warning ("Error creating object: %s", error->message);
      g_error_free (error);
    }
  else
    {
      g_object_ref (data->event);
      gcal_manager_set_default_calendar (data->manager, gcal_event_get_calendar (data->event));
      g_debug ("Event: %s created successfully", new_uid);
    }

  g_free (new_uid);
  free_async_ops_data (data);

  GCAL_EXIT;
}

/**
 * on_component_updated:
 * @source_object: #ECalClient source
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
  GError *error = NULL;

  GCAL_ENTRY;

  if (! e_cal_client_modify_object_finish (E_CAL_CLIENT (source_object),
                                           result,
                                           &error))
    {
      g_warning ("Error updating component: %s", error->message);
      g_error_free (error);
    }
  g_object_unref (E_CAL_COMPONENT (user_data));

  GCAL_EXIT;
}

/**
 * on_event_removed:
 * @source_object: #ECalClient source
 * @result: result of the operation
 * @user_data: an #ECalComponent
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
  GcalEvent *event;
  GError *error;

  GCAL_ENTRY;

  client = E_CAL_CLIENT (source_object);
  event = user_data;
  error = NULL;

  e_cal_client_remove_object_finish (client, result, &error);

  if (error)
    {
      /* FIXME: Notify the user somehow */
      g_warning ("Error removing event: %s", error->message);
      g_error_free (error);
      GCAL_RETURN ();
    }

  g_object_unref (event);

  GCAL_EXIT;
}

static void
model_state_changed (GcalManager            *self,
                     ECalClientView         *view,
                     ECalDataModelViewState  state,
                     guint                   percent,
                     const gchar            *message,
                     const GError           *error,
                     ECalDataModel          *data_model)
{
  gchar *filter;

  GCAL_ENTRY;

  filter = e_cal_data_model_dup_filter (data_model);

  if (state == E_CAL_DATA_MODEL_VIEW_STATE_START &&
      g_strcmp0 (self->search_view_data->query, filter) == 0)
    {
      self->search_view_data->passed_start = TRUE;
      GCAL_GOTO (out);
    }

  if (self->search_view_data->passed_start &&
      state == E_CAL_DATA_MODEL_VIEW_STATE_COMPLETE &&
      g_strcmp0 (self->search_view_data->query, filter) == 0)
    {
      self->search_view_data->sources_left--;
      self->search_view_data->search_done = (self->search_view_data->sources_left == 0);

      if (self->search_view_data->search_done)
        g_signal_emit (self, signals[QUERY_COMPLETED], 0);
    }

out:
  g_free (filter);

  GCAL_EXIT;
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

  GCAL_ENTRY;

  if (!e_source_invoke_authenticate_finish (source, result, &error) &&
      !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      show_source_error (G_STRFUNC, "Failed to invoke authenticate", source, error);
    }

  g_clear_error (&error);

  GCAL_EXIT;
}

static void
source_trust_prompt_done_cb (GObject      *source_object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  ETrustPromptResponse response = E_TRUST_PROMPT_RESPONSE_UNKNOWN;
  ESource *source = E_SOURCE (source_object);
  GError *error = NULL;

  GCAL_ENTRY;

  if (!e_trust_prompt_run_for_source_finish (source, result, &response, &error))
    {
      show_source_error (G_STRFUNC, "Failed to prompt for trust for", source, error);
    }
  else if (response == E_TRUST_PROMPT_RESPONSE_ACCEPT ||
           response == E_TRUST_PROMPT_RESPONSE_ACCEPT_TEMPORARILY)
    {
      /* Use NULL credentials to reuse those from the last time. */
      e_source_invoke_authenticate (source, NULL, NULL /* cancellable */, source_invoke_authenticate_cb, NULL);
    }

  g_clear_error (&error);

  GCAL_EXIT;
}

static void
source_credentials_required_cb (ESourceRegistry         *registry,
                                ESource                 *source,
                                ESourceCredentialsReason reason,
                                const gchar             *certificate_pem,
                                GTlsCertificateFlags     certificate_errors,
                                const GError            *op_error,
                                GcalManager             *self)
{
  ECredentialsPrompter *credentials_prompter;

  GCAL_ENTRY;

  g_return_if_fail (GCAL_IS_MANAGER (self));

  credentials_prompter = self->credentials_prompter;

  if (e_credentials_prompter_get_auto_prompt_disabled_for (credentials_prompter, source))
    return;

  if (reason == E_SOURCE_CREDENTIALS_REASON_SSL_FAILED)
    {
      e_trust_prompt_run_for_source (e_credentials_prompter_get_dialog_parent (credentials_prompter),
                                     source,
                                     certificate_pem,
                                     certificate_errors,
                                     op_error ? op_error->message : NULL,
                                     TRUE /* allow_source_save */,
                                     NULL /* cancellable */,
                                     source_trust_prompt_done_cb,
                                     NULL);
    }
  else if (reason == E_SOURCE_CREDENTIALS_REASON_ERROR && op_error)
    {
      show_source_error (G_STRFUNC, "Failed to authenticate", source, op_error);
    }

  GCAL_EXIT;
}

static void
source_get_last_credentials_required_arguments_cb (GObject      *source_object,
                                                   GAsyncResult *result,
                                                   gpointer      user_data)
{
  GcalManager *self = user_data;
  ESource *source;
  ESourceCredentialsReason reason = E_SOURCE_CREDENTIALS_REASON_UNKNOWN;
  gchar *certificate_pem = NULL;
  GTlsCertificateFlags certificate_errors = 0;
  GError *op_error = NULL;
  GError *error = NULL;

  GCAL_ENTRY;

  g_return_if_fail (E_IS_SOURCE (source_object));

  source = E_SOURCE (source_object);

  if (!e_source_get_last_credentials_required_arguments_finish (source, result, &reason,
          &certificate_pem, &certificate_errors, &op_error, &error))
    {
      /* Can be cancelled only if the manager is disposing/disposed */
      if (error && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        show_source_error (G_STRFUNC, "Failed to get last credentials required arguments for", source, error);

      g_clear_error (&error);
      return;
    }

  g_return_if_fail (GCAL_IS_MANAGER (self));

  if (reason != E_SOURCE_CREDENTIALS_REASON_UNKNOWN)
    source_credentials_required_cb (NULL, source, reason, certificate_pem, certificate_errors, op_error, self);

  g_free (certificate_pem);
  g_clear_error (&op_error);

  GCAL_EXIT;
}

static void
gcal_manager_finalize (GObject *object)
{
  GcalManager *self =GCAL_MANAGER (object);

  GCAL_ENTRY;

  g_clear_object (&self->timeline);
  g_clear_object (&self->e_data_model);
  g_clear_object (&self->shell_search_data_model);

  if (self->context)
    {
      g_object_remove_weak_pointer (G_OBJECT (self->context), (gpointer *)&self->context);
      self->context = NULL;
    }

  if (self->search_view_data)
    {
      g_clear_pointer (&self->search_view_data->query, g_free);
      g_clear_pointer (&self->search_view_data, g_free);
    }

  g_clear_pointer (&self->clients, g_hash_table_destroy);

  G_OBJECT_CLASS (gcal_manager_parent_class)->finalize (object);

  GCAL_EXIT;
}

static void
gcal_manager_set_property (GObject      *object,
                           guint         property_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  GcalManager *self = GCAL_MANAGER (object);

  GCAL_ENTRY;

  switch (property_id)
    {
    case PROP_CONTEXT:
      g_assert (self->context == NULL);
      self->context = g_value_get_object (value);
      g_object_add_weak_pointer (G_OBJECT (self->context), (gpointer *)&self->context);
      break;

    case PROP_DEFAULT_CALENDAR:
      gcal_manager_set_default_calendar (self, g_value_get_object (value));
      break;

    case PROP_SYNCHRONIZING:
      g_assert_not_reached ();
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }

  GCAL_EXIT;
}

static void
gcal_manager_get_property (GObject    *object,
                           guint       property_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  GcalManager *self = GCAL_MANAGER (object);

  GCAL_ENTRY;

  switch (property_id)
    {
    case PROP_CONTEXT:
      g_value_set_object (value, self->context);
      break;

    case PROP_DEFAULT_CALENDAR:
      g_value_set_object (value, gcal_manager_get_default_calendar (self));
      break;

    case PROP_SYNCHRONIZING:
      g_value_set_boolean (value, self->clients_synchronizing != 0);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }

  GCAL_EXIT;
}

static void
gcal_manager_class_init (GcalManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gcal_manager_finalize;
  object_class->set_property = gcal_manager_set_property;
  object_class->get_property = gcal_manager_get_property;

  /**
   * GcalManager:context:
   *
   * The #GcalContext.
   */
  properties[PROP_CONTEXT] = g_param_spec_object ("context",
                                                  "Data context",
                                                  "Data context",
                                                  GCAL_TYPE_CONTEXT,
                                                  G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GcalManager:default-calendar:
   *
   * The default calendar.
   */
  properties[PROP_DEFAULT_CALENDAR] = g_param_spec_object ("default-calendar",
                                                           "Default calendar",
                                                           "The default calendar",
                                                           GCAL_TYPE_CALENDAR,
                                                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * GcalManager:refreshing:
   *
   * Whether there are any sources refreshing or not.
   */
  properties[PROP_SYNCHRONIZING] = g_param_spec_boolean ("synchronizing",
                                                         "Synchronizing",
                                                         "Whether there are any sources synchronizing or not",
                                                         FALSE,
                                                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, NUM_PROPS, properties);

  /* signals */
  signals[CALENDAR_ADDED] = g_signal_new ("calendar-added",
                                          GCAL_TYPE_MANAGER,
                                          G_SIGNAL_RUN_LAST,
                                          0, NULL, NULL, NULL,
                                          G_TYPE_NONE,
                                          1,
                                          GCAL_TYPE_CALENDAR);

  signals[CALENDAR_CHANGED] = g_signal_new ("calendar-changed",
                                            GCAL_TYPE_MANAGER,
                                            G_SIGNAL_RUN_LAST,
                                            0, NULL, NULL, NULL,
                                            G_TYPE_NONE,
                                            1,
                                            GCAL_TYPE_CALENDAR);

  signals[CALENDAR_REMOVED] = g_signal_new ("calendar-removed",
                                            GCAL_TYPE_MANAGER,
                                            G_SIGNAL_RUN_LAST,
                                            0, NULL, NULL, NULL,
                                            G_TYPE_NONE,
                                            1,
                                            GCAL_TYPE_CALENDAR);

  signals[QUERY_COMPLETED] = g_signal_new ("query-completed",
                                           GCAL_TYPE_MANAGER,
                                           G_SIGNAL_RUN_LAST,
                                           0,
                                           NULL, NULL, NULL,
                                           G_TYPE_NONE,
                                           0);
}

static void
gcal_manager_init (GcalManager *self)
{
}

/* Public API */
/**
 * gcal_manager_new:
 * @context: a #GcalContext
 *
 * Creates a new #GcalManager.
 *
 * Returns: (transfer full): a newly created #GcalManager
 */
GcalManager*
gcal_manager_new (GcalContext *context)
{
  return g_object_new (GCAL_TYPE_MANAGER,
                       "context", context,
                       NULL);
}

/**
 * gcal_manager_get_source:
 * @self: a #GcalManager
 * @uid: the unique identifier of the source
 *
 * Retrieve a source according to it's UID. The source
 * is referenced for thread-safety and must be unreferenced
 * after user.
 *
 * Returns: (nullable)(transfer full): an #ESource, or %NULL.
 */
ESource*
gcal_manager_get_source (GcalManager *self,
                         const gchar *uid)
{
  g_return_val_if_fail (GCAL_IS_MANAGER (self), NULL);

  return e_source_registry_ref_source (self->source_registry, uid);
}

GList*
gcal_manager_get_calendars (GcalManager *self)
{
  g_return_val_if_fail (GCAL_IS_MANAGER (self), NULL);

  return g_hash_table_get_values (self->clients);
}

/**
 * gcal_manager_get_default_calendar:
 * @self: a #GcalManager
 *
 * Returns: (transfer none): a #GcalCalendar.
 */
GcalCalendar*
gcal_manager_get_default_calendar (GcalManager *self)
{
  g_autoptr (ESource) default_source = NULL;

  g_return_val_if_fail (GCAL_IS_MANAGER (self), NULL);

  default_source = e_source_registry_ref_default_calendar (self->source_registry);
  return g_hash_table_lookup (self->clients, default_source);
}

/**
 * gcal_manager_set_default_calendar:
 * @self: a #GcalManager
 * @calendar: the new default calendar.
 *
 * Sets the default calendar.
 */
void
gcal_manager_set_default_calendar (GcalManager  *self,
                                   GcalCalendar *calendar)
{
  g_return_if_fail (GCAL_IS_MANAGER (self));

  if (calendar == gcal_manager_get_default_calendar (self))
    return;

  e_source_registry_set_default_calendar (self->source_registry,
                                          gcal_calendar_get_source (calendar));
}

/**
 * gcal_manager_setup_shell_search:
 * @self: a #GcalManager
 * @subscriber: an #ECalDataModelSubscriber
 *
 * Sets up the GNOME Shell search subscriber.
 */
void
gcal_manager_setup_shell_search (GcalManager             *self,
                                 ECalDataModelSubscriber *subscriber)
{
  GTimeZone *zone;

  g_return_if_fail (GCAL_IS_MANAGER (self));

  if (self->shell_search_data_model)
    return;

  self->shell_search_data_model = e_cal_data_model_new (gcal_thread_submit_job);
  g_signal_connect_object (self->shell_search_data_model,
                           "view-state-changed",
                           G_CALLBACK (model_state_changed),
                           self,
                           G_CONNECT_SWAPPED);

  e_cal_data_model_set_expand_recurrences (self->shell_search_data_model, TRUE);

  zone = gcal_context_get_timezone (self->context);
  e_cal_data_model_set_timezone (self->shell_search_data_model, gcal_timezone_to_icaltimezone (zone));

  self->search_view_data = g_new0 (ViewStateData, 1);
  self->search_view_data->subscriber = subscriber;
}

/**
 * gcal_manager_set_shell_search_query:
 * @self: A #GcalManager instance
 * @query: query string (an s-exp)
 *
 * Set the query terms of the #ECalDataModel used in the shell search
 */
void
gcal_manager_set_shell_search_query (GcalManager *self,
                                     const gchar *query)
{
  GCAL_ENTRY;

  g_return_if_fail (GCAL_IS_MANAGER (self));

  self->search_view_data->passed_start = FALSE;
  self->search_view_data->search_done = FALSE;
  self->search_view_data->sources_left = g_hash_table_size (self->clients);

  if (self->search_view_data->query != NULL)
    g_free (self->search_view_data->query);
  self->search_view_data->query = g_strdup (query);

  e_cal_data_model_set_filter (self->shell_search_data_model, query);
}

/**
 * gcal_manager_set_shell_search_subscriber:
 * @self: a #GcalManager
 * @subscriber: the #ECalDataModelSubscriber to subscribe
 * @range_start: the start of the range
 * @range_end: the end of the range
 *
 * Subscribe @subscriber to the shell data modal at the given range.
 */
void
gcal_manager_set_shell_search_subscriber (GcalManager             *self,
                                          ECalDataModelSubscriber *subscriber,
                                          time_t                   range_start,
                                          time_t                   range_end)
{
  GCAL_ENTRY;

  e_cal_data_model_subscribe (self->shell_search_data_model, subscriber, range_start, range_end);

  GCAL_EXIT;
}

/**
 * gcal_manager_shell_search_done:
 * @self: a #GcalManager
 *
 * Retrieves whether the search at @self is done or not.
 *
 * Returns: %TRUE if the search is finished.
 */
gboolean
gcal_manager_shell_search_done (GcalManager *self)
{
  g_return_val_if_fail (GCAL_IS_MANAGER (self), FALSE);

  return self->search_view_data->search_done;
}

/**
 * gcal_manager_get_shell_search_events:
 * @self: a #GcalManager
 *
 * Retrieves all the events available for GNOME Shell search.
 *
 * Returns: (nullable)(transfer full)(content-type GcalEvent): a #GList
 */
GList*
gcal_manager_get_shell_search_events (GcalManager *self)
{
  time_t range_start, range_end;
  GatherEventData data = {
    .manager = self,
    .events = NULL,
  };

  GCAL_ENTRY;

  e_cal_data_model_get_subscriber_range (self->shell_search_data_model,
                                         self->search_view_data->subscriber,
                                         &range_start,
                                         &range_end);

  e_cal_data_model_foreach_component (self->shell_search_data_model,
                                      range_start,
                                      range_end,
                                      gather_events,
                                      &data);

  GCAL_RETURN (data.events);
}

/**
 * gcal_manager_set_subscriber:
 * @self: a #GcalManager
 * @subscriber: a #ECalDataModelSubscriber
 * @range_start: the start of the range
 * @range_end: the end of the range
 *
 * Sets the @subscriber to show events between @range_start
 * and @range_end.
 */
void
gcal_manager_set_subscriber (GcalManager             *self,
                             ECalDataModelSubscriber *subscriber,
                             time_t                   range_start,
                             time_t                   range_end)
{
  GCAL_ENTRY;

  g_return_if_fail (GCAL_IS_MANAGER (self));

  if (GCAL_IS_TIMELINE_SUBSCRIBER (subscriber))
    gcal_timeline_add_subscriber (self->timeline, GCAL_TIMELINE_SUBSCRIBER (subscriber));
  else
    e_cal_data_model_subscribe (self->e_data_model, subscriber, range_start, range_end);

  GCAL_EXIT;
}

/**
 * gcal_manager_query_client_data:
 *
 * Queries for a specific data field of the #ECalClient
 *
 * Returns: (nullable)(transfer none): a string representing
 * the retrieved value.
 */
gchar*
gcal_manager_query_client_data (GcalManager *self,
                                ESource     *source,
                                const gchar *field)
{
  GcalCalendar *calendar;
  ECalClient *client;
  gchar *out;

  GCAL_ENTRY;

  g_return_val_if_fail (GCAL_IS_MANAGER (self), NULL);

  calendar = g_hash_table_lookup (self->clients, source);

  if (!calendar)
    GCAL_RETURN (NULL);

  client = gcal_calendar_get_client (calendar);
  g_object_get (client, field, &out, NULL);

  GCAL_RETURN (out);
}

/**
 * gcal_manager_add_source:
 * @self: a #GcalManager
 * @base_uri: URI defining the ESourceGroup the client will belongs
 * @relative_uri: URI, relative to the base URI
 * @color: a string representing a color parseable for gdk_rgba_parse
 *
 * Add a new calendar by its URI.
 * The calendar is enabled by default
 *
 * Returns: (nullable)(transfer full): unique identifier of calendar's source, or
 * %NULL.
 */
gchar*
gcal_manager_add_source (GcalManager *self,
                         const gchar *name,
                         const gchar *backend,
                         const gchar *color)
{
  ESource *source;
  ESourceCalendar *extension;
  GError *error;

  GCAL_ENTRY;

  g_return_val_if_fail (GCAL_IS_MANAGER (self), NULL);

  source = e_source_new (NULL, NULL, NULL);
  extension = E_SOURCE_CALENDAR (e_source_get_extension (source, E_SOURCE_EXTENSION_CALENDAR));

  g_object_set (extension,
                "backend-name", backend,
                "color", color,
                NULL);

  e_source_set_display_name (source, name);

  error = NULL;
  e_source_registry_commit_source_sync (self->source_registry,
                                        source,
                                        NULL,
                                        &error);
  if (error)
    {
      g_warning ("Failed to store calendar configuration: %s", error->message);
      g_object_unref (source);
      g_clear_error (&error);

      GCAL_RETURN (NULL);
    }

  load_source (self, source);

  GCAL_RETURN (e_source_dup_uid (source));
}

/**
 * gcal_manager_save_source:
 * @self: a #GcalManager
 * @source: the target ESource
 *
 * Commit the given ESource.
 */
void
gcal_manager_save_source (GcalManager *self,
                          ESource     *source)
{
  GError *error = NULL;

  GCAL_ENTRY;

  g_return_if_fail (GCAL_IS_MANAGER (self));
  g_return_if_fail (E_IS_SOURCE (source));

  e_source_registry_commit_source_sync (self->source_registry, source, NULL, &error);

  if (error)
    {
      /* FIXME: Notify the user somehow */
      g_warning ("Error saving source: %s", error->message);
      g_error_free (error);
    }

  GCAL_EXIT;
}

/**
 * gcal_manager_refresh:
 * @self: a #GcalManager
 *
 * Forces a full refresh and synchronization of all available
 * calendars.
 */
void
gcal_manager_refresh (GcalManager *self)
{
  GList *clients;
  GList *l;

  GCAL_ENTRY;

  g_return_if_fail (GCAL_IS_MANAGER (self));

  clients = g_hash_table_get_values (self->clients);

  /* refresh clients */
  for (l = clients; l != NULL; l = l->next)
    {
      GcalCalendar *calendar = l->data;
      EClient *client;

      client = E_CLIENT (gcal_calendar_get_client (calendar));

      if (!e_client_check_refresh_supported (client))
        continue;

      e_client_refresh (client,
                        NULL,
                        on_client_refreshed,
                        self);

      self->clients_synchronizing++;
    }

  g_list_free (clients);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SYNCHRONIZING]);

  GCAL_EXIT;
}

/**
 * gcal_manager_create_event:
 * @self: a #GcalManager
 * @event: a #GcalEvent
 *
 * Creates @event.
 */
void
gcal_manager_create_event (GcalManager *self,
                           GcalEvent   *event)
{
  ICalComponent *new_event_icalcomp;
  ECalComponent *component;
  GcalCalendar *calendar;
  AsyncOpsData *data;

  GCAL_ENTRY;

  g_return_if_fail (GCAL_IS_MANAGER (self));
  g_return_if_fail (GCAL_IS_EVENT (event));

  component = gcal_event_get_component (event);
  calendar = gcal_event_get_calendar (event);

  new_event_icalcomp = e_cal_component_get_icalcomponent (component);

  data = g_new0 (AsyncOpsData, 1);
  data->event = g_object_ref (event);
  data->manager = self;

  e_cal_client_create_object (gcal_calendar_get_client (calendar),
                              new_event_icalcomp,
                              E_CAL_OPERATION_FLAG_NONE,
                              self->async_ops,
                              on_event_created,
                              data);

  GCAL_EXIT;
}

/**
 * gcal_manager_update_event:
 * @self: a #GcalManager
 * @event: a #GcalEvent
 * @mod: an #GcalRecurrenceModType
 *
 * Saves all changes made to @event persistently.
 */
void
gcal_manager_update_event (GcalManager           *self,
                           GcalEvent             *event,
                           GcalRecurrenceModType  mod)
{
  ECalComponent *component;
  GcalCalendar *calendar;

  GCAL_ENTRY;

  g_return_if_fail (GCAL_IS_MANAGER (self));
  g_return_if_fail (GCAL_IS_EVENT (event));

  calendar = gcal_event_get_calendar (event);
  component = gcal_event_get_component (event);

  /*
   * HACK: In Evolution Calendar, a NULL 'rid' is usually associated
   * with an E_CAL_OBJ_MOD_ALL modtype. Here, we are manually setting
   * the rid to NULL when modifying a recurrent event with MOD_ALL
   * modtype.
   */
  if (mod == GCAL_RECURRENCE_MOD_ALL)
    e_cal_component_set_recurid (component, NULL);

  /*
   * While we're updating the event, we don't want the component
   * to be destroyed, so take a reference of the component while
   * we're performing the update on it.
   */
  g_object_ref (component);

  e_cal_client_modify_object (gcal_calendar_get_client (calendar),
                              e_cal_component_get_icalcomponent (component),
                              (ECalObjModType) mod,
                              E_CAL_OPERATION_FLAG_NONE,
                              NULL,
                              on_event_updated,
                              component);

  GCAL_EXIT;
}

/**
 * gcal_manager_remove_event:
 * @self: a #GcalManager
 * @event: a #GcalEvent
 * @mod: an #GcalRecurrenceModType
 *
 * Deletes @event.
 */
void
gcal_manager_remove_event (GcalManager           *self,
                           GcalEvent             *event,
                           GcalRecurrenceModType  mod)
{
  ECalComponent *component;
  GcalCalendar *calendar;
  gchar *rid;
  const gchar *uid;

  GCAL_ENTRY;

  g_return_if_fail (GCAL_IS_MANAGER (self));
  g_return_if_fail (GCAL_IS_EVENT (event));

  component = gcal_event_get_component (event);
  calendar = gcal_event_get_calendar (event);
  rid = NULL;

  uid = e_cal_component_get_uid (component);

  if (gcal_event_has_recurrence (event))
    rid = e_cal_component_get_recurid_as_string (component);

  e_cal_client_remove_object (gcal_calendar_get_client (calendar),
                              uid,
                              mod == GCAL_RECURRENCE_MOD_ALL ? NULL : rid,
                              (ECalObjModType) mod,
                              E_CAL_OPERATION_FLAG_NONE,
                              self->async_ops,
                              on_event_removed,
                              g_object_ref (event));

  g_free (rid);

  GCAL_EXIT;
}

/**
 * gcal_manager_move_event_to_source:
 * @self: a #GcalManager
 * @event: a #GcalEvent
 * @dest: the destination calendar
 *
 * Moves @event to @dest calendar. This is a fail-safe operation:
 * worst case, the user will have two duplicated events, and we
 * guarantee to never loose any data.
 */
void
gcal_manager_move_event_to_source (GcalManager *self,
                                   GcalEvent   *event,
                                   ESource     *dest)
{
  ECalComponent *ecomponent;
  ECalComponent *clone;
  ICalComponent *comp;
  GcalCalendar *calendar;
  ECalComponentId *id;
  GError *error;

  GCAL_ENTRY;

  g_return_if_fail (GCAL_IS_MANAGER (self));
  g_return_if_fail (GCAL_IS_EVENT (event));
  g_return_if_fail (E_IS_SOURCE (dest));

  error = NULL;

  /* First, try to create the component on the destination source */
  calendar = g_hash_table_lookup (self->clients, dest);

  ecomponent = gcal_event_get_component (event);
  clone = e_cal_component_clone (ecomponent);
  comp = e_cal_component_get_icalcomponent (clone);

  e_cal_client_create_object_sync (gcal_calendar_get_client (calendar),
                                   comp,
                                   E_CAL_OPERATION_FLAG_NONE,
                                   NULL,
                                   NULL,
                                   &error);

  if (error)
    {
      g_warning ("Error moving source: %s", error->message);
      g_clear_error (&error);
      GCAL_EXIT;
      return;
    }

  /*
   * After we carefully confirm that a clone of the component was
   * created, try to remove the old component. Data loss it the last
   * thing we want to happen here.
   */
  calendar = gcal_event_get_calendar (event);

  id = e_cal_component_get_id (ecomponent);

  e_cal_client_remove_object_sync (gcal_calendar_get_client (calendar),
                                   e_cal_component_id_get_uid (id),
                                   e_cal_component_id_get_rid (id),
                                   E_CAL_OBJ_MOD_THIS,
                                   E_CAL_OPERATION_FLAG_NONE,
                                   self->async_ops,
                                   &error);

  if (error)
    {
      g_warning ("Error moving source: %s", error->message);
      g_clear_error (&error);
      GCAL_EXIT;
      return;
    }

  e_cal_component_id_free (id);

  GCAL_EXIT;
}

/**
 * gcal_manager_get_events:
 * @self: a #GcalManager
 * @start_date: the start of the dete range
 * @end_date: the end of the dete range
 *
 * Returns a list with #GcalEvent objects owned by the caller,
 * the list and the objects. The components inside the list are
 * owned by the caller as well.
 *
 * Returns: (nullable)(transfer full)(content-type GcalEvent):a #GList
 */
GList*
gcal_manager_get_events (GcalManager *self,
                         ICalTime    *start_date,
                         ICalTime    *end_date)
{
  time_t range_start, range_end;
  GTimeZone *zone;
  ICalTimezone *tz;
  GatherEventData data = {
    .manager = self,
    .events = NULL,
  };

  GCAL_ENTRY;

  g_return_val_if_fail (GCAL_IS_MANAGER (self), NULL);

  zone = gcal_context_get_timezone (self->context);
  tz = gcal_timezone_to_icaltimezone (zone);
  range_start = i_cal_time_as_timet_with_zone (start_date, tz);
  range_end = i_cal_time_as_timet_with_zone (end_date, tz);

  e_cal_data_model_foreach_component (self->e_data_model,
                                      range_start,
                                      range_end,
                                      gather_events,
                                      &data);

  GCAL_RETURN (data.events);
}

/**
 * gcal_manager_get_event_from_shell_search:
 * @self: a #GcalManager
 * @uuid: the unique identier of the event
 *
 * Retrieves the #GcalEvent with @uuid.
 *
 * Returns: (nullable)(transfer full): a #GcalEvent
 */
GcalEvent*
gcal_manager_get_event_from_shell_search (GcalManager *self,
                                          const gchar *uuid)
{
  GcalEvent *new_event;
  GList *l;
  time_t range_start, range_end;
  GatherEventData data = {
    .manager = self,
    .events = NULL,
  };


  GCAL_ENTRY;

  g_return_val_if_fail (GCAL_IS_MANAGER (self), NULL);

  new_event = NULL;

  e_cal_data_model_get_subscriber_range (self->shell_search_data_model,
                                         self->search_view_data->subscriber,
                                         &range_start,
                                         &range_end);

  e_cal_data_model_foreach_component (self->shell_search_data_model,
                                      range_start,
                                      range_end,
                                      gather_events,
                                      &data);

  for (l = data.events; l != NULL; l = g_list_next (l))
    {
      GcalEvent *event;

      event = l->data;

      /* Found the event */
      if (g_strcmp0 (gcal_event_get_uid (event), uuid) == 0)
        new_event = event;
      else
        g_object_unref (event);
    }

  g_list_free (data.events);

  GCAL_RETURN (new_event);
}

/**
 * gcal_manager_get_synchronizing:
 * @self: a #GcalManager
 *
 * Retrieves whether @self is refreshing the calendars or not.
 *
 * Returns: %TRUE if any calendars are being synchronizing.
 */
gboolean
gcal_manager_get_synchronizing (GcalManager *self)
{
  g_return_val_if_fail (GCAL_IS_MANAGER (self), FALSE);

  return self->clients_synchronizing != 0;
}

void
gcal_manager_startup (GcalManager *self)
{
  GList *sources, *l;
  GError *error = NULL;
  ESourceCredentialsProvider *credentials_provider;
  GTimeZone *zone;

  GCAL_ENTRY;

  self->timeline = gcal_timeline_new (self->context);
  self->clients = g_hash_table_new_full ((GHashFunc) e_source_hash,
                                         (GEqualFunc) e_source_equal,
                                         g_object_unref,
                                         g_object_unref);

  /* reading sources and schedule its connecting */
  self->source_registry = e_source_registry_new_sync (NULL, &error);

  if (!self->source_registry)
    {
      g_warning ("Failed to access calendar configuration: %s", error->message);
      g_error_free (error);
      return;
    }

  g_object_bind_property_full (self->source_registry,
                               "default-calendar",
                               self,
                               "default-calendar",
                               G_BINDING_DEFAULT,
                               transform_e_source_to_gcal_calendar_cb,
                               NULL,
                               self,
                               NULL);

  self->credentials_prompter = e_credentials_prompter_new (self->source_registry);

  /* First disable credentials prompt for all but calendar sources... */
  sources = e_source_registry_list_sources (self->source_registry, NULL);

  for (l = sources; l != NULL; l = g_list_next (l))
    {
      ESource *source = E_SOURCE (l->data);

      /* Mark for skip also currently disabled sources */
      if (!e_source_has_extension (source, E_SOURCE_EXTENSION_CALENDAR) &&
          !e_source_has_extension (source, E_SOURCE_EXTENSION_COLLECTION))
        {
          e_credentials_prompter_set_auto_prompt_disabled_for (self->credentials_prompter, source, TRUE);
        }
      else
        {
          e_source_get_last_credentials_required_arguments (source,
                                                            NULL,
                                                            source_get_last_credentials_required_arguments_cb,
                                                            self);
        }
    }

  g_list_free_full (sources, g_object_unref);

  credentials_provider = e_credentials_prompter_get_provider (self->credentials_prompter);

  /* ...then enable credentials prompt for credential source of the calendar sources,
     which can be a collection source.  */
  sources = e_source_registry_list_sources (self->source_registry, E_SOURCE_EXTENSION_CALENDAR);

  for (l = sources; l != NULL; l = g_list_next (l))
    {
      ESource *source, *cred_source;

      source = l->data;
      cred_source = e_source_credentials_provider_ref_credentials_source (credentials_provider, source);

      if (cred_source && !e_source_equal (source, cred_source))
	{
          e_credentials_prompter_set_auto_prompt_disabled_for (self->credentials_prompter, cred_source, FALSE);

          /* Only consider SSL errors */
          if (e_source_get_connection_status (cred_source) != E_SOURCE_CONNECTION_STATUS_SSL_FAILED)
            continue;

          e_source_get_last_credentials_required_arguments (cred_source,
                                                            NULL,
                                                            source_get_last_credentials_required_arguments_cb,
                                                            self);
        }

      g_clear_object (&cred_source);
    }

  g_list_free_full (sources, g_object_unref);

  /* The eds_credentials_prompter responses to REQUIRED and REJECTED reasons,
     the SSL_FAILED should be handled elsewhere. */
  g_signal_connect_object (self->source_registry, "credentials-required", G_CALLBACK (source_credentials_required_cb), self, 0);

  e_credentials_prompter_process_awaiting_credentials (self->credentials_prompter);

  g_signal_connect_object (self->source_registry, "source-added", G_CALLBACK (load_source), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->source_registry, "source-removed", G_CALLBACK (remove_source), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->source_registry, "source-changed", G_CALLBACK (source_changed), self, G_CONNECT_SWAPPED);

  /* create data model */
  self->e_data_model = e_cal_data_model_new (gcal_thread_submit_job);

  e_cal_data_model_set_expand_recurrences (self->e_data_model, TRUE);

  zone = gcal_context_get_timezone (self->context);
  e_cal_data_model_set_timezone (self->e_data_model, gcal_timezone_to_icaltimezone (zone));

  sources = e_source_registry_list_enabled (self->source_registry, E_SOURCE_EXTENSION_CALENDAR);

  for (l = sources; l != NULL; l = l->next)
    load_source (self, l->data);

  g_list_free (sources);

  GCAL_EXIT;
}
