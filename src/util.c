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

#include <unistd.h>
#include <signal.h>
#include <string.h>

// Needed for Solaris
#ifdef HAVE_STRINGS_H
  #include <strings.h>
#endif  // HAVE_STRINGS_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <ctype.h>
#include <math.h>
#include <errno.h>

#include "xastir.h"
#include "util.h"
#include "main.h"
#include "xa_config.h"
#include "datum.h"
#include "hashtable.h"
#include "hashtable_itr.h"
#include "maps.h"


#define CHECKMALLOC(m)  if (!m) { fprintf(stderr, "***** Malloc Failed *****\n"); exit(0); }

// For mutex debugging with Linux threads only
#ifdef MUTEX_DEBUG
  #include <asm/errno.h>
  //
  // Newer pthread function
  #ifdef HAVE_PTHREAD_MUTEXATTR_SETTYPE
    extern int pthread_mutexattr_settype(pthread_mutexattr_t *attr, int kind);
  #endif  // HAVE_PTHREAD_MUTEXATTR_SETTYPE
  //
  // Older, deprecated pthread function
  #ifdef HAVE_PTHREAD_MUTEXATTR_SETKIND_NP
    extern int pthread_mutexattr_setkind_np(pthread_mutexattr_t *attr, int kind);
  #endif  // HAVE_PTHREAD_MUTEXATTR_SETKIND_NP
  //
#endif  // MUTEX_DEBUG


#ifdef HAVE_LIBCURL
  #include <curl/curl.h>
#endif  // HAVE_LIBCURL

// Needed for size_t
#include <sys/types.h>


// Must be last include file
#include "leak_detection.h"



int position_amb_chars;
char echo_digis[6][MAX_CALLSIGN+1];

#define ACCEPT_0N_0E    /* set this to see stations at 0N/0E on the map */

struct timeval timer_start;
struct timeval timer_stop;
struct timezone tz;


static struct hashtable *tactical_hash = NULL;
#define TACTICAL_HASH_SIZE 1024

#define MAX_LOGFILE_SIZE 2048000


/////////////////////////////////////////////////////////////////////
// convert_from_xastir_coordinates()
//
// Converts from Xastir coordinate system to lat/lon.  First two
// parameters are the output floating point lat/lon values.  2nd two
// are the input Xastir X/Y values.
//
//              0 (90 deg. or 90N)
//
// 0 (-180 deg. or 180W)      129,600,000 (180 deg. or 180E)
//
//          64,800,000 (-90 deg. or 90S)
//
// Returns 0 if error, 1 if good values were converted.
//         Errors are due to the x and/or y values exceeding the above
//         limits. In such cases the float values are set to appropriate
//         minimum or maximum values.
/////////////////////////////////////////////////////////////////////
int convert_from_xastir_coordinates ( float *f_longitude,
                                      float *f_latitude,
                                      long x,
                                      long y )
{

//fprintf(stderr,"convert_from_xastir_coordinates\n");
  int result = 1;  // assume the input values are in range

  if (x < 0l )
  {
    fprintf(stderr,
            "convert_from_xastir_coordinates:X out-of-range (too low):%lu\n",
            x);
    x = 0;
    result = 0;
  }

  if (x > 129600000l)
  {
    fprintf(stderr,
            "convert_from_xastir_coordinates:X out-of-range (too high):%lu\n",
            x);
    x = 129600000l;
    result = 0;
  }

  if (y < 0l)
  {
    fprintf(stderr,
            "convert_from_xastir_coordinates:Y out-of-range (too low):%lu\n",
            y);
    y = 0;
    result = 0;
  }

  if (y > 64800000l)
  {
    fprintf(stderr,
            "convert_from_xastir_coordinates:Y out-of-range (too high):%lu\n",
            y);
    y = 64800000l;
    result = 0;
  }

  *f_latitude  = (float)( -((y - 32400000l) / 360000.0) );
  *f_longitude = (float)(   (x - 64800000l) / 360000.0  );

//fprintf(stderr,"input x: %lu\tinput y: %lu\n",
//    x,
//    y);
//fprintf(stderr,"latitude: %f\tlongitude: %f\n",
//    *f_latitude,
//    *f_longitude);

  return(result);
}





/////////////////////////////////////////////////////////////////////
// convert_to_xastir_coordinates()
//
// Converts from lat/lon to Xastir coordinate system.
// First two parameters are the output Xastir X/Y values,
// 2nd two are the input floating point lat/lon values.
//
//              0 (90 deg. or 90N)
//
// 0 (-180 deg. or 180W)      129,600,000 (180 deg. or 180E)
//
//          64,800,000 (-90 deg. or 90S)
//
// Returns 0 if error, 1 if good values were converted.
/////////////////////////////////////////////////////////////////////
int convert_to_xastir_coordinates ( unsigned long* x,
                                    unsigned long* y,
                                    float f_longitude,
                                    float f_latitude )
{

  int ok = 1;

  *y = (unsigned long)(32400000l + (360000.0 * (-f_latitude)));
  *x = (unsigned long)(64800000l + (360000.0 * f_longitude));

  if (f_longitude < -180.0)
  {
    fprintf(stderr,
            "convert_to_xastir_coordinates:Longitude out-of-range (too low):%f\n",
            f_longitude);
    *x = 0;
    ok = 0;
  }

  if (f_longitude >  180.0)
  {
    fprintf(stderr,
            "convert_to_xastir_coordinates:Longitude out-of-range (too high):%f\n",
            f_longitude);
    *x = 129600000l;
    ok = 0;
  }

  if (f_latitude <  -90.0)
  {
    fprintf(stderr,
            "convert_to_xastir_coordinates:Latitude out-of-range (too low):%f\n",
            f_latitude);
    *y = 0;
    ok = 0;
  }

  if (f_latitude >   90.0)
  {
    fprintf(stderr,
            "convert_to_xastir_coordinates:Latitude out-of-range (too high):%f\n",
            f_latitude);
    *y = 64800000l;
    ok =0;
  }

  return(ok);
}





// Multiply all the characters in the callsign, truncated to
// TACTICAL_HASH_SIZE
//
unsigned int tactical_hash_from_key(void *key)
{
  unsigned char *jj = key;
  unsigned int tac_hash = 1;

  while (*jj != '\0')
  {
    tac_hash = tac_hash * (unsigned int)*jj++;
  }

  tac_hash = tac_hash % TACTICAL_HASH_SIZE;

//    fprintf(stderr,"hash = %d\n", tac_hash);
  return (tac_hash);
}





int tactical_keys_equal(void *key1, void *key2)
{

//fprintf(stderr,"Comparing %s to %s\n",(char *)key1,(char *)key2);
  if (strlen((char *)key1) == strlen((char *)key2)
      && strncmp((char *)key1,(char *)key2,strlen((char *)key1))==0)
  {
//fprintf(stderr,"    match\n");
    return(1);
  }
  else
  {
//fprintf(stderr,"  no match\n");
    return(0);
  }
}





void init_tactical_hash(int clobber)
{
//    fprintf(stderr," Initializing tactical hash \n");
  // make sure we don't leak
//fprintf(stderr,"init_tactical_hash\n");
  if (tactical_hash)
  {
//fprintf(stderr,"Already have one!\n");
    if (clobber)
    {
//fprintf(stderr,"Clobbering hash table\n");
      hashtable_destroy(tactical_hash, 1);
      tactical_hash=create_hashtable(TACTICAL_HASH_SIZE,
                                     tactical_hash_from_key,
                                     tactical_keys_equal);
    }
  }
  else
  {
//fprintf(stderr,"Creating hash table from scratch\n");
    tactical_hash=create_hashtable(TACTICAL_HASH_SIZE,
                                   tactical_hash_from_key,
                                   tactical_keys_equal);
  }
}





char *get_tactical_from_hash(char *callsign)
{
  char *result;

  if (callsign == NULL || *callsign == '\0')
  {
    fprintf(stderr,"Empty callsign passed to get_tactical_from_hash()\n");
    return(NULL);
  }

  if (!tactical_hash)    // no table to search
  {
//fprintf(stderr,"Creating hash table\n");
    init_tactical_hash(1); // so create one
    return NULL;
  }

//    fprintf(stderr,"   searching for %s...",callsign);

  result=hashtable_search(tactical_hash,callsign);

  if (result)
  {
//            fprintf(stderr,"\t\tFound it, %s, len=%d, %s\n",
//                callsign,
//                strlen(callsign),
//                result);
  }
  else
  {
//            fprintf(stderr,"\t\tNot found, %s, len=%d\n",
//                callsign,
//                strlen(callsign));
  }

  return (result);
}





// This function checks whether there already is something in the
// hashtable that matches.  If a match found, it overwrites the
// tactical call for that entry, else it inserts a new record.
//
void add_tactical_to_hash(char *callsign, char *tactical_call)
{
  char *temp1;    // tac-call
  char *temp2;    // callsign
  char *ptr;


  // Note that tactical_call can be '\0', which means we're
  // getting rid of a previous tactical call.
  //
  if (callsign == NULL || *callsign == '\0'
      || tactical_call == NULL)
  {
    return;
  }

  if (!tactical_hash)    // no table to add to
  {
//fprintf(stderr,"init_tactical_hash\n");
    init_tactical_hash(1); // so create one
  }

  // Remove any matching entry to avoid duplicates
  ptr = hashtable_remove(tactical_hash, callsign);
  if (ptr)    // If value found, free the storage space for it as
  {
    // the hashtable_remove function doesn't.  It does
    // however remove the key (callsign) ok.
    free(ptr);
  }

  temp1 = (char *)malloc(MAX_TACTICAL_CALL+1);
  CHECKMALLOC(temp1);

  temp2 = (char *)malloc(MAX_CALLSIGN+1);
  CHECKMALLOC(temp2);

//fprintf(stderr, "\t\t\tAdding %s = %s...\n", callsign, tactical_call);

  xastir_snprintf(temp2,
                  MAX_CALLSIGN+1,
                  "%s",
                  callsign);

  xastir_snprintf(temp1,
                  MAX_TACTICAL_CALL+1,
                  "%s",
                  tactical_call);

  //                                   (key)  (value)
  //                         hash      call   tac-call
  if (!hashtable_insert(tactical_hash, temp2, temp1))
  {
    fprintf(stderr,"Insert failed on tactical hash --- fatal\n");
    free(temp1);
    free(temp2);
    exit(1);
  }

  // A check to see whether hash insert/update worked properly
  ptr = get_tactical_from_hash(callsign);
  if (!ptr)
  {
    fprintf(stderr,"***Failed hash insert/update***\n");
  }
  else
  {
//fprintf(stderr,"Current: %s -> %s\n",
//    callsign,
//    ptr);
  }
}





void destroy_tactical_hash(void)
{
  struct hashtable_itr *iterator = NULL;
  char *value;

  if (tactical_hash && hashtable_count(tactical_hash) > 0)
  {

    iterator = hashtable_iterator(tactical_hash);

    do
    {
      if (iterator)
      {
        value = hashtable_iterator_value(iterator);
        if (value)
        {
          free(value);
        }
      }
    }
    while (hashtable_iterator_remove(iterator));

    // Destroy the hashtable, freeing what's left of the
    // entries.
    hashtable_destroy(tactical_hash, 1);

    tactical_hash = NULL;

    if (iterator)
    {
      free(iterator);
    }
  }
}





// Prints string to STDERR only if "my_debug_level" bits are set in
// the global "debug_level" variable.  Used for getting extra debug
// messages during various stages of debugging.
//
void xastir_debug(int my_debug_level, char *debug_string)
{

  if (debug_level & my_debug_level)
  {
    fprintf(stderr, "%s", debug_string);
  }
}





char *remove_all_spaces(char *data)
{
  char *ptr;
  int length = strlen(data);

  ptr = data;
  while ( (ptr = strpbrk(data, " ")) )
  {
    memmove(ptr, ptr+1, strlen(ptr)+1);
    length--;
  }

  // Terminate at the new string length
  data[length] = '\0';

  return(data);
}





char *remove_leading_spaces(char *data)
{
  int i,j;
  int count;

  if (data == NULL)
  {
    return NULL;
  }

  if (strlen(data) == 0)
  {
    return NULL;
  }

  count = 0;
  // Count the leading space characters
  for (i = 0; i < (int)strlen(data); i++)
  {
    if (data[i] == ' ')
    {
      count++;
    }
    else    // Found a non-space
    {
      break;
    }
  }

  // Check whether entire string was spaces
  if (count == (int)strlen(data))
  {
    // Empty the string
    data[0] = '\0';
  }
  else if (count > 0)    // Found some spaces
  {
    i = 0;
    for( j = count; j < (int)strlen(data); j++ )
    {
      data[i++] = data[j];    // Move string left
    }
    data[i] = '\0'; // Terminate the new string
  }

  return(data);
}





char *remove_trailing_spaces(char *data)
{
  int datalen;

  if (data != NULL)
  {

    datalen=strlen(data);
    for(datalen--; datalen>=0; datalen--)
      if(data[datalen] == ' ')
      {
        data[datalen] = '\0';
      }
      else
      {
        break;
      }
  }

  // May end up with nothing left.
  return(data);
}





char *remove_trailing_asterisk(char *data)
{
  int datalen;

  if (data != NULL)
  {

    datalen=strlen(data);
    for(datalen--; datalen>0; datalen--)
    {
      if(data[datalen] == '*')
      {
        data[datalen] = '\0';
      }
      else
      {
        break;
      }
    }
  }
  // May end up with nothing left.
  return(data);
}





// Removes trailing "-0" from string.
//
// Modifies "data" variable.
//
char *remove_trailing_dash_zero(char *data)
{
  char *ptr;
  char *ptr2;
  int len = strlen(data);


  // String too short?
  if (len < 2)
  {
    return(data);
  }

  ptr2 = data + len - 1; // Point to last char
  ptr = ptr2 - 1; // Point to next-to-last char

  // Check for "-0" at end.  Remove if found.
  if (*ptr == '-' && *ptr2 == '0')
  {
    *ptr = '\0';    // Terminate
  }

  return(data);
}





// Save the current time, used for timing code sections.
void start_timer(void)
{
  gettimeofday(&timer_start,&tz);
}





// Save the current time, used for timing code sections.
void stop_timer(void)
{
  gettimeofday(&timer_stop,&tz);
}





// Print the difference in the two times saved above.
void print_timer_results(void)
{
  fprintf(stderr,"Total: %f sec\n",
          (float)(timer_stop.tv_sec - timer_start.tv_sec +
                  ((timer_stop.tv_usec - timer_start.tv_usec) / 1000000.0) ));
}





//
// Inserts localtime date/time in "timestring".  Timestring
// Should be at least 101 characters long.
//
void get_timestamp(char *timestring)
{
  struct tm *time_now;
  time_t secs_now;


  secs_now=sec_now();
  time_now = localtime(&secs_now);

// %e is not implemented on all systems, but %d should be
//    (void)strftime(timestring,100,"%a %b %e %H:%M:%S %Z %Y",time_now);
  (void)strftime(timestring,100,"%a %b %d %H:%M:%S %Z %Y",time_now);
}





/* function get_iso_datetime converts time in seconds to an ISO date
 * time in the form yyyy-mm-dd hh:mm:ss utc_offset
 * @param aTime time in seconds since the begining of the unix epoch
 * @param timestring pointer to a char[101] into which the timestamp
 * is written in the format yyyy-mm-dd hh:mm:ss followed by a utc
 * offset for the timezone.
 * @param nowIfNotSet when true, if aTime is not set (int)aTime==0,
 * then return the current time rather than formatting aTime, when
 * false, returns formatted aTime even if it was zero.
 * @param nowIfInvalid when true, if aTime is invalid (int)aTime=-1,
 * then returns the formatted current time rather than formatting
 * aTime, otherwise returns formatted invalid time.
 * @returns 0 on when (int)aTime==-1 where time provided invalid
 * returns 1 otherwise. */
int get_iso_datetime(time_t aTime, char *timestring,int nowIfNotSet, int nowIfInvalid)
{
  struct tm *time_now;
  time_t secs_now;
  int returnvalue = 1;
  if (((int)aTime==0 && nowIfNotSet) || ((int)aTime==-1 && nowIfInvalid))
  {
    secs_now=sec_now();
    time_now = localtime(&secs_now);
    (void)strftime(timestring,100,"%F %H:%M:%S %z",time_now);
  }
  else
  {
    // will also end up here if time_t is -1
    (void)strftime(timestring,100,"%F %H:%M:%S %z",localtime(&aTime));
  }
  if ((int)aTime==-1)
  {
    returnvalue = 0;
  }
  return returnvalue;
}






/* function get_W3CDTF_datetime converts time in seconds to a W3CDTF date
 * time in the form yyyy-mm-ddThh:mm:ss-utc_offsethh:mm
 * See: http://www.w3.org/TR/NOTE-datetime
 * This is effectively an ISO date, with a T between the date and the
 * time, no space between the time and the utc offset, and a colon
 * separating utc offset hours from utc offset seconds.
 * Example:  2008-01-25T08:35:20-05:00
 * @param aTime time in seconds since the begining of the unix epoch
 * @param timestring pointer to a char[101] into which the timestamp
 * is written in the format yyyy-mm-ddThh:mm:ss followed by a utc
 * offset for the timezone, (e.g. -05:00).
 * @param nowIfNotSet when true, if aTime is not set (int)aTime==0,
 * then return the current time rather than formatting aTime, when
 * false, returns formatted aTime even if it was zero.
 * @param nowIfInvalid when true, if aTime is invalid (int)aTime=-1,
 * then returns the formatted current time rather than formatting
 * aTime, otherwise returns formatted invalid time.
 * @returns 0 on when (int)aTime==-1 where time provided invalid
 * returns 1 otherwise. */
int get_w3cdtf_datetime(time_t aTime, char *timestring,int nowIfNotSet, int nowIfInvalid)
{
  struct tm *time_now;
  time_t secs_now;
  int returnvalue = 1;
  if (((int)aTime==0 && nowIfNotSet) || ((int)aTime==-1 && nowIfInvalid))
  {
    secs_now=sec_now();
    time_now = localtime(&secs_now);
    (void)strftime(timestring,100,"%FT%H:%M:%S %z",time_now);
    timestring[19] = timestring[20];
    timestring[20] = timestring[21];
    timestring[21] = timestring[22];
    timestring[22] = ':';
  }
  else
  {
    // will also end up here if time_t is -1
    (void)strftime(timestring,100,"%FT%H:%M:%S %z",localtime(&aTime));
    timestring[19] = timestring[20];
    timestring[20] = timestring[21];
    timestring[21] = timestring[22];
    timestring[22] = ':';
  }
  if ((int)aTime==-1)
  {
    returnvalue = 0;
  }
  return returnvalue;
}




/***********************************************************/
/* returns the hour (00..23), localtime                    */
/***********************************************************/
int get_hours(void)
{
  struct tm *time_now;
  time_t secs_now;
  char shour[5];

  secs_now=sec_now();
  time_now = localtime(&secs_now);
  (void)strftime(shour,4,"%H",time_now);
  return(atoi(shour));
}





