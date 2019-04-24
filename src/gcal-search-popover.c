/* gcal-search-popover.c
 *
 * Copyright (C) 2014 - Erick Pérez Castellanos
 *               2018 - Georges Basile Stavracas Neto
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
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

#define G_LOG_DOMAIN "GcalSearchPopover"

#include "gcal-debug.h"
#include "gcal-enums.h"
#include "gcal-event.h"
#include "gcal-search-popover.h"
#include "gcal-utils.h"

#include <glib/gi18n.h>

typedef struct
{
  GcalEvent *event;
  GtkWidget *row;
} RowEventData;

struct _GcalSearchPopover
{
  GtkPopover          parent;

  GtkWidget          *listbox;
  GtkWidget          *stack;

  /* Since the user can have (literally)
   * thousands of events, the usage of
   * the following hash table is to turn
   * the RowEventData lookup constant
   * time.
   */
  GHashTable         *uuid_to_event;

  /* misc */
  gint                no_results_timeout_id;
  gint                num_results;
  gchar              *field;
  gchar              *query;
  time_t              current_utc_date;
  guint               search_timeout_id;

  /* property */
  icaltimetype       *date;
  GcalManager        *manager; /* weak reference */

  /* flags */
  gboolean            subscribed;

  GcalContext        *context;
};

enum
{
  PROP_0,
  PROP_CONTEXT,
  PROP_DATE,
  N_PROPS,
};

enum
{
  EVENT_ACTIVATED,
  NUM_SIGNALS
};

static guint signals[NUM_SIGNALS] = { 0, };

#define NO_RESULT_TIMEOUT 250 /* ms */

static void           gcal_data_model_subscriber_interface_init (ECalDataModelSubscriberInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GcalSearchPopover,
                         gcal_search_popover,
                         GTK_TYPE_POPOVER,
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
 * @user_data: a #GcalSearchPopover instance.
 *
 * Fire GcalSearchPopover::'event-activated' event
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
  icaltimetype *date_start;

  data = g_object_get_data (G_OBJECT (row), "event-data");
  date_start = datetime_to_icaltime (gcal_event_get_date_start (data->event));

  g_signal_emit_by_name (user_data, "event-activated", date_start);

  g_free (date_start);
}

/**
 * sort_by_event:
 * @row1: the current #GtkListBoxRow being iterated.
 * @row2: the reference #GtkListBoxRow.
 * @user_data: a #GcalSearchPopover instance
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
  GcalSearchPopover *self;
  RowEventData *rd1, *rd2;
  GcalEvent *ev1, *ev2;

  self = GCAL_SEARCH_POPOVER (user_data);

  /* retrieve event data */
  rd1 = g_object_get_data (G_OBJECT (row1), "event-data");
  rd2 = g_object_get_data (G_OBJECT (row2), "event-data");

  ev1 = rd1->event;
  ev2 = rd2->event;

  if (ev1 == NULL || ev2 == NULL)
      return 0;

  return gcal_event_compare_with_current (ev1, ev2, &(self->current_utc_date));
}

/**
 * show_no_results_page:
 * @view: a #GcalSearchPopover instance.
 *
 * Callback to update the 'No results found' page.
 *
 * Returns: @G_SOURCE_REMOVE
 */
static gboolean
show_no_results_page (GcalSearchPopover *self)
{
  self->no_results_timeout_id = 0;

  if (self->query)
    gtk_stack_set_visible_child_name (GTK_STACK (self->stack), self->num_results != 0 ? "results" : "no_results");
  else
    gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "results");

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

  g_object_unref (data->event);
  g_free (data);
}

/**
 * make_row_for_event_data:
 * @view: a #GcalSearchPopover
 * @data: a #GcalEventData pointer
 *
 * Creates a new #GtkListBoxRow with the event information
 * as input.
 *
 * Returns: the newly created #GtkListBoxRow with the event data
 */
