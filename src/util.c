/* -*- c-basic-indent: 4; indent-tabs-mode: nil -*-
 * $Id$
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2003  The Xastir Group
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


#include "config.h"
#include "snprintf.h"

#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <ctype.h>
#include <math.h>

#include "xastir.h"
#include "util.h"
#include "db.h"
#include "main.h"
#include "xa_config.h"
#include "datum.h"


#ifdef HAVE_DMALLOC
#include <dmalloc.h>
#endif


// For mutex debugging with Linux threads only
#ifdef MUTEX_DEBUG
#include <asm/errno.h>
extern int pthread_mutexattr_setkind_np(pthread_mutexattr_t *attr, int kind);
#endif


int position_amb_chars;
char echo_digis[6][9+1];

#define ACCEPT_0N_0E    /* set this to see stations at 0N/0E on the map */

struct timeval timer_start;
struct timeval timer_stop;
struct timezone tz;





// Save the current time, used for timing code sections.
void start_timer(void) {
    gettimeofday(&timer_start,&tz);
}





// Save the current time, used for timing code sections.
void stop_timer(void) {
    gettimeofday(&timer_stop,&tz);
}





// Print the difference in the two times saved above.
void print_timer_results(void) {
printf("Total: %f sec\n",
    (float)(timer_stop.tv_sec - timer_start.tv_sec +
    ((timer_stop.tv_usec - timer_start.tv_usec) / 1000000.0) ));
}





//
// Inserts localtime date/time in "timestring".  Timestring
// Should be at least 101 characters long.
//
void get_timestamp(char *timestring) {
    struct tm *time_now;
    time_t secs_now;


    secs_now=sec_now();
    time_now = localtime(&secs_now);
    (void)strftime(timestring,100,"%a %b %e %T %Z %Y",time_now);
}





