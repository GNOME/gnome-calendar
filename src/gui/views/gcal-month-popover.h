/* gcal-month-popover.h
 *
 * Copyright © 2017 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#pragma once

#include "gcal-manager.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GCAL_TYPE_MONTH_POPOVER (gcal_month_popover_get_type())
G_DECLARE_FINAL_TYPE (GcalMonthPopover, gcal_month_popover, GCAL, MONTH_POPOVER, GtkWidget)

GtkWidget*           gcal_month_popover_new                      (void);


void                 gcal_month_popover_set_relative_to          (GcalMonthPopover   *self,
                                                                  GtkWidget          *relative_to);

void                 gcal_month_popover_popup                    (GcalMonthPopover   *self);

void                 gcal_month_popover_popdown                  (GcalMonthPopover   *self);

GDateTime*           gcal_month_popover_get_date                 (GcalMonthPopover   *self);

void                 gcal_month_popover_set_date                 (GcalMonthPopover   *self,
                                                                  GDateTime          *date);

G_END_DECLS
