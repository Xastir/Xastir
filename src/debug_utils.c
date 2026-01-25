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


#include <stdio.h>

#include "xastir.h"
#include "main.h"


// Prints string to STDERR only if "my_debug_level" bits are set in
// the global "debug_level" variable.  Used for getting extra debug
// messages during various stages of debugging.
//
// As far as I can tell, this was defined in utils.c but never, ever called
// anywhere in Xastir.
void xastir_debug(int my_debug_level, char *debug_string)
{

  if (debug_level & my_debug_level)
  {
    fprintf(stderr, "%s", debug_string);
  }
}
