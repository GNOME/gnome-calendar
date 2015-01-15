/* -*- mode: c; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* gcal-year-view.c
 *
 * Copyright (C) 2015 Erick Pérez Castellanos <erick.red@gmail.com>
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "gcal-year-view.h"
#include "gcal-view.h"

struct _GcalYearViewPrivate
{
  /* composite, GtkBuilder's widgets */
  GtkWidget    *year_view_navigator;
  GtkWidget    *sidebar;
  GtkWidget    *add_event_button;
  GtkWidget    *events_listbox;

  /* manager singleton */
  GcalManager  *manager;

  /* date property */
  icaltimetype *date;
};

enum {
  PROP_0,
  PROP_DATE,
  LAST_PROP
};

static void   gcal_view_interface_init (GcalViewIface *iface);
static void   gcal_data_model_subscriber_interface_init (ECalDataModelSubscriberInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GcalYearView, gcal_year_view, GTK_TYPE_BOX,
                         G_ADD_PRIVATE (GcalYearView)
                         G_IMPLEMENT_INTERFACE (GCAL_TYPE_VIEW, gcal_view_interface_init)
                         G_IMPLEMENT_INTERFACE (E_TYPE_CAL_DATA_MODEL_SUBSCRIBER,
                                                gcal_data_model_subscriber_interface_init));

static void
update_date (GcalYearView *year_view,
             icaltimetype *new_date)
{
  GcalYearViewPrivate *priv = year_view->priv;

  /* FIXME: add updating subscribe range */
  if (priv->date != NULL)
    g_free (priv->date);
  priv->date = new_date;
}

static gboolean
draw_navigator (GcalYearView *year_view,
                cairo_t      *cr,
                GtkWidget    *widget)
{
  /* FIXME: draw navigator */
  guint width, height;
  GdkRGBA color;

  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);
  cairo_arc (cr, width / 2.0, height / 2.0, MIN (width, height) / 2.0, 0, 2 * G_PI);

  gtk_style_context_get_color (gtk_widget_get_style_context (widget), 0, &color);
  gdk_cairo_set_source_rgba (cr, &color);

  cairo_fill (cr);

  return FALSE;
}

static void
add_event_clicked_cb (GcalYearView *year_view,
                      GtkButton    *button)
{
  /* FIXME: implement send detailed signal */
}

static void
gcal_year_view_finalize (GObject *object)
{
  GcalYearViewPrivate *priv = GCAL_YEAR_VIEW (object)->priv;

  if (priv->date != NULL)
    g_free (priv->date);

  G_OBJECT_CLASS (gcal_year_view_parent_class)->finalize (object);
}

static void
gcal_year_view_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GcalYearViewPrivate *priv = GCAL_YEAR_VIEW (object)->priv;

  switch (prop_id)
    {
    case PROP_DATE:
      g_value_set_boxed (value, priv->date);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_year_view_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  switch (prop_id)
    {
    case PROP_DATE:
      update_date (GCAL_YEAR_VIEW (object), g_value_dup_boxed (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static gchar*
gcal_year_view_get_left_header (GcalView *view)
{
  GcalYearViewPrivate *priv = GCAL_YEAR_VIEW (view)->priv;

  return g_strdup_printf ("%d", priv->date->year);
}

static gchar*
gcal_year_view_get_right_header (GcalView *view)
{
  return g_strdup ("");
}

static void
gcal_year_view_class_init (GcalYearViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gcal_year_view_finalize;
  object_class->get_property = gcal_year_view_get_property;
  object_class->set_property = gcal_year_view_set_property;

  g_object_class_override_property (object_class, PROP_DATE, "active-date");

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/year-view.ui");

  gtk_widget_class_bind_template_child_private (widget_class, GcalYearView, year_view_navigator);
  gtk_widget_class_bind_template_child_private (widget_class, GcalYearView, sidebar);
  gtk_widget_class_bind_template_child_private (widget_class, GcalYearView, add_event_button);
  gtk_widget_class_bind_template_child_private (widget_class, GcalYearView, events_listbox);

  gtk_widget_class_bind_template_callback (widget_class, draw_navigator);
  gtk_widget_class_bind_template_callback (widget_class, add_event_clicked_cb);
}

static void
gcal_year_view_init (GcalYearView *self)
{
  self->priv = gcal_year_view_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));
  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (self)), "calendar-view");
}

static void
gcal_view_interface_init (GcalViewIface *iface)
{
  /* FIXME: implement what's needed */
  iface->get_left_header = gcal_year_view_get_left_header;
  iface->get_right_header = gcal_year_view_get_right_header;
}

static void
gcal_data_model_subscriber_interface_init (ECalDataModelSubscriberInterface *iface)
{
  /* FIXME: implement this */
  iface->component_added = NULL;
  iface->component_modified = NULL;
  iface->component_removed = NULL;
  iface->freeze = NULL;
  iface->thaw = NULL;
}

/* Public API */
GcalYearView *
gcal_year_view_new (void)
{
  return g_object_new (GCAL_TYPE_YEAR_VIEW, NULL);
}

void
gcal_year_view_set_manager (GcalYearView *year_view,
                            GcalManager  *manager)
{
  GcalYearViewPrivate *priv = year_view->priv;

  priv->manager = manager;
}
