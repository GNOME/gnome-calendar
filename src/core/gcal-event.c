/* gcal-event.c
 *
 * Copyright (C) 2016-2020 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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
#include "gcal-application.h"
#include "gcal-context.h"
#include "gcal-debug.h"
#include "gcal-event.h"
#include "gcal-utils.h"
#include "gcal-recurrence.h"

#include <glib/gi18n.h>

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
 * event = gcal_event_new (calendar, component, &error);
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

  /* These are cached, because ECalComponent returns newly allocated data for them */
  gchar              *summary;
  gchar              *location;

  /*
   * The description is cached in the class because it
   * may come as a GSList of descriptions, in which
   * case we merge them and cache it here.
   */
  gchar              *description;

  GDateTime          *dt_start;
  GDateTime          *dt_end;
  GcalRange          *range;

  GdkRGBA            *color;
  GBinding           *color_binding;

  gboolean            all_day;

  /* A map of GcalAlarmType */
  GHashTable         *alarms;

  ECalComponent      *component;
  GcalCalendar       *calendar;

  GcalRecurrence     *recurrence;
};

static void          gcal_event_initable_iface_init              (GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (GcalEvent, gcal_event, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, gcal_event_initable_iface_init))

G_DEFINE_QUARK (GcalEvent, gcal_event_error);

enum {
  PROP_0,
  PROP_ALL_DAY,
  PROP_CALENDAR,
  PROP_COLOR,
  PROP_COMPONENT,
  PROP_DESCRIPTION,
  PROP_DATE_END,
  PROP_DATE_START,
  PROP_LOCATION,
  PROP_RANGE,
  PROP_SUMMARY,
  PROP_TIMEZONE,
  PROP_UID,
  PROP_HAS_RECURRENCE,
  PROP_RECURRENCE,
  N_PROPS
};

static GParamSpec* properties[N_PROPS] = { NULL, };

/*
 * Auxiliary methods
 */

static void
clear_range (GcalEvent *self)
{
  g_clear_pointer (&self->range, gcal_range_unref);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_RANGE]);
}

static GTimeZone*
get_timezone_from_ical (GcalEvent             *self,
                        ECalComponentDateTime *comp)
{
  g_autoptr (GTimeZone) tz = NULL;
  ICalTimezone *zone;
  ICalTime *itt;

  itt = e_cal_component_datetime_get_value (comp);
  zone = i_cal_time_get_timezone (itt);

  if (i_cal_time_is_date (itt))
    {
      /*
       * When loading an all day event, the timezone must not be taken into account
       * when loading the date. Since all events in ical are UTC, we match it by
       * setting the timezone as UTC as well.
       */
      tz = g_time_zone_new_utc ();
    }
  else if (e_cal_component_datetime_get_tzid (comp))
    {
      const gchar *original_tzid, *tzid;

      tzid = e_cal_component_datetime_get_tzid (comp);
      original_tzid = tzid;

      if (g_str_has_prefix (tzid, LIBICAL_TZID_PREFIX))
        tzid += strlen (LIBICAL_TZID_PREFIX);

      tz = g_time_zone_new_identifier (tzid);

      if (!tz && self->calendar)
        {
          ICalTimezone *tzone = NULL;
          ECalClient *client;

          client = gcal_calendar_get_client (self->calendar);

          if (client && e_cal_client_get_timezone_sync (client, original_tzid, &tzone, NULL, NULL))
            zone = tzone;
        }
    }

  if (!tz && zone)
    {
      g_autofree gchar *tzid = NULL;
      gint offset;
      gint is_daylight = 0;

      /* libical-glib prior to 3.0.12 fails if no return location for is_daylight is passed */
      offset = i_cal_timezone_get_utc_offset (zone, itt, &is_daylight);
      tzid = format_utc_offset (offset);
      tz = g_time_zone_new_identifier (tzid);
    }

  /*
   * If tz is NULL, the timezone identifier is invalid. Fallback to the local timezone
   * in this case.
   */
  if (!tz)
    tz = g_time_zone_new_local ();

  GCAL_TRACE_MSG ("%s (%p)", g_time_zone_get_identifier (tz), tz);

  return g_steal_pointer (&tz);
}

