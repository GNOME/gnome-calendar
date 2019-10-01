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

#include "config.h"

#define G_LOG_DOMAIN "GcalMultiChoice"

#include "gcal-multi-choice.h"

struct _GcalMultiChoice
{
  GtkBox parent;
  GtkWidget *down_button;
  GtkWidget *stack;
  GtkWidget *up_button;
  gint value;
  gint min_value;
  gint max_value;
  gboolean wrap;
  gboolean animate;
  GtkWidget **choices;
  gint n_choices;
  guint click_id;
  GtkWidget *active;
  GtkWidget *label1;
  GtkWidget *label2;
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
  NUM_PROPERTIES
};

enum
{
  WRAPPED,
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

static gboolean
button_activate (GcalMultiChoice *self,
                 GtkWidget      *button)
{
  if (button == self->down_button)
    go_down (self);
  else if (button == self->up_button)
    go_up (self);
  else
    g_assert_not_reached ();

  return TRUE;
}

static gboolean
button_timeout (gpointer user_data)
{
  GcalMultiChoice *self = GCAL_MULTI_CHOICE (user_data);
  gboolean res;

  if (self->click_id == 0)
    return G_SOURCE_REMOVE;

  if (!gtk_widget_get_mapped (self->down_button) && !gtk_widget_get_mapped (self->up_button))
    {
      if (self->click_id)
        g_source_remove (self->click_id);
      self->click_id = 0;
      self->active = NULL;
      return G_SOURCE_REMOVE;
    }

  res = button_activate (self, self->active);
  if (!res)
    {
      g_source_remove (self->click_id);
      self->click_id = 0;
    }

  return res;
}

static gboolean
button_press_cb (GtkWidget      *widget,
                 GdkEventButton *button,
                 GcalMultiChoice *self)
{
  gint double_click_time;

  if (button->type != GDK_BUTTON_PRESS)
    return TRUE;

  g_object_get (gtk_widget_get_settings (widget),
                "gtk-double-click-time", &double_click_time,
                NULL);

  if (self->click_id != 0)
    g_source_remove (self->click_id);

  self->active = widget;

  self->click_id = gdk_threads_add_timeout (double_click_time,
                                            button_timeout,
                                            self);
  g_source_set_name_by_id (self->click_id, "[gtk+] button_timeout");
  button_timeout (self);

  return TRUE;
}

static gboolean
button_release_cb (GtkWidget      *widget,
                   GdkEventButton *event,
                   GcalMultiChoice *self)
{
  if (self->click_id != 0)
    {
      g_source_remove (self->click_id);
      self->click_id = 0;
    }

  self->active = NULL;

  return TRUE;
}

static void
button_clicked_cb (GtkWidget      *button,
                   GcalMultiChoice *self)
{
  if (self->click_id != 0)
    return;

  button_activate (self, button);
}

static void
gcal_multi_choice_init (GcalMultiChoice *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
gcal_multi_choice_dispose (GObject *object)
{
  GcalMultiChoice *self = GCAL_MULTI_CHOICE (object);

  if (self->click_id != 0)
    {
      g_source_remove (self->click_id);
      self->click_id = 0;
    }

  g_free (self->choices);
  self->choices = NULL;

  if (self->format_destroy)
    {
      self->format_destroy (self->format_data);
      self->format_destroy = NULL;
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

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gcal_multi_choice_class_init (GcalMultiChoiceClass *class)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = gcal_multi_choice_dispose;
  object_class->set_property = gcal_multi_choice_set_property;
  object_class->get_property = gcal_multi_choice_get_property;

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

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);

  signals[WRAPPED] =
    g_signal_new ("wrapped",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/multi-choice.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalMultiChoice, down_button);
  gtk_widget_class_bind_template_child (widget_class, GcalMultiChoice, up_button);
  gtk_widget_class_bind_template_child (widget_class, GcalMultiChoice, stack);
  gtk_widget_class_bind_template_child (widget_class, GcalMultiChoice, label1);
  gtk_widget_class_bind_template_child (widget_class, GcalMultiChoice, label2);

  gtk_widget_class_bind_template_callback (widget_class, button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, button_press_cb);
  gtk_widget_class_bind_template_callback (widget_class, button_release_cb);

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
    gtk_container_remove (GTK_CONTAINER (self->stack), self->choices[i]);
  g_free (self->choices);

  self->n_choices = g_strv_length ((gchar **)choices);
  self->choices = g_new (GtkWidget *, self->n_choices);
  for (i = 0; i < self->n_choices; i++)
    {
      self->choices[i] = gtk_label_new (choices[i]);
      gtk_widget_show (self->choices[i]);
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
