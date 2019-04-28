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

#include <dazzle.h>

G_BEGIN_DECLS

#define GCAL_TYPE_SEARCH_HIT (gcal_search_hit_get_type ())
G_DECLARE_INTERFACE (GcalSearchHit, gcal_search_hit, GCAL, SEARCH_HIT, DzlSuggestion)

struct _GcalSearchHitInterface
{
  GTypeInterface parent;

  gint               (*get_priority)                             (GcalSearchHit      *self);

  gint               (*compare)                                  (GcalSearchHit      *a,
                                                                  GcalSearchHit      *b);
};

gint                 gcal_search_hit_get_priority                (GcalSearchHit      *self);

gint                 gcal_search_hit_compare                     (GcalSearchHit      *a,
                                                                  GcalSearchHit      *b);

G_END_DECLS
