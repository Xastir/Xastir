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


#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif  // HAVE_CONFIG_H

#include <stdlib.h>
#include <math.h>

#include "util.h"
#include "main.h"
#include "database.h"

// Calculate new position based on distance and angle.
//
// Input:   lat/long in Xastir coordinate system (100ths of seconds)
//          distance in nautical miles
//          angle in ° true
//
// Outputs: *x_long, *y_lat in Xastir coordinate system (100ths of
//           seconds)
//
//
// From http://home.t-online.de/home/h.umland/Chapter12.pdf
//
// Dead-reckoning using distance in km, course C:
// Lat_B° = Lat_A° + ( (360/40031.6) * distance * cos C )
//
// Dead-reckoning using distance in nm, course C:
// Lat_B° = Lat_A° + ( (distance/60) * cos C )
//
// Average of two latitudes (required for next two equations)
// Lat_M° = (Lat_A° + Lat_B°) / 2
//
// Dead-reckoning using distance in km, course C:
// Lon_B° = Lon_A° + ( (360/40031.6) * distance * (sin C / cos
// Lat_M) )
//
// Dead-reckoning using distance in nm, course C:
// Lon_B° = Lon_A° + ( (distance/60) * (sin C / cos Lat_M) )
//
// If resulting longitude exceeds +/- 180°, subtract/add 360°.
//
void compute_DR_position(long x_long,   // input
                         long y_lat,     // input
                         double range,   // input in nautical miles
                         double course,  // input in ° true
                         long *x_long2,  // output
                         long *y_lat2)   // output
{
  double bearing_radians, lat_M_radians;
  float lat_A, lat_B, lon_A, lon_B, lat_M;
  int ret;
  unsigned long x_u_long, y_u_lat;


//fprintf(stderr,"Distance:%fnm, Course:%f,  Time:%d\n",
//    range,
//    course,
//    (int)(sec_now() - p_station->sec_heard));

  // Bearing in radians
  bearing_radians = (double)((course/360.0) * 2.0 * M_PI);

  // Convert lat/long to floats
  ret = convert_from_xastir_coordinates( &lon_A,
                                         &lat_A,
                                         x_long,
                                         y_lat);

  // Check if conversion ok
  if (!ret)
  {
    // Problem during conversion.  Exit without changes.
    *x_long2 = x_long;
    *y_lat2 = y_lat;
    return;
  }

  // Compute new latitude
  lat_B = (float)((double)(lat_A) + (range/60.0) * cos(bearing_radians));

  // Compute mid-range latitude
  lat_M = (lat_A + lat_B) / 2.0;

  // Convert lat_M to radians
  lat_M_radians = (double)((lat_M/360.0) * 2.0 * M_PI);

  // Compute new longitude
  lon_B = (float)((double)(lon_A)
                  + (range/60.0) * ( sin(bearing_radians) / cos(lat_M_radians)) );

  // Test for out-of-bounds longitude, correct if so.
  if (lon_B < -360.0)
  {
    lon_B = lon_B + 360.0;
  }
  if (lon_B >  360.0)
  {
    lon_B = lon_B - 360.0;
  }

//fprintf(stderr,"Lat:%f,  Lon:%f\n", lat_B, lon_B);

  ret = convert_to_xastir_coordinates(&x_u_long,
                                      &y_u_lat,
                                      lon_B,
                                      lat_B);

  // Check if conversion ok
  if (!ret)
  {
    // Problem during conversion.  Exit without changes.
    *x_long2 = x_long;
    *y_lat2 = y_lat;
    return;
  }

  // Convert from unsigned long to long
  *x_long2 = (long)x_u_long;
  *y_lat2  = (long)y_u_lat;
}





