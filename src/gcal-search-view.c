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
  gchar          *uuid;
} RowEventData;

typedef struct
{
  GtkWidget      *listbox;
  GtkWidget      *scrolled_window;
  GtkWidget      *no_results_grid;

  /* Since the user can have (literally)
   * thousands of events, the usage of
   * the following hash table is to turn
   * the RowEventData lookup constant
   * time.
   */
  GHashTable     *uuid_to_event;

  /* misc */
  gint            no_results_timeout_id;
  gint            num_results;
  gchar          *field;
  gchar          *query;
  time_t          current_utc_date;

  /* property */
  icaltimetype   *date;
  GcalManager    *manager; /* weak reference */

  /* flags */
  gboolean        format_24h;
  gboolean        subscribed;
} GcalSearchViewPrivate;

enum
{
  PROP_0,
  PROP_DATE,  /* active-date inherited property */
};

enum
{
  EVENT_ACTIVATED,
  NUM_SIGNALS
};

static guint signals[NUM_SIGNALS] = { 0, };

#define NO_RESULT_TIMEOUT 250 /* ms */

/* callbacks */
static void           display_header_func                       (GtkListBoxRow        *row,
                                                                 GtkListBoxRow        *before,
                                                                 gpointer              user_data);

static void           open_event                                (GtkListBox           *list,
                                                                 GtkListBoxRow        *row,
                                                                 gpointer              user_data);

static gint           sort_by_event                             (GtkListBoxRow        *row1,
                                                                 GtkListBoxRow        *row2,
                                                                 gpointer              user_data);

static gint           compare_events                            (GcalEventData        *ev1,
                                                                 GcalEventData        *ev2,
                                                                 time_t               *utc);

static gboolean       show_no_results_page                      (GcalSearchView       *view);

/* private */
static void           free_row_data                             (RowEventData          *data);

static GtkWidget*     make_row_for_event_data                   (GcalSearchView       *view,
                                                                 GcalEventData        *data);

static void           update_view                               (GcalSearchView       *view);

/* prefixed */
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
                         GTK_TYPE_POPOVER,
                         G_ADD_PRIVATE (GcalSearchView)
                         G_IMPLEMENT_INTERFACE (E_TYPE_CAL_DATA_MODEL_SUBSCRIBER,
                                                gcal_data_model_subscriber_interface_init));

/**
 * display_header_func:
 *
 * Shows a separator before each row.
 *
 */
static void
display_header_func (GtkListBoxRow *row,
                     GtkListBoxRow *before,
                     gpointer       user_data)
{
  if (before != NULL)
    {
      GtkWidget *header;

      header = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_widget_show (header);

      gtk_list_box_row_set_header (row, header);
    }
}

/**
 * open_event:
 * @list: the source #GtkListBox
 * @row: the activated #GtkListBoxRow
 * @user_data: a #GcalSearchView instance.
 *
 * Fire GcalSearchView::'event-activated' event
 * when the @row is activated by the user.
 *
 * It is up to #GcalWindow to hear the signal,
 * retrieve the #icaltimetype passed as parameter
 * and select the day from the last view.
 *
 * Returns:
 */
static void
open_event (GtkListBox    *list,
            GtkListBoxRow *row,
            gpointer       user_data)
{
  RowEventData *data;
  ECalComponentDateTime dt;

  data = g_object_get_data (G_OBJECT (row), "event-data");

  e_cal_component_get_dtstart (data->event_data->event_component, &dt);

  g_signal_emit_by_name (user_data, "event-activated", dt.value);

  e_cal_component_free_datetime (&dt);
}

/**
 * sort_by_event:
 * @row1: the current #GtkListBoxRow being iterated.
 * @row2: the reference #GtkListBoxRow.
 * @user_data: a #GcalSearchView instance
 *
 * A #GtkListBoxSortFunc specialy crafted to sort
 * event rows.
 *
 * It first compares the summary (i.e. event name).
 * If the names are equal, they are compared by their
 * date.
 *
 * Returns: -1 if @row1 should be placed before @row2, 0 if
 * they are equal, 1 otherwise.
 */
