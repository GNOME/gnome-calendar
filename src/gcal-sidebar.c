/* gcal-sidebar.c
 *
 * Copyright (C) 2017 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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
 */

#define G_LOG_DOMAIN "GcalSidebar"

#include "e-cal-data-model-subscriber.h"
#include "gcal-date-chooser.h"
#include "gcal-event.h"
#include "gcal-manager.h"
#include "gcal-sidebar.h"
#include "gcal-utils.h"
#include "gcal-view.h"

#include <glib/gi18n.h>

struct _GcalSidebar
{
  GtkRevealer         parent;

  GcalDateChooser    *date_chooser;
  GtkWidget          *empty_placeholder_widget;
  GtkWidget          *listbox;
  GtkWidget          *search_placeholder_widget;
  GtkWidget          *sidebar_header;

  icaltimetype       *active_date;
  GcalManager        *manager;
};

static void           gcal_view_interface_init                   (GcalViewInterface *iface);

static void   e_cal_data_model_subscriber_interface_init         (ECalDataModelSubscriberInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GcalSidebar, gcal_sidebar, GTK_TYPE_REVEALER,
                         G_IMPLEMENT_INTERFACE (GCAL_TYPE_VIEW, gcal_view_interface_init)
                         G_IMPLEMENT_INTERFACE (E_TYPE_CAL_DATA_MODEL_SUBSCRIBER,
                                                e_cal_data_model_subscriber_interface_init))

enum
{
  PROP_0,
  PROP_ACTIVE_DATE,
  PROP_MANAGER,
  N_PROPS
};

/* Auxiliary methods */

static GtkWidget*
create_row_for_event (GcalEvent *event)
{
  cairo_surface_t *surface;
  GtkWidget *row, *grid, *icon, *label;
  GdkRGBA *color;

  /* Box */
  grid = g_object_new (GTK_TYPE_GRID,
                       "column-spacing", 6,
                       "margin", 12,
                       "margin-bottom", 6,
                       "margin-top", 6,
                       NULL);
  /* Circular icon */
  color = gcal_event_get_color (event);
  surface = get_circle_surface_from_color (color, 12);

  icon = gtk_image_new_from_surface (surface);

  gtk_grid_attach (GTK_GRID (grid), icon, 0, 0, 1, 1);

  /* Summary label */
  label = g_object_new (GTK_TYPE_LABEL,
                        "label", gcal_event_get_summary (event),
                        "hexpand", TRUE,
                        "xalign", 0.0,
                        "ellipsize", PANGO_ELLIPSIZE_END,
                        "single-line-mode", TRUE,
                        NULL);

  gtk_grid_attach (GTK_GRID (grid), label, 1, 0, 1, 1);

  /* Description label */
  if (g_utf8_strlen (gcal_event_get_description (event), -1) > 0)
    {
      label = g_object_new (GTK_TYPE_LABEL,
                        "label", gcal_event_get_description (event),
                        "hexpand", TRUE,
                        "xalign", 0.0,
                        "ellipsize", PANGO_ELLIPSIZE_END,
                        "single-line-mode", TRUE,
                        NULL);

      gtk_style_context_add_class (gtk_widget_get_style_context (label), "dim-label");
      gtk_style_context_add_class (gtk_widget_get_style_context (label), "tiny-label");

      gtk_grid_attach (GTK_GRID (grid), label, 1, 1, 1, 1);
    }

  /* Row */
  row = gtk_list_box_row_new ();
  gtk_container_add (GTK_CONTAINER (row), grid);

  g_object_set_data_full (G_OBJECT (row), "event", g_object_ref (event), g_object_unref);

  gtk_widget_show_all (row);

  g_clear_pointer (&surface, cairo_surface_destroy);
  g_clear_pointer (&color, gdk_rgba_free);

  return row;
}

