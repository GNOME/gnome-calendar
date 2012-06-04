/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 2 -*- */
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

#ifndef __GCAL_UTILS_H__
#define __GCAL_UTILS_H__

#include <gtk/gtk.h>
#include <libical/icaltime.h>

#define ICAL_TIME_TYPE (icaltime_get_type ())

typedef enum
{
  GCAL_VIEW_TYPE_DAILY = 0,
  GCAL_VIEW_TYPE_WEEKLY,
  GCAL_VIEW_TYPE_MONTHLY,
  GCAL_VIEW_TYPE_YEARLY,
  GCAL_VIEW_TYPE_LIST,
} GcalViewTypeEnum;

typedef enum
{
  GCAL_TOOLBAR_OVERVIEW = 0,
  GCAL_TOOLBAR_VIEW_EVENT
} GcalToolbarMode;

typedef enum
{
  GCAL_EDIT_MODE = 0,
  GCAL_VIEW_MODE,
} GcalEditableMode;

typedef
const gchar*  (*GcalTranslateFunc)                             (GtkWidget      *source_widget);

GType           icaltime_get_type                               (void)           G_GNUC_CONST;

void            gcal_gtk_tree_view_set_activate_on_single_click (GtkTreeView    *tree_view,
                                                                 gboolean       should_activate);


const gchar*    gcal_get_group_name                             (const gchar    *base_uri);

icaltimetype*   gcal_dup_icaltime                               (icaltimetype   *date);

#endif // __GCAL_UTILS_H__
