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
 * Test program for object_utils.c functions
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>

#include "tests/test_framework.h"

#include "util.h"
#include "globals.h"

int test_convert_lat_l2s_basic(void)
{
  long lat;
  char lat_str[20];
  // Compute Xastir coordinates for 35d01.631'N
  // Xastir coordinates are in hundredths of seconds, with 0 being 90d N
  lat = 90*60*60*100-(35*60+1.631)*60*100;
  convert_lat_l2s(lat,lat_str, sizeof(lat_str),CONVERT_HP_NOSP);
  TEST_ASSERT_STR_EQ("3501.631N",lat_str,"xastir y value correctly converted to string");
  TEST_PASS("convert_lat_l2s: correct");
}
int test_convert_lon_l2s_basic(void)
{
  long lon;
  char lon_str[20];
  // Compute Xastir coordinates for 106d12.385'W
  // Xastir coordinates are in hundredths of seconds, with 0 being 90d N
  lon = 180*60*60*100-(106*60+12.385)*60*100;
  convert_lon_l2s(lon,lon_str, sizeof(lon_str),CONVERT_HP_NOSP);
  TEST_ASSERT_STR_EQ("10612.385W",lon_str,"xastir x value correctly converted to string");
  TEST_PASS("convert_lon_l2s: correct");
}
int test_convert_lat_l2s_lp(void)
{
  long lat;
  char lat_str[20];
  // Compute Xastir coordinates for 35d01.631'N
  // Xastir coordinates are in hundredths of seconds, with 0 being 90d N
  lat = 90*60*60*100-(35*60+1.631)*60*100;
  convert_lat_l2s(lat,lat_str, sizeof(lat_str),CONVERT_LP_NOSP);
  TEST_ASSERT_STR_EQ("3501.63N",lat_str,"xastir y value correctly converted to string");
  TEST_PASS("convert_lat_l2s: correct");
}
int test_convert_lon_l2s_lp(void)
{
  long lon;
  char lon_str[20];
  // Compute Xastir coordinates for 106d12.384'W
  // Xastir coordinates are in hundredths of seconds, with 0 being 90d N
  // we're using ".384" here because ".385" would get rounded up, not truncated.
  lon = 180*60*60*100-(106*60+12.384)*60*100;
  convert_lon_l2s(lon,lon_str, sizeof(lon_str),CONVERT_LP_NOSP);
  TEST_ASSERT_STR_EQ("10612.38W",lon_str,"xastir x value correctly converted to string");
  TEST_PASS("convert_lon_l2s: correct");
}
int test_convert_lat_s2l_basic(void)
{
  long lat;
  long lat_expect;
  // Compute Xastir coordinates for 35d01.631'N
  // Xastir coordinates are in hundredths of seconds, with 0 being 90d N
  lat_expect = 90*60*60*100-(35*60+1.631)*60*100;
  lat = convert_lat_s2l("3501.631N");
  TEST_ASSERT(lat==lat_expect,"xastir y value correctly converted from string");
  TEST_PASS("convert_lat_s2l: correct");
}
int test_convert_lon_s2l_basic(void)
{
  long lon;
  long lon_expect;
  // Compute Xastir coordinates for 106d12.385'W
  // Xastir coordinates are in hundredths of seconds, with 0 being 90d N
  lon_expect = 180*60*60*100-(106*60+12.385)*60*100;
  lon = convert_lon_s2l("10612.385W");
  TEST_ASSERT(lon==lon_expect,"xastir x value correctly converted from string");
  TEST_PASS("convert_lon_s2l: correct");
}

/* Test runner */
typedef struct {
    const char *name;
    int (*func)(void);
} test_case_t;

int main(int argc, char *argv[])
{
  test_case_t tests[] = {
    {"convert_lat_l2s_basic",test_convert_lat_l2s_basic},
    {"convert_lon_l2s_basic",test_convert_lon_l2s_basic},
    {"convert_lat_l2s_lp",test_convert_lat_l2s_lp},
    {"convert_lon_l2s_lp",test_convert_lon_l2s_lp},
    {"convert_lat_s2l_basic",test_convert_lat_s2l_basic},
    {"convert_lon_s2l_basic",test_convert_lon_s2l_basic},
    {NULL,NULL}
  };


  if (argc < 2)
  {
    fprintf(stderr, "Usage: %s <test name>\n", argv[0]);
    fprintf(stderr, "Available tests: \n");
    for (int i = 0; tests[i].name != NULL; i++)
    {
      fprintf(stderr, "  %s\n", tests[i].name);
    }
    return 1;
  }

  const char *test_name = argv[1];

  /* Run the requested test */
  for (int i = 0; tests[i].name != NULL; i++)
  {
    if (strcmp(test_name, tests[i].name) == 0)
    {
      return tests[i].func();
    }
  }

  fprintf(stderr, "Unknown test: %s\n", test_name);
  return 1;
}


