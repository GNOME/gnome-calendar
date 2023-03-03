/* GTK - The GIMP Toolkit
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

struct _GcalMultiChoice
{
  GtkBox parent;
  GtkWidget *down_button;
  GtkWidget *button;
  GtkStack *stack;
  GtkWidget *up_button;
  gint value;
  gint min_value;
  gint max_value;
  gboolean wrap;
  gboolean animate;
  GtkWidget **choices;
  gint n_choices;
  GtkWidget *active;
  GtkWidget *label1;
  GtkWidget *label2;
  GtkWidget *popover;
  GcalMultiChoiceFormatCallback format_cb;
  gpointer                      format_data;
  GDestroyNotify                format_destroy;
};

enum
{
  PROP_VALUE = 1,
  PROP_MIN_VALUE,
  PROP_MAX_VALUE,
  PROP_WRAP,
  PROP_ANIMATE,
  PROP_CHOICES,
  PROP_POPOVER,
  NUM_PROPERTIES
};

enum
{
  WRAPPED,
  ACTIVATE,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };
static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

G_DEFINE_TYPE (GcalMultiChoice, gcal_multi_choice, GTK_TYPE_BOX)

static gchar *
get_value_string (GcalMultiChoice *self,
                  gint            value)
{
  if (self->format_cb)
    return self->format_cb (self, value, self->format_data);
  else if (0 <= value && value < self->n_choices)
    return g_strdup (gtk_label_get_label (GTK_LABEL (self->choices[value])));
  else
    return g_strdup_printf ("%d", value);
}

static void
apply_value (GcalMultiChoice        *self,
             GtkStackTransitionType  transition)
{
  GtkWidget *label;
  const gchar *name;
  gchar *text;

  if (gtk_stack_get_visible_child (GTK_STACK (self->stack)) == self->label1)
    {
      name = "label2";
      label = self->label2;
    }
  else
    {
      name = "label1";
      label = self->label1;
    }

  text = get_value_string (self, self->value);
  gtk_label_set_text (GTK_LABEL (label), text);
  g_free (text);

  if (!self->animate)
    transition = GTK_STACK_TRANSITION_TYPE_NONE;

  gtk_stack_set_visible_child_full (GTK_STACK (self->stack), name, transition);

  gtk_widget_set_sensitive (self->down_button,
                            self->wrap || self->value > self->min_value);
  gtk_widget_set_sensitive (self->up_button,
                            self->wrap || self->value < self->max_value);
}

static void
set_value (GcalMultiChoice         *self,
           gint                    value,
           GtkStackTransitionType  transition)
{
  value = CLAMP (value, self->min_value, self->max_value);

  if (self->value == value)
    return;

  self->value = value;

  apply_value (self, transition);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_VALUE]);
}

static void
go_up (GcalMultiChoice *self)
{
  gboolean wrapped = FALSE;
  gint value;

  value = self->value + 1;
  if (value > self->max_value)
    {
      if (!self->wrap)
        return;

      value = self->min_value;
      wrapped = TRUE;
    }

  set_value (self, value, GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT);

  if (wrapped)
    g_signal_emit (self, signals[WRAPPED], 0);
}

static void
go_down (GcalMultiChoice *self)
{
  gint value;
  gboolean wrapped = FALSE;

  value = self->value - 1;
  if (value < self->min_value)
    {
      if (!self->wrap)
        return;

      value = self->max_value;
      wrapped = TRUE;
    }

  set_value (self, value, GTK_STACK_TRANSITION_TYPE_SLIDE_RIGHT);

  if (wrapped)
    g_signal_emit (self, signals[WRAPPED], 0);
}

static void
update_sensitivity (GcalMultiChoice *self)
{
  gboolean has_popup;

  has_popup = self->popover != NULL;

  gtk_widget_set_can_target (self->button, has_popup);

  gtk_accessible_update_property (GTK_ACCESSIBLE (self->button),
                                  GTK_ACCESSIBLE_PROPERTY_HAS_POPUP, has_popup,
                                  -1);
  if (self->popover != NULL)
    gtk_accessible_update_relation (GTK_ACCESSIBLE (self->button),
                                    GTK_ACCESSIBLE_RELATION_CONTROLS, self->popover, NULL,
                                    -1);
  else
    gtk_accessible_reset_relation (GTK_ACCESSIBLE (self->button),
                                   GTK_ACCESSIBLE_RELATION_CONTROLS);
}

static void
button_clicked_cb (GtkWidget      *button,
                   GcalMultiChoice *self)
{
  if (button == self->down_button)
    go_down (self);
  else if (button == self->up_button)
    go_up (self);
  else
    g_assert_not_reached ();
}

static void
button_toggled_cb (GcalMultiChoice *self)
{
  const gboolean active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->button));

  if (self->popover)
    {
      if (active)
        {
          gtk_popover_popup (GTK_POPOVER (self->popover));
          gtk_accessible_update_state (GTK_ACCESSIBLE (self),
                                       GTK_ACCESSIBLE_STATE_EXPANDED, TRUE,
                                       -1);
        }
      else
        {
          gtk_popover_popdown (GTK_POPOVER (self->popover));
          gtk_accessible_reset_state (GTK_ACCESSIBLE (self),
                                      GTK_ACCESSIBLE_STATE_EXPANDED);
        }
    }
}

static gboolean
menu_deactivate_cb (GcalMultiChoice *self)
{
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->button), FALSE);

  return TRUE;
}

static void
popover_destroy_cb (GcalMultiChoice *menu_button)
{
  gcal_multi_choice_set_popover (menu_button, NULL);
}

static void
gcal_multi_choice_init (GcalMultiChoice *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  update_sensitivity (self);
}

static void
gcal_multi_choice_dispose (GObject *object)
{
  GcalMultiChoice *self = GCAL_MULTI_CHOICE (object);

  g_free (self->choices);
  self->choices = NULL;

  if (self->format_destroy)
    {
      self->format_destroy (self->format_data);
      self->format_destroy = NULL;
    }

  if (self->popover)
    {
      g_signal_handlers_disconnect_by_func (self->popover,
                                            menu_deactivate_cb,
                                            object);
      g_signal_handlers_disconnect_by_func (self->popover,
                                            popover_destroy_cb,
                                            object);
      gtk_widget_unparent (self->popover);
      self->popover = NULL;
    }

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
      g_value_set_int (value, self->value);
      break;

    case PROP_MIN_VALUE:
      g_value_set_int (value, self->min_value);
      break;

    case PROP_MAX_VALUE:
      g_value_set_int (value, self->max_value);
      break;

    case PROP_WRAP:
      g_value_set_boolean (value, self->wrap);
      break;

    case PROP_ANIMATE:
      g_value_set_boolean (value, self->animate);
      break;

    case PROP_POPOVER:
      g_value_set_object (value, self->popover);
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
      self->min_value = g_value_get_int (value);
      g_object_notify_by_pspec (object, properties[PROP_MIN_VALUE]);
      gcal_multi_choice_set_value (self, self->value);
      break;

    case PROP_MAX_VALUE:
      self->max_value = g_value_get_int (value);
      g_object_notify_by_pspec (object, properties[PROP_MAX_VALUE]);
      gcal_multi_choice_set_value (self, self->value);
      break;

    case PROP_WRAP:
      self->wrap = g_value_get_boolean (value);
      g_object_notify_by_pspec (object, properties[PROP_WRAP]);
      break;

    case PROP_ANIMATE:
      self->animate = g_value_get_boolean (value);
      g_object_notify_by_pspec (object, properties[PROP_ANIMATE]);
      break;

    case PROP_CHOICES:
      gcal_multi_choice_set_choices (self, (const gchar **)g_value_get_boxed (value));
      break;

    case PROP_POPOVER:
      gcal_multi_choice_set_popover (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gcal_multi_choice_notify (GObject    *object,
                        GParamSpec *pspec)
{
  if (strcmp (pspec->name, "focus-on-click") == 0)
    {
      GcalMultiChoice *self = GCAL_MULTI_CHOICE (object);

      gtk_widget_set_focus_on_click (self->button,
                                     gtk_widget_get_focus_on_click (GTK_WIDGET (self)));
    }

  if (G_OBJECT_CLASS (gcal_multi_choice_parent_class)->notify)
    G_OBJECT_CLASS (gcal_multi_choice_parent_class)->notify (object, pspec);
}

static void
gcal_multi_choice_state_flags_changed (GtkWidget    *widget,
                                     GtkStateFlags previous_state_flags)
{
  GcalMultiChoice *self = GCAL_MULTI_CHOICE (widget);

  if (!gtk_widget_is_sensitive (widget))
    {
      if (self->popover)
        gtk_widget_set_visible (self->popover, FALSE);
    }
}

static void
gcal_multi_choice_measure (GtkWidget      *widget,
                         GtkOrientation  orientation,
                         int             for_size,
                         int            *minimum,
                         int            *natural,
                         int            *minimum_baseline,
                         int            *natural_baseline)
{
  GcalMultiChoice *self = GCAL_MULTI_CHOICE (widget);

  gtk_widget_measure (self->button,
                      orientation,
                      for_size,
                      minimum, natural,
                      minimum_baseline, natural_baseline);

}

static void
gcal_multi_choice_size_allocate (GtkWidget *widget,
                               int        width,
                               int        height,
                               int        baseline)
{
  GcalMultiChoice *self= GCAL_MULTI_CHOICE (widget);

  gtk_widget_size_allocate (self->button,
                            &(GtkAllocation) { 0, 0, width, height },
                            baseline);
  if (self->popover)
    gtk_popover_present (GTK_POPOVER (self->popover));
}

static gboolean
gcal_multi_choice_focus (GtkWidget        *widget,
                       GtkDirectionType  direction)
{
  GcalMultiChoice *self = GCAL_MULTI_CHOICE (widget);

  if (self->popover && gtk_widget_get_visible (self->popover))
    return gtk_widget_child_focus (self->popover, direction);
  else
    return gtk_widget_child_focus (self->button, direction);
}

static gboolean
gcal_multi_choice_grab_focus (GtkWidget *widget)
{
  GcalMultiChoice *self = GCAL_MULTI_CHOICE (widget);

  return gtk_widget_grab_focus (self->button);
}

static void
gcal_multi_choice_class_init (GcalMultiChoiceClass *class)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->notify = gcal_multi_choice_notify;
  object_class->dispose = gcal_multi_choice_dispose;
  object_class->set_property = gcal_multi_choice_set_property;
  object_class->get_property = gcal_multi_choice_get_property;

  widget_class->measure = gcal_multi_choice_measure;
  widget_class->size_allocate = gcal_multi_choice_size_allocate;
  widget_class->state_flags_changed = gcal_multi_choice_state_flags_changed;
  widget_class->focus = gcal_multi_choice_focus;
  widget_class->grab_focus = gcal_multi_choice_grab_focus;

  properties[PROP_VALUE] =
      g_param_spec_int ("value", "Value", "Value",
                        G_MININT, G_MAXINT, 0,
                        G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);
  properties[PROP_MIN_VALUE] =
      g_param_spec_int ("min-value", "Minimum Value", "Minimum Value",
                        G_MININT, G_MAXINT, 0,
                        G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);
  properties[PROP_MAX_VALUE] =
      g_param_spec_int ("max-value", "Maximum Value", "Maximum Value",
                        G_MININT, G_MAXINT, 0,
                        G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);
  properties[PROP_WRAP] =
      g_param_spec_boolean ("wrap", "Wrap", "Wrap",
                            FALSE,
                            G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);
  properties[PROP_ANIMATE] =
      g_param_spec_boolean ("animate", "Animate", "Animate",
                            FALSE,
                            G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);
  properties[PROP_CHOICES] =
      g_param_spec_boxed ("choices", "Choices", "Choices",
                          G_TYPE_STRV,
                          G_PARAM_WRITABLE|G_PARAM_EXPLICIT_NOTIFY);
  properties[PROP_POPOVER] =
      g_param_spec_object ("popover", "Popover", "Popover",
                           GTK_TYPE_POPOVER,
                           G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);

  signals[WRAPPED] =
    g_signal_new ("wrapped",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);
  signals[ACTIVATE] =
      g_signal_new ("activate",
                    G_TYPE_FROM_CLASS (object_class),
                    G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                    0,
                    NULL, NULL,
                    NULL,
                    G_TYPE_NONE, 0);

  gtk_widget_class_set_activate_signal (widget_class, signals[ACTIVATE]);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/event-editor/gcal-multi-choice.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalMultiChoice, down_button);
  gtk_widget_class_bind_template_child (widget_class, GcalMultiChoice, up_button);
  gtk_widget_class_bind_template_child (widget_class, GcalMultiChoice, button);
  gtk_widget_class_bind_template_child (widget_class, GcalMultiChoice, stack);
  gtk_widget_class_bind_template_child (widget_class, GcalMultiChoice, label1);
  gtk_widget_class_bind_template_child (widget_class, GcalMultiChoice, label2);

  gtk_widget_class_bind_template_callback (widget_class, button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, button_toggled_cb);

  gtk_widget_class_set_css_name (widget_class, "navigator");
}

GtkWidget *
gcal_multi_choice_new (void)
{
  return GTK_WIDGET (g_object_new (GCAL_TYPE_MULTI_CHOICE, NULL));
}

void
gcal_multi_choice_set_value (GcalMultiChoice *self,
                            gint            value)
{
  set_value (self, value, GTK_STACK_TRANSITION_TYPE_NONE);
}

gint
gcal_multi_choice_get_value (GcalMultiChoice *self)
{
  return self->value;
}

void
gcal_multi_choice_set_choices (GcalMultiChoice  *self,
                               const gchar     **choices)
{
  gint i;

  for (i = 0; i < self->n_choices; i++)
    gtk_stack_remove (self->stack, self->choices[i]);
  g_free (self->choices);

  self->n_choices = g_strv_length ((gchar **)choices);
  self->choices = g_new (GtkWidget *, self->n_choices);
  for (i = 0; i < self->n_choices; i++)
    {
      self->choices[i] = gtk_label_new (choices[i]);
      gtk_widget_set_visible (self->choices[i], TRUE);
      gtk_stack_add_named (GTK_STACK (self->stack),
                           self->choices[i],
                           choices[i]);
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CHOICES]);
}

void
gcal_multi_choice_set_format_callback (GcalMultiChoice               *self,
                                       GcalMultiChoiceFormatCallback  callback,
                                       gpointer                       user_data,
                                       GDestroyNotify                 destroy)
{
  if (self->format_destroy)
    self->format_destroy (self->format_data);

  self->format_cb = callback;
  self->format_data = user_data;
  self->format_destroy = destroy;

  apply_value (self, GTK_STACK_TRANSITION_TYPE_NONE);
}

void
gcal_multi_choice_set_popover (GcalMultiChoice *self,
                               GtkWidget       *popover)
{
  g_return_if_fail (GCAL_IS_MULTI_CHOICE (self));
  g_return_if_fail (popover == NULL || GTK_IS_POPOVER (popover));

  g_object_freeze_notify (G_OBJECT (self));

  if (self->popover)
    {
      if (gtk_widget_get_visible (self->popover))
        gtk_widget_set_visible (self->popover, FALSE);

      g_signal_handlers_disconnect_by_func (self->popover,
                                            menu_deactivate_cb,
                                            self);
      g_signal_handlers_disconnect_by_func (self->popover,
                                            popover_destroy_cb,
                                            self);

      gtk_widget_unparent (self->popover);
    }

  self->popover = popover;

  if (popover)
    {
      gtk_widget_set_parent (self->popover, GTK_WIDGET (self));
      g_signal_connect_swapped (self->popover, "closed",
                                G_CALLBACK (menu_deactivate_cb), self);
      g_signal_connect_swapped (self->popover, "destroy",
                                G_CALLBACK (popover_destroy_cb), self);
      gtk_popover_set_position (GTK_POPOVER (self->popover), GTK_POS_BOTTOM);
    }

  update_sensitivity (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_POPOVER]);
  g_object_thaw_notify (G_OBJECT (self));
}

GtkPopover *
gcal_multi_choice_get_popover (GcalMultiChoice *self)
{
  g_return_val_if_fail (GCAL_IS_MULTI_CHOICE (self), NULL);

  return GTK_POPOVER (self->popover);
}
