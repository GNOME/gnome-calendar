/* gcal-thread-utils.c
 *
 * Copyright 2019 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#include "gcal-thread-utils.h"

/* Code kindly provided by Milan Crha */

typedef struct {
  EThreadJobFunc      func;
  gpointer            user_data;
  GDestroyNotify      free_user_data;

  GCancellable       *cancellable;
  GError             *error;
} ThreadJobData;

static void
thread_job_data_free (gpointer ptr)
{
  ThreadJobData *tjd = ptr;

  if (tjd != NULL)
    {
      /* This should go to UI with more info/description,
       * if it is not G_IOI_ERROR_CANCELLED */
      if (tjd->error != NULL && !g_error_matches (tjd->error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Job failed: %s\n", tjd->error->message);

      if (tjd->free_user_data != NULL)
        tjd->free_user_data (tjd->user_data);

      g_clear_object (&tjd->cancellable);
      g_clear_error (&tjd->error);
      g_free (tjd);
    }
}

static gpointer
thread_job_thread (gpointer user_data)
{
  ThreadJobData *tjd = user_data;

  g_return_val_if_fail (tjd != NULL, NULL);

  if (tjd->func != NULL)
    tjd->func (tjd->user_data, tjd->cancellable, &tjd->error);

  thread_job_data_free (tjd);

  return g_thread_self ();
}

GCancellable*
gcal_thread_submit_job (EThreadJobFunc func,
                        gpointer       user_data,
                        GDestroyNotify free_user_data)
{
  ThreadJobData *tjd;
  GThread *thread;
  GCancellable *cancellable;

  cancellable = g_cancellable_new ();

  tjd = g_new0 (ThreadJobData, 1);
  tjd->func = func;
  tjd->user_data = user_data;
  tjd->free_user_data = free_user_data;
  /* user should be able to cancel this cancellable somehow */
  tjd->cancellable = g_object_ref (cancellable);
  tjd->error = NULL;

  thread = g_thread_try_new (NULL, thread_job_thread, tjd, &tjd->error);
  if (thread != NULL)
    {
      g_thread_unref (thread);
    }
  else
    {
      thread_job_data_free (tjd);
      g_clear_object (&cancellable);
    }

  return cancellable;
}
