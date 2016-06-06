/* -*- mode: c; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gcal-week-view.c
 *
 * Copyright (C) 2015 - Erick Pérez Castellanos
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

#include "gcal-week-view.h"
#include "gcal-utils.h"
#include "gcal-view.h"
#include "gcal-event-widget.h"
#include "gcal-week-header.h"
#include "gcal-week-grid.h"

#include <glib/gi18n.h>

#include <math.h>

#define ALL_DAY_CELLS_HEIGHT 40

static const double dashed [] =
{
  1.0,
  1.0
};

enum
{
  PROP_0,
  PROP_DATE
};

struct _GcalWeekViewChild
{
  GtkWidget *widget;
  gboolean   hidden_by_me;

  /* vertical index */
  gint       index;
};

typedef struct _GcalWeekViewChild GcalWeekViewChild;

typedef struct
{
  GtkBox          parent;

  /*
   * first day of the week according to user locale, being
   * 0 for Sunday, 1 for Monday and so on 
   */
  gint            first_weekday;

  /*
   * clock format from GNOME desktop settings
   */
  gboolean        use_24h_format;

  /* property */
  icaltimetype   *date;
  GcalManager    *manager; /* weak referenced */

  gint            clicked_cell;
} GcalWeekViewPrivate;

static void           gcal_week_view_constructed           (GObject        *object);

static void           gcal_week_view_finalize              (GObject        *object);

static void           gcal_week_view_set_property          (GObject        *object,
                                                            guint           property_id,
                                                            const GValue   *value,
                                                            GParamSpec     *pspec);

static void           gcal_week_view_get_property          (GObject        *object,
                                                            guint           property_id,
                                                            GValue         *value,
                                                            GParamSpec     *pspec);

static icaltimetype*  gcal_week_view_get_final_date        (GcalView       *view);

static void           gcal_view_interface_init             (GcalViewInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GcalWeekView, gcal_week_view, GCAL_TYPE_SUBSCRIBER_VIEW,
                         G_ADD_PRIVATE (GcalWeekView)
                         G_IMPLEMENT_INTERFACE (GCAL_TYPE_VIEW, gcal_view_interface_init));

/*
 * gcal_week_view_get_start_grid_y:
 *
 * In GcalMonthView this method returns the height of the headers of the view
 * and the grid. Here this points just the place where the grid_window hides
 * behind the header
 * Here this height includes:
 *  - The big header of the view
 *  - The grid header dislaying weekdays
 *  - The cell containing all-day events.
 */
gint
gcal_week_view_get_start_grid_y (GtkWidget *widget)
{
  GtkStyleContext *context;
  GtkStateFlags flags;
  GtkBorder padding;

  PangoLayout *layout;
  PangoFontDescription *font_desc;
  gint font_height;
  gdouble start_grid_y;

  context = gtk_widget_get_style_context (widget);
  flags = gtk_widget_get_state_flags (widget);

  layout = gtk_widget_create_pango_layout (widget, NULL);

  /* init header values */
  gtk_style_context_get_padding (context, flags, &padding);

  gtk_style_context_get (context, flags, "font", &font_desc, NULL);

  pango_font_description_set_weight (font_desc, PANGO_WEIGHT_SEMIBOLD);
  pango_layout_set_font_description (layout, font_desc);
  pango_layout_get_pixel_size (layout, NULL, &font_height);
  pango_font_description_free (font_desc);

  /* 6: is padding around the header */
  start_grid_y = font_height + padding.bottom;

  /* for including the all-day cells */
  start_grid_y += ALL_DAY_CELLS_HEIGHT;

  g_object_unref (layout);
  return start_grid_y;
}

