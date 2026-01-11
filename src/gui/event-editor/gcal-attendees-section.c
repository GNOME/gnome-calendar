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
#include "gcal-utils.h"
#include "glib-object.h"

struct _GcalAttendeesSection
{
  GtkBox parent_instance;

  GcalContext         *context;
  GcalEvent           *event;

  AdwActionRow        *organizer_row;
  AdwActionRow        *summary_row;

  GListStore          *attendees;
};

static void          gcal_event_editor_section_init_iface        (GcalEventEditorSectionInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (GcalAttendeesSection, gcal_attendees_section, GTK_TYPE_BOX,
                               G_IMPLEMENT_INTERFACE (GCAL_TYPE_EVENT_EDITOR_SECTION, gcal_event_editor_section_init_iface))

enum
{
  PROP_0,
  PROP_CONTEXT,
  PROP_ATTENDEES,
  N_PROPS
};

static GParamSpec *properties[N_PROPS] = { NULL, };

static void
reset_section (GcalAttendeesSection *self,
               GcalEvent            *event)
{
  g_set_object (&self->event, event);

  g_list_store_remove_all (self->attendees);
  adw_action_row_set_subtitle (self->organizer_row, NULL);
}

static void
gcal_attendees_section_set_event (GcalEventEditorSection *section,
                                  GcalEvent              *event,
                                  GcalEventEditorFlags    flags)
{
  GcalAttendeesSection *self = GCAL_ATTENDEES_SECTION (section);
  GcalEventOrganizer *organizer;
  g_autoptr (GSList) attendees = NULL;

  if (event == self->event)
    return;

  reset_section (self, event);

  if (self->event == NULL)
    return;

  /* set organizer */
  organizer = gcal_event_get_organizer (self->event);
  if (organizer)
    {
      g_autofree const gchar *email = gcal_get_email_from_mailto_uri (gcal_event_organizer_get_uri (organizer));
      g_autofree const gchar *name_with_email = g_strdup_printf ("%s (%s)", gcal_event_organizer_get_name (organizer), email);

      adw_action_row_set_subtitle (self->organizer_row, name_with_email);
    }

  /* set attendees and summarize attendance status */
  attendees = gcal_event_get_attendees (self->event);
  for (GSList *l = attendees; l != NULL; l = l->next)
    g_list_store_append (self->attendees, GCAL_EVENT_ATTENDEE (l->data));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ATTENDEES]);
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
  g_clear_pointer (&self->attendees, g_list_store_remove_all);

  G_OBJECT_CLASS (gcal_attendees_section_parent_class)->finalize (object);
}

static void
gcal_attendees_section_init (GcalAttendeesSection *instance)
{
  instance->attendees = g_list_store_new (GCAL_TYPE_EVENT_ATTENDEE);
  gtk_widget_init_template (GTK_WIDGET (instance));
}

static void
gcal_attendees_section_class_init (GcalAttendeesSectionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

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

  properties[PROP_ATTENDEES] =
    g_param_spec_object ("attendees", NULL, NULL,
                         G_TYPE_LIST_STORE,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/event-editor/gcal-attendees-section.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalAttendeesSection, organizer_row);
  gtk_widget_class_bind_template_child (widget_class, GcalAttendeesSection, summary_row);
}
