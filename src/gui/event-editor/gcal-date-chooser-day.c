/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2015 Red Hat, Inc.
 * Copyright (C) 2022 Purism SPC
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#define G_LOG_DOMAIN "GcalDateChooserDay"

#include "config.h"

#include "gcal-date-chooser-day.h"
#include "gcal-date-time-utils.h"
#include "gcal-utils.h"

#include <stdlib.h>
#include <langinfo.h>

struct _GcalDateChooserDay
{
  GtkButton           parent;

  GtkWidget          *dot_revealer;
  GtkWidget          *label;
  GDateTime          *date;
};

enum {
  PROP_0,
  PROP_HAS_DOT,
  PROP_DOT_VISIBLE,
  NUM_PROPS
};

static GParamSpec *properties[NUM_PROPS] = { NULL, };

G_DEFINE_TYPE (GcalDateChooserDay, gcal_date_chooser_day, GTK_TYPE_BUTTON)

static void
gcal_date_chooser_day_dispose (GObject *object)
{
  GcalDateChooserDay *self = GCAL_DATE_CHOOSER_DAY (object);

  g_clear_pointer (&self->date, g_date_time_unref);

  G_OBJECT_CLASS (gcal_date_chooser_day_parent_class)->dispose (object);
}

static void
gcal_date_chooser_day_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  GcalDateChooserDay *self = GCAL_DATE_CHOOSER_DAY (object);

  switch (prop_id)
    {
    case PROP_HAS_DOT:
      g_value_set_boolean (value, gcal_date_chooser_day_get_has_dot (self));
      break;

    case PROP_DOT_VISIBLE:
      g_value_set_boolean (value, gcal_date_chooser_day_get_dot_visible (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_date_chooser_day_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  GcalDateChooserDay *self = GCAL_DATE_CHOOSER_DAY (object);

  switch (prop_id)
    {
    case PROP_HAS_DOT:
      gcal_date_chooser_day_set_has_dot (self, g_value_get_boolean (value));
      break;

    case PROP_DOT_VISIBLE:
      gcal_date_chooser_day_set_dot_visible (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_date_chooser_day_class_init (GcalDateChooserDayClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = gcal_date_chooser_day_dispose;
  object_class->get_property = gcal_date_chooser_day_get_property;
  object_class->set_property = gcal_date_chooser_day_set_property;

  /**
   * GcalDateChooserDay:has-dot:
   *
   * Whether the day has a dot or not.
   *
   * If the day has a dot, the label will be excentered to leave room for the
   * dot, it will be centered otherwise.
   */
  properties[PROP_HAS_DOT] =
    g_param_spec_boolean ("has-dot",
                          "Has dot",
                          "Whether the day has a dot or not",
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GcalDateChooserDay:dot-visible:
   *
   * Whether the dot is visible or not.
   *
   * There will be no visible dot if [property@GcalDateChooserDay:has-dot] is
   * #FALSE.
   */
  properties[PROP_DOT_VISIBLE] =
    g_param_spec_boolean ("dot-visible",
                          "Dot visible",
                          "Whether the dot is visible or not",
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, NUM_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/event-editor/gcal-date-chooser-day.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalDateChooserDay, dot_revealer);
  gtk_widget_class_bind_template_child (widget_class, GcalDateChooserDay, label);
}

static void
gcal_date_chooser_day_init (GcalDateChooserDay *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget*
gcal_date_chooser_day_new (void)
{
  return g_object_new (GCAL_TYPE_DATE_CHOOSER_DAY, NULL);
}

void
gcal_date_chooser_day_set_date (GcalDateChooserDay *self,
                                GDateTime          *date)
{
  GtkWidget *widget = GTK_WIDGET (self);
  g_autoptr (GDateTime) now = NULL;
  g_autofree gchar *text = NULL;
  gboolean today;
  gint weekday;

  g_assert (date != NULL);

  g_clear_pointer (&self->date, g_date_time_unref);
  self->date = g_date_time_ref (date);

  now = g_date_time_new_now (g_date_time_get_timezone (date));
  today = gcal_date_time_compare_date (date, now) == 0;
  weekday = g_date_time_get_day_of_week (date) % 7;

  if (G_UNLIKELY (today))
    gtk_widget_add_css_class (widget, "today");
  else
    gtk_widget_remove_css_class (widget, "today");

  if (G_LIKELY (is_workday (weekday)))
    gtk_widget_remove_css_class (widget, "non-workday");
  else
    gtk_widget_add_css_class (widget, "non-workday");

  text = g_strdup_printf ("%d", g_date_time_get_day_of_month (date));
  gtk_label_set_label (GTK_LABEL (self->label), text);
}

GDateTime*
gcal_date_chooser_day_get_date (GcalDateChooserDay *self)
{
  return self->date;
}

void
gcal_date_chooser_day_set_other_month (GcalDateChooserDay *self,
                                       gboolean            other_month)
{
  if (other_month)
    gtk_widget_add_css_class (GTK_WIDGET (self), "other-month");
  else
    gtk_widget_remove_css_class (GTK_WIDGET (self), "other-month");
}

/**
 * gcal_date_chooser_day_set_has_dot:
 * @self: a #GcalDateChooserDay
 * @has_dot: whether the day has a dot or not.
 *
 * Sets whether the day has a dot or not.
 *
 * If the day has a dot, the label will be excentered to leave room for the
 * dot, it will be centered otherwise.
 */
void
gcal_date_chooser_day_set_has_dot (GcalDateChooserDay *self,
                                   gboolean            has_dot)
{
  g_return_if_fail (GCAL_IS_DATE_CHOOSER_DAY (self));

  has_dot = !!has_dot;

  if (gtk_widget_get_visible (self->dot_revealer) == has_dot)
    return;

  gtk_widget_set_visible (self->dot_revealer, has_dot);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_HAS_DOT]);
}

/**
 * gcal_date_chooser_day_get_has_dot:
 * @self: a #GcalDateChooserDay
 *
 * Returns: whether the day has a dot or not.
 */
gboolean
gcal_date_chooser_day_get_has_dot (GcalDateChooserDay *self)
{
  g_return_val_if_fail (GCAL_IS_DATE_CHOOSER_DAY (self), FALSE);

  return gtk_widget_get_visible (self->dot_revealer);
}

/**
 * gcal_date_chooser_day_set_dot_visible:
 * @self: a #GcalDateChooserDay
 * @dot_visible: whether the dot is visible or not.
 *
 * Sets whether the dot is visible or not.
 *
 * There will be no visible dot if [property@GcalDateChooserDay:has-dot] is
 * #FALSE.
 */
void
gcal_date_chooser_day_set_dot_visible (GcalDateChooserDay *self,
                                       gboolean            dot_visible)
{
  g_return_if_fail (GCAL_IS_DATE_CHOOSER_DAY (self));

  dot_visible = !!dot_visible;

  if (gtk_revealer_get_reveal_child (GTK_REVEALER (self->dot_revealer)) == dot_visible)
    return;

  gtk_revealer_set_reveal_child (GTK_REVEALER (self->dot_revealer), dot_visible);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DOT_VISIBLE]);
}

/**
 * gcal_date_chooser_day_get_dot_visible:
 * @self: a #GcalDateChooserDay
 *
 * Returns: whether the dot is visible or not.
 */
gboolean
gcal_date_chooser_day_get_dot_visible (GcalDateChooserDay *self)
{
  g_return_val_if_fail (GCAL_IS_DATE_CHOOSER_DAY (self), FALSE);

  return gtk_revealer_get_reveal_child (GTK_REVEALER (self->dot_revealer));
}

void
gcal_date_chooser_day_set_selected (GcalDateChooserDay *self,
                                    gboolean            selected)
{
  GtkWidget *widget = GTK_WIDGET (self);

  if (G_UNLIKELY (selected))
    {
      gtk_widget_set_state_flags (widget, GTK_STATE_FLAG_SELECTED, FALSE);
      gtk_widget_remove_css_class (widget, "flat");
    }
  else
    {
      gtk_widget_unset_state_flags (widget, GTK_STATE_FLAG_SELECTED);
      gtk_widget_add_css_class (widget, "flat");
    }
}
