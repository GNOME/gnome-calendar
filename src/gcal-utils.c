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

#include <libecal/libecal.h>
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

icaltimetype*
gcal_dup_icaltime (const icaltimetype *date)
{
  icaltimetype *new_date;

  if (date == NULL)
    return NULL;

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

/**
 * gcal_get_pixbuf_from_color:
 * @color:
 * @size:
 *
 * Create a pixbuf of a simple {@link GdkRGBA} color.
 *
 * Returns: (Transfer full): An instance of {@link GdkPixbuf} to be freed with g_object_unref()
 **/
GdkPixbuf*
gcal_get_pixbuf_from_color (GdkRGBA  *color,
                            gint      size)
{
  cairo_surface_t *surface;
  cairo_t *cr;
  GdkPixbuf *pix;

  /* TODO: review size here, maybe not hardcoded */
  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, size, size);
  cr = cairo_create (surface);

  cairo_set_source_rgba (cr,
                         color->red,
                         color->green,
                         color->blue,
                         color->alpha);
  cairo_rectangle (cr, 0, 0, size, size);
  cairo_fill (cr);
  cairo_destroy (cr);
  pix = gdk_pixbuf_get_from_surface (surface,
                                     0, 0,
                                     size, size);
  cairo_surface_destroy (surface);
  return pix;
}

GdkPixbuf*
get_circle_pixbuf_from_color (GdkRGBA *color,
                              gint     size)
{
  cairo_surface_t *surface;
  cairo_t *cr;
  GdkPixbuf *pix;

  /* TODO: review size here, maybe not hardcoded */
  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, size, size);
  cr = cairo_create (surface);

  cairo_set_source_rgba (cr,
                         color->red,
                         color->green,
                         color->blue,
                         color->alpha);
  cairo_arc (cr, size / 2.0, size / 2.0, size / 2.0, 0., 2 * M_PI);
  cairo_fill (cr);
  cairo_destroy (cr);
  pix = gdk_pixbuf_get_from_surface (surface,
                                     0, 0,
                                     size, size);
  cairo_surface_destroy (surface);
  return pix;
}

void
get_color_name_from_source (ESource *source, GdkRGBA *out_color)
{
  ESourceSelectable *extension = E_SOURCE_SELECTABLE (e_source_get_extension (source, E_SOURCE_EXTENSION_CALENDAR));

  /* FIXME: We should handle calendars colours better */
  if (!gdk_rgba_parse (out_color, e_source_selectable_get_color (extension)))
    gdk_rgba_parse (out_color, "#becedd"); /* calendar default colour */
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

  gint comparison;

  a_child = (GcalViewChild*) a;
  b_child = (GcalViewChild*) b;

  a_date =
    gcal_event_widget_get_date (GCAL_EVENT_WIDGET (a_child->widget));
  b_date =
    gcal_event_widget_get_date (GCAL_EVENT_WIDGET (b_child->widget));

  comparison = icaltime_compare (*a_date, *b_date);
  g_free (a_date);
  g_free (b_date);

  return comparison;
}

/**
 * get_desc_from_component:
 * @component:
 *
 * Utility method to handle the extraction of the description from an
 * #ECalComponent. This cycle through the list of #ECalComponentText
 * and concatenate each string into one.
 *
 * Returns: (Transfer full) a new allocated string with the description
 **/
gchar*
get_desc_from_component (ECalComponent *component,
                         const gchar   *joint_char)
{
  GSList *text_list;
  GSList *l;

  gchar *desc = NULL;
  e_cal_component_get_description_list (component, &text_list);

  for (l = text_list; l != NULL; l = l->next)
    {
      if (l->data != NULL)
        {
          ECalComponentText *text;
          gchar *carrier;
          text = l->data;

          if (desc != NULL)
            {
              carrier = g_strconcat (desc, joint_char, text->value, NULL);
              g_free (desc);
              desc = carrier;
            }
          else
            {
              desc = g_strdup (text->value);
            }
        }
    }

  e_cal_component_free_text_list (text_list);
  return desc != NULL ? g_strstrip (desc) : NULL;
}

/**
 * get_uuid_from_component:
 * @source: an {@link ESource}
 * @component: an {@link ECalComponent}
 *
 * Obtains the uuid from a component in the form
 * "source_uid:event_uid:event_rid" or "source:uid:event_uid" if the
 * component doesn't hold a recurrence event
 *
 * Returns: (Transfer full) a new allocated string with the description
 **/
gchar*
get_uuid_from_component (ESource       *source,
                         ECalComponent *component)
{
  gchar *uuid;
  ECalComponentId *id;

  id = e_cal_component_get_id (component);
  if (id->rid != NULL)
    {
      uuid = g_strdup_printf ("%s:%s:%s",
                              e_source_get_uid (source),
                              id->uid,
                              id->rid);
    }
  else
    {
      uuid = g_strdup_printf ("%s:%s",
                              e_source_get_uid (source),
                              id->uid);
    }
  e_cal_component_free_id (id);

  return uuid;
}

