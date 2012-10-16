/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gcal-date-entry.h
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

#ifndef __GCAL_DATE_ENTRY_H__
#define __GCAL_DATE_ENTRY_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GCAL_TYPE_DATE_ENTRY                (gcal_date_entry_get_type ())
#define GCAL_DATE_ENTRY(obj)                (G_TYPE_CHECK_INSTANCE_CAST((obj), GCAL_TYPE_DATE_ENTRY, GcalDateEntry))
#define GCAL_DATE_ENTRY_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST((klass), GCAL_TYPE_DATE_ENTRY, GcalDateEntryClass))
#define GCAL_IS_DATE_ENTRY(obj)             (G_TYPE_CHECK_INSTANCE_TYPE((obj), GCAL_TYPE_DATE_ENTRY))
#define GCAL_IS_DATE_ENTRY_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE((klass), GCAL_TYPE_DATE_ENTRY))
#define GCAL_DATE_ENTRY_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS((obj), GCAL_TYPE_DATE_ENTRY, GcalDateEntryClass))

typedef struct _GcalTime GcalTime;

typedef struct _GcalDateEntry                GcalDateEntry;
typedef struct _GcalDateEntryClass           GcalDateEntryClass;
typedef struct _GcalDateEntryPrivate         GcalDateEntryPrivate;

struct _GcalDateEntry
{
  GtkEntry parent;
  /* add your public declarations here */

  GcalDateEntryPrivate *priv;
};

struct _GcalDateEntryClass
{
  GtkEntryClass parent_class;
};

GType            gcal_date_entry_get_type        (void);

GtkWidget*       gcal_date_entry_new             (void);

void             gcal_date_entry_set_date        (GcalDateEntry *entry,
                                                  guint          day,
                                                  guint          month,
                                                  guint          year);

void             gcal_date_entry_get_date        (GcalDateEntry *entry,
                                                  guint         *day,
                                                  guint         *month,
                                                  guint         *year);

G_END_DECLS

#endif /* __GCAL_DATE_ENTRY_H__ */