/***********************************************************/
/* returns the minute (00..59), localtime                  */
/***********************************************************/
int get_minutes(void)
{
  struct tm *time_now;
  time_t secs_now;
  char sminute[5];

  secs_now=sec_now();
  time_now = localtime(&secs_now);
  (void)strftime(sminute,4,"%M",time_now);
  return(atoi(sminute));
}





/***********************************************************/
/* returns the second (00..61), localtime                  */
/***********************************************************/
int get_seconds(void)
{
  struct tm *time_now;
  time_t secs_now;
  char sminute[5];

  secs_now=sec_now();
  time_now = localtime(&secs_now);
  (void)strftime(sminute,4,"%S",time_now);
  return(atoi(sminute));
}






/*************************************************************************/
/* output_lat - format position with position_amb_chars for transmission */
/*************************************************************************/
char *output_lat(char *in_lat, int comp_pos)
{
  int i,j;

//fprintf(stderr,"in_lat:%s\n", in_lat);

  if (!comp_pos)
  {
    // Don't do this as it results in truncation!
    //in_lat[7]=in_lat[8]; // Shift N/S down for transmission
  }
  else if (position_amb_chars>0)
  {
    in_lat[7]='0';
  }

  j=0;
  if (position_amb_chars>0 && position_amb_chars<5)
  {
    for (i=6; i>(6-position_amb_chars-j); i--)
    {
      if (i==4)
      {
        i--;
        j=1;
      }
      if (!comp_pos)
      {
        in_lat[i]=' ';
      }
      else
      {
        in_lat[i]='0';
      }
    }
  }

  if (!comp_pos)
  {
    in_lat[8] = '\0';
  }

  return(in_lat);
}





/**************************************************************************/
/* output_long - format position with position_amb_chars for transmission */
/**************************************************************************/
char *output_long(char *in_long, int comp_pos)
{
  int i,j;

//fprintf(stderr,"in_long:%s\n", in_long);

  if (!comp_pos)
  {
    // Don't do this as it results in truncation!
    //in_long[8]=in_long[9]; // Shift e/w down for transmission
  }
  else if (position_amb_chars>0)
  {
    in_long[8]='0';
  }

  j=0;
  if (position_amb_chars>0 && position_amb_chars<5)
  {
    for (i=7; i>(7-position_amb_chars-j); i--)
    {
      if (i==5)
      {
        i--;
        j=1;
      }
      if (!comp_pos)
      {
        in_long[i]=' ';
      }
      else
      {
        in_long[i]='0';
      }
    }
  }

  if (!comp_pos)
  {
    in_long[9] = '\0';
  }

  return(in_long);
}





/*********************************************************************/
/* PHG range calculation                                             */
/* NOTE: Keep these calculations consistent with phg_decode!         */
/*       Yes, there is a reason why they both exist.                 */
/*********************************************************************/
double phg_range(char p, char h, char g)
{
  double power, height, gain, range;

  if ( (p < '0') || (p > '9') )   // Power is outside limits
  {
    power = 0.0;
  }
  else
  {
    power = (double)( (p-'0')*(p-'0') );  // lclint said: "Assignment of char to double" here
  }

  if (h < '0')   // Height is outside limits (there is no upper limit according to the spec)
  {
    height = 10.0;
  }
  else
  {
    height = 10.0 * pow(2.0, (double)(h-'0'));
  }

  if ( (g < '0') || (g > '9') )   // Gain is outside limits
  {
    gain = 1.0;
  }
  else
  {
    gain = pow(10.0, (double)(g-'0') / 10.0);
  }

  range = sqrt(2.0 * height * sqrt((power / 10.0) * (gain / 2.0)));

//    if (range > 70.0)
//        fprintf(stderr,"PHG%c%c%c results in range of %f\n", p, h, g, range);

  // Note:  Bob Bruninga, WB4APR, decided to cut PHG circles by
  // 1/2 in order to show more realistic mobile ranges.
  range = range / 2.0;

  return(range);
}





/*********************************************************************/
/* shg range calculation (for DF'ing)                                */
/*                                                                   */
/*********************************************************************/
double shg_range(char s, char h, char g)
{
  double power, height, gain, range;

  if ( (s < '0') || (s > '9') )   // Power is outside limits
  {
    s = '0';
  }

  if (s == '0')               // No signal strength
  {
    power = 10.0 / 0.8;  // Preventing divide by zero (same as DOSaprs does it)
  }
  else
  {
    power = 10 / (s - '0');  // Makes circle smaller with higher signal strengths
  }

  if (h < '0')   // Height is outside limits (there is no upper limit according to the spec)
  {
    height = 10.0;
  }
  else
  {
    height = 10.0 * pow(2.0, (double)(h-'0'));
  }

  if ( (g < '0') || (g > '9') )   // Gain is outside limits
  {
    gain = 1.0;
  }
  else
  {
    gain = pow(10.0, (double)(g-'0') / 10.0);
  }

  range = sqrt(2.0 * height * sqrt((power / 10.0) * (gain / 2.0)));

  range = (range * 40) / 51;   // Present fudge factors used in DOSaprs

  //fprintf(stderr,"SHG%c%c%c results in range of %f\n", s, h, g, range);

  return(range);
}





/*********************************************************************/
/* PHG decode                                                        */
/* NOTE: Keep these calculations consistent with phg_range!          */
/*       Yes, there is a reason why they both exist.                 */
/*********************************************************************/
void phg_decode(const char *langstr, const char *phg, char *phg_decoded, int phg_decoded_length)
{
  double power, height, gain, range;
  char directivity[6], temp[64];
  int  gain_db;


  if (strlen(phg) != 7)
  {
    xastir_snprintf(phg_decoded,
                    phg_decoded_length,
                    "%s %s",
                    langstr,
                    langcode("WPUPSTI073") ); // "BAD PHG"
    return;
  }

  if ( (phg[3] < '0') || (phg[3] > '9') )   // Power is outside limits
  {
    power = 0.0;
  }
  else
  {
    power = (double)( (phg[3]-'0')*(phg[3]-'0') );
  }

  if (phg[4] < '0')   // Height is outside limits (there is no upper limit according to the spec)
  {
    height = 10.0;
  }
  else
  {
    height = 10.0 * pow(2.0, (double)(phg[4]-'0'));
  }

  if ( (phg[5] < '0') || (phg[5] > '9') )   // Gain is outside limits
  {
    gain = 1.0;
    gain_db = 0;
  }
  else
  {
    gain = pow(10.0, (double)(phg[5]-'0') / 10.0);
    gain_db = phg[5]-'0';
  }

  range = sqrt(2.0 * height * sqrt((power / 10.0) * (gain / 2.0)));
  // Note:  Bob Bruninga, WB4APR, decided to cut PHG circles by
  // 1/2 in order to show more realistic mobile ranges.
  range = range / 2.0;

  switch (phg[6])
  {
    case '0':
      xastir_snprintf(directivity,
                      sizeof(directivity),
                      "%s",
                      langcode("WPUPSTI071") );   // "omni"
      break;
    case '1':
      xastir_snprintf(directivity,
                      sizeof(directivity),
                      "%s",
                      " NE");
      break;
    case '2':
      xastir_snprintf(directivity,
                      sizeof(directivity),
                      "%s",
                      " E");
      break;
    case '3':
      xastir_snprintf(directivity,
                      sizeof(directivity),
                      "%s",
                      " SE");
      break;
    case '4':
      xastir_snprintf(directivity,
                      sizeof(directivity),
                      "%s",
                      " S");
      break;
    case '5':
      xastir_snprintf(directivity,
                      sizeof(directivity),
                      "%s",
                      " SW");
      break;
    case '6':
      xastir_snprintf(directivity,
                      sizeof(directivity),
                      "%s",
                      " W");
      break;
    case '7':
      xastir_snprintf(directivity,
                      sizeof(directivity),
                      "%s",
                      " NW");
      break;
    case '8':
      xastir_snprintf(directivity,
                      sizeof(directivity),
                      "%s",
                      " N");
      break;
    default:
      directivity[0] = '\0';
      break;
  }

  if (english_units)
    xastir_snprintf(temp,
                    sizeof(temp),
                    "%.0fW @ %.0fft %s, %ddB%s, %s %.1fmi",
                    power,
                    height,
                    langcode("WPUPSTI070"), // HAAT
                    gain_db,
                    directivity,
                    langcode("WPUPSTI072"), // range
                    range);
  else
    xastir_snprintf(temp,
                    sizeof(temp),
                    "%.0fW @ %.1fm %s, %ddB%s, %s %.1fkm",
                    power,
                    height*0.3048,
                    langcode("WPUPSTI070"), // HAAT
                    gain_db,
                    directivity,
                    langcode("WPUPSTI072"), // range
                    range*1.609344);

  xastir_snprintf(phg_decoded,
                  phg_decoded_length,
                  "%s %s",
                  langstr,
                  temp);
}





/*********************************************************************/
/* SHG decode                                                        */
/*                                                                   */
/*********************************************************************/
void shg_decode(const char *langstr, const char *shg, char *shg_decoded, int shg_decoded_length)
{
  double power, height, gain, range;
  char directivity[6], temp[100], signal[64];
  int  gain_db;
  char s;

  if (strlen(shg) != 7)
  {
    xastir_snprintf(shg_decoded,
                    shg_decoded_length,
                    langstr,
                    langcode("WPUPSTI074") );   // "BAD SHG"
    return;
  }

  s = shg[3];

  if ( (s < '0') || (s > '9') )   // Signal is outside limits
  {
    s = '0';  // force to lowest legal value
  }

  switch (s)
  {
    case '0':
      xastir_snprintf(signal,
                      sizeof(signal),
                      "%s",
                      langcode("WPUPSTI076") );
      // "No signal detected"
      break;
    case '1':
      xastir_snprintf(signal,
                      sizeof(signal),
                      "%s",
                      langcode("WPUPSTI077") );
      // "Detectible signal (Maybe)"
      break;
    case '2':
      xastir_snprintf(signal,
                      sizeof(signal),
                      "%s",
                      langcode("WPUPSTI078") );
      // "Detectible signal but not copyable)"
      break;
    case '3':
      xastir_snprintf(signal,
                      sizeof(signal),
                      "%s",
                      langcode("WPUPSTI079") );
      // "Weak signal, marginally readable"
      break;
    case '4':
      xastir_snprintf(signal,
                      sizeof(signal),
                      "%s",
                      langcode("WPUPSTI080") );
      // "Noisy but copyable signal"
      break;
    case '5':
      xastir_snprintf(signal,
                      sizeof(signal),
                      "%s",
                      langcode("WPUPSTI081") );
      // "Some noise, easy to copy signal"
      break;
    case '6':
      xastir_snprintf(signal,
                      sizeof(signal),
                      "%s",
                      langcode("WPUPSTI082") );
      // "Good signal w/detectible noise"
      break;
    case '7':
      xastir_snprintf(signal,
                      sizeof(signal),
                      "%s",
                      langcode("WPUPSTI083") );
      // "Near full-quieting signal"
      break;
    case '8':
      xastir_snprintf(signal,
                      sizeof(signal),
                      "%s",
                      langcode("WPUPSTI084") );
      // "Full-quieting signal"
      break;
    case '9':
      xastir_snprintf(signal,
                      sizeof(signal),
                      "%s",
                      langcode("WPUPSTI085") );
      // "Extremely strong & full-quieting signal"
      break;
    default:
      signal[0] = '\0';
      break;
  }

  if (s == '0')
  {
    power = (double)( 10 / 0.8 );  // Preventing divide by zero (same as DOSaprs does it)
  }
  else
  {
    power = (double)( 10 / (s - '0') );
  }

  if (shg[4] < '0')   // Height is outside limits (there is no upper limit according to the spec)
  {
    height = 10.0;
  }
  else
  {
    height = 10.0 * pow(2.0, (double)(shg[4]-'0'));
  }

  if ( (shg[5] < '0') || (shg[5] > '9') )   // Gain is outside limits
  {
    gain = 1.0;
    gain_db = 0;
  }
  else
  {
    gain = pow(10.0, (double)(shg[5]-'0') / 10.0);
    gain_db = shg[5]-'0';
  }

  range = sqrt(2.0 * height * sqrt((power / 10.0) * (gain / 2.0)));

  range = range * 0.85;   // Present fudge factor (used by DOSaprs)

  switch (shg[6])
  {
    case '0':
      xastir_snprintf(directivity,
                      sizeof(directivity),
                      "%s ",
                      langcode("WPUPSTI071") );   // "omni"
      break;
    case '1':
      xastir_snprintf(directivity,
                      sizeof(directivity),
                      "%s",
                      " NE");
      break;
    case '2':
      xastir_snprintf(directivity,
                      sizeof(directivity),
                      "%s",
                      " E");
      break;
    case '3':
      xastir_snprintf(directivity,
                      sizeof(directivity),
                      "%s",
                      " SE");
      break;
    case '4':
      xastir_snprintf(directivity,
                      sizeof(directivity),
                      "%s",
                      " S");
      break;
    case '5':
      xastir_snprintf(directivity,
                      sizeof(directivity),
                      "%s",
                      " SW");
      break;
    case '6':
      xastir_snprintf(directivity,
                      sizeof(directivity),
                      "%s",
                      " W");
      break;
    case '7':
      xastir_snprintf(directivity,
                      sizeof(directivity),
                      "%s",
                      " NW");
      break;
    case '8':
      xastir_snprintf(directivity,
                      sizeof(directivity),
                      "%s",
                      " N");
      break;
    default:
      directivity[0] = '\0';
      break;
  }

  if (english_units)
  {
    xastir_snprintf(temp,
                    sizeof(temp),
                    "%.0fft %s, %ddB%s, %s: %.1fmi, %s",
                    height,
                    langcode("WPUPSTI070"), // "HAAT"
                    gain_db,
                    directivity,
                    langcode("WPUPSTI075"), // "DF Range"
                    range,
                    signal);
  }
  else
  {
    xastir_snprintf(temp,
                    sizeof(temp),
                    "%.1fm %s, %ddB%s, %s: %.1fkm, %s",
                    height*0.3048,
                    langcode("WPUPSTI070"), // "HAAT"
                    gain_db,
                    directivity,
                    langcode("WPUPSTI075"), // "DF Range"
                    range*1.609344,
                    signal);
  }

  xastir_snprintf(shg_decoded,
                  shg_decoded_length,
                  langstr,
                  temp);
}





/*********************************************************************/
/* Bearing decode                                                    */
/*                                                                   */
/*********************************************************************/
void bearing_decode(const char *langstr, const char *bearing_str,
                    const char *NRQ, char *bearing_decoded, int bearing_decoded_length)
{
  int bearing, range, width;
  char N,R,Q;
  char temp[64];


//fprintf(stderr,"bearing_decode incoming: bearing is %s, NRQ is %s\n", bearing_str, NRQ);

  if (strlen(bearing_str) != 3)
  {
    xastir_snprintf(bearing_decoded,
                    bearing_decoded_length,
                    langstr,
                    langcode("WPUPSTI086") );   // "BAD BEARING"
    return;
  }

  if (strlen(NRQ) != 3)
  {
    xastir_snprintf(bearing_decoded,
                    bearing_decoded_length,
                    langstr,
                    langcode("WPUPSTI087") );   // "BAD NRQ"
    return;
  }

  bearing = atoi(bearing_str);
  N = NRQ[0];
  R = NRQ[1];
  Q = NRQ[2];

  range = 0;
  width = 0;

  if (N != 0)
  {

//fprintf(stderr,"N != 0\n");

    range = pow(2.0,R - '0');

    switch (Q)
    {
      case('1'):
        width = 240;    // Degrees of beam width.  What's the point?
        break;
      case('2'):
        width = 120;    // Degrees of beam width.  What's the point?
        break;
      case('3'):
        width = 64; // Degrees of beam width.  What's the point?
        break;
      case('4'):
        width = 32; // Degrees of beam width.  Starting to be usable.
        break;
      case('5'):
        width = 16; // Degrees of beam width.  Usable.
        break;
      case('6'):
        width = 8;  // Degrees of beam width.  Usable.
        break;
      case('7'):
        width = 4;  // Degrees of beam width.  Nice!
        break;
      case('8'):
        width = 2;  // Degrees of beam width.  Nice!
        break;
      case('9'):
        width = 1;  // Degrees of beam width.  Very Nice!
        break;
      default:
        width = 8;  // Degrees of beam width
        break;
    }

//fprintf(stderr,"Width = %d\n",width);

    if (english_units)
    {
      xastir_snprintf(temp,
                      sizeof(temp),
                      "%i%c, %s: %i%c, %s: %i mi",
                      bearing,
                      0xb0,                   // Degree symbol
                      langcode("WPUPSTI088"), // DF Beamwidth
                      width,
                      0xb0,                   // Degree symbol
                      langcode("WPUPSTI089"), // DF Length
                      range);
    }
    else
    {
      xastir_snprintf(temp,
                      sizeof(temp),
                      "%i%c, %s: %i%c, %s: %0.2f km",
                      bearing,
                      0xb0,                   // Degree symbol
                      langcode("WPUPSTI088"), // DF Beamwidth
                      width,
                      0xb0,                   // Degree symbol
                      langcode("WPUPSTI089"), // DF Length
                      range*1.609344);
    }
  }
  else
  {
    xastir_snprintf(temp,
                    sizeof(temp),
                    "%s",
                    langcode("WPUPSTI090") );   // "Not Valid"

//fprintf(stderr,"N was 0\n");
  }
  xastir_snprintf(bearing_decoded,
                  bearing_decoded_length,
                  langstr,
                  temp);

//fprintf(stderr,"bearing_decoded:%s\n", bearing_decoded);
//fprintf(stderr,"temp:%s\n", temp);
}





//********************************************************************/
// get_line - read a line from a file                                */
//
// NOTE: This routine will not work for binary files.  It works only
// for ASCII-format files, and terminates each line at the first
// control-character found (unless it's a tab).  Converts tab
// character to 1 space character.
//
//********************************************************************/
/*
char *get_line(FILE *f, char *linedata, int maxline) {
    char temp_line[32768];
    int i;

    // Snag one string from the file.  We'll end up with a
    // terminating zero at temp_line[32767] in any case, because the
    // max quantity we'll get here will be 32767 with a terminating
    // zero added after whatever quantity is read.
    (void)fgets(temp_line, 32768, f);

    // A newline may have been added by the above fgets call.
    // Change any newlines or other control characters to line-end
    // characters.
    for (i = 0; i < strlen(temp_line); i++) {
        // Change any control characters to '\0';
        if (temp_line[i] < 0x20) {
            if (temp_line[i] == '\t') { // Found a tab char
                temp_line[i] = ' '; // Convert to a space char
            }
            else {  // Not a tab
                if (temp_line[i] != '\n') {                         // LF
                    if ( (i != (strlen(temp_line) - 2) )            // CRLF
                            && (i != (strlen(temp_line) - 1) ) ) {  // CR
                        fprintf(stderr,"get_line: found control chars in: %s\n",
                            temp_line);
                    }
                }
                temp_line[i] = '\0';    // Terminate the string
            }
        }
    }

    xastir_snprintf(linedata,
        maxline,
        "%s",
        temp_line);

    return(linedata);
}
*/
char *get_line(FILE *f, char *linedata, int maxline)
{
  int length;

  // Write terminating chars throughout variable
  memset(linedata,0,maxline);

  // Get the data
  if (fgets(linedata, maxline, f) == 0)
  {
    return "\0";  // Couldn't read from file: Return empty string
  }

  // Change CR/LF to '\0'
  length = strlen(linedata);

  // Check the last two characters
  if (length > 0)
  {
    if ( (linedata[length - 1] == '\n')
         || (linedata[length - 1] == '\r') )
    {
      linedata[length - 1] = '\0';
    }
  }

  if (length > 1)
  {
    if ( (linedata[length - 2] == '\n')
         || (linedata[length - 2] == '\r') )
    {
      linedata[length - 2] = '\0';
    }
  }

  return(linedata);
}





