/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 2 -*- */
/*
 * gcal-editable-reminder.c
 *
 * Copyright (C) 2012 - Erick Pérez Castellanos
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "gcal-editable-reminder.h"

#include <glib/gi18n.h>

struct _GcalEditableReminderPrivate
{
  GtkWidget *view_widget;
  GtkWidget *edit_widget;

  GtkWidget *left_box;

  GList     *labels;

  GList     *reminders;
  GtkWidget *add_button;

  gboolean   update_edit_mode;
};

static void     gcal_editable_reminder_constructed         (GObject              *object);

static void     gcal_editable_reminder_finalize            (GObject              *object);

static void     gcal_editable_reminder_enter_edit_mode     (GcalEditable         *editable);

static void     gcal_editable_reminder_leave_edit_mode     (GcalEditable         *editable);

static void     gcal_editable_reminder_clear               (GcalEditable         *editable);

static void     gcal_editable_reminder_add_button_clicked  (GtkButton            *button,
                                                            gpointer              user_data);

static void     gcal_editable_reminder_insert_content      (GcalEditableReminder *editable,
                                                            const gchar          *content);

static void     gcal_editable_reminder_insert_reminder     (GcalEditableReminder *editable,
                                                            const gchar          *type,
                                                            gint                  number,
                                                            const gchar          *time);
static void     gcal_editable_reminder_remove_reminder     (GtkButton            *button,
                                                            gpointer              user_data);

G_DEFINE_TYPE (GcalEditableReminder, gcal_editable_reminder, GCAL_TYPE_EDITABLE);

static void gcal_editable_reminder_class_init (GcalEditableReminderClass *klass)
{
  GcalEditableClass *editable_class;
  GObjectClass *object_class;

  editable_class = GCAL_EDITABLE_CLASS (klass);
  editable_class->enter_edit_mode = gcal_editable_reminder_enter_edit_mode;
  editable_class->leave_edit_mode = gcal_editable_reminder_leave_edit_mode;
  editable_class->clear = gcal_editable_reminder_clear;

  object_class = G_OBJECT_CLASS (klass);
  object_class->constructed = gcal_editable_reminder_constructed;
  object_class->finalize = gcal_editable_reminder_finalize;

  g_type_class_add_private ((gpointer)klass, sizeof (GcalEditableReminderPrivate));
}



static void gcal_editable_reminder_init (GcalEditableReminder *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            GCAL_TYPE_EDITABLE_REMINDER,
                                            GcalEditableReminderPrivate);
}

static void
gcal_editable_reminder_constructed (GObject *object)
{
  GcalEditableReminderPrivate *priv;

  priv = GCAL_EDITABLE_REMINDER (object)->priv;

  if (G_OBJECT_CLASS (gcal_editable_reminder_parent_class)->constructed != NULL)
    G_OBJECT_CLASS (gcal_editable_reminder_parent_class)->constructed (object);

  priv->view_widget = gtk_grid_new ();
  g_object_set (priv->view_widget,
                "row-spacing", 6,
                "valign", GTK_ALIGN_START,
                "halign", GTK_ALIGN_START,
                "orientation", GTK_ORIENTATION_VERTICAL,
                NULL);
  gtk_widget_show (priv->view_widget);
  gcal_editable_set_view_widget (GCAL_EDITABLE (object), priv->view_widget);

  priv->edit_widget = gtk_grid_new ();
  gtk_grid_set_column_spacing (GTK_GRID (priv->edit_widget), 6);
  priv->left_box = gtk_grid_new ();
  g_object_set (priv->left_box,
                "orientation", GTK_ORIENTATION_VERTICAL,
                "row-spacing", 6,
                "column-spacing", 6,
                "vexpand", FALSE,
                "valign", GTK_ALIGN_START,
                NULL);
  gtk_grid_attach (GTK_GRID (priv->edit_widget), priv->left_box, 0, 0, 1, 1);
  gcal_editable_reminder_insert_reminder (GCAL_EDITABLE_REMINDER (object),
                                          NULL,
                                          1,
                                          NULL);

  priv->add_button = gtk_button_new ();
  g_object_ref_sink (priv->add_button);
  g_object_set (priv->add_button,
                "valign", GTK_ALIGN_START,
                "halign", GTK_ALIGN_END,
                "relief", GTK_RELIEF_NONE,
                NULL);
  gtk_container_add (
      GTK_CONTAINER (priv->add_button),
      gtk_image_new_from_icon_name ("list-add-symbolic",
                                    GTK_ICON_SIZE_MENU));
  gtk_grid_attach (GTK_GRID (priv->edit_widget), priv->add_button, 1, 0, 1, 1);
  g_signal_connect (priv->add_button,
                    "clicked",
                    G_CALLBACK (gcal_editable_reminder_add_button_clicked),
                    object);

  gtk_widget_show_all (priv->edit_widget);
  gcal_editable_set_edit_widget (GCAL_EDITABLE (object), priv->edit_widget);

  priv->update_edit_mode = FALSE;
}

static void
gcal_editable_reminder_finalize (GObject  *object)
{
  GcalEditableReminderPrivate *priv;

  g_return_if_fail (GCAL_IS_EDITABLE_REMINDER (object));
  priv = GCAL_EDITABLE_REMINDER (object)->priv;

  if (priv->add_button != NULL)
    g_clear_object (&(priv->add_button));

  if (G_OBJECT_CLASS (gcal_editable_reminder_parent_class)->finalize != NULL)
    G_OBJECT_CLASS (gcal_editable_reminder_parent_class)->finalize (object);
}

static void
gcal_editable_reminder_enter_edit_mode (GcalEditable *editable)
{
  GcalEditableReminderPrivate *priv;
  GList *l;

  g_return_if_fail (GCAL_IS_EDITABLE_REMINDER (editable));
  priv = GCAL_EDITABLE_REMINDER (editable)->priv;

  if (priv->update_edit_mode)
    {
      /* will parse everything */
      g_list_free_full (priv->reminders, (GDestroyNotify) gtk_widget_destroy);
      priv->reminders = NULL;

      for (l = priv->labels; l != NULL; l = l->next)
        {
          gchar **tokens;
          gchar *type;
          gint number;
          gchar *time;

          tokens = g_strsplit (gtk_label_get_text (GTK_LABEL (l->data)), " ", -1);
          if (g_strcmp0 (_("Email"), tokens[0]) == 0)
            type = g_strdup ("email");
          else
            type = g_strdup ("notification");

          number = g_strtod (tokens[1], NULL);

          if (g_str_has_prefix (tokens[2], _("minute")))
            time = g_strdup ("minutes");
          else if (g_str_has_prefix (tokens[2], _("hour")))
            time = g_strdup ("hours");
          else if (g_str_has_prefix (tokens[2], _("day")))
            time = g_strdup ("days");
          else
            time = g_strdup ("weeks");

          gcal_editable_reminder_insert_reminder (
              GCAL_EDITABLE_REMINDER (editable),
              type,
              number,
              time);

          g_free (time);
          g_free (type);
          g_strfreev (tokens);
        }
      priv->update_edit_mode = FALSE;
    }
}

