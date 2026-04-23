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

#include "gcal-debug.h"
#include "gcal-notes-section.h"
#include "gcal-notes-row.h"

struct _GcalNotesSection
{
  GcalEventEditorSection  parent;

  GcalNotesRow           *notes_row;
};

G_DEFINE_FINAL_TYPE (GcalNotesSection, gcal_notes_section, GCAL_TYPE_EVENT_EDITOR_SECTION)

/*
 * Callbacks
 */

static void
on_notes_row_text_changed_cb (GcalNotesSection *self)
{
  g_autofree char *text = NULL;
  GcalEvent *event;

  event = gcal_event_editor_section_get_event (GCAL_EVENT_EDITOR_SECTION (self));

  text = gcal_notes_row_get_text (self->notes_row);

  gcal_event_set_description (event, text);
}


/*
 * GcalEventEditorSection overrides
 */

static void
gcal_notes_section_event_set_cb (GcalEventEditorSection *section,
                                 GcalEvent              *event)
{
  GcalNotesSection *self = GCAL_NOTES_SECTION (section);
  g_signal_handlers_block_by_func (self->notes_row, on_notes_row_text_changed_cb, self);

  if (event)
    gcal_notes_row_set_text (self->notes_row, gcal_event_get_description (event));
  else
    gcal_notes_row_set_text (self->notes_row, NULL);

  g_signal_handlers_unblock_by_func (self->notes_row, on_notes_row_text_changed_cb, self);
}


/*
 * GObject overrides
 */

static void
gcal_notes_section_class_init (GcalNotesSectionClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GcalEventEditorSectionClass *section_class = GCAL_EVENT_EDITOR_SECTION_CLASS (klass);

  g_type_ensure (GCAL_TYPE_NOTES_ROW);

  section_class->event_set_cb = gcal_notes_section_event_set_cb;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/event-editor/gcal-notes-section.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalNotesSection, notes_row);

  gtk_widget_class_bind_template_callback (widget_class, on_notes_row_text_changed_cb);
}

static void
gcal_notes_section_init (GcalNotesSection *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