static void
reload_events (GcalSidebar *self)
{
  g_autoptr (GDateTime) end_date, now;
  GDateTime *selected_date;
  guint64 start, end;

  selected_date = gcal_date_chooser_get_date (GCAL_DATE_CHOOSER (self->date_chooser));
  now = g_date_time_new_now_local ();

  /*
   * If the selected date is today, show the next 2 days. Otherwise
   * only show events of the selected date.
   */
  if (datetime_compare_date (now, selected_date) == 0)
    end_date = g_date_time_add_days (selected_date, 2);
  else
    end_date = g_date_time_add_days (selected_date, 1);

  start = g_date_time_to_unix (selected_date);
  end = g_date_time_to_unix (end_date);

  gcal_manager_set_subscriber (self->manager,
                               E_CAL_DATA_MODEL_SUBSCRIBER (self),
                               start,
                               end);
}

/* Stealed from GNOME To Do */
static void
get_date_offset (GDateTime *dt,
                 gint      *days_diff,
                 gint      *next_year_diff)
{
  g_autoptr (GDateTime) now, next_year;

  now = g_date_time_new_now_local ();
  next_year = g_date_time_new_utc (g_date_time_get_year (now) + 1,
                                   G_DATE_JANUARY,
                                   1,
                                   0, 0, 0);

  if (g_date_time_get_year (dt) == g_date_time_get_year (now))
    {
      if (days_diff)
        *days_diff = g_date_time_get_day_of_year (dt) - g_date_time_get_day_of_year (now);
    }
  else
    {
      if (next_year_diff)
        *next_year_diff = g_date_time_difference (dt, now) / G_TIME_SPAN_DAY;
    }

  if (next_year_diff)
    *next_year_diff = g_date_time_difference (next_year, now) / G_TIME_SPAN_DAY;
}

static gchar*
get_string_for_date (GDateTime *dt)
{
  gchar *str;
  gint days_diff;
  gint next_year_diff;

  /* This case should never happen */
  if (!dt)
    return g_strdup (_("No date set"));

  days_diff = next_year_diff = 0;

  get_date_offset (dt, &days_diff, &next_year_diff);

  if (days_diff < 0)
    {
      g_autofree gchar *date_format;

      date_format = g_date_time_format (dt, "%x");
      str = g_strdup_printf (g_dngettext (NULL, "Yesterday", date_format, -days_diff), -days_diff);
    }
  else if (days_diff == 0)
    {
      str = g_strdup (_("Today"));
    }
  else if (days_diff == 1)
    {
      str = g_strdup (_("Tomorrow"));
    }
  else if (days_diff > 1 && days_diff < 7)
    {
      str = g_date_time_format (dt, "%A"); // Weekday name
    }
  else
    {
      str = g_date_time_format (dt, "%x");
    }

  return str;
}

/* Listbox functions */

static void
header_func (GtkListBoxRow *row,
             GtkListBoxRow *before,
             gpointer       user_data)
{
  GcalSidebar *self;
  GDateTime *row_date, *selected_date;
  GcalEvent *row_event;
  GtkWidget *label;
  g_autofree gchar *text;

  text = NULL;
  self = GCAL_SIDEBAR (user_data);
  selected_date = gcal_date_chooser_get_date (self->date_chooser);
  row_event = g_object_get_data (G_OBJECT (row), "event");
  row_date = gcal_event_get_date_start (row_event);

  gtk_list_box_row_set_header (row, NULL);

  /* Normalize row date */
  if (datetime_compare_date (row_date, selected_date) < 0)
    row_date = selected_date;

  if (before)
    {
      GcalEvent *before_event;
      GDateTime *before_date;

      before_event = g_object_get_data (G_OBJECT (before), "event");
      before_date = gcal_event_get_date_start (before_event);

      /* Normalize previous row date */
      if (datetime_compare_date (before_date, selected_date) < 0)
        before_date = selected_date;

      /* Don't add headers if the start dates are equal */
      if (datetime_compare_date (before_date, row_date) == 0)
        return;
    }

  /* Add the label as the row header */
  text = get_string_for_date (row_date);
  label = g_object_new (GTK_TYPE_LABEL,
                        "label", text,
                        "xalign", 0.0,
                        "hexpand", TRUE,
                        "margin-start", 12,
                        "margin-top", before ? 12 : 6,
                        "margin-bottom", 6,
                        NULL);

  gtk_style_context_add_class (gtk_widget_get_style_context (label), "sidebar-header");
  gtk_list_box_row_set_header (row, label);
}

