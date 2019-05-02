/* gcal-utils.c
 *
 * Copyright (C) 2012 - Erick Pérez Castellanos
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
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

#include "config.h"

#define G_LOG_DOMAIN "Utils"

/* langinfo.h in glibc 2.27 defines ALTMON_* only if _GNU_SOURCE is defined.  */
#define _GNU_SOURCE

#include "gcal-enums.h"
#include "gcal-utils.h"
#include "gcal-event-widget.h"
#include "gcal-view.h"

#include <libecal/libecal.h>
#include <libedataserver/libedataserver.h>

#include <glib/gi18n.h>

#include <langinfo.h>

#include <string.h>
#include <math.h>
#include <stdlib.h>

/**
 * SECTION:gcal-utils
 * @short_description: Utility functions
 * @title:Utility functions
 */

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
  /* ALTMON_* constants have been introduced in glibc 2.27 (Feb 1, 2018), also
   * have been supported in *BSD family (but not in OS X) since 1990s.
   * If they exist they are the correct way to obtain the month names in
   * nominative case, standalone, without the day number, as used in the
   * calendar header.  This is obligatory in some languages (Slavic, Baltic,
   * Greek, etc.) but also recommended to use in all languages because for
   * other languages there is no difference between ALTMON_* and MON_*.
   * If ALTMON_* is not supported then we must use MON_*.
   */
#ifdef HAVE_ALTMON
  ALTMON_1,
  ALTMON_2,
  ALTMON_3,
  ALTMON_4,
  ALTMON_5,
  ALTMON_6,
  ALTMON_7,
  ALTMON_8,
  ALTMON_9,
  ALTMON_10,
  ALTMON_11,
  ALTMON_12
#else
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
#endif
};

#define SCROLL_HARDNESS 10.0

/**
 * datetime_compare_date:
 * @dt1: (nullable): a #GDateTime
 * @dt2: (nullable): a #GDateTime
 *
 * Compares the dates of @dt1 and @dt2. The times are
 * ignored.
 *
 * Returns: negative, 0 or positive
 */
gint
datetime_compare_date (GDateTime *dt1,
                       GDateTime *dt2)
{
  if (!dt1 && !dt2)
    return 0;
  else if (!dt1)
    return -1;
  else if (!dt2)
    return 1;

  if (g_date_time_get_year (dt1) != g_date_time_get_year (dt2))
    return (g_date_time_get_year (dt1) - g_date_time_get_year (dt2)) * 360;

  if (g_date_time_get_month (dt1) != g_date_time_get_month (dt2))
    return (g_date_time_get_month (dt1) - g_date_time_get_month (dt2)) * 30;

  if (g_date_time_get_day_of_month (dt1) != g_date_time_get_day_of_month (dt2))
    return g_date_time_get_day_of_month (dt1) - g_date_time_get_day_of_month (dt2);

  return 0;
}

/**
 * datetime_to_icaltime:
 * @dt: a #GDateTime
 *
 * Converts the #GDateTime's @dt to an #ICalTime.
 *
 * Returns: (transfer full): an #ICalTime.
 */
ICalTime*
datetime_to_icaltime (GDateTime *dt)
{
  ICalTime *idt;

  if (!dt)
    return NULL;

  idt = i_cal_time_new_null_time ();

  i_cal_time_set_date (idt,
                       g_date_time_get_year (dt),
                       g_date_time_get_month (dt),
                       g_date_time_get_day_of_month (dt));
  i_cal_time_set_time (idt,
                       g_date_time_get_hour (dt),
                       g_date_time_get_minute (dt),
                       g_date_time_get_seconds (dt));
  i_cal_time_set_is_date (idt,
                  i_cal_time_get_hour (idt) == 0 &&
                  i_cal_time_get_minute (idt) == 0 &&
                  i_cal_time_get_second (idt) == 0);

  return idt;
}

/**
 * icaltime_to_datetime:
 * @date: an #ICalTime
 *
 * Converts the #ICalTime's @date to a #GDateTime. The
 * timezone is preserved.
 *
 * Returns: (transfer full): a #GDateTime.
 */
