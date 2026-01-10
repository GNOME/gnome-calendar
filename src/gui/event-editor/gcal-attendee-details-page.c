/* gcal-attendee-details-page.c
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

#include "gcal-attendee-details-page.h"
#include "gcal-attendee-row.h"
#include "gcal-enum-types.h"
#include "gcal-enums.h"
#include "gcal-event-attendee.h"

#include <adwaita.h>
#include <gio/gio.h>
#include <glib-object.h>
#include <gtk/gtk.h>

struct _GcalAttendeeDetailsPage
{
  AdwNavigationPage parent_instance;

  GListStore               *attendees;

  GtkFilterListModel       *base_filtered_list;
  GtkFilterListModel       *filter_list_accepted;
  GtkFilterListModel       *filter_list_tentative;
  GtkFilterListModel       *filter_list_declined;
  GtkFilterListModel       *filter_list_delegated;
  GtkFilterListModel       *filter_list_not_responded;

  GtkCustomFilter          *base_filter;
  GtkCustomFilter          *accepted_filter;
  GtkCustomFilter          *tentative_filter;
  GtkCustomFilter          *declined_filter;
  GtkCustomFilter          *delegated_filter;
  GtkCustomFilter          *not_responded_filter;

  GtkWidget                *list_accepted;
  GtkWidget                *list_tentative;
  GtkWidget                *list_declined;
  GtkWidget                *list_delegated;
  GtkWidget                *list_not_responded;

  GcalEventAttendeeTypeFilterFlags attendee_type_filter_flags;
};

enum
{
  PROP_MODEL_ACCEPTED = 1,
  PROP_MODEL_TENTATIVE,
  PROP_MODEL_DECLINED,
  PROP_MODEL_DELEGATED,
  PROP_MODEL_NOT_RESPONDED,
  PROP_ATTENDEE_TYPE,
  N_PROPS
};

static GParamSpec *properties[N_PROPS] = { NULL, };

G_DEFINE_FINAL_TYPE (GcalAttendeeDetailsPage, gcal_attendee_details_page, ADW_TYPE_NAVIGATION_PAGE)

static void
set_type_filter_flags (GcalAttendeeDetailsPage         *self,
                       GcalEventAttendeeTypeFilterFlags flags)
{
  self->attendee_type_filter_flags = flags;
  gtk_filter_changed (GTK_FILTER (self->base_filter), GTK_FILTER_CHANGE_DIFFERENT);
}

static void
gcal_attendee_details_page_clear_list (GcalAttendeeDetailsPage *self)
{
  g_assert (GCAL_IS_ATTENDEE_DETAILS_PAGE (self));

  g_list_store_remove_all (self->attendees);
}

static GtkWidget *
create_attendee_row_widget (void *attendee,
                            void *user_data)
{
  return gcal_attendee_row_new (attendee);
}

static void
gcal_attendee_details_page_finalize (GObject *object)
{
  GcalAttendeeDetailsPage *page = (GcalAttendeeDetailsPage *) object;

  g_clear_pointer (&page->attendees, g_list_store_remove_all);

  g_clear_object (&page->base_filter);
  g_clear_object (&page->accepted_filter);
  g_clear_object (&page->tentative_filter);
  g_clear_object (&page->declined_filter);
  g_clear_object (&page->delegated_filter);
  g_clear_object (&page->not_responded_filter);

  /* g_clear_pointer on these would try to free self->attendees again */
  page->base_filtered_list = NULL;
  page->filter_list_accepted = NULL;
  page->filter_list_tentative = NULL;
  page->filter_list_declined = NULL;
  page->filter_list_delegated = NULL;
  page->filter_list_not_responded = NULL;
}

