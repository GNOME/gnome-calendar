/*
 * gcal-agenda-view-day-row.c
 *
 * Copyright 2026 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#include "gcal-agenda-view-day-row.h"
#include "gcal-date-time-utils.h"
#include "gcal-debug.h"
#include "gcal-enums.h"
#include "gcal-event-widget.h"

#include <glib/gi18n.h>

struct _GcalAgendaViewDayRow
{
  AdwBin             parent_instance;

  GtkListBox        *listbox;
  GtkLabel          *title_label;

  GcalAgendaViewDay *day;
};

G_DEFINE_FINAL_TYPE (GcalAgendaViewDayRow, gcal_agenda_view_day_row, ADW_TYPE_BIN)

enum {
  PROP_0,
  PROP_DAY,
  N_PROPS,
};

enum
{
  EVENT_ACTIVATED,
  N_SIGNALS,
};

static GParamSpec *properties [N_PROPS];
static guint signals[N_SIGNALS] = { 0, };


/*
 * Auxiliary methods
 */

static gchar *
new_date_header_string (GDateTime *date)
{
  g_autoptr (GDateTime) today = NULL;
  g_autoptr (GDateTime) tomorrow = NULL;
  g_autoptr (GDateTime) yesterday = NULL;

  if (date == NULL)
    return NULL;

  today = g_date_time_new_now_local ();
  tomorrow = g_date_time_add_days (today, 1);
  yesterday = g_date_time_add_days (today, -1);

  if (gcal_date_time_compare_date (date, today) == 0)
    return g_strdup (_("Today"));
  else if (gcal_date_time_compare_date (date, tomorrow) == 0)
    return g_strdup (_("Tomorrow"));
  else if (gcal_date_time_compare_date (date, yesterday) == 0)
    return g_strdup (_("Yesterday"));
  else
    /*
     * Translators: %A is the full day name, %B is the month name
     * and %d is the day of the month as a number between 0 and 31.
     * More formats can be found on the doc:
     * https://docs.gtk.org/glib/method.DateTime.format.html
     */
    return g_date_time_format (date, _("%A %B %d"));
}


/*
 * Callbacks
 */

static void
on_event_widget_activated_cb (GcalEventWidget      *event_widget,
                              GcalAgendaViewDayRow *self)
{
  g_assert (GCAL_IS_AGENDA_VIEW_DAY_ROW (self));
  g_assert (GCAL_IS_EVENT_WIDGET (event_widget));

  g_signal_emit (self, signals[EVENT_ACTIVATED], 0, event_widget);
}

static void
on_list_box_row_activated_cb (GtkListBox           *listbox,
                              GtkListBoxRow        *row,
                              GcalAgendaViewDayRow *self)
{
  GtkWidget *child = gtk_list_box_row_get_child (row);

  g_assert (GCAL_IS_AGENDA_VIEW_DAY_ROW (self));
  g_assert (GCAL_IS_EVENT_WIDGET (child));

  g_signal_emit (self, signals[EVENT_ACTIVATED], 0, child);
}

static GtkWidget *
create_day_row_func (gpointer item,
                     gpointer user_data)
{
  GcalAgendaViewDayRow *self;
  GcalTimestampPolicy timestamp_policy;
  GcalEvent *event;
  GtkWidget *widget;
  GtkWidget *row;

  self = (GcalAgendaViewDayRow *) user_data;

  g_assert (GCAL_IS_EVENT (item));
  g_assert (GCAL_IS_AGENDA_VIEW_DAY_ROW (self));

  event = item;

  /* Create and add the new event widget */
  if (gcal_event_get_all_day (event) || gcal_event_is_multiday (event))
    timestamp_policy = GCAL_TIMESTAMP_POLICY_END;
  else
    timestamp_policy = GCAL_TIMESTAMP_POLICY_START;

  widget = g_object_new (GCAL_TYPE_EVENT_WIDGET,
                         "event", event,
                         "orientation", GTK_ORIENTATION_VERTICAL,
                         "timestamp-policy", timestamp_policy,
                         "focusable", FALSE,
                         NULL);

  row = g_object_new (GTK_TYPE_LIST_BOX_ROW,
                      "child", widget,
                      "activatable", TRUE,
                      NULL);

  gtk_accessible_update_property (GTK_ACCESSIBLE (row),
                                  GTK_ACCESSIBLE_PROPERTY_HAS_POPUP, TRUE,
                                  -1);

  gtk_accessible_update_relation (GTK_ACCESSIBLE (row),
                                  GTK_ACCESSIBLE_RELATION_LABELLED_BY, widget, NULL,
                                  -1);

  g_signal_connect (widget, "activate", G_CALLBACK (on_event_widget_activated_cb), self);

  return row;
}

