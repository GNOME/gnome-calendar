/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 2 -*- */
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

#include <gtk/gtk.h>

G_BEGIN_DECLS


#define GCAL_TYPE_EVENT_WIDGET                    (gcal_event_widget_get_type ())
#define GCAL_EVENT_WIDGET(obj)                    (G_TYPE_CHECK_INSTANCE_CAST((obj), GCAL_TYPE_EVENT_WIDGET, GcalEventWidget))
#define GCAL_EVENT_WIDGET_CLASS(klass)            (G_TYPE_CHECK_CLASS_CAST((klass), GCAL_TYPE_EVENT_WIDGET, GcalEventWidgetClass))
#define GCAL_IS_EVENT_WIDGET(obj)                 (G_TYPE_CHECK_INSTANCE_TYPE((obj), GCAL_TYPE_EVENT_WIDGET))
#define GCAL_IS_EVENT_WIDGET_CLASS(klass)         (G_TYPE_CHECK_CLASS_TYPE((klass), GCAL_TYPE_EVENT_WIDGET))
#define GCAL_EVENT_WIDGET_GET_CLASS(obj)          (G_TYPE_INSTANCE_GET_CLASS((obj), GCAL_TYPE_EVENT_WIDGET, GcalEventWidgetClass))

typedef struct _GcalEventWidget                   GcalEventWidget;
typedef struct _GcalEventWidgetClass              GcalEventWidgetClass;
typedef struct _GcalEventWidgetPrivate            GcalEventWidgetPrivate;

struct _GcalEventWidget
{
  GtkApplicationWindow parent;

  GcalEventWidgetPrivate *priv;
};

struct _GcalEventWidgetClass
{
  GtkWidgetClass parent_class;
};


GType        gcal_event_widget_get_type                   (void);

GtkWidget*   gcal_event_widget_new                        (void);

GtkWidget*   gcal_event_widget_new_with_summary_and_color (const gchar   *summary,
                                                           const GdkRGBA *color);

G_END_DECLS

#endif /* __GCAL_EVENT_WIDGET_H__ */
