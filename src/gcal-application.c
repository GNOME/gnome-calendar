/* -*- mode: c; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gcal-application.c
 * Copyright (C) 2012 Erick Pérez Castellanos <erickpc@gnome.org>
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
#include "gcal-window.h"

#include <glib/gi18n.h>

#define CSS_FILE UI_DATA_DIR "/gtk-styles.css"

struct _GcalApplicationPrivate
{
  GtkWidget      *window;

  GSimpleAction  *view;
  GSettings      *settings;
  GcalManager    *manager;

  GtkCssProvider *provider;
};

static void     gcal_application_finalize             (GObject                 *object);

static void     gcal_application_activate             (GApplication            *app);

static void     gcal_application_startup              (GApplication            *app);

static gint     gcal_application_command_line         (GApplication            *app,
                                                       GApplicationCommandLine *command_line);

static void     gcal_application_set_app_menu         (GApplication            *app);

static void     gcal_application_create_new_event     (GSimpleAction           *new_event,
                                                       GVariant                *parameter,
                                                       gpointer                *app);

static void     gcal_application_launch_search        (GSimpleAction           *search,
                                                       GVariant                *parameter,
                                                       gpointer                *app);

static void     gcal_application_change_view          (GSimpleAction           *simple,
                                                       GVariant                *parameter,
                                                       gpointer                 user_data);

static void     gcal_application_show_about           (GSimpleAction           *simple,
                                                       GVariant                *parameter,
                                                       gpointer                 user_data);

static void     gcal_application_quit                 (GSimpleAction           *simple,
                                                       GVariant                *parameter,
                                                       gpointer                 user_data);

static void     gcal_application_changed_view         (GSettings               *settings,
                                                       gchar                   *key,
                                                       gpointer                 user_data);

static gboolean gcal_application_window_state_changed (GtkWidget               *widget,
                                                       GdkEvent                *event,
                                                       gpointer                 user_data);

static gboolean gcal_application_window_configured    (GtkWidget               *widget,
                                                       GdkEvent                *event,
                                                       gpointer                 user_data);

G_DEFINE_TYPE (GcalApplication, gcal_application, GTK_TYPE_APPLICATION);

static gboolean show_version = FALSE;

static GOptionEntry gcal_application_goptions[] = {
  {
    "version", 'v', 0,
    G_OPTION_ARG_NONE, &show_version,
    N_("Display version number"), NULL
  },
  { NULL }
};

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

  g_type_class_add_private ((gpointer) klass, sizeof(GcalApplicationPrivate));
}

static void
gcal_application_init (GcalApplication *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            GCAL_TYPE_APPLICATION,
                                            GcalApplicationPrivate);
}

static void
gcal_application_finalize (GObject *object)
{
  GcalApplicationPrivate *priv;

  g_return_if_fail (GCAL_IS_APPLICATION (object));
  priv = GCAL_APPLICATION (object)->priv;

  g_clear_object (&(priv->settings));

  if (G_OBJECT_CLASS (gcal_application_parent_class)->finalize != NULL)
    G_OBJECT_CLASS (gcal_application_parent_class)->finalize (object);
}

static void
gcal_application_activate (GApplication *application)
{
  GcalApplicationPrivate *priv;
  priv = GCAL_APPLICATION (application)->priv;

  if (priv->window != NULL)
    {
      gtk_window_present (GTK_WINDOW (priv->window));
    }
  else
    {
      GVariant *variant;
      gboolean maximized;
      const gint32 *position;
      const gint32 *size;
      gsize n_elements;

      priv->window =
        gcal_window_new_with_view (GCAL_APPLICATION (application),
                                   g_settings_get_enum (priv->settings,
                                                        "active-view"));
      g_settings_bind (priv->settings,
                       "active-view",
                       priv->window,
                       "active-view",
                       G_SETTINGS_BIND_SET | G_SETTINGS_BIND_GET);

      g_object_bind_property (priv->window,
                              "new-event-mode",
                              priv->view,
                              "enabled",
                              G_BINDING_DEFAULT | G_BINDING_INVERT_BOOLEAN);

      variant = g_settings_get_value (priv->settings, "window-size");
      size = g_variant_get_fixed_array (variant, &n_elements, sizeof (gint32));
      if (n_elements == 2)
        gtk_window_set_default_size (GTK_WINDOW (priv->window), size[0], size[1]);
      g_variant_unref (variant);

      variant = g_settings_get_value (priv->settings, "window-position");
      position = g_variant_get_fixed_array (variant, &n_elements, sizeof (gint32));
      if (n_elements == 2)
        gtk_window_move (GTK_WINDOW (priv->window), position[0], position[1]);
      g_variant_unref (variant);

      maximized = g_settings_get_boolean (priv->settings, "window-maximized");
      if (maximized)
        gtk_window_maximize (GTK_WINDOW (priv->window));

      gtk_widget_show_all (priv->window);

      g_signal_connect (priv->window,
                        "window-state-event",
                        G_CALLBACK (gcal_application_window_state_changed),
                        application);

      g_signal_connect (priv->window,
                        "configure-event",
                        G_CALLBACK (gcal_application_window_configured),
                        application);
    }
}

static void
gcal_application_startup (GApplication *app)
{
  GcalApplicationPrivate *priv;
  GError *error;
  priv = GCAL_APPLICATION (app)->priv;

  G_APPLICATION_CLASS (gcal_application_parent_class)->startup (app);

  if (priv->provider == NULL)
   {
     priv->provider = gtk_css_provider_new ();
     gtk_style_context_add_provider_for_screen (
         gdk_screen_get_default (),
         GTK_STYLE_PROVIDER (priv->provider),
         G_MAXUINT);

     error = NULL;
     gtk_css_provider_load_from_path (priv->provider, CSS_FILE, &error);
     if (error != NULL)
       {
         g_warning ("Error loading stylesheet from file %s. %s",
                    CSS_FILE,
                    error->message);
       }

     g_object_set (gtk_settings_get_default (),
                   "gtk-application-prefer-dark-theme", FALSE,
                   NULL);
   }

  priv->manager = gcal_manager_new ();

  gcal_application_set_app_menu (app);
}

static gint
gcal_application_command_line (GApplication            *app,
                               GApplicationCommandLine *command_line)
{
  GOptionContext *context;
  gchar **argv;
  GError *error;
  gint argc;

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

  g_option_context_free (context);
  g_strfreev (argv);

  g_application_activate (app);

  return 0;
}

static void
gcal_application_set_app_menu (GApplication *app)
{
  GcalApplicationPrivate *priv;

  GMenu *app_menu;
  GMenu *view_as;
  GSimpleAction *new_event;
  GSimpleAction *search;
  GSimpleAction *about;
  GSimpleAction *quit;

  g_return_if_fail (GCAL_IS_APPLICATION (app));
  priv = GCAL_APPLICATION (app)->priv;

  app_menu = g_menu_new ();

  new_event = g_simple_action_new ("new_event", NULL);
  g_signal_connect (new_event, "activate",
                    G_CALLBACK (gcal_application_create_new_event), app);
  g_action_map_add_action ( G_ACTION_MAP (app), G_ACTION (new_event));
  g_menu_append (app_menu, _("New Event"), "app.new_event");

  search = g_simple_action_new ("search", NULL);
  g_signal_connect (search, "activate",
                    G_CALLBACK (gcal_application_launch_search), app);
  g_action_map_add_action ( G_ACTION_MAP (app), G_ACTION (search));
  g_menu_append (app_menu, _("Search"), "app.search");

  priv->view = g_simple_action_new_stateful (
      "view",
      G_VARIANT_TYPE_STRING,
      g_settings_get_value (priv->settings, "active-view"));

  g_signal_connect (priv->view, "activate",
                    G_CALLBACK (gcal_application_change_view), app);
  g_action_map_add_action ( G_ACTION_MAP (app), G_ACTION (priv->view));

  view_as = g_menu_new ();
  g_menu_append (view_as, _("Day"), "app.view::day");
  g_menu_append (view_as, _("Week"), "app.view::week");
  g_menu_append (view_as, _("Month"), "app.view::month");
  g_menu_append (view_as, _("Year"), "app.view::year");

  g_menu_append_section (app_menu, _("View as"), G_MENU_MODEL (view_as));

  about = g_simple_action_new ("about", NULL);
  g_signal_connect (about, "activate",
                    G_CALLBACK (gcal_application_show_about), app);
  g_action_map_add_action ( G_ACTION_MAP (app), G_ACTION (about));
  g_menu_append (app_menu, _("About"), "app.about");

  quit = g_simple_action_new ("quit", NULL);
  g_signal_connect (quit, "activate",
                    G_CALLBACK (gcal_application_quit), app);
  g_action_map_add_action ( G_ACTION_MAP (app), G_ACTION (quit));
  g_menu_append (app_menu, _("Quit"), "app.quit");

  gtk_application_set_app_menu (GTK_APPLICATION (app), G_MENU_MODEL (app_menu));

  /* Accelerators */
  gtk_application_add_accelerator (GTK_APPLICATION (app), "<Primary>n", "app.new_event", NULL);
  gtk_application_add_accelerator (GTK_APPLICATION (app), "<Primary>f", "app.search", NULL);
  gtk_application_add_accelerator (GTK_APPLICATION (app), "<Primary>q", "app.quit", NULL);
}

