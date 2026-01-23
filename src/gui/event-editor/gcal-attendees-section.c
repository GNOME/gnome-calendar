/* gcal-attendees-section.c
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

#include "gcal-attendees-section.h"

#include <adwaita.h>
#include <gtk/gtk.h>
#include <libintl.h>

#include "gcal-attendee-summary-row.h"
#include "gcal-context.h"
#include "gcal-event-attendee.h"
#include "gcal-event-editor-section.h"
#include "gcal-event-organizer.h"
#include "gcal-event.h"
#include "gcal-organizer-row.h"
#include "glib-object.h"

struct _GcalAttendeesSection
{
  GtkBox parent_instance;

  GcalContext         *context;
  GcalEvent           *event;
  GcalEventOrganizer  *organizer;

  AdwActionRow        *summary_row;

  GListModel          *attendees;
};

static void          gcal_event_editor_section_init_iface        (GcalEventEditorSectionInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (GcalAttendeesSection, gcal_attendees_section, GTK_TYPE_BOX,
                               G_IMPLEMENT_INTERFACE (GCAL_TYPE_EVENT_EDITOR_SECTION, gcal_event_editor_section_init_iface))

enum
{
  PROP_0,
  PROP_CONTEXT,
  PROP_ATTENDEES,
  PROP_ORGANIZER,
  N_PROPS
};

static GParamSpec *properties[N_PROPS] = { NULL, };

static void
gcal_attendees_section_set_event (GcalEventEditorSection *section,
                                  GcalEvent              *event,
                                  GcalEventEditorFlags    flags)
{
  GcalAttendeesSection *self = GCAL_ATTENDEES_SECTION (section);

  if (event == self->event)
    return;

  g_set_object (&self->event, event);

  if (self->event == NULL)
    return;

  /* set organizer */
  self->organizer = gcal_event_get_organizer (self->event);

  /* set attendees and summarize attendance status */
  self->attendees = gcal_event_get_attendees (self->event);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ATTENDEES]);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ORGANIZER]);
}

static void
gcal_attendees_section_apply (GcalEventEditorSection *section)
{
  /* read-only */
}

static gboolean
gcal_attendees_section_changed (GcalEventEditorSection *section)
{
  /* This section is read-only for now */
  return FALSE;
}

static void
gcal_event_editor_section_init_iface (GcalEventEditorSectionInterface *iface)
{
  iface->set_event = gcal_attendees_section_set_event;
  iface->apply = gcal_attendees_section_apply;
  iface->changed = gcal_attendees_section_changed;
}

static void
gcal_attendees_section_get_property (GObject *object,
                                     guint property_id,
                                     GValue *value,
                                     GParamSpec *pspec)
{
  GcalAttendeesSection *self = GCAL_ATTENDEES_SECTION (object);

  switch (property_id)
    {
    case PROP_CONTEXT:
      g_value_set_object (value, self->context);
      break;

    case PROP_ATTENDEES:
      g_value_set_object (value, self->attendees);
      break;

    case PROP_ORGANIZER:
      g_value_set_object (value, self->organizer);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
gcal_attendees_section_set_property (GObject *object,
                                     guint property_id,
                                     const GValue *value,
                                     GParamSpec *pspec)
{
  GcalAttendeesSection *self = GCAL_ATTENDEES_SECTION (object);

  switch (property_id)
    {
    case PROP_CONTEXT:
      g_assert (self->context == NULL);
      self->context = g_value_dup_object (value);
      break;

    case PROP_ORGANIZER:
      self->organizer = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
gcal_attendees_section_finalize (GObject *object)
{
  GcalAttendeesSection *self = (GcalAttendeesSection *) object;

  g_clear_object (&self->context);
  g_clear_object (&self->event);

  self->attendees = NULL; /* not owned */

  G_OBJECT_CLASS (gcal_attendees_section_parent_class)->finalize (object);
}

static void
gcal_attendees_section_init (GcalAttendeesSection *instance)
{
  gtk_widget_init_template (GTK_WIDGET (instance));
}

static void
gcal_attendees_section_class_init (GcalAttendeesSectionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  g_type_ensure (GCAL_TYPE_ORGANIZER_ROW);
  g_type_ensure (GCAL_TYPE_ATTENDEE_SUMMARY_ROW);

  object_class->finalize = gcal_attendees_section_finalize;
  object_class->get_property = gcal_attendees_section_get_property;
  object_class->set_property = gcal_attendees_section_set_property;

  /**
   * GcalAttendeesSection:context:
   *
   * The #GcalContext instance. Required by the interface.
   */
  properties[PROP_CONTEXT] =
    g_param_spec_object ("context", NULL, NULL,
                         GCAL_TYPE_CONTEXT,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * GcalAttendeesSection:attendees:
   *
   * ListStore of #GcalEventAttendee.
   * Passed to the summary rows #GcalAttendeeSummaryRow.
   */
  properties[PROP_ATTENDEES] =
    g_param_spec_object ("attendees", NULL, NULL,
                         G_TYPE_LIST_MODEL,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GcalAttendeesSection:organizer:
   *
   * Organizer of the event.
   * #GcalOrganizerRow binds to this property.
   */
  properties[PROP_ORGANIZER] =
    g_param_spec_object ("organizer", NULL, NULL,
                         GCAL_TYPE_EVENT_ORGANIZER,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/event-editor/gcal-attendees-section.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalAttendeesSection, summary_row);
}
