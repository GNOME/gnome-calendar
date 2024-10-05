/* gcal-event-widget.c
 *
 * Copyright (C) 2015 Erick Pérez Castellanos <erickpc@gnome.org>
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

#define G_LOG_DOMAIN "GcalEventWidget"

#include <libecal/libecal.h>
#include <glib/gi18n.h>
#include <string.h>

#include "gcal-application.h"
#include "gcal-context.h"
#include "gcal-clock.h"
#include "gcal-debug.h"
#include "gcal-event-popover.h"
#include "gcal-event-widget.h"
#include "gcal-overflow-bin.h"
#include "gcal-utils.h"

#define LOCATION_MAX_LEN 50
#define DESC_MAX_CHAR    200
#define INTENSITY(c)     ((c->red) * 0.30 + (c->green) * 0.59 + (c->blue) * 0.11)
#define ICON_SIZE        16

typedef struct
{
  GcalEventWidget *event_widget;
  GcalEventPreviewCallback callback;
  gpointer user_data;
} PreviewData;

struct _GcalEventWidget
{
  GtkWidget           parent;

  /* properties */
  GDateTime          *dt_start;
  GDateTime          *dt_end;

  /* widgets */
  GtkWidget          *horizontal_box;
  GtkWidget          *timestamp_label;
  GtkWidget          *edge;
  GtkWidget          *overflow_bin;
  GtkWidget          *summary_inscription;
  GtkWidget          *vertical_box;
  GtkEventController *drag_source;
  GtkWidget          *preview_popover;

  /* internal data */
  gchar              *css_class;

  GcalEvent          *event;

  GtkOrientation      orientation;

  GcalTimestampPolicy timestamp_policy;

  gint                old_width;
  gint                old_height;

  GcalContext        *context;
};

enum
{
  PROP_0,
  PROP_CONTEXT,
  PROP_EVENT,
  PROP_TIMESTAMP_POLICY,
  PROP_ORIENTATION,
  NUM_PROPS
};

enum
{
  ACTIVATE,
  NUM_SIGNALS
};

typedef enum
{
  CURSOR_NONE,
  CURSOR_GRAB,
  CURSOR_GRABBING
} CursorType;

static guint signals[NUM_SIGNALS] = { 0, };

static gboolean read_only_to_propagation_phase_cb (GBinding     *binding,
                                                   const GValue *from_value,
                                                   GValue       *to_value,
                                                   gpointer      user_data);

G_DEFINE_TYPE_WITH_CODE (GcalEventWidget, gcal_event_widget, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE, NULL));

/*
 * Auxiliary methods
 */

static void
gcal_event_widget_update_orientation_widgets (GcalEventWidget *self)
{
  gboolean is_vertical;
  gboolean is_all_day;

  is_vertical = self->orientation == GTK_ORIENTATION_VERTICAL;
  is_all_day = gcal_event_get_all_day (self->event);

  if (is_vertical && !is_all_day)
    gcal_overflow_bin_set_child (GCAL_OVERFLOW_BIN (self->overflow_bin), self->vertical_box);
  else
    gcal_overflow_bin_set_child (GCAL_OVERFLOW_BIN (self->overflow_bin), self->horizontal_box);
}

static void
gcal_event_widget_update_style (GcalEventWidget *self)
{
  gboolean slanted_start;
  gboolean slanted_end;
  gboolean timed;

  slanted_start = FALSE;
  slanted_end = FALSE;

  /* Clear previous style classes */
  gtk_widget_remove_css_class (GTK_WIDGET (self), "slanted");
  gtk_widget_remove_css_class (GTK_WIDGET (self), "slanted-start");
  gtk_widget_remove_css_class (GTK_WIDGET (self), "slanted-end");
  gtk_widget_remove_css_class (GTK_WIDGET (self), "timed");

  /*
   * If the event's dates differs from the widget's dates,
   * add a slanted edge class at the widget.
   */

  if (self->dt_start)
    slanted_start = g_date_time_compare (gcal_event_get_date_start (self->event), self->dt_start) != 0;

  if (self->dt_end)
    slanted_end = g_date_time_compare (gcal_event_get_date_end (self->event), self->dt_end) != 0;

  if (slanted_start && slanted_end)
    gtk_widget_add_css_class (GTK_WIDGET (self), "slanted");
  else if (slanted_start)
    gtk_widget_add_css_class (GTK_WIDGET (self), "slanted-start");
  else if (slanted_end)
    gtk_widget_add_css_class (GTK_WIDGET (self), "slanted-end");

  /* Add style classes for orientation selectors */
  if (self->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      gtk_widget_remove_css_class (GTK_WIDGET (self), "vertical");
      gtk_widget_add_css_class (GTK_WIDGET (self), "horizontal");
    }
  else
    {
      gtk_widget_remove_css_class (GTK_WIDGET (self), "horizontal");
      gtk_widget_add_css_class (GTK_WIDGET (self), "vertical");
    }

  /*
   * If the event is a timed, single-day event, draw it differently
   * from all-day or multi-day events.
   */
  timed = !gcal_event_get_all_day (self->event) && !gcal_event_is_multiday (self->event);

  if (timed)
    gtk_widget_add_css_class (GTK_WIDGET (self), "timed");
}

