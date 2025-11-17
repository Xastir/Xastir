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

/* forward declarations (so we don't need to include object_utils.h */
void pad_item_name(char *name, size_t name_size);
void format_course_speed(char *dst, size_t dst_size, char *course_str, char *speed_str, int *course, int *speed);
void format_altitude(char *dst, size_t dst_size, char *altitude_str);
void format_zulu_time(char *dst, size_t dst_size);
void format_area_color_from_numeric(char * dst, size_t dst_size, unsigned int color);
unsigned int area_color_from_string(char *color_string);
void format_area_color_from_dialog(char *dst, size_t dst_size, char *color, int bright);
void format_area_corridor(char *dst, size_t dst_size, unsigned int type, unsigned int width);
void format_probability_ring_data(char *dst, size_t dst_size, char *pmin,
                                  char *pmax);
void format_area_object_item_packet(char *dst, size_t dst_size,
                                    char *name, char object_group,
                                    char object_symbol, char *time, char *lat_str,
                                    char *lon_str, int area_type,
                                    char *area_color,
                                    int lat_offset, int lon_offset,
                                    char *speed_course, char *corridor,
                                    char *altitude, int course, int speed,
                                    int is_object, int compressed);

/* test cases for pad_item_name */
int test_pad_item_name_nopad9(void)
{
  char name[10];
  snprintf(name,sizeof(name),"A12345678");
  pad_item_name(name,sizeof(name));
  TEST_ASSERT_STR_EQ("A12345678",name,"9-char name not modified");
  TEST_PASS("pad_item_name: 9 character name not modified");
}
int test_pad_item_name_nopad8(void)
{
  char name[10];
  snprintf(name,sizeof(name),"A1234567");
  pad_item_name(name,sizeof(name));
  TEST_ASSERT_STR_EQ("A1234567",name,"8-char name not modified");
  TEST_PASS("pad_item_name: 8 character name not modified");
}
int test_pad_item_name_nopad7(void)
{
  char name[10];
  snprintf(name,sizeof(name),"A123456");
  pad_item_name(name,sizeof(name));
  TEST_ASSERT_STR_EQ("A123456",name,"7-char name not modified");
  TEST_PASS("pad_item_name: 7 character name not modified");
}
int test_pad_item_name_nopad6(void)
{
  char name[10];
  snprintf(name,sizeof(name),"A12345");
  pad_item_name(name,sizeof(name));
  TEST_ASSERT_STR_EQ("A12345",name,"6-char name not modified");
  TEST_PASS("pad_item_name: 6 character name not modified");
}
int test_pad_item_name_nopad5(void)
{
  char name[10];
  snprintf(name,sizeof(name),"A1234");
  pad_item_name(name,sizeof(name));
  TEST_ASSERT_STR_EQ("A1234",name,"5-char name not modified");
  TEST_PASS("pad_item_name: 5 character name not modified");
}
int test_pad_item_name_nopad4(void)
{
  char name[10];
  snprintf(name,sizeof(name),"A123");
  pad_item_name(name,sizeof(name));
  TEST_ASSERT_STR_EQ("A123",name,"4-char name not modified");
  TEST_PASS("pad_item_name: 4 character name not modified");
}
int test_pad_item_name_nopad3(void)
{
  char name[10];
  snprintf(name,sizeof(name),"A12");
  pad_item_name(name,sizeof(name));
  TEST_ASSERT_STR_EQ("A12",name,"3-char name not modified");
  TEST_PASS("pad_item_name: 3 character name not modified");
}
int test_pad_item_name_pad2(void)
{
  char name[10];
  snprintf(name,sizeof(name),"A1");
  pad_item_name(name,sizeof(name));
  TEST_ASSERT_STR_EQ("A1 ",name,"2-char name padded with one space");
  TEST_PASS("pad_item_name: 2 character name padded with one space");
}
int test_pad_item_name_pad1(void)
{
  char name[10];
  snprintf(name,sizeof(name),"A");
  pad_item_name(name,sizeof(name));
  TEST_ASSERT_STR_EQ("A  ",name,"1-char name padded with two spaces");
  TEST_PASS("pad_item_name: 1 character name padded with two spaces");
}

