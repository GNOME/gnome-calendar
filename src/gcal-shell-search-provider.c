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

#include "gcal-shell-search-provider.h"
#include "gcal-shell-search-provider-generated.h"

#include "gcal-application.h"
#include "gcal-window.h"
#include "gcal-utils.h"

typedef struct
{
  GDBusMethodInvocation    *invocation;
  gchar                   **terms;
  icaltimetype              date;
} PendingSearch;

typedef struct
{
  GcalShellSearchProvider2 *skel;
  GcalManager              *manager;

  PendingSearch            *pending_search;
  guint                     scheduled_search_id;
  GHashTable               *events;
} GcalShellSearchProviderPrivate;

struct _GcalShellSearchProvider
{
  GObject parent;

  /*< private >*/
  GcalShellSearchProviderPrivate *priv;
};

static void   gcal_subscriber_interface_init (ECalDataModelSubscriberInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GcalShellSearchProvider, gcal_shell_search_provider, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GcalShellSearchProvider)
                         G_IMPLEMENT_INTERFACE (E_TYPE_CAL_DATA_MODEL_SUBSCRIBER, gcal_subscriber_interface_init));

static void
free_event_data (gpointer data)
{
  GcalEventData *event_data = data;
  g_object_unref (event_data->event_component);
  g_free (event_data);
}

static gint
sort_event_data (gconstpointer a,
                 gconstpointer b,
                 gpointer user_data)
{
  ECalComponent *comp1, *comp2;
  ECalComponentDateTime date1, date2;
  gint result;


  comp1 = ((GcalEventData*) a)->event_component;
  comp2 = ((GcalEventData*) b)->event_component;

  e_cal_component_get_dtstart (comp1, &date1);
  e_cal_component_get_dtstart (comp2, &date2);

  if (date1.tzid != NULL)
    date1.value->zone = icaltimezone_get_builtin_timezone_from_tzid (date1.tzid);
  if (date2.tzid != NULL)
    date2.value->zone = icaltimezone_get_builtin_timezone_from_tzid (date2.tzid);
  result = icaltime_compare_with_current (date1.value, date2.value, user_data);

  e_cal_component_free_datetime (&date1);
  e_cal_component_free_datetime (&date2);

  return result;
}

static gboolean
execute_search (GcalShellSearchProvider *search_provider)
{
  GcalShellSearchProviderPrivate *priv;

  guint i;
  gchar *search_query;

  icaltimezone *zone;
  time_t range_start, range_end;

  priv = search_provider->priv;

  if (!gcal_manager_load_completed (priv->manager))
    return TRUE;

  zone = gcal_manager_get_system_timezone (priv->manager);
  priv->pending_search->date = icaltime_current_time_with_zone (zone);
  icaltime_adjust (&(priv->pending_search->date), -7, 0, 0, 0); /* -1 weeks from today */
  range_start = icaltime_as_timet_with_zone (priv->pending_search->date, zone);

  icaltime_adjust (&(priv->pending_search->date), 21 * 2, 0, 0, 0); /* +3 weeks from today */
  range_end = icaltime_as_timet_with_zone (priv->pending_search->date, zone);

  gcal_manager_set_shell_search_subscriber (priv->manager, E_CAL_DATA_MODEL_SUBSCRIBER (search_provider),
                                            range_start, range_end);

  search_query = g_strdup_printf ("(or (contains? \"summary\" \"%s\") (contains? \"description\" \"%s\"))",
                                  priv->pending_search->terms[0], priv->pending_search->terms[0]);
  for (i = 1; i < g_strv_length (priv->pending_search->terms); i++)
    {
      gchar *complete_query;
      gchar *second_query = g_strdup_printf ("(or (contains? \"summary\" \"%s\") (contains? \"description\" \"%s\"))",
                                             priv->pending_search->terms[0], priv->pending_search->terms[0]);
      complete_query = g_strdup_printf ("(and %s %s)", search_query, second_query);

      g_free (second_query);
      g_free (search_query);

      search_query = complete_query;
    }

  gcal_manager_set_shell_search_query (priv->manager,  search_query);
  g_free (search_query);

  priv->scheduled_search_id = 0;
  g_application_hold (g_application_get_default ());
  return FALSE;
}