// time_from_aprsstring()
//
// Called from alert.c:alert_build_list() only.  Converts to
// localtime if it's a zulu time string we're parsing.
//
time_t time_from_aprsstring(char *aprs_time)
{
  int day, hour, minute;
  char tz[20];
  struct tm *time_now, alert_time;
  time_t timenw;
  long zone;

#ifndef HAVE_TM_GMTOFF
#ifdef __CYGWIN__
  // Use "_timezone" instead of timezone in Cygwin
#define timezone _timezone
#else // __CYGWIN__
  extern time_t timezone;
#endif  // __CYGWIN__
#endif  // HAVE_TM_GMTOFF


#ifdef __CYGWIN__
  // Must call tzset before using the _timezone variable in
  // Cygwin, else the timezone won't have been initialized.
  tzset();
#endif  // __CYGWIN__

  // Compute our current time and the offset from GMT.  If
  // daylight savings time is in effect, factor that in as well.
  (void)time(&timenw);
  time_now = localtime(&timenw);
#ifdef HAVE_TM_GMTOFF
  // tm_gmtoff is the GMT offset in seconds.  Some Unix systems
  // have this extra field in the tm struct, some don't.
  // tm_gmtoff is seconds EAST of UTC.
  zone = time_now->tm_gmtoff;
  //fprintf(stderr,"gmtoff: %ld, tm_isdst: %d\n",
  //time_now->tm_gmtoff,
  //time_now->tm_isdst);
#else   // HAVE_TM_GMTOFF
  // Note:  timezone is seconds WEST of UTC.  Need to negate
  // timezone to have the offset occur in the correct direction.
  zone = -((int)timezone - 3600 * (int)(time_now->tm_isdst > 0));
  //fprintf(stderr,"timezone: %d, tm_isdst: %d\n",
  //timezone,
  //time_now->tm_isdst);
#endif  // HAVE_TM_GMTOFF
  // zone should now be the number to subtract in order to get
  // localtime, in seconds.  For PST, I get -28800 which equals -8
  // hours.  Summertime I should get -25200, or -7 hours.
  //fprintf(stderr,"Zone: %ld\n",zone);


  // Split the input time string into its component parts.
  tz[0] = tz[1] = '\0';
  switch (sscanf(aprs_time, "%2d%2d%2d%19s", &day, &hour, &minute, tz))
  {
    case 0:
      day = 0;
    /* Falls through. */

    case 1:
      hour = 0;
    /* Falls through. */

    case 2:
      minute = 0;
    /* Falls through. */

    default:
      break;
  }
  if (day > 31)
  {
    day = 31; // Wierd, can't have too many days in the month!
    hour = 23;
    minute = 59;
  }

  // We set up our alert_time so that it points into the same
  // struct as time_now.  We do this both so that we can get
  // automatically filled in pieces of the struct (year, etc), and
  // so that we have a more global struct to return the time in.
  // We'll have to adjust a few things like month/year if the time
  // is too far distant from our current time and crosses some
  // boundary.
  alert_time = *time_now;
  alert_time.tm_sec = 0;

  //fprintf(stderr,"alert_time: %d %d %d\n",
  //    alert_time.tm_mday,
  //    alert_time.tm_hour,
  //    alert_time.tm_min);

  // Check to see how many parameters we parsed, and do our
  // computations accordingly.
  if (day)    // If we parsed out the day
  {
    // Check whether our new day is more than ten days +/- from
    // the current day of the month.  If so, assume it was from
    // the month previous or after.
    if (day < (time_now->tm_mday - 10))
    {
      // Day of month went down drastically.  Must be a date
      // in the next month.  Bump up by one month and check
      // whether we overflowed into the next year.  Note that
      // month ranges from 0 to 11.
      if (++alert_time.tm_mon > 11)
      {
        alert_time.tm_mon = 0;
        alert_time.tm_year++;
      }
    }
    else if (day > (time_now->tm_mday + 10))
    {
      // Day of month went up drastically.  Must be a date
      // during last month.  Decrement by one month and check
      // whether we need to also decrement the year.
      if (--alert_time.tm_mon < 0)
      {
        alert_time.tm_mon = 11;
        alert_time.tm_year--;
      }
    }

    // Fill in the struct with our new values that we parsed.
    alert_time.tm_mday = day;
    alert_time.tm_min = minute;
    alert_time.tm_hour = hour;

    // Need to do conversions from zulu time?
    if ((char)tolower((int)tz[0]) == 'z')
    {
      // Yep, do the conversions.  Note that the zone variable
      // already has the sign set correctly to get the correct
      // time by using addition (PDT zone = -28800).

      // Initialize daylight savings time to 0 in this
      // instance, 'cuz we're starting with Zulu time and we
      // want the localtime conversion to change it correctly.
      // Zulu time has no daylight savings time offset.
      //
// WE7U:
// No, it gave us an offset of 6 hours from UTC when we set this to
// zero, 7 hours (correct) when we set it to one, during the summer.
// Hopefully it will give us an 8-hour offset during the wintertime
// but that remains to be seen...
//
// FYI:  We're in daylight savings time during the summer, when
// we're at a 7-hour offset.  Winter is actual time and a -8 hour
// offset.
//
// One on-line resource suggested setting it to -1 for automatic
// determination of DST.  This works too, during the summer.  Again,
// check during the wintertime too when we're at normal time.
      //
      alert_time.tm_isdst = -1;


      // Do the hour offset
      alert_time.tm_hour += zone/3600;

      // Now check whether we have any offsets left to do.
      zone %= 3600;
      if (zone)
      {
        alert_time.tm_min += zone/60;
      }

      // Now check whether we have any overflows.  According
      // to the "mktime()" man page, we probably don't need to
      // do this for overflow (It normalizes the time itself),
      // but I think we still need to for underflow.
//WE7U: Check this stuff carefully!
      if (alert_time.tm_min > 59)
      {
        alert_time.tm_hour++;
        alert_time.tm_min -= 60;
      }
      if (alert_time.tm_hour > 23)
      {
        alert_time.tm_mday++;
        alert_time.tm_hour -= 24;
        if (mktime(&alert_time) == -1)
        {
          alert_time.tm_mday = 1;
          alert_time.tm_mon++;
          if (mktime(&alert_time) == -1)
          {
            alert_time.tm_mon = 0;
            alert_time.tm_year++;
          }
        }
      }
      else if (alert_time.tm_hour < 0)
      {
        alert_time.tm_hour += 24;
        if (--alert_time.tm_mday <= 0)
        {
          if (--alert_time.tm_mon < 0)
          {
            alert_time.tm_year--;
            alert_time.tm_mon = 11;
            alert_time.tm_mday = 31;
          }
          else if (alert_time.tm_mon == 3 || alert_time.tm_mon == 5 ||
                   alert_time.tm_mon == 8 || alert_time.tm_mon == 10)
          {
            alert_time.tm_mday = 30;
          }
          else if (alert_time.tm_mon == 1)
          {
            alert_time.tm_mday = (alert_time.tm_year%4 == 0) ? 29: 28;
          }
          else
          {
            alert_time.tm_mday = 31;
          }
        }
      }
    }
  }
  else    // We didn't parse out the day from the input string.
  {

    // What's this all about???  Different format of APRS
    // time/date string?
    alert_time.tm_year--;
  }

  if ( debug_level & 2 )
  {
    time_t a_time,now_time,diff;

    fprintf(stderr,"\n Input: %s\n",aprs_time);

    fprintf(stderr,"Output: %02d%02d%02d\n\n",
            alert_time.tm_mday,
            alert_time.tm_hour,
            alert_time.tm_min);

    a_time = mktime(&alert_time);
    fprintf(stderr,"Alert: %ld\n", (long)a_time);

    now_time = sec_now();
    fprintf(stderr,"  Now: %ld\n", (long)now_time);

    diff = now_time - a_time;

    if (diff >= 0)
    {
      fprintf(stderr,"Expired by %ld minutes\n", (long)(diff/60) );
    }
    else
    {
      fprintf(stderr,"%ld minutes until expiration\n", (long)((-diff)/60) );
    }

    if (alert_time.tm_isdst > 0)
    {
      fprintf(stderr,"Daylight savings time is in effect\n");
    }
  }

  return(mktime(&alert_time));
}





