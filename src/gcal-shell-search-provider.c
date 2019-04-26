/* gcal-shell-search-provider.c
 *
 * Copyright (C) 2015 Erick Pérez Castellanos <erick.red@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define G_LOG_DOMAIN "GcalShellSearchProvider"

#include "gcal-shell-search-provider.h"
#include "gcal-shell-search-provider-generated.h"

#include "gcal-application.h"
#include "gcal-debug.h"
#include "gcal-event.h"
#include "gcal-window.h"
#include "gcal-utils.h"

typedef struct
{
  GDBusMethodInvocation    *invocation;
  gchar                   **terms;
} PendingSearch;

struct _GcalShellSearchProvider
{
  GObject             parent;

  GcalShellSearchProvider2 *skel;
  GcalManager        *manager;

  PendingSearch      *pending_search;
  guint               scheduled_search_id;
  GHashTable         *events;
};

static void   gcal_subscriber_interface_init (ECalDataModelSubscriberInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GcalShellSearchProvider, gcal_shell_search_provider, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (E_TYPE_CAL_DATA_MODEL_SUBSCRIBER, gcal_subscriber_interface_init));

enum
{
  PROP_0,
  PROP_MANAGER,
  N_PROPS
};

static GParamSpec* properties[N_PROPS] = { NULL, };


/*
 * Auxiliary methods
 */

static gint
sort_event_data (GcalEvent *a,
                 GcalEvent *b,
                 gpointer   user_data)
{
  return gcal_event_compare_with_current (a, b, user_data);
}

static gboolean
execute_search (GcalShellSearchProvider *self)
{
  g_autoptr (GDateTime) start = NULL;
  g_autoptr (GDateTime) end = NULL;
  g_autoptr (GDateTime) now = NULL;
  g_autofree gchar *search_query = NULL;
  time_t range_start, range_end;
  guint i;

  GCAL_ENTRY;

  if (gcal_manager_get_loading (self->manager))
    GCAL_RETURN (TRUE);

  now = g_date_time_new_now_local ();
  start = g_date_time_add_weeks (now, -1);
  range_start = g_date_time_to_unix (start);

  end = g_date_time_add_weeks (now, 3);
  range_end = g_date_time_to_unix (end);

  gcal_manager_set_shell_search_subscriber (self->manager, E_CAL_DATA_MODEL_SUBSCRIBER (self),
                                            range_start, range_end);

  search_query = g_strdup_printf ("(or (contains? \"summary\" \"%s\") (contains? \"description\" \"%s\"))",
                                  self->pending_search->terms[0], self->pending_search->terms[0]);
  for (i = 1; i < g_strv_length (self->pending_search->terms); i++)
    {
      g_autofree gchar *complete_query = NULL;
      g_autofree gchar *second_query = NULL;

      second_query = g_strdup_printf ("(or (contains? \"summary\" \"%s\") (contains? \"description\" \"%s\"))",
                                      self->pending_search->terms[0], self->pending_search->terms[0]);
      complete_query = g_strdup_printf ("(and %s %s)", search_query, second_query);

      g_clear_pointer (&search_query, g_free);
      search_query = g_steal_pointer (&complete_query);
    }

  gcal_manager_set_shell_search_query (self->manager,  search_query);

  self->scheduled_search_id = 0;
  g_application_hold (g_application_get_default ());

  GCAL_RETURN (FALSE);
}

static void
schedule_search (GcalShellSearchProvider *self,
                 GDBusMethodInvocation   *invocation,
                 gchar                  **terms)
{
  GCAL_ENTRY;

  /* don't attempt searches for a single character */
  if (g_strv_length (terms) == 1 && g_utf8_strlen (terms[0], -1) == 1)
    {
      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(as)", NULL));
      GCAL_RETURN ();
    }

  if (self->pending_search != NULL)
    {
      g_object_unref (self->pending_search->invocation);
      g_strfreev (self->pending_search->terms);

      if (self->scheduled_search_id == 0)
        g_application_release (g_application_get_default ());
    }
  else
    {
      self->pending_search = g_new0 (PendingSearch, 1);
    }

  if (self->scheduled_search_id != 0)
    {
      g_source_remove (self->scheduled_search_id);
      self->scheduled_search_id = 0;
    }

  self->pending_search->invocation = g_object_ref (invocation);
  self->pending_search->terms = g_strdupv (terms);

  if (gcal_manager_get_loading (self->manager))
    {
      self->scheduled_search_id = g_timeout_add_seconds (1, (GSourceFunc) execute_search, self);
      GCAL_RETURN ();
    }

  execute_search (self);

  GCAL_EXIT;
}


/*
 * Callbacks
 */

static gboolean
get_initial_result_set_cb (GcalShellSearchProvider  *self,
                           GDBusMethodInvocation    *invocation,
                           gchar                   **terms,
                           GcalShellSearchProvider2 *skel)
{
  schedule_search (self, invocation, terms);
  return TRUE;
}

