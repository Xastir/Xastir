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
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include "tests/test_framework.h"

// Not actually a stub, a fake implementation of sec_now()
time_t sec_now(void)
{
  // Unix timestamp for 2025-11-11 16:18:00 UTC
  return( (time_t) 1762877880);
}

// Complete implementation of compress_posit, cut/pasted from util.c, coz
// I need to test with more than fake data, and utils.c isn't ready to be
// used in unit tests
char compress_group(char group_in)
{
  char group_out=group_in;
  if (group_in >= '0' && group_in <= '9')
  {
    group_out=group_in - '0' + 'a';
  }
  return (group_out);
}

char *compress_posit(const char *input_lat, const char group, const char *input_lon, const char symbol,
                     const unsigned int last_course, const unsigned int last_speed, const char *phg)
{
  static char pos[100];
  char lat[5], lon[5];
  char c, s, t, ext;
  int temp, deg;
  double minutes;
  char temp_str[20];


//fprintf(stderr,"lat:%s, long:%s, symbol:%c%c, course:%d, speed:%d, phg:%s\n",
//    input_lat,
//    input_lon,
//    group,
//    symbol,
//    last_course,
//    last_speed,
//    phg);

  // Fetch degrees (first two chars)
  temp_str[0] = input_lat[0];
  temp_str[1] = input_lat[1];
  temp_str[2] = '\0';
  deg = atoi(temp_str);

  // Fetch minutes (rest of numbers)
  snprintf(temp_str,
                  sizeof(temp_str),
                  "%s",
                  input_lat);
  temp_str[0] = ' ';  // Blank out degrees
  temp_str[1] = ' ';  // Blank out degrees
  minutes = atof(temp_str);

  // Check for North latitude
  if (strstr(input_lat, "N") || strstr(input_lat, "n"))
  {
    ext = 'N';
  }
  else
  {
    ext = 'S';
  }

//fprintf(stderr,"ext:%c\n", ext);

  temp = 380926 * (90 - (deg + minutes / 60.0) * ( ext=='N' ? 1 : -1 ));

//fprintf(stderr,"temp: %d\t",temp);

  lat[3] = (char)(temp%91 + 33);
  temp /= 91;
  lat[2] = (char)(temp%91 + 33);
  temp /= 91;
  lat[1] = (char)(temp%91 + 33);
  temp /= 91;
  lat[0] = (char)(temp    + 33);
  lat[4] = '\0';

//fprintf(stderr,"%s\n",lat);

  // Fetch degrees (first three chars)
  temp_str[0] = input_lon[0];
  temp_str[1] = input_lon[1];
  temp_str[2] = input_lon[2];
  temp_str[3] = '\0';
  deg = atoi(temp_str);

  // Fetch minutes (rest of numbers)
  snprintf(temp_str,
                  sizeof(temp_str),
                  "%s",
                  input_lon);
  temp_str[0] = ' ';  // Blank out degrees
  temp_str[1] = ' ';  // Blank out degrees
  temp_str[2] = ' ';  // Blank out degrees
  minutes = atof(temp_str);

  // Check for West longitude
  if (strstr(input_lon, "W") || strstr(input_lon, "w"))
  {
    ext = 'W';
  }
  else
  {
    ext = 'E';
  }

//fprintf(stderr,"ext:%c\n", ext);

  temp = 190463 * (180 + (deg + minutes / 60.0) * ( ext=='W' ? -1 : 1 ));

//fprintf(stderr,"temp: %d\t",temp);

  lon[3] = (char)(temp%91 + 33);
  temp /= 91;
  lon[2] = (char)(temp%91 + 33);
  temp /= 91;
  lon[1] = (char)(temp%91 + 33);
  temp /= 91;
  lon[0] = (char)(temp    + 33);
  lon[4] = '\0';

//fprintf(stderr,"%s\n",lon);

  // Set up csT bytes for course/speed if either are non-zero
  c = s = t = ' ';
  if (last_course > 0 || last_speed > 0)
  {

    if (last_course >= 360)
    {
      c = '!';  // 360 would be past 'z'.  Set it to zero.
    }
    else
    {
      c = (char)(last_course/4 + 33);
    }

    s = (char)(log(last_speed + 1.0) / log(1.08) + 33.5);  // Poor man's rounding + ASCII
    t = 'C';
  }
  // Else set up csT bytes for PHG if within parameters
  else if (strlen(phg) >= 6)
  {
    double power, height, gain, range, s_temp;


    c = '{';

    if ( (phg[3] < '0') || (phg[3] > '9') )   // Power is out of limits
    {
      power = 0.0;
    }
    else
    {
      power = (double)((int)(phg[3]-'0'));
      power = power * power;  // Lowest possible value is 0.0
    }

    if (phg[4] < '0') // Height is out of limits (no upper limit according to the spec)
    {
      height = 10.0;
    }
    else
    {
      height= 10.0 * pow(2.0,(double)phg[4] - (double)'0');  // Lowest possible value is 10.0
    }

    if ( (phg[5] < '0') || (phg[5] > '9') ) // Gain is out of limits
    {
      gain = 1.0;
    }
    else
    {
      gain = pow(10.0,((double)(phg[5]-'0') / 10.0));  // Lowest possible value is 1.0
    }

    range = sqrt(2.0 * height * sqrt((power / 10.0) * (gain / 2.0)));   // Lowest possible value is 0.0

    // Check for range of 0, and skip log10 if so
    if (range != 0.0)
    {
      s_temp = log10(range/2) / log10(1.08) + 33.0;
    }
    else
    {
      s_temp = 0.0 + 33.0;
    }

    s = (char)(s_temp + 0.5);    // Cheater's way of rounding, add 0.5 and truncate

    t = 'C';
  }
  // Note that we can end up with three spaces at the end if no
  // course/speed/phg were supplied.  Do not knock this down, as
  // the compressed posit has a fixed 13-character length
  // according to the spec!
  //
  snprintf(pos,
                  sizeof(pos),
                  "%c%s%s%c%c%c%c",
                  compress_group(group),
                  lat,
                  lon,
                  symbol,
                  c,
                  s,
                  t);

//fprintf(stderr,"New compressed pos: (%s)\n",pos);
  return pos;
}


