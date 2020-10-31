/* gcal-ediat-dialog.c
 *
 * Copyright (C) 2015 Erick Pérez Castellanos <erickpc@gnome.org>
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
#include "config.h"

#define G_LOG_DOMAIN "GcalEventEditorDialog"

#include "gcal-alarm-row.h"
#include "gcal-context.h"
#include "gcal-debug.h"
#include "gcal-event-editor-dialog.h"
#include "gcal-event-editor-section.h"
#include "gcal-utils.h"
#include "gcal-event.h"
#include "gcal-notes-section.h"
#include "gcal-reminders-section.h"
#include "gcal-schedule-section.h"
#include "gcal-summary-section.h"

#include <dazzle.h>
#include <libecal/libecal.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

/**
 * SECTION:gcal-event-editor-dialog
 * @short_description: Event editor dialog
 * @title:GcalEventEditorDialog
 * @image:gcal-event-editor-dialog.png
 *
 * #GcalEventEditorDialog is the event editor dialog of GNOME Calendar. It
 * allows the user to change the various aspects of the events, as
 * well as managing alarms.
 */

struct _GcalEventEditorDialog
{
  GtkDialog         parent;

  gboolean          writable;

  GcalContext      *context;

  /* titlebar */
  GtkWidget        *titlebar;
  GtkWidget        *title_label;
  GtkWidget        *subtitle_label;

  GtkWidget        *scrolled_window;

  GcalEventEditorSection *reminders_section;
  GcalEventEditorSection *schedule_section;
  GcalEventEditorSection *summary_section;
  GcalEventEditorSection *notes_section;

  GtkWidget        *lock;
  GtkWidget        *source_image;
  GtkWidget        *source_label;

  GtkWidget        *delete_button;
  GtkWidget        *done_button;
  GtkWidget        *cancel_button;
  GtkWidget        *sources_button;
  GtkWidget        *sources_popover;

  GBinding           *event_title_binding;

  /* actions */
  GMenu              *sources_menu;
  GSimpleActionGroup *action_group;

  /* new data holders */
  GcalEvent        *event;
  GcalCalendar     *selected_calendar;

  /* flags */
  gboolean          event_is_new;
  gboolean          recurrence_changed;
  gboolean          setting_event;
};

static void          on_calendar_selected_action_cb              (GSimpleAction      *menu_item,
                                                                  GVariant           *value,
                                                                  gpointer            user_data);

G_DEFINE_TYPE (GcalEventEditorDialog, gcal_event_editor_dialog, GTK_TYPE_DIALOG)

enum
{
  PROP_0,
  PROP_CONTEXT,
  PROP_EVENT,
  PROP_WRITABLE,
  N_PROPS
};

static GParamSpec* properties[N_PROPS] = { NULL, };

static const GActionEntry action_entries[] =
{
  { "select-calendar", on_calendar_selected_action_cb, "s" },
};


/*
 * Auxiliary methods
 */

static gint
sources_menu_sort_func (gconstpointer a,
                        gconstpointer b)
{
  GcalCalendar *calendar1, *calendar2;

  calendar1 = GCAL_CALENDAR ((gpointer) a);
  calendar2 = GCAL_CALENDAR ((gpointer) b);

  return g_ascii_strcasecmp (gcal_calendar_get_name (calendar1),
                             gcal_calendar_get_name (calendar2));
}

