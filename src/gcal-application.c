/* -*- mode: c; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gcal-application.c
 * Copyright (C) 2015 Erick Pérez Castellanos <erickpc@gnome.org>
 *
 * gnome-calendar is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * gnome-calendar is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gcal-application.h"
#include "css-code.h"
#include "gcal-window.h"
#include "gcal-shell-search-provider.h"

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <glib/gi18n.h>

#define CSS_FILE "resource:///org/gnome/calendar/gtk-styles.css"

struct _GcalApplicationPrivate
{
  GtkWidget      *window;

  GSettings      *settings;
  GcalManager    *manager;

  GcalShellSearchProvider *search_provider;

  GtkCssProvider *provider;
  GtkCssProvider *colors_provider;

  gchar          *uuid;
  icaltimetype   *initial_date;
};

static void     gcal_application_finalize             (GObject                 *object);

static void     gcal_application_activate             (GApplication            *app);

static void     gcal_application_startup              (GApplication            *app);

static gint     gcal_application_command_line         (GApplication            *app,
                                                       GApplicationCommandLine *command_line);

static void     gcal_application_set_app_menu         (GApplication            *app);

static void     gcal_application_create_new_event     (GSimpleAction           *new_event,
                                                       GVariant                *parameter,
                                                       gpointer                 app);

static void     gcal_application_launch_search        (GSimpleAction           *search,
                                                       GVariant                *parameter,
                                                       gpointer                 app);

static void     gcal_application_show_about           (GSimpleAction           *simple,
                                                       GVariant                *parameter,
                                                       gpointer                 user_data);

static void     gcal_application_sync                 (GSimpleAction           *sync,
                                                       GVariant                *parameter,
                                                       gpointer                 app);

static void     gcal_application_quit                 (GSimpleAction           *simple,
                                                       GVariant                *parameter,
                                                       gpointer                 user_data);

static gboolean gcal_application_dbus_register       (GApplication             *application,
                                                      GDBusConnection          *connection,
                                                      const gchar              *object_path,
                                                      GError                  **error);

static void     gcal_application_dbus_unregister     (GApplication             *application,
                                                      GDBusConnection          *connection,
                                                      const gchar              *object_path);

G_DEFINE_TYPE_WITH_PRIVATE (GcalApplication, gcal_application, GTK_TYPE_APPLICATION);

static gboolean show_version = FALSE;
static gchar* date = NULL;
static gchar* uuid = NULL;

static GOptionEntry gcal_application_goptions[] = {
  {
    "version", 'v', 0,
    G_OPTION_ARG_NONE, &show_version,
    N_("Display version number"), NULL
  },
  {
    "date", 'd', 0,
    G_OPTION_ARG_STRING, &date,
    N_("Open calendar on the passed date"), NULL
  },
  {
    "uuid", 'u', 0,
    G_OPTION_ARG_STRING, &uuid,
    N_("Open calendar showing the passed event"), NULL
  },
  { NULL }
};

static const GActionEntry gcal_app_entries[] = {
  { "new",    gcal_application_create_new_event },
  { "sync",   gcal_application_sync },
  { "search", gcal_application_launch_search },
  { "about",  gcal_application_show_about },
  { "quit",   gcal_application_quit },
};

static void
process_sources (GcalApplication *application)
{
  GcalApplicationPrivate *priv;

  GList *sources, *l;
  ESource *source;

  gint arr_length, i = 0;
  GQuark color_id;
  GdkRGBA color;
  gchar* color_str;

  gchar **new_css_snippets;
  gchar *new_css_data;

  GError *error = NULL;

  priv = application->priv;

  sources = gcal_manager_get_sources_connected (priv->manager);
  arr_length = g_list_length (sources);
  new_css_snippets = g_new0 (gchar*, arr_length + 2);
  for (l = sources; l != NULL; l = g_list_next (l), i++)
    {
      source = l->data;

      get_color_name_from_source (source, &color);
      color_str = gdk_rgba_to_string (&color);
      color_id = g_quark_from_string (color_str);

      new_css_snippets[i] = g_strdup_printf (CSS_TEMPLATE, color_id, color_str);

      g_free (color_str);
    }

  g_list_free (sources);

  new_css_data = g_strjoinv ("\n", new_css_snippets);
  g_strfreev (new_css_snippets);

  error = NULL;
  gtk_css_provider_load_from_data (priv->colors_provider, new_css_data, -1, &error);
  if (error != NULL)
    g_warning ("Error creating custom stylesheet. %s", error->message);

  g_free (new_css_data);
}

static void
sources_added_cb (GcalApplication *application,
                  ESource         *source,
                  gboolean         enabled,
                  GcalManager     *manager)
{
  process_sources (application);
}

static void
gcal_application_class_init (GcalApplicationClass *klass)
{
  GObjectClass *object_class;
  GApplicationClass *application_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = gcal_application_finalize;

  application_class = G_APPLICATION_CLASS (klass);
  application_class->activate = gcal_application_activate;
  application_class->startup = gcal_application_startup;
  application_class->command_line = gcal_application_command_line;

  application_class->dbus_register = gcal_application_dbus_register;
  application_class->dbus_unregister = gcal_application_dbus_unregister;
}

static void
gcal_application_init (GcalApplication *self)
{
  GcalApplicationPrivate *priv = gcal_application_get_instance_private (self);

  priv->settings = g_settings_new ("org.gnome.calendar");
  priv->colors_provider = gtk_css_provider_new ();

  priv->manager = gcal_manager_new_with_settings (priv->settings);
  g_signal_connect_swapped (priv->manager, "source-added", G_CALLBACK (sources_added_cb), self);

  priv->search_provider = gcal_shell_search_provider_new ();
  gcal_shell_search_provider_connect (priv->search_provider, priv->manager);

  self->priv = priv;
}

static void
gcal_application_finalize (GObject *object)
{
 GcalApplicationPrivate *priv = GCAL_APPLICATION (object)->priv;

  g_free (priv->uuid);
  if (priv->initial_date != NULL)
    g_free (priv->initial_date);

  if (priv->provider != NULL)
    {
      gtk_style_context_remove_provider_for_screen (gdk_screen_get_default (), GTK_STYLE_PROVIDER (priv->colors_provider));

      gtk_style_context_remove_provider_for_screen (gdk_screen_get_default (), GTK_STYLE_PROVIDER (priv->provider));
      g_clear_object (&(priv->provider));
    }

  g_clear_object (&(priv->colors_provider));
  g_clear_object (&(priv->settings));

  g_clear_object (&(priv->manager));

  G_OBJECT_CLASS (gcal_application_parent_class)->finalize (object);
  g_clear_object (&(priv->search_provider));
}

static void
gcal_application_activate (GApplication *application)
{
  GcalApplicationPrivate *priv;
  GFile* css_file;
  GError *error;

  priv = GCAL_APPLICATION (application)->priv;

  if (priv->provider == NULL)
   {
     priv->provider = gtk_css_provider_new ();
     gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                                GTK_STYLE_PROVIDER (priv->provider),
                                                GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 1);

     error = NULL;
     css_file = g_file_new_for_uri (CSS_FILE);
     gtk_css_provider_load_from_file (priv->provider, css_file, &error);
     if (error != NULL)
         g_warning ("Error loading stylesheet from file %s. %s", CSS_FILE, error->message);

     g_object_set (gtk_settings_get_default (), "gtk-application-prefer-dark-theme", FALSE, NULL);

     g_object_unref (css_file);
   }

  if (priv->colors_provider != NULL)
    {
      gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                                 GTK_STYLE_PROVIDER (priv->colors_provider),
                                                 GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 2);
    }

  gcal_application_set_app_menu (application);

  if (priv->window != NULL)
    {
      gtk_window_present (GTK_WINDOW (priv->window));
      if (priv->initial_date != NULL)
        g_object_set (priv->window, "active-date", priv->initial_date, NULL);
    }
  else
    {
      if (priv->initial_date == NULL)
        {
          priv->initial_date = g_new0 (icaltimetype, 1);
          *(priv->initial_date) = icaltime_current_time_with_zone (gcal_manager_get_system_timezone (priv->manager));
          *(priv->initial_date) = icaltime_set_timezone (priv->initial_date,
                                                         gcal_manager_get_system_timezone (priv->manager));
        }

      priv->window = gcal_window_new_with_view_and_date (GCAL_APPLICATION (application),
                                                         g_settings_get_enum (priv->settings, "active-view"),
                                                         priv->initial_date);
      g_settings_bind (priv->settings, "active-view", priv->window, "active-view",
                       G_SETTINGS_BIND_SET | G_SETTINGS_BIND_GET);

      gtk_widget_show (priv->window);
    }

    g_clear_pointer (&(priv->initial_date), g_free);
    if (priv->uuid != NULL)
      {
        gcal_window_open_event_by_uuid (GCAL_WINDOW (priv->window), priv->uuid);
        g_clear_pointer (&(priv->uuid), g_free);
      }
}

static void
gcal_application_startup (GApplication *app)
{
  G_APPLICATION_CLASS (gcal_application_parent_class)->startup (app);

  /* We're assuming the application is called as a service only by the shell search system */
  if ((g_application_get_flags (app) & G_APPLICATION_IS_SERVICE) != 0)
    g_application_set_inactivity_timeout (app, 3 * 60 * 1000);
}

