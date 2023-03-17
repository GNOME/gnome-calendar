/* gcal-search-hit-event.c
 *
 * Copyright 2019 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "GcalSearchHitEvent"

#include "gcal-search-hit.h"
#include "gcal-search-hit-event.h"
#include "gcal-utils.h"

struct _GcalSearchHitEvent
{
  GcalSearchHit       parent;

  GcalEvent          *event;
};

G_DEFINE_TYPE (GcalSearchHitEvent, gcal_search_hit_event, GCAL_TYPE_SEARCH_HIT)

enum
{
  PROP_0,
  PROP_EVENT,
  N_PROPS,
};

static GParamSpec *properties [N_PROPS];

/*
 * Auxiliary methods
 */

static void
set_event (GcalSearchHitEvent *self,
           GcalEvent          *event)
{
  g_autoptr (GdkPaintable) paintable = NULL;
  g_autofree gchar *date_string = NULL;
  GcalSearchHit *search_hit;
  const GdkRGBA *color;
  GcalCalendar *calendar;

  self->event = g_object_ref (event);

  search_hit = GCAL_SEARCH_HIT (self);
  gcal_search_hit_set_id (search_hit, gcal_event_get_uid (event));
  gcal_search_hit_set_title (search_hit, gcal_event_get_summary (event));

  date_string = gcal_event_format_date (event);
  gcal_search_hit_set_subtitle (search_hit, date_string);

  calendar = gcal_event_get_calendar (self->event);
  color = gcal_calendar_get_color (calendar);
  paintable = get_circle_paintable_from_color (color, 16);
  gcal_search_hit_set_primary_icon (search_hit, paintable);
}


/*
 * GcalSearchHit overrides
 */

static void
gcal_search_hit_event_activate (GcalSearchHit *search_hit,
                                GtkWidget     *for_widget)
{
  GcalSearchHitEvent *self;
  GApplication *application;
  const gchar *event_uid;

  self = GCAL_SEARCH_HIT_EVENT (search_hit);
  event_uid = gcal_event_get_uid (self->event);

  application = g_application_get_default ();
  g_assert (application != NULL);

  g_action_group_activate_action (G_ACTION_GROUP (application),
                                  "open-event",
                                  g_variant_new_string (event_uid));
}

static gint
gcal_search_hit_event_get_priority (GcalSearchHit *search_hit)
{
  return 0;
}

static gint
gcal_search_hit_event_compare (GcalSearchHit *a,
                               GcalSearchHit *b)
{
  GcalCalendar *calendar_a;
  GcalCalendar *calendar_b;
  GcalEvent *event_a;
  GcalEvent *event_b;
  time_t now_utc;
  gint result;

  g_assert (GCAL_IS_SEARCH_HIT_EVENT (a));
  g_assert (GCAL_IS_SEARCH_HIT_EVENT (b));

  event_a = GCAL_SEARCH_HIT_EVENT (a)->event;
  event_b = GCAL_SEARCH_HIT_EVENT (b)->event;
  now_utc = time (NULL);

  result = gcal_event_compare_with_current (event_a, event_b, now_utc);
  if (result != 0)
    return result;

  calendar_a = gcal_event_get_calendar (event_a);
  calendar_b = gcal_event_get_calendar (event_b);

  return g_strcmp0 (gcal_calendar_get_name (calendar_b), gcal_calendar_get_name (calendar_a));
}


/*
 * GObject overrides
 */

static void
gcal_search_hit_event_finalize (GObject *object)
{
  GcalSearchHitEvent *self = (GcalSearchHitEvent *)object;

  g_clear_object (&self->event);

  G_OBJECT_CLASS (gcal_search_hit_event_parent_class)->finalize (object);
}

static void
gcal_search_hit_event_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  GcalSearchHitEvent *self = GCAL_SEARCH_HIT_EVENT (object);

  switch (prop_id)
    {
    case PROP_EVENT:
      g_value_set_object (value, self->event);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_search_hit_event_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  GcalSearchHitEvent *self = GCAL_SEARCH_HIT_EVENT (object);

  switch (prop_id)
    {
    case PROP_EVENT:
      g_assert (self->event == NULL);
      set_event (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_search_hit_event_class_init (GcalSearchHitEventClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GcalSearchHitClass *search_hit_class = GCAL_SEARCH_HIT_CLASS (klass);

  object_class->finalize = gcal_search_hit_event_finalize;
  object_class->get_property = gcal_search_hit_event_get_property;
  object_class->set_property = gcal_search_hit_event_set_property;

  search_hit_class->activate = gcal_search_hit_event_activate;
  search_hit_class->get_priority = gcal_search_hit_event_get_priority;
  search_hit_class->compare = gcal_search_hit_event_compare;

  properties[PROP_EVENT] = g_param_spec_object ("event",
                                                "Event",
                                                "Event",
                                                GCAL_TYPE_EVENT,
                                                G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gcal_search_hit_event_init (GcalSearchHitEvent *self)
{
}

GcalSearchHitEvent *
gcal_search_hit_event_new (GcalEvent *event)
{
  return g_object_new (GCAL_TYPE_SEARCH_HIT_EVENT,
                       "event", event,
                       NULL);
}

GcalEvent*
gcal_search_hit_event_get_event (GcalSearchHitEvent *self)
{
  g_return_val_if_fail (GCAL_IS_SEARCH_HIT_EVENT (self), NULL);

  return self->event;
}
