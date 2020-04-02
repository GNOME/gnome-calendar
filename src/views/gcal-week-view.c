/* gcal-week-view.c
 *
 * Copyright (C) 2015 - Erick Pérez Castellanos
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
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

#define G_LOG_DOMAIN "GcalWeekView"

#include "gcal-debug.h"
#include "gcal-enums.h"
#include "gcal-event-widget.h"
#include "gcal-timeline-subscriber.h"
#include "gcal-utils.h"
#include "gcal-view.h"
#include "gcal-week-header.h"
#include "gcal-week-grid.h"
#include "gcal-week-view.h"

#include <glib/gi18n.h>

#include <math.h>

#define ALL_DAY_CELLS_HEIGHT 40

static const double dashed [] =
{
  5.0,
  6.0
};

struct _GcalWeekView
{
  GtkBox              parent;

  GtkWidget          *header;
  GtkWidget          *hours_bar;
  GtkWidget          *scrolled_window;
  GtkWidget          *week_grid;

  /* property */
  GDateTime          *date;
  GcalContext        *context;

  guint               scroll_grid_timeout_id;

  gint                clicked_cell;
};

static void          schedule_position_scroll                    (GcalWeekView       *self);

static void          gcal_view_interface_init                    (GcalViewInterface  *iface);

static void          gcal_data_model_subscriber_interface_init   (ECalDataModelSubscriberInterface *iface);

static void          gcal_timeline_subscriber_interface_init     (GcalTimelineSubscriberInterface *iface);

enum
{
  PROP_0,
  PROP_DATE,
  PROP_CONTEXT,
  NUM_PROPS
};


G_DEFINE_TYPE_WITH_CODE (GcalWeekView, gcal_week_view, GTK_TYPE_BOX,
                         G_IMPLEMENT_INTERFACE (GCAL_TYPE_VIEW, gcal_view_interface_init)
                         G_IMPLEMENT_INTERFACE (E_TYPE_CAL_DATA_MODEL_SUBSCRIBER,
                                                gcal_data_model_subscriber_interface_init)
                         G_IMPLEMENT_INTERFACE (GCAL_TYPE_TIMELINE_SUBSCRIBER,
                                                gcal_timeline_subscriber_interface_init));

/* Callbacks */
static void
on_event_activated (GcalWeekView *self,
                    GtkWidget    *widget)
{
  g_signal_emit_by_name (self, "event-activated", widget);
}

static void
stack_visible_child_changed_cb (GtkStack     *stack,
                                GParamSpec   *pspec,
                                GcalWeekView *self)
{
  if (gtk_stack_get_visible_child (stack) != (GtkWidget*) self)
    return;

  schedule_position_scroll (self);

  g_signal_handlers_disconnect_by_func (stack, stack_visible_child_changed_cb, self);
}

/* Auxiliary methods */
static gboolean
update_grid_scroll_position (GcalWeekView *self)
{
  g_autoptr(GDateTime) week_start = NULL;
  g_autoptr(GDateTime) week_end = NULL;
  g_autoptr(GDateTime) now = NULL;
  GtkAdjustment *vadjustment;
  gdouble minutes, real_value;
  gdouble max, page, page_increment, value;
  gboolean dummy;

  /* While the scrolled window is not mapped, we keep waiting */
  if (!gtk_widget_get_realized (self->scrolled_window) ||
      !gtk_widget_get_mapped (self->scrolled_window))
    {
      GtkWidget *stack;

      stack = gtk_widget_get_ancestor (GTK_WIDGET (self), GTK_TYPE_STACK);

      g_signal_connect_object (stack,
                               "notify::visible-child",
                               G_CALLBACK (stack_visible_child_changed_cb),
                               self,
                               0);

      self->scroll_grid_timeout_id = 0;

      GCAL_RETURN (G_SOURCE_REMOVE);
    }

  now = g_date_time_new_now_local ();
  week_start = gcal_date_time_get_start_of_week (self->date);
  week_end = gcal_date_time_get_end_of_week (self->date);

  /* Don't animate when not today */
  if (gcal_date_time_compare_date (now, week_start) < 0 || gcal_date_time_compare_date (now, week_end) >= 0)
    GCAL_GOTO (out);

  vadjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (self->scrolled_window));
  minutes = g_date_time_get_hour (now) * 60 + g_date_time_get_minute (now);
  page = gtk_adjustment_get_page_size (vadjustment);
  max = gtk_adjustment_get_upper (vadjustment);

  real_value = max / MINUTES_PER_DAY * minutes - (page / 2.0);
  page_increment = gtk_adjustment_get_page_increment (vadjustment);
  value = gtk_adjustment_get_value (vadjustment);

  gtk_adjustment_set_page_increment (vadjustment, real_value - value);

  g_signal_emit_by_name (self->scrolled_window,
                         "scroll-child",
                         GTK_SCROLL_PAGE_FORWARD,
                         FALSE,
                         &dummy);

  gtk_adjustment_set_page_increment (vadjustment, page_increment);

