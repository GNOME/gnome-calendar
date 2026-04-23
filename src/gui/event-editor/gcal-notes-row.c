/* gcal-notes-row.c
 *
 * Copyright 2020 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#define G_LOG_DOMAIN "GcalNotesRow"

#include "gcal-debug.h"
#include "gcal-notes-row.h"
#include "gcal-gizmo.h"

#define ANIMATION_DURATION 150
#define ROW_MIN_HEIGHT 50
#define FADE_RADIUS 18.0f
#define TITLE_SPACING 3


/**
 * GcalNotesRow:
 *
 * A [class@Gtk.ListBoxRow] with an embedded text view.
 *
 * This is very similar to [class@Adw.EntryRow], but it comes with the added functionality of
 * allowing to use multiple lines.
 */

struct _GcalNotesRow
{
  AdwPreferencesRow  parent;

  GtkTextView       *text_view;
  GtkLabel          *reference_title;
  GtkLabel          *visible_title;
  GtkImage          *edit_icon;
  GcalGizmo         *titles;

  AdwTimedAnimation *animation;

  gboolean           has_text;
  gboolean           is_focused;
  gboolean           editable;
};

G_DEFINE_FINAL_TYPE (GcalNotesRow, gcal_notes_row, ADW_TYPE_PREFERENCES_ROW)

enum {
  PROP_0,
  PROP_EDITABLE,
  PROP_TEXT,
  N_PROPS,
};

static GParamSpec *properties[N_PROPS];

static const GdkRGBA OPAQUE = { 1, 1, 1, 1.0 };
static const GdkRGBA TRANSLUCENT = { 0, 0, 0, 0.5 };

static const GskColorStop color_stops[2] = {
  {
    .offset = FADE_RADIUS / ROW_MIN_HEIGHT,
    .color = TRANSLUCENT,
  },
  {
    .offset = 1.0,
    .color = OPAQUE,
  }
};


/*
 * Auxiliary methods
 */

static void
update_contains_text (GcalNotesRow  *self,
                      GtkTextBuffer *buffer)
{
  g_autofree PangoLayout *layout = NULL;
  g_autofree gchar *text = NULL;
  int text_width, text_height;
  GtkTextIter start, end;

  g_assert (GCAL_IS_NOTES_ROW (self));
  g_assert (GTK_IS_TEXT_BUFFER (buffer));

  buffer = gtk_text_view_get_buffer (self->text_view);
  gtk_text_buffer_get_bounds (buffer, &start, &end);

  text = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
  layout = gtk_widget_create_pango_layout (GTK_WIDGET (self->text_view), text);
  pango_layout_get_pixel_size (layout, &text_width, &text_height);

  self->has_text = text_width > 0;
}

static void
update_widgets (GcalNotesRow  *self,
                GtkTextBuffer *buffer)
{
  double value;

  update_contains_text (self, buffer);

  gtk_text_view_set_cursor_visible (self->text_view, self->editable || self->has_text);

  if (self->has_text != adw_timed_animation_get_value_to (self->animation))
    {
      value = adw_animation_get_value (ADW_ANIMATION (self->animation));
      adw_timed_animation_set_value_to (self->animation, self->has_text);
      adw_timed_animation_set_value_from (self->animation, value);
      adw_animation_play (ADW_ANIMATION (self->animation));
    }
}


/*
 * Callbacks
 */

static void
empty_animation_value_cb (double        value,
                          GcalNotesRow *self)
{
  gtk_widget_queue_allocate (GTK_WIDGET (self->titles));
}

static void
on_row_pressed (GtkGestureClick *gesture,
                int              n_press,
                double           x,
                double           y,
                GtkWidget       *text_view)
{
  g_assert (GTK_IS_TEXT_VIEW (text_view));

  gtk_widget_grab_focus (text_view);

  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_NONE);
}

static void
on_text_changed_cb (GcalNotesRow     *self,
                    GParamSpecString *string,
                    GtkTextBuffer    *buffer)
{
  update_contains_text (self, buffer);

  /* HACK: Force Gtk.WidgetClass.size_allocate() and Gtk.Widget.queue_allocate()
   *       to avoid flickering when height changes */
  GTK_WIDGET_GET_CLASS (self->text_view)->size_allocate (GTK_WIDGET (self->text_view),
                                                         gtk_widget_get_width (GTK_WIDGET (self->text_view)),
                                                         gtk_widget_get_height (GTK_WIDGET (self->text_view)),
                                                         -1);
  gtk_widget_queue_allocate (GTK_WIDGET (self->text_view));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TEXT]);
}


/*
 * GcalGizmo overrides
 */