gint
gcal_week_view_get_sidebar_width (GtkWidget *widget)
{
  GtkStyleContext *context;
  GtkBorder padding;

  PangoLayout *layout;
  PangoFontDescription *font_desc;
  gint mid_width;
  gint noon_width;
  gint hours_width;
  gint sidebar_width;

  context = gtk_widget_get_style_context (widget);

  gtk_style_context_get_padding (
      gtk_widget_get_style_context (widget),
      gtk_widget_get_state_flags (widget),
      &padding);

  layout = pango_layout_new (gtk_widget_get_pango_context (widget));
  gtk_style_context_get (context,
                         gtk_widget_get_state_flags(widget),
                         "font", &font_desc,
                         NULL);
  pango_layout_set_font_description (layout, font_desc);

  pango_layout_set_text (layout, _("Midnight"), -1);
  pango_layout_get_pixel_size (layout, &mid_width, NULL);

  pango_layout_set_text (layout, _("Noon"), -1);
  pango_layout_get_pixel_size (layout, &noon_width, NULL);

  pango_layout_set_text (layout, _("00:00 PM"), -1);
  pango_layout_get_pixel_size (layout, &hours_width, NULL);

  sidebar_width = noon_width > mid_width ? noon_width : mid_width;
  sidebar_width = sidebar_width > hours_width ? 0 : hours_width;
  sidebar_width += padding.left + padding.right;

  pango_font_description_free (font_desc);
  g_object_unref (layout);

  return sidebar_width;
}

static void
gcal_week_view_class_init (GcalWeekViewClass *klass)
{
  GtkWidgetClass *widget_class;
  GObjectClass *object_class;

  widget_class = GTK_WIDGET_CLASS (klass);

  object_class = G_OBJECT_CLASS (klass);
  object_class->constructed = gcal_week_view_constructed;
  object_class->finalize = gcal_week_view_finalize;
  object_class->set_property = gcal_week_view_set_property;
  object_class->get_property = gcal_week_view_get_property;

  g_object_class_override_property (object_class,
                                    PROP_DATE, "active-date");
}

static void
gcal_week_view_init (GcalWeekView *self)
{
  gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);
  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (self)), "calendar-view");
}

static void
gcal_view_interface_init (GcalViewInterface *iface)
{
  iface->get_initial_date = gcal_week_view_get_initial_date;
  iface->get_final_date = gcal_week_view_get_final_date;

  /* iface->clear_marks = gcal_week_view_clear_marks; */
}

static void
gcal_week_view_constructed (GObject *object)
{
  GcalWeekViewPrivate *priv;

  g_return_if_fail (GCAL_IS_WEEK_VIEW (object));
  priv = gcal_week_view_get_instance_private (GCAL_WEEK_VIEW (object));

  if (G_OBJECT_CLASS (gcal_week_view_parent_class)->constructed != NULL)
      G_OBJECT_CLASS (gcal_week_view_parent_class)->constructed (object);
}

static void
gcal_week_view_finalize (GObject       *object)
{
  GcalWeekViewPrivate *priv;

  priv = gcal_week_view_get_instance_private (GCAL_WEEK_VIEW (object));

  if (priv->date != NULL)
    g_free (priv->date);

  /* Chain up to parent's finalize() method. */
  G_OBJECT_CLASS (gcal_week_view_parent_class)->finalize (object);
}

