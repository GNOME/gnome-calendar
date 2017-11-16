/* gcal-month-popover.c
 *
 * Copyright © 2017 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define G_LOG_DOMAIN "GcalMonthPopover"

#include "gcal-event-widget.h"
#include "gcal-month-popover.h"
#include "gcal-utils.h"

#include <dazzle.h>

#define RATIO_TO_RELATIVE  1.25

struct _GcalMonthPopover
{
  GtkWindow           parent;

  GtkLabel           *day_label;
  GtkWidget          *listbox;
  GtkRevealer        *revealer;
  GtkWidget          *scrolled_window;

  GtkWidget          *relative_to;
  GtkWindow          *transient_for;

  GcalManager        *manager;

  DzlAnimation       *opacity_animation;
  DzlAnimation       *position_animation;
  DzlAnimation       *size_animation;

  gulong              delete_event_handler;
  gulong              configure_event_handler;
  gulong              size_allocate_handler;

  /* Positioning flags - range from [0, 1] */
  gdouble             x_ratio;
  gdouble             y_ratio;

  GDateTime          *date;
};

static void          event_activated_cb                          (GcalEventWidget    *event_widget,
                                                                  GcalMonthPopover   *self);

G_DEFINE_TYPE (GcalMonthPopover, gcal_month_popover, GTK_TYPE_WINDOW)

enum
{
  PROP_0,
  PROP_MANAGER,
  PROP_RELATIVE_TO,
  PROP_X,
  PROP_Y,
  N_PROPS
};

enum
{
  EVENT_ACTIVATED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };
static GParamSpec *properties [N_PROPS] = { NULL, };


/*
 * Auxiliary functions
 */

static void
adjust_margin (GcalMonthPopover *self,
               GdkRectangle     *area)
{
  GtkStyleContext *style_context;
  GtkBorder margin;

  style_context = gtk_widget_get_style_context (gtk_bin_get_child (GTK_BIN (self)));
  gtk_style_context_get_margin (style_context,
                                gtk_style_context_get_state (style_context),
                                &margin);

  area->x -= margin.right;
  area->y -= margin.top;
  area->width += margin.right + margin.left;
  area->height += margin.top + margin.bottom;
}

static void
animate_opacity (GcalMonthPopover *self,
                 gdouble           target)
{
  DzlAnimation *animation;

  if (self->opacity_animation)
    dzl_animation_stop (self->opacity_animation);

  /* Animate the opacity */
  animation = dzl_object_animate (self,
                                  DZL_ANIMATION_EASE_IN_OUT_CUBIC,
                                  250,
                                  gtk_widget_get_frame_clock (GTK_WIDGET (self)),
                                  "opacity", target,
                                  NULL);
  dzl_set_weak_pointer (&self->opacity_animation, animation);
}

static void
animate_position (GcalMonthPopover *self,
                  gdouble           x_ratio,
                  gdouble           y_ratio)
{
  DzlAnimation *animation;

  if (self->position_animation)
    dzl_animation_stop (self->position_animation);

  /* Animate the opacity */
  animation = dzl_object_animate (self,
                                  DZL_ANIMATION_EASE_IN_OUT_CUBIC,
                                  200,
                                  gtk_widget_get_frame_clock (GTK_WIDGET (self)),
                                  "x", x_ratio,
                                  "y", y_ratio,
                                  NULL);
  dzl_set_weak_pointer (&self->position_animation, animation);
}

static void
animate_size (GcalMonthPopover *self,
              gint              target_width,
              gint              target_height)
{
  DzlAnimation *animation;

  if (self->size_animation)
    dzl_animation_stop (self->size_animation);

  /* Animate the size */
  animation = dzl_object_animate (self,
                                  DZL_ANIMATION_EASE_IN_OUT_CUBIC,
                                  200,
                                  gtk_widget_get_frame_clock (GTK_WIDGET (self)),
                                  "width-request", target_width,
                                  "height-request", target_height,
                                  NULL);
  dzl_set_weak_pointer (&self->size_animation, animation);
}