static gint
sort_by_event (GtkListBoxRow *row1,
               GtkListBoxRow *row2,
               gpointer       user_data)
{
  GcalSearchViewPrivate *priv;
  RowEventData *rd1, *rd2;
  GcalEventData *ev1, *ev2;

  priv = gcal_search_view_get_instance_private (GCAL_SEARCH_VIEW (user_data));

  /* retrieve event data */
  rd1 = g_object_get_data (G_OBJECT (row1), "event-data");
  rd2 = g_object_get_data (G_OBJECT (row2), "event-data");

  ev1 = rd1->event_data;
  ev2 = rd2->event_data;

  if (ev1 == NULL || ev2 == NULL)
      return 0;

  return compare_events (ev1, ev2, &(priv->current_utc_date));
}

static gint
compare_events (GcalEventData *ev1,
                GcalEventData *ev2,
                time_t        *utc)
{
  ECalComponentDateTime date1, date2;
  gint result;

  e_cal_component_get_dtstart (ev1->event_component, &date1);
  e_cal_component_get_dtstart (ev2->event_component, &date2);

  if (date1.tzid != NULL)
    date1.value->zone = icaltimezone_get_builtin_timezone_from_tzid (date1.tzid);
  if (date2.tzid != NULL)
    date2.value->zone = icaltimezone_get_builtin_timezone_from_tzid (date2.tzid);
  result = icaltime_compare_with_current (date1.value, date2.value, utc);

  e_cal_component_free_datetime (&date1);
  e_cal_component_free_datetime (&date2);

  return result;
}

/**
 * show_no_results_page:
 * @view: a #GcalSearchView instance.
 *
 * Callback to update the 'No results found' page.
 *
 * Returns: @G_SOURCE_REMOVE
 */
static gboolean
show_no_results_page (GcalSearchView *view)
{
  GcalSearchViewPrivate *priv;

  priv = gcal_search_view_get_instance_private (view);
  priv->no_results_timeout_id = 0;

  gtk_widget_set_visible (priv->scrolled_window, priv->num_results != 0);
  gtk_widget_set_visible (priv->no_results_grid, priv->num_results == 0);

  return G_SOURCE_REMOVE;
}

/**
 * free_row_data:
 * @data: a #RowEventData instance.
 *
 * Deallocate @data memory by destroying the #GtkListBoxRow
 * and freeing the #GcalEventData associated from the structure.
 *
 * Returns:
 */
static void
free_row_data (RowEventData *data)
{
  g_assert_nonnull (data);

  if (data->row != NULL)
    gtk_widget_destroy (GTK_WIDGET (data->row));

  g_object_unref (data->event_data->event_component);

  g_free (data->event_data);
  g_free (data->uuid);
  g_free (data);
}

/**
 * make_row_for_event_data:
 * @view: a #GcalSearchView
 * @data: a #GcalEventData pointer
 *
 * Creates a new #GtkListBoxRow with the event information
 * as input.
 *
 * Returns: the newly created #GtkListBoxRow with the event data
 */
