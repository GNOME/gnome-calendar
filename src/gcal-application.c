/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 2 -*- */
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
#include "gcal-application.h"

#include <glib/gi18n.h>

struct _GcalApplicationPrivate
{
  GtkWidget  *window;
};

static void gcal_application_startup        (GApplication    *app);

static void _gcal_application_set_app_menu  (GApplication    *app);
static void _gcal_application_show_about    (GSimpleAction   *simple,
                                             GVariant        *parameter,
                                             gpointer         user_data);
static void _gcal_application_quit          (GSimpleAction   *simple,
                                             GVariant        *parameter,
                                             gpointer         user_data);

G_DEFINE_TYPE (GcalApplication, gcal_application, GTK_TYPE_APPLICATION);

static void
gcal_application_activate (GApplication *application)
{
  GcalApplication *app = GCAL_APPLICATION (application);

  if (app->priv->window != NULL)
    gtk_window_present (GTK_WINDOW (app->priv->window));
  else
    {
      app->priv->window = gtk_application_window_new (GTK_APPLICATION (app));
      gtk_window_set_title (GTK_WINDOW (app->priv->window), _("Calendar"));

      gtk_widget_show_all (GTK_WIDGET (app->priv->window));
    }
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
  G_OBJECT_CLASS (gcal_application_parent_class)->finalize (object);
}

static void
gcal_application_class_init (GcalApplicationClass *klass)
{
  G_APPLICATION_CLASS (klass)->activate = gcal_application_activate;
  G_APPLICATION_CLASS (klass)->startup = gcal_application_startup;

  G_OBJECT_CLASS (klass)->finalize = gcal_application_finalize;

  g_type_class_add_private ((gpointer) klass, sizeof(GcalApplicationPrivate));
}

static void
gcal_application_startup (GApplication *app)
{
  G_APPLICATION_CLASS (gcal_application_parent_class)->startup (app);

  _gcal_application_set_app_menu (app);
}

static void 
_gcal_application_set_app_menu (GApplication *app)
{
  GMenu *app_menu = g_menu_new ();
  
  GSimpleAction *about = g_simple_action_new ("about", NULL);
  g_signal_connect (about,
                    "activate",
                    G_CALLBACK (_gcal_application_show_about),
                    app);
  g_action_map_add_action ( G_ACTION_MAP (app), G_ACTION (about));
  g_menu_append (app_menu, _("About"), "app.about");

  GSimpleAction *quit = g_simple_action_new ("quit", NULL);
  g_signal_connect (quit,
                    "activate",
                    G_CALLBACK (_gcal_application_quit),
                    app);
  g_action_map_add_action ( G_ACTION_MAP (app), G_ACTION (quit));
  g_menu_append (app_menu, _("Quit"), "app.quit");

  gtk_application_set_app_menu (GTK_APPLICATION (app), G_MENU_MODEL (app_menu));
}

static void
_gcal_application_show_about (GSimpleAction   *simple,
                              GVariant        *parameter,
                              gpointer         user_data)
{
  GcalApplication *app = GCAL_APPLICATION (user_data);

  gtk_show_about_dialog (GTK_WINDOW (app->priv->window),
                         "program-name", "Calendar",
                         NULL);
}

static void _gcal_application_quit          (GSimpleAction   *simple,
                                             GVariant        *parameter,
                                             gpointer         user_data)
{
  GApplication *app = G_APPLICATION (user_data);

  g_application_quit (app);
}

GcalApplication *
gcal_application_new (void)
{
  g_type_init ();

  g_set_application_name ("Calendar");

  return g_object_new (gcal_application_get_type (),
                       "application-id", "org.gnome.Calendar",
                       NULL);
}
