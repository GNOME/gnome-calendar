/* -*- mode: c; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gcal-days-grid.c
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

#include "gcal-days-grid.h"
#include "gcal-event-widget.h"

#include <glib/gi18n.h>

#include <string.h>

static const double dashed [] =
{
  1.0,
  1.0
};

enum
{
  PROP_0,
  PROP_COLUMNS,
  PROP_SPACING,
  PROP_FOLDED
};

struct _ChildInfo
{
  GtkWidget *widget;

  guint      cell;
  gboolean   hidden;
};

typedef struct _ChildInfo ChildInfo;

struct _GcalDaysGridPrivate
{
  /* property */
  guint           columns_nr;
  guint           spacing;
  gboolean        folded;

  guint           scale_width;
  guint           req_cell_height;

  GList          *children;

  GdkWindow      *event_window;
};

static void           gcal_days_grid_finalize              (GObject        *object);

static void           gcal_days_grid_set_property          (GObject        *object,
                                                            guint           property_id,
                                                            const GValue   *value,
                                                            GParamSpec     *pspec);

static void           gcal_days_grid_get_property          (GObject        *object,
                                                            guint           property_id,
                                                            GValue         *value,
                                                            GParamSpec     *pspec);

static void           gcal_days_grid_get_preferred_width   (GtkWidget      *widget,
                                                            gint           *minimum,
                                                            gint           *natural);

static void           gcal_days_grid_get_preferred_height  (GtkWidget      *widget,
                                                            gint           *minimum,
                                                            gint           *natural);

static void           gcal_days_grid_realize               (GtkWidget      *widget);

static void           gcal_days_grid_unrealize             (GtkWidget      *widget);

static void           gcal_days_grid_map                   (GtkWidget      *widget);

static void           gcal_days_grid_unmap                 (GtkWidget      *widget);

static void           gcal_days_grid_size_allocate         (GtkWidget      *widget,
                                                            GtkAllocation  *allocation);

static gboolean       gcal_days_grid_draw                  (GtkWidget      *widget,
                                                            cairo_t        *cr);

static gboolean       gcal_days_grid_button_press_event    (GtkWidget      *widget,
                                                            GdkEventButton *event);

static gboolean       gcal_days_grid_button_release_event  (GtkWidget      *widget,
                                                            GdkEventButton *event);

static void           gcal_days_grid_add                   (GtkContainer    *container,
                                                            GtkWidget       *widget);

static void           gcal_days_grid_remove                (GtkContainer    *container,
                                                            GtkWidget       *widget);

static void           gcal_days_grid_forall                (GtkContainer    *container,
                                                            gboolean         include_internals,
                                                            GtkCallback      callback,
                                                            gpointer         callback_data);

G_DEFINE_TYPE (GcalDaysGrid,gcal_days_grid, GTK_TYPE_CONTAINER);

static void
gcal_days_grid_class_init (GcalDaysGridClass *klass)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = gcal_days_grid_finalize;
  object_class->set_property = gcal_days_grid_set_property;
  object_class->get_property = gcal_days_grid_get_property;

  widget_class = GTK_WIDGET_CLASS (klass);
  widget_class->get_preferred_width = gcal_days_grid_get_preferred_width;
  widget_class->get_preferred_height = gcal_days_grid_get_preferred_height;
  widget_class->realize = gcal_days_grid_realize;
  widget_class->unrealize = gcal_days_grid_unrealize;
  widget_class->map = gcal_days_grid_map;
  widget_class->unmap = gcal_days_grid_unmap;
  widget_class->size_allocate = gcal_days_grid_size_allocate;
  widget_class->draw = gcal_days_grid_draw;
  widget_class->button_press_event = gcal_days_grid_button_press_event;
  widget_class->button_release_event = gcal_days_grid_button_release_event;

  container_class = GTK_CONTAINER_CLASS (klass);
  container_class->add   = gcal_days_grid_add;
  container_class->remove = gcal_days_grid_remove;
  container_class->forall = gcal_days_grid_forall;
  gtk_container_class_handle_border_width (container_class);

  g_object_class_install_property (
      object_class,
      PROP_COLUMNS,
      g_param_spec_uint ("columns",
                         "Columns",
                         "The number of columns in which to split the grid",
                         0,
                         G_MAXUINT,
                         0,
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_READWRITE));

  g_object_class_install_property (
      object_class,
      PROP_SPACING,
      g_param_spec_uint ("spacing",
                         "Spacing",
                         "The horizontal space between the header and the side and below lines",
                         0,
                         G_MAXINT,
                         0,
                         G_PARAM_CONSTRUCT |
                         G_PARAM_READWRITE));

  g_object_class_install_property (
      object_class,
      PROP_FOLDED,
      g_param_spec_boolean ("folded",
                            "Folded status",
                            "Whether the view is folded or not",
                            FALSE,
                            G_PARAM_CONSTRUCT |
                            G_PARAM_READWRITE));

  g_type_class_add_private ((gpointer)klass, sizeof (GcalDaysGridPrivate));
}



