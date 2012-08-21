/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 2 -*- */
/*
 * gcal-event-overlay.c
 * Copyright (C) 2012 Erick Pérez Castellanos <erickpc@gnome.org>
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

#include "gcal-event-overlay.h"
#include "gcal-arrow-container.h"
#include "gcal-utils.h"
#include "gcal-marshalers.h"

#include <libecal/libecal.h>
#include <glib/gi18n.h>

#define CLOSE_IMAGE_FILE UI_DATA_DIR "/close-window.svg"

struct _GcalEventOverlayPrivate
{
  GtkWidget     *container;
  GtkWidget     *title_label;
  GtkWidget     *what_entry;
  GtkWidget     *calendar_button;

  GtkListStore  *sources_model;
  GtkTreeIter   *active_iter;

  icaltimetype  *start_span;
  icaltimetype  *end_span;
};

enum
{
  CANCELLED = 1,
  CREATED,
  NUM_SIGNALS
};

static guint signals[NUM_SIGNALS] = { 0, };

static void        gcal_event_overlay_constructed           (GObject           *object);

static void        gcal_event_overlay_set_calendars_menu    (GcalEventOverlay  *widget);

static gboolean    gcal_event_overlay_gather_data           (GcalEventOverlay  *widget,
                                                             GcalNewEventData **new_data);

static void        gcal_event_overlay_create_button_clicked (GtkWidget         *widget,
                                                             gpointer           user_data);

static void        gcal_event_overlay_close_button_clicked  (GtkWidget         *widget,
                                                             gpointer           user_data);

static void        gcal_event_overlay_calendar_selected     (GtkWidget         *menu_item,
                                                             gpointer           user_data);

static GdkPixbuf*  gcal_event_overlay_get_pixbuf_for_color  (GdkColor          *color);

G_DEFINE_TYPE(GcalEventOverlay, gcal_event_overlay, GTK_CLUTTER_TYPE_ACTOR)

static void
gcal_event_overlay_class_init (GcalEventOverlayClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->constructed = gcal_event_overlay_constructed;

  signals[CANCELLED] = g_signal_new ("cancelled",
                                     GCAL_TYPE_EVENT_OVERLAY,
                                     G_SIGNAL_RUN_LAST,
                                     G_STRUCT_OFFSET (GcalEventOverlayClass,
                                                      cancelled),
                                     NULL, NULL,
                                     g_cclosure_marshal_VOID__VOID,
                                     G_TYPE_NONE,
                                     0);

  signals[CREATED] = g_signal_new ("created",
                                   GCAL_TYPE_EVENT_OVERLAY,
                                   G_SIGNAL_RUN_LAST,
                                   G_STRUCT_OFFSET (GcalEventOverlayClass,
                                                    created),
                                   NULL, NULL,
                                   _gcal_marshal_VOID__POINTER_BOOLEAN,
                                   G_TYPE_NONE,
                                   2,
                                   G_TYPE_POINTER,
                                   G_TYPE_BOOLEAN);

  g_type_class_add_private ((gpointer)klass, sizeof(GcalEventOverlayPrivate));
}

static void
gcal_event_overlay_init (GcalEventOverlay *self)
{
  GcalEventOverlayPrivate *priv;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            GCAL_TYPE_EVENT_OVERLAY,
                                            GcalEventOverlayPrivate);
  priv = self->priv;

  priv->active_iter = NULL;
  priv->start_span = NULL;
  priv->end_span = NULL;
}

static void
gcal_event_overlay_constructed (GObject* object)
{
  GcalEventOverlayPrivate *priv;
  GtkWidget *main_grid;
  GtkWidget *row_grid;
  GtkWidget *overlay;
  GtkWidget *create_button;
  GtkWidget *details_button;
  GtkWidget *close_image;
  GtkWidget *close_button;
  GtkWidget *calendar_image;

  priv = GCAL_EVENT_OVERLAY (object)->priv;

  /* chaining up */
  G_OBJECT_CLASS (gcal_event_overlay_parent_class)->constructed (object);

  overlay = gtk_overlay_new ();

  priv->container = gcal_arrow_container_new ();
  gtk_container_set_border_width (GTK_CONTAINER (priv->container), 14);
  gtk_style_context_add_class (
      gtk_widget_get_style_context (priv->container),
      "new-event-view");
  gtk_container_add (GTK_CONTAINER (overlay), priv->container);

  main_grid = gtk_grid_new ();
  g_object_set (main_grid,
                "row-spacing", 18,
                "column-spacing", 12,
                "column-homogeneous", TRUE,
                NULL);

  priv->title_label = gtk_label_new (_("New Event"));
  gtk_grid_attach (GTK_GRID (main_grid),
                   priv->title_label,
                   0, 0,
                   2, 1);
  row_grid = gtk_grid_new ();
  gtk_grid_attach (GTK_GRID (main_grid),
                   row_grid,
                   0, 1,
                   2, 1);
  create_button = gtk_button_new_with_label (_("Create"));
  g_object_set_data (G_OBJECT (create_button), "open-details", GUINT_TO_POINTER (0));
  gtk_style_context_add_class (
      gtk_widget_get_style_context (create_button),
      "suggested-action");
  gtk_grid_attach (GTK_GRID (main_grid),
                   create_button,
                   0, 2,
                   1, 1);
  details_button = gtk_button_new_with_label (_("More details"));
  g_object_set_data (G_OBJECT (details_button), "open-details", GUINT_TO_POINTER (1));
  gtk_grid_attach (GTK_GRID (main_grid),
                   details_button,
                   1, 2,
                   1, 1);

  g_object_set (row_grid,
                "orientation", GTK_ORIENTATION_HORIZONTAL,
                "column-spacing", 6,
                NULL);
  priv->what_entry = gtk_entry_new ();
  g_object_set_data (G_OBJECT (priv->what_entry), "open-details", GUINT_TO_POINTER (0));
  gtk_entry_set_placeholder_text (GTK_ENTRY (priv->what_entry),
                                  _("What (e.g. Alien Invasion)"));
  g_object_set (priv->what_entry,
                "valign", GTK_ALIGN_CENTER,
                "expand", TRUE,
                NULL);
  gtk_container_add (GTK_CONTAINER (row_grid),
                     priv->what_entry);

  priv->calendar_button = gtk_menu_button_new ();
  calendar_image = gtk_image_new_from_icon_name ("x-office-calendar-symbolic",
                                                 GTK_ICON_SIZE_MENU);
  g_object_set (priv->calendar_button,
                "vexpand", TRUE,
                "always-show-image", TRUE,
                "image", calendar_image,
                "menu", gtk_menu_new (),
                NULL);

  gtk_container_add (GTK_CONTAINER (row_grid),
                     priv->calendar_button);

  gtk_container_add (GTK_CONTAINER (priv->container),
                     main_grid);

  close_button = gtk_button_new ();
  close_image = gtk_image_new_from_file (CLOSE_IMAGE_FILE);
  gtk_container_add (GTK_CONTAINER (close_button), close_image);
  g_object_set (close_button,
                "relief", GTK_RELIEF_NONE,
                "focus-on-click", FALSE,
                "halign", GTK_ALIGN_END,
                "valign", GTK_ALIGN_START,
                NULL);

  gtk_overlay_add_overlay (GTK_OVERLAY (overlay), close_button);

  gtk_widget_show_all (overlay);
  gtk_container_add (
      GTK_CONTAINER (gtk_clutter_actor_get_widget (GTK_CLUTTER_ACTOR (object))),
      overlay);

  /* Hooking signals */
  g_signal_connect (priv->what_entry,
                    "activate",
                    G_CALLBACK (gcal_event_overlay_create_button_clicked),
                    object);

  g_signal_connect (create_button,
                    "clicked",
                    G_CALLBACK (gcal_event_overlay_create_button_clicked),
                    object);

  g_signal_connect (details_button,
                    "clicked",
                    G_CALLBACK (gcal_event_overlay_create_button_clicked),
                    object);

  g_signal_connect (close_button,
                    "clicked",
                    G_CALLBACK (gcal_event_overlay_close_button_clicked),
                    object);
}

