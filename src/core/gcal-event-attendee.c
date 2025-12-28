/* gcal-event-attendee.c
 *
 * Copyright (C) 2025 Alen Galinec <mind.on.warp@gmail.com>
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

#include "gcal-event-attendee.h"
#include "gcal-enum-types.h"
#include "gcal-enums.h"
#include "glib-object.h"
#include "glib.h"
#include "libecal/libecal.h"

struct _GcalEventAttendee
{
  GObject parent_instance;

  ECalComponentAttendee *ecal_attendee;
};

enum
{
  PROP_NAME = 1,
  PROP_TYPE,
  PROP_DELEGATED_FROM,
  PROP_DELEGATED_TO,
  PROP_LANGUAGE,
  PROP_MEMBER,
  PROP_PARTICIPATION_STATUS,
  PROP_ROLE,
  PROP_RSVP,
  PROP_SENT_BY,
  PROP_URI,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

G_DEFINE_FINAL_TYPE (GcalEventAttendee, gcal_event_attendee, G_TYPE_OBJECT);

/*
 * Auxiliary functions
 */

static GcalEventAttendeeType
to_gcal_attendee_type (ICalParameterCutype ical_type)
{
  switch (ical_type)
    {
    case I_CAL_CUTYPE_INDIVIDUAL:
      return GCAL_EVENT_ATTENDEE_TYPE_INDIVIDUAL;

    case I_CAL_CUTYPE_GROUP:
      return GCAL_EVENT_ATTENDEE_TYPE_GROUP;

    case I_CAL_CUTYPE_RESOURCE:
      return GCAL_EVENT_ATTENDEE_TYPE_RESOURCE;

    case I_CAL_CUTYPE_ROOM:
      return GCAL_EVENT_ATTENDEE_TYPE_ROOM;

    case I_CAL_CUTYPE_UNKNOWN:
      return GCAL_EVENT_ATTENDEE_TYPE_UNKNOWN;

    case I_CAL_CUTYPE_NONE:
    default:
      return GCAL_EVENT_ATTENDEE_TYPE_NONE;
    }
}

static GcalEventAttendeePartStat
to_gcal_attendee_participation_status (ICalParameterPartstat ical_rsvp)
{
  switch (ical_rsvp)
    {
    case I_CAL_PARTSTAT_NEEDSACTION:
      return GCAL_EVENT_ATTENDEE_PART_NEEDS_ACTION;

    case I_CAL_PARTSTAT_ACCEPTED:
      return GCAL_EVENT_ATTENDEE_PART_ACCEPTED;

    case I_CAL_PARTSTAT_DECLINED:
      return GCAL_EVENT_ATTENDEE_PART_DECLINED;

    case I_CAL_PARTSTAT_TENTATIVE:
      return GCAL_EVENT_ATTENDEE_PART_TENTATIVE;

    case I_CAL_PARTSTAT_DELEGATED:
      return GCAL_EVENT_ATTENDEE_PART_DELEGATED;

    case I_CAL_PARTSTAT_COMPLETED:
      return GCAL_EVENT_ATTENDEE_PART_COMPLETED;

    case I_CAL_PARTSTAT_INPROCESS:
      return GCAL_EVENT_ATTENDEE_PART_INPROCESS;

    case I_CAL_PARTSTAT_FAILED:
      return GCAL_EVENT_ATTENDEE_PART_FAILED;

    case I_CAL_PARTSTAT_NONE:
    default:
      return GCAL_EVENT_ATTENDEE_PART_NONE;
    }
}

static GcalEventAttendeeRole
to_gcal_attendee_role (ICalParameterRole ical_role)
{
  switch (ical_role)
    {
    case I_CAL_ROLE_CHAIR:
      return GCAL_EVENT_ATTENDEE_ROLE_CHAIR;

    case I_CAL_ROLE_REQPARTICIPANT:
      return GCAL_EVENT_ATTENDEE_ROLE_REQUIRED;

    case I_CAL_ROLE_OPTPARTICIPANT:
      return GCAL_EVENT_ATTENDEE_ROLE_OPTIONAL;

    case I_CAL_ROLE_NONPARTICIPANT:
      return GCAL_EVENT_ATTENDEE_ROLE_NONPARTICIPANT;

    case I_CAL_ROLE_NONE:
    default:
      return GCAL_EVENT_ATTENDEE_ROLE_NONE;
    }
}

