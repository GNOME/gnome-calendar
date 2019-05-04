/* gcal-stub-calendar.c
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

#include "gcal-stub-calendar.h"

struct _GcalStubCalendar
{
  GcalCalendar        parent;
};

static void          g_initable_iface_init                       (GInitableIface     *iface);

G_DEFINE_TYPE_WITH_CODE (GcalStubCalendar, gcal_stub_calendar, GCAL_TYPE_CALENDAR,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, g_initable_iface_init))

/*
 * GInitable iface
 */

static gboolean
gcal_stub_calendar_initable_init (GInitable     *initable,
                                  GCancellable  *cancellable,
                                  GError       **error)
{
  return TRUE;
}

static void
g_initable_iface_init (GInitableIface *iface)
{
  iface->init = gcal_stub_calendar_initable_init;
}

static void
gcal_stub_calendar_class_init (GcalStubCalendarClass *klass)
{
}

static void
gcal_stub_calendar_init (GcalStubCalendar *self)
{
}

GcalCalendar*
gcal_stub_calendar_new (GCancellable  *cancellable,
                        GError       **error)
{
  g_autoptr (ESource) source = NULL;

  source = e_source_new_with_uid ("stub", NULL, error);

  return g_initable_new (GCAL_TYPE_STUB_CALENDAR,
                         cancellable,
                         error,
                         "source", source,
                         NULL);
}
