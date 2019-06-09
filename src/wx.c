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


//
// The code currently supports these types of locally-connected or
// network-connected weather stations:
//
//   Peet Brothers Ultimeter 2000 (Set to Data logging mode)
//   Peet Brothers Ultimeter 2000 (Set to Packet mode)
//   Peet Brothers Ultimeter 2000 (Set to Complete Record Mode)
//   Peet Brothers Ultimeter-II
//   Qualimetrics Q-Net?
//   Radio Shack WX-200/Huger WM-918/Oregon Scientific WM-918
//   Dallas One-Wire Weather Station (via OWW network daemon)
//   Davis Weather Monitor II/Wizard III/Vantage Pro (via meteo/db2APRS link)
//


// Need to modify code to use WX_rain_gauge_type.  Peet brothers.
// See http://www.peetbros.com, FAQ's and owner's manuals for
// details:
//
//  Peet Bros Ultimeter II:   0.1"/0.5mm or 0.01"/0.25mm
//      Divide by 10 or 100 from the serial output.
//
//  Peet Bros Ultimeter 2000, 800, & 100:  0.01"/0.25mm or 0.1mm
//      If 0.01" gauge, divide by 100.  If 0.1mm gauge, convert to
//      proper English units.



#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif  // HAVE_CONFIG_H

#include "snprintf.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#if TIME_WITH_SYS_TIME
  #include <sys/time.h>
  #include <time.h>
#else   // TIME_WITH_SYS_TIME
  #if HAVE_SYS_TIME_H
    #include <sys/time.h>
  #else  // HAVE_SYS_TIME_H
    #include <time.h>
  #endif // HAVE_SYS_TIME_H
#endif  // TIME_WITH_SYS_TIME

#include <string.h>

#include <Xm/XmAll.h>

#include "wx.h"
#include "main.h"
#include "xastir.h"
#include "interface.h"
#include "lang.h"
#include "util.h"

// Must be last include file
#include "leak_detection.h"



#define MAX_RAW_WX_STRING 800

char wx_station_type[100];
char raw_wx_string[MAX_RAW_WX_STRING+1];

#define MAX_WX_STRING 300
#define WX_TYPE 'X'

/* rain totals */
float rain_minute[60];              // Total rain for each min. of last hour, hundredths of an inch
float rain_minute_total = 0.0;      // Total for last hour, hundredths of an inch
int rain_minute_last_write = -1;    // Write pointer for rain_minute[] array, set to an invalid number
float rain_00 = 0.0;                // hundredths of an inch
float rain_24 = 0.0;                // hundredths of an inch
float rain_base[24];                // hundredths of an inch
int rain_check = 0;                 // Flag for re-checking rain_total each hour

float wind_chill = 0;               // holder for wind chill calc

// Gust totals
float gust[60];                     // High wind gust for each min. of last hour
int gust_write_ptr = 0;
int gust_read_ptr = 0;
int gust_last_write = 0;


/* Other WX station data */
char wx_dew_point[10];
char wx_dew_point_on;
char wx_high_wind[10];
char wx_high_wind_on;
char wx_wind_chill[10];
char wx_wind_chill_on;
char wx_three_hour_baro[10];  // hPa
char wx_three_hour_baro_on;
char wx_hi_temp[10];
char wx_hi_temp_on;
char wx_low_temp[10];
char wx_low_temp_on;
char wx_heat_index[10];
char wx_heat_index_on;





/***********************************************************/
/* clear rain data                                         */
/***********************************************************/
void clear_rain_data(void)
{
  int x;


  // Clear rain_base queue (starting rain total for each hour)
  for ( x = 0; x < 24; x++ )
  {
    rain_base[x] = 0.0;
  }

  rain_00 = 0.0;
  rain_24 = 0.0;
  rain_check = 0;   // Set flag so we'll recheck rain_total
  // a few times at start of each hour

  // Clear hourly rain queue
  for ( x = 0; x < 60; x++ )
  {
    rain_minute[x] = 0.0;
  }

  rain_minute_total = 0.0;
  rain_minute_last_write = 70;  // Invalid number so we'll know we're just starting.
}





/**************************************************************/
/* compute_rain_hour - rolling average for the last 59.x      */
/* minutes of rain.  I/O numbers are in hundredths of an inch.*/
/* Output is in "rain_minute_total", a global variable.       */
/**************************************************************/
void compute_rain_hour(float rain_total)
{
  int current, j;
  float lowest;


  // Deposit the _starting_ rain_total for each minute into a separate bucket.
  // Subtract lowest rain_total in queue from current rain_total to get total
  // for the last hour.


  current = get_minutes(); // Fetch the current minute value.  Use this as an index
  // into our minute "buckets" where we store the data.


  rain_minute[ (current + 1) % 60 ] = 0.0;   // Zero out the next bucket (probably have data in
  // there from the previous hour).


  if (rain_minute[current] == 0.0)           // If no rain_total stored yet in this minute's bucket
  {
    rain_minute[current] = rain_total;  // Write into current bucket
  }


  // Find the lowest non-zero value for rain_total.  The max value is "rain_total".
  lowest = rain_total;                    // Set to maximum to get things going
  for (j = 0; j < 60; j++)
  {
    if ( (rain_minute[j] > 0.0) && (rain_minute[j] < lowest) )   // Found a lower non-zero value?
    {
      lowest = rain_minute[j];        // Snag it
    }
  }
  // Found it, subtract the two to get total for the last hour
  rain_minute_total = rain_total - lowest;

  if (debug_level & 2)
  {
    fprintf(stderr,"Rain_total:%0.2f  Hourly:%0.2f  (Low:%0.2f)  ", rain_total, rain_minute_total, lowest);
  }
}





/***********************************************************/
/* compute_rain - compute rain totals from the total rain  */
/* so far.  rain_total (in hundredths of an inch) keeps on */
/* incrementing.                                           */
/***********************************************************/
void compute_rain(float rain_total)
{
  int current, i;
  float lower;


  // Skip the routine if input is outlandish (Negative value, zero, or 512 inches!).
  // We seem to get occasional 0.0 packets from wx200d.  This makes them go away.
  if ( (rain_total <= 0.0) || (rain_total > 51200.0) )
  {
    return;
  }

  compute_rain_hour(rain_total);

  current = get_hours();

  // Set rain_base:  The first rain_total for each hour.
  if (rain_base[current] == 0.0)         // If we don't have a start value yet for this hour,
  {
    rain_base[current] = rain_total;    // save it away.
    rain_check = 0;                     // Set up rain_check so we'll do the following
    // "else" clause a few times at start of each hour.
  }
  else    // rain_base has been set, is it wrong?  We recheck three times at start of hour.
  {
    if (rain_check < 3)
    {
      rain_check++;
      // Is rain less than base?  It shouldn't be.
      if (rain_total < rain_base[current])
      {
        rain_base[current] = rain_total;
      }

      // Difference greater than 10 inches since last reading?  It shouldn't be.
      if (fabs(rain_total - rain_base[current]) > 1000.0) // Remember:  Hundredths of an inch
      {
        rain_base[current] = rain_total;
      }
    }
  }
  rain_base[ (current + 1) % 24 ] = 0.0;    // Clear next hour's index.


  // Compute total rain in last 24 hours:  Really we'll compute the total rain
  // in the last 23 hours plus the portion of an hour we've completed (Sum up
  // all 24 of the hour totals).  This isn't the perfect way to do it, but to
  // really do it right we'd need finer increments of time (to get closer to
  // the goal of 24 hours of rain).
  lower = rain_total;
  for ( i = 0; i < 24; i++ )      // Find the lowest non-zero rain_base value in last 24 hours
  {
    if ( (rain_base[i] > 0.0) && (rain_base[i] < lower) )
    {
      lower = rain_base[i];
    }
  }
  rain_24 = rain_total - lower;    // Hundredths of an inch


  // Compute rain since midnight.  Note that this uses whatever local time was set
  // on the machine.  It might not be local midnight if your box is set to GMT.
  lower = rain_total;
  for ( i = 0; i <= current; i++ )    // Find the lowest non-zero rain_base value since midnight
  {
    if ( (rain_base[i] > 0.0) && (rain_base[i] < lower) )
    {
      lower = rain_base[i];
    }
  }
  rain_00 = rain_total - lower;    // Hundredths of an inch

  // It is the responsibility of the calling program to save
  // the new totals in the data structure for our station.
  // We don't return anything except in global variables.


  if (debug_level & 2)
  {
    fprintf(stderr,"24hrs:%0.2f  ", rain_24);
    fprintf(stderr,"rain_00:%0.2f\n", rain_00);
  }
}





/**************************************************************/
/* compute_gust - compute max wind gust during last 5 minutes */
/*                                                            */
/**************************************************************/
float compute_gust(float wx_speed, float UNUSED(last_speed), time_t *last_speed_time)
{
  float computed_gust;
  int current, j;


  // Deposit max gust for each minute into a different bucket.
  // Check all buckets for max gust within the last five minutes
  // (Really 4 minutes plus whatever portion of a minute we've completed).

  current = get_minutes(); // Fetch the current minute value.  We use this as an index
  // into our minute "buckets" where we store the data.

  // If we haven't started collecting yet, set up to do so
  if (gust_read_ptr == gust_write_ptr)    // We haven't started yet
  {
    gust_write_ptr = current;           // Set to write into current bucket
    gust_last_write = current;

    gust_read_ptr = current - 1;        // Set read pointer back one, modulus 60
    if (gust_read_ptr < 0)
    {
      gust_read_ptr = 59;
    }

    gust[gust_write_ptr] = 0.0;         // Zero the max gust
    gust[gust_read_ptr] = 0.0;          // for both buckets.

//WE7U: Debug
//gust[gust_write_ptr] = 45.9;
  }

  // Check whether we've advanced at least one minute yet
  if (current != gust_write_ptr)          // We've advanced to a different minute
  {
    gust_write_ptr = current;           // Start writing into a new bucket.
    gust[gust_write_ptr] = 0.0;         // Zero the new bucket

    // Check how many bins of real data we have currently.  Note that this '5' is
    // correct, as we just advanced "current" to the next minute.  We're just pulling
    // along the read_ptr behind us if we have 5 bins worth of data by now.
    if ( ((gust_read_ptr + 5) % 60) == gust_write_ptr)  // We have 5 bins of real data
    {
      gust_read_ptr = (gust_read_ptr + 1) % 60;  // So advance the read pointer,
    }

    // Check for really bad pointers, perhaps the weather station got
    // unplugged for a while or it's just REALLY slow at sending us data?
    // We're checking to see if gust_last_write happened in the previous
    // minute.  If not, we skipped a minute or more somewhere.
    if ( ((gust_last_write + 1) % 60) != current )
    {
      // We lost some time somewhere: Reset the pointers, older gust data is
      // lost.  Start over collecting new gust data.

      gust_read_ptr = current - 1;    // Set read pointer back one, modulus 60
      if (gust_read_ptr < 0)
      {
        gust_read_ptr = 59;
      }

      gust[gust_read_ptr] = 0.0;
    }
    gust_last_write = current;
  }

  // Is current wind speed higher than the current minute bucket?
  if (wx_speed > gust[gust_write_ptr])
  {
    gust[gust_write_ptr] = wx_speed;  // Save it in the bucket
  }

  // Read the last (up to) five buckets and find the max gust
  computed_gust=gust[gust_write_ptr];
  j = gust_read_ptr;
  while (j != ((gust_write_ptr + 1) % 60) )
  {
    if ( computed_gust < gust[j] )
    {
      computed_gust = gust[j];
    }
    j = (j + 1) % 60;
  }

  if (debug_level & 2)
  {
    j = gust_read_ptr;
    while (j != ((gust_write_ptr + 1) % 60) )
    {
      fprintf(stderr,"%0.2f   ", gust[j]);
      j = (j + 1) % 60;
    }
    fprintf(stderr,"gust:%0.2f\n", computed_gust);
  }

  *last_speed_time = sec_now();
  return(computed_gust);
}





//
// cycle_weather - keep the weather queues moving even if data from
// weather station is scarce.  This is called from main.c:UpdateTime()
// on a periodic basis.  This routine also does the 30 second timestamp
// for the log files.
//
void cycle_weather(void)
{
  DataRow *p_station;
  WeatherRow *weather;
  float last_speed, computed_gust;
  time_t last_speed_time;


  // Find my own local weather data
  if (search_station_name(&p_station,my_callsign,1))
  {
    if (p_station->weather_data != NULL)    // If station has WX data
    {
      weather = p_station->weather_data;
      // Cycle the rain queues, feed in the last rain total we had
      (void)compute_rain((float)atof(weather->wx_rain_total));


      // Note:  Some weather stations provide the per-hour, 24-hour,
      // and since-midnight rain rates already.  Further, some stations
      // don't even provide total rain (Davis APRS DataLogger and the
      // db2APRS program), so anything we compute here is actually wrong.
      // Do NOT clobber these if so.  This flag is set in fill_wx_data
      // when the station provides its data.

      if (weather->wx_compute_rain_rates)
      {
        // Hourly rain total
        xastir_snprintf(weather->wx_rain,
                        sizeof(weather->wx_rain),
                        "%0.2f",
                        rain_minute_total);

        // Last 24 hour rain
        xastir_snprintf(weather->wx_prec_24,
                        sizeof(weather->wx_prec_24),
                        "%0.2f",
                        rain_24);

        // Rain since midnight
        xastir_snprintf(weather->wx_prec_00,
                        sizeof(weather->wx_prec_00),
                        "%0.2f",
                        rain_00);
      }
      else
      {
        // LaCrosse stations don't provide the since-midnight
        // numbers and so this will be blank.
        // So if we have blanks here, fill it in.
        if ( weather->wx_prec_00[0] == '\0' &&
             weather->wx_rain_total[0] != '\0')
        {

          // Rain since midnight
          xastir_snprintf(weather->wx_prec_00,
                          sizeof(weather->wx_prec_00),
                          "%0.2f",
                          rain_00);
        }
      }


      /* get last gust speed */
      if (strlen(weather->wx_gust) > 0)
      {
        /* get last speed */
        last_speed = (float)atof(weather->wx_gust);
        last_speed_time = weather->wx_speed_sec_time;
      }
      else
      {
        last_speed = 0.0;
      }

      /* wind speed */
      computed_gust = compute_gust((float)atof(weather->wx_speed),
                                   last_speed,
                                   &last_speed_time);
      weather->wx_speed_sec_time = sec_now();
      xastir_snprintf(weather->wx_gust,
                      sizeof(weather->wx_gust),
                      "%03d",
                      (int)(computed_gust + 0.5)); // Cheater's way of rounding
    }
  }
}





/***********************************************************/
/* clear other wx data                                     */
/***********************************************************/
void clear_local_wx_data(void)
{
  memset(wx_dew_point,0,sizeof(wx_dew_point));
  wx_dew_point_on = 0;

  memset(wx_high_wind,0,sizeof(wx_high_wind));
  wx_high_wind_on = 0;

  memset(wx_wind_chill,0,sizeof(wx_wind_chill));
  wx_wind_chill_on = 0;

  memset(wx_three_hour_baro,0,sizeof(wx_three_hour_baro));
  wx_three_hour_baro_on = 0;

  memset(wx_hi_temp,0,sizeof(wx_hi_temp));
  wx_hi_temp_on = 0;

  memset(wx_low_temp,0,sizeof(wx_low_temp));
  wx_low_temp_on = 0;

  memset(wx_heat_index,0,sizeof(wx_heat_index));
  wx_heat_index_on = 0;
}





/***************************************************/
/* Check last WX data received - clear data if old */
/***************************************************/
void wx_last_data_check(void)
{
  DataRow *p_station;

  p_station = NULL;
  if (search_station_name(&p_station,my_callsign,1))
  {
    if (p_station->weather_data != NULL)
      if (p_station->weather_data->wx_speed_sec_time+360 < sec_now())
        if (p_station->weather_data->wx_gust[0] != 0)
          xastir_snprintf(p_station->weather_data->wx_gust,
                          sizeof(p_station->weather_data->wx_gust),
                          "%03d",
                          0);
  }
}