static gint
gcal_application_command_line (GApplication            *app,
                               GApplicationCommandLine *command_line)
{
  GcalApplicationPrivate *priv;

  GOptionContext *context;
  gchar **argv;
  GError *error;
  gint argc;

  priv = GCAL_APPLICATION (app)->priv;
  argv = g_application_command_line_get_arguments (command_line, &argc);
  context = g_option_context_new (N_("- Calendar management"));
  g_option_context_add_main_entries (context, gcal_application_goptions, GETTEXT_PACKAGE);

  error = NULL;
  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_critical ("Failed to parse argument: %s", error->message);
      g_error_free (error);
      g_option_context_free (context);
      return 1;
    }

  if (show_version)
    {
      g_print ("gnome-calendar: Version %s\n", PACKAGE_VERSION);
      return 0;
    }

  if (uuid != NULL)
    {
      gcal_application_set_uuid (GCAL_APPLICATION (app), uuid);
      g_clear_pointer (&uuid, g_free);
    }
  else if (date != NULL)
    {
      struct tm result;

      if (e_time_parse_date_and_time (date, &result) == E_TIME_PARSE_OK)
        {
          if (priv->initial_date == NULL)
            priv->initial_date = g_new0 (icaltimetype, 1);
          *(priv->initial_date) = tm_to_icaltimetype (&result, FALSE);

          *(priv->initial_date) = icaltime_set_timezone (priv->initial_date,
                                                         gcal_manager_get_system_timezone (priv->manager));
        }

      g_clear_pointer (&date, g_free);
    }

  g_option_context_free (context);
  g_strfreev (argv);

  g_application_activate (app);

  return 0;
}

