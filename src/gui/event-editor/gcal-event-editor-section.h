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

#include "gcal-event.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GCAL_TYPE_EVENT_EDITOR_SECTION (gcal_event_editor_section_get_type ())

G_DECLARE_INTERFACE (GcalEventEditorSection, gcal_event_editor_section, GCAL, EVENT_EDITOR_SECTION, GtkBox)

typedef enum
{
  GCAL_EVENT_EDITOR_FLAG_NONE      = 0,
  GCAL_EVENT_EDITOR_FLAG_NEW_EVENT = 1 << 0,
} GcalEventEditorFlags;

struct _GcalEventEditorSectionInterface
{
  GTypeInterface parent;

  void               (*set_event)                                (GcalEventEditorSection *self,
                                                                  GcalEvent              *event,
                                                                  GcalEventEditorFlags    flags);

  void               (*apply)                                    (GcalEventEditorSection *self);

  gboolean           (*changed)                                  (GcalEventEditorSection *self);
};

void                 gcal_event_editor_section_set_event         (GcalEventEditorSection *self,
                                                                  GcalEvent              *event,
                                                                  GcalEventEditorFlags    flags);

void                 gcal_event_editor_section_apply             (GcalEventEditorSection *self);

gboolean             gcal_event_editor_section_changed           (GcalEventEditorSection *self);

G_END_DECLS
