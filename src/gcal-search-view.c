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

#include <glib/gi18n.h>

typedef struct
{
  GcalEventData  *event_data;
  GtkWidget      *row;
} RowEventData;

typedef struct
{
  GtkWidget      *listbox;
  GtkWidget      *frame;
  GtkWidget      *no_results_grid;

  /* internal hash of uuid:row_data */
  GHashTable     *events;
  GHashTable     *row_to_event;

  /* misc */
  gint            no_results_timeout_id;
  gint            num_results;
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

enum
{
  EVENT_ACTIVATED,
  NUM_SIGNALS
};

static guint signals[NUM_SIGNALS] = { 0, };

#define NO_RESULT_TIMEOUT 250 /* ms */

static GtkWidget*     make_row_for_event_data                   (GcalSearchView       *view,
                                                                 GcalEventData        *data);

static gint           sort_by_event                             (GtkListBoxRow        *row1,
                                                                 GtkListBoxRow        *row2,
                                                                 gpointer              user_data);

static void           open_event                                (GtkListBox           *list,
                                                                 GtkListBoxRow        *row,
                                                                 gpointer              user_data);

static void           free_row_data                             (RowEventData          *data);

static gboolean       show_no_results_page                      (GcalSearchView       *view);

static void           gcal_data_model_subscriber_interface_init (ECalDataModelSubscriberInterface *iface);

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
                                                gcal_data_model_subscriber_interface_init));

static GtkWidget*
make_row_for_event_data (GcalSearchView  *view,
                         GcalEventData   *data)
{
  GcalSearchViewPrivate *priv;

  GDateTime *datetime;
  ECalComponentDateTime comp_dt;
  ECalComponentText summary;
  ESourceSelectable *extension;
  GdkRGBA color;
  GdkPixbuf *pixbuf;

  GtkWidget *row;
  GtkWidget *grid;
  GtkWidget *box;

  gchar *text;
  GtkWidget *date_label;
  GtkWidget *time_label;
  GtkWidget *name_label;
  GtkWidget *image;

  priv = gcal_search_view_get_instance_private (view);

  /* event color */
  extension = E_SOURCE_SELECTABLE (e_source_get_extension (data->source, E_SOURCE_EXTENSION_CALENDAR));
  gdk_rgba_parse (&color, e_source_selectable_get_color (E_SOURCE_SELECTABLE (extension)));

  pixbuf = gcal_get_pixbuf_from_color (&color, 16);

  image = gtk_image_new_from_pixbuf (pixbuf);

  /* date */
  e_cal_component_get_dtstart (data->event_component, &comp_dt);
  e_cal_component_get_summary (data->event_component, &summary);

  /* grid & box*/
  row = gtk_list_box_row_new ();
  grid = gtk_grid_new ();
  box = gtk_grid_new ();

  g_object_set (grid,
                "column-spacing", 12,
                "border-width", 10,
                "margin-start", 16,
                "margin-end", 16,
                "hexpand", TRUE, NULL);

  gtk_grid_set_column_spacing (GTK_GRID (box), 12);
  gtk_grid_set_column_homogeneous (GTK_GRID (box), TRUE);
  gtk_widget_set_valign (box, GTK_ALIGN_CENTER);
  gtk_widget_set_margin_end (box, 6);

  /* start date & time */
  datetime = g_date_time_new_local (comp_dt.value->year, comp_dt.value->month, comp_dt.value->day, comp_dt.value->hour,
                                    comp_dt.value->minute, comp_dt.value->second);
  text = g_date_time_format (datetime, priv->date_mask);
  date_label = gtk_label_new (text);
  gtk_label_set_width_chars (GTK_LABEL (date_label), 14);
  g_free (text);

  if (comp_dt.value->is_date == 0)
    {
      text = g_date_time_format (datetime, priv->time_mask);
      time_label = gtk_label_new (text);
      g_free (text);
    }
  else
    {
      time_label = gtk_label_new (_("All day"));
    }

  gtk_label_set_width_chars (GTK_LABEL (date_label), 14);
  gtk_style_context_add_class (gtk_widget_get_style_context (time_label), "dim-label");

  /* name label */
  name_label = gtk_label_new (summary.value);
  gtk_widget_set_hexpand (name_label, TRUE);
  gtk_widget_set_halign (name_label, GTK_ALIGN_START);

  gtk_grid_attach (GTK_GRID (box), time_label, 0, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (box), date_label, 1, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), image, 0, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), name_label, 1, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), box, 2, 0, 1, 1);
  gtk_container_add (GTK_CONTAINER (row), grid);

  gtk_widget_show (image);
  gtk_widget_show (date_label);
  gtk_widget_show (time_label);
  gtk_widget_show (name_label);
  gtk_widget_show (box);
  gtk_widget_show (grid);
  gtk_widget_show (row);

  g_date_time_unref (datetime);
  e_cal_component_free_datetime (&comp_dt);
  g_object_unref (pixbuf);

  return row;
}