static GtkWidget*
make_row_for_event (GcalSearchPopover *self,
                    GcalEvent         *event)
{
  GDateTime *local_datetime;
  GDateTime *date_start;
  cairo_surface_t *surface;

  GtkWidget *row;
  GtkWidget *grid;
  GtkWidget *name_box;
  GtkWidget *box;

  gchar *text;
  GtkWidget *date_label;
  GtkWidget *time_label;
  GtkWidget *name_label;
  GtkWidget *image;

  /* get event color */
  surface = gcal_get_surface_from_color (gcal_event_get_color (event), 16);

  /* make an image of the color */
  image = gtk_image_new_from_surface (surface);

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
  date_start = gcal_event_get_date_start (event);
  local_datetime = g_date_time_to_local (date_start);
  text = g_date_time_format (local_datetime, "%x");

  /* Date label */
  date_label = gtk_label_new (text);
  gtk_label_set_width_chars (GTK_LABEL (date_label), 11);
  g_free (text);

  /* show 'all day' instead of 00:00 */
  if (!gcal_event_get_all_day (event))
    {
      GcalTimeFormat time_format;

      time_format = gcal_context_get_time_format (self->context);
      text = g_date_time_format (local_datetime,
                                 time_format == GCAL_TIME_FORMAT_24H ? "%R" : "%r");
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
  name_label = gtk_label_new (gcal_event_get_summary (event));
  gtk_widget_set_hexpand (name_label, TRUE);
  gtk_widget_set_halign (name_label, GTK_ALIGN_START);
  gtk_label_set_ellipsize (GTK_LABEL (name_label), PANGO_ELLIPSIZE_END);

  /* alarm icon */
  if (e_cal_component_has_alarms (gcal_event_get_component (event)))
    {
      GtkWidget *alarm_icon;

      alarm_icon = gtk_image_new_from_icon_name ("alarm-symbolic", GTK_ICON_SIZE_MENU);
      gtk_grid_attach (GTK_GRID (name_box), alarm_icon, 2, 0, 1, 1);
    }

  /* lock icon */
  if (!gcal_manager_is_client_writable (self->manager, gcal_event_get_source (event)))
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

  g_clear_pointer (&local_datetime, g_date_time_unref);
  g_clear_pointer (&surface, cairo_surface_destroy);

  return row;
}

static void
update_view (GcalSearchPopover *self)
{
  GCAL_ENTRY;

  if (self->no_results_timeout_id != 0)
    g_source_remove (self->no_results_timeout_id);

  self->no_results_timeout_id = g_timeout_add (NO_RESULT_TIMEOUT,
                                               (GSourceFunc) show_no_results_page,
                                               self);

  GCAL_EXIT;
}

/**
 * gcal_search_popover_do_search:
 * @view: a #GcalSearchPopover instance.
 *
 * Called after 500ms since last typed character
 * in the search field.
 *
 * Returns: @G_SOURCE_REMOVE
 */
static gboolean
gcal_search_popover_do_search (gpointer user_data)
{
  GcalSearchPopover *self;
  gchar *search_query;

  GCAL_ENTRY;

  self = GCAL_SEARCH_POPOVER (user_data);

  search_query = g_strdup_printf ("(contains? \"%s\" \"%s\")",
                                  self->field ? self->field : "summary",
                                  self->query ? self->query : "");

  if (!self->subscribed)
    {
      g_autoptr (GDateTime) now, start, end;

      now = g_date_time_new_now_local ();
      start = g_date_time_add_years (now, -5);
      end = g_date_time_add_years (now, 5);

      gcal_manager_set_search_subscriber (self->manager,
                                          E_CAL_DATA_MODEL_SUBSCRIBER (self),
                                          g_date_time_to_unix (start),
                                          g_date_time_to_unix (end));

      /* Mark the view as subscribed */
      self->subscribed = TRUE;
    }

  /* update internal current time_t */
  self->current_utc_date = time (NULL);

  gcal_manager_set_query (self->manager, search_query);

  self->search_timeout_id = 0;
  g_free (search_query);

  GCAL_RETURN (G_SOURCE_REMOVE);
}


/*
 * ECalDataModelSubscriber iface
 */

static void
gcal_search_popover_component_added (ECalDataModelSubscriber *subscriber,
                                     ECalClient              *client,
                                     ECalComponent           *comp)
{
  GcalSearchPopover *self;
  RowEventData *row_data;
  GcalEvent *event;
  GError *error;
  gchar *uid;

  self = GCAL_SEARCH_POPOVER (subscriber);
  error = NULL;

  /* event */
  event = gcal_event_new (e_client_get_source (E_CLIENT (client)), comp, &error);

  if (error)
    {
      g_warning ("Error creating event: %s", error->message);
      g_clear_error (&error);
      return;
    }

  /* insert row_data at the hash of events */
  row_data = g_new0 (RowEventData, 1);
  row_data->event = event;
  row_data->row = make_row_for_event (GCAL_SEARCH_POPOVER (subscriber), event);
  g_signal_connect (row_data->row, "destroy", G_CALLBACK (gtk_widget_destroyed), &(row_data->row));

  /* Add to the hash table */
  uid = g_strdup (gcal_event_get_uid (event));

  g_object_set_data (G_OBJECT (row_data->row), "event-data", row_data);
  g_hash_table_insert (self->uuid_to_event, uid, row_data);

  gtk_container_add (GTK_CONTAINER (self->listbox), row_data->row);

  /* show 'no results' */
  self->num_results++;

  update_view (GCAL_SEARCH_POPOVER (subscriber));
}

static void
gcal_search_popover_component_modified (ECalDataModelSubscriber *subscriber,
                                        ECalClient              *client,
                                        ECalComponent           *comp)
{
  ;
}

static void
gcal_search_popover_component_removed (ECalDataModelSubscriber *subscriber,
                                       ECalClient              *client,
                                       const gchar             *uid,
                                       const gchar             *rid)
{
  GcalSearchPopover *self;
  RowEventData *row_data;
  ESource *source;
  gchar *uuid;

  self = GCAL_SEARCH_POPOVER (subscriber);

  /* if the widget has recurrency, it's UUID is different */
  source = e_client_get_source (E_CLIENT (client));

  if (rid != NULL)
    uuid = g_strdup_printf ("%s:%s:%s", e_source_get_uid (source), uid, rid);
  else
    uuid = g_strdup_printf ("%s:%s", e_source_get_uid (source), uid);

  /* Lookup the RowEventData */
  row_data = g_hash_table_lookup (self->uuid_to_event, uuid);

  /* We didn't add this event, so there's nothing to do */
  if (!row_data)
    goto out;

  /*
   * Removing the given RowEventData entry will call free_row_data(),
   * which removes the row from the listbox and also frees the hashtable's
   * uuid.
   */
  g_hash_table_remove (self->uuid_to_event, uuid);

  /* show 'no results' */
  self->num_results--;

  update_view (GCAL_SEARCH_POPOVER (subscriber));

out:
  g_free (uuid);
}

static void
gcal_search_popover_freeze (ECalDataModelSubscriber *subscriber)
{
  ;
}

static void
gcal_search_popover_thaw (ECalDataModelSubscriber *subscriber)
{
}

static void
gcal_data_model_subscriber_interface_init (ECalDataModelSubscriberInterface *iface)
{
  iface->component_added = gcal_search_popover_component_added;
  iface->component_modified = gcal_search_popover_component_modified;
  iface->component_removed = gcal_search_popover_component_removed;
  iface->freeze = gcal_search_popover_freeze;
  iface->thaw = gcal_search_popover_thaw;
}


/*
 * GObject overrides
 */

static void
gcal_search_popover_constructed (GObject *object)
{
  GcalSearchPopover *self;
  GtkWidget *placeholder_label;

  self = GCAL_SEARCH_POPOVER (object);

  /* make the listbox sorted */
  gtk_list_box_set_sort_func (GTK_LIST_BOX (self->listbox), (GtkListBoxSortFunc) sort_by_event, object, NULL);
  gtk_list_box_set_header_func (GTK_LIST_BOX (self->listbox), display_header_func, NULL, NULL);

  /* gchar* -> RowEventData* */
  self->uuid_to_event = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) free_row_data);

  /* don't fill the list with all events on startup */
  gcal_search_popover_search (GCAL_SEARCH_POPOVER (object), NULL, NULL);

  /* "type to search" label */
  placeholder_label = g_object_new (GTK_TYPE_LABEL,
                                    "label", _("Use the entry above to search for events."),
                                    "expand", TRUE,
                                    "valign", GTK_ALIGN_CENTER,
                                    "halign", GTK_ALIGN_CENTER,
                                    "visible", TRUE,
                                    NULL);
  gtk_style_context_add_class (gtk_widget_get_style_context (placeholder_label), "dim-label");

  gtk_list_box_set_placeholder (GTK_LIST_BOX (self->listbox), placeholder_label);
}

