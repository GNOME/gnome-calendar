/*
 * Copyright (C) 2021 Purism SPC
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Author: Alice Mikhaylenko <alice.mikhaylenko@puri.sm>
 */

/* Copied from libadwaita 1.8 and adapted for use in gnome-calendar. */

#include "config.h"
#include "gcal-fading-label.h"

#include <glib/gi18n-lib.h>
#include "gcal-bidi.h"

#define FADE_WIDTH 18.0f

struct _GcalFadingLabel
{
  GtkWidget parent_instance;

  GtkWidget *label;
  gfloat align;
};

G_DEFINE_FINAL_TYPE (GcalFadingLabel, gcal_fading_label, GTK_TYPE_WIDGET)

enum {
  PROP_0,
  PROP_LABEL,
  PROP_ALIGN,
  LAST_PROP
};

static GParamSpec *props[LAST_PROP];

static gboolean
is_rtl (GcalFadingLabel *self)
{
  PangoDirection pango_direction = PANGO_DIRECTION_NEUTRAL;
  const gchar *label = gcal_fading_label_get_label (self);

  if (label)
    pango_direction = gcal_find_base_dir (label, -1);

  if (pango_direction == PANGO_DIRECTION_RTL)
    return TRUE;

  if (pango_direction == PANGO_DIRECTION_LTR)
    return FALSE;

  return gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL;
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

  gtk_widget_measure (self->label,
                      orientation,
                      for_size,
                      min, nat,
                      min_baseline, nat_baseline);

  if (orientation == GTK_ORIENTATION_HORIZONTAL && min)
    *min = 0;
}

static void
gcal_fading_label_size_allocate (GtkWidget *widget,
                                gint        width,
                                gint        height,
                                gint        baseline)
{
  GcalFadingLabel *self = GCAL_FADING_LABEL (widget);
  GskTransform *transform;
  gfloat align = is_rtl (self) ? 1 - self->align : self->align;
  gint child_width;
  gfloat offset;

  gtk_widget_measure (self->label,
                      GTK_ORIENTATION_HORIZONTAL,
                      height,
                      NULL,
                      &child_width,
                      NULL, NULL);

  offset = (width - child_width) * align;
  transform = gsk_transform_translate (NULL, &GRAPHENE_POINT_INIT (offset, 0));

  gtk_widget_allocate (self->label, child_width, height, baseline, transform);
}

static void
gcal_fading_label_snapshot (GtkWidget   *widget,
                            GtkSnapshot *snapshot)
{
  GcalFadingLabel *self = GCAL_FADING_LABEL (widget);
  g_autoptr (GskRenderNode) node = NULL;
  g_autoptr (GtkSnapshot) child_snapshot = NULL;
  graphene_rect_t bounds;
  gfloat align = is_rtl (self) ? 1 - self->align : self->align;
  gint width = gtk_widget_get_width (widget);
  gint clipped_size;

  if (width <= 0)
    return;

  clipped_size = gtk_widget_get_width (self->label) - width;

  if (clipped_size <= 0)
    {
      gtk_widget_snapshot_child (widget, self->label, snapshot);
      return;
    }

  child_snapshot = gtk_snapshot_new ();
  gtk_widget_snapshot_child (widget, self->label, child_snapshot);
  node = gtk_snapshot_free_to_node (g_steal_pointer (&child_snapshot));

  gsk_render_node_get_bounds (node, &bounds);
  bounds.origin.x = 0;
  bounds.origin.y = floor (bounds.origin.y);
  bounds.size.width = width;
  bounds.size.height = ceil (bounds.size.height) + 1;

  gtk_snapshot_push_mask (snapshot, GSK_MASK_MODE_INVERTED_ALPHA);

  if (align > 0)
    {
      gtk_snapshot_append_linear_gradient (snapshot,
                                           &GRAPHENE_RECT_INIT (0, bounds.origin.y,
                                                                FADE_WIDTH, bounds.size.height),
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
                                           &GRAPHENE_RECT_INIT (width - FADE_WIDTH, bounds.origin.y,
                                                                FADE_WIDTH, bounds.size.height),
                                           &GRAPHENE_POINT_INIT (width, 0),
                                           &GRAPHENE_POINT_INIT (width - FADE_WIDTH, 0),
                                           (GskColorStop[2]) {
                                               { 0, { 0, 0, 0, 1 } },
                                               { 1, { 0, 0, 0, 0 } },
                                           },
                                           2);
    }

  gtk_snapshot_pop (snapshot);

  gtk_snapshot_push_clip (snapshot, &bounds);
  gtk_snapshot_append_node (snapshot, node);
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
  case PROP_LABEL:
    g_value_set_string (value, gcal_fading_label_get_label (self));
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
  case PROP_LABEL:
    gcal_fading_label_set_label (self, g_value_get_string (value));
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

  g_clear_pointer (&self->label, gtk_widget_unparent);

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

  widget_class->measure = gcal_fading_label_measure;
  widget_class->size_allocate = gcal_fading_label_size_allocate;
  widget_class->snapshot = gcal_fading_label_snapshot;

  props[PROP_LABEL] =
    g_param_spec_string ("label", NULL, NULL,
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
  self->label = gtk_label_new (NULL);
  gtk_label_set_single_line_mode (GTK_LABEL (self->label), TRUE);

  gtk_widget_set_parent (self->label, GTK_WIDGET (self));
}

const gchar *
gcal_fading_label_get_label (GcalFadingLabel *self)
{
  g_return_val_if_fail (GCAL_IS_FADING_LABEL (self), NULL);

  return gtk_label_get_label (GTK_LABEL (self->label));
}

void
gcal_fading_label_set_label (GcalFadingLabel *self,
                             const gchar      *label)
{
  g_return_if_fail (GCAL_IS_FADING_LABEL (self));

  if (!g_strcmp0 (label, gcal_fading_label_get_label (self)))
    return;

  gtk_label_set_label (GTK_LABEL (self->label), label);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_LABEL]);
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

  gtk_widget_queue_allocate (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ALIGN]);
}