/* test cases for format_course_speed */

int test_format_course_speed_basic(void)
{
  char course_speed_string[8];
  int speed,course;
  format_course_speed(course_speed_string,sizeof(course_speed_string),
                      "090","005", &course, &speed);

  TEST_ASSERT_STR_EQ("090/005",course_speed_string, "Valid course and speed should be CSE/SPD");
  TEST_ASSERT(course == 90, "Integer course should be 90");
  TEST_ASSERT(speed == 5, "Integer speed should be 5");
  TEST_PASS("format_course_speed with valid input for both");
}

int test_format_course_speed_bad_course(void)
{
  char course_speed_string[8];
  int speed,course;
  format_course_speed(course_speed_string,sizeof(course_speed_string),
                      "390","005", &course, &speed);

  TEST_ASSERT_STR_EQ(".../005",course_speed_string, "Valid course and speed should be CSE/SPD, invalid course should be ...");
  TEST_ASSERT(course == 0, "Integer course should be 00");
  TEST_ASSERT(speed == 5, "Integer speed should be 5");
  TEST_PASS("format_course_speed with bad input for course");
}

int test_format_course_speed_null_course(void)
{
  char course_speed_string[8];
  int speed,course;
  format_course_speed(course_speed_string,sizeof(course_speed_string),
                      "","005", &course, &speed);

  TEST_ASSERT_STR_EQ(".../005",course_speed_string, "Valid course and speed should be CSE/SPD, null course should be ...");
  TEST_ASSERT(course == 0, "Integer course should be 00");
  TEST_ASSERT(speed == 5, "Integer speed should be 5");
  TEST_PASS("format_course_speed with null input for course");
}

int test_format_course_speed_bad_speed(void)
{
  char course_speed_string[8];
  int speed,course;
  format_course_speed(course_speed_string,sizeof(course_speed_string),
                      "090","1005", &course, &speed);

  TEST_ASSERT_STR_EQ("090/...",course_speed_string, "Valid course and speed should be CSE/SPD, invalid speed should be ...");
  TEST_ASSERT(course == 90, "Integer course should be 90");
  TEST_ASSERT(speed == 0, "Integer speed should be 0");
  TEST_PASS("format_course_speed with bad input for speed");
}

int test_format_course_speed_null_speed(void)
{
  char course_speed_string[8];
  int speed,course;
  format_course_speed(course_speed_string,sizeof(course_speed_string),
                      "090","", &course, &speed);

  TEST_ASSERT_STR_EQ("090/...",course_speed_string, "Valid course and speed should be CSE/SPD, missing speed should be ...");
  TEST_ASSERT(course == 90, "Integer course should be 90");
  TEST_ASSERT(speed == 0, "Integer speed should be 0");
  TEST_PASS("format_course_speed with null input for speed");
}

int test_format_course_speed_null_inputs(void)
{
  char course_speed_string[8];
  int speed,course;
  format_course_speed(course_speed_string,sizeof(course_speed_string),
                      "","", &course, &speed);

  TEST_ASSERT_STR_EQ("",course_speed_string, "Valid course and speed should be CSE/SPD, missing both should be empty string");
  TEST_ASSERT(course == 0, "Integer course should be 90");
  TEST_ASSERT(speed == 0, "Integer speed should be 0");
  TEST_PASS("format_course_speed with null input for both");
}

// tests for format_altitude

int test_format_altitude_basic(void)
{
  char altitude[10];
  format_altitude(altitude, sizeof(altitude),"15");
  TEST_ASSERT_STR_EQ("/A=000049",altitude,"15 feet altitude should be 49 meters, properly formatted");
  TEST_PASS("format_altitude with valid input");
}

int test_format_altitude_null_input(void)
{
  char altitude[10];
  format_altitude(altitude, sizeof(altitude),"");
  TEST_ASSERT_STR_EQ("",altitude,"Null input should yield null output");
  TEST_PASS("format_altitude with null input");
}

int test_format_altitude_invalid_input(void)
{
  char altitude[10];
  format_altitude(altitude, sizeof(altitude),"330000");
  TEST_ASSERT_STR_EQ("",altitude,"Excessive altitude should yield null output");
  TEST_PASS("format_altitude with invalid input");
}

