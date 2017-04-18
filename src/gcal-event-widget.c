/* -*- mode: c; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gcal-event-widget.c
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

#include <string.h>
#include <glib/gi18n.h>

#include "gcal-event-widget.h"
#include "gcal-utils.h"

#define DESC_MAX_CHAR 200
#define INTENSITY(c)  ((c->red) * 0.30 + (c->green) * 0.59 + (c->blue) * 0.11)
#define ICON_SIZE     16

struct _GcalEventWidget
{
  GtkWidget      parent;

  /* properties */
  GDateTime     *dt_start;
  GDateTime     *dt_end;

  /* internal data */
  gboolean       clock_format_24h : 1;
  gboolean       read_only : 1;
  gchar         *css_class;

  GcalEvent     *event;

  GtkOrientation orientation;

  GdkWindow     *event_window;
  gboolean       button_pressed;
};

enum
{
  PROP_0,
  PROP_DATE_END,
  PROP_DATE_START,
  PROP_EVENT,
  PROP_ORIENTATION,
  NUM_PROPS
};

enum
{
  ACTIVATE,
  NUM_SIGNALS
};

static guint signals[NUM_SIGNALS] = { 0, };

G_DEFINE_TYPE_WITH_CODE (GcalEventWidget, gcal_event_widget, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE, NULL));

static gchar*
get_visible_text (GtkWidget         *widget,
                  GtkOrientation     orientation)
{
  GcalEventWidget *self;
  gboolean is_ltr;
  gchar *display_text;
  gchar *escaped_text;

  self = GCAL_EVENT_WIDGET (widget);
  is_ltr = gtk_widget_get_direction (widget) != GTK_TEXT_DIR_RTL;

  /*
   * Since we set markup labels, we have to escape text before
   * using it, or we get empty event widgets.
   */
  escaped_text = g_markup_escape_text (gcal_event_get_summary (self->event), -1);

  /*
   * The text displayed in the widget includes the time when the event
   * is timed, or not if the event is all day.
   */
  if (gcal_event_get_all_day (self->event))
    {
      display_text = escaped_text;
    }
  else
    {
      GDateTime *local_start_time;
      gchar *start_time;
      local_start_time = g_date_time_to_local (gcal_event_get_date_start (self->event));

      if (self->clock_format_24h)
        start_time = g_date_time_format (local_start_time, "%R");
      else
        start_time = g_date_time_format (local_start_time, "%I:%M %P");

      /*
       * When in horizontal, draw the elements sequentially; otherwise,
       * draw the time in the first line, and the title in subsequent
       * lines.
       */
      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          if (is_ltr)
            display_text = g_strdup_printf ("(%s) %s", start_time, escaped_text);
          else
            display_text = g_strdup_printf ("%s (%s)", escaped_text, start_time);
        }
      else
        {
          display_text = g_strdup_printf ("<b>%s</b>\n%s", start_time, escaped_text);
        }

      g_clear_pointer (&local_start_time, g_date_time_unref);
      g_clear_pointer (&escaped_text, g_free);
      g_clear_pointer (&start_time, g_free);
    }

    return display_text;
}

static gint
get_vertical_text_height (GtkWidget         *widget)
{
  GcalEventWidget *self;
  GtkBorder border, padding;
  GtkStyleContext *context;
  PangoLayout *layout;
  gint layout_height;
  gchar* display_text;

  self = GCAL_EVENT_WIDGET (widget);
  context = gtk_widget_get_style_context (widget);
  display_text = get_visible_text (widget, GTK_ORIENTATION_VERTICAL);
  layout = gtk_widget_create_pango_layout (widget, display_text);
  pango_layout_get_pixel_size (layout, NULL, &layout_height);
  g_object_unref (layout);

  gtk_style_context_get_border (context, gtk_style_context_get_state (context), &border);
  gtk_style_context_get_padding (context, gtk_style_context_get_state (context), &padding);

  g_clear_pointer(&display_text, g_free);

  return layout_height + padding.top + padding.bottom + border.top + border.bottom;
}

