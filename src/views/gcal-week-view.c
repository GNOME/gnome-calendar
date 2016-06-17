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

struct _GcalWeekView
{
  GtkBox          parent;

  GtkWidget      *header;

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
  icaltimetype   *current_date;
  GcalManager    *manager; /* weak referenced */

  gint            clicked_cell;
};

static void           gcal_week_view_component_added            (ECalDataModelSubscriber *subscriber,
                                                                 ECalClient              *client,
                                                                 ECalComponent           *comp);

static void           gcal_week_view_component_modified         (ECalDataModelSubscriber *subscriber,
                                                                 ECalClient              *client,
                                                                 ECalComponent           *comp);

static void           gcal_week_view_component_removed          (ECalDataModelSubscriber *subscriber,
                                                                 ECalClient              *client,
                                                                 const gchar             *uid,
                                                                 const gchar             *rid);

static void           gcal_week_view_freeze                     (ECalDataModelSubscriber *subscriber);

static void           gcal_week_view_thaw                       (ECalDataModelSubscriber *subscriber);

static void           gcal_view_interface_init                  (GcalViewInterface *iface);

static void           gcal_data_model_subscriber_interface_init (ECalDataModelSubscriberInterface *iface);

static void           gcal_week_view_constructed                (GObject        *object);

static void           gcal_week_view_finalize                   (GObject        *object);

static void           gcal_week_view_set_property               (GObject        *object,
                                                                 guint           property_id,
                                                                 const GValue   *value,
                                                                 GParamSpec     *pspec);

static void           gcal_week_view_get_property               (GObject        *object,
                                                                 guint           property_id,
                                                                 GValue         *value,
                                                                 GParamSpec     *pspec);

static icaltimetype*  gcal_week_view_get_final_date             (GcalView       *view);

G_DEFINE_TYPE_WITH_CODE (GcalWeekView, gcal_week_view, GTK_TYPE_BOX,
                         G_IMPLEMENT_INTERFACE (GCAL_TYPE_VIEW, gcal_view_interface_init)
                         G_IMPLEMENT_INTERFACE (E_TYPE_CAL_DATA_MODEL_SUBSCRIBER,
                                                gcal_data_model_subscriber_interface_init));

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
gcal_week_view_component_added (ECalDataModelSubscriber *subscriber,
                                ECalClient              *client,
                                ECalComponent           *comp)
{
  GcalWeekView *self = GCAL_WEEK_VIEW (subscriber);

  GcalEvent *event;

  event = gcal_event_new (e_client_get_source (E_CLIENT (client)), comp, NULL);

  if (gcal_event_is_multiday (event) || gcal_event_get_all_day (event))
    gcal_week_header_add_event (GCAL_WEEK_HEADER (self->header), event);
}

static void
gcal_week_view_component_modified (ECalDataModelSubscriber *subscriber,
                                   ECalClient              *client,
                                   ECalComponent           *comp)
{
  GcalWeekView *self = GCAL_WEEK_VIEW (subscriber);
  GcalEvent *event;
  GcalWeekHeader *header;
  gchar *uuid;

  header = GCAL_WEEK_HEADER (self->header);

  event = gcal_event_new (e_client_get_source (E_CLIENT (client)), comp, NULL);
  uuid = get_uuid_from_component (e_client_get_source (E_CLIENT (client)), comp);

  if (gcal_event_is_multiday (event) || gcal_event_get_all_day (event))
    gcal_week_header_remove_event (header, uuid);

  g_free (uuid);

  gcal_week_view_component_added (subscriber, client, comp);
}

static void
gcal_week_view_component_removed (ECalDataModelSubscriber *subscriber,
                                  ECalClient              *client,
                                  const gchar             *uid,
                                  const gchar             *rid)
{
  GcalWeekView *self = GCAL_WEEK_VIEW (subscriber);
  GcalWeekHeader *header;
  ESource *source;
  gchar *uuid;

  header = GCAL_WEEK_HEADER (self->header);

  source = e_client_get_source (E_CLIENT (client));

  if (rid != NULL)
    uuid = g_strdup_printf ("%s:%s:%s", e_source_get_uid (source), uid, rid);
  else
    uuid = g_strdup_printf ("%s:%s", e_source_get_uid (source), uid);

  gcal_week_header_remove_event (header, uuid);

  g_free (uuid);
}

static void
gcal_week_view_freeze (ECalDataModelSubscriber *subscriber)
{
}

static void
gcal_week_view_thaw (ECalDataModelSubscriber *subscriber)
{
}

static void
gcal_week_view_class_init (GcalWeekViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = gcal_week_view_constructed;
  object_class->finalize = gcal_week_view_finalize;
  object_class->set_property = gcal_week_view_set_property;
  object_class->get_property = gcal_week_view_get_property;

  g_object_class_override_property (object_class,
                                    PROP_DATE, "active-date");

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/week-view.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalWeekView, header);

  gtk_widget_class_set_css_name (widget_class, "calendar-view");
}

