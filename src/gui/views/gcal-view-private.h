/* gcal-view-private.h
 *
 * Copyright 2023 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#include "gcal-view.h"

G_BEGIN_DECLS

void                 gcal_view_create_event                      (GcalView              *self,
                                                                  GcalRange             *range,
                                                                  gdouble                x,
                                                                  gdouble                y);

void                 gcal_view_create_event_detailed             (GcalView              *self,
                                                                  GcalRange             *range);

void                 gcal_view_event_activated                   (GcalView              *self,
                                                                  GcalEventWidget       *event_widget);

G_END_DECLS
