/* gcal-quick-add-popover.c
 *
 * Copyright (C) 2016 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#define G_LOG_DOMAIN "GcalQuickAddPopover"

#include <glib/gi18n.h>

#include "gcal-quick-add-popover.h"
#include "gcal-utils.h"

struct _GcalQuickAddPopover
{
  GtkPopover          parent;

  GtkWidget          *add_button;
  GtkWidget          *calendars_listbox;
  GtkWidget          *calendar_name_label;
  GtkWidget          *color_image;
  GtkWidget          *stack;
  GtkWidget          *summary_entry;
  GtkWidget          *title_label;

  /* Internal data */
  GDateTime          *date_start;
  GDateTime          *date_end;

  GtkWidget          *selected_row;

  GcalContext        *context;
};

G_DEFINE_TYPE (GcalQuickAddPopover, gcal_quick_add_popover, GTK_TYPE_POPOVER)

enum {
  PROP_0,
  PROP_DATE_START,
  PROP_DATE_END,
  PROP_CONTEXT,
  N_PROPS
};

enum
{
  EDIT_EVENT,
  NUM_SIGNALS
};

static guint signals[NUM_SIGNALS] = { 0, };

/*
 * Utility functions
 */

static GtkWidget*
create_calendar_row (GcalManager  *manager,
                     GcalCalendar *calendar)
{
  cairo_surface_t *surface;
  const GdkRGBA *color;
  GtkWidget *row, *box, *icon, *label, *selected_icon;
  gboolean read_only;
  gchar *tooltip, *parent_name;

  /* The main box */
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_widget_set_margin_start (box, 6);
  gtk_widget_set_margin_end (box, 6);
  gtk_widget_set_margin_top (box, 3);
  gtk_widget_set_margin_bottom (box, 3);

  /* The icon with the source color */
  color = gcal_calendar_get_color (calendar);
  surface = get_circle_surface_from_color (color, 16);
  icon = gtk_image_new_from_surface (surface);

  gtk_container_add (GTK_CONTAINER (box), icon);

  gtk_style_context_add_class (gtk_widget_get_style_context (icon), "calendar-color-image");

  /* Source name label */
  label = gtk_label_new (gcal_calendar_get_name (calendar));
  gtk_widget_set_margin_end (label, 12);

  gtk_container_add (GTK_CONTAINER (box), label);

  /* Selected icon */
  selected_icon = gtk_image_new_from_icon_name ("emblem-ok-symbolic", GTK_ICON_SIZE_BUTTON);
  gtk_container_add (GTK_CONTAINER (box), selected_icon);

  /* The row itself */
  row = gtk_list_box_row_new ();

  read_only = gcal_calendar_is_read_only (calendar);
  gtk_widget_set_sensitive (row, !read_only);

  gtk_container_add (GTK_CONTAINER (row), box);

  /* Setup also a cool tooltip */
  get_source_parent_name_color (manager, gcal_calendar_get_source (calendar), &parent_name, NULL);

  if (read_only)
    tooltip = g_strdup_printf (_("%s (this calendar is read-only)"), parent_name);
  else
    tooltip = g_strdup (parent_name);

  gtk_widget_set_tooltip_text (row, tooltip);

  /*
   * Setup the row data. A reference for the source is
   * taken to make sure it is not destroyed before this
   * row.
   */
  g_object_set_data_full (G_OBJECT (row), "calendar", g_object_ref (calendar), g_object_unref);
  g_object_set_data (G_OBJECT (row), "selected-icon", selected_icon);
  g_object_set_data (G_OBJECT (row), "color-icon", icon);
  g_object_set_data (G_OBJECT (row), "name-label", label);

  gtk_widget_show (label);
  gtk_widget_show (icon);
  gtk_widget_show (box);
  gtk_widget_show (row);

  g_clear_pointer (&surface, cairo_surface_destroy);
  g_free (parent_name);
  g_free (tooltip);

  return row;
}

/*
 * Retrieves the corresponding row for the given
 * ESource from the listbox.
 */
static GtkWidget*
get_row_for_calendar (GcalQuickAddPopover *self,
                      GcalCalendar        *calendar)
{
  GList *children, *l;
  GtkWidget *row;

  row = NULL;
  children = gtk_container_get_children (GTK_CONTAINER (self->calendars_listbox));

  for (l = children; l != NULL; l = g_list_next (l))
    {
      GcalCalendar *row_calendar = g_object_get_data (l->data, "calendar");

      if (row_calendar == calendar)
        {
          row = l->data;
          break;
        }
    }

  g_list_free (children);

  return row;
}