static void
fill_sources_menu (GcalEventEditorDialog *self)
{
  g_autoptr (GList) list = NULL;
  GcalManager *manager;
  GList *aux;

  if (self->context == NULL)
    return;

  manager = gcal_context_get_manager (self->context);
  list = gcal_manager_get_calendars (manager);
  self->sources_menu = g_menu_new ();

  list = g_list_sort (list, sources_menu_sort_func);

  for (aux = list; aux != NULL; aux = aux->next)
    {
      GcalCalendar *calendar;
      const GdkRGBA *color;
      GMenuItem *item;
      cairo_surface_t *surface;
      GdkPixbuf *pix;

      calendar = GCAL_CALENDAR (aux->data);

      /* retrieve color */
      color = gcal_calendar_get_color (calendar);
      surface = get_circle_surface_from_color (color, 16);
      pix = gdk_pixbuf_get_from_surface (surface, 0, 0, 16, 16);

      /* menu item */
      item = g_menu_item_new (gcal_calendar_get_name (calendar), "select-calendar");
      g_menu_item_set_icon (item, G_ICON (pix));

      /* set insensitive for read-only calendars */
      if (gcal_calendar_is_read_only (calendar))
        {
          g_menu_item_set_action_and_target_value (item, "select-calendar", NULL);
        }
      else
        {
          const gchar *id = gcal_calendar_get_id (calendar);
          g_menu_item_set_action_and_target_value (item, "select-calendar", g_variant_new_string (id));
        }

      g_menu_append_item (self->sources_menu, item);

      g_clear_pointer (&surface, cairo_surface_destroy);
      g_object_unref (pix);
      g_object_unref (item);
    }

  gtk_popover_bind_model (GTK_POPOVER (self->sources_popover), G_MENU_MODEL (self->sources_menu), "edit");

  /* HACK: show the popover menu icons */
  fix_popover_menu_icons (GTK_POPOVER (self->sources_popover));
}

static void
set_writable (GcalEventEditorDialog *self,
              gboolean               writable)
{
  if (self->writable == writable)
    return;

  gtk_button_set_label (GTK_BUTTON (self->done_button), writable ? _("Save") : _("Done"));

  self->writable = writable;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_WRITABLE]);
}

static void
apply_changes_to_event (GcalEventEditorDialog *self)
{
  gcal_event_editor_section_apply (self->notes_section);
  gcal_event_editor_section_apply (self->reminders_section);
  gcal_event_editor_section_apply (self->schedule_section);
  gcal_event_editor_section_apply (self->summary_section);
}


/*
 * Callbacks
 */

static void
on_calendar_selected_action_cb (GSimpleAction *action,
                                GVariant      *value,
                                gpointer       user_data)
{
  g_autoptr (GList) list = NULL;
  GcalEventEditorDialog *self;
  GcalManager *manager;
  GList *aux;
  gchar *uid;

  GCAL_ENTRY;

  self = GCAL_EVENT_EDITOR_DIALOG (user_data);
  manager = gcal_context_get_manager (self->context);
  list = gcal_manager_get_calendars (manager);

  /* retrieve selected calendar uid */
  g_variant_get (value, "s", &uid);

  /* search for any source with the given UID */
  for (aux = list; aux != NULL; aux = aux->next)
    {
      GcalCalendar *calendar = GCAL_CALENDAR (aux->data);

      if (g_strcmp0 (gcal_calendar_get_id (calendar), uid) == 0)
        {
          cairo_surface_t *surface;
          const GdkRGBA *color;

          /* retrieve color */
          color = gcal_calendar_get_color (calendar);
          surface = get_circle_surface_from_color (color, 16);
          gtk_image_set_from_surface (GTK_IMAGE (self->source_image), surface);

          self->selected_calendar = calendar;

          gtk_label_set_label (GTK_LABEL (self->subtitle_label), gcal_calendar_get_name (calendar));

          g_clear_pointer (&surface, cairo_surface_destroy);
          break;
        }
    }

  g_free (uid);

  GCAL_EXIT;
}

static void
transient_size_allocate_cb (GcalEventEditorDialog *self)
{
  GtkAllocation alloc;
  GtkWindow *transient;

  transient = gtk_window_get_transient_for (GTK_WINDOW (self));
  gtk_widget_get_allocation (GTK_WIDGET (transient), &alloc);

  gtk_scrolled_window_set_max_content_height (GTK_SCROLLED_WINDOW (self->scrolled_window),
                                              MAX (400, (gint) (0.75 * alloc.height)));
}

static void
fix_reminders_label_height_cb (GtkWidget    *summary_label,
                               GdkRectangle *allocation,
                               GtkWidget    *reminders_label)
{
  gtk_widget_set_size_request (reminders_label, -1, allocation->height);
}

