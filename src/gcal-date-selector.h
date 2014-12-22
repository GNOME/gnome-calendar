/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gcal-date-selector.h
 * Copyright (C) 2012 Erick Pérez Castellanos <erickpc@gnome.org>
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
#define GCAL_DATE_SELECTOR(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), GCAL_TYPE_DATE_SELECTOR, GcalDateSelector))
#define GCAL_DATE_SELECTOR_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GCAL_TYPE_DATE_SELECTOR, GcalDateSelectorClass))
#define GCAL_IS_DATE_SELECTOR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), GCAL_TYPE_DATE_SELECTOR))
#define GCAL_IS_DATE_SELECTOR_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GCAL_TYPE_DATE_SELECTOR))
#define GCAL_DATE_SELECTOR_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), GCAL_TYPE_DATE_SELECTOR, GcalDateSelectorClass))

typedef struct _GcalTime GcalTime;

typedef struct _GcalDateSelector             GcalDateSelector;
typedef struct _GcalDateSelectorClass        GcalDateSelectorClass;
typedef struct _GcalDateSelectorPrivate      GcalDateSelectorPrivate;

struct _GcalDateSelector
{
  GtkToggleButton parent;
};

struct _GcalDateSelectorClass
{
  GtkToggleButtonClass parent_class;

  /* signals */
  void (*modified)  (GcalDateSelector *selector);
};

GType            gcal_date_selector_get_type        (void);

GtkWidget*       gcal_date_selector_new             (void);

void             gcal_date_selector_use_24h_format  (GcalDateSelector *selector,
                                                     gboolean          use_24h_format);

void             gcal_date_selector_set_date        (GcalDateSelector *selector,
                                                     gint              day,
                                                     gint              month,
                                                     gint              year);

void             gcal_date_selector_get_date        (GcalDateSelector *selector,
                                                     gint             *day,
                                                     gint             *month,
                                                     gint             *year);

G_END_DECLS

#endif /* __GCAL_DATE_SELECTOR_H__ */
