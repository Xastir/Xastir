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
 * Presentation-free kernel extracted from track_gui.c.
 * No Motif, no X11, no Xastir GUI globals — standard C plus the three
 * call-validation predicates (valid_object / valid_call / valid_item).
 */

#ifndef __TRACK_CONTROLLER_H
#define __TRACK_CONTROLLER_H

#include <stddef.h>   /* size_t */

/* ------------------------------------------------------------------ */
/* Constants                                                           */
/* ------------------------------------------------------------------ */

/** Maximum hours a findu trail can span (also used as initial posit_start). */
#define MAX_FINDU_DURATION    120

/** Maximum hours ago findu will accept as a trail start offset. */
#define MAX_FINDU_START_TIME  336

/** Maximum length of a callsign field including NUL. */
#define TRACK_CALL_LEN  30

/** URL template used by track_controller_build_findu_url(). */
#define FINDU_URL_FMT  \
    "http://www.findu.com/cgi-bin/raw.cgi?call=%s&start=%d&length=%d"


/* ------------------------------------------------------------------ */
/* Controller struct                                                    */
/* ------------------------------------------------------------------ */

/**
 * All non-Widget state for the station-tracking and findu-trail dialogs.
 *
 * The caller is responsible for syncing the legacy file-scope globals in
 * track_gui.c to/from this struct before and after each controller call.
 */
typedef struct
{
  /* --- station tracking --- */
  int  station_on;                    /**< 1 = tracking enabled            */
  int  track_me;                      /**< 1 = tracking my own call         */
  int  case_sensitive;                /**< search is case-sensitive         */
  int  match_exact;                   /**< search requires exact match      */
  char station_call[TRACK_CALL_LEN];  /**< callsign being tracked           */

  /* --- findu trail download --- */
  char download_call[TRACK_CALL_LEN]; /**< callsign for trail download      */
  int  posit_start;                   /**< hours ago to start trail         */
  int  posit_length;                  /**< trail duration in hours          */
  int  fetching_now;                  /**< download thread currently active */
} track_controller_t;


/* ------------------------------------------------------------------ */
/* Lifecycle                                                           */
/* ------------------------------------------------------------------ */

/**
 * Initialise all fields to safe defaults.
 * posit_start and posit_length are set to MAX_FINDU_DURATION.
 * Safe to call with NULL (no-op).
 */
void track_controller_init(track_controller_t *tc);


/* ------------------------------------------------------------------ */
/* Station tracking                                                    */
/* ------------------------------------------------------------------ */

/**
 * Validate and set the station to track.
 *
 * @param tc        Controller instance.
 * @param raw_call  Callsign as entered by the user (may have trailing
 *                  spaces or a "-0" SSID suffix).
 *
 * The function:
 *  1. Trims trailing whitespace and the "-0" suffix from raw_call.
 *  2. Calls valid_object() || valid_call() || valid_item() on the result.
 *  3. If valid: stores trimmed call in tc->station_call, sets
 *     tc->station_on = 1, returns 1.
 *  4. If invalid: clears tc->station_call, sets tc->station_on = 0,
 *     returns 0.
 *
 * Safe to call with NULL tc or NULL raw_call (returns 0, no crash).
 */
int track_controller_set_station(track_controller_t *tc,
                                 const char         *raw_call);

/**
 * Clear the tracking target.
 * Sets tc->station_on = 0 and tc->station_call[0] = '\\0'.
 * Safe to call with NULL (no-op).
 */
void track_controller_clear_station(track_controller_t *tc);


/* ------------------------------------------------------------------ */
/* Findu download helpers                                              */
/* ------------------------------------------------------------------ */

/**
 * Clamp tc->posit_length to be consistent with tc->posit_start.
 *
 *  - If posit_start > MAX_FINDU_DURATION: posit_length = MAX_FINDU_DURATION.
 *  - Otherwise:                           posit_length = posit_start.
 *
 * Mirrors the Reset_posit_length_max() callback logic.
 * Safe to call with NULL (no-op).
 */
void track_controller_clamp_posit_length(track_controller_t *tc);

/**
 * Return 1 if it is safe to start a new findu download, 0 otherwise.
 *
 * Conditions that block a download:
 *  - tc->fetching_now is non-zero (download thread already running).
 *  - read_file_busy is non-zero (another file is being read in).
 *
 * Safe to call with NULL tc (returns 0).
 */
int track_controller_can_start_download(const track_controller_t *tc,
                                        int read_file_busy);

/**
 * Build the findu raw-packet download URL.
 *
 * Uses tc->download_call, tc->posit_start, tc->posit_length.
 *
 * @param tc     Controller instance (must not be NULL).
 * @param out    Output buffer.
 * @param outsz  Size of output buffer (must be > 0).
 *
 * Returns 0 on success, -1 if any argument is NULL / outsz is 0 /
 * download_call is empty.
 */
int track_controller_build_findu_url(const track_controller_t *tc,
                                     char                     *out,
                                     size_t                    outsz);


#endif /* __TRACK_CONTROLLER_H */
