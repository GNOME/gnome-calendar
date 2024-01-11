/* gcal-quick-add-popover.h
 *
 * Copyright (C) 2016 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#ifndef GCAL_QUICK_ADD_POPOVER_H
#define GCAL_QUICK_ADD_POPOVER_H

#include "gcal-context.h"
#include "gcal-event.h"

#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GCAL_TYPE_QUICK_ADD_POPOVER (gcal_quick_add_popover_get_type())

G_DECLARE_FINAL_TYPE (GcalQuickAddPopover, gcal_quick_add_popover, GCAL, QUICK_ADD_POPOVER, GtkPopover)

GtkWidget*           gcal_quick_add_popover_new                  (void);

GcalRange*           gcal_quick_add_popover_get_range            (GcalQuickAddPopover *self);

void                 gcal_quick_add_popover_set_range            (GcalQuickAddPopover *self,
                                                                  GcalRange           *range);

G_END_DECLS

#endif /* GCAL_QUICK_ADD_POPOVER_H */
