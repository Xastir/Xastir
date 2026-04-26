/*
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2026 The Xastir Group
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Look at the README for more information on the program.
 */

/*
 * Presentation-free kernel extracted from bulletin_gui.c.
 * No Motif, no X11, no Xastir globals — standard C only.
 */

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include <stdio.h>
#include <string.h>

#include "bulletin_controller.h"


/* ------------------------------------------------------------------ */
/* Lifecycle                                                           */
/* ------------------------------------------------------------------ */

void bulletin_controller_init(bulletin_controller_t *bc)
{
  if (!bc)
  {
    return;
  }
  bc->range              = 0;
  bc->view_zero_distance = 0;
  bc->pop_up_enabled     = 0;
  bc->new_flag           = 0;
  bc->new_count          = 0;
  bc->first_new_time     = (time_t)0;
  bc->last_new_time      = (time_t)0;
  bc->last_check         = (time_t)0;
}


/* ------------------------------------------------------------------ */
/* Range filter                                                        */
/* ------------------------------------------------------------------ */

int bulletin_controller_in_range(const bulletin_controller_t *bc, double distance)
{
  if (!bc)
  {
    return 0;
  }

  /* Unknown position (distance == 0.0): show only if view_zero_distance is on. */
  if (distance == 0.0)
  {
    return bc->view_zero_distance ? 1 : 0;
  }

  /* Known position (distance > 0): apply range filter. */
  if (bc->range == 0)
  {
    /* No range limit — show everything with a known position. */
    return 1;
  }

  return ((int)distance <= bc->range) ? 1 : 0;
}


/* ------------------------------------------------------------------ */
/* New-bulletin batch tracking                                         */
/* ------------------------------------------------------------------ */

void bulletin_controller_record_new(bulletin_controller_t *bc, time_t sec_heard)
{
  if (!bc)
  {
    return;
  }

  bc->new_flag = 1;

  /* Maintain the bracket: first_new_time is the earliest, last_new_time the latest. */
  if (bc->first_new_time == (time_t)0 || sec_heard < bc->first_new_time)
  {
    bc->first_new_time = sec_heard;
  }
  if (sec_heard > bc->last_new_time)
  {
    bc->last_new_time = sec_heard;
  }
}


void bulletin_controller_count_one(bulletin_controller_t *bc,
                                   double distance,
                                   time_t sec_heard)
{
  if (!bc)
  {
    return;
  }

  if (!bulletin_controller_in_range(bc, distance))
  {
    return;
  }

  if (sec_heard >= bc->first_new_time)
  {
    bc->new_count++;
  }
}


int bulletin_controller_should_check(const bulletin_controller_t *bc,
                                     time_t curr_sec)
{
  if (!bc)
  {
    return 0;
  }

  /* Outer throttle: don't check more than once every BULLETIN_CHECK_INTERVAL. */
  if (curr_sec < bc->last_check + BULLETIN_CHECK_INTERVAL)
  {
    return 0;
  }

  /* No new bulletins pending — nothing to do. */
  if (!bc->new_flag)
  {
    return 0;
  }

  /* Wait for the settle period after the last new bulletin. */
  if (curr_sec < bc->last_new_time + BULLETIN_SETTLE_TIME)
  {
    return 0;
  }

  return 1;
}


void bulletin_controller_mark_checked(bulletin_controller_t *bc, time_t curr_sec)
{
  if (!bc)
  {
    return;
  }
  bc->last_check = curr_sec;
}


void bulletin_controller_reset_batch(bulletin_controller_t *bc)
{
  if (!bc)
  {
    return;
  }
  bc->first_new_time = bc->last_new_time + 1;
  bc->new_flag       = 0;
  bc->new_count      = 0;
}


/* ------------------------------------------------------------------ */
/* Formatting                                                          */
/* ------------------------------------------------------------------ */

int bulletin_format_line(
    const char *from_call,
    const char *tag,
    const char *time_str,
    double      distance,
    const char *dist_unit,
    const char *message,
    char       *out,
    size_t      outsz)
{
  if (!from_call || !tag || !time_str || !dist_unit || !message
      || !out || outsz == 0)
  {
    return -1;
  }

  /* tag+3 matches the original code: "BLN0XYZ" → show "XYZ\0" */
  snprintf(out, outsz, "%-9s:%-4s (%s %6.1f %s) %s\n",
           from_call, tag + 3, time_str, distance, dist_unit, message);
  return 0;
}
