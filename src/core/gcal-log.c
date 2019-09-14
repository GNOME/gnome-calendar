/* gcal-log.c
 *
 * Copyright (C) 2017 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#include "gcal-debug.h"
#include "gcal-log.h"

#include <unistd.h>
#include <glib.h>

G_LOCK_DEFINE_STATIC (channel_lock);

GIOChannel *standard_channel = NULL;

static const gchar* ignored_domains[] =
{
  "GdkPixbuf",
  NULL
};

static const gchar *
log_level_str (GLogLevelFlags log_level)
{
  switch (((gulong)log_level & G_LOG_LEVEL_MASK))
    {
    case G_LOG_LEVEL_ERROR:    return "   \033[1;31mERROR\033[0m";
    case G_LOG_LEVEL_CRITICAL: return "\033[1;35mCRITICAL\033[0m";
    case G_LOG_LEVEL_WARNING:  return " \033[1;33mWARNING\033[0m";
    case G_LOG_LEVEL_MESSAGE:  return " \033[1;34mMESSAGE\033[0m";
    case G_LOG_LEVEL_INFO:     return "    \033[1;32mINFO\033[0m";
    case G_LOG_LEVEL_DEBUG:    return "   \033[1;32mDEBUG\033[0m";
    case GCAL_LOG_LEVEL_TRACE: return "   \033[1;36mTRACE\033[0m";

    default:
      return " UNKNOWN";
    }
}

static void
gcal_log_handler (const gchar    *domain,
                  GLogLevelFlags  log_level,
                  const gchar    *message,
                  gpointer        user_data)
{
  g_autoptr (GDateTime) now = NULL;
  g_autofree gchar *buffer = NULL;
  g_autofree gchar *ftime = NULL;
  const gchar *level;
  gint microsecond;

  /* Skip ignored log domains */
  if (domain && g_strv_contains (ignored_domains, domain))
    return;

  level = log_level_str (log_level);
  now = g_date_time_new_now_local ();
  ftime = g_date_time_format (now, "%H:%M:%S");
  microsecond = g_date_time_get_microsecond (now);
  buffer = g_strdup_printf ("%s.%04d  %28s: %s: %s\n",
                            ftime,
                            microsecond,
                            domain,
                            level,
                            message);

  /* Safely write to the channel */
  G_LOCK (channel_lock);

  g_io_channel_write_chars (standard_channel, buffer, -1, NULL, NULL);
  g_io_channel_flush (standard_channel, NULL);

  G_UNLOCK (channel_lock);
}

void
gcal_log_init (void)
{
  static gsize initialized = FALSE;

  if (g_once_init_enter (&initialized))
    {
      standard_channel = g_io_channel_unix_new (STDOUT_FILENO);

      g_log_set_default_handler (gcal_log_handler, NULL);

      g_once_init_leave (&initialized, TRUE);
    }
}

