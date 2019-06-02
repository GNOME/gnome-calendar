/* gcal-new-calendar-page.c
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

#define G_LOG_DOMAIN "GcalNewCalendarPage"

#include <glib/gi18n.h>

#include "gcal-context.h"
#include "gcal-calendar-management-page.h"
#include "gcal-new-calendar-page.h"

struct _GcalNewCalendarPage
{
  GtkBox              parent;

  GcalContext        *context;
};

static void          gcal_calendar_management_page_iface_init    (GcalCalendarManagementPageInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GcalNewCalendarPage, gcal_new_calendar_page, GTK_TYPE_BOX,
                         G_IMPLEMENT_INTERFACE (GCAL_TYPE_CALENDAR_MANAGEMENT_PAGE,
                                                gcal_calendar_management_page_iface_init))

enum
{
  PROP_0,
  PROP_CONTEXT,
  N_PROPS
};

/*
 * GcalCalendarManagementPage iface
 */

static const gchar*
gcal_new_calendar_page_get_name (GcalCalendarManagementPage *page)
{
  return "new-calendar";
}

static const gchar*
gcal_new_calendar_page_get_title (GcalCalendarManagementPage *page)
{
  return _("New Calendar");
}

static void
gcal_calendar_management_page_iface_init (GcalCalendarManagementPageInterface *iface)
{
  iface->get_name = gcal_new_calendar_page_get_name;
  iface->get_title = gcal_new_calendar_page_get_title;
}

/*
 * GObject overrides
 */

static void
gcal_new_calendar_page_finalize (GObject *object)
{
  GcalNewCalendarPage *self = (GcalNewCalendarPage *)object;

  g_clear_object (&self->context);

  G_OBJECT_CLASS (gcal_new_calendar_page_parent_class)->finalize (object);
}

static void
gcal_new_calendar_page_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GcalNewCalendarPage *self = GCAL_NEW_CALENDAR_PAGE (object);

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
gcal_new_calendar_page_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GcalNewCalendarPage *self = GCAL_NEW_CALENDAR_PAGE (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      self->context = g_value_dup_object (value);
      g_assert (self->context != NULL);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_new_calendar_page_class_init (GcalNewCalendarPageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gcal_new_calendar_page_finalize;
  object_class->get_property = gcal_new_calendar_page_get_property;
  object_class->set_property = gcal_new_calendar_page_set_property;

  g_object_class_override_property (object_class, PROP_CONTEXT, "context");

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/new-calendar-page.ui");
}

static void
gcal_new_calendar_page_init (GcalNewCalendarPage *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