static gint
sort_func (GtkListBoxRow *a,
           GtkListBoxRow *b,
           gpointer       user_data)
{
  GcalEvent *event_a, *event_b;

  event_a = g_object_get_data (G_OBJECT (a), "event");
  event_b = g_object_get_data (G_OBJECT (b), "event");

  return gcal_event_compare (event_a, event_b);
}

/* Callbacks */

static void
update_header_visibility (GcalSidebar *self)
{
  if (!gtk_revealer_get_child_revealed (GTK_REVEALER (self)))
    gtk_widget_hide (self->sidebar_header);

  if (gtk_revealer_get_reveal_child (GTK_REVEALER (self)))
    gtk_widget_show (self->sidebar_header);

  /*
   * We can't bind 'reveal-child' with GcalSidebar because it'll make the
   * 'reveal-child' property change ~before~ it reaches here and makes the
   * sidebar header visible. When that happens, the header skips the reveal
   * animation, and looses the synchronization with the sidebar animation.
   */
  gtk_revealer_set_reveal_child (GTK_REVEALER (self->sidebar_header),
                                 gtk_revealer_get_reveal_child (GTK_REVEALER (self)));
}

/* ECalDataModelSubscriber */

static void
gcal_sidebar_component_added (ECalDataModelSubscriber *subscriber,
                              ECalClient              *client,
                              ECalComponent           *comp)
{
  GcalSidebar *self;
  GcalEvent *event;
  ESource *source;
  GError *error;

  error = NULL;
  self = GCAL_SIDEBAR (subscriber);
  source = e_client_get_source (E_CLIENT (client));
  event = gcal_event_new (source, comp, &error);

  if (error)
    {
      g_warning ("Error adding events: %s", error->message);
      return;
    }

  gtk_container_add (GTK_CONTAINER (self->listbox), create_row_for_event (event));
}

static void
gcal_sidebar_component_changed (ECalDataModelSubscriber *subscriber,
                                ECalClient              *client,
                                ECalComponent           *comp)
{
  ECalComponentId *id;

  id = e_cal_component_get_id (comp);

  e_cal_data_model_subscriber_component_removed (subscriber, client, id->uid, id->rid);
  e_cal_data_model_subscriber_component_added (subscriber, client, comp);

  g_clear_pointer (&id, e_cal_component_free_id);
}

static void
gcal_sidebar_component_removed (ECalDataModelSubscriber *subscriber,
                                ECalClient              *client,
                                const gchar             *uid,
                                const gchar             *rid)
{
  GcalSidebar *self;
  ESource *source;
  GList *children, *l;
  g_autofree gchar *uuid;

  self = GCAL_SIDEBAR (subscriber);
  source = e_client_get_source (E_CLIENT (client));

  if (rid)
    uuid = g_strdup_printf ("%s:%s:%s", e_source_get_uid (source), uid, rid);
  else
    uuid = g_strdup_printf ("%s:%s", e_source_get_uid (source), uid);

  /* Remove the rows with the given uuid */
  children = gtk_container_get_children (GTK_CONTAINER (self->listbox));

  for (l = children; l != NULL; l = l->next)
    {
      GcalEvent *event;

      event = g_object_get_data (l->data, "event");

      if (g_str_equal (uuid, gcal_event_get_uid (event)))
        gtk_widget_destroy (l->data);
    }

  g_list_free (children);
}

static void
gcal_sidebar_freeze (ECalDataModelSubscriber *subscriber)
{
}

static void
gcal_sidebar_thaw (ECalDataModelSubscriber *subscriber)
{
}

/* GcalView */

static icaltimetype*
gcal_sidebar_get_date (GcalView *view)
{
  GcalSidebar *self = GCAL_SIDEBAR (view);

  return self->active_date;
}

static void
gcal_sidebar_set_date (GcalView     *view,
                       icaltimetype *active_date)
{
  GcalSidebar *self;
  GDateTime *date;

  self = GCAL_SIDEBAR (view);
  date = icaltime_to_datetime (active_date);

  g_clear_pointer (&self->active_date, g_free);
  self->active_date = gcal_dup_icaltime (active_date);

  gcal_date_chooser_set_date (self->date_chooser, date);

  gcal_clear_datetime (&date);
}

