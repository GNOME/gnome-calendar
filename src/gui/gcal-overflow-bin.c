/* gcal-overflow-bin-layout.c
 *
 * Copyright (C) 2022 Purism SPC
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "gcal-overflow-bin.h"
#include "gcal-overflow-bin-layout.h"

typedef struct
{
  GtkWidget *child;
  GtkSizeRequestMode request_mode;
} GcalOverflowBinPrivate;

static void gcal_overflow_bin_buildable_init (GtkBuildableIface *iface);

G_DEFINE_TYPE_WITH_CODE (GcalOverflowBin, gcal_overflow_bin, GTK_TYPE_WIDGET,
                         G_ADD_PRIVATE (GcalOverflowBin)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, gcal_overflow_bin_buildable_init))

static GtkBuildableIface *parent_buildable_iface;

enum
{
  PROP_0,
  PROP_CHILD,
  PROP_REQUEST_MODE,
  LAST_PROP
};

static GParamSpec *props[LAST_PROP];


/*
 * GtkBuildable interface
 */

static void
gcal_overflow_bin_buildable_add_child (GtkBuildable *buildable,
                                       GtkBuilder   *builder,
                                       GObject      *child,
                                       const char   *type)
{
  if (GTK_IS_WIDGET (child))
    gcal_overflow_bin_set_child (GCAL_OVERFLOW_BIN (buildable), GTK_WIDGET (child));
  else
    parent_buildable_iface->add_child (buildable, builder, child, type);
}

static void
gcal_overflow_bin_buildable_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);

  iface->add_child = gcal_overflow_bin_buildable_add_child;
}


/*
 * GtkWidget overrides
 */

static void
gcal_overflow_bin_compute_expand (GtkWidget *widget,
                                  gboolean  *hexpand_p,
                                  gboolean  *vexpand_p)
{
  GtkWidget *child;
  gboolean hexpand = FALSE;
  gboolean vexpand = FALSE;

  for (child = gtk_widget_get_first_child (widget);
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      hexpand = hexpand || gtk_widget_compute_expand (child, GTK_ORIENTATION_HORIZONTAL);
      vexpand = vexpand || gtk_widget_compute_expand (child, GTK_ORIENTATION_VERTICAL);
    }

  *hexpand_p = hexpand;
  *vexpand_p = vexpand;
}


/*
 * GObject overrides
 */

static void
gcal_overflow_bin_dispose (GObject *object)
{
  GcalOverflowBin *self = GCAL_OVERFLOW_BIN (object);
  GcalOverflowBinPrivate *priv = gcal_overflow_bin_get_instance_private (self);

  g_clear_pointer (&priv->child, gtk_widget_unparent);

  G_OBJECT_CLASS (gcal_overflow_bin_parent_class)->dispose (object);
}