static void
select_row (GcalQuickAddPopover *self,
            GtkListBoxRow       *row)
{
  cairo_surface_t *surface;
  const GdkRGBA *color;
  GcalCalendar *calendar;
  GtkWidget *icon;

  /* First, unselect the previous row */
  if (self->selected_row)
    {
      icon = g_object_get_data (G_OBJECT (self->selected_row), "selected-icon");
      gtk_widget_set_visible (icon, FALSE);
    }

  /* Now, select and save the new row */
  icon = g_object_get_data (G_OBJECT (row), "selected-icon");
  gtk_widget_set_visible (icon, TRUE);

  self->selected_row = GTK_WIDGET (row);

  /* Setup the event page's source name and color */
  calendar = g_object_get_data (G_OBJECT (row), "calendar");

  gtk_label_set_label (GTK_LABEL (self->calendar_name_label), gcal_calendar_get_name (calendar));

  color = gcal_calendar_get_color (calendar);
  surface = get_circle_surface_from_color (color, 16);
  gtk_image_set_from_surface (GTK_IMAGE (self->color_image), surface);

  /* Return to the events page */
  gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "events");

  /* Focus back the event Entry */
  if (gtk_widget_get_visible (GTK_WIDGET (self)))
    gtk_entry_grab_focus_without_selecting (GTK_ENTRY (self->summary_entry));

  g_clear_pointer (&surface, cairo_surface_destroy);
}

static gint
get_number_of_days_from_today (GDateTime *day)
{
  GDate today, event_day;
  gint n_days;

  g_date_set_dmy (&today,
                  g_date_time_get_day_of_month (g_date_time_new_now_local ()),
                  g_date_time_get_month (g_date_time_new_now_local ()),
                  g_date_time_get_year (g_date_time_new_now_local ()));

  g_date_set_dmy (&event_day,
                  g_date_time_get_day_of_month (day),
                  g_date_time_get_month (day),
                  g_date_time_get_year (day));
  n_days = g_date_days_between (&event_day, &today);

  return n_days;
}

static gchar*
get_date_string_for_multiday (GDateTime *start,
                              GDateTime *end)
{
  gint n_days;
  gchar *start_date_str, *end_date_str, *date_string;

  gchar *start_date_weekdays_strings [] = {
    N_("from next Monday"),
    N_("from next Tuesday"),
    N_("from next Wednesday"),
    N_("from next Thursday"),
    N_("from next Friday"),
    N_("from next Saturday"),
    N_("from next Sunday"),
    NULL
  };

  gchar *end_date_weekdays_strings [] = {
    N_("to next Monday"),
    N_("to next Tuesday"),
    N_("to next Wednesday"),
    N_("to next Thursday"),
    N_("to next Friday"),
    N_("to next Saturday"),
    N_("to next Sunday"),
    NULL
  };

  gchar *month_names [] = {
    N_("January"),
    N_("February"),
    N_("March"),
    N_("April"),
    N_("May"),
    N_("June"),
    N_("July"),
    N_("August"),
    N_("September"),
    N_("October"),
    N_("November"),
    N_("December"),
    NULL
  };

  start_date_str = NULL;
  n_days = get_number_of_days_from_today (start);

  if (n_days == 0)
    {
      start_date_str = g_strdup_printf (_("from Today"));
    }
  else if (n_days == -1)
    {
      start_date_str = g_strdup_printf (_("from Tomorrow"));
    }
  else if (n_days == 1)
    {
      start_date_str = g_strdup_printf (_("from Yesterday"));
    }
  else if (n_days < -1 && n_days > -8)
    {
      gchar *start_weekday_str;

      start_weekday_str = start_date_weekdays_strings [g_date_time_get_day_of_week (start) - 1];
      start_date_str = g_strdup_printf ("%s", start_weekday_str);
    }
  else
    {
      gchar *day_number_str;

      day_number_str = g_strdup_printf ("%u", g_date_time_get_day_of_month (start));
      /* Translators:
       * this is the format string for representing a date consisting of a month
       * name and a date of month.
       */
      start_date_str = g_strdup_printf (_("from %1$s %2$s"),
                                        month_names [g_date_time_get_month (start) - 1],
                                        day_number_str);
      g_free (day_number_str);
    }

  end_date_str = NULL;
  n_days = get_number_of_days_from_today (end);

  if (n_days == 0)
    {
      end_date_str = g_strdup_printf (_("to Today"));
    }
  else if (n_days == -1)
    {
      end_date_str = g_strdup_printf (_("to Tomorrow"));
    }
  else if (n_days == 1)
    {
      end_date_str = g_strdup_printf (_("to Yesterday"));
    }
  else if (n_days < -1 && n_days > -8)
    {
      gchar *end_weekday_str;

      end_weekday_str = end_date_weekdays_strings [g_date_time_get_day_of_week (end) - 1];
      end_date_str = g_strdup_printf ("%s", end_weekday_str);
    }
  else
    {
      gchar *day_number_str;

      day_number_str = g_strdup_printf ("%u", g_date_time_get_day_of_month (end));
      /* Translators:
       * this is the format string for representing a date consisting of a month
       * name and a date of month.
       */
      end_date_str = g_strdup_printf (_("to %1$s %2$s"),
                                      month_names [g_date_time_get_month (end) - 1],
                                      day_number_str);
      g_free (day_number_str);
    }

  /* Translators: %1$s is the start date (e.g. "from Today") and %2$s is the end date (e.g. "to Tomorrow") */
  date_string = g_strdup_printf (_("New Event %1$s %2$s"),
                                 start_date_str,
                                 end_date_str);
  return date_string;
}

