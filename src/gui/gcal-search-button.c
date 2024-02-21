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
#include "gcal-debug.h"
#include "gcal-search-button.h"
#include "gcal-search-hit.h"

#include <math.h>

#define MIN_WIDTH 450

struct _GcalSearchButton
{
  AdwBin               parent;

  GtkEditable         *entry;
  GtkWidget           *popover;
  GtkListView         *results_listview;
  GtkRevealer         *results_revealer;
  GtkSingleSelection  *results_selection_model;
  GtkStack            *stack;

  GCancellable        *cancellable;
  gint                 max_width_chars;
  GListModel          *model;

  GcalContext         *context;
};

G_DEFINE_TYPE (GcalSearchButton, gcal_search_button, ADW_TYPE_BIN)

enum
{
  PROP_0,
  PROP_CONTEXT,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];


/*
 * Auxiliary methods
 */

static void
show_suggestions (GcalSearchButton *self)
{
  gtk_popover_popup (GTK_POPOVER (self->popover));
  gtk_revealer_set_reveal_child (self->results_revealer, TRUE);
}

static void
hide_suggestions (GcalSearchButton *self)
{
  gtk_revealer_set_reveal_child (self->results_revealer, FALSE);
}

static void
quit_search_entry (GcalSearchButton *self)
{
  gtk_editable_set_width_chars (self->entry, 0);
  gtk_editable_set_max_width_chars (self->entry, 0);
  gtk_stack_set_visible_child_name (self->stack, "button");

  hide_suggestions (self);

  gtk_editable_set_text (self->entry, "");
}

static inline void
scroll_to_result (GcalSearchButton *self,
                  guint             position)
{
  gtk_widget_activate_action (GTK_WIDGET (self->results_listview),
                              "list.scroll-to-item",
                              "u",
                              position);
}

static void
set_model (GcalSearchButton *self,
           GListModel       *model)
{
  GCAL_ENTRY;

  gtk_single_selection_set_model (self->results_selection_model, model);

  if (model)
    show_suggestions (self);
  else
    hide_suggestions (self);

  GCAL_EXIT;
}


/*
 * Callbacks
 */

static gchar *
escape_markup_cb (GcalSearchHit *hit,
                  const gchar   *string)
{
  g_autofree gchar *escaped_string = NULL;

  escaped_string = g_markup_escape_text (string, -1);
  escaped_string = g_strstrip (escaped_string);

  return g_steal_pointer (&escaped_string);
}

static void
on_button_clicked_cb (GtkButton        *button,
                      GcalSearchButton *self)
{
  gint max_width_chars;


  max_width_chars = gtk_editable_get_max_width_chars (self->entry);

  if (max_width_chars)
    self->max_width_chars = max_width_chars;

  gtk_editable_set_width_chars (self->entry, 1);
  gtk_editable_set_max_width_chars (self->entry, self->max_width_chars ?: 20);
  gtk_stack_set_visible_child_name (self->stack, "entry");
  gtk_widget_grab_focus (GTK_WIDGET (self->entry));
}

static void
on_focus_controller_leave_cb (GtkEventControllerFocus *focus_controller,
                              GcalSearchButton        *self)
{
  quit_search_entry (self);
}

static void
on_search_finished_cb (GObject      *source_object,
                       GAsyncResult *result,
                       gpointer      user_data)
{
  g_autoptr (GListModel) model = NULL;
  g_autoptr (GError) error = NULL;
  GcalSearchButton *self;

  GCAL_ENTRY;

  self = GCAL_SEARCH_BUTTON (user_data);
  model = gcal_search_engine_search_finish (GCAL_SEARCH_ENGINE (source_object), result, &error);

  set_model (self, model);

  GCAL_EXIT;
}

static void
on_entry_activate_cb (GtkSearchEntry   *entry,
                      GcalSearchButton *self)
{
  GcalSearchHit *hit;

  GCAL_ENTRY;

  hit = gtk_single_selection_get_selected_item (self->results_selection_model);

  if (hit)
    {
      GCAL_TRACE_MSG ("Activating \"%s\"", gcal_search_hit_get_title (hit));

      gcal_search_hit_activate (hit, GTK_WIDGET (self));
      quit_search_entry (self);
    }

  GCAL_EXIT;
}

static void
on_entry_next_match_cb (GtkSearchEntry   *entry,
                        GcalSearchButton *self)
{
  guint selected;

  GCAL_ENTRY;

  selected = gtk_single_selection_get_selected (self->results_selection_model);

  if (selected != GTK_INVALID_LIST_POSITION &&
      selected + 1 < g_list_model_get_n_items (G_LIST_MODEL (self->results_selection_model)))
    {
      GCAL_TRACE_MSG ("Changing selection to %u", selected + 1);
      gtk_single_selection_set_selected (self->results_selection_model, selected + 1);
      scroll_to_result (self, selected + 1);
    }

  GCAL_EXIT;
}