static ECalComponentDateTime*
build_component_from_datetime (GcalEvent *self,
                               GDateTime *dt)
{
  ICalTime *itt = NULL;
  gchar *tzid = NULL;

  if (!dt)
    return NULL;

  itt = gcal_date_time_to_icaltime (dt);

  if (self->all_day)
    {
      i_cal_time_set_timezone (itt, i_cal_timezone_get_utc_timezone ());
      tzid = g_strdup ("UTC");
    }
  else
    {
      g_autoptr (GTimeZone) zone = NULL;
      ICalTimezone *tz;

      zone = g_date_time_get_timezone (dt);
      tz = gcal_timezone_to_icaltimezone (zone);
      i_cal_time_set_timezone (itt, tz);
      tzid = g_strdup (i_cal_timezone_get_tzid (tz));
    }

  /* Call it after setting the timezone, because the DATE values do not let set the timezone */
  i_cal_time_set_is_date (itt, self->all_day);

  return e_cal_component_datetime_new_take (itt, tzid);
}

static void
gcal_event_update_uid_internal (GcalEvent *self)
{
  ECalComponentId *id;
  const gchar *source_id;

  /* Setup event uid */
  source_id = self->calendar ? gcal_calendar_get_id (self->calendar) : "";
  id = e_cal_component_get_id (self->component);

  if (e_cal_component_id_get_rid (id) != NULL)
    {
      self->uid = g_strdup_printf ("%s:%s:%s",
                                   source_id,
                                   e_cal_component_id_get_uid (id),
                                   e_cal_component_id_get_rid (id));
    }
  else
    {
      self->uid = g_strdup_printf ("%s:%s",
                                   source_id,
                                   e_cal_component_id_get_uid (id));
    }

  e_cal_component_id_free (id);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_UID]);
}

static void
load_alarms (GcalEvent *self)
{
  GSList *alarm_uids, *l;

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

  g_slist_free_full (alarm_uids, g_free);
}

