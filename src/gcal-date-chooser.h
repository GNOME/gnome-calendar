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

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef enum
{
  GCAL_DATE_CHOOSER_DAY_NONE    = 1 << 0,
  GCAL_DATE_CHOOSER_DAY_WEEKEND = 1 << 1,
  GCAL_DATE_CHOOSER_DAY_HOLIDAY = 1 << 2,
  GCAL_DATE_CHOOSER_DAY_MARKED  = 1 << 3
} GcalDateChooserDayOptions;

#define GCAL_TYPE_DATE_CHOOSER (gcal_date_chooser_get_type ())

G_DECLARE_FINAL_TYPE (GcalDateChooser, gcal_date_chooser, GCAL, DATE_CHOOSER, GtkBin)

typedef GcalDateChooserDayOptions (*GcalDateChooserDayOptionsCallback) (GcalDateChooser *self,
                                                                        GDateTime       *date,
                                                                        gpointer         user_data);

GtkWidget*           gcal_date_chooser_new                       (void);

GDateTime*           gcal_date_chooser_get_date                  (GcalDateChooser    *self);

void                 gcal_date_chooser_set_date                  (GcalDateChooser    *self,
                                                                  GDateTime          *date);

void                 gcal_date_chooser_set_day_options_callback  (GcalDateChooser    *self,
                                                                  GcalDateChooserDayOptionsCallback callback,
                                                                  gpointer            data,
                                                                  GDestroyNotify      destroy);

void                 gcal_date_chooser_invalidate_day_options    (GcalDateChooser    *self);

gboolean             gcal_date_chooser_get_no_month_change       (GcalDateChooser    *self);

void                 gcal_date_chooser_set_no_month_change       (GcalDateChooser    *self,
                                                                  gboolean            setting);

gboolean             gcal_date_chooser_get_show_heading          (GcalDateChooser    *self);

void                 gcal_date_chooser_set_show_heading          (GcalDateChooser    *self,
                                                                  gboolean            setting);

gboolean             gcal_date_chooser_get_show_day_names        (GcalDateChooser    *self);

void                 gcal_date_chooser_set_show_day_names        (GcalDateChooser    *self,
                                                                  gboolean            setting);

gboolean             gcal_date_chooser_get_show_week_numbers     (GcalDateChooser    *self);

void                 gcal_date_chooser_set_show_week_numbers     (GcalDateChooser    *self,
                                                                  gboolean            setting);

gboolean             gcal_date_chooser_get_show_month_only       (GcalDateChooser    *self);

void                 gcal_date_chooser_set_show_month_only       (GcalDateChooser    *self,
                                                                  gboolean            setting);

G_END_DECLS

#endif /* __GCAL_DATE_CHOOSER_H__ */
