/* gcal-timer.c
 *
 * Copyright (C) 2017 - Florian Brosch <flo.brosch@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#define G_LOG_DOMAIN "Timer"

#include "gcal-timer.h"


/**
 * The #GcalTimer object structure.
 *
 * Do not use this one as #GSource.
 * Only use it in combination with its
 * methods.
 */
typedef struct _GcalTimer
{
  GSource            parent;
  gint64             last_event;
  gint64             default_duration;
} GcalTimer;


/* CbWrapperData:
 * @timer: The timer the callback applies to.
 * @callback: Original callback.
 * @destroy_notify: Original user-data destroy function.
 * @data: Original user data.
 *
 * Internal struct used to hide GSource internals
 * from API users.
 */
typedef struct
{
  GcalTimer          *timer; /* unowned */
  GCalTimerFunc       callback;
  GDestroyNotify      destroy_notify;
  gpointer            data;
} CbWrapperData;


static void          timer_func_destroy_notify_wrapper           (CbWrapperData      *wrapper);

static gboolean      timer_func_wrapper                          (CbWrapperData      *wrapper);

static gboolean      timer_source_dispatch                       (GcalTimer          *self,
                                                                  GSourceFunc         callback,
                                                                  CbWrapperData      *user_data);

static void          schedule_next                               (GcalTimer          *self);

static void          timer_source_finalize                       (GcalTimer          *self);



/*< private >*/
static void
timer_func_destroy_notify_wrapper (CbWrapperData *wrapper)
{
  g_return_if_fail (wrapper != NULL);

  if (wrapper->destroy_notify != NULL && wrapper->data != NULL)
    wrapper->destroy_notify (wrapper->data);

  g_free (wrapper);
}

static gboolean
timer_func_wrapper (CbWrapperData *wrapper)
{
  g_return_val_if_fail (wrapper != NULL, G_SOURCE_REMOVE);

  if (wrapper->callback != NULL)
    wrapper->callback (wrapper->timer, wrapper->data);

  return G_SOURCE_CONTINUE;
}

static gboolean
timer_source_dispatch (GcalTimer       *self,
                       GSourceFunc      user_callback,
                       CbWrapperData   *user_data)
{
  gboolean result = G_SOURCE_CONTINUE;

  g_return_val_if_fail (self != NULL, G_SOURCE_REMOVE);

  if (user_callback != NULL)
    result = user_callback (user_data);

  self->last_event = g_source_get_time ((GSource*) self);
  schedule_next (self);

  return result;
}

static void
schedule_next (GcalTimer *self)
{
  gint64 now;
  gint64 next;

  g_return_if_fail (self != NULL);
  g_return_if_fail (self->last_event >= 0);

  now = g_source_get_time ((GSource*) self);
  next = self->last_event + self->default_duration*G_GUINT64_CONSTANT(1000000);

  if (next > now)
    g_source_set_ready_time ((GSource*) self, next);
  else
    g_source_set_ready_time ((GSource*) self, 0);
}

static void
timer_source_finalize (GcalTimer *self)
{
}



/*< public >*/
/**
 * gcal_timer_new:
 * @default_duration: The duration (>0) between two events in seconds.
 *
 * Creates a new #GcalTimer object.
 *
 * While #GcalTimers are proper #GSources, they are not intended
 * to be used directly.
 *
 * Returns: (transfer full): A new timer object.
 */
GcalTimer*
gcal_timer_new (gint64 default_duration)
{
  GMainContext *cntxt; /* unowned */
  GcalTimer    *self;  /* owned */

  g_return_val_if_fail (default_duration > 0, NULL);

  static GSourceFuncs source_funcs =
      { NULL, /* prepare */
        NULL, /* check */
        (gboolean (*) (GSource*, GSourceFunc, gpointer)) timer_source_dispatch,
        (void (*) (GSource*)) timer_source_finalize
      };

  self = (GcalTimer*) g_source_new (&source_funcs, sizeof (GcalTimer));
  self->default_duration = default_duration;
  self->last_event = -1;

  cntxt = g_main_context_default ();
  g_source_set_ready_time ((GSource*) self, -1);
  g_source_attach ((GSource*) self, cntxt);

  return g_steal_pointer (&self);
}

