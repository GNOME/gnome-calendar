/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 2 -*- */
/*
 * gcal-manager.h
 * Copyright (C) 2012 Erick Pérez Castellanos <erickpc@gnome.org>
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

#include <gtk/gtk.h>
#include <glib-object.h>

#include <icaltime.h>

G_BEGIN_DECLS

#define GCAL_TYPE_MANAGER                            (gcal_manager_get_type ())
#define GCAL_MANAGER(obj)                            (G_TYPE_CHECK_INSTANCE_CAST((obj), GCAL_TYPE_MANAGER, GcalManager))
#define GCAL_MANAGER_CLASS(klass)                    (G_TYPE_CHECK_CLASS_CAST((klass), GCAL_TYPE_MANAGER, GcalManagerClass))
#define GCAL_IS_MANAGER(obj)                         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GCAL_TYPE_MANAGER))
#define GCAL_IS_MANAGER_CLASS(klass)                 (G_TYPE_CHECK_CLASS_TYPE((klass), GCAL_TYPE_MANAGER))
#define GCAL_MANAGER_GET_CLASS(obj)                  (G_TYPE_INSTANCE_GET_CLASS((obj), GCAL_TYPE_MANAGER, GcalManagerClass))

typedef struct _GcalManager                           GcalManager;
typedef struct _GcalManagerClass                      GcalManagerClass;
typedef struct _GcalManagerPrivate                    GcalManagerPrivate;

struct _GcalManager
{
  GObject parent;
  /* add your public declarations here */

  GcalManagerPrivate *priv;
};

struct _GcalManagerClass
{
  GObjectClass parent_class;

  /* signals */
  void (* events_added)    (GcalManager *manager, const GList *events);
  void (* events_modified) (GcalManager *manager, const GList *events);
  void (* events_removed)  (GcalManager *manager, const GList *uids);

};

GType          gcal_manager_get_type                (void);

GcalManager*   gcal_manager_new                     (void);

GtkListStore*  gcal_manager_get_sources_model       (GcalManager        *manager);

icaltimezone*  gcal_manager_get_system_timezone     (GcalManager        *manager);

gchar*         gcal_manager_add_source              (GcalManager        *manager,
                                                     const gchar        *name,
                                                     const gchar        *backend,
                                                     const gchar        *color);

const gchar*   gcal_manager_get_source_name         (GcalManager        *manager,
                                                     const gchar        *source_uid);

void           gcal_manager_set_new_range           (GcalManager        *manager,
                                                     const icaltimetype *initial_date,
                                                     const icaltimetype *final_date);

gboolean       gcal_manager_exists_event            (GcalManager        *manager,
                                                     const gchar        *source_uid,
                                                     const gchar        *event_uid);

icaltimetype*  gcal_manager_get_event_start_date    (GcalManager        *manager,
                                                     const gchar        *source_uid,
                                                     const gchar        *event_uid);

gchar*         gcal_manager_get_event_summary       (GcalManager        *manager,
                                                     const gchar        *source_uid,
                                                     const gchar        *event_uid);

gchar**        gcal_manager_get_event_organizer     (GcalManager        *manager,
                                                     const gchar        *source_uid,
                                                     const gchar        *event_uid);

const gchar*   gcal_manager_get_event_location      (GcalManager        *manager,
                                                     const gchar        *source_uid,
                                                     const gchar        *event_uid);

gchar*         gcal_manager_get_event_description   (GcalManager        *manager,
                                                     const gchar        *source_uid,
                                                     const gchar        *event_uid);

GdkRGBA*       gcal_manager_get_event_color         (GcalManager        *manager,
                                                     const gchar        *source_uid,
                                                     const gchar        *event_uid);

GList*         gcal_manager_get_event_reminders     (GcalManager        *manager,
                                                     const gchar        *source_uid,
                                                     const gchar        *event_uid);

gboolean       gcal_manager_has_event_reminders     (GcalManager        *manager,
                                                     const gchar        *source_uid,
                                                     const gchar        *event_uid);

gboolean       gcal_manager_get_event_all_day       (GcalManager        *manager,
                                                     const gchar        *source_uid,
                                                     const gchar        *event_uid);

void           gcal_manager_remove_event            (GcalManager        *manager,
                                                     const gchar        *source_uid,
                                                     const gchar        *event_uid);

void           gcal_manager_create_event            (GcalManager        *manager,
                                                     const gchar        *source_uid,
                                                     const gchar        *summary,
                                                     const icaltimetype *initial_date,
                                                     const icaltimetype *final_date);

G_END_DECLS

#endif /* __GCAL_MANAGER_H__ */
