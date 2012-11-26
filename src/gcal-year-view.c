/* -*- mode: c; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gcal-year-view.c
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

#include "gcal-year-view.h"
#include "gcal-utils.h"
#include "gcal-view.h"
#include "gcal-event-widget.h"

#include <glib/gi18n.h>

#include <libecal/libecal.h>

struct _GcalYearViewPrivate
{
  /**
   * This is where we keep the refs of the child widgets.
   * Every child added to the list placed in the position
   * of it corresponding cell number.
   * The cell number is calculated in _add method.
   * cell_index = date->month - 1
   */
  GList          *months [12];

  GdkWindow      *event_window;

  /* property */
  icaltimetype   *date;

  /* helpers */
  GdkRectangle   *prev_rectangle;
  GdkRectangle   *next_rectangle;
  gboolean        clicked_prev;
  gboolean        clicked_next;
};

enum
{
  PROP_0,
  PROP_DATE  //active-date inherited property
};

static void           gcal_view_interface_init                    (GcalViewIface  *iface);

static void           gcal_year_view_set_property                 (GObject        *object,
                                                                   guint           property_id,
                                                                   const GValue   *value,
                                                                   GParamSpec     *pspec);

static void           gcal_year_view_get_property                 (GObject        *object,
                                                                   guint           property_id,
                                                                   GValue         *value,
                                                                   GParamSpec     *pspec);

static void           gcal_year_view_finalize                     (GObject        *object);

static void           gcal_year_view_realize                      (GtkWidget      *widget);

static void           gcal_year_view_unrealize                    (GtkWidget      *widget);

static void           gcal_year_view_map                          (GtkWidget      *widget);

static void           gcal_year_view_unmap                        (GtkWidget      *widget);

static void           gcal_year_view_size_allocate                (GtkWidget      *widget,
                                                                   GtkAllocation  *allocation);

static gboolean       gcal_year_view_draw                         (GtkWidget      *widget,
                                                                   cairo_t        *cr);

static void           gcal_year_view_add                          (GtkContainer   *constainer,
                                                                   GtkWidget      *widget);

static void           gcal_year_view_remove                       (GtkContainer   *constainer,
                                                                   GtkWidget      *widget);

static void           gcal_year_view_forall                       (GtkContainer   *container,
                                                                   gboolean        include_internals,
                                                                   GtkCallback     callback,
                                                                   gpointer        callback_data);

static void           gcal_year_view_set_date                     (GcalYearView  *view,
                                                                   icaltimetype   *date);

static void           gcal_year_view_draw_header                  (GcalYearView  *view,
                                                                   cairo_t        *cr,
                                                                   GtkAllocation  *alloc,
                                                                   GtkBorder      *padding);

static void           gcal_year_view_draw_grid                    (GcalYearView  *view,
                                                                   cairo_t        *cr,
                                                                   GtkAllocation  *alloc,
                                                                   GtkBorder      *padding);

static gdouble        gcal_year_view_get_start_grid_y             (GtkWidget      *widget);

static icaltimetype*  gcal_year_view_get_initial_date             (GcalView       *view);

static icaltimetype*  gcal_year_view_get_final_date               (GcalView       *view);

static gboolean       gcal_year_view_contains                     (GcalView       *view,
                                                                   icaltimetype   *date);

static void           gcal_year_view_remove_by_uuid               (GcalView       *view,
                                                                   const gchar    *uuid);

static GtkWidget*     gcal_year_view_get_by_uuid                  (GcalView       *view,
                                                                   const gchar    *uuid);

static void           gcal_year_view_reposition_child             (GcalView       *view,
                                                                   const gchar    *uuid);

static void           gcal_year_view_clear_selection              (GcalView       *view);

G_DEFINE_TYPE_WITH_CODE (GcalYearView,
                         gcal_year_view,
                         GTK_TYPE_CONTAINER,
                         G_IMPLEMENT_INTERFACE (GCAL_TYPE_VIEW,
                                                gcal_view_interface_init));


