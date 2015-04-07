/* gcal-source-manager.h
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


#ifndef GCAL_SOURCE_DIALOG_H
#define GCAL_SOURCE_DIALOG_H

#include <glib-object.h>
#include "gcal-manager.h"

#include <libecal/libecal.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GCAL_RESPONSE_REMOVE_SOURCE   2

#define GCAL_TYPE_SOURCE_DIALOG (gcal_source_dialog_get_type())

G_DECLARE_FINAL_TYPE (GcalSourceDialog, gcal_source_dialog, GCAL, SOURCE_DIALOG, GtkDialog)

struct _GcalSourceDialogClass
{
  GtkDialogClass parent;
};

typedef enum
{
  GCAL_SOURCE_DIALOG_MODE_EDIT,
  GCAL_SOURCE_DIALOG_MODE_NORMAL
} GcalSourceDialogMode;

GcalSourceDialog*   gcal_source_dialog_new                  (void);

void                gcal_source_dialog_set_manager          (GcalSourceDialog   *dialog,
                                                             GcalManager        *manager);

void                gcal_source_dialog_set_mode             (GcalSourceDialog     *dialog,
                                                             GcalSourceDialogMode  mode);

void                gcal_source_dialog_set_source           (GcalSourceDialog     *dialog,
                                                             ESource              *source);

G_END_DECLS

#endif /* GCAL_SOURCE_DIALOG_H */