static gboolean
base_filter_cb (GcalEventAttendee *attendee,
                gpointer           data)
{
  GcalAttendeeDetailsPage *self = GCAL_ATTENDEE_DETAILS_PAGE (data);
  GcalEventAttendeeType type = gcal_event_attendee_get_attendee_type (attendee);

  switch (self->attendee_type_filter_flags)
  {
    case GCAL_EVENT_ATTENDEE_TYPE_FILTER_PERSON:
      return type == GCAL_EVENT_ATTENDEE_TYPE_INDIVIDUAL || type == GCAL_EVENT_ATTENDEE_TYPE_GROUP;
    case GCAL_EVENT_ATTENDEE_TYPE_FILTER_RESOURCE:
      return type == GCAL_EVENT_ATTENDEE_TYPE_RESOURCE || type == GCAL_EVENT_ATTENDEE_TYPE_ROOM;
    case GCAL_EVENT_ATTENDEE_TYPE_FILTER_NONE:
    default:
      return TRUE;
  }
}

static gboolean
custom_accepted_filter_cb (GcalEventAttendee *attendee,
                           gpointer           data)
{
  return gcal_event_attendee_get_part_status (attendee) == GCAL_EVENT_ATTENDEE_PART_ACCEPTED;
}

static gboolean
custom_tentative_filter_cb (GcalEventAttendee *attendee,
                            gpointer           data)
{
  return gcal_event_attendee_get_part_status (attendee) == GCAL_EVENT_ATTENDEE_PART_TENTATIVE;
}

static gboolean
custom_declined_filter_cb (GcalEventAttendee *attendee,
                           gpointer           data)
{
  return gcal_event_attendee_get_part_status (attendee) == GCAL_EVENT_ATTENDEE_PART_DECLINED;
}

static gboolean
custom_delegated_filter_cb (GcalEventAttendee *attendee,
                            gpointer           data)
{
  return gcal_event_attendee_get_part_status (attendee) == GCAL_EVENT_ATTENDEE_PART_DELEGATED;
}

static gboolean
custom_not_responded_filter_cb (GcalEventAttendee *attendee,
                                gpointer           data)
{
  switch (gcal_event_attendee_get_part_status (attendee))
    {
    case GCAL_EVENT_ATTENDEE_PART_NONE:
    case GCAL_EVENT_ATTENDEE_PART_NEEDS_ACTION:
      return TRUE;
    default:
      return FALSE;
    }
}

static void
gcal_attendee_details_page_init_filters (GcalAttendeeDetailsPage *self)
{
  self->base_filter = gtk_custom_filter_new ((GtkCustomFilterFunc) base_filter_cb, self, NULL);
  self->accepted_filter = gtk_custom_filter_new ((GtkCustomFilterFunc) custom_accepted_filter_cb, NULL, NULL);
  self->tentative_filter = gtk_custom_filter_new ((GtkCustomFilterFunc) custom_tentative_filter_cb, NULL, NULL);
  self->declined_filter = gtk_custom_filter_new ((GtkCustomFilterFunc) custom_declined_filter_cb, NULL, NULL);
  self->delegated_filter = gtk_custom_filter_new ((GtkCustomFilterFunc) custom_delegated_filter_cb, NULL, NULL);
  self->not_responded_filter = gtk_custom_filter_new ((GtkCustomFilterFunc) custom_not_responded_filter_cb, NULL, NULL);
}

static void
gcal_attendee_details_page_init_filter_models (GcalAttendeeDetailsPage *self)
{
  self->base_filtered_list =
    gtk_filter_list_model_new (G_LIST_MODEL (self->attendees),
                               GTK_FILTER (self->base_filter));

  self->filter_list_accepted =
    gtk_filter_list_model_new (G_LIST_MODEL (self->base_filtered_list),
                               GTK_FILTER (self->accepted_filter));

  self->filter_list_tentative =
    gtk_filter_list_model_new (G_LIST_MODEL (self->base_filtered_list),
                               GTK_FILTER (self->tentative_filter));

  self->filter_list_declined =
    gtk_filter_list_model_new (G_LIST_MODEL (self->base_filtered_list),
                               GTK_FILTER (self->declined_filter));

  self->filter_list_delegated =
    gtk_filter_list_model_new (G_LIST_MODEL (self->base_filtered_list),
                               GTK_FILTER (self->delegated_filter));

  self->filter_list_not_responded =
    gtk_filter_list_model_new (G_LIST_MODEL (self->base_filtered_list),
                               GTK_FILTER (self->not_responded_filter));
}

