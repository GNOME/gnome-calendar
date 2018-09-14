/* gcal-event.c
 *
 * Copyright (C) 2016 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#define G_LOG_DOMAIN "GcalEvent"

#include "gconstructor.h"
#include "gcal-debug.h"
#include "gcal-event.h"
#include "gcal-utils.h"
#include "gcal-recurrence.h"

#define LIBICAL_TZID_PREFIX "/freeassociation.sourceforge.net/"

/**
 * SECTION:gcal-event
 * @short_description: A class that represents an event
 * @title:GcalEvent
 * @stability:unstable
 * @see_also:GcalEventWidget,GcalManager
 *
 * The #GcalEvent class represents an appointment, with
 * various functions to modify it. All the changes are
 * transient. To persistently store the changes, you
 * need to call gcal_manager_update_event().
 *
 * Although the #ECalComponent may have no end date. In this
 * case, gcal_event_get_date_end() returns the same date that
 * gcal_event_get_date_start().
 *
 * #GcalEvent implements #GInitable, and creating it possibly
 * can generate an error. At the moment, the only error that
 * can be generate is #GCAL_EVENT_ERROR_INVALID_START_DATE.
 *
 * ## Example:
 * |[<!-- language="C" -->
 * GcalEvent *event;
 * GError *error;
 *
 * error = NULL;
 * event = gcal_event_new (source, component, &error);
 *
 * if (error)
 *   {
 *     g_warning ("Error creating event: %s", error->message);
 *     g_clear_error (&error);
 *     return;
 *   }
 *
 * ...
 * ]|
 */

struct _GcalEvent
{
  GObject             parent;

  gchar              *uid;
  gboolean            has_recurrence;

  /*
   * The description is cached in the class because it
   * may come as a GSList of descriptions, in which
   * case we merge them and cache it here.
   */
  gchar              *description;

  GDateTime          *dt_start;
  GDateTime          *dt_end;

  GdkRGBA            *color;
  GBinding           *color_binding;

  gboolean            all_day;

  /* A map of GcalAlarmType */
  GHashTable         *alarms;

  ECalComponent      *component;
  ESource            *source;

  GcalRecurrence     *recurrence;

  gboolean            is_valid : 1;
  GError             *initialization_error;
};

static void          gcal_event_initable_iface_init              (GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (GcalEvent, gcal_event, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, gcal_event_initable_iface_init))

G_DEFINE_QUARK (GcalEvent, gcal_event_error);

enum {
  PROP_0,
  PROP_ALL_DAY,
  PROP_COLOR,
  PROP_COMPONENT,
  PROP_DESCRIPTION,
  PROP_DATE_END,
  PROP_DATE_START,
  PROP_LOCATION,
  PROP_SOURCE,
  PROP_SUMMARY,
  PROP_TIMEZONE,
  PROP_UID,
  PROP_HAS_RECURRENCE,
  PROP_RECURRENCE,
  N_PROPS
};

static GParamSpec* properties[N_PROPS] = { NULL, };


/*
 * GcalEvent cache
 */

static GHashTable *event_cache = NULL;

G_DEFINE_CONSTRUCTOR (init_event_cache_map);

static void
init_event_cache_map (void)
{
  event_cache = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, NULL);
}

G_DEFINE_DESTRUCTOR (destroy_event_cache_map);

static void
destroy_event_cache_map (void)
{
  GList *events;

  g_debug ("Number of cached events at destruction: %d", g_hash_table_size (event_cache));

  /* Destroy all events */
  events = g_hash_table_get_values (event_cache);
  g_list_free_full (events, g_object_unref);

  g_hash_table_destroy (event_cache);
}

/*
 * Auxiliary methods
 */

static GTimeZone*
get_timezone_from_ical (ECalComponentDateTime *comp)
{
  const icaltimezone *zone;
  GTimeZone *tz;

  zone = icaltime_get_timezone (*comp->value);

  if (comp->value->is_date)
    {
      /*
       * When loading an all day event, the timezone must not be taken into account
       * when loading the date. Since all events in ical are UTC, we match it by
       * setting the timezone as UTC as well.
       */
      tz = g_time_zone_new_utc ();
    }
  else if (comp->tzid)
    {
      const gchar *real_tzid;

      real_tzid = comp->tzid;

      if (g_str_has_prefix (comp->tzid, LIBICAL_TZID_PREFIX))
        real_tzid += strlen (LIBICAL_TZID_PREFIX);

      tz = g_time_zone_new (real_tzid);
    }
  else if (zone)
    {
      g_autofree gchar *tzid = NULL;
      gint offset;

      offset = icaltimezone_get_utc_offset ((icaltimezone*) icaltime_get_timezone (*comp->value),
                                            comp->value, NULL);
      tzid = format_utc_offset (offset);
      tz = g_time_zone_new (tzid);
    }
  else
    {
      tz = g_time_zone_new_utc ();
    }

  g_assert (tz != NULL);

  GCAL_TRACE_MSG ("%s (%p)", g_time_zone_get_identifier (tz), tz);

  return tz;
}

static ECalComponentDateTime*
build_component_from_datetime (GcalEvent *self,
                               GDateTime *dt)
{
  ECalComponentDateTime *comp_dt;

  if (!dt)
    return NULL;

  comp_dt = g_new0 (ECalComponentDateTime, 1);
  comp_dt->value = datetime_to_icaltime (dt);
  comp_dt->value->is_date = self->all_day;

  if (self->all_day)
    {
      comp_dt->value->zone = icaltimezone_get_utc_timezone ();
      comp_dt->tzid = g_strdup ("UTC");
    }
  else
    {
      comp_dt->value->zone = e_cal_util_get_system_timezone ();
      comp_dt->tzid = g_strdup (icaltimezone_get_tzid ((icaltimezone *) comp_dt->value->zone));
    }

  return comp_dt;
}