// Note: last_speed should be in knots.
//
// Input format for lat/long is DDMM.MM or DDMM.MMM format, like:
// 4722.93N   12244.17W  or
// 4722.938N  12244.177W
//
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
  xastir_snprintf(temp_str,
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
  xastir_snprintf(temp_str,
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
  xastir_snprintf(pos,
                  sizeof(pos),
                  "%c%s%s%c%c%c%c",
                  group,
                  lat,
                  lon,
                  symbol,
                  c,
                  s,
                  t);

//fprintf(stderr,"New compressed pos: (%s)\n",pos);
  return pos;
}





/*
 * See if position is defined
 * 90°N 180°W (0,0 in internal coordinates) is our undefined position
 * 0N/0E is excluded from trails, could be excluded from map (#define ACCEPT_0N_0E)
 */

int position_defined(long lat, long lon, int UNUSED(strict))
{

  if (lat == 0l && lon == 0l)
  {
    return(0);  // undefined location
  }
#ifndef ACCEPT_0N_0E
  if (strict)
#endif  // ACCEPT_0N_0E
    if (lat == 90*60*60*100l && lon == 180*60*60*100l)      // 0N/0E
    {
      return(0);  // undefined location
    }
  return(1);
}





// Function to convert from screen (pixel) coordinates to the Xastir
// coordinate system.
//
void convert_screen_to_xastir_coordinates(int x,
    int y,
    long *lat,
    long *lon)
{

  *lon = (center_longitude - ((screen_width  * scale_x)/2) + (x * scale_x));
  *lat = (center_latitude  - ((screen_height * scale_y)/2) + (y * scale_y));

  if (*lon < 0)
  {
    *lon = 0l;  // 180°W
  }

  if (*lon > 129600000l)
  {
    *lon = 129600000l;  // 180°E
  }

  if (*lat < 0)
  {
    *lat = 0l;  //  90°N
  }

  if (*lat > 64800000l)
  {
    *lat = 64800000l;  //  90°S
  }
}





// Convert Xastir lat/lon to UTM printable string
void convert_xastir_to_UTM_str(char *str, int str_len, long x, long y)
{
  double utmNorthing;
  double utmEasting;
  char utmZone[10];

  ll_to_utm_ups(E_WGS_84,
                (double)(-((y - 32400000l )/360000.0)),
                (double)((x - 64800000l )/360000.0),
                &utmNorthing,
                &utmEasting,
                utmZone,
                sizeof(utmZone) );
  utmZone[9] = '\0';
  //fprintf(stderr,"%s %07.0f %07.0f\n", utmZone, utmEasting,
  //utmNorthing );
  xastir_snprintf(str,
                  str_len,
                  "%s %07.0f %07.0f",
                  utmZone,
                  utmEasting,
                  utmNorthing);
}




// To produce MGRS coordinates, we'll call ll_to_utm_ups()
// [?? Earlier text: "the above function" ??]
// then convert the result to the 2-letter digraph format used
// for MGRS.  The ll_to_utm_ups() function switches to the special
// irregular UTM zones for the areas near Svalbard and SW Norway
// if the "coordinate_system" variable is set to "USE_MGRS",
// so we'll be using the correct zone boundaries for MGRS if that
// variable is set when we make the call.
//
// If "nice_format" == 1, we add leading spaces plus spaces between
// the easting and northing in order to line up more nicely with the
// UTM output format.
//
// convert_xastir_to_MGRS_str is a wrapper around the components
// function below that returns the MGRS coordinate of x and y as a
// single MGRS string.
//
/* convert_xastir_to_MGRS_str_components returns each of the components
   of the MGRS string separately.
   Example MGRS string: 18T VK 66790 55998
   Parameters:
   utmZone          Returns the UTM zone: e.g. 18T
   utmZone_len      length of the utmZone char[]
   EastingL         Returns the first letter of the MGRS digraph: e.g. V
   EastingL_len     length of the EastingL char[]
   NorthingL        Returns the second letter of the MGRS digraph: e.g. K
   NorthingL_len    length of the NorthingL char[]
   int_utmEasting   returns the MGRS easting: e.g. 66790
   int_utmNorthing  returns the MGRS northing: e.g. 55998
   x                xastir x coordinate to obtain MGRS coordinate for.
   y                xastir x coordinate to obtain MGRS coordinate for.
   nice_format      1 for populate space_string with three spaces
                    0 to make space_string and empty string, see above.
   space_string     Returned string that can be used to make MGRS strings
                    allign more cleanly with UTM strings.
   space_string_len length of the space_string char[]
*/
void convert_xastir_to_MGRS_str_components(char *utmZone, int UNUSED(utmZone_len),
    char *EastingL, int EastingL_len,
    char *NorthingL, int NorthingL_len,
    unsigned int *int_utmEasting, unsigned int *int_utmNorthing,
    long x,  long y,
    int nice_format, char *space_string, int UNUSED(space_string_len) )
{

  double utmNorthing;
  double utmEasting;
  //char utmZone[10];
  int start;
  int my_east, my_north;
  //unsigned int int_utmEasting, int_utmNorthing;
  int UPS = 0;
  int North_UPS = 0;
  int coordinate_system_save = coordinate_system;
  //char space_string[4] = "   ";    // Three spaces

  // Set for correct zones
  coordinate_system = USE_MGRS;

  ll_to_utm_ups(E_WGS_84,
                (double)(-((y - 32400000l )/360000.0)),
                (double)((x - 64800000l )/360000.0),
                &utmNorthing,
                &utmEasting,
                utmZone,
                sizeof(utmZone) );
  utmZone[9] = '\0';

  // Restore it
  coordinate_system = coordinate_system_save;


  //fprintf(stderr,"%s %07.0f %07.0f\n", utmZone, utmEasting,
  //utmNorthing );
  //xastir_snprintf(str, str_len, "%s %07.0f %07.0f",
  //    utmZone, utmEasting, utmNorthing );


  // Convert the northing and easting values to the digraph letter
  // format.  Letters 'I' and 'O' are skipped for both eastings
  // and northings.  Letters 'W' through 'Z' are skipped for
  // northings.
  //
  // N/S letters alternate in a cycle of two.  Begins at the
  // equator with A and F in alternate zones.  Odd-numbered zones
  // use A-V, even-numbered zones use F-V, then A-V.
  //
  // E/W letters alternate in a cycle of three.  Each E/W zone
  // uses an 8-letter block.  Starts at A-H, then J-R, then S-Z,
  // then repeats every 18 degrees.
  //
  // N/S letters have a cycle of two, E/W letters have a cycle of
  // three.  The same lettering repeats after six zones (2,000,000
  // meter intervals).
  //
  // AA is at equator and 180W.

  // Easting:  Each zone covers 6 degrees.  Zone 1 = A-H, Zone 2 =
  // J-R, Zone 3 = S-Z, then repeat.  So, take zone number-1,
  // modulus 3, multiple that number by 8.  Modulus 24.  That's
  // our starting letter for the zone.  Take the easting number,
  // divide by 100,000, , add the starting number, compute modulus
  // 24, then use that index into our E_W array.
  //
  // Northing:  Figure out whether even/odd zone number.  Divide
  // by 100,000.  If even, add 5 (starts at 'F' if even).  Compute
  // modulus 20, then use that index into our N_S array.


  *int_utmEasting = (unsigned int)utmEasting;
  *int_utmNorthing = (unsigned int)utmNorthing;
  *int_utmEasting = *int_utmEasting % 100000;
  *int_utmNorthing = *int_utmNorthing % 100000;


  // Check for South Polar UPS area, set flags if found.
  if (   utmZone[0] == 'A'
         || utmZone[0] == 'B'
         || utmZone[1] == 'A'
         || utmZone[1] == 'B'
         || utmZone[2] == 'A'
         || utmZone[2] == 'B' )
  {
    // We're in the South Polar UPS area
    UPS++;
  }
  // Check for North Polar UPS area, set flags if found.
  else if (   utmZone[0] == 'Y'
              || utmZone[0] == 'Z'
              || utmZone[1] == 'Y'
              || utmZone[1] == 'Z'
              || utmZone[2] == 'Y'
              || utmZone[2] == 'Z')
  {
    // We're in the North Polar UPS area
    UPS++;
    North_UPS++;
  }
  else
  {
    // We're in the UTM area.  Set no flags.
  }


  if (UPS)    // Special processing for UPS area (A/B/Y/Z bands)
  {

    // Note: Zone number isn't used for UPS, but zone letter is.
    if (nice_format)    // Add two leading spaces
    {
      utmZone[2] = utmZone[0];
      utmZone[0] = ' ';
      utmZone[1] = ' ';
      utmZone[3] = '\0';
    }
    else
    {
      space_string[0] = '\0';
    }

    if (North_UPS)      // North polar UPS zone
    {
      char UPS_N_Easting[15]  = "RSTUXYZABCFGHJ";
      char UPS_N_Northing[15] = "ABCDEFGHJKLMNP";

      // Calculate the index into the 2-letter digraph arrays.
      my_east = (int)(utmEasting / 100000.0);
      my_east = my_east - 13;
      my_north = (int)(utmNorthing / 100000.0);
      my_north = my_north - 13;

      /*xastir_snprintf(str,
          str_len,
          "%s %c%c %05d %s%05d",
          utmZone,
          UPS_N_Easting[my_east],
          UPS_N_Northing[my_north],
          int_utmEasting,
          space_string,
          int_utmNorthing ); */
      xastir_snprintf(EastingL,EastingL_len,
                      "%c", UPS_N_Easting[my_east]);
      xastir_snprintf(NorthingL,NorthingL_len,
                      "%c", UPS_N_Northing[my_north]);
    }
    else    // South polar UPS zone
    {
      char UPS_S_Easting[25]  = "JKLPQRSTUXYZABCFGHJKLPQR";
      char UPS_S_Northing[25] = "ABCDEFGHJKLMNPQRSTUVWXYZ";

      // Calculate the index into the 2-letter digraph arrays.
      my_east = (int)(utmEasting / 100000.0);
      my_east = my_east - 8;
      my_north = (int)(utmNorthing / 100000.0);
      my_north = my_north - 8;

      /* xastir_snprintf(str,
          str_len,
          "%s %c%c %05d %s%05d",
          utmZone,
          UPS_S_Easting[my_east],
          UPS_S_Northing[my_north],
          int_utmEasting,
          space_string,
          int_utmNorthing ); */
      xastir_snprintf(EastingL,EastingL_len,
                      "%c", UPS_S_Easting[my_east]);
      xastir_snprintf(NorthingL,NorthingL_len,
                      "%c", UPS_S_Northing[my_north]);
    }
  }
  else    // UTM Area
  {
    char E_W[25] = "ABCDEFGHJKLMNPQRSTUVWXYZ";
    char N_S[21] = "ABCDEFGHJKLMNPQRSTUV";


    if (!nice_format)
    {
      space_string[0] = '\0';
    }


    // Calculate the indexes into the 2-letter digraph arrays.
    start = atoi(utmZone);
    start--;
    start = start % 3;
    start = start * 8;
    start = start % 24;
    // "start" is now an index into the starting letter for the
    // zone.


    my_east = (int)(utmEasting / 100000.0) - 1;
    my_east = my_east + start;
    my_east = my_east % 24;
//        fprintf(stderr, "Start: %c   East (guess): %c   ",
//            E_W[start],
//            E_W[my_east]);


    start = atoi(utmZone);
    start = start % 2;
    if (start)      // Odd-numbered zone
    {
      start = 0;
    }
    else    // Even-numbered zone
    {
      start = 5;
    }
    my_north = (int)(utmNorthing / 100000.0);
    my_north = my_north + start;
    my_north = my_north % 20;
//        fprintf(stderr, "Start: %c   North (guess): %c\n",
//            N_S[start],
//            N_S[my_north]);

    /* xastir_snprintf(str,
        str_len,
        "%s %c%c %05d %s%05d",
        utmZone,
        E_W[my_east],
        N_S[my_north],
        int_utmEasting,
        space_string,
        int_utmNorthing ); */
    xastir_snprintf(EastingL, EastingL_len,
                    "%c", E_W[my_east]);
    xastir_snprintf(NorthingL, NorthingL_len,
                    "%c", N_S[my_north]);
  }
}





/* Wrapper around convert_xastir_to_MGRS_str_components
   to return an MGRS coordinate as a single string.
   Parameters:
   str The character array to be populated with the MGRS string.
   str_len Length of str.
   x xastir x coordinate.
   y xastir y coordinate.
   If "nice_format" == 1, we add leading spaces plus spaces between
   the easting and northing in order to line up more nicely with the
   UTM output format.
*/
void convert_xastir_to_MGRS_str(char *str, int str_len, long x, long y, int nice_format)
{
  char space_string[4] = "   ";    // Three spaces
  unsigned int intEasting = 0;
  unsigned int intNorthing = 0;
  char EastingL[3] = "  ";
  char NorthingL[3] = "  ";
  char utmZone[10];
  convert_xastir_to_MGRS_str_components(utmZone, strlen(utmZone),
                                        EastingL, sizeof(EastingL),
                                        NorthingL, sizeof(NorthingL),
                                        &intEasting, &intNorthing,
                                        x,  y,
                                        nice_format, space_string, strlen(space_string)) ;
  xastir_snprintf(str,
                  str_len,
                  "%s %c%c %05d %s%05d",
                  utmZone,
                  EastingL[0],
                  NorthingL[0],
                  intEasting,
                  space_string,
                  intNorthing );
}





// Convert Xastir lat/lon to UTM
void convert_xastir_to_UTM(double *easting, double *northing, char *zone, int zone_len, long x, long y)
{

  ll_to_utm_ups(E_WGS_84,
                (double)(-((y - 32400000l )/360000.0)),
                (double)((x - 64800000l )/360000.0),
                northing,
                easting,
                zone,
                zone_len);
  zone[zone_len-1] = '\0';
}





// Convert UTM to Xastir lat/lon
void convert_UTM_to_xastir(double easting, double northing, char *zone, long *x, long *y)
{
  double lat, lon;

  utm_ups_to_ll(E_WGS_84,
                northing,
                easting,
                zone,
                &lat,
                &lon);

  // Reverse latitude to fit our coordinate system then convert to
  // Xastir units.
  *y = (long)(lat * -360000.0) + 32400000l;

  // Convert longitude to xastir units
  *x = (long)(lon *  360000.0) + 64800000l;
}




// convert latitude from long to string
// Input is in Xastir coordinate system
//
// CONVERT_LP_NOSP      = DDMM.MMN
// CONVERT_HP_NOSP      = DDMM.MMMN
// CONVERT_VHP_NOSP     = DDMM.MMMMN
// CONVERT_LP_NORMAL    = DD MM.MMN
// CONVERT_HP_NORMAL    = DD MM.MMMN
// CONVERT_UP_TRK       = NDD MM.MMMM
// CONVERT_DEC_DEG      = DD.DDDDDN
// CONVERT_DMS_NORMAL   = DD MM SS.SN
// CONVERT_DMS_NORMAL_FORMATED   = DD'MM'SS.SN
// CONVERT_HP_NORMAL_FORMATED   = DD'MM.MMMMN
//
void convert_lat_l2s(long lat, char *str, int str_len, int type)
{
  char ns;
  float deg, min, sec;
  int ideg, imin;
  long temp;


  str[0] = '\0';
  deg = (float)(lat - 32400000l) / 360000.0;

  // Switch to integer arithmetic to avoid floating-point
  // rounding errors.
  temp = (long)(deg * 100000);

  ns = 'S';
  if (temp <= 0)
  {
    ns = 'N';
    temp = labs(temp);
  }

  ideg = (int)temp / 100000;
  min = (temp % 100000) * 60.0 / 100000.0;

  // Again switch to integer arithmetic to avoid floating-point
  // rounding errors.
  temp = (long)(min * 1000);
  imin = (int)(temp / 1000);
  sec = (temp % 1000) * 60.0 / 1000.0;

  switch (type)
  {

    case(CONVERT_LP_NOSP): /* do low P w/no space */
      xastir_snprintf(str,
                      str_len,
                      "%02d%05.2f%c",
                      ideg,
//                min+0.001, // Correct possible unbiased rounding
                      min,
                      ns);
      break;

    case(CONVERT_LP_NORMAL): /* do low P normal */
      xastir_snprintf(str,
                      str_len,
                      "%02d %05.2f%c",
                      ideg,
//                min+0.001, // Correct possible unbiased rounding
                      min,
                      ns);
      break;

    case(CONVERT_HP_NOSP): /* do HP w/no space */
      xastir_snprintf(str,
                      str_len,
                      "%02d%06.3f%c",
                      ideg,
//                min+0.0001, // Correct possible unbiased rounding
                      min,
                      ns);
      break;

    case(CONVERT_VHP_NOSP): /* do Very HP w/no space */
      xastir_snprintf(str,
                      str_len,
                      "%02d%07.4f%c",
                      ideg,
//                min+0.00001, // Correct possible unbiased rounding
                      min,
                      ns);
      break;

    case(CONVERT_UP_TRK): /* for tracklog files */
      xastir_snprintf(str,
                      str_len,
                      "%c%02d %07.4f",
                      ns,
                      ideg,
//                min+0.00001); // Correct possible unbiased rounding
                      min);
      break;

    case(CONVERT_DEC_DEG):
      xastir_snprintf(str,
                      str_len,
                      "%08.5f%c",
//                (ideg+min/60.0)+0.000001, // Correct possible unbiased rounding
                      ideg+min/60.0,
                      ns);
      break;

    case(CONVERT_DMS_NORMAL):
      xastir_snprintf(str,
                      str_len,
                      "%02d %02d %04.1f%c",
                      ideg,
                      imin,
//                sec+0.01, // Correct possible unbiased rounding
                      sec,
                      ns);
      break;

    case(CONVERT_DMS_NORMAL_FORMATED):
      xastir_snprintf(str,
                      str_len,
                      "%02d%c%02d\'%04.1f%c",
                      ideg,
                      0xb0,       // Degree symbol
                      imin,
//                sec+0.01, // Correct possible unbiased rounding
                      sec,
                      ns);
      break;

    case(CONVERT_HP_NORMAL_FORMATED):
      xastir_snprintf(str,
                      str_len,
                      "%02d%c%06.3f%c",
                      ideg,
                      0xb0,         // Degree symbol
//                min+0.0001, // Correct possible unbiased roundin
                      min,
                      ns);
      break;

    case(CONVERT_HP_NORMAL):
    default: /* do HP normal */
      xastir_snprintf(str,
                      str_len,
                      "%02d %06.3f%c",
                      ideg,
//                min+0.0001, // Correct possible unbiased rounding
                      min,
                      ns);
      break;
  }
}





// convert longitude from long to string
// Input is in Xastir coordinate system
//
// CONVERT_LP_NOSP      = DDDMM.MME
// CONVERT_HP_NOSP      = DDDMM.MMME
// CONVERT_VHP_NOSP     = DDDMM.MMMME
// CONVERT_LP_NORMAL    = DDD MM.MME
// CONVERT_HP_NORMAL    = DDD MM.MMME
// CONVERT_UP_TRK       = EDDD MM.MMMM
// CONVERT_DEC_DEG      = DDD.DDDDDE
// CONVERT_DMS_NORMAL   = DDD MM SS.SN
// CONVERT_DMS_NORMAL_FORMATED   = DDD'MM'SS.SN
//
void convert_lon_l2s(long lon, char *str, int str_len, int type)
{
  char ew;
  float deg, min, sec;
  int ideg, imin;
  long temp;

  str[0] = '\0';
  deg = (float)(lon - 64800000l) / 360000.0;

  // Switch to integer arithmetic to avoid floating-point rounding
  // errors.
  temp = (long)(deg * 100000);

  ew = 'E';
  if (temp <= 0)
  {
    ew = 'W';
    temp = labs(temp);
  }

  ideg = (int)temp / 100000;
  min = (temp % 100000) * 60.0 / 100000.0;

  // Again switch to integer arithmetic to avoid floating-point
  // rounding errors.
  temp = (long)(min * 1000);
  imin = (int)(temp / 1000);
  sec = (temp % 1000) * 60.0 / 1000.0;

  switch(type)
  {

    case(CONVERT_LP_NOSP): /* do low P w/nospacel */
      xastir_snprintf(str,
                      str_len,
                      "%03d%05.2f%c",
                      ideg,
//                min+0.001, // Correct possible unbiased rounding
                      min,
                      ew);
      break;

    case(CONVERT_LP_NORMAL): /* do low P normal */
      xastir_snprintf(str,
                      str_len,
                      "%03d %05.2f%c",
                      ideg,
//                min+0.001, // Correct possible unbiased rounding
                      min,
                      ew);
      break;

    case(CONVERT_HP_NOSP): /* do HP w/nospace */
      xastir_snprintf(str,
                      str_len,
                      "%03d%06.3f%c",
                      ideg,
//                min+0.0001, // Correct possible unbiased rounding
                      min,
                      ew);
      break;

    case(CONVERT_VHP_NOSP): /* do Very HP w/nospace */
      xastir_snprintf(str,
                      str_len,
                      "%03d%07.4f%c",
                      ideg,
//                min+0.00001, // Correct possible unbiased rounding
                      min,
                      ew);
      break;

    case(CONVERT_UP_TRK): /* for tracklog files */
      xastir_snprintf(str,
                      str_len,
                      "%c%03d %07.4f",
                      ew,
                      ideg,
//                min+0.00001); // Correct possible unbiased rounding
                      min);
      break;

    case(CONVERT_DEC_DEG):
      xastir_snprintf(str,
                      str_len,
                      "%09.5f%c",
//                (ideg+min/60.0)+0.000001, // Correct possible unbiased rounding
                      ideg+min/60.0,
                      ew);
      break;

    case(CONVERT_DMS_NORMAL):
      xastir_snprintf(str,
                      str_len,
                      "%03d %02d %04.1f%c",
                      ideg,
                      imin,
//                sec+0.01, // Correct possible unbiased rounding
                      sec,
                      ew);
      break;

    case(CONVERT_DMS_NORMAL_FORMATED):
      xastir_snprintf(str,
                      str_len,
                      "%03d%c%02d\'%04.1f%c",
                      ideg,
                      0xb0,       // Degree symbol
                      imin,
//                sec+0.01, // Correct possible unbiased rounding
                      sec,
                      ew);
      break;

    case(CONVERT_HP_NORMAL_FORMATED):
      xastir_snprintf(str,
                      str_len,
                      "%03d%c%06.3f%c",
                      ideg,
                      0xb0,         // Degree symbol
//                min+0.0001, // Correct possible unbiased rounding
                      min,
                      ew);
      break;

    case(CONVERT_HP_NORMAL):
    default: /* do HP normal */
      xastir_snprintf(str,
                      str_len,
                      "%03d %06.3f%c",
                      ideg,
//                min+0.0001, // Correct possible unbiased rounding
                      min,
                      ew);
      break;
  }
}





/* convert latitude from string to long with 1/100 sec resolution */
//
// Input is in [D]DMM.MM[MM]N format (degrees/decimal
// minutes/direction)
//
long convert_lat_s2l(char *lat)        /* N=0°, Ctr=90°, S=180° */
{
  long centi_sec;
  char copy[15];
  char n[15];
  char *p;
  char offset;


  // Find the decimal point if present
  p = strstr(lat, ".");

  if (p == NULL)  // No decimal point found
  {
    return(0l);
  }

  memset(copy, '\0', sizeof(copy));
  offset = p - lat;   // Arithmetic on pointers
  switch (offset)
  {
    case 0:     // .MM[MM]N
      return(0l); // Bad, no degrees or minutes
      break;
    case 1:     // M.MM[MM]N
      return(0l); // Bad, no degrees
      break;
    case 2:     // MM.MM[MM]N
      return(0l); // Bad, no degrees
      break;
    case 3:     // DMM.MM[MM]N
      xastir_snprintf(copy,
                      sizeof(copy),
                      "0%s",  // Add a leading '0'
                      lat);
      break;
    case 4:     // DDMM.MM[MM]N
      xastir_snprintf(copy,
                      sizeof(copy),
                      "%s",   // Copy verbatim
                      lat);
      break;
    default:
      break;
  }

  copy[14] = '\0';
  centi_sec=0l;
  if (copy[4]=='.'
      && (   (char)toupper((int)copy[ 5])=='N'
             || (char)toupper((int)copy[ 6])=='N'
             || (char)toupper((int)copy[ 7])=='N'
             || (char)toupper((int)copy[ 8])=='N'
             || (char)toupper((int)copy[ 9])=='N'
             || (char)toupper((int)copy[10])=='N'
             || (char)toupper((int)copy[11])=='N'
             || (char)toupper((int)copy[ 5])=='S'
             || (char)toupper((int)copy[ 6])=='S'
             || (char)toupper((int)copy[ 7])=='S'
             || (char)toupper((int)copy[ 8])=='S'
             || (char)toupper((int)copy[ 9])=='S'
             || (char)toupper((int)copy[10])=='S'
             || (char)toupper((int)copy[11])=='S'))
  {

    substr(n, copy, 2);       // degrees
    centi_sec=atoi(n)*60*60*100;

    substr(n, copy+2, 2);     // minutes
    centi_sec += atoi(n)*60*100;

    substr(n, copy+5, 4);     // fractional minutes
    // Keep the fourth digit if present, as it resolves to 0.6
    // of a 1/100 sec resolution.  Two counts make one count in
    // the Xastir coordinate system.

    // Extend the digits to full precision by adding zeroes on
    // the end.
    strncat(n, "0000", sizeof(n) - 1 - strlen(n));

    // Get rid of the N/S character
    if (!isdigit((int)n[2]))
    {
      n[2] = '0';
    }
    if (!isdigit((int)n[3]))
    {
      n[3] = '0';
    }

    // Terminate substring at the correct digit
    n[4] = '\0';
//fprintf(stderr,"Lat: %s\n", n);

    // Add 0.5 (Poor man's rounding)
    centi_sec += (long)((atoi(n) * 0.6) + 0.5);

    if (       (char)toupper((int)copy[ 5])=='N'
               || (char)toupper((int)copy[ 6])=='N'
               || (char)toupper((int)copy[ 7])=='N'
               || (char)toupper((int)copy[ 8])=='N'
               || (char)toupper((int)copy[ 9])=='N'
               || (char)toupper((int)copy[10])=='N'
               || (char)toupper((int)copy[11])=='N')
    {
      centi_sec = -centi_sec;
    }

    centi_sec += 90*60*60*100;
  }
  return(centi_sec);
}





/* convert longitude from string to long with 1/100 sec resolution */
//
// Input is in [DD]DMM.MM[MM]W format (degrees/decimal
// minutes/direction).
//
long convert_lon_s2l(char *lon)       /* W=0°, Ctr=180°, E=360° */
{
  long centi_sec;
  char copy[16];
  char n[16];
  char *p;
  char offset;


  // Find the decimal point if present
  p = strstr(lon, ".");

  if (p == NULL)  // No decimal point found
  {
    return(0l);
  }

  memset(copy, '\0', sizeof(copy));
  offset = p - lon;   // Arithmetic on pointers
  switch (offset)
  {
    case 0:     // .MM[MM]N
      return(0l); // Bad, no degrees or minutes
      break;
    case 1:     // M.MM[MM]N
      return(0l); // Bad, no degrees
      break;
    case 2:     // MM.MM[MM]N
      return(0l); // Bad, no degrees
      break;
    case 3:     // DMM.MM[MM]N
      xastir_snprintf(copy,
                      sizeof(copy),
                      "00%s",  // Add two leading zeroes
                      lon);
      break;
    case 4:     // DDMM.MM[MM]N
      xastir_snprintf(copy,
                      sizeof(copy),
                      "0%s",   // Add leading '0'
                      lon);
      break;
    case 5:     // DDDMM.MM[MM]N
      xastir_snprintf(copy,
                      sizeof(copy),
                      "%s",   // Copy verbatim
                      lon);
      break;
    default:
      break;
  }

  copy[15] = '\0';
  centi_sec=0l;
  if (copy[5]=='.'
      && (   (char)toupper((int)copy[ 6])=='W'
             || (char)toupper((int)copy[ 7])=='W'
             || (char)toupper((int)copy[ 8])=='W'
             || (char)toupper((int)copy[ 9])=='W'
             || (char)toupper((int)copy[10])=='W'
             || (char)toupper((int)copy[11])=='W'
             || (char)toupper((int)copy[12])=='W'
             || (char)toupper((int)copy[ 6])=='E'
             || (char)toupper((int)copy[ 7])=='E'
             || (char)toupper((int)copy[ 8])=='E'
             || (char)toupper((int)copy[ 9])=='E'
             || (char)toupper((int)copy[10])=='E'
             || (char)toupper((int)copy[11])=='E'
             || (char)toupper((int)copy[12])=='E'))
  {

    substr(n,copy,3);    // degrees 013
    centi_sec=atoi(n)*60*60*100;

    substr(n,copy+3,2);  // minutes 26
    centi_sec += atoi(n)*60*100;
    // 01326.66E  01326.660E

    substr(n,copy+6,4);  // fractional minutes 66E 660E or 6601
    // Keep the fourth digit if present, as it resolves to 0.6
    // of a 1/100 sec resolution.  Two counts make one count in
    // the Xastir coordinate system.

    // Extend the digits to full precision by adding zeroes on
    // the end.
    strncat(n, "0000", sizeof(n) - 1 - strlen(n));

    // Get rid of the E/W character
    if (!isdigit((int)n[2]))
    {
      n[2] = '0';
    }
    if (!isdigit((int)n[3]))
    {
      n[3] = '0';
    }

    n[4] = '\0';    // Make sure substring is terminated
//fprintf(stderr,"Lon: %s\n", n);

    // Add 0.5 (Poor man's rounding)
    centi_sec += (long)((atoi(n) * 0.6) + 0.5);

    if (       (char)toupper((int)copy[ 6])=='W'
               || (char)toupper((int)copy[ 7])=='W'
               || (char)toupper((int)copy[ 8])=='W'
               || (char)toupper((int)copy[ 9])=='W'
               || (char)toupper((int)copy[10])=='W'
               || (char)toupper((int)copy[11])=='W'
               || (char)toupper((int)copy[12])=='W')
    {
      centi_sec = -centi_sec;
    }

    centi_sec +=180*60*60*100;;
  }
  return(centi_sec);
}





/*
 *  Convert latitude from Xastir format to radian
 */
double convert_lat_l2r(long lat)
{
  double ret;
  double degrees;

  degrees = (double)(lat / (100.0*60.0*60.0)) -90.0;
  ret = (-1)*degrees*(M_PI/180.0);
  return(ret);
}





/*
 *  Convert longitude from Xastir format to radian
 */
double convert_lon_l2r(long lon)
{
  double ret;
  double degrees;

  degrees = (double)(lon / (100.0*60.0*60.0)) -180.0;
  ret = (-1)*degrees*(M_PI/180.0);
  return(ret);
}





// Distance calculation (Great Circle) using the Haversine formula
// (2-parameter arctan version), which gives better accuracy than
// the "Law of Cosines" for short distances.  It should be
// equivalent to the "Law of Cosines for Spherical Trigonometry" for
// longer distances.  Haversine is a great-circle calculation.
//
//
// Inputs:  lat1/long1/lat2/long2 in radians (double)
//
// Outputs: Distance in meters between them (double)
//
double calc_distance_haversine_radian(double lat1, double lon1, double lat2, double lon2)
{
  double dlon, dlat;
  double a, c, d;
  double R = EARTH_RADIUS_METERS;
#define square(x) (x)*(x)


  dlon = lon2 - lon1;
  dlat = lat2 - lat1;
  a = square((sin(dlat/2.0))) + cos(lat1) * cos(lat2) * square((sin(dlon/2.0)));
  c = 2.0 * atan2(sqrt(a), sqrt(1.0-a));
  d = R * c;

  return(d);
}





// Distance calculation (Great Circle) using the Haversine formula
// (2-parameter arctan version), which gives better accuracy than
// the "Law of Cosines" for short distances.  It should be
// equivalent to the "Law of Cosines for Spherical Trigonometry" for
// longer distances.  Haversine is a great-circle calculation.
//
//
// Inputs:  lat1/long1/lat2/long2 in Xastir coordinate system (long)
//
// Outputs: Distance in meters between them (double)
//
double calc_distance_haversine(long lat1, long lon1, long lat2, long lon2)
{
  double r_lat1,r_lat2;
  double r_lon1,r_lon2;

  r_lat1 = convert_lat_l2r(lat1);
  r_lon1 = convert_lon_l2r(lon1);
  r_lat2 = convert_lat_l2r(lat2);
  r_lon2 = convert_lon_l2r(lon2);

  return(calc_distance_haversine_radian(r_lat1, r_lon1, r_lat2, r_lon2));
}





/*
 *  Calculate distance in meters between two locations
 *
 *  What type of calculation is this, Rhumb Line distance or Great
 *  Circle distance?
 *  Answer:  "Law of Cosines for Spherical Trigonometry", which is a
 *  great-circle calculation.
 *
 *
 * Inputs:  lat1/long1/lat2/long2 in Xastir coordinate system (long)
 *
 * Outputs: Distance in meters between them (double)
 *
 */
double calc_distance_law_of_cosines(long lat1, long lon1, long lat2, long lon2)
{
  double r_lat1,r_lat2;
  double r_lon1,r_lon2;
  double r_d;


  r_lat1 = convert_lat_l2r(lat1);
  r_lon1 = convert_lon_l2r(lon1);
  r_lat2 = convert_lat_l2r(lat2);
  r_lon2 = convert_lon_l2r(lon2);
  r_d = acos(sin(r_lat1) * sin(r_lat2) + cos(r_lat1) * cos(r_lat2) * cos(r_lon1-r_lon2));

//fprintf(stderr,"Law of Cosines Distance: %f\n",
//    r_d*180*60/M_PI*1852);
//fprintf(stderr,"     Haversine Distance: %f\n\n",
//    calc_distance_haversine(lat1, lon1, lat2, lon2));

  return(r_d*180*60/M_PI*1852);
}





// Here's where we choose which of the above two functions get used.
//
double calc_distance(long lat1, long lon1, long lat2, long lon2)
{
//    return(calc_distance_law_of_cosines(lat1, lon1, lat2, lon2));
  return(calc_distance_haversine(lat1, lon1, lat2, lon2));
}





/*
 *  Calculate distance between two locations in nautical miles and course from loc2 to loc1
 *
 *  What type of calculation is this, Rhumb Line distance or Great
 *  Circle distance?
 *  Answer:  "Law of Cosines for Spherical Trigonometry", which is a
 *  great-circle calculation, or Haversine, also a great-circle
 *  calculation.
 *
 *  NOTE:  The angle returned is a separate calculation, but using
 *  the unit sphere distance in it's calculation.  A great circle
 *  bearing is computed, not a Rhumb-line bearing.
 *
 *
 * Inputs:  lat1/long1/lat2/long2 in Xastir coordinate system (long)
 *          Length of course_deg string (int)
 *
 * Outputs: Distance in nautical miles between them (double).
 *          course_deg (string)
 *
 */
double calc_distance_course(long lat1, long lon1, long lat2, long lon2, char *course_deg, int course_deg_length)
{
  double ret;
  double r_lat1, r_lat2;
  double r_lon1, r_lon2;
  double r_d, r_c, r_m;

  r_lat1 = convert_lat_l2r(lat1);
  r_lon1 = convert_lon_l2r(lon1);
  r_lat2 = convert_lat_l2r(lat2);
  r_lon2 = convert_lon_l2r(lon2);


  // Compute the distance.  We have a choice between using Law of
  // Cosines or Haversine Formula here.

  // 1) Law of Cosines for Spherical Trigonometry.  This is
  // unreliable for small distances because the inverse cosine is
  // ill-conditioned.  A computer carrying seven significant
  // digits can't distinguish the cosines of distances smaller
  // than about one minute of arc.
//    r_d = acos(sin(r_lat1) * sin(r_lat2) + cos(r_lat1) * cos(r_lat2) * cos(r_lon1-r_lon2));


  // 2) Haversine Formula.  Returns answer in meters.
  r_m = calc_distance_haversine_radian(r_lat1, r_lon1, r_lat2, r_lon2);
  //
  // Conversion from distance in meters back to unit sphere.  This
  // is needed for the course calculation below as well as the
  // later scaling up to feet/meters or miles/km.
  r_d = r_m / EARTH_RADIUS_METERS;


  // Compute the great-circle bearing
  if (cos(r_lat1) < 0.0000000001)
  {
    if (r_lat1>0.0)
    {
      r_c=M_PI;
    }
    else
    {
      r_c=0.0;
    }
  }
  else
  {
    if (sin((r_lon2-r_lon1))<0.0)
    {
      r_c = acos((sin(r_lat2)-sin(r_lat1)*cos(r_d))/(sin(r_d)*cos(r_lat1)));
    }
    else
    {
      r_c = (2*M_PI) - acos((sin(r_lat2)-sin(r_lat1)*cos(r_d))/(sin(r_d)*cos(r_lat1)));
    }

  }

  // Return the course
  xastir_snprintf(course_deg,
                  course_deg_length,
                  "%.1f",
                  (180.0/M_PI)*r_c);

  // Return the distance (nautical miles?)
  ret = r_d*180*60/M_PI;

  /*
  // Convert from nautical miles to feet
  fprintf(stderr,"Law of Cosines Distance: %fft\t%fmi\n",
      ret*5280.0*1.15078, ret*1.15078);
  // Convert from meters to feet
  fprintf(stderr,"     Haversine Distance: %fft\t%fmi\n\n",
      calc_distance_haversine(lat1, lon1, lat2, lon2)*3.28084,
      calc_distance_haversine(lat1, lon1, lat2, lon2)/1000/1.609344);
  */

  return(ret);
}





//*****************************************************************
// distance_from_my_station - compute distance from my station and
//       course with a given call
//
// return distance and course
//
// Returns 0.0 for distance if station not found in database or the
// station hasn't sent out a posit yet.
//*****************************************************************

double distance_from_my_station(char *call_sign, char *course_deg)
{
  DataRow *p_station;
  double distance;
  float value;
  long l_lat, l_lon;

  distance = 0.0;
  l_lat = convert_lat_s2l(my_lat);
  l_lon = convert_lon_s2l(my_long);
  p_station = NULL;
  if (search_station_name(&p_station,call_sign,1))
  {
    // Check whether we have a posit yet for this station
    if ( (p_station->coord_lat == 0l)
         && (p_station->coord_lon == 0l) )
    {
      distance = 0.0;
    }
    else
    {
      value = (float)calc_distance_course(l_lat,
                                          l_lon,
                                          p_station->coord_lat,
                                          p_station->coord_lon,
                                          course_deg,
                                          sizeof(course_deg));

      if (english_units)
      {
        distance = value * 1.15078;  // nautical miles to miles
      }
      else
      {
        distance = value * 1.852;  // nautical miles to km
      }
    }
//        fprintf(stderr,"DistFromMy: %s %s -> %f\n",temp_lat,temp_long,distance);
  }
  else    // Station not found
  {
    distance = 0.0;
  }

  //fprintf(stderr,"Distance for %s: %f\n", call_sign, distance);

  return(distance);
}





/*********************************************************************/
/* convert_bearing_to_name - converts a bearing in degrees to        */
/*   name for the bearing.  Expects the degrees as a text string     */
/*   since that's what the preceding functions output.               */
/* Set the opposite flag true for the inverse bearing (from vs to)   */
/*********************************************************************/

static struct
{
  double low,high;
  char *dircode,*lang;
} directions[] =
{
  {327.5,360,"N","SPCHDIRN00"},
  {0,22.5,"N","SPCHDIRN00"},
  {22.5,67.5,"NE","SPCHDIRNE0"},
  {67.5,112.5,"E","SPCHDIRE00"},
  {112.5,157.5,"SE","SPCHDIRSE0"},
  {157.5,202.5,"S","SPCHDIRS00"},
  {202.5,247.5,"SW","SPCHDIRSW0"},
  {247.5,292.5,"W","SPCHDIRW00"},
  {292.5,327.5,"NW","SPCHDIRNW0"},
};



char *convert_bearing_to_name(char *bearing, int opposite)
{
  double deg = atof(bearing);
  int i;

  if (opposite)
  {
    if (deg > 180)
    {
      deg -= 180.0;
    }
    else if (deg <= 180)
    {
      deg += 180.0;
    }
  }
  for (i = 0; i < (int)( sizeof(directions)/sizeof(directions[0]) ); i++)
  {
    if (deg >= directions[i].low && deg < directions[i].high)
    {
      return langcode(directions[i].lang);
    }
  }
  return "?";
}





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





int filethere(char *fn)
{
  FILE *f;
  int ret;

  ret =0;
  f=fopen(fn,"r");
  if (f != NULL)
  {
    ret=1;
    (void)fclose(f);
  }
  return(ret);
}





int filecreate(char *fn)
{
  FILE *f;
  int ret;

  if (! filethere(fn))     // If no file
  {

    ret = 0;
    fprintf(stderr,"Making user %s file\n", fn);
    f=fopen(fn,"w+");   // Create it
    if (f != NULL)
    {
      ret=1;          // We were successful
      (void)fclose(f);
    }
    return(ret);        // Couldn't create file for some reason
  }
  return(1);  // File already exists
}





time_t file_time(char *fn)
{
  struct stat file_status;

  if(stat(fn,&file_status)==0)
  {
    return((time_t)file_status.st_ctime);
  }

  return(-1);
}





// Function written by Adam Hahn, AI4QB.  Contributed to the public
// domain.  We've modified it from his initial code so any bugs are
// our fault.
int copy_file(char *infilename, char *outfilename)
{
  FILE *infile, *outfile;
  char *buffer;
  size_t numread = 0;


  if ((infile = fopen(infilename,"rb")) > (FILE *)0)
  {

    if ((outfile = fopen(outfilename,"wb")) > (FILE *)0)
    {
      buffer = (char *)malloc(1024);

      while (!feof(infile))
      {
        numread = fread(buffer, 1, 1024, infile);
        fwrite(buffer, 1, numread, outfile);
      }
      free(buffer);
      fflush(outfile);
      fclose(outfile);
    }
    else
    {
      fprintf(stderr,"Error opening destination file %s for writing", outfilename);
      fclose(infile);
      return(1);
    }
    fclose(infile);
  }
  else
  {
    fprintf(stderr,"Error opening source file %s for reading", infilename);
    return(1);
  }
  return 0;
}





// used by log_data
void rotate_file(char *file, int max_keep )
{
  int i;
  char file_a[MAX_FILENAME];
  char file_b[MAX_FILENAME];
  struct stat file_status;

  if (debug_level & 1)
  {
    fprintf(stderr, "Rotating: %s. Will keep %d \n", file, max_keep);
  }

  for(i=max_keep; i>=1; i--)
  {

    if (debug_level & 1)
    {
      fprintf(stderr, "rotate: %s : %s\n", file_b, file_a);
    }

    xastir_snprintf(file_a,sizeof(file_a),"%s.%d",file,i);
    xastir_snprintf(file_b,sizeof(file_b),"%s.%d",file,i-1);

    unlink (file_a);
    if (stat(file_a, &file_status) == 0)
    {
      // We got good status.  That means it didn't get deleted!
      fprintf(stderr,
              "Couldn't delete file '%s': %s",
              file_a,strerror(errno));
      return;
    }

    // Rename previous to next
    //
    // Check whether file_b exists
    if (stat(file_b, &file_status) == 0)
    {
      if (!S_ISREG(file_status.st_mode))
      {
        fprintf(stderr,
                "Couldn't stat %s\n",
                file_b);
        break;
      }
      if ( rename (file_b, file_a) )
      {
        fprintf(stderr,
                "Couldn't rename %s to %s, cancelling log_rotate()\n",
                file_b,
                file_a);
        return;
      }
    }

  }

  if (debug_level & 1)
  {
    fprintf(stderr, "rotate: %s : %s\n", file, file_a);
  }

  if ( rename (file, file_a) )
  {
    fprintf(stderr,
            "Couldn't rename %s to %s, cancelling log_rotate()\n",
            file,
            file_a);
  }

  return;
}





char *fetch_file_line(FILE *f, char *line)
{
  char cin;
  int pos;


  pos = 0;
  line[0] = '\0';

  while (!feof(f))
  {

    // Read one character at a time
    if (fread(&cin,1,1,f) == 1)
    {

      if (pos < MAX_LINE_SIZE)
      {
        if (cin != (char)13)    // CR
        {
          line[pos++] = cin;
        }
      }

      if (cin == (char)10)   // Found LF as EOL char
      {
        line[pos++] = '\0'; // Always add a terminating zero after last char
        pos = 0;          // start next line
        return(line);
      }
    }
  }

  // Must be end of file
  line[pos] = '\0';
  return(line);
}





// Restore weather alerts so that we have a clear picture of the
// current state.  Check timestamps on the file.  If relatively
// current, read the file in.
//
void load_wx_alerts_from_log_working_sub(time_t time_now, char *filename)
{
  time_t file_timestamp;
  int file_age;
  int expire_limit;   // In seconds
  char line[MAX_LINE_SIZE+1];
  FILE *f;


  expire_limit = 60 * 60 * 24 * 15;   // 15 days
//    expire_limit = 60 * 60 * 24 * 1;   // 1 day

  file_timestamp = file_time(filename);

  if (file_timestamp == -1)
  {
//        fprintf(stderr,"File %s doesn't exist\n", filename);
    return;
  }

  file_age = time_now - file_timestamp;

  if ( file_age > expire_limit)
  {
//        fprintf(stderr,"Old file: %s, skipping...\n", filename);
    return;
  }

//    fprintf(stderr,"File is current: %s\n", filename);

  // Read the file in, as it exists and is relatively new.

  // Check timestamps before each log line and skip those that are
  // old.  Lines in the file should look about like this:
  //
  // # 1157027319  Thu Aug 31 05:28:39 PDT 2006
  // OUNSWO>APRS::SKYOUN   :OUN Check For Activation VCEAB
  // # 1157027319  Thu Aug 31 05:28:39 PDT 2006
  // LZKFFS>APRS::NWS_ADVIS:311324z,FLOOD,ARC67-147 V1PAA
  //
  // We could try to use the regular read_file and read_file_ptr
  // scheme for reading in the log file, but we'd have to modify
  // it in two ways:  We need to keep from bringing up the
  // interfaces so we'd need to set a flag when done reading and
  // then start them, plus we'd need to have it check the
  // timestamps and skip old ones.  Instead we'll do it all on our
  // own here so that we can control everything ourselves.

  f = fopen(filename, "r");
  if (!f)
  {
    fprintf(stderr,"Wx Alert log file could not be opened for reading\n");
    return;
  }

  while (!feof(f))    // Read until end of file
  {

    (void)fetch_file_line(f, line);

restart_sync:

    if (line[0] == '\0')
    {
      // Empty line found, skip
    }

    else if (line[0] == '#')   // Timestamp line, check the date
    {
      time_t line_stamp;
      int line_age;

      if (strlen(line) < 3)   // Line is too short, skip
      {
        goto restart_sync;
      }

      line_stamp = atoi(&line[2]);
      line_age = time_now - line_stamp;
//fprintf(stderr, "Age: %d\t", line_age);

      if ( line_age < expire_limit)
      {
        // Age is good, read next line and process it

        (void)fetch_file_line(f, line);

        if (line[0] != '#')   // It's a packet, not a timestamp line
        {
//fprintf(stderr,"%s\n",line);
          decode_ax25_line(line,'F',-1, 1);   // Decode the packet
        }
        else
        {
          goto restart_sync;
        }
      }
    }
  }
  if (feof(f))   // Close file if at the end
  {
    (void)fclose(f);
  }
}





// Restore weather alerts so that we have a clear picture of the
// current state.  Do this before we start the interfaces.  Only
// reload if the log files datestamps are relatively current.
//
// Check timestamps on each file in turn.  If relatively
// current, read them in the correct order:
// wx_alert.log.3
// wx_alert.log.2
// wx_alert.log.1
// wx.alert.log
//
void load_wx_alerts_from_log(void)
{
  time_t time_now;
  char filename[MAX_FILENAME];
  char logfile_path[MAX_VALUE];

  get_user_base_dir(LOGFILE_WX_ALERT,logfile_path, sizeof(logfile_path));

  time_now = sec_now();

  fprintf(stderr,"*** Reading WX Alert log files\n");

  // wx_alert.log.3
  xastir_snprintf(filename,
                  sizeof(filename),
                  "%s.3",
                  logfile_path );
  load_wx_alerts_from_log_working_sub(time_now, filename);

  // wx_alert.log.2
  xastir_snprintf(filename,
                  sizeof(filename),
                  "%s.2",
                  logfile_path );
  load_wx_alerts_from_log_working_sub(time_now, filename);

  // wx_alert.log.1
  xastir_snprintf(filename,
                  sizeof(filename),
                  "%s.1",
                  logfile_path );
  load_wx_alerts_from_log_working_sub(time_now, filename);

  // wx_alert.log
  xastir_snprintf(filename,
                  sizeof(filename),
                  "%s",
                  logfile_path );
  load_wx_alerts_from_log_working_sub(time_now, filename);

  fill_in_new_alert_entries();

  fprintf(stderr,"*** Done with WX Alert log files\n");
}





// Note that the length of "line" can be up to MAX_DEVICE_BUFFER,
// which is currently set to 4096.
//
void log_data(char *file, char *line)
{
  FILE *f;
  struct stat file_status;
  int reset_setuid = 0 ;


  // Check for "# Tickle" first, don't log it if found.
  // It's an idle string designed to keep the socket active.
  if ( (strncasecmp(line, "#Tickle", 7)==0)
       || (strncasecmp(line, "# Tickle", 8) == 0) )
  {
    return;
  }
  else
  {
    char temp[200];
    char timestring[100+1];
    struct tm *time_now;
    time_t secs_now;

    // Fetch the current date/time string
//        get_timestamp(timestring);
    secs_now=sec_now();

    time_now = localtime(&secs_now);

    (void)strftime(timestring,100,"%a %b %d %H:%M:%S %Z %Y",time_now);

    xastir_snprintf(temp,
                    sizeof(temp),
                    "# %ld  %s",
                    (unsigned long)secs_now,
                    timestring);

    // Change back to the base directory

// This call corrupts the "file" variable.  Commented it out as we
// don't appear to need it anyway.  The complete root-anchored
// path/filename are passed to us in the "file" parameter.
//
//        chdir(get_user_base_dir(""));

    // check size and rotate if too big

    if (stat(file, &file_status)==0)
    {

//            if (debug_level & 1) {
//                fprintf(stderr, "log_data(): logfile size: %ld \n",(long) file_status.st_size);
//            }

      if ((file_status.st_size + strlen(temp) + strlen(line) )> MAX_LOGFILE_SIZE)
      {
        if (debug_level & 1)
        {
          fprintf(stderr, "log_data(): calling rotate_file()\n");
        }

        rotate_file(file,3);
      }

    }
    else
    {
      // ENOENT is ok -- we make the file below
      if (errno != ENOENT )
      {
        fprintf(stderr,"Couldn't stat log file '%s': %s\n",
                file,strerror(errno));
        return;
      }

    }

    if (getuid() != geteuid())
    {
      reset_setuid=1;
      DISABLE_SETUID_PRIVILEGE;
    }

    f=fopen(file,"a");
    if (f!=NULL)
    {
      fprintf(f,"%s\n", temp); // Write the timestamp line
      fprintf(f,"%s\n",line);  // Write the data line
      (void)fclose(f);
    }
    else
    {
      fprintf(stderr,"Couldn't open file for appending: %s\n", file);
    }

    if(reset_setuid)
    {
      ENABLE_SETUID_PRIVILEGE;
    }
  }
}





//
// Tactical callsign logging
//
// Logging function called from the Assign Tactical Call right-click
// menu option and also from db.c:fill_in_tactical_call.  Each
// tactical assignment is logged as one line.
//
// We need to check for identical callsigns in the file, deleting
// lines that have the same name and adding new records to the end.
// Actually  BAD IDEA!  We want to keep the history of the tactical
// calls so that we can trace the changes later.
//
// Best method:  Look for lines containing matching callsigns and
// comment them out, adding the new tactical callsign at the end of
// the file.
//
// Do we need to use a special marker to mean NO tactical callsign
// is assigned?  This is for when we had a tactical call but now
// we're removing it.  The reload_tactical_calls() routine below
// would need to match.  Since we're using comma-delimited files, we
// can just check for an empty string instead.
//
void log_tactical_call(char *call_sign, char *tactical_call_sign)
{
  char file[MAX_VALUE];
  FILE *f;


  // Add it to our in-memory hash so that if stations expire and
  // then appear again they get assigned the same tac-call.
  add_tactical_to_hash(call_sign, tactical_call_sign);


  get_user_base_dir("config/tactical_calls.log", file, sizeof(file));

  f=fopen(file,"a");
  if (f!=NULL)
  {

    if (tactical_call_sign == NULL)
    {
      fprintf(f,"%s,\n",call_sign);
    }
    else
    {
      fprintf(f,"%s,%s\n",call_sign,tactical_call_sign);
    }

    (void)fclose(f);

    if (debug_level & 1)
    {
      fprintf(stderr,
              "Saving tactical call to file: %s:%s",
              call_sign,
              tactical_call_sign);
    }
  }
  else
  {
    fprintf(stderr,"Couldn't open file for appending: %s\n", file);
  }
}





//
// Function to load saved tactical calls back into xastir.  This
// is called on startup.  This implements persistent tactical calls
// across xastir restarts.
//
// Here we create a hash lookup and store one record for each valid
// line read from the tactical calls log file.  The key for each
// hash entry is the callsign-SSID.  Here we simply read them in and
// create the hash.  When a new station is heard on the air, it is
// checked against this hash and the tactical call field filled in
// if there is a match.
//
// Note that the length of "line" can be up to max_device_buffer,
// which is currently set to 4096.
//
void reload_tactical_calls(void)
{
  char file[MAX_VALUE];
  FILE *f;
  char line[300+1];


  get_user_base_dir("config/tactical_calls.log", file, sizeof(file));

  f=fopen(file,"r");
  if (f!=NULL)
  {

    while (fgets(line, 300, f) != NULL)
    {

      if (debug_level & 1)
      {
        fprintf(stderr,"loading tactical calls from file: %s",line);
      }

      if (line[0] != '#')     // skip comment lines
      {
        char *ptr;

        // we're dealing with comma-separated files, so
        // break the two pieces at the comma.
        ptr = index(line,',');

        if (ptr != NULL)
        {
          char *ptr2;


          ptr[0] = '\0';  // terminate the callsign
          ptr++;  // point to the tactical callsign

          // check for lf
          ptr2 = index(ptr,'\n');
          if (ptr2 != NULL)
          {
            ptr2[0] = '\0';
          }

          // check for cr
          ptr2 = index(ptr,'\r');
          if (ptr2 != NULL)
          {
            ptr2[0] = '\0';
          }

          if (debug_level & 1)
          {
            fprintf(stderr, "Call=%s,\tTactical=%s\n", line, ptr);
          }

          //                   call tac-call
          add_tactical_to_hash(line, ptr);
        }
      }
    }
    (void)fclose(f);
  }
  else
  {
    if (debug_level & 1)
    {
      fprintf(stderr,"couldn't open file for reading: %s\n", file);
    }
  }

  /*
      if (tactical_hash) {
          fprintf(stderr,"Stations w/tactical calls defined: %d\n",
              hashtable_count(tactical_hash));
      }
  */

}





// Returns time in seconds since the Unix epoch.
//
time_t sec_now(void)
{
  time_t timenw;
  time_t ret;

  ret = time(&timenw);
  return(ret);
}





char *get_time(char *time_here)
{
  struct tm *time_now;
  time_t timenw;

  (void)time(&timenw);
  time_now = localtime(&timenw);
  (void)strftime(time_here,MAX_TIME,"%m%d%Y%H%M%S",time_now);
  return(time_here);
}





///////////////////////////////////  Utilities  ////////////////////////////////////////////////////


/*
 *  Check for valid path and change it to TAPR format
 *  Add missing asterisk for stations that we must have heard via a digi
 *  Extract port for KAM TNCs
 *  Handle igate injection ID formats: "callsign-ssid,I" & "callsign-0ssid"
 *
 * TAPR-2 Format:
 * KC2ELS-1*>SX0PWT,RELAY,WIDE:`2`$l##>/>"4)}
 *
 * AEA Format:
 * KC2ELS-1*>RELAY>WIDE>SX0PWT:`2`$l##>/>"4)}
 */

int valid_path(char *path)
{
  int i,len,hops,j;
  int type,ast,allast,ins;
  char ch;


  len  = (int)strlen(path);
  type = 0;       // 0: unknown, 1: AEA '>', 2: TAPR2 ',', 3: mixed
  hops = 1;
  ast  = 0;
  allast = 0;

  // There are some multi-port TNCs that deliver the port at the end
  // of the path. For now we discard this information. If there is
  // multi-port TNC support some day, we should write the port into
  // our database.
  // KAM:        /V /H
  // KPC-9612:   /0 /1 /2
  if (len > 2 && path[len-2] == '/')
  {
    ch = path[len-1];
    if (ch == 'V' || ch == 'H' || ch == '0' || ch == '1' || ch == '2')
    {
      path[len-2] = '\0';
      len  = (int)strlen(path);
    }
  }


  // One way of adding igate injection ID is to add "callsign-ssid,I".
  // We need to remove the ",I" portion so it doesn't count as another
  // digi here.  This should be at the end of the path.
  if (len > 2 && path[len-2] == ',' && path[len-1] == 'I')    // Found ",I"
  {
    //fprintf(stderr,"%s\n",path);
    //fprintf(stderr,"Found ',I'\n");
    path[len-2] = '\0';
    len  = (int)strlen(path);
    //fprintf(stderr,"%s\n\n",path);
  }
  // Now look for the same thing but with a '*' character at the end.
  // This should be at the end of the path.
  if (len > 3 && path[len-3] == ',' && path[len-2] == 'I' && path[len-1] == '*')    // Found ",I*"
  {
    //fprintf(stderr,"%s\n",path);
    //fprintf(stderr,"Found ',I*'\n");
    path[len-3] = '\0';
    len  = (int)strlen(path);
    //fprintf(stderr,"%s\n\n",path);
  }


  // Another method of adding igate injection ID is to add a '0' in front of
  // the SSID.  For WE7U it would change to WE7U-00, for WE7U-15 it would
  // change to WE7U-015.  Take out this zero so the rest of the decoding will
  // work.  This should be at the end of the path.
  // Also look for the same thing but with a '*' character at the end.
  if (len > 6)
  {
    for (i=len-1; i>len-6; i--)
    {
      if (path[i] == '-' && path[i+1] == '0')
      {
        //fprintf(stderr,"%s\n",path);
        for (j=i+1; j<len; j++)
        {
          path[j] = path[j+1];    // Shift everything left by one
        }
        len = (int)strlen(path);
        //fprintf(stderr,"%s\n\n",path);
      }
      // Check whether we just chopped off the '0' from "-0".
      // If so, chop off the dash as well.
      if (path[i] == '-' && path[i+1] == '\0')
      {
        //fprintf(stderr,"%s\tChopping off dash\n",path);
        path[i] = '\0';
        len = (int)strlen(path);
        //fprintf(stderr,"%s\n",path);
      }
      // Check for "-*", change to '*' only
      if (path[i] == '-' && path[i+1] == '*')
      {
        //fprintf(stderr,"%s\tChopping off dash\n",path);
        path[i] = '*';
        path[i+1] = '\0';
        len = (int)strlen(path);
        //fprintf(stderr,"%s\n",path);
      }
      // Check for "-0" or "-0*".  Change to "" or "*".
      if ( path[i] == '-' && path[i+1] == '0' )
      {
        //fprintf(stderr,"%s\tShifting left by two\n",path);
        for (j=i; j<len; j++)
        {
          path[j] = path[j+2];    // Shift everything left by two
        }
        len = (int)strlen(path);
        //fprintf(stderr,"%s\n",path);
      }
    }
  }


  for (i=0,j=0; i<len; i++)
  {
    ch = path[i];

    if (ch == '>' || ch == ',')     // found digi call separator
    {
      // We're at the start of a callsign entry in the path

      if (ast > 1 || (ast == 1 && i-j > 10) || (ast == 0 && (i == j || i-j > 9)))
      {
        if (debug_level & 1)
        {
          fprintf(stderr, "valid_path(): Bad Path: More than one * in call or wrong call size\n");
        }
        return(0);              // more than one asterisk in call or wrong call size
      }
      ast = 0;                    // reset local asterisk counter

      j = i+1;                    // set to start of next call
      if (ch == ',')
      {
        type |= 0x02;  // set TAPR2 flag
      }
      else
      {
        type |= 0x01;  // set AEA flag (found '>')
      }
      hops++;                     // count hops
    }

    else                            // digi call character or asterisk
    {
      // We're in the middle of a callsign entry

      if (ch == '*')
      {
        ast++;                  // count asterisks in call
        allast++;               // count asterisks in path
      }
      else if ((ch <'A' || ch > 'Z')      // Not A-Z
               && (ch <'a' || ch > 'z')    // Not a-z
               && (ch <'0' || ch > '9')    // Not 0-9
               && ch != '-')
      {
        // Note that Q-construct and internet callsigns can
        // have a-z in them, AX.25 callsigns cannot unless
        // they are in a 3rd-party packet.

        if (debug_level & 1)
        {
          fprintf(stderr, "valid_path: Bad Path: Anti-loop stuff from aprsd or lower-case chars found\n");
        }
        return(0);          // wrong character in path
      }
    }
  }
  if (ast > 1 || (ast == 1 && i-j > 10) || (ast == 0 && (i == j || i-j > 9)))
  {
    if (debug_level & 1)
    {
      fprintf(stderr, "valid_path: Bad Path: More than one * or wrong call size (2)\n");
    }
    return(0);                      // more than one asterisk or wrong call size
  }

  if (type == 0x03)
  {
    if (debug_level & 1)
    {
      fprintf(stderr, "valid_path: Bad Path: Wrong format, both > and , in path\n");
    }
    return(0);                      // wrong format, both '>' and ',' in path
  }

  if (hops > 9)                       // [APRS Reference chapter 3]
  {
    if (debug_level & 1)
    {
      fprintf(stderr, "valid_path: Bad Path: hops > 9\n");
    }
    return(0);                      // too much hops, destination + 0-8 digipeater addresses
  }

  if (type == 0x01)
  {
    int delimiters[20];
    int k = 0;
    char dest[15];
    char rest[100];

    for (i=0; i<len; i++)
    {
      if (path[i] == '>')
      {
        path[i] = ',';          // Exchange separator character
        delimiters[k++] = i;    // Save the delimiter indexes
      }
    }

    // We also need to move the destination callsign to the end.
    // AEA has them in a different order than TAPR-2 format.
    // We'll move the destination address between delimiters[0]
    // and [1] to the end of the string.

    //fprintf(stderr,"Orig. Path:%s\n",path);
    // Save the destination
    xastir_snprintf(dest,sizeof(dest),"%s",&path[delimiters[--k]+1]);
    dest[strlen(path) - delimiters[k] - 1] = '\0'; // Terminate it
    dest[14] = '\0';    // Just to make sure
    path[delimiters[k]] = '\0'; // Delete it from the original path
    //fprintf(stderr,"Destination: %s\n",dest);

    // TAPR-2 Format:
    // KC2ELS-1*>SX0PWT,RELAY,WIDE:`2`$l##>/>"4)}
    //
    // AEA Format:
    // KC2ELS-1*>RELAY>WIDE>SX0PWT:`2`$l##>/>"4)}
    //          9     15   20

    // We now need to insert the destination into the middle of
    // the string.  Save part of it in another variable first.
    xastir_snprintf(rest,
                    sizeof(rest),
                    "%s",
                    path);
    //fprintf(stderr,"Rest:%s\n",rest);
    xastir_snprintf(path,len+1,"%s,%s",dest,rest);
    //fprintf(stderr,"New Path:%s\n",path);
  }

  if (allast < 1)                     // try to insert a missing asterisk
  {
    ins  = 0;
    hops = 0;

    for (i=0; i<len; i++)
    {

      for (j=i; j<len; j++)               // search for separator
      {
        if (path[j] == ',')
        {
          break;
        }
      }

      if (hops > 0 && (j - i) == 5)       // WIDE3
      {
        if (  path[ i ] == 'W' && path[i+1] == 'I' && path[i+2] == 'D'
              && path[i+3] == 'E' && path[i+4] >= '0' && path[i+4] <= '9')
        {
          ins = j;
        }
      }

      /*
      Don't do this!  It can mess up relay/wide1-1 digipeating by adding
      an asterisk later in the path than the first unused digi.
                  if (hops > 0 && (j - i) == 7) {     // WIDE3-2
                      if (  path[ i ] == 'W' && path[i+1] == 'I' && path[i+2] == 'D'
                         && path[i+3] == 'E' && path[i+4] >= '0' && path[i+4] <= '9'
                         && path[i+5] == '-' && path[i+6] >= '0' && path[i+6] <= '9'
                         && (path[i+4] != path[i+6]) ) {
                          ins = j;
                      }
                  }
      */

      if (hops > 0 && (j - i) == 6)       // TRACE3
      {
        if (  path[ i ] == 'T' && path[i+1] == 'R' && path[i+2] == 'A'
              && path[i+3] == 'C' && path[i+4] == 'E'
              && path[i+5] >= '0' && path[i+5] <= '9')
        {
          if (hops == 1)
          {
            ins = j;
          }
          else
          {
            ins = i-1;
          }
        }
      }

      /*
      Don't do this!  It can mess up relay/wide1-1 digipeating by adding
      an asterisk later in the path than the first unused digi.
                  if (hops > 0 && (j - i) == 8) {     // TRACE3-2
                      if (  path[ i ] == 'T' && path[i+1] == 'R' && path[i+2] == 'A'
                         && path[i+3] == 'C' && path[i+4] == 'E' && path[i+5] >= '0'
                         && path[i+5] <= '9' && path[i+6] == '-' && path[i+7] >= '0'
                         && path[i+7] <= '9' && (path[i+5] != path[i+7]) ) {
                          if (hops == 1)
                              ins = j;
                          else
                              ins = i-1;
                      }
                  }
      */

      hops++;
      i = j;                      // skip to start of next call
    }
    if (ins > 0)
    {
      for (i=len; i>=ins; i--)
      {
        path[i+1] = path[i];    // generate space for '*'
        // we work on a separate path copy which is long enough to do it
      }
      path[ins] = '*';            // and insert it
    }
  }
  return(1);  // Path is good
}





/*
 *  Check for a valid AX.25 call
 *      Valid calls consist of up to 6 uppercase alphanumeric characters
 *      plus optional SSID (four-bit integer)       [APRS Reference, AX.25 Reference]
 *
 * We originally required at least one number so-as to get rid of calls like
 * "NOCALL", but got rid of that code.
 */
int valid_call(char *call)
{
  int len, ok;
  int i, del, has_chr;
  char c;

  has_chr = 0;
  ok      = 1;
  len = (int)strlen(call);

  if (len == 0)
  {
    return(0);  // wrong size
  }

  while (call[0]=='c' && call[1]=='m' && call[2]=='d' && call[3]==':')
  {
    // Erase TNC prompts from beginning of callsign.  This may
    // not be the right place to do this, but it came in handy
    // here, so that's where I put it. -- KB6MER

    if (debug_level & 1)
    {
      char filtered_data[MAX_LINE_SIZE+1];

      xastir_snprintf(filtered_data,
                      sizeof(filtered_data),
                      "%s",
                      call);
      makePrintable(filtered_data);
      fprintf(stderr,"valid_call: Command Prompt removed from: %s",
              filtered_data);
    }

    for(i=0; call[i+4]; i++)
    {
      call[i]=call[i+4];
    }

    call[i++]=0;
    call[i++]=0;
    call[i++]=0;
    call[i++]=0;
    len=strlen(call);

    if (debug_level & 1)
    {
      char filtered_data[MAX_LINE_SIZE+1];

      xastir_snprintf(filtered_data,
                      sizeof(filtered_data),
                      "%s",
                      call);
      makePrintable(filtered_data);
      fprintf(stderr," result: %s", filtered_data);
    }
  }

  if (len > 9)
  {
    return(0);  // Too long for valid call (6-2 max e.g. KB6MER-12)
  }

  del = 0;
  for (i=len-2; ok && i>0 && i>=len-3; i--)   // search for optional SSID
  {
    if (call[i] =='-')
    {
      del = i;  // found the delimiter
    }
  }
  if (del)                                    // we have a SSID, so check it
  {
    if (len-del == 2)                       // 2 char SSID
    {
      if (call[del+1] < '1' || call[del+1] > '9')                         //  -1 ...  -9
      {
        del = 0;
      }
    }
    else                                    // 3 char SSID
    {
      if (call[del+1] != '1' || call[del+2] < '0' || call[del+2] > '5')   // -10 ... -15
      {
        del = 0;
      }
    }
  }

  if (del)
  {
    len = del;  // length of base call
  }

  for (i=0; ok && i<len; i++)                 // check for uppercase alphanumeric
  {
    c = call[i];

    if (c >= 'A' && c <= 'Z')
    {
      has_chr = 1;  // we need at least one char
    }
    else if (c >= '0' && c <= '9')
    {
      // We originally required at least one number
    }
    else
    {
      ok = 0;  // wrong character in call
    }
  }

//    if (!has_num || !has_chr)                 // with this we also discard NOCALL etc.
  if (!has_chr)
  {
    ok = 0;
  }

  ok = (ok && strcmp(call,"NOCALL") != 0);    // check for errors
  ok = (ok && strcmp(call,"ERROR!") != 0);
  ok = (ok && strcmp(call,"WIDE")   != 0);
  ok = (ok && strcmp(call,"RELAY")  != 0);
  ok = (ok && strcmp(call,"MAIL")   != 0);

  return(ok);
}





/*
 *  Check for a valid internet name.
 *  Accept darned-near anything here as long as it is the proper
 *  length and printable.
 */
int valid_inet_name(char *name, char *info, char *origin, int origin_size)
{
  int len, i, ok, oknws, okbom;
  char *ptr;

  len = (int)strlen(name);

  if (len > 9 || len == 0)            // max 9 printable ASCII characters
  {
    return(0);  // wrong size
  }

  for (i=0; i<len; i++)
    if (!isprint((int)name[i]))
    {
      return(0);  // not printable
    }

  // Modifies "origin" if a match found
  //
  if (len >= 5 && strncmp(name,"aprsd",5) == 0)
  {
    xastir_snprintf(origin, origin_size, "INET");
    origin[4] = '\0';   // Terminate it
    return(1);                      // aprsdXXXX is ok
  }

  // Modifies "origin" if a match found
  if (len == 6)                       // check for NWS or BOM
  {
    ok = 1;
    oknws = 0;
    okbom = 0;
    for (i=0; i<6; i++)
      if (name[i] <'A' || name[i] > 'Z')  // 6 uppercase characters
      {
        ok = 0;
      }
    ok = ok && (info != NULL);      // check if we can test info
    if (ok)
    {
      ptr = strstr(info,":NWS-"); // "NWS-" in info field (non-compressed alert)
      oknws = (ptr != NULL);

      if (!oknws)
      {
        ptr = strstr(info,":NWS_"); // "NWS_" in info field (compressed alert)
        oknws = (ptr != NULL);
      }

      // If we've got here, it's not an NWS message, let's see if its a BOM message
      if (!oknws)
      {
        ptr = strstr(info,":BOM-"); // "BOM-" in info field (compressed alert)
        okbom = (ptr != NULL);
      }

      if (!okbom)
      {
        ptr = strstr(info,":BOM_"); // "BOM_" in info field (compressed alert)
        okbom = (ptr != NULL);
      }
    }

    // Depending on whether we had an NWS or BOM message, se the origin appropriately
    if (oknws)
    {
      xastir_snprintf(origin, origin_size, "INET-NWS");
      origin[8] = '\0';
      return(1);                      // weather alerts
    }
    if (okbom)
    {
      xastir_snprintf(origin, origin_size, "INET-BOM");
      origin[8] = '\0';
      return(1);                      // weather alerts
    }
  }

  return(1);  // Accept anything else if we get to this point in
  // the code.  After all, the message came from the
  // internet, not from RF.
}





/*
 *  Keep track of last six digis that echo my transmission
 */
void upd_echo(char *path)
{
  int i,j,len;

  if (echo_digis[5][0] != '\0')
  {
    for (i=0; i<5; i++)
    {
      xastir_snprintf(echo_digis[i],
                      MAX_CALLSIGN+1,
                      "%s",
                      echo_digis[i+1]);

    }
    echo_digis[5][0] = '\0';
  }
  for (i=0,j=0; i < (int)strlen(path); i++)
  {
    if (path[i] == '*')
    {
      break;
    }
    if (path[i] == ',')
    {
      j=i;
    }
  }
  if (j > 0)
  {
    j++;  // first char of call
  }
  if (i > 0 && i-j <= 9)
  {
    len = i-j;
    for (i=0; i<5; i++)     // look for free entry
    {
      if (echo_digis[i][0] == '\0')
      {
        break;
      }
    }
    substr(echo_digis[i],path+j,len);
  }
}





// dl9sau:
// I liked to have xastir to compute the locator along with the normal coordinates.
// The algorithm derives from dk5sg's util/qth.c (wampes)
//
// This computes Maidenhead grid coordinates and is used for both
// the status line ("text2" widget) and the Coordinate Calculator at
// present.
//
char *sec_to_loc(long longitude, long latitude)
{
  static char buf[7];
  char *loc = buf;

  // database.h:    long coord_lon;                     // Xastir coordinates 1/100 sec, 0 = 180°W
  // database.h:    long coord_lat;                     // Xastir coordinates 1/100 sec, 0 =  90°N

  longitude /= 100L;
  latitude  =  2L * 90L * 3600L - 1L - (latitude / 100L);

  *loc++ = (char) (longitude / 72000L + 'A');
  longitude = longitude % 72000L;
  *loc++ = (char) (latitude  / 36000L + 'A');
  latitude  = latitude %  36000L;
  *loc++ = (char) (longitude /  7200L + '0');
  longitude = longitude %  7200L;
  *loc++ = (char) (latitude  /  3600L + '0');
  latitude  = latitude %   3600L;
  *loc++ = (char) (longitude /   300L + 'a');
  *loc++ = (char) (latitude  /   150L + 'a');
  *loc   = 0;   // Terminate the string
  return buf;
}





/*
 *  Substring function WITH a terminating NULL char, needs a string of at least size+1
 */
void substr(char *dest, char *src, int size)
{
  memcpy(dest, src, size+1);
  dest[size] = '\0';  // Terminate string
}






char *to_upper(char *data)
{
  int i;

  for(i=strlen(data)-1; i>=0; i--)
    if(islower((int)data[i]))
    {
      data[i]=toupper((int)data[i]);
    }

  return(data);
}





char *to_lower(char *data)
{
  int i;

  for(i=strlen(data)-1; i>=0; i--)
    if(isupper((int)data[i]))
    {
      data[i]=tolower((int)data[i]);
    }

  return(data);
}





int is_num_chr(char ch)
{
  return((int)isdigit(ch));
}






int is_num_or_sp(char ch)
{
  return((int)((ch >= '0' && ch <= '9') || ch == ' '));
}






int is_xnum_or_dash(char *data, int max)
{
  int i;
  int ok;

  ok=1;
  for(i=0; i<max; i++)
    if(!(isxdigit((int)data[i]) || data[i]=='-'))
    {
      ok=0;
      break;
    }

  return(ok);
}





// not used
int is_num_stuff(char *data, int max)
{
  int i;
  int ok;

  ok=1;
  for(i=0; i<max; i++)
    if(!(isdigit((int)data[i]) || data[i]=='-' || data[i]=='+' || data[i]=='.'))
    {
      ok=0;
      break;
    }

  return(ok);
}





// not used
int is_num(char *data)
{
  int i;
  int ok;

  ok=1;
  for(i=strlen(data)-1; i>=0; i--)
    if(!isdigit((int)data[i]))
    {
      ok=0;
      break;
    }

  return(ok);
}





//--------------------------------------------------------------------
//Removes all control codes ( < 1Ch ) from a string and set the 8th bit to zero
void removeCtrlCodes(char *cp)
{
  int i,j;
  int len = (int)strlen(cp);
  unsigned char *ucp = (unsigned char *)cp;

  for (i=0, j=0; i<=len; i++)
  {
    ucp[i] &= 0x7f;                 // Clear 8th bit
    if ( (ucp[i] >= (unsigned char)0x1c)           // Check for printable plus the Mic-E codes
         || ((char)ucp[i] == '\0') )   // or terminating 0
    {
      ucp[j++] = ucp[i] ;  // Copy to temp if printable
    }
  }
}





//--------------------------------------------------------------------
//Removes all control codes ( <0x20 or >0x7e ) from a string, including
// CR's, LF's, tab's, etc.
//
void makePrintable(char *cp)
{
  int i,j;
  int len = (int)strlen(cp);
  unsigned char *ucp = (unsigned char *)cp;

  for (i=0, j=0; i<=len; i++)
  {
    ucp[i] &= 0x7f;                 // Clear 8th bit
    if ( ((ucp[i] >= (unsigned char)0x20) && (ucp[i] <= (unsigned char)0x7e))
         || ((char)ucp[i] == '\0') )     // Check for printable or terminating 0
    {
      ucp[j++] = ucp[i] ;  // Copy to (possibly) new location if printable
    }
  }
}






//----------------------------------------------------------------------
// Implements safer mutex locking.  Posix-compatible mutex's will lock
// up a thread if lock's/unlock's aren't in perfect pairs.  They also
// allow one thread to unlock a mutex that was locked by a different
// thread.
// Remember to keep this thread-safe.  Need dynamic storage (no statics)
// and thread-safe calls.
//
// In order to keep track of process ID's in a portable way, we probably
// can't use the process ID stored inside the pthread_mutex_t struct.
// There's no easy way to get at it.  For that reason we'll create
// another struct that contains both the process ID and a pointer to
// the pthread_mutex_t, and pass pointers to those structs around in
// our programs instead.  The new struct is "xastir_mutex".
//


// Function to initialize the new xastir_mutex
// objects before use.
//
void init_critical_section(xastir_mutex *lock)
{
#ifdef MUTEX_DEBUG
  pthread_mutexattr_t attr;

  // Note that this stuff is not POSIX, so is considered non-portable.
  // Exists in Linux Threads implementation only?

# ifdef __LSB__
  // LSB VERSION (Linux Standard Base)
  // Note that we _must_ use the newer pthread function in this
  // case per LSB specs.
  (void)pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
# else   // __LSB__
  // NON_LSB VERSION
  // Check first for newer pthread function
#  ifdef HAVE_PTHREAD_MUTEXATTR_SETTYPE
  (void)pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK_NP);
#  else
  // Use older, deprecated pthread function
  (void)pthread_mutexattr_setkind_np(&attr, PTHREAD_MUTEX_ERRORCHECK_NP);
#  endif  // HAVE_PTHREAD_MUTEXATTR_SETTYPE
# endif  // __LSB__

  (void)pthread_mutexattr_init(&attr);
  (void)pthread_mutex_init(&lock->lock, &attr);
#else   // MUTEX_DEBUG
  (void)pthread_mutex_init(&lock->lock, NULL); // Set to default POSIX-compatible type
#endif  // MUTEX_DEBUG

  lock->threadID = 0;  // No thread has locked it yet
}





