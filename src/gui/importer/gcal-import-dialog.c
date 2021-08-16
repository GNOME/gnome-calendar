/* gcal-import-dialog.c
 *
 * Copyright 2021 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#define G_LOG_DOMAIN "GcalImportDialog"

#include "gcal-import-dialog.h"

#include "config.h"
#include "gcal-debug.h"
#include "gcal-import-file-row.h"
#include "gcal-utils.h"

#include <glib/gi18n.h>

struct _GcalImportDialog
{
  HdyWindow           parent;

  GtkImage           *calendar_color_image;
  GtkLabel           *calendar_name_label;
  GtkListBox         *calendars_listbox;
  GtkPopover         *calendars_popover;
  GtkWidget          *cancel_button;
  GtkListBox         *files_listbox;
  HdyHeaderBar       *headerbar;
  GtkWidget          *import_button;
  GtkSizeGroup       *title_sizegroup;

  GtkWidget          *selected_row;

  GCancellable       *cancellable;
  GcalContext        *context;
  gint                n_events;
  gint                n_files;
};


static void          on_import_row_file_loaded_cb                 (GcalImportFileRow *row,
                                                                   GPtrArray         *events,
                                                                   GcalImportDialog  *self);

static void          on_manager_calendar_added_cb                (GcalManager        *manager,
                                                                  GcalCalendar       *calendar,
                                                                  GcalImportDialog   *self);

static void          on_manager_calendar_changed_cb              (GcalManager        *manager,
                                                                  GcalCalendar       *calendar,
                                                                  GcalImportDialog   *self);

static void          on_manager_calendar_removed_cb              (GcalManager        *manager,
                                                                  GcalCalendar       *calendar,
                                                                  GcalImportDialog   *self);

G_DEFINE_TYPE (GcalImportDialog, gcal_import_dialog, HDY_TYPE_WINDOW)

enum
{
  PROP_0,
  PROP_CONTEXT,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];


/*
 * Auxiliary methods
 */

static GtkWidget*
create_calendar_row (GcalManager  *manager,
                     GcalCalendar *calendar)
{
  g_autofree gchar *parent_name = NULL;
  cairo_surface_t *surface;
  const GdkRGBA *color;
  GtkWidget *icon;
  GtkWidget *row;

  color = gcal_calendar_get_color (calendar);
  surface = get_circle_surface_from_color (color, 16);
  get_source_parent_name_color (manager,
                                gcal_calendar_get_source (calendar),
                                &parent_name,
                                NULL);

  /* The icon with the source color */
  icon = gtk_image_new_from_surface (surface);
  gtk_style_context_add_class (gtk_widget_get_style_context (icon), "calendar-color-image");
  gtk_widget_show (icon);

  /* The row itself */
  row = g_object_new (HDY_TYPE_ACTION_ROW,
                      "title", gcal_calendar_get_name (calendar),
                      "subtitle", parent_name,
                      "sensitive", !gcal_calendar_is_read_only (calendar),
                      "activatable", TRUE,
                      "width-request", 300,
                      NULL);
  hdy_action_row_add_prefix (HDY_ACTION_ROW (row), icon);
  gtk_widget_show (row);

  g_object_set_data_full (G_OBJECT (row), "calendar", g_object_ref (calendar), g_object_unref);
  g_object_set_data (G_OBJECT (row), "color-icon", icon);

  g_clear_pointer (&surface, cairo_surface_destroy);

  return row;
}

static GtkWidget*
get_row_for_calendar (GcalImportDialog *self,
                      GcalCalendar     *calendar)
{
  g_autoptr (GList) children = NULL;
  GtkWidget *row;
  GList *l;

  row = NULL;
  children = gtk_container_get_children (GTK_CONTAINER (self->calendars_listbox));

  for (l = children; l != NULL; l = g_list_next (l))
    {
      GcalCalendar *row_calendar = g_object_get_data (l->data, "calendar");

      if (row_calendar == calendar)
        {
          row = l->data;
          break;
        }
    }

  return row;
}