/***********************************************************/
/* returns the hour (00..23), localtime                    */
/***********************************************************/
int get_hours(void) {
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
int get_minutes(void) {
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
int get_seconds(void) {
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
char *output_lat(char *in_lat, int comp_pos) {
    int i,j;

    if (!comp_pos)
        in_lat[7]=in_lat[8]; /*Shift N/S down for transmission */
    else if (position_amb_chars>0)
        in_lat[7]='0';

    j=0;
    if (position_amb_chars>0 && position_amb_chars<5) {
        for (i=6;i>(6-position_amb_chars-j);i--) {
            if (i==4) {
                i--;
                j=1;
            }
            if (!comp_pos) {
                in_lat[i]=' ';
            } else
                in_lat[i]='0';
        }
    }

    if (!comp_pos) {
        in_lat[8] = '\0';
    }

    return(in_lat);
}





/**************************************************************************/
/* output_long - format position with position_amb_chars for transmission */
/**************************************************************************/
char *output_long(char *in_long, int comp_pos) {
    int i,j;

    if (!comp_pos)
        in_long[8]=in_long[9]; /*shift e/w down for transmission */
    else if (position_amb_chars>0)
        in_long[8]='0';

    j=0;
    if (position_amb_chars>0 && position_amb_chars<5) {
        for (i=7;i>(7-position_amb_chars-j);i--) {
            if (i==5) {
                i--;
                j=1;
            }
            if (!comp_pos) {
                in_long[i]=' ';
            } else
                in_long[i]='0';
        }
    }

    if (!comp_pos)
        in_long[9] = '\0';

    return(in_long);
}





/*********************************************************************/
/* PHG range calculation                                             */
/* NOTE: Keep these calculations consistent with phg_decode!         */
/*       Yes, there is a reason why they both exist.                 */
/*********************************************************************/
double phg_range(char p, char h, char g) {
    double power, height, gain, range;

    if ( (p < '0') || (p > '9') )   // Power is outside limits
        power = 0.0;
    else
        power = (double)( (p-'0')*(p-'0') );    // lclint said: "Assignment of char to double" here

    if (h < '0')   // Height is outside limits (there is no upper limit according to the spec)
        height = 10.0;
    else
        height = 10.0 * pow(2.0, (double)(h-'0'));

    if ( (g < '0') || (g > '9') )   // Gain is outside limits
        gain = 1.0;
    else
        gain = pow(10.0, (double)(g-'0') / 10.0);

    range = sqrt(2.0 * height * sqrt((power / 10.0) * (gain / 2.0)));

//    if (range > 70.0)
//        printf("PHG%c%c%c results in range of %f\n", p, h, g, range);

    // Note:  Bob Bruninga, WB4APR, decided to cut PHG circles by
    // 1/2 in order to show more realistic mobile ranges.
    range = range / 2.0;

    return(range);
}





/*********************************************************************/
/* shg range calculation (for DF'ing)                                */
/*                                                                   */
/*********************************************************************/
double shg_range(char s, char h, char g) {
    double power, height, gain, range;

    if ( (s < '0') || (s > '9') )   // Power is outside limits
        s = '0';

    if (s == '0')               // No signal strength
        power = 10.0 / 0.8;     // Preventing divide by zero (same as DOSaprs does it)
    else
        power = 10 / (s - '0'); // Makes circle smaller with higher signal strengths

    if (h < '0')   // Height is outside limits (there is no upper limit according to the spec)
        height = 10.0;
    else
        height = 10.0 * pow(2.0, (double)(h-'0'));

    if ( (g < '0') || (g > '9') )   // Gain is outside limits
        gain = 1.0;
    else
        gain = pow(10.0, (double)(g-'0') / 10.0);

    range = sqrt(2.0 * height * sqrt((power / 10.0) * (gain / 2.0)));

    range = (range * 40) / 51;   // Present fudge factors used in DOSaprs

    //printf("SHG%c%c%c results in range of %f\n", s, h, g, range);

    return(range);
}





/*********************************************************************/
/* PHG decode                                                        */
/* NOTE: Keep these calculations consistent with phg_range!          */
/*       Yes, there is a reason why they both exist.                 */
/*********************************************************************/
void phg_decode(const char *langstr, const char *phg, char *phg_decoded, int phg_decoded_length) {
    double power, height, gain, range;
    char directivity[6], temp[64];
    int  gain_db;
    int len;

    len = strlen(phg_decoded);

    if (strlen(phg) != 7) {
        xastir_snprintf(phg_decoded, phg_decoded_length, langstr, "BAD PHG");
        return;
    }

    if ( (phg[3] < '0') || (phg[3] > '9') )   // Power is outside limits
        power = 0.0;
    else
        power = (double)( (phg[3]-'0')*(phg[3]-'0') );

    if (phg[4] < '0')   // Height is outside limits (there is no upper limit according to the spec)
        height = 10.0;
    else
        height = 10.0 * pow(2.0, (double)(phg[4]-'0'));

    if ( (phg[5] < '0') || (phg[5] > '9') ) { // Gain is outside limits
        gain = 1.0;
        gain_db = 0;
    }
    else {
        gain = pow(10.0, (double)(phg[5]-'0') / 10.0);
        gain_db = phg[5]-'0';
    }

    range = sqrt(2.0 * height * sqrt((power / 10.0) * (gain / 2.0)));
    // Note:  Bob Bruninga, WB4APR, decided to cut PHG circles by
    // 1/2 in order to show more realistic mobile ranges.
    range = range / 2.0;

    switch (phg[6]) {
        case '0':
            xastir_snprintf(directivity, sizeof(directivity), "%s", " omni");
            break;
        case '1':
            xastir_snprintf(directivity, sizeof(directivity), "%s", " NE");
            break;
        case '2':
            xastir_snprintf(directivity, sizeof(directivity), "%s", " E");
            break;
        case '3':
            xastir_snprintf(directivity, sizeof(directivity), "%s", " SE");
            break;
        case '4':
            xastir_snprintf(directivity, sizeof(directivity), "%s", " S");
            break;
        case '5':
            xastir_snprintf(directivity, sizeof(directivity), "%s", " SW");
            break;
        case '6':
            xastir_snprintf(directivity, sizeof(directivity), "%s", " W");
            break;
        case '7':
            xastir_snprintf(directivity, sizeof(directivity), "%s", " NW");
            break;
        case '8':
            xastir_snprintf(directivity, sizeof(directivity), "%s", " N");
            break;
        default:
            directivity[0] = '\0';  break;
    }

    if (units_english_metric)
        xastir_snprintf(temp, sizeof(temp), "%.0fW @ %.0fft HAAT, %ddB%s, range %.1fmi", power, height, gain_db, directivity, range);
    else
        xastir_snprintf(temp, sizeof(temp), "%.0fW @ %.1fm HAAT, %ddB%s, range %.1fkm", power, height*0.3048, gain_db, directivity, range*1.609344);

    xastir_snprintf(phg_decoded, phg_decoded_length, langstr, temp);
}





/*********************************************************************/
/* SHG decode                                                        */
/*                                                                   */
/*********************************************************************/
void shg_decode(const char *langstr, const char *shg, char *shg_decoded, int shg_decoded_length) {
    double power, height, gain, range;
    char directivity[6], temp[80], signal[64];
    int  gain_db;
    char s;
    int len;

    len = strlen(shg_decoded);
    if (strlen(shg) != 7) {
        xastir_snprintf(shg_decoded, shg_decoded_length, langstr, "BAD SHG");
        return;
    }

    s = shg[3];

    if ( (s < '0') || (s > '9') )   // Signal is outside limits
        s = '0';                    // force to lowest legal value

    switch (s) {
        case '0':
            xastir_snprintf(signal, sizeof(signal), "%s", "No signal detected");
            break;
        case '1':
            xastir_snprintf(signal, sizeof(signal), "%s", "Detectible signal (Maybe)");
            break;
        case '2':
            xastir_snprintf(signal, sizeof(signal), "%s", "Detectible signal but not copyable)");
            break;
        case '3':
            xastir_snprintf(signal, sizeof(signal), "%s", "Weak signal, marginally readable");
            break;
        case '4':
            xastir_snprintf(signal, sizeof(signal), "%s", "Noisy but copyable signal");
            break;
        case '5':
            xastir_snprintf(signal, sizeof(signal), "%s", "Some noise, easy to copy signal");
            break;
        case '6':
            xastir_snprintf(signal, sizeof(signal), "%s", "Good signal w/detectible noise");
            break;
        case '7':
            xastir_snprintf(signal, sizeof(signal), "%s", "Near full-quieting signal");
            break;
        case '8':
            xastir_snprintf(signal, sizeof(signal), "%s", "Full-quieting signal");
            break;
        case '9':
            xastir_snprintf(signal, sizeof(signal), "%s", "Extremely strong & full-quieting signal");
            break;
        default:
            signal[0] = '\0';
            break;
    }

    if (s == '0')
        power = (double)( 10 / 0.8 );   // Preventing divide by zero (same as DOSaprs does it)
    else
        power = (double)( 10 / (s - '0') );

    if (shg[4] < '0')   // Height is outside limits (there is no upper limit according to the spec)
        height = 10.0;
    else
        height = 10.0 * pow(2.0, (double)(shg[4]-'0'));

    if ( (shg[5] < '0') || (shg[5] > '9') ) { // Gain is outside limits
        gain = 1.0;
        gain_db = 0;
    } else {
        gain = pow(10.0, (double)(shg[5]-'0') / 10.0);
        gain_db = shg[5]-'0';
    }

    range = sqrt(2.0 * height * sqrt((power / 10.0) * (gain / 2.0)));

    range = range * 0.85;   // Present fudge factor (used by DOSaprs)

    switch (shg[6]) {
        case '0':
            xastir_snprintf(directivity, sizeof(directivity), "%s", " omni");
            break;
        case '1':
            xastir_snprintf(directivity, sizeof(directivity), "%s", " NE");
            break;
        case '2':
            xastir_snprintf(directivity, sizeof(directivity), "%s", " E");
            break;
        case '3':
            xastir_snprintf(directivity, sizeof(directivity), "%s", " SE");
            break;
        case '4':
            xastir_snprintf(directivity, sizeof(directivity), "%s", " S");
            break;
        case '5':
            xastir_snprintf(directivity, sizeof(directivity), "%s", " SW");
            break;
        case '6':
            xastir_snprintf(directivity, sizeof(directivity), "%s", " W");
            break;
        case '7':
            xastir_snprintf(directivity, sizeof(directivity), "%s", " NW");
            break;
        case '8':
            xastir_snprintf(directivity, sizeof(directivity), "%s", " N");
            break;
        default:
            directivity[0] = '\0';  break;
    }

    if (units_english_metric)
        xastir_snprintf(temp, sizeof(temp), "%.0fft HAAT, %ddB%s, DF Range: %.1fmi, %s", height, gain_db, directivity, range, signal);
    else
        xastir_snprintf(temp, sizeof(temp), "%.1fm HAAT, %ddB%s, DF Range: %.1fkm, %s", height*0.3048, gain_db, directivity, range*1.609344, signal);

    xastir_snprintf(shg_decoded, shg_decoded_length, langstr, temp);
}





/*********************************************************************/
/* Bearing decode                                                    */
/*                                                                   */
/*********************************************************************/
void bearing_decode(const char *langstr, const char *bearing_str,
        const char *NRQ, char *bearing_decoded, int bearing_decoded_length) {
    int bearing, range, width;
    char N,R,Q;
    char temp[64];
    int len;

    //printf("bearing_decode incoming: bearing is %s, NRQ is %s\n", bearing_str, NRQ);
    len = strlen(bearing_decoded);

    if (strlen(bearing_str) != 3) {
        xastir_snprintf(bearing_decoded, bearing_decoded_length, langstr, "BAD BEARING");
        return;
    }

    if (strlen(NRQ) != 3) {
        xastir_snprintf(bearing_decoded, bearing_decoded_length, langstr, "BAD NRQ");
        return;
    }

    bearing = atoi(bearing_str);
    N = NRQ[0];
    R = NRQ[1];
    Q = NRQ[2];

    range = 0;
    width = 0;

    if (N != 0) {

        //printf("N != 0\n");

        range = (int)( pow(2.0,R - '0') );

        switch (Q) {
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

        //printf("Width = %d\n",width);

        if (units_english_metric) {
            xastir_snprintf(temp, sizeof(temp), "%i°, DF Beamwidth: %i°, DF Length: %i mi", bearing, width, range);
        } else {
            xastir_snprintf(temp, sizeof(temp), "%i°, DF Beamwidth: %i°, DF Length: %0.2f km", bearing, width, range*1.609344);
        }
    } else {
        xastir_snprintf(temp, sizeof(temp), "%s", "Not Valid");
        //printf("N was 0\n");
    }
    xastir_snprintf(bearing_decoded, bearing_decoded_length, langstr, temp);
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
                        printf("get_line: found control chars in: %s\n",
                            temp_line);
                    }
                }
                temp_line[i] = '\0';    // Terminate the string
            }
        }
    }

    xastir_snprintf(linedata,maxline,"%s",temp_line);

    return(linedata);
}
*/
char *get_line(FILE *f, char *linedata, int maxline) {
    int length;

    // Write terminating chars throughout variable
    memset(linedata,0,maxline);

    // Get the data
    (void)fgets(linedata, maxline, f);

    // Change CR/LF to '\0'
    length = strlen(linedata);

    // Check the last two characters
    if (length > 0) {
        if ( (linedata[length - 1] == '\n')
                || (linedata[length - 1] == '\r') ) {
            linedata[length - 1] = '\0';
        }
    }

    if (length > 1) {
        if ( (linedata[length - 2] == '\n')
                || (linedata[length - 2] == '\r') ) {
            linedata[length - 2] = '\0';
        }
    }

    return(linedata);
}





char *new_get_line(FILE *f, char *linedata, int maxline) {
    char *ptr;
    int pos;
    static char Buffer[4097];

    pos=0;
    *linedata = '\0';
    while((ptr = strpbrk(Buffer, "\r\n")) == NULL && !feof(f) && strlen(Buffer) <= 4096)
        (void)fread(&Buffer[strlen(Buffer)], sizeof(char), sizeof(Buffer)-strlen(Buffer)-1, f);

    if (ptr)
        *ptr = '\0';

    strncpy(linedata, Buffer, (size_t)maxline);
    if ((pos = (int)strlen(Buffer)) > maxline)
        fprintf(stderr, "Line length overflow: get_line\n");
    else if (pos < maxline)
        linedata[pos] = '\0';

    if (ptr) {
        for (ptr++; *ptr == '\r' || *ptr == '\n'; ptr++);
            strcpy(Buffer, ptr);
    }
    return(linedata);
}