static void
schedule_search (GcalShellSearchProvider *search_provider,
                 GDBusMethodInvocation   *invocation,
                 gchar                  **terms)
{
  GcalShellSearchProviderPrivate *priv = search_provider->priv;

  /* don't attempt searches for a single character */
  if (g_strv_length (terms) == 1 && g_utf8_strlen (terms[0], -1) == 1)
    {
      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(as)", NULL));
      return;
    }

  if (priv->pending_search != NULL)
    {
      g_object_unref (priv->pending_search->invocation);
      g_strfreev (priv->pending_search->terms);

      if (priv->scheduled_search_id == 0)
        g_application_release (g_application_get_default ());
    }
  else
    {
      priv->pending_search = g_new0 (PendingSearch, 1);
    }

  if (priv->scheduled_search_id != 0)
    {
      g_source_remove (priv->scheduled_search_id);
      priv->scheduled_search_id = 0;
    }

  priv->pending_search->invocation = g_object_ref (invocation);
  priv->pending_search->terms = g_strdupv (terms);

  if (!gcal_manager_load_completed (priv->manager))
    {
      priv->scheduled_search_id = g_timeout_add_seconds (1, (GSourceFunc) execute_search, search_provider);
      return;
    }

  execute_search (search_provider);
  return;
}

static gboolean
get_initial_result_set_cb (GcalShellSearchProvider  *search_provider,
                           GDBusMethodInvocation    *invocation,
                           gchar                   **terms,
                           GcalShellSearchProvider2 *skel)
{
  schedule_search (search_provider, invocation, terms);
  return TRUE;
}

static gboolean
get_subsearch_result_set_cb (GcalShellSearchProvider  *search_provider,
                             GDBusMethodInvocation    *invocation,
                             gchar                   **previous_results,
                             gchar                   **terms,
                             GcalShellSearchProvider2 *skel)
{
  schedule_search (search_provider, invocation, terms);
  return TRUE;
}

static gboolean
get_result_metas_cb (GcalShellSearchProvider  *search_provider,
                     GDBusMethodInvocation    *invocation,
                     gchar                   **results,
                     GcalShellSearchProvider2 *skel)
{
  GcalShellSearchProviderPrivate *priv;
  gint i;
  gchar *uuid, *desc;
  const gchar* location;

  g_autoptr(GTimeZone) tz;
  g_autoptr (GDateTime) datetime;
  g_autoptr (GDateTime) local_datetime;
  ECalComponentDateTime dtstart;
  gchar *start_date;

  ECalComponentText summary;
  GdkRGBA color;
  GVariantBuilder abuilder, builder;
  GVariant *icon_variant;
  GcalEventData *data;
  GdkPixbuf *gicon;

  priv = search_provider->priv;

  g_variant_builder_init (&abuilder, G_VARIANT_TYPE ("aa{sv}"));
  for (i = 0; i < g_strv_length (results); i++)
    {
      uuid = results[i];
      data = g_hash_table_lookup (priv->events, uuid);

      g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sv}"));
      g_variant_builder_add (&builder, "{sv}", "id", g_variant_new_string (uuid));

      e_cal_component_get_summary (data->event_component, &summary);
      g_variant_builder_add (&builder, "{sv}", "name", g_variant_new_string (summary.value));

      get_color_name_from_source (data->source, &color);
      gicon = get_circle_pixbuf_from_color (&color, 128);
      icon_variant = g_icon_serialize (G_ICON (gicon));
      g_variant_builder_add (&builder, "{sv}", "icon", icon_variant);
      g_object_unref (gicon);
      g_variant_unref (icon_variant);

      e_cal_component_get_dtstart (data->event_component, &dtstart);

      if (dtstart.tzid != NULL)
        tz = g_time_zone_new (dtstart.tzid);
      else if (dtstart.value->zone != NULL)
        tz = g_time_zone_new (icaltimezone_get_tzid ((icaltimezone*) dtstart.value->zone));
      else
        tz = g_time_zone_new_local ();

      datetime = g_date_time_new (tz,
                                  dtstart.value->year, dtstart.value->month, dtstart.value->day,
                                  dtstart.value->hour, dtstart.value->minute, dtstart.value->second);
      local_datetime = g_date_time_to_local (datetime);

      /* FIXME: respect 24h time format */
      start_date = g_date_time_format (local_datetime,
                                       (dtstart.value->is_date == 1) ? "%x" : "%c");
      e_cal_component_free_datetime (&dtstart);

      e_cal_component_get_location (data->event_component, &location);
      if (location != NULL)
        desc = g_strconcat (start_date, ". ", location, NULL);
      else
        desc = g_strdup (start_date);

      g_variant_builder_add (&builder, "{sv}", "description", g_variant_new_string (desc));
      g_free (start_date);
      g_free (desc);

      g_variant_builder_add_value (&abuilder, g_variant_builder_end (&builder));
    }
  g_dbus_method_invocation_return_value (invocation, g_variant_new ("(aa{sv})", &abuilder));

  return TRUE;
}

