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

/*
 AX.25 Parts adopted from: aprs_tty.c by Henk de Groot - PE1DNN
*/

#include "config.h"
#include "snprintf.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <unistd.h>
#include <dirent.h>
#include <signal.h>
#include <termios.h>
#include <pwd.h>
#include <pthread.h>
#include <termios.h>
#include <setjmp.h>
#include <sys/socket.h>
#include <fcntl.h>

#include <netinet/in.h>     // Moved ahead of inet.h as reports of some *BSD's not
                            // including this as they should.
#include <arpa/inet.h>
#include <netinet/tcp.h>    // Needed for TCP_NODELAY setsockopt() (disabling Nagle algorithm)

#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif  // HAVE_NETDB_H

#include <sys/types.h>
#include <sys/ioctl.h>

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else   // TIME_WITH_SYS_TIME
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else  // HAVE_SYS_TIME_H
#  include <time.h>
# endif // HAVE_SYS_TIME_H
#endif  // TIME_WITH_SYS_TIME

#include <errno.h>

#ifdef  HAVE_LOCALE_H
#include <locale.h>
#endif  // HAVE_LOCALE_H

#ifdef  HAVE_LIBINTL_H
#include <libintl.h>
#define _(x)        gettext(x)
#else   // HAVE_LIBINTL_H
#define _(x)        (x)
#endif  // HAVE_LIBINTL_H

#include <Xm/XmAll.h>
#include <X11/cursorfont.h>

#include "xastir.h"
#include "symbols.h"
#include "main.h"
#include "xa_config.h"
#include "maps.h"
#include "interface.h"
#include "main.h"
#include "util.h"
#include "wx.h"
#include "hostname.h"

#ifdef HAVE_LIBAX25
#include <netax25/ax25.h>
#include <netrose/rose.h>
#include <netax25/axlib.h>
#include <netax25/axconfig.h>
#endif  // HAVE_LIBAX25

#ifndef SIGRET
#define SIGRET  void
#endif  // SIGRET


//extern pid_t getpgid(pid_t pid);
extern void port_write_binary(int port, unsigned char *data, int length);
 

iodevices dtype[MAX_IFACE_DEVICE_TYPES]; // device names

iface port_data[MAX_IFACE_DEVICES];     // shared port data

int port_id[MAX_IFACE_DEVICES];         // shared port id data

xastir_mutex port_data_lock;            // Protects the port_data[] array of structs
xastir_mutex data_lock;                 // Protects global data, data_port, data_avail variables
xastir_mutex output_data_lock;          // Protects interface.c:channel_data() function only
xastir_mutex connect_lock;              // Protects port_data[].thread_status and port_data[].connect_status

void port_write_string(int port, char *data);

int ax25_ports_loaded = 0;

// decode data
unsigned char *incoming_data;
int incoming_data_length;               // Used for binary strings such as KISS
unsigned char incoming_data_copy[MAX_LINE_SIZE];    // Used for debug
int data_avail = 0;
int data_port;

// interface wait time out
int NETWORK_WAITTIME;





// Create a packet and send to AGWPE for transmission.
// Format is as follows:
//
//  RadioPort     4 bytes (0-3)
//  DataType      4 bytes (4-7)
//  FromCall     10 bytes (8-17)
//  ToCall       10 bytes (18-27)
//  DataLength    4 bytes (28-31)
//  UserField     4 bytes (32-35)
//  Data         DataLength bytes (36-?)
//
// Callsigns are null-terminated at end of string, but callsign
// field width is specified to be 10 bytes in all cases.
//
// Path is split up into the various ViaCalls.  Path may also be a
// NULL pointer.
//
// If type != '\0', then we'll create the specified type of packet.
//
// Else if Path is not empty, we'll use packet format "V" with
// Viacalls prepended to the Data portion of the packet, 10 chars
// per digi, with the number of digis as the first character.  The
// packet data then follows after the last via callsign.
//
// Else if no Path, then put the Data directly into the Data
// field and use "M" format packets.
//
void send_agwpe_packet(int xastir_interface,// Xastir interface port
                       int RadioPort,       // AGWPE RadioPort
                       unsigned char type,
                       unsigned char *FromCall,
                       unsigned char *ToCall,
                       unsigned char *Path,
                       unsigned char *Data,
                       int length) {
    int ii;
    unsigned char output_string[512];
    unsigned char path_string[200];
    int full_length;
    int data_length;


//fprintf(stderr,"Sending to RadioPort %d\n", RadioPort);
//fprintf(stderr,"Type:%c, From:%s, To:%s, Path:%s, Data:%s\n",
//    type, FromCall, ToCall, Path, Data);


    // Check size of data
    if (length > 512)
        return;

/*
fprintf(stderr,"%s %s %s\n",
    FromCall,
    ToCall,
    Path);
*/

    // Clear the output_string (set to binary zeroes)
    for (ii = 0; ii < (int)sizeof(output_string); ii++) {
        output_string[ii] = '\0';
    }

    // Write the port number into the frame
    output_string[0] = (unsigned char)RadioPort;

    if (FromCall)   // Write the FromCall string into the frame
        strcpy(&output_string[8], FromCall);

    if (ToCall) // Write the ToCall string into the frame
        strcpy(&output_string[18], ToCall);


    if ( (type != '\0') && (type != 'P') ) {
        // Type was specified, not a data frame or login frame

        // Write the type character into the frame
        output_string[4] = type;

        // Send the packet to AGWPE
        port_write_binary(xastir_interface, output_string, 36);
    }
 
    else if (Path == NULL) { // No ViaCalls, Data or login packet

        if (type == 'P') {
            // Login/Password frame
            char callsign_base[15];
            int jj;


            // Write the type character into the frame
            output_string[4] = type;

            output_string[28] = (unsigned char)(length % 256);
            output_string[29] = (unsigned char)((length >> 8) % 256);

//fprintf(stderr,"%02x %02x\n", output_string[28], output_string[29]);

            // Send the header frame to AGWPE
            port_write_binary(xastir_interface, output_string, 36);
 
            // Clear the output_string (set to binary zeroes)
            for (ii = 0; ii < 36; ii++) {
                output_string[ii] = '\0';
            }

            // Write login/password out as 255-byte strings

            // Compute the callsign base string
            // (callsign minus SSID)
            strcpy(callsign_base,my_callsign);
            for (jj = 0; jj < (int)strlen(my_callsign); jj++) {
                // Change '-' into end of string
                if (callsign_base[jj] == '-')
                    callsign_base[jj] = '\0';
            }

            // Write the login string               
            xastir_snprintf(output_string, sizeof(output_string),
                "%s", callsign_base);

            // Send the packet to AGWPE
            port_write_binary(xastir_interface, output_string, 255);
 
            // Clear the output_string (set to binary zeroes)
            for (ii = 0; ii < 10; ii++) {
                output_string[ii] = '\0';
            }

            // Write the password string
            xastir_snprintf(output_string,sizeof(output_string),
                "%s", Data);
//fprintf(stderr,"%s %d\n", output_string, strlen(output_string));

            // Send the packet to AGWPE
            port_write_binary(xastir_interface, output_string, 255);
        }
        else {  // Data frame
            // Write the type character into the frame
            output_string[4] = 'M'; // Unproto, no via calls

            // Write the PID type into the frame
            output_string[6] = 0xF0;    // UI Frame

            output_string[28] = (unsigned char)(length % 256);
            output_string[29] = (unsigned char)((length >> 8) % 256);

//fprintf(stderr,"%02x %02x\n", output_string[28], output_string[29]);

            // Copy Data onto the end of the string.  This one
            // doesn't have to be null-terminated, so strncpy() is
            // ok to use here.  strncpy stops at the first null byte
            // though.  Proper for a binary output routine?
            strncpy(&output_string[36], Data, length);

            full_length = length + 36;

            // Send the packet to AGWPE
            port_write_binary(xastir_interface, output_string, full_length);
        }
    }

    else {  // We have ViaCalls.  Data packet.
        char *ViaCall[10];

        // Doesn't need to be null-terminated, so strncpy is ok to
        // use here.  strncpy stops at the first null byte though.
        // Proper for a binary output routine?
        strncpy(path_string, Path, sizeof(path_string));

        split_string(path_string, ViaCall, 10);

        // Write the type character into the frame
        output_string[4] = 'V'; // Unproto, via calls present

        // Write the PID type into the frame
        output_string[6] = 0xF0;    // UI Frame

        // Write the number of ViaCalls into the first byte
        if (ViaCall[7])
            output_string[36] = 0x08;
        else if (ViaCall[6])
            output_string[36] = 0x07;
        else if (ViaCall[5])
            output_string[36] = 0x06;
        else if (ViaCall[4])
            output_string[36] = 0x05;
        else if (ViaCall[3])
            output_string[36] = 0x04;
        else if (ViaCall[2])
            output_string[36] = 0x03;
        else if (ViaCall[1])
            output_string[36] = 0x02;
        else
            output_string[36] = 0x01;
 
        // Write the ViaCalls into the Data field
        switch (output_string[36]) {
            case 8:
                if (ViaCall[7])
                    strcpy(&output_string[37+70], ViaCall[7]);
                else
                    return;
            case 7:
                if (ViaCall[6])
                    strcpy(&output_string[37+60], ViaCall[6]);
                else
                    return;
            case 6:
                if (ViaCall[5])
                    strcpy(&output_string[37+50], ViaCall[5]);
                else
                    return;
            case 5:
                if (ViaCall[4])
                    strcpy(&output_string[37+40], ViaCall[4]);
                else
                    return;
            case 4:
                if (ViaCall[3])
                    strcpy(&output_string[37+30], ViaCall[3]);
                else
                    return;
            case 3:
                if (ViaCall[2])
                    strcpy(&output_string[37+20], ViaCall[2]);
                else
                    return;
            case 2:
                if (ViaCall[1])
                    strcpy(&output_string[37+10], ViaCall[1]);
                else
                    return;
            case 1:
            default:
                if (ViaCall[0])
                    strcpy(&output_string[37],    ViaCall[0]);
                else
                    return;
                break;
        }

        // Write the Data onto the end.
        // Doesn't need to be null-terminated, so strncpy is ok to
        // use here.  strncpy stops at the first null byte though.
        // Proper for a binary output routine?
        strncpy(&output_string[(output_string[36] * 10) + 37], Data, length);

        //Fill in the data length field.  We're assuming the total
        //is less than 512 + 37.
        data_length = length + (output_string[36] * 10) + 1;
        if ( data_length > (512 + 37) )
            return;

        output_string[28] = (unsigned char)(data_length % 256);
        output_string[29] = (unsigned char)((data_length >> 8) % 256);

        full_length = data_length + 36;

        // Send the packet to AGWPE
        port_write_binary(xastir_interface, output_string, full_length);
    }
}





// Parse an AGWPE header.  Create a TAPR-2 style header out of the
// data for feeding into the Xastir parsing code.  Input format is
// as follows:
//
//  RadioPort     4 bytes (0-3)
//  DataType      4 bytes (4-7)
//  FromCall     10 bytes (8-17)
//  ToCall       10 bytes (18-27)
//  DataLength    4 bytes (28-31)
//  UserField     4 bytes (32-35)
//  Data         xx bytes (36-??)
//
// Callsigns are null-terminated at end of string, but field width
// is specified to be 10 bytes in all cases.
//
// output_string should be quite long, perhaps 1000 characters.
//
unsigned char *parse_agwpe_packet(unsigned char *input_string,
                                  int output_string_length,
                                  unsigned char *output_string,
                                  int *new_length) {
    int ii, jj, kk;
    unsigned char data_length;


    // Check that it's a UI packet.  It should have a 'U' in
    // position input_string[4].
    switch (input_string[4]) {
        case 'U':
//fprintf(stderr,"AGWPE: Got UI packet\n");
            break;
        case 'R':
//fprintf(stderr,"AGWPE: Got software version packet\n");
            return(NULL);
            break;
        default:
//fprintf(stderr,"AGWPE: Got '%c' packet\n",input_string[4]);
            return(NULL);
            break;
    }

    // Clear the output_string (set to binary zeroes)
    for (ii = 0; ii < output_string_length; ii++) {
        output_string[ii] = '\0';
    }

    jj = 0;

// The example in the docs shows that the packet starts with a space
// character.  Watch out for that.  We currently skip over this part
// of the string entirely:  " 2:Fm " and go straight for the
// callsign at offset 42.

    // Copy the source callsign
    ii = 42;
    while (input_string[ii] != ' ') {
        output_string[jj++] = input_string[ii++];
    }

    // Add a '>' character
    output_string[jj++] = '>';

    // Skip over the <space>To<space> portion
    ii += 4;

    // Copy the destination callsign
    while (input_string[ii] != ' ') {
        output_string[jj++] = input_string[ii++];
    }
    ii++;

    // If next characters are "Via", then we have digipeaters, else
    // we have the "<Ui pid=" portion.
    if (input_string[ii] == 'V'
            && input_string[ii+1] == 'i'
            && input_string[ii+2] == 'a') {
        output_string[jj++] = ',';   // Add a comma
        // Copy the digipeater callsigns
        // APRS Via RELAY,SAR1-1,SAR2-1,SAR3-1,SAR4-1,SAR5-1,SAR6-1,SAR7-1 <UI pid=F0
        ii += 4;
        while (input_string[ii] != ' ') {
            output_string[jj++] = input_string[ii++];
        }
        ii++;
    }

    // Add a ':' character
    output_string[jj++] = ':';

    // Skip over the "<UI pid=" portion
    ii += 8;

    // Make sure that the protocol ID is "F0"
    if (input_string[ii++] != 'F')
        return(NULL);
    if (input_string[ii++] != '0')
        return(NULL);

    // Skip over the " Len=" portion
    ii += 5;

    // Figure out how much data is in the payload
    // "Len=126"
    data_length = 0;
    while (input_string[ii] != ' ') {
        data_length = (data_length * 10) + (input_string[ii++] - 0x30);
    }
//fprintf(stderr,"parse_awgpe_packet:Length:%d\n", data_length);

    // Skip over the timestamp
    ii += 13;
    
    // Copy the data across
    kk = data_length;
    while (kk > 0) {
//fprintf(stderr,"%c",input_string[ii]);
        output_string[jj++] = input_string[ii++];
        kk--;
    }
//fprintf(stderr,"\n");

    // We end up with 0x0d characters on the end.  Make sure we
    // account for these (and get rid of them):
    // Terminate it to knock off trailing 0x0d characters
    output_string[jj] = '\0';
    if (output_string[jj-1] == 0x0d) {
        jj--;
        output_string[jj] = '\0';
//fprintf(stderr,"Found 0x0d, chopping 1\n");
    }
    if (output_string[jj-1] == 0x0d) {
        jj--;
        output_string[jj] = '\0';
//fprintf(stderr,"Found 0x0d, chopping 2\n");
    }


    *new_length = jj;

    // Print out the intermediate result
//fprintf(stderr,"AGWPE RX: %s\n", output_string);
//fprintf(stderr,"new_length: %d\n",*new_length);
//for (ii = 0; ii < strlen(output_string); ii++) {
//  fprintf(stderr,"%02x ",output_string[ii]);
//}
//fprintf(stderr,"\n");

    return(output_string);
}
/*
Found complete AGWPE packet, 93 bytes total in frame:
00 00 00 00
55 00 00 00                     'U' Packet
57 45 37 55 2d 33 00 00 ff ff   WE7U-3
41 50 52 53 00 20 ec e9 6c 00   APRS
39 00 00 00                     Length
00 00 00 00

20 31                           .1 (36-37)
3a 46 6d 20                     :Fm (38-41)
57 45 37 55 2d 33               WE7U-3 (42-space)
20 54 6f                        .To
20 41 50 52 53                  .APRS
20 3c 55 49                     .<UI
20 70 69 64 3d 46 30            .pid=F0
20 4c 65 6e 3d 34               .Len=4
20 3e 5b 32 33 3a 31 33 3a 32 30 5d 0d  >[23:13:20].
54 65 73 74 0d 0d 00            Test<CR><CR>.
....U...WE7U-3....APRS. ..l.9....... 1:Fm WE7U-3 To APRS <UI pid=F0 Len=4 >[23:13:20].Test...

1:Fm WE7U-3 To APRS Via RELAY,SAR1-1,SAR2-1,SAR3-1,SAR4-1,SAR5-1,SAR6-1,SAR7-1 <UI pid=F0 Len=26 >[23:51:46].Testing this darned thing!...
*/





//****************************************************************
// get device name only (the portion at the end of the full path)
// device_name current full name of device
//****************************************************************

char *get_device_name_only(char *device_name) {
    int i,len,done;

    if (device_name == NULL)
        return(NULL);

    done = 0;
    len = (int)strlen(device_name);
    for(i = len; i > 0 && !done; i--){
        if(device_name[i] == '/'){
            device_name += (i+1);
            done = 1;
        }
    }
    return(device_name);
}





//***********************************************************
// Get Open Device
//
// if device is available this will return the port #
// otherwise a -1 will be returned in error.
//***********************************************************
int get_open_device(void) {
    int i, found;

begin_critical_section(&devices_lock, "interface.c:get_open_device" );

    found = -1;
    for(i = 0; i < MAX_IFACE_DEVICES && found == -1; i++) {
        if (devices[i].device_type == DEVICE_NONE){
            found = i;
            break;
        }
    }

end_critical_section(&devices_lock, "interface.c:get_open_device" );

    if (found == -1)
        popup_message(langcode("POPEM00004"),langcode("POPEM00017"));

    return(found);
}





//***********************************************************
// Get Device Status
//
// this will return the device status for the port specified
//***********************************************************
int get_device_status(int port) {
    int stat;

    if (begin_critical_section(&port_data_lock, "interface.c:get_device_status(1)" ) > 0)
        fprintf(stderr,"port_data_lock, Port = %d\n", port);

    stat = port_data[port].status;

    if (end_critical_section(&port_data_lock, "interface.c:get_device_status(2)" ) > 0)
        fprintf(stderr,"port_data_lock, Port = %d\n", port);

    return(stat);
}





//***********************************************************
// channel_data()
//
// Takes data read in from a port and makes the incoming_data
// pointer point to it.  Waits until the string is processed.
//
// port #                                                    
// string is the string of data
// length is the length of the string.  If 0 then use strlen()
// on the string itself to determine the length.
//***********************************************************
void channel_data(int port, unsigned char *string, int length) {
    int max;
    struct timeval tmv;
    // Some messiness necessary because we're using xastir_mutex's
    // instead of pthread_mutex_t's.
    pthread_mutex_t *cleanup_mutex1;
    pthread_mutex_t *cleanup_mutex2;


    //fprintf(stderr,"channel_data: %x %d\n",string[0],length);

    // Save a backup copy of the incoming string.  Used for
    // debugging purposes.  If we get a segfault, we can print out
    // the last message received.
    xastir_snprintf(incoming_data_copy,
        MAX_LINE_SIZE,
        "Port%d:%s",
        port,
        string);

    max = 0;

    if (string == NULL)
        return;

    if (string[0] == '\0')
        return;

    if (length == 0)
        length = strlen(string);

    // Check for excessively long packets.  These might be TCP/IP
    // packets or concatenated APRS packets.  In any case it's some
    // kind of garbage that we don't want to try to parse.

    // Note that for binary data (WX stations and KISS packets), the
    // strlen() function may not work correctly.
    if (length > MAX_LINE_SIZE) {   // Too long!
        if (debug_level & 1) {
            fprintf(stderr,"\nchannel_data: LONG packet:%d,  Dumping it:\n%s\n",
            length,
            string);
        }

        string[0] = '\0';   // Truncate it to zero length
        return;
    }


    // Install the cleanup routine for the case where this thread
    // gets killed while the mutex is locked.  The cleanup routine
    // initiates an unlock before the thread dies.  We must be in
    // deferred cancellation mode for the thread to have this work
    // properly.  We must first get the pthread_mutex_t address:
    cleanup_mutex1 = &output_data_lock.lock;

    // Then install the cleanup routine:
    pthread_cleanup_push((void *)pthread_mutex_unlock, (void *)cleanup_mutex1);


    // This protects channel_data from being run by more than one
    // thread at the same time.
    if (begin_critical_section(&output_data_lock, "interface.c:channel_data(1)" ) > 0)
        fprintf(stderr,"output_data_lock, Port = %d\n", port);


    if (length > 0) {


        // Install the cleanup routine for the case where this
        // thread gets killed while the mutex is locked.  The
        // cleanup routine initiates an unlock before the thread
        // dies.  We must be in deferred cancellation mode for the
        // thread to have this work properly.  We must first get the
        // pthread_mutex_t address.
        cleanup_mutex2 = &data_lock.lock;

        // Then install the cleanup routine:
        pthread_cleanup_push((void *)pthread_mutex_unlock, (void *)cleanup_mutex2);

        if (begin_critical_section(&data_lock, "interface.c:channel_data(2)" ) > 0)
            fprintf(stderr,"data_lock, Port = %d\n", port);


        // If it's any of three types of GPS ports and is a GPRMC or
        // GPGGA string, just stick it in one of two global
        // variables for holding such strings.  UpdateTime() can
        // come along and process/clear-out those strings at the
        // gps_time interval.
        //
        switch(port_data[port].device_type) {

            case DEVICE_SERIAL_GPS:
            case DEVICE_SERIAL_TNC_HSP_GPS:
            case DEVICE_NET_GPSD:

                // One of the three types of interfaces that might
                // send in a lot of GPS data constantly.  Save only
                // GPRMC and GPGGA strings into global variables.
                // Drop other GPS strings on the floor.
                //
                if ( (length > 7) && (strncmp(string,"$GPRMC,",7) == 0) ) {
                    xastir_snprintf(gprmc_save_string,MAX_LINE_SIZE,"%s",string);
                    gps_port_save = port;
                }
                else if ( (length > 7) && (strncmp(string,"$GPGGA,",7) == 0) ) {
                    xastir_snprintf(gpgga_save_string,MAX_LINE_SIZE,"%s",string);
                    gps_port_save = port;
                }
                else {
                    // It's not one of the GPS strings we're looking
                    // for.  It could be another GPS string, a
                    // partial GPS string, or a full/partial TNC
                    // string.  Drop the string on the floor unless
                    // it's an HSP interface.
                    //
                    if (port_data[port].device_type == DEVICE_SERIAL_TNC_HSP_GPS) {
                        // Decode the string normally.
                        incoming_data = string;
                        incoming_data_length = length;
                        data_port = port;
                        data_avail = 1;
                        //fprintf(stderr,"data_avail\n");
                    }
                }
                break;
// We need to make sure that the variables stating that a string is
// available are reset in any case.  Look at how/where data_avail is
// reset.  We may not care if we just wait for data_avail to be
// cleared before writing to the string again.

            default:    // Not one of the above three types, decode
                        // the string normally.
                incoming_data = string;
                incoming_data_length = length;
                data_port = port;
                data_avail = 1;
                //fprintf(stderr,"data_avail\n");
                break;
        }


        if (end_critical_section(&data_lock, "interface.c:channel_data(3)" ) > 0)
            fprintf(stderr,"data_lock, Port = %d\n", port);

        // Remove the cleanup routine for the case where this thread
        // gets killed while the mutex is locked.  The cleanup
        // routine initiates an unlock before the thread dies.  We
        // must be in deferred cancellation mode for the thread to
        // have this work properly.
        pthread_cleanup_pop(0);

 
        if (debug_level & 1)
            fprintf(stderr,"Channel data on Port %d [%s]\n",port,(char *)string);

        /* wait until data is processed */
        while (data_avail && max < 5400) {
            sched_yield();  // Yield to other threads
            tmv.tv_sec = 0;
            tmv.tv_usec = 2;  // 2 usec
            (void)select(0,NULL,NULL,NULL,&tmv);
            max++;
        }
    }


    if (end_critical_section(&output_data_lock, "interface.c:channel_data(4)" ) > 0)
        fprintf(stderr,"output_data_lock, Port = %d\n", port);

    // Remove the cleanup routine for the case where this thread
    // gets killed while the mutex is locked.  The cleanup routine
    // initiates an unlock before the thread dies.  We must be in
    // deferred cancellation mode for the thread to have this work
    // properly.
    pthread_cleanup_pop(0);
}





//********************************* START AX.25 ********************************

#ifdef HAVE_LIBAX25
// stolen from libax25-0.0.9 and modified to set digipeated bit based on '*'
int my_ax25_aton_arglist(char *call[], struct full_sockaddr_ax25 *sax)
{
    char *bp;
    char *addrp;
    int n = 0;
    int argp = 0;
    int len = 0;
    int star = 0;

    addrp = sax->fsa_ax25.sax25_call.ax25_call;

    do {
        /* Fetch one callsign token */
        if ((bp = call[argp++]) == NULL)
            break;

        /* Check for the optional 'via' syntax */
        if (n == 1 && (strcasecmp(bp, "V") == 0 || strcasecmp(bp, "VIA") == 0))
            continue;

        /* Process the token (Removes the star before the ax25_aton_entry call
           because it would call it a bad callsign.) */
        len = strlen(bp);
        if (len > 1 && bp[len-1] == '*') {
            star = 1;
            bp[len-1] = '\0';
        }
        else {
            star = 0;
        }
        if (ax25_aton_entry(bp, addrp) == -1) {
            popup_message("Bad callsign!", bp);
            return -1;
        }
        if (n >= 1 && star) {
            addrp[6] |= 0x80; // set digipeated bit if we had found a star
        }

        n++;

        if (n == 1)
            addrp  = sax->fsa_digipeater[0].ax25_call;      /* First digipeater address */
        else
            addrp += sizeof(ax25_address);

    } while (n < AX25_MAX_DIGIS && call[argp] != NULL);

    /* Tidy up */
    sax->fsa_ax25.sax25_ndigis = n - 1;
    sax->fsa_ax25.sax25_family = AF_AX25;

    return sizeof(struct full_sockaddr_ax25);
}
#endif  // HAVE_LIBAX25





//***********************************************************
// ui connect: change call and proto paths and reconnect net
// port device to work with
//***********************************************************
int ui_connect( int port, char *to[]) {
    int    s = -1;
#ifdef HAVE_LIBAX25
    int    sockopt;
    int    addrlen = sizeof(struct full_sockaddr_ax25);
    struct full_sockaddr_ax25 axbind, axconnect;
    /* char  *arg[2]; */
    char  *portcall;
    char temp[200];

    if (to == NULL)
        return(-1);

    if (to[0] == '\0')
        return(-1);

    /*
     * Handle incoming data
     *
     * Parse the passed values for correctness.
     */

    axconnect.fsa_ax25.sax25_family = AF_AX25;
    axbind.fsa_ax25.sax25_family    = AF_AX25;
    axbind.fsa_ax25.sax25_ndigis    = 1;

    if ((portcall = ax25_config_get_addr(port_data[port].device_name)) == NULL) {
        xastir_snprintf(temp, sizeof(temp), langcode("POPEM00005"),
                port_data[port].device_name);
        popup_message(langcode("POPEM00004"),temp);
        return -1;
    }
    if (ax25_aton_entry(portcall, axbind.fsa_digipeater[0].ax25_call) == -1) {
        xastir_snprintf(temp, sizeof(temp), langcode("POPEM00006"),
                port_data[port].device_name);
        popup_message(langcode("POPEM00004"), temp);
        return -1;
    }

    if (ax25_aton_entry(port_data[port].ui_call, axbind.fsa_ax25.sax25_call.ax25_call) == -1) {
        xastir_snprintf(temp, sizeof(temp), langcode("POPEM00007"), port_data[port].ui_call);
        popup_message(langcode("POPEM00004"),temp);
        return -1;
    }

    if (my_ax25_aton_arglist(to, &axconnect) == -1) {
        popup_message(langcode("POPEM00004"),langcode("POPEM00008"));
        return -1;
    }

    /*
     * Open the socket into the kernel.
     */

    if ((s = socket(AF_AX25, SOCK_DGRAM, 0)) < 0) {
        xastir_snprintf(temp, sizeof(temp), langcode("POPEM00009"), strerror(errno));
        popup_message(langcode("POPEM00004"),temp);
        return -1;
    }

    /*
     * Set our AX.25 callsign and AX.25 port callsign accordingly.
     */
    ENABLE_SETUID_PRIVILEGE;
    if (bind(s, (struct sockaddr *)&axbind, addrlen) != 0) {
        DISABLE_SETUID_PRIVILEGE;
        xastir_snprintf(temp, sizeof(temp), langcode("POPEM00010"), strerror(errno));
        popup_message(langcode("POPEM00004"),temp);
        return -1;
    }
    DISABLE_SETUID_PRIVILEGE;

    if (devices[port].relay_digipeat)
        sockopt = 1;
    else
        sockopt = 0;

    if (setsockopt(s, SOL_AX25, AX25_IAMDIGI, &sockopt, sizeof(int))) {
        fprintf(stderr,"AX25 IAMDIGI setsockopt FAILED");
        return -1;
    }

    if (debug_level & 2)
       fprintf(stderr,"*** Connecting to UNPROTO port for transmission...\n");

    /*
     * Lets try and connect to the far end.
     */

    if (connect(s, (struct sockaddr *)&axconnect, addrlen) != 0) {
        xastir_snprintf(temp, sizeof(temp), langcode("POPEM00011"), strerror(errno));
        popup_message(langcode("POPEM00004"),temp);
        return -1;
    }

    /*
     * We got there.
     */
#endif /* HAVE_LIBAX25 */
    return s;
}