static void
on_action_button_clicked_cb (GtkWidget *widget,
                             gpointer   user_data)
{
  GcalEventEditorDialog *self;

  GCAL_ENTRY;

  self = GCAL_EVENT_EDITOR_DIALOG (user_data);

  if (widget == self->cancel_button || (widget == self->done_button && !self->writable))
    {
      gtk_dialog_response (GTK_DIALOG (self), GTK_RESPONSE_CANCEL);
    }
  else if (widget == self->delete_button)
    {
      gtk_dialog_response (GTK_DIALOG (self), GCAL_RESPONSE_DELETE_EVENT);
    }
  else
    {
      GcalCalendar *calendar;
      gint response;

      apply_changes_to_event (self);

      response = self->event_is_new ? GCAL_RESPONSE_CREATE_EVENT : GCAL_RESPONSE_SAVE_EVENT;

      /* Update the source if needed */
      calendar = gcal_event_get_calendar (self->event);

      if (self->selected_calendar && calendar != self->selected_calendar)
        {
          if (self->event_is_new)
            {
              gcal_event_set_calendar (self->event, self->selected_calendar);
            }
          else
            {
              gcal_manager_move_event_to_source (gcal_context_get_manager (self->context),
                                                 self->event,
                                                 gcal_calendar_get_source (self->selected_calendar));
              response = GTK_RESPONSE_CANCEL;
            }
        }

      self->selected_calendar = NULL;

      /* Send the response */
      gtk_dialog_response (GTK_DIALOG (self), response);
    }

  GCAL_EXIT;
}


/*
 * Gobject overrides
 */

static void
gcal_event_editor_dialog_finalize (GObject *object)
{
  GcalEventEditorDialog *self;

  GCAL_ENTRY;

  self = GCAL_EVENT_EDITOR_DIALOG (object);

  g_clear_object (&self->action_group);
  g_clear_object (&self->context);
  g_clear_object (&self->event);

  G_OBJECT_CLASS (gcal_event_editor_dialog_parent_class)->finalize (object);

  GCAL_EXIT;
}

