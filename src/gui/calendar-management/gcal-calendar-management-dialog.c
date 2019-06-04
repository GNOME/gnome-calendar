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

  GtkWidget          *back_button;
  GtkWidget          *headerbar;
  GtkWidget          *notebook;
  GtkWidget          *stack;

  /* flags */
  GcalCalendarManagementDialogMode mode;
  ESource            *source;
  ESource            *old_default_source;
  GBinding           *title_bind;

  /* auxiliary */
  GSimpleActionGroup *action_group;

  GcalCalendarManagementPage *pages[N_PAGES];

  GcalContext        *context;
};

typedef enum
{
  GCAL_ACCOUNT_TYPE_EXCHANGE,
  GCAL_ACCOUNT_TYPE_GOOGLE,
  GCAL_ACCOUNT_TYPE_OWNCLOUD,
  GCAL_ACCOUNT_TYPE_NOT_SUPPORTED
} GcalAccountType;

static void       action_widget_activated               (GtkWidget            *widget,
                                                         gpointer              user_data);

static void       back_button_clicked                   (GtkButton            *button,
                                                         gpointer              user_data);

static void       calendar_file_selected                (GtkFileChooser       *button,
                                                         gpointer              user_data);

static void       calendar_visible_check_toggled        (GObject             *object,
                                                         GParamSpec          *pspec,
                                                         gpointer             user_data);

static void       on_file_activated                     (GSimpleAction       *action,
                                                         GVariant            *param,
                                                         gpointer             user_data);

static void       on_local_activated                    (GSimpleAction       *action,
                                                         GVariant            *param,
                                                         gpointer             user_data);

static void       on_web_activated                      (GSimpleAction       *action,
                                                         GVariant            *param,
                                                         gpointer             user_data);

static void       response_signal                       (GtkDialog           *dialog,
                                                         gint                 response_id,
                                                         gpointer             user_data);

G_DEFINE_TYPE (GcalCalendarManagementDialog, gcal_calendar_management_dialog, GTK_TYPE_DIALOG)

enum
{
  PROP_0,
  PROP_CONTEXT,
  N_PROPS
};

static GParamSpec *properties[N_PROPS] = { NULL, };

GActionEntry actions[] = {
  {"file",  on_file_activated,  NULL, NULL, NULL},
  {"local", on_local_activated, NULL, NULL, NULL},
  {"web",   on_web_activated,   NULL, NULL, NULL}
};

const gchar*
import_file_extensions[] = {
  ".ical",
  ".ics",
  ".ifb",
  ".icalendar",
  ".vcs"
};

static void
add_button_clicked (GtkWidget *button,
                    gpointer   user_data)
{
  GcalCalendarManagementDialog *self = GCAL_CALENDAR_MANAGEMENT_DIALOG (user_data);
  GcalManager *manager;

  manager = gcal_context_get_manager (self->context);

  if (self->source != NULL)
    {
      /* Commit the new source */
      gcal_manager_save_source (manager, self->source);

      self->source = NULL;

      gcal_calendar_management_dialog_set_mode (GCAL_CALENDAR_MANAGEMENT_DIALOG (user_data),
                                                GCAL_CALENDAR_MANAGEMENT_MODE_NORMAL);
    }
#if 0
  if (self->remote_sources != NULL)
    {
      GList *l;

      /* Commit each new remote source */
      for (l = self->remote_sources; l != NULL; l = l->next)
        gcal_manager_save_source (manager, l->data);

      g_list_free (self->remote_sources);
      self->remote_sources = NULL;

      /* Go back to overview */
      gcal_calendar_management_dialog_set_mode (GCAL_CALENDAR_MANAGEMENT_DIALOG (user_data),
                                                GCAL_CALENDAR_MANAGEMENT_MODE_NORMAL);
    }
#endif
}

static void
action_widget_activated (GtkWidget *widget,
                         gpointer   user_data)
{
  GcalCalendarManagementDialog *self = GCAL_CALENDAR_MANAGEMENT_DIALOG (user_data);
  gint response = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (widget), "response"));

  self->old_default_source = NULL;

  gtk_dialog_response (GTK_DIALOG (user_data), response);
}

static void
back_button_clicked (GtkButton *button,
                     gpointer   user_data)
{
  GcalCalendarManagementDialog *self = GCAL_CALENDAR_MANAGEMENT_DIALOG (user_data);

  gcal_calendar_management_dialog_set_mode (self, GCAL_CALENDAR_MANAGEMENT_MODE_NORMAL);
}