//*********************************************************************
// Decode Peet Brothers Ultimeter 2000 weather data (Data logging mode)
//
// This function is called from db.c:data_add() only.  Used for
// decoding incoming packets, not for our own weather station data.
//
// The Ultimeter 2000 can be in any of three modes, Data Logging Mode,
// Packet Mode, or Complete Record Mode.  This routine handles only
// the Data Logging Mode.
//*********************************************************************
void decode_U2000_L(int from, unsigned char *data, WeatherRow *weather)
{
  time_t last_speed_time;
  float last_speed;
  float computed_gust;
  char temp_data1[10];
  char *temp_conv;

  last_speed = 0.0;
  last_speed_time = 0;
  computed_gust = 0.0;

  if (debug_level & 1)
  {
    fprintf(stderr,"APRS WX3 Peet Bros U-2k (data logging mode): |%s|\n", data);
  }

  weather->wx_type = WX_TYPE;
  xastir_snprintf(weather->wx_station,
                  sizeof(weather->wx_station),
                  "U2k");

  /* get last gust speed */
  if (strlen(weather->wx_gust) > 0 && !from)
  {
    /* get last speed */
    last_speed = (float)atof(weather->wx_gust);
    last_speed_time = weather->wx_speed_sec_time;
  }

  // 006B 00 58
  // 00A4 00 46 01FF 380E 2755 02C1 03E8 ---- 0052 04D7    0001 007BM
  // ^       ^  ^    ^    ^         ^                      ^
  // 0       6  8    12   16        24                     40
  /* wind speed */
  if (data[0] != '-')   // '-' signifies invalid data
  {
    substr(temp_data1,(char *)data,4);
    xastir_snprintf(weather->wx_speed,
                    sizeof(weather->wx_speed),
                    "%03.0f",
                    0.5 + (strtol(temp_data1,&temp_conv,16)/10.0)*0.62137);
    if (from)
    {
      weather->wx_speed_sec_time = sec_now();
    }
    else
    {
      /* local station */
      computed_gust = compute_gust((float)atof(weather->wx_speed),
                                   last_speed,
                                   &last_speed_time);
      weather->wx_speed_sec_time = sec_now();
      xastir_snprintf(weather->wx_gust,
                      sizeof(weather->wx_gust),
                      "%03d",
                      (int)(0.5 + computed_gust)); // Cheater's way of rounding
    }
  }
  else
  {
    if (!from)
    {
      weather->wx_speed[0] = 0;
    }
  }

  /* wind direction */
  //
  // Note that the first two digits here may be 00, or may be FF
  // if a direction calibration has been entered.  We should zero
  // them.
  //
  if (data[4] != '-')   // '-' signifies invalid data
  {
    substr(temp_data1,(char *)(data+4),4);
    temp_data1[0] = '0';
    temp_data1[1] = '0';
    xastir_snprintf(weather->wx_course,
                    sizeof(weather->wx_course),
                    "%03.0f",
                    ((strtol(temp_data1,&temp_conv,16)/256.0)*360.0));
  }
  else
  {
    xastir_snprintf(weather->wx_course,
                    sizeof(weather->wx_course),
                    "000");
    if (!from)
    {
      weather->wx_course[0]=0;
    }
  }

  /* outdoor temp */
  if (data[8] != '-')   // '-' signifies invalid data
  {
    int temp4;

    substr(temp_data1,(char *)(data+8),4);
    temp4 = (int)strtol(temp_data1,&temp_conv,16);

    if (temp_data1[0] > '7')    // Negative value, convert
    {
      temp4 = (temp4 & (temp4-0x7FFF)) - 0x8000;
    }

    xastir_snprintf(weather->wx_temp,
                    sizeof(weather->wx_temp),
                    "%03d",
                    (int)((float)((temp4<<16)/65536)/10.0));

  }
  else
  {
    if (!from)
    {
      weather->wx_temp[0]=0;
    }
  }

  /* rain total long term */
  if (data[12] != '-')   // '-' signifies invalid data
  {
    substr(temp_data1,(char *)(data+12),4);
    xastir_snprintf(weather->wx_rain_total,
                    sizeof(weather->wx_rain_total),
                    "%0.2f",
                    strtol(temp_data1,&temp_conv,16)/100.0);
    if (!from)
    {
      /* local station */
      compute_rain((float)atof(weather->wx_rain_total));
      /*last hour rain */
      xastir_snprintf(weather->wx_rain,
                      sizeof(weather->wx_rain),
                      "%0.2f",
                      rain_minute_total);
      /*last 24 hour rain */
      xastir_snprintf(weather->wx_prec_24,
                      sizeof(weather->wx_prec_24),
                      "%0.2f",
                      rain_24);
      /* rain since midnight */
      xastir_snprintf(weather->wx_prec_00,
                      sizeof(weather->wx_prec_00),
                      "%0.2f",
                      rain_00);
    }
  }
  else
  {
    if (!from)
    {
      weather->wx_rain_total[0]=0;
    }
  }

  /* baro */
  if (data[16] != '-')   // '-' signifies invalid data
  {
    substr(temp_data1,(char *)(data+16),4);
    xastir_snprintf(weather->wx_baro,
                    sizeof(weather->wx_baro),
                    "%0.1f",
                    strtol(temp_data1,&temp_conv,16)/10.0);
  }
  else
  {
    if (!from)
    {
      weather->wx_baro[0]=0;
    }
  }


  /* outdoor humidity */
  if (data[24] != '-')   // '-' signifies invalid data
  {
    substr(temp_data1,(char *)(data+24),4);
    xastir_snprintf(weather->wx_hum,
                    sizeof(weather->wx_hum),
                    "%03.0f",
                    (strtol(temp_data1,&temp_conv,16)/10.0));
  }
  else
  {
    if (!from)
    {
      weather->wx_hum[0]=0;
    }
  }

  /* todays rain total */
  if (data[40] != '-')   // '-' signifies invalid data
  {
    if (from)
    {
      substr(temp_data1,(char *)(data+40),4);
      xastir_snprintf(weather->wx_prec_00,
                      sizeof(weather->wx_prec_00),
                      "%0.2f",
                      strtol(temp_data1,&temp_conv,16)/100.0);
    }
  }
  else
  {
    if (!from)
    {
      weather->wx_prec_00[0] = 0;
    }
  }
}





//********************************************************************
// Decode Peet Brothers Ultimeter 2000 weather data (Packet mode)
//
// This function is called from db.c:data_add() only.  Used for
// decoding incoming packets, not for our own weather station data.
//
// The Ultimeter 2000 can be in any of three modes, Data Logging Mode,
// Packet Mode, or Complete Record Mode.  This routine handles only
// the Packet Mode.
//********************************************************************
void decode_U2000_P(int from, unsigned char *data, WeatherRow *weather)
{
  time_t last_speed_time;
  float last_speed;
  float computed_gust;
  char temp_data1[10];
  char *temp_conv;
  int len;

  last_speed      = 0.0;
  last_speed_time = 0;
  computed_gust   = 0.0;
  len = (int)strlen((char *)data);

  if (debug_level & 1)
  {
    fprintf(stderr,"APRS WX5 Peet Bros U-2k Packet (Packet mode): |%s|\n",data);
  }

  weather->wx_type = WX_TYPE;
  xastir_snprintf(weather->wx_station,
                  sizeof(weather->wx_station),
                  "U2k");

  /* get last gust speed */
  if (strlen(weather->wx_gust) > 0 && !from)
  {
    /* get last speed */
    last_speed = (float)atof(weather->wx_gust);
    last_speed_time = weather->wx_speed_sec_time;
  }

  // $ULTW   0031 00 37 02CE  0069 ---- 0000 86A0 0001 ---- 011901CC   0000 0005
  //         ^       ^  ^     ^    ^                   ^               ^    ^
  //         0       6  8     12   16                  32              44   48

  /* wind speed peak over last 5 min */
  if (data[0] != '-')   // '-' signifies invalid data
  {
    substr(temp_data1,(char *)data,4);
    if (from)
    {
      xastir_snprintf(weather->wx_gust,
                      sizeof(weather->wx_gust),
                      "%03.0f",
                      0.5 + (strtol(temp_data1,&temp_conv,16)/10.0)*0.62137);
      /* this may be the only wind data */
      xastir_snprintf(weather->wx_speed,
                      sizeof(weather->wx_speed),
                      "%03.0f",
                      0.5 + (strtol(temp_data1,&temp_conv,16)/10.0)*0.62137);
    }
    else
    {
      /* local station and may be the only wind data */
      if (len < 51)
      {
        xastir_snprintf(weather->wx_speed,
                        sizeof(weather->wx_speed),
                        "%03.0f",
                        0.5 + (strtol(temp_data1,&temp_conv,16)/10.0)*0.62137);
        computed_gust = compute_gust((float)atof(weather->wx_speed),
                                     last_speed,
                                     &last_speed_time);
        weather->wx_speed_sec_time = sec_now();
        xastir_snprintf(weather->wx_gust,
                        sizeof(weather->wx_gust),
                        "%03d",
                        (int)(0.5 + computed_gust));
      }
    }
  }
  else
  {
    if (!from)
    {
      weather->wx_gust[0] = 0;
    }
  }

  /* wind direction */
  //
  // Note that the first two digits here may be 00, or may be FF
  // if a direction calibration has been entered.  We should zero
  // them.
  //
  if (data[4] != '-')   // '-' signifies invalid data
  {
    substr(temp_data1,(char *)(data+4),4);
    temp_data1[0] = '0';
    temp_data1[1] = '0';
    xastir_snprintf(weather->wx_course,
                    sizeof(weather->wx_course),
                    "%03.0f",
                    (strtol(temp_data1,&temp_conv,16)/256.0)*360.0);
  }
  else
  {
    xastir_snprintf(weather->wx_course,
                    sizeof(weather->wx_course),
                    "000");
    if (!from)
    {
      weather->wx_course[0] = 0;
    }
  }

  /* outdoor temp */
  if (data[8] != '-')   // '-' signifies invalid data
  {
    int temp4;

    substr(temp_data1,(char *)(data+8),4);
    temp4 = (int)strtol(temp_data1,&temp_conv,16);

    if (temp_data1[0] > '7')    // Negative value, convert
    {
      temp4 = (temp4 & (temp4-0x7FFF)) - 0x8000;
    }

    xastir_snprintf(weather->wx_temp,
                    sizeof(weather->wx_temp),
                    "%03d",
                    (int)((float)((temp4<<16)/65536)/10.0));
  }
  else
  {
    if (!from)
    {
      weather->wx_temp[0] = 0;
    }
  }
  /* todays rain total (on some units) */
  if ((data[44]) != '-')   // '-' signifies invalid data
  {
    if (from)
    {
      substr(temp_data1,(char *)(data+44),4);
      xastir_snprintf(weather->wx_prec_00,
                      sizeof(weather->wx_prec_00),
                      "%0.2f",
                      strtol(temp_data1,&temp_conv,16)/100.0);
    }
  }
  else
  {
    if (!from)
    {
      weather->wx_prec_00[0] = 0;
    }
  }

  /* rain total long term */
  if (data[12] != '-')   // '-' signifies invalid data
  {
    substr(temp_data1,(char *)(data+12),4);
    xastir_snprintf(weather->wx_rain_total,
                    sizeof(weather->wx_rain_total),
                    "%0.2f",
                    strtol(temp_data1,&temp_conv,16)/100.0);
    if (!from)
    {
      /* local station */
      compute_rain((float)atof(weather->wx_rain_total));
      /*last hour rain */
      xastir_snprintf(weather->wx_rain,
                      sizeof(weather->wx_rain),
                      "%0.2f",
                      rain_minute_total);
      /*last 24 hour rain */
      xastir_snprintf(weather->wx_prec_24,
                      sizeof(weather->wx_prec_24),
                      "%0.2f",
                      rain_24);
      /* rain since midnight */
      xastir_snprintf(weather->wx_prec_00,
                      sizeof(weather->wx_prec_00),
                      "%0.2f",
                      rain_00);
    }
  }
  else
  {
    if (!from)
    {
      weather->wx_rain_total[0] = 0;
    }
  }

  /* baro */
  if (data[16] != '-')   // '-' signifies invalid data
  {
    substr(temp_data1,(char *)(data+16),4);
    xastir_snprintf(weather->wx_baro,
                    sizeof(weather->wx_baro),
                    "%0.1f",
                    strtol(temp_data1,&temp_conv,16)/10.0);
  }
  else
  {
    if (!from)
    {
      weather->wx_baro[0] = 0;
    }
  }

  /* outdoor humidity */
  if (data[32] != '-')   // '-' signifies invalid data
  {
    substr(temp_data1,(char *)(data+32),4);
    xastir_snprintf(weather->wx_hum,
                    sizeof(weather->wx_hum),
                    "%03.0f",
                    strtol(temp_data1,&temp_conv,16)/10.0);
  }
  else
  {
    if (!from)
    {
      weather->wx_hum[0] = 0;
    }
  }

  /* 1 min wind speed avg */
  if (len > 48 && (data[48]) != '-')   // '-' signifies invalid data
  {
    substr(temp_data1,(char *)(data+48),4);
    xastir_snprintf(weather->wx_speed,
                    sizeof(weather->wx_speed),
                    "%03.0f",
                    0.5 + (strtol(temp_data1,&temp_conv,16)/10.0)*0.62137);
    if (from)
    {
      weather->wx_speed_sec_time = sec_now();
    }
    else
    {
      /* local station */
      computed_gust = compute_gust((float)atof(weather->wx_speed),
                                   last_speed,
                                   &last_speed_time);
      weather->wx_speed_sec_time = sec_now();
      xastir_snprintf(weather->wx_gust,
                      sizeof(weather->wx_gust),
                      "%03d",
                      (int)(0.5 + computed_gust));
    }
  }
  else
  {
    if (!from)
    {
      if (len > 48)
      {
        weather->wx_speed[0] = 0;
      }
    }
  }
}





//*****************************************************************
// Decode Peet Brothers Ultimeter-II weather data
//
// This function is called from db.c:data_add() only.  Used for
// decoding incoming packets, not for our own weather station data.
//*****************************************************************
void decode_Peet_Bros(int from, unsigned char *data, WeatherRow *weather, int type)
{
  time_t last_speed_time;
  float last_speed;
  float computed_gust;
  char temp_data1[10];
  char *temp_conv;

  last_speed    = 0.0;
  computed_gust = 0.0;
  last_speed_time = 0;

  if (debug_level & 1)
  {
    fprintf(stderr,"APRS WX4 Peet Bros U-II: |%s|\n",data);
  }

  weather->wx_type = WX_TYPE;
  xastir_snprintf(weather->wx_station,
                  sizeof(weather->wx_station),
                  "UII");

  // '*' = MPH
  // '#' = km/h
  //
  // #  5 0B 75 0082 0082
  // *  7 00 76 0000 0000
  //    ^ ^  ^  ^
  //            rain [1/100 inch ?]
  //         outdoor temp
  //      wind speed [mph / km/h]
  //    wind dir

  /* wind direction */
  //
  // 0x00 is N
  // 0x04 is E
  // 0x08 is S
  // 0x0C is W
  //
  substr(temp_data1,(char *)data,1);
  xastir_snprintf(weather->wx_course,
                  sizeof(weather->wx_course),
                  "%03.0f",
                  (strtol(temp_data1,&temp_conv,16)/16.0)*360.0);

  /* get last gust speed */
  if (strlen(weather->wx_gust) > 0 && !from)
  {
    /* get last speed */
    last_speed = (float)atof(weather->wx_gust);
    last_speed_time = weather->wx_speed_sec_time;
  }

  /* wind speed */
  substr(temp_data1,(char *)(data+1),2);
  if (type == APRS_WX4)       // '#'  speed in km/h, convert to mph
  {
    xastir_snprintf(weather->wx_speed,
                    sizeof(weather->wx_speed),
                    "%03d",
                    (int)(0.5 + (float)(strtol(temp_data1,&temp_conv,16)*0.62137)));
  }
  else     // type == APRS_WX6,  '*'  speed in mph
  {
    xastir_snprintf(weather->wx_speed,
                    sizeof(weather->wx_speed),
                    "%03.0f",
                    0.5 + (1.0 * strtol(temp_data1,&temp_conv,16)) );
  }

  if (from)
  {
    weather->wx_speed_sec_time = sec_now();
  }
  else
  {
    /* local station */
    computed_gust = compute_gust((float)atof(weather->wx_speed),
                                 last_speed,
                                 &last_speed_time);
    weather->wx_speed_sec_time = sec_now();
    xastir_snprintf(weather->wx_gust,
                    sizeof(weather->wx_gust),
                    "%03d",
                    (int)(0.5 + computed_gust));
  }

  /* outdoor temp */
  if (data[3] != '-')   // '-' signifies invalid data
  {
    int temp4;

    substr(temp_data1,(char *)(data+3),2);
    temp4 = (int)strtol(temp_data1,&temp_conv,16);

    if (temp_data1[0] > '7')    // Negative value, convert
    {
      temp4 = (temp4 & (temp4-0x7FFF)) - 0x8000;
    }

    xastir_snprintf(weather->wx_temp,
                    sizeof(weather->wx_temp),
                    "%03.0f",
                    (float)(temp4-56) );
  }
  else
  {
    if (!from)
    {
      weather->wx_temp[0] = 0;
    }
  }

  // Rain divided by 100 for readings in hundredth of an inch
  if (data[5] != '-')   // '-' signifies invalid data
  {
    substr(temp_data1,(char *)(data+5),4);
    xastir_snprintf(weather->wx_rain_total,
                    sizeof(weather->wx_rain_total),
                    "%0.2f",
                    strtol(temp_data1,&temp_conv,16)/100.0);
    if (!from)
    {
      /* local station */
      compute_rain((float)atof(weather->wx_rain_total));
      /*last hour rain */
      xastir_snprintf(weather->wx_rain,
                      sizeof(weather->wx_rain),
                      "%0.2f",
                      rain_minute_total);
      /*last 24 hour rain */
      xastir_snprintf(weather->wx_prec_24,
                      sizeof(weather->wx_prec_24),
                      "%0.2f",
                      rain_24);
      /* rain since midnight */
      xastir_snprintf(weather->wx_prec_00,
                      sizeof(weather->wx_prec_00),
                      "%0.2f",
                      rain_00);
    }
  }
  else
  {
    if (!from)
    {
      weather->wx_rain_total[0] = 0;
    }
  }
}





