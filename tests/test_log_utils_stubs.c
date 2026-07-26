/*
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 2025-2026 The Xastir Group
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
 * Stub implementations for symbols referenced by log_utils.o and
 * util.o but not exercised by the unit tests.
 *
 * These stubs allow us to link with the real log_utils.o and util.o
 * for testing without pulling in the entire Xastir codebase.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include "globals.h"
#include "tests/test_framework.h"

STUB_IMPL(langcode);
STUB_IMPL(ll_to_utm_ups);
STUB_IMPL(utm_ups_to_ll);
STUB_IMPL(search_station_name);
STUB_IMPL(fill_in_new_alert_entries);

// The real decode_ax25_line() tokenizes its "line" argument in
// place with strtok(), truncating it at the first '>'. Mimic that
// destructive behavior here so tests catch callers who read "line"
// again afterward expecting it to still be intact.
int decode_ax25_line(char *line, char from, int port, int dbadd)
{
  (void)from;
  (void)port;
  (void)dbadd;
  (void)strtok(line, ">");
  return(1);
}

char *get_user_base_dir(char *dir, char *dest, size_t dest_size)
{
  (void)dir;
  if (dest != NULL && dest_size > 0)
  {
    dest[0] = '\0';
  }
  return(dest);
}

// global variables referenced but unused:
int debug_level=0;
long scale_x, scale_y;
long center_longitude, center_latitude;
long NW_corner_longitude, NW_corner_latitude;
long SE_corner_longitude, SE_corner_latitude;
char dangerous_operation[200];
char my_long[MAX_LONG], my_lat[MAX_LAT];
long screen_height, screen_width;
char LOGFILE_WX_ALERT[400];
uid_t euid;
gid_t egid;