GDateTime*
icaltime_to_datetime (const ICalTime *date)
{
  GDateTime *dt;
  GTimeZone *tz;
  ICalTimezone *zone;

  zone = i_cal_time_get_timezone (date);
  tz = (zone && i_cal_timezone_get_location (zone)) ? g_time_zone_new (i_cal_timezone_get_location (zone)) : g_time_zone_new_utc ();
  dt = g_date_time_new (tz,
                        i_cal_time_get_year (date),
                        i_cal_time_get_month (date),
                        i_cal_time_get_day (date),
                        i_cal_time_is_date (date) ? 0 : i_cal_time_get_hour (date),
                        i_cal_time_is_date (date) ? 0 : i_cal_time_get_minute (date),
                        i_cal_time_is_date (date) ? 0 : i_cal_time_get_second (date));

  g_clear_pointer (&tz, g_time_zone_unref);

  return dt;
}

/**
 * datetime_is_date:
 * @dt: a #GDateTime
 *
 * Checks if @dt represents a date. A pure date
 * has the values of hour, minutes and seconds set
 * to 0.
 *
 * Returns: %TRUE if @dt is a date, %FALSE if it's a
 * timed datetime.
 */
gboolean
datetime_is_date (GDateTime *dt)
{
  return g_date_time_get_hour (dt) == 0 &&
         g_date_time_get_minute (dt) == 0 &&
         g_date_time_get_seconds (dt) == 0;
}

/**
 * gcal_get_weekday:
 * @i: the weekday index
 *
 * Retrieves the weekday name.
 *
 * Returns: (transfer full): the weekday name
 */
gchar*
gcal_get_weekday (gint i)
{
  return nl_langinfo (ab_day[i]);
}

/**
 * gcal_get_month_name:
 * @i: the month index
 *
 * Retrieves the month name.
 *
 * Returns: (transfer full): the month name
 */
gchar*
gcal_get_month_name (gint i)
{
  return nl_langinfo (month_item[i]);
}

/**
 * gcal_get_surface_from_color:
 * @color: a #GdkRGBA
 * @size: the size of the surface
 *
 * Creates a squared surface filled with @color. The
 * surface is always @size x @size.
 *
 * Returns: (transfer full): a #cairo_surface_t
 */
cairo_surface_t*
gcal_get_surface_from_color (GdkRGBA  *color,
                             gint      size)
{
  cairo_surface_t *surface;
  cairo_t *cr;

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

  return surface;
}

/**
 * get_circle_surface_from_color:
 * @color: a #GdkRGBA
 * @size: the size of the surface
 *
 * Creates a circular surface filled with @color. The
 * surface is always @size x @size.
 *
 * Returns: (transfer full): a #cairo_surface_t
 */
cairo_surface_t*
get_circle_surface_from_color (GdkRGBA *color,
                               gint     size)
{
  cairo_surface_t *surface;
  cairo_t *cr;

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

  return surface;
}

/**
 * get_color_name_from_source:
 * @source: an #ESource
 * @out_color: return value for the color
 *
 * Utility function to retrieve the color from @source.
 */
void
get_color_name_from_source (ESource *source,
                            GdkRGBA *out_color)
{
  ESourceSelectable *extension = E_SOURCE_SELECTABLE (e_source_get_extension (source, E_SOURCE_EXTENSION_CALENDAR));

  /* FIXME: We should handle calendars colours better */
  if (!gdk_rgba_parse (out_color, e_source_selectable_get_color (extension)))
    gdk_rgba_parse (out_color, "#becedd"); /* calendar default colour */
}

/**
 * get_desc_from_component:
 * @component: an #ECalComponent
 * @joint_char: the character to use when merging event comments
 *
 * Utility method to handle the extraction of the description from an
 * #ECalComponent. This cycle through the list of #ECalComponentText
 * and concatenate each string into one.
 *
 * Returns: (nullable)(transfer full) a new allocated string with the
 * description
 **/
