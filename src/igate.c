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

#include <termios.h>
#include <pthread.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <Xm/XmAll.h>
#include <X11/Xatom.h>
#include <X11/Shell.h>

#include "igate.h"
#include "main.h"
#include "xastir.h"
#include "interface.h"
#include "xa_config.h"
#include "util.h"


time_t last_nws_stations_file_time = 0;
int NWS_stations = 0;
int max_NWS_stations = 0;
NWS_Data *NWS_station_data;





/****************************************************************/
/* output data to inet interfaces                               */
/* line: data to send out                                       */
/* port: port data came from                                    */
/****************************************************************/
void output_igate_net(char *line, int port, int third_party) {
    char data_txt[MAX_LINE_SIZE+5];
    char temp[MAX_LINE_SIZE+5];
    char *call_sign;
    char *path;
    char *message;
    int len,i,x,first;
    int igate_options;

    call_sign = NULL;
    path      = NULL;
    message   = NULL;
    first     = 1;

    if (line == NULL)
        return;

    if (line[0] == '\0')
        return;

    // Should we Igate from RF->NET?
    if (operate_as_an_igate <= 0)
        return;

    strcpy(temp,line);

    // Check for null call_sign field
    call_sign = strtok(temp,">");
    if (call_sign == NULL)
        return;

    // Check for null path field
    path = strtok(NULL,":");
    if (path == NULL)
        return;

    // Check for "TCPIP" or "TCPXX" in the path.  If found, don't
    // gate this into the internet again, it's already been gated to
    // RF, which means it's already been on the 'net.  No looping
    // allowed here...
    if ( (strstr(path,"TCPXX") != NULL)
            || (strstr(path,"TCPIP") != NULL) ) {
 
            if (log_igate && (debug_level & 1024) ) {

                xastir_snprintf(temp,
                    sizeof(temp),
                    "IGATE RF->NET(%c):%s\n",
                    third_party ? 'T':'N',
                    line);
                log_data(LOGFILE_IGATE,temp);

                xastir_snprintf(temp,
                    sizeof(temp),
                    "REJECT: Packet has been gated before\n");
                log_data(LOGFILE_IGATE,temp);

                printf(temp);
        }
        return;
    }
 
    // Check for null message field
    message = strtok(NULL,"");
    if (message == NULL)
        return;

    // Check for third party messages.  We don't want to gate these
    // back onto the internet feeds
    if (message[0] == '}') {

        if (log_igate && (debug_level & 1024) ) {

            xastir_snprintf(temp,
                sizeof(temp),
                "IGATE RF->NET(%c):%s\n",
                third_party ? 'T':'N',
                line);
            log_data(LOGFILE_IGATE,temp);

            xastir_snprintf(temp,
                sizeof(temp),
                "REJECT: Third party traffic\n");
            log_data(LOGFILE_IGATE,temp);

            printf(temp);
        }
        return;
    }

    len = (int)strlen(call_sign);
    for (i=0;i<len;i++) {

        // just in case we see an asterisk get rid of it
        if (call_sign[i] == '*') {
            call_sign[i] = '\0';
            i = len+1;
        }
    }

    // Check for my callsign
    if (strcmp(call_sign,my_callsign) == 0) {

        if (log_igate && (debug_level & 1024) ) {

            xastir_snprintf(temp,
                sizeof(temp),
                "IGATE RF->NET(%c):%s\n",
                third_party ? 'T':'N',
                line);
            log_data(LOGFILE_IGATE,temp);

            xastir_snprintf(temp,
                sizeof(temp),
                "REJECT: From my call\n");
            log_data(LOGFILE_IGATE,temp);

            printf(temp);
        }
        return;
    }
 
    // Should I filter out more here.. get rid of all data
    // or Look in the path for things line "AP" "GPS" "ID" etc..?

begin_critical_section(&devices_lock, "igate.c:output_igate_net" );

    igate_options = devices[port].igate_options;

end_critical_section(&devices_lock, "igate.c:output_igate_net" );

    if (igate_options <= 0 ) {

        if (log_igate && (debug_level & 1024) ) {

            xastir_snprintf(temp,
                sizeof(temp),
                "IGATE RF->NET(%c):%s\n",
                third_party ? 'T':'N',
                line);
            log_data(LOGFILE_IGATE,temp);

            xastir_snprintf(temp,
                sizeof(temp),
                "REJECT: No RF->NET from input port [%d]\n",
                port);
            log_data(LOGFILE_IGATE,temp);

            printf(temp);
        }
        return;
    }

    xastir_snprintf(data_txt,
        sizeof(data_txt),
        "%s%c%c",
        line,
        '\r',
        '\n');

    // write data out to net interfaces
    for (x = 0; x < MAX_IFACE_DEVICES; x++) {

        if (port_data[x].device_type == DEVICE_NET_STREAM
                && x!=port && port_data[x].status == DEVICE_UP) {

            // log traffic for the first "up" interface only
            if (log_igate && first) {
                xastir_snprintf(temp,
                    sizeof(temp),
                    "IGATE RF->NET(%c):%s\n",
                    third_party ? 'T':'N',
                    line);
                log_data(LOGFILE_IGATE,temp);

                first = 0;
            }

            xastir_snprintf(temp,
                sizeof(temp),
                "TRANSMIT: IGate RF->NET packet on device:%d\n",
                x);

            // log output
            if (log_igate)
                log_data(LOGFILE_IGATE,temp);

            if (debug_level & 1024)
                printf("%s\n",temp);

            // Write this data out to the Inet port
            // The "1" means raw format, the last digit
            // says to _not_ use the unproto_igate path
            output_my_data(data_txt,x,1,0,0,NULL);
        }
    }
}





