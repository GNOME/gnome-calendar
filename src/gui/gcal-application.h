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

#include "gcal-clock.h"
#include "gcal-context.h"
#include "gcal-manager.h"

#include <adwaita.h>

G_BEGIN_DECLS

#define GCAL_TYPE_APPLICATION (gcal_application_get_type ())
G_DECLARE_FINAL_TYPE (GcalApplication, gcal_application, GCAL, APPLICATION, AdwApplication)

GcalApplication*     gcal_application_new                        (void);

GcalContext*         gcal_application_get_context                (GcalApplication    *self);

void                 gcal_application_set_uuid                   (GcalApplication    *self,
                                                                  const gchar        *app_uuid);

void                 gcal_application_set_initial_date           (GcalApplication    *self,
                                                                  GDateTime          *initial_date);

G_END_DECLS

#endif /* _GCAL_APPLICATION_H_ */
