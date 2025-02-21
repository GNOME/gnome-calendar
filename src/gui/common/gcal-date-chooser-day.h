/* GTK - The GIMP Toolkit
 * Copyright (C) 2015 Red Hat, Inc.
 * Copyright (C) 2022 Purism SPC
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GCAL_TYPE_DATE_CHOOSER_DAY (gcal_date_chooser_day_get_type())
G_DECLARE_FINAL_TYPE (GcalDateChooserDay, gcal_date_chooser_day, GCAL, DATE_CHOOSER_DAY, GtkButton)

GtkWidget*           gcal_date_chooser_day_new                   (void);

GDateTime*           gcal_date_chooser_day_get_date              (GcalDateChooserDay *day);

void                 gcal_date_chooser_day_set_date              (GcalDateChooserDay *day,
                                                                  GDateTime          *date);

gboolean             gcal_date_chooser_day_get_has_dot           (GcalDateChooserDay *day);

void                 gcal_date_chooser_day_set_has_dot           (GcalDateChooserDay *day,
                                                                  gboolean            has_dot);

gboolean             gcal_date_chooser_day_get_dot_visible       (GcalDateChooserDay *day);

void                 gcal_date_chooser_day_set_dot_visible       (GcalDateChooserDay *day,
                                                                  gboolean            dot_visible);

void                 gcal_date_chooser_day_set_other_month       (GcalDateChooserDay *day,
                                                                  gboolean            other_month);

void                 gcal_date_chooser_day_set_selected          (GcalDateChooserDay *day,
                                                                  gboolean            selected);

G_END_DECLS
