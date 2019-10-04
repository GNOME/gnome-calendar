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
#include <goa/goa.h>
#include <libedataserverui/libedataserverui.h>
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
  GtkDialog           parent;

  GtkWidget          *headerbar;
  GtkWidget          *notebook;
  GtkWidget          *stack;

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

G_DEFINE_TYPE (GcalCalendarManagementDialog, gcal_calendar_management_dialog, GTK_TYPE_DIALOG)

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
  GcalCalendarManagementPage *current_page;
  guint i;

  GCAL_TRACE_MSG ("Switching to page '%s'", page_name);

  current_page = (GcalCalendarManagementPage*) gtk_stack_get_visible_child (GTK_STACK (self->stack));

  for (i = 0; i < N_PAGES; i++)
    {
      GcalCalendarManagementPage *page = self->pages[i];

      if (g_strcmp0 (page_name, gcal_calendar_management_page_get_name (page)) != 0)
        continue;

      gtk_stack_set_visible_child (GTK_STACK (self->stack), GTK_WIDGET (page));
      gcal_calendar_management_page_activate (page, calendar);

      gtk_header_bar_set_title (GTK_HEADER_BAR (self->headerbar),
                                gcal_calendar_management_page_get_title (page));
      break;
    }

  gcal_calendar_management_page_deactivate (current_page);
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

      page = g_object_new (pages[i].gtype,
                           "context", self->context,
                           NULL);
      gtk_widget_show (GTK_WIDGET (page));

      gtk_stack_add_titled (GTK_STACK (self->stack),
                            GTK_WIDGET (page),
                            gcal_calendar_management_page_get_name (page),
                            gcal_calendar_management_page_get_title (page));

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
on_page_switched_cb (GcalCalendarManagementPage   *page,
                     const gchar                  *next_page,
                     GcalCalendar                 *calendar,
                     GcalCalendarManagementDialog *self)
{
  GCAL_ENTRY;

  set_page (self, next_page, calendar);

  GCAL_EXIT;
}

static void
on_dialog_response_signal_cb (GtkDialog                    *dialog,
                              gint                          response_id,
                              GcalCalendarManagementDialog *self)
{
  set_page (self, "calendars", NULL);
  gtk_widget_hide (GTK_WIDGET (dialog));
}


/*
 * GObject overrides
 */

static void
gcal_calendar_management_dialog_constructed (GObject *object)
{
  GcalCalendarManagementDialog *self;

  self = GCAL_CALENDAR_MANAGEMENT_DIALOG (object);

  G_OBJECT_CLASS (gcal_calendar_management_dialog_parent_class)->constructed (object);

  /* widget responses */
  gtk_dialog_set_default_response (GTK_DIALOG (object), GTK_RESPONSE_CANCEL);

  /* setup titlebar */
  gtk_window_set_titlebar (GTK_WINDOW (object), self->headerbar);
}

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

  object_class->constructed = gcal_calendar_management_dialog_constructed;
  object_class->get_property = gcal_calendar_management_dialog_get_property;
  object_class->set_property = gcal_calendar_management_dialog_set_property;

  properties[PROP_CONTEXT] = g_param_spec_object ("context",
                                                  "Context",
                                                  "The context object of the application",
                                                  GCAL_TYPE_CONTEXT,
                                                  G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/org/gnome/calendar/calendar-management-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalCalendarManagementDialog, headerbar);
  gtk_widget_class_bind_template_child (widget_class, GcalCalendarManagementDialog, stack);

  gtk_widget_class_bind_template_callback (widget_class, on_dialog_response_signal_cb);
}

static void
gcal_calendar_management_dialog_init (GcalCalendarManagementDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