static void
gcal_days_grid_init (GcalDaysGrid *self)
{
  GcalDaysGridPrivate *priv;

  gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            GCAL_TYPE_DAYS_GRID,
                                            GcalDaysGridPrivate);
  priv = self->priv;
  priv->scale_width = 0;
  priv->req_cell_height = 0;

  priv->children = NULL;
}

static void
gcal_days_grid_finalize (GObject *object)
{
  GcalDaysGridPrivate *priv;

  priv = GCAL_DAYS_GRID (object)->priv;

  if (priv->children != NULL)
    g_list_free_full (priv->children, (GDestroyNotify) g_list_free);

  /* Chain up to parent's finalize() method. */
  G_OBJECT_CLASS (gcal_days_grid_parent_class)->finalize (object);
}

static void
gcal_days_grid_set_property (GObject      *object,
                             guint         property_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GcalDaysGridPrivate *priv;
  priv = GCAL_DAYS_GRID (object)->priv;

  switch (property_id)
    {
    case PROP_COLUMNS:
      {
        gint i;
        priv->columns_nr = g_value_get_uint (value);
        for (i = 0; i < priv->columns_nr; ++i)
          {
            priv->children = g_list_prepend (priv->children, NULL);
          }
        break;
      }
    case PROP_SPACING:
      priv->spacing = g_value_get_uint (value);
      break;
    case PROP_FOLDED:
      priv->folded = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gcal_days_grid_get_property (GObject    *object,
                             guint       property_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GcalDaysGridPrivate *priv;
  priv = GCAL_DAYS_GRID (object)->priv;

  switch (property_id)
    {
    case PROP_COLUMNS:
      g_value_set_uint (value, priv->columns_nr);
      break;
    case PROP_SPACING:
      g_value_set_uint (value, priv->spacing);
      break;
    case PROP_FOLDED:
      g_value_set_boolean (value, priv->folded);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

/* GtkWidget API */
static void
gcal_days_grid_get_preferred_width (GtkWidget *widget,
                                    gint      *minimum,
                                    gint      *natural)
{
  GcalDaysGridPrivate* priv;

  GtkBorder padding;

  GList *columns;

  gint nat_keeper;
  gint nat_width;

  gint min_keeper;
  gint min_width;

  priv = GCAL_DAYS_GRID (widget)->priv;

  gtk_style_context_get_padding (gtk_widget_get_style_context (widget),
                                 gtk_widget_get_state_flags (widget),
                                 &padding);

  gcal_days_grid_get_scale_width (GCAL_DAYS_GRID (widget));

  nat_keeper = 0;
  min_keeper = G_MAXINT;

  for (columns = priv->children;
       columns != NULL;
       columns = g_list_next (columns))
    {
      GList *column;

      column = (GList*) columns->data;
      while (column)
        {
          ChildInfo *info = (ChildInfo*) column->data;
          column = column->next;

          gtk_widget_get_preferred_width (info->widget, &min_width, &nat_width);
          if (nat_keeper < nat_width)
            nat_keeper = nat_width;
          if (min_keeper > min_width)
            min_keeper = min_width;
        }
    }

  if (min_keeper == G_MAXINT)
    min_keeper = 0;

  if (minimum != NULL)
    {
      *minimum =
        (priv->columns_nr * min_keeper) + padding.left + padding.right + priv->scale_width;
    }
  if (natural != NULL)
    {
      *natural =
        (priv->columns_nr * nat_keeper) + padding.left + padding.right + priv->scale_width;
    }
}

static void
gcal_days_grid_get_preferred_height (GtkWidget *widget,
                                     gint      *minimum,
                                     gint      *natural)
{
  GcalDaysGridPrivate* priv;

  GtkBorder padding;

  priv = GCAL_DAYS_GRID (widget)->priv;

  gtk_style_context_get_padding (gtk_widget_get_style_context (widget),
                                 gtk_widget_get_state_flags (widget),
                                 &padding);

  if (minimum != NULL)
    {
      *minimum = priv->req_cell_height * 48 + padding.top + padding.bottom;
    }
  if (natural != NULL)
    {
      *natural = priv->req_cell_height * 48 + padding.top + padding.bottom;
    }
}

static void
gcal_days_grid_realize (GtkWidget *widget)
{
  GcalDaysGridPrivate *priv;
  GdkWindow *parent_window;
  GdkWindowAttr attributes;
  gint attributes_mask;
  GtkAllocation allocation;

  priv = GCAL_DAYS_GRID (widget)->priv;
  gtk_widget_set_realized (widget, TRUE);

  parent_window = gtk_widget_get_parent_window (widget);
  gtk_widget_set_window (widget, parent_window);
  g_object_ref (parent_window);

  gtk_widget_get_allocation (widget, &allocation);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.x = allocation.x;
  attributes.y = allocation.y;
  attributes.width = allocation.width;
  attributes.height = allocation.height;
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= (GDK_BUTTON_PRESS_MASK |
                            GDK_BUTTON_RELEASE_MASK |
                            GDK_BUTTON1_MOTION_MASK |
                            GDK_POINTER_MOTION_HINT_MASK |
                            GDK_POINTER_MOTION_MASK |
                            GDK_ENTER_NOTIFY_MASK |
                            GDK_LEAVE_NOTIFY_MASK);
  attributes_mask = GDK_WA_X | GDK_WA_Y;

  priv->event_window = gdk_window_new (parent_window,
                                       &attributes,
                                       attributes_mask);
  gdk_window_set_user_data (priv->event_window, widget);
}

static void
gcal_days_grid_unrealize (GtkWidget *widget)
{
  GcalDaysGridPrivate *priv;

  priv = GCAL_DAYS_GRID (widget)->priv;
  if (priv->event_window != NULL)
    {
      gdk_window_set_user_data (priv->event_window, NULL);
      gdk_window_destroy (priv->event_window);
      priv->event_window = NULL;
    }

  GTK_WIDGET_CLASS (gcal_days_grid_parent_class)->unrealize (widget);
}

static void
gcal_days_grid_map (GtkWidget *widget)
{
  GcalDaysGridPrivate *priv;

  priv = GCAL_DAYS_GRID (widget)->priv;
  if (priv->event_window != NULL)
    gdk_window_show (priv->event_window);

  GTK_WIDGET_CLASS (gcal_days_grid_parent_class)->map (widget);
}

static void
gcal_days_grid_unmap (GtkWidget *widget)
{
  GcalDaysGridPrivate *priv;

  priv = GCAL_DAYS_GRID (widget)->priv;
  if (priv->event_window != NULL)
    gdk_window_hide (priv->event_window);

  GTK_WIDGET_CLASS (gcal_days_grid_parent_class)->map (widget);
}

static void
gcal_days_grid_size_allocate (GtkWidget     *widget,
                              GtkAllocation *allocation)
{
  /* GcalDaysGridPrivate *priv; */

  /* priv = GCAL_DAYS_GRID (widget)->priv; */
  gtk_widget_set_allocation (widget, allocation);

  /* FIXME: Todo, write later,
   maybe write through delegation */
}

static gboolean
gcal_days_grid_draw (GtkWidget *widget,
                     cairo_t   *cr)
{
  GcalDaysGridPrivate *priv;

  GtkBorder padding;
  GtkAllocation alloc;
  gint width;
  gint height;

  GdkRGBA ligther_color;
  GdkRGBA font_color;

  PangoLayout *layout;
  PangoFontDescription *font_desc;

  gint i;

  priv = GCAL_DAYS_GRID (widget)->priv;

  gtk_style_context_get_padding (
      gtk_widget_get_style_context (widget),
      gtk_widget_get_state_flags (widget),
      &padding);

  gtk_widget_get_allocation (widget, &alloc);
  width = alloc.width - (padding.left + padding.right + priv->scale_width);
  height = alloc.height - (padding.top + padding.bottom);

  gtk_style_context_get_color (
      gtk_widget_get_style_context (widget),
      gtk_widget_get_state_flags (widget),
      &font_color);

  gtk_style_context_get_color (
      gtk_widget_get_style_context (widget),
      gtk_widget_get_state_flags (widget) | GTK_STATE_FLAG_INSENSITIVE,
      &ligther_color);

  gtk_style_context_get (
      gtk_widget_get_style_context (widget),
      gtk_widget_get_state_flags (widget),
      "font", &font_desc, NULL);

  cairo_save (cr);

  /* drawing text of the sidebar */
  layout = pango_cairo_create_layout (cr);
  pango_layout_set_font_description (layout, font_desc);

  cairo_set_source_rgb (cr,
                        font_color.red,
                        font_color.green,
                        font_color.blue);

  for (i = 0; i < 24; i++)
    {
      gchar *hours;
      hours = g_strdup_printf ("%d:00 %s", i % 12, i < 12 ? _("AM") : _("PM"));

      if (i == 0)
        pango_layout_set_text (layout, _("Midnight"), -1);
      else if (i == 12)
        pango_layout_set_text (layout, _("Noon"), -1);
      else
        pango_layout_set_text (layout, hours, -1);

      pango_cairo_update_layout (cr, layout);
      cairo_move_to (cr,
                     padding.left + priv->spacing,
                     padding.top + (height / 24) * i);
      pango_cairo_show_layout (cr, layout);

      g_free (hours);
    }

  /* drawing lines */
  cairo_set_source_rgb (cr,
                        ligther_color.red,
                        ligther_color.green,
                        ligther_color.blue);
  cairo_set_line_width (cr, 0.3);

  /* grid, vertical lines first */
  for (i = 0; i < priv->columns_nr + 1; i++)
    {
      cairo_move_to (cr,
                     padding.left + priv->scale_width + (width / priv->columns_nr) * i + 0.4,
                     0);
      cairo_rel_line_to (cr, 0, height);
    }

  /* rest of the lines */
  for (i = 0; i < 24; i++)
    {
      /* hours lines */
      cairo_move_to (cr, padding.left, (height / 24) * i + 0.4);
      cairo_rel_line_to (cr, width + priv->scale_width, 0);
    }

  cairo_stroke (cr);

  /* 30 minutes lines */
  cairo_set_dash (cr, dashed, 2, 0);
  for (i = 0; i < 24; i++)
    {
      cairo_move_to (cr, padding.left + priv->scale_width, (height / 24) * i + (height / 48) + 0.4);
      cairo_rel_line_to (cr, width + priv->scale_width, 0);
    }

  cairo_stroke (cr);
  cairo_restore (cr);

  /* drawing children */
  if (GTK_WIDGET_CLASS (gcal_days_grid_parent_class)->draw != NULL)
    GTK_WIDGET_CLASS (gcal_days_grid_parent_class)->draw (widget, cr);

  pango_font_description_free (font_desc);
  g_object_unref (layout);

  return FALSE;
}

static gboolean
gcal_days_grid_button_press_event (GtkWidget      *widget,
                                   GdkEventButton *event)
{
  g_debug ("Button pressed on days-grid area");
  return FALSE;
}

static gboolean
gcal_days_grid_button_release_event (GtkWidget      *widget,
                                     GdkEventButton *event)
{
  g_debug ("Button released on days-grid area");
  return FALSE;
}

/* GtkContainer API */
/*
 * gcal_days_grid_add:
 *
 * @gtk_container_add implementation. It assumes the widget will go
 * to the first column. If there's no column set, will set columns to 1
 */
static void
gcal_days_grid_add (GtkContainer *container,
                    GtkWidget    *widget)
{
  GcalDaysGridPrivate* priv;

  priv = GCAL_DAYS_GRID (container)->priv;
  g_return_if_fail (priv->columns_nr != 0);

  gcal_days_grid_place (GCAL_DAYS_GRID (container),
                        widget,
                        0, 0);
}

static void
gcal_days_grid_remove (GtkContainer *container,
                       GtkWidget    *widget)
{
  GcalDaysGridPrivate *priv;

  GList *columns;

  priv = GCAL_DAYS_GRID (container)->priv;

  if (priv->children != NULL)
    {
      for (columns = priv->children;
           columns != NULL;
           columns = g_list_next (columns))
        {
          GList* column = (GList*) columns->data;
          for (;column != NULL; column = g_list_next (column))
            {
              ChildInfo *info = (ChildInfo*) column->data;
              if (widget == info->widget)
                {
                  GList* orig = (GList*) columns->data;
                  orig = g_list_delete_link (orig, column);

                  g_free (info);

                  gtk_widget_unparent (widget);

                  columns->data = orig;

                  columns = NULL;
                  break;
                }
            }
        }
    }
}

static void
gcal_days_grid_forall (GtkContainer *container,
                       gboolean      include_internals,
                       GtkCallback   callback,
                       gpointer      callback_data)
{
  GcalDaysGridPrivate *priv;

  GList* columns;

  priv = GCAL_DAYS_GRID (container)->priv;

  if (priv->children != NULL)
    {
      columns = priv->children;

      while (columns)
        {
          GList *column;

          column = columns->data;
          columns = columns->next;

          while (column)
            {
              ChildInfo *info = (ChildInfo*) column->data;
              column  = column->next;

              (* callback) (info->widget, callback_data);
            }
        }
    }
}

/* Public API */
/**
 * gcal_days_grid_new:
 *
 * Since: 0.1
 * Return value: A new #GcalDaysGrid
 * Returns: (transfer full):
 **/
GtkWidget*
gcal_days_grid_new (guint columns)
{
  return g_object_new (GCAL_TYPE_DAYS_GRID, "columns", columns, NULL);
}

void
gcal_days_grid_set_preferred_cell_height (GcalDaysGrid *days_grid,
                                          guint         cell_height)
{
  GcalDaysGridPrivate *priv;
  priv = days_grid->priv;

  priv->req_cell_height = cell_height;
}

guint
gcal_days_grid_get_scale_width (GcalDaysGrid *days_grid)
{
  GcalDaysGridPrivate *priv;
  priv = days_grid->priv;

  if (priv->scale_width == 0)
    {
      GtkWidget *widget;

      PangoLayout *layout;
      PangoFontDescription *font_desc;

      gint mid_width;

      widget = GTK_WIDGET (days_grid);

      layout = gtk_widget_create_pango_layout (widget, "00:00 0000");
      gtk_style_context_get (gtk_widget_get_style_context (widget),
                             gtk_widget_get_state_flags (widget),
                             "font", &font_desc,
                             NULL);
      pango_layout_set_font_description (layout, font_desc);

      pango_layout_get_pixel_size (layout, &mid_width, NULL);

      priv->scale_width = mid_width + priv->spacing * 2;

      pango_font_description_free (font_desc);
      g_object_unref (layout);
    }

  return priv->scale_width;
}

/**
 * gcal_days_grid_place:
 * @all_day: A #GcalDaysGrid widget
 * @widget: The child widget to add
 * @column_idx: The index of the column, starting with zero
 * @cell_idx: The cell, refering the month, starting with zero
 *
 * Adding a widget to a specified column. If the column index
 * is bigger than the #GcalDaysGrid:columns property set,
 * it will do nothing.
 **/
void
gcal_days_grid_place (GcalDaysGrid *all_day,
                      GtkWidget    *widget,
                      guint         column_idx,
                      guint         cell_idx)
{
  GcalDaysGridPrivate *priv;
  GList* children_link;
  GList* column;
  ChildInfo *info;

  priv = all_day->priv;

  g_return_if_fail (column_idx < priv->columns_nr);

  children_link = g_list_nth (priv->children, column_idx);
  column = (GList*) children_link->data;

  info = g_new0 (ChildInfo, 1);
  info->widget = widget;
  info->hidden = FALSE;
  info->cell = cell_idx;
  children_link->data = g_list_append (column, info);

  gtk_widget_set_parent (widget, GTK_WIDGET (all_day));
}

GtkWidget*
gcal_days_grid_get_by_uuid (GcalDaysGrid *days_grid,
                            const gchar  *uuid)
{
  GcalDaysGridPrivate *priv;

  GList* columns;

  priv = days_grid->priv;

  columns = priv->children;
  while (columns)
    {
      GList *column;

      column = columns->data;
      columns = columns->next;

      while (column)
        {
          GcalEventWidget *event;

          ChildInfo *info = (ChildInfo*) column->data;
          column  = column->next;

          event = GCAL_EVENT_WIDGET (info->widget);

          if (g_strcmp0 (uuid,
                         gcal_event_widget_peek_uuid (event)) == 0)
            {
              return info->widget;
            }
        }
    }

  return NULL;
}
