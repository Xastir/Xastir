/*
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 2025 The Xastir Group
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
 * Stub implementations for symbols referenced by util.o
 * but not used by the unit tests.
 * 
 * These stubs allow us to link with the real util.o for testing
 * without pulling in the entire Xastir codebase.
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "globals.h"
#include "database.h"
#include "tests/test_framework.h"

// global variables referenced but unused in tests so far:
int transmit_compressed_objects_items=0;
int object_tx_disable=0;
int transmit_disable=0;
int debug_level=0;
long scale_x, scale_y;
long center_longitude, center_latitude;
char dangerous_operation[200];
char my_long[MAX_LONG], my_lat[MAX_LAT];
long screen_height, screen_width;
char my_callsign[MAX_CALLSIGN+1];

DataRow *n_first=NULL;
time_t last_object_check=0l;
time_t OBJECT_rate=0l;

STUB_IMPL(compute_current_DR_position);
STUB_IMPL(output_my_data);
STUB_IMPL(langcode);
STUB_IMPL(is_my_object_item);
STUB_IMPL(is_my_call);
STUB_IMPL(get_user_base_dir);
STUB_IMPL(statusline);
STUB_IMPL(move_station_time);
STUB_IMPL(ll_to_utm_ups);
STUB_IMPL(utm_ups_to_ll);
STUB_IMPL(search_station_name);
