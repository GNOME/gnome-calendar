/* gcal-event-editor-section.h
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

#pragma once

#include <adwaita.h>

#include "gcal-event.h"

G_BEGIN_DECLS

#define GCAL_TYPE_EVENT_EDITOR_SECTION (gcal_event_editor_section_get_type())

G_DECLARE_DERIVABLE_TYPE (GcalEventEditorSection, gcal_event_editor_section, GCAL, EVENT_EDITOR_SECTION, AdwPreferencesGroup)

struct _GcalEventEditorSectionClass
{
  AdwPreferencesGroupClass parent_class;

  /**
   * GcalEventEditorSectionClass::event_set_cb:
   * @self: a #GcalEventEditorSection
   * @event: (nullable) (transfer none): a #GcalEvent
   *
   * Invoked when a new event has been set.
   *
   * Subclasses must implement this virtual method and update its children
   * to reflect the new event.
   */
  void (* event_set_cb) (GcalEventEditorSection *self,
                         GcalEvent              *event);
};

GcalEvent *gcal_event_editor_section_get_event  (GcalEventEditorSection *self);
GtkWidget *gcal_event_editor_section_get_dialog (GcalEventEditorSection *self);
void       gcal_event_editor_section_set_dialog (GcalEventEditorSection *self,
                                                 GtkWidget              *dialog);

G_END_DECLS
