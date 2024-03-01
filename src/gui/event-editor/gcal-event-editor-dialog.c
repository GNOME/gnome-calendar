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
  AdwDialog               parent;

  GtkListBox             *calendars_listbox;
  GtkWidget              *cancel_button;
  GtkWidget              *delete_button;
  GtkWidget              *done_button;
  GtkWidget              *lock;
  GcalEventEditorSection *notes_section;
  GcalEventEditorSection *reminders_section;
  GcalEventEditorSection *schedule_section;
  GtkWidget              *sources_button;
  GtkWidget              *source_image;
  GtkWidget              *source_label;
  GtkWidget              *subtitle_label;
  GcalEventEditorSection *summary_section;
  GtkWidget              *titlebar;
  GtkWidget              *title_label;


  GcalEventEditorSection *sections[4];


  GMenu              *sources_menu;
  GSimpleActionGroup *action_group;

  GcalContext        *context;
  GcalEvent          *event;
  GcalCalendar       *selected_calendar;

  GBinding           *event_title_binding;

  /* flags */
  gboolean          event_is_new;
  gboolean          recurrence_changed;
  gboolean          writable;
};

G_DEFINE_TYPE (GcalEventEditorDialog, gcal_event_editor_dialog, ADW_TYPE_DIALOG)

enum
{
  PROP_0,
  PROP_CONTEXT,
  PROP_EVENT,
  PROP_WRITABLE,
  N_PROPS
};

enum
{
  REMOVE_EVENT,
  NUM_SIGNALS,
};

static guint signals[NUM_SIGNALS] = { 0, };
static GParamSpec* properties[N_PROPS] = { NULL, };


/*
 * Auxiliary methods
 */

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
clear_and_hide_dialog (GcalEventEditorDialog *self)
{
  gcal_event_editor_dialog_set_event (self, NULL, FALSE);
  adw_dialog_close (ADW_DIALOG (self));
}

static void
apply_event_properties_to_template_event (GcalEvent *template_event,
                                          GcalEvent *event)
{
  g_autoptr (GDateTime) start_date = NULL;
  g_autoptr (GDateTime) end_date = NULL;
  GDateTime *template_start_date;
  GDateTime *event_start_date;
  GDateTime *template_end_date;
  GDateTime *event_end_date;
  gboolean was_all_day;
  gboolean is_all_day;

  is_all_day = gcal_event_get_all_day (event);
  was_all_day = gcal_event_get_all_day (template_event);

  template_start_date = gcal_event_get_date_start (template_event);
  template_end_date = gcal_event_get_date_end (template_event);
  event_start_date = gcal_event_get_date_start (event);
  event_end_date = gcal_event_get_date_end (event);

  start_date = g_date_time_new (g_date_time_get_timezone (event_start_date),
                                g_date_time_get_year (template_start_date),
                                g_date_time_get_month (template_start_date),
                                g_date_time_get_day_of_month (template_start_date),
                                g_date_time_get_hour (event_start_date),
                                g_date_time_get_minute (event_start_date),
                                g_date_time_get_second (event_start_date));

  end_date = g_date_time_new (g_date_time_get_timezone (event_end_date),
                              g_date_time_get_year (template_end_date),
                              g_date_time_get_month (template_end_date),
                              g_date_time_get_day_of_month (template_end_date),
                              g_date_time_get_hour (event_end_date),
                              g_date_time_get_minute (event_end_date),
                              g_date_time_get_second (event_end_date));

  if (was_all_day != is_all_day)
    {
      g_autoptr (GDateTime) fake_end_date = NULL;

      if (is_all_day)
        fake_end_date = g_date_time_add_days (end_date, -1);
      else
        fake_end_date = g_date_time_add_days (end_date, 1);

      gcal_set_date_time (&end_date, fake_end_date);
    }

  gcal_event_set_summary (template_event, gcal_event_get_summary (event));
  gcal_event_set_location (template_event, gcal_event_get_location (event));
  gcal_event_set_description (template_event, gcal_event_get_description (event));
  gcal_event_set_recurrence (template_event, gcal_event_get_recurrence (event));
  gcal_event_set_all_day (template_event, gcal_event_get_all_day (event));
  gcal_event_set_date_start (template_event, start_date);
  gcal_event_set_date_end (template_event, end_date);
}