static gboolean
activate_result_cb (GcalShellSearchProvider  *search_provider,
                    GDBusMethodInvocation    *invocation,
                    gchar                    *result,
                    gchar                   **terms,
                    guint32                   timestamp,
                    GcalShellSearchProvider2 *skel)
{
  GcalShellSearchProviderPrivate *priv;
  GApplication *application;
  GcalEventData *data;
  ECalComponentDateTime dtstart;

  priv = search_provider->priv;
  application = g_application_get_default ();

  data = gcal_manager_get_event_from_shell_search (priv->manager, result);
  e_cal_component_get_dtstart (data->event_component, &dtstart);
  if (dtstart.tzid != NULL)
    dtstart.value->zone = icaltimezone_get_builtin_timezone_from_tzid (dtstart.tzid);

  gcal_application_set_uuid (GCAL_APPLICATION (application), result);
  gcal_application_set_initial_date (GCAL_APPLICATION (application), dtstart.value);
  e_cal_component_free_datetime (&dtstart);

  g_application_activate (application);

  g_object_unref (data->event_component);
  g_free (data);
  return TRUE;
}

static gboolean
launch_search_cb (GcalShellSearchProvider  *search_provider,
                  GDBusMethodInvocation    *invocation,
                  gchar                   **terms,
                  guint32                   timestamp,
                  GcalShellSearchProvider2 *skel)
{
  GApplication *application;
  gchar *terms_joined;
  GList *windows;

  application = g_application_get_default ();
  g_application_activate (application);

  terms_joined = g_strjoinv (" ", terms);
  windows = g_list_reverse (gtk_application_get_windows (GTK_APPLICATION (application)));
  if (windows != NULL)
    {
      gcal_window_set_search_mode (GCAL_WINDOW (windows->data), TRUE);
      gcal_window_set_search_query (GCAL_WINDOW (windows->data), terms_joined);

      g_list_free (windows);
    }

  g_free (terms_joined);
  return TRUE;
}

