/* -*- mode: c; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gcal-manager.h
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
#include "gcal-event.h"

#include <libical/icaltime.h>
#include <goa/goa.h>

G_BEGIN_DECLS

#define GCAL_TYPE_MANAGER                                        (gcal_manager_get_type ())

G_DECLARE_FINAL_TYPE (GcalManager, gcal_manager, GCAL, MANAGER, GObject)

GcalManager*         gcal_manager_new_with_settings              (GSettings          *settings);

ESource*             gcal_manager_get_source                     (GcalManager        *manager,
                                                                  const gchar        *uid);

GList*               gcal_manager_get_sources                    (GcalManager        *manager);

GList*               gcal_manager_get_sources_connected          (GcalManager        *manager);

ESource*             gcal_manager_get_default_source             (GcalManager        *manager);

void                 gcal_manager_set_default_source             (GcalManager        *manager,
                                                                  ESource            *source);

icaltimezone*        gcal_manager_get_system_timezone            (GcalManager        *manager);

void                 gcal_manager_set_subscriber                 (GcalManager        *manager,
                                                                  ECalDataModelSubscriber *subscriber,
                                                                  time_t              range_start,
                                                                  time_t              range_end);

void                 gcal_manager_set_search_subscriber          (GcalManager        *manager,
                                                                  ECalDataModelSubscriber *subscriber,
                                                                  time_t              range_start,
                                                                  time_t              range_end);

void                 gcal_manager_set_query                      (GcalManager        *self,
                                                                  const gchar        *query);

gchar*               gcal_manager_query_client_data              (GcalManager        *manager,
                                                                  ESource            *source,
                                                                  const gchar        *field);

void                 gcal_manager_refresh                        (GcalManager        *manager);

gboolean             gcal_manager_is_client_writable             (GcalManager        *manager,
                                                                  ESource            *source);

void                 gcal_manager_create_event                   (GcalManager        *manager,
                                                                  GcalEvent          *event);

void                 gcal_manager_update_event                   (GcalManager        *manager,
                                                                  GcalEvent          *event);

void                 gcal_manager_remove_event                   (GcalManager        *manager,
                                                                  GcalEvent          *event);

void                 gcal_manager_move_event_to_source           (GcalManager        *manager,
                                                                  GcalEvent          *event,
                                                                  ESource            *dest);

gchar*               gcal_manager_add_source                     (GcalManager        *manager,
                                                                  const gchar        *name,
                                                                  const gchar        *backend,
                                                                  const gchar        *color);

void                 gcal_manager_enable_source                  (GcalManager        *manager,
                                                                  ESource            *source);

void                 gcal_manager_disable_source                 (GcalManager        *manager,
                                                                  ESource            *source);

void                 gcal_manager_save_source                    (GcalManager        *manager,
                                                                  ESource            *source);

gboolean             gcal_manager_source_enabled                 (GcalManager        *manager,
                                                                  ESource            *source);

GList*               gcal_manager_get_events                     (GcalManager        *manager,
                                                                  icaltimetype       *range_start,
                                                                  icaltimetype       *range_end);

gboolean             gcal_manager_get_loading                    (GcalManager        *self);

/* Online Accounts */
GoaClient*           gcal_manager_get_goa_client                 (GcalManager        *manager);


/* GNOME Shell-related functions */
GcalEvent*           gcal_manager_get_event_from_shell_search    (GcalManager        *manager,
                                                                  const gchar        *uuid);

void                 gcal_manager_setup_shell_search             (GcalManager             *self,
                                                                  ECalDataModelSubscriber *subscriber);

void                 gcal_manager_set_shell_search_query         (GcalManager        *manager,
                                                                  const gchar        *query);

void                 gcal_manager_set_shell_search_subscriber    (GcalManager             *manager,
                                                                  ECalDataModelSubscriber *subscriber,
                                                                  time_t                   range_start,
                                                                  time_t                   range_end);

gboolean             gcal_manager_shell_search_done              (GcalManager        *manager);

GList*               gcal_manager_get_shell_search_events        (GcalManager        *manager);



G_END_DECLS

#endif /* __GCAL_MANAGER_H__ */
