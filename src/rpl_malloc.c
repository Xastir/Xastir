/*
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2019 The Xastir Group
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
//
// We DON'T want config.h to redefine malloc in this file else we'll
// get an infinite loop.
//
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif  // HAVE_CONFIG_H
*/

#include <stdlib.h>

#include "rpl_malloc.h"

//
// Work around bug on some systems where malloc (0) fails.
// written by Jim Meyering
//
// configure.ac calls out AC_FUNC_MALLOC which checks the malloc()
// function.  If malloc() is determined to do the wrong thing when
// passed a 0 value, the Autoconf macro will do this:
//      #define malloc rpl_malloc
// We then need to have an rpl_malloc function defined.  Here it is:
//
// Allocate an N-byte block of memory from the heap.
// If N is zero, allocate a 1-byte block.
//
void *rpl_malloc (size_t size)
{
  if (size == 0)
  {
    size++;
  }
  return malloc (size);
}


