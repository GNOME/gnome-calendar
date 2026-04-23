/*
 * gcal-agenda-view-day.h
 *
 * Copyright 2026 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

G_BEGIN_DECLS

#define GCAL_TYPE_AGENDA_VIEW_DAY (gcal_agenda_view_day_get_type())
G_DECLARE_FINAL_TYPE (GcalAgendaViewDay, gcal_agenda_view_day, GCAL, AGENDA_VIEW_DAY, GObject)

GcalAgendaViewDay *gcal_agenda_view_day_new       (void);
GDateTime         *gcal_agenda_view_day_get_date  (GcalAgendaViewDay *self);
void               gcal_agenda_view_day_set_date  (GcalAgendaViewDay *self,
                                                   GDateTime         *date);
GListModel        *gcal_agenda_view_day_get_model (GcalAgendaViewDay *self);
void               gcal_agenda_view_day_set_model (GcalAgendaViewDay *self,
                                                   GListModel        *model);

G_END_DECLS