// test for format_zulu_time
int test_format_zulu_time(void)
{
  char time[8];
  format_zulu_time(time,sizeof(time));
  TEST_ASSERT_STR_EQ("111618z",time,"Format of unix timestamp 1762877880 should map to day 11 hour 16 minute 18 zulu");
  TEST_PASS("format_zulu_time for specific unix timestamp");
}

int test_format_area_color_from_numeric_basic(void)
{
  char color[3];
  format_area_color_from_numeric(color,sizeof(color),0);
  TEST_ASSERT_STR_EQ("/0",color,"Bright black correct");
  format_area_color_from_numeric(color,sizeof(color),1);
  TEST_ASSERT_STR_EQ("/1",color,"Bright blue correct");
  format_area_color_from_numeric(color,sizeof(color),8);
  TEST_ASSERT_STR_EQ("/8",color,"Low black correct");
  format_area_color_from_numeric(color,sizeof(color),14);
  TEST_ASSERT_STR_EQ("14",color,"Low yellow correct");
  TEST_PASS("format_area_color_from_numeric for valid colors");
}

int test_format_area_color_from_numeric_invalid(void)
{
  char color[3];
  format_area_color_from_numeric(color,sizeof(color),16);
  TEST_ASSERT_STR_EQ("",color,"Bad color gives empty string");
  TEST_PASS("format_area_color_from_numeric for invalid colors");
}

int test_area_color_from_string_basic(void)
{
  int color;
  color = area_color_from_string("/1");
  TEST_ASSERT(color == 1, " /1 maps to 1");
  color = area_color_from_string("/8");
  TEST_ASSERT(color == 8, " /8 maps to 8");
  color = area_color_from_string("15");
  TEST_ASSERT(color == 15, " 15 maps to 15");
  TEST_PASS("area_color_from_string for valid color strings");
}

int test_area_color_from_string_invalid(void)
{
  int color;
  color = area_color_from_string("16");
  TEST_ASSERT(color == 0, " invalid value maps to 0");
  color = area_color_from_string("20");
  TEST_ASSERT(color == 0, " invalid string maps to 0");
  color = area_color_from_string("FOOBIE");
  TEST_ASSERT(color == 0, " non-numeric string maps to 0");
  color = area_color_from_string("");
  TEST_ASSERT(color == 0, " empty string maps to 0");
  TEST_PASS("area_color_from_string for invalid color strings");
}

int test_area_color_from_string_midstring(void)
{
  int color;
  char *color_string = "XXX/1XXX";
  color = area_color_from_string(&(color_string[3]));
  TEST_ASSERT(color == 1, " /1 maps to 1");
  TEST_PASS("area_color_from_string for color string inside longer string");
}

int test_format_area_color_from_dialog_basic(void)
{
  char outstring[3];

  format_area_color_from_dialog(outstring, sizeof(outstring), "/1", 1);
  TEST_ASSERT_STR_EQ("/1", outstring, "Bright blue maps correctly");
  format_area_color_from_dialog(outstring, sizeof(outstring), "/1", 0);
  TEST_ASSERT_STR_EQ("/9", outstring, "Dim blue maps correctly");
  format_area_color_from_dialog(outstring, sizeof(outstring), "/7", 1);
  TEST_ASSERT_STR_EQ("/7", outstring, "Bright grey maps correctly");
  format_area_color_from_dialog(outstring, sizeof(outstring), "/7", 0);
  TEST_ASSERT_STR_EQ("15", outstring, "Dim grey maps correctly");
  TEST_PASS("format_area_color_from_dialog for valid bright and dim colors");
}

int test_format_area_corridor_threedigit(void)
{
  char complete_corridor[6];
  format_area_corridor(complete_corridor, sizeof(complete_corridor), 1, 100);
  TEST_ASSERT_STR_EQ("{100}", complete_corridor, "Three-digit corridor correctly formatted");
  TEST_PASS("format_area_corridor for three-digit corridor values");
}