static void
update_color (GcalEventWidget *self)
{
  GdkRGBA *color;
  GQuark color_id;
  gchar *color_str;
  gchar *css_class;
  GDateTime *now;
  gint date_compare;

  color = gcal_event_get_color (self->event);
  now = g_date_time_new_now_local ();
  date_compare = g_date_time_compare (self->dt_end, now);

  /* Fades out an event that's earlier than the current date */
  if (date_compare < 0)
    gtk_widget_add_css_class (GTK_WIDGET (self), "dimmed");
  else
    gtk_widget_remove_css_class (GTK_WIDGET (self), "dimmed");

  /* Remove the old style class */
  if (self->css_class)
    {
      gtk_widget_remove_css_class (GTK_WIDGET (self), self->css_class);
      g_clear_pointer (&self->css_class, g_free);
    }

  color_str = gdk_rgba_to_string (color);
  color_id = g_quark_from_string (color_str);
  css_class = g_strdup_printf ("color-%d", color_id);

  gtk_widget_add_css_class (GTK_WIDGET (self), css_class);

  if (INTENSITY (color) > 0.5)
    {
      gtk_widget_remove_css_class (GTK_WIDGET (self), "color-dark");
      gtk_widget_add_css_class (GTK_WIDGET (self), "color-light");
    }
  else
    {
      gtk_widget_remove_css_class (GTK_WIDGET (self), "color-light");
      gtk_widget_add_css_class (GTK_WIDGET (self), "color-dark");
    }

  /* Keep the current style around, so we can remove it later */
  self->css_class = css_class;

  g_clear_pointer (&now, g_date_time_unref);
  g_free (color_str);
}

