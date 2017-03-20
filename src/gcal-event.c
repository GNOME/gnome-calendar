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

#include "gcal-event.h"
#include "gcal-utils.h"

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
 * ## Timezones
 *
 * When a #GcalEvent is created, the timezone is parsed from
 * the #ECalComponent. The start and end dates can possibly
 * have different timezones, e.g. when the user is traveling
 * across timezones and the departure time is in a different
 * timezone of the arrival time.
 *
 * For the sake of sanity, gcal_event_get_timezone() returns
 * the timezone of the start date. If you need to precisely
 * check the timezones of the start and end dates, you have
 * to use g_date_time_get_timezone_identifier().
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

  /*
   * The description is cached in the class because it
   * may come as a GSList of descriptions, in which
   * case we merge them and cache it here.
   */
  gchar              *description;

  GTimeZone          *timezone;
  GDateTime          *dt_start;
  GDateTime          *dt_end;

  GdkRGBA            *color;
  GBinding           *color_binding;

  gboolean            all_day;

  /* A map of GcalAlarmType */
  GHashTable         *alarms;

  ECalComponent      *component;
  ESource            *source;

  gboolean            is_valid : 1;
  gboolean            is_initialized : 1;
  GError             *initialization_error;
};

static void          gcal_event_initable_iface_init              (GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (GcalEvent, gcal_event, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, gcal_event_initable_iface_init))

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
  N_PROPS
};

static GTimeZone*
get_timezone_from_ical (ECalComponentDateTime *comp)
{
  GTimeZone *tz;

  if (comp->value->is_date)
    {
      /*
       * When loading an all day event, the timezone must not be taken into account
       * when loading the date. Since all events in ical are UTC, we match it by
       * setting the timezone as UTC as well.
       */
      tz = g_time_zone_new_utc ();
    }
  else if (icaltime_get_timezone (*comp->value))
    {
      gchar *tzid;
      gint offset;

      offset = icaltimezone_get_utc_offset ((icaltimezone*) icaltime_get_timezone (*comp->value),
                                            comp->value, NULL);
      tzid = format_utc_offset (offset);
      tz = g_time_zone_new (tzid);

      g_free (tzid);
    }
  else if (comp->tzid)
    {
      icaltimezone *zone;
      gchar *tzid;
      gint offset;

      if (g_str_has_prefix (comp->tzid, LIBICAL_TZID_PREFIX))
          zone = icaltimezone_get_builtin_timezone_from_tzid (comp->tzid);
      else
          zone = icaltimezone_get_builtin_timezone (comp->tzid);

      offset = icaltimezone_get_utc_offset (zone, comp->value, NULL);
      tzid = format_utc_offset (offset);

      tz = g_time_zone_new (tzid);

      g_free (tzid);
    }
  else
    {
      tz = g_time_zone_new_utc ();
    }

  return tz;
}