// Calculate new position based on speed/course/modified-time.
// We'll call it from Create_object_item_tx_string() and from the
// modify object/item routines to calculate a new position and stuff
// it into the record along with the modification time before we
// start off in a new direction.
//
// Input:   *p_station
//
// Outputs: *x_long, *y_lat in Xastir coordinate system (100ths of
//           seconds)
//
//
// From http://home.t-online.de/home/h.umland/Chapter12.pdf
//
// Dead-reckoning using distance in km, course C:
// Lat_B° = Lat_A° + ( (360/40031.6) * distance * cos C )
//
// Dead-reckoning using distance in nm, course C:
// Lat_B° = Lat_A° + ( (distance/60) * cos C )
//
// Average of two latitudes (required for next two equations)
// Lat_M° = (Lat_A° + Lat_B°) / 2
//
// Dead-reckoning using distance in km, course C:
// Lon_B° = Lon_A° + ( (360/40031.6) * distance * (sin C / cos
// Lat_M) )
//
// Dead-reckoning using distance in nm, course C:
// Lon_B° = Lon_A° + ( (distance/60) * (sin C / cos Lat_M) )
//
// If resulting longitude exceeds +/- 180°, subtract/add 360°.
//
//
// Possible Problems/Changes:
// --------------------------
// *) Change to using last_modified_time for DR.  Also tweak the
//    code so that we don't do incremental DR and use our own
//    decoded objects to update everything.  If we keep the
//    last_modified_time and the last_modified_position separate
//    DR'ed objects/items, we can always use those instead of the
//    other variables if we have a non-zero speed.
//
// *) Make sure not to corrupt our position of the object when we
//    receive the packet back via loopback/RF/internet.  In
//    particular the position and the last_modified_time should stay
//    constant in this case so that dead-reckoning can continue to
//    move the object consistently, plus we won't compound errors as
//    we go.
//
// *) A server Xastir sees empty strings on it's server port when
//    these objects are transmitted to it.  Investigate.  It
//    sometimes does it when speed is 0, but it's not consistent.
//
// *) Get the last_modified_time embedded into the logfile so that
//    we don't "lose time" if we shut down for a bit.  DR'ed objects
//    will be at the proper positions when we start back up.
//
void compute_current_DR_position(DataRow *p_station, long *x_long, long *y_lat)
{
  int my_course = 0;  // In ° true
  double range = 0.0;
  double bearing_radians, lat_M_radians;
  float lat_A, lat_B, lon_A, lon_B, lat_M;
  int ret;
  unsigned long x_u_long, y_u_lat;
  time_t secs_now;


  secs_now=sec_now();


  // Check whether we have course in the current data
  //
  if ( (strlen(p_station->course)>0) && (atof(p_station->course) > 0) )
  {

    my_course = atoi(p_station->course);    // In ° true
  }
  //
  // Else check whether the previous position had a course.  Note
  // that newest_trackpoint if it exists should be the same as the
  // current data, so we have to go back one further trackpoint.
  // Make sure in this case that this trackpoint has occurred
  // within the dead-reckoning timeout period though, else ignore
  // it.
  //
  else if ( (p_station->newest_trackpoint != NULL)
            && (p_station->newest_trackpoint->prev != NULL)
            && (p_station->newest_trackpoint->prev->course != -1)   // Undefined
            && ( (secs_now-p_station->newest_trackpoint->prev->sec) < dead_reckoning_timeout) )
  {

    // In ° true
    my_course = p_station->newest_trackpoint->prev->course;
  }


  // Get distance in nautical miles from the current data
  //
  if ( (strlen(p_station->speed)>0) && (atof(p_station->speed) >= 0) )
  {

    // Speed is in knots (same as nautical miles/hour)
    range = (double)( (sec_now() - p_station->sec_heard)
                      * ( atof(p_station->speed) / 3600.0 ) );
  }
  //
  // Else check whether the previous position had speed.  Note
  // that newest_trackpoint if it exists should be the same as the
  // current data, so we have to go back one further trackpoint.
  //
  else if ( (p_station->newest_trackpoint != NULL)
            && (p_station->newest_trackpoint->prev != NULL)
            && (p_station->newest_trackpoint->prev->speed != -1) // Undefined
            && ( (secs_now-p_station->newest_trackpoint->prev->sec) < dead_reckoning_timeout) )
  {

    // Speed is in units of 0.1km/hour.  Different than above!
    range = (double)( (sec_now() - p_station->sec_heard)
                      * ( p_station->newest_trackpoint->prev->speed / 10 * 0.5399568 / 3600.0 ) );
  }


//fprintf(stderr,"Distance:%fnm, Course:%d,  Time:%d\n",
//    range,
//    my_course,
//    (int)(sec_now() - p_station->sec_heard));

  // Bearing in radians
  bearing_radians = (double)((my_course/360.0) * 2.0 * M_PI);

  // Convert lat/long to floats
  ret = convert_from_xastir_coordinates( &lon_A,
                                         &lat_A,
                                         p_station->coord_lon,
                                         p_station->coord_lat);

  // Check if conversion ok
  if (!ret)
  {
    // Problem during conversion.  Exit without changes.
    *x_long = p_station->coord_lon;
    *y_lat = p_station->coord_lat;
    return;
  }

  // Compute new latitude
  lat_B = (float)((double)(lat_A) + (range/60.0) * cos(bearing_radians));

  // Compute mid-range latitude
  lat_M = (lat_A + lat_B) / 2.0;

  // Convert lat_M to radians
  lat_M_radians = (double)((lat_M/360.0) * 2.0 * M_PI);

  // Compute new longitude
  lon_B = (float)((double)(lon_A)
                  + (range/60.0) * ( sin(bearing_radians) / cos(lat_M_radians)) );

  // Test for out-of-bounds longitude, correct if so.
  if (lon_B < -360.0)
  {
    lon_B = lon_B + 360.0;
  }
  if (lon_B >  360.0)
  {
    lon_B = lon_B - 360.0;
  }

//fprintf(stderr,"Lat:%f,  Lon:%f\n", lat_B, lon_B);

  ret = convert_to_xastir_coordinates(&x_u_long,
                                      &y_u_lat,
                                      lon_B,
                                      lat_B);

  // Check if conversion ok
  if (!ret)
  {
    // Problem during conversion.  Exit without changes.
    *x_long = p_station->coord_lon;
    *y_lat = p_station->coord_lat;
    return;
  }

  // Convert from unsigned long to long
  *x_long = (long)x_u_long;
  *y_lat  = (long)y_u_lat;
}