out:
  self->scroll_grid_timeout_id = 0;
  GCAL_RETURN (G_SOURCE_REMOVE);
}

static void
schedule_position_scroll (GcalWeekView *self)
{
  /* Nothing is scheduled, we should do it now */
  if (self->scroll_grid_timeout_id > 0)
    g_source_remove (self->scroll_grid_timeout_id);

  self->scroll_grid_timeout_id = g_timeout_add (200,
                                                (GSourceFunc) update_grid_scroll_position,
                                                self);
}

/* GcalView implementation */
static GDateTime*
gcal_week_view_get_date (GcalView *view)
{
  GcalWeekView *self = GCAL_WEEK_VIEW (view);

  return self->date;
}

static void
gcal_week_view_set_date (GcalView  *view,
                         GDateTime *date)
{
  GcalWeekView *self = GCAL_WEEK_VIEW (view);

  GCAL_ENTRY;

  gcal_set_date_time (&self->date, date);

  /* Propagate the new date */
  gcal_week_grid_set_date (GCAL_WEEK_GRID (self->week_grid), date);
  gcal_week_header_set_date (GCAL_WEEK_HEADER (self->header), date);

  schedule_position_scroll (self);

  gcal_timeline_subscriber_range_changed (GCAL_TIMELINE_SUBSCRIBER (view));

  GCAL_EXIT;
}

static GList*
gcal_week_view_get_children_by_uuid (GcalView              *view,
                                     GcalRecurrenceModType  mod,
                                     const gchar           *uuid)
{
  GcalWeekView *self;
  GList *grid, *header;

  GCAL_ENTRY;

  self = GCAL_WEEK_VIEW (view);
  grid = gcal_week_grid_get_children_by_uuid (GCAL_WEEK_GRID (self->week_grid), mod, uuid);
  header = gcal_week_header_get_children_by_uuid (GCAL_WEEK_HEADER (self->header), mod, uuid);

  GCAL_RETURN (g_list_concat (grid, header));
}

static void
gcal_week_view_clear_marks (GcalView *view)
{
  GcalWeekView *self = GCAL_WEEK_VIEW (view);

  gcal_week_header_clear_marks (GCAL_WEEK_HEADER (self->header));
  gcal_week_grid_clear_marks (GCAL_WEEK_GRID (self->week_grid));
}

static void
gcal_week_view_update_subscription (GcalView *view)
{
  g_autoptr (GDateTime) date_start = NULL;
  g_autoptr (GDateTime) date_end = NULL;
  GcalWeekView *self;
  time_t range_start;
  time_t range_end;

  self = GCAL_WEEK_VIEW (view);
  date_start = gcal_date_time_get_start_of_week (self->date);
  range_start = g_date_time_to_unix (date_start);

  date_end = gcal_date_time_get_end_of_week (self->date);
  range_end = g_date_time_to_unix (date_end);

  gcal_manager_set_subscriber (gcal_context_get_manager (self->context),
                               E_CAL_DATA_MODEL_SUBSCRIBER (self),
                               range_start,
                               range_end);
}

static GDateTime*
gcal_week_view_get_next_date (GcalView *view)
{
  GcalWeekView *self = GCAL_WEEK_VIEW (view);

  g_assert (self->date != NULL);
  return g_date_time_add_weeks (self->date, 1);
}


static GDateTime*
gcal_week_view_get_previous_date (GcalView *view)
{
  GcalWeekView *self = GCAL_WEEK_VIEW (view);

  g_assert (self->date != NULL);
  return g_date_time_add_weeks (self->date, -1);
}