static char *
datetime_to_string_func (gpointer   item,
                         GDateTime *datetime,
                         gpointer   user_data)
{
  return datetime ? new_date_header_string (datetime) : g_strdup ("");
}


/*
 * GObject overrides
 */

static void
gcal_agenda_view_day_row_dispose (GObject *object)
{
  GcalAgendaViewDayRow *self = (GcalAgendaViewDayRow *)object;

  g_clear_object (&self->day);

  gtk_widget_dispose_template (GTK_WIDGET (self), GCAL_TYPE_AGENDA_VIEW_DAY_ROW);

  G_OBJECT_CLASS (gcal_agenda_view_day_row_parent_class)->dispose (object);
}

static void
gcal_agenda_view_day_row_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  GcalAgendaViewDayRow *self = GCAL_AGENDA_VIEW_DAY_ROW (object);

  switch (prop_id)
    {
    case PROP_DAY:
      g_value_set_object (value, self->day);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_agenda_view_day_row_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  GcalAgendaViewDayRow *self = GCAL_AGENDA_VIEW_DAY_ROW (object);

  switch (prop_id)
    {
    case PROP_DAY:
      gcal_agenda_view_day_row_set_day (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_agenda_view_day_row_class_init (GcalAgendaViewDayRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gcal_agenda_view_day_row_dispose;
  object_class->get_property = gcal_agenda_view_day_row_get_property;
  object_class->set_property = gcal_agenda_view_day_row_set_property;

  properties[PROP_DAY] = g_param_spec_object ("day", NULL, NULL,
                                              GCAL_TYPE_AGENDA_VIEW_DAY,
                                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals[EVENT_ACTIVATED] = g_signal_new ("event-activated",
                                           GCAL_TYPE_AGENDA_VIEW_DAY_ROW,
                                           G_SIGNAL_RUN_LAST,
                                           0, NULL, NULL, NULL,
                                           G_TYPE_NONE,
                                           1,
                                           GCAL_TYPE_EVENT_WIDGET);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/views/gcal-agenda-view-day-row.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalAgendaViewDayRow, listbox);
  gtk_widget_class_bind_template_child (widget_class, GcalAgendaViewDayRow, title_label);

  gtk_widget_class_bind_template_callback (widget_class, datetime_to_string_func);
  gtk_widget_class_bind_template_callback (widget_class, on_list_box_row_activated_cb);

  gtk_widget_class_set_css_name (widget_class, "agendaviewdayrow");
}

static void
gcal_agenda_view_day_row_init (GcalAgendaViewDayRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GcalAgendaViewDay *
gcal_agenda_view_day_row_get_day (GcalAgendaViewDayRow *self)
{
  g_assert (GCAL_IS_AGENDA_VIEW_DAY_ROW (self));

  return self->day;
}

void
gcal_agenda_view_day_row_set_day (GcalAgendaViewDayRow *self,
                                  GcalAgendaViewDay    *day)
{
  g_assert (GCAL_IS_AGENDA_VIEW_DAY_ROW (self));
  g_assert (day == NULL || GCAL_IS_AGENDA_VIEW_DAY (day));

  if (g_set_object (&self->day, day))
    {
      gtk_list_box_bind_model (GTK_LIST_BOX (self->listbox),
                               day ? G_LIST_MODEL (day) : NULL,
                               day ? create_day_row_func : NULL,
                               day ? self : NULL,
                               NULL);

      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DAY]);
    }
}