void
update_maximum_height (GcalMonthPopover *self)
{
  GdkWindow *window;
  gint clip_height;

  clip_height = 3000;
  window = gtk_widget_get_window (GTK_WIDGET (self->relative_to));

  if (window)
    {
      GdkDisplay *display;
      GdkMonitor *monitor;

      display = gtk_widget_get_display (GTK_WIDGET (self));
      monitor = gdk_display_get_monitor_at_window (display, window);

      if (monitor)
        {
          GdkRectangle workarea;

          gdk_monitor_get_workarea (monitor, &workarea);

          clip_height = workarea.height / 2;
        }
    }

  g_object_set (self->scrolled_window,
                "max-content-height", clip_height,
                NULL);
}

static void
update_position (GcalMonthPopover *self)
{
  GtkAllocation alloc;
  GtkWidget *toplevel;
  GdkWindow *window;
  gint diff_w;
  gint diff_h;
  gint x;
  gint y;

  if (!gtk_widget_get_realized (GTK_WIDGET (self)))
    return;

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self->relative_to));
  window = gtk_widget_get_window (toplevel);

  gtk_widget_get_allocation (GTK_WIDGET (self->relative_to), &alloc);

  alloc.x = 0;
  alloc.y = 0;

  gtk_widget_translate_coordinates (GTK_WIDGET (self->relative_to), toplevel, 0, 0, &alloc.x, &alloc.y);

  gdk_window_get_position (window, &x, &y);
  alloc.x += x;
  alloc.y += y;

  adjust_margin (self, &alloc);

  diff_w = (RATIO_TO_RELATIVE - 1) * alloc.width * (1.0 - self->x_ratio);
  diff_h = (RATIO_TO_RELATIVE - 1) * alloc.height * (1.0 - self->y_ratio);

  /* Animate the margins, not the (x, y) position) */
  gtk_widget_set_margin_top (gtk_bin_get_child (GTK_BIN (self)), diff_h / 2);

  if (gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL)
    gtk_widget_set_margin_end (gtk_bin_get_child (GTK_BIN (self)), diff_w / 2);
  else
    gtk_widget_set_margin_start (gtk_bin_get_child (GTK_BIN (self)), diff_w / 2);
}

static void
reposition_popover (GcalMonthPopover *self,
                    gboolean          animate)
{
  GtkAllocation alloc;
  GtkWidget *toplevel;
  GdkWindow *window;
  gint diff_w;
  gint diff_h;
  gint x;
  gint y;

  if (!gtk_widget_get_realized (GTK_WIDGET (self)))
    return;

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self->relative_to));
  window = gtk_widget_get_window (toplevel);

  gtk_widget_get_allocation (GTK_WIDGET (self->relative_to), &alloc);

  alloc.x = 0;
  alloc.y = 0;

  gtk_widget_translate_coordinates (GTK_WIDGET (self->relative_to), toplevel, 0, 0, &alloc.x, &alloc.y);

  gdk_window_get_position (window, &x, &y);
  alloc.x += x;
  alloc.y += y;

  adjust_margin (self, &alloc);
  update_maximum_height (self);

  diff_w = (RATIO_TO_RELATIVE - 1) * alloc.width;
  diff_h = (RATIO_TO_RELATIVE - 1) * alloc.height;

  gtk_window_move (GTK_WINDOW (self), alloc.x - diff_w / 2, alloc.y - diff_h / 2);

  if (animate)
    {
      gtk_widget_set_size_request (GTK_WIDGET (self), alloc.width, alloc.height);

      animate_position (self, 1.0, 1.0);
      animate_size (self, alloc.width * RATIO_TO_RELATIVE, alloc.height * RATIO_TO_RELATIVE);
    }
  else
    {
      gtk_widget_set_size_request (GTK_WIDGET (self), alloc.width * RATIO_TO_RELATIVE, alloc.height * RATIO_TO_RELATIVE);
    }
}