static gchar*
get_date_string_for_day (GDateTime *day)
{
  gchar *string_for_date;
  gint n_days;

  string_for_date = NULL;
  n_days = get_number_of_days_from_today (day);

  if (n_days == 0)
    {
      string_for_date = g_strdup_printf (_("New Event Today"));
    }
  else if (n_days == -1)
    {
      string_for_date = g_strdup_printf (_("New Event Tomorrow"));
    }
  else if (n_days == 1)
    {
      string_for_date = g_strdup_printf (_("New Event Yesterday"));
    }
  else if (n_days < -1 && n_days > -8)
    {
       gchar *event_weekday;
       gchar *event_weekday_names [] = {
         N_("New Event next Monday"),
         N_("New Event next Tuesday"),
         N_("New Event next Wednesday"),
         N_("New Event next Thursday"),
         N_("New Event next Friday"),
         N_("New Event next Saturday"),
         N_("New Event next Sunday"),
         NULL
       };

       event_weekday = gettext (event_weekday_names [g_date_time_get_day_of_week (day) - 1]);
       string_for_date = g_strdup_printf ("%s", event_weekday);
    }
  else
    {
      const gchar *event_month;
      gchar *event_month_names [] = {
        /* Translators: %d is the numeric day of month */
        N_("New Event on January %d"),
        N_("New Event on February %d"),
        N_("New Event on March %d"),
        N_("New Event on April %d"),
        N_("New Event on May %d"),
        N_("New Event on June %d"),
        N_("New Event on July %d"),
        N_("New Event on August %d"),
        N_("New Event on September %d"),
        N_("New Event on October %d"),
        N_("New Event on November %d"),
        N_("New Event on December %d"),
        NULL
      };

      event_month = gettext (event_month_names [g_date_time_get_month (day) - 1]);
      string_for_date = g_strdup_printf (event_month, g_date_time_get_day_of_month (day));
    }

  return string_for_date;
}

static void
update_header (GcalQuickAddPopover *self)
{
  gboolean multiday_or_timed;
  gchar *title_date;

  if (!self->date_start)
    return;

  multiday_or_timed = self->date_end &&
                      (g_date_time_difference (self->date_end, self->date_start) / G_TIME_SPAN_DAY > 1 ||
                       g_date_time_difference (self->date_end, self->date_start) / G_TIME_SPAN_MINUTE > 1);

  if (multiday_or_timed)
    {
      gboolean all_day;

      all_day = gcal_date_time_is_date (self->date_start) && (self->date_end ? gcal_date_time_is_date (self->date_end) : TRUE);

      if (all_day &&
          (g_date_time_difference (self->date_end, self->date_start) / G_TIME_SPAN_DAY > 1 &&
           g_date_time_difference (self->date_end, self->date_start) / G_TIME_SPAN_MINUTE > 1))
        {
          title_date = get_date_string_for_multiday (self->date_start, g_date_time_add_days (self->date_end, -1));
        }
      else
        {
          g_autofree gchar *start_hour, *end_hour, *event_date_name;
          GcalTimeFormat time_format;
          gchar *hour_format;

          time_format = gcal_context_get_time_format (self->context);

          if (time_format == GCAL_TIME_FORMAT_24H)
              hour_format = "%R";
          else
              hour_format = "%I:%M %P";

          start_hour = g_date_time_format (self->date_start, hour_format);
          end_hour = g_date_time_format (self->date_end, hour_format);

          event_date_name = get_date_string_for_day (self->date_start);
          /* Translators: %1$s is the event name, %2$s is the start hour, and %3$s is the end hour */
          title_date = g_strdup_printf (_("%1$s, %2$s – %3$s"),
                                        event_date_name,
                                        start_hour,
                                        end_hour);
        }
    }
  else
    {
      g_autofree gchar *event_date_name;
      event_date_name = get_date_string_for_day (self->date_start);
      title_date = g_strdup_printf ("%s", event_date_name);
    }

  gtk_label_set_label (GTK_LABEL (self->title_label), title_date);
  g_free (title_date);
}

