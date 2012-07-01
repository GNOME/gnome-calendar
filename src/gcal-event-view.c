/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 2 -*- */
/*
 * gcal-event-view.c
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

#include "gcal-event-view.h"
#include "gcal-editable-entry.h"
#include "gcal-editable-text.h"
#include "gcal-editable-date.h"
#include "gcal-editable-combo.h"
#include "gcal-editable-reminder.h"

#include <glib/gi18n.h>

struct _GcalEventViewPrivate
{
  GcalEditableMode   edit_mode;
  gchar             *source_uid;
  gchar             *event_uid;
  GSList            *e_widgets;

  GtkWidget         *e_what;
  GtkWidget         *e_host;

  GtkWidget         *d_when;

  GtkWidget         *e_where;

  GtkWidget         *cb_cal;

  GtkWidget         *t_desc;

  GtkWidget         *reminders;

  GtkWidget         *attending_yes;
  GtkWidget         *attending_maybe;
  GtkWidget         *attending_no;

  GcalManager       *manager;
};

static void     gcal_event_view_constructed          (GObject        *object);

static void     gcal_event_view_finalize             (GObject        *object);

static void     gcal_event_view_set_combo_box        (GcalEventView  *view);

static void     gcal_event_view_update               (GcalEventView  *view);

G_DEFINE_TYPE(GcalEventView, gcal_event_view, GTK_TYPE_GRID)

static void
gcal_event_view_class_init(GcalEventViewClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->constructed = gcal_event_view_constructed;
  object_class->finalize = gcal_event_view_finalize;

  g_type_class_add_private((gpointer)klass, sizeof(GcalEventViewPrivate));
}

static void gcal_event_view_init(GcalEventView *self)
{
  GtkStyleContext *context;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self,
                                           GCAL_TYPE_EVENT_VIEW,
                                           GcalEventViewPrivate);

  context = gtk_widget_get_style_context (GTK_WIDGET (self));
  gtk_style_context_add_class (context, "event-view");
}

static void
gcal_event_view_constructed (GObject *object)
{
  GcalEventViewPrivate *priv;
  GtkWidget *what;
  GtkWidget *host;
  GtkWidget *when;
  GtkWidget *calendar;
  GtkWidget *where;
  GtkWidget *desc;
  GtkWidget *rem;
  GtkWidget *attending;
  GtkWidget *attending_box;

  priv = GCAL_EVENT_VIEW (object)->priv;
  if (G_OBJECT_CLASS (gcal_event_view_parent_class)->constructed != NULL)
    G_OBJECT_CLASS (gcal_event_view_parent_class)->constructed (object);

  priv->edit_mode = GCAL_EDIT_MODE;
  priv->source_uid = NULL;
  priv->event_uid = NULL;
  priv->e_widgets = NULL;

  priv->e_what = gcal_editable_entry_new ();
  priv->e_widgets = g_slist_append (priv->e_widgets, priv->e_what);
  priv->e_host = gcal_editable_entry_new ();
  priv->e_widgets = g_slist_append (priv->e_widgets, priv->e_host);

  priv->d_when = gcal_editable_date_new ();
  priv->e_widgets = g_slist_append (priv->e_widgets, priv->d_when);

  priv->e_where = gcal_editable_entry_new ();
  priv->e_widgets = g_slist_append (priv->e_widgets, priv->e_where);

  /* FIXME: turn me into a GcalEditable Widget */
  priv->cb_cal = gcal_editable_combo_new ();
  priv->e_widgets = g_slist_append (priv->e_widgets, priv->cb_cal);
  gtk_widget_set_halign (priv->cb_cal, GTK_ALIGN_START);

  priv->t_desc = gcal_editable_text_new ();
  priv->e_widgets = g_slist_append (priv->e_widgets, priv->t_desc);

  priv->reminders = gcal_editable_reminder_new ();
  priv->e_widgets = g_slist_append (priv->e_widgets, priv->reminders);

  /* FIXME: do something with this to add it on the slist of widget
   * something like, turning all the three into on widget
   */
  priv->attending_yes = gtk_toggle_button_new_with_label (_("Yes"));
  priv->attending_maybe = gtk_toggle_button_new_with_label (_("Maybe"));
  priv->attending_no = gtk_toggle_button_new_with_label (_("No"));

  attending_box = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_box_set_spacing (GTK_BOX (attending_box), 2);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (attending_box), GTK_BUTTONBOX_SPREAD);
  gtk_widget_set_halign (attending_box, GTK_ALIGN_START);
  gtk_container_add (GTK_CONTAINER (attending_box), priv->attending_yes);
  gtk_container_add (GTK_CONTAINER (attending_box), priv->attending_maybe);
  gtk_container_add (GTK_CONTAINER (attending_box), priv->attending_no);

  what = gtk_label_new (_("What"));
  gtk_widget_set_halign (what, GTK_ALIGN_END);
  gtk_grid_attach (GTK_GRID (object), what, 0, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (object), priv->e_what, 1, 0, 3, 1);

  host = gtk_label_new (_("Host"));
  gtk_widget_set_halign (host, GTK_ALIGN_END);
  gtk_grid_attach (GTK_GRID (object), host, 4, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (object), priv->e_host, 5, 0, 1, 1);

  when = gtk_label_new (_("When"));
  gtk_widget_set_valign (when, GTK_ALIGN_START);
  gtk_widget_set_halign (when, GTK_ALIGN_END);
  gtk_grid_attach (GTK_GRID (object), when, 0, 1, 1, 1);
  gtk_grid_attach (GTK_GRID (object), priv->d_when, 1, 1, 1, 1);

  where = gtk_label_new (_("Where"));
  gtk_widget_set_halign (where, GTK_ALIGN_END);
  gtk_grid_attach (GTK_GRID (object), where, 0, 2, 1, 1);
  gtk_grid_attach (GTK_GRID (object), priv->e_where, 1, 2, 3, 1);

  calendar = gtk_label_new (_("Calendar"));
  gtk_widget_set_halign (calendar, GTK_ALIGN_END);
  gtk_grid_attach (GTK_GRID (object), calendar, 0, 3, 1, 1);
  gtk_grid_attach (GTK_GRID (object), priv->cb_cal, 1, 3, 3, 1);

  desc = gtk_label_new (_("Description"));
  gtk_widget_set_halign (desc, GTK_ALIGN_END);
  gtk_widget_set_valign (desc, GTK_ALIGN_START);
  gtk_grid_attach (GTK_GRID (object), desc, 0, 4, 1, 1);
  gtk_widget_set_size_request (priv->t_desc, -1, 200);
  gtk_grid_attach (GTK_GRID (object), priv->t_desc, 1, 4, 3, 1);

  rem = gtk_label_new (_("Reminders"));
  gtk_widget_set_halign (rem, GTK_ALIGN_END);
  gtk_widget_set_valign (rem, GTK_ALIGN_START);
  gtk_grid_attach (GTK_GRID (object), rem, 0, 5, 1, 1);
  gtk_grid_attach (GTK_GRID (object), priv->reminders, 1, 5, 3, 1);

  attending = gtk_label_new (_("Attending"));
  gtk_widget_set_halign (attending, GTK_ALIGN_END);
  gtk_grid_attach (GTK_GRID (object), attending, 0, 6, 1, 1);
  gtk_grid_attach (GTK_GRID (object), attending_box, 1, 6, 3, 1);

  gtk_grid_set_row_spacing (GTK_GRID (object), 12);
  gtk_grid_set_column_spacing (GTK_GRID (object), 12);
  gtk_widget_show_all (GTK_WIDGET (object));
}