static gboolean
setup_component (GcalEvent  *self,
                 GError    **error)
{
  g_autoptr (GTimeZone) zone_start = NULL;
  g_autoptr (GTimeZone) zone_end = NULL;
  ECalComponentDateTime *start;
  ECalComponentDateTime *end;
  ECalComponentText *text;
  ICalTime *date;
  GDateTime *date_start;
  GDateTime *date_end;
  gboolean start_is_all_day, end_is_all_day;
  gchar *description, *location;

  g_assert (self->component != NULL);

  /* Setup start date */
  start = e_cal_component_get_dtstart (self->component);

  /*
   * A NULL start date is invalid. We set something bogus to proceed, and make
   * it set a GError and return NULL.
   */
  if (!start || !e_cal_component_datetime_get_value (start))
    {
      g_set_error (error,
                   GCAL_EVENT_ERROR,
                   GCAL_EVENT_ERROR_INVALID_START_DATE,
                   "Event '%s' has an invalid start date", gcal_event_get_uid (self));

      e_cal_component_datetime_free (start);
      start = e_cal_component_datetime_new_take (i_cal_time_new_today (), NULL);
      g_clear_pointer (&start, e_cal_component_datetime_free);
      return FALSE;
    }

  GCAL_TRACE_MSG ("Retrieving start timezone");

  date = i_cal_time_normalize (e_cal_component_datetime_get_value (start));
  zone_start = get_timezone_from_ical (self, start);
  date_start = g_date_time_new (zone_start,
                                i_cal_time_get_year (date),
                                i_cal_time_get_month (date),
                                i_cal_time_get_day (date),
                                i_cal_time_is_date (date) ? 0 : i_cal_time_get_hour (date),
                                i_cal_time_is_date (date) ? 0 : i_cal_time_get_minute (date),
                                i_cal_time_is_date (date) ? 0 : i_cal_time_get_second (date));
  start_is_all_day = gcal_date_time_is_date (date_start);

  self->dt_start = date_start;

  g_clear_object (&date);

  /* Setup end date */
  end = e_cal_component_get_dtend (self->component);

  if (!end || !e_cal_component_datetime_get_value (end))
    {
      self->all_day = TRUE;
      self->dt_end = g_date_time_add_days (self->dt_start, 1);
    }
  else
    {
      GCAL_TRACE_MSG ("Retrieving end timezone");

      date = i_cal_time_normalize (e_cal_component_datetime_get_value (end));
      zone_end = get_timezone_from_ical (self, end);
      date_end = g_date_time_new (zone_end,
                                  i_cal_time_get_year (date),
                                  i_cal_time_get_month (date),
                                  i_cal_time_get_day (date),
                                  i_cal_time_is_date (date) ? 0 : i_cal_time_get_hour (date),
                                  i_cal_time_is_date (date) ? 0 : i_cal_time_get_minute (date),
                                  i_cal_time_is_date (date) ? 0 : i_cal_time_get_second (date));
      end_is_all_day = gcal_date_time_is_date (date_end);

      self->dt_end = g_date_time_ref (date_end);

      /* Setup all day */
      self->all_day = start_is_all_day && end_is_all_day;

      g_clear_object (&date);
    }

  /* Summary */
  text = e_cal_component_get_summary (self->component);
  if (text && e_cal_component_text_get_value (text))
    gcal_event_set_summary (self, e_cal_component_text_get_value (text));
  else
    gcal_event_set_summary (self, "");

  /* Location */
  location = e_cal_component_get_location (self->component);
  gcal_event_set_location (self, location ? location : "");

  /* Setup description */
  description = get_desc_from_component (self->component, "\n\n");
  gcal_event_set_description (self, description);

  /* Setup UID */
  gcal_event_update_uid_internal (self);

  /* Set has-recurrence to check if the component has recurrence or not */
  self->has_recurrence = e_cal_component_has_recurrences (self->component);

  /* Load the recurrence-rules in GcalRecurrence struct */
  self->recurrence = gcal_recurrence_parse_recurrence_rules (self->component);

  /* Load and setup the alarms */
  load_alarms (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_HAS_RECURRENCE]);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_RECURRENCE]);

  g_clear_pointer (&description, g_free);
  g_clear_pointer (&location, g_free);

  e_cal_component_text_free (text);
  e_cal_component_datetime_free (start);
  e_cal_component_datetime_free (end);

  return TRUE;
}


static void
gcal_event_set_component_internal (GcalEvent     *self,
                                   ECalComponent *component)
{
  g_assert (self->component == NULL);
  self->component = e_cal_component_clone (component);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_COMPONENT]);
}

/*
 * GInitable iface implementation
 */

