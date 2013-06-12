/* -*- mode: c; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gcal-day-view.c
 *
 * Copyright (C) 2013 - Erick Pérez Castellanos
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
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

#include "gcal-day-view.h"
#include "gcal-view.h"
#include "gcal-all-day-grid.h"
#include "gcal-days-grid.h"
#include "gcal-viewport.h"
#include "gcal-utils.h"
#include "gcal-view.h"

#include <glib/gi18n.h>

#include <libecal/libecal.h>

enum
{
  PROP_0,
  PROP_DATE
};

struct _GcalDayViewPrivate
{
  /* property */
  icaltimetype   *date;

  GtkWidget      *fold_button;

  /* events widgets parents */
  GtkWidget      *all_day_grid;
  GtkWidget      *day_grid;
};

static void           gcal_view_interface_init            (GcalViewIface  *iface);

static void           gcal_day_view_constructed           (GObject        *object);

static void           gcal_day_view_finalize              (GObject        *object);

static void           gcal_day_view_set_property          (GObject        *object,
                                                           guint           property_id,
                                                           const GValue   *value,
                                                           GParamSpec     *pspec);

static void           gcal_day_view_get_property          (GObject        *object,
                                                           guint           property_id,
                                                           GValue         *value,
                                                           GParamSpec     *pspec);

static void           gcal_day_view_set_date              (GcalDayView    *view,
                                                           icaltimetype   *date);

static icaltimetype*  gcal_day_view_get_initial_date      (GcalView       *view);

static icaltimetype*  gcal_day_view_get_final_date        (GcalView       *view);

static gboolean       gcal_day_view_contains_date         (GcalView       *view,
                                                           icaltimetype   *date);

static gchar*         gcal_day_view_get_left_header       (GcalView       *view);

static gchar*         gcal_day_view_get_right_header      (GcalView       *view);

static GtkWidget*     gcal_day_view_get_by_uuid           (GcalView       *view,
                                                           const gchar    *uuid);

G_DEFINE_TYPE_WITH_CODE (GcalDayView,
                         gcal_day_view,
                         GTK_TYPE_GRID,
                         G_IMPLEMENT_INTERFACE (GCAL_TYPE_VIEW,
                                                gcal_view_interface_init));

static void
gcal_day_view_class_init (GcalDayViewClass *klass)
{
  /* FIXME: Uncomment stuff here */
  /* GtkContainerClass *container_class; */
  GObjectClass *object_class;

  /* container_class = GTK_CONTAINER_CLASS (klass); */
  /* container_class->add   = gcal_day_view_add; */
  /* container_class->remove = gcal_day_view_remove; */
  /* container_class->forall = gcal_day_view_forall; */
  /* gtk_container_class_handle_border_width (container_class); */

  object_class = G_OBJECT_CLASS (klass);
  object_class->constructed = gcal_day_view_constructed;
  object_class->finalize = gcal_day_view_finalize;
  object_class->set_property = gcal_day_view_set_property;
  object_class->get_property = gcal_day_view_get_property;

  g_object_class_override_property (object_class, PROP_DATE, "active-date");

  g_type_class_add_private ((gpointer)klass, sizeof (GcalDayViewPrivate));
}



static void
gcal_day_view_init (GcalDayView *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            GCAL_TYPE_DAY_VIEW,
                                            GcalDayViewPrivate);
}

static void
gcal_view_interface_init (GcalViewIface *iface)
{
  /* FIXME: add new GcalView API */
  /* iface->remove_by_uuid = gcal_day_view_remove_by_uuid; */

  /* New API */
  iface->get_initial_date = gcal_day_view_get_initial_date;
  iface->get_final_date = gcal_day_view_get_final_date;
  iface->contains_date = gcal_day_view_contains_date;

  iface->get_left_header = gcal_day_view_get_left_header;
  iface->get_right_header = gcal_day_view_get_right_header;

  iface->get_by_uuid = gcal_day_view_get_by_uuid;
}

static void
gcal_day_view_constructed (GObject *object)
{
  GcalDayViewPrivate *priv;

  /* FIXME: debug code */
  GtkWidget *sw; /* involve overlay in it */
  GdkRGBA color;

  g_return_if_fail (GCAL_IS_DAY_VIEW (object));
  priv = GCAL_DAY_VIEW (object)->priv;

  if (G_OBJECT_CLASS (gcal_day_view_parent_class)->constructed != NULL)
      G_OBJECT_CLASS (gcal_day_view_parent_class)->constructed (object);

  priv->fold_button = gtk_button_new_with_label (_("Fold"));
  g_object_set (priv->fold_button,
                "valign", GTK_ALIGN_END,
                "margin", 6,
                NULL);
  gtk_style_context_add_class (
      gtk_widget_get_style_context (priv->fold_button),
      "nav-button");
  gtk_grid_attach (GTK_GRID (object), priv->fold_button, 0, 0, 1, 1);

  /* event widget holders */
  priv->all_day_grid = gcal_all_day_grid_new (2);
  gcal_all_day_grid_set_column_headers (GCAL_ALL_DAY_GRID (priv->all_day_grid),
                                        _("Today"),
                                        _("Tomorrow"));
  g_object_set (priv->all_day_grid, "spacing", 6, NULL);
  gtk_widget_set_size_request (priv->all_day_grid, -1, 64);
  gtk_widget_set_hexpand (priv->all_day_grid, TRUE);

  priv->day_grid = gcal_days_grid_new (2);
  gcal_days_grid_set_preferred_cell_height (GCAL_DAYS_GRID (priv->day_grid), 60);
  g_object_set (priv->day_grid, "spacing", 6, NULL);

  gtk_widget_set_hexpand (priv->day_grid, TRUE);
  gtk_widget_set_vexpand (priv->day_grid, TRUE);

  gtk_grid_attach (GTK_GRID (object), priv->all_day_grid, 1, 0, 1, 1);

  sw = gcal_viewport_new ();
  gcal_viewport_add (GCAL_VIEWPORT (sw), priv->day_grid);
  gtk_grid_attach (GTK_GRID (object), sw, 0, 1, 2, 1);

  /* binding sizes */
  gtk_widget_set_size_request (
      priv->fold_button,
      gcal_days_grid_get_scale_width (GCAL_DAYS_GRID (priv->day_grid)) - 12,
      -1);

  gtk_widget_show_all (GTK_WIDGET (object));
}

