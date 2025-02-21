/* GTK - The GIMP Toolkit
 * Copyright (C) 2015 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GCAL_DATE_CHOOSER_H__
#define __GCAL_DATE_CHOOSER_H__

#include <adwaita.h>

G_BEGIN_DECLS

#define GCAL_TYPE_DATE_CHOOSER (gcal_date_chooser_get_type ())

G_DECLARE_FINAL_TYPE (GcalDateChooser, gcal_date_chooser, GCAL, DATE_CHOOSER, AdwBin)

GtkWidget*           gcal_date_chooser_new                       (void);

gboolean             gcal_date_chooser_get_show_heading          (GcalDateChooser    *self);

void                 gcal_date_chooser_set_show_heading          (GcalDateChooser    *self,
                                                                  gboolean            setting);

gboolean             gcal_date_chooser_get_show_day_names        (GcalDateChooser    *self);

void                 gcal_date_chooser_set_show_day_names        (GcalDateChooser    *self,
                                                                  gboolean            setting);

gboolean             gcal_date_chooser_get_show_week_numbers     (GcalDateChooser    *self);

void                 gcal_date_chooser_set_show_week_numbers     (GcalDateChooser    *self,
                                                                  gboolean            setting);

gboolean             gcal_date_chooser_get_show_selected_week    (GcalDateChooser    *self);

void                 gcal_date_chooser_set_show_selected_week    (GcalDateChooser    *self,
                                                                  gboolean            setting);

gboolean             gcal_date_chooser_get_show_events           (GcalDateChooser    *self);

void                 gcal_date_chooser_set_show_events           (GcalDateChooser    *self,
                                                                  gboolean            setting);

gboolean             gcal_date_chooser_get_split_month_year      (GcalDateChooser    *self);

void                 gcal_date_chooser_set_split_month_year      (GcalDateChooser    *self,
                                                                  gboolean            setting);

G_END_DECLS

#endif /* __GCAL_DATE_CHOOSER_H__ */