static gboolean
gcal_event_initable_init (GInitable     *initable,
                          GCancellable  *cancellable,
                          GError       **error)
{
  GcalEvent *self = GCAL_EVENT (initable);

  return setup_component (self, error);
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

  g_clear_pointer (&self->dt_start, g_date_time_unref);
  g_clear_pointer (&self->dt_end, g_date_time_unref);
  g_clear_pointer (&self->range, gcal_range_unref);
  g_clear_pointer (&self->summary, g_free);
  g_clear_pointer (&self->location, g_free);
  g_clear_pointer (&self->description, g_free);
  g_clear_pointer (&self->alarms, g_hash_table_unref);
  g_clear_pointer (&self->uid, g_free);
  g_clear_pointer (&self->color, gdk_rgba_free);
  g_clear_object (&self->component);
  g_clear_object (&self->calendar);
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

    case PROP_CALENDAR:
      g_value_set_object (value, self->calendar);
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

    case PROP_RANGE:
      g_value_take_boxed (value, gcal_event_get_range (self));
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

    case PROP_CALENDAR:
      gcal_event_set_calendar (self, g_value_get_object (value));
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
  * GcalEvent::range:
  *
  * The range of the event, as a combination of the start and end dates.
  */
  properties[PROP_RANGE] = g_param_spec_boxed ("range",
                                               "Range of the event",
                                               "The time range of the event",
                                               GCAL_TYPE_RANGE,
                                               G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

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
   * GcalEvent::calendar:
   *
   * The #GcalCalendar this event belongs to.
   */
  properties[PROP_CALENDAR] = g_param_spec_object ("calendar",
                                                   "Calendar",
                                                   "The calendar this event belongs to",
                                                   GCAL_TYPE_CALENDAR,
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
}

/**
 * gcal_event_new:
 * @calendar: (nullable): a #GcalCalendar
 * @component: a #ECalComponent
 * @error: (nullable): return location for a #GError
 *
 * Creates a new event which belongs to @calendar and
 * is represented by @component. New events will have
 * a %NULL @calendar.
 *
 * Returns: (transfer full)(nullable): a #GcalEvent
 */
GcalEvent*
gcal_event_new (GcalCalendar   *calendar,
                ECalComponent  *component,
                GError        **error)
{
  return g_initable_new (GCAL_TYPE_EVENT,
                         NULL,
                         error,
                         "calendar", calendar,
                         "component", component,
                         NULL);
}

/**
 * gcal_event_new:
 * @self: a #GcalEvent
 *
 * Clones @event into a new #GcalEvent instance. This is useful
 * for updating events.
 *
 * Returns: (transfer full)(nullable): a #GcalEvent
 */
GcalEvent*
gcal_event_new_from_event (GcalEvent *self)
{
  g_autoptr (ECalComponent) component = NULL;

  g_return_val_if_fail (GCAL_IS_EVENT (self), NULL);

  component = e_cal_component_clone (self->component);
  e_cal_component_commit_sequence (component);

  return gcal_event_new (self->calendar, component, NULL);
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

      clear_range (self);
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

  if (gcal_set_date_time (&self->dt_end, dt))
    {
      ECalComponentDateTime *component_dt;

      /* Setup the ECalComponent's datetime value */
      component_dt = build_component_from_datetime (self, dt);

      e_cal_component_set_dtend (self->component, component_dt);
      e_cal_component_commit_sequence (self->component);

      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DATE_END]);

      clear_range (self);
      e_cal_component_datetime_free (component_dt);
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

  if (gcal_set_date_time (&self->dt_start, dt))
    {
      ECalComponentDateTime *component_dt;

      /* Setup the ECalComponent's datetime value */
      component_dt = build_component_from_datetime (self, dt);

      e_cal_component_set_dtstart (self->component, component_dt);
      e_cal_component_commit_sequence (self->component);

      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DATE_START]);

      clear_range (self);
      e_cal_component_datetime_free (component_dt);
    }
}

/**
 * gcal_event_get_range:
 * @self: a #GcalEvent
 *
 * Retrieves the range of @self. This is a simple combination of
 * the start and end dates.
 *
 * Returns: (transfer none): a #GcalRange
 */
