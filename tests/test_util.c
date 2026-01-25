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

extern long scale_x, scale_y;
extern long center_longitude, center_latitude;
extern long NW_corner_longitude, NW_corner_latitude;
extern long SE_corner_longitude, SE_corner_latitude;
extern long screen_height, screen_width;

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
int test_convert_lat_l2s_basic_s(void)
{
  long lat;
  char lat_str[20];
  // Compute Xastir coordinates for 35d01.631'S
  // Xastir coordinates are in hundredths of seconds, with 0 being 90d N
  lat = 90*60*60*100+(35*60+1.631)*60*100;
  convert_lat_l2s(lat,lat_str, sizeof(lat_str),CONVERT_HP_NOSP);
  TEST_ASSERT_STR_EQ("3501.631S",lat_str,"xastir y value correctly converted to string");
  TEST_PASS("convert_lat_l2s: correct");
}
int test_convert_lon_l2s_basic_e(void)
{
  long lon;
  char lon_str[20];
  // Compute Xastir coordinates for 106d12.385'E
  // Xastir coordinates are in hundredths of seconds, with 0 being 90d N
  lon = 180*60*60*100+(106*60+12.385)*60*100;
  convert_lon_l2s(lon,lon_str, sizeof(lon_str),CONVERT_HP_NOSP);
  TEST_ASSERT_STR_EQ("10612.385E",lon_str,"xastir x value correctly converted to string");
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
int test_convert_lat_s2l_basic_s(void)
{
  long lat;
  long lat_expect;
  // Compute Xastir coordinates for 35d01.631'S
  // Xastir coordinates are in hundredths of seconds, with 0 being 90d N
  lat_expect = 90*60*60*100+(35*60+1.631)*60*100;
  lat = convert_lat_s2l("3501.631S");
  TEST_ASSERT(lat==lat_expect,"xastir y value correctly converted from string");
  TEST_PASS("convert_lat_s2l: correct");
}
int test_convert_lon_s2l_basic_e(void)
{
  long lon;
  long lon_expect;
  // Compute Xastir coordinates for 106d12.385'E
  // Xastir coordinates are in hundredths of seconds, with 0 being 90d N
  lon_expect = 180*60*60*100+(106*60+12.385)*60*100;
  lon = convert_lon_s2l("10612.385E");
  TEST_ASSERT(lon==lon_expect,"xastir x value correctly converted from string");
  TEST_PASS("convert_lon_s2l: correct");
}

// Check that s2l->l2s gives back what we started with.
int test_s2l_l2s_consistency(void)
{
  long lon;
  long lat;
  char lon_s[10+1];
  char lat_s[9+1];
  lon=convert_lon_s2l("10612.385W");
  lat=convert_lat_s2l("3501.631N");
  convert_lon_l2s(lon,lon_s,sizeof(lon_s),CONVERT_HP_NOSP);
  convert_lat_l2s(lat,lat_s,sizeof(lat_s),CONVERT_HP_NOSP);

  TEST_ASSERT_STR_EQ("3501.631N",lat_s,"Round-trip latitude consistent");
  TEST_ASSERT_STR_EQ("10612.385W",lon_s,"Round-trip longitude consistent");
  TEST_PASS("convert_lon_s2l and back: correct");
}
// Check that l2s->s2l gives back what we started with.
int test_l2s_s2l_consistency(void)
{
  long lon;
  long lat;
  long lon_return;
  long lat_return;
  char lon_s[10+1];
  char lat_s[9+1];
  lon = 180*60*60*100-(106*60+12.385)*60*100;
  lat = 90*60*60*100-(35*60+1.631)*60*100;
  convert_lon_l2s(lon,lon_s,sizeof(lon_s),CONVERT_HP_NOSP);
  convert_lat_l2s(lat,lat_s,sizeof(lat_s),CONVERT_HP_NOSP);
  lat_return = convert_lat_s2l(lat_s);
  lon_return = convert_lon_s2l(lon_s);
  TEST_ASSERT(lat==lat_return,"Round-trip latitude consistent");
  TEST_ASSERT(lon==lon_return,"Round-trip longitude consistent");
  TEST_PASS("convert_lon_s2l and back: correct");
}

// test screen/xastir/other converters
int test_convert_screen_to_xastir_coordinates(void)
{
  // presume screen to be 1900x712 pixels
  // NW corner 3515.704N 10706.340W
  // SW corner 3454.727N 10548.923W
  // the "long" coords are in centi-seconds (1/100 second)
  // scale_x and scale_y are centi-seconds per pixel
  char center_lat_s[10] = "3505.215N";
  char center_lon_s[11] = "10627.632W";
  char corner_lat_s[10];
  char corner_lon_s[11];
  char computed_lat[10];
  char computed_lon[11];
  long lon_xa, lat_xa;
  long screen_x, screen_y;

  screen_width=1900;
  screen_height=712;
  NW_corner_longitude = convert_lon_s2l("10706.340W");
  NW_corner_latitude  = convert_lat_s2l("3515.704N");
  SE_corner_longitude = convert_lon_s2l("10548.923W");
  SE_corner_latitude  = convert_lat_s2l("3454.727N");
  // Remember that Xastir coords are 0,0 at 90N 180W and increase as we
  // go east and south.
  scale_x = (SE_corner_longitude - NW_corner_longitude)/screen_width;
  scale_y = (SE_corner_latitude - NW_corner_latitude)/screen_height;
  center_latitude = (NW_corner_latitude + SE_corner_latitude)/2;
  center_longitude = (NW_corner_longitude + SE_corner_longitude)/2;

  // Now, Xastir itself actually makes the center lat/lon the primary
  // variable, and recomputes NW and SW based on that and the scale.  Let's
  // do that ourselves now.  Otherwise we get rounding problems later.

  NW_corner_longitude = center_longitude - (screen_width*scale_x)/2;
  NW_corner_latitude  = center_latitude  - (screen_height*scale_y)/2;
  SE_corner_longitude = center_longitude + (screen_width*scale_x)/2;
  SE_corner_latitude  = center_latitude  + (screen_height*scale_y)/2;

  convert_screen_to_xastir_coordinates(screen_width/2, screen_height/2,
                                       &lat_xa, &lon_xa);
  convert_lon_l2s(lon_xa, computed_lon, sizeof(computed_lon),CONVERT_HP_NOSP);
  convert_lat_l2s(lat_xa, computed_lat, sizeof(computed_lat),CONVERT_HP_NOSP);

  TEST_ASSERT_STR_EQ(center_lon_s,computed_lon,"center lon correct");
  TEST_ASSERT_STR_EQ(center_lat_s,computed_lat,"center lat correct");
  TEST_ASSERT(lon_xa == center_longitude, "Center pixel mapped correctly to center longitude");
  TEST_ASSERT(lat_xa == center_latitude, "Center pixel mapped correctly to center latitude");

  // Now try to convert this back to screen coords the way
  // map_shp and others do
  screen_x = center_longitude - NW_corner_longitude;
  screen_y = center_latitude - NW_corner_latitude;
  screen_x = screen_x/scale_x;
  screen_y = screen_y/scale_y;

  // We expect these screen coords to be screen_width/2, screen_height/2
  TEST_ASSERT(screen_x == screen_width/2, "center lon maps onto center pixel");
  TEST_ASSERT(screen_y == screen_height/2, "center lat maps onto center pixel.");

  // Now what about NW and SE corners?
  // Remember we fudged the corners based on screen size and original intended
  // scale, and rounding probably moved the actual xastir coords of the /
  // corners.  So recompute what we *expect* the conversions to be.

  // This is what we should be expecting for the NW corner strings.
  // Note that they are almost certainly different from the strings
  // we used to initialize NW_corner_lat/lon
  convert_lon_l2s(NW_corner_longitude,corner_lon_s,sizeof(corner_lon_s), CONVERT_HP_NOSP);
  convert_lat_l2s(NW_corner_latitude,corner_lat_s,sizeof(corner_lat_s), CONVERT_HP_NOSP);

  convert_screen_to_xastir_coordinates(0,0,
                                       &lat_xa, &lon_xa);
  convert_lon_l2s(lon_xa, computed_lon, sizeof(computed_lon),CONVERT_HP_NOSP);
  convert_lat_l2s(lat_xa, computed_lat, sizeof(computed_lat),CONVERT_HP_NOSP);

  TEST_ASSERT_STR_EQ(corner_lon_s,computed_lon,"NW corner lon correct");
  TEST_ASSERT_STR_EQ(corner_lat_s,computed_lat,"NW corner lat correct");
  TEST_ASSERT(lon_xa == NW_corner_longitude, "Top left pixel mapped correctly to NW corner longitude");
  TEST_ASSERT(lat_xa == NW_corner_latitude, "top left pixel mapped correctly to NW corner latitude");

  // No point checking the formula for converting back to screen, because
  // it is trivially 0,0

  // now the SE corner
  // This is what we should be expecting for the SE corner strings.
  // Note that they are almost certainly different from the strings
  // we used to initialize SE_corner_lat/lon
  convert_lon_l2s(SE_corner_longitude,corner_lon_s,sizeof(corner_lon_s), CONVERT_HP_NOSP);
  convert_lat_l2s(SE_corner_latitude,corner_lat_s,sizeof(corner_lat_s), CONVERT_HP_NOSP);

  convert_screen_to_xastir_coordinates(screen_width, screen_height,
                                       &lat_xa, &lon_xa);
  convert_lon_l2s(lon_xa, computed_lon, sizeof(computed_lon),CONVERT_HP_NOSP);
  convert_lat_l2s(lat_xa, computed_lat, sizeof(computed_lat),CONVERT_HP_NOSP);

  TEST_ASSERT_STR_EQ(corner_lon_s,computed_lon,"SE corner lon correct");
  TEST_ASSERT_STR_EQ(corner_lat_s,computed_lat,"SE corner lat correct");
  TEST_ASSERT(lon_xa == SE_corner_longitude, "Top left pixel mapped correctly to SE corner longitude");
  TEST_ASSERT(lat_xa == SE_corner_latitude, "top left pixel mapped correctly to SE corner latitude");

  // Now try to convert this back to screen coords the way
  // map_shp and others do
  screen_x = SE_corner_longitude - NW_corner_longitude;
  screen_y = SE_corner_latitude - NW_corner_latitude;
  screen_x = screen_x/scale_x;
  screen_y = screen_y/scale_y;

  // We expect these screen coords to be screen_width, screen_height
  TEST_ASSERT(screen_x == screen_width, "SE corner lon maps onto bot right pixel");
  TEST_ASSERT(screen_y == screen_height, "SE corner lat maps onto bot right pixel.");

  TEST_PASS("convert_screen_to_xastir_coordinates: works as expected");
}

// Note that this is *identical* to the previous function except it
// calls convert_xastir_to_screen_coordinates instead of having hard-coded
// junk in it.
int test_convert_xastir_to_screen_coordinates(void)
{
  // presume screen to be 1900x712 pixels
  // NW corner 3515.704N 10706.340W
  // SW corner 3454.727N 10548.923W
  // the "long" coords are in centi-seconds (1/100 second)
  // scale_x and scale_y are centi-seconds per pixel
  long screen_x, screen_y;

  screen_width=1900;
  screen_height=712;
  NW_corner_longitude = convert_lon_s2l("10706.340W");
  NW_corner_latitude  = convert_lat_s2l("3515.704N");
  SE_corner_longitude = convert_lon_s2l("10548.923W");
  SE_corner_latitude  = convert_lat_s2l("3454.727N");
  // Remember that Xastir coords are 0,0 at 90N 180W and increase as we
  // go east and south.
  scale_x = (SE_corner_longitude - NW_corner_longitude)/screen_width;
  scale_y = (SE_corner_latitude - NW_corner_latitude)/screen_height;
  center_latitude = (NW_corner_latitude + SE_corner_latitude)/2;
  center_longitude = (NW_corner_longitude + SE_corner_longitude)/2;

  // Now, Xastir itself actually makes the center lat/lon the primary
  // variable, and recomputes NW and SW based on that and the scale.  Let's
  // do that ourselves now.  Otherwise we get rounding problems later.

  NW_corner_longitude = center_longitude - (screen_width*scale_x)/2;
  NW_corner_latitude  = center_latitude  - (screen_height*scale_y)/2;
  SE_corner_longitude = center_longitude + (screen_width*scale_x)/2;
  SE_corner_latitude  = center_latitude  + (screen_height*scale_y)/2;

  // Now try to convert this back to screen coords using the
  // utility function
  convert_xastir_to_screen_coordinates(center_longitude, center_latitude, &screen_x, &screen_y);

  // We expect these screen coords to be screen_width/2, screen_height/2
  TEST_ASSERT(screen_x == screen_width/2, "center lon maps onto center pixel");
  TEST_ASSERT(screen_y == screen_height/2, "center lat maps onto center pixel.");

  // Now what about NW and SE corners?

  convert_xastir_to_screen_coordinates(NW_corner_longitude, NW_corner_latitude, &screen_x, &screen_y);
  // We expect these screen coords to be 0,0
  TEST_ASSERT(screen_x == 0, "NW corner lon maps onto top left pixel");
  TEST_ASSERT(screen_y == 0, "NW corner lat maps onto top left pixel.");

  // now the SE corner

  convert_xastir_to_screen_coordinates(SE_corner_longitude, SE_corner_latitude, &screen_x, &screen_y);

  // We expect these screen coords to be screen_width, screen_height
  TEST_ASSERT(screen_x == screen_width, "SE corner lon maps onto bot right pixel");
  TEST_ASSERT(screen_y == screen_height, "SE corner lat maps onto bot right pixel.");

  TEST_PASS("convert_xastir_to_screen_coordinates: works as expected");
}

int test_short_filename_for_status_notrunc(void)
{
  char filename[MAX_FILENAME]="this_is_short.shp";
  char short_filename[MAX_FILENAME];

  short_filename_for_status(filename, short_filename, sizeof(short_filename));

  TEST_ASSERT_STR_EQ("this_is_short.shp", short_filename, "Name not truncated if already short enough");
  TEST_PASS("short_filename_for_status");
}
int test_short_filename_for_status_trunc(void)
{
  char filename[MAX_FILENAME]="/a/long/path/with/lots/of/components/basename.shp";
  char short_filename[MAX_FILENAME];

  short_filename_for_status(filename, short_filename, sizeof(short_filename));

  TEST_ASSERT_STR_EQ("..ots/of/components/basename.shp", short_filename, "Name truncated if long");
  TEST_PASS("short_filename_for_status");
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
    {"convert_lat_l2s_basic_s",test_convert_lat_l2s_basic_s},
    {"convert_lon_l2s_basic_e",test_convert_lon_l2s_basic_e},
    {"convert_lat_l2s_lp",test_convert_lat_l2s_lp},
    {"convert_lon_l2s_lp",test_convert_lon_l2s_lp},
    {"convert_lat_s2l_basic",test_convert_lat_s2l_basic},
    {"convert_lon_s2l_basic",test_convert_lon_s2l_basic},
    {"convert_lat_s2l_basic_s",test_convert_lat_s2l_basic_s},
    {"convert_lon_s2l_basic_e",test_convert_lon_s2l_basic_e},
    {"s2l_l2s_consistency",test_s2l_l2s_consistency},
    {"l2s_s2l_consistency",test_l2s_s2l_consistency},
    {"convert_screen_to_xastir_coordinates", test_convert_screen_to_xastir_coordinates},
    {"convert_xastir_to_screen_coordinates", test_convert_xastir_to_screen_coordinates},
    {"short_filename_for_status_notrunc",test_short_filename_for_status_notrunc},
    {"short_filename_for_status_trunc",test_short_filename_for_status_trunc},
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