static void
calendar_visible_check_toggled (GObject    *object,
                                GParamSpec *pspec,
                                gpointer    user_data)
{
  GcalCalendarManagementDialog *self = GCAL_CALENDAR_MANAGEMENT_DIALOG (user_data);
  GcalCalendar *calendar;
  GcalManager *manager;

  manager = gcal_context_get_manager (self->context);

  calendar = gcal_manager_get_calendar_from_source (manager, self->source);
  gcal_calendar_set_visible (calendar, gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (object)));
}

static void
response_signal (GtkDialog *dialog,
                 gint       response_id,
                 gpointer   user_data)
{
  GcalCalendarManagementDialog *self = GCAL_CALENDAR_MANAGEMENT_DIALOG (dialog);
  GcalManager *manager;

  manager = gcal_context_get_manager (self->context);

  /* Save the source */
  if (self->mode == GCAL_CALENDAR_MANAGEMENT_MODE_EDIT && self->source != NULL)
    {
      gcal_manager_save_source (manager, self->source);
      g_clear_object (&self->source);
    }
#if 0
  /* Commit the new source; save the current page's source */
  if (self->mode == GCAL_CALENDAR_MANAGEMENT_MODE_NORMAL && response_id == GTK_RESPONSE_APPLY && self->remote_sources != NULL)
      {
        GList *l;

        /* Commit each new remote source */
        for (l = self->remote_sources; l != NULL; l = l->next)
          gcal_manager_save_source (manager, l->data);

        g_list_free (self->remote_sources);
        self->remote_sources = NULL;
      }

  /* Destroy the source when the operation is cancelled */
  if (self->mode == GCAL_CALENDAR_MANAGEMENT_MODE_NORMAL && response_id == GTK_RESPONSE_CANCEL && self->remote_sources != NULL)
    {
      g_list_free_full (self->remote_sources, g_object_unref);
      self->remote_sources = NULL;
    }
#endif
  gtk_widget_hide (GTK_WIDGET (dialog));
}

static gchar*
calendar_path_to_name_suggestion (GFile *file)
{
  g_autofree gchar *unencoded_basename = NULL;
  g_autofree gchar *basename = NULL;
  gchar *ext;
  guint i;

  g_return_val_if_fail (G_IS_FILE (file), NULL);

  unencoded_basename = g_file_get_basename (file);
  basename = g_filename_display_name (unencoded_basename);

  ext = strrchr (basename, '.');

  if (!ext)
    return NULL;

  for (i = 0; i < G_N_ELEMENTS(import_file_extensions); i++)
    {
      if (g_ascii_strcasecmp (import_file_extensions[i], ext) == 0)
        {
           *ext = '\0';
           break;
        }
    }

  g_strdelimit (basename, "-_", ' ');

  return g_steal_pointer (&basename);
}

static void
calendar_file_selected (GtkFileChooser *button,
                        gpointer        user_data)
{
  g_autofree gchar *display_name = NULL;
  g_autoptr (ESource) source = NULL;
  g_autoptr (GFile) file = NULL;
  GcalCalendarManagementDialog *self;
  ESourceExtension *ext;

  self = GCAL_CALENDAR_MANAGEMENT_DIALOG (user_data);
  file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (button));

  if (!file)
    return;

  /**
   * Create the new source and add the needed
   * extensions.
   */
  source = e_source_new (NULL, NULL, NULL);
  e_source_set_parent (source, "local-stub");

  ext = e_source_get_extension (source, E_SOURCE_EXTENSION_CALENDAR);
  e_source_backend_set_backend_name (E_SOURCE_BACKEND (ext), "local");

  ext = e_source_get_extension (source, E_SOURCE_EXTENSION_LOCAL_BACKEND);
  e_source_local_set_custom_file (E_SOURCE_LOCAL (ext), file);

  /* update the source properties */
  display_name = calendar_path_to_name_suggestion (file);
  e_source_set_display_name (source, display_name);

  /* Jump to the edit page */
  gcal_calendar_management_dialog_set_source (GCAL_CALENDAR_MANAGEMENT_DIALOG (user_data), source);
  gcal_calendar_management_dialog_set_mode (GCAL_CALENDAR_MANAGEMENT_DIALOG (user_data), GCAL_CALENDAR_MANAGEMENT_MODE_CREATE);

  //gtk_widget_set_sensitive (self->add_button, TRUE);
}

