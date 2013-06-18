/*
 * gcal-viewport.h
 *
 * Copyright (C) 2012 - Erick Pérez Castellanos
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GCAL_VIEWPORT_H__
#define __GCAL_VIEWPORT_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GCAL_TYPE_VIEWPORT                       (gcal_viewport_get_type ())
#define GCAL_VIEWPORT(obj)                       (G_TYPE_CHECK_INSTANCE_CAST((obj), GCAL_TYPE_VIEWPORT, GcalViewport))
#define GCAL_VIEWPORT_CLASS(klass)               (G_TYPE_CHECK_CLASS_CAST((klass), GCAL_TYPE_VIEWPORT, GcalViewportClass))
#define GCAL_IS_VIEWPORT(obj)                    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GCAL_TYPE_VIEWPORT))
#define GCAL_IS_VIEWPORT_CLASS(klass)            (G_TYPE_CHECK_CLASS_TYPE((klass), GCAL_TYPE_VIEWPORT))
#define GCAL_VIEWPORT_GET_CLASS(obj)             (G_TYPE_INSTANCE_GET_CLASS((obj), GCAL_TYPE_VIEWPORT, GcalViewportClass))

typedef struct _GcalViewport                      GcalViewport;
typedef struct _GcalViewportClass                 GcalViewportClass;
typedef struct _GcalViewportPrivate               GcalViewportPrivate;

struct _GcalViewport
{
  GtkOverlay  parent;

  /* add your public declarations here */
  GcalViewportPrivate *priv;
};

struct _GcalViewportClass
{
  GtkOverlayClass parent_class;
};

GType          gcal_viewport_get_type                  (void);

GtkWidget*     gcal_viewport_new                       (void);

void           gcal_viewport_add                       (GcalViewport *viewport,
							GtkWidget    *widget);

void           gcal_viewport_scroll_to                 (GcalViewport *viewport,
							gdouble       value);

G_END_DECLS

#endif /* __GCAL_VIEWPORT_H__ */