static gboolean
gcal_application_dbus_register (GApplication    *application,
                                GDBusConnection *connection,
                                const gchar     *object_path,
                                GError         **error)
{
  GcalApplicationPrivate *priv;
  gchar *search_provider_path = NULL;
  gboolean ret_val = FALSE;

  priv = GCAL_APPLICATION (application)->priv;

  if (!G_APPLICATION_CLASS (gcal_application_parent_class)->dbus_register (application, connection, object_path, error))
    goto out;

  search_provider_path = g_strconcat (object_path, "/SearchProvider", NULL);
  if (!gcal_shell_search_provider_dbus_export (priv->search_provider, connection, search_provider_path, error))
    goto out;

  ret_val = TRUE;

out:
  g_free (search_provider_path);
  return ret_val;
}

static void
gcal_application_dbus_unregister (GApplication *application,
                                  GDBusConnection *connection,
                                  const gchar *object_path)
{
  GcalApplicationPrivate *priv;
  gchar *search_provider_path = NULL;

  priv = GCAL_APPLICATION (application)->priv;

  search_provider_path = g_strconcat (object_path, "/SearchProvider", NULL);
  gcal_shell_search_provider_dbus_unexport (priv->search_provider, connection, search_provider_path);

  G_APPLICATION_CLASS (gcal_application_parent_class)->dbus_unregister (application, connection, object_path);

  g_free (search_provider_path);
}

