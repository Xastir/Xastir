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
 * Stub implementations for symbols referenced by track_controller.c
 * that are not exercised by the unit tests.
 *
 * valid_object / valid_call / valid_item are given controllable return
 * values so that tests can simulate any combination of validation outcomes.
 */

#include <stdio.h>
#include <stdlib.h>

/* ------------------------------------------------------------------ */
/* Controllable return values — set by each test case before calling  */
/* track_controller_set_station().                                      */
/* ------------------------------------------------------------------ */

int stub_valid_object_result = 0;
int stub_valid_call_result   = 0;
int stub_valid_item_result   = 0;

/* ------------------------------------------------------------------ */
/* Stub implementations                                                */
/* ------------------------------------------------------------------ */

int valid_object(char *name)
{
  (void)name;
  return stub_valid_object_result;
}

int valid_call(char *call)
{
  (void)call;
  return stub_valid_call_result;
}

int valid_item(char *name)
{
  (void)name;
  return stub_valid_item_result;
}
