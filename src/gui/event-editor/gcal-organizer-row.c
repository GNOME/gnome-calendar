/* gcal-organizer-row.c
 *
 * Copyright (C) 2026 Alen Galinec <mind.on.warp@gmail.com>
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

#include "gcal-organizer-row.h"

#include <adwaita.h>
#include <gio/gio.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#include "gcal-event-organizer.h"
#include "gcal-utils.h"

struct _GcalOrganizerRow
{
  AdwActionRow parent_instance;

  GcalEventOrganizer *organizer;

  GtkWidget          *context_menu;
  GtkGestureClick    *gesture_click;

  GSimpleActionGroup *action_group;
};

G_DEFINE_FINAL_TYPE (GcalOrganizerRow, gcal_organizer_row, ADW_TYPE_ACTION_ROW)

enum
{
  PROP_0,
  PROP_ORGANIZER,
  N_PROPS,
};

static GParamSpec *properties[N_PROPS] = { NULL, };

static void
on_copy_email_cb (GSimpleAction *action,
                  GVariant      *param,
                  gpointer       user_data)
{
  GcalOrganizerRow *self = GCAL_ORGANIZER_ROW (user_data);

  GdkDisplay *display = gtk_widget_get_display (GTK_WIDGET (self));
  GdkClipboard *clipboard = gdk_display_get_clipboard (display);

  g_autofree const gchar *email =
    gcal_get_email_from_mailto_uri (gcal_event_organizer_get_uri (self->organizer));

  gdk_clipboard_set_text (clipboard, email);
}

static void
show_context_menu (GcalOrganizerRow *self,
                   guint             npress,
                   double            x,
                   double            y)
{
  g_assert (GCAL_IS_ORGANIZER_ROW (self));

  const GdkRectangle position = { .x = x, .y = y };
  gtk_popover_set_pointing_to (GTK_POPOVER (self->context_menu), &position);
  gtk_popover_popup (GTK_POPOVER (self->context_menu));
}

static gchar*
generate_subtitle (GcalOrganizerRow   *self,
                   GcalEventOrganizer *organizer)
{
  g_autofree const gchar *email = NULL;
  g_autofree const gchar *name_with_email = g_strdup_printf ("%s (%s)", gcal_event_organizer_get_name (organizer), email);

  if (organizer == NULL)
    return NULL;

  email = gcal_get_email_from_mailto_uri (gcal_event_organizer_get_uri (organizer));

  return g_strdup (name_with_email);
}


static void
gcal_organizer_row_dispose (GObject *object)
{
  GcalOrganizerRow *self = GCAL_ORGANIZER_ROW (object);

  g_clear_pointer (&self->context_menu, gtk_widget_unparent);

  G_OBJECT_CLASS (gcal_organizer_row_parent_class)->dispose (object);
}

static void
gcal_organizer_row_finalize (GObject *object)
{
  GcalOrganizerRow *self = GCAL_ORGANIZER_ROW (object);

  g_clear_object (&self->gesture_click);
  g_clear_object (&self->action_group);

  G_OBJECT_CLASS (gcal_organizer_row_parent_class)->finalize (object);
}

static void
gcal_organizer_row_set_property (GObject      *object,
                                 guint         property_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GcalOrganizerRow *self = GCAL_ORGANIZER_ROW (object);

  switch (property_id)
    {
    case PROP_ORGANIZER:
      self->organizer = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
gcal_organizer_row_get_property (GObject    *object,
                                 guint       property_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GcalOrganizerRow *self = GCAL_ORGANIZER_ROW (object);

  switch (property_id)
    {
    case PROP_ORGANIZER:
      g_value_set_object (value, self->organizer);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
gcal_organizer_row_init (GcalOrganizerRow *self)
{
  static const GActionEntry actions[] = {
    { "copy-email", on_copy_email_cb }
  };

  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_widget_set_parent (GTK_WIDGET (self->context_menu), GTK_WIDGET (self));

  self->action_group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (self->action_group),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   self);

  gtk_widget_insert_action_group (self->context_menu,
                                  "organizer-row",
                                  G_ACTION_GROUP (self->action_group));

  g_signal_connect_swapped (self->gesture_click, "pressed", G_CALLBACK (show_context_menu), self);
}

static void
gcal_organizer_row_class_init (GcalOrganizerRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gcal_organizer_row_dispose;
  object_class->finalize = gcal_organizer_row_finalize;
  object_class->set_property = gcal_organizer_row_set_property;
  object_class->get_property = gcal_organizer_row_get_property;

  /**
   * GcalOrganizerRow:organizer
   *
   * The #GcalEventOrganizer of the event for which the row is displayed.
   */
  properties[PROP_ORGANIZER] =
    g_param_spec_object ("organizer", NULL, NULL,
                         GCAL_TYPE_EVENT_ORGANIZER,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/event-editor/gcal-organizer-row.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalOrganizerRow, context_menu);
  gtk_widget_class_bind_template_child (widget_class, GcalOrganizerRow, gesture_click);

  gtk_widget_class_bind_template_callback (widget_class, generate_subtitle);
}
