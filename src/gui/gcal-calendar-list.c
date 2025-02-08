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

#include "gcal-calendar-list.h"
#include "gcal-calendar.h"
#include "gcal-context.h"
#include "gcal-utils.h"

struct _GcalCalendarList
{
  AdwBin              parent;

  GtkWidget          *calendar_listbox;

  GcalContext        *context;
};

G_DEFINE_TYPE (GcalCalendarList, gcal_calendar_list, ADW_TYPE_BIN)

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
  GtkWidget *label, *icon, *checkbox, *box, *row;

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

  /* checkbox */
  checkbox = gtk_check_button_new ();
  g_object_bind_property (calendar,
                          "visible",
                          checkbox,
                          "active",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

  gtk_box_append (GTK_BOX (box), icon);
  gtk_box_append (GTK_BOX (box), label);
  gtk_box_append (GTK_BOX (box), checkbox);
  gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), box);

  g_object_set_data (G_OBJECT (row), "check", checkbox);
  g_object_set_data (G_OBJECT (row), "calendar", calendar);

  return row;
}

/*
 * Callbacks
 */

static void
on_listbox_row_activated_cb (GtkListBox *listbox,
                             GtkListBoxRow *row,
                             GcalCalendarList *self)
{
  GtkCheckButton *check = g_object_get_data (G_OBJECT (row), "check");

  gtk_check_button_set_active (check, !gtk_check_button_get_active (check));
}


/*
 * GObject overrides
 */

static void
gcal_calendar_list_finalize (GObject *object)
{
  GcalCalendarList *self = (GcalCalendarList *) object;

  g_clear_object (&self->context);

  G_OBJECT_CLASS (gcal_calendar_list_parent_class)->finalize (object);
}

static void
gcal_calendar_list_get_property (GObject *object,
                                 guint prop_id,
                                 GValue *value,
                                 GParamSpec *pspec)
{
  GcalCalendarList *self = GCAL_CALENDAR_LIST (object);

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
gcal_calendar_list_set_property (GObject *object,
                                 guint prop_id,
                                 const GValue *value,
                                 GParamSpec *pspec)
{
  GcalCalendarList *self = GCAL_CALENDAR_LIST (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      {
        GcalManager *manager;

        g_assert (self->context == NULL);
        self->context = g_value_dup_object (value);

        manager = gcal_context_get_manager (self->context);
        gtk_list_box_bind_model (GTK_LIST_BOX (self->calendar_listbox),
                                 gcal_manager_get_calendars_model (manager),
                                 create_row_func,
                                 NULL,
                                 NULL);
      }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_calendar_list_class_init (GcalCalendarListClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gcal_calendar_list_finalize;
  object_class->get_property = gcal_calendar_list_get_property;
  object_class->set_property = gcal_calendar_list_set_property;

  /**
   * GcalCalendarList::context:
   *
   * The #GcalContext of the application.
   */
  properties[PROP_CONTEXT] = g_param_spec_object ("context",
                                                  "Context of the application",
                                                  "The context of the application",
                                                  GCAL_TYPE_CONTEXT,
                                                  G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/gui/gcal-calendar-list.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalCalendarList, calendar_listbox);

  gtk_widget_class_bind_template_callback (widget_class, on_listbox_row_activated_cb);
}

static void
gcal_calendar_list_init (GcalCalendarList *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