static void
update_event_list (GcalMonthPopover *self)
{
  g_autofree icaltimetype *start = NULL;
  g_autofree icaltimetype *end = NULL;
  g_autoptr (GDateTime) start_dt = NULL;
  g_autoptr (GDateTime) end_dt = NULL;
  g_autoptr (GList) events = NULL;
  GList *l;

  gtk_container_foreach (GTK_CONTAINER (self->listbox), (GtkCallback) gtk_widget_destroy, NULL);

  if (!gtk_widget_get_realized (GTK_WIDGET (self)) ||
      !gtk_widget_get_visible (GTK_WIDGET (self)) ||
      !self->date)
    {
      return;
    }

  start_dt = g_date_time_new_local (g_date_time_get_year (self->date),
                                    g_date_time_get_month (self->date),
                                    g_date_time_get_day_of_month (self->date),
                                    0, 0, 0);
  end_dt = g_date_time_add_days (start_dt, 1);

  start = datetime_to_icaltime (start_dt);
  end = datetime_to_icaltime (end_dt);

  events = gcal_manager_get_events (self->manager, start, end);

  for (l = events; l; l = l->next)
    {
      g_autoptr (GDateTime) event_start = NULL;
      g_autoptr (GDateTime) event_end = NULL;
      g_autoptr (GTimeZone) tz = NULL;
      GtkWidget *event_widget;
      GcalEvent *event;

      event = l->data;

      if (gcal_event_get_all_day (event))
        tz = g_time_zone_new_utc ();
      else
        tz = g_time_zone_new_local ();

      event_start = g_date_time_new (tz,
                                     g_date_time_get_year (start_dt),
                                     g_date_time_get_month (start_dt),
                                     g_date_time_get_day_of_month (start_dt),
                                     0, 0, 0);

      event_end = g_date_time_add_days (event_start, 1);

      event_widget = gcal_event_widget_new (event);
      gcal_event_widget_set_date_start (GCAL_EVENT_WIDGET (event_widget), event_start);
      gcal_event_widget_set_date_end (GCAL_EVENT_WIDGET (event_widget), event_end);

      gtk_container_add (GTK_CONTAINER (self->listbox), event_widget);

      g_signal_connect (event_widget,
                        "activate",
                        G_CALLBACK (event_activated_cb),
                        self);
    }

  gtk_widget_show_all (self->listbox);
}


/*
 * GtkListBox functions
 */

static gint
sort_func (GtkListBoxRow *a,
           GtkListBoxRow *b,
           gpointer       user_data)
{
  GcalEventWidget *event_a;
  GcalEventWidget *event_b;

  event_a = GCAL_EVENT_WIDGET (gtk_bin_get_child (GTK_BIN (a)));
  event_b = GCAL_EVENT_WIDGET (gtk_bin_get_child (GTK_BIN (b)));

  return gcal_event_widget_sort_events (event_a, event_b);
}


/*
 * Callbacks
 */

static void
event_activated_cb (GcalEventWidget  *event_widget,
                    GcalMonthPopover *self)
{
  g_signal_emit (self, signals[EVENT_ACTIVATED], 0, event_widget);
}

static void
close_button_clicked_cb (GcalMonthPopover *self)
{
  gcal_month_popover_popdown (self);
}

static void
new_event_button_clicked_cb (GtkWidget        *button,
                             GcalMonthPopover *self)
{
  GActionMap *map;
  GAction *action;

  map = G_ACTION_MAP (g_application_get_default ());
  action = g_action_map_lookup_action (map, "new");

  g_action_activate (action, NULL);
}

static void
revealer_notify_child_revealed_cb (GcalMonthPopover *self,
                                   GParamSpec       *pspec,
                                   GtkRevealer      *revealer)
{
  if (!gtk_revealer_get_reveal_child (self->revealer))
    gtk_widget_hide (GTK_WIDGET (self));
}

static gboolean
transient_for_configure_event_cb (GcalMonthPopover *self,
                                  GdkEvent         *event,
                                  GtkWindow        *toplevel)
{
  gtk_widget_hide (GTK_WIDGET (self));
  return FALSE;
}

static gboolean
transient_for_delete_event_cb (GcalMonthPopover *self,
                               GdkEvent         *event,
                               GtkWindow        *toplevel)
{
  gtk_widget_hide (GTK_WIDGET (self));
  return FALSE;
}

static void
transient_for_size_allocate_cb (GcalMonthPopover *self,
                                GtkAllocation    *allocation,
                                GtkWindow        *toplevel)
{
  reposition_popover (self, FALSE);
}


/*
 * GtkWidget overrides
 */

