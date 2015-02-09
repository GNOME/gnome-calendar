/* gcal-source-manager.c
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

#include "gcal-manager.h"

#include "gcal-source-manager.h"

typedef enum
{
  SOURCE_DIALOG_EDIT_MODE,
  SOURCE_DIALOG_CREATE_MODE
}

typedef struct
{
  gint                mode : 1;

  /* manager */
  GcalManager        *manager;
} GcalSourceManagerPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GcalSourceManager, gcal_source_manager, GTK_TYPE_DIALOG)

enum {
  PROP_0,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

GcalSourceManager *
gcal_source_manager_new (void)
{
  return g_object_new (GCAL_TYPE_SOURCE_MANAGER, NULL);
}

static void
gcal_source_manager_finalize (GObject *object)
{
  GcalSourceManager *self = (GcalSourceManager *)object;
  GcalSourceManagerPrivate *priv = gcal_source_manager_get_instance_private (self);

  G_OBJECT_CLASS (gcal_source_manager_parent_class)->finalize (object);
}

static void
gcal_source_manager_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GcalSourceManager *self = GCAL_SOURCE_MANAGER (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_source_manager_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GcalSourceManager *self = GCAL_SOURCE_MANAGER (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_source_manager_class_init (GcalSourceManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gcal_source_manager_finalize;
  object_class->get_property = gcal_source_manager_get_property;
  object_class->set_property = gcal_source_manager_set_property;
}

static void
gcal_source_manager_init (GcalSourceManager *self)
{
}
