/* gcal-week-hour-bar.c
 *
 * Copyright 2022 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#include "gcal-week-hour-bar.h"
#include "gcal-week-view-common.h"

#include <glib/gi18n.h>

struct _GcalWeekHourBar
{
  GtkBox              parent_instance;

  GtkLabel           *labels[24];

  GcalContext        *context;
};

G_DEFINE_FINAL_TYPE (GcalWeekHourBar, gcal_week_hour_bar, GTK_TYPE_BOX)

static void
update_labels (GcalWeekHourBar *self)
{
  GcalTimeFormat time_format;
  gint i;

  time_format = gcal_context_get_time_format (self->context);

  for (i = 0; i < 24; i++)
    {
      g_autofree gchar *hours = NULL;

      if (time_format == GCAL_TIME_FORMAT_24H)
        {
          hours = g_strdup_printf ("%02d:00", i);
        }
      else
        {
          hours = g_strdup_printf ("%d %s",
                                   i % 12 == 0 ? 12 : i % 12,
                                   i >= 12 ? _("PM") : _("AM"));
        }

      gtk_label_set_label (self->labels[i], hours);
    }
}

static void
gcal_week_hour_bar_snapshot (GtkWidget   *widget,
                             GtkSnapshot *snapshot)
{
  gcal_week_view_common_snapshot_hour_lines (widget,
                                             snapshot,
                                             GTK_ORIENTATION_VERTICAL,
                                             gtk_widget_get_width (widget),
                                             gtk_widget_get_height (widget));

  for (GtkWidget *child = gtk_widget_get_first_child (widget);
       child;
       child = gtk_widget_get_next_sibling (child))
    {
      gtk_widget_snapshot_child (widget, child, snapshot);
    }
}

static void
gcal_week_hour_bar_class_init (GcalWeekHourBarClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->snapshot = gcal_week_hour_bar_snapshot;

  gtk_widget_class_set_css_name (widget_class, "weekhourbar");
}

static void
gcal_week_hour_bar_init (GcalWeekHourBar *self)
{
  gint i;

  g_object_set (self,
                "orientation", GTK_ORIENTATION_VERTICAL,
                "homogeneous", TRUE,
                "spacing", 1,
                NULL);


  for (i = 0; i < 24; i++)
    {
      GtkWidget *label = gtk_label_new ("");

      gtk_widget_add_css_class (label, "line");
      gtk_widget_add_css_class (label, "dimmed");
      gtk_widget_set_vexpand (label, TRUE);
      gtk_label_set_yalign (GTK_LABEL (label), 0.0);
      gtk_box_append (GTK_BOX (self), label);

      self->labels[i] = GTK_LABEL (label);
    }
}

void
gcal_week_hour_bar_set_context (GcalWeekHourBar *self,
                                GcalContext     *context)
{
  g_return_if_fail (GCAL_IS_WEEK_HOUR_BAR (self));

  self->context = context;

  g_signal_connect_object (context,
                           "notify::time-format",
                           G_CALLBACK (update_labels),
                           self,
                           G_CONNECT_SWAPPED);
  update_labels (self);
}
