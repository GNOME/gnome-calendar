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

#include "gcal-event.h"
#include "gcal-manager.h"

#include <gtk/gtk.h>

#include <libical/icaltime.h>

G_BEGIN_DECLS

#define GCAL_TYPE_EVENT_WIDGET                    (gcal_event_widget_get_type ())

G_DECLARE_FINAL_TYPE (GcalEventWidget, gcal_event_widget, GCAL, EVENT_WIDGET, GtkWidget)

GtkWidget*    gcal_event_widget_new                        (GcalEvent          *event);

GcalEvent*    gcal_event_widget_get_event                  (GcalEventWidget    *self);

GDateTime*    gcal_event_widget_get_date_start             (GcalEventWidget    *self);

void          gcal_event_widget_set_date_start             (GcalEventWidget    *self,
                                                            GDateTime          *date_start);

GDateTime*    gcal_event_widget_get_date_end               (GcalEventWidget    *self);

void          gcal_event_widget_set_date_end               (GcalEventWidget    *self,
                                                            GDateTime          *date_end);

void          gcal_event_widget_set_read_only              (GcalEventWidget    *event,
                                                            gboolean            read_only);

gboolean      gcal_event_widget_get_read_only              (GcalEventWidget    *event);

void         gcal_event_widget_set_has_reminders           (GcalEventWidget    *event,
                                                            gboolean            has_reminders);

gboolean     gcal_event_widget_get_has_reminders           (GcalEventWidget    *event);

/* Utilities */

GtkWidget*    gcal_event_widget_clone                      (GcalEventWidget    *widget);

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
