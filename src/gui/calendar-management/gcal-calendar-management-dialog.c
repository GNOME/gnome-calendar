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
};

static void          on_page_switched_cb                         (GcalCalendarManagementPage   *page,
                                                                  const gchar                  *next_page,
                                                                  GcalCalendar                 *calendar,
                                                                  GcalCalendarManagementDialog *self);

G_DEFINE_TYPE (GcalCalendarManagementDialog, gcal_calendar_management_dialog, ADW_TYPE_DIALOG)


/*
 * Auxiliary methods
 */

static void
set_page (GcalCalendarManagementDialog *self,
          const gchar                  *page_name,
          GcalCalendar                 *calendar)
{
  AdwNavigationPage *page;

  GCAL_TRACE_MSG ("Switching to page '%s'", page_name);

  page = adw_navigation_view_find_page (self->navigation_view, page_name);
  g_assert (GCAL_IS_CALENDAR_MANAGEMENT_PAGE (page));

  gcal_calendar_management_page_activate (GCAL_CALENDAR_MANAGEMENT_PAGE (page), calendar);

  adw_navigation_view_pop (self->navigation_view);

  if (page != adw_navigation_view_get_visible_page (self->navigation_view))
    adw_navigation_view_push (self->navigation_view, page);
}


/*
 * Callbacks
 */

static void
on_navigation_view_popped_cb (AdwNavigationView            *navigation_view,
                              AdwNavigationPage            *popped_page,
                              GcalCalendarManagementDialog *self)
{
  g_assert (GCAL_IS_CALENDAR_MANAGEMENT_PAGE (popped_page));

  gcal_calendar_management_page_deactivate (GCAL_CALENDAR_MANAGEMENT_PAGE (popped_page));
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
gcal_calendar_management_dialog_unmap (GtkWidget *widget)
{
  GcalCalendarManagementDialog *self = GCAL_CALENDAR_MANAGEMENT_DIALOG (widget);

  set_page (self, "calendars", NULL);

  GTK_WIDGET_CLASS (gcal_calendar_management_dialog_parent_class)->unmap (widget);
}


/*
 * GObject overrides
 */

static void
gcal_calendar_management_dialog_class_init (GcalCalendarManagementDialogClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  g_type_ensure (E_TYPE_SOURCE_LOCAL);

  widget_class->unmap = gcal_calendar_management_dialog_unmap;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/gui/calendar-management/gcal-calendar-management-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalCalendarManagementDialog, navigation_view);

  gtk_widget_class_bind_template_callback (widget_class, on_navigation_view_popped_cb);
}

static void
gcal_calendar_management_dialog_init (GcalCalendarManagementDialog *self)
{
  const struct {
    GcalPageType page_type;
    GType        gtype;
  } pages[] = {
    { GCAL_PAGE_CALENDARS, GCAL_TYPE_CALENDARS_PAGE },
    { GCAL_PAGE_NEW_CALENDAR, GCAL_TYPE_NEW_CALENDAR_PAGE },
    { GCAL_PAGE_EDIT_CALENDAR, GCAL_TYPE_EDIT_CALENDAR_PAGE },
  };

  gtk_widget_init_template (GTK_WIDGET (self));

  for (gint i = 0; i < N_PAGES; i++)
    {
      GcalCalendarManagementPage *page;

      page = g_object_new (pages[i].gtype, NULL);

      adw_navigation_view_add (self->navigation_view, ADW_NAVIGATION_PAGE (page));

      g_signal_connect_object (page,
                               "switch-page",
                               G_CALLBACK (on_page_switched_cb),
                               self,
                               0);

      self->pages[i] = page;
    }

  set_page (self, "calendars", NULL);
}

/*
 * Public API
 */

GcalCalendarManagementDialog*
gcal_calendar_management_dialog_new (void)
{
  return g_object_new (GCAL_TYPE_CALENDAR_MANAGEMENT_DIALOG, NULL);
}