// Function which uses xastir_mutex objects to lock a
// critical shared section of code or variables.  Makes
// sure that only one thread can access the critical section
// at a time.  If there are no problems, it returns zero.
//
int begin_critical_section(xastir_mutex *lock, char *text)
{
  pthread_t calling_thread;
  int problems;
#ifdef MUTEX_DEBUG
  int ret;
#endif  // MUTEX_DEBUG

  problems = 0;

  // Note that /usr/include/bits/pthreadtypes.h has the
  // definition for pthread_mutex_t, and it includes the
  // owner thread number:  _pthread_descr pthread_mutex_t.__m_owner
  // It's probably not portable to use it though.

  // Get thread number we're currently running under
  calling_thread = pthread_self();

  if (pthread_equal( lock->threadID, calling_thread ))
  {
    // We tried to lock something that we already have the lock on.
    fprintf(stderr,"%s:Warning:This thread already has the lock on this resource\n", text);
    problems++;
    return(0);  // Return the OK code.  Skip trying the lock.
  }

  //if (lock->threadID != 0)
  //{
  // We tried to lock something that another thread has the lock on.
  // This is normal operation.  The pthread_mutex_lock call below
  // will put our process to sleep until the mutex is unlocked.
  //}

  // Try to lock it.
#ifdef MUTEX_DEBUG
  // Instead of blocking the thread, we'll loop here until there's
  // no deadlock, printing out status as we go.
  do
  {
    ret = pthread_mutex_lock(&lock->lock);
    if (ret == EDEADLK)     // Normal operation.  At least two threads want this lock.
    {
      fprintf(stderr,"%s:pthread_mutex_lock(&lock->lock) == EDEADLK\n", text);
      sched_yield();  // Yield the cpu to another thread
    }
    else if (ret == EINVAL)
    {
      fprintf(stderr,"%s:pthread_mutex_lock(&lock->lock) == EINVAL\n", text);
      fprintf(stderr,"Forgot to initialize the mutex object...\n");
      problems++;
      sched_yield();  // Yield the cpu to another thread
    }
  }
  while (ret == EDEADLK);

  if (problems)
  {
    fprintf(stderr,"Returning %d to calling thread\n", problems);
  }
#else   // MUTEX_DEBUG
  pthread_mutex_lock(&lock->lock);
#endif  // MUTEX_DEBUG

  // Note:  This can only be set AFTER we already have the mutex lock.
  // Otherwise there may be contention with other threads for setting
  // this variable.
  //
  lock->threadID = calling_thread;    // Save the threadID that we used
  // to lock this resource

  return(problems);
}