static gboolean
string_to_color (GBinding     *binding,
                 const GValue *from_value,
                 GValue       *to_value,
                 gpointer      user_data)
{
  GdkRGBA color;

  if (!gdk_rgba_parse (&color, g_value_get_string (from_value)))
    gdk_rgba_parse (&color, "#ffffff"); /* calendar default colour */

  g_value_set_boxed (to_value, &color);

  return TRUE;
}

static void
gcal_event_update_uid_internal (GcalEvent *self)
{
  ECalComponentId *id;
  const gchar *source_id;

  /* Setup event uid */
  source_id = self->source ? e_source_get_uid (self->source) : "";
  id = e_cal_component_get_id (self->component);

  /* Clear the previous uid */
  g_clear_pointer (&self->uid, g_free);

  if (id->rid != NULL)
    {
      self->uid = g_strdup_printf ("%s:%s:%s",
                                   source_id,
                                   id->uid,
                                   id->rid);
    }
  else
    {
      self->uid = g_strdup_printf ("%s:%s",
                                   source_id,
                                   id->uid);
    }

  e_cal_component_free_id (id);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_UID]);
}

static void
load_alarms (GcalEvent *self)
{
  GList *alarm_uids, *l;

  alarm_uids = e_cal_component_get_alarm_uids (self->component);

  for (l = alarm_uids; l != NULL; l = l->next)
    {
      ECalComponentAlarm *alarm;
      gint trigger_minutes;

      alarm = e_cal_component_get_alarm (self->component, l->data);
      trigger_minutes = get_alarm_trigger_minutes (self, alarm);

      /* We only support a single alarm for a given time */
      if (!g_hash_table_contains (self->alarms, GINT_TO_POINTER (trigger_minutes)))
        {
          g_hash_table_insert (self->alarms,
                               GINT_TO_POINTER (trigger_minutes),
                               g_strdup (e_cal_component_alarm_get_uid (alarm)));
        }

      e_cal_component_alarm_free (alarm);
    }

  cal_obj_uid_list_free (alarm_uids);
}

static void
gcal_event_set_component_internal (GcalEvent     *self,
                                   ECalComponent *component)
{
  if (g_set_object (&self->component, component))
    {
      ECalComponentDateTime start;
      ECalComponentDateTime end;
      icaltimetype date;
      GDateTime *date_start;
      GTimeZone *zone_start = NULL;
      GDateTime *date_end;
      GTimeZone *zone_end = NULL;
      gboolean start_is_all_day, end_is_all_day;
      gchar *description;

      /* Setup start date */
      e_cal_component_get_dtstart (component, &start);

      /*
       * A NULL start date is invalid. We set something bogus to proceed, and make
       * it set a GError and return NULL.
       */
      if (!start.value)
        {
          self->is_valid = FALSE;
          g_set_error (&self->initialization_error,
                       GCAL_EVENT_ERROR,
                       GCAL_EVENT_ERROR_INVALID_START_DATE,
                       "Event '%s' has an invalid start date", gcal_event_get_uid (self));

          start.value = g_new0 (icaltimetype, 1);
          *start.value = icaltime_today ();
        }

      GCAL_TRACE_MSG ("Retrieving start timezone");

      date = icaltime_normalize (*start.value);
      zone_start = get_timezone_from_ical (&start);
      date_start = g_date_time_new (zone_start,
                                    date.year, date.month, date.day,
                                    date.is_date ? 0 : date.hour,
                                    date.is_date ? 0 : date.minute,
                                    date.is_date ? 0 : date.second);
      start_is_all_day = datetime_is_date (date_start);

      self->dt_start = date_start;

      /* Setup end date */
      e_cal_component_get_dtend (component, &end);

      if(!end.value)
        {
          self->all_day = TRUE;
        }
      else
        {
          GCAL_TRACE_MSG ("Retrieving end timezone");

          date = icaltime_normalize (*end.value);
          zone_end = get_timezone_from_ical (&end);
          date_end = g_date_time_new (zone_end,
                                      date.year, date.month, date.day,
                                      date.is_date ? 0 : date.hour,
                                      date.is_date ? 0 : date.minute,
                                      date.is_date ? 0 : date.second);
          end_is_all_day = datetime_is_date (date_end);

          self->dt_end = g_date_time_ref (date_end);

          /* Setup all day */
          self->all_day = start_is_all_day && end_is_all_day;

          e_cal_component_free_datetime (&end);
        }

      /* Setup description */
      description = get_desc_from_component (component, "\n\n");
      gcal_event_set_description (self, description);

      /* Setup UID */
      gcal_event_update_uid_internal (self);

      /* Set has-recurrence to check if the component has recurrence or not */
      self->has_recurrence = e_cal_component_has_recurrences(component);

      /* Load the recurrence-rules in GcalRecurrence struct */
      self->recurrence = gcal_recurrence_parse_recurrence_rules (component);

      /* Load and setup the alarms */
      load_alarms (self);

      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_HAS_RECURRENCE]);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_RECURRENCE]);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_COMPONENT]);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LOCATION]);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SUMMARY]);

      g_clear_pointer (&zone_start, g_time_zone_unref);
      g_clear_pointer (&zone_end, g_time_zone_unref);
      g_clear_pointer (&description, g_free);

      e_cal_component_free_datetime (&start);
    }
}

/*
 * GInitable iface implementation
 */

G_LOCK_DEFINE_STATIC (init_lock);

static gboolean
gcal_event_initable_init (GInitable     *initable,
                          GCancellable  *cancellable,
                          GError       **error)
{
  GcalEvent *self;

  self = GCAL_EVENT (initable);

  G_LOCK (init_lock);

  if (!self->is_valid)
    g_propagate_error (error, g_error_copy (self->initialization_error));

  G_UNLOCK (init_lock);

  return self->is_valid;
}

static void
gcal_event_initable_iface_init (GInitableIface *iface)
{
  iface->init = gcal_event_initable_init;
}

