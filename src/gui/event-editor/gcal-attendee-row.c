/* gcal-attendee-row.c
 *
 * Copyright (C) 2025 Alen Galinec <mind.on.warp@gmail.com>
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
 */

#include "gcal-attendee-row.h"
#include "gcal-enums.h"
#include "gcal-event-attendee.h"
#include "gcal-utils.h"

#include <adwaita.h>
#include <gdk/gdk.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include <libintl.h>

struct _GcalAttendeeRow
{
  AdwActionRow parent_instance;

  GcalEventAttendee *attendee;

  GtkWidget         *role;
  GtkWidget         *context_menu;
  GtkGestureClick   *gesture_click;

  GSimpleActionGroup *action_group;
};

G_DEFINE_FINAL_TYPE (GcalAttendeeRow, gcal_attendee_row, ADW_TYPE_ACTION_ROW);

enum
{
  PROP_0,
  PROP_ATTENDEE,
  N_PROPS,
};

static GParamSpec *properties[N_PROPS];

/* Aux functions */

static void
on_copy_email_cb (GSimpleAction *action,
                  GVariant      *param,
                  gpointer       user_data)
{
  GcalAttendeeRow *self = GCAL_ATTENDEE_ROW (user_data);

  GdkDisplay *display = gtk_widget_get_display (GTK_WIDGET (self));
  GdkClipboard *clipboard = gdk_display_get_clipboard (display);

  g_autofree const gchar *email =
    gcal_get_email_from_mailto_uri (gcal_event_attendee_get_uri (self->attendee));

  gdk_clipboard_set_text (clipboard, email);
}

static void
show_context_menu (GcalAttendeeRow *self,
                   guint            npress,
                   double           x,
                   double           y)
{
  g_return_if_fail (GCAL_IS_ATTENDEE_ROW (self));

  const GdkRectangle position = { .x = x, .y = y };
  gtk_popover_set_pointing_to (GTK_POPOVER (self->context_menu), &position);
  gtk_popover_popup (GTK_POPOVER (self->context_menu));
}

static void
setup_row_title_and_subtitle (GcalAttendeeRow *self)
{
  g_autoptr (GString) title = NULL;
  g_autoptr (GString) subtitle = NULL;

  /* this is the initial line for both cases */
  title = g_string_new (gcal_event_attendee_get_name (self->attendee));
  g_autofree const gchar *delegator_email = gcal_get_email_from_mailto_uri (gcal_event_attendee_get_uri (self->attendee));

  if (gcal_event_attendee_get_part_status (self->attendee) != GCAL_EVENT_ATTENDEE_PART_DELEGATED)
    {
      /* show subtitle only when the name != email */
      if (g_strcmp0 (gcal_event_attendee_get_name (self->attendee), delegator_email) != 0)
        subtitle = g_string_new (delegator_email ? delegator_email : "");
    }
  else
    {
      /* append the delegator email to the title */
      if (delegator_email)
        g_string_append_printf (title, " (%s)", delegator_email);

      g_autofree const gchar *delegated_to = gcal_get_email_from_mailto_uri (gcal_event_attendee_get_delegated_to (self->attendee));
      subtitle = g_string_new (gettext ("Delegated to: "));
      g_string_append (subtitle, delegated_to);
    }

  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self), title->str);
  adw_action_row_set_subtitle (ADW_ACTION_ROW (self), subtitle ? subtitle->str : NULL);
}

static void
setup_row_content (GcalAttendeeRow *self)
{
  setup_row_title_and_subtitle (self);

  /* set role */
  if (gcal_event_attendee_get_role (self->attendee) == GCAL_EVENT_ATTENDEE_ROLE_OPTIONAL)
    {
      gtk_label_set_label (GTK_LABEL (self->role), gettext ("optional"));
    }
  else
    {
      /* other types don't need extra visual hints */
      gtk_label_set_label (GTK_LABEL (self->role), NULL);
    }
}

