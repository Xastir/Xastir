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

#include "database.h"
#include "objects.h"

//forward declaration of function not exported by objects.h (yet)
int Create_object_item_tx_string(DataRow *p_station, char *line, int line_length);

int test_constructor_null_everything(void)
{
  DataRow *theDataRow;
  theDataRow=construct_object_item_data_row("","","",  // name, lat/lon
                                            '/','/',    // group, symbol
                                            "",         //comment
                                            "","",      //course, speed
                                            "",         //altitude
                                            0,0,0,      //area, type, filled
                                            "",         // area color
                                            "","",      // offsets
                                            "",         // corridor
                                            0,          // signpost
                                            "",         // signpost string
                                            0, 0, 0,    // df, omni, beam
                                            "",         // shgd
                                            "",         //bearing
                                            "",         // NRQ
                                            0,          // prob circles
                                            "","",      // prob min, max
                                            1,          // is_object
                                            0);         // killed
  TEST_ASSERT(theDataRow == NULL, "Constructor returns null appropriately");
  TEST_PASS("construct_object_item_data_row");
}
int test_constructor_simple_object(void)
{
  DataRow *theDataRow;
  long expect_lat = 90*60*60*100-(35*60+1.63)*60*100;
  long expect_lon = 180*60*60*100-(106*60+12.38)*60*100;
  char line[256];
  theDataRow=construct_object_item_data_row("TEST",  // name
                                            "3501.63N",
                                            "10612.38W", // lat/lon
                                            '/','/',    // group, symbol
                                            "",         //comment
                                            "","",      //course, speed
                                            "",         //altitude
                                            0,0,0,      //area, type, filled
                                            "",         // area color
                                            "","",      // offsets
                                            "",         // corridor
                                            0,          // signpost
                                            "",         // signpost string
                                            0, 0, 0,    // df, omni, beam
                                            "",         // shgd
                                            "",         //bearing
                                            "",         // NRQ
                                            0,          // prob circles
                                            "","",      // prob min, max
                                            1,          // is_object
                                            0);         // killed
  TEST_ASSERT(theDataRow, "Constructor returns valid pointer appropriately");
  TEST_ASSERT(strcmp(theDataRow->call_sign,"TEST")==0,"Name populated correctly");
  TEST_ASSERT(theDataRow->flag & ST_OBJECT, "Constructor creates object");
  TEST_ASSERT(theDataRow->flag & ST_ACTIVE, "Constructor creates active object");
  TEST_ASSERT(theDataRow->coord_lat == expect_lat, "lat is correct");
  TEST_ASSERT(theDataRow->coord_lon == expect_lon, "lat is correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_type == '/',"Symbol table correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_symbol == '/',"Symbol correct");
  TEST_ASSERT(theDataRow->aprs_symbol.special_overlay == '\0',"overlay null");

  Create_object_item_tx_string(theDataRow,line,sizeof(line));
  // clobber the time with our standard fake time, don't worry about termination
  // coz it's in the middle of an existing string
  memcpy(&(line[11]),"111618z",7);
  TEST_ASSERT_STR_EQ(";TEST     *111618z3501.63N/10612.38W/",line,
                     "Object string correctly formatted");

  if (theDataRow)
    free(theDataRow);

  TEST_PASS("construct_object_item_data_row");

}

