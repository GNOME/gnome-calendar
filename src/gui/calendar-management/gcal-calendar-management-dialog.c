/* gcal-calendar-management-dialog.c
 *
 * Copyright (C) 2015 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#define G_LOG_DOMAIN "GcalCalendarManagementDialog"

#include "gcal-context.h"
#include "gcal-debug.h"
#include "gcal-calendar-management-dialog.h"
#include "gcal-calendar-management-page.h"
#include "gcal-calendars-page.h"
#include "gcal-edit-calendar-page.h"
#include "gcal-new-calendar-page.h"
#include "gcal-utils.h"

#include <glib/gi18n.h>
#include <libsoup/soup.h>

/**
 * SECTION:gcal-calendar-management-dialog
 * @short_description: Dialog to manage calendars
 * @title:GcalCalendarManagementDialog
 * @stability:unstable
 * @image:gcal-calendar-management-dialog.png
 *
 * #GcalCalendarManagementDialog is the calendar management widget of GNOME
 * Calendar. With it, users can create calendars from local files,
 * local calendars or even import calendars from the Web or their
 * online accounts.
 */

typedef enum
{
  GCAL_PAGE_CALENDARS,
  GCAL_PAGE_NEW_CALENDAR,
  GCAL_PAGE_EDIT_CALENDAR,
  N_PAGES,
} GcalPageType;

struct _GcalCalendarManagementDialog
{
  AdwDialog           parent;

  AdwNavigationView  *navigation_view;

  /* flags */
  ESource            *source;
  ESource            *old_default_source;
  GBinding           *title_bind;

  /* auxiliary */
  GcalCalendarManagementPage *pages[N_PAGES];

  GcalContext        *context;
};

static void          on_page_switched_cb                         (GcalCalendarManagementPage   *page,
                                                                  const gchar                  *next_page,
                                                                  GcalCalendar                 *calendar,
                                                                  GcalCalendarManagementDialog *self);

G_DEFINE_TYPE (GcalCalendarManagementDialog, gcal_calendar_management_dialog, ADW_TYPE_DIALOG)

enum
{
  PROP_0,
  PROP_CONTEXT,
  N_PROPS
};

static GParamSpec *properties[N_PROPS] = { NULL, };


/*
 * Auxiliary methods
 */

static void
set_page (GcalCalendarManagementDialog *self,
          const gchar                  *page_name,
          GcalCalendar                 *calendar)
{
  GcalCalendarManagementPage *next_page;
  AdwNavigationPage *page;

  GCAL_TRACE_MSG ("Switching to page '%s'", page_name);

  page = adw_navigation_view_find_page (self->navigation_view, page_name);
  g_assert (page != NULL);
  next_page = GCAL_CALENDAR_MANAGEMENT_PAGE (adw_navigation_page_get_child (page));

  gcal_calendar_management_page_activate (next_page, calendar);

  adw_navigation_view_pop (self->navigation_view);

  if (page != adw_navigation_view_get_visible_page (self->navigation_view))
    adw_navigation_view_push (self->navigation_view, page);
}

static void
setup_context (GcalCalendarManagementDialog *self)
{
  const struct {
    GcalPageType page_type;
    GType        gtype;
  } pages[] = {
    { GCAL_PAGE_CALENDARS, GCAL_TYPE_CALENDARS_PAGE },
    { GCAL_PAGE_NEW_CALENDAR, GCAL_TYPE_NEW_CALENDAR_PAGE },
    { GCAL_PAGE_EDIT_CALENDAR, GCAL_TYPE_EDIT_CALENDAR_PAGE },
  };
  gint i;

  GCAL_ENTRY;

  for (i = 0; i < N_PAGES; i++)
    {
      GcalCalendarManagementPage *page;
      AdwNavigationPage *navigation_page;

      page = g_object_new (pages[i].gtype,
                           "context", self->context,
                           NULL);

      navigation_page = adw_navigation_page_new (GTK_WIDGET (page),
                                                 gcal_calendar_management_page_get_title (page));
      adw_navigation_page_set_tag (navigation_page, gcal_calendar_management_page_get_name (page));
      adw_navigation_view_add (self->navigation_view, navigation_page);

      g_signal_connect_object (page,
                               "switch-page",
                               G_CALLBACK (on_page_switched_cb),
                               self,
                               0);

      self->pages[i] = page;
    }

  set_page (self, "calendars", NULL);

  GCAL_EXIT;
}


/*
 * Callbacks
 */

static void
on_navigation_view_popped_cb (AdwNavigationView            *navigation_view,
                              AdwNavigationPage            *popped_page,
                              GcalCalendarManagementDialog *self)
{
  GcalCalendarManagementPage *page;

  page = GCAL_CALENDAR_MANAGEMENT_PAGE (adw_navigation_page_get_child (popped_page));
  g_assert (page != NULL);

  gcal_calendar_management_page_deactivate (page);
}

static void
on_page_switched_cb (GcalCalendarManagementPage   *page,
                     const gchar                  *next_page,
                     GcalCalendar                 *calendar,
                     GcalCalendarManagementDialog *self)
{
  GCAL_ENTRY;

  set_page (self, next_page, calendar);

  GCAL_EXIT;
}


/*
 * GtkWidget overrides
 */

static void
gcal_calendar_management_dialog_hide (GtkWidget *widget)
{
  GcalCalendarManagementDialog *self = (GcalCalendarManagementDialog *) widget;

  set_page (self, "calendars", NULL);

  GTK_WIDGET_CLASS (gcal_calendar_management_dialog_parent_class)->hide (widget);
}


/*
 * GObject overrides
 */

static void
gcal_calendar_management_dialog_get_property (GObject    *object,
                                              guint       prop_id,
                                              GValue     *value,
                                              GParamSpec *pspec)
{
  GcalCalendarManagementDialog *self = (GcalCalendarManagementDialog *) object;

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_value_set_object (value, self->context);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_calendar_management_dialog_set_property (GObject      *object,
                                              guint         prop_id,
                                              const GValue *value,
                                              GParamSpec   *pspec)
{
  GcalCalendarManagementDialog *self = (GcalCalendarManagementDialog *) object;

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_assert (self->context == NULL);
      self->context = g_value_dup_object (value);
      setup_context (self);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_calendar_management_dialog_class_init (GcalCalendarManagementDialogClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_ensure (E_TYPE_SOURCE_LOCAL);

  object_class->get_property = gcal_calendar_management_dialog_get_property;
  object_class->set_property = gcal_calendar_management_dialog_set_property;

  widget_class->hide = gcal_calendar_management_dialog_hide;

  properties[PROP_CONTEXT] = g_param_spec_object ("context",
                                                  "Context",
                                                  "The context object of the application",
                                                  GCAL_TYPE_CONTEXT,
                                                  G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/gui/calendar-management/gcal-calendar-management-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalCalendarManagementDialog, navigation_view);

  gtk_widget_class_bind_template_callback (widget_class, on_navigation_view_popped_cb);
}

static void
gcal_calendar_management_dialog_init (GcalCalendarManagementDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