static void
update_default_calendar_row (GcalQuickAddPopover *self)
{
  GcalCalendar *default_calendar;
  GcalManager *manager;
  GtkWidget *row;

  manager = gcal_context_get_manager (self->context);
  default_calendar = gcal_manager_get_default_calendar (manager);

  row = get_row_for_calendar (self, default_calendar);
  select_row (self, GTK_LIST_BOX_ROW (row));
}


/*
 * Callbacks
 */

static void
on_calendar_added (GcalManager         *manager,
                   GcalCalendar        *calendar,
                   GcalQuickAddPopover *self)
{
  GcalCalendar *default_calendar;
  GtkWidget *row;

  /* Since we can't add on read-only calendars, lets not show them at all */
  if (gcal_calendar_is_read_only (calendar))
    return;

  default_calendar = gcal_manager_get_default_calendar (manager);
  row = create_calendar_row (manager, calendar);

  gtk_container_add (GTK_CONTAINER (self->calendars_listbox), row);

  /* Select the default source whe first adding events */
  if (calendar == default_calendar && !self->selected_row)
    select_row (self, GTK_LIST_BOX_ROW (row));
}

static void
on_calendar_changed (GcalManager         *manager,
                     GcalCalendar        *calendar,
                     GcalQuickAddPopover *self)
{
  cairo_surface_t *surface;
  const GdkRGBA *color;
  GtkWidget *row, *color_icon, *name_label;
  gboolean read_only;

  read_only = gcal_calendar_is_read_only (calendar);
  row = get_row_for_calendar (self, calendar);

  /* If the calendar changed from/to read-only, we add or remove it here */
  if (read_only)
    {
      if (row)
        gtk_container_remove (GTK_CONTAINER (self->calendars_listbox), row);

      return;
    }
  else if (!row)
    {
      on_calendar_added (manager, calendar, self);

      row = get_row_for_calendar (self, calendar);
    }

  color_icon = g_object_get_data (G_OBJECT (row), "color-icon");
  name_label = g_object_get_data (G_OBJECT (row), "name-label");

  /* If the source is writable now (or not), update sensitivity */
  gtk_widget_set_sensitive (row, !read_only);

  /* Setup the source color, in case it changed */
  color = gcal_calendar_get_color (calendar);
  surface = get_circle_surface_from_color (color, 16);
  gtk_image_set_from_surface (GTK_IMAGE (color_icon), surface);

  g_clear_pointer (&surface, cairo_surface_destroy);

  /* Also setup the row name, in case we just changed the source name */
  gtk_label_set_text (GTK_LABEL (name_label), gcal_calendar_get_name (calendar));

  gtk_list_box_invalidate_sort (GTK_LIST_BOX (self->calendars_listbox));

  /*
   * If the row is the selected one, make a fake selection so the
   * icon & name of the source is updated at the events' page.
   */
  if (row == self->selected_row)
    select_row (self, GTK_LIST_BOX_ROW (row));
}

static void
on_calendar_removed (GcalManager         *manager,
                     GcalCalendar        *calendar,
                     GcalQuickAddPopover *self)
{
  GtkWidget *row;

  row = get_row_for_calendar (self, calendar);

  if (!row)
    return;

  gtk_container_remove (GTK_CONTAINER (self->calendars_listbox), row);
}