static void
gcal_application_set_app_menu (GApplication *app)
{
  GtkBuilder *builder;
  GMenuModel *appmenu;

  builder = gtk_builder_new ();
  gtk_builder_add_from_resource (builder, "/org/gnome/calendar/menus.ui", NULL);

  appmenu = (GMenuModel *)gtk_builder_get_object (builder, "appmenu");

  g_action_map_add_action_entries (G_ACTION_MAP (app),
                                   gcal_app_entries,
                                   G_N_ELEMENTS (gcal_app_entries),
                                   app);

  gtk_application_set_app_menu (GTK_APPLICATION (app), appmenu);

  g_object_unref (builder);
}

static void
gcal_application_create_new_event (GSimpleAction *new_event,
                                   GVariant      *parameter,
                                   gpointer       app)
{
  GcalApplicationPrivate *priv = GCAL_APPLICATION (app)->priv;
  gcal_window_new_event (GCAL_WINDOW (priv->window));
}

static void
gcal_application_sync (GSimpleAction *sync,
                       GVariant      *parameter,
                       gpointer       app)
{
  GcalApplicationPrivate *priv = GCAL_APPLICATION (app)->priv;
  gcal_manager_refresh (priv->manager);
}

static void
gcal_application_launch_search (GSimpleAction *search,
                                GVariant      *parameter,
                                gpointer       app)
{
  GcalApplicationPrivate *priv = GCAL_APPLICATION (app)->priv;
  gcal_window_set_search_mode (GCAL_WINDOW (priv->window), TRUE);
}

static void
gcal_application_show_about (GSimpleAction *simple,
                             GVariant      *parameter,
                             gpointer       user_data)
{
  GcalApplicationPrivate *priv;
  char *copyright;
  GDateTime *date;
  int created_year = 2012;
  const gchar *authors[] = {
    "Erick Pérez Castellanos <erickpc@gnome.org>",
    "Georges Basile Stavracas Neto <georges.stavracas@gmail.com>",
    NULL
  };
  const gchar *artists[] = {
    "Jakub Steiner <jimmac@gmail.com>",
    "Lapo Calamandrei <calamandrei@gmail.com>",
    "Reda Lazri <the.red.shortcut@gmail.com>",
    "William Jon McCann <jmccann@redhat.com>",
    NULL
  };

  priv = GCAL_APPLICATION (user_data)->priv;
  date = g_date_time_new_now_local ();

  if (g_date_time_get_year (date) == created_year)
    {
      copyright = g_strdup_printf (_("Copyright \xC2\xA9 %Id "
                                     "The Calendar authors"),
                                   created_year);
    }
  else
    {
      copyright = g_strdup_printf (_("Copyright \xC2\xA9 %Id\xE2\x80\x93%Id "
                                     "The Calendar authors"),
                                   created_year, g_date_time_get_year (date));
    }

  gtk_show_about_dialog (GTK_WINDOW (priv->window),
                         "program-name", _("Calendar"),
                         "version", VERSION,
                         "copyright", copyright,
                         "license-type", GTK_LICENSE_GPL_3_0,
                         "authors", authors,
                         "artists", artists,
                         "logo-icon-name", "gnome-calendar",
                         "translator-credits", _("translator-credits"),
                         NULL);
  g_free (copyright);
  g_date_time_unref (date);
}

static void
gcal_application_quit (GSimpleAction *simple,
                       GVariant      *parameter,
                       gpointer       user_data)
{
  GcalApplicationPrivate *priv = GCAL_APPLICATION (user_data)->priv;

  gtk_widget_destroy (priv->window);
}

/* Public API */
GcalApplication*
gcal_application_new (void)
{
  g_set_application_name ("Calendar");

  return g_object_new (gcal_application_get_type (),
                       "application-id", "org.gnome.Calendar",
                       "flags", G_APPLICATION_HANDLES_COMMAND_LINE,
                       NULL);
}

GcalManager*
gcal_application_get_manager (GcalApplication *app)
{
  return app->priv->manager;
}

GSettings*
gcal_application_get_settings (GcalApplication *app)
{
  return app->priv->settings;
}

void
gcal_application_set_uuid (GcalApplication *application,
                           const gchar     *uuid)
{
  GcalApplicationPrivate *priv = application->priv;

  g_free (priv->uuid);
  priv->uuid = g_strdup (uuid);
}

void
gcal_application_set_initial_date (GcalApplication *application,
                                   const icaltimetype *date)
{
  GcalApplicationPrivate *priv = application->priv;

  g_free (priv->initial_date);
  priv->initial_date = gcal_dup_icaltime (date);
}
