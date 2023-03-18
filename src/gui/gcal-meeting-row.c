/* gcal-meeting-row.c
 *
 * Copyright 2021 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#define G_LOG_DOMAIN "GcalMeetingRow"

#include "gcal-meeting-row.h"
#include "gcal-utils.h"

#include <glib/gi18n.h>

struct _GcalMeetingRow
{
  AdwActionRow        parent_instance;

  GtkWidget          *join_button;

  gchar              *url;
};

G_DEFINE_TYPE (GcalMeetingRow, gcal_meeting_row, ADW_TYPE_ACTION_ROW)

enum
{
  PROP_0,
  PROP_URL,
  N_PROPS
};

enum
{
  JOIN_MEETING,
  N_SIGNALS,
};

static guint signals[N_SIGNALS] = { 0, };
static GParamSpec *properties[N_PROPS] = { NULL, };


/*
 * Auxiliary methods
 */

const gchar*
get_service_name_from_url (const gchar *url)
{
  struct {
    const gchar *needle;
    const gchar *service_name;
  } service_name_vtable[] = {
    { "meet.google.com", N_("Google Meet") },
    { "meet.jit.si", N_("Jitsi") },
    { "whereby.com", N_("Whereby") },
    { "zoom.us", N_("Zoom") },
    { "teams.microsoft.com", N_("Microsoft Teams") },
  };
  gsize i;

  for (i = 0; i < G_N_ELEMENTS (service_name_vtable); i++)
    {
      if (strstr (url, service_name_vtable[i].needle))
        return gettext (service_name_vtable[i].service_name);
    }

  return _("Unknown Service");
}

static void
setup_meeting (GcalMeetingRow *self)
{
  g_autofree gchar *markup_url = NULL;

  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self), get_service_name_from_url (self->url));

  markup_url = g_strdup_printf ("<a href=\"%s\">%s</a>", self->url, self->url);
  adw_action_row_set_subtitle (ADW_ACTION_ROW (self), markup_url);
}


/*
 * Callbacks
 */

static void
on_join_button_clicked_cb (GtkButton      *button,
                           GcalMeetingRow *self)
{
  g_signal_emit (self, signals[JOIN_MEETING], 0, self->url);
}


static gboolean
gcal_meeting_row_grab_focus (GtkWidget *widget)
{
  GcalMeetingRow *self = (GcalMeetingRow *) widget;

  gtk_widget_grab_focus (self->join_button);

  return TRUE;
}


/*
 * GObject overrides
 */

static void
gcal_meeting_row_finalize (GObject *object)
{
  GcalMeetingRow *self = (GcalMeetingRow *)object;

  g_clear_pointer (&self->url, g_free);

  G_OBJECT_CLASS (gcal_meeting_row_parent_class)->finalize (object);
}

static void
gcal_meeting_row_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GcalMeetingRow *self = GCAL_MEETING_ROW (object);

  switch (prop_id)
    {
    case PROP_URL:
      g_value_set_string (value, self->url);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_meeting_row_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GcalMeetingRow *self = GCAL_MEETING_ROW (object);

  switch (prop_id)
    {
    case PROP_URL:
      g_assert (self->url == NULL);
      self->url = g_value_dup_string (value);
      setup_meeting (self);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_meeting_row_class_init (GcalMeetingRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gcal_meeting_row_finalize;
  object_class->get_property = gcal_meeting_row_get_property;
  object_class->set_property = gcal_meeting_row_set_property;

  widget_class->grab_focus = gcal_meeting_row_grab_focus;

  signals[JOIN_MEETING] = g_signal_new ("join-meeting",
                                        GCAL_TYPE_MEETING_ROW,
                                        G_SIGNAL_RUN_LAST,
                                        0, NULL, NULL, NULL,
                                        G_TYPE_NONE,
                                        1,
                                        G_TYPE_STRING);

  properties[PROP_URL] = g_param_spec_string ("url",
                                              "URL of the meeting",
                                              "URL of the meeting",
                                              NULL,
                                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/gui/gcal-meeting-row.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalMeetingRow, join_button);

  gtk_widget_class_bind_template_callback (widget_class, on_join_button_clicked_cb);

  gtk_widget_class_set_css_name (widget_class, "meetingrow");
}

static void
gcal_meeting_row_init (GcalMeetingRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget*
gcal_meeting_row_new (const gchar *url)
{
  return g_object_new (GCAL_TYPE_MEETING_ROW,
                       "url", url,
                       NULL);
}
