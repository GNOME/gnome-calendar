/* gcal-overflow-bin-layout.c
 *
 * Copyright (C) 2022 Purism SPC
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * GcalOverflowBinLayout:
 *
 * `GcalOverflowBinLayout` is a `GtkLayoutManager` subclass useful for create "bins" of
 * widgets.
 *
 * `GcalOverflowBinLayout` will stack each child of a widget on top of each other,
 * using the [property@Gtk.Widget:hexpand], [property@Gtk.Widget:vexpand],
 * [property@Gtk.Widget:halign], and [property@Gtk.Widget:valign] properties
 * of each child to determine where they should be positioned.
 */

#include "gcal-overflow-bin-layout.h"

struct _GcalOverflowBinLayout
{
  GtkLayoutManager parent_instance;

  GtkSizeRequestMode request_mode;
};

G_DEFINE_TYPE (GcalOverflowBinLayout, gcal_overflow_bin_layout, GTK_TYPE_LAYOUT_MANAGER)

/*
 * GtkLayoutManager overrides
 */

static void
gcal_overflow_bin_layout_measure (GtkLayoutManager *layout_manager,
                                  GtkWidget        *widget,
                                  GtkOrientation    orientation,
                                  gint              for_size,
                                  gint             *minimum,
                                  gint             *natural,
                                  gint             *minimum_baseline,
                                  gint             *natural_baseline)
{
  GtkSizeRequestMode request_mode;
  GtkWidget *child;
  gboolean clamp;

  request_mode = gtk_widget_get_request_mode (widget);
  switch (request_mode)
    {
    case GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH:
      clamp = orientation == GTK_ORIENTATION_VERTICAL;
      break;
    case GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT:
      clamp = orientation == GTK_ORIENTATION_HORIZONTAL;
      break;
    case GTK_SIZE_REQUEST_CONSTANT_SIZE:
      clamp = TRUE;
      break;
    }

  for (child = gtk_widget_get_first_child (widget);
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      gint child_min = 0;
      gint child_nat = 0;

      if (!gtk_widget_should_layout (child))
        continue;

      if (clamp || for_size == -1)
        {
          gtk_widget_measure (child, orientation, for_size,
                              &child_min, &child_nat,
                              NULL, NULL);
        }
      else
        {
          gint child_min_opposite;
          GtkOrientation opposite_orientation = 1 - orientation;

          /* We'll be clamping the child's size in the opposite
           * orientation. The for_size value describes our size along
           * that opposite orientation, but the child's size could be
           * larger if we are to clamp it.
           *
           * As such:
           * - measure the child's *overall* mininmum size, since its
           *   size along the opposite orientation can be arbitrary
           *   (we'd clamp it if it was larger than for_size);
           * - try to measure the child's natural size at for_size, but
           *   not any less than its minimum opposite size.
           *
           * When for_size = -1, we take the other branch and just
           * measure the child' overall minimum and natural sizes
           * directly.
           */

          gtk_widget_measure (child, opposite_orientation, -1,
                              &child_min_opposite, NULL,
                              NULL, NULL);
          gtk_widget_measure (child, orientation, -1,
                              &child_min, NULL,
                              NULL, NULL);
          gtk_widget_measure (child, orientation, MAX (for_size, child_min_opposite),
                              NULL, &child_nat,
                              NULL, NULL);
        }

      *minimum = MAX (*minimum, child_min);
      *natural = MAX (*natural, child_nat);
    }

  if (clamp)
    *minimum = 0;
  *minimum_baseline = -1;
  *natural_baseline = -1;
}

static void
gcal_overflow_bin_layout_allocate (GtkLayoutManager *layout_manager,
                                   GtkWidget        *widget,
                                   gint              width,
                                   gint              height,
                                   gint              baseline)
{
  GtkWidget *child;

  for (child = gtk_widget_get_first_child (widget);
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      GtkSizeRequestMode child_request_mode;
      gint child_min_height = 0;
      gint child_min_width = 0;

      if (!gtk_widget_should_layout (child))
        continue;

      child_request_mode = gtk_widget_get_request_mode (child);

      switch (child_request_mode)
        {
        case GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH:
          gtk_widget_measure (child,
                              GTK_ORIENTATION_HORIZONTAL,
                              -1,
                              &child_min_width, NULL,
                              NULL, NULL);
          width = MAX (width, child_min_width);

          gtk_widget_measure (child,
                              GTK_ORIENTATION_VERTICAL,
                              width,
                              &child_min_height, NULL,
                              NULL, NULL);
          height = MAX (height, child_min_height);
          break;

        case GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT:
          gtk_widget_measure (child,
                              GTK_ORIENTATION_VERTICAL,
                              -1,
                              &child_min_height, NULL,
                              NULL, NULL);
          height = MAX (height, child_min_height);

          gtk_widget_measure (child,
                              GTK_ORIENTATION_HORIZONTAL,
                              height,
                              &child_min_width, NULL,
                              NULL, NULL);
          width = MAX (width, child_min_width);
          break;

        case GTK_SIZE_REQUEST_CONSTANT_SIZE:
          gtk_widget_measure (child,
                              GTK_ORIENTATION_HORIZONTAL,
                              -1,
                              &child_min_width, NULL,
                              NULL, NULL);
          width = MAX (width, child_min_width);

          gtk_widget_measure (child,
                              GTK_ORIENTATION_VERTICAL,
                              -1,
                              &child_min_height, NULL,
                              NULL, NULL);
          height = MAX (height, child_min_height);
          break;
        }

      gtk_widget_allocate (child, width, height, -1, NULL);
    }
}

static GtkSizeRequestMode
gcal_overflow_bin_layout_get_request_mode (GtkLayoutManager *layout_manager,
                                           GtkWidget        *widget)
{
  GcalOverflowBinLayout *self = GCAL_OVERFLOW_BIN_LAYOUT (layout_manager);

  return self->request_mode;
}

static void
gcal_overflow_bin_layout_class_init (GcalOverflowBinLayoutClass *klass)
{
  GtkLayoutManagerClass *layout_manager_class = GTK_LAYOUT_MANAGER_CLASS (klass);

  layout_manager_class->measure = gcal_overflow_bin_layout_measure;
  layout_manager_class->allocate = gcal_overflow_bin_layout_allocate;
  layout_manager_class->get_request_mode = gcal_overflow_bin_layout_get_request_mode;
}

static void
gcal_overflow_bin_layout_init (GcalOverflowBinLayout *self)
{
  self->request_mode = GTK_SIZE_REQUEST_CONSTANT_SIZE;
}

void
gcal_overflow_bin_layout_set_request_mode (GcalOverflowBinLayout *self,
                                           GtkSizeRequestMode     request_mode)
{
  g_return_if_fail (GCAL_IS_OVERFLOW_BIN_LAYOUT (self));
  g_return_if_fail (request_mode >= GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH && request_mode <= GTK_SIZE_REQUEST_CONSTANT_SIZE);

  if (self->request_mode == request_mode)
    return;

  self->request_mode = request_mode;

  gtk_layout_manager_layout_changed (GTK_LAYOUT_MANAGER (self));
}
