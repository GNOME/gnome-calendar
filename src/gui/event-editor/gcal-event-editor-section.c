/* gcal-event-editor-section.c
 *
 * Copyright 2020 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#include "gcal-event-editor-dialog.h"
#include "gcal-event-editor-section.h"

/**
 * GcalEventEditorSection:
 *
 * An abstract class representing a section for an event editor dialog.
 *
 * Subclasses must override [vfunc@EventEditorSection.event_set_cb] to update widgets, and
 * should call [method@EventEditorSection.get_event] to propagate changes.
 */

typedef struct
{
  AdwPreferencesGroup  parent_instance;

  GtkWidget           *dialog;
} GcalEventEditorSectionPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GcalEventEditorSection, gcal_event_editor_section, ADW_TYPE_PREFERENCES_GROUP)

enum {
  PROP_0,
  PROP_DIALOG,
  PROP_EVENT,
  N_PROPS,
};

static GParamSpec *properties[N_PROPS];


/*
 * Callbacks
 */

static void
on_event_set_cb (GcalEventEditorSection *self)
{
  GcalEvent *event;

  event = gcal_event_editor_section_get_event (self);

  GCAL_EVENT_EDITOR_SECTION_GET_CLASS (self)->event_set_cb (self, event);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_EVENT]);
}


/*
 * GtkWidget overrides
 */

static void
gcal_event_editor_section_root (GtkWidget *widget)
{
  GcalEventEditorSection *self = GCAL_EVENT_EDITOR_SECTION (widget);
  GcalEventEditorSectionPrivate *priv = gcal_event_editor_section_get_instance_private (self);
  GtkWidget *dialog;

  dialog = gtk_widget_get_ancestor (widget, GCAL_TYPE_EVENT_EDITOR_DIALOG);

  g_assert (GCAL_IS_EVENT_EDITOR_DIALOG (dialog));

  g_set_object (&priv->dialog, dialog);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DIALOG]);

  GTK_WIDGET_CLASS (gcal_event_editor_section_parent_class)->root (widget);
}

static void
gcal_event_editor_section_map (GtkWidget *widget)
{
  GcalEventEditorSection *self = GCAL_EVENT_EDITOR_SECTION (widget);
  GcalEvent *event;

  event = gcal_event_editor_section_get_event (self);

  GCAL_EVENT_EDITOR_SECTION_GET_CLASS (self)->event_set (self, event);

  GTK_WIDGET_CLASS (gcal_event_editor_section_parent_class)->map (widget);
}

static void
gcal_event_editor_section_unmap (GtkWidget *widget)
{
  GcalEventEditorSection *self = GCAL_EVENT_EDITOR_SECTION (widget);

  GCAL_EVENT_EDITOR_SECTION_GET_CLASS (self)->event_set_cb (self, NULL);

  GTK_WIDGET_CLASS (gcal_event_editor_section_parent_class)->unmap (widget);
}


/*
 * GObject overrides
 */

static void
gcal_event_editor_section_constructed (GObject *object)
{
  g_assert (GCAL_EVENT_EDITOR_SECTION_GET_CLASS (object)->event_set_cb != NULL);

  G_OBJECT_CLASS (gcal_event_editor_section_parent_class)->constructed (object);
}

static void
gcal_event_editor_section_dispose (GObject *object)
{
  GcalEventEditorSection *self = GCAL_EVENT_EDITOR_SECTION (object);

  gcal_event_editor_section_set_dialog (self, NULL);

  G_OBJECT_CLASS (gcal_event_editor_section_parent_class)->dispose (object);
}