static gboolean
paintable_from_gdk_rgb (GBinding     *binding,
                        const GValue *from_value,
                        GValue       *to_value,
                        gpointer      user_data)
{
  GdkRGBA *rgba = g_value_get_boxed (from_value);
  g_value_take_object (to_value, get_circle_paintable_from_color (rgba, 16));
  return TRUE;
}

static GtkWidget *
create_row_func (gpointer data,
                 gpointer user_data)
{
  GcalCalendar *calendar = (GcalCalendar *) data;
  GtkWidget *label;
  GtkWidget *icon;
  GtkWidget *row;
  GtkWidget *box;

  g_assert (GCAL_IS_CALENDAR (calendar));

  row = gtk_list_box_row_new ();

  gtk_actionable_set_action_name (GTK_ACTIONABLE (row), "event-editor-dialog.select-calendar");
  if (!gcal_calendar_is_read_only (calendar))
    gtk_actionable_set_action_target (GTK_ACTIONABLE (row), "s", gcal_calendar_get_id (calendar));

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), box);

  icon = gtk_image_new ();
  gtk_widget_add_css_class (icon, "calendar-color-image");
  g_object_bind_property_full (data, "color",
                               icon, "paintable",
                               G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE,
                               paintable_from_gdk_rgb,
                               NULL,
                               NULL,
                               NULL);
  gtk_box_append (GTK_BOX (box), icon);

  label = gtk_label_new (NULL);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  g_object_bind_property (data, "name", label, "label", G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
  gtk_box_append (GTK_BOX (box), label);

  return row;
}

static void
setup_context (GcalEventEditorDialog *self)
{
  GcalManager *manager;
  GListModel *calendars;

  GCAL_ENTRY;

  manager = gcal_context_get_manager (self->context);
  calendars = gcal_manager_get_calendars_model (manager);

  gtk_list_box_bind_model (self->calendars_listbox, calendars, create_row_func, self, NULL);

  GCAL_EXIT;
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
  const gchar *uid;

  GCAL_ENTRY;

  self = GCAL_EVENT_EDITOR_DIALOG (user_data);
  manager = gcal_context_get_manager (self->context);
  list = gcal_manager_get_calendars (manager);

  /* retrieve selected calendar uid */
  g_variant_get (value, "&s", &uid);

  /* search for any source with the given UID */
  for (aux = list; aux != NULL; aux = aux->next)
    {
      GcalCalendar *calendar = GCAL_CALENDAR (aux->data);

      if (g_strcmp0 (gcal_calendar_get_id (calendar), uid) == 0)
        {
          g_autoptr (GdkPaintable) paintable = NULL;
          const GdkRGBA *color;

          /* retrieve color */
          color = gcal_calendar_get_color (calendar);
          paintable = get_circle_paintable_from_color (color, 16);
          gtk_image_set_from_paintable (GTK_IMAGE (self->source_image), paintable);

          self->selected_calendar = calendar;

          gtk_label_set_label (GTK_LABEL (self->subtitle_label), gcal_calendar_get_name (calendar));
          break;
        }
    }

  GCAL_EXIT;
}

static void
on_cancel_button_clicked_cb (GtkButton             *button,
                             GcalEventEditorDialog *self)
{
  GCAL_ENTRY;

  clear_and_hide_dialog (self);

  GCAL_EXIT;
}

static void
on_ask_recurrence_response_delete_cb (GcalEvent             *event,
                                      GcalRecurrenceModType  mod_type,
                                      gpointer               user_data)
{
  GcalEventEditorDialog *self = GCAL_EVENT_EDITOR_DIALOG (user_data);

  if (mod_type == GCAL_RECURRENCE_MOD_NONE)
    return;

  g_signal_emit (self, signals[REMOVE_EVENT], 0, event, mod_type);
  clear_and_hide_dialog (self);
}