static GtkWidget*
make_row_for_event_data (GcalSearchView  *view,
                         GcalEventData   *data)
{
  GcalSearchViewPrivate *priv;

  g_autoptr(GTimeZone) tz;
  g_autoptr (GDateTime) datetime;
  g_autoptr (GDateTime) local_datetime;
  ECalComponentDateTime comp_dt;
  ECalComponentText summary;
  GdkRGBA color;
  GdkPixbuf *pixbuf;

  GtkWidget *row;
  GtkWidget *grid;
  GtkWidget *name_box;
  GtkWidget *box;

  gchar *text;
  GtkWidget *date_label;
  GtkWidget *time_label;
  GtkWidget *name_label;
  GtkWidget *image;

  priv = gcal_search_view_get_instance_private (view);

  /* get event color */
  get_color_name_from_source (data->source, &color);
  pixbuf = gcal_get_pixbuf_from_color (&color, 16);

  /* make an image of the color */
  image = gtk_image_new_from_pixbuf (pixbuf);

  /* date */
  e_cal_component_get_dtstart (data->event_component, &comp_dt);
  e_cal_component_get_summary (data->event_component, &summary);

  /* grid & box*/
  row = gtk_list_box_row_new ();
  grid = gtk_grid_new ();
  name_box = gtk_grid_new ();
  box = gtk_grid_new ();

  g_object_set (grid,
                "column-spacing", 12,
                "border-width", 10,
                "margin-start", 16,
                "margin-end", 16,
                "hexpand", TRUE, NULL);

  gtk_grid_set_column_spacing (GTK_GRID (box), 12);
  gtk_grid_set_column_spacing (GTK_GRID (name_box), 12);
  gtk_widget_set_valign (box, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (name_box, GTK_ALIGN_CENTER);
  gtk_widget_set_margin_end (box, 6);
  gtk_widget_set_hexpand (name_box, TRUE);

  /* start date & time */
  if (comp_dt.tzid != NULL)
    tz = g_time_zone_new (comp_dt.tzid);
  else if (comp_dt.value->zone != NULL)
    tz = g_time_zone_new (icaltimezone_get_tzid ((icaltimezone*) comp_dt.value->zone));
  else
    tz = g_time_zone_new_local ();

  datetime = g_date_time_new (tz,
                              comp_dt.value->year, comp_dt.value->month, comp_dt.value->day,
                              comp_dt.value->hour, comp_dt.value->minute, comp_dt.value->second);
  local_datetime = g_date_time_to_local (datetime);
  text = g_date_time_format (local_datetime, "%x");
  date_label = gtk_label_new (text);
  gtk_label_set_width_chars (GTK_LABEL (date_label), 11);
  g_free (text);

  /* show 'all day' instead of 00:00 */
  if (comp_dt.value->is_date == 0)
    {
      text = g_date_time_format (local_datetime, priv->format_24h ? "%R" : "%r");
      time_label = gtk_label_new (text);
      g_free (text);
    }
  else
    {
      time_label = gtk_label_new (_("All day"));
    }

  gtk_label_set_width_chars (GTK_LABEL (time_label), 12);
  gtk_style_context_add_class (gtk_widget_get_style_context (time_label), "dim-label");

  /* name label */
  name_label = gtk_label_new (summary.value);
  gtk_widget_set_hexpand (name_label, TRUE);
  gtk_widget_set_halign (name_label, GTK_ALIGN_START);
  gtk_label_set_ellipsize (GTK_LABEL (name_label), PANGO_ELLIPSIZE_END);

  /* alarm icon */
  if (e_cal_component_has_alarms (data->event_component))
    {
      GtkWidget *alarm_icon;

      alarm_icon = gtk_image_new_from_icon_name ("alarm-symbolic", GTK_ICON_SIZE_MENU);
      gtk_grid_attach (GTK_GRID (name_box), alarm_icon, 2, 0, 1, 1);
    }

  /* lock icon */
  if (gcal_manager_is_client_writable (priv->manager, data->source))
    {
      GtkWidget *lock_icon;

      lock_icon = gtk_image_new_from_icon_name ("changes-prevent-symbolic", GTK_ICON_SIZE_MENU);
      gtk_grid_attach (GTK_GRID (name_box), lock_icon, 3, 0, 1, 1);
    }

  /* attach things up */
  gtk_grid_attach (GTK_GRID (box), time_label, 0, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (box), date_label, 1, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (name_box), image, 0, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (name_box), name_label, 1, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), name_box, 0, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), box, 1, 0, 1, 1);
  gtk_container_add (GTK_CONTAINER (row), grid);

  gtk_widget_show_all (row);

  e_cal_component_free_datetime (&comp_dt);
  g_object_unref (pixbuf);
  return row;
}

/**
 * update_view:
 * @view: a #GcalSearchView instance.
 *
 * Updates the timeout for the 'No results found'
 * page.
 *
 * Returns:
 */