static void
gcal_week_view_set_property (GObject       *object,
                             guint          property_id,
                             const GValue  *value,
                             GParamSpec    *pspec)
{
  GcalWeekViewPrivate *priv;

  priv = gcal_week_view_get_instance_private (GCAL_WEEK_VIEW (object));

  switch (property_id)
    {
    case PROP_DATE:
      {
        time_t range_start, range_end;
        icaltimetype *date;
        icaltimezone* default_zone;

        if (priv->date != NULL)
          g_free (priv->date);

        priv->date = g_value_dup_boxed (value);

        default_zone =
          gcal_manager_get_system_timezone (priv->manager);
        date = gcal_view_get_initial_date (GCAL_VIEW (object));
        range_start = icaltime_as_timet_with_zone (*date,
                                                   default_zone);
        g_free (date);
        date = gcal_view_get_final_date (GCAL_VIEW (object));
        range_end = icaltime_as_timet_with_zone (*date,
                                                 default_zone);
        g_free (date);
        gcal_manager_set_subscriber (priv->manager,
                                     E_CAL_DATA_MODEL_SUBSCRIBER (object),
                                     range_start,
                                     range_end);
        gtk_widget_queue_draw (GTK_WIDGET (object));
        break;
      }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gcal_week_view_get_property (GObject       *object,
                              guint          property_id,
                              GValue        *value,
                              GParamSpec    *pspec)
{
  GcalWeekViewPrivate *priv;

  g_return_if_fail (GCAL_IS_WEEK_VIEW (object));
  priv = gcal_week_view_get_instance_private (GCAL_WEEK_VIEW (object));

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

/* GcalView Interface API */
/**
 * gcal_week_view_get_initial_date:
 *
 * Since: 0.1
 * Return value: the first day of the week
 * Returns: (transfer full): Release with g_free()
 **/
icaltimetype*
gcal_week_view_get_initial_date (GcalView *view)
{
  GcalWeekViewPrivate *priv;
  icaltimetype *new_date;

  g_return_val_if_fail (GCAL_IS_WEEK_VIEW (view), NULL);
  priv = gcal_week_view_get_instance_private (GCAL_WEEK_VIEW(view));
  new_date = g_new0 (icaltimetype, 1);
  *new_date = icaltime_from_day_of_year (
                  icaltime_start_doy_week (*(priv->date), priv->first_weekday + 1),
                  priv->date->year);
  new_date->is_date = 0;
  new_date->hour = 0;
  new_date->minute = 0;
  new_date->second = 0;
  *new_date = icaltime_set_timezone (new_date, priv->date->zone);
  return new_date;
}

/**
 * gcal_week_view_get_final_date:
 *
 * Since: 0.1
 * Return value: the last day of the week
 * Returns: (transfer full): Release with g_free()
 **/
static icaltimetype*
gcal_week_view_get_final_date (GcalView *view)
{
  GcalWeekViewPrivate *priv;
  icaltimetype *new_date;

  g_return_val_if_fail (GCAL_IS_WEEK_VIEW (view), NULL);
  priv = gcal_week_view_get_instance_private (GCAL_WEEK_VIEW(view));
  new_date = g_new0 (icaltimetype, 1);
  *new_date = icaltime_from_day_of_year (
                  icaltime_start_doy_week (*(priv->date), priv->first_weekday + 1) + 6,
                  priv->date->year);
  new_date->is_date = 0;
  new_date->hour = 23;
  new_date->minute = 59;
  new_date->second = 0;
  *new_date = icaltime_set_timezone (new_date, priv->date->zone);
  return new_date;
}

/* Public API */
/**
 * gcal_week_view_new:
 *
 * Create a week-view widget
 *
 * Returns: (transfer full):
 **/
GtkWidget*
gcal_week_view_new (void)
{
  return g_object_new (GCAL_TYPE_WEEK_VIEW, NULL);
}

void
gcal_week_view_set_manager (GcalWeekView *week_view,
                            GcalManager  *manager)
{
  GcalWeekViewPrivate *priv = gcal_week_view_get_instance_private (week_view);

  priv->manager = manager;
}

/**
 * gcal_week_view_set_first_weekday:
 * @view:
 * @day_nr:
 *
 * Set the first day of the week according to the locale, being
 * 0 for Sunday, 1 for Monday and so on.
 **/
void
gcal_week_view_set_first_weekday (GcalWeekView *view,
                                  gint          day_nr)
{
  GcalWeekViewPrivate *priv;

  priv = gcal_week_view_get_instance_private (view);
  priv->first_weekday = day_nr;
}

/**
 * gcal_week_view_set_use_24h_format:
 * @view:
 * @use_24h:
 *
 * Whether the view will show time using 24h or 12h format
 **/
void
gcal_week_view_set_use_24h_format (GcalWeekView *view,
                                   gboolean      use_24h)
{
  GcalWeekViewPrivate *priv;

  priv = gcal_week_view_get_instance_private (view);
  priv->use_24h_format = use_24h;
}
