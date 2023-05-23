/* gcal-calendar-button.c
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

#define G_LOG_DOMAIN "GcalCalendarButton"

#include "gcal-calendar.h"
#include "gcal-calendar-button.h"
#include "gcal-context.h"
#include "gcal-utils.h"

struct _GcalCalendarButton
{
  AdwBin              parent;

  GtkWidget          *calendar_listbox;

  GcalContext        *context;
};

G_DEFINE_TYPE (GcalCalendarButton, gcal_calendar_button, ADW_TYPE_BIN)

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
make_calendar_row (GcalCalendar *calendar)
{
  g_autoptr (GdkPaintable) paintable = NULL;
  GtkWidget *label, *icon, *checkbox, *box, *row;
  const GdkRGBA *color;

  row = g_object_new (GTK_TYPE_LIST_BOX_ROW,
                      "css-name", "modelbutton",
                      NULL);

  /* main box */
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_widget_set_margin_start (box, 6);
  gtk_widget_set_margin_end (box, 6);

  /* source color icon */
  color = gcal_calendar_get_color (calendar);
  paintable = get_circle_paintable_from_color (color, 16);
  icon = gtk_image_new_from_paintable (paintable);

  gtk_widget_add_css_class (icon, "calendar-color-image");

  /* source name label */
  label = gtk_label_new (gcal_calendar_get_name (calendar));
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
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

static void
add_calendar (GcalCalendarButton *self,
              GcalCalendar        *calendar)
{
  GtkWidget *row;

  row = make_calendar_row (calendar);
  gtk_list_box_append (GTK_LIST_BOX (self->calendar_listbox), row);
}

static void
remove_calendar (GcalCalendarButton *self,
                 GcalCalendar        *calendar)
{
  GtkWidget *child;

  for (child = gtk_widget_get_first_child (self->calendar_listbox);
       child;
       child = gtk_widget_get_next_sibling (child))
    {
      GcalCalendar *row_calendar = g_object_get_data (G_OBJECT (child), "calendar");

      if (row_calendar && row_calendar == calendar)
        {
          gtk_list_box_remove (GTK_LIST_BOX (self->calendar_listbox), child);
          break;
        }
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
  GcalCalendar *calendar1, *calendar2;
  gint result;

  calendar1 = g_object_get_data (G_OBJECT (row1), "calendar");
  calendar2 = g_object_get_data (G_OBJECT (row2), "calendar");

  result = g_ascii_strcasecmp (gcal_calendar_get_name (calendar1), gcal_calendar_get_name (calendar2));

  if (result != 0)
    return result;

  return g_ascii_strcasecmp (gcal_calendar_get_id (calendar1), gcal_calendar_get_id (calendar2));
}

static void
on_manager_calendar_added_cb (GcalManager         *manager,
                              GcalCalendar        *calendar,
                              GcalCalendarButton *self)
{
  add_calendar (self, calendar);
}

static void
on_manager_calendar_changed_cb (GcalManager         *manager,
                                GcalCalendar        *calendar,
                                GcalCalendarButton *self)
{
  remove_calendar (self, calendar);
  add_calendar (self, calendar);
}

static void
on_manager_calendar_removed_cb (GcalManager         *manager,
                                GcalCalendar        *calendar,
                                GcalCalendarButton *self)
{
  remove_calendar (self, calendar);
}

static void
on_listbox_row_activated_cb (GtkListBox          *listbox,
                             GtkListBoxRow       *row,
                             GcalCalendarButton *self)
{
  GtkCheckButton *check = g_object_get_data (G_OBJECT (row), "check");

  gtk_check_button_set_active (check, !gtk_check_button_get_active (check));
}


/*
 * GObject overrides
 */

static void
gcal_calendar_button_finalize (GObject *object)
{
  GcalCalendarButton *self = (GcalCalendarButton *)object;

  g_clear_object (&self->context);

  G_OBJECT_CLASS (gcal_calendar_button_parent_class)->finalize (object);
}

static void
gcal_calendar_button_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  GcalCalendarButton *self = GCAL_CALENDAR_BUTTON (object);

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
gcal_calendar_button_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  GcalCalendarButton *self = GCAL_CALENDAR_BUTTON (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      {
        g_autoptr (GList) calendars = NULL;
        GcalManager *manager;
        GList *l;

        g_assert (self->context == NULL);
        self->context = g_value_dup_object (value);

        manager = gcal_context_get_manager (self->context);
        calendars = gcal_manager_get_calendars (manager);

        for (l = calendars; l; l = l->next)
          add_calendar (self, l->data);

        /*
        g_object_bind_property (manager,
                                "synchronizing",
                                self->synchronize_button,
                                "sensitive",
                                G_BINDING_INVERT_BOOLEAN | G_BINDING_SYNC_CREATE);
         */

        g_signal_connect_object (manager, "calendar-added", G_CALLBACK (on_manager_calendar_added_cb), object, 0);
        g_signal_connect_object (manager, "calendar-removed", G_CALLBACK (on_manager_calendar_removed_cb), object, 0);
        g_signal_connect_object (manager, "calendar-changed", G_CALLBACK (on_manager_calendar_changed_cb), object, 0);
      }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_calendar_button_class_init (GcalCalendarButtonClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gcal_calendar_button_finalize;
  object_class->get_property = gcal_calendar_button_get_property;
  object_class->set_property = gcal_calendar_button_set_property;

  /**
   * GcalCalendarButton::context:
   *
   * The #GcalContext of the application.
   */
  properties[PROP_CONTEXT] = g_param_spec_object ("context",
                                                  "Context of the application",
                                                  "The context of the application",
                                                  GCAL_TYPE_CONTEXT,
                                                  G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/gui/gcal-calendar-button.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalCalendarButton, calendar_listbox);

  gtk_widget_class_bind_template_callback (widget_class, on_listbox_row_activated_cb);
}

static void
gcal_calendar_button_init (GcalCalendarButton *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_list_box_set_sort_func (GTK_LIST_BOX (self->calendar_listbox),
                              (GtkListBoxSortFunc) listbox_sort_func,
                              self,
                              NULL);
}
