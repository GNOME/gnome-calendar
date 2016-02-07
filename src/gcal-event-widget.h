/* -*- mode: c; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gcal-event-widget.h
 * Copyright (C) 2015 Erick Pérez Castellanos <erickpc@gnome.org>
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

G_DECLARE_FINAL_TYPE (GcalEventWidget, gcal_event_widget, GCAL, EVENT_WIDGET, GtkWidget)

GtkWidget*    gcal_event_widget_new                        (gchar              *uuid);

GtkWidget*    gcal_event_widget_new_from_data              (GcalEventData      *data);

GtkWidget*    gcal_event_widget_clone                      (GcalEventWidget    *widget);

const gchar*  gcal_event_widget_peek_uuid                  (GcalEventWidget    *event);

void          gcal_event_widget_set_read_only              (GcalEventWidget    *event,
                                                            gboolean            read_only);

gboolean      gcal_event_widget_get_read_only              (GcalEventWidget    *event);

void          gcal_event_widget_set_date                   (GcalEventWidget    *event,
                                                            const icaltimetype *date);

icaltimetype* gcal_event_widget_get_date                   (GcalEventWidget    *event);

const icaltimetype* gcal_event_widget_peek_start_date            (GcalEventWidget    *event);

void          gcal_event_widget_set_end_date               (GcalEventWidget    *event,
                                                            const icaltimetype *date);

icaltimetype* gcal_event_widget_get_end_date               (GcalEventWidget    *event);

const icaltimetype* gcal_event_widget_peek_end_date              (GcalEventWidget    *event);

void          gcal_event_widget_set_summary                (GcalEventWidget    *event,
                                                            gchar              *summary);

gchar*        gcal_event_widget_get_summary                (GcalEventWidget    *event);

void          gcal_event_widget_set_color                  (GcalEventWidget    *event,
                                                            GdkRGBA            *color);

GdkRGBA*      gcal_event_widget_get_color                  (GcalEventWidget    *event);

void          gcal_event_widget_set_all_day                (GcalEventWidget    *event,
                                                            gboolean            all_day);

gboolean     gcal_event_widget_get_all_day                 (GcalEventWidget    *event);

gboolean     gcal_event_widget_is_multiday                 (GcalEventWidget    *event);

void         gcal_event_widget_set_has_reminders           (GcalEventWidget    *event,
                                                            gboolean            has_reminders);

gboolean     gcal_event_widget_get_has_reminders           (GcalEventWidget    *event);

GcalEventData* gcal_event_widget_get_data                  (GcalEventWidget    *event);

gboolean     gcal_event_widget_equal                       (GcalEventWidget    *widget1,
                                                            GcalEventWidget    *widget2);

gint         gcal_event_widget_compare_by_length           (GcalEventWidget    *widget1,
                                                            GcalEventWidget    *widget2);

gint         gcal_event_widget_compare_by_start_date       (GcalEventWidget    *widget1,
                                                            GcalEventWidget    *widget2);

gint         gcal_event_widget_compare_for_single_day      (GcalEventWidget    *widget1,
                                                            GcalEventWidget    *widget2);

G_END_DECLS

#endif /* __GCAL_EVENT_WIDGET_H__ */