static gboolean
get_subsearch_result_set_cb (GcalShellSearchProvider  *self,
                             GDBusMethodInvocation    *invocation,
                             gchar                   **previous_results,
                             gchar                   **terms,
                             GcalShellSearchProvider2 *skel)
{
  schedule_search (self, invocation, terms);
  return TRUE;
}

static gboolean
get_result_metas_cb (GcalShellSearchProvider  *self,
                     GDBusMethodInvocation    *invocation,
                     gchar                   **results,
                     GcalShellSearchProvider2 *skel)
{
  GDateTime *local_datetime;
  GVariantBuilder abuilder, builder;
  GcalEvent *event;
  gchar *uuid, *desc;
  gchar *start_date;
  gint i;

  GCAL_ENTRY;

  g_variant_builder_init (&abuilder, G_VARIANT_TYPE ("aa{sv}"));
  for (i = 0; i < g_strv_length (results); i++)
    {
      g_autoptr (GVariant) icon_variant = NULL;
      g_autoptr (GdkPixbuf) gicon = NULL;
      cairo_surface_t *surface;

      uuid = results[i];
      event = g_hash_table_lookup (self->events, uuid);

      g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sv}"));
      g_variant_builder_add (&builder, "{sv}", "id", g_variant_new_string (uuid));
      g_variant_builder_add (&builder, "{sv}", "name", g_variant_new_string (gcal_event_get_summary (event)));

      surface = get_circle_surface_from_color (gcal_event_get_color (event), 96);
      gicon = gdk_pixbuf_get_from_surface (surface, 0, 0, 96, 96);
      icon_variant = g_icon_serialize (G_ICON (gicon));
      g_variant_builder_add (&builder, "{sv}", "icon", icon_variant);

      local_datetime = g_date_time_to_local (gcal_event_get_date_start (event));
      start_date = g_date_time_format (local_datetime, gcal_event_get_all_day (event) ? "%x" : "%c");

      if (gcal_event_get_location (event))
        desc = g_strconcat (start_date, ". ", gcal_event_get_location (event), NULL);
      else
        desc = g_strdup (start_date);

      g_variant_builder_add (&builder, "{sv}", "description", g_variant_new_string (desc));
      g_variant_builder_add_value (&abuilder, g_variant_builder_end (&builder));

      g_clear_pointer (&surface, cairo_surface_destroy);
    }
  g_dbus_method_invocation_return_value (invocation, g_variant_new ("(aa{sv})", &abuilder));

  GCAL_RETURN (TRUE);
}

static gboolean
activate_result_cb (GcalShellSearchProvider  *self,
                    GDBusMethodInvocation    *invocation,
                    gchar                    *result,
                    gchar                   **terms,
                    guint32                   timestamp,
                    GcalShellSearchProvider2 *skel)
{
  g_autoptr (GcalEvent) event = NULL;
  GApplication *application;
  GDateTime *dtstart;

  GCAL_ENTRY;

  application = g_application_get_default ();

  event = gcal_manager_get_event_from_shell_search (self->manager, result);
  dtstart = gcal_event_get_date_start (event);

  gcal_application_set_uuid (GCAL_APPLICATION (application), result);
  gcal_application_set_initial_date (GCAL_APPLICATION (application), dtstart);

  g_application_activate (application);

  GCAL_RETURN (TRUE);
}

static gboolean
launch_search_cb (GcalShellSearchProvider  *self,
                  GDBusMethodInvocation    *invocation,
                  gchar                   **terms,
                  guint32                   timestamp,
                  GcalShellSearchProvider2 *skel)
{
  g_autofree gchar *terms_joined = NULL;
  GApplication *application;
  GcalWindow *window;

  GCAL_ENTRY;

  application = g_application_get_default ();
  g_application_activate (application);

  terms_joined = g_strjoinv (" ", terms);
  window = (GcalWindow *) gtk_application_get_active_window (GTK_APPLICATION (application));
  if (window)
    {
      gcal_window_set_search_mode (window, TRUE);
      gcal_window_set_search_query (window, terms_joined);
    }

  GCAL_RETURN (TRUE);
}

static gboolean
query_completed_cb (GcalShellSearchProvider *self,
                    GcalManager             *manager)
{
  GList *events, *l;
  GVariantBuilder builder;
  time_t current_time_t;

  GCAL_ENTRY;

  g_hash_table_remove_all (self->events);

  events = gcal_manager_get_shell_search_events (self->manager);
  if (events == NULL)
    {
      g_dbus_method_invocation_return_value (self->pending_search->invocation, g_variant_new ("(as)", NULL));
      GCAL_GOTO (out);
    }

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("as"));

  current_time_t = time (NULL);
  events = g_list_sort_with_data (events, (GCompareDataFunc) sort_event_data, &current_time_t);
  for (l = events; l != NULL; l = g_list_next (l))
    {
      const gchar *uid;

      uid = gcal_event_get_uid (l->data);

      if (g_hash_table_contains (self->events, uid))
        continue;

      g_variant_builder_add (&builder, "s", uid);

      g_hash_table_insert (self->events, g_strdup (uid), l->data);
    }
  g_list_free (events);

  g_dbus_method_invocation_return_value (self->pending_search->invocation, g_variant_new ("(as)", &builder));

