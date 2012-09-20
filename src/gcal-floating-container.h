/* -*- mode: c; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gcal-floating-container.h
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

#ifndef __GCAL_FLOATING_CONTAINER_H__
#define __GCAL_FLOATING_CONTAINER_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS


#define GCAL_TYPE_FLOATING_CONTAINER                (gcal_floating_container_get_type ())
#define GCAL_FLOATING_CONTAINER(obj)                (G_TYPE_CHECK_INSTANCE_CAST((obj), GCAL_TYPE_FLOATING_CONTAINER, GcalFloatingContainer))
#define GCAL_FLOATING_CONTAINER_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST((klass), GCAL_TYPE_FLOATING_CONTAINER, GcalFloatingContainerClass))
#define GCAL_IS_FLOATING_CONTAINER(obj)             (G_TYPE_CHECK_INSTANCE_TYPE((obj), GCAL_TYPE_FLOATING_CONTAINER))
#define GCAL_IS_FLOATING_CONTAINER_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE((klass), GCAL_TYPE_FLOATING_CONTAINER))
#define GCAL_FLOATING_CONTAINER_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS((obj), GCAL_TYPE_FLOATING_CONTAINER, GcalFloatingContainerClass))

typedef struct _GcalFloatingContainer                GcalFloatingContainer;
typedef struct _GcalFloatingContainerClass           GcalFloatingContainerClass;
typedef struct _GcalFloatingContainerPrivate         GcalFloatingContainerPrivate;

struct _GcalFloatingContainer
{
  GtkBin parent;
  /* add your public declarations here */

  GcalFloatingContainerPrivate *priv;
};

struct _GcalFloatingContainerClass
{
  GtkBinClass parent_class;
};


GType        gcal_floating_container_get_type        (void);
GtkWidget*   gcal_floating_container_new             (void);


G_END_DECLS

#endif /* __GCAL_FLOATING_CONTAINER_H__ */
