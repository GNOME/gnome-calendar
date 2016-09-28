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

  GcalManager        *manager;
};

G_DEFINE_TYPE (GcalQuickAddPopover, gcal_quick_add_popover, GTK_TYPE_POPOVER)

enum {
  PROP_0,
  PROP_DATE_START,
  PROP_DATE_END,
  PROP_MANAGER,
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
create_row_for_source (GcalManager *manager,
                       ESource     *source,
                       gboolean     writable)
{
  GtkWidget *row, *box, *icon, *label, *selected_icon;
  GdkPixbuf *circle_pixbuf;
  GdkRGBA color;
  gchar *tooltip, *parent_name;

  /* The main box */
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_widget_set_margin_start (box, 6);
  gtk_widget_set_margin_end (box, 6);
  gtk_widget_set_margin_top (box, 3);
  gtk_widget_set_margin_bottom (box, 3);

  /* The icon with the source color */
  get_color_name_from_source (source, &color);
  circle_pixbuf = get_circle_pixbuf_from_color (&color, 16);
  icon = gtk_image_new_from_pixbuf (circle_pixbuf);

  gtk_container_add (GTK_CONTAINER (box), icon);

  gtk_style_context_add_class (gtk_widget_get_style_context (icon), "calendar-color-image");

  /* Source name label */
  label = gtk_label_new (e_source_get_display_name (source));
  gtk_widget_set_margin_end (label, 12);

  gtk_container_add (GTK_CONTAINER (box), label);

  /* Selected icon */
  selected_icon = gtk_image_new_from_icon_name ("emblem-ok-symbolic", GTK_ICON_SIZE_BUTTON);
  gtk_container_add (GTK_CONTAINER (box), selected_icon);

  /* The row itself */
  row = gtk_list_box_row_new ();
  gtk_widget_set_sensitive (row, writable);

  gtk_container_add (GTK_CONTAINER (row), box);

  /* Setup also a cool tooltip */
  get_source_parent_name_color (manager, source, &parent_name, NULL);

  if (writable)
    tooltip = g_strdup (parent_name);
  else
    tooltip = g_strdup_printf (_("%s (this calendar is read-only)"), parent_name);

  gtk_widget_set_tooltip_text (row, tooltip);

  /*
   * Setup the row data. A reference for the source is
   * taken to make sure it is not destroyed before this
   * row.
   */
  g_object_set_data_full (G_OBJECT (row), "source", g_object_ref (source), g_object_unref);
  g_object_set_data (G_OBJECT (row), "selected-icon", selected_icon);
  g_object_set_data (G_OBJECT (row), "color-icon", icon);
  g_object_set_data (G_OBJECT (row), "name-label", label);

  gtk_widget_show (label);
  gtk_widget_show (icon);
  gtk_widget_show (box);
  gtk_widget_show (row);

  g_clear_object (&circle_pixbuf);
  g_free (parent_name);
  g_free (tooltip);

  return row;
}

/*
 * Retrieves the corresponding row for the given
 * ESource from the listbox.
 */
static GtkWidget*
get_row_for_source (GcalQuickAddPopover *self,
                    ESource             *source)
{
  GList *children, *l;
  GtkWidget *row;

  row = NULL;
  children = gtk_container_get_children (GTK_CONTAINER (self->calendars_listbox));

  for (l = children; l != NULL; l = g_list_next (l))
    {
      ESource *row_source = g_object_get_data (l->data, "source");

      if (row_source == source)
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
  GtkWidget *icon;
  GdkPixbuf *circle_pixbuf;
  ESource *source;
  GdkRGBA color;

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
  source = g_object_get_data (G_OBJECT (row), "source");

  gtk_label_set_label (GTK_LABEL (self->calendar_name_label), e_source_get_display_name (source));

  get_color_name_from_source (source, &color);
  circle_pixbuf = get_circle_pixbuf_from_color (&color, 16);
  gtk_image_set_from_pixbuf (GTK_IMAGE (self->color_image), circle_pixbuf);

  /* Return to the events page */
  gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "events");

  g_clear_object (&circle_pixbuf);
}

static void
update_header (GcalQuickAddPopover *self)
{
  icaltimetype *dtstart;
  struct tm stm_date, etm_date;
  gboolean all_day;
  gchar start[64], end[64];
  gchar *title_date;

  all_day = datetime_is_date (self->date_start) && (self->date_end ? datetime_is_date (self->date_end) : TRUE);
  dtstart = datetime_to_icaltime (self->date_start);

  /* Translators:
   * this is the format string for representing a date consisting of a month name
   * and a date of month.
   */
  stm_date = icaltimetype_to_tm (dtstart);
  e_utf8_strftime_fix_am_pm (start, 64, C_("event date format", "%B %d"), &stm_date);

  if (self->date_end &&
      (!all_day ||
       (all_day && g_date_time_difference (self->date_end, self->date_start) / G_TIME_SPAN_DAY > 1)))
    {
      icaltimetype *dtend;

      dtend = datetime_to_icaltime (self->date_end);

      if (dtend->is_date)
        icaltime_adjust (dtend, -1, 0, 0, 0);

      /* Translators:
       * this is the format string for representing a date consisting of a month name
       * and a date of month.
       */
      etm_date = icaltimetype_to_tm (dtend);
      e_utf8_strftime_fix_am_pm (end, 64, C_("event date format", "%B %d"), &etm_date);

      title_date = g_strdup_printf (_("New Event from %s to %s"), start, end);

      g_free (dtend);
    }
  else
    {
      title_date = g_strdup_printf (_("New Event on %s"), start);
    }

  gtk_label_set_label (GTK_LABEL (self->title_label), title_date);

  g_free (title_date);
  g_free (dtstart);
}

static void
update_default_calendar_row (GcalQuickAddPopover *self)
{
  GtkWidget *row;
  ESource *default_source;

  default_source = gcal_manager_get_default_source (self->manager);

  row = get_row_for_source (self, default_source);
  select_row (self, GTK_LIST_BOX_ROW (row));

  g_clear_object (&default_source);
}


/*
 * Callbacks
 */

static void
on_source_added (GcalManager         *manager,
                 ESource             *source,
                 gboolean             enabled,
                 GcalQuickAddPopover *self)
{
  ESource *default_source;
  GtkWidget *row;

  /* Since we can't add on read-only calendars, lets not show them at all */
  if (!gcal_manager_is_client_writable (manager, source))
    return;

  default_source = gcal_manager_get_default_source (manager);
  row = create_row_for_source (manager, source, gcal_manager_is_client_writable (manager, source));

  gtk_container_add (GTK_CONTAINER (self->calendars_listbox), row);

  /* Select the default source whe first adding events */
  if (source == default_source && !self->selected_row)
    select_row (self, GTK_LIST_BOX_ROW (row));

  g_clear_object (&default_source);
}

static void
on_source_changed (GcalManager         *manager,
                   ESource             *source,
                   GcalQuickAddPopover *self)
{
  GtkWidget *row, *color_icon, *name_label;
  GdkPixbuf *circle_pixbuf;
  GdkRGBA color;

  row = get_row_for_source (self, source);

  /* If the calendar changed from/to read-only, we add or remove it here */
  if (!gcal_manager_is_client_writable (self->manager, source))
    {
      gtk_container_remove (GTK_CONTAINER (self->calendars_listbox), row);
      return;
    }
  else if (!row)
    {
      on_source_added (self->manager,
                       source,
                       gcal_manager_source_enabled (self->manager, source),
                       self);

      row = get_row_for_source (self, source);
    }

  color_icon = g_object_get_data (G_OBJECT (row), "color-icon");
  name_label = g_object_get_data (G_OBJECT (row), "name-label");

  /* If the source is writable now (or not), update sensitivity */
  gtk_widget_set_sensitive (row, gcal_manager_is_client_writable (manager, source));

  /* Setup the source color, in case it changed */
  get_color_name_from_source (source, &color);
  circle_pixbuf = get_circle_pixbuf_from_color (&color, 16);
  gtk_image_set_from_pixbuf (GTK_IMAGE (color_icon), circle_pixbuf);

  g_clear_object (&circle_pixbuf);

  /* Also setup the row name, in case we just changed the source name */
  gtk_label_set_text (GTK_LABEL (name_label), e_source_get_display_name (source));

  gtk_list_box_invalidate_sort (GTK_LIST_BOX (self->calendars_listbox));

  /*
   * If the row is the selected one, make a fake selection so the
   * icon & name of the source is updated at the events' page.
   */
  if (row == self->selected_row)
    select_row (self, GTK_LIST_BOX_ROW (row));
}

static void
on_source_removed (GcalManager         *manager,
                   ESource             *source,
                   GcalQuickAddPopover *self)
{
  GtkWidget *row;

  row = get_row_for_source (self, source);

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
  ESource *source1, *source2;
  gchar *name1, *name2;
  gint retval;

  source1 = g_object_get_data (G_OBJECT (row1), "source");
  source2 = g_object_get_data (G_OBJECT (row2), "source");

  name1 = g_utf8_casefold (e_source_get_display_name (source1), -1);
  name2 = g_utf8_casefold (e_source_get_display_name (source2), -1);

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
  GDateTime *date_start, *date_end, *now;
  GTimeZone *tz;
  GcalEvent *event;
  ESource *source;
  const gchar *summary;
  gboolean all_day, single_day, is_today;

  if (!self->selected_row)
    return;

  now = g_date_time_new_now_local ();
  source = g_object_get_data (G_OBJECT (self->selected_row), "source");

  /*
   * We consider all day events multiday and/or non-today events.
   * Events on today starts now and lasts 1 hour.
   */
  single_day = datetime_compare_date (self->date_end, self->date_start) == 0;
  is_today = single_day && datetime_compare_date (now, self->date_start) == 0;
  all_day = datetime_compare_date (self->date_end, self->date_start) > 1 || !is_today;

  tz = all_day ? g_time_zone_new_utc () : g_time_zone_new_local ();

  /* Gather start date */
  date_start = g_date_time_new (tz,
                                g_date_time_get_year (self->date_start),
                                g_date_time_get_month (self->date_start),
                                g_date_time_get_day_of_month (self->date_start),
                                all_day ? 0 : g_date_time_get_hour (now),
                                all_day ? 0 : g_date_time_get_minute (now),
                                all_day ? 0 : g_date_time_get_second (now));

  /* Gather date end */
  if (self->date_end)
    {
      date_end = g_date_time_new (tz,
                                  g_date_time_get_year (self->date_end),
                                  g_date_time_get_month (self->date_end),
                                  g_date_time_get_day_of_month (self->date_end) + (single_day && all_day ? 1 : 0),
                                  all_day ? 0 : g_date_time_get_hour (now) + (is_today ? 1 : 0),
                                  all_day ? 0 : g_date_time_get_minute (now),
                                  all_day ? 0 : g_date_time_get_second (now));
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

  event = gcal_event_new (source, component, NULL);
  gcal_event_set_all_day (event, all_day);
  gcal_event_set_timezone (event, tz);

  /* If we clicked edit button, send a signal; otherwise, create the event */
  if (button == self->add_button)
    gcal_manager_create_event (self->manager, event);
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

  g_clear_object (&self->manager);

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

    case PROP_MANAGER:
      g_value_set_object (value, self->manager);
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

    case PROP_MANAGER:
      gcal_quick_add_popover_set_manager (self, g_value_get_object (value));
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
                                   PROP_MANAGER,
                                   g_param_spec_object ("manager",
                                                        "Manager of the application",
                                                        "The singleton manager of the application",
                                                        GCAL_TYPE_MANAGER,
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
 * gcal_quick_add_popover_set_manager:
 * @self: a #GcalQuickAddPopover
 * @manager: a #GcalManager
 *
 * Sets the manager of the popover.
 */
void
gcal_quick_add_popover_set_manager (GcalQuickAddPopover *self,
                                    GcalManager         *manager)
{
  g_return_if_fail (GCAL_IS_QUICK_ADD_POPOVER (self));

  if (g_set_object (&self->manager, manager))
    {
      GList *sources, *l;

      /* Add currently leaded sources */
      sources = gcal_manager_get_sources_connected (manager);

      for (l = sources; l != NULL; l = g_list_next (l))
        on_source_added (manager, l->data, gcal_manager_is_client_writable (manager, l->data), self);

      g_list_free (sources);

      /* Connect to the manager signals and keep the list updates */
      g_signal_connect (manager, "source-added", G_CALLBACK (on_source_added), self);
      g_signal_connect (manager, "source-changed", G_CALLBACK (on_source_changed), self);
      g_signal_connect (manager, "source-removed", G_CALLBACK (on_source_removed), self);
      g_signal_connect_swapped (manager, "notify::default-calendar", G_CALLBACK (update_default_calendar_row), self);

      g_object_notify (G_OBJECT (self), "manager");
    }
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