static void
gcal_event_finalize (GObject *object)
{
  GcalEvent *self = (GcalEvent *)object;

  g_debug ("Removing '%s' (%p) from cache", self->uid, self);

  g_hash_table_remove (event_cache, self->uid);

  g_clear_pointer (&self->dt_start, g_date_time_unref);
  g_clear_pointer (&self->dt_end, g_date_time_unref);
  g_clear_pointer (&self->description, g_free);
  g_clear_pointer (&self->alarms, g_hash_table_unref);
  g_clear_pointer (&self->uid, g_free);
  g_clear_pointer (&self->color, gdk_rgba_free);
  g_clear_object (&self->component);
  g_clear_object (&self->source);
  g_clear_pointer (&self->recurrence, gcal_recurrence_unref);

  G_OBJECT_CLASS (gcal_event_parent_class)->finalize (object);
}

static void
gcal_event_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  GcalEvent *self = GCAL_EVENT (object);

  switch (prop_id)
    {
    case PROP_ALL_DAY:
      g_value_set_boolean (value, self->all_day);
      break;

    case PROP_COLOR:
      g_value_set_boxed (value, self->color);
      break;

    case PROP_COMPONENT:
      g_value_set_object (value, self->component);
      break;

    case PROP_DATE_END:
      g_value_set_boxed (value, gcal_event_get_date_end (self));
      break;

    case PROP_DATE_START:
      g_value_set_boxed (value, self->dt_start);
      break;

    case PROP_DESCRIPTION:
      g_value_set_string (value, gcal_event_get_description (self));
      break;

    case PROP_LOCATION:
      g_value_set_string (value, gcal_event_get_location (self));
      break;

    case PROP_SOURCE:
      g_value_set_object (value, self->source);
      break;

    case PROP_SUMMARY:
      g_value_set_string (value, gcal_event_get_summary (self));
      break;

    case PROP_UID:
      g_value_set_string (value, self->uid);
      break;

    case PROP_HAS_RECURRENCE:
      g_value_set_boolean (value, self->has_recurrence);
      break;

    case PROP_RECURRENCE:
      g_value_set_boxed (value, self->recurrence);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_event_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  GcalEvent *self = GCAL_EVENT (object);

  switch (prop_id)
    {
    case PROP_ALL_DAY:
      gcal_event_set_all_day (self, g_value_get_boolean (value));
      break;

    case PROP_COLOR:
      gcal_event_set_color (self, g_value_get_boxed (value));
      break;

    case PROP_COMPONENT:
      gcal_event_set_component_internal (self, g_value_get_object (value));
      break;

    case PROP_DATE_END:
      gcal_event_set_date_end (self, g_value_get_boxed (value));
      break;

    case PROP_DATE_START:
      gcal_event_set_date_start (self, g_value_get_boxed (value));
      break;

    case PROP_DESCRIPTION:
      gcal_event_set_description (self, g_value_get_string (value));
      break;

    case PROP_LOCATION:
      gcal_event_set_location (self, g_value_get_string (value));
      break;

    case PROP_SOURCE:
      gcal_event_set_source (self, g_value_get_object (value));
      break;

    case PROP_SUMMARY:
      gcal_event_set_summary (self, g_value_get_string (value));
      break;

    case PROP_RECURRENCE:
      gcal_event_set_recurrence (self, g_value_get_boxed (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_event_class_init (GcalEventClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gcal_event_finalize;
  object_class->get_property = gcal_event_get_property;
  object_class->set_property = gcal_event_set_property;

  /**
   * GcalEvent::all-day:
   *
   * Whether the event is all day or not.
   */

  properties[PROP_ALL_DAY] = g_param_spec_boolean ("all-day",
                                                   "If event is all day",
                                                   "Whether the event is all day or not",
                                                   FALSE,
                                                   G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GcalEvent::color:
   *
   * The color of the event.
   */
  properties[PROP_COLOR] = g_param_spec_boxed ("color",
                                               "Color of the event",
                                               "The color of the event",
                                               GDK_TYPE_RGBA,
                                               G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GcalEvent::component:
   *
   * The #ECalComponent of this event.
   */
  properties[PROP_COMPONENT] = g_param_spec_object ("component",
                                                     "Component",
                                                     "The ECalComponent of the event",
                                                     E_TYPE_CAL_COMPONENT,
                                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GcalEvent::date-end:
   *
   * The end date of the event.
   */
  properties[PROP_DATE_END] = g_param_spec_boxed ("date-end",
                                                  "End date of the event",
                                                  "The end date of the event",
                                                  G_TYPE_DATE_TIME,
                                                  G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GcalEvent::date-start:
   *
   * The start date of the event.
   */
  properties[PROP_DATE_START] = g_param_spec_boxed ("date-start",
                                                    "Start date of the event",
                                                    "The start date of the event",
                                                    G_TYPE_DATE_TIME,
                                                    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GcalEvent::description:
   *
   * The description of the event.
   */
  properties[PROP_DESCRIPTION] = g_param_spec_string ("description",
                                                      "Description of the event",
                                                      "The description of the event",
                                                      "",
                                                      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
  * GcalEvent::has-recurrence:
  *
  * The recurrence property of the event.
  */
  properties[PROP_HAS_RECURRENCE] = g_param_spec_boolean ("has-recurrence",
                                                          "If event has recurrence",
                                                          "Whether the event has recurrence or not",
                                                          FALSE,
                                                          G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GcalEvent::location:
   *
   * The location of the event.
   */
  properties[PROP_LOCATION] = g_param_spec_string ("location",
                                                   "Location of the event",
                                                   "The location of the event",
                                                   "",
                                                   G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
  * GcalEvent::recurrence:
  *
  * The recurrence-rules property of the event.
  */
  properties[PROP_RECURRENCE] = g_param_spec_boxed ("recurrence",
                                                    "Recurrence property of the event",
                                                    "The recurrence property of the event",
                                                    GCAL_TYPE_RECURRENCE,
                                                    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GcalEvent::source:
   *
   * The #ESource this event belongs to.
   */
  properties[PROP_SOURCE] = g_param_spec_object ("source",
                                                 "ESource",
                                                 "The ESource this event belongs to",
                                                 E_TYPE_SOURCE,
                                                 G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GcalEvent::summary:
   *
   * The summary of the event.
   */
  properties[PROP_SUMMARY] = g_param_spec_string ("summary",
                                                        "Summary of the event",
                                                        "The summary of the event",
                                                        "",
                                                        G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GcalEvent::timezone:
   *
   * The timezone of the event.
   */
  properties[PROP_TIMEZONE] = g_param_spec_boxed ("timezone",
                                                  "Timezone of the event",
                                                  "The timezone of the event",
                                                  G_TYPE_TIME_ZONE,
                                                  G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GcalEvent::uid:
   *
   * The unique identifier of the event.
   */
  properties[PROP_UID] = g_param_spec_string ("uid",
                                              "Identifier of the event",
                                              "The unique identifier of the event",
                                              "",
                                              G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gcal_event_init (GcalEvent *self)
{
  GdkRGBA rgba;

  /* Default color of events */
  gdk_rgba_parse (&rgba, "#ffffff");
  self->color = gdk_rgba_copy (&rgba);

  /* Alarms */
  self->alarms = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);

  self->is_valid = TRUE;
}

/**
 * gcal_event_new:
 * @source: (nullable): an #ESource
 * @component: a #ECalComponent
 * @error: (nullable): return location for a #GError
 *
 * Creates a new event which belongs to @source and
 * is represented by @component. New events will have
 * a %NULL @source.
 *
 * Returns: (transfer full)(nullable): a #GcalEvent
 */
GcalEvent*
gcal_event_new (ESource        *source,
                ECalComponent  *component,
                GError        **error)
{
  GcalEvent *event;
  g_autofree gchar *uuid;

  uuid = get_uuid_from_component (source, component);

  if (g_hash_table_contains (event_cache, uuid))
    {
      g_debug ("Using cached value for %s", uuid);

      event = g_hash_table_lookup (event_cache, uuid);
      gcal_event_set_component_internal (event, component);
      g_object_ref (event);
    }
  else
    {
      event = g_initable_new (GCAL_TYPE_EVENT,
                              NULL,
                              error,
                              "source", source,
                              "component", component,
                              NULL);

      if (event)
        {
          g_debug ("Adding %s to the cache", event->uid);
          g_hash_table_insert (event_cache, event->uid, event);
        }
    }

  return event;
}

/**
 * gcal_event_get_all_day:
 * @self: a #GcalEvent
 *
 * Retrieves whether the event is all day or not.
 *
 * Returns: %TRUE if @self is all day, %FALSE otherwise.
 */
gboolean
gcal_event_get_all_day (GcalEvent *self)
{
  g_return_val_if_fail (GCAL_IS_EVENT (self), FALSE);

  return self->all_day;
}

/**
 * gcal_event_get_color:
 * @self: a #GcalEvent
 *
 * Retrieves the color of the event.
 *
 * Returns: (transfer none): a #GdkRGBA
 */
GdkRGBA*
gcal_event_get_color (GcalEvent *self)
{
  g_return_val_if_fail (GCAL_IS_EVENT (self), NULL);

  return self->color;
}

/**
 * gcal_event_set_color:
 * @self: a #GcalEvent
 * @color: a #GdkRGBA
 *
 * Sets the color of the event, which is passed to the
 * parent #ESource.
 */
void
gcal_event_set_color (GcalEvent *self,
                      GdkRGBA   *color)
{
  g_return_if_fail (GCAL_IS_EVENT (self));

  if (!gdk_rgba_equal (self->color, color))
    {
      g_clear_pointer (&self->color, gdk_rgba_free);
      self->color = gdk_rgba_copy (color);

      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_COLOR]);
    }
}

/**
 * gcal_event_get_component:
 * @self: a #GcalEvent
 *
 * Retrieves the internal component of the event.
 *
 * Returns: (transfer none): an #ECalComponent.
 */
ECalComponent*
gcal_event_get_component (GcalEvent *self)
{
  g_return_val_if_fail (GCAL_IS_EVENT (self), NULL);

  return self->component;
}

/**
 * gcal_event_set_all_day:
 * @self: a #GcalEvent
 * @all_day: whether the event is all day or not
 *
 * Sets the @GcalEvent::all-day property.
 */
void
gcal_event_set_all_day (GcalEvent *self,
                        gboolean   all_day)
{
  g_return_if_fail (GCAL_IS_EVENT (self));

  if (self->all_day != all_day)
    {
      self->all_day = all_day;

      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ALL_DAY]);
    }
}

/**
 * gcal_event_get_date_end:
 * @self: a #GcalEvent
 *
 * Retrieves the end date of @self. If the component doesn't
 * have an end date, then the returned date is the same of
 * gcal_event_get_date_start().
 *
 * Returns: (transfer none): a #GDateTime.
 */
GDateTime*
gcal_event_get_date_end (GcalEvent *self)
{
  g_return_val_if_fail (GCAL_IS_EVENT (self), NULL);

  /* End date can be NULL, meaning dt_end is the same as dt_start */
  return self->dt_end ? self->dt_end : self->dt_start;
}

/**
 * gcal_event_set_date_end:
 * @self: a #GcalEvent
 * @dt: a #GDateTime
 *
 * Sets the end date as @dt.
 */
void
gcal_event_set_date_end (GcalEvent *self,
                         GDateTime *dt)
{
  g_return_if_fail (GCAL_IS_EVENT (self));

  if (self->dt_end != dt)
    {
      ECalComponentDateTime *component_dt;

      g_clear_pointer (&self->dt_end, g_date_time_unref);
      self->dt_end = g_date_time_ref (dt);

      /* Setup the ECalComponent's datetime value */
      component_dt = build_component_from_datetime (self, dt);

      e_cal_component_set_dtend (self->component, component_dt);
      e_cal_component_commit_sequence (self->component);

      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DATE_END]);

      e_cal_component_free_datetime (component_dt);
      g_free (component_dt);
    }
}

/**
 * gcal_event_get_date_start:
 * @self: a #GcalEvent
 *
 * Retrieves the start date of @self.
 *
 * Returns: (transfer none): a #GDateTime.
 */
GDateTime*
gcal_event_get_date_start (GcalEvent *self)
{
  g_return_val_if_fail (GCAL_IS_EVENT (self), NULL);

  return self->dt_start;
}

/**
 * gcal_event_set_date_start:
 * @self: a #GcalEvent
 * @dt: a #GDateTime
 *
 * Sets the start date as @dt.
 */
void
gcal_event_set_date_start (GcalEvent *self,
                           GDateTime *dt)
{
  g_return_if_fail (GCAL_IS_EVENT (self));

  if (self->dt_start != dt)
    {
      ECalComponentDateTime *component_dt;

      g_clear_pointer (&self->dt_start, g_date_time_unref);
      self->dt_start = g_date_time_ref (dt);

      /* Setup the ECalComponent's datetime value */
      component_dt = build_component_from_datetime (self, dt);

      e_cal_component_set_dtstart (self->component, component_dt);
      e_cal_component_commit_sequence (self->component);

      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DATE_START]);

      e_cal_component_free_datetime (component_dt);
      g_free (component_dt);
    }
}

/**
 * gcal_event_get_description:
 * @self a #GcalEvent
 *
 * Retrieves the description of @self.
 *
 * Returns: (transfer none): the description of the event.
 */
const gchar*
gcal_event_get_description (GcalEvent *self)
{
  g_return_val_if_fail (GCAL_IS_EVENT (self), NULL);

  return self->description ? self->description : "";
}

/**
 * gcal_event_set_description:
 * @self: a #GcalEvent
 * @description: (nullable): the new description of the event
 *
 * Sets the description of the event.
 */
void
gcal_event_set_description (GcalEvent   *self,
                            const gchar *description)
{
  g_return_if_fail (GCAL_IS_EVENT (self));

  if (g_strcmp0 (self->description, description) != 0)
    {
      ECalComponentText text_component;
      GSList list;

      g_clear_pointer (&self->description, g_free);
      self->description = g_strdup (description);

      text_component.value = description;
      text_component.altrep = NULL;

      list.data = &text_component;
      list.next = NULL;

      e_cal_component_set_description_list (self->component, &list);
      e_cal_component_commit_sequence (self->component);

      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DESCRIPTION]);
    }
}

/**
 * gcal_event_has_recurrence:
 * @self: a #GcalEvent
 *
 * Returns whether the event has recurrences or not.
 *
 * Returns: %TRUE if @self has recurrences, %FALSE otherwise
 */
gboolean
gcal_event_has_recurrence (GcalEvent *self)
{
  g_return_val_if_fail (GCAL_IS_EVENT (self), FALSE);

  return e_cal_component_has_recurrences (self->component);
}

/**
 * gcal_event_has_alarms:
 * @self: a #GcalEvent
 *
 * Retrieves whether the event has alarms or not.
 *
 * Returns: %TRUE if @event has alarms, %FALSE otherwise.
 */
gboolean
gcal_event_has_alarms (GcalEvent *self)
{
  g_return_val_if_fail (GCAL_IS_EVENT (self), FALSE);

  return e_cal_component_has_alarms (self->component);
}

/**
 * gcal_event_get_alarms:
 * @self: a #GcalEvent
 *
 * Retrieves the alarms available for @self.
 *
 * Returns: (transfer full): a #GList of #ECalComponentAlarm.
 */
GList*
gcal_event_get_alarms (GcalEvent *self)
{
  GHashTable *tmp;
  GList *alarm_uids, *alarms, *l;

  g_return_val_if_fail (GCAL_IS_EVENT (self), NULL);

  tmp = g_hash_table_new (g_direct_hash, g_direct_equal);
  alarms = NULL;
  alarm_uids = e_cal_component_get_alarm_uids (self->component);

  for (l = alarm_uids; l != NULL; l = l->next)
    {
      ECalComponentAlarm *alarm;
      gint trigger_minutes;

      alarm = e_cal_component_get_alarm (self->component, l->data);
      trigger_minutes = get_alarm_trigger_minutes (self, alarm);

      /* We only support a single alarm for a given time */
      if (!g_hash_table_contains (tmp, GINT_TO_POINTER (trigger_minutes)))
        {
          alarms = g_list_prepend (alarms, alarm);

          g_hash_table_insert (tmp, GINT_TO_POINTER (trigger_minutes), NULL);
        }
      else
        {
          e_cal_component_alarm_free (alarm);
        }
    }

  cal_obj_uid_list_free (alarm_uids);
  g_hash_table_destroy (tmp);

  return alarms;
}

/**
 * gcal_event_add_alarm:
 * @self: a #GcalEvent
 * @type: the minutes before the start date to trigger the alarm
 * @has_sound: whether the alarm plays a sound or not
 *
 * Adds an alarm to @self that triggers @type minutes before the event's
 * start date.
 *
 * If there's already an alarm for @type, it'll be replaced.
 */
void
gcal_event_add_alarm (GcalEvent *self,
                      guint      type,
                      gboolean   has_sound)
{
  ECalComponentAlarmTrigger trigger;
  ECalComponentAlarmAction action;
  ECalComponentAlarm *alarm;
  gchar *alarm_uid;

  g_return_if_fail (GCAL_IS_EVENT (self));

  /* Only 1 alarm per relative time */
  if (g_hash_table_contains (self->alarms, GINT_TO_POINTER (type)))
    {
      alarm_uid = g_hash_table_lookup (self->alarms, GINT_TO_POINTER (type));

      e_cal_component_remove_alarm (self->component, alarm_uid);
    }

  /* Alarm */
  alarm = e_cal_component_alarm_new ();

  /* Setup the alarm trigger */
  trigger.type = E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_START;

  trigger.u.rel_duration = icaldurationtype_null_duration ();
  trigger.u.rel_duration.is_neg = TRUE;
  trigger.u.rel_duration.minutes = type;

  e_cal_component_alarm_set_trigger (alarm, trigger);

  /* Action */
  if (has_sound)
    action = E_CAL_COMPONENT_ALARM_AUDIO;
  else
    action = E_CAL_COMPONENT_ALARM_DISPLAY;

  e_cal_component_alarm_set_action (alarm, action);

  /* Add the alarm to the component */
  e_cal_component_add_alarm (self->component, alarm);

  /* Add to the hash table */
  alarm_uid = g_strdup (e_cal_component_alarm_get_uid (alarm));

  g_hash_table_insert (self->alarms, GINT_TO_POINTER (type), alarm_uid);

  e_cal_component_alarm_free (alarm);
}

/**
 * gcal_event_remove_alarm:
 * @self: a #GcalEvent
 * @type: the minutes before the start date to trigger the alarm
 *
 * Removes an alarm from @self that triggers @type minutes before the event's
 * start date.
 *
 * If there's no alarm set for @type, nothing happens.
 */
void
gcal_event_remove_alarm (GcalEvent *self,
                         guint      type)
{
  g_return_if_fail (GCAL_IS_EVENT (self));

  /* Only 1 alarm per relative time */
  if (g_hash_table_contains (self->alarms, GINT_TO_POINTER (type)))
    {
      const gchar *alarm_uid;

      alarm_uid = g_hash_table_lookup (self->alarms, GINT_TO_POINTER (type));

      e_cal_component_remove_alarm (self->component, alarm_uid);

      g_hash_table_remove (self->alarms, GINT_TO_POINTER (type));
    }
}


/**
 * gcal_event_get_location:
 * @self: a #GcalEvent
 *
 * Retrieves the location of the event.
 *
 * Returns: (transfer none): the location of the event
 */
const gchar*
gcal_event_get_location (GcalEvent *self)
{
  const gchar *location;

  g_return_val_if_fail (GCAL_IS_EVENT (self), NULL);

  e_cal_component_get_location (self->component, &location);

  return location ? location : "";
}

/**
 * gcal_event_set_location:
 * @self: a #GcalEvent
 * @location: (nullable): the new location of the event
 *
 * Sets the location of the event.
 */
void
gcal_event_set_location (GcalEvent   *self,
                         const gchar *location)
{
  const gchar *current_location;

  g_return_if_fail (GCAL_IS_EVENT (self));

  current_location = gcal_event_get_location (self);

  if (g_strcmp0 (current_location, location) != 0)
    {
      e_cal_component_set_location (self->component, location);

      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LOCATION]);
    }
}

/**
 * gcal_event_get_source:
 * @self: a #GcalEvent
 *
 * Retrieves the source of the event.
 *
 * Returns: (nullable): an #ESource.
 */
ESource*
gcal_event_get_source (GcalEvent *self)
{
  g_return_val_if_fail (GCAL_IS_EVENT (self), NULL);

  return self->source;
}

/**
 * gcal_event_set_source:
 * @self: a #GcalEvent
 * @source: an #ESource
 *
 * Sets the source of this event. The color of the
 * event is automatically tied with the source's
 * color.
 *
 * The source should only be set once.
 */
void
gcal_event_set_source (GcalEvent *self,
                       ESource   *source)
{
  g_return_if_fail (GCAL_IS_EVENT (self));

  if (self->source != source)
    {
      /* Remove previous binding */
      g_clear_pointer (&self->color_binding, g_binding_unbind);

      g_set_object (&self->source, source);

      if (source)
        {
          ESourceSelectable *extension;
          GdkRGBA color;

          extension = E_SOURCE_SELECTABLE (e_source_get_extension (source, E_SOURCE_EXTENSION_CALENDAR));

          /* calendar default color */
          if (!gdk_rgba_parse (&color, e_source_selectable_get_color (extension)))
            gdk_rgba_parse (&color, "#ffffff");

          gcal_event_set_color (self, &color);

          /* Bind the source's color with this event's color */
          self->color_binding = g_object_bind_property_full (extension, "color",
                                                             self, "color",
                                                             G_BINDING_DEFAULT,
                                                             string_to_color,
                                                             NULL,
                                                             self,
                                                             NULL);
        }

      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SOURCE]);

      gcal_event_update_uid_internal (self);
    }
}

/**
 * gcal_event_get_summary:
 * @self: a #GcalEvent
 *
 * Retrieves the summary of this event.
 *
 * Returns: (transfer none): the summary of the event.
 */
const gchar*
gcal_event_get_summary (GcalEvent *self)
{
  ECalComponentText summary;

  g_return_val_if_fail (GCAL_IS_EVENT (self), NULL);

  e_cal_component_get_summary (self->component, &summary);

  return summary.value ? summary.value : "";
}

/**
 * gcal_event_set_summary:
 * @event: a #GcalEvent
 * @summary: (nullable): the new summary of @event.
 *
 * Sets the summary of @event.
 */
void
gcal_event_set_summary (GcalEvent   *self,
                        const gchar *summary)
{
  const gchar *current_summary;

  g_return_if_fail (GCAL_IS_EVENT (self));

  current_summary = gcal_event_get_summary (self);

  if (g_strcmp0 (current_summary, summary) != 0)
    {
      ECalComponentText text_component;

      text_component.value = summary ? summary : "";
      text_component.altrep = NULL;

      e_cal_component_set_summary (self->component, &text_component);
      e_cal_component_commit_sequence (self->component);

      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SUMMARY]);
    }
}

/**
 * gcal_event_get_uid:
 * @self: a #GcalEvent
 *
 * Retrieves the unique identifier of the event. The UID
 * consists of '[source id]:[event id]:[recurrency id]'.
 * If there's no recurrency assigned, the last field is
 * ignored.
 *
 * Returns: (transfer none): the unique identifier of the event
 */
const gchar*
gcal_event_get_uid (GcalEvent *self)
{
  g_return_val_if_fail (GCAL_IS_EVENT (self), NULL);

  return self->uid;
}

/**
 * gcal_event_is_multiday:
 * @self: a #GcalEvent
 *
 * Whether the event visibly spans more than one day in the
 * calendar.
 *
 * This function can return different values for the same event
 * depending on your timezone. For example, an event that spans
 * [ 02:00, 22:00 ) +0000 would assume the following values:
 *
 * |[
 *             D-1            D            D+1
 * -0600 [         ---][-------     ][            ] (multiday)
 * +0000 [            ][ ---------- ][            ] (single day)
 * +0800 [            ][     -------][---         ] (multiday)
 * ]|
 *
 * Returns: %TRUE if @self spans more than 1 day, %FALSE otherwise.
 */
gboolean
gcal_event_is_multiday (GcalEvent *self)
{
  g_autoptr (GDateTime) inclusive_end_date = NULL;
  g_autoptr (GDateTime) local_start_date = NULL;
  g_autoptr (GDateTime) local_end_date = NULL;
  GDate start_dt;
  GDate end_dt;

  g_return_val_if_fail (GCAL_IS_EVENT (self), FALSE);

  inclusive_end_date = g_date_time_add_seconds (gcal_event_get_date_end (self), -1);
  local_start_date = g_date_time_to_local (self->dt_start);
  local_end_date = g_date_time_to_local (inclusive_end_date);

  g_date_clear (&start_dt, 1);
  g_date_set_dmy (&start_dt,
                  g_date_time_get_day_of_month (local_start_date),
                  g_date_time_get_month (local_start_date),
                  g_date_time_get_year (local_start_date));

  g_date_clear (&end_dt, 1);
  g_date_set_dmy (&end_dt,
                  g_date_time_get_day_of_month (local_end_date),
                  g_date_time_get_month (local_end_date),
                  g_date_time_get_year (local_end_date));

  return g_date_days_between (&start_dt, &end_dt) > 0;
}

/**
 * gcal_event_compare:
 * @event1: a #GcalEvent
 * @event2: a #GcalEvent
 *
 * Compare @event1 and @event2. It compares the start dates of
 * the events and, when they have the same date (time is ignored),
 * the @GcalEvent::all-day is the tiebreaker criteria.
 *
 * Events with equal start dates are sorted by the length.
 *
 * Returns: 1, 0 or -1 if @event1 is, respectively, lower, equal or
 * greater than @event2.
 */
gint
gcal_event_compare (GcalEvent *event1,
                    GcalEvent *event2)
{
  if (!event1 && !event2)
    return 0;
  if (!event1)
    return 1;
  else if (!event2)
    return -1;

  /*
   * When the start date is the same, the priority is:
   *  1. Whether the event is all day or not
   *  2. The start date
   *  3. The event duration
   */
  if (event1->all_day != event2->all_day)
    {
      return event2->all_day - event1->all_day;
    }
  else
    {
      gint start_date_diff;

      start_date_diff = g_date_time_compare (event1->dt_start, event2->dt_start);

      if (start_date_diff != 0)
        {
          return start_date_diff;
        }
      else
        {
          GTimeSpan span1, span2;

          span1 = g_date_time_difference (event1->dt_start, gcal_event_get_date_end (event1));
          span2 = g_date_time_difference (event2->dt_start, gcal_event_get_date_end (event2));

          return span2 - span1;
        }
    }
}

/**
 * gcal_event_compare_with_current:
 * @event1: a #GcalEvent
 * @event2: a #GcalEvent
 * @current_time: the current time
 *
 * Compares @event1 and @event2 related to @current_time; the closest
 * an event is from @current_time, the more priority it has.
 *
 * Returns: -1, 0 and 1 if @event1 is, respectively, farther, equal or
 * closer from @current_time relative to @event2.
 */
gint
gcal_event_compare_with_current (GcalEvent *event1,
                                 GcalEvent *event2,
                                 time_t    *current_time)
{
  time_t time1, time2;
  time_t diff1, diff2;

  if (!event1 && !event2)
    return 0;
  if (!event1)
    return 1;
  else if (!event2)
    return -1;

  time1 = g_date_time_to_unix (event1->dt_start);
  time2 = g_date_time_to_unix (event2->dt_start);
  diff1 = time1 - *current_time;
  diff2 = time2 - *current_time;

  if (diff1 != diff2)
    {
      if (diff1 == 0)
        return -1;
      else if (diff2 == 0)
        return 1;

      if (diff1 > 0 && diff2 < 0)
        return -1;
      else if (diff2 > 0 && diff1 < 0)
        return 1;
      else if (diff1 < 0 && diff2 < 0)
        return ABS (diff1) - ABS (diff2);
      else if (diff1 > 0 && diff2 > 0)
        return diff1 - diff2;
    }

  return 0;
}

/**
 * gcal_event_set_recurrence:
 * @self: a #GcalEvent
 * @recur: (nullable): a #GcalRecurrence
 *
 * Sets the recurrence struct of the event to @recur
 * and adds the corresponding rrule to the event.
 */
void
gcal_event_set_recurrence (GcalEvent      *self,
                           GcalRecurrence *recur)
{
  ECalComponent *comp;
  icalcomponent *icalcomp;
  icalproperty *prop;
  struct icalrecurrencetype *rrule;

  g_return_if_fail (GCAL_IS_EVENT (self));

  rrule = gcal_recurrence_to_rrule (recur);

  comp = gcal_event_get_component (self);
  icalcomp = e_cal_component_get_icalcomponent (comp);

  g_clear_pointer (&self->recurrence, gcal_recurrence_unref);
  self->recurrence = gcal_recurrence_copy (recur);

  prop = icalcomponent_get_first_property (icalcomp, ICAL_RRULE_PROPERTY);

  if (prop)
    {
      icalproperty_set_rrule (prop, *rrule);
    }
  else
    {
      prop = icalproperty_new_rrule (*rrule);
      icalcomponent_add_property (icalcomp, prop);
    }

  /* Rescan the component for the changes to be detected and registered */
  e_cal_component_rescan (comp);
}

/**
 * gcal_event_get_recurrence:
 * @self: a #GcalEvent
 *
 * Gets the recurrence struct @recur of the event.
 *
 * Returns: (transfer full): a #GcalRecurrence
 */
GcalRecurrence*
gcal_event_get_recurrence (GcalEvent *self)
{
  g_return_val_if_fail (GCAL_IS_EVENT (self), NULL);

  return self->recurrence;
}

/**
 * gcal_event_get_original_timezones:
 * @self: a #GcalEvent
 * @out_start_timezone: (direction out)(nullable): the return location for the
 * previously stored start date timezone
 * @out_end_timezone: (direction out)(nullable): the return location for the
 * previously stored end date timezone
 *
 * Retrieves the start and end date timezones previously stored by
 * gcal_event_save_original_timezones().
 *
 * If gcal_event_save_original_timezones() wasn't called before,
 * it will use local timezones instead.
 */
void
gcal_event_get_original_timezones (GcalEvent  *self,
                                   GTimeZone **out_start_timezone,
                                   GTimeZone **out_end_timezone)
{
  g_autoptr (GTimeZone) original_start_tz = NULL;
  g_autoptr (GTimeZone) original_end_tz = NULL;
  icalcomponent *ical_comp;
  icalproperty *property;

  g_return_if_fail (GCAL_IS_EVENT (self));
  g_assert (self->component != NULL);

  ical_comp = e_cal_component_get_icalcomponent (self->component);

  for (property = icalcomponent_get_first_property (ical_comp, ICAL_X_PROPERTY);
       property;
       property = icalcomponent_get_next_property (ical_comp, ICAL_X_PROPERTY))
    {
      const gchar *value;

      if (original_start_tz && original_end_tz)
        break;

      if (g_strcmp0 (icalproperty_get_x_name (property), "X-GNOME-CALENDAR-ORIGINAL-TZ-START") == 0)
        {
          value = icalproperty_get_x (property);
          original_start_tz = g_time_zone_new (value);

          GCAL_TRACE_MSG ("Found X-GNOME-CALENDAR-ORIGINAL-TZ-START=%s", value);
        }
      else if (g_strcmp0 (icalproperty_get_x_name (property), "X-GNOME-CALENDAR-ORIGINAL-TZ-END") == 0)
        {
          value = icalproperty_get_x (property);
          original_end_tz = g_time_zone_new (value);

          GCAL_TRACE_MSG ("Found X-GNOME-CALENDAR-ORIGINAL-TZ-END=%s", value);
        }
    }

  if (!original_start_tz)
    original_start_tz = g_time_zone_new_local ();

  if (!original_end_tz)
    original_end_tz = g_time_zone_new_local ();

  g_assert (original_start_tz != NULL);
  g_assert (original_end_tz != NULL);

  if (out_start_timezone)
    *out_start_timezone = g_steal_pointer (&original_start_tz);

  if (out_end_timezone)
    *out_end_timezone = g_steal_pointer (&original_end_tz);
}

/**
 * gcal_event_save_original_timezones:
 * @self: a #GcalEvent
 *
 * Stores the current timezones of the start and end dates of @self
 * into separate, GNOME Calendar specific fields. These fields can
 * be used by gcal_event_get_original_timezones() to retrieve the
 * previous timezones of the event later.
 */
void
gcal_event_save_original_timezones (GcalEvent *self)
{
  icalcomponent *ical_comp;
  icalproperty *property;
  GTimeZone *tz;
  gboolean has_original_start_tz;
  gboolean has_original_end_tz;

  GCAL_ENTRY;

  g_return_if_fail (GCAL_IS_EVENT (self));
  g_assert (self->component != NULL);

  has_original_start_tz = FALSE;
  has_original_end_tz = FALSE;
  ical_comp = e_cal_component_get_icalcomponent (self->component);

  for (property = icalcomponent_get_first_property (ical_comp, ICAL_X_PROPERTY);
       property;
       property = icalcomponent_get_next_property (ical_comp, ICAL_X_PROPERTY))
    {
      if (g_strcmp0 (icalproperty_get_x_name (property), "X-GNOME-CALENDAR-ORIGINAL-TZ-START") == 0)
        {
          tz = g_date_time_get_timezone (self->dt_start);
          icalproperty_set_x (property, g_time_zone_get_identifier (tz));

          GCAL_TRACE_MSG ("Reusing X-GNOME-CALENDAR-ORIGINAL-TZ-START property with %s", icalproperty_get_x (property));

          has_original_start_tz = TRUE;
        }
      else if (g_strcmp0 (icalproperty_get_x_name (property), "X-GNOME-CALENDAR-ORIGINAL-TZ-END") == 0)
        {
          tz = g_date_time_get_timezone (self->dt_end);
          icalproperty_set_x (property, g_time_zone_get_identifier (tz));

          GCAL_TRACE_MSG ("Reusing X-GNOME-CALENDAR-ORIGINAL-TZ-END property with %s", icalproperty_get_x (property));

          has_original_end_tz = TRUE;
        }
    }

  if (!has_original_start_tz)
    {
      tz = g_date_time_get_timezone (self->dt_start);
      property = icalproperty_new_x (g_time_zone_get_identifier (tz));
      icalproperty_set_x_name (property, "X-GNOME-CALENDAR-ORIGINAL-TZ-START");

      icalcomponent_add_property (ical_comp, property);

      GCAL_TRACE_MSG ("Added new X-GNOME-CALENDAR-ORIGINAL-TZ-START property with %s", icalproperty_get_x (property));
    }

  if (!has_original_end_tz)
    {
      tz = g_date_time_get_timezone (self->dt_end);
      property = icalproperty_new_x (g_time_zone_get_identifier (tz));
      icalproperty_set_x_name (property, "X-GNOME-CALENDAR-ORIGINAL-TZ-END");

      icalcomponent_add_property (ical_comp, property);

      GCAL_TRACE_MSG ("Added new X-GNOME-CALENDAR-ORIGINAL-TZ-END property with %s", icalproperty_get_x (property));
    }

  GCAL_EXIT;
}