//************************************************************
// data_out_ax25()
//
// Send string data out ax25 port
//************************************************************

static void data_out_ax25(int port, unsigned char *string) {
    static char ui_mycall[10];
    char        *temp;
    char        *to[10];
    int         quantity;

    if (string == NULL)
        return;

    if (string[0] == '\0')
        return;

    if (begin_critical_section(&port_data_lock, "interface.c:data_out_ax25(1)" ) > 0)
        fprintf(stderr,"port_data_lock, Port = %d\n", port);

    // Check for commands (start with Control-C)
    if (string[0] == (unsigned char)3) { // Yes, process TNC type commands

        // Look for MYCALL command
        if (strncmp((char *)&string[1],"MYCALL", 6) == 0) {

            // Found MYCALL.  Snag the callsign and put it into the
            // structure for the port

            // Look for whitespace/CR/LF (end of "MYCALL")
            temp = strtok((char *)&string[1]," \t\r\n");
            if (temp != NULL) {

                // Look for whitespace/CR/LF (after callsign)
                temp = strtok(NULL," \t\r\n");
                if (temp != NULL) {
                    substr(ui_mycall, temp, 9);
                    strcpy(port_data[port].ui_call, ui_mycall);
                    if (debug_level & 2)
                        fprintf(stderr,"*** MYCALL %s\n",port_data[port].ui_call);
                }
            }
        }

        // Look for UNPROTO command
        else if (strncmp((char *)&string[1],"UNPROTO", 6) == 0) {
            quantity = 0;   // Number of callsigns found

            // Look for whitespace/CR/LF (end of "UNPROTO")
            temp = strtok((char *)&string[1]," \t\r\n");
            if (temp != NULL) { // Found end of "UNPROTO"

                // Find first callsign (destination call)
                temp = strtok(NULL," \t\r\n");
                if (temp != NULL) {
                    to[quantity++] = temp; // Store it
                    //fprintf(stderr,"Destination call: %s\n",temp);

                    // Look for "via" or "v"
                    temp = strtok(NULL," \t\r\n");
                    //fprintf(stderr,"Via: %s\n",temp);

                    while (temp != NULL) {  // Found it
                        // Look for the rest of the callsigns (up to
                        // eight of them)
                        temp = strtok(NULL," ,\t\r\n");
                        if (temp != NULL) {
                            //fprintf(stderr,"Call: %s\n",temp);
                            if (quantity < 9) {
                                to[quantity++] = temp;
                            }
                        }
                    }
                    to[quantity] = NULL;

                    if (debug_level & 2) {
                        int i = 1;

                        fprintf(stderr,"UNPROTO %s VIA ",*to);
                        while (to[i] != NULL)
                            fprintf(stderr,"%s,",to[i++]);
                        fprintf(stderr,"\n");
                    }

                    if (port_data[port].channel2 != -1) {
                        if (debug_level & 2)
                            fprintf(stderr,"Write DEVICE is UP!  Taking it down to reconfigure UI path.\n");

                        (void)close(port_data[port].channel2);
                        port_data[port].channel2 = -1;
                    }

                    if ((port_data[port].channel2 = ui_connect(port,to)) < 0) {
                        popup_message(langcode("POPEM00004"),langcode("POPEM00012"));
                        port_data[port].errors++;
                    }
                    else {    // Port re-opened and re-configured
                        if (debug_level & 2)
                            fprintf(stderr,"WRITE port re-opened after UI path change\n");
                    }
                }
            }
        }
    }

    // Else not a command, write the data directly out to the port
    else {
        if (debug_level & 2)
            fprintf(stderr,"*** DATA: %s\n",(char *)string);

        if (port_data[port].channel2 != -1)
            (void)write(port_data[port].channel2, string, strlen((char *)string));
        else if (debug_level & 2)
            fprintf(stderr,"\nPort down for writing!\n\n");
    }

    if (end_critical_section(&port_data_lock, "interface.c:data_out_ax25(2)" ) > 0)
        fprintf(stderr,"port_data_lock, Port = %d\n", port);
}





// fetch16bits
//
// Modifies: Nothing.
//
int fetch16bits(unsigned char *str) {
    int i;

    
    i = *str++;
    i = i << 8;
    i = i | *str++;
    return(i);
}





// fetch32bits
//
// Modifies: Nothing.
//
int fetch32bits(unsigned char *str) {
    int i;

 
    i = *str++;
    i = i << 8;
    i = i | *str++;
    i = i << 8;
    i = i | *str++;
    i = i << 8;
    i = i | *str;
    return(i);
}





// 0x00 Sequence number - 16 bit integer
//
// Modifies: sequence
//
int OpenTrac_decode_sequence(unsigned char *element,
                             int           element_len,
                             unsigned int  *sequence) {

    if (element_len != 2 && element_len != 0)
        return -1;

    // No sequence number, increment by one
    if (element_len == 0) {
        *sequence = *sequence + 1;
    }
    else {
        *sequence = fetch16bits(element);
    }

    fprintf(stderr,"Sequence: %d\n",*sequence);

    return 0;
}





// 0x01 Originating Station - Callsign, SSID, Sequence, and Network
//
// Modifies: sequence
//           callsign
//           ssid
//           network
//
int OpenTrac_decode_origination(unsigned char *element,
                                int           element_len,
                                unsigned int  *sequence,
                                char          *callsign,
                                unsigned char *ssid,
                                unsigned char *network) {
    unsigned char c;


    if (element_len < 6)
        return -1;

    if (element_len > 8)
        return -1;

    // Binary routine.  strncpy is ok here as long as nulls not in
    // data.  We null-terminate it ourselves to make sure it is
    // terminated.
    strncpy(callsign, element, 6);
    callsign[6]=0;
    for (c=*ssid=0;c<6;c++) {
        *ssid |= (callsign[c] & 0x80) >> (c+2);
        callsign[c] &= 0x7f;
    }

    *sequence = fetch16bits(element+6);
    if (element_len == 9) {
        *network = *(element+8);

        fprintf(stderr, "Origin: %s-%d seq %d net %d\n",
            callsign,
            *ssid,
            *sequence,
            *network);
    }
    else {
        fprintf(stderr, "Origin: %s-%d seq %d direct\n",
            callsign,
            *ssid,
            *sequence);
    }

    return 0;
}





// Strip the SSID from the callsign and return it
// 
// Modifies: call (Strips top bit from each char)
//
int OpenTrac_extract_ssid(unsigned char *call) {
    int c, ssid;


    for (c=ssid=0;c<6;c++) {
        ssid |= (call[c] & 0x80) >> (c+2);
        call[c] &= 0x7f;
    }

    return ssid;
}





// 0x02 Entity ID
//
// Modifies: entity_call
//           entity_ssid
//           entity_serial
//           entity_sequence
//
int OpenTrac_decode_entityid(unsigned char *element,
                             int           element_len,
                             unsigned char *origin_call,
                             unsigned char origin_ssid,
                             unsigned char *entity_call,
                             unsigned char *entity_ssid,
                             unsigned int  *entity_serial,
                             unsigned int  *entity_sequence) {

    if (element_len > 10) {
        return -1;
    }
    else if (element_len > 5) {
        memcpy(entity_call, element, 6);
        entity_call[6]=0;
        *entity_ssid = OpenTrac_extract_ssid(entity_call);
    }
    else {  // Not enough, so use origin_call instead
        strcpy(entity_call, origin_call);
        *entity_ssid = origin_ssid;
    }

    switch (element_len) {
        case 0:
            *entity_serial = *entity_serial + 1;
            *entity_sequence = 0;
            break;
        case 2:
            *entity_serial = fetch16bits(element);
            *entity_sequence = 0;
            break;
        case 4:
            *entity_serial = fetch16bits(element);
            *entity_sequence = fetch16bits(element+2);
            break;
        case 6:
            *entity_serial = 0;
            break;
        case 8:
            *entity_serial = fetch16bits(element+6);
            *entity_sequence = 0;
            break;
        case 10:
            *entity_serial = fetch16bits(element+6);
            *entity_sequence = fetch16bits(element+8);
            break;
        default:
            *entity_sequence = 0;
            *entity_serial = *entity_serial + 1;
            break;
    }

    fprintf(stderr, "Entity %s-%d:%04x #%d\n",
        entity_call,
        *entity_ssid,
        *entity_serial,
        *entity_sequence);

    return 0;
}





// 0x10 Position Report - Lat/Lon/<Alt>
// Lat/Lon is WGS84, 180/2^31 degrees,  Alt is 1/100 meter
//
// Modifies: lat
//           lon
//           alt
//
int OpenTrac_decode_position(unsigned char *element,
                             int           element_len,
                             double        *lat,
                             double        *lon,
                             float         *alt) {

    const double semicircles = 11930464.71111111111;


    if (element_len < 8)
        return -1; // Too short!

    if (element_len > 11)
        return -1; // Too long!

    *lat = fetch32bits(element) / semicircles;
    *lon = fetch32bits(element+4) / semicircles;

    if (element_len == 11) {
        *alt = ( (*(element+8))<<16 ) + fetch16bits(element+9);
        *alt = (*alt / 100) - 10000;
    }

    fprintf(stderr, "Position: Lat %f Lon %f Alt %f\n",*lat,*lon,*alt);

    return 0;
}





// 0x11 Timestamp - Unix format time (unsigned)
//
// Modifies: rawtime
// 
int OpenTrac_decode_timestamp(unsigned char *element,
                              int           element_len,
                              long          *rawtime) {

    *rawtime = 0;

    if (element_len != 4)
        return -1;

    *rawtime = fetch32bits(element);

    fprintf(stderr, "Time: %s", ctime(rawtime));

    return 0;
}





// 0x12 Freeform Comment - ASCII text
//
// Modifies: comment
// 
int OpenTrac_decode_comment(unsigned char *element,
                            int           element_len,
                            char          *comment) {

    if (element_len > 126)
        return -1;  // shouldn't be possible

    strcat(comment," ");
    strncat(comment, element, element_len);
    comment[element_len + 1] = 0;   // Account for the space char

    fprintf(stderr, "Text: %s\n", comment);

    return 0;
}





// 0x13 Course and Speed - Course in degrees, speed in 1/50 m/s
//
// Modifies: course  (degrees true)
//           speed   (kph)
//
int OpenTrac_decode_courseandspeed(unsigned char *element,
                                   int           element_len,
                                   int           *course,
                                   float         *speed) {
    int rawspeed;


    if (element_len != 3)
        return -1;

    *course = ( (*element) << 1 ) + ( (*(element+1) & 0x80) >> 7);
    rawspeed = fetch16bits(element+1) & 0x7fff;
    *speed = (float)(rawspeed * 0.072); // kph

    fprintf(stderr, "Course: %d Speed: %f kph\n", *course, *speed);

    return 0;
}





// 0x14 Positional Ambiguity - 16 bits, in meters
//
// Modifies: ambiguity
// 
int OpenTrac_decode_ambiguity(unsigned char *element,
                              int           element_len,
                              int           *ambiguity) {

    if (element_len != 2)
        return -1;

    *ambiguity = fetch16bits(element);

    fprintf(stderr, "Position +/- %d meters\n", *ambiguity);

    return 0;
}





// 0x15 Country Code - ISO 3166-1 and optionally -2
//
// Modifies: country
//           subdivision
// 
int OpenTrac_decode_country(unsigned char *element,
                            int           element_len,
                            char          *country,
                            char          *subdivision) {

    if (element_len < 2)
        return -1;

    if (element_len > 5)
        return -1;

    // Binary routine.  strncpy is ok here as long as nulls not in
    // data.  We null-terminate it ourselves to make sure it is
    // terminated.
    strncpy(country, element, 2);
    country[2] = 0;
    if (element_len > 2) {
        // Binary routine.  strncpy is ok here as long as nulls not
        // in data.  We null-terminate it ourselves to make sure it
        // is terminated.
        strncpy(subdivision, element+2, element_len-2);
        subdivision[element_len-2] = 0;
        fprintf(stderr, "Country Code %s-%s\n", country, subdivision);
    }
    else {
        fprintf(stderr, "Country Code %s\n", country);
    }
    return 0;
}





// 0x16 - Display Name (UTF-8 text)
//
// Modifies: displayname
// 
int OpenTrac_decode_displayname(unsigned char *element,
                                int           element_len,
                                char          *displayname) {

    if (element_len > 30 || !element_len)
        return -1;

    // Binary routine.  strncpy is ok here as long as nulls not in
    // data.  We null-terminate it ourselves to make sure it is
    // terminated.
    strncpy(displayname, element, element_len);
    displayname[element_len] = 0;

    fprintf(stderr, "Display Name: %s\n", displayname);
    return 0;
}





// 0x17 - Waypoint Name (up to 6 chars, uppercase)
//
// Modifies: waypoint
// 
int OpenTrac_decode_waypoint(unsigned char *element,
                             int           element_len,
                             char          *waypoint) {

    if (element_len > 6 || !element_len)
        return -1;

    // Binary routine.  strncpy is ok here as long as nulls not in
    // data.  We null-terminate it ourselves to make sure it is
    // terminated.
    strncpy(waypoint, element, element_len);
    waypoint[element_len] = 0;

    fprintf(stderr, "Waypoint Name: %s\n", waypoint);

    return 0;
}





// Mapping between OpenTrac symbols and APRS symbols
char symbol_translate[100][14] = {
    "3 11100000 /S",  // space shuttle
    "3 12100000 \\S", // satellite
    "1 10000000 \\S", // *** other space
    "2 21000000 /O",  // balloon
    "4 22120000 /g",  // glider
    "3 22100000 /'",  // small plane
    "3 22200000 /^",  // large aircraft
    "3 22300000 /^",  // large aircraft
    "2 23000000 /X",  // helicopter
    "1 20000000 /'",  // *** other air
    "4 31310000 /h",  // hospital
    "4 31340000 /A",  // aid station
    "4 31350000 \\X", // pharmacy
    "4 31410000 /d",  // fire station
    "4 31440000 /o",  // EOC
    "4 31460000 /+",  // red cross
    "4 31620000 \\h", // ham store
    "3 31700000 /-",  // house
    "3 31800000 \\+", // church
    "4 31910000 /H",  // hotel
    "4 31920000 \\9", // gas station
    "4 31930000 \\R", // restaurant
    "4 31940000 \\?", // information
    "4 31950000 \\P", // parking
    "4 31960000 /t",  // truck stop
    "4 31970000 \\r", // restrooms
    "4 31980000 \\$", // bank/atm
    "3 31a00000 /K",  // school
    "4 31c10000 /,",  // boy scouts
    "4 31c20000 \\,", // girl scouts
    "4 31d10000 \\;", // park/picnic area
    "4 31d20000 \\;", // park/picnic area
    "4 31d30000 /;",  // campground
    "5 31e13000 \\V", // VORTAC
    "5 31f12000 /$",  // phone
    "5 32111000 />",  // car
    "5 32112000 /v",  // van
    "5 32113000 /k",  // truck
    "5 32114000 /j",  // jeep
    "5 32115000 /R",  // rv
    "5 32116000 /<",  // motorcycle
    "5 32117000 /b",  // bicycle
    "5 32121000 /u",  // 18-wheeler
    "5 32122000 /U",  // bus
    "5 32131000 /=",  // railroad engine
    "5 32141000 /*",  // snowmobile
    "5 32161000 /!",  // police
    "5 32162000 /a",  // ambulance
    "5 32163000 /f",  // fire truck
    "3 33100000 /(",  // sat station
    "1 30000000 //",  // *** other ground
    "3 41200000 /s",  // ship
    "4 41510000 /C",  // canoe
    "4 41520000 /C",  // canoe (kayak)
    "4 41540000 /Y",  // yacht
    "4 41620000 \\C", // coastguard
    "3 43100000 \\N", // nav buoy
    "1 40000000 \\s", // *** other sea
    "1 50000000 /.",  // ***
    "4 62110000 /[",  // jogger
    "3 64200000 /:",  // fire
    "3 65500000 \\'", // crash site
    "3 66100000 \\Q", // earthquake
    "3 66300000 \\w", // flooding
    "1 60000000 /.",  // *** other activities
    "4 71610000 \\U", // sunny
    "4 71620000 \\U", // sunny
    "4 71630000 \\(", // cloudy
    "4 71640000 \\(", // cloudy
    "4 71650000 \\(", // cloudy
    "4 71660000 \\(", // cloudy
    "5 71711000 \\I", // rain shower
    "5 71712000 \\F", // freezing rain
    "5 71713000 \\D", // drizzle
    "4 71710000 \\'", // rain
    "5 71721000 \\G", // snow shower
    "4 71720000 \\*", // snow
    "4 71730000 \\:", // hail
    "5 71812000 \\f", // funnel cloud
    "5 71813000 \\J", // lightning
    "4 71810000 \\T", // thunderstorm
    "5 71822000 /@",  // hurricane
    "4 71820000 \\@", // storm
    "4 71920000 \\{", // fog
    "4 71950000 \\E", // smoke
    "4 71960000 \\H", // haze
    "4 71970000 \\b", // blowing dust/sand
    "1 70000000 \\o", // *** other weather
    "1 80000000 /.",  // ***
    "1 90000000 /.",  // ***
    "1 A0000000 /.",  // ***
    "1 B0000000 /.",  // ***
    "1 C0000000 /.",  // ***
    "1 D0000000 /.",  // ***
    "1 E0000000 /.",  // ***
    "1 F0000000 /.",  // ***
    "0"};





// 0x18 Map Symbol - Packed 4-bit integers
//
// Modifies: symbol (leaves it in 4-bit packed format)
//           aprs_symbol_table
//           aprs_symbol_char
// 
int OpenTrac_decode_symbol(unsigned char *element,
                           int           element_len,
                           char          *symbol,
                           char          *aprs_symbol_table,
                           char          *aprs_symbol_char) {
    int c, ii, done;
    unsigned char split[9];


    ii = 0;
    symbol[0] = '\0';

    if (!element_len)
        return -1;

    if (element_len > 4)
        return -1;

    fprintf(stderr, "Symbol: ");

    for (c = 0; c < element_len; c++) {

        symbol[c] = element[c];

        // Split nibbles into two bytes
        split[ii++] = element[c] >> 4;
        split[ii++] = element[c] & 0x0f;

        if (c > 0) {
            fprintf(stderr, ".");
        }
        fprintf(stderr, "%d", element[c] >> 4);

        if (element[c] & 0x0f) {
            fprintf(stderr, ".%d", element[c] & 0x0f);
        }
    }

    symbol[element_len] = '\0'; // Terminate string
    split[ii] = '\0';   // Terminate split integers

    // Convert split string chars into hex chars
    for (c = 0; c < element_len * 2; c++) {
        if (split[c] < 10) {
            split[c] += 0x30;
        }
        else {
            split[c] += 0x37;
        }
    }

//fprintf(stderr,"\n%s\n",split);

    // Find the symbol from the table above that matches.  Use the
    // split[] string.

    // Defaults:
    *aprs_symbol_table = '/';
    *aprs_symbol_char  = '/';   // A dot

    ii = 0;
    done = 0;

    while (!done && symbol_translate[ii][0] != '0') {
        int len;

        // Find out how many chars to compare
        len = symbol_translate[ii][0] - 0x30;

//fprintf(stderr,"%d:%s\n",len,&symbol_translate[ii][2]);

        // Do a strncasecmp() for "len" chars in the hex part of the
        // string.  Once we find a match, the last two chars in the
        // string are our symbol table and symbol.  If we don't find
        // a match, we use the default "//" symbol (a dot) instead.
        if ( strncasecmp(&symbol_translate[ii][2],split,len) == 0 ) {
            // Found a match
//fprintf(stderr,"Found a match: %s in %d", split, ii);
            len = strlen(&symbol_translate[ii][0]);
            *aprs_symbol_table = symbol_translate[ii][len-2];
            *aprs_symbol_char  = symbol_translate[ii][len-1];
            done++;
        }
        else {
            // No match
            ii++;
        }
    }

    switch (split[0]) {
        case '1':
            fprintf(stderr, " (Space Symbol)");
            break;
        case '2':
            fprintf(stderr, " (Air Symbol)");
            break;
        case '3':
            fprintf(stderr, " (Ground Symbol)");
            break;
        case '4':
            fprintf(stderr, " (Sea Symbol)");
            break;
        case '5':
            fprintf(stderr, " (Subsurface Symbol)");
            break;
        case '6':
            fprintf(stderr, " (Activities Symbol)");
            break;
        case '7':
            fprintf(stderr, " (Weather Symbol)");
            break;
        default:
            fprintf(stderr, " (Unknown Symbol)");
            break;
    }
    fprintf(stderr, "\n");

    return 0;
}





// 0x20 Path Trace - Call/SSID, Network
//
// Modifies:
// 
int OpenTrac_decode_pathtrace(unsigned char *element,
                              int           element_len) {

//WE7U:  Need to pass back and use callsign/ssid/network.

    char callsign[7];
    int ssid, c, network;


    if (element_len % 7)
        return -1; // Must be multiple of 7 octets

    if (!element_len) {
        fprintf(stderr, "Empty Path\n");
        return 0;
    }

    fprintf(stderr, "Path: ");
    for (c=0; c<element_len; c+=7) {
        memcpy(callsign, element+c, 6);
        ssid = OpenTrac_extract_ssid(callsign);
        network = (int)*(element+c+6);
        fprintf(stderr, " %s-%d (%d)", callsign, ssid, network);
    }
    fprintf(stderr, "\n");

    return 0;
}





// 0x21 Heard-By List
//
// Modifies:
// 
int OpenTrac_decode_heardby(unsigned char *element,
                            int           element_len) {

//WE7U:  Need to pass back "Heard By" string.

    int c;


    if (!element_len)
        return -1;
 
    fprintf(stderr, "Heard By:");
    for (c=0; c<element_len; c++) {
        fprintf(stderr, " %d", (int)*(element+c));
    }
    fprintf(stderr, "\n");

    return 0;
}





// 0x22 Available Networks
//
// Modifies:
// 
int OpenTrac_decode_availablenets(unsigned char *element,
                                  int           element_len) {

//WE7U:  Need to pass back "Available Networks" string.

    int c;


    if (!element_len)
        return -1;

    fprintf(stderr, "Available Networks:");
    for (c=0; c<element_len; c++) {
        fprintf(stderr, " %d", (int)*(element+c));
    }
    fprintf(stderr, "\n");

    return 0;
}





// 0x32 - Maidenhead Locator (4 or 6 chars)
//
// Modifies: maidenhead
// 
int OpenTrac_decode_maidenhead(unsigned char *element,
                               int           element_len,
                               char          *maidenhead) {

    if (element_len > 6)
        return -1;

    if (element_len < 4)
        return -1;

    // Binary routine.  strncpy is ok here as long as nulls not in
    // data.  We null-terminate it ourselves to make sure it is
    // terminated.
    strncpy(maidenhead, element, element_len);
    maidenhead[element_len] = 0;

    fprintf(stderr, "Grid ID: %s\n", maidenhead);
    return 0;
}





// 0x34 GPS Data Quality - Fix, Validity, Sats, PDOP, HDOP, VDOP
//
// Modifies:
// 
int OpenTrac_decode_gpsquality(unsigned char *element,
                               int           element_len) {

//WE7U:  Need to pass back gps quality parameters/strings.

    int fixtype, validity, sats;
    const char *fixstr[] = {"Unknown Fix", "No Fix", "2D Fix", "3D Fix"};
    const char *validstr[] = {"Invalid", "Valid SPS", "Valid DGPS", "Valid PPS"};


    if (element_len > 4 || !element_len)
        return -1;

    fixtype = (element[0] & 0xc0) >> 6;
    validity = (element[0] & 0x30) >> 4;
    sats = (element[0] & 0x0f);
    fprintf(stderr, "GPS: %s %s, %d sats", fixstr[fixtype],
        validstr[validity], sats);
    if (element_len > 1)
        fprintf(stderr, " PDOP=%.1f", (float)element[1]/10);
    if (element_len > 2)
        fprintf(stderr, " HDOP=%.1f", (float)element[2]/10);
    if (element_len > 3)
        fprintf(stderr, " VDOP=%.1f", (float)element[3]/10);
    fprintf(stderr, "\n");

    return 0;
}





// 0x35 Aircraft Registration - ASCII text
//
// Modifies: aircraft_id
// 
int OpenTrac_decode_acreg(unsigned char *element,
                          int           element_len,
                          char          *aircraft_id) {

    if (element_len > 8)
        return -1;

    // Binary routine.  strncpy is ok here as long as nulls not in
    // data.  We null-terminate it ourselves to make sure it is
    // terminated.
    strncpy(aircraft_id, element, element_len);
    aircraft_id[element_len]=0;

    fprintf(stderr, "Aircraft ID: %s\n", aircraft_id);

    return 0;
}





// 0x42 River Flow Gauge - 1/64 m^3/sec, centimeters
//
// Modifies: Nothing.
// 
int OpenTrac_decode_rivergauge(unsigned char *element,
                               int           element_len) {

//WE7U:  Need to pass back gauge indications.

    unsigned int flow;
    unsigned int height;
    float flowm;
    float heightm;


    if (element_len != 4)
        return -1;

    flow = fetch16bits(element);
    height = fetch16bits(element+2);
    flowm = (float)flow / 64;
    heightm = (float)height / 100;
    fprintf(stderr, "River flow rate: %f Cu M/Sec  Height: %f M\n",
        flowm, heightm);

    return 0;
}





// 0x0100 - Emergency / Distress Call
//
// Modifies: nothing
//
int OpenTrac_flag_emergency(void) {

    fprintf(stderr, "* * * EMERGENCY * * *\n");

    return 0;
}





// 0x0101 - Attention / Ident
//
// Modifies: nothing
//
int OpenTrac_flag_attention(void) {

    fprintf(stderr, " - ATTENTION - \n");

    return 0;
}





// 0x0300 - HAZMAT (UN ID in lower 14 bits)
//
// Modifies: hazmat_id
// 
int OpenTrac_decode_hazmat(unsigned char *element,
                           int           element_len,
                           int           *hazmat_id,
                           char          *comment) {

    if (element_len < 2) {
        fprintf(stderr, "HAZMAT: Unknown Material\n");
        strcat(comment," HAZMAT: Unknown Material");
    }
    else if (element_len > 2) {
        fprintf(stderr, "HAZMAT: Unknown Material: ID too Long\n");
        strcat(comment," HAZMAT: Unknown Material: ID too Long");
    }
    else {
        char temp[200];

        *hazmat_id = fetch16bits(element) & 0x3fff;
        fprintf(stderr, "HAZMAT: UN%04d\n", *hazmat_id);
        xastir_snprintf(temp,
            sizeof(temp),
            " HAZMAT: UN%04d",
            *hazmat_id);
        strcat(comment,temp);
    }
    return 0;
}





// 0x0500 to 0x05ff Generic Measurement Elements
// Values may be 8-bit int, 16-bit int, single float, or double
// float
//
// Modifies: Nothing.
// 
int OpenTrac_decode_units(int           unitnum,
                          unsigned char *element,
                          int           element_len,
                          char          *comment) {

//WE7U:  Need to pass back and use units.

    const char *units[]={"Volts","Amperes","Watts","Kelvins",
        "Meters","Seconds","Meters/Second","Liters","Kilograms",
        "Bits/Second","Bytes","Radians","Radians/Second",
        "Square Meters","Joules","Newtons","Pascals","Hertz",
        "Meters/Sec^2","Grays","Lumens","Cubic Meters/Second",
        "Pascal Seconds","Kilograms/Meter^3","Radians/Second^2",
        "Coulombs","Farads","Siemens"};

    union measurement {
        char c;
        float f;
        double d;
    } *mval;

    int ival; // too much variation in byte order and size for union
    char temp[200];


    mval = (void *)element;
    switch (element_len) {
        case 1:
            xastir_snprintf(temp,
                sizeof(temp),
                " %d %s",
                mval->c,
                units[unitnum]);
            strcat(comment,temp);
            fprintf(stderr, "%s\n",temp);
            break;
        case 2:
            ival = fetch16bits(element);
            xastir_snprintf(temp,
                sizeof(temp),
                " %d %s",
                ival,
                units[unitnum]);
            strcat(comment,temp);
            fprintf(stderr, "%s\n", temp);
            break;
        case 4:
            xastir_snprintf(temp,
                sizeof(temp),
                " %f %s",
                mval->f,
                units[unitnum]);
            strcat(comment,temp);
            fprintf(stderr, "%s\n", temp);
            break;
        case 8:
            xastir_snprintf(temp,
                sizeof(temp),
                " %f %s",
                mval->d,
                units[unitnum]);
            strcat(comment,temp);
            fprintf(stderr, "%s\n", temp);
            break;
        default:
            return -1;
    }

    return 0;
}





