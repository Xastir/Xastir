/*
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2019 The Xastir Group
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

#include <stdio.h>
#include <ctype.h>
#include <Xm/XmAll.h>

// The following files support setting the system time from the GPS
#if TIME_WITH_SYS_TIME
  // Define needed by some versions of Linux in order to define
  // strptime()
  #ifndef __USE_XOPEN
    #define __USE_XOPEN
  #endif
  #include <sys/time.h>
  #include <time.h>
#else   // TIME_WITH_SYS_TIME
  #if HAVE_SYS_TIME_H
    #include <sys/time.h>
  #else  // HAVE_SYS_TIME_H
    #include <time.h>
  #endif // HAVE_SYS_TIME_H
#endif  // TIME_WITH_SYS_TIME

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

#include "gps.h"
#include "main.h"
#include "interface.h"
#include "lang.h"
#include "util.h"

// Must be last include file
#include "leak_detection.h"



char gps_gprmc[MAX_GPS_STRING+1];
char gps_gpgga[MAX_GPS_STRING+1];
char gps_sats[4] = "";
char gps_alt[8] = "";
char gps_spd[10] = "";
char gps_sunit[2] = "";
char gps_cse[10] = "";
int  gps_valid = 0; // 0=invalid, 1=valid, 2=2D Fix, 3=3D Fix

int gps_stop_now;





// This function is destructive to its first parameter
//
// GPRMC,UTC-Time,status(A/V),lat,N/S,lon,E/W,SOG,COG,UTC-Date,Mag-Var,E/W[*CHK]
// GPRMC,hhmmss[.sss],{A|V},ddmm.mm[mm],{N|S},dddmm.mm[mm],{E|W},[dd]d.d[ddddd],[dd]d.d[d],ddmmyy,[ddd.d],[{E|W}][,A|D|E|N|S][*CHK]
//
// The last field before the checksum is entirely optional, and in
// fact first appeared in NMEA 2.3 (fairly recently).  Most GPS's do
// not currently put out that field.  The field may be null or
// nonexistent including the comma.  Only "A" or "D" are considered
// to be active and reliable fixes if this field is present.
// Fix-Quality:
//  A: Autonomous
//  D: Differential
//  E: Estimated
//  N: Not Valid
//  S: Simulator
//
// $GPRMC,081836,A,3751.65,S,14507.36,E,000.0,360.0,130998,011.3,E*62
// $GPRMC,104748.821,A,4301.1492,N,08803.0374,W,0.085048,102.36,010605,,*1A
// $GPRMC,104749.821,A,4301.1492,N,08803.0377,W,0.054215,74.60,010605,,*2D
//
int decode_gps_rmc( char *data,
                    char *long_pos,
                    int long_pos_length,
                    char *lat_pos,
                    int lat_pos_length,
                    char *spd,
                    char *unit,
                    int unit_length,
                    char *cse,
                    time_t *stim,
                    int *status)
{

  char *temp_ptr;
  char *temp_ptr2;
  char temp_data[MAX_LINE_SIZE+1];    // Big in case we get concatenated packets (it happens!)
  char sampletime[7]; // We ignore fractional seconds
  char long_pos_x[11];
  char long_ew;
  char lat_pos_y[10];
  char lat_ns;
  char speed[10];
  char speed_unit;
  char course[8];
  char sampledate[7];

#ifdef HAVE_STRPTIME
  char sampledatime[15];
  char *tzp;
  char tzn[512];
  struct tm stm;
#endif // HAVE_STRPTIME


// We should check for a minimum line length before parsing,
// and check for end of input while tokenizing.

  if ( (data == NULL) || (strlen(data) < 37) )
  {
    return(0);  // Not enough data to parse position from.
  }

  if (!(isRMC(data)))   // No GxRMC found
  {
    return(0);
  }

  if(strchr(data,',') == NULL)  // No comma found
  {
    return(0);
  }

  (void)strtok(data,",");   // get GxRMC and skip it
  temp_ptr=strtok(NULL,",");   // get time

  if (temp_ptr == NULL)   // No comma found
  {
    return(0);
  }

  xastir_snprintf(sampletime,
                  sizeof(sampletime),
                  "%s",
                  temp_ptr);

  temp_ptr=strtok(NULL,",");  // get fix status

  if (temp_ptr == NULL) // No comma found
  {
    return(0);
  }

  if (temp_ptr[0] == 'A')
  {
    *status = 1;
  }
  else
  {
    *status = 0;
  }

  xastir_snprintf(temp_data,
                  sizeof(temp_data),
                  "%s",
                  temp_ptr);
  temp_data[2] = '\0';

  if (temp_data[0] != 'A')  // V is a warning but we can get good data still ?
  {
    return(0);  // Didn't find 'A' in the proper spot
  }

  temp_ptr=strtok(NULL,",");  // get latitude

  if (temp_ptr == NULL)
  {
    return(0);  // Doesn't look like latitude
  }

  // Newer GPS'es appear not to zero-fill on the left.  Check for
  // the decimal point in all the possible places.
  if (temp_ptr[1] != '.'
      && temp_ptr[2] != '.'
      && temp_ptr[3] != '.'
      && temp_ptr[4] != '.')
  {
    return(0);  // Doesn't look like latitude
  }

// Note:  Starlink Invicta shows "lllll.ll" format for latitude in
// the GPRMC sentence, which would mean we'd need another term in
// the above, and would need to terminate at [10] below (making sure
// we extended the field another char as well to handle it).  I'm
// hoping it was a typo in the Starlink Invicta spec, as latitude
// never requires three digits for degrees.

  xastir_snprintf(lat_pos_y,
                  sizeof(lat_pos_y),
                  "%s",
                  temp_ptr);
  lat_pos_y[9] = '\0';

// Note that some GPS's put out latitude with extra precision, such as 4801.1234

  // Check for comma char, replace with '\0'
  temp_ptr2 = strstr(lat_pos_y, ",");
  if (temp_ptr2)
  {
    temp_ptr2[0] = '\0';
  }

  temp_ptr=strtok(NULL,",");  // get N-S

  if (temp_ptr == NULL)   // No comma found
  {
    return(0);
  }

  xastir_snprintf(temp_data,
                  sizeof(temp_data),
                  "%s",
                  temp_ptr);
  temp_data[1] = '\0';

  lat_ns=toupper((int)temp_data[0]);

  if(lat_ns != 'N' && lat_ns != 'S')
  {
    return(0);  // Doesn't look like latitude
  }

  temp_ptr=strtok(NULL,",");  // get long

  if (temp_ptr == NULL)
  {
    return(0);  // Doesn't look like longitude
  }

  // Newer GPS'es appear not to zero-fill on the left.  Check for
  // the decimal point in all the possible places.
  if (temp_ptr[1] != '.'
      && temp_ptr[2] != '.'
      && temp_ptr[3] != '.'
      && temp_ptr[4] != '.'
      && temp_ptr[5] != '.')
  {
    return(0);  // Doesn't look like longitude
  }


  xastir_snprintf(long_pos_x,
                  sizeof(long_pos_x),
                  "%s",
                  temp_ptr);
  long_pos_x[10] = '\0';

// Note that some GPS's put out longitude with extra precision, such as 12201.1234

  // Check for comma char, replace with '\0'
  temp_ptr2 = strstr(long_pos_x, ",");
  if (temp_ptr2)
  {
    temp_ptr2[0] = '\0';
  }

  temp_ptr=strtok(NULL,",");  // get E-W

  if (temp_ptr == NULL)   // No comma found
  {
    return(0);
  }

  xastir_snprintf(temp_data,
                  sizeof(temp_data),
                  "%s",
                  temp_ptr);
  temp_data[1] = '\0';

  long_ew=toupper((int)temp_data[0]);

  if (long_ew != 'E' && long_ew != 'W')
  {
    return(0);  // Doesn't look like longitude
  }

  temp_ptr=strtok(NULL,",");  // Get speed

  if (temp_ptr == 0)  // No comma found
  {
    return(0);
  }

  xastir_snprintf(speed,
                  sizeof(speed),
                  "%s",
                  temp_ptr);
  speed[9] = '\0';

  speed_unit='K';
  temp_ptr=strtok(NULL,",");  // Get course

  if (temp_ptr == NULL)   // No comma found
  {
    return(0);
  }

  xastir_snprintf(course,
                  sizeof(course),
                  "%s",
                  temp_ptr);
  course[7] = '\0';

  temp_ptr=strtok(NULL,",");   // get date of fix

  if (temp_ptr == NULL)   // No comma found
  {
    return(0);
  }

  xastir_snprintf(sampledate,
                  sizeof(sampledate),
                  "%s",
                  temp_ptr);
  sampledate[6] = '\0';


  // Data is good
  xastir_snprintf(long_pos, long_pos_length, "%s%c", long_pos_x,long_ew);
  xastir_snprintf(lat_pos, lat_pos_length, "%s%c", lat_pos_y, lat_ns);
  xastir_snprintf(spd, 10, "%s", speed);
  xastir_snprintf(unit, unit_length, "%c", speed_unit);
  xastir_snprintf(cse, 10, "%s", course);

#ifdef HAVE_STRPTIME
  // Translate date/time into time_t GPS time is in UTC.  First,
  // save existing TZ Then set conversion to UTC, then set back to
  // existing TZ.
  tzp=getenv("TZ");
  if ( tzp == NULL )
  {
    tzp = "";
  }
  xastir_snprintf(tzn,
                  sizeof(tzn),
                  "TZ=%s",
                  tzp);
  putenv("TZ=UTC");
  tzset();
  xastir_snprintf(sampledatime,
                  sizeof(sampledatime),
                  "%s%s",
                  sampledate,
                  sampletime);
  (void)strptime(sampledatime, "%d%m%y%H%M%S", &stm);
  *stim=mktime(&stm);
  putenv(tzn);
  tzset();
#endif  // HAVE_STRPTIME

  //fprintf(stderr,"Speed %s\n",spd);
  return(1);
}





// This function is destructive to its first parameter
//
// GPGGA,UTC-Time,lat,N/S,long,E/W,GPS-Quality,nsat,HDOP,MSL-Meters,M,Geoidal-Meters,M,DGPS-Data-Age(seconds),DGPS-Ref-Station-ID[*CHK]
// GPGGA,hhmmss[.sss],ddmm.mm[mm],{N|S},dddmm.mm[mm],{E|W},{0-8},dd,[d]d.d,[-dddd]d.d,M,[-ddd]d.d,M,[dddd.d],[dddd][*CHK]
//
// GPS-Quality:
//  0: Invalid Fix
//  1: GPS Fix
//  2: DGPS Fix
//  3: PPS Fix
//  4: RTK Fix
//  5: Float RTK Fix
//  6: Estimated (dead-reckoning) Fix
//  7: Manual Input Mode
//  8: Simulation Mode
//
// $GPGGA,170834,4124.8963,N,08151.6838,W,1,05,1.5,280.2,M,-34.0,M,,,*75
// $GPGGA,104438.833,4301.1439,N,08803.0338,W,1,05,1.8,185.8,M,-34.2,M,0.0,0000*40

// NOTE:  while the above specifically refers to $GP strings for the GPS
// receivers, there are others such as GNSS, GLONASS, Beidou, and Gallileo
// that differ only in the third character.  We support those, too.
//
int decode_gps_gga( char *data,
                    char *long_pos,
                    int long_pos_length,
                    char *lat_pos,
                    int lat_pos_length,
                    char *sats,
                    char *alt,
                    char *aunit,
                    int *status )
{

  char *temp_ptr;
  char *temp_ptr2;
  char temp_data[MAX_LINE_SIZE+1];    // Big in case we get concatenated packets (it happens!)
  char long_pos_x[11];
  char long_ew;
  char lat_pos_y[10];
  char lat_ns;
  char sats_visible[4];
  char altitude[8];
  char alt_unit;


// We should check for a minimum line length before parsing,
// and check for end of input while tokenizing.


  if ( (data == NULL) || (strlen(data) < 35) )  // Not enough data to parse position from.
  {
    return(0);
  }

  if (!(isGGA(data)))
  {
    return(0);
  }

  if (strchr(data,',') == NULL)
  {
    return(0);
  }

  if (strtok(data,",") == NULL) // get GPGGA and skip it
  {
    return(0);
  }

  if(strtok(NULL,",") == NULL)    // get time and skip it
  {
    return(0);
  }

  temp_ptr = strtok(NULL,",");    // get latitude

  if (temp_ptr == NULL)
  {
    return(0);
  }

  xastir_snprintf(lat_pos_y,
                  sizeof(lat_pos_y),
                  "%s",
                  temp_ptr);
  lat_pos_y[9] = '\0';

// Note that some GPS's put out latitude with extra precision, such as 4801.1234

  // Check for comma char, replace with '\0'
  temp_ptr2 = strstr(lat_pos_y, ",");
  if (temp_ptr2)
  {
    temp_ptr2[0] = '\0';
  }

  temp_ptr = strtok(NULL,",");    // get N-S

  if (temp_ptr == NULL)
  {
    return(0);
  }

  xastir_snprintf(temp_data,
                  sizeof(temp_data),
                  "%s",
                  temp_ptr);
  temp_data[1] = '\0';

  lat_ns=toupper((int)temp_data[0]);

  if(lat_ns != 'N' && lat_ns != 'S')
  {
    return(0);
  }

  temp_ptr = strtok(NULL,",");    // get long

  if(temp_ptr == NULL)
  {
    return(0);
  }

  xastir_snprintf(long_pos_x,
                  sizeof(long_pos_x),
                  "%s",
                  temp_ptr);
  long_pos_x[10] = '\0';

// Note that some GPS's put out longitude with extra precision, such as 12201.1234

  // Check for comma char, replace with '\0'
  temp_ptr2 = strstr(long_pos_x, ",");
  if (temp_ptr2)
  {
    temp_ptr2[0] = '\0';
  }

  temp_ptr = strtok(NULL,",");    // get E-W

  if (temp_ptr == NULL)
  {
    return(0);
  }

  xastir_snprintf(temp_data,
                  sizeof(temp_data),
                  "%s",
                  temp_ptr);
  temp_data[1] = '\0';

  long_ew=toupper((int)temp_data[0]);

  if (long_ew != 'E' && long_ew != 'W')
  {
    return(0);
  }

  temp_ptr = strtok(NULL,",");    // get FIX Quality

  if (temp_ptr == NULL)
  {
    return(0);
  }

  xastir_snprintf(temp_data,
                  sizeof(temp_data),
                  "%s",
                  temp_ptr);
  temp_data[1] = '\0';

  // '0' = bad fix, positive numbers = ok
  if(temp_data[0] == '0')
  {
    return(0);
  }

  // Save the fix quality in "status"
  *status = atoi(temp_data);

  temp_ptr=strtok(NULL,",");      // Get sats vis

  if (temp_ptr == NULL)
  {
    return(0);
  }

  xastir_snprintf(sats_visible,
                  sizeof(sats_visible),
                  "%s",
                  temp_ptr);
  sats_visible[2] = '\0';

  temp_ptr=strtok(NULL,",");      // get hoz dil

  if (temp_ptr == NULL)
  {
    return(0);
  }

  temp_ptr=strtok(NULL,",");      // Get altitude

  if (temp_ptr == NULL)
  {
    return(0);
  }

  // Get altitude
  xastir_snprintf(altitude,
                  sizeof(altitude),
                  "%s",
                  temp_ptr);
  altitude[7] = '\0';

  temp_ptr=strtok(NULL,",");      // get UNIT

  if (temp_ptr == NULL)
  {
    return(0);
  }

  // get UNIT
  xastir_snprintf(temp_data,
                  sizeof(temp_data),
                  "%s",
                  temp_ptr);
  temp_data[1] = '\0';

  alt_unit=temp_data[0];

  // Data is good
  xastir_snprintf(long_pos, long_pos_length, "%s%c", long_pos_x, long_ew);
  xastir_snprintf(lat_pos, lat_pos_length, "%s%c", lat_pos_y, lat_ns);
  xastir_snprintf(sats, 4, "%s", sats_visible);
  xastir_snprintf(alt, 8, "%s", altitude);
  xastir_snprintf(aunit, 2, "%c", alt_unit);

  return(1);
}





//
// Note that the length of "gps_line_data" can be up to
// MAX_DEVICE_BUFFER, which is currently set to 4096.
//
int gps_data_find(char *gps_line_data, int port)
{

  char long_pos[20],lat_pos[20],aunit[2];
  time_t t;
  char temp_str[MAX_GPS_STRING+1];
  int have_valid_string = 0;
#ifndef __CYGWIN__
  struct timeval tv;
  struct timezone tz;
#endif  // __CYGWIN__



  if (isRMC(gps_line_data))
  {

    if (debug_level & 128)
    {
      char filtered_data[MAX_LINE_SIZE+1];

      xastir_snprintf(filtered_data,
                      sizeof(filtered_data),
                      "%s",
                      gps_line_data);

      makePrintable(filtered_data);
      fprintf(stderr,"Got RMC %s\n", filtered_data);
    }

    if (debug_level & 128)
    {
      // Got GPS RMC String
      statusline(langcode("BBARSTA015"),0);
    }

    xastir_snprintf(gps_gprmc,
                    sizeof(gps_gprmc),
                    "%s",
                    gps_line_data);

    xastir_snprintf(temp_str, sizeof(temp_str), "%s", gps_gprmc);
    // decode_gps_rmc is destructive to its first parameter
    if (decode_gps_rmc( temp_str,
                        long_pos,
                        sizeof(long_pos),
                        lat_pos,
                        sizeof(lat_pos),
                        gps_spd,
                        gps_sunit,
                        sizeof(gps_sunit),
                        gps_cse,
                        &t,
                        &gps_valid ) == 1)      // mod station data
    {
      // got GPS data
      have_valid_string++;
      if (debug_level & 128)
        fprintf(stderr,"RMC <%s> <%s><%s> %c <%s>\n",
                long_pos,lat_pos,gps_spd,gps_sunit[0],gps_cse);

      if (debug_level & 128)
      {
        fprintf(stderr,"Checking for Time Set on %d (%d)\n",
                port, devices[port].set_time);
      }

// Don't set the time if it's a Cygwin system.  Causes problems with
// date, plus time can be an hour off if daylight savings time is
// enabled on Windows.
//
#ifndef __CYGWIN__
      if (devices[port].set_time)
      {
        tv.tv_sec=t;
        tv.tv_usec=0;
        tz.tz_minuteswest=0;
        tz.tz_dsttime=0;

        if (debug_level & 128)
        {
          fprintf(stderr,"Setting Time %ld EUID: %d, RUID: %d\n",
                  (long)t, (int)getuid(), (int)getuid());
        }

#ifdef HAVE_SETTIMEOFDAY

        ENABLE_SETUID_PRIVILEGE;
        settimeofday(&tv, &tz);
        DISABLE_SETUID_PRIVILEGE;

#endif  // HAVE_SETTIMEOFDAY

      }

#endif  // __CYGWIN__

    }
  }
  else
  {
    if (debug_level & 128)
    {
      int i;
      fprintf(stderr,"Not $GxRMC: ");
      for (i = 0; i<7; i++)
      {
        fprintf(stderr,"%c", gps_line_data[i]);
      }
      fprintf(stderr,"\n");
    }
  }

  if (isGGA(gps_line_data))
  {

    if (debug_level & 128)
    {
      char filtered_data[MAX_LINE_SIZE+1];

      xastir_snprintf(filtered_data,
                      sizeof(filtered_data),
                      "%s",
                      gps_line_data);

      makePrintable(filtered_data);
      fprintf(stderr,"Got GGA %s\n", filtered_data);
    }

    if (debug_level & 128)
    {
      // Got GPS GGA String
      statusline(langcode("BBARSTA016"),0);
    }

    xastir_snprintf(gps_gpgga,
                    sizeof(gps_gpgga),
                    "%s",
                    gps_line_data);

    xastir_snprintf(temp_str, sizeof(temp_str), "%s", gps_gpgga);

    // decode_gps_gga is destructive to its first parameter
    if ( decode_gps_gga( temp_str,
                         long_pos,
                         sizeof(long_pos),
                         lat_pos,
                         sizeof(lat_pos),
                         gps_sats,
                         gps_alt,
                         aunit,
                         &gps_valid ) == 1)   // mod station data
    {
      // got GPS data
      have_valid_string++;
      if (debug_level & 128)
        fprintf(stderr,"GGA <%s> <%s> <%s> <%s> %c\n",
                long_pos,lat_pos,gps_sats,gps_alt,aunit[0]);
    }
  }
  else
  {
    if (debug_level & 128)
    {
      int i;
      fprintf(stderr,"Not $GPGGA: ");
      for (i = 0; i<7; i++)
      {
        fprintf(stderr,"%c",gps_line_data[i]);
      }
      fprintf(stderr,"\n");
    }
  }


  if (have_valid_string)
  {

    if (debug_level & 128)
    {
      statusline(langcode("BBARSTA037"),0);
    }

    // Go update my screen position
    my_station_gps_change(long_pos,lat_pos,gps_cse,gps_spd,
                          gps_sunit[0],gps_alt,gps_sats);

    // gps_stop_now is how we tell main.c that we've got a valid GPS string.
    // Only useful for HSP mode?
    if (!gps_stop_now)
    {
      gps_stop_now=1;
    }

    // If HSP port, shutdown gps for timed interval
    if (port_data[port].device_type == DEVICE_SERIAL_TNC_HSP_GPS)
    {
      // return dtr to normal
      port_dtr(port,0);
    }
  }
  return(have_valid_string);
}





static char checksum[3];





// Function to compute checksums for NMEA sentences
//
// Input: "$............*"
// Output: Two character string containing the checksum
//
// Checksum is computed from the '$' to the '*', but not including
// these two characters.  It is an 8-bit Xor of the characters
// specified, encoded in hexadecimal format.
//
char *nmea_checksum(char *nmea_sentence)
{
  int i;
  int sum = 0;
  int right;
  int left;
  char convert[17] = "0123456789ABCDEF";


  for (i = 1; i < ((int)strlen(nmea_sentence) - 1); i++)
  {
    sum = sum ^ nmea_sentence[i];
  }

  right = sum % 16;
  left = (sum / 16) % 16;

  xastir_snprintf(checksum, sizeof(checksum), "%c%c",
                  convert[left],
                  convert[right]);

  return(checksum);
}






// Function which will send an NMEA sentence to a Garmin GPS which
// will create a waypoint if the Garmin is set to NMEA-in/NMEA-out
// mode.  The sentence looks like this:
//
// $GPWPL,4807.038,N,01131.000,E,WPTNME*31
//
// $GPWPL,4849.65,N,06428.53,W,0001*54
// $GPWPL,4849.70,N,06428.50,W,0002*50
//
// 4807.038,N   Latitude
// 01131.000,E  Longitude
// WPTNME       Waypoint Name (stick to 6 chars for compatibility?)
// *31          Checksum, always begins with '*'
//
//
// Future implementation ideas:
//
// Create linked list of waypoints/location.
// Use the list to prevent multiple writes of the same waypoint if
// nothing has changed.
//
// Use the list to check distance of previously-written waypoints.
// If we're out of range, delete the waypoint and remove it from the
// list.
//
// Perhaps write the list to disk also.  As we shut down, delete the
// waypoints (self-cleaning).  As we come up, load them in again?
// We could also just continue cleaning out waypoints that are
// out-of-range since the last time we ran the program.  That's
// probably a better solution.
//
void create_garmin_waypoint(long latitude,long longitude,char *call_sign)
{
  char short_callsign[10];
  char lat_string[15];
  char long_string[15];
  char lat_char;
  char long_char;
  int i,j,len;
  char out_string[80];
  char out_string2[80];


  convert_lat_l2s(latitude,
                  lat_string,
                  sizeof(lat_string),
                  CONVERT_HP_NOSP);
  lat_char = lat_string[strlen(lat_string) - 1];
  lat_string[strlen(lat_string) - 1] = '\0';

  convert_lon_l2s(longitude,
                  long_string,
                  sizeof(long_string),
                  CONVERT_HP_NOSP);
  long_char = long_string[strlen(long_string) - 1];
  long_string[strlen(long_string) - 1] = '\0';

  len = strlen(call_sign);
  if (len > 9)
  {
    len = 9;
  }

  j = 0;
  for (i = 0; i <= len; i++)      // Copy the '\0' as well
  {
    if (call_sign[i] != '-')    // We don't want the dash
    {
      short_callsign[j++] = call_sign[i];
    }
  }
  short_callsign[6] = '\0';   // Truncate at 6 chars

  // Convert to upper case.  Garmin's don't seem to like lower
  // case waypoint names
  to_upper(short_callsign);

  //fprintf(stderr,"Creating waypoint for %s:%s\n",call_sign,short_callsign);

  xastir_snprintf(out_string, sizeof(out_string),
                  "$GPWPL,%s,%c,%s,%c,%s*",
                  lat_string,
                  lat_char,
                  long_string,
                  long_char,
                  short_callsign);

  nmea_checksum(out_string);

  strncpy(out_string2, out_string, sizeof(out_string2));
  out_string2[sizeof(out_string2)-1] = '\0';  // Terminate string

  strcat(out_string2, checksum);

  output_waypoint_data(out_string2);

  //fprintf(stderr,"%s\n",out_string2);
}

// Test if this is a GGA string, irrespective of what constellation it
// might be for.  This should allow us to support GPS, GLONASS, Gallileo,
// Beidou, or GNSS receivers, and multi-constellation receivers.
int isGGA(char *gps_line_data)
{
  int retval;
  retval = (strncmp(gps_line_data,"$",1)==0);
  retval = retval && ((strlen(gps_line_data)>7)
                      && strncmp(&(gps_line_data[3]),"GGA,",4)==0);
  retval = retval && (strchr("PNALB",gps_line_data[2])!=0);
  return (retval);
}

// Test if this is an RMC string.   See comments for isGGA.
int isRMC(char *gps_line_data)
{
  int retval;
  retval = (strncmp(gps_line_data,"$",1)==0);
  retval = retval && ((strlen(gps_line_data)>7)
                      && strncmp(&(gps_line_data[3]),"RMC,",4)==0);
  retval = retval && (strchr("PNALB",gps_line_data[2])!=0);
  return (retval);
}
