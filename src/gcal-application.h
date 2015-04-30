/* -*- mode: c; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gcal-application.h
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

#ifndef _GCAL_APPLICATION_H_
#define _GCAL_APPLICATION_H_

#include "gcal-manager.h"

#include <gtk/gtk.h>
#include <goa/goa.h>

G_BEGIN_DECLS

#define GCAL_TYPE_APPLICATION               (gcal_application_get_type ())
#define GCAL_APPLICATION(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCAL_TYPE_APPLICATION, GcalApplication))
#define GCAL_APPLICATION_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), GCAL_TYPE_APPLICATION, GcalApplicationClass))
#define GCAL_IS_APPLICATION(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCAL_TYPE_APPLICATION))
#define GCAL_IS_APPLICATION_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), GCAL_TYPE_APPLICATION))
#define GCAL_APPLICATION_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), GCAL_TYPE_APPLICATION, GcalApplicationClass))

typedef struct _GcalApplicationClass         GcalApplicationClass;
typedef struct _GcalApplication              GcalApplication;
typedef struct _GcalApplicationPrivate       GcalApplicationPrivate;

struct _GcalApplication
{
  GtkApplication parent;

  /*< private >*/
  GcalApplicationPrivate *priv;
};

struct _GcalApplicationClass
{
  GtkApplicationClass parent_class;
};

GType             gcal_application_get_type     (void) G_GNUC_CONST;
GcalApplication*  gcal_application_new          (void);
GcalManager*      gcal_application_get_manager  (GcalApplication *app);
GSettings*        gcal_application_get_settings (GcalApplication *app);
void              gcal_application_set_uuid     (GcalApplication *application,
                                                 const gchar     *uuid);
void              gcal_application_set_initial_date (GcalApplication *application,
                                                     const icaltimetype *date);
GoaClient*        gcal_application_get_client   (GcalApplication *application);

G_END_DECLS

#endif /* _GCAL_APPLICATION_H_ */
