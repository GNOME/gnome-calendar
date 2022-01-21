/* gcal-search-hit.h
 *
 * Copyright 2019 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GCAL_TYPE_SEARCH_HIT (gcal_search_hit_get_type ())
G_DECLARE_DERIVABLE_TYPE (GcalSearchHit, gcal_search_hit, GCAL, SEARCH_HIT, GObject)

struct _GcalSearchHitClass
{
  GObjectClass parent_class;

  void               (*activate)                                 (GcalSearchHit      *self,
                                                                  GtkWidget          *for_widget);

  gint               (*get_priority)                             (GcalSearchHit      *self);

  gint               (*compare)                                  (GcalSearchHit      *a,
                                                                  GcalSearchHit      *b);
};

GcalSearchHit *      gcal_search_hit_new                         (void);

const gchar *        gcal_search_hit_get_id                      (GcalSearchHit      *self);

void                 gcal_search_hit_set_id                      (GcalSearchHit      *self,
                                                                  const gchar        *id);

const gchar *        gcal_search_hit_get_title                   (GcalSearchHit      *self);

void                 gcal_search_hit_set_title                   (GcalSearchHit      *self,
                                                                  const gchar        *title);

const gchar *        gcal_search_hit_get_subtitle                (GcalSearchHit      *self);

void                 gcal_search_hit_set_subtitle                (GcalSearchHit      *self,
                                                                  const gchar        *subtitle);

GdkPaintable *       gcal_search_hit_get_primary_icon            (GcalSearchHit      *self);

void                 gcal_search_hit_set_primary_icon            (GcalSearchHit      *self,
                                                                  GdkPaintable       *paintable);

void                 gcal_search_hit_activate                    (GcalSearchHit      *self,
                                                                  GtkWidget          *for_widget);

gint                 gcal_search_hit_get_priority                (GcalSearchHit      *self);

gint                 gcal_search_hit_compare                     (GcalSearchHit      *a,
                                                                  GcalSearchHit      *b);

G_END_DECLS