gchar*
get_desc_from_component (ECalComponent *component,
                         const gchar   *joint_char)
{
  GSList *text_list;
  GSList *l;

  gchar *desc = NULL;
  text_list = e_cal_component_get_descriptions (component);

  for (l = text_list; l != NULL; l = l->next)
    {
      if (l->data != NULL)
        {
          ECalComponentText *text;
          gchar *carrier;
          text = l->data;

          if (desc != NULL)
            {
              carrier = g_strconcat (desc, joint_char, e_cal_component_text_get_value (text), NULL);
              g_free (desc);
              desc = carrier;
            }
          else
            {
              desc = g_strdup (e_cal_component_text_get_value (text));
            }
        }
    }

  g_slist_free_full (text_list, e_cal_component_text_free);
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
  if (e_cal_component_id_get_rid (id) != NULL)
    {
      uuid = g_strdup_printf ("%s:%s:%s",
                              e_source_get_uid (source),
                              e_cal_component_id_get_uid (id),
                              e_cal_component_id_get_rid (id));
    }
  else
    {
      uuid = g_strdup_printf ("%s:%s",
                              e_source_get_uid (source),
                              e_cal_component_id_get_uid (id));
    }
  e_cal_component_id_free (id);

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
  gint week_1stday = 0;
  gint first_weekday = 1;
  guint week_origin;

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

  gchar *gtk_week_start;


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
 * Returns: (transfer full): an {@link ECalComponent} object
 **/
ECalComponent*
build_component_from_details (const gchar *summary,
                              GDateTime   *initial_date,
                              GDateTime   *final_date)
{
  ECalComponent *event;
  ECalComponentDateTime *dt;
  ECalComponentText *summ;
  ICalTimezone *zone;
  ICalTime *itt;
  gboolean all_day;

  event = e_cal_component_new ();
  e_cal_component_set_new_vtype (event, E_CAL_COMPONENT_EVENT);

  /*
   * Check if the event is all day. Notice that it can be all day even
   * without the final date.
   */
  all_day = datetime_is_date (initial_date) && (final_date ? datetime_is_date (final_date) : TRUE);

  /*
   * When the event is all day, we consider UTC timezone by default. Otherwise,
   * we always use the system timezone to create new events
   */
  if (all_day)
    {
      zone = i_cal_timezone_get_utc_timezone ();
    }
  else
    {
      zone = e_cal_util_get_system_timezone ();
    }

  /* Start date */
  itt = datetime_to_icaltime (initial_date);
  i_cal_time_set_timezone (itt, zone);
  i_cal_time_set_is_date (itt, all_day);
  dt = e_cal_component_datetime_new_take (itt, zone ? g_strdup (i_cal_timezone_get_tzid (zone)) : NULL);
  e_cal_component_set_dtstart (event, dt);

  e_cal_component_datetime_free (dt);

  /* End date */
  if (!final_date)
    final_date = g_date_time_add_days (initial_date, 1);

  itt = datetime_to_icaltime (final_date);
  i_cal_time_set_timezone (itt, zone);
  i_cal_time_set_is_date (itt, all_day);
  dt = e_cal_component_datetime_new_take (itt, zone ? g_strdup (i_cal_timezone_get_tzid (zone)) : NULL);
  e_cal_component_set_dtend (event, dt);

  e_cal_component_datetime_free (dt);

  /* Summary */
  summ = e_cal_component_text_new (summary, NULL);
  e_cal_component_set_summary (event, summ);
  e_cal_component_text_free (summ);

  e_cal_component_commit_sequence (event);

  return event;
}

/**
 * icaltime_compare_date:
 * @date1: an #ICalTime
 * @date2: an #ICalTime
 *
 * Compare date parts of #ICalTime objects. Returns negative value,
 * 0 or positive value accordingly if @date1 is before, same day or
 * after date2.
 *
 * As a bonus it returns the amount of days passed between two days on the
 * same year.
 *
 * Returns: negative, 0 or positive
 **/
gint
icaltime_compare_date (const ICalTime *date1,
                       const ICalTime *date2)
{
  if (date2 == NULL)
    return 0;

  if (i_cal_time_get_year (date1) < i_cal_time_get_year (date2))
    return -1;
  else if (i_cal_time_get_year (date1) > i_cal_time_get_year (date2))
    return 1;
  else
    return time_day_of_year (i_cal_time_get_day (date1), i_cal_time_get_month (date1) - 1, i_cal_time_get_year (date1)) -
           time_day_of_year (i_cal_time_get_day (date2), i_cal_time_get_month (date2) - 1, i_cal_time_get_year (date2));
}

/**
 * icaltime_compare_with_current:
 * @date1: an #ICalTime
 * @date2: an #ICalTime
 * @current_time_t: the current time
 *
 * Compares @date1 and @date2 against the current time. Dates
 * closer to the current date are sorted before.
 *
 * Returns: negative if @date1 comes after @date2, 0 if they're
 * equal, positive otherwise
 */
gint
icaltime_compare_with_current (const ICalTime *date1,
                               const ICalTime *date2,
                               time_t         *current_time_t)
{
  ICalTimezone *zone1, *zone2;
  gint result = 0;
  time_t start1, start2, diff1, diff2;

  zone1 = i_cal_time_get_timezone (date1);
  if (!zone1)
    zone1 = e_cal_util_get_system_timezone ();

  zone2 = i_cal_time_get_timezone (date2);
  if (!zone2)
    zone2 = e_cal_util_get_system_timezone ();

  start1 = i_cal_time_as_timet_with_zone (date1, zone1);
  start2 = i_cal_time_as_timet_with_zone (date2, zone2);
  diff1 = start1 - *current_time_t;
  diff2 = start2 - *current_time_t;

  if (diff1 != diff2)
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

/**
 * get_start_of_week:
 * @date: an #ICalTime
 *
 * Retrieves the start of the week that @date
 * falls in. This function already takes the
 * first weekday into account.
 *
 * Returns: (transfer full): a #GDateTime with
 * the start of the week.
 */
GDateTime*
get_start_of_week (ICalTime *date)
{
  ICalTime *new_date;
  GDateTime *dt;

  new_date = i_cal_time_new_from_day_of_year (i_cal_time_start_doy_week (date, get_first_weekday () + 1),
                                              i_cal_time_get_year (date));
  i_cal_time_set_is_date (new_date, FALSE);
  i_cal_time_set_time (new_date, 0, 0, 0);

  dt = g_date_time_new_local (i_cal_time_get_year (new_date),
                              i_cal_time_get_month (new_date),
                              i_cal_time_get_day (new_date),
                              0, 0, 0);

  g_clear_object (&new_date);

  return dt;
}

/**
 * get_end_of_week:
 * @date: an #ICalTime
 *
 * Retrieves the end of the week that @date
 * falls in. This function already takes the
 * first weekday into account.
 *
 * Returns: (transfer full): a #GDateTime with
 * the end of the week.
 */
GDateTime*
get_end_of_week (ICalTime *date)
{
  GDateTime *week_start, *week_end;

  week_start = get_start_of_week (date);
  week_end = g_date_time_add_days (week_start, 7);

  g_clear_pointer (&week_start, g_date_time_unref);

  return week_end;
}

/**
 * is_clock_format_24h:
 *
 * Retrieves whether the current clock format is
 * 12h or 24h based.
 *
 * Returns: %TRUE if the clock format is 24h, %FALSE
 * otherwise.
 */
gboolean
is_clock_format_24h (void)
{
  static GSettings *settings = NULL;
  g_autofree gchar *clock_format = NULL;

  if (!settings)
    settings = g_settings_new ("org.gnome.desktop.interface");

  clock_format = g_settings_get_string (settings, "clock-format");

  return g_strcmp0 (clock_format, "24h") == 0;
}

/**
 * e_strftime_fix_am_pm:
 *
 * Function to do a last minute fixup of the AM/PM stuff if the locale
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

/**
 * e_utf8_strftime_fix_am_pm:
 *
 * Stolen from Evolution codebase. Selects the
 * correct time format.
 *
 * Returns: the size of the string
 */
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

  gtk_style_context_add_class (gtk_widget_get_style_context (menu_section_box), "calendars-list");

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
              gtk_style_context_add_class (gtk_widget_get_style_context (button_box_child), "calendar-color-image");
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
 * get_source_parent_name_color:
 * @manager: a #GcalManager
 * @source: an #ESource
 * @name: (nullable): return location for the name
 * @color: (nullable): return location for the color
 *
 * Retrieves the name and the color of the #ESource that is
 * parent of @source.
 */
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

/**
 * format_utc_offset:
 * @offset: an UTC offset
 *
 * Formats the UTC offset to a string that GTimeZone can
 * parse. E.g. "-0300" or "+0530".
 *
 * Returns: (transfer full): a string representing the
 * offset
 */
gchar*
format_utc_offset (gint64 offset)
{
  const char *sign = "+";
  gint hours, minutes, seconds;

  if (offset < 0) {
      offset = -offset;
      sign = "-";
  }

  /* offset can be seconds or microseconds */
  if (offset >= 1000000)
    offset = offset / 1000000;

  hours = offset / 3600;
  minutes = (offset % 3600) / 60;
  seconds = offset % 60;

  if (seconds > 0)
    return g_strdup_printf ("%s%02i%02i%02i", sign, hours, minutes, seconds);
  else
    return g_strdup_printf ("%s%02i%02i", sign, hours, minutes);
}

/**
 * get_alarm_trigger_minutes:
 * @event: a #GcalEvent
 * @alarm: a #ECalComponentAlarm
 *
 * Calculates the number of minutes before @event's
 * start time that the alarm should be triggered.
 *
 * Returns: the number of minutes before the event
 * start that @alarm will be triggered.
 */
gint
get_alarm_trigger_minutes (GcalEvent          *event,
                           ECalComponentAlarm *alarm)
{
  ECalComponentAlarmTrigger *trigger;
  ICalDuration *duration;
  GDateTime *alarm_dt;
  gint diff;

  trigger = e_cal_component_alarm_get_trigger (alarm);

  /*
   * We only support alarms relative to the start date, and solely
   * ignore whetever different it may be.
   */
  if (!trigger || e_cal_component_alarm_trigger_get_kind (trigger) != E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_START)
    return -1;

  duration = e_cal_component_alarm_trigger_get_duration (trigger);
  alarm_dt = g_date_time_add_full (gcal_event_get_date_start (event),
                                   0,
                                   0,
                                   - (i_cal_duration_get_days (duration) + i_cal_duration_get_weeks (duration) * 7),
                                   - i_cal_duration_get_hours (duration),
                                   - i_cal_duration_get_minutes (duration),
                                   - i_cal_duration_get_seconds (duration));

  diff = g_date_time_difference (gcal_event_get_date_start (event), alarm_dt) / G_TIME_SPAN_MINUTE;

  g_clear_pointer (&alarm_dt, g_date_time_unref);

  return diff;
}

/**
 * should_change_date_for_scroll:
 * @scroll_value: the current scroll value
 * @scroll_event: the #GdkEventScroll that is being parsed
 *
 * Utility function to check if the date should change based
 * on the scroll. The date is changed when the user scrolls
 * too much on touchpad, or performs a rotation of the scroll
 * button in a mouse.
 *
 * Returns: %TRUE if the date should change, %FALSE otherwise.
 */
gboolean
should_change_date_for_scroll (gdouble        *scroll_value,
                               GdkEventScroll *scroll_event)
{
  gdouble delta_y;

  switch (scroll_event->direction)
    {
    case GDK_SCROLL_DOWN:
      *scroll_value = SCROLL_HARDNESS;
      break;

    case GDK_SCROLL_UP:
      *scroll_value = -SCROLL_HARDNESS;
      break;

    case GDK_SCROLL_SMOOTH:
      gdk_event_get_scroll_deltas ((GdkEvent*) scroll_event, NULL, &delta_y);
      *scroll_value += delta_y;
      break;

    /* Ignore horizontal scrolling for now */
    case GDK_SCROLL_LEFT:
    case GDK_SCROLL_RIGHT:
    default:
      break;
    }

  if (*scroll_value <= -SCROLL_HARDNESS || *scroll_value >= SCROLL_HARDNESS)
    return TRUE;

  return FALSE;
}

/**
 * is_source_enabled:
 * @source: an #ESource
 *
 * Retrieves whether the @source is enabled or not.
 * Disabled sources don't show their events.
 *
 * Returns: %TRUE if @source is enabled, %FALSE otherwise.
 */
gboolean
is_source_enabled (ESource *source)
{
  ESourceSelectable *selectable;

  g_return_val_if_fail (E_IS_SOURCE (source), FALSE);

  selectable = e_source_get_extension (source, E_SOURCE_EXTENSION_CALENDAR);

  return e_source_selectable_get_selected (selectable);
}

/**
 * ask_recurrence_modification_type:
 * @parent: a #GtkWidget
 * @modtype: an #ECalObjModType
 * @source: an #ESource
 *
 * Assigns the appropriate modtype while modifying an event
 * based on user's choice in the GtkMessageDialog that pops up.
 * The modtype helps the user choose the part of recurrent events
 * to modify. Such as Only This Event, Subsequent events
 * or All events.
 *
 * Returns: %TRUE if user chooses appropriate option and
 * @modtype is assigned, %FALSE otherwise.
 */
gboolean
ask_recurrence_modification_type (GtkWidget      *parent,
                                  GcalRecurrenceModType *modtype,
                                  ESource        *source)
{
  GtkWidget *dialog;
  GtkDialogFlags flags;
  gint result;
  gboolean is_set;
  EClient *client;

  flags = GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT;
  *modtype = GCAL_RECURRENCE_MOD_THIS_ONLY;

  dialog = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (parent)),
                                   flags,
                                   GTK_MESSAGE_QUESTION,
                                   GTK_BUTTONS_NONE,
                                   _("The event you are trying to modify is recurring. The changes you have selected should be applied to:"));

  gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                          _("_Cancel"),
                          GTK_RESPONSE_CANCEL,
                          _("_Only This Event"),
                          GTK_RESPONSE_ACCEPT,
                          NULL);

  client = g_object_get_data (G_OBJECT (source), "client");

  if (!e_client_check_capability (E_CLIENT (client), E_CAL_STATIC_CAPABILITY_NO_THISANDFUTURE))
    gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Subsequent events"), GTK_RESPONSE_OK);

  gtk_dialog_add_button (GTK_DIALOG (dialog), _("_All events"), GTK_RESPONSE_YES);

  gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (gtk_widget_get_toplevel (parent)));

  result = gtk_dialog_run (GTK_DIALOG (dialog));

  switch (result)
    {
      case GTK_RESPONSE_CANCEL:
        is_set = FALSE;
        break;
      case GTK_RESPONSE_ACCEPT:
        *modtype = GCAL_RECURRENCE_MOD_THIS_ONLY;
        is_set = TRUE;
        break;
      case GTK_RESPONSE_OK:
        *modtype = GCAL_RECURRENCE_MOD_THIS_AND_FUTURE;
        is_set = TRUE;
        break;
      case GTK_RESPONSE_YES:
        *modtype = GCAL_RECURRENCE_MOD_ALL;
        is_set = TRUE;
        break;
      default:
        is_set = FALSE;
        break;
    }

  gtk_widget_destroy (GTK_WIDGET (dialog));

  return is_set;
}