static gint
sort_by_event (GtkListBoxRow *row1,
               GtkListBoxRow *row2,
               gpointer       user_data)
{
  GcalSearchViewPrivate *priv;
  GcalEventData *ev1, *ev2;
  ECalComponentText summary1, summary2;
  ECalComponentDateTime date1, date2;
  gchar *down1, *down2;
  gint result;

  priv = gcal_search_view_get_instance_private (GCAL_SEARCH_VIEW (user_data));

  /* retrieve event data */
  ev1 = g_hash_table_lookup (priv->row_to_event, row1);
  ev2 = g_hash_table_lookup (priv->row_to_event, row2);

  if (ev1 == NULL || ev2 == NULL)
      return 0;

  e_cal_component_get_summary (ev1->event_component, &summary1);
  e_cal_component_get_summary (ev2->event_component, &summary2);
  down1 = g_utf8_strdown (summary1.value, -1);
  down2 = g_utf8_strdown (summary2.value, -1);

  /* First, by their names */
  result = g_strcmp0 (down1, down2);
  g_free (down1);
  g_free (down2);

  if (result != 0)
    return result;

  e_cal_component_get_dtstart (ev1->event_component, &date1);
  e_cal_component_get_dtstart (ev2->event_component, &date2);

  /* Second, compare by their dates */
  result = icaltime_compare (*date1.value, *date2.value);
  e_cal_component_free_datetime (&date1);
  e_cal_component_free_datetime (&date2);

  return -1 * result;
}

static void
open_event (GtkListBox    *list,
            GtkListBoxRow *row,
            gpointer       user_data)
{
  GcalSearchViewPrivate *priv;
  GcalEventData *data;
  ECalComponentDateTime dt;
  icaltimetype *time;


  priv = gcal_search_view_get_instance_private (GCAL_SEARCH_VIEW (user_data));
  data = g_hash_table_lookup (priv->row_to_event, row);

  e_cal_component_get_dtstart (data->event_component, &dt);
  time = gcal_dup_icaltime (dt.value);

  g_signal_emit_by_name (user_data, "event-activated", time);

  e_cal_component_free_datetime (&dt);
}

static void
free_row_data (RowEventData *data)
{
  g_assert_nonnull (data);

  gtk_widget_destroy (GTK_WIDGET (data->row));

  g_object_unref (data->event_data->event_component);

  g_free (data->event_data);
  g_free (data);
}

