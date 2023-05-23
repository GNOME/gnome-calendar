/*
 * gcal-sync-indicator.c
 *
 * Copyright 2023 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#define G_LOG_DOMAIN "GcalSyncIndicator"

#include "gcal-context.h"
#include "gcal-sync-indicator.h"

struct _GcalSyncIndicator
{
  AdwBin              parent_instance;

  GtkSpinner         *refreshing_spinner;
  GtkStack           *stack;

  guint               icon_changed_source_id;

  GcalContext        *context;
};

static gboolean      icon_change_timeout_cb                      (gpointer           data);

G_DEFINE_FINAL_TYPE (GcalSyncIndicator, gcal_sync_indicator, ADW_TYPE_BIN)

enum
{
  PROP_0,
  PROP_CONTEXT,
  N_PROPS,
};

static GParamSpec *properties [N_PROPS] = { NULL, };


/*
 * Auxiliary methods
 */

static void
schedule_switch_to_spinner (GcalSyncIndicator *self)
{
  if (self->icon_changed_source_id > 0)
    return;

  g_debug ("Scheduling synchronization icon update");

  self->icon_changed_source_id = g_timeout_add (500, icon_change_timeout_cb, self);
}

static void
schedule_switch_to_success (GcalSyncIndicator *self)
{
  g_clear_handle_id (&self->icon_changed_source_id, g_source_remove);

  g_debug ("Switching to success icon");

  gtk_stack_set_visible_child_name (self->stack, "success");

  self->icon_changed_source_id = g_timeout_add (2000, icon_change_timeout_cb, self);
}


/*
 * Callbacks
 */

static gboolean
icon_change_timeout_cb (gpointer data)
{
  GcalSyncIndicator *self;
  GcalManager *manager;
  gboolean synchronizing;

  self = GCAL_SYNC_INDICATOR (data);
  manager = gcal_context_get_manager (self->context);
  synchronizing = gcal_manager_get_synchronizing (manager);

  g_debug ("Updating calendar icon to spinner");

  gtk_stack_set_visible_child_name (self->stack, synchronizing ? "spinner" : "empty");
  gtk_spinner_set_spinning (self->refreshing_spinner, synchronizing);

  self->icon_changed_source_id = 0;
  return G_SOURCE_REMOVE;
}

static void
on_manager_synchronizing_changed_cb (GcalManager       *manager,
                                     GParamSpec        *pspec,
                                     GcalSyncIndicator *self)
{
  if (gcal_manager_get_synchronizing (manager))
    schedule_switch_to_spinner (self);
  else
    schedule_switch_to_success (self);
}


/*
 * GObject overrides
 */

static void
gcal_sync_indicator_finalize (GObject *object)
{
  GcalSyncIndicator *self = (GcalSyncIndicator *)object;

  g_clear_handle_id (&self->icon_changed_source_id, g_source_remove);
  g_clear_object (&self->context);

  G_OBJECT_CLASS (gcal_sync_indicator_parent_class)->finalize (object);
}

static void
gcal_sync_indicator_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GcalSyncIndicator *self = GCAL_SYNC_INDICATOR (object);

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
gcal_sync_indicator_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GcalSyncIndicator *self = GCAL_SYNC_INDICATOR (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_assert (self->context == NULL);
      self->context = g_value_dup_object (value);

      g_signal_connect_object (gcal_context_get_manager (self->context),
                               "notify::synchronizing",
                               G_CALLBACK (on_manager_synchronizing_changed_cb),
                               object,
                               0);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_sync_indicator_class_init (GcalSyncIndicatorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gcal_sync_indicator_finalize;
  object_class->get_property = gcal_sync_indicator_get_property;
  object_class->set_property = gcal_sync_indicator_set_property;

  /**
   * GcalSyncIndicator::context:
   *
   * The #GcalContext of the application.
   */
  properties[PROP_CONTEXT] = g_param_spec_object ("context",
                                                  "Context of the application",
                                                  "The context of the application",
                                                  GCAL_TYPE_CONTEXT,
                                                  G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/gui/gcal-sync-indicator.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalSyncIndicator, refreshing_spinner);
  gtk_widget_class_bind_template_child (widget_class, GcalSyncIndicator, stack);

  gtk_widget_class_set_css_name (widget_class, "syncindicator");
}

static void
gcal_sync_indicator_init (GcalSyncIndicator *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