static void
gcal_event_view_finalize (GObject *object)
{
  GcalEventViewPrivate *priv;

  priv = GCAL_EVENT_VIEW (object)->priv;

  if (priv->source_uid != NULL)
    g_free (priv->source_uid);
  if (priv->event_uid != NULL)
    g_free (priv->event_uid);

  if (G_OBJECT_CLASS (gcal_event_view_parent_class)->finalize != NULL)
    G_OBJECT_CLASS (gcal_event_view_parent_class)->finalize (object);
}

static void
gcal_event_view_set_combo_box (GcalEventView *view)
{
  GcalEventViewPrivate *priv;
  priv = view->priv;

  gcal_editable_combo_set_model (
      GCAL_EDITABLE_COMBO (priv->cb_cal),
      GTK_TREE_MODEL (gcal_manager_get_sources_model (priv->manager)));
}

/**
 * gcal_event_view_update:
 * @view: A GcalEventView widget
 *
 * Update the contents of the widget.
 */
static void
gcal_event_view_update (GcalEventView *view)
{
  GcalEventViewPrivate *priv;

  g_return_if_fail (GCAL_IS_EVENT_VIEW (view));
  priv = view->priv;

  if (priv->event_uid == NULL)
    {
      /* Setting the widget to blank state */
      g_slist_foreach (priv->e_widgets, (GFunc) gcal_editable_clear, NULL);
    }
  else
    {
      gchar  *summary;
      gchar  *organizer_label;
      gchar **organizer;
      gchar *when;
      const gchar *location;
      gchar *description;
      GList *reminders;

      g_slist_foreach (priv->e_widgets, (GFunc) gcal_editable_leave_edit_mode, NULL);

      summary = gcal_manager_get_event_summary (priv->manager,
                                                priv->source_uid,
                                                priv->event_uid);
      gcal_editable_entry_set_content (GCAL_EDITABLE_ENTRY (priv->e_what),
                                       summary,
                                       FALSE);

      /* FIXME: Hide when no shost, hide the panel completely */
      organizer = gcal_manager_get_event_organizer (priv->manager,
                                                    priv->source_uid,
                                                    priv->event_uid);
      if (organizer == NULL)
        {
          //HIDE PANEL
          ;
        }
      else
        {
          gchar *link;
          link = g_utf8_strdown (organizer[1], -1);
          organizer_label = g_strdup_printf ("<a href=\"%s\">%s</a>",
                                             link,
                                             organizer[0]);
          gcal_editable_entry_set_content (GCAL_EDITABLE_ENTRY (priv->e_host),
                                           organizer_label,
                                           TRUE);

          g_free (organizer_label);
          g_free (link);
          g_strfreev (organizer);
        }

      when = gcal_manager_get_event_date (priv->manager,
                                          priv->source_uid,
                                          priv->event_uid);

      gcal_editable_date_set_content (GCAL_EDITABLE_DATE (priv->d_when),
                                      when);

      location = gcal_manager_get_event_location (priv->manager,
                                                  priv->source_uid,
                                                  priv->event_uid);
      gcal_editable_entry_set_content (GCAL_EDITABLE_ENTRY (priv->e_where),
                                       location,
                                       FALSE);

      gcal_editable_combo_set_content (
          GCAL_EDITABLE_COMBO (priv->cb_cal),
          gcal_manager_get_source_name (priv->manager, priv->source_uid));

      description = gcal_manager_get_event_description (priv->manager,
                                                        priv->source_uid,
                                                        priv->event_uid);
      gcal_editable_text_set_content (GCAL_EDITABLE_TEXT (priv->t_desc),
                                      description);

      reminders = gcal_manager_get_event_reminders (priv->manager,
                                                    priv->source_uid,
                                                    priv->event_uid);

      gcal_editable_reminder_set_content (
          GCAL_EDITABLE_REMINDER (priv->reminders),
          reminders);

      g_free (description);
      g_free (when);
      g_free (summary);
      g_list_free_full (reminders, (GDestroyNotify) g_free);
    }
}

