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

#include "gcal-debug.h"
#include "gcal-summary-section.h"
#include "gcal-utils.h"

#include <glib/gi18n.h>

struct _GcalSummarySection
{
  GcalEventEditorSection  parent;

  AdwEntryRow            *summary_entry;
  AdwEntryRow            *location_entry;
};


G_DEFINE_FINAL_TYPE (GcalSummarySection, gcal_summary_section, GCAL_TYPE_EVENT_EDITOR_SECTION)

/*
 * Callbacks
 */

static void
on_summary_entry_text_changed_cb (GcalSummarySection *self)
{
  const char *text = gtk_editable_get_text (GTK_EDITABLE (self->summary_entry));
  GcalEvent *event;

  event = gcal_event_editor_section_get_event (GCAL_EVENT_EDITOR_SECTION (self));

  if (gcal_is_valid_event_name (text))
    gtk_widget_remove_css_class (GTK_WIDGET (self->summary_entry), "error");
  else
    gtk_widget_add_css_class (GTK_WIDGET (self->summary_entry), "error");

  gcal_event_set_summary (event, text);
}

static void
on_location_entry_changed_cb (GcalSummarySection *self)
{
  const char *text = gtk_editable_get_text (GTK_EDITABLE (self->location_entry));
  GcalEvent *event;

  event = gcal_event_editor_section_get_event (GCAL_EVENT_EDITOR_SECTION (self));

  gcal_event_set_location (event, text);
}


/*
 * GcalEventEditorSection overrides
 */

static void
gcal_summary_section_event_set_cb (GcalEventEditorSection *section,
                                GcalEvent              *event)
{
  GcalSummarySection *self = GCAL_SUMMARY_SECTION (section);

  g_signal_handlers_block_by_func (self->summary_entry, on_summary_entry_text_changed_cb, self);
  g_signal_handlers_block_by_func (self->location_entry, on_location_entry_changed_cb, self);

  if (event)
    {
      gtk_editable_set_text (GTK_EDITABLE (self->summary_entry), gcal_event_get_summary (event));
      gtk_editable_set_text (GTK_EDITABLE (self->location_entry), gcal_event_get_location (event));
    }
  else
    {
      gtk_editable_delete_text (GTK_EDITABLE (self->summary_entry), 0, -1);
      gtk_editable_delete_text (GTK_EDITABLE (self->location_entry), 0, -1);
    }

  g_signal_handlers_unblock_by_func (self->summary_entry, on_summary_entry_text_changed_cb, self);
  g_signal_handlers_unblock_by_func (self->location_entry, on_location_entry_changed_cb, self);

  gtk_widget_remove_css_class (GTK_WIDGET (self->summary_entry), "error");
}


/*
 * GtkWidget overrides
 */

static gboolean
gcal_summary_section_grab_focus (GtkWidget *widget)
{
  GcalSummarySection *self = GCAL_SUMMARY_SECTION (widget);

  return gtk_widget_grab_focus (GTK_WIDGET (self->summary_entry));
}

static void
gcal_summary_section_unmap (GtkWidget *widget)
{
  GcalSummarySection *self = GCAL_SUMMARY_SECTION (widget);

  gtk_widget_remove_css_class (GTK_WIDGET (self->summary_entry), "error");

  GTK_WIDGET_CLASS (gcal_summary_section_parent_class)->unmap (widget);
}


/*
 * GObject overrides
 */

static void
gcal_summary_section_class_init (GcalSummarySectionClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GcalEventEditorSectionClass *section_class = GCAL_EVENT_EDITOR_SECTION_CLASS (klass);

  widget_class->grab_focus = gcal_summary_section_grab_focus;
  widget_class->unmap = gcal_summary_section_unmap;

  section_class->event_set_cb = gcal_summary_section_event_set_cb;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/event-editor/gcal-summary-section.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalSummarySection, location_entry);
  gtk_widget_class_bind_template_child (widget_class, GcalSummarySection, summary_entry);

  gtk_widget_class_bind_template_callback (widget_class, on_summary_entry_text_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_location_entry_changed_cb);
}

static void
gcal_summary_section_init (GcalSummarySection *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
