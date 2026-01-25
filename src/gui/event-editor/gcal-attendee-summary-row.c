/* gcal-attendee-summary-row.c
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

#include "config.h"
#include "gcal-attendee-summary-row.h"
#include "gcal-enum-types.h"
#include "gcal-enums.h"
#include "gcal-event-attendee.h"

#include <adwaita.h>
#include <gio/gio.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include <gtk/gtk.h>

struct _GcalAttendeeSummaryRow
{
  AdwActionRow parent_instance;

  GListStore         *attendees;
  GtkFilterListModel *filtered_attendees;
  GtkCustomFilter    *attendee_filter;

  /* participation summary */
  guint num_accepted;
  guint num_declined;
  guint num_tentative;
  guint num_not_responded;
  guint num_delegated;

  GcalEventAttendeeTypeFilterFlags filter_flags;
};

G_DEFINE_FINAL_TYPE (GcalAttendeeSummaryRow, gcal_attendee_summary_row, ADW_TYPE_ACTION_ROW)

enum
{
  PROP_ATTENDEES = 1,
  PROP_FILTER_FLAGS,
  PROP_FILTERED_LIST,
  PROP_NUM_ACCEPTED,
  PROP_NUM_DECLINED,
  PROP_NUM_TENTATIVE,
  PROP_NUM_DELEGATED,
  PROP_NUM_UNKNOWN,
  N_PROPS
};

static GParamSpec *properties[N_PROPS] = { NULL, };

/* Auxiliary methods */

static void
notify_summary_changed (GcalAttendeeSummaryRow *self)
{
  for (guint i = 1; i < N_PROPS; ++i)
    g_object_notify_by_pspec (G_OBJECT (self), properties[i]);
}

static gboolean
custom_filter_func (GcalEventAttendee      *attendee,
                    GcalAttendeeSummaryRow *self)
{
  GcalEventAttendeeType attendee_type = gcal_event_attendee_get_attendee_type (attendee);

  switch (self->filter_flags)
  {
    case GCAL_EVENT_ATTENDEE_TYPE_FILTER_PERSON:
      return attendee_type == GCAL_EVENT_ATTENDEE_TYPE_INDIVIDUAL || attendee_type == GCAL_EVENT_ATTENDEE_TYPE_GROUP;

    case GCAL_EVENT_ATTENDEE_TYPE_FILTER_RESOURCE:
      return attendee_type == GCAL_EVENT_ATTENDEE_TYPE_ROOM || attendee_type == GCAL_EVENT_ATTENDEE_TYPE_RESOURCE;

    default:
      return TRUE;
  }
}

static inline void
concat_subtitle_part (GStrvBuilder *builder,
                      guint         value,
                      const gchar  *format)
{
  if (value <= 0)
    return;

  g_strv_builder_take (builder, g_strdup_printf (format, value));
}

static const gchar*
build_summary_subtitle (GcalAttendeeSummaryRow *self)
{
  g_autoptr (GStrvBuilder) builder = g_strv_builder_new ();
  g_auto (GStrv) parts = NULL;

  concat_subtitle_part (builder, self->num_accepted, g_dngettext (GETTEXT_PACKAGE, "%1$u accepted", "%1$u accepted", self->num_accepted));
  concat_subtitle_part (builder, self->num_tentative, g_dngettext (GETTEXT_PACKAGE, "%1$u maybe", "%1$u maybe", self->num_tentative));
  concat_subtitle_part (builder, self->num_declined, g_dngettext (GETTEXT_PACKAGE, "%1$u declined", "%1$u declined", self->num_declined));
  concat_subtitle_part (builder, self->num_delegated, g_dngettext (GETTEXT_PACKAGE, "%1$u delegated", "%1$u delegated", self->num_delegated));
  concat_subtitle_part (builder, self->num_not_responded, g_dngettext (GETTEXT_PACKAGE, "%1$u not responded", "%1$u not responded", self->num_not_responded));

  parts = g_strv_builder_end (builder);

  return g_strjoinv (", ", parts);
}

static void
setup_filtered_attendees (GcalAttendeeSummaryRow *self,
                          GListModel             *attendees)
{
  g_return_if_fail (GCAL_IS_ATTENDEE_SUMMARY_ROW (self));

  g_list_store_remove_all (self->attendees);
  self->num_accepted = 0;
  self->num_declined = 0;
  self->num_delegated = 0;
  self->num_tentative = 0;
  self->num_not_responded = 0;

  if (attendees == NULL)
    return;

  guint n = g_list_model_get_n_items (attendees);
  for (uint i = 0; i < n; ++i)
    {
      GcalEventAttendee *attendee = g_list_model_get_item (attendees, i);
      g_list_store_append (self->attendees, attendee);

      /* this should be faster than going through self->filtered_attendees afterwards */
      if (!custom_filter_func (attendee, self))
        continue;

      switch (gcal_event_attendee_get_part_status (attendee))
        {
        case GCAL_EVENT_ATTENDEE_PART_ACCEPTED:
          self->num_accepted++;
          break;
        case GCAL_EVENT_ATTENDEE_PART_DECLINED:
          self->num_declined++;
          break;
        case GCAL_EVENT_ATTENDEE_PART_DELEGATED:
          self->num_delegated++;
          break;
        case GCAL_EVENT_ATTENDEE_PART_TENTATIVE:
          self->num_tentative++;
          break;
        default:
          self->num_not_responded++;
        }
    }

  g_autofree const gchar *subtitle = build_summary_subtitle (self);
  adw_action_row_set_subtitle (ADW_ACTION_ROW (self), subtitle);

  notify_summary_changed (self);
}

