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

#include "e-cal-data-model.h"
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

GcalCalendar*        gcal_manager_get_default_calendar           (GcalManager        *self);

void                 gcal_manager_set_default_calendar           (GcalManager        *self,
                                                                  GcalCalendar       *calendar);

void                 gcal_manager_set_subscriber                 (GcalManager        *self,
                                                                  ECalDataModelSubscriber *subscriber,
                                                                  time_t              range_start,
                                                                  time_t              range_end);

void                 gcal_manager_set_search_subscriber          (GcalManager        *self,
                                                                  ECalDataModelSubscriber *subscriber,
                                                                  time_t              range_start,
                                                                  time_t              range_end);

void                 gcal_manager_set_query                      (GcalManager        *self,
                                                                  const gchar        *query);

gchar*               gcal_manager_query_client_data              (GcalManager        *self,
                                                                  ESource            *source,
                                                                  const gchar        *field);

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

/* GNOME Shell-related functions */
GcalEvent*           gcal_manager_get_event_from_shell_search    (GcalManager        *self,
                                                                  const gchar        *uuid);

void                 gcal_manager_setup_shell_search             (GcalManager             *self,
                                                                  ECalDataModelSubscriber *subscriber);

void                 gcal_manager_set_shell_search_query         (GcalManager        *self,
                                                                  const gchar        *query);

void                 gcal_manager_set_shell_search_subscriber    (GcalManager             *self,
                                                                  ECalDataModelSubscriber *subscriber,
                                                                  time_t                   range_start,
                                                                  time_t                   range_end);

gboolean             gcal_manager_shell_search_done              (GcalManager        *self);

GList*               gcal_manager_get_shell_search_events        (GcalManager        *self);

gboolean             gcal_manager_get_synchronizing              (GcalManager        *self);

void                 gcal_manager_startup                        (GcalManager        *self);

G_END_DECLS

#endif /* __GCAL_MANAGER_H__ */
