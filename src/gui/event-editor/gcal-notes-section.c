/* gcal-notes-section.c
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

#define G_LOG_DOMAIN "GcalNotesSection"

#include "gcal-context.h"
#include "gcal-debug.h"
#include "gcal-event-editor-section.h"
#include "gcal-notes-section.h"

struct _GcalNotesSection
{
  AdwPreferencesRow   parent;

  GtkTextView        *notes_text;

  GcalContext        *context;
  GcalEvent          *event;
};

static void          gcal_event_editor_section_iface_init        (GcalEventEditorSectionInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GcalNotesSection, gcal_notes_section, ADW_TYPE_PREFERENCES_ROW,
                         G_IMPLEMENT_INTERFACE (GCAL_TYPE_EVENT_EDITOR_SECTION, gcal_event_editor_section_iface_init))

enum
{
  PROP_0,
  PROP_CONTEXT,
  N_PROPS
};

/*
 * Callbacks
 */

static void
on_notes_text_state_flags_changed_cb (GtkTextView      *text_view,
                                      GtkStateFlags     previous_flags,
                                      GcalNotesSection *self)
{
  GtkStateFlags flags = gtk_widget_get_state_flags (GTK_WIDGET (text_view));

  if (flags & GTK_STATE_FLAG_FOCUS_WITHIN)
    gtk_widget_add_css_class (GTK_WIDGET (self), "focused");
  else
    gtk_widget_remove_css_class (GTK_WIDGET (self), "focused");
}


/*
 * GcalEventEditorSection interface
 */

static void
gcal_notes_section_set_event (GcalEventEditorSection *section,
                              GcalEvent              *event,
                              GcalEventEditorFlags    flags)
{
  GcalNotesSection *self;
  GtkTextBuffer *buffer;

  GCAL_ENTRY;

  self = GCAL_NOTES_SECTION (section);

  g_set_object (&self->event, event);

  if (!event)
    GCAL_RETURN ();

  buffer = gtk_text_view_get_buffer (self->notes_text);
  gtk_text_buffer_set_text (buffer, gcal_event_get_description (event), -1);

  GCAL_EXIT;
}

static void
gcal_notes_section_apply (GcalEventEditorSection *section)
{
  g_autofree gchar *note_text = NULL;
  GcalNotesSection *self;
  GtkTextBuffer *buffer;

  GCAL_ENTRY;

  self = GCAL_NOTES_SECTION (section);

  /* Update description */
  buffer = gtk_text_view_get_buffer (self->notes_text);
  g_object_get (G_OBJECT (buffer), "text", &note_text, NULL);

  gcal_event_set_description (self->event, note_text);

  GCAL_EXIT;
}

static gboolean
gcal_notes_section_changed (GcalEventEditorSection *section)
{
  g_autofree gchar *note_text = NULL;
  GcalNotesSection *self;
  GtkTextBuffer *buffer;

  GCAL_ENTRY;

  self = GCAL_NOTES_SECTION (section);

  /* Update description */
  buffer = gtk_text_view_get_buffer (self->notes_text);
  g_object_get (G_OBJECT (buffer), "text", &note_text, NULL);

  GCAL_RETURN (g_strcmp0 (gcal_event_get_description (self->event), note_text) != 0);
}

static void
gcal_event_editor_section_iface_init (GcalEventEditorSectionInterface *iface)
{
  iface->set_event = gcal_notes_section_set_event;
  iface->apply = gcal_notes_section_apply;
  iface->changed = gcal_notes_section_changed;
}


/*
 * GObject overrides
 */

static void
gcal_notes_section_finalize (GObject *object)
{
  GcalNotesSection *self = (GcalNotesSection *)object;

  g_clear_object (&self->context);
  g_clear_object (&self->event);

  G_OBJECT_CLASS (gcal_notes_section_parent_class)->finalize (object);
}

static void
gcal_notes_section_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GcalNotesSection *self = GCAL_NOTES_SECTION (object);

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
gcal_notes_section_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GcalNotesSection *self = GCAL_NOTES_SECTION (object);

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
gcal_notes_section_class_init (GcalNotesSectionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gcal_notes_section_finalize;
  object_class->get_property = gcal_notes_section_get_property;
  object_class->set_property = gcal_notes_section_set_property;

  g_object_class_override_property (object_class, PROP_CONTEXT, "context");

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/event-editor/gcal-notes-section.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalNotesSection, notes_text);

  gtk_widget_class_bind_template_callback (widget_class, on_notes_text_state_flags_changed_cb);
}

static void
gcal_notes_section_init (GcalNotesSection *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_widget_remove_css_class (GTK_WIDGET (self->notes_text), "view");
}