static void
on_delete_button_clicked_cb (GtkButton             *button,
                             GcalEventEditorDialog *self)
{
  GcalRecurrenceModType mod = GCAL_RECURRENCE_MOD_THIS_ONLY;

  GCAL_ENTRY;

  if (gcal_event_has_recurrence (self->event))
    {
      gcal_utils_ask_recurrence_modification_type (GTK_WIDGET (self),
                                                   self->event,
                                                   TRUE,
                                                   on_ask_recurrence_response_delete_cb,
                                                   self);
    }
  else
    {
      g_signal_emit (self, signals[REMOVE_EVENT], 0, self->event, mod);
      clear_and_hide_dialog (self);
    }

  GCAL_EXIT;
}

static void
on_ask_recurrence_response_save_cb (GcalEvent             *event,
                                    GcalRecurrenceModType  mod_type,
                                    gpointer               user_data)
{
  GcalEventEditorDialog *self = GCAL_EVENT_EDITOR_DIALOG (user_data);
  GcalManager *manager;

  GCAL_ENTRY;

  manager = gcal_context_get_manager (self->context);

  switch (mod_type)
    {
    case GCAL_RECURRENCE_MOD_NONE:
      GCAL_RETURN ();

    case GCAL_RECURRENCE_MOD_ALL:
      {
        g_autoptr (GcalEvent) template_event = NULL;
        g_autoptr (GError) error = NULL;
        ECalComponentId *component_id;
        ICalComponent *template_icomponent;
        ECalComponent *template_ecomponent;
        ECalComponent *component;
        GcalCalendar *calendar;
        ECalClient *client;

        calendar = gcal_event_get_calendar (event);
        client = gcal_calendar_get_client (calendar);
        component = gcal_event_get_component (event);
        component_id = e_cal_component_get_id (component);

        e_cal_client_get_object_sync (client,
                                      e_cal_component_id_get_uid (component_id),
                                      NULL,
                                      &template_icomponent,
                                      NULL,
                                      &error);

        g_clear_pointer (&component_id, e_cal_component_id_free);

        if (error)
          {
            g_warning ("Error updating event: %s", error->message);
            break;
          }

        template_ecomponent = e_cal_component_new_from_icalcomponent (template_icomponent);
        template_event = gcal_event_new (calendar, template_ecomponent, &error);
        if (error)
          {
            g_warning ("Error updating event: %s", error->message);
            break;
          }

        apply_event_properties_to_template_event (template_event, event);
        gcal_manager_update_event (manager, template_event, mod_type);
      }
      break;

    case GCAL_RECURRENCE_MOD_THIS_AND_FUTURE:
    case GCAL_RECURRENCE_MOD_THIS_ONLY:
      gcal_manager_update_event (manager, self->event, mod_type);
      break;
    }

  clear_and_hide_dialog (self);

  GCAL_EXIT;
}