/**************************************************/
/* RSW num convert.  For Radio Shack WX-200,      */
/* converts two decimal nibbles into integer      */
/* number.                                        */
/**************************************************/
int rswnc(unsigned char c)
{
  return( (int)( (c>>4) & 0x0f) * 10 + (int)(c&0x0f) );
}





//*********************************************************
// wx fill data field
// from: 0=local station, 1=regular decode (other stations)
// type: type of WX packet
// data: the packet of WX data
// fill: the station data to fill
//
// This function is called only by wx.c:wx_decode()
//
// It is always called with a first parameter of 0, so we
// use this only for our own serially-connected or network
// connected weather station, not for decoding other
// people's weather packets.
//*********************************************************
//
// Note that the length of "data" can be up to MAX_DEVICE_BUFFER,
// which is currently set to 4096.
//
void wx_fill_data(int from, int type, unsigned char *data, DataRow *fill)
{
  time_t last_speed_time;
  float last_speed;
  float computed_gust;
  int  temp1;
  int  temp2;
  int  temp3;
  float temp_temp;
  char temp[MAX_DEVICE_BUFFER+1];
  char temp_data1[10];
  char *temp_conv;
  int len;
  int t2;
  int hidx_temp;
  int rh2;
  int hi_hum;
  int heat_index;
  WeatherRow *weather;
  float tmp1,tmp2,tmp3,tmp4,tmp5,tmp6,tmp9,tmp10,tmp11,tmp12,tmp13,tmp14,tmp15,tmp16,tmp17,tmp18,tmp19;
  int tmp7,tmp8;
  int dallas_type = 19;


  last_speed=0.0;
  computed_gust=0.0;
  last_speed_time=0;


  len=(int)strlen((char*)data);

  weather = fill->weather_data;  // should always be defined

  switch (type)
  {

//WE7U
    /////////////////////////////////////
    // Dallas One-Wire Weather Station //
    /////////////////////////////////////

// KB1MTS - Added values for T13 thru T19 for humidity and barometer,
// however only current values (not min or max) are used.


    case (DALLAS_ONE_WIRE):

      if (debug_level & 1)
      {
        fprintf(stderr,"APRS WX Dallas One-Wire %s:<%s>\n",fill->call_sign,data);
      }

      weather->wx_type=WX_TYPE;
      xastir_snprintf(weather->wx_station,
                      sizeof(weather->wx_station),
                      "OWW");

      if (19 == sscanf((const char *)data,
                       "%f %f %f %f %f %f %d %d %f %f %f %f %f %f %f %f %f %f %f",
                       &tmp1,
                       &tmp2,
                       &tmp3,
                       &tmp4,
                       &tmp5,
                       &tmp6,
                       &tmp7,
                       &tmp8,
                       &tmp9,
                       &tmp10,
                       &tmp11,
                       &tmp12,
                       &tmp13,
                       &tmp14,
                       &tmp15,
                       &tmp16,
                       &tmp17,
                       &tmp18,
                       &tmp19))
      {
        dallas_type = 19;
      }
      else if (12 == sscanf((const char *)data,
                            "%f %f %f %f %f %f %d %d %f %f %f %f",
                            &tmp1,
                            &tmp2,
                            &tmp3,
                            &tmp4,
                            &tmp5,
                            &tmp6,
                            &tmp7,
                            &tmp8,
                            &tmp9,
                            &tmp10,
                            &tmp11,
                            &tmp12))
      {
        dallas_type = 12;
      }
      else
      {
        fprintf(stderr,"wx_fill_data:sscanf parsing error\n");
      }


      // The format of the data originates here:
      // http://weather.henriksens.net/

      // tmp1: primary temp (C)
      // tmp2: temp max (C)
      // tmp3: temp min (C)
      // tmp4: anemometer (mps)
      // tmp5: anemometer gust (peak speed MS)
      // tmp6: anemometer speed max * 0.447040972 (max speed MS)
      // tmp7: vane bearing - 1 (current wind direction)
      // tmp8: vane mode (max dir)
      // tmp9: rain rate
      // tmp10: rain total today
      // tmp11: rain total week
      // tmp12: rain since month
      // tmp13: Current Humidity
      // tmp14: Max Humidity
      // tmp15: Min Humidity
      // tmp16: Current Barometer
      // tmp17: Max Barometer
      // tmp18: Min Barometer
      // tmp19: Barometer Rate

      // Temperature
      xastir_snprintf(weather->wx_temp,
                      sizeof(weather->wx_temp),
                      "%03d",
                      (int)(tmp1 * 9.0 / 5.0 + 32.0 + 0.5));
      //fprintf(stderr,"Read: %2.1f C, Storing: %s F\n",tmp1,weather->wx_temp);

      // Wind direction.  Each vane increment equals 22.5 degrees.
      xastir_snprintf(weather->wx_course,
                      sizeof(weather->wx_course),
                      "%03d",
                      (int)(tmp7 * 22.5 + 0.5));

      // Check for course = 0.  Change to 360.
      if (strncmp(weather->wx_course,"000",3) == 0)
      {
        xastir_snprintf(weather->wx_course,
                        sizeof(weather->wx_course),
                        "360");
      }

      // Wind speed.  We get it in meters per second, store it
      // in mph.
      tmp4 = tmp4 * 3600.0 / 1000.0; // kph
      tmp4 = tmp4 * 0.62137;         // mph
      xastir_snprintf(weather->wx_speed,
                      sizeof(weather->wx_speed),
                      "%03d",
                      (int)(tmp4 + 0.5));

      if (dallas_type == 19)
      {
        // Humidity.  This is received by percentage.
        xastir_snprintf(weather->wx_hum,
                        sizeof(weather->wx_hum),
                        "%2.1f", (double)(tmp13));

        // Barometer. Sent in inHg
        xastir_snprintf(weather->wx_baro,
                        sizeof(weather->wx_baro),
                        "%4.4f", (float)(tmp16 * 33.864));
      }


// Rain:  I don't have a rain gauge, and I couldn't tell from the
// "OWW" docs exactly which of the four rain fields did what.  If
// someone can help me with that I'll add rain gauge code for the
// Dallas One-Wire.



      break;

    ////////////////////////////////
    // Peet Brothers Ultimeter-II //
    ////////////////////////////////
    case (APRS_WX4):    // '#', Wind speed in km/h
    case (APRS_WX6):    // '*', Wind speed in mph

      // This one assumes 0.1" rain gauge.  Must correct in software if
      // any other type is used.

      if (debug_level & 1)
      {
        fprintf(stderr,"APRS WX4 Peet Bros U-II %s:<%s>\n",fill->call_sign,data);
      }

      weather->wx_type=WX_TYPE;
      xastir_snprintf(weather->wx_station,
                      sizeof(weather->wx_station),
                      "UII");

      /* wind direction */
      //
      // 0x00 is N
      // 0x04 is E
      // 0x08 is S
      // 0x0C is W
      //
      substr(temp_data1,(char *)(data+1),1);
      xastir_snprintf(weather->wx_course,
                      sizeof(weather->wx_course),
                      "%03.0f",
                      (strtol(temp_data1,&temp_conv,16)/16.0)*360.0);

      // Check for course == 0.  Change to 360.
      if (strncmp(weather->wx_course,"000",3) == 0)
      {
        xastir_snprintf(weather->wx_course,
                        sizeof(weather->wx_course),
                        "360");
      }

      /* get last gust speed */
      if (strlen(weather->wx_gust) > 0 && !from)      // From local station
      {
        /* get last speed */
        last_speed=(float)atof(weather->wx_gust);
        last_speed_time=weather->wx_speed_sec_time;
      }

      /* wind speed */
      substr(temp_data1,(char *)(data+2),2);
      if (type == APRS_WX4)   // '#', Data is in km/h, convert to mph
      {
        xastir_snprintf(weather->wx_speed,
                        sizeof(weather->wx_speed),
                        "%03d",
                        (int)(0.5 + (float)(strtol(temp_data1,&temp_conv,16)*0.62137)));
      }
      else     // APRS_WX6 or '*', Data is in MPH
      {
        xastir_snprintf(weather->wx_speed,
                        sizeof(weather->wx_speed),
                        "%03.0f",
                        0.5 + (strtol(temp_data1,&temp_conv,16)*1.0) );
      }

      if (from)   // From remote station
      {
        weather->wx_speed_sec_time = sec_now();
      }
      else
      {
        /* local station */
        computed_gust = compute_gust((float)atof(weather->wx_speed),
                                     last_speed,
                                     &last_speed_time);
        weather->wx_speed_sec_time = sec_now();
        xastir_snprintf(weather->wx_gust,
                        sizeof(weather->wx_gust),
                        "%03d",
                        (int)(0.5 + computed_gust));
      }

      /* outdoor temp */
      if (data[4]!='-')   // '-' signifies invalid data
      {
        int temp4;

        substr(temp_data1,(char *)(data+4),2);
        temp4 = (int)strtol(temp_data1,&temp_conv,16);

        if (temp_data1[0] > '7')    // Negative value, convert
        {
          temp4 = (temp4 & (temp4-0x7FFF)) - 0x8000;
        }

        xastir_snprintf(weather->wx_temp,
                        sizeof(weather->wx_temp),
                        "%03.0f",
                        (float)(temp4-56) );
      }
      else
      {
        if (!from)  // From local station
        {
          weather->wx_temp[0]=0;
        }
      }

      /* rain div by 100 for readings 0.1 inch */
// What?  Shouldn't this be /10?

      if (data[6]!='-')   // '-' signifies invalid data
      {
        substr(temp_data1,(char *)(data+6),4);
        if (!from)      // From local station
        {
          switch (WX_rain_gauge_type)
          {
            case 1: // 0.1" rain gauge
              xastir_snprintf(weather->wx_rain_total,
                              sizeof(weather->wx_rain_total),
                              "%0.2f",
                              strtol(temp_data1,&temp_conv,16)*10.0);
              break;
            case 3: // 0.1mm rain gauge
              xastir_snprintf(weather->wx_rain_total,
                              sizeof(weather->wx_rain_total),
                              "%0.2f",
                              strtol(temp_data1,&temp_conv,16)/2.54);
              break;
            case 2: // 0.01" rain gauge
            case 0: // No conversion
            default:
              xastir_snprintf(weather->wx_rain_total,
                              sizeof(weather->wx_rain_total),
                              "%0.2f",
                              strtol(temp_data1,&temp_conv,16)*1.0);
              break;
          }
          /* local station */
          compute_rain((float)atof(weather->wx_rain_total));
          weather->wx_compute_rain_rates=1;
          /*last hour rain */
          xastir_snprintf(weather->wx_rain,
                          sizeof(weather->wx_rain),
                          "%0.2f",
                          rain_minute_total);
          /*last 24 hour rain */
          xastir_snprintf(weather->wx_prec_24,
                          sizeof(weather->wx_prec_24),
                          "%0.2f",
                          rain_24);
          /* rain since midnight */
          xastir_snprintf(weather->wx_prec_00,
                          sizeof(weather->wx_prec_00),
                          "%0.2f",
                          rain_00);
        }
      }
      else
      {
        if (!from)  // From local station
        {
          weather->wx_rain_total[0]=0;
        }
      }
      break;



    ///////////////////////////////////////////////////////
    // Peet Brothers Ultimeter 2000 in data logging mode //
    ///////////////////////////////////////////////////////
    case (APRS_WX3):
      if (debug_level & 1)
      {
        fprintf(stderr,"APRS WX3 Peet Bros U-2k (data logging mode) %s:<%s>\n",fill->call_sign,data+2);
      }

      weather->wx_type=WX_TYPE;
      xastir_snprintf(weather->wx_station,
                      sizeof(weather->wx_station),
                      "U2k");

      /* get last gust speed */
      if (strlen(weather->wx_gust) > 0 && !from)      // From local station
      {
        /* get last speed */
        last_speed=(float)atof(weather->wx_gust);
        last_speed_time=weather->wx_speed_sec_time;
      }

      /* wind speed */
      if (data[2]!='-')   // '-' signifies invalid data
      {
        substr(temp_data1,(char *)(data+2),4);
        xastir_snprintf(weather->wx_speed,
                        sizeof(weather->wx_speed),
                        "%03.0f",
                        0.5 + ((strtol(temp_data1,&temp_conv,16) /10.0)*0.62137));
        if (from)   // From remote station
        {
          weather->wx_speed_sec_time = sec_now();
        }
        else
        {
          /* local station */
          computed_gust = compute_gust((float)atof(weather->wx_speed),
                                       last_speed,
                                       &last_speed_time);
          weather->wx_speed_sec_time = sec_now();
          xastir_snprintf(weather->wx_gust,
                          sizeof(weather->wx_gust),
                          "%03d",
                          (int)(0.5 + computed_gust));
        }
      }
      else
      {
        if (!from)  // From local station
        {
          weather->wx_speed[0]=0;
        }
      }

      /* wind direction */
      //
      // Note that the first two digits here may be 00, or may
      // be FF if a direction calibration has been entered.
      // We should zero them.
      //
      if (data[6]!='-')   // '-' signifies invalid data
      {
        substr(temp_data1,(char *)(data+6),4);
        // Zero out the first two bytes
        temp_data1[0] = '0';
        temp_data1[1] = '0';
        xastir_snprintf(weather->wx_course,
                        sizeof(weather->wx_course),
                        "%03.0f",
                        (strtol(temp_data1,&temp_conv,16)/256.0)*360.0);

        // Check for course = 0.  Change to 360.
        if (strncmp(weather->wx_course,"000",3) == 0)
        {
          xastir_snprintf(weather->wx_course,
                          sizeof(weather->wx_course),
                          "360");
        }

      }
      else
      {
        xastir_snprintf(weather->wx_course,
                        sizeof(weather->wx_course),
                        "000");
        if (!from)  // From local station
        {
          weather->wx_course[0]=0;
        }
      }

      /* outdoor temp */
      if (data[10]!='-')   // '-' signifies invalid data
      {
        int temp4;

        substr(temp_data1,(char *)(data+10),4);
        temp4 = (int)strtol(temp_data1,&temp_conv,16);

        if (temp_data1[0] > '7')    // Negative value, convert
        {
          temp4 = (temp4 & (temp4-0x7FFF)) - 0x8000;
        }

        xastir_snprintf(weather->wx_temp,
                        sizeof(weather->wx_temp),
                        "%03d",
                        (int)((float)((temp4<<16)/65536)/10.0));
      }
      else
      {
        if (!from)  // From local station
        {
          weather->wx_temp[0]=0;
        }
      }

      /* rain total long term */
      if (data[14]!='-')   // '-' signifies invalid data
      {
        substr(temp_data1,(char *)(data+14),4);
        if (!from)    // From local station
        {
          switch (WX_rain_gauge_type)
          {
            case 1: // 0.1" rain gauge
              xastir_snprintf(weather->wx_rain_total,
                              sizeof(weather->wx_rain_total),
                              "%0.2f",
                              strtol(temp_data1,&temp_conv,16)*10.0);
              break;
            case 3: // 0.1mm rain gauge
              xastir_snprintf(weather->wx_rain_total,
                              sizeof(weather->wx_rain_total),
                              "%0.2f",
                              strtol(temp_data1,&temp_conv,16)/2.54);
              break;
            case 2: // 0.01" rain gauge
            case 0: // No conversion
            default:
              xastir_snprintf(weather->wx_rain_total,
                              sizeof(weather->wx_rain_total),
                              "%0.2f",
                              strtol(temp_data1,&temp_conv,16)*1.0);
              break;
          }
          /* local station */
          compute_rain((float)atof(weather->wx_rain_total));
          weather->wx_compute_rain_rates=1;
          /*last hour rain */
          xastir_snprintf(weather->wx_rain,
                          sizeof(weather->wx_rain),
                          "%0.2f",
                          rain_minute_total);
          /*last 24 hour rain */
          xastir_snprintf(weather->wx_prec_24,
                          sizeof(weather->wx_prec_24),
                          "%0.2f",
                          rain_24);
          /* rain since midnight */
          xastir_snprintf(weather->wx_prec_00,
                          sizeof(weather->wx_prec_00),
                          "%0.2f",
                          rain_00);
        }
      }
      else
      {
        if (!from)  // From local station
        {
          weather->wx_rain_total[0]=0;
        }
      }

      /* baro */
      if (data[18]!='-')   // '-' signifies invalid data
      {
        substr(temp_data1,(char *)(data+18),4);
        xastir_snprintf(weather->wx_baro,
                        sizeof(weather->wx_baro),
                        "%0.1f",
                        strtol(temp_data1,&temp_conv,16)/10.0);
      }

      /* outdoor humidity */
      if (data[26]!='-')   // '-' signifies invalid data
      {
        substr(temp_data1,(char *)(data+26),4);
        xastir_snprintf(weather->wx_hum,
                        sizeof(weather->wx_hum),
                        "%03.0f",
                        strtol(temp_data1,&temp_conv,16)/10.0);
      }
      else
      {
        if (!from)  // From local station
        {
          weather->wx_hum[0]=0;
        }
      }

      // Isn't this replaced by the above switch-case?
      // No, I don't think so.  We can get these packets over
      // RF as well.
      /* todays rain total */
      if (strlen((const char *)data) > 45)
      {
        if (data[42]!='-')   // '-' signifies invalid data
        {
          if (from)   // From remote station
          {
            substr(temp_data1,(char *)(data+42),4);
            xastir_snprintf(weather->wx_prec_00,
                            sizeof(weather->wx_prec_00),
                            "%0.2f",
                            strtol(temp_data1,&temp_conv,16)/100.0);
          }
        }
        else
        {
          if (!from)  // From local station
          {
            weather->wx_prec_00[0]=0;
          }
        }
      }
      break;



    /////////////////////////////////////////////////
    // Peet Brothers Ultimeter 2000 in packet mode //
    /////////////////////////////////////////////////
    case(APRS_WX5):
      if (debug_level & 1)
      {
        fprintf(stderr,"APRS WX5 Peet Bros U-2k Packet (Packet mode) %s:<%s>\n",fill->call_sign,data);
      }

      weather->wx_type=WX_TYPE;
      xastir_snprintf(weather->wx_station,
                      sizeof(weather->wx_station),
                      "U2k");

      /* get last gust speed */
      if (strlen(weather->wx_gust) > 0 && !from)      // From local station
      {
        /* get last speed */
        last_speed=(float)atof(weather->wx_gust);
        last_speed_time=weather->wx_speed_sec_time;
      }

      /* wind speed peak over last 5 min */
      if (data[5]!='-')   // '-' signifies invalid data
      {
        substr(temp_data1,(char *)(data+5),4);
        if (from)   // From remote station
        {
          xastir_snprintf(weather->wx_gust,
                          sizeof(weather->wx_gust),
                          "%03.0f",
                          0.5 + (strtol(temp_data1,&temp_conv,16)/10.0)*0.62137);
          /* this may be the only wind data */
          xastir_snprintf(weather->wx_speed,
                          sizeof(weather->wx_speed),
                          "%03.0f",
                          0.5 + (strtol(temp_data1,&temp_conv,16)/10.0)*0.62137);
        }
        else
        {
          /* local station and may be the only wind data */
          if (len<56)
          {
            xastir_snprintf(weather->wx_speed,
                            sizeof(weather->wx_speed),
                            "%03.0f",
                            0.5 + ( strtol(temp_data1,&temp_conv,16)/10.0)*0.62137);
            computed_gust = compute_gust((float)atof(weather->wx_speed),
                                         last_speed,
                                         &last_speed_time);
            weather->wx_speed_sec_time = sec_now();
            xastir_snprintf(weather->wx_gust,
                            sizeof(weather->wx_gust),
                            "%03d",
                            (int)(0.5 + computed_gust));
          }
        }
      }
      else
      {
        if (!from)  // From local station
        {
          weather->wx_gust[0]=0;
        }
      }

      /* wind direction */
      //
      // Note that the first two digits here may be 00, or may
      // be FF if a direction calibration has been entered.
      // We should zero them.
      //
      if (data[9]!='-')   // '-' signifies invalid data
      {
        substr(temp_data1,(char *)(data+9),4);
        temp_data1[0] = '0';
        temp_data1[1] = '0';
        xastir_snprintf(weather->wx_course,
                        sizeof(weather->wx_course),
                        "%03.0f",
                        (strtol(temp_data1,&temp_conv,16)/256.0)*360.0);

        // Check for course = 0.  Change to 360.
        if (strncmp(weather->wx_course,"000",3) == 0)
        {
          xastir_snprintf(weather->wx_course,
                          sizeof(weather->wx_course),
                          "360");
        }

      }
      else
      {
        xastir_snprintf(weather->wx_course,
                        sizeof(weather->wx_course),
                        "000");
        if (!from)  // From local station
        {
          weather->wx_course[0]=0;
        }
      }

      /* outdoor temp */
      if (data[13]!='-')   // '-' signifies invalid data
      {
        int temp4;

        substr(temp_data1,(char *)(data+13),4);
        temp4 = (int)strtol(temp_data1,&temp_conv,16);

        if (temp_data1[0] > '7')    // Negative value, convert
        {
          temp4 = (temp4 & (temp4-0x7FFF)) - 0x8000;
        }

        xastir_snprintf(weather->wx_temp,
                        sizeof(weather->wx_temp),
                        "%03d",
                        (int)((float)((temp4<<16)/65536)/10.0));
      }
      else
      {
        if (!from)  // From local station
        {
          weather->wx_temp[0]=0;
        }
      }
      /* todays rain total (on some units) */
      if (data[49]!='-')   // '-' signifies invalid data
      {
        if (from)   // From remote station
        {
          substr(temp_data1,(char *)(data+49),4);
          xastir_snprintf(weather->wx_prec_00,
                          sizeof(weather->wx_prec_00),
                          "%0.2f",
                          strtol(temp_data1,&temp_conv,16)/100.0);
        }
      }
      else
      {
        if (!from)  // From local station
        {
          weather->wx_prec_00[0]=0;
        }
      }

      /* rain total long term */
      if (data[17]!='-')   // '-' signifies invalid data
      {
        substr(temp_data1,(char *)(data+17),4);
        if (!from)      // From local station
        {
          switch (WX_rain_gauge_type)
          {
            case 1: // 0.1" rain gauge
              xastir_snprintf(weather->wx_rain_total,
                              sizeof(weather->wx_rain_total),
                              "%0.2f",
                              strtol(temp_data1,&temp_conv,16)*10.0);
              break;
            case 3: // 0.1mm rain gauge
              xastir_snprintf(weather->wx_rain_total,
                              sizeof(weather->wx_rain_total),
                              "%0.2f",
                              strtol(temp_data1,&temp_conv,16)/2.54);
              break;
            case 2: // 0.01" rain gauge
            case 0: // No conversion
            default:
              xastir_snprintf(weather->wx_rain_total,
                              sizeof(weather->wx_rain_total),
                              "%0.2f",
                              strtol(temp_data1,&temp_conv,16)*1.0);
              break;
          }
          /* local station */
          compute_rain((float)atof(weather->wx_rain_total));
          weather->wx_compute_rain_rates=1;
          /*last hour rain */
          xastir_snprintf(weather->wx_rain,
                          sizeof(weather->wx_rain),
                          "%0.2f",
                          rain_minute_total);
          /*last 24 hour rain */
          xastir_snprintf(weather->wx_prec_24,
                          sizeof(weather->wx_prec_24),
                          "%0.2f",
                          rain_24);
          /* rain since midnight */
          xastir_snprintf(weather->wx_prec_00,
                          sizeof(weather->wx_prec_00),
                          "%0.2f",
                          rain_00);
        }
      }
      else
      {
        if (!from)  // From local station
        {
          weather->wx_rain_total[0]=0;
        }
      }

      /* baro */
      if (data[21]!='-')   // '-' signifies invalid data
      {
        substr(temp_data1,(char *)(data+21),4);
        xastir_snprintf(weather->wx_baro,
                        sizeof(weather->wx_baro),
                        "%0.1f",
                        strtol(temp_data1, &temp_conv, 16)/10.0);
      }
      else
      {
        if (!from)  // From local station
        {
          weather->wx_baro[0]=0;
        }
      }

      /* outdoor humidity */
      if (data[37]!='-')   // '-' signifies invalid data
      {
        substr(temp_data1,(char *)(data+37),4);
        xastir_snprintf(weather->wx_hum,
                        sizeof(weather->wx_hum),
                        "%03.0f",
                        strtol(temp_data1,&temp_conv,16)/10.0);
      }
      else
      {
        if (!from)  // From local station
        {
          weather->wx_hum[0]=0;
        }
      }

      /* 1 min wind speed avg */
      if (len>53 && (data[53]) != '-')   // '-' signifies invalid data
      {
        substr(temp_data1,(char *)(data+53),4);
        xastir_snprintf(weather->wx_speed,
                        sizeof(weather->wx_speed),
                        "%03.0f",
                        0.5 + (strtol(temp_data1,&temp_conv,16)/10.0)*0.62137);
        if (from)   // From remote station
        {
          weather->wx_speed_sec_time = sec_now();
        }
        else
        {
          /* local station */
          computed_gust = compute_gust((float)atof(weather->wx_speed),
                                       last_speed,
                                       &last_speed_time);
          weather->wx_speed_sec_time = sec_now();
          xastir_snprintf(weather->wx_gust,
                          sizeof(weather->wx_gust),
                          "%03d",
                          (int)(0.5 + computed_gust));
        }
      }
      else
      {
        if (!from)      // From local station
        {
          if (len>53)
          {
            weather->wx_speed[0]=0;
          }
        }
      }
      break;



    //////////////////////////////////////////////////////////
    // Peet Brothers Ultimeter 2000 in complete record mode //
    //////////////////////////////////////////////////////////
    //
    // In this mode most fields are 4-bytes two's complement.  A
    // few fields are 2-bytes wide.
    //
    // <http://www.peetbros.com/HTML_Pages/faqs.htm>
    //
    case(PEET_COMPLETE):
      if (debug_level & 1)
      {
        fprintf(stderr,"Peet Bros U-2k Packet (Complete Record Mode) %s:<%s>\n",fill->call_sign,data);
      }

      if (!from)      // From local station
      {
        int done_with_wx_speed = 0;


        /* decode only for local station */
        weather->wx_type=WX_TYPE;
        xastir_snprintf(weather->wx_station,
                        sizeof(weather->wx_station),
                        "U2k");


        if (data[12]!='-')   // '-' signifies invalid data
        {
          substr(temp_data1,(char *)(data+12),4);
          xastir_snprintf(weather->wx_gust,
                          sizeof(weather->wx_gust),
                          "%03.0f",
                          0.5 + (strtol(temp_data1,&temp_conv,16)/10.0)*0.62137);
        }
        else
        {
          weather->wx_gust[0]=0;
        }


        // Check whether field 115 is available at bytes 452
        // through 455.  If so, that's the one-minute wind
        // speed average in 0.1kph, which matches the APRS
        // spec except for the units (which should be MPH).
        if ( (len >= 456) && (data[452] != '-') )   // '-' signifies invalid data
        {
          substr(temp_data1,(char *)(data+452),4);
          xastir_snprintf(weather->wx_speed,
                          sizeof(weather->wx_speed),
                          "%03.0f",
                          0.5 + (strtol(temp_data1,&temp_conv,16)/10.0)*0.62137);
          done_with_wx_speed++;
        }


        // Some Peet units don't have that particular wind
        // speed field, so snag what wind speed we can from
        // the other wind speed fields, which don't quite
        // match the APRS spec as they're instantaneous
        // values, not one-minute sustained speeds.


        // KG9AE
        // Peet Bros CR mode wind values should be selected based on which are highest.
        /* Wind Speed fields 1, 34, and 71.  Wind direction fields 2, 35, 72. */
        if (data[4] !='-')   // '-' signifies invalid data
        {
          substr(temp_data1, (char *)data+4, 4);
          temp1 = 0.5 + (strtol(temp_data1,&temp_conv,16)/10.0)*0.62137;
        }
        else
        {
          temp1=0;
        }
        if (data[136] !='-')   // '-' signifies invalid data
        {
          substr(temp_data1, (char *)data+136, 4);
          temp2 = 0.5 + (strtol(temp_data1,&temp_conv,16)/10.0)*0.62137;
        }
        else
        {
          temp2=0;
        }
        if (data[284] !='-')   // '-' signifies invalid data
        {
          substr(temp_data1, (char *)data+284, 4);
          temp3 = 0.5 + (strtol(temp_data1,&temp_conv,16)/10.0)*0.62137;
        }
        else
        {
          temp3=0;
        }

        // fprintf(stderr,"WIND: wind1 %d, wind2 %d, wind3 %d\n", temp1, temp2, temp3);

        // Select wind speed and direction based on which
        // wind speed is the highest.  Ugh, surely there's a
        // way to make this pretty. A function might be
        // better.
        if ( temp1 >= temp2 && temp1 >= temp3 )
        {
          // fprintf(stderr,"WIND:      ***\n");

          /* wind speed */
          if (!done_with_wx_speed)
          {
            substr(temp_data1,(char *)(data+4),4);
            xastir_snprintf(weather->wx_speed,
                            sizeof(weather->wx_speed),
                            "%03.0f",
                            0.5 + (strtol(temp_data1,&temp_conv,16)/10.0)*0.62137);
          }

          /* wind direction */
          //
          // Note that the first two digits here may be
          // 00, or may be FF if a direction calibration
          // has been entered.  We should zero them.
          //
          if (data[8]!='-')   // '-' signifies invalid data
          {
            substr(temp_data1,(char *)(data+8),4);
            temp_data1[0] = '0';
            temp_data1[1] = '0';
            xastir_snprintf(weather->wx_course,
                            sizeof(weather->wx_course),
                            "%03.0f",
                            (strtol(temp_data1,&temp_conv,16)/256.0)*360.0);

            // Check for course = 0.  Change to 360.
            if (strncmp(weather->wx_course,"000",3) == 0)
            {
              xastir_snprintf(weather->wx_course,
                              sizeof(weather->wx_course),
                              "360");
            }

          }
          else
          {
            xastir_snprintf(weather->wx_course,
                            sizeof(weather->wx_course),
                            "000");
            weather->wx_course[0]=0;
          }
        }
        else if ( temp2 >= temp1 && temp2 >= temp3 )
        {
          // fprintf(stderr,"WIND:               ***\n");

          if (!done_with_wx_speed)
          {
            /* wind speed */
            substr(temp_data1,(char *)(data+136),4);
            xastir_snprintf(weather->wx_speed,
                            sizeof(weather->wx_speed),
                            "%03.0f",
                            0.5 + (strtol(temp_data1,&temp_conv,16)/10.0)*0.62137);
          }

          /* wind direction */
          //
          // Note that the first two digits here may be
          // 00, or may be FF if a direction calibration
          // has been entered.  We should zero them.
          //
          if (data[140]!='-')   // '-' signifies invalid data
          {
            substr(temp_data1,(char *)(data+140),4);
            temp_data1[0] = '0';
            temp_data1[1] = '0';
            xastir_snprintf(weather->wx_course,
                            sizeof(weather->wx_course),
                            "%03.0f",
                            (strtol(temp_data1,&temp_conv,16)/256.0)*360.0);

            // Check for course = 0.  Change to 360.
            if (strncmp(weather->wx_course,"000",3) == 0)
            {
              xastir_snprintf(weather->wx_course,
                              sizeof(weather->wx_course),
                              "360");
            }

          }
          else
          {
            xastir_snprintf(weather->wx_course,
                            sizeof(weather->wx_course),
                            "000");
            weather->wx_course[0]=0;
          }
        }
        else if ( temp3 >= temp2 && temp3 >= temp1 )
        {
          // fprintf(stderr,"WIND:                        ***\n");

          if (!done_with_wx_speed)
          {
            /* wind speed */
            substr(temp_data1,(char *)(data+284),4);
            xastir_snprintf(weather->wx_speed,
                            sizeof(weather->wx_speed),
                            "%03.0f",
                            0.5 + (strtol(temp_data1,&temp_conv,16)/10.0)*0.62137);
          }

          /* wind direction */
          //
          // Note that the first two digits here may be
          // 00, or may be FF if a direction calibration
          // has been entered.  We should zero them.
          //
          if (data[288]!='-')   // '-' signifies invalid data
          {
            substr(temp_data1,(char *)(data+288),4);
            temp_data1[0] = '0';
            temp_data1[1] = '0';
            xastir_snprintf(weather->wx_course,
                            sizeof(weather->wx_course),
                            "%03.0f",
                            (strtol(temp_data1,&temp_conv,16)/256.0)*360.0);

            // Check for course = 0.  Change to 360.
            if (strncmp(weather->wx_course,"000",3) == 0)
            {
              xastir_snprintf(weather->wx_course,
                              sizeof(weather->wx_course),
                              "360");
            }

          }
          else
          {
            xastir_snprintf(weather->wx_course,
                            sizeof(weather->wx_course),
                            "000");
            weather->wx_course[0]=0;
          }
        }
        else    /* Or default to the first value */
        {
          // fprintf(stderr,"WIND: DEFAULTING!\n");

          if (!done_with_wx_speed)
          {
            /* wind speed */
            substr(temp_data1,(char *)(data+4),4);
            xastir_snprintf(weather->wx_speed,
                            sizeof(weather->wx_speed),
                            "%03.0f",
                            0.5 + (strtol(temp_data1,&temp_conv,16)/10.0)*0.62137);
          }

          /* wind direction */
          //
          // Note that the first two digits here may be
          // 00, or may be FF if a direction calibration
          // has been entered.  We should zero them.
          //
          if (data[8]!='-')   // '-' signifies invalid data
          {
            substr(temp_data1,(char *)(data+8),4);
            temp_data1[0] = '0';
            temp_data1[1] = '0';
            xastir_snprintf(weather->wx_course,
                            sizeof(weather->wx_course),
                            "%03.0f",
                            (strtol(temp_data1,&temp_conv,16)/256.0)*360.0);

            // Check for course = 0.  Change to 360.
            if (strncmp(weather->wx_course,"000",3) == 0)
            {
              xastir_snprintf(weather->wx_course,
                              sizeof(weather->wx_course),
                              "360");
            }

          }
          else
          {
            xastir_snprintf(weather->wx_course,
                            sizeof(weather->wx_course),
                            "000");
            weather->wx_course[0]=0;
          }
        }


        /* outdoor temp */
        if (data[24]!='-')   // '-' signifies invalid data
        {
          int temp4;

          substr(temp_data1,(char *)(data+24),4);
          temp4 = (int)strtol(temp_data1,&temp_conv,16);

          if (temp_data1[0] > '7')    // Negative value, convert
          {
            temp4 = (temp4 & (temp4-0x7FFF)) - 0x8000;
          }

          xastir_snprintf(weather->wx_temp,
                          sizeof(weather->wx_temp),
                          "%03d",
                          (int)((float)((temp4<<16)/65536)/10.0));
        }
        else
        {
          weather->wx_temp[0]=0;
        }

// We don't want to parse this here because compute_rain()
// calculates this for us from the accumulating long-term rain
// total.  If we were to do it here as well, we'll get conflicting
// results.  Since only some units put out today's rain total, we'll
// just rely on our own calculations for it instead.  It'll work
// across more units.
        /*
                        // todays rain total (on some units)
                        if (data[28]!='-') { // '-' signifies invalid data
                            substr(temp_data1,(char *)(data+28),4);
                            switch (WX_rain_gauge_type) {
                                case 1: // 0.1" rain gauge
                                    xastir_snprintf(weather->wx_prec_00,
                                        sizeof(weather->wx_prec_00),
                                        "%0.2f",
                                        (float)strtol(temp_data1,&temp_conv,16)/10.0);
                                    break;
                                case 3: // 0.1mm rain gauge
                                    xastir_snprintf(weather->wx_prec_00,
                                        sizeof(weather->wx_prec_00),
                                        "%0.2f",
                                        (float)strtol(temp_data1,&temp_conv,16)/254.0);
                                    break;
                                case 2: // 0.01" rain gauge
                                case 0: // No conversion
                                default:
                                    xastir_snprintf(weather->wx_prec_00,
                                        sizeof(weather->wx_prec_00),
                                        "%0.2f",
                                        (float)strtol(temp_data1,&temp_conv,16)/100.0);
                                    break;
                            }
                        } else
                            weather->wx_prec_00[0]=0;
        */

        /* rain total long term */
        if ((char)data[432]!='-')   // '-' signifies invalid data
        {
          substr(temp_data1,(char *)(data+432),4);
          switch (WX_rain_gauge_type)
          {
            case 1: // 0.1" rain gauge
              xastir_snprintf(weather->wx_rain_total,
                              sizeof(weather->wx_rain_total),
                              "%0.2f",
                              strtol(temp_data1,&temp_conv,16)*10.0);
              break;
            case 3: // 0.1mm rain gauge
              xastir_snprintf(weather->wx_rain_total,
                              sizeof(weather->wx_rain_total),
                              "%0.2f",
                              strtol(temp_data1,&temp_conv,16)/2.54);
              break;
            case 2: // 0.01" rain gauge
            case 0: // No conversion
            default:
              xastir_snprintf(weather->wx_rain_total,
                              sizeof(weather->wx_rain_total),
                              "%0.2f",
                              strtol(temp_data1,&temp_conv,16)*1.0);
              break;
          }
          /* Since local station only */
          compute_rain((float)atof(weather->wx_rain_total));
          weather->wx_compute_rain_rates=1;

          /*last hour rain */
          xastir_snprintf(weather->wx_rain,
                          sizeof(weather->wx_rain),
                          "%0.2f",
                          rain_minute_total);

          /*last 24 hour rain */
          xastir_snprintf(weather->wx_prec_24,
                          sizeof(weather->wx_prec_24),
                          "%0.2f",
                          rain_24);

          /* rain since midnight */
          xastir_snprintf(weather->wx_prec_00,
                          sizeof(weather->wx_prec_00),
                          "%0.2f",
                          rain_00);
        }
        else
        {
          weather->wx_rain_total[0]=0;
        }

        /* baro */
        if (data[32]!='-')   // '-' signifies invalid data
        {
          substr(temp_data1,(char *)(data+32),4);
          xastir_snprintf(weather->wx_baro,
                          sizeof(weather->wx_baro),
                          "%0.1f",
                          strtol(temp_data1,&temp_conv,16)/10.0);
        }
        else
        {
          weather->wx_baro[0]=0;
        }

        /* outdoor humidity */
        if (data[52]!='-')   // '-' signifies invalid data
        {
          substr(temp_data1,(char *)(data+52),4);
          xastir_snprintf(weather->wx_hum,
                          sizeof(weather->wx_hum),
                          "%03.0f",
                          strtol(temp_data1,&temp_conv,16)/10.0);
        }
        else
        {
          weather->wx_hum[0]=0;
        }

        /* dew point */
        if (data[60]!='-')   // '-' signifies invalid data
        {
          int temp4;

          substr(temp_data1,(char *)(data+60),4);
          temp4 = (int)strtol(temp_data1,&temp_conv,16);

          if (temp_data1[0] > '7')    // Negative value, convert
          {
            temp4 = (temp4 & (temp4-0x7FFF)) - 0x8000;
          }

          xastir_snprintf(wx_dew_point,
                          sizeof(wx_dew_point),
                          "%03d",
                          (int)((float)((temp4<<16)/65536)/10.0));
          wx_dew_point_on = 1;
        }

        /*high winds for today*/
        if (data[248]!='-')   // '-' signifies invalid data
        {
          substr(temp_data1,(char *)(data+248),4);
          xastir_snprintf(wx_high_wind,
                          sizeof(wx_high_wind),
                          "%03.0f",
                          0.5 + (strtol(temp_data1,&temp_conv,16)/10.0)*0.62137);
          wx_high_wind_on = 1;
        }

        /*wind chill */
        if (data[20]!='-')   // '-' signifies invalid data
        {
          int temp4;

          substr(temp_data1,(char *)(data+20),4);
          temp4 = (int)strtol(temp_data1,&temp_conv,16);

          if (temp_data1[0] > '7')    // Negative value, convert
          {
            temp4 = (temp4 & (temp4-0x7FFF)) - 0x8000;
          }

          xastir_snprintf(wx_wind_chill,
                          sizeof(wx_wind_chill),
                          "%03d",
                          (int)((float)((temp4<<16)/65536)/10.0));
          wx_wind_chill_on = 1;
        }

        /*3-Hr Barometric Change */
        if (data[36]!='-')   // '-' signifies invalid data
        {
          int temp4;

          substr(temp_data1,(char *)(data+36),4);
          temp4 = (int)strtol(temp_data1,&temp_conv,16);

          if (temp_data1[0] > '7')    // Negative value, convert
          {
            temp4 = (temp4 & (temp4-0x7FFF)) - 0x8000;
          }

          xastir_snprintf(wx_three_hour_baro,
                          sizeof(wx_three_hour_baro),
                          "%0.2f",
// Old code
//                  (float)((strtol(temp_data1,&temp_conv,16)<<16)/65536)/100.0/3.38639);
// New code, fix by Matt Werner, kb0kqa:
                          (float)((temp4<<16)/65536)/10.0);

          wx_three_hour_baro_on = 1;
        }

        /* High Temp for Today*/
        if (data[276]!='-')   // '-' signifies invalid data
        {
          int temp4;

          substr(temp_data1,(char *)(data+276),4);
          temp4 = (int)strtol(temp_data1,&temp_conv,16);

          if (temp_data1[0] > '7')    // Negative value, convert
          {
            temp4 = (temp4 & (temp4-0x7FFF)) - 0x8000;
          }

          xastir_snprintf(wx_hi_temp,
                          sizeof(wx_hi_temp),
                          "%03d",
                          (int)((float)((temp4<<16)/65536)/10.0));
          wx_hi_temp_on = 1;
        }
        else
        {
          wx_hi_temp_on = 0;
        }

        /* Low Temp for Today*/
        if (data[100]!='-')   // '-' signifies invalid data
        {
          int temp4;

          substr(temp_data1,(char *)(data+100),4);
          temp4 = (int)strtol(temp_data1,&temp_conv,16);

          if (temp_data1[0] > '7')    // Negative value, convert
          {
            temp4 = (temp4 & (temp4-0x7FFF)) - 0x8000;
          }

          xastir_snprintf(wx_low_temp,
                          sizeof(wx_low_temp),
                          "%03d",
                          (int)((float)((temp4<<16)/65536)/10.0));
          wx_low_temp_on = 1;
        }
        else
        {
          wx_low_temp_on = 0;
        }

        /* Heat Index Calculation*/
        hi_hum=atoi(weather->wx_hum);
        rh2= atoi(weather->wx_hum);
        rh2=(rh2 * rh2);
        hidx_temp=atoi(weather->wx_temp);
        t2= atoi(weather->wx_temp);
        t2=(t2 * t2);

        if (hidx_temp >= 70)
        {
          heat_index=-42.379+2.04901523 * hidx_temp+10.1433127 * hi_hum-0.22475541
                     * hidx_temp * hi_hum-0.00683783 * t2-0.05481717 * rh2+0.00122874
                     * t2 * hi_hum+0.00085282 * hidx_temp * rh2-0.00000199 * t2 * rh2;
          xastir_snprintf (wx_heat_index,
                           sizeof(wx_heat_index),
                           "%03d",
                           heat_index);
          wx_heat_index_on = 1;
        }
        else
        {
          wx_heat_index_on = 0;
        }
      }
      break;



    ////////////////////////
    // Qualimetrics Q-Net //
    ////////////////////////
    case(QM_WX):
      if (debug_level & 1)
      {
        fprintf(stderr,"Qualimetrics Q-Net %s:<%s>\n",fill->call_sign,data);
      }

      weather->wx_type=WX_TYPE;
      xastir_snprintf(weather->wx_station,
                      sizeof(weather->wx_station),
                      "Q-N");

      // Can this sscanf overflow the "temp" buffer?  I
      // changed the length of temp to MAX_DEVICE_BUFFER to
      // avoid this problem.
      if (6 != sscanf((char *)data,"%19s %d %19s %d %19s %d",temp,&temp1,temp,&temp2,temp,&temp3))
      {
        fprintf(stderr,"wx_fill_data:sscanf parsing error\n");
      }

      /* outdoor temp */
      xastir_snprintf(weather->wx_temp,
                      sizeof(weather->wx_temp),
                      "%03d",
                      (int)((temp2/10.0)));

      /* baro */
      xastir_snprintf(weather->wx_baro,
                      sizeof(weather->wx_baro),
                      "%0.1f",
                      ((float)temp3/100.0)*33.864);

      /* outdoor humidity */
      xastir_snprintf(weather->wx_hum,
                      sizeof(weather->wx_hum),
                      "%03d",
                      temp1);

      if (!from)      // From local station
      {
        weather->wx_gust[0]=0;
        weather->wx_course[0]=0;
        weather->wx_rain[0]=0;
        weather->wx_prec_00[0]=0;
        weather->wx_prec_24[0]=0;
        weather->wx_rain_total[0]=0;
        weather->wx_gust[0]=0;
        weather->wx_speed[0]=0;
      }
      break;



    ///////////////////////////////////////////////////////////
    //  Radio Shack WX-200 or Huger/Oregon Scientific WM-918 //
    ///////////////////////////////////////////////////////////
    case(RSWX200):

      // Notes:  Many people run the wx200d daemon connected to the weather station,
      // with Xastir then connected to wx200d.  Note that wx200d changes the protocol
      // slightly:  It only sends frames that have changed to the clients.  This means
      // even if the weather station is sending regular packets, wx200d won't send
      // them along to Xastir if all the bits are the same as the last packet of that
      // type.  To fix this I had to tie into the main.c:UpdateTime() function to do
      // regular updates at a 30 second rate, to keep the rain and gust queues cycling
      // on a regular basis.
      //
      // 2nd Note:  Some WX-200 weather stations send bogus data.  I had to add in a
      // bunch of filtering to keep the global variables from getting corrupted by
      // this data.  More filtering may need to be done and/or the limits may need to
      // be changed.

      if (!from)      // From local station
      {
        if (debug_level & 1)
        {
          fprintf(stderr,"RSWX200 WX (binary)\n");
        }

        weather->wx_type=WX_TYPE;
        xastir_snprintf(weather->wx_station,
                        sizeof(weather->wx_station),
                        "RSW");

        switch (data[0])
        {
          case 0x8f: /* humidity */
            if ( (rswnc(data[20]) <= 100) && (rswnc(data[2]) >= 0) )
              xastir_snprintf(weather->wx_hum,
                              sizeof(weather->wx_hum),
                              "%03d",
                              rswnc(data[20]));
            else
              //sprintf(weather->wx_hum,"100");
            {
              fprintf(stderr,"Humidity out-of-range, ignoring: %03d\n",rswnc(data[20]) );
            }
            break;

          case 0x9f: /* temp */
            /* all data in C ?*/
            xastir_snprintf(temp_data1,
                            sizeof(temp_data1),
                            "%c%d%0.1f",
                            ((data[17]&0x08) ? '-' : '+'),(data[17]&0x7),rswnc(data[16])/10.0);
            /*fprintf(stderr,"temp data: <%s> %d %d %d\n", temp_data1,((data[17]&0x08)==0x08),(data[17]&0x7),rswnc(data[16]));*/
            temp_temp = (float)((atof(temp_data1)*1.8)+32);
            if ( (temp_temp >= -99.0) && (temp_temp < 200.0) )
            {
              xastir_snprintf(weather->wx_temp,
                              sizeof(weather->wx_temp),
                              "%03d",
                              (int)((atof(temp_data1)*1.8)+32));
              /*fprintf(stderr,"Temp %s C %0.2f %03d\n",temp_data1,atof(temp_data1),(int)atof(temp_data1));
              fprintf(stderr,"Temp F %0.2f %03d\n",(atof(temp_data1)*1.8)+32,(int)(atof(temp_data1)*1.8)+32);
              */
            }
            else      // We don't want to save this one
            {
              fprintf(stderr,"Temp out-of-range, ignoring: %0.2f\n", temp_temp);
            }
            xastir_snprintf(temp_data1,
                            sizeof(temp_data1),
                            "%c%d%d.%d",
                            ((data[18]&0x80) ? '-' : '+'),(data[18]&0x70)>>4,(data[18]&0x0f),(data[17] & 0xf0) >> 4);
            xastir_snprintf(wx_hi_temp,
                            sizeof(wx_hi_temp),
                            "%03d",
                            (int)((atof(temp_data1)*1.8)+32));
            wx_hi_temp_on=1;
            xastir_snprintf(temp_data1,
                            sizeof(temp_data1),
                            "%c%d%d.%d",
                            ((data[23]&0x80) ? '-' : '+'),(data[23]&0x70)>>4,(data[23]&0x0f),(data[22] & 0xf0) >> 4);
            xastir_snprintf(wx_low_temp,
                            sizeof(wx_low_temp),
                            "%03d",
                            (int)((atof(temp_data1)*1.8)+32));
            wx_low_temp_on=1;
            break;

          case 0xaf: /* baro/dewpt */
            // local baro pressure in mb?
            // sprintf(weather->wx_baro,"%02d%02d",rswnc(data[2]),rswnc(data[1]));
            // Sea Level Adjusted baro in mb
            xastir_snprintf(weather->wx_baro,
                            sizeof(weather->wx_baro),
                            "%0d%02d%0.1f",
                            (data[5]&0x0f),
                            rswnc(data[4]),
                            rswnc(data[3])/10.0);

            /* dew point in C */
            temp_temp = (int)((rswnc(data[18])*1.8)+32);
            if ( (temp_temp >= 32.0) && (temp_temp < 150.0) )
              xastir_snprintf(wx_dew_point,
                              sizeof(wx_dew_point),
                              "%03d",
                              (int)((rswnc(data[18])*1.8)+32));
            else
            {
              fprintf(stderr,"Dew point out-of-range, ignoring: %0.2f\n", temp_temp);
            }
            break;

          case 0xbf: /* Rain */
            // All data in mm.  Convert to hundredths of an inch.
            xastir_snprintf(temp_data1,
                            sizeof(temp_data1),
                            "%02d%02d",
                            rswnc(data[6]),
                            rswnc(data[5]));

            temp_temp = (float)(atof(temp_data1) * 3.9370079);

            if ( (temp_temp >= 0) && (temp_temp < 51200.0) )   // Between 0 and 512 inches
            {
              xastir_snprintf(weather->wx_rain_total,
                              sizeof(weather->wx_rain_total),
                              "%0.2f",
                              atof(temp_data1) * 3.9370079);

              /* Since local station only */
              compute_rain((float)atof(weather->wx_rain_total));
              weather->wx_compute_rain_rates=1;

              /* Last hour rain */
              xastir_snprintf(weather->wx_rain,
                              sizeof(weather->wx_rain),
                              "%0.2f",
                              rain_minute_total);

              /* Last 24 hour rain */
              xastir_snprintf(weather->wx_prec_24,
                              sizeof(weather->wx_prec_24),
                              "%0.2f",
                              rain_24);

              /* Rain since midnight */
              xastir_snprintf(weather->wx_prec_00,
                              sizeof(weather->wx_prec_00),
                              "%0.2f",
                              rain_00);
            }
            else
            {
              fprintf(stderr,"Total Rain out-of-range, ignoring: %0.2f\n", temp_temp);
            }
            break;

          case 0xcf: /* Wind w/chill*/
            /* get last gust speed */
            if (strlen(weather->wx_gust) > 0)
            {
              /* get last speed */
              last_speed=(float)atof(weather->wx_gust);
              last_speed_time=weather->wx_speed_sec_time;
            }
            /* all data in m/s */
            /* average wind speed */
            xastir_snprintf(temp_data1,
                            sizeof(temp_data1),
                            "%01d%0.1f",
                            (data[5]&0xf),
                            (float)( rswnc(data[4]) / 10 ));
            // Convert to mph
            xastir_snprintf(weather->wx_speed,
                            sizeof(weather->wx_speed),
                            "%03d",
                            (int)(0.5 + (atof(temp_data1)*2.2369)));

            /* wind gust */
            xastir_snprintf(temp_data1,
                            sizeof(temp_data1),
                            "%01d%0.1f",
                            (data[2]&0xf),
                            (float)( rswnc(data[1]) / 10 ));
            /*sprintf(weather->wx_gust,"%03d",(int)(0.5 + (atof(temp_data1)*2.2369)));*/

            /* do computed gust, convert to mph */
            computed_gust = compute_gust((int)(0.5 + (atof(temp_data1)*2.2369)),
                                         last_speed,
                                         &last_speed_time);
            weather->wx_speed_sec_time = sec_now();
            xastir_snprintf(weather->wx_gust,
                            sizeof(weather->wx_gust),
                            "%03d",
                            (int)(0.5 + computed_gust));

            /* high wind gust */
            xastir_snprintf(temp_data1,
                            sizeof(temp_data1),
                            "%01d%0.1f",
                            (data[8]&0xf),
                            (float)( rswnc(data[7]) / 10 ));
            xastir_snprintf(wx_high_wind,
                            sizeof(wx_high_wind),
                            "%03d",
                            (int)(0.5 + (atof(temp_data1)*2.2369)));
            wx_high_wind_on = 1;

            xastir_snprintf(weather->wx_course,
                            sizeof(weather->wx_course),
                            "%03d",
                            ( ((rswnc(data[3])*10) + ((data[2]&0xf0)>>4)) %1000 ) );

            // Check for course = 0.  Change to 360.
            if (strncmp(weather->wx_course,"000",3) == 0)
            {
              xastir_snprintf(weather->wx_course,
                              sizeof(weather->wx_course),
                              "360");
            }

            /* wind chill in C */
            xastir_snprintf(temp_data1,
                            sizeof(temp_data1),
                            "%c%d",
                            ((data[21]&0x20) ? '-' : '+'),
                            rswnc(data[16]));

            temp_temp = (float)((atof(temp_data1)*1.8)+32);
            if ( (temp_temp > -200.0) && (temp_temp < 200.0) )
              xastir_snprintf(wx_wind_chill,
                              sizeof(wx_wind_chill),
                              "%03d",
                              (int)((atof(temp_data1)*1.8)+32));
            else
            {
              fprintf(stderr,"Wind_chill out-of-range, ignoring: %0.2f\n", temp_temp);
            }

            wx_wind_chill_on = 1;
            break;
          default:
            break;
        }

        if (strlen(weather->wx_hum) > 0 && strlen(weather->wx_temp) > 0)
        {
          /* Heat Index Calculation*/
          hi_hum=atoi(weather->wx_hum);
          rh2= atoi(weather->wx_hum);
          rh2=(rh2 * rh2);
          hidx_temp=atoi(weather->wx_temp);
          t2= atoi(weather->wx_temp);
          t2=(t2 * t2);

          if (hidx_temp >= 70)
          {
            heat_index=(-42.379+2.04901523 * hidx_temp+10.1433127 * hi_hum-0.22475541 * hidx_temp *
                        hi_hum-0.00683783 * t2-0.05481717 * rh2+0.00122874 * t2 * hi_hum+0.00085282 *
                        hidx_temp * rh2-0.00000199 * t2 * rh2);
            xastir_snprintf(wx_heat_index,
                            sizeof(wx_heat_index),
                            "%03d",
                            heat_index);

            wx_heat_index_on=1;
          }
          else
          {
            wx_heat_index[0] = 0;
          }
        }   // end of heat index calculation
      }   // end of if (!from)
      break;  // End of case for RSWX200 weather station


    ///////////////////////////////////////////////////////////
    //  Davis WMII/WWIII/Vantage Pro via meteo & db2APRS     //
    ///////////////////////////////////////////////////////////

    // Note: format is really APRS Spec 'positionless' WX string w/tag for X and Davis

    case(DAVISMETEO) :


      // todo - need to deal with lack of values, such as c...s...g...t045 string

      memset(weather->wx_course,0,4);    // Keep out fradulent data...
      memset(weather->wx_speed,0,4);
      memset(weather->wx_gust,0,4);
      memset(weather->wx_temp,0,5);
      memset(weather->wx_rain,0,10);
      memset(weather->wx_prec_00,0,10);
      memset(weather->wx_prec_24,0,10);
      memset(weather->wx_rain_total,0,10);
      memset(weather->wx_hum,0,5);
      memset(weather->wx_baro,0,10);
      memset(weather->wx_station,0,MAX_WXSTATION);

      if ((temp_conv=strchr((char *)data,'c')))   // Wind Direction in Degrees
      {
        xastir_snprintf(weather->wx_course,
                        sizeof(weather->wx_course),
                        "%s",
                        temp_conv+1);
        weather->wx_course[3] = '\0';
      }

      // Check for course = 0.  Change to 360.
      if (strncmp(weather->wx_course,"000",3) == 0)
      {
        xastir_snprintf(weather->wx_course,
                        sizeof(weather->wx_course),
                        "360");
      }

      if ((temp_conv=strchr((char *)data,'s')))   // Wind Speed in MPH - not snowfall
      {
        xastir_snprintf(weather->wx_speed,
                        sizeof(weather->wx_speed),
                        "%s",
                        temp_conv+1);
        weather->wx_speed[3] = '\0';
      }

      if ((temp_conv=strchr((char *)data,'g')))   // Wind Gust in MPH
      {
        xastir_snprintf(weather->wx_gust,
                        sizeof(weather->wx_gust),
                        "%s",
                        temp_conv+1);
        weather->wx_gust[3] = '\0';

        // compute high wind
        if(wx_high_wind[0] == '\0' ||   // first time
            (get_hours() == 0 && get_minutes() == 0) || // midnite
            (atol(weather->wx_gust) > atol(wx_high_wind)))   // gust
        {
          xastir_snprintf(wx_high_wind,
                          sizeof(wx_high_wind),
                          "%s",
                          weather->wx_gust);
        }
        wx_high_wind_on=1;
      }

      if ((temp_conv=strchr((char *)data,'t')))   // Temperature in Degrees F
      {
        xastir_snprintf(weather->wx_temp,
                        sizeof(weather->wx_temp),
                        "%s",
                        temp_conv+1);
        weather->wx_temp[3] = '\0';

        // compute hi temp, since APRS doesn't send that
        if(wx_hi_temp[0] == '\0' || // first time
            (get_hours() == 0 && get_minutes() == 0) || // midnite
            (atol(weather->wx_temp) > atol(wx_hi_temp)))
        {
          xastir_snprintf(wx_hi_temp,
                          sizeof(wx_hi_temp),
                          "%s",
                          weather->wx_temp);
        }
        wx_hi_temp_on=1;

        // compute low temp, since APRS doesn't send that
        if(wx_low_temp[0] == '\0' || // first time
            (get_hours() == 0 && get_minutes() == 0) || // midnite
            (atol(weather->wx_temp) < atol(wx_low_temp)))
        {
          xastir_snprintf(wx_low_temp,
                          sizeof(wx_low_temp),
                          "%s",
                          weather->wx_temp);
        }
        wx_low_temp_on=1;
      }

      if ((temp_conv=strchr((char *)data,'r')))   // Rain per hour
      {
        xastir_snprintf(weather->wx_rain,
                        sizeof(weather->wx_rain),
                        "%s",
                        temp_conv+1);
        weather->wx_rain[3] = '\0';
      }

      if ((temp_conv=strchr((char *)data,'p')))   // Rain per 24 hrs/total
      {
        xastir_snprintf(weather->wx_prec_24,
                        sizeof(weather->wx_prec_24),
                        "%s",
                        temp_conv+1);
        weather->wx_prec_24[3] = '\0';
      }

      if ((temp_conv=strchr((char *)data,'P')))   // Rain since midnight
      {
        xastir_snprintf(weather->wx_prec_00,
                        sizeof(weather->wx_prec_00),
                        "%s",
                        temp_conv+1);
        weather->wx_prec_00[3] = '\0';
      }

      if ((temp_conv=strchr((char *)data,'T')))   // Total Rain since
      {
        // wx station reset
        xastir_snprintf(weather->wx_rain_total,
                        sizeof(weather->wx_rain_total),
                        "%s",
                        temp_conv+1);
        weather->wx_rain_total[4] = '\0';
      }

      // Ok, here's the deal --- if we got total rain AND we didn't get
      // rain-since-midnight, fix it up.
      // This is a problem with LaCrosse --- no rain-since-midnight
      // provided.  Don't do anything at all if we didn't get total rain
      // from the station.  compute_rain *depends* on "total rain" being
      // a strictly increasing number that is never reset to zero during
      // Xastir's run.
      if (strlen(weather->wx_rain_total) >0 )
      {
        compute_rain((float)atof(weather->wx_rain_total));
        if (weather->wx_prec_00[0] == '\0')
        {
          /* rain since midnight */
          xastir_snprintf(weather->wx_prec_00,
                          sizeof(weather->wx_prec_00),
                          "%0.2f",
                          rain_00);
        }
      }

      // we are should be getting total rain from the station, but
      // we are also getting the rates.
      // Davis gives 24-hour, since-midnight, and 1-hour rates.
      // LaCrosse gives 24 and 1 hour.
      // Don't recompute what the station already gave us.
      weather->wx_compute_rain_rates=0;

      if ((temp_conv=strchr((char *)data,'h')))   // Humidity %
      {

        if (!strncmp(temp_conv+1,"00",2))    // APRS says 00 is
        {
          xastir_snprintf(weather->wx_hum, // 100% humidity
                          sizeof(weather->wx_hum),
                          "%s",
                          "100");
          weather->wx_hum[3] = '\0';
        }
        else
        {
          xastir_snprintf(weather->wx_hum, // humidity less than
                          sizeof(weather->wx_hum),     // 100%
                          "%s",
                          temp_conv+1);
          weather->wx_hum[2] = '\0';
        }
      }

      if ((temp_conv=strchr((char *)data,'b')))   // Air Pressure in 1/10 hPa
      {
        memset(temp_data1,0,sizeof(temp_data1));

        xastir_snprintf(temp_data1,
                        sizeof(temp_data1),
                        "%s",
                        temp_conv+1);
        temp_data1[5] = '\0';

        temp_temp = (float)(atof(temp_data1))/10.0;
        xastir_snprintf(temp_data1,
                        sizeof(temp_data1),
                        "%0.1f",
                        temp_temp);
        xastir_snprintf(weather->wx_baro,
                        sizeof(weather->wx_baro),
                        "%s",
                        temp_data1);
      }

      if ((temp_conv=strchr((char *)data,'x')))   // WX Station Identifier
      {
        xastir_snprintf(weather->wx_station,
                        sizeof(weather->wx_station),
                        "%s",
                        temp_conv+1);
        weather->wx_station[MAX_WXSTATION-1] = '\0';
      }

      // now compute wind chill
      wind_chill = 35.74 + .6215 * atof(weather->wx_temp) -
                   35.75 * pow(atof(weather->wx_gust), .16) +
                   .4275 * atof(weather->wx_temp) *
                   pow(atof(weather->wx_gust), .16);

      if((wind_chill < atof(weather->wx_temp)) &&
          (atof(weather->wx_temp) < 50))
      {

        xastir_snprintf(wx_wind_chill,
                        sizeof(wx_wind_chill),
                        "%.0f",
                        wind_chill);
        wx_wind_chill_on = 1;
      }
      else
      {
        wx_wind_chill_on = 0;
        wx_wind_chill[0] = '\0';
      }

      // The rest of the optional WX data is not used by
      // xastir (Luminosity, etc), except for snow, which
      // conflicts with wind speed (both are lower case 's')

      if (debug_level & 1)
        fprintf(stdout,"Davis Decode: wd-%s,ws-%s,wg-%s,t-%s,rh-%s,r00-%s,r24-%s,rt-%s,h-%s,ap-%s,station-%s\n",

                weather->wx_course,weather->wx_speed,weather->wx_gust,
                weather->wx_temp,weather->wx_rain,weather->wx_prec_00,
                weather->wx_prec_24,weather->wx_rain_total,
                weather->wx_hum,weather->wx_baro,weather->wx_station);
      break;
    // This is the output of the Davis APRS Data Logger.  The format
    // is in fact exactly the same as a regular APRS weather packet,
    // complete with position information.  Ignore that.
    // @xxxxxxzDDMM.mmN/DDDMM.mmW_CSE/SPDgGGGtTTTrRRRpRRRPRRRhXXbXXXXX.DsVP
    case (DAVISAPRSDL):

      memset(weather->wx_course,0,4);    // Keep out fradulent data...
      memset(weather->wx_speed,0,4);
      memset(weather->wx_gust,0,4);
      memset(weather->wx_temp,0,5);
      memset(weather->wx_rain,0,10);
      memset(weather->wx_prec_00,0,10);
      memset(weather->wx_prec_24,0,10);
      memset(weather->wx_rain_total,0,10);
      memset(weather->wx_hum,0,5);
      memset(weather->wx_baro,0,10);
      memset(weather->wx_station,0,MAX_WXSTATION);

      if (sscanf((char *)data,
                 "%*27s%3s/%3sg%3st%3sr%3sp%3sP%3sh%2sb%5s.DsVP",
                 weather->wx_course,
                 weather->wx_speed,
                 weather->wx_gust,
                 weather->wx_temp,
                 weather->wx_rain,
                 weather->wx_prec_24,
                 weather->wx_prec_00,
                 weather->wx_hum,
                 weather->wx_baro) == 9)
      {
        // then we got all the data out of the packet... now process
        // First null-terminate all the strings:
        weather->wx_course[3]='\0';
        weather->wx_speed[3]='\0';
        weather->wx_gust[3]='\0';
        weather->wx_temp[3]='\0';
        weather->wx_rain[3]='\0';
        weather->wx_prec_24[3]='\0';
        weather->wx_prec_00[3]='\0';
        weather->wx_hum[2]='\0';
        weather->wx_baro[6]='\0';


        // NOTE:  Davis APRS Data Logger does NOT provide total rain,
        // and so data from compute_rain (which needs total rain) will
        // be wrong.  Set this flag to stop that from clobbering our
        // good rain rate data.
        weather->wx_compute_rain_rates=0;

        // Check for course = 0.  Change to 360.
        if (strncmp(weather->wx_course,"000",3) == 0)
        {
          xastir_snprintf(weather->wx_course,
                          sizeof(weather->wx_course),
                          "360");
        }

        // compute high wind
        if(wx_high_wind[0] == '\0' ||   // first time
            (get_hours() == 0 && get_minutes() == 0) || // midnite
            (atol(weather->wx_gust) > atol(wx_high_wind)))   // gust
        {
          xastir_snprintf(wx_high_wind,
                          sizeof(wx_high_wind),
                          "%s",
                          weather->wx_gust);
        }
        wx_high_wind_on=1;

        // compute hi temp, since APRS doesn't send that
        if(wx_hi_temp[0] == '\0' || // first time
            (get_hours() == 0 && get_minutes() == 0) || // midnite
            (atol(weather->wx_temp) > atol(wx_hi_temp)))
        {
          xastir_snprintf(wx_hi_temp,
                          sizeof(wx_hi_temp),
                          "%s",
                          weather->wx_temp);
        }
        wx_hi_temp_on=1;

        // compute low temp, since APRS doesn't send that
        if(wx_low_temp[0] == '\0' || // first time
            (get_hours() == 0 && get_minutes() == 0) || // midnite
            (atol(weather->wx_temp) < atol(wx_low_temp)))
        {
          xastir_snprintf(wx_low_temp,
                          sizeof(wx_low_temp),
                          "%s",
                          weather->wx_temp);
        }
        wx_low_temp_on=1;


        // fix up humidity --- 00 in APRS means 100%:
        if (strncmp(weather->wx_hum,"00",2)==0)
        {
          weather->wx_hum[0]='1';
          weather->wx_hum[1]=weather->wx_hum[2]='0';
          weather->wx_hum[3]='\0';
        }

        // fix up barometer.  APRS sends in 10ths of millibars:
        temp_temp=(float)(atof(weather->wx_baro))/10.0;
        weather->wx_baro[0]='\0'; // zero out so snprintf doesn't append
        xastir_snprintf(weather->wx_baro,
                        sizeof(weather->wx_baro),
                        "%0.1f",
                        temp_temp);  // this should terminate Just Fine.

        // now compute wind chill
        wind_chill = 35.74 + .6215 * atof(weather->wx_temp) -
                     35.75 * pow(atof(weather->wx_gust), .16) +
                     .4275 * atof(weather->wx_temp) *
                     pow(atof(weather->wx_gust), .16);

        if((wind_chill < atof(weather->wx_temp)) &&
            (atof(weather->wx_temp) < 50))
        {

          xastir_snprintf(wx_wind_chill,
                          sizeof(wx_wind_chill),
                          "%.0f",
                          wind_chill);
          wx_wind_chill_on = 1;
        }
        else
        {
          wx_wind_chill_on = 0;
          wx_wind_chill[0] = '\0';
        }
        xastir_snprintf(weather->wx_station,
                        sizeof(weather->wx_station),
                        "%s",
                        (char *) &(data[63]));

        /* Heat Index Calculation*/
        hi_hum=atoi(weather->wx_hum);
        rh2= atoi(weather->wx_hum);
        rh2=(rh2 * rh2);
        hidx_temp=atoi(weather->wx_temp);
        t2= atoi(weather->wx_temp);
        t2=(t2 * t2);

        if (hidx_temp >= 70)
        {
          heat_index=-42.379+2.04901523 * hidx_temp+10.1433127 * hi_hum-0.22475541
                     * hidx_temp * hi_hum-0.00683783 * t2-0.05481717 * rh2+0.00122874
                     * t2 * hi_hum+0.00085282 * hidx_temp * rh2-0.00000199 * t2 * rh2;
          xastir_snprintf (wx_heat_index,
                           sizeof(wx_heat_index),
                           "%03d",
                           heat_index);
          wx_heat_index_on = 1;
        }
        else
        {
          wx_heat_index_on = 0;
        }


        if (debug_level & 1)
          fprintf(stdout,"Davis APRS DataLogger Decode $Revision$: wd-%s,ws-%s,wg-%s,t-%s,rh-%s,r24-%s,r00-%s,h-%s,ap-%s,station-%s\n",

                  weather->wx_course,weather->wx_speed,weather->wx_gust,
                  weather->wx_temp, weather->wx_rain,
                  weather->wx_prec_24, weather->wx_prec_00,weather->wx_hum,weather->wx_baro,
                  weather->wx_station);
      }
      break;

  }
  // End of big switch
}   // End of wx_fill_data()