GcalRange*
gcal_event_get_range (GcalEvent *self)
{
  g_return_val_if_fail (GCAL_IS_EVENT (self), NULL);

  if (!self->range)
    {
      self->range = gcal_range_new (gcal_event_get_date_start (self),
                                    gcal_event_get_date_end (self),
                                    self->all_day ? GCAL_RANGE_DATE_ONLY : GCAL_RANGE_DEFAULT);
    }

  return self->range;
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

  if (description && !*description)
    description = NULL;

  if (g_strcmp0 (self->description, description) != 0)
    {
      g_clear_pointer (&self->description, g_free);
      self->description = g_strdup (description);

      if (description)
        {
          ECalComponentText* text_component;
          GSList list;

          text_component = e_cal_component_text_new (description, NULL);

          list.data = text_component;
          list.next = NULL;

          e_cal_component_set_descriptions (self->component, &list);
          e_cal_component_text_free (text_component);
        }
      else
        {
          e_cal_component_set_descriptions (self->component, NULL);
        }

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
  GSList *alarm_uids, *l;
  GList *alarms;

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

  g_slist_free_full (alarm_uids, g_free);
  g_hash_table_destroy (tmp);

  return alarms;
}

/**
 * gcal_event_remove_all_alarms:
 * @self: a #GcalEvent
 *
 * Removes all alarms from @self.
 */
void
gcal_event_remove_all_alarms (GcalEvent *self)
{
  GHashTableIter iter;
  const gchar *alarm_uid;
  gint minutes;

  GCAL_ENTRY;

  g_return_if_fail (GCAL_IS_EVENT (self));

  g_hash_table_iter_init (&iter, self->alarms);
  while (g_hash_table_iter_next (&iter, (gpointer*) &minutes, (gpointer*) &alarm_uid))
    {
      GCAL_TRACE_MSG ("Removing alarm %s from event %s", alarm_uid, gcal_event_get_uid (self));

      e_cal_component_remove_alarm (self->component, alarm_uid);
      g_hash_table_iter_remove (&iter);
    }

  e_cal_component_commit_sequence (self->component);

  GCAL_EXIT;
}

/**
 * gcal_event_add_alarm:
 * @self: a #GcalEvent
 * @alarm: a #ECalComponentAlarm
 *
 * Adds @alarm to @self. If there's already an alarm scheduled for the
 * same time, it'll be replaced.
 */
void
gcal_event_add_alarm (GcalEvent          *self,
                      ECalComponentAlarm *alarm)
{
  ECalComponentAlarm *new_alarm;
  gchar *alarm_uid;
  guint minutes;

  GCAL_ENTRY;

  g_return_if_fail (GCAL_IS_EVENT (self));

  new_alarm = e_cal_component_alarm_copy (alarm);
  minutes = get_alarm_trigger_minutes (self, alarm);

  /* Only 1 alarm per relative time */
  if (g_hash_table_contains (self->alarms, GINT_TO_POINTER (minutes)))
    {
      alarm_uid = g_hash_table_lookup (self->alarms, GINT_TO_POINTER (minutes));
      e_cal_component_remove_alarm (self->component, alarm_uid);
    }

  /* Add the alarm to the component */
  e_cal_component_add_alarm (self->component, new_alarm);

  /* Add to the hash table */
  alarm_uid = g_strdup (e_cal_component_alarm_get_uid (new_alarm));
  g_hash_table_insert (self->alarms, GINT_TO_POINTER (minutes), alarm_uid);

  GCAL_TRACE_MSG ("Added alarm %s in event %s", alarm_uid, gcal_event_get_uid (self));

  e_cal_component_alarm_free (new_alarm);

  e_cal_component_commit_sequence (self->component);

  GCAL_EXIT;
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
  const gchar *alarm_uid;

  g_return_if_fail (GCAL_IS_EVENT (self));

  alarm_uid = g_hash_table_lookup (self->alarms, GINT_TO_POINTER (type));

  /* Only 1 alarm per relative time */
  if (alarm_uid)
    {
      e_cal_component_remove_alarm (self->component, alarm_uid);

      g_hash_table_remove (self->alarms, GINT_TO_POINTER (type));

      e_cal_component_commit_sequence (self->component);
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
  g_return_val_if_fail (GCAL_IS_EVENT (self), NULL);

  return self->location;
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
      e_cal_component_set_location (self->component, (location && *location) ? location : NULL);
      e_cal_component_commit_sequence (self->component);

      g_clear_pointer (&self->location, g_free);
      self->location = g_strdup (location ? location : "");

      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LOCATION]);
    }
}