//WE7U: Protect "buffer" from getting overrun!!!
//
//***********************************************************
// process_OpenTrac_packet()
//
// data     raw packet data: Points to first char of info field
// len      length of info field of AX.25 packet
// buffer   buffer to write readable packet data to
// source   source callsign
// ssid     ssid of source callsign
// dest     destination callsign
// digis    number of digi's that we're being passed
// digi[][] digi list (up to 10 digis, 10 chars each)
// digi_h   high bit of the digis (up to 10 chars)
//
// Function used to process an OpenTrac packet that is not in a
// KISS frame.  In other words the underlying transport protocol
// portions have been stripped off by this point, or perhaps the
// packet was received from an OpenTrac internet feed that didn't
// have anything to strip off.
//
// This is where the majority of the OpenTrac-specific decoding gets
// done.  Much of the code for this routine and the routines above
// that deal with OpenTrac packets was generously donated by Scott
// Miller, N1VG.  He's allowing us to put it under the GPL license.
//
// A new sequence or a new entityID should trigger an APRS packet to
// be generated (with the old sequence/entityID).  A packet should
// also be generated at the end of processing.  There can be
// multiple ID's and multiple locations embedded inside an OpenTrac
// packet.
//
//***********************************************************

char *process_OpenTrac_packet( unsigned char *data,
                               unsigned int  len,
                               char          *buffer,
                               unsigned char *source_call,
                               unsigned int  source_ssid,
                               unsigned char *dest,
                               unsigned int  digis,
                               unsigned char digi[10][10],
                               unsigned char *digi_h) {
    int           i;
    int           elen;
    int           etype;
    unsigned int  decoded         = 0;
    unsigned char origin_call[7];       // Where the packet originated
    unsigned char origin_ssid     = 0;
    unsigned char entity_call[7];       // What the packet is talking about
    unsigned char entity_ssid     = 0x00;
    unsigned int  entity_serial   = 0;
    unsigned int  entity_sequence = 0;
    unsigned int  temp_entity_sequence = 0;
    unsigned char network         = 0;
    long          rawtime         = 0;
    int           have_position   = 0;
    double        latitude        = 0.0;
    double        longitude       = 0.0;
    float         altitude        = 0;
    int           course          = 0;
    float         speed           = 0;
    int           ambiguity       = 0;
    char          country[3];
    char          subdivision[4];
    char          aircraft_id[9];
    int           hazmat_id       = 0;
    char          displayname[31];
    char          maidenhead[7];
    char          waypoint[7];
    char          symbol[5];
    char          aprs_symbol_table = '/';
    char          aprs_symbol_char = '/';   // A "dot"
    char          comment[127];





    void process_opentrac_aprs(void) {
        // Construct a standard APRS-format packet out of the parsed
        // information.  We may need to construct several APRS packets
        // out of the OpenTrac packet, as there may be several entity
        // ID's or other types of info that can't fit into one APRS
        // packet.
        //
        // Note that if we got an Origination Station element, the path
        // here is not representative of the entire path the packet
        // took, and in fact we don't know who the transmitting station
        // was for this packet anymore, just the originating station for
        // the packet.
 
        strcat(buffer,(char *)origin_call);
        if (origin_ssid != 0x00) {
            char temp[10];

            strcat(buffer,"-");
            xastir_snprintf(temp,sizeof(temp),"%d",origin_ssid);
            strcat(buffer,temp);
        }
        strcat(buffer,">");
        strcat(buffer,(char *)dest);

        for(i = 0; i < (int)digis; i++) {
            strcat(buffer,",");
            strcat(buffer,(char *)digi[i]);
            /* at the last digi always put a '*' when h_bit is set */
            if (i == (int)(digis - 1)) {
                if (digi_h[i] == (unsigned char)0x80) {
                    /* this digi must have transmitted the packet */
                    strcat(buffer,"*");
                }
            } else {
                if (digi_h[i] == (unsigned char)0x80) {
                    /* only put a '*' when the next digi has no h_bit */
                    if (digi_h[i + 1] != (unsigned char)0x80) {
                        /* this digi must have transmitted the packet */
                        strcat(buffer,"*");
                    }
                }
            }
        }
        strcat(buffer,":");


        // If we parsed a position, finish creating an APRS packet.
        if (have_position) {
            // We have latitude/longitude/altitude
            int ok;
            unsigned long temp_lat, temp_lon;
            char lat_str[20];
            char lon_str[20];
            char alt_str[20];


            // Format it in DDMM.MMN/DDDMM.MMW format
            // lat/lon are doubles, alt is a float
            //

//fprintf(stderr, "Decoded this position: %f %f\n", latitude, longitude);
 
            // Convert lat/long to Xastir coordinate system first
            ok = convert_to_xastir_coordinates (
                &temp_lon,
                &temp_lat,
                (float)longitude,
                (float)latitude);

            if (ok) {
                // Convert to the proper format for an APRS position
                // packet:  "4903.50N/07201.75W/"
                char temp[20];


                convert_lat_l2s( temp_lat, lat_str, 20, CONVERT_LP_NOSP);
                convert_lon_l2s( temp_lon, lon_str, 20, CONVERT_LP_NOSP);
           
                if (entity_serial) {
                    // We have an entity that is non-zero.  Create an
                    // APRS "Item" from the data.  NOTE:  Items have to
                    // have at minimum 3, maximum 9 characters for the
                    // name.
                    char entity_name[10];

                    if (strlen(displayname)) {
                        xastir_snprintf(entity_name,
                            sizeof(entity_name),
                            "%s",
                            displayname);
                    }
                    else {
                        xastir_snprintf(entity_name,
                            sizeof(entity_name),
                            "Ent. %04x",    // Short for "Entity"
                            entity_serial);
                    }

                    strcat(buffer,")"); // APRS Item packet
                    strcat(buffer,entity_name);   // Entity name
                    strcat(buffer,"!");
                    strcat(buffer,lat_str);
                    temp[0] = aprs_symbol_table;
                    temp[1] = '\0';
                    strcat(buffer,temp);
                    strcat(buffer,lon_str);
                    temp[0] = aprs_symbol_char;
                    strcat(buffer,temp);

                    // Course/Speed
                    if ((int)speed != 0 || course != 0) {
                        xastir_snprintf(temp,
                            sizeof(temp),
                            "%03d/%03d",
                            course % 360,
                            (int)(speed / 1.852));  // Convert from kph to knots
                        strcat(buffer,temp);
                   }
                }
                else {
                    // Entity is zero.  Create an APRS position packet.
                    strcat(buffer,"!"); // APRS non-messaging position packet
                    strcat(buffer,lat_str);
                    temp[0] = aprs_symbol_table;
                    temp[1] = '\0';
                    strcat(buffer,temp);
                    strcat(buffer,lon_str);
                    temp[0] = aprs_symbol_char;
                    strcat(buffer,temp);

                    // Course/Speed
                    if ((int)speed != 0 || course != 0) {
                        xastir_snprintf(temp,
                            sizeof(temp),
                            "%03d/%03d",
                            course % 360,
                            (int)(speed / 1.852));  // Convert from kph to knots
                        strcat(buffer,temp);
                    }
                    if (strlen(displayname)) {
                        // Add displayname to the comment field
                        strcat(comment," ");
                        strcat(comment,displayname);
                    }
                }

                // Append the comment to the end.

// We really should check length here.  APRS packets can't handle
// much.  Item and position packets probably have different
// restriction on length of comment field as well.

                strcat(buffer,comment);

                // Altitude should be in feet "/A=001234", and placed in
                // the comment field of an APRS packet.
                xastir_snprintf(alt_str,
                    sizeof(alt_str),
                    " /A=%06d",
                    (int)(altitude * 3.28084)); // meters to feet
                strcat(buffer,alt_str);
            }
        }


        // Null-terminate the buffer string to make sure.
        buffer[MAX_DEVICE_BUFFER - 1] = '\0';

        fprintf(stderr, "%s\n", buffer);

        decode_ax25_line( buffer, DATA_VIA_TNC, 0, 1);

        // Clear the buffer for the next round.
        buffer[0] = '\0';
    }





    fprintf(stderr, "process_OpenTrac_packet()\n");

    // Initialize strings
    entity_call[0] = '\0';
    country[0]     = '\0';
    subdivision[0] = '\0';
    aircraft_id[0] = '\0';
    displayname[0] = '\0';
    maidenhead[0]  = '\0';
    waypoint[0]    = '\0';
    symbol[0]      = '\0';
    comment[0]     = '\0';

    // Fill origin with source-call/SSID initially.
    xastir_snprintf(origin_call,
        sizeof(origin_call),
        "%s",
        source_call);
    origin_ssid = source_ssid;


    // OpenTrac-specific decoding code.  For the moment we'll
    // construct an APRS-format packet from it and send it through
    // our regular decoding routines.  Later we'll dissect the
    // OpenTrac packets and store them into their own data
    // structures, but then we'll also need code in place for
    // _displaying_ that data.


    fprintf(stderr, "OpenTRAC: %d bytes\n", len);


    // Main loop.  Keep decoding elements until we run out of things
    // to process.
    //
    while (decoded < len) {
        elen = (int)*data;
        decoded += (elen & 0x7f)+1;
        if (elen & 0x80) { // See if it's got a 16-bit ID
            elen = (elen & 0x7f) - 2; // Strip the extid flag
            etype = fetch16bits(++data);
        }
        else {
            elen--; // Don't count the type byte
            etype = (int)*(data+1);
        }
        data+=2; // Skip to the body
        fprintf(stderr, "EID 0x%x len %d: ", etype, elen);
        switch (etype) {
            case (0x00): // Sequence
                temp_entity_sequence = entity_sequence;

                OpenTrac_decode_sequence(
                    data,
                    elen,
                    &temp_entity_sequence);

                if (temp_entity_sequence != entity_sequence) {
                    process_opentrac_aprs();
                    entity_sequence = temp_entity_sequence;
                }

// Problem here:  What to do for multi-sequence OpenTrac packets?
// Generate a new APRS packet for each so that we get an entire
// history?

                break;
            case (0x01): // Originating Station
                temp_entity_sequence = entity_sequence;

                OpenTrac_decode_origination(
                    data,
                    elen,
                    &temp_entity_sequence,   // Origin sequence?
                    origin_call,
                    &origin_ssid,
                    &network);

                if (temp_entity_sequence != entity_sequence) {
                    process_opentrac_aprs();
                    entity_sequence = temp_entity_sequence;
                }

                // Originating station different from transmitting
                // station.

                break;
            case (0x02): // Entity ID
                temp_entity_sequence = entity_sequence;

                OpenTrac_decode_entityid(
                    data,
                    elen,
                    origin_call,
                    origin_ssid,
                    entity_call,
                    &entity_ssid,
                    &entity_serial,
                    &temp_entity_sequence);

                if (temp_entity_sequence != entity_sequence) {
                    process_opentrac_aprs();
                    entity_sequence = temp_entity_sequence;
                }

// We're dealing with a different entity than the transmitting or
// originating station.  Need to do something different here,
// perhaps creating an object or an item from it?

                break;
            case (0x10): // Position report
                OpenTrac_decode_position(
                    data,
                    elen,
                    &latitude,
                    &longitude,
                    &altitude);
                have_position++;
//fprintf(stderr, "Decoded this position: %f %f\n", latitude, longitude);
                break;
            case (0x11): // Timestamp
                OpenTrac_decode_timestamp(
                    data,
                    elen,
                    &rawtime);
                break;
            case (0x12): // Comment
                OpenTrac_decode_comment(
                    data,
                    elen,
                    comment);
                break;
            case (0x13): // Course and Speed
                OpenTrac_decode_courseandspeed(
                    data,
                    elen,
                    &course,    // degrees true
                    &speed);    // kph
                break;
            case (0x14): // Positional Ambiguity
                OpenTrac_decode_ambiguity(
                    data,
                    elen,
                    &ambiguity);
                break;
            case (0x15): // Country Code
                OpenTrac_decode_country(
                    data,
                    elen,
                    country,
                    subdivision);
                break;
            case (0x16): // Display Name
                OpenTrac_decode_displayname(
                    data,
                    elen,
                    displayname);
                break;
            case (0x17): // Waypoint Name
                OpenTrac_decode_waypoint(
                    data,
                    elen,
                    waypoint);
                break;
            case (0x18): // Map Symbol
                OpenTrac_decode_symbol(
                    data,
                    elen,
                    symbol,
                    &aprs_symbol_table,
                    &aprs_symbol_char);
                break;
             case (0x20): // Path Trace
                OpenTrac_decode_pathtrace(
                    data,
                    elen);
                break;
            case (0x21): // Heard-By List
                OpenTrac_decode_heardby(
                    data,
                    elen);
                break;
            case (0x22): // Available Networks
                OpenTrac_decode_availablenets(
                    data,
                    elen);
                break;
            case (0x32): // Maidenhead Locator
                OpenTrac_decode_maidenhead(
                    data,
                    elen,
                    maidenhead);
                break;
            case (0x34): // GPS Data Quality
                OpenTrac_decode_gpsquality(
                    data,
                    elen);
                break;
            case (0x35): // Aircraft Registration
                OpenTrac_decode_acreg(
                    data,
                    elen,
                    aircraft_id);
                break;
            case (0x42): // River Flow Gauge
                OpenTrac_decode_rivergauge(
                    data,
                    elen);
                break;
            case (0x100): // Emergency/distress flag
                OpenTrac_flag_emergency();
                break;
            case (0x101): // Attention/ident flag
                OpenTrac_flag_attention();
                break;
            case (0x300): // Hazmat
                OpenTrac_decode_hazmat(
                    data,
                    elen,
                    &hazmat_id,
                    comment);
                break;
            default: // Everything else
                if ((etype & 0xff00) == 0x500) {
                    OpenTrac_decode_units(
                        etype & 0x00ff,
                        data,
                        elen,
                        comment);
                }
                else {
                    fprintf(stderr, "Unknown Element Type\n");
                }
                break;
        }
        data+=elen;
    }

    process_opentrac_aprs();

    return( buffer );
}
 




//***********************************************************
// process_ax25_packet()
//
// bp       raw packet data
// len      length of raw packet data
// buffer   buffer to write readable packet data to
//
// Note that db.c:decode_ax25_header does much the same thing for
// Serial KISS interface packets.  Consider combining the two
// functions.  process_ax25_packet() would be the earlier and more
// thought-out function.  This function now has some OpenTrac code
// as well.
//***********************************************************

char *process_ax25_packet(unsigned char *bp, unsigned int len, char *buffer) {
    int i,j;
    unsigned int  k,l;
    unsigned int  digis;
    unsigned char source[10];
    unsigned char dest[10];
    unsigned char digi[10][10];
    unsigned char digi_h[10];
    unsigned int  ssid;
    unsigned char message[513];

    if ( (bp == NULL) || (buffer == NULL) )
        return(NULL);

    /* clear buffer */
    strcpy(buffer,"");

    if (*bp != (unsigned char)0)
        return(NULL); /* not a DATA packet */

    // We have a KISS packet here, so we know that the first
    // character is a flag character.  Skip over it.
    bp++;
    len--;

    // Check the length to make sure that we don't have an empty
    // packet.
    if (!bp || !len)
        return(NULL);

    // Check for minimum KISS frame bytes.
    if (len < 15)
        return(NULL);

    if (bp[1] & 1) /* Compressed FlexNet Header */
        return(NULL);

    /* Destination of frame */
    j = 0;
    for(i = 0; i < 6; i++) {
        if ((bp[i] &0xfe) != (unsigned char)0x40)
            dest[j++] = bp[i] >> 1;
    }
    ssid = (unsigned int)( (bp[6] & 0x1e) >> 1 );
    if (ssid != 0) {
        dest[j++] = '-';
        if ((ssid / 10) != 0) {
            dest[j++] = '1';
        }
        ssid = (ssid % 10);
        dest[j++] = (unsigned char)ssid + (unsigned char)'0';
    }
    dest[j] = '\0';
    bp += 7;
    len -= 7;

    /* Source of frame */
    j = 0;
    for(i = 0; i < 6; i++) {
        if ((bp[i] &0xfe) != (unsigned char)0x40)
            source[j++] = bp[i] >> 1;
    }
    ssid = (unsigned int)( (bp[6] & 0x1e) >> 1 );
    if (ssid != 0) {
        source[j++] = '-';
        if ((ssid / 10) != 0) {
            source[j++] = '1';
        }
        ssid = (ssid % 10);
        source[j++] = (unsigned char)ssid + (unsigned char)'0';
    }
    source[j] = '\0';
    bp += 7;
    len -= 7;

    /* Digipeaters */
    digis = 0;
    while ((!(bp[-1] & 1)) && (len >= 7)) {
        /* Digi of frame */
        if (digis != 10) {
            j = 0;
            for (i = 0; i < 6; i++) {
                if ((bp[i] &0xfe) != (unsigned char)0x40)
                    digi[digis][j++] = bp[i] >> 1;
            }
            digi_h[digis] = (bp[6] & 0x80);
            ssid = (unsigned int)( (bp[6] & 0x1e) >> 1 );
            if (ssid != 0) {
                digi[digis][j++] = '-';
                if ((ssid / 10) != 0) {
                    digi[digis][j++] = '1';
                }
                ssid = (ssid % 10);
                digi[digis][j++] = (unsigned char)ssid + (unsigned char)'0';
            }
            digi[digis][j] = '\0';
            digis++;
        }
        bp += 7;
        len -= 7;
    }
    if (!len)
        return(NULL);

    /* We are now at the primitive bit */
    i = (int)(*bp++);
    len--;

    /* strip the poll-bit from the primitive */
    i = i & (~0x10);

    /* return if this is not an UI frame (= 0x03) */
    if(i != 0x03)
        return(NULL);

    /* no data left */
    if (!len)
        return(NULL);

    // Check whether we're dealing with an OpenTrac KISS packet.  If
    // so, go process it and then pass through the return code that
    // the OpenTrac functions provide.
    if (*bp == 0x77) {
        char* ret;

        bp++;
        len--;
        // We have an OpenTrac packet.
        ret = process_OpenTrac_packet(
                    bp,
                    len,
                    buffer,
                    source,
                    ssid,
                    dest,
                    digis,
                    digi,
                    digi_h);

        // If we wish to process some APRS-converted OpenTrac
        // packet: 
        return(ret);

        // If we processed it in another manner and just wish to
        // quit:
        //return(NULL);

    }
    else if(*bp != (unsigned char)0xF0) {   // APRS PID
        // We _don't_ have an APRS packet
        return(NULL);
    }

    // We have what looks like a valid KISS-frame containing APRS
    // protocol data.

    bp++;
    len--;
    k = 0;
    l = 0;
    while (len) {
        i = (int)(*bp++);
        if ((i != (int)'\n') && (i != (int)'\r')) {
            if (l < 512)
                message[l++] = (unsigned char)i;
        }
        len--;
    }
    /* add terminating '\0' to allow handling as a string */
    message[l] = '\0';

    strcat(buffer,(char *)source);
    /*
     * if there are no digis or the first digi has not handled the
     * packet then this is directly from the source, mark it with
     * a "*" in that case
     */

    /* I don't think we need this just yet, perhaps not at all? */
    /* I think if we don't have a '*' in the path we can assume direct? -FG */
    /*
    if((digis == 0) || (digi_h[0] != 0x80))
        strcat(buffer,"*");
     */

    strcat(buffer,">");

    /* destination is at the begining of the chain, because it is  */
    /* needed so MIC-E packets can be decoded correctly. */
    /* this may be changed in the future but for now leave it here -FG */
    strcat(buffer,(char *)dest);

    for(i = 0; i < (int)digis; i++) {
        strcat(buffer,",");
        strcat(buffer,(char *)digi[i]);
        /* at the last digi always put a '*' when h_bit is set */
        if (i == (int)(digis - 1)) {
            if (digi_h[i] == (unsigned char)0x80) {
                /* this digi must have transmitted the packet */
                strcat(buffer,"*");
            }
        } else {
            if (digi_h[i] == (unsigned char)0x80) {
                /* only put a '*' when the next digi has no h_bit */
                if (digi_h[i + 1] != (unsigned char)0x80) {
                    /* this digi must have transmitted the packet */
                    strcat(buffer,"*");
                }
            }
        }
    }
    strcat(buffer,":");

    // Have to be careful here:  With all of the text we've already
    // but into the buffer, we don't want to overflow it with the
    // message data.
    //strcat(buffer,(char *)message);   // Old insecure code.

    //Copy into only the free space in buffer.
    strncat( buffer, (char *)message, MAX_DEVICE_BUFFER - strlen(buffer) - 1 );

    // And null-terminate it to make sure.
    buffer[MAX_DEVICE_BUFFER - 1] = '\0';

    return(buffer);
}





//*********************************************************
// AX25 port INIT
//
// port is port# used
//*********************************************************

int ax25_init(int port) {

    /*
        COMMENT:tested this Seems to work fine as ETH_P_AX25
        on newer linux kernels (and you see your own transmissions
        but it is not good for older linux kernels and FreeBSD  -FG
    */

#ifdef HAVE_LIBAX25
    int proto = PF_AX25;
    char temp[200];
    char *dev = NULL;
#endif  // HAVE_LIBAX25

    if (begin_critical_section(&port_data_lock, "interface.c:ax25_init(1)" ) > 0)
        fprintf(stderr,"port_data_lock, Port = %d\n", port);

    /* clear port_channel */
//    port_data[port].channel = -1;

    /* clear port active */
    port_data[port].active = DEVICE_NOT_IN_USE;

    /* clear port status */
    port_data[port].status = DEVICE_DOWN;

#ifdef HAVE_LIBAX25
    if (ax25_ports_loaded == 0) {
        /* port file has not been loaded before now */
        if (ax25_config_load_ports() == 0) {
            fprintf(stderr, "ERROR: problem with axports file\n");
            popup_message(langcode("POPEM00004"),langcode("POPEM00013"));

            if (end_critical_section(&port_data_lock, "interface.c:ax25_init(2)" ) > 0)
                fprintf(stderr,"port_data_lock, Port = %d\n", port);

            return -1;
        }
        /* we can only load the port file once!!! so do not load again */
        ax25_ports_loaded = 1;
    }

    if (port_data[port].device_name != NULL) {
        if ((dev = ax25_config_get_dev(port_data[port].device_name)) == NULL) {
            xastir_snprintf(temp, sizeof(temp), langcode("POPEM00014"),
                    port_data[port].device_name);
            popup_message(langcode("POPEM00004"),temp);

            if (end_critical_section(&port_data_lock, "interface.c:ax25_init(3)" ) > 0)
                fprintf(stderr,"port_data_lock, Port = %d\n", port);

            return -1;
        }
    }

    /* COMMENT: tested this AF_INET is CORRECT -FG */
    // Commented out sections below.  We keep the old socket number
    // around now, so have to start a new socket in all cases to make it work.
//    if (port_data[port].channel == -1) {

    ENABLE_SETUID_PRIVILEGE;
#if __GLIBC__ >= 2 && __GLIBC_MINOR >= 3
    port_data[port].channel = socket(PF_INET, SOCK_DGRAM, htons(proto));   // proto = AF_AX25
#else   // __GLIBC__ >= 2 && __GLIBC_MINOR >= 3
    port_data[port].channel = socket(PF_INET, SOCK_PACKET, htons(proto));
#endif      // __GLIBC__ >= 2 && __GLIBC_MINOR >= 3
    DISABLE_SETUID_PRIVILEGE;

    if (port_data[port].channel == -1) {
        perror("socket");
        if (end_critical_section(&port_data_lock, "interface.c:ax25_init(4)" ) > 0)
            fprintf(stderr,"port_data_lock, Port = %d\n", port);

        return -1;
    }

//    }
//    else {
        // Use socket number that is already defined
//    }

    /* port active */
    port_data[port].active = DEVICE_IN_USE;

    /* port status */
    port_data[port].status = DEVICE_UP;


    temp[0] = proto = (int)dev;     // Converting pointer to int, then to char?????
    // ^^^^ this doesn't do anything
#else /* HAVE_LIBAX25 */
    fprintf(stderr,"AX.25 support not compiled into Xastir!\n");
    popup_message(langcode("POPEM00004"),langcode("POPEM00021"));
#endif /* HAVE_LIBAX25 */
    if (end_critical_section(&port_data_lock, "interface.c:ax25_init(5)" ) > 0)
        fprintf(stderr,"port_data_lock, Port = %d\n", port);

    return(1);
}

//********************************* STOP AX.25 ********************************





//*************************** START SERIAL PORT FUNCTIONS ********************************


//******************************************************
// command file to tnc port
// port to send config data to
// Filename containing the config data
//******************************************************
int command_file_to_tnc_port(int port, char *filename) {
    FILE *f;
    char line[MAX_LINE_SIZE+1];
    char command[MAX_LINE_SIZE+5];
    int i;
    char cin;
    int error;
    struct stat file_status;


    if (filename == NULL)
        return(-1);

    // Check file status
    if (stat(filename, &file_status) < 0) {
        fprintf(stderr,
            "Couldn't stat file: %s\n",
            filename);
        fprintf(stderr,
            "Skipping send to TNC\n");
        return(-1);
    }

    // Check that it is a regular file
    if (!S_ISREG(file_status.st_mode)) {
        fprintf(stderr,
            "File is not a regular file: %s\n",
            filename);
        fprintf(stderr,
            "Skipping send to TNC\n");
        return(-1);
    } 

    error = 0;
    i = 0;
    f = fopen(filename,"r");
    if (f != NULL) {
        line[0] = (char)0;
        while (!feof(f) && error != -1) {
            if (fread(&cin,1,1,f) == 1) {
                if (cin != (char)10 && cin != (char)13) {
                    if (i < MAX_LINE_SIZE) {
                        line[i++] = cin;
                        line[i] = (char)0;
                    }
                } else {
                    i = 0;
                    if (line[0] != '#' && strlen(line) > 0) {
                        xastir_snprintf(command, sizeof(command), "%c%s\r", (char)03, line);
                        if (debug_level & 2)
                            fprintf(stderr,"CMD:%s\n",command);

                        port_write_string(port,command);
                        line[0] = (char)0;
                    }
                }
            }
        }
        (void)fclose(f);
    } else {
        if (debug_level & 2)
            fprintf(stderr,"Could not open TNC command file: %s\n",filename);
    }
    return(error);
}





//***********************************************************
// port_dtr INIT
// port is port# used
// dtr 1 is down, 0 is normal(up)
//***********************************************************
void port_dtr(int port, int dtr) {

// It looks like we have two methods of getting this to compile on
// CYGWIN, getting rid of the entire procedure contents, and getting
// rid of the TIO* code.  One method or the other should work to get
// it compiled.  We shouldn't need both.
    int sg;

    /* check for 1 or 0 */
    dtr = (dtr & 0x1);

    if (begin_critical_section(&port_data_lock, "interface.c:port_dtr(1)" ) > 0)
        fprintf(stderr,"port_data_lock, Port = %d\n", port);

    if (port_data[port].active == DEVICE_IN_USE
            && port_data[port].status == DEVICE_UP
            && port_data[port].device_type == DEVICE_SERIAL_TNC_HSP_GPS){

        port_data[port].dtr = dtr;
        if (debug_level & 2)
            fprintf(stderr,"DTR %d\n",port_data[port].dtr);

#ifdef TIOCMGET
            ENABLE_SETUID_PRIVILEGE;
        (void)ioctl(port_data[port].channel, TIOCMGET, &sg);
            DISABLE_SETUID_PRIVILEGE;
#endif  // TIOCMGET

        sg &= 0xff;

#ifdef TIOCM_DTR
        sg = TIOCM_DTR;
#endif  // TIOCM_DIR

        if (dtr) {
            dtr &= ~sg;

#ifdef TIOCMBIC
            ENABLE_SETUID_PRIVILEGE;
            (void)ioctl(port_data[port].channel, TIOCMBIC, &sg);
            DISABLE_SETUID_PRIVILEGE;
#endif  // TIOCMBIC

            if (debug_level & 2)
                fprintf(stderr,"Down\n");

            // statusline(langcode("BBARSTA026"),1);

        } else {
            dtr |= sg;

#ifdef TIOCMBIS
            ENABLE_SETUID_PRIVILEGE;
            (void)ioctl(port_data[port].channel, TIOCMBIS, &sg);
            DISABLE_SETUID_PRIVILEGE;
#endif  // TIOCMBIS

            if (debug_level & 2)
                fprintf(stderr,"UP\n");

            // statusline(langcode("BBARSTA027"),1);
        }
    }

    if (end_critical_section(&port_data_lock, "interface.c:port_dtr(2)" ) > 0)
        fprintf(stderr,"port_data_lock, Port = %d\n", port);
}





