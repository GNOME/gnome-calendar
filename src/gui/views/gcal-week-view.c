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
#include "gcal-view-private.h"
#include "gcal-week-header.h"
#include "gcal-week-hour-bar.h"
#include "gcal-week-grid.h"
#include "gcal-week-view.h"

#include <adwaita.h>
#include <glib/gi18n.h>

#include <math.h>

struct _GcalWeekView
{
  GtkBox              parent;

  GtkWidget          *content;
  GtkWidget          *header;
  GcalWeekHourBar    *hours_bar;
  GtkWidget          *scrolled_window;
  GtkWidget          *week_grid;

  /* property */
  GDateTime          *date;
  GcalContext        *context;

  guint               scroll_grid_timeout_id;
  gulong              stack_page_changed_id;

  gint                clicked_cell;

  gdouble             gesture_zoom_center;

  gboolean            pointer_position_valid;
  gdouble             pointer_position_y;

  gdouble             initial_zoom_level;
  gdouble             zoom_level;
};

static void          stack_visible_child_changed_cb              (AdwViewStack       *stack,
                                                                  GParamSpec         *pspec,
                                                                  GcalWeekView       *self);

static void          gcal_view_interface_init                    (GcalViewInterface  *iface);

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
                         G_IMPLEMENT_INTERFACE (GCAL_TYPE_TIMELINE_SUBSCRIBER,
                                                gcal_timeline_subscriber_interface_init));

/* 60px * 1 * 48 rows + 1px * 47 lines = 2927px */
#define HEIGHT_DEFAULT 2927

#define MIN_ZOOM_LEVEL 0.334
#define MAX_ZOOM_LEVEL 3.0

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
      if (self->stack_page_changed_id == 0)
        {
          GtkWidget *stack = gtk_widget_get_ancestor (GTK_WIDGET (self), ADW_TYPE_VIEW_STACK);

          self->stack_page_changed_id = g_signal_connect_object (stack,
                                                                 "notify::visible-child",
                                                                 G_CALLBACK (stack_visible_child_changed_cb),
                                                                 self,
                                                                 0);
        }

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

static void
begin_zoom (GcalWeekView *self,
            gdouble       view_center_y)
{
  GtkAdjustment *vadjustment;
  gdouble center, height;

  vadjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (self->scrolled_window));

  center = gtk_adjustment_get_value (vadjustment) + view_center_y - gtk_adjustment_get_lower (vadjustment);
  height = gtk_adjustment_get_upper (vadjustment) - gtk_adjustment_get_lower (vadjustment);

  self->gesture_zoom_center = center / height;

  self->initial_zoom_level = self->zoom_level;
}

static void
apply_zoom (GcalWeekView *self,
            gdouble       view_center_y,
            gdouble       scale)
{
  GtkAdjustment *vadjustment;
  gdouble height;

  self->zoom_level = CLAMP (self->initial_zoom_level + (scale - 1.0),
                            MIN_ZOOM_LEVEL,
                            MAX_ZOOM_LEVEL);

  vadjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (self->scrolled_window));

  height = HEIGHT_DEFAULT * self->zoom_level;

  gtk_widget_set_size_request (self->content, -1, height);
  gtk_adjustment_set_lower (vadjustment, 0);
  gtk_adjustment_set_upper (vadjustment, height);
  gtk_adjustment_set_value (vadjustment, (self->gesture_zoom_center * height) - view_center_y);
}

static void
save_zoom_level (GcalWeekView *self)
{
  GSettings *settings;

  g_assert (self->context != NULL);

  settings = gcal_context_get_settings (self->context);
  g_settings_set_double (settings, "week-view-zoom-level", self->zoom_level);
}

static void
restore_zoom_level (GcalWeekView *self)
{
  GSettings *settings;

  g_assert (self->context != NULL);

  settings = gcal_context_get_settings (self->context);
  self->zoom_level = g_settings_get_double (settings, "week-view-zoom-level");

  begin_zoom (self, 0);
  apply_zoom (self, 0, 1.0);
}


/* Callbacks */
static void
on_event_activated (GcalWeekView *self,
                    GtkWidget    *widget)
{
  gcal_view_event_activated (GCAL_VIEW (self), GCAL_EVENT_WIDGET (widget));
}

static void
on_motion_controller_enter_cb (GtkEventControllerMotion *controller,
                               gdouble                   x,
                               gdouble                   y,
                               GcalWeekView             *self)
{
  self->pointer_position_valid = TRUE;
  self->pointer_position_y = y;
}

static void
on_motion_controller_motion_cb (GtkEventControllerMotion *controller,
                                gdouble                   x,
                                gdouble                   y,
                                GcalWeekView             *self)
{
  self->pointer_position_valid = TRUE;
  self->pointer_position_y = y;
}

static void
on_motion_controller_leave_cb (GtkEventControllerMotion *controller,
                               GcalWeekView             *self)
{
  self->pointer_position_valid = FALSE;
}

static void
on_scroll_controller_scroll_begin_cb (GtkEventControllerScroll *controller,
                                      GcalWeekView             *self)
{
  gdouble view_center_y;

  if (self->pointer_position_valid)
    view_center_y = self->pointer_position_y;
  else
    view_center_y = gtk_widget_get_height (self->scrolled_window) / 2.0;

  begin_zoom (self, view_center_y);
}