struct
{
  const gchar        *territory;
  GcalWeekDay         no_work_days;
} no_work_day_per_locale[] = {
  { "AE", GCAL_WEEK_DAY_FRIDAY   | GCAL_WEEK_DAY_SATURDAY } /* United Arab Emirates */,
  { "AF", GCAL_WEEK_DAY_FRIDAY   | GCAL_WEEK_DAY_SATURDAY } /* Afghanistan */,
  { "BD", GCAL_WEEK_DAY_FRIDAY   | GCAL_WEEK_DAY_SATURDAY } /* Bangladesh */,
  { "BH", GCAL_WEEK_DAY_FRIDAY   | GCAL_WEEK_DAY_SATURDAY } /* Bahrain */,
  { "BN", GCAL_WEEK_DAY_SUNDAY   | GCAL_WEEK_DAY_FRIDAY   } /* Brunei Darussalam */,
  { "CR", GCAL_WEEK_DAY_SATURDAY                          } /* Costa Rica */,
  { "DJ", GCAL_WEEK_DAY_FRIDAY                            } /* Djibouti */,
  { "DZ", GCAL_WEEK_DAY_FRIDAY   | GCAL_WEEK_DAY_SATURDAY } /* Algeria */,
  { "EG", GCAL_WEEK_DAY_FRIDAY   | GCAL_WEEK_DAY_SATURDAY } /* Egypt */,
  { "GN", GCAL_WEEK_DAY_SATURDAY                          } /* Equatorial Guinea */,
  { "HK", GCAL_WEEK_DAY_SATURDAY                          } /* Hong Kong */,
  { "IL", GCAL_WEEK_DAY_FRIDAY   | GCAL_WEEK_DAY_SATURDAY } /* Israel */,
  { "IQ", GCAL_WEEK_DAY_FRIDAY   | GCAL_WEEK_DAY_SATURDAY } /* Iraq */,
  { "IR", GCAL_WEEK_DAY_THURSDAY | GCAL_WEEK_DAY_FRIDAY   } /* Iran */,
  { "KW", GCAL_WEEK_DAY_FRIDAY   | GCAL_WEEK_DAY_SATURDAY } /* Kuwait */,
  { "KZ", GCAL_WEEK_DAY_FRIDAY   | GCAL_WEEK_DAY_SATURDAY } /* Kazakhstan */,
  { "LY", GCAL_WEEK_DAY_FRIDAY   | GCAL_WEEK_DAY_SATURDAY } /* Libya */,
  { "MX", GCAL_WEEK_DAY_SATURDAY                          } /* Mexico */,
  { "MY", GCAL_WEEK_DAY_FRIDAY   | GCAL_WEEK_DAY_SATURDAY } /* Malaysia */,
  { "NP", GCAL_WEEK_DAY_SATURDAY                          } /* Nepal */,
  { "OM", GCAL_WEEK_DAY_FRIDAY   | GCAL_WEEK_DAY_SATURDAY } /* Oman */,
  { "QA", GCAL_WEEK_DAY_FRIDAY   | GCAL_WEEK_DAY_SATURDAY } /* Qatar */,
  { "SA", GCAL_WEEK_DAY_FRIDAY   | GCAL_WEEK_DAY_SATURDAY } /* Saudi Arabia */,
  { "SU", GCAL_WEEK_DAY_FRIDAY   | GCAL_WEEK_DAY_SATURDAY } /* Sudan */,
  { "SY", GCAL_WEEK_DAY_FRIDAY   | GCAL_WEEK_DAY_SATURDAY } /* Syria */,
  { "UG", GCAL_WEEK_DAY_SUNDAY                            } /* Uganda */,
  { "YE", GCAL_WEEK_DAY_FRIDAY   | GCAL_WEEK_DAY_SATURDAY } /* Yemen */,
};