int test_format_area_corridor_twodigit(void)
{
  char complete_corridor[6];
  format_area_corridor(complete_corridor, sizeof(complete_corridor), 1, 10);
  TEST_ASSERT_STR_EQ("{10}", complete_corridor, "Two-digit corridor correctly formatted");
  TEST_PASS("format_area_corridor for two-digit corridor values");
}

int test_format_area_corridor_onedigit(void)
{
  char complete_corridor[6];
  format_area_corridor(complete_corridor, sizeof(complete_corridor), 1, 1);
  TEST_ASSERT_STR_EQ("{1}", complete_corridor, "one-digit corridor correctly formatted");
  TEST_PASS("format_area_corridor for one-digit corridor values");
}

int test_format_probability_ring_data_both(void)
{
  char comment[43+1];
  snprintf(comment,sizeof(comment),"Pointless Noise");
  format_probability_ring_data(comment,sizeof(comment),"1","2");
  TEST_ASSERT_STR_EQ("Pmin1,Pmax2,Pointless Noise", comment, "Probability rings correctly formatted when both min and max specified.");
  TEST_PASS("format_probability_ring_data when min and max specified");
}

int test_format_probability_ring_data_min_only(void)
{
  char comment[43+1];
  snprintf(comment,sizeof(comment),"Pointless Noise");
  format_probability_ring_data(comment,sizeof(comment),"1","");
  TEST_ASSERT_STR_EQ("Pmin1,Pointless Noise", comment, "Probability rings correctly formatted when only min specified.");
  TEST_PASS("format_probability_ring_data when only min specified");
}

int test_format_probability_ring_data_max_only(void)
{
  char comment[43+1];
  snprintf(comment,sizeof(comment),"Pointless Noise");
  format_probability_ring_data(comment,sizeof(comment),"","1");
  TEST_ASSERT_STR_EQ("Pmax1,Pointless Noise", comment, "Probability rings correctly formatted when only max specified.");
  TEST_PASS("format_probability_ring_data when only max specified");
}

int test_format_area_object_item_packet_object_circle_uncomp_nodata(void)
{
  char line[256];

  format_area_object_item_packet(line,sizeof(line),
                                 "TestAr", '\\',     // name, group
                                 'l',"111618z","3501.63N", // sym, time, lat
                                 "10612.38W", 0,           // lon, type(cir)
                                 "/8",                     // color (dark, bla)
                                 6, 6,                   //sqrt lat, lon offsets
                                 "","",                  //spd/cse, corridor
                                 "", 0, 0,               //alt, speed, course
                                 1, 0);                  // object, uncomp
  TEST_ASSERT_STR_EQ(";TestAr   *111618z3501.63N\\10612.38Wl006/806",
                     line,
                     "circle, dim black, no course/speed/alt, object, uncompressed is as expected.");
  TEST_PASS("format_area_object_item_packet produces correct result for simple circle case");
}

int test_format_area_object_item_packet_item_circle_uncomp_nodata(void)
{
  char line[256];

  format_area_object_item_packet(line,sizeof(line),
                                 "TestAr", '\\',     // name, group
                                 'l',"111618z","3501.63N", // sym, time, lat
                                 "10612.38W", 0,           // lon, type(cir)
                                 "/8",                     // color (dark, bla)
                                 6, 6,                   //sqrt lat, lon offsets
                                 "","",                  //spd/cse, corridor
                                 "", 0, 0,               //alt, speed, course
                                 0, 0);                  // item, uncomp
  TEST_ASSERT_STR_EQ(")TestAr!3501.63N\\10612.38Wl006/806",
                     line,
                     "circle, dim black, no course/speed/alt, item, uncompressed is as expected.");
  TEST_PASS("format_area_object_item_packet produces correct result for simple circle item case");
}

int test_format_area_object_item_packet_object_circle_comp_nodata(void)
{
  char line[256];

  format_area_object_item_packet(line,sizeof(line),
                                 "TestAr", '\\',     // name, group
                                 'l',"111618z","3501.630N", // sym, time, lat
                                 "10612.380W", 0,           // lon, type(cir)
                                 "/8",                     // color (dark, bla)
                                 6, 6,                   //sqrt lat, lon offsets
                                 "","",                  //spd/cse, corridor
                                 "", 0, 0,               //alt, speed, course
                                 1, 1);                  // object, comp
  TEST_ASSERT_STR_EQ(";TestAr   *111618z\\<he:3\\8.l   006/806",
                     line,
                     "circle, dim black, no course/speed/alt, object, compressed is as expected.");
  TEST_PASS("format_area_object_item_packet produces correct result for simple circle case, compressed");
}

