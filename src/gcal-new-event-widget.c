/* -*- mode: c; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gcal-new-event-widget.c
 * Copyright (C) 2012 Erick Pérez Castellanos <erickpc@gnome.org>
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

#include "gcal-new-event-widget.h"
#include "gcal-utils.h"

typedef struct
{
  GtkWidget     *title_label;
  GtkWidget     *what_entry;
  GtkWidget     *calendar_button;
  GtkWidget     *create_button;
  GtkWidget     *details_button;
} GcalNewEventWidgetPrivate;

static void      item_activated          (GtkWidget *item,
                                          gpointer   user_data);

G_DEFINE_TYPE_WITH_PRIVATE (GcalNewEventWidget, gcal_new_event_widget, GTK_TYPE_GRID)

static void
item_activated (GtkWidget *item,
                gpointer   user_data)
{
  GcalNewEventWidgetPrivate *priv;

  gchar *uid;
  gchar *color_name;
  GdkRGBA color;

  GdkPixbuf *pix;
  GtkWidget *image;

  priv = gcal_new_event_widget_get_instance_private (
              GCAL_NEW_EVENT_WIDGET (user_data));

  uid = g_object_get_data (G_OBJECT (item), "calendar-uid");
  color_name = g_object_get_data (G_OBJECT (item), "color");
  gdk_rgba_parse (&color, color_name);

  pix = gcal_get_pixbuf_from_color (&color, 16);
  image = gtk_image_new_from_pixbuf (pix);
  gtk_button_set_image (GTK_BUTTON (priv->calendar_button), image);

  /* saving the value */
  g_object_set_data (G_OBJECT (priv->calendar_button),
                     "calendar-uid", uid);
  g_object_unref (pix);
}

static void
gcal_new_event_widget_class_init (GcalNewEventWidgetClass *klass)
{
  GtkWidgetClass *widget_class;

  widget_class = GTK_WIDGET_CLASS (klass);

  /* Setup the template GtkBuilder xml for this class */
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/new-event.ui");

  /* Bind internals widgets */
  gtk_widget_class_bind_template_child_private (widget_class, GcalNewEventWidget, title_label);
  gtk_widget_class_bind_template_child_private (widget_class, GcalNewEventWidget, what_entry);
  gtk_widget_class_bind_template_child_private (widget_class, GcalNewEventWidget, calendar_button);
  gtk_widget_class_bind_template_child_private (widget_class, GcalNewEventWidget, create_button);
  gtk_widget_class_bind_template_child_private (widget_class, GcalNewEventWidget, details_button);

}