out:
  g_clear_object (&self->pending_search->invocation);
  g_clear_pointer (&self->pending_search->terms, g_strfreev);
  g_clear_pointer (&self->pending_search, g_free);
  g_application_release (g_application_get_default ());

  GCAL_RETURN (FALSE);
}


/*
 * ECalDataModelSubscriber iface
 */

static void
gcal_shell_search_provider_component_changed (ECalDataModelSubscriber *subscriber,
                                              ECalClient              *client,
                                              ECalComponent           *comp)
{
  ;
}

static void
gcal_shell_search_provider_component_removed (ECalDataModelSubscriber *subscriber,
                                              ECalClient              *client,
                                              const gchar             *uid,
                                              const gchar             *rid)
{
  ;
}

static void
gcal_shell_search_provider_freeze (ECalDataModelSubscriber *subscriber)
{
  ;
}

static void
gcal_shell_search_provider_thaw (ECalDataModelSubscriber *subscriber)
{
  ;
}

static void
gcal_subscriber_interface_init (ECalDataModelSubscriberInterface *iface)
{
  iface->component_added = gcal_shell_search_provider_component_changed;
  iface->component_modified = gcal_shell_search_provider_component_changed;
  iface->component_removed = gcal_shell_search_provider_component_removed;
  iface->freeze = gcal_shell_search_provider_freeze;
  iface->thaw = gcal_shell_search_provider_thaw;
}


/*
 * GObject overrides
 */

static void
gcal_shell_search_provider_finalize (GObject *object)
{
  GcalShellSearchProvider *self = (GcalShellSearchProvider *) object;

  g_clear_pointer (&self->events, g_hash_table_destroy);
  g_clear_object (&self->manager);
  g_clear_object (&self->skel);

  G_OBJECT_CLASS (gcal_shell_search_provider_parent_class)->finalize (object);
}

static void
gcal_shell_search_provider_get_property (GObject    *object,
                                         guint       property_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  GcalShellSearchProvider *self = GCAL_SHELL_SEARCH_PROVIDER (object);

  switch (property_id)
    {
    case PROP_MANAGER:
      g_value_set_object (value, self->manager);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
gcal_shell_search_provider_set_property (GObject      *object,
                                         guint         property_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  GcalShellSearchProvider *self = GCAL_SHELL_SEARCH_PROVIDER (object);

  switch (property_id)
    {
    case PROP_MANAGER:
      if (g_set_object (&self->manager, g_value_get_object (value)))
        {
          gcal_manager_setup_shell_search (self->manager, E_CAL_DATA_MODEL_SUBSCRIBER (self));
          g_signal_connect_swapped (self->manager, "query-completed", G_CALLBACK (query_completed_cb), self);

          g_object_notify_by_pspec (object, properties[PROP_MANAGER]);
        }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
gcal_shell_search_provider_class_init (GcalShellSearchProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gcal_shell_search_provider_finalize;
  object_class->get_property = gcal_shell_search_provider_get_property;
  object_class->set_property = gcal_shell_search_provider_set_property;

  properties[PROP_MANAGER] = g_param_spec_object ("manager",
                                                  "The manager object",
                                                  "The manager object",
                                                  GCAL_TYPE_MANAGER,
                                                  G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gcal_shell_search_provider_init (GcalShellSearchProvider *self)
{
  self->events = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
  self->skel = gcal_shell_search_provider2_skeleton_new ();

  g_signal_connect_swapped (self->skel, "handle-get-initial-result-set", G_CALLBACK (get_initial_result_set_cb), self);
  g_signal_connect_swapped (self->skel, "handle-get-subsearch-result-set", G_CALLBACK (get_subsearch_result_set_cb), self);
  g_signal_connect_swapped (self->skel, "handle-get-result-metas", G_CALLBACK (get_result_metas_cb), self);
  g_signal_connect_swapped (self->skel, "handle-activate-result", G_CALLBACK (activate_result_cb), self);
  g_signal_connect_swapped (self->skel, "handle-launch-search", G_CALLBACK (launch_search_cb), self);
}

GcalShellSearchProvider*
gcal_shell_search_provider_new (GcalManager *manager)
{
  return g_object_new (GCAL_TYPE_SHELL_SEARCH_PROVIDER,
                       "manager", manager,
                       NULL);
}

gboolean
gcal_shell_search_provider_dbus_export (GcalShellSearchProvider  *self,
                                        GDBusConnection          *connection,
                                        const gchar              *object_path,
                                        GError                  **error)
{
  return g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (self->skel), connection, object_path, error);
}

void
gcal_shell_search_provider_dbus_unexport (GcalShellSearchProvider *self,
                                          GDBusConnection         *connection,
                                          const gchar             *object_path)
{
  if (g_dbus_interface_skeleton_has_connection (G_DBUS_INTERFACE_SKELETON (self->skel), connection))
    g_dbus_interface_skeleton_unexport_from_connection (G_DBUS_INTERFACE_SKELETON (self->skel), connection);
}