static gboolean
show_no_results_page (GcalSearchView *view)
{
  GcalSearchViewPrivate *priv;

  priv = gcal_search_view_get_instance_private (view);
  priv->no_results_timeout_id = 0;

  gtk_widget_set_visible (priv->frame, priv->num_results != 0);
  gtk_widget_set_visible (priv->no_results_grid, priv->num_results == 0);

  return G_SOURCE_REMOVE;
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

  /* signals */
  signals[EVENT_ACTIVATED] = g_signal_new ("event-activated", GCAL_TYPE_SEARCH_VIEW, G_SIGNAL_RUN_LAST,
                                           G_STRUCT_OFFSET (GcalSearchViewClass, event_activated),
                                           NULL, NULL, NULL,
                                           G_TYPE_NONE, 1, ICAL_TIME_TYPE);

  /* properties */
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

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/org/gnome/calendar/search-view.ui");

  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GcalSearchView, no_results_grid);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GcalSearchView, frame);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GcalSearchView, listbox);

  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), open_event);
}

static void
gcal_search_view_init (GcalSearchView *self)
{
  GcalSearchViewPrivate *priv;

  priv = gcal_search_view_get_instance_private (self);

  priv->events = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) free_row_data);
  priv->row_to_event = g_hash_table_new (g_direct_hash, g_direct_equal);
  priv->date_mask = "%d %b %Y";
  priv->time_mask = "%H:%M";

  gtk_widget_init_template (GTK_WIDGET (self));
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

  priv =
    gcal_search_view_get_instance_private (GCAL_SEARCH_VIEW (object));

  /* listbox */
  gtk_list_box_set_sort_func (GTK_LIST_BOX (priv->listbox), (GtkListBoxSortFunc) sort_by_event, object, NULL);

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

  RowEventData *row_data;
  GcalEventData *data;
  ECalComponentId *id;
  gchar *uuid;

  priv =
    gcal_search_view_get_instance_private (GCAL_SEARCH_VIEW (subscriber));

  data = g_new0 (GcalEventData, 1);
  data->source = e_client_get_source (E_CLIENT (client));
  data->event_component = e_cal_component_clone (comp);

  id = e_cal_component_get_id (comp);

  /* build uuid */
  if (id->rid != NULL)
    uuid = g_strdup_printf ("%s:%s:%s", e_source_get_uid (data->source), id->uid, id->rid);
  else
    uuid = g_strdup_printf ("%s:%s", e_source_get_uid (data->source), id->uid);

  /* insert row_data at the hash of events */
  row_data = g_new0 (RowEventData, 1);
  row_data->event_data = data;
  row_data->row = make_row_for_event_data (GCAL_SEARCH_VIEW (subscriber), data);

  g_hash_table_insert (priv->row_to_event, row_data->row, data);
  g_hash_table_insert (priv->events, uuid, row_data);

  gtk_container_add (GTK_CONTAINER (priv->listbox), row_data->row);

  priv->num_results++;

  /* show the 'no results' page with a delay */
  if (priv->no_results_timeout_id != 0)
    g_source_remove (priv->no_results_timeout_id);

  priv->no_results_timeout_id = g_timeout_add (NO_RESULT_TIMEOUT, (GSourceFunc) show_no_results_page, subscriber);
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
  ESource *source;
  RowEventData *row_data;
  gchar *uuid;

  priv = gcal_search_view_get_instance_private (GCAL_SEARCH_VIEW (subscriber));
  source = e_client_get_source (E_CLIENT (client));

  /* if the widget has recurrency, it's UUID is different */
  if (rid != NULL)
    uuid = g_strdup_printf ("%s:%s:%s", e_source_get_uid (source), uid, rid);
  else
    uuid = g_strdup_printf ("%s:%s", e_source_get_uid (source), uid);

  row_data = g_hash_table_lookup (priv->events, uuid);

  g_hash_table_remove (priv->row_to_event, row_data->row);
  g_hash_table_remove (priv->events, uuid);

  priv->num_results--;

  /* show the 'no results' page with a delay */
  if (priv->no_results_timeout_id != 0)
    g_source_remove (priv->no_results_timeout_id);

  priv->no_results_timeout_id = g_timeout_add (NO_RESULT_TIMEOUT, (GSourceFunc) show_no_results_page, subscriber);
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