/* Sort the calendars by their display name */
static gint
sort_func (GtkListBoxRow *row1,
           GtkListBoxRow *row2,
           gpointer       user_data)
{
  GcalCalendar *calendar1, *calendar2;
  gchar *name1, *name2;
  gint retval;

  calendar1 = g_object_get_data (G_OBJECT (row1), "calendar");
  calendar2 = g_object_get_data (G_OBJECT (row2), "calendar");

  name1 = g_utf8_casefold (gcal_calendar_get_name (calendar1), -1);
  name2 = g_utf8_casefold (gcal_calendar_get_name (calendar2), -1);

  retval = g_strcmp0 (name1, name2);

  g_free (name1);
  g_free (name2);

  return retval;
}

static void
select_calendar_button_clicked (GcalQuickAddPopover *self)
{
  gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "calendars");
}

static void
back_button_clicked (GcalQuickAddPopover *self)
{
  gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "events");
}

static void
edit_or_create_event (GcalQuickAddPopover *self,
                      GtkWidget           *button)
{
  ECalComponent *component;
  GcalCalendar *calendar;
  GcalManager *manager;
  GDateTime *date_start, *date_end;
  GTimeZone *tz;
  GcalEvent *event;
  const gchar *summary;
  gboolean all_day, single_day;

  if (!self->selected_row)
    return;

  manager = gcal_context_get_manager (self->context);
  calendar = g_object_get_data (G_OBJECT (self->selected_row), "calendar");

  single_day = gcal_date_time_compare_date (self->date_end, self->date_start) == 0;
  all_day = gcal_date_time_compare_date (self->date_end, self->date_start) > 1 ||
            g_date_time_compare (self->date_end, self->date_start) == 0;
  tz = all_day ? g_time_zone_new_utc () : g_time_zone_new_local ();

  /* Gather start date */
  date_start = g_date_time_new (tz,
                                g_date_time_get_year (self->date_start),
                                g_date_time_get_month (self->date_start),
                                g_date_time_get_day_of_month (self->date_start),
                                g_date_time_get_hour (self->date_start),
                                g_date_time_get_minute (self->date_start),
                                0);

  /* Gather date end */
  if (self->date_end)
    {
      date_end = g_date_time_new (tz,
                                  g_date_time_get_year (self->date_end),
                                  g_date_time_get_month (self->date_end),
                                  g_date_time_get_day_of_month (self->date_end) + (single_day && all_day ? 1 : 0),
                                  g_date_time_get_hour (self->date_end),
                                  g_date_time_get_minute (self->date_end),
                                  0);
    }
  else
    {
      date_end = all_day ? g_date_time_add_days (date_start, 1) : g_date_time_add_hours (date_start, 1);
    }

  /* Gather the summary */
  if (gtk_entry_get_text_length (GTK_ENTRY (self->summary_entry)) > 0)
    summary = gtk_entry_get_text (GTK_ENTRY (self->summary_entry));
  else
    summary = _("Unnamed event");

  /* Create an ECalComponent from the data above */
  component = build_component_from_details (summary, date_start, date_end);

  event = gcal_event_new (calendar, component, NULL);
  gcal_event_set_all_day (event, all_day);

  /* If we clicked edit button, send a signal; otherwise, create the event */
  if (button == self->add_button)
    gcal_manager_create_event (manager, event);
  else
    g_signal_emit (self, signals[EDIT_EVENT], 0, event);

  gtk_widget_hide (GTK_WIDGET (self));

  g_clear_pointer (&date_start, g_date_time_unref);
  g_clear_pointer (&date_end, g_date_time_unref);
  g_clear_pointer (&tz, g_time_zone_unref);
  g_clear_object (&event);
}

static void
summary_entry_text_changed (GtkEntry            *entry,
                            GParamSpec          *pspec,
                            GcalQuickAddPopover *self)
{
  gtk_widget_set_sensitive (self->add_button, gtk_entry_get_text_length (entry) > 0);
}

static void
summary_entry_activated (GtkEntry            *entry,
                         GcalQuickAddPopover *self)
{
  if (gtk_entry_get_text_length (entry) > 0)
    edit_or_create_event (self, self->add_button);
}


/*
 * Overrides
 */

static void
gcal_quick_add_popover_finalize (GObject *object)
{
  GcalQuickAddPopover *self = (GcalQuickAddPopover *)object;

  g_clear_object (&self->context);

  G_OBJECT_CLASS (gcal_quick_add_popover_parent_class)->finalize (object);
}

