/*
 * gcal-editable-date.h
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

#ifndef __GCAL_EDITABLE_DATE_H__
#define __GCAL_EDITABLE_DATE_H__

#include "gcal-editable.h"

G_BEGIN_DECLS

#define GCAL_TYPE_EDITABLE_DATE                       (gcal_editable_date_get_type ())
#define GCAL_EDITABLE_DATE(obj)                       (G_TYPE_CHECK_INSTANCE_CAST((obj), GCAL_TYPE_EDITABLE_DATE, GcalEditableDate))
#define GCAL_EDITABLE_DATE_CLASS(klass)               (G_TYPE_CHECK_CLASS_CAST((klass), GCAL_TYPE_EDITABLE_DATE, GcalEditableDateClass))
#define GCAL_IS_EDITABLE_DATE(obj)                    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GCAL_TYPE_EDITABLE_DATE))
#define GCAL_IS_EDITABLE_DATE_CLASS(klass)            (G_TYPE_CHECK_CLASS_TYPE((klass), GCAL_TYPE_EDITABLE_DATE))
#define GCAL_EDITABLE_DATE_GET_CLASS(obj)             (G_TYPE_INSTANCE_GET_CLASS((obj), GCAL_TYPE_EDITABLE_DATE, GcalEditableDateClass))

typedef struct _GcalEditableDate                       GcalEditableDate;
typedef struct _GcalEditableDateClass                  GcalEditableDateClass;
typedef struct _GcalEditableDatePrivate                GcalEditableDatePrivate;

struct _GcalEditableDate
{
  GcalEditable parent;

  /* add your public declarations here */
  GcalEditableDatePrivate *priv;
};

struct _GcalEditableDateClass
{
  GcalEditableClass parent_class;
};

GType          gcal_editable_date_get_type           (void);

GtkWidget*     gcal_editable_date_new                (void);

GtkWidget*     gcal_editable_date_new_with_content   (const gchar       *content);

void           gcal_editable_date_set_content        (GcalEditableDate *entry,
                                                      const gchar       *content);
G_END_DECLS

#endif /* __GCAL_EDITABLE_DATE_H__ */