static void
gcal_event_attendee_get_property (GObject *object,
                                  guint property_id,
                                  GValue *value,
                                  GParamSpec *pspec)
{
  GcalEventAttendee *self = GCAL_EVENT_ATTENDEE (object);

  g_assert (self != NULL);

  switch (property_id)
    {
    case PROP_NAME:
      g_value_set_string (value, gcal_event_attendee_get_name (self));
      break;

    case PROP_TYPE:
      g_value_set_enum (value, gcal_event_attendee_get_attendee_type (self));
      break;

    case PROP_DELEGATED_FROM:
      g_value_set_string (value, gcal_event_attendee_get_delegated_from (self));
      break;

    case PROP_DELEGATED_TO:
      g_value_set_string (value, gcal_event_attendee_get_delegated_to (self));
      break;

    case PROP_LANGUAGE:
      g_value_set_string (value, gcal_event_attendee_get_language (self));
      break;

    case PROP_MEMBER:
      g_value_set_string (value, gcal_event_attendee_get_member (self));
      break;

    case PROP_PARTICIPATION_STATUS:
      g_value_set_enum (value, gcal_event_attendee_get_part_status (self));
      break;

    case PROP_ROLE:
      g_value_set_enum (value, gcal_event_attendee_get_role (self));
      break;

    case PROP_RSVP:
      g_value_set_boolean (value, gcal_event_attendee_get_requires_rsvp (self));
      break;

    case PROP_SENT_BY:
      g_value_set_string (value, gcal_event_attendee_get_sent_by (self));
      break;

    case PROP_URI:
      g_value_set_string (value, gcal_event_attendee_get_uri (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
gcal_event_attendee_dispose (GObject *instance)
{
  GcalEventAttendee *self = NULL;

  g_return_if_fail ((self = GCAL_EVENT_ATTENDEE (instance)));

  g_clear_pointer (&self->ecal_attendee, e_cal_component_attendee_free);
}

static void
gcal_event_attendee_class_init (GcalEventAttendeeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gcal_event_attendee_dispose;
  object_class->get_property = gcal_event_attendee_get_property;

  /**
   * GcalEventAttendee:name:
   *
   * The display name (common name) of the attendee.
   */
  properties[PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL,
                         NULL,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GcalEventAttendee:type:
   *
   * The calendar user type of the attendee.
   * A value of type #GcalEventAttendeeType
   *
   * @see-also: ICalParameterCutype
   */
  properties[PROP_TYPE] =
    g_param_spec_enum ("type", NULL, NULL,
                       GCAL_TYPE_EVENT_ATTENDEE_TYPE,
                       GCAL_EVENT_ATTENDEE_TYPE_NONE,
                       G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GcalEventAttendee:delegated-from:
   *
   * Delegator:
   * The mailto of the attendee that @self is delegated from.
   */
  properties[PROP_DELEGATED_FROM] =
    g_param_spec_string ("delegated-from", NULL, NULL,
                         NULL,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GcalEventAttendee:delegated-to:
   *
   * Delegatee:
   * The mailto of the attendee that @self is delegating to.
   */
  properties[PROP_DELEGATED_TO] =
    g_param_spec_string ("delegated-to", NULL, NULL,
                         NULL,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GcalEventAttendee:language:
   *
   * The language (for text) of the attendee.
   */
  properties[PROP_LANGUAGE] =
    g_param_spec_string ("language", NULL, NULL,
                         NULL,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GcalEventAttendee:member:
   *
   * Group or list membership.
   */
  properties[PROP_MEMBER] =
    g_param_spec_string ("member", NULL, NULL,
                         NULL,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GcalEventAttendee:participation-status:
   *
   * The participation status response of the attendee.
   * Can be one of the values from the #GcalEventAttendeePartStat enum.
   */
  properties[PROP_PARTICIPATION_STATUS] =
    g_param_spec_enum ("participation-status", NULL, NULL,
                       GCAL_TYPE_EVENT_ATTENDEE_PART_STAT,
                       GCAL_EVENT_ATTENDEE_PART_NONE,
                       G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GcalEventAttendee:role:
   *
   * Participation role of the attendee (e.g. "required", "optional").
   * Can be one of the values from the #GcalEventAttendeeRole enum.
   */
  properties[PROP_ROLE] =
    g_param_spec_enum ("role", NULL, NULL,
                       GCAL_TYPE_EVENT_ATTENDEE_ROLE,
                       GCAL_EVENT_ATTENDEE_ROLE_NONE,
                       G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GcalEventAttendee:requires-rsvp:
   *
   * Whether this attendee requires RSVP or not.
   */
  properties[PROP_RSVP] =
    g_param_spec_boolean ("requires-rsvp", NULL, NULL,
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GcalEventAttendee:sent-by:
   *
   * The mailto of the user that is acting on behalf of this attendee.
   */
  properties[PROP_SENT_BY] =
    g_param_spec_string ("sent-by", NULL, NULL,
                         NULL,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GcalEventAttendee:uri:
   *
   * Corresponds to the "value" property of the #ECalComponentAttendee.
   * This is the email address or, more generally,
   *  the URI of the attendee, which can also be a resource/room.
   */
  properties[PROP_URI] =
    g_param_spec_string ("uri", NULL, NULL,
                         NULL,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gcal_event_attendee_init (GcalEventAttendee *self)
{
}

/**
 * gcal_event_attendee_new:
 * @ecal_attendee: the #ECalComponentAttendee from the event
 *
 * Takes the #ECalComponentAttendee from the parent event and
 * stores a copy in its own structure and cleans it up when
 * disposed.
 *
 * Returns: a new instance of #GcalEventAttendee.
 */
GcalEventAttendee *
gcal_event_attendee_new (ECalComponentAttendee *ecal_attendee)
{
  g_return_val_if_fail (ecal_attendee != NULL, NULL);

  GcalEventAttendee *instance = g_object_new (GCAL_TYPE_EVENT_ATTENDEE,
                                              NULL);

  instance->ecal_attendee = e_cal_component_attendee_copy (ecal_attendee);

  return instance;
}

/**
 * gcal_event_attendee_get_name:
 * @self: a #GcalEventAttendee
 *
 * Returns: (transfer none) (nullable): the common name of the attendee.
 */
const gchar *
gcal_event_attendee_get_name (GcalEventAttendee *self)
{
  g_return_val_if_fail (GCAL_IS_EVENT_ATTENDEE (self), NULL);

  return e_cal_component_attendee_get_cn (self->ecal_attendee);
}

/**
 * gcal_event_attendee_get_attendee_type:
 * @self: a #GcalEventAttendee
 *
 * Returns: the type of attendee (ECal cutype).
 */
GcalEventAttendeeType
gcal_event_attendee_get_attendee_type (GcalEventAttendee *self)
{
  ICalParameterCutype ical_type;

  g_return_val_if_fail (GCAL_IS_EVENT_ATTENDEE (self), GCAL_EVENT_ATTENDEE_TYPE_NONE);

  ical_type = e_cal_component_attendee_get_cutype (self->ecal_attendee);

  return to_gcal_attendee_type (ical_type);
}

/**
 * gcal_event_attendee_get_delegated_from:
 * @self: a #GcalEventAttendee
 *
 * Returns: (transfer none) (nullable): the mailto of "delegated-from".
 */
const gchar *
gcal_event_attendee_get_delegated_from (GcalEventAttendee *self)
{
  g_return_val_if_fail (GCAL_IS_EVENT_ATTENDEE (self), NULL);

  return e_cal_component_attendee_get_delegatedfrom (self->ecal_attendee);
}

/**
 * gcal_event_attendee_get_delegated_to:
 * @self: a #GcalEventAttendee
 *
 * Returns: (transfer none) (nullable): the mailto of "delegated-to".
 */
const gchar *
gcal_event_attendee_get_delegated_to (GcalEventAttendee *self)
{
  g_return_val_if_fail (GCAL_IS_EVENT_ATTENDEE (self), NULL);

  return e_cal_component_attendee_get_delegatedto (self->ecal_attendee);
}

/**
 * gcal_event_attendee_get_language:
 * @self: a #GcalEventAttendee
 *
 * Returns: (transfer none) (nullable): attendee's language.
 */
const gchar *
gcal_event_attendee_get_language (GcalEventAttendee *self)
{
  g_return_val_if_fail (GCAL_IS_EVENT_ATTENDEE (self), NULL);

  return e_cal_component_attendee_get_language (self->ecal_attendee);
}

/**
 * gcal_event_attendee_get_member:
 * @self: a #GcalEventAttendee
 *
 * Returns: (transfer none) (nullable): member.
 */
const gchar *
gcal_event_attendee_get_member (GcalEventAttendee *self)
{
  g_return_val_if_fail (GCAL_IS_EVENT_ATTENDEE (self), NULL);

  return e_cal_component_attendee_get_member (self->ecal_attendee);
}

/**
 * gcal_event_attendee_get_part_status:
 * @self: a #GcalEventAttendee
 *
 * Maps the ECal part_stat to #GcalEventAttendeePartStat and returns it.
 *
 * Returns: the participation response.
 */
GcalEventAttendeePartStat
gcal_event_attendee_get_part_status (GcalEventAttendee *self)
{
  ICalParameterPartstat ical_partstat;

  g_return_val_if_fail (GCAL_IS_EVENT_ATTENDEE (self), GCAL_EVENT_ATTENDEE_PART_NONE);

  ical_partstat = e_cal_component_attendee_get_partstat (self->ecal_attendee);

  return to_gcal_attendee_participation_status (ical_partstat);
}

/**
 * gcal_event_attendee_get_role:
 * @self: a #GcalEventAttendee
 *
 * Maps the ECal role to #GcalEventAttendeeRole and returns it.
 * This represents the role of the attendee, i.e.
 * whether they're _required_, _optional_, etc.
 *
 * Returns: the attendee role.
 */
GcalEventAttendeeRole
gcal_event_attendee_get_role (GcalEventAttendee *self)
{
  ICalParameterRole ical_role;

  g_return_val_if_fail (GCAL_IS_EVENT_ATTENDEE (self), GCAL_EVENT_ATTENDEE_ROLE_NONE);

  ical_role = e_cal_component_attendee_get_role (self->ecal_attendee);

  return to_gcal_attendee_role (ical_role);
}

/**
 * gcal_event_attendee_get_requires_rsvp:
 * @self: a #GcalEventAttendee
 *
 * Returns: whether the attendee requires rsvp.
 */
gboolean
gcal_event_attendee_get_requires_rsvp (GcalEventAttendee *self)
{
  g_return_val_if_fail (GCAL_IS_EVENT_ATTENDEE (self), FALSE);

  return e_cal_component_attendee_get_rsvp (self->ecal_attendee);
}

/**
 * gcal_event_attendee_get_sent_by:
 * @self: a #GcalEventAttendee
 *
 * Returns: (transfer none) (nullable): the mailto of "sent-by".
 */
const gchar *
gcal_event_attendee_get_sent_by (GcalEventAttendee *self)
{
  g_return_val_if_fail (GCAL_IS_EVENT_ATTENDEE (self), NULL);

  return e_cal_component_attendee_get_sentby (self->ecal_attendee);
}

/**
 * gcal_event_attendee_get_uri:
 * @self: a #GcalEventAttendee
 *
 * Corresponds to the "value" property of the ECalComponentAttendee.
 * Value seems a bit vague as a name for something that contains
 * the URI of the resource (in this case, an attendee).
 *
 * Returns: (transfer none) (nullable): the URI of the attendee.
 */
const gchar *
gcal_event_attendee_get_uri (GcalEventAttendee *self)
{
  g_return_val_if_fail (GCAL_IS_EVENT_ATTENDEE (self), NULL);

  return e_cal_component_attendee_get_value (self->ecal_attendee);
}
