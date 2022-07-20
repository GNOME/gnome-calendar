/* gcal-toolbar-end.c
 *
 * Copyright 2022 Christopher Davis <christopherdavis@gnome.org>
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

#include "gcal-toolbar-end.h"

#include "gcal-search-button.h"

struct _GcalToolbarEnd
{
  AdwBin parent_instance;
};

G_DEFINE_FINAL_TYPE (GcalToolbarEnd, gcal_toolbar_end, ADW_TYPE_BIN)

static void
gcal_toolbar_end_class_init (GcalToolbarEndClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/calendar/ui/gui/gcal-toolbar-end.ui");
}

static void
gcal_toolbar_end_init (GcalToolbarEnd *self)
{
  g_type_ensure (GCAL_TYPE_SEARCH_BUTTON);

  gtk_widget_init_template (GTK_WIDGET (self));
}
