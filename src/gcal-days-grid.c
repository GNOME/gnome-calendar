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
#include <math.h>

/*
 * Notes:
 * 1. the widget's height is divided between 48 cells
 *    representing each a half-hour of the day.
 *    the preferred value of the cell is kept is req_cell_heigth
 *    private field.
 */

static const double dashed [] =
{
  1.0,
  1.0
};

enum
{
  PROP_0,
  PROP_COLUMNS,
  PROP_FOLDED
};

struct _ChildInfo
{
  GtkWidget *widget;

  guint      start_cell;
  guint      end_cell;
  guint      sub_column;

  gboolean   hidden;
};

typedef struct _ChildInfo ChildInfo;

typedef struct
{
  /* property */
  guint           columns_nr;
  guint           spacing;
  gboolean        folded;

  guint           scale_width;
  guint           req_cell_height;

  GList          *children;

  GdkWindow      *event_window;

  /* button_down/up flags */
  gint            clicked_cell;
  gint            start_mark_cell;
  gint            end_mark_cell;

} GcalDaysGridPrivate;

enum
{
  MARKED,
  NUM_SIGNALS
};

static guint signals[NUM_SIGNALS] = { 0, };

/* Helpers and private */
static gint           compare_child_info                   (gconstpointer   a,
                                                            gconstpointer   b);

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

static gboolean       gcal_days_grid_motion_notify_event   (GtkWidget      *widget,
                                                            GdkEventMotion *event);

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


G_DEFINE_TYPE_WITH_PRIVATE (GcalDaysGrid,gcal_days_grid, GTK_TYPE_CONTAINER);

/* Helpers and private */
static gint
compare_child_info (gconstpointer a,
                    gconstpointer b)
{
  ChildInfo *ac = (ChildInfo*) a;
  ChildInfo *bc = (ChildInfo*) b;

  if (ac->start_cell < bc->start_cell)
    return -1;
  else if (ac->start_cell == bc->start_cell)
    return 0;
  else
    return 1;
}


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
  widget_class->motion_notify_event = gcal_days_grid_motion_notify_event;
  widget_class->button_release_event = gcal_days_grid_button_release_event;

  container_class = GTK_CONTAINER_CLASS (klass);
  container_class->add = gcal_days_grid_add;
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
      PROP_FOLDED,
      g_param_spec_boolean ("folded",
                            "Folded status",
                            "Whether the view is folded or not",
                            FALSE,
                            G_PARAM_CONSTRUCT |
                            G_PARAM_READWRITE));

  /* comunicate to its parent view the marking */
  signals[MARKED] = g_signal_new ("marked",
                                  GCAL_TYPE_DAYS_GRID,
                                  G_SIGNAL_RUN_LAST,
                                  G_STRUCT_OFFSET (GcalDaysGridClass,
                                                   marked),
                                  NULL, NULL, NULL,
                                  G_TYPE_NONE,
                                  2,
                                  G_TYPE_UINT,
                                  G_TYPE_UINT);
}



static void
gcal_days_grid_init (GcalDaysGrid *self)
{
  GcalDaysGridPrivate *priv;

  gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);

  priv = gcal_days_grid_get_instance_private (self);
  priv->scale_width = 0;
  priv->req_cell_height = 0;

  priv->children = NULL;

  priv->clicked_cell = -1;
  priv->start_mark_cell = -1;
  priv->end_mark_cell = -1;

  gtk_style_context_add_class (
      gtk_widget_get_style_context (GTK_WIDGET (self)),
      "calendar-view");
}

static void
gcal_days_grid_finalize (GObject *object)
{
  GcalDaysGridPrivate *priv;

  priv = gcal_days_grid_get_instance_private (GCAL_DAYS_GRID (object));

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

  priv = gcal_days_grid_get_instance_private (GCAL_DAYS_GRID (object));

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

  priv = gcal_days_grid_get_instance_private (GCAL_DAYS_GRID (object));

  switch (property_id)
    {
    case PROP_COLUMNS:
      g_value_set_uint (value, priv->columns_nr);
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

  GList *columns;

  gint nat_keeper;
  gint nat_width;

  gint min_keeper;
  gint min_width;

  priv = gcal_days_grid_get_instance_private (GCAL_DAYS_GRID (widget));

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
      *minimum = (priv->columns_nr * min_keeper) + priv->scale_width;
    }
  if (natural != NULL)
    {
      *natural = (priv->columns_nr * nat_keeper) + priv->scale_width;
    }
}