static void
select_row (GcalImportDialog *self,
            GtkListBoxRow    *row)
{
  cairo_surface_t *surface;
  const GdkRGBA *color;
  GcalCalendar *calendar;

  self->selected_row = GTK_WIDGET (row);

  /* Setup the event page's source name and color */
  calendar = g_object_get_data (G_OBJECT (row), "calendar");

  gtk_label_set_label (self->calendar_name_label, gcal_calendar_get_name (calendar));

  color = gcal_calendar_get_color (calendar);
  surface = get_circle_surface_from_color (color, 16);
  gtk_image_set_from_surface (self->calendar_color_image, surface);

  g_clear_pointer (&surface, cairo_surface_destroy);
}

static void
update_default_calendar_row (GcalImportDialog *self)
{
  GcalCalendar *default_calendar;
  GcalManager *manager;
  GtkWidget *row;

  manager = gcal_context_get_manager (self->context);
  default_calendar = gcal_manager_get_default_calendar (manager);

  row = get_row_for_calendar (self, default_calendar);
  if (row != NULL)
    select_row (self, GTK_LIST_BOX_ROW (row));
}

static void
setup_calendars (GcalImportDialog *self)
{
  g_autoptr (GList) calendars = NULL;
  GcalManager *manager;
  GList *l;

  manager = gcal_context_get_manager (self->context);
  calendars = gcal_manager_get_calendars (manager);

  for (l = calendars; l; l = l->next)
    on_manager_calendar_added_cb (manager, l->data, self);

  update_default_calendar_row (self);

  g_signal_connect_object (manager, "calendar-added", G_CALLBACK (on_manager_calendar_added_cb), self, 0);
  g_signal_connect_object (manager, "calendar-changed", G_CALLBACK (on_manager_calendar_changed_cb), self, 0);
  g_signal_connect_object (manager, "calendar-removed", G_CALLBACK (on_manager_calendar_removed_cb), self, 0);
  g_signal_connect_object (manager, "notify::default-calendar", G_CALLBACK (update_default_calendar_row), self, G_CONNECT_SWAPPED);
}

static void
setup_files (GcalImportDialog  *self,
             GFile            **files,
             gint               n_files)
{
  gint i;

  GCAL_ENTRY;

  self->n_files = n_files;

  for (i = 0; i < n_files; i++)
    {
      GtkWidget *row;

      row = gcal_import_file_row_new (files[i], self->title_sizegroup);
      g_signal_connect (row, "file-loaded", G_CALLBACK (on_import_row_file_loaded_cb), self);

      if (n_files > 1)
        gcal_import_file_row_show_filename (GCAL_IMPORT_FILE_ROW (row));

      gtk_list_box_insert (self->files_listbox, row, -1);
    }

  GCAL_EXIT;
}


/*
 * Callbacks
 */

static void
on_events_created_cb (GObject      *source_object,
                      GAsyncResult *result,
                      gpointer      user_data)
{
  g_autoptr (GError) error = NULL;
  GcalImportDialog *self;

  GCAL_ENTRY;

  self = GCAL_IMPORT_DIALOG (user_data);

  e_cal_client_create_objects_finish (E_CAL_CLIENT (source_object), result, NULL, &error);
  if (error)
    g_warning ("Error creating events: %s", error->message);

  gtk_widget_destroy (GTK_WIDGET (self));

  GCAL_EXIT;
}

static void
on_calendars_listbox_row_activated_cb (GtkListBox       *listbox,
                                       GtkListBoxRow    *row,
                                       GcalImportDialog *self)
{
  GCAL_ENTRY;

  select_row (self, row);
  gtk_popover_popdown (self->calendars_popover);

  GCAL_EXIT;
}