//***********************************************************
// port_dtr INIT
// port is port# used
// dtr 1 is down, 0 is normal(up)
//***********************************************************
void dtr_all_set(int dtr) {
    int i;

//fprintf(stderr,"dtr_all_set(%d)\t",dtr);
    for (i = 0; i < MAX_IFACE_DEVICES; i++) {
        if (port_data[i].device_type == DEVICE_SERIAL_TNC_HSP_GPS
                && port_data[i].status == DEVICE_UP) {
            port_dtr(i,dtr);
        }
    }
}





//***********************************************************
// Serial port close
// port is port# used
//***********************************************************
int serial_detach(int port) {
    char fn[600];
    int ok;
    ok = -1;

    if (begin_critical_section(&port_data_lock, "interface.c:serial_detach(1)" ) > 0)
        fprintf(stderr,"port_data_lock, Port = %d\n", port);

    if (port_data[port].active == DEVICE_IN_USE && port_data[port].status == DEVICE_UP){

        // Close port first
        (void)tcsetattr(port_data[port].channel, TCSANOW, &port_data[port].t_old);
        if (close(port_data[port].channel) == 0) {
            port_data[port].status = DEVICE_DOWN;
            usleep(200);
            port_data[port].active = DEVICE_NOT_IN_USE;
            ok = 1;
        } else {
            if (debug_level & 2)
                fprintf(stderr,"Could not close port %s\n",port_data[port].device_name);

            port_data[port].status = DEVICE_DOWN;
            usleep(200);
            port_data[port].active = DEVICE_NOT_IN_USE;
        }

        // Now delete lock
        xastir_snprintf(fn, sizeof(fn), "/var/lock/LCK..%s", get_device_name_only(port_data[port].device_name));
        if (debug_level & 2)
            fprintf(stderr,"Delete lock file %s\n",fn);

        ENABLE_SETUID_PRIVILEGE;
        (void)unlink(fn);
        DISABLE_SETUID_PRIVILEGE;
    }

    if (end_critical_section(&port_data_lock, "interface.c:serial_detach(2)" ) > 0)
        fprintf(stderr,"port_data_lock, Port = %d\n", port);

    return(ok);
}