static void
gcal_notes_row_child_snapshot (GcalGizmo   *gizmo,
                               GtkSnapshot *snapshot,
                               gpointer     user_data)
{
  GcalNotesRow *self = GCAL_NOTES_ROW (user_data);
  double value = adw_animation_get_value (ADW_ANIMATION (self->animation));

  gtk_widget_snapshot_child (GTK_WIDGET (gizmo), GTK_WIDGET (self->visible_title), snapshot);
  gtk_widget_snapshot_child (GTK_WIDGET (gizmo), GTK_WIDGET (self->edit_icon), snapshot);

  if (gtk_widget_get_child_visible (GTK_WIDGET (self->edit_icon)))
    {
      int widget_height = gtk_widget_get_height (GTK_WIDGET (self));
      int widget_width = gtk_widget_get_width (GTK_WIDGET (self));
      int edit_icon_width = gtk_widget_get_width (GTK_WIDGET (self->edit_icon));
      int edit_icon_height = gtk_widget_get_height (GTK_WIDGET (self->edit_icon));

      graphene_point_t point;

      if (!gtk_widget_compute_point (GTK_WIDGET (self->edit_icon), GTK_WIDGET (gizmo),
                                     &GRAPHENE_POINT_INIT (edit_icon_width / 2.0, edit_icon_height / 2.0),
                                     &point))
        g_assert_not_reached ();

      gtk_snapshot_push_mask (snapshot, GSK_MASK_MODE_ALPHA);
      gtk_snapshot_append_radial_gradient (snapshot,
                                           &GRAPHENE_RECT_INIT (0, 0, widget_width, widget_height),
                                           &point,
                                           edit_icon_width + FADE_RADIUS,
                                           edit_icon_height + FADE_RADIUS,
                                           0.0, 1.0, color_stops, G_N_ELEMENTS (color_stops));
      gtk_snapshot_pop (snapshot);
    }

  gtk_snapshot_push_opacity (snapshot, pow (value, 2));
  gtk_widget_snapshot_child (GTK_WIDGET (gizmo), GTK_WIDGET (self->text_view), snapshot);
  gtk_snapshot_pop (snapshot);

  if (gtk_widget_get_child_visible (GTK_WIDGET (self->edit_icon)))
    gtk_snapshot_pop (snapshot);
}

static void
gcal_notes_row_child_size_allocate (GcalGizmo *gizmo,
                                    int        width,
                                    int        height,
                                    int        baseline,
                                    gpointer   user_data)
{
  GcalNotesRow *self = GCAL_NOTES_ROW (user_data);
  double value = adw_animation_get_value (ADW_ANIMATION (self->animation));
  g_autofree PangoLayout *layout = NULL;
  GskTransform *transform = NULL;
  double editable_min_height, non_editable_min_height;
  double horizontal_offset, vertical_offset;
  int reference_height, reference_width;
  int visible_height, visible_width;
  int edit_icon_height;
  double factor, start;
  int text_height;

  gtk_widget_allocate (GTK_WIDGET (self->text_view), width, height, baseline, NULL);

  gtk_widget_measure (GTK_WIDGET (self->reference_title), GTK_ORIENTATION_HORIZONTAL, -1,
                      NULL, &reference_width, NULL, NULL);
  gtk_widget_measure (GTK_WIDGET (self->reference_title), GTK_ORIENTATION_VERTICAL, -1,
                      NULL, &reference_height, NULL, NULL);
  gtk_widget_measure (GTK_WIDGET (self->visible_title), GTK_ORIENTATION_HORIZONTAL, -1,
                      NULL, &visible_width, NULL, NULL);
  gtk_widget_measure (GTK_WIDGET (self->visible_title), GTK_ORIENTATION_VERTICAL, -1,
                      NULL, &visible_height, NULL, NULL);
  gtk_widget_measure (GTK_WIDGET (self->edit_icon), GTK_ORIENTATION_VERTICAL, -1,
                      NULL, &edit_icon_height, NULL, NULL);

  transform = gsk_transform_translate (NULL, &GRAPHENE_POINT_INIT (0, (ROW_MIN_HEIGHT - height) / 2.0));
  gtk_widget_allocate (GTK_WIDGET (self->edit_icon), width, height, baseline, transform);

  layout = gtk_widget_create_pango_layout (GTK_WIDGET (self->text_view), NULL);
  pango_layout_get_pixel_size (layout, NULL, &text_height);

  editable_min_height = (double) (ROW_MIN_HEIGHT - visible_height);
  gtk_text_view_set_top_margin (self->text_view, editable_min_height / 2 + pow (TITLE_SPACING, 2));
  gtk_text_view_set_bottom_margin (self->text_view, editable_min_height / 2 - pow (TITLE_SPACING, 2));

  start = (double) reference_height / visible_height;
  factor = adw_lerp (1.0, start, value);

  if (gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL)
    horizontal_offset = width * (1 - factor);
  else
    horizontal_offset = 0.0;

  non_editable_min_height = (double) ROW_MIN_HEIGHT - reference_height - text_height - TITLE_SPACING;
  vertical_offset = adw_lerp (editable_min_height / 2, non_editable_min_height / 2, value);

  transform = gsk_transform_translate (NULL, &GRAPHENE_POINT_INIT (horizontal_offset, vertical_offset));
  transform = gsk_transform_scale (transform, factor, factor);

  gtk_widget_allocate (GTK_WIDGET (self->visible_title), width, height, baseline, transform);
}

