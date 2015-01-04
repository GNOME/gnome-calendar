/* -*- mode: c; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gcal-search-view.c
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

#include "gcal-search-view.h"

#include "gcal-event-widget.h"
#include "gcal-utils.h"
#include "gcal-view.h"

#include <locale.h>
#include <langinfo.h>

#include <glib/gi18n.h>

typedef struct
{
  GtkWidget      *listbox;

  /* misc */
  gchar          *time_mask;
  gchar          *date_mask;

  /* property */
  icaltimetype   *date;
  GcalManager    *manager; /* weak reference */
} GcalSearchViewPrivate;

enum
{
  PROP_0,
  PROP_DATE,  /* active-date inherited property */
  PROP_MANAGER  /* manager inherited property */
};

static GtkWidget*     get_event_from_grid                       (GtkWidget            *grid);

static GtkWidget*     make_grid_for_event                       (GcalSearchView       *view,
                                                                 GcalEventWidget      *event);

static gint           sort_by_event                             (GtkListBoxRow        *row1,
                                                                 GtkListBoxRow        *row2,
                                                                 gpointer              user_data);

static void           open_event                                (GcalEventWidget      *event_widget,
                                                                 gpointer              user_data);

static void           gcal_data_model_subscriber_interface_init (ECalDataModelSubscriberInterface *iface);

static void           gcal_view_interface_init                  (GcalViewIface  *iface);

static void           gcal_search_view_constructed              (GObject        *object);

static void           gcal_search_view_set_property             (GObject        *object,
                                                                 guint           property_id,
                                                                 const GValue   *value,
                                                                 GParamSpec     *pspec);

static void           gcal_search_view_get_property             (GObject        *object,
                                                                 guint           property_id,
                                                                 GValue         *value,
                                                                 GParamSpec     *pspec);

static void           gcal_search_view_finalize                 (GObject        *object);

static void           gcal_search_view_component_added          (ECalDataModelSubscriber *subscriber,
                                                                 ECalClient              *client,
                                                                 ECalComponent           *comp);

static void           gcal_search_view_component_modified       (ECalDataModelSubscriber *subscriber,
                                                                 ECalClient              *client,
                                                                 ECalComponent           *comp);

static void           gcal_search_view_component_removed        (ECalDataModelSubscriber *subscriber,
                                                                 ECalClient              *client,
                                                                 const gchar             *uid,
                                                                 const gchar             *rid);

static void           gcal_search_view_freeze                   (ECalDataModelSubscriber *subscriber);

static void           gcal_search_view_thaw                     (ECalDataModelSubscriber *subscriber);

G_DEFINE_TYPE_WITH_CODE (GcalSearchView,
                         gcal_search_view,
                         GTK_TYPE_SCROLLED_WINDOW,
                         G_ADD_PRIVATE (GcalSearchView)
                         G_IMPLEMENT_INTERFACE (E_TYPE_CAL_DATA_MODEL_SUBSCRIBER,
                                                gcal_data_model_subscriber_interface_init)
                         G_IMPLEMENT_INTERFACE (GCAL_TYPE_VIEW, gcal_view_interface_init));


static GtkWidget*
get_event_from_grid (GtkWidget *grid)
{
  return gtk_grid_get_child_at (GTK_GRID (grid), 1, 0);
}

static GtkWidget*
make_grid_for_event (GcalSearchView  *view,
                     GcalEventWidget *event)
{
  GcalSearchViewPrivate *priv;
  GDateTime *datetime;
  GtkWidget *grid;
  GtkWidget *box;

  gchar *text;
  GtkWidget *start_date;
  GtkWidget *start_time;

  icaltimetype *start;
  gboolean all_day;

  priv = gcal_search_view_get_instance_private (view);
  start = gcal_event_widget_get_date (event);
  all_day = gcal_event_widget_get_all_day (event);

  /* event widget properties */
  gtk_widget_set_valign (GTK_WIDGET (event), GTK_ALIGN_CENTER);
  gtk_widget_set_hexpand (GTK_WIDGET (event), TRUE);

  /* grid & box*/
  grid = gtk_grid_new ();
  box = gtk_grid_new ();

  g_object_set (grid,
                "column-spacing", 6,
                "border-width", 6,
                "margin-start", 12,
                "margin-end", 12,
                "hexpand", TRUE, NULL);

  gtk_grid_set_column_spacing (GTK_GRID (box), 6);
  gtk_grid_set_column_homogeneous (GTK_GRID (box), TRUE);
  gtk_widget_set_valign (box, GTK_ALIGN_CENTER);
  gtk_widget_set_margin_end (box, 6);

  /* start date & time */
  datetime = g_date_time_new_local (start->year, start->month, start->day, start->hour, start->minute, start->second);
  text = g_date_time_format (datetime, priv->date_mask);
  start_date = gtk_label_new (text);
  g_free (text);

  if (!all_day)
    {
      text = g_date_time_format (datetime, priv->time_mask);
      start_time = gtk_label_new (text);
      g_free (text);
    }
  else
    {
      start_time = gtk_label_new (NULL);
    }

  g_date_time_unref (datetime);
  g_free (start);

  /* labels: 10%; event widget: 90% */
  gtk_grid_attach (GTK_GRID (box), start_date, 0, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (box), start_time, 1, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), GTK_WIDGET (event), 1, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), box, 0, 0, 1, 1);
  gtk_widget_show (start_date);
  gtk_widget_show (start_time);
  gtk_widget_show (box);
  gtk_widget_show (grid);
  gtk_widget_show (GTK_WIDGET (event));

  return grid;
}