static void
gcal_event_overlay_set_calendars_menu (GcalEventOverlay *widget)
{
  GcalEventOverlayPrivate *priv;
  GtkMenu *menu;
  GtkWidget *item;
  GList *children;

  gboolean valid;
  GtkTreeIter iter;

  priv = widget->priv;

  valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->sources_model),
                                         &iter);
  if (! valid)
    return;

  menu = gtk_menu_button_get_menu (GTK_MENU_BUTTON (priv->calendar_button));
  children = gtk_container_get_children (GTK_CONTAINER (menu));
  g_list_foreach (children, (GFunc) gtk_widget_destroy, NULL);

  while (valid)
    {
      /* Walk through the list, reading each row */
      gchar *name;
      GdkColor *color;
      GtkWidget *cal_image;
      GdkPixbuf *pix;

      gtk_tree_model_get (GTK_TREE_MODEL (priv->sources_model), &iter,
                          1, &name,
                          3, &color,
                          -1);

      item = gtk_image_menu_item_new ();
      g_object_set_data_full (G_OBJECT (item),
                              "sources-iter",
                              gtk_tree_iter_copy (&iter),
                              (GDestroyNotify) gtk_tree_iter_free);
      g_signal_connect (item,
                        "activate",
                        G_CALLBACK (gcal_event_overlay_calendar_selected),
                        widget);

      pix = gcal_event_overlay_get_pixbuf_for_color (color);

      cal_image = gtk_image_new_from_pixbuf (pix);
      g_object_set (item,
                    "always-show-image", TRUE,
                    "image", cal_image,
                    "label", name,
                    NULL);

      gtk_container_add (GTK_CONTAINER (menu), item);

      g_object_unref (pix);
      g_free (name);
      gdk_color_free (color);

      valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (priv->sources_model),
                                        &iter);
    }

  gtk_widget_show_all (GTK_WIDGET (menu));