static void
gcal_notes_row_child_measure (GcalGizmo      *gizmo,
                              GtkOrientation  orientation,
                              int             for_size,
                              int            *minimum,
                              int            *natural,
                              int            *minimum_baseline,
                              int            *natural_baseline,
                              gpointer        user_data)
{
  GtkWidget *child;

  for (child = gtk_widget_get_first_child (GTK_WIDGET (gizmo));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      gtk_widget_measure (child, orientation, for_size, minimum, natural, NULL, NULL);
    }
}


/*
 * GtkWidget overrides
 */

static gboolean
gcal_notes_row_focus (GtkWidget        *widget,
                      GtkDirectionType  direction)
{
  GcalNotesRow *self = GCAL_NOTES_ROW (widget);

  return gtk_widget_child_focus (GTK_WIDGET (self->text_view), direction);
}

static gboolean
gcal_notes_row_grab_focus (GtkWidget *widget)
{
  GcalNotesRow *self = GCAL_NOTES_ROW (widget);

  return gtk_widget_grab_focus (GTK_WIDGET (self->text_view));
}

static void
gcal_notes_row_state_flags_changed (GtkWidget     *widget,
                                    GtkStateFlags  previous_flags)
{
  GcalNotesRow *self = GCAL_NOTES_ROW (widget);
  gboolean is_focused, was_focused;
  GtkStateFlags flags, focus_flags;
  double value;

  flags = gtk_widget_get_state_flags (GTK_WIDGET (self));

  gtk_widget_set_state_flags (GTK_WIDGET (self), flags, TRUE);

  focus_flags = GTK_STATE_FLAG_FOCUS_WITHIN | GTK_STATE_FLAG_FOCUSED | GTK_STATE_FLAG_FOCUS_VISIBLE;
  is_focused = !!(flags & focus_flags);
  was_focused = !!(previous_flags & focus_flags);

  gtk_widget_set_child_visible (GTK_WIDGET (self->edit_icon), !is_focused || !self->editable);

  if (is_focused)
    gtk_widget_add_css_class (GTK_WIDGET (self), "focused");
  else
    gtk_widget_remove_css_class (GTK_WIDGET (self), "focused");

  if (self->editable && !self->has_text && is_focused != was_focused)
    {
      value = adw_animation_get_value (ADW_ANIMATION (self->animation));
      adw_timed_animation_set_value_to (self->animation, is_focused);
      adw_timed_animation_set_value_from (self->animation, value);
      adw_animation_play (ADW_ANIMATION (self->animation));
    }
}


/*
 * GObject overrides
 */

static void
gcal_notes_row_finalize (GObject *object)
{
  GcalNotesRow *self = GCAL_NOTES_ROW (object);

  g_clear_object (&self->animation);

  G_OBJECT_CLASS (gcal_notes_row_parent_class)->finalize (object);
}

