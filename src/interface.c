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

#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif  // HAVE_NETDB_H

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/time.h>
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
#include "db.h"
#include "interface.h"
#include "main.h"
#include "util.h"
#include "wx.h"
#include "hostname.h"

#ifdef HAVE_AX25
#include <netax25/ax25.h>
#include <netrose/rose.h>
#include <netax25/axlib.h>
#include <netax25/axconfig.h>
#endif  // HAVE_AX25

#ifndef SIGRET
#define SIGRET  void
#endif  // SIGRET


//extern pid_t getpgid(pid_t pid);

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

    //fprintf(stderr,"channel_data: %x %d\n",string[0],length);

    // Save a backup copy of the incoming string.  Used for
    // debugging purposes.  If we get a segfault, we can print out
    // the last message received.
    xastir_snprintf(incoming_data_copy,
        MAX_LINE_SIZE,
        "%s",
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

// This protects channel_data from being run by more
// than one thread at the same time
    if (begin_critical_section(&output_data_lock, "interface.c:channel_data(1)" ) > 0)
        fprintf(stderr,"output_data_lock, Port = %d\n", port);

    if (length > 0) {

        if (begin_critical_section(&data_lock, "interface.c:channel_data(2)" ) > 0)
            fprintf(stderr,"data_lock, Port = %d\n", port);

        incoming_data = string;
        incoming_data_length = length;
        data_port = port;
        data_avail = 1;
        //fprintf(stderr,"data_avail\n");

        if (end_critical_section(&data_lock, "interface.c:channel_data(3)" ) > 0)
            fprintf(stderr,"data_lock, Port = %d\n", port);

        if (debug_level & 1)
            fprintf(stderr,"Channel data on Port %d [%s]\n",port,(char *)string);

        /* wait until data is processed */
        while (data_avail && max < 5400) {
            tmv.tv_sec = 0;
            tmv.tv_usec = 100;
            (void)select(0,NULL,NULL,NULL,&tmv);
            max++;
        }
    }

    if (end_critical_section(&output_data_lock, "interface.c:channel_data(4)" ) > 0)
        fprintf(stderr,"output_data_lock, Port = %d\n", port);
}





//********************************* START AX.25 ********************************

