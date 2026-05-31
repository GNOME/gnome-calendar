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

struct _GcalNotesSection
{
  GcalEventEditorSection  parent;

  GtkTextView            *notes_text;
  GtkLabel               *placeholder;
};

G_DEFINE_FINAL_TYPE (GcalNotesSection, gcal_notes_section, GCAL_TYPE_EVENT_EDITOR_SECTION)

/*
 * Callbacks
 */

static void
on_notes_text_changed_cb (GcalNotesSection *self)
{
  g_autofree char *text = NULL;
  GtkTextIter start, end;
  GtkTextBuffer *buffer;
  GcalEvent *event;

  event = gcal_event_editor_section_get_event (GCAL_EVENT_EDITOR_SECTION (self));

  buffer = gtk_text_view_get_buffer (self->notes_text);
  gtk_text_buffer_get_bounds (buffer, &start, &end);
  text = gtk_text_buffer_get_text (buffer, &start, &end, TRUE);

  gcal_event_set_description (event, text);
}

static void
on_row_state_flags_changed_cb (AdwPreferencesRow *row,
                               GtkStateFlags      previous_flags)
{
  GtkStateFlags flags = gtk_widget_get_state_flags (GTK_WIDGET (row));

  if (flags & GTK_STATE_FLAG_FOCUS_WITHIN)
    gtk_widget_add_css_class (GTK_WIDGET (row), "focused");
  else
    gtk_widget_remove_css_class (GTK_WIDGET (row), "focused");
}

static gboolean
empty_string (gpointer     user_data,
              const gchar *text)
{
  return g_utf8_strlen (text, -1) == 0;
}

static void
on_row_pressed (GtkGestureClick *gesture,
                gint             n_press,
                gdouble          x,
                gdouble          y,
                GtkWidget       *text_view)
{
  GtkWidget *row;

  g_assert (GTK_IS_TEXT_VIEW (text_view));

  row = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));

  if (gtk_widget_contains (row, x, y))
    gtk_widget_grab_focus (text_view);

  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_NONE);
}


/*
 * GcalEventEditorSection overrides
 */

static void
gcal_notes_section_event_set_cb (GcalEventEditorSection *section,
                                 GcalEvent              *event)
{
  GcalNotesSection *self = GCAL_NOTES_SECTION (section);
  GtkTextIter start, end;
  GtkTextBuffer *buffer;

  buffer = gtk_text_view_get_buffer (self->notes_text);
  gtk_text_buffer_get_bounds (buffer, &start, &end);

  g_signal_handlers_block_by_func (buffer, on_notes_text_changed_cb, self);

  if (event)
    gtk_text_buffer_set_text (buffer, gcal_event_get_description (event), -1);
  else
    gtk_text_buffer_delete (buffer, &start, &end);

  g_signal_handlers_unblock_by_func (buffer, on_notes_text_changed_cb, self);
}


/*
 * GObject overrides
 */

static void
gcal_notes_section_class_init (GcalNotesSectionClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GcalEventEditorSectionClass *section_class = GCAL_EVENT_EDITOR_SECTION_CLASS (klass);

  section_class->event_set_cb = gcal_notes_section_event_set_cb;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/event-editor/gcal-notes-section.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalNotesSection, notes_text);
  gtk_widget_class_bind_template_child (widget_class, GcalNotesSection, placeholder);

  gtk_widget_class_bind_template_callback (widget_class, empty_string);
  gtk_widget_class_bind_template_callback (widget_class, on_row_pressed);
  gtk_widget_class_bind_template_callback (widget_class, on_row_state_flags_changed_cb);
}

static void
gcal_notes_section_init (GcalNotesSection *self)
{
  GtkTextBuffer *buffer;

  gtk_widget_init_template (GTK_WIDGET (self));

  buffer = gtk_text_view_get_buffer (self->notes_text);
  g_signal_connect_object (buffer, "notify::text", G_CALLBACK (on_notes_text_changed_cb), self, G_CONNECT_SWAPPED);

  gtk_widget_remove_css_class (GTK_WIDGET (self->notes_text), "view");
}