static void
gcal_event_widget_update_style (GcalEventWidget *self)
{
  GtkStyleContext *context;
  gboolean slanted_start, slanted_end;

  context = gtk_widget_get_style_context (GTK_WIDGET (self));
  slanted_start = FALSE;
  slanted_end = FALSE;

  /* Clear previous style classes */
  gtk_style_context_remove_class (context, "slanted");
  gtk_style_context_remove_class (context, "slanted-start");
  gtk_style_context_remove_class (context, "slanted-end");

  /*
   * If the event's dates differs from the widget's dates,
   * add a slanted edge class at the widget.
   */

  if (self->dt_start)
    slanted_start = g_date_time_compare (gcal_event_get_date_start (self->event), self->dt_start) != 0;

  if (self->dt_end)
    slanted_end = g_date_time_compare (gcal_event_get_date_end (self->event), self->dt_end) != 0;

  if (slanted_start && slanted_end)
    gtk_style_context_add_class (context, "slanted");
  else if (slanted_start)
    gtk_style_context_add_class (context, "slanted-start");
  else if (slanted_end)
    gtk_style_context_add_class (context, "slanted-end");
}

static void
update_color (GcalEventWidget *self)
{
  GtkStyleContext *context;
  GdkRGBA *color;
  GQuark color_id;
  gchar *color_str;
  gchar *css_class;
  GDateTime *now;
  gint date_compare;

  context = gtk_widget_get_style_context (GTK_WIDGET (self));
  color = gcal_event_get_color (self->event);
  now = g_date_time_new_now_local ();
  date_compare = g_date_time_compare (self->dt_end, now);

  /* Fades out an event that's earlier than the current date */
  gtk_widget_set_opacity (GTK_WIDGET (self), date_compare < 0 ? 0.6 : 1.0);

  /* Remove the old style class */
  if (self->css_class)
    {
      gtk_style_context_remove_class (context, self->css_class);
      g_clear_pointer (&self->css_class, g_free);
    }

  color_str = gdk_rgba_to_string (color);
  color_id = g_quark_from_string (color_str);
  css_class = g_strdup_printf ("color-%d", color_id);

  gtk_style_context_add_class (context, css_class);

  if (INTENSITY (color) > 0.5)
    {
      gtk_style_context_remove_class (context, "color-dark");
      gtk_style_context_add_class (context, "color-light");
    }
  else
    {
      gtk_style_context_remove_class (context, "color-light");
      gtk_style_context_add_class (context, "color-dark");
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
  g_autoptr (GDateTime) tooltip_start, tooltip_end;
  g_autofree gchar *start, *end, *escaped_summary, *escaped_description;
  gboolean allday, multiday, is_ltr;
  GString *tooltip_desc, *tooltip_mesg;

  tooltip_mesg = g_string_new (NULL);
  escaped_summary = g_markup_escape_text (gcal_event_get_summary (event), -1);
  g_string_append_printf (tooltip_mesg, "<b>%s</b>", escaped_summary);

  tooltip_start = g_date_time_to_local (gcal_event_get_date_start (event));
  tooltip_end = g_date_time_to_local (gcal_event_get_date_end (event));

  allday = gcal_event_get_all_day (event);
  multiday = gcal_event_is_multiday (event);

  is_ltr = gtk_widget_get_direction (GTK_WIDGET (self)) != GTK_TEXT_DIR_RTL;

  if (allday)
    {
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
      if (multiday)
        {
          if (self->clock_format_24h)
            {
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
            }
          else
            {
              if (is_ltr)
                {
                  start = g_date_time_format (tooltip_start, "%x I:%M %P");
                  end = g_date_time_format (tooltip_end, "%x I:%M %P");
                }
              else
                {
                  start = g_date_time_format (tooltip_start, "%P %M:%I %x");
                  end = g_date_time_format (tooltip_end, "%P %M:%I %x");
                }
            }
        }
      else
        {
          if (self->clock_format_24h)
            {
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
            }
          else
            {
              if (is_ltr)
                {
                  start = g_date_time_format (tooltip_start, "%x, I:%M %P");
                  end = g_date_time_format (tooltip_end, "I:%M %P");
                }
              else
                {
                  start = g_date_time_format (tooltip_start, "%P %M:%I ,%x");
                  end = g_date_time_format (tooltip_end, "%P %M:%I");
                }
            }
        }
    }

  if (allday && !multiday)
    {
      g_string_append_printf (tooltip_mesg,
                              "\n%s",
                              start);
    }
  else
    {
      g_string_append_printf (tooltip_mesg,
                              "\n%s - %s",
                              is_ltr ? start : end,
                              is_ltr ? end : start);
    }

  /* Append event location */
  if (g_utf8_strlen (gcal_event_get_location (event), -1) > 0)
    {
      g_autofree gchar *location;

      escaped_location = g_markup_escape_text (gcal_event_get_location (event), -1);

      g_string_append (tooltip_mesg, "\n\n");
      g_string_append_printf (tooltip_mesg, "%s", escaped_location);
    }

  escaped_description = g_markup_escape_text (gcal_event_get_description (event), -1);
  tooltip_desc = g_string_new (escaped_description);

  /* Truncate long descriptions at a white space and ellipsize */
  if (tooltip_desc->len > 0)
    {
      /* If the description is larger than DESC_MAX_CHAR, ellipsize it */
      if (g_utf8_strlen (tooltip_desc->str, -1) > DESC_MAX_CHAR)
        {
          g_string_truncate (tooltip_desc, DESC_MAX_CHAR - 1);
          g_string_append (tooltip_desc, "…");
        }

      g_string_append_printf (tooltip_mesg, "\n\n%s", tooltip_desc->str);
    }

  gtk_widget_set_tooltip_markup (GTK_WIDGET (self), tooltip_mesg->str);

  g_string_free (tooltip_desc, TRUE);
  g_string_free (tooltip_mesg, TRUE);
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

  g_signal_connect_swapped (event,
                            "notify::color",
                            G_CALLBACK (update_color),
                            self);

  g_signal_connect_swapped (event,
                            "notify::summary",
                            G_CALLBACK (gtk_widget_queue_draw),
                            self);

  gcal_event_widget_set_event_tooltip (self, event);
}

static void
gcal_event_widget_init (GcalEventWidget *self)
{
  GtkWidget *widget;

  widget = GTK_WIDGET (self);
  self->clock_format_24h = is_clock_format_24h ();
  self->orientation = GTK_ORIENTATION_HORIZONTAL;

  gtk_widget_set_has_window (widget, FALSE);
  gtk_widget_set_can_focus (widget, TRUE);

  /* Setup the event widget as a drag source */
  gtk_drag_source_set (widget,
                       GDK_BUTTON1_MASK,
                       NULL,
                       0,
                       GDK_ACTION_MOVE);

  gtk_drag_source_add_text_targets (widget);
}

static void
gcal_event_widget_set_property (GObject      *object,
                                guint         property_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GcalEventWidget *self = GCAL_EVENT_WIDGET (object);

  switch (property_id)
    {
    case PROP_DATE_END:
      gcal_event_widget_set_date_end (self, g_value_get_boxed (value));
      break;

    case PROP_DATE_START:
      gcal_event_widget_set_date_start (self, g_value_get_boxed (value));
      break;

    case PROP_EVENT:
      gcal_event_widget_set_event_internal (self, g_value_get_object (value));
      break;

    case PROP_ORIENTATION:
      self->orientation = g_value_get_enum (value);
      gtk_widget_queue_draw (GTK_WIDGET (object));
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
    case PROP_DATE_END:
      g_value_set_boxed (value, self->dt_end);
      break;

    case PROP_DATE_START:
      g_value_set_boxed (value, self->dt_start);
      break;

    case PROP_EVENT:
      g_value_set_object (value, self->event);
      break;

    case PROP_ORIENTATION:
      g_value_set_enum (value, self->orientation);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
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

  G_OBJECT_CLASS (gcal_event_widget_parent_class)->finalize (object);
}

static void
gcal_event_widget_get_preferred_width (GtkWidget *widget,
                                       gint      *minimum,
                                       gint      *natural)
{
  GtkBorder border, padding;
  GtkStyleContext *context;
  PangoLayout *layout;
  gint layout_width;

  context = gtk_widget_get_style_context (widget);
  layout = gtk_widget_create_pango_layout (widget, "00:00:00 00:00");
  pango_layout_get_pixel_size (layout, &layout_width, NULL);
  g_object_unref (layout);

  gtk_style_context_get_border (context, gtk_style_context_get_state (context), &border);
  gtk_style_context_get_padding (context, gtk_style_context_get_state (context), &padding);

  if (minimum != NULL)
    *minimum = layout_width + padding.left + padding.right + border.left + border.right;
  if (natural != NULL)
    *natural = layout_width + padding.left + padding.right + border.left + border.right;
}

static void
gcal_event_widget_get_preferred_height (GtkWidget *widget,
                                        gint      *minimum,
                                        gint      *natural)
{
  GcalEventWidget *self;
  GtkBorder border, padding;
  GtkStyleContext *context;
  PangoLayout *layout;
  gint layout_height;
  gchar* display_text;

  self = GCAL_EVENT_WIDGET (widget);
  context = gtk_widget_get_style_context (widget);
  display_text = get_visible_text (widget, self->orientation);
  layout = gtk_widget_create_pango_layout (widget, display_text);
  pango_layout_get_pixel_size (layout, NULL, &layout_height);
  g_object_unref (layout);

  gtk_style_context_get_border (context, gtk_style_context_get_state (context), &border);
  gtk_style_context_get_padding (context, gtk_style_context_get_state (context), &padding);

  if (minimum != NULL)
    *minimum = layout_height + padding.top + padding.bottom + border.top + border.bottom;
  if (natural != NULL)
    *natural = layout_height + padding.top + padding.bottom + border.top + border.bottom;

  g_clear_pointer(&display_text, g_free);
}

static void
gcal_event_widget_realize (GtkWidget *widget)
{
  GcalEventWidget *self;
  GdkWindow *parent_window;
  GdkWindowAttr attributes;
  gint attributes_mask;
  GtkAllocation allocation;

  GdkCursor* pointer_cursor;

  self = GCAL_EVENT_WIDGET (widget);
  gtk_widget_set_realized (widget, TRUE);

  parent_window = gtk_widget_get_parent_window (widget);
  gtk_widget_set_window (widget, parent_window);
  g_object_ref (parent_window);

  gtk_widget_get_allocation (widget, &allocation);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.x = allocation.x;
  attributes.y = allocation.y;
  attributes.width = allocation.width;
  attributes.height = allocation.height;
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= (GDK_BUTTON_PRESS_MASK |
                            GDK_BUTTON_RELEASE_MASK |
                            GDK_BUTTON1_MOTION_MASK |
                            GDK_POINTER_MOTION_HINT_MASK |
                            GDK_POINTER_MOTION_MASK |
                            GDK_ENTER_NOTIFY_MASK |
                            GDK_LEAVE_NOTIFY_MASK |
                            GDK_SMOOTH_SCROLL_MASK |
                            GDK_SCROLL_MASK);
  attributes_mask = GDK_WA_X | GDK_WA_Y;

  self->event_window = gdk_window_new (parent_window,
                                       &attributes,
                                       attributes_mask);
  gtk_widget_register_window (widget, self->event_window);
  gdk_window_show (self->event_window);

  pointer_cursor = gdk_cursor_new_for_display (gdk_display_get_default (),
                                               GDK_HAND1);
  gdk_window_set_cursor (self->event_window, pointer_cursor);
}

static void
gcal_event_widget_unrealize (GtkWidget *widget)
{
  GcalEventWidget *self;

  self = GCAL_EVENT_WIDGET (widget);

  if (self->event_window != NULL)
    {
      gtk_widget_unregister_window (widget, self->event_window);
      gdk_window_destroy (self->event_window);
      self->event_window = NULL;
    }

  GTK_WIDGET_CLASS (gcal_event_widget_parent_class)->unrealize (widget);
}

static void
gcal_event_widget_map (GtkWidget *widget)
{
  GcalEventWidget *self;

  self = GCAL_EVENT_WIDGET (widget);

  GTK_WIDGET_CLASS (gcal_event_widget_parent_class)->map (widget);

  if (self->event_window != NULL)
    gdk_window_show (self->event_window);
}

static void
gcal_event_widget_unmap (GtkWidget *widget)
{
   GcalEventWidget *self;

  self = GCAL_EVENT_WIDGET (widget);

  GTK_WIDGET_CLASS (gcal_event_widget_parent_class)->unmap (widget);

  if (self->event_window != NULL)
    gdk_window_hide (self->event_window);
}

static void
gcal_event_widget_size_allocate (GtkWidget     *widget,
                                 GtkAllocation *allocation)
{
  GcalEventWidget *self;

  self = GCAL_EVENT_WIDGET (widget);
  gtk_widget_set_allocation (widget, allocation);

  if (gtk_widget_get_realized (widget))
    {
      gdk_window_move_resize (self->event_window,
                              allocation->x,
                              allocation->y,
                              allocation->width,
                              allocation->height);
    }
}

static gboolean
gcal_event_widget_draw (GtkWidget *widget,
                        cairo_t   *cr)
{
  GcalEventWidget *self;

  GtkStyleContext *context;
  GtkStateFlags state;
  GtkBorder padding;

  gint width, height, layout_height, x;
  gint end_gap, icon_size = 0;
  gboolean is_ltr;

  PangoLayout *layout;
  PangoFontDescription *font_desc;
  gchar *display_text;

  self = GCAL_EVENT_WIDGET (widget);
  context = gtk_widget_get_style_context (widget);
  state = gtk_style_context_get_state (context);
  is_ltr = gtk_widget_get_direction (widget) != GTK_TEXT_DIR_RTL;

  gtk_style_context_get_padding (context, state, &padding);

  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);

  gtk_render_background (context, cr, 0, 0, width, height);
  gtk_render_frame (context, cr, 0, 0, width, height);

  /* Set the text to be displayed */
  if (self->orientation == GTK_ORIENTATION_HORIZONTAL)
    display_text = get_visible_text (widget, GTK_ORIENTATION_HORIZONTAL);
  else
    {
      if (get_vertical_text_height (widget) >= height)
        display_text = get_visible_text (widget, GTK_ORIENTATION_HORIZONTAL);
      else
        display_text = get_visible_text (widget, GTK_ORIENTATION_VERTICAL);
    }

  /* Retrieve the font description */
  gtk_style_context_get (context, state, "font", &font_desc, NULL);
  layout = gtk_widget_create_pango_layout (widget, "");
  pango_layout_set_markup (layout, display_text, -1);
  pango_layout_set_font_description (layout, font_desc);
  pango_layout_set_ellipsize (layout, PANGO_ELLIPSIZE_END);

  /* Icon size */
  icon_size = ICON_SIZE;
  end_gap = 0;

  if (gcal_event_has_alarms (self->event))
    end_gap += icon_size + (is_ltr ? padding.left : padding.right);

  if (self->read_only)
    end_gap += icon_size + (is_ltr ? padding.left : padding.right);

  x = is_ltr ? padding.left : padding.left + end_gap;

  pango_layout_set_width (layout, (width - (padding.left + padding.right + end_gap)) * PANGO_SCALE);
  pango_layout_get_pixel_size (layout, NULL, &layout_height);
  gtk_render_layout (context, cr, x, padding.top, layout);

#if 0
  /* TODO: find out a good way to handle this */
  /*
   * On vertical event widgets, we assume we might have space to draw the
   * event's detail (or at least part of it).
   */
  if (self->orientation == GTK_ORIENTATION_VERTICAL &&
      2 * padding.top + layout_height + padding.bottom < height &&
      strlen (gcal_event_get_description (self->event)) > 0)
    {
      g_autofree gchar *description;

      description = g_strdup_printf ("<small>%s</small>", gcal_event_get_description (self->event));
      pango_layout_set_markup (layout, description, -1);
      pango_layout_set_width (layout, (width - (padding.left + padding.right) ) * PANGO_SCALE);
      pango_layout_set_height (layout, (height - (2 * padding.top + layout_height + padding.bottom) ) * PANGO_SCALE);
      pango_layout_set_font_description (layout, font_desc);
      pango_layout_set_ellipsize (layout, PANGO_ELLIPSIZE_END);

      gtk_render_layout (context,
                         cr,
                         x,
                         2 * padding.top + layout_height + padding.bottom,
                         layout);
    }
#endif

  x += is_ltr ? (width - end_gap - padding.left - padding.right) : (- padding.right - padding.left - icon_size);

  /* render reminder icon */
  if (gcal_event_has_alarms (self->event))
    {
      GtkIconTheme *icon_theme;
      GtkIconInfo *icon_info;
      GdkPixbuf *pixbuf;
      gboolean was_symbolic;

      icon_theme = gtk_icon_theme_get_default ();
      icon_info = gtk_icon_theme_lookup_icon (icon_theme,
                                              "alarm-symbolic",
                                              icon_size,
                                              0);
      pixbuf = gtk_icon_info_load_symbolic_for_context (icon_info,
                                                        context,
                                                        &was_symbolic,
                                                        NULL);

      gtk_render_icon (context,
                       cr,
                       pixbuf,
                       x + padding.left,
                       padding.top);

      x += is_ltr ? (padding.left + icon_size) : (- padding.right - icon_size);

      g_object_unref (pixbuf);
      cairo_paint (cr);
    }

  /* render locked icon */
  if (self->read_only)
    {
      GtkIconTheme *icon_theme;
      GtkIconInfo *icon_info;
      GdkPixbuf *pixbuf;
      gboolean was_symbolic;

      icon_theme = gtk_icon_theme_get_default ();
      icon_info = gtk_icon_theme_lookup_icon (icon_theme,
                                              "changes-prevent-symbolic",
                                              icon_size,
                                              0);
      pixbuf = gtk_icon_info_load_symbolic_for_context (icon_info,
                                                        context,
                                                        &was_symbolic,
                                                        NULL);

      gtk_render_icon (context,
                       cr,
                       pixbuf,
                       x + padding.left,
                       padding.top);

      g_object_unref (pixbuf);
      cairo_paint (cr);
    }

  pango_font_description_free (font_desc);
  g_object_unref (layout);
  g_free (display_text);

  return FALSE;
}

static gboolean
gcal_event_widget_button_press_event (GtkWidget      *widget,
                                      GdkEventButton *event)
{
  GcalEventWidget *self;

  self = GCAL_EVENT_WIDGET (widget);
  self->button_pressed = TRUE;
  return TRUE;
}

static gboolean
gcal_event_widget_button_release_event (GtkWidget      *widget,
                                        GdkEventButton *event)
{
  GcalEventWidget *self;

  self = GCAL_EVENT_WIDGET (widget);

  if (self->button_pressed)
    {
      self->button_pressed = FALSE;
      g_signal_emit (widget, signals[ACTIVATE], 0);
      return TRUE;
    }

  return FALSE;
}

static cairo_surface_t*
get_dnd_icon (GtkWidget *widget)
{
  cairo_surface_t *surface;
  cairo_t *cr;

  /* Make it transparent */
  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                        gtk_widget_get_allocated_width (widget),
                                        gtk_widget_get_allocated_height (widget));

  cr = cairo_create (surface);

  gtk_widget_draw (widget, cr);

  return surface;
}