//***********************************************************
// Serial port INIT
// port is port# used
//***********************************************************
int serial_init (int port) {
    FILE *lock;
    int speed;
    pid_t mypid = 0;
    int myintpid;
    char fn[600];
    uid_t user_id;
    struct passwd *user_info;
    char temp[100];
    char temp1[100];
    pid_t status;

    status = -9999;

    if (begin_critical_section(&port_data_lock, "interface.c:serial_init(1)" ) > 0)
        fprintf(stderr,"port_data_lock, Port = %d\n", port);

    // clear port_channel
    port_data[port].channel = -1;

    // clear port active
    port_data[port].active = DEVICE_NOT_IN_USE;

    // clear port status
    port_data[port].status = DEVICE_DOWN;

    // check for lock file
    xastir_snprintf(fn, sizeof(fn), "/var/lock/LCK..%s",
            get_device_name_only(port_data[port].device_name));
    if (filethere(fn) == 1) {
        // Also look for pid of other process and see if it is a valid lock
        fprintf(stderr,"Found an existing lockfile %s for this port!\n",fn);
        lock = fopen(fn,"r");
        if (lock != NULL) { // We could open it so it must have
                            // been created by this userid
            if (fscanf(lock,"%d %99s %99s",&myintpid,temp,temp1) == 3) {
                //fprintf(stderr,"Current lock %d %s %s\n",mypid,temp,temp1);
                mypid = (pid_t)myintpid;

#ifdef HAVE_GETPGRP
  #ifdef GETPGRP_VOID
                status = getpgrp();
  #else // GETPGRP_VOID
                status = getpgrp(mypid);
  #endif // GETPGRP_VOID
#else   // HAVE_GETPGRP
                status = getpgid(mypid);
#endif // HAVE_GETPGRP

            }

            (void)fclose(lock);

            // check to see if it is stale
            if (status != mypid) {
                fprintf(stderr,"Lock is stale!  Removing it.\n");
                ENABLE_SETUID_PRIVILEGE;
                (void)unlink(fn);
                DISABLE_SETUID_PRIVILEGE;
            } else {
                fprintf(stderr,"Cannot open port:  Another program has the lock!\n");

                if (end_critical_section(&port_data_lock, "interface.c:serial_init(2)" ) > 0)
                    fprintf(stderr,"port_data_lock, Port = %d\n", port);

                return (-1);
            }
        } else {    // Couldn't open it, so the lock must have been
                    // created by another userid
            fprintf(stderr,"Cannot open port:  Another program has the lock!\n");

            if (end_critical_section(&port_data_lock, "interface.c:serial_init(3)" ) > 0)
                fprintf(stderr,"port_data_lock, Port = %d\n", port);

            return (-1);
        }
    }

    // Try to open the serial port now
    ENABLE_SETUID_PRIVILEGE;
    port_data[port].channel = open(port_data[port].device_name, O_RDWR|O_NOCTTY);
    DISABLE_SETUID_PRIVILEGE;
    if (port_data[port].channel == -1){

        if (end_critical_section(&port_data_lock, "interface.c:serial_init(4)" ) > 0)
            fprintf(stderr,"port_data_lock, Port = %d\n", port);

        if (debug_level & 2)
            fprintf(stderr,"Could not open channel on port %d!\n",port);

        return (-1);
    }

    // Attempt to create the lock file
    xastir_snprintf(fn, sizeof(fn), "/var/lock/LCK..%s", get_device_name_only(port_data[port].device_name));
    if (debug_level & 2)
        fprintf(stderr,"Create lock file %s\n",fn);

    ENABLE_SETUID_PRIVILEGE;
    lock = fopen(fn,"w");
    DISABLE_SETUID_PRIVILEGE;
    if (lock != NULL) {
        // get my process id for lock file
        mypid = getpid();

        // get user info
        user_id = getuid();
        user_info = getpwuid(user_id);
        strcpy(temp,user_info->pw_name);

        fprintf(lock,"%9d %s %s",(int)mypid,"xastir",temp);
        (void)fclose(lock);
        // We've successfully created our own lock file
    }
    else {
        // lock failed
        if (debug_level & 2)
            fprintf(stderr,"Warning:  Failed opening LCK file!  Continuing on...\n");

        /* if we can't create lock file don't fail!

if (end_critical_section(&port_data_lock, "interface.c:serial_init(5)" ) > 0)
    fprintf(stderr,"port_data_lock, Port = %d\n", port);

        return (-1);*/
    }

    // get port attributes for new and old
    if (tcgetattr(port_data[port].channel, &port_data[port].t) != 0) {

    if (end_critical_section(&port_data_lock, "interface.c:serial_init(6)" ) > 0)
        fprintf(stderr,"port_data_lock, Port = %d\n", port);

        if (debug_level & 2)
            fprintf(stderr,"Could not get t port attributes for port %d!\n",port);

        // Here we should close the port and remove the lock.
        serial_detach(port);

        return (-1);
    }

    if (tcgetattr(port_data[port].channel, &port_data[port].t_old) != 0) {

    if (end_critical_section(&port_data_lock, "interface.c:serial_init(7)" ) > 0)
        fprintf(stderr,"port_data_lock, Port = %d\n", port);

        if (debug_level & 2)
            fprintf(stderr,"Could not get t_old port attributes for port %d!\n",port);

        // Here we should close the port and remove the lock.
        serial_detach(port);

        return (-1);
    }

    // set time outs
    port_data[port].t.c_cc[VMIN] = (cc_t)1;
    port_data[port].t.c_cc[VTIME] = (cc_t)2;

    // set port flags
    port_data[port].t.c_iflag &= ~(BRKINT | IGNPAR | PARMRK | INPCK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    port_data[port].t.c_iflag = (tcflag_t)(IGNBRK | IGNPAR);

    port_data[port].t.c_oflag = (0);
    port_data[port].t.c_lflag = (0);

#ifdef    CBAUD
    speed = (int)(port_data[port].t.c_cflag & CBAUD);
#else   // CBAUD
    speed = 0;
#endif  // CBAUD
    port_data[port].t.c_cflag = (tcflag_t)(HUPCL|CLOCAL|CREAD);
    port_data[port].t.c_cflag &= ~PARENB;
    switch (port_data[port].style){
        case(0):
            // No parity (8N1)
            port_data[port].t.c_cflag &= ~CSTOPB;
            port_data[port].t.c_cflag &= ~CSIZE;
            port_data[port].t.c_cflag |= CS8;
            break;

        case(1):
            // Even parity (7E1)
            port_data[port].t.c_cflag &= ~PARODD;
            port_data[port].t.c_cflag &= ~CSTOPB;
            port_data[port].t.c_cflag &= ~CSIZE;
            port_data[port].t.c_cflag |= CS7;
            break;

        case(2):
            // Odd parity (7O1):
            port_data[port].t.c_cflag |= PARODD;
            port_data[port].t.c_cflag &= ~CSTOPB;
            port_data[port].t.c_cflag &= ~CSIZE;
            port_data[port].t.c_cflag |= CS7;
            break;

        default:
            break;
    }

    port_data[port].t.c_cflag |= speed;
    // set input and out put speed
    if (cfsetispeed(&port_data[port].t, port_data[port].sp) == -1) {

    if (end_critical_section(&port_data_lock, "interface.c:serial_init(8)" ) > 0)
        fprintf(stderr,"port_data_lock, Port = %d\n", port);

        if (debug_level & 2)
            fprintf(stderr,"Could not set port input speed for port %d!\n",port);

        // Here we should close the port and remove the lock.
        serial_detach(port);

        return (-1);
    }

    if (cfsetospeed(&port_data[port].t, port_data[port].sp) == -1) {

        if (end_critical_section(&port_data_lock, "interface.c:serial_init(9)" ) > 0)
            fprintf(stderr,"port_data_lock, Port = %d\n", port);

        if (debug_level & 2)
            fprintf(stderr,"Could not set port output speed for port %d!\n",port);

        // Here we should close the port and remove the lock.
        serial_detach(port);

        return (-1);
    }

    if (tcflush(port_data[port].channel, TCIFLUSH) == -1) {

        if (end_critical_section(&port_data_lock, "interface.c:serial_init(10)" ) > 0)
            fprintf(stderr,"port_data_lock, Port = %d\n", port);

        if (debug_level & 2)
            fprintf(stderr,"Could not flush data for port %d!\n",port);

        // Here we should close the port and remove the lock.
        serial_detach(port);

        return (-1);
    }

    if (tcsetattr(port_data[port].channel,TCSANOW, &port_data[port].t) == -1) {

        if (end_critical_section(&port_data_lock, "interface.c:serial_init(11)" ) > 0)
            fprintf(stderr,"port_data_lock, Port = %d\n", port);

        if (debug_level & 2)
            fprintf(stderr,"Could not set port attributes for port %d!\n",port);

        // Here we should close the port and remove the lock.
        serial_detach(port);

        return (-1);
    }

    // clear port active
    port_data[port].active = DEVICE_IN_USE;

    // clear port status
    port_data[port].status = DEVICE_UP;

    if (end_critical_section(&port_data_lock, "interface.c:serial_init(12)" ) > 0)
        fprintf(stderr,"port_data_lock, Port = %d\n", port);

    // return good condition
    return (1);
}

//*************************** STOP SERIAL PORT FUNCTIONS ********************************





//***************************** START NETWORK FUNCTIONS *********************************

//**************************************************************
// net_connect_thread()
// Temporary thread used to start up a socket.
//**************************************************************
static void* net_connect_thread(void *arg) {
    int port;
    int ok;
    int len;
    int result;
    int flag;
    //int stat;
    struct sockaddr_in address;

    // Some messiness necessary because we're using
    // xastir_mutex's instead of pthread_mutex_t's.
    pthread_mutex_t *cleanup_mutex;


    if (debug_level & 2)
        fprintf(stderr,"net_connect_thread start\n");

    port = *((int *) arg);
    // This call means we don't care about the return code and won't
    // use pthread_join() later.  Makes threading more efficient.
    (void)pthread_detach(pthread_self());
    ok = -1;

//if (begin_critical_section(&port_data_lock, "interface.c:net_connect_thread(1)" ) > 0)
//    fprintf(stderr,"port_data_lock, Port = %d\n", port);

    /* set address */
    address.sin_addr.s_addr = (port_data[port].address);

    // Create a socket if we don't have one yet for this channel
    if (port_data[port].channel == -1) {
        pthread_testcancel();   // Check for thread termination request
        port_data[port].channel = socket(PF_INET, SOCK_STREAM, 0);
        pthread_testcancel();   // Check for thread termination request
    }

    if (port_data[port].channel != -1) {
        if (debug_level & 2)
            fprintf(stderr,"We have a socket to use\n");
        address.sin_family = AF_INET;
        address.sin_port = htons(port_data[port].socket_port);
        if (debug_level & 2)
            fprintf(stderr,"after htons\n");
        len = (int)sizeof(address);
        flag = 1;

        // Turn on the socket keepalive option
        (void)setsockopt(port_data[port].channel,  SOL_SOCKET, SO_KEEPALIVE, (char *) &flag, sizeof(int));

        // Disable the Nagle algorithm (speeds things up)
        (void)setsockopt(port_data[port].channel, IPPROTO_TCP,  TCP_NODELAY, (char *) &flag, sizeof(int));

        if (debug_level & 2)
            fprintf(stderr,"after setsockopt\n");
        pthread_testcancel();  // Check for thread termination request
        if (debug_level & 2)
            fprintf(stderr,"calling connect(), port: %d\n", port_data[port].socket_port);
        result = connect(port_data[port].channel, (struct sockaddr *)&address, len);
        if (debug_level & 2)
            fprintf(stderr,"connect result was: %d\n", result);
        pthread_testcancel();  // Check for thread termination request
        if (result != -1){
            /* connection up */
            if (debug_level & 2)
                fprintf(stderr,"net_connect_thread():Net up, port %d\n",port);

            port_data[port].status = DEVICE_UP;
            ok = 1;
        } else {    /* net connection failed */
            ok = 0;
            if (debug_level & 2)
                fprintf(stderr,"net_connect_thread():net connection failed, port %d, DEVICE_ERROR ***\n",port);
            port_data[port].status = DEVICE_ERROR;

            // Shut down and close the socket

            //pthread_testcancel();   // Check for thread termination request
            //
            // Don't do a shutdown!  The socket wasn't connected.  This causes
            // problems due to the same socket number getting recycled.  It can
            // shut down another socket.
            //
            //stat = shutdown(port_data[port].channel,2);
            //pthread_testcancel();   // Check for thread termination request
            //if (debug_level & 2)
            //    fprintf(stderr,"net_connect_thread():Net Shutdown 1 Returned %d, port %d\n",stat,port);

            usleep(100000); // 100ms
            //pthread_testcancel();   // Check for thread termination request
            //stat = close(port_data[port].channel);
            //pthread_testcancel();   // Check for thread termination request
            //if (debug_level & 2)
            //    fprintf(stderr,"net_connect_thread():Net Close 1 Returned %d, port %d\n",stat,port);

            //if (debug_level & 2)
            //    fprintf(stderr,"net_connect_thread():Net connection 1 failed, port %d\n",port);
        }
    } else { /* Could not bind socket */
        ok = -1;
        if (debug_level & 2)
            fprintf(stderr,"net_connect_thread():could not bind socket, port %d, DEVICE_ERROR ***\n",port);
        port_data[port].status = DEVICE_ERROR;

        // Shut down and close the socket
        //pthread_testcancel();   // Check for thread termination request
        //
        // Don't do a shutdown!  The socket wasn't connected.  This causes
        // problems due to the same socket number getting recycled.  It can
        // shut down another socket.
        //
        //stat = shutdown(port_data[port].channel,2);
        //pthread_testcancel();   // Check for thread termination request
        //if (debug_level & 2)
        //    fprintf(stderr,"net_connect_thread():Net Shutdown 2 Returned %d, port %d\n",stat,port);

        usleep(100000); // 100ms
        //pthread_testcancel();   // Check for thread termination request
        //stat = close(port_data[port].channel);
        //pthread_testcancel();   // Check for thread termination request
        //if (debug_level & 2)
        //    fprintf(stderr,"net_connect_thread():Net Close 2 Returned %d, port %d\n",stat,port);

        //if (debug_level & 2)
        //    fprintf(stderr,"net_connect_thread():Could not bind socket, port %d\n",port);
    }


    // Install the cleanup routine for the case where this thread
    // gets killed while the mutex is locked.  The cleanup routine
    // initiates an unlock before the thread dies.  We must be in
    // deferred cancellation mode for the thread to have this work
    // properly.  We must first get the pthread_mutex_t address:
    cleanup_mutex = &connect_lock.lock;

    // Then install the cleanup routine:
    pthread_cleanup_push((void *)pthread_mutex_unlock, (void *)cleanup_mutex);

    if (begin_critical_section(&connect_lock, "interface.c:net_connect_thread(2)" ) > 0)
        fprintf(stderr,"net_connect_thread():connect_lock, Port = %d\n", port);

    port_data[port].connect_status = ok;
    port_data[port].thread_status = 0;

    if (end_critical_section(&connect_lock, "interface.c:net_connect_thread(3)" ) > 0)
        fprintf(stderr,"net_connect_thread():connect_lock, Port = %d\n", port);

    // Remove the cleanup routine for the case where this thread
    // gets killed while the mutex is locked.  The cleanup routine
    // initiates an unlock before the thread dies.  We must be in
    // deferred cancellation mode for the thread to have this work
    // properly.
    pthread_cleanup_pop(0);
 

//if (end_critical_section(&port_data_lock, "interface.c:net_connect_thread(4)" ) > 0)
//    fprintf(stderr,"port_data_lock, Port = %d\n", port);

    if (debug_level & 2)
        fprintf(stderr,"net_connect_thread terminating itself\n");

    return(NULL);   // This should kill the thread
}





//**************************************************************
// net_init()
//
// This brings up a network connection
//
// returns -1 on hard error, 0 on time out, 1 if ok
//**************************************************************
int net_init(int port) {
    int ok;
    char ip_addrs[400];
    char ip_addr[40];
    char st[200];
    pthread_t connect_thread;
    int stat;
    int wait_on_connect;
    time_t wait_time;

    if (begin_critical_section(&port_data_lock, "interface.c:net_init(1)" ) > 0)
        fprintf(stderr,"port_data_lock, Port = %d\n", port);

    /* clear port_channel */
//    port_data[port].channel = -1;

    /* set port active */
    port_data[port].active = DEVICE_IN_USE;

    /* clear port status */
    port_data[port].status = DEVICE_DOWN;

    ok = -1;

    xastir_snprintf(st, sizeof(st), langcode("BBARSTA019"), port_data[port].device_host_name);
    statusline(st,1);   // Looking up host

    // We currently give 13 seconds to look up the hostname
    (void)host_lookup(port_data[port].device_host_name,ip_addrs,13);

    if (strcmp(ip_addrs,"NOIP") != 0) {
        if (strcmp(ip_addrs,"NOHOST") != 0) {
            if (strcmp(ip_addrs,"TIMEOUT") != 0) {    // We found an IP address
                /* get the first ip */
                (void)sscanf(ip_addrs,"%39s",ip_addr);
                if (debug_level & 2)
                    fprintf(stderr,"IP Address: %s\n",ip_addr);
                /* set address for connection */
                port_data[port].address = inet_addr(ip_addr);

                /* ok try to connect */

                if (begin_critical_section(&connect_lock, "interface.c:net_init(2)" ) > 0)
                    fprintf(stderr,"connect_lock, Port = %d\n", port);

                port_data[port].thread_status = 1;
                port_data[port].connect_status = -1;

                // If channel is != -1, we have a socket remaining from a previous
                // connect attempt.  Shutdown and close that socket, then create
                // a new one.
                if (port_data[port].channel != -1) {    // We have a socket already

                    // Shut down and close the socket
                    pthread_testcancel();   // Check for thread termination request
                    stat = shutdown(port_data[port].channel,2);
                    pthread_testcancel();   // Check for thread termination request
                    if (debug_level & 2)
                        fprintf(stderr,"net_connect_thread():Net Shutdown 1 Returned %d, port %d\n",stat,port);
                    usleep(100000);         // 100ms
                    pthread_testcancel();   // Check for thread termination request
                    stat = close(port_data[port].channel);
                    pthread_testcancel();   // Check for thread termination request
                    if (debug_level & 2)
                        fprintf(stderr,"net_connect_thread():Net Close 1 Returned %d, port %d\n",stat,port);
                    usleep(100000);         // 100ms
                    port_data[port].channel = -1;
               }

                if (end_critical_section(&connect_lock, "interface.c:net_init(3)" ) > 0)
                    fprintf(stderr,"connect_lock, Port = %d\n", port);

                if (debug_level & 2)
                    fprintf(stderr,"Creating new thread\n");
                if (pthread_create(&connect_thread, NULL, net_connect_thread, &port)){
                    /* error starting thread*/
                    ok = -1;
                    fprintf(stderr,"Error creating net_connect thread, port %d\n",port);
                }

                busy_cursor(appshell);
                wait_time = sec_now() + NETWORK_WAITTIME;  // Set ending time for wait
                wait_on_connect = 1;
                while (wait_on_connect && (sec_now() < wait_time)) {

                    if (begin_critical_section(&connect_lock, "interface.c:net_init(4)" ) > 0)
                        fprintf(stderr,"connect_lock, Port = %d\n", port);

                    wait_on_connect = port_data[port].thread_status;

                    if (end_critical_section(&connect_lock, "interface.c:net_init(5)" ) > 0)
                        fprintf(stderr,"connect_lock, Port = %d\n", port);

                    xastir_snprintf(st, sizeof(st), langcode("BBARSTA025"), wait_time - sec_now() );
                    statusline(st,1);           // Host found, connecting n
                    if (debug_level & 2)
                        fprintf(stderr,"%d\n", (int)(wait_time - sec_now()) );

                    /* update display while waiting */
                    // XmUpdateDisplay(XtParent(da));
                    usleep(20000);      // 20mS
                    //sched_yield();    // Too fast!
                }

                ok = port_data[port].connect_status;
                /* thread did not return! kill it */
                if ( (sec_now() >= wait_time)      // Timed out
                        || (ok != 1) ) {            // or connection failure of another type
                    if (debug_level & 2)
                        fprintf(stderr,"Thread exceeded it's time limit or failed to connect! Port %d\n",port);

                    if (begin_critical_section(&connect_lock, "interface.c:net_init(6)" ) > 0)
                        fprintf(stderr,"connect_lock, Port = %d\n", port);

                    if (debug_level & 2)
                        fprintf(stderr,"Killing thread\n");
                    if (pthread_cancel(connect_thread)) {
                        // The only error code we can get here is ESRCH, which means
                        // that the thread number wasn't found.  The thread is already
                        // dead, so let's not print out an error code.
                        //fprintf(stderr,"Error on termination of connect thread!\n");
                    }

                    if (sec_now() >= wait_time) {  // Timed out
                        port_data[port].connect_status = -2;
                        if (debug_level & 2)
                            fprintf(stderr,"It was a timeout.\n");
                    }

                    if (end_critical_section(&connect_lock, "interface.c:net_init(7)" ) > 0)
                        fprintf(stderr,"connect_lock, Port = %d\n", port);

                    port_data[port].status = DEVICE_ERROR;
                    if (debug_level & 2)
                        fprintf(stderr,"Thread did not return, port %d, DEVICE_ERROR ***\n",port);
                }
                if (begin_critical_section(&connect_lock, "interface.c:net_init(8)" ) > 0)
                    fprintf(stderr,"connect_lock, Port = %d\n", port);

                ok = port_data[port].connect_status;

                if (end_critical_section(&connect_lock, "interface.c:net_init(9)" ) > 0)
                    fprintf(stderr,"connect_lock, Port = %d\n", port);

                if (debug_level & 2)
                    fprintf(stderr,"Net ok: %d, port %d\n", ok, port);

                switch (ok) {

                    case 1: /* connection up */
                        xastir_snprintf(st, sizeof(st), langcode("BBARSTA020"), port_data[port].device_host_name);
                        statusline(st,1);               // Connected to ...
                        break;

                    case 0:
                        xastir_snprintf(st, sizeof(st), langcode("BBARSTA021"));
                        statusline(st,1);               // Net Connection Failed!
                        ok = -1;
                        break;

                    case -1:
                        xastir_snprintf(st, sizeof(st), langcode("BBARSTA022"));
                        statusline(st,1);               // Could not bind socket
                        break;

                    case -2:
                        xastir_snprintf(st, sizeof(st), langcode("BBARSTA018"));
                        statusline(st,1);               // Net Connection timed out
                        ok = 0;
                        break;

                    default:
                        break;
                        /*break;*/
                }
            } else { /* host lookup time out */
                xastir_snprintf(st, sizeof(st), langcode("BBARSTA018"));
                statusline(st,1);                       // Net Connection timed out
                port_data[port].status = DEVICE_ERROR;
                if (debug_level & 2)
                    fprintf(stderr,"Host lookup timeout, port %d, DEVICE_ERROR ***\n",port);
                ok = 0;
            }
        } else {    /* Host ip look up failure (no ip address for that host) */
            xastir_snprintf(st, sizeof(st), langcode("BBARSTA023"));
            statusline(st,1);                           // No IP for Host
            port_data[port].status = DEVICE_ERROR;
            if (debug_level & 2)
                fprintf(stderr,"Host IP lookup failure, port %d, DEVICE_ERROR ***\n",port);
        }
    }
    else {    /* Host look up failure (no host by that name) */
        xastir_snprintf(st, sizeof(st), langcode("BBARSTA023"));
        statusline(st,1);                               // No IP for Host
        port_data[port].status = DEVICE_ERROR;
        if (debug_level & 2)
            fprintf(stderr,"Host lookup failure, port %d, DEVICE_ERROR ***\n",port);
    }

    if (end_critical_section(&port_data_lock, "interface.c:net_init(10)" ) > 0)
        fprintf(stderr,"port_data_lock, Port = %d\n", port);

    if (debug_level & 2)
        fprintf(stderr,"*** net_init is returning a %d ***\n",ok);

    return(ok);
}





//**************************************************************
// This shuts down a network connection
//
//**************************************************************
int net_detach(int port) {
    int ok;
    int max;
    int stat;
    char quiti[2];

    if (debug_level & 2)
        fprintf(stderr,"Net detach Start, port %d\n",port);

    ok = -1;
    max = 0;

    if (begin_critical_section(&port_data_lock, "interface.c:net_detach(1)" ) > 0)
        fprintf(stderr,"net_detach():port_data_lock, Port = %d\n", port);

    if (port_data[port].active == DEVICE_IN_USE) {
        if (port_data[port].status == DEVICE_UP && port_data[port].device_type == DEVICE_NET_STREAM){

            if (debug_level & 2)
                fprintf(stderr,"net_detach():Found port %d up, shutting it down\n",port);

            quiti[0] = (char)4;
            quiti[1] = (char)0;
            if (port_data[port].status == DEVICE_UP) {
                port_write_string(port,quiti);
                usleep(100000); // 100ms
            }
            /* wait to write */
            while (port_data[port].status == DEVICE_UP && port_data[port].write_in_pos != port_data[port].write_out_pos && max < 25) {
                if (debug_level & 2)
                    fprintf(stderr,"net_detach():Waiting to finish writing data to port %d\n",port);

                //(void)sleep(1);
                usleep(100000);    // 100ms
                max++;
            }
        }
        /*
            Shut down and Close were separated but this would cause sockets to
            just float around
        */

        // It doesn't matter whether we _think_ the device is up.  It might
        // be in some other state, but the socket still needs to be closed.
        //if (port_data[port].status == DEVICE_UP) {

        /* we don't need to do a shut down on AX_25 devices */
       if ( (port_data[port].status == DEVICE_UP)
                && (port_data[port].device_type != DEVICE_AX25_TNC) ) {
            stat = shutdown(port_data[port].channel,2);
            if (debug_level & 2)
                fprintf(stderr,"net_detach():Net Shutdown Returned %d, port %d\n",stat,port);
        }

        usleep(100000); // 100ms
        // We wish to close down the socket (so both ends of the darn thing
        // go away), but we want to keep the number on those systems that
        // re-assign the same file descriptor again.  This is to prevent
        // cross-connects from one interface to another in Xastir (big pain!).

        // Close it
        stat = close(port_data[port].channel);
        if (debug_level & 2)
            fprintf(stderr,"net_detach():Net Close Returned %d, port %d\n",stat,port);

        usleep(100000); // 100ms

        // Snag a socket again.  We'll use it next time around.
        port_data[port].channel = socket(PF_INET, SOCK_STREAM, 0);

        ok = 1;
    }
    /* close down no matter what */
    port_data[port].status = DEVICE_DOWN;
    //usleep(300);
    port_data[port].active = DEVICE_NOT_IN_USE;

    if (end_critical_section(&port_data_lock, "interface.c:net_detach(2)" ) > 0)
        fprintf(stderr,"net_detach():port_data_lock, Port = %d\n", port);

    if (debug_level & 2)
        fprintf(stderr,"Net detach stop, port %d\n",port);

    return(ok);
}





//***************************** STOP NETWORK FUNCTIONS **********************************



// This routine changes callsign chars to proper uppercase chars or
// numerals, fixes the callsign to six bytes, shifts the letters left by
// one bit, and puts the SSID number into the proper bits in the seventh
// byte.  The callsign as processed is ready for inclusion in an
// AX.25 header.
//
void fix_up_callsign(unsigned char *data) {
    unsigned char new_call[8] = "       ";  // Start with seven spaces
    int ssid = 0;
    int i;
    int j = 0;
    int digipeated_flag = 0;


    // Check whether we've digipeated through this callsign yet.
    if (strstr(data,"*") != 0) {
         digipeated_flag++;
    }

    // Change callsign to upper-case and pad out to six places with
    // space characters.
    for (i = 0; i < (int)strlen(data); i++) {
        toupper(data[i]);

        if (data[i] == '-') {   // Stop at '-'
            break;
        }
        else if (data[i] == '*') {
        }
        else {
            new_call[j++] = data[i];
        }
    }
    new_call[7] = '\0';

    //fprintf(stderr,"new_call:(%s)\n",new_call);

    // Handle SSID.  'i' should now be pointing at a dash or at the
    // terminating zero character.
    if ( (i < (int)strlen(data)) && (data[i++] == '-') ) {   // We might have an SSID
        if (data[i] != '\0')
            ssid = atoi(&data[i]);
//            ssid = data[i++] - 0x30;    // Convert from ascii to int
//        if (data[i] != '\0')
//            ssid = (ssid * 10) + (data[i] - 0x30);
    }

//fprintf(stderr,"SSID:%d\t",ssid);

    if (ssid >= 0 && ssid <= 15) {
        new_call[6] = ssid | 0x30;  // Set 2 reserved bits
    }
    else {  // Whacko SSID.  Set it to zero
        new_call[6] = 0x30;     // Set 2 reserved bits
    }

    if (digipeated_flag) {
        new_call[6] = new_call[6] | 0x40; // Set the 'H' bit
    }
 
    // Shift each byte one bit to the left
    for (i = 0; i < 7; i++) {
        new_call[i] = new_call[i] << 1;
        new_call[i] = new_call[i] & 0xfe;
    }

//fprintf(stderr,"Last:%0x\n",new_call[6]);

    // Write over the top of the input string with the newly
    // formatted callsign
    strcpy(data,new_call);
}





//-------------------------------------------------------------------
// Had to snag code from port_write_string() below because our string
// needs to have 0x00 chars inside it.  port_write_string() can't
// handle that case.  It's a good thing the transmit queue stuff
// could handle it.
//-------------------------------------------------------------------
//
//WE7U
// Modify the other routines that needed binary output so that they
// use this routine.
//
void port_write_binary(int port, unsigned char *data, int length) {
    int ii,erd;
    int write_in_pos_hold;


//fprintf(stderr,"Sending to AGWPE:\n");

    erd = 0;

    if (begin_critical_section(&port_data[port].write_lock, "interface.c:port_write_binary(1)" ) > 0)
        fprintf(stderr,"write_lock, Port = %d\n", port);

    // Save the current position, just in case we have trouble
    write_in_pos_hold = port_data[port].write_in_pos;

    for (ii = 0; ii < length && !erd; ii++) {

//fprintf(stderr,"%02x ",data[ii]);

        // Put character into write buffer and advance pointer
        port_data[port].device_write_buffer[port_data[port].write_in_pos++] = data[ii];

        // Check whether we need to wrap back to the start of the
        // circular buffer
        if (port_data[port].write_in_pos >= MAX_DEVICE_BUFFER)
            port_data[port].write_in_pos = 0;

        // Check whether we just filled our buffer (read/write
        // pointers are equal).  If so, exit gracefully, dumping
        // this string and resetting the write pointer.
        if (port_data[port].write_in_pos == port_data[port].write_out_pos) {
            if (debug_level & 2)
                fprintf(stderr,"Port %d Buffer overrun\n",port);

            // Restore original write_in pos and dump this string
            port_data[port].write_in_pos = write_in_pos_hold;
            port_data[port].errors++;
            erd = 1;
        }
    }

// Check that the data got placed in the buffer ok
//for (ii = write_in_pos_hold;  ii < port_data[port].write_in_pos; ii++) {
//  fprintf(stderr,"%02x ",port_data[port].device_write_buffer[ii]);
//}
//fprintf(stderr,"\n");


    if (end_critical_section(&port_data[port].write_lock, "interface.c:port_write_binary(2)" ) > 0)
        fprintf(stderr,"write_lock, Port = %d\n", port);

//fprintf(stderr,"\n");

}





// Create an AX25 frame and then turn it into a KISS packet.  Dump
// it into the transmit queue.
//
void send_ax25_frame(int port, char *source, char *destination, char *path, char *data) {
    unsigned char temp_source[15];
    unsigned char temp_dest[15];
    unsigned char temp[15];
    unsigned char control[2], pid[2];
    unsigned char transmit_txt[MAX_LINE_SIZE*2];
    unsigned char transmit_txt2[MAX_LINE_SIZE*2];
    unsigned char c;
    int i, j;
    int erd;
    int write_in_pos_hold;

    //fprintf(stderr,"KISS String:%s>%s,%s:%s\n",source,destination,path,data);

    transmit_txt[0] = '\0';

    // Format the destination callsign
    strcpy(temp_dest,destination);
    fix_up_callsign(temp_dest);
    strcat(transmit_txt,temp_dest);

    // Format the source callsign
    strcpy(temp_source,source);
    fix_up_callsign(temp_source);
    strcat(transmit_txt,temp_source);

    // Break up the path into individual callsigns and send them one
    // by one to fix_up_callsign()
    j = 0;
    if ( (path != NULL) && (strlen(path) != 0) ) {
        while (path[j] != '\0') {
            i = 0;
            while ( (path[j] != ',') && (path[j] != '\0') ) {
                temp[i++] = path[j++];
            }
            temp[i] = '\0';

            if (path[j] == ',') {   // Skip over comma
                j++;
            }

            //fprintf(stderr,"%s\n",temp);

            fix_up_callsign(temp);
            strcat(transmit_txt,temp);
        }
    }

    // Set the end-of-address bit on the last callsign in the
    // address field
    transmit_txt[strlen(transmit_txt) - 1] |= 0x01;

    // Add the Control byte
    control[0] = 0x03;
    control[1] = '\0';
    strcat(transmit_txt,control);

    // Add the PID byte
    pid[0] = 0xf0;
    pid[1] = '\0';
    strcat(transmit_txt,pid);

    // Append the information chars
    strcat(transmit_txt,data);

    //fprintf(stderr,"%s\n",transmit_txt);

    // Add the KISS framing characters and do the proper escapes.
    j = 0;
    transmit_txt2[j++] = KISS_FEND;

    // Note:  This byte is where different interfaces would be
    // specified:
    transmit_txt2[j++] = 0x00;

    for (i = 0; i < (int)strlen(transmit_txt); i++) {
        c = transmit_txt[i];
        if (c == KISS_FEND) {
            transmit_txt2[j++] = KISS_FESC;
            transmit_txt2[j++] = KISS_TFEND;
        }
        else if (c == KISS_FESC) {
            transmit_txt2[j++] = KISS_FESC;
            transmit_txt2[j++] = KISS_TFESC;
        }
        else {
            transmit_txt2[j++] = c;
        }
    }
    transmit_txt2[j++] = KISS_FEND;
    transmit_txt2[j++] = '\0';  // Terminate the string


//-------------------------------------------------------------------
// Had to snag code from port_write_string() below because our string
// needs to have 0x00 chars inside it.  port_write_string() can't
// handle that case.  It's a good thing the transmit queue stuff
// could handle it.
//-------------------------------------------------------------------

    erd = 0;

    if (begin_critical_section(&port_data[port].write_lock, "interface.c:send_ax25_frame(1)" ) > 0)
        fprintf(stderr,"write_lock, Port = %d\n", port);

    write_in_pos_hold = port_data[port].write_in_pos;

    for (i = 0; i < j && !erd; i++) {
        port_data[port].device_write_buffer[port_data[port].write_in_pos++] = transmit_txt2[i];
        if (port_data[port].write_in_pos >= MAX_DEVICE_BUFFER)
            port_data[port].write_in_pos = 0;

        if (port_data[port].write_in_pos == port_data[port].write_out_pos) {
            if (debug_level & 2)
                fprintf(stderr,"Port %d Buffer overrun\n",port);

            /* clear this restore original write_in pos and dump this string */
            port_data[port].write_in_pos = write_in_pos_hold;
            port_data[port].errors++;
            erd = 1;
        }
    }

    if (end_critical_section(&port_data[port].write_lock, "interface.c:send_ax25_frame(2)" ) > 0)
        fprintf(stderr,"write_lock, Port = %d\n", port);
}





// Send a KISS configuration command to the selected port.
// The KISS spec allows up to 16 devices to be configured.  We
// support that here with the "device" input, which should be
// between 0 and 15.  The commands accepted are integer values:
//
// 0x01 TXDELAY
// 0x02 P-Persistence
// 0x03 SlotTime
// 0x04 TxTail
// 0x05 FullDuplex
// 0x06 SetHardware
// 0xff Exit from KISS mode (not implemented yet)
//
void send_kiss_config(int port, int device, int command, int value) {
    unsigned char transmit_txt[MAX_LINE_SIZE+1];
    int i, j;
    int erd;
    int write_in_pos_hold;


    if (device < 0 || device > 15) {
        fprintf(stderr,"send_kiss_config: out-of-range value for device\n");
        return;
    }

    if (command < 1 || command > 6) {
        fprintf(stderr,"send_kiss_config: out-of-range value for command\n");
        return;
    }

    if (value < 0 || value > 255) {
        fprintf(stderr,"send_kiss_config: out-of-range value for value\n");
        return;
    }

    // Add the KISS framing characters and do the proper escapes.
    j = 0;
    transmit_txt[j++] = KISS_FEND;

    transmit_txt[j++] = (device << 4) | (command & 0x0f);

    transmit_txt[j++] = value & 0xff;

    transmit_txt[j++] = KISS_FEND;
    transmit_txt[j++] = '\0';  // Terminate the string


//-------------------------------------------------------------------
// Had to snag code from port_write_string() below because our string
// needs to have 0x00 chars inside it.  port_write_string() can't
// handle that case.  It's a good thing the transmit queue stuff
// could handle it.
//-------------------------------------------------------------------

    erd = 0;

    if (begin_critical_section(&port_data[port].write_lock, "interface.c:send_kiss_config(1)" ) > 0)
        fprintf(stderr,"write_lock, Port = %d\n", port);

    write_in_pos_hold = port_data[port].write_in_pos;

    for (i = 0; i < j && !erd; i++) {
        port_data[port].device_write_buffer[port_data[port].write_in_pos++] = transmit_txt[i];
        if (port_data[port].write_in_pos >= MAX_DEVICE_BUFFER)
            port_data[port].write_in_pos = 0;

        if (port_data[port].write_in_pos == port_data[port].write_out_pos) {
            if (debug_level & 2)
                fprintf(stderr,"Port %d Buffer overrun\n",port);

            /* clear this restore original write_in pos and dump this string */
            port_data[port].write_in_pos = write_in_pos_hold;
            port_data[port].errors++;
            erd = 1;
        }
    }

    if (end_critical_section(&port_data[port].write_lock, "interface.c:send_kiss_config(2)" ) > 0)
        fprintf(stderr,"write_lock, Port = %d\n", port);
}





//***********************************************************
// port_write_string()
//
// port is port# used
// data is the string to write
//***********************************************************

void port_write_string(int port, char *data) {
    int i,erd;
    int write_in_pos_hold;

    if (data == NULL)
        return;

    if (data[0] == '\0')
        return;

    erd = 0;

    if (debug_level & 2)
        fprintf(stderr,"CMD:%s\n",data);

    if (begin_critical_section(&port_data[port].write_lock, "interface.c:port_write_string(1)" ) > 0)
        fprintf(stderr,"write_lock, Port = %d\n", port);

    write_in_pos_hold = port_data[port].write_in_pos;

    // Normal Serial/Net output?
    if (port_data[port].device_type != DEVICE_AX25_TNC) {
        for (i = 0; i < (int)strlen(data) && !erd; i++) {
            port_data[port].device_write_buffer[port_data[port].write_in_pos++] = data[i];
            if (port_data[port].write_in_pos >= MAX_DEVICE_BUFFER)
                port_data[port].write_in_pos = 0;

            if (port_data[port].write_in_pos == port_data[port].write_out_pos){
                if (debug_level & 2)
                    fprintf(stderr,"Port %d Buffer overrun\n",port);

                /* clear this restore original write_in pos and dump this string */
                port_data[port].write_in_pos = write_in_pos_hold;
                port_data[port].errors++;
                erd = 1;
            }
        }
    }

    // AX.25 port output
    else {
        port_data[port].bytes_output += strlen(data);
        data_out_ax25(port,(unsigned char *)data);
        /* do for interface indicators */
        if (port_data[port].write_in_pos >= MAX_DEVICE_BUFFER)
            port_data[port].write_in_pos = 0;
    }

    if (end_critical_section(&port_data[port].write_lock, "interface.c:port_write_string(2)" ) > 0)
        fprintf(stderr,"write_lock, Port = %d\n", port);
}





//***********************************************************
// port_read()
//
// port is port# used
//
// This function becomes the long-running thread that snags
// characters from an interface and passes them off to the
// decoding routines.  One copy of this is run for each read
// thread for each interface.
//***********************************************************

void port_read(int port) {
    unsigned char cin, last;
    unsigned char buffer[MAX_DEVICE_BUFFER];    // Only used for AX.25 packets
    int i;
    struct timeval tmv;
    fd_set rd;
    int group;
    int max;
    /*
    * Some local variables used for checking AX.25 data - PE1DNN
    *
    * "from"     is used to look up where the data comes from
    * "from_len" is used to keep the size of sockaddr structure
    * "dev"      is used to keep the name of the interface that
    *            belongs to our port/device_name
    */
    struct sockaddr from;
    socklen_t from_len;

#ifdef HAVE_LIBAX25
    char           *dev;
#endif /* USE_AX25 */

    if (debug_level & 2)
        fprintf(stderr,"Port %d read start\n",port);

//    init_critical_section(&port_data[port].read_lock);

    group = 0;
    max = MAX_DEVICE_BUFFER - 1;
    cin = (unsigned char)0;
    last = (unsigned char)0;

    // We stay in this read loop until the port is shut down
    while(port_data[port].active == DEVICE_IN_USE){

        if (port_data[port].status == DEVICE_UP){

            port_data[port].read_in_pos = 0;
            port_data[port].scan = 1;

            while (port_data[port].scan
                    && (port_data[port].read_in_pos < (MAX_DEVICE_BUFFER - 1) )
                    && (port_data[port].status == DEVICE_UP) ) {

                int skip = 0;

//                pthread_testcancel();   // Check for thread termination request
 
                // Handle all EXCEPT AX25_TNC interfaces here
                if (port_data[port].device_type != DEVICE_AX25_TNC) {
                    // Get one character
                    port_data[port].scan = (int)read(port_data[port].channel,&cin,1);
//fprintf(stderr," in:%x ",cin);
                }

                else {  // Handle AX25_TNC interfaces
                    /*
                    * Use recvfrom on a network socket to know from
                    * which interface the packet came - PE1DNN
                    */

#ifdef __solaris__
                    from_len = (unsigned int)sizeof(from);
#else   // __solaris__
  #ifndef socklen_t
                    from_len = (int)sizeof(from);
  #else // socklen_t
                    from_len = (socklen_t)sizeof(from);
  #endif    // socklen_t
#endif  // __solaris__

                    port_data[port].scan = recvfrom(port_data[port].channel,buffer,
                        sizeof(buffer) - 1,
                        0,
                        &from,
                        &from_len);
                }


                // Below is code for ALL types of interfaces
                if (port_data[port].scan > 0 && port_data[port].status == DEVICE_UP ) {

                    if (port_data[port].device_type != DEVICE_AX25_TNC)
                        port_data[port].bytes_input += port_data[port].scan;      // Add character to read buffer


// Somewhere between these lock statements the read_lock got unlocked.  How?
// if (begin_critical_section(&port_data[port].read_lock, "interface.c:port_read(1)" ) > 0)
//    fprintf(stderr,"read_lock, Port = %d\n", port);


                    // Handle all EXCEPT AX25_TNC interfaces here
                    if (port_data[port].device_type != DEVICE_AX25_TNC){


                        // Do special KISS packet processing here.
                        // We save
                        // the last character in
                        // port_data[port].channel2,
                        // as it is otherwise only used for AX.25
                        // ports.

                        if (port_data[port].device_type == DEVICE_SERIAL_KISS_TNC) {

                            if (port_data[port].channel2 == KISS_FESC) { // Frame Escape char
                                if (cin == KISS_TFEND) { // Transposed Frame End char

                                    // Save this char for next time
                                    // around
                                    port_data[port].channel2 = cin;

                                    cin = KISS_FEND;
                                }
                                else if (cin == KISS_TFESC) { // Transposed Frame Escape char

                                    // Save this char for next time
                                    // around
                                    port_data[port].channel2 = cin;

                                    cin = KISS_FESC;
                                }
                                else {
                                    port_data[port].channel2 = cin;
                                }
                            }
                            else if (port_data[port].channel2 == KISS_FEND) { // Frame End char
                                // Frame start or frame end.  Drop
                                // the next character which should
                                // either be another frame end or a
                                // type byte.

// Note this "type" byte is where it specifies which KISS interface
// the packet came from.  We may want to use this later for
// multi-drop KISS or other types of KISS protocols.

                                // Save this char for next time
                                // around
                                port_data[port].channel2 = cin;
                                skip++;
                            }
                            else if (cin == KISS_FESC) { // Frame Escape char
                                port_data[port].channel2 = cin;
                                skip++;
                            }
                            else {
                                port_data[port].channel2 = cin;
                            }
                        }   // End of first special KISS processing





// AGWPE
// Process AGWPE packets here.  Massage the frames so that they look
// like normal serial packets to the Xastir decoding functions?
//
// We turn on monitoring of packets when we first connect.  We now
// need to throw away all but the "U" packets, which are unconnected
// information packets.
//
// Check for enough bytes to complete a header (36 bytes).  If
// enough, check the datalength to see if an entire packet has been
// read.  If so, run that packet through a conversion routine to
// convert it to a TAPR2-style packet.
//
// Right now we're not taking into account multiple radio ports that
// AGWPE is capable of.  Just assume that we'll receive from all
// radio ports, but transmit out port 0.
//
                        if (port_data[port].device_type == DEVICE_NET_AGWPE) {
                            int bytes_available = 0;
                            long frame_length = 0;


                            skip = 1;   // Keeps next block of code from
                                        // trying to process this data.

                            // Add it to the buffer
                            if (port_data[port].read_in_pos < (MAX_DEVICE_BUFFER - 1) ) {
                                port_data[port].device_read_buffer[port_data[port].read_in_pos] = (char)cin;
                                port_data[port].read_in_pos++;
                                port_data[port].device_read_buffer[port_data[port].read_in_pos] = (char)0;
                            } else {
                                if (debug_level & 2)
                                    fprintf(stderr,"Port read overrun (1) on %d\n",port);

                                port_data[port].read_in_pos = 0;
                            }
 
                            bytes_available = port_data[port].read_in_pos - port_data[port].read_out_pos;
                            if (bytes_available < 0)
                                bytes_available = (bytes_available + MAX_DEVICE_BUFFER) % MAX_DEVICE_BUFFER;

//fprintf(stderr," bytes_avail:%d ",bytes_available);
 
                            if (bytes_available >= 36) {
                                // We have a full AGWPE header,
                                // which means we can compute the
                                // frame length.
                                unsigned char count[4];
                                int my_pointer;

                                // Snag bytes 28-32 of the buffer and compute frame_length
                                my_pointer = (port_data[port].read_out_pos + 28) % MAX_DEVICE_BUFFER;
                                count[0] = (unsigned char)port_data[port].device_read_buffer[my_pointer];
                                my_pointer = (my_pointer + 1) % MAX_DEVICE_BUFFER;
                                count[1] = (unsigned char)port_data[port].device_read_buffer[my_pointer];
                                my_pointer = (my_pointer + 1) % MAX_DEVICE_BUFFER;
                                count[2] = (unsigned char)port_data[port].device_read_buffer[my_pointer];
                                my_pointer = (my_pointer + 1) % MAX_DEVICE_BUFFER;
                                count[3] = (unsigned char)port_data[port].device_read_buffer[my_pointer];

                                frame_length = 0;
                                frame_length = frame_length | (count[0]      );
                                frame_length = frame_length | (count[1] <<  8);
                                frame_length = frame_length | (count[2] << 16);
                                frame_length = frame_length | (count[3] << 24);

//fprintf(stderr,"Found complete AGWPE header: DataLength: %d\n",frame_length);

                               // Have a complete AGWPE packet?  If
                                // so, convert it to a more standard
                                // packet format then feed it to our
                                // decoding routines.
                                //
                                if (bytes_available >= (frame_length + 36)) {
                                    char input_string[MAX_DEVICE_BUFFER];
                                    char output_string[MAX_DEVICE_BUFFER];
                                    int ii,jj,new_length;

//fprintf(stderr,"Found complete AGWPE packet, %d bytes total in frame:\n",frame_length + 36);

                                    my_pointer = port_data[port].read_out_pos;
                                    jj = 0;
                                    for (ii = 0; ii < frame_length + 36; ii++) {
                                        input_string[jj++] = (unsigned char)port_data[port].device_read_buffer[my_pointer];
                                        my_pointer = (my_pointer + 1) % MAX_DEVICE_BUFFER;
                                    }
                                    my_pointer = port_data[port].read_out_pos;

                                    if ( parse_agwpe_packet(input_string, frame_length+36, output_string, &new_length) ) {
                                        channel_data(port, output_string, new_length);
                                    }

                                    for (i = 0; i <= port_data[port].read_in_pos; i++)
                                        port_data[port].device_read_buffer[i] = (char)0;

                                    port_data[port].read_in_pos = 0;
                                }
                            }
                            else {
                                // Not enough for a full header so
                                // we can't compute frame length
                                // yet.  Do nothing until we have
                                // more data.
                            }
                        }
// End of new AGWPE code



                        // We shouldn't see any AX.25 flag
                        // characters on a KISS interface because
                        // they are stripped out by the KISS code.
                        // What we should see though are KISS_FEND
                        // characters at the beginning of each
                        // packet.  These characters are where we
                        // should break the data apart in order to
                        // send strings to the decode routines.  It
                        // may be just fine to still break it on \r
                        // or \n chars, as the KISS_FEND should
                        // appear immediately afterwards in
                        // properly formed packets.


                        if ( (!skip)
                                && (cin == (unsigned char)'\r'
                                    || cin == (unsigned char)'\n'
                                    || port_data[port].read_in_pos >= (MAX_DEVICE_BUFFER - 1)
                                    || ( (cin == KISS_FEND) && (port_data[port].device_type == DEVICE_SERIAL_KISS_TNC) ) )
                               && port_data[port].data_type == 0) {     // If end-of-line

// End serial/net type data send it to the decoder Put a terminating
// zero at the end of the read-in data

                            port_data[port].device_read_buffer[port_data[port].read_in_pos] = (char)0;

                            if (port_data[port].status == DEVICE_UP && port_data[port].read_in_pos > 0) {
                                int length;

                                // Compute length of string in
                                // circular queue

                                //fprintf(stderr,"%d\t%d\n",port_data[port].read_in_pos,port_data[port].read_out_pos);

                                // KISS TNC sends binary data
                                if (port_data[port].device_type == DEVICE_SERIAL_KISS_TNC) {
                                    length = port_data[port].read_in_pos - port_data[port].read_out_pos;
                                    if (length < 0)
                                        length = (length + MAX_DEVICE_BUFFER) % MAX_DEVICE_BUFFER;
                                }
                                else {  // ASCII data
                                    length = 0;
                                }

                                channel_data(port,
                                    (unsigned char *)port_data[port].device_read_buffer,
                                    length);   // Length of string
                            }

                            for (i = 0; i <= port_data[port].read_in_pos; i++)
                                port_data[port].device_read_buffer[i] = (char)0;

                            port_data[port].read_in_pos = 0;
                        }
                        else if (!skip) {

                            // Check for binary WX station data
                            if (port_data[port].data_type == 1 && (port_data[port].device_type == DEVICE_NET_WX ||
                                    port_data[port].device_type == DEVICE_SERIAL_WX)) {

                                /* BINARY DATA input (WX data ?) */
                                /* check RS WX200 */
                                switch (cin) {

                                    case 0x8f:
                                    case 0x9f:
                                    case 0xaf:
                                    case 0xbf:
                                    case 0xcf:

                                        if (group == 0) {
                                            port_data[port].read_in_pos = 0;
                                            group = (int)cin;
                                            switch (cin) {

                                                case 0x8f:
                                                    max = 35;
                                                    break;

                                                case 0x9f:
                                                    max = 34;
                                                    break;

                                                case 0xaf:
                                                    max = 31;
                                                    break;

                                                case 0xbf:
                                                    max = 14;
                                                    break;

                                                case 0xcf:
                                                    max = 27;
                                                    break;

                                                default:
                                                    break;
                                            }
                                        }
                                        break;

                                    default:
                                        break;
                                }
                                if (port_data[port].read_in_pos < (MAX_DEVICE_BUFFER - 1) ) {
                                    port_data[port].device_read_buffer[port_data[port].read_in_pos] = (char)cin;
                                    port_data[port].read_in_pos++;
                                    port_data[port].device_read_buffer[port_data[port].read_in_pos] = (char)0;
                                } else {
                                    if (debug_level & 2)
                                        fprintf(stderr,"Port read overrun (1) on %d\n",port);

                                    port_data[port].read_in_pos = 0;
                                }
                                if (port_data[port].read_in_pos >= max) {
                                    if (group != 0) {   /* ok try to decode it */

                                        channel_data(port,
                                            (unsigned char *)port_data[port].device_read_buffer,
                                            0);
                                    }
                                    max = MAX_DEVICE_BUFFER - 1;
                                    group = 0;

                                    port_data[port].read_in_pos = 0;
                                }
                            }
                            else { /* Normal Data input */

                                if (cin == '\0')    // OWW WX daemon sends 0x00's!
                                    cin = '\n';

                                if (port_data[port].read_in_pos < (MAX_DEVICE_BUFFER - 1) ) {
                                    port_data[port].device_read_buffer[port_data[port].read_in_pos] = (char)cin;
                                    port_data[port].read_in_pos++;
                                    port_data[port].device_read_buffer[port_data[port].read_in_pos] = (char)0;
                                }
                                else {
                                    if (debug_level & 2)
                                        fprintf(stderr,"Port read overrun (2) on %d\n",port);

                                    port_data[port].read_in_pos = 0;
                                }
                            }
                        }

                        // Ascii WX station data but no line-ends?
                        if (port_data[port].read_in_pos > MAX_DEVICE_BUFFER_UNTIL_BINARY_SWITCH &&
                                (port_data[port].device_type == DEVICE_NET_WX
                                || port_data[port].device_type == DEVICE_SERIAL_WX)) {

                            /* normal data on WX not found do look at data for binary WX */
                            port_data[port].data_type++;
                            port_data[port].data_type &= 1;
                            port_data[port].read_in_pos = 0;
                        }
                    }   // End of non-AX.25 interface code block


                    else {    // Process ax25 interface data and send to the decoder
                        /*
                         * Only accept data from our own interface (recvfrom will get
                         * data from all AX.25 interfaces!) - PE1DNN
                         */
#ifdef HAVE_LIBAX25
                        if (port_data[port].device_name != NULL) {
                            if ((dev = ax25_config_get_dev(port_data[port].device_name)) != NULL) {
                                /* if the data is not from our interface, ignore it! PE1DNN */
                                if(strcmp(dev, from.sa_data) == 0) {
                                    /* Received data from our interface! - process data */
                                    if (process_ax25_packet(buffer, port_data[port].scan, port_data[port].device_read_buffer) != NULL) {
                                        port_data[port].bytes_input += strlen(port_data[port].device_read_buffer);

                                        channel_data(port,
                                            (unsigned char *)port_data[port].device_read_buffer,
                                             0);
                                    }
                                    /*
                                        do this for interface indicator in this case we only do it for,
                                        data from the correct AX.25 port
                                    */
                                    if (port_data[port].read_in_pos < (MAX_DEVICE_BUFFER - 1) ) {
                                        port_data[port].read_in_pos += port_data[port].scan;
                                    } else {

                                       /* no buffer over runs writing a line at a time */
                                        port_data[port].read_in_pos = 0;
                                    }
                                }
                            }
                        }
#endif /* HAVE_LIBAX25 */
                    }   // End of AX.25 interface code block


//if (end_critical_section(&port_data[port].read_lock, "interface.c:port_read(2)" ) > 0)
//    fprintf(stderr,"read_lock, Port = %d\n", port);

                }
                else if (port_data[port].status == DEVICE_UP) {    /* error or close on read */
                    port_data[port].errors++;
                    if (port_data[port].scan == 0) {
                        // Should not get this unless the device is down.  NOT TRUE!
                        // We seem to also be able to get here if we're closing/restarting
                        // another interface.  For that reason I commented out the below
                        // statement so that this interface won't go down.  The inactivity
                        // timer solves that issue now anyway.  --we7u.
                        port_data[port].status = DEVICE_ERROR;
                        if (debug_level & 2)
                            fprintf(stderr,"end of file on read, or signal interrupted the read, port %d\n",port);
                    } else {
                        if (port_data[port].scan == -1) {
                            /* Should only get this if an real error occurs */
                            port_data[port].status = DEVICE_ERROR;
                            if (debug_level & 2) {
                                fprintf(stderr,"error on read with error no %d, or signal interrupted the read, port %d, DEVICE_ERROR ***\n",
                                    errno,port);
                                switch (errno) {

                                    case EINTR:
                                        fprintf(stderr,"EINTR ERROR\n");
                                        break;

                                    case EAGAIN:
                                        fprintf(stderr,"EAGAIN ERROR\n");
                                        break;

                                    case EIO:
                                        fprintf(stderr,"EIO ERROR\n");
                                        break;

                                    case EISDIR:
                                        fprintf(stderr,"EISDIR ERROR\n");
                                        break;

                                    case EBADF: // Get this one when we terminate nearby threads
                                        fprintf(stderr,"EBADF ERROR\n");
                                        break;

                                    case EINVAL:
                                        fprintf(stderr,"EINVAL ERROR\n");
                                        break;

                                    case EFAULT:
                                        fprintf(stderr,"EFAULT ERROR\n");
                                        break;

                                    default:
                                        fprintf(stderr,"OTHER ERROR\n");
                                        break;
                                }
                            }
                        }
                    }
                }
            }
        }
        if (port_data[port].active == DEVICE_IN_USE)  {

            // We need to delay here so that the thread doesn't use
            // high amounts of CPU doing nothing.

// This select that waits on data and a timeout, so that if data
// doesn't come in within a certain period of time, we wake up to
// check whether the socket has gone down.  Else, we go back into
// the select to wait for more data or a timeout.

//sched_yield();  // Yield to other threads

            // Set up the select to block until data ready or 200ms
            // timeout, whichever occurs first.
            FD_ZERO(&rd);
            FD_SET(port_data[port].channel, &rd);
            tmv.tv_sec = 0;
            tmv.tv_usec = 500000;    // 500 ms
            (void)select(0,&rd,NULL,NULL,&tmv);
        }
    }

    if (debug_level & 2)
        fprintf(stderr,"Thread for port %d read down!\n",port);
}





//***********************************************************
// port_write()
//
// port is port# used
//
// This function becomes the long-running thread that sends
// characters to an interface.  One copy of this is run for
// each write thread for each interface.
//***********************************************************
void port_write(int port) {
    int retval;
    struct timeval tmv;
    fd_set wd;
    int wait_max;
    unsigned long bytes_input;
    char write_buffer[MAX_DEVICE_BUFFER];
    int quantity;


    if (debug_level & 2)
        fprintf(stderr,"Port %d write start\n",port);

    init_critical_section(&port_data[port].write_lock);

    while(port_data[port].active == DEVICE_IN_USE) {

        if (port_data[port].status == DEVICE_UP) {
            // Some messiness necessary because we're using
            // xastir_mutex's instead of pthread_mutex_t's.
            pthread_mutex_t *cleanup_mutex;


            // Install the cleanup routine for the case where this
            // thread gets killed while the mutex is locked.  The
            // cleanup routine initiates an unlock before the thread
            // dies.  We must be in deferred cancellation mode for
            // the thread to have this work properly.  We must first
            // get the pthread_mutex_t address:
            cleanup_mutex = &port_data[port].write_lock.lock;

            // Then install the cleanup routine:
            pthread_cleanup_push((void *)pthread_mutex_unlock, (void *)cleanup_mutex);


            if (begin_critical_section(&port_data[port].write_lock, "interface.c:port_write(1)" ) > 0)
                fprintf(stderr,"write_lock, Port = %d\n", port);

            if ( (port_data[port].write_in_pos != port_data[port].write_out_pos)
                    && port_data[port].status == DEVICE_UP) {
                // We have something in the buffer to transmit!


// Handle control-C delay
                switch (port_data[port].device_type) {

                    // Use this block for serial interfaces where we
                    // need special delays for control-C character
                    // processing in the TNC.
                    case DEVICE_SERIAL_TNC_HSP_GPS:
                    case DEVICE_SERIAL_TNC_AUX_GPS:
                    case DEVICE_SERIAL_TNC:

                        // Are we trying to send a control-C?  If so, wait a
                        // special amount of time _before_ we send
                        // it out the serial port.
                        if (port_data[port].device_write_buffer[port_data[port].write_out_pos] == (char)0x03) {
                            // Sending control-C.

                            if (debug_level & 128) {
                                fprintf(stderr,"Writing command [%x] on port %d, at pos %d\n",
                                    *(port_data[port].device_write_buffer + 
                                    port_data[port].write_out_pos),
                                    port, port_data[port].write_out_pos);
                            }

                            wait_max = 0;
                            bytes_input = port_data[port].bytes_input + 40;
                            while ( (port_data[port].bytes_input != bytes_input)
                                    && (port_data[port].status == DEVICE_UP)
                                    && (wait_max < 100) ) {
                                bytes_input = port_data[port].bytes_input;
                                /*sleep(1);*/

                                /*wait*/
                                FD_ZERO(&wd);
                                FD_SET(port_data[port].channel, &wd);
                                tmv.tv_sec = 0;
                                tmv.tv_usec = 80000l;   // Delay 80ms
                                (void)select(0,NULL,&wd,NULL,&tmv);
                                wait_max++;
                                /*fprintf(stderr,"Bytes in %ld %ld\n",bytes_input,port_data[port].bytes_input);*/
                            }
                            /*fprintf(stderr,"Wait_max %d\n",wait_max);*/
                        }   // End of command byte wait
                        break;

                    // Use this block for all other interfaces.
                    default:
                        // Do nothing (no delays for control-C's)
                        break;

                }   // End of switch
// End of control-C delay code

 
                pthread_testcancel();   // Check for thread termination request


// Handle method of sending data (1 or multiple chars per TX)
                switch (port_data[port].device_type) {

                    // Use this block for serial interfaces where we
                    // need character pacing and so must send one
                    // character per write.
                    case DEVICE_SERIAL_TNC_HSP_GPS:
                    case DEVICE_SERIAL_TNC_AUX_GPS:
                    case DEVICE_SERIAL_KISS_TNC:
                    case DEVICE_SERIAL_TNC:
                    case DEVICE_SERIAL_GPS:
                    case DEVICE_SERIAL_WX:
                        // Do the actual write here, one character
                        // at a time for these types of interfaces.

                        retval = (int)write(port_data[port].channel,
                            &port_data[port].device_write_buffer[port_data[port].write_out_pos],
                            1);

//fprintf(stderr,"%02x ", (unsigned char)port_data[port].device_write_buffer[port_data[port].write_out_pos]);

                        pthread_testcancel();   // Check for thread termination request

                        if (retval == 1) {  // We succeeded in writing one byte

                            port_data[port].bytes_output++;

                            port_data[port].write_out_pos++;
                            if (port_data[port].write_out_pos >= MAX_DEVICE_BUFFER)
                                port_data[port].write_out_pos = 0;

                        }  else {
                            /* error of some kind */
                            port_data[port].errors++;
                            port_data[port].status = DEVICE_ERROR;
                            if (retval == 0) {
                                /* Should not get this unless the device is down */
                                if (debug_level & 2)
                                    fprintf(stderr,"no data written %d, DEVICE_ERROR ***\n",port);
                            } else {
                                if (retval == -1) {
                                    /* Should only get this if an real error occurs */
                                    if (debug_level & 2)
                                        fprintf(stderr,"error on write with error no %d, or port %d\n",errno,port);
                                }
                            }
                        }
//fprintf(stderr,"Char pacing ");
//                        usleep(25000); // character pacing, 25ms per char.  20ms doesn't work for PicoPacket.
                        if (serial_char_pacing > 0) {
                            // Character pacing.  Delay in between
                            // each character in milliseconds.
                            // Convert to microseconds for this
                            // usleep() call .
                          usleep(serial_char_pacing * 1000);
                        }
                        break;

                    // Use this block for all other interfaces where
                    // we don't need character pacing and we can
                    // send blocks of data in one write.
                    default:
                        // Do the actual write here, one buffer's
                        // worth at a time.

                        // Copy the data to a linear write buffer so
                        // that we can send it all in one shot.

// Need to handle the case where only a portion of the data was
// written by the write() function.  Perhaps just write out an error
// message?
                        quantity = 0;
                        while (port_data[port].write_in_pos != port_data[port].write_out_pos) {

                            write_buffer[quantity] = port_data[port].device_write_buffer[port_data[port].write_out_pos];

                            port_data[port].write_out_pos++;
                            if (port_data[port].write_out_pos >= MAX_DEVICE_BUFFER)
                                port_data[port].write_out_pos = 0;

//fprintf(stderr,"%02x ",(unsigned char)write_buffer[quantity]);

                            quantity++;
                        }

//fprintf(stderr,"\nWriting %d bytes\n\n", quantity);

                        retval = (int)write(port_data[port].channel,
                            write_buffer,
                            quantity);

//fprintf(stderr,"%02x ", (unsigned char)port_data[port].device_write_buffer[port_data[port].write_out_pos]);

                        pthread_testcancel();   // Check for thread termination request

                        if (retval == quantity) {  // We succeeded in writing one byte
                            port_data[port].bytes_output++;
                        }  else {
                            /* error of some kind */
                            port_data[port].errors++;
                            port_data[port].status = DEVICE_ERROR;
                            if (retval == 0) {
                                /* Should not get this unless the device is down */
                                if (debug_level & 2)
                                    fprintf(stderr,"no data written %d, DEVICE_ERROR ***\n",port);
                            } else {
                                if (retval == -1) {
                                    /* Should only get this if an real error occurs */
                                    if (debug_level & 2)
                                        fprintf(stderr,"error on write with error no %d, or port %d\n",errno,port);
                                }
                            }
                        }
                        break;

                }   // End of switch
// End of handling method of sending data (1 or multiple char per TX)


            }

            if (end_critical_section(&port_data[port].write_lock, "interface.c:port_write(2)" ) > 0)
                fprintf(stderr,"write_lock, Port = %d\n", port);

            // Remove the cleanup routine for the case where this
            // thread gets killed while the mutex is locked.  The
            // cleanup routine initiates an unlock before the thread
            // dies.  We must be in deferred cancellation mode for
            // the thread to have this work properly.
            pthread_cleanup_pop(0);
 
        }

        if (port_data[port].active == DEVICE_IN_USE) {

            // Delay here so that the thread doesn't use high
            // amounts of CPU doing _nothing_.  Take this delay out
            // and the thread will take lots of CPU time.

// Try to change this to a select that waits on data and a timeout,
// so that if data doesn't come in within a certain period of time,
// we wake up to check whether the socket has gone down.  Else, we
// go back into the select to wait for more data or a timeout.

            FD_ZERO(&wd);
            FD_SET(port_data[port].channel, &wd);
            tmv.tv_sec = 0;
            tmv.tv_usec = 100;  // Delay 100us
            (void)select(0,NULL,&wd,NULL,&tmv);
        }
    }
    if (debug_level & 2)
        fprintf(stderr,"Thread for port %d write down!\n",port);
}





//***********************************************************
// read_access_port_thread()
//
// Port read thread.
// port is port# used
//
// open threads for reading data from this port.
//***********************************************************
static void* read_access_port_thread(void *arg) {
    int port;

    port = *((int *) arg);
    // This call means we don't care about the return code and won't
    // use pthread_join() later.  Makes threading more efficient.
    (void)pthread_detach(pthread_self());
    port_read(port);

    return(NULL);
}





//***********************************************************
// write_access_port_thread()
//
// Port write thread.
// port is port# used
//
// open threads for writing data to this port.
//***********************************************************
static void* write_access_port_thread(void *arg) {
    int port;

    port = *((int *) arg);
    // This call means we don't care about the return code and won't
    // use pthread_join() later.  Makes threading more efficient.
    (void)pthread_detach(pthread_self());
    port_write(port);

    return(NULL);
}





//***********************************************************
// Start port read & write threads
// port is port# used
//
// open threads for reading and writing data to and from this
// port.
//***********************************************************
int start_port_threads(int port) {
    int ok;

    port_id[port] = port;
    if (debug_level & 2)
        fprintf(stderr,"Start port %d threads\n",port);

    ok = 1;
    if (port_data[port].active == DEVICE_IN_USE && port_data[port].status == DEVICE_UP){
        if (debug_level & 2)
            fprintf(stderr,"*** Startup of read/write threads for port %d ***\n",port);

        /* start the two threads */
        if (pthread_create(&port_data[port].read_thread, NULL, read_access_port_thread, &port_id[port])) {
            /* error starting read thread*/
            fprintf(stderr,"Error starting read thread, port %d\n",port);
            port_data[port].read_thread = 0;
            ok = -1;
        }
        else if (pthread_create(&port_data[port].write_thread, NULL, write_access_port_thread, &port_id[port])) {
                /* error starting write thread*/
                fprintf(stderr,"Error starting write thread, port %d\n",port);
                port_data[port].write_thread = 0;
                ok = -1;
        }

    }
    else if (debug_level & 2) {
        fprintf(stderr,"*** Skipping startup of read/write threads for port %d ***\n",port);
    }

    if (debug_level & 2)
        fprintf(stderr,"End port %d threads\n",port);

    return(ok);
}





//***********************************************************
// Clear Port Data
// int port to be cleared
//***********************************************************
void clear_port_data(int port, int clear_more) {

    if (begin_critical_section(&port_data_lock, "interface.c:clear_port_data(1)" ) > 0)
        fprintf(stderr,"port_data_lock, Port = %d\n", port);

    port_data[port].device_type = -1;
    port_data[port].active = DEVICE_NOT_IN_USE;
    port_data[port].status = DEVICE_DOWN;
    strcpy(port_data[port].device_name,"");
    strcpy(port_data[port].device_host_name,"");

    if (begin_critical_section(&connect_lock, "interface.c:clear_port_data(2)" ) > 0)
        fprintf(stderr,"connect_lock, Port = %d\n", port);

    port_data[port].thread_status = -1;
    port_data[port].connect_status = -1;
    port_data[port].read_thread = 0;
    port_data[port].write_thread = 0;

    if (end_critical_section(&connect_lock, "interface.c:clear_port_data(3)" ) > 0)
        fprintf(stderr,"connect_lock, Port = %d\n", port);

    port_data[port].decode_errors = 0;
    port_data[port].data_type = 0;
    port_data[port].socket_port = -1;
    strcpy(port_data[port].device_host_pswd,"");

    if (clear_more)
        port_data[port].channel = -1;

    port_data[port].channel2 = -1;
    strcpy(port_data[port].ui_call,"");
    port_data[port].dtr = 0;
    port_data[port].sp = -1;
    port_data[port].style = -1;
    port_data[port].errors = 0;
    port_data[port].bytes_input = 0l;
    port_data[port].bytes_output = 0l;
    port_data[port].bytes_input_last = 0l;
    port_data[port].bytes_output_last = 0l;
    port_data[port].port_activity = 1;    // First time-period is a freebie
    port_data[port].read_in_pos = 0;
    port_data[port].read_out_pos = 0;
    port_data[port].write_in_pos = 0;
    port_data[port].write_out_pos = 0;

    if (end_critical_section(&port_data_lock, "interface.c:clear_port_data(4)" ) > 0)
        fprintf(stderr,"port_data_lock, Port = %d\n", port);
}





//***********************************************************
// Clear All Port Data
//***********************************************************
void clear_all_port_data(void) {
    int i;

    for (i = 0; i < MAX_IFACE_DEVICES; i++)
        clear_port_data(i,1);
}





//***********************************************************
// INIT Device names Data
//***********************************************************
void init_device_names(void) {
    strcpy(dtype[DEVICE_NONE].device_name,langcode("IFDNL00000"));
    strcpy(dtype[DEVICE_SERIAL_TNC].device_name,langcode("IFDNL00001"));
    strcpy(dtype[DEVICE_SERIAL_TNC_HSP_GPS].device_name,langcode("IFDNL00002"));
    strcpy(dtype[DEVICE_SERIAL_GPS].device_name,langcode("IFDNL00003"));
    strcpy(dtype[DEVICE_SERIAL_WX].device_name,langcode("IFDNL00004"));
    strcpy(dtype[DEVICE_NET_STREAM].device_name,langcode("IFDNL00005"));
    strcpy(dtype[DEVICE_AX25_TNC].device_name,langcode("IFDNL00006"));
    strcpy(dtype[DEVICE_NET_GPSD].device_name,langcode("IFDNL00007"));
    strcpy(dtype[DEVICE_NET_WX].device_name,langcode("IFDNL00008"));
    strcpy(dtype[DEVICE_SERIAL_TNC_AUX_GPS].device_name,langcode("IFDNL00009"));
    strcpy(dtype[DEVICE_SERIAL_KISS_TNC].device_name,langcode("IFDNL00010"));
    strcpy(dtype[DEVICE_NET_DATABASE].device_name,langcode("IFDNL00011"));
    strcpy(dtype[DEVICE_NET_AGWPE].device_name,langcode("IFDNL00012"));
}





//***********************************************************
// Delete Device.  Shuts down active port/ports.
//***********************************************************
int del_device(int port) {
    int ok;
    char temp[300];

    if (debug_level & 2)
        fprintf(stderr,"Delete Device start\n");

    ok = -1;
    switch (port_data[port].device_type) {

        case(DEVICE_SERIAL_TNC):
        case(DEVICE_SERIAL_KISS_TNC):
        case(DEVICE_SERIAL_GPS):
        case(DEVICE_SERIAL_WX):
        case(DEVICE_SERIAL_TNC_HSP_GPS):
        case(DEVICE_SERIAL_TNC_AUX_GPS):

            switch (port_data[port].device_type){

                case DEVICE_SERIAL_TNC:

                    if (debug_level & 2)
                        fprintf(stderr,"Close a Serial TNC device\n");

begin_critical_section(&devices_lock, "interface.c:del_device" );

                    xastir_snprintf(temp, sizeof(temp), "config/%s", devices[port].tnc_down_file);

end_critical_section(&devices_lock, "interface.c:del_device" );

                    (void)command_file_to_tnc_port(port,get_data_base_dir(temp));
                    usleep(2000000);    // 2secs
                    break;

                case DEVICE_SERIAL_KISS_TNC:
                    if (debug_level & 2)
                        fprintf(stderr,"Close a Serial KISS TNC device\n");
                        break;


                case DEVICE_SERIAL_GPS:
                    if (debug_level & 2)
                        fprintf(stderr,"Close a Serial GPS device\n");
                        if (using_gps_position) {
                            using_gps_position--;
                        }
                        break;

                case DEVICE_SERIAL_WX:
                    if (debug_level & 2)
                        fprintf(stderr,"Close a Serial WX device\n");

                    break;

                case DEVICE_SERIAL_TNC_HSP_GPS:
                    if (debug_level & 2)
                        fprintf(stderr,"Close a Serial TNC w/HSP GPS\n");
                        if (using_gps_position) {
                            using_gps_position--;
                        }

begin_critical_section(&devices_lock, "interface.c:del_device" );

                    xastir_snprintf(temp, sizeof(temp), "config/%s", devices[port].tnc_down_file);

end_critical_section(&devices_lock, "interface.c:del_device" );

                    (void)command_file_to_tnc_port(port,get_data_base_dir(temp));
                    usleep(2000000);    // 2secs
                    break;

                case DEVICE_SERIAL_TNC_AUX_GPS:
                    if (debug_level & 2)
                        fprintf(stderr,"Close a Serial TNC w/AUX GPS\n");
                        if (using_gps_position) {
                            using_gps_position--;
                        }

begin_critical_section(&devices_lock, "interface.c:del_device");

                    sprintf(temp, "config/%s", devices[port].tnc_down_file);

end_critical_section(&devices_lock, "interface.c:del_device");

                    (void)command_file_to_tnc_port(port,
                        get_data_base_dir(temp));
                    usleep(2000000);    // 2secs
                    break;

                default:
                    break;
            }
            if (debug_level & 2)
                fprintf(stderr,"Serial detach\n");

            ok = serial_detach(port);
            break;

        case(DEVICE_NET_STREAM):
        case(DEVICE_AX25_TNC):
        case(DEVICE_NET_GPSD):
        case(DEVICE_NET_WX):
        case(DEVICE_NET_DATABASE):
        case(DEVICE_NET_AGWPE):

            switch (port_data[port].device_type){

                case DEVICE_NET_STREAM:
                    if (debug_level & 2)
                        fprintf(stderr,"Close a Network stream\n");
                    break;

                case DEVICE_AX25_TNC:
                    if (debug_level & 2)
                        fprintf(stderr,"Close a AX25 TNC device\n");
                    break;

                case DEVICE_NET_GPSD:
                    if (debug_level & 2)
                        fprintf(stderr,"Close a Network GPSd stream\n");
                        if (using_gps_position) {
                            using_gps_position--;
                        }
                    break;

                case DEVICE_NET_WX:
                    if (debug_level & 2)
                        fprintf(stderr,"Close a Network WX stream\n");
                    break;

                case DEVICE_NET_DATABASE:
                    if (debug_level & 2)
                        fprintf(stderr,"Close a Network Database stream\n");
                    break;

                case DEVICE_NET_AGWPE:
                    if (debug_level & 2)
                        fprintf(stderr,"Close a Network AGWPE stream\n");
                    break;

                default:
                    break;
            }
            if (debug_level & 2)
                fprintf(stderr,"Net detach\n");

            ok = net_detach(port);
            break;

        default:
            break;
    }

    if (ok) {
        int retvalue;

        if (debug_level & 2)
            fprintf(stderr,"port detach OK\n");

        usleep(100000);    // 100ms
        if (debug_level & 2)
            fprintf(stderr,"Cancel threads\n");

        if (begin_critical_section(&port_data_lock, "interface.c:del_device(1)" ) > 0)
            fprintf(stderr,"port_data_lock, Port = %d\n", port);

        if (begin_critical_section(&connect_lock, "interface.c:del_device(2)" ) > 0)
            fprintf(stderr,"connect_lock, Port = %d\n", port);

        if (port_data[port].read_thread != 0) { // If we have a thread defined
            retvalue = pthread_cancel(port_data[port].read_thread);
            if (retvalue == ESRCH) {
                //fprintf(stderr,"ERROR: Could not cancel read thread on port %d\n", port);
                //fprintf(stderr,"No thread found with that thread ID\n");
            }
        }

        if (port_data[port].write_thread != 0) {    // If we have a thread defined
            retvalue = pthread_cancel(port_data[port].write_thread);
            if (retvalue == ESRCH) {
                //fprintf(stderr,"ERROR: Could not cancel write thread on port %d\n", port);
                //fprintf(stderr,"No thread found with that thread ID\n");
            }
        }

        if (end_critical_section(&connect_lock, "interface.c:del_device(3)" ) > 0)
            fprintf(stderr,"connect_lock, Port = %d\n", port);

        if (end_critical_section(&port_data_lock, "interface.c:del_device(4)" ) > 0)
            fprintf(stderr,"port_data_lock, Port = %d\n", port);

        usleep(100000); // 100ms
    } else {
        if (debug_level & 2)
            fprintf(stderr,"Port %d could not be closed\n",port);
    }
    usleep(10);

    // Cover the case where someone plays with a GPS interface or
    // three and then turns it/them off again: They won't send a
    // posit again until the next restart or whenever they enable a
    // GPS interface again that has good data, unless we set this
    // variable again for them.
    if (!using_gps_position) {
        my_position_valid = 1;
    }

    return(ok);
}





//***********************************************************
// Add Device.  Starts up ports (makes them active).
// dev_type is the device type to add
// dev_num is the device name
// dev_hst is the host name to connect to (network only)
// dev_sck_p is the socket port to connect to (network only)
// dev_sp is the baud rate of the port (serial only)
// dev_sty is the port style (serial only)
//
// this will return the port # if one is available
// otherwise it will return -1 if there is an error
//***********************************************************
int add_device(int port_avail,int dev_type,char *dev_nm,char *passwd,int dev_sck_p,
        int dev_sp,int dev_sty,int reconnect, char *filter_string) {
    char logon_txt[600];
    int ok;
    char temp[300];
    char verstr[15];

    if ( (dev_nm == NULL) || (passwd == NULL) )
        return(-1);

    if (dev_nm[0] == '\0')
        return(-1);

    strcpy(verstr, "XASTIR ");
    strcat(verstr, VERSION);
    ok = -1;
    if (port_avail >= 0){
        if (debug_level & 2)
            fprintf(stderr,"Port Available %d\n",port_avail);

        switch(dev_type){

            case DEVICE_SERIAL_TNC:
            case DEVICE_SERIAL_KISS_TNC:
            case DEVICE_SERIAL_GPS:
            case DEVICE_SERIAL_WX:
            case DEVICE_SERIAL_TNC_HSP_GPS:
            case DEVICE_SERIAL_TNC_AUX_GPS:

                switch (dev_type) {

                    case DEVICE_SERIAL_TNC:
                        if (debug_level & 2)
                            fprintf(stderr,"Opening a Serial TNC device\n");

                        break;

                    case DEVICE_SERIAL_KISS_TNC:
                        if (debug_level & 2)
                            fprintf(stderr,"Opening a Serial KISS TNC device\n");

                        break;

                    case DEVICE_SERIAL_GPS:
                        if (debug_level & 2)
                            fprintf(stderr,"Opening a Serial GPS device\n");
                        // Must wait for valid GPS parsing after
                        // sending one posit.
                        my_position_valid = 1;
                        using_gps_position++;
                        statusline(langcode("BBARSTA041"),1);
//fprintf(stderr,"my_position_valid = 1, using_gps_position:%d\n",using_gps_position);
 
                        break;

                    case DEVICE_SERIAL_WX:
                        if (debug_level & 2)
                            fprintf(stderr,"Opening a Serial WX device\n");

                        break;

                    case DEVICE_SERIAL_TNC_HSP_GPS:
                        if (debug_level & 2)
                            fprintf(stderr,"Opening a Serial TNC w/HSP GPS device\n");
                        // Must wait for valid GPS parsing after
                        // sending one posit.
                        my_position_valid = 1;
                        using_gps_position++;
                        statusline(langcode("BBARSTA041"),1);
//fprintf(stderr,"my_position_valid = 1, using_gps_position:%d\n",using_gps_position);
 
                        break;

                    case DEVICE_SERIAL_TNC_AUX_GPS:
                        if (debug_level & 2)
                            fprintf(stderr,"Opening a Serial TNC w/AUX GPS device\n");
                        // Must wait for valid GPS parsing after
                        // sending one posit.
                        my_position_valid = 1;
                        using_gps_position++;
                        statusline(langcode("BBARSTA041"),1);
//fprintf(stderr,"my_position_valid = 1, using_gps_position:%d\n",using_gps_position);
 
                        break;

                    default:
                        break;
                }
                clear_port_data(port_avail,0);

//if (begin_critical_section(&port_data_lock, "interface.c:add_device(1)" ) > 0)
//    fprintf(stderr,"port_data_lock, Port = %d\n", port_avail);    

                port_data[port_avail].device_type = dev_type;
                strcpy(port_data[port_avail].device_name,dev_nm);
                port_data[port_avail].sp = dev_sp;
                port_data[port_avail].style = dev_sty;
                if (dev_type == DEVICE_SERIAL_WX) {
                    if (strcmp("1",passwd) == 0)
                        port_data[port_avail].data_type = 1;
                }

//if (end_critical_section(&port_data_lock, "interface.c:add_device(2)" ) > 0)
//    fprintf(stderr,"port_data_lock, Port = %d\n", port_avail);    

                ok = serial_init(port_avail);
                break;

            case DEVICE_NET_STREAM:
                if (debug_level & 2)
                    fprintf(stderr,"Opening a Network stream\n");

                clear_port_data(port_avail,0);

//if (begin_critical_section(&port_data_lock, "interface.c:add_device(3)" ) > 0)
//    fprintf(stderr,"port_data_lock, Port = %d\n", port_avail);    

                port_data[port_avail].device_type = DEVICE_NET_STREAM;
                strcpy(port_data[port_avail].device_host_name,dev_nm);
                strcpy(port_data[port_avail].device_host_pswd,passwd);
                port_data[port_avail].socket_port = dev_sck_p;
                port_data[port_avail].reconnect = reconnect;

//if (end_critical_section(&port_data_lock, "interface.c:add_device(4)" ) > 0)
//    fprintf(stderr,"port_data_lock, Port = %d\n", port_avail);

                ok = net_init(port_avail);

                if (ok == 1) {

                    /* if connected now send password */
                    if (strlen(passwd)) {

                        if (filter_string != NULL
                                && strlen(filter_string) > 0) {    // Filter specified

                            // Please note that "filter" must be the 8th
                            // parameter on the line in order to be
                            // parsed properly by the servers.
                            xastir_snprintf(logon_txt,
                                sizeof(logon_txt),
                                "user %s pass %s vers %s filter %s%c%c",
                                my_callsign,
                                passwd,
                                verstr,
                                filter_string,
                                '\r',
                                '\n');
                        }
                        else {  // No filter specified
                            xastir_snprintf(logon_txt,
                                sizeof(logon_txt),
                                "user %s pass %s vers %s%c%c",
                                my_callsign,
                                passwd,
                                verstr,
                                '\r',
                                '\n');
                        }
                    }
                    else {
                        xastir_snprintf(logon_txt,
                            sizeof(logon_txt),
                            "user %s pass -1 vers %s %c%c",
                            my_callsign,
                            verstr,
                            '\r',
                            '\n');
                    }

//fprintf(stderr,"Sending this string: %s\n", logon_txt);
 
                    port_write_string(port_avail,logon_txt);
                }
                break;

            case DEVICE_AX25_TNC:
                if (debug_level & 2)
                    fprintf(stderr,"Opening a network AX25 TNC\n");

                clear_port_data(port_avail,0);

//if (begin_critical_section(&port_data_lock, "interface.c:add_device(5)" ) > 0)
//    fprintf(stderr,"port_data_lock, Port = %d\n", port_avail);

                port_data[port_avail].device_type = DEVICE_AX25_TNC;
                strcpy(port_data[port_avail].device_name,dev_nm);

//if (end_critical_section(&port_data_lock, "interface.c:add_device(6)" ) > 0)
//    fprintf(stderr,"port_data_lock, Port = %d\n", port_avail);

                ok = ax25_init(port_avail);
                break;

            case DEVICE_NET_GPSD:
                if (debug_level & 2)
                    fprintf(stderr,"Opening a network GPS using gpsd\n");

                clear_port_data(port_avail,0);

//if (begin_critical_section(&port_data_lock, "interface.c:add_device(7)" ) > 0)
//    fprintf(stderr,"port_data_lock, Port = %d\n", port_avail);

                port_data[port_avail].device_type = DEVICE_NET_GPSD;
                strcpy(port_data[port_avail].device_host_name,dev_nm);
                port_data[port_avail].socket_port = dev_sck_p;
                port_data[port_avail].reconnect = reconnect;

//if (end_critical_section(&port_data_lock, "interface.c:add_device(8)" ) > 0)
//    fprintf(stderr,"port_data_lock, Port = %d\n", port_avail);

                ok = net_init(port_avail);
                if (ok == 1) {
                    xastir_snprintf(logon_txt, sizeof(logon_txt), "R\r\n");
                    port_write_string(port_avail,logon_txt);
                    // Must wait for valid GPS parsing after sending
                    // one posit.
                    my_position_valid = 1;
                    using_gps_position++;
                    statusline(langcode("BBARSTA041"),1);
//fprintf(stderr,"my_position_valid = 1, using_gps_position:%d\n",using_gps_position);
                }
                break;

            case DEVICE_NET_WX:
                if (debug_level & 2)
                    fprintf(stderr,"Opening a network WX\n");

                clear_port_data(port_avail,0);

//if (begin_critical_section(&port_data_lock, "interface.c:add_device(9)" ) > 0)
//    fprintf(stderr,"port_data_lock, Port = %d\n", port_avail);

                port_data[port_avail].device_type = DEVICE_NET_WX;
                strcpy(port_data[port_avail].device_host_name,dev_nm);
                port_data[port_avail].socket_port = dev_sck_p;
                port_data[port_avail].reconnect = reconnect;
                if (strcmp("1",passwd) == 0)
                    port_data[port_avail].data_type = 1;

//if (end_critical_section(&port_data_lock, "interface.c:add_device(10)" ) > 0)
//    fprintf(stderr,"port_data_lock, Port = %d\n", port_avail);

                ok = net_init(port_avail);
                if (ok == 1) {
                    /* if connected now send call and program version */
                    xastir_snprintf(logon_txt, sizeof(logon_txt), "%s %s%c%c", my_callsign, VERSIONTXT, '\r', '\n');
                    port_write_string(port_avail,logon_txt);
                }
                break;

            case DEVICE_NET_DATABASE:
                if (debug_level & 2)
                    fprintf(stderr,"Opening a network database stream\n");

                clear_port_data(port_avail,0);

//if (begin_critical_section(&port_data_lock, "interface.c:add_device(11)" ) > 0)
//    fprintf(stderr,"port_data_lock, Port = %d\n", port_avail);

                port_data[port_avail].device_type = DEVICE_NET_DATABASE;
                strcpy(port_data[port_avail].device_host_name,dev_nm);
                port_data[port_avail].socket_port = dev_sck_p;
                port_data[port_avail].reconnect = reconnect;
                if (strcmp("1",passwd) == 0)
                    port_data[port_avail].data_type = 1;

//if (end_critical_section(&port_data_lock, "interface.c:add_device(12)" ) > 0)
//    fprintf(stderr,"port_data_lock, Port = %d\n", port_avail);

                ok = net_init(port_avail);
                if (ok == 1) {
                    /* if connected now send call and program version */
                    xastir_snprintf(logon_txt, sizeof(logon_txt), "%s %s%c%c", my_callsign, VERSIONTXT, '\r', '\n');
                    port_write_string(port_avail,logon_txt);
                }
                break;

            case DEVICE_NET_AGWPE:
                if (debug_level & 2)
                    fprintf(stderr,"Opening a network AGWPE stream");

                clear_port_data(port_avail,0);

//if (begin_critical_section(&port_data_lock, "interface.c:add_device(13)" ) > 0)
//    fprintf(stderr,"port_data_lock, Port = %d\n", port_avail);

                port_data[port_avail].device_type = DEVICE_NET_AGWPE;
                strcpy(port_data[port_avail].device_host_name,dev_nm);
                port_data[port_avail].socket_port = dev_sck_p;
                port_data[port_avail].reconnect = reconnect;
                if (strcmp("1",passwd) == 0)
                    port_data[port_avail].data_type = 1;

//if (end_critical_section(&port_data_lock, "interface.c:add_device(14)" ) > 0)
//    fprintf(stderr,"port_data_lock, Port = %d\n", port_avail);

                ok = net_init(port_avail);
                if (ok == 1) {
                    // If password isn't empty, send login
                    // information
                    //
                    if (strlen(passwd) != 0) {

                        // Send the login packet 
                        send_agwpe_packet(port_avail,
                            0,      // AGWPE RadioPort
                            'P',    // Login/Password Frame
                            NULL,   // FromCall
                            NULL,   // ToCall
                            NULL,   // Path
                            passwd, // Path (password in this case only)
                            510);   // Length
                    }
                }
                break;

            default:
                break;
        }

        if (ok == 1) {  // If port is connected...

            if (debug_level & 2)
                fprintf(stderr,"*** add_device: ok: %d ***\n",ok);

            /* if all is ok check and start read write threads */
            (void)start_port_threads(port_avail);
            usleep(100000); // 100ms

            switch (dev_type) {

                case DEVICE_SERIAL_TNC:
                case DEVICE_SERIAL_TNC_HSP_GPS:
                case DEVICE_SERIAL_TNC_AUX_GPS:

                    if (ok == 1) {

// We already have the lock by the time add_device() is called!
//begin_critical_section(&devices_lock, "interface.c:add_device" );

                        xastir_snprintf(temp, sizeof(temp), "config/%s", devices[port_avail].tnc_up_file);

//end_critical_section(&devices_lock, "interface.c:add_device" );

                        (void)command_file_to_tnc_port(port_avail,get_data_base_dir(temp));
                    }
                    break;

                case DEVICE_SERIAL_KISS_TNC:
                    // Send the KISS parameters to the TNC
                    send_kiss_config(port_avail,0,0x01,atoi(devices[port_avail].txdelay));
                    send_kiss_config(port_avail,0,0x02,atoi(devices[port_avail].persistence));
                    send_kiss_config(port_avail,0,0x03,atoi(devices[port_avail].slottime));
                    send_kiss_config(port_avail,0,0x04,atoi(devices[port_avail].txtail));
                    send_kiss_config(port_avail,0,0x05,devices[port_avail].fullduplex);
                    break;

                case DEVICE_NET_AGWPE:
                    // Query for the AGWPE version
                    //
                    send_agwpe_packet(port_avail,
                        0,      // AGWPE RadioPort
                        'R',    // Request SW Version Frame
                        NULL,   // FromCall
                        NULL,   // ToCall
                        NULL,   // Path
                        NULL,   // Data
                        0);     // Length

                    // Ask to receive "Monitor" frames
                    //
                    send_agwpe_packet(port_avail,
                        0,      // AGWPE RadioPort
                        'm',    // Monitor Packets Frame
                        NULL,   // FromCall
                        NULL,   // ToCall
                        NULL,   // Path
                        NULL,   // Data
                        0);     // Length

/*
                    // Send a dummy UI frame for testing purposes.
                    //
                    send_agwpe_packet(port_avail,
                        atoi(devices[port_avail].device_host_filter_string), // AGWPE radio port
                        '\0',       // type
                        "TEST-3",   // FromCall
                        "APRS",     // ToCall
                        NULL,       // Path
                        "Test",     // Data
                        4);         // length

                    // Send another dummy UI frame.
                    //
                    send_agwpe_packet(port_avail,
                        atoi(devices[port_avail].device_host_filter_string), // AGWPE radio port
                        '\0',       // type
                        "TEST-3",   // FromCall
                        "APRS",     // ToCall
                        "RELAY,SAR1-1,SAR2-1,SAR3-1,SAR4-1,SAR5-1,SAR6-1,SAR7-1", // Path
                        "Testing this darned thing!",   // Data
                        26);     // length
*/

                    break;
 
                default:
                    break;
            }
        }

        if (ok == -1) {
            xastir_snprintf(temp, sizeof(temp), langcode("POPEM00015"), port_avail);
            popup_message(langcode("POPEM00004"),temp);
            port_avail = -1;
        } else {
            if (ok == 0) {
                xastir_snprintf(temp, sizeof(temp), langcode("POPEM00016"), port_avail);
                popup_message(langcode("POPEM00004"),temp);
                port_avail = -1;
            }
        }
    } else
        popup_message(langcode("POPEM00004"),langcode("POPEM00017"));

    return(port_avail);
}





//***********************************************************
// port status
// port is the port to get status on
//***********************************************************
void port_stats(int port) {
    if (port >= 0) {
        fprintf(stderr,"Port %d %s Status\n\n",port,dtype[port_data[port].device_type].device_name);
        fprintf(stderr,"Errors %d\n",port_data[port].errors);
        fprintf(stderr,"Reconnects %d\n",port_data[port].reconnects);
        fprintf(stderr,"Bytes in: %ld  out: %ld\n",(long)port_data[port].bytes_input,(long)port_data[port].bytes_output);
        fprintf(stderr,"\n");
    }
}





//***********************************************************
// startup defined ports
//***********************************************************
void startup_all_or_defined_port(int port) {
    int i, override;
    int start;

    override = 0;
    if (port == -1) {
        start = 0;
    } else {
        start = port;
        override = 1;
    }

begin_critical_section(&devices_lock, "interface.c:startup_all_or_defined_port" );

    for (i = start; i < MAX_IFACE_DEVICES; i++){

        // Only start ports that aren't already up
        if ( (port_data[i].active != DEVICE_IN_USE)
                || (port_data[i].status != DEVICE_UP) ) {

            switch (devices[i].device_type) {

                case DEVICE_NET_STREAM:
                    if (devices[i].connect_on_startup == 1 || override) {

//end_critical_section(&devices_lock, "interface.c:startup_all_or_defined_port" );
                    //(void)del_device(i);    // Disconnect old port if it exists
//begin_critical_section(&devices_lock, "interface.c:startup_all_or_defined_port" );

                        (void)add_device(i,
                            DEVICE_NET_STREAM,
                            devices[i].device_host_name,
                            devices[i].device_host_pswd,
                            devices[i].sp,
                            0,
                            0,
                            devices[i].reconnect,
                            devices[i].device_host_filter_string);
                    }
                    break;

                case DEVICE_NET_DATABASE:
                    if (devices[i].connect_on_startup == 1 || override) {

//end_critical_section(&devices_lock, "interface.c:startup_all_or_defined_port" );
                    //(void)del_device(i);    // Disconnect old port if it exists
//begin_critical_section(&devices_lock, "interface.c:startup_all_or_defined_port" );

                        (void)add_device(i,
                            DEVICE_NET_DATABASE,
                            devices[i].device_host_name,
                            devices[i].device_host_pswd,
                            devices[i].sp,
                            0,
                            0,
                            devices[i].reconnect,
                            devices[i].device_host_filter_string);
                    }
                    break;

                case DEVICE_NET_AGWPE:
                    if (devices[i].connect_on_startup == 1 || override) {

//end_critical_section(&devices_lock, "interface.c:startup_all_or_defined_port" );
                    //(void)del_device(i);    // Disconnect old port if it exists
//begin_critical_section(&devices_lock, "interface.c:startup_all_or_defined_port" );

                        (void)add_device(i,
                            DEVICE_NET_AGWPE,
                            devices[i].device_host_name,
                            devices[i].device_host_pswd,
                            devices[i].sp,
                            0,
                            0,
                            devices[i].reconnect,
                            devices[i].device_host_filter_string);
                    }
                    break;

                case DEVICE_NET_GPSD:
                    if (devices[i].connect_on_startup == 1 || override) {

//end_critical_section(&devices_lock, "interface.c:startup_all_or_defined_port" );
//                    (void)del_device(i);    // Disconnect old port if it exists
//begin_critical_section(&devices_lock, "interface.c:startup_all_or_defined_port" );

                        (void)add_device(i,
                            DEVICE_NET_GPSD,
                            devices[i].device_host_name,
                            "",
                            devices[i].sp,
                            0,
                            0,
                            devices[i].reconnect,
                            "");
                    }
                    break;

                case DEVICE_SERIAL_WX:
                    if (devices[i].connect_on_startup == 1 || override) {
                        (void)add_device(i,
                            DEVICE_SERIAL_WX,
                            devices[i].device_name,
                            devices[i].device_host_pswd,
                            -1,
                            devices[i].sp,
                            devices[i].style,
                            0,
                            "");
                    }
                    break;

                case DEVICE_NET_WX:
                    if (devices[i].connect_on_startup == 1 || override) {

//end_critical_section(&devices_lock, "interface.c:startup_all_or_defined_port" );
//                    (void)del_device(i);    // Disconnect old port if it exists
//begin_critical_section(&devices_lock, "interface.c:startup_all_or_defined_port" );

                        (void)add_device(i,
                            DEVICE_NET_WX,
                            devices[i].device_host_name,
                            devices[i].device_host_pswd,
                            devices[i].sp,
                            0,
                            0,
                            devices[i].reconnect,
                            "");
                    }
                    break;

                case DEVICE_SERIAL_GPS:
                    if (devices[i].connect_on_startup == 1 || override) {
                        (void)add_device(i,
                            DEVICE_SERIAL_GPS,
                            devices[i].device_name,
                            "",
                            -1,
                            devices[i].sp,
                            devices[i].style,
                            0,
                            "");
                    }
                    break;

                case DEVICE_SERIAL_TNC:
                case DEVICE_SERIAL_KISS_TNC:
                case DEVICE_SERIAL_TNC_HSP_GPS:
                case DEVICE_SERIAL_TNC_AUX_GPS:

                    if (devices[i].connect_on_startup == 1 || override) {
                        (void)add_device(i,
                            devices[i].device_type,
                            devices[i].device_name,
                            "",
                            -1,
                            devices[i].sp,
                            devices[i].style,
                            0,
                            "");
                    }
                    break;

                case DEVICE_AX25_TNC:
                    if (devices[i].connect_on_startup == 1 || override) {
                        (void)add_device(i,
                            DEVICE_AX25_TNC,
                            devices[i].device_name,
                            "",
                            -1,
                            -1,
                            -1,
                            0,
                            "");
                    }
                    break;

                default:
                    break;
            }   // End of switch
        }
        else if (debug_level & 2) {
            fprintf(stderr,"Skipping port %d, it's already running\n",i);
        }

        if (port != -1) {
            i = MAX_IFACE_DEVICES+1;
        }
    }

end_critical_section(&devices_lock, "interface.c:startup_all_or_defined_port" );

}





//***********************************************************
// shutdown all active ports
//***********************************************************
void shutdown_all_active_or_defined_port(int port) {
    int i;
    int start;

    if (debug_level & 2)
        fprintf(stderr,"\nshutdown_all_active_or_defined_port: %d\n\n",port);

    if (port == -1)
        start = 0;
    else
        start = port;

    for( i = start; i < MAX_IFACE_DEVICES; i++ ){
        if ( (port_data[i].active == DEVICE_IN_USE)
                && ( (port_data[i].status == DEVICE_UP)
                    || (port_data[i].status == DEVICE_ERROR) ) ) {
            if (debug_level & 2)
                fprintf(stderr,"Shutting down port %d \n",i);

            (void)del_device(i);
        }
        if (port != -1) // Stop after one iteration if port specified
            i = MAX_IFACE_DEVICES+1;
    }
}





//*************************************************************
// check ports                                               
//
// Called periodically by main.c:UpdateTime() function.
// Attempts to reconnect interfaces that are down.
//*************************************************************
void check_ports(void) {
    int i;
    int temp;

    for (i = 0; i < MAX_IFACE_DEVICES; i++){

        switch (port_data[i].device_type){
            case(DEVICE_NET_STREAM):
            //case(DEVICE_AX25_TNC):
            case(DEVICE_NET_GPSD):
            case(DEVICE_NET_WX):
                if (port_data[i].port_activity == 0) {
                    // We've seen no activity for one time period.  This variable
                    // is updated in interface_gui.c
    
                    if (port_data[i].status == DEVICE_ERROR) {
                        // We're already in the error state, so force a reconnect
                        port_data[i].reconnects = -1;
                    }
                    else if (port_data[i].status == DEVICE_UP) {
                        // No activity on a port that's being used.
                        // Cause a reconnect at the next iteration
                        if (debug_level & 2)
                            fprintf(stderr,"check_ports(): Inactivity on port %d, DEVICE_ERROR ***\n",i);
                        port_data[i].status = DEVICE_ERROR; // No activity, so force a shutdown

                        // If the below statement is enabled, it causes an immediate reconnect
                        // after one time-period of inactivity, currently 7.5 minutes, as set in
                        // main.c:UpdateTime().  This means the symbol will never change from green
                        // to red on the status bar, so the operator might not know about a
                        // connection that is being constantly reconnected.  By leaving it commented
                        // out we get one time period of red, and then it will reconnect at the 2nd
                        // time period.  This means we can reconnect within 15 minutes if a line
                        // goes dead.
                        //
                        //port_data[i].reconnects = -1;     // Causes an immediate reconnect
                    }

                }
                else {  // We saw activity on this port.
                    port_data[i].port_activity = 0;     // Reset counter for next time
                }
                break;
        }

        if (port_data[i].active == DEVICE_IN_USE && port_data[i].status == DEVICE_ERROR) {
            if (debug_level & 2)
                fprintf(stderr,"Found device error on port %d\n",i);

            if (port_data[i].reconnect == 1) {
                port_data[i].reconnects++;
                temp = port_data[i].reconnects;
                if (temp < 1) {
                    if (debug_level & 2)
                        fprintf(stderr,"Device asks for reconnect count now at %d\n",temp);

                    if (debug_level & 2)
                        fprintf(stderr,"Shutdown device %d\n",i);

                    shutdown_all_active_or_defined_port(i);

                    if (debug_level & 2)
                        fprintf(stderr,"Starting device %d\n",i);

                    startup_all_or_defined_port(i);

                    /* if error on startup */
                    if (port_data[i].status == DEVICE_ERROR)
                        port_data[i].reconnects = temp;
                } else {
                    if (debug_level & 2)
                        fprintf(stderr,"Device has either too many errors, or no activity at all!\n");

                    port_data[i].reconnects = temp - 2;
                }
            }
        }
    }
}





static char unproto_path_txt[MAX_LINE_SIZE+5];




 
// Function which selects an unproto path in round-robin fashion.
// Once we select a path, we save the number selected back to
// devices[port].unprotonum so that the next time around we select
// the next in the sequence.  If we don't come up with a valid
// unproto path, we use the unproto path: "WIDE,WIDE2-2".
//
// Input:  Port number
// Ouput:  String pointer containing unproto path
//
// WE7U:  Should we check to make sure that there are printable
// characters in the path?
//
unsigned char *select_unproto_path(int port) {
    int count;
    int done;
    int temp;
    int bump_up;
 

    // Set unproto path:
    // We look for a non-null path entry starting at the current
    // value of "unprotonum".  The first non-null path wins.
    count = 0;
    done = 0;
    bump_up = 0;


    while (!done && (count < 3)) {
        temp = (devices[port].unprotonum + count) % 3;
        switch (temp) {

            case 0:
                if (strlen(devices[port].unproto1) > 0) {
                    xastir_snprintf(unproto_path_txt,
                        sizeof(unproto_path_txt),
                        "%s",
                        devices[port].unproto1);
                    done++;
                }
                else {
                    // No path entered here.  Skip this path in the
                    // rotation for next time.
                    bump_up++;
                }
                break;

            case 1:
                    if (strlen(devices[port].unproto2) > 0) {
                        xastir_snprintf(unproto_path_txt,
                            sizeof(unproto_path_txt),
                            "%s",
                            devices[port].unproto2);
                        done++;
                    }
                    else {
                        // No path entered here.  Skip this path in
                        // the rotation for next time.
                        bump_up++;
                    }
                    break;

            case 2:
                    if (strlen(devices[port].unproto3) > 0) {
                        xastir_snprintf(unproto_path_txt,
                            sizeof(unproto_path_txt),
                            "%s",
                            devices[port].unproto3);
                        done++;
                    }
                    else {
                        // No path entered here.  Skip this path in
                        // the rotation for next time.
                        bump_up++;
                    }
                    break;
        }   // End of switch
        count++;
    }   // End of while loop

    if (!done) {
        // We found no entries in the unproto fields for the
        // interface.

        xastir_snprintf(unproto_path_txt,
                        sizeof(unproto_path_txt),
                        "WIDE,WIDE2-2");

    }

    // Increment the path number for the next round of
    // transmissions.  This will round-robin the paths so that all
    // entered paths get used.
    devices[port].unprotonum = (devices[port].unprotonum + 1 + bump_up) % 3;

    return(unproto_path_txt);
}





//***********************************************************
// output_my_aprs_data
// This is the function responsible for sending out my own
// posits.  The next function below this one handles objects,
// messages and the like (output_my_data).
//***********************************************************
void output_my_aprs_data(void) {
    char header_txt[MAX_LINE_SIZE+5];
    char header_txt_save[MAX_LINE_SIZE+5];
    char data_txt[MAX_LINE_SIZE+5];
    char data_txt_save[MAX_LINE_SIZE+5];
    char path_txt[MAX_LINE_SIZE+5];
    char *unproto_path;
    char data_txt2[5];
    struct tm *day_time;
    time_t sec;
    char my_pos[100];
    char my_output_lat[MAX_LAT];
    char my_output_long[MAX_LONG];
    char output_net[100];
    char wx_data[200];
    char output_phg[10];
    char output_cs[10];
    char output_alt[20];
    char output_brk[3];
    int ok;
    int port;


    header_txt_save[0] = '\0';
    data_txt_save[0] = '\0';
    sec = sec_now();

    // Format latitude string for transmit later
    strcpy(my_output_lat,my_lat);
    (void)output_lat(my_output_lat,transmit_compressed_posit);
    if (debug_level & 128)
        fprintf(stderr,"OUT LAT <%s>\n",my_output_lat);

    // Format longitude string for transmit later
    strcpy(my_output_long,my_long);
    (void)output_long(my_output_long,transmit_compressed_posit);
    if (debug_level & 128)
        fprintf(stderr,"OUT LONG <%s>\n",my_output_long);

begin_critical_section(&devices_lock, "interface.c:output_my_aprs_data" );

    // Iterate across the ports, set up each device's headers/paths/handshakes,
    // then transmit the posit if the port is open and tx is enabled.
    for (port = 0; port < MAX_IFACE_DEVICES; port++) {

        // First send any header/path info we might need out the port,
        // set up TNC's to the proper mode, etc.
        ok = 1;
        switch (port_data[port].device_type) {

//            case DEVICE_NET_DATABASE:

            case DEVICE_NET_AGWPE:

                output_net[0] = '\0';   // We don't need this header for AGWPE

                // Set unproto path:  Get next unproto path in
                // sequence.
                unproto_path = select_unproto_path(port);

                break;

            case DEVICE_NET_STREAM:

                xastir_snprintf(output_net, sizeof(output_net), "%s>%s,TCPIP*:", my_callsign, VERSIONFRM);
                break;

            case DEVICE_SERIAL_TNC_HSP_GPS:

                /* make dtr normal (talk to TNC) */
                if (port_data[port].status == DEVICE_UP) {
                    port_dtr(port,0);
                }

            case DEVICE_SERIAL_TNC_AUX_GPS:
            case DEVICE_SERIAL_KISS_TNC:
            case DEVICE_SERIAL_TNC:
            case DEVICE_AX25_TNC:

                /* clear this for a TNC */
                strcpy(output_net,"");

                /* Set my call sign */
                xastir_snprintf(header_txt, sizeof(header_txt), "%c%s %s\r", '\3', "MYCALL", my_callsign);

                // Send the callsign out to the TNC only if the interface is up and tx is enabled???
                // We don't set it this way for KISS TNC interfaces.
                if ( (port_data[port].device_type != DEVICE_SERIAL_KISS_TNC)
                        && (port_data[port].status == DEVICE_UP)
                        && (devices[port].transmit_data == 1)
                        && !transmit_disable
                        && !posit_tx_disable) {
                    port_write_string(port,header_txt);
                }

                // Set unproto path:  Get next unproto path in
                // sequence.
                unproto_path = select_unproto_path(port);

                xastir_snprintf(header_txt,
                        sizeof(header_txt),
                        "%c%s %s VIA %s\r",
                        '\3',
                        "UNPROTO",
                        VERSIONFRM,
                        unproto_path);

                xastir_snprintf(header_txt_save,
                        sizeof(header_txt_save),
                        "%s>%s,%s:",
                        my_callsign,
                        VERSIONFRM,
                        unproto_path);

                xastir_snprintf(path_txt,
                        sizeof(path_txt),
                        "%s",
                        unproto_path);


                // Send the header data to the TNC.  This sets the
                // unproto path that'll be used by the next packet.
                // We don't set it this way for KISS TNC interfaces.
                if ( (port_data[port].device_type != DEVICE_SERIAL_KISS_TNC)
                        && (port_data[port].status == DEVICE_UP)
                        && (devices[port].transmit_data == 1)
                        && !transmit_disable
                        && !posit_tx_disable) {
                    port_write_string(port,header_txt);
                }


                // Set converse mode.  We don't need to do this for
                // KISS TNC interfaces.
                xastir_snprintf(header_txt, sizeof(header_txt), "%c%s\r", '\3', "CONV");
                if ( (port_data[port].device_type != DEVICE_SERIAL_KISS_TNC)
                        && (port_data[port].status == DEVICE_UP)
                        && (devices[port].transmit_data == 1)
                        && !transmit_disable
                        && !posit_tx_disable) {
                    port_write_string(port,header_txt);
                }
                /*sleep(1);*/
                break;

            default: /* port has unknown device_type */
                ok = 0;
                break;

        } // End of switch


        // Set up some more strings for later transmission

        /* send station info */
        strcpy(output_cs, "");
        strcpy(output_phg,"");
        strcpy(output_alt,"");
        strcpy(output_brk,"");


        if (transmit_compressed_posit)
            strcpy(my_pos,
                compress_posit(my_output_lat,
                    my_group,
                    my_output_long,
                    my_symbol,
                    my_last_course,
                    my_last_speed,  // In knots
                    my_phg));
        else { /* standard non compressed mode */
            xastir_snprintf(my_pos, sizeof(my_pos), "%s%c%s%c", my_output_lat, my_group, my_output_long, my_symbol);
            /* get PHG, if used for output */
            if (strlen(my_phg) >= 6)
                strcpy(output_phg,my_phg);

            /* get CSE/SPD, Always needed for output even if 0 */
            xastir_snprintf(output_cs,
                sizeof(output_cs),
                "%03d/%03d/",
                my_last_course,
                my_last_speed);    // Speed in knots

            /* get altitude */
            if (my_last_altitude_time > 0)
                xastir_snprintf(output_alt, sizeof(output_alt), "A=%06ld/", my_last_altitude);
        }


        // And set up still more strings for later transmission
        switch (output_station_type) {
            case(1):
                /* APRS_MOBILE LOCAL TIME */
                if((strlen(output_cs) < 8) && (my_last_altitude_time > 0))
                    strcpy(output_brk,"/");

                day_time = localtime(&sec);

                xastir_snprintf(data_txt, sizeof(data_txt), "%s@%02d%02d%02d/%s%s%s%s%s\r",
                        output_net, day_time->tm_mday, day_time->tm_hour,
                        day_time->tm_min, my_pos, output_cs, output_brk,
                        output_alt, my_comment);
                xastir_snprintf(data_txt_save, sizeof(data_txt_save),
                        "@%02d%02d%02d/%s%s%s%s%s\r", day_time->tm_mday,
                        day_time->tm_hour, day_time->tm_min,
                        my_pos,output_cs, output_brk,output_alt, my_comment);
                break;

            case(2):
                /* APRS_MOBILE ZULU DATE-TIME */
                if((strlen(output_cs) < 8) && (my_last_altitude_time > 0))
                    strcpy(output_brk,"/");

                day_time = gmtime(&sec);

                xastir_snprintf(data_txt, sizeof(data_txt),
                        "%s@%02d%02d%02dz%s%s%s%s%s\r", output_net,
                        day_time->tm_mday, day_time->tm_hour,
                        day_time->tm_min,my_pos, output_cs,
                        output_brk, output_alt, my_comment);
                xastir_snprintf(data_txt_save, sizeof(data_txt_save),
                        "@%02d%02d%02dz%s%s%s%s%s\r", day_time->tm_mday,
                        day_time->tm_hour, day_time->tm_min,my_pos,
                        output_cs, output_brk, output_alt, my_comment);
                break;

            case(3):
                /* APRS_MOBILE ZULU TIME w/SEC */
                if((strlen(output_cs) < 8) && (my_last_altitude_time > 0))
                    strcpy(output_brk,"/");

                day_time = gmtime(&sec);

                xastir_snprintf(data_txt, sizeof(data_txt), "%s@%02d%02d%02dh%s%s%s%s%s\r",
                        output_net, day_time->tm_hour, day_time->tm_min,
                        day_time->tm_sec, my_pos,output_cs, output_brk,
                        output_alt,my_comment);
                xastir_snprintf(data_txt_save, sizeof(data_txt_save),
                        "@%02d%02d%02dh%s%s%s%s%s\r", day_time->tm_hour,
                        day_time->tm_min, day_time->tm_sec, my_pos,output_cs,
                        output_brk,output_alt,my_comment);
                break;

            case(4):
                /* APRS position with WX data*/
                sec = wx_tx_data1(wx_data);
                if (sec != 0) {
                    xastir_snprintf(data_txt, sizeof(data_txt), "%s%c%s%s\r",
                            output_net, aprs_station_message_type, my_pos,wx_data);
                    xastir_snprintf(data_txt_save, sizeof(data_txt_save), "%c%s%s\r",
                            aprs_station_message_type, my_pos, wx_data);
                }
                else {
                    /* default to APRS FIXED if no wx data */
                    if ((strlen(output_phg) < 6) && (my_last_altitude_time > 0))
                        strcpy(output_brk,"/");

                    xastir_snprintf(data_txt, sizeof(data_txt), "%s%c%s%s%s%s%s\r",
                            output_net, aprs_station_message_type,
                            my_pos,output_phg, output_brk, output_alt, my_comment);
                    xastir_snprintf(data_txt_save, sizeof(data_txt_save), "%c%s%s%s%s%s\r",
                            aprs_station_message_type, my_pos, output_phg,
                            output_brk, output_alt, my_comment);
                }
                break;

            case(5):
                /* APRS position with ZULU DATE-TIME and WX data */
                sec = wx_tx_data1(wx_data);
                if (sec != 0) {
                    day_time = gmtime(&sec);

                    xastir_snprintf(data_txt, sizeof(data_txt), "%s@%02d%02d%02dz%s%s\r",
                            output_net, day_time->tm_mday, day_time->tm_hour,
                            day_time->tm_min, my_pos,wx_data);
                    xastir_snprintf(data_txt_save, sizeof(data_txt_save),
                            "@%02d%02d%02dz%s%s\r", day_time->tm_mday,
                            day_time->tm_hour, day_time->tm_min, my_pos,wx_data);
                } else {
                    /* default to APRS FIXED if no wx data */
                    if((strlen(output_phg) < 6) && (my_last_altitude_time > 0))
                        strcpy(output_brk,"/");

                    xastir_snprintf(data_txt, sizeof(data_txt), "%s%c%s%s%s%s%s\r",
                            output_net, aprs_station_message_type, my_pos,output_phg,
                            output_brk, output_alt, my_comment);
                    xastir_snprintf(data_txt_save, sizeof(data_txt_save), "%c%s%s%s%s%s\r",
                            aprs_station_message_type, my_pos, output_phg,
                            output_brk, output_alt, my_comment);
                }
                break;

                /* default to APRS FIXED if no wx data */
            case(0):

            default:
                /* APRS_FIXED */
                if ((strlen(output_phg) < 6) && (my_last_altitude_time > 0))
                    strcpy(output_brk,"/");

                xastir_snprintf(data_txt, sizeof(data_txt), "%s%c%s%s%s%s%s\r",
                        output_net, aprs_station_message_type, my_pos,output_phg,
                        output_brk, output_alt, my_comment);
                xastir_snprintf(data_txt_save, sizeof(data_txt_save), "%c%s%s%s%s%s\r",
                        aprs_station_message_type, my_pos, output_phg,
                        output_brk, output_alt, my_comment);
                break;
        }


        //fprintf(stderr,"data_txt_save: %s\n",data_txt_save);


        if (ok) {
            // Here's where the actual transmit of the posit occurs.  The
            // transmit string has been set up in "data_txt" by this point.

            // If transmit or posits have been turned off, don't transmit posit
            if ( (port_data[port].status == DEVICE_UP)
                    && (devices[port].transmit_data == 1)
                    && !transmit_disable
                    && !posit_tx_disable) {

// WE7U:  Change so that path is passed as well for KISS TNC
// interfaces:  header_txt_save would probably be the one to pass,
// or create a new string just for KISS TNC's.

                if (port_data[port].device_type == DEVICE_SERIAL_KISS_TNC) {

// Note:  This one has callsign & destination in the string

                    // Transmit the posit out the KISS interface
                    send_ax25_frame(port,
                                    my_callsign,    // source
                                    VERSIONFRM,     // destination
                                    path_txt,       // path
                                    data_txt);      // data
                }

//WE7U:AGWPE
                else if (port_data[port].device_type == DEVICE_NET_AGWPE) {

                    // Set unproto path:  Get next unproto path in
                    // sequence.
                    unproto_path = select_unproto_path(port);

// We need to remove the complete AX.25 header from data_txt before
// we call this routine!  Instead put the digipeaters into the
// ViaCall fields.
                    send_agwpe_packet(port,         // Xastir interface port
                        atoi(devices[port].device_host_filter_string), // AGWPE RadioPort
                        '\0',         // Type of frame
                        my_callsign,  // source
                        VERSIONFRM,   // destination
                        unproto_path, // Path,
                        data_txt,
                        strlen(data_txt));
                }

                else {  // Not a Serial KISS TNC interface
                    port_write_string(port, data_txt);  // Transmit the posit
                }

                if (debug_level & 2)
                    fprintf(stderr,"TX:%d<%s>\n",port,data_txt);

                /* add new line on network data */
                if (port_data[port].device_type == DEVICE_NET_STREAM) {
                    xastir_snprintf(data_txt2, sizeof(data_txt2), "\n");                 // Transmit a newline
                    port_write_string(port, data_txt2);
                }
            } else {
            }
        } // End of posit transmit: "if (ok)"
    } // End of big loop

end_critical_section(&devices_lock, "interface.c:output_my_aprs_data" );

    // This will log a posit in the general format for a network interface
    // whether or not any network interfaces are currently up.
    if (log_net_data) {
        xastir_snprintf(data_txt, sizeof(data_txt), "%s>%s,TCPIP*:%s", my_callsign,
                VERSIONFRM, data_txt_save);
        log_data(LOGFILE_NET,(char *)data_txt);
    }


    // Note that this will only log one TNC line per transmission now matter
    // how many TNC's are defined.  It's a representative sample of what we're
    // sending out.  At least one TNC interface must be enabled in order to
    // have anything output to the log file here.
    if (log_tnc_data) {
        if (header_txt_save[0] != '\0') {
            xastir_snprintf(data_txt, sizeof(data_txt), "%s%s", header_txt_save, data_txt_save);
            log_data(LOGFILE_TNC,(char *)data_txt);
        }
    }

//fprintf(stderr,"Data_txt:%s\n", data_txt);
}





//*****************************************************************************
// output_my_data()
//
// 1) Used to send local messages/objects/items.  Cooked mode.
// 2) Used from output_igate_net(), igating from RF to the 'net.  Raw mode.
// 3) Used from output_igate_rf() to igate from the 'net to RF.  Cooked mode.
// 4) Used from output_nws_igate_rf() to send NWS packets out RF.  Cooked mode.
// 5) Used for queries and responses.  Cooked mode.
//
// Parameters:
// message: the message data to send
// port: the port transmitting through, or -1 for all
// type: 0 for my data, 1 for raw data (Cooked/Raw)
// loopback_only: 0 for transmit/loopback, 1 for loopback only
// use_igate_path: 0 for standard unproto paths, 1 for igate path
//
// This function sends out messages/objects/bulletins/etc.
// This one currently tries to do local logging even if
// transmit is disabled.
//*****************************************************************************
void output_my_data(char *message, int port, int type, int loopback_only, int use_igate_path, char *path) {
    char data_txt[MAX_LINE_SIZE+5];
    char data_txt_save[MAX_LINE_SIZE+5];
    char path_txt[MAX_LINE_SIZE+5];
    char *unproto_path;
    char output_net[100];
    int ok, start, finish, i;
    int done;


    if (debug_level & 1)
        fprintf(stderr,"Sending out port: %d, type: %d\n", port, type);

    if (message == NULL)
        return;

    if (message[0] == '\0')
        return;
 
    data_txt_save[0] = '\0';

    if (port == -1) {   // Send out all of the interfaces
        start = 0;
        finish = MAX_IFACE_DEVICES;
    }
    else {  // Only send out the chosen interface
        start  = port;
        finish = port + 1;
    }

begin_critical_section(&devices_lock, "interface.c:output_my_data" );

    for (i = start; i < finish; i++) {
        ok = 1;
        if (type == 0) {                        // my data
            switch (port_data[i].device_type) {

//                case DEVICE_NET_DATABASE:

                case DEVICE_NET_AGWPE:
//fprintf(stderr,"DEVICE_NET_AGWPE\n");
                    output_net[0] = '\0';   // Clear header
                    break;

                case DEVICE_NET_STREAM:
                    if (debug_level & 1)
                        fprintf(stderr,"%d Net\n",i);
                    xastir_snprintf(output_net,
                        sizeof(output_net),
                        "%s>%s,TCPIP*:",
                        my_callsign,
                        VERSIONFRM);
                    break;

                case DEVICE_SERIAL_TNC_HSP_GPS:
                    if (port_data[i].status == DEVICE_UP && !loopback_only) {
                        port_dtr(i,0);           // make DTR normal (talk to TNC)
                    }

                case DEVICE_SERIAL_TNC_AUX_GPS:
                case DEVICE_SERIAL_KISS_TNC:
                case DEVICE_SERIAL_TNC:
                case DEVICE_AX25_TNC:

                    if (debug_level & 1)
                        fprintf(stderr,"%d AX25 TNC\n",i);
                    strcpy(output_net,"");      // clear this for a TNC

                    /* Set my call sign */
                    xastir_snprintf(data_txt,
                        sizeof(data_txt),
                        "%c%s %s\r",
                        '\3',
                        "MYCALL",
                        my_callsign);

                    if ( (port_data[i].device_type != DEVICE_SERIAL_KISS_TNC)
                            &&  (port_data[i].status == DEVICE_UP)
                            && (devices[i].transmit_data == 1)
                            && !transmit_disable
                            && !loopback_only) {
                        port_write_string(i,data_txt);
                        usleep(10000);  // 10ms
                    }
 
                    done = 0;
 
                    // Set unproto path.  First check whether we're
                    // to use the igate path.  If so and the path
                    // isn't empty, skip the rest of the path selection:
                    if ( (use_igate_path)
                            && (strlen(devices[i].unproto_igate) > 0) ) {

// WE7U:  Should we check here and in the following path
// selection code to make sure that there are printable characters
// in the path?  Also:  Output_my_aprs_data() has nearly identical
// path selection code.  Fix it in one place, fix it in the other.

                        xastir_snprintf(data_txt,
                            sizeof(data_txt),
                            "%c%s %s VIA %s\r",
                            '\3',
                            "UNPROTO",
                            VERSIONFRM,
                            devices[i].unproto_igate);

                        xastir_snprintf(data_txt_save,
                            sizeof(data_txt_save),
                            "%s>%s,%s:",
                            my_callsign,
                            VERSIONFRM,
                            devices[i].unproto_igate);

                        xastir_snprintf(path_txt,
                            sizeof(path_txt),
                            "%s",
                            devices[i].unproto_igate);

                        done++;
                    }

                    // Check whether a path was passed to us as a
                    // parameter:
                    if ( (path != NULL) && (strlen(path) != 0) ) {

                        xastir_snprintf(data_txt,
                            sizeof(data_txt),
                            "%c%s %s VIA %s\r",
                            '\3',
                            "UNPROTO",
                            VERSIONFRM,
                            path);

                        xastir_snprintf(data_txt_save,
                            sizeof(data_txt_save),
                            "%s>%s,%s:",
                            my_callsign,
                            VERSIONFRM,
                            path);

                        xastir_snprintf(path_txt,
                            sizeof(path_txt),
                            "%s",
                            path);

                        done++;
                    }

                    if (!done) {

                        // Set unproto path:  Get next unproto path
                        // in sequence.
                        unproto_path = select_unproto_path(i);

                        xastir_snprintf(data_txt,
                            sizeof(data_txt),
                            "%c%s %s VIA %s\r",
                            '\3',
                            "UNPROTO",
                            VERSIONFRM,
                            unproto_path);

                        xastir_snprintf(data_txt_save,
                            sizeof(data_txt_save),
                            "%s>%s,%s:",
                            my_callsign,
                            VERSIONFRM,
                            unproto_path);

                        xastir_snprintf(path_txt,
                            sizeof(path_txt),
                            "%s",
                            unproto_path);

                        done++;
                    }


                    if ( (port_data[i].device_type != DEVICE_SERIAL_KISS_TNC)
                            && (port_data[i].status == DEVICE_UP)
                            && (devices[i].transmit_data == 1)
                            && !transmit_disable
                            && !loopback_only) {
                        port_write_string(i,data_txt);
                        usleep(10000);  // 10ms
                    }
 
                    // Set converse mode
                    xastir_snprintf(data_txt, sizeof(data_txt), "%c%s\r", '\3', "CONV");

                    if ( (port_data[i].device_type != DEVICE_SERIAL_KISS_TNC)
                            && (port_data[i].status == DEVICE_UP)
                            && (devices[i].transmit_data == 1)
                            && !transmit_disable
                            && !loopback_only) {
                        port_write_string(i,data_txt);
                        usleep(20000); // 20ms
                    }
                    break;

                default: /* unknown */
                    ok = 0;
                    break;
            } // End of switch
        } else {    // Type == 1, raw data.  Probably igating something...
            strcpy(output_net,"");
        }


        if (ok) {
            /* send data */
            xastir_snprintf(data_txt, sizeof(data_txt), "%s%s\r", output_net, message);

//fprintf(stderr,"%s\n",data_txt);

            if ( (port_data[i].status == DEVICE_UP)
                    && (devices[i].transmit_data == 1)
                    && !transmit_disable
                    && !loopback_only) {

// WE7U:  Change so that path is passed as well for KISS TNC
// interfaces:  data_txt_save would probably be the one to pass,
// or create a new string just for KISS TNC's.

                if (port_data[i].device_type == DEVICE_SERIAL_KISS_TNC) {

                    // Transmit
                    send_ax25_frame(i,
                                    my_callsign,    // source
                                    VERSIONFRM,     // destination
                                    path_txt,       // path
                                    data_txt);      // data
                }

//WE7U:AGWPE
                else if (port_data[i].device_type == DEVICE_NET_AGWPE) {

                    // Set unproto path.  First check whether we're
                    // to use the igate path.  If so and the path
                    // isn't empty, skip the rest of the path selection:
                    if ( (use_igate_path)
                            && (strlen(devices[i].unproto_igate) > 0) ) {

                        xastir_snprintf(path_txt,
                            sizeof(path_txt),
                            "%s",
                            devices[i].unproto_igate);
                    }
                    // Check whether a path was passed to us as a
                    // parameter:
                    else if ( (path != NULL) && (strlen(path) != 0) ) {

                        xastir_snprintf(path_txt,
                            sizeof(path_txt),
                            "%s",
                            path);
                    }
                    else {
                        // Set unproto path:  Get next unproto path in
                        // sequence.

                        unproto_path = select_unproto_path(port);
                        xastir_snprintf(path_txt,
                            sizeof(path_txt),
                            "%s",
                            unproto_path);
                    }

// We need to remove the complete AX.25 header from data_txt before
// we call this routine!  Instead put the digipeaters into the
// ViaCall fields.
//fprintf(stderr,"send_agwpe_packet\n");

                    send_agwpe_packet(i,            // Xastir interface port
                        atoi(devices[i].device_host_filter_string), // AGWPE RadioPort
                        '\0',         // Type of frame
                        my_callsign,  // source
                        VERSIONFRM,   // destination
                        path_txt, // Path,
                        data_txt,
                        strlen(data_txt));
                }

                else {  // Not a Serial KISS TNC interface
                    port_write_string(i, data_txt);  // Transmit
                }

                if (debug_level & 1)
                    fprintf(stderr,"Sending to interface:%d, %s\n",i,data_txt);
            }

            if (debug_level & 2)
                fprintf(stderr,"TX:%d<%s>\n",i,data_txt);

            /* add newline on network data */
            if (port_data[i].device_type == DEVICE_NET_STREAM) {
                xastir_snprintf(data_txt, sizeof(data_txt), "\n");

                if ( (port_data[i].status == DEVICE_UP)
                        && (devices[i].transmit_data == 1)
                        && !transmit_disable
                        && !loopback_only) {
                    port_write_string(i,data_txt);
                }
            } else {
            }
        }
//        if (port != -1)
//            i = MAX_IFACE_DEVICES+1;    // process only one port
    }

end_critical_section(&devices_lock, "interface.c:output_my_data" );

    // This will log a posit in the general format for a network interface
    // whether or not any network interfaces are currently up.
    xastir_snprintf(data_txt, sizeof(data_txt), "%s>%s,TCPIP*:%s", my_callsign,
            VERSIONFRM, message);
    if (debug_level & 2)
        fprintf(stderr,"output_my_data: Transmitting and decoding: %s\n", data_txt);

    if (log_net_data)
        log_data(LOGFILE_NET,(char *)data_txt);


    // Note that this will only log one TNC line per transmission now matter
    // how many TNC's are defined.  It's a representative sample of what we're
    // sending out.  At least one TNC interface must be enabled in order to
    // have anything output to the log file here.
    if (data_txt_save[0] != '\0') {
        xastir_snprintf(data_txt, sizeof(data_txt), "%s%s", data_txt_save, message);
        if (log_tnc_data)
            log_data(LOGFILE_TNC,(char *)data_txt);
    }


    // Decode our own transmitted packets.
    // Note that this function call is destructive to the first parameter.
    // This is why we call it _after_ we call the log_data functions.
    //
    // This must be the "L" packet we see in the View->Messages
    // dialog.  We don't see a "T" packet (for TNC) and we only see
    // "I" packets if we re-receive our own packet from the internet
    // feeds.
    decode_ax25_line( data_txt, DATA_VIA_LOCAL, port, 1);

//fprintf(stderr,"Data_txt:%s\n", data_txt);
}





//*****************************************************************************
// output_waypoint_data()
//
// message: the message data to send
//
// This function sends out waypoint creation strings to GPS
// interfaces capable of dealing with it.
//
//*****************************************************************************
void output_waypoint_data(char *message) {
    char data_txt[MAX_LINE_SIZE+5];
    char data_txt_save[MAX_LINE_SIZE+5];
    int ok, start, finish, i;

    if (debug_level & 1)
        fprintf(stderr,"Sending to GPS interfaces: %s\n", message);

    if (message == NULL)
        return;

    if (message[0] == '\0')
        return;
 
    data_txt_save[0] = '\0';

    start = 0;
    finish = MAX_IFACE_DEVICES;

begin_critical_section(&devices_lock, "interface.c:output_waypoint_data" );

    for (i = start; i < finish; i++) {
        ok = 1;
        switch (port_data[i].device_type) {

            case DEVICE_SERIAL_TNC_HSP_GPS:
                port_dtr(i,1);   // make DTR active (select GPS)
                break;

            case DEVICE_SERIAL_GPS:
                break;

            default: /* unknown */
                ok = 0;
                break;
        } // End of switch

        if (ok) {   // Found a GPS interface
            /* send data */
            xastir_snprintf(data_txt, sizeof(data_txt), "%s\r\n", message);

            if (port_data[i].status == DEVICE_UP) {
                port_write_string(i,data_txt);
                usleep(250000);    // 0.25 secs
 
                if (debug_level & 1)
                    fprintf(stderr,"Sending to interface:%d, %s\n",i,data_txt);
            }

            if (debug_level & 2)
                fprintf(stderr,"TX:%d<%s>\n",i,data_txt);

            if (port_data[i].device_type == DEVICE_SERIAL_TNC_HSP_GPS) {
                port_dtr(i,0);  // make DTR inactive (select TNC data)
            }
        }
    }

end_critical_section(&devices_lock, "interface.c:output_waypoint_data" );

}





// Added by KB6MER for KAM XL(SERIAL_TNC_AUX_GPS) support
// buf is a null terminated string
// returns buf as a null terminated string after cleaning.
// Currently:
//    removes leading 'cmd:' prompts from TNC if needed
// Can be used to add any additional data cleaning functions desired.
// Currently only called for SERIAL_TNC_AUX_GPS, but could be added
// to other device routines to improve packet decode on other devices.
//
// Note that the length of "buf" can be up to MAX_DEVICE_BUFFER,
// which is currently set to 4096.
//
void tnc_data_clean(char *buf) {

    if (debug_level & 1) {
        char filtered_data[MAX_LINE_SIZE+1];

        // strncpy is ok here as long as nulls not in data.  We
        // null-terminate it ourselves to make sure it is terminated.
        strncpy(filtered_data, buf, MAX_LINE_SIZE);
        filtered_data[MAX_LINE_SIZE] = '\0';    // Terminate it

        makePrintable(filtered_data);
        fprintf(stderr,"tnc_data_clean: called to clean %s\n", filtered_data);
    }

    while (!strncmp(buf,"cmd:",4)) {
        strcpy(buf,&buf[4]);
    }

    if (debug_level & 1) {
        char filtered_data[MAX_LINE_SIZE+1];

        // Binary routine.  strncpy is ok here as long as nulls not
        // in data.  We null-terminate it ourselves to make sure it
        // is terminated.
        strncpy(filtered_data, buf, MAX_LINE_SIZE);
        filtered_data[MAX_LINE_SIZE] = '\0';    // Terminate it

        makePrintable(filtered_data);
        fprintf(stderr,"tnc_data_clean: clean result %s\n", filtered_data);
    }
}





// Added by KB6MER for KAM XL (SERIAL_TNC_AUX_GPS) support
// buf is a null terminated string.
// port is integer offset into port_data[] array of iface data (see interface.h)
// returns int 0=AX25, 1=GPS
// Tries to guess from the contents of buf whether it represents data from
// the GPS or data from an AX25 packet.
//
// Note that the length of "buf" can be up to MAX_DEVICE_BUFFER,
// which is currently set to 4096.
//
int tnc_get_data_type(char *buf, int port) {
    register int i;
    int type=1;      // Don't know what it is yet.  Assume NMEA for now.

    if (debug_level & 1) {
        char filtered_data[MAX_LINE_SIZE+1];

        // Binary routine.  strncpy is ok here as long as nulls not
        // in data.  We null-terminate it ourselves to make sure it
        // is terminated.
        strncpy(filtered_data, buf, MAX_LINE_SIZE);
        filtered_data[MAX_LINE_SIZE] = '\0';    // Terminate it

        makePrintable(filtered_data);
        fprintf(stderr,"tnc_get_data_type: parsing %s\n", filtered_data);
    }

    // First, let's look for NMEA-ish things.
    if (buf[0]=='$') {
        //This looks kind of NMEA-ish, let's check for known message type
        //headers ($P[A-Z][A-Z][A-Z][A-Z] or $GP[A-Z][A-Z][A-Z])
        if(buf[1]=='P') {
            for(i=2; i<=5; i++) {
                if (buf[i]<'A' || buf[i]>'Z') {
                    type=0; // Disqualified, not valid NMEA-0183
                    if (debug_level & 1) {
                        char filtered_data[MAX_LINE_SIZE+1];

                        // Binary routine.  strncpy is ok here as
                        // long as nulls not in data.  We
                        // null-terminate it ourselves to make
                        // sure it is terminated.
                        strncpy(filtered_data, buf, MAX_LINE_SIZE);
                        filtered_data[MAX_LINE_SIZE] = '\0';    // Terminate it

                        makePrintable(filtered_data);
                        fprintf(stderr,"tnc_get_data_type: Not NMEA %s\n",
                            filtered_data);
                    }
                }
            }
        }
        else if(buf[1]=='G' && buf[2]=='P') {
            for(i=3; i<=5; i++) {
                if (buf[i]<'A' || buf[i]>'Z') {
                    type=0; // Disqualified, not valid NMEA-0183
                    if (debug_level & 1) {
                        char filtered_data[MAX_LINE_SIZE+1];

                        // Binary routine.  strncpy is ok here as
                        // long as nulls not in data.  We
                        // null-terminate it ourselves to make
                        // sure it is terminated.
                        strncpy(filtered_data, buf, MAX_LINE_SIZE);
                        filtered_data[MAX_LINE_SIZE] = '\0';    // Terminate it

                        makePrintable(filtered_data);
                        fprintf(stderr,"tnc_get_data_type: Not NMEA %s\n",
                            filtered_data);
                    }
                }
            }
        }
    }
    else {  // Must be APRS data
        type = 0;
    }

    if (debug_level & 1) {
        if (type == 0)
            fprintf(stderr,"APRS data\n");
        else
            fprintf(stderr,"NMEA data\n");
    }

    return(type);
}


