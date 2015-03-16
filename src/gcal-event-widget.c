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

#include "gcal-event-widget.h"
#include "gcal-utils.h"

#define INTENSITY(r, g, b) ((r) * 0.30 + (g) * 0.59 + (b) * 0.11)

typedef struct
{
  /* properties */
  gchar         *uuid;
  gchar         *summary;
  GdkRGBA       *color;
  icaltimetype  *dt_start;
  icaltimetype  *dt_end; /* could be NULL, meaning dt_end is the same as start_date */
  gboolean       all_day;
  gboolean       has_reminders;

  /* internal data */
  gboolean       read_only;

  /* weak ESource reference */
  ESource       *source;
  /* ECalComponent data */
  ECalComponent *component;

  GdkWindow     *event_window;
  gboolean       button_pressed;
} GcalEventWidgetPrivate;

enum
{
  PROP_0,
  PROP_UUID,
  PROP_SUMMARY,
  PROP_COLOR,
  PROP_DTSTART,
  PROP_DTEND,
  PROP_ALL_DAY,
  PROP_HAS_REMINDERS
};

enum
{
  ACTIVATE,
  NUM_SIGNALS
};

static guint signals[NUM_SIGNALS] = { 0, };

static void     gcal_event_widget_set_property         (GObject        *object,
                                                        guint           property_id,
                                                        const GValue   *value,
                                                        GParamSpec     *pspec);

static void     gcal_event_widget_get_property         (GObject        *object,
                                                        guint           property_id,
                                                        GValue         *value,
                                                        GParamSpec     *pspec);

static void     gcal_event_widget_finalize             (GObject        *object);

static void     gcal_event_widget_get_preferred_width  (GtkWidget      *widget,
                                                        gint           *minimum,
                                                        gint           *natural);

static void     gcal_event_widget_get_preferred_height (GtkWidget      *widget,
                                                        gint           *minimum,
                                                        gint           *natural);

static void     gcal_event_widget_realize              (GtkWidget      *widget);

static void     gcal_event_widget_unrealize            (GtkWidget      *widget);

static void     gcal_event_widget_map                  (GtkWidget      *widget);

static void     gcal_event_widget_unmap                (GtkWidget      *widget);

static void     gcal_event_widget_size_allocate        (GtkWidget      *widget,
                                                        GtkAllocation  *allocation);

static gboolean gcal_event_widget_draw                 (GtkWidget      *widget,
                                                        cairo_t        *cr);

static gboolean gcal_event_widget_button_press_event   (GtkWidget      *widget,
                                                        GdkEventButton *event);

static gboolean gcal_event_widget_button_release_event (GtkWidget      *widget,
                                                        GdkEventButton *event);