/*
  if (! had_menu)
    {
      gtk_menu_button_set_menu (GTK_MENU_BUTTON (priv->calendar_button),
                                menu);
    }*/
}

static gboolean
gcal_event_overlay_gather_data (GcalEventOverlay  *widget,
                                GcalNewEventData **new_data)
{
  GcalEventOverlayPrivate *priv;
  GcalNewEventData *data;
  gchar *uid;

  priv = widget->priv;
  if (priv->active_iter == NULL)
    return FALSE;
  if (g_strcmp0 (gtk_entry_get_text (GTK_ENTRY (priv->what_entry)), "") == 0)
    return FALSE;

  data = g_new0 (GcalNewEventData, 1);
  data->summary = g_strdup (gtk_entry_get_text (GTK_ENTRY (priv->what_entry)));
  gtk_tree_model_get (GTK_TREE_MODEL (priv->sources_model), priv->active_iter,
                      0, &uid,
                      -1);
  data->calendar_uid = uid;
  data->start_date = priv->start_span;
  data->end_date = priv->end_span;

  *new_data = data;
  return TRUE;
}

static void
gcal_event_overlay_create_button_clicked (GtkWidget *widget,
                                         gpointer   user_data)
{
  GcalNewEventData *data;
  guint open_details;

  open_details = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (widget),
                                   "open-details"));

  /* Maybe some kind of notification */
  if (! gcal_event_overlay_gather_data (GCAL_EVENT_OVERLAY (user_data), &data))
    return;

  g_signal_emit (GCAL_EVENT_OVERLAY (user_data),
                 signals[CREATED],
                 0,
                 data, open_details == 1);

  g_free (data->calendar_uid);
  g_free (data->summary);
  g_free (data);
}

static void
gcal_event_overlay_close_button_clicked (GtkWidget *widget,
                                         gpointer   user_data)
{
  GcalEventOverlay *self;
  self = GCAL_EVENT_OVERLAY (user_data);

  g_signal_emit (self, signals[CANCELLED], 0);
}

static void
gcal_event_overlay_calendar_selected (GtkWidget *menu_item,
                                      gpointer   user_data)
{
  GcalEventOverlayPrivate *priv;
  GtkTreeIter *iter;
  GdkColor *color;

  GdkPixbuf *pix;
  GtkWidget *cal_image;

  priv = GCAL_EVENT_OVERLAY (user_data)->priv;
  iter = g_object_get_data (G_OBJECT (menu_item), "sources-iter");

  gtk_tree_model_get (GTK_TREE_MODEL (priv->sources_model), iter,
                      3, &color,
                      -1);

  if (priv->active_iter != NULL)
    gtk_tree_iter_free (priv->active_iter);
  priv->active_iter = gtk_tree_iter_copy (iter);

  pix = gcal_event_overlay_get_pixbuf_for_color (color);
  cal_image = gtk_image_new_from_pixbuf (pix);
  gtk_button_set_image (GTK_BUTTON (priv->calendar_button), cal_image);

  gdk_color_free (color);
  g_object_unref (pix);
}