static gint
sort_by_event (GtkListBoxRow *row1,
               GtkListBoxRow *row2,
               gpointer       user_data)
{
  GcalEventWidget *ev1, *ev2;
  icaltimetype *date1, *date2;
  gint result;

  ev1 = GCAL_EVENT_WIDGET (get_event_from_grid (gtk_bin_get_child (GTK_BIN (row1))));
  ev2 = GCAL_EVENT_WIDGET (get_event_from_grid (gtk_bin_get_child (GTK_BIN (row2))));
  date1 = gcal_event_widget_get_date (ev1);
  date2 = gcal_event_widget_get_date (ev2);

  /* First, compare by their dates */
  result = icaltime_compare (*date1, *date2);
  g_free (date1);
  g_free (date2);

  if (result != 0)
    return -1 * result;

  /* Second, by their names */
  return -1 * g_strcmp0 (gcal_event_widget_peek_uuid (ev1), gcal_event_widget_peek_uuid (ev2));
}

static void
open_event (GcalEventWidget *event_widget,
            gpointer         user_data)
{
  g_signal_emit_by_name (GCAL_VIEW (user_data), "event-activated", event_widget);
}

static void
gcal_search_view_class_init (GcalSearchViewClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->constructed = gcal_search_view_constructed;
  object_class->set_property = gcal_search_view_set_property;
  object_class->get_property = gcal_search_view_get_property;
  object_class->finalize = gcal_search_view_finalize;

  g_object_class_install_property (
      object_class,
      PROP_DATE,
      g_param_spec_boxed ("active-date",
                          "The active date",
                          "The active/selected date in the view",
                          ICAL_TIME_TYPE,
                          G_PARAM_READWRITE));

  g_object_class_install_property (
      object_class,
      PROP_MANAGER,
      g_param_spec_pointer ("manager",
                            "The manager object",
                            "A weak reference to the app manager object",
                            G_PARAM_CONSTRUCT_ONLY |
                            G_PARAM_READWRITE));
}

static void
gcal_view_interface_init (GcalViewIface *iface)
{
  iface->get_initial_date = NULL;
  iface->get_final_date = NULL;

  iface->clear_marks = NULL;

  iface->get_left_header = NULL;
  iface->get_right_header = NULL;

  iface->get_by_uuid = NULL;
}

static void
gcal_search_view_init (GcalSearchView *self)
{
  GcalSearchViewPrivate *priv;

  priv = gcal_search_view_get_instance_private (self);

  priv->date_mask = nl_langinfo (D_FMT);
  priv->time_mask = "%H:%M";
}

static void
gcal_data_model_subscriber_interface_init (ECalDataModelSubscriberInterface *iface)
{
  iface->component_added = gcal_search_view_component_added;
  iface->component_modified = gcal_search_view_component_modified;
  iface->component_removed = gcal_search_view_component_removed;
  iface->freeze = gcal_search_view_freeze;
  iface->thaw = gcal_search_view_thaw;
}

static void
gcal_search_view_constructed (GObject *object)
{
  GcalSearchViewPrivate *priv;
  GtkWidget *frame;

  priv =
    gcal_search_view_get_instance_private (GCAL_SEARCH_VIEW (object));

  /* frame */
  frame = gtk_frame_new (NULL);
  gtk_widget_set_margin_start (frame, 96);
  gtk_widget_set_margin_end (frame, 96);
  gtk_widget_set_margin_top (frame, 24);
  gtk_widget_set_margin_bottom (frame, 24);
  gtk_widget_show (frame);

  /* listbox */
  priv->listbox = gtk_list_box_new ();
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (priv->listbox),
                                   GTK_SELECTION_NONE);
  gtk_list_box_set_sort_func (GTK_LIST_BOX (priv->listbox), (GtkListBoxSortFunc) sort_by_event, NULL, NULL);
  gtk_widget_show (priv->listbox);

  gtk_container_add (GTK_CONTAINER (frame), priv->listbox);
  gtk_container_add (GTK_CONTAINER (object), frame);


  gcal_manager_set_search_subscriber (
      priv->manager,
      E_CAL_DATA_MODEL_SUBSCRIBER (object),
      0, 0);
}

