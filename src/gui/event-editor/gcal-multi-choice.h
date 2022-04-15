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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GCAL_MULTI_CHOICE_H
#define GCAL_MULTI_CHOICE_H

#include <gtk/gtk.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define GCAL_TYPE_MULTI_CHOICE (gcal_multi_choice_get_type())

G_DECLARE_FINAL_TYPE (GcalMultiChoice, gcal_multi_choice, GCAL, MULTI_CHOICE, GtkBox)

GtkWidget*           gcal_multi_choice_new                       (void);

gint                 gcal_multi_choice_get_value                 (GcalMultiChoice    *self);

void                 gcal_multi_choice_set_value                 (GcalMultiChoice    *self,
                                                                  gint                value);

GtkPopover*          gcal_multi_choice_get_popover               (GcalMultiChoice    *self);

void                 gcal_multi_choice_set_popover               (GcalMultiChoice    *self,
                                                                  GtkWidget          *popover);

void                 gcal_multi_choice_set_choices               (GcalMultiChoice     *self,
                                                                  const gchar        **selfs);

typedef gchar*       (*GcalMultiChoiceFormatCallback)            (GcalMultiChoice     *self,
                                                                  gint                 value,
                                                                  gpointer             user_data);

void                 gcal_multi_choice_set_format_callback       (GcalMultiChoice     *self,
                                                                  GcalMultiChoiceFormatCallback  callback,
                                                                  gpointer             user_data,
                                                                  GDestroyNotify       notify);

G_END_DECLS

#endif /* GCAL_MULTI_CHOICE_H */
