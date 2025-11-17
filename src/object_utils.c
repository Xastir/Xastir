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
// drags in, and so we can mock them up in unit testing
time_t sec_now(void);
char *compress_posit(const char *input_lat, const char group,
                     const char *input_lon, const char symbol,
                     const unsigned int last_course,
                     const unsigned int last_speed, const char *phg);
char compress_group(char group_in);

// Pad an item name out to at least 3 characters
// Direct cut and paste from Create_object_item_tx_string.
// Replaces given name with padded name.
void pad_item_name(char *name, size_t name_size)
{
  char tempstr[10]; // max name is 9 characters
  xastir_snprintf(tempstr,
                  sizeof(tempstr),
                  "%s",
                  name);
  if (strlen(tempstr) < 3)
    xastir_snprintf(name, name_size,"%-3s",tempstr);
}

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
        xastir_snprintf(dst, dst_size, "/A=%06ld",alt_in_meters);
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
  unsigned int color_int;
  // We are going to be passed a color /0 through /7.  decode it.
  color_int = area_color_from_string(color);

  // If it's bright, just use it.  If it's low-intensity we need to add 8
  if (!bright)
    color_int += 8;

  // now encode it
  format_area_color_from_numeric(dst, dst_size, color_int);
}

// Area objects of the linear type may have a corridor width, which
// is transmitted as "{xxx}".
//
// The corridor width is stored in the DataRow as a 16-bit bitfield,
// and so has a maximum value of 65535, too big to fit in the 6-byte
// buffer that Create_object_item_tx_string reserves for the formatted
// string.  So really we mustmake sure that the corridor is less than
// 1000 (miles) before formatting it.  Otherwise we'd create a
// truncated string and a malformed corridor.  Fortunately, Xastir's
// object creation dialog doesn't let us enter more than three digits,
// so there has never been a problem.

void format_area_corridor(char *dst, size_t dst_size, unsigned int type, unsigned int width)
{
  dst[0] = '\0';
  if ( (type == 1) || (type == 6))
  {
    if (width > 0 && width < 1000)
    {
      xastir_snprintf(dst, dst_size, "{%d}", width);
    }
  }
}

// Signpost objects can have a 3 character string attached.
// The resulting packet is supposed to get that string inserted as "{xxx}",
// just as for area object corridors.  If we are given too many characters,
// just ignore the whole thing.

void format_signpost(char *dst, size_t dst_size, char *signpost)
{
  dst[0]='\0';
  if (strlen(signpost) > 0 && strlen(signpost)<=3)
  {
    xastir_snprintf(dst, dst_size, "{%s}", signpost);
  }
}

// If we are a probability ring object we have pmin and/or pmax data and
// should prepend the data to the object comment.
//
void format_probability_ring_data(char *dst, size_t dst_size, char *pmin,
                                  char *pmax)
{
  char comment2[43+1];

  if ( (pmin[0] != '\0')
       || (pmax[0] != '\0') )
  {
    strncpy(comment2,dst,sizeof(comment2)-1);
    comment2[sizeof(comment2)-1] = '\0';  // assure that we're null terminated

    if (pmax[0] == '\0')
    {
      xastir_snprintf(dst, dst_size, "Pmin%s,%s",pmin,comment2);
    }
    else if (pmin[0] == '\0')
    {
      xastir_snprintf(dst, dst_size, "Pmax%s,%s",pmax,comment2);
    }
    else    // Have both
    {
      xastir_snprintf(dst, dst_size, "Pmin%s,Pmax%s,%s",pmin,pmax,comment2);
    }
  }
}

// While Xastir doesn't actually allow you to enter PHG and RNG for
// objects (?) it does check to see if a station record for an object
// has such data and tries to insert it.  Perhaps this is possible only
// when Xastir adopts an object created elsewhere, and that other station
// transmitted the object with PHG?
//
// If it's there, this data needs to be prepended to the comment
// string.  We are given the comment string in dst.

void prepend_rng_phg(char *dst, size_t dst_size, char *power_gain)
{
  char comment2[43+1];
  xastir_snprintf(comment2,sizeof(comment2),"%s%s",power_gain,dst);
  strncpy(dst,comment2,dst_size-1);
}

