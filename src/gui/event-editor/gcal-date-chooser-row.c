/* gcal-date-chooser-row.c
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

#define G_LOG_DOMAIN "GcalDateChooserRow"

#include "gcal-date-chooser-row.h"
#include "gcal-date-chooser.h"
#include "gcal-view.h"

#include <locale.h>
#include <langinfo.h>
#include <stdlib.h>
#include <glib/gi18n.h>
#include "gcal-utils.h"

struct _GcalDateChooserRow
{
  AdwEntryRow      parent;

  /* widgets */
  GcalDateChooser *date_chooser;

  GDBusProxy      *settings_portal;
};

G_DEFINE_TYPE (GcalDateChooserRow, gcal_date_chooser_row, ADW_TYPE_ENTRY_ROW);

enum
{
  PROP_0,
  PROP_DATE,
  N_PROPS
};

static GParamSpec* properties[N_PROPS] = { NULL, };


static void
update_entry (GcalDateChooserRow *self)
{
  g_autofree gchar *formatted_date = NULL;
  GDateTime *date;

  date = gcal_date_chooser_row_get_date (self);
  formatted_date = g_date_time_format (date, "%x");

  gtk_editable_set_text (GTK_EDITABLE (self), formatted_date);
}

static void
parse_date (GcalDateChooserRow *self)
{
  g_autoptr (GDateTime) new_date = NULL;
  GDate parsed_date;

  g_date_clear (&parsed_date, 1);
  g_date_set_parse (&parsed_date, gtk_editable_get_text (GTK_EDITABLE (self)));

  if (!g_date_valid (&parsed_date))
    {
      update_entry (self);
      return;
    }

  new_date = g_date_time_new_local (g_date_get_year (&parsed_date),
                                    g_date_get_month (&parsed_date),
                                    g_date_get_day (&parsed_date),
                                    0, 0, 0);

  gcal_date_chooser_row_set_date (self, new_date);
}

static void
set_show_weekdate_from_variant (GcalDateChooserRow *self,
                                GVariant           *variant)
{
  g_assert (g_variant_type_equal (g_variant_get_type (variant), "b"));

  gcal_date_chooser_set_show_week_numbers (GCAL_DATE_CHOOSER (self->date_chooser),
                                           g_variant_get_boolean (variant));
}

static gboolean
read_show_weekdate (GcalDateChooserRow *self)
{
  g_autoptr (GVariant) other_child = NULL;
  g_autoptr (GVariant) child = NULL;
  g_autoptr (GVariant) ret = NULL;
  g_autoptr (GError) error = NULL;

  ret = g_dbus_proxy_call_sync (self->settings_portal,
                                "Read",
                                g_variant_new ("(ss)", "org.gnome.desktop.calendar", "show-weekdate"),
                                G_DBUS_CALL_FLAGS_NONE,
                                G_MAXINT,
                                NULL,
                                &error);

  if (error)
    return FALSE;

  g_variant_get (ret, "(v)", &child);
  g_variant_get (child, "v", &other_child);

  set_show_weekdate_from_variant (self, other_child);

  return TRUE;
}


/*
 * Callbacks
 */

static void
on_contains_focus_changed_cb (GtkEventControllerFocus *focus_controller,
                              GParamSpec              *pspec,
                              GcalDateChooserRow      *self)
{
 parse_date (self);
}

static void
on_date_selected_changed_cb (GcalDateChooser    *selector,
                             GcalDateChooserRow *self)
{
  update_entry (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DATE]);
}

static void
on_date_popover_shown_cb (GtkPopover         *popover,
                          GcalDateChooserRow *self)
{
  parse_date (self);
}

static void
on_portal_proxy_signal_cb (GDBusProxy         *proxy,
                           const char         *sender_name,
                           const char         *signal_name,
                           GVariant           *parameters,
                           GcalDateChooserRow *self)
{
  g_autoptr (GVariant) value = NULL;
  const char *namespace;
  const char *name;

  if (g_strcmp0 (signal_name, "SettingChanged") != 0)
    return;

  g_variant_get (parameters, "(&s&sv)", &namespace, &name, &value);

  if (g_strcmp0 (namespace, "org.gnome.desktop.calendar") == 0 &&
      g_strcmp0 (name, "show-weekdate") == 0)
    {
      set_show_weekdate_from_variant (self, value);
    }
}