static void
gcal_event_widget_set_event_tooltip (GcalEventWidget *self,
                                     GcalEvent       *event)
{
  g_autoptr (GDateTime) tooltip_start = NULL;
  g_autoptr (GDateTime) tooltip_end = NULL;
  g_autofree gchar *escaped_summary = NULL;
  g_autofree gchar *location = NULL;
  g_autofree gchar *start = NULL;
  g_autofree gchar *end = NULL;
  GString *tooltip_mesg;
  gboolean allday, multiday, is_ltr;
  guint description_len;

  tooltip_mesg = g_string_new (NULL);
  escaped_summary = g_markup_escape_text (gcal_event_get_summary (event), -1);
  g_string_append_printf (tooltip_mesg, "<b>%s</b>", escaped_summary);

  allday = gcal_event_get_all_day (event);
  multiday = gcal_event_is_multiday (event);

  is_ltr = gtk_widget_get_direction (GTK_WIDGET (self)) != GTK_TEXT_DIR_RTL;

  if (allday)
    {
      /* All day events span from [ start, end - 1 day ] */
      tooltip_start = g_date_time_ref (gcal_event_get_date_start (event));
      tooltip_end = g_date_time_add_days (gcal_event_get_date_end (event), -1);

      if (multiday)
        {
          start = g_date_time_format (tooltip_start, "%x");
          end = g_date_time_format (tooltip_end, "%x");
        }
      else
        {
          start = g_date_time_format (tooltip_start, "%x");
          end = NULL;
        }
    }
  else
    {
      tooltip_start = g_date_time_to_local (gcal_event_get_date_start (event));
      tooltip_end = g_date_time_to_local (gcal_event_get_date_end (event));

      if (multiday)
        {
          switch (gcal_context_get_time_format (self->context))
            {
            case GCAL_TIME_FORMAT_24H:
              if (is_ltr)
                {
                  start = g_date_time_format (tooltip_start, "%x %R");
                  end = g_date_time_format (tooltip_end, "%x %R");
                }
              else
                {
                  start = g_date_time_format (tooltip_start, "%R %x");
                  end = g_date_time_format (tooltip_end, "%R %x");
                }
              break;

            case GCAL_TIME_FORMAT_12H:
              if (is_ltr)
                {
                  start = g_date_time_format (tooltip_start, "%x %I:%M %P");
                  end = g_date_time_format (tooltip_end, "%x %I:%M %P");
                }
              else
                {
                  start = g_date_time_format (tooltip_start, "%P %M:%I %x");
                  end = g_date_time_format (tooltip_end, "%P %M:%I %x");
                }
              break;
            }
        }
      else
        {
          switch (gcal_context_get_time_format (self->context))
            {
            case GCAL_TIME_FORMAT_24H:
              if (is_ltr)
                {
                  start = g_date_time_format (tooltip_start, "%x, %R");
                  end = g_date_time_format (tooltip_end, "%R");
                }
              else
                {
                  start = g_date_time_format (tooltip_start, "%R ,%x");
                  end = g_date_time_format (tooltip_end, "%R");
                }
              break;

            case GCAL_TIME_FORMAT_12H:
              if (is_ltr)
                {
                  start = g_date_time_format (tooltip_start, "%x, %I:%M %P");
                  end = g_date_time_format (tooltip_end, "%I:%M %P");
                }
              else
                {
                  start = g_date_time_format (tooltip_start, "%P %M:%I ,%x");
                  end = g_date_time_format (tooltip_end, "%P %M:%I");
                }
              break;
            }
        }
    }

  if (allday && !multiday)
    {
      g_assert (start != NULL);

      g_string_append_printf (tooltip_mesg, "\n%s", start);
    }
  else
    {
      g_assert (start != NULL);
      g_assert (end != NULL);

      g_string_append_printf (tooltip_mesg,
                              "\n%s - %s",
                              is_ltr ? start : end,
                              is_ltr ? end : start);
    }

  /* Append event location */
  location = g_strdup (gcal_event_get_location (event));
  if (location)
    g_strstrip (location);

  if (location && location[0] != '\0')
    {
      g_autofree gchar *escaped_location = NULL;
      g_autoptr (GUri) G_GNUC_UNUSED guri = NULL; /* we just check for a parse error; don't care about the URI */
      g_autoptr (GString) string = NULL;
      g_autoptr (GError) error = NULL;

      string = g_string_new (NULL);
      guri = g_uri_parse (location, SOUP_HTTP_URI_FLAGS | G_URI_FLAGS_PARSE_RELAXED, &error);

      if (error == NULL)
        {
          const gchar *service_name = gcal_get_service_name_from_url (location);

          if (service_name)
            {
              /* Translators: %s is the meeting service name for the event (e.g. "Google Meet") */
              g_string_append_printf (string, _("\n\nOn %s"), service_name);
            }
          else
            {
              g_autoptr (GString) truncated_location = g_string_new (location);

              if (truncated_location->len > LOCATION_MAX_LEN)
                {
                  g_string_truncate (truncated_location, LOCATION_MAX_LEN - 1);
                  g_string_append (truncated_location, "…");
                }

              /* Translators: %s is a URL informed as the location of the event */
              g_string_append_printf (string, _("\n\nAt %s"), truncated_location->str);
            }
        }
      else
        {
          /* Translators: %s is the location of the event (e.g. "Downtown, 3rd Avenue") */
          g_string_append_printf (string, _("\n\nAt %s"), location);
        }

      escaped_location = g_markup_escape_text (string->str, -1);

      g_string_append (tooltip_mesg, escaped_location);
    }

  description_len = g_utf8_strlen (gcal_event_get_description (event), -1);

  /* Truncate long descriptions at a white space and ellipsize */
  if (description_len > 0)
    {
      g_autofree gchar *escaped_description;
      GString *tooltip_desc;

      tooltip_desc = g_string_new (gcal_event_get_description (event));

      /* If the description is larger than DESC_MAX_CHAR, ellipsize it */
      if (description_len > DESC_MAX_CHAR)
        {
          g_string_truncate (tooltip_desc, DESC_MAX_CHAR - 1);
          g_string_append (tooltip_desc, "…");
        }

      escaped_description = g_markup_escape_text (tooltip_desc->str, -1);

      g_string_append_printf (tooltip_mesg, "\n\n%s", escaped_description);

      g_string_free (tooltip_desc, TRUE);
    }

  gtk_widget_set_tooltip_markup (GTK_WIDGET (self), tooltip_mesg->str);

  g_string_free (tooltip_mesg, TRUE);
}

