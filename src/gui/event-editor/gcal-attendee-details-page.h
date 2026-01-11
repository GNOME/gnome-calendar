/* gcal-attendee-details-page.c
 *
 * Copyright (C) 2025 Alen Galinec <mind.on.warp@gmail.com>
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
 */

#ifndef GCAL_ATTENDEE_DETAILS_PAGE_H
#define GCAL_ATTENDEE_DETAILS_PAGE_H

#include "gcal-enums.h"
#include <adwaita.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define GCAL_TYPE_ATTENDEE_DETAILS_PAGE (gcal_attendee_details_page_get_type ())
G_DECLARE_FINAL_TYPE (GcalAttendeeDetailsPage, gcal_attendee_details_page, GCAL, ATTENDEE_DETAILS_PAGE, AdwNavigationPage)

void                 gcal_attendee_details_page_set_attendees          (GcalAttendeeDetailsPage *self,
                                                                        GListModel              *attendees);

void                 gcal_attendee_details_page_set_type_filter        (GcalAttendeeDetailsPage         *self,
                                                                        GcalEventAttendeeTypeFilterFlags flags);

G_END_DECLS

#endif
