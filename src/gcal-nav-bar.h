/*
 * gcal-nav-bar.h
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

#ifndef __GCAL_NAV_BAR_H__
#define __GCAL_NAV_BAR_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GCAL_TYPE_NAV_BAR                       (gcal_nav_bar_get_type ())
#define GCAL_NAV_BAR(obj)                       (G_TYPE_CHECK_INSTANCE_CAST((obj), GCAL_TYPE_NAV_BAR, GcalNavBar))
#define GCAL_NAV_BAR_CLASS(klass)               (G_TYPE_CHECK_CLASS_CAST((klass), GCAL_TYPE_NAV_BAR, GcalNavBarClass))
#define GCAL_IS_NAV_BAR(obj)                    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GCAL_TYPE_NAV_BAR))
#define GCAL_IS_NAV_BAR_CLASS(klass)            (G_TYPE_CHECK_CLASS_TYPE((klass), GCAL_TYPE_NAV_BAR))
#define GCAL_NAV_BAR_GET_CLASS(obj)             (G_TYPE_INSTANCE_GET_CLASS((obj), GCAL_TYPE_NAV_BAR, GcalNavBarClass))

typedef struct _GcalNavBar                       GcalNavBar;
typedef struct _GcalNavBarClass                  GcalNavBarClass;

struct _GcalNavBar
{
  GtkGrid parent;
};

struct _GcalNavBarClass
{
  GtkGridClass parent_class;
};

GType          gcal_nav_bar_get_type         (void);

GtkWidget*     gcal_nav_bar_new              (void);

G_END_DECLS

#endif /* __GCAL_NAV_BAR_H__ */