static gboolean
on_scroll_controller_scroll_cb (GtkEventControllerScroll *controller,
                                gdouble                   dx,
                                gdouble                   dy,
                                GcalWeekView             *self)
{
  GdkEvent *event;
  gboolean discrete;
  gdouble view_center_y;
  gdouble scale;

  if (!(gtk_event_controller_get_current_event_state (GTK_EVENT_CONTROLLER (controller)) & GDK_CONTROL_MASK))
    return FALSE;

  event = gtk_event_controller_get_current_event (GTK_EVENT_CONTROLLER (controller));

  switch (gdk_scroll_event_get_direction (event))
    {
    case GDK_SCROLL_SMOOTH:
      scale = dy / 100.0 + 1.0;
      discrete = FALSE;
      break;

    case GDK_SCROLL_UP:
    case GDK_SCROLL_DOWN:
      scale = dy / 10.0 + 1.0;
      discrete = TRUE;
      break;

    case GDK_SCROLL_LEFT:
    case GDK_SCROLL_RIGHT:
    default:
      g_assert_not_reached ();
    }

  if (self->pointer_position_valid)
    view_center_y = self->pointer_position_y;
  else
    view_center_y = gtk_widget_get_height (self->scrolled_window) / 2.0;

  if (discrete)
    begin_zoom (self, view_center_y);

  apply_zoom (self, view_center_y, scale);

  if (discrete)
    save_zoom_level (self);

  return TRUE;
}

static void
on_scroll_controller_scroll_end_cb (GtkEventControllerScroll *controller,
                                    GcalWeekView             *self)
{
  save_zoom_level (self);
}

static void
on_zoom_gesture_scale_changed_cb (GtkGestureZoom *gesture,
                                  gdouble         scale,
                                  GcalWeekView   *self)
{
  gdouble view_center_x, view_center_y;

  gtk_gesture_get_bounding_box_center (GTK_GESTURE (gesture), &view_center_x, &view_center_y);

  apply_zoom (self, view_center_y, scale);
}

static void
on_zoom_gesture_begin_cb (GtkGesture       *gesture,
                          GdkEventSequence *sequence,
                          GcalWeekView     *self)
{
  gdouble view_center_x, view_center_y;

  gtk_gesture_get_bounding_box_center (gesture, &view_center_x, &view_center_y);

  begin_zoom (self, view_center_y);
}

static void
on_zoom_gesture_end_cb (GtkGesture       *gesture,
                        GdkEventSequence *sequence,
                        GcalWeekView     *self)
{
  save_zoom_level (self);
}

static void
stack_visible_child_changed_cb (AdwViewStack *stack,
                                GParamSpec   *pspec,
                                GcalWeekView *self)
{
  if (adw_view_stack_get_visible_child (stack) != (GtkWidget*) self)
    return;

  schedule_position_scroll (self);

  g_clear_signal_handler (&self->stack_page_changed_id, stack);
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
  iface->get_next_date = gcal_week_view_get_next_date;
  iface->get_previous_date = gcal_week_view_get_previous_date;
}


/*
 * GcalTimelineSubscriber iface
 */

static GcalRange*
gcal_week_view_get_range (GcalTimelineSubscriber *subscriber)
{
  GcalWeekView *self = GCAL_WEEK_VIEW (subscriber);

  return gcal_range_new_take (gcal_date_time_get_start_of_week (self->date),
                              gcal_date_time_get_end_of_week (self->date),
                              GCAL_RANGE_DEFAULT);
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
                             GcalEvent              *old_event,
                             GcalEvent              *event)
{
  GcalWeekView *self = GCAL_WEEK_VIEW (subscriber);
  const gchar *event_id;

  GCAL_ENTRY;

  event_id = gcal_event_get_uid (old_event);
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
  iface->get_range = gcal_week_view_get_range;
  iface->add_event = gcal_week_view_add_event;
  iface->update_event = gcal_week_view_update_event;
  iface->remove_event = gcal_week_view_remove_event;
}


/*
 * GObject overrides
 */

static void
gcal_week_view_finalize (GObject       *object)
{
  GcalWeekView *self;

  self = GCAL_WEEK_VIEW (object);

  g_clear_pointer (&self->date, g_date_time_unref);

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
      gcal_week_hour_bar_set_context (self->hours_bar, self->context);
      restore_zoom_level (self);
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
  g_type_ensure (GCAL_TYPE_WEEK_HOUR_BAR);

  object_class->finalize = gcal_week_view_finalize;
  object_class->set_property = gcal_week_view_set_property;
  object_class->get_property = gcal_week_view_get_property;

  g_object_class_override_property (object_class, PROP_DATE, "active-date");
  g_object_class_override_property (object_class, PROP_CONTEXT, "context");

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/views/gcal-week-view.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalWeekView, content);
  gtk_widget_class_bind_template_child (widget_class, GcalWeekView, header);
  gtk_widget_class_bind_template_child (widget_class, GcalWeekView, hours_bar);
  gtk_widget_class_bind_template_child (widget_class, GcalWeekView, scrolled_window);
  gtk_widget_class_bind_template_child (widget_class, GcalWeekView, week_grid);

  gtk_widget_class_bind_template_callback (widget_class, on_event_activated);
  gtk_widget_class_bind_template_callback (widget_class, on_motion_controller_enter_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_motion_controller_motion_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_motion_controller_leave_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_scroll_controller_scroll_begin_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_scroll_controller_scroll_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_scroll_controller_scroll_end_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_zoom_gesture_scale_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_zoom_gesture_begin_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_zoom_gesture_end_cb);

  gtk_widget_class_set_css_name (widget_class, "calendar-view");
}

static void
gcal_week_view_init (GcalWeekView *self)
{
  GtkSizeGroup *size_group;

  gtk_widget_init_template (GTK_WIDGET (self));

  self->zoom_level = 1.0;

  gtk_widget_set_size_request (self->content, -1, HEIGHT_DEFAULT);

  size_group = gcal_week_header_get_sidebar_size_group (GCAL_WEEK_HEADER (self->header));
  gtk_size_group_add_widget (size_group, GTK_WIDGET (self->hours_bar));
}