int test_constructor_simple_item(void)
{
  DataRow *theDataRow;
  long expect_lat = 90*60*60*100-(35*60+1.63)*60*100;
  long expect_lon = 180*60*60*100-(106*60+12.38)*60*100;
  char line[256];
  theDataRow=construct_object_item_data_row("TEST",  // name
                                            "3501.63N",
                                            "10612.38W", // lat/lon
                                            '/','/',    // group, symbol
                                            "",         //comment
                                            "","",      //course, speed
                                            "",         //altitude
                                            0,0,0,      //area, type, filled
                                            "",         // area color
                                            "","",      // offsets
                                            "",         // corridor
                                            0,          // signpost
                                            "",         // signpost string
                                            0, 0, 0,    // df, omni, beam
                                            "",         // shgd
                                            "",         //bearing
                                            "",         // NRQ
                                            0,          // prob circles
                                            "","",      // prob min, max
                                            0,          // is_object
                                            0);         // killed
  TEST_ASSERT(theDataRow, "Constructor returns valid pointer appropriately");
  TEST_ASSERT(strcmp(theDataRow->call_sign,"TEST")==0,"Name populated correctly");
  TEST_ASSERT(theDataRow->flag & ST_ITEM, "Constructor creates item");
  TEST_ASSERT(theDataRow->flag & ST_ACTIVE, "Constructor creates active item");
  TEST_ASSERT(theDataRow->coord_lat == expect_lat, "lat is correct");
  TEST_ASSERT(theDataRow->coord_lon == expect_lon, "lat is correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_type == '/',"Symbol table correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_symbol == '/',"Symbol correct");
  TEST_ASSERT(theDataRow->aprs_symbol.special_overlay == '\0',"overlay null");

  Create_object_item_tx_string(theDataRow,line,sizeof(line));
  TEST_ASSERT_STR_EQ(")TEST!3501.63N/10612.38W/",line,
                     "item string correctly formatted");

  if (theDataRow)
    free(theDataRow);

  TEST_PASS("construct_object_item_data_row");

}
int test_constructor_simple_killed_object(void)
{
  DataRow *theDataRow;
  long expect_lat = 90*60*60*100-(35*60+1.63)*60*100;
  long expect_lon = 180*60*60*100-(106*60+12.38)*60*100;
  char line[256];
  theDataRow=construct_object_item_data_row("TEST",  // name
                                            "3501.63N",
                                            "10612.38W", // lat/lon
                                            '/','/',    // group, symbol
                                            "",         //comment
                                            "","",      //course, speed
                                            "",         //altitude
                                            0,0,0,      //area, type, filled
                                            "",         // area color
                                            "","",      // offsets
                                            "",         // corridor
                                            0,          // signpost
                                            "",         // signpost string
                                            0, 0, 0,    // df, omni, beam
                                            "",         // shgd
                                            "",         //bearing
                                            "",         // NRQ
                                            0,          // prob circles
                                            "","",      // prob min, max
                                            1,          // is_object
                                            1);         // killed
  TEST_ASSERT(theDataRow, "Constructor returns valid pointer appropriately");
  TEST_ASSERT(strcmp(theDataRow->call_sign,"TEST")==0,"Name populated correctly");
  TEST_ASSERT(theDataRow->flag & ST_OBJECT, "Constructor creates object");
  TEST_ASSERT((theDataRow->flag & ST_ACTIVE) == 0, "Constructor creates killed object");
  TEST_ASSERT(theDataRow->coord_lat == expect_lat, "lat is correct");
  TEST_ASSERT(theDataRow->coord_lon == expect_lon, "lat is correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_type == '/',"Symbol table correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_symbol == '/',"Symbol correct");
  TEST_ASSERT(theDataRow->aprs_symbol.special_overlay == '\0',"overlay null");

  Create_object_item_tx_string(theDataRow,line,sizeof(line));
  // clobber the time with our standard fake time, don't worry about termination
  // coz it's in the middle of an existing string
  memcpy(&(line[11]),"111618z",7);
  TEST_ASSERT_STR_EQ(";TEST     _111618z3501.63N/10612.38W/",line,
                     "Object string correctly formatted");

  if (theDataRow)
    free(theDataRow);

  TEST_PASS("construct_object_item_data_row");

}
int test_constructor_simple_killed_item(void)
{
  DataRow *theDataRow;
  long expect_lat = 90*60*60*100-(35*60+1.63)*60*100;
  long expect_lon = 180*60*60*100-(106*60+12.38)*60*100;
  char line[256];
  theDataRow=construct_object_item_data_row("TEST",  // name
                                            "3501.63N",
                                            "10612.38W", // lat/lon
                                            '/','/',    // group, symbol
                                            "",         //comment
                                            "","",      //course, speed
                                            "",         //altitude
                                            0,0,0,      //area, type, filled
                                            "",         // area color
                                            "","",      // offsets
                                            "",         // corridor
                                            0,          // signpost
                                            "",         // signpost string
                                            0, 0, 0,    // df, omni, beam
                                            "",         // shgd
                                            "",         //bearing
                                            "",         // NRQ
                                            0,          // prob circles
                                            "","",      // prob min, max
                                            0,          // is_object
                                            1);         // killed
  TEST_ASSERT(theDataRow, "Constructor returns valid pointer appropriately");
  TEST_ASSERT(strcmp(theDataRow->call_sign,"TEST")==0,"Name populated correctly");
  TEST_ASSERT(theDataRow->flag & ST_ITEM, "Constructor creates item");
  TEST_ASSERT((theDataRow->flag & ST_ACTIVE) == 0, "Constructor creates killed item");
  TEST_ASSERT(theDataRow->coord_lat == expect_lat, "lat is correct");
  TEST_ASSERT(theDataRow->coord_lon == expect_lon, "lat is correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_type == '/',"Symbol table correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_symbol == '/',"Symbol correct");
  TEST_ASSERT(theDataRow->aprs_symbol.special_overlay == '\0',"overlay null");

  Create_object_item_tx_string(theDataRow,line,sizeof(line));
  TEST_ASSERT_STR_EQ(")TEST_3501.63N/10612.38W/",line,
                     "item string correctly formatted");

  if (theDataRow)
    free(theDataRow);

  TEST_PASS("construct_object_item_data_row");

}
int test_constructor_simple_object_course_speed(void)
{
  DataRow *theDataRow;
  long expect_lat = 90*60*60*100-(35*60+1.63)*60*100;
  long expect_lon = 180*60*60*100-(106*60+12.38)*60*100;
  char line[256];
  theDataRow=construct_object_item_data_row("TEST",  // name
                                            "3501.63N",
                                            "10612.38W", // lat/lon
                                            '/','/',    // group, symbol
                                            "",         //comment
                                            "90","5",      //course, speed
                                            "",         //altitude
                                            0,0,0,      //area, type, filled
                                            "",         // area color
                                            "","",      // offsets
                                            "",         // corridor
                                            0,          // signpost
                                            "",         // signpost string
                                            0, 0, 0,    // df, omni, beam
                                            "",         // shgd
                                            "",         //bearing
                                            "",         // NRQ
                                            0,          // prob circles
                                            "","",      // prob min, max
                                            1,          // is_object
                                            0);         // killed
  TEST_ASSERT(theDataRow, "Constructor returns valid pointer appropriately");
  TEST_ASSERT(strcmp(theDataRow->call_sign,"TEST")==0,"Name populated correctly");
  TEST_ASSERT(theDataRow->flag & ST_OBJECT, "Constructor creates object");
  TEST_ASSERT(theDataRow->flag & ST_ACTIVE, "Constructor creates active object");
  TEST_ASSERT(theDataRow->coord_lat == expect_lat, "lat is correct");
  TEST_ASSERT(theDataRow->coord_lon == expect_lon, "lat is correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_type == '/',"Symbol table correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_symbol == '/',"Symbol correct");
  TEST_ASSERT(theDataRow->aprs_symbol.special_overlay == '\0',"overlay null");
  TEST_ASSERT_STR_EQ(theDataRow->course,"090","Course correct");
  TEST_ASSERT_STR_EQ(theDataRow->speed,"  5","Speed correct");

  Create_object_item_tx_string(theDataRow,line,sizeof(line));
  // clobber the time with our standard fake time, don't worry about termination
  // coz it's in the middle of an existing string
  memcpy(&(line[11]),"111618z",7);
  TEST_ASSERT_STR_EQ(";TEST     *111618z3501.63N/10612.38W/090/005",line,
                     "Object string correctly formatted");

  if (theDataRow)
    free(theDataRow);

  TEST_PASS("construct_object_item_data_row");

}
int test_constructor_simple_item_course_speed(void)
{
  DataRow *theDataRow;
  long expect_lat = 90*60*60*100-(35*60+1.63)*60*100;
  long expect_lon = 180*60*60*100-(106*60+12.38)*60*100;
  char line[256];
  theDataRow=construct_object_item_data_row("TEST",  // name
                                            "3501.63N",
                                            "10612.38W", // lat/lon
                                            '/','/',    // group, symbol
                                            "",         //comment
                                            "90","5",      //course, speed
                                            "",         //altitude
                                            0,0,0,      //area, type, filled
                                            "",         // area color
                                            "","",      // offsets
                                            "",         // corridor
                                            0,          // signpost
                                            "",         // signpost string
                                            0, 0, 0,    // df, omni, beam
                                            "",         // shgd
                                            "",         //bearing
                                            "",         // NRQ
                                            0,          // prob circles
                                            "","",      // prob min, max
                                            0,          // is_object
                                            0);         // killed
  TEST_ASSERT(theDataRow, "Constructor returns valid pointer appropriately");
  TEST_ASSERT(strcmp(theDataRow->call_sign,"TEST")==0,"Name populated correctly");
  TEST_ASSERT(theDataRow->flag & ST_ITEM, "Constructor creates item");
  TEST_ASSERT(theDataRow->flag & ST_ACTIVE, "Constructor creates active item");
  TEST_ASSERT(theDataRow->coord_lat == expect_lat, "lat is correct");
  TEST_ASSERT(theDataRow->coord_lon == expect_lon, "lat is correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_type == '/',"Symbol table correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_symbol == '/',"Symbol correct");
  TEST_ASSERT(theDataRow->aprs_symbol.special_overlay == '\0',"overlay null");
  TEST_ASSERT_STR_EQ(theDataRow->course,"090","Course correct");
  TEST_ASSERT_STR_EQ(theDataRow->speed,"  5","Speed correct");

  Create_object_item_tx_string(theDataRow,line,sizeof(line));
  TEST_ASSERT_STR_EQ(")TEST!3501.63N/10612.38W/090/005",line,
                     "item string correctly formatted");

  if (theDataRow)
    free(theDataRow);

  TEST_PASS("construct_object_item_data_row");

}

/* Test runner */
typedef struct {
    const char *name;
    int (*func)(void);
} test_case_t;

int main(int argc, char *argv[])
{
  test_case_t tests[] = {
    {"constructor_null_everything",test_constructor_null_everything},
    {"constructor_simple_object",test_constructor_simple_object},
    {"constructor_simple_item",test_constructor_simple_item},
    {"constructor_simple_killed_object",test_constructor_simple_killed_object},
    {"constructor_simple_killed_item",test_constructor_simple_killed_item},
    {"constructor_simple_object_course_speed",test_constructor_simple_object_course_speed},
    {"constructor_simple_item_course_speed",test_constructor_simple_item_course_speed},
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