static void
gcal_event_editor_section_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  GcalEventEditorSection *self = GCAL_EVENT_EDITOR_SECTION (object);

  switch (prop_id)
    {
    case PROP_DIALOG:
      g_value_set_object (value, gcal_event_editor_section_get_dialog (self));
      break;
    case PROP_EVENT:
      g_value_set_object (value, gcal_event_editor_section_get_event (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_event_editor_section_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  GcalEventEditorSection *self = GCAL_EVENT_EDITOR_SECTION (object);

  switch (prop_id)
    {
    case PROP_DIALOG:
      gcal_event_editor_section_set_dialog (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}


/*
 * Initialization
 */

static void
gcal_event_editor_section_class_init (GcalEventEditorSectionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = gcal_event_editor_section_constructed;
  object_class->dispose = gcal_event_editor_section_dispose;
  object_class->get_property = gcal_event_editor_section_get_property;
  object_class->set_property = gcal_event_editor_section_set_property;

  widget_class->root = gcal_event_editor_section_root;
  widget_class->map = gcal_event_editor_section_map;
  widget_class->unmap = gcal_event_editor_section_unmap;

  klass->event_set_cb = NULL;

  /**
   * GcalEventEditorSection:event:
   *
   * The #GcalEvent being edited.
   */
  properties[PROP_EVENT] =
      g_param_spec_object ("event", NULL, NULL,
                           GCAL_TYPE_EVENT,
                           G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GcalEventEditorSection:dialog:
   *
   * The event editor dialog.
   *
   * The dialog must have the `event` property.
   * Each time `event` is set, [vfunc@EventEditorSection.event_set] is invoked.
   */
  properties[PROP_DIALOG] =
      g_param_spec_object ("dialog", NULL, NULL,
                           GTK_TYPE_WIDGET,
                           G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gcal_event_editor_section_init (GcalEventEditorSection *self)
{
}


/*
 * Public methods
 */

/**
 * gcal_event_editor_section_get_event:
 * @self: a #GcalEventEditorSection
 *
 * Gets the event that is currently being edited.
 *
 * Returns: (nullable) (transfer none): a #GcalEvent
 */
GcalEvent *
gcal_event_editor_section_get_event (GcalEventEditorSection *self)
{
  GcalEventEditorSectionPrivate *priv;
  GcalEvent *event;

  g_assert (GCAL_IS_EVENT_EDITOR_SECTION (self));

  priv = gcal_event_editor_section_get_instance_private (self);

  g_object_get (priv->dialog,
                "event", &event,
                NULL);

  return event;
}

/**
 * gcal_event_editor_section_get_dialog:
 * @self: a #GcalEventEditorSection
 *
 * Gets the dialog.
 *
 * Returns: (nullable) (transfer none): a dialog
 */
GtkWidget *
gcal_event_editor_section_get_dialog (GcalEventEditorSection *self)
{
  GcalEventEditorSectionPrivate *priv;

  g_assert (GCAL_IS_EVENT_EDITOR_SECTION (self));

  priv = gcal_event_editor_section_get_instance_private (self);

  return priv->dialog;
}

/**
 * gcal_event_editor_section_set_dialog:
 * @self: a #GcalEventEditorSection
 * @dialog: (nullable) (transfer none): a dialog
 *
 * Sets the dialog.
 */
void
gcal_event_editor_section_set_dialog (GcalEventEditorSection *self,
                                      GtkWidget              *dialog)
{
  GcalEventEditorSectionPrivate *priv;

  g_assert (GCAL_IS_EVENT_EDITOR_SECTION (self));
  g_assert (dialog == NULL || GTK_IS_WIDGET (dialog));

  priv = gcal_event_editor_section_get_instance_private (self);

  if (priv->dialog == dialog)
    return;

  if (priv->dialog)
    {
      g_signal_handlers_disconnect_by_func (priv->dialog, on_event_set_cb, self);
      g_object_unref (priv->dialog);
    }

  priv->dialog = dialog;

  if (priv->dialog)
    {
      g_signal_connect_object (priv->dialog, "notify::event", G_CALLBACK (on_event_set_cb), self, G_CONNECT_SWAPPED);
      g_object_ref (priv->dialog);
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DIALOG]);
}
