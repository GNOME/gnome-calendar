/* gcal-file-chooser-button.c
 *
 * Copyright 2021 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "gcal-file-chooser-button.h"

#include <glib/gi18n.h>

struct _GcalFileChooserButton
{
  GtkButton           parent_instance;

  GCancellable       *cancellable;
  GtkFileFilter      *filter;
  GFile              *file;
  gchar              *title;
};

G_DEFINE_FINAL_TYPE (GcalFileChooserButton, gcal_file_chooser_button, GTK_TYPE_BUTTON)

enum
{
  PROP_0,
  PROP_FILE,
  PROP_FILTER,
  PROP_TITLE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS] = { NULL, };

static const gchar *
get_title (GcalFileChooserButton *self)
{
  return self->title ? self->title : _("Select a file");
}

static void
update_label (GcalFileChooserButton *self)
{
  g_autofree gchar *label = NULL;

  if (self->file)
    label = g_file_get_basename (self->file);
  else
    label = g_strdup (get_title (self));

  gtk_button_set_label (GTK_BUTTON (self), label);
}

static void
on_file_opened_cb (GObject      *source,
                   GAsyncResult *result,
                   gpointer      user_data)
{
  GcalFileChooserButton *self;
  g_autoptr (GError) error = NULL;
  g_autoptr (GFile) file = NULL;

  file = gtk_file_dialog_open_finish (GTK_FILE_DIALOG (source), result, &error);

  if (error)
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        return;

      g_warning ("Error opening file: %s", error->message);
    }

  self = GCAL_FILE_CHOOSER_BUTTON (user_data);
  gcal_file_chooser_button_set_file (self, file);
}

static void
gcal_file_chooser_button_clicked (GtkButton *button)
{
  g_autoptr (GtkFileDialog) file_dialog = NULL;
  GcalFileChooserButton *self;
  GtkRoot *root;

  self = GCAL_FILE_CHOOSER_BUTTON (button);
  root = gtk_widget_get_root (GTK_WIDGET (self));

  file_dialog = gtk_file_dialog_new ();
  gtk_file_dialog_set_initial_file (file_dialog, self->file);
  gtk_file_dialog_set_default_filter (file_dialog, self->filter);

  gtk_file_dialog_open (file_dialog,
                        GTK_WINDOW (root),
                        self->cancellable,
                        on_file_opened_cb,
                        self);
}

static void
gcal_file_chooser_button_finalize (GObject *object)
{
  GcalFileChooserButton *self = (GcalFileChooserButton *)object;

  g_cancellable_cancel (self->cancellable);

  g_clear_pointer (&self->title, g_free);
  g_clear_object (&self->cancellable);
  g_clear_object (&self->filter);
  g_clear_object (&self->file);

  G_OBJECT_CLASS (gcal_file_chooser_button_parent_class)->finalize (object);
}

static void
gcal_file_chooser_button_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  GcalFileChooserButton *self = GCAL_FILE_CHOOSER_BUTTON (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, self->file);
      break;

    case PROP_FILTER:
      g_value_set_object (value, self->filter);
      break;

    case PROP_TITLE:
      g_value_set_string (value, self->title);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_file_chooser_button_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  GcalFileChooserButton *self = GCAL_FILE_CHOOSER_BUTTON (object);

  switch (prop_id)
    {
    case PROP_FILE:
      gcal_file_chooser_button_set_file (self, g_value_get_object (value));
      break;

    case PROP_FILTER:
      gcal_file_chooser_button_set_filter (self, g_value_get_object (value));
      break;

    case PROP_TITLE:
      gcal_file_chooser_button_set_title (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_file_chooser_button_class_init (GcalFileChooserButtonClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkButtonClass *button_class = GTK_BUTTON_CLASS (klass);

  button_class->clicked = gcal_file_chooser_button_clicked;

  object_class->finalize = gcal_file_chooser_button_finalize;
  object_class->get_property = gcal_file_chooser_button_get_property;
  object_class->set_property = gcal_file_chooser_button_set_property;

  properties[PROP_FILE] = g_param_spec_object ("file", "", "",
                                               G_TYPE_FILE,
                                               G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_FILTER] = g_param_spec_object ("filter", "", "",
                                                 GTK_TYPE_FILE_FILTER,
                                                 G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_TITLE] = g_param_spec_string ("title", "", "", NULL,
                                                G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gcal_file_chooser_button_init (GcalFileChooserButton *self)
{
  self->cancellable = g_cancellable_new ();

  update_label (self);
}

GtkWidget*
gcal_file_chooser_button_new (void)
{
  return g_object_new (GCAL_TYPE_FILE_CHOOSER_BUTTON, NULL);
}

void
gcal_file_chooser_button_set_file (GcalFileChooserButton *self,
                                   GFile                 *file)
{
  g_return_if_fail (GCAL_IS_FILE_CHOOSER_BUTTON (self));

  if (g_set_object (&self->file, file))
    {
      update_label (self);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FILE]);
    }
}

GFile *
gcal_file_chooser_button_get_file (GcalFileChooserButton *self)
{
  g_return_val_if_fail (GCAL_IS_FILE_CHOOSER_BUTTON (self), NULL);

  return self->file ? g_object_ref (self->file) : NULL;
}

void
gcal_file_chooser_button_set_title (GcalFileChooserButton *self,
                                    const gchar           *title)
{
  g_autofree gchar *old_title = NULL;

  g_return_if_fail (GCAL_IS_FILE_CHOOSER_BUTTON (self));

  old_title = g_steal_pointer (&self->title);
  self->title = g_strdup (title);

  update_label (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TITLE]);
}

const gchar*
gcal_file_chooser_button_get_title (GcalFileChooserButton *self)
{
  g_return_val_if_fail (GCAL_IS_FILE_CHOOSER_BUTTON (self), NULL);

  return self->title;
}

void
gcal_file_chooser_button_set_filter (GcalFileChooserButton *self,
                                     GtkFileFilter         *filter)
{
  g_return_if_fail (GCAL_IS_FILE_CHOOSER_BUTTON (self));
  g_return_if_fail (filter == NULL || GTK_IS_FILE_FILTER (filter));

  if (g_set_object (&self->filter, filter))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FILTER]);
}
