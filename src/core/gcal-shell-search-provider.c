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
#include "gcal-timeline.h"
#include "gcal-timeline-subscriber.h"
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
  GcalContext        *context;

  PendingSearch      *pending_search;
  GHashTable         *events;

  GDateTime          *range_start;
  GDateTime          *range_end;
  GcalTimeline       *timeline;
};

static void          gcal_timeline_subscriber_interface_init     (GcalTimelineSubscriberInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GcalShellSearchProvider, gcal_shell_search_provider, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GCAL_TYPE_TIMELINE_SUBSCRIBER,
                                                gcal_timeline_subscriber_interface_init));

enum
{
  PROP_0,
  PROP_CONTEXT,
  N_PROPS
};

static GParamSpec* properties[N_PROPS] = { NULL, };


/*
 * Auxiliary methods
 */

GdkTexture*
paintable_to_texture (GdkPaintable *paintable)
{
  g_autoptr (GtkSnapshot) snapshot = NULL;
  g_autoptr (GskRenderNode) node = NULL;
  g_autoptr (GskRenderer) renderer = NULL;
  g_autoptr (GdkTexture) texture = NULL;
  g_autoptr (GError) error = NULL;
  graphene_rect_t viewport;

  snapshot = gtk_snapshot_new ();
  gdk_paintable_snapshot (paintable, snapshot,
                          gdk_paintable_get_intrinsic_width (paintable),
                          gdk_paintable_get_intrinsic_height (paintable));
  node = gtk_snapshot_free_to_node (g_steal_pointer (&snapshot));

  renderer = gsk_cairo_renderer_new ();
  gsk_renderer_realize (renderer, NULL, &error);
  if (error)
    {
      g_warning ("Couldn't realize Cairo renderer: %s", error->message);
      return NULL;
    }

  viewport = GRAPHENE_RECT_INIT (0, 0,
                                 gdk_paintable_get_intrinsic_width (paintable),
                                 gdk_paintable_get_intrinsic_height (paintable));
  texture = gsk_renderer_render_texture (renderer, node, &viewport);
  gsk_renderer_unrealize (renderer);

  return g_steal_pointer (&texture);
}

static gint
sort_event_data (GcalEvent *a,
                 GcalEvent *b,
                 gpointer   user_data)
{
  return gcal_event_compare_with_current (a, b, GPOINTER_TO_INT (user_data));
}

static void
maybe_update_range (GcalShellSearchProvider *self)
{
  g_autoptr (GDateTime) start = NULL;
  g_autoptr (GDateTime) end = NULL;
  g_autoptr (GDateTime) now = NULL;
  gboolean range_changed = FALSE;

  GCAL_ENTRY;

  now = g_date_time_new_now (gcal_context_get_timezone (self->context));
  start = g_date_time_add_weeks (now, -1);
  end = g_date_time_add_weeks (now, 3);

  if (self->range_start && gcal_date_time_compare_date (self->range_start, start) != 0)
    {
      gcal_set_date_time (&self->range_start, start);
      range_changed = TRUE;
    }

  if (self->range_end && gcal_date_time_compare_date (self->range_end, end) != 0)
    {
      gcal_set_date_time (&self->range_end, end);
      range_changed = TRUE;
    }

  if (range_changed)
    gcal_timeline_subscriber_range_changed (GCAL_TIMELINE_SUBSCRIBER (self));
}