static void
gcal_event_widget_drag_begin (GtkWidget      *widget,
                              GdkDragContext *context)
{
  GcalEventWidget *self = GCAL_EVENT_WIDGET (widget);
  cairo_surface_t *surface;

  if (self->read_only)
    {
      gtk_drag_cancel (context);
      return;
    }

  /* Setup the drag n' drop icon */
  surface = get_dnd_icon (widget);

  gtk_drag_set_icon_surface (context, surface);

  g_clear_pointer (&surface, cairo_surface_destroy);
}

static gboolean
gcal_event_widget_scroll_event (GtkWidget      *widget,
                                GdkEventScroll *scroll_event)
{
  return GDK_EVENT_PROPAGATE;
}

static void
gcal_event_widget_class_init(GcalEventWidgetClass *klass)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->set_property = gcal_event_widget_set_property;
  object_class->get_property = gcal_event_widget_get_property;
  object_class->finalize = gcal_event_widget_finalize;

  widget_class = GTK_WIDGET_CLASS (klass);
  widget_class->get_preferred_width = gcal_event_widget_get_preferred_width;
  widget_class->get_preferred_height = gcal_event_widget_get_preferred_height;
  widget_class->realize = gcal_event_widget_realize;
  widget_class->unrealize = gcal_event_widget_unrealize;
  widget_class->map = gcal_event_widget_map;
  widget_class->unmap = gcal_event_widget_unmap;
  widget_class->size_allocate = gcal_event_widget_size_allocate;
  widget_class->draw = gcal_event_widget_draw;
  widget_class->button_press_event = gcal_event_widget_button_press_event;
  widget_class->button_release_event = gcal_event_widget_button_release_event;
  widget_class->drag_begin = gcal_event_widget_drag_begin;
  widget_class->scroll_event = gcal_event_widget_scroll_event;

  /**
   * GcalEventWidget::date-end:
   *
   * The end date this widget represents. Notice that this may
   * differ from the event's end date. For example, if the event
   * spans more than one month and we're in Month View, the end
   * date marks the last day this event is visible.
   */
  g_object_class_install_property (object_class,
                                   PROP_DATE_END,
                                   g_param_spec_boxed ("date-end",
                                                       "End date",
                                                       "The end date of the widget",
                                                       G_TYPE_DATE_TIME,
                                                       G_PARAM_READWRITE));

  /**
   * GcalEventWidget::date-start:
   *
   * The start date this widget represents. Notice that this may
   * differ from the event's start date. For example, if the event
   * spans more than one month and we're in Month View, the start
   * date marks the first day this event is visible.
   */
  g_object_class_install_property (object_class,
                                   PROP_DATE_START,
                                   g_param_spec_boxed ("date-start",
                                                       "Start date",
                                                       "The start date of the widget",
                                                       G_TYPE_DATE_TIME,
                                                       G_PARAM_READWRITE));

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
   * GcalEventWidget::orientation:
   *
   * The orientation of the event widget.
   */
  g_object_class_override_property (object_class, PROP_ORIENTATION, "orientation");

  signals[ACTIVATE] = g_signal_new ("activate",
                                     GCAL_TYPE_EVENT_WIDGET,
                                     G_SIGNAL_RUN_LAST,
                                     0,
                                     NULL, NULL,
                                     g_cclosure_marshal_VOID__VOID,
                                     G_TYPE_NONE,
                                     0);

  gtk_widget_class_set_css_name (widget_class, "event-widget");
}


