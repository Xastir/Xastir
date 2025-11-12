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
#include <time.h>
#include <string.h>

#include "object_utils.h"

// forward declaration so we don't need to include util.h and all that
// drags in
time_t sec_now(void);

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

// Given a string representation of altitude, return the correctly
// formatted string for transmission of that altitude as part of an object
// packet.  Returns an empty string if the altitude is out of range or empty.
//
// At the moment (and for at least 20 years prior to the moment) this code
// only handles input altitudes in feet.  These altitudes are converted to
// meters for purposes of transmission.
void format_altitude(char *dst, size_t dst_size, char *altitude_str)
{
  long alt_in_meters;
  dst[0] = '\0'; // Start with empty string
  if (strlen(altitude_str) != 0)     // Altitude was entered (we only handle feet currently)
  {
    // Need to check for all digits, and 1 to 6 digits
    if (isdigit((int)altitude_str[0]))
    {
      // Must convert from meters to feet before transmitting
      alt_in_meters = (int)( (atof(altitude_str) / 0.3048) + 0.5);
      if ( (alt_in_meters >= 0) && (alt_in_meters <= 99999l) )
      {
        char temp_alt[20];
        xastir_snprintf(temp_alt, sizeof(temp_alt), "/A=%06ld",alt_in_meters);
        memcpy(dst, temp_alt, dst_size - 1);
        dst[dst_size-1] = '\0';  // Terminate string
      }
    }
  }
}

// Return a string suitable for placing the current zulu time into an APRS
// packet
void format_zulu_time(char *dst, size_t dst_size)
{
  struct tm *day_time;
  time_t sec;
  sec = sec_now();
  day_time = gmtime(&sec);
  xastir_snprintf(dst,
                  dst_size,
                  "%02d%02d%02dz",
                  day_time->tm_mday,
                  day_time->tm_hour,
                  day_time->tm_min);
}

// APRS Area objects are transmitted with colors from /0 through /9 and 10
// through 15.  /0-/7 are "high intensity" colors and /8 through 15 are low
// intensity values of the same.   They are stored in the DataRow as an unsigned
// four-bit bit field.

// This function formats a color as stored numerically into the format needed
// for transmit

// Because the DataRow only stores this value in a 4-bit bit unsigned bit
// field, we'll never get an invalid color, but guard against misuse just in
// case.
void format_area_color_from_numeric(char * dst, size_t dst_size, unsigned int color)
{
  dst[0] = '\0';  // start with null string

  // Because the DataRow only stores this value in a 4-bit bit unsigned bit
  // field, we'll never get an invalid color, but guard against misuse just in
  // case.
  if (color <= 15)
  {
    xastir_snprintf(dst,dst_size,"%02d", color);
    if ( dst[0] == '0')
      dst[0]='/';
  }
}

// When decoding a color from a packet, we need to convert it to a number
// This function takes such a string and returns the appropriate value
// (it is currently unused by any object code, but db.c does some goofy stuff
// to decode a received area object's color in extract_area, and this
// function might just come in useful later in a refactor)
unsigned int area_color_from_string(char *color_string)
{
  unsigned int return_color=0;
  // we might be getting passed a pointer to the middle of an APRS packet
  // that might not end at the color's end, so let's just make sure we
  // have enough characters to read and not make assumptions that we're
  // ONLY getting the color.
  // Valid colors are /0-/9 and 10-15.  If invalid, just return 0.
  if (strlen(color_string) >= 2
      && ((color_string[0] == '/' && isdigit((int)color_string[1]))
          || (color_string[0] == '1'
              &&  color_string[1] >= '0'
              && color_string[1] <= '5')))

  {
    if (color_string[0] == '/')
    {
      return_color = color_string[1] - '0';
    }
    else  // first character must be a one if we get here
    {
      return_color = color_string[1] - '0' + 10;
    }
  }
  return (return_color);
}

// When we *create* an object from the dialog box, the color is
// determined from a combination of the selected color and the
// selected (or not-selected) "Bright color" button.  The dialog
// provides '/0' through '/7' for the color and an integer
// representing the button state (0 is deselected)
//
// This function takes values that came from the dialog box and formats
// it into an appropriate two-character color string that combines the
// two bits of information

void format_area_color_from_dialog(char *dst, size_t dst_size, char *color, int bright)
{
  // This initial implementation just does exactly what
  // Setup_object_data does in all its glory.  It is ripe for
  // rewriting after we have it in unit testing

  // Note that we are ASSUMING that "color" has at least 2 elements.  Not cool.

  // note also the lack of any sanity checking of inputs, because the function
  // assumes valid inputs from the dialog box because the user must choose
  // from radio buttons and on/off buttons, not freeform values.

  if (bright)
  {
    // if it's bright color, it's /0 - /7 directly from the dialog, so just
    // use it.
    xastir_snprintf(dst, dst_size, "%2s", color);
  }
  else  // Dim color
  {
    // if it's dim color, it needs to be /8, /9, or 10-15.  Add 8 to the
    // given color's value (which is always 0-7), stuff it in the output,
    // and then change the first character to '/' if the original was 0 or 1
    // (i.e. the new value is 8 or 9).
    xastir_snprintf(dst, dst_size, "%02.0f",
                      (float)(atoi(&color[1]) + 8) );

      if ( (color[1] == '0') || (color[1] == '1') )
      {
        dst[0] = '/';
      }
  }
}