static gboolean
execute_search (GcalShellSearchProvider *self)
{
  g_autofree gchar *search_query = NULL;
  guint i;

  GCAL_ENTRY;

  maybe_update_range (self);

  search_query = g_strdup_printf ("(or (contains? \"summary\" \"%s\") (contains? \"description\" \"%s\"))",
                                  self->pending_search->terms[0],
                                  self->pending_search->terms[0]);

  for (i = 1; i < g_strv_length (self->pending_search->terms); i++)
    {
      g_autofree gchar *complete_query = NULL;
      g_autofree gchar *second_query = NULL;

      second_query = g_strdup_printf ("(or (contains? \"summary\" \"%s\") (contains? \"description\" \"%s\"))",
                                      self->pending_search->terms[0],
                                      self->pending_search->terms[0]);
      complete_query = g_strdup_printf ("(and %s %s)", search_query, second_query);

      g_clear_pointer (&search_query, g_free);
      search_query = g_steal_pointer (&complete_query);
    }

  gcal_timeline_set_filter (self->timeline,  search_query);
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

      g_application_release (g_application_get_default ());
    }
  else
    {
      self->pending_search = g_new0 (PendingSearch, 1);
    }

  self->pending_search->invocation = g_object_ref (invocation);
  self->pending_search->terms = g_strdupv (terms);

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
      g_autoptr (GdkPaintable) paintable = NULL;
      g_autoptr (GdkTexture) texture = NULL;
      g_autoptr (GVariant) icon_variant = NULL;

      uuid = results[i];
      event = g_hash_table_lookup (self->events, uuid);

      g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sv}"));
      g_variant_builder_add (&builder, "{sv}", "id", g_variant_new_string (uuid));
      g_variant_builder_add (&builder, "{sv}", "name", g_variant_new_string (gcal_event_get_summary (event)));

      paintable = get_circle_paintable_from_color (gcal_event_get_color (event), 96);
      texture = paintable_to_texture (paintable);
      icon_variant = g_icon_serialize (G_ICON (texture));
      g_variant_builder_add (&builder, "{sv}", "icon", icon_variant);

      local_datetime = g_date_time_to_local (gcal_event_get_date_start (event));
      start_date = g_date_time_format (local_datetime, gcal_event_get_all_day (event) ? "%x" : "%c");

      if (gcal_event_get_location (event))
        desc = g_strconcat (start_date, ". ", gcal_event_get_location (event), NULL);
      else
        desc = g_strdup (start_date);

      g_variant_builder_add (&builder, "{sv}", "description", g_variant_new_string (desc));
      g_variant_builder_add_value (&abuilder, g_variant_builder_end (&builder));
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

  event = g_hash_table_lookup (self->events, result);
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
    gcal_window_set_search_query (window, terms_joined);

  GCAL_RETURN (TRUE);
}

static void
on_timeline_completed_cb (GcalTimeline            *timeline,
                          GParamSpec              *pspec,
                          GcalShellSearchProvider *self)
{
  GVariantBuilder builder;
  g_autoptr (GList) events = NULL;
  GList *l;
  time_t current_time_t;

  GCAL_ENTRY;

  if (!self->pending_search)
    GCAL_RETURN ();

  if (!gcal_timeline_is_complete (timeline))
    GCAL_RETURN ();

  events = g_hash_table_get_values (self->events);
  if (!events)
    {
      g_dbus_method_invocation_return_value (self->pending_search->invocation, g_variant_new ("(as)", NULL));
      GCAL_GOTO (out);
    }

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("as"));

  current_time_t = time (NULL);
  events = g_list_sort_with_data (events, (GCompareDataFunc) sort_event_data, GINT_TO_POINTER (current_time_t));
  for (l = events; l != NULL; l = g_list_next (l))
    {
      const gchar *uid;

      uid = gcal_event_get_uid (l->data);

      if (g_hash_table_contains (self->events, uid))
        continue;

      g_variant_builder_add (&builder, "s", uid);

      g_hash_table_insert (self->events, g_strdup (uid), l->data);
    }

  g_dbus_method_invocation_return_value (self->pending_search->invocation, g_variant_new ("(as)", &builder));

out:
  g_clear_object (&self->pending_search->invocation);
  g_clear_pointer (&self->pending_search->terms, g_strfreev);
  g_clear_pointer (&self->pending_search, g_free);
  g_application_release (g_application_get_default ());

  GCAL_EXIT;
}

static void
on_manager_calendar_added_cb (GcalManager             *manager,
                              GcalCalendar            *calendar,
                              GcalShellSearchProvider *self)
{
  GCAL_ENTRY;

  gcal_timeline_add_calendar (self->timeline, calendar);

  GCAL_EXIT;
}