int test_format_area_object_item_packet_item_circle_comp_nodata(void)
{
  char line[256];

  format_area_object_item_packet(line,sizeof(line),
                                 "TestAr", '\\',     // name, group
                                 'l',"111618z","3501.630N", // sym, time, lat
                                 "10612.380W", 0,           // lon, type(cir)
                                 "/8",                     // color (dark, bla)
                                 6, 6,                   //sqrt lat, lon offsets
                                 "","",                  //spd/cse, corridor
                                 "", 0, 0,               //alt, speed, course
                                 0, 1);                  // item, comp
  TEST_ASSERT_STR_EQ(")TestAr!\\<he:3\\8.l   006/806",
                     line,
                     "circle, dim black, no course/speed/alt, item, compressed is as expected.");
  TEST_PASS("format_area_object_item_packet produces correct result for simple circle item case, compressed");
}

int test_format_area_object_item_packet_object_circle_uncomp_alt(void)
{
  char line[256];

  format_area_object_item_packet(line,sizeof(line),
                                 "TestAr", '\\',     // name, group
                                 'l',"111618z","3501.63N", // sym, time, lat
                                 "10612.38W", 0,           // lon, type(cir)
                                 "/8",                     // color (dark, bla)
                                 6, 6,                   //sqrt lat, lon offsets
                                 "","",                  //spd/cse, corridor
                                 "/A=000100", 0, 0,      //alt, speed, course
                                 1, 0);                  // object, uncomp
  TEST_ASSERT_STR_EQ(";TestAr   *111618z3501.63N\\10612.38Wl006/806/A=000100",
                     line,
                     "circle, dim black, no course/speed with alt, object, uncompressed is as expected.");
  TEST_PASS("format_area_object_item_packet produces correct result for circle case with alt");
}

int test_format_area_object_item_packet_item_circle_uncomp_alt(void)
{
  char line[256];

  format_area_object_item_packet(line,sizeof(line),
                                 "TestAr", '\\',     // name, group
                                 'l',"111618z","3501.63N", // sym, time, lat
                                 "10612.38W", 0,           // lon, type(cir)
                                 "/8",                     // color (dark, bla)
                                 6, 6,                   //sqrt lat, lon offsets
                                 "","",                  //spd/cse, corridor
                                 "/A=000100", 0, 0,      //alt, speed, course
                                 0, 0);                  // item, uncomp
  TEST_ASSERT_STR_EQ(")TestAr!3501.63N\\10612.38Wl006/806/A=000100",
                     line,
                     "circle, dim black, no course/speed with alt, item, uncompressed is as expected.");
  TEST_PASS("format_area_object_item_packet produces correct result for circle item case with alt");
}

int test_format_area_object_item_packet_object_circle_comp_alt(void)
{
  char line[256];

  format_area_object_item_packet(line,sizeof(line),
                                 "TestAr", '\\',     // name, group
                                 'l',"111618z","3501.630N", // sym, time, lat
                                 "10612.380W", 0,           // lon, type(cir)
                                 "/8",                     // color (dark, bla)
                                 6, 6,                   //sqrt lat, lon offsets
                                 "","",                  //spd/cse, corridor
                                 "/A=000100", 0, 0,      //alt, speed, course
                                 1, 1);                  // object, comp
  TEST_ASSERT_STR_EQ(";TestAr   *111618z\\<he:3\\8.l   006/806/A=000100",
                     line,
                     "circle, dim black, no course/speed with alt, object, compressed is as expected.");
  TEST_PASS("format_area_object_item_packet produces correct result for circle case with alt, compressed");
}

