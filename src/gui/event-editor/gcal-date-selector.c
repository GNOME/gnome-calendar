/* gcal-date-selector.c
 *
 * Copyright (C) 2014 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#define G_LOG_DOMAIN "GcalDateSelector"

#include "gcal-date-chooser.h"
#include "gcal-date-selector.h"
#include "gcal-view.h"

#include <locale.h>
#include <langinfo.h>
#include <stdlib.h>
#include <glib/gi18n.h>

struct _GcalDateSelector
{
  GtkEntry     parent;

  /* widgets */
  GtkWidget   *date_chooser;
  GtkWidget   *date_selector_popover;

  GDBusProxy  *settings_portal;
};

G_DEFINE_TYPE (GcalDateSelector, gcal_date_selector, GTK_TYPE_ENTRY);

enum
{
  PROP_0,
  PROP_DATE,
  N_PROPS
};

static GParamSpec* properties[N_PROPS] = { NULL, };


static void
set_show_weekdate_from_variant (GcalDateSelector *self,
                                GVariant         *variant)
{
  g_assert (g_variant_type_equal (g_variant_get_type (variant), "b"));

  gcal_date_chooser_set_show_week_numbers (GCAL_DATE_CHOOSER (self->date_chooser),
                                           g_variant_get_boolean (variant));
}

static gboolean
read_show_weekdate (GcalDateSelector *self)
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
    {
      return FALSE;
    }

  g_variant_get (ret, "(v)", &child);
  g_variant_get (child, "v", &other_child);

  set_show_weekdate_from_variant (self, other_child);

  return TRUE;
}

static void
update_text (GcalDateSelector *self)
{
  GDateTime *date;
  gchar *label;

  date = gcal_view_get_date (GCAL_VIEW (self->date_chooser));

  /* rebuild the date label */
  label = g_date_time_format (date, "%x");

  gtk_editable_set_text (GTK_EDITABLE (self), label);
  g_free (label);
}

static void
calendar_day_selected (GcalDateSelector *self)
{
  update_text (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DATE]);
}

static void
parse_date (GcalDateSelector *self)
{
  GDateTime *new_date;
  GDate parsed_date;

  g_date_clear (&parsed_date, 1);
  g_date_set_parse (&parsed_date, gtk_editable_get_text (GTK_EDITABLE (self)));

  if (!g_date_valid (&parsed_date))
    {
      update_text (self);
      return;
    }

  new_date = g_date_time_new_local (g_date_get_year (&parsed_date),
                                    g_date_get_month (&parsed_date),
                                    g_date_get_day (&parsed_date),
                                    0, 0, 0);

  gcal_date_selector_set_date (self, new_date);

  g_clear_pointer (&new_date, g_date_time_unref);
}

static void
on_portal_proxy_signal_cb (GDBusProxy       *proxy,
                           const gchar      *sender_name,
                           const gchar      *signal_name,
                           GVariant         *parameters,
                           GcalDateSelector *self)
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

static void
icon_pressed_cb (GcalDateSelector     *self,
                 GtkEntryIconPosition  position,
                 GdkEvent             *event)
{
  GdkRectangle icon_bounds;

  gtk_entry_get_icon_area (GTK_ENTRY (self), position, &icon_bounds);

  gtk_popover_set_pointing_to (GTK_POPOVER (self->date_selector_popover), &icon_bounds);

  gtk_popover_popup (GTK_POPOVER (self->date_selector_popover));
}

static void
on_contains_focus_changed_cb (GtkEventControllerFocus *focus_controller,
                              GParamSpec              *pspec,
                              GcalDateSelector        *self)
{
  parse_date (self);
}

/*
 * GtkWidget overrides
 */

static gboolean
gcal_date_selector_focus (GtkWidget        *widget,
                          GtkDirectionType  direction)
{
  GcalDateSelector *self= GCAL_DATE_SELECTOR (widget);

  if (gtk_widget_get_visible (self->date_selector_popover))
    return gtk_widget_child_focus (self->date_selector_popover, direction);
  else
    return GTK_WIDGET_CLASS (gcal_date_selector_parent_class)->focus (widget, direction);
}