/**
 * is_workday:
 * @day: a guint representing the day of a week (0…Sunday, 6…Saturday)
 *
 * Checks whether @day is workday or not based on the Territory part of Locale.
 *
 * Returns: %TRUE if @day is a workday, %FALSE otherwise.
 */
gboolean
is_workday (guint day)
{
  GcalWeekDay no_work_days;
  gchar *locale;
  gchar territory[3] = { 0, };
  guint i;

  if (day > 6)
    return FALSE;

  no_work_days = GCAL_WEEK_DAY_SATURDAY | GCAL_WEEK_DAY_SUNDAY;

  locale = getenv ("LC_ALL");
  if (!locale || g_utf8_strlen (locale, -1) < 5)
    locale = getenv ("LC_TIME");

  if (!locale)
    {
      static gboolean know_that = FALSE;
      if (!know_that)
        {
          know_that = TRUE;
          g_warning ("Locale is NULL, assuming Saturday and Sunday as non workdays");
        }
      return !(no_work_days & 1 << day);
    }

  g_return_val_if_fail (g_utf8_strlen (locale, -1) >= 5, TRUE);

  territory[0] = locale[3];
  territory[1] = locale[4];

  for (i = 0; i < G_N_ELEMENTS (no_work_day_per_locale); i++)
    {
      if (g_strcmp0 (territory, no_work_day_per_locale[i].territory) == 0)
        {
          no_work_days = no_work_day_per_locale[i].no_work_days;
          break;
        }
    }

  return !(no_work_days & 1 << day);
}