static void
gcal_days_grid_get_preferred_height (GtkWidget *widget,
                                     gint      *minimum,
                                     gint      *natural)
{
  GcalDaysGridPrivate* priv;

  GtkBorder padding;

  priv = gcal_days_grid_get_instance_private (GCAL_DAYS_GRID (widget));

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

  priv = gcal_days_grid_get_instance_private (GCAL_DAYS_GRID (widget));
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

  priv = gcal_days_grid_get_instance_private (GCAL_DAYS_GRID (widget));
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

  priv = gcal_days_grid_get_instance_private (GCAL_DAYS_GRID (widget));
  if (priv->event_window != NULL)
    gdk_window_show (priv->event_window);

  GTK_WIDGET_CLASS (gcal_days_grid_parent_class)->map (widget);
}

static void
gcal_days_grid_unmap (GtkWidget *widget)
{
  GcalDaysGridPrivate *priv;

  priv = gcal_days_grid_get_instance_private (GCAL_DAYS_GRID (widget));
  if (priv->event_window != NULL)
    gdk_window_hide (priv->event_window);

  GTK_WIDGET_CLASS (gcal_days_grid_parent_class)->unmap (widget);
}

static void
gcal_days_grid_size_allocate (GtkWidget     *widget,
                              GtkAllocation *allocation)
{
  GcalDaysGridPrivate *priv;

  GtkBorder padding;
  gint width_block;
  gint height_block;

  GList *columns;
  gint idx;

  priv = gcal_days_grid_get_instance_private (GCAL_DAYS_GRID (widget));
  gtk_widget_set_allocation (widget, allocation);

  /* Placing the event_window */
  if (gtk_widget_get_realized (widget))
    {
      gdk_window_move_resize (priv->event_window,
                              allocation->x,
                              allocation->y,
                              allocation->width,
                              allocation->height);
    }

  gtk_style_context_get_padding (gtk_widget_get_style_context (widget),
                                 gtk_widget_get_state_flags (widget),
                                 &padding);

  width_block = (allocation->width - priv->scale_width) / priv->columns_nr;
  height_block = (allocation->height - (padding.top + padding.bottom)) / 48;

  /* detecting columns and allocs */
  for (columns = priv->children, idx = 0;
       columns != NULL;
       columns = g_list_next (columns), ++idx)
    {
      GtkAllocation child_allocation;
      GArray *last_y;
      GList* column = (GList*) columns->data;

      /* last_y value for each column,
         will keep the amount of columns as well */
      last_y = g_array_new (FALSE, FALSE, sizeof (guint));

      /* going through widgets */
      for (;column != NULL; column = g_list_next (column))
        {
          ChildInfo *info;
          gboolean new_column = TRUE;
          gint i;

          info = (ChildInfo*) column->data;

          for (i = 0; i < last_y->len; ++i)
            {
              /* start before */
              if (info->start_cell > g_array_index (last_y, guint, i))
                {
                  info->sub_column = i;
                  new_column = FALSE;
                  break;
                }
            }

          if (new_column)
            {
              /* adding column to array */
              g_array_append_val (last_y, info->end_cell);
              info->sub_column = last_y->len - 1;
            }
          else
            {
              g_array_index (last_y, guint, info->sub_column) = info->end_cell;
            }
        }

      /* allocating */
      for (column = (GList*) columns->data;
           column != NULL;
           column = g_list_next (column))
        {
          ChildInfo *info;
          guint sub_column_block;

          info = (ChildInfo*) column->data;
          sub_column_block = width_block / last_y->len;

          child_allocation.x = allocation->x + priv->scale_width +
            idx * width_block + sub_column_block * info->sub_column;
          child_allocation.y = info->start_cell * height_block;
          child_allocation.width = sub_column_block;
          child_allocation.height = (info->end_cell - info->start_cell) * height_block;

          gtk_widget_size_allocate (info->widget, &child_allocation);
        }

      g_array_free (last_y, TRUE);
    }
}