/****************************************************************/
/* output data to tnc interfaces                                */
/* from: type of port heard from                                */
/* call: call sign heard from                                   */
/* line: data to gate to rf                                     */
/* port: port data came from                                    */
/****************************************************************/
void output_igate_rf(char *from, char *call, char *path, char *line, int port, int third_party) {
    char temp[MAX_LINE_SIZE+20];
    int x;
    int first = 1;


    if ( (from == NULL) || (call == NULL) || (path == NULL) || (line == NULL) )
        return;

    if ( (from[0] == '\0') || (call[0] == '\0') || (path[0] == '\0') || (line[0] == '\0') )
        return;

    // Should we Igate from NET->RF?
    if (operate_as_an_igate <= 1)
        return;

    // Check for TCPXX* in string.
    if (strstr(path,"TCPXX*") != NULL) {
        // "TCPXX*" was found in the header.  We have an unregistered user.
        if (log_igate && (debug_level & 1024) ) {
            xastir_snprintf(temp,
                sizeof(temp),
                "IGATE NET->RF(%c):%s\n",
                third_party ? 'T':'N',
                line);
            log_data(LOGFILE_IGATE,temp);

            xastir_snprintf(temp,
                sizeof(temp),
                "REJECT: Unregistered net user!\n");
            log_data(LOGFILE_IGATE,temp);
            printf(temp);
        }
        return;
    }

    // Check whether the source and destination calls have been
    // heard on local RF.
    if (!heard_via_tnc_in_past_hour(call)==1        // Haven't heard destination call in previous hour
            || heard_via_tnc_in_past_hour(from)) {  // Have heard source call in previous hour

        if (log_igate && (debug_level & 1024) ) {
            xastir_snprintf(temp,
                sizeof(temp),
                "IGATE NET->RF(%c):%s\n",
                third_party ? 'T':'N',
                line);
            log_data(LOGFILE_IGATE,temp);

        //  heard(call),  heard(from) : RF-to-RF talk
        // !heard(call),  heard(from) : Destination not heard on TNC
        // !heard(call), !heard(from) : Destination/source not heard on TNC

        if (!heard_via_tnc_in_past_hour(call))
            xastir_snprintf(temp,
                sizeof(temp),
                "REJECT: Destination not heard on TNC within an hour %s\n",
                call );
        else
            xastir_snprintf(temp,
                sizeof(temp),
                "REJECT: RF->RF talk\n");
            log_data(LOGFILE_IGATE,temp);
            printf(temp);
        }
        return;
    }
 


    // Station we are going to is heard via tnc but station sending
    // shouldn't be heard via TNC.  Write data out to interfaces.
    for (x=0; x<MAX_IFACE_DEVICES;x++) {
        if (x != port) {
            switch (port_data[x].device_type) {
                case DEVICE_SERIAL_TNC_AUX_GPS:

                case DEVICE_SERIAL_TNC_HSP_GPS:

                case DEVICE_SERIAL_TNC:

                case DEVICE_AX25_TNC:

                case DEVICE_SERIAL_KISS_TNC:

begin_critical_section(&devices_lock, "igate.c:output_igate_rf" );

                    if (devices[x].igate_options>1 && port_data[x].status==DEVICE_UP) {

                        // log traffic for first "up" interface only
                        if (log_igate && first) {
                            xastir_snprintf(temp,
                                sizeof(temp),
                                "IGATE NET->RF(%c):%s\n",
                                third_party ? 'T':'N',
                                line);
                            log_data(LOGFILE_IGATE,temp);
                            first = 0;
                        }

                        xastir_snprintf(temp,
                            sizeof(temp),
                            "TRANSMIT: IGate NET->RF packet on device:%d\n",
                            x);

                        // log output
                        if (log_igate)
                            log_data(LOGFILE_IGATE,temp);

                        if (debug_level & 1024)
                            printf(temp);

                        // ok write this data out to the RF port

end_critical_section(&devices_lock, "igate.c:output_igate_rf" );

                        // First "0" means "cooked"
                        // format, last digit: use
                        // unproto_igate path
                        output_my_data(line,x,0,0,1,NULL);

begin_critical_section(&devices_lock, "igate.c:output_igate_rf" );

                    }
                    else {
                        if (log_igate && (debug_level & 1024) ) {
                            xastir_snprintf(temp,
                                sizeof(temp),
                                "IGATE NET->RF(%c):%s\n",
                                third_party ? 'T':'N',
                                line);
                            log_data(LOGFILE_IGATE,temp);

                            xastir_snprintf(temp,
                                sizeof(temp),
                                "REJECT: NET->RF on port [%d]\n",
                                x);
                            log_data(LOGFILE_IGATE,temp);
                            printf(temp);
                        }
                    }

end_critical_section(&devices_lock, "igate.c:output_igate_rf" );

                    break;

                default:
                    break;
            }
        }
    }
}





