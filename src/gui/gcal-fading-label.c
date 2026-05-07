/*
 * Copyright (C) 2021 Purism SPC
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Author: Alice Mikhaylenko <alice.mikhaylenko@puri.sm>
 */

/* Copied from libadwaita 1.8 and adapted for use in gnome-calendar. */

#define G_LOG_DOMAIN "GcalFadingLabel"

#include "config.h"
#include "gcal-fading-label.h"

#include <glib/gi18n-lib.h>
#include "gcal-bidi.h"

#define FADE_WIDTH 18.0f

struct _GcalFadingLabel
{
  GtkWidget parent_instance;

  PangoLayout *layout;

  GskRenderNode *cached_text_node;
  PangoRectangle cached_extents;
  gboolean cached_extents_valid;
  int cached_ascent;
  int cached_descent;

  char *text;

  gfloat align;
};

G_DEFINE_FINAL_TYPE (GcalFadingLabel, gcal_fading_label, GTK_TYPE_WIDGET)

enum {
  PROP_0,
  PROP_TEXT,
  PROP_ALIGN,
  LAST_PROP
};

static GParamSpec *props[LAST_PROP];

static gboolean
is_rtl (GcalFadingLabel *self)
{
  PangoDirection pango_direction = PANGO_DIRECTION_NEUTRAL;

  if (self->text)
    pango_direction = gcal_find_base_dir (self->text, -1);

  if (pango_direction == PANGO_DIRECTION_RTL)
    return TRUE;

  if (pango_direction == PANGO_DIRECTION_LTR)
    return FALSE;

  return gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL;
}

static int
get_line_pixels (GcalFadingLabel *self,
                 int            *baseline)
{
  if (self->cached_ascent == -1 || self->cached_descent == -1)
    {
      PangoFontMetrics *metrics;
      PangoContext *context;

      context = gtk_widget_get_pango_context (GTK_WIDGET (self));
      metrics = pango_context_get_metrics (context, NULL, NULL);

      self->cached_ascent = pango_font_metrics_get_ascent (metrics);
      self->cached_descent = pango_font_metrics_get_descent (metrics);

      g_clear_pointer (&metrics, pango_font_metrics_unref);
    }

  if (baseline)
    *baseline = self->cached_ascent;

  return self->cached_ascent + self->cached_descent;
}

static void
gcal_fading_label_css_changed (GtkWidget         *widget,
                               GtkCssStyleChange *change)
{
  GcalFadingLabel *self = GCAL_FADING_LABEL (widget);

  GTK_WIDGET_CLASS (gcal_fading_label_parent_class)->css_changed (widget, change);

  self->cached_ascent = -1;
  self->cached_descent = -1;
  self->cached_extents_valid = FALSE;
  g_clear_pointer (&self->cached_text_node, gsk_render_node_unref);

  gtk_widget_queue_draw (widget);
}

static void
gcal_fading_label_measure (GtkWidget      *widget,
                          GtkOrientation  orientation,
                          gint             for_size,
                          gint             *min,
                          gint             *nat,
                          gint             *min_baseline,
                          gint             *nat_baseline)
{
  GcalFadingLabel *self = GCAL_FADING_LABEL (widget);

  switch (orientation)
    {
    case GTK_ORIENTATION_VERTICAL:
      {
        int line_pixels, baseline;

        line_pixels = get_line_pixels (self, &baseline);

        *min = line_pixels;
        *nat = line_pixels;
      }
      break;

    case GTK_ORIENTATION_HORIZONTAL:
      *min = 0;
      *nat = 0;
      break;

    default:
      g_assert_not_reached ();
    }

  *min = PANGO_PIXELS_CEIL (*min);
  *nat = PANGO_PIXELS_CEIL (*nat);
  if (*min_baseline > 0)
    *min_baseline = PANGO_PIXELS_CEIL (*min_baseline);
  if (*nat_baseline > 0)
    *nat_baseline = PANGO_PIXELS_CEIL (*nat_baseline);
}