static gboolean
gcal_days_grid_draw (GtkWidget *widget,
                     cairo_t   *cr)
{
  GcalDaysGridPrivate *priv;

  GtkBorder padding;
  GtkAllocation alloc;
  gint width;

  GdkRGBA ligther_color;
  GdkRGBA background_selected_color;
  GdkRGBA font_color;

  PangoLayout *layout;
  PangoFontDescription *font_desc;

  gint i;

  priv = gcal_days_grid_get_instance_private (GCAL_DAYS_GRID (widget));

  gtk_style_context_get_padding (
      gtk_widget_get_style_context (widget),
      gtk_widget_get_state_flags (widget),
      &padding);

  gtk_widget_get_allocation (widget, &alloc);
  width = alloc.width - (priv->scale_width);

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
                     padding.left,
                     padding.top + (alloc.height / 24) * i);
      pango_cairo_show_layout (cr, layout);

      g_free (hours);
    }

  /* drawing new-event mark */
  if (priv->start_mark_cell != -1 &&
      priv->end_mark_cell != -1)
    {
      gint first_cell;
      gint last_cell;
      gint columns;

      gtk_style_context_get_background_color (
          gtk_widget_get_style_context (widget),
          gtk_widget_get_state_flags (widget) | GTK_STATE_FLAG_SELECTED,
          &background_selected_color);

      if (priv->start_mark_cell < priv->end_mark_cell)
        {
          first_cell = priv->start_mark_cell;
          last_cell = priv->end_mark_cell;
        }
      else
        {
          first_cell = priv->end_mark_cell;
          last_cell = priv->start_mark_cell;
        }

      cairo_save (cr);
      cairo_set_source_rgba (cr,
                             background_selected_color.red,
                             background_selected_color.green,
                             background_selected_color.blue,
                             background_selected_color.alpha);

      for (columns = 0; columns < last_cell / 48 - first_cell / 48 + 1; columns++)
        {
          gint first_point;
          gint last_point;

          first_point = (columns == 0) ? first_cell : ((first_cell / 48) + columns) * 48;
          last_point = (columns == (last_cell / 48 - first_cell / 48)) ? last_cell : ((first_cell / 48) + columns) * 48 + 47;

          cairo_rectangle (cr,
                           priv->scale_width + (width / priv->columns_nr) * (first_point / 48),
                           (first_point % 48) * (alloc.height / 48),
                           (width / priv->columns_nr),
                           (last_point - first_point + 1) * (alloc.height / 48));
          cairo_fill (cr);

        }
      cairo_restore (cr);
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
                     priv->scale_width + (width / priv->columns_nr) * i + 0.4,
                     0);
      cairo_rel_line_to (cr, 0, alloc.height);
    }

  /* rest of the lines */
  for (i = 0; i < 24; i++)
    {
      /* hours lines */
      cairo_move_to (cr, 0, (alloc.height / 24) * i + 0.4);
      cairo_rel_line_to (cr, width + priv->scale_width, 0);
    }

  cairo_stroke (cr);

  /* 30 minutes lines */
  cairo_set_dash (cr, dashed, 2, 0);
  for (i = 0; i < 24; i++)
    {
      cairo_move_to (cr, priv->scale_width, (alloc.height / 24) * i + (alloc.height / 48) + 0.4);
      cairo_rel_line_to (cr, width + priv->scale_width, 0);
    }

  cairo_stroke (cr);
  cairo_restore (cr);

  pango_font_description_free (font_desc);
  g_object_unref (layout);

  /* drawing children */
  if (GTK_WIDGET_CLASS (gcal_days_grid_parent_class)->draw != NULL)
    GTK_WIDGET_CLASS (gcal_days_grid_parent_class)->draw (widget, cr);

  return FALSE;
}

static gboolean
gcal_days_grid_button_press_event (GtkWidget      *widget,
                                   GdkEventButton *event)
{
  GcalDaysGridPrivate *priv;

  GtkBorder padding;
  GtkAllocation alloc;
  gint width_block;
  gint height_block;

  priv = gcal_days_grid_get_instance_private (GCAL_DAYS_GRID (widget));

  gtk_style_context_get_padding (
      gtk_widget_get_style_context (widget),
      gtk_widget_get_state_flags (widget),
      &padding);

  /* XXX: an improvement will be scroll to that position when
          click the scale at the side */
  if (event->x < priv->scale_width)
    return FALSE;

  gtk_widget_get_allocation (widget, &alloc);
  width_block = (alloc.width - priv->scale_width) / priv->columns_nr;
  height_block = (alloc.height - (padding.top + padding.bottom)) / 48;

  priv->clicked_cell = 48 * (((gint) event->x - priv->scale_width) / width_block) + event->y / height_block;
  priv->start_mark_cell = priv->clicked_cell;

  return TRUE;
}

