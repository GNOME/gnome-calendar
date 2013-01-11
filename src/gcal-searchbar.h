/* -*- mode: c; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gcal-searchbar.h
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

#ifndef _GCAL_SEARCHBAR_H_
#define _GCAL_SEARCHBAR_H_

#include "gcal-utils.h"

#include <clutter-gtk/clutter-gtk.h>

G_BEGIN_DECLS

#define GCAL_TYPE_SEARCHBAR              (gcal_searchbar_get_type ())
#define GCAL_SEARCHBAR(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCAL_TYPE_SEARCHBAR, GcalSearchbar))
#define GCAL_SEARCHBAR_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GCAL_TYPE_SEARCHBAR, GcalSearchbarClass))
#define GCAL_IS_SEARCHBAR(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCAL_TYPE_SEARCHBAR))
#define GCAL_IS_SEARCHBAR_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GCAL_TYPE_SEARCHBAR))
#define GCAL_SEARCHBAR_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GCAL_TYPE_SEARCHBAR, GcalSearchbarClass))

typedef struct _GcalSearchbarClass        GcalSearchbarClass;
typedef struct _GcalSearchbar             GcalSearchbar;
typedef struct _GcalSearchbarPrivate      GcalSearchbarPrivate;

struct _GcalSearchbarClass
{
  GtkClutterActorClass parent_class;

  /* Signals */
  void (*done)          (GcalSearchbar *searchbar);
};

struct _GcalSearchbar
{
  GtkClutterActor parent_instance;

  GcalSearchbarPrivate *priv;
};

GType         gcal_searchbar_get_type          (void) G_GNUC_CONST;

ClutterActor* gcal_searchbar_new               (void);

G_END_DECLS

#endif /* _GCAL_SEARCHBAR_H_ */
