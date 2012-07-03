/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 2 -*- */
/*
 * gcal-event-view.h
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

#ifndef __GCAL_EVENT_VIEW_H__
#define __GCAL_EVENT_VIEW_H__

#include "gcal-manager.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GCAL_TYPE_EVENT_VIEW                    (gcal_event_view_get_type ())
#define GCAL_EVENT_VIEW(obj)                    (G_TYPE_CHECK_INSTANCE_CAST((obj), GCAL_TYPE_EVENT_VIEW, GcalEventView))
#define GCAL_EVENT_VIEW_CLASS(klass)            (G_TYPE_CHECK_CLASS_CAST((klass), GCAL_TYPE_EVENT_VIEW, GcalEventViewClass))
#define GCAL_IS_EVENT_VIEW(obj)                 (G_TYPE_CHECK_INSTANCE_TYPE((obj), GCAL_TYPE_EVENT_VIEW))
#define GCAL_IS_EVENT_VIEW_CLASS(klass)         (G_TYPE_CHECK_CLASS_TYPE((klass), GCAL_TYPE_EVENT_VIEW))
#define GCAL_EVENT_VIEW_GET_CLASS(obj)          (G_TYPE_INSTANCE_GET_CLASS((obj), GCAL_TYPE_EVENT_VIEW, GcalEventViewClass))

typedef struct _GcalEventView                    GcalEventView;
typedef struct _GcalEventViewClass               GcalEventViewClass;
typedef struct _GcalEventViewPrivate             GcalEventViewPrivate;

struct _GcalEventView
{
  GtkGrid parent;

  GcalEventViewPrivate *priv;
};

struct _GcalEventViewClass
{
  GtkGridClass parent_class;

  /* Signals */
  void (*done) (GcalEventView *view);
};

GType         gcal_event_view_get_type                   (void);

GtkWidget*    gcal_event_view_new_with_manager           (GcalManager   *manager);

void          gcal_event_view_load_new                   (GcalEventView *view);

void          gcal_event_view_load_event                 (GcalEventView *view,
                                                          const gchar   *event_uuid);

void          gcal_event_view_enter_edit_mode            (GcalEventView *view);

void          gcal_event_view_leave_edit_mode            (GcalEventView *view);

G_END_DECLS

#endif /* __GCAL_EVENT_VIEW_H__ */