static void
gcal_month_popover_destroy (GtkWidget *widget)
{
  GcalMonthPopover *self = (GcalMonthPopover *)widget;

  if (self->transient_for)
    {
      g_signal_handler_disconnect (self->transient_for, self->size_allocate_handler);
      g_signal_handler_disconnect (self->transient_for, self->configure_event_handler);
      g_signal_handler_disconnect (self->transient_for, self->delete_event_handler);

      self->size_allocate_handler = 0;
      self->configure_event_handler = 0;
      self->delete_event_handler = 0;

      self->transient_for = NULL;
    }

  if (self->opacity_animation)
    {
      dzl_animation_stop (self->opacity_animation);
      dzl_clear_weak_pointer (&self->opacity_animation);
    }

  if (self->position_animation)
    {
      dzl_animation_stop (self->position_animation);
      dzl_clear_weak_pointer (&self->position_animation);
    }

  if (self->size_animation)
    {
      dzl_animation_stop (self->size_animation);
      dzl_clear_weak_pointer (&self->size_animation);
    }

  gcal_month_popover_set_relative_to (self, NULL);

  GTK_WIDGET_CLASS (gcal_month_popover_parent_class)->destroy (widget);
}

static gboolean
gcal_month_popover_focus_out_event (GtkWidget     *widget,
                                    GdkEventFocus *event_focus)
{
  gcal_month_popover_popdown (GCAL_MONTH_POPOVER (widget));

  return GTK_WIDGET_CLASS (gcal_month_popover_parent_class)->focus_out_event (widget, event_focus);
}

static void
gcal_month_popover_hide (GtkWidget *widget)
{
  GcalMonthPopover *self = (GcalMonthPopover *)widget;

  if (self->transient_for)
    gtk_window_group_remove_window (gtk_window_get_group (self->transient_for), GTK_WINDOW (self));

  g_signal_handler_disconnect (self->transient_for, self->delete_event_handler);
  g_signal_handler_disconnect (self->transient_for, self->size_allocate_handler);
  g_signal_handler_disconnect (self->transient_for, self->configure_event_handler);

  self->delete_event_handler = 0;
  self->size_allocate_handler = 0;
  self->configure_event_handler = 0;

  self->transient_for = NULL;

  gcal_month_popover_popdown (self);

  GTK_WIDGET_CLASS (gcal_month_popover_parent_class)->hide (widget);
}

static void
gcal_month_popover_show (GtkWidget *widget)
{
  GcalMonthPopover *self = (GcalMonthPopover *)widget;

  if (self->relative_to)
    {
      GtkWidget *toplevel;

      toplevel = gtk_widget_get_ancestor (GTK_WIDGET (self->relative_to), GTK_TYPE_WINDOW);

      if (GTK_IS_WINDOW (toplevel))
        {
          self->transient_for = GTK_WINDOW (toplevel);

          gtk_window_group_add_window (gtk_window_get_group (self->transient_for), GTK_WINDOW (self));

          self->delete_event_handler =
            g_signal_connect_object (toplevel,
                                     "delete-event",
                                     G_CALLBACK (transient_for_delete_event_cb),
                                     self,
                                     G_CONNECT_SWAPPED);

          self->size_allocate_handler =
            g_signal_connect_object (toplevel,
                                     "size-allocate",
                                     G_CALLBACK (transient_for_size_allocate_cb),
                                     self,
                                     G_CONNECT_SWAPPED | G_CONNECT_AFTER);

          self->configure_event_handler =
            g_signal_connect_object (toplevel,
                                     "configure-event",
                                     G_CALLBACK (transient_for_configure_event_cb),
                                     self,
                                     G_CONNECT_SWAPPED);

          gtk_window_set_transient_for (GTK_WINDOW (self), self->transient_for);

          reposition_popover (self, FALSE);
        }
    }

  GTK_WIDGET_CLASS (gcal_month_popover_parent_class)->show (widget);
}

static void
gcal_month_popover_realize (GtkWidget *widget)
{
  GcalMonthPopover *self = (GcalMonthPopover *)widget;
  GdkScreen *screen;
  GdkVisual *visual;

  screen = gtk_widget_get_screen (widget);
  visual = gdk_screen_get_rgba_visual (screen);

  if (visual)
    gtk_widget_set_visual (widget, visual);

  GTK_WIDGET_CLASS (gcal_month_popover_parent_class)->realize (widget);

  reposition_popover (self, TRUE);
}


