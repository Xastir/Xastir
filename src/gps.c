/*
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

#include <stdio.h>
#include <ctype.h>
#include <Xm/XmAll.h>

/* The following files support setting the system time from the GPS */
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

#include "gps.h"
#include "main.h"
#include "xastir.h"
#include "interface.h"
#include "lang.h"
#include "db.h"
#include "util.h"


char gps_gprmc[MAX_GPS_STRING+1];
char gps_gpgga[MAX_GPS_STRING+1];
char gps_sats[4] = "";
char gps_alt[8] = "";
char gps_spd[10] = "";
char gps_sunit[2] = "";
char gps_cse[10] = "";
 
int gps_stop_now;





// This function is destructive to its first parameter
int decode_gps_rmc( char *data,
                    char *long_pos,
                    int long_pos_length,
                    char *lat_pos,
                    int lat_pos_length,
                    char *spd,
                    char *unit,
                    int unit_length,
                    char *cse,
                    time_t *stim) {

    char *temp_ptr;
    char temp_data[MAX_TNC_LINE_SIZE+1];    // Big in case we get concatenated packets (it happens!)
        char sampletime[10];
    char long_pos_x[11];
    char long_ew;
    char lat_pos_y[10];
    char lat_ns;
    char speed[9];
    char speed_unit;
    char course[7];
    char sampledate[7];

#ifdef HAVE_STRPTIME
    char sampledatime[15];
    char *tzp;
    char tzn[512];
    struct tm stm;
#endif // HAVE_STRPTIME
 
    int ok;

// We should check for a minimum line length before parsing,
// and check for end of input while tokenizing.

    ok=0;

    if ( (data == NULL) || (strlen(data) < 37) )  // Not enough data to parse position from.
        return(ok);

    if (strncmp(data,"$GPRMC,",7)==0) {
        if(strchr(data,',')!=NULL) {
            (void)strtok(data,",");   /* get GPRMC and skip it */
            temp_ptr=strtok(NULL,",");   /* get time */
            if (temp_ptr != NULL) {
                strncpy(sampletime, temp_ptr, 6);
                sampletime[6]='\0';
                temp_ptr=strtok(NULL,",");  /* get fix status */
                if (temp_ptr!=NULL) {
                    strncpy(temp_data,temp_ptr,2);
                    if (temp_data[0]=='A') {    /* V is a warning but we can get good data still ? */
                        temp_ptr=strtok(NULL,",");  /* get latitude */
                        if (temp_ptr!=NULL && temp_ptr[4]=='.') {
                            strncpy(lat_pos_y,temp_ptr,9);
// Note that some GPS's put out latitude with extra precision, such as 4801.1234
                            // Check for comma char
                            if (lat_pos_y[8] == ',')
                                lat_pos_y[8] = '\0';
                            lat_pos_y[9]='\0';
                            temp_ptr=strtok(NULL,",");  /* get N-S */
                            if (temp_ptr!=NULL) {
                                strncpy(temp_data,temp_ptr,1);
                                lat_ns=toupper((int)temp_data[0]);
                                if(lat_ns =='N' || lat_ns =='S') {
                                    temp_ptr=strtok(NULL,",");  /* get long */
                                    if (temp_ptr!=NULL && temp_ptr[5] == '.') {
                                        strncpy(long_pos_x,temp_ptr,10);
// Note that some GPS's put out longitude with extra precision, such as 12201.1234
                                        // Check for comma char
                                        if (long_pos_x[9] == ',')
                                            long_pos_x[9] = '\0';
                                        long_pos_x[10]='\0';
                                        temp_ptr=strtok(NULL,",");  /* get E-W */
                                        if (temp_ptr!=NULL) {
                                            strncpy(temp_data,temp_ptr,1);
                                            long_ew=toupper((int)temp_data[0]);
                                            if (long_ew =='E' || long_ew =='W') {
                                                temp_ptr=strtok(NULL,",");  /* Get speed */
                                                if (temp_ptr!=0) {
                                                    strncpy(speed,temp_ptr,9);
                                                    speed_unit='K';
                                                    temp_ptr=strtok(NULL,",");  /* Get course */
                                                    if (temp_ptr!=NULL) {
                                                        strncpy(course,temp_ptr,7);
                                                        temp_ptr=strtok(NULL,",");   /* get date of fix */
                                                        if (temp_ptr!=NULL) {
                                                            strncpy(sampledate, temp_ptr, 6);
                                                            sampledate[6]='\0';
                                                        }
                                                        /* Data good? */
                                                        ok=1;
                                                        xastir_snprintf(long_pos, long_pos_length, "%s%c", long_pos_x,long_ew);
                                                        xastir_snprintf(lat_pos, lat_pos_length, "%s%c", lat_pos_y, lat_ns);
                                                        strcpy(spd,speed);
                                                        xastir_snprintf(unit, unit_length, "%c", speed_unit);
                                                        strcpy(cse,course);

#ifdef HAVE_STRPTIME
                                                        /* Translate date/time into time_t */
                                                        /* GPS time is in UTC.
                                                         * First, save existing TZ
                                                         * Then set conversion to
                                                         * UTC, then set back to
                                                         * existing TZ
                                                         */
                                                        tzp=getenv("TZ");
                                                        xastir_snprintf(tzn, 512, "TZ=%s", tzp);
                                                        putenv("TZ=UTC");
                                                        tzset();
                                                        xastir_snprintf(sampledatime, 15, "%s%s", sampledate, sampletime);
                                                        (void)strptime(sampledatime, "%d%m%y%H%M%S", &stm);
                                                        *stim=mktime(&stm);
                                                        putenv(tzn);
                                                        tzset();
#endif  // HAVE_STRPTIME
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    //fprintf(stderr,"Speed %s\n",spd);
    return(ok);
}





// This function is destructive to its first parameter
int decode_gps_gga( char *data,
                    char *long_pos,
                    int long_pos_length,
                    char *lat_pos,
                    int lat_pos_length,
                    char *sats,
                    char *alt,
                    char *aunit ) {

    char *temp_ptr;
    char temp_data[MAX_TNC_LINE_SIZE+1];    // Big in case we get concatenated packets (it happens!)
    char long_pos_x[11];
    char long_ew;
    char lat_pos_y[10];
    char lat_ns;
    char sats_visible[4];
    char altitude[8];
    char alt_unit;
    int ok;

// We should check for a minimum line length before parsing,
// and check for end of input while tokenizing.

    ok=0;

    if ( (data == NULL) || (strlen(data) < 35) )  // Not enough data to parse position from.
        return(ok);

    if (strncmp(data,"$GPGGA,",7)==0) {
        if (strchr(data,',')!=NULL) {
            if (strtok(data,",")!=NULL) {   /* get GPGGA and skip it */
                if(strtok(NULL,",")!=NULL) { /* get time and skip it */
                    temp_ptr = strtok(NULL,","); /* get latitude */
                    if (temp_ptr !=NULL) {
                        strncpy(lat_pos_y,temp_ptr,9);
// Note that some GPS's put out latitude with extra precision, such as 4801.1234
                        // Check for comma char
                        if (lat_pos_y[8] == ',')
                            lat_pos_y[8] = '\0';
                        lat_pos_y[9]='\0';
                        temp_ptr = strtok(NULL,",");    /* get N-S */
                        if (temp_ptr!=NULL) {
                            strncpy(temp_data,temp_ptr,1);
                            lat_ns=toupper((int)temp_data[0]);
                            if(lat_ns == 'N' || lat_ns == 'S') {
                                temp_ptr = strtok(NULL,",");                             /* get long */
                                if(temp_ptr!=NULL) {
                                    strncpy(long_pos_x,temp_ptr,10);
// Note that some GPS's put out longitude with extra precision, such as 12201.1234
                                    // Check for comma char
                                    if (long_pos_x[9] == ',')
                                        long_pos_x[9] = '\0';
                                    long_pos_x[10]='\0';
                                    temp_ptr = strtok(NULL,",");                          /* get E-W */
                                    if (temp_ptr!=NULL) {
                                        strncpy(temp_data,temp_ptr,1);
                                        long_ew=toupper((int)temp_data[0]);
                                        if (long_ew == 'E' || long_ew == 'W') {
                                            temp_ptr = strtok(NULL,",");                    /* get FIX Quality */
                                            if (temp_ptr!=NULL) {
                                                strncpy(temp_data,temp_ptr,2);
                                                 if(temp_data[0]=='1' || temp_data[0] =='2' ) {
                                                    temp_ptr=strtok(NULL,",");                /* Get sats vis */
                                                    if (temp_ptr!=NULL) {
                                                        strncpy(sats_visible,temp_ptr,4);
                                                        temp_ptr=strtok(NULL,",");             /* get hoz dil */
                                                        if (temp_ptr!=NULL) {
                                                            temp_ptr=strtok(NULL,",");          /* Get altitude */
                                                            if (temp_ptr!=NULL) {
                                                                strcpy(altitude,temp_ptr);       /* Get altitude */
                                                                temp_ptr=strtok(NULL,",");       /* get UNIT */
                                                                if (temp_ptr!=NULL) {
                                                                    strncpy(temp_data,temp_ptr,1);/* get UNIT */
                                                                    alt_unit=temp_data[0];
                                                                    /* Data good? */
                                                                    ok=1;
                                                                    xastir_snprintf(long_pos, long_pos_length, "%s%c", long_pos_x, long_ew);
                                                                    xastir_snprintf(lat_pos, lat_pos_length, "%s%c", lat_pos_y, lat_ns);
                                                                    strcpy(sats,sats_visible);
                                                                    strcpy(alt,altitude);
                                                                    sprintf(aunit,"%c",alt_unit);
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return(ok);
}





//
// Note that the length of "gps_line_data" can be up to
// MAX_DEVICE_BUFFER, which is currently set to 4096.
//
int gps_data_find(char *gps_line_data, int port) {

    char long_pos[20],lat_pos[20],aunit[2];
    time_t t;
    struct timeval tv;
    struct timezone tz;
    char temp_str[MAX_GPS_STRING+1];
    int have_valid_string = 0;


    if (strncmp(gps_line_data,"$GPRMC,",7)==0) {

        if (debug_level & 128) {
            char filtered_data[MAX_LINE_SIZE+1];

            // Make sure not to overrun our local variable
            strncpy(filtered_data, gps_line_data, MAX_LINE_SIZE);
            filtered_data[MAX_LINE_SIZE] = '\0';    // Terminate it
            
            makePrintable(filtered_data);
            fprintf(stderr,"Got RMC %s\n", filtered_data);
        }

        if (debug_level & 128) {
            // Got GPS RMC String
            statusline(langcode("BBARSTA015"),0);
        }

        strncpy(gps_gprmc, gps_line_data, MAX_GPS_STRING);
        gps_gprmc[MAX_GPS_STRING] = '\0';   // Terminate it

        strcpy(temp_str, gps_gprmc);
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
                            &t ) == 1) {    /* mod station data */
            /* got GPS data */
            have_valid_string++;
            if (debug_level & 128)
                fprintf(stderr,"RMC <%s> <%s><%s> %c <%s>\n",
                    long_pos,lat_pos,gps_spd,gps_sunit[0],gps_cse);

            if (debug_level & 128) {
                fprintf(stderr,"Checking for Time Set on %d (%d)\n",
                    port, devices[port].set_time);
            }

            if (devices[port].set_time) {
                tv.tv_sec=t;
                tv.tv_usec=0;
                tz.tz_minuteswest=0;
                tz.tz_dsttime=0;

                if (debug_level & 128) {
                    fprintf(stderr,"Setting Time %ld EUID: %d, RUID: %d\n",
                        (long)t, (int)getuid(), (int)getuid());
                }
#ifdef HAVE_SETTIMEOFDAY
                settimeofday(&tv, &tz);
#endif  // HAVE_SETTIMEOFDAY
            }
        }
    }
    else {
        if (debug_level & 128) {
            int i;
            fprintf(stderr,"Not $GPRMC: ");
            for (i = 0; i<7; i++)
                fprintf(stderr,"%c", gps_line_data[i]);
            fprintf(stderr,"\n");
        }
    }

    if (strncmp(gps_line_data,"$GPGGA,",7)==0) {

        if (debug_level & 128) {
            char filtered_data[MAX_LINE_SIZE+1];

            strncpy(filtered_data, gps_line_data, MAX_LINE_SIZE);
            filtered_data[MAX_LINE_SIZE] = '\0';    // Terminate it

            makePrintable(filtered_data);
            fprintf(stderr,"Got GGA %s\n", filtered_data);
        }

        if (debug_level & 128) {
            // Got GPS GGA String
            statusline(langcode("BBARSTA016"),0);
        }

        strncpy(gps_gpgga, gps_line_data, MAX_GPS_STRING);
        gps_gpgga[MAX_GPS_STRING] = '\0';   // Terminate it

        strcpy(temp_str, gps_gpgga);

        // decode_gps_gga is destructive to its first parameter
        if ( decode_gps_gga( temp_str,
                             long_pos,
                             sizeof(long_pos),
                             lat_pos,
                             sizeof(lat_pos),
                             gps_sats,
                             gps_alt,
                             aunit) == 1) { /* mod station data */
            /* got GPS data */
            have_valid_string++;
            if (debug_level & 128)
                fprintf(stderr,"GGA <%s> <%s> <%s> <%s> %c\n",
                    long_pos,lat_pos,gps_sats,gps_alt,aunit[0]);
        }
    }
    else {
        if (debug_level & 128) {
            int i;
            fprintf(stderr,"Not $GPGGA: ");
            for (i = 0; i<7; i++)
                fprintf(stderr,"%c",gps_line_data[i]);
            fprintf(stderr,"\n");
        }
    }


    if (have_valid_string) {

        if (debug_level & 128) {
            statusline(langcode("BBARSTA037"),0);
        }

        // Go update my screen position
        my_station_gps_change(long_pos,lat_pos,gps_cse,gps_spd,
            gps_sunit[0],gps_alt,gps_sats);

        // gps_stop_now is how we tell main.c that we've got a valid GPS string.
        // Only useful for HSP mode?
        if (!gps_stop_now)
            gps_stop_now=1;

        /* If HSP port, shutdown gps for timed interval */
        if (port_data[port].device_type == DEVICE_SERIAL_TNC_HSP_GPS) {
            /* return dtr to normal */
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
char *nmea_checksum(char *nmea_sentence) {
    int i;
    int sum = 0;
    int right;
    int left;
    char convert[17] = "0123456789ABCDEF";


    for (i = 1; i < (strlen(nmea_sentence) - 1); i++) {
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
void create_garmin_waypoint(long latitude,long longitude,char *call_sign) {
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
        len = 9;

    j = 0;
    for (i = 0; i <= len; i++) {    // Copy the '\0' as well
        if (call_sign[i] != '-') {  // We don't want the dash
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

    xastir_snprintf(out_string2,
        sizeof(out_string2),
        "%s%s",
        out_string,
        checksum);

    output_waypoint_data(out_string2);

    //fprintf(stderr,"%s\n",out_string2);
}


