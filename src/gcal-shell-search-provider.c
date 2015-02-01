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
#include "gcal-manager.h"

typedef struct
{
  GcalShellSearchProvider2 *skeleton;

  gboolean                  searching;
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

static gboolean
get_initial_result_set_cb (GcalShellSearchProvider2 *skeleton,
                           GDBusMethodInvocation    *invocation,
                           gchar                   **terms,
                           gpointer                  user_data)
{
  gchar *strv;

  strv = g_strjoinv (", ", terms);
  g_debug ("will search: %s", strv);

  g_free (strv);
  return TRUE;
}

static gboolean
get_subsearch_result_set_cb (GcalShellSearchProvider2 *skeleton,
                             GDBusMethodInvocation    *invocation,
                             gchar                   **previous_results,
                             gchar                   **terms,
                             gpointer                  user_data)
{
  gchar *strv;

  strv = g_strjoinv (", ", terms);
  g_debug ("will subsearch: %s", strv);

  g_free (strv);
  return TRUE;
}

static gboolean
get_result_metas_cb (GcalShellSearchProvider2 *skeleton,
                     GDBusMethodInvocation    *invocation,
                     gchar                   **results,
                     gpointer                  user_data)
{
  gchar *strv;

  strv = g_strjoinv (", ", results);
  g_debug ("Get result metas: %s", strv);

  g_free (strv);
  return TRUE;
}

static gboolean
activate_result_cb (GcalShellSearchProvider2 *skeleton,
                    GDBusMethodInvocation    *invocation,
                    gchar                    *result,
                    gchar                   **terms,
                    guint32                   timestamp,
                    gpointer                  user_data)
{
  return TRUE;
}

static gboolean
launch_search_cb (GcalShellSearchProvider2 *skeleton,
                  GDBusMethodInvocation    *invocation,
                  gchar                   **terms,
                  guint32                   timestamp,
                  gpointer                  user_data)
{
  return TRUE;
}

static void
gcal_shell_search_provider_finalize (GObject *object)
{
  GcalShellSearchProviderPrivate *priv = GCAL_SHELL_SEARCH_PROVIDER (object)->priv;

  g_clear_object (&priv->skeleton);
  G_OBJECT_CLASS (gcal_shell_search_provider_parent_class)->finalize (object);
}

static void
gcal_subscriber_interface_init (ECalDataModelSubscriberInterface *iface)
{
  iface->component_added = NULL; //gcal_shell_search_provider_component_changed;
  iface->component_modified = NULL; //gcal_shell_search_provider_component_changed;
  iface->component_removed = NULL; //gcal_shell_search_provider_component_removed;
  iface->freeze = NULL; //gcal_shell_search_provider_freeze;
  iface->thaw = NULL; //gcal_shell_search_provider_thaw;
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

  priv->skeleton = gcal_shell_search_provider2_skeleton_new ();

  g_signal_connect (priv->skeleton, "handle-get-initial-result-set", G_CALLBACK (get_initial_result_set_cb), self);
  g_signal_connect (priv->skeleton, "handle-get-subsearch-result-set", G_CALLBACK (get_subsearch_result_set_cb), self);
  g_signal_connect (priv->skeleton, "handle-get-result-metas", G_CALLBACK (get_result_metas_cb), self);
  g_signal_connect (priv->skeleton, "handle-activate-result", G_CALLBACK (activate_result_cb), self);
  g_signal_connect (priv->skeleton, "handle-launch-search", G_CALLBACK (launch_search_cb), self);

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

  g_application_hold (g_application_get_default ());
  return g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (priv->skeleton), connection, object_path, error);
}

void
gcal_shell_search_provider_dbus_unexport (GcalShellSearchProvider *search_provider,
                                          GDBusConnection         *connection,
                                          const gchar             *object_path)
{
  GcalShellSearchProviderPrivate *priv = GCAL_SHELL_SEARCH_PROVIDER (search_provider)->priv;

  if (g_dbus_interface_skeleton_has_connection (G_DBUS_INTERFACE_SKELETON (priv->skeleton), connection))
    {
      g_application_release (g_application_get_default ());
      g_dbus_interface_skeleton_unexport_from_connection (G_DBUS_INTERFACE_SKELETON (priv->skeleton), connection);
    }
}