static void
gcal_event_editor_dialog_constructed (GObject* object)
{
  GcalEventEditorDialog *self;

  self = GCAL_EVENT_EDITOR_DIALOG (object);

  /* chaining up */
  G_OBJECT_CLASS (gcal_event_editor_dialog_parent_class)->constructed (object);

  gtk_window_set_title (GTK_WINDOW (object), "");

  /* titlebar */
  gtk_window_set_titlebar (GTK_WINDOW (object), self->titlebar);

  /* Actions */
  self->action_group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (self->action_group),
                                   action_entries,
                                   G_N_ELEMENTS (action_entries),
                                   self);

  gtk_widget_insert_action_group (GTK_WIDGET (self),
                                  "edit",
                                  G_ACTION_GROUP (self->action_group));

  /* Watch the main window and adapt the maximum size */
  g_signal_connect_object (gtk_window_get_transient_for (GTK_WINDOW (self)),
                           "size-allocate",
                           G_CALLBACK (transient_size_allocate_cb),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
gcal_event_editor_dialog_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  GcalEventEditorDialog *self = GCAL_EVENT_EDITOR_DIALOG (object);

  switch (prop_id)
    {
    case PROP_EVENT:
      g_value_set_object (value, self->event);
      break;

    case PROP_CONTEXT:
      g_value_set_object (value, self->context);
      break;

    case PROP_WRITABLE:
      g_value_set_boolean (value, self->writable);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_event_editor_dialog_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  GcalEventEditorDialog *self = GCAL_EVENT_EDITOR_DIALOG (object);

  switch (prop_id)
    {
    case PROP_EVENT:
      gcal_event_editor_dialog_set_event (self, g_value_get_object (value));
      break;

    case PROP_CONTEXT:
      g_assert (self->context == NULL);
      self->context = g_value_dup_object (value);
      g_object_notify_by_pspec (object, properties[PROP_CONTEXT]);
      break;

    case PROP_WRITABLE:
      set_writable (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_event_editor_dialog_class_init (GcalEventEditorDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  g_type_ensure (GCAL_TYPE_NOTES_SECTION);
  g_type_ensure (GCAL_TYPE_REMINDERS_SECTION);
  g_type_ensure (GCAL_TYPE_SCHEDULE_SECTION);
  g_type_ensure (GCAL_TYPE_SUMMARY_SECTION);

  object_class->finalize = gcal_event_editor_dialog_finalize;
  object_class->constructed = gcal_event_editor_dialog_constructed;
  object_class->get_property = gcal_event_editor_dialog_get_property;
  object_class->set_property = gcal_event_editor_dialog_set_property;

  /**
   * GcalEventEditorDialog::event:
   *
   * The #GcalEvent being edited.
   */
  properties[PROP_EVENT] = g_param_spec_object ("event",
                                                "event of the dialog",
                                                "The event being edited",
                                                GCAL_TYPE_EVENT,
                                                G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GcalEventEditorDialog::manager:
   *
   * The #GcalManager of the dialog.
   */
  properties[PROP_CONTEXT] = g_param_spec_object ("context",
                                                  "Context of the dialog",
                                                  "The context of the dialog",
                                                  GCAL_TYPE_CONTEXT,
                                                  G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GcalEventEditorDialog::writable:
   *
   * Whether the current event can be edited or not.
   */
  properties[PROP_WRITABLE] = g_param_spec_boolean ("writable",
                                                    "Whether the current event can be edited",
                                                    "Whether the current event can be edited or not",
                                                    TRUE,
                                                    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/event-editor/gcal-event-editor-dialog.ui");

  /* Buttons */
  gtk_widget_class_bind_template_child (widget_class, GcalEventEditorDialog, done_button);
  gtk_widget_class_bind_template_child (widget_class, GcalEventEditorDialog, cancel_button);
  gtk_widget_class_bind_template_child (widget_class, GcalEventEditorDialog, delete_button);
  gtk_widget_class_bind_template_child (widget_class, GcalEventEditorDialog, sources_button);
  /* Other */
  gtk_widget_class_bind_template_child (widget_class, GcalEventEditorDialog, titlebar);
  gtk_widget_class_bind_template_child (widget_class, GcalEventEditorDialog, title_label);
  gtk_widget_class_bind_template_child (widget_class, GcalEventEditorDialog, subtitle_label);
  gtk_widget_class_bind_template_child (widget_class, GcalEventEditorDialog, lock);
  gtk_widget_class_bind_template_child (widget_class, GcalEventEditorDialog, scrolled_window);
  gtk_widget_class_bind_template_child (widget_class, GcalEventEditorDialog, source_image);
  gtk_widget_class_bind_template_child (widget_class, GcalEventEditorDialog, sources_popover);
  gtk_widget_class_bind_template_child (widget_class, GcalEventEditorDialog, notes_section);
  gtk_widget_class_bind_template_child (widget_class, GcalEventEditorDialog, reminders_section);
  gtk_widget_class_bind_template_child (widget_class, GcalEventEditorDialog, schedule_section);
  gtk_widget_class_bind_template_child (widget_class, GcalEventEditorDialog, summary_section);


  /* callbacks */
  gtk_widget_class_bind_template_callback (widget_class, fix_reminders_label_height_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_action_button_clicked_cb);
}

static void
gcal_event_editor_dialog_init (GcalEventEditorDialog *self)
{
  self->writable = TRUE;

  gtk_widget_init_template (GTK_WIDGET (self));

  g_object_bind_property (self, "context", self->notes_section, "context", G_BINDING_DEFAULT);
  g_object_bind_property (self, "context", self->reminders_section, "context", G_BINDING_DEFAULT);
  g_object_bind_property (self, "context", self->schedule_section, "context", G_BINDING_DEFAULT);
  g_object_bind_property (self, "context", self->summary_section, "context", G_BINDING_DEFAULT);
}

/**
 * gcal_event_editor_dialog_new:
 *
 * Creates a new #GcalEventEditorDialog
 *
 * Returns: (transfer full): a #GcalEventEditorDialog
 */
GtkWidget*
gcal_event_editor_dialog_new (void)
{
  return g_object_new (GCAL_TYPE_EVENT_EDITOR_DIALOG, NULL);
}

/**
 * gcal_event_editor_dialog_set_event_is_new:
 * @dialog: a #GcalDialog
 * @event_is_new: %TRUE if the event is new, %FALSE otherwise
 *
 * Sets whether the currently edited event is a new event, or not.
 * The @dialog will adapt it's UI elements to reflect that.
 */
void
gcal_event_editor_dialog_set_event_is_new (GcalEventEditorDialog *self,
                                           gboolean               event_is_new)
{
  self->event_is_new = event_is_new;

  gtk_widget_set_visible (self->delete_button, !event_is_new);
}

/**
 * gcal_event_editor_dialog_get_event:
 * @dialog: a #GcalDialog
 *
 * Retrieves the current event being edited by the @dialog.
 *
 * Returns: (transfer none)(nullable): a #GcalEvent
 */
GcalEvent*
gcal_event_editor_dialog_get_event (GcalEventEditorDialog *self)
{
  g_return_val_if_fail (GCAL_IS_EVENT_EDITOR_DIALOG (self), NULL);

  return self->event;
}

/**
 * gcal_event_editor_dialog_set_event:
 * @dialog: a #GcalDialog
 * @event: (nullable): a #GcalEvent
 *
 * Sets the event of the @dialog. When @event is
 * %NULL, the current event information is unset.
 */
void
gcal_event_editor_dialog_set_event (GcalEventEditorDialog *self,
                                    GcalEvent             *event)
{
  g_autoptr (GcalEvent) cloned_event = NULL;
  GcalEventEditorFlags flags;
  GcalCalendar *calendar;
  cairo_surface_t *surface;

  GCAL_ENTRY;

  g_return_if_fail (GCAL_IS_EVENT_EDITOR_DIALOG (self));

  g_clear_object (&self->event);

  self->setting_event = TRUE;

  /* If we just set the event to NULL, simply send a property notify */
  if (!event)
    GCAL_GOTO (out);

  cloned_event = gcal_event_new_from_event (event);
  self->event = g_object_ref (cloned_event);

  calendar = gcal_event_get_calendar (cloned_event);

  /* update sources list */
  if (self->sources_menu != NULL)
    g_menu_remove_all (self->sources_menu);

  fill_sources_menu (self);

  /* dialog titlebar's title & subtitle */
  surface = get_circle_surface_from_color (gcal_event_get_color (cloned_event), 10);
  gtk_image_set_from_surface (GTK_IMAGE (self->source_image), surface);
  g_clear_pointer (&surface, cairo_surface_destroy);

  g_clear_pointer (&self->event_title_binding, g_binding_unbind);
  self->event_title_binding = g_object_bind_property (cloned_event,
                                                      "summary",
                                                      self->title_label,
                                                      "label",
                                                      G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
  g_object_add_weak_pointer (G_OBJECT (self->event_title_binding),
                             (gpointer*) &self->event_title_binding);

  gtk_label_set_label (GTK_LABEL (self->subtitle_label), gcal_calendar_get_name (calendar));
  self->selected_calendar = calendar;

  /* recurrence_changed */
  self->recurrence_changed = FALSE;

  set_writable (self, !gcal_calendar_is_read_only (calendar));

out:
  flags = GCAL_EVENT_EDITOR_FLAG_NONE;

  if (self->event_is_new)
    flags |= GCAL_EVENT_EDITOR_FLAG_NEW_EVENT;

  gcal_event_editor_section_set_event (self->notes_section, cloned_event, flags);
  gcal_event_editor_section_set_event (self->reminders_section, cloned_event, flags);
  gcal_event_editor_section_set_event (self->schedule_section, cloned_event, flags);
  gcal_event_editor_section_set_event (self->summary_section, cloned_event, flags);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_EVENT]);

  self->setting_event = FALSE;

  GCAL_EXIT;
}

gboolean
gcal_event_editor_dialog_get_recurrence_changed (GcalEventEditorDialog *self)
{
  g_return_val_if_fail (GCAL_IS_EVENT_EDITOR_DIALOG (self), FALSE);

  return self->recurrence_changed;
}
