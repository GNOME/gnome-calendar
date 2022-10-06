/* gcal-import-file-row.c
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

#define G_LOG_DOMAIN "GcalImportFileRow"

#include "config.h"
#include "gcal-import-file-row.h"
#include "gcal-importer.h"
#include "gcal-utils.h"

#include <glib/gi18n.h>

struct _GcalImportFileRow
{
  AdwBin              parent;

  GtkListBox         *events_listbox;
  GtkSizeGroup       *title_sizegroup;

  GCancellable       *cancellable;
  GFile              *file;
  GPtrArray          *ical_components;
  GPtrArray          *ical_timezones;
};

static void          read_calendar_finished_cb                   (GObject            *source_object,
                                                                  GAsyncResult       *res,
                                                                  gpointer            user_data);

G_DEFINE_TYPE (GcalImportFileRow, gcal_import_file_row, ADW_TYPE_BIN)

enum
{
  PROP_0,
  PROP_FILE,
  N_PROPS,
};

enum
{
  FILE_LOADED,
  N_SIGNALS,
};

static guint signals[N_SIGNALS] = { 0, };
static GParamSpec *properties[N_PROPS] = { NULL, };


/*
 * Auxiliary methods
 */

static void
add_grid_row (GcalImportFileRow *self,
              GtkGrid           *grid,
              gint               row,
              const gchar       *title,
              const gchar       *value)
{
  GtkWidget *title_label;
  GtkWidget *value_label;

  if (!value || g_utf8_strlen (value, -1) == 0)
    return;

  title_label = g_object_new (GTK_TYPE_LABEL,
                              "visible", TRUE,
                              "label", title,
                              "xalign", 1.0f,
                              "yalign", 0.0f,
                              "ellipsize", PANGO_ELLIPSIZE_END,
                              "max-width-chars", 40,
                              NULL);
  gtk_widget_add_css_class (title_label, "dim-label");
  gtk_grid_attach (grid, title_label, 0, row, 1, 1);

  gtk_size_group_add_widget (self->title_sizegroup, title_label);

  value_label = g_object_new (GTK_TYPE_LABEL,
                              "visible", TRUE,
                              "label", value,
                              "xalign", 0.0f,
                              "selectable", TRUE,
                              "ellipsize", PANGO_ELLIPSIZE_END,
                              "max-width-chars", 40,
                              NULL);
  gtk_grid_attach (grid, value_label, 1, row, 1, 1);
}

static void
fill_grid_with_event_data (GcalImportFileRow *self,
                           GtkGrid           *grid,
                           ICalComponent     *ical_component)
{
  g_autofree gchar *start_string = NULL;
  g_autofree gchar *description = NULL;
  g_autofree gchar *end_string = NULL;
  g_autoptr (GDateTime) start = NULL;
  g_autoptr (GDateTime) end = NULL;
  ICalTime *ical_start;
  ICalTime *ical_end;
  gint row = 0;

  ical_start = i_cal_component_get_dtstart (ical_component);
  start = gcal_date_time_from_icaltime (ical_start);
  if (i_cal_time_is_date (ical_start))
    start_string = g_date_time_format (start, "%x");
  else
    start_string = g_date_time_format (start, "%x %X");

  ical_end = i_cal_component_get_dtend (ical_component);
  if (ical_end)
    {
      end = gcal_date_time_from_icaltime (ical_end);
      if (i_cal_time_is_date (ical_end))
        end_string = g_date_time_format (end, "%x");
      else
        end_string = g_date_time_format (end, "%x %X");
    }
  else
    {
      end = g_date_time_add_days (start, 1);
      if (i_cal_time_is_date (ical_start))
        end_string = g_date_time_format (end, "%x");
      else
        end_string = g_date_time_format (end, "%x %X");
    }

  gcal_utils_extract_google_section (i_cal_component_get_description (ical_component),
                                     &description,
                                     NULL);

  add_grid_row (self, grid, row++, _("Title"), i_cal_component_get_summary (ical_component));
  add_grid_row (self, grid, row++, _("Location"), i_cal_component_get_location (ical_component));
  add_grid_row (self, grid, row++, _("Starts"), start_string);
  add_grid_row (self, grid, row++, _("Ends"), end_string);
  add_grid_row (self, grid, row++, _("Description"), description);

  g_clear_object (&ical_start);
  g_clear_object (&ical_end);
}

static void
add_events_to_listbox (GcalImportFileRow *self,
                       GPtrArray         *events)
{
  guint i;

  for (i = 0; i < events->len; i++)
    {
      ICalComponent *ical_component;
      GtkWidget *grid;
      GtkWidget *row;

      ical_component = g_ptr_array_index (events, i);

      row = g_object_new (GTK_TYPE_LIST_BOX_ROW,
                          "visible", TRUE,
                          "activatable", FALSE,
                          NULL);

      grid = g_object_new (GTK_TYPE_GRID,
                           "visible", TRUE,
                           "row-spacing", 6,
                           "column-spacing", 12,
                           "margin-top", 18,
                           "margin-bottom", 18,
                           "margin-start", 24,
                           "margin-end", 24,
                           NULL);
      fill_grid_with_event_data (self, GTK_GRID (grid), ical_component);
      gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), grid);

      gtk_list_box_insert (self->events_listbox, row, -1);
    }
}

