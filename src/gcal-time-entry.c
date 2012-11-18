/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gcal-time-entry.c
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

#include "gcal-time-entry.h"

#include <glib/gi18n.h>

struct _GcalTimeEntryPrivate
{
  gboolean internal_delete;

  gint    hours;
  gint    minutes;
};

enum
{
  MODIFIED,
  NUM_SIGNALS
};

static guint signals[NUM_SIGNALS] = { 0, };


static void     gtk_editable_iface_init                        (GtkEditableInterface *iface);

static void     gcal_time_entry_constructed                    (GObject              *object);

static gboolean gcal_time_entry_key_press_event                (GtkWidget            *widget,
                                                                GdkEventKey          *event);

static void     gcal_time_entry_insert_text                    (GtkEditable          *editable,
                                                                const gchar          *new_text,
                                                                gint                  new_text_length,
                                                                gint                 *position);

static void     gcal_time_entry_delete_text                    (GtkEditable          *editable,
                                                                gint                  start_pos,
                                                                gint                  end_pos);

G_DEFINE_TYPE_WITH_CODE (GcalTimeEntry,
                         gcal_time_entry,
                         GTK_TYPE_ENTRY,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_EDITABLE,
                                                gtk_editable_iface_init));

static void
gcal_time_entry_class_init (GcalTimeEntryClass *klass)
{
  GtkWidgetClass *widget_class;
  GObjectClass *object_class;

  widget_class = GTK_WIDGET_CLASS (klass);
  widget_class->key_press_event = gcal_time_entry_key_press_event;

  object_class = G_OBJECT_CLASS (klass);
  object_class->constructed = gcal_time_entry_constructed;

  signals[MODIFIED] = g_signal_new ("modified",
                                    GCAL_TYPE_TIME_ENTRY,
                                    G_SIGNAL_RUN_LAST,
                                    G_STRUCT_OFFSET (GcalTimeEntryClass,
                                                     modified),
                                    NULL, NULL,
                                    g_cclosure_marshal_VOID__VOID,
                                    G_TYPE_NONE,
                                    0);


  g_type_class_add_private ((gpointer)klass, sizeof(GcalTimeEntryPrivate));
}

static void
gcal_time_entry_init (GcalTimeEntry *self)
{
  GcalTimeEntryPrivate *priv;
  gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            GCAL_TYPE_TIME_ENTRY,
                                            GcalTimeEntryPrivate);
  priv = self->priv;

  priv->internal_delete = FALSE;
  priv->hours = 0;
  priv->minutes = 0;
}

static void
gtk_editable_iface_init (GtkEditableInterface *iface)
{
  iface->insert_text = gcal_time_entry_insert_text;
  iface->delete_text = gcal_time_entry_delete_text;
}

static void
gcal_time_entry_constructed (GObject *object)
{
  /* chaining up */
  G_OBJECT_CLASS (gcal_time_entry_parent_class)->constructed (object);

  gtk_entry_set_width_chars (GTK_ENTRY (object), 8);
  gtk_entry_set_alignment (GTK_ENTRY (object), 0.5);

  gtk_entry_set_icon_from_icon_name (GTK_ENTRY (object),
                                     GTK_ENTRY_ICON_SECONDARY,
                                     "preferences-system-time-symbolic");
  gtk_entry_set_icon_tooltip_text (GTK_ENTRY (object),
                                   GTK_ENTRY_ICON_SECONDARY,
                                   _("Change the time"));

  /* default text */
  gtk_entry_set_text (GTK_ENTRY (object), "00:00");
}

static gboolean
gcal_time_entry_key_press_event (GtkWidget   *widget,
                                 GdkEventKey *event)
{
  GcalTimeEntryPrivate *priv;
  gint cursor_position;
  gchar *new_time;

  priv = GCAL_TIME_ENTRY (widget)->priv;

  if (event->keyval == GDK_KEY_Up)
    {
      g_object_get (widget, "cursor-position", &cursor_position, NULL);
      if (cursor_position == 0 ||
          cursor_position == 1)
        {
          priv->hours = (priv->hours + 1) % 24;
        }
      else if (cursor_position == 3 ||
               cursor_position == 4)
        {
          priv->minutes = (priv->minutes + 1) % 60;
        }

      new_time = g_strdup_printf ("%.2d:%.2d", priv->hours, priv->minutes);
      gtk_entry_set_text (GTK_ENTRY (widget), new_time);
      g_free (new_time);

      g_signal_emit_by_name (widget, "move-cursor", 0, cursor_position, FALSE, NULL);
      return TRUE;
    }
  else if (event->keyval == GDK_KEY_Down)
    {
      g_object_get (widget, "cursor-position", &cursor_position, NULL);
      if (cursor_position == 0 ||
          cursor_position == 1)
        {
          priv->hours = priv->hours == 0 ? 23 : priv->hours - 1;
        }
      else if (cursor_position == 3 ||
               cursor_position == 4)
        {
          priv->minutes = priv->minutes == 0 ? 59 : priv->minutes - 1;
        }

      new_time = g_strdup_printf ("%.2d:%.2d", priv->hours, priv->minutes);
      gtk_entry_set_text (GTK_ENTRY (widget), new_time);
      g_free (new_time);

      g_signal_emit_by_name (widget, "move-cursor", 0, cursor_position, FALSE, NULL);
      return TRUE;
    }

  GTK_WIDGET_CLASS (gcal_time_entry_parent_class)->key_press_event (widget, event);

  return FALSE;
}