/* Public API */

/**
 * gcal_event_view_new_with_manager:
 * @manager: A GcalManager instance, the app singleton
 *
 * Returns: (transfer full):
 */
GtkWidget*
gcal_event_view_new_with_manager (GcalManager *manager)
{
  GtkWidget *view;
  GcalEventViewPrivate *priv;

  view = g_object_new (GCAL_TYPE_EVENT_VIEW, NULL);
  priv = GCAL_EVENT_VIEW (view)->priv;
  priv->manager = manager;
  gcal_event_view_set_combo_box (GCAL_EVENT_VIEW (view));

  return view;
}

/**
 * gcal_event_view_load_new:
 * @view: A GcalEventView widget
 *
 * Sets the widget in a edit mode for creating a new
 * event.
 */
void
gcal_event_view_load_new (GcalEventView *view)
{
}

/**
 * gcal_event_view_load_event:
 * @view: A GcalEventView widget
 * @event_uuid: the uuid of the event as referenced in GcalManager
 *
 * Loads events data in the widget, for viewing
 */
void
gcal_event_view_load_event (GcalEventView *view,
                            const gchar   *event_uuid)
{
  GcalEventViewPrivate *priv;
  gchar **tokens;

  g_return_if_fail (GCAL_IS_EVENT_VIEW (view));
  priv = view->priv;

  if (event_uuid != NULL)
    {
      tokens = g_strsplit (event_uuid, ":", -1);

      if (gcal_manager_exists_event (priv->manager, tokens[0], tokens[1]))
        {
          if (priv->source_uid != NULL)
            g_free (priv->source_uid);
          if (priv->event_uid != NULL)
            g_free (priv->event_uid);

          priv->source_uid = tokens[0];
          priv->event_uid = tokens[1];
          gcal_event_view_update (view);
        }
    }
}

/**
 * gcal_event_view_enter_edit_mode:
 * @view: A GcalEventView widget
 *
 * Sets the widget into edit mode. If it has an event set
 * then edit that events, otherwise acts as load_new method.
 */
void
gcal_event_view_enter_edit_mode (GcalEventView *view)
{
}

/**
 * gcal_event_view_leave_edit_mode:
 * @view: A GcalEventView widget
 *
 * Sets the widget into view.
 * FIXME: Maybe this is the place for update/create the event.
 */
void
gcal_event_view_leave_edit_mode (GcalEventView *view)
{
}