GtkWidget*
gcal_event_widget_new (GcalEvent *event)
{
  return g_object_new (GCAL_TYPE_EVENT_WIDGET,
                       "event", event,
                       NULL);
}

void
gcal_event_widget_set_read_only (GcalEventWidget *event,
                                 gboolean         read_only)
{
  g_return_if_fail (GCAL_IS_EVENT_WIDGET (event));

  if (read_only)
    {
      GtkWidget *widget = GTK_WIDGET (event);

      /* Setup the event widget as a drag source */
      gtk_drag_source_set (widget,
                           0,
                           NULL,
                           0,
                           GDK_ACTION_MOVE);

      gtk_drag_source_add_text_targets (widget);
    }

  event->read_only = read_only;
}

gboolean
gcal_event_widget_get_read_only (GcalEventWidget *event)
{
  return event->read_only;
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

      g_object_notify (G_OBJECT (self), "date-end");
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
 * @date_end: the start date of this widget
 *
 * Sets the visible start date of this widget. This
 * may differ from the event's start date, but cannot
 * be before it.
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

      g_object_notify (G_OBJECT (self), "date-start");
    }
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
  GtkWidget *new_widget;

  new_widget = gcal_event_widget_new (widget->event);
  gcal_event_widget_set_read_only(GCAL_EVENT_WIDGET (new_widget), gcal_event_widget_get_read_only (widget));

  return new_widget;
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
  if (!e_source_equal (gcal_event_get_source (widget1->event), gcal_event_get_source (widget2->event)))
    return FALSE;

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