//**********************************************************
// Decode WX data line
// wx_line: raw wx data to decode
//
// This is called from main.c:UpdateTime() only.  It
// decodes data for serially-connected and network-connected
// WX interfaces only.   It calls wx_fill_data() to do the
// real work once it figures out what type of data it has.
//**********************************************************
//
// Note that the length of "wx_line" can be up to MAX_DEVICE_BUFFER,
// which is currently set to 4096.
//
void wx_decode(unsigned char *wx_line, int data_length, int port)
{
  DataRow *p_station;
  int decoded;
  int find;
  int i;
  int len;
  char time_data[MAX_TIME];
  unsigned int check_sum;
  int max;
  WeatherRow *weather;
  float t1,t2,t3,t4,t5,t6,t9,t10,t11,t12,t13,t14,t15,t16,t17,t18,t19;
  int t7,t8;


  //fprintf(stderr,"wx_decode: %s\n",wx_line);
  //fprintf(stderr,"\nwx_decode: %d bytes\n", data_length);

  find=0;

  len = data_length;
  if (len == 0)
  {
    len=strlen((char *)wx_line);
  }

  if (len>10 || ((int)wx_line[0]!=0 && port_data[port].data_type==1))
  {
    if (search_station_name(&p_station,my_callsign,1))
    {
      if (get_weather_record(p_station))      // DK7IN: only add record if we found something...
      {
        weather = p_station->weather_data;

        decoded=0;
        /* Ok decode wx data */
        if (wx_line[0]=='!'
            && wx_line[1]=='!'
            && is_xnum_or_dash((char *)(wx_line+2),40)
            && port_data[port].data_type==0)
        {

          /* Found Peet Bros U-2k */
          if (debug_level & 1)
          {
            fprintf(stderr,"Found Peet Bros U-2k WX:%s\n",wx_line+2);
          }

          xastir_snprintf(wx_station_type,
                          sizeof(wx_station_type),
                          "%s",
                          langcode("WXPUPSI011"));

          xastir_snprintf(raw_wx_string,
                          sizeof(raw_wx_string),
                          "%s",
                          wx_line);

          raw_wx_string[MAX_RAW_WX_STRING] = '\0';    // Terminate it

          xastir_snprintf(weather->wx_time,
                          sizeof(weather->wx_time),
                          "%s",
                          get_time(time_data));

          weather->wx_sec_time=sec_now();
          //weather->wx_data=1;
          wx_fill_data(0,APRS_WX3,wx_line,p_station);
          decoded=1;
        }
        else if (((wx_line[0]=='#') || (wx_line[0]=='*'))
                 && is_xnum_or_dash((char *)(wx_line+1),13)
                 && port_data[port].data_type==0)
        {

          /* Found Peet Bros raw U2 data */
          xastir_snprintf(wx_station_type,
                          sizeof(wx_station_type),
                          "%s",
                          langcode("WXPUPSI012"));

          if (debug_level & 1)
          {
            fprintf(stderr,"Found Peet Bros raw U2 data WX#:%s\n",wx_line+1);
          }

          xastir_snprintf(raw_wx_string,
                          sizeof(raw_wx_string),
                          "%s",
                          wx_line);

          raw_wx_string[MAX_RAW_WX_STRING] = '\0'; // Terminate it

          xastir_snprintf(weather->wx_time,
                          sizeof(weather->wx_time),
                          "%s",
                          get_time(time_data));
          weather->wx_sec_time=sec_now();
          //weather->wx_data=1;

          if (wx_line[0]=='#')    // Wind speed in km/h
          {
            wx_fill_data(0,APRS_WX4,wx_line,p_station);
          }
          else               // '*', Wind speed in mph
          {
            wx_fill_data(0,APRS_WX6,wx_line,p_station);
          }

          decoded=1;
        }
        else if (strncmp("$ULTW",(char *)wx_line,5)==0
                 && is_xnum_or_dash((char *)(wx_line+5),44)
                 && port_data[port].data_type==0)
        {

          /* Found Peet Bros raw U2 data */

          xastir_snprintf(wx_station_type,
                          sizeof(wx_station_type),
                          "%s",
                          langcode("WXPUPSI013"));

          if (debug_level & 1)
          {
            fprintf(stderr,"Found Peet Bros Ultimeter Packet data WX#:%s\n",wx_line+5);
          }

          xastir_snprintf(raw_wx_string,
                          sizeof(raw_wx_string),
                          "%s",
                          wx_line);
          raw_wx_string[MAX_RAW_WX_STRING] = '\0'; // Terminate it

          weather->wx_sec_time=sec_now();
          //weather->wx_data=1;
          wx_fill_data(0,APRS_WX5,wx_line,p_station);
          decoded=1;
        }
        else if (wx_line[2]==' ' && wx_line[5]==' ' && wx_line[8]=='/' && wx_line[11]=='/'
                 && wx_line[14]==' ' && wx_line[17]==':' && wx_line[20]==':'
                 && strncmp(" #0:",(char *)wx_line+23,4)==0 && port_data[port].data_type==0)
        {
          find=0;
          for (i=len; i>23 && !find; i--)
          {
            if ((int)wx_line[i]==0x03)
            {
              find=1;
              wx_line[i] = 0;
            }
          }
          if (find)
          {

            /* found Qualimetrics Q-Net station */

            xastir_snprintf(wx_station_type,
                            sizeof(wx_station_type),
                            "%s",
                            langcode("WXPUPSI016"));

            if (debug_level & 1)
            {
              fprintf(stderr,"Found Qualimetrics Q-Net station data WX#:%s\n",wx_line+23);
            }

            xastir_snprintf(weather->wx_time,
                            sizeof(weather->wx_time),
                            "%s",
                            get_time(time_data));
            weather->wx_sec_time=sec_now();
            //weather->wx_data=1;
            wx_fill_data(0,QM_WX,wx_line+24,p_station);
            decoded=1;
          }
        }

//WE7U
        // else look for ten ASCII decimal point chars in the input, or do an
        // sscanf looking for the correct number and types of fields for the
        // OWW server in ARNE mode.  6 %f's, 2 %d's, 11 %f's.
        else if (sscanf((const char *)wx_line,"%f %f %f %f %f %f %d %d %f %f %f %f %f %f %f %f %f %f %f", &t1,&t2,&t3,&t4,&t5,&t6,&t7,&t8,&t9,&t10,&t11,&t12,&t13,&t14,&t15,&t16,&t17,&t18,&t19) == 19)
        {

          // Found Dallas One-Wire Weather Station
          if (debug_level & 1)
          {
            fprintf(stderr,"Found OWW ARNE-mode(19) one-wire weather station data\n");
          }

          weather->wx_sec_time=sec_now();
          wx_fill_data(0,DALLAS_ONE_WIRE,wx_line,p_station);
          decoded=1;
        }

        else if (sscanf((const char *)wx_line,"%f %f %f %f %f %f %d %d %f %f %f %f", &t1,&t2,&t3,&t4,&t5,&t6,&t7,&t8,&t9,&t10,&t11,&t12) == 12)
        {

          // Found Dallas One-Wire Weather Station
          if (debug_level & 1)
          {
            fprintf(stderr,"Found OWW ARNE-mode(12) one-wire weather station data\n");
          }

          weather->wx_sec_time=sec_now();
          wx_fill_data(0,DALLAS_ONE_WIRE,wx_line,p_station);
          decoded=1;
        }

        else if (strncmp("&CR&",(char *)wx_line,4)==0
                 && is_xnum_or_dash((char *)(wx_line+5),44)
                 && port_data[port].data_type==0)
        {

          if (debug_level & 1)
          {
            fprintf(stderr,"Found Peet Complete station data\n");
          }

          xastir_snprintf(wx_station_type,
                          sizeof(wx_station_type),
                          "%s",
                          langcode("WXPUPSI017"));

          xastir_snprintf(raw_wx_string,
                          sizeof(raw_wx_string),
                          "%s",
                          wx_line);

          raw_wx_string[MAX_RAW_WX_STRING] = '\0'; // Terminate it

          weather->wx_sec_time=sec_now();
          //weather->wx_data=1;
          wx_fill_data(0,PEET_COMPLETE,wx_line,p_station);
          decoded=1;
        }

        else if (port_data[port].data_type==1)
        {
//                    int jj;

          /* binary data type */
          if (debug_level & 1)
          {
            fprintf(stderr,"Found binary data:  %d bytes\n", len);
          }

          /* clear raw string */
          memset(raw_wx_string,0,sizeof(raw_wx_string));
          max=0;
          switch (wx_line[0])
          {
            case 0x8f:
              max=34;
              break;

            case 0x9f:
              max=33;
              break;

            case 0xaf:
              max=30;
              break;

            case 0xbf:
              max=13;
              break;

            case 0xcf:
              max=26;
              break;

            default:
              break;
          }

//                    fprintf(stderr, "wx_decode binary: ");
//                    for (jj = 0; jj < len+1; jj++) {
//                        fprintf(stderr, "%02x ", wx_line[jj]);
//                    }
//                    fprintf(stderr, "\n");
//                    fprintf(stderr, "Integers: ");
//                    for (jj = 0; jj < max+1; jj++) {
//                        fprintf(stderr, "%0d ", wx_line[jj]);
//                    }
//                    fprintf(stderr, "\n");


          if (len < (max+1))
          {
            fprintf(stderr, " Short NET_WX packet, %d bytes\n", len);
          }

          if (max > 0 && len >= (max+1) )
          {

            // Compute the checksum from the data
            check_sum = 0;

            for (i = 0; i < max; i++)
            {
              check_sum += wx_line[i];
            }
//                        fprintf(stderr," Checksum: 0x%02x ", 0x0ff & check_sum);

            if ( wx_line[max] == (0xff & check_sum) )
            {

              /* good RS WX-200 data */
              //fprintf(stderr,"GOOD RS WX-200 %0X data\n",wx_line[0]);
              /* found RS WX-200 */
              xastir_snprintf(wx_station_type,
                              sizeof(wx_station_type),
                              "%s",
                              langcode("WXPUPSI025"));
              xastir_snprintf(weather->wx_time,
                              sizeof(weather->wx_time),
                              "%s",
                              get_time(time_data));
              weather->wx_sec_time=sec_now();
              //weather->wx_data=1;
              wx_fill_data(0,RSWX200,wx_line,p_station);
              decoded=1;
            }
            else
            {
//                            fprintf(stderr, "bad");
            }
          }
        }
        else if (wx_line[0]=='@' && strncmp((char *)&(wx_line[63]),".DsVP",5)==0)
        {
          // this is a Davis Vantage Pro with APRS Data Logger
          if (debug_level & 1)
          {
            fprintf(stdout,"Davis VP APRS Data Logger data found ... %s\n", wx_line);
          }
          xastir_snprintf(wx_station_type,
                          sizeof(wx_station_type),
                          "%s",
                          langcode("WXPUPSI028"));

          xastir_snprintf(weather->wx_time,
                          sizeof(weather->wx_time),
                          "%s",
                          get_time(time_data));
          weather->wx_sec_time=sec_now();
          wx_fill_data(0,DAVISAPRSDL,wx_line,p_station);
          decoded=1;
        }
        else      // ASCII data of undetermined type
        {

          // Davis Weather via meteo -> db2APRS -> TCP port

          if (strstr((const char *)wx_line, "xDvs"))    // APRS 'postionless' WX data w/ Davis & X tag
          {

            if (debug_level & 1)
            {
              fprintf(stdout,"Davis Data found... %s\n",wx_line);
            }

            xastir_snprintf(wx_station_type,
                            sizeof(wx_station_type),
                            "%s",
                            langcode("WXPUPSI026"));
            xastir_snprintf(weather->wx_time,
                            sizeof(weather->wx_time),
                            "%s",
                            get_time(time_data));
            weather->wx_sec_time=sec_now();
            wx_fill_data(0,DAVISMETEO,wx_line,p_station);
            decoded=1;
          }

          else if (debug_level & 1)
          {
            fprintf(stderr,"Unknown WX DATA:%s\n",wx_line);
          }
        }

        if (decoded)
        {
          /* save data back */

          if (begin_critical_section(&port_data_lock, "wx.c:wx_decode(1)" ) > 0)
          {
            fprintf(stderr,"port_data_lock, Port = %d\n", port);
          }

          port_data[port].decode_errors=0;

          if (end_critical_section(&port_data_lock, "wx.c:wx_decode(2)" ) > 0)
          {
            fprintf(stderr,"port_data_lock, Port = %d\n", port);
          }

          statusline(langcode("BBARSTA032"),1);       // Decoded WX Data
          /* redraw now */
          //redraw_on_new_data=2;
          redraw_on_new_data=1;
          fill_wx_data();
        }
        else
        {
          /* Undecoded packet */
          memset(raw_wx_string,0,sizeof(raw_wx_string));

          if (begin_critical_section(&port_data_lock, "wx.c:wx_decode(3)" ) > 0)
          {
            fprintf(stderr,"port_data_lock, Port = %d\n", port);
          }

          port_data[port].decode_errors++;    // We have errors in decoding
          if (port_data[port].decode_errors>10)
          {
            /* wrong data type? */
            port_data[port].data_type++;    // Try another data type. 0=ascii, 1=wx binary
            port_data[port].data_type&=0x01;
            /*if (debug_level & 1)*/
            fprintf(stderr,"Data type %d\n",port_data[port].data_type);
            port_data[port].decode_errors=0;
          }

          if (end_critical_section(&port_data_lock, "wx.c:wx_decode(4)" ) > 0)
          {
            fprintf(stderr,"port_data_lock, Port = %d\n", port);
          }

        }
      }
    }
  }
}