static void
on_entry_previous_match_cb (GtkSearchEntry   *entry,
                            GcalSearchButton *self)
{
  guint selected;

  GCAL_ENTRY;

  selected = gtk_single_selection_get_selected (self->results_selection_model);

  if (selected > 0 && selected != GTK_INVALID_LIST_POSITION)
    {
      GCAL_TRACE_MSG ("Changing selection to %u", selected - 1);
      gtk_single_selection_set_selected (self->results_selection_model, selected - 1);
      scroll_to_result (self, selected - 1);
    }

  GCAL_EXIT;
}

static void
on_entry_search_changed_cb (GtkSearchEntry   *entry,
                            GcalSearchButton *self)
{
  g_autofree gchar *sexp_query = NULL;
  GcalSearchEngine *search_engine;
  const gchar *text;

  GCAL_ENTRY;

  text = gtk_editable_get_text (self->entry);

  g_cancellable_cancel (self->cancellable);

  if (!text || *text == '\0')
    {
      g_debug ("Search query contents have been cleared");
      set_model (self, NULL);
      GCAL_RETURN ();
    }

  g_debug ("Search query changed to \"%s\"", text);

  sexp_query = g_strdup_printf ("(contains? \"summary\" \"%s\")", text);
  search_engine = gcal_context_get_search_engine (self->context);
  gcal_search_engine_search (search_engine,
                             sexp_query,
                             self->cancellable,
                             on_search_finished_cb,
                             g_object_ref (self));

  GCAL_EXIT;
}

static void
on_entry_stop_search_cb (GtkSearchEntry   *search_entry,
                         GcalSearchButton *self)
{
  g_debug ("Exiting search mode");
  quit_search_entry (self);
}

static void
on_results_listview_activated_cb (GtkListBox       *listbox,
                                  guint             position,
                                  GcalSearchButton *self)
{
  GcalSearchHit *search_hit;

  search_hit = g_list_model_get_item (G_LIST_MODEL (self->results_selection_model), position);
  g_assert (GCAL_IS_SEARCH_HIT (search_hit));

  gcal_search_hit_activate (search_hit, GTK_WIDGET (self));

  quit_search_entry (self);
}

static void
on_results_revealer_child_reveal_state_changed_cb (GtkRevealer      *revealer,
                                                   GParamSpec       *pspec,
                                                   GcalSearchButton *self)
{
  if (!gtk_revealer_get_child_revealed (revealer) && !gtk_revealer_get_reveal_child (revealer))
    gtk_popover_popdown (GTK_POPOVER (self->popover));
}

static gboolean
string_is_not_empty_cb (GcalSearchHit *hit,
                        const gchar   *string)
{
  return string != NULL && *string != '\0';
}


/*
 * GObject overrides
 */

static void
gcal_search_button_dispose (GObject *object)
{
  GcalSearchButton *self = (GcalSearchButton *)object;

  g_clear_pointer (&self->popover, gtk_widget_unparent);

  G_OBJECT_CLASS (gcal_search_button_parent_class)->dispose (object);
}

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
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gcal_search_button_dispose;
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

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/gui/gcal-search-button.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalSearchButton, entry);
  gtk_widget_class_bind_template_child (widget_class, GcalSearchButton, popover);
  gtk_widget_class_bind_template_child (widget_class, GcalSearchButton, results_listview);
  gtk_widget_class_bind_template_child (widget_class, GcalSearchButton, results_selection_model);
  gtk_widget_class_bind_template_child (widget_class, GcalSearchButton, results_revealer);
  gtk_widget_class_bind_template_child (widget_class, GcalSearchButton, stack);

  gtk_widget_class_bind_template_callback (widget_class, escape_markup_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_focus_controller_leave_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_entry_activate_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_entry_next_match_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_entry_previous_match_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_entry_search_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_entry_stop_search_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_results_listview_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_results_revealer_child_reveal_state_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, string_is_not_empty_cb);

  gtk_widget_class_set_css_name (widget_class, "searchbutton");
}

static void
gcal_search_button_init (GcalSearchButton *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_widget_set_parent (GTK_WIDGET (self->popover), GTK_WIDGET (self));
}

void
gcal_search_button_search (GcalSearchButton *self,
                           const gchar      *search_text)
{
  g_return_if_fail (GCAL_IS_SEARCH_BUTTON (self));

  gtk_widget_grab_focus (GTK_WIDGET (self));
  gtk_editable_set_text (self->entry, search_text);
}
