/* gcal-calendars-page.c
 *
 * Copyright 2019 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#define G_LOG_DOMAIN "GcalCalendarsPage"

#include <glib/gi18n.h>

#include "gcal-context.h"
#include "gcal-calendar-management-page.h"
#include "gcal-calendars-page.h"
#include "gcal-debug.h"
#include "gcal-utils.h"

struct _GcalCalendarsPage
{
  GtkBox              parent;

  GtkListBoxRow      *add_calendar_row;
  GtkListBox         *listbox;
  GtkLabel           *notification_label;
  GtkRevealer        *notification_revealer;
  GtkSizeGroup       *sizegroup;

  GcalCalendar       *removed_calendar;
  gint                notification_timeout_id;

  GcalContext        *context;
};

static void          gcal_calendar_management_page_iface_init    (GcalCalendarManagementPageInterface *iface);

static void          on_calendar_color_changed_cb                (GcalCalendar       *calendar,
                                                                  GParamSpec         *pspec,
                                                                  GtkImage           *icon);

G_DEFINE_TYPE_WITH_CODE (GcalCalendarsPage, gcal_calendars_page, GTK_TYPE_BOX,
                         G_IMPLEMENT_INTERFACE (GCAL_TYPE_CALENDAR_MANAGEMENT_PAGE,
                                                gcal_calendar_management_page_iface_init))

enum
{
  PROP_0,
  PROP_CONTEXT,
  N_PROPS
};


/*
 * Auxiliary methods
 */