/**
 * get_first_weekday:
 *
 * Copied from Clocks, which by itself is
 * copied from GtkCalendar.
 *
 * Returns: the first weekday, from 0 to 6
 */
gint
get_first_weekday (void)
{
  int week_start;
#ifdef HAVE__NL_TIME_FIRST_WEEKDAY
  union { unsigned int word; char *string; } langinfo;
  int week_1stday = 0;
  int first_weekday = 1;
  guint week_origin;
#else
  char *gtk_week_start;
#endif

#ifdef HAVE__NL_TIME_FIRST_WEEKDAY
  langinfo.string = nl_langinfo (_NL_TIME_FIRST_WEEKDAY);
  first_weekday = langinfo.string[0];
  langinfo.string = nl_langinfo (_NL_TIME_WEEK_1STDAY);
  week_origin = langinfo.word;
  if (week_origin == 19971130) /* Sunday */
    week_1stday = 0;
  else if (week_origin == 19971201) /* Monday */
    week_1stday = 1;
  else
    g_warning ("Unknown value of _NL_TIME_WEEK_1STDAY.\n");

  week_start = (week_1stday + first_weekday - 1) % 7;
#else
  /* Use a define to hide the string from xgettext */
# define GTK_WEEK_START "calendar:week_start:0"
  gtk_week_start = dgettext ("gtk30", GTK_WEEK_START);

  if (strncmp (gtk_week_start, "calendar:week_start:", 20) == 0)
    week_start = *(gtk_week_start + 20) - '0';
  else
    week_start = -1;

  if (week_start < 0 || week_start > 6)
    {
      g_warning ("Whoever translated calendar:week_start:0 for GTK+ "
                 "did so wrongly.\n");
      week_start = 0;
    }
#endif

  return week_start;
}

/**
 * build_component_from_details:
 * @summary:
 * @initial_date:
 * @final_date:
 *
 * Create a component with the provided details
 *
 * Returns: (Transfer full): an {@link ECalComponent} object
 **/
ECalComponent*
build_component_from_details (const gchar        *summary,
                              const icaltimetype *initial_date,
                              const icaltimetype *final_date)
{
  ECalComponent *event;

  ECalComponentDateTime dt;
  ECalComponentText summ;

  event = e_cal_component_new ();
  e_cal_component_set_new_vtype (event, E_CAL_COMPONENT_EVENT);

  dt.value = (icaltimetype*) initial_date;
  dt.tzid = NULL;
  e_cal_component_set_dtstart (event, &dt);

  if (final_date != NULL)
    {
      dt.value = (icaltimetype*) final_date;
      e_cal_component_set_dtend (event, &dt);
    }
  else
    {
      icaltimetype *dt_end = gcal_dup_icaltime (initial_date);
      icaltime_adjust (dt_end, 1, 0, 0, 0);
      dt.value = dt_end;
      e_cal_component_set_dtend (event, &dt);
      g_free (dt_end);
    }

  summ.altrep = NULL;
  summ.value = summary;
  e_cal_component_set_summary (event, &summ);

  e_cal_component_commit_sequence (event);

  return event;
}

/**
 * icaltime_compare_date:
 * @date1:
 * @date2:
 *
 * Compare date parts of {@link icaltimetype} objects. Return negative value, 0 or positive value
 * accordingly if date1 is before, same day of after date2.
 * As a bonus it returns the amount of days passed between two days on the same year.
 *
 * Returns: negative, 0 or positive
 **/
gint
icaltime_compare_date (const icaltimetype *date1,
                       const icaltimetype *date2)
{
  if (date2 == NULL)
    return 0;

  if (date1->year < date2->year)
    return -1;
  else if (date1->year > date2->year)
    return 1;
  else
    return time_day_of_year (date1->day, date1->month - 1, date1->year) -
           time_day_of_year (date2->day, date2->month - 1, date2->year);
}

