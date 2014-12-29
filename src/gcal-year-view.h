/*
 * gcal-year-view.h
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

#ifndef __GCAL_YEAR_VIEW_H__
#define __GCAL_YEAR_VIEW_H__

#include "gcal-manager.h"

#include "gcal-subscriber-view.h"

G_BEGIN_DECLS

#define GCAL_TYPE_YEAR_VIEW                       (gcal_year_view_get_type ())
#define GCAL_YEAR_VIEW(obj)                       (G_TYPE_CHECK_INSTANCE_CAST((obj), GCAL_TYPE_YEAR_VIEW, GcalYearView))
#define GCAL_YEAR_VIEW_CLASS(klass)               (G_TYPE_CHECK_CLASS_CAST((klass), GCAL_TYPE_YEAR_VIEW, GcalYearViewClass))
#define GCAL_IS_YEAR_VIEW(obj)                    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GCAL_TYPE_YEAR_VIEW))
#define GCAL_IS_YEAR_VIEW_CLASS(klass)            (G_TYPE_CHECK_CLASS_TYPE((klass), GCAL_TYPE_YEAR_VIEW))
#define GCAL_YEAR_VIEW_GET_CLASS(obj)             (G_TYPE_INSTANCE_GET_CLASS((obj), GCAL_TYPE_YEAR_VIEW, GcalYearViewClass))

typedef struct _GcalYearView                       GcalYearView;
typedef struct _GcalYearViewClass                  GcalYearViewClass;

struct _GcalYearView
{
  GcalSubscriberView parent;
};

struct _GcalYearViewClass
{
  GcalSubscriberViewClass parent_class;

  /* signals */
  void (*new_event) (gint new_day);
};

GType          gcal_year_view_get_type         (void);

GtkWidget*     gcal_year_view_new              (GcalManager *manager);

G_END_DECLS

#endif /* __GCAL_YEAR_VIEW_H__ */
