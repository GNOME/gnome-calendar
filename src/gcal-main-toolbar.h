/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 2 -*- */
/*
 * gcal-main-toolbar.h
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

#ifndef _GCAL_MAIN_TOOLBAR_H_
#define _GCAL_MAIN_TOOLBAR_H_

#include <clutter-gtk/clutter-gtk.h>

G_BEGIN_DECLS

#define GCAL_TYPE_MAIN_TOOLBAR              (gcal_main_toolbar_get_type ())
#define GCAL_MAIN_TOOLBAR(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCAL_TYPE_MAIN_TOOLBAR, GcalMainToolbar))
#define GCAL_MAIN_TOOLBAR_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GCAL_TYPE_MAIN_TOOLBAR, GcalMainToolbarClass))
#define GCAL_IS_MAIN_TOOLBAR(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCAL_TYPE_MAIN_TOOLBAR))
#define GCAL_IS_MAIN_TOOLBAR_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GCAL_TYPE_MAIN_TOOLBAR))
#define GCAL_MAIN_TOOLBAR_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GCAL_TYPE_MAIN_TOOLBAR, GcalMainToolbarClass))

typedef struct _GcalMainToolbarClass         GcalMainToolbarClass;
typedef struct _GcalMainToolbar              GcalMainToolbar;
typedef struct _GcalMainToolbarPrivate       GcalMainToolbarPrivate;

struct _GcalMainToolbarClass
{
  GtkClutterActorClass parent_class;
};

struct _GcalMainToolbar
{
  GtkClutterActor parent_instance;

  GcalMainToolbarPrivate *priv;
};

GType         gcal_main_toolbar_get_type (void) G_GNUC_CONST;
ClutterActor* gcal_main_toolbar_new      (void);

/* Callbacks */

G_END_DECLS

#endif /* _GCAL_MAIN_TOOLBAR_H_ */