static void
gcal_time_entry_insert_text (GtkEditable *editable,
                             const gchar *new_text,
                             gint         new_text_length,
                             gint        *position)
{
  GcalTimeEntryPrivate *priv;
  GtkEditableInterface *parent_editable_iface;
  gint i;
  gboolean had_colon;
  gchar *text_to_insert;
  gchar *old_text;
  gchar *hours_text;
  gint next_pos;

  priv = GCAL_TIME_ENTRY (editable)->priv;

  parent_editable_iface = g_type_interface_peek (gcal_time_entry_parent_class,
                                                 GTK_TYPE_EDITABLE);
  next_pos = *position + new_text_length;

  if (*position > 4 ||
      *position == 2)
    {
      return;
    }

  new_text_length = new_text_length + *position > 5 ? 5 - *position : new_text_length;
  text_to_insert = g_malloc0 (sizeof(gchar) * new_text_length);
  old_text = g_strdup_printf ("%.2d:%.2d", priv->hours, priv->minutes);

  had_colon = FALSE;
  for (i = 0; i < new_text_length; i++)
    {
      if (new_text[i] == 0x3a)
        {
          had_colon = TRUE;
          continue;
        }
      if (new_text[i] < 0x30 || new_text[i] > 0x39)
        text_to_insert[i - (had_colon ? 1 : 0)] = old_text[*position + i];
      else
        text_to_insert[i - (had_colon ? 1 : 0)] = new_text[i];
    }

  if (text_to_insert[0] == '\0')
    return;

  if (had_colon)
    new_text_length--;

  switch (*position)
    {
      case 0:
        switch (new_text_length)
          {
            case 4:
              if (text_to_insert[2] <= '5')
                {
                  old_text[3] = text_to_insert[2];
                }
              old_text[4] = text_to_insert[3];
            case 2:
              if (text_to_insert[0] == '2')
                {
                  if (text_to_insert[1] <= '3')
                    old_text[1] = text_to_insert[1];
                  else if (old_text[1] > '3')
                    old_text[1] = '3';
                }
              else
                {
                  old_text[1] = text_to_insert[1];
                }
            case 1:
              if (text_to_insert[0] <= '2')
                {
                  old_text[0] = text_to_insert[0];

                  if (text_to_insert[0] == '2' &&
                      old_text[1] > '3')
                    {
                      old_text[1] = '3';
                    }
                }
              break;
            default:
              return;
          }
        break;
      case 1:
        /* this is the only case I will analyze here */
        if (new_text_length == 1)
          {
            if (old_text[0] == '2')
              {
                if (text_to_insert[0] < '4')
                  old_text[1] = text_to_insert[0];
              }
            else
              {
                old_text[1] = text_to_insert[0];
              }
          }
        break;
      case 3:
        switch (new_text_length)
          {
            case 2:
              old_text[4] = text_to_insert[1];
            case 1:
              if (text_to_insert[0] <= '5')
                {
                  old_text[3] = text_to_insert[0];
                }
              break;
            default:
              return;
          }
        break;
      case 4:
        old_text[4] = text_to_insert[0];
        break;

      default:
        return;
    }

  /* setting hours, minutes */
  hours_text = g_strdup_printf ("%.2s", old_text);
  priv->hours = (gint) g_ascii_strtoull (hours_text, NULL, 10);
  g_free (hours_text);
  priv->minutes = (gint) g_ascii_strtoull (old_text + 3, NULL, 10);

  priv->internal_delete = TRUE;
  gtk_editable_delete_text (editable, 0, 5);
  priv->internal_delete = FALSE;
  *position = 0;
  parent_editable_iface->insert_text (editable,
                                      old_text,
                                      5,
                                      position);

  /* skipping colon */
  if (next_pos == 2)
    next_pos = 3;
  *position = next_pos;
  g_free (old_text);
  g_free (text_to_insert);

  /* sending modified signal */
  g_signal_emit (editable, signals[MODIFIED], 0);
}

static void
gcal_time_entry_delete_text (GtkEditable *editable,
                             gint         start_pos,
                             gint         end_pos)
{
  GcalTimeEntryPrivate *priv;
  GtkEditableInterface *parent_editable_iface;

  priv = GCAL_TIME_ENTRY (editable)->priv;
  parent_editable_iface = g_type_interface_peek (gcal_time_entry_parent_class,
                                                 GTK_TYPE_EDITABLE);
  if (! priv->internal_delete)
    {
      /* FIXME: Handle delete_text */
      /* handle delete */
    }

  parent_editable_iface->delete_text (editable,
                                      start_pos,
                                      end_pos);

}

/* Public API */
GtkWidget*
gcal_time_entry_new (void)
{
  return g_object_new (GCAL_TYPE_TIME_ENTRY, NULL);
}

void
gcal_time_entry_set_time (GcalTimeEntry *entry,
                          gint           hours,
                          gint           minutes)
{
  GcalTimeEntryPrivate *priv;
  gchar *new_time;

  g_return_if_fail (GCAL_IS_TIME_ENTRY (entry));
  priv = entry->priv;
  g_warn_if_fail (hours < 24);
  g_warn_if_fail (minutes < 60);

  priv->hours = hours < 24 ? hours : 0;
  priv->minutes = minutes < 60 ? minutes : 0;

  new_time = g_strdup_printf ("%.2d:%.2d", priv->hours, priv->minutes);
  gtk_entry_set_text (GTK_ENTRY (entry), new_time);
  g_free (new_time);
}

void
gcal_time_entry_get_time (GcalTimeEntry *entry,
                          gint          *hours,
                          gint          *minutes)
{
  GcalTimeEntryPrivate *priv;

  g_return_if_fail (GCAL_IS_TIME_ENTRY (entry));
  priv = entry->priv;

  *hours = priv->hours;
  *minutes = priv->minutes;
}
