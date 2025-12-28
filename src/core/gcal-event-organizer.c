/* gcal-event-organizer.c
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

#define G_LOG_DOMAIN "gcal_event_organizer"

#include "gcal-event-organizer.h"
#include "glib-object.h"
#include "glib.h"
#include "libecal/libecal.h"

struct _GcalEventOrganizer
{
  GObject parent_instance;

  ECalComponentOrganizer *ecal_organizer;
};

G_DEFINE_FINAL_TYPE (GcalEventOrganizer, gcal_event_organizer, G_TYPE_OBJECT)

enum
{
  PROP_0,
  PROP_NAME,
  PROP_URI,
  PROP_SENTBY,
  PROP_LANGUAGE,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
gcal_event_organizer_finalize (GObject *object)
{
  GcalEventOrganizer *self = (GcalEventOrganizer *) object;

  g_clear_pointer (&self->ecal_organizer, e_cal_component_organizer_free);

  G_OBJECT_CLASS (gcal_event_organizer_parent_class)->finalize (object);
}

static void
gcal_event_organizer_get_property (GObject    *object,
                                   guint       property_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GcalEventOrganizer *self = GCAL_EVENT_ORGANIZER (object);

  switch (property_id)
    {
    case PROP_NAME:
      g_value_set_string (value, gcal_event_organizer_get_name (self));
      break;

    case PROP_URI:
      g_value_set_string (value, gcal_event_organizer_get_uri (self));
      break;

    case PROP_SENTBY:
      g_value_set_string (value, gcal_event_organizer_get_sent_by (self));
      break;

    case PROP_LANGUAGE:
      g_value_set_string (value, gcal_event_organizer_get_language (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
gcal_event_organizer_class_init (GcalEventOrganizerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gcal_event_organizer_finalize;
  object_class->get_property = gcal_event_organizer_get_property;

  /**
   * GcalEventOrganizer:name:
   *
   * The display (common name) of the organizer.
   */
  properties[PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL, NULL,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GcalEventOrganizer:uri:
   *
   * This corresponds with the "value" from the #ECalComponentOrganizer.
   * This is the email address or, more generally, the URI of the organizer.
   */
  properties[PROP_URI] =
    g_param_spec_string ("uri", NULL, NULL, NULL,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GcalEventOrganizer:sent-by:
   *
   * The mailto of the user acting on behalf of @self.
   */
  properties[PROP_SENTBY] =
    g_param_spec_string ("sent-by", NULL, NULL, NULL,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GcalEventOrganizer:language:
   *
   * The language (used for text) of the organizer.
   */
  properties[PROP_LANGUAGE] =
    g_param_spec_string ("language", NULL, NULL, NULL,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gcal_event_organizer_init (GcalEventOrganizer *instance)
{
}

/**
 * gcal_event_organizer_new:
 * @ecal_organizer: #ECalComponentOrganizer from the parent event.
 *
 * Takes the ECalComponentOrganizer from its parent event instance
 * and stores a copy of it internally.
 *
 * Returns: an instance of #GcalEventOrganizer.
 */
GcalEventOrganizer *
gcal_event_organizer_new (ECalComponentOrganizer *ecal_organizer)
{
  g_autoptr (GcalEventOrganizer) self = NULL;

  g_assert (ecal_organizer != NULL);

  self = g_object_new (GCAL_TYPE_EVENT_ORGANIZER, NULL);
  self->ecal_organizer = e_cal_component_organizer_copy (ecal_organizer);

  return g_steal_pointer (&self);
}

/**
 * gcal_event_organizer_get_name:
 * @self: an #GcalEventOrganizer instance.
 *
 * Returns: (transfer none) (nullable): the name (CN) of the organizer.
 */
const gchar *
gcal_event_organizer_get_name (GcalEventOrganizer *self)
{
  g_assert (GCAL_IS_EVENT_ORGANIZER (self));

  return e_cal_component_organizer_get_cn (self->ecal_organizer);
}

/**
 * gcal_event_organizer_get_uri:
 * @self: an #GcalEventOrganizer instance.
 *
 * The URI aka ECalComponentOrganizer.value is normally
 * the "mailto:user@email.org".
 *
 * Returns: (transfer none) (nullable): the URI (value) of the organizer.
 */
const gchar *
gcal_event_organizer_get_uri (GcalEventOrganizer *self)
{
  g_assert (GCAL_IS_EVENT_ORGANIZER (self));

  return e_cal_component_organizer_get_value (self->ecal_organizer);
}

/**
 * gcal_event_organizer_get_sent_by:
 * @self: an #GcalEventOrganizer instance.
 *
 * Returns: (transfer none) (nullable): "mailto:" of the sender.
 */
const gchar *
gcal_event_organizer_get_sent_by (GcalEventOrganizer *self)
{
  g_assert (GCAL_IS_EVENT_ORGANIZER (self));

  return e_cal_component_organizer_get_sentby (self->ecal_organizer);
}

/**
 * gcal_event_organizer_get_language:
 * @self: an #GcalEventOrganizer instance.
 *
 * Returns: (transfer none) (nullable): the language of the organizer.
 */
const gchar *
gcal_event_organizer_get_language (GcalEventOrganizer *self)
{
  g_assert (GCAL_IS_EVENT_ORGANIZER (self));

  return e_cal_component_organizer_get_language (self->ecal_organizer);
}