static GtkWidget*
make_calendar_row (GcalCalendarsPage *self,
                   GcalCalendar      *calendar)
{
  g_autofree gchar *parent_name = NULL;
  g_autoptr (GtkBuilder) builder = NULL;
  cairo_surface_t *surface;
  const GdkRGBA *color;
  GcalManager *manager;
  GtkWidget *bottom_label;
  GtkWidget *top_label;
  GtkWidget *icon;
  GtkWidget *row;
  GtkWidget *sw;

  manager = gcal_context_get_manager (self->context);
  get_source_parent_name_color (manager, gcal_calendar_get_source (calendar), &parent_name, NULL);

  builder = gtk_builder_new_from_resource ("/org/gnome/calendar/calendar-row.ui");

  /*
   * Since we're destroying the builder instance before adding
   * the row to the listbox, it should be referenced here so
   * it isn't destroyed with the GtkBuilder.
   */
  row = g_object_ref (GTK_WIDGET (gtk_builder_get_object (builder, "row")));

  /* source color icon */
  color = gcal_calendar_get_color (calendar);
  surface = get_circle_surface_from_color (color, 24);
  icon = GTK_WIDGET (gtk_builder_get_object (builder, "icon"));
  gtk_image_set_from_surface (GTK_IMAGE (icon), surface);

  /* source name label */
  top_label = GTK_WIDGET (gtk_builder_get_object (builder, "title"));
  gtk_label_set_label (GTK_LABEL (top_label), gcal_calendar_get_name (calendar));
  g_object_bind_property (calendar, "name", top_label, "label", G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
  g_signal_connect_swapped (calendar,
                            "notify::name",
                            G_CALLBACK (gtk_list_box_invalidate_sort),
                            self->listbox);

  g_signal_connect_object (calendar,
                           "notify::color",
                           G_CALLBACK (on_calendar_color_changed_cb),
                           icon,
                           0);

  /* visibility switch */
  sw = GTK_WIDGET (gtk_builder_get_object (builder, "switch"));
  g_object_bind_property (calendar, "visible", sw, "active", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

  /* parent source name label */
  bottom_label = GTK_WIDGET (gtk_builder_get_object (builder, "subtitle"));
  gtk_label_set_label (GTK_LABEL (bottom_label), parent_name);

  g_clear_pointer (&surface, cairo_surface_destroy);

  gtk_size_group_add_widget (self->sizegroup, row);

  return row;
}

static void
add_calendar (GcalCalendarsPage *self,
              GcalCalendar      *calendar)
{
  g_autoptr (GList) children = NULL;
  GtkWidget *row;
  ESource *source;
  GList *l;

  children = gtk_container_get_children (GTK_CONTAINER (self->listbox));

  for (l = children; l; l = l->next)
    {
      if (g_object_get_data (l->data, "calendar") == calendar)
        return;
    }

  source = gcal_calendar_get_source (calendar);

  row = make_calendar_row (self, calendar);
  g_object_set_data (G_OBJECT (row), "source", source);
  g_object_set_data (G_OBJECT (row), "calendar", calendar);
  gtk_container_add (GTK_CONTAINER (self->listbox), row);
}


static void
remove_calendar (GcalCalendarsPage *self,
                 GcalCalendar      *calendar)
{
  g_autoptr (GList) children = NULL;
  GList *aux;

  children = gtk_container_get_children (GTK_CONTAINER (self->listbox));

  for (aux = children; aux != NULL; aux = aux->next)
    {
      GcalCalendar *row_calendar = g_object_get_data (G_OBJECT (aux->data), "calendar");

      if (row_calendar && row_calendar == calendar)
        {
          gtk_widget_destroy (aux->data);
          break;
        }
    }
}

static void
delete_calendar (GcalCalendarsPage *self,
                 GcalCalendar      *calendar)
{
  g_autoptr (GError) error = NULL;
  ESource *removed_source;

  g_assert (calendar != NULL);

  removed_source = gcal_calendar_get_source (calendar);

  /* We don't really want to remove non-removable sources */
  if (!e_source_get_removable (removed_source))
    return;

  /* Enable the source again to remove it's name from disabled list */
  gcal_calendar_set_visible (self->removed_calendar, TRUE);
  e_source_remove_sync (removed_source, NULL, &error);

  if (error != NULL)
    {
      g_warning ("[source-dialog] Error removing source: %s", error->message);
      add_calendar (self, self->removed_calendar);
    }

  gtk_revealer_set_reveal_child (self->notification_revealer, FALSE);
  g_clear_handle_id (&self->notification_timeout_id, g_source_remove);
}


/*
 * Callbacks
 */

static gint
listbox_sort_func (GtkListBoxRow *row1,
                   GtkListBoxRow *row2,
                   gpointer       user_data)
{
  g_autofree gchar *parent_name1 = NULL;
  g_autofree gchar *parent_name2 = NULL;
  GcalCalendarsPage *self;
  GcalManager *manager;
  ESource *source1;
  ESource *source2;
  gint retval;

  self = GCAL_CALENDARS_PAGE (user_data);

  /* Keep "Add Calendar" row always at the bottom */
  if (row1 == self->add_calendar_row)
    return 1;
  else if (row2 == self->add_calendar_row)
    return -1;

  manager = gcal_context_get_manager (self->context);

  source1 = g_object_get_data (G_OBJECT (row1), "source");
  source2 = g_object_get_data (G_OBJECT (row2), "source");

  retval = g_ascii_strcasecmp (e_source_get_display_name (source1), e_source_get_display_name (source2));

  if (retval != 0)
    return retval;

  get_source_parent_name_color (manager, source1, &parent_name1, NULL);
  get_source_parent_name_color (manager, source2, &parent_name2, NULL);

  return g_strcmp0 (parent_name1, parent_name2);
}

static gboolean
remove_calendar_after_delay_cb (gpointer data)
{
  GcalCalendarsPage *self = GCAL_CALENDARS_PAGE (data);

  g_assert (self->removed_calendar != NULL);

  delete_calendar (self, self->removed_calendar);
  g_clear_object (&self->removed_calendar);

  gtk_revealer_set_reveal_child (self->notification_revealer, FALSE);
  self->notification_timeout_id = 0;

  return G_SOURCE_REMOVE;
}

static void
on_calendar_color_changed_cb (GcalCalendar *calendar,
                              GParamSpec   *pspec,
                              GtkImage     *icon)
{
  cairo_surface_t *surface;
  const GdkRGBA *color;

  color = gcal_calendar_get_color (calendar);
  surface = get_circle_surface_from_color (color, 24);
  gtk_image_set_from_surface (GTK_IMAGE (icon), surface);
  g_clear_pointer (&surface, cairo_surface_destroy);
}

static void
on_close_notification_button_clicked_cb (GtkWidget         *button,
                                         GcalCalendarsPage *self)
{
  g_assert (self->removed_calendar != NULL);

  delete_calendar (self, self->removed_calendar);
  g_clear_object (&self->removed_calendar);

  gtk_revealer_set_reveal_child (self->notification_revealer, FALSE);
  g_clear_handle_id (&self->notification_timeout_id, g_source_remove);
}

static void
on_listbox_row_activated_cb (GtkListBox        *listbox,
                             GtkListBoxRow     *row,
                             GcalCalendarsPage *self)
{
  GcalCalendarManagementPage *page = GCAL_CALENDAR_MANAGEMENT_PAGE (self);

  if (row == self->add_calendar_row)
    {
      gcal_calendar_management_page_switch_page (page, "new-calendar", NULL);
    }
  else
    {
      GcalCalendar *calendar = g_object_get_data (G_OBJECT (row), "calendar");

      gcal_calendar_management_page_switch_page (page, "edit-calendar", calendar);
    }
}

static void
on_manager_calendar_added_cb (GcalManager       *manager,
                              GcalCalendar      *calendar,
                              GcalCalendarsPage *self)
{
  add_calendar (self, calendar);
}

static void
on_manager_calendar_removed_cb (GcalManager       *manager,
                                GcalCalendar      *calendar,
                                GcalCalendarsPage *self)
{
  remove_calendar (self, calendar);
}

static void
on_undo_remove_button_clicked_cb (GtkButton         *button,
                                  GcalCalendarsPage *self)
{
  if (!self->removed_calendar)
    return;

  gcal_calendar_set_visible (self->removed_calendar, TRUE);

  add_calendar (self, self->removed_calendar);
  g_clear_object (&self->removed_calendar);

  gtk_revealer_set_reveal_child (self->notification_revealer, FALSE);
  g_clear_handle_id (&self->notification_timeout_id, g_source_remove);
}


/*
 * GcalCalendarManagementPage iface
 */

static const gchar*
gcal_calendars_page_get_name (GcalCalendarManagementPage *page)
{
  return "calendars";
}

static const gchar*
gcal_calendars_page_get_title (GcalCalendarManagementPage *page)
{
  return _("Manage Calendars");
}

static void
gcal_calendars_page_activate (GcalCalendarManagementPage *page,
                              gpointer                    page_data)
{
  g_autofree gchar *new_string = NULL;
  GcalCalendarsPage *self;
  GcalCalendar *calendar;

  GCAL_ENTRY;

  g_assert (!page_data || GCAL_IS_CALENDAR (page_data));

  if (!page_data)
    GCAL_RETURN ();

  self = GCAL_CALENDARS_PAGE (page);

  /* Remove the previously deleted calendar, if any */
  if (self->removed_calendar)
    {
      delete_calendar (self, self->removed_calendar);
      g_clear_object (&self->removed_calendar);
    }

  calendar = GCAL_CALENDAR (page_data);
  self->removed_calendar = g_object_ref (calendar);

  /* Remove the listbox entry (if any) */
  remove_calendar (self, calendar);

  /* Update notification label */
  new_string = g_markup_printf_escaped (_("Calendar <b>%s</b> removed"),
                                        gcal_calendar_get_name (self->removed_calendar));
  gtk_label_set_markup (self->notification_label, new_string);

  gtk_revealer_set_reveal_child (self->notification_revealer, TRUE);

  /* Remove old notifications */
  if (self->notification_timeout_id != 0)
    g_source_remove (self->notification_timeout_id);

  self->notification_timeout_id = g_timeout_add_seconds (7, remove_calendar_after_delay_cb, self);

  gcal_calendar_set_visible (self->removed_calendar, FALSE);


  GCAL_EXIT;
}

static void
gcal_calendar_management_page_iface_init (GcalCalendarManagementPageInterface *iface)
{
  iface->get_name = gcal_calendars_page_get_name;
  iface->get_title = gcal_calendars_page_get_title;
  iface->activate = gcal_calendars_page_activate;
}

/*
 * GObject overrides
 */

static void
gcal_calendars_page_finalize (GObject *object)
{
  GcalCalendarsPage *self = (GcalCalendarsPage *)object;

  g_clear_handle_id (&self->notification_timeout_id, g_source_remove);
  g_clear_object (&self->context);

  G_OBJECT_CLASS (gcal_calendars_page_parent_class)->finalize (object);
}

static void
gcal_calendars_page_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GcalCalendarsPage *self = GCAL_CALENDARS_PAGE (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_value_set_object (value, self->context);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_calendars_page_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GcalCalendarsPage *self = GCAL_CALENDARS_PAGE (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
        {
          GcalManager *manager;

          self->context = g_value_dup_object (value);
          g_assert (self->context != NULL);

          manager = gcal_context_get_manager (self->context);
          g_signal_connect_object (manager, "calendar-added", G_CALLBACK (on_manager_calendar_added_cb), self, 0);
          g_signal_connect_object (manager, "calendar-removed", G_CALLBACK (on_manager_calendar_removed_cb), self, 0);
        }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_calendars_page_class_init (GcalCalendarsPageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gcal_calendars_page_finalize;
  object_class->get_property = gcal_calendars_page_get_property;
  object_class->set_property = gcal_calendars_page_set_property;

  g_object_class_override_property (object_class, PROP_CONTEXT, "context");

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/calendars-page.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalCalendarsPage, add_calendar_row);
  gtk_widget_class_bind_template_child (widget_class, GcalCalendarsPage, listbox);
  gtk_widget_class_bind_template_child (widget_class, GcalCalendarsPage, notification_label);
  gtk_widget_class_bind_template_child (widget_class, GcalCalendarsPage, notification_revealer);
  gtk_widget_class_bind_template_child (widget_class, GcalCalendarsPage, sizegroup);

  gtk_widget_class_bind_template_callback (widget_class, on_close_notification_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_listbox_row_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_undo_remove_button_clicked_cb);
}

static void
gcal_calendars_page_init (GcalCalendarsPage *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_list_box_set_sort_func (self->listbox,
                              (GtkListBoxSortFunc) listbox_sort_func,
                              self,
                              NULL);
}
