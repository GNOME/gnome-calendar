/* -*- mode: c; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gcal-event-overlay.h
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

#ifndef __GCAL_EVENT_OVERLAY_H__
#define __GCAL_EVENT_OVERLAY_H__

#include <gtk/gtk.h>
#include <libical/icaltime.h>

G_BEGIN_DECLS

typedef struct _GcalNewEventData                GcalNewEventData;

struct _GcalNewEventData
{
  icaltimetype  *start_date;
  icaltimetype  *end_date;
  gchar         *calendar_uid;
  gchar         *summary;
};

#define GCAL_TYPE_EVENT_OVERLAY                (gcal_event_overlay_get_type ())
#define GCAL_EVENT_OVERLAY(obj)                (G_TYPE_CHECK_INSTANCE_CAST((obj), GCAL_TYPE_EVENT_OVERLAY, GcalEventOverlay))
#define GCAL_EVENT_OVERLAY_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST((klass), GCAL_TYPE_EVENT_OVERLAY, GcalEventOverlayClass))
#define GCAL_IS_EVENT_OVERLAY(obj)             (G_TYPE_CHECK_INSTANCE_TYPE((obj), GCAL_TYPE_EVENT_OVERLAY))
#define GCAL_IS_EVENT_OVERLAY_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE((klass), GCAL_TYPE_EVENT_OVERLAY))
#define GCAL_EVENT_OVERLAY_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS((obj), GCAL_TYPE_EVENT_OVERLAY, GcalEventOverlayClass))

typedef struct _GcalEventOverlay                GcalEventOverlay;
typedef struct _GcalEventOverlayClass           GcalEventOverlayClass;
typedef struct _GcalEventOverlayPrivate         GcalEventOverlayPrivate;

struct _GcalEventOverlay
{
  GtkOverlay parent;

  GcalEventOverlayPrivate *priv;
};

struct _GcalEventOverlayClass
{
  GtkOverlayClass parent_class;

  /* Signals */
  void (*cancelled) (GcalEventOverlay *widget);
  void (*created)   (GcalEventOverlay *widget, GcalNewEventData *new_data, gboolean open_details);
};


GType           gcal_event_overlay_get_type          (void);

GtkWidget*      gcal_event_overlay_new               (void);

void            gcal_event_overlay_reset             (GcalEventOverlay *widget);

void            gcal_event_overlay_set_span          (GcalEventOverlay *widget,
                                                      icaltimetype     *start_date,
                                                      icaltimetype     *end_date);

void            gcal_event_overlay_set_sources_model (GcalEventOverlay *widget,
                                                      GtkListStore     *sources_model);


G_END_DECLS

#endif /* __GCAL_EVENT_OVERLAY_H__ */
