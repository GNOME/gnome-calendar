/*
 * gcal-editable.h
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

#ifndef __GCAL_EDITABLE_H__
#define __GCAL_EDITABLE_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GCAL_TYPE_EDITABLE                       (gcal_editable_get_type ())
#define GCAL_EDITABLE(obj)                       (G_TYPE_CHECK_INSTANCE_CAST((obj), GCAL_TYPE_EDITABLE, GcalEditable))
#define GCAL_EDITABLE_CLASS(klass)               (G_TYPE_CHECK_CLASS_CAST((klass), GCAL_TYPE_EDITABLE, GcalEditableClass))
#define GCAL_IS_EDITABLE(obj)                    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GCAL_TYPE_EDITABLE))
#define GCAL_IS_EDITABLE_CLASS(klass)            (G_TYPE_CHECK_CLASS_TYPE((klass), GCAL_TYPE_EDITABLE))
#define GCAL_EDITABLE_GET_CLASS(obj)             (G_TYPE_INSTANCE_GET_CLASS((obj), GCAL_TYPE_EDITABLE, GcalEditableClass))

typedef struct _GcalEditable                      GcalEditable;
typedef struct _GcalEditableClass                 GcalEditableClass;
typedef struct _GcalEditablePrivate               GcalEditablePrivate;

struct _GcalEditable
{
  GtkNotebook parent;

  /* add your public declarations here */
  GcalEditablePrivate *priv;
};

struct _GcalEditableClass
{
  GtkNotebookClass parent_class;

  /* virtual methods */
  gchar*     (*update_view_widget)     (GcalEditable *editable);
  void       (*enter_edit_mode)        (GcalEditable *editable);
  void       (*leave_edit_mode)        (GcalEditable *editable);
};

GType          gcal_editable_get_type           (void);

void           gcal_editable_set_view_contents  (GcalEditable *editable,
                                                 const gchar  *contents);

void           gcal_editable_set_edit_widget    (GcalEditable *editable,
                                                 GtkWidget    *widget);

const gchar*   gcal_editable_get_view_contents  (GcalEditable *editable);

void           gcal_editable_enter_edit_mode    (GcalEditable   *editable);

void           gcal_editable_leave_edit_mode    (GcalEditable   *editable);

G_END_DECLS

#endif /* __GCAL_EDITABLE_H__ */
