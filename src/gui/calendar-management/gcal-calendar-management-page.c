/* gcal-calendar-management-page.c
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

#define G_LOG_DOMAIN "GcalCalendarManagementPage"

#include "gcal-calendar-management-dialog.h"
#include "gcal-calendar-management-page.h"
#include "gcal-context.h"

G_DEFINE_INTERFACE (GcalCalendarManagementPage, gcal_calendar_management_page, G_TYPE_OBJECT)

enum
{
  SWITCH_PAGE,
  N_SIGNALS,
};

static guint signals[N_SIGNALS] = { 0, };

static void
gcal_calendar_management_page_default_init (GcalCalendarManagementPageInterface *iface)
{
  g_object_interface_install_property (iface,
                                       g_param_spec_object ("context",
                                                            "Context",
                                                            "Context",
                                                            GCAL_TYPE_CONTEXT,
                                                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  signals[SWITCH_PAGE] = g_signal_new ("switch-page",
                                       GCAL_TYPE_CALENDAR_MANAGEMENT_PAGE,
                                       G_SIGNAL_RUN_LAST,
                                       G_STRUCT_OFFSET (GcalCalendarManagementPageInterface, switch_page),
                                       NULL, NULL, NULL,
                                       G_TYPE_NONE,
                                       2,
                                       G_TYPE_STRING,
                                       GCAL_TYPE_CALENDAR);
}

const gchar*
gcal_calendar_management_page_get_name (GcalCalendarManagementPage *self)
{
  g_return_val_if_fail (GCAL_IS_CALENDAR_MANAGEMENT_PAGE (self), NULL);
  g_return_val_if_fail (GCAL_CALENDAR_MANAGEMENT_PAGE_GET_IFACE (self)->get_name, NULL);

  return GCAL_CALENDAR_MANAGEMENT_PAGE_GET_IFACE (self)->get_name (self);
}

const gchar*
gcal_calendar_management_page_get_title (GcalCalendarManagementPage *self)
{
  g_return_val_if_fail (GCAL_IS_CALENDAR_MANAGEMENT_PAGE (self), NULL);
  g_return_val_if_fail (GCAL_CALENDAR_MANAGEMENT_PAGE_GET_IFACE (self)->get_title, NULL);

  return GCAL_CALENDAR_MANAGEMENT_PAGE_GET_IFACE (self)->get_title (self);
}

void
gcal_calendar_management_page_activate (GcalCalendarManagementPage *self,
                                        GcalCalendar               *calendar)
{
  g_return_if_fail (GCAL_IS_CALENDAR_MANAGEMENT_PAGE (self));

  if (GCAL_CALENDAR_MANAGEMENT_PAGE_GET_IFACE (self)->activate)
    {
      g_debug ("Activating %s", G_OBJECT_TYPE_NAME (self));
      GCAL_CALENDAR_MANAGEMENT_PAGE_GET_IFACE (self)->activate (self, calendar);
    }
}

void
gcal_calendar_management_page_deactivate (GcalCalendarManagementPage *self)
{
  g_return_if_fail (GCAL_IS_CALENDAR_MANAGEMENT_PAGE (self));

  if (GCAL_CALENDAR_MANAGEMENT_PAGE_GET_IFACE (self)->deactivate)
    {
      g_debug ("Deactivating %s", G_OBJECT_TYPE_NAME (self));
      GCAL_CALENDAR_MANAGEMENT_PAGE_GET_IFACE (self)->deactivate (self);
    }
}

void
gcal_calendar_management_page_switch_page (GcalCalendarManagementPage *self,
                                           const gchar                *page_name,
                                           GcalCalendar               *calendar)
{
  g_return_if_fail (GCAL_IS_CALENDAR_MANAGEMENT_PAGE (self));

  g_signal_emit (self, signals[SWITCH_PAGE], 0, page_name, calendar);
}
