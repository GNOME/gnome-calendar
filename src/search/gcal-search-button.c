/* gcal-search-button.c
 *
 * Copyright 2019 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#define G_LOG_DOMAIN "GcalSearchButton"

#include "gcal-context.h"
#include "gcal-search-button.h"
#include "gcal-search-hit.h"

#include <math.h>

#define MIN_WIDTH 450

struct _GcalSearchButton
{
  DzlSuggestionButton  parent;

  GCancellable        *cancellable;

  GcalContext         *context;
};

G_DEFINE_TYPE (GcalSearchButton, gcal_search_button, DZL_TYPE_SUGGESTION_BUTTON)

enum
{
  PROP_0,
  PROP_CONTEXT,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];


/*
 * Callbacks
 */

static void
position_suggestion_popover_func (DzlSuggestionEntry *entry,
                                  GdkRectangle       *area,
                                  gboolean           *is_absolute,
                                  gpointer            user_data)
{
  gint new_width;

#define RIGHT_MARGIN 6

  dzl_suggestion_entry_window_position_func (entry, area, is_absolute, NULL);

  new_width = MAX (area->width * 2 / 5, MIN_WIDTH);
  area->x += area->width - new_width;
  area->width = new_width - RIGHT_MARGIN;
  area->y -= 3;

#undef RIGHT_MARGIN

}

static void
on_search_finished_cb (GObject      *source_object,
                       GAsyncResult *result,
                       gpointer      user_data)
{
  g_autoptr (GListModel) model = NULL;
  g_autoptr (GError) error = NULL;
  DzlSuggestionEntry *entry;
  GcalSearchButton *self;

  self = GCAL_SEARCH_BUTTON (user_data);
  model = gcal_search_engine_search_finish (GCAL_SEARCH_ENGINE (source_object), result, &error);

  entry = dzl_suggestion_button_get_entry (DZL_SUGGESTION_BUTTON (self));
  dzl_suggestion_entry_set_model (entry, model);
}

static void
on_search_entry_changed_cb (GcalSearchButton *self)
{
  g_autofree gchar *sexp_query = NULL;
  DzlSuggestionEntry *entry;
  GcalSearchEngine *search_engine;
  const gchar *typed_text;

  entry = dzl_suggestion_button_get_entry (DZL_SUGGESTION_BUTTON (self));
  typed_text = dzl_suggestion_entry_get_typed_text (entry);

  g_cancellable_cancel (self->cancellable);

  if (dzl_str_empty0 (typed_text))
    {
      dzl_suggestion_entry_set_model (entry, NULL);
      return;
    }

  sexp_query = g_strdup_printf ("(contains? \"summary\" \"%s\")", typed_text);
  search_engine = gcal_context_get_search_engine (self->context);
  gcal_search_engine_search (search_engine,
                             sexp_query,
                             50,
                             self->cancellable,
                             on_search_finished_cb,
                             g_object_ref (self));
}

static void
on_search_entry_suggestion_activated_cb (DzlSuggestionEntry *entry,
                                         GcalSearchHit      *search_hit,
                                         GcalSearchButton   *self)
{
  gcal_search_hit_activate (search_hit, GTK_WIDGET (self));
}

static void
on_unfocus_action_activated_cb (GSimpleAction *action,
                                GVariant      *param,
                                gpointer       user_data)
{
  DzlSuggestionEntry *entry;
  GcalSearchButton *self;
  GtkWidget *toplevel;

  g_assert (GCAL_IS_SEARCH_BUTTON (user_data));
  g_assert (G_IS_SIMPLE_ACTION (action));

  g_debug ("Unfocusing search button");

  self = GCAL_SEARCH_BUTTON (user_data);
  entry = dzl_suggestion_button_get_entry (DZL_SUGGESTION_BUTTON (self));
  g_signal_emit_by_name (entry, "hide-suggestions");

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
  gtk_widget_grab_focus (toplevel);
  gtk_entry_set_text (GTK_ENTRY (entry), "");
}

static void
on_shortcut_grab_focus_cb (GtkWidget *widget,
                           gpointer   user_data)
{
  g_debug ("Focusing search button");

  gtk_widget_grab_focus (GTK_WIDGET (user_data));
}


/*
 * GObject overrides
 */


static void
gcal_search_button_finalize (GObject *object)
{
  GcalSearchButton *self = (GcalSearchButton *)object;

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);
  g_clear_object (&self->context);

  G_OBJECT_CLASS (gcal_search_button_parent_class)->finalize (object);
}

static void
gcal_search_button_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GcalSearchButton *self = GCAL_SEARCH_BUTTON (object);

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
gcal_search_button_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GcalSearchButton *self = GCAL_SEARCH_BUTTON (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_assert (self->context == NULL);
      self->context = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}


static void
gcal_search_button_class_init (GcalSearchButtonClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gcal_search_button_finalize;
  object_class->get_property = gcal_search_button_get_property;
  object_class->set_property = gcal_search_button_set_property;

  /**
   * GcalSearchButton::context:
   *
   * The #GcalContext of the application.
   */
  properties[PROP_CONTEXT] = g_param_spec_object ("context",
                                                  "Context of the application",
                                                  "The context of the application",
                                                  GCAL_TYPE_CONTEXT,
                                                  G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gcal_search_button_init (GcalSearchButton *self)
{
  g_autoptr (GSimpleActionGroup) group = NULL;
  DzlShortcutController *controller;
  DzlSuggestionEntry *entry;

  static GActionEntry actions[] = {
    { "unfocus", on_unfocus_action_activated_cb },
  };

  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   self);

  gtk_widget_insert_action_group (GTK_WIDGET (self), "search", G_ACTION_GROUP (group));

  entry = dzl_suggestion_button_get_entry (DZL_SUGGESTION_BUTTON (self));
  g_signal_connect_object (entry,
                           "changed",
                           G_CALLBACK (on_search_entry_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (entry,
                           "suggestion-activated",
                           G_CALLBACK (on_search_entry_suggestion_activated_cb),
                           self,
                           0);

  dzl_suggestion_entry_set_position_func (entry,
                                          position_suggestion_popover_func,
                                          self,
                                          NULL);

  controller = dzl_shortcut_controller_find (GTK_WIDGET (entry));
  dzl_shortcut_controller_add_command_callback (controller,
                                                "org.gnome.calendar.search",
                                                "<Primary>f",
                                                DZL_SHORTCUT_PHASE_CAPTURE | DZL_SHORTCUT_PHASE_GLOBAL,
                                                on_shortcut_grab_focus_cb,
                                                self,
                                                NULL);

  dzl_shortcut_controller_add_command_action (controller,
                                              "org.gnome.calendar.search-button.unfocus",
                                              "Escape",
                                              DZL_SHORTCUT_PHASE_CAPTURE,
                                              "search.unfocus");
}
