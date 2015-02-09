/* gcal-source-dialog.c
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

#include "gcal-source-dialog.h"

#include "gcal-utils.h"

#include <glib/gi18n.h>

typedef struct
{
  GtkWidget          *add_button;
  GtkWidget          *calendar_color_button;
  GtkWidget          *cancel_button;
  GtkWidget          *headerbar;
  GtkWidget          *name_entry;
  GtkWidget          *stack;

  /* flags */
  gint                mode : 1;
  ESource            *source;

  /* manager */
  GcalManager        *manager;
} GcalSourceDialogPrivate;

struct _GcalSourceDialog
{
  GtkDialog parent;

  /*< private >*/
  GcalSourceDialogPrivate *priv;
};

static void       action_widget_activated               (GtkWidget            *widget,
                                                         gpointer              user_data);

G_DEFINE_TYPE_WITH_PRIVATE (GcalSourceDialog, gcal_source_dialog, GTK_TYPE_DIALOG)

enum {
  PROP_0,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

/**
 * action_widget_activated:
 * @widget: the button which emited the signal.
 * @user_data: a {@link GcalSourceDialog} instance.
 *
 * Emit a response when action buttons
 * are clicked.
 *
 * Returns:
 */
static void
action_widget_activated (GtkWidget *widget,
                         gpointer   user_data)
{
  GcalSourceDialogPrivate *priv = GCAL_SOURCE_DIALOG (user_data)->priv;
  gint response;

  response = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (widget), "response"));

  gtk_dialog_response (GTK_DIALOG (user_data), response);
}

GcalSourceDialog *
gcal_source_dialog_new (void)
{
  return g_object_new (GCAL_TYPE_SOURCE_DIALOG, NULL);
}

static void
gcal_source_dialog_constructed (GObject *object)
{
  GcalSourceDialog *self = (GcalSourceDialog *)object;
  GcalSourceDialogPrivate *priv = gcal_source_dialog_get_instance_private (self);

  G_OBJECT_CLASS (gcal_source_dialog_parent_class)->constructed (object);

  /* widget responses */
  gtk_dialog_set_default_response (GTK_DIALOG (object), GTK_RESPONSE_CANCEL);

  g_object_set_data (G_OBJECT (priv->add_button), "response", GINT_TO_POINTER (GTK_RESPONSE_APPLY));
  g_object_set_data (G_OBJECT (priv->cancel_button), "response", GINT_TO_POINTER (GTK_RESPONSE_CANCEL));

  /* setup titlebar */
  gtk_window_set_titlebar (GTK_WINDOW (object), priv->headerbar);
}

static void
gcal_source_dialog_finalize (GObject *object)
{
  GcalSourceDialog *self = (GcalSourceDialog *)object;
  GcalSourceDialogPrivate *priv = gcal_source_dialog_get_instance_private (self);

  G_OBJECT_CLASS (gcal_source_dialog_parent_class)->finalize (object);
}

static void
gcal_source_dialog_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GcalSourceDialog *self = GCAL_SOURCE_DIALOG (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_source_dialog_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GcalSourceDialog *self = GCAL_SOURCE_DIALOG (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_source_dialog_class_init (GcalSourceDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class;

  object_class->constructed = gcal_source_dialog_constructed;
  object_class->finalize = gcal_source_dialog_finalize;
  object_class->get_property = gcal_source_dialog_get_property;
  object_class->set_property = gcal_source_dialog_set_property;

  widget_class = GTK_WIDGET_CLASS (klass);

  /* bind things for/from the template class */
  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/org/gnome/calendar/source-dialog.ui");

  gtk_widget_class_bind_template_child_private (widget_class, GcalSourceDialog, add_button);
  gtk_widget_class_bind_template_child_private (widget_class, GcalSourceDialog, calendar_color_button);
  gtk_widget_class_bind_template_child_private (widget_class, GcalSourceDialog, cancel_button);
  gtk_widget_class_bind_template_child_private (widget_class, GcalSourceDialog, headerbar);
  gtk_widget_class_bind_template_child_private (widget_class, GcalSourceDialog, name_entry);
  gtk_widget_class_bind_template_child_private (widget_class, GcalSourceDialog, stack);

  gtk_widget_class_bind_template_callback (widget_class, action_widget_activated);
}

static void
gcal_source_dialog_init (GcalSourceDialog *self)
{
  GcalSourceDialogPrivate *priv = gcal_source_dialog_get_instance_private (self);

  self->priv = priv;

  gtk_widget_init_template (GTK_WIDGET (self));
}

/**
 * gcal_source_dialog_set_manager:
 *
 * Setup the {@link GcalManager} singleton
 * instance of the application.
 *
 * Returns:
 */
void
gcal_source_dialog_set_manager (GcalSourceDialog *dialog,
                                GcalManager      *manager)
{
  GcalSourceDialogPrivate *priv = dialog->priv;

  priv->manager = manager;

  /* TODO: connect ::source-added & ::source-removed signals */
}

/**
 * gcal_source_dialog_set_mode:
 *
 * Set the source dialog mode. Creation
 * mode means that a new calendar will
 * be created, while edit mode means a
 * calendar will be edited.
 *
 * Returns:
 */
gcal_source_dialog_set_mode (GcalSourceDialog    *dialog,
                             GcalSourceDialogMode mode)
{
  GcalSourceDialogPrivate *priv = dialog->priv;
  gboolean edit_mode;

  priv->mode = mode;
  edit_mode = (mode == GCAL_SOURCE_DIALOG_MODE_EDIT);

  gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (priv->headerbar), edit_mode);
  gtk_widget_set_visible (priv->cancel_button, !edit_mode);
  gtk_widget_set_visible (priv->add_button, !edit_mode);
  gtk_stack_set_visible_child_name (GTK_STACK (priv->stack), edit_mode ? "edit" : "create");

  if (!edit_mode)
    {
      gtk_header_bar_set_title (GTK_HEADER_BAR (priv->headerbar), _("Add Calendar"));
      gtk_header_bar_set_subtitle (GTK_HEADER_BAR (priv->headerbar), "");
    }
}

/**
 * gcal_source_dialog_set_source:
 *
 * Sets the source to be edited by the user.
 *
 * Returns:
 */
void
gcal_source_dialog_set_source (GcalSourceDialog *dialog,
                               ESource          *source)
{
  GcalSourceDialogPrivate *priv = dialog->priv;
  GdkRGBA color;

  priv->source = source;

  /* color button */
  gdk_rgba_parse (&color, get_color_name_from_source (source));
  gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (priv->calendar_color_button), &color);

  /* entry */
  gtk_entry_set_text (GTK_ENTRY (priv->name_entry), e_source_get_display_name (source));

  /* title */
  gtk_header_bar_set_title (GTK_HEADER_BAR (priv->headerbar), e_source_get_display_name (source));

  /* FIXME: account information on subtitle */
  gtk_header_bar_set_subtitle (GTK_HEADER_BAR (priv->headerbar), "");
}