/**
 * gcal_event_get_calendar:
 * @self: a #GcalEvent
 *
 * Retrieves the source of the event.
 *
 * Returns: (nullable): a #GcalCalendar.
 */
GcalCalendar*
gcal_event_get_calendar (GcalEvent *self)
{
  g_return_val_if_fail (GCAL_IS_EVENT (self), NULL);

  return self->calendar;
}

/**
 * gcal_event_set_calendar:
 * @self: a #GcalEvent
 * @calendar: a #GcalCalendar
 *
 * Sets the calendar of this event. The color of the
 * event is automatically tied with the source's
 * color.
 *
 * The source should only be set once.
 */
void
gcal_event_set_calendar (GcalEvent    *self,
                         GcalCalendar *calendar)
{
  g_return_if_fail (GCAL_IS_EVENT (self));

  if (g_set_object (&self->calendar, calendar))
    {
      /* Remove previous binding */
      g_clear_pointer (&self->color_binding, g_binding_unbind);

      if (calendar)
        {
          self->color_binding = g_object_bind_property (calendar,
                                                        "color",
                                                        self,
                                                        "color",
                                                        G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
        }

      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CALENDAR]);

      if (self->component != NULL)
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
  g_return_val_if_fail (GCAL_IS_EVENT (self), NULL);

  return self->summary;
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
      ECalComponentText *text_component;

      text_component = e_cal_component_text_new (summary ? summary : "", NULL);

      e_cal_component_set_summary (self->component, text_component);
      e_cal_component_commit_sequence (self->component);

      e_cal_component_text_free (text_component);

      g_clear_pointer (&self->summary, g_free);
      self->summary = g_strdup (summary ? summary : "");

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
  g_autoptr (GDateTime) start_date = NULL;
  g_autoptr (GDateTime) end_date = NULL;
  GDate start_dt;
  GDate end_dt;
  gint n_days;

  g_return_val_if_fail (GCAL_IS_EVENT (self), FALSE);

  if (self->all_day)
    {
      start_date = g_date_time_ref (self->dt_start);
      end_date = g_date_time_ref (gcal_event_get_date_end (self));
      n_days = 1;
    }
  else
    {
      g_autoptr (GDateTime) inclusive_end_date = NULL;

      inclusive_end_date = g_date_time_add_seconds (gcal_event_get_date_end (self), -1);
      start_date = g_date_time_to_local (self->dt_start);
      end_date = g_date_time_to_local (inclusive_end_date);
      n_days = 0;
    }

  g_date_clear (&start_dt, 1);
  g_date_set_dmy (&start_dt,
                  g_date_time_get_day_of_month (start_date),
                  g_date_time_get_month (start_date),
                  g_date_time_get_year (start_date));

  g_date_clear (&end_dt, 1);
  g_date_set_dmy (&end_dt,
                  g_date_time_get_day_of_month (end_date),
                  g_date_time_get_month (end_date),
                  g_date_time_get_year (end_date));

  return g_date_days_between (&start_dt, &end_dt) > n_days;
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
                                 time_t     current_time)
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
  diff1 = time1 - current_time;
  diff2 = time2 - current_time;

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
  ICalComponent *icalcomp;
  ICalProperty *prop;
  ICalRecurrence *rrule;

  g_return_if_fail (GCAL_IS_EVENT (self));

  rrule = gcal_recurrence_to_rrule (recur);

  comp = gcal_event_get_component (self);
  icalcomp = e_cal_component_get_icalcomponent (comp);

  g_clear_pointer (&self->recurrence, gcal_recurrence_unref);
  self->recurrence = gcal_recurrence_copy (recur);

  prop = i_cal_component_get_first_property (icalcomp, I_CAL_RRULE_PROPERTY);

  if (prop)
    {
      i_cal_property_set_rrule (prop, rrule);
    }
  else
    {
      prop = i_cal_property_new_rrule (rrule);
      i_cal_component_add_property (icalcomp, prop);
    }

  e_cal_component_commit_sequence (self->component);

  g_clear_object (&rrule);
  g_clear_object (&prop);
}

