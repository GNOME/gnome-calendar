/*
 * gcal-subscriber.h
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

#ifndef __GCAL_SUBSCRIBER_H__
#define __GCAL_SUBSCRIBER_H__

#include "gcal-manager.h"
#include "gcal-event-widget.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GCAL_TYPE_SUBSCRIBER                       (gcal_subscriber_get_type ())
#define GCAL_SUBSCRIBER(obj)                       (G_TYPE_CHECK_INSTANCE_CAST((obj), GCAL_TYPE_SUBSCRIBER, GcalSubscriber))
#define GCAL_SUBSCRIBER_CLASS(klass)               (G_TYPE_CHECK_CLASS_CAST((klass), GCAL_TYPE_SUBSCRIBER, GcalSubscriberClass))
#define GCAL_IS_SUBSCRIBER(obj)                    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GCAL_TYPE_SUBSCRIBER))
#define GCAL_IS_SUBSCRIBER_CLASS(klass)            (G_TYPE_CHECK_CLASS_TYPE((klass), GCAL_TYPE_SUBSCRIBER))
#define GCAL_SUBSCRIBER_GET_CLASS(obj)             (G_TYPE_INSTANCE_GET_CLASS((obj), GCAL_TYPE_SUBSCRIBER, GcalSubscriberClass))

typedef struct _GcalSubscriber                       GcalSubscriber;
typedef struct _GcalSubscriberClass                  GcalSubscriberClass;
typedef struct _GcalSubscriberPrivate                GcalSubscriberPrivate;

struct _GcalSubscriber
{
  GtkContainer parent;

  GcalSubscriberPrivate *priv;
};

struct _GcalSubscriberClass
{
  GtkContainerClass parent_class;

  /*< public >*/
  gboolean  (*is_child_multicell) (GcalSubscriber *subscriber, GcalEventWidget *child);
  guint     (*get_child_cell)     (GcalSubscriber *subscriber, GcalEventWidget *child);
};

GType          gcal_subscriber_get_type         (void);

G_END_DECLS

#endif /* __GCAL_SUBSCRIBER_H__ */
