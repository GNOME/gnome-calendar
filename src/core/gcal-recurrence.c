/* gcal-recurrence.c
 *
 * Copyright (C) 2017 Yash Singh <yashdev10p@gmail.com>
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

#include "gcal-recurrence.h"
#include "gcal-utils.h"
#include "gcal-event.h"

#include <glib.h>

G_DEFINE_BOXED_TYPE (GcalRecurrence, gcal_recurrence, gcal_recurrence_ref, gcal_recurrence_unref)

static void
gcal_recurrence_free (GcalRecurrence *self)
{
  g_assert (self);
  g_assert_cmpint (self->ref_count, ==, 0);

  gcal_clear_date_time (&self->limit.until);
  g_slice_free (GcalRecurrence, self);
}

/**
 * gcal_recurrence_new:
 *
 * Creates a new #GcalRecurrence
 *
 * Returns: (transfer full): a #GcalRecurrence
 */
GcalRecurrence*
gcal_recurrence_new (void)
{
  GcalRecurrence *new_recur;

  new_recur = g_slice_new (GcalRecurrence);

  new_recur->frequency = GCAL_RECURRENCE_NO_REPEAT;
  new_recur->limit_type = GCAL_RECURRENCE_FOREVER;

  new_recur->limit.until = NULL;
  new_recur->limit.count = 0;

  g_atomic_ref_count_init (&new_recur->ref_count);

  return new_recur;
}

/**
 * gcal_recurrence_copy:
 * @recur: a #GcalRecurrence
 *
 * Creates a copy of @recur
 *
 * Returns: (transfer full): a #GcalRecurrence
 */
GcalRecurrence*
gcal_recurrence_copy (GcalRecurrence *recur)
{
  GcalRecurrence *new_recur;

  g_return_val_if_fail (recur != NULL, NULL);

  new_recur = gcal_recurrence_new ();

  new_recur->frequency = recur->frequency;
  new_recur->limit_type = recur->limit_type;

  if (recur->limit_type == GCAL_RECURRENCE_UNTIL && recur->limit.until)
    new_recur->limit.until = g_date_time_ref (recur->limit.until);

  new_recur->limit.count = recur->limit.count;

  return new_recur;
}

GcalRecurrence *
gcal_recurrence_ref (GcalRecurrence *self)
{
  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (self->ref_count, NULL);

  g_atomic_ref_count_inc (&self->ref_count);

  return self;
}

void
gcal_recurrence_unref (GcalRecurrence *self)
{
  g_return_if_fail (self);
  g_return_if_fail (self->ref_count);

  if (g_atomic_ref_count_dec (&self->ref_count))
    gcal_recurrence_free (self);
}

/**
 * gcal_recurrence_is_equal:
 * @recur1: (nullable): a #GcalRecurrence
 * @recur2: (nullable): a #GcalRecurrence
 *
 * Checks if @recur1 and @recur2 are equal or not
 *
 * Returns: %TRUE if both are equal, %FALSE otherwise
 */

gboolean
gcal_recurrence_is_equal (GcalRecurrence *recur1,
                          GcalRecurrence *recur2)
{
  if (recur1 == recur2)
    return TRUE;
  else if (!recur1 || !recur2)
    return FALSE;

  if (recur1->frequency != recur2->frequency)
    return FALSE;

  if (recur1->limit_type != recur2->limit_type)
    return FALSE;

  if (recur1->limit_type == GCAL_RECURRENCE_UNTIL)
    {
      if (!g_date_time_equal (recur1->limit.until, recur2->limit.until))
        return FALSE;
    }
  else if (recur1->limit_type == GCAL_RECURRENCE_COUNT)
    {
      if (recur1->limit.count != recur2->limit.count)
        return FALSE;
    }

  return TRUE;
}

/**
 * gcal_recurrence_parse_recurrence_rules:
 * @comp: an #ECalComponent
 *
 * Parses the rrule of the @comp and
 * sets the GcalRecurrence struct accordingly.
 *
 * Returns: (transfer full): a #GcalRecurrence
 */
