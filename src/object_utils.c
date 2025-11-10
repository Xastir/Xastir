/*
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2023 The Xastir Group
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
#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif  // HAVE_CONFIG_H

#include "snprintf.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <string.h>

#include "object_utils.h"

// Given strings representing course and speed, return an appropriate
// CSE/SPD string for transmitting of object in the dst array, and integer
// representations of course and speed
void format_course_speed(char *dst, size_t dst_size, char *course_str, char *speed_str, int *course, int *speed)
{
  char tempstr[50];
  int temp;

  xastir_snprintf(dst, dst_size, ".../"); // Start with invalid-data string
  *course = 0;
  if (strlen(course_str) != 0)      // Course was entered
  {
    // Need to check for 1 to three digits only, and 001-360
    // degrees)
    temp = atoi(course_str);
    if ( (temp >= 1) && (temp <= 360) )
    {
      xastir_snprintf(dst, dst_size, "%03d/",temp);
      *course = temp;
    }
    else if (temp == 0)     // Spec says 001 to 360 degrees...
    {
      xastir_snprintf(dst, dst_size, "360/");
    }
  }
  *speed = 0;
  if (strlen(speed_str) != 0)   // Speed was entered (we only handle knots currently)
  {
    // Need to check for 1 to three digits, no alpha characters
    temp = atoi(speed_str);
    if ( (temp >= 0) && (temp <= 999) )
    {
      xastir_snprintf(tempstr, sizeof(tempstr), "%03d",temp);
      strncat(dst,
              tempstr,
              dst_size - 1 - strlen(dst));
      *speed = temp;
    }
    else
    {
      strncat(dst,
              "...",
              dst_size - 1 - strlen(dst));
    }
  }
  else    // No speed entered, blank it out
  {
    strncat(dst,
            "...",
            dst_size - 1 - strlen(dst));
  }
  if ( (dst[0] == '.') && (dst[4] == '.') )
  {
    dst[0] = '\0'; // No speed or course entered, so blank it
  }
}
