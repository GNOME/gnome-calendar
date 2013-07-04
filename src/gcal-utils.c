/* -*- mode: c; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gcal-utils.c
 *
 * Copyright (C) 2012 - Erick Pérez Castellanos
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "gcal-utils.h"
#include "gcal-event-widget.h"
#include "gcal-view.h"

#include <libedataserver/libedataserver.h>

#include <glib/gi18n.h>

#include <langinfo.h>

#include <string.h>
#include <math.h>

static const gint
ab_day[7] =
{
  ABDAY_1,
  ABDAY_2,
  ABDAY_3,
  ABDAY_4,
  ABDAY_5,
  ABDAY_6,
  ABDAY_7,
};

static const gint
month_item[12] =
{
  MON_1,
  MON_2,
  MON_3,
  MON_4,
  MON_5,
  MON_6,
  MON_7,
  MON_8,
  MON_9,
  MON_10,
  MON_11,
  MON_12
};

G_DEFINE_BOXED_TYPE (icaltimetype, icaltime, gcal_dup_icaltime, g_free)

/* taken from eel/eel-gtk-extensions.c */
static gboolean
tree_view_button_press_callback (GtkWidget *tree_view,
         GdkEventButton *event,
         gpointer data)
{
  GtkTreePath *path;
  GtkTreeViewColumn *column;

  if (event->button == 1 && event->type == GDK_BUTTON_PRESS) {
    if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (tree_view),
             event->x, event->y,
             &path,
             &column,
             NULL,
             NULL)) {
      gtk_tree_view_row_activated
        (GTK_TREE_VIEW (tree_view), path, column);
    }
  }

  return FALSE;
}

void
gcal_gtk_tree_view_set_activate_on_single_click (GtkTreeView *tree_view,
                                                 gboolean should_activate)
{
  guint button_press_id;

  button_press_id = GPOINTER_TO_UINT
    (g_object_get_data (G_OBJECT (tree_view),
          "gd-tree-view-activate"));

  if (button_press_id && !should_activate) {
    g_signal_handler_disconnect (tree_view, button_press_id);
    g_object_set_data (G_OBJECT (tree_view),
         "gd-tree-view-activate",
         NULL);
  } else if (!button_press_id && should_activate) {
    button_press_id = g_signal_connect
      (tree_view,
       "button_press_event",
       G_CALLBACK  (tree_view_button_press_callback),
       NULL);
    g_object_set_data (G_OBJECT (tree_view),
         "gd-tree-view-activate",
         GUINT_TO_POINTER (button_press_id));
  }
}

const gchar*
gcal_get_group_name (const gchar *base_uri)
{
  if (g_strcmp0 (base_uri, "local:") == 0)
    return "Local";
  else
    return "External";
}

icaltimetype*
gcal_dup_icaltime (const icaltimetype *date)
{
  icaltimetype *new_date;

  new_date= g_new (icaltimetype, 1);
  new_date->year = date->year;
  new_date->month = date->month;
  new_date->day = date->day;
  new_date->hour = date->hour;
  new_date->minute = date->minute;
  new_date->second = date->second;
  new_date->is_utc = date->is_utc;
  new_date->is_date = date->is_date;
  new_date->is_daylight = date->is_daylight;
  new_date->zone = date->zone;

  return new_date;
}

/*
 * gcal_get_source_name:
 *
 * This method assume it receive only the model inside GcalManager
 * */
gchar*
gcal_get_source_name (GtkTreeModel *model,
                      const gchar  *uid)
{
  GtkTreeIter iter;
  gboolean valid;
  gchar *name;

  name = NULL;
  valid = gtk_tree_model_get_iter_first (model, &iter);
  while (valid)
    {
      gchar *name_data;
      gchar *uid_data;

      gtk_tree_model_get (model, &iter,
                          0, &uid_data,
                          1, &name_data,
                          -1);

      if (g_strcmp0 (uid_data, uid) == 0)
        {
          name = g_strdup (name_data);

          g_free (name_data);
          g_free (uid_data);
          break;
        }

      g_free (name_data);
      g_free (uid_data);

      valid = gtk_tree_model_iter_next (model, &iter);
    }
  return name;
}

gchar*
gcal_get_source_uid (GtkTreeModel *model,
                     const gchar  *name)
{
  GtkTreeIter iter;
  gboolean valid;
  gchar *uid;

  uid = NULL;
  valid = gtk_tree_model_get_iter_first (model, &iter);
  while (valid)
    {
      gchar *name_data;
      gchar *uid_data;

      gtk_tree_model_get (model, &iter,
                          0, &uid_data,
                          1, &name_data,
                          -1);

      if (g_strcmp0 (name_data, name) == 0)
        {
          uid = g_strdup (uid_data);

          g_free (name_data);
          g_free (uid_data);
          break;
        }

      g_free (name_data);
      g_free (uid_data);

      valid = gtk_tree_model_iter_next (model, &iter);
    }
  return uid;
}