static void
gcal_day_view_finalize (GObject       *object)
{
  GcalDayViewPrivate *priv = GCAL_DAY_VIEW (object)->priv;

  if (priv->date != NULL)
    g_free (priv->date);

  /* Chain up to parent's finalize() method. */
  G_OBJECT_CLASS (gcal_day_view_parent_class)->finalize (object);
}

static void
gcal_day_view_set_property (GObject       *object,
                             guint          property_id,
                             const GValue  *value,
                             GParamSpec    *pspec)
{
  g_return_if_fail (GCAL_IS_DAY_VIEW (object));

  switch (property_id)
    {
    case PROP_DATE:
      gcal_day_view_set_date (
          GCAL_DAY_VIEW (object),
          g_value_dup_boxed (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gcal_day_view_get_property (GObject       *object,
                              guint          property_id,
                              GValue        *value,
                              GParamSpec    *pspec)
{
  GcalDayViewPrivate *priv;

  g_return_if_fail (GCAL_IS_DAY_VIEW (object));
  priv = GCAL_DAY_VIEW (object)->priv;

  switch (property_id)
    {
    case PROP_DATE:
      g_value_set_boxed (value, priv->date);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gcal_day_view_set_date (GcalDayView  *view,
                        icaltimetype *date)

{
  GcalDayViewPrivate *priv;

  priv = view->priv;

  if (priv->date != NULL)
    g_free (priv->date);

  priv->date = date;
}

/* GcalView API */
static icaltimetype*
gcal_day_view_get_initial_date (GcalView *view)
{
  GcalDayViewPrivate *priv;
  icaltimetype *new_date;

  priv = GCAL_DAY_VIEW (view)->priv;
  new_date = g_new0 (icaltimetype, 1);
  *new_date = *(priv->date);
  new_date->hour = 0;
  new_date->minute = 0;
  new_date->second = 0;

  return new_date;
}

/**
 * gcal_day_view_get_final_date:
 * @view: a #GcalDayView
 *
 * Returns the date of "tomorrow" at 23:59
 *
 * Returns:
 **/
static icaltimetype*
gcal_day_view_get_final_date (GcalView *view)
{
  GcalDayViewPrivate *priv;
  icaltimetype *new_date;

  priv = GCAL_DAY_VIEW (view)->priv;
  new_date = g_new0 (icaltimetype, 1);
  *new_date = *(priv->date);

  icaltime_adjust (new_date, 1, 0, 0, 0);

  new_date->hour = 23;
  new_date->minute = 59;
  new_date->second = 59;

  return new_date;
}

static gboolean
gcal_day_view_contains_date (GcalView     *view,
                             icaltimetype *date)
{
  GcalDayViewPrivate *priv;

  icaltimetype *first_day;
  icaltimetype *last_day;
  gint left_boundary;
  gint right_boundary;

  priv = GCAL_DAY_VIEW (view)->priv;
  first_day = gcal_day_view_get_initial_date (view);
  last_day = gcal_day_view_get_final_date (view);

  if (priv->date == NULL)
    return FALSE;

  /* XXX: Check for date_only comparison since might drop timezone info */
  left_boundary = icaltime_compare_date_only (*first_day, *date);
  right_boundary = icaltime_compare_date_only (*date, *last_day);

  if ((left_boundary == -1 || left_boundary == 0) &&
      (right_boundary == -1 || right_boundary == 0))
    {
      return TRUE;
    }

    return FALSE;
}

static gchar*
gcal_day_view_get_left_header (GcalView *view)
{
  GcalDayViewPrivate *priv;

  gchar str_date[64];

  struct tm tm_date;

  priv = GCAL_DAY_VIEW (view)->priv;

  tm_date = icaltimetype_to_tm (priv->date);
  e_utf8_strftime_fix_am_pm (str_date, 64, "%B", &tm_date);

  return g_strdup_printf ("%s, %d", str_date, priv->date->day);
}

static gchar*
gcal_day_view_get_right_header (GcalView *view)
{
  GcalDayViewPrivate *priv;

  priv = GCAL_DAY_VIEW (view)->priv;

  return g_strdup_printf ("%d", priv->date->year);
}

static GtkWidget*
gcal_day_view_get_by_uuid (GcalView    *view,
                           const gchar *uuid)
{
  GcalDayViewPrivate *priv;
  GtkWidget *widget;

  priv = GCAL_DAY_VIEW (view)->priv;

  widget =
    gcal_all_day_grid_get_by_uuid (GCAL_ALL_DAY_GRID (priv->all_day_grid),
                                   uuid);
  if (widget != NULL)
    return widget;

  widget =
    gcal_days_grid_get_by_uuid (GCAL_DAYS_GRID (priv->day_grid),
                                uuid);
  if (widget != NULL)
    return widget;

  return NULL;
}

/* Public API */
/**
 * gcal_day_view_new:
 * @date:
 *
 * Since: 0.1
 * Return value: A new #GcalDayView
 * Returns: (transfer full):
 **/
GtkWidget*
gcal_day_view_new (void)
{
  return g_object_new (GCAL_TYPE_DAY_VIEW, NULL);
}