static void
on_manager_calendar_removed_cb (GcalManager             *manager,
                                GcalCalendar            *calendar,
                                GcalShellSearchProvider *self)
{
  GCAL_ENTRY;

  gcal_timeline_remove_calendar (self->timeline, calendar);

  GCAL_EXIT;
}

/*
 * GcalTimelineSubscriber iface
 */

static GcalRange*
gcal_shell_search_provider_get_range (GcalTimelineSubscriber *subscriber)
{
  GcalShellSearchProvider *self = GCAL_SHELL_SEARCH_PROVIDER (subscriber);

  return gcal_range_new (self->range_start, self->range_end, GCAL_RANGE_DEFAULT);
}

static void
gcal_shell_search_provider_add_event (GcalTimelineSubscriber *subscriber,
                                      GcalEvent              *event)
{
  GcalShellSearchProvider *self = GCAL_SHELL_SEARCH_PROVIDER (subscriber);

  GCAL_ENTRY;

  g_hash_table_insert (self->events,
                       g_strdup (gcal_event_get_uid (event)),
                       g_object_ref (event));

  GCAL_EXIT;
}

static void
gcal_shell_search_provider_update_event (GcalTimelineSubscriber *subscriber,
                                         GcalEvent              *old_event,
                                         GcalEvent              *event)
{
}

static void
gcal_shell_search_provider_remove_event (GcalTimelineSubscriber *subscriber,
                                         GcalEvent              *event)
{
  GcalShellSearchProvider *self = GCAL_SHELL_SEARCH_PROVIDER (subscriber);

  GCAL_ENTRY;

  g_hash_table_remove (self->events, gcal_event_get_uid (event));

  GCAL_EXIT;
}

static void
gcal_timeline_subscriber_interface_init (GcalTimelineSubscriberInterface *iface)
{
  iface->get_range = gcal_shell_search_provider_get_range;
  iface->add_event = gcal_shell_search_provider_add_event;
  iface->update_event = gcal_shell_search_provider_update_event;
  iface->remove_event = gcal_shell_search_provider_remove_event;
}


/*
 * GObject overrides
 */

static void
gcal_shell_search_provider_finalize (GObject *object)
{
  GcalShellSearchProvider *self = (GcalShellSearchProvider *) object;

  g_clear_pointer (&self->events, g_hash_table_destroy);
  g_clear_object (&self->context);
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
    case PROP_CONTEXT:
      g_value_set_object (value, self->context);
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
    case PROP_CONTEXT:
      {
        GcalManager *manager;

        g_assert (self->context == NULL);
        self->context = g_value_get_object (value);

        self->timeline = gcal_timeline_new (self->context);
        g_signal_connect (self->timeline, "notify::complete", G_CALLBACK (on_timeline_completed_cb), self);

        manager = gcal_context_get_manager (self->context);
        g_signal_connect (manager, "calendar-added", G_CALLBACK (on_manager_calendar_added_cb), self);
        g_signal_connect (manager, "calendar-removed", G_CALLBACK (on_manager_calendar_removed_cb), self);

        g_object_notify_by_pspec (object, properties[PROP_CONTEXT]);
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

  properties[PROP_CONTEXT] = g_param_spec_object ("context",
                                                  "The context object",
                                                  "The context object",
                                                  GCAL_TYPE_CONTEXT,
                                                  G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gcal_shell_search_provider_init (GcalShellSearchProvider *self)
{
  self->events = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
  self->skel = gcal_shell_search_provider2_skeleton_new ();

  g_signal_connect_object (self->skel, "handle-get-initial-result-set", G_CALLBACK (get_initial_result_set_cb), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->skel, "handle-get-subsearch-result-set", G_CALLBACK (get_subsearch_result_set_cb), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->skel, "handle-get-result-metas", G_CALLBACK (get_result_metas_cb), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->skel, "handle-activate-result", G_CALLBACK (activate_result_cb), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->skel, "handle-launch-search", G_CALLBACK (launch_search_cb), self, G_CONNECT_SWAPPED);
}

GcalShellSearchProvider*
gcal_shell_search_provider_new (GcalContext *context)
{
  return g_object_new (GCAL_TYPE_SHELL_SEARCH_PROVIDER,
                       "context", context,
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