/**
 * gcal_timer_set_start:
 * @self: The #GcalTimer.
 *
 * Starts this timer.
 */
void
gcal_timer_start (GcalTimer *self)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (!gcal_timer_is_running (self));

  self->last_event = g_source_get_time ((GSource*) self);
  schedule_next (self);
}

/**
 * gcal_timer_set_reset:
 * @self: The #GcalTimer.
 *
 * Re-schedules next event.
 */
void
gcal_timer_reset (GcalTimer *self)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (gcal_timer_is_running (self));

  self->last_event = g_source_get_time ((GSource*) self);
  schedule_next (self);
}

/**
 * gcal_timer_set_stop:
 * @self: The #GcalTimer.
 *
 * Stops a previously started timer.
 */
void
gcal_timer_stop (GcalTimer *self)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (gcal_timer_is_running (self));

  g_source_set_ready_time ((GSource*) self, -1);
}

/**
 * gcal_timer_is_running
 * @self: The #GcalTimer.
 *
 * Whether this timer is running.
 */
gboolean
gcal_timer_is_running (GcalTimer *self)
{
  g_return_val_if_fail (self != NULL, FALSE);

  return g_source_get_ready_time ((GSource*) self) >= 0;
}

/**
 * gcal_timer_set_duration:
 * @self: The #GcalTimer.
 * @duration: The new duration
 *
 * Changes the duration between two events.
 */
void
gcal_timer_set_default_duration (GcalTimer *self,
                                 gint64     duration)
{
  gint64 now;

  g_return_if_fail (self != NULL);
  g_return_if_fail (duration > 0);

  now = g_source_get_time ((GSource*) self);

  self->default_duration = duration;
  if (!gcal_timer_is_running (self))
    /* nothing to do (not running) */;
  else if (self->last_event + duration < now)
    g_source_set_ready_time ((GSource*) self, 0);
  else
    schedule_next (self);
}

/**
 * gcal_timer_get_default_duration:
 * @self: The #GcalTimer.
 *
 * Returns the default duration.
 */
gint64
gcal_timer_get_default_duration (GcalTimer *self)
{
  g_return_val_if_fail (self != NULL, -1);

  return self->default_duration;
}

/**
 * gcal_timer_set_callback:
 * @self:   The #GcalTimer.
 * @func:   The event handler callback function.
 * @data:   (nullable): User data passed to @func.
 * @notify: (nullable): free or unref function for @data.
 *
 * Sets the callback to be triggered after each duration
 * or given day-times.
 */
void
gcal_timer_set_callback (GcalTimer      *self,
                         GCalTimerFunc   func,
                         gpointer        data,
                         GDestroyNotify  notify)
{
  CbWrapperData *wrapper; /* owned */

  g_return_if_fail (self != NULL);
  g_return_if_fail (func != NULL);

  wrapper = g_new0 (CbWrapperData, 1);
  wrapper->timer = self;
  wrapper->callback = func;
  wrapper->destroy_notify = notify;
  wrapper->data = data;

  g_source_set_callback ((GSource*) self,
                         (GSourceFunc) timer_func_wrapper,
                         g_steal_pointer (&wrapper),
                         (GDestroyNotify) timer_func_destroy_notify_wrapper);
}

/**
 * gcal_timer_free:
 * @self: The #GcalTimer.
 *
 * Frees #GcalTimer.
 */
void
gcal_timer_free (GcalTimer *self)
{
  g_return_if_fail (self != NULL);
  g_source_destroy ((GSource *) self);
  g_source_unref ((GSource *) self);
}
