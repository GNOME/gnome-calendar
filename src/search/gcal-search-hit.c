/* gcal-search-hit.c
 *
 * Copyright 2019 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "gcal-search-hit.h"

typedef struct
{
  gchar              *id;
  gchar              *title;
  gchar              *subtitle;

  GdkPaintable       *primary_icon;
} GcalSearchHitPrivate;


G_DEFINE_TYPE_WITH_PRIVATE (GcalSearchHit, gcal_search_hit, G_TYPE_OBJECT)

enum
{
  PROP_0,
  PROP_ID,
  PROP_SUBTITLE,
  PROP_TITLE,
  PROP_PRIMARY_ICON,
  N_PROPS,
};

static GParamSpec *properties [N_PROPS];

/*
 * GcalSearchHit overrides
 */

static void
gcal_search_hit_real_activate (GcalSearchHit *self,
                               GtkWidget     *for_widget)
{
}

static gint
gcal_search_hit_real_compare (GcalSearchHit *a,
                              GcalSearchHit *b)
{
  return 0;
}

static gint
gcal_search_hit_real_get_priority (GcalSearchHit *self)
{
  return 0;
}


/*
 * GObject overrides
 */

static void
gcal_search_hit_finalize (GObject *object)
{
  GcalSearchHit *self = (GcalSearchHit *)object;
  GcalSearchHitPrivate *priv = gcal_search_hit_get_instance_private (self);

  g_clear_pointer (&priv->id, g_free);
  g_clear_pointer (&priv->title, g_free);
  g_clear_pointer (&priv->subtitle, g_free);
  g_clear_object (&priv->primary_icon);

  G_OBJECT_CLASS (gcal_search_hit_parent_class)->finalize (object);
}

