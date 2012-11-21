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

#ifndef __GCAL_UTILS_H__
#define __GCAL_UTILS_H__

#include <gtk/gtk.h>
#include <libical/icaltime.h>

#define ICAL_TIME_TYPE (icaltime_get_type ())

typedef enum
{
  GCAL_WINDOW_VIEW_DAY,
  GCAL_WINDOW_VIEW_WEEK,
  GCAL_WINDOW_VIEW_MONTH,
  GCAL_WINDOW_VIEW_YEAR,
  GCAL_WINDOW_VIEW_LIST,
} GcalWindowViewType;

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

typedef enum
{
  EVENT_SUMMARY = 0,
  EVENT_START_DATE,
  EVENT_END_DATE,
  EVENT_LOCATION,
  EVENT_DESCRIPTION,
  EVENT_SOURCE,
} EventEditableProperty;

typedef
const gchar*  (*GcalTranslateFunc)                              (GtkWidget             *source_widget);

GType           icaltime_get_type                               (void)            G_GNUC_CONST;

void            gcal_gtk_tree_view_set_activate_on_single_click (GtkTreeView           *tree_view,
                                                                 gboolean               should_activate);


const gchar*    gcal_get_group_name                             (const gchar           *base_uri);

icaltimetype*   gcal_dup_icaltime                               (const icaltimetype    *date);

gchar*          gcal_get_source_name                            (GtkTreeModel          *model,
                                                                 const gchar           *uid);

gchar*          gcal_get_source_uid                             (GtkTreeModel          *model,
                                                                 const gchar           *name);

gchar*          gcal_get_weekday                                (gint                   i);

gchar*          gcal_get_month_name                             (gint                   i);

GdkPixbuf*      gcal_get_pixbuf_from_color                      (GdkColor              *color);

/* code brought from evolution */
gsize           e_strftime_fix_am_pm                            (gchar                 *str,
                                                                 gsize                  max,
                                                                 const gchar           *fmt,
                                                                 const struct tm       *tm);
gsize           e_utf8_strftime_fix_am_pm                       (gchar                 *str,

                                                                 gsize                  max,
                                                                 const gchar           *fmt,
                                                                 const struct tm       *tm);

#endif // __GCAL_UTILS_H__
