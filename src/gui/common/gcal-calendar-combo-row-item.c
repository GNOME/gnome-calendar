/* gcal-calendar-combo-row-item.c
 *
 * Copyright 2024 Diego Iván M.E <diegoivan.mae@gmail.com>
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

#define G_LOG_DOMAIN "GcalCalendarComboRowItem"

#include "config.h"
#include "gcal-calendar-combo-row-item.h"
#include "gcal-debug.h"
#include "gcal-utils.h"

#include <glib/gi18n.h>

struct _GcalCalendarComboRowItem
{
  AdwActionRow        parent;

  GtkImage           *color_image;
  GtkImage           *visibility_image;
  GcalCalendar       *calendar;
};

G_DEFINE_TYPE (GcalCalendarComboRowItem, gcal_calendar_combo_row_item, ADW_TYPE_PREFERENCES_ROW)

enum
{
  PROP_0,
  PROP_CALENDAR,
  N_PROPS,
};

static GParamSpec *properties [N_PROPS] = { NULL, };

static gboolean
paintable_from_gdk_rgb (GBinding     *binding,
                        const GValue *from_value,
                        GValue       *to_value,
                        gpointer      user_data)
{
  GdkRGBA *rgba = g_value_get_boxed (from_value);
  g_value_take_object (to_value, get_circle_paintable_from_color (rgba, 16));
  return TRUE;
}

static void
updated_visibility_cb (GcalCalendarComboRowItem *self)
{
  g_autofree gchar *accessible_description = NULL;

  GCAL_ENTRY;
  gtk_widget_set_visible (GTK_WIDGET (self->visibility_image), !gcal_calendar_get_visible (self->calendar));

  if (!gcal_calendar_get_visible (self->calendar))
    accessible_description = g_strdup (_("This calendar is hidden"));

  gtk_accessible_update_property (GTK_ACCESSIBLE (self),
                                  GTK_ACCESSIBLE_PROPERTY_DESCRIPTION, accessible_description, -1);
  GCAL_EXIT;
}

static void
set_calendar (GcalCalendarComboRowItem *self,
              GcalCalendar             *calendar)
{
  g_return_if_fail (GCAL_IS_CALENDAR (calendar));
  g_set_object (&self->calendar, calendar);

  g_object_bind_property_full (self->calendar, "color",
                               self->color_image, "paintable",
                               G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE,
                               paintable_from_gdk_rgb,
                               NULL,
                               NULL,
                               NULL);
  g_signal_connect_object (self->calendar,
                           "notify::visible", (GCallback) updated_visibility_cb,
                           self, G_CONNECT_SWAPPED);

  g_object_bind_property (calendar, "name", self, "title", G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

  updated_visibility_cb (self);
}

/*
 * GObject overrides
 */

static void
gcal_calendar_combo_row_item_finalize (GObject *object)
{
  GcalCalendarComboRowItem *self = (GcalCalendarComboRowItem *)object;

  g_clear_object (&self->calendar);

  G_OBJECT_CLASS (gcal_calendar_combo_row_item_parent_class)->finalize (object);
}

static void
gcal_calendar_combo_row_item_get_property (GObject    *object,
                                           guint       prop_id,
                                           GValue     *value,
                                           GParamSpec *pspec)
{
  GcalCalendarComboRowItem *self = GCAL_CALENDAR_COMBO_ROW_ITEM (object);

  switch (prop_id)
    {
    case PROP_CALENDAR:
      g_value_set_object (value, self->calendar);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_calendar_combo_row_item_set_property (GObject      *object,
                                           guint         prop_id,
                                           const GValue *value,
                                           GParamSpec   *pspec)
{
  GcalCalendarComboRowItem *self = GCAL_CALENDAR_COMBO_ROW_ITEM (object);

  switch (prop_id)
    {
    case PROP_CALENDAR:
      g_assert (self->calendar == NULL);
      set_calendar (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_calendar_combo_row_item_class_init (GcalCalendarComboRowItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gcal_calendar_combo_row_item_finalize;
  object_class->get_property = gcal_calendar_combo_row_item_get_property;
  object_class->set_property = gcal_calendar_combo_row_item_set_property;

  properties[PROP_CALENDAR] = g_param_spec_object ("calendar",
                                                   "Calendar",
                                                   "Calendar",
                                                   GCAL_TYPE_CALENDAR,
                                                   G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/common/gcal-calendar-combo-row-item.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalCalendarComboRowItem, color_image);
  gtk_widget_class_bind_template_child (widget_class, GcalCalendarComboRowItem, visibility_image);
}

static void
gcal_calendar_combo_row_item_init (GcalCalendarComboRowItem *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget*
gcal_calendar_combo_row_item_new (GcalCalendar *calendar)
{
  return g_object_new (GCAL_TYPE_CALENDAR_COMBO_ROW_ITEM,
                       "calendar", calendar,
                       NULL);
}