#ifdef HAVE_AX25
// stolen from libax25-0.0.9 and modified to set digipeated bit based on '*'
int my_ax25_aton_arglist(char *call[], struct full_sockaddr_ax25 *sax)
{
    char *bp;
    char *addrp;
    int n = 0;
    int argp = 0;

    addrp = sax->fsa_ax25.sax25_call.ax25_call;

    do {
	/* Fetch one callsign token */
	if ((bp = call[argp++]) == NULL)
	    break;

	/* Check for the optional 'via' syntax */
	if (n == 1 && (strcasecmp(bp, "V") == 0 || strcasecmp(bp, "VIA") == 0))
	    continue;

	/* Process the token */
	if (ax25_aton_entry(bp, addrp) == -1)
	    return -1;
	else {
	    if (n >= 1 && index(bp, '*'))
		addrp[6] |= 0x80; // set digipeated bit
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
#endif  // HAVE_AX25


//***********************************************************
// ui connect: change call and proto paths and reconnect net
// port device to work with
//***********************************************************
int ui_connect( int port, char *to[]) {
    int    s = -1;
#ifdef HAVE_AX25
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

    if (bind(s, (struct sockaddr *)&axbind, addrlen) != 0) {
        xastir_snprintf(temp, sizeof(temp), langcode("POPEM00010"), strerror(errno));
        popup_message(langcode("POPEM00004"),temp);
        return -1;
    }

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
#endif /* HAVE_AX25 */
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





//***********************************************************
// process_ax25_packet()
//
// bp       raw packet data
// len      length of raw packet data
// buffer   buffer to write readable packet data to
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

    bp++;
    len--;

    if (!bp || !len)
        return(NULL);

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

    if(*bp != (unsigned char)0xF0)
        return(NULL); /* check the the PID */

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

#ifdef HAVE_AX25
    int proto = PF_AX25;
    char temp[200];
    char *dev = NULL;
#endif  // HAVE_AX25

    if (begin_critical_section(&port_data_lock, "interface.c:ax25_init(1)" ) > 0)
        fprintf(stderr,"port_data_lock, Port = %d\n", port);

    /* clear port_channel */
//    port_data[port].channel = -1;

    /* clear port active */
    port_data[port].active = DEVICE_NOT_IN_USE;

    /* clear port status */
    port_data[port].status = DEVICE_DOWN;

#ifdef HAVE_AX25
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
#else /* HAVE_AX25 */
    fprintf(stderr,"AX.25 support not compiled into Xastir!\n");
    popup_message(langcode("POPEM00004"),langcode("POPEM00021"));
#endif /* HAVE_AX25 */
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

    if (filename == NULL)
        return(-1);

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
        (void)ioctl(port_data[port].channel, TIOCMGET, &sg);
#endif  // TIOCMGET
        sg &= 0xff;
#ifdef TIOCM_DTR
        sg = TIOCM_DTR;
#endif  // TIOCM_DIR
        if (dtr) {
            dtr &= ~sg;
#ifdef TIOCMBIC
            (void)ioctl(port_data[port].channel, TIOCMBIC, &sg);
#endif  // TIOCMBIC
            if (debug_level & 2)
                fprintf(stderr,"Down\n");

            // statusline(langcode("BBARSTA026"),1);

        } else {
            dtr |= sg;
#ifdef TIOCMBIS
            (void)ioctl(port_data[port].channel, TIOCMBIS, &sg);
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

    for (i = 0; i < MAX_IFACE_DEVICES; i++)
        if (port_data[i].device_type == DEVICE_SERIAL_TNC_HSP_GPS && port_data[i].status == DEVICE_UP)
            port_dtr(i,dtr);


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
            if (fscanf(lock,"%d %s %s",&myintpid,temp,temp1) == 3) {
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

#ifdef __solaris__
    char flag;
#else   // __solaris__
    int flag;
#endif  // __solaris__

    //int stat;
    struct sockaddr_in address;


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
        (void)setsockopt(port_data[port].channel, SOL_SOCKET, SO_KEEPALIVE, &flag, sizeof(int));
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

            usleep(300);
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

        usleep(300);
        //pthread_testcancel();   // Check for thread termination request
        //stat = close(port_data[port].channel);
        //pthread_testcancel();   // Check for thread termination request
        //if (debug_level & 2)
        //    fprintf(stderr,"net_connect_thread():Net Close 2 Returned %d, port %d\n",stat,port);

        //if (debug_level & 2)
        //    fprintf(stderr,"net_connect_thread():Could not bind socket, port %d\n",port);
    }

    if (begin_critical_section(&connect_lock, "interface.c:net_connect_thread(2)" ) > 0)
        fprintf(stderr,"net_connect_thread():connect_lock, Port = %d\n", port);

    port_data[port].connect_status = ok;
    port_data[port].thread_status = 0;

    if (end_critical_section(&connect_lock, "interface.c:net_connect_thread(3)" ) > 0)
        fprintf(stderr,"net_connect_thread():connect_lock, Port = %d\n", port);

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
                (void)sscanf(ip_addrs,"%s",ip_addr);
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
                usleep(300);
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


    // Change callsign to upper-case and pad out to six places with
    // space characters.
    for (i = 0; i < strlen(data); i++) {
        toupper(data[i]);

        if (data[i] == '-') {   // Stop at '-'
            break;
        }
        else {
            new_call[j++] = data[i];
        }
    }
    new_call[7] = '\0';

    //fprintf(stderr,"new_call:(%s)\n",new_call);

    // Handle SSID.  'i' should now be pointing at a dash or at the
    // terminating zero character.
    if ( (i < strlen(data)) && (data[i++] == '-') ) {   // We might have an SSID
        if (data[i] != '\0')
            ssid = data[i++] - 0x30;
        if (data[i] != '\0')
            ssid = (ssid * 10) + data[i] - 0x30;
    }

    //fprintf(stderr,"SSID:%d\n",ssid);

    if (ssid >= 0 && ssid <= 15) {
        new_call[6] = ssid | 0x30;  // Set 2 reserved bits
    }
    else {  // Whacko SSID.  Set it to zero
        new_call[6] = 0x30;     // Set 2 reserved bits
    }

    // Check for an asterisk, which means it's been digipeated through
    // the callsign already
    if (strstr(data,"*") != 0) {
        new_call[6] = new_call[6] | 0x80; // Set the 'H' bit
    }

    // Shift each byte one bit to the left
    for (i = 0; i < 7; i++)
        new_call[i] = new_call[i] << 1;

    // Write over the top of the input string with the newly
    // formatted callsign
    strcpy(data,new_call);
}





// WE7U
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

    for (i = 0; i < strlen(transmit_txt); i++) {
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





// WE7U
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

    if (begin_critical_section(&port_data[port].write_lock, "interface.c:send_ax25_frame(1)" ) > 0)
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

    if (end_critical_section(&port_data[port].write_lock, "interface.c:send_ax25_frame(2)" ) > 0)
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

#ifdef __solaris__
    unsigned int    from_len;
#else   // __solaris__
  #ifndef socklen_t
    int             from_len;
  #else // socklen_t
    socklen_t       from_len;
  #endif    // socklen_t
#endif  // __solaris__

#ifdef HAVE_AX25
    char           *dev;
#endif /* USE_AX25 */

    if (debug_level & 2)
        fprintf(stderr,"Port %d read start\n",port);

//    init_critical_section(&port_data[port].read_lock);

    group = 0;
    max = MAX_DEVICE_BUFFER - 1;
    cin = (unsigned char)0;
    last = (unsigned char)0;
    while(port_data[port].active == DEVICE_IN_USE){
        if (port_data[port].status == DEVICE_UP){
            port_data[port].read_in_pos = 0;
            port_data[port].scan = 1;
            while (port_data[port].scan
                    && (port_data[port].read_in_pos < (MAX_DEVICE_BUFFER - 1) )
                    && (port_data[port].status == DEVICE_UP) ) {
                int skip = 0;

                // Handle all except AX25_TNC interfaces here
                if (port_data[port].device_type != DEVICE_AX25_TNC) {
                    pthread_testcancel();   // Check for thread termination request
                    // Get one character
                    port_data[port].scan = (int)read(port_data[port].channel,&cin,1);
//fprintf(stderr,"%c",cin);
                    pthread_testcancel();   // Check for thread termination request
                }

                else {  // Handle AX25_TNC interfaces
                    /*
                    * Use recvfrom on a network socket to know from which interface
                    * the packet came - PE1DNN
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

                    pthread_testcancel();   // Check for thread termination request
                    port_data[port].scan = recvfrom(port_data[port].channel,buffer,
                        sizeof(buffer) - 1,
                        0,
                        &from,
                        &from_len);
                    pthread_testcancel();   // Check for thread termination request
                }

                if (port_data[port].scan > 0 && port_data[port].status == DEVICE_UP ) {

                    if (port_data[port].device_type != DEVICE_AX25_TNC)
                        port_data[port].bytes_input += port_data[port].scan;      // Add character to read buffer


// Somewhere between these lock statements the read_lock got unlocked.  How?
// if (begin_critical_section(&port_data[port].read_lock, "interface.c:port_read(1)" ) > 0)
//    fprintf(stderr,"read_lock, Port = %d\n", port);

                    // Handle all except AX25_TNC interfaces here
                    if (port_data[port].device_type != DEVICE_AX25_TNC){


                        // Do special KISS packet processing here.  We save
                        // the last character in port_data[port].channel2,
                        // as it is otherwise only used for AX.25 ports.

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

                                // Save this char for next time around
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

                            // End serial/net type data send it to the decoder
                            // Put a terminating zero at the end of the read-in data
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
                    } else {    /* process ax25 data and send to the decoder */
                        /*
                        * Only accept data from our own interface (recvfrom will get
                        * data from all AX.25 interfaces!) - PE1DNN
                        */
#ifdef HAVE_AX25
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
#endif /* HAVE_AX25 */
                    }

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
            /*usleep(100);*/
            FD_ZERO(&rd);
            FD_SET(port_data[port].channel, &rd);
            tmv.tv_sec = 0;
            tmv.tv_usec = 100000;
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
// characters to and interface.  One copy of this is run for
// each write thread for each interface.
//***********************************************************
void port_write(int port) {
    int retval;
    struct timeval tmv;
    fd_set wd;
    int wait_max;
    unsigned long bytes_input;

    if (debug_level & 2)
        fprintf(stderr,"Port %d write start\n",port);

    init_critical_section(&port_data[port].write_lock);

    while(port_data[port].active == DEVICE_IN_USE) {
        if (port_data[port].status == DEVICE_UP) {

            if (begin_critical_section(&port_data[port].write_lock, "interface.c:port_write(1)" ) > 0)
                fprintf(stderr,"write_lock, Port = %d\n", port);

            if (port_data[port].write_in_pos != port_data[port].write_out_pos && port_data[port].status == DEVICE_UP) {
                if (port_data[port].device_write_buffer[port_data[port].write_out_pos] == (char)0x03) {
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
                }
                pthread_testcancel();   // Check for thread termination request
                retval = (int)write(port_data[port].channel,
                    port_data[port].device_write_buffer + port_data[port].write_out_pos,
                    1);
                pthread_testcancel();   // Check for thread termination request
                if (retval == 1) {
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
                switch (port_data[port].device_type) {
                    case DEVICE_SERIAL_TNC_HSP_GPS:
                    case DEVICE_SERIAL_TNC_AUX_GPS:
                    case DEVICE_SERIAL_KISS_TNC:
                    case DEVICE_SERIAL_TNC:
                        usleep(25000); // character pacing, 25ms per char.  20ms doesn't work for PicoPacket.
                        break;
                    default:
                        break;
                }

            }

            if (end_critical_section(&port_data[port].write_lock, "interface.c:port_write(2)" ) > 0)
                fprintf(stderr,"write_lock, Port = %d\n", port);

        }
        if (port_data[port].active == DEVICE_IN_USE) {
            /*usleep(100);*/
            FD_ZERO(&wd);
            FD_SET(port_data[port].channel, &wd);
            tmv.tv_sec = 0;
            tmv.tv_usec = 100000;  // Delay 100ms
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
                    //(void)sleep(3);
                    usleep(3000000);    // 3secs
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
                    //(void)sleep(3);
                    usleep(3000000);    // 3secs
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
                    usleep(3000000);
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
            switch (port_data[port].device_type){
                case DEVICE_NET_STREAM:
                    if (debug_level & 2)
                        fprintf(stderr,"Close a Network stream\n");

                    break;

                case DEVICE_AX25_TNC:
                    if (debug_level & 2)
                        fprintf(stderr,"Close a AX25 TNC device\n");

                    //(void)sleep(3);
                    //usleep(3000000);  // 3secs
                    break;

                case DEVICE_NET_GPSD:
                    if (debug_level & 2)
                        fprintf(stderr,"Close a Network GPSd device\n");
                        if (using_gps_position) {
                            using_gps_position--;
                        }

                    break;

                case DEVICE_NET_WX:
                    if (debug_level & 2)
                        fprintf(stderr,"Close a Network WX\n");

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

        //(void)sleep(1);
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

        //(void)sleep(1);
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
    char logon_txt[200];
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
                        my_position_valid = 0;  // Must wait for valid GPS parsing
                        using_gps_position++;
                        statusline(langcode("BBARSTA041"),1);
//fprintf(stderr,"my_position_valid = 0, using_gps_position:%d\n",using_gps_position);
 
                        break;

                    case DEVICE_SERIAL_WX:
                        if (debug_level & 2)
                            fprintf(stderr,"Opening a Serial WX device\n");

                        break;

                    case DEVICE_SERIAL_TNC_HSP_GPS:
                        if (debug_level & 2)
                            fprintf(stderr,"Opening a Serial TNC w/HSP GPS device\n");
                        my_position_valid = 0;  // Must wait for valid GPS parsing
                        using_gps_position++;
                        statusline(langcode("BBARSTA041"),1);
//fprintf(stderr,"my_position_valid = 0, using_gps_position:%d\n",using_gps_position);
 
                        break;

                    case DEVICE_SERIAL_TNC_AUX_GPS:
                        if (debug_level & 2)
                            fprintf(stderr,"Opening a Serial TNC w/AUX GPS device\n");
                        my_position_valid = 0;  // Must wait for valid GPS parsing
                        using_gps_position++;
                        statusline(langcode("BBARSTA041"),1);
//fprintf(stderr,"my_position_valid = 0, using_gps_position:%d\n",using_gps_position);
 
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
                    my_position_valid = 0;  // Must wait for valid GPS parsing
                    using_gps_position++;
                    statusline(langcode("BBARSTA041"),1);
//fprintf(stderr,"my_position_valid = 0, using_gps_position:%d\n",using_gps_position);
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

            default:
                break;
        }

        if (ok == 1) {  // If port is connected...

            if (debug_level & 2)
                fprintf(stderr,"*** add_device: ok: %d ***\n",ok);

            /* if all is ok check and start read write threads */
            (void)start_port_threads(port_avail);
            usleep(500);

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
    int count;
    int done;
    int temp;
    int bump_up;


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

            case DEVICE_NET_STREAM:

                xastir_snprintf(output_net, sizeof(output_net), "%s>%s,TCPIP*:", my_callsign, VERSIONFRM);
                break;

            case DEVICE_SERIAL_TNC_HSP_GPS:

                /* make dtr normal */
                port_dtr(port,0);

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

// WE7U:  Should we check here to make sure that there are printable
// characters in the path?  Also:  Output_aprs_data() has nearly
// identical path selection code.  Fix it in one place, fix it in
// the other.

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
                                xastir_snprintf(header_txt, sizeof(header_txt), "%c%s %s VIA %s\r", '\3', "UNPROTO", VERSIONFRM, devices[port].unproto1);
                                xastir_snprintf(header_txt_save, sizeof(header_txt_save), "%s>%s,%s:", my_callsign, VERSIONFRM, devices[port].unproto1);
                                xastir_snprintf(path_txt,sizeof(path_txt),"%s",devices[port].unproto1);
                                done++;
                            }
                            else {
                                // No path entered here.  Skip this path in the rotation for next time.
                                bump_up++;
                            }
                            break;
                        case 1:
                            if (strlen(devices[port].unproto2) > 0) {
                                xastir_snprintf(header_txt, sizeof(header_txt), "%c%s %s VIA %s\r", '\3', "UNPROTO", VERSIONFRM, devices[port].unproto2);
                                xastir_snprintf(header_txt_save, sizeof(header_txt_save), "%s>%s,%s:",my_callsign,VERSIONFRM,devices[port].unproto2);
                                xastir_snprintf(path_txt,sizeof(path_txt),"%s",devices[port].unproto2);
                                done++;
                            }
                            else {
                                // No path entered here.  Skip this path in the rotation for next time.
                                bump_up++;
                            }
                            break;
                        case 2:
                            if (strlen(devices[port].unproto3) > 0) {
                                xastir_snprintf(header_txt, sizeof(header_txt), "%c%s %s VIA %s\r", '\3', "UNPROTO", VERSIONFRM, devices[port].unproto3);
                                xastir_snprintf(header_txt_save, sizeof(header_txt_save), "%s>%s,%s:", my_callsign, VERSIONFRM, devices[port].unproto3);
                                xastir_snprintf(path_txt,sizeof(path_txt),"%s",devices[port].unproto3);
                                done++;
                            }
                            else {
                                // No path entered here.  Skip this path in the rotation for next time.
                                bump_up++;
                            }
                            break;
                    }   // End of switch
                    count++;
                }   // End of while loop
                if (!done) {   // We found no entries in the unproto fields for the interface.

                    xastir_snprintf(header_txt,
                        sizeof(header_txt),
                        "%c%s %s VIA %s\r",
                        '\3',
                        "UNPROTO",
                        VERSIONFRM,
                        "WIDE,WIDE2-2");

                    xastir_snprintf(header_txt_save,
                        sizeof(header_txt_save),
                        "%s>%s,%s:",
                        my_callsign,
                        VERSIONFRM,
                        "WIDE,WIDE2-2");

                    xastir_snprintf(path_txt,
                        sizeof(path_txt),
                        "WIDE,WIDE2-2");

                }
                // Increment the path number for the next round of transmissions.
                // This will round-robin the paths so that all entered paths get used.
                devices[port].unprotonum = (devices[port].unprotonum + 1 + bump_up) % 3;


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
                /* port transmitting off message, Shut off for now -FG */
                /*sprintf(temp,langcode("POPEM00019"),port);
                popup_message(langcode("POPEM00004"),temp);*/
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
    char output_net[100];
    int ok, start, finish, i;
    /*char temp[150]; for port message -FG */
    int count;
    int done;
    int temp;
    int bump_up;

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
                    if (!transmit_disable
                            && port_data[i].status == DEVICE_UP
                            && devices[i].transmit_data == 1
                            && !loopback_only) {
                        port_dtr(i,0);           // make DTR normal
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
                        usleep(100000);  // 100ms
                    }
 
                    count = 0;
                    done = 0;
                    bump_up = 0;
 
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

                    // We look for a non-null path entry starting at the current
                    // value of "unprotonum".  The first non-null path wins.
                    while (!done && (count < 3)) {
                        temp = (devices[i].unprotonum + count) % 3;
                        switch (temp) {
                            case 0:
                                if (strlen(devices[i].unproto1) > 0) {

                                    xastir_snprintf(data_txt,
                                            sizeof(data_txt),
                                            "%c%s %s VIA %s\r",
                                            '\3',
                                            "UNPROTO",
                                            VERSIONFRM,
                                            devices[i].unproto1);

                                    xastir_snprintf(data_txt_save,
                                            sizeof(data_txt_save),
                                            "%s>%s,%s:",
                                            my_callsign,
                                            VERSIONFRM,
                                            devices[i].unproto1);

                                    xastir_snprintf(path_txt,
                                        sizeof(path_txt),
                                        "%s",
                                        devices[i].unproto1);

                                    done++;
                                }
                                else {
                                    // No path entered here.  Skip this path in the rotation for next time.
                                    bump_up++;
                                }
                                break;
                            case 1:
                                if (strlen(devices[i].unproto2) > 0) {

                                    xastir_snprintf(data_txt,
                                        sizeof(data_txt),
                                        "%c%s %s VIA %s\r",
                                        '\3',
                                        "UNPROTO",
                                        VERSIONFRM,
                                        devices[i].unproto2);

                                    xastir_snprintf(data_txt_save,
                                        sizeof(data_txt_save),
                                        "%s>%s,%s:",
                                        my_callsign,
                                        VERSIONFRM,
                                        devices[i].unproto2);

                                    xastir_snprintf(path_txt,
                                        sizeof(path_txt),
                                        "%s",
                                        devices[i].unproto2);

                                    done++;
                                }
                                else {
                                    // No path entered here.  Skip this path in the rotation for next time.
                                    bump_up++;
                                }
                                break;
                            case 2:
                                if (strlen(devices[i].unproto3) > 0) {

                                    xastir_snprintf(data_txt,
                                        sizeof(data_txt),
                                        "%c%s %s VIA %s\r",
                                        '\3',
                                        "UNPROTO",
                                        VERSIONFRM,
                                        devices[i].unproto3);

                                    xastir_snprintf(data_txt_save,
                                        sizeof(data_txt_save),
                                        "%s>%s,%s:",
                                        my_callsign,
                                        VERSIONFRM,
                                        devices[i].unproto3);

                                    xastir_snprintf(path_txt,
                                        sizeof(path_txt),
                                        "%s",
                                        devices[i].unproto3);

                                    done++;
                                }
                                else {
                                    // No path entered here.  Skip this path in the rotation for next time.
                                    bump_up++;
                                }
                                break;
                        }   // End of switch
                        count++;
                    }   // End of while loop
                    if (!done) {    // We found no entries in the unproto fields for the interface

                        xastir_snprintf(data_txt,
                            sizeof(data_txt),
                            "%c%s %s VIA %s\r",
                            '\3',
                            "UNPROTO",
                            VERSIONFRM,
                            "WIDE,WIDE2-2");

                        xastir_snprintf(data_txt_save,
                            sizeof(data_txt_save),
                            "%s>%s,%s:",
                            my_callsign,
                            VERSIONFRM,
                            "WIDE,WIDE2-2");

                        xastir_snprintf(path_txt,
                            sizeof(path_txt),
                            "WIDE,WIDE2-2");

                    }
                    // Increment the path number for the next round of transmissions.
                    // This will round-robin the paths so that all entered paths get used.
                    devices[i].unprotonum = (devices[i].unprotonum + 1 + bump_up) % 3;

                    if ( (port_data[i].device_type != DEVICE_SERIAL_KISS_TNC)
                            && (port_data[i].status == DEVICE_UP)
                            && (devices[i].transmit_data == 1)
                            && !transmit_disable
                            && !loopback_only) {
                        port_write_string(i,data_txt);
                        usleep(100000);  // 100ms
                    }
 
                    // Set converse mode
                    xastir_snprintf(data_txt, sizeof(data_txt), "%c%s\r", '\3', "CONV");

                    if ( (port_data[i].device_type != DEVICE_SERIAL_KISS_TNC)
                            && (port_data[i].status == DEVICE_UP)
                            && (devices[i].transmit_data == 1)
                            && !transmit_disable
                            && !loopback_only) {
                        port_write_string(i,data_txt);
                        usleep(1500000);    // 1.5 secs
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
                /* port transmitting off message, Shut off for now -FG */
                /*sprintf(temp,langcode("POPEM00019"),port);
                popup_message(langcode("POPEM00004"),temp);*/
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
    register int i;

    if (debug_level & 1) {
        char filtered_data[MAX_LINE_SIZE+1];

        strncpy(filtered_data, buf, MAX_LINE_SIZE);
        filtered_data[MAX_LINE_SIZE] = '\0';    // Terminate it

        makePrintable(filtered_data);
        fprintf(stderr,"tnc_data_clean: called to clean %s\n", filtered_data);
    }

    while (buf[0]=='c' && buf[1]=='m' && buf[2]=='d' && buf[3]==':') {
        for(i=4; i<strlen(buf); i++) {
            buf[i-4]=buf[i];
        }
        buf[i++]=0;     //Null out any remaining old data just in case
        buf[i++]=0;
        buf[i++]=0;
        buf[i++]=0;
    }

    if (debug_level & 1) {
        char filtered_data[MAX_LINE_SIZE+1];

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