static GdkPixbuf*
gcal_event_overlay_get_pixbuf_for_color (GdkColor *color)
{
  cairo_surface_t *surface;
  cairo_t *cr;
  gint width, height;
  GdkPixbuf *pix;

  /* TODO: review size here, maybe not hardcoded */
  width = height = 10;
  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
  cr = cairo_create (surface);

  cairo_set_source_rgb (cr,
                         color->red / 65535.0,
                         color->green / 65535.0,
                         color->blue / 65535.0);
  cairo_rectangle (cr, 0, 0, width, height);
  cairo_fill (cr);
  cairo_destroy (cr);
  pix = gdk_pixbuf_get_from_surface (surface,
                                     0, 0,
                                     width, height);
  cairo_surface_destroy (surface);
  return pix;
}

/* Public API */

ClutterActor*
gcal_event_overlay_new (void)
{
  return g_object_new (GCAL_TYPE_EVENT_OVERLAY, NULL);
}

void
gcal_event_overlay_reset (GcalEventOverlay *widget)
{
  GcalEventOverlayPrivate *priv;

  g_return_if_fail (GCAL_IS_EVENT_OVERLAY (widget));
  priv = widget->priv;

  if (priv->active_iter != NULL)
    {
      gtk_tree_iter_free (priv->active_iter);
      priv->active_iter = NULL;
    }
  if (priv->start_span != NULL)
    {
      g_free (priv->start_span);
      priv->start_span = NULL;
    }
  if (priv->end_span != NULL)
    {
      g_free (priv->end_span);
      priv->end_span = NULL;
    }

  gtk_label_set_text (GTK_LABEL (priv->title_label), _("New Event"));
  gtk_entry_set_text (GTK_ENTRY (priv->what_entry), "");

  gtk_button_set_image (GTK_BUTTON (priv->calendar_button),
                        gtk_image_new_from_icon_name ("x-office-calendar-symbolic",
                                                      GTK_ICON_SIZE_MENU));
}

/**
 * gcal_event_overlay_set_span:
 *
 * @widget: self
 * @start_date: The initial date
 * @end_date: The end date or NULL if the date is the same as the initial
 */
void
gcal_event_overlay_set_span (GcalEventOverlay *widget,
                             icaltimetype     *start_date,
                             icaltimetype     *end_date)
{
  GcalEventOverlayPrivate *priv;
  struct tm tm_date;
  gchar start[64];
  gchar end[64];
  gchar *title_date;
  icaltimetype *real_end_date;

  g_return_if_fail (GCAL_IS_EVENT_OVERLAY (widget));
  priv = widget->priv;

  if (priv->start_span == NULL)
    priv->start_span = gcal_dup_icaltime (start_date);
  else
    *(priv->start_span) = *start_date;

  real_end_date = end_date == NULL ? start_date : end_date;
  if (priv->end_span == NULL)
    priv->end_span = gcal_dup_icaltime (real_end_date);
  else
    *(priv->end_span) = *real_end_date;

  /* Updating labels */
  /* TODO: add as many posibles fixes for cases here, since this is when we
   * format the dates for the label */
  tm_date = icaltimetype_to_tm (start_date);
  e_utf8_strftime_fix_am_pm (start, 64, "%B %d", &tm_date);

  if (end_date == NULL)
    {
      title_date = g_strdup_printf (_("New Event on %s"), start);
    }
  else
    {
      tm_date = icaltimetype_to_tm (real_end_date);

      /* Fix 1: Not repeating the month */
      if (start_date->month == end_date->month &&
          start_date->year == end_date->year)
        {
          e_utf8_strftime_fix_am_pm (end, 64, "%d", &tm_date);
        }
      else
        {
          e_utf8_strftime_fix_am_pm (end, 64, "%B %d", &tm_date);
        }

      title_date = g_strdup_printf (_("New Event from %s to %s"),
                                    start,
                                    end);
    }

  gtk_label_set_text (GTK_LABEL (priv->title_label), title_date);
  g_free (title_date);
}

void
gcal_event_overlay_set_sources_model (GcalEventOverlay *widget,
                                      GtkListStore     *sources_model)
{
  GcalEventOverlayPrivate *priv;

  g_return_if_fail (GCAL_IS_EVENT_OVERLAY (widget));
  g_return_if_fail (GTK_IS_LIST_STORE (sources_model));
  priv = widget->priv;

  priv->sources_model = sources_model;

  gcal_event_overlay_set_calendars_menu (widget);
}
