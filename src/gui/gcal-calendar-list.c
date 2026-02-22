/* gcal-calendar-list.c
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

#define G_LOG_DOMAIN "GcalCalendarList"

#include "gcal-application.h"
#include "gcal-calendar-list.h"
#include "gcal-calendar.h"
#include "gcal-context.h"
#include "gcal-utils.h"

struct _GcalCalendarList
{
  AdwBin              parent;

  GtkWidget          *calendar_listbox;
};

G_DEFINE_TYPE (GcalCalendarList, gcal_calendar_list, ADW_TYPE_BIN)


/*
 * Auxiliary methods
 */

static gboolean
paintable_from_gdk_rgb (GBinding     *binding,
                        const GValue *from_value,
                        GValue       *to_value,
                        gpointer      user_data)
{
  GdkRGBA *rgba = g_value_get_boxed (from_value);
  g_value_take_object (to_value, get_circle_paintable_from_color (rgba, 16));
  return TRUE;
}

static GtkWidget*
create_row_func (gpointer data,
                 gpointer user_data)
{
  g_autoptr (GdkPaintable) paintable = NULL;
  GcalCalendar *calendar;
  GtkWidget *label, *icon, *box, *row, *image;

  calendar = GCAL_CALENDAR (data);

  row = g_object_new (GTK_TYPE_LIST_BOX_ROW, NULL);

  /* main box */
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_widget_set_margin_start (box, 6);
  gtk_widget_set_margin_end (box, 6);

  /* source color icon */
  icon = gtk_image_new ();
  gtk_widget_add_css_class (icon, "calendar-color-image");
  g_object_bind_property_full (calendar, "color",
                               icon, "paintable",
                               G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE,
                               paintable_from_gdk_rgb,
                               NULL,
                               NULL,
                               NULL);


  /* source name label */
  label = gtk_label_new (gcal_calendar_get_name (calendar));
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
  gtk_widget_set_tooltip_text (label, gcal_calendar_get_name (calendar));
  gtk_widget_set_hexpand (label, TRUE);

  image = gtk_image_new_from_icon_name ("checkmark-small-symbolic");
  g_object_bind_property (calendar,
                          "visible",
                          image,
                          "visible",
                          G_BINDING_SYNC_CREATE);

  gtk_box_append (GTK_BOX (box), icon);
  gtk_box_append (GTK_BOX (box), label);
  gtk_box_append (GTK_BOX (box), image);
  gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), box);

  g_object_set (G_OBJECT (row), "accessible-role", GTK_ACCESSIBLE_ROLE_CHECKBOX, NULL);
  gtk_accessible_update_state (GTK_ACCESSIBLE (row),
                               GTK_ACCESSIBLE_STATE_CHECKED, gcal_calendar_get_visible (calendar),
                               -1);

  g_object_set_data (G_OBJECT (row), "calendar", calendar);

  return row;
}

static GListModel*
create_sorted_calendars_model (GcalCalendarList *self)
{
  GcalContext *context = gcal_application_get_context (GCAL_DEFAULT_APPLICATION);
  GcalManager *manager = gcal_context_get_manager (context);
  g_autoptr (GtkSortListModel) sort_model = NULL;
  g_autoptr (GtkStringSorter) sorter = NULL;

  sorter = gtk_string_sorter_new (gtk_property_expression_new (E_TYPE_SOURCE,
                                                               gtk_property_expression_new (GCAL_TYPE_CALENDAR, NULL, "parent-source"),
                                                               "display-name"));
  sort_model = gtk_sort_list_model_new (g_object_ref (gcal_manager_get_calendars_model (manager)),
                                        GTK_SORTER (g_steal_pointer (&sorter)));

  return G_LIST_MODEL (g_steal_pointer (&sort_model));
}


/*
 * Callbacks
 */

static void
on_listbox_row_activated_cb (GtkListBox *listbox,
                             GtkListBoxRow *row,
                             GcalCalendarList *self)
{
  GcalCalendar *calendar = g_object_get_data (G_OBJECT (row), "calendar");

  gcal_calendar_set_visible (calendar, !gcal_calendar_get_visible (calendar));

  gtk_accessible_update_state (GTK_ACCESSIBLE (row),
                               GTK_ACCESSIBLE_STATE_CHECKED, gcal_calendar_get_visible (calendar),
                               -1);
}

static void
update_header_func (GtkListBoxRow *row,
                    GtkListBoxRow *before,
                    gpointer       user_data)
{
  GcalCalendar *calendar;
  GtkWidget *header = NULL;
  ESource *parent;

  calendar = g_object_get_data (G_OBJECT (row), "calendar");
  parent = gcal_calendar_get_parent_source (calendar);

  if (before)
    {
      GcalCalendar *before_calendar;
      ESource *before_parent;

      before_calendar = g_object_get_data (G_OBJECT (before), "calendar");
      before_parent = gcal_calendar_get_parent_source (before_calendar);

      if (g_strcmp0 (e_source_get_display_name (parent), e_source_get_display_name (before_parent)) != 0)
        {
          header = g_object_new (GTK_TYPE_LABEL,
                                 "label", e_source_get_display_name (parent),
                                 "css-classes", (const gchar * const []) { "heading", NULL },
                                 "ellipsize", PANGO_ELLIPSIZE_END,
                                 "xalign", 0.0,
                                 "margin-top", 18,
                                 "margin-bottom", 6,
                                 "margin-start", 18,
                                 "margin-end", 18,
                                 NULL);
        }
    }
  else
    {
      header = g_object_new (GTK_TYPE_LABEL,
                             "label", e_source_get_display_name (parent),
                             "css-classes", (const gchar * const []) { "heading", NULL },
                             "xalign", 0.0,
                             "margin-top", 18,
                             "margin-bottom", 6,
                             "margin-start", 18,
                             "margin-end", 18,
                             NULL);
    }

  gtk_list_box_row_set_header (row, header);
}


static void
gcal_calendar_list_class_init (GcalCalendarListClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/gui/gcal-calendar-list.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalCalendarList, calendar_listbox);

  gtk_widget_class_bind_template_callback (widget_class, on_listbox_row_activated_cb);
}

static void
gcal_calendar_list_init (GcalCalendarList *self)
{
  g_autoptr (GListModel) calendars_model = NULL;

  gtk_widget_init_template (GTK_WIDGET (self));

  calendars_model = create_sorted_calendars_model (self);

  gtk_list_box_bind_model (GTK_LIST_BOX (self->calendar_listbox),
                           calendars_model,
                           create_row_func,
                           NULL,
                           NULL);
  gtk_list_box_set_header_func (GTK_LIST_BOX (self->calendar_listbox),
                                update_header_func,
                                NULL, NULL);
}

