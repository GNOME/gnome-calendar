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

#include "gcal-utils.h"

typedef struct
{
  GcalShellSearchProvider2 *skel;
  GcalManager              *manager;

  GDBusMethodInvocation    *search_invocation; /* current */
  gchar                    *search_query;

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

static void
execute_search (GcalShellSearchProvider *search_provider,
                GDBusMethodInvocation   *invocation,
                gchar                  **terms)
{
  GcalShellSearchProviderPrivate *priv;

  guint i;
  gchar *search_query;

  icaltimetype date;
  icaltimezone *zone;
  time_t range_start, range_end;

  priv = search_provider->priv;

  /* don't attempt searches for a single character */
  if (g_strv_length (terms) == 1 && g_utf8_strlen (terms[0], -1) == 1)
    {
      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(as)", NULL));
      return;
    }

  /* FIXME: handle hold/release */
  //g_application_hold (g_application_get_default ());

  priv->search_invocation = g_object_ref (invocation);

  zone = gcal_manager_get_system_timezone (priv->manager);
  date = icaltime_current_time_with_zone (zone);
  icaltime_adjust (&date, -7, 0, 0, 0); /* -1 weeks from today */
  range_start = icaltime_as_timet_with_zone (date, zone);

  icaltime_adjust (&date, 21 * 2, 0, 0, 0); /* +3 weeks from today */
  range_end = icaltime_as_timet_with_zone (date, zone);

  gcal_manager_set_shell_search_subscriber (priv->manager, E_CAL_DATA_MODEL_SUBSCRIBER (search_provider),
                                            range_start, range_end);

  /* FIXME: terms */
  search_query = g_strdup_printf ("(or (contains? \"summary\" \"%s\") (contains? \"description\" \"%s\"))",
                                  terms[0], terms[0]);
  for (i = 1; i < g_strv_length (terms); i++)
    {
      gchar *complete_query;
      gchar *second_query = g_strdup_printf ("(or (contains? \"summary\" \"%s\") (contains? \"description\" \"%s\"))",
                                             terms[0], terms[0]);
      complete_query = g_strdup_printf ("(and %s %s)", search_query, second_query);

      g_free (second_query);
      g_free (search_query);

      search_query = complete_query;
    }

  g_free (priv->search_query);
  priv->search_query = search_query;
  gcal_manager_set_shell_search_query (priv->manager,  search_query);
}

static gboolean
get_initial_result_set_cb (GcalShellSearchProvider  *search_provider,
                           GDBusMethodInvocation    *invocation,
                           gchar                   **terms,
                           GcalShellSearchProvider2 *skel)
{
  execute_search (search_provider, invocation, terms);
  return TRUE;
}

static gboolean
get_subsearch_result_set_cb (GcalShellSearchProvider  *search_provider,
                             GDBusMethodInvocation    *invocation,
                             gchar                   **previous_results,
                             gchar                   **terms,
                             GcalShellSearchProvider2 *skel)
{
  execute_search (search_provider, invocation, terms);
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
  gchar *uuid, *desc, *str;
  gchar start_date [64];
  ECalComponentText summary;
  ECalComponentDateTime dtstart;
  struct tm tm_date;
  GdkRGBA color;
  GVariantBuilder abuilder, builder;
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

      gdk_rgba_parse (&color, get_color_name_from_source (data->source));
      gicon = get_circle_pixbuf_from_color (&color, 128);
      g_variant_builder_add (&builder, "{sv}", "icon", g_icon_serialize (G_ICON (gicon)));
      g_object_unref (gicon);

      str = get_desc_from_component (data->event_component, "\n");
      if (str != NULL)
        {
          gchar *end = g_utf8_strchr (g_strstrip (str), -1, '\n');
          if (end == NULL)
            desc = g_strdup (str);
          else
            desc = g_utf8_substring (str, 0, end - str);
          g_free (str);
        }
      else
        {
          const gchar *location;
          e_cal_component_get_location (data->event_component, &location);
          if (location != NULL)
            desc = g_strdup (location);
          else
            desc = e_source_dup_display_name (data->source);
        }

      e_cal_component_get_dtstart (data->event_component, &dtstart);
      tm_date = icaltimetype_to_tm (dtstart.value);
      /* FIXME: respect 24h time format */
      e_utf8_strftime_fix_am_pm (start_date, 64, (dtstart.value->is_date == 1) ? "%x" : "%c", &tm_date);
      e_cal_component_free_datetime (&dtstart);

      str = desc;
      desc = g_strconcat (start_date, ". ", str, NULL);
      g_variant_builder_add (&builder, "{sv}", "description", g_variant_new_string (desc));
      g_variant_builder_add_value (&abuilder, g_variant_builder_end (&builder));

      g_free (desc);
      g_free (str);
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
  return TRUE;
}

static gboolean
launch_search_cb (GcalShellSearchProvider  *search_provider,
                  GDBusMethodInvocation    *invocation,
                  gchar                   **terms,
                  guint32                   timestamp,
                  GcalShellSearchProvider2 *skel)
{
  return TRUE;
}

static void
query_completed_cb (GcalShellSearchProvider *search_provider,
                    GcalManager             *manager)
{
  GcalShellSearchProviderPrivate *priv = search_provider->priv;
  GList *events, *l;
  GVariantBuilder builder;

  GcalEventData *data;
  gchar *uuid;

  if (!gcal_manager_load_completed (priv->manager))
    return;

  g_hash_table_remove_all (priv->events);

  events = gcal_manager_get_shell_search_events (priv->manager);
  if (events == NULL)
    {
      g_dbus_method_invocation_return_value (priv->search_invocation, g_variant_new ("(as)", NULL));
      g_object_unref (priv->search_invocation);
      return;
    }

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("as"));
  /* FIXME: add sorting and ranking */
  //events = g_list_sort (events, search_compare_func);
  for (l = events; l != NULL; l = g_list_next (l))
    {
      data = l->data;
      uuid = get_uuid_from_component (data->source, data->event_component);

      g_variant_builder_add (&builder, "s", uuid);

      g_hash_table_insert (priv->events, uuid, data);
    }

  /* FIXME: handle hold/release
  if (events != NULL)
    g_application_release (g_application_get_default ());*/

  g_dbus_method_invocation_return_value (priv->search_invocation, g_variant_new ("(as)", &builder));

  g_list_free (events);
  g_object_unref (priv->search_invocation);
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