GcalRecurrence*
gcal_recurrence_parse_recurrence_rules (ECalComponent *comp)
{
  GcalRecurrence *recur;
  ICalProperty *prop;
  ICalComponent *icalcomp;
  ICalRecurrence *rrule;

  if (!e_cal_component_has_recurrences (comp))
    return NULL;

  recur = gcal_recurrence_new ();
  icalcomp = e_cal_component_get_icalcomponent (comp);

  prop = i_cal_component_get_first_property (icalcomp, I_CAL_RRULE_PROPERTY);
  g_return_val_if_fail (prop != NULL, NULL);

  rrule = i_cal_property_get_rrule (prop);

  g_clear_object (&prop);

  switch (i_cal_recurrence_get_freq (rrule))
    {
      case I_CAL_DAILY_RECURRENCE:
        recur->frequency = GCAL_RECURRENCE_DAILY;
        break;

      case I_CAL_WEEKLY_RECURRENCE:
        {
          if (i_cal_recurrence_get_by_day (rrule, 0) == I_CAL_MONDAY_WEEKDAY &&
              i_cal_recurrence_get_by_day (rrule, 1) == I_CAL_TUESDAY_WEEKDAY &&
              i_cal_recurrence_get_by_day (rrule, 2) == I_CAL_WEDNESDAY_WEEKDAY &&
              i_cal_recurrence_get_by_day (rrule, 3) == I_CAL_THURSDAY_WEEKDAY &&
              i_cal_recurrence_get_by_day (rrule, 4) == I_CAL_FRIDAY_WEEKDAY &&
              i_cal_recurrence_get_by_day (rrule, 5) != I_CAL_SATURDAY_WEEKDAY &&
              i_cal_recurrence_get_by_day (rrule, 6) != I_CAL_SUNDAY_WEEKDAY)
            {
              recur->frequency = GCAL_RECURRENCE_MON_FRI;
            }
          else
            {
              recur->frequency = GCAL_RECURRENCE_WEEKLY;
            }
          break;
        }

      case I_CAL_MONTHLY_RECURRENCE:
        recur->frequency = GCAL_RECURRENCE_MONTHLY;
        break;

      case I_CAL_YEARLY_RECURRENCE:
        recur->frequency = GCAL_RECURRENCE_YEARLY;
        break;

      default:
        recur->frequency = GCAL_RECURRENCE_OTHER;
    }

  if (i_cal_recurrence_get_count (rrule) > 0)
    {
      recur->limit_type = GCAL_RECURRENCE_COUNT;
      recur->limit.count = i_cal_recurrence_get_count (rrule);
    }
  else
    {
      ICalTime *until;

      until = i_cal_recurrence_get_until (rrule);

      if (i_cal_time_get_year (until) != 0)
        {
          recur->limit_type = GCAL_RECURRENCE_UNTIL;
          recur->limit.until = gcal_date_time_from_icaltime (until);
          if (!recur->limit.until)
            recur->limit_type = GCAL_RECURRENCE_FOREVER;
        }
      else
        {
          recur->limit_type = GCAL_RECURRENCE_FOREVER;
        }

      g_clear_object (&until);
    }

  g_clear_object (&rrule);

  return recur;
}

/**
 * gcal_recurrence_to_rrule:
 * @recur: a #GcalRecurrence
 *
 * Converts @recur into corresponding rrule.
 *
 * Returns: (transfer full): an #ICalRecurrence
 */
ICalRecurrence*
gcal_recurrence_to_rrule (GcalRecurrence *recur)
{
  ICalRecurrence *rrule;

  if (!recur)
    return NULL;

  /* Initialize and clear the rrule to get rid of unwanted fields */
  rrule = i_cal_recurrence_new ();

  switch (recur->frequency)
    {
    case GCAL_RECURRENCE_DAILY:
      i_cal_recurrence_set_freq (rrule, I_CAL_DAILY_RECURRENCE);
      break;

    case GCAL_RECURRENCE_WEEKLY:
      i_cal_recurrence_set_freq (rrule, I_CAL_WEEKLY_RECURRENCE);
      break;

    case GCAL_RECURRENCE_MONTHLY:
      i_cal_recurrence_set_freq (rrule, I_CAL_MONTHLY_RECURRENCE);
      break;

    case GCAL_RECURRENCE_YEARLY:
      i_cal_recurrence_set_freq (rrule, I_CAL_YEARLY_RECURRENCE);
      break;

    case GCAL_RECURRENCE_NO_REPEAT:
      i_cal_recurrence_set_freq (rrule, I_CAL_NO_RECURRENCE);
      break;

    case GCAL_RECURRENCE_MON_FRI:
      {
        i_cal_recurrence_set_freq (rrule, I_CAL_WEEKLY_RECURRENCE);
        i_cal_recurrence_set_by_day (rrule, 0, I_CAL_MONDAY_WEEKDAY);
        i_cal_recurrence_set_by_day (rrule, 1, I_CAL_TUESDAY_WEEKDAY);
        i_cal_recurrence_set_by_day (rrule, 2, I_CAL_WEDNESDAY_WEEKDAY);
        i_cal_recurrence_set_by_day (rrule, 3, I_CAL_THURSDAY_WEEKDAY);
        i_cal_recurrence_set_by_day (rrule, 4, I_CAL_FRIDAY_WEEKDAY);
        break;
      }

    default:
      i_cal_recurrence_set_freq (rrule, I_CAL_NO_RECURRENCE);
      break;
    }

  switch (recur->limit_type)
    {
    case GCAL_RECURRENCE_COUNT:
      i_cal_recurrence_set_count (rrule, recur->limit.count);
      break;

    case GCAL_RECURRENCE_UNTIL:
      {
        ICalTime *until;

        until = i_cal_time_new_null_time ();
        i_cal_time_set_date (until,
                             g_date_time_get_year (recur->limit.until),
                             g_date_time_get_month (recur->limit.until),
                             g_date_time_get_day_of_month (recur->limit.until));
        i_cal_time_set_time (until,
                             g_date_time_get_hour (recur->limit.until),
                             g_date_time_get_minute (recur->limit.until),
                             g_date_time_get_second (recur->limit.until));

        i_cal_recurrence_set_until (rrule, until);

        g_clear_object (&until);
        break;
      }

    case GCAL_RECURRENCE_FOREVER:
      break;

    default:
      break;
    }

  return rrule;
}