static void
gcal_fading_label_snapshot (GtkWidget   *widget,
                            GtkSnapshot *snapshot)
{
  GcalFadingLabel *self = GCAL_FADING_LABEL (widget);
  float align;
  int height;
  int width;

  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_height (widget);

  if (width <= 0)
    return;

  if (!self->cached_extents_valid)
    {
      pango_layout_get_pixel_extents (self->layout, NULL, &self->cached_extents);
      self->cached_extents_valid = TRUE;
    }

  if (!self->cached_text_node)
    {
      g_autoptr (GtkSnapshot) text_snapshot = NULL;
      GdkRGBA foreground_color;

      gtk_widget_get_color (widget, &foreground_color);

      text_snapshot = gtk_snapshot_new ();
      gtk_snapshot_append_layout (text_snapshot, self->layout, &foreground_color);

      self->cached_text_node = gtk_snapshot_free_to_node (g_steal_pointer (&text_snapshot));
    }

  if (self->cached_extents.x + self->cached_extents.width <= width)
    {
      gtk_snapshot_append_node (snapshot, self->cached_text_node);
      return;
    }

  align = is_rtl (self) ? 1 - self->align : self->align;

  gtk_snapshot_push_mask (snapshot, GSK_MASK_MODE_INVERTED_ALPHA);

  if (align > 0)
    {
      gtk_snapshot_append_linear_gradient (snapshot,
                                           &GRAPHENE_RECT_INIT (0, 0, FADE_WIDTH, height),
                                           &GRAPHENE_POINT_INIT (0, 0),
                                           &GRAPHENE_POINT_INIT (FADE_WIDTH, 0),
                                           (GskColorStop[2]) {
                                               { 0, { 0, 0, 0, 1 } },
                                               { 1, { 0, 0, 0, 0 } },
                                           },
                                           2);
    }

  if (align < 1)
    {
      gtk_snapshot_append_linear_gradient (snapshot,
                                           &GRAPHENE_RECT_INIT (width - FADE_WIDTH, 0, FADE_WIDTH, height),
                                           &GRAPHENE_POINT_INIT (width, 0),
                                           &GRAPHENE_POINT_INIT (width - FADE_WIDTH, 0),
                                           (GskColorStop[2]) {
                                               { 0, { 0, 0, 0, 1 } },
                                               { 1, { 0, 0, 0, 0 } },
                                           },
                                           2);
    }

  gtk_snapshot_pop (snapshot);

  gtk_snapshot_push_clip (snapshot, &GRAPHENE_RECT_INIT (0, 0, width, height));
  gtk_snapshot_append_node (snapshot, self->cached_text_node);
  gtk_snapshot_pop (snapshot);

  gtk_snapshot_pop (snapshot);
}

static void
gcal_fading_label_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GcalFadingLabel *self = GCAL_FADING_LABEL (object);

  switch (prop_id) {
  case PROP_TEXT:
    g_value_set_string (value, self->text);
    break;

  case PROP_ALIGN:
    g_value_set_float (value, gcal_fading_label_get_align (self));
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
gcal_fading_label_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GcalFadingLabel *self = GCAL_FADING_LABEL (object);

  switch (prop_id) {
  case PROP_TEXT:
    gcal_fading_label_set_text (self, g_value_get_string (value));
    break;

  case PROP_ALIGN:
    gcal_fading_label_set_align (self, g_value_get_float (value));
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
gcal_fading_label_dispose (GObject *object)
{
  GcalFadingLabel *self = GCAL_FADING_LABEL (object);

  g_clear_object (&self->layout);
  g_clear_pointer (&self->cached_text_node, gsk_render_node_unref);

  G_OBJECT_CLASS (gcal_fading_label_parent_class)->dispose (object);
}

static void
gcal_fading_label_class_init (GcalFadingLabelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = gcal_fading_label_get_property;
  object_class->set_property = gcal_fading_label_set_property;
  object_class->dispose = gcal_fading_label_dispose;

  widget_class->css_changed = gcal_fading_label_css_changed;
  widget_class->measure = gcal_fading_label_measure;
  widget_class->snapshot = gcal_fading_label_snapshot;

  props[PROP_TEXT] =
    g_param_spec_string ("text", NULL, NULL,
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_ALIGN] =
    g_param_spec_float ("align", NULL, NULL,
                        0.0, 1.0, 0.0,
                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static void
gcal_fading_label_init (GcalFadingLabel *self)
{
  self->layout = gtk_widget_create_pango_layout (GTK_WIDGET (self), NULL);
  self->cached_ascent = -1;
  self->cached_descent = -1;
  self->cached_extents_valid = FALSE;
}

const gchar *
gcal_fading_label_get_text (GcalFadingLabel *self)
{
  g_return_val_if_fail (GCAL_IS_FADING_LABEL (self), NULL);

  return self->text;
}

void
gcal_fading_label_set_text (GcalFadingLabel *self,
                            const gchar     *text)
{
  g_return_if_fail (GCAL_IS_FADING_LABEL (self));

  if (!g_set_str (&self->text, text))
    return;

  pango_layout_set_text (self->layout,
                         self->text ? self->text : "",
                         -1);

  self->cached_ascent = -1;
  self->cached_descent = -1;
  self->cached_extents_valid = FALSE;
  g_clear_pointer (&self->cached_text_node, gsk_render_node_unref);

  gtk_widget_queue_draw (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_TEXT]);
}

float
gcal_fading_label_get_align (GcalFadingLabel *self)
{
  g_return_val_if_fail (GCAL_IS_FADING_LABEL (self), 0.0f);

  return self->align;
}

void
gcal_fading_label_set_align (GcalFadingLabel *self,
                             gfloat           align)
{
  g_return_if_fail (GCAL_IS_FADING_LABEL (self));

  align = CLAMP (align, 0.0, 1.0);

  if (G_APPROX_VALUE (self->align, align, FLT_EPSILON))
    return;

  self->align = align;

  gtk_widget_queue_draw (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ALIGN]);
}
