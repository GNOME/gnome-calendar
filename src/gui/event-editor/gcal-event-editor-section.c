/* gcal-event-editor-section.c
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

#include "gcal-context.h"
#include "gcal-event-editor-section.h"

G_DEFINE_INTERFACE (GcalEventEditorSection, gcal_event_editor_section, GTK_TYPE_WIDGET)

static void
gcal_event_editor_section_default_init (GcalEventEditorSectionInterface *iface)
{
  g_object_interface_install_property (iface,
                                       g_param_spec_object ("context",
                                                            "Context",
                                                            "Context",
                                                            GCAL_TYPE_CONTEXT,
                                                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

/**
 * gcal_event_editor_section_set_event:
 * @self: a #GcalEventEditorSection
 * @event: (nullable): a #GcalEvent
 *
 * Sets the current event to be displayed / edited.
 */
void
gcal_event_editor_section_set_event (GcalEventEditorSection *self,
                                     GcalEvent              *event,
                                     GcalEventEditorFlags    flags)
{
  g_return_if_fail (GCAL_IS_EVENT_EDITOR_SECTION (self));
  g_return_if_fail (GCAL_EVENT_EDITOR_SECTION_GET_IFACE (self)->set_event);

  GCAL_EVENT_EDITOR_SECTION_GET_IFACE (self)->set_event (self, event, flags);
}

/**
 * gcal_event_editor_section_apply:
 * @self: a #GcalEventEditorSection
 *
 * Applies the changes to the event.
 */
void
gcal_event_editor_section_apply (GcalEventEditorSection *self)
{
  g_return_if_fail (GCAL_IS_EVENT_EDITOR_SECTION (self));

  if (GCAL_EVENT_EDITOR_SECTION_GET_IFACE (self)->apply)
    GCAL_EVENT_EDITOR_SECTION_GET_IFACE (self)->apply (self);
}

/**
 * gcal_event_editor_section_changed:
 * @self: a #GcalEventEditorSection
 *
 * Checks whether this section has any pending changes to the event.
 *
 * Returns: %TRUE if this section has pending changes
 */
gboolean
gcal_event_editor_section_changed (GcalEventEditorSection *self)
{
  g_return_val_if_fail (GCAL_IS_EVENT_EDITOR_SECTION (self), FALSE);

  if (GCAL_EVENT_EDITOR_SECTION_GET_IFACE (self)->changed)
    return GCAL_EVENT_EDITOR_SECTION_GET_IFACE (self)->changed (self);
  else
    return TRUE;
}