static void
gcal_week_view_init (GcalWeekView *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
gcal_view_interface_init (GcalViewInterface *iface)
{
  iface->get_initial_date = gcal_week_view_get_initial_date;
  iface->get_final_date = gcal_week_view_get_final_date;

  /* iface->clear_marks = gcal_week_view_clear_marks; */
}

static void
gcal_data_model_subscriber_interface_init (ECalDataModelSubscriberInterface *iface)
{
  iface->component_added = gcal_week_view_component_added;
  iface->component_modified = gcal_week_view_component_modified;
  iface->component_removed = gcal_week_view_component_removed;
  iface->freeze = gcal_week_view_freeze;
  iface->thaw = gcal_week_view_thaw;
}

static void
gcal_week_view_constructed (GObject *object)
{
  GcalWeekView *self;

  g_return_if_fail (GCAL_IS_WEEK_VIEW (object));
  self = GCAL_WEEK_VIEW (object);

  if (G_OBJECT_CLASS (gcal_week_view_parent_class)->constructed != NULL)
      G_OBJECT_CLASS (gcal_week_view_parent_class)->constructed (object);
}

static void
gcal_week_view_finalize (GObject       *object)
{
  GcalWeekView *self;

  self = GCAL_WEEK_VIEW (object);

  g_clear_pointer (&self->date, g_free);

  /* Chain up to parent's finalize() method. */
  G_OBJECT_CLASS (gcal_week_view_parent_class)->finalize (object);
}

static void
gcal_week_view_set_property (GObject       *object,
                             guint          property_id,
                             const GValue  *value,
                             GParamSpec    *pspec)
{
  GcalWeekView *self;

  self = GCAL_WEEK_VIEW (object);

  switch (property_id)
    {
    case PROP_DATE:
      {
        time_t range_start, range_end;
        icaltimetype *date;
        icaltimezone* default_zone;

        g_clear_pointer (&self->date, g_free);

        self->date = g_value_dup_boxed (value);

        default_zone = gcal_manager_get_system_timezone (self->manager);
        date = gcal_view_get_initial_date (GCAL_VIEW (object));
        range_start = icaltime_as_timet_with_zone (*date,
                                                   default_zone);
        g_free (date);
        date = gcal_view_get_final_date (GCAL_VIEW (object));
        range_end = icaltime_as_timet_with_zone (*date,
                                                 default_zone);
        g_free (date);
        gcal_manager_set_subscriber (self->manager,
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
  GcalWeekView *self;

  g_return_if_fail (GCAL_IS_WEEK_VIEW (object));
  self = GCAL_WEEK_VIEW (object);

  switch (property_id)
    {
    case PROP_DATE:
      g_value_set_boxed (value, self->date);
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
  GcalWeekView *self;
  icaltimetype *new_date;

  g_return_val_if_fail (GCAL_IS_WEEK_VIEW (view), NULL);
  self = GCAL_WEEK_VIEW(view);
  new_date = g_new0 (icaltimetype, 1);
  *new_date = icaltime_from_day_of_year (icaltime_start_doy_week (*(self->date),
                                                                  self->first_weekday + 1),
                                         self->date->year);
  new_date->is_date = 0;
  new_date->hour = 0;
  new_date->minute = 0;
  new_date->second = 0;
  *new_date = icaltime_set_timezone (new_date, self->date->zone);
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
  GcalWeekView *self;
  icaltimetype *new_date;

  g_return_val_if_fail (GCAL_IS_WEEK_VIEW (view), NULL);
  self = GCAL_WEEK_VIEW(view);
  new_date = g_new0 (icaltimetype, 1);
  *new_date = icaltime_from_day_of_year (icaltime_start_doy_week (*(self->date),
                                                                  self->first_weekday + 1) + 6,
                                         self->date->year);
  new_date->is_date = 0;
  new_date->hour = 23;
  new_date->minute = 59;
  new_date->second = 0;
  *new_date = icaltime_set_timezone (new_date, self->date->zone);
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
gcal_week_view_set_manager (GcalWeekView *self,
                            GcalManager  *manager)
{
  g_return_if_fail (GCAL_IS_WEEK_VIEW (self));

  self->manager = manager;
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
gcal_week_view_set_first_weekday (GcalWeekView *self,
                                  gint          day_nr)
{
  g_return_if_fail (GCAL_IS_WEEK_VIEW (self));

  self->first_weekday = day_nr;
}

/**
 * gcal_week_view_set_use_24h_format:
 * @view:
 * @use_24h:
 *
 * Whether the view will show time using 24h or 12h format
 **/
void
gcal_week_view_set_use_24h_format (GcalWeekView *self,
                                   gboolean      use_24h)
{
  g_return_if_fail (GCAL_IS_WEEK_VIEW (self));

  self->use_24h_format = use_24h;
}

void
gcal_week_view_set_current_date (GcalWeekView *self,
                                 icaltimetype *current_date)
{
  g_return_if_fail (GCAL_IS_WEEK_VIEW (self));

  gcal_week_header_set_current_date (GCAL_WEEK_HEADER (self->header), current_date);

  self->current_date = current_date;
}