static void
gcal_editable_reminder_leave_edit_mode (GcalEditable *editable)
{
  GcalEditableReminderPrivate *priv;
  GList *l;

  g_return_if_fail (GCAL_IS_EDITABLE_REMINDER (editable));
  priv = GCAL_EDITABLE_REMINDER (editable)->priv;

  /* clearing */
  g_list_free_full (priv->labels, (GDestroyNotify) gtk_widget_destroy);
  priv->labels = NULL;

  for (l = priv->reminders; l != NULL; l = l->next)
    {
      gchar *text;
      gchar *c, *c2;
      GtkWidget *type_combo;
      GtkWidget *number_spin;
      GtkWidget *time_combo;

      type_combo = gtk_grid_get_child_at (GTK_GRID (l->data), 0, 0);
      number_spin = gtk_grid_get_child_at (GTK_GRID (l->data), 1, 0);
      time_combo = gtk_grid_get_child_at (GTK_GRID (l->data), 2, 0);

      text =
        gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (type_combo));
      c = g_strdup_printf (
          "%s %d",
          text,
          gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (number_spin)));
      g_free (text);
      text = c;
      c2 = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (time_combo));
      c = g_strdup_printf ("%s %s %s",
                           text,
                           c2,
                           _("before"));
      g_free (c2);
      g_free (text);

      gcal_editable_reminder_insert_content (GCAL_EDITABLE_REMINDER (editable),
                                             c);
      g_free (c);
    }
}