static gboolean
gcal_days_grid_motion_notify_event (GtkWidget      *widget,
                                    GdkEventMotion *event)
{
  GcalDaysGridPrivate *priv;

  GtkBorder padding;
  GtkAllocation alloc;
  gint width_block;
  gint height_block;

  priv = gcal_days_grid_get_instance_private (GCAL_DAYS_GRID (widget));

  if (priv->clicked_cell == -1)
    return FALSE;

  gtk_style_context_get_padding (
      gtk_widget_get_style_context (widget),
      gtk_widget_get_state_flags (widget),
      &padding);

  gtk_widget_get_allocation (widget, &alloc);
  width_block = (alloc.width - priv->scale_width) / priv->columns_nr;
  height_block = (alloc.height - (padding.top + padding.bottom)) / 48;

  priv->end_mark_cell = 48 * (((gint) event->x - priv->scale_width) / width_block) + event->y / height_block;

  gtk_widget_queue_draw (widget);
  return TRUE;
}

static gboolean
gcal_days_grid_button_release_event (GtkWidget      *widget,
                                     GdkEventButton *event)
{
  GcalDaysGridPrivate *priv;

  GtkBorder padding;
  GtkAllocation alloc;
  gint width_block;
  gint height_block;

  gint cell_temp;

  priv = gcal_days_grid_get_instance_private (GCAL_DAYS_GRID (widget));

  if (priv->clicked_cell == -1)
    return FALSE;

  gtk_style_context_get_padding (
      gtk_widget_get_style_context (widget),
      gtk_widget_get_state_flags (widget),
      &padding);

  if (event->x < priv->scale_width)
    {
      priv->start_mark_cell = -1;
      priv->end_mark_cell = -1;
      return FALSE;
    }

  gtk_widget_get_allocation (widget, &alloc);
  width_block = (alloc.width - priv->scale_width) / priv->columns_nr;
  height_block = (alloc.height - (padding.top + padding.bottom)) / 48;

  priv->end_mark_cell = 48 * (((gint) event->x - priv->scale_width) / width_block) + event->y / height_block;

  gtk_widget_queue_draw (widget);

  if (priv->start_mark_cell > priv->end_mark_cell)
    {
      cell_temp = priv->end_mark_cell;
      priv->end_mark_cell = priv->start_mark_cell;
      priv->start_mark_cell = cell_temp;
    }

  g_signal_emit (GCAL_DAYS_GRID (widget),
                 signals[MARKED], 0,
                 priv->start_mark_cell, priv->end_mark_cell);

  priv->clicked_cell = -1;
  return TRUE;
}

/* GtkContainer API */
/**
 * gcal_days_grid_add:
 * @container:
 * @widget:
 *
 * @gtk_container_add implementation. It assumes the widget will go
 * to the first column. If there's no column set, will set columns to 1
 *
 **/
static void
gcal_days_grid_add (GtkContainer *container,
                    GtkWidget    *widget)
{
  GcalDaysGridPrivate* priv;

  priv = gcal_days_grid_get_instance_private (GCAL_DAYS_GRID (container));
  g_return_if_fail (priv->columns_nr != 0);

  gcal_days_grid_place (GCAL_DAYS_GRID (container),
                        widget,
                        0, 0, 0);
}

static void
gcal_days_grid_remove (GtkContainer *container,
                       GtkWidget    *widget)
{
  GcalDaysGridPrivate *priv;

  GList *columns;

  priv = gcal_days_grid_get_instance_private (GCAL_DAYS_GRID (container));

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

              gtk_widget_unparent (widget);

              columns->data = orig;

              columns = NULL;
              break;
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

  priv = gcal_days_grid_get_instance_private (GCAL_DAYS_GRID (container));

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

  priv = gcal_days_grid_get_instance_private (days_grid);

  priv->req_cell_height = cell_height;
}

/**
 * gcal_days_grid_get_scale_width:
 * @days_grid: a #GcalDaysGrid widget
 *
 * Retrieve the size of the scale in the left size
 * of the widget. The value is calculated the first
 * time the function is called. After that, the value
 * is cached internally.
 *
 * Returns: the size of the scale
 **/