static void
update_view (GcalSearchView *view)
{
  GcalSearchViewPrivate *priv;
  priv = gcal_search_view_get_instance_private (view);

  if (priv->no_results_timeout_id != 0)
    g_source_remove (priv->no_results_timeout_id);

  priv->no_results_timeout_id = g_timeout_add (NO_RESULT_TIMEOUT, (GSourceFunc) show_no_results_page, view);
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
  /**
   * GcalSearchView::event-activated:
   * @view: the #GcalSearchView that generated the signal
   * @date: the date of the selected event
   *
   * Emitted when an event is selected from the list.
   *
   */
  signals[EVENT_ACTIVATED] = g_signal_new ("event-activated", GCAL_TYPE_SEARCH_VIEW, G_SIGNAL_RUN_LAST,
                                           G_STRUCT_OFFSET (GcalSearchViewClass, event_activated),
                                           NULL, NULL, NULL,
                                           G_TYPE_NONE, 1, ICAL_TIME_TYPE);

  /* properties */
  /**
   * GcalSearchView::active-date:
   *
   * The date from this view, as defined by #GcalWindow.
   * Actually it is not used.
   *
   */
  g_object_class_install_property (object_class, PROP_DATE,
      g_param_spec_boxed ("active-date",
                          "The active date",
                          "The active/selected date in the view",
                          ICAL_TIME_TYPE, G_PARAM_READWRITE));

  /* bind things for/from the template class */
  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/org/gnome/calendar/search-view.ui");

  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GcalSearchView, no_results_grid);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GcalSearchView, scrolled_window);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GcalSearchView, listbox);

  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), open_event);
}

static void
gcal_search_view_init (GcalSearchView *self)
{
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
  priv = gcal_search_view_get_instance_private (GCAL_SEARCH_VIEW (object));

  /* make the listbox sorted */
  gtk_list_box_set_sort_func (GTK_LIST_BOX (priv->listbox), (GtkListBoxSortFunc) sort_by_event, object, NULL);
  gtk_list_box_set_header_func (GTK_LIST_BOX (priv->listbox), display_header_func, NULL, NULL);

  // (gchar)uuid -> (RowEventData)event Hash table
  priv->uuid_to_event = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, (GDestroyNotify) free_row_data);

  /* don't fill the list with all events on startup */
  gcal_search_view_search (GCAL_SEARCH_VIEW (object), NULL, NULL);
}