time_t time_from_aprsstring(char *aprs_time) {
    int day, hour, minute;
    char tz[20];
    struct tm *time_now, alert_time;
    time_t timenw;
    long zone;

#ifdef USING_SOLARIS
    extern time_t timezone;
#endif


    (void)time(&timenw);
    time_now = localtime(&timenw);
#ifdef __FreeBSD__
    zone = (time_now->tm_gmtoff) - 3600 * (int)(time_now->tm_isdst);
#else
    zone = (int)timezone - 3600 * (int)(time_now->tm_isdst > 0);
#endif
    tz[0] = tz[1] = '\0';
    switch (sscanf(aprs_time, "%2d%2d%2d%19s", &day, &hour, &minute, tz)) {
        case 0:
            day = 0;

        case 1:
            hour = 0;

        case 2:
            minute = 0;

        default:
            break;
    }
    alert_time = *time_now;
    alert_time.tm_sec = 0;
    if (day) {
        alert_time.tm_mday = day;
        if (day < (time_now->tm_mday - 10)) {
            if (++alert_time.tm_mon > 11) {
                alert_time.tm_mon = 0;
                alert_time.tm_year++;
            }
        }
        if (day > (time_now->tm_mday + 10)) {
            if (--alert_time.tm_mon < 0) {
                alert_time.tm_mon = 11;
                alert_time.tm_year--;
            }
        }
        alert_time.tm_min = minute;
        alert_time.tm_hour = hour;
        if ((char)tolower((int)tz[0]) == 'z') {
            alert_time.tm_hour -= zone/3600;
            zone %= 3600;
            if (zone)
                alert_time.tm_min += zone/60;

            if (alert_time.tm_min > 60) {
                alert_time.tm_hour++;
                alert_time.tm_min -= 60;
            }
            if (alert_time.tm_hour > 23) {
                alert_time.tm_hour -= 24;
                alert_time.tm_mday++;
                if (mktime(&alert_time) == -1) {
                    alert_time.tm_mday = 1;
                    alert_time.tm_mon++;
                    if (mktime(&alert_time) == -1) {
                        alert_time.tm_mon = 0;
                        alert_time.tm_year++;
                    }
                }
            } else if (alert_time.tm_hour < 0) {
                alert_time.tm_hour += 24;
                if (--alert_time.tm_mday <= 0) {
                    if (--alert_time.tm_mon < 0) {
                        alert_time.tm_year--;
                        alert_time.tm_mon = 11;
                        alert_time.tm_mday = 31;
                    } else if (alert_time.tm_mon == 3 || alert_time.tm_mon == 5 ||
                                alert_time.tm_mon == 8 || alert_time.tm_mon == 10)
                        alert_time.tm_mday = 30;

                    else if (alert_time.tm_mon == 1)
                        alert_time.tm_mday = (alert_time.tm_year%4 == 0) ? 29: 28;

                    else
                        alert_time.tm_mday = 31;
                }
            }
        }
    }
    else alert_time.tm_year--;
    return(mktime(&alert_time));
}





// Note: last_speed should be in knots.
//
char *compress_posit(const char *input_lat, const char group, const char *input_lon, const char symbol,
            const int last_course, const int last_speed, const char *phg) {
    static char pos[100];
    char lat[5], lon[5];
    char c, s, t, ext;
    int temp, deg;
    double minutes;

    //printf("lat:%s, long:%s, symbol:%c%c, course:%d, speed:%d, phg:%s\n",
    //    input_lat,
    //    input_lon,
    //    group,
    //    symbol,
    //    last_course,
    //    last_speed,
    //    phg);

    (void)sscanf(input_lat, "%2d%lf%c", &deg, &minutes, &ext);
    temp = 380926 * (90 - (deg + minutes / 60.0) * ( ext=='N' ? 1 : -1 ));

    //printf("temp: %d\t",temp);

    lat[3] = (char)(temp%91 + 33); temp /= 91;
    lat[2] = (char)(temp%91 + 33); temp /= 91;
    lat[1] = (char)(temp%91 + 33); temp /= 91;
    lat[0] = (char)(temp    + 33);
    lat[4] = '\0';

    //printf("%s\n",lat);

    (void)sscanf(input_lon, "%3d%lf%c", &deg, &minutes, &ext);
    temp = 190463 * (180 + (deg + minutes / 60.0) * ( ext=='W' ? -1 : 1 ));

    //printf("temp: %d\t",temp);

    lon[3] = (char)(temp%91 + 33); temp /= 91;
    lon[2] = (char)(temp%91 + 33); temp /= 91;
    lon[1] = (char)(temp%91 + 33); temp /= 91;
    lon[0] = (char)(temp    + 33);
    lon[4] = '\0';

    //printf("%s\n",lon);

    // Set up csT bytes for course/speed if either are non-zero
    c = s = t = ' ';
    if (last_course > 0 || last_speed > 0) {

        if (last_course >= 360)
            c = '!';    // 360 would be past 'z'.  Set it to zero.
        else
            c = (char)(last_course/4 + 33);

        s = (char)(log(last_speed + 1.0) / log(1.08) + 0.5);  // Poor man's rounding
        s = (char)(s + 33); // Convert to ASCII
        t = 'C';
    }
    // Else set up csT bytes for PHG if within parameters
    else if (strlen(phg) >= 6) {
        double power, height, gain, range, s_temp;


        c = '{';

        if ( (phg[3] < '0') || (phg[3] > '9') ) { // Power is out of limits
            power = 0.0;
        }
        else {
            power = (double)((int)(phg[3]-'0'));
            power = power * power;  // Lowest possible value is 0.0
        }

        if (phg[4] < '0') // Height is out of limits (no upper limit according to the spec)
            height = 10.0;
        else
            height= 10.0 * pow(2.0,(double)phg[4] - (double)'0');   // Lowest possible value is 10.0

        if ( (phg[5] < '0') || (phg[5] > '9') ) // Gain is out of limits
            gain = 1.0;
        else
            gain = pow(10.0,((double)(phg[5]-'0') / 10.0)); // Lowest possible value is 1.0

        range = sqrt(2.0 * height * sqrt((power / 10.0) * (gain / 2.0)));   // Lowest possible value is 0.0

        // Check for range of 0, and skip log10 if so
        if (range != 0.0)
            s_temp = log10(range/2) / log10(1.08) + 33.0;
        else
            s_temp = 0.0 + 33.0;

        s = (char)(s_temp + 0.5);    // Cheater's way of rounding, add 0.5 and truncate

        t = 'C';
    }
    // Note that we can end up with three spaces at the end if no
    // course/speed/phg were supplied.  Need to knock this down to
    // one space (One space means: Don't use the csT bytes).
    if ( (c == ' ') && (s == ' ') && (t == ' ') )
        xastir_snprintf(pos, sizeof(pos), "%c%s%s%c ", group, lat, lon, symbol);
    else
        xastir_snprintf(pos, sizeof(pos), "%c%s%s%c%c%c%c", group, lat, lon, symbol, c, s, t);

    //printf("New compressed pos: (%s)\n",pos);
    return pos;
}





/*
 * See if position is defined
 * 90°N 180°W (0,0 in internal coordinates) is our undefined position
 * 0N/0E is excluded from trails, could be excluded from map (#define ACCEPT_0N_0E)
 */

int position_defined(long lat, long lon, int strict) {

    if (lat == 0l && lon == 0l)
        return(0);              // undefined location
#ifndef ACCEPT_0N_0E
    if (strict)
#endif
        if (lat == 90*60*60*100l && lon == 180*60*60*100l)      // 0N/0E
            return(0);          // undefined location
    return(1);
}





// Convert Xastir lat/lon to UTM printable string
void convert_xastir_to_UTM_str(char *str, int str_len, long x, long y) {
    double utmNorthing;
    double utmEasting;
    char utmZone[10];
 
    ll_to_utm(E_WGS_84,
        (double)(-((y - 32400000l )/360000.0)),
        (double)((x - 64800000l )/360000.0),
        &utmNorthing,
        &utmEasting,
        utmZone,
        sizeof(utmZone) );
    utmZone[9] = '\0';
    //printf( "%s %07.0f %07.0f\n", utmZone, utmEasting,
    //utmNorthing );
    xastir_snprintf(str, str_len, "%s %07.0f %07.0f",
        utmZone, utmEasting, utmNorthing );
}

// Convert Xastir lat/lon to UTM
void convert_xastir_to_UTM(double *easting, double *northing, char *zone, int zone_len, long x, long y) {
    ll_to_utm(E_WGS_84,
        (double)(-((y - 32400000l )/360000.0)),
        (double)((x - 64800000l )/360000.0),
        northing,
        easting,
        zone,
        zone_len);
    zone[zone_len] = '\0';
}

