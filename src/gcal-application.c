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

  GtkCssProvider *provider;

  gchar         **css_code_snippets;
  GtkCssProvider *colors_provider;

  icaltimetype   *initial_date;
};

static void     source_added_cb                       (GcalManager         *manager,
                                                       ESource             *source,
                                                       gboolean             enabled,
                                                       gpointer             user_data);

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

G_DEFINE_TYPE_WITH_PRIVATE (GcalApplication, gcal_application, GTK_TYPE_APPLICATION);

static gboolean show_version = FALSE;
static gchar* date = NULL;

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
source_added_cb (GcalManager *manager,
                 ESource     *source,
                 gboolean     enabled,
                 gpointer     user_data)
{
  GcalApplicationPrivate *priv;

  gint i, arr_length = 0;
  ESourceSelectable *extension;
  GQuark color_id;
  const gchar* color_str;

  gchar *css_snippet, *bkg_color, *slanted_edge_both_ltr, *slanted_edge_both_rtl;
  gchar *slanted_edge_start_ltr, *slanted_edge_start_rtl, *slanted_edge_end_ltr, *slanted_edge_end_rtl;
  gchar **new_css_snippets;
  gchar *new_css_data;

  GError *error = NULL;

  priv = GCAL_APPLICATION (user_data)->priv;

  extension = E_SOURCE_SELECTABLE (e_source_get_extension (source, E_SOURCE_EXTENSION_CALENDAR));
  color_id = g_quark_from_string (e_source_selectable_get_color (extension));
  color_str = e_source_selectable_get_color (extension);

  if (priv->css_code_snippets != NULL)
    arr_length = g_strv_length (priv->css_code_snippets);

  new_css_snippets = g_new0 (gchar*, arr_length + 2);

  for (i = 0; i < arr_length; i++)
    new_css_snippets[i] = priv->css_code_snippets[i];

  bkg_color = g_strdup_printf (CSS_TEMPLATE, color_id, color_str);
  slanted_edge_both_ltr =
      g_strdup_printf (CSS_TEMPLATE_EDGE_BOTH, color_id,
                       color_str, color_str, color_str, color_str, color_str, color_str, color_str, color_str,
                       color_str, color_str, color_str, color_str, color_str, color_str, color_str);
  slanted_edge_both_rtl =
      g_strdup_printf (CSS_TEMPLATE_EDGE_BOTH_RTL, color_id,
                       color_str, color_str, color_str, color_str, color_str, color_str, color_str, color_str,
                       color_str, color_str, color_str, color_str, color_str, color_str, color_str);
  slanted_edge_end_ltr =
      g_strdup_printf (CSS_TEMPLATE_EDGE, color_id,
                       color_str, color_str, color_str, color_str, color_str, color_str, color_str, color_str);
  slanted_edge_start_ltr =
      g_strdup_printf (CSS_TEMPLATE_EDGE_START, color_id,
                       color_str, color_str, color_str, color_str, color_str, color_str, color_str, color_str);
  slanted_edge_end_rtl =
      g_strdup_printf (CSS_TEMPLATE_EDGE_RTL, color_id,
                       color_str, color_str, color_str, color_str, color_str, color_str, color_str, color_str);
  slanted_edge_start_rtl =
      g_strdup_printf (CSS_TEMPLATE_EDGE_START_RTL, color_id,
                       color_str, color_str, color_str, color_str, color_str, color_str, color_str, color_str);
  css_snippet = g_strconcat (bkg_color, slanted_edge_both_ltr, slanted_edge_both_rtl,
                             slanted_edge_end_ltr, slanted_edge_start_ltr,
                             slanted_edge_end_rtl, slanted_edge_start_rtl, NULL);
  new_css_snippets[arr_length] = css_snippet;
  g_free (bkg_color);
  g_free (slanted_edge_both_ltr);
  g_free (slanted_edge_both_rtl);
  g_free (slanted_edge_end_ltr);
  g_free (slanted_edge_start_ltr);
  g_free (slanted_edge_start_rtl);
  g_free (slanted_edge_end_rtl);

  if (priv->css_code_snippets != NULL)
    g_free (priv->css_code_snippets); /* shallow free, cause I'm just moving the pointers */
  priv->css_code_snippets = new_css_snippets;

  new_css_data = g_strjoinv ("\n", new_css_snippets);
  error = NULL;
  gtk_css_provider_load_from_data (priv->colors_provider, new_css_data, -1, &error);
  if (error != NULL)
    g_warning ("Error creating custom stylesheet. %s", error->message);

  g_free (new_css_data);
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
}

