/* -*- mode: c; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gcal-window.h
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

#ifndef __GCAL_WINDOW_H__
#define __GCAL_WINDOW_H__

#include "gcal-application.h"
#include "gcal-utils.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS


#define GCAL_TYPE_WINDOW                    (gcal_window_get_type ())
#define GCAL_WINDOW(obj)                    (G_TYPE_CHECK_INSTANCE_CAST((obj), GCAL_TYPE_WINDOW, GcalWindow))
#define GCAL_WINDOW_CLASS(klass)            (G_TYPE_CHECK_CLASS_CAST((klass), GCAL_TYPE_WINDOW, GcalWindowClass))
#define GCAL_IS_WINDOW(obj)                 (G_TYPE_CHECK_INSTANCE_TYPE((obj), GCAL_TYPE_WINDOW))
#define GCAL_IS_WINDOW_CLASS(klass)         (G_TYPE_CHECK_CLASS_TYPE((klass), GCAL_TYPE_WINDOW))
#define GCAL_WINDOW_GET_CLASS(obj)          (G_TYPE_INSTANCE_GET_CLASS((obj), GCAL_TYPE_WINDOW, GcalWindowClass))

typedef struct _GcalWindow                   GcalWindow;
typedef struct _GcalWindowClass              GcalWindowClass;
typedef struct _GcalWindowPrivate            GcalWindowPrivate;

struct _GcalWindow
{
  GtkApplicationWindow parent;

  GcalWindowPrivate *priv;
};

struct _GcalWindowClass
{
  GtkApplicationWindowClass parent_class;
};


GType        gcal_window_get_type             (void);

GtkWidget*   gcal_window_new_with_view        (GcalApplication    *app,
                                               GcalWindowViewType  view_type);

void         gcal_window_new_event            (GcalWindow         *window);

void         gcal_window_set_search_mode      (GcalWindow         *window,
                                               gboolean            enabled);

void         gcal_window_show_notification    (GcalWindow         *window);

void         gcal_window_hide_notification    (GcalWindow         *window);

G_END_DECLS

#endif /* __GCAL_WINDOW_H__ */