static void
gcal_new_event_widget_init (GcalNewEventWidget *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

/* Public API */
GtkWidget*
gcal_new_event_widget_new (void)
{
  return g_object_new (GCAL_TYPE_NEW_EVENT_WIDGET, NULL);
}

void
gcal_new_event_widget_set_title (GcalNewEventWidget *widget,
                                 const gchar        *title)
{
  GcalNewEventWidgetPrivate *priv;

  priv = gcal_new_event_widget_get_instance_private (widget);
  gtk_label_set_text (GTK_LABEL (priv->title_label), title);
}

void
gcal_new_event_widget_set_calendars (GcalNewEventWidget *widget,
                                     GtkTreeModel       *sources_model)
{
  GcalNewEventWidgetPrivate *priv;
  GtkWidget *menu;
  GtkWidget *item;

  gboolean valid;
  GtkTreeIter iter;

  priv = gcal_new_event_widget_get_instance_private (widget);

  valid = gtk_tree_model_get_iter_first (sources_model, &iter);
  if (! valid)
    return;

  menu = gtk_menu_new ();
  gtk_menu_button_set_popup (GTK_MENU_BUTTON (priv->calendar_button), menu);

  while (valid)
    {
      /* Walk through the list, reading each row */
      gchar *uid;
      gchar *name;
      gboolean active;
      GdkRGBA *color;

      GtkWidget *box;
      GtkWidget *name_label;
      GtkWidget *cal_image;
      GdkPixbuf *pix;

      gtk_tree_model_get (sources_model, &iter,
                          0, &uid,
                          1, &name,
                          2, &active,
                          3, &color,
                          -1);

      if (! active)
        {
          valid = gtk_tree_model_iter_next (sources_model, &iter);
          continue;
        }

      item = gtk_menu_item_new ();
      g_object_set_data_full (G_OBJECT (item),
                              "calendar-uid", uid, g_free);
      g_object_set_data_full (G_OBJECT (item),
                              "color", gdk_rgba_to_string (color), g_free);
      g_signal_connect (item,
                        "activate",
                        G_CALLBACK (item_activated),
                        widget);

      pix = gcal_get_pixbuf_from_color (color, 16);
      cal_image = gtk_image_new_from_pixbuf (pix);
      name_label = gtk_label_new (name);
      box = gtk_grid_new ();
      gtk_grid_set_column_spacing (GTK_GRID (box), 6);
      gtk_container_add (GTK_CONTAINER (box), cal_image);
      gtk_container_add (GTK_CONTAINER (box), name_label);
      gtk_container_add (GTK_CONTAINER (item), box);

      gtk_container_add (GTK_CONTAINER (menu), item);

      g_object_unref (pix);
      g_free (name);
      gdk_rgba_free (color);

      valid = gtk_tree_model_iter_next (sources_model, &iter);
    }

  gtk_widget_show_all (GTK_WIDGET (menu));
}

void
gcal_new_event_widget_set_default_calendar (GcalNewEventWidget *widget,
                                            const gchar        *source_uid)
{
  GcalNewEventWidgetPrivate *priv;

  GList *l;
  GtkMenu *menu;

  gchar *uid;
  gchar *color_name;
  GdkRGBA color;
  GdkPixbuf *pix;
  GtkWidget *image;

  priv = gcal_new_event_widget_get_instance_private (widget);
  color_name = NULL;
  uid = NULL;

  menu = gtk_menu_button_get_popup (GTK_MENU_BUTTON (priv->calendar_button));
  for (l = gtk_container_get_children (GTK_CONTAINER (menu));
       l != NULL;
       l = l->next)
    {
      GObject *item = l->data;
      uid = g_object_get_data (item, "calendar-uid");
      if (g_strcmp0 (source_uid, uid) == 0)
        {
          color_name = g_object_get_data (item, "color");
          break;
        }
    }

  gdk_rgba_parse (&color, color_name);
  pix = gcal_get_pixbuf_from_color (&color, 16);
  image = gtk_image_new_from_pixbuf (pix);
  gtk_button_set_image (GTK_BUTTON (priv->calendar_button), image);

  /* saving the value */
  g_object_set_data (G_OBJECT (priv->calendar_button),
                     "calendar-uid", uid);
  g_object_unref (pix);
}

/**
 * gcal_new_event_widget_get_entry:
 * @widget:
 *
 * Get a reference to the summanry entry
 *
 * Returns: (transfer none) a #GtkWidget
 **/
GtkWidget*
gcal_new_event_widget_get_entry (GcalNewEventWidget *widget)
{
  GcalNewEventWidgetPrivate *priv;

  priv = gcal_new_event_widget_get_instance_private (widget);
  return priv->what_entry;
}

GtkWidget*
gcal_new_event_widget_get_create_button (GcalNewEventWidget *widget)
{
  GcalNewEventWidgetPrivate *priv;

  priv = gcal_new_event_widget_get_instance_private (widget);
  return priv->create_button;
}

GtkWidget*
gcal_new_event_widget_get_details_button (GcalNewEventWidget *widget)
{
  GcalNewEventWidgetPrivate *priv;

  priv = gcal_new_event_widget_get_instance_private (widget);
  return priv->details_button;
}

/**
 * gcal_new_event_widget_get_calendar_uid:
 * @widget: a #GcalNewEventWidget
 *
 * Returns the selected calendar uid.
 *
 * Returns: (transfer full) a c-string
 **/
gchar*
gcal_new_event_widget_get_calendar_uid (GcalNewEventWidget *widget)
{
  GcalNewEventWidgetPrivate *priv;

  priv = gcal_new_event_widget_get_instance_private (widget);

  return g_strdup (g_object_get_data (G_OBJECT (priv->calendar_button),
                                      "calendar-uid"));
}

/**
 * gcal_new_event_widget_get_summary:
 * @widget: a #GcalNewEventWidget
 *
 * Return the summary written down in the entry
 *
 * Returns: (transfer full) a c-string
 **/
gchar*
gcal_new_event_widget_get_summary (GcalNewEventWidget *widget)
{
  GcalNewEventWidgetPrivate *priv;

  priv = gcal_new_event_widget_get_instance_private (widget);

  return g_strdup (gtk_entry_get_text (GTK_ENTRY (priv->what_entry)));
}