int test_format_area_object_item_packet_item_circle_comp_alt(void)
{
  char line[256];

  format_area_object_item_packet(line,sizeof(line),
                                 "TestAr", '\\',     // name, group
                                 'l',"111618z","3501.630N", // sym, time, lat
                                 "10612.380W", 0,           // lon, type(cir)
                                 "/8",                     // color (dark, bla)
                                 6, 6,                   //sqrt lat, lon offsets
                                 "","",                  //spd/cse, corridor
                                 "/A=000100", 0, 0,      //alt, speed, course
                                 0, 1);                  // item, comp
  TEST_ASSERT_STR_EQ(")TestAr!\\<he:3\\8.l   006/806/A=000100",
                     line,
                     "circle, dim black, no course/speed with alt, item, compressed is as expected.");
  TEST_PASS("format_area_object_item_packet produces correct result for circle item case with alt, compressed");
}

// line right with offset and corridor
int test_format_area_object_item_packet_object_line_uncomp_nodata(void)
{
  char line[256];

  format_area_object_item_packet(line,sizeof(line),
                                 "TestAr", '\\',     // name, group
                                 'l',"111618z","3501.63N", // sym, time, lat
                                 "10612.38W", 1,           // lon, type(line right)
                                 "/4",                     // color (high, red)
                                 6, 6,                   //sqrt lat, lon offsets
                                 "","{100}",                  //spd/cse, corridor
                                 "", 0, 0,               //alt, speed, course
                                 1, 0);                  // object, uncomp
  TEST_ASSERT_STR_EQ(";TestAr   *111618z3501.63N\\10612.38Wl106/406{100}",
                     line,
                     "line, high red, no course/speed/alt, object, uncompressed is as expected.");
  TEST_PASS("format_area_object_item_packet produces correct result with corridor for simple line case");
}

int test_format_area_object_item_packet_item_line_uncomp_nodata(void)
{
  char line[256];

  format_area_object_item_packet(line,sizeof(line),
                                 "TestAr", '\\',     // name, group
                                 'l',"111618z","3501.63N", // sym, time, lat
                                 "10612.38W", 1,           // lon, type(line right)
                                 "/4",                     // color (high, red)
                                 6, 6,                   //sqrt lat, lon offsets
                                 "","{100}",                  //spd/cse, corridor
                                 "", 0, 0,               //alt, speed, course
                                 0, 0);                  // item, uncomp
  TEST_ASSERT_STR_EQ(")TestAr!3501.63N\\10612.38Wl106/406{100}",
                     line,
                     "line, high red, no course/speed/alt, item, uncompressed is as expected.");
  TEST_PASS("format_area_object_item_packet produces correct result with corridor for simple line item case");
}

int test_format_area_object_item_packet_object_line_comp_nodata(void)
{
  char line[256];

  format_area_object_item_packet(line,sizeof(line),
                                 "TestAr", '\\',     // name, group
                                 'l',"111618z","3501.630N", // sym, time, lat
                                 "10612.380W", 1,           // lon, type(line right)
                                 "/4",                     // color (high, red)
                                 6, 6,                   //sqrt lat, lon offsets
                                 "","{100}",                  //spd/cse, corridor
                                 "", 0, 0,               //alt, speed, course
                                 1, 1);                  // object, comp
  TEST_ASSERT_STR_EQ(";TestAr   *111618z\\<he:3\\8.l   106/406{100}",
                     line,
                     "line, high red, no course/speed/alt, object, compressed is as expected.");
  TEST_PASS("format_area_object_item_packet produces correct result with corridor for simple line case, compressed");
}

int test_format_area_object_item_packet_item_line_comp_nodata(void)
{
  char line[256];

  format_area_object_item_packet(line,sizeof(line),
                                 "TestAr", '\\',     // name, group
                                 'l',"111618z","3501.630N", // sym, time, lat
                                 "10612.380W", 1,           // lon, type(line right)
                                 "/4",                     // color (high, red)
                                 6, 6,                   //sqrt lat, lon offsets
                                 "","{100}",                  //spd/cse, corridor
                                 "", 0, 0,               //alt, speed, course
                                 0, 1);                  // item, comp
  TEST_ASSERT_STR_EQ(")TestAr!\\<he:3\\8.l   106/406{100}",
                     line,
                     "line, high red, no course/speed/alt, item, compressed is as expected.");
  TEST_PASS("format_area_object_item_packet produces correct result with corridor for simple line item case, compressed");
}