static void
gcal_search_popover_set_property (GObject      *object,
                                  guint         property_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GcalSearchPopover *self = GCAL_SEARCH_POPOVER (object);

  switch (property_id)
    {
    case PROP_CONTEXT:
      self->context = g_value_dup_object (value);
      break;

    case PROP_DATE:
      g_clear_pointer (&self->date, g_free);
      self->date = g_value_dup_boxed (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gcal_search_popover_get_property (GObject    *object,
                                  guint       property_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GcalSearchPopover *self = GCAL_SEARCH_POPOVER (object);

  switch (property_id)
    {
    case PROP_CONTEXT:
      g_value_set_object (value, self->context);
      break;

    case PROP_DATE:
      g_value_set_boxed (value, self->date);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gcal_search_popover_finalize (GObject *object)
{
  GcalSearchPopover *self = GCAL_SEARCH_POPOVER (object);

  g_clear_pointer (&self->date, g_free);
  g_clear_pointer (&self->uuid_to_event, g_hash_table_unref);

  /* Chain up to parent's finalize() method. */
  G_OBJECT_CLASS (gcal_search_popover_parent_class)->finalize (object);
}


static void
gcal_search_popover_class_init (GcalSearchPopoverClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->constructed = gcal_search_popover_constructed;
  object_class->set_property = gcal_search_popover_set_property;
  object_class->get_property = gcal_search_popover_get_property;
  object_class->finalize = gcal_search_popover_finalize;

  /* signals */
  /**
   * GcalSearchPopover::event-activated:
   * @view: the #GcalSearchPopover that generated the signal
   * @date: the date of the selected event
   *
   * Emitted when an event is selected from the list.
   *
   */
  signals[EVENT_ACTIVATED] = g_signal_new ("event-activated", GCAL_TYPE_SEARCH_POPOVER, G_SIGNAL_RUN_LAST,
                                           0,
                                           NULL, NULL, NULL,
                                           G_TYPE_NONE, 1, ICAL_TIME_TYPE);

  /* properties */
  /**
   * GcalSearchPopover::context:
   *
   * The context
   *
   */
  g_object_class_install_property (object_class,
                                   PROP_CONTEXT,
                                   g_param_spec_object ("context",
                                                        "Context",
                                                        "Context",
                                                        GCAL_TYPE_CONTEXT,
                                                        G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));


  /**
   * GcalSearchPopover::active-date:
   *
   * The date from this view, as defined by #GcalWindow.
   * Actually it is not used.
   *
   */
  g_object_class_install_property (object_class,
                                   PROP_DATE,
                                   g_param_spec_boxed ("active-date",
                                                       "The active date",
                                                       "The active/selected date in the view",
                                                       ICAL_TIME_TYPE,
                                                       G_PARAM_READWRITE));

  /* bind things for/from the template class */
  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/org/gnome/calendar/search-popover.ui");

  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GcalSearchPopover, stack);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GcalSearchPopover, listbox);

  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), open_event);
}

static void
gcal_search_popover_init (GcalSearchPopover *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

/* Public API */
/**
 * gcal_search_popover_new:
 *
 * Since: 0.1
 * Create a new month view widget
 *
 * Returns: (transfer full):
 **/
GtkWidget*
gcal_search_popover_new (void)
{
  return g_object_new (GCAL_TYPE_SEARCH_POPOVER, NULL);
}

/**
 * gcal_search_popover_connect:
 * @search_popover: a #GcalSearchPopover instance
 * @manager: App singleton #GcalManager instance
 *
 * Connect the view to the App singleton manager instance.
 * This is designed to be called once, after create of the view and before any use.
 */
void
gcal_search_popover_connect (GcalSearchPopover *search_popover,
                             GcalManager       *manager)
{
  if (manager != NULL && manager != search_popover->manager)
    search_popover->manager = manager;
}

/**
 * gcal_search_popover_set_search:
 * @view: a #GcalSearchPopover instance
 * @field: the field to perform the search.
 * @query: what the search will look for.
 *
 *
 *
 * Returns:
 */
void
gcal_search_popover_search (GcalSearchPopover *self,
                            const gchar       *field,
                            const gchar       *query)
{
  GCAL_ENTRY;

  g_clear_pointer (&self->query, g_free);
  g_clear_pointer (&self->field, g_free);

  if (self->search_timeout_id != 0)
    g_source_remove (self->search_timeout_id);

  /* Only perform search on valid non-empty strings */
  if (query && g_utf8_strlen (query, -1) >= 3)
    {
      self->query = g_strdup (query);
      self->field = g_strdup (field);
      self->search_timeout_id = g_timeout_add (500, gcal_search_popover_do_search, self);
    }
  else
    {
      g_hash_table_remove_all (self->uuid_to_event);
      self->num_results = 0;
    }

  update_view (self);

  GCAL_EXIT;
}