gint
icaltime_compare_with_current (const icaltimetype *date1,
                               const icaltimetype *date2,
                               time_t             *current_time_t)
{
  gint result;

  time_t start1, start2, diff1, diff2;
  start1 = icaltime_as_timet_with_zone (*date1, date1->zone != NULL ? date1->zone : e_cal_util_get_system_timezone ());
  start2 = icaltime_as_timet_with_zone (*date2, date2->zone != NULL ? date2->zone : e_cal_util_get_system_timezone ());
  diff1 = start1 - *current_time_t;
  diff2 = start2 - *current_time_t;

  if (diff1 == diff2)
    {
      result = 0;
    }
  else
    {
      if (diff1 == 0)
        result = -1;
      else if (diff2 == 0)
        result = 1;

      if (diff1 > 0 && diff2 < 0)
        result = -1;
      else if (diff2 > 0 && diff1 < 0)
        result = 1;
      else if (diff1 < 0 && diff2 < 0)
        result = ABS (diff1) - ABS (diff2);
      else if (diff1 > 0 && diff2 > 0)
        result = diff1 - diff2;
    }

  return result;
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


/**
 * fix_popover_menu_icons:
 * @window: a #GtkPopover
 *
 * Hackish code that inspects the popover's children,
 * retrieve the hidden GtkImage buried under lots of
 * widgets, and make it visible again.
 *
 * Hopefully, we'll find a better way to do this in
 * the long run.
 *
 */
void
fix_popover_menu_icons (GtkPopover *popover)
{
  GtkWidget *popover_stack;
  GtkWidget *menu_section;
  GtkWidget *menu_section_box;
  GList *stack_children;
  GList *menu_section_children;
  GList *menu_section_box_children, *aux;

  popover_stack = gtk_bin_get_child (GTK_BIN (popover));
  stack_children = gtk_container_get_children (GTK_CONTAINER (popover_stack));

  /**
   * At the moment, the popover stack surely contains only
   * one child of type GtkMenuSectionBox, which contains
   * a single GtkBox.
   */
  menu_section = stack_children->data;
  menu_section_children = gtk_container_get_children (GTK_CONTAINER (menu_section));

	/**
	 * Get the unique box's children.
	 */
  menu_section_box = menu_section_children->data;
  menu_section_box_children = gtk_container_get_children (GTK_CONTAINER (menu_section_box));

  /**
   * Iterate through the GtkModelButtons inside the menu section box.
   */
  for (aux = menu_section_box_children; aux != NULL; aux = aux->next)
    {
      GtkWidget *button_box;
      GList *button_box_children, *aux2;

      button_box = gtk_bin_get_child (GTK_BIN (aux->data));
      button_box_children = gtk_container_get_children (GTK_CONTAINER (button_box));

      /**
       * Since there is no guarantee that the first child is
       * the GtkImage we're looking for, we have to iterate
       * through the children and check if the types match.
       */
      for (aux2 = button_box_children; aux2 != NULL; aux2 = aux2->next)
        {
          GtkWidget *button_box_child;
          button_box_child = aux2->data;

          if (g_type_is_a (G_OBJECT_TYPE (button_box_child), GTK_TYPE_IMAGE))
            {
              gtk_widget_show (button_box_child);
              break;
            }
        }

      g_list_free (button_box_children);
    }

  g_list_free (stack_children);
  g_list_free (menu_section_children);
  g_list_free (menu_section_box_children);
}

/**
 * uri_get_fields:
 *
 * Split the given URI into the
 * fields.
 *
 * Returns: #TRUE if @uri could be parsed, #FALSE otherwise
 */
gboolean
uri_get_fields (const gchar  *uri,
                gchar       **schema,
                gchar       **host,
                gchar       **path)
{
  GRegex *regex;
  GMatchInfo *match;
  gboolean valid;

  g_return_val_if_fail (uri != NULL, FALSE);

  match = NULL;
  valid = FALSE;

  regex = g_regex_new ("([a-zA-Z0-9\\+\\.\\-]*:\\/\\/){0,1}([-a-zA-Z0-9@:%._\\+~#=]{2,256}\\.[a-z]{2,6}\\b)([-a-zA-Z0-9@:%_\\+.//=]*)",
                       G_REGEX_CASELESS, 0, NULL);

  /*
   * Retrieved matching URI. The whole url is
   * checked and the regex groups are:
   * 1. schema
   * 2. host
   * 3. server path
   */
  if (g_regex_match (regex, uri, 0, &match))
    {
      valid = TRUE;

      if (schema != NULL)
        *schema = g_match_info_fetch (match, 1);

      if (host != NULL)
        *host = g_match_info_fetch (match, 2);

      if (path != NULL)
        *path = g_match_info_fetch (match, 3);
    }
  else
    {
      if (schema != NULL)
        *schema = NULL;

      if (host != NULL)
        *host = NULL;

      if (path != NULL)
        *path = NULL;
    }

  g_match_info_free (match);
  g_regex_unref (regex);
  return valid;
}

void
get_source_parent_name_color (GcalManager  *manager,
                              ESource      *source,
                              gchar       **name,
                              gchar       **color)
{
  ESource *parent_source;

  g_assert (source && E_IS_SOURCE (source));

  parent_source = gcal_manager_get_source (manager, e_source_get_parent (source));

  if (name != NULL)
    *name = e_source_dup_display_name (parent_source);

  if (color != NULL)
    {
      GdkRGBA c;

      get_color_name_from_source (parent_source, &c);

      *color = gdk_rgba_to_string (&c);
    }
}