static void
gcal_view_interface_init (GcalViewInterface *iface)
{
  iface->get_date = gcal_week_view_get_date;
  iface->set_date = gcal_week_view_set_date;
  iface->get_children_by_uuid = gcal_week_view_get_children_by_uuid;
  iface->clear_marks = gcal_week_view_clear_marks;
  iface->update_subscription = gcal_week_view_update_subscription;
  iface->get_next_date = gcal_week_view_get_next_date;
  iface->get_previous_date = gcal_week_view_get_previous_date;
}

static void
update_hours_sidebar_size (GcalWeekView *self)
{
  GtkStyleContext *context;
  GtkStateFlags state;
  GtkSizeGroup *sidebar_sizegroup;
  GtkWidget *widget;
  GtkBorder padding;

  PangoLayout *layout;
  PangoFontDescription *font_desc;

  gint hours_12_width, hours_24_width, sidebar_width;
  gint hours_12_height, hours_24_height, cell_height;

  widget = GTK_WIDGET (self);
  context = gtk_widget_get_style_context (widget);
  state = gtk_style_context_get_state (context);

  gtk_style_context_save (context);
  gtk_style_context_add_class (context, "hours");

  gtk_style_context_get (context, state,
                         "font", &font_desc,
                         NULL);
  gtk_style_context_get_padding (context, state, &padding);

  layout = pango_layout_new (gtk_widget_get_pango_context (widget));
  pango_layout_set_font_description (layout, font_desc);

  pango_layout_set_text (layout, _("00 AM"), -1);
  pango_layout_get_pixel_size (layout, &hours_12_width, &hours_12_height);

  pango_layout_set_text (layout, _("00:00"), -1);
  pango_layout_get_pixel_size (layout, &hours_24_width, &hours_24_height);

  sidebar_width = MAX (hours_12_width, hours_24_width) + padding.left + padding.right;
  cell_height = MAX (hours_12_height, hours_24_height) + padding.top + padding.bottom + 1;

  gtk_style_context_restore (context);

  /* Update the size requests */
  gtk_widget_set_size_request (self->hours_bar,
                               sidebar_width,
                               48 * cell_height);

  /* Sync with the week header sidebar */
  sidebar_sizegroup = gcal_week_header_get_sidebar_size_group (GCAL_WEEK_HEADER (self->header));
  gtk_size_group_add_widget (sidebar_sizegroup, self->hours_bar);

  pango_font_description_free (font_desc);
  g_object_unref (layout);
}

/*
 * GcalTimelineSubscriber iface
 */

static GDateTime*
gcal_week_view_get_range_start (GcalTimelineSubscriber *subscriber)
{
  GcalWeekView *self = GCAL_WEEK_VIEW (subscriber);

  return gcal_date_time_get_start_of_week (self->date);
}

static GDateTime*
gcal_week_view_get_range_end (GcalTimelineSubscriber *subscriber)
{
  GcalWeekView *self = GCAL_WEEK_VIEW (subscriber);

  return gcal_date_time_get_end_of_week (self->date);
}

static void
gcal_week_view_add_event (GcalTimelineSubscriber *subscriber,
                          GcalEvent              *event)
{
  GcalWeekView *self = GCAL_WEEK_VIEW (subscriber);

  GCAL_ENTRY;

  if (gcal_event_is_multiday (event) || gcal_event_get_all_day (event))
    gcal_week_header_add_event (GCAL_WEEK_HEADER (self->header), event);
  else
    gcal_week_grid_add_event (GCAL_WEEK_GRID (self->week_grid), event);

  GCAL_EXIT;
}

static void
gcal_week_view_update_event (GcalTimelineSubscriber *subscriber,
                             GcalEvent              *event)
{
  GcalWeekView *self = GCAL_WEEK_VIEW (subscriber);
  const gchar *event_id;

  GCAL_ENTRY;

  event_id = gcal_event_get_uid (event);
  gcal_week_header_remove_event (GCAL_WEEK_HEADER (self->header), event_id);
  gcal_week_grid_remove_event (GCAL_WEEK_GRID (self->week_grid), event_id);

  gcal_week_view_add_event (subscriber, event);

  GCAL_EXIT;
}

static void
gcal_week_view_remove_event (GcalTimelineSubscriber *subscriber,
                             GcalEvent              *event)
{
  GcalWeekView *self = GCAL_WEEK_VIEW (subscriber);
  const gchar *uuid = gcal_event_get_uid (event);

  GCAL_ENTRY;

  gcal_week_header_remove_event (GCAL_WEEK_HEADER (self->header), uuid);
  gcal_week_grid_remove_event (GCAL_WEEK_GRID (self->week_grid), uuid);

  GCAL_EXIT;
}