static void
on_cancel_button_clicked_cb (GtkButton        *button,
                             GcalImportDialog *self)
{
  gtk_widget_destroy (GTK_WIDGET (self));
}

static void
on_import_button_clicked_cb (GtkButton        *button,
                             GcalImportDialog *self)
{
  g_autoptr (GList) children = NULL;
  GcalCalendar *calendar;
  ECalClient *client;
  GSList *slist;
  GList *l;

  GCAL_ENTRY;

  calendar = g_object_get_data (G_OBJECT (self->selected_row), "calendar");
  g_assert (self->selected_row != NULL);

  slist = NULL;
  children = gtk_container_get_children (GTK_CONTAINER (self->files_listbox));
  for (l = children; l; l = l->next)
    {
      GcalImportFileRow *row = l->data;
      GPtrArray *ical_components;
      guint i;

      ical_components = gcal_import_file_row_get_ical_components (row);
      if (!ical_components)
        continue;

      for (i = 0; i < ical_components->len; i++)
        slist = g_slist_prepend (slist, g_ptr_array_index (ical_components, i));
    }

  if (!slist)
    GCAL_RETURN ();

  self->cancellable = g_cancellable_new ();

  gtk_widget_set_sensitive (GTK_WIDGET (self), FALSE);

  client = gcal_calendar_get_client (calendar);
  e_cal_client_create_objects (client,
                               slist,
                               E_CAL_OPERATION_FLAG_NONE,
                               self->cancellable,
                               on_events_created_cb,
                               self);

  GCAL_EXIT;
}

static void
on_import_row_file_loaded_cb (GcalImportFileRow *row,
                              GPtrArray         *events,
                              GcalImportDialog  *self)
{
  g_autofree gchar *title = NULL;

  GCAL_ENTRY;

  self->n_events += events ? events->len : 0;

  title = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE,
                                        "Import %d event",
                                        "Import %d events",
                                        self->n_events),
                           self->n_events);
  hdy_header_bar_set_title (self->headerbar, title);

  gtk_widget_show (GTK_WIDGET (row));

  GCAL_EXIT;
}

static void
on_manager_calendar_added_cb (GcalManager      *manager,
                              GcalCalendar     *calendar,
                              GcalImportDialog *self)
{
  if (gcal_calendar_is_read_only (calendar))
    return;

  gtk_container_add (GTK_CONTAINER (self->calendars_listbox),
                     create_calendar_row (manager, calendar));
}

static void
on_manager_calendar_changed_cb (GcalManager      *manager,
                                GcalCalendar     *calendar,
                                GcalImportDialog *self)
{
  cairo_surface_t *surface;
  const GdkRGBA *color;
  GtkWidget *row, *color_icon;
  gboolean read_only;

  read_only = gcal_calendar_is_read_only (calendar);
  row = get_row_for_calendar (self, calendar);

  /* If the calendar changed from/to read-only, we add or remove it here */
  if (read_only)
    {
      if (row)
        gtk_container_remove (GTK_CONTAINER (self->calendars_listbox), row);
      return;
    }
  else if (!row)
    {
      on_manager_calendar_added_cb (manager, calendar, self);
      row = get_row_for_calendar (self, calendar);
    }

  hdy_preferences_row_set_title (HDY_PREFERENCES_ROW (row), gcal_calendar_get_name (calendar));
  gtk_widget_set_sensitive (row, !read_only);

  /* Setup the source color, in case it changed */
  color = gcal_calendar_get_color (calendar);
  surface = get_circle_surface_from_color (color, 16);
  color_icon = g_object_get_data (G_OBJECT (row), "color-icon");
  gtk_image_set_from_surface (GTK_IMAGE (color_icon), surface);

  gtk_list_box_invalidate_sort (GTK_LIST_BOX (self->calendars_listbox));

  g_clear_pointer (&surface, cairo_surface_destroy);
}