static gboolean
query_completed_cb (GcalShellSearchProvider *search_provider,
                    GcalManager             *manager)
{
  GcalShellSearchProviderPrivate *priv = search_provider->priv;
  GList *events, *l;
  GVariantBuilder builder;

  GcalEventData *data;
  gchar *uuid;
  time_t current_time_t;

  g_hash_table_remove_all (priv->events);

  events = gcal_manager_get_shell_search_events (priv->manager);
  if (events == NULL)
    {
      g_dbus_method_invocation_return_value (priv->pending_search->invocation, g_variant_new ("(as)", NULL));
      goto out;
    }

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("as"));

  current_time_t = time (NULL);
  events = g_list_sort_with_data (events, sort_event_data, &current_time_t);
  for (l = events; l != NULL; l = g_list_next (l))
    {
      data = l->data;
      uuid = get_uuid_from_component (data->source, data->event_component);

      g_variant_builder_add (&builder, "s", uuid);

      g_hash_table_insert (priv->events, uuid, data);
    }
  g_list_free (events);

  g_dbus_method_invocation_return_value (priv->pending_search->invocation, g_variant_new ("(as)", &builder));

out:
  g_object_unref (priv->pending_search->invocation);
  g_strfreev (priv->pending_search->terms);
  g_clear_pointer (&(priv->pending_search), g_free);
  g_application_release (g_application_get_default ());
  return FALSE;
}

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
gcal_shell_search_provider_finalize (GObject *object)
{
  GcalShellSearchProviderPrivate *priv = GCAL_SHELL_SEARCH_PROVIDER (object)->priv;

  g_hash_table_destroy (priv->events);
  g_clear_object (&priv->skel);
  G_OBJECT_CLASS (gcal_shell_search_provider_parent_class)->finalize (object);
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

static void
gcal_shell_search_provider_class_init (GcalShellSearchProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gcal_shell_search_provider_finalize;
}

static void
gcal_shell_search_provider_init (GcalShellSearchProvider *self)
{
  GcalShellSearchProviderPrivate *priv = gcal_shell_search_provider_get_instance_private (self);

  priv->events = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, free_event_data);
  priv->skel = gcal_shell_search_provider2_skeleton_new ();

  g_signal_connect_swapped (priv->skel, "handle-get-initial-result-set", G_CALLBACK (get_initial_result_set_cb), self);
  g_signal_connect_swapped (priv->skel, "handle-get-subsearch-result-set", G_CALLBACK (get_subsearch_result_set_cb), self);
  g_signal_connect_swapped (priv->skel, "handle-get-result-metas", G_CALLBACK (get_result_metas_cb), self);
  g_signal_connect_swapped (priv->skel, "handle-activate-result", G_CALLBACK (activate_result_cb), self);
  g_signal_connect_swapped (priv->skel, "handle-launch-search", G_CALLBACK (launch_search_cb), self);

  self->priv = priv;
}

GcalShellSearchProvider*
gcal_shell_search_provider_new (void)
{
  return g_object_new (GCAL_TYPE_SHELL_SEARCH_PROVIDER, NULL);
}

gboolean
gcal_shell_search_provider_dbus_export (GcalShellSearchProvider *search_provider,
                                        GDBusConnection         *connection,
                                        const gchar             *object_path,
                                        GError                 **error)
{
  GcalShellSearchProviderPrivate *priv = GCAL_SHELL_SEARCH_PROVIDER (search_provider)->priv;

  return g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (priv->skel), connection, object_path, error);
}

void
gcal_shell_search_provider_dbus_unexport (GcalShellSearchProvider *search_provider,
                                          GDBusConnection         *connection,
                                          const gchar             *object_path)
{
  GcalShellSearchProviderPrivate *priv = GCAL_SHELL_SEARCH_PROVIDER (search_provider)->priv;

  if (g_dbus_interface_skeleton_has_connection (G_DBUS_INTERFACE_SKELETON (priv->skel), connection))
    g_dbus_interface_skeleton_unexport_from_connection (G_DBUS_INTERFACE_SKELETON (priv->skel), connection);
}

void
gcal_shell_search_provider_connect (GcalShellSearchProvider *search_provider,
                                    GcalManager             *manager)
{
  search_provider->priv->manager = manager;

  gcal_manager_setup_shell_search (manager, E_CAL_DATA_MODEL_SUBSCRIBER (search_provider));
  g_signal_connect_swapped (manager, "query-completed", G_CALLBACK (query_completed_cb), search_provider);
}