int test_format_area_object_item_packet_object_line_uncomp_alt(void)
{
  char line[256];

  format_area_object_item_packet(line,sizeof(line),
                                 "TestAr", '\\',     // name, group
                                 'l',"111618z","3501.63N", // sym, time, lat
                                 "10612.38W", 1,           // lon, type(line right)
                                 "/4",                     // color (high, red)
                                 6, 6,                   //sqrt lat, lon offsets
                                 "","{100}",                  //spd/cse, corridor
                                 "/A=000100", 0, 0,      //alt, speed, course
                                 1, 0);                  // object, uncomp
  TEST_ASSERT_STR_EQ(";TestAr   *111618z3501.63N\\10612.38Wl106/406{100}/A=000100",
                     line,
                     "line, high red, no course/speed with alt, object, uncompressed is as expected.");
  TEST_PASS("format_area_object_item_packet produces correct result with corridor for line case with alt");
}

int test_format_area_object_item_packet_item_line_uncomp_alt(void)
{
  char line[256];

  format_area_object_item_packet(line,sizeof(line),
                                 "TestAr", '\\',     // name, group
                                 'l',"111618z","3501.63N", // sym, time, lat
                                 "10612.38W", 1,           // lon, type(line right)
                                 "/4",                     // color (high, red)
                                 6, 6,                   //sqrt lat, lon offsets
                                 "","{100}",                  //spd/cse, corridor
                                 "/A=000100", 0, 0,      //alt, speed, course
                                 0, 0);                  // item, uncomp
  TEST_ASSERT_STR_EQ(")TestAr!3501.63N\\10612.38Wl106/406{100}/A=000100",
                     line,
                     "line, high red, no course/speed with alt, item, uncompressed is as expected.");
  TEST_PASS("format_area_object_item_packet produces correct result with corridor for line item case with alt");
}

int test_format_area_object_item_packet_object_line_comp_alt(void)
{
  char line[256];

  format_area_object_item_packet(line,sizeof(line),
                                 "TestAr", '\\',     // name, group
                                 'l',"111618z","3501.630N", // sym, time, lat
                                 "10612.380W", 1,           // lon, type(line right)
                                 "/4",                     // color (high, red)
                                 6, 6,                   //sqrt lat, lon offsets
                                 "","{100}",                  //spd/cse, corridor
                                 "/A=000100", 0, 0,      //alt, speed, course
                                 1, 1);                  // object, comp
  TEST_ASSERT_STR_EQ(";TestAr   *111618z\\<he:3\\8.l   106/406{100}/A=000100",
                     line,
                     "line, high red, no course/speed with alt, object, compressed is as expected.");
  TEST_PASS("format_area_object_item_packet produces correct result with corridor for line case with alt, compressed");
}

int test_format_area_object_item_packet_item_line_comp_alt(void)
{
  char line[256];

  format_area_object_item_packet(line,sizeof(line),
                                 "TestAr", '\\',     // name, group
                                 'l',"111618z","3501.630N", // sym, time, lat
                                 "10612.380W", 1,           // lon, type(line right)
                                 "/4",                     // color (high, red)
                                 6, 6,                   //sqrt lat, lon offsets
                                 "","{100}",                  //spd/cse, corridor
                                 "/A=000100", 0, 0,      //alt, speed, course
                                 0, 1);                  // item, comp
  TEST_ASSERT_STR_EQ(")TestAr!\\<he:3\\8.l   106/406{100}/A=000100",
                     line,
                     "line, dim black, no course/speed with alt, item, compressed is as expected.");
  TEST_PASS("format_area_object_item_packet produces correct result with corridor for line item case with alt, compressed");
}

/* Test runner */
typedef struct {
    const char *name;
    int (*func)(void);
} test_case_t;

