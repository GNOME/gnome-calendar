/* gcal-calendar-combo-row.c
 *
 * Copyright (C) 2024 Titouan Real <titouan.real@gmail.com>
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

#define G_LOG_DOMAIN "GcalCalendarComboRow"

#include "gcal-calendar-combo-row.h"
#include "gcal-calendar-combo-row-item.h"

#include <locale.h>
#include <langinfo.h>
#include <stdlib.h>
#include <glib/gi18n.h>

struct _GcalCalendarComboRow
{
  AdwComboRow         parent;
};

G_DEFINE_TYPE (GcalCalendarComboRow, gcal_calendar_combo_row, ADW_TYPE_COMBO_ROW);

enum
{
  PROP_0,
  PROP_CALENDAR,
  N_PROPS
};

static GParamSpec* properties[N_PROPS] = { NULL, };


/*
 * Callbacks
 */

static void
on_calendar_selected_cb (AdwComboRow *row,
                         GParamSpec  *pspec,
                         GtkListItem *item)
{
  GtkWidget *checkmark = g_object_get_data (G_OBJECT (item), "checkmark");

  if (adw_combo_row_get_selected_item (row) == gtk_list_item_get_item (item))
    gtk_widget_set_opacity (checkmark, 1);
  else
    gtk_widget_set_opacity (checkmark, 0);
}

static void
calendar_item_setup_cb (GtkSignalListItemFactory *factory,
                        GtkListItem              *item)
{
  GtkWidget *box, *checkmark;

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

  checkmark = g_object_new (GTK_TYPE_IMAGE,
                            "accessible-role", GTK_ACCESSIBLE_ROLE_PRESENTATION,
                            "icon-name", "object-select-symbolic",
                            NULL);
  gtk_box_append (GTK_BOX (box), checkmark);

  g_object_set_data (G_OBJECT (item), "box", box);
  g_object_set_data (G_OBJECT (item), "checkmark", checkmark);

  gtk_list_item_set_child (item, box);
}

static void
calendar_item_bind_cb (GtkSignalListItemFactory *factory,
                       GtkListItem              *item,
                       GcalCalendarComboRow     *self)
{
  AdwComboRow *row = ADW_COMBO_ROW (self);
  GtkWidget *box, *calendar_row, *checkmark;
  GcalCalendar *data;
  GtkWidget *popup;

  data = gtk_list_item_get_item (item);

  box = g_object_get_data (G_OBJECT (item), "box");
  checkmark = g_object_get_data (G_OBJECT (item), "checkmark");

  calendar_row = gcal_calendar_combo_row_item_new (data);

  gtk_box_insert_child_after (GTK_BOX (box), calendar_row, NULL);

  popup = gtk_widget_get_ancestor (box, GTK_TYPE_POPOVER);
  if (popup && gtk_widget_is_ancestor (popup, GTK_WIDGET (row)))
    {
      gtk_box_set_spacing (GTK_BOX (box), 0);
      gtk_widget_set_visible (checkmark, TRUE);
      g_signal_connect (row, "notify::selected-item", G_CALLBACK (on_calendar_selected_cb), item);
      on_calendar_selected_cb (row, NULL, item);
    }
  else
    {
      gtk_box_set_spacing (GTK_BOX (box), 6);
      gtk_widget_set_visible (checkmark, FALSE);
    }

}

static void
calendar_item_unbind_cb (GtkSignalListItemFactory *factory,
                         GtkListItem              *item,
                         GcalCalendarComboRow     *self)
{
  AdwComboRow *row = ADW_COMBO_ROW (self);

  g_signal_handlers_disconnect_by_func (row, on_calendar_selected_cb, item);
}


/*
 * GObject overrides
 */

static void
gcal_calendar_combo_row_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  GcalCalendarComboRow *self = GCAL_CALENDAR_COMBO_ROW (object);

  switch (prop_id)
    {
    case PROP_CALENDAR:
      g_value_set_object (value, GCAL_CALENDAR (adw_combo_row_get_selected_item (ADW_COMBO_ROW (self))));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_calendar_combo_row_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  GcalCalendarComboRow *self = GCAL_CALENDAR_COMBO_ROW (object);

  switch (prop_id)
    {
    case PROP_CALENDAR:
      gcal_calendar_combo_row_set_calendar (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_calendar_combo_row_class_init (GcalCalendarComboRowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = gcal_calendar_combo_row_get_property;
  object_class->set_property = gcal_calendar_combo_row_set_property;

  /**
   * GcalCalendarComboRow::calendar:
   *
   * The selected calendar of the combo row.
   */
  properties[PROP_CALENDAR] = g_param_spec_object ("calendar",
                                                   "Calendar of the combo row",
                                                   "The selected calendar from the combo row",
                                                   GCAL_TYPE_CALENDAR,
                                                   G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/common/gcal-calendar-combo-row.ui");

  gtk_widget_class_bind_template_callback (widget_class, calendar_item_setup_cb);
  gtk_widget_class_bind_template_callback (widget_class, calendar_item_bind_cb);
  gtk_widget_class_bind_template_callback (widget_class, calendar_item_unbind_cb);
}

static void
gcal_calendar_combo_row_init (GcalCalendarComboRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

/**
 * gcal_calendar_combo_row_new:
 *
 * Creates a new `CalendarComboRow`.
 *
 * Returns: the newly created `CalendarComboRow`
 */
GtkWidget*
gcal_calendar_combo_row_new (void)
{
  return g_object_new (GCAL_TYPE_CALENDAR_COMBO_ROW, NULL);
}

/**
 * gcal_calendar_combo_row_set_calendar:
 * @self: a #GcalCalendarComboRow
 * @calendar: a valid #GcalCalendar
 *
 * Sets the selected calendar.
 */
void
gcal_calendar_combo_row_set_calendar (GcalCalendarComboRow *self,
                                      GcalCalendar         *calendar)
{
  GListModel *model;

  g_return_if_fail (GCAL_IS_CALENDAR_COMBO_ROW (self));

  model = adw_combo_row_get_model (ADW_COMBO_ROW (self));

  if (model == NULL)
    return;

  for (guint i = 0; i < g_list_model_get_n_items (model); i++)
    {
      g_autoptr (GcalCalendar) aux = g_list_model_get_item (model, i);
      if (aux == calendar)
        {
          adw_combo_row_set_selected (ADW_COMBO_ROW (self), i);
          break;
        }
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CALENDAR]);
}

/**
 * gcal_calendar_combo_row_get_calendar:
 * @self: a #GcalCalendarComboRow
 *
 * Gets the selected calendar.
 *
 * Returns: (nullable) (transfer none): The selected calendar
 */
GcalCalendar*
gcal_calendar_combo_row_get_calendar (GcalCalendarComboRow *self)
{
  g_return_val_if_fail (GCAL_IS_CALENDAR_COMBO_ROW (self), NULL);

  return adw_combo_row_get_selected_item (ADW_COMBO_ROW (self));
}