void add_NWS_stations(void) {
    void *tmp_ptr;

    if (NWS_stations>=max_NWS_stations) {
        if ((tmp_ptr = realloc(NWS_station_data, sizeof(NWS_Data)*(max_NWS_stations+11)))) {
            NWS_station_data = tmp_ptr;
            max_NWS_stations += 10;
        }
    }
}





/****************************************************************/
/* Load NWS stations file                                       */
/* file: file to read                                           */
/****************************************************************/
void load_NWS_stations(char *file) {
    FILE *f;
    char line[40];

    if (file == NULL)
        return;

    if (file[0] == '\0')
        return;

    f = fopen(file,"r");
    if (f!=NULL) {
        while (!feof(f)) {
            if (strlen(get_line(f,line,39))>0) {
                // look for comment
                if (line[0] != '#' ) {
                    NWS_stations++;
                    add_NWS_stations();
                    if (NWS_station_data != NULL) {
                        // add data
                        (void)sscanf(line,"%s",NWS_station_data[NWS_stations-1].call);
                        if (debug_level & 1024)
                            printf("LINE:%s\n",line);
                    }
                    else {
                        printf("Can't allocate data space for NWS station\n");
                    }
                }
            }
        }
        (void)fclose(f);
    }
    else
        printf("Couldn't open NWS stations file: %s\n", file);
}





/****************************************************************/
/* check NWS stations file                                      */
/* call: call to check                                          */
/* returns 1 for found                                          */
/****************************************************************/
int check_NWS_stations(char *call) {
    int ok,i;

    if (debug_level & 1024)
	    printf("igate.c::check_NWS_stations %s\n", call);

    if (call == NULL)
        return(0);

    if (call[0] == '\0')
        return(0);

    ok=0;
    for (i=0; i<NWS_stations && !ok; i++) {
        if (strcasecmp(call,NWS_station_data[i].call)==0) {
            ok=1; // match found 
	        if (debug_level && 1024) {
                printf("NWS-MATCH:(%s) (%s)\n",NWS_station_data[i].call,call);
	        }
        }
    }
    return(ok);
}