int main(int argc, char *argv[])
{
  test_case_t tests[] = {
    /* format_course_speed tests */
    {"format_course_speed_basic", test_format_course_speed_basic},
    {"format_course_speed_bad_course", test_format_course_speed_bad_course},
    {"format_course_speed_null_course", test_format_course_speed_null_course},
    {"format_course_speed_bad_speed", test_format_course_speed_bad_speed},
    {"format_course_speed_null_speed", test_format_course_speed_null_speed},
    {"format_course_speed_null_inputs", test_format_course_speed_null_inputs},
    {"format_altitude_basic", test_format_altitude_basic},
    {"format_altitude_null_input", test_format_altitude_null_input},
    {"format_altitude_invalid_input", test_format_altitude_invalid_input},
    {"format_zulu_time", test_format_zulu_time},
    {"format_area_color_from_numeric_basic", test_format_area_color_from_numeric_basic},
    {"format_area_color_from_numeric_invalid", test_format_area_color_from_numeric_invalid},
    {"area_color_from_string_basic", test_area_color_from_string_basic},
    {"area_color_from_string_invalid", test_area_color_from_string_invalid},
    {"area_color_from_string_midstring", test_area_color_from_string_midstring},
    {"format_area_color_from_dialog_basic",test_format_area_color_from_dialog_basic},
    {"format_area_corridor_threedigit",test_format_area_corridor_threedigit},
    {"format_area_corridor_twodigit",test_format_area_corridor_twodigit},
    {"format_area_corridor_onedigit",test_format_area_corridor_onedigit},
    {"format_probability_ring_data_both",test_format_probability_ring_data_both},
    {"format_probability_ring_data_min_only",test_format_probability_ring_data_min_only},
    {"format_probability_ring_data_max_only",test_format_probability_ring_data_max_only},
    {"format_area_object_item_packet_object_circle_uncomp_nodata",test_format_area_object_item_packet_object_circle_uncomp_nodata},
    {"format_area_object_item_packet_item_circle_uncomp_nodata",test_format_area_object_item_packet_item_circle_uncomp_nodata},
    {"format_area_object_item_packet_object_circle_comp_nodata",test_format_area_object_item_packet_object_circle_comp_nodata},
    {"format_area_object_item_packet_item_circle_comp_nodata",test_format_area_object_item_packet_item_circle_comp_nodata},
    {"format_area_object_item_packet_object_circle_uncomp_alt",test_format_area_object_item_packet_object_circle_uncomp_alt},
    {"format_area_object_item_packet_item_circle_uncomp_alt",test_format_area_object_item_packet_item_circle_uncomp_alt},
    {"format_area_object_item_packet_object_circle_comp_alt",test_format_area_object_item_packet_object_circle_comp_alt},
    {"format_area_object_item_packet_item_circle_comp_alt",test_format_area_object_item_packet_item_circle_comp_alt},
    {"format_area_object_item_packet_object_line_uncomp_nodata",test_format_area_object_item_packet_object_line_uncomp_nodata},
    {"format_area_object_item_packet_item_line_uncomp_nodata",test_format_area_object_item_packet_item_line_uncomp_nodata},
    {"format_area_object_item_packet_object_line_comp_nodata",test_format_area_object_item_packet_object_line_comp_nodata},
    {"format_area_object_item_packet_item_line_comp_nodata",test_format_area_object_item_packet_item_line_comp_nodata},
    {"format_area_object_item_packet_object_line_uncomp_alt",test_format_area_object_item_packet_object_line_uncomp_alt},
    {"format_area_object_item_packet_item_line_uncomp_alt",test_format_area_object_item_packet_item_line_uncomp_alt},
    {"format_area_object_item_packet_object_line_comp_alt",test_format_area_object_item_packet_object_line_comp_alt},
    {"format_area_object_item_packet_item_line_comp_alt",test_format_area_object_item_packet_item_line_comp_alt},
    {"pad_item_name_nopad9",test_pad_item_name_nopad9},
    {"pad_item_name_nopad8",test_pad_item_name_nopad8},
    {"pad_item_name_nopad7",test_pad_item_name_nopad7},
    {"pad_item_name_nopad6",test_pad_item_name_nopad6},
    {"pad_item_name_nopad5",test_pad_item_name_nopad5},
    {"pad_item_name_nopad4",test_pad_item_name_nopad4},
    {"pad_item_name_nopad3",test_pad_item_name_nopad3},
    {"pad_item_name_pad2",test_pad_item_name_pad2},
    {"pad_item_name_pad1",test_pad_item_name_pad1},
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
