/* gcal-drop-overlay.c
 *
 * Copyright 2024 Diego Iván <diegoivan.mae@gmail.com>
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// A drop overlay widget, based on Loupe's LpDragOverlay
// https://gitlab.gnome.org/GNOME/loupe/-/blob/692997ff0ee10ea187469c9da328010e1733dd6f/src/widgets/drag_overlay.rs

#define G_LOG_DOMAIN "GcalDropOverlay"

#include "gcal-debug.h"
#include "gcal-drop-overlay.h"

struct _GcalDropOverlay
{
  AdwBin              parent;

  GtkOverlay         *overlay;
  GtkRevealer        *revealer;
  GtkDropTarget      *drop_target;

  guint              hide_timeout_id;
};

static void on_current_drop_notify_cb (GcalDropOverlay *self);

G_DEFINE_TYPE (GcalDropOverlay, gcal_drop_overlay, ADW_TYPE_BIN)

enum
{
  PROP_0,
  PROP_CHILD,
  PROP_OVERLAYED,
  N_PROPS
};

static GParamSpec *properties[N_PROPS] = { NULL, };

static gboolean
hide_internal_overlay_cb (gpointer data)
{
  GcalDropOverlay *self = (GcalDropOverlay *) data;

  g_assert (GCAL_IS_DROP_OVERLAY (self));

  gtk_widget_set_visible (GTK_WIDGET (self->revealer), FALSE);
  self->hide_timeout_id = 0;

  return G_SOURCE_REMOVE;
}

static void
on_current_drop_notify_cb (GcalDropOverlay *self)
{
  GdkDrop *current_drop;

  GCAL_ENTRY;

  current_drop = gtk_drop_target_get_current_drop (self->drop_target);

  g_clear_handle_id (&self->hide_timeout_id, g_source_remove);

  if (current_drop != NULL)
    {
      gtk_widget_set_visible (GTK_WIDGET (self->revealer), TRUE);
      gtk_revealer_set_reveal_child (self->revealer, TRUE);
      gtk_widget_add_css_class (gtk_overlay_get_child (self->overlay), "blurred");
    }
  else
    {
      /* Hide the internal overlay only when the hide animations has been completed */
      self->hide_timeout_id = g_timeout_add (gtk_revealer_get_transition_duration (self->revealer),
                                             hide_internal_overlay_cb,
                                             self);

      gtk_revealer_set_reveal_child (self->revealer, FALSE);
      gtk_widget_remove_css_class (gtk_overlay_get_child (self->overlay), "blurred");
    }

  gtk_revealer_set_reveal_child (self->revealer, current_drop != NULL);

  GCAL_EXIT;
}

static void
gcal_drop_overlay_dispose (GObject *object)
{
  GcalDropOverlay *self = GCAL_DROP_OVERLAY (object);

  g_clear_object (&self->drop_target);
  g_clear_handle_id (&self->hide_timeout_id, g_source_remove);

  G_OBJECT_CLASS (gcal_drop_overlay_parent_class)->dispose (object);
}

static void
gcal_drop_overlay_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GcalDropOverlay *self = GCAL_DROP_OVERLAY (object);

  switch (prop_id)
  {
    case PROP_CHILD:
      g_value_set_object (value, gtk_overlay_get_child (self->overlay));
      break;

    case PROP_OVERLAYED:
      g_value_set_object (value, gtk_revealer_get_child (self->revealer));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
gcal_drop_overlay_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GcalDropOverlay *self = GCAL_DROP_OVERLAY (object);

  switch (prop_id)
    {
    case PROP_CHILD:
      gtk_overlay_set_child (self->overlay, g_value_get_object (value));
      break;

    case PROP_OVERLAYED:
      gtk_revealer_set_child (self->revealer, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_drop_overlay_class_init (GcalDropOverlayClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gcal_drop_overlay_dispose;
  object_class->get_property = gcal_drop_overlay_get_property;
  object_class->set_property = gcal_drop_overlay_set_property;

  properties[PROP_CHILD] = g_param_spec_object ("child", NULL, NULL,
                                                GTK_TYPE_WIDGET,
                                                G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_OVERLAYED] = g_param_spec_object ("overlayed", NULL, NULL,
                                                    GTK_TYPE_WIDGET,
                                                    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gcal_drop_overlay_init (GcalDropOverlay *self)
{
  self->overlay = GTK_OVERLAY (gtk_overlay_new ());
  self->revealer = GTK_REVEALER (gtk_revealer_new ());

  gtk_overlay_add_overlay (self->overlay, GTK_WIDGET (self->revealer));
  gtk_widget_set_visible (GTK_WIDGET (self->revealer), FALSE);
  gtk_widget_set_can_target (GTK_WIDGET (self->revealer), FALSE);
  gtk_revealer_set_transition_type (self->revealer, GTK_REVEALER_TRANSITION_TYPE_CROSSFADE);
  gtk_revealer_set_reveal_child (self->revealer, FALSE);

  adw_bin_set_child (ADW_BIN (self), GTK_WIDGET (self->overlay));
}

void
gcal_drop_overlay_set_drop_target (GcalDropOverlay *self,
                                   GtkDropTarget   *drop_target)
{
  GCAL_ENTRY;

  g_return_if_fail (GCAL_IS_DROP_OVERLAY (self));
  g_return_if_fail (GTK_IS_DROP_TARGET (drop_target));

  g_set_object (&self->drop_target, drop_target);

  g_signal_connect_object (self->drop_target,
                           "notify::current-drop",
                           G_CALLBACK (on_current_drop_notify_cb),
                           self,
                           G_CONNECT_SWAPPED);
  GCAL_EXIT;
}
