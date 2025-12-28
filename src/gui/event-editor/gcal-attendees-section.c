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

#include "gcal-context.h"
#include "gcal-enums.h"
#include "gcal-event-attendee.h"
#include "gcal-event-editor-section.h"
#include "gcal-event-organizer.h"
#include "gcal-event.h"
#include "gcal-utils.h"

struct _GcalAttendeesSection
{
  GtkBox parent_instance;

  GcalContext         *context;
  GcalEvent           *event;

  AdwActionRow        *organizer_row;
  AdwActionRow        *summary_row;

  /* participation summary */
  guint num_accepted;
  guint num_declined;
  guint num_tentative;
  guint num_not_responded;
  guint num_delegated;
};

static void          gcal_event_editor_section_init_iface        (GcalEventEditorSectionInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (GcalAttendeesSection, gcal_attendees_section, GTK_TYPE_BOX,
                               G_IMPLEMENT_INTERFACE (GCAL_TYPE_EVENT_EDITOR_SECTION, gcal_event_editor_section_init_iface))

enum
{
  PROP_0,
  PROP_CONTEXT,
  PROP_NUM_ACCEPTED,
  PROP_NUM_DECLINED,
  PROP_NUM_TENTATIVE,
  PROP_NUM_DELEGATED,
  PROP_NUM_UNKNOWN,
  PROP_NUM_TOTAL,
  N_PROPS
};

enum
{
  SUMMARY_ROW_ACTIVATED,
  N_SIGNALS,
};

static guint signals[N_SIGNALS] = { 0, };
static GParamSpec *properties[N_PROPS] = { NULL, };

static void
reset_section (GcalAttendeesSection *self,
               GcalEvent            *event)
{
  if (event == self->event)
    return;

  g_set_object (&self->event, event);

  adw_action_row_set_subtitle (self->organizer_row, NULL);
  adw_action_row_set_subtitle (self->summary_row, NULL);

  self->num_accepted = 0;
  self->num_declined = 0;
  self->num_tentative = 0;
  self->num_not_responded = 0;
  self->num_delegated = 0;
}

static inline void
concat_subtitle_part (GStrvBuilder *builder,
                      guint         value,
                      const gchar  *text)
{
  if (value <= 0)
    return;

  g_strv_builder_take (builder, g_strdup_printf ("%d %s", value, text));
}

static gchar*
build_summary_subtitle (GcalAttendeesSection  *self)
{
  g_autoptr (GStrvBuilder) builder = g_strv_builder_new ();

  concat_subtitle_part (builder, self->num_accepted, ngettext ("accepted", "accepted", self->num_accepted));
  concat_subtitle_part (builder, self->num_tentative, ngettext ("maybe", "maybe", self->num_tentative));
  concat_subtitle_part (builder, self->num_declined, ngettext ("declined", "declined", self->num_declined));
  concat_subtitle_part (builder, self->num_delegated, ngettext ("delegated", "delegated", self->num_delegated));
  concat_subtitle_part (builder, self->num_not_responded, ngettext ("not responded", "not responded", self->num_not_responded));

  g_auto (GStrv) parts = g_strv_builder_end (builder);

  gchar *result = g_strjoinv (", ", parts);

  return result;
}

static void
gcal_attendees_section_notify (GcalAttendeesSection *self)
{
  for (guint i = 1; i < N_PROPS; ++i)
    g_object_notify_by_pspec (G_OBJECT (self), properties[i]);
}

static void
gcal_attendees_section_set_event (GcalEventEditorSection *section,
                                  GcalEvent              *event,
                                  GcalEventEditorFlags    flags)
{
  GcalAttendeesSection *self = GCAL_ATTENDEES_SECTION (section);
  GcalEventOrganizer *organizer;
  g_autoptr (GSList) attendees = NULL;

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
    {
      g_autoptr (GcalEventAttendee) attendee = GCAL_EVENT_ATTENDEE (l->data);

      switch (gcal_event_attendee_get_part_status (attendee))
        {
        case GCAL_EVENT_ATTENDEE_PART_ACCEPTED:
          self->num_accepted++;
          break;

        case GCAL_EVENT_ATTENDEE_PART_DECLINED:
          self->num_declined++;
          break;

        case GCAL_EVENT_ATTENDEE_PART_TENTATIVE:
          self->num_tentative++;
          break;

        case GCAL_EVENT_ATTENDEE_PART_DELEGATED:
          self->num_delegated++;
          break;

        default:
          self->num_not_responded++;
          break;
        }
    }

  adw_action_row_set_subtitle (self->summary_row, build_summary_subtitle (self));

  /* notify properties changed */
  gcal_attendees_section_notify (self);
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

static guint
gcal_attendees_section_get_num_participants (GcalAttendeesSection *self)
{
  g_return_val_if_fail (GCAL_IS_ATTENDEES_SECTION (self), 0);

  return self->num_accepted + self->num_declined + self->num_tentative + self->num_delegated + self->num_not_responded;
}

static void
on_participant_summary_row_activated_cb (AdwActionRow *summary_row,
                                         GcalAttendeesSection *self)
{
  g_signal_emit (self, signals[SUMMARY_ROW_ACTIVATED], 0);
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

    case PROP_NUM_ACCEPTED:
      g_value_set_uint (value, self->num_accepted);
      break;

    case PROP_NUM_DECLINED:
      g_value_set_uint (value, self->num_declined);
      break;

    case PROP_NUM_TENTATIVE:
      g_value_set_uint (value, self->num_tentative);
      break;

    case PROP_NUM_DELEGATED:
      g_value_set_uint (value, self->num_delegated);
      break;

    case PROP_NUM_UNKNOWN:
      g_value_set_uint (value, self->num_not_responded);
      break;

    case PROP_NUM_TOTAL:
      g_value_set_uint (value, gcal_attendees_section_get_num_participants (self));
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
   * GcalAttendeesSection:num-accepted:
   *
   * Gets the number of attendees who accepted.
   */
  properties[PROP_NUM_ACCEPTED] =
    g_param_spec_uint ("num-accepted", NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GcalAttendeesSection:num-declined:
   *
   * Gets the number of attendees who declined.
   */
  properties[PROP_NUM_DECLINED] =
    g_param_spec_uint ("num-declined", NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GcalAttendeesSection:num-tentative:
   *
   * Gets the number of attendees who "maybe" will attend.
   */
  properties[PROP_NUM_TENTATIVE] =
    g_param_spec_uint ("num-tentative", NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY  | G_PARAM_STATIC_STRINGS);

  /**
   * GcalAttendeesSection:num-unknown:
   *
   * Gets the number of attendees with an unknown response.
   */
  properties[PROP_NUM_UNKNOWN] =
    g_param_spec_uint ("num-unknown", NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY  | G_PARAM_STATIC_STRINGS);

  /**
   * GcalAttendeesSection: num-delegated:
   *
   * Gets the number of attendees who delegated their attendance to someone else.
   */
  properties[PROP_NUM_DELEGATED] =
    g_param_spec_uint ("num-delegated", NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY  | G_PARAM_STATIC_STRINGS);

  /**
   * GcalAttendeesSection:num-participants:
   *
   * Gets the total number of attendees/participants.
   */
  properties[PROP_NUM_TOTAL] =
    g_param_spec_uint ("num-participants", NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  /**
   * GcalAttendeesSection::summary-row-activated:
   *
   * This signal is emitted when the summary row is activated.
   */
  signals[SUMMARY_ROW_ACTIVATED] =
    g_signal_new ("summary-row-activated",
                  GCAL_TYPE_ATTENDEES_SECTION,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  0);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/event-editor/gcal-attendees-section.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalAttendeesSection, organizer_row);
  gtk_widget_class_bind_template_child (widget_class, GcalAttendeesSection, summary_row);

  gtk_widget_class_bind_template_callback (widget_class, on_participant_summary_row_activated_cb);
}