static void
on_file_activated (GSimpleAction *action,
                   GVariant      *param,
                   gpointer       user_data)
{
  GtkWidget *dialog;
  GtkFileFilter *filter;
  gint response;

  /* Dialog */
  dialog = gtk_file_chooser_dialog_new (_("Select a calendar file"),
                                        GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (user_data))),
                                        GTK_FILE_CHOOSER_ACTION_OPEN,
                                        _("Cancel"), GTK_RESPONSE_CANCEL,
                                        _("Open"), GTK_RESPONSE_OK,
                                        NULL);

  g_signal_connect (dialog, "file-activated", G_CALLBACK (calendar_file_selected), user_data);

  /* File filter */
  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("Calendar files"));
  gtk_file_filter_add_mime_type (filter, "text/calendar");

  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

  response = gtk_dialog_run (GTK_DIALOG (dialog));

  if (response == GTK_RESPONSE_OK)
    calendar_file_selected (GTK_FILE_CHOOSER (dialog), user_data);

  gtk_widget_destroy (dialog);
}

static void
on_local_activated (GSimpleAction *action,
                    GVariant      *param,
                    gpointer       user_data)
{
  GcalCalendarManagementDialog *self;
  ESourceExtension *ext;
  ESource *source;

  self = GCAL_CALENDAR_MANAGEMENT_DIALOG (user_data);
  /**
   * Create the new source and add the needed
   * extensions.
   */
  source = e_source_new (NULL, NULL, NULL);
  e_source_set_parent (source, "local-stub");

  ext = e_source_get_extension (source, E_SOURCE_EXTENSION_CALENDAR);
  e_source_backend_set_backend_name (E_SOURCE_BACKEND (ext), "local");

  /* update the source properties */
  e_source_set_display_name (source, _("Unnamed Calendar"));

  /* Jump to the edit page */
  gcal_calendar_management_dialog_set_source (GCAL_CALENDAR_MANAGEMENT_DIALOG (user_data), source);
  gcal_calendar_management_dialog_set_mode (GCAL_CALENDAR_MANAGEMENT_DIALOG (user_data), GCAL_CALENDAR_MANAGEMENT_MODE_CREATE);

  //gtk_widget_set_sensitive (self->add_button, TRUE);
}

static void
on_web_activated (GSimpleAction *action,
                  GVariant      *param,
                  gpointer       user_data)
{
  gcal_calendar_management_dialog_set_mode (GCAL_CALENDAR_MANAGEMENT_DIALOG (user_data), GCAL_CALENDAR_MANAGEMENT_MODE_CREATE_WEB);
}

static void
set_page (GcalCalendarManagementDialog *self,
          const gchar                  *page_name,
          gpointer                      page_data)
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
      gcal_calendar_management_page_activate (page, page_data);

      gtk_header_bar_set_title (GTK_HEADER_BAR (self->headerbar),
                                gcal_calendar_management_page_get_title (page));
      break;
    }

  gcal_calendar_management_page_deactivate (current_page);
}

/*
 * Callbacks
 */

