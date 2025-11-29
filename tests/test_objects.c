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
void destroy_object_item_data_row(DataRow *theDataRow);

// this is defined and set to zero in test_objects_stubs.c, and
// Create_object_item_tx_string reads it to decide whether to compress
// the posit.
extern int transmit_compressed_objects_items;

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
  TEST_ASSERT_STR_EQ("TEST",theDataRow->call_sign,"Name populated correctly");
  TEST_ASSERT(theDataRow->flag & ST_OBJECT, "Constructor creates object");
  TEST_ASSERT(theDataRow->flag & ST_ACTIVE, "Constructor creates active object");
  TEST_ASSERT(theDataRow->coord_lat == expect_lat, "lat is correct");
  TEST_ASSERT(theDataRow->coord_lon == expect_lon, "lon is correct");
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
    destroy_object_item_data_row(theDataRow);

  TEST_PASS("construct_object_item_data_row");

}
int test_constructor_simple_object_numeric_overlay(void)
{
  DataRow *theDataRow;
  long expect_lat = 90*60*60*100-(35*60+1.63)*60*100;
  long expect_lon = 180*60*60*100-(106*60+12.38)*60*100;
  char line[256];
  theDataRow=construct_object_item_data_row("TEST",  // name
                                            "3501.63N",
                                            "10612.38W", // lat/lon
                                            '5','0',    // group, symbol
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
  TEST_ASSERT_STR_EQ("TEST",theDataRow->call_sign,"Name populated correctly");
  TEST_ASSERT(theDataRow->flag & ST_OBJECT, "Constructor creates object");
  TEST_ASSERT(theDataRow->flag & ST_ACTIVE, "Constructor creates active object");
  TEST_ASSERT(theDataRow->coord_lat == expect_lat, "lat is correct");
  TEST_ASSERT(theDataRow->coord_lon == expect_lon, "lon is correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_type == '\\',"Symbol table correct");
  TEST_ASSERT(theDataRow->aprs_symbol.special_overlay == '5',"overlay correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_symbol == '0',"Symbol correct");

  Create_object_item_tx_string(theDataRow,line,sizeof(line));
  // clobber the time with our standard fake time, don't worry about termination
  // coz it's in the middle of an existing string
  memcpy(&(line[11]),"111618z",7);
  TEST_ASSERT_STR_EQ(";TEST     *111618z3501.63N510612.38W0",line,
                     "Object string correctly formatted");

  if (theDataRow)
    destroy_object_item_data_row(theDataRow);

  TEST_PASS("construct_object_item_data_row");

}
int test_constructor_simple_object_numeric_overlay_compressed(void)
{
  DataRow *theDataRow;
  long expect_lat = 90*60*60*100-(35*60+1.633)*60*100;
  long expect_lon = 180*60*60*100-(106*60+12.384)*60*100;
  char line[256];

  theDataRow=construct_object_item_data_row("TEST",  // name
                                            "3501.633N",
                                            "10612.384W", // lat/lon
                                            '5','0',    // group, symbol
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
  TEST_ASSERT_STR_EQ("TEST",theDataRow->call_sign,"Name populated correctly");
  TEST_ASSERT(theDataRow->flag & ST_OBJECT, "Constructor creates object");
  TEST_ASSERT(theDataRow->flag & ST_ACTIVE, "Constructor creates active object");
  TEST_ASSERT(theDataRow->coord_lat == expect_lat, "lat is correct");
  TEST_ASSERT(theDataRow->coord_lon == expect_lon, "lon is correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_type == '\\',"Symbol table correct");
  TEST_ASSERT(theDataRow->aprs_symbol.special_overlay == '5',"overlay null");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_symbol == '0',"Symbol correct");

  transmit_compressed_objects_items = 1;
  Create_object_item_tx_string(theDataRow,line,sizeof(line));
  transmit_compressed_objects_items = 0;
  // clobber the time with our standard fake time, don't worry about termination
  // coz it's in the middle of an existing string
  memcpy(&(line[11]),"111618z",7);
  TEST_ASSERT_STR_EQ(";TEST     *111618zf<he'3\\8!0   ",line,
                     "Object string correctly formatted");

  if (theDataRow)
    destroy_object_item_data_row(theDataRow);

  TEST_PASS("construct_object_item_data_row");

}
int test_constructor_simple_object_name_too_long(void)
{
  DataRow *theDataRow;
  long expect_lat = 90*60*60*100-(35*60+1.63)*60*100;
  long expect_lon = 180*60*60*100-(106*60+12.38)*60*100;
  char line[256];
  char name[12]="TEST5678901";
  theDataRow=construct_object_item_data_row(name,  // name
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
  TEST_ASSERT_STR_EQ("TEST56789",theDataRow->call_sign,"Name populated correctly with truncated name");
  TEST_ASSERT(theDataRow->flag & ST_OBJECT, "Constructor creates object");
  TEST_ASSERT(theDataRow->flag & ST_ACTIVE, "Constructor creates active object");
  TEST_ASSERT(theDataRow->coord_lat == expect_lat, "lat is correct");
  TEST_ASSERT(theDataRow->coord_lon == expect_lon, "lon is correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_type == '/',"Symbol table correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_symbol == '/',"Symbol correct");
  TEST_ASSERT(theDataRow->aprs_symbol.special_overlay == '\0',"overlay null");

  Create_object_item_tx_string(theDataRow,line,sizeof(line));
  // clobber the time with our standard fake time, don't worry about termination
  // coz it's in the middle of an existing string
  memcpy(&(line[11]),"111618z",7);
  TEST_ASSERT_STR_EQ(";TEST56789*111618z3501.63N/10612.38W/",line,
                     "Object string correctly formatted");

  if (theDataRow)
    destroy_object_item_data_row(theDataRow);

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
  TEST_ASSERT_STR_EQ("TEST",theDataRow->call_sign,"Name populated correctly");
  TEST_ASSERT(theDataRow->flag & ST_ITEM, "Constructor creates item");
  TEST_ASSERT(theDataRow->flag & ST_ACTIVE, "Constructor creates active item");
  TEST_ASSERT(theDataRow->coord_lat == expect_lat, "lat is correct");
  TEST_ASSERT(theDataRow->coord_lon == expect_lon, "lon is correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_type == '/',"Symbol table correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_symbol == '/',"Symbol correct");
  TEST_ASSERT(theDataRow->aprs_symbol.special_overlay == '\0',"overlay null");

  Create_object_item_tx_string(theDataRow,line,sizeof(line));
  TEST_ASSERT_STR_EQ(")TEST!3501.63N/10612.38W/",line,
                     "item string correctly formatted");

  if (theDataRow)
    destroy_object_item_data_row(theDataRow);

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
  TEST_ASSERT_STR_EQ("TEST",theDataRow->call_sign,"Name populated correctly");
  TEST_ASSERT(theDataRow->flag & ST_OBJECT, "Constructor creates object");
  TEST_ASSERT((theDataRow->flag & ST_ACTIVE) == 0, "Constructor creates killed object");
  TEST_ASSERT(theDataRow->coord_lat == expect_lat, "lat is correct");
  TEST_ASSERT(theDataRow->coord_lon == expect_lon, "lon is correct");
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
    destroy_object_item_data_row(theDataRow);

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
  TEST_ASSERT_STR_EQ("TEST",theDataRow->call_sign,"Name populated correctly");
  TEST_ASSERT(theDataRow->flag & ST_ITEM, "Constructor creates item");
  TEST_ASSERT((theDataRow->flag & ST_ACTIVE) == 0, "Constructor creates killed item");
  TEST_ASSERT(theDataRow->coord_lat == expect_lat, "lat is correct");
  TEST_ASSERT(theDataRow->coord_lon == expect_lon, "lon is correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_type == '/',"Symbol table correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_symbol == '/',"Symbol correct");
  TEST_ASSERT(theDataRow->aprs_symbol.special_overlay == '\0',"overlay null");

  Create_object_item_tx_string(theDataRow,line,sizeof(line));
  TEST_ASSERT_STR_EQ(")TEST_3501.63N/10612.38W/",line,
                     "item string correctly formatted");

  if (theDataRow)
    destroy_object_item_data_row(theDataRow);

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
  TEST_ASSERT_STR_EQ("TEST",theDataRow->call_sign,"Name populated correctly");
  TEST_ASSERT(theDataRow->flag & ST_OBJECT, "Constructor creates object");
  TEST_ASSERT(theDataRow->flag & ST_ACTIVE, "Constructor creates active object");
  TEST_ASSERT(theDataRow->coord_lat == expect_lat, "lat is correct");
  TEST_ASSERT(theDataRow->coord_lon == expect_lon, "lon is correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_type == '/',"Symbol table correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_symbol == '/',"Symbol correct");
  TEST_ASSERT(theDataRow->aprs_symbol.special_overlay == '\0',"overlay null");
  TEST_ASSERT_STR_EQ("090",theDataRow->course,"Course correct");
  TEST_ASSERT_STR_EQ("  5",theDataRow->speed,"Speed correct");

  Create_object_item_tx_string(theDataRow,line,sizeof(line));
  // clobber the time with our standard fake time, don't worry about termination
  // coz it's in the middle of an existing string
  memcpy(&(line[11]),"111618z",7);
  TEST_ASSERT_STR_EQ(";TEST     *111618z3501.63N/10612.38W/090/005",line,
                     "Object string correctly formatted");

  if (theDataRow)
    destroy_object_item_data_row(theDataRow);

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
  TEST_ASSERT_STR_EQ("TEST",theDataRow->call_sign,"Name populated correctly");
  TEST_ASSERT(theDataRow->flag & ST_ITEM, "Constructor creates item");
  TEST_ASSERT(theDataRow->flag & ST_ACTIVE, "Constructor creates active item");
  TEST_ASSERT(theDataRow->coord_lat == expect_lat, "lat is correct");
  TEST_ASSERT(theDataRow->coord_lon == expect_lon, "lon is correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_type == '/',"Symbol table correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_symbol == '/',"Symbol correct");
  TEST_ASSERT(theDataRow->aprs_symbol.special_overlay == '\0',"overlay null");
  TEST_ASSERT_STR_EQ("090",theDataRow->course,"Course correct");
  TEST_ASSERT_STR_EQ("  5",theDataRow->speed,"Speed correct");

  Create_object_item_tx_string(theDataRow,line,sizeof(line));
  TEST_ASSERT_STR_EQ(")TEST!3501.63N/10612.38W/090/005",line,
                     "item string correctly formatted");

  if (theDataRow)
    destroy_object_item_data_row(theDataRow);

  TEST_PASS("construct_object_item_data_row");

}
int test_constructor_simple_object_course_speed_alt(void)
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
                                            "100",         //altitude
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
  TEST_ASSERT_STR_EQ("TEST",theDataRow->call_sign,"Name populated correctly");
  TEST_ASSERT(theDataRow->flag & ST_OBJECT, "Constructor creates object");
  TEST_ASSERT(theDataRow->flag & ST_ACTIVE, "Constructor creates active object");
  TEST_ASSERT(theDataRow->coord_lat == expect_lat, "lat is correct");
  TEST_ASSERT(theDataRow->coord_lon == expect_lon, "lon is correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_type == '/',"Symbol table correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_symbol == '/',"Symbol correct");
  TEST_ASSERT(theDataRow->aprs_symbol.special_overlay == '\0',"overlay null");
  TEST_ASSERT_STR_EQ("090",theDataRow->course,"Course correct");
  TEST_ASSERT_STR_EQ("  5",theDataRow->speed,"Speed correct");
  TEST_ASSERT_STR_EQ("30.48",theDataRow->altitude,"Altitude correct in meters");
  Create_object_item_tx_string(theDataRow,line,sizeof(line));
  // clobber the time with our standard fake time, don't worry about termination
  // coz it's in the middle of an existing string
  memcpy(&(line[11]),"111618z",7);
  TEST_ASSERT_STR_EQ(";TEST     *111618z3501.63N/10612.38W/090/005/A=000100",line,
                     "Object string correctly formatted");

  if (theDataRow)
    destroy_object_item_data_row(theDataRow);

  TEST_PASS("construct_object_item_data_row");

}
int test_constructor_simple_item_course_speed_alt(void)
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
                                            "100",         //altitude
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
  TEST_ASSERT_STR_EQ("TEST",theDataRow->call_sign,"Name populated correctly");
  TEST_ASSERT(theDataRow->flag & ST_ITEM, "Constructor creates item");
  TEST_ASSERT(theDataRow->flag & ST_ACTIVE, "Constructor creates active item");
  TEST_ASSERT(theDataRow->coord_lat == expect_lat, "lat is correct");
  TEST_ASSERT(theDataRow->coord_lon == expect_lon, "lon is correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_type == '/',"Symbol table correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_symbol == '/',"Symbol correct");
  TEST_ASSERT(theDataRow->aprs_symbol.special_overlay == '\0',"overlay null");
  TEST_ASSERT_STR_EQ("090",theDataRow->course,"Course correct");
  TEST_ASSERT_STR_EQ("  5",theDataRow->speed,"Speed correct");
  TEST_ASSERT_STR_EQ("30.48",theDataRow->altitude,"Altitude correct in meters");

  Create_object_item_tx_string(theDataRow,line,sizeof(line));
  TEST_ASSERT_STR_EQ(")TEST!3501.63N/10612.38W/090/005/A=000100",line,
                     "item string correctly formatted");

  if (theDataRow)
    destroy_object_item_data_row(theDataRow);

  TEST_PASS("construct_object_item_data_row");

}
int test_constructor_simple_object_course_speed_alt_comment(void)
{
  DataRow *theDataRow;
  long expect_lat = 90*60*60*100-(35*60+1.63)*60*100;
  long expect_lon = 180*60*60*100-(106*60+12.38)*60*100;
  char line[256];
  theDataRow=construct_object_item_data_row("TEST",  // name
                                            "3501.63N",
                                            "10612.38W", // lat/lon
                                            '/','/',    // group, symbol
                                            "A123456789012345678901234567890123456789012",         //comment
                                            "90","5",      //course, speed
                                            "100",         //altitude
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
  TEST_ASSERT_STR_EQ("TEST",theDataRow->call_sign,"Name populated correctly");
  TEST_ASSERT(theDataRow->flag & ST_OBJECT, "Constructor creates object");
  TEST_ASSERT(theDataRow->flag & ST_ACTIVE, "Constructor creates active object");
  TEST_ASSERT(theDataRow->coord_lat == expect_lat, "lat is correct");
  TEST_ASSERT(theDataRow->coord_lon == expect_lon, "lon is correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_type == '/',"Symbol table correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_symbol == '/',"Symbol correct");
  TEST_ASSERT(theDataRow->aprs_symbol.special_overlay == '\0',"overlay null");
  TEST_ASSERT_STR_EQ("090",theDataRow->course,"Course correct");
  TEST_ASSERT_STR_EQ("  5",theDataRow->speed,"Speed correct");
  TEST_ASSERT_STR_EQ("30.48",theDataRow->altitude,"Altitude correct in meters");
  TEST_ASSERT(theDataRow->comment_data,"Comment Data Pointer non-null");
  TEST_ASSERT(theDataRow->comment_data->text_ptr,"Comment text Pointer non-null");
  TEST_ASSERT_STR_EQ("A123456789012345678901234567890123456789012",theDataRow->comment_data->text_ptr,"Comment correctly stored");

  Create_object_item_tx_string(theDataRow,line,sizeof(line));
  // clobber the time with our standard fake time, don't worry about termination
  // coz it's in the middle of an existing string
  memcpy(&(line[11]),"111618z",7);
  TEST_ASSERT_STR_EQ(";TEST     *111618z3501.63N/10612.38W/090/005/A=000100A12345678901234567890123456",line,
                     "Object string correctly formatted");

  if (theDataRow)
    destroy_object_item_data_row(theDataRow);

  TEST_PASS("construct_object_item_data_row");

}
int test_constructor_simple_item_course_speed_alt_comment(void)
{
  DataRow *theDataRow;
  long expect_lat = 90*60*60*100-(35*60+1.63)*60*100;
  long expect_lon = 180*60*60*100-(106*60+12.38)*60*100;
  char line[256];
  theDataRow=construct_object_item_data_row("TEST",  // name
                                            "3501.63N",
                                            "10612.38W", // lat/lon
                                            '/','/',    // group, symbol
                                            "A123456789012345678901234567890123456789012",         //comment
                                            "90","5",      //course, speed
                                            "100",         //altitude
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
  TEST_ASSERT_STR_EQ("TEST",theDataRow->call_sign,"Name populated correctly");
  TEST_ASSERT(theDataRow->flag & ST_ITEM, "Constructor creates item");
  TEST_ASSERT(theDataRow->flag & ST_ACTIVE, "Constructor creates active item");
  TEST_ASSERT(theDataRow->coord_lat == expect_lat, "lat is correct");
  TEST_ASSERT(theDataRow->coord_lon == expect_lon, "lon is correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_type == '/',"Symbol table correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_symbol == '/',"Symbol correct");
  TEST_ASSERT(theDataRow->aprs_symbol.special_overlay == '\0',"overlay null");
  TEST_ASSERT_STR_EQ("090",theDataRow->course,"Course correct");
  TEST_ASSERT_STR_EQ("  5",theDataRow->speed,"Speed correct");
  TEST_ASSERT_STR_EQ("30.48",theDataRow->altitude,"Altitude correct in meters");
  TEST_ASSERT(theDataRow->comment_data,"Comment Data Pointer non-null");
  TEST_ASSERT(theDataRow->comment_data->text_ptr,"Comment text Pointer non-null");
  TEST_ASSERT_STR_EQ("A123456789012345678901234567890123456789012",theDataRow->comment_data->text_ptr,"Comment correctly stored");

  Create_object_item_tx_string(theDataRow,line,sizeof(line));
  TEST_ASSERT_STR_EQ(")TEST!3501.63N/10612.38W/090/005/A=000100A12345678901234567890123456",line,
                     "item string correctly formatted");

  if (theDataRow)
    destroy_object_item_data_row(theDataRow);


  TEST_PASS("construct_object_item_data_row");

}

// Area objects tests.
int test_constructor_area_object_basic(void)
{
  DataRow *theDataRow;
  // this is not *exactly* what convert_lon_s2l does, and so rounding can
  // make us a little off.  These coords chosen because rounding does NOT
  // bite us.
  long expect_lat = 90*60*60*100-(35*60+2.644)*60*100;
  long expect_lon = 180*60*60*100-((106*60+17.833)*60)*100;
  char line[256];

  theDataRow=construct_object_item_data_row("TEST",  // name
                                            "3502.644N",
                                            "10617.833W", // lat/lon
                                            '\\','l',    // group, symbol
                                            "",         //comment
                                            "","",      //course, speed
                                            "",         //altitude
                                            1,0,0,      //area, type, filled
                                            "/8",         // area color
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
  TEST_ASSERT_STR_EQ("TEST",theDataRow->call_sign,"Name populated correctly");
  TEST_ASSERT(theDataRow->flag & ST_OBJECT, "Constructor creates object");
  TEST_ASSERT(theDataRow->flag & ST_ACTIVE, "Constructor creates active object");
  TEST_ASSERT(theDataRow->coord_lat == expect_lat, "lat is correct");
  TEST_ASSERT(theDataRow->coord_lon == expect_lon, "lon is correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_type == '\\',"Symbol table correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_symbol == 'l',"Symbol correct");
  TEST_ASSERT(theDataRow->aprs_symbol.special_overlay == '\0',"overlay null");
  TEST_ASSERT_STR_EQ("",theDataRow->course,"Course correct");
  TEST_ASSERT_STR_EQ("",theDataRow->speed,"Speed correct");
  TEST_ASSERT_STR_EQ("",theDataRow->altitude,"Altitude correct in meters");

  Create_object_item_tx_string(theDataRow,line,sizeof(line));

  // clobber the time with our standard fake time, don't worry about termination
  // coz it's in the middle of an existing string
  memcpy(&(line[11]),"111618z",7);
  TEST_ASSERT_STR_EQ(";TEST     *111618z3502.64N\\10617.83Wl000/800",line,
                     "object string correctly formatted");

  if (theDataRow)
    destroy_object_item_data_row(theDataRow);


  TEST_PASS("construct_object_item_data_row");

}
int test_constructor_area_item_basic(void)
{
  DataRow *theDataRow;
  // this is not *exactly* what convert_lon_s2l does, and so rounding can
  // make us a little off.  These coords chosen because rounding does NOT
  // bite us.
  long expect_lat = 90*60*60*100-(35*60+2.644)*60*100;
  long expect_lon = 180*60*60*100-((106*60+17.833)*60)*100;
  char line[256];

  theDataRow=construct_object_item_data_row("TEST",  // name
                                            "3502.644N",
                                            "10617.833W", // lat/lon
                                            '\\','l',    // group, symbol
                                            "",         //comment
                                            "","",      //course, speed
                                            "",         //altitude
                                            1,0,0,      //area, type, filled
                                            "/8",         // area color
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
  TEST_ASSERT_STR_EQ("TEST",theDataRow->call_sign,"Name populated correctly");
  TEST_ASSERT(theDataRow->flag & ST_ITEM, "Constructor creates item");
  TEST_ASSERT(theDataRow->flag & ST_ACTIVE, "Constructor creates active item");
  TEST_ASSERT(theDataRow->coord_lat == expect_lat, "lat is correct");
  TEST_ASSERT(theDataRow->coord_lon == expect_lon, "lon is correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_type == '\\',"Symbol table correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_symbol == 'l',"Symbol correct");
  TEST_ASSERT(theDataRow->aprs_symbol.special_overlay == '\0',"overlay null");
  TEST_ASSERT_STR_EQ("",theDataRow->course,"Course correct");
  TEST_ASSERT_STR_EQ("",theDataRow->speed,"Speed correct");
  TEST_ASSERT_STR_EQ("",theDataRow->altitude,"Altitude correct in meters");

  Create_object_item_tx_string(theDataRow,line,sizeof(line));

  TEST_ASSERT_STR_EQ(")TEST!3502.64N\\10617.83Wl000/800",line,
                     "item string correctly formatted");

  if (theDataRow)
    destroy_object_item_data_row(theDataRow);


  TEST_PASS("construct_object_item_data_row");

}
int test_constructor_area_object_speed(void)
{
  DataRow *theDataRow;
  // this is not *exactly* what convert_lon_s2l does, and so rounding can
  // make us a little off.  These coords chosen because rounding does NOT
  // bite us.
  long expect_lat = 90*60*60*100-(35*60+2.644)*60*100;
  long expect_lon = 180*60*60*100-((106*60+17.833)*60)*100;
  char line[256];

  theDataRow=construct_object_item_data_row("TEST",  // name
                                            "3502.644N",
                                            "10617.833W", // lat/lon
                                            '\\','l',    // group, symbol
                                            "",         //comment
                                            "090","5",      //course, speed
                                            "",         //altitude
                                            1,0,0,      //area, type, filled
                                            "/8",         // area color
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
  TEST_ASSERT_STR_EQ("TEST",theDataRow->call_sign,"Name populated correctly");
  TEST_ASSERT(theDataRow->flag & ST_OBJECT, "Constructor creates object");
  TEST_ASSERT(theDataRow->flag & ST_ACTIVE, "Constructor creates active object");
  TEST_ASSERT(theDataRow->coord_lat == expect_lat, "lat is correct");
  TEST_ASSERT(theDataRow->coord_lon == expect_lon, "lon is correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_type == '\\',"Symbol table correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_symbol == 'l',"Symbol correct");
  TEST_ASSERT(theDataRow->aprs_symbol.special_overlay == '\0',"overlay null");
  // Area objects/items are not allowed to have course/speed, should get
  // zeroed out even if somehow the user has specified them.
  // The dialog doesn't actually allow them to enter it, but let's be sure
  // we handle correctly
  TEST_ASSERT_STR_EQ("",theDataRow->course,"Course correct");
  TEST_ASSERT_STR_EQ("",theDataRow->speed,"Speed correct");
  TEST_ASSERT_STR_EQ("",theDataRow->altitude,"Altitude correct in meters");

  Create_object_item_tx_string(theDataRow,line,sizeof(line));

  // clobber the time with our standard fake time, don't worry about termination
  // coz it's in the middle of an existing string
  memcpy(&(line[11]),"111618z",7);
  TEST_ASSERT_STR_EQ(";TEST     *111618z3502.64N\\10617.83Wl000/800",line,
                     "object string correctly formatted");

  if (theDataRow)
    destroy_object_item_data_row(theDataRow);


  TEST_PASS("construct_object_item_data_row");

}
int test_constructor_area_item_speed(void)
{
  DataRow *theDataRow;
  // this is not *exactly* what convert_lon_s2l does, and so rounding can
  // make us a little off.  These coords chosen because rounding does NOT
  // bite us.
  long expect_lat = 90*60*60*100-(35*60+2.644)*60*100;
  long expect_lon = 180*60*60*100-((106*60+17.833)*60)*100;
  char line[256];

  theDataRow=construct_object_item_data_row("TEST",  // name
                                            "3502.644N",
                                            "10617.833W", // lat/lon
                                            '\\','l',    // group, symbol
                                            "",         //comment
                                            "090","5",      //course, speed
                                            "",         //altitude
                                            1,0,0,      //area, type, filled
                                            "/8",         // area color
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
  TEST_ASSERT_STR_EQ("TEST",theDataRow->call_sign,"Name populated correctly");
  TEST_ASSERT(theDataRow->flag & ST_ITEM, "Constructor creates item");
  TEST_ASSERT(theDataRow->flag & ST_ACTIVE, "Constructor creates active item");
  TEST_ASSERT(theDataRow->coord_lat == expect_lat, "lat is correct");
  TEST_ASSERT(theDataRow->coord_lon == expect_lon, "lon is correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_type == '\\',"Symbol table correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_symbol == 'l',"Symbol correct");
  TEST_ASSERT(theDataRow->aprs_symbol.special_overlay == '\0',"overlay null");
  // Area objects/items are not allowed to have course/speed, should get
  // zeroed out even if somehow the user has specified them.
  // The dialog doesn't actually allow them to enter it, but let's be sure
  // we handle correctly
  TEST_ASSERT_STR_EQ("",theDataRow->course,"Course correct");
  TEST_ASSERT_STR_EQ("",theDataRow->speed,"Speed correct");
  TEST_ASSERT_STR_EQ("",theDataRow->altitude,"Altitude correct in meters");

  Create_object_item_tx_string(theDataRow,line,sizeof(line));

  TEST_ASSERT_STR_EQ(")TEST!3502.64N\\10617.83Wl000/800",line,
                     "item string correctly formatted");

  if (theDataRow)
    destroy_object_item_data_row(theDataRow);


  TEST_PASS("construct_object_item_data_row");

}
int test_constructor_area_object_offsets(void)
{
  DataRow *theDataRow;
  // this is not *exactly* what convert_lon_s2l does, and so rounding can
  // make us a little off.  These coords chosen because rounding does NOT
  // bite us.
  long expect_lat = 90*60*60*100-(35*60+2.644)*60*100;
  long expect_lon = 180*60*60*100-((106*60+17.833)*60)*100;
  char line[256];

  theDataRow=construct_object_item_data_row("TEST",  // name
                                            "3502.644N",
                                            "10617.833W", // lat/lon
                                            '\\','l',    // group, symbol
                                            "",         //comment
                                            "","",      //course, speed
                                            "",         //altitude
                                            1,0,0,      //area, type, filled
                                            "/8",         // area color
                                            "40","40",      // offsets
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
  TEST_ASSERT_STR_EQ("TEST",theDataRow->call_sign,"Name populated correctly");
  TEST_ASSERT(theDataRow->flag & ST_OBJECT, "Constructor creates object");
  TEST_ASSERT(theDataRow->flag & ST_ACTIVE, "Constructor creates active object");
  TEST_ASSERT(theDataRow->coord_lat == expect_lat, "lat is correct");
  TEST_ASSERT(theDataRow->coord_lon == expect_lon, "lon is correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_type == '\\',"Symbol table correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_symbol == 'l',"Symbol correct");
  TEST_ASSERT(theDataRow->aprs_symbol.special_overlay == '\0',"overlay null");
  TEST_ASSERT_STR_EQ("",theDataRow->course,"Course correct");
  TEST_ASSERT_STR_EQ("",theDataRow->speed,"Speed correct");
  TEST_ASSERT_STR_EQ("",theDataRow->altitude,"Altitude correct in meters");
  TEST_ASSERT(theDataRow->aprs_symbol.area_object.sqrt_lat_off == 6,"Correct lat offset value stored");
  TEST_ASSERT(theDataRow->aprs_symbol.area_object.sqrt_lon_off == 6,"Correct lon offset value stored");

  Create_object_item_tx_string(theDataRow,line,sizeof(line));

  // clobber the time with our standard fake time, don't worry about termination
  // coz it's in the middle of an existing string
  memcpy(&(line[11]),"111618z",7);
  TEST_ASSERT_STR_EQ(";TEST     *111618z3502.64N\\10617.83Wl006/806",line,
                     "object string correctly formatted");

  if (theDataRow)
    destroy_object_item_data_row(theDataRow);


  TEST_PASS("construct_object_item_data_row");

}
int test_constructor_area_item_offsets(void)
{
  DataRow *theDataRow;
  // this is not *exactly* what convert_lon_s2l does, and so rounding can
  // make us a little off.  These coords chosen because rounding does NOT
  // bite us.
  long expect_lat = 90*60*60*100-(35*60+2.644)*60*100;
  long expect_lon = 180*60*60*100-((106*60+17.833)*60)*100;
  char line[256];

  theDataRow=construct_object_item_data_row("TEST",  // name
                                            "3502.644N",
                                            "10617.833W", // lat/lon
                                            '\\','l',    // group, symbol
                                            "",         //comment
                                            "","",      //course, speed
                                            "",         //altitude
                                            1,0,0,      //area, type, filled
                                            "/8",         // area color
                                            "40","40",      // offsets
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
  TEST_ASSERT_STR_EQ("TEST",theDataRow->call_sign,"Name populated correctly");
  TEST_ASSERT(theDataRow->flag & ST_ITEM, "Constructor creates item");
  TEST_ASSERT(theDataRow->flag & ST_ACTIVE, "Constructor creates active item");
  TEST_ASSERT(theDataRow->coord_lat == expect_lat, "lat is correct");
  TEST_ASSERT(theDataRow->coord_lon == expect_lon, "lon is correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_type == '\\',"Symbol table correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_symbol == 'l',"Symbol correct");
  TEST_ASSERT(theDataRow->aprs_symbol.special_overlay == '\0',"overlay null");
  TEST_ASSERT_STR_EQ("",theDataRow->course,"Course correct");
  TEST_ASSERT_STR_EQ("",theDataRow->speed,"Speed correct");
  TEST_ASSERT_STR_EQ("",theDataRow->altitude,"Altitude correct in meters");
  TEST_ASSERT(theDataRow->aprs_symbol.area_object.sqrt_lat_off == 6,"Correct lat offset value stored");
  TEST_ASSERT(theDataRow->aprs_symbol.area_object.sqrt_lon_off == 6,"Correct lon offset value stored");

  Create_object_item_tx_string(theDataRow,line,sizeof(line));

  TEST_ASSERT_STR_EQ(")TEST!3502.64N\\10617.83Wl006/806",line,
                     "item string correctly formatted");

  if (theDataRow)
    destroy_object_item_data_row(theDataRow);


  TEST_PASS("construct_object_item_data_row");

}
int test_constructor_area_object_line_corridor(void)
{
  DataRow *theDataRow;
  // this is not *exactly* what convert_lon_s2l does, and so rounding can
  // make us a little off.  These coords chosen because rounding does NOT
  // bite us.
  long expect_lat = 90*60*60*100-(35*60+2.644)*60*100;
  long expect_lon = 180*60*60*100-((106*60+17.833)*60)*100;
  char line[256];

  theDataRow=construct_object_item_data_row("TEST",  // name
                                            "3502.644N",
                                            "10617.833W", // lat/lon
                                            '\\','l',    // group, symbol
                                            "",         //comment
                                            "","",      //course, speed
                                            "",         //altitude
                                            1,1,0,      //area, type, filled
                                            "/4",         // area color
                                            "40","40",      // offsets
                                            "1",         // corridor
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
  TEST_ASSERT_STR_EQ("TEST",theDataRow->call_sign,"Name populated correctly");
  TEST_ASSERT(theDataRow->flag & ST_OBJECT, "Constructor creates object");
  TEST_ASSERT(theDataRow->flag & ST_ACTIVE, "Constructor creates active object");
  TEST_ASSERT(theDataRow->coord_lat == expect_lat, "lat is correct");
  TEST_ASSERT(theDataRow->coord_lon == expect_lon, "lon is correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_type == '\\',"Symbol table correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_symbol == 'l',"Symbol correct");
  TEST_ASSERT(theDataRow->aprs_symbol.special_overlay == '\0',"overlay null");
  TEST_ASSERT_STR_EQ("",theDataRow->course,"Course correct");
  TEST_ASSERT_STR_EQ("",theDataRow->speed,"Speed correct");
  TEST_ASSERT_STR_EQ("",theDataRow->altitude,"Altitude correct in meters");
  TEST_ASSERT(theDataRow->aprs_symbol.area_object.sqrt_lat_off == 6,"Correct lat offset value stored");
  TEST_ASSERT(theDataRow->aprs_symbol.area_object.sqrt_lon_off == 6,"Correct lon offset value stored");
  TEST_ASSERT(theDataRow->aprs_symbol.area_object.corridor_width == 1,"Correct corridor width stored");

  Create_object_item_tx_string(theDataRow,line,sizeof(line));

  // clobber the time with our standard fake time, don't worry about termination
  // coz it's in the middle of an existing string
  memcpy(&(line[11]),"111618z",7);
  TEST_ASSERT_STR_EQ(";TEST     *111618z3502.64N\\10617.83Wl106/406{1}",line,
                     "object string correctly formatted");

  if (theDataRow)
    destroy_object_item_data_row(theDataRow);


  TEST_PASS("construct_object_item_data_row");

}
int test_constructor_area_item_line_corridor(void)
{
  DataRow *theDataRow;
  // this is not *exactly* what convert_lon_s2l does, and so rounding can
  // make us a little off.  These coords chosen because rounding does NOT
  // bite us.
  long expect_lat = 90*60*60*100-(35*60+2.644)*60*100;
  long expect_lon = 180*60*60*100-((106*60+17.833)*60)*100;
  char line[256];

  theDataRow=construct_object_item_data_row("TEST",  // name
                                            "3502.644N",
                                            "10617.833W", // lat/lon
                                            '\\','l',    // group, symbol
                                            "",         //comment
                                            "","",      //course, speed
                                            "",         //altitude
                                            1,1,0,      //area, type, filled
                                            "/4",         // area color
                                            "40","40",      // offsets
                                            "1",         // corridor
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
  TEST_ASSERT_STR_EQ("TEST",theDataRow->call_sign,"Name populated correctly");
  TEST_ASSERT(theDataRow->flag & ST_ITEM, "Constructor creates item");
  TEST_ASSERT(theDataRow->flag & ST_ACTIVE, "Constructor creates active item");
  TEST_ASSERT(theDataRow->coord_lat == expect_lat, "lat is correct");
  TEST_ASSERT(theDataRow->coord_lon == expect_lon, "lon is correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_type == '\\',"Symbol table correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_symbol == 'l',"Symbol correct");
  TEST_ASSERT(theDataRow->aprs_symbol.special_overlay == '\0',"overlay null");
  TEST_ASSERT_STR_EQ("",theDataRow->course,"Course correct");
  TEST_ASSERT_STR_EQ("",theDataRow->speed,"Speed correct");
  TEST_ASSERT_STR_EQ("",theDataRow->altitude,"Altitude correct in meters");
  TEST_ASSERT(theDataRow->aprs_symbol.area_object.sqrt_lat_off == 6,"Correct lat offset value stored");
  TEST_ASSERT(theDataRow->aprs_symbol.area_object.sqrt_lon_off == 6,"Correct lon offset value stored");
  TEST_ASSERT(theDataRow->aprs_symbol.area_object.corridor_width == 1,"Correct corridor width stored");

  Create_object_item_tx_string(theDataRow,line,sizeof(line));

  TEST_ASSERT_STR_EQ(")TEST!3502.64N\\10617.83Wl106/406{1}",line,
                     "item string correctly formatted");

  if (theDataRow)
    destroy_object_item_data_row(theDataRow);


  TEST_PASS("construct_object_item_data_row");

}

int test_constructor_area_object_offsets_filled(void)
{
  DataRow *theDataRow;
  // this is not *exactly* what convert_lon_s2l does, and so rounding can
  // make us a little off.  These coords chosen because rounding does NOT
  // bite us.
  long expect_lat = 90*60*60*100-(35*60+2.644)*60*100;
  long expect_lon = 180*60*60*100-((106*60+17.833)*60)*100;
  char line[256];

  theDataRow=construct_object_item_data_row("TEST",  // name
                                            "3502.644N",
                                            "10617.833W", // lat/lon
                                            '\\','l',    // group, symbol
                                            "",         //comment
                                            "","",      //course, speed
                                            "",         //altitude
                                            1,0,1,      //area, type, filled
                                            "/5",         // area color
                                            "40","40",      // offsets
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
  TEST_ASSERT_STR_EQ("TEST",theDataRow->call_sign,"Name populated correctly");
  TEST_ASSERT(theDataRow->flag & ST_OBJECT, "Constructor creates object");
  TEST_ASSERT(theDataRow->flag & ST_ACTIVE, "Constructor creates active object");
  TEST_ASSERT(theDataRow->coord_lat == expect_lat, "lat is correct");
  TEST_ASSERT(theDataRow->coord_lon == expect_lon, "lon is correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_type == '\\',"Symbol table correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_symbol == 'l',"Symbol correct");
  TEST_ASSERT(theDataRow->aprs_symbol.special_overlay == '\0',"overlay null");
  TEST_ASSERT_STR_EQ("",theDataRow->course,"Course correct");
  TEST_ASSERT_STR_EQ("",theDataRow->speed,"Speed correct");
  TEST_ASSERT_STR_EQ("",theDataRow->altitude,"Altitude correct in meters");
  TEST_ASSERT(theDataRow->aprs_symbol.area_object.sqrt_lat_off == 6,"Correct lat offset value stored");
  TEST_ASSERT(theDataRow->aprs_symbol.area_object.sqrt_lon_off == 6,"Correct lon offset value stored");

  Create_object_item_tx_string(theDataRow,line,sizeof(line));

  // clobber the time with our standard fake time, don't worry about termination
  // coz it's in the middle of an existing string
  memcpy(&(line[11]),"111618z",7);
  TEST_ASSERT_STR_EQ(";TEST     *111618z3502.64N\\10617.83Wl506/506",line,
                     "object string correctly formatted");

  if (theDataRow)
    destroy_object_item_data_row(theDataRow);


  TEST_PASS("construct_object_item_data_row");

}
int test_constructor_area_item_offsets_filled(void)
{
  DataRow *theDataRow;
  // this is not *exactly* what convert_lon_s2l does, and so rounding can
  // make us a little off.  These coords chosen because rounding does NOT
  // bite us.
  long expect_lat = 90*60*60*100-(35*60+2.644)*60*100;
  long expect_lon = 180*60*60*100-((106*60+17.833)*60)*100;
  char line[256];

  theDataRow=construct_object_item_data_row("TEST",  // name
                                            "3502.644N",
                                            "10617.833W", // lat/lon
                                            '\\','l',    // group, symbol
                                            "",         //comment
                                            "","",      //course, speed
                                            "",         //altitude
                                            1,0,1,      //area, type, filled
                                            "/5",         // area color
                                            "40","40",      // offsets
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
  TEST_ASSERT_STR_EQ("TEST",theDataRow->call_sign,"Name populated correctly");
  TEST_ASSERT(theDataRow->flag & ST_ITEM, "Constructor creates item");
  TEST_ASSERT(theDataRow->flag & ST_ACTIVE, "Constructor creates active item");
  TEST_ASSERT(theDataRow->coord_lat == expect_lat, "lat is correct");
  TEST_ASSERT(theDataRow->coord_lon == expect_lon, "lon is correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_type == '\\',"Symbol table correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_symbol == 'l',"Symbol correct");
  TEST_ASSERT(theDataRow->aprs_symbol.special_overlay == '\0',"overlay null");
  TEST_ASSERT_STR_EQ("",theDataRow->course,"Course correct");
  TEST_ASSERT_STR_EQ("",theDataRow->speed,"Speed correct");
  TEST_ASSERT_STR_EQ("",theDataRow->altitude,"Altitude correct in meters");
  TEST_ASSERT(theDataRow->aprs_symbol.area_object.sqrt_lat_off == 6,"Correct lat offset value stored");
  TEST_ASSERT(theDataRow->aprs_symbol.area_object.sqrt_lon_off == 6,"Correct lon offset value stored");

  Create_object_item_tx_string(theDataRow,line,sizeof(line));

  TEST_ASSERT_STR_EQ(")TEST!3502.64N\\10617.83Wl506/506",line,
                     "item string correctly formatted");

  if (theDataRow)
    destroy_object_item_data_row(theDataRow);


  TEST_PASS("construct_object_item_data_row");

}

int test_constructor_area_object_offsets_alt(void)
{
  DataRow *theDataRow;
  // this is not *exactly* what convert_lon_s2l does, and so rounding can
  // make us a little off.  These coords chosen because rounding does NOT
  // bite us.
  long expect_lat = 90*60*60*100-(35*60+2.644)*60*100;
  long expect_lon = 180*60*60*100-((106*60+17.833)*60)*100;
  char line[256];

  theDataRow=construct_object_item_data_row("TEST",  // name
                                            "3502.644N",
                                            "10617.833W", // lat/lon
                                            '\\','l',    // group, symbol
                                            "",         //comment
                                            "","",      //course, speed
                                            "100",         //altitude
                                            1,0,0,      //area, type, filled
                                            "/8",         // area color
                                            "40","40",      // offsets
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
  TEST_ASSERT_STR_EQ("TEST",theDataRow->call_sign,"Name populated correctly");
  TEST_ASSERT(theDataRow->flag & ST_OBJECT, "Constructor creates object");
  TEST_ASSERT(theDataRow->flag & ST_ACTIVE, "Constructor creates active object");
  TEST_ASSERT(theDataRow->coord_lat == expect_lat, "lat is correct");
  TEST_ASSERT(theDataRow->coord_lon == expect_lon, "lon is correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_type == '\\',"Symbol table correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_symbol == 'l',"Symbol correct");
  TEST_ASSERT(theDataRow->aprs_symbol.special_overlay == '\0',"overlay null");
  TEST_ASSERT_STR_EQ("",theDataRow->course,"Course correct");
  TEST_ASSERT_STR_EQ("",theDataRow->speed,"Speed correct");
  TEST_ASSERT_STR_EQ("30.48",theDataRow->altitude,"Altitude correct in meters");
  TEST_ASSERT(theDataRow->aprs_symbol.area_object.sqrt_lat_off == 6,"Correct lat offset value stored");
  TEST_ASSERT(theDataRow->aprs_symbol.area_object.sqrt_lon_off == 6,"Correct lon offset value stored");

  Create_object_item_tx_string(theDataRow,line,sizeof(line));

  // clobber the time with our standard fake time, don't worry about termination
  // coz it's in the middle of an existing string
  memcpy(&(line[11]),"111618z",7);
  TEST_ASSERT_STR_EQ(";TEST     *111618z3502.64N\\10617.83Wl006/806/A=000100",line,
                     "object string correctly formatted");

  if (theDataRow)
    destroy_object_item_data_row(theDataRow);


  TEST_PASS("construct_object_item_data_row");

}
int test_constructor_area_item_offsets_alt(void)
{
  DataRow *theDataRow;
  // this is not *exactly* what convert_lon_s2l does, and so rounding can
  // make us a little off.  These coords chosen because rounding does NOT
  // bite us.
  long expect_lat = 90*60*60*100-(35*60+2.644)*60*100;
  long expect_lon = 180*60*60*100-((106*60+17.833)*60)*100;
  char line[256];

  theDataRow=construct_object_item_data_row("TEST",  // name
                                            "3502.644N",
                                            "10617.833W", // lat/lon
                                            '\\','l',    // group, symbol
                                            "",         //comment
                                            "","",      //course, speed
                                            "100",         //altitude
                                            1,0,0,      //area, type, filled
                                            "/8",         // area color
                                            "40","40",      // offsets
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
  TEST_ASSERT_STR_EQ("TEST",theDataRow->call_sign,"Name populated correctly");
  TEST_ASSERT(theDataRow->flag & ST_ITEM, "Constructor creates item");
  TEST_ASSERT(theDataRow->flag & ST_ACTIVE, "Constructor creates active item");
  TEST_ASSERT(theDataRow->coord_lat == expect_lat, "lat is correct");
  TEST_ASSERT(theDataRow->coord_lon == expect_lon, "lon is correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_type == '\\',"Symbol table correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_symbol == 'l',"Symbol correct");
  TEST_ASSERT(theDataRow->aprs_symbol.special_overlay == '\0',"overlay null");
  TEST_ASSERT_STR_EQ("",theDataRow->course,"Course correct");
  TEST_ASSERT_STR_EQ("",theDataRow->speed,"Speed correct");
  TEST_ASSERT_STR_EQ("30.48",theDataRow->altitude,"Altitude correct in meters");
  TEST_ASSERT(theDataRow->aprs_symbol.area_object.sqrt_lat_off == 6,"Correct lat offset value stored");
  TEST_ASSERT(theDataRow->aprs_symbol.area_object.sqrt_lon_off == 6,"Correct lon offset value stored");

  Create_object_item_tx_string(theDataRow,line,sizeof(line));

  TEST_ASSERT_STR_EQ(")TEST!3502.64N\\10617.83Wl006/806/A=000100",line,
                     "item string correctly formatted");

  if (theDataRow)
    destroy_object_item_data_row(theDataRow);


  TEST_PASS("construct_object_item_data_row");

}

// Tests for signpost objects
// Fortunately, there are only a couple of variants here, and the only
// thing special about these objects is that their symbol must be a specific
// one and they may have a 3-character bit of signpost data
//
int test_constructor_area_object_signpost_basic(void)
{
  DataRow *theDataRow;
  // this is not *exactly* what convert_lon_s2l does, and so rounding can
  // make us a little off.  These coords chosen because rounding does NOT
  // bite us.
  long expect_lat = 90*60*60*100-(35*60+2.644)*60*100;
  long expect_lon = 180*60*60*100-((106*60+17.833)*60)*100;
  char line[256];

  theDataRow=construct_object_item_data_row("TEST",  // name
                                            "3502.644N",
                                            "10617.833W", // lat/lon
                                            '\\','m',    // group, symbol
                                            "",         //comment
                                            "","",      //course, speed
                                            "",         //altitude
                                            0,0,0,      //area, type, filled
                                            "",         // area color
                                            "","",      // offsets
                                            "",         // corridor
                                            1,          // signpost
                                            "111",         // signpost string
                                            0, 0, 0,    // df, omni, beam
                                            "",         // shgd
                                            "",         //bearing
                                            "",         // NRQ
                                            0,          // prob circles
                                            "","",      // prob min, max
                                            1,          // is_object
                                            0);         // killed
  TEST_ASSERT(theDataRow, "Constructor returns valid pointer appropriately");
  TEST_ASSERT_STR_EQ("TEST",theDataRow->call_sign,"Name populated correctly");
  TEST_ASSERT(theDataRow->flag & ST_OBJECT, "Constructor creates object");
  TEST_ASSERT(theDataRow->flag & ST_ACTIVE, "Constructor creates active object");
  TEST_ASSERT(theDataRow->coord_lat == expect_lat, "lat is correct");
  TEST_ASSERT(theDataRow->coord_lon == expect_lon, "lon is correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_type == '\\',"Symbol table correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_symbol == 'm',"Symbol correct");
  TEST_ASSERT(theDataRow->aprs_symbol.special_overlay == '\0',"overlay null");
  TEST_ASSERT_STR_EQ("",theDataRow->course,"Course correct");
  TEST_ASSERT_STR_EQ("",theDataRow->speed,"Speed correct");
  TEST_ASSERT_STR_EQ("",theDataRow->altitude,"Altitude correct in meters");
  TEST_ASSERT_STR_EQ("111",theDataRow->signpost,"Signpost data stored properly.");
  Create_object_item_tx_string(theDataRow,line,sizeof(line));

  // clobber the time with our standard fake time, don't worry about termination
  // coz it's in the middle of an existing string
  memcpy(&(line[11]),"111618z",7);
  TEST_ASSERT_STR_EQ(";TEST     *111618z3502.64N\\10617.83Wm{111}",line,
                     "object string correctly formatted");

  if (theDataRow)
    destroy_object_item_data_row(theDataRow);


  TEST_PASS("construct_object_item_data_row");

}
int test_constructor_area_item_signpost_basic(void)
{
  DataRow *theDataRow;
  // this is not *exactly* what convert_lon_s2l does, and so rounding can
  // make us a little off.  These coords chosen because rounding does NOT
  // bite us.
  long expect_lat = 90*60*60*100-(35*60+2.644)*60*100;
  long expect_lon = 180*60*60*100-((106*60+17.833)*60)*100;
  char line[256];

  theDataRow=construct_object_item_data_row("TEST",  // name
                                            "3502.644N",
                                            "10617.833W", // lat/lon
                                            '\\','m',    // group, symbol
                                            "",         //comment
                                            "","",      //course, speed
                                            "",         //altitude
                                            0,0,0,      //area, type, filled
                                            "",         // area color
                                            "","",      // offsets
                                            "",         // corridor
                                            1,          // signpost
                                            "111",         // signpost string
                                            0, 0, 0,    // df, omni, beam
                                            "",         // shgd
                                            "",         //bearing
                                            "",         // NRQ
                                            0,          // prob circles
                                            "","",      // prob min, max
                                            0,          // is_object
                                            0);         // killed
  TEST_ASSERT(theDataRow, "Constructor returns valid pointer appropriately");
  TEST_ASSERT_STR_EQ("TEST",theDataRow->call_sign,"Name populated correctly");
  TEST_ASSERT(theDataRow->flag & ST_ITEM, "Constructor creates item");
  TEST_ASSERT(theDataRow->flag & ST_ACTIVE, "Constructor creates active item");
  TEST_ASSERT(theDataRow->coord_lat == expect_lat, "lat is correct");
  TEST_ASSERT(theDataRow->coord_lon == expect_lon, "lon is correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_type == '\\',"Symbol table correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_symbol == 'm',"Symbol correct");
  TEST_ASSERT(theDataRow->aprs_symbol.special_overlay == '\0',"overlay null");
  TEST_ASSERT_STR_EQ("",theDataRow->course,"Course correct");
  TEST_ASSERT_STR_EQ("",theDataRow->speed,"Speed correct");
  TEST_ASSERT_STR_EQ("",theDataRow->altitude,"Altitude correct in meters");
  TEST_ASSERT_STR_EQ("111",theDataRow->signpost,"Signpost data stored properly.");

  Create_object_item_tx_string(theDataRow,line,sizeof(line));

  TEST_ASSERT_STR_EQ(")TEST!3502.64N\\10617.83Wm{111}",line,
                     "item string correctly formatted");

  if (theDataRow)
    destroy_object_item_data_row(theDataRow);


  TEST_PASS("construct_object_item_data_row");

}
int test_constructor_area_object_signpost_speed(void)
{
  DataRow *theDataRow;
  // this is not *exactly* what convert_lon_s2l does, and so rounding can
  // make us a little off.  These coords chosen because rounding does NOT
  // bite us.
  long expect_lat = 90*60*60*100-(35*60+2.644)*60*100;
  long expect_lon = 180*60*60*100-((106*60+17.833)*60)*100;
  char line[256];

  theDataRow=construct_object_item_data_row("TEST",  // name
                                            "3502.644N",
                                            "10617.833W", // lat/lon
                                            '\\','m',    // group, symbol
                                            "",         //comment
                                            "90","5",      //course, speed
                                            "",         //altitude
                                            0,0,0,      //area, type, filled
                                            "",         // area color
                                            "","",      // offsets
                                            "",         // corridor
                                            1,          // signpost
                                            "111",         // signpost string
                                            0, 0, 0,    // df, omni, beam
                                            "",         // shgd
                                            "",         //bearing
                                            "",         // NRQ
                                            0,          // prob circles
                                            "","",      // prob min, max
                                            1,          // is_object
                                            0);         // killed
  TEST_ASSERT(theDataRow, "Constructor returns valid pointer appropriately");
  TEST_ASSERT_STR_EQ("TEST",theDataRow->call_sign,"Name populated correctly");
  TEST_ASSERT(theDataRow->flag & ST_OBJECT, "Constructor creates object");
  TEST_ASSERT(theDataRow->flag & ST_ACTIVE, "Constructor creates active object");
  TEST_ASSERT(theDataRow->coord_lat == expect_lat, "lat is correct");
  TEST_ASSERT(theDataRow->coord_lon == expect_lon, "lon is correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_type == '\\',"Symbol table correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_symbol == 'm',"Symbol correct");
  TEST_ASSERT(theDataRow->aprs_symbol.special_overlay == '\0',"overlay null");
  TEST_ASSERT_STR_EQ("090",theDataRow->course,"Course correct");
  TEST_ASSERT_STR_EQ("  5",theDataRow->speed,"Speed correct");
  TEST_ASSERT_STR_EQ("",theDataRow->altitude,"Altitude correct in meters");
  TEST_ASSERT_STR_EQ("111",theDataRow->signpost,"Signpost data stored properly.");
  Create_object_item_tx_string(theDataRow,line,sizeof(line));

  // clobber the time with our standard fake time, don't worry about termination
  // coz it's in the middle of an existing string
  memcpy(&(line[11]),"111618z",7);
  TEST_ASSERT_STR_EQ(";TEST     *111618z3502.64N\\10617.83Wm090/005{111}",line,
                     "object string correctly formatted");

  if (theDataRow)
    destroy_object_item_data_row(theDataRow);


  TEST_PASS("construct_object_item_data_row");

}
int test_constructor_area_item_signpost_speed(void)
{
  DataRow *theDataRow;
  // this is not *exactly* what convert_lon_s2l does, and so rounding can
  // make us a little off.  These coords chosen because rounding does NOT
  // bite us.
  long expect_lat = 90*60*60*100-(35*60+2.644)*60*100;
  long expect_lon = 180*60*60*100-((106*60+17.833)*60)*100;
  char line[256];

  theDataRow=construct_object_item_data_row("TEST",  // name
                                            "3502.644N",
                                            "10617.833W", // lat/lon
                                            '\\','m',    // group, symbol
                                            "",         //comment
                                            "90","5",      //course, speed
                                            "",         //altitude
                                            0,0,0,      //area, type, filled
                                            "",         // area color
                                            "","",      // offsets
                                            "",         // corridor
                                            1,          // signpost
                                            "111",         // signpost string
                                            0, 0, 0,    // df, omni, beam
                                            "",         // shgd
                                            "",         //bearing
                                            "",         // NRQ
                                            0,          // prob circles
                                            "","",      // prob min, max
                                            0,          // is_object
                                            0);         // killed
  TEST_ASSERT(theDataRow, "Constructor returns valid pointer appropriately");
  TEST_ASSERT_STR_EQ("TEST",theDataRow->call_sign,"Name populated correctly");
  TEST_ASSERT(theDataRow->flag & ST_ITEM, "Constructor creates item");
  TEST_ASSERT(theDataRow->flag & ST_ACTIVE, "Constructor creates active item");
  TEST_ASSERT(theDataRow->coord_lat == expect_lat, "lat is correct");
  TEST_ASSERT(theDataRow->coord_lon == expect_lon, "lon is correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_type == '\\',"Symbol table correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_symbol == 'm',"Symbol correct");
  TEST_ASSERT(theDataRow->aprs_symbol.special_overlay == '\0',"overlay null");
  TEST_ASSERT_STR_EQ("090",theDataRow->course,"Course correct");
  TEST_ASSERT_STR_EQ("  5",theDataRow->speed,"Speed correct");
  TEST_ASSERT_STR_EQ("",theDataRow->altitude,"Altitude correct in meters");
  TEST_ASSERT_STR_EQ("111",theDataRow->signpost,"Signpost data stored properly.");

  Create_object_item_tx_string(theDataRow,line,sizeof(line));

  TEST_ASSERT_STR_EQ(")TEST!3502.64N\\10617.83Wm090/005{111}",line,
                     "item string correctly formatted");

  if (theDataRow)
    destroy_object_item_data_row(theDataRow);


  TEST_PASS("construct_object_item_data_row");

}

// Omni DF objects
int test_constructor_object_df_omni(void)
{
  DataRow *theDataRow;
  // this is not *exactly* what convert_lon_s2l does, and so rounding can
  // make us a little off.  These coords chosen because rounding does NOT
  // bite us.
  long expect_lat = 90*60*60*100-(35*60+2.644)*60*100;
  long expect_lon = 180*60*60*100-((106*60+17.833)*60)*100;
  char line[256];

  theDataRow=construct_object_item_data_row("TEST",  // name
                                            "3502.644N",
                                            "10617.833W", // lat/lon
                                            '/','\\',    // group, symbol
                                            "",         //comment
                                            "","",      //course, speed
                                            "",         //altitude
                                            0,0,0,      //area, type, filled
                                            "",         // area color
                                            "","",      // offsets
                                            "",         // corridor
                                            0,          // signpost
                                            "",         // signpost string
                                            1, 1, 0,    // df, omni, beam
                                            "4133",         // shgd
                                            "",         //bearing
                                            "",         // NRQ
                                            0,          // prob circles
                                            "","",      // prob min, max
                                            1,          // is_object
                                            0);         // killed
  TEST_ASSERT(theDataRow, "Constructor returns valid pointer appropriately");
  TEST_ASSERT_STR_EQ("TEST",theDataRow->call_sign,"Name populated correctly");
  TEST_ASSERT(theDataRow->flag & ST_OBJECT, "Constructor creates object");
  TEST_ASSERT(theDataRow->flag & ST_ACTIVE, "Constructor creates active object");
  TEST_ASSERT(theDataRow->coord_lat == expect_lat, "lat is correct");
  TEST_ASSERT(theDataRow->coord_lon == expect_lon, "lon is correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_type == '/',"Symbol table correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_symbol == '\\',"Symbol correct");
  TEST_ASSERT(theDataRow->aprs_symbol.special_overlay == '\0',"overlay null");
  TEST_ASSERT_STR_EQ("",theDataRow->course,"Course correct");
  TEST_ASSERT_STR_EQ("",theDataRow->speed,"Speed correct");
  TEST_ASSERT_STR_EQ("",theDataRow->altitude,"Altitude correct in meters");
  TEST_ASSERT_STR_EQ("DFS4133",theDataRow->signal_gain, "signal/gain correct");

  Create_object_item_tx_string(theDataRow,line,sizeof(line));

  // clobber the time with our standard fake time, don't worry about termination
  // coz it's in the middle of an existing string
  memcpy(&(line[11]),"111618z",7);
  TEST_ASSERT_STR_EQ(";TEST     *111618z3502.64N/10617.83W\\DFS4133/",line,
                     "object string correctly formatted");

  if (theDataRow)
    destroy_object_item_data_row(theDataRow);


  TEST_PASS("construct_object_item_data_row");

}
int test_constructor_item_df_omni(void)
{
  DataRow *theDataRow;
  // this is not *exactly* what convert_lon_s2l does, and so rounding can
  // make us a little off.  These coords chosen because rounding does NOT
  // bite us.
  long expect_lat = 90*60*60*100-(35*60+2.644)*60*100;
  long expect_lon = 180*60*60*100-((106*60+17.833)*60)*100;
  char line[256];

  theDataRow=construct_object_item_data_row("TEST",  // name
                                            "3502.644N",
                                            "10617.833W", // lat/lon
                                            '/','\\',    // group, symbol
                                            "",         //comment
                                            "","",      //course, speed
                                            "",         //altitude
                                            0,0,0,      //area, type, filled
                                            "",         // area color
                                            "","",      // offsets
                                            "",         // corridor
                                            0,          // signpost
                                            "",         // signpost string
                                            1, 1, 0,    // df, omni, beam
                                            "4133",         // shgd
                                            "",         //bearing
                                            "",         // NRQ
                                            0,          // prob circles
                                            "","",      // prob min, max
                                            0,          // is_object
                                            0);         // killed
  TEST_ASSERT(theDataRow, "Constructor returns valid pointer appropriately");
  TEST_ASSERT_STR_EQ("TEST",theDataRow->call_sign,"Name populated correctly");
  TEST_ASSERT(theDataRow->flag & ST_ITEM, "Constructor creates item");
  TEST_ASSERT(theDataRow->flag & ST_ACTIVE, "Constructor creates active item");
  TEST_ASSERT(theDataRow->coord_lat == expect_lat, "lat is correct");
  TEST_ASSERT(theDataRow->coord_lon == expect_lon, "lon is correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_type == '/',"Symbol table correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_symbol == '\\',"Symbol correct");
  TEST_ASSERT(theDataRow->aprs_symbol.special_overlay == '\0',"overlay null");
  TEST_ASSERT_STR_EQ("",theDataRow->course,"Course correct");
  TEST_ASSERT_STR_EQ("",theDataRow->speed,"Speed correct");
  TEST_ASSERT_STR_EQ("",theDataRow->altitude,"Altitude correct in meters");
  TEST_ASSERT_STR_EQ("DFS4133",theDataRow->signal_gain, "signal/gain correct");

  Create_object_item_tx_string(theDataRow,line,sizeof(line));

  TEST_ASSERT_STR_EQ(")TEST!3502.64N/10617.83W\\DFS4133/",line,
                     "item string correctly formatted");

  if (theDataRow)
    destroy_object_item_data_row(theDataRow);


  TEST_PASS("construct_object_item_data_row");

}
// beam DF objects
int test_constructor_object_df_beam(void)
{
  DataRow *theDataRow;
  // this is not *exactly* what convert_lon_s2l does, and so rounding can
  // make us a little off.  These coords chosen because rounding does NOT
  // bite us.
  long expect_lat = 90*60*60*100-(35*60+2.644)*60*100;
  long expect_lon = 180*60*60*100-((106*60+17.833)*60)*100;
  char line[256];

  theDataRow=construct_object_item_data_row("TEST",  // name
                                            "3502.644N",
                                            "10617.833W", // lat/lon
                                            '/','\\',    // group, symbol
                                            "",         //comment
                                            "","",      //course, speed
                                            "",         //altitude
                                            0,0,0,      //area, type, filled
                                            "",         // area color
                                            "","",      // offsets
                                            "",         // corridor
                                            0,          // signpost
                                            "",         // signpost string
                                            1, 0, 1,    // df, omni, beam
                                            "",         // shgd
                                            "140",         //bearing
                                            "965",         // NRQ
                                            0,          // prob circles
                                            "","",      // prob min, max
                                            1,          // is_object
                                            0);         // killed
  TEST_ASSERT(theDataRow, "Constructor returns valid pointer appropriately");
  TEST_ASSERT_STR_EQ("TEST",theDataRow->call_sign,"Name populated correctly");
  TEST_ASSERT(theDataRow->flag & ST_OBJECT, "Constructor creates object");
  TEST_ASSERT(theDataRow->flag & ST_ACTIVE, "Constructor creates active object");
  TEST_ASSERT(theDataRow->coord_lat == expect_lat, "lat is correct");
  TEST_ASSERT(theDataRow->coord_lon == expect_lon, "lon is correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_type == '/',"Symbol table correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_symbol == '\\',"Symbol correct");
  TEST_ASSERT(theDataRow->aprs_symbol.special_overlay == '\0',"overlay null");
  TEST_ASSERT_STR_EQ("",theDataRow->course,"Course correct");
  TEST_ASSERT_STR_EQ("",theDataRow->speed,"Speed correct");
  TEST_ASSERT_STR_EQ("",theDataRow->altitude,"Altitude correct in meters");
  TEST_ASSERT_STR_EQ("140",theDataRow->bearing, "bearing correct");
  TEST_ASSERT_STR_EQ("965",theDataRow->NRQ, "NRQ correct");

  Create_object_item_tx_string(theDataRow,line,sizeof(line));

  // clobber the time with our standard fake time, don't worry about termination
  // coz it's in the middle of an existing string
  memcpy(&(line[11]),"111618z",7);
  TEST_ASSERT_STR_EQ(";TEST     *111618z3502.64N/10617.83W\\000/000/140/965",line,
                     "object string correctly formatted");

  if (theDataRow)
    destroy_object_item_data_row(theDataRow);


  TEST_PASS("construct_object_item_data_row");

}
int test_constructor_item_df_beam(void)
{
  DataRow *theDataRow;
  // this is not *exactly* what convert_lon_s2l does, and so rounding can
  // make us a little off.  These coords chosen because rounding does NOT
  // bite us.
  long expect_lat = 90*60*60*100-(35*60+2.644)*60*100;
  long expect_lon = 180*60*60*100-((106*60+17.833)*60)*100;
  char line[256];

  theDataRow=construct_object_item_data_row("TEST",  // name
                                            "3502.644N",
                                            "10617.833W", // lat/lon
                                            '/','\\',    // group, symbol
                                            "",         //comment
                                            "","",      //course, speed
                                            "",         //altitude
                                            0,0,0,      //area, type, filled
                                            "",         // area color
                                            "","",      // offsets
                                            "",         // corridor
                                            0,          // signpost
                                            "",         // signpost string
                                            1, 0, 1,    // df, omni, beam
                                            "",         // shgd
                                            "140",         //bearing
                                            "965",         // NRQ
                                            0,          // prob circles
                                            "","",      // prob min, max
                                            0,          // is_object
                                            0);         // killed
  TEST_ASSERT(theDataRow, "Constructor returns valid pointer appropriately");
  TEST_ASSERT_STR_EQ("TEST",theDataRow->call_sign,"Name populated correctly");
  TEST_ASSERT(theDataRow->flag & ST_ITEM, "Constructor creates item");
  TEST_ASSERT(theDataRow->flag & ST_ACTIVE, "Constructor creates active item");
  TEST_ASSERT(theDataRow->coord_lat == expect_lat, "lat is correct");
  TEST_ASSERT(theDataRow->coord_lon == expect_lon, "lon is correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_type == '/',"Symbol table correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_symbol == '\\',"Symbol correct");
  TEST_ASSERT(theDataRow->aprs_symbol.special_overlay == '\0',"overlay null");
  TEST_ASSERT_STR_EQ("",theDataRow->course,"Course correct");
  TEST_ASSERT_STR_EQ("",theDataRow->speed,"Speed correct");
  TEST_ASSERT_STR_EQ("",theDataRow->altitude,"Altitude correct in meters");
  TEST_ASSERT_STR_EQ("140",theDataRow->bearing, "bearing correct");
  TEST_ASSERT_STR_EQ("965",theDataRow->NRQ, "NRQ correct");

  Create_object_item_tx_string(theDataRow,line,sizeof(line));

  TEST_ASSERT_STR_EQ(")TEST!3502.64N/10617.83W\\000/000/140/965",line,
                     "item string correctly formatted");

  if (theDataRow)
    destroy_object_item_data_row(theDataRow);


  TEST_PASS("construct_object_item_data_row");

}


// Tests for probability circles
// Probability circles are just like normal objects but they get a
// bit of Xastir-specific text added to their comments reflecting radii.
// beam DF objects
int test_constructor_object_prob_circles_noring(void)
{
  DataRow *theDataRow;
  // this is not *exactly* what convert_lon_s2l does, and so rounding can
  // make us a little off.  These coords chosen because rounding does NOT
  // bite us.
  long expect_lat = 90*60*60*100-(35*60+2.644)*60*100;
  long expect_lon = 180*60*60*100-((106*60+17.833)*60)*100;
  char line[256];

  theDataRow=construct_object_item_data_row("TEST",  // name
                                            "3502.644N",
                                            "10617.833W", // lat/lon
                                            '/','[',    // group, symbol
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
                                            1,          // prob circles
                                            "","",      // prob min, max
                                            1,          // is_object
                                            0);         // killed
  TEST_ASSERT(theDataRow, "Constructor returns valid pointer appropriately");
  TEST_ASSERT_STR_EQ("TEST",theDataRow->call_sign,"Name populated correctly");
  TEST_ASSERT(theDataRow->flag & ST_OBJECT, "Constructor creates object");
  TEST_ASSERT(theDataRow->flag & ST_ACTIVE, "Constructor creates active object");
  TEST_ASSERT(theDataRow->coord_lat == expect_lat, "lat is correct");
  TEST_ASSERT(theDataRow->coord_lon == expect_lon, "lon is correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_type == '/',"Symbol table correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_symbol == '[',"Symbol correct");
  TEST_ASSERT(theDataRow->aprs_symbol.special_overlay == '\0',"overlay null");
  TEST_ASSERT_STR_EQ("",theDataRow->course,"Course correct");
  TEST_ASSERT_STR_EQ("",theDataRow->speed,"Speed correct");
  TEST_ASSERT_STR_EQ("",theDataRow->altitude,"Altitude correct in meters");
  TEST_ASSERT_STR_EQ("",theDataRow->probability_min,"Probability min correct");
  TEST_ASSERT_STR_EQ("",theDataRow->probability_max,"Probability max correct");

  Create_object_item_tx_string(theDataRow,line,sizeof(line));

  // clobber the time with our standard fake time, don't worry about termination
  // coz it's in the middle of an existing string
  memcpy(&(line[11]),"111618z",7);
  TEST_ASSERT_STR_EQ(";TEST     *111618z3502.64N/10617.83W[",line,
                     "object string correctly formatted");

  if (theDataRow)
    destroy_object_item_data_row(theDataRow);


  TEST_PASS("construct_object_item_data_row");

}
int test_constructor_item_prob_circles_noring(void)
{
  DataRow *theDataRow;
  // this is not *exactly* what convert_lon_s2l does, and so rounding can
  // make us a little off.  These coords chosen because rounding does NOT
  // bite us.
  long expect_lat = 90*60*60*100-(35*60+2.644)*60*100;
  long expect_lon = 180*60*60*100-((106*60+17.833)*60)*100;
  char line[256];

  theDataRow=construct_object_item_data_row("TEST",  // name
                                            "3502.644N",
                                            "10617.833W", // lat/lon
                                            '/','[',    // group, symbol
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
                                            1,          // prob circles
                                            "","",      // prob min, max
                                            0,          // is_object
                                            0);         // killed
  TEST_ASSERT(theDataRow, "Constructor returns valid pointer appropriately");
  TEST_ASSERT_STR_EQ("TEST",theDataRow->call_sign,"Name populated correctly");
  TEST_ASSERT(theDataRow->flag & ST_ITEM, "Constructor creates item");
  TEST_ASSERT(theDataRow->flag & ST_ACTIVE, "Constructor creates active item");
  TEST_ASSERT(theDataRow->coord_lat == expect_lat, "lat is correct");
  TEST_ASSERT(theDataRow->coord_lon == expect_lon, "lon is correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_type == '/',"Symbol table correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_symbol == '[',"Symbol correct");
  TEST_ASSERT(theDataRow->aprs_symbol.special_overlay == '\0',"overlay null");
  TEST_ASSERT_STR_EQ("",theDataRow->course,"Course correct");
  TEST_ASSERT_STR_EQ("",theDataRow->speed,"Speed correct");
  TEST_ASSERT_STR_EQ("",theDataRow->altitude,"Altitude correct in meters");
  TEST_ASSERT_STR_EQ("",theDataRow->probability_min,"Probability min correct");
  TEST_ASSERT_STR_EQ("",theDataRow->probability_max,"Probability max correct");

  Create_object_item_tx_string(theDataRow,line,sizeof(line));

  TEST_ASSERT_STR_EQ(")TEST!3502.64N/10617.83W[",line,
                     "item string correctly formatted");

  if (theDataRow)
    destroy_object_item_data_row(theDataRow);


  TEST_PASS("construct_object_item_data_row");

}
int test_constructor_object_prob_circles_minring(void)
{
  DataRow *theDataRow;
  // this is not *exactly* what convert_lon_s2l does, and so rounding can
  // make us a little off.  These coords chosen because rounding does NOT
  // bite us.
  long expect_lat = 90*60*60*100-(35*60+2.644)*60*100;
  long expect_lon = 180*60*60*100-((106*60+17.833)*60)*100;
  char line[256];

  theDataRow=construct_object_item_data_row("TEST",  // name
                                            "3502.644N",
                                            "10617.833W", // lat/lon
                                            '/','[',    // group, symbol
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
                                            1,          // prob circles
                                            "1","",      // prob min, max
                                            1,          // is_object
                                            0);         // killed
  TEST_ASSERT(theDataRow, "Constructor returns valid pointer appropriately");
  TEST_ASSERT_STR_EQ("TEST",theDataRow->call_sign,"Name populated correctly");
  TEST_ASSERT(theDataRow->flag & ST_OBJECT, "Constructor creates object");
  TEST_ASSERT(theDataRow->flag & ST_ACTIVE, "Constructor creates active object");
  TEST_ASSERT(theDataRow->coord_lat == expect_lat, "lat is correct");
  TEST_ASSERT(theDataRow->coord_lon == expect_lon, "lon is correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_type == '/',"Symbol table correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_symbol == '[',"Symbol correct");
  TEST_ASSERT(theDataRow->aprs_symbol.special_overlay == '\0',"overlay null");
  TEST_ASSERT_STR_EQ("",theDataRow->course,"Course correct");
  TEST_ASSERT_STR_EQ("",theDataRow->speed,"Speed correct");
  TEST_ASSERT_STR_EQ("",theDataRow->altitude,"Altitude correct in meters");
  TEST_ASSERT_STR_EQ("1",theDataRow->probability_min,"Probability min correct");
  TEST_ASSERT_STR_EQ("",theDataRow->probability_max,"Probability max correct");

  Create_object_item_tx_string(theDataRow,line,sizeof(line));

  // clobber the time with our standard fake time, don't worry about termination
  // coz it's in the middle of an existing string
  memcpy(&(line[11]),"111618z",7);
  TEST_ASSERT_STR_EQ(";TEST     *111618z3502.64N/10617.83W[Pmin1,",line,
                     "object string correctly formatted");

  if (theDataRow)
    destroy_object_item_data_row(theDataRow);


  TEST_PASS("construct_object_item_data_row");

}
int test_constructor_item_prob_circles_minring(void)
{
  DataRow *theDataRow;
  // this is not *exactly* what convert_lon_s2l does, and so rounding can
  // make us a little off.  These coords chosen because rounding does NOT
  // bite us.
  long expect_lat = 90*60*60*100-(35*60+2.644)*60*100;
  long expect_lon = 180*60*60*100-((106*60+17.833)*60)*100;
  char line[256];

  theDataRow=construct_object_item_data_row("TEST",  // name
                                            "3502.644N",
                                            "10617.833W", // lat/lon
                                            '/','[',    // group, symbol
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
                                            1,          // prob circles
                                            "1","",      // prob min, max
                                            0,          // is_object
                                            0);         // killed
  TEST_ASSERT(theDataRow, "Constructor returns valid pointer appropriately");
  TEST_ASSERT_STR_EQ("TEST",theDataRow->call_sign,"Name populated correctly");
  TEST_ASSERT(theDataRow->flag & ST_ITEM, "Constructor creates item");
  TEST_ASSERT(theDataRow->flag & ST_ACTIVE, "Constructor creates active item");
  TEST_ASSERT(theDataRow->coord_lat == expect_lat, "lat is correct");
  TEST_ASSERT(theDataRow->coord_lon == expect_lon, "lon is correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_type == '/',"Symbol table correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_symbol == '[',"Symbol correct");
  TEST_ASSERT(theDataRow->aprs_symbol.special_overlay == '\0',"overlay null");
  TEST_ASSERT_STR_EQ("",theDataRow->course,"Course correct");
  TEST_ASSERT_STR_EQ("",theDataRow->speed,"Speed correct");
  TEST_ASSERT_STR_EQ("",theDataRow->altitude,"Altitude correct in meters");
  TEST_ASSERT_STR_EQ("1",theDataRow->probability_min,"Probability min correct");
  TEST_ASSERT_STR_EQ("",theDataRow->probability_max,"Probability max correct");

  Create_object_item_tx_string(theDataRow,line,sizeof(line));

  TEST_ASSERT_STR_EQ(")TEST!3502.64N/10617.83W[Pmin1,",line,
                     "item string correctly formatted");

  if (theDataRow)
    destroy_object_item_data_row(theDataRow);


  TEST_PASS("construct_object_item_data_row");

}
int test_constructor_object_prob_circles_maxring(void)
{
  DataRow *theDataRow;
  // this is not *exactly* what convert_lon_s2l does, and so rounding can
  // make us a little off.  These coords chosen because rounding does NOT
  // bite us.
  long expect_lat = 90*60*60*100-(35*60+2.644)*60*100;
  long expect_lon = 180*60*60*100-((106*60+17.833)*60)*100;
  char line[256];

  theDataRow=construct_object_item_data_row("TEST",  // name
                                            "3502.644N",
                                            "10617.833W", // lat/lon
                                            '/','[',    // group, symbol
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
                                            1,          // prob circles
                                            "","5",      // prob min, max
                                            1,          // is_object
                                            0);         // killed
  TEST_ASSERT(theDataRow, "Constructor returns valid pointer appropriately");
  TEST_ASSERT_STR_EQ("TEST",theDataRow->call_sign,"Name populated correctly");
  TEST_ASSERT(theDataRow->flag & ST_OBJECT, "Constructor creates object");
  TEST_ASSERT(theDataRow->flag & ST_ACTIVE, "Constructor creates active object");
  TEST_ASSERT(theDataRow->coord_lat == expect_lat, "lat is correct");
  TEST_ASSERT(theDataRow->coord_lon == expect_lon, "lon is correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_type == '/',"Symbol table correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_symbol == '[',"Symbol correct");
  TEST_ASSERT(theDataRow->aprs_symbol.special_overlay == '\0',"overlay null");
  TEST_ASSERT_STR_EQ("",theDataRow->course,"Course correct");
  TEST_ASSERT_STR_EQ("",theDataRow->speed,"Speed correct");
  TEST_ASSERT_STR_EQ("",theDataRow->altitude,"Altitude correct in meters");
  TEST_ASSERT_STR_EQ("",theDataRow->probability_min,"Probability min correct");
  TEST_ASSERT_STR_EQ("5",theDataRow->probability_max,"Probability max correct");

  Create_object_item_tx_string(theDataRow,line,sizeof(line));

  // clobber the time with our standard fake time, don't worry about termination
  // coz it's in the middle of an existing string
  memcpy(&(line[11]),"111618z",7);
  TEST_ASSERT_STR_EQ(";TEST     *111618z3502.64N/10617.83W[Pmax5,",line,
                     "object string correctly formatted");

  if (theDataRow)
    destroy_object_item_data_row(theDataRow);


  TEST_PASS("construct_object_item_data_row");

}
int test_constructor_item_prob_circles_maxring(void)
{
  DataRow *theDataRow;
  // this is not *exactly* what convert_lon_s2l does, and so rounding can
  // make us a little off.  These coords chosen because rounding does NOT
  // bite us.
  long expect_lat = 90*60*60*100-(35*60+2.644)*60*100;
  long expect_lon = 180*60*60*100-((106*60+17.833)*60)*100;
  char line[256];

  theDataRow=construct_object_item_data_row("TEST",  // name
                                            "3502.644N",
                                            "10617.833W", // lat/lon
                                            '/','[',    // group, symbol
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
                                            1,          // prob circles
                                            "","5",      // prob min, max
                                            0,          // is_object
                                            0);         // killed
  TEST_ASSERT(theDataRow, "Constructor returns valid pointer appropriately");
  TEST_ASSERT_STR_EQ("TEST",theDataRow->call_sign,"Name populated correctly");
  TEST_ASSERT(theDataRow->flag & ST_ITEM, "Constructor creates item");
  TEST_ASSERT(theDataRow->flag & ST_ACTIVE, "Constructor creates active item");
  TEST_ASSERT(theDataRow->coord_lat == expect_lat, "lat is correct");
  TEST_ASSERT(theDataRow->coord_lon == expect_lon, "lon is correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_type == '/',"Symbol table correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_symbol == '[',"Symbol correct");
  TEST_ASSERT(theDataRow->aprs_symbol.special_overlay == '\0',"overlay null");
  TEST_ASSERT_STR_EQ("",theDataRow->course,"Course correct");
  TEST_ASSERT_STR_EQ("",theDataRow->speed,"Speed correct");
  TEST_ASSERT_STR_EQ("",theDataRow->altitude,"Altitude correct in meters");
  TEST_ASSERT_STR_EQ("",theDataRow->probability_min,"Probability min correct");
  TEST_ASSERT_STR_EQ("5",theDataRow->probability_max,"Probability max correct");

  Create_object_item_tx_string(theDataRow,line,sizeof(line));

  TEST_ASSERT_STR_EQ(")TEST!3502.64N/10617.83W[Pmax5,",line,
                     "item string correctly formatted");

  if (theDataRow)
    destroy_object_item_data_row(theDataRow);


  TEST_PASS("construct_object_item_data_row");

}
int test_constructor_object_prob_circles_minmax(void)
{
  DataRow *theDataRow;
  // this is not *exactly* what convert_lon_s2l does, and so rounding can
  // make us a little off.  These coords chosen because rounding does NOT
  // bite us.
  long expect_lat = 90*60*60*100-(35*60+2.644)*60*100;
  long expect_lon = 180*60*60*100-((106*60+17.833)*60)*100;
  char line[256];

  theDataRow=construct_object_item_data_row("TEST",  // name
                                            "3502.644N",
                                            "10617.833W", // lat/lon
                                            '/','[',    // group, symbol
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
                                            1,          // prob circles
                                            "1","5",      // prob min, max
                                            1,          // is_object
                                            0);         // killed
  TEST_ASSERT(theDataRow, "Constructor returns valid pointer appropriately");
  TEST_ASSERT_STR_EQ("TEST",theDataRow->call_sign,"Name populated correctly");
  TEST_ASSERT(theDataRow->flag & ST_OBJECT, "Constructor creates object");
  TEST_ASSERT(theDataRow->flag & ST_ACTIVE, "Constructor creates active object");
  TEST_ASSERT(theDataRow->coord_lat == expect_lat, "lat is correct");
  TEST_ASSERT(theDataRow->coord_lon == expect_lon, "lon is correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_type == '/',"Symbol table correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_symbol == '[',"Symbol correct");
  TEST_ASSERT(theDataRow->aprs_symbol.special_overlay == '\0',"overlay null");
  TEST_ASSERT_STR_EQ("",theDataRow->course,"Course correct");
  TEST_ASSERT_STR_EQ("",theDataRow->speed,"Speed correct");
  TEST_ASSERT_STR_EQ("",theDataRow->altitude,"Altitude correct in meters");
  TEST_ASSERT_STR_EQ("1",theDataRow->probability_min,"Probability min correct");
  TEST_ASSERT_STR_EQ("5",theDataRow->probability_max,"Probability max correct");

  Create_object_item_tx_string(theDataRow,line,sizeof(line));

  // clobber the time with our standard fake time, don't worry about termination
  // coz it's in the middle of an existing string
  memcpy(&(line[11]),"111618z",7);
  TEST_ASSERT_STR_EQ(";TEST     *111618z3502.64N/10617.83W[Pmin1,Pmax5,",line,
                     "object string correctly formatted");

  if (theDataRow)
    destroy_object_item_data_row(theDataRow);


  TEST_PASS("construct_object_item_data_row");

}
int test_constructor_item_prob_circles_minmax(void)
{
  DataRow *theDataRow;
  // this is not *exactly* what convert_lon_s2l does, and so rounding can
  // make us a little off.  These coords chosen because rounding does NOT
  // bite us.
  long expect_lat = 90*60*60*100-(35*60+2.644)*60*100;
  long expect_lon = 180*60*60*100-((106*60+17.833)*60)*100;
  char line[256];

  theDataRow=construct_object_item_data_row("TEST",  // name
                                            "3502.644N",
                                            "10617.833W", // lat/lon
                                            '/','[',    // group, symbol
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
                                            1,          // prob circles
                                            "1","5",      // prob min, max
                                            0,          // is_object
                                            0);         // killed
  TEST_ASSERT(theDataRow, "Constructor returns valid pointer appropriately");
  TEST_ASSERT_STR_EQ("TEST",theDataRow->call_sign,"Name populated correctly");
  TEST_ASSERT(theDataRow->flag & ST_ITEM, "Constructor creates item");
  TEST_ASSERT(theDataRow->flag & ST_ACTIVE, "Constructor creates active item");
  TEST_ASSERT(theDataRow->coord_lat == expect_lat, "lat is correct");
  TEST_ASSERT(theDataRow->coord_lon == expect_lon, "lon is correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_type == '/',"Symbol table correct");
  TEST_ASSERT(theDataRow->aprs_symbol.aprs_symbol == '[',"Symbol correct");
  TEST_ASSERT(theDataRow->aprs_symbol.special_overlay == '\0',"overlay null");
  TEST_ASSERT_STR_EQ("",theDataRow->course,"Course correct");
  TEST_ASSERT_STR_EQ("",theDataRow->speed,"Speed correct");
  TEST_ASSERT_STR_EQ("",theDataRow->altitude,"Altitude correct in meters");
  TEST_ASSERT_STR_EQ("1",theDataRow->probability_min,"Probability min correct");
  TEST_ASSERT_STR_EQ("5",theDataRow->probability_max,"Probability max correct");

  Create_object_item_tx_string(theDataRow,line,sizeof(line));

  TEST_ASSERT_STR_EQ(")TEST!3502.64N/10617.83W[Pmin1,Pmax5,",line,
                     "item string correctly formatted");

  if (theDataRow)
    destroy_object_item_data_row(theDataRow);


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
    {"constructor_simple_object_numeric_overlay",test_constructor_simple_object_numeric_overlay},
    {"constructor_simple_object_numeric_overlay_compressed",test_constructor_simple_object_numeric_overlay_compressed},
    {"constructor_simple_object_name_too_long",test_constructor_simple_object_name_too_long},
    {"constructor_simple_item",test_constructor_simple_item},
    {"constructor_simple_killed_object",test_constructor_simple_killed_object},
    {"constructor_simple_killed_item",test_constructor_simple_killed_item},
    {"constructor_simple_object_course_speed",test_constructor_simple_object_course_speed},
    {"constructor_simple_item_course_speed",test_constructor_simple_item_course_speed},
    {"constructor_simple_object_course_speed_alt",test_constructor_simple_object_course_speed_alt},
    {"constructor_simple_item_course_speed_alt",test_constructor_simple_item_course_speed_alt},
    {"constructor_simple_object_course_speed_alt_comment",test_constructor_simple_object_course_speed_alt_comment},
    {"constructor_simple_item_course_speed_alt_comment",test_constructor_simple_item_course_speed_alt_comment},
    {"constructor_area_object_basic",test_constructor_area_object_basic},
    {"constructor_area_item_basic",test_constructor_area_item_basic},
    {"constructor_area_object_speed",test_constructor_area_object_speed},
    {"constructor_area_item_speed",test_constructor_area_item_speed},
    {"constructor_area_object_offsets",test_constructor_area_object_offsets},
    {"constructor_area_item_offsets",test_constructor_area_item_offsets},
    {"constructor_area_object_line_corridor",test_constructor_area_object_line_corridor},
    {"constructor_area_item_line_corridor",test_constructor_area_item_line_corridor},
    {"constructor_area_object_offsets_filled",test_constructor_area_object_offsets_filled},
    {"constructor_area_item_offsets_filled",test_constructor_area_item_offsets_filled},
    {"constructor_area_object_offsets_alt",test_constructor_area_object_offsets_alt},
    {"constructor_area_item_offsets_alt",test_constructor_area_item_offsets_alt},
    {"constructor_area_object_signpost_basic",test_constructor_area_object_signpost_basic},
    {"constructor_area_item_signpost_basic",test_constructor_area_item_signpost_basic},
    {"constructor_area_object_signpost_speed",test_constructor_area_object_signpost_speed},
    {"constructor_area_item_signpost_speed",test_constructor_area_item_signpost_speed},
    {"constructor_object_df_omni",test_constructor_object_df_omni},
    {"constructor_item_df_omni",test_constructor_item_df_omni},
    {"constructor_object_df_beam",test_constructor_object_df_beam},
    {"constructor_item_df_beam",test_constructor_item_df_beam},
    {"constructor_object_prob_circles_noring",test_constructor_object_prob_circles_noring},
    {"constructor_item_prob_circles_noring",test_constructor_item_prob_circles_noring},
    {"constructor_object_prob_circles_minring",test_constructor_object_prob_circles_minring},
    {"constructor_item_prob_circles_minring",test_constructor_item_prob_circles_minring},
    {"constructor_object_prob_circles_maxring",test_constructor_object_prob_circles_maxring},
    {"constructor_item_prob_circles_maxring",test_constructor_item_prob_circles_maxring},
    {"constructor_object_prob_circles_minmax",test_constructor_object_prob_circles_minmax},
    {"constructor_item_prob_circles_minmax",test_constructor_item_prob_circles_minmax},
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