static void
gcal_date_selector_size_allocate (GtkWidget *widget,
                                  gint       width,
                                  gint       height,
                                  gint       baseline)
{
  GcalDateSelector *self= GCAL_DATE_SELECTOR (widget);

  GTK_WIDGET_CLASS (gcal_date_selector_parent_class)->size_allocate (widget,
                                                                     width,
                                                                     height,
                                                                     baseline);

  gtk_popover_present (GTK_POPOVER (self->date_selector_popover));
}

/*
 * GObject overrides
 */

static void
gcal_date_selector_dispose (GObject *object)
{
  GcalDateSelector *self = GCAL_DATE_SELECTOR (object);

  g_clear_pointer (&self->date_selector_popover, gtk_widget_unparent);
  g_clear_object (&self->settings_portal);

  G_OBJECT_CLASS (gcal_date_selector_parent_class)->dispose (object);
}

static void
gcal_date_selector_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GcalDateSelector *self = (GcalDateSelector*) object;

  switch (prop_id)
    {
    case PROP_DATE:
      g_value_set_boxed (value, gcal_view_get_date (GCAL_VIEW (self->date_chooser)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_date_selector_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GcalDateSelector *self = (GcalDateSelector*) object;

  switch (prop_id)
    {
    case PROP_DATE:
      gcal_date_selector_set_date (self, g_value_get_boxed (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_date_selector_activate (GtkEntry *entry)
{
  parse_date (GCAL_DATE_SELECTOR (entry));
}

static void
gcal_date_selector_class_init (GcalDateSelectorClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkEntryClass *entry_class = GTK_ENTRY_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_ensure (GCAL_TYPE_DATE_CHOOSER);

  object_class->dispose = gcal_date_selector_dispose;
  object_class->get_property = gcal_date_selector_get_property;
  object_class->set_property = gcal_date_selector_set_property;

  widget_class->focus = gcal_date_selector_focus;
  widget_class->size_allocate = gcal_date_selector_size_allocate;

  entry_class->activate = gcal_date_selector_activate;

  /**
   * GcalDateSelector::date:
   *
   * The current date of the selector.
   */
  properties[PROP_DATE] = g_param_spec_boxed ("date",
                                              "Date of the selector",
                                              "The current date of the selector",
                                              G_TYPE_DATE_TIME,
                                              G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/event-editor/gcal-date-selector.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalDateSelector, date_chooser);
  gtk_widget_class_bind_template_child (widget_class, GcalDateSelector, date_selector_popover);

  gtk_widget_class_bind_template_callback (widget_class, calendar_day_selected);
  gtk_widget_class_bind_template_callback (widget_class, icon_pressed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_contains_focus_changed_cb);
}

static void
gcal_date_selector_init (GcalDateSelector *self)
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

/* Public API */
GtkWidget*
gcal_date_selector_new (void)
{
  return g_object_new (GCAL_TYPE_DATE_SELECTOR, NULL);
}

/**
 * gcal_date_selector_set_date:
 * @selector: a #GcalDateSelector
 * @date: a valid #GDateTime
 *
 * Set the value of the shown date.
 */
void
gcal_date_selector_set_date (GcalDateSelector *selector,
                             GDateTime        *date)
{
  g_return_if_fail (GCAL_IS_DATE_SELECTOR (selector));

  /* set calendar's date */
  gcal_view_set_date (GCAL_VIEW (selector->date_chooser), date);
  update_text (selector);

  /* emit the MODIFIED signal */
  g_object_notify_by_pspec (G_OBJECT (selector), properties[PROP_DATE]);
}

/**
 * gcal_date_selector_get_date:
 * @selector: a #GcalDateSelector
 *
 * Get the value of the date shown
 *
 * Returns: (transfer none): the date of the selector.
 */
GDateTime*
gcal_date_selector_get_date (GcalDateSelector *selector)
{
  g_return_val_if_fail (GCAL_IS_DATE_SELECTOR (selector), NULL);

  return gcal_view_get_date (GCAL_VIEW (selector->date_chooser));
}
