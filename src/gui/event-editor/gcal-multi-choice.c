/* gcal-multi-choice.c
 *
 * GTK - The GIMP Toolkit
 *
 * Copyright (C) 2015 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#define G_LOG_DOMAIN "GcalMultiChoice"

#include "config.h"

#include "gcal-multi-choice.h"
#include "gcal-stack-label.h"
#include "gcal-utils.h"

#define WRAP(value, min, max) (((value - min) % (max + 1 - min)) + (max + 1 - min)) % (max + 1 - min) + min

struct _GcalMultiChoice
{
  GtkWidget                       parent;

  GtkWidget                      *down_button;
  GtkWidget                      *button;
  GtkWidget                      *up_button;
  GcalStackLabel                 *label;

  gint                            value;
  gint                            min_value;
  gint                            max_value;
  gchar                          *category;
  gchar                          *previous_button_tooltip;
  gchar                          *next_button_tooltip;

  GtkStringList                  *string_list;
  GtkWidget                      *active;
  GtkPopoverBin                  *popover_bin;

  GcalMultiChoiceFormatCallback   format_cb;
  gpointer                        format_data;
  GDestroyNotify                  format_destroy;
  GcalMultiChoiceValueCallback    prev_cb;
  GcalMultiChoiceValueCallback    next_cb;
};

enum
{
  PROP_0,
  PROP_VALUE,
  PROP_MIN_VALUE,
  PROP_MAX_VALUE,
  PROP_STRING_LIST,
  PROP_POPOVER,
  PROP_CATEGORY,
  PROP_PREVIOUS_BUTTON_TOOLTIP,
  PROP_NEXT_BUTTON_TOOLTIP,
  N_PROPS,
};

enum
{
  WRAPPED,
  ACTIVATE,
  USER_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };
static GParamSpec *properties[N_PROPS] = { NULL, };

static void gcal_multi_choice_accessible_range_init (GtkAccessibleRangeInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (GcalMultiChoice, gcal_multi_choice, GTK_TYPE_WIDGET,
                               G_IMPLEMENT_INTERFACE (GTK_TYPE_ACCESSIBLE_RANGE, gcal_multi_choice_accessible_range_init))

/*
 * Auxiliary methods
 */

static unsigned int
string_list_get_n_items (GcalMultiChoice *self)
{
  return self->string_list ? g_list_model_get_n_items (G_LIST_MODEL (self->string_list)) : 0;
}

static gchar *
get_value_string (GcalMultiChoice *self,
                  gint             value)
{
  if (self->format_cb)
    return self->format_cb (self, value, self->format_data);
  else if (0 <= value && value < string_list_get_n_items (self))
    return g_strdup (gtk_string_list_get_string (self->string_list, value));
  else
    return g_strdup_printf ("%d", value);
}

static void
apply_value (GcalMultiChoice *self)
{
  g_autofree char *text = NULL;

  text = get_value_string (self, self->value);
  gcal_stack_label_set_label (self->label, text);
  gtk_accessible_update_property (GTK_ACCESSIBLE (self),
                                  GTK_ACCESSIBLE_PROPERTY_VALUE_TEXT, text,
                                  -1);
}

static void
action_activated (GcalMultiChoice *self,
                  int              value)
{
  int wrapped_value;

  wrapped_value = WRAP (value, self->min_value, self->max_value);

  gcal_multi_choice_set_value (self, wrapped_value);

  if (wrapped_value != value)
    g_signal_emit (self, signals[WRAPPED], 0);

  gtk_widget_grab_focus (GTK_WIDGET (self));

  g_signal_emit (self, signals[USER_CHANGED], 0);
}

static void
up_action_activated (GcalMultiChoice *self)
{
  int value;

  value = self->next_cb ? self->next_cb (self->value) : self->value + 1;
  g_assert_cmpint (value, >, self->value);

  action_activated (self, value);
}

static void
down_action_activated (GcalMultiChoice *self)
{
  int value;

  value = self->prev_cb ? self->prev_cb (self->value) : self->value - 1;
  g_assert_cmpint (value, <, self->value);

  action_activated (self, value);
}

