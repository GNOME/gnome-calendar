/*
 * gcal-agenda-view-item.h
 *
 * Copyright 2026 Zelda Ahmed <zoeyahmed10@proton.me>
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
#include "gcal-event-widget.h"

#include <glib-object.h>

G_BEGIN_DECLS

#define GCAL_TYPE_AGENDA_VIEW_ITEM (gcal_agenda_view_item_get_type())
G_DECLARE_FINAL_TYPE (GcalAgendaViewItem, gcal_agenda_view_item, GCAL, AGENDA_VIEW_ITEM, GObject)

GcalAgendaViewItem *gcal_agenda_view_item_new              (void);
GcalEvent          *gcal_agenda_view_item_get_event        (GcalAgendaViewItem *self);
void                gcal_agenda_view_item_set_event        (GcalAgendaViewItem *self,
                                                            GcalEvent          *event);
GcalEventWidget    *gcal_agenda_view_item_get_event_widget (GcalAgendaViewItem *self);
void                gcal_agenda_view_item_set_event_widget (GcalAgendaViewItem *self,
                                                            GcalEventWidget    *widget);

G_END_DECLS