static void
gcal_timeline_subscriber_interface_init (GcalTimelineSubscriberInterface *iface)
{
  iface->get_range_start = gcal_week_view_get_range_start;
  iface->get_range_end = gcal_week_view_get_range_end;
  iface->add_event = gcal_week_view_add_event;
  iface->update_event = gcal_week_view_update_event;
  iface->remove_event = gcal_week_view_remove_event;
}


/* ECalDataModelSubscriber implementation */
static void
gcal_week_view_component_added (ECalDataModelSubscriber *subscriber,
                                ECalClient              *client,
                                ECalComponent           *comp)
{
  GcalWeekView *self = GCAL_WEEK_VIEW (subscriber);
  g_autoptr (GcalEvent) event = NULL;
  GcalCalendar *calendar;

  GCAL_ENTRY;

  calendar = gcal_manager_get_calendar_from_source (gcal_context_get_manager (self->context),
                                                    e_client_get_source (E_CLIENT (client)));
  event = gcal_event_new (calendar, comp, NULL);

  gcal_week_view_add_event (GCAL_TIMELINE_SUBSCRIBER (subscriber), event);

  GCAL_EXIT;
}

static void
gcal_week_view_component_modified (ECalDataModelSubscriber *subscriber,
                                   ECalClient              *client,
                                   ECalComponent           *comp)
{
  GcalWeekView *self = GCAL_WEEK_VIEW (subscriber);
  GcalWeekHeader *header;
  gchar *uuid;

  GCAL_ENTRY;

  header = GCAL_WEEK_HEADER (self->header);

  uuid = get_uuid_from_component (e_client_get_source (E_CLIENT (client)), comp);

  gcal_week_header_remove_event (header, uuid);
  gcal_week_grid_remove_event (GCAL_WEEK_GRID (self->week_grid), uuid);

  gcal_week_view_component_added (subscriber, client, comp);

  g_free (uuid);

  GCAL_EXIT;
}

static void
gcal_week_view_component_removed (ECalDataModelSubscriber *subscriber,
                                  ECalClient              *client,
                                  const gchar             *uid,
                                  const gchar             *rid)
{
  GcalWeekView *self = GCAL_WEEK_VIEW (subscriber);
  ESource *source;
  gchar *uuid;

  GCAL_ENTRY;

  source = e_client_get_source (E_CLIENT (client));

  if (rid != NULL)
    uuid = g_strdup_printf ("%s:%s:%s", e_source_get_uid (source), uid, rid);
  else
    uuid = g_strdup_printf ("%s:%s", e_source_get_uid (source), uid);

  gcal_week_header_remove_event (GCAL_WEEK_HEADER (self->header), uuid);
  gcal_week_grid_remove_event (GCAL_WEEK_GRID (self->week_grid), uuid);

  g_free (uuid);

  GCAL_EXIT;
}

static void
gcal_week_view_freeze (ECalDataModelSubscriber *subscriber)
{
}

static void
gcal_week_view_thaw (ECalDataModelSubscriber *subscriber)
{
}

