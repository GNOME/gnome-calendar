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

#include <libical/icaltime.h>

G_BEGIN_DECLS

#define GCAL_TYPE_VIEW                 (gcal_view_get_type ())
#define GCAL_VIEW(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCAL_TYPE_VIEW, GcalView))
#define GCAL_IS_VIEW(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCAL_TYPE_VIEW))
#define GCAL_VIEW_GET_INTERFACE(inst)  (G_TYPE_INSTANCE_GET_INTERFACE ((inst), GCAL_TYPE_VIEW, GcalViewInterface))


typedef struct _GcalView                GcalView;
typedef struct _GcalViewInterface       GcalViewInterface;

struct _GcalViewInterface
{
  GTypeInterface parent_iface;

  gboolean (*is_in_range) (GcalView *view, icaltimetype* date);
};

GType  gcal_view_get_type  (void);

gboolean gcal_view_is_in_range (GcalView *view, icaltimetype* date);

G_END_DECLS

#endif /* __GCAL_MONTH_VIEW_H__ */