static void
gcal_year_view_class_init (GcalYearViewClass *klass)
{
  GtkContainerClass *container_class;
  GtkWidgetClass *widget_class;
  GObjectClass *object_class;

  container_class = GTK_CONTAINER_CLASS (klass);
  container_class->add   = gcal_year_view_add;
  container_class->remove = gcal_year_view_remove;
  container_class->forall = gcal_year_view_forall;

  widget_class = GTK_WIDGET_CLASS (klass);
  widget_class->realize = gcal_year_view_realize;
  widget_class->unrealize = gcal_year_view_unrealize;
  widget_class->map = gcal_year_view_map;
  widget_class->unmap = gcal_year_view_unmap;
  widget_class->size_allocate = gcal_year_view_size_allocate;
  widget_class->draw = gcal_year_view_draw;

  object_class = G_OBJECT_CLASS (klass);
  object_class->set_property = gcal_year_view_set_property;
  object_class->get_property = gcal_year_view_get_property;
  object_class->finalize = gcal_year_view_finalize;

  g_object_class_override_property (object_class, PROP_DATE, "active-date");

  g_type_class_add_private ((gpointer)klass, sizeof (GcalYearViewPrivate));
}



static void
gcal_year_view_init (GcalYearView *self)
{
  GcalYearViewPrivate *priv;
  gint i;

  gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            GCAL_TYPE_YEAR_VIEW,
                                            GcalYearViewPrivate);
  priv = self->priv;

  priv->prev_rectangle = NULL;
  priv->next_rectangle = NULL;

  priv->clicked_prev = FALSE;
  priv->clicked_next = FALSE;

  for (i = 0; i < 12; i++)
    {
      priv->months[i] = NULL;
    }

  gtk_style_context_add_class (
      gtk_widget_get_style_context (GTK_WIDGET (self)),
      "calendar-view");
}

static void
gcal_view_interface_init (GcalViewIface *iface)
{
  iface->get_initial_date = gcal_year_view_get_initial_date;
  iface->get_final_date = gcal_year_view_get_final_date;

  iface->contains = gcal_year_view_contains;
  iface->remove_by_uuid = gcal_year_view_remove_by_uuid;
  iface->get_by_uuid = gcal_year_view_get_by_uuid;
  iface->reposition_child = gcal_year_view_reposition_child;

  /* iface->clear_selection = gcal_year_view_clear_selection; */
}