static GList*
gcal_sidebar_get_children_by_uuid (GcalView    *self,
                                   const gchar *uuid)
{
  return NULL;
}

static void
gcal_sidebar_clear_marks (GcalView *view)
{
}

static void
gcal_view_interface_init (GcalViewInterface *iface)
{
  iface->get_date = gcal_sidebar_get_date;
  iface->set_date = gcal_sidebar_set_date;
  iface->get_children_by_uuid = gcal_sidebar_get_children_by_uuid;
  iface->clear_marks = gcal_sidebar_clear_marks;
}

static void
e_cal_data_model_subscriber_interface_init (ECalDataModelSubscriberInterface *iface)
{
  iface->component_added = gcal_sidebar_component_added;
  iface->component_modified = gcal_sidebar_component_changed;
  iface->component_removed = gcal_sidebar_component_removed;
  iface->freeze = gcal_sidebar_freeze;
  iface->thaw = gcal_sidebar_thaw;
}

static void
gcal_sidebar_finalize (GObject *object)
{
  GcalSidebar *self = (GcalSidebar *)object;

  G_OBJECT_CLASS (gcal_sidebar_parent_class)->finalize (object);
}

static void
gcal_sidebar_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  GcalSidebar *self = GCAL_SIDEBAR (object);

  switch (prop_id)
    {
    case PROP_ACTIVE_DATE:
      g_value_set_boxed (value, self->active_date);
      break;

    case PROP_MANAGER:
      g_value_set_object (value, self->manager);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_sidebar_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  GcalSidebar *self = GCAL_SIDEBAR (object);

  switch (prop_id)
    {
    case PROP_ACTIVE_DATE:
      gcal_sidebar_set_date (GCAL_VIEW (self), g_value_get_boxed (value));
      break;

    case PROP_MANAGER:
      self->manager = g_value_dup_object (value);
      reload_events (self);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_sidebar_class_init (GcalSidebarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gcal_sidebar_finalize;
  object_class->get_property = gcal_sidebar_get_property;
  object_class->set_property = gcal_sidebar_set_property;


  /**
   * GcalSidebar::manager:
   *
   * The manager of the sidebar.
   */
  g_object_class_install_property (object_class,
                                   PROP_MANAGER,
                                   g_param_spec_object ("manager",
                                                        "Manager",
                                                        "Manager of the application",
                                                        GCAL_TYPE_MANAGER,
                                                        G_PARAM_READWRITE));

  g_object_class_override_property (object_class, PROP_ACTIVE_DATE, "active-date");

  g_type_ensure (GCAL_TYPE_DATE_CHOOSER);
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/sidebar.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalSidebar, date_chooser);
  gtk_widget_class_bind_template_child (widget_class, GcalSidebar, empty_placeholder_widget);
  gtk_widget_class_bind_template_child (widget_class, GcalSidebar, listbox);
  gtk_widget_class_bind_template_child (widget_class, GcalSidebar, search_placeholder_widget);
  gtk_widget_class_bind_template_child (widget_class, GcalSidebar, sidebar_header);

  gtk_widget_class_bind_template_callback (widget_class, reload_events);
  gtk_widget_class_bind_template_callback (widget_class, update_header_visibility);
}

static void
gcal_sidebar_init (GcalSidebar *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_list_box_set_placeholder (GTK_LIST_BOX (self->listbox), self->empty_placeholder_widget);

  gtk_list_box_set_header_func (GTK_LIST_BOX (self->listbox),
                                header_func,
                                self,
                                NULL);

  gtk_list_box_set_sort_func (GTK_LIST_BOX (self->listbox),
                              sort_func,
                              NULL,
                              NULL);
}

GtkWidget*
gcal_sidebar_new (void)
{
  return g_object_new (GCAL_TYPE_SIDEBAR, NULL);
}

GtkWidget*
gcal_sidebar_get_header_widget (GcalSidebar *self)
{
  g_return_val_if_fail (GCAL_IS_SIDEBAR (self), NULL);

  return self->sidebar_header;
}