guint
gcal_days_grid_get_scale_width (GcalDaysGrid *days_grid)
{
  GcalDaysGridPrivate *priv;

  priv = gcal_days_grid_get_instance_private (days_grid);

  if (priv->scale_width == 0)
    {
      GtkWidget *widget;

      GtkBorder padding;
      PangoLayout *layout;
      PangoFontDescription *font_desc;

      gint mid_width;

      widget = GTK_WIDGET (days_grid);

      gtk_style_context_get_padding (gtk_widget_get_style_context (widget),
                                     gtk_widget_get_state_flags (widget),
                                     &padding);

      layout = gtk_widget_create_pango_layout (widget, "00:00 0000");
      gtk_style_context_get (gtk_widget_get_style_context (widget),
                             gtk_widget_get_state_flags (widget),
                             "font", &font_desc,
                             NULL);
      pango_layout_set_font_description (layout, font_desc);

      pango_layout_get_pixel_size (layout, &mid_width, NULL);

      priv->scale_width = mid_width + padding.left + padding.right;

      pango_font_description_free (font_desc);
      g_object_unref (layout);
    }

  return priv->scale_width;
}

/**
 * gcal_days_grid_place:
 * @days_grid: a #GcalDaysGrid widget
 * @widget: the child widget to add
 * @column_idx: the index of the column, starting with zero
 * @start_cell: the start-cell, refering the hour, starting with zero
 * @end_cell: the end-cell, refering the hour, starting with zero
 *
 * Adding a widget to a specified column. If the column index
 * is bigger than the #GcalDaysGrid:columns property set,
 * it will do nothing.
 **/
void
gcal_days_grid_place (GcalDaysGrid *days_grid,
                      GtkWidget    *widget,
                      guint         column_idx,
                      guint         start_cell,
                      guint         end_cell)
{
  GcalDaysGridPrivate *priv;
  GList* children_link;
  GList* column;
  ChildInfo *info;

  priv = gcal_days_grid_get_instance_private (days_grid);

  g_return_if_fail (column_idx < priv->columns_nr);

  children_link = g_list_nth (priv->children, column_idx);
  column = (GList*) children_link->data;

  info = g_new0 (ChildInfo, 1);
  info->widget = widget;
  info->hidden = FALSE;
  info->start_cell = start_cell;
  info->end_cell = end_cell == 0 ? start_cell + 1 : end_cell;
  info->sub_column = 0;
  children_link->data = g_list_insert_sorted (column, info, compare_child_info);

  gtk_widget_set_parent (widget, GTK_WIDGET (days_grid));
}

GtkWidget*
gcal_days_grid_get_by_uuid (GcalDaysGrid *days_grid,
                            const gchar  *uuid)
{
  GcalDaysGridPrivate *priv;

  GList* columns;

  priv = gcal_days_grid_get_instance_private (days_grid);

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

void
gcal_days_grid_clear_marks (GcalDaysGrid *days_grid)
{
  GcalDaysGridPrivate *priv;

  priv = gcal_days_grid_get_instance_private (days_grid);

  priv->start_mark_cell = -1;
  priv->end_mark_cell = -1;
  gtk_widget_queue_draw (GTK_WIDGET (days_grid));
}

void
gcal_days_grid_mark_cell (GcalDaysGrid *days_grid,
                          guint         cell)
{
  GcalDaysGridPrivate *priv;

  priv = gcal_days_grid_get_instance_private (days_grid);

  priv->start_mark_cell = cell;
  priv->end_mark_cell = cell;
  gtk_widget_queue_draw (GTK_WIDGET (days_grid));
}

void
gcal_days_grid_get_cell_position (GcalDaysGrid *days_grid,
                                  guint         cell,
                                  gint         *x,
                                  gint         *y)
{
  GcalDaysGridPrivate *priv;

  GtkWidget *widget;
  GtkBorder padding;
  gint width;
  gint height;

  priv = gcal_days_grid_get_instance_private (days_grid);
  widget = GTK_WIDGET (days_grid);

  gtk_style_context_get_padding (
      gtk_widget_get_style_context (widget),
      gtk_widget_get_state_flags (widget),
      &padding);

  width = gtk_widget_get_allocated_width (widget) - priv->scale_width;
  height = gtk_widget_get_allocated_height (widget) - (padding.top + padding.bottom);

  *x = (gint) priv->scale_width + (width / priv->columns_nr) * (cell / 48 + 0.5);
  *y = (gint) (height / 48) * ((cell % 48) + 0.5);
}
