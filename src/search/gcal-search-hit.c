/* gcal-search-hit.c
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

#include "gcal-search-hit.h"

G_DEFINE_INTERFACE (GcalSearchHit, gcal_search_hit, DZL_TYPE_SUGGESTION)

static void
gcal_search_hit_default_init (GcalSearchHitInterface *iface)
{
}

void
gcal_search_hit_activate (GcalSearchHit *self,
                          GtkWidget     *for_widget)
{
  g_return_if_fail (GCAL_IS_SEARCH_HIT (self));
  g_return_if_fail (GCAL_SEARCH_HIT_GET_IFACE (self)->activate);

  GCAL_SEARCH_HIT_GET_IFACE (self)->activate (self, for_widget);
}

gint
gcal_search_hit_get_priority (GcalSearchHit *self)
{
  g_return_val_if_fail (GCAL_IS_SEARCH_HIT (self), 0);
  g_return_val_if_fail (GCAL_SEARCH_HIT_GET_IFACE (self)->get_priority, 0);

  return GCAL_SEARCH_HIT_GET_IFACE (self)->get_priority (self);
}

gint
gcal_search_hit_compare (GcalSearchHit *a,
                         GcalSearchHit *b)
{
  g_return_val_if_fail (GCAL_IS_SEARCH_HIT (a), 0);
  g_return_val_if_fail (GCAL_SEARCH_HIT_GET_IFACE (a)->compare, 0);

  return GCAL_SEARCH_HIT_GET_IFACE (a)->compare (a, b);
}
