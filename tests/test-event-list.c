/*
 * test-event-list.c
 *
 * Copyright 2026 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#include <glib.h>

#include "gcal-event.h"
#include "gcal-event-list.h"
#include "gcal-stub-calendar.h"
#include "gcal-utils.h"

#define EVENT_STRING_FOR_DATE(dtstart,dtend)    \
                   "BEGIN:VEVENT\n"             \
                   "SUMMARY:Stub event\n"       \
                   "UID:example@uid\n"          \
                   "DTSTAMP:19970114T170000Z\n" \
                   "DTSTART"dtstart"\n"        \
                   "DTEND"dtend"\n"            \
                   "END:VEVENT\n"

#define EVENT_STRING_FOR_DATE_START(dtstart)    \
                   EVENT_STRING_FOR_DATE(dtstart, ":20260715T035959Z")

#define EVENT_STRING_FOR_DATE_END(dtend)        \
                   EVENT_STRING_FOR_DATE(":19970101T170000Z", dtend)

/*
 * Auxiliary methods
 */

static GcalEvent*
create_event_for_string (const gchar  *string,
                         GError      **error)
{
  g_autoptr (ECalComponent) component = NULL;
  g_autoptr (GcalCalendar) calendar = NULL;

  component = e_cal_component_new_from_string (string);
  calendar = gcal_stub_calendar_new (NULL, error);

  return component ? gcal_event_new (calendar, component, error) : NULL;
}

/*********************************************************************************************************************/

static void
event_list_new (void)
{
  g_autoptr (GcalEventList) event_list = NULL;

  event_list = gcal_event_list_new ();
  g_assert_true (GCAL_IS_EVENT_LIST (event_list));
}

/*********************************************************************************************************************/

typedef struct {
  guint n_items_changed;
  guint position;
  guint added;
  guint removed;
} AddEventsHelper;

static void
items_changed_cb (GListModel *model,
                  guint       position,
                  guint       removed,
                  guint       added,
                  gpointer    user_data)
{
  AddEventsHelper *helper = user_data;

  helper->n_items_changed++;
  helper->position = position;
  helper->removed = removed;
  helper->added = added;
}

static void
event_list_add_events (void)
{
  g_autoptr (GcalEventList) event_list = NULL;
  g_autoptr (GPtrArray) events = NULL;
  AddEventsHelper helper = { };

  const gchar * const event_strings[] = {
    EVENT_STRING_FOR_DATE (":20260331T000000Z", ":20260331T030000Z"),
    EVENT_STRING_FOR_DATE (":20260401T000000Z", ":20260401T030000Z"),
    EVENT_STRING_FOR_DATE (":20260402T000000Z", ":20260403T030000Z"),
  };

  event_list = gcal_event_list_new ();
  g_assert_true (GCAL_IS_EVENT_LIST (event_list));

  events = g_ptr_array_new_null_terminated (G_N_ELEMENTS (event_strings), g_object_unref, TRUE);
  for (gsize i = 0; i < G_N_ELEMENTS (event_strings); i++)
    {
      g_autoptr (GcalEvent) event = NULL;
      g_autoptr (GError) error = NULL;

      event = create_event_for_string (event_strings[i], &error);

      g_assert_no_error (error);

      g_ptr_array_add (events, g_steal_pointer (&event));
    }

  g_signal_connect (event_list, "items-changed", G_CALLBACK (items_changed_cb), &helper);
  gcal_event_list_add_events (event_list, (GcalEvent **) events->pdata);

  g_assert_cmpuint (helper.n_items_changed, ==, 1);
  g_assert_cmpuint (helper.position, ==, 0);
  g_assert_cmpuint (helper.removed, ==, 0);
  g_assert_cmpuint (helper.added, ==, 3);
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (event_list)), ==, 3);

  for (guint i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (event_list)); i++)
    {
      g_autoptr (GcalEvent) event = g_list_model_get_item (G_LIST_MODEL (event_list), i);

      g_assert_true (event == g_ptr_array_index (events, i));
    }
}

/*********************************************************************************************************************/

typedef struct {
  guint n_items_changed;
  guint position;
  guint added;
  guint removed;
} RemoveEventsHelper;

static void
event_list_remove_events_items_changed_cb (GListModel *model,
                                           guint       position,
                                           guint       removed,
                                           guint       added,
                                           gpointer    user_data)
{
  RemoveEventsHelper *helper = user_data;

  helper->n_items_changed++;
  helper->position = position;
  helper->removed = removed;
  helper->added = added;
}

static void
event_list_remove_events (void)
{
  g_autoptr (GcalEventList) event_list = NULL;
  g_autoptr (GPtrArray) events = NULL;
  RemoveEventsHelper helper = { };

  const gchar * const event_strings[] = {
    EVENT_STRING_FOR_DATE (":20260331T000000Z", ":20260331T030000Z"),
    EVENT_STRING_FOR_DATE (":20260401T000000Z", ":20260401T030000Z"),
    EVENT_STRING_FOR_DATE (":20260402T000000Z", ":20260403T030000Z"),
  };

  event_list = gcal_event_list_new ();
  g_assert_true (GCAL_IS_EVENT_LIST (event_list));

  events = g_ptr_array_new_null_terminated (G_N_ELEMENTS (event_strings), g_object_unref, TRUE);
  for (gsize i = 0; i < G_N_ELEMENTS (event_strings); i++)
    {
      g_autoptr (GcalEvent) event = NULL;
      g_autoptr (GError) error = NULL;

      event = create_event_for_string (event_strings[i], &error);

      g_assert_no_error (error);

      g_ptr_array_add (events, g_steal_pointer (&event));
    }

  g_signal_connect (event_list,
                    "items-changed",
                    G_CALLBACK (event_list_remove_events_items_changed_cb),
                    &helper);

  gcal_event_list_add_events (event_list, (GcalEvent **) events->pdata);

  g_assert_cmpuint (helper.n_items_changed, ==, 1);
  g_assert_cmpuint (helper.position, ==, 0);
  g_assert_cmpuint (helper.removed, ==, 0);
  g_assert_cmpuint (helper.added, ==, 3);
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (event_list)), ==, 3);

  for (guint i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (event_list)); i++)
    {
      g_autoptr (GcalEvent) event = g_list_model_get_item (G_LIST_MODEL (event_list), i);

      g_assert_true (event == g_ptr_array_index (events, i));
    }

  helper = (RemoveEventsHelper) { };

  gcal_event_list_remove_events (event_list, (GcalEvent **) events->pdata);
  g_assert_cmpuint (helper.n_items_changed, ==, 1);
  g_assert_cmpuint (helper.position, ==, 0);
  g_assert_cmpuint (helper.removed, ==, 3);
  g_assert_cmpuint (helper.added, ==, 0);
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (event_list)), ==, 0);


  /* Remove again. These events are not in the list anymore. Nothing should happen */
  helper = (RemoveEventsHelper) { };

  gcal_event_list_remove_events (event_list, (GcalEvent **) events->pdata);
  g_assert_cmpuint (helper.n_items_changed, ==, 0);
  g_assert_cmpuint (helper.position, ==, 0);
  g_assert_cmpuint (helper.removed, ==, 0);
  g_assert_cmpuint (helper.added, ==, 0);
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (event_list)), ==, 0);
}

/*********************************************************************************************************************/

gint
main (gint   argc,
      gchar *argv[])
{
  g_setenv ("TZ", "UTC", TRUE);

  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/event-list/new", event_list_new);
  g_test_add_func ("/event-list/add-events", event_list_add_events);
  g_test_add_func ("/event-list/remove-events", event_list_remove_events);

  return g_test_run ();
}
