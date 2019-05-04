/* gcal-calendar-popover.c
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

#define G_LOG_DOMAIN "GcalCalendarPopover"

#include "gcal-calendar.h"
#include "gcal-calendar-popover.h"
#include "gcal-context.h"
#include "gcal-utils.h"

struct _GcalCalendarPopover
{
  GtkPopover          parent;

  GtkWidget          *calendar_listbox;

  GcalContext        *context;
};

G_DEFINE_TYPE (GcalCalendarPopover, gcal_calendar_popover, GTK_TYPE_POPOVER)

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
  GtkWidget *label, *icon, *checkbox, *box, *row;
  GtkStyleContext *context;
  cairo_surface_t *surface;
  const GdkRGBA *color;

  row = gtk_list_box_row_new ();

  /* apply some nice styling */
  context = gtk_widget_get_style_context (row);
  gtk_style_context_add_class (context, "button");
  gtk_style_context_add_class (context, "flat");
  gtk_style_context_add_class (context, "menuitem");

  /* main box */
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_container_set_border_width (GTK_CONTAINER (box), 6);

  /* source color icon */
  color = gcal_calendar_get_color (calendar);
  surface = get_circle_surface_from_color (color, 16);
  icon = gtk_image_new_from_surface (surface);

  gtk_style_context_add_class (gtk_widget_get_style_context (icon), "calendar-color-image");

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

  gtk_container_add (GTK_CONTAINER (box), icon);
  gtk_container_add (GTK_CONTAINER (box), label);
  gtk_container_add (GTK_CONTAINER (box), checkbox);
  gtk_container_add (GTK_CONTAINER (row), box);

  g_object_set_data (G_OBJECT (row), "check", checkbox);
  g_object_set_data (G_OBJECT (row), "calendar", calendar);

  gtk_widget_show_all (row);

  g_clear_pointer (&surface, cairo_surface_destroy);

  return row;
}

static void
add_calendar (GcalCalendarPopover *self,
              GcalCalendar        *calendar)
{
  GtkWidget *row;

  row = make_calendar_row (calendar);
  gtk_container_add (GTK_CONTAINER (self->calendar_listbox), row);
}

static void
remove_calendar (GcalCalendarPopover *self,
                 GcalCalendar        *calendar)
{
  g_autoptr (GList) children = NULL;
  GList *aux;

  children = gtk_container_get_children (GTK_CONTAINER (self->calendar_listbox));

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


/*
 * Callbacks
 */

static gint
listbox_sort_func (GtkListBoxRow *row1,
                   GtkListBoxRow *row2,
                   gpointer       user_data)
{
  GcalCalendar *calendar1, *calendar2;

  calendar1 = g_object_get_data (G_OBJECT (row1), "calendar");
  calendar2 = g_object_get_data (G_OBJECT (row2), "calendar");

  return g_ascii_strcasecmp (gcal_calendar_get_name (calendar1), gcal_calendar_get_name (calendar2));
}

static void
on_manager_calendar_added_cb (GcalManager         *manager,
                              GcalCalendar        *calendar,
                              GcalCalendarPopover *self)
{
  add_calendar (self, calendar);
}

static void
on_manager_calendar_changed_cb (GcalManager         *manager,
                                GcalCalendar        *calendar,
                                GcalCalendarPopover *self)
{
  remove_calendar (self, calendar);
  add_calendar (self, calendar);
}

static void
on_manager_calendar_removed_cb (GcalManager         *manager,
                                GcalCalendar        *calendar,
                                GcalCalendarPopover *self)
{
  remove_calendar (self, calendar);
}

static void
on_listbox_row_activated_cb (GtkListBox          *listbox,
                             GtkListBoxRow       *row,
                             GcalCalendarPopover *self)
{
  GtkToggleButton *check;

  check = (GtkToggleButton *) g_object_get_data (G_OBJECT (row), "check");

  gtk_toggle_button_set_active (check, !gtk_toggle_button_get_active (check));
}


/*
 * GObject overrides
 */

static void
gcal_calendar_popover_finalize (GObject *object)
{
  GcalCalendarPopover *self = (GcalCalendarPopover *)object;

  g_clear_object (&self->context);

  G_OBJECT_CLASS (gcal_calendar_popover_parent_class)->finalize (object);
}

static void
gcal_calendar_popover_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  GcalCalendarPopover *self = GCAL_CALENDAR_POPOVER (object);

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
gcal_calendar_popover_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  GcalCalendarPopover *self = GCAL_CALENDAR_POPOVER (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      {
        GcalManager *manager;

        g_assert (self->context == NULL);
        self->context = g_value_dup_object (value);

        manager = gcal_context_get_manager (self->context);
        if (!gcal_manager_get_loading (manager))
          {
            g_autoptr (GList) calendars = NULL;
            GList *l;

            calendars = gcal_manager_get_calendars (manager);

            for (l = calendars; l; l = l->next)
              add_calendar (self, l->data);
          }

        g_signal_connect (manager, "calendar-added", G_CALLBACK (on_manager_calendar_added_cb), object);
        g_signal_connect (manager, "calendar-removed", G_CALLBACK (on_manager_calendar_removed_cb), object);
        g_signal_connect (manager, "calendar-changed", G_CALLBACK (on_manager_calendar_changed_cb), object);

      }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_calendar_popover_class_init (GcalCalendarPopoverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gcal_calendar_popover_finalize;
  object_class->get_property = gcal_calendar_popover_get_property;
  object_class->set_property = gcal_calendar_popover_set_property;

  /**
   * GcalCalendarPopover::context:
   *
   * The #GcalContext of the application.
   */
  properties[PROP_CONTEXT] = g_param_spec_object ("context",
                                                  "Context of the application",
                                                  "The context of the application",
                                                  GCAL_TYPE_CONTEXT,
                                                  G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/calendar-popover.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalCalendarPopover, calendar_listbox);

  gtk_widget_class_bind_template_callback (widget_class, on_listbox_row_activated_cb);
}

static void
gcal_calendar_popover_init (GcalCalendarPopover *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_list_box_set_sort_func (GTK_LIST_BOX (self->calendar_listbox),
                              (GtkListBoxSortFunc) listbox_sort_func,
                              self,
                              NULL);
}
