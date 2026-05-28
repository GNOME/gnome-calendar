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
#include "gcal-event-editor-dialog.h"
#include "gcal-event-editor-section.h"
#include "gcal-notes-section.h"

struct _GcalNotesSection
{
  AdwPreferencesGroup parent;

  GtkTextView        *notes_text;
  GtkLabel           *placeholder;
};

G_DEFINE_FINAL_TYPE (GcalNotesSection, gcal_notes_section, ADW_TYPE_PREFERENCES_GROUP)

/*
 * Callbacks
 */

static void
on_notes_text_changed_cb (GcalNotesSection *self)
{
  g_autofree char *text = NULL;
  GtkTextIter start, end;
  GtkTextBuffer *buffer;
  GtkWidget *dialog;
  GcalEvent *event;

  dialog = gtk_widget_get_ancestor (GTK_WIDGET (self), GCAL_TYPE_EVENT_EDITOR_DIALOG);

  g_assert (GCAL_IS_EVENT_EDITOR_DIALOG (dialog));

  event = gcal_event_editor_dialog_get_event (GCAL_EVENT_EDITOR_DIALOG (dialog));

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
 * GtkWidget overrides
 */

static void
gcal_notes_section_map (GtkWidget *widget)
{
  GcalNotesSection *self = GCAL_NOTES_SECTION (widget);
  GtkTextBuffer *buffer;
  GtkWidget *dialog;
  GcalEvent *event;

  dialog = gtk_widget_get_ancestor (widget, GCAL_TYPE_EVENT_EDITOR_DIALOG);

  g_assert (GCAL_IS_EVENT_EDITOR_DIALOG (dialog));

  event = gcal_event_editor_dialog_get_event (GCAL_EVENT_EDITOR_DIALOG (dialog));

  buffer = gtk_text_view_get_buffer (self->notes_text);

  g_signal_handlers_block_by_func (buffer, on_notes_text_changed_cb, self);

  gtk_text_buffer_set_text (buffer, gcal_event_get_description (event), -1);

  g_signal_handlers_unblock_by_func (buffer, on_notes_text_changed_cb, self);

  GTK_WIDGET_CLASS (gcal_notes_section_parent_class)->map (widget);
}

static void
gcal_notes_section_unmap (GtkWidget *widget)
{
  GcalNotesSection *self = GCAL_NOTES_SECTION (widget);
  GtkTextIter start, end;
  GtkTextBuffer *buffer;
  GtkWidget *dialog;

  dialog = gtk_widget_get_ancestor (widget, GCAL_TYPE_EVENT_EDITOR_DIALOG);

  g_assert (GCAL_IS_EVENT_EDITOR_DIALOG (dialog));

  buffer = gtk_text_view_get_buffer (self->notes_text);
  gtk_text_buffer_get_bounds (buffer, &start, &end);

  g_signal_handlers_block_by_func (buffer, on_notes_text_changed_cb, self);

  gtk_text_buffer_delete (buffer, &start, &end);

  g_signal_handlers_unblock_by_func (buffer, on_notes_text_changed_cb, self);

  GTK_WIDGET_CLASS (gcal_notes_section_parent_class)->unmap (widget);
}


/*
 * GObject overrides
 */

static void
gcal_notes_section_class_init (GcalNotesSectionClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->map = gcal_notes_section_map;
  widget_class->unmap = gcal_notes_section_unmap;

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
