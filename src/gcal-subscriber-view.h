/*
 * gcal-subscriber-view.h
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

#ifndef __GCAL_SUBSCRIBER_VIEW_H__
#define __GCAL_SUBSCRIBER_VIEW_H__

#include "gcal-manager.h"
#include "gcal-event-widget.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GCAL_TYPE_SUBSCRIBER_VIEW                       (gcal_subscriber_view_get_type ())
#define GCAL_SUBSCRIBER_VIEW(obj)                       (G_TYPE_CHECK_INSTANCE_CAST((obj), GCAL_TYPE_SUBSCRIBER_VIEW, GcalSubscriberView))
#define GCAL_SUBSCRIBER_VIEW_CLASS(klass)               (G_TYPE_CHECK_CLASS_CAST((klass), GCAL_TYPE_SUBSCRIBER_VIEW, GcalSubscriberViewClass))
#define GCAL_IS_SUBSCRIBER_VIEW(obj)                    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GCAL_TYPE_SUBSCRIBER_VIEW))
#define GCAL_IS_SUBSCRIBER_VIEW_CLASS(klass)            (G_TYPE_CHECK_CLASS_TYPE((klass), GCAL_TYPE_SUBSCRIBER_VIEW))
#define GCAL_SUBSCRIBER_VIEW_GET_CLASS(obj)             (G_TYPE_INSTANCE_GET_CLASS((obj), GCAL_TYPE_SUBSCRIBER_VIEW, GcalSubscriberViewClass))

typedef struct _GcalSubscriberView                       GcalSubscriberView;
typedef struct _GcalSubscriberViewClass                  GcalSubscriberViewClass;
typedef struct _GcalSubscriberViewPrivate                GcalSubscriberViewPrivate;

struct _GcalSubscriberView
{
  GtkContainer parent;

  GcalSubscriberViewPrivate *priv;
};

struct _GcalSubscriberViewClass
{
  GtkContainerClass parent_class;

  /*< signals >*/
  void       (*event_activated)   (GcalSubscriberView *subscriber_view, GcalEventWidget *event_widget);

  /*< public >*/
  gboolean  (*is_child_multicell) (GcalSubscriberView *subscriber, GcalEventWidget *child);
  guint     (*get_child_cell)     (GcalSubscriberView *subscriber, GcalEventWidget *child);

  /* gcal-view replacements */
  GtkWidget*     (*get_child_by_uuid)        (GcalSubscriberView *subscriber_view, const gchar *uuid);
};

GType          gcal_subscriber_view_get_type           (void);

GtkWidget*     gcal_subscriber_view_get_child_by_uuid  (GcalSubscriberView *subscriber_view,
							const gchar        *uuid);

/* protected */
void           _gcal_subscriber_view_setup_child       (GcalSubscriberView *subscriber_view,
							GtkWidget          *child_widget);

G_END_DECLS

#endif /* __GCAL_SUBSCRIBER_VIEW_H__ */
