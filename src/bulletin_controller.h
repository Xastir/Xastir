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
 * Presentation/logic separation: this header is intentionally free of
 * Motif/X11 dependencies so the controller can be unit-tested standalone.
 * Do NOT include xastir.h, main.h, or any Xt/Xm header from here.
 */

#ifndef XASTIR_BULLETIN_CONTROLLER_H
#define XASTIR_BULLETIN_CONTROLLER_H

#include <time.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* How often check_for_new_bulletins runs its outer gate (seconds). */
#define BULLETIN_CHECK_INTERVAL  15

/* How long to wait after the last new bulletin before doing the count
 * sweep (seconds). */
#define BULLETIN_SETTLE_TIME     15

/*
 * Filter + timing state for the bulletin subsystem.
 *
 * The persistent globals (bulletin_range, view_zero_distance_bulletins,
 * pop_up_new_bulletins, new_bulletin_flag, new_bulletin_count) are still
 * the xa_config source-of-truth; the GUI syncs them into this struct
 * before each logical operation.  A future pass will retire those globals.
 */
typedef struct
{
  /* Filter settings */
  int    range;               /* 0 = no distance limit; >0 = max distance    */
  int    view_zero_distance;  /* 1 = include bulletins with unknown position  */
  int    pop_up_enabled;      /* 1 = raise dialog when new bulletins arrive   */

  /* New-bulletin batch tracking */
  int    new_flag;            /* set when a new in-range bulletin arrives     */
  int    new_count;           /* count for current batch                      */
  time_t first_new_time;      /* earliest sec_heard in current batch          */
  time_t last_new_time;       /* most recent sec_heard in current batch       */
  time_t last_check;          /* wall time of the last check sweep            */
} bulletin_controller_t;


/* ------------------------------------------------------------------ */
/* Lifecycle                                                           */
/* ------------------------------------------------------------------ */

/*
 * Initialise with safe defaults:
 *   range=0, view_zero_distance=0, pop_up_enabled=0, all counters/timers 0.
 */
void bulletin_controller_init(bulletin_controller_t *bc);


/* ------------------------------------------------------------------ */
/* Range filter                                                        */
/* ------------------------------------------------------------------ */

/*
 * Returns 1 if a bulletin at the given distance (any unit) passes the
 * current filter settings stored in *bc, 0 otherwise.
 *
 *   distance  — pre-computed distance from my station (caller's units).
 *               Pass 0.0 when the position of the sending station is
 *               unknown (no posit received yet).
 *
 * Filter rules (matching the original code):
 *   - distance > 0 AND distance <= range  → show
 *   - distance > 0 AND range == 0         → show (no range limit)
 *   - distance == 0 AND view_zero_distance → show
 *   - otherwise                           → hide
 */
int bulletin_controller_in_range(const bulletin_controller_t *bc, double distance);


/* ------------------------------------------------------------------ */
/* New-bulletin batch tracking                                         */
/* ------------------------------------------------------------------ */

/*
 * Called when a NEW bulletin (temp_bulletin_record == -1) is received.
 * Updates first_new_time / last_new_time and sets new_flag = 1.
 * sec_heard is the bulletin's timestamp (use sec_now() at the call site).
 */
void bulletin_controller_record_new(bulletin_controller_t *bc, time_t sec_heard);

/*
 * Per-message callback used during the count sweep
 * (mirrors count_bulletin_messages logic).
 * Increments new_count if the bulletin at 'distance' arrived at or
 * after bc->first_new_time and passes the range filter.
 */
void bulletin_controller_count_one(bulletin_controller_t *bc,
                                   double distance,
                                   time_t sec_heard);

/*
 * Returns 1 if the outer timing gate in check_for_new_bulletins allows
 * a sweep to run, 0 otherwise.
 *
 *   curr_sec — current wall-clock seconds (time(NULL) at the call site).
 *
 * Gate conditions (both must be true):
 *   - curr_sec >= bc->last_check + BULLETIN_CHECK_INTERVAL
 *   - bc->new_flag is set
 *   - curr_sec >= bc->last_new_time + BULLETIN_SETTLE_TIME
 */
int bulletin_controller_should_check(const bulletin_controller_t *bc,
                                     time_t curr_sec);

/*
 * Update last_check to curr_sec (called at the top of the check loop
 * regardless of whether new bulletins were found).
 */
void bulletin_controller_mark_checked(bulletin_controller_t *bc, time_t curr_sec);

/*
 * Reset the batch after a check sweep completes:
 *   first_new_time = last_new_time + 1
 *   new_flag = 0
 *   new_count = 0
 */
void bulletin_controller_reset_batch(bulletin_controller_t *bc);


/* ------------------------------------------------------------------ */
/* Formatting                                                          */
/* ------------------------------------------------------------------ */

/*
 * Format one bulletin display line:
 *
 *   "%-9s:%-4s (%s %6.1f %s) %s\n"
 *    from_call  tag  time_str  distance  dist_unit  message
 *
 * tag is the raw call_sign field from the Message struct (e.g. "BLN0XYZ").
 * Only the last 4 characters (tag+3) are shown, matching the original code.
 *
 * time_str   — pre-formatted timestamp string (strftime output).
 * dist_unit  — localised unit label ("miles" or "km").
 *
 * Writes into out[outsz].  Returns 0 on success, -1 if any required
 * pointer is NULL or outsz == 0.
 */
int bulletin_format_line(
    const char *from_call,
    const char *tag,
    const char *time_str,
    double      distance,
    const char *dist_unit,
    const char *message,
    char       *out,
    size_t      outsz);

#ifdef __cplusplus
}
#endif

#endif /* XASTIR_BULLETIN_CONTROLLER_H */
