/*
 * gcal-editable-reminder.h
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

#ifndef __GCAL_EDITABLE_REMINDER_H__
#define __GCAL_EDITABLE_REMINDER_H__

#include "gcal-editable.h"

G_BEGIN_DECLS

#define GCAL_TYPE_EDITABLE_REMINDER                       (gcal_editable_reminder_get_type ())
#define GCAL_EDITABLE_REMINDER(obj)                       (G_TYPE_CHECK_INSTANCE_CAST((obj), GCAL_TYPE_EDITABLE_REMINDER, GcalEditableReminder))
#define GCAL_EDITABLE_REMINDER_CLASS(klass)               (G_TYPE_CHECK_CLASS_CAST((klass), GCAL_TYPE_EDITABLE_REMINDER, GcalEditableReminderClass))
#define GCAL_IS_EDITABLE_REMINDER(obj)                    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GCAL_TYPE_EDITABLE_REMINDER))
#define GCAL_IS_EDITABLE_REMINDER_CLASS(klass)            (G_TYPE_CHECK_CLASS_TYPE((klass), GCAL_TYPE_EDITABLE_REMINDER))
#define GCAL_EDITABLE_REMINDER_GET_CLASS(obj)             (G_TYPE_INSTANCE_GET_CLASS((obj), GCAL_TYPE_EDITABLE_REMINDER, GcalEditableReminderClass))

typedef struct _GcalEditableReminder                       GcalEditableReminder;
typedef struct _GcalEditableReminderClass                  GcalEditableReminderClass;
typedef struct _GcalEditableReminderPrivate                GcalEditableReminderPrivate;

struct _GcalEditableReminder
{
  GcalEditable parent;

  /* add your public declarations here */
  GcalEditableReminderPrivate *priv;
};

struct _GcalEditableReminderClass
{
  GcalEditableClass parent_class;
};

GType          gcal_editable_reminder_get_type           (void);

GtkWidget*     gcal_editable_reminder_new                (void);

GtkWidget*     gcal_editable_reminder_new_with_content   (GList                *content);

void           gcal_editable_reminder_set_content        (GcalEditableReminder *entry,
                                                          GList                *content);
G_END_DECLS

#endif /* __GCAL_EDITABLE_REMINDER_H__ */
