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
  DzlSuggestion       parent;

  GcalEvent          *event;
};

static void          gcal_search_hit_interface_init              (GcalSearchHitInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GcalSearchHitEvent, gcal_search_hit_event, DZL_TYPE_SUGGESTION,
                         G_IMPLEMENT_INTERFACE (GCAL_TYPE_SEARCH_HIT,  gcal_search_hit_interface_init))

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
  g_autofree gchar *date_string = NULL;
  DzlSuggestion *suggestion;

  self->event = g_object_ref (event);

  suggestion = DZL_SUGGESTION (self);
  dzl_suggestion_set_id (suggestion, gcal_event_get_uid (event));
  dzl_suggestion_set_title (suggestion, gcal_event_get_summary (event));

  /* FIXME: use a better date description */
  date_string = g_date_time_format (gcal_event_get_date_start (event), "%x %X %z");
  dzl_suggestion_set_subtitle (suggestion, date_string);
}


/*
 * DzlSuggestion overrides
 */

static cairo_surface_t*
gcal_search_hit_event_get_icon_surface (DzlSuggestion *suggestion,
                                        GtkWidget     *widget)
{
  GcalSearchHitEvent *self;
  cairo_surface_t *surface;
  ESource *source;
  GdkRGBA color;

  self = GCAL_SEARCH_HIT_EVENT (suggestion);
  source = gcal_event_get_source (self->event);

  get_color_name_from_source (source, &color);
  surface = get_circle_surface_from_color (&color, 16);

  /* Inject our custom style class into the given widget */
  gtk_style_context_add_class (gtk_widget_get_style_context (widget), "calendar-color-image");

  return surface;
}


/*
 * GcalSearchHit interface
 */

static gint
gcal_search_hit_event_get_priority (GcalSearchHit *search_hit)
{
  return 0;
}

static gint
gcal_search_hit_event_compare (GcalSearchHit *a,
                               GcalSearchHit *b)
{
  GcalEvent *event_a;
  GcalEvent *event_b;
  time_t now_utc;

  g_assert (GCAL_IS_SEARCH_HIT_EVENT (a));
  g_assert (GCAL_IS_SEARCH_HIT_EVENT (b));

  event_a = GCAL_SEARCH_HIT_EVENT (a)->event;
  event_b = GCAL_SEARCH_HIT_EVENT (b)->event;
  now_utc = time (NULL);

  return -gcal_event_compare_with_current (event_a, event_b, now_utc);
}

static void
gcal_search_hit_interface_init (GcalSearchHitInterface *iface)
{
  iface->get_priority = gcal_search_hit_event_get_priority;
  iface->compare = gcal_search_hit_event_compare;
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
  DzlSuggestionClass *suggestion_class = DZL_SUGGESTION_CLASS (klass);

  object_class->finalize = gcal_search_hit_event_finalize;
  object_class->get_property = gcal_search_hit_event_get_property;
  object_class->set_property = gcal_search_hit_event_set_property;

  suggestion_class->get_icon_surface = gcal_search_hit_event_get_icon_surface;

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