// Function which ends the locking of a critical section
// of code.  If there are no problems, it returns zero.
//
int end_critical_section(xastir_mutex *lock, char *text)
{
  pthread_t calling_thread;
  int problems;
#ifdef MUTEX_DEBUG
  int ret;
#endif  // MUTEX_DEBUG

  problems = 0;

  // Get thread number we're currently running under
  calling_thread = pthread_self();

  if (lock->threadID == 0)
  {
    // We have a problem.  This resource hasn't been locked.
    fprintf(stderr,"%s:Warning:Trying to unlock a resource that hasn't been locked:%ld\n",
            text,
            (long int)lock->threadID);
    problems++;
  }

  if (! pthread_equal( lock->threadID, calling_thread ))
  {
    // We have a problem.  We just tried to unlock a mutex which
    // a different thread originally locked.  Not good.
    fprintf(stderr,"%s:Trying to unlock a resource that a different thread locked originally: %ld:%ld\n",
            text,
            (long int)lock->threadID,
            (long int)calling_thread);
    problems++;
  }


  // We need to clear this variable BEFORE we unlock the mutex, 'cuz
  // other threads might be waiting to lock the mutex.
  //
  lock->threadID = 0; // We're done with this lock.


  // Try to unlock it.  Compare the thread identifier we used before to make
  // sure we should.

#ifdef MUTEX_DEBUG
  // EPERM error: We're trying to unlock something that we don't have a lock on.
  ret = pthread_mutex_unlock(&lock->lock);

  if (ret == EPERM)
  {
    fprintf(stderr,"%s:pthread_mutex_unlock(&lock->lock) == EPERM\n", text);
    problems++;
  }
  else if (ret == EINVAL)
  {
    fprintf(stderr,"%s:pthread_mutex_unlock(&lock->lock) == EINVAL\n", text);
    fprintf(stderr,"Someone forgot to initialize the mutex object\n");
    problems++;
  }

  if (problems)
  {
    fprintf(stderr,"Returning %d to calling thread\n", problems);
  }
#else   // MUTEX_DEBUG
  (void)pthread_mutex_unlock(&lock->lock);
#endif  // MUTEX_DEBUG

  return(problems);
}