// Convert UTM to Xastir lat/lon
void convert_UTM_to_xastir(double easting, double northing, char *zone, long *x, long *y) {
    double lat, lon;
    utm_to_ll(E_WGS_84,
        northing,
        easting,
        zone,
        &lat,
        &lon);

    *y = (long)(lat * -360000.0) + 32400000l;
    *x = (long)(lon *  360000.0) + 64800000l;
}




// convert latitude from long to string 
// Input is in Xastir coordinate system
//
// CONVERT_LP_NOSP      = DDMM.MMN
// CONVERT_LP_NORMAL    = DD MM.MMN
// CONVERT_HP_NOSP      = DDMM.MMMN
// CONVERT_UP_TRK       = NDD MM.MMMM
// CONVERT_DEC_DEG      = DD.DDDDDN
// CONVERT_DMS_NORMAL   = DD MM SS.SN
// CONVERT_HP_NORMAL    = DD MM.MMMN
//
void convert_lat_l2s(long lat, char *str, int str_len, int type) {
    char ns;
    float deg, min, sec;
    int ideg, imin;
    long temp;


    strcpy(str,"");
    deg = (float)(lat - 32400000l) / 360000.0;
 
    // Switch to integer arithmetic to avoid floating-point
    // rounding errors.
    temp = (long)(deg * 100000);

    ns = 'S';
    if (temp <= 0) {
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

    switch (type) {
        case(CONVERT_LP_NOSP): /* do low P w/no space */
            xastir_snprintf(str, str_len, "%02d%05.2f%c", ideg, min, ns);
            break;
        case(CONVERT_LP_NORMAL): /* do low P normal */
            xastir_snprintf(str, str_len, "%02d %05.2f%c", ideg, min, ns);
            break;
        case(CONVERT_HP_NOSP): /* do HP w/no space */
            xastir_snprintf(str, str_len, "%02d%06.3f%c", ideg, min, ns);
            break;
        case(CONVERT_UP_TRK): /* for tracklog files */
            xastir_snprintf(str, str_len, "%c%02d %07.4f", ns, ideg, min);
            break;
        case(CONVERT_DEC_DEG):
            xastir_snprintf(str, str_len, "%08.5f%c", ideg+min/60.0, ns);
            break;
        case(CONVERT_DMS_NORMAL):
            xastir_snprintf(str, str_len, "%02d %02d %04.1f%c", ideg, imin, sec, ns);
            break;
        case(CONVERT_HP_NORMAL):

        default: /* do HP normal */
            xastir_snprintf(str, str_len, "%02d %06.3f%c", ideg, min, ns);
            break;
    }
}





// convert longitude from long to string
// Input is in Xastir coordinate system
//
// CONVERT_LP_NOSP      = DDDMM.MME
// CONVERT_LP_NORMAL    = DDD MM.MME
// CONVERT_HP_NOSP      = DDDMM.MMME
// CONVERT_UP_TRK       = EDDD MM.MMMM
// CONVERT_DEC_DEG      = DDD.DDDDDE
// CONVERT_DMS_NORMAL   = DDD MM SS.SN
// CONVERT_HP_NORMAL    = DDD MM.MMME
//
void convert_lon_l2s(long lon, char *str, int str_len, int type) {
    char ew;
    float deg, min, sec;
    int ideg, imin;
    long temp;

    strcpy(str,"");
    deg = (float)(lon - 64800000l) / 360000.0;

    // Switch to integer arithmetic to avoid floating-point rounding
    // errors.
    temp = (long)(deg * 100000);

    ew = 'E';
    if (temp <= 0) {
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

    switch(type) {
        case(CONVERT_LP_NOSP): /* do low P w/nospacel */
            xastir_snprintf(str, str_len, "%03d%05.2f%c", ideg, min, ew);
            break;
        case(CONVERT_LP_NORMAL): /* do low P normal */
            xastir_snprintf(str, str_len, "%03d %05.2f%c", ideg, min, ew);
            break;
        case(CONVERT_HP_NOSP): /* do HP w/nospace */
            xastir_snprintf(str, str_len, "%03d%06.3f%c", ideg, min, ew);
            break;
        case(CONVERT_UP_TRK): /* for tracklog files */
            xastir_snprintf(str, str_len, "%c%03d %07.4f", ew, ideg, min);
            break;
        case(CONVERT_DEC_DEG):
            xastir_snprintf(str, str_len, "%09.5f%c", ideg+min/60.0, ew);
            break;
        case(CONVERT_DMS_NORMAL):
            xastir_snprintf(str, str_len, "%03d %02d %04.1f%c", ideg, imin, sec, ew);
            break;
        case(CONVERT_HP_NORMAL):

        default: /* do HP normal */
            xastir_snprintf(str, str_len, "%03d %06.3f%c", ideg, min, ew);
            break;
    }
}





/* convert latitude from string to long with 1/100 sec resolution */
// Input is in DDMM.MMN, DDMM.MMMN, or DDMM.MMMMN format
// (degrees/decimal minutes/direction)
long convert_lat_s2l(char *lat) {      /* N=0°, Ctr=90°, S=180° */
    long centi_sec;
    char copy[11];
    char n[4];

    strncpy(copy,lat,sizeof(copy));
    copy[10] = '\0';
    centi_sec=0l;
    if (copy[4]=='.'
            && (   (char)toupper((int)copy[7])=='N'
                || (char)toupper((int)copy[8])=='N'
                || (char)toupper((int)copy[9])=='N'
                || (char)toupper((int)copy[7])=='S'
                || (char)toupper((int)copy[8])=='S'
                || (char)toupper((int)copy[9])=='S')) {
        substr(n, copy, 2);       // degrees
        centi_sec=atoi(n)*60*60*100;
        substr(n, copy+2, 2);     // minutes
        centi_sec += atoi(n)*60*100;
        substr(n, copy+5, 4);     // fractional minutes
        if ( (!isdigit((int)n[2]))
                && (n[2] != '\0') )
            n[2] = '0';            /* extend low precision */
        n[3] = '\0';    // Throw this one away

        centi_sec += atoi(n)*6;
        if (       (char)toupper((int)copy[7])=='N'
                || (char)toupper((int)copy[8])=='N'
                || (char)toupper((int)copy[9])=='N')
            centi_sec = -centi_sec;

        centi_sec += 90*60*60*100;
    }
    return(centi_sec);
}





/* convert longitude from string to long with 1/100 sec resolution */
// Input is in DDDMM.MMW format (degrees/decimal minutes/direction),
// DDDMM.MMMW, or DDDMM.MMMMW format
long convert_lon_s2l(char *lon) {     /* W=0°, Ctr=180°, E=360° */
    long centi_sec;
    char copy[12];
    char n[4];

    strncpy(copy,lon,sizeof(copy));
    copy[11] = '\0';
    centi_sec=0l;
    if (copy[5]=='.'
            && (   (char)toupper((int)copy[ 8])=='W'
                || (char)toupper((int)copy[ 9])=='W'
                || (char)toupper((int)copy[10])=='W'
                || (char)toupper((int)copy[ 8])=='E'
                || (char)toupper((int)copy[ 9])=='E'
                || (char)toupper((int)copy[10])=='E')) {
        substr(n,copy,3);    // degrees 013
        centi_sec=atoi(n)*60*60*100;

        substr(n,copy+3,2);  // minutes 26
        centi_sec += atoi(n)*60*100;
        // 01326.66E  01326.660E
        substr(n,copy+6,4);  // fractional minutes 66E 660E or 6601
        if (!isdigit((int)n[2]) && (n[2] != '\0'))
            n[2] = '0';            /* extend low precision */
        n[3] = '\0';    // Throw this one away
        centi_sec += atoi(n)*6;
        if (       (char)toupper((int)copy[ 8])=='W'
                || (char)toupper((int)copy[ 9])=='W'
                || (char)toupper((int)copy[10])=='W')
            centi_sec = -centi_sec;
        centi_sec +=180*60*60*100;;
    }
    return(centi_sec);
}





/*
 *  Convert latitude from Xastir format to radian
 */
double convert_lat_l2r(long lat) {
    double ret;
    double degrees;

    degrees = (double)(lat / (100.0*60.0*60.0)) -90.0;
    ret = (-1)*degrees*(M_PI/180.0);
    return(ret);
}





/*
 *  Convert longitude from Xastir format to radian
 */
double convert_lon_l2r(long lon) {
    double ret;
    double degrees;

    degrees = (double)(lon / (100.0*60.0*60.0)) -180.0;
    ret = (-1)*degrees*(M_PI/180.0);
    return(ret);
}





/*
 *  Calculate distance in meters between two locations
 */
double calc_distance(long lat1, long lon1, long lat2, long lon2) {
    double r_lat1,r_lat2;
    double r_lon1,r_lon2;
    double r_d;

    r_lat1 = convert_lat_l2r(lat1);
    r_lon1 = convert_lon_l2r(lon1);
    r_lat2 = convert_lat_l2r(lat2);
    r_lon2 = convert_lon_l2r(lon2);
    r_d = acos(sin(r_lat1) * sin(r_lat2) + cos(r_lat1) * cos(r_lat2) * cos(r_lon1-r_lon2));
    return(r_d*180*60/M_PI*1852);
}





/*
 *  Calculate distance between two locations in nautical miles and course from loc2 to loc1
 */
double calc_distance_course(long lat1, long lon1, long lat2, long lon2, char *course_deg, int course_deg_length) {
    float ret;
    double r_lat1,r_lat2;
    double r_lon1,r_lon2;
    double r_d,r_c;

    r_lat1 = convert_lat_l2r(lat1);
    r_lon1 = convert_lon_l2r(lon1);
    r_lat2 = convert_lat_l2r(lat2);
    r_lon2 = convert_lon_l2r(lon2);

    // Compute the distance:
    // Law of Cosines for Spherical Trigonometry.  This is unreliable for small
    // distances because the inverse cosine is ill-conditioned.  A computer
    // carrying seven significant digits can't distinguish the cosines of
    // distances smaller than about one minute of arc.
    r_d = acos(sin(r_lat1) * sin(r_lat2) + cos(r_lat1) * cos(r_lat2) * cos(r_lon1-r_lon2));

    // Compute the course:
    if (cos(r_lat1) < 0.0000000001) {
        if (r_lat1>0.0)
            r_c=M_PI;
        else
            r_c=0.0;
    } else {
        if (sin((r_lon2-r_lon1))<0.0)
            r_c = acos((sin(r_lat2)-sin(r_lat1)*cos(r_d))/(sin(r_d)*cos(r_lat1)));
        else
            r_c = (2*M_PI) - acos((sin(r_lat2)-sin(r_lat1)*cos(r_d))/(sin(r_d)*cos(r_lat1)));

    }

    // Return the course
    xastir_snprintf(course_deg, course_deg_length, "%.1f", (180.0/M_PI)*r_c);

    // Return the distance (nautical miles?)
    ret = r_d*180*60/M_PI;
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

double distance_from_my_station(char *call_sign, char *course_deg) {
    DataRow *p_station;
    double distance;
    float value;
    long l_lat, l_lon;

    distance = 0.0;
    l_lat = convert_lat_s2l(my_lat);
    l_lon = convert_lon_s2l(my_long);
    p_station = NULL;
    if (search_station_name(&p_station,call_sign,1)) {
        // Check whether we have a posit yet for this station
        if ( (p_station->coord_lat == 0l)
                && (p_station->coord_lon == 0l) ) {
            distance = 0.0;
        }
        else {
            value = (float)calc_distance_course(l_lat,
                l_lon,
                p_station->coord_lat,
                p_station->coord_lon,
                course_deg,
                sizeof(course_deg));

            if (units_english_metric)
                distance = value * 1.15078;         // nautical miles to miles
            else
                distance = value * 1.852;           // nautical miles to km
        }
//        printf("DistFromMy: %s %s -> %f\n",temp_lat,temp_long,distance);
    }
    else {  // Station not found
        distance = 0.0;
    }

    //printf("Distance for %s: %f\n", call_sign, distance);

    return(distance);
}





/*********************************************************************/
/* convert_bearing_to_name - converts a bearing in degrees to        */
/*   name for the bearing.  Expects the degrees as a text string     */
/*   since that's what the preceding functions output.               */
/* Set the opposite flag true for the inverse bearing (from vs to)   */
/*********************************************************************/

static struct {
    double low,high;
    char *dircode,*lang;
} directions[] = {
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



char *convert_bearing_to_name(char *bearing, int opposite) {
    double deg = atof(bearing);
    int i;

    if (opposite) {
        if (deg > 180) deg -= 180.0;
        else if (deg <= 180) deg += 180.0;
    }
    for (i = 0; i < sizeof(directions)/sizeof(directions[0]); i++) {
        if (deg >= directions[i].low && deg < directions[i].high)
            return langcode(directions[i].lang);
    }
    return "?";
}





int filethere(char *fn) {
    FILE *f;
    int ret;

    ret =0;
    f=fopen(fn,"r");
    if (f != NULL) {
        ret=1;
        (void)fclose(f);
    }
    return(ret);
}





int filecreate(char *fn) {
    FILE *f;
    int ret;

    if (! filethere(fn)) {   // If no file

        ret = 0;
        printf("Making user %s file\n", fn);
        f=fopen(fn,"w+");   // Create it
        if (f != NULL) {
            ret=1;          // We were successful
            (void)fclose(f);
        }
        return(ret);        // Couldn't create file for some reason
    }
    return(1);  // File already exists
}





time_t file_time(char *fn) {
    struct stat nfile;

    if(stat(fn,&nfile)==0)
        return((time_t)nfile.st_ctime);

    return(-1);
}





//
// Note that the length of "line" can be up to MAX_DEVICE_BUFFER,
// which is currently set to 4096.
//
void log_data(char *file, char *line) {
    FILE *f;

    // Check for "# Tickle" first, and don't log it if found.
    // It's an idle string designed to keep the socket active.
    if ( (strncasecmp(line, "#Tickle", 7)==0)
        || (strncasecmp(line, "# Tickle", 8) == 0) ) {
        return;
    }
    else {
        // Change back to the base directory
        chdir(get_user_base_dir(""));

        f=fopen(file,"a");
        if (f!=NULL) {
            fprintf(f,"%s\n",line);
            (void)fclose(f);
        }
        else {
            printf("Couldn't open file for appending: %s\n", file);
        }
    }
}





//
// Disown function called by object/item decode routines.
// If an object/item is received that matches something in our
// object.log file, we immediately cease to transmit that object and
// we mark each line containing that object in our log file with a
// hash mark ('#').  This comments out that object so that the next
// time we reboot, we won't start transmitting it again.

// Note that the length of "line" can be up to MAX_DEVICE_BUFFER,
// which is currently set to 4096.
//
void disown_object_item(char *call_sign, char *new_owner) {
    char *ptr;
    char file[200];
    char file_temp[200];
    FILE *f;
    FILE *f_temp;
    char command[300];
    char line[300];
    char name[15];


    // If it's my call in the new_owner field, then I must have just
    // deleted the object and am transmitting a killed object for
    // it.  If it's not my call, someone else has assumed control of
    // the object.
    //
    // Comment out any references to the object in the log file so
    // that we don't start retransmitting it on a restart.

    if (is_my_call(new_owner,1)) {
        //printf("Commenting out %s in object.log\n", call_sign);
    }
    else {
        printf("Disowning '%s': '%s' is taking over control of it.\n",
            call_sign, new_owner);
    }

    ptr =  get_user_base_dir("config/object.log");
    strcpy(file,ptr);

    ptr =  get_user_base_dir("config/object-temp.log");
    strcpy(file_temp,ptr);

    //printf("%s\t%s\n",file,file_temp);

    // Copy to a temp file
    xastir_snprintf(command,
        sizeof(command), "cp -f %s %s", file, file_temp);

    if ( debug_level & 512 )
        printf( "%s\n", command );

    if ( system( command ) != 0 ) {
        printf("\n\nCouldn't create temp file %s!\n\n\n",
            file_temp);
        return;
    }

    // Open the temp file and write to the original file, with hash
    // marks in front of the appropriate lines.
    f_temp=fopen(file_temp,"r");
    f=fopen(file,"w");

    if (f == NULL) {
        printf("Couldn't open %s\n",file);
        return;
    }
    if (f_temp == NULL) {
        printf("Couldn't open %s\n",file_temp);
        return;
    }

    // Read lines from the temp file and write them to the standard
    // file, modifying them as necessary.
    while (fgets(line, 300, f_temp) != NULL) {

        // Need to check that the length matches for both!  Best way
        // is to parse the object/item name out of the string and
        // then do a normal string compare between the two.

        if (line[0] == ';') {       // Object
            substr(name,&line[1],9);
            name[9] = '\0';
        }
        else if (line[0] == ')') {  // Item
            int i;

            // 3-9 char name
            for (i = 1; i <= 9; i++) {
                if (line[i] == '!' || line[i] == '_') {
                    name[i-1] = '\0';
                    break;
                }
                name[i-1] = line[i];
            }
            name[9] = '\0';  // In case we never saw '!' || '_'
        }

        remove_trailing_spaces(name);

        //printf("'%s'\t'%s'\n", name, call_sign);

        if (valid_object(name)) {

            if ( strcmp(name,call_sign) == 0 ) {
                // Match.  Comment it out in the file unless it's
                // already commented out.
                if (line[0] != '#') {
                    fprintf(f,"#%s",line);
                    //printf("#%s",line);
                }
                else {
                    fprintf(f,"%s",line);
                    //printf("%s",line);
                }
            }
            else {
                // No match.  Copy the line verbatim unless it's just a
                // blank line.
                if (line[0] != '\n') {
                    fprintf(f,"%s",line);
                    //printf("%s",line);
                }
            }
        }
    }
    fclose(f);
    fclose(f_temp);
}





//
// Logging function called by object/item create/modify routines.
// We log each object/item as one line in a file.
//
// We need to check for objects of the same name in the file,
// deleting lines that have the same name, and adding new records to
// the end.  Actually  BAD IDEA!  We want to keep the history of the
// object so that we can trace its movements later.
//
// Note that the length of "line" can be up to MAX_DEVICE_BUFFER,
// which is currently set to 4096.
//
// Change this function so that deleted objects/items get disowned
// instead (commented out in the file so that they're not
// transmitted again after a restart).
//
void log_object_item(char *line, int disable_object, char *object_name) {
    char *file;
    FILE *f;

    file = get_user_base_dir("config/object.log");

    f=fopen(file,"a");
    if (f!=NULL) {
        fprintf(f,"%s\n",line);
        (void)fclose(f);

        if (debug_level & 1)
            printf("Saving object/item to file: %s",line);

        // Comment out all instances of the object/item in the log
        // file.  This will make sure that the object is not
        // retransmitted again when Xastir is restarted.
        if (disable_object) {
            disown_object_item(object_name, my_callsign);
       }

    }
    else {
        printf("Couldn't open file for appending: %s\n", file);
    }
}





//
// Function to load saved objects and items back into Xastir.  This
// is called on startup.  This implements persistent objects/items
// across Xastir restarts.
//
// Note that the length of "line" can be up to MAX_DEVICE_BUFFER,
// which is currently set to 4096.
//
void reload_object_item(void) {
    char *file;
    FILE *f;
    char line[300+1];
    char line2[300+1];

    file = get_user_base_dir("config/object.log");

    f=fopen(file,"r");
    if (f!=NULL) {
        while (fgets(line, 300, f) != NULL) {
            if (debug_level & 1)
                printf("Loading object/item from file: %s",line);
   
            if (line[0] != '#') {   // Skip comment lines
                xastir_snprintf(line2,sizeof(line2),"%s>%s:%s",my_callsign,VERSIONFRM,line);

                // Decode this packet.  This will put it into our
                // station database and cause it to be transmitted at
                // regular intervals.  Port is set to -1 here.
                decode_ax25_line( line2, DATA_VIA_LOCAL, -1, 1);
            }
        }
        (void)fclose(f);

        // Update the screen
        redraw_symbols(da);
        (void)XCopyArea(XtDisplay(da),pixmap_final,XtWindow(da),gc,0,0,screen_width,screen_height,0,0);
    }
    else {
        if (debug_level & 1)
            printf("Couldn't open file for reading: %s\n", file);
    }
}





time_t sec_now(void) {
    time_t timenw;
    time_t ret;

    ret = time(&timenw);
    return(ret);
}





char *get_time(char *time_here) {
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

int valid_path(char *path) {
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
    if (len > 2 && path[len-2] == '/') {
        ch = path[len-1];
        if (ch == 'V' || ch == 'H' || ch == '0' || ch == '1' || ch == '2') {
            path[len-2] = '\0';
            len  = (int)strlen(path);
        }
    }


    // One way of adding igate injection ID is to add "callsign-ssid,I".
    // We need to remove the ",I" portion so it doesn't count as another
    // digi here.  This should be at the end of the path.
    if (len > 2 && path[len-2] == ',' && path[len-1] == 'I') {  // Found ",I"
        //printf("%s\n",path);
        //printf("Found ',I'\n");
        path[len-2] = '\0';
        len  = (int)strlen(path);
        //printf("%s\n\n",path);
    }
    // Now look for the same thing but with a '*' character at the end.
    // This should be at the end of the path.
    if (len > 3 && path[len-3] == ',' && path[len-2] == 'I' && path[len-1] == '*') {  // Found ",I*"
        //printf("%s\n",path);
        //printf("Found ',I*'\n");
        path[len-3] = '\0';
        len  = (int)strlen(path);
        //printf("%s\n\n",path);
    }


    // Another method of adding igate injection ID is to add a '0' in front of
    // the SSID.  For WE7U it would change to WE7U-00, for WE7U-15 it would
    // change to WE7U-015.  Take out this zero so the rest of the decoding will
    // work.  This should be at the end of the path.
    // Also look for the same thing but with a '*' character at the end.
    if (len > 6) {
        for (i=len-1; i>len-6; i--) {
            if (path[i] == '-' && path[i+1] == '0') {
                //printf("%s\n",path);
                for (j=i+1; j<len; j++) {
                    path[j] = path[j+1];    // Shift everything left by one
                }
                len = (int)strlen(path);
                //printf("%s\n\n",path);
            }
            // Check whether we just chopped off the '0' from "-0".
            // If so, chop off the dash as well.
            if (path[i] == '-' && path[i+1] == '\0') {
                //printf("%s\tChopping off dash\n",path);
                path[i] = '\0';
                len = (int)strlen(path);
                //printf("%s\n",path);
            }
            // Check for "-*", change to '*' only
            if (path[i] == '-' && path[i+1] == '*') {
                //printf("%s\tChopping off dash\n",path);
                path[i] = '*';
                path[i+1] = '\0';
                len = (int)strlen(path);
                //printf("%s\n",path);
            }
            // Check for "-0" or "-0*".  Change to "" or "*".
            if ( path[i] == '-' && path[i+1] == '0' ) {
                //printf("%s\tShifting left by two\n",path);
                for (j=i; j<len; j++) {
                    path[j] = path[j+2];    // Shift everything left by two
                }
                len = (int)strlen(path);
                //printf("%s\n",path);
            }
        }
    }


    for (i=0,j=0; i<len; i++) {
        ch = path[i];
        if (ch == '>' || ch == ',') {   // found digi call separator
            if (ast > 1 || (ast == 1 && i-j > 10) || (ast == 0 && (i == j || i-j > 9)))
                return(0);              // more than one asterisk in call or wrong call size
            ast = 0;                    // reset local asterisk counter
            
            j = i+1;                    // set to start of next call
            if (ch == ',')
                type |= 0x02;           // set TAPR2 flag
            else
                type |= 0x01;           // set AEA flag (found '>')
            hops++;                     // count hops
        } else {                        // digi call character or asterisk
            if (ch == '*') {
                ast++;                  // count asterisks in call
                allast++;               // count asterisks in path
            } else
                if ((ch <'A' || ch > 'Z') && (ch <'0' || ch > '9')
                        && ch != '-'
                        && ch != 'q')   // New anti-loop stuff from aprsd
                    return(0);          // wrong character in path
        }
    }
    if (ast > 1 || (ast == 1 && i-j > 10) || (ast == 0 && (i == j || i-j > 9)))
        return(0);                      // more than one asterisk or wrong call size

    if (type == 0x03)
        return(0);                      // wrong format, both '>' and ',' in path

    if (hops > 9)                       // [APRS Reference chapter 3]
        return(0);                      // too much hops, destination + 0-8 digipeater addresses

    if (type == 0x01) {
        int delimiters[20];
        int k = 0;
        char dest[15];
        char rest[100];

        for (i=0; i<len; i++) {
            if (path[i] == '>') {
                path[i] = ',';          // Exchange separator character
                delimiters[k++] = i;    // Save the delimiter indexes
            }
        }

        // We also need to move the destination callsign to the end.
        // AEA has them in a different order than TAPR-2 format.
        // We'll move the destination address between delimiters[0]
        // and [1] to the end of the string.

        //printf("Orig. Path:%s\n",path);
        // Save the destination
        xastir_snprintf(dest,sizeof(dest),"%s",&path[delimiters[--k]+1]);
        dest[strlen(path) - delimiters[k] - 1] = '\0'; // Terminate it
        dest[14] = '\0';    // Just to make sure
        path[delimiters[k]] = '\0'; // Delete it from the original path
        //printf("Destination: %s\n",dest);

        // TAPR-2 Format:
        // KC2ELS-1*>SX0PWT,RELAY,WIDE:`2`$l##>/>"4)}
        //
        // AEA Format:
        // KC2ELS-1*>RELAY>WIDE>SX0PWT:`2`$l##>/>"4)}
        //          9     15   20

        // We now need to insert the destination into the middle of
        // the string.  Save part of it in another variable first.
        strcpy(rest,path);
        //printf("Rest:%s\n",rest);
        xastir_snprintf(path,len+1,"%s,%s",dest,rest);
        //printf("New Path:%s\n",path);
    }

    if (allast < 1) {                   // try to insert a missing asterisk
        ins  = 0;
        hops = 0;
        for (i=0; i<len; i++) {
            for (j=i; j<len; j++) {             // search for separator
                if (path[j] == ',')
                    break;
            }
            if (hops > 0 && (j - i) == 5) {     // WIDE3
                if (  path[ i ] == 'W' && path[i+1] == 'I' && path[i+2] == 'D' 
                   && path[i+3] == 'E' && path[i+4] >= '0' && path[i+4] <= '9') {
                    ins = j;
                }
            }
            if (hops > 0 && (j - i) == 7) {     // WIDE3-2
                if (  path[ i ] == 'W' && path[i+1] == 'I' && path[i+2] == 'D' 
                   && path[i+3] == 'E' && path[i+4] >= '0' && path[i+4] <= '9'
                   && path[i+5] == '-' && path[i+6] >= '0' && path[i+6] <= '9'
                   && (path[i+4] != path[i+6]) ) {
                    ins = j;
                }
            }
            if (hops > 0 && (j - i) == 6) {     // TRACE3
                if (  path[ i ] == 'T' && path[i+1] == 'R' && path[i+2] == 'A' 
                   && path[i+3] == 'C' && path[i+4] == 'E'
                   && path[i+5] >= '0' && path[i+5] <= '9') {
                    if (hops == 1)
                        ins = j;
                    else
                        ins = i-1;
                }
            }
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
            hops++;
            i = j;                      // skip to start of next call
        }
        if (ins > 0) {
            for (i=len;i>=ins;i--) {
                path[i+1] = path[i];    // generate space for '*'
                // we work on a separate path copy which is long enough to do it
            }
            path[ins] = '*';            // and insert it
        }
    }
    return(1);
}





/*
 *  Check for a valid AX.25 call
 *      Valid calls consist of up to 6 uppercase alphanumeric characters
 *      plus optional SSID (four-bit integer)       [APRS Reference, AX.25 Reference]
 */
int valid_call(char *call) {
    int len, ok;
    int i, del, has_num, has_chr;
    char c;

    has_num = 0;
    has_chr = 0;
    ok      = 1;
    len = (int)strlen(call);

    if (len == 0)
        return(0);                              // wrong size

        while (call[0]=='c' && call[1]=='m' && call[2]=='d' && call[3]==':')
        {       // Erase TNC prompts from beginning of callsign.
                // This may not be the right place to do this, but it came in
                // handy here, so that's where I put it. -- KB6MER

        if (debug_level & 1) {
            char filtered_data[MAX_LINE_SIZE+1];
            strcpy(filtered_data, call);
            makePrintable(filtered_data);
            printf("valid_call: Command Prompt removed from: %s",
                filtered_data);
        }
                for(i=0; call[i+4]; i++)
                        call[i]=call[i+4];
                call[i++]=0;
                call[i++]=0;
                call[i++]=0;
                call[i++]=0;
                len=strlen(call);
        if (debug_level & 1) {
            char filtered_data[MAX_LINE_SIZE+1];
            strcpy(filtered_data, call);
            makePrintable(filtered_data);
            printf(" result: %s", filtered_data);
                }
        }

        if (len > 9)
                return(0);      // Too long for valid call (6-2 max e.g. KB6MER-12)

    del = 0;
    for (i=len-2;ok && i>0 && i>=len-3;i--) {   // search for optional SSID
        if (call[i] =='-')
            del = i;                            // found the delimiter
    }
    if (del) {                                  // we have a SSID, so check it
        if (len-del == 2) {                     // 2 char SSID
            if (call[del+1] < '1' || call[del+1] > '9')                         //  -1 ...  -9
                del = 0;
        } else {                                // 3 char SSID
            if (call[del+1] != '1' || call[del+2] < '0' || call[del+2] > '5')   // -10 ... -15
                del = 0;
        }
    }
    if (del)
        len = del;                              // length of base call
    for (i=0;ok && i<len;i++) {                 // check for uppercase alphanumeric
        c = call[i];
        if (c >= 'A' && c <= 'Z')
            has_chr = 1;                        // we need at least one char
        else if (c >= '0' && c <= '9')
            has_num = 1;                        // we need at least one number
        else
            ok = 0;                             // wrong character in call
    }

//    if (!has_num || !has_chr)                 // with this we also discard NOCALL etc.
    if (!has_chr)                               
        ok = 0;
    ok = (ok && strcmp(call,"NOCALL") != 0);    // check for errors
    ok = (ok && strcmp(call,"ERROR!") != 0);
    ok = (ok && strcmp(call,"WIDE")   != 0);
    ok = (ok && strcmp(call,"RELAY")  != 0);
    ok = (ok && strcmp(call,"MAIL")   != 0);
    return(ok);
}





/*
 *  Check for a valid object name
 */
int valid_object(char *name) {
    int len, i;
    
    // max 9 printable ASCII characters, case sensitive   [APRS Reference]
    len = (int)strlen(name);
    if (len > 9 || len == 0)
        return(0);                      // wrong size

    for (i=0;i<len;i++)
        if (!isprint((int)name[i]))
            return(0);                  // not printable

    return(1);
}





/*
 *  Check for a valid item name (3-9 chars, any printable ASCII except '!' or '_')
 */
int valid_item(char *name) {
    int len, i;
    
    // min 3, max 9 printable ASCII characters, case sensitive   [APRS Reference]
    len = (int)strlen(name);
    if (len > 9 || len < 3)
        return(0);                      // Wrong size

    for (i=0;i<len;i++) {
        if (!isprint((int)name[i])) {
            return(0);                  // Not printable
        }
        if ( (name[i] == '!') || (name[i] == '_') ) {
            return(0);                  // Contains '!' or '_'
        }
    }

    return(1);
}





/*
 *  Check for a valid internet name
 *  accept only a few well formed names...
 */
int valid_inet_name(char *name, char *info, char *origin) {
    int len, i, ok;
    char *ptr;
    
    len = (int)strlen(name);

    if (len > 9 || len == 0)            // max 9 printable ASCII characters
        return(0);                      // wrong size

    for (i=0;i<len;i++)
        if (!isprint((int)name[i]))
            return(0);                  // not printable

    if (len > 5 && strncmp(name,"aprsd",5) == 0) {
        strcpy(origin, "INET");
        return(1);                      // aprsdXXXX is ok
    }

    if (len == 6) {                     // check for NWS
        ok = 1;
        for (i=0;i<6;i++)
            if (name[i] <'A' || name[i] > 'Z')  // 6 uppercase characters
                ok = 0;
        ok = ok && (info != NULL);      // check if we can test info
        if (ok) {
            ptr = strstr(info,":NWS-"); // NWS data in info field
            ok = (ptr != NULL);
        }
        if (ok) {
            strcpy(origin, "INET-NWS");
            return(1);                      // weather alerts
        }
    }

    // other acceptable internet names...
    // don't be too generous here, there is a lot of garbage on the internet

    return(0);                          // ignore all other
    
}





/*
 *  Keep track of last six digis that echo my transmission
 */
void upd_echo(char *path) {
    int i,j,len;

    if (echo_digis[5][0] != '\0') {
        for (i=0;i<5;i++) {
            strcpy(echo_digis[i], echo_digis[i+1]);
        }
        echo_digis[5][0] = '\0';
    }
    for (i=0,j=0;i < (int)strlen(path);i++) {
        if (path[i] == '*')
            break;
        if (path[i] == ',')
            j=i;
    }
    if (j > 0)
        j++;                    // first char of call
    if (i > 0 && i-j <= 9) {
        len = i-j;
        for (i=0;i<5;i++) {     // look for free entry
            if (echo_digis[i][0] == '\0')
                break;
        }
        substr(echo_digis[i],path+j,len);
    }
}





// dl9sau:
// I liked to have xastir to compute the locator along with the normal coordinates.
// The algorithm derives from dk5sg's util/qth.c (wampes)
//
char *sec_to_loc(long longitude, long latitude)
{
  static char buf[7];
  char *loc = buf;

  // db.h:    long coord_lon;                     // Xastir coordinates 1/100 sec, 0 = 180°W
  // db.h:    long coord_lat;                     // Xastir coordinates 1/100 sec, 0 =  90°N

  longitude /= 100;
  latitude  =  2* 90 * 3600L - latitude / 100;
  *loc++ = (char) (longitude / 72000 + 'A'); longitude = longitude % 72000;
  *loc++ = (char) (latitude  / 36000 + 'A'); latitude  = latitude % 36000;
  *loc++ = (char) (longitude /  7200 + '0'); longitude = longitude %  7200;
  *loc++ = (char) (latitude  /  3600 + '0'); latitude  = latitude %  3600;
  *loc++ = (char) (longitude /   300 + 'a');
  *loc++ = (char) (latitude  /   150 + 'a');
  *loc   = 0;
  return buf;
}





/*
 *  Substring function WITH a terminating NULL char, needs a string of at least size+1
 */
void substr(char *dest, char *src, int size) {

    strncpy(dest,src,(size_t)size);
    dest[size] = '\0';
}






char *to_upper(char *data) {
    int i;

    for(i=strlen(data)-1;i>=0;i--)
        if(isalpha((int)data[i]))
            data[i]=toupper((int)data[i]);

    return(data);
}






int is_num_chr(char ch) {
    return((int)(ch >= '0' && ch <= '9'));
}






int is_num_or_sp(char ch) {
    return((int)((ch >= '0' && ch <= '9') || ch == ' '));
}






int is_xnum_or_dash(char *data, int max){
    int i;
    int ok;

    ok=1;
    for(i=0; i<max;i++)
        if(!(isxdigit((int)data[i]) || data[i]=='-')) {
            ok=0;
            break;
        }

    return(ok);
}





// not used
int is_num_stuff(char *data, int max) {
    int i;
    int ok;

    ok=1;
    for(i=0; i<max;i++)
        if(!(isdigit((int)data[i]) || data[i]=='-' || data[i]=='+' || data[i]=='.')) {
            ok=0;
            break;
        }

    return(ok);
}





// not used
int is_num(char *data) {
    int i;
    int ok;

    ok=1;
    for(i=strlen(data)-1;i>=0;i--)
        if(!isdigit((int)data[i])) {
            ok=0;
            break;
        }

    return(ok);
}





//--------------------------------------------------------------------
//Removes all control codes ( < 1Ch ) from a string and set the 8th bit to zero
void removeCtrlCodes(char *cp) {
    int i,j;
    int len = (int)strlen(cp);
    unsigned char *ucp = (unsigned char *)cp;

    for (i=0, j=0; i<=len; i++) {
        ucp[i] &= 0x7f;                 // Clear 8th bit
        if ( (ucp[i] >= (unsigned char)0x1c)           // Check for printable plus the Mic-E codes
                || ((char)ucp[i] == '\0') )   // or terminating 0
            ucp[j++] = ucp[i] ;        // Copy to temp if printable
    }
}





//--------------------------------------------------------------------
//Removes all control codes ( <0x20 or >0x7e ) from a string, including
// CR's, LF's, tab's, etc.
//
void makePrintable(char *cp) {
    int i,j;
    int len = (int)strlen(cp);
    unsigned char *ucp = (unsigned char *)cp;

    for (i=0, j=0; i<=len; i++) {
        ucp[i] &= 0x7f;                 // Clear 8th bit
        if ( ((ucp[i] >= (unsigned char)0x20) && (ucp[i] <= (unsigned char)0x7e))
              || ((char)ucp[i] == '\0') )     // Check for printable or terminating 0
            ucp[j++] = ucp[i] ;        // Copy to (possibly) new location if printable
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
void init_critical_section(xastir_mutex *lock) {
#ifdef MUTEX_DEBUG
    pthread_mutexattr_t attr;

    // Note that this stuff is not POSIX, so is considered non-portable.
    // Exists in Linux Threads implementation only?
    (void)pthread_mutexattr_setkind_np(&attr, PTHREAD_MUTEX_ERRORCHECK_NP);
    (void)pthread_mutexattr_init(&attr);
    (void)pthread_mutex_init(&lock->lock, &attr);
#else
    (void)pthread_mutex_init(&lock->lock, NULL); // Set to default POSIX-compatible type
#endif

    lock->threadID = 0;  // No thread has locked it yet
}





// Function which uses xastir_mutex objects to lock a
// critical shared section of code or variables.  Makes
// sure that only one thread can access the critical section
// at a time.  If there are no problems, it returns zero.
//
int begin_critical_section(xastir_mutex *lock, char *text) {
    pthread_t calling_thread;
    int problems;
#ifdef MUTEX_DEBUG
    int ret;
#endif

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
        printf("%s:This thread already has the lock on this resource\n", text);
        problems++;
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
    do {
        ret = pthread_mutex_lock(&lock->lock);
        if (ret == EDEADLK) {   // Normal operation.  At least two threads want this lock.
            printf("%s:pthread_mutex_lock(&lock->lock) == EDEADLK\n", text);
            sched_yield();  // Yield the cpu to another thread
        }
        else if (ret == EINVAL) {
            printf("%s:pthread_mutex_lock(&lock->lock) == EINVAL\n", text);
            printf("Forgot to initialize the mutex object...\n");
            problems++;
            sched_yield();  // Yield the cpu to another thread
        }
    } while (ret == EDEADLK);

    if (problems)
        printf("Returning %d to calling thread\n", problems);
#else
    pthread_mutex_lock(&lock->lock);
#endif

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
int end_critical_section(xastir_mutex *lock, char *text) {
    pthread_t calling_thread;
    int problems;
#ifdef MUTEX_DEBUG
    int ret;
#endif

    problems = 0;

    // Get thread number we're currently running under
    calling_thread = pthread_self();

    if (lock->threadID == 0)
    {
        // We have a problem.  This resource hasn't been locked.
        printf("%s:Trying to unlock a resource that hasn't been locked:%ld\n",
            text,
            (long int)lock->threadID);
        problems++;
    }

    if (! pthread_equal( lock->threadID, calling_thread ))
    {
        // We have a problem.  We just tried to unlock a mutex which
        // a different thread originally locked.  Not good.
        printf("%s:Trying to unlock a resource that a different thread locked originally: %ld:%ld\n",
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

    if (ret == EPERM) {
        printf("%s:pthread_mutex_unlock(&lock->lock) == EPERM\n", text);
        problems++;
    }
    else if (ret == EINVAL) {
        printf("%s:pthread_mutex_unlock(&lock->lock) == EINVAL\n", text);
        printf("Someone forgot to initialize the mutex object\n");
        problems++;
    }

    if (problems)
        printf("Returning %d to calling thread\n", problems);
#else
    (void)pthread_mutex_unlock(&lock->lock);
#endif

    return(problems);
}


#ifdef TIMING_DEBUG
void time_mark(int start)
{
    static struct timeval t_start;
    struct timeval t_cur;
    long sec, usec;

    if (start) {
        gettimeofday(&t_start, NULL);
        puts("\nstart: 0.000000s");
    }
    else {
        gettimeofday(&t_cur, NULL);
        sec  = t_cur.tv_sec  - t_start.tv_sec;
        usec = t_cur.tv_usec - t_start.tv_usec;
        if (usec < 0) {
            sec--;
            usec += 1000000;
        }
        printf("time:  %ld.%06lds\n", sec, usec);
    }
}
#endif


// Function which adds commas to callsigns (and other abbreviations?)
// in order to make the text sound better when run through a Text-to-
// Speech system.  We try to change only normal amateur callsigns.
// If we find a number in the text before a dash is found, we
// consider it to be a normal callsign.  We don't add commas to the
// SSID portion of a call.
void spell_it_out(char *text) {
    char buffer[2000];
    int i = 0;
    int j = 0;
    int number_found_before_dash = 0;
    int dash_found = 0;

    while (text[i] != '\0') {

        if (text[i] == '-')
            dash_found++;

        if (is_num_chr(text[i]) && !dash_found)
            number_found_before_dash++;

        buffer[j++] = text[i];

        if (!dash_found) // Don't add commas to SSID
            buffer[j++] = ',';

        i++;
    }
    buffer[j] = '\0';

    // Only use the new string if it kind'a looks like a callsign
    if (number_found_before_dash)
        strcpy(text,buffer);
}