static void
gcal_search_view_set_property (GObject       *object,
                               guint          property_id,
                               const GValue  *value,
                               GParamSpec    *pspec)
{
  GcalSearchViewPrivate *priv;

  priv =
    gcal_search_view_get_instance_private (GCAL_SEARCH_VIEW (object));

  switch (property_id)
    {
    case PROP_DATE:
      {
        if (priv->date != NULL)
          g_free (priv->date);

        priv->date = g_value_dup_boxed (value);
        break;
      }
    case PROP_MANAGER:
      {
        priv->manager = g_value_get_pointer (value);
        break;
      }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gcal_search_view_get_property (GObject       *object,
                               guint          property_id,
                               GValue        *value,
                               GParamSpec    *pspec)
{
  GcalSearchViewPrivate *priv;

  priv = gcal_search_view_get_instance_private (GCAL_SEARCH_VIEW (object));

  switch (property_id)
    {
    case PROP_DATE:
      g_value_set_boxed (value, priv->date);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gcal_search_view_finalize (GObject       *object)
{
  GcalSearchViewPrivate *priv;
  priv = gcal_search_view_get_instance_private (GCAL_SEARCH_VIEW (object));

  if (priv->date != NULL)
    g_free (priv->date);

  /* Chain up to parent's finalize() method. */
  G_OBJECT_CLASS (gcal_search_view_parent_class)->finalize (object);
}

/* ECalDataModelSubscriber interface API */
static void
gcal_search_view_component_added (ECalDataModelSubscriber *subscriber,
                                  ECalClient              *client,
                                  ECalComponent           *comp)
{
  GcalSearchViewPrivate *priv;

  GtkWidget *event;
  GtkWidget *grid;
  GcalEventData *data;

  priv =
    gcal_search_view_get_instance_private (GCAL_SEARCH_VIEW (subscriber));

  data = g_new0 (GcalEventData, 1);
  data->source = e_client_get_source (E_CLIENT (client));
  data->event_component = e_cal_component_clone (comp);

  event = gcal_event_widget_new_from_data (data);
  gcal_event_widget_set_read_only (GCAL_EVENT_WIDGET (event), e_client_is_readonly (E_CLIENT (client)));
  g_signal_connect (event, "activate", G_CALLBACK (open_event), subscriber);
  g_free (data);

  grid = make_grid_for_event (GCAL_SEARCH_VIEW (subscriber), GCAL_EVENT_WIDGET (event));
  gtk_container_add (GTK_CONTAINER (priv->listbox), grid);
}

static void
gcal_search_view_component_modified (ECalDataModelSubscriber *subscriber,
                                     ECalClient              *client,
                                     ECalComponent           *comp)
{
  ;
}

static void
gcal_search_view_component_removed (ECalDataModelSubscriber *subscriber,
                                    ECalClient              *client,
                                    const gchar             *uid,
                                    const gchar             *rid)
{
  GcalSearchViewPrivate *priv;
  GList *children, *aux;
  ESource *source;

  priv = gcal_search_view_get_instance_private (GCAL_SEARCH_VIEW (subscriber));
  children = gtk_container_get_children (GTK_CONTAINER (priv->listbox));
  source = e_client_get_source (E_CLIENT (client));

  /* search for the event */
  for (aux = children; aux != NULL; aux = aux->next)
    {
      GcalEventWidget *event_widget;
      GtkWidget *row;
      gchar *uuid;

      row = aux->data;
      event_widget = GCAL_EVENT_WIDGET (get_event_from_grid (gtk_bin_get_child (GTK_BIN (row))));

      /* if the widget has recurrency, it's UUID is different */
      if (rid != NULL)
        uuid = g_strdup_printf ("%s:%s:%s", e_source_get_uid (source), uid, rid);
      else
        uuid = g_strdup_printf ("%s:%s", e_source_get_uid (source), uid);

      /* compare widget by uid */
      if (event_widget != NULL && g_strcmp0 (gcal_event_widget_peek_uuid (event_widget), uuid) == 0)
        {
          gtk_container_remove (GTK_CONTAINER (priv->listbox), row);
        }

      g_free (uuid);
    }

  g_list_free (children);
}

static void
gcal_search_view_freeze (ECalDataModelSubscriber *subscriber)
{
  ;
}

static void
gcal_search_view_thaw (ECalDataModelSubscriber *subscriber)
{
}

/* Public API */
/**
 * gcal_search_view_new:
 * @manager: App singleton #GcalManager instance
 *
 * Since: 0.1
 * Create a new month view widget
 *
 * Returns: (transfer full):
 **/
GtkWidget*
gcal_search_view_new (GcalManager *manager)
{
  return g_object_new (GCAL_TYPE_SEARCH_VIEW, "manager", manager, NULL);
}