gchar*
gcal_get_weekday (gint i)
{
  return nl_langinfo (ab_day[i]);
}

gchar*
gcal_get_month_name (gint i)
{
  return nl_langinfo (month_item[i]);
}

GdkPixbuf*
gcal_get_pixbuf_from_color (GdkColor *color,
                            gint      size)
{
  cairo_surface_t *surface;
  cairo_t *cr;
  GdkPixbuf *pix;

  /* TODO: review size here, maybe not hardcoded */
  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, size, size);
  cr = cairo_create (surface);

  cairo_set_source_rgb (cr,
                         color->red / 65535.0,
                         color->green / 65535.0,
                         color->blue / 65535.0);
  cairo_rectangle (cr, 0, 0, size, size);
  cairo_fill (cr);
  cairo_destroy (cr);
  pix = gdk_pixbuf_get_from_surface (surface,
                                     0, 0,
                                     size, size);
  cairo_surface_destroy (surface);
  return pix;
}

gint
gcal_compare_event_widget_by_date (gconstpointer a,
                                   gconstpointer b)
{
  /* negative value if a < b; zero if a = b; positive value if a > b. */
  GcalViewChild *a_child;
  GcalViewChild *b_child;
  icaltimetype *a_date;
  icaltimetype *b_date;

  a_child = (GcalViewChild*) a;
  b_child = (GcalViewChild*) b;

  a_date = gcal_event_widget_get_date (GCAL_EVENT_WIDGET (a_child->widget));
  b_date = gcal_event_widget_get_date (GCAL_EVENT_WIDGET (b_child->widget));

  return icaltime_compare (*a_date, *b_date);
}

/* Function to do a last minute fixup of the AM/PM stuff if the locale
 * and gettext haven't done it right. Most English speaking countries
 * except the USA use the 24 hour clock (UK, Australia etc). However
 * since they are English nobody bothers to write a language
 * translation (gettext) file. So the locale turns off the AM/PM, but
 * gettext does not turn on the 24 hour clock. Leaving a mess.
 *
 * This routine checks if AM/PM are defined in the locale, if not it
 * forces the use of the 24 hour clock.
 *
 * The function itself is a front end on strftime and takes exactly
 * the same arguments.
 *
 * TODO: Actually remove the '%p' from the fixed up string so that
 * there isn't a stray space.
 */

gsize
e_strftime_fix_am_pm (gchar *str,
                      gsize max,
                      const gchar *fmt,
                      const struct tm *tm)
{
  gchar buf[10];
  gchar *sp;
  gchar *ffmt;
  gsize ret;

  if (strstr(fmt, "%p")==NULL && strstr(fmt, "%P")==NULL) {
    /* No AM/PM involved - can use the fmt string directly */
    ret = e_strftime (str, max, fmt, tm);
  } else {
    /* Get the AM/PM symbol from the locale */
    e_strftime (buf, 10, "%p", tm);

    if (buf[0]) {
      /* AM/PM have been defined in the locale
       * so we can use the fmt string directly. */
      ret = e_strftime (str, max, fmt, tm);
    } else {
      /* No AM/PM defined by locale
       * must change to 24 hour clock. */
      ffmt = g_strdup (fmt);
      for (sp=ffmt; (sp=strstr(sp, "%l")); sp++) {
        /* Maybe this should be 'k', but I have never
         * seen a 24 clock actually use that format. */
        sp[1]='H';
      }
      for (sp=ffmt; (sp=strstr(sp, "%I")); sp++) {
        sp[1]='H';
      }
      ret = e_strftime (str, max, ffmt, tm);
      g_free (ffmt);
    }
  }

  return (ret);
}

gsize
e_utf8_strftime_fix_am_pm (gchar *str,
                           gsize max,
                           const gchar *fmt,
                           const struct tm *tm)
{
  gsize sz, ret;
  gchar *locale_fmt, *buf;

  locale_fmt = g_locale_from_utf8 (fmt, -1, NULL, &sz, NULL);
  if (!locale_fmt)
    return 0;

  ret = e_strftime_fix_am_pm (str, max, locale_fmt, tm);
  if (!ret) {
    g_free (locale_fmt);
    return 0;
  }

  buf = g_locale_to_utf8 (str, ret, NULL, &sz, NULL);
  if (!buf) {
    g_free (locale_fmt);
    return 0;
  }

  if (sz >= max) {
    gchar *tmp = buf + max - 1;
    tmp = g_utf8_find_prev_char (buf, tmp);
    if (tmp)
      sz = tmp - buf;
    else
      sz = 0;
  }
  memcpy (str, buf, sz);
  str[sz] = '\0';
  g_free (locale_fmt);
  g_free (buf);
  return sz;
}