static void
update_sensitivity (GcalMultiChoice *self)
{
  GtkWidget *popover = gtk_popover_bin_get_popover (self->popover_bin);
  GtkAccessibleTristate state;

  gtk_widget_set_can_target (self->button, !!popover);

  state = !!popover ? GTK_ACCESSIBLE_TRISTATE_TRUE : GTK_ACCESSIBLE_TRISTATE_FALSE;
  gtk_accessible_update_property (GTK_ACCESSIBLE (self),
                                  GTK_ACCESSIBLE_PROPERTY_HAS_POPUP, state,
                                  -1);
  if (popover != NULL)
    gtk_accessible_update_relation (GTK_ACCESSIBLE (self),
                                    GTK_ACCESSIBLE_RELATION_CONTROLS, popover, NULL,
                                    -1);
  else
    gtk_accessible_reset_relation (GTK_ACCESSIBLE (self),
                                   GTK_ACCESSIBLE_RELATION_CONTROLS);
}

static void
on_widget_activated (GcalMultiChoice *self)
{
  if (gtk_popover_bin_get_popover (self->popover_bin))
    {
      gtk_popover_bin_popup (self->popover_bin);
      gtk_accessible_update_state (GTK_ACCESSIBLE (self),
                                   GTK_ACCESSIBLE_STATE_EXPANDED, TRUE,
                                   -1);
    }
  else
    {
      gtk_accessible_reset_state (GTK_ACCESSIBLE (self),
                                  GTK_ACCESSIBLE_STATE_EXPANDED);
    }
}

static void
on_gesture_click_released (GcalMultiChoice *self,
                           gint             n_press,
                           double           x,
                           double           y,
                           GtkGestureClick *gesture)
{
  if (gtk_widget_contains (GTK_WIDGET (self), x, y))
    {
      if (!gtk_widget_grab_focus (GTK_WIDGET (self)))
        g_assert_not_reached ();

      gtk_widget_activate (GTK_WIDGET (self));
    }

  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_NONE);
}

/*
 * GObject overrides
 */

static void
gcal_multi_choice_dispose (GObject *object)
{
  GcalMultiChoice *self = GCAL_MULTI_CHOICE (object);

  g_clear_pointer ((GtkWidget **) &self->popover_bin, gtk_widget_unparent);

  g_clear_pointer (&self->string_list, g_object_unref);

  g_clear_pointer (&self->category, g_free);
  g_clear_pointer (&self->previous_button_tooltip, g_free);
  g_clear_pointer (&self->next_button_tooltip, g_free);

  if (self->format_destroy)
    g_clear_pointer (&self->format_data, self->format_destroy);

  G_OBJECT_CLASS (gcal_multi_choice_parent_class)->dispose (object);
}