/*
 * GObject overrides
 */

static void
gcal_month_popover_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GcalMonthPopover *self = GCAL_MONTH_POPOVER (object);

  switch (prop_id)
    {
    case PROP_MANAGER:
      g_value_set_object (value, self->manager);
      break;

    case PROP_RELATIVE_TO:
      g_value_set_object (value, self->relative_to);
      break;

    case PROP_X:
      g_value_set_double (value, self->x_ratio);
      break;

    case PROP_Y:
      g_value_set_double (value, self->y_ratio);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_month_popover_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GcalMonthPopover *self = GCAL_MONTH_POPOVER (object);

  switch (prop_id)
    {
    case PROP_MANAGER:
      self->manager = g_value_dup_object (value);
      break;

    case PROP_RELATIVE_TO:
      gcal_month_popover_set_relative_to (self, g_value_get_object (value));
      break;

    case PROP_X:
      self->x_ratio = g_value_get_double (value);
      update_position (self);
      break;


    case PROP_Y:
      self->y_ratio = g_value_get_double (value);
      update_position (self);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_month_popover_class_init (GcalMonthPopoverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = gcal_month_popover_get_property;
  object_class->set_property = gcal_month_popover_set_property;

  widget_class->destroy = gcal_month_popover_destroy;
  widget_class->focus_out_event = gcal_month_popover_focus_out_event;
  widget_class->hide = gcal_month_popover_hide;
  widget_class->show = gcal_month_popover_show;
  widget_class->realize = gcal_month_popover_realize;

  properties [PROP_MANAGER] = g_param_spec_object ("manager",
                                                   "Manager",
                                                   "Manager",
                                                   GCAL_TYPE_MANAGER,
                                                   (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_RELATIVE_TO] = g_param_spec_object ("relative-to",
                                                       "Relative To",
                                                       "The widget to be relative to",
                                                       GTK_TYPE_WIDGET,
                                                       (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_X] = g_param_spec_double ("x",
                                             "X Percent position",
                                             "X Percent position",
                                             0.0,
                                             1.0,
                                             0.0,
                                             (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_Y] = g_param_spec_double ("y",
                                             "Y Percent position",
                                             "Y Percent position",
                                             0.0,
                                             1.0,
                                             0.0,
                                             (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals[EVENT_ACTIVATED] = g_signal_new ("event-activated",
                                           GCAL_TYPE_MONTH_POPOVER,
                                           G_SIGNAL_RUN_FIRST,
                                           0,  NULL, NULL, NULL,
                                           G_TYPE_NONE,
                                           1,
                                           GCAL_TYPE_EVENT_WIDGET);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/month-popover.ui");

  gtk_widget_class_bind_template_callback (widget_class, close_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, new_event_button_clicked_cb);

  gtk_widget_class_bind_template_child (widget_class, GcalMonthPopover, day_label);
  gtk_widget_class_bind_template_child (widget_class, GcalMonthPopover, listbox);
  gtk_widget_class_bind_template_child (widget_class, GcalMonthPopover, revealer);
  gtk_widget_class_bind_template_child (widget_class, GcalMonthPopover, scrolled_window);

  gtk_widget_class_set_css_name (widget_class, "monthpopover");
}

static void
gcal_month_popover_init (GcalMonthPopover *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_window_set_type_hint (GTK_WINDOW (self), GDK_WINDOW_TYPE_HINT_COMBO);
  gtk_window_set_decorated (GTK_WINDOW (self), FALSE);
  gtk_window_set_resizable (GTK_WINDOW (self), FALSE);
  gtk_window_set_skip_pager_hint (GTK_WINDOW (self), TRUE);
  gtk_window_set_skip_taskbar_hint (GTK_WINDOW (self), TRUE);

  g_signal_connect_object (self->revealer,
                           "notify::child-revealed",
                           G_CALLBACK (revealer_notify_child_revealed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  gtk_list_box_set_sort_func (GTK_LIST_BOX (self->listbox), sort_func, NULL, NULL);
}

GtkWidget*
gcal_month_popover_new (void)
{
  return g_object_new (GCAL_TYPE_MONTH_POPOVER,
                       "type", GTK_WINDOW_POPUP,
                       NULL);
}

void
gcal_month_popover_set_relative_to (GcalMonthPopover *self,
                                    GtkWidget        *relative_to)
{
  g_return_if_fail (GCAL_MONTH_POPOVER (self));
  g_return_if_fail (!relative_to || GTK_IS_WIDGET (relative_to));

  if (self->relative_to == relative_to)
    return;

  if (self->relative_to)
    {
      g_signal_handlers_disconnect_by_func (self->relative_to, gtk_widget_destroyed, &self->relative_to);
      self->relative_to = NULL;
    }

  if (relative_to)
    {
      self->relative_to = relative_to;
      g_signal_connect (self->relative_to,
                        "destroy",
                        G_CALLBACK (gtk_widget_destroyed),
                        &self->relative_to);
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_RELATIVE_TO]);
}

void
gcal_month_popover_popup (GcalMonthPopover *self)
{
  gint duration = 250;

  g_return_if_fail (GCAL_IS_MONTH_POPOVER (self));

  if (self->relative_to)
    {
      GtkAllocation alloc;
      GdkDisplay *display;
      GdkMonitor *monitor;
      GdkWindow *window;
      gint min_height;
      gint nat_height;

      display = gtk_widget_get_display (GTK_WIDGET (self->relative_to));
      window = gtk_widget_get_window (GTK_WIDGET (self->relative_to));
      monitor = gdk_display_get_monitor_at_window (display, window);

      gtk_widget_get_preferred_height (GTK_WIDGET (self), &min_height, &nat_height);
      gtk_widget_get_allocation (GTK_WIDGET (self), &alloc);

      duration = dzl_animation_calculate_duration (monitor, alloc.height, nat_height);
    }

  gtk_widget_show (GTK_WIDGET (self));

  gtk_revealer_set_transition_duration (self->revealer, duration);
  gtk_revealer_set_reveal_child (self->revealer, TRUE);

  update_event_list (self);

  gtk_widget_grab_focus (GTK_WIDGET (self));

  /* Animations */
  animate_opacity (self, 1.0);
  reposition_popover (self, TRUE);
}

void
gcal_month_popover_popdown (GcalMonthPopover *self)
{
  GtkAllocation alloc;
  GdkDisplay *display;
  GdkMonitor *monitor;
  GdkWindow *window;
  guint duration;

  g_return_if_fail (GCAL_IS_MONTH_POPOVER (self));

  if (!gtk_widget_get_realized (GTK_WIDGET (self)))
    return;

  display = gtk_widget_get_display (GTK_WIDGET (self->relative_to));
  window = gtk_widget_get_window (GTK_WIDGET (self->relative_to));
  monitor = gdk_display_get_monitor_at_window (display, window);

  gtk_widget_get_allocation (GTK_WIDGET (self), &alloc);

  duration = dzl_animation_calculate_duration (monitor, alloc.height, 0);

  gtk_revealer_set_transition_duration (self->revealer, duration);
  gtk_revealer_set_reveal_child (self->revealer, FALSE);

  /* Animations */
  gtk_widget_get_allocation (GTK_WIDGET (self->relative_to), &alloc);
  gtk_widget_translate_coordinates (self->relative_to,
                                    gtk_widget_get_toplevel (self->relative_to), 0, 0, &alloc.x, &alloc.y);
  adjust_margin (self, &alloc);

  animate_opacity (self, 0.0);
  animate_position (self, 0.0, 0.0);
  animate_size (self, alloc.width, alloc.height);
}

GDateTime*
gcal_month_popover_get_date (GcalMonthPopover *self)
{
  g_return_val_if_fail (GCAL_IS_MONTH_POPOVER (self), NULL);

  return self->date;
}

void
gcal_month_popover_set_date (GcalMonthPopover *self,
                             GDateTime        *date)
{
  g_return_if_fail (GCAL_IS_MONTH_POPOVER (self));

  if (date && self->date && datetime_compare_date (self->date, date) == 0)
    return;

  gcal_clear_datetime (&self->date);

  if (date)
    {
      g_autofree gchar *label;

      self->date = g_date_time_ref (date);

      label = g_strdup_printf ("%d", g_date_time_get_day_of_month (date));
      gtk_label_set_label (self->day_label, label);
    }

  update_event_list (self);
}
