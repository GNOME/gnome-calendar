/* gcal-date-selector.h
 *
 * Copyright (C) 2015 Erick Pérez Castellanos <erickpc@gnome.org>
 *
 * gnome-calendar is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * gnome-calendar is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GCAL_DATE_SELECTOR_H__
#define __GCAL_DATE_SELECTOR_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GCAL_TYPE_DATE_SELECTOR             (gcal_date_selector_get_type ())

G_DECLARE_FINAL_TYPE (GcalDateSelector, gcal_date_selector, GCAL, DATE_SELECTOR, GtkEntry)

GtkWidget*       gcal_date_selector_new             (void);

GDateTime*       gcal_date_selector_get_date        (GcalDateSelector *selector);

void             gcal_date_selector_set_date        (GcalDateSelector *selector,
                                                     GDateTime        *date);

G_END_DECLS

#endif /* __GCAL_DATE_SELECTOR_H__ */