static void
gcal_multi_choice_get_property (GObject    *object,
                                guint       property_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GcalMultiChoice *self = GCAL_MULTI_CHOICE (object);

  switch (property_id)
    {
    case PROP_VALUE:
      g_value_set_int (value, gcal_multi_choice_get_value (self));
      break;

    case PROP_MIN_VALUE:
      g_value_set_int (value, gcal_multi_choice_get_min_value (self));
      break;

    case PROP_MAX_VALUE:
      g_value_set_int (value, gcal_multi_choice_get_max_value (self));
      break;

    case PROP_STRING_LIST:
      g_value_set_object (value, gcal_multi_choice_get_string_list (self));
      break;

    case PROP_POPOVER:
      g_value_set_object (value, gcal_multi_choice_get_popover (self));
      break;

    case PROP_CATEGORY:
      g_value_set_string (value, gcal_multi_choice_get_category (self));
      break;

    case PROP_PREVIOUS_BUTTON_TOOLTIP:
      g_value_set_string (value, gcal_multi_choice_get_previous_button_tooltip (self));
      break;

    case PROP_NEXT_BUTTON_TOOLTIP:
      g_value_set_string (value, gcal_multi_choice_get_next_button_tooltip (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gcal_multi_choice_set_property (GObject      *object,
                                guint         property_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GcalMultiChoice *self = GCAL_MULTI_CHOICE (object);

  switch (property_id)
    {
    case PROP_VALUE:
      gcal_multi_choice_set_value (self, g_value_get_int (value));
      break;

    case PROP_MIN_VALUE:
      gcal_multi_choice_set_min_value (self, g_value_get_int (value));
      break;

    case PROP_MAX_VALUE:
      gcal_multi_choice_set_max_value (self, g_value_get_int (value));
      break;

    case PROP_STRING_LIST:
      gcal_multi_choice_set_string_list (self, g_value_get_object (value));
      break;

    case PROP_POPOVER:
      gcal_multi_choice_set_popover (self, g_value_get_object (value));
      break;

    case PROP_CATEGORY:
      gcal_multi_choice_set_category (self, g_value_get_string (value));
      break;

    case PROP_PREVIOUS_BUTTON_TOOLTIP:
      gcal_multi_choice_set_previous_button_tooltip (self, g_value_get_string (value));
      break;

    case PROP_NEXT_BUTTON_TOOLTIP:
      gcal_multi_choice_set_next_button_tooltip (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

/*
 * GtkWidget overrides
 */

static void
gcal_multi_choice_state_flags_changed (GtkWidget    *widget,
                                       GtkStateFlags previous_state_flags)
{
  GcalMultiChoice *self;
  GtkStateFlags state_flags;
  GtkWidget *popover;

  g_assert (GCAL_IS_MULTI_CHOICE (widget));

  self = GCAL_MULTI_CHOICE (widget);

  if ((popover = gtk_popover_bin_get_popover (self->popover_bin)))
    {
      if (gtk_widget_get_mapped (popover))
        gtk_widget_set_state_flags (self->button, GTK_STATE_FLAG_CHECKED, FALSE);
      else
        gtk_widget_unset_state_flags (self->button, GTK_STATE_FLAG_CHECKED);
    }
  else
    {
      gtk_widget_set_state_flags (self->button, GTK_STATE_FLAG_NORMAL, TRUE);
    }

  state_flags = gtk_widget_get_state_flags (widget);
  if (state_flags & GTK_STATE_FLAG_FOCUSED)
    gtk_widget_set_state_flags (widget, GTK_STATE_FLAG_FOCUS_VISIBLE, FALSE);
}

/*
 * GtkAccessibleRange overrides
 */

static gboolean
gcal_multi_choice_accessible_range_set_current_value (GtkAccessibleRange *accessible_range,
                                                      gdouble             value)
{
  gcal_multi_choice_set_value (GCAL_MULTI_CHOICE (accessible_range), value);
  return TRUE;
}

/*
 * Init
 */

static void
gcal_multi_choice_class_init (GcalMultiChoiceClass *class)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  g_type_ensure (GCAL_TYPE_STACK_LABEL);

  object_class->dispose = gcal_multi_choice_dispose;
  object_class->set_property = gcal_multi_choice_set_property;
  object_class->get_property = gcal_multi_choice_get_property;

  widget_class->state_flags_changed = gcal_multi_choice_state_flags_changed;

  /**
   * GcalMultiChoice:value:
   *
   * The current value.
   */
  properties[PROP_VALUE] =
      g_param_spec_int ("value", NULL, NULL,
                        G_MININT, G_MAXINT, 0,
                        G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GcalMultiChoice:min-value:
   *
   * The minimum value.
   */
  properties[PROP_MIN_VALUE] =
      g_param_spec_int ("min-value", NULL, NULL,
                        G_MININT, G_MAXINT, 0,
                        G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GcalMultiChoice:max-value:
   *
   * The maximum value.
   */
  properties[PROP_MAX_VALUE] =
      g_param_spec_int ("max-value", NULL, NULL,
                        G_MININT, G_MAXINT, 0,
                        G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GcalMultiChoice:string-list:
   *
   * The string list.
   */
  properties[PROP_STRING_LIST] =
      g_param_spec_object ("string-list", NULL, NULL,
                           GTK_TYPE_STRING_LIST,
                           G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GcalMultiChoice:popover:
   *
   * The popover widget.
   */
  properties[PROP_POPOVER] =
      g_param_spec_object ("popover", NULL, NULL,
                           GTK_TYPE_POPOVER,
                           G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GcalMultiChoice:category:
   *
   * The category representing this #GcalMultiChoice.
   */
  properties[PROP_CATEGORY] =
      g_param_spec_string ("category", NULL, NULL,
                           "",
                           G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GcalMultiChoice:previous-button-tooltip:
   *
   * The tooltip text of the previous button.
   */
  properties[PROP_PREVIOUS_BUTTON_TOOLTIP] =
      g_param_spec_string ("previous-button-tooltip", NULL, NULL,
                           NULL,
                           G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GcalMultiChoice:next-button-tooltip:
   *
   * The tooltip text of the next button.
   */
  properties[PROP_NEXT_BUTTON_TOOLTIP] =
      g_param_spec_string ("next-button-tooltip", NULL, NULL,
                           NULL,
                           G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  /**
   * GcalMultiChoice::wrapped:
   *
   * Emitted when the value wraps
   */
  signals[WRAPPED] =
    g_signal_new ("wrapped",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GcalMultiChoice::user-changed:
   *
   * Emitted when the user explicitly changes the value using up and down buttons.
   */
  signals[USER_CHANGED] =
    g_signal_new ("user-changed",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  gtk_widget_class_install_action (widget_class, "multi-choice.up", NULL, (GtkWidgetActionActivateFunc) up_action_activated);
  gtk_widget_class_install_action (widget_class, "multi-choice.down", NULL, (GtkWidgetActionActivateFunc) down_action_activated);

  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Up, 0, "multi-choice.up", NULL);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Down, 0, "multi-choice.down", NULL);

  signals[ACTIVATE] = gcal_util_create_activate_signal_and_shortcuts (widget_class, GCAL_TYPE_MULTI_CHOICE);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/event-editor/gcal-multi-choice.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalMultiChoice, down_button);
  gtk_widget_class_bind_template_child (widget_class, GcalMultiChoice, up_button);
  gtk_widget_class_bind_template_child (widget_class, GcalMultiChoice, button);
  gtk_widget_class_bind_template_child (widget_class, GcalMultiChoice, popover_bin);
  gtk_widget_class_bind_template_child (widget_class, GcalMultiChoice, label);

  gtk_widget_class_bind_template_callback (widget_class, on_gesture_click_released);
  gtk_widget_class_bind_template_callback (widget_class, gcal_multi_choice_state_flags_changed);

  gtk_widget_class_set_css_name (widget_class, "navigator");

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

static void
gcal_multi_choice_accessible_range_init (GtkAccessibleRangeInterface *iface)
{
  iface->set_current_value = gcal_multi_choice_accessible_range_set_current_value;
}

static void
gcal_multi_choice_init (GcalMultiChoice *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  update_sensitivity (self);

  g_signal_connect_swapped (self, "activate", (GCallback) on_widget_activated, self);
}

/*
 * Public methods
 */

/**
 * gcal_multi_choice_new:
 *
 * Creates a new #GcalMultiChoice.
 *
 * Returns: (transfer full): a newly created #GcalEventPopover
 */
GtkWidget *
gcal_multi_choice_new (void)
{
  return GTK_WIDGET (g_object_new (GCAL_TYPE_MULTI_CHOICE, NULL));
}

/**
 * gcal_multi_choice_get_value:
 * @self: a #GcalMultiChoice
 *
 * Gets the value for @self.
 *
 * Returns: The value for @self.
 */
int
gcal_multi_choice_get_value (GcalMultiChoice *self)
{
  g_assert (GCAL_IS_MULTI_CHOICE (self));

  return self->value;
}

/**
 * gcal_multi_choice_set_value:
 * @self: a #GcalMultiChoice
 * @value: the value
 *
 * Sets the value for @self.
 */
void
gcal_multi_choice_set_value (GcalMultiChoice *self,
                             int              value)
{
  g_assert (GCAL_IS_MULTI_CHOICE (self));

  value = CLAMP (value, self->min_value, self->max_value);

  if (self->value == value)
    return;

  self->value = value;

  apply_value (self);

  gtk_accessible_update_property (GTK_ACCESSIBLE (self),
                                  GTK_ACCESSIBLE_PROPERTY_VALUE_NOW, (double) value,
                                  -1);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_VALUE]);
}

/**
 * gcal_multi_choice_get_min_value:
 * @self: a #GcalMultiChoice
 *
 * Gets the minimum value for @self.
 *
 * Returns: the minimum value for @self.
 */
int
gcal_multi_choice_get_min_value (GcalMultiChoice *self)
{
  g_assert (GCAL_IS_MULTI_CHOICE (self));

  return self->min_value;
}

/**
 * gcal_multi_choice_set_min_value:
 * @self: a #GcalMultiChoice
 * @min_value: the minimum value
 *
 * Sets the minimum value for @self.
 */
void
gcal_multi_choice_set_min_value (GcalMultiChoice *self,
                                 int              min_value)
{
  g_assert (GCAL_IS_MULTI_CHOICE (self));

  if (self->min_value == min_value)
    return;

  self->min_value = min_value;

  gtk_accessible_update_property (GTK_ACCESSIBLE (self),
                                  GTK_ACCESSIBLE_PROPERTY_VALUE_MIN, (double) min_value,
                                  -1);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MIN_VALUE]);
}

/**
 * gcal_multi_choice_get_max_value:
 * @self: a #GcalMultiChoice
 *
 * Gets the maximum value for @self.
 *
 * Returns: the maximum value for @self.
 */
int
gcal_multi_choice_get_max_value (GcalMultiChoice *self)
{
  g_assert (GCAL_IS_MULTI_CHOICE (self));

  return self->max_value;
}

/**
 * gcal_multi_choice_set_max_value:
 * @self: a #GcalMultiChoice
 * @max_value: the maximum value
 *
 * Sets the maximum value for @self.
 */
void
gcal_multi_choice_set_max_value (GcalMultiChoice *self,
                                 int              max_value)
{
  g_assert (GCAL_IS_MULTI_CHOICE (self));

  if (self->max_value == max_value)
    return;

  self->max_value = max_value;

  gtk_accessible_update_property (GTK_ACCESSIBLE (self),
                                  GTK_ACCESSIBLE_PROPERTY_VALUE_MAX, (double) max_value,
                                  -1);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MAX_VALUE]);
}

/**
 * gcal_multi_choice_get_string_list:
 * @self: a #GcalMultiChoice
 *
 * Gets the string list for @self.
 *
 * Returns: (transfer none) (nullable): the string list
 */
GtkStringList *
gcal_multi_choice_get_string_list (GcalMultiChoice *self)
{
  g_assert (GCAL_IS_MULTI_CHOICE (self));

  return self->string_list;
}

/**
 * gcal_multi_choice_set_string_list:
 * @self: a #GcalMultiChoice
 * @string_list: the string list
 *
 * Sets the string list for @self.
 */
void
gcal_multi_choice_set_string_list (GcalMultiChoice *self,
                                   GtkStringList   *string_list)
{
  g_assert (GCAL_IS_MULTI_CHOICE (self));

  if (!g_set_object (&self->string_list, string_list))
    return;

  gcal_stack_label_compute_labels (self->label, self->string_list);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_STRING_LIST]);
}

/**
 * gcal_multi_choice_set_format_callback:
 * @self: a #GcalMultiChoice
 * @callback: (nullable) (scope notified) (closure user_data) (destroy destroy): callback
 * @user_data: user data passed to @callback
 * @destroy: destroy notifier for @user_data
 *
 * Sets the format callback.
 */
void
gcal_multi_choice_set_format_callback (GcalMultiChoice               *self,
                                       GcalMultiChoiceFormatCallback  callback,
                                       gpointer                       user_data,
                                       GDestroyNotify                 destroy)
{
  g_assert (GCAL_IS_MULTI_CHOICE (self));

  if (self->format_destroy)
    self->format_destroy (self->format_data);

  self->format_cb = callback;
  self->format_data = user_data;
  self->format_destroy = destroy;

  apply_value (self);
}

/**
 * gcal_multi_choice_set_popover:
 * @self: a #GcalMultiChoice
 * @popover: a popover widget.
 *
 * Sets the popover for @self.
 */
void
gcal_multi_choice_set_value_callbacks (GcalMultiChoice              *self,
                                       GcalMultiChoiceValueCallback  prev_cb,
                                       GcalMultiChoiceValueCallback  next_cb)
{
  self->prev_cb = prev_cb;
  self->next_cb = next_cb;
}

/**
 * gcal_multi_choice_get_popover:
 * @self: a #GcalMultiChoice
 *
 * Gets the popover for @self.
 *
 * Returns: (transfer none): a #GtkPopover
 */
GtkPopover *
gcal_multi_choice_get_popover (GcalMultiChoice *self)
{
  GtkWidget *popover;

  g_assert (GCAL_IS_MULTI_CHOICE (self));

  popover = gtk_popover_bin_get_popover (self->popover_bin);

  return (GtkPopover *)popover;
}

/**
 * gcal_multi_choice_set_popover:
 * @self: a #GcalMultiChoice
 * @popover: (transfer none) (nullable): the popover
 *
 * Sets the popover.
 */
void
gcal_multi_choice_set_popover (GcalMultiChoice *self,
                               GtkWidget       *popover)
{
  g_assert (GCAL_IS_MULTI_CHOICE (self));
  g_assert (popover == NULL || GTK_IS_POPOVER (popover));

  if (gtk_popover_bin_get_popover (self->popover_bin) == popover)
    return;

  gtk_popover_bin_set_popover (self->popover_bin, popover);

  update_sensitivity (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_POPOVER]);
}

/**
 * gcal_multi_choice_get_category:
 * @self: a #GcalMultiChoice
 *
 * Get the name of the category
 *
 * Returns: (transfer none): the category for the multi-choice.
 */
const gchar*
gcal_multi_choice_get_category (GcalMultiChoice *self)
{
  g_assert (GCAL_IS_MULTI_CHOICE (self));

  return self->category ? self->category : "";
}

/**
 * gcal_multi_choice_set_category:
 * @self: a #GcalMultiChoice
 * @category: The category name
 *
 * Set the value of the category name
 */
void
gcal_multi_choice_set_category (GcalMultiChoice *self,
                                const gchar     *category)
{
  g_assert (GCAL_IS_MULTI_CHOICE (self));

  if (g_strcmp0 (self->category, category) == 0)
    return;

  self->category = g_strdup (category ? category : "");

  gtk_accessible_update_property (GTK_ACCESSIBLE (self),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL, category,
                                  -1);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CATEGORY]);
}

/**
 * gcal_multi_choice_get_previous_button_tooltip:
 * @self: a #GcalMultiChoice
 *
 * Gets the tooltip text of the previous button.
 *
 * Returns: (transfer none): the tooltip text of the previous button.
 */
const char *
gcal_multi_choice_get_previous_button_tooltip (GcalMultiChoice *self)
{
  g_assert (GCAL_IS_MULTI_CHOICE (self));

  return self->previous_button_tooltip;
}

/**
 * gcal_multi_choice_set_previous_button_tooltip:
 * @self: a #GcalMultiChoice
 * @previous_button_tooltip: the previous button tooltip text
 *
 * Sets the previous button tooltip text.
 */
void
gcal_multi_choice_set_previous_button_tooltip (GcalMultiChoice *self,
                                               const char      *previous_button_tooltip)
{
  g_assert (GCAL_IS_MULTI_CHOICE (self));

  if (!g_set_str (&self->previous_button_tooltip, previous_button_tooltip))
    return;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PREVIOUS_BUTTON_TOOLTIP]);
}

/**
 * gcal_multi_choice_get_next_button_tooltip:
 * @self: a #GcalMultiChoice
 *
 * Gets the tooltip text of the next button.
 *
 * Returns: (transfer none): the tooltip text of the next button.
 */
const char *
gcal_multi_choice_get_next_button_tooltip (GcalMultiChoice *self)
{
  g_assert (GCAL_IS_MULTI_CHOICE (self));

  return self->next_button_tooltip;
}

/**
 * gcal_multi_choice_set_next_button_tooltip:
 * @self: a #GcalMultiChoice
 * @next_button_tooltip: the next button tooltip text
 *
 * Sets the next button tooltip text.
 */
void
gcal_multi_choice_set_next_button_tooltip (GcalMultiChoice *self,
                                           const char      *next_button_tooltip)
{
  g_assert (GCAL_IS_MULTI_CHOICE (self));

  if (!g_set_str (&self->next_button_tooltip, next_button_tooltip))
    return;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_NEXT_BUTTON_TOOLTIP]);
}

