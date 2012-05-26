/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 2 -*- */
/*
 * gcal-toolbar.h
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

#ifndef _GCAL_TOOLBAR_H_
#define _GCAL_TOOLBAR_H_

#include "gcal-utils.h"

#include <clutter-gtk/clutter-gtk.h>

G_BEGIN_DECLS

#define GCAL_TYPE_TOOLBAR              (gcal_toolbar_get_type ())
#define GCAL_TOOLBAR(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCAL_TYPE_TOOLBAR, GcalToolbar))
#define GCAL_TOOLBAR_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GCAL_TYPE_TOOLBAR, GcalToolbarClass))
#define GCAL_IS_TOOLBAR(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCAL_TYPE_TOOLBAR))
#define GCAL_IS_TOOLBAR_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GCAL_TYPE_TOOLBAR))
#define GCAL_TOOLBAR_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GCAL_TYPE_TOOLBAR, GcalToolbarClass))

typedef struct _GcalToolbarClass        GcalToolbarClass;
typedef struct _GcalToolbar             GcalToolbar;
typedef struct _GcalToolbarPrivate      GcalToolbarPrivate;

struct _GcalToolbarClass
{
  GtkClutterActorClass parent_class;

  /* Signals */
  void (*view_changed)  (GcalToolbar *toolbar, guint view_type);
  void (*sources_shown) (GcalToolbar *toolbar, gboolean visible);
  void (*add_event)     (GcalToolbar *toolbar);
};

struct _GcalToolbar
{
  GtkClutterActor parent_instance;

  GcalToolbarPrivate *priv;
};

GType         gcal_toolbar_get_type          (void) G_GNUC_CONST;

ClutterActor* gcal_toolbar_new               (void);

void          gcal_toolbar_set_mode          (GcalToolbar     *toolbar,
                                              GcalToolbarMode  mode);

G_END_DECLS

#endif /* _GCAL_TOOLBAR_H_ */