static void
gcal_quick_add_popover_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GcalQuickAddPopover *self = GCAL_QUICK_ADD_POPOVER (object);

  switch (prop_id)
    {
    case PROP_DATE_END:
      g_value_set_boxed (value, self->date_end);
      break;

    case PROP_DATE_START:
      g_value_set_boxed (value, self->date_start);
      break;

    case PROP_CONTEXT:
      g_value_set_object (value, self->context);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_quick_add_popover_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GcalQuickAddPopover *self = GCAL_QUICK_ADD_POPOVER (object);

  switch (prop_id)
    {
    case PROP_DATE_END:
      gcal_quick_add_popover_set_date_end (self, g_value_get_boxed (value));
      break;

    case PROP_DATE_START:
      gcal_quick_add_popover_set_date_start (self, g_value_get_boxed (value));
      break;

    case PROP_CONTEXT:
      if (self->context != g_value_get_object (value))
        {
          g_autoptr (GList) calendars = NULL;
          GcalManager *manager;
          GList *l;

          if (self->context != NULL)
            {
              manager = gcal_context_get_manager (self->context);
              g_signal_handlers_disconnect_by_data (manager, self);
            }

          g_set_object (&self->context, g_value_get_object (value));

          /* Add currently loaded sources */
          manager = gcal_context_get_manager (self->context);
          calendars = gcal_manager_get_calendars (manager);

          for (l = calendars; l; l = l->next)
            on_calendar_added (manager, l->data, self);

          g_list_free (calendars);

          /* Connect to the manager signals and keep the list updates */
          g_signal_connect_object (manager, "calendar-added", G_CALLBACK (on_calendar_added), self, 0);
          g_signal_connect_object (manager, "calendar-changed", G_CALLBACK (on_calendar_changed), self, 0);
          g_signal_connect_object (manager, "calendar-removed", G_CALLBACK (on_calendar_removed), self, 0);
          g_signal_connect_object (manager, "notify::default-calendar", G_CALLBACK (update_default_calendar_row), self, G_CONNECT_SWAPPED);

          g_signal_connect_object (self->context,
                                   "notify::time-format",
                                   G_CALLBACK (update_header),
                                   self,
                                   G_CONNECT_SWAPPED);

          g_object_notify (G_OBJECT (self), "context");
        }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_quick_add_popover_closed (GtkPopover *popover)
{
  GcalQuickAddPopover *self;

  self = GCAL_QUICK_ADD_POPOVER (popover);

  /* Clear text */
  gtk_entry_set_text (GTK_ENTRY (self->summary_entry), "");

  /* Select the default row again */
  update_default_calendar_row (self);

  g_clear_pointer (&self->date_start, g_date_time_unref);
  g_clear_pointer (&self->date_end, g_date_time_unref);
}

static void
gcal_quick_add_popover_class_init (GcalQuickAddPopoverClass *klass)
{
  GtkPopoverClass *popover_class = GTK_POPOVER_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  popover_class->closed = gcal_quick_add_popover_closed;

  object_class->finalize = gcal_quick_add_popover_finalize;
  object_class->get_property = gcal_quick_add_popover_get_property;
  object_class->set_property = gcal_quick_add_popover_set_property;

  /**
   * GcalQuickAddPopover::edit-event:
   * @self: a #GcalQuickAddPopover
   * @event: the new #GcalEvent
   *
   * Emited when the user clicks 'Edit event' button.
   */
  signals[EDIT_EVENT] = g_signal_new ("edit-event",
                                      GCAL_TYPE_QUICK_ADD_POPOVER,
                                      G_SIGNAL_RUN_LAST,
                                      0, NULL, NULL, NULL,
                                      G_TYPE_NONE,
                                      1,
                                      GCAL_TYPE_EVENT);

  /**
   * GcalQuickAddPopover::date-end:
   *
   * The end date of the new event.
   */
  g_object_class_install_property (object_class,
                                   PROP_DATE_END,
                                   g_param_spec_boxed ("date-end",
                                                       "End date of the event",
                                                       "The end date of the new event",
                                                       G_TYPE_DATE_TIME,
                                                       G_PARAM_READWRITE));

  /**
   * GcalQuickAddPopover::date-start:
   *
   * The start date of the new event.
   */
  g_object_class_install_property (object_class,
                                   PROP_DATE_START,
                                   g_param_spec_boxed ("date-start",
                                                       "Start date of the event",
                                                       "The start date of the new event",
                                                       G_TYPE_DATE_TIME,
                                                       G_PARAM_READWRITE));

  /**
   * GcalQuickAddPopover::manager:
   *
   * The manager of the application.
   */
  g_object_class_install_property (object_class,
                                   PROP_CONTEXT,
                                   g_param_spec_object ("context",
                                                        "Context of the application",
                                                        "The singleton context of the application",
                                                        GCAL_TYPE_CONTEXT,
                                                        G_PARAM_READWRITE));

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/quick-add-popover.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalQuickAddPopover, add_button);
  gtk_widget_class_bind_template_child (widget_class, GcalQuickAddPopover, calendars_listbox);
  gtk_widget_class_bind_template_child (widget_class, GcalQuickAddPopover, calendar_name_label);
  gtk_widget_class_bind_template_child (widget_class, GcalQuickAddPopover, color_image);
  gtk_widget_class_bind_template_child (widget_class, GcalQuickAddPopover, stack);
  gtk_widget_class_bind_template_child (widget_class, GcalQuickAddPopover, summary_entry);
  gtk_widget_class_bind_template_child (widget_class, GcalQuickAddPopover, title_label);

  gtk_widget_class_bind_template_callback (widget_class, back_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, edit_or_create_event);
  gtk_widget_class_bind_template_callback (widget_class, select_calendar_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, select_row);
  gtk_widget_class_bind_template_callback (widget_class, summary_entry_activated);
  gtk_widget_class_bind_template_callback (widget_class, summary_entry_text_changed);
}

static void
gcal_quick_add_popover_init (GcalQuickAddPopover *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_list_box_set_sort_func (GTK_LIST_BOX (self->calendars_listbox), sort_func, NULL, NULL);
}

GtkWidget*
gcal_quick_add_popover_new (void)
{
  return g_object_new (GCAL_TYPE_QUICK_ADD_POPOVER, NULL);
}

/**
 * gcal_quick_add_popover_get_date_start:
 * @self: a #GcalQuickAddPopover
 *
 * Retrieves the start date of @self.
 *
 * Returns: (transfer none): a #GDateTime
 */
GDateTime*
gcal_quick_add_popover_get_date_start (GcalQuickAddPopover *self)
{
  g_return_val_if_fail (GCAL_IS_QUICK_ADD_POPOVER (self), NULL);

  return self->date_start;
}

/**
 * gcal_quick_add_popover_set_date_start:
 * @self: a #GcalQuickAddPopover
 * @dt: a #GDateTime
 *
 * Sets the start date of the to-be-created event.
 */
void
gcal_quick_add_popover_set_date_start (GcalQuickAddPopover *self,
                                       GDateTime           *dt)
{
  g_return_if_fail (GCAL_IS_QUICK_ADD_POPOVER (self));

  if (self->date_start != dt &&
      (!self->date_start || !dt ||
       (self->date_start && dt && !g_date_time_equal (self->date_start, dt))))
    {
      g_clear_pointer (&self->date_start, g_date_time_unref);
      self->date_start = g_date_time_ref (dt);

      update_header (self);

      g_object_notify (G_OBJECT (self), "date-start");
    }
}

/**
 * gcal_quick_add_popover_get_date_end:
 * @self: a #GcalQuickAddPopover
 *
 * Retrieves the end date of @self.
 *
 * Returns: (transfer none): a #GDateTime
 */
GDateTime*
gcal_quick_add_popover_get_date_end (GcalQuickAddPopover *self)
{
  g_return_val_if_fail (GCAL_IS_QUICK_ADD_POPOVER (self), NULL);

  return self->date_end;
}

/**
 * gcal_quick_add_popover_set_date_end:
 * @self: a #GcalQuickAddPopover
 * @dt: a #GDateTime
 *
 * Sets the end date of the to-be-created event.
 */
void
gcal_quick_add_popover_set_date_end (GcalQuickAddPopover *self,
                                     GDateTime           *dt)
{
  g_return_if_fail (GCAL_IS_QUICK_ADD_POPOVER (self));

  if (self->date_end != dt &&
      (!self->date_end || !dt ||
       (self->date_end && dt && !g_date_time_equal (self->date_end, dt))))
    {
      g_clear_pointer (&self->date_end, g_date_time_unref);
      self->date_end = g_date_time_ref (dt);

      update_header (self);

      g_object_notify (G_OBJECT (self), "date-end");
    }
}
