/* -*- mode: c; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gcal-new-event-widget.h
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

#ifndef __GCAL_NEW_EVENT_WIDGET_H__
#define __GCAL_NEW_EVENT_WIDGET_H__

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

#define GCAL_TYPE_NEW_EVENT_WIDGET                (gcal_new_event_widget_get_type ())
#define GCAL_NEW_EVENT_WIDGET(obj)                (G_TYPE_CHECK_INSTANCE_CAST((obj), GCAL_TYPE_NEW_EVENT_WIDGET, GcalNewEventWidget))
#define GCAL_NEW_EVENT_WIDGET_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST((klass), GCAL_TYPE_NEW_EVENT_WIDGET, GcalNewEventWidgetClass))
#define GCAL_IS_NEW_EVENT_WIDGET(obj)             (G_TYPE_CHECK_INSTANCE_TYPE((obj), GCAL_TYPE_NEW_EVENT_WIDGET))
#define GCAL_IS_NEW_EVENT_WIDGET_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE((klass), GCAL_TYPE_NEW_EVENT_WIDGET))
#define GCAL_NEW_EVENT_WIDGET_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS((obj), GCAL_TYPE_NEW_EVENT_WIDGET, GcalNewEventWidgetClass))

typedef struct _GcalNewEventWidget                GcalNewEventWidget;
typedef struct _GcalNewEventWidgetClass           GcalNewEventWidgetClass;

struct _GcalNewEventWidget
{
  GtkOverlay parent;
};

struct _GcalNewEventWidgetClass
{
  GtkOverlayClass parent_class;
};


GType           gcal_new_event_widget_get_type           (void);

GtkWidget*      gcal_new_event_widget_new                  (void);

void            gcal_new_event_widget_set_title            (GcalNewEventWidget *widget,
                                                            const gchar        *title);

void            gcal_new_event_widget_set_calendars        (GcalNewEventWidget *widget,
                                                            GtkTreeModel       *sources_model);

void            gcal_new_event_widget_set_default_calendar (GcalNewEventWidget *widget,
                                                            const gchar        *source_uid);

GtkWidget*      gcal_new_event_widget_get_entry            (GcalNewEventWidget *widget);

GtkWidget*      gcal_new_event_widget_get_create_button    (GcalNewEventWidget *widget);

GtkWidget*      gcal_new_event_widget_get_details_button   (GcalNewEventWidget *widget);

GtkWidget*      gcal_new_event_widget_get_close_button     (GcalNewEventWidget *widget);

gchar*          gcal_new_event_widget_get_calendar_uid     (GcalNewEventWidget *widget);

gchar*          gcal_new_event_widget_get_summary          (GcalNewEventWidget *widget);

G_END_DECLS

#endif /* __GCAL_NEW_EVENT_WIDGET_H__ */