#ifdef TIMING_DEBUG
void time_mark(int start)
{
  static struct timeval t_start;
  struct timeval t_cur;
  long sec, usec;

  if (start)
  {
    gettimeofday(&t_start, NULL);
    fprintf(stderr,"\nstart: 0.000000s");
  }
  else
  {
    gettimeofday(&t_cur, NULL);
    sec  = t_cur.tv_sec  - t_start.tv_sec;
    usec = t_cur.tv_usec - t_start.tv_usec;
    if (usec < 0)
    {
      sec--;
      usec += 1000000;
    }
    fprintf(stderr,"time:  %ld.%06lds\n", sec, usec);
  }
}
#endif  // TIMING_DEBUG





// Function which adds commas to callsigns (and other abbreviations?)
// in order to make the text sound better when run through a Text-to-
// Speech system.  We try to change only normal amateur callsigns.
// If we find a number in the text before a dash is found, we
// consider it to be a normal callsign.  We don't add commas to the
// SSID portion of a call.
void spell_it_out(char *text, int max_length)
{
  char buffer[2000];
  int i = 0;
  int j = 0;
  int number_found_before_dash = 0;
  int dash_found = 0;

  while (text[i] != '\0')
  {

    if (text[i] == '-')
    {
      dash_found++;
    }

    if (is_num_chr(text[i]) && !dash_found)
    {
      number_found_before_dash++;
    }

    buffer[j++] = text[i];

    if (!dash_found) // Don't add commas to SSID
    {
      buffer[j++] = ',';
    }

    i++;
  }
  buffer[j] = '\0';

  // Only use the new string if it kind'a looks like a callsign
  if (number_found_before_dash)
  {
    xastir_snprintf(text, max_length, "%s", buffer);
  }
}





#define kKey 0x73e2 // This is the seed for the key