static void
gcal_attendee_row_finalize (GObject *object)
{
  GcalAttendeeRow *self = GCAL_ATTENDEE_ROW (object);

  g_clear_object (&self->gesture_click);
  g_clear_object (&self->action_group);

  G_OBJECT_CLASS (gcal_attendee_row_parent_class)->finalize (object);
}

static void
gcal_attendee_row_dispose (GObject *object)
{
  GcalAttendeeRow *self = GCAL_ATTENDEE_ROW (object);
  g_clear_pointer (&self->context_menu, gtk_widget_unparent);

  G_OBJECT_CLASS (gcal_attendee_row_parent_class)->dispose (object);
}

static void
gcal_attendee_row_set_property (GObject      *object,
                                guint         property_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GcalAttendeeRow *self = GCAL_ATTENDEE_ROW (object);

  switch (property_id)
    {
    case PROP_ATTENDEE:
      g_assert (self->attendee == NULL);
      self->attendee = g_value_get_object (value);
      setup_row_content (self);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
gcal_attendee_row_get_property (GObject    *object,
                                guint       property_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GcalAttendeeRow *self = GCAL_ATTENDEE_ROW (object);

  switch (property_id)
    {
    case PROP_ATTENDEE:
      g_value_set_object (value, self->attendee);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
gcal_attendee_row_class_init (GcalAttendeeRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = gcal_attendee_row_set_property;
  object_class->get_property = gcal_attendee_row_get_property;
  object_class->dispose = gcal_attendee_row_dispose;
  object_class->finalize = gcal_attendee_row_finalize;

  /**
   * GcalAttendeeRow:attendee:
   *
   * A reference to a #GcalEventAttendee for which this row is displayed.
   */
  properties[PROP_ATTENDEE] =
    g_param_spec_object ("attendee", NULL, NULL,
                         GCAL_TYPE_EVENT_ATTENDEE,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/event-editor/gcal-attendee-row.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalAttendeeRow, context_menu);
  gtk_widget_class_bind_template_child (widget_class, GcalAttendeeRow, gesture_click);
  gtk_widget_class_bind_template_child (widget_class, GcalAttendeeRow, role);
}

static void
gcal_attendee_row_init (GcalAttendeeRow *instance)
{
  static const GActionEntry actions[] = {
    { "copy-email", on_copy_email_cb }
  };

  gtk_widget_init_template (GTK_WIDGET (instance));

  gtk_widget_set_parent (GTK_WIDGET (instance->context_menu), GTK_WIDGET (instance));

  instance->action_group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (instance->action_group),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   instance);

  gtk_widget_insert_action_group (instance->context_menu,
                                  "attendee-row",
                                  G_ACTION_GROUP (instance->action_group));

  g_signal_connect_swapped (instance->gesture_click, "pressed", G_CALLBACK (show_context_menu), instance);
}


/**
 * gcal_attendee_row_new:
 * @attendee: an instance of #GcalEventAttendee to display.
 *
 * Creates a new #GcalAttendeeRow widget.
 *
 * Returns: a new #GcalAttendeeRow widget instance.
 */
GtkWidget *
gcal_attendee_row_new (GcalEventAttendee *attendee)
{
  g_return_val_if_fail (GCAL_IS_EVENT_ATTENDEE (attendee), NULL);

  return g_object_new (GCAL_TYPE_ATTENDEE_ROW,
                       "attendee", attendee,
                       NULL);
}

/**
 * gcal_attendee_row_get_attendee:
 * @self: a #GcalAttendeeRow
 *
 * Returns the internally stored reference to a #GcalEventAttendee.
 *
 * Returns: (transfer none) (nullable): the contained #GcalEventAttendee.
 */
GcalEventAttendee *
gcal_attendee_row_get_attendee (GcalAttendeeRow *self)
{
  g_return_val_if_fail (GCAL_IS_ATTENDEE_ROW (self), NULL);

  return self->attendee;
}
