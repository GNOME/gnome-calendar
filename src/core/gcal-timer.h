/* gcal-timer.h
 *
 * Copyright (C) 2017 - Florian Brosch <flo.brosch@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GCAL_TIMER_H__
#define __GCAL_TIMER_H__

#include <glib.h>

struct _GcalTimer;
typedef struct _GcalTimer GcalTimer;

typedef void (*GCalTimerFunc) (GcalTimer *self, gpointer  data);


GcalTimer*           gcal_timer_new                              (gint64              default_duration);

void                 gcal_timer_start                            (GcalTimer          *self);

void                 gcal_timer_reset                            (GcalTimer          *self);

void                 gcal_timer_stop                             (GcalTimer          *self);

gboolean             gcal_timer_is_running                       (GcalTimer          *self);

void                 gcal_timer_set_default_duration             (GcalTimer          *self,
                                                                  gint64              duration);

gint64               gcal_timer_get_default_duration             (GcalTimer          *self);

void                 gcal_timer_set_callback                     (GcalTimer          *self,
                                                                  GCalTimerFunc       func,
                                                                  gpointer            data,
                                                                  GDestroyNotify      notify);

void                 gcal_timer_free                             (GcalTimer          *self);


#endif /* __GCAL_TIMER_H__ */