GList*
filter_event_list_by_uid_and_modtype (GList                 *widgets,
                                      GcalRecurrenceModType  mod,
                                      const gchar           *uid)
{
  GcalEvent *event;
  GList *result;
  GList *l;

  event = NULL;
  result = NULL;

  /* First pass: find the GcalEvent */
  for (l = widgets; l != NULL; l = l->next)
    {
      GcalEventWidget *event_widget;
      GcalEvent *ev;

      event_widget = l->data;

      /* Safeguard against stray widgets */
      if (!GCAL_IS_EVENT_WIDGET (event_widget))
        continue;

      ev = gcal_event_widget_get_event (event_widget);

      /*
       * We can assume only one event will have the exact uuid. Even among
       * recurrencies.
       */
      if (g_str_equal (uid, gcal_event_get_uid (ev)))
        {
          result = g_list_prepend (result, event_widget);
          event = ev;
        }
    }

  /* Second pass: find the other related events */
  if (event && mod != GCAL_RECURRENCE_MOD_THIS_ONLY)
    {
      ECalComponentId *id;
      ECalComponent *component;
      ESource *source;
      g_autofree gchar *id_prefix;

      component = gcal_event_get_component (event);
      source = gcal_event_get_source (event);
      id = e_cal_component_get_id (component);
      id_prefix = g_strdup_printf ("%s:%s", e_source_get_uid (source), e_cal_component_id_get_uid (id));

      for (l = widgets; l != NULL; l = l->next)
        {
          GcalEventWidget *event_widget;
          GcalEvent *ev;

          event_widget = l->data;

          /* Safeguard against stray widgets */
          if (!GCAL_IS_EVENT_WIDGET (event_widget))
            continue;

          ev = gcal_event_widget_get_event (event_widget);

          if (g_str_equal (gcal_event_get_uid (ev), uid))
            continue;

          if (!g_str_has_prefix (gcal_event_get_uid (ev), id_prefix))
            continue;

          if (mod == GCAL_RECURRENCE_MOD_ALL)
            {
              result = g_list_prepend (result, event_widget);
            }
          else if (mod == GCAL_RECURRENCE_MOD_THIS_AND_FUTURE)
            {
              if (g_date_time_compare (gcal_event_get_date_start (event), gcal_event_get_date_start (ev)) < 0)
                result = g_list_prepend (result, event_widget);
            }

        }

      e_cal_component_id_free (id);
    }

  return result;
}

gboolean
gcal_translate_child_window_position (GtkWidget *target,
                                      GdkWindow *child_window,
                                      gdouble    src_x,
                                      gdouble    src_y,
                                      gdouble   *real_x,
                                      gdouble   *real_y)
{
  GdkWindow *window;
  gdouble x, y;

  x = src_x;
  y = src_y;

  /* Find the (x, y) values relative to the workbench */
  window = child_window;
  while (window && window != gtk_widget_get_window (target))
    {
      gdk_window_coords_to_parent (window, x, y, &x, &y);
      window = gdk_window_get_parent (window);
    }

  if (!window)
    return FALSE;

  if (real_x)
    *real_x = x;

  if (real_y)
    *real_y = y;

  return TRUE;
}