static ECalComponentDateTime*
build_component_from_datetime (GcalEvent    *self,
                               icaltimezone *current_tz,
                               GDateTime    *dt)
{
  ECalComponentDateTime *comp_dt;

  if (!dt)
    return NULL;

  comp_dt = g_new0 (ECalComponentDateTime, 1);
  comp_dt->value = NULL;
  comp_dt->tzid = NULL;

  comp_dt->value = datetime_to_icaltime (dt);
  comp_dt->value->zone = self->all_day ? icaltimezone_get_utc_timezone () : current_tz;
  comp_dt->value->is_date = self->all_day;

  /* All day events have UTC timezone */
  comp_dt->tzid = g_strdup (self->all_day ? "UTC" : icaltimezone_get_tzid (current_tz));

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
  g_object_notify (G_OBJECT (self), "uid");
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
       * A NULL start date is invalid. We set something bogus to procceed, and make
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

      date = icaltime_normalize (*start.value);
      zone_start = get_timezone_from_ical (&start);
      date_start = g_date_time_new (zone_start,
                                    date.year, date.month, date.day,
                                    date.is_date ? 0 : date.hour,
                                    date.is_date ? 0 : date.minute,
                                    date.is_date ? 0 : date.second);
      start_is_all_day = datetime_is_date (date_start);

      self->dt_start = date_start;


      /* The timezone of the event is the timezone of the start date */
      self->timezone = g_time_zone_ref (zone_start);

      /* Setup end date */
      e_cal_component_get_dtend (component, &end);

      if(!end.value)
        {
            self->all_day = TRUE;
        }
      else
        {
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

      /* Load and setup the alarms */
      load_alarms (self);

      g_object_notify (G_OBJECT (self), "component");
      g_object_notify (G_OBJECT (self), "location");
      g_object_notify (G_OBJECT (self), "summary");

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

  if (self->is_initialized)
    goto out;


out:
  self->is_initialized = TRUE;

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

  g_clear_pointer (&self->dt_start, g_date_time_unref);
  g_clear_pointer (&self->dt_end, g_date_time_unref);
  g_clear_pointer (&self->timezone, g_time_zone_unref);
  g_clear_pointer (&self->description, g_free);
  g_clear_pointer (&self->alarms, g_hash_table_unref);
  g_clear_pointer (&self->uid, g_free);
  g_clear_pointer (&self->color, gdk_rgba_free);
  g_clear_object (&self->component);
  g_clear_object (&self->source);

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

    case PROP_TIMEZONE:
      g_value_set_boxed (value, self->timezone);
      break;

    case PROP_UID:
      g_value_set_string (value, self->uid);
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

    case PROP_TIMEZONE:
      gcal_event_set_timezone (self, g_value_get_boxed (value));
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
  g_object_class_install_property (object_class,
                                   PROP_ALL_DAY,
                                   g_param_spec_boolean ("all-day",
                                                         "If event is all day",
                                                         "Whether the event is all day or not",
                                                         FALSE,
                                                         G_PARAM_READWRITE));

  /**
   * GcalEvent::color:
   *
   * The color of the event.
   */
  g_object_class_install_property (object_class,
                                   PROP_COLOR,
                                   g_param_spec_boxed ("color",
                                                       "Color of the event",
                                                       "The color of the event",
                                                       GDK_TYPE_RGBA,
                                                       G_PARAM_READWRITE));

  /**
   * GcalEvent::component:
   *
   * The #ECalComponent of this event.
   */
  g_object_class_install_property (object_class,
                                   PROP_COMPONENT,
                                   g_param_spec_object ("component",
                                                        "Component",
                                                        "The ECalComponent of the event",
                                                        E_TYPE_CAL_COMPONENT,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  /**
   * GcalEvent::date-end:
   *
   * The end date of the event.
   */
  g_object_class_install_property (object_class,
                                   PROP_DATE_END,
                                   g_param_spec_boxed ("date-end",
                                                       "End date of the event",
                                                       "The end date of the event",
                                                       G_TYPE_DATE_TIME,
                                                       G_PARAM_READWRITE));

  /**
   * GcalEvent::date-start:
   *
   * The start date of the event.
   */
  g_object_class_install_property (object_class,
                                   PROP_DATE_START,
                                   g_param_spec_boxed ("date-start",
                                                       "Start date of the event",
                                                       "The start date of the event",
                                                       G_TYPE_DATE_TIME,
                                                       G_PARAM_READWRITE));

  /**
   * GcalEvent::description:
   *
   * The description of the event.
   */
  g_object_class_install_property (object_class,
                                   PROP_DESCRIPTION,
                                   g_param_spec_string ("description",
                                                        "Description of the event",
                                                        "The description of the event",
                                                        "",
                                                        G_PARAM_READWRITE));

  /**
   * GcalEvent::location:
   *
   * The location of the event.
   */
  g_object_class_install_property (object_class,
                                   PROP_LOCATION,
                                   g_param_spec_string ("location",
                                                        "Location of the event",
                                                        "The location of the event",
                                                        "",
                                                        G_PARAM_READWRITE));

  /**
   * GcalEvent::source:
   *
   * The #ESource this event belongs to.
   */
  g_object_class_install_property (object_class,
                                   PROP_SOURCE,
                                   g_param_spec_object ("source",
                                                        "ESource",
                                                        "The ESource this event belongs to",
                                                        E_TYPE_SOURCE,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  /**
   * GcalEvent::summary:
   *
   * The summary of the event.
   */
  g_object_class_install_property (object_class,
                                   PROP_SUMMARY,
                                   g_param_spec_string ("summary",
                                                        "Summary of the event",
                                                        "The summary of the event",
                                                        "",
                                                        G_PARAM_READWRITE));

  /**
   * GcalEvent::timezone:
   *
   * The timezone of the event.
   */
  g_object_class_install_property (object_class,
                                   PROP_TIMEZONE,
                                   g_param_spec_boxed ("timezone",
                                                       "Timezone of the event",
                                                       "The timezone of the event",
                                                       G_TYPE_TIME_ZONE,
                                                       G_PARAM_READWRITE));

  /**
   * GcalEvent::uid:
   *
   * The unique identifier of the event.
   */
  g_object_class_install_property (object_class,
                                   PROP_UID,
                                   g_param_spec_string ("uid",
                                                        "Identifier of the event",
                                                        "The unique identifier of the event",
                                                        "",
                                                        G_PARAM_READABLE));
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
 * gcal_event_error_quark:
 *
 * Error quark for event creating.
 */
GQuark
gcal_event_error_quark (void)
{
  return g_quark_from_static_string ("Invalid start date");
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
  return g_initable_new (GCAL_TYPE_EVENT,
                         NULL,
                         error,
                         "source", source,
                         "component", component,
                         NULL);
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

      g_object_notify (G_OBJECT (self), "color");
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

      g_object_notify (G_OBJECT (self), "all-day");
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

  if (self->dt_end != dt &&
      (!self->dt_end || !dt ||
       (self->dt_end && dt && !g_date_time_equal (self->dt_end, dt))))
    {
      ECalComponentDateTime *component_dt, current;

      g_clear_pointer (&self->dt_end, g_date_time_unref);
      self->dt_end = g_date_time_ref (dt);

      /* Retrieve the current timezone */
      e_cal_component_get_dtstart (self->component, &current);

      /* Setup the ECalComponent's datetime value */
      component_dt = build_component_from_datetime (self,
                                                    icaltimezone_get_builtin_timezone_from_tzid (current.tzid),
                                                    dt);

      e_cal_component_set_dtend (self->component, component_dt);
      e_cal_component_commit_sequence (self->component);

      g_object_notify (G_OBJECT (self), "date-end");

      g_clear_pointer (&component_dt, e_cal_component_free_datetime);
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

  if (self->dt_start != dt &&
      (!self->dt_start || !dt ||
       (self->dt_start && dt && !g_date_time_equal (self->dt_start, dt))))
    {
      ECalComponentDateTime *component_dt, current;

      g_clear_pointer (&self->dt_start, g_date_time_unref);
      self->dt_start = g_date_time_ref (dt);

      /* Retrieve the current timezone */
      e_cal_component_get_dtstart (self->component, &current);

      /* Setup the ECalComponent's datetime value */
      component_dt = build_component_from_datetime (self,
                                                    icaltimezone_get_builtin_timezone_from_tzid (current.tzid),
                                                    dt);

      e_cal_component_set_dtstart (self->component, component_dt);
      e_cal_component_commit_sequence (self->component);

      g_object_notify (G_OBJECT (self), "date-start");

      g_clear_pointer (&component_dt, e_cal_component_free_datetime);
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

      self->description = g_strdup (description);

      text_component.value = description;
      text_component.altrep = NULL;

      list.data = &text_component;
      list.next = NULL;

      e_cal_component_set_description_list (self->component, &list);
      e_cal_component_commit_sequence (self->component);

      g_object_notify (G_OBJECT (self), "description");
    }
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

      g_object_notify (G_OBJECT (self), "location");
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

      g_object_notify (G_OBJECT (self), "source");

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

      g_object_notify (G_OBJECT (self), "summary");
    }
}

/**
 * gcal_event_get_timezone:
 * @self: a #GcalEvent
 *
 * Retrieves the event's timezone.
 *
 * Returns: (transfer none): a #GTimeZone
 */
GTimeZone*
gcal_event_get_timezone (GcalEvent *self)
{
  g_return_val_if_fail (GCAL_IS_EVENT (self), NULL);

  return self->timezone;
}

/**
 * gcal_event_set_timezone:
 * @self: a #GcalEvent
 * @timezone: a #GTimeZone
 *
 * Sets the timezone of the event to @timezone.
 */
void
gcal_event_set_timezone (GcalEvent *self,
                         GTimeZone *timezone)
{
  g_return_if_fail (GCAL_IS_EVENT (self));

  if (self->timezone != timezone)
    {
      g_clear_pointer (&self->timezone, g_time_zone_unref);
      self->timezone = g_time_zone_ref (timezone);

      /* Swap the timezone from the component start & end dates*/
      if (!self->all_day)
        {
          GDateTime *new_dtstart;

          new_dtstart = g_date_time_to_timezone (self->dt_start, timezone);
          gcal_event_set_date_start (self, new_dtstart);

          if (self->dt_end)
            {
              GDateTime *new_dtend;

              new_dtend = g_date_time_to_timezone (self->dt_end, timezone);
              gcal_event_set_date_end (self, new_dtend);

              g_clear_pointer (&new_dtstart, g_date_time_unref);
            }

          g_clear_pointer (&new_dtstart, g_date_time_unref);
        }

      g_object_notify (G_OBJECT (self), "timezone");
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
 * Whether the event spans more than 1 day.
 *
 * Returns: %TRUE if @self spans more than 1 day, %FALSE otherwise.
 */
gboolean
gcal_event_is_multiday (GcalEvent *self)
{
  GDateTime *end_date, *real_end_date;
  gboolean is_multiday;
  gint n_days;

  g_return_val_if_fail (GCAL_IS_EVENT (self), FALSE);

  end_date = gcal_event_get_date_end (self);
  real_end_date = g_date_time_add_seconds (end_date, -1);
  is_multiday = FALSE;
  n_days = g_date_time_difference (real_end_date, self->dt_start) / G_TIME_SPAN_DAY;

  /*
   * An all-day event with only 1 day of time span is treated as a
   * single-day event.
   */
  if (self->all_day && n_days == 1)
    is_multiday = FALSE;

  /*
   * If any of the following fields are different, we're certain that
   * it's a multiday event. Otherwise, we're certain it's NOT a multiday
   * event.
   */
  if (g_date_time_get_year (self->dt_start) != g_date_time_get_year (real_end_date) ||
      g_date_time_get_month (self->dt_start) != g_date_time_get_month (real_end_date) ||
      g_date_time_get_day_of_month (self->dt_start) != g_date_time_get_day_of_month (real_end_date))
    {
      is_multiday = TRUE;
    }

  g_clear_pointer (&real_end_date, g_date_time_unref);

  return is_multiday;
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