/**
 * gcal_event_get_recurrence:
 * @self: a #GcalEvent
 *
 * Gets the recurrence struct @recur of the event.
 *
 * Returns: (transfer none): a #GcalRecurrence
 */
GcalRecurrence*
gcal_event_get_recurrence (GcalEvent *self)
{
  g_return_val_if_fail (GCAL_IS_EVENT (self), NULL);

  return self->recurrence;
}

/**
 * gcal_event_format_date:
 * @self: a #GcalEvent
 *
 * Formats the start and end dates of @self into a human-readable
 * string. The final string depends on whether the event is single
 * or multiday, and timed or all-day.
 *
 * Returns: (transfer full): a string
 */
gchar*
gcal_event_format_date (GcalEvent *self)
{
  g_autofree gchar *formatted_string = NULL;
  g_autofree gchar *start_date = NULL;
  g_autofree gchar *start_time = NULL;
  g_autofree gchar *end_date = NULL;
  g_autofree gchar *end_time = NULL;
  GcalApplication *application;
  GcalTimeFormat time_format;
  GcalContext *context;
  GDateTime *date_start;
  GDateTime *date_end;
  const gchar *hour_format;
  gboolean is_multiday;
  gboolean is_all_day;

  g_return_val_if_fail (GCAL_IS_EVENT (self), NULL);

  /* XXX: nasty... */
  application = GCAL_APPLICATION (g_application_get_default ());
  context = gcal_application_get_context (application);
  time_format = gcal_context_get_time_format (context);

  if (time_format == GCAL_TIME_FORMAT_24H)
    hour_format = "%R";
  else
    hour_format = "%I:%M %P";

  date_start = gcal_event_get_date_start (self);
  date_end = gcal_event_get_date_end (self);
  is_multiday = gcal_event_is_multiday (self);
  is_all_day = gcal_event_get_all_day (self);

  start_date = g_date_time_format (date_start, "%x");
  start_time = g_date_time_format (date_start, hour_format);
  end_date = g_date_time_format (date_end, "%x");
  end_time = g_date_time_format (date_end, hour_format);

  if (is_multiday)
    {
      if (is_all_day)
        {
          /* Translators: %1$s is the start date and %2$s is the end date. */
          formatted_string = g_strdup_printf (_("%1$s — %2$s"), start_date, end_date);
        }
      else
        {
          /*
           * Translators: %1$s is the start date, %2$s is the start time,
           * %3$s is the end date, and %4$s is the end time.
           */
          formatted_string = g_strdup_printf (_("%1$s %2$s — %3$s %4$s"),
                                              start_date,
                                              start_time,
                                              end_date,
                                              end_time);
        }
    }
  else
    {
      if (is_all_day)
        {
          formatted_string = g_steal_pointer (&start_date);
        }
      else
        {
          /* Translators: %1$s is a date, %2$s is the start hour, and %3$s is the end hour */
          formatted_string = g_strdup_printf (_("%1$s, %2$s – %3$s"), start_date, start_time, end_time);
        }
    }

  return g_steal_pointer (&formatted_string);
}

/**
 * gcal_event_overlaps:
 * @self: a #GcalEvent
 * @range: a #GcalRange
 *
 * Returns: True if the event range overlaps the @range
 */
gboolean
gcal_event_overlaps (GcalEvent *self,
                     GcalRange *range)
{
  GcalRangeOverlap overlap;

  g_return_val_if_fail (GCAL_IS_EVENT (self), FALSE);
  overlap = gcal_range_calculate_overlap (gcal_event_get_range (self), range, NULL);
  return overlap != GCAL_RANGE_NO_OVERLAP;
}
