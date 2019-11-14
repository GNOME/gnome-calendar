/* gcal-time-selector.h
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

#ifndef __GCAL_TIME_SELECTOR_H__
#define __GCAL_TIME_SELECTOR_H__

#include "gcal-enums.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GCAL_TYPE_TIME_SELECTOR             (gcal_time_selector_get_type ())
G_DECLARE_FINAL_TYPE (GcalTimeSelector, gcal_time_selector, GCAL, TIME_SELECTOR, GtkBox)

GtkWidget*       gcal_time_selector_new          (void);

void             gcal_time_selector_set_time_format (GcalTimeSelector *selector,
                                                     GcalTimeFormat    time_format);

GDateTime*       gcal_time_selector_get_time     (GcalTimeSelector *selector);

void             gcal_time_selector_set_time     (GcalTimeSelector *selector,
                                                  GDateTime        *time);

G_END_DECLS

#endif /* __GCAL_TIME_SELECTOR_H__ */