static gboolean
gcal_week_view_draw_hours (GcalWeekView *self,
                           cairo_t      *cr,
                           GtkWidget    *widget)
{
  GtkStyleContext *context;
  GcalTimeFormat time_format;
  GtkStateFlags state;
  GtkBorder padding;
  GdkRGBA color;

  gboolean ltr;
  gint i, width, height;
  gint font_width;

  PangoLayout *layout;
  PangoFontDescription *font_desc;

  time_format = gcal_context_get_time_format (self->context);
  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);
  ltr = gtk_widget_get_direction (widget) != GTK_TEXT_DIR_RTL;

  gtk_style_context_save (context);

  gtk_style_context_add_class (context, "hours");
  gtk_style_context_get_color (context, state, &color);
  gtk_style_context_get_padding (context, state, &padding);
  gtk_style_context_get (context, state, "font", &font_desc, NULL);

  layout = pango_cairo_create_layout (cr);
  pango_layout_set_font_description (layout, font_desc);
  gdk_cairo_set_source_rgba (cr, &color);

  /* Gets the size of the widget */
  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);

  /* Draws the hours in the sidebar */
  for (i = 0; i < 24; i++)
    {
      gchar *hours;

      if (time_format == GCAL_TIME_FORMAT_24H)
        {
          hours = g_strdup_printf ("%02d:00", i);
        }
      else
        {
          hours = g_strdup_printf ("%d %s",
                                   i % 12 == 0 ? 12 : i % 12,
                                   i >= 12 ? _("PM") : _("AM"));
        }

      pango_layout_set_text (layout, hours, -1);
      pango_layout_get_pixel_size (layout, &font_width, NULL);

      gtk_render_layout (context,
                         cr,
                         ltr ? padding.left : width - font_width - padding.right,
                         (height / 24) * i + padding.top,
                         layout);

      g_free (hours);
    }

  gtk_style_context_restore (context);

  gtk_style_context_save (context);
  gtk_style_context_add_class (context, "lines");
  gtk_style_context_get_color (context, state, &color);

  gdk_cairo_set_source_rgba (cr, &color);
  cairo_set_line_width (cr, 0.65);

  if (!ltr)
    {
      cairo_move_to (cr, 0.5, 0);
      cairo_rel_line_to (cr, 0, height);
    }

  /* Draws the horizontal complete lines */
  for (i = 1; i < 24; i++)
    {
      cairo_move_to (cr, 0, (height / 24) * i + 0.4);
      cairo_rel_line_to (cr, width, 0);
    }

  cairo_stroke (cr);

  cairo_set_dash (cr, dashed, 2, 0);

  /* Draws the horizontal dashed lines */
  for (i = 0; i < 24; i++)
    {
      cairo_move_to (cr, 0, (height / 24) * i + (height / 48) + 0.4);
      cairo_rel_line_to (cr, width, 0);
    }

  cairo_stroke (cr);

  gtk_style_context_restore (context);

  pango_font_description_free (font_desc);
  g_object_unref (layout);

  return FALSE;
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
gcal_week_view_finalize (GObject       *object)
{
  GcalWeekView *self;

  self = GCAL_WEEK_VIEW (object);

  g_clear_pointer (&self->date, g_free);

  g_clear_object (&self->context);

  /* Chain up to parent's finalize() method. */
  G_OBJECT_CLASS (gcal_week_view_parent_class)->finalize (object);
}

static void
gcal_week_view_set_property (GObject       *object,
                             guint          property_id,
                             const GValue  *value,
                             GParamSpec    *pspec)
{
  GcalWeekView *self = (GcalWeekView *) object;

  switch (property_id)
    {
    case PROP_DATE:
      gcal_view_set_date (GCAL_VIEW (object), g_value_get_boxed (value));
      break;

    case PROP_CONTEXT:
      g_assert (self->context == NULL);
      self->context = g_value_dup_object (value);

      gcal_week_grid_set_context (GCAL_WEEK_GRID (self->week_grid), self->context);
      gcal_week_header_set_context (GCAL_WEEK_HEADER (self->header), self->context);

      g_signal_connect_object (self->context,
                               "notify::time-format",
                               G_CALLBACK (gtk_widget_queue_draw),
                               self->hours_bar,
                               G_CONNECT_SWAPPED);
      break;

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

    case PROP_CONTEXT:
      g_value_set_object (value, self->context);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gcal_week_view_class_init (GcalWeekViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  g_type_ensure (GCAL_TYPE_WEEK_GRID);
  g_type_ensure (GCAL_TYPE_WEEK_HEADER);

  object_class->finalize = gcal_week_view_finalize;
  object_class->set_property = gcal_week_view_set_property;
  object_class->get_property = gcal_week_view_get_property;

  g_object_class_override_property (object_class, PROP_DATE, "active-date");
  g_object_class_override_property (object_class, PROP_CONTEXT, "context");

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/views/gcal-week-view.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalWeekView, header);
  gtk_widget_class_bind_template_child (widget_class, GcalWeekView, hours_bar);
  gtk_widget_class_bind_template_child (widget_class, GcalWeekView, scrolled_window);
  gtk_widget_class_bind_template_child (widget_class, GcalWeekView, week_grid);

  gtk_widget_class_bind_template_callback (widget_class, gcal_week_view_draw_hours);
  gtk_widget_class_bind_template_callback (widget_class, on_event_activated);

  gtk_widget_class_set_css_name (widget_class, "calendar-view");
}

static void
gcal_week_view_init (GcalWeekView *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  update_hours_sidebar_size (self);
}