static void
on_done_button_clicked_cb (GtkButton             *button,
                           GcalEventEditorDialog *self)
{
  GcalCalendar *selected_calendar;
  GcalCalendar *calendar;
  GcalManager *manager;
  gboolean can_show_mod_all;
  gboolean was_recurrent;
  gboolean calendar_changed;
  gint i;

  manager = gcal_context_get_manager (self->context);
  calendar = gcal_event_get_calendar (self->event);

  if (gcal_calendar_is_read_only (calendar))
    GCAL_GOTO (out);

  selected_calendar = g_steal_pointer (&self->selected_calendar);
  calendar_changed = selected_calendar && calendar != selected_calendar;
  can_show_mod_all = TRUE;
  if (!self->event_is_new)
    {
      gboolean anything_changed = calendar_changed;

      for (i = 0; i < G_N_ELEMENTS (self->sections); i++)
        {
          gboolean section_changed;

          section_changed = gcal_event_editor_section_changed (self->sections[i]);
          anything_changed |= section_changed;
        }

      if (!anything_changed)
        goto out;

      can_show_mod_all =
        !gcal_schedule_section_recurrence_changed (GCAL_SCHEDULE_SECTION (self->schedule_section)) &&
        !gcal_schedule_section_day_changed (GCAL_SCHEDULE_SECTION (self->schedule_section));
    }

  /*
   * We don't want to ask the recurrence mod type if the event wasn't
   * actually recurrent.
   */
  was_recurrent = gcal_event_has_recurrence (self->event);

  for (i = 0; i < G_N_ELEMENTS (self->sections); i++)
    {
      if (gcal_event_editor_section_changed (self->sections[i]))
        gcal_event_editor_section_apply (self->sections[i]);
    }

  if (calendar_changed)
    {
      if (self->event_is_new)
        {
          gcal_event_set_calendar (self->event, selected_calendar);
        }
      else
        {
          ESource *source = gcal_calendar_get_source (selected_calendar);

          gcal_manager_move_event_to_source (manager, self->event, source);
          goto out;
        }
    }

  if (self->event_is_new)
    {
      gcal_manager_create_event (manager, self->event);
    }
  else if (was_recurrent && gcal_event_has_recurrence (self->event))
    {
      gcal_utils_ask_recurrence_modification_type (GTK_WIDGET (self),
                                                   self->event,
                                                   can_show_mod_all,
                                                   on_ask_recurrence_response_save_cb,
                                                   self);
      return;
    }
  else
    {
      gcal_manager_update_event (manager, self->event, GCAL_RECURRENCE_MOD_THIS_ONLY);
    }

out:
  clear_and_hide_dialog (self);
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
    case PROP_CONTEXT:
      g_assert (self->context == NULL);
      self->context = g_value_dup_object (value);
      setup_context (self);
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
  object_class->get_property = gcal_event_editor_dialog_get_property;
  object_class->set_property = gcal_event_editor_dialog_set_property;

  signals[REMOVE_EVENT] = g_signal_new ("remove-event",
                                        GCAL_TYPE_EVENT_EDITOR_DIALOG,
                                        G_SIGNAL_RUN_FIRST,
                                        0, NULL, NULL, NULL,
                                        G_TYPE_NONE,
                                        2,
                                        GCAL_TYPE_EVENT,
                                        G_TYPE_INT);

  /**
   * GcalEventEditorDialog::event:
   *
   * The #GcalEvent being edited.
   */
  properties[PROP_EVENT] = g_param_spec_object ("event",
                                                "event of the dialog",
                                                "The event being edited",
                                                GCAL_TYPE_EVENT,
                                                G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

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
                                                    FALSE,
                                                    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/calendar/ui/event-editor/gcal-event-editor-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, GcalEventEditorDialog, calendars_listbox);
  gtk_widget_class_bind_template_child (widget_class, GcalEventEditorDialog, cancel_button);
  gtk_widget_class_bind_template_child (widget_class, GcalEventEditorDialog, delete_button);
  gtk_widget_class_bind_template_child (widget_class, GcalEventEditorDialog, done_button);
  gtk_widget_class_bind_template_child (widget_class, GcalEventEditorDialog, lock);
  gtk_widget_class_bind_template_child (widget_class, GcalEventEditorDialog, notes_section);
  gtk_widget_class_bind_template_child (widget_class, GcalEventEditorDialog, reminders_section);
  gtk_widget_class_bind_template_child (widget_class, GcalEventEditorDialog, schedule_section);
  gtk_widget_class_bind_template_child (widget_class, GcalEventEditorDialog, sources_button);
  gtk_widget_class_bind_template_child (widget_class, GcalEventEditorDialog, source_image);
  gtk_widget_class_bind_template_child (widget_class, GcalEventEditorDialog, subtitle_label);
  gtk_widget_class_bind_template_child (widget_class, GcalEventEditorDialog, summary_section);
  gtk_widget_class_bind_template_child (widget_class, GcalEventEditorDialog, titlebar);
  gtk_widget_class_bind_template_child (widget_class, GcalEventEditorDialog, title_label);


  /* callbacks */
  gtk_widget_class_bind_template_callback (widget_class, on_cancel_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_delete_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_done_button_clicked_cb);
}

