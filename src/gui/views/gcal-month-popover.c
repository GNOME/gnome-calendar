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

#include "gcal-context.h"
#include "gcal-debug.h"
#include "gcal-event-widget.h"
#include "gcal-month-popover.h"
#include "gcal-utils.h"

#include <adwaita.h>

#define RATIO_TO_RELATIVE  1.25
#define MIN_WIDTH          250

struct _GcalMonthPopover
{
  GtkWidget           parent;

  GtkLabel           *day_label;
  GtkListBox         *listbox;
  GtkWidget          *main_box;

  GcalContext        *context;

  GDateTime          *date;
  AdwAnimation       *animation;
};

static void          event_activated_cb                          (GcalEventWidget    *event_widget,
                                                                  GcalMonthPopover   *self);

G_DEFINE_TYPE (GcalMonthPopover, gcal_month_popover, GTK_TYPE_WIDGET)

enum
{
  PROP_0,
  PROP_CONTEXT,
  N_PROPS,
};

enum
{
  EVENT_ACTIVATED,
  LAST_SIGNAL,
};

static guint signals[LAST_SIGNAL] = { 0, };
static GParamSpec *properties [N_PROPS] = { NULL, };


/*
 * Auxiliary functions
 */

static void
update_event_list (GcalMonthPopover *self)
{
  g_autoptr (GDateTime) start_dt = NULL;
  g_autoptr (GDateTime) end_dt = NULL;
  g_autoptr (GPtrArray) events = NULL;
  GtkWidget *child;
  guint i;

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (self->listbox))) != NULL)
    gtk_list_box_remove (self->listbox, child);

  if (!self->date)
    return;

  start_dt = g_date_time_new_local (g_date_time_get_year (self->date),
                                    g_date_time_get_month (self->date),
                                    g_date_time_get_day_of_month (self->date),
                                    0, 0, 0);
  end_dt = g_date_time_add_days (start_dt, 1);

  events = gcal_manager_get_events (gcal_context_get_manager (self->context), start_dt, end_dt);

  for (i = 0; events && i < events->len; i++)
    {
      g_autoptr (GDateTime) event_start = NULL;
      g_autoptr (GDateTime) event_end = NULL;
      g_autoptr (GTimeZone) tz = NULL;
      GtkWidget *event_widget;
      GcalEvent *event;
      GtkWidget *row;

      event = g_ptr_array_index (events, i);

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

      event_widget = gcal_event_widget_new (self->context, event);
      gcal_event_widget_set_date_start (GCAL_EVENT_WIDGET (event_widget), event_start);
      gcal_event_widget_set_date_end (GCAL_EVENT_WIDGET (event_widget), event_end);

      row = gtk_list_box_row_new ();
      gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), event_widget);
      gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), FALSE);

      gtk_list_box_append (self->listbox, row);

      g_signal_connect_object (event_widget,
                               "activate",
                               G_CALLBACK (event_activated_cb),
                               self,
                               0);
    }
}

static inline gdouble
lerp (gdouble from,
      gdouble to,
      gdouble progress)
{
  return from * (1.0 - progress) + to * progress;
}

static GskTransform*
create_transform (GcalMonthPopover *self,
                  gint              width,
                  gint              height)
{
  graphene_point_t offset;
  GskTransform *transform;
  gdouble progress;
  gdouble scale;

  progress = adw_animation_get_value (self->animation);

  transform = gsk_transform_new ();

  graphene_point_init (&offset, width / 2.0, height / 2.0);
  transform = gsk_transform_translate (transform, &offset);

  scale = lerp (0.75, 1.0, progress);
  transform = gsk_transform_scale (transform, scale, scale);

  graphene_point_init (&offset, -width / 2.0, -height / 2.0);
  transform = gsk_transform_translate (transform, &offset);

  return g_steal_pointer (&transform);
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

  event_a = GCAL_EVENT_WIDGET (gtk_list_box_row_get_child (a));
  event_b = GCAL_EVENT_WIDGET (gtk_list_box_row_get_child (b));

  return gcal_event_widget_sort_events (event_a, event_b);
}


/*
 * Callbacks
 */

static void
animation_cb (gdouble  value,
              gpointer user_data)
{
  GcalMonthPopover *self = GCAL_MONTH_POPOVER (user_data);

  gtk_widget_set_opacity (GTK_WIDGET (self->main_box), value);
  gtk_widget_queue_allocate (GTK_WIDGET (self));
}

static void
on_animation_done_cb (AdwAnimation     *animation,
                      GcalMonthPopover *self)
{
  if (adw_animation_get_value (animation) == 0.0)
    gtk_widget_set_visible (GTK_WIDGET (self), FALSE);
}

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
  GCAL_ENTRY;

  gcal_month_popover_popdown (self);
  gtk_widget_activate_action (GTK_WIDGET (self),  "win.new-event",  NULL);

  GCAL_EXIT;
}


/*
 * GtkWidget overrides
 */