/*
 * GObject overrides
 */

static void
gcal_date_chooser_row_dispose (GObject *object)
{
  GcalDateChooserRow *self = GCAL_DATE_CHOOSER_ROW (object);

  g_clear_object (&self->settings_portal);

  G_OBJECT_CLASS (gcal_date_chooser_row_parent_class)->dispose (object);
}

static void
gcal_date_chooser_row_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  GcalDateChooserRow *self = GCAL_DATE_CHOOSER_ROW (object);

  switch (prop_id)
    {
    case PROP_DATE:
      g_value_set_boxed (value, gcal_date_chooser_row_get_date(self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_date_chooser_row_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  GcalDateChooserRow *self = GCAL_DATE_CHOOSER_ROW (object);

  switch (prop_id)
    {
    case PROP_DATE:
      gcal_date_chooser_row_set_date (self, g_value_get_boxed (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_date_chooser_row_class_init (GcalDateChooserRowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gcal_date_chooser_row_dispose;
  object_class->get_property = gcal_date_chooser_row_get_property;
  object_class->set_property = gcal_date_chooser_row_set_property;

  /**
   * GcalDateChooserRow::date:
   *
   * The current date of the selector.
   */
  properties[PROP_DATE] = g_param_spec_boxed ("date",
                                              "Date of the chooser row",
                                              "The current date of the chooser row",
                                              G_TYPE_DATE_TIME,
                                              G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/event-editor/gcal-date-chooser-row.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalDateChooserRow, date_chooser);

  gtk_widget_class_bind_template_callback (widget_class, on_contains_focus_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_date_selected_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_date_popover_shown_cb);
}

static void
gcal_date_chooser_row_init (GcalDateChooserRow *self)
{
  g_autoptr (GError) error = NULL;

  gtk_widget_init_template (GTK_WIDGET (self));

  self->settings_portal = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                         G_DBUS_PROXY_FLAGS_NONE,
                                                         NULL,
                                                         "org.freedesktop.portal.Desktop",
                                                         "/org/freedesktop/portal/desktop",
                                                         "org.freedesktop.portal.Settings",
                                                         NULL,
                                                         &error);

  if (error)
    g_error ("Failed to load portals: %s. Aborting...", error->message);

  if (read_show_weekdate (self))
    {
      g_signal_connect (self->settings_portal,
                        "g-signal",
                        G_CALLBACK (on_portal_proxy_signal_cb),
                        self);
    }
}

/**
 * gcal_date_chooser_row_set_date:
 * @self: a #GcalDateChooserRow
 * @date: a valid #GDateTime
 *
 * Set the value of the shown date.
 */
void
gcal_date_chooser_row_set_date (GcalDateChooserRow *self,
                                GDateTime          *date)
{
  g_autoptr (GDateTime) date_utc = NULL;

  g_return_if_fail (GCAL_IS_DATE_CHOOSER_ROW (self));

  date_utc = g_date_time_new_utc (g_date_time_get_year (date),
                                  g_date_time_get_month (date),
                                  g_date_time_get_day_of_month (date),
                                  0, 0, 0);

  gcal_view_set_date (GCAL_VIEW (self->date_chooser), date_utc);

  update_entry (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DATE]);
}

/**
 * gcal_date_chooser_row_get_date:
 * @self: a #GcalDateChooserRow
 *
 * Get the value of the date shown
 *
 * Returns: (transfer none): the date of the selector.
 */
GDateTime*
gcal_date_chooser_row_get_date (GcalDateChooserRow *self)
{
  g_return_val_if_fail (GCAL_IS_DATE_CHOOSER_ROW (self), NULL);

  return gcal_view_get_date (GCAL_VIEW (self->date_chooser));
}
