/* -*- mode: c; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gcal-event-widget.h
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

#ifndef __GCAL_EVENT_WIDGET_H__
#define __GCAL_EVENT_WIDGET_H__

#include "gcal-manager.h"

#include <gtk/gtk.h>

#include <libical/icaltime.h>

G_BEGIN_DECLS


#define GCAL_TYPE_EVENT_WIDGET                    (gcal_event_widget_get_type ())
#define GCAL_EVENT_WIDGET(obj)                    (G_TYPE_CHECK_INSTANCE_CAST((obj), GCAL_TYPE_EVENT_WIDGET, GcalEventWidget))
#define GCAL_EVENT_WIDGET_CLASS(klass)            (G_TYPE_CHECK_CLASS_CAST((klass), GCAL_TYPE_EVENT_WIDGET, GcalEventWidgetClass))
#define GCAL_IS_EVENT_WIDGET(obj)                 (G_TYPE_CHECK_INSTANCE_TYPE((obj), GCAL_TYPE_EVENT_WIDGET))
#define GCAL_IS_EVENT_WIDGET_CLASS(klass)         (G_TYPE_CHECK_CLASS_TYPE((klass), GCAL_TYPE_EVENT_WIDGET))
#define GCAL_EVENT_WIDGET_GET_CLASS(obj)          (G_TYPE_INSTANCE_GET_CLASS((obj), GCAL_TYPE_EVENT_WIDGET, GcalEventWidgetClass))

typedef struct _GcalEventWidget                   GcalEventWidget;
typedef struct _GcalEventWidgetClass              GcalEventWidgetClass;

struct _GcalEventWidget
{
  GtkWidget parent;
};

struct _GcalEventWidgetClass
{
  GtkWidgetClass parent_class;

  /* signals */
  void (*activate)  (GcalEventWidget *event_widget);
};

GType         gcal_event_widget_get_type                   (void);

GtkWidget*    gcal_event_widget_new                        (gchar              *uuid);

GtkWidget*    gcal_event_widget_new_from_data              (GcalEventData      *data);

GtkWidget*    gcal_event_widget_new_with_summary_and_color (const gchar        *summary,
                                                            const GdkRGBA      *color);

const gchar*  gcal_event_widget_peek_uuid                  (GcalEventWidget    *event);

void          gcal_event_widget_set_date                   (GcalEventWidget    *event,
                                                            const icaltimetype *date);

icaltimetype* gcal_event_widget_get_date                   (GcalEventWidget    *event);

void          gcal_event_widget_set_end_date               (GcalEventWidget    *event,
                                                            const icaltimetype *date);

icaltimetype* gcal_event_widget_get_end_date               (GcalEventWidget    *event);

void          gcal_event_widget_set_summary                (GcalEventWidget    *event,
                                                            gchar              *summary);

gchar*        gcal_event_widget_get_summary                (GcalEventWidget    *event);

void          gcal_event_widget_set_color                  (GcalEventWidget    *event,
                                                            GdkRGBA            *color);

GdkRGBA*      gcal_event_widget_get_color                  (GcalEventWidget    *event);

void          gcal_event_widget_set_all_day                (GcalEventWidget    *event,
                                                            gboolean            all_day);

gboolean     gcal_event_widget_get_all_day                 (GcalEventWidget    *event);

void         gcal_event_widget_set_has_reminders           (GcalEventWidget    *event,
                                                            gboolean            has_reminders);

gboolean     gcal_event_widget_get_has_reminders           (GcalEventWidget    *event);

GcalEventData* gcal_event_widget_get_data                  (GcalEventWidget    *event);

gboolean     gcal_event_widget_equal                       (GcalEventWidget    *widget1,
                                                            GcalEventWidget    *widget2);

G_END_DECLS

#endif /* __GCAL_EVENT_WIDGET_H__ */
