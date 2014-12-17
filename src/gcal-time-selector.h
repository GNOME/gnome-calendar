/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gcal-time-selector.h
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

#ifndef __GCAL_TIME_SELECTOR_H__
#define __GCAL_TIME_SELECTOR_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GCAL_TYPE_TIME_SELECTOR             (gcal_time_selector_get_type ())
#define GCAL_TIME_SELECTOR(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), GCAL_TYPE_TIME_SELECTOR, GcalTimeSelector))
#define GCAL_TIME_SELECTOR_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GCAL_TYPE_TIME_SELECTOR, GcalTimeSelectorClass))
#define GCAL_IS_TIME_SELECTOR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), GCAL_TYPE_TIME_SELECTOR))
#define GCAL_IS_TIME_SELECTOR_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GCAL_TYPE_TIME_SELECTOR))
#define GCAL_TIME_SELECTOR_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), GCAL_TYPE_TIME_SELECTOR, GcalTimeSelectorClass))

typedef struct _GcalTimeSelector                GcalTimeSelector;
typedef struct _GcalTimeSelectorClass           GcalTimeSelectorClass;
typedef struct _GcalTimeSelectorPrivate         GcalTimeSelectorPrivate;

struct _GcalTimeSelector
{
  GtkToggleButton parent;
  /* add your public declarations here */

  GcalTimeSelectorPrivate *priv;
};

struct _GcalTimeSelectorClass
{
  GtkToggleButtonClass parent_class;

  /* signals */
  void (*modified)  (GcalTimeSelector *selector);
};

GType            gcal_time_selector_get_type     (void);

GtkWidget*       gcal_time_selector_new          (void);

void             gcal_time_selector_set_time     (GcalTimeSelector *selector,
                                                  gint              hours,
                                                  gint              minutes);

void             gcal_time_selector_get_time     (GcalTimeSelector *selector,
                                                  gint             *hours,
                                                  gint             *minutes);

G_END_DECLS

#endif /* __GCAL_TIME_SELECTOR_H__ */
