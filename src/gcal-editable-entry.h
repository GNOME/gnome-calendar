/*
 * gcal-editable-entry.h
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

#ifndef __GCAL_EDITABLE_ENTRY_H__
#define __GCAL_EDITABLE_ENTRY_H__

#include "gcal-editable.h"

G_BEGIN_DECLS

#define GCAL_TYPE_EDITABLE_ENTRY                       (gcal_editable_entry_get_type ())
#define GCAL_EDITABLE_ENTRY(obj)                       (G_TYPE_CHECK_INSTANCE_CAST((obj), GCAL_TYPE_EDITABLE_ENTRY, GcalEditableEntry))
#define GCAL_EDITABLE_ENTRY_CLASS(klass)               (G_TYPE_CHECK_CLASS_CAST((klass), GCAL_TYPE_EDITABLE_ENTRY, GcalEditableEntryClass))
#define GCAL_IS_EDITABLE_ENTRY(obj)                    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GCAL_TYPE_EDITABLE_ENTRY))
#define GCAL_IS_EDITABLE_ENTRY_CLASS(klass)            (G_TYPE_CHECK_CLASS_TYPE((klass), GCAL_TYPE_EDITABLE_ENTRY))
#define GCAL_EDITABLE_ENTRY_GET_CLASS(obj)             (G_TYPE_INSTANCE_GET_CLASS((obj), GCAL_TYPE_EDITABLE_ENTRY, GcalEditableEntryClass))

typedef struct _GcalEditableEntry                       GcalEditableEntry;
typedef struct _GcalEditableEntryClass                  GcalEditableEntryClass;
typedef struct _GcalEditableEntryPrivate                GcalEditableEntryPrivate;

struct _GcalEditableEntry
{
  GcalEditable parent;

  /* add your public declarations here */
  GcalEditableEntryPrivate *priv;
};

struct _GcalEditableEntryClass
{
  GcalEditableClass parent_class;
};

GType          gcal_editable_entry_get_type           (void);

GtkWidget*     gcal_editable_entry_new                (void);

GtkWidget*     gcal_editable_entry_new_with_content   (const gchar       *content);

void           gcal_editable_entry_set_content        (GcalEditableEntry *entry,
                                                       const gchar       *content,
                                                       gboolean           use_markup);
G_END_DECLS

#endif /* __GCAL_EDITABLE_ENTRY_H__ */
