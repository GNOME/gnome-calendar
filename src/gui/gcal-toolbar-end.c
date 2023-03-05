/* gcal-toolbar-end.c
 *
 * Copyright 2022 Christopher Davis <christopherdavis@gnome.org>
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

#include "gcal-context.h"
#include "gcal-search-button.h"
#include "gcal-toolbar-end.h"

struct _GcalToolbarEnd
{
  AdwBin               parent_instance;

  GcalSearchButton    *search_button;

  GcalContext         *context;
};

G_DEFINE_FINAL_TYPE (GcalToolbarEnd, gcal_toolbar_end, ADW_TYPE_BIN)

enum
{
  PROP_0,
  PROP_CONTEXT,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];


/*
 * GObject overrides
 */

static void
gcal_toolbar_end_finalize (GObject *object)
{
  GcalToolbarEnd *self = (GcalToolbarEnd *)object;

  g_clear_object (&self->context);

  G_OBJECT_CLASS (gcal_toolbar_end_parent_class)->finalize (object);
}

static void
gcal_toolbar_end_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GcalToolbarEnd *self = GCAL_TOOLBAR_END (object);

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
gcal_toolbar_end_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GcalToolbarEnd *self = GCAL_TOOLBAR_END (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_assert (self->context == NULL);
      self->context = g_value_dup_object (value);
      g_object_bind_property (self, "context", self->search_button, "context", G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_toolbar_end_class_init (GcalToolbarEndClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gcal_toolbar_end_finalize;
  object_class->get_property = gcal_toolbar_end_get_property;
  object_class->set_property = gcal_toolbar_end_set_property;

  /**
   * GcalToolbarEnd::context:
   *
   * The #GcalContext of the application.
   */
  properties[PROP_CONTEXT] = g_param_spec_object ("context",
                                                  "Context of the application",
                                                  "The context of the application",
                                                  GCAL_TYPE_CONTEXT,
                                                  G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/gui/gcal-toolbar-end.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalToolbarEnd, search_button);
}

static void
gcal_toolbar_end_init (GcalToolbarEnd *self)
{
  g_type_ensure (GCAL_TYPE_SEARCH_BUTTON);

  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
gcal_toolbar_end_get_search_button (GcalToolbarEnd *self)
{
  g_return_val_if_fail (GCAL_IS_TOOLBAR_END (self), NULL);

  return GTK_WIDGET (self->search_button);
}