static void
on_manager_calendar_removed_cb (GcalManager      *manager,
                                GcalCalendar     *calendar,
                                GcalImportDialog *self)
{
  GtkWidget *row;

  row = get_row_for_calendar (self, calendar);

  if (!row)
    return;

  gtk_container_remove (GTK_CONTAINER (self->calendars_listbox), row);
}

static void
on_select_calendar_row_activated_cb (GtkListBox       *listbox,
                                     GtkListBoxRow    *row,
                                     GcalImportDialog *self)
{
  gtk_popover_popup (self->calendars_popover);
}

static gint
sort_func (GtkListBoxRow *row1,
           GtkListBoxRow *row2,
           gpointer       user_data)
{
  GcalCalendar *calendar1, *calendar2;
  g_autofree gchar *name1 = NULL;
  g_autofree gchar *name2 = NULL;

  calendar1 = g_object_get_data (G_OBJECT (row1), "calendar");
  calendar2 = g_object_get_data (G_OBJECT (row2), "calendar");

  name1 = g_utf8_casefold (gcal_calendar_get_name (calendar1), -1);
  name2 = g_utf8_casefold (gcal_calendar_get_name (calendar2), -1);

  return g_strcmp0 (name1, name2);
}


/*
 * GObject overrides
 */

static void
gcal_import_dialog_finalize (GObject *object)
{
  GcalImportDialog *self = (GcalImportDialog *)object;

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);
  g_clear_object (&self->context);

  G_OBJECT_CLASS (gcal_import_dialog_parent_class)->finalize (object);
}

static void
gcal_import_dialog_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GcalImportDialog *self = GCAL_IMPORT_DIALOG (object);

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
gcal_import_dialog_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GcalImportDialog *self = GCAL_IMPORT_DIALOG (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_assert (self->context == NULL);
      self->context = g_value_dup_object (value);
      setup_calendars (self);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_import_dialog_class_init (GcalImportDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gcal_import_dialog_finalize;
  object_class->get_property = gcal_import_dialog_get_property;
  object_class->set_property = gcal_import_dialog_set_property;

  /**
   * GcalEventPopover::context:
   *
   * The context of the import dialog.
   */
  properties[PROP_CONTEXT] = g_param_spec_object ("context",
                                                  "Context",
                                                  "Context",
                                                  GCAL_TYPE_CONTEXT,
                                                  G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/gui/importer/gcal-import-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalImportDialog, calendar_color_image);
  gtk_widget_class_bind_template_child (widget_class, GcalImportDialog, calendar_name_label);
  gtk_widget_class_bind_template_child (widget_class, GcalImportDialog, calendars_listbox);
  gtk_widget_class_bind_template_child (widget_class, GcalImportDialog, calendars_popover);
  gtk_widget_class_bind_template_child (widget_class, GcalImportDialog, cancel_button);
  gtk_widget_class_bind_template_child (widget_class, GcalImportDialog, files_listbox);
  gtk_widget_class_bind_template_child (widget_class, GcalImportDialog, headerbar);
  gtk_widget_class_bind_template_child (widget_class, GcalImportDialog, import_button);
  gtk_widget_class_bind_template_child (widget_class, GcalImportDialog, title_sizegroup);

  gtk_widget_class_bind_template_callback (widget_class, on_calendars_listbox_row_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_cancel_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_import_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_select_calendar_row_activated_cb);
}

static void
gcal_import_dialog_init (GcalImportDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_list_box_set_sort_func (GTK_LIST_BOX (self->calendars_listbox), sort_func, NULL, NULL);
}

GtkWidget*
gcal_import_dialog_new_for_files (GcalContext  *context,
                                  GFile       **files,
                                  gint          n_files)
{
  GcalImportDialog *self;

  self =  g_object_new (GCAL_TYPE_IMPORT_DIALOG,
                        "context", context,
                        NULL);

  setup_files (self, files, n_files);

  return GTK_WIDGET (self);
}