static void
gcal_year_view_set_property (GObject       *object,
                             guint          property_id,
                             const GValue  *value,
                             GParamSpec    *pspec)
{
  g_return_if_fail (GCAL_IS_YEAR_VIEW (object));

  switch (property_id)
    {
    case PROP_DATE:
      gcal_year_view_set_date (GCAL_YEAR_VIEW (object),
                               g_value_dup_boxed (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gcal_year_view_get_property (GObject       *object,
                             guint          property_id,
                             GValue        *value,
                             GParamSpec    *pspec)
{
  GcalYearViewPrivate *priv;

  g_return_if_fail (GCAL_IS_YEAR_VIEW (object));
  priv = GCAL_YEAR_VIEW (object)->priv;

  switch (property_id)
    {
    case PROP_DATE:
      g_value_set_boxed (value, priv->date);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gcal_year_view_finalize (GObject       *object)
{
  GcalYearViewPrivate *priv = GCAL_YEAR_VIEW (object)->priv;

  if (priv->date != NULL)
    g_free (priv->date);

  if (priv->prev_rectangle != NULL)
    g_free (priv->prev_rectangle);
  if (priv->next_rectangle != NULL)
    g_free (priv->next_rectangle);

  /* Chain up to parent's finalize() method. */
  G_OBJECT_CLASS (gcal_year_view_parent_class)->finalize (object);
}

static void
gcal_year_view_realize (GtkWidget *widget)
{
  GcalYearViewPrivate *priv;
  GdkWindow *parent_window;
  GdkWindowAttr attributes;
  gint attributes_mask;
  GtkAllocation allocation;

  priv = GCAL_YEAR_VIEW (widget)->priv;
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
gcal_year_view_unrealize (GtkWidget *widget)
{
  GcalYearViewPrivate *priv;

  priv = GCAL_YEAR_VIEW (widget)->priv;
  if (priv->event_window != NULL)
    {
      gdk_window_set_user_data (priv->event_window, NULL);
      gdk_window_destroy (priv->event_window);
      priv->event_window = NULL;
    }

  GTK_WIDGET_CLASS (gcal_year_view_parent_class)->unrealize (widget);
}

static void
gcal_year_view_map (GtkWidget *widget)
{
  GcalYearViewPrivate *priv;

  priv = GCAL_YEAR_VIEW (widget)->priv;
  if (priv->event_window)
    gdk_window_show (priv->event_window);

  GTK_WIDGET_CLASS (gcal_year_view_parent_class)->map (widget);
}

static void
gcal_year_view_unmap (GtkWidget *widget)
{
  GcalYearViewPrivate *priv;

  priv = GCAL_YEAR_VIEW (widget)->priv;
  if (priv->event_window)
    gdk_window_hide (priv->event_window);

  GTK_WIDGET_CLASS (gcal_year_view_parent_class)->unmap (widget);
}

static void
gcal_year_view_size_allocate (GtkWidget     *widget,
                              GtkAllocation *allocation)
{
  GcalYearViewPrivate *priv;
  gint i;
  GList *l;

  GtkBorder padding;
  PangoLayout *layout;

  gint font_height;
  gdouble start_grid_y;

  gdouble added_height;
  gdouble horizontal_block;
  gdouble vertical_block;
  gdouble vertical_cell_margin;

  priv = GCAL_YEAR_VIEW (widget)->priv;

  gtk_widget_set_allocation (widget, allocation);
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
  layout = pango_layout_new (gtk_widget_get_pango_context (widget));

  pango_layout_set_font_description (
      layout,
      gtk_style_context_get_font (gtk_widget_get_style_context (widget),
                                  gtk_widget_get_state_flags (widget)));
  pango_layout_get_pixel_size (layout, NULL, &font_height);
  g_object_unref (layout);

  start_grid_y = gcal_year_view_get_start_grid_y (widget);
  horizontal_block = allocation->width / 6;
  vertical_block = (allocation->height - start_grid_y) / 2;
  vertical_cell_margin = padding.top + font_height;

  for (i = 0; i < 12; i++)
    {
      added_height = 0;
      for (l = priv->months[i]; l != NULL; l = l->next)
        {
          GcalViewChild *child;
          gint pos_x;
          gint pos_y;
          gint min_height;
          gint natural_height;
          GtkAllocation child_allocation;

          child = (GcalViewChild*) l->data;

          pos_x = ( i % 6 ) * horizontal_block;
          pos_y = ( i / 6 ) * vertical_block;

          if ((! gtk_widget_get_visible (child->widget))
              && (! child->hidden_by_me))
            continue;

          gtk_widget_get_preferred_height (child->widget,
                                           &min_height,
                                           &natural_height);
          child_allocation.x = pos_x;
          child_allocation.y = start_grid_y + vertical_cell_margin + pos_y;
          child_allocation.width = horizontal_block;
          child_allocation.height = MIN (natural_height, vertical_block);
          if (added_height + vertical_cell_margin + child_allocation.height
              > vertical_block)
            {
              gtk_widget_hide (child->widget);
              child->hidden_by_me = TRUE;

              l = l->next;
              for (; l != NULL; l = l->next)
                {
                  child = (GcalViewChild*) l->data;

                  gtk_widget_hide (child->widget);
                  child->hidden_by_me = TRUE;
                }

              break;
            }
          else
            {
              gtk_widget_show (child->widget);
              child->hidden_by_me = FALSE;
              child_allocation.y = child_allocation.y + added_height;
              gtk_widget_size_allocate (child->widget, &child_allocation);
              added_height += child_allocation.height;
            }
        }
    }
}

static gboolean
gcal_year_view_draw (GtkWidget *widget,
                     cairo_t   *cr)
{
  GtkStyleContext *context;
  GtkStateFlags state;
  GtkBorder padding;
  GtkAllocation alloc;

  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);

  gtk_style_context_get_padding (context, state, &padding);
  gtk_widget_get_allocation (widget, &alloc);

  gcal_year_view_draw_header (GCAL_YEAR_VIEW (widget), cr, &alloc, &padding);
  gcal_year_view_draw_grid (GCAL_YEAR_VIEW (widget), cr, &alloc, &padding);

  if (GTK_WIDGET_CLASS (gcal_year_view_parent_class)->draw != NULL)
    GTK_WIDGET_CLASS (gcal_year_view_parent_class)->draw (widget, cr);

  return FALSE;
}

static void
gcal_year_view_add (GtkContainer *container,
                    GtkWidget    *widget)
{
  GcalYearViewPrivate *priv;
  GList *l;
  icaltimetype *date;

  GcalViewChild *new_child;

  g_return_if_fail (GCAL_IS_YEAR_VIEW (container));
  g_return_if_fail (GCAL_IS_EVENT_WIDGET (widget));
  g_return_if_fail (gtk_widget_get_parent (widget) == NULL);
  priv = GCAL_YEAR_VIEW (container)->priv;

  /* Check if it's already added for date */
  date = gcal_event_widget_get_date (GCAL_EVENT_WIDGET (widget));

  for (l = priv->months[date->month - 1]; l != NULL; l = l->next)
    {
      GcalViewChild *child;

      child = (GcalViewChild*) l->data;
      if (g_strcmp0 (
            gcal_event_widget_peek_uuid (GCAL_EVENT_WIDGET (widget)),
            gcal_event_widget_peek_uuid (GCAL_EVENT_WIDGET (child->widget)))
          == 0)
        {
          //TODO: remove once the main-dev phase its over
          g_warning ("Trying to add an event with the same uuid to the view");
          return;
        }
    }

  new_child = g_new0 (GcalViewChild, 1);
  new_child->widget = widget;
  new_child->hidden_by_me = FALSE;

  priv->months[date->month - 1] =
    g_list_insert_sorted (priv->months[date->month - 1],
                          new_child,
                          gcal_compare_event_widget_by_date);

  gtk_widget_set_parent (widget, GTK_WIDGET (container));

  g_free (date);
}

static void
gcal_year_view_remove (GtkContainer *container,
                       GtkWidget    *widget)
{
  GcalYearViewPrivate *priv;
  gint i;
  GList *l;

  g_return_if_fail (GCAL_IS_YEAR_VIEW (container));
  g_return_if_fail (gtk_widget_get_parent (widget) == GTK_WIDGET (container));
  priv = GCAL_YEAR_VIEW (container)->priv;

  for (i = 0; i < 12; i++)
    {
      for (l = priv->months[i]; l != NULL; l = l->next)
        {
          GcalViewChild *child;

          child = (GcalViewChild*) l->data;
          if (child->widget == widget)
            {
              gboolean was_visible;

              was_visible = gtk_widget_get_visible (widget);
              gtk_widget_unparent (widget);

              priv->months[i] = g_list_remove (priv->months[i], child);
              g_free (child);

              if (was_visible)
                gtk_widget_queue_resize (GTK_WIDGET (container));

              return;
            }
        }
    }
}

static void
gcal_year_view_forall (GtkContainer *container,
                       gboolean      include_internals,
                       GtkCallback   callback,
                       gpointer      callback_data)
{
  GcalYearViewPrivate *priv;
  gint i;
  GList *l;

  priv = GCAL_YEAR_VIEW (container)->priv;

  for (i = 0; i < 12; i++)
    {
      l = priv->months[i];

      while (l)
        {
          GcalViewChild *child;

          child = (GcalViewChild*) l->data;
          l  = l->next;

          (* callback) (child->widget, callback_data);
        }
    }
}

static void
gcal_year_view_set_date (GcalYearView *view,
                         icaltimetype  *date)
{
  GcalYearViewPrivate *priv;
  gboolean will_resize;

  gint i;
  GList *l;
  GList *to_remove;

  priv = view->priv;
  will_resize = FALSE;

  /* if span_updated: queue_resize */
  will_resize = ! gcal_view_contains (GCAL_VIEW (view), date);

  if (priv->date != NULL)
    g_free (priv->date);

  priv->date = date;

  if (will_resize)
    {
      to_remove = NULL;

      for (i = 0; i < 12; i++)
        {
          for (l = priv->months[i]; l != NULL; l = l->next)
            {
              GcalViewChild *child;
              icaltimetype *child_date;

              child = (GcalViewChild*) l->data;
              child_date =
                gcal_event_widget_get_date (GCAL_EVENT_WIDGET (child->widget));
              if (! gcal_view_contains (GCAL_VIEW (view), child_date))
                to_remove = g_list_append (to_remove, child->widget);
            }
        }
      g_list_foreach (to_remove, (GFunc) gtk_widget_destroy, NULL);

      gtk_widget_queue_resize (GTK_WIDGET (view));
      gtk_widget_queue_draw (GTK_WIDGET (view));
    }
}

static void
gcal_year_view_draw_header (GcalYearView  *view,
                            cairo_t       *cr,
                            GtkAllocation *alloc,
                            GtkBorder     *padding)
{
  GcalYearViewPrivate *priv;
  GtkStyleContext *context;
  GtkStateFlags state;
  GdkRGBA color;
  GtkBorder header_padding;

  PangoLayout *layout;
  gint layout_width;
  gint header_width;
  gint layout_height;

  gchar *left_header;

  GtkIconTheme *icon_theme;
  GdkPixbuf *pixbuf;

  g_return_if_fail (GCAL_IS_YEAR_VIEW (view));
  priv = view->priv;

  context = gtk_widget_get_style_context (GTK_WIDGET (view));
  state = gtk_widget_get_state_flags (GTK_WIDGET (view));

  gtk_style_context_save (context);
  gtk_style_context_add_region (context, "header", 0);

  gtk_style_context_get_padding (context, state, &header_padding);

  gtk_style_context_get_color (context, state, &color);
  cairo_set_source_rgb (cr, color.red, color.green, color.blue);

  layout = pango_cairo_create_layout (cr);
  pango_layout_set_font_description (layout,
                                     gtk_style_context_get_font (context,
                                                                 state));
  gtk_style_context_restore (context);

  /* Here translators should put the widgest letter in their alphabet, this
   * taken to make it align with week-view header, which is the larger for now */
  pango_layout_set_text (layout, _("WWW 99 - WWW 99"), -1);
  pango_cairo_update_layout (cr, layout);
  pango_layout_get_pixel_size (layout, &layout_width, &layout_height);

  left_header = g_strdup_printf (_("Year %d"), priv->date->year);

  pango_layout_set_text (layout, left_header, -1);
  pango_cairo_update_layout (cr, layout);
  pango_layout_get_pixel_size (layout, &header_width, NULL);

  cairo_move_to (cr,
                 alloc->x + header_padding.left + ((layout_width - header_width) / 2),
                 alloc->y + header_padding.top);
  pango_cairo_show_layout (cr, layout);

  /* Drawing arrows */
  icon_theme = gtk_icon_theme_get_default ();
  pixbuf = gtk_icon_theme_load_icon (icon_theme,
                                     "go-previous-symbolic",
                                     layout_height,
                                     0,
                                     NULL);

  gdk_cairo_set_source_pixbuf (cr,
                               pixbuf,
                               alloc->x + layout_width + 2 * header_padding.left,
                               alloc->y + header_padding.top);
  g_object_unref (pixbuf);
  cairo_paint (cr);

  pixbuf = gtk_icon_theme_load_icon (icon_theme,
                                     "go-next-symbolic",
                                     layout_height,
                                     0,
                                     NULL);

  gdk_cairo_set_source_pixbuf (cr,
                               pixbuf,
                               alloc->x + layout_width + 2 * header_padding.left + layout_height,
                               alloc->y + header_padding.top);
  g_object_unref (pixbuf);
  cairo_paint (cr);

  /* allocating rects */
  if (priv->prev_rectangle == NULL)
    priv->prev_rectangle = g_new0 (GdkRectangle, 1);
  priv->prev_rectangle->x = alloc->x + layout_width + 2 * header_padding.left;
  priv->prev_rectangle->y = alloc->y + header_padding.top;
  priv->prev_rectangle->width = layout_height;
  priv->prev_rectangle->height = layout_height;

  if (priv->next_rectangle == NULL)
    priv->next_rectangle = g_new0 (GdkRectangle, 1);
  priv->next_rectangle->x = alloc->x + layout_width + 2 * header_padding.left + layout_height;
  priv->next_rectangle->y = alloc->y + header_padding.top;
  priv->next_rectangle->width = layout_height;
  priv->next_rectangle->height = layout_height;

  g_free (left_header);
  g_object_unref (layout);
}

static void
gcal_year_view_draw_grid (GcalYearView *view,
                          cairo_t       *cr,
                          GtkAllocation *alloc,
                          GtkBorder     *padding)
{
  GcalYearViewPrivate *priv;
  GtkWidget *widget;

  GtkStyleContext *context;
  GtkStateFlags state;
  GdkRGBA color;
  GdkRGBA ligther_color;
  GdkRGBA selected_color;
  GdkRGBA background_selected_color;
  GtkBorder header_padding;

  gint i, j;
  gint font_width;
  gint font_height;

  gdouble start_grid_y;

  PangoLayout *layout;
  const PangoFontDescription *font;

  priv = view->priv;
  widget = GTK_WIDGET (view);

  context = gtk_widget_get_style_context (widget);
  layout = pango_cairo_create_layout (cr);

  start_grid_y = gcal_year_view_get_start_grid_y (widget);
  state = gtk_widget_get_state_flags (widget);
  gtk_style_context_get_color (context,
                               state | GTK_STATE_FLAG_SELECTED,
                               &selected_color);

  gtk_style_context_get_background_color (context,
                                          state | GTK_STATE_FLAG_SELECTED,
                                          &background_selected_color);

  gtk_style_context_get_color (context,
                               state | GTK_STATE_FLAG_INSENSITIVE,
                               &ligther_color);
  gtk_style_context_get_color (context, state, &color);
  font = gtk_style_context_get_font (context, state);
  cairo_set_source_rgb (cr, color.red, color.green, color.blue);

  pango_layout_set_font_description (layout, font);

  gtk_style_context_save (context);
  gtk_style_context_add_region (context, "header", 0);

  gtk_style_context_get_padding (context, state, &header_padding);

  gtk_style_context_restore (context);

  cairo_set_source_rgb (cr,
                        ligther_color.red,
                        ligther_color.green,
                        ligther_color.blue);

  /* drawing grid text */
  for (i = 0; i < 2; i++)
    {
      for (j = 0; j < 6; j++)
        {
          pango_layout_set_text (layout, gcal_get_month_name (i * 6 + j), -1);
          pango_cairo_update_layout (cr, layout);
          pango_layout_get_pixel_size (layout, &font_width, &font_height);

          cairo_move_to (cr,
                         (alloc->width / 6) * j + header_padding.left,
                         start_grid_y + padding->top + i * (alloc->height - start_grid_y) / 2);
          pango_cairo_show_layout (cr, layout);
        }
    }
  /* free the layout object */
  g_object_unref (layout);

  /* drawing grid skel */
  cairo_set_line_width (cr, 0.3);

  /* vertical lines */
  for (i = 0; i < 5; i++)
    {
      //FIXME: ensure x coordinate has an integer value plus 0.4
      cairo_move_to (cr, (alloc->width / 6) * (i + 1) + 0.4, start_grid_y);
      cairo_rel_line_to (cr, 0, alloc->height - start_grid_y);
    }

  for (i = 0; i < 2; i++)
    {
      //FIXME: ensure y coordinate has an integer value plus 0.4
      cairo_move_to (cr, 0, start_grid_y + ((alloc->height - start_grid_y) / 2) * i + 0.4);
      cairo_rel_line_to (cr, alloc->width, 0);
    }

  cairo_stroke (cr);

  /* drawing actual_day_cell */
  cairo_set_source_rgb (cr,
                        selected_color.red,
                        selected_color.green,
                        selected_color.blue);

  /* Two pixel line on the selected day cell */
  cairo_set_line_width (cr, 2.0);
  cairo_move_to (cr,
                 (alloc->width / 6) * ( (priv->date->month - 1) % 6),
                 start_grid_y + ((alloc->height - start_grid_y) / 2) * ( (priv->date->month - 1) / 6) + 1);
  cairo_rel_line_to (cr, (alloc->width / 6), 0);
  cairo_stroke (cr);
}

static gdouble
gcal_year_view_get_start_grid_y (GtkWidget *widget)
{
  GtkStyleContext *context;
  GtkBorder header_padding;

  PangoLayout *layout;
  gint font_height;
  gdouble start_grid_y;

  context = gtk_widget_get_style_context (widget);
  layout = pango_layout_new (gtk_widget_get_pango_context (widget));

  /* init header values */
  gtk_style_context_save (context);
  gtk_style_context_add_region (context, "header", 0);

  gtk_style_context_get_padding (gtk_widget_get_style_context (widget),
                                 gtk_widget_get_state_flags (widget),
                                 &header_padding);

  pango_layout_set_font_description (
      layout,
      gtk_style_context_get_font (context,
                                  gtk_widget_get_state_flags(widget)));
  pango_layout_get_pixel_size (layout, NULL, &font_height);

  /* 6: is padding around the header */
  start_grid_y = header_padding.top + font_height + header_padding.bottom;
  gtk_style_context_restore (context);

  g_object_unref (layout);
  return start_grid_y;
}

/* GcalView Interface API */
/**
 * gcal_year_view_get_initial_date:
 *
 * Since: 0.1
 * Return value: the first day of the month
 * Returns: (transfer full): Release with g_free
 **/
static icaltimetype*
gcal_year_view_get_initial_date (GcalView *view)
{
  //FIXME to retrieve the 35 days range
  GcalYearViewPrivate *priv;
  icaltimetype *new_date;

  g_return_val_if_fail (GCAL_IS_YEAR_VIEW (view), NULL);
  priv = GCAL_YEAR_VIEW (view)->priv;
  new_date = gcal_dup_icaltime (priv->date);
  new_date->day = 1;
  new_date->month = 1;

  return new_date;
}

/**
 * gcal_year_view_get_final_date:
 *
 * Since: 0.1
 * Return value: the last day of the month
 * Returns: (transfer full): Release with g_free
 **/
static icaltimetype*
gcal_year_view_get_final_date (GcalView *view)
{
  //FIXME to retrieve the 35 days range
  GcalYearViewPrivate *priv;
  icaltimetype *new_date;

  g_return_val_if_fail (GCAL_IS_YEAR_VIEW (view), NULL);
  priv = GCAL_YEAR_VIEW (view)->priv;
  new_date = gcal_dup_icaltime (priv->date);
  new_date->day = 31;
  new_date->month =  12;

  return new_date;
}

static gboolean
gcal_year_view_contains (GcalView     *view,
                         icaltimetype *date)
{
  GcalYearViewPrivate *priv;

  g_return_val_if_fail (GCAL_IS_YEAR_VIEW (view), FALSE);
  priv = GCAL_YEAR_VIEW (view)->priv;

  if (priv->date == NULL)
    return FALSE;

  return priv->date->year == date->year;
}

static void
gcal_year_view_remove_by_uuid (GcalView    *view,
                               const gchar *uuid)
{
  GcalYearViewPrivate *priv;
  gint i;
  GList *l;

  g_return_if_fail (GCAL_IS_YEAR_VIEW (view));
  priv = GCAL_YEAR_VIEW (view)->priv;

  for (i = 0; i < 12; i++)
    {
      for (l = priv->months[i]; l != NULL; l = l->next)
        {
          GcalViewChild *child;
          const gchar* widget_uuid;

          child = (GcalViewChild*) l->data;
          widget_uuid = gcal_event_widget_peek_uuid (GCAL_EVENT_WIDGET (child->widget));
          if (g_strcmp0 (uuid, widget_uuid) == 0)
            {
              gtk_widget_destroy (child->widget);
              return;
            }
        }
    }
}

static GtkWidget*
gcal_year_view_get_by_uuid (GcalView    *view,
                            const gchar *uuid)
{
  GcalYearViewPrivate *priv;
  gint i;
  GList *l;

  g_return_val_if_fail (GCAL_IS_YEAR_VIEW (view), NULL);
  priv = GCAL_YEAR_VIEW (view)->priv;

  for (i = 0; i < 12; i++)
    {
      for (l = priv->months[i]; l != NULL; l = l->next)
        {
          GcalViewChild *child;
          const gchar* widget_uuid;

          child = (GcalViewChild*) l->data;
          widget_uuid = gcal_event_widget_peek_uuid (GCAL_EVENT_WIDGET (child->widget));
          if (g_strcmp0 (uuid, widget_uuid) == 0)
            return child->widget;
        }
    }
  return NULL;
}

static void
gcal_year_view_reposition_child (GcalView    *view,
                                 const gchar *uuid)
{
  GcalYearViewPrivate *priv;
  gint i;
  GList *l;

  g_return_if_fail (GCAL_IS_YEAR_VIEW (view));
  priv = GCAL_YEAR_VIEW (view)->priv;

  for (i = 0; i < 12; i++)
    {
      for (l = priv->months[i]; l != NULL; l = l->next)
        {
          GcalViewChild *child;
          const gchar* widget_uuid;

          child = (GcalViewChild*) l->data;
          widget_uuid = gcal_event_widget_peek_uuid (GCAL_EVENT_WIDGET (child->widget));
          if (g_strcmp0 (uuid, widget_uuid) == 0)
            {
              icaltimetype *date;

              date =
                gcal_event_widget_get_date (GCAL_EVENT_WIDGET (child->widget));

              if (gcal_year_view_contains (view, date))
                {
                  if (date->month - 1 == i)
                    {
                      priv->months[i] =
                        g_list_sort (priv->months[i],
                                     gcal_compare_event_widget_by_date);
                    }
                  else
                    {
                      priv->months[i] = g_list_remove (priv->months[i], child);

                      child->hidden_by_me = TRUE;
                      priv->months[date->month - 1] =
                        g_list_insert_sorted (priv->months[date->month - 1],
                                              child,
                                              gcal_compare_event_widget_by_date);
                    }

                  gtk_widget_queue_resize (GTK_WIDGET (view));
                }
              else
                {
                  gcal_year_view_remove_by_uuid (view, uuid);
                }

              g_free (date);
              return;
            }
        }
    }
}

/* Public API */
/**
 * gcal_year_view_new:
 *
 * Since: 0.1
 * Return value: the new month view widget
 * Returns: (transfer full):
 **/
GtkWidget*
gcal_year_view_new (void)
{
  return g_object_new (GCAL_TYPE_YEAR_VIEW, NULL);
}