static void
gcal_event_widget_update_timestamp (GcalEventWidget *self)
{
  g_autofree gchar *timestamp_str = NULL;

  if (GCAL_IS_EVENT (self->event) &&
      self->timestamp_policy != GCAL_TIMESTAMP_POLICY_NONE)
    {
      g_autoptr (GDateTime) time = NULL;

      if (self->timestamp_policy == GCAL_TIMESTAMP_POLICY_START)
        time = g_date_time_to_local (gcal_event_get_date_start (self->event));
      else
        time = g_date_time_to_local (gcal_event_get_date_end (self->event));

      if (gcal_event_get_all_day (self->event) || gcal_event_is_multiday (self->event))
        /*
         * Translators: %a is the abbreviated day name, %B is the month name
         * and %d is the day of the month as a number between 0 and 31.
         * More formats can be found on the doc:
         * https://docs.gtk.org/glib/method.DateTime.format.html
         */
        timestamp_str = g_date_time_format (time, _("%a %B %d"));
      else if (gcal_context_get_time_format (self->context) == GCAL_TIME_FORMAT_24H)
        timestamp_str = g_date_time_format (time, "%R");
      else
        timestamp_str = g_date_time_format (time, "%I:%M %P");
    }

  gtk_widget_set_visible (self->timestamp_label, timestamp_str != NULL);
  gtk_label_set_label (GTK_LABEL (self->timestamp_label), timestamp_str);
}

