/*
 * gcal-stack-label.c
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 The GNOME Calendar authors
 */

#include "config.h"

#include "gcal-stack-label.h"


/**
 * GcalStackLabel:
 *
 * A wrapper for [class@Gtk.Label] which displays a string with a fixed width based
 * on the widest string from an array of strings.
 *
 * The width is set after invoking [method@Gcal.StackLabel.compute_labels].
 */

struct _GcalStackLabel
{
  GtkWidget  parent_instance;

  GtkWidget *label;

  int        width;
};

G_DEFINE_FINAL_TYPE_WITH_CODE (GcalStackLabel, gcal_stack_label, GTK_TYPE_WIDGET,
                               G_IMPLEMENT_INTERFACE (GTK_TYPE_ACCESSIBLE_TEXT, NULL))

enum {
  PROP_0,
  PROP_LABEL,
  N_PROPS,
};

static GParamSpec *properties [N_PROPS];


/*
 * GObject overrides
 */

static void
gcal_stack_label_finalize (GObject *object)
{
  GcalStackLabel *self = GCAL_STACK_LABEL (object);

  g_clear_pointer (&self->label, gtk_widget_unparent);

  G_OBJECT_CLASS (gcal_stack_label_parent_class)->finalize (object);
}

static void
gcal_stack_label_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GcalStackLabel *self = GCAL_STACK_LABEL (object);

  switch (prop_id)
    {
    case PROP_LABEL:
      g_value_set_string (value, gcal_stack_label_get_label (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_stack_label_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GcalStackLabel *self = GCAL_STACK_LABEL (object);

  switch (prop_id)
    {
    case PROP_LABEL:
      gcal_stack_label_set_label (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}


/*
 * GtkWidget overrides
 */

static void
gcal_stack_label_measure (GtkWidget      *widget,
                          GtkOrientation  orientation,
                          int             for_size,
                          int            *minimum,
                          int            *natural,
                          int            *minimum_baseline,
                          int            *natural_baseline)
{
  GcalStackLabel *self = GCAL_STACK_LABEL (widget);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      *natural = self->width;
      *minimum = self->width;
    }
  else
    {
      gtk_widget_measure (self->label, orientation, for_size,
                          minimum, natural,
                          NULL, NULL);
    }
}

static void
gcal_stack_label_size_allocate (GtkWidget *widget,
                                int        width,
                                int        height,
                                int        baseline)
{
  GcalStackLabel *self = GCAL_STACK_LABEL (widget);
  int label_natural_width;

  gtk_widget_measure (self->label, GTK_ORIENTATION_HORIZONTAL, height,
                      NULL, &label_natural_width,
                      NULL, NULL);

  g_assert_cmpint (self->width, >=, label_natural_width);

  gtk_widget_allocate (self->label, self->width, height, baseline, NULL);
}


/*
 * Initialization
 */

static void
gcal_stack_label_class_init (GcalStackLabelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gcal_stack_label_finalize;
  object_class->get_property = gcal_stack_label_get_property;
  object_class->set_property = gcal_stack_label_set_property;

  widget_class->measure = gcal_stack_label_measure;
  widget_class->size_allocate = gcal_stack_label_size_allocate;

  /**
   * GcalStackLabel:label:
   *
   * The contents of the label.
   *
   * See [property@Gtk.Label:label].
   */
  properties[PROP_LABEL] =
      g_param_spec_string ("label", NULL, NULL,
                           NULL,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_LABEL);
}

static void
gcal_stack_label_init (GcalStackLabel *self)
{
  self->label = gtk_label_new (NULL);
  gtk_widget_set_parent (self->label, GTK_WIDGET (self));

  gtk_accessible_update_relation (GTK_ACCESSIBLE (self),
                                  GTK_ACCESSIBLE_RELATION_LABELLED_BY, self->label,
                                  NULL, -1);
}


/*
 * Public methods
 */

/**
 * gcal_stack_label_get_label:
 * @self: a #GcalStackLabel
 *
 * Gets the label.
 *
 * Returns: (transfer none): the label
 */
const char *
gcal_stack_label_get_label (GcalStackLabel *self)
{
  g_assert (GCAL_IS_STACK_LABEL (self));

  return gtk_label_get_label (GTK_LABEL (self->label));
}

/**
 * gcal_stack_label_set_label:
 * @self: a #GcalStackLabel
 * @label: (transfer none): the label to set
 *
 * Sets the label.
 */
void
gcal_stack_label_set_label (GcalStackLabel *self,
                            const char     *label)
{
  g_assert (GCAL_IS_STACK_LABEL (self));

  if (g_strcmp0 (gcal_stack_label_get_label (self), label) == 0)
    return;

  gtk_label_set_label (GTK_LABEL (self->label), label);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LABEL]);
}

/**
 * gcal_stack_label_compute_labels:
 * @self: a #GcalStackLabel
 * @labels: (transfer none): an array of labels
 *
 * Computes and sets the widest label provided by @labels as the width of
 * the widget to guarantee a fixed width when cycling through them.
 */
void
gcal_stack_label_compute_labels (GcalStackLabel *self,
                                 GtkStringList  *labels)
{
  int max_width = 0;
  unsigned int n_items;

  g_assert (GCAL_IS_STACK_LABEL (self));
  g_assert (GTK_IS_STRING_LIST (labels));

  n_items = g_list_model_get_n_items (G_LIST_MODEL (labels));

  for (int i = 0; i < n_items; i++)
    {
      g_autoptr (PangoLayout) layout = NULL;
      const char *label = gtk_string_list_get_string (labels, i);
      int width;

      layout = gtk_widget_create_pango_layout (self->label, label);

      pango_layout_get_pixel_size (layout, &width, NULL);
      max_width = MAX (max_width, width);
    }

  self->width = max_width;

  gtk_widget_queue_resize (GTK_WIDGET (self));
}

