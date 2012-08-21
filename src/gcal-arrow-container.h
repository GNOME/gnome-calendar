/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 2 -*- */
/*
 * gcal-arrow-container.h
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

#ifndef __GCAL_ARROW_CONTAINER_H__
#define __GCAL_ARROW_CONTAINER_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS


#define GCAL_TYPE_ARROW_CONTAINER                (gcal_arrow_container_get_type ())
#define GCAL_ARROW_CONTAINER(obj)                (G_TYPE_CHECK_INSTANCE_CAST((obj), GCAL_TYPE_ARROW_CONTAINER, GcalArrowContainer))
#define GCAL_ARROW_CONTAINER_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST((klass), GCAL_TYPE_ARROW_CONTAINER, GcalArrowContainerClass))
#define GCAL_IS_ARROW_CONTAINER(obj)             (G_TYPE_CHECK_INSTANCE_TYPE((obj), GCAL_TYPE_ARROW_CONTAINER))
#define GCAL_IS_ARROW_CONTAINER_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE((klass), GCAL_TYPE_ARROW_CONTAINER))
#define GCAL_ARROW_CONTAINER_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS((obj), GCAL_TYPE_ARROW_CONTAINER, GcalArrowContainerClass))

typedef struct _GcalArrowContainer                GcalArrowContainer;
typedef struct _GcalArrowContainerClass           GcalArrowContainerClass;
typedef struct _GcalArrowContainerPrivate         GcalArrowContainerPrivate;

struct _GcalArrowContainer
{
  GtkBin parent;
  /* add your public declarations here */

  GcalArrowContainerPrivate *priv;
};

struct _GcalArrowContainerClass
{
  GtkBinClass parent_class;
};


GType        gcal_arrow_container_get_type        (void);
GtkWidget*   gcal_arrow_container_new             (void);


G_END_DECLS

#endif /* __GCAL_ARROW_CONTAINER_H__ */
