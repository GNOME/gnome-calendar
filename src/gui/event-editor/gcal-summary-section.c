/* gcal-summary-section.c
 *
 * Copyright 2020 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#define G_LOG_DOMAIN "GcalSummarySection"

#include "gcal-context.h"
#include "gcal-debug.h"
#include "gcal-event-editor-section.h"
#include "gcal-expandable-entry.h"
#include "gcal-summary-section.h"

#include <glib/gi18n.h>

struct _GcalSummarySection
{
  GtkBin              parent;

  GtkEntry           *summary_entry;
  GtkEntry           *location_entry;

  GcalContext        *context;
  GcalEvent          *event;
};


static void          gcal_event_editor_section_iface_init        (GcalEventEditorSectionInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GcalSummarySection, gcal_summary_section, GTK_TYPE_BIN,
                         G_IMPLEMENT_INTERFACE (GCAL_TYPE_EVENT_EDITOR_SECTION, gcal_event_editor_section_iface_init))

enum
{
  PROP_0,
  PROP_CONTEXT,
  N_PROPS
};

/*
 * GcalEventEditorSection interface
 */

static void
gcal_reminders_section_set_event (GcalEventEditorSection *section,
                                  GcalEvent              *event,
                                  GcalEventEditorFlags    flags)
{
  GcalSummarySection *self;
  const gchar *summary;

  GCAL_ENTRY;

  self = GCAL_SUMMARY_SECTION (section);

  g_set_object (&self->event, event);

  if (!event)
    GCAL_RETURN ();

  summary = gcal_event_get_summary (event);

  if (g_strcmp0 (summary, "") == 0)
    gtk_entry_set_text (GTK_ENTRY (self->summary_entry), _("Unnamed event"));
  else
    gtk_entry_set_text (GTK_ENTRY (self->summary_entry), summary);

  gtk_entry_set_text (self->location_entry, gcal_event_get_location (event));

  GCAL_EXIT;
}

static void
gcal_reminders_section_apply (GcalEventEditorSection *section)
{
  GcalSummarySection *self;

  GCAL_ENTRY;

  self = GCAL_SUMMARY_SECTION (section);

  gcal_event_set_summary (self->event, gtk_entry_get_text (self->summary_entry));
  gcal_event_set_location (self->event, gtk_entry_get_text (self->location_entry));

  GCAL_EXIT;
}

static void
gcal_event_editor_section_iface_init (GcalEventEditorSectionInterface *iface)
{
  iface->set_event = gcal_reminders_section_set_event;
  iface->apply = gcal_reminders_section_apply;
}


/*
 * GObject overrides
 */

static void
gcal_summary_section_finalize (GObject *object)
{
  GcalSummarySection *self = (GcalSummarySection *)object;

  g_clear_object (&self->context);
  g_clear_object (&self->event);

  G_OBJECT_CLASS (gcal_summary_section_parent_class)->finalize (object);
}

static void
gcal_summary_section_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GcalSummarySection *self = GCAL_SUMMARY_SECTION (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_value_set_object (value, self->context);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_summary_section_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GcalSummarySection *self = GCAL_SUMMARY_SECTION (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_assert (self->context == NULL);
      self->context = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_summary_section_class_init (GcalSummarySectionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  g_type_ensure (GCAL_TYPE_EXPANDABLE_ENTRY);

  object_class->finalize = gcal_summary_section_finalize;
  object_class->get_property = gcal_summary_section_get_property;
  object_class->set_property = gcal_summary_section_set_property;

  g_object_class_override_property (object_class, PROP_CONTEXT, "context");

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/event-editor/gcal-summary-section.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalSummarySection, location_entry);
  gtk_widget_class_bind_template_child (widget_class, GcalSummarySection, summary_entry);
}

static void
gcal_summary_section_init (GcalSummarySection *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