static void
gcal_editable_reminder_clear (GcalEditable  *editable)
{
  GcalEditableReminderPrivate *priv;

  g_return_if_fail (GCAL_IS_EDITABLE_REMINDER (editable));
  priv = GCAL_EDITABLE_REMINDER (editable)->priv;

  g_list_free_full (priv->reminders, (GDestroyNotify) gtk_widget_destroy);
  priv->reminders = NULL;

  g_list_free_full (priv->labels, (GDestroyNotify) gtk_widget_destroy);
  priv->labels = NULL;
}

static void
gcal_editable_reminder_add_button_clicked (GtkButton *button,
                                           gpointer   user_data)
{
  gcal_editable_reminder_insert_reminder (GCAL_EDITABLE_REMINDER (user_data),
                                          NULL,
                                          0,
                                          NULL);
}

static void
gcal_editable_reminder_insert_content (GcalEditableReminder *editable,
                                       const gchar          *content)
{
  GcalEditableReminderPrivate *priv;
  GtkWidget *label;

  g_return_if_fail (GCAL_IS_EDITABLE_REMINDER (editable));
  priv = editable->priv;

  label = gtk_label_new (content);
  gtk_widget_set_halign (label, GTK_ALIGN_START);

  gtk_container_add (GTK_CONTAINER (priv->view_widget), label);
  gtk_widget_show_all (priv->view_widget);

  priv->labels = g_list_append (priv->labels, label);
}

static void
gcal_editable_reminder_insert_reminder (GcalEditableReminder *editable,
                                        const gchar          *type,
                                        gint                  number,
                                        const gchar          *time)
{
  GcalEditableReminderPrivate *priv;

  GtkWidget *hbox;
  GtkWidget *type_combo;
  GtkWidget *number_spin;
  GtkWidget *time_combo;
  GtkWidget *del_button;

  g_return_if_fail (GCAL_IS_EDITABLE_REMINDER (editable));
  priv = editable->priv;

  hbox = gtk_grid_new ();
  g_object_set (hbox,
                "orientation", GTK_ORIENTATION_HORIZONTAL,
                "column-spacing", 6,
                NULL);

  type_combo = gtk_combo_box_text_new ();

  number_spin = gtk_spin_button_new (gtk_adjustment_new (1, 1, 99, 1, 1, 0),
                                     1,
                                     0);

  time_combo = gtk_combo_box_text_new ();

  del_button = gtk_button_new ();
  g_signal_connect (del_button,
                    "clicked",
                    G_CALLBACK (gcal_editable_reminder_remove_reminder),
                    editable);
  gtk_button_set_relief (GTK_BUTTON (del_button), GTK_RELIEF_NONE);
  gtk_container_add (
      GTK_CONTAINER (del_button),
      gtk_image_new_from_icon_name ("list-remove-symbolic",
                                    GTK_ICON_SIZE_MENU));

  gtk_container_add (GTK_CONTAINER (hbox), type_combo);
  gtk_container_add (GTK_CONTAINER (hbox), number_spin);
  gtk_container_add (GTK_CONTAINER (hbox), time_combo);
  gtk_container_add (GTK_CONTAINER (hbox), del_button);

  /* loading values and defaults */
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (type_combo),
                             "email",
                             _("Email"));
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (type_combo),
                             "notification",
                             _("Notification"));
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (time_combo),
                             "minutes",
                             _("minutes"));
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (time_combo),
                             "hours",
                             _("hours"));
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (time_combo),
                             "days",
                             _("days"));
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (time_combo),
                             "weeks",
                             _("weeks"));

  gtk_combo_box_set_active_id (GTK_COMBO_BOX (type_combo),
                               type != NULL ? type : "email");
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (number_spin),
                             number > 1 ? number : 1);
  gtk_combo_box_set_active_id (GTK_COMBO_BOX (time_combo),
                               time != NULL ? time : "hours");

  gtk_container_add (GTK_CONTAINER (priv->left_box), hbox);

  gtk_widget_show_all (hbox);
  priv->reminders = g_list_append (priv->reminders, hbox);

  /* hack for activating/deactivating first del_button */
  if (g_list_length (priv->reminders) == 1)
    {
      gtk_widget_set_sensitive (del_button, FALSE);
    }
  else
    {
      del_button = gtk_grid_get_child_at (
          GTK_GRID (g_list_first (priv->reminders)->data),
          3, 0);
      gtk_widget_set_sensitive (del_button, TRUE);
    }
}

