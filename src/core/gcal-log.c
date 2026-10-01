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

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif

#ifdef __linux__
# include <sys/types.h>
# include <sys/syscall.h>
#endif

#include "gcal-debug.h"
#include "gcal-log.h"

#include <unistd.h>
#include <glib.h>

G_LOCK_DEFINE_STATIC (channel_lock);

GIOChannel *standard_channel = NULL;
static int log_verbosity = 0;

static const gchar* ignored_domains[] =
{
  "GdkPixbuf",
  "GWeather",
  NULL
};

static inline gint
get_thread_id (void)
{
#ifdef __linux__
  return (gint) syscall (SYS_gettid);
#else
  return GPOINTER_TO_INT (g_thread_self ());
#endif /* __linux__ */
}

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
  gint64 now;
  struct tm tt;
  time_t t;
  const gchar *level;
  gchar ftime[32];
  gchar *buffer;

  /* Skip ignored log domains */
  if (domain && g_strv_contains (ignored_domains, domain))
    return;

  switch ((int)log_level)
    {
    case G_LOG_LEVEL_MESSAGE:
      if (log_verbosity < 1)
        return;
      break;

    case G_LOG_LEVEL_INFO:
      if (log_verbosity < 2)
        return;
      break;

    case G_LOG_LEVEL_DEBUG:
      if (log_verbosity < 3)
        return;
      break;

    case GCAL_LOG_LEVEL_TRACE:
      if (log_verbosity < 4)
        return;
      break;

    default:
      break;
    }

  level = log_level_str (log_level);
  now = g_get_real_time ();
  t = now / G_USEC_PER_SEC;
  tt = *localtime (&t);
  strftime (ftime, sizeof (ftime), "%H:%M:%S", &tt);
  buffer = g_strdup_printf ("%s.%04d\t%30s[%5d]: %s: %s\n",
                            ftime,
                            (gint)((now % G_USEC_PER_SEC) / 100L),
                            domain,
                            get_thread_id (),
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
      const char *messages_debug = g_getenv ("G_MESSAGES_DEBUG");

      standard_channel = g_io_channel_unix_new (STDOUT_FILENO);

      g_log_set_default_handler (gcal_log_handler, NULL);

      /* Assume tracing if G_MESSAGES_DEBUG=all */
      if (g_strcmp0 (messages_debug, "all") == 0)
        log_verbosity = 4;

      g_once_init_leave (&initialized, TRUE);
    }
}

void
gcal_log_increase_verbosity (void)
{
  log_verbosity++;
}
