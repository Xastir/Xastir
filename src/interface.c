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

/*
  AX.25 Parts adopted from: aprs_tty.c by Henk de Groot - PE1DNN
*/



#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif  // HAVE_CONFIG_H

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
  #include <sys/time.h>
  #include <time.h>
#else   // TIME_WITH_SYS_TIME
  #if HAVE_SYS_TIME_H
    #include <sys/time.h>
  #else  // HAVE_SYS_TIME_H
    #include <time.h>
  #endif // HAVE_SYS_TIME_H
#endif  // TIME_WITH_SYS_TIME

#include <errno.h>

#ifdef  HAVE_LOCALE_H
  #include <locale.h>
#endif  // HAVE_LOCALE_H

#include <Xm/XmAll.h>

#include "xastir.h"
#include "symbols.h"
#include "main.h"
#include "xa_config.h"
//#include "maps.h"
#include "interface.h"
#include "util.h"
#include "wx.h"
#include "forked_getaddrinfo.h"
#include "x_spider.h"
#include "db_gis.h"
#include "gps.h"

#ifdef HAVE_LIBAX25
  #include <netax25/ax25.h>
  #include <netrose/rose.h>
  #include <netax25/axlib.h>
  #include <netax25/axconfig.h>
#endif  // HAVE_LIBAX25

// Must be last include file
#include "leak_detection.h"



#ifndef SIGRET
  #define SIGRET  void
#endif  // SIGRET

// Older versions of glibc <= 2.3.0 and <= OS X 10.5 do not have this
// constant defined
#ifndef AI_NUMERICSERV
  #define AI_NUMERICSERV 0
#endif

//extern pid_t getpgid(pid_t pid);
extern void port_write_binary(int port, unsigned char *data, int length);


iodevices dtype[MAX_IFACE_DEVICE_TYPES]; // device names

iface port_data[MAX_IFACE_DEVICES];     // shared port data

int port_id[MAX_IFACE_DEVICES];         // shared port id data

xastir_mutex port_data_lock;            // Protects the port_data[] array of structs
xastir_mutex data_lock;                 // Protects incoming_data_queue
xastir_mutex output_data_lock;          // Protects interface.c:channel_data() function only
xastir_mutex connect_lock;              // Protects port_data[].thread_status and port_data[].connect_status

void port_write_string(int port, char *data);

int ax25_ports_loaded = 0;


// Incoming data queue
typedef struct _incoming_data_record
{
  int length;   // Used for binary strings such as KISS
  int port;
  unsigned char data[MAX_LINE_SIZE];
} incoming_data_record;
#define MAX_INPUT_QUEUE 1000
static incoming_data_record incoming_data_queue[MAX_INPUT_QUEUE];
unsigned char incoming_data_copy[MAX_LINE_SIZE];            // Used for debug
unsigned char incoming_data_copy_previous[MAX_LINE_SIZE];   // Used for debug

// interface wait time out
int NETWORK_WAITTIME;





// Read/write pointers for the circular input queue
static int incoming_read_ptr = 0;
static int incoming_write_ptr = 0;
static int queue_depth = 0;
static int push_count = 0;
static int pop_count = 0;





// Fetch a record from the circular queue.
// Returns 0 if no records available
// Else returns length of string, data_string and port
// data_string variable should be of size MAX_LINE_SIZE
//
int pop_incoming_data(unsigned char *data_string, int *port)
{
  int length;
  int jj;

  if (begin_critical_section(&data_lock, "interface.c:pop_incoming_data" ) > 0)
  {
    fprintf(stderr,"data_lock\n");
  }

  // Check for queue empty
  if (incoming_read_ptr == incoming_write_ptr)
  {
    // Yep, it's empty

    queue_depth = 0;

    if (end_critical_section(&data_lock, "interface.c:pop_incoming_data" ) > 0)
    {
      fprintf(stderr,"data_lock\n");
    }
    return(0);
  }

  // Bump the read pointer
  incoming_read_ptr = (incoming_read_ptr + 1) % MAX_INPUT_QUEUE;

  *port = incoming_data_queue[incoming_read_ptr].port;

  length = incoming_data_queue[incoming_read_ptr].length;

  // Yes, this is a string, but it may have zeros embedded.  We can't
  // use string manipulation functions because of that:
  for (jj = 0; jj < length; jj++)
  {
    data_string[jj] = incoming_data_queue[incoming_read_ptr].data[jj];
  }

  // Add terminator, just in case
  data_string[length+1] = '\0';

  queue_depth--;
  pop_count++;

  if (end_critical_section(&data_lock, "interface.c:pop_incoming_data" ) > 0)
  {
    fprintf(stderr,"data_lock\n");
  }

  return(length);
}





// Add one record to the circular queue.  Returns 1 if queue is
// full, 0 if successful.
//
int push_incoming_data(unsigned char *data_string, int length, int port)
{
  int next_write_ptr = (incoming_write_ptr + 1) % MAX_INPUT_QUEUE;
  int jj;


  if (begin_critical_section(&data_lock, "interface.c:push_incoming_data" ) > 0)
  {
    fprintf(stderr,"data_lock\n");
  }

  // Check whether queue is full
  if (incoming_read_ptr == next_write_ptr)
  {
    // Yep, it's full!

    if (end_critical_section(&data_lock, "interface.c:push_incoming_data" ) > 0)
    {
      fprintf(stderr,"data_lock\n");
    }
    return(1);
  }


  // Advance the write pointer
  incoming_write_ptr = next_write_ptr;

  incoming_data_queue[incoming_write_ptr].length = length;

  incoming_data_queue[incoming_write_ptr].port = port;

  // Binary safe copy in case there are embedded zeros
  for (jj = 0; jj < length; jj++)
  {
    incoming_data_queue[incoming_write_ptr].data[jj] = data_string[jj];
  }

  queue_depth++;
  push_count++;

  if (end_critical_section(&data_lock, "interface.c:push_incoming_data" ) > 0)
  {
    fprintf(stderr,"data_lock\n");
  }

  return(0);
}





// Returns 1 if a local interface, 0 otherwise
//
int is_local_interface(int port)
{

  switch (port_data[port].device_type)
  {

    case DEVICE_SERIAL_TNC:
    case DEVICE_SERIAL_TNC_HSP_GPS:
    case DEVICE_SERIAL_GPS:
    case DEVICE_SERIAL_WX:
    case DEVICE_AX25_TNC:
    case DEVICE_SERIAL_TNC_AUX_GPS:
    case DEVICE_SERIAL_KISS_TNC:
    case DEVICE_SERIAL_MKISS_TNC:
      return(1);  // Found a local interface
      break;

    // Could be port -1 which signifies a spider port or port
    // -99 which signifies "All Ports" and is used for
    // transmitting out all ports at once.
    default:
      return(0);  // Unknown or network interface
      break;
  }
}





// Returns 1 if a network interface, 0 otherwise
//
int is_network_interface(int port)
{

  switch (port_data[port].device_type)
  {

    case DEVICE_NET_STREAM:
    case DEVICE_NET_GPSD:
    case DEVICE_NET_WX:
    case DEVICE_NET_DATABASE:
    case DEVICE_NET_AGWPE:
      return(1);  // Found a network interface
      break;

    // Could be port -1 which signifies a spider port or port
    // -99 which signifies "All Ports" and is used for
    // transmitting out all ports at once.
    default:
      return(0);  // Unknown or local interface
      break;
  }
}






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
// We currently use the base portion of my_callsign as the username
// portion of the AGWPE login.  This must be upper-case when you're
// setting up the account in AGWPE, as that's what we send to
// authenticate.
//
void send_agwpe_packet(int xastir_interface,// Xastir interface port
                       int RadioPort,       // AGWPE RadioPort
                       unsigned char type,
                       unsigned char *FromCall,
                       unsigned char *ToCall,
                       unsigned char *Path,
                       unsigned char *Data,
                       int length)
{
  int ii;
#define agwpe_header_size 36
  unsigned char output_string[512+agwpe_header_size];
  unsigned char path_string[200];
  int full_length;
  int data_length;


  // Check size of data
  if (length > 512)
  {
    return;
  }

  // Clear the output_string (set to binary zeroes)
  for (ii = 0; ii < (int)sizeof(output_string); ii++)
  {
    output_string[ii] = '\0';
  }

  if (type != 'P')
  {
    // Write the port number into the frame.  Note that AGWPE
    // uses 1 for the first port in its GUI, but the programming
    // interface starts at 0.
    output_string[0] = (unsigned char)RadioPort;

    if (FromCall)   // Write the FromCall string into the frame
      xastir_snprintf((char *)&output_string[8],
                      sizeof(output_string) - 8,
                      "%s",
                      FromCall);

    if (ToCall) // Write the ToCall string into the frame
      xastir_snprintf((char *)&output_string[18],
                      sizeof(output_string) - 18,
                      "%s",
                      ToCall);
  }

  if ( (type != '\0') && (type != 'P') )
  {
    // Type was specified, not a data frame or login frame

    // Write the type character into the frame
    output_string[4] = type;

    // Send the packet to AGWPE
    port_write_binary(xastir_interface,
                      output_string,
                      agwpe_header_size);
  }

  else if (Path == NULL)   // No ViaCalls, Data or login packet
  {

    if (type == 'P')
    {
      // Login/Password frame
      char callsign_base[15];
      int new_length;


      // Write the type character into the frame
      output_string[4] = type;

      // Compute the callsign base string
      // (callsign minus SSID)
      xastir_snprintf(callsign_base,
                      sizeof(callsign_base),
                      "%s",
                      my_callsign);
      // Change '-' into end of string
      strtok(callsign_base, "-");

      // Length = length of each string plus the two
      // terminating zeroes.
      //new_length = strlen(callsign_base) + length + 2;
      new_length = 255+255;

      output_string[28] = (unsigned char)(new_length % 256);
      output_string[29] = (unsigned char)((new_length >> 8) % 256);

      // Write login/password out as 255-byte strings each

      // Put the login string into the buffer
      xastir_snprintf((char *)&output_string[agwpe_header_size],
                      sizeof(output_string) - agwpe_header_size,
                      "%s",
                      callsign_base);

      // Put the password string into the buffer
      xastir_snprintf((char *)&output_string[agwpe_header_size+255],
                      sizeof(output_string) - agwpe_header_size - 255,
                      "%s",
                      Data);

      // Send the packet to AGWPE
      port_write_binary(xastir_interface,
                        output_string,
                        255+255+agwpe_header_size);
    }
    else    // Data frame
    {
      // Write the type character into the frame
      output_string[4] = 'M'; // Unproto, no via calls

      // Write the PID type into the frame
      output_string[6] = 0xF0;    // UI Frame

      output_string[28] = (unsigned char)(length % 256);
      output_string[29] = (unsigned char)((length >> 8) % 256);

      // Copy Data onto the end of the string.  This one
      // doesn't have to be null-terminated, so strncpy() is
      // ok to use here.  strncpy stops at the first null byte
      // though.  Proper for a binary output routine?  NOPE!
      strncpy((char *)(&output_string[agwpe_header_size]),(char *)Data, length);

      full_length = length + agwpe_header_size;

      // Send the packet to AGWPE
      port_write_binary(xastir_interface,
                        output_string,
                        full_length);

    }
  }

  else    // We have ViaCalls.  Data packet.
  {
    char *ViaCall[10];

    // Doesn't need to be null-terminated, so strncpy is ok to
    // use here.  strncpy stops at the first null byte though.
    // Proper for a binary output routine?  NOPE!
    strncpy((char *)path_string, (char *)Path, sizeof(path_string));

    // Convert path_string to upper-case
    to_upper((char *)path_string);

    split_string((char *)path_string, ViaCall, 10, ',');

    // Write the type character into the frame
    output_string[4] = 'V'; // Unproto, via calls present

    // Write the PID type into the frame
    output_string[6] = 0xF0;    // UI Frame

    // Write the number of ViaCalls into the first byte
    if (ViaCall[7])
    {
      output_string[agwpe_header_size] = 0x08;
    }
    else if (ViaCall[6])
    {
      output_string[agwpe_header_size] = 0x07;
    }
    else if (ViaCall[5])
    {
      output_string[agwpe_header_size] = 0x06;
    }
    else if (ViaCall[4])
    {
      output_string[agwpe_header_size] = 0x05;
    }
    else if (ViaCall[3])
    {
      output_string[agwpe_header_size] = 0x04;
    }
    else if (ViaCall[2])
    {
      output_string[agwpe_header_size] = 0x03;
    }
    else if (ViaCall[1])
    {
      output_string[agwpe_header_size] = 0x02;
    }
    else
    {
      output_string[agwpe_header_size] = 0x01;
    }

    // Write the ViaCalls into the Data field
    switch (output_string[agwpe_header_size])
    {
      case 8:
        if (ViaCall[7])
        {
          strncpy((char *)(&output_string[agwpe_header_size+1+70]), ViaCall[7], 10);
        }
        else
        {
          return;
        }
      /* Falls through. */
      case 7:
        if (ViaCall[6])
        {
          strncpy((char *)(&output_string[agwpe_header_size+1+60]), ViaCall[6], 10);
        }
        else
        {
          return;
        }
      /* Falls through. */
      case 6:
        if (ViaCall[5])
        {
          strncpy((char *)(&output_string[agwpe_header_size+1+50]), ViaCall[5], 10);
        }
        else
        {
          return;
        }
      /* Falls through. */
      case 5:
        if (ViaCall[4])
        {
          strncpy((char *)(&output_string[agwpe_header_size+1+40]), ViaCall[4], 10);
        }
        else
        {
          return;
        }
      /* Falls through. */
      case 4:
        if (ViaCall[3])
        {
          strncpy((char *)(&output_string[agwpe_header_size+1+30]), ViaCall[3], 10);
        }
        else
        {
          return;
        }
      /* Falls through. */
      case 3:
        if (ViaCall[2])
        {
          strncpy((char *)(&output_string[agwpe_header_size+1+20]), ViaCall[2], 10);
        }
        else
        {
          return;
        }
      /* Falls through. */
      case 2:
        if (ViaCall[1])
        {
          strncpy((char *)(&output_string[agwpe_header_size+1+10]), ViaCall[1], 10);
        }
        else
        {
          return;
        }
      /* Falls through. */
      case 1:
      default:
        if (ViaCall[0])
        {
          strncpy((char *)(&output_string[agwpe_header_size+1+0]),  ViaCall[0], 10);
        }
        else
        {
          return;
        }
        break;
    }

    // Write the Data onto the end.
    // Doesn't need to be null-terminated, so strncpy is ok to
    // use here.  strncpy stops at the first null byte though.
    // Proper for a binary output routine?
    strncpy((char *)(&output_string[((int)(output_string[agwpe_header_size]) * 10) + agwpe_header_size + 1]),
            (char *)Data,
            length);

    //Fill in the data length field.  We're assuming the total
    //is less than 512 + 37.
    data_length = length + ((int)(output_string[agwpe_header_size]) * 10) + 1;

    if ( data_length > 512 )
    {
      return;
    }

    output_string[28] = (unsigned char)(data_length % 256);
    output_string[29] = (unsigned char)((data_length >> 8) % 256);

    full_length = data_length + agwpe_header_size;

    // Send the packet to AGWPE
    port_write_binary(xastir_interface,
                      output_string,
                      full_length);


  }
}





/*
// Here is a "monitor" UI packet
//
Total Length = 150
HEX:00 00 00 00 55 00 00 00 4b 4b 31 57 00 ed 12 00 96 ed 41 50 54
57 30 31 00 00 00 00 72 00 00 00 00 00 00 00 20 31 3a 46 6d 20 4b 4b
31 57 20 54 6f 20 41 50 54 57 30 31 20 56 69 61 20 57 49 44 45 33 20
3c 55 49 20 70 69 64 3d 46 30 20 4c 65 6e 3d 35 30 20 3e 5b 31 30 3a
34 33 3a 34 33 5d 0d 5f 30 38 30 36 31 30 33 39 63 33 35 39 73 30 30
30 67 30 30 30 74 30 36 32 72 30 30 30 70 30 30 33 50 30 39 36 68 30
30 62 31 30 30 39 33 74 55 32 6b 0d 0d 00
ASC:....U...KK1W......APTW01....r....... 1:Fm KK1W To APTW01 Via WIDE3 <UI pid=F0 Len=50 >[10:43:43]._08061039c359s000g000t062r000p003P096h00b10093tU2k...
*/

