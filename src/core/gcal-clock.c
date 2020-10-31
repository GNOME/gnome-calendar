/* gcal-clock.c
 *
 * Copyright (C) 2017 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#define G_LOG_DOMAIN "GcalClock"

#include "gcal-clock.h"
#include "gcal-debug.h"
#include "gcal-utils.h"

#include <gio/gio.h>

struct _GcalClock
{
  GObject             parent;

  guint               timeout_id;

  GDateTime          *current;

  GDBusProxy         *proxy;
  GCancellable       *cancellable;
};

static gboolean      timeout_cb                                  (gpointer user_data);

G_DEFINE_TYPE (GcalClock, gcal_clock, G_TYPE_OBJECT)

enum
{
  MINUTE_CHANGED,
  HOUR_CHANGED,
  DAY_CHANGED,
  NUM_SIGNALS
};

static guint signals[NUM_SIGNALS] = { 0, };

/*
 * Auxiliary methods
 */

static void
update_current_date (GcalClock *self)
{
  g_autoptr (GDateTime) now = NULL;
  gboolean minute_changed;
  gboolean hour_changed;
  gboolean day_changed;

  GCAL_ENTRY;

  now = g_date_time_new_now_local ();

  day_changed = g_date_time_get_year (now) != g_date_time_get_year (self->current) ||
                g_date_time_get_day_of_year (now) != g_date_time_get_day_of_year (self->current);
  hour_changed = day_changed || g_date_time_get_hour (now) != g_date_time_get_hour (self->current);
  minute_changed = hour_changed || g_date_time_get_minute (now) != g_date_time_get_minute (self->current);

  if (day_changed)
    g_signal_emit (self, signals[DAY_CHANGED], 0);

  if (hour_changed)
    g_signal_emit (self, signals[HOUR_CHANGED], 0);

  if (minute_changed)
    g_signal_emit (self, signals[MINUTE_CHANGED], 0);

  g_debug ("Updating clock time");

  gcal_clear_date_time (&self->current);
  self->current = g_date_time_ref (now);

  GCAL_EXIT;
}

static void
schedule_update_timeout (GcalClock *self)
{
  g_autoptr (GDateTime) now = NULL;
  guint seconds_between;

  /* Remove the previous timeout if we came from resume */
  if (self->timeout_id > 0)
    {
      g_source_remove (self->timeout_id);
      self->timeout_id = 0;
    }

  now = g_date_time_new_now_local ();

  seconds_between = 60 - g_date_time_get_second (now);

  self->timeout_id = g_timeout_add_seconds (seconds_between, timeout_cb, self);

  g_debug ("Scheduling update for %d seconds", seconds_between);
}

/*
 * Callbacks
 */

static gboolean
timeout_cb (gpointer user_data)
{
  GcalClock *self = user_data;

  self->timeout_id = 0;

  update_current_date (self);
  schedule_update_timeout (self);

  return G_SOURCE_REMOVE;
}

static void
logind_signal_received_cb (GDBusProxy  *logind,
                           const gchar *sender,
                           const gchar *signal,
                           GVariant    *params,
                           GcalClock   *self)
{
  GVariant *child;
  gboolean resuming;

  if (!g_str_equal (signal, "PrepareForSleep"))
    return;

  child = g_variant_get_child_value (params, 0);
  resuming = !g_variant_get_boolean (child);

  /* Only emit :update when resuming */
  if (resuming)
    {
      update_current_date (self);
      schedule_update_timeout (self);
    }

  g_clear_pointer (&child, g_variant_unref);
}

static void
login_proxy_acquired_cb (GObject      *source,
                         GAsyncResult *res,
                         gpointer      user_data)
{
  GcalClock *self;
  GError *error;

  self = GCAL_CLOCK (user_data);
  error = NULL;

  self->proxy = g_dbus_proxy_new_for_bus_finish (res, &error);

  if (error)
    {
      g_warning ("Error acquiring logind DBus proxy: %s", error->message);
      g_clear_error (&error);
      return;
    }

  g_signal_connect_object (self->proxy,
                           "g-signal",
                           G_CALLBACK (logind_signal_received_cb),
                           self,
                           0);

  g_debug ("Successfully acquired logind DBus proxy");
}

static void
gcal_clock_finalize (GObject *object)
{
  GcalClock *self = (GcalClock *)object;

  g_cancellable_cancel (self->cancellable);

  if (self->timeout_id > 0)
    {
      g_source_remove (self->timeout_id);
      self->timeout_id = 0;
    }

  gcal_clear_date_time (&self->current);

  g_clear_object (&self->cancellable);
  g_clear_object (&self->proxy);

  G_OBJECT_CLASS (gcal_clock_parent_class)->finalize (object);
}

static void
gcal_clock_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
}

static void
gcal_clock_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
}

static void
gcal_clock_class_init (GcalClockClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gcal_clock_finalize;
  object_class->get_property = gcal_clock_get_property;
  object_class->set_property = gcal_clock_set_property;

  signals[DAY_CHANGED] = g_signal_new ("day-changed",
                                       GCAL_TYPE_CLOCK,
                                       G_SIGNAL_RUN_LAST,
                                       0, NULL, NULL, NULL,
                                       G_TYPE_NONE,
                                       0);

  signals[HOUR_CHANGED] = g_signal_new ("hour-changed",
                                        GCAL_TYPE_CLOCK,
                                        G_SIGNAL_RUN_LAST,
                                        0, NULL, NULL, NULL,
                                        G_TYPE_NONE,
                                        0);

  signals[MINUTE_CHANGED] = g_signal_new ("minute-changed",
                                          GCAL_TYPE_CLOCK,
                                          G_SIGNAL_RUN_LAST,
                                          0, NULL, NULL, NULL,
                                          G_TYPE_NONE,
                                          0);
}

static void
gcal_clock_init (GcalClock *self)
{
  self->current = g_date_time_new_now_local ();
  self->cancellable = g_cancellable_new ();

  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                            G_DBUS_PROXY_FLAGS_NONE,
                            NULL,
                            "org.freedesktop.login1",
                            "/org/freedesktop/login1",
                            "org.freedesktop.login1.Manager",
                            self->cancellable,
                            login_proxy_acquired_cb,
                            self);

  schedule_update_timeout (self);
}

/**
 * gcal_clock_new:
 *
 * Creates a new #GcalClock.
 *
 * Returns: (transfer full): a newly-allocated #GcalClock.
 */
GcalClock*
gcal_clock_new (void)
{
  return g_object_new (GCAL_TYPE_CLOCK, NULL);
}