static void
gcal_editable_reminder_remove_reminder (GtkButton *button,
                                        gpointer   user_data)
{
  GcalEditableReminderPrivate *priv;
  GtkWidget *parent;
  gint index;

  g_return_if_fail (GCAL_IS_EDITABLE_REMINDER (user_data));
  priv = ((GcalEditableReminder*)user_data)->priv;

  parent = gtk_widget_get_parent (GTK_WIDGET (button));
  index = g_list_index (priv->reminders, parent);
  if (index != -1)
    {
      priv->reminders = g_list_remove (priv->reminders, parent);
      gtk_container_remove (GTK_CONTAINER (priv->left_box), parent);

      /* hack for activating/deactivating first del_button */
      if (g_list_length (priv->reminders) == 1)
        {
          GtkWidget *del_button;
          del_button = gtk_grid_get_child_at (
              GTK_GRID (g_list_last (priv->reminders)->data),
              3, 0);
          gtk_widget_set_sensitive (del_button, FALSE);
        }
    }
}

/**
 * gcal_editable_reminder_new:
 *
 * Init the reminder in EDIT_MODE
 *
 * Since: 0.1
 * Returns: (transfer full):
 **/
GtkWidget*
gcal_editable_reminder_new (void)
{
  GtkWidget *reminder = g_object_new (GCAL_TYPE_EDITABLE_REMINDER, NULL);
  gcal_editable_enter_edit_mode (GCAL_EDITABLE (reminder));
  return reminder;
}

/**
 * gcal_editable_reminder_new_with_content:
 *
 * Init the reminder in VIEW_MODE
 *
 * Since 0.2
 * Returns: (transfer full)
 */
GtkWidget*
gcal_editable_reminder_new_with_content (GList* content)
{
  GtkWidget *reminder;
  reminder = g_object_new (GCAL_TYPE_EDITABLE_REMINDER, NULL);
  gcal_editable_reminder_set_content (GCAL_EDITABLE_REMINDER (reminder), content);
  gcal_editable_leave_edit_mode (GCAL_EDITABLE (reminder));
  return reminder;
}

void
gcal_editable_reminder_set_content (GcalEditableReminder *reminder,
                                    GList                *content)
{
  GcalEditableReminderPrivate *priv;
  GList *l;

  g_return_if_fail (GCAL_IS_EDITABLE_REMINDER (reminder));
  priv = reminder->priv;

  if (content != NULL)
    {
      g_list_free_full (priv->labels, (GDestroyNotify) gtk_widget_destroy);
      priv->labels = NULL;
    }

  for (l = content; l != NULL; l = l->next)
    {
      if (l->data != NULL)
        {
          priv->update_edit_mode = TRUE;
          gcal_editable_reminder_insert_content (reminder, l->data);
        }
    }
}