/***********************************************************/
/* fill string with WX data for transmit                   */
/*                            */
/***********************************************************/

time_t wx_tx_data1(char *st, int st_size)
{
  DataRow *p_station;
  time_t wx_time;
  char temp[100];
  WeatherRow *weather;

  st[0] = '\0';
  wx_time = 0;
  if (search_station_name(&p_station,my_callsign,1))
  {
    if (get_weather_record(p_station))      // station has wx data
    {
      weather = p_station->weather_data;


//WE7U: For debugging purposes only
//weather->wx_sec_time=sec_now();
//sprintf(weather->wx_course,"359");          // degrees
//sprintf(weather->wx_speed,"001");           // mph
//sprintf(weather->wx_gust,"010");            // mph
//sprintf(weather->wx_temp,"069");            // Farenheit

//if ( strlen(weather->wx_rain_total) == 0)
//    sprintf(weather->wx_rain_total,"1900.40");

//sprintf(weather->wx_rain_total,"1987.6");   // hundredths of an inch
//compute_rain(atof(weather->wx_rain_total));
//sprintf(weather->wx_rain_total,"1988.6");
//compute_rain(atof(weather->wx_rain_total));
//sprintf(weather->wx_rain_total,"1990.6");

//compute_rain(atof(weather->wx_rain_total));
//sprintf(weather->wx_rain,"%0.2f",rain_minute_total);
//sprintf(weather->wx_prec_24,"%0.2f",rain_24);
//sprintf(weather->wx_prec_00,"%0.2f",rain_00);

//sprintf(weather->wx_rain,"0");            // hundredths of an inch
//sprintf(weather->wx_prec_00,"0");         // hundredths of an inch
//sprintf(weather->wx_prec_24,"0");         // hundredths of an inch

//sprintf(weather->wx_hum,"92");              // %
//sprintf(weather->wx_baro,"1013.0");         // hPa
//weather->wx_type = WX_TYPE;
//xastir_snprintf(weather->wx_station,sizeof(weather->wx_station),"RSW");
//  359/000g000t065r010P020p030h92b01000


      if (strlen(weather->wx_course) > 0 && strlen(weather->wx_speed) > 0)
      {
        // We have enough wx_data
        wx_time=weather->wx_sec_time;
        xastir_snprintf(temp,
                        sizeof(temp),
                        "%s",
                        weather->wx_course);
        if (strlen(temp) > 3)
        {
          if (debug_level & 1)
          {
            fprintf(stderr,"wx_course too long: %s\n", temp);
          }
          xastir_snprintf(temp,
                          sizeof(temp),
                          "...");
        }
        if ( (atoi(weather->wx_course) > 360) || (atoi(weather->wx_course) < 0) )
        {
          if (debug_level & 1)
          {
            fprintf(stderr,"wx_course out-of-range: %s\n", weather->wx_course);
          }
          xastir_snprintf(temp,
                          sizeof(temp),
                          "...");
        }
        //sprintf(st,"%s/%s",weather->wx_course,weather->wx_speed);
        strncat(st, temp, st_size - 1 - strlen(st));
        strncat(st, "/", st_size - 1 - strlen(st));

        xastir_snprintf(temp,
                        sizeof(temp),
                        "%s",
                        weather->wx_speed);
        if (strlen(temp) > 3)
        {
          if (debug_level & 1)
          {
            fprintf(stderr,"wx_speed too long: %s\n", temp);
          }
          xastir_snprintf(temp,
                          sizeof(temp),
                          "...");
        }
        if ( (atoi(weather->wx_speed) < 0) || (atoi(weather->wx_speed) > 999) )
        {
          if (debug_level & 1)
          {
            fprintf(stderr,"wx_speed out-of-range: %s\n", weather->wx_speed);
          }
          xastir_snprintf(temp,
                          sizeof(temp),
                          "...");
        }
        strncat(st, temp, st_size - 1 - strlen(st));
      }
      else
      {
        // We don't have enough wx_data, may be from a Qualimetrics Q-Net?
        wx_time=weather->wx_sec_time;
        xastir_snprintf(st,
                        st_size,
                        ".../...");


        if (debug_level & 1)
        {
          fprintf(stderr,"\n\nAt least one field was empty: Course: %s\tSpeed: %s\n", weather->wx_course, weather->wx_speed);
          fprintf(stderr,"Will be sending '.../...' instead of real values.\n\n\n");
        }


      }
      if (strlen(weather->wx_gust) > 0)
      {
        xastir_snprintf(temp,
                        sizeof(temp),
                        "g%s",
                        weather->wx_gust);
        if (strlen(temp) > 4)
        {
          if (debug_level & 1)
          {
            fprintf(stderr,"wx_gust too long: %s\n", temp);
          }

          xastir_snprintf(temp,
                          sizeof(temp),
                          "g...");
        }
        if (atoi(weather->wx_gust) < 0)
        {
          if (debug_level & 1)
          {
            fprintf(stderr,"wx_gust out-of-range: %s\n", weather->wx_gust);
          }

          xastir_snprintf(temp,
                          sizeof(temp),
                          "g...");
        }
        strncat(st, temp, st_size - 1 - strlen(st));
      }
      else
      {
        strncat(st, "g...", st_size - 1 - strlen(st));
      }

      if (strlen(weather->wx_temp) > 0)
      {
        xastir_snprintf(temp,
                        sizeof(temp),
                        "t%s",
                        weather->wx_temp);
        if (strlen(temp) > 4)
        {
          if (debug_level & 1)
          {
            fprintf(stderr,"wx_temp too long: %s\n", temp);
          }

          xastir_snprintf(temp,
                          sizeof(temp),
                          "t...");
        }
        if ( (atoi(weather->wx_temp) > 999) || (atoi(weather->wx_temp) < -99) )
        {
          if (debug_level & 1)
          {
            fprintf(stderr,"wx_temp out-of-bounds: %s\n", weather->wx_temp);
          }

          xastir_snprintf(temp,
                          sizeof(temp),
                          "t...");
        }
        strncat(st, temp, st_size - 1 - strlen(st));
      }
      else
      {
        strncat(st, "t...", st_size - 1 - strlen(st));
      }

      if (strlen(weather->wx_rain) > 0)
      {
        xastir_snprintf(temp,
                        sizeof(temp),
                        "r%03d",
                        (int)(atof(weather->wx_rain) + 0.5) );  // Cheater's way of rounding
        if (strlen(temp)>4)
        {
          if (debug_level & 1)
          {
            fprintf(stderr,"wx_rain too long: %s\n", temp);
          }

          // Don't transmit this field if it's not valid
          xastir_snprintf(temp,
                          sizeof(temp),
                          "r   ");
        }
        if (atoi(weather->wx_rain) < 0)
        {
          if (debug_level & 1)
          {
            fprintf(stderr,"wx_rain out-of-bounds: %s\n", weather->wx_rain);
          }

          // Don't transmit this field if it's not valid
          xastir_snprintf(temp,
                          sizeof(temp),
                          "r...");
        }
        strncat(st, temp, st_size - 1 - strlen(st));
      }
      else
      {
        // Don't transmit this field if it's not valid
        //strncat(st, "r...", st_size - 1 - strlen(st));
      }

      if (strlen(weather->wx_prec_00) > 0)
      {
        xastir_snprintf(temp,
                        sizeof(temp),
                        "P%03d",
                        (int)(atof(weather->wx_prec_00) + 0.5) );   // Cheater's way of rounding
        if (strlen(temp)>4)
        {
          if (debug_level & 1)
          {
            fprintf(stderr,"wx_prec_00 too long: %s\n", temp);
          }

          // Don't transmit this field if it's not valid
          xastir_snprintf(temp,
                          sizeof(temp),
                          "P   ");
        }
        if (atoi(weather->wx_prec_00) < 0)
        {
          if (debug_level & 1)
          {
            fprintf(stderr,"wx_prec_00 out-of-bounds: %s\n", weather->wx_prec_00);
          }

          // Don't transmit this field if it's not valid
          xastir_snprintf(temp,
                          sizeof(temp),
                          "P...");
        }
        strncat(st, temp, st_size - 1 - strlen(st));
      }
      else
      {
        // Don't transmit this field if it's not valid
        //strncat(st, "P...", st_size - 1 - strlen(st));
      }

      if (strlen(weather->wx_prec_24) > 0)
      {
        xastir_snprintf(temp,
                        sizeof(temp),
                        "p%03d",
                        (int)(atof(weather->wx_prec_24) + 0.5) );   // Cheater's way of rounding
        if (strlen(temp)>4)
        {
          if (debug_level & 1)
          {
            fprintf(stderr,"wx_prec_24 too long: %s\n", temp);
          }

          // Don't transmit this field if it's not valid
          xastir_snprintf(temp,
                          sizeof(temp),
                          "p   ");
        }
        if (atoi(weather->wx_prec_24) < 0)
        {
          if (debug_level & 1)
          {
            fprintf(stderr,"wx_prec_24 out-of-bounds: %s\n", weather->wx_prec_24);
          }

          // Don't transmit this field if it's not valid
          xastir_snprintf(temp,
                          sizeof(temp),
                          "p...");
        }
        strncat(st, temp, st_size - 1 - strlen(st));
      }
      else
      {
        // Don't transmit this field if it's not valid
        //strncat(st, "p...", st_size - 1 - strlen(st));
      }

      if (strlen(weather->wx_hum) > 0)
      {
        if (atoi(weather->wx_hum)>99)
        {
          xastir_snprintf(temp,
                          sizeof(temp),
                          "h00");
        }
        else
          xastir_snprintf(temp,
                          sizeof(temp),
                          "h%02d",
                          atoi(weather->wx_hum));

        if (strlen(temp) > 4)
        {
          if (debug_level & 1)
          {
            fprintf(stderr,"wx_hum too long: %s\n", temp);
          }

          // Don't transmit this field if it's not valid
          //xastir_snprintf(temp,sizeof(temp),"h..");
        }
        if (atoi(weather->wx_hum) < 0)
        {
          if (debug_level & 1)
          {
            fprintf(stderr,"wx_hum out-of-bounds: %s\n", weather->wx_hum);
          }
          // Don't transmit this field if it's not valid
          //xastir_snprintf(temp,sizeof(temp),"h..");
        }
        strncat(st, temp, st_size - 1 - strlen(st));
      }
      else
      {
        // Don't transmit this field if it's not valid
        //strncat(st, "h..", st_size - 1 - strlen(st));
      }

      if (strlen(weather->wx_baro) > 0)
      {
        xastir_snprintf(temp,
                        sizeof(temp),
                        "b%05d",
                        (int)((atof(weather->wx_baro) * 10.0)) );
        if (strlen(temp)>6)
        {
          if (debug_level & 1)
          {
            fprintf(stderr,"wx_baro too long: %s\n", temp);
          }
          // Don't transmit this field if it's not valid
          //xastir_snprintf(temp,sizeof(temp),"b.....");
        }
        if ((int)((atof(weather->wx_baro) * 10.0) < 0))
        {
          if (debug_level & 1)
          {
            fprintf(stderr,"wx_baro out-of-bounds: %s\n", weather->wx_baro);
          }
          // Don't transmit this field if it's not valid
          //xastir_snprintf(temp,sizeof(temp),"b.....");
        }
        strncat(st, temp, st_size - 1 - strlen(st));
      }
      else
      {
        // Don't transmit this field if it's not valid
        //strncat(st, "b.....", st_size - 1 - strlen(st));
      }

      xastir_snprintf(temp,
                      sizeof(temp),
                      "%c%s",
                      weather->wx_type,
                      weather->wx_station);
      strncat(st, temp, st_size - 1 - strlen(st));
    }
  }


  if (debug_level & 1)
  {
    fprintf(stderr,"Weather String:  %s\n", st);
  }


  return(wx_time);
}





/***********************************************************/
/* transmit raw wx data                                    */
/*                                                         */
/***********************************************************/
void tx_raw_wx_data(void)
{

  if (strlen(raw_wx_string)>10)
  {
    output_my_data(raw_wx_string,-1,0,0,0,NULL);
    if (debug_level & 1)
    {
      fprintf(stderr,"Sending Raw WX data <%s>\n",raw_wx_string);
    }
  }
}