/****************************************************************/
/* output NWS data to tnc interfaces                            */
/* from: type of port heard from                                */
/* call: call sign heard fro                                    */
/* line: data to gate to rf                                     */
/* port: port data came from                                    */
/****************************************************************/
void output_nws_igate_rf(char *from, char *path, char *line, int port, int third_party) {
    char temp[MAX_LINE_SIZE+20];
    int x,first;

    first = 1;

    if ( (from == NULL) || (path == NULL) || (line == NULL) )
        return;

    if ( (from[0] == '\0') || (path[0] == '\0') || (line[0] == '\0') )
        return;

    // should we output as an Igate?
    if (operate_as_an_igate <= 1)
        return;

    // check for TCPXX* in string!  If found, we have an unregistered net user.
    if (strstr(path,"TCPXX*") != NULL) {
        if (log_igate && (debug_level & 1024) ) {
            xastir_snprintf(temp,
                sizeof(temp),
                "NWS IGATE NET->RF(%c):%s\n",
                third_party ? 'T':'N',
                line);
            log_data(LOGFILE_IGATE,temp);

            xastir_snprintf(temp,
                sizeof(temp),
                "REJECT: Unregistered net user!\n");
            log_data(LOGFILE_IGATE,temp);
            printf(temp);
        }
        return;
    }

    // no unregistered net user found in string

    // see if we can gate NWS messages
    if (!filethere(get_user_base_dir("data/nws-stations.txt"))) {
        if (log_igate && (debug_level & 1024) ) {
            xastir_snprintf(temp,
                sizeof(temp),
                "NWS IGATE NET->RF(%c):%s\n",
                third_party ? 'T':'N',
                line);
            log_data(LOGFILE_IGATE,temp);

            xastir_snprintf(temp,
                sizeof(temp),
                "REJECT: No nws-stations.txt file!\n");
            log_data(LOGFILE_IGATE,temp);
            printf(temp);
        }
        return;
    }

    // check to see if the nws-stations file is newer than last read
    if (last_nws_stations_file_time < file_time(get_user_base_dir("data/nws-stations.txt"))) {
        last_nws_stations_file_time = file_time(get_user_base_dir("data/nws-stations.txt"));
        load_NWS_stations(get_user_base_dir("data/nws-stations.txt"));
        //printf("NWS Station file time is old\n");
    }

    // Look for NWS station in file data
    if (!check_NWS_stations(from)){ // Couldn't find the station

        if (log_igate && (debug_level & 1024) ) {
            xastir_snprintf(temp,
                sizeof(temp),
                "NWS IGATE NET->RF(%c):%s\n",
                third_party ? 'T':'N',
                line);
            log_data(LOGFILE_IGATE,temp);

            xastir_snprintf(temp,
                sizeof(temp),
                "REJECT: No matching station in nws-stations.txt file!\n");
            log_data(LOGFILE_IGATE,temp);
            printf(temp);
        }
        return;
    }

    //printf("SENDING NWS VIA TNC!!!!\n");
    // write data out to interfaces
    for (x=0; x<MAX_IFACE_DEVICES;x++) {
        if (x!=port) {
            switch (port_data[x].device_type) {
                case DEVICE_SERIAL_TNC_AUX_GPS:

                case DEVICE_SERIAL_TNC_HSP_GPS:

                case DEVICE_SERIAL_TNC:

                case DEVICE_AX25_TNC:

                case DEVICE_SERIAL_KISS_TNC:

begin_critical_section(&devices_lock, "igate.c:output_nws_igate_rf" );

                    if (devices[x].igate_options>1
                            && port_data[x].status==DEVICE_UP) {

                        // log traffic for first "up" interface only
                        if (log_igate && first) {
                            xastir_snprintf(temp,
                                sizeof(temp),
                                "NWS IGATE NET->RF(%c):%s\n",
                                third_party ? 'T':'N',
                                line);
                            log_data(LOGFILE_IGATE,temp);
                            first = 0;
                        }

                        xastir_snprintf(temp,
                            sizeof(temp),
                            "TRANSMIT: IGate NET->RF packet on device:%d\n",
                            x);

                        // log output
                        if (log_igate)
                            log_data(LOGFILE_IGATE,temp);

                        if (debug_level & 1024)
                            printf(temp);

                        // ok write this data out to the RF port

end_critical_section(&devices_lock, "igate.c:output_nws_igate_rf" );

                        // First "0" means "cooked"
                        // format, last digit: use
                        // unproto_igate path
                        output_my_data(line,x,0,0,1,NULL);

begin_critical_section(&devices_lock, "igate.c:output_nws_igate_rf" );

                    }
                    else {
                        if (log_igate && (debug_level & 1024) ) {
                            xastir_snprintf(temp,
                                sizeof(temp),
                                "NWS IGATE NET->RF(%c):%s\n",
                                third_party ? 'T':'N',
                                line);
                            log_data(LOGFILE_IGATE,temp);

                            xastir_snprintf(temp,
                                sizeof(temp),
                                "REJECT: NET->RF on port [%d]\n",
                                x);
                            log_data(LOGFILE_IGATE,temp);
                            printf(temp);
                        }
                    }

end_critical_section(&devices_lock, "igate.c:output_nws_igate_rf" );

                   break;

                default:
                    break;
            }
        }
    }
}