static GPtrArray*
filter_event_components (ICalComponent *component)
{
  g_autoptr (GPtrArray) event_components = NULL;
  ICalComponent *aux;

  if (!component)
    return NULL;

  event_components = g_ptr_array_new_full (20, g_object_unref);
  aux = i_cal_component_get_first_real_component (component);
  while (aux)
    {
      g_ptr_array_add (event_components, g_object_ref (aux));
      aux = i_cal_component_get_next_component (component, I_CAL_VEVENT_COMPONENT);
    }

  return g_steal_pointer (&event_components);
}

static GPtrArray*
filter_timezones (ICalComponent *component)
{
  g_autoptr (GPtrArray) timezones = NULL;
  ICalComponent *aux;

  if (!component || i_cal_component_isa (component) != I_CAL_VCALENDAR_COMPONENT)
    return NULL;

  timezones = g_ptr_array_new_full (2, g_object_unref);
  for (aux = i_cal_component_get_first_component (component, I_CAL_VTIMEZONE_COMPONENT);
       aux;
       aux = i_cal_component_get_next_component (component, I_CAL_VTIMEZONE_COMPONENT))
    {
      ICalTimezone *zone = i_cal_timezone_new ();
      ICalComponent *clone = i_cal_component_clone (aux);

      if (i_cal_timezone_set_component (zone, clone))
         g_ptr_array_add (timezones, g_steal_pointer (&zone));

      g_clear_object (&clone);
      g_clear_object (&zone);
      g_clear_object (&aux);
    }

  if (!timezones->len)
    return NULL;

  return g_steal_pointer (&timezones);
}

static void
setup_file (GcalImportFileRow *self)
{
  gcal_importer_import_file (self->file,
                             self->cancellable,
                             read_calendar_finished_cb,
                             self);
}


/*
 * Callbacks
 */

static void
read_calendar_finished_cb (GObject      *source_object,
                           GAsyncResult *res,
                           gpointer      user_data)
{
  g_autoptr (GPtrArray) event_components = NULL;
  g_autoptr (GPtrArray) timezones = NULL;
  g_autoptr (GError) error = NULL;
  ICalComponent *component;
  GcalImportFileRow *self;

  self = GCAL_IMPORT_FILE_ROW (user_data);
  component = gcal_importer_import_file_finish (res, &error);
  event_components = filter_event_components (component);
  timezones = filter_timezones (component);

  gtk_widget_set_sensitive (GTK_WIDGET (self), !error && event_components && event_components->len > 0);

  if (error || !event_components || event_components->len == 0)
    return;

  add_events_to_listbox (self, event_components);

  self->ical_components = g_ptr_array_ref (event_components);
  self->ical_timezones = g_ptr_array_ref (timezones);

  g_signal_emit (self, signals[FILE_LOADED], 0, event_components);
}


/*
 * GObject overrides
 */

static void
gcal_import_file_row_finalize (GObject *object)
{
  GcalImportFileRow *self = (GcalImportFileRow *)object;

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);
  g_clear_object (&self->file);
  g_clear_pointer (&self->ical_components, g_ptr_array_unref);
  g_clear_pointer (&self->ical_timezones, g_ptr_array_unref);

  G_OBJECT_CLASS (gcal_import_file_row_parent_class)->finalize (object);
}

static void
gcal_import_file_row_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GcalImportFileRow *self = GCAL_IMPORT_FILE_ROW (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, self->file);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_import_file_row_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GcalImportFileRow *self = GCAL_IMPORT_FILE_ROW (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_assert (self->file == NULL);
      self->file = g_value_dup_object (value);
      setup_file (self);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_import_file_row_class_init (GcalImportFileRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gcal_import_file_row_finalize;
  object_class->get_property = gcal_import_file_row_get_property;
  object_class->set_property = gcal_import_file_row_set_property;

  signals[FILE_LOADED] = g_signal_new ("file-loaded",
                                       GCAL_TYPE_IMPORT_FILE_ROW,
                                       G_SIGNAL_RUN_LAST,
                                       0, NULL, NULL,
                                       g_cclosure_marshal_VOID__BOXED,
                                       G_TYPE_NONE,
                                       1,
                                       G_TYPE_PTR_ARRAY);

  properties[PROP_FILE] = g_param_spec_object ("file",
                                               "An ICS file",
                                               "An ICS file",
                                               G_TYPE_FILE,
                                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/gui/importer/gcal-import-file-row.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalImportFileRow, events_listbox);
}

static void
gcal_import_file_row_init (GcalImportFileRow *self)
{
  self->cancellable = g_cancellable_new ();

  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget*
gcal_import_file_row_new (GFile        *file,
                          GtkSizeGroup *title_sizegroup)
{
  GcalImportFileRow *self;

  self = g_object_new (GCAL_TYPE_IMPORT_FILE_ROW,
                       "file", file,
                       NULL);
  self->title_sizegroup = title_sizegroup;

  return (GtkWidget*) self;
}

GPtrArray*
gcal_import_file_row_get_ical_components (GcalImportFileRow *self)
{
  g_return_val_if_fail (GCAL_IS_IMPORT_FILE_ROW (self), NULL);

  return self->ical_components;
}

GPtrArray*
gcal_import_file_row_get_timezones (GcalImportFileRow *self)
{
  g_return_val_if_fail (GCAL_IS_IMPORT_FILE_ROW (self), NULL);

  return self->ical_timezones;
}