static void
gcal_overflow_bin_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GcalOverflowBin *self = GCAL_OVERFLOW_BIN (object);

  switch (prop_id)
    {
    case PROP_CHILD:
      g_value_set_object (value, gcal_overflow_bin_get_child (self));
      break;

    case PROP_REQUEST_MODE:
      g_value_set_enum (value, gtk_widget_get_request_mode (GTK_WIDGET (self)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_overflow_bin_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GcalOverflowBin *self = GCAL_OVERFLOW_BIN (object);

  switch (prop_id)
    {
    case PROP_CHILD:
      gcal_overflow_bin_set_child (self, g_value_get_object (value));
      break;

    case PROP_REQUEST_MODE:
      gcal_overflow_bin_set_request_mode (self, g_value_get_enum (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_overflow_bin_class_init (GcalOverflowBinClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gcal_overflow_bin_dispose;
  object_class->get_property = gcal_overflow_bin_get_property;
  object_class->set_property = gcal_overflow_bin_set_property;

  widget_class->compute_expand = gcal_overflow_bin_compute_expand;

  /**
   * GcalOverflowBin:child: (attributes org.gtk.Property.get=gcal_overflow_bin_get_child org.gtk.Property.set=gcal_overflow_bin_set_child)
   *
   * The child widget of the `GcalOverflowBin`.
   */
  props[PROP_CHILD] = g_param_spec_object ("child", NULL, NULL,
                                           GTK_TYPE_WIDGET,
                                           G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GcalOverflowBin:child: (attributes org.gtk.Property.get=gtk_widget_get_request_mode org.gtk.Property.set=gcal_overflow_bin_set_request_mode)
   *
   * The size request mode of the `GcalOverflowBin`.
   */
  props[PROP_REQUEST_MODE] = g_param_spec_enum ("request-mode", NULL, NULL,
                                                GTK_TYPE_SIZE_REQUEST_MODE,
                                                GTK_SIZE_REQUEST_CONSTANT_SIZE,
                                                G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  gtk_widget_class_set_layout_manager_type (widget_class, GCAL_TYPE_OVERFLOW_BIN_LAYOUT);
}

static void
gcal_overflow_bin_init (GcalOverflowBin *self)
{
  GcalOverflowBinPrivate *priv = gcal_overflow_bin_get_instance_private (self);

  priv->request_mode = GTK_SIZE_REQUEST_CONSTANT_SIZE;
}

/**
 * gcal_overflow_bin_new:
 *
 * Creates a new `GcalOverflowBin`.
 *
 * Returns: the new created `GcalOverflowBin`
 */
GtkWidget *
gcal_overflow_bin_new (void)
{
  return g_object_new (GCAL_TYPE_OVERFLOW_BIN, NULL);
}

/**
 * gcal_overflow_bin_get_child: (attributes org.gtk.Method.get_property=child)
 * @self: a bin
 *
 * Gets the child widget of @self.
 *
 * Returns: (nullable) (transfer none): the child widget of @self
 */
GtkWidget *
gcal_overflow_bin_get_child (GcalOverflowBin *self)
{
  GcalOverflowBinPrivate *priv;

  g_return_val_if_fail (GCAL_IS_OVERFLOW_BIN (self), NULL);

  priv = gcal_overflow_bin_get_instance_private (self);

  return priv->child;
}

/**
 * gcal_overflow_bin_set_child: (attributes org.gtk.Method.set_property=child)
 * @self: a bin
 * @child: (nullable): the child widget
 *
 * Sets the child widget of @self.
 */
void
gcal_overflow_bin_set_child (GcalOverflowBin *self,
                             GtkWidget       *child)
{
  GcalOverflowBinPrivate *priv;

  g_return_if_fail (GCAL_IS_OVERFLOW_BIN (self));
  g_return_if_fail (child == NULL || GTK_IS_WIDGET (child));

  priv = gcal_overflow_bin_get_instance_private (self);

  if (priv->child == child)
    return;

  g_clear_pointer (&priv->child, gtk_widget_unparent);

  priv->child = child;

  if (priv->child)
    gtk_widget_set_parent (priv->child, GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_CHILD]);
}

/**
 * gcal_overflow_bin_set_request_mode: (attributes org.gtk.Method.set_property=request-mode)
 * @self: a bin
 * @request_ode: the size request mode
 *
 * Sets the size request mode of @self.
 */
void
gcal_overflow_bin_set_request_mode (GcalOverflowBin    *self,
                                    GtkSizeRequestMode  request_mode)
{
  GcalOverflowBinPrivate *priv;
  GtkLayoutManager *layout_manager;

  g_return_if_fail (GCAL_IS_OVERFLOW_BIN (self));
  g_return_if_fail (request_mode >= GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH && request_mode <= GTK_SIZE_REQUEST_CONSTANT_SIZE);

  priv = gcal_overflow_bin_get_instance_private (self);

  if (priv->request_mode == request_mode)
    return;

  priv->request_mode = request_mode;

  layout_manager = gtk_widget_get_layout_manager (GTK_WIDGET (self));
  gcal_overflow_bin_layout_set_request_mode (GCAL_OVERFLOW_BIN_LAYOUT (layout_manager), request_mode);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_REQUEST_MODE]);
}
