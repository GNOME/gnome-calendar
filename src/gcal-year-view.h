/* gcal-year-view.h
 *
 * Copyright (C) 2015 Erick Pérez Castellanos <erick.red@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
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

#ifndef GCAL_YEAR_VIEW_H
#define GCAL_YEAR_VIEW_H

#include "gcal-manager.h"
#include "gcal-event-widget.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GCAL_TYPE_YEAR_VIEW            (gcal_year_view_get_type())
#define GCAL_YEAR_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCAL_TYPE_YEAR_VIEW, GcalYearView))
#define GCAL_YEAR_VIEW_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCAL_TYPE_YEAR_VIEW, GcalYearView const))
#define GCAL_YEAR_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GCAL_TYPE_YEAR_VIEW, GcalYearViewClass))
#define GCAL_IS_YEAR_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCAL_TYPE_YEAR_VIEW))
#define GCAL_IS_YEAR_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GCAL_TYPE_YEAR_VIEW))
#define GCAL_YEAR_VIEW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GCAL_TYPE_YEAR_VIEW, GcalYearViewClass))

typedef struct _GcalYearView        GcalYearView;
typedef struct _GcalYearViewClass   GcalYearViewClass;
typedef struct _GcalYearViewPrivate GcalYearViewPrivate;

struct _GcalYearView
{
  GtkBox parent;

  /*< private >*/
  GcalYearViewPrivate *priv;
};

struct _GcalYearViewClass
{
  GtkBoxClass parent;

  /*< signals >*/
  void       (*event_activated)   (GcalYearView *year_view, GcalEventWidget *event_widget);
};

GType             gcal_year_view_get_type           (void);
GcalYearView     *gcal_year_view_new                (void);
void              gcal_year_view_set_manager        (GcalYearView *year_view,
                                                     GcalManager  *manager);
void              gcal_year_view_set_first_weekday  (GcalYearView *year_view,
                                                     gint          nr_day);
void              gcal_year_view_set_use_24h_format (GcalYearView *year_view,
                                                     gboolean      use_24h_format);

G_END_DECLS

#endif /* GCAL_YEAR_VIEW_H */