static void
gcal_search_hit_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GcalSearchHit *self = GCAL_SEARCH_HIT (object);

  switch (prop_id)
    {
    case PROP_ID:
      g_value_set_string (value, gcal_search_hit_get_id (self));
      break;

    case PROP_TITLE:
      g_value_set_string (value, gcal_search_hit_get_title (self));
      break;

    case PROP_SUBTITLE:
      g_value_set_string (value, gcal_search_hit_get_subtitle (self));
      break;

    case PROP_PRIMARY_ICON:
      g_value_set_object (value, gcal_search_hit_get_primary_icon (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_search_hit_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GcalSearchHit *self = GCAL_SEARCH_HIT (object);

  switch (prop_id)
    {
    case PROP_ID:
      gcal_search_hit_set_id (self, g_value_get_string (value));
      break;

    case PROP_TITLE:
      gcal_search_hit_set_title (self, g_value_get_string (value));
      break;

    case PROP_SUBTITLE:
      gcal_search_hit_set_subtitle (self, g_value_get_string (value));
      break;

    case PROP_PRIMARY_ICON:
      gcal_search_hit_set_primary_icon (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_search_hit_class_init (GcalSearchHitClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gcal_search_hit_finalize;
  object_class->get_property = gcal_search_hit_get_property;
  object_class->set_property = gcal_search_hit_set_property;

  klass->activate = gcal_search_hit_real_activate;
  klass->compare = gcal_search_hit_real_compare;
  klass->get_priority = gcal_search_hit_real_get_priority;

  properties [PROP_ID] =
    g_param_spec_string ("id",
                         "Id",
                         "The suggestion identifier",
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  properties [PROP_TITLE] =
    g_param_spec_string ("title",
                         "Title",
                         "The title of the suggestion",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_SUBTITLE] =
    g_param_spec_string ("subtitle",
                         "Subtitle",
                         "The subtitle of the suggestion",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_PRIMARY_ICON] =
    g_param_spec_object ("primary-icon",
                         "Primary icon",
                         "The primary icon for the suggestion",
                         GDK_TYPE_PAINTABLE,
                         G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

}

static void
gcal_search_hit_init (GcalSearchHit *self)
{
}

/**
 * gcal_search_hit_new:
 *
 * Create a new #GcalSearchHit.
 *
 * Returns: (transfer full): a newly created #GcalSearchHit
 */
GcalSearchHit *
gcal_search_hit_new (void)
{
  return g_object_new (GCAL_TYPE_SEARCH_HIT, NULL);
}

const gchar *
gcal_search_hit_get_id (GcalSearchHit *self)
{
  GcalSearchHitPrivate *priv = gcal_search_hit_get_instance_private (self);

  g_return_val_if_fail (GCAL_IS_SEARCH_HIT (self), NULL);

  return priv->id;
}

void
gcal_search_hit_set_id (GcalSearchHit *self,
                       const gchar   *id)
{
  GcalSearchHitPrivate *priv = gcal_search_hit_get_instance_private (self);

  g_return_if_fail (GCAL_IS_SEARCH_HIT (self));

  if (g_strcmp0 (priv->id, id) != 0)
    {
      g_free (priv->id);
      priv->id = g_strdup (id);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ID]);
    }
}

const gchar *
gcal_search_hit_get_title (GcalSearchHit *self)
{
  GcalSearchHitPrivate *priv = gcal_search_hit_get_instance_private (self);

  g_return_val_if_fail (GCAL_IS_SEARCH_HIT (self), NULL);

  return priv->title;
}

void
gcal_search_hit_set_title (GcalSearchHit *self,
                          const gchar   *title)
{
  GcalSearchHitPrivate *priv = gcal_search_hit_get_instance_private (self);

  g_return_if_fail (GCAL_IS_SEARCH_HIT (self));

  if (g_strcmp0 (priv->title, title) != 0)
    {
      g_clear_pointer (&priv->title, g_free);
      priv->title = g_strdup (title);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_TITLE]);
    }
}

const gchar *
gcal_search_hit_get_subtitle (GcalSearchHit *self)
{
  GcalSearchHitPrivate *priv = gcal_search_hit_get_instance_private (self);

  g_return_val_if_fail (GCAL_IS_SEARCH_HIT (self), NULL);

  return priv->subtitle;
}

void
gcal_search_hit_set_subtitle (GcalSearchHit *self,
                              const gchar   *subtitle)
{
  GcalSearchHitPrivate *priv = gcal_search_hit_get_instance_private (self);

  g_return_if_fail (GCAL_IS_SEARCH_HIT (self));

  if (g_strcmp0 (priv->subtitle, subtitle) != 0)
    {
      g_clear_pointer (&priv->subtitle, g_free);
      priv->subtitle = g_strdup (subtitle);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SUBTITLE]);
    }
}

GdkPaintable *
gcal_search_hit_get_primary_icon (GcalSearchHit *self)
{
  GcalSearchHitPrivate *priv = gcal_search_hit_get_instance_private (self);

  g_return_val_if_fail (GCAL_IS_SEARCH_HIT (self), NULL);

  return priv->primary_icon;
}

void
gcal_search_hit_set_primary_icon (GcalSearchHit *self,
                                  GdkPaintable  *primary_icon)
{
  GcalSearchHitPrivate *priv = gcal_search_hit_get_instance_private (self);

  g_return_if_fail (GCAL_IS_SEARCH_HIT (self));
  g_return_if_fail (!primary_icon || GDK_IS_PAINTABLE (primary_icon));

  if (g_set_object (&priv->primary_icon, primary_icon))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PRIMARY_ICON]);
}

void
gcal_search_hit_activate (GcalSearchHit *self,
                          GtkWidget     *for_widget)
{
  g_return_if_fail (GCAL_IS_SEARCH_HIT (self));

  GCAL_SEARCH_HIT_GET_CLASS (self)->activate (self, for_widget);
}

gint
gcal_search_hit_get_priority (GcalSearchHit *self)
{
  g_return_val_if_fail (GCAL_IS_SEARCH_HIT (self), 0);

  return GCAL_SEARCH_HIT_GET_CLASS (self)->get_priority (self);
}

gint
gcal_search_hit_compare (GcalSearchHit *a,
                         GcalSearchHit *b)
{
  g_return_val_if_fail (GCAL_IS_SEARCH_HIT (a), 0);
  g_return_val_if_fail (GCAL_IS_SEARCH_HIT (b), 0);

  return GCAL_SEARCH_HIT_GET_CLASS (a)->compare (a, b);
}
