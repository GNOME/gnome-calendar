/*
 * gcal-search-view.h
 *
 * Copyright (C) 2014 - Erick Pérez Castellanos
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

#ifndef __GCAL_SEARCH_VIEW_H__
#define __GCAL_SEARCH_VIEW_H__

#include "gcal-event-widget.h"
#include "gcal-manager.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GCAL_TYPE_SEARCH_VIEW                       (gcal_search_view_get_type ())
#define GCAL_SEARCH_VIEW(obj)                       (G_TYPE_CHECK_INSTANCE_CAST((obj), GCAL_TYPE_SEARCH_VIEW, GcalSearchView))
#define GCAL_SEARCH_VIEW_CLASS(klass)               (G_TYPE_CHECK_CLASS_CAST((klass), GCAL_TYPE_SEARCH_VIEW, GcalSearchViewClass))
#define GCAL_IS_SEARCH_VIEW(obj)                    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GCAL_TYPE_SEARCH_VIEW))
#define GCAL_IS_SEARCH_VIEW_CLASS(klass)            (G_TYPE_CHECK_CLASS_TYPE((klass), GCAL_TYPE_SEARCH_VIEW))
#define GCAL_SEARCH_VIEW_GET_CLASS(obj)             (G_TYPE_INSTANCE_GET_CLASS((obj), GCAL_TYPE_SEARCH_VIEW, GcalSearchViewClass))

typedef struct _GcalSearchView                       GcalSearchView;
typedef struct _GcalSearchViewClass                  GcalSearchViewClass;

struct _GcalSearchView
{
  GtkPopover parent;
};

struct _GcalSearchViewClass
{
  GtkPopoverClass parent_class;

  /* signals */
  void       (*event_activated)   (GcalSearchView *view, icaltimetype *date);
};

GType          gcal_search_view_get_type         (void);

void           gcal_search_view_set_time_format  (GcalSearchView *view,
                                                  gboolean        format_24h);

void           gcal_search_view_search           (GcalSearchView *view,
                                                  const gchar    *field,
                                                  const gchar    *query);


GtkWidget*     gcal_search_view_new              (GcalManager *manager);

G_END_DECLS

#endif /* __GCAL_SEARCH_VIEW_H__ */
