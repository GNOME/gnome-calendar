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


G_DEFINE_TYPE (GcalApplication, gnome_calendar, GTK_TYPE_APPLICATION);



static void
gcal_application_new_window (GApplication *app,
                             GFile        *file)
{
  GtkWidget *window;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), _("Calendar"));
  
  
  gtk_window_set_application (GTK_WINDOW (window), GTK_APPLICATION (app));
  gtk_widget_show_all (GTK_WIDGET (window));
}

/* GApplication implementation */
static void
gcal_application_activate (GApplication *application)
{
  gcal_application_new_window (application, NULL);
}

static void
gcal_application_init (GcalApplication *object)
{

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
  G_APPLICATION_CLASS (klass)->open = gcal_application_open;

  G_OBJECT_CLASS (klass)->finalize = gcal_application_finalize;
}

GcalApplication *
gcal_application_new (void)
{
  g_type_init ();

  return g_object_new (gcal_application_get_type (),
                       "application-id", "org.gnome.Calendar",
                       "flags", G_APPLICATION_HANDLES_COMMAND_LINE,
                       NULL);
}
