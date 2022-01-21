/* gcal-manager.h
 *
 * Copyright (C) 2015 Erick Pérez Castellanos <erickpc@gnome.org>
 *
 * gnome-calendar is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * gnome-calendar is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GCAL_MANAGER_H__
#define __GCAL_MANAGER_H__

#include "gcal-calendar.h"
#include "gcal-event.h"
#include "gcal-types.h"

#include <libecal/libecal.h>

G_BEGIN_DECLS

#define GCAL_TYPE_MANAGER (gcal_manager_get_type ())
G_DECLARE_FINAL_TYPE (GcalManager, gcal_manager, GCAL, MANAGER, GObject)

GcalManager*         gcal_manager_new                            (GcalContext        *context);

ESource*             gcal_manager_get_source                     (GcalManager        *self,
                                                                  const gchar        *uid);

GList*               gcal_manager_get_calendars                  (GcalManager        *self);

GListModel*          gcal_manager_get_calendars_model            (GcalManager        *self);

GcalCalendar*        gcal_manager_get_default_calendar           (GcalManager        *self);

void                 gcal_manager_set_default_calendar           (GcalManager        *self,
                                                                  GcalCalendar       *calendar);

GcalTimeline*        gcal_manager_get_timeline                   (GcalManager        *self);

void                 gcal_manager_refresh                        (GcalManager        *self);

void                 gcal_manager_create_event                   (GcalManager        *self,
                                                                  GcalEvent          *event);

void                 gcal_manager_update_event                   (GcalManager           *self,
                                                                  GcalEvent             *event,
                                                                  GcalRecurrenceModType  mod);

void                 gcal_manager_remove_event                   (GcalManager           *self,
                                                                  GcalEvent             *event,
                                                                  GcalRecurrenceModType  mod);

void                 gcal_manager_move_event_to_source           (GcalManager        *self,
                                                                  GcalEvent          *event,
                                                                  ESource            *dest);

gchar*               gcal_manager_add_source                     (GcalManager        *self,
                                                                  const gchar        *name,
                                                                  const gchar        *backend,
                                                                  const gchar        *color);

void                 gcal_manager_save_source                    (GcalManager        *self,
                                                                  ESource            *source);

GPtrArray*           gcal_manager_get_events                     (GcalManager        *self,
                                                                  GDateTime          *range_start,
                                                                  GDateTime          *range_end);

gboolean             gcal_manager_get_synchronizing              (GcalManager        *self);

void                 gcal_manager_startup                        (GcalManager        *self);

G_END_DECLS

#endif /* __GCAL_MANAGER_H__ */