static void
gcal_application_create_new_event (GSimpleAction *new_event,
                                   GVariant      *parameter,
                                   gpointer      *app)
{
  GcalApplicationPrivate *priv;

  priv = GCAL_APPLICATION (app)->priv;

  gcal_window_new_event (GCAL_WINDOW (priv->window));
}

static void
gcal_application_launch_search (GSimpleAction *search,
                                GVariant      *parameter,
                                gpointer      *app)
{
  GcalApplicationPrivate *priv;

  priv = GCAL_APPLICATION (app)->priv;

  gcal_window_set_search_mode (GCAL_WINDOW (priv->window), TRUE);
}

static void
gcal_application_change_view (GSimpleAction *simple,
                              GVariant      *parameter,
                              gpointer       user_data)
{
  GcalApplicationPrivate *priv;

  g_return_if_fail (GCAL_IS_APPLICATION (user_data));
  priv = GCAL_APPLICATION (user_data)->priv;

  g_settings_set_value (priv->settings, "active-view", parameter);
}

static void
gcal_application_show_about (GSimpleAction *simple,
                             GVariant      *parameter,
                             gpointer       user_data)
{
  GcalApplication *app = GCAL_APPLICATION (user_data);
  char *copyright;
  GDateTime *date;
  int created_year = 2012;
  const gchar *authors[] = {
    "Erick Pérez Castellanos <erickpc@gnome.org>",
    NULL
  };
  const gchar *artists[] = {
    "Reda Lazri <the.red.shortcut@gmail.com>",
    "William Jon McCann <jmccann@redhat.com>",
    NULL
  };

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

  gtk_show_about_dialog (GTK_WINDOW (app->priv->window),
                         "program-name", _("Calendar"),
                         "version", VERSION,
                         "copyright", copyright,
                         "license-type", GTK_LICENSE_GPL_3_0,
                         "authors", authors,
                         "artists", artists,
                         "logo-icon-name", "x-office-calendar",
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
  GApplication *app = G_APPLICATION (user_data);

  g_application_quit (app);
}

static void
gcal_application_changed_view (GSettings *settings,
                               gchar     *key,
                               gpointer   user_data)
{
  GcalApplicationPrivate *priv;

  g_return_if_fail (GCAL_IS_APPLICATION (user_data));
  priv = GCAL_APPLICATION (user_data)->priv;
  g_simple_action_set_state (priv->view,
                             g_settings_get_value (priv->settings,
                                                   "active-view"));
}

static gboolean
gcal_application_window_state_changed (GtkWidget *widget,
                                       GdkEvent  *event,
                                       gpointer   user_data)
{
  GcalApplicationPrivate *priv;
  GdkWindowState state;
  gboolean maximized;

  priv = GCAL_APPLICATION (user_data)->priv;
  state = gdk_window_get_state (gtk_widget_get_window (widget));
  maximized = state & GDK_WINDOW_STATE_MAXIMIZED;

  g_settings_set_boolean (priv->settings, "window-maximized", maximized);

  return FALSE;
}

static gboolean
gcal_application_window_configured (GtkWidget *widget,
                                    GdkEvent  *event,
                                    gpointer   user_data)
{
  GcalApplicationPrivate *priv;
  GVariant *variant;
  GdkWindowState state;
  gint32 size[2];
  gint32 position[2];

  priv = GCAL_APPLICATION (user_data)->priv;
  state = gdk_window_get_state (gtk_widget_get_window (widget));
  if (state & GDK_WINDOW_STATE_MAXIMIZED)
    return FALSE;

  gtk_window_get_size (GTK_WINDOW (priv->window),
                       (gint *) &size[0],
                       (gint *) &size[1]);
  variant = g_variant_new_fixed_array (G_VARIANT_TYPE_INT32,
                                       size, 2,
                                       sizeof (size[0]));
  g_settings_set_value (priv->settings, "window-size", variant);

  gtk_window_get_position (GTK_WINDOW (priv->window),
                           (gint *) &position[0],
                           (gint *) &position[1]);
  variant = g_variant_new_fixed_array (G_VARIANT_TYPE_INT32,
                                       position, 2,
                                       sizeof (position[0]));
  g_settings_set_value (priv->settings, "window-position", variant);

  return FALSE;
}

/* Public API */
GcalApplication*
gcal_application_new (void)
{
  GcalApplication *app;

  g_set_application_name ("Calendar");

  app = g_object_new (gcal_application_get_type (),
                      "application-id", "org.gnome.Calendar",
                      "flags", G_APPLICATION_HANDLES_COMMAND_LINE,
                      NULL);
  app->priv->settings = g_settings_new ("org.gnome.calendar");
  g_signal_connect (app->priv->settings,
                    "changed::active-view",
                    G_CALLBACK (gcal_application_changed_view),
                    app);
  return app;
}

GcalManager*
gcal_application_get_manager (GcalApplication *app)
{
  g_return_val_if_fail (GCAL_IS_APPLICATION (app), NULL);
  return app->priv->manager;
}

GSettings*
gcal_application_get_settings (GcalApplication *app)
{
  g_return_val_if_fail (GCAL_IS_APPLICATION (app), NULL);
  return app->priv->settings;
}
