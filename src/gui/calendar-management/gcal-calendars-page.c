/* gcal-calendars-page.c
 *
 * Copyright 2019-2020 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#include "gcal-calendar.h"
#include "gcal-context.h"
#include "gcal-calendar-management-page.h"
#include "gcal-calendars-page.h"
#include "gcal-debug.h"
#include "gcal-utils.h"

struct _GcalCalendarsPage
{
  AdwNavigationPage   parent;

  GtkListBox         *listbox;
  AdwToastOverlay    *toast_overlay;

  AdwToast           *toast;

  GcalContext        *context;
};

static void          gcal_calendar_management_page_iface_init    (GcalCalendarManagementPageInterface *iface);

static void          on_calendar_color_changed_cb                (GcalCalendar       *calendar,
                                                                  GParamSpec         *pspec,
                                                                  GtkImage           *icon);

G_DEFINE_TYPE_WITH_CODE (GcalCalendarsPage, gcal_calendars_page, ADW_TYPE_NAVIGATION_PAGE,
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
  g_autoptr (GdkPaintable) color_paintable = NULL;
  g_autofree gchar *parent_name = NULL;
  g_autoptr (GtkBuilder) builder = NULL;
  GtkWidget *read_only_icon;
  const GdkRGBA *color;
  GtkWidget *icon;
  GtkWidget *row;
  GtkWidget *sw;

  parent_name = e_source_dup_display_name (gcal_calendar_get_parent_source (calendar));

  builder = gtk_builder_new_from_resource ("/org/gnome/calendar/ui/gui/calendar-management/calendar-row.ui");

  /*
   * Since we're destroying the builder instance before adding
   * the row to the listbox, it should be referenced here so
   * it isn't destroyed with the GtkBuilder.
   */
  row = g_object_ref (GTK_WIDGET (gtk_builder_get_object (builder, "row")));

  /* read-only icon */
  read_only_icon = GTK_WIDGET (gtk_builder_get_object (builder, "read_only_icon"));
  gtk_widget_set_visible (read_only_icon, gcal_calendar_is_read_only (calendar));

  /* source color icon */
  color = gcal_calendar_get_color (calendar);
  color_paintable = get_circle_paintable_from_color (color, 24);
  icon = GTK_WIDGET (gtk_builder_get_object (builder, "icon"));
  gtk_image_set_from_paintable (GTK_IMAGE (icon), color_paintable);

  /* source name label */
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), gcal_calendar_get_name (calendar));
  g_object_bind_property (calendar, "name", row, "title", G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

  g_signal_connect_object (calendar,
                           "notify::name",
                           G_CALLBACK (gtk_list_box_invalidate_sort),
                           self->listbox,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (calendar,
                           "notify::color",
                           G_CALLBACK (on_calendar_color_changed_cb),
                           icon,
                           0);

  /* visibility switch */
  sw = GTK_WIDGET (gtk_builder_get_object (builder, "switch"));
  g_object_bind_property (calendar, "visible", sw, "active", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

  /* parent source name label */
  adw_action_row_set_subtitle (ADW_ACTION_ROW (row), parent_name);

  return row;
}

static void
add_calendar (GcalCalendarsPage *self,
              GcalCalendar      *calendar)
{
  GtkWidget *child;
  GtkWidget *row;
  ESource *source;

  for (child = gtk_widget_get_first_child (GTK_WIDGET (self->listbox));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      if (g_object_get_data (G_OBJECT (child), "calendar") == calendar)
        return;
    }

  source = gcal_calendar_get_source (calendar);

  row = make_calendar_row (self, calendar);
  g_object_set_data (G_OBJECT (row), "source", source);
  g_object_set_data (G_OBJECT (row), "calendar", calendar);
  gtk_list_box_append (self->listbox, row);
}


static void
remove_calendar (GcalCalendarsPage *self,
                 GcalCalendar      *calendar)
{
  GtkWidget *child;

  for (child = gtk_widget_get_first_child (GTK_WIDGET (self->listbox));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      GcalCalendar *row_calendar = g_object_get_data (G_OBJECT (child), "calendar");

      if (row_calendar && row_calendar == calendar)
        {
          gtk_list_box_remove (self->listbox, child);
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
  gcal_calendar_set_visible (calendar, TRUE);
  e_source_remove_sync (removed_source, NULL, &error);

  if (error != NULL)
    {
      g_warning ("[source-dialog] Error removing source: %s", error->message);
      add_calendar (self, calendar);
    }
}


/*
 * Callbacks
 */

static gint
listbox_sort_func (GtkListBoxRow *row1,
                   GtkListBoxRow *row2,
                   gpointer       user_data)
{
  GcalCalendar *calendar1;
  GcalCalendar *calendar2;
  const gchar *parent_name1;
  const gchar *parent_name2;
  gint retval;

  calendar1 = g_object_get_data (G_OBJECT (row1), "calendar");
  calendar2 = g_object_get_data (G_OBJECT (row2), "calendar");

  retval = g_ascii_strcasecmp (gcal_calendar_get_name (calendar1), gcal_calendar_get_name (calendar2));

  if (retval != 0)
    return retval;


  parent_name1 = e_source_get_display_name (gcal_calendar_get_parent_source (calendar1));
  parent_name2 = e_source_get_display_name (gcal_calendar_get_parent_source (calendar2));

  return g_strcmp0 (parent_name1, parent_name2);
}

static void
on_calendar_color_changed_cb (GcalCalendar *calendar,
                              GParamSpec   *pspec,
                              GtkImage     *icon)
{
  g_autoptr (GdkPaintable) color_paintable = NULL;
  const GdkRGBA *color;

  color = gcal_calendar_get_color (calendar);
  color_paintable = get_circle_paintable_from_color (color, 24);
  gtk_image_set_from_paintable (GTK_IMAGE (icon), color_paintable);
}

static void
on_listbox_row_activated_cb (GtkListBox        *listbox,
                             GtkListBoxRow     *row,
                             GcalCalendarsPage *self)
{
  GcalCalendarManagementPage *page = GCAL_CALENDAR_MANAGEMENT_PAGE (self);

  GcalCalendar *calendar = g_object_get_data (G_OBJECT (row), "calendar");

  gcal_calendar_management_page_switch_page (page, "edit-calendar", calendar);
}

static void
on_new_calendar_row_activated_cb (AdwButtonRow      *button,
                                  GcalCalendarsPage *self)
{
  GcalCalendarManagementPage *page = GCAL_CALENDAR_MANAGEMENT_PAGE (self);

  gcal_calendar_management_page_switch_page (page, "new-calendar", NULL);
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
on_toast_button_clicked_cb (AdwToast          *toast,
                            GcalCalendarsPage *self)
{
  GcalCalendar *calendar;

  GCAL_ENTRY;

  calendar = g_object_get_data (G_OBJECT (toast), "calendar");
  g_assert (calendar != NULL);

  gcal_calendar_set_visible (calendar, TRUE);
  add_calendar (self, calendar);

  g_object_set_data (G_OBJECT (toast), "calendar", NULL);

  g_clear_object (&self->toast);

  GCAL_EXIT;
}

static void
on_toast_dismissed_cb (AdwToast          *toast,
                       GcalCalendarsPage *self)
{
  GcalCalendar *calendar;

  GCAL_ENTRY;

  calendar = g_object_get_data (G_OBJECT (toast), "calendar");

  if (!calendar)
    {
      g_assert (self->toast == NULL);
      GCAL_RETURN ();
    }

  delete_calendar (self, calendar);

  g_clear_object (&self->toast);

  GCAL_EXIT;
}


/*
 * GcalCalendarManagementPage iface
 */

static void
gcal_calendars_page_activate (GcalCalendarManagementPage *page,
                              GcalCalendar               *calendar)
{
  g_autofree gchar *new_string = NULL;
  g_autoptr (AdwToast) toast = NULL;
  GcalCalendarsPage *self;

  GCAL_ENTRY;

  if (!calendar)
    GCAL_RETURN ();

  self = GCAL_CALENDARS_PAGE (page);

  /* Remove the previously deleted calendar, if any */
  g_clear_pointer (&self->toast, adw_toast_dismiss);

  /* Remove the listbox entry (if any) */
  remove_calendar (self, calendar);

  /* Create the new toast */

  /* TRANSLATORS: %s is a calendar name. */
  new_string = g_markup_printf_escaped (_("Calendar “%s” removed"),
                                        gcal_calendar_get_name (calendar));

  toast = adw_toast_new (new_string);
  adw_toast_set_timeout (toast, 7);
  adw_toast_set_button_label (toast, _("_Undo"));
  g_object_set_data_full (G_OBJECT (toast), "calendar", g_object_ref (calendar), g_object_unref);
  g_signal_connect (toast, "dismissed", G_CALLBACK (on_toast_dismissed_cb), self);
  g_signal_connect (toast, "button-clicked", G_CALLBACK (on_toast_button_clicked_cb), self);
  adw_toast_overlay_add_toast (self->toast_overlay, g_object_ref (toast));

  self->toast = g_steal_pointer (&toast);

  gcal_calendar_set_visible (calendar, FALSE);

  GCAL_EXIT;
}

static void
gcal_calendar_management_page_iface_init (GcalCalendarManagementPageInterface *iface)
{
  iface->activate = gcal_calendars_page_activate;
}

/*
 * GObject overrides
 */

static void
gcal_calendars_page_finalize (GObject *object)
{
  GcalCalendarsPage *self = (GcalCalendarsPage *)object;

  g_clear_object (&self->toast);
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
          g_autoptr (GList) calendars = NULL;
          GcalManager *manager;
          GList *l;

          self->context = g_value_dup_object (value);
          g_assert (self->context != NULL);

          manager = gcal_context_get_manager (self->context);
          g_signal_connect_object (manager, "calendar-added", G_CALLBACK (on_manager_calendar_added_cb), self, 0);
          g_signal_connect_object (manager, "calendar-removed", G_CALLBACK (on_manager_calendar_removed_cb), self, 0);

          calendars = gcal_manager_get_calendars (manager);
          for (l = calendars; l; l = l->next)
              add_calendar (self, l->data);
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

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/gui/calendar-management/gcal-calendars-page.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalCalendarsPage, listbox);
  gtk_widget_class_bind_template_child (widget_class, GcalCalendarsPage, toast_overlay);

  gtk_widget_class_bind_template_callback (widget_class, on_listbox_row_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_new_calendar_row_activated_cb);
}

static void
gcal_calendars_page_init (GcalCalendarsPage *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_list_box_set_sort_func (self->listbox,
                              (GtkListBoxSortFunc) listbox_sort_func,
                              NULL,
                              NULL);
}
