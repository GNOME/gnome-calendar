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
#include "gcal-summary-section.h"
#include "gcal-utils.h"

#include <glib/gi18n.h>

struct _GcalSummarySection
{
  AdwBin              parent;

  AdwEntryRow        *summary_entry;
  AdwEntryRow        *location_entry;

  GcalContext        *context;
  GcalEvent          *event;
};


static void          gcal_event_editor_section_iface_init        (GcalEventEditorSectionInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GcalSummarySection, gcal_summary_section, ADW_TYPE_BIN,
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

  GCAL_ENTRY;

  self = GCAL_SUMMARY_SECTION (section);

  g_set_object (&self->event, event);

  if (!event)
    GCAL_RETURN ();

  gtk_editable_set_text (GTK_EDITABLE (self->summary_entry), gcal_event_get_summary (event));
  gtk_editable_set_text (GTK_EDITABLE (self->location_entry), gcal_event_get_location (event));

  gtk_widget_grab_focus (GTK_WIDGET (self->summary_entry));

  gtk_widget_remove_css_class (GTK_WIDGET (self->summary_entry), "error");

  GCAL_EXIT;
}

static void
on_summary_entry_text_changed_cb (AdwEntryRow *entry_row)
{
  if (gcal_is_valid_event_name (gtk_editable_get_text (GTK_EDITABLE (entry_row))))
    gtk_widget_remove_css_class (GTK_WIDGET (entry_row), "error");
  else
    gtk_widget_add_css_class (GTK_WIDGET (entry_row), "error");
}

static void
gcal_reminders_section_apply (GcalEventEditorSection *section)
{
  GcalSummarySection *self;

  GCAL_ENTRY;

  self = GCAL_SUMMARY_SECTION (section);

  gcal_event_set_summary (self->event, gtk_editable_get_text (GTK_EDITABLE (self->summary_entry)));
  gcal_event_set_location (self->event, gtk_editable_get_text (GTK_EDITABLE (self->location_entry)));

  GCAL_EXIT;
}

static gboolean
gcal_reminders_section_changed (GcalEventEditorSection *section)
{
  GcalSummarySection *self;
  const gchar *event_location;
  const gchar *event_summary;

  GCAL_ENTRY;

  self = GCAL_SUMMARY_SECTION (section);
  event_summary = gcal_event_get_summary (self->event);
  event_location = gcal_event_get_location (self->event);

  GCAL_RETURN (g_strcmp0 (event_summary, gtk_editable_get_text (GTK_EDITABLE (self->summary_entry))) != 0 ||
               g_strcmp0 (event_location, gtk_editable_get_text (GTK_EDITABLE (self->location_entry))) != 0);
}

static void
gcal_event_editor_section_iface_init (GcalEventEditorSectionInterface *iface)
{
  iface->set_event = gcal_reminders_section_set_event;
  iface->apply = gcal_reminders_section_apply;
  iface->changed = gcal_reminders_section_changed;
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

  object_class->finalize = gcal_summary_section_finalize;
  object_class->get_property = gcal_summary_section_get_property;
  object_class->set_property = gcal_summary_section_set_property;

  g_object_class_override_property (object_class, PROP_CONTEXT, "context");

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/event-editor/gcal-summary-section.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalSummarySection, location_entry);
  gtk_widget_class_bind_template_child (widget_class, GcalSummarySection, summary_entry);

  gtk_widget_class_bind_template_callback (widget_class, on_summary_entry_text_changed_cb);
}

static void
gcal_summary_section_init (GcalSummarySection *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
