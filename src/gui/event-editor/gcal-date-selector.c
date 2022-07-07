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
  AdwEntryRow  parent;

  /* widgets */
  GtkWidget   *date_chooser;
  GtkWidget   *date_selector_popover;

  GSettings   *settings;
};

G_DEFINE_TYPE (GcalDateSelector, gcal_date_selector, ADW_TYPE_ENTRY_ROW);

enum
{
  PROP_0,
  PROP_DATE,
  N_PROPS
};

static GParamSpec* properties[N_PROPS] = { NULL, };

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
  g_clear_object (&self->settings);

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
gcal_date_selector_activate (GcalDateSelector *self)
{
  parse_date (GCAL_DATE_SELECTOR (self));
}

static void
gcal_date_selector_class_init (GcalDateSelectorClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_ensure (GCAL_TYPE_DATE_CHOOSER);

  object_class->dispose = gcal_date_selector_dispose;
  object_class->get_property = gcal_date_selector_get_property;
  object_class->set_property = gcal_date_selector_set_property;

  widget_class->focus = gcal_date_selector_focus;
  widget_class->size_allocate = gcal_date_selector_size_allocate;

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
  gtk_widget_class_bind_template_callback (widget_class, on_contains_focus_changed_cb);
}

static void
gcal_date_selector_init (GcalDateSelector *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->settings = g_settings_new ("org.gnome.desktop.calendar");

  g_settings_bind (self->settings,
                   "show-weekdate",
                   self->date_chooser,
                   "show-week-numbers",
                   G_SETTINGS_BIND_DEFAULT);

  g_signal_connect_swapped (gtk_editable_get_delegate (GTK_EDITABLE (self)),
                            "activate", G_CALLBACK (gcal_date_selector_activate),
                            self);
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
