/* -*- mode: c; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gcal-edit-dialog.h
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

#ifndef __GCAL_EDIT_DIALOG_H__
#define __GCAL_EDIT_DIALOG_H__

#include "gcal-manager.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GCAL_RESPONSE_DELETE_EVENT 2


#define GCAL_TYPE_EDIT_DIALOG                (gcal_edit_dialog_get_type ())
#define GCAL_EDIT_DIALOG(obj)                (G_TYPE_CHECK_INSTANCE_CAST((obj), GCAL_TYPE_EDIT_DIALOG, GcalEditDialog))
#define GCAL_EDIT_DIALOG_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST((klass), GCAL_TYPE_EDIT_DIALOG, GcalEditDialogClass))
#define GCAL_IS_EDIT_DIALOG(obj)             (G_TYPE_CHECK_INSTANCE_TYPE((obj), GCAL_TYPE_EDIT_DIALOG))
#define GCAL_IS_EDIT_DIALOG_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE((klass), GCAL_TYPE_EDIT_DIALOG))
#define GCAL_EDIT_DIALOG_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS((obj), GCAL_TYPE_EDIT_DIALOG, GcalEditDialogClass))

typedef struct _GcalEditDialog                GcalEditDialog;
typedef struct _GcalEditDialogClass           GcalEditDialogClass;
typedef struct _GcalEditDialogPrivate         GcalEditDialogPrivate;

struct _GcalEditDialog
{
  GtkDialog parent;

  GcalEditDialogPrivate *priv;
};

struct _GcalEditDialogClass
{
  GtkDialogClass parent_class;
};


GType                gcal_edit_dialog_get_type                (void);

GtkWidget*           gcal_edit_dialog_new                     (void);

void                 gcal_edit_dialog_set_event               (GcalEditDialog *dialog,
                                                               const gchar    *source_uid,
                                                               const gchar    *event_uid);

void                 gcal_edit_dialog_set_manager             (GcalEditDialog *dialog,
                                                               GcalManager    *manager);

const gchar*         gcal_edit_dialog_peek_source_uid         (GcalEditDialog *dialog);

const gchar*         gcal_edit_dialog_peek_event_uid          (GcalEditDialog *dialog);

gchar*               gcal_edit_dialog_get_event_uuid          (GcalEditDialog *dialog);

GList*               gcal_edit_dialog_get_modified_properties (GcalEditDialog *dialog);

const gchar*         gcal_edit_dialog_peek_summary            (GcalEditDialog *dialog);

const gchar*         gcal_edit_dialog_peek_location           (GcalEditDialog *dialog);

gchar*               gcal_edit_dialog_get_event_description   (GcalEditDialog *dialog);

icaltimetype*        gcal_edit_dialog_get_start_date          (GcalEditDialog *dialog);

icaltimetype*        gcal_edit_dialog_get_end_date            (GcalEditDialog *dialog);

gchar*               gcal_edit_dialog_get_new_source_uid      (GcalEditDialog *dialog);

G_END_DECLS

#endif /* __GCAL_EDIT_DIALOG_H__ */