G_DEFINE_TYPE_WITH_PRIVATE (GcalEventWidget, gcal_event_widget, GTK_TYPE_WIDGET)

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

  g_object_class_install_property (object_class,
                                   PROP_UUID,
                                   g_param_spec_string ("uuid",
                                                        "Unique uid",
                                                        "The unique-unique id composed of source_uid:event_uid",
                                                        NULL,
                                                        G_PARAM_CONSTRUCT |
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_SUMMARY,
                                   g_param_spec_string ("summary",
                                                        "Summary",
                                                        "The event summary",
                                                        NULL,
                                                        G_PARAM_CONSTRUCT |
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_COLOR,
                                   g_param_spec_boxed ("color",
                                                       "Color",
                                                       "The color to render",
                                                       GDK_TYPE_RGBA,
                                                       G_PARAM_CONSTRUCT |
                                                       G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_DTSTART,
                                   g_param_spec_boxed ("date-start",
                                                       "Date Start",
                                                       "The starting date of the event",
                                                       ICAL_TIME_TYPE,
                                                       G_PARAM_CONSTRUCT |
                                                       G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_DTEND,
                                   g_param_spec_boxed ("date-end",
                                                       "Date End",
                                                       "The end date of the event",
                                                       ICAL_TIME_TYPE,
                                                       G_PARAM_CONSTRUCT |
                                                       G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_ALL_DAY,
                                   g_param_spec_boolean ("all-day",
                                                         "All day",
                                                         "Wheter the event is all-day or not",
                                                         FALSE,
                                                         G_PARAM_CONSTRUCT |
                                                         G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_HAS_REMINDERS,
                                   g_param_spec_boolean ("has-reminders",
                                                         "Event has reminders",
                                                         "Wheter the event has reminders set or not",
                                                         FALSE,
                                                         G_PARAM_CONSTRUCT |
                                                         G_PARAM_READWRITE));

  signals[ACTIVATE] = g_signal_new ("activate",
                                     GCAL_TYPE_EVENT_WIDGET,
                                     G_SIGNAL_RUN_LAST,
                                     G_STRUCT_OFFSET (GcalEventWidgetClass,
                                                      activate),
                                     NULL, NULL,
                                     g_cclosure_marshal_VOID__VOID,
                                     G_TYPE_NONE,
                                     0);
}

static void
gcal_event_widget_init(GcalEventWidget *self)
{
  gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);
  gtk_widget_set_can_focus (GTK_WIDGET (self), TRUE);
}

static void
gcal_event_widget_set_property (GObject      *object,
                                guint         property_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GcalEventWidgetPrivate *priv;

  priv = gcal_event_widget_get_instance_private (GCAL_EVENT_WIDGET (object));

  switch (property_id)
    {
    case PROP_UUID:
      if (priv->uuid != NULL)
        g_free (priv->uuid);

      priv->uuid = g_value_dup_string (value);
      return;
    case PROP_SUMMARY:
      if (priv->summary != NULL)
        g_free (priv->summary);

      priv->summary = g_value_dup_string (value);
      return;
    case PROP_COLOR:
      {
        GtkStyleContext *context;

        context = gtk_widget_get_style_context (GTK_WIDGET (object));

        if (priv->color != NULL)
          gdk_rgba_free (priv->color);

        priv->color = g_value_dup_boxed (value);

        if (priv->color == NULL)
          return;

        if (INTENSITY (priv->color->red,
                       priv->color->green,
                       priv->color->blue) > 0.5)
          {
            gtk_style_context_remove_class (context, "color-dark");
            gtk_style_context_add_class (context, "color-light");
          }
        else
          {
            gtk_style_context_remove_class (context, "color-light");
            gtk_style_context_add_class (context, "color-dark");
          }

        return;
      }
    case PROP_DTSTART:
      if (priv->dt_start != NULL)
        g_free (priv->dt_start);

      priv->dt_start = g_value_dup_boxed (value);
      return;
    case PROP_DTEND:
      if (priv->dt_end != NULL)
        g_free (priv->dt_end);

      priv->dt_end = g_value_dup_boxed (value);
      return;
    case PROP_ALL_DAY:
      priv->all_day = g_value_get_boolean (value);
      return;
    case PROP_HAS_REMINDERS:
      priv->has_reminders = g_value_get_boolean (value);
      return;
    }

  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
gcal_event_widget_get_property (GObject      *object,
                                guint         property_id,
                                GValue       *value,
                                GParamSpec   *pspec)
{
  GcalEventWidgetPrivate *priv;

  priv = gcal_event_widget_get_instance_private (GCAL_EVENT_WIDGET (object));

  switch (property_id)
    {
    case PROP_UUID:
      g_value_set_string (value, priv->uuid);
      return;
    case PROP_SUMMARY:
      g_value_set_string (value, priv->summary);
      return;
    case PROP_COLOR:
      g_value_set_boxed (value, priv->color);
      return;
    case PROP_DTSTART:
      g_value_set_boxed (value, priv->dt_start);
      return;
    case PROP_DTEND:
      g_value_set_boxed (value, priv->dt_end);
      return;
    case PROP_ALL_DAY:
      g_value_set_boolean (value, priv->all_day);
      return;
    case PROP_HAS_REMINDERS:
      g_value_set_boolean (value, priv->has_reminders);
      return;
    }

  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
gcal_event_widget_finalize (GObject *object)
{
  GcalEventWidgetPrivate *priv;

  priv =
    gcal_event_widget_get_instance_private (GCAL_EVENT_WIDGET (object));

  if (priv->component != NULL)
    g_object_unref (priv->component);

  /* releasing properties */
  if (priv->uuid != NULL)
    g_free (priv->uuid);

  if (priv->summary != NULL)
    g_free (priv->summary);

  if (priv->color != NULL)
    gdk_rgba_free (priv->color);

  if (priv->dt_start != NULL)
    g_free (priv->dt_start);

  if (priv->dt_end != NULL)
    g_free (priv->dt_end);

  G_OBJECT_CLASS (gcal_event_widget_parent_class)->finalize (object);
}

static void
gcal_event_widget_get_preferred_width (GtkWidget *widget,
                                       gint      *minimum,
                                       gint      *natural)
{
  GtkBorder border, padding;
  PangoLayout *layout;
  gint layout_width;

  layout = gtk_widget_create_pango_layout (widget, "00:00:00 00:00");
  pango_layout_get_pixel_size (layout, &layout_width, NULL);
  g_object_unref (layout);

  gtk_style_context_get_border (gtk_widget_get_style_context (widget), gtk_widget_get_state_flags (widget), &border);
  gtk_style_context_get_padding (gtk_widget_get_style_context (widget), gtk_widget_get_state_flags (widget), &padding);

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
  GtkBorder border, padding;
  PangoLayout *layout;
  gint layout_height;

  layout = gtk_widget_create_pango_layout (widget, NULL);
  pango_layout_get_pixel_size (layout, NULL, &layout_height);
  g_object_unref (layout);

  gtk_style_context_get_border (gtk_widget_get_style_context (widget), gtk_widget_get_state_flags (widget), &border);
  gtk_style_context_get_padding (gtk_widget_get_style_context (widget), gtk_widget_get_state_flags (widget), &padding);

  if (minimum != NULL)
    *minimum = layout_height + padding.top + padding.bottom + border.top + border.bottom;
  if (natural != NULL)
    *natural = layout_height + padding.top + padding.bottom + border.top + border.bottom;
}

static void
gcal_event_widget_realize (GtkWidget *widget)
{
  GcalEventWidgetPrivate *priv;
  GdkWindow *parent_window;
  GdkWindowAttr attributes;
  gint attributes_mask;
  GtkAllocation allocation;

  GdkCursor* pointer_cursor;

  priv = gcal_event_widget_get_instance_private (GCAL_EVENT_WIDGET (widget));
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
                            GDK_LEAVE_NOTIFY_MASK);
  attributes_mask = GDK_WA_X | GDK_WA_Y;

  priv->event_window = gdk_window_new (parent_window,
                                       &attributes,
                                       attributes_mask);
  gtk_widget_register_window (widget, priv->event_window);
  gdk_window_show (priv->event_window);

  pointer_cursor = gdk_cursor_new_for_display (gdk_display_get_default (),
                                               GDK_HAND1);
  gdk_window_set_cursor (priv->event_window, pointer_cursor);
}

static void
gcal_event_widget_unrealize (GtkWidget *widget)
{
  GcalEventWidgetPrivate *priv;

  priv = gcal_event_widget_get_instance_private (GCAL_EVENT_WIDGET (widget));
  if (priv->event_window != NULL)
    {
      gtk_widget_unregister_window (widget, priv->event_window);
      gdk_window_destroy (priv->event_window);
      priv->event_window = NULL;
    }

  GTK_WIDGET_CLASS (gcal_event_widget_parent_class)->unrealize (widget);
}

static void
gcal_event_widget_map (GtkWidget *widget)
{
  GcalEventWidgetPrivate *priv;

  priv = gcal_event_widget_get_instance_private (GCAL_EVENT_WIDGET (widget));

  GTK_WIDGET_CLASS (gcal_event_widget_parent_class)->map (widget);

  if (priv->event_window != NULL)
    gdk_window_show (priv->event_window);
}

static void
gcal_event_widget_unmap (GtkWidget *widget)
{
  GcalEventWidgetPrivate *priv;

  priv = gcal_event_widget_get_instance_private (GCAL_EVENT_WIDGET (widget));

  GTK_WIDGET_CLASS (gcal_event_widget_parent_class)->unmap (widget);

  if (priv->event_window != NULL)
    gdk_window_hide (priv->event_window);
}

static void
gcal_event_widget_size_allocate (GtkWidget     *widget,
                                 GtkAllocation *allocation)
{
  GcalEventWidgetPrivate *priv;

  priv = gcal_event_widget_get_instance_private (GCAL_EVENT_WIDGET (widget));
  gtk_widget_set_allocation (widget, allocation);

  if (gtk_widget_get_realized (widget))
    {
      gdk_window_move_resize (priv->event_window,
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
  GcalEventWidgetPrivate *priv;

  GtkStyleContext *context;
  GtkStateFlags state;
  GtkBorder padding;

  gint width, height, layout_height;
  gint left_gap, right_gap, icon_size = 0;

  PangoLayout *layout;
  PangoFontDescription *font_desc;

  priv = gcal_event_widget_get_instance_private (GCAL_EVENT_WIDGET (widget));
  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);

  gtk_style_context_get_padding (context, state, &padding);

  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);

  gtk_render_background (context, cr, 0, 0, width, height);
  gtk_render_frame (context, cr, 0, 0, width, height);

  /* FIXME for RTL alignment and icons positions */
  gtk_style_context_get (context, state, "font", &font_desc, NULL);
  layout = gtk_widget_create_pango_layout (widget, priv->summary);
  pango_layout_set_font_description (layout, font_desc);
  pango_layout_set_ellipsize (layout, PANGO_ELLIPSIZE_END);
  pango_layout_set_width (layout, (width - (padding.left + padding.right) ) * PANGO_SCALE);
  pango_cairo_update_layout (cr, layout);

  left_gap = 0;
  if (priv->has_reminders)
    {
      pango_layout_get_pixel_size (layout, NULL, &layout_height);
      icon_size = layout_height;
      left_gap = icon_size + padding.left;
      pango_layout_set_width (layout, (width - (left_gap + padding.left + padding.right) ) * PANGO_SCALE);
    }

  right_gap = 0;
  if (priv->read_only)
    {
      if (icon_size == 0)
        {
          pango_layout_get_pixel_size (layout, NULL, &layout_height);
          icon_size = layout_height;
        }

      right_gap = icon_size + padding.right;
      pango_layout_set_width (layout, (width - (left_gap + padding.left + padding.right + right_gap) ) * PANGO_SCALE);
    }

  gtk_render_layout (context, cr, padding.left + left_gap, padding.top, layout);

  /* render reminder icon */
  if (priv->has_reminders)
    {
      GtkIconTheme *icon_theme;
      GtkIconInfo *icon_info;
      GdkPixbuf *pixbuf;
      gboolean was_symbolic;
      gint multiplier;

      multiplier = icon_size / 16;
      icon_theme = gtk_icon_theme_get_default ();
      icon_info = gtk_icon_theme_lookup_icon (icon_theme,
                                              "alarm-symbolic",
                                              16 * multiplier,
                                              0);
      pixbuf = gtk_icon_info_load_symbolic_for_context (icon_info,
                                                        context,
                                                        &was_symbolic,
                                                        NULL);

      gdk_cairo_set_source_pixbuf (cr, pixbuf, padding.left, padding.top + ((icon_size - (16 * multiplier)) / 2));
      g_object_unref (pixbuf);
      cairo_paint (cr);
    }

  /* render locked icon */
  if (priv->read_only)
    {
      GtkIconTheme *icon_theme;
      GtkIconInfo *icon_info;
      GdkPixbuf *pixbuf;
      gboolean was_symbolic;
      gint multiplier;

      multiplier = icon_size / 16;
      icon_theme = gtk_icon_theme_get_default ();
      icon_info = gtk_icon_theme_lookup_icon (icon_theme,
                                              "changes-prevent-symbolic",
                                              16 * multiplier,
                                              0);
      pixbuf = gtk_icon_info_load_symbolic_for_context (icon_info,
                                                        context,
                                                        &was_symbolic,
                                                        NULL);

      gdk_cairo_set_source_pixbuf (cr, pixbuf, width - right_gap, padding.top + ((icon_size - (16 * multiplier)) / 2));
      g_object_unref (pixbuf);
      cairo_paint (cr);
    }

  pango_font_description_free (font_desc);
  g_object_unref (layout);

  return FALSE;
}

static gboolean
gcal_event_widget_button_press_event (GtkWidget      *widget,
                                      GdkEventButton *event)
{
  GcalEventWidgetPrivate *priv;

  priv = gcal_event_widget_get_instance_private (GCAL_EVENT_WIDGET (widget));
  priv->button_pressed = TRUE;

  return TRUE;
}

static gboolean
gcal_event_widget_button_release_event (GtkWidget      *widget,
                                        GdkEventButton *event)
{
  GcalEventWidgetPrivate *priv;

  priv = gcal_event_widget_get_instance_private (GCAL_EVENT_WIDGET (widget));

  if (priv->button_pressed)
    {
      priv->button_pressed = FALSE;
      g_signal_emit (widget, signals[ACTIVATE], 0);
      return TRUE;
    }

  return FALSE;
}

GtkWidget*
gcal_event_widget_new (gchar *uuid)
{
  return g_object_new (GCAL_TYPE_EVENT_WIDGET, "uuid", uuid, NULL);
}

/**
 * gcal_event_widget_new_from_data:
 * @data: a #GcalEventData instance
 *
 * Create an event widget by passing its #ECalComponent and #ESource
 *
 * Returns: a #GcalEventWidget as #GtkWidget
 **/
GtkWidget*
gcal_event_widget_new_from_data (GcalEventData *data)
{
  GtkWidget *widget;
  GcalEventWidget *event;
  GcalEventWidgetPrivate *priv;

  gchar *uuid;
  ECalComponentId *id;
  ECalComponentText e_summary;

  GQuark color_id;
  GdkRGBA color;
  gchar *color_str, *custom_css_class;

  ECalComponentDateTime dt;
  icaltimetype *date;
  gboolean start_is_date, end_is_date;

  id = e_cal_component_get_id (data->event_component);
  if (id->rid != NULL)
    {
      uuid = g_strdup_printf ("%s:%s:%s",
                              e_source_get_uid (data->source),
                              id->uid,
                              id->rid);
    }
  else
    {
      uuid = g_strdup_printf ("%s:%s",
                              e_source_get_uid (data->source),
                              id->uid);
    }
  widget = g_object_new (GCAL_TYPE_EVENT_WIDGET, "uuid", uuid, NULL);
  e_cal_component_free_id (id);
  g_free (uuid);

  event = GCAL_EVENT_WIDGET (widget);

  priv = gcal_event_widget_get_instance_private (event);
  priv->component = data->event_component;
  priv->source = data->source;

  /* summary */
  e_cal_component_get_summary (priv->component, &e_summary);
  gcal_event_widget_set_summary (event, (gchar*) e_summary.value);

  /* color */
  get_color_name_from_source (priv->source, &color);
  gcal_event_widget_set_color (event, &color);

  color_str = gdk_rgba_to_string (&color);
  color_id = g_quark_from_string (color_str);
  custom_css_class = g_strdup_printf ("color-%d", color_id);
  gtk_style_context_add_class (gtk_widget_get_style_context (widget), custom_css_class);
  g_free (custom_css_class);
  g_free (color_str);

  /* start date */
  e_cal_component_get_dtstart (priv->component, &dt);
  date = gcal_dup_icaltime (dt.value);

  start_is_date = date->is_date == 1;
  if (!start_is_date)
    {
      if (dt.tzid != NULL)
        dt.value->zone = icaltimezone_get_builtin_timezone_from_tzid (dt.tzid);
      *date = icaltime_convert_to_zone (*(dt.value),
                                        e_cal_util_get_system_timezone ());
    }

  gcal_event_widget_set_date (event, date);
  e_cal_component_free_datetime (&dt);
  g_free (date);

  /* end date */
  e_cal_component_get_dtend (priv->component, &dt);
  if (dt.value != NULL)
    {
      date = gcal_dup_icaltime (dt.value);

      end_is_date = date->is_date == 1;
      if (!end_is_date)
        {
          if (dt.tzid != NULL)
            dt.value->zone = icaltimezone_get_builtin_timezone_from_tzid (dt.tzid);
          *date = icaltime_convert_to_zone (*(dt.value),
                                            e_cal_util_get_system_timezone ());
        }

      gcal_event_widget_set_end_date (event, date);
      e_cal_component_free_datetime (&dt);
      g_free (date);

      /* set_all_day */
      gcal_event_widget_set_all_day (event, start_is_date && end_is_date);
    }

  /* set_has_reminders */
  gcal_event_widget_set_has_reminders (
      event,
      e_cal_component_has_alarms (priv->component));

  return widget;
}

GtkWidget*
gcal_event_widget_new_with_summary_and_color (const gchar   *summary,
                                              const GdkRGBA *color)
{
  return g_object_new (GCAL_TYPE_EVENT_WIDGET,
                       "summary",
                       summary,
                       "color",
                       color,
                       NULL);
}

GtkWidget*
gcal_event_widget_clone (GcalEventWidget *widget)
{
  GtkWidget *new_widget;
  GcalEventData *data;

  data = gcal_event_widget_get_data (widget);
  g_object_ref (data->event_component);

  new_widget = gcal_event_widget_new_from_data (data);
  g_free (data);

  gcal_event_widget_set_read_only(GCAL_EVENT_WIDGET (new_widget), gcal_event_widget_get_read_only (widget));
  return new_widget;
}

const gchar*
gcal_event_widget_peek_uuid (GcalEventWidget *event)
{
  GcalEventWidgetPrivate *priv;
  priv = gcal_event_widget_get_instance_private (event);

  return priv->uuid;
}

void
gcal_event_widget_set_read_only (GcalEventWidget *event,
                                 gboolean         read_only)
{
  GcalEventWidgetPrivate *priv;

  priv = gcal_event_widget_get_instance_private (event);

  priv->read_only = read_only;
}

gboolean
gcal_event_widget_get_read_only (GcalEventWidget *event)
{
  GcalEventWidgetPrivate *priv;
  priv = gcal_event_widget_get_instance_private (event);

  return priv->read_only;
}

/**
 * gcal_event_widget_set_date:
 * @event: a #GcalEventWidget
 * @date: a #icaltimetype object with the date
 *
 * Set the start-date of the event
 **/
void
gcal_event_widget_set_date (GcalEventWidget    *event,
                            const icaltimetype *date)
{
  g_object_set (event, "date-start", date, NULL);
}

/**
 * gcal_event_widget_get_date:
 * @event: a #GcalEventWidget
 *
 * Return the starting date of the event
 *
 * Returns: (transfer full): Release with g_free()
 **/
icaltimetype*
gcal_event_widget_get_date (GcalEventWidget *event)
{
  icaltimetype *dt;

  g_object_get (event, "date-start", &dt, NULL);
  return dt;
}

/**
 * gcal_event_widget_peek_start_date:
 * @event:
 *
 * Return the starting date of the event.
 *
 * Returns: (Transfer none): An {@link icaltimetype} instance
 **/
const icaltimetype*
gcal_event_widget_peek_start_date (GcalEventWidget *event)
{
  GcalEventWidgetPrivate *priv;
  priv = gcal_event_widget_get_instance_private (event);

  return priv->dt_start;
}

/**
 * gcal_event_widget_set_end_date:
 * @event: a #GcalEventWidget
 * @date: a #icaltimetype object with the date
 *
 * Set the end date of the event
 **/
void
gcal_event_widget_set_end_date (GcalEventWidget    *event,
                                const icaltimetype *date)
{
  g_object_set (event, "date-end", date, NULL);
}

/**
 * gcal_event_widget_get_end_date:
 * @event: a #GcalEventWidget
 *
 * Return the end date of the event. If the event has no end_date
 * (as Google does on 0 sec events) %NULL will be returned
 *
 * Returns: (transfer full): Release with g_free()
 **/
icaltimetype*
gcal_event_widget_get_end_date (GcalEventWidget *event)
{
  icaltimetype *dt;

  g_object_get (event, "date-end", &dt, NULL);
  return dt;
}

/**
 * gcal_event_widget_peek_end_date:
 * @event:
 *
 * Return the end date of the event.
 *
 * Returns: (Transfer none): An {@link icaltimetype} instance
 **/
const icaltimetype*
gcal_event_widget_peek_end_date (GcalEventWidget *event)
{
  GcalEventWidgetPrivate *priv;
  priv = gcal_event_widget_get_instance_private (event);

  return priv->dt_end != NULL ? priv->dt_end : priv->dt_start;
}

void
gcal_event_widget_set_summary (GcalEventWidget *event,
                               gchar           *summary)
{
  g_return_if_fail (GCAL_IS_EVENT_WIDGET (event));

  g_object_set (event, "summary", summary, NULL);
}

gchar*
gcal_event_widget_get_summary (GcalEventWidget *event)
{
  GcalEventWidgetPrivate *priv;

  priv = gcal_event_widget_get_instance_private (event);

  return g_strdup (priv->summary);
}

void
gcal_event_widget_set_color (GcalEventWidget *event,
                             GdkRGBA         *color)
{
  g_return_if_fail (GCAL_IS_EVENT_WIDGET (event));

  g_object_set (event, "color", color, NULL);
}

GdkRGBA*
gcal_event_widget_get_color (GcalEventWidget *event)
{
  GdkRGBA *color;
  g_return_val_if_fail (GCAL_IS_EVENT_WIDGET (event), NULL);

  color = NULL;
  g_object_get (event, "color", color, NULL);
  return color;
}

void
gcal_event_widget_set_all_day (GcalEventWidget *event,
                               gboolean         all_day)
{
  g_return_if_fail (GCAL_IS_EVENT_WIDGET (event));

  g_object_set (event, "all-day", all_day, NULL);
}

gboolean
gcal_event_widget_get_all_day (GcalEventWidget *event)
{
  gboolean all_day;
  g_return_val_if_fail (GCAL_IS_EVENT_WIDGET (event), FALSE);

  g_object_get (event, "all-day", &all_day, NULL);
  return all_day;
}

gboolean
gcal_event_widget_is_multiday (GcalEventWidget *event)
{
  GcalEventWidgetPrivate *priv;

  gint start_day_of_year, end_day_of_year;
  priv = gcal_event_widget_get_instance_private (event);

  if (priv->dt_end == NULL)
    return FALSE;

  start_day_of_year = icaltime_day_of_year (*(priv->dt_start));
  end_day_of_year = icaltime_day_of_year (*(priv->dt_end));

  if (priv->all_day && start_day_of_year + 1 == end_day_of_year)
    return FALSE;

  if (priv->all_day && start_day_of_year == icaltime_days_in_year (priv->dt_start->year) && end_day_of_year == 1 &&
      priv->dt_start->year + 1 == priv->dt_end->year)
    return FALSE;

  return start_day_of_year != end_day_of_year;
}

void
gcal_event_widget_set_has_reminders (GcalEventWidget *event,
                                     gboolean         has_reminders)
{
  g_return_if_fail (GCAL_IS_EVENT_WIDGET (event));

  g_object_set (event, "has-reminders", has_reminders, NULL);
}

gboolean
gcal_event_widget_get_has_reminders (GcalEventWidget *event)
{
  gboolean has_reminders;
  g_return_val_if_fail (GCAL_IS_EVENT_WIDGET (event), FALSE);

  g_object_get (event, "has-reminders", &has_reminders, NULL);
  return has_reminders;
}

/**
 * gcal_event_widget_get_data:
 * @event: a #GcalEventWidget instance
 *
 * Returns a #GcalEventData with shallows members, meaning the members
 * are owned but the struct should be freed.
 *
 * Returns: (transfer full): a #GcalEventData
 **/
GcalEventData*
gcal_event_widget_get_data (GcalEventWidget *event)
{
  GcalEventWidgetPrivate *priv;
  GcalEventData *data;

  g_return_val_if_fail (GCAL_IS_EVENT_WIDGET (event), NULL);
  priv = gcal_event_widget_get_instance_private (event);

  data = g_new0 (GcalEventData, 1);
  data->source = priv->source;
  data->event_component = priv->component;

  return data;
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
  GcalEventWidgetPrivate *priv1;
  GcalEventWidgetPrivate *priv2;

  ECalComponentId *id1;
  ECalComponentId *id2;

  gboolean same_id = FALSE;

  priv1 = gcal_event_widget_get_instance_private (widget1);
  priv2 = gcal_event_widget_get_instance_private (widget2);

  if (!e_source_equal (priv1->source, priv2->source))
    return FALSE;

  id1 = e_cal_component_get_id (priv1->component);
  id2 = e_cal_component_get_id (priv2->component);
  same_id = e_cal_component_id_equal (id1, id2);

  e_cal_component_free_id (id1);
  e_cal_component_free_id (id2);

  return same_id;
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
  GcalEventWidgetPrivate *priv1;
  GcalEventWidgetPrivate *priv2;

  time_t time_s1, time_s2;
  time_t time_e1, time_e2;

  priv1 = gcal_event_widget_get_instance_private (widget1);
  priv2 = gcal_event_widget_get_instance_private (widget2);

  time_e1 = time_s1 = icaltime_as_timet (*(priv1->dt_start));
  time_e2 = time_s2 = icaltime_as_timet (*(priv2->dt_start));

  if (priv1->dt_end != NULL)
    time_e1 = icaltime_as_timet (*(priv1->dt_end));
  if (priv2->dt_end)
    time_e2 = icaltime_as_timet (*(priv2->dt_end));

  return (time_e2 - time_s2) - (time_e1 - time_s1);
}

gint
gcal_event_widget_compare_by_start_date (GcalEventWidget *widget1,
                                         GcalEventWidget *widget2)
{
  GcalEventWidgetPrivate *priv1;
  GcalEventWidgetPrivate *priv2;

  priv1 = gcal_event_widget_get_instance_private (widget1);
  priv2 = gcal_event_widget_get_instance_private (widget2);

  return icaltime_compare (*(priv1->dt_start), *(priv2->dt_start));
}

/**
 * gcal_event_widget_compare_for_single_day:
 * @widget1:
 * @widget2:
 *
 * Compare widgets by putting those that span over a day before the rest, and between those
 * who last less than a day by its start time/date
 *
 * Returns:
 **/
gint
gcal_event_widget_compare_for_single_day (GcalEventWidget *widget1,
                                          GcalEventWidget *widget2)
{
  GcalEventWidgetPrivate *priv1;
  GcalEventWidgetPrivate *priv2;

  priv1 = gcal_event_widget_get_instance_private (widget1);
  priv2 = gcal_event_widget_get_instance_private (widget2);

  if (gcal_event_widget_is_multiday (widget1) && gcal_event_widget_is_multiday (widget2))
    {
      time_t time_s1, time_s2;
      time_t time_e1, time_e2;
      time_t result;

      time_s1 = icaltime_as_timet (*(priv1->dt_start));
      time_s2 = icaltime_as_timet (*(priv2->dt_start));
      time_e1 = icaltime_as_timet (*(priv1->dt_end));
      time_e2 = icaltime_as_timet (*(priv2->dt_end));

      result = (time_e2 - time_s2) - (time_e1 - time_s1);
      if (result != 0)
        return result;
      else
        return icaltime_compare (*(priv1->dt_start), *(priv2->dt_start));
    }
  else
    {
      if (gcal_event_widget_is_multiday (widget1))
        return -1;
      else if (gcal_event_widget_is_multiday (widget2))
        return 1;
      else
        {
          if (priv1->all_day && priv2->all_day)
            return 0;
          else if (priv1->all_day)
            return -1;
          else if (priv2->all_day)
            return 1;
          else
            return icaltime_compare (*(priv1->dt_start), *(priv2->dt_start));
        }
    }
}
