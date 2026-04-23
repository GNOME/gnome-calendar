/*
 * gcal-agenda-view-day.c
 *
 * Copyright 2026 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#define G_LOG_DOMAIN "GcalAgendaViewDay"

#include "gcal-agenda-view-day.h"

#include "gcal-date-time-utils.h"
#include "gcal-range.h"

struct _GcalAgendaViewDay
{
  GObject parent_instance;

  GDateTime *date;
  GcalRange *range;

  GtkFilterListModel *filtered_events;
};

static void          g_list_model_interface_init                 (GListModelInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (GcalAgendaViewDay, gcal_agenda_view_day, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, g_list_model_interface_init))

enum {
  PROP_0,
  PROP_DATE,
  PROP_MODEL,
  PROP_N_ITEMS,
  N_PROPS,
};

static GParamSpec *properties [N_PROPS];


/*
 * Callbacks
 */
static void
on_filtered_events_items_changed_cb (GListModel        *model,
                                     unsigned int       position,
                                     unsigned int       removed,
                                     unsigned int       added,
                                     GcalAgendaViewDay *self)
{
  g_list_model_items_changed (G_LIST_MODEL (self), position, removed, added);
  if (removed != added)
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_N_ITEMS]);
}


/*
 * GListModel interface
 */

static GType
gcal_agenda_view_day_get_item_type (GListModel *model G_GNUC_UNUSED)
{
  return GCAL_TYPE_EVENT;
}

static unsigned int
gcal_agenda_view_day_get_n_items (GListModel *model)
{
  GcalAgendaViewDay *self = (GcalAgendaViewDay *) model;

  g_assert (GCAL_IS_AGENDA_VIEW_DAY (self));

  return g_list_model_get_n_items (G_LIST_MODEL (self->filtered_events));
}

static gpointer
gcal_agenda_view_day_get_item (GListModel *model,
                               guint       position)
{
  GcalAgendaViewDay *self = (GcalAgendaViewDay *) model;

  g_assert (GCAL_IS_AGENDA_VIEW_DAY (self));

  return g_list_model_get_item (G_LIST_MODEL (self->filtered_events), position);
}

static void
g_list_model_interface_init (GListModelInterface *iface)
{
  iface->get_item_type = gcal_agenda_view_day_get_item_type;
  iface->get_n_items = gcal_agenda_view_day_get_n_items;
  iface->get_item = gcal_agenda_view_day_get_item;
}


/*
 * GObject overrides
 */

static void
gcal_agenda_view_day_dispose (GObject *object)
{
  GcalAgendaViewDay *self = (GcalAgendaViewDay *)object;

  g_clear_pointer (&self->range, gcal_range_unref);
  gcal_clear_date_time (&self->date);

  G_OBJECT_CLASS (gcal_agenda_view_day_parent_class)->dispose (object);
}

static void
gcal_agenda_view_day_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GcalAgendaViewDay *self = GCAL_AGENDA_VIEW_DAY (object);

  switch (prop_id)
    {
    case PROP_DATE:
      g_value_set_boxed (value, self->date);
      break;

    case PROP_MODEL:
      g_value_set_object (value, gcal_agenda_view_day_get_model (self));
      break;

    case PROP_N_ITEMS:
      g_value_set_uint (value, g_list_model_get_n_items (G_LIST_MODEL (self->filtered_events)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_agenda_view_day_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GcalAgendaViewDay *self = GCAL_AGENDA_VIEW_DAY (object);

  switch (prop_id)
    {
    case PROP_DATE:
      gcal_agenda_view_day_set_date (self, g_value_get_boxed (value));
      break;

    case PROP_MODEL:
      gcal_agenda_view_day_set_model (self, g_value_get_object (value));
      break;

    case PROP_N_ITEMS:
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_agenda_view_day_class_init (GcalAgendaViewDayClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gcal_agenda_view_day_dispose;
  object_class->get_property = gcal_agenda_view_day_get_property;
  object_class->set_property = gcal_agenda_view_day_set_property;

  properties[PROP_DATE] = g_param_spec_boxed ("date", NULL, NULL,
                                              G_TYPE_DATE_TIME,
                                              G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  properties[PROP_MODEL] = g_param_spec_object ("model", NULL, NULL,
                                                G_TYPE_LIST_MODEL,
                                                G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_N_ITEMS] = g_param_spec_uint ("n-items", NULL, NULL,
                                                0, G_MAXUINT, 0,
                                                G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static gboolean
filter_events_func (gpointer item,
                    gpointer user_data)
{
  GcalAgendaViewDay *self = (GcalAgendaViewDay *) user_data;

  g_assert (GCAL_IS_EVENT (item));
  g_assert (GCAL_IS_AGENDA_VIEW_DAY (self));

  if (!self->range)
    return FALSE;

  return gcal_event_overlaps (item, self->range);
}

static void
gcal_agenda_view_day_init (GcalAgendaViewDay *self)
{
  self->filtered_events = gtk_filter_list_model_new (NULL,
                                                     GTK_FILTER (gtk_custom_filter_new (filter_events_func, self, NULL)));
  g_signal_connect (self->filtered_events, "items-changed", G_CALLBACK (on_filtered_events_items_changed_cb), self);
}

GcalAgendaViewDay *
gcal_agenda_view_day_new (void)
{
  return g_object_new (GCAL_TYPE_AGENDA_VIEW_DAY, NULL);
}

GDateTime *
gcal_agenda_view_day_get_date (GcalAgendaViewDay *self)
{
  g_assert (GCAL_IS_AGENDA_VIEW_DAY (self));

  return self->date;
}

void
gcal_agenda_view_day_set_date (GcalAgendaViewDay *self,
                               GDateTime         *date)
{
  g_assert (GCAL_IS_AGENDA_VIEW_DAY (self));

  if (gcal_set_date_time (&self->date, date))
    {
      GtkFilter *filter = NULL;

      g_clear_pointer (&self->range, gcal_range_unref);

      if (date)
        {
          g_autoptr (GDateTime) start = NULL;
          g_autoptr (GDateTime) end = NULL;

          start = g_date_time_new (g_date_time_get_timezone (date),
                                   g_date_time_get_year (date),
                                   g_date_time_get_month (date),
                                   g_date_time_get_day_of_month (date),
                                   0, 0, 0);
          end = g_date_time_add_days (start, 1);

          self->range = gcal_range_new_take (g_steal_pointer (&start),
                                             g_steal_pointer (&end),
                                             GCAL_RANGE_DEFAULT);
        }

      filter = gtk_filter_list_model_get_filter (self->filtered_events);
      // FIXME: better filter change
      gtk_filter_changed (filter, GTK_FILTER_CHANGE_DIFFERENT);

      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DATE]);
    }
}

GListModel *
gcal_agenda_view_day_get_model (GcalAgendaViewDay *self)
{
  g_assert (GCAL_IS_AGENDA_VIEW_DAY (self));

  return gtk_filter_list_model_get_model (self->filtered_events);
}

void
gcal_agenda_view_day_set_model (GcalAgendaViewDay *self,
                                GListModel        *model)
{
  g_assert (GCAL_IS_AGENDA_VIEW_DAY (self));

  if (gtk_filter_list_model_get_model (self->filtered_events) != model)
    {
      gtk_filter_list_model_set_model (self->filtered_events, model);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODEL]);
    }
}