static void
gcal_application_init (GcalApplication *self)
{
  GcalApplicationPrivate *priv = gcal_application_get_instance_private (self);

  priv->settings = g_settings_new ("org.gnome.calendar");
  priv->colors_provider = gtk_css_provider_new ();

  self->priv = priv;
}

static void
gcal_application_finalize (GObject *object)
{
 GcalApplicationPrivate *priv = GCAL_APPLICATION (object)->priv;

  gtk_style_context_remove_provider_for_screen (gdk_screen_get_default (), GTK_STYLE_PROVIDER (priv->colors_provider));
  gtk_style_context_remove_provider_for_screen (gdk_screen_get_default (), GTK_STYLE_PROVIDER (priv->provider));

  g_clear_object (&(priv->settings));
  g_clear_object (&(priv->colors_provider));
  g_clear_object (&(priv->provider));
  g_clear_object (&(priv->manager));

  if (priv->initial_date != NULL)
    g_free (priv->initial_date);

  if (priv->css_code_snippets != NULL)
    g_strfreev (priv->css_code_snippets);

  G_OBJECT_CLASS (gcal_application_parent_class)->finalize (object);
}

static void
gcal_application_activate (GApplication *application)
{
  GcalApplicationPrivate *priv = GCAL_APPLICATION (application)->priv;

  if (priv->window != NULL)
    {
      gtk_window_present (GTK_WINDOW (priv->window));
    }
  else
    {
      if (priv->initial_date == NULL)
        {
          priv->initial_date = g_new0 (icaltimetype, 1);
          /* FIXME: here read the initial date from somewehere */
          *(priv->initial_date) = icaltime_current_time_with_zone (gcal_manager_get_system_timezone (priv->manager));
          *(priv->initial_date) = icaltime_set_timezone (priv->initial_date,
                                                         gcal_manager_get_system_timezone (priv->manager));
        }

      priv->window = gcal_window_new_with_view_and_date (GCAL_APPLICATION (application),
                                                         g_settings_get_enum (priv->settings, "active-view"),
                                                         priv->initial_date);
      g_settings_bind (priv->settings, "active-view", priv->window, "active-view",
                       G_SETTINGS_BIND_SET | G_SETTINGS_BIND_GET);

      /* FIXME: remove me in favor of gtk_widget_show() */
      gtk_widget_show_all (priv->window);
    }
}

static void
gcal_application_startup (GApplication *app)
{
  GcalApplicationPrivate *priv;
  GFile* css_file;
  GError *error;

  priv = GCAL_APPLICATION (app)->priv;

  G_APPLICATION_CLASS (gcal_application_parent_class)->startup (app);

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

  priv->manager = gcal_manager_new_with_settings (priv->settings);
  g_signal_connect (priv->manager, "source-added", G_CALLBACK (source_added_cb), app);

  gcal_application_set_app_menu (app);
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

  if (date != NULL)
    {
      struct tm result;

      if (e_time_parse_date_and_time (date, &result) == E_TIME_PARSE_OK)
        {
          if (priv->initial_date == NULL)
            priv->initial_date = g_new0 (icaltimetype, 1);
          *(priv->initial_date) = tm_to_icaltimetype (&result, FALSE);

          *(priv->initial_date) = icaltime_set_timezone (priv->initial_date,
                                                         gcal_manager_get_system_timezone (priv->manager));
          print_date ("loading date", priv->initial_date);
        }

      g_clear_pointer (&date, g_free);
    }

  g_option_context_free (context);
  g_strfreev (argv);

  g_application_activate (app);

  return 0;
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

