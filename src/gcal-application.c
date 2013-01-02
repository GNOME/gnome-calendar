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

#include "config.h"
#include "gcal-application.h"
#include "gcal-window.h"

#include <clutter-gtk/clutter-gtk.h>

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

static void gcal_application_finalize      (GObject         *object);

static void gcal_application_activate      (GApplication    *app);

static void gcal_application_startup       (GApplication    *app);

static void gcal_application_set_app_menu  (GApplication    *app);

static void gcal_application_changed_view  (GSettings       *settings,
                                            gchar           *key,
                                            gpointer         user_data);

static void gcal_application_change_view   (GSimpleAction   *simple,
                                            GVariant        *parameter,
                                            gpointer         user_data);

static void gcal_application_show_about    (GSimpleAction   *simple,
                                            GVariant        *parameter,
                                            gpointer         user_data);

static void gcal_application_quit          (GSimpleAction   *simple,
                                            GVariant        *parameter,
                                            gpointer         user_data);

G_DEFINE_TYPE (GcalApplication, gcal_application, GTK_TYPE_APPLICATION);

static void
gcal_application_class_init (GcalApplicationClass *klass)
{
  GApplicationClass *application_class;
  GObjectClass *object_class;

  application_class = G_APPLICATION_CLASS (klass);
  application_class->activate = gcal_application_activate;
  application_class->startup = gcal_application_startup;

  object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = gcal_application_finalize;

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
      priv->window =
        gcal_window_new_with_view (GCAL_APPLICATION (application),
                                   g_settings_get_enum (priv->settings,
                                                        "active-view"));
      g_settings_bind (priv->settings,
                       "active-view",
                       priv->window,
                       "active-view",
                       G_SETTINGS_BIND_SET | G_SETTINGS_BIND_GET);
      gtk_window_set_title (GTK_WINDOW (priv->window), _("Calendar"));
      gtk_window_set_hide_titlebar_when_maximized (GTK_WINDOW (priv->window),
                                                   TRUE);

      gtk_window_maximize (GTK_WINDOW (priv->window));
      gtk_widget_show_all (priv->window);
    }
}

static void
gcal_application_startup (GApplication *app)
{
  GcalApplicationPrivate *priv;
  GError *error;
  priv = GCAL_APPLICATION (app)->priv;

  G_APPLICATION_CLASS (gcal_application_parent_class)->startup (app);

  if (gtk_clutter_init (NULL, NULL) < 0)
   {
     g_error (_("Unable to initialize GtkClutter"));
     g_application_quit (app);
   }

  if (priv->provider == NULL)
   {
     priv->provider = gtk_css_provider_new ();
     gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                                GTK_STYLE_PROVIDER (priv->provider),
                                                G_MAXUINT);

     error = NULL;
     gtk_css_provider_load_from_path (priv->provider, CSS_FILE, &error);
     if (error != NULL)
       g_warning ("Error loading stylesheet from file %s. %s", CSS_FILE, error->message);
   }

  priv->manager = gcal_manager_new ();

  gcal_application_set_app_menu (app);
}

static void
gcal_application_set_app_menu (GApplication *app)
{
  GcalApplicationPrivate *priv;

  GMenu *app_menu;
  GMenu *view_as;
  GSimpleAction *about;
  GSimpleAction *quit;

  GVariant *va;

  g_return_if_fail (GCAL_IS_APPLICATION (app));
  priv = GCAL_APPLICATION (app)->priv;

  app_menu = g_menu_new ();

  priv->view = g_simple_action_new_stateful (
      "view",
      G_VARIANT_TYPE_STRING,
      g_settings_get_value (priv->settings, "active-view"));

  g_signal_connect (priv->view,
                    "activate",
                    G_CALLBACK (gcal_application_change_view),
                    app);
  g_action_map_add_action ( G_ACTION_MAP (app), G_ACTION (priv->view));

  view_as = g_menu_new ();
  g_menu_append (view_as, _("Weeks"), "app.view::week");
  g_menu_append (view_as, _("Months"), "app.view::month");
  g_menu_append (view_as, _("Years"), "app.view::year");

  g_menu_append_section (app_menu, _("View as"), G_MENU_MODEL (view_as));

  about = g_simple_action_new ("about", NULL);
  g_signal_connect (about,
                    "activate",
                    G_CALLBACK (gcal_application_show_about),
                    app);
  g_action_map_add_action ( G_ACTION_MAP (app), G_ACTION (about));
  g_menu_append (app_menu, _("About"), "app.about");

  quit = g_simple_action_new ("quit", NULL);
  g_signal_connect (quit,
                    "activate",
                    G_CALLBACK (gcal_application_quit),
                    app);
  g_action_map_add_action ( G_ACTION_MAP (app), G_ACTION (quit));
  g_menu_append (app_menu, _("Quit"), "app.quit");

  gtk_application_set_app_menu (GTK_APPLICATION (app), G_MENU_MODEL (app_menu));

  /* Accelerators */
  gtk_application_add_accelerator (GTK_APPLICATION (app), "<Primary>q", "app.quit", NULL);

  va = g_variant_new_string ("month");
  gtk_application_add_accelerator (GTK_APPLICATION (app), "<Primary>m", "app.view", va);
  g_variant_unref (va);
  va = g_variant_new_string ("year");
  gtk_application_add_accelerator (GTK_APPLICATION (app), "<Primary>y", "app.view", va);
  g_variant_unref (va);
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

/* Public API */
GcalApplication*
gcal_application_new (void)
{
  GcalApplication *app;
  g_type_init ();

  g_set_application_name ("Calendar");

  app = g_object_new (gcal_application_get_type (),
                      "application-id", "org.gnome.Calendar",
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
