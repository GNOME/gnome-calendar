/* gcal-new-local-calendar-page.c
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2019 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 * SPDX-FileCopyrightText: 2026 The GNOME Calendar authors
 */

#define G_LOG_DOMAIN "GcalNewLocalCalendarPage"

#include "config.h"

#include "gcal-new-local-calendar-page.h"

#include <glib/gi18n.h>

#include "gcal-calendar-management-page.h"
#include "gcal-context.h"
#include "gcal-debug.h"
#include "gcal-utils.h"

#define ENTRY_PROGRESS_TIMEOUT 100 // ms

typedef enum
{
  ENTRY_STATE_EMPTY,
  ENTRY_STATE_VALIDATING,
  ENTRY_STATE_VALID,
  ENTRY_STATE_INVALID
} EntryState;

struct _GcalNewLocalCalendarPage
{
  AdwNavigationPage   parent;

  GtkWidget          *add_button;
  GtkColorDialogButton *local_calendar_color_button;
  AdwEntryRow        *local_calendar_name_row;

  ESource            *local_source;
};

static void          gcal_calendar_management_page_iface_init    (GcalCalendarManagementPageInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (GcalNewLocalCalendarPage,
                               gcal_new_local_calendar_page,
                               ADW_TYPE_NAVIGATION_PAGE,
                               G_IMPLEMENT_INTERFACE (GCAL_TYPE_CALENDAR_MANAGEMENT_PAGE,
                                                      gcal_calendar_management_page_iface_init))


/*
 * Auxiliary methods
 */

static void
update_add_button (GcalNewLocalCalendarPage *self)
{
  g_autofree gchar *add_button_label = NULL;
  gboolean valid;
  uint32_t n_calendars;

  valid = self->local_source != NULL;
  gtk_widget_set_sensitive (self->add_button, valid);

  n_calendars = 0;
  if (self->local_source)
    n_calendars++;

  if (n_calendars > 0)
    {
      add_button_label = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE,
                                                       "Add Calendar",
                                                       "Add %1$u Calendars",
                                                       n_calendars),
                                          n_calendars);
    }
  else
    {
      add_button_label = g_strdup (C_("button", "Add Calendar"));
    }

  gtk_button_set_label (GTK_BUTTON (self->add_button), add_button_label);
}

static void
update_local_source (GcalNewLocalCalendarPage *self)
{
  g_autofree gchar *calendar_name = NULL;

  g_clear_object (&self->local_source);

  calendar_name = g_strdup (gtk_editable_get_text (GTK_EDITABLE (self->local_calendar_name_row)));
  calendar_name = g_strstrip (calendar_name);

  if (calendar_name && g_utf8_strlen (calendar_name, -1) > 0)
    {
      g_autofree gchar *color_string = NULL;
      ESourceExtension *ext;
      const GdkRGBA *color;
      ESource *source;

      color = gtk_color_dialog_button_get_rgba (self->local_calendar_color_button);
      color_string = gdk_rgba_to_string (color);

      /* Create the new source and add the needed extensions */
      source = e_source_new (NULL, NULL, NULL);
      e_source_set_parent (source, "local-stub");
      e_source_set_display_name (source, calendar_name);

      ext = e_source_get_extension (source, E_SOURCE_EXTENSION_CALENDAR);
      e_source_backend_set_backend_name (E_SOURCE_BACKEND (ext), "local");
      e_source_selectable_set_color (E_SOURCE_SELECTABLE (ext), color_string);

      e_source_backend_set_backend_name (E_SOURCE_BACKEND (ext), "local");

      self->local_source = source;
    }

  update_add_button (self);
}

/*
 * Callbacks
 */

static void
on_add_button_clicked_cb (GtkWidget           *button,
                          GcalNewLocalCalendarPage *self)
{
  GcalContext *context;
  GcalManager *manager;

  context = gcal_application_get_context (GCAL_DEFAULT_APPLICATION);
  manager = gcal_context_get_manager (context);

  /* Commit each new remote source */
  if (self->local_source)
    {
      gcal_manager_save_source (manager, self->local_source);
    }

  gcal_calendar_management_page_switch_page (GCAL_CALENDAR_MANAGEMENT_PAGE (self),
                                             "calendars",
                                             NULL);
}

static void
on_local_calendar_name_row_text_changed_cb (AdwEntryRow         *entry_row,
                                            GParamSpec          *pspec,
                                            GcalNewLocalCalendarPage *self)
{
  update_local_source (self);
}

static void
on_local_calendar_color_button_rgba_changed_cb (GtkColorChooser     *chooser,
                                                GParamSpec          *pspec,
                                                GcalNewLocalCalendarPage *self)
{
  update_local_source (self);
}

/*
 * GcalCalendarManagementPage iface
 */

static void
gcal_new_calendar_page_deactivate (GcalCalendarManagementPage *page)
{
  GcalNewLocalCalendarPage *self;

  GCAL_ENTRY;

  self = GCAL_NEW_LOCAL_CALENDAR_PAGE (page);

  g_clear_object (&self->local_source);
  update_add_button (self);

  gtk_editable_set_text (GTK_EDITABLE (self->local_calendar_name_row), "");

  GCAL_EXIT;
}

static void
gcal_calendar_management_page_iface_init (GcalCalendarManagementPageInterface *iface)
{
  iface->deactivate = gcal_new_calendar_page_deactivate;
}

/*
 * GObject overrides
 */

static void
gcal_new_calendar_page_dispose (GObject *object)
{
  GcalNewLocalCalendarPage *self = (GcalNewLocalCalendarPage *)object;

  gtk_widget_dispose_template (GTK_WIDGET (self), GCAL_TYPE_NEW_LOCAL_CALENDAR_PAGE);

  G_OBJECT_CLASS (gcal_new_local_calendar_page_parent_class)->dispose (object);
}

static void
gcal_new_local_calendar_page_class_init (GcalNewLocalCalendarPageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gcal_new_calendar_page_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/gui/calendar-management/gcal-new-local-calendar-page.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalNewLocalCalendarPage, add_button);
  gtk_widget_class_bind_template_child (widget_class, GcalNewLocalCalendarPage, local_calendar_color_button);
  gtk_widget_class_bind_template_child (widget_class, GcalNewLocalCalendarPage, local_calendar_name_row);

  gtk_widget_class_bind_template_callback (widget_class, on_add_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_local_calendar_name_row_text_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_local_calendar_color_button_rgba_changed_cb);
}

static void
gcal_new_local_calendar_page_init (GcalNewLocalCalendarPage *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