// given a mess of data, format into an area object or item packet,
// compressed or otherwise.
// If compressed, lat_str and lon_str must be in high precision, we do not
// check that here --- do it in the caller
void format_area_object_item_packet(char *dst, size_t dst_size,
                                    char *name, char object_group,
                                    char object_symbol, char *time, char *lat_str,
                                    char *lon_str, int area_type,
                                    char *area_color,
                                    int lat_offset, int lon_offset,
                                    char *speed_course, char *corridor,
                                    char *altitude, int course, int speed,
                                    int is_object, int compressed)
{
  if (is_object)     // It's an object
  {

    if (compressed)
    {

      xastir_snprintf(dst, dst_size, ";%-9s*%s%s%1d%02d%2s%02d%s%s%s",
                      name,
                      time,
                      compress_posit(lat_str,
                                     object_group,
                                     lon_str,
                                     object_symbol,
                                     course,
                                     speed,  // In knots
                                     ""),    // PHG, must be blank
                      area_type,
                      lat_offset,
                      area_color,
                      lon_offset,
                      speed_course,
                      corridor,
                      altitude);

    }
    else    // Non-compressed posit object
    {

      xastir_snprintf(dst, dst_size, ";%-9s*%s%s%c%s%c%1d%02d%2s%02d%s%s%s",
                      name,
                      time,
                      lat_str,
                      object_group,
                      lon_str,
                      object_symbol,
                      area_type,
                      lat_offset,
                      area_color,
                      lon_offset,
                      speed_course,
                      corridor,
                      altitude);
    }
  }
  else        // It's an item
  {

    if (compressed)
    {

      xastir_snprintf(dst, dst_size, ")%s!%s%1d%02d%2s%02d%s%s%s",
                      name,
                      compress_posit(lat_str,
                                     object_group,
                                     lon_str,
                                     object_symbol,
                                     course,
                                     speed,  // In knots
                                     ""),    // PHG, must be blank
                      area_type,
                      lat_offset,
                      area_color,
                      lon_offset,
                      speed_course,
                      corridor,
                      altitude);

    }
    else    // Non-compressed item
    {

      xastir_snprintf(dst, dst_size, ")%s!%s%c%s%c%1d%02d%2s%02d%s%s%s",
                      name,
                      lat_str,
                      object_group,
                      lon_str,
                      object_symbol,
                      area_type,
                      lat_offset,
                      area_color,
                      lon_offset,
                      speed_course,
                      corridor,
                      altitude);
    }
  }
}


// Format a signpost object or item packet given all the pre-formatted
// bits and bobs

void format_signpost_object_item_packet(char *dst, size_t dst_size,
                                        char *name, char object_group,
                                        char object_symbol, char *time,
                                        char * lat_str, char *lon_str,
                                        char *speed_course,
                                        char *altitude,
                                        char *signpost,
                                        int course, int speed,
                                        int is_object, int compressed)
{
  if (is_object)     // It's an object
  {

    if (compressed)
    {

      xastir_snprintf(dst, dst_size, ";%-9s*%s%s%s%s",
                      name,
                      time,
                      compress_posit(lat_str,
                                     object_group,
                                     lon_str,
                                     object_symbol,
                                     course,
                                     speed,  // In knots
                                     ""),    // PHG, must be blank
                      altitude,
                      signpost);
    }
    else    // Non-compressed posit object
    {

      xastir_snprintf(dst, dst_size, ";%-9s*%s%s%c%s%c%s%s%s",
                      name,
                      time,
                      lat_str,
                      object_group,
                      lon_str,
                      object_symbol,
                      speed_course,
                      altitude,
                      signpost);
    }
  }
  else    // It's an item
  {

    if (compressed)
    {

      xastir_snprintf(dst, dst_size, ")%s!%s%s%s",
                      name,
                      compress_posit(lat_str,
                                     object_group,
                                     lon_str,
                                     object_symbol,
                                     course,
                                     speed,  // In knots
                                     ""),    // PHG, must be blank
                      altitude,
                      signpost);
    }
    else    // Non-compressed item
    {

      xastir_snprintf(dst, dst_size, ")%s!%s%c%s%c%s%s%s",
                      name,
                      lat_str,
                      object_group,
                      lon_str,
                      object_symbol,
                      speed_course,
                      altitude,
                      signpost);
    }
  }

}