static void
gcal_attendee_details_page_get_property (GObject    *object,
                                         guint       property_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  GcalAttendeeDetailsPage *self = GCAL_ATTENDEE_DETAILS_PAGE (object);

  switch (property_id)
    {
    case PROP_MODEL_ACCEPTED:
      g_value_set_object (value, self->filter_list_accepted);
      break;
    case PROP_MODEL_TENTATIVE:
      g_value_set_object (value, self->filter_list_tentative);
      break;
    case PROP_MODEL_DECLINED:
      g_value_set_object (value, self->filter_list_declined);
      break;
    case PROP_MODEL_DELEGATED:
      g_value_set_object (value, self->filter_list_delegated);
      break;
    case PROP_MODEL_NOT_RESPONDED:
      g_value_set_object (value, self->filter_list_not_responded);
      break;
    case PROP_ATTENDEE_TYPE:
      g_value_set_flags (value, self->attendee_type_filter_flags);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
gcal_attendee_details_page_set_property (GObject      *object,
                                         guint         property_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  GcalAttendeeDetailsPage *self = GCAL_ATTENDEE_DETAILS_PAGE (object);

  switch (property_id)
    {
    case PROP_ATTENDEE_TYPE:
      set_type_filter_flags (self, g_value_get_flags (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
gcal_attendee_details_page_init (GcalAttendeeDetailsPage *instance)
{
  g_assert (GCAL_IS_ATTENDEE_DETAILS_PAGE (instance));

  instance->attendee_type_filter_flags = GCAL_EVENT_ATTENDEE_TYPE_FILTER_NONE;

  instance->attendees = g_list_store_new (GCAL_TYPE_EVENT_ATTENDEE);
  gcal_attendee_details_page_init_filters (instance);
  gcal_attendee_details_page_init_filter_models (instance);

  gtk_widget_init_template (GTK_WIDGET (instance));

  gtk_list_box_bind_model (GTK_LIST_BOX (instance->list_accepted),
                           G_LIST_MODEL (instance->filter_list_accepted),
                           create_attendee_row_widget,
                           NULL, NULL);

  gtk_list_box_bind_model (GTK_LIST_BOX (instance->list_tentative),
                           G_LIST_MODEL (instance->filter_list_tentative),
                           create_attendee_row_widget,
                           NULL, NULL);

  gtk_list_box_bind_model (GTK_LIST_BOX (instance->list_declined),
                           G_LIST_MODEL (instance->filter_list_declined),
                           create_attendee_row_widget,
                           NULL, NULL);

  gtk_list_box_bind_model (GTK_LIST_BOX (instance->list_delegated),
                           G_LIST_MODEL (instance->filter_list_delegated),
                           create_attendee_row_widget,
                           NULL, NULL);

  gtk_list_box_bind_model (GTK_LIST_BOX (instance->list_not_responded),
                           G_LIST_MODEL (instance->filter_list_not_responded),
                           create_attendee_row_widget,
                           NULL, NULL);
}

static void
gcal_attendee_details_page_class_init (GcalAttendeeDetailsPageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  /* object */

  object_class->finalize = gcal_attendee_details_page_finalize;
  object_class->get_property = gcal_attendee_details_page_get_property;
  object_class->set_property = gcal_attendee_details_page_set_property;

  /**
   * GcalAttendeeDetailsPage:model-accepted:
   *
   * Binding model for the list of attendees with "accepted" response.
   */
  properties[PROP_MODEL_ACCEPTED] =
    g_param_spec_object ("model-accepted", NULL, NULL,
                         GTK_TYPE_FILTER_LIST_MODEL, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GcalAttendeeDetailsPage:model-tentative:
   *
   * Binding model for the list of attendees with "tentative" (aka maybe) response.
   */
  properties[PROP_MODEL_TENTATIVE] =
    g_param_spec_object ("model-tentative", NULL, NULL,
                         GTK_TYPE_FILTER_LIST_MODEL, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GcalAttendeeDetailsPage:model-declined:
   *
   * Binding model for the list of attendees with "declined" response.
   */
  properties[PROP_MODEL_DECLINED] =
    g_param_spec_object ("model-declined", NULL, NULL,
                         GTK_TYPE_FILTER_LIST_MODEL, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GcalAttendeeDetailsPage:model-delegated:
   *
   * Binding model for the list of attendees with "delegated" response.
   * Lists the attendees who delegated their attendance to someone else.
   */
  properties[PROP_MODEL_DELEGATED] =
    g_param_spec_object ("model-delegated", NULL, NULL,
                         GTK_TYPE_FILTER_LIST_MODEL, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GcalAttendeeDetailsPage:model-not-responded:
   *
   * Binding model for the list of attendees who haven't responded yet
   * or thei response type is unknown.
   */
  properties[PROP_MODEL_NOT_RESPONDED] =
    g_param_spec_object ("model-not-responded", NULL, NULL,
                         GTK_TYPE_FILTER_LIST_MODEL, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GcalAttendeeDetailsPage:attendee-type:
   *
   * Used for the base filter to filter the attendees based on
   * attendee type (aka CUTYPE).
   * GCAL_EVENT_ATTENDEE_TYPE_NONE is used to clear the filter.
   */
  properties[PROP_ATTENDEE_TYPE] =
    g_param_spec_flags ("attendee-type-filter-flags", NULL, NULL,
                       GCAL_TYPE_EVENT_ATTENDEE_TYPE_FILTER_FLAGS,
                       GCAL_EVENT_ATTENDEE_TYPE_FILTER_NONE,
                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  /* widget */

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/event-editor/gcal-attendee-details-page.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalAttendeeDetailsPage, list_accepted);
  gtk_widget_class_bind_template_child (widget_class, GcalAttendeeDetailsPage, list_tentative);
  gtk_widget_class_bind_template_child (widget_class, GcalAttendeeDetailsPage, list_declined);
  gtk_widget_class_bind_template_child (widget_class, GcalAttendeeDetailsPage, list_delegated);
  gtk_widget_class_bind_template_child (widget_class, GcalAttendeeDetailsPage, list_not_responded);
}

/**
 * gcal_attendee_details_page_set_attendees:
 * @self: a #GcalAttendeeDetailsPage instance.
 * @attendees: a list of #GcalEventAttendee to display.
 *
 * Sets the attendees for display.
 */
void
gcal_attendee_details_page_set_attendees (GcalAttendeeDetailsPage *self,
                                          GSList                  *attendees)
{
  g_assert (GCAL_IS_ATTENDEE_DETAILS_PAGE (self));

  gcal_attendee_details_page_clear_list (self);

  g_autoptr (GSList) iter = NULL;

  for (iter = attendees; iter != NULL; iter = iter->next)
    {
      g_list_store_append (self->attendees, iter->data);
    }
}

/**
 * gcal_attendee_details_page_set_type_filter:
 * @self: a #GcalAttendeeDetailsPage instance.
 * @flags: flags of type #GcalEventAttendeeTypeFilterFlags.
 *
 * Sets the type of attendees to use as the base filter on the
 * list of attendees. E.g. only people or only resources, or both
 * are filtered before grouping by response is performed.
 */
void
gcal_attendee_details_page_set_type_filter (GcalAttendeeDetailsPage         *self,
                                            GcalEventAttendeeTypeFilterFlags flags)
{
  g_assert (GCAL_IS_ATTENDEE_DETAILS_PAGE (self));
  set_type_filter_flags (self, flags);
}
