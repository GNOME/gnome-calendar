/* gcal-search-hit-row.c
 *
 * Copyright 2022 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#include "gcal-search-hit-row.h"

struct _GcalSearchHitRow
{
  GtkListBoxRow       parent_instance;

  GtkImage           *image;
  GtkWidget          *separator;
  GtkLabel           *subtitle;
  GtkLabel           *title;

  GcalSearchHit      *search_hit;
};

G_DEFINE_FINAL_TYPE (GcalSearchHitRow, gcal_search_hit_row, GTK_TYPE_LIST_BOX_ROW)

enum
{
  PROP_0,
  PROP_SEARCH_HIT,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];


/*
 * Auxiliary methods
 */

static void
update_search_hit (GcalSearchHitRow *self)
{
  g_autofree gchar *escaped_title = NULL;
  const gchar *subtitle;

  gtk_image_set_from_paintable (self->image, gcal_search_hit_get_primary_icon (self->search_hit));

  escaped_title = g_markup_escape_text (gcal_search_hit_get_title (self->search_hit), -1);
  gtk_label_set_label (self->title, escaped_title);

  subtitle = gcal_search_hit_get_subtitle (self->search_hit);
  if (subtitle)
    {
      g_autofree gchar *escaped_subtitle = NULL;

      escaped_subtitle = g_markup_escape_text (subtitle, -1);
      escaped_subtitle = g_strstrip (escaped_subtitle);
      gtk_label_set_label (self->subtitle, escaped_subtitle);

      gtk_widget_set_visible (self->separator, escaped_subtitle && *escaped_subtitle != '\0');
    }
  else
    {
      gtk_widget_set_visible (self->separator, FALSE);
    }
}


/*
 * Callbacks
 */

static void
on_search_hit_changed_cb (GcalSearchHit    *search_hit,
                          GParamSpec       *pspec,
                          GcalSearchHitRow *self)
{
  update_search_hit (self);
}

/*
 * GObject overrides
 */

static void
gcal_search_hit_row_finalize (GObject *object)
{
  GcalSearchHitRow *self = (GcalSearchHitRow *)object;

  g_clear_object (&self->search_hit);

  G_OBJECT_CLASS (gcal_search_hit_row_parent_class)->finalize (object);
}

static void
gcal_search_hit_row_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GcalSearchHitRow *self = GCAL_SEARCH_HIT_ROW (object);

  switch (prop_id)
    {
    case PROP_SEARCH_HIT:
      g_value_set_object (value, self->search_hit);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_search_hit_row_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GcalSearchHitRow *self = GCAL_SEARCH_HIT_ROW (object);

  switch (prop_id)
    {
    case PROP_SEARCH_HIT:
      g_assert (self->search_hit == NULL);
      self->search_hit = g_value_dup_object (value);
      g_signal_connect_object (self->search_hit, "notify", G_CALLBACK (on_search_hit_changed_cb), self, 0);
      update_search_hit (self);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_search_hit_row_class_init (GcalSearchHitRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gcal_search_hit_row_finalize;
  object_class->get_property = gcal_search_hit_row_get_property;
  object_class->set_property = gcal_search_hit_row_set_property;

  properties[PROP_SEARCH_HIT] = g_param_spec_object ("search-hit",
                                                     NULL,
                                                     NULL,
                                                     GCAL_TYPE_SEARCH_HIT,
                                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/gui/gcal-search-hit-row.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalSearchHitRow, image);
  gtk_widget_class_bind_template_child (widget_class, GcalSearchHitRow, separator);
  gtk_widget_class_bind_template_child (widget_class, GcalSearchHitRow, subtitle);
  gtk_widget_class_bind_template_child (widget_class, GcalSearchHitRow, title);
}

static void
gcal_search_hit_row_init (GcalSearchHitRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
gcal_search_hit_row_new (GcalSearchHit *search_hit)
{
  return g_object_new (GCAL_TYPE_SEARCH_HIT_ROW,
                       "search-hit", search_hit,
                       NULL);
}

GcalSearchHit *
gcal_search_hit_row_get_search_hit (GcalSearchHitRow *self)
{
  g_return_val_if_fail (GCAL_IS_SEARCH_HIT_ROW (self), NULL);

  return self->search_hit;
}
