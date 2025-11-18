/* gcal-quick-add-popover.c
 *
 * Copyright (C) 2016-2020 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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
#include "gcal-calendar-row.h"
#include "gcal-utils.h"

struct _GcalQuickAddPopover
{
  GtkPopover          parent;

  GtkWidget          *add_button;
  GtkWidget          *calendars_list_box;
  GtkWidget          *color_image;
  GtkWidget          *edit_button;
  GtkWidget          *summary_entry;
  GtkWidget          *title_label;

  /* Internal data */
  GcalRange          *range;

  GListModel         *read_write_calendars_model;

  GcalContext        *context;
};

G_DEFINE_TYPE (GcalQuickAddPopover, gcal_quick_add_popover, GTK_TYPE_POPOVER)

enum {
  PROP_0,
  PROP_RANGE,
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

static void
row_selected_cb (GtkListBox          *list_box,
                 GtkListBoxRow       *row,
                 GcalQuickAddPopover *self)
{
  GtkWidget *check_button;

  if (!row)
    return;

  check_button = gcal_calendar_row_get_first_suffix_child (GCAL_CALENDAR_ROW (row));

  gtk_check_button_set_active (GTK_CHECK_BUTTON (check_button), TRUE);
}

static void
emit_changed (GcalQuickAddPopover *self)
{
  GtkFilter *filter;

  filter = gtk_filter_list_model_get_filter (GTK_FILTER_LIST_MODEL (self->read_write_calendars_model));
  gtk_filter_changed (filter, GTK_FILTER_CHANGE_DIFFERENT);
}

static GtkWidget*
create_row_func (gpointer data,
                 gpointer user_data)
{
  GcalCalendar *default_calendar;
  GcalCalendar *calendar = data;
  GcalQuickAddPopover *self = user_data;
  GcalManager *manager;
  g_autoptr (GdkPaintable) paintable = NULL;
  g_autofree gchar *parent_name = NULL;
  g_autofree gchar *tooltip = NULL;
  GtkWidget *first_row, *row, *check_button;

  manager = gcal_context_get_manager (self->context);

  default_calendar = gcal_manager_get_default_calendar (manager);

  /* The check button */
  check_button = gtk_check_button_new ();
  gtk_widget_set_valign (check_button, GTK_ALIGN_CENTER);
  gtk_widget_set_can_focus (check_button, FALSE);
  gtk_widget_set_can_target (check_button, FALSE);

  /* Turn these check buttons into radio buttons */
  first_row = gtk_widget_get_first_child (self->calendars_list_box);
  if (first_row)
    {
      GtkWidget *first_check_button;

      first_check_button = gcal_calendar_row_get_first_suffix_child (GCAL_CALENDAR_ROW (first_row));
      gtk_check_button_set_group (GTK_CHECK_BUTTON (check_button), GTK_CHECK_BUTTON (first_check_button));
    }

  row = gcal_calendar_row_new (calendar);
  gcal_calendar_row_add_header_class (GCAL_CALENDAR_ROW (row));
  gcal_calendar_row_add_suffix (GCAL_CALENDAR_ROW (row), check_button);

  if (calendar == default_calendar)
    gtk_list_box_select_row (GTK_LIST_BOX (self->calendars_list_box), GTK_LIST_BOX_ROW (row));

  g_signal_connect_object (calendar, "notify::visible", G_CALLBACK (emit_changed), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (calendar, "notify::read-only", G_CALLBACK (emit_changed), self, G_CONNECT_SWAPPED);

  return row;
}

static void
bind_model (GcalQuickAddPopover *self)
{
  gtk_list_box_bind_model (GTK_LIST_BOX (self->calendars_list_box),
                           self->read_write_calendars_model,
                           create_row_func, self, NULL);
}

static void
set_up_context (GcalQuickAddPopover *self)
{
  GcalManager *manager;

  manager = gcal_context_get_manager (self->context);
  self->read_write_calendars_model = gcal_create_writable_calendars_model (manager);

  bind_model (self);
}

static gint
get_number_of_days_from_today (GDateTime *day)
{
  g_autoptr (GDateTime) now = NULL;
  GDate today, event_day;
  gint n_days;

  now = g_date_time_new_now_local ();

  g_date_set_dmy (&today,
                  g_date_time_get_day_of_month (now),
                  g_date_time_get_month (now),
                  g_date_time_get_year (now));

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
  g_autofree gchar *start_date_str = NULL;
  g_autofree gchar *end_date_str = NULL;
  gchar *date_string;
  gint n_days;

  const gchar *start_date_weekdays_strings[] = {
    N_("from next Monday"),
    N_("from next Tuesday"),
    N_("from next Wednesday"),
    N_("from next Thursday"),
    N_("from next Friday"),
    N_("from next Saturday"),
    N_("from next Sunday"),
    NULL
  };

  const gchar *end_date_weekdays_strings[] = {
    N_("to next Monday"),
    N_("to next Tuesday"),
    N_("to next Wednesday"),
    N_("to next Thursday"),
    N_("to next Friday"),
    N_("to next Saturday"),
    N_("to next Sunday"),
    NULL
  };

  const gchar *month_names[] = {
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
      const gchar *start_weekday_str;

      start_weekday_str = gettext (start_date_weekdays_strings[g_date_time_get_day_of_week (start) - 1]);
      start_date_str = g_strdup_printf ("%s", start_weekday_str);
    }
  else
    {
      g_autofree gchar *day_number_str = NULL;

      day_number_str = g_strdup_printf ("%u", g_date_time_get_day_of_month (start));
      /* Translators:
       * this is the format string for representing a date consisting of a month
       * name and a date of month.
       */
      start_date_str = g_strdup_printf (_("from %1$s %2$s"),
                                        gettext (month_names[g_date_time_get_month (start) - 1]),
                                        day_number_str);
    }

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
      const gchar *end_weekday_str;

      end_weekday_str = gettext (end_date_weekdays_strings[g_date_time_get_day_of_week (end) - 1]);
      end_date_str = g_strdup_printf ("%s", end_weekday_str);
    }
  else
    {
      g_autofree gchar *day_number_str = NULL;

      day_number_str = g_strdup_printf ("%u", g_date_time_get_day_of_month (end));
      /* Translators:
       * this is the format string for representing a date consisting of a month
       * name and a date of month.
       */
      end_date_str = g_strdup_printf (_("to %1$s %2$s"),
                                      gettext (month_names[g_date_time_get_month (end) - 1]),
                                      day_number_str);
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
       const gchar *event_weekday;
       const gchar *event_weekday_names[] = {
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
      const gchar *event_month_names[] = {
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
  g_autoptr (GDateTime) range_start = NULL;
  g_autoptr (GDateTime) range_end = NULL;
  g_autofree gchar *title_date = NULL;
  gboolean multiday_or_timed;


  if (!self->range)
    return;

  range_start = gcal_range_get_start (self->range);
  range_end = gcal_range_get_end (self->range);

  multiday_or_timed = gcal_range_get_range_type (self->range) == GCAL_RANGE_DEFAULT ||
                      gcal_date_time_compare_date (range_start, range_end) != 0;

  if (multiday_or_timed)
    {
      gboolean all_day;

      all_day = gcal_range_get_range_type (self->range) == GCAL_RANGE_DATE_ONLY;

      if (all_day &&
          (g_date_time_difference (range_end, range_start) / G_TIME_SPAN_DAY > 1 &&
           g_date_time_difference (range_end, range_start) / G_TIME_SPAN_MINUTE > 1))
        {
          title_date = get_date_string_for_multiday (range_start, g_date_time_add_days (range_end, -1));
        }
      else
        {
          g_autofree gchar *event_date_name = NULL;
          g_autofree gchar *start_hour = NULL;
          g_autofree gchar *end_hour = NULL;
          GcalTimeFormat time_format;
          const gchar *hour_format;

          time_format = gcal_context_get_time_format (self->context);

          if (time_format == GCAL_TIME_FORMAT_24H)
              hour_format = "%R";
          else
              hour_format = "%I:%M %P";

          start_hour = g_date_time_format (range_start, hour_format);
          end_hour = g_date_time_format (range_end, hour_format);

          event_date_name = get_date_string_for_day (range_start);

          if (all_day)
            {
              title_date = g_strdup (event_date_name);
            }
          else
            {
              /* Translators: %1$s is the event name, %2$s is the start hour, and %3$s is the end hour.
               * To avoid spurious line wrapping, use non-breaking space characters around the en dash. */
              title_date = g_strdup_printf (_("%1$s, %2$s – %3$s"),
                                            event_date_name,
                                            start_hour,
                                            end_hour);
            }

        }
    }
  else
    {
      g_autofree gchar *event_date_name = NULL;
      event_date_name = get_date_string_for_day (range_start);
      title_date = g_strdup_printf ("%s", event_date_name);
    }

  gtk_label_set_label (GTK_LABEL (self->title_label), title_date);
}


/*
 * Callbacks
 */

static void
edit_or_create_event (GcalQuickAddPopover *self,
                      GtkWidget           *button)
{
  g_autoptr (GDateTime) range_start = NULL;
  g_autoptr (GDateTime) range_end = NULL;
  ECalComponent *component;
  GcalCalendar *calendar;
  GtkListBoxRow *selected_row;
  GcalManager *manager;
  GDateTime *date_start, *date_end;
  GTimeZone *tz;
  GcalEvent *event;
  const gchar *summary;
  gboolean all_day, single_day;

  range_start = gcal_range_get_start (self->range);
  range_end = gcal_range_get_end (self->range);

  manager = gcal_context_get_manager (self->context);
  selected_row = gtk_list_box_get_selected_row (GTK_LIST_BOX (self->calendars_list_box));
  calendar = gcal_calendar_row_get_calendar (GCAL_CALENDAR_ROW (selected_row));

  single_day = gcal_date_time_compare_date (range_end, range_start) == 0;
  all_day = gcal_date_time_compare_date (range_end, range_start) > 1 ||
            g_date_time_compare (range_end, range_start) == 0;
  tz = all_day ? g_time_zone_new_utc () : g_time_zone_new_local ();

  /* Gather start date */
  date_start = g_date_time_new (tz,
                                g_date_time_get_year (range_start),
                                g_date_time_get_month (range_start),
                                g_date_time_get_day_of_month (range_start),
                                g_date_time_get_hour (range_start),
                                g_date_time_get_minute (range_start),
                                0);

  /* Gather date end */
  if (range_end)
    {
      date_end = g_date_time_new (tz,
                                  g_date_time_get_year (range_end),
                                  g_date_time_get_month (range_end),
                                  g_date_time_get_day_of_month (range_end) + (single_day && all_day ? 1 : 0),
                                  g_date_time_get_hour (range_end),
                                  g_date_time_get_minute (range_end),
                                  0);
    }
  else
    {
      date_end = all_day ? g_date_time_add_days (date_start, 1) : g_date_time_add_hours (date_start, 1);
    }

  /* Gather the summary */
  summary = gtk_editable_get_text (GTK_EDITABLE (self->summary_entry));

  /* Create an ECalComponent from the data above */
  component = build_component_from_details (summary, date_start, date_end);

  event = gcal_event_new (calendar, component, NULL);
  gcal_event_set_all_day (event, all_day);

  /* If we clicked edit button, send a signal; otherwise, create the event */
  if (button == self->add_button)
    gcal_manager_create_event (manager, event);
  else
    g_signal_emit (self, signals[EDIT_EVENT], 0, event);

  gtk_widget_set_visible (GTK_WIDGET (self), FALSE);

  g_clear_pointer (&date_start, g_date_time_unref);
  g_clear_pointer (&date_end, g_date_time_unref);
  g_clear_pointer (&tz, g_time_zone_unref);
  g_clear_object (&event);
}

static void
summary_entry_text_changed (AdwEntryRow         *entry,
                            GParamSpec          *pspec,
                            GcalQuickAddPopover *self)
{
  gboolean is_valid_event_name;

  is_valid_event_name = gcal_is_valid_event_name (gtk_editable_get_text (GTK_EDITABLE (entry)));

  if (is_valid_event_name)
    gtk_widget_remove_css_class (GTK_WIDGET (entry), "error");
  else
    gtk_widget_add_css_class (GTK_WIDGET (entry), "error");

  gtk_widget_set_sensitive (self->add_button, is_valid_event_name);
}

static void
summary_entry_activated (AdwEntryRow         *entry,
                         GcalQuickAddPopover *self)
{
  if (gcal_is_valid_event_name (gtk_editable_get_text (GTK_EDITABLE (entry))))
    edit_or_create_event (self, self->add_button);
  else
    edit_or_create_event (self, self->edit_button);
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
    case PROP_RANGE:
      g_value_set_object (value, self->range);
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
    case PROP_RANGE:
      gcal_quick_add_popover_set_range (self, g_value_get_boxed (value));
      break;

    case PROP_CONTEXT:
        {
          GcalManager *manager;

          g_assert (self->context == NULL);
          self->context = g_value_dup_object (value);

          manager = gcal_context_get_manager (self->context);
          set_up_context (self);

          /* Connect to the manager signals and keep the list updates */
          g_signal_connect_object (manager,
                                   "notify::default-calendar",
                                   G_CALLBACK (bind_model),
                                   self,
                                   G_CONNECT_SWAPPED);

          g_signal_connect_object (self->context,
                                   "notify::time-format",
                                   G_CALLBACK (update_header),
                                   self,
                                   G_CONNECT_SWAPPED);
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
  gtk_editable_set_text (GTK_EDITABLE (self->summary_entry), "");

  /* Remove "error" class */
  gtk_widget_remove_css_class (self->summary_entry, "error");

  g_clear_pointer (&self->range, gcal_range_unref);

  bind_model (self);
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
   * Emitted when the user clicks 'Edit event' button.
   */
  signals[EDIT_EVENT] = g_signal_new ("edit-event",
                                      GCAL_TYPE_QUICK_ADD_POPOVER,
                                      G_SIGNAL_RUN_LAST,
                                      0, NULL, NULL, NULL,
                                      G_TYPE_NONE,
                                      1,
                                      GCAL_TYPE_EVENT);

  /**
   * GcalQuickAddPopover::range:
   *
   * The range of the new event.
   */
  g_object_class_install_property (object_class,
                                   PROP_RANGE,
                                   g_param_spec_boxed ("range",
                                                       "Range of the new event",
                                                       "The range of the new event",
                                                       G_TYPE_DATE_TIME,
                                                       G_PARAM_READWRITE));

  /**
   * GcalQuickAddPopover::context:
   *
   * The context of the application.
   */
  g_object_class_install_property (object_class,
                                   PROP_CONTEXT,
                                   g_param_spec_object ("context",
                                                        "Context of the application",
                                                        "The singleton context of the application",
                                                        GCAL_TYPE_CONTEXT,
                                                        G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/gui/gcal-quick-add-popover.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalQuickAddPopover, add_button);
  gtk_widget_class_bind_template_child (widget_class, GcalQuickAddPopover, calendars_list_box);
  gtk_widget_class_bind_template_child (widget_class, GcalQuickAddPopover, edit_button);
  gtk_widget_class_bind_template_child (widget_class, GcalQuickAddPopover, summary_entry);
  gtk_widget_class_bind_template_child (widget_class, GcalQuickAddPopover, title_label);

  gtk_widget_class_bind_template_callback (widget_class, edit_or_create_event);
  gtk_widget_class_bind_template_callback (widget_class, row_selected_cb);
  gtk_widget_class_bind_template_callback (widget_class, summary_entry_activated);
  gtk_widget_class_bind_template_callback (widget_class, summary_entry_text_changed);
}

static void
gcal_quick_add_popover_init (GcalQuickAddPopover *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget*
gcal_quick_add_popover_new (void)
{
  return g_object_new (GCAL_TYPE_QUICK_ADD_POPOVER, NULL);
}

/**
 * gcal_quick_add_popover_get_range:
 * @self: a #GcalQuickAddPopover
 *
 * Retrieves the range of @self.
 *
 * Returns: (transfer none): a #GcalRange
 */
GcalRange*
gcal_quick_add_popover_get_range (GcalQuickAddPopover *self)
{
  g_return_val_if_fail (GCAL_IS_QUICK_ADD_POPOVER (self), NULL);

  return self->range;
}

/**
 * gcal_quick_add_popover_set_range:
 * @self: a #GcalQuickAddPopover
 * @range: a #GcalRange
 *
 * Sets the range of the to-be-created event.
 */
void
gcal_quick_add_popover_set_range (GcalQuickAddPopover *self,
                                  GcalRange           *range)
{
  g_return_if_fail (GCAL_IS_QUICK_ADD_POPOVER (self));

  if (self->range != range &&
      (!self->range || !range ||
       (self->range && range && gcal_range_compare (self->range, range) != 0)))
    {
      g_clear_pointer (&self->range, gcal_range_unref);
      self->range = gcal_range_ref (range);

      update_header (self);

      g_object_notify (G_OBJECT (self), "range");
    }
}