static void
gcal_month_popover_measure (GtkWidget      *widget,
                            GtkOrientation  orientation,
                            gint            for_size,
                            gint           *minimum,
                            gint           *natural,
                            gint           *minimum_baseline,
                            gint           *natural_baseline)
{
  GtkWidget *child;

  for (child = gtk_widget_get_first_child (widget);
       child;
       child = gtk_widget_get_next_sibling (child))
    {
      gint child_min_baseline = -1;
      gint child_nat_baseline = -1;
      gint child_min = 0;
      gint child_nat = 0;

      if (!gtk_widget_should_layout (child))
        continue;

      gtk_widget_measure (child,
                          orientation,
                          for_size,
                          &child_min,
                          &child_nat,
                          &child_min_baseline,
                          &child_nat_baseline);

      *minimum = MAX (*minimum, child_min);
      *natural = MAX (*natural, child_nat);

      if (child_min_baseline > -1)
        *minimum_baseline = MAX (*minimum_baseline, child_min_baseline);
      if (child_nat_baseline > -1)
        *natural_baseline = MAX (*natural_baseline, child_nat_baseline);
    }
}
static void
gcal_month_popover_size_allocate (GtkWidget *widget,
                                  gint       width,
                                  gint       height,
                                  gint       baseline)
{
  GcalMonthPopover *self = GCAL_MONTH_POPOVER (widget);
  g_autoptr (GskTransform) transform = NULL;
  GtkWidget *child;

  transform = create_transform (self, width, height);

  for (child = gtk_widget_get_first_child (widget);
       child;
       child = gtk_widget_get_next_sibling (child))
    {
      if (!gtk_widget_should_layout (child))
        continue;

      gtk_widget_allocate (child, width, height, baseline, gsk_transform_ref (transform));
    }
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
    case PROP_CONTEXT:
      g_value_set_object (value, self->context);
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
    case PROP_CONTEXT:
      self->context = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_month_popover_dispose (GObject *object)
{
  GcalMonthPopover *self = (GcalMonthPopover *)object;

  g_clear_pointer (&self->main_box, gtk_widget_unparent);

  G_OBJECT_CLASS (gcal_month_popover_parent_class)->dispose (object);
}

static void
gcal_month_popover_class_init (GcalMonthPopoverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gcal_month_popover_dispose;
  object_class->get_property = gcal_month_popover_get_property;
  object_class->set_property = gcal_month_popover_set_property;

  widget_class->measure = gcal_month_popover_measure;
  widget_class->size_allocate = gcal_month_popover_size_allocate;

  properties [PROP_CONTEXT] = g_param_spec_object ("context",
                                                   "Context",
                                                   "Context",
                                                   GCAL_TYPE_CONTEXT,
                                                   G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals[EVENT_ACTIVATED] = g_signal_new ("event-activated",
                                           GCAL_TYPE_MONTH_POPOVER,
                                           G_SIGNAL_RUN_FIRST,
                                           0,  NULL, NULL, NULL,
                                           G_TYPE_NONE,
                                           1,
                                           GCAL_TYPE_EVENT_WIDGET);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/views/gcal-month-popover.ui");

  gtk_widget_class_bind_template_callback (widget_class, close_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, new_event_button_clicked_cb);

  gtk_widget_class_bind_template_child (widget_class, GcalMonthPopover, day_label);
  gtk_widget_class_bind_template_child (widget_class, GcalMonthPopover, listbox);
  gtk_widget_class_bind_template_child (widget_class, GcalMonthPopover, main_box);

  gtk_widget_class_set_css_name (widget_class, "monthpopover");
}

static void
gcal_month_popover_init (GcalMonthPopover *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_list_box_set_sort_func (GTK_LIST_BOX (self->listbox), sort_func, NULL, NULL);

  self->animation = adw_timed_animation_new (GTK_WIDGET (self),
                                             0.0,
                                             1.0,
                                             250,
                                             adw_callback_animation_target_new (animation_cb, self, NULL));
  g_signal_connect (self->animation, "done", G_CALLBACK (on_animation_done_cb), self);
}

GtkWidget*
gcal_month_popover_new (void)
{
  return g_object_new (GCAL_TYPE_MONTH_POPOVER, NULL);
}

void
gcal_month_popover_popup (GcalMonthPopover *self)
{
  g_return_if_fail (GCAL_IS_MONTH_POPOVER (self));

  GCAL_ENTRY;

  gtk_widget_set_visible (GTK_WIDGET (self), TRUE);

  adw_timed_animation_set_reverse (ADW_TIMED_ANIMATION (self->animation), FALSE);
  adw_timed_animation_set_easing (ADW_TIMED_ANIMATION (self->animation), ADW_EASE_OUT_EXPO);
  adw_animation_play (self->animation);

  update_event_list (self);

  GCAL_EXIT;
}

void
gcal_month_popover_popdown (GcalMonthPopover *self)
{
  g_return_if_fail (GCAL_IS_MONTH_POPOVER (self));

  GCAL_ENTRY;

  adw_timed_animation_set_reverse (ADW_TIMED_ANIMATION (self->animation), TRUE);
  adw_timed_animation_set_easing (ADW_TIMED_ANIMATION (self->animation), ADW_EASE_IN_EXPO);
  adw_animation_play (self->animation);

  GCAL_EXIT;
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

  if (date && self->date && gcal_date_time_compare_date (self->date, date) == 0)
    return;

  gcal_clear_date_time (&self->date);

  if (date)
    {
      g_autofree gchar *label = NULL;

      self->date = g_date_time_ref (date);

      label = g_strdup_printf ("%d", g_date_time_get_day_of_month (date));
      gtk_label_set_label (self->day_label, label);
    }

  update_event_list (self);
}