/*
// And here are some "raw" UI packets
//
AGWPE: Got raw frame packet
3:2e
Bad KISS packet.  Dropping it.
Total Length = 135
HEX:00 00 00 00 4b 00 00 00 4e 32 4c 42 54 2d 37 00 01 00 41 50 58
31 33 33 00 00 00 00 63 00 00 00 00 00 00 00

c0 82 a0 b0 62 66 66 60
9c 64 98 84 a8 40 6e 96 82 64 a2 b2 8a f4 ae 92 88 8a 40 40 61 03 f0
40 30 37 30 30 32 37 7a 34 32 33 39 2e 30 34 4e 5c 30 37 33 34 38 2e
31 30 57 5f 30 30 30 2f 30 30 30 67 30 30 30 74 30 36 34 72 30 30 30
50 30 30 30 70 30 30 30 68 35 33 62 31 30 31 32 37 58 55 32 6b 0d
ASC:....K...N2LBT-7...APX133....c...........bff`.d...@n..d........@@a..@070027z4239.04N\07348.10W_000/000g000t064r000P000p000h53b10127XU2k.
AGWPE: Got raw frame packet
3:23
Bad KISS packet.  Dropping it.
Total Length = 104
HEX:00 00 00 00 4b 00 00 00 4e 31 45 44 5a 2d 37 00 01 00 54 52 31
55 37 58 00 00 00 00 44 00 00 00 00 00 00 00

c0 a8 a4 62 aa 6e b0 60
9c 62 8a 88 b4 40 ee 96 82 62 a2 8c 8a fe ae 62 a8 9e 9a 40 fe 96 82
64 a2 b2 8a f5 03 f0 60 64 2a 39 6c 23 22 3e 2f 3e 22 35 6b 7d 6e 31
65 64 7a 40 61 6d 73 61 74 2e 6f 72 67 0d
ASC:....K...N1EDZ-7...TR1U7X....D..........b.n.`.b...@...b.....b...@...d......`d*9l#">/>"5k}n1edz@amsat.org.
AGWPE: Got raw frame packet
3:2e
Bad KISS packet.  Dropping it.
Total Length = 103
HEX:00 00 00 00 4b 00 00 00 4b 32 52 52 54 2d 39 00 01 00 41 50 54
33 31 31 00 00 00 00 43 00 00 00 00 00 00 00

c0 82 a0 a8 66 62 62 60
96 64 a4 a4 a8 40 72 ae 82 64 aa 9a b0 e4 ae 92 88 8a 40 40 61 03 f0
21 34 33 31 39 2e 37 39 4e 2f 30 37 33 34 30 2e 38 37 57 3e 32 36 38
2f 30 32 30 2f 41 3d 30 30 30 34 38 35
ASC:....K...K2RRT-9...APT311....C...........fbb`.d...@r..d........@@a..!4319.79N/07340.87W>268/020/A=000485
AGWPE: Got raw frame packet
3:74
Bad KISS packet.  Dropping it.
Total Length = 82
HEX:00 00 00 00 4b 00 00 00 4b 32 52 52 54 2d 39 00 01 00 41 50 54
33 31 31 00 00 00 00 2e 00 00 00 00 00 00 00

c0 82 a0 a8 66 62 62 60
96 64 a4 a4 a8 40 72 ae 64 8e ae b2 40 e4 ae 82 64 aa 9a b0 e5 03 f0
3e 6e 32 79 71 74 40 61 72 72 6c 2e 6e 65 74
ASC:....K...K2RRT-9...APT311................fbb`.d...@r.d...@...d......>n2yqt@arrl.net
AGWPE: Got raw frame packet
3:2e
Bad KISS packet.  Dropping it.
Total Length = 103
HEX:00 00 00 00 4b 00 00 00 4b 32 52 52 54 2d 39 00 01 00 41 50 54
33 31 31 00 00 00 00 43 00 00 00 00 00 00 00

c0 82 a0 a8 66 62 62 60
96 64 a4 a4 a8 40 72 ae 64 8e ae b2 40 e4 96 82 64 a2 b2 8a f5 03 f0
21 34 33 31 39 2e 37 39 4e 2f 30 37 33 34 30 2e 38 37 57 3e 32 36 38
2f 30 32 30 2f 41 3d 30 30 30 34 38 35
ASC:....K...K2RRT-9...APT311....C...........fbb`.d...@r.d...@...d......!4319.79N/07340.87W>268/020/A=000485
AGWPE: Got raw frame packet
3:74
Bad KISS packet.  Dropping it.
Total Length = 82
HEX:00 00 00 00 4b 00 00 00 4b 32 52 52 54 2d 39 00 01 00 41 50 54
33 31 31 00 00 00 00 2e 00 00 00 00 00 00 00

c0 82 a0 a8 66 62 62 60
96 64 a4 a4 a8 40 72 ae 64 8e ae b2 40 e4 96 82 64 a2 b2 8a f5 03 f0
3e 6e 32 79 71 74 40 61 72 72 6c 2e 6e 65 74
ASC:....K...K2RRT-9...APT311................fbb`.d...@r.d...@...d......>n2yqt@arrl.net
AGWPE: Got raw frame packet
3:2e
Bad KISS packet.  Dropping it.
Total Length = 103
HEX:00 00 00 00 4b 00 00 00 4b 32 52 52 54 2d 39 00 01 00 41 50 54
33 31 31 00 00 00 00 43 00 00 00 00 00 00 00

c0 82 a0 a8 66 62 62 60
96 64 a4 a4 a8 40 72 ae 82 64 aa 9a b0 e4 96 82 64 a2 b2 8a f5 03 f0
21 34 33 31 39 2e 37 39 4e 2f 30 37 33 34 30 2e 38 37 57 3e 32 36 38
2f 30 32 30 2f 41 3d 30 30 30 34 38 35
ASC:....K...K2RRT-9...APT311....C...........fbb`.d...@r..d......d......!4319.79N/07340.87W>268/020/A=000485
*/


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
// output_string variable should be quite long, perhaps 1000
// characters.
//
// Someday it would be nice to turn on raw packet format in AGWPE
// which gives us the AX.25 packet format directly.  We should be
// able to use our normal KISS decoding functions to parse those
// types of packets, instead of the mess we have below which is
// parsing a few things out of the header, a few things out of the
// text that AGWPE puts after the header, and then snagging the info
// field of the packet from the tail-end.
//
unsigned char *parse_agwpe_packet(unsigned char *input_string,
                                  int output_string_length,
                                  unsigned char *output_string,
                                  int *new_length)
{
  int ii, jj, kk;
  char *info_ptr;
  char *via_ptr;
  char temp_str[512];
  int special_debug = 0;
  int data_length;


  // Fetch the length of the data portion of the packet
  data_length = (unsigned char)(input_string[31]);
  data_length = (data_length << 8) + (unsigned char)(input_string[30]);
  data_length = (data_length << 8) + (unsigned char)(input_string[29]);
  data_length = (data_length << 8) + (unsigned char)(input_string[28]);

  // Implementing some special debugging output for the case of
  // third-party NWS messages, which so far haven't been parsed
  // properly by this function.
  //
  // Check for NWS string past the header part of the AGWPE
  // packet.
  //

  // Make sure we have a terminating '\0' at the end.
  // Note that this doesn't help for binary packets (like OpenTrac),
  // but doesn't really hurt either.
  input_string[38+data_length] = '\0';


  // Check what sort of AGWPE packet it is.
  switch (input_string[4])
  {

    case 'R':
      if (data_length == 8)
      {
        fprintf(stderr,
                "\nConnected to AGWPE server, version: %d.%d\n",
                (input_string[37] << 8) + input_string[36],
                (input_string[41] << 8) + input_string[40]);
      }
      return(NULL);   // All done!
      break;

    case 'G':
      // Print out the data, changing all ';' characters to
      // <CR> and a bunch of spaces to format it nicely.
      fprintf(stderr, "    Port Info, total ports = ");
      ii = 36;
      while (ii < data_length + 36 && input_string[ii] != '\0')
      {
        if (input_string[ii] == ';')
        {
          fprintf(stderr, "\n    ");
        }
        else
        {
          fprintf(stderr, "%c", input_string[ii]);
        }
        ii++;
      }
      fprintf(stderr,"\n");
      return(NULL);   // All done!
      break;

    case 'g':
      return(NULL);   // All done!
      break;

    case 'X':
      return(NULL);   // All done!
      break;

    case 'y':
      return(NULL);   // All done!
      break;

    case 'Y':
      return(NULL);   // All done!
      break;

    case 'H':
      return(NULL);   // All done!
      break;

    case 'C':
      return(NULL);   // All done!
      break;

    case 'v':
      return(NULL);   // All done!
      break;

    case 'c':
      return(NULL);   // All done!
      break;

    case 'D':
      return(NULL);   // All done!
      break;

    case 'd':
      return(NULL);   // All done!
      break;

    case 'U':
      // We can decode this one ok in the below code (after
      // this switch statement), but we no longer use
      // "monitor" mode packets in AGWPE, switching to the
      // "raw" mode instead.
      return(NULL);   // All done!
      break;

    case 'I':
      return(NULL);   // All done!
      break;

    case 'S':
      return(NULL);   // All done!
      break;

    case 'T':
      // We should decode this one ok in the below code (after
      // this switch statement), but we no longer use
      // "monitor" mode packets in AGWPE, switching to the
      // "raw" mode instead.
      return(NULL);   // All done!
      break;

    case 'K':
      // Code here processes the packet for handing to our
      // KISS decoding routines.  Chop off the header, add
      // anything to the beginning/end that we need, then send
      // it to decode_ax25_header().

      // Try to decode header and checksum.  If bad, break,
      // else continue through to ASCII logging & decode
      // routines.  We skip the first byte as it's not part of
      // the AX.25 packet.
      //
      // Note that the packet length often increases here in
      // decode_ax25_header, as we add '*' characters and such
      // to the header as it's decoded.

      // This string already has a terminator on the end,
      // added by the code in port_read().  If we didn't have
      // one here, we could end up with portions of strings
      // concatenated on the end of our string by the time
      // we're done processing the data here.
      //
      //            input_string[data_length+36] = '\0';


      // WE7U:
      // We may need to extend input_string by a few characters before it
      // is fed to us.  Something like max_callsigns * 3 or 4 characters,
      // to account for '*' and SSID characters that we might add.  This
      // keeps the string from getting truncated as we add bytes to the
      // header in decode_ax25_header.


      if ( !decode_ax25_header( (unsigned char *)&input_string[37], &data_length ) )
      {
        //                int zz;

        // Had a problem decoding it.  Drop it on the floor.
        fprintf(stderr, "AGWPE: Bad KISS packet.  Dropping it.\n");

        special_debug++;
        //                for (zz = 0; zz < data_length; zz++) {
        //                    fprintf(stderr, "%02x ", input_string[zz+36]);
        //                }
        //                fprintf(stderr,"\n");

        return(NULL);
      }

      // Good header.  Compute the new length, again skipping
      // the first byte.
      data_length = strlen((const char *)&input_string[37]);

      // The above strlen() requires it to be printable ascii in the KISS
      // packet, so won't work for OpenTrac protocol or other binary
      // protocols.  The decode_ax25_header() function also looks for
      // PID=0xF0, which again won't work for OpenTrac.  It would be
      // better to have the decode_ax25_header routine return the new
      // length of the packet so that decoding of binary packets is still
      // possible.

      // Check for OpenTrac packets.  If found, dump them into
      // OpenTrac-specific decode and skip the other decode below.  Must
      // tweak the above stuff to allow binary-format packets to get
      // through to this point.

      // Do more stuff with the packet here.  The actual
      // packet itself starts at offset 37.  We can end up
      // with 0x0d, 0x0d 0x0d, or 0x0d 0x00 0x0d on the end of
      // it (or none of the above).  Best method should be to
      // just search for any 0x0d's or 0x0a's starting at the
      // beginning of the string and overwrite them with
      // 0x00's.  That's what we do here.
      //
      for (ii = 0; ii < data_length; ii++)
      {
        if (input_string[ii+37] == 0x0d
            || input_string[ii+37] == 0x0a)
        {
          input_string[ii+37] = '\0';
        }
      }

      // Compute data_length again.
      data_length = strlen((const char *)&input_string[37]);

      // Send the processed string back for decoding
      xastir_snprintf((char *)output_string,
                      output_string_length,
                      "%s",
                      &input_string[37]);

      // Send back the new length.
      *new_length = data_length;

      return(output_string);
      break;

    default:
      fprintf(stderr,"AGWPE: Got unrecognized '%c' packet\n",input_string[4]);
      return(NULL);   // All done!
      break;
  }


  // NOTE:  All of the code below gets used in "monitor" mode, which
  // we no longer use.  We might keep this code around for a bit and
  // then delete it, as we've probably switched to "raw" mode for
  // good.  "raw" mode allows us to use our KISS processing routines,
  // plus allows us to support digipeating and OpenTrac (binary)
  // protocol in the future.


  if (special_debug)
  {
    // Dump the hex & ascii representation of the whole packet

    kk = data_length + 36; // Add the header length
    fprintf(stderr, "Total Length = %d\n", kk);

    fprintf(stderr, "HEX:");
    for (ii = 0; ii < kk; ii++)
    {
      fprintf(stderr, "%02x ", input_string[ii]);
    }
    fprintf(stderr, "\n");

    fprintf(stderr, "ASC:");
    for (ii = 0; ii < kk; ii++)
    {
      if (input_string[ii] < ' ' || input_string[ii] > '~')
      {
        fprintf(stderr, ".");
      }
      else
      {
        fprintf(stderr, "%c", input_string[ii]);
      }
    }
    fprintf(stderr, "\n");
  }

  // Clear the output_string (set to binary zeroes)
  for (ii = 0; ii < output_string_length; ii++)
  {
    output_string[ii] = '\0';
  }

  jj = 0;

  // Copy the source callsign
  ii = 8;
  while (input_string[ii] != '\0')
  {
    output_string[jj++] = input_string[ii++];
  }

  // Add a '>' character
  output_string[jj++] = '>';

  // Copy the destination callsign
  ii = 18;
  while (input_string[ii] != '\0')
  {
    output_string[jj++] = input_string[ii++];
  }

  // Search for "]" (0x5d) which is the end of the header string,
  // beginning of the AX.25 information field.
  info_ptr = strstr((const char *)&input_string[36], "]");

  // If not found, we can't process anymore
  if (!info_ptr)
  {
    output_string[0] = '\0';
    new_length = 0;
    return(NULL);
  }

  // Copy the first part of the string into a variable.  We'll
  // look for Via calls in this string, if present.
  ii = 36;
  temp_str[0] = '\0';

  while (input_string[ii] != ']')
  {
    strncat(temp_str, (char *)(&input_string[ii++]), 1);
  }

  // Make sure that the protocol ID is "F0".  If not, return.
  if (strstr(temp_str, "pid=F0") == NULL)
  {
    char *pid_ptr;

    // Look for the "pid=" string and print out what we can
    // figure out about the protocol ID.
    pid_ptr = strstr(temp_str, "pid=");
    if (pid_ptr)
    {
      pid_ptr +=4;
      fprintf(stderr,
              "parse_agwpe_packet: Non-APRS protocol was seen: PID=%2s.  Dropping the packet.\n",
              pid_ptr);
    }
    else
    {
      fprintf(stderr,
              "parse_agwpe_packet: Non-APRS protocol was seen.  Dropping the packet.\n");
    }
    output_string[0] = '\0';
    new_length = 0;
    return(NULL);
  }

  // Search for "Via" in temp_str
  via_ptr = strstr(temp_str, "Via");

  if (via_ptr)
  {
    // Found some Via calls.  Copy them into our output string.

    // Add a comma first
    output_string[jj++] = ',';

    // Skip past "Via " portion of string
    via_ptr += 4;

    // Copy the string across until we hit a space
    while (via_ptr[0] != ' ')
    {
      output_string[jj++] = via_ptr[0];
      via_ptr++;
    }
  }

  // Add a ':' character
  output_string[jj++] = ':';

  // Move the pointer past the "]<CR>" to the real info part of the
  // packet.
  info_ptr++;
  info_ptr++;

  // Copy the info field to the output string
  while (info_ptr[0] != '\0')
  {
    strncat((char *)output_string, &info_ptr[0], 1);
    info_ptr++;
  }

  // We end up with 0x0d characters on the end.  Get rid of them.
  // The strtok() function will overwrite the first one found with
  // a '\0' character.
  (void)strtok((char *)output_string, "\n");
  (void)strtok((char *)output_string, "\r");

  *new_length = strlen((const char *)output_string);

  if (special_debug)
  {
    // Print out the resulting string
    fprintf(stderr,"AGWPE RX: %s\n", output_string);
    fprintf(stderr,"new_length: %d\n",*new_length);
    for (ii = 0; ii < (int)strlen((const char *)output_string); ii++)
    {
      fprintf(stderr,"%02x ",output_string[ii]);
    }
    fprintf(stderr,"\n");
  }

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

char *get_device_name_only(char *device_name)
{
  int i,len,done;

  if (device_name == NULL)
  {
    return(NULL);
  }

  done = 0;
  len = (int)strlen(device_name);
  for(i = len; i > 0 && !done; i--)
  {
    if(device_name[i] == '/')
    {
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
int get_open_device(void)
{
  int i, found;

  begin_critical_section(&devices_lock, "interface.c:get_open_device" );

  found = -1;
  for(i = 0; i < MAX_IFACE_DEVICES && found == -1; i++)
  {
    if (devices[i].device_type == DEVICE_NONE)
    {
      found = i;
      break;
    }
  }

  end_critical_section(&devices_lock, "interface.c:get_open_device" );

  if (found == -1)
  {
    popup_message(langcode("POPEM00004"),langcode("POPEM00017"));
  }

  return(found);
}





//***********************************************************
// Get Device Status
//
// this will return the device status for the port specified
//***********************************************************
int get_device_status(int port)
{
  int stat;

  if (begin_critical_section(&port_data_lock, "interface.c:get_device_status(1)" ) > 0)
  {
    fprintf(stderr,"port_data_lock, Port = %d\n", port);
  }

  stat = port_data[port].status;

  if (end_critical_section(&port_data_lock, "interface.c:get_device_status(2)" ) > 0)
  {
    fprintf(stderr,"port_data_lock, Port = %d\n", port);
  }

  return(stat);
}





//***********************************************************
// channel_data()
//
// Takes data read in from a port and adds it to the
// incoming_data_queue.  If queue is full, waits for queue to have
// space before continuing.
//
// port #
// string is the string of data
// length is the length of the string.  If 0 then use strlen()
// on the string itself to determine the length.
//
// Note that decode_ax25_header() and perhaps other routines may
// increase the length of the string while processing.  We need to
// send a COPY of our input string off to the decoding routines for
// this reason, and the size of the buffer must be MAX_LINE_SIZE
// for this reason also.
//***********************************************************
void channel_data(int port, unsigned char *string, volatile int length)
{
  volatile int max;
  struct timeval tmv;
  // Some messiness necessary because we're using xastir_mutex's
  // instead of pthread_mutex_t's.
  pthread_mutex_t *cleanup_mutex1;
  pthread_mutex_t *cleanup_mutex2;
  // This variable defined as volatile to quash a GCC warning on some
  // platforms that "process_it" might be "clobbered" by a longjmp or
  // vfork.  There is a longjmp in forked_getaddrinfo, and somehow this
  // function gets involved somewhere.  If the compiler optimizes this
  // function just right, putting process_it into a register, that fouls
  // things up.  Declaring volatile silences the warning by removing
  // the possibility of clobbering by longjmp.
  volatile int process_it = 0;


  // Save backup copies of the incoming string and the previous
  // string.  Used for debugging purposes.  If we get a segfault,
  // we can print out the last two messages received.
  xastir_snprintf((char *)incoming_data_copy_previous,
                  sizeof(incoming_data_copy_previous),
                  "%s",
                  incoming_data_copy);
  xastir_snprintf((char *)incoming_data_copy,
                  sizeof(incoming_data_copy),
                  "Port%d:%s",
                  port,
                  string);

  max = 0;

  if (string == NULL)
  {
    return;
  }

  if (string[0] == '\0')
  {
    return;
  }

  if (length == 0)
  {
    // Compute length of string including terminator
    length = strlen((const char *)string) + 1;
  }

  // Check for excessively long packets.  These might be TCP/IP
  // packets or concatenated APRS packets.  In any case it's some
  // kind of garbage that we don't want to try to parse.

  // Note that for binary data (WX stations and KISS packets), the
  // strlen() function may not work correctly.
  if (length > MAX_LINE_SIZE)     // Too long!
  {
    if (debug_level & 1)
    {
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
  //    pthread_cleanup_push(void (*pthread_mutex_unlock)(void *), (void *)cleanup_mutex1);


  // This protects channel_data from being run by more than one
  // thread at the same time.
  if (begin_critical_section(&output_data_lock, "interface.c:channel_data(1)" ) > 0)
  {
    fprintf(stderr,"output_data_lock, Port = %d\n", port);
  }


  if (length > 0)
  {


    // Install the cleanup routine for the case where this
    // thread gets killed while the mutex is locked.  The
    // cleanup routine initiates an unlock before the thread
    // dies.  We must be in deferred cancellation mode for the
    // thread to have this work properly.  We must first get the
    // pthread_mutex_t address.
    cleanup_mutex2 = &data_lock.lock;

    // Then install the cleanup routine:
    pthread_cleanup_push((void *)pthread_mutex_unlock, (void *)cleanup_mutex2);
    //        pthread_cleanup_push(void (*pthread_mutex_unlock)(void *), (void *)cleanup_mutex2);


    //        if (begin_critical_section(&data_lock, "interface.c:channel_data(2)" ) > 0)
    //            fprintf(stderr,"data_lock, Port = %d\n", port);


    // If it's any of three types of GPS ports and is a GPRMC or
    // GPGGA string, just stick it in one of two global
    // variables for holding such strings.  UpdateTime() can
    // come along and process/clear-out those strings at the
    // gps_time interval.
    //
    switch(port_data[port].device_type)
    {

      case DEVICE_SERIAL_GPS:
      case DEVICE_SERIAL_TNC_HSP_GPS:
      case DEVICE_NET_GPSD:

        // One of the three types of interfaces that might
        // send in a lot of GPS data constantly.  Save only
        // GPRMC and GPGGA strings into global variables.
        // Drop other GPS strings on the floor.
        //
        if ( (length > 7) && (isRMC((char *)string)))
        {
          xastir_snprintf(gprmc_save_string,
                          sizeof(gprmc_save_string),
                          "%s",
                          string);
          gps_port_save = port;
          process_it = 0;
        }
        else if ( (length > 7) && (isGGA((char *)string)))
        {
          xastir_snprintf(gpgga_save_string,
                          sizeof(gpgga_save_string),
                          "%s",
                          string);
          gps_port_save = port;
          process_it = 0;
        }
        else
        {
          // It's not one of the GPS strings we're looking
          // for.  It could be another GPS string, a
          // partial GPS string, or a full/partial TNC
          // string.  Drop the string on the floor unless
          // it's an HSP interface.
          //
          if (port_data[port].device_type == DEVICE_SERIAL_TNC_HSP_GPS)
          {
            // Decode the string normally.
            process_it++;
          }
        }
        break;
      // We need to make sure that the variables stating that a string is
      // available are reset in any case.  Look at how/where data_avail is
      // reset.  We may not care if we just wait for data_avail to be
      // cleared before writing to the string again.

      default:    // Not one of the above three types, decode
        // the string normally.
        process_it++;
        break;
    }

    // Remove the cleanup routine for the case where this thread
    // gets killed while the mutex is locked.  The cleanup
    // routine initiates an unlock before the thread dies.  We
    // must be in deferred cancellation mode for the thread to
    // have this work properly.
    //
    pthread_cleanup_pop(0);


    if (debug_level & 1)
    {
      fprintf(stderr,"Channel data on Port %d [%s]\n",port,(char *)string);
    }

    if (process_it)
    {

      // Wait for empty space in queue
      while (push_incoming_data(string, length, port) && max < 5400)
      {
        sched_yield();  // Yield to other threads
        tmv.tv_sec = 0;
        tmv.tv_usec = 2;  // 2 usec
        (void)select(0,NULL,NULL,NULL,&tmv);
        max++;
      }
    }
  }


  if (end_critical_section(&output_data_lock, "interface.c:channel_data(4)" ) > 0)
  {
    fprintf(stderr,"output_data_lock, Port = %d\n", port);
  }

  // Remove the cleanup routine for the case where this thread
  // gets killed while the mutex is locked.  The cleanup routine
  // initiates an unlock before the thread dies.  We must be in
  // deferred cancellation mode for the thread to have this work
  // properly.
  //
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

  do
  {
    /* Fetch one callsign token */
    if ((bp = call[argp++]) == NULL)
    {
      break;
    }

    /* Check for the optional 'via' syntax */
    if (n == 1 && (strcasecmp(bp, "V") == 0 || strcasecmp(bp, "VIA") == 0))
    {
      continue;
    }

    /* Process the token (Removes the star before the ax25_aton_entry call
       because it would call it a bad callsign.) */
    len = strlen(bp);
    if (len > 1 && bp[len-1] == '*')
    {
      star = 1;
      bp[len-1] = '\0';
    }
    else
    {
      star = 0;
    }
    if (ax25_aton_entry(bp, addrp) == -1)
    {
      popup_message("Bad callsign!", bp);
      return -1;
    }
    if (n >= 1 && star)
    {
      addrp[6] |= 0x80; // set digipeated bit if we had found a star
    }

    n++;

    if (n == 1)
    {
      addrp  = sax->fsa_digipeater[0].ax25_call;  /* First digipeater address */
    }
    else
    {
      addrp += sizeof(ax25_address);
    }

  }
  while (n < AX25_MAX_DIGIS && call[argp] != NULL);

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
int ui_connect( int port, char *to[])
{
  int    s = -1;
#ifdef HAVE_LIBAX25
  int    sockopt;
  int    addrlen = sizeof(struct full_sockaddr_ax25);
  struct full_sockaddr_ax25 axbind, axconnect;
  /* char  *arg[2]; */
  char  *portcall;
  char temp[200];

  if (to == NULL)
  {
    return(-1);
  }

  if (*to[0] == '\0')
  {
    return(-1);
  }

  /*
   * Handle incoming data
   *
   * Parse the passed values for correctness.
   */

  axconnect.fsa_ax25.sax25_family = AF_AX25;
  axbind.fsa_ax25.sax25_family    = AF_AX25;
  axbind.fsa_ax25.sax25_ndigis    = 1;

  if ((portcall = ax25_config_get_addr(port_data[port].device_name)) == NULL)
  {
    xastir_snprintf(temp, sizeof(temp), langcode("POPEM00005"),
                    port_data[port].device_name);
    popup_message(langcode("POPEM00004"),temp);
    return -1;
  }
  if (ax25_aton_entry(portcall, axbind.fsa_digipeater[0].ax25_call) == -1)
  {
    xastir_snprintf(temp, sizeof(temp), langcode("POPEM00006"),
                    port_data[port].device_name);
    popup_message(langcode("POPEM00004"), temp);
    return -1;
  }

  if (ax25_aton_entry(port_data[port].ui_call, axbind.fsa_ax25.sax25_call.ax25_call) == -1)
  {
    xastir_snprintf(temp, sizeof(temp), langcode("POPEM00007"), port_data[port].ui_call);
    popup_message(langcode("POPEM00004"),temp);
    return -1;
  }

  if (my_ax25_aton_arglist(to, &axconnect) == -1)
  {
    popup_message(langcode("POPEM00004"),langcode("POPEM00008"));
    return -1;
  }

  /*
   * Open the socket into the kernel.
   */

  if ((s = socket(AF_AX25, SOCK_DGRAM, 0)) < 0)
  {
    xastir_snprintf(temp, sizeof(temp), langcode("POPEM00009"), strerror(errno));
    popup_message(langcode("POPEM00004"),temp);
    return -1;
  }

  /*
   * Set our AX.25 callsign and AX.25 port callsign accordingly.
   */
  ENABLE_SETUID_PRIVILEGE;
  if (bind(s, (struct sockaddr *)&axbind, addrlen) != 0)
  {
    DISABLE_SETUID_PRIVILEGE;
    xastir_snprintf(temp, sizeof(temp), langcode("POPEM00010"), strerror(errno));
    popup_message(langcode("POPEM00004"),temp);
    return -1;
  }
  DISABLE_SETUID_PRIVILEGE;

  if (devices[port].relay_digipeat)
  {
    sockopt = 1;
  }
  else
  {
    sockopt = 0;
  }

  if (setsockopt(s, SOL_AX25, AX25_IAMDIGI, &sockopt, sizeof(int)))
  {
    fprintf(stderr,"AX25 IAMDIGI setsockopt FAILED");
    return -1;
  }

  if (debug_level & 2)
  {
    fprintf(stderr,"*** Connecting to UNPROTO port for transmission...\n");
  }

  /*
   * Lets try and connect to the far end.
   */

  if (connect(s, (struct sockaddr *)&axconnect, addrlen) != 0)
  {
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

static void data_out_ax25(int port, unsigned char *string)
{
  static char ui_mycall[10];
  char        *temp;
  char        *to[10];
  int         quantity;

  if (string == NULL)
  {
    return;
  }

  if (string[0] == '\0')
  {
    return;
  }

  if (begin_critical_section(&port_data_lock, "interface.c:data_out_ax25(1)" ) > 0)
  {
    fprintf(stderr,"port_data_lock, Port = %d\n", port);
  }

  // Check for commands (start with Control-C)
  if (string[0] == (unsigned char)3)   // Yes, process TNC type commands
  {

    // Look for MYCALL command
    if (strncmp((char *)&string[1],"MYCALL", 6) == 0)
    {

      // Found MYCALL.  Snag the callsign and put it into the
      // structure for the port

      // Look for whitespace/CR/LF (end of "MYCALL")
      temp = strtok((char *)&string[1]," \t\r\n");
      if (temp != NULL)
      {

        // Look for whitespace/CR/LF (after callsign)
        temp = strtok(NULL," \t\r\n");
        if (temp != NULL)
        {
          substr(ui_mycall, temp, 9);
          xastir_snprintf(port_data[port].ui_call,
                          sizeof(port_data[port].ui_call),
                          "%s",
                          ui_mycall);
          if (debug_level & 2)
          {
            fprintf(stderr,"*** MYCALL %s\n",port_data[port].ui_call);
          }
        }
      }
    }

    // Look for UNPROTO command
    else if (strncmp((char *)&string[1],"UNPROTO", 6) == 0)
    {
      quantity = 0;   // Number of callsigns found

      // Look for whitespace/CR/LF (end of "UNPROTO")
      temp = strtok((char *)&string[1]," \t\r\n");
      if (temp != NULL)   // Found end of "UNPROTO"
      {

        // Find first callsign (destination call)
        temp = strtok(NULL," \t\r\n");
        if (temp != NULL)
        {
          to[quantity++] = temp; // Store it

          // Look for "via" or "v"
          temp = strtok(NULL," \t\r\n");

          while (temp != NULL)    // Found it
          {
            // Look for the rest of the callsigns (up to
            // eight of them)
            temp = strtok(NULL," ,\t\r\n");
            if (temp != NULL)
            {
              if (quantity < 9)
              {
                to[quantity++] = temp;
              }
            }
          }
          to[quantity] = NULL;

          if (debug_level & 2)
          {
            int i = 1;

            fprintf(stderr,"UNPROTO %s VIA ",*to);
            while (to[i] != NULL)
            {
              fprintf(stderr,"%s,",to[i++]);
            }
            fprintf(stderr,"\n");
          }

          if (port_data[port].channel2 != -1)
          {
            if (debug_level & 2)
            {
              fprintf(stderr,"Write DEVICE is UP!  Taking it down to reconfigure UI path.\n");
            }

            (void)close(port_data[port].channel2);
            port_data[port].channel2 = -1;
          }

          if ((port_data[port].channel2 = ui_connect(port,to)) < 0)
          {
            popup_message(langcode("POPEM00004"),langcode("POPEM00012"));
            port_data[port].errors++;
          }
          else      // Port re-opened and re-configured
          {
            if (debug_level & 2)
            {
              fprintf(stderr,"WRITE port re-opened after UI path change\n");
            }
          }
        }
      }
    }
  }

  // Else not a command, write the data directly out to the port
  else
  {
    if (debug_level & 2)
    {
      fprintf(stderr,"*** DATA: %s\n",(char *)string);
    }

    if (port_data[port].channel2 != -1)
    {
      if (write(port_data[port].channel2, string, strlen((char *)string)) != -1)
      {
        /* we don't actually care if this returns -1 or not
           but newer linux systems hate when we ignore the return
           value of write() */
      }
    }
    else if (debug_level & 2)
    {
      fprintf(stderr,"\nPort down for writing!\n\n");
    }
  }

  if (end_critical_section(&port_data_lock, "interface.c:data_out_ax25(2)" ) > 0)
  {
    fprintf(stderr,"port_data_lock, Port = %d\n", port);
  }
}





// fetch16bits
//
// Modifies: Nothing.
//
int fetch16bits(unsigned char *str)
{
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
int fetch32bits(unsigned char *str)
{
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



//***********************************************************
// process_ax25_packet()
//
// bp           raw packet data
// len          length of raw packet data
// buffer       buffer to write readable packet data to
// buffer_size  max length of buffer
//
// Note that db.c:decode_ax25_header does much the same thing for
// Serial KISS interface packets.  Consider combining the two
// functions.  process_ax25_packet() would be the earlier and more
// thought-out function.
//***********************************************************

char *process_ax25_packet(unsigned char *bp, unsigned int len, char *buffer, int buffer_size)
{
  int i,j;
  unsigned int  l;
  unsigned int  digis;
  unsigned char source[10];
  unsigned char dest[10];
  unsigned char digi[10][10];
  unsigned char digi_h[10];
  unsigned int  ssid;
  unsigned char message[513];

  if ( (bp == NULL) || (buffer == NULL) )
  {
    return(NULL);
  }

  /* clear buffer */
  buffer[0] = '\0';

  if (*bp != (unsigned char)0)
  {
    return(NULL);  /* not a DATA packet */
  }

  // We have a KISS packet here, so we know that the first
  // character is a flag character.  Skip over it.
  bp++;
  len--;

  // Check the length to make sure that we don't have an empty
  // packet.
  if (!bp || !len)
  {
    return(NULL);
  }

  // Check for minimum KISS frame bytes.
  if (len < 15)
  {
    return(NULL);
  }

  if (bp[1] & 1) /* Compressed FlexNet Header */
  {
    return(NULL);
  }

  /* Destination of frame */
  j = 0;
  for(i = 0; i < 6; i++)
  {
    if ((bp[i] &0xfe) != (unsigned char)0x40)
    {
      dest[j++] = bp[i] >> 1;
    }
  }
  ssid = (unsigned int)( (bp[6] & 0x1e) >> 1 );
  if (ssid != 0)
  {
    dest[j++] = '-';
    if ((ssid / 10) != 0)
    {
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
  for(i = 0; i < 6; i++)
  {
    if ((bp[i] &0xfe) != (unsigned char)0x40)
    {
      source[j++] = bp[i] >> 1;
    }
  }
  ssid = (unsigned int)( (bp[6] & 0x1e) >> 1 );
  if (ssid != 0)
  {
    source[j++] = '-';
    if ((ssid / 10) != 0)
    {
      source[j++] = '1';
    }

    source[j++] = (unsigned char)(ssid % 10) + (unsigned char)'0';
    // source[j++] = (unsigned char)ssid + (unsigned char)'0';
  }
  source[j] = '\0';
  bp += 7;
  len -= 7;

  // by KJ5O - test for proper extraction of source call and ssid
  // fprintf(stderr, "|KJ5O-test| %s-%d\n", source, ssid);

  /* Digipeaters */
  digis = 0;
  while ((!(bp[-1] & 1)) && (len >= 7))
  {
    /* Digi of frame */
    if (digis != 10)
    {
      j = 0;
      for (i = 0; i < 6; i++)
      {
        if ((bp[i] &0xfe) != (unsigned char)0x40)
        {
          digi[digis][j++] = bp[i] >> 1;
        }
      }
      digi_h[digis] = (bp[6] & 0x80);
      ssid = (unsigned int)( (bp[6] & 0x1e) >> 1 );
      if (ssid != 0)
      {
        digi[digis][j++] = '-';
        if ((ssid / 10) != 0)
        {
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
  {
    return(NULL);
  }

  /* We are now at the primitive bit */
  i = (int)(*bp++);
  len--;

  /* strip the poll-bit from the primitive */
  i = i & (~0x10);

  /* return if this is not an UI frame (= 0x03) */
  if(i != 0x03)
  {
    return(NULL);
  }

  /* no data left */
  if (!len)
  {
    return(NULL);
  }

  if(*bp != (unsigned char)0xF0)     // APRS PID
  {
    // We _don't_ have an APRS packet
    return(NULL);
  }

  // We have what looks like a valid KISS-frame containing APRS
  // protocol data.

  bp++;
  len--;
  l = 0;
  while (len)
  {
    i = (int)(*bp++);
    if ((i != (int)'\n') && (i != (int)'\r'))
    {
      if (l < 512)
      {
        message[l++] = (unsigned char)i;
      }
    }
    len--;
  }
  /* add terminating '\0' to allow handling as a string */
  message[l] = '\0';

  xastir_snprintf(buffer,
                  buffer_size,
                  "%s",
                  source);

  /*
   * if there are no digis or the first digi has not handled the
   * packet then this is directly from the source, mark it with
   * a "*" in that case
   */

  strncat(buffer, ">", buffer_size - 1 - strlen(buffer));

  /* destination is at the begining of the chain, because it is  */
  /* needed so MIC-E packets can be decoded correctly. */
  /* this may be changed in the future but for now leave it here -FG */
  strncat(buffer, (char *)dest, buffer_size - 1 - strlen(buffer));

  for(i = 0; i < (int)digis; i++)
  {
    strncat(buffer, ",", buffer_size - 1 - strlen(buffer));
    strncat(buffer, (char *)digi[i], buffer_size - 1 - strlen(buffer));
    /* at the last digi always put a '*' when h_bit is set */
    if (i == (int)(digis - 1))
    {
      if (digi_h[i] == (unsigned char)0x80)
      {
        /* this digi must have transmitted the packet */
        strncat(buffer, "*", buffer_size - 1 - strlen(buffer));
      }
    }
    else
    {
      if (digi_h[i] == (unsigned char)0x80)
      {
        /* only put a '*' when the next digi has no h_bit */
        if (digi_h[i + 1] != (unsigned char)0x80)
        {
          /* this digi must have transmitted the packet */
          strncat(buffer, "*", buffer_size - 1 - strlen(buffer));
        }
      }
    }
  }
  strncat(buffer, ":", buffer_size - 1 - strlen(buffer));

  //Copy into only the free space in buffer.
  strncat( buffer, (char *)message, MAX_DEVICE_BUFFER - 1 - strlen(buffer));

  // And null-terminate it to make sure.
  buffer[MAX_DEVICE_BUFFER - 1] = '\0';

  return(buffer);
}





//*********************************************************
// AX25 port INIT
//
// port is port# used
//*********************************************************

int ax25_init(int port)
{

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
  {
    fprintf(stderr,"port_data_lock, Port = %d\n", port);
  }

  /* clear port_channel */
  //    port_data[port].channel = -1;

  /* clear port active */
  port_data[port].active = DEVICE_NOT_IN_USE;

  /* clear port status */
  port_data[port].status = DEVICE_DOWN;

  // Show the latest status in the interface control dialog
  update_interface_list();

#ifdef HAVE_LIBAX25
  if (ax25_ports_loaded == 0)
  {
    /* port file has not been loaded before now */
    if (ax25_config_load_ports() == 0)
    {
      fprintf(stderr, "ERROR: problem with axports file\n");
      popup_message(langcode("POPEM00004"),langcode("POPEM00013"));

      if (end_critical_section(&port_data_lock, "interface.c:ax25_init(2)" ) > 0)
      {
        fprintf(stderr,"port_data_lock, Port = %d\n", port);
      }

      return -1;
    }
    /* we can only load the port file once!!! so do not load again */
    ax25_ports_loaded = 1;
  }

  if (port_data[port].device_name != NULL)
  {
    if ((dev = ax25_config_get_dev(port_data[port].device_name)) == NULL)
    {
      xastir_snprintf(temp, sizeof(temp), langcode("POPEM00014"),
                      port_data[port].device_name);
      popup_message(langcode("POPEM00004"),temp);

      if (end_critical_section(&port_data_lock, "interface.c:ax25_init(3)" ) > 0)
      {
        fprintf(stderr,"port_data_lock, Port = %d\n", port);
      }

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

  if (port_data[port].channel == -1)
  {
    perror("socket");
    if (end_critical_section(&port_data_lock, "interface.c:ax25_init(4)" ) > 0)
    {
      fprintf(stderr,"port_data_lock, Port = %d\n", port);
    }

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

  // Show the latest status in the interface control dialog
  update_interface_list();

#else /* HAVE_LIBAX25 */
  fprintf(stderr,"AX.25 support not compiled into Xastir!\n");
  popup_message(langcode("POPEM00004"),langcode("POPEM00021"));
#endif /* HAVE_LIBAX25 */
  if (end_critical_section(&port_data_lock, "interface.c:ax25_init(5)" ) > 0)
  {
    fprintf(stderr,"port_data_lock, Port = %d\n", port);
  }

  return(1);
}

//********************************* STOP AX.25 ********************************





//*************************** START SERIAL PORT FUNCTIONS ********************************


//******************************************************
// command file to tnc port
// port to send config data to
// Filename containing the config data
//******************************************************
int command_file_to_tnc_port(int port, char *filename)
{
  FILE *f;
  char line[MAX_LINE_SIZE+1];
  char command[MAX_LINE_SIZE+5];
  int i;
  char cin;
  int error;
  struct stat file_status;


  if (filename == NULL)
  {
    return(-1);
  }

  // Check file status
  if (stat(filename, &file_status) < 0)
  {
    fprintf(stderr,
            "Couldn't stat file: %s\n",
            filename);
    fprintf(stderr,
            "Skipping send to TNC\n");
    return(-1);
  }

  // Check that it is a regular file
  if (!S_ISREG(file_status.st_mode))
  {
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
  if (f != NULL)
  {
    int send_ctrl_C = 1;

    line[0] = (char)0;
    while (!feof(f) && error != -1)
    {

      if (fread(&cin,1,1,f) == 1)
      {

        // Check for <LF>/<CR>
        if (cin != (char)10 && cin != (char)13)
        {

          // If NOT <LF> or <CR>
          if (i < MAX_LINE_SIZE)
          {

            // Add to buffer
            line[i++] = cin;
            line[i] = (char)0;
          }
        }

        else    // Found a <LF> or <CR>, process line
        {
          i = 0;

          // Check whether comment or zero-length line
          if (line[0] != '#' && strlen(line) > 0)
          {

            // Line looks good.  Send it to the TNC.

            if (send_ctrl_C)
            {
              // Control-C desired
              xastir_snprintf(command,
                              sizeof(command),
                              "%c%s\r",
                              (char)03,   // Control-C
                              line);
            }
            else
            {
              // No Control-C desired
              xastir_snprintf(command,
                              sizeof(command),
                              "%s\r",
                              line);
            }

            if (debug_level & 2)
            {
              fprintf(stderr,"CMD:%s\n",command);
            }

            port_write_string(port,command);
            line[0] = (char)0;

            // Set flag to default condition
            send_ctrl_C = 1;
          }
          else    // Check comment to see if it is a META
          {
            // command

            // Should we make these ignore white-space?

            if (strncasecmp(line, "##META <", 8) == 0)
            {
              // Found a META command, process it
              if (strncasecmp(line+8, "delay", 5) == 0)
              {
                usleep(500000); // Sleep 500ms
              }
              else if (strncasecmp(line+8, "no-ctrl-c", 9) == 0)
              {
                // Reset the flag
                send_ctrl_C = 0;
              }
              else
              {
                fprintf(stderr,
                        "Unrecognized ##META command: %s\n",
                        line);
              }
            }
          }
        }
      }
    }
    (void)fclose(f);
  }
  else
  {
    if (debug_level & 2)
    {
      fprintf(stderr,"Could not open TNC command file: %s\n",filename);
    }
  }

  return(error);
}





//***********************************************************
// port_dtr INIT
// port is port# used
// dtr 1 is down, 0 is normal(up)
//***********************************************************
void port_dtr(int port, int dtr)
{

  // It looks like we have two methods of getting this to compile on
  // CYGWIN, getting rid of the entire procedure contents, and getting
  // rid of the TIO* code.  One method or the other should work to get
  // it compiled.  We shouldn't need both.
  int sg;

  /* check for 1 or 0 */
  dtr = (dtr & 0x1);

  if (begin_critical_section(&port_data_lock, "interface.c:port_dtr(1)" ) > 0)
  {
    fprintf(stderr,"port_data_lock, Port = %d\n", port);
  }

  if (port_data[port].active == DEVICE_IN_USE
      && port_data[port].status == DEVICE_UP
      && port_data[port].device_type == DEVICE_SERIAL_TNC_HSP_GPS)
  {

    port_data[port].dtr = dtr;
    if (debug_level & 2)
    {
      fprintf(stderr,"DTR %d\n",port_data[port].dtr);
    }

#ifdef TIOCMGET
    ENABLE_SETUID_PRIVILEGE;
    (void)ioctl(port_data[port].channel, TIOCMGET, &sg);
    DISABLE_SETUID_PRIVILEGE;
#endif  // TIOCMGET

    sg &= 0xff;

#ifdef TIOCM_DTR

    // ugly HPUX hack - n8ysz 20041206

#ifndef MDTR
#define MDTR 99999
#if (TIOCM_DTR == 99999)
#include <sys/modem.h>
#endif
#endif

    // end ugly hack

    sg = TIOCM_DTR;
#endif  // TIOCM_DIR

    if (dtr)
    {
      dtr &= ~sg;

#ifdef TIOCMBIC
      ENABLE_SETUID_PRIVILEGE;
      (void)ioctl(port_data[port].channel, TIOCMBIC, &sg);
      DISABLE_SETUID_PRIVILEGE;
#endif  // TIOCMBIC

      if (debug_level & 2)
      {
        fprintf(stderr,"Down\n");
      }

      // statusline(langcode("BBARSTA026"),1);

    }
    else
    {
      dtr |= sg;

#ifdef TIOCMBIS
      ENABLE_SETUID_PRIVILEGE;
      (void)ioctl(port_data[port].channel, TIOCMBIS, &sg);
      DISABLE_SETUID_PRIVILEGE;
#endif  // TIOCMBIS

      if (debug_level & 2)
      {
        fprintf(stderr,"UP\n");
      }

      // statusline(langcode("BBARSTA027"),1);
    }
  }

  if (end_critical_section(&port_data_lock, "interface.c:port_dtr(2)" ) > 0)
  {
    fprintf(stderr,"port_data_lock, Port = %d\n", port);
  }
}





//***********************************************************
// port_dtr INIT
// port is port# used
// dtr 1 is down, 0 is normal(up)
//***********************************************************
void dtr_all_set(int dtr)
{
  int i;

  for (i = 0; i < MAX_IFACE_DEVICES; i++)
  {
    if (port_data[i].device_type == DEVICE_SERIAL_TNC_HSP_GPS
        && port_data[i].status == DEVICE_UP)
    {
      port_dtr(i,dtr);
    }
  }
}





//***********************************************************
// Serial port close.  Remove the lockfile as well.
// port is port# used
//***********************************************************
int serial_detach(int port)
{
  char fn[600];
  int ok;
  ok = -1;

  if (begin_critical_section(&port_data_lock, "interface.c:serial_detach(1)" ) > 0)
  {
    fprintf(stderr,"port_data_lock, Port = %d\n", port);
  }

  if (port_data[port].active == DEVICE_IN_USE && port_data[port].status == DEVICE_UP)
  {

    // Close port first
    (void)tcsetattr(port_data[port].channel, TCSANOW, &port_data[port].t_old);
    if (close(port_data[port].channel) == 0)
    {
      port_data[port].status = DEVICE_DOWN;
      usleep(200);
      port_data[port].active = DEVICE_NOT_IN_USE;
      ok = 1;

      // Show the latest status in the interface control dialog
      update_interface_list();
    }
    else
    {
      if (debug_level & 2)
      {
        fprintf(stderr,"Could not close port %s\n",port_data[port].device_name);
      }

      port_data[port].status = DEVICE_DOWN;
      usleep(200);
      port_data[port].active = DEVICE_NOT_IN_USE;

      // Show the latest status in the interface control dialog
      update_interface_list();
    }

    // Delete lockfile
    xastir_snprintf(fn, sizeof(fn), "/var/lock/LCK..%s", get_device_name_only(port_data[port].device_name));
    if (debug_level & 2)
    {
      fprintf(stderr,"Delete lock file %s\n",fn);
    }

    ENABLE_SETUID_PRIVILEGE;
    (void)unlink(fn);
    DISABLE_SETUID_PRIVILEGE;
  }
  else
  {

    // If we didn't have the port in use, for instance we
    // weren't able to open it, we should check whether a
    // lockfile exists for the port and see if another running
    // process owns the lockfile (the PID of the owner is inside
    // the lockfile).  If not, remove the lockfile 'cuz it may
    // have been ours from this or a previous run.  Note that we
    // can now run multiple Xastir sessions from a single user,
    // and the lockfiles must be kept straight between them.  If
    // a lockfile doesn't contain a PID from a running process,
    // it's fair game to delete the lockfile and/or take over
    // the port with a new lockfile.
    //
    //   if (lockfile exists) {
    //       PID = read contents of lockfile
    //       if (PID is running) {
    //           Do nothing, leave the file alone
    //       }
    //       else {
    //           Delete the lockfile
    //       }
    //   }
  }

  if (end_critical_section(&port_data_lock, "interface.c:serial_detach(2)" ) > 0)
  {
    fprintf(stderr,"port_data_lock, Port = %d\n", port);
  }

  return(ok);
}





//***********************************************************
// Serial port INIT
// port is port# used
//***********************************************************
int serial_init (int port)
{
  FILE *lock;
  int speed;
  pid_t mypid = 0;
  pid_t lockfile_pid = 0;
  int lockfile_intpid;
  char fn[600];
  uid_t user_id;
  struct passwd *user_info;
  char temp[100];
  char temp1[100];
  pid_t status;
  int ii;
  int myerrno;

  status = -9999;

  if (begin_critical_section(&port_data_lock, "interface.c:serial_init(1)" ) > 0)
  {
    fprintf(stderr,"port_data_lock, Port = %d\n", port);
  }

  // clear port_channel
  port_data[port].channel = -1;

  // clear port active
  port_data[port].active = DEVICE_NOT_IN_USE;

  // clear port status
  port_data[port].status = DEVICE_DOWN;

  // Show the latest status in the interface control dialog
  update_interface_list();


  // Check whether we have a port with the same device already
  // open.  Check all ports except this one and check for
  // DEVICE_IN_USE.  If found, check whether the device_name
  // matches.  If a match, skip initializing this port.
  //
  for (ii = 0; ii < MAX_IFACE_DEVICES; ii++)
  {
    if (ii != port)
    {
      if (port_data[ii].active == DEVICE_IN_USE)
      {
        if (strcmp(port_data[ii].device_name, port_data[port].device_name) == 0)
        {
          // Found a port with the same device_name which
          // is already active.  Skip bringing up another
          // interface on the same port.
          return(-1);
        }
      }
    }
  }


  // check for lockfile
  xastir_snprintf(fn, sizeof(fn), "/var/lock/LCK..%s",
                  get_device_name_only(port_data[port].device_name));

  if (filethere(fn) == 1)
  {

    // Also look for pid of other process and see if it is a valid lock
    fprintf(stderr,"Found an existing lockfile %s for this port!\n",fn);

    lock = fopen(fn,"r");
    if (lock != NULL)   // We could open it so it must have
    {
      // been created by this userid
      if (fscanf(lock,"%d %99s %99s",&lockfile_intpid,temp,temp1) == 3)
      {
        lockfile_pid = (pid_t)lockfile_intpid;

#ifdef HAVE_GETPGRP
#ifdef GETPGRP_VOID
        // Won't this one get our process group instead of
        // the process group for the lockfile?  Not of that
        // much use to us here.
        status = getpgrp();
#else // GETPGRP_VOID
        status = getpgrp(lockfile_pid);
#endif // GETPGRP_VOID
#else   // HAVE_GETPGRP
        status = getpgid(lockfile_pid);
#endif // HAVE_GETPGRP

      }
      else
      {
        // fscanf parsed the wrong number of items.
        // lockfile is different, perhaps created by some
        // other program.
      }

      (void)fclose(lock);

      // See whether the existing lockfile is stale.  Remove
      // the file if it belongs to our process group or if the
      // PID in the file is no longer running.
      //
      // The only time we _shouldn't_ delete the file and
      // claim the port for our own is when the process that
      // created the lockfile is still running.

      // Get my process id
      mypid = getpid();

      // If status = -1, the process that created the lockfile
      // is no longer running and of course will not match
      // "lockfile_pid", so we can delete it.
      //
      // If "lockfile_pid == mypid", then this currently
      // running instance of Xastir was the one that created
      // the lockfile and it is again ok to delete it.
      //
      if (status != lockfile_pid || lockfile_pid == mypid)
      {
        fprintf(stderr,"Lock is stale!  Removing it.\n");
        ENABLE_SETUID_PRIVILEGE;
        (void)unlink(fn);
        DISABLE_SETUID_PRIVILEGE;
      }
      else
      {
        fprintf(stderr,"Cannot open port:  Another program has the lock!\n");

        if (end_critical_section(&port_data_lock, "interface.c:serial_init(2)" ) > 0)
        {
          fprintf(stderr,"port_data_lock, Port = %d\n", port);
        }

        return (-1);
      }
    }
    else      // Couldn't open it, so the lock must have been
    {
      // created by another userid
      fprintf(stderr,"Cannot open port:  Lockfile cannot be opened!\n");

      if (end_critical_section(&port_data_lock, "interface.c:serial_init(3)" ) > 0)
      {
        fprintf(stderr,"port_data_lock, Port = %d\n", port);
      }

      return (-1);
    }
  }

  // Try to open the serial port now
  ENABLE_SETUID_PRIVILEGE;
  port_data[port].channel = open(port_data[port].device_name, O_RDWR|O_NOCTTY);
  myerrno = errno;
  DISABLE_SETUID_PRIVILEGE;
  if (port_data[port].channel == -1)
  {

    if (end_critical_section(&port_data_lock, "interface.c:serial_init(4)" ) > 0)
    {
      fprintf(stderr,"port_data_lock, Port = %d\n", port);
    }

    if (debug_level & 2)
    {
      fprintf(stderr,"Could not open channel on port %d!\n",port);
    }

    switch (myerrno)
    {

      case EACCES:
        fprintf(stderr,"\tEACCESS ERROR\n");
        break;

      case EEXIST:
        fprintf(stderr,"\tEEXIST ERROR\n");
        break;

      case EFAULT:
        fprintf(stderr,"\tEFAULT ERROR\n");
        break;

      case EISDIR:
        fprintf(stderr,"\tEISDIR ERROR\n");
        break;

      case ELOOP:
        fprintf(stderr,"\tELOOP ERROR\n");
        break;

      case EMFILE:
        fprintf(stderr,"\tEMFILE ERROR\n");
        break;

      case ENAMETOOLONG:
        fprintf(stderr,"\tENAMETOOLONG ERROR\n");
        break;

      case ENFILE:
        fprintf(stderr,"\tEMFILE ERROR\n");
        break;

      case ENODEV:
        fprintf(stderr,"\tENODEV ERROR\n");
        break;

      case ENOENT:
        fprintf(stderr,"\tENOENT ERROR\n");
        break;

      case ENOMEM:
        fprintf(stderr,"\tENOMEM ERROR\n");
        break;

      case ENOSPC:
        fprintf(stderr,"\tENOSPC ERROR\n");
        break;

      case ENOTDIR:
        fprintf(stderr,"\tENOTDIR ERROR\n");
        break;

      case ENXIO:
        fprintf(stderr,"\tENXIO ERROR\n");
        break;

      case EOVERFLOW:
        fprintf(stderr,"\tEOVERFLOW ERROR\n");
        break;

      case EPERM:
        fprintf(stderr,"\tEPERM ERROR\n");
        break;

      case EROFS:
        fprintf(stderr,"\tEROFS ERROR\n");
        break;

      case ETXTBSY:
        fprintf(stderr,"\tETXTBSY ERROR\n");
        break;

      default:
        fprintf(stderr,"\tOTHER ERROR\n");
        break;
    }

    return (-1);
  }

  // Attempt to create the lockfile
  xastir_snprintf(fn, sizeof(fn), "/var/lock/LCK..%s", get_device_name_only(port_data[port].device_name));
  if (debug_level & 2)
  {
    fprintf(stderr,"Create lock file %s\n",fn);
  }

  ENABLE_SETUID_PRIVILEGE;
  lock = fopen(fn,"w");
  DISABLE_SETUID_PRIVILEGE;
  if (lock != NULL)
  {
    // get my process id for lockfile
    mypid = getpid();

    // get user info
    user_id = getuid();
    user_info = getpwuid(user_id);
    xastir_snprintf(temp,
                    sizeof(temp),
                    "%s",
                    user_info->pw_name);

    fprintf(lock,"%9d %s %s",(int)mypid,"xastir",temp);
    (void)fclose(lock);
    // We've successfully created our own lockfile
  }
  else
  {
    // lock failed
    if (debug_level & 2)
    {
      fprintf(stderr,"Warning:  Failed opening LCK file!  Continuing on...\n");
    }

    /* if we can't create lockfile don't fail!

       if (end_critical_section(&port_data_lock, "interface.c:serial_init(5)" ) > 0)
       fprintf(stderr,"port_data_lock, Port = %d\n", port);

       return (-1);*/
  }

  // get port attributes for new and old
  if (tcgetattr(port_data[port].channel, &port_data[port].t) != 0)
  {

    if (end_critical_section(&port_data_lock, "interface.c:serial_init(6)" ) > 0)
    {
      fprintf(stderr,"port_data_lock, Port = %d\n", port);
    }

    if (debug_level & 2)
    {
      fprintf(stderr,"Could not get t port attributes for port %d!\n",port);
    }

    // Close the port and remove the lock.
    serial_detach(port);

    return (-1);
  }

  if (tcgetattr(port_data[port].channel, &port_data[port].t_old) != 0)
  {

    if (end_critical_section(&port_data_lock, "interface.c:serial_init(7)" ) > 0)
    {
      fprintf(stderr,"port_data_lock, Port = %d\n", port);
    }

    if (debug_level & 2)
    {
      fprintf(stderr,"Could not get t_old port attributes for port %d!\n",port);
    }

    // Close the port and remove the lock.
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
  switch (port_data[port].style)
  {
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
  if (cfsetispeed(&port_data[port].t, port_data[port].sp) == -1)
  {

    if (end_critical_section(&port_data_lock, "interface.c:serial_init(8)" ) > 0)
    {
      fprintf(stderr,"port_data_lock, Port = %d\n", port);
    }

    if (debug_level & 2)
    {
      fprintf(stderr,"Could not set port input speed for port %d!\n",port);
    }

    // Close the port and remove the lock.
    serial_detach(port);

    return (-1);
  }

  if (cfsetospeed(&port_data[port].t, port_data[port].sp) == -1)
  {

    if (end_critical_section(&port_data_lock, "interface.c:serial_init(9)" ) > 0)
    {
      fprintf(stderr,"port_data_lock, Port = %d\n", port);
    }

    if (debug_level & 2)
    {
      fprintf(stderr,"Could not set port output speed for port %d!\n",port);
    }

    // Close the port and remove the lock.
    serial_detach(port);

    return (-1);
  }

  if (tcflush(port_data[port].channel, TCIFLUSH) == -1)
  {

    if (end_critical_section(&port_data_lock, "interface.c:serial_init(10)" ) > 0)
    {
      fprintf(stderr,"port_data_lock, Port = %d\n", port);
    }

    if (debug_level & 2)
    {
      fprintf(stderr,"Could not flush data for port %d!\n",port);
    }

    // Close the port and remove the lock.
    serial_detach(port);

    return (-1);
  }

  if (tcsetattr(port_data[port].channel,TCSANOW, &port_data[port].t) == -1)
  {

    if (end_critical_section(&port_data_lock, "interface.c:serial_init(11)" ) > 0)
    {
      fprintf(stderr,"port_data_lock, Port = %d\n", port);
    }

    if (debug_level & 2)
    {
      fprintf(stderr,"Could not set port attributes for port %d!\n",port);
    }

    // Close the port and remove the lock.
    serial_detach(port);

    return (-1);
  }

  // clear port active
  port_data[port].active = DEVICE_IN_USE;

  // clear port status
  port_data[port].status = DEVICE_UP;

  // Show the latest status in the interface control dialog
  update_interface_list();

  if (end_critical_section(&port_data_lock, "interface.c:serial_init(12)" ) > 0)
  {
    fprintf(stderr,"port_data_lock, Port = %d\n", port);
  }

  // return good condition
  return (1);
}

//*************************** STOP SERIAL PORT FUNCTIONS ********************************





//***************************** START NETWORK FUNCTIONS *********************************

//**************************************************************
// net_connect_thread()
// Temporary thread used to start up a socket.
//**************************************************************
static void* net_connect_thread(void *arg)
{
  int port;
  volatile int ok = -1;
  int result=-1;
  int flag;
  //int stat;
  struct addrinfo *res;

  // Some messiness necessary because we're using
  // xastir_mutex's instead of pthread_mutex_t's.
  pthread_mutex_t *cleanup_mutex;


  if (debug_level & 2)
  {
    fprintf(stderr,"net_connect_thread start\n");
  }

  port = *((int *) arg);
  // This call means we don't care about the return code and won't
  // use pthread_join() later.  Makes threading more efficient.
  (void)pthread_detach(pthread_self());

  for (res = port_data[port].addr_list; res; res = res->ai_next)
  {

    pthread_testcancel();   // Check for thread termination request
    port_data[port].channel = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    pthread_testcancel();   // Check for thread termination request

    if (port_data[port].channel == -1)
    {
      fprintf(stderr, "Socket creation for type (%d, %d, %d) failed: %s\n",
              res->ai_family, res->ai_socktype, res->ai_protocol, strerror(errno) );
      fprintf(stderr, "This may be OK if we have more to try.\n");
      continue;
    }

    if (debug_level & 2)
    {
      fprintf(stderr,"We have a socket to use\n");
    }
    flag = 1;

    // Turn on the socket keepalive option
    (void)setsockopt(port_data[port].channel,  SOL_SOCKET, SO_KEEPALIVE, (char *) &flag, sizeof(int));

    // Disable the Nagle algorithm (speeds things up)
    (void)setsockopt(port_data[port].channel, IPPROTO_TCP,  TCP_NODELAY, (char *) &flag, sizeof(int));

    if (debug_level & 2)
    {
      fprintf(stderr,"after setsockopt\n");
    }
    pthread_testcancel();  // Check for thread termination request
    if (debug_level & 2)
    {
      fprintf(stderr,"calling connect(), port: %d\n", port_data[port].socket_port);
    }
    result = connect(port_data[port].channel, res->ai_addr, res->ai_addrlen);
    if (debug_level & 2)
    {
      fprintf(stderr,"connect result was: %d\n", result);
    }
    if(result == 0 )
    {
      break;
    }
    if(result == -1)
    {
      fprintf(stderr, "Socket connection for interface %d type (%d, %d, %d) failed: %s\n",
              port, res->ai_family, res->ai_socktype, res->ai_protocol, strerror(errno) );
      if(res->ai_next)
      {
        fprintf(stderr, "This is OK since we have more to try.\n");
      }
      close(port_data[port].channel);
      port_data[port].channel = -1;
      continue;
    }
  }
  ok = 0;
  pthread_testcancel();  // Check for thread termination request
  if (result != -1)
  {
    /* connection up */
    if (debug_level & 2)
    {
      fprintf(stderr,"net_connect_thread():Net up, port %d\n",port);
    }

    port_data[port].status = DEVICE_UP;
    ok = 1;

    // Show the latest status in the interface control dialog
    update_interface_list();
  }
  else      /* net connection failed */
  {
    ok = 0;
    if (debug_level & 2)
    {
      fprintf(stderr,"net_connect_thread():net connection failed, port %d, DEVICE_ERROR ***\n",port);
    }
    port_data[port].status = DEVICE_ERROR;

    // Show the latest status in the interface control dialog
    update_interface_list();

    usleep(100000); // 100ms
    // Note:  Old comments note that it is essential not to shut down
    // the socket here, because the connection didn't actually happen,
    // and multithreading could lead to the socket number having
    // already been reused elsewhere.
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
  {
    fprintf(stderr,"net_connect_thread():connect_lock, Port = %d\n", port);
  }

  port_data[port].connect_status = ok;
  port_data[port].thread_status = 0;

  if (end_critical_section(&connect_lock, "interface.c:net_connect_thread(3)" ) > 0)
  {
    fprintf(stderr,"net_connect_thread():connect_lock, Port = %d\n", port);
  }

  // Remove the cleanup routine for the case where this thread
  // gets killed while the mutex is locked.  The cleanup routine
  // initiates an unlock before the thread dies.  We must be in
  // deferred cancellation mode for the thread to have this work
  // properly.
  //
  pthread_cleanup_pop(0);

  if (debug_level & 2)
  {
    fprintf(stderr,"net_connect_thread terminating itself\n");
  }

  return(NULL);   // This should kill the thread
}





//**************************************************************
// net_init()
//
// This brings up a network connection
//
// returns -1 on hard error, 0 on time out, 1 if ok
//**************************************************************
int net_init(int port)
{
  int ok;
  char st[200];
  pthread_t connect_thread;
  int stat;
  int wait_on_connect;
  time_t wait_time;
  int gai_rc;     // Return code from get address info
  char port_num[16];
  struct addrinfo hints;

  if (begin_critical_section(&port_data_lock, "interface.c:net_init(1)" ) > 0)
  {
    fprintf(stderr,"port_data_lock, Port = %d\n", port);
  }

  /* set port active */
  port_data[port].active = DEVICE_IN_USE;

  /* clear port status */
  port_data[port].status = DEVICE_DOWN;

  // Show the latest status in the interface control dialog
  update_interface_list();

  ok = -1;


  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = 0;
  hints.ai_flags = AI_NUMERICSERV|AI_ADDRCONFIG;
  xastir_snprintf(port_num, sizeof(port_num), "%d", port_data[port].socket_port);

  xastir_snprintf(st, sizeof(st), langcode("BBARSTA019"), port_data[port].device_host_name);
  statusline(st,1);   // Looking up host

  if(port_data[port].addr_list)
  {
    forked_freeaddrinfo(port_data[port].addr_list);
    port_data[port].addr_list = NULL;
  }
  gai_rc = forked_getaddrinfo(port_data[port].device_host_name, port_num, &hints, &port_data[port].addr_list, 13);

  if(gai_rc == 0)
  {
    /* ok try to connect */

    if (begin_critical_section(&connect_lock, "interface.c:net_init(2)" ) > 0)
    {
      fprintf(stderr,"connect_lock, Port = %d\n", port);
    }

    port_data[port].thread_status = 1;
    port_data[port].connect_status = -1;

    // If channel is != -1, we have a socket remaining from a previous
    // connect attempt.  Shutdown and close that socket, then create
    // a new one.
    if (port_data[port].channel != -1)      // We have a socket already
    {

      // Shut down and close the socket
      pthread_testcancel();   // Check for thread termination request
      stat = shutdown(port_data[port].channel,2);
      pthread_testcancel();   // Check for thread termination request
      if (debug_level & 2)
      {
        fprintf(stderr,"net_connect_thread():Net Shutdown 1 Returned %d, port %d\n",stat,port);
      }
      usleep(100000);         // 100ms
      pthread_testcancel();   // Check for thread termination request
      stat = close(port_data[port].channel);
      pthread_testcancel();   // Check for thread termination request
      if (debug_level & 2)
      {
        fprintf(stderr,"net_connect_thread():Net Close 1 Returned %d, port %d\n",stat,port);
      }
      usleep(100000);         // 100ms
      port_data[port].channel = -1;
    }

    if (end_critical_section(&connect_lock, "interface.c:net_init(3)" ) > 0)
    {
      fprintf(stderr,"connect_lock, Port = %d\n", port);
    }

    if (debug_level & 2)
    {
      fprintf(stderr,"Creating new thread\n");
    }
    if (pthread_create(&connect_thread, NULL, net_connect_thread, &port))
    {
      /* error starting thread*/
      ok = -1;
      fprintf(stderr,"Error creating net_connect thread, port %d\n",port);
    }

    busy_cursor(appshell);
    wait_time = sec_now() + NETWORK_WAITTIME;  // Set ending time for wait
    wait_on_connect = 1;
    while (wait_on_connect && (sec_now() < wait_time))
    {

      if (begin_critical_section(&connect_lock, "interface.c:net_init(4)" ) > 0)
      {
        fprintf(stderr,"connect_lock, Port = %d\n", port);
      }

      wait_on_connect = port_data[port].thread_status;

      if (end_critical_section(&connect_lock, "interface.c:net_init(5)" ) > 0)
      {
        fprintf(stderr,"connect_lock, Port = %d\n", port);
      }

      xastir_snprintf(st, sizeof(st), langcode("BBARSTA025"), wait_time - sec_now() );
      statusline(st,1);           // Host found, connecting n
      if (debug_level & 2)
      {
        fprintf(stderr,"%d\n", (int)(wait_time - sec_now()) );
      }

      usleep(250000);      // 250mS
    }

    ok = port_data[port].connect_status;

    /* thread did not return! kill it */
    if ( (sec_now() >= wait_time)      // Timed out
         || (ok != 1) )              // or connection failure of another type
    {
      if (debug_level & 2)
      {
        fprintf(stderr,"Thread exceeded it's time limit or failed to connect! Port %d\n",port);
      }

      if (begin_critical_section(&connect_lock, "interface.c:net_init(6)" ) > 0)
      {
        fprintf(stderr,"connect_lock, Port = %d\n", port);
      }

      if (debug_level & 2)
      {
        fprintf(stderr,"Killing thread\n");
      }
      if (pthread_cancel(connect_thread))
      {
        // The only error code we can get here is ESRCH, which means
        // that the thread number wasn't found.  The thread is already
        // dead, so let's not print out an error code.
      }

      if (sec_now() >= wait_time)    // Timed out
      {
        port_data[port].connect_status = -2;
        if (debug_level & 2)
        {
          fprintf(stderr,"It was a timeout.\n");
        }
      }

      if (end_critical_section(&connect_lock, "interface.c:net_init(7)" ) > 0)
      {
        fprintf(stderr,"connect_lock, Port = %d\n", port);
      }

      port_data[port].status = DEVICE_ERROR;
      if (debug_level & 2)
      {
        fprintf(stderr,"Thread did not return, port %d, DEVICE_ERROR ***\n",port);
      }

      // Show the latest status in the interface control dialog
      update_interface_list();
    }
    if (begin_critical_section(&connect_lock, "interface.c:net_init(8)" ) > 0)
    {
      fprintf(stderr,"connect_lock, Port = %d\n", port);
    }

    ok = port_data[port].connect_status;

    if (end_critical_section(&connect_lock, "interface.c:net_init(9)" ) > 0)
    {
      fprintf(stderr,"connect_lock, Port = %d\n", port);
    }

    if (debug_level & 2)
    {
      fprintf(stderr,"Net ok: %d, port %d\n", ok, port);
    }

    switch (ok)
    {
      case 1: /* connection up */
        xastir_snprintf(st, sizeof(st), langcode("BBARSTA020"), port_data[port].device_host_name);
        statusline(st,1);               // Connected to ...
        break;

      case 0:
        xastir_snprintf(st, sizeof(st), "%s", langcode("BBARSTA021"));
        statusline(st,1);               // Net Connection Failed!
        ok = -1;
        break;

      case -1:
        xastir_snprintf(st, sizeof(st), "%s", langcode("BBARSTA022"));
        statusline(st,1);               // Could not bind socket
        break;

      case -2:
        xastir_snprintf(st, sizeof(st), "%s", langcode("BBARSTA018"));
        statusline(st,1);               // Net Connection timed out
        ok = 0;
        break;

      default:
        break;
        /*break;*/
    }
  }
  else if (gai_rc == FAI_TIMEOUT)   /* host lookup time out */
  {
    xastir_snprintf(st, sizeof(st), "%s", langcode("BBARSTA018"));
    statusline(st,1);                       // Net Connection timed out
    port_data[port].status = DEVICE_ERROR;
    if (debug_level & 2)
    {
      fprintf(stderr,"Host lookup timeout, port %d, DEVICE_ERROR ***\n",port);
    }

    // Show the latest status in the interface control dialog
    update_interface_list();

    ok = 0;
  }
  else        /* Host ip look up failure (no ip address for that host) */
  {
    xastir_snprintf(st, sizeof(st), "%s", langcode("BBARSTA023"));
    statusline(st,1);                           // No IP for Host
    port_data[port].status = DEVICE_ERROR;
    if (debug_level & 2)
    {
      fprintf(stderr,"Host IP lookup failure, port %d, rc %d, DEVICE_ERROR ***\n",port, gai_rc);
    }

    // Show the latest status in the interface control dialog
    update_interface_list();
  }

  if (end_critical_section(&port_data_lock, "interface.c:net_init(10)" ) > 0)
  {
    fprintf(stderr,"port_data_lock, Port = %d\n", port);
  }

  if (debug_level & 2)
  {
    fprintf(stderr,"*** net_init is returning a %d ***\n",ok);
  }

  return(ok);
}





//**************************************************************
// This shuts down a network connection
//
//**************************************************************
int net_detach(int port)
{
  int ok;
  int max;
  int stat;
  char quiti[2];

  if (debug_level & 2)
  {
    fprintf(stderr,"Net detach Start, port %d\n",port);
  }

  ok = -1;
  max = 0;

  if (begin_critical_section(&port_data_lock, "interface.c:net_detach(1)" ) > 0)
  {
    fprintf(stderr,"net_detach():port_data_lock, Port = %d\n", port);
  }

  if (port_data[port].active == DEVICE_IN_USE)
  {
    if (port_data[port].status == DEVICE_UP && port_data[port].device_type == DEVICE_NET_STREAM)
    {

      if (debug_level & 2)
      {
        fprintf(stderr,"net_detach():Found port %d up, shutting it down\n",port);
      }

      quiti[0] = (char)4;
      quiti[1] = (char)0;
      if (port_data[port].status == DEVICE_UP)
      {
        port_write_string(port,quiti);
        usleep(100000); // 100ms
      }
      /* wait to write */
      while (port_data[port].status == DEVICE_UP && port_data[port].write_in_pos != port_data[port].write_out_pos && max < 25)
      {
        if (debug_level & 2)
        {
          fprintf(stderr,"net_detach():Waiting to finish writing data to port %d\n",port);
        }

        usleep(100000);    // 100ms
        max++;
      }
    }
    /*
      Shut down and Close were separated but this would cause sockets to
      just float around
    */

    /* we don't need to do a shut down on AX_25 devices */
    if ( (port_data[port].status == DEVICE_UP)
         && (port_data[port].device_type != DEVICE_AX25_TNC) )
    {
      stat = shutdown(port_data[port].channel,2);
      if (debug_level & 2)
      {
        fprintf(stderr,"net_detach():Net Shutdown Returned %d, port %d\n",stat,port);
      }
    }

    usleep(100000); // 100ms
    // We wish to close down the socket (so both ends of the darn thing
    // go away), but we want to keep the number on those systems that
    // re-assign the same file descriptor again.  This is to prevent
    // cross-connects from one interface to another in Xastir (big pain!).

    // Close it
    stat = close(port_data[port].channel);
    if (debug_level & 2)
    {
      fprintf(stderr,"net_detach():Net Close Returned %d, port %d\n",stat,port);
    }

    usleep(100000); // 100ms

    // Snag a socket again.  We'll use it next time around.
    port_data[port].channel = socket(PF_INET, SOCK_STREAM, 0);

    ok = 1;
  }
  /* close down no matter what */
  port_data[port].status = DEVICE_DOWN;
  //usleep(300);
  port_data[port].active = DEVICE_NOT_IN_USE;

  // Show the latest status in the interface control dialog
  update_interface_list();

  if (end_critical_section(&port_data_lock, "interface.c:net_detach(2)" ) > 0)
  {
    fprintf(stderr,"net_detach():port_data_lock, Port = %d\n", port);
  }

  if (debug_level & 2)
  {
    fprintf(stderr,"Net detach stop, port %d\n",port);
  }

  return(ok);
}





//***************************** STOP NETWORK FUNCTIONS **********************************



// This routine changes callsign chars to proper uppercase chars or
// numerals, fixes the callsign to six bytes, shifts the letters left by
// one bit, and puts the SSID number into the proper bits in the seventh
// byte.  The callsign as processed is ready for inclusion in an
// AX.25 header.
//
void fix_up_callsign(unsigned char *data, int data_size)
{
  unsigned char new_call[8] = "       ";  // Start with seven spaces
  int ssid = 0;
  int i;
  int j = 0;
  int digipeated_flag = 0;


  // Check whether we've digipeated through this callsign yet.
  if (strstr((const char *)data,"*") != 0)
  {
    digipeated_flag++;
  }

  // Change callsign to upper-case and pad out to six places with
  // space characters.
  for (i = 0; i < (int)strlen((const char *)data); i++)
  {
    data[i] = toupper(data[i]);

    if (data[i] == '-')     // Stop at '-'
    {
      break;
    }
    else if (data[i] == '*')
    {
    }
    else
    {
      new_call[j++] = data[i];
    }
  }
  new_call[7] = '\0';

  // Handle SSID.  'i' should now be pointing at a dash or at the
  // terminating zero character.
  if ( (i < (int)strlen((const char *)data)) && (data[i++] == '-') )     // We might have an SSID
  {
    if (data[i] != '\0')
    {
      ssid = atoi((const char *)&data[i]);
    }
  }

  if (ssid >= 0 && ssid <= 15)
  {
    new_call[6] = ssid | 0x30;  // Set 2 reserved bits
  }
  else    // Whacko SSID.  Set it to zero
  {
    new_call[6] = 0x30;     // Set 2 reserved bits
  }

  if (digipeated_flag)
  {
    new_call[6] = new_call[6] | 0x40; // Set the 'H' bit
  }

  // Shift each byte one bit to the left
  for (i = 0; i < 7; i++)
  {
    new_call[i] = new_call[i] << 1;
    new_call[i] = new_call[i] & 0xfe;
  }

  // Write over the top of the input string with the newly
  // formatted callsign
  xastir_snprintf((char *)data,
                  data_size,
                  "%s",
                  new_call);
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
void port_write_binary(int port, unsigned char *data, int length)
{
  int ii,erd;
  int write_in_pos_hold;


  erd = 0;

  if (begin_critical_section(&port_data[port].write_lock, "interface.c:port_write_binary(1)" ) > 0)
  {
    fprintf(stderr,"write_lock, Port = %d\n", port);
  }

  // Save the current position, just in case we have trouble
  write_in_pos_hold = port_data[port].write_in_pos;

  for (ii = 0; ii < length && !erd; ii++)
  {

    // Put character into write buffer and advance pointer
    port_data[port].device_write_buffer[port_data[port].write_in_pos++] = data[ii];

    // Check whether we need to wrap back to the start of the
    // circular buffer
    if (port_data[port].write_in_pos >= MAX_DEVICE_BUFFER)
    {
      port_data[port].write_in_pos = 0;
    }

    // Check whether we just filled our buffer (read/write
    // pointers are equal).  If so, exit gracefully, dumping
    // this string and resetting the write pointer.
    if (port_data[port].write_in_pos == port_data[port].write_out_pos)
    {
      if (debug_level & 2)
      {
        fprintf(stderr,"Port %d Buffer overrun\n",port);
      }

      // Restore original write_in pos and dump this string
      port_data[port].write_in_pos = write_in_pos_hold;
      port_data[port].errors++;
      erd = 1;
    }
  }

  if (end_critical_section(&port_data[port].write_lock, "interface.c:port_write_binary(2)" ) > 0)
  {
    fprintf(stderr,"write_lock, Port = %d\n", port);
  }
}





// Create an AX25 frame and then turn it into a KISS packet.  Dump
// it into the transmit queue.
//
void send_ax25_frame(int port, char *source, char *destination, char *path, char *data)
{
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


  // Check whether transmits are disabled globally
  if (transmit_disable)
  {
    return;
  }

  // Check whether transmit has been enabled for this interface.
  // If not, get out while the gettin's good.
  if (devices[port].transmit_data != 1)
  {
    return;
  }

  transmit_txt[0] = '\0';

  // Format the destination callsign
  xastir_snprintf((char *)temp_dest,
                  sizeof(temp_dest),
                  "%s",
                  destination);
  fix_up_callsign(temp_dest, sizeof(temp_dest));
  xastir_snprintf((char *)transmit_txt,
                  sizeof(transmit_txt),
                  "%s",
                  temp_dest);

  // Format the source callsign
  xastir_snprintf((char *)temp_source,
                  sizeof(temp_source),
                  "%s",
                  source);
  fix_up_callsign(temp_source, sizeof(temp_source));
  strncat((char *)transmit_txt,
          (char *)temp_source,
          sizeof(transmit_txt) - 1 - strlen((char *)transmit_txt));

  // Break up the path into individual callsigns and send them one
  // by one to fix_up_callsign().  If we get passed an empty path,
  // we merely skip this section and no path gets added to
  // "transmit_txt".
  j = 0;
  temp[0] = '\0'; // Start with empty path
  if ( (path != NULL) && (strlen(path) != 0) )
  {
    while (path[j] != '\0')
    {
      i = 0;
      while ( (path[j] != ',') && (path[j] != '\0') )
      {
        temp[i++] = path[j++];
      }
      temp[i] = '\0';

      if (path[j] == ',')     // Skip over comma
      {
        j++;
      }

      fix_up_callsign(temp, sizeof(temp));
      strncat((char *)transmit_txt,
              (char *)temp,
              sizeof(transmit_txt) - 1 - strlen((char *)transmit_txt));
    }
  }

  // Set the end-of-address bit on the last callsign in the
  // address field
  transmit_txt[strlen((const char *)transmit_txt) - 1] |= 0x01;

  // Add the Control byte
  control[0] = 0x03;
  control[1] = '\0';
  strncat((char *)transmit_txt,
          (char *)control,
          sizeof(transmit_txt) - 1 - strlen((char *)transmit_txt));

  // Add the PID byte
  pid[0] = 0xf0;
  pid[1] = '\0';
  strncat((char *)transmit_txt,
          (char *)pid,
          sizeof(transmit_txt) - 1 - strlen((char *)transmit_txt));

  // Append the information chars
  strncat((char *)transmit_txt,
          data,
          sizeof(transmit_txt) - 1 - strlen((char *)transmit_txt));

  // Add the KISS framing characters and do the proper escapes.
  j = 0;
  transmit_txt2[j++] = KISS_FEND;

  // Note:  This byte is where different interfaces would be
  // specified:
  transmit_txt2[j++] = 0x00;

  for (i = 0; i < (int)strlen((const char *)transmit_txt); i++)
  {
    c = transmit_txt[i];
    if (c == KISS_FEND)
    {
      transmit_txt2[j++] = KISS_FESC;
      transmit_txt2[j++] = KISS_TFEND;
    }
    else if (c == KISS_FESC)
    {
      transmit_txt2[j++] = KISS_FESC;
      transmit_txt2[j++] = KISS_TFESC;
    }
    else
    {
      transmit_txt2[j++] = c;
    }
  }
  transmit_txt2[j++] = KISS_FEND;

  // Terminate the string, but don't increment the 'j' counter.
  // We don't want to send the NULL byte out the KISS interface,
  // just make sure the string is terminated in all cases.
  //
  transmit_txt2[j] = '\0';

  //-------------------------------------------------------------------
  // Had to snag code from port_write_string() below because our string
  // needs to have 0x00 chars inside it.  port_write_string() can't
  // handle that case.  It's a good thing the transmit queue stuff
  // could handle it.
  //-------------------------------------------------------------------

  erd = 0;

  if (begin_critical_section(&port_data[port].write_lock, "interface.c:send_ax25_frame(1)" ) > 0)
  {
    fprintf(stderr,"write_lock, Port = %d\n", port);
  }

  write_in_pos_hold = port_data[port].write_in_pos;

  for (i = 0; i < j && !erd; i++)
  {
    port_data[port].device_write_buffer[port_data[port].write_in_pos++] = transmit_txt2[i];
    if (port_data[port].write_in_pos >= MAX_DEVICE_BUFFER)
    {
      port_data[port].write_in_pos = 0;
    }

    if (port_data[port].write_in_pos == port_data[port].write_out_pos)
    {
      if (debug_level & 2)
      {
        fprintf(stderr,"Port %d Buffer overrun\n",port);
      }

      /* clear this restore original write_in pos and dump this string */
      port_data[port].write_in_pos = write_in_pos_hold;
      port_data[port].errors++;
      erd = 1;
    }
  }

  if (end_critical_section(&port_data[port].write_lock, "interface.c:send_ax25_frame(2)" ) > 0)
  {
    fprintf(stderr,"write_lock, Port = %d\n", port);
  }
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
void send_kiss_config(int port, int device, int command, int value)
{
  unsigned char transmit_txt[MAX_LINE_SIZE+1];
  int i, j;
  int erd;
  int write_in_pos_hold;


  if (device < 0 || device > 15)
  {
    fprintf(stderr,"send_kiss_config: out-of-range value for device\n");
    return;
  }

  if (command < 1 || command > 6)
  {
    fprintf(stderr,"send_kiss_config: out-of-range value for command\n");
    return;
  }

  if (value < 0 || value > 255)
  {
    fprintf(stderr,"send_kiss_config: out-of-range value for value\n");
    return;
  }

  // Add the KISS framing characters and do the proper escapes.
  j = 0;
  transmit_txt[j++] = KISS_FEND;

  transmit_txt[j++] = (device << 4) | (command & 0x0f);

  transmit_txt[j++] = value & 0xff;

  transmit_txt[j++] = KISS_FEND;

  // Terminate the string, but don't increment the 'j' counter.
  // We don't want to send the NULL byte out the KISS interface,
  // just make sure the string is terminated in all cases.
  //
  transmit_txt[j] = '\0';




  //-------------------------------------------------------------------
  // Had to snag code from port_write_string() below because our string
  // needs to have 0x00 chars inside it.  port_write_string() can't
  // handle that case.  It's a good thing the transmit queue stuff
  // could handle it.
  //-------------------------------------------------------------------

  erd = 0;

  if (begin_critical_section(&port_data[port].write_lock, "interface.c:send_kiss_config(1)" ) > 0)
  {
    fprintf(stderr,"write_lock, Port = %d\n", port);
  }

  write_in_pos_hold = port_data[port].write_in_pos;

  for (i = 0; i < j && !erd; i++)
  {
    port_data[port].device_write_buffer[port_data[port].write_in_pos++] = transmit_txt[i];
    if (port_data[port].write_in_pos >= MAX_DEVICE_BUFFER)
    {
      port_data[port].write_in_pos = 0;
    }

    if (port_data[port].write_in_pos == port_data[port].write_out_pos)
    {
      if (debug_level & 2)
      {
        fprintf(stderr,"Port %d Buffer overrun\n",port);
      }

      /* clear this restore original write_in pos and dump this string */
      port_data[port].write_in_pos = write_in_pos_hold;
      port_data[port].errors++;
      erd = 1;
    }
  }

  if (end_critical_section(&port_data[port].write_lock, "interface.c:send_kiss_config(2)" ) > 0)
  {
    fprintf(stderr,"write_lock, Port = %d\n", port);
  }
}





//***********************************************************
// port_write_string()
//
// port is port# used
// data is the string to write
//***********************************************************

void port_write_string(int port, char *data)
{
  int i,erd;
  int write_in_pos_hold;

  if (data == NULL)
  {
    return;
  }

  if (data[0] == '\0')
  {
    return;
  }

  erd = 0;

  if (debug_level & 2)
  {
    fprintf(stderr,"CMD:%s\n",data);
  }

  if (begin_critical_section(&port_data[port].write_lock, "interface.c:port_write_string(1)" ) > 0)
  {
    fprintf(stderr,"write_lock, Port = %d\n", port);
  }

  write_in_pos_hold = port_data[port].write_in_pos;

  // Normal Serial/Net output?
  if (port_data[port].device_type != DEVICE_AX25_TNC)
  {
    for (i = 0; i < (int)strlen(data) && !erd; i++)
    {
      port_data[port].device_write_buffer[port_data[port].write_in_pos++] = data[i];
      if (port_data[port].write_in_pos >= MAX_DEVICE_BUFFER)
      {
        port_data[port].write_in_pos = 0;
      }

      if (port_data[port].write_in_pos == port_data[port].write_out_pos)
      {
        if (debug_level & 2)
        {
          fprintf(stderr,"Port %d Buffer overrun\n",port);
        }

        /* clear this restore original write_in pos and dump this string */
        port_data[port].write_in_pos = write_in_pos_hold;
        port_data[port].errors++;
        erd = 1;
      }
    }
  }

  // AX.25 port output
  else
  {
    port_data[port].bytes_output += strlen(data);
    data_out_ax25(port,(unsigned char *)data);
    /* do for interface indicators */
    if (port_data[port].write_in_pos >= MAX_DEVICE_BUFFER)
    {
      port_data[port].write_in_pos = 0;
    }
  }

  if (end_critical_section(&port_data[port].write_lock, "interface.c:port_write_string(2)" ) > 0)
  {
    fprintf(stderr,"write_lock, Port = %d\n", port);
  }
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

void port_read(int port)
{
  unsigned char cin;
  unsigned char buffer[MAX_DEVICE_BUFFER];    // Only used for AX.25 packets
  int i;
  struct timeval tmv;
  fd_set rd;
  int group;
  int binary_wx_data = 0;
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
  {
    fprintf(stderr,"Port %d read start\n",port);
  }

  group = 0;
  max = MAX_DEVICE_BUFFER - 1;
  cin = (unsigned char)0;

  // We stay in this read loop until the port is shut down
  while(port_data[port].active == DEVICE_IN_USE)
  {

    if (port_data[port].status == DEVICE_UP)
    {

      port_data[port].read_in_pos = 0;
      port_data[port].scan = 1;

      while (port_data[port].scan
             && (port_data[port].read_in_pos < (MAX_DEVICE_BUFFER - 1) )
             && (port_data[port].status == DEVICE_UP) )
      {

        int skip = 0;

        // Handle all EXCEPT AX25_TNC interfaces here
        if (port_data[port].device_type != DEVICE_AX25_TNC)
        {
          // Get one character
          port_data[port].scan = (int)read(port_data[port].channel,&cin,1);
        }

        else    // Handle AX25_TNC interfaces
        {
          /*
           * Use recvfrom on a network socket to know from
           * which interface the packet came - PE1DNN
           */

#ifdef __solaris__
          from_len = (unsigned int)sizeof(from);
#else   // __solaris__
          from_len = (socklen_t)sizeof(from);
#endif  // __solaris__

          port_data[port].scan = recvfrom(port_data[port].channel,buffer,
                                          sizeof(buffer) - 1,
                                          0,
                                          &from,
                                          &from_len);
        }


        // Below is code for ALL types of interfaces
        if (port_data[port].scan > 0 && port_data[port].status == DEVICE_UP )
        {

          if (port_data[port].device_type != DEVICE_AX25_TNC)
          {
            port_data[port].bytes_input += port_data[port].scan;  // Add character to read buffer
          }

          // Handle all EXCEPT AX25_TNC interfaces here
          if (port_data[port].device_type != DEVICE_AX25_TNC)
          {


            // Do special KISS packet processing here.
            // We save the last character in
            // port_data[port].channel2, as it is
            // otherwise only used for AX.25 ports.

            if ( (port_data[port].device_type == DEVICE_SERIAL_KISS_TNC)
                 || (port_data[port].device_type == DEVICE_SERIAL_MKISS_TNC) )
            {


              if (port_data[port].channel2 == KISS_FESC)   // Frame Escape char
              {
                if (cin == KISS_TFEND)   // Transposed Frame End char
                {

                  // Save this char for next time
                  // around
                  port_data[port].channel2 = cin;

                  cin = KISS_FEND;
                }
                else if (cin == KISS_TFESC)   // Transposed Frame Escape char
                {

                  // Save this char for next time
                  // around
                  port_data[port].channel2 = cin;

                  cin = KISS_FESC;
                }
                else
                {
                  port_data[port].channel2 = cin;
                }
              }
              else if (port_data[port].channel2 == KISS_FEND)   // Frame End char
              {
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
              else if (cin == KISS_FESC)   // Frame Escape char
              {
                port_data[port].channel2 = cin;
                skip++;
              }
              else
              {
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
            if (port_data[port].device_type == DEVICE_NET_AGWPE)
            {
              int bytes_available = 0;
              long frame_length = 0;


              skip = 1;   // Keeps next block of code from
              // trying to process this data.

              // Add it to the buffer
              if (port_data[port].read_in_pos < (MAX_DEVICE_BUFFER - 1) )
              {
                port_data[port].device_read_buffer[port_data[port].read_in_pos] = (char)cin;
                port_data[port].read_in_pos++;
                port_data[port].device_read_buffer[port_data[port].read_in_pos] = (char)0;
              }
              else
              {
                if (debug_level & 2)
                {
                  fprintf(stderr,"Port read overrun (1) on %d\n",port);
                }

                port_data[port].read_in_pos = 0;
              }

              bytes_available = port_data[port].read_in_pos - port_data[port].read_out_pos;
              if (bytes_available < 0)
              {
                bytes_available = (bytes_available + MAX_DEVICE_BUFFER) % MAX_DEVICE_BUFFER;
              }

              if (bytes_available >= 36)
              {
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

                // Have a complete AGWPE packet?  If
                // so, convert it to a more standard
                // packet format then feed it to our
                // decoding routines.
                //
                if (bytes_available >= (frame_length+36))
                {
                  char input_string[MAX_DEVICE_BUFFER];
                  char output_string[MAX_DEVICE_BUFFER];
                  int ii,jj,new_length;

                  my_pointer = port_data[port].read_out_pos;
                  jj = 0;
                  for (ii = 0; ii < frame_length+36; ii++)
                  {
                    input_string[jj++] = (unsigned char)port_data[port].device_read_buffer[my_pointer];
                    my_pointer = (my_pointer + 1) % MAX_DEVICE_BUFFER;
                  }

                  // Add a terminator.  We need
                  // this for the raw packets so
                  // that we don't end up getting
                  // portions of strings
                  // concatenated onto the end of
                  // our current packet during
                  // later processing.
                  input_string[jj] = '\0';

                  my_pointer = port_data[port].read_out_pos;

                  if ( parse_agwpe_packet((unsigned char *)input_string,
                                          frame_length+36,
                                          (unsigned char *)output_string,
                                          &new_length) )
                  {
                    channel_data(port,
                                 (unsigned char *)output_string,
                                 new_length+1); // include terminator
                  }

                  for (i = 0; i <= port_data[port].read_in_pos; i++)
                  {
                    port_data[port].device_read_buffer[i] = (char)0;
                  }

                  port_data[port].read_in_pos = 0;
                }
              }
              else
              {
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
                     || ( (cin == KISS_FEND) && (port_data[port].device_type == DEVICE_SERIAL_KISS_TNC) )
                     || ( (cin == KISS_FEND) && (port_data[port].device_type == DEVICE_SERIAL_MKISS_TNC) ) )
                 && port_data[port].data_type == 0)       // If end-of-line
            {

              // End serial/net type data send it to the decoder Put a terminating
              // zero at the end of the read-in data

              port_data[port].device_read_buffer[port_data[port].read_in_pos] = (char)0;

              if (port_data[port].status == DEVICE_UP && port_data[port].read_in_pos > 0)
              {
                int length;

                // Compute length of string in
                // circular queue

                // KISS TNC sends binary data
                if ( (port_data[port].device_type == DEVICE_SERIAL_KISS_TNC)
                     || (port_data[port].device_type == DEVICE_SERIAL_MKISS_TNC) )
                {

                  length = port_data[port].read_in_pos - port_data[port].read_out_pos;
                  if (length < 0)
                  {
                    length = (length + MAX_DEVICE_BUFFER) % MAX_DEVICE_BUFFER;
                  }

                  length++;
                }
                else    // ASCII data
                {
                  length = 0;
                }

                channel_data(port,
                             (unsigned char *)port_data[port].device_read_buffer,
                             length);   // Length of string
              }

              for (i = 0; i <= port_data[port].read_in_pos; i++)
              {
                port_data[port].device_read_buffer[i] = (char)0;
              }

              port_data[port].read_in_pos = 0;
            }
            else if (!skip)
            {

              // Check for binary WX station data
              if (port_data[port].data_type == 1 && (port_data[port].device_type == DEVICE_NET_WX ||
                                                     port_data[port].device_type == DEVICE_SERIAL_WX))
              {

                /* BINARY DATA input (WX data ?) */
                /* check RS WX200 */
                switch (cin)
                {

                  case 0x8f:
                  case 0x9f:
                  case 0xaf:
                  case 0xbf:
                  case 0xcf:

                    if (group == 0)
                    {
                      port_data[port].read_in_pos = 0;
                      group = (int)cin;
                      switch (cin)
                      {

                        case 0x8f:
                          max = 35;
                          binary_wx_data = 1;
                          break;

                        case 0x9f:
                          max = 34;
                          binary_wx_data = 1;
                          break;

                        case 0xaf:
                          max = 31;
                          binary_wx_data = 1;
                          break;

                        case 0xbf:
                          max = 14;
                          binary_wx_data = 1;
                          break;

                        case 0xcf:
                          max = 27;
                          binary_wx_data = 1;
                          break;

                        default:
                          break;
                      }
                    }
                    break;

                  default:
                    break;
                }
                if (port_data[port].read_in_pos < (MAX_DEVICE_BUFFER - 1) )
                {
                  port_data[port].device_read_buffer[port_data[port].read_in_pos] = (char)cin;
                  port_data[port].read_in_pos++;
                  port_data[port].device_read_buffer[port_data[port].read_in_pos] = (char)0;
                }
                else
                {
                  if (debug_level & 2)
                  {
                    fprintf(stderr,"Port read overrun (1) on %d\n",port);
                  }

                  port_data[port].read_in_pos = 0;
                }
                if (port_data[port].read_in_pos >= max)
                {
                  if (group != 0)     /* ok try to decode it */
                  {
                    int length = 0;
                    if (binary_wx_data)
                    {
                      length = port_data[port].read_in_pos - port_data[port].read_out_pos;
                      if (length < 0)
                      {
                        length = (length + MAX_DEVICE_BUFFER) % MAX_DEVICE_BUFFER;
                      }
                      length++;
                    }

                    channel_data(port,
                                 (unsigned char *)port_data[port].device_read_buffer,
                                 length);
                  }
                  max = MAX_DEVICE_BUFFER - 1;
                  group = 0;

                  port_data[port].read_in_pos = 0;
                }
              }
              else   /* Normal Data input */
              {

                if (cin == '\0')    // OWW WX daemon sends 0x00's!
                {
                  cin = '\n';
                }

                if (port_data[port].read_in_pos < (MAX_DEVICE_BUFFER - 1) )
                {
                  port_data[port].device_read_buffer[port_data[port].read_in_pos] = (char)cin;
                  port_data[port].read_in_pos++;
                  port_data[port].device_read_buffer[port_data[port].read_in_pos] = (char)0;
                }
                else
                {
                  if (debug_level & 2)
                  {
                    fprintf(stderr,"Port read overrun (2) on %d\n",port);
                  }

                  port_data[port].read_in_pos = 0;
                }
              }
            }

            // Ascii WX station data but no line-ends?
            if (port_data[port].read_in_pos > MAX_DEVICE_BUFFER_UNTIL_BINARY_SWITCH &&
                (port_data[port].device_type == DEVICE_NET_WX
                 || port_data[port].device_type == DEVICE_SERIAL_WX))
            {

              /* normal data on WX not found do look at data for binary WX */
              port_data[port].data_type++;
              port_data[port].data_type &= 1;
              port_data[port].read_in_pos = 0;
            }
          }   // End of non-AX.25 interface code block


          else      // Process ax25 interface data and send to the decoder
          {
            /*
             * Only accept data from our own interface (recvfrom will get
             * data from all AX.25 interfaces!) - PE1DNN
             */
#ifdef HAVE_LIBAX25
            if (port_data[port].device_name != NULL)
            {
              if ((dev = ax25_config_get_dev(port_data[port].device_name)) != NULL)
              {
                /* if the data is not from our interface, ignore it! PE1DNN */
                if(strcmp(dev, from.sa_data) == 0)
                {
                  /* Received data from our interface! - process data */
                  if (process_ax25_packet(buffer,
                                          port_data[port].scan,
                                          port_data[port].device_read_buffer,
                                          sizeof(port_data[port].device_read_buffer)) != NULL)
                  {
                    port_data[port].bytes_input += strlen(port_data[port].device_read_buffer);

                    channel_data(port,
                                 (unsigned char *)port_data[port].device_read_buffer,
                                 0);
                  }
                  /*
                    do this for interface indicator in this case we only do it for,
                    data from the correct AX.25 port
                  */
                  if (port_data[port].read_in_pos < (MAX_DEVICE_BUFFER - 1) )
                  {
                    port_data[port].read_in_pos += port_data[port].scan;
                  }
                  else
                  {

                    /* no buffer over runs writing a line at a time */
                    port_data[port].read_in_pos = 0;
                  }
                }
              }
            }
#endif /* HAVE_LIBAX25 */
          }   // End of AX.25 interface code block
        }
        else if (port_data[port].status == DEVICE_UP)      /* error or close on read */
        {
          port_data[port].errors++;
          if (port_data[port].scan == 0)
          {
            // Should not get this unless the device is down.  NOT TRUE!
            // We seem to also be able to get here if we're closing/restarting
            // another interface.  For that reason I commented out the below
            // statement so that this interface won't go down.  The inactivity
            // timer solves that issue now anyway.  --we7u.
            port_data[port].status = DEVICE_ERROR;

            // If the below statement is enabled, it causes an immediate reconnect
            // after one time-period of inactivity, currently 7.5 minutes, as set in
            // main.c:UpdateTime().  This means the symbol will never change from green
            // to red on the status bar, so the operator might not know about a
            // connection that is being constantly reconnected.  By leaving it commented
            // out we get one time period of red, and then it will reconnect at the 2nd
            // time period.  This means we can reconnect within 15 minutes if a line
            // goes dead.
            //
            port_data[port].reconnects = -1;     // Causes an immediate reconnect

            if (debug_level & 2)
            {
              fprintf(stderr,"end of file on read, or signal interrupted the read, port %d\n",port);
            }

            // Show the latest status in the interface control dialog
            update_interface_list();

          }
          else
          {
            if (port_data[port].scan == -1)
            {
              /* Should only get this if an real error occurs */
              port_data[port].status = DEVICE_ERROR;

              // If the below statement is enabled, it causes an immediate reconnect
              // after one time-period of inactivity, currently 7.5 minutes, as set in
              // main.c:UpdateTime().  This means the symbol will never change from green
              // to red on the status bar, so the operator might not know about a
              // connection that is being constantly reconnected.  By leaving it commented
              // out we get one time period of red, and then it will reconnect at the 2nd
              // time period.  This means we can reconnect within 15 minutes if a line
              // goes dead.
              //
              port_data[port].reconnects = -1;     // Causes an immediate reconnect

              // Show the latest status in the
              // interface control dialog
              update_interface_list();

              if (debug_level & 2)
              {
                fprintf(stderr,"error on read with error no %d, or signal interrupted the read, port %d, DEVICE_ERROR ***\n",
                        errno,port);
                switch (errno)
                {

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
    if (port_data[port].active == DEVICE_IN_USE)
    {

      // We need to delay here so that the thread doesn't use
      // high amounts of CPU doing nothing.

      // This select that waits on data and a timeout, so that if data
      // doesn't come in within a certain period of time, we wake up to
      // check whether the socket has gone down.  Else, we go back into
      // the select to wait for more data or a timeout.  FreeBSD has a
      // problem if this is less than 1ms.  Linux works ok down to 100us.
      // We don't need it anywhere near that short though.  We just need
      // to check whether the main thread has requested the interface be
      // closed, and so need to have this short enough to have reasonable
      // response time to the user.

      // Set up the select to block until data ready or 100ms
      // timeout, whichever occurs first.
      FD_ZERO(&rd);
      FD_SET(port_data[port].channel, &rd);
      tmv.tv_sec = 0;
      tmv.tv_usec = 100000;    // 100 ms
      (void)select(0,&rd,NULL,NULL,&tmv);
    }
  }

  if (debug_level & 2)
  {
    fprintf(stderr,"Thread for port %d read down!\n",port);
  }
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
void port_write(int port)
{
  int retval;
  struct timeval tmv;
  fd_set wd;
  int wait_max;
  unsigned long bytes_input;
  char write_buffer[MAX_DEVICE_BUFFER];
  int quantity;


  if (debug_level & 2)
  {
    fprintf(stderr,"Port %d write start\n",port);
  }

  init_critical_section(&port_data[port].write_lock);

  while(port_data[port].active == DEVICE_IN_USE)
  {

    if (port_data[port].status == DEVICE_UP)
    {
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
      {
        fprintf(stderr,"write_lock, Port = %d\n", port);
      }

      if ( (port_data[port].write_in_pos != port_data[port].write_out_pos)
           && port_data[port].status == DEVICE_UP)
      {
        // We have something in the buffer to transmit!


        // Handle control-C delay
        switch (port_data[port].device_type)
        {

          // Use this block for serial interfaces where we
          // need special delays for control-C character
          // processing in the TNC.
          case DEVICE_SERIAL_TNC_HSP_GPS:
          case DEVICE_SERIAL_TNC_AUX_GPS:
          case DEVICE_SERIAL_TNC:

            // Are we trying to send a control-C?  If so, wait a
            // special amount of time _before_ we send
            // it out the serial port.
            if (port_data[port].device_write_buffer[port_data[port].write_out_pos] == (char)0x03)
            {
              // Sending control-C.

              if (debug_level & 128)
              {
                fprintf(stderr,"Writing command [%x] on port %d, at pos %d\n",
                        *(port_data[port].device_write_buffer +
                          port_data[port].write_out_pos),
                        port, port_data[port].write_out_pos);
              }

              wait_max = 0;
              bytes_input = port_data[port].bytes_input + 40;
              while ( (port_data[port].bytes_input != bytes_input)
                      && (port_data[port].status == DEVICE_UP)
                      && (wait_max < 100) )
              {
                bytes_input = port_data[port].bytes_input;

                /*wait*/
                FD_ZERO(&wd);
                FD_SET(port_data[port].channel, &wd);
                tmv.tv_sec = 0;
                tmv.tv_usec = 80000l;   // Delay 80ms
                (void)select(0,NULL,&wd,NULL,&tmv);
                wait_max++;
              }
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
        switch (port_data[port].device_type)
        {

          // Use this block for serial interfaces where we
          // need character pacing and so must send one
          // character per write.
          case DEVICE_SERIAL_TNC_HSP_GPS:
          case DEVICE_SERIAL_TNC_AUX_GPS:
          case DEVICE_SERIAL_KISS_TNC:
          case DEVICE_SERIAL_MKISS_TNC:
          case DEVICE_SERIAL_TNC:
          case DEVICE_SERIAL_GPS:
          case DEVICE_SERIAL_WX:
            // Do the actual write here, one character
            // at a time for these types of interfaces.

            retval = (int)write(port_data[port].channel,
                                &port_data[port].device_write_buffer[port_data[port].write_out_pos],
                                1);

            pthread_testcancel();   // Check for thread termination request

            if (retval == 1)    // We succeeded in writing one byte
            {

              port_data[port].bytes_output++;

              port_data[port].write_out_pos++;
              if (port_data[port].write_out_pos >= MAX_DEVICE_BUFFER)
              {
                port_data[port].write_out_pos = 0;
              }

            }
            else
            {
              /* error of some kind */
              port_data[port].errors++;
              port_data[port].status = DEVICE_ERROR;

              // If the below statement is enabled, it causes an immediate reconnect
              // after one time-period of inactivity, currently 7.5 minutes, as set in
              // main.c:UpdateTime().  This means the symbol will never change from green
              // to red on the status bar, so the operator might not know about a
              // connection that is being constantly reconnected.  By leaving it commented
              // out we get one time period of red, and then it will reconnect at the 2nd
              // time period.  This means we can reconnect within 15 minutes if a line
              // goes dead.
              //
              port_data[port].reconnects = -1;     // Causes an immediate reconnect

              if (retval == 0)
              {
                /* Should not get this unless the device is down */
                if (debug_level & 2)
                {
                  fprintf(stderr,"no data written %d, DEVICE_ERROR ***\n",port);
                }
              }
              else
              {
                if (retval == -1)
                {
                  /* Should only get this if an real error occurs */
                  if (debug_level & 2)
                  {
                    fprintf(stderr,"error on write with error no %d, or port %d\n",errno,port);
                  }
                }
              }
              // Show the latest status in the interface control dialog
              update_interface_list();
            }
            if (serial_char_pacing > 0)
            {
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
            while (port_data[port].write_in_pos != port_data[port].write_out_pos)
            {

              write_buffer[quantity] = port_data[port].device_write_buffer[port_data[port].write_out_pos];

              port_data[port].write_out_pos++;
              if (port_data[port].write_out_pos >= MAX_DEVICE_BUFFER)
              {
                port_data[port].write_out_pos = 0;
              }

              quantity++;
            }

            retval = (int)write(port_data[port].channel,
                                write_buffer,
                                quantity);

            pthread_testcancel();   // Check for thread termination request

            if (retval == quantity)    // We succeeded in writing one byte
            {
              port_data[port].bytes_output++;
            }
            else
            {
              /* error of some kind */
              port_data[port].errors++;
              port_data[port].status = DEVICE_ERROR;

              // If the below statement is enabled, it causes an immediate reconnect
              // after one time-period of inactivity, currently 7.5 minutes, as set in
              // main.c:UpdateTime().  This means the symbol will never change from green
              // to red on the status bar, so the operator might not know about a
              // connection that is being constantly reconnected.  By leaving it commented
              // out we get one time period of red, and then it will reconnect at the 2nd
              // time period.  This means we can reconnect within 15 minutes if a line
              // goes dead.
              //
              port_data[port].reconnects = -1;     // Causes an immediate reconnect

              if (retval == 0)
              {
                /* Should not get this unless the device is down */
                if (debug_level & 2)
                {
                  fprintf(stderr,"no data written %d, DEVICE_ERROR ***\n",port);
                }
              }
              else
              {
                if (retval == -1)
                {
                  /* Should only get this if an real error occurs */
                  if (debug_level & 2)
                  {
                    fprintf(stderr,"error on write with error no %d, or port %d\n",errno,port);
                  }
                }
              }
              // Show the latest status in the interface control dialog
              update_interface_list();
            }
            break;

        }   // End of switch
        // End of handling method of sending data (1 or multiple char per TX)


      }

      if (end_critical_section(&port_data[port].write_lock, "interface.c:port_write(2)" ) > 0)
      {
        fprintf(stderr,"write_lock, Port = %d\n", port);
      }

      // Remove the cleanup routine for the case where this
      // thread gets killed while the mutex is locked.  The
      // cleanup routine initiates an unlock before the thread
      // dies.  We must be in deferred cancellation mode for
      // the thread to have this work properly.
      //
      pthread_cleanup_pop(0);
    }

    if (port_data[port].active == DEVICE_IN_USE)
    {

      // Delay here so that the thread doesn't use high
      // amounts of CPU doing _nothing_.  Take this delay out
      // and the thread will take lots of CPU time.

      // Try to change this to a select that waits on data and a timeout,
      // so that if data doesn't come in within a certain period of time,
      // we wake up to check whether the socket has gone down.  Else, we
      // go back into the select to wait for more data or a timeout.
      // FreeBSD has a problem if this is less than 1ms.  Linux works ok
      // down to 100us.  Theoretically we don't need it anywhere near that
      // short, we just need to check whether the main thread has
      // requested the interface be closed, and so need to have this short
      // enough to have reasonable response time to the user.
      // Unfortunately it has been reported that having this at 100ms
      // causes about 9 seconds of delay when transmitting to a KISS TNC,
      // so it's good to keep this short also.

      FD_ZERO(&wd);
      FD_SET(port_data[port].channel, &wd);
      tmv.tv_sec = 0;
      tmv.tv_usec = 2000;  // Delay 2ms
      (void)select(0,NULL,&wd,NULL,&tmv);
    }
  }
  if (debug_level & 2)
  {
    fprintf(stderr,"Thread for port %d write down!\n",port);
  }
}





//***********************************************************
// read_access_port_thread()
//
// Port read thread.
// port is port# used
//
// open threads for reading data from this port.
//***********************************************************
static void* read_access_port_thread(void *arg)
{
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
static void* write_access_port_thread(void *arg)
{
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
int start_port_threads(int port)
{
  int ok;

  port_id[port] = port;
  if (debug_level & 2)
  {
    fprintf(stderr,"Start port %d threads\n",port);
  }

  ok = 1;
  if (port_data[port].active == DEVICE_IN_USE && port_data[port].status == DEVICE_UP)
  {
    if (debug_level & 2)
    {
      fprintf(stderr,"*** Startup of read/write threads for port %d ***\n",port);
    }

    /* start the two threads */
    if (pthread_create(&port_data[port].read_thread, NULL, read_access_port_thread, &port_id[port]))
    {
      /* error starting read thread*/
      fprintf(stderr,"Error starting read thread, port %d\n",port);
      port_data[port].read_thread = 0;
      ok = -1;
    }
    else if (pthread_create(&port_data[port].write_thread, NULL, write_access_port_thread, &port_id[port]))
    {
      /* error starting write thread*/
      fprintf(stderr,"Error starting write thread, port %d\n",port);
      port_data[port].write_thread = 0;
      ok = -1;
    }

  }
  else if (debug_level & 2)
  {
    fprintf(stderr,"*** Skipping startup of read/write threads for port %d ***\n",port);
  }

  if (debug_level & 2)
  {
    fprintf(stderr,"End port %d threads\n",port);
  }

  return(ok);
}





//***********************************************************
// Clear Port Data
// int port to be cleared
//***********************************************************
void clear_port_data(int port, int clear_more)
{

  if (begin_critical_section(&port_data_lock, "interface.c:clear_port_data(1)" ) > 0)
  {
    fprintf(stderr,"port_data_lock, Port = %d\n", port);
  }

  port_data[port].device_type = -1;
  port_data[port].active = DEVICE_NOT_IN_USE;
  port_data[port].status = DEVICE_DOWN;

  // Show the latest status in the interface control dialog
  update_interface_list();

  port_data[port].device_name[0] = '\0';
  port_data[port].device_host_name[0] = '\0';
  port_data[port].addr_list = NULL;

  if (begin_critical_section(&connect_lock, "interface.c:clear_port_data(2)" ) > 0)
  {
    fprintf(stderr,"connect_lock, Port = %d\n", port);
  }

  port_data[port].thread_status = -1;
  port_data[port].connect_status = -1;
  port_data[port].read_thread = 0;
  port_data[port].write_thread = 0;

  if (end_critical_section(&connect_lock, "interface.c:clear_port_data(3)" ) > 0)
  {
    fprintf(stderr,"connect_lock, Port = %d\n", port);
  }

  port_data[port].decode_errors = 0;
  port_data[port].data_type = 0;
  port_data[port].socket_port = -1;
  port_data[port].device_host_pswd[0] = '\0';

  if (clear_more)
  {
    port_data[port].channel = -1;
  }

  port_data[port].channel2 = -1;
  port_data[port].ui_call[0] = '\0';
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
  {
    fprintf(stderr,"port_data_lock, Port = %d\n", port);
  }
}





//***********************************************************
// Clear All Port Data
//***********************************************************
void clear_all_port_data(void)
{
  int i;

  for (i = 0; i < MAX_IFACE_DEVICES; i++)
  {
    clear_port_data(i,1);
  }
}





//***********************************************************
// INIT Device names Data
//***********************************************************
void init_device_names(void)
{
  xastir_snprintf(dtype[DEVICE_NONE].device_name,
                  sizeof(dtype[DEVICE_NONE].device_name),
                  "%s",
                  langcode("IFDNL00000"));
  xastir_snprintf(dtype[DEVICE_SERIAL_TNC].device_name,
                  sizeof(dtype[DEVICE_SERIAL_TNC].device_name),
                  "%s",
                  langcode("IFDNL00001"));
  xastir_snprintf(dtype[DEVICE_SERIAL_TNC_HSP_GPS].device_name,
                  sizeof(dtype[DEVICE_SERIAL_TNC_HSP_GPS].device_name),
                  "%s",
                  langcode("IFDNL00002"));
  xastir_snprintf(dtype[DEVICE_SERIAL_GPS].device_name,
                  sizeof(dtype[DEVICE_SERIAL_GPS].device_name),
                  "%s",
                  langcode("IFDNL00003"));
  xastir_snprintf(dtype[DEVICE_SERIAL_WX].device_name,
                  sizeof(dtype[DEVICE_SERIAL_WX].device_name),
                  "%s",
                  langcode("IFDNL00004"));
  xastir_snprintf(dtype[DEVICE_NET_STREAM].device_name,
                  sizeof(dtype[DEVICE_NET_STREAM].device_name),
                  "%s",
                  langcode("IFDNL00005"));
  xastir_snprintf(dtype[DEVICE_AX25_TNC].device_name,
                  sizeof(dtype[DEVICE_AX25_TNC].device_name),
                  "%s",
                  langcode("IFDNL00006"));
  xastir_snprintf(dtype[DEVICE_NET_GPSD].device_name,
                  sizeof(dtype[DEVICE_NET_GPSD].device_name),
                  "%s",
                  langcode("IFDNL00007"));
  xastir_snprintf(dtype[DEVICE_NET_WX].device_name,
                  sizeof(dtype[DEVICE_NET_WX].device_name),
                  "%s",
                  langcode("IFDNL00008"));
  xastir_snprintf(dtype[DEVICE_SERIAL_TNC_AUX_GPS].device_name,
                  sizeof(dtype[DEVICE_SERIAL_TNC_AUX_GPS].device_name),
                  "%s",
                  langcode("IFDNL00009"));
  xastir_snprintf(dtype[DEVICE_SERIAL_KISS_TNC].device_name,
                  sizeof(dtype[DEVICE_SERIAL_KISS_TNC].device_name),
                  "%s",
                  langcode("IFDNL00010"));
  xastir_snprintf(dtype[DEVICE_NET_DATABASE].device_name,
                  sizeof(dtype[DEVICE_NET_DATABASE].device_name),
                  "%s",
                  langcode("IFDNL00011"));
  xastir_snprintf(dtype[DEVICE_NET_AGWPE].device_name,
                  sizeof(dtype[DEVICE_NET_AGWPE].device_name),
                  "%s",
                  langcode("IFDNL00012"));
  xastir_snprintf(dtype[DEVICE_SERIAL_MKISS_TNC].device_name,
                  sizeof(dtype[DEVICE_SERIAL_MKISS_TNC].device_name),
                  "%s",
                  langcode("IFDNL00013"));

#ifdef HAVE_DB
  // SQL Database (experimental)
  xastir_snprintf(dtype[DEVICE_SQL_DATABASE].device_name,
                  sizeof(dtype[DEVICE_SQL_DATABASE].device_name),
                  "%s",
                  langcode("IFDNL00014"));
#endif /* HAVE_DB */

}





//***********************************************************
// Delete Device.  Shuts down active port/ports.
//***********************************************************
int del_device(int port)
{
  int ok;
  char temp[300];
  long wait_time = 0;


  if (debug_level & 2)
  {
    fprintf(stderr,"Delete Device start\n");
  }

  ok = -1;
  switch (port_data[port].device_type)
  {

    case(DEVICE_SERIAL_TNC):
    case(DEVICE_SERIAL_KISS_TNC):
    case(DEVICE_SERIAL_MKISS_TNC):
    case(DEVICE_SERIAL_GPS):
    case(DEVICE_SERIAL_WX):
    case(DEVICE_SERIAL_TNC_HSP_GPS):
    case(DEVICE_SERIAL_TNC_AUX_GPS):

      switch (port_data[port].device_type)
      {

        case DEVICE_SERIAL_TNC:

          if (debug_level & 2)
          {
            fprintf(stderr,"Close a Serial TNC device\n");
          }

          begin_critical_section(&devices_lock, "interface.c:del_device" );

          xastir_snprintf(temp, sizeof(temp), "config/%s", devices[port].tnc_down_file);

          end_critical_section(&devices_lock, "interface.c:del_device" );

          (void)command_file_to_tnc_port(port,get_data_base_dir(temp));
          break;

        case DEVICE_SERIAL_KISS_TNC:
          if (debug_level & 2)
          {
            fprintf(stderr,"Close a Serial KISS TNC device\n");
          }
          break;

        case DEVICE_SERIAL_MKISS_TNC:
          if (debug_level & 2)
          {
            fprintf(stderr,"Close a Serial MKISS TNC device\n");
          }
          break;

        case DEVICE_SERIAL_GPS:
          if (debug_level & 2)
          {
            fprintf(stderr,"Close a Serial GPS device\n");
          }
          if (using_gps_position)
          {
            using_gps_position--;
          }
          break;

        case DEVICE_SERIAL_WX:
          if (debug_level & 2)
          {
            fprintf(stderr,"Close a Serial WX device\n");
          }

          break;

        case DEVICE_SERIAL_TNC_HSP_GPS:
          if (debug_level & 2)
          {
            fprintf(stderr,"Close a Serial TNC w/HSP GPS\n");
          }
          if (using_gps_position)
          {
            using_gps_position--;
          }

          begin_critical_section(&devices_lock, "interface.c:del_device" );

          xastir_snprintf(temp, sizeof(temp), "config/%s", devices[port].tnc_down_file);

          end_critical_section(&devices_lock, "interface.c:del_device" );

          (void)command_file_to_tnc_port(port,get_data_base_dir(temp));
          break;

        case DEVICE_SERIAL_TNC_AUX_GPS:
          if (debug_level & 2)
          {
            fprintf(stderr,"Close a Serial TNC w/AUX GPS\n");
          }
          if (using_gps_position)
          {
            using_gps_position--;
          }

          begin_critical_section(&devices_lock, "interface.c:del_device");

          sprintf(temp, "config/%s", devices[port].tnc_down_file);

          end_critical_section(&devices_lock, "interface.c:del_device");

          (void)command_file_to_tnc_port(port,
                                         get_data_base_dir(temp));
          break;

        default:
          break;
      }   // End of switch


      // Let the write queue empty before we return, to make
      // sure all of the data gets written out.
      while ( (port_data[port].write_in_pos != port_data[port].write_out_pos)
              && port_data[port].status == DEVICE_UP)
      {

        // Check whether we're hung waiting on the device
        if (wait_time > SERIAL_MAX_WAIT)
        {
          break;  // Break out of the while loop
        }

        sched_yield();
        usleep(25000);    // 25ms
        wait_time = wait_time + 25000;
      }


      if (debug_level & 2)
      {
        fprintf(stderr,"Serial detach\n");
      }

      ok = serial_detach(port);
      break;

    case(DEVICE_NET_STREAM):
    case(DEVICE_AX25_TNC):
    case(DEVICE_NET_GPSD):
    case(DEVICE_NET_WX):
    case(DEVICE_NET_DATABASE):
    case(DEVICE_NET_AGWPE):

      switch (port_data[port].device_type)
      {

        case DEVICE_NET_STREAM:
          if (debug_level & 2)
          {
            fprintf(stderr,"Close a Network stream\n");
          }
          break;

        case DEVICE_AX25_TNC:
          if (debug_level & 2)
          {
            fprintf(stderr,"Close a AX25 TNC device\n");
          }
          break;

        case DEVICE_NET_GPSD:
          if (debug_level & 2)
          {
            fprintf(stderr,"Close a Network GPSd stream\n");
          }
          if (using_gps_position)
          {
            using_gps_position--;
          }
          break;

        case DEVICE_NET_WX:
          if (debug_level & 2)
          {
            fprintf(stderr,"Close a Network WX stream\n");
          }
          break;

        case DEVICE_NET_DATABASE:
          if (debug_level & 2)
          {
            fprintf(stderr,"Close a Network Database stream\n");
          }
          break;

        case DEVICE_NET_AGWPE:
          if (debug_level & 2)
          {
            fprintf(stderr,"Close a Network AGWPE stream\n");
          }
          break;

        default:
          break;
      }
      if (debug_level & 2)
      {
        fprintf(stderr,"Net detach\n");
      }

      ok = net_detach(port);
      break;

#ifdef HAVE_DB
    case DEVICE_SQL_DATABASE:
      if (debug_level & 2)
      {
        fprintf(stderr,"Close connection to database on device %d\n",port);
      }
      if (port_data[port].status==DEVICE_UP)
      {
        ok = closeConnection(&connections[port],port);
      }
      // remove the connection from the list of open connections
      /* clear port active */
      port_data[port].active = DEVICE_NOT_IN_USE;
      /* clear port status */
      port_data[port].active = DEVICE_DOWN;
      update_interface_list();
      fprintf(stderr,"Closed connection to database on device %d\n",port);
      break;
#endif /* HAVE_DB */

    default:
      break;
  }

  if (ok)
  {
    int retvalue;

    if (debug_level & 2)
    {
      fprintf(stderr,"port detach OK\n");
    }

    usleep(100000);    // 100ms
    if (debug_level & 2)
    {
      fprintf(stderr,"Cancel threads\n");
    }

    if (begin_critical_section(&port_data_lock, "interface.c:del_device(1)" ) > 0)
    {
      fprintf(stderr,"port_data_lock, Port = %d\n", port);
    }

    if (begin_critical_section(&connect_lock, "interface.c:del_device(2)" ) > 0)
    {
      fprintf(stderr,"connect_lock, Port = %d\n", port);
    }

    if (port_data[port].read_thread != 0)   // If we have a thread defined
    {
      retvalue = pthread_cancel(port_data[port].read_thread);
      if (retvalue == ESRCH)
      {
      }
    }

    if (port_data[port].write_thread != 0)      // If we have a thread defined
    {
      retvalue = pthread_cancel(port_data[port].write_thread);
      // we used to test retvalue against ESRCH and throw a
      // warning if this failed, but it got commented out a very
      // long time ago (around 2003).
    }

    if (end_critical_section(&connect_lock, "interface.c:del_device(3)" ) > 0)
    {
      fprintf(stderr,"connect_lock, Port = %d\n", port);
    }

    if (end_critical_section(&port_data_lock, "interface.c:del_device(4)" ) > 0)
    {
      fprintf(stderr,"port_data_lock, Port = %d\n", port);
    }

    usleep(100000); // 100ms
  }
  else
  {
    if (debug_level & 2)
    {
      fprintf(stderr,"Port %d could not be closed\n",port);
    }
  }
  usleep(10);

  // Cover the case where someone plays with a GPS interface or
  // three and then turns it/them off again: They won't send a
  // posit again until the next restart or whenever they enable a
  // GPS interface again that has good data, unless we set this
  // variable again for them.
  if (!using_gps_position)
  {
    my_position_valid = 1;
  }

  return(ok);
}


#ifdef HAVE_DB
/* Add a device, passing it a pointer to the ioparam
 * that describes the interface to start up, rather than passing
 * an extracted list of elements
 *
 * temporary addition for testing sql_database_functionality
 * when working, needs to be integrated into add_device
 */
int add_device_by_ioparam(int port_avail, ioparam *device)
{
  int ok;
  int got_conn;
  int done = 0;
  DataRow *dr;
  ok = -1;

  if (port_avail >= 0)
  {

    switch (device->device_type)
    {
      case DEVICE_SQL_DATABASE:
        if (debug_level & 4096)
        {
          fprintf(stderr,"Opening a sql db connection to %s\n",device->device_host_name);
        }
        clear_port_data(port_avail,0);

        port_data[port_avail].device_type = DEVICE_SQL_DATABASE;
        xastir_snprintf(port_data[port_avail].device_host_name,
                        sizeof(port_data[port_avail].device_host_name),
                        "%s",
                        device->device_host_name);
        if (connections_initialized==0)
        {
          if (debug_level & 4096)
          {
            fprintf(stderr,"Calling initConnections in add_device_by_ioparam\n");
          }
          fprintf(stderr,"adddevice, initializing connections");
          connections_initialized = initConnections();
        }
        if (debug_level & 4096)
        {
          fprintf(stderr,"Opening (in interfaces) device on port [%d] with connection [%p]\n",port_avail,&connections[port_avail]);
          fprintf(stderr,"device [%p][%p] device_type=%d\n",device,&device,device->device_type);
        }
        got_conn = 0;
        got_conn=openConnection(device, &connections[port_avail]);
        if (debug_level & 4096)
        {
          fprintf(stderr,"got_conn connections[%d] [%p] result=%d\n",port_avail,&connections[port_avail],got_conn);
          if (got_conn==1)
          {
            fprintf(stderr,"got_conn connection type %d\n",connections[port_avail].type);
          }
        }
        if ((got_conn == 1) && (!(connections[port_avail].type==NULL)))
        {
          if (debug_level & 4096)
          {
            fprintf(stderr, "Opened connection [%d] type=[%d]\n",port_avail,connections[port_avail].type);
          }
          ok = 1;
          port_data[port_avail].active = DEVICE_IN_USE;
          port_data[port_avail].status = DEVICE_UP;
        }
        else
        {
          port_data[port_avail].active = DEVICE_IN_USE;
          port_data[port_avail].status = DEVICE_ERROR;
        }
        // Show the latest status in the interface control dialog
        update_interface_list();
        if (ok == 1)
        {
          /* if connected save top of call list */
          ok = storeStationSimpleToGisDb(&connections[port_avail], n_first);
          if (ok==1)
          {
            if (debug_level & 4096)
            {
              fprintf(stderr,"Stored station n_first\n");
            }
            // iterate through station_pointers and write all stations currently known
            dr = n_first->n_next;
            if (dr!=NULL)
            {
              while (done==0)
              {
                if (debug_level & 4096)
                {
                  fprintf(stderr,"storing additional stations\n");
                }
                // Need to check that stations aren't from the database
                // preventing creation of duplicate round trip records.
                ok = storeStationSimpleToGisDb(&connections[port_avail], dr);
                if (ok==1)
                {
                  dr = dr->n_next;
                  if (dr==NULL)
                  {
                    done = 1;
                  }
                }
                else
                {
                  done = 1;
                }
              }
            }
          }
        }
    }
  }
  return ok;
}
#endif /* HAVE_DB */


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
               int dev_sp,int dev_sty,int reconnect, char *filter_string)
{
  char logon_txt[600];
  char init_kiss_string[5];   // KISS-mode on startup
  int ok;
  char temp[300];
  char verstr[15];

  if ( (dev_nm == NULL) || (passwd == NULL) )
  {
    return(-1);
  }

  if (dev_nm[0] == '\0')
  {
    return(-1);
  }

  xastir_snprintf(verstr,
                  sizeof(verstr),
                  "XASTIR %s",
                  VERSION);

  ok = -1;
  if (port_avail >= 0)
  {
    if (debug_level & 2)
    {
      fprintf(stderr,"Port Available %d\n",port_avail);
    }

    switch(dev_type)
    {

      case DEVICE_SERIAL_TNC:
      case DEVICE_SERIAL_KISS_TNC:
      case DEVICE_SERIAL_MKISS_TNC:
      case DEVICE_SERIAL_GPS:
      case DEVICE_SERIAL_WX:
      case DEVICE_SERIAL_TNC_HSP_GPS:
      case DEVICE_SERIAL_TNC_AUX_GPS:

        switch (dev_type)
        {

          case DEVICE_SERIAL_TNC:
            if (debug_level & 2)
            {
              fprintf(stderr,"Opening a Serial TNC device\n");
            }

            break;

          case DEVICE_SERIAL_KISS_TNC:
            if (debug_level & 2)
            {
              fprintf(stderr,"Opening a Serial KISS TNC device\n");
            }

            break;

          case DEVICE_SERIAL_MKISS_TNC:
            if (debug_level & 2)
            {
              fprintf(stderr,"Opening a Serial MKISS TNC device\n");
            }

            break;

          case DEVICE_SERIAL_GPS:
            if (debug_level & 2)
            {
              fprintf(stderr,"Opening a Serial GPS device\n");
            }
            // Must wait for valid GPS parsing after
            // sending one posit.
            my_position_valid = 1;
            using_gps_position++;
            statusline(langcode("BBARSTA041"),1);
            break;

          case DEVICE_SERIAL_WX:
            if (debug_level & 2)
            {
              fprintf(stderr,"Opening a Serial WX device\n");
            }

            break;

          case DEVICE_SERIAL_TNC_HSP_GPS:
            if (debug_level & 2)
            {
              fprintf(stderr,"Opening a Serial TNC w/HSP GPS device\n");
            }
            // Must wait for valid GPS parsing after
            // sending one posit.
            my_position_valid = 1;
            using_gps_position++;
            statusline(langcode("BBARSTA041"),1);
            break;

          case DEVICE_SERIAL_TNC_AUX_GPS:
            if (debug_level & 2)
            {
              fprintf(stderr,"Opening a Serial TNC w/AUX GPS device\n");
            }
            // Must wait for valid GPS parsing after
            // sending one posit.
            my_position_valid = 1;
            using_gps_position++;
            statusline(langcode("BBARSTA041"),1);
            break;

          default:
            break;
        }
        clear_port_data(port_avail,0);

        port_data[port_avail].device_type = dev_type;
        xastir_snprintf(port_data[port_avail].device_name,
                        sizeof(port_data[port_avail].device_name),
                        "%s",
                        dev_nm);
        port_data[port_avail].sp = dev_sp;
        port_data[port_avail].style = dev_sty;
        if (dev_type == DEVICE_SERIAL_WX)
        {
          if (strcmp("1",passwd) == 0)
          {
            port_data[port_avail].data_type = 1;
          }
        }

        ok = serial_init(port_avail);
        break;

      case DEVICE_NET_STREAM:
        if (debug_level & 2)
        {
          fprintf(stderr,"Opening a Network stream\n");
        }

        clear_port_data(port_avail,0);

        port_data[port_avail].device_type = DEVICE_NET_STREAM;
        xastir_snprintf(port_data[port_avail].device_host_name,
                        sizeof(port_data[port_avail].device_host_name),
                        "%s",
                        dev_nm);
        xastir_snprintf(port_data[port_avail].device_host_pswd,
                        sizeof(port_data[port_avail].device_host_pswd),
                        "%s",
                        passwd);
        port_data[port_avail].socket_port = dev_sck_p;
        port_data[port_avail].reconnect = reconnect;

        ok = net_init(port_avail);

        if (ok == 1)
        {

          /* if connected now send password */
          if (strlen(passwd))
          {

            if (filter_string != NULL
                && strlen(filter_string) > 0)      // Filter specified
            {

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
            else    // No filter specified
            {
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
          else
          {
            xastir_snprintf(logon_txt,
                            sizeof(logon_txt),
                            "user %s pass -1 vers %s %c%c",
                            my_callsign,
                            verstr,
                            '\r',
                            '\n');
          }
          port_write_string(port_avail,logon_txt);
        }
        break;

      case DEVICE_AX25_TNC:
        if (debug_level & 2)
        {
          fprintf(stderr,"Opening a network AX25 TNC\n");
        }

        clear_port_data(port_avail,0);

        port_data[port_avail].device_type = DEVICE_AX25_TNC;
        xastir_snprintf(port_data[port_avail].device_name,
                        sizeof(port_data[port_avail].device_name),
                        "%s",
                        dev_nm);

        ok = ax25_init(port_avail);
        break;

      case DEVICE_NET_GPSD:
        if (debug_level & 2)
        {
          fprintf(stderr,"Opening a network GPS using gpsd\n");
        }

        clear_port_data(port_avail,0);

        port_data[port_avail].device_type = DEVICE_NET_GPSD;
        xastir_snprintf(port_data[port_avail].device_host_name,
                        sizeof(port_data[port_avail].device_host_name),
                        "%s",
                        dev_nm);
        port_data[port_avail].socket_port = dev_sck_p;
        port_data[port_avail].reconnect = reconnect;

        ok = net_init(port_avail);
        if (ok == 1)
        {

          // Pre-2.90 GPSD protocol
          xastir_snprintf(logon_txt, sizeof(logon_txt), "R\r\n");
          port_write_string(port_avail,logon_txt);

          // Post-2.90 GPSD protocol is handled near the
          // bottom of this routine.

          // Must wait for valid GPS parsing after sending
          // one posit.
          my_position_valid = 1;
          using_gps_position++;
          statusline(langcode("BBARSTA041"),1);
        }
        break;

      case DEVICE_NET_WX:
        if (debug_level & 2)
        {
          fprintf(stderr,"Opening a network WX\n");
        }

        clear_port_data(port_avail,0);

        port_data[port_avail].device_type = DEVICE_NET_WX;
        xastir_snprintf(port_data[port_avail].device_host_name,
                        sizeof(port_data[port_avail].device_host_name),
                        "%s",
                        dev_nm);
        port_data[port_avail].socket_port = dev_sck_p;
        port_data[port_avail].reconnect = reconnect;
        if (strcmp("1",passwd) == 0)
        {
          port_data[port_avail].data_type = 1;
        }

        ok = net_init(port_avail);
        if (ok == 1)
        {
          /* if connected now send call and program version */
          xastir_snprintf(logon_txt, sizeof(logon_txt), "%s %s%c%c", my_callsign, VERSIONTXT, '\r', '\n');
          port_write_string(port_avail,logon_txt);
        }
        break;

      case DEVICE_NET_DATABASE:
        if (debug_level & 2)
        {
          fprintf(stderr,"Opening a network database stream\n");
        }

        clear_port_data(port_avail,0);

        port_data[port_avail].device_type = DEVICE_NET_DATABASE;
        xastir_snprintf(port_data[port_avail].device_host_name,
                        sizeof(port_data[port_avail].device_host_name),
                        "%s",
                        dev_nm);
        port_data[port_avail].socket_port = dev_sck_p;
        port_data[port_avail].reconnect = reconnect;
        if (strcmp("1",passwd) == 0)
        {
          port_data[port_avail].data_type = 1;
        }

        ok = net_init(port_avail);
        if (ok == 1)
        {
          /* if connected now send call and program version */
          xastir_snprintf(logon_txt, sizeof(logon_txt), "%s %s%c%c", my_callsign, VERSIONTXT, '\r', '\n');
          port_write_string(port_avail,logon_txt);
        }
        break;

      case DEVICE_NET_AGWPE:
        if (debug_level & 2)
        {
          fprintf(stderr,"Opening a network AGWPE stream");
        }

        clear_port_data(port_avail,0);

        port_data[port_avail].device_type = DEVICE_NET_AGWPE;
        xastir_snprintf(port_data[port_avail].device_host_name,
                        sizeof(port_data[port_avail].device_host_name),
                        "%s",
                        dev_nm);
        port_data[port_avail].socket_port = dev_sck_p;
        port_data[port_avail].reconnect = reconnect;
        if (strcmp("1",passwd) == 0)
        {
          port_data[port_avail].data_type = 1;
        }

        ok = net_init(port_avail);

        if (ok == 1)
        {

          // If password isn't empty, send login
          // information
          //
          if (strlen(passwd) != 0)
          {

            // Send the login packet
            send_agwpe_packet(port_avail,
                              0,                       // AGWPE RadioPort
                              'P',                     // Login/Password Frame
                              NULL,                    // FromCall
                              NULL,                    // ToCall
                              NULL,                    // Path
                              (unsigned char *)passwd, // Data
                              strlen(passwd));         // Length
          }
        }
        break;

      default:
        break;
    }

    if (ok == 1)    // If port is connected...
    {

      if (debug_level & 2)
      {
        fprintf(stderr,"*** add_device: ok: %d ***\n",ok);
      }

      /* if all is ok check and start read write threads */
      (void)start_port_threads(port_avail);
      usleep(100000); // 100ms

      switch (dev_type)
      {

        case DEVICE_SERIAL_TNC:
        case DEVICE_SERIAL_TNC_HSP_GPS:
        case DEVICE_SERIAL_TNC_AUX_GPS:

          if (ok == 1)
          {
            xastir_snprintf(temp, sizeof(temp), "config/%s", devices[port_avail].tnc_up_file);
            (void)command_file_to_tnc_port(port_avail,get_data_base_dir(temp));
          }
          break;

        case DEVICE_SERIAL_KISS_TNC:

          // Initialize KISS-Mode at startup
          if (devices[port_avail].init_kiss)
          {
            xastir_snprintf(init_kiss_string,
                            sizeof(init_kiss_string),
                            "\x1B@k\r");    // [ESC@K sets tnc from terminal- into kissmode
            port_write_string(port_avail,init_kiss_string);
            usleep(100000); // wait a little bit...
          }

          // Send the KISS parameters to the TNC
          send_kiss_config(port_avail,0,0x01,atoi(devices[port_avail].txdelay));
          send_kiss_config(port_avail,0,0x02,atoi(devices[port_avail].persistence));
          send_kiss_config(port_avail,0,0x03,atoi(devices[port_avail].slottime));
          send_kiss_config(port_avail,0,0x04,atoi(devices[port_avail].txtail));
          send_kiss_config(port_avail,0,0x05,devices[port_avail].fullduplex);
          break;

        //WE7U
        case DEVICE_SERIAL_MKISS_TNC:
          // Send the KISS parameters to the TNC.  We'll
          // need to send them to the correct port for
          // this MKISS device.
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


          // Query for port information
          //
          send_agwpe_packet(port_avail,
                            0,      // AGWPE RadioPort
                            'G',    // Request Port Info Frame
                            NULL,   // FromCall
                            NULL,   // ToCall
                            NULL,   // Path
                            NULL,   // Data
                            0);     // Length

          // Ask to receive "raw" frames
          //
          send_agwpe_packet(port_avail,
                            0,      // AGWPE RadioPort
                            'k',    // Request Raw Packets Frame
                            NULL,   // FromCall
                            NULL,   // ToCall
                            NULL,   // Path
                            NULL,   // Data
                            0);     // Length


          /*
          // Send a dummy UI frame for testing purposes.
          //
          send_agwpe_packet(port_avail,
          atoi(devices[port_avail].device_host_filter_string) - 1, // AGWPE radio port
          '\0',       // type
          "TEST-3",   // FromCall
          "APRS",     // ToCall
          NULL,       // Path
          "Test",     // Data
          4);         // length


          // Send another dummy UI frame.
          //
          send_agwpe_packet(port_avail,
          atoi(devices[port_avail].device_host_filter_string) - 1, // AGWPE radio port
          '\0',       // type
          "TEST-3",   // FromCall
          "APRS",     // ToCall
          "RELAY,SAR1-1,SAR2-1,SAR3-1,SAR4-1,SAR5-1,SAR6-1,SAR7-1", // Path
          "Testing this darned thing!",   // Data
          26);     // length
          */

          break;

        case DEVICE_NET_GPSD:

          // Post-2.90 GPSD protocol
          // (Pre-2.90 protocol handled in a prior section of
          // this routine)
          xastir_snprintf(logon_txt, sizeof(logon_txt), "?WATCH={\"enable\":true,\"nmea\":true}\r\n");
          port_write_string(port_avail,logon_txt);
          break;

        default:
          break;
      }
    }

    if (ok == -1)
    {
      xastir_snprintf(temp, sizeof(temp), langcode("POPEM00015"), port_avail);
      popup_message(langcode("POPEM00004"),temp);
      port_avail = -1;
    }
    else
    {
      if (ok == 0)
      {
        xastir_snprintf(temp, sizeof(temp), langcode("POPEM00016"), port_avail);
        popup_message(langcode("POPEM00004"),temp);
        port_avail = -1;
      }
    }
  }
  else
  {
    popup_message(langcode("POPEM00004"),langcode("POPEM00017"));
  }

  return(port_avail);
}





//***********************************************************
// port status
// port is the port to get status on
//***********************************************************
void port_stats(int port)
{
  if (port >= 0)
  {
    fprintf(stderr,"Port %d %s Status\n\n",port,dtype[port_data[port].device_type].device_name);
    fprintf(stderr,"Errors %d\n",port_data[port].errors);
    fprintf(stderr,"Reconnects %d\n",port_data[port].reconnects);
    fprintf(stderr,"Bytes in: %ld  out: %ld\n",(long)port_data[port].bytes_input,(long)port_data[port].bytes_output);
    fprintf(stderr,"\n");
  }
}





//***********************************************************
// startup defined ports
//
// port = -2: Start all defined interfaces
// port = -1: Start all interfaces with "Activate on Startup"
// port = 0 - MAX: Start only the one port specified
//***********************************************************
void startup_all_or_defined_port(int port)
{
  int i, override;
  int start;

  override = 0;

  switch (port)
  {

    case -1:    // Start if "Activate on Startup" enabled
      start = 0;
      break;

    case -2:    // Start all interfaces, period!
      start = 0;
      override = 1;
      break;

    default:    // Start only the interface specified in "port"
      start = port;
      override = 1;
      break;
  }

  begin_critical_section(&devices_lock, "interface.c:startup_all_or_defined_port" );

  for (i = start; i < MAX_IFACE_DEVICES; i++)
  {

    // Only start ports that aren't already up
    if ( (port_data[i].active != DEVICE_IN_USE)
         || (port_data[i].status != DEVICE_UP) )
    {

      switch (devices[i].device_type)
      {

        case DEVICE_NET_STREAM:
          if (devices[i].connect_on_startup == 1 || override)
          {
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
          if (devices[i].connect_on_startup == 1 || override)
          {
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
          if (devices[i].connect_on_startup == 1 || override)
          {
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
          if (devices[i].connect_on_startup == 1 || override)
          {
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
          if (devices[i].connect_on_startup == 1 || override)
          {
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
          if (devices[i].connect_on_startup == 1 || override)
          {
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
          if (devices[i].connect_on_startup == 1 || override)
          {
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
        case DEVICE_SERIAL_MKISS_TNC:
        case DEVICE_SERIAL_TNC_HSP_GPS:
        case DEVICE_SERIAL_TNC_AUX_GPS:

          if (devices[i].connect_on_startup == 1 || override)
          {
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
          if (devices[i].connect_on_startup == 1 || override)
          {
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
#ifdef HAVE_DB
        case DEVICE_SQL_DATABASE:
          if (debug_level & 4096)
          {
            fprintf(stderr,"Device %d Connect_on_startup=%d\n",i,devices[i].connect_on_startup);
          }
          if (devices[i].connect_on_startup == 1 || override)
          {
            ioparam *d = &devices[i];
            if (debug_level & 4096)
            {
              fprintf(stderr,"Opening a sql db with device type %d\n",d->device_type);
            }
            (void)add_device_by_ioparam(i, &devices[i]);
            if (debug_level & 4096)
            {
              fprintf(stderr, "added device by ioparam [%d] type=[%d]\n",i,connections[i].type);
            }
          }
          break;
#endif /* HAVE_DB */

        default:
          break;
      }   // End of switch
    }
    else if (debug_level & 2)
    {
      fprintf(stderr,"Skipping port %d, it's already running\n",i);
    }

    if (port != -1 && port != -2)
    {
      // We're doing a specific port #, so stop the loop
      i = MAX_IFACE_DEVICES+1;
    }
  }

  end_critical_section(&devices_lock, "interface.c:startup_all_or_defined_port" );

}





//***********************************************************
// shutdown active ports
//
// port = -1:  Shut down all active ports
// port = 0 to max: Shut down the specified port if active
//***********************************************************
void shutdown_all_active_or_defined_port(int port)
{
  int i;
  int start;

  if (debug_level & 2)
  {
    fprintf(stderr,"\nshutdown_all_active_or_defined_port: %d\n\n",port);
  }

  if (port == -1)
  {
    start = 0;
  }
  else
  {
    start = port;
  }

  for( i = start; i < MAX_IFACE_DEVICES; i++ )
  {
    if ( (port_data[i].active == DEVICE_IN_USE)
         && ( (port_data[i].status == DEVICE_UP)
              || (port_data[i].status == DEVICE_ERROR) ) )
    {
      if (debug_level & 2)
      {
        fprintf(stderr,"Shutting down port %d \n",i);
      }

      (void)del_device(i);
    }
    if (port != -1) // Stop after one iteration if port specified
    {
      i = MAX_IFACE_DEVICES+1;
    }
  }
}





//*************************************************************
// check ports
//
// Called periodically by main.c:UpdateTime() function.
// Attempts to reconnect interfaces that are down.
//*************************************************************
void check_ports(void)
{
  int i;
  int temp;

  for (i = 0; i < MAX_IFACE_DEVICES; i++)
  {

    switch (port_data[i].device_type)
    {
      case(DEVICE_NET_STREAM):
      //case(DEVICE_AX25_TNC):
      case(DEVICE_NET_GPSD):
      case(DEVICE_NET_WX):
        if (port_data[i].port_activity == 0)
        {
          // We've seen no activity for one time period.  This variable
          // is updated in interface_gui.c

          if (port_data[i].status == DEVICE_ERROR)
          {
            // We're already in the error state, so force a reconnect
            port_data[i].reconnects = -1;
          }
          else if (port_data[i].status == DEVICE_UP)
          {
            // No activity on a port that's being used.
            // Cause a reconnect at the next iteration
            if (debug_level & 2)
            {
              fprintf(stderr,"check_ports(): Inactivity on port %d, DEVICE_ERROR ***\n",i);
            }
            port_data[i].status = DEVICE_ERROR; // No activity, so force a shutdown

            // Show the latest status in the interface control dialog
            update_interface_list();


            // If the below statement is enabled, it causes an immediate reconnect
            // after one time-period of inactivity, currently 7.5 minutes, as set in
            // main.c:UpdateTime().  This means the symbol will never change from green
            // to red on the status bar, so the operator might not know about a
            // connection that is being constantly reconnected.  By leaving it commented
            // out we get one time period of red, and then it will reconnect at the 2nd
            // time period.  This means we can reconnect within 15 minutes if a line
            // goes dead.
            //
            port_data[i].reconnects = -1;     // Causes an immediate reconnect
          }

        }
        else    // We saw activity on this port.
        {
          port_data[i].port_activity = 0;     // Reset counter for next time
        }
        break;
    }

    if (port_data[i].active == DEVICE_IN_USE && port_data[i].status == DEVICE_ERROR)
    {
      if (debug_level & 2)
      {
        fprintf(stderr,"Found device error on port %d\n",i);
      }

      if (port_data[i].reconnect == 1)
      {
        port_data[i].reconnects++;
        temp = port_data[i].reconnects;
        if (temp < 1)
        {
          if (debug_level & 2)
          {
            fprintf(stderr,"Device asks for reconnect count now at %d\n",temp);
          }

          if (debug_level & 2)
          {
            fprintf(stderr,"Shutdown device %d\n",i);
          }

          shutdown_all_active_or_defined_port(i);

          if (debug_level & 2)
          {
            fprintf(stderr,"Starting device %d\n",i);
          }

          startup_all_or_defined_port(i);

          /* if error on startup */
          if (port_data[i].status == DEVICE_ERROR)
          {
            port_data[i].reconnects = temp;
          }
        }
        else
        {
          if (debug_level & 2)
          {
            fprintf(stderr,"Device has either too many errors, or no activity at all!\n");
          }

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
// unproto path, we use the unproto path: "WIDE2-2".
//
// Input:  Port number
// Ouput:  String pointer containing unproto path
//
// WE7U:  Should we check to make sure that there are printable
// characters in the path?
//
unsigned char *select_unproto_path(int port)
{
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


  while (!done && (count < 3))
  {
    temp = (devices[port].unprotonum + count) % 3;
    switch (temp)
    {

      case 0:
        if (strlen(devices[port].unproto1) > 0)
        {
          xastir_snprintf(unproto_path_txt,
                          sizeof(unproto_path_txt),
                          "%s",
                          devices[port].unproto1);
          done++;
        }
        else
        {
          // No path entered here.  Skip this path in the
          // rotation for next time.
          bump_up++;
        }
        break;

      case 1:
        if (strlen(devices[port].unproto2) > 0)
        {
          xastir_snprintf(unproto_path_txt,
                          sizeof(unproto_path_txt),
                          "%s",
                          devices[port].unproto2);
          done++;
        }
        else
        {
          // No path entered here.  Skip this path in
          // the rotation for next time.
          bump_up++;
        }
        break;

      case 2:
        if (strlen(devices[port].unproto3) > 0)
        {
          xastir_snprintf(unproto_path_txt,
                          sizeof(unproto_path_txt),
                          "%s",
                          devices[port].unproto3);
          done++;
        }
        else
        {
          // No path entered here.  Skip this path in
          // the rotation for next time.
          bump_up++;
        }
        break;
    }   // End of switch
    count++;
  }   // End of while loop

  if (done)
  {
    // We found an unproto path.  Check it for accepted values.
    // Output a warning message if it is beyond normal ranges,
    // but still allow it to be used.
    //
    if(check_unproto_path(unproto_path_txt))
    {
      popup_message_always(langcode("WPUPCFT045"),
                           langcode("WPUPCFT043"));
    }
  }
  else
  {
    // We found no entries in the unproto fields for the
    // interface.  Set a default path of "WIDE2-2".

    xastir_snprintf(unproto_path_txt,
                    sizeof(unproto_path_txt),
                    "WIDE2-2");
  }

  // Increment the path number for the next round of
  // transmissions.  This will round-robin the paths so that all
  // entered paths get used.
  devices[port].unprotonum = (devices[port].unprotonum + 1 + bump_up) % 3;

  // Make sure the path is in upper-case
  (void)to_upper(unproto_path_txt);

  return((unsigned char *)unproto_path_txt);
}





//***********************************************************
// output_my_aprs_data
// This is the function responsible for sending out my own
// posits.  The next function below this one handles objects,
// messages and the like (output_my_data).
//***********************************************************
void output_my_aprs_data(void)
{
  char header_txt[MAX_LINE_SIZE+5];
  char header_txt_save[MAX_LINE_SIZE+5];
  char data_txt[MAX_LINE_SIZE+5];
  char data_txt_save[MAX_LINE_SIZE+5];
  char temp[MAX_LINE_SIZE+5];
  char path_txt[MAX_LINE_SIZE+5];
  char *unproto_path = "";
  char data_txt2[5];
  struct tm *day_time;
  time_t sec;
  char my_pos[256];
  char my_output_lat[MAX_LAT];
  char my_output_long[MAX_LONG];
  char output_net[256];
  char wx_data[200];
  char output_phg[10];
  char output_cs[10];
  char output_alt[20];
  char output_brk[3];
  int ok;
  int port;
  char my_comment_tx[MAX_COMMENT+1];
  int interfaces_ok_for_transmit = 0;
  char logfile_tmp_path[MAX_VALUE];

  // Check whether transmits are disabled globally
  if (transmit_disable)
  {

    if (emergency_beacon)
    {

      // Notify the operator because emergency_beacon mode is on but
      // nobody will know it 'cuz global transmit is disabled.
      //
      // "Warning"
      // "Global transmit is DISABLED.  Emergency beacons are NOT going out!"
      popup_message_always( langcode("POPEM00035"),
                            langcode("POPEM00047") );
    }
    return;
  }

  header_txt_save[0] = '\0';
  data_txt_save[0] = '\0';
  sec = sec_now();


  // Check whether we're in emergency beacon mode.  If so, add
  // "EMERGENCY" at the beginning of the comment field we'll
  // transmit.
  if (emergency_beacon)
  {
    strncpy(my_comment_tx, "EMERGENCY", sizeof(my_comment_tx));
    my_comment_tx[sizeof(my_comment_tx)-1] = '\0';  // Terminate string
  }
  else
  {
    xastir_snprintf(my_comment_tx,
                    sizeof(my_comment_tx),
                    "%s",
                    my_comment);
  }


  // Format latitude string for transmit later
  if (transmit_compressed_posit)      // High res version
  {
    xastir_snprintf(my_output_lat,
                    sizeof(my_output_lat),
                    "%s",
                    my_lat);
  }
  else    // Create a low-res version of the latitude string
  {
    long my_temp_lat;
    char temp_data[20];

    // Convert to long
    my_temp_lat = convert_lat_s2l(my_lat);

    // Convert to low-res string
    convert_lat_l2s(my_temp_lat,
                    temp_data,
                    sizeof(temp_data),
                    CONVERT_LP_NORMAL);

    xastir_snprintf(my_output_lat,
                    sizeof(my_output_lat),
                    "%c%c%c%c.%c%c%c",
                    temp_data[0],
                    temp_data[1],
                    temp_data[3],
                    temp_data[4],
                    temp_data[6],
                    temp_data[7],
                    temp_data[8]);
  }

  (void)output_lat(my_output_lat,transmit_compressed_posit);
  if (debug_level & 128)
  {
    fprintf(stderr,"OUT LAT <%s>\n",my_output_lat);
  }

  // Format longitude string for transmit later
  if (transmit_compressed_posit)      // High res version
  {
    xastir_snprintf(my_output_long,
                    sizeof(my_output_long),
                    "%s",
                    my_long);
  }
  else    // Create a low-res version of the longitude string
  {
    long my_temp_long;
    char temp_data[20];

    // Convert to long
    my_temp_long = convert_lon_s2l(my_long);

    // Convert to low-res string
    convert_lon_l2s(my_temp_long,
                    temp_data,
                    sizeof(temp_data),
                    CONVERT_LP_NORMAL);

    xastir_snprintf(my_output_long,
                    sizeof(my_output_long),
                    "%c%c%c%c%c.%c%c%c",
                    temp_data[0],
                    temp_data[1],
                    temp_data[2],
                    temp_data[4],
                    temp_data[5],
                    temp_data[7],
                    temp_data[8],
                    temp_data[9]);
  }

  (void)output_long(my_output_long,transmit_compressed_posit);
  if (debug_level & 128)
  {
    fprintf(stderr,"OUT LONG <%s>\n",my_output_long);
  }

  output_net[0]='\0'; // Make sure this array at least starts initialized.

  begin_critical_section(&devices_lock, "interface.c:output_my_aprs_data" );

  // Iterate across the ports, set up each device's headers/paths/handshakes,
  // then transmit the posit if the port is open and tx is enabled.
  for (port = 0; port < MAX_IFACE_DEVICES; port++)
  {

    // First send any header/path info we might need out the port,
    // set up TNC's to the proper mode, etc.
    ok = 1;
    switch (port_data[port].device_type)
    {

      //            case DEVICE_NET_DATABASE:

      case DEVICE_NET_AGWPE:

        output_net[0]='\0'; // We don't need this header for AGWPE
        break;

      case DEVICE_NET_STREAM:

        xastir_snprintf(output_net,
                        sizeof(output_net),
                        "%s>%s,TCPIP*:",
                        my_callsign,
                        VERSIONFRM);
        break;

      case DEVICE_SERIAL_TNC_HSP_GPS:
        /* make dtr normal (talk to TNC) */
        if (port_data[port].status == DEVICE_UP)
        {
          port_dtr(port,0);
        }
      /* Falls through. */

      case DEVICE_SERIAL_TNC_AUX_GPS:
      case DEVICE_SERIAL_KISS_TNC:
      case DEVICE_SERIAL_MKISS_TNC:
      case DEVICE_SERIAL_TNC:
      case DEVICE_AX25_TNC:

        /* clear this for a TNC */
        output_net[0] = '\0';

        /* Set my call sign */
        xastir_snprintf(header_txt,
                        sizeof(header_txt),
                        "%c%s %s\r",
                        '\3',
                        "MYCALL",
                        my_callsign);

        // Send the callsign out to the TNC only if the interface is up and tx is enabled???
        // We don't set it this way for KISS TNC interfaces.
        if ( (port_data[port].device_type != DEVICE_SERIAL_KISS_TNC)
             && (port_data[port].device_type != DEVICE_SERIAL_MKISS_TNC)
             && (port_data[port].status == DEVICE_UP)
             && (devices[port].transmit_data == 1)
             && !transmit_disable
             && !posit_tx_disable)
        {
          port_write_string(port,header_txt);
        }

        // Set unproto path:  Get next unproto path in
        // sequence.
        unproto_path = (char *)select_unproto_path(port);

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
             && (port_data[port].device_type != DEVICE_SERIAL_MKISS_TNC)
             && (port_data[port].status == DEVICE_UP)
             && (devices[port].transmit_data == 1)
             && !transmit_disable
             && !posit_tx_disable)
        {
          port_write_string(port,header_txt);
        }


        // Set converse mode.  We don't need to do this for
        // KISS TNC interfaces.  One european TNC (tnc2-ui)
        // doesn't accept "conv" but does accept the 'k'
        // command.  A Kantronics KPC-2 v2.71 TNC accepts
        // the "conv" command but not the 'k' command.
        // Figures!  The  choice of whether to send "k" or "conv"
        // is made by the user in the Serial TNC interface properties
        // dialog.  Older versions of Xastir had this hardcoded here.
        //
        xastir_snprintf(header_txt, sizeof(header_txt), "%c%s\r", '\3', devices[port].device_converse_string);

        if ( (port_data[port].device_type != DEVICE_SERIAL_KISS_TNC)
             && (port_data[port].device_type != DEVICE_SERIAL_MKISS_TNC)
             && (port_data[port].status == DEVICE_UP)
             && (devices[port].transmit_data == 1)
             && !transmit_disable
             && !posit_tx_disable)
        {
          port_write_string(port,header_txt);
        }
        // Delay a bit if the user clicked on the "Add Delay"
        // togglebutton in the port's interface properties dialog.
        // This is primarily needed for KAM TNCs, which will fail to
        // go into converse mode if there is no delay here.
        if (devices[port].tnc_extra_delay != 0)
        {
          usleep(devices[port].tnc_extra_delay);
        }
        break;

      default: /* port has unknown device_type */
        ok = 0;
        break;

    } // End of switch


    // Set up some more strings for later transmission

    /* send station info */
    output_cs[0] = '\0';
    output_phg[0] = '\0';
    output_alt[0] = '\0';
    output_brk[0] = '\0';


    if (transmit_compressed_posit)
      xastir_snprintf(my_pos,
                      sizeof(my_pos),
                      "%s",
                      compress_posit(my_output_lat,
                                     my_group,
                                     my_output_long,
                                     my_symbol,
                                     my_last_course,
                                     my_last_speed,  // In knots
                                     my_phg));
    else   /* standard non compressed mode */
    {
      xastir_snprintf(my_pos,
                      sizeof(my_pos),
                      "%s%c%s%c",
                      my_output_lat,
                      my_group,
                      my_output_long,
                      my_symbol);
      /* get PHG, if used for output */
      if (strlen(my_phg) >= 6)
        xastir_snprintf(output_phg,
                        sizeof(output_phg),
                        "%s",
                        my_phg);

      /* get CSE/SPD, Always needed for output even if 0 */
      xastir_snprintf(output_cs,
                      sizeof(output_cs),
                      "%03d/%03d/",
                      my_last_course,
                      my_last_speed);    // Speed in knots

      /* get altitude */
      if (my_last_altitude_time > 0)
        xastir_snprintf(output_alt,
                        sizeof(output_alt),
                        "A=%06ld/",
                        my_last_altitude);
    }


    // And set up still more strings for later transmission
    switch (output_station_type)
    {
      case(1):
        /* APRS_MOBILE LOCAL TIME */

        if((strlen(output_cs) < 8) && (my_last_altitude_time > 0) &&
            (strlen(output_alt) > 0))
        {
          xastir_snprintf(output_brk,
                          sizeof(output_brk),
                          "/");
        }

        day_time = localtime(&sec);

        xastir_snprintf(data_txt_save,
                        sizeof(data_txt_save),
                        "@%02d%02d%02d/%s%s%s%s%s",
                        day_time->tm_mday,
                        day_time->tm_hour,
                        day_time->tm_min,
                        my_pos,
                        output_cs,
                        output_brk,
                        output_alt,
                        my_comment_tx);

        //WE7U2:
        // Truncate at max length for this type of APRS
        // packet.
        if (transmit_compressed_posit)
        {
          if (strlen(data_txt_save) > 61)
          {
            data_txt_save[61] = '\0';
          }
        }
        else   // Uncompressed lat/long
        {
          if (strlen(data_txt_save) > 70)
          {
            data_txt_save[70] = '\0';
          }
        }

        // Add '\r' onto end.
        strncat(data_txt_save, "\r", sizeof(data_txt_save)-strlen(data_txt_save)-1);

        strcpy(data_txt, output_net);
        data_txt[sizeof(data_txt)-1] = '\0';  // Terminate string

        strcat(data_txt, data_txt_save);
        data_txt[sizeof(data_txt)-1] = '\0';  // Terminate string

        break;

      case(2):
        /* APRS_MOBILE ZULU DATE-TIME */

        if((strlen(output_cs) < 8) && (my_last_altitude_time > 0) &&
            (strlen(output_alt) > 0))
        {
          xastir_snprintf(output_brk,
                          sizeof(output_brk),
                          "/");
        }

        day_time = gmtime(&sec);

        xastir_snprintf(data_txt_save,
                        sizeof(data_txt_save),
                        "@%02d%02d%02dz%s%s%s%s%s",
                        day_time->tm_mday,
                        day_time->tm_hour,
                        day_time->tm_min,
                        my_pos,
                        output_cs,
                        output_brk,
                        output_alt,
                        my_comment_tx);

        //WE7U2:
        // Truncate at max length for this type of APRS
        // packet.
        if (transmit_compressed_posit)
        {
          if (strlen(data_txt_save) > 61)
          {
            data_txt_save[61] = '\0';
          }
        }
        else   // Uncompressed lat/long
        {
          if (strlen(data_txt_save) > 70)
          {
            data_txt_save[70] = '\0';
          }
        }

        // Add '\r' onto end.
        strncat(data_txt_save, "\r", sizeof(data_txt_save)-strlen(data_txt_save)-1);

        strcpy(data_txt, output_net);
        data_txt[sizeof(data_txt)-1] = '\0';  // Terminate string

        strcat(data_txt, data_txt_save);
        data_txt[sizeof(data_txt)-1] = '\0';  // Terminate string

        break;

      case(3):
        /* APRS_MOBILE ZULU TIME w/SEC */

        if((strlen(output_cs) < 8) && (my_last_altitude_time > 0) &&
            (strlen(output_alt) > 0))
        {
          xastir_snprintf(output_brk,
                          sizeof(output_brk),
                          "/");
        }

        day_time = gmtime(&sec);

        xastir_snprintf(data_txt_save,
                        sizeof(data_txt_save),
                        "@%02d%02d%02dh%s%s%s%s%s",
                        day_time->tm_hour,
                        day_time->tm_min,
                        day_time->tm_sec,
                        my_pos,
                        output_cs,
                        output_brk,
                        output_alt,
                        my_comment_tx);

        //WE7U2:
        // Truncate at max length for this type of APRS
        // packet.
        if (transmit_compressed_posit)
        {
          if (strlen(data_txt_save) > 61)
          {
            data_txt_save[61] = '\0';
          }
        }
        else   // Uncompressed lat/long
        {
          if (strlen(data_txt_save) > 70)
          {
            data_txt_save[70] = '\0';
          }
        }

        // Add '\r' onto end.
        strncat(data_txt_save, "\r", sizeof(data_txt_save)-strlen(data_txt_save)-1);

        strcpy(data_txt, output_net);
        data_txt[sizeof(data_txt)-1] = '\0';  // Terminate string

        strcat(data_txt, data_txt_save);
        data_txt[sizeof(data_txt)-1] = '\0';  // Terminate string

        break;

      case(4):
        /* APRS position with WX data, no timestamp */
        sec = wx_tx_data1(wx_data, sizeof(wx_data));
        if (sec != 0)
        {

          xastir_snprintf(data_txt_save,
                          sizeof(data_txt_save),
                          "%c%s%s",
                          aprs_station_message_type,
                          my_pos,
                          wx_data);

          // WE7U2:
          // There's no limit on the max size for this kind of packet except
          // for the AX.25 limit of 256 bytes!
          //
          // Truncate at max length for this type of APRS
          // packet.  Left the compressed/uncompressed
          // "if" statement here in case we need to change
          // this in the future due to spec changes.
          // Consistent with the rest of the code in this
          // function which does similar things.
          //
          if (transmit_compressed_posit)
          {
            if (strlen(data_txt_save) > 256)
            {
              data_txt_save[256] = '\0';
            }
          }
          else   // Uncompressed lat/long
          {
            if (strlen(data_txt_save) > 256)
            {
              data_txt_save[256] = '\0';
            }
          }

          // Add '\r' onto end.
          strncat(data_txt_save, "\r", sizeof(data_txt_save)-strlen(data_txt_save)-1);

          strcpy(data_txt, output_net);
          data_txt[sizeof(data_txt)-1] = '\0';  // Terminate string

          strcat(data_txt, data_txt_save);
          data_txt[sizeof(data_txt)-1] = '\0';  // Terminate string
        }
        else
        {
          /* default to APRS FIXED if no wx data. No timestamp */

          if ((strlen(output_phg) < 6) && (my_last_altitude_time > 0) &&
              (strlen(output_alt) > 0))
          {
            xastir_snprintf(output_brk,
                            sizeof(output_brk),
                            "/");
          }

          xastir_snprintf(data_txt_save,
                          sizeof(data_txt_save),
                          "%c%s%s%s%s%s",
                          aprs_station_message_type,
                          my_pos,
                          output_phg,
                          output_brk,
                          output_alt,
                          my_comment_tx);

          // WE7U2:
          // Truncate at max length for this type of APRS
          // packet.
          if (transmit_compressed_posit)
          {
            if (strlen(data_txt_save) > 54)
            {
              data_txt_save[54] = '\0';
            }
          }
          else   // Uncompressed lat/long
          {
            if (strlen(data_txt_save) > 63)
            {
              data_txt_save[63] = '\0';
            }
          }

          // Add '\r' onto end.
          strncat(data_txt_save, "\r", sizeof(data_txt_save)-strlen(data_txt_save)-1);

          strcpy(data_txt, output_net);
          data_txt[sizeof(data_txt)-1] = '\0';  // Terminate string

          strcat(data_txt, data_txt_save);
          data_txt[sizeof(data_txt)-1] = '\0';  // Terminate string
        }

        break;

      case(5):
        /* APRS position with ZULU DATE-TIME and WX data */
        sec = wx_tx_data1(wx_data,sizeof(wx_data));
        if (sec != 0)
        {
          day_time = gmtime(&sec);

          xastir_snprintf(data_txt_save,
                          sizeof(data_txt_save),
                          "@%02d%02d%02dz%s%s",
                          day_time->tm_mday,
                          day_time->tm_hour,
                          day_time->tm_min,
                          my_pos,
                          wx_data);

          // WE7U2:
          // Truncate at max length for this type of APRS
          // packet.
          if (transmit_compressed_posit)
          {
            if (strlen(data_txt_save) > 61)
            {
              data_txt_save[61] = '\0';
            }
          }
          else   // Uncompressed lat/long
          {
            if (strlen(data_txt_save) > 70)
            {
              data_txt_save[70] = '\0';
            }
          }

          // Add '\r' onto end.
          strncat(data_txt_save, "\r", sizeof(data_txt_save)-strlen(data_txt_save)-1);

          strcpy(data_txt, output_net);
          data_txt[sizeof(data_txt)-1] = '\0';  // Terminate string

          strcat(data_txt, data_txt_save);
          data_txt[sizeof(data_txt)-1] = '\0';  // Terminate string
        }
        else
        {
          /* default to APRS FIXED if no wx data */

          if((strlen(output_phg) < 6) && (my_last_altitude_time > 0) &&
              (strlen(output_alt) > 0))
          {
            xastir_snprintf(output_brk,
                            sizeof(output_brk),
                            "/");
          }

          xastir_snprintf(data_txt_save,
                          sizeof(data_txt_save),
                          "%c%s%s%s%s%s",
                          aprs_station_message_type,
                          my_pos,
                          output_phg,
                          output_brk,
                          output_alt,
                          my_comment_tx);

          // WE7U2:
          // Truncate at max length for this type of APRS
          // packet.
          if (transmit_compressed_posit)
          {
            if (strlen(data_txt_save) > 54)
            {
              data_txt_save[54] = '\0';
            }
          }
          else   // Uncompressed lat/long
          {
            if (strlen(data_txt_save) > 63)
            {
              data_txt_save[63] = '\0';
            }
          }

          // Add '\r' onto end.
          strncat(data_txt_save, "\r", sizeof(data_txt_save)-strlen(data_txt_save)-1);

          strcpy(data_txt, output_net);
          data_txt[sizeof(data_txt)-1] = '\0';  // Terminate string

          strcat(data_txt, data_txt_save);
          data_txt[sizeof(data_txt)-1] = '\0';  // Terminate string
        }
        break;

      /* default to APRS FIXED if no wx data */
      case(0):

      default:
        /* APRS_FIXED */

        if ((strlen(output_phg) < 6) && (my_last_altitude_time > 0) &&
            (strlen(output_alt) > 0))
        {
          xastir_snprintf(output_brk,
                          sizeof(output_brk),
                          "/");
        }

        xastir_snprintf(data_txt_save,
                        sizeof(data_txt_save),
                        "%c%s%s%s%s%s",
                        aprs_station_message_type,
                        my_pos,
                        output_phg,
                        output_brk,
                        output_alt,
                        my_comment_tx);

        // WE7U2:
        // Truncate at max length for this type of APRS
        // packet.
        if (transmit_compressed_posit)
        {
          if (strlen(data_txt_save) > 54)
          {
            data_txt_save[54] = '\0';
          }
        }
        else   // Uncompressed lat/long
        {
          if (strlen(data_txt_save) > 63)
          {
            data_txt_save[63] = '\0';
          }
        }

        // Add '\r' onto end.
        strncat(data_txt_save, "\r", sizeof(data_txt_save)-strlen(data_txt_save)-1);

        strcpy(data_txt, output_net);
        data_txt[sizeof(data_txt)-1] = '\0';  // Terminate string

        strcat(data_txt, data_txt_save);
        data_txt[sizeof(data_txt)-1] = '\0';  // Terminate string

        break;
    }

    if (ok)
    {
      // Here's where the actual transmit of the posit occurs.  The
      // transmit string has been set up in "data_txt" by this point.

      // If transmit or posits have been turned off, don't transmit posit
      if ( (port_data[port].status == DEVICE_UP)
           && (devices[port].transmit_data == 1)
           && !transmit_disable
           && !posit_tx_disable)
      {

        interfaces_ok_for_transmit++;

        // WE7U:  Change so that path is passed as well for KISS TNC
        // interfaces:  header_txt_save would probably be the one to pass,
        // or create a new string just for KISS TNC's.

        if ( (port_data[port].device_type == DEVICE_SERIAL_KISS_TNC)
             || (port_data[port].device_type == DEVICE_SERIAL_MKISS_TNC) )
        {

          // Note:  This one has callsign & destination in the string

          // Transmit the posit out the KISS interface
          send_ax25_frame(port,
                          my_callsign,    // source
                          VERSIONFRM,     // destination
                          path_txt,       // path
                          data_txt);      // data
        }

        //WE7U:AGWPE
        else if (port_data[port].device_type == DEVICE_NET_AGWPE)
        {

          // Set unproto path:  Get next unproto path in
          // sequence.
          unproto_path = (char *)select_unproto_path(port);

          // We need to remove the complete AX.25 header from data_txt before
          // we call this routine!  Instead put the digipeaters into the
          // ViaCall fields.  We do this above by setting output_net to '\0'
          // before creating the data_txt string.
          send_agwpe_packet(port,            // Xastir interface port
                            atoi(devices[port].device_host_filter_string) - 1, // AGWPE RadioPort
                            '\0',                          // Type of frame
                            (unsigned char *)my_callsign,  // source
                            (unsigned char *)VERSIONFRM,   // destination
                            (unsigned char *)unproto_path, // Path,
                            (unsigned char *)data_txt,     // Data
                            strlen(data_txt) - 1);         // Skip \r
        }

        else
        {
          // Not a Serial KISS TNC interface
          port_write_string(port, data_txt);  // Transmit the posit
        }

        if (debug_level & 2)
        {
          fprintf(stderr,"TX:%d<%s>\n",port,data_txt);
        }

        /* add new line on network data */
        if (port_data[port].device_type == DEVICE_NET_STREAM)
        {
          xastir_snprintf(data_txt2, sizeof(data_txt2), "\n");                 // Transmit a newline
          port_write_string(port, data_txt2);
        }


        // Put our transmitted packet into the Incoming Data
        // window as well.  This way we can see both sides of a
        // conversation.  data_port == -1 for x_spider port,
        // normal interface number otherwise.  -99 to get a "**"
        // display meaning all ports.
        //
        // For packets that we're igating we end up with a CR or
        // LF on the end of them.  Remove that so the display
        // looks nice.
        strcpy(temp, my_callsign);
        temp[sizeof(temp)-1] = '\0';  // Terminate string
        strcat(temp, ">");
        temp[sizeof(temp)-1] = '\0';  // Terminate string
        strcat(temp, VERSIONFRM);
        temp[sizeof(temp)-1] = '\0';  // Terminate string
        strcat(temp, ",");
        temp[sizeof(temp)-1] = '\0';  // Terminate string
        strcat(temp, unproto_path);
        temp[sizeof(temp)-1] = '\0';  // Terminate string
        strcat(temp, ":");
        temp[sizeof(temp)-1] = '\0';  // Terminate string
        strcat(temp, data_txt);
        temp[sizeof(temp)-1] = '\0';  // Terminate string

        makePrintable(temp);
        packet_data_add("TX ", temp, port);
      }
      else
      {
      }
    } // End of posit transmit: "if (ok)"
  } // End of big loop

  end_critical_section(&devices_lock, "interface.c:output_my_aprs_data" );


  // Check the interfaces_ok_for_transmit variable if we're in
  // emergency_beacon mode.  If we didn't transmit out any interfaces, alert
  // the operator so that they can either enable interfaces or get emergency
  // help in some other manner.
  //
  if (emergency_beacon)
  {

    if (interfaces_ok_for_transmit)
    {

      // Beacons are going out in emergency beacon mode.  Alert the
      // operator so that he/she knows they've enabled that mode.
      //
      // "Emergency Beacon Mode"
      // "EMERGENCY BEACON MODE, transmitting every 60 seconds!"
      popup_message_always( langcode("POPEM00048"),
                            langcode("POPEM00049") );
    }

    else    // Emergency beacons are not going out for some reason
    {

      // Notify the operator because emergency_beacon mode is on but
      // nobody will know it 'cuz there are no interfaces enabled for
      // transmit.
      //
      // "Warning"
      // "Interfaces or posits/transmits DISABLED.  Emergency beacons are NOT going out!"
      popup_message_always( langcode("POPEM00035"),
                            langcode("POPEM00050") );
    }
  }


  // This will log a posit in the general format for a network interface
  // whether or not any network interfaces are currently up.
  if (log_net_data)
  {

    strcpy(data_txt, my_callsign);
    data_txt[sizeof(data_txt)-1] = '\0';  // Terminate string
    strcat(data_txt, ">");
    data_txt[sizeof(data_txt)-1] = '\0';  // Terminate string
    strcat(data_txt, VERSIONFRM);
    data_txt[sizeof(data_txt)-1] = '\0';  // Terminate string
    strcat(data_txt, ",TCPIP*:");
    data_txt[sizeof(data_txt)-1] = '\0';  // Terminate string
    strcat(data_txt, data_txt_save);
    data_txt[sizeof(data_txt)-1] = '\0';  // Terminate string

    log_data( get_user_base_dir(LOGFILE_NET, logfile_tmp_path,
                                sizeof(logfile_tmp_path)),
              (char *)data_txt );
  }


  if (enable_server_port && !transmit_disable && !posit_tx_disable)
  {
    // Send data to the x_spider server

    strcpy(data_txt, my_callsign);
    data_txt[sizeof(data_txt)-1] = '\0';  // Terminate string
    strcat(data_txt, ">");
    data_txt[sizeof(data_txt)-1] = '\0';  // Terminate string
    strcat(data_txt, VERSIONFRM);
    data_txt[sizeof(data_txt)-1] = '\0';  // Terminate string
    strcat(data_txt, ",TCPIP*:");
    data_txt[sizeof(data_txt)-1] = '\0';  // Terminate string
    strcat(data_txt, data_txt_save);
    data_txt[sizeof(data_txt)-1] = '\0';  // Terminate string

    if (writen(pipe_xastir_to_tcp_server,
               data_txt,
               strlen(data_txt)) != (int)strlen(data_txt))
    {
      fprintf(stderr,
              "my_aprs_data: Writen error: %d\n",
              errno);
    }
    // Terminate it with a linefeed
    if (writen(pipe_xastir_to_tcp_server, "\n", 1) != 1)
    {
      fprintf(stderr,
              "my_aprs_data: Writen error: %d\n",
              errno);
    }
  }
  // End of x_spider server send code


  // Note that this will only log one TNC line per transmission now matter
  // how many TNC's are defined.  It's a representative sample of what we're
  // sending out.  At least one TNC interface must be enabled in order to
  // have anything output to the log file here.
  if (log_tnc_data)
  {
    if (header_txt_save[0] != '\0')
    {

      strcpy(data_txt, header_txt_save);
      data_txt[sizeof(data_txt)-1] = '\0';  // Terminate string
      strcat(data_txt, data_txt_save);
      data_txt[sizeof(data_txt)-1] = '\0';  // Terminate string

      log_data( get_user_base_dir(LOGFILE_TNC, logfile_tmp_path,
                                  sizeof(logfile_tmp_path)),
                (char *)data_txt );
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
// path: Set to non-NULL if special path selected for messaging
//
// This function sends out messages/objects/bulletins/etc.
// This one currently tries to do local logging even if
// transmit is disabled.
//*****************************************************************************
void output_my_data(char *message, int incoming_port, int type, int loopback_only, int use_igate_path, char *path)
{
  char data_txt[MAX_LINE_SIZE+5];
  char data_txt_save[MAX_LINE_SIZE+5];
  char temp[MAX_LINE_SIZE+5];
  char path_txt[MAX_LINE_SIZE+5];
  char *unproto_path = "";
  char output_net[256];
  int ok, start, finish, port;
  int done;
  char logfile_tmp_path[MAX_VALUE];

  // Check whether transmits are disabled globally
  if (transmit_disable && !loopback_only)
  {
    return;
  }

  //// cbell- if path is null, strlen/printf segv in solaris
  if (path == NULL)
  {
    path = "";
  }

  if (debug_level & 1)
  {
    fprintf(stderr,
            "Sending out port: %d, type: %d, path: %s\n",
            incoming_port,
            type,
            path);
  }

  if (message == NULL)
  {
    return;
  }

  if (message[0] == '\0')
  {
    return;
  }

  data_txt_save[0] = '\0';

  if (incoming_port == -1)     // Send out all of the interfaces
  {
    start = 0;
    finish = MAX_IFACE_DEVICES;
  }
  else    // Only send out the chosen interface
  {
    start  = incoming_port;
    finish = incoming_port + 1;
  }


  begin_critical_section(&devices_lock, "interface.c:output_my_data" );

  for (port = start; port < finish; port++)
  {

    ok = 1;
    if (type == 0)                          // my data
    {
      switch (port_data[port].device_type)
      {

        //                case DEVICE_NET_DATABASE:

        case DEVICE_NET_AGWPE:
          output_net[0] = '\0';   // Clear header
          break;

        case DEVICE_NET_STREAM:
          if (debug_level & 1)
          {
            fprintf(stderr,"%d Net\n",port);
          }
          xastir_snprintf(output_net,
                          sizeof(output_net),
                          "%s>%s,TCPIP*:",
                          my_callsign,
                          VERSIONFRM);
          break;

        case DEVICE_SERIAL_TNC_HSP_GPS:
          if (port_data[port].status == DEVICE_UP && !loopback_only && !transmit_disable)
          {
            port_dtr(port,0);           // make DTR normal (talk to TNC)
          }
        /* Falls through. */

        case DEVICE_SERIAL_TNC_AUX_GPS:
        case DEVICE_SERIAL_KISS_TNC:
        case DEVICE_SERIAL_MKISS_TNC:
        case DEVICE_SERIAL_TNC:
        case DEVICE_AX25_TNC:

          if (debug_level & 1)
          {
            fprintf(stderr,"%d AX25 TNC\n",port);
          }
          output_net[0] = '\0';   // clear this for a TNC

          /* Set my call sign */
          xastir_snprintf(data_txt,
                          sizeof(data_txt),
                          "%c%s %s\r",
                          '\3',
                          "MYCALL",
                          my_callsign);

          if ( (port_data[port].device_type != DEVICE_SERIAL_KISS_TNC)
               && (port_data[port].device_type != DEVICE_SERIAL_MKISS_TNC)
               && (port_data[port].status == DEVICE_UP)
               && (devices[port].transmit_data == 1)
               && !transmit_disable
               && !loopback_only)
          {
            port_write_string(port,data_txt);
            usleep(10000);  // 10ms
          }

          done = 0;

          // Set unproto path.  First check whether we're
          // to use the igate path.  If so and the path
          // isn't empty, skip the rest of the path selection:
          if ( (use_igate_path)
               && (strlen(devices[port].unproto_igate) > 0) )
          {

            // WE7U:  Should we check here and in the following path
            // selection code to make sure that there are printable characters
            // in the path?  Also:  Output_my_aprs_data() has nearly identical
            // path selection code.  Fix it in one place, fix it in the other.

            // Check whether igate path is socially
            // acceptable.  Output warning if not, but
            // still allow the transmit.
            if(check_unproto_path(devices[port].unproto_igate))
            {
              popup_message_always(langcode("WPUPCFT046"),
                                   langcode("WPUPCFT043"));
            }

            xastir_snprintf(data_txt,
                            sizeof(data_txt),
                            "%c%s %s VIA %s\r",
                            '\3',
                            "UNPROTO",
                            VERSIONFRM,
                            devices[port].unproto_igate);

            xastir_snprintf(data_txt_save,
                            sizeof(data_txt_save),
                            "%s>%s,%s:",
                            my_callsign,
                            VERSIONFRM,
                            devices[port].unproto_igate);

            xastir_snprintf(path_txt,
                            sizeof(path_txt),
                            "%s",
                            devices[port].unproto_igate);

            done++;
          }


          // Check whether a path was passed to us as a
          // parameter:
          if ( (path != NULL) && (strlen(path) != 0) )
          {

            if (strncmp(path, "DIRECT PATH", 11) == 0)
            {
              // The user has requested a direct path

              xastir_snprintf(data_txt,
                              sizeof(data_txt),
                              "%c%s %s\r",
                              '\3',
                              "UNPROTO",
                              VERSIONFRM);

              xastir_snprintf(data_txt_save,
                              sizeof(data_txt_save),
                              "%s>%s:",
                              my_callsign,
                              VERSIONFRM);
            }
            else
            {

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
            }

            if (strncmp(path, "DIRECT PATH", 11) == 0)
            {
              // The user has requested a direct path
              path_txt[0] = '\0'; // Empty path
            }
            else
            {
              xastir_snprintf(path_txt,
                              sizeof(path_txt),
                              "%s",
                              path);
            }

            done++;

            // If "DEFAULT PATH" was passed to us, then
            // we're not done yet.
            //
            if (strncmp(path, "DEFAULT PATH", 12) == 0)
            {
              done = 0;
            }
          }

          if (!done)
          {

            // Set unproto path:  Get next unproto path
            // in sequence.
            unproto_path = (char *)select_unproto_path(port);

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


          if ( (port_data[port].device_type != DEVICE_SERIAL_KISS_TNC)
               && (port_data[port].device_type != DEVICE_SERIAL_MKISS_TNC)
               && (port_data[port].status == DEVICE_UP)
               && (devices[port].transmit_data == 1)
               && !transmit_disable
               && !loopback_only)
          {
            port_write_string(port,data_txt);
            usleep(10000);  // 10ms
          }

          // Set converse mode.  One european TNC
          // (tnc2-ui) doesn't accept "conv" but does
          // accept the 'k' command.  A Kantronics  KPC-2
          // v2.71 TNC accepts the "conv" command but not
          // the 'k' command.  Figures!
          //
          //                    xastir_snprintf(data_txt, sizeof(data_txt), "%c%s\r", '\3', "CONV");
          //                    xastir_snprintf(data_txt, sizeof(data_txt), "%c%s\r", '\3', "k");
          //                    xastir_snprintf(data_txt, sizeof(data_txt), "%c%s\r", '\3', CONVERSE_MODE);
          xastir_snprintf(data_txt, sizeof(data_txt), "%c%s\r", '\3', devices[port].device_converse_string);



          if ( (port_data[port].device_type != DEVICE_SERIAL_KISS_TNC)
               && (port_data[port].device_type != DEVICE_SERIAL_MKISS_TNC)
               && (port_data[port].status == DEVICE_UP)
               && (devices[port].transmit_data == 1)
               && !transmit_disable
               && !loopback_only)
          {
            port_write_string(port,data_txt);
            usleep(20000); // 20ms
          }
          break;

        default: /* unknown */
          ok = 0;
          break;
      } // End of switch
    }
    else        // Type == 1, raw data.  Probably igating something...
    {
      output_net[0] = '\0';
    }


    if (ok)
    {
      /* send data */
      xastir_snprintf(data_txt, sizeof(data_txt), "%s%s\r", output_net, message);
      if ( (port_data[port].status == DEVICE_UP)
           && (devices[port].transmit_data == 1)
           && !transmit_disable
           && !loopback_only)
      {

        // WE7U:  Change so that path is passed as well for KISS TNC
        // interfaces:  data_txt_save would probably be the one to pass,
        // or create a new string just for KISS TNC's.

        if ( (port_data[port].device_type == DEVICE_SERIAL_KISS_TNC)
             || (port_data[port].device_type == DEVICE_SERIAL_MKISS_TNC) )
        {

          // Transmit
          send_ax25_frame(port,
                          my_callsign,    // source
                          VERSIONFRM,     // destination
                          path_txt,       // path
                          data_txt);      // data
        }

        //WE7U:AGWPE
        else if (port_data[port].device_type == DEVICE_NET_AGWPE)
        {

          // Set unproto path.  First check whether we're
          // to use the igate path.  If so and the path
          // isn't empty, skip the rest of the path selection:
          if ( (use_igate_path)
               && (strlen(devices[port].unproto_igate) > 0) )
          {

            // Check whether igate path is socially
            // acceptable.  Output warning if not, but
            // still allow the transmit.
            if(check_unproto_path(devices[port].unproto_igate))
            {
              popup_message_always(langcode("WPUPCFT046"),
                                   langcode("WPUPCFT043"));
            }

            xastir_snprintf(path_txt,
                            sizeof(path_txt),
                            "%s",
                            devices[port].unproto_igate);
          }
          // Check whether a path was passed to us as a
          // parameter:
          else if ( (path != NULL) && (strlen(path) != 0) )
          {

            if (strncmp(path, "DEFAULT PATH", 12) == 0)
            {
              unproto_path = (char *)select_unproto_path(port);

              xastir_snprintf(path_txt,
                              sizeof(path_txt),
                              "%s",
                              unproto_path);
            }

            else if (strncmp(path, "DIRECT PATH", 11) == 0)
            {
              // The user has requested a direct path
              path_txt[0] = '\0'; // Empty string

              // WE7U
              // TEST THIS TO SEE IF IT WORKS ON AGWPE TO HAVE NO PATH

            }
            else
            {
              xastir_snprintf(path_txt,
                              sizeof(path_txt),
                              "%s",
                              path);
            }
          }
          else
          {
            // Set unproto path:  Get next unproto path in
            // sequence.

            unproto_path = (char *)select_unproto_path(port);

            xastir_snprintf(path_txt,
                            sizeof(path_txt),
                            "%s",
                            unproto_path);
          }

          // We need to remove the complete AX.25 header from data_txt before
          // we call this routine!  Instead put the digipeaters into the
          // ViaCall fields.  We do this above by setting output_net to '\0'
          // before creating the data_txt string.

          send_agwpe_packet(port,           // Xastir interface port
                            atoi(devices[port].device_host_filter_string) - 1, // AGWPE RadioPort
                            '\0',                         // Type of frame
                            (unsigned char *)my_callsign, // source
                            (unsigned char *)VERSIONFRM,  // destination
                            (unsigned char *)path_txt,    // Path,
                            (unsigned char *)data_txt,    // Data
                            strlen(data_txt) - 1);        // Skip \r
        }

        else
        {
          // Not a Serial KISS TNC interface
          port_write_string(port, data_txt);  // Transmit
        }

        if (debug_level & 1)
          fprintf(stderr,"Sending to interface:%d, %s\n",
                  port,
                  data_txt);


        // Put our transmitted packet into the Incoming Data
        // window as well.  This way we can see both sides of a
        // conversation.  data_port == -1 for x_spider port,
        // normal interface number otherwise.  -99 to get a "**"
        // display meaning all ports.
        //
        // For packets that we're igating we end up with a CR or
        // LF on the end of them.  Remove that so the display
        // looks nice.
        //
        // Check whether a path was passed to us as a
        // parameter:

        if ( (path != NULL) && (strlen(path) != 0) )
        {

          if (strncmp(path, "DEFAULT PATH", 12) == 0)
          {
            xastir_snprintf(temp,
                            sizeof(temp),
                            "%s>%s,%s:%s",
                            my_callsign,
                            VERSIONFRM,
                            unproto_path,
                            message);
          }
          else if (strncmp(path, "DIRECT PATH", 11) == 0)
          {
            // The user has requested a direct path
            xastir_snprintf(temp,
                            sizeof(temp),
                            "%s>%s:%s",
                            my_callsign,
                            VERSIONFRM,
                            message);
          }
          else
          {
            xastir_snprintf(temp,
                            sizeof(temp),
                            "%s>%s,%s:%s",
                            my_callsign,
                            VERSIONFRM,
                            path,
                            message);
          }
        }
        else
        {
          xastir_snprintf(temp,
                          sizeof(temp),
                          "%s>%s,%s:%s",
                          my_callsign,
                          VERSIONFRM,
                          unproto_path,
                          message);
        }
        makePrintable(temp);
        packet_data_add("TX ", temp, port);
      }

      if (debug_level & 2)
      {
        fprintf(stderr,"TX:%d<%s>\n",port,data_txt);
      }

      /* add newline on network data */
      if (port_data[port].device_type == DEVICE_NET_STREAM)
      {
        xastir_snprintf(data_txt, sizeof(data_txt), "\n");

        if ( (port_data[port].status == DEVICE_UP)
             && (devices[port].transmit_data == 1)
             && !transmit_disable
             && !loopback_only)
        {
          port_write_string(port,data_txt);
        }
      }
      else
      {
      }
    }
    //        if (incoming_port != -1)
    //            port = MAX_IFACE_DEVICES+1;    // process only one port
  }

  end_critical_section(&devices_lock, "interface.c:output_my_data" );

  // This will log a posit in the general format for a network interface
  // whether or not any network interfaces are currently up.
  xastir_snprintf(data_txt, sizeof(data_txt), "%s>%s,TCPIP*:%s", my_callsign,
                  VERSIONFRM, message);
  if (debug_level & 2)
  {
    fprintf(stderr,"output_my_data: Transmitting and decoding: %s\n", data_txt);
  }

  if (log_net_data)
    log_data( get_user_base_dir(LOGFILE_NET, logfile_tmp_path,
                                sizeof(logfile_tmp_path)),
              (char *)data_txt );


  // Note that this will only log one TNC line per transmission now matter
  // how many TNC's are defined.  It's a representative sample of what we're
  // sending out.  At least one TNC interface must be enabled in order to
  // have anything output to the log file here.
  if (data_txt_save[0] != '\0')
  {
    xastir_snprintf(data_txt, sizeof(data_txt), "%s%s", data_txt_save, message);
    if (log_tnc_data)
      log_data( get_user_base_dir(LOGFILE_TNC, logfile_tmp_path,
                                  sizeof(logfile_tmp_path)),
                (char *)data_txt );
  }


  if (enable_server_port && !loopback_only && !transmit_disable)
  {
    // Send data to the x_spider server

    if (type == 0)      // My data, add a header
    {
      xastir_snprintf(data_txt,
                      sizeof(data_txt),
                      "%s>%s,TCPIP*:%s",
                      my_callsign,
                      VERSIONFRM,
                      message);
    }
    else    // Not my data, don't add a header
    {
      xastir_snprintf(data_txt,
                      sizeof(data_txt),
                      "%s",
                      message);
    }

    if (writen(pipe_xastir_to_tcp_server,
               data_txt,
               strlen(data_txt)) != (int)strlen(data_txt))
    {
      fprintf(stderr,
              "output_my_data: Writen error: %d\n",
              errno);
    }
    // Terminate it with a linefeed
    if (writen(pipe_xastir_to_tcp_server, "\n", 1) != 1)
    {
      fprintf(stderr,
              "output_my_data: Writen error: %d\n",
              errno);
    }
  }
  // End of x_spider server send code


  // Decode our own transmitted packets.
  // Note that this function call is destructive to the first parameter.
  // This is why we call it _after_ we call the log_data functions.
  //
  // This must be the "L" packet we see in the View->Messages
  // dialog.  We don't see a "T" packet (for TNC) and we only see
  // "I" packets if we re-receive our own packet from the internet
  // feeds.
  if (incoming_port == -1)     // We were sending to all ports
  {
    // Pretend we received it from port 1
    decode_ax25_line( data_txt, DATA_VIA_LOCAL, 1, 1);
  }
  else    // We were sending to a specific port
  {
    decode_ax25_line( data_txt, DATA_VIA_LOCAL, incoming_port, 1);
  }
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
void output_waypoint_data(char *message)
{
  char data_txt[MAX_LINE_SIZE+5];
  int ok, start, finish, i;

  if (message == NULL)
  {
    return;
  }

  if (message[0] == '\0')
  {
    return;
  }

  if (debug_level & 1)
  {
    fprintf(stderr,"Sending to GPS interfaces: %s\n", message);
  }

  start = 0;
  finish = MAX_IFACE_DEVICES;

  begin_critical_section(&devices_lock, "interface.c:output_waypoint_data" );

  for (i = start; i < finish; i++)
  {
    ok = 1;
    switch (port_data[i].device_type)
    {

      case DEVICE_SERIAL_TNC_HSP_GPS:
        port_dtr(i,1);   // make DTR active (select GPS)
        break;

      case DEVICE_SERIAL_GPS:
        break;

      default: /* unknown */
        ok = 0;
        break;
    } // End of switch

    if (ok)     // Found a GPS interface
    {
      /* send data */
      xastir_snprintf(data_txt, sizeof(data_txt), "%s\r\n", message);

      if (port_data[i].status == DEVICE_UP)
      {
        port_write_string(i,data_txt);
        usleep(250000);    // 0.25 secs

        if (debug_level & 1)
        {
          fprintf(stderr,"Sending to interface:%d, %s\n",i,data_txt);
        }
      }

      if (debug_level & 2)
      {
        fprintf(stderr,"TX:%d<%s>\n",i,data_txt);
      }

      if (port_data[i].device_type == DEVICE_SERIAL_TNC_HSP_GPS)
      {
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
void tnc_data_clean(char *buf)
{

  if (debug_level & 1)
  {
    char filtered_data[MAX_LINE_SIZE+1];

    // strncpy is ok here as long as nulls not in data.  We
    // null-terminate it ourselves to make sure it is terminated.
    strncpy(filtered_data, buf, MAX_LINE_SIZE);
    filtered_data[MAX_LINE_SIZE] = '\0';    // Terminate it

    makePrintable(filtered_data);
    fprintf(stderr,"tnc_data_clean: called to clean %s\n", filtered_data);
  }

  while (!strncmp(buf,"cmd:",4))
  {
    int ii;

    // We're _shortening_ the string here, so we don't need to
    // know the length of the buffer unless it has no '\0'
    // terminator to begin with!  In that one case we could run
    // off the end of the string and get a segfault or cause
    // other problems.
    for (ii = 0; ; ii++)
    {
      buf[ii] = buf[ii+4];
      if (buf[ii] == '\0')
      {
        break;
      }
    }
  }

  if (debug_level & 1)
  {
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
int tnc_get_data_type(char *buf, int UNUSED(port) )
{
  register int i;
  int type=1;      // Don't know what it is yet.  Assume NMEA for now.

  if (debug_level & 1)
  {
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
  if (buf[0]=='$')
  {
    //This looks kind of NMEA-ish, let's check for known message type
    //headers ($P[A-Z][A-Z][A-Z][A-Z] or $GP[A-Z][A-Z][A-Z])
    if(buf[1]=='P')
    {
      for(i=2; i<=5; i++)
      {
        if (buf[i]<'A' || buf[i]>'Z')
        {
          type=0; // Disqualified, not valid NMEA-0183
          if (debug_level & 1)
          {
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
    else if(buf[1]=='G' && buf[2]=='P')
    {
      for(i=3; i<=5; i++)
      {
        if (buf[i]<'A' || buf[i]>'Z')
        {
          type=0; // Disqualified, not valid NMEA-0183
          if (debug_level & 1)
          {
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
  else    // Must be APRS data
  {
    type = 0;
  }

  if (debug_level & 1)
  {
    if (type == 0)
    {
      fprintf(stderr,"APRS data\n");
    }
    else
    {
      fprintf(stderr,"NMEA data\n");
    }
  }

  return(type);
}


