/*
 * gcal-view.h
 *
 * Copyright (C) 2012 - Erick Pérez Castellanos
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GCAL_VIEW_H__
#define __GCAL_VIEW_H__

#include <glib-object.h>
#include <gtk/gtk.h>

#include <icaltime.h>

G_BEGIN_DECLS

#define GCAL_TYPE_VIEW                 (gcal_view_get_type ())
#define GCAL_VIEW(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCAL_TYPE_VIEW, GcalView))
#define GCAL_IS_VIEW(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCAL_TYPE_VIEW))
#define GCAL_VIEW_GET_INTERFACE(inst)  (G_TYPE_INSTANCE_GET_INTERFACE ((inst), GCAL_TYPE_VIEW, GcalViewIface))


typedef struct _GcalView                GcalView;
typedef struct _GcalViewIface           GcalViewIface;

struct _GcalViewIface
{
  GTypeInterface parent_iface;

  /* pure virtual methods */
  icaltimetype*   (*get_initial_date)     (GcalView *view);
  icaltimetype*   (*get_final_date)       (GcalView *view);

  gboolean        (*contains)             (GcalView *view, icaltimetype *date);
  void            (*remove_by_uuid)       (GcalView *view, const gchar *uuid);
  GtkWidget*      (*get_by_uuid)          (GcalView *view, const gchar *uuid);
};

GType         gcal_view_get_type          (void);

void          gcal_view_set_date          (GcalView     *view,
                                           icaltimetype *date);

icaltimetype* gcal_view_get_date          (GcalView     *view);

icaltimetype* gcal_view_get_initial_date  (GcalView     *view);

icaltimetype* gcal_view_get_final_date    (GcalView     *view);

gboolean      gcal_view_contains       (GcalView     *view,
                                           icaltimetype *date);

void          gcal_view_remove_by_uuid    (GcalView     *view,
                                           const gchar  *uuid);

GtkWidget*    gcal_view_get_by_uuid       (GcalView     *view,
                                           const gchar  *uuid);

G_END_DECLS

#endif /* __GCAL_MONTH_VIEW_H__ */
