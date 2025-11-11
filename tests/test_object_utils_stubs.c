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
 * Stub implementations for symbols referenced by object_utils.c
 * but not used by the unit tests.
 * 
 * These stubs allow us to link with the real object_utils.o for testing
 * without pulling in the entire Xastir codebase.
 */

#include <time.h>
#include "tests/test_framework.h"

// Not actually a stub, a fake implementation of sec_now()
time_t sec_now(void)
{
  // Unix timestamp for 2025-11-11 16:18:00 UTC
  return( (time_t) 1762877880);
}
