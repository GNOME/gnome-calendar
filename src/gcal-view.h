/*
 * gcal-view.h
 *
 * Copyright (C) 2012 - Erick Pérez Castellanos
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
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

#ifndef __GCAL_VIEW_H__
#define __GCAL_VIEW_H__

#include <glib-object.h>
#include <gtk/gtk.h>

#include <libical/icaltime.h>

G_BEGIN_DECLS

#define GCAL_TYPE_VIEW                 (gcal_view_get_type ())
#define GCAL_VIEW(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCAL_TYPE_VIEW, GcalView))
#define GCAL_IS_VIEW(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCAL_TYPE_VIEW))
#define GCAL_VIEW_GET_INTERFACE(inst)  (G_TYPE_INSTANCE_GET_INTERFACE ((inst), GCAL_TYPE_VIEW, GcalViewIface))

struct _GcalViewChild
{
  GtkWidget *widget;
  gboolean   hidden_by_me;
};

typedef struct _GcalViewChild GcalViewChild;

typedef struct _GcalView                GcalView;
typedef struct _GcalViewIface           GcalViewIface;

struct _GcalViewIface
{
  GTypeInterface parent_iface;

  /* signals */
  void            (*create_event)                       (GcalView *view, icaltimetype *start_span, icaltimetype *end_span, gdouble x, gdouble y);
  void            (*updated)                            (GcalView *view, icaltimetype *date);

  /* pure virtual methods */
  icaltimetype*   (*get_initial_date)                   (GcalView *view);
  icaltimetype*   (*get_final_date)                     (GcalView *view);

  gboolean        (*contains)                           (GcalView *view, icaltimetype *date);
  void            (*remove_by_uuid)                     (GcalView *view, const gchar *uuid);
  GtkWidget*      (*get_by_uuid)                        (GcalView *view, const gchar *uuid);
  void            (*reposition_child)                   (GcalView *view, const gchar *uuid);

/* FIXME remove me in favor of the one below */
  void            (*clear_selection)                    (GcalView *view);

/* FIXME remove me in favor of the one below */
  void            (*create_event_on_current_unit)       (GcalView *view);

/* New API */
  /* Marks related API */
  void       (*mark_current_unit)             (GcalView     *view);
  void       (*clear_mark)                    (GcalView     *view);

  /* Navigation related API */
  void       (*move_back)                     (GcalView     *view,
					       gint          steps);
  void       (*move_forward)                  (GcalView     *view,
					       gint          steps);

};

GType         gcal_view_get_type                      (void);


void          gcal_view_set_date                      (GcalView     *view,
						       icaltimetype *date);

icaltimetype* gcal_view_get_date                      (GcalView     *view);

icaltimetype* gcal_view_get_initial_date              (GcalView     *view);

icaltimetype* gcal_view_get_final_date                (GcalView     *view);

gboolean      gcal_view_contains                      (GcalView     *view,
						       icaltimetype *date);

void          gcal_view_remove_by_uuid                (GcalView     *view,
						       const gchar  *uuid);

GtkWidget*    gcal_view_get_by_uuid                   (GcalView     *view,
						       const gchar  *uuid);

void          gcal_view_reposition_child              (GcalView     *view,
						       const gchar  *uuid);

/* FIXME remove me in favor of the one below */
void          gcal_view_clear_selection               (GcalView     *view);

/* FIXME remove me in favor of the one below */
void          gcal_view_create_event_on_current_unit  (GcalView     *view);

/* New API */
void          gcal_view_mark_current_unit             (GcalView     *view);

void          gcal_view_clear_mark                    (GcalView     *view);

void          gcal_view_move_back                     (GcalView     *view,
						       gint          steps);

void          gcal_view_move_forward                  (GcalView     *view,
						       gint          steps);

G_END_DECLS

#endif /* __GCAL_MONTH_VIEW_H__ */
