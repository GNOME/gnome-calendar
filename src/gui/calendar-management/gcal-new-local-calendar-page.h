/* gcal-new-local-calendar-page.h
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2019 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 * SPDX-FileCopyrightText: 2026 The GNOME Calendar authors
 */

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define GCAL_TYPE_NEW_LOCAL_CALENDAR_PAGE (gcal_new_local_calendar_page_get_type())
G_DECLARE_FINAL_TYPE (GcalNewLocalCalendarPage, gcal_new_local_calendar_page, GCAL, NEW_LOCAL_CALENDAR_PAGE, AdwNavigationPage)

G_END_DECLS
