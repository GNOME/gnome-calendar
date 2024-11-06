/* gcal-importer.c
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

#include "gcal-importer.h"
#include "gcal-debug.h"

#include <glib/gi18n.h>

G_DEFINE_QUARK (ICalErrorEnum, i_cal_error);

static const gchar*
i_cal_error_enum_to_string (ICalErrorEnum ical_error)
{
  switch (ical_error)
    {
    case I_CAL_NO_ERROR:
      return _("No error");

    case I_CAL_BADARG_ERROR:
      return _("Bad argument to function");

    case I_CAL_NEWFAILED_ERROR:
    case I_CAL_ALLOCATION_ERROR:
      return _("Failed to allocate a new object in memory");

    case I_CAL_MALFORMEDDATA_ERROR:
      return _("File is malformed, invalid, or corrupted");

    case I_CAL_PARSE_ERROR:
      return _("Failed to parse the calendar contents");

    case I_CAL_FILE_ERROR:
      return _("Failed to read file");

    case I_CAL_INTERNAL_ERROR:
    case I_CAL_USAGE_ERROR:
    case I_CAL_UNIMPLEMENTED_ERROR:
    case I_CAL_UNKNOWN_ERROR:
    default:
      return _("Internal error");
    }
}

static const gchar*
guess_file_encoding (const gchar *contents,
                     gsize        length)
{
  if (length > 4 && contents[0] == '\xFF' && contents[1] == '\xFE' && contents[2] == '\x00' && contents[3] == '\x00') /* UTF-32LE case */
    return "UTF-32LE";
  else if (length > 4 && contents[0] == '\x00' && contents[1] == '\x00' && contents[2] == '\xFE' && contents[3] == '\xFF') /* UTF-32BE case */
    return "UTF-32BE";
  else if (length > 2 && contents[0] == '\xFF' && contents[1] == '\xFE') /* UTF-16LE case */
    return "UTF-16LE";
  else if (length > 2 && contents[0] == '\xFE' && contents[1] == '\xFF') /* UTF-16BE case */
    return "UTF-16BE";

  return "UTF-8";
}

static void
read_file_in_thread (GTask        *task,
                     gpointer      source_object,
                     gpointer      task_data,
                     GCancellable *cancellable)
{
  g_autoptr (GFileInfo) file_info = NULL;
  g_autoptr (GError) error = NULL;
  g_autofree gchar *contents = NULL;
  g_autofree gchar *path = NULL;
  const gchar *encoding = NULL;
  ICalComponent *component;
  ICalErrorEnum ical_error;
  gsize length;
  GFile *file;

  file = task_data;
  file_info = g_file_query_info (file,
                                 G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                                 G_FILE_QUERY_INFO_NONE,
                                 cancellable,
                                 &error);

  if (error)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  if (g_strcmp0 (g_file_info_get_content_type (file_info), "text/calendar") != 0)
    {
      g_task_return_new_error (task,
                               G_FILE_ERROR,
                               G_FILE_ERROR_FAILED,
                               "%s",
                               _("File is not an iCalendar (.ics) file"));
      return;
    }

  path = g_file_get_path (file);
  g_file_get_contents (path, &contents, &length, &error);

  if (error)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  encoding = guess_file_encoding (contents, length);

  if (g_strcmp0 (encoding, "UTF-8") != 0)
    {
      g_autofree gchar *content_converted = NULL;
      gsize new_length;

      GCAL_TRACE_MSG ("Converting file from %s to UTF-8", encoding);

      content_converted = g_convert (contents, length, "UTF-8", encoding, NULL, &new_length, &error);

      if (error)
        {
          g_task_return_error (task, g_steal_pointer (&error));
          return;
        }

      g_clear_pointer (&contents, g_free);
      contents = g_steal_pointer (&content_converted);
      length = new_length;
    }

  g_assert (g_utf8_validate (contents, length, NULL));

  component = i_cal_parser_parse_string (contents);
  ical_error = i_cal_errno_return ();

  if (ical_error != I_CAL_NO_ERROR)
    {
      g_task_return_new_error (task,
                               I_CAL_ERROR,
                               ical_error,
                               "%s",
                               i_cal_error_enum_to_string (ical_error));
      return;
    }

  if (!component)
    {
      g_task_return_new_error (task,
                               I_CAL_ERROR,
                               I_CAL_MALFORMEDDATA_ERROR,
                               "%s",
                               i_cal_error_enum_to_string (I_CAL_MALFORMEDDATA_ERROR));
      return;
    }

  g_task_return_pointer (task, g_object_ref (component), g_object_unref);
}

/**
 * gcal_importer_import_file:
 * @file: a #GFile
 * @cancellable: (nullable): a #GCancellable
 * @callback: a #GAsyncReadyCallback to execute upon completion
 * @user_data: closure data for @callback
 *
 * Import an ICS file.
 */
void
gcal_importer_import_file (GFile               *file,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{

  g_autoptr (GTask) task = NULL;

  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (NULL, cancellable, callback, user_data);
  g_task_set_task_data (task, g_object_ref (file), g_object_unref);
  g_task_set_source_tag (task, gcal_importer_import_file);
  g_task_run_in_thread (task, read_file_in_thread);
}

/**
 * gcal_importer_do_something_finish:
 * @result: a #GAsyncResult provided to callback
 * @error: a location for a #GError, or %NULL
 *
 * Returns: (nullable): an #ICalComponent
 */
ICalComponent*
gcal_importer_import_file_finish (GAsyncResult  *result,
                                  GError       **error)
{
  g_return_val_if_fail (g_task_is_valid (result, NULL), FALSE);

  return g_task_propagate_pointer (G_TASK (result), error);
}