static void
gcal_event_editor_dialog_init (GcalEventEditorDialog *self)
{
  const GActionEntry action_entries[] = {
    { "select-calendar", on_calendar_selected_action_cb, "s" },
  };
  gint i = 0;

  /* Actions */
  self->action_group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (self->action_group),
                                   action_entries,
                                   G_N_ELEMENTS (action_entries),
                                   self);

  gtk_widget_insert_action_group (GTK_WIDGET (self),
                                  "event-editor-dialog",
                                  G_ACTION_GROUP (self->action_group));


  gtk_widget_init_template (GTK_WIDGET (self));

  self->sections[i++] = self->notes_section;
  self->sections[i++] = self->reminders_section;
  self->sections[i++] = self->schedule_section;
  self->sections[i++] = self->summary_section;

  for (i = 0; i < G_N_ELEMENTS (self->sections); i++)
    g_object_bind_property (self, "context", self->sections[i], "context", G_BINDING_DEFAULT);
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
 * gcal_event_editor_dialog_set_event:
 * @dialog: a #GcalDialog
 * @event: (nullable): a #GcalEvent
 *
 * Sets the event of the @dialog. When @event is
 * %NULL, the current event information is unset.
 */
void
gcal_event_editor_dialog_set_event (GcalEventEditorDialog *self,
                                    GcalEvent             *event,
                                    gboolean               new_event)
{
  g_autoptr (GdkPaintable) paintable = NULL;
  g_autoptr (GcalEvent) cloned_event = NULL;
  GcalEventEditorFlags flags;
  GcalCalendar *calendar;
  gint i;

  GCAL_ENTRY;

  g_return_if_fail (GCAL_IS_EVENT_EDITOR_DIALOG (self));

  g_clear_object (&self->event);

  /* If we just set the event to NULL, simply send a property notify */
  if (!event)
    GCAL_GOTO (out);

  cloned_event = gcal_event_new_from_event (event);
  self->event = g_object_ref (cloned_event);

  calendar = gcal_event_get_calendar (cloned_event);

  /* update sources list */
  if (self->sources_menu != NULL)
    g_menu_remove_all (self->sources_menu);

  /* dialog titlebar's title & subtitle */
  paintable = get_circle_paintable_from_color (gcal_event_get_color (cloned_event), 10);
  gtk_image_set_from_paintable (GTK_IMAGE (self->source_image), paintable);

  g_clear_pointer (&self->event_title_binding, g_binding_unbind);
  self->event_title_binding = g_object_bind_property (cloned_event,
                                                      "summary",
                                                      self->title_label,
                                                      "label",
                                                      G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
  g_object_add_weak_pointer (G_OBJECT (self->event_title_binding),
                             (gpointer*) &self->event_title_binding);

  /* change title when event name is empty */
  if (!gcal_is_valid_event_name (gtk_label_get_label (GTK_LABEL (self->title_label))))
    gtk_label_set_label (GTK_LABEL (self->title_label), _("New Event"));

  gtk_label_set_label (GTK_LABEL (self->subtitle_label), gcal_calendar_get_name (calendar));
  self->selected_calendar = calendar;

  /* recurrence_changed */
  self->recurrence_changed = FALSE;

  set_writable (self, !gcal_calendar_is_read_only (calendar));

  self->event_is_new = new_event;
  gtk_widget_set_visible (self->delete_button, !new_event);

out:
  flags = GCAL_EVENT_EDITOR_FLAG_NONE;

  if (new_event)
    flags |= GCAL_EVENT_EDITOR_FLAG_NEW_EVENT;

  for (i = 0; i < G_N_ELEMENTS (self->sections); i++)
    gcal_event_editor_section_set_event (self->sections[i], cloned_event, flags);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_EVENT]);

  GCAL_EXIT;
}