static void
gcal_notes_row_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GcalNotesRow *self = GCAL_NOTES_ROW (object);

  switch (prop_id)
    {
    case PROP_EDITABLE:
      g_value_set_boolean (value, gcal_notes_row_get_editable (self));
      break;

    case PROP_TEXT:
      g_value_set_string (value, gcal_notes_row_get_text (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_notes_row_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GcalNotesRow *self = GCAL_NOTES_ROW (object);

  switch (prop_id)
    {
    case PROP_EDITABLE:
      gcal_notes_row_set_editable (self, g_value_get_boolean (value));
      break;

    case PROP_TEXT:
      gcal_notes_row_set_text (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_notes_row_class_init (GcalNotesRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gcal_notes_row_finalize;
  object_class->get_property = gcal_notes_row_get_property;
  object_class->set_property = gcal_notes_row_set_property;

  widget_class->focus = gcal_notes_row_focus;
  widget_class->grab_focus = gcal_notes_row_grab_focus;
  widget_class->state_flags_changed = gcal_notes_row_state_flags_changed;

  /**
   * GcalNotesRow:editable:
   *
   * Whether the text can be modified by the user.
   */
  properties[PROP_EDITABLE] =
      g_param_spec_boolean ("editable", NULL, NULL,
                            FALSE,
                            G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GcalNotesRow:text:
   *
   * The text content of the #GcalNotesRow.
   */
  properties[PROP_TEXT] =
      g_param_spec_string ("text", NULL, NULL,
                           NULL,
                           G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/event-editor/gcal-notes-row.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalNotesRow, text_view);
  gtk_widget_class_bind_template_child (widget_class, GcalNotesRow, titles);
  gtk_widget_class_bind_template_child (widget_class, GcalNotesRow, reference_title);
  gtk_widget_class_bind_template_child (widget_class, GcalNotesRow, visible_title);
  gtk_widget_class_bind_template_child (widget_class, GcalNotesRow, edit_icon);

  gtk_widget_class_bind_template_callback (widget_class, on_row_pressed);
}

static void
gcal_notes_row_init (GcalNotesRow *self)
{
  AdwAnimationTarget *target;
  GtkTextBuffer *buffer;

  gtk_widget_init_template (GTK_WIDGET (self));

  buffer = gtk_text_view_get_buffer (self->text_view);
  g_signal_connect_swapped (buffer, "notify::text", G_CALLBACK (on_text_changed_cb), self);

  target =
      adw_callback_animation_target_new ((AdwAnimationTargetFunc) empty_animation_value_cb,
                                         self, NULL);

  self->animation =
      ADW_TIMED_ANIMATION (adw_timed_animation_new (GTK_WIDGET (self), 0, 0, ANIMATION_DURATION, target));

  adw_timed_animation_set_easing (self->animation, ADW_EASE);

  gcal_gizmo_set_size_allocate_func (self->titles, gcal_notes_row_child_size_allocate, self);
  gcal_gizmo_set_measure_func (self->titles, gcal_notes_row_child_measure, NULL);
  gcal_gizmo_set_snapshot_func (self->titles, gcal_notes_row_child_snapshot, self);
}

/**
 * gcal_notes_row_new:
 *
 * Creates a new notes row.
 *
 * Returns: a newly created #GcalNotesRow
 */
GcalNotesRow *
gcal_notes_row_new (void)
{
  return g_object_new (GCAL_TYPE_NOTES_ROW, NULL);
}

/**
 * gcal_notes_row_get_text:
 * @self: a #GcalNotesRow
 *
 * Returns the current text.
 *
 * Returns: (transfer full): the current text
 */
char *
gcal_notes_row_get_text (GcalNotesRow *self)
{
  GtkTextBuffer *buffer;
  GtkTextIter start, end;

  g_assert (GCAL_IS_NOTES_ROW (self));

  buffer = gtk_text_view_get_buffer (self->text_view);
  gtk_text_buffer_get_bounds (buffer, &start, &end);

  return gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
}

/**
 * gcal_notes_row_set_text:
 * @self: a #GcalNotesRow
 * @text: (transfer none) (nullable): UTF-8 text to insert
 *
 * Deletes the content of @self and inserts @text.
 */
void
gcal_notes_row_set_text (GcalNotesRow *self,
                         const char   *text)
{
  g_autofree char *current_text = NULL;
  GtkTextBuffer *buffer;

  g_assert (GCAL_IS_NOTES_ROW (self));

  current_text = gcal_notes_row_get_text (self);
  if (g_strcmp0 (current_text, text) == 0)
    return;

  buffer = gtk_text_view_get_buffer (self->text_view);

  gtk_text_buffer_set_text (buffer, text, -1);
  update_widgets (self, buffer);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TEXT]);
}

/**
 * gcal_notes_row_get_editable:
 * @self: a #GcalNotesRow
 *
 * Returns the editability of @self.
 *
 * Returns: whether the text is editable
 */
gboolean
gcal_notes_row_get_editable (GcalNotesRow *self)
{
  g_assert (GCAL_IS_NOTES_ROW (self));

  return self->editable;
}

/**
 * gcal_notes_row_set_editable:
 * @self: a #GcalNotesRow
 * @editable: whether it should be editable
 *
 * Sets the editability of @self.
 */
void
gcal_notes_row_set_editable (GcalNotesRow *self,
                             gboolean      editable)
{
  g_assert (GCAL_IS_NOTES_ROW (self));

  if (self->editable == editable)
    return;

  self->editable = editable;
  gtk_text_view_set_cursor_visible (self->text_view, editable || self->has_text);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_EDITABLE]);
}

