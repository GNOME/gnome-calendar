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

#include <libical/icaltime.h>

G_BEGIN_DECLS

#define GCAL_TYPE_MANAGER                            (gcal_manager_get_type ())
#define GCAL_MANAGER(obj)                            (G_TYPE_CHECK_INSTANCE_CAST((obj), GCAL_TYPE_MANAGER, GcalManager))
#define GCAL_MANAGER_CLASS(klass)                    (G_TYPE_CHECK_CLASS_CAST((klass), GCAL_TYPE_MANAGER, GcalManagerClass))
#define GCAL_IS_MANAGER(obj)                         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GCAL_TYPE_MANAGER))
#define GCAL_IS_MANAGER_CLASS(klass)                 (G_TYPE_CHECK_CLASS_TYPE((klass), GCAL_TYPE_MANAGER))
#define GCAL_MANAGER_GET_CLASS(obj)                  (G_TYPE_INSTANCE_GET_CLASS((obj), GCAL_TYPE_MANAGER, GcalManagerClass))

typedef struct _GcalManager                           GcalManager;
typedef struct _GcalManagerClass                      GcalManagerClass;

struct _GcalManager
{
  GObject parent;
};

struct _GcalManagerClass
{
  GObjectClass parent_class;

  /* signals */
  void (*source_activated)  (GcalManager *manager, ESource *source, gboolean active);
  void (*source_added)  (GcalManager *manager, ESource *source, gboolean enabled);
  void (*source_removed)  (GcalManager *manager, ESource *source);
  void (*load_completed)  (GcalManager *manager);
  void (*query_completed) (GcalManager *manager);
};

typedef struct
{
  ESource       *source;
  ECalComponent *event_component;
} GcalEventData;

GType          gcal_manager_get_type                (void);

GcalManager*   gcal_manager_new_with_settings       (GSettings          *settings);

ESource*       gcal_manager_get_source              (GcalManager        *manager,
                                                     const gchar        *uid);

GList*         gcal_manager_get_sources             (GcalManager        *manager);

GList*         gcal_manager_get_sources_connected   (GcalManager        *manager);

ESource*       gcal_manager_get_default_source      (GcalManager        *manager);

void           gcal_manager_set_default_source      (GcalManager        *manager,
                                                     ESource            *source);

icaltimezone*  gcal_manager_get_system_timezone     (GcalManager        *manager);

void           gcal_manager_setup_shell_search      (GcalManager             *manager,
                                                     ECalDataModelSubscriber *subscriber);

void           gcal_manager_set_shell_search_query  (GcalManager        *manager,
                                                     const gchar        *query);

void           gcal_manager_set_shell_search_subscriber (GcalManager             *manager,
                                                         ECalDataModelSubscriber *subscriber,
                                                         time_t                   range_start,
                                                         time_t                   range_end);

gboolean       gcal_manager_shell_search_done       (GcalManager        *manager);

GList*         gcal_manager_get_shell_search_events (GcalManager        *manager);

void           gcal_manager_set_subscriber          (GcalManager        *manager,
                                                     ECalDataModelSubscriber *subscriber,
                                                     time_t              range_start,
                                                     time_t              range_end);

void           gcal_manager_set_search_subscriber   (GcalManager        *manager,
                                                     ECalDataModelSubscriber *subscriber,
                                                     time_t              range_start,
                                                     time_t              range_end);

void           gcal_manager_set_query               (GcalManager        *manager,
                                                     const gchar        *query);

gchar*         gcal_manager_add_source              (GcalManager        *manager,
                                                     const gchar        *name,
                                                     const gchar        *backend,
                                                     const gchar        *color);

void           gcal_manager_enable_source           (GcalManager        *manager,
                                                     ESource            *source);

void           gcal_manager_disable_source          (GcalManager        *manager,
                                                     ESource            *source);

void           gcal_manager_save_source             (GcalManager        *manager,
                                                     ESource            *source);

gboolean       gcal_manager_source_enabled          (GcalManager        *manager,
                                                     ESource            *source);

void           gcal_manager_refresh                 (GcalManager        *manager);

gboolean       gcal_manager_is_client_writable      (GcalManager        *manager,
                                                     ESource            *source);

/* Create method */
void           gcal_manager_create_event            (GcalManager        *manager,
                                                     ESource            *source,
                                                     ECalComponent      *component);

/* Update method */
void           gcal_manager_update_event            (GcalManager        *manager,
                                                     ESource            *source,
                                                     ECalComponent      *component);

/* Remove method */
void           gcal_manager_remove_event            (GcalManager        *manager,
                                                     ESource            *source,
                                                     ECalComponent      *component);

/* Set methods */
void           gcal_manager_move_event_to_source    (GcalManager        *manager,
                                                     const gchar        *source_uid,
                                                     const gchar        *event_uid,
                                                     const gchar        *new_source_uid);

GList*         gcal_manager_get_events              (GcalManager        *manager,
                                                     icaltimetype       *range_start,
                                                     icaltimetype       *range_end);

gboolean       gcal_manager_load_completed          (GcalManager        *manager);

GcalEventData* gcal_manager_get_event_from_shell_search (GcalManager        *manager,
                                                         const gchar        *uuid);

G_END_DECLS

#endif /* __GCAL_MANAGER_H__ */
