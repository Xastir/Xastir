/*
 * $Id$
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2002  The Xastir Group
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


char gps_gprmc[MAX_GPS_STRING];
char gps_gpgga[MAX_GPS_STRING];

int got_gps_gprmc;
int got_gps_gpgga;
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
    char long_pos_x[10];
    char long_ew;
    char lat_pos_y[9];
    char lat_ns;
    char speed[9];
    char speed_unit;
    char course[7];
    char sampledate[7];
    char sampledatime[15];
    char *tzp;
    char tzn[512];
    struct tm stm;
    int ok;

// We should check for a miniumum line length before parsing,
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
                            strncpy(lat_pos_y,temp_ptr,8);
// Note that some GPS's put out latitude with extra precision, such as 4801.1234
                            lat_pos_y[8]='\0';
                            temp_ptr=strtok(NULL,",");  /* get N-S */
                            if (temp_ptr!=NULL) {
                                strncpy(temp_data,temp_ptr,1);
                                lat_ns=toupper((int)temp_data[0]);
                                if(lat_ns =='N' || lat_ns =='S') {
                                    temp_ptr=strtok(NULL,",");  /* get long */
                                    if (temp_ptr!=NULL && temp_ptr[5] == '.') {
                                        strncpy(long_pos_x,temp_ptr,9);
// Note that some GPS's put out longitude with extra precision, such as 12201.1234
                                        long_pos_x[9]='\0';
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
    char long_pos_x[10];
    char long_ew;
    char lat_pos_y[9];
    char lat_ns;
    char sats_visible[4];
    char altitude[8];
    char alt_unit;
    int ok;

// We should check for a miniumum line length before parsing,
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
                        strncpy(lat_pos_y,temp_ptr,8);
// Note that some GPS's put out latitude with extra precision, such as 4801.1234
                        lat_pos_y[8]='\0';
                        temp_ptr = strtok(NULL,",");    /* get N-S */
                        if (temp_ptr!=NULL) {
                            strncpy(temp_data,temp_ptr,1);
                            lat_ns=toupper((int)temp_data[0]);
                            if(lat_ns == 'N' || lat_ns == 'S') {
                                temp_ptr = strtok(NULL,",");                             /* get long */
                                if(temp_ptr!=NULL) {
                                    strncpy(long_pos_x,temp_ptr,9);
// Note that some GPS's put out longitude with extra precision, such as 12201.1234
                                    long_pos_x[9]='\0';
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





void gps_data_find(char *gps_line_data, int port) {

    char long_pos[20],lat_pos[20],sats[4],alt[8],spd[10],aunit[2],sunit[2],cse[10];
    time_t t;
    struct timeval tv;
    struct timezone tz;
    char temp_str[MAX_GPS_STRING];


    if (strncmp(gps_line_data,"$GPRMC,",7)==0 && !gps_stop_now) {
        if (debug_level & 128) {
            char filtered_data[MAX_LINE_SIZE+1];
            strcpy(filtered_data, gps_line_data);
            makePrintable(filtered_data);
            printf("Got RMC %s\n", filtered_data);
        }
        statusline(langcode("BBARSTA015"),0);
        strcpy(gps_gprmc,gps_line_data);
        strcpy(temp_str,gps_line_data);
        got_gps_gprmc=1;
        if (decode_gps_rmc( temp_str,
                            long_pos,
                            sizeof(long_pos),
                            lat_pos,
                            sizeof(lat_pos),
                            spd,
                            sunit,
                            sizeof(sunit),
                            cse,
                            &t ) == 1) {    /* mod station data */
            /* got GPS data */
            my_station_gps_change(long_pos,lat_pos,cse,spd,
                sunit[0],alt,sats);
        }
 
    }

    if(strncmp(gps_line_data,"$GPGGA,",7)==0 && !gps_stop_now) {
        if (debug_level & 128) {
            char filtered_data[MAX_LINE_SIZE+1];
            strcpy(filtered_data, gps_line_data);
            makePrintable(filtered_data);
            printf("Got GGA %s\n", filtered_data);
        }
        statusline(langcode("BBARSTA016"),0);
        strcpy(gps_gpgga,gps_line_data);
        strcpy(temp_str,gps_line_data);
        got_gps_gpgga=1;
        if ( decode_gps_gga( temp_str,
                             long_pos,
                             sizeof(long_pos),
                             lat_pos,
                             sizeof(lat_pos),
                             sats,
                             alt,
                             aunit) == 1) { /* mod station data */
            /* got GPS data */
            my_station_gps_change(long_pos,lat_pos,cse,spd,
                sunit[0],alt,sats);
        }
    }

    if (debug_level & 128)
        printf("Is GPS data complete?...");

    if (got_gps_gpgga==1 && got_gps_gprmc==1) { /* decode data and shutdown gps port for timed interval */
        if (debug_level & 128)
            printf("Yes.\n");
            statusline(langcode("BBARSTA037"),0);
        if (!gps_stop_now) {
            gps_stop_now=1;
            if (decode_gps_rmc(gps_gprmc,
                                long_pos,
                                sizeof(long_pos),
                                lat_pos,
                                sizeof(lat_pos),
                                spd,
                                sunit,
                                sizeof(sunit),
                                cse,
                                &t)==1) { /* mod station data */

                if (debug_level & 128) {
                    printf("Checking for Time Set on %d (%d)\n",
                        port, devices[port].set_time);
                }

                if (devices[port].set_time) {
                    tv.tv_sec=t;
                    tv.tv_usec=0;
                    tz.tz_minuteswest=0;
                    tz.tz_dsttime=0;

                    if (debug_level & 128) {
                        printf("Setting Time %ld EUID: %d, RUID: %d\n",
                            (long)t, (int)getuid(), (int)getuid());
                    }
#ifdef __linux__
                    settimeofday(&tv, &tz);
#endif
                }

                if (debug_level & 128)
                    printf("RMC <%s> <%s><%s> %c <%s>\n",
                        long_pos,lat_pos,spd,sunit[0],cse);

                if (decode_gps_gga( gps_gpgga,
                                    long_pos,
                                    sizeof(long_pos),
                                    lat_pos,
                                    sizeof(lat_pos),
                                    sats,
                                    alt,
                                    aunit)==1) {     /* mod station data */
                    got_gps_gpgga=0;
                    if (debug_level & 128)
                        printf("GGA <%s> <%s> <%s> <%s> %c\n",
                            long_pos,lat_pos,sats,alt,aunit[0]);
                    got_gps_gprmc=0;

                    /* got GPS data */
                    my_station_gps_change(long_pos,lat_pos,cse,spd,
                        sunit[0],alt,sats);
                    if (port_data[port].device_type == DEVICE_SERIAL_TNC_HSP_GPS) {
                        /* return dtr to normal */
                        port_dtr(port,0);
                    }
                }
            }
        }
    }
    else {
        if (debug_level & 128)
            printf("No.\n");
    }
}