static void
on_page_switched_cb (GcalCalendarManagementPage   *page,
                     const gchar                  *next_page,
                     gpointer                      page_data,
                     GcalCalendarManagementDialog *self)
{
  GCAL_ENTRY;

  set_page (self, next_page, page_data);

  GCAL_EXIT;
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

static void
gcal_calendar_management_dialog_constructed (GObject *object)
{
  GcalCalendarManagementDialog *self;

  self = GCAL_CALENDAR_MANAGEMENT_DIALOG (object);

  G_OBJECT_CLASS (gcal_calendar_management_dialog_parent_class)->constructed (object);

  /* widget responses */
  gtk_dialog_set_default_response (GTK_DIALOG (object), GTK_RESPONSE_CANCEL);

  /* Action group */
  self->action_group = g_simple_action_group_new ();
  gtk_widget_insert_action_group (GTK_WIDGET (object), "source", G_ACTION_GROUP (self->action_group));

  g_action_map_add_action_entries (G_ACTION_MAP (self->action_group), actions, G_N_ELEMENTS (actions), object);

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
      g_object_notify_by_pspec (object, properties[PROP_CONTEXT]);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_calendar_management_dialog_class_init (GcalCalendarManagementDialogClass *klass)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;

  /**
   * Since we cannot guarantee that the
   * type system registered ESourceLocal,
   * it must be ensured at least here.
   */
  g_type_ensure (E_TYPE_SOURCE_LOCAL);

  object_class = G_OBJECT_CLASS (klass);
  object_class->constructed = gcal_calendar_management_dialog_constructed;
  object_class->get_property = gcal_calendar_management_dialog_get_property;
  object_class->set_property = gcal_calendar_management_dialog_set_property;

  properties[PROP_CONTEXT] = g_param_spec_object ("context",
                                                  "Context",
                                                  "The context object of the application",
                                                  GCAL_TYPE_CONTEXT,
                                                  G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  widget_class = GTK_WIDGET_CLASS (klass);

  /* bind things for/from the template class */
  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/org/gnome/calendar/calendar-management-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalCalendarManagementDialog, back_button);
  gtk_widget_class_bind_template_child (widget_class, GcalCalendarManagementDialog, headerbar);
  gtk_widget_class_bind_template_child (widget_class, GcalCalendarManagementDialog, stack);

  gtk_widget_class_bind_template_callback (widget_class, add_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, action_widget_activated);
  gtk_widget_class_bind_template_callback (widget_class, back_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, calendar_file_selected);
  gtk_widget_class_bind_template_callback (widget_class, calendar_visible_check_toggled);
  gtk_widget_class_bind_template_callback (widget_class, response_signal);
}

static void
gcal_calendar_management_dialog_init (GcalCalendarManagementDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

/**
 * gcal_calendar_management_dialog_set_mode:
 * @dialog: a #GcalCalendarManagementDialog
 * @mode: a #GcalCalendarManagementDialogMode
 *
 * Set the source dialog mode. Creation mode means that a new
 * calendar will be created, while edit mode means a calendar
 * will be edited.
 */
void
gcal_calendar_management_dialog_set_mode (GcalCalendarManagementDialog     *dialog,
                                          GcalCalendarManagementDialogMode  mode)
{
  switch (mode)
    {
    case GCAL_CALENDAR_MANAGEMENT_MODE_CREATE:
      gtk_header_bar_set_title (GTK_HEADER_BAR (dialog->headerbar), _("Add Calendar"));
      gtk_header_bar_set_subtitle (GTK_HEADER_BAR (dialog->headerbar), NULL);
      gtk_stack_set_visible_child (GTK_STACK (dialog->stack), GTK_WIDGET (dialog->pages[GCAL_PAGE_NEW_CALENDAR]));
      break;

    case GCAL_CALENDAR_MANAGEMENT_MODE_CREATE_WEB:
      gtk_header_bar_set_title (GTK_HEADER_BAR (dialog->headerbar), _("Add Calendar"));
      gtk_header_bar_set_subtitle (GTK_HEADER_BAR (dialog->headerbar), NULL);
      gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (dialog->headerbar), FALSE);
      gtk_stack_set_visible_child (GTK_STACK (dialog->stack), GTK_WIDGET (dialog->pages[GCAL_PAGE_NEW_CALENDAR]));
      break;

    case GCAL_CALENDAR_MANAGEMENT_MODE_EDIT:
      gtk_stack_set_visible_child (GTK_STACK (dialog->stack), GTK_WIDGET (dialog->pages[GCAL_PAGE_EDIT_CALENDAR]));
      break;

    case GCAL_CALENDAR_MANAGEMENT_MODE_NORMAL:
      /* Free any bindings left behind */
      g_clear_pointer (&dialog->title_bind, g_binding_unbind);

      gtk_header_bar_set_title (GTK_HEADER_BAR (dialog->headerbar), _("Calendar Settings"));
      gtk_header_bar_set_subtitle (GTK_HEADER_BAR (dialog->headerbar), NULL);
      gtk_stack_set_visible_child (GTK_STACK (dialog->stack), GTK_WIDGET (dialog->pages[GCAL_PAGE_CALENDARS]));
      break;

    default:
      g_assert_not_reached ();
    }
}

/**
 * gcal_calendar_management_dialog_set_source:
 * @dialog: a #GcalCalendarManagementDialog
 * @source: an #ESource
 *
 * Sets the source to be edited by the user.
 */
void
gcal_calendar_management_dialog_set_source (GcalCalendarManagementDialog *dialog,
                                            ESource                      *source)
{
  g_return_if_fail (source && E_IS_SOURCE (source));

  g_set_object (&dialog->source, source);
}
