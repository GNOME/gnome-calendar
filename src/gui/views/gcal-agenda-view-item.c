/*
 * gcal-agenda-view-item.c
 *
 * Copyright 2026 Zelda Ahmed <zoeyahmed10@proton.me>
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "gcal-agenda-view-item.h"
#include "gcal-date-time-utils.h"

struct _GcalAgendaViewItem
{
  GObject parent_instance;

  GcalEvent       *event;
  GcalEventWidget *event_widget;

};

G_DEFINE_FINAL_TYPE (GcalAgendaViewItem, gcal_agenda_view_item, G_TYPE_OBJECT)

enum
{
  PROP_0,
  PROP_EVENT,
  PROP_EVENT_WIDGET,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
gcal_agenda_view_item_dispose (GObject *object)
{
  GcalAgendaViewItem *self = GCAL_AGENDA_VIEW_ITEM (object);

  g_clear_object (&self->event_widget);
  g_clear_object (&self->event);

  G_OBJECT_CLASS (gcal_agenda_view_item_parent_class)->dispose (object);
}

static void
gcal_agenda_view_item_get_property (GObject *object,
                                    guint prop_id,
                                    GValue *value,
                                    GParamSpec *pspec)
{
  GcalAgendaViewItem *self = GCAL_AGENDA_VIEW_ITEM (object);

  switch (prop_id)
    {
    case PROP_EVENT:
      g_value_set_object (value, gcal_agenda_view_item_get_event (self));
      break;

    case PROP_EVENT_WIDGET:
      g_value_set_object (value, gcal_agenda_view_item_get_event_widget (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_agenda_view_item_set_property (GObject *object,
                                    guint prop_id,
                                    const GValue *value,
                                    GParamSpec *pspec)
{
  GcalAgendaViewItem *self = GCAL_AGENDA_VIEW_ITEM (object);

  switch (prop_id)
    {
    case PROP_EVENT:
      gcal_agenda_view_item_set_event (self, g_value_get_object (value));
      break;
    case PROP_EVENT_WIDGET:
      gcal_agenda_view_item_set_event_widget (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_agenda_view_item_class_init (GcalAgendaViewItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gcal_agenda_view_item_dispose;
  object_class->get_property = gcal_agenda_view_item_get_property;
  object_class->set_property = gcal_agenda_view_item_set_property;

  /**
   * GcalAgendaViewItem:event:
   *
   * The event that will be presented by the item's row.
   */
  properties [PROP_EVENT] =
    g_param_spec_object ("event", NULL, NULL,
                         GCAL_TYPE_EVENT,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  /**
   * GcalAgendaViewItem:event:
   *
   * The event widget representing the item in the #GcalAgendaView.
   *
   * This should be passed into `gcal_view_event_activated`
   * when the `::activated` signal is emitted on the item's row.
   */
  properties [PROP_EVENT_WIDGET] =
    g_param_spec_object ("event-widget", NULL, NULL,
                         GCAL_TYPE_EVENT_WIDGET,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gcal_agenda_view_item_init (GcalAgendaViewItem *self)
{
}

/*
 * Public API
 */

/**
 * gcal_agenda_view_item_new:
 *
 * Creates a new #GcalAgendaViewItem
 *
 * Returns: (transfer full): a #GcalAgendaViewItem
 */
GcalAgendaViewItem *
gcal_agenda_view_item_new (void)
{
  GcalAgendaViewItem *item = g_object_new (GCAL_TYPE_AGENDA_VIEW_ITEM, NULL);

  return item;
}

/**
 * gcal_agenda_view_item_get_event:
 * @self: a #GcalAgendaViewItem
 *
 * Gets the event the item presents in the agenda view.
 *
 * Returns (nullable) (transfer none): the #GcalEvent being presented.
 */
GcalEvent *
gcal_agenda_view_item_get_event (GcalAgendaViewItem *self)
{
  g_assert (GCAL_IS_AGENDA_VIEW_ITEM (self));

  return self->event;
}

/**
 * gcal_agenda_view_item_set_event:
 * @self: a #GcalAgendaViewItem
 * @event (nullable) (transfer none): the #GcalEvent to present
 *
 * Sets the event the item will present in the agenda view.
 */
void
gcal_agenda_view_item_set_event (GcalAgendaViewItem *self,
                                 GcalEvent          *event)
{
  g_return_if_fail (GCAL_IS_AGENDA_VIEW_ITEM (self));
  g_return_if_fail (event == NULL || GCAL_IS_EVENT (event));

  if (g_set_object (&self->event, event))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_EVENT]);
}

/**
 * gcal_agenda_view_item_get_event_widget:
 * @self: a #GcalAgendaViewItem
 *
 * Gets the event widget representing the item in the agenda view.
 *
 * Returns (nullable) (transfer none): the #GcalEventWidget presenting the @event.
 */

GcalEventWidget *
gcal_agenda_view_item_get_event_widget (GcalAgendaViewItem *self)
{
  g_return_val_if_fail (GCAL_IS_AGENDA_VIEW_ITEM (self), NULL);

  return self->event_widget;
}

/**
 * gcal_agenda_view_item_set_event_widget:
 * @self: a #GcalAgendaViewItem
 * @widget: (nullable) (transfer none): the #GcalEventWidget presenting the @event.
 *
 * Sets the event widget representing the item in the agenda view.
 */
void
gcal_agenda_view_item_set_event_widget (GcalAgendaViewItem *self,
                                        GcalEventWidget    *event_widget)
{
  g_return_if_fail (GCAL_IS_AGENDA_VIEW_ITEM (self));
  g_return_if_fail (event_widget == NULL || GCAL_IS_EVENT_WIDGET (event_widget));

  if (g_set_object (&self->event_widget, event_widget))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_EVENT_WIDGET]);
}
