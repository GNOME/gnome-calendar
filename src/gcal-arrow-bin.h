/* -*- mode: c; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gcal-arrow-bin.h
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

#ifndef __GCAL_ARROW_BIN_H__
#define __GCAL_ARROW_BIN_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS


#define GCAL_TYPE_ARROW_BIN                (gcal_arrow_bin_get_type ())
#define GCAL_ARROW_BIN(obj)                (G_TYPE_CHECK_INSTANCE_CAST((obj), GCAL_TYPE_ARROW_BIN, GcalArrowBin))
#define GCAL_ARROW_BIN_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST((klass), GCAL_TYPE_ARROW_BIN, GcalArrowBinClass))
#define GCAL_IS_ARROW_BIN(obj)             (G_TYPE_CHECK_INSTANCE_TYPE((obj), GCAL_TYPE_ARROW_BIN))
#define GCAL_IS_ARROW_BIN_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE((klass), GCAL_TYPE_ARROW_BIN))
#define GCAL_ARROW_BIN_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS((obj), GCAL_TYPE_ARROW_BIN, GcalArrowBinClass))

typedef struct _GcalArrowBin                GcalArrowBin;
typedef struct _GcalArrowBinClass           GcalArrowBinClass;
typedef struct _GcalArrowBinPrivate         GcalArrowBinPrivate;

struct _GcalArrowBin
{
  GtkBin parent;
  /* add your public declarations here */

  GcalArrowBinPrivate *priv;
};

struct _GcalArrowBinClass
{
  GtkBinClass parent_class;
};

GType            gcal_arrow_bin_get_type        (void);

GtkWidget*       gcal_arrow_bin_new             (void);

void             gcal_arrow_bin_set_arrow_pos   (GcalArrowBin     *arrow,
                                                 GtkPositionType   arrow_pos);

GtkPositionType  gcal_arrow_bin_get_arrow_pos   (GcalArrowBin     *arrow);

void             gcal_arrow_bin_set_arrow_align (GcalArrowBin     *arrow,
                                                 gdouble           align);

gdouble          gcal_arrow_bin_get_arrow_align (GcalArrowBin     *arrow);

G_END_DECLS

#endif /* __GCAL_ARROW_BIN_H__ */