static void
gcal_search_view_set_property (GObject       *object,
                               guint          property_id,
                               const GValue  *value,
                               GParamSpec    *pspec)
{
  GcalSearchViewPrivate *priv;

  priv = gcal_search_view_get_instance_private (GCAL_SEARCH_VIEW (object));

  switch (property_id)
    {
    case PROP_DATE:
      {
        if (priv->date != NULL)
          g_free (priv->date);

        priv->date = g_value_dup_boxed (value);
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

  g_hash_table_unref (priv->uuid_to_event);

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

  priv = gcal_search_view_get_instance_private (GCAL_SEARCH_VIEW (subscriber));

  /* event data */
  data = g_new0 (GcalEventData, 1);
  data->source = e_client_get_source (E_CLIENT (client));
  data->event_component = e_cal_component_clone (comp);

  /* build uuid */
  id = e_cal_component_get_id (comp);

  if (id->rid != NULL)
    uuid = g_strdup_printf ("%s:%s:%s", e_source_get_uid (data->source), id->uid, id->rid);
  else
    uuid = g_strdup_printf ("%s:%s", e_source_get_uid (data->source), id->uid);

  /* insert row_data at the hash of events */
  row_data = g_new0 (RowEventData, 1);
  row_data->event_data = data;
  row_data->uuid = uuid;
  row_data->row = make_row_for_event_data (GCAL_SEARCH_VIEW (subscriber), data);
  g_signal_connect (row_data->row, "destroy", G_CALLBACK (gtk_widget_destroyed), &(row_data->row));

  g_object_set_data (G_OBJECT (row_data->row), "event-data", row_data);
  g_hash_table_insert (priv->uuid_to_event, uuid, row_data);

  gtk_container_add (GTK_CONTAINER (priv->listbox), row_data->row);

  /* show 'no results' */
  priv->num_results++;

  update_view (GCAL_SEARCH_VIEW (subscriber));
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

  /* if the widget has recurrency, it's UUID is different */
  source = e_client_get_source (E_CLIENT (client));

  if (rid != NULL)
    uuid = g_strdup_printf ("%s:%s:%s", e_source_get_uid (source), uid, rid);
  else
    uuid = g_strdup_printf ("%s:%s", e_source_get_uid (source), uid);

  // Lookup the RowEventData
  row_data = g_hash_table_lookup (priv->uuid_to_event, uuid);

  /* Removing the given RowEventData entry will
   * call free_row_data, which removes the row
   * from the listbox and also free the uuid.
   */
  if (row_data)
    g_hash_table_remove (priv->uuid_to_event, uuid);

  g_free (uuid);

  /* show 'no results' */
  priv->num_results--;

  update_view (GCAL_SEARCH_VIEW (subscriber));
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
 *
 * Since: 0.1
 * Create a new month view widget
 *
 * Returns: (transfer full):
 **/
GtkWidget*
gcal_search_view_new (void)
{
  return g_object_new (GCAL_TYPE_SEARCH_VIEW, NULL);
}

/**
 * gcal_search_view_connect:
 * @search_view: a #GcalSearchView instance
 * @manager: App singleton #GcalManager instance
 *
 * Connect the view to the App singleton manager instance.
 * This is designed to be called once, after create of the view and before any use.
 */
void
gcal_search_view_connect (GcalSearchView *search_view,
                          GcalManager    *manager)
{
  GcalSearchViewPrivate *priv;

  priv = gcal_search_view_get_instance_private (search_view);
  if (manager != NULL && manager != priv->manager)
    priv->manager = manager;
}

/**
 * gcal_search_view_set_time_format:
 * @view: a #GcalSearchView instance.
 * @format_24h: whether is 24h or not.
 *
 * Setup time format, instead of accessing DConf
 * again.
 *
 */
void
gcal_search_view_set_time_format (GcalSearchView *view,
                                  gboolean        format_24h)
{
  GcalSearchViewPrivate *priv;

  priv = gcal_search_view_get_instance_private (view);
  priv->format_24h = format_24h;
}

/**
 * gcal_search_view_set_search:
 * @view: a #GcalSearchView instance
 * @field: the field to perform the search.
 * @query: what the search will look for.
 *
 *
 *
 * Returns:
 */
void
gcal_search_view_search (GcalSearchView *view,
                         const gchar    *field,
                         const gchar    *query)
{
  GcalSearchViewPrivate *priv;

  priv = gcal_search_view_get_instance_private (view);

  if (priv->query != NULL)
    g_free (priv->query);
  if (priv->field != NULL)
    g_free (priv->field);

  priv->query = g_strdup (query);
  priv->field = g_strdup (field);

  gtk_widget_show (priv->scrolled_window);
  gtk_widget_hide (priv->no_results_grid);

  /* Only perform search on valid non-empty strings */
  if (query && g_utf8_strlen (query, -1) > 0)
    {
      gchar *search_query = g_strdup_printf ("(contains? \"%s\" \"%s\")", field != NULL? field : "summary",
                                             query != NULL? query : "");

      if (!priv->subscribed)
      {
        gcal_manager_set_search_subscriber (priv->manager, E_CAL_DATA_MODEL_SUBSCRIBER (view), 0, 0);
        priv->subscribed = TRUE;
      }

      /* update internal current time_t */
      priv->current_utc_date = time (NULL);

      gcal_manager_set_query (priv->manager, search_query);
      gtk_widget_show (priv->listbox);

      update_view (view);

      g_free (search_query);
    }
  else
    {
      g_hash_table_remove_all (priv->uuid_to_event);
      gtk_widget_hide (priv->listbox);
    }
}