static void
gcal_event_widget_set_event_internal (GcalEventWidget *self,
                                      GcalEvent       *event)
{
  /*
   * This function is called only once, since the property is
   * set as CONSTRUCT_ONLY. Any other attempt to set an event
   * will be ignored.
   *
   * Because of that condition, we don't really have to care about
   * disconnecting functions or cleaning up the previous event.
   */

  /* The event spawns with a floating reference, and we take it's ownership */
  g_set_object (&self->event, event);

  /*
   * Initially, the widget's start and end dates are the same
   * of the event's ones. We may change it afterwards.
   */
  gcal_event_widget_set_date_start (self, gcal_event_get_date_start (event));
  gcal_event_widget_set_date_end (self, gcal_event_get_date_end (event));

  /* Update color */
  update_color (self);

  g_signal_connect_object (event,
                           "notify::color",
                           G_CALLBACK (update_color),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (event,
                           "notify::summary",
                           G_CALLBACK (gtk_widget_queue_draw),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->context,
                           "notify::time-format",
                           G_CALLBACK (gcal_event_widget_update_timestamp),
                           self,
                           G_CONNECT_SWAPPED);


  /* Tooltip */
  gcal_event_widget_set_event_tooltip (self, event);

  /* Summary label */
  g_object_bind_property (event,
                          "summary",
                          self->summary_inscription,
                          "text",
                          G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

  g_object_bind_property_full (gcal_event_get_calendar (event),
                               "read-only",
                               self->drag_source,
                               "propagation-phase",
                               G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE,
                               read_only_to_propagation_phase_cb,
                               NULL,
                               self,
                               NULL);

  gcal_event_widget_update_style (self);
  gcal_event_widget_update_timestamp (self);
}

static void
reply_preview_callback (GtkWidget              *event_popover,
                        PreviewData            *data,
                        GcalEventPreviewAction  action)
{
  if (data->callback)
    data->callback (data->event_widget, action, data->user_data);

  g_signal_handlers_disconnect_by_data (event_popover, data);

  gtk_popover_popdown (GTK_POPOVER (event_popover));
  gtk_widget_unparent (event_popover);
  gtk_accessible_update_state (GTK_ACCESSIBLE (data->event_widget),
                               GTK_ACCESSIBLE_STATE_PRESSED, FALSE,
                               -1);

  g_clear_pointer (&data, g_free);
}


/*
 * Callbacks
 */

static void
on_click_gesture_pressed_cb (GtkGestureClick *click_gesture,
                             gint             n_press,
                             gdouble          x,
                             gdouble          y,
                             GcalEventWidget *self)
{
  gtk_gesture_set_state (GTK_GESTURE (click_gesture), GTK_EVENT_SEQUENCE_CLAIMED);
}

static void
on_click_gesture_release_cb (GtkGestureClick *click_gesture,
                             gint             n_press,
                             gdouble          x,
                             gdouble          y,
                             GcalEventWidget *self)
{
  gtk_gesture_set_state (GTK_GESTURE (click_gesture), GTK_EVENT_SEQUENCE_CLAIMED);
  g_signal_emit (self, signals[ACTIVATE], 0);
}

static void
on_drag_source_begin_cb (GtkDragSource   *source,
                         GdkDrag         *drag,
                         GcalEventWidget *self)
{
  g_autoptr (GdkPaintable) paintable = NULL;

  paintable = gtk_widget_paintable_new (GTK_WIDGET (self));
  gtk_drag_source_set_icon (source, paintable, 0, 0);
}

static GdkContentProvider*
on_drag_source_prepare_cb (GtkDragSource   *source,
                           gdouble          x,
                           gdouble          y,
                           GcalEventWidget *self)
{
  return gdk_content_provider_new_typed (GCAL_TYPE_EVENT_WIDGET, self);
}

static void
on_event_popover_closed_cb (GtkWidget   *event_popover,
                            PreviewData *data)
{
  reply_preview_callback (event_popover, data, GCAL_EVENT_PREVIEW_ACTION_NONE);
}

static void
on_event_popover_edit_cb (GtkWidget   *event_popover,
                               PreviewData *data)
{
  reply_preview_callback (event_popover, data, GCAL_EVENT_PREVIEW_ACTION_EDIT);
}

static gboolean
read_only_to_propagation_phase_cb (GBinding     *binding,
                                   const GValue *from_value,
                                   GValue       *to_value,
                                   gpointer      user_data)
{
  GtkPropagationPhase phase;

  if (g_value_get_boolean (from_value))
    phase = GTK_PHASE_NONE;
  else
    phase = GTK_PHASE_BUBBLE;

  g_value_set_enum (to_value, phase);

  return TRUE;
}


/*
 * GObject overrides
 */

static void
gcal_event_widget_set_property (GObject      *object,
                                guint         property_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GcalEventWidget *self = GCAL_EVENT_WIDGET (object);

  switch (property_id)
    {
    case PROP_CONTEXT:
      g_assert (self->context == NULL);
      self->context = g_value_dup_object (value);

      g_signal_connect_object (gcal_context_get_clock (self->context),
                               "minute-changed",
                               G_CALLBACK (update_color),
                               self,
                               G_CONNECT_SWAPPED);
      break;

    case PROP_EVENT:
      gcal_event_widget_set_event_internal (self, g_value_get_object (value));
      break;

    case PROP_TIMESTAMP_POLICY:
      gcal_event_widget_set_timestamp_policy (self, g_value_get_enum (value));
      break;

    case PROP_ORIENTATION:
      self->orientation = g_value_get_enum (value);
      gcal_event_widget_update_orientation_widgets (self);
      gcal_event_widget_update_style (self);
      gcal_event_widget_update_timestamp (self);
      g_object_notify (object, "orientation");
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
gcal_event_widget_get_property (GObject      *object,
                                guint         property_id,
                                GValue       *value,
                                GParamSpec   *pspec)
{
  GcalEventWidget *self = GCAL_EVENT_WIDGET (object);

  switch (property_id)
    {
    case PROP_CONTEXT:
      g_value_set_object (value, self->context);
      break;

    case PROP_EVENT:
      g_value_set_object (value, self->event);
      break;

    case PROP_TIMESTAMP_POLICY:
      g_value_set_enum (value, self->timestamp_policy);
      break;

    case PROP_ORIENTATION:
      g_value_set_enum (value, self->orientation);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
gcal_event_widget_dispose (GObject *object)
{
  GcalEventWidget *self = GCAL_EVENT_WIDGET (object);

  g_clear_pointer (&self->preview_popover, gtk_widget_unparent);
  g_clear_pointer (&self->overflow_bin, gtk_widget_unparent);
  g_clear_pointer (&self->edge, gtk_widget_unparent);

  G_OBJECT_CLASS (gcal_event_widget_parent_class)->dispose (object);
}

static void
gcal_event_widget_finalize (GObject *object)
{
  GcalEventWidget *self;

  self = GCAL_EVENT_WIDGET (object);

  /* disconnect signals */
  g_signal_handlers_disconnect_by_func (self->event, update_color, self);
  g_signal_handlers_disconnect_by_func (self->event, gtk_widget_queue_draw, self);

  /* releasing properties */
  g_clear_pointer (&self->css_class, g_free);
  g_clear_object (&self->event);
  g_clear_object (&self->context);

  G_OBJECT_CLASS (gcal_event_widget_parent_class)->finalize (object);
}

static void
gcal_event_widget_class_init (GcalEventWidgetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = gcal_event_widget_set_property;
  object_class->get_property = gcal_event_widget_get_property;
  object_class->dispose = gcal_event_widget_dispose;
  object_class->finalize = gcal_event_widget_finalize;

  /**
   * GcalEventWidget::context:
   *
   * The context of the event.
   */
  g_object_class_install_property (object_class,
                                   PROP_CONTEXT,
                                   g_param_spec_object ("context",
                                                        "Context",
                                                        "Context",
                                                        GCAL_TYPE_CONTEXT,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  /**
   * GcalEventWidget::event:
   *
   * The event this widget represents.
   */
  g_object_class_install_property (object_class,
                                   PROP_EVENT,
                                   g_param_spec_object ("event",
                                                        "Event",
                                                        "The event this widget represents",
                                                        GCAL_TYPE_EVENT,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  /**
   * GcalEventWidget::timestamp-policy:
   *
   * The policy for this widget's timestamp.
   *
   * Whether to show the start time, end time, or no time for the
   * event. Depending on the event's kind, it will be an hour or a day.
   */
  g_object_class_install_property (object_class,
                                   PROP_TIMESTAMP_POLICY,
                                   g_param_spec_enum ("timestamp-policy",
                                                      "Timestamp Policy",
                                                      "The policy for this widget's timestamp",
                                                      GCAL_TYPE_TIMESTAMP_POLICY,
                                                      GCAL_TIMESTAMP_POLICY_NONE,
                                                      G_PARAM_READWRITE));

  /**
   * GcalEventWidget::orientation:
   *
   * The orientation of the event widget.
   */
  g_object_class_override_property (object_class, PROP_ORIENTATION, "orientation");

  signals[ACTIVATE] = g_signal_new ("activate",
                                     GCAL_TYPE_EVENT_WIDGET,
                                     G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                                     0,
                                     NULL, NULL,
                                     g_cclosure_marshal_VOID__VOID,
                                     G_TYPE_NONE,
                                     0);

  gtk_widget_class_set_activate_signal (widget_class, signals[ACTIVATE]);

  {
    g_autoptr (GtkShortcutAction) activate_action = NULL;
    const guint activate_keyvals[] = {
      GDK_KEY_space,
      GDK_KEY_KP_Space,
      GDK_KEY_Return,
      GDK_KEY_ISO_Enter,
      GDK_KEY_KP_Enter,
    };

    activate_action = gtk_signal_action_new ("activate");

    for (size_t i = 0; i < G_N_ELEMENTS (activate_keyvals); i++)
      {
        g_autoptr (GtkShortcut) activate_shortcut = NULL;

        activate_shortcut = gtk_shortcut_new (gtk_keyval_trigger_new (activate_keyvals[i], 0),
                                              g_object_ref (activate_action));

        gtk_widget_class_add_shortcut (widget_class, activate_shortcut);
      }
  }

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/gui/gcal-event-widget.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalEventWidget, drag_source);
  gtk_widget_class_bind_template_child (widget_class, GcalEventWidget, horizontal_box);
  gtk_widget_class_bind_template_child (widget_class, GcalEventWidget, edge);
  gtk_widget_class_bind_template_child (widget_class, GcalEventWidget, overflow_bin);
  gtk_widget_class_bind_template_child (widget_class, GcalEventWidget, timestamp_label);
  gtk_widget_class_bind_template_child (widget_class, GcalEventWidget, summary_inscription);
  gtk_widget_class_bind_template_child (widget_class, GcalEventWidget, vertical_box);

  gtk_widget_class_bind_template_callback (widget_class, on_click_gesture_pressed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_click_gesture_release_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_drag_source_begin_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_drag_source_prepare_cb);

  gtk_widget_class_set_css_name (widget_class, "event");
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_TOGGLE_BUTTON);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BOX_LAYOUT);
}

static void
gcal_event_widget_init (GcalEventWidget *self)
{
  g_type_ensure (GCAL_TYPE_OVERFLOW_BIN);

  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_widget_set_cursor_from_name (GTK_WIDGET (self), "pointer");

  /* Starts with horizontal */
  self->orientation = GTK_ORIENTATION_HORIZONTAL;
  gtk_widget_add_css_class (GTK_WIDGET (self), "horizontal");

  gtk_accessible_update_state (GTK_ACCESSIBLE (self),
                               GTK_ACCESSIBLE_STATE_PRESSED, FALSE,
                               -1);
}

GtkWidget*
gcal_event_widget_new (GcalContext *context,
                       GcalEvent   *event)
{
  return g_object_new (GCAL_TYPE_EVENT_WIDGET,
                       "context", context,
                       "event", event,
                       NULL);
}

/**
 * gcal_event_widget_get_date_end:
 * @self: a #GcalEventWidget
 *
 * Retrieves the visible end date of this widget. This
 * may differ from the event's end date.
 *
 * Returns: (transfer none): a #GDateTime
 */
GDateTime*
gcal_event_widget_get_date_end (GcalEventWidget *self)
{
  g_return_val_if_fail (GCAL_IS_EVENT_WIDGET (self), NULL);

  return self->dt_end ? self->dt_end : self->dt_start;
}

/**
 * gcal_event_widget_set_date_end:
 * @self: a #GcalEventWidget
 * @date_end: the end date of this widget
 *
 * Sets the visible end date of this widget. This
 * may differ from the event's end date, but cannot
 * be after it.
 *
 * The end date for this widget may differ from the event's end
 * date. For example, if the event spans more than one month and we're
 * in Month View, the end date marks the last day this event widget is
 * visible.
 */
void
gcal_event_widget_set_date_end (GcalEventWidget *self,
                                GDateTime       *date_end)
{
  g_return_if_fail (GCAL_IS_EVENT_WIDGET (self));

  if (self->dt_end != date_end &&
      (!self->dt_end || !date_end ||
       (self->dt_end && date_end && !g_date_time_equal (self->dt_end, date_end))))
    {
      /* The end date should never be after the event's end date */
      if (date_end && g_date_time_compare (date_end, gcal_event_get_date_end (self->event)) > 0)
        return;

      g_clear_pointer (&self->dt_end, g_date_time_unref);
      self->dt_end = g_date_time_ref (date_end);

      gcal_event_widget_update_style (self);
    }
}

/**
 * gcal_event_widget_get_date_start:
 * @self: a #GcalEventWidget
 *
 * Retrieves the visible start date of this widget. This
 * may differ from the event's start date.
 *
 * Returns: (transfer none): a #GDateTime
 */
GDateTime*
gcal_event_widget_get_date_start (GcalEventWidget *self)
{
  g_return_val_if_fail (GCAL_IS_EVENT_WIDGET (self), NULL);

  return self->dt_start;
}

/**
 * gcal_event_widget_set_date_start:
 * @self: a #GcalEventWidget
 * @date_start: the start date of this widget
 *
 * Sets the visible start date of this widget. This
 * may differ from the event's start date, but cannot
 * be before it.
 *
 * The start date for this widget may differ from the event's start
 * date. For example, if the event spans more than one month and we're
 * in Month View, the start date marks the first day this event widget
 * is visible.
 */
void
gcal_event_widget_set_date_start (GcalEventWidget *self,
                                  GDateTime       *date_start)
{
  g_return_if_fail (GCAL_IS_EVENT_WIDGET (self));

  if (self->dt_start != date_start &&
      (!self->dt_start || !date_start ||
       (self->dt_start && date_start && !g_date_time_equal (self->dt_start, date_start))))
    {
      /* The start date should never be before the event's start date */
      if (date_start && g_date_time_compare (date_start, gcal_event_get_date_start (self->event)) < 0)
        return;

      g_clear_pointer (&self->dt_start, g_date_time_unref);
      self->dt_start = g_date_time_ref (date_start);

      gcal_event_widget_update_style (self);
    }
}

/**
 * gcal_event_widget_set_timestamp_policy:
 * @self: a #GcalEventWidget
 * @policy: the timestamp policy
 *
 * Sets whether to show the start time, end time, or no time for the
 * event. Depending on the event's kind, it will be an hour or a day.
 */
void
gcal_event_widget_set_timestamp_policy (GcalEventWidget     *self,
                                        GcalTimestampPolicy  policy)
{
  g_return_if_fail (GCAL_IS_EVENT_WIDGET (self));
  g_return_if_fail (policy >= GCAL_TIMESTAMP_POLICY_NONE && policy <= GCAL_TIMESTAMP_POLICY_END);

  if (self->timestamp_policy != policy)
    {
      self->timestamp_policy = policy;

      gcal_event_widget_update_timestamp (self);

      g_object_notify (G_OBJECT (self), "timestamp-policy");
    }
}

/**
 * gcal_event_widget_show_preview:
 * @self: a #GcalEventWidget
 * @callback: (nullable): callback for the event preview
 * @user_data: (nullable): user data for @callback
 *
 * Shows an event preview popover for this event widget, and
 * calls @callback when the popover is either dismissed, or
 * the edit button is clicked.
 */
void
gcal_event_widget_show_preview (GcalEventWidget          *self,
                                GcalEventPreviewCallback  callback,
                                gpointer                  user_data)
{
  PreviewData *data;

  GCAL_ENTRY;

  g_clear_pointer (&self->preview_popover, gtk_widget_unparent);

  data = g_new0 (PreviewData, 1);
  data->event_widget = self;
  data->callback = callback;
  data->user_data = user_data;

  self->preview_popover = gcal_event_popover_new (self->context, self->event);
  g_object_add_weak_pointer (G_OBJECT (self->preview_popover), (gpointer *) &self->preview_popover);

  gtk_widget_set_parent (self->preview_popover, GTK_WIDGET (self));
  g_signal_connect (self->preview_popover, "closed", G_CALLBACK (on_event_popover_closed_cb), data);
  g_signal_connect (self->preview_popover, "edit", G_CALLBACK (on_event_popover_edit_cb), data);
  gtk_popover_popup (GTK_POPOVER (self->preview_popover));
  gtk_accessible_update_state (GTK_ACCESSIBLE (self),
                               GTK_ACCESSIBLE_STATE_PRESSED, TRUE,
                               -1);

  GCAL_EXIT;
}

/**
 * gcal_event_widget_get_event:
 * @self: a #GcalEventWidget
 *
 * Retrieves the @GcalEvent this widget represents.
 *
 * Returns: (transfer none): a #GcalEvent
 */
GcalEvent*
gcal_event_widget_get_event (GcalEventWidget *self)
{
  g_return_val_if_fail (GCAL_IS_EVENT_WIDGET (self), NULL);

  return self->event;
}

GtkWidget*
gcal_event_widget_clone (GcalEventWidget *widget)
{
  return gcal_event_widget_new (widget->context, widget->event);
}

/**
 * gcal_event_widget_equal:
 * @widget1: an #GcalEventWidget representing an event
 * @widget2: an #GcalEventWidget representing an event
 *
 * Check if two widget represent the same event.
 *
 * Returns: %TRUE if both widget represent the same event,
 *          false otherwise
 **/
gboolean
gcal_event_widget_equal (GcalEventWidget *widget1,
                         GcalEventWidget *widget2)
{
  return g_strcmp0 (gcal_event_get_uid (widget1->event), gcal_event_get_uid (widget2->event)) == 0;
}

/**
 * gcal_event_widget_compare_by_length:
 * @widget1:
 * @widget2:
 *
 * Compare two widgets by the duration of the events they represent. From shortest to longest span.
 *
 * Returns: negative value if a < b ; zero if a = b ; positive value if a > b
 **/
gint
gcal_event_widget_compare_by_length (GcalEventWidget *widget1,
                                     GcalEventWidget *widget2)
{
  time_t time_s1, time_s2;
  time_t time_e1, time_e2;

  time_e1 = time_s1 = g_date_time_to_unix (widget1->dt_start);
  time_e2 = time_s2 = g_date_time_to_unix (widget2->dt_start);

  if (widget1->dt_end != NULL)
    time_e1 = g_date_time_to_unix (widget1->dt_end);
  if (widget2->dt_end)
    time_e2 = g_date_time_to_unix (widget2->dt_end);

  return (time_e2 - time_s2) - (time_e1 - time_s1);
}

gint
gcal_event_widget_compare_by_start_date (GcalEventWidget *widget1,
                                         GcalEventWidget *widget2)
{
  return g_date_time_compare (widget1->dt_start, widget2->dt_start);
}

gint
gcal_event_widget_sort_events (GcalEventWidget *widget1,
                               GcalEventWidget *widget2)
{
  g_autoptr (GDateTime) dt_time1 = NULL;
  g_autoptr (GDateTime) dt_time2 = NULL;
  ICalTime *itt;
  gint diff;

  diff = gcal_event_is_multiday (widget2->event) - gcal_event_is_multiday (widget1->event);

  if (diff != 0)
    return diff;

  diff = gcal_event_widget_compare_by_start_date (widget1, widget2);

  if (diff != 0)
    return diff;

  diff = gcal_event_widget_compare_by_length (widget1, widget2);

  if (diff != 0)
    return diff;

  itt = e_cal_component_get_last_modified (gcal_event_get_component (widget1->event));
  if (itt)
    dt_time1 = gcal_date_time_from_icaltime (itt);
  g_clear_object (&itt);

  itt = e_cal_component_get_last_modified (gcal_event_get_component (widget2->event));
  if (itt)
    dt_time2 = gcal_date_time_from_icaltime (itt);
  g_clear_object (&itt);

  return dt_time1 && dt_time2 ? g_date_time_compare (dt_time2, dt_time1) : 0;
}