static void
change_attendee_filter_type (GcalAttendeeSummaryRow           *self,
                             GcalEventAttendeeTypeFilterFlags  value)
{
  self->filter_flags = value;
  gtk_filter_changed (GTK_FILTER (self->attendee_filter), GTK_FILTER_CHANGE_DIFFERENT);
  gtk_actionable_set_action_target (GTK_ACTIONABLE (self), "u", self->filter_flags);
}

static void
gcal_attendee_summary_row_set_property (GObject      *object,
                                        guint         property_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  GcalAttendeeSummaryRow *self = GCAL_ATTENDEE_SUMMARY_ROW (object);

  switch (property_id)
    {
    case PROP_ATTENDEES:
      setup_filtered_attendees (self, g_value_get_object (value));
      break;
    case PROP_FILTER_FLAGS:
      change_attendee_filter_type (self, g_value_get_flags (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
gcal_attendee_summary_row_get_property (GObject    *object,
                                        guint       property_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  GcalAttendeeSummaryRow *self = GCAL_ATTENDEE_SUMMARY_ROW (object);

  switch (property_id)
    {
    case PROP_FILTER_FLAGS:
      g_value_set_flags (value, self->filter_flags);
      break;
    case PROP_FILTERED_LIST:
      g_value_set_object (value, self->filtered_attendees);
      break;
    case PROP_NUM_ACCEPTED:
      g_value_set_uint (value, self->num_accepted);
      break;
    case PROP_NUM_DECLINED:
      g_value_set_uint (value, self->num_declined);
      break;
    case PROP_NUM_DELEGATED:
      g_value_set_uint (value, self->num_delegated);
      break;
    case PROP_NUM_TENTATIVE:
      g_value_set_uint (value, self->num_tentative);
      break;
    case PROP_NUM_UNKNOWN:
      g_value_set_uint (value, self->num_not_responded);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
gcal_attendee_summary_row_finalize (GObject *object)
{
  GcalAttendeeSummaryRow *self = (GcalAttendeeSummaryRow *) object;

  g_clear_pointer (&self->attendees, g_list_store_remove_all);
  g_clear_object (&self->attendee_filter);

  self->filtered_attendees = NULL;

  G_OBJECT_CLASS (gcal_attendee_summary_row_parent_class)->finalize (object);
}

static void
gcal_attendee_summary_row_class_init (GcalAttendeeSummaryRowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gcal_attendee_summary_row_finalize;
  object_class->set_property = gcal_attendee_summary_row_set_property;
  object_class->get_property = gcal_attendee_summary_row_get_property;

  /**
   * GcalAttendeeSummaryRow:attendees:
   *
   * The unfiltered list of attendees.
   */
  properties[PROP_ATTENDEES] =
    g_param_spec_object ("attendees", NULL, NULL,
                         G_TYPE_LIST_MODEL,
                         G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GcalAttendeeSummaryRow:filter-flags:
   *
   * The GCAL_TYPE_EVENT_ATTENDEE_TYPE_FILTER_FLAGS used to filter the attendees.
   */
  properties[PROP_FILTER_FLAGS] =
    g_param_spec_flags ("filter-flags", NULL, NULL,
                       GCAL_TYPE_EVENT_ATTENDEE_TYPE_FILTER_FLAGS, GCAL_EVENT_ATTENDEE_TYPE_FILTER_NONE,
                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * GcalAttendeeSummaryRow:filtered-list:
   *
   * The filtered list of attendees.
   * Filtered by #GcalAttendeeSummaryRow:filter-type
   */
  properties[PROP_FILTERED_LIST] =
    g_param_spec_object ("filtered-list", NULL, NULL,
                         GTK_TYPE_FILTER_LIST_MODEL,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GcalAttendeeSummaryRow:num-accepted:
   *
   * Gets the number of attendees who accepted.
   */
  properties[PROP_NUM_ACCEPTED] =
    g_param_spec_uint ("num-accepted", NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GcalAttendeeSummaryRow:num-declined:
   *
   * Gets the number of attendees who declined.
   */
  properties[PROP_NUM_DECLINED] =
    g_param_spec_uint ("num-declined", NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GcalAttendeeSummaryRow:num-tentative:
   *
   * Gets the number of attendees who "maybe" will attend.
   */
  properties[PROP_NUM_TENTATIVE] =
    g_param_spec_uint ("num-tentative", NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY  | G_PARAM_STATIC_STRINGS);

  /**
   * GcalAttendeeSummaryRow:num-unknown:
   *
   * Gets the number of attendees with an unknown response.
   */
  properties[PROP_NUM_UNKNOWN] =
    g_param_spec_uint ("num-unknown", NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY  | G_PARAM_STATIC_STRINGS);

  /**
   * GcalAttendeeSummaryRow:num-delegated:
   *
   * Gets the number of attendees who delegated their attendance to someone else.
   */
  properties[PROP_NUM_DELEGATED] =
    g_param_spec_uint ("num-delegated", NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY  | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/event-editor/gcal-attendee-summary-row.ui");
}

static void
gcal_attendee_summary_row_init (GcalAttendeeSummaryRow *instance)
{
  instance->attendees = g_list_store_new (GCAL_TYPE_EVENT_ATTENDEE);
  instance->attendee_filter = gtk_custom_filter_new ((GtkCustomFilterFunc) custom_filter_func, instance, NULL);
  instance->filtered_attendees = gtk_filter_list_model_new (G_LIST_MODEL (instance->attendees), GTK_FILTER (instance->attendee_filter));

  gtk_widget_init_template(GTK_WIDGET(instance));

  gtk_actionable_set_action_name (GTK_ACTIONABLE (instance), "event-editor.show-attendees-detail-page");
}

