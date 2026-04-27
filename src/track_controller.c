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
 * No Motif, no X11 — depends only on the three call-validation predicates
 * (valid_object / valid_call / valid_item) which are forward-declared here
 * so that tests can supply lightweight stubs.
 */

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "track_controller.h"

/* ------------------------------------------------------------------ */
/* Forward declarations for the three call-validation predicates.      */
/* Defined in objects.c / util.c in the main build; provided as       */
/* lightweight stubs in tests/test_track_controller_stubs.c.           */
/* ------------------------------------------------------------------ */
extern int valid_object(char *name);
extern int valid_call(char *call);
extern int valid_item(char *name);


/* ------------------------------------------------------------------ */
/* Private helpers                                                     */
/* ------------------------------------------------------------------ */

/** Strip trailing ASCII whitespace from s in place. */
static void strip_trailing_spaces(char *s)
{
  size_t len;

  if (!s)
  {
    return;
  }
  len = strlen(s);
  while (len > 0 && isspace((unsigned char)s[len - 1]))
  {
    s[--len] = '\0';
  }
}

/**
 * Strip the SSID suffix "-0" from s in place.
 * Only removes the last two characters when s ends exactly with "-0".
 */
static void strip_trailing_dash_zero(char *s)
{
  size_t len;

  if (!s)
  {
    return;
  }
  len = strlen(s);
  if (len >= 2 && s[len - 2] == '-' && s[len - 1] == '0')
  {
    s[len - 2] = '\0';
  }
}


/* ------------------------------------------------------------------ */
/* Lifecycle                                                           */
/* ------------------------------------------------------------------ */

void track_controller_init(track_controller_t *tc)
{
  if (!tc)
  {
    return;
  }
  memset(tc, 0, sizeof(*tc));
  tc->posit_start  = MAX_FINDU_DURATION;
  tc->posit_length = MAX_FINDU_DURATION;
}


/* ------------------------------------------------------------------ */
/* Station tracking                                                    */
/* ------------------------------------------------------------------ */

int track_controller_set_station(track_controller_t *tc,
                                 const char         *raw_call)
{
  char trimmed[TRACK_CALL_LEN];

  if (!tc || !raw_call)
  {
    return 0;
  }

  /* Copy and trim */
  snprintf(trimmed, sizeof(trimmed), "%s", raw_call);
  strip_trailing_spaces(trimmed);
  strip_trailing_dash_zero(trimmed);

  /* Validate: name of object, amateur call, or item designator */
  if (valid_object(trimmed) || valid_call(trimmed) || valid_item(trimmed))
  {
    snprintf(tc->station_call, sizeof(tc->station_call), "%s", trimmed);
    tc->station_on = 1;
    return 1;
  }

  /* Invalid: clear state */
  tc->station_call[0] = '\0';
  tc->station_on      = 0;
  return 0;
}


void track_controller_clear_station(track_controller_t *tc)
{
  if (!tc)
  {
    return;
  }
  tc->station_on      = 0;
  tc->station_call[0] = '\0';
}


/* ------------------------------------------------------------------ */
/* Findu download helpers                                              */
/* ------------------------------------------------------------------ */

void track_controller_clamp_posit_length(track_controller_t *tc)
{
  if (!tc)
  {
    return;
  }

  if (tc->posit_start > MAX_FINDU_DURATION)
  {
    /* Start time exceeds findu's max duration — cap the length too */
    tc->posit_length = MAX_FINDU_DURATION;
  }
  else
  {
    /* Duration cannot exceed start offset (can't download more than available) */
    tc->posit_length = tc->posit_start;
  }
}


int track_controller_can_start_download(const track_controller_t *tc,
                                        int read_file_busy)
{
  if (!tc)
  {
    return 0;
  }
  if (tc->fetching_now)
  {
    return 0;
  }
  if (read_file_busy)
  {
    return 0;
  }
  return 1;
}


int track_controller_build_findu_url(const track_controller_t *tc,
                                     char                     *out,
                                     size_t                    outsz)
{
  if (!tc || !out || outsz == 0)
  {
    return -1;
  }
  if (tc->download_call[0] == '\0')
  {
    return -1;
  }

  snprintf(out, outsz, FINDU_URL_FMT,
           tc->download_call,
           tc->posit_start,
           tc->posit_length);
  return 0;
}
