/* gcal-calendar-navigation-button.c
 *
 * Copyright 2025 Markus Göllnitz <camelcasenick@bewares.it>
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

#define G_LOG_DOMAIN "GcalCalendarNavigationButton"

#include <glib/gi18n.h>

#include "gcal-calendar-navigation-button.h"
#include "gcal-utils.h"

struct _GcalCalendarNavigationButton
{
  AdwBin parent;

  GDateTime *active_date;
  gboolean active;

  GtkMenuButton *button;
  GtkStack *stack;
};

G_DEFINE_TYPE (GcalCalendarNavigationButton, gcal_calendar_navigation_button, ADW_TYPE_BIN)

enum
{
  PROP_0,
  PROP_ACTIVE_DATE,
  PROP_ACTIVE,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

/*
 * GObject overrides
 */

static void
gcal_calendar_navigation_button_finalize (GObject *object)
{
  GcalCalendarNavigationButton *self = (GcalCalendarNavigationButton *) object;

  gcal_clear_date_time (&self->active_date);

  G_OBJECT_CLASS (gcal_calendar_navigation_button_parent_class)->finalize (object);
}

static void
gcal_calendar_navigation_button_get_property (GObject *object,
                                              guint prop_id,
                                              GValue *value,
                                              GParamSpec *pspec)
{
  GcalCalendarNavigationButton *self = GCAL_CALENDAR_NAVIGATION_BUTTON (object);

  switch (prop_id)
    {
    case PROP_ACTIVE_DATE:
      g_value_set_boxed (value, self->active_date);
      break;

    case PROP_ACTIVE:
      g_value_set_boolean (value, self->active);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_calendar_navigation_button_set_property (GObject *object,
                                              guint prop_id,
                                              const GValue *value,
                                              GParamSpec *pspec)
{
  GcalCalendarNavigationButton *self = GCAL_CALENDAR_NAVIGATION_BUTTON (object);

  switch (prop_id)
    {
    case PROP_ACTIVE_DATE:
      gcal_clear_date_time (&self->active_date);
      self->active_date = g_date_time_ref (g_value_get_boxed (value));
      /*
       * Translators: %B is the month name and %Y is the year.
       * More formats can be found on the doc:
       * https://docs.gtk.org/glib/method.DateTime.format.html
       */
      gtk_menu_button_set_label (self->button, g_date_time_format (self->active_date, _ ("%B %Y")));
      break;

    case PROP_ACTIVE:
      self->active = g_value_get_boolean (value);
      gtk_stack_set_visible_child_name (self->stack, self->active ? "active" : "inactive");
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_calendar_navigation_button_class_init (GcalCalendarNavigationButtonClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gcal_calendar_navigation_button_finalize;
  object_class->get_property = gcal_calendar_navigation_button_get_property;
  object_class->set_property = gcal_calendar_navigation_button_set_property;

  properties[PROP_ACTIVE_DATE] = g_param_spec_boxed ("active-date",
                                                     NULL,
                                                     "The active/selected date",
                                                     G_TYPE_DATE_TIME,
                                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_ACTIVE] = g_param_spec_boolean ("active",
                                                  NULL,
                                                  "Whether the date chooser is active, or a static date should be displayed",
                                                  TRUE,
                                                  G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/gui/gcal-calendar-navigation-button.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalCalendarNavigationButton, button);
  gtk_widget_class_bind_template_child (widget_class, GcalCalendarNavigationButton, stack);
}

static void
gcal_calendar_navigation_button_init (GcalCalendarNavigationButton *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