static short doHash(char *theCall)
{
  char rootCall[10]; // need to copy call to remove ssid from parse
  char *p1 = rootCall;
  short hash;
  short i,len;
  char *ptr = rootCall;

  while ((*theCall != '-') && (*theCall != '\0'))
  {
    *p1++ = toupper((int)(*theCall++));
  }
  *p1 = '\0';

  hash = kKey; // Initialize with the key value
  i = 0;
  len = (short)strlen(rootCall);

  while (i<len)  // Loop through the string two bytes at a time
  {
    hash ^= (unsigned char)(*ptr++)<<8; // xor high byte with accumulated hash
    hash ^= (*ptr++); // xor low byte with accumulated hash
    i += 2;
  }
  return (short)(hash & 0x7fff); // mask off the high bit so number is always positive
}





short checkHash(char *theCall, short theHash)
{
  return (short)(doHash(theCall) == theHash);
}





// Breaks up a string into substrings using an arbitrary character
// as the delimiter.  Makes each entry in the array of char ptrs
// point to one substring.  Modifies incoming string and cptr[]
// array.  Send a character constant string to it and you'll get an
// instant segfault (the function can't modify a char constant
// string).  Use this function instead of strtok().
//
void split_string( char *data, char *cptr[], int max, char search_char )
{
  int ii;
  char *temp;
  char *current = data;


  // NULL each char pointer
  for (ii = 0; ii < max; ii++)
  {
    cptr[ii] = NULL;
  }

  // Save the beginning substring address
  cptr[0] = current;

  for (ii = 1; ii < max; ii++)
  {
    temp = strchr(current,search_char);  // Find next search character

    if(!temp)   // No search characters found
    {
      return; // All done with string
    }

    // Store pointer to next substring in array
    cptr[ii] = &temp[1];
    current  = &temp[1];

    // Overwrite search character  with end-of-string char and bump
    // pointer by one.
    temp[0] = '\0';
  }
}





// Function to check for proper relations between the n-N
// numbers.  Called from check_unproto_path() function below.
//
int check_n_N (int prev, int last)
{
  int bad_path = 0;


  if (prev < last)
  {
    // Whoa, n < N, not good
    bad_path = 1;
  }
  else if (last > MAX_WIDES)
  {
    // N is greater than MAX_WIDES
    bad_path = 1;
  }
  else if (prev > MAX_WIDES)
  {
    // n is greater than MAX_WIDES
    bad_path = 1;
  }
  else if (last == 0)
  {
    // N is 0, it's a used-up digipeater slot
    bad_path = 1;
  }
  if (debug_level & 1 && bad_path)
  {
    fprintf(stderr,"n-N wrong\n");
  }

  return(bad_path);
}





// This function checks to make sure an unproto path falls within
// the socially acceptable values, such as only one RELAY or
// WIDE1-1 which may appear only as the first option, use of WIDE4-4
// and higher should be questioned, etc.
//
// "MAX_WIDES" is defined in util.h
//
// Returns:
//  1 = bad path
//  0 = good path
//
int check_unproto_path ( char *data )
{
  char *tmpdata;
  char *tmp;
  char *ViaCalls[10];
  int bad_path, ii, have_relay, have_wide, have_widen;
  int have_trace, have_tracen;
  int lastp = 0;
  int prevp = 0;
  int last = 0;
  int prev = 0;
  int total_digi_length = 0;


  if (debug_level & 1)
  {
    fprintf(stderr, "Input string: %s\n", data);
  }

  bad_path = ii = have_relay = have_wide = 0;
  have_widen = have_trace = have_tracen = 0;

  // Remember to free() tmpdata before we return
#ifdef HAVE_STRNDUP
  tmpdata = (char *)strndup(data, strlen(data));
#else
  tmpdata = (char *)strdup(data);
#endif
  (void)to_upper(tmpdata);
  if ((tmp = strchr(tmpdata, '/')))
  {
    *tmp ='\0';  // Only check VHF portion of path
  }
  split_string(tmpdata, ViaCalls, 10, ',');

  for (ii = 0; ii < 10; ii++)
  {
    lastp = 0;
    prevp = 0;
    last = 0;
    prev = 0;


    // Check for empty string in this slot
    if (ViaCalls[ii] == NULL)
    {
      // We're done.  Exit the loop with whatever flags were
      // set in previous iterations.
      if (debug_level & 1)
      {
        fprintf(stderr,"NULL string, slot %d\n", ii);
      }
      break;
    }
    else
    {
      if (debug_level & 1)
      {
        fprintf(stderr,"%s string, slot %d\n", ViaCalls[ii], ii);
      }
    }

    lastp = strlen(ViaCalls[ii]) - 1;
    prevp = lastp - 2;
    // Snag the N character, convert to integer
    last  = ViaCalls[ii][lastp] - 0x30;

    if (prevp >= 0)
    {
      // Snag the n character, convert to integer
      prev = ViaCalls[ii][prevp] - 0x30;
    }

    // Check whether RELAY appears later in the path
    if (!strncmp(ViaCalls[ii], "RELAY", 5))
    {
      have_relay++;
      total_digi_length++;
      if (ii != 0)
      {
        // RELAY should not appear after the first item in a
        // path!
        bad_path = 1;
        if (debug_level & 1)
        {
          fprintf(stderr,"RELAY appears later in the path\n");
        }
        break;
      }
    }

    // Check whether WIDE1-1 appears later in the path (the new
    // "RELAY")
    else if (!strncmp(ViaCalls[ii], "WIDE1-1", 7))
    {
      have_relay++;
      total_digi_length++;
      if (ii != 0)
      {
        // WIDE1-1 should not appear after the first item in
        // a path!
        bad_path = 1;
        if (debug_level & 1)
        {
          fprintf(stderr,"WIDE1-1 appears later in the path\n");
        }
        break;
      }
    }

    // Check whether WIDE2-1 appears first in the path.  This is
    // fine, but don't trigger an error later because of another
    // WIDEn-N after an initial WIDE2-1.  This is to allow paths
    // like "WIDE2-1,WIDE-2-2" or "WIDE2-1,MD2-2"
    else if ( (ii == 0) && (!strncmp(ViaCalls[ii], "WIDE2-1", 7)) )
    {
      total_digi_length++;
      if (debug_level & 1)
      {
        fprintf(stderr,"Found initial WIDE2-1 (a good thing)\n");
      }
    }

    // Check for WIDE/TRACE/WIDEn-N/TRACEn-N in the path
    else if (strstr(ViaCalls[ii], "WIDE") || strstr(ViaCalls[ii], "TRACE"))
    {
      // This is some variant of WIDE or TRACE, could be
      // WIDE/WIDEn-N/TRACE/TRACEn-N
      int is_wide = 0;


      if (strstr(ViaCalls[ii], "WIDE"))
      {
        is_wide++;
      }

      // Check for WIDEn-N or TRACEn-N
      if (strstr(ViaCalls[ii], "-"))
      {
        // This is a WIDEn-N or TRACEn-N callsign


        // Here we are adding the unused portion of the
        // WIDEn-N/TRACEn-N to the total_digi_length
        // variable.  We use the unused portion because that
        // way we're not fooled if people start with a
        // number for N that is higher/lower than n.  The
        // initial thought was to grab the higher of n or N,
        // but those lines are commented out here.
        //
        //if (last > prev)
        total_digi_length += last;
        //else
        //    total_digi_length += prev;


        // Check for previous WIDEn-N/TRACEn-N
        if (have_widen || have_tracen)
        {
          // Already have a large area via
          bad_path = 1;
          if (debug_level & 1)
          {
            fprintf(stderr,"Previous WIDEn-N or TRACEn-N\n");
          }
          break;
        }

        // Perform WIDEn-N checks
        if (is_wide)
        {
          if (debug_level & 1)
          {
            fprintf(stderr,"Found wideN-n at slot %d\n", ii);
          }

          if (strcmp(ViaCalls[ii], "WIDE1-1") !=0) // Home station, RELAY replacement
          {
            have_widen++;  // Note: We mark "have_relay" for
          }
          // "WIDE1-1" instead of "have_widen"

          // We know its a WIDEn-N, time to find out what n is
          if (strlen(ViaCalls[ii]) != 7)
          {
            // This should be WIDEn-N and should be
            // exactly 7 characters
            bad_path = 1;
            if (debug_level & 1)
            {
              fprintf(stderr,"String length of widen-N is not 7 characters, slot %d\n", ii);
            }
            break;
          }

          // Check for proper relations between the n-N
          // numbers.
          if ( check_n_N(prev, last) )
          {
            bad_path = 1;
            if (debug_level & 1)
            {
              fprintf(stderr,"In WIDEn-N checks, slot %d\n", ii);
            }
            break;
          }
        }

        // Perform similar checks for TRACEn-N
        else
        {
          if (debug_level & 1)
          {
            fprintf(stderr,"Found traceN-n, slot %d\n", ii);
          }
          have_tracen++;

          // We know its a TRACEn-N time to find out what
          // n is
          if (strlen(ViaCalls[ii]) != 8)
          {
            // This should be TRACEn-N and should be
            // exactly 8 characters
            bad_path = 1;
            if (debug_level & 1)
            {
              fprintf(stderr,"String length of tracen-N is not 8 characters, slot %d\n", ii);
            }
            break;
          }

          // Check for proper relations between the n-N
          // numbers.
          if ( check_n_N(prev, last) )
          {
            bad_path = 1;
            if (debug_level & 1)
            {
              fprintf(stderr,"In TRACEn-N checks, slot %d\n", ii);
            }
            break;
          }
        }

      }

      else
      {
        // We know we have a WIDE or TRACE in this callsign slot
        total_digi_length++;
        if (is_wide)
        {
          have_wide++;
        }
        else
        {
          have_trace++;
        }

        if (have_relay && (ii > MAX_WIDES))
        {
          // RELAY and more than "MAX_WIDES" WIDE/TRACE calls
// Here we could check have_wide/have_trace to see what the actual
// count of WIDE/TRACE calls is at this point...
          bad_path = 1;
          if (debug_level & 1)
          {
            fprintf(stderr,"Have relay and ii > MAX_WIDES\n");
          }
          break;
        }
        else if (!have_relay && ii > (MAX_WIDES - 1))
        {
          // More than WIDE,WIDE,WIDE or TRACE,TRACE,TRACE
// Here we could check have_wide/have_trace to see what the actual
// count of WIDE/TRACE calls is at this point...
          bad_path = 1;
          if (debug_level & 1)
          {
            fprintf(stderr,"No relay, but ii > MAX_WIDES-1\n");
          }
          break;
        }
        else if (have_widen || have_tracen)
        {
          // WIDE/TRACE after something other than RELAY
          // or WIDE1-1
          bad_path = 1;
          if (debug_level & 1)
          {
            fprintf(stderr,"WIDE or TRACE after something other than RELAY or WIDE1-1\n");
          }
          break;
        }
      }
    }

    // Not RELAY/WIDE/TRACE/WIDEn-N/TRACEn-N call
    else
    {
      // Might as well stub this out, could be anything at
      // this point, a LINKn-N or LANn-N or a explicit
      // callsign
      if (!strstr(ViaCalls[ii], "-"))
      {
        /*
                        // We do not have an SSID, treat it as a RELAY
                        if (have_relay) {
                            bad_path = 1;
                            if (debug_level & 1)
                                fprintf(stderr,"No SSID\n");
                            break;
                        }
        */

        have_relay++;
      }

      else
      {
        // Could be a LAN or LINK or explicit call, check
        // for a digit preceding the dash

        if (prev > 0 && prev <= 9)      // Found a digit
        {
          // We've found an n-N */
          if (have_widen || have_tracen)
          {
            // Already have a previous wide path
            bad_path = 1;
            if (debug_level & 1)
            {
              fprintf(stderr,"Found n-N and previous WIDEn-N or TRACEn-N\n");
            }
            break;
          }

          // Check for proper relations between the n-N
          // numbers.
          if ( check_n_N(prev, last) )
          {
            bad_path = 1;
            if (debug_level & 1)
            {
              fprintf(stderr,"In OTHER checks\n");
            }
            break;
          }

          if (debug_level & 1)
          {
            fprintf(stderr,"Found wideN-n, slot %d\n", ii);
          }
          have_widen++;
        }
        else
        {
          /*
                              // Must be an explicit callsign, treat as relay
                              if (have_relay) {
                                  bad_path = 1;
                                  if (debug_level & 1)
                                      fprintf(stderr,"\n");
                                  break;
                              }
          */
          have_relay++;
        }
      }
    }
  }   // End of for loop

  if (debug_level & 1)
  {
    fprintf(stderr,"Total digi length: %d\n", total_digi_length);
  }

  if (total_digi_length > MAX_WIDES + 1)
  {

    if (debug_level & 1)
    {
      fprintf(stderr,"Total digi count is too high!\n");
    }

    bad_path = 1;
  }

  // Free the memory we allocated with strndup()
  free(tmpdata);
  return(bad_path);

}   // End of check_unproto_path





// Set string printed out by segfault handler
void set_dangerous( char *ptr )
{
  xastir_snprintf(dangerous_operation,
                  sizeof(dangerous_operation),
                  "%s",
                  ptr);
}





// Clear string printed out by segfault handler
void clear_dangerous(void)
{
  dangerous_operation[0] = '\0';
}





// Write out a WKT file
void xastirWriteWKT(char *filename)
{
  // This "WKT" string describes the coordinate system we use in Xastir.
  // We'll use this string to write out ".prj" files to associate with
  // shapefiles we create from GPS or APRS tracks.
  char Xastir_WKT[] = "GEOGCS[\"GCS_WGS_1984\",DATUM[\"D_WGS_1984\",SPHEROID[\"WGS_84\",6378137,298.257223563]],PRIMEM[\"Greenwich\",0],UNIT[\"Degree\",0.017453292519943295]]";

  FILE *f;

  f=fopen(filename,"w"); // open for write
  if (f != NULL)
  {
    fprintf(f,"%s\n",Xastir_WKT);
    fclose(f);
  }
  else
  {
    fprintf(stderr,"Could not open file %s for writing\n",filename);
  }
}





// makeMultiline
// Create an APRS multiline string given an array of lat/lon pairs.
//
// Allocates memory that must be freed by the caller.
//
// lon and lat are arrays.  lonObj, latObj are return values of object
// location (point from which offsets are computed).
//
// lineType is 0 for a closed polygon, 1 for a polyline
//
// colorStyle is a character as defined in the wxsvr.net multiline protocol
// web site at wxsvr.net.
//
// character | color | style
//   a          red     solid
//   b          red     dashed
//   c          red     double-dashed
//   d          yellow  solid
//   e          yellow  dashed
//   f          yellow  double-dashed
//   g          blue    solid
//   h          blue    dashed
//   i          blue    double-dashed
//   j          green   solid
//   k          green   dashed
//   l          green   double-dashed

// Returns a null pointer if user requested too many vertices, or if scale
// is out of range, or if we fail to malloc the string.
//
// One could pass only a list of lat/lons here and get back a point at which
// to create an object (at the centroid) and a string representing the
// multiline.
#define minFun(a,b) ( ((a)<(b))?(a):(b))
#define maxFun(a,b) ( ((a)>(b))?(a):(b))

char * makeMultiline(int numPairs, double *lon, double *lat, char colorStyle,
                     int lineType, char* sqnc,
                     double *lonCentr, double *latCentr  )
{

  char * returnString;

  // the APRS spec requires a max of 43 chars in the comment section of
  // objects, which leaves room for only so many vertices in a multiline
  //   number allowed= (43-(6-5))/2=16
  // 43chars - 6 for the sequence number- 5 for the starting pattern leaves
  // 32 characters for lat/lon pairs, or 16 pairs

  if ( numPairs > 16)
  {
    returnString = NULL;
  }
  else
  {
    double minLat, minLon;
    double maxLat, maxLon;
    int iPair;
    double scale1,scale2,scale;

    // find min/max of arrays
    minLat=minLon=180;
    maxLat=maxLon=-180;

    for ( iPair=0; iPair < numPairs; iPair++)
    {
      minLon = minFun(minLon,lon[iPair]);
      minLat = minFun(minLat,lat[iPair]);
      maxLon = maxFun(maxLon,lon[iPair]);
      maxLat = maxFun(maxLat,lat[iPair]);
    }

    *lonCentr = (maxLon+minLon)/2;
    *latCentr = (maxLat+minLat)/2;

    // Compute scale:
    // The scale is the value that makes the maximum or minimum offset
    // map to +44 or -45.  Pick the scale factor that keeps the
    // offsets in that range:

    if (maxLat > maxLon)
    {
      scale1= (maxLat-*latCentr)/44.0;
    }
    else
    {
      scale1= (maxLon-*lonCentr)/44.0;
    }

    if (minLat < minLon)
    {
      scale2 = (minLat-*latCentr)/(-45.0);
    }
    else
    {
      scale2 = (minLon-*lonCentr)/(-45.0);
    }

    scale = maxFun(scale1,scale2);

    if (scale < .0001)
    {
      scale=0.0001;
    }

    if (scale > 1)
    {
      // Out of range, no shape returned
      returnString = NULL;
    }
    else
    {
      // Not all systems have a log10(), but they all have log()
      // So let's stick with natural logs
      double ln10=log(10.0);
      // KLUDGE:  the multiline spec says we use
      // 20*(int)(log10(scale/.0001)) to generate the scale char,
      // but this means we'll often produce real scales that are smaller
      // than the one we just calculated, which means we'd produce
      // offsets outside the (-45,44) allowed range.  So kludge and
      // add 1 to the value
      int lnscalefac=20*log(scale/.0001)/ln10+1;
      int rsMaxLen=numPairs*2+6+4+1;
      int stringOffset=0;

      // Now recompute the scale to be the one we actually transmitted
      // This pretty much means we'll never have the best precision
      // we could possibly have, but it'll be close enough
      scale=pow(10,(double)lnscalefac/20-4);

      // We're ready to produce the multiline string.  So get on with it

      // multiline string is "}CTS" (literal "}" followed by
      // line Color-style specifier, followed by open/closed
      // Type specifier, followed by Scale character), followed
      // by even number of character pairs, followed by "{seqnc"
      // (sequence number).


      returnString=malloc(sizeof(char)*rsMaxLen);

      if (returnString != NULL)
      {
        returnString[stringOffset++]=' ';
        returnString[stringOffset++]='}';
        returnString[stringOffset++]=colorStyle;
        returnString[stringOffset++]= (lineType == 0)?'0':'1';

        returnString[stringOffset++] = lnscalefac+33;

        for ( iPair=0; iPair<numPairs; ++iPair)
        {
          double latOffset=lat[iPair]-*latCentr;
          // the wxsvr protocol is Western Hemisphere-Centric,
          // and treats positive offsets in longitude as being
          // west of the reference point.  So have to reverse
          // the sense of direction here.
          // This will yield positive offsets if lonCenter is
          // negative (west) and lon[iPair] is more negative
          // (more west)
          double lonOffset=*lonCentr-lon[iPair];

          returnString[stringOffset++]=
            (char)((int)(latOffset/scale)+78);
          returnString[stringOffset++]=
            (char)((int)(lonOffset/scale)+78);
        }
        returnString[stringOffset++]='{';
        strncpy(&(returnString[stringOffset]),sqnc,6);
        returnString[rsMaxLen-1] = '\0'; // Just to be safe
      }
    }
  }
  return (returnString);
}


