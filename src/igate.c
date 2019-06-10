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

#include <termios.h>
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

#include "xastir.h"
#include "igate.h"
#include "main.h"
#include "interface.h"
#include "xa_config.h"
#include "util.h"

// Must be last include file
#include "leak_detection.h"



time_t last_nws_stations_file_time = 0;
int NWS_stations = 0;
int max_NWS_stations = 0;
NWS_Data *NWS_station_data;


void load_NWS_stations(char *file);
int check_NWS_stations(char *call);



// Struct for holding packet data.  We use dynamically-allocated
// singly-linked lists.  The last record should have its "next"
// pointer set to NULL.
//
typedef struct _DupeRecord
{
  char    data[MAX_LINE_SIZE+15]; // Packet data
  time_t  time;                       // The time the record was inserted
  struct  _DupeRecord *next;          // pointer to next record in list
} DupeRecord;



// Sent and Heard queue pointers.  These are used for the dupe-checking
// we do in the below routines.  We have one Sent and one Heard queue
// for each interface device.  These pointers will point to the head of
// each queue.  We really only need these queues for each TNC interface,
// but the user might destroy a NET interface and create a TNC interface
// during a single runtime, so we need to populate all of the pointers
// just in case.  If people switch types, the old queue will empty out
// (effectively anyway) within XX seconds, so we don't have to worry
// about cleaning out the queues in this case.
//
DupeRecord *heard_queue[MAX_IFACE_DEVICES];
DupeRecord  *sent_queue[MAX_IFACE_DEVICES];



// Types of queues for each interface
#define HEARD               0
#define SENT                1



// Insert mode for the queues
#define NO_FORCED_INSERT    0
#define FORCED_INSERT       1





// Initialization routine for this module which sets up the queue
// pointers when Xastir first starts.  Called from main.c:main()
//
void igate_init(void)
{
  int i;

  for (i = 0; i < MAX_IFACE_DEVICES; i++)
  {
    heard_queue[i] = NULL;
    sent_queue[i] = NULL;
  }
}





//
// not_a_dupe:  Function which checks for duplicate packets.
//
// Returns: 1 if it's _not_ a duplicate record or we have an error
//          0 if it _is_ a duplicate record
//
// Since we need to run through every record checking for dupes anyway,
// we check the timestamp on each one as we go through.  If too old, we
// delete it from the head of the chain.  We add new records to the end.
// This makes it easy to keep it as a singly-linked list, and only have
// to deal with one record at a time.
//
// The way this is set up we keep a thirty second queue for each
// interface.  Any records older than this are at the head of the chain
// and are deleted one by one whenever this routine is called again due
// to a new packet coming through.  It's ok if these records sit around
// on the queue for a long time due to no igate activity.  It doesn't
// take long to delete them!
//
int not_a_dupe(int queue_type, int port, char *line, int insert_mode)
{
  DupeRecord *ptr;
  DupeRecord *ptr_last;
  int insert_new;
  time_t time_cutoff;
  int found_dupe = 0;
  char match_line[MAX_LINE_SIZE*2];
  char line2[MAX_LINE_SIZE+1];
  char *c0, *c1, *c2;


  if ( (line == NULL) || (line[0] == '\0') )
  {
    return(1);
  }


  // Figure out what's "old"
  time_cutoff = sec_now() - (time_t)29;   // 29 seconds ago


  // Fill the destination string with zeroes.  This is a nice
  // segfault-prevention technique.  Whatever strings we throw in here
  // will be automatically terminated.
  memset(match_line, 0, MAX_LINE_SIZE*2);


  switch (queue_type)
  {

    case HEARD:
      ptr_last = ptr = heard_queue[port]; // May be NULL!

      // The insert_into_heard_queue() function below (called by
      // db.c decode routines in turn) will call this function
      // with FORCED_INSERT.  Other routines in igate.c will call
      // it with NO_FORCED_INSERT.  For the Heard queue we only
      // want the db.c decode routines inserting records.
      if (insert_mode == FORCED_INSERT)
      {
        insert_new = 1;  // Insert new records.
      }
      else
      {
        insert_new = 0;  // Don't insert new records.
      }

      // RF packets will have third-party headers and regular
      // headers that must be stripped off.  We only want to store
      // 3rd party RF strings in the Heard queue as the others
      // aren't going to be igated anyway.  For matching and
      // storage purposes the 3rd party packets should look
      // identical to how they were originally passed on the 'net,
      // so that we can try to find duplicates before transmitting
      // them again.

// NOTE:  Below is the parsing code for an internet packet for the Sent
// queue.  Modify it to parse 3rd party packets for the Heard queue.

// VE7VFM-12>APD214,VE7VAN-3*,WIDE3*:}WA7JAK>APK002,TCPIP*,VE7VFM-12*::N7WGR-7  :does{2

      // Changes needed before parsing code:  Get rid of first
      // part of packet up to the '}' symbol.  After this the
      // generic parsing code will work.
// Note that the REPLY-ACK algorithm also uses the '}' symbol.

      c0 = strstr(line, ":}"); // Find start of 3rd party packet
      if (c0 == NULL)     // Not 3rd party packet
      {
        if (debug_level & 1024)
        {
          fprintf(stderr," Not 3rd party HeardQ: %s\n",line);
        }
        return(1);
      }

      // Copy original packet into line2 for later parsing.  We
      // want to keep the '}' character because our own
      // transmissions out RF have that character as well.
// Note that the REPLY-ACK algorithm also uses the '}' symbol.
      if (debug_level & 1024)
      {
        fprintf(stderr,"3rd party HeardQ: %s\n",line);
      }

      xastir_snprintf(line2,
                      sizeof(line2),
                      "%s",
                      c0+1);

      break;

    case SENT:
      // For this queue we always want to insert records.  Only
      // igate.c functions call this.
      ptr_last = ptr = sent_queue[port];  // May be NULL!
      insert_new = 1; // Insert new records

      // No extra changes needed before parsing code, Example:
      // }VE7VFM-11>APW251,TCPIP,WE7U-14*::VE7VFM-9 :OK GOT EMAIL OK{058
      xastir_snprintf(line2,
                      sizeof(line2),
                      "%s",
                      line);

      if (debug_level & 1024)
      {
        fprintf(stderr," COMPLETE SENT PACKET: %s\n",line2);
      }

      break;

    default:
      // We shouldn't be here.
      return(1);

      break;
  }


  // Create the string we're going to compare against and that we
  // might store in the queue.  Knock off the path info and just check
  // source/destination/info portions of the packet for a match.

  c1 = strstr(line2, ","); // Find comma after destination
  c2 = strstr(line2, ":"); // Find end of path

  if ( (c1 != NULL) && (c2 != NULL) )             // Found both separators
  {

    // Copy source/destination portion
    xastir_snprintf(match_line,
                    sizeof(match_line),
                    "%s",
                    line2);
    match_line[(int)(c1-line2)] = '\0';         // Terminate the substring

    strncat(match_line,                         // Copy info portion
            c2+1,
            sizeof(match_line) - 1 - strlen(match_line));
  }
  else    // At least one separator was not found, copy entire string
  {
    xastir_snprintf(match_line,
                    sizeof(match_line),
                    "%s",
                    line2);
  }


  // Run through the selected queue from beginning to end.  If the
  // pointer is NULL, the queue is empty and we're already done.
  while (ptr != NULL && !found_dupe)
  {

    // Check the timestamp to determine whether to delete this
    // record
    if (ptr->time < time_cutoff)    // Old record, delete it
    {
      DupeRecord *temp;

      if (debug_level & 1024)
      {
        switch (queue_type)
        {
          case HEARD:
            fprintf(stderr,"HEARD Deleting record: %s\n",ptr->data);
            break;
          case SENT:
            fprintf(stderr," SENT Deleting record: %s\n",ptr->data);
            break;
          default:
            break;
        }
      }

      // Delete record and free up the space
      temp = ptr;
      ptr = ptr->next;    // May be NULL!
      free(temp);

      // Point master queue pointer to new head of queue.
      // Make sure to carry along ptr_last as well, as this is
      // our possible insertion point later.
      switch (queue_type)
      {
        case HEARD:
          heard_queue[port] = ptr_last = ptr; // May be NULL!
          break;
        case SENT:
          sent_queue[port] = ptr_last = ptr;  // May be NULL!
        default:
          break;
      }
    }

    else    // Record is current.  Check for a match.
    {

      //fprintf(stderr,"\n\t\t%s\n\t\t%s\n",ptr->data,match_line);

      if (strcmp(ptr->data,match_line) == 0)
      {
        // We found a dupe!  We're done with the loop.
        found_dupe++;

        if (debug_level & 1024)
        {
          switch (queue_type)
          {
            case HEARD:
              fprintf(stderr,"HEARD*     Found dupe: %s\n",match_line);
              break;
            case SENT:
              fprintf(stderr,"SENT*      Found dupe: %s\n",match_line);
            default:
              break;
          }
        }
      }
      else    // Not a dupe, skip to the next record in the
      {
        // queue.  Keep a pointer to the last record
        // compared so that we have a possible insertion
        // point later.  Once we hit the end (NULL), we
        // can't back up one.
        ptr_last = ptr;     // Save pointer to last record
        ptr = ptr->next;    // Advance one.  May be NULL!
      }
    }
  }   // End of while loop


  if (found_dupe)
  {

    if (debug_level & 1024)
    {
      switch (port_data[port].device_type)
      {
        case DEVICE_SERIAL_TNC_AUX_GPS:
        case DEVICE_SERIAL_TNC_HSP_GPS:
        case DEVICE_SERIAL_TNC:
        case DEVICE_AX25_TNC:
        case DEVICE_SERIAL_KISS_TNC:
        case DEVICE_SERIAL_MKISS_TNC:
        case DEVICE_NET_AGWPE:
          fprintf(stderr,"        Found RF dupe: %s\n",match_line);
          break;

        default:
          fprintf(stderr,"       Found NET dupe: %s\n",match_line);
          break;
      }
    }

    return(0);  // Found a dupe, return
  }

  else
  {

    // If insert_new == 1, insert each non-dupe record into the
    // queue and give it a timestamp.  ptr_next is currently
    // either NULL or points to the last record in the chain.
    if (insert_new)
    {
      DupeRecord *temp;

      if (debug_level & 1024)
      {
        switch (queue_type)
        {
          case HEARD:
            fprintf(stderr,"HEARD   Adding record: %s\n",match_line);
            break;
          case SENT:
            fprintf(stderr," SENT   Adding record: %s\n",match_line);
            break;
          default:
            break;
        }
      }

      // Allocate a new storage space for the record and fill
      // it in.
      temp = (DupeRecord *)malloc(sizeof(DupeRecord));

      if (!temp)
      {
        fprintf(stderr,"Couldn't allocate memory in not_a_dupe()\n");
        return(1);  // Send back "not a dupe"
      }

      temp->time = (time_t)sec_now();

      memcpy(temp->data, match_line, sizeof(temp->data));
      temp->data[sizeof(temp->data)-1] = '\0';  // Terminate string

      temp->next = NULL;  // Will be the end of the linked list

      if (ptr_last == NULL)   // Queue is currently empty
      {

        // Add record to empty list.  Point master queue pointer
        // to new head of queue.
        switch (queue_type)
        {
          case HEARD:
            heard_queue[port] = temp;
            break;
          case SENT:
            sent_queue[port] = temp;
          default:
            break;
        }
      }
      else    // Queue is not empty, add the record to the end of
      {
        // the list.
        ptr_last->next = temp;
      }
    }
  }
  return(1);  // Nope, not a dupe
}





// Function which the receive routines call to insert a received
// packet into the HEARD queue for an interface.  The packet will
// get added to the end of the linked list if it's not a duplicate
// record.
//
// Check to make sure it's an RF interface, else return
//
void insert_into_heard_queue(int port, char *line)
{

  switch (port_data[port].device_type)
  {

    case DEVICE_SERIAL_TNC_AUX_GPS:
    case DEVICE_SERIAL_TNC_HSP_GPS:
    case DEVICE_SERIAL_TNC:
    case DEVICE_AX25_TNC:
    case DEVICE_SERIAL_KISS_TNC:
    case DEVICE_SERIAL_MKISS_TNC:
    case DEVICE_NET_AGWPE:

      // We're not using the dupe check function, but merely the
      // expiration and insert functions of not_a_dupe()

      // Don't insert the "Tickle" lines that keep the internet
      // sockets alive
      if ( (strncasecmp(line,"# Tickle",8) != 0)
           && (strncasecmp(line,"#Tickle",7) != 0) )
      {
        (void)not_a_dupe(HEARD, port, line, FORCED_INSERT);
      }

      break;

    default:    // Get out if not an RF interface
      return;

      break;
  }
}





/****************************************************************/
/* output data to inet interfaces                               */
/* line: data to send out                                       */
/* port: port data came from                                    */
/****************************************************************/
void output_igate_net(char *line, int port, int third_party)
{
  char data_txt[MAX_LINE_SIZE+5];
  char temp[MAX_LINE_SIZE+5];
  char *call_sign;
  char *path;
  char *message;
  int len,i,x,first;
  int igate_options;
  char log_file_path[MAX_VALUE];

  call_sign = NULL;
  path      = NULL;
  message   = NULL;
  first     = 1;

  if (line == NULL)
  {
    return;
  }

  if (line[0] == '\0')
  {
    return;
  }

  // Don't igate packets read in from a log file (port -1).
  // Packets from x_spider (port -2) are ok to igate.
  if (port == -1)
  {
    return;
  }

//fprintf(stderr,"Igating: %s\n", line);

  // Should we Igate from RF->NET?
  if (operate_as_an_igate <= 0)
  {
    return;
  }

  xastir_snprintf(temp,
                  sizeof(temp),
                  "%s",
                  line);

  // Check for null call_sign field
  call_sign = strtok(temp,">");
  if (call_sign == NULL)
  {
    return;
  }

  // Check for null path field
  path = strtok(NULL,":");
  if (path == NULL)
  {
    return;
  }

  get_user_base_dir(LOGFILE_IGATE,log_file_path, sizeof(log_file_path));
  // Check for "TCPIP" or "TCPXX" in the path.  If found, don't
  // gate this into the internet again, it's already been gated to
  // RF, which means it's already been on the 'net.  No looping
  // allowed here...
  //
  // We also now support NOGATE and RFONLY options.  If these are
  // seen in the path, do _not_ gate those packets into the
  // internet.
  //
  // Don't gate OpenTrac expanded packets to the 'net.
  //
  if ( (strstr(path,"TCPXX") != NULL)
       || (strstr(path,"TCPIP") != NULL && port >= 0) // x_spider ok
       || (strstr(path,"NOGATE") != NULL)
       || (strstr(path,"RFONLY") != NULL)
       || (strstr(path,"OPNTRK") != NULL)      // OpenTrac Packet
       || (strstr(path,"OPNTRC") != NULL) )    // OpenTrac Packet
  {

    if (log_igate && (debug_level & 1024) )
    {

      xastir_snprintf(temp,
                      sizeof(temp),
                      "IGATE RF->NET(%c):%s\n",
                      third_party ? 'T':'N',
                      line);
      log_data( log_file_path, temp );

      xastir_snprintf(temp,
                      sizeof(temp),
                      "REJECT: Packet was gated before or shouldn't be gated!\n");
      log_data( log_file_path, temp );

      fprintf(stderr, "%s", temp);
    }
    return;
  }

  // Check for null message field
  message = strtok(NULL,"");
  if (message == NULL)
  {
    return;
  }

  // Check for third party messages.  We don't want to gate these
  // back onto the internet feeds
// Note that the REPLY-ACK algorithm also uses the '}' symbol.
  if (message[0] == '}')
  {

    if (log_igate && (debug_level & 1024) )
    {

      xastir_snprintf(temp,
                      sizeof(temp),
                      "IGATE RF->NET(%c):%s\n",
                      third_party ? 'T':'N',
                      line);
      log_data( log_file_path, temp );

      xastir_snprintf(temp,
                      sizeof(temp),
                      "REJECT: Third party traffic!\n");
      log_data( log_file_path, temp );

      fprintf(stderr, "%s", temp);
    }
    return;
  }

  // Check for "general" queries.  We don't wish to gate these in
  // either direction.  There are exactly three general query
  // types defined in the spec.
  //
  if ( (   strstr(message,"?APRS?" ) != NULL)
       || (strstr(message,"?IGATE?") != NULL)
       || (strstr(message,"?WX?"   ) != NULL) )
  {

    // We found a general query, don't gate it.

    if (log_igate && (debug_level & 1024) )
    {

      xastir_snprintf(temp,
                      sizeof(temp),
                      "IGATE RF->NET(%c):%s\n",
                      third_party ? 'T':'N',
                      line);
      log_data( log_file_path, temp );

      xastir_snprintf(temp,
                      sizeof(temp),
                      "REJECT: General Query!\n");
      log_data( log_file_path, temp );

      fprintf(stderr, "%s", temp);
    }
    return;
  }

  len = (int)strlen(call_sign);
  for (i=0; i<len; i++)
  {

    // just in case we see an asterisk get rid of it
    if (call_sign[i] == '*')
    {
      call_sign[i] = '\0';
      i = len+1;
    }
  }

  // Check for my callsign
  if (strcmp(call_sign,my_callsign) == 0)
  {

    if (log_igate && (debug_level & 1024) )
    {

      xastir_snprintf(temp,
                      sizeof(temp),
                      "IGATE RF->NET(%c):%s\n",
                      third_party ? 'T':'N',
                      line);
      log_data( log_file_path, temp );

      xastir_snprintf(temp,
                      sizeof(temp),
                      "REJECT: From my call!\n");
      log_data( log_file_path, temp );

      fprintf(stderr, "%s", temp);
    }
    return;
  }

  // Should I filter out more here.. get rid of all data
  // or Look in the path for things line "AP" "GPS" "ID" etc..?

  begin_critical_section(&devices_lock, "igate.c:output_igate_net" );

  // If received from x_spider port or it's our own tactical call.
  // Here are the special port numbers we might see:
  // -1: We're reading in from a log file
  // -2: Packet came from x_spider server port (therefore it's
  //     already authenticated)
  // -3: We're reading in tactical calls from file
  //
  if (port == -1)         // Packet came from a log file.
  {
    igate_options = 0;  // Don't igate it.
  }

  else if (port == -2)    // Packet came from x_spider server port
  {
    igate_options = 1;  // Ok to igate.
  }

  else if (port == -3)    // We're reading tactical call from file.
  {
    igate_options = 0;  // Don't igate it.
  }

  else if (port < -2)     // Errant port number.
  {
    igate_options = 0;  // Don't igate it.
  }

  else    // Port number is 0 or positive number.  A real port.
    // Decide whether to igate it based on the port's
    // configuration.
  {
    igate_options = devices[port].igate_options;
  }


  end_critical_section(&devices_lock, "igate.c:output_igate_net" );

  if (igate_options <= 0 )
  {

    if (log_igate && (debug_level & 1024) )
    {

      xastir_snprintf(temp,
                      sizeof(temp),
                      "IGATE RF->NET(%c):%s\n",
                      third_party ? 'T':'N',
                      line);
      log_data( log_file_path, temp );

      xastir_snprintf(temp,
                      sizeof(temp),
                      "REJECT: No RF->NET from input port [%d]!\n",
                      port);
      log_data( log_file_path, temp );

      fprintf(stderr, "%s", temp);
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
  for (x = 0; x < MAX_IFACE_DEVICES; x++)
  {

    // Find all internet interfaces that are "up"
    if (port_data[x].device_type == DEVICE_NET_STREAM
        && x!=port && port_data[x].status == DEVICE_UP)
    {
      int pcode;

      // Check whether we have a valid callsign/password
      // combination for this interface.  If not, don't gate
      // packets to it.
      pcode = atoi(port_data[x].device_host_pswd);
      if (checkHash(my_callsign, pcode))
      {
        // The pcode checks out.  Allow sending the
        // packet out to the internet.

        // log traffic for the first "up" interface only
        if (log_igate && first)
        {
          xastir_snprintf(temp,
                          sizeof(temp),
                          "IGATE RF->NET(%c):%s\n",
                          third_party ? 'T':'N',
                          line);
          log_data( log_file_path, temp );

          first = 0;
        }

        // Now log the interface that each bit of traffic
        // goes out on.
        xastir_snprintf(temp,
                        sizeof(temp),
                        "TRANSMIT: IGate RF->NET packet on device:%d\n",
                        x);

        // log output
        if (log_igate)
        {
          log_data( log_file_path, temp );
        }

        if (debug_level & 1024)
        {
          fprintf(stderr,"%s\n",temp);
        }

        // Write this data out to the Inet port The "1"
        // means raw format, the last digit says to _not_
        // use the unproto_igate path
        output_my_data(data_txt,x,1,0,0,NULL);
//fprintf(stderr,"Sending: %s\n", data_txt);
      }
    }
  }
}





/****************************************************************/
/* output data to tnc interfaces                                */
/* from: type of port heard from (No! It's the source call!)    */
/* call: call sign heard from (No! It's the destination call!)  */
/* line: data to gate to rf                                     */
/* port: port data came from                                    */
/****************************************************************/
void output_igate_rf(char *from, char *call, char *path, char *line,
                     int port, int third_party, char *object_name)
{

  char temp[MAX_LINE_SIZE+20];
  int x;
  int first = 1;
  int found_in_nws_file = 0;
  char log_file_path[MAX_VALUE];
  char nws_file_path[MAX_VALUE];


  if ( (from == NULL) || (call == NULL) || (path == NULL) || (line == NULL) )
  {
    return;
  }

  if ( (from[0] == '\0') || (call[0] == '\0') || (path[0] == '\0') || (line[0] == '\0') )
  {
    return;
  }

  // Should we Igate from NET->RF?
  if (operate_as_an_igate <= 1)
  {
    return;
  }


  get_user_base_dir(LOGFILE_IGATE,log_file_path, sizeof(log_file_path));

  // Don't gate anything with NOGATE in it, in either direction.
  // Same for OpenTrac packets.
  if ( (strstr(path,"NOGATE") != NULL)
       || (strstr(path,"OPNTRK") != NULL)     // OpenTrac Packet
       || (strstr(path,"OPNTRC") != NULL) )   // OpenTrac Packet
  {
    // "NOGATE" was found in the header.  Don't gate it.
    if (log_igate && (debug_level & 1024) )
    {
      xastir_snprintf(temp,
                      sizeof(temp),
                      "IGATE NET->RF(%c):%s\n",
                      third_party ? 'T':'N',
                      line);
      log_data( log_file_path, temp );

      xastir_snprintf(temp,
                      sizeof(temp),
                      "REJECT: NOGATE found in path or shouldn't be gated!\n");
      log_data( log_file_path, temp );
      fprintf(stderr, "%s", temp);
    }
    return;
  }

  // Don't gate "general" queries in any direction.  There are
  // exactly three general query types defined in the spec.
  //
  if (   (strstr(line,"?APRS?" ) != NULL)
         || (strstr(line,"?IGATE?") != NULL)
         || (strstr(line,"?WX?"   ) != NULL) )
  {

    // We found a general query, don't gate it.

    if (log_igate && (debug_level & 1024) )
    {
      xastir_snprintf(temp,
                      sizeof(temp),
                      "IGATE NET->RF(%c):%s\n",
                      third_party ? 'T':'N',
                      line);
      log_data( log_file_path, temp );

      xastir_snprintf(temp,
                      sizeof(temp),
                      "REJECT: General Query!\n");
      log_data( log_file_path, temp );
      fprintf(stderr, "%s", temp);
    }
    return;
  }


  // check to see if the nws-stations file is newer than last read
  get_user_base_dir("data/nws-stations.txt",nws_file_path,
                    sizeof(nws_file_path));
  if (last_nws_stations_file_time < file_time(nws_file_path))
  {
    last_nws_stations_file_time = file_time(nws_file_path);
    load_NWS_stations(nws_file_path);
    //fprintf(stderr,"NWS Station file time is old\n");
  }


  // Check whether gating of packets from this station/object/item
  // has been specifically authorized via the nws-stations.txt
  // mechanism.
  //
  if (object_name)    // It's an object or item name
  {

    if ( check_NWS_stations( object_name ) || group_active(object_name))
    {

      found_in_nws_file++; // Object/Item is in nws-stations.txt
    }
  }
  else                // It's a station callsign
  {

    if ( check_NWS_stations( from ) || group_active(from))
    {

      found_in_nws_file++; // Source callsign is in nws-stations.txt
    }
  }
// The above is really the same as the following code, but less
// confusing:
//    if (check_NWS_stations( (object_name) ? object_name : from ) ) {
//        found_in_nws_file++;
//    }


  // Check for TCPXX in string only if station wasn't found in the
  // nws-stations.txt file.  If TCPXX found, we have an
  // unregistered net user and the packet shouldn't normally head
  // to RF.
  //
  // I removed the trailing asterisk -we7u
  //
  // Note that we CAN now gate stations to RF that have TCPXX in
  // the string if they are authorized via the nws-stations.txt
  // mechanism.
  //
  if (!found_in_nws_file)   // Skip this check if they're always authorized via the file
  {

    if (strstr(path,"TCPXX") != NULL)
    {

      // "TCPXX" was found in the header.  We have an
      // unregistered user.

      if (log_igate && (debug_level & 1024) )
      {
        xastir_snprintf(temp,
                        sizeof(temp),
                        "IGATE NET->RF(%c):%s\n",
                        third_party ? 'T':'N',
                        line);
        log_data( log_file_path, temp );

        xastir_snprintf(temp,
                        sizeof(temp),
                        "REJECT: Unregistered net user!\n");
        log_data( log_file_path, temp );
        fprintf(stderr, "%s", temp);
      }
      return;
    }
  }



// If we made it to this point, the packet is from an authorized net
// user (no TCPXX found in the path), or the callsign has been
// authorized via the nws-stations.txt file (whether or not TCPXX
// was found in the path).
//
// Found in file: Gate always
//
// Not found:     Gate if TCPXX not in path -AND- if destination
//                station was heard in last hour on RF -AND- if
//                source station was NOT heard in last hour on RF.



  // Check whether the source and destination calls have been
  // heard on local RF.
  if ( !found_in_nws_file // Skip this check if they're always authorized via the file
       && ( !heard_via_tnc_in_past_hour(call)    // Haven't heard destination call in previous hour
            || heard_via_tnc_in_past_hour(from)) )    // Have heard source call in previous hour
  {

    if (log_igate && (debug_level & 1024) )
    {
      xastir_snprintf(temp,
                      sizeof(temp),
                      "IGATE NET->RF(%c):%s\n",
                      third_party ? 'T':'N',
                      line);
      log_data( log_file_path, temp );

      //  heard(call),  heard(from) : RF-to-RF talk
      // !heard(call),  heard(from) : Destination not heard on TNC
      // !heard(call), !heard(from) : Destination/source not heard on TNC

      if (!heard_via_tnc_in_past_hour(call))
        xastir_snprintf(temp,
                        sizeof(temp),
                        "REJECT: Destination not heard on TNC within an hour %s!\n",
                        call );
      else
        xastir_snprintf(temp,
                        sizeof(temp),
                        "REJECT: RF->RF talk!\n");
      log_data( log_file_path, temp );
      fprintf(stderr, "%s", temp);
    }
    return;
  }



  // Station we are going to is heard via tnc but station sending
  // shouldn't be heard via TNC.  Write data out to interfaces.
  for (x=0; x<MAX_IFACE_DEVICES; x++)
  {

    //fprintf(stderr,"%d\n",x);

//WE7U
    // Check here against "heard" queue for each interface.
    // Drop the packet on the floor if it was already received
    // on this interface within the last XX seconds.  This means
    // that some other igate beat us to the punch.  The receive
    // routines have to fill this queue.  Prune outdated
    // records.
    //
    // Also check here against "sent" queue for each interface.
    // Drop the packet on the floor if already sent within the
    // last XX seconds.  Add this packet to the queue if it
    // isn't already in it.  Prune outdated records.
    if (x != port
        && not_a_dupe(HEARD, x, line, NO_FORCED_INSERT)
        && not_a_dupe( SENT, x, line, NO_FORCED_INSERT) )
    {

      //fprintf(stderr,"output_igate_rf: Not a dupe port %d, transmitting\n",x);

      switch (port_data[x].device_type)
      {

        case DEVICE_SERIAL_TNC_AUX_GPS:
        case DEVICE_SERIAL_TNC_HSP_GPS:
        case DEVICE_SERIAL_TNC:
        case DEVICE_AX25_TNC:
        case DEVICE_SERIAL_KISS_TNC:
        case DEVICE_SERIAL_MKISS_TNC:
        case DEVICE_NET_AGWPE:

          begin_critical_section(&devices_lock, "igate.c:output_igate_rf" );

          if (devices[x].igate_options>1 && port_data[x].status==DEVICE_UP)
          {

            // log traffic for first "up" interface only
            if (log_igate && first)
            {
              xastir_snprintf(temp,
                              sizeof(temp),
                              "IGATE NET->RF(%c):%s\n",
                              third_party ? 'T':'N',
                              line);
              log_data( log_file_path, temp );
              first = 0;
            }

            xastir_snprintf(temp,
                            sizeof(temp),
                            "TRANSMIT: IGate NET->RF packet on device:%d\n",
                            x);

            // log output
            if (log_igate)
            {
              log_data( log_file_path, temp );
            }

            if (debug_level & 1024)
            {
              fprintf(stderr, "%s", temp);
            }

            // ok write this data out to the RF port

            end_critical_section(&devices_lock, "igate.c:output_igate_rf" );

            // First "0" means "cooked"
            // format, last digit: use
            // unproto_igate path
            output_my_data(line,x,0,0,1,NULL);

//fprintf(stderr, "Igating->RF: %s\n", line);

            begin_critical_section(&devices_lock, "igate.c:output_igate_rf" );

          }
          else
          {
            if (log_igate && (debug_level & 1024) )
            {
              xastir_snprintf(temp,
                              sizeof(temp),
                              "IGATE NET->RF(%c):%s\n",
                              third_party ? 'T':'N',
                              line);
              log_data( log_file_path, temp );

              xastir_snprintf(temp,
                              sizeof(temp),
                              "REJECT: NET->RF on port [%d]!\n",
                              x);
              log_data( log_file_path, temp );
              fprintf(stderr, "%s", temp);
            }
          }

          end_critical_section(&devices_lock, "igate.c:output_igate_rf" );

          break;

        default:
          break;
      }   // End of switch
    }   // End of if
  }   // End of for
}





void add_NWS_stations(void)
{
  void *tmp_ptr;

  if (NWS_stations>=max_NWS_stations)
  {
    if ((tmp_ptr = realloc(NWS_station_data, sizeof(NWS_Data)*(max_NWS_stations+11))))
    {
      NWS_station_data = tmp_ptr;
      max_NWS_stations += 10;
    }
    else
    {
      fprintf(stderr,"Unable to allocate more space for NWS_station_data\n");
    }
  }
}





/****************************************************************/
/* Load NWS stations file                                       */
/* file: file to read                                           */
/****************************************************************/
void load_NWS_stations(char *file)
{
  FILE *f;
  char line[40];

  if (file == NULL)
  {
    return;
  }

  if (file[0] == '\0')
  {
    return;
  }

  if (NWS_station_data)
  {
    free(NWS_station_data);
    NWS_station_data = NULL;
  }

  NWS_stations = 0;
  max_NWS_stations = 0;

  f = fopen(file,"r");
  if (f!=NULL)
  {
    while (!feof(f))
    {
      if (strlen(get_line(f,line,40))>0)
      {
        // look for comment
        if (line[0] != '#' )
        {
          NWS_stations++;
          add_NWS_stations();
          if (NWS_station_data != NULL)
          {
            // add data
            // Note:  Size of string variable is 12
            // bytes, defined in igate.h
            if (1 != sscanf(line,"%11s",NWS_station_data[NWS_stations-1].call))
            {
              fprintf(stderr,"load_NWS_stations: sscanf parsing error\n");
            }
            if (debug_level & 1024)
            {
              fprintf(stderr,"LINE:%s\n",line);
            }
          }
          else
          {
            fprintf(stderr,"Can't allocate data space for NWS station\n");
          }
        }
      }
    }
    (void)fclose(f);
  }
  else
  {
    fprintf(stderr,"Couldn't open NWS stations file: %s\n", file);
  }
}





// check NWS stations file
//
// call: call to check
// returns 1 for found
//
// Both the incoming call and the stored call we're matching against
// have to be >= 3 characters long.  This routine will match only up
// to the length of the stored string, so we now allow partial
// matches.
//
int check_NWS_stations(char *call)
{
  int ok, i, length, length_incoming;


  if (call == NULL)
  {
    return(0);
  }

  if (call[0] == '\0')
  {
    return(0);
  }

  if (NWS_station_data == NULL)
  {
    return(0);
  }

  if (debug_level & 1024)
  {
    fprintf(stderr,"igate.c::check_NWS_stations %s\n", call);
  }

  // Make sure that the incoming call is longer than three
  // characters.  If not, skip it.
  length_incoming = strlen(call);
  if (length_incoming < 3)
  {
    return(0);
  }

  ok=0;
  for (i=0; i<NWS_stations && !ok; i++)
  {

    // Compute length of stored string.  If it's shorter than
    // three characters, skip it and go on to the next one.
    length = strlen(NWS_station_data[i].call);

    if (length >= 3)
    {
      // Compare the incoming call only up to the length of the
      // stored call.  This allows partial matches.  The
      // stored call could be significantly shorter than the
      // incoming call, but at least three characters.
      if (strncasecmp(call, NWS_station_data[i].call, length)==0)
      {

        ok=1; // match found
        if (debug_level & 1024)
        {
          fprintf(stderr,"NWS-MATCH:(%s) (%s)\n",NWS_station_data[i].call,call);
        }
      }
    }
    else
    {
      // Do nothing.  Stored call is too short.
    }
  }
  return(ok);
}





/****************************************************************/
/* output NWS data to tnc interfaces                            */
/* from: type of port heard from                                */
/* call: call sign heard from                                   */
/* line: data to gate to rf                                     */
/* port: port data came from                                    */
/****************************************************************/
void output_nws_igate_rf(char *from, char *path, char *line, int port, int third_party)
{
  char temp[MAX_LINE_SIZE+20];
  int x;
  int first = 1;
  char log_file_path[MAX_VALUE];
  char nws_file_path[MAX_VALUE];


  if ( (from == NULL) || (path == NULL) || (line == NULL) )
  {
    return;
  }

  if ( (from[0] == '\0') || (path[0] == '\0') || (line[0] == '\0') )
  {
    return;
  }

  // Should we Igate from NET->RF?
  if (operate_as_an_igate <= 1)
  {
    return;
  }

  get_user_base_dir(LOGFILE_IGATE,log_file_path, sizeof(log_file_path));
  get_user_base_dir("data/nws-stations.txt",nws_file_path,
                    sizeof(nws_file_path));

  // Check for TCPXX in string!  If found, we have an
  // unregistered net user.
  // I removed the trailing asterisk --we7u
  if (strstr(path,"TCPXX") != NULL)
  {
    // "TCPXX" was found in the header.  We have an
    // unregistered user.
    if (log_igate && (debug_level & 1024) )
    {
      xastir_snprintf(temp,
                      sizeof(temp),
                      "NWS IGATE NET->RF(%c):%s\n",
                      third_party ? 'T':'N',
                      line);
      log_data( log_file_path, temp );

      xastir_snprintf(temp,
                      sizeof(temp),
                      "REJECT: Unregistered net user!\n");
      log_data( log_file_path, temp );
      fprintf(stderr, "%s", temp);
    }
    return;
  }

  // no unregistered net user found in string.  Look for NOGATE
  // next.

  if ( strstr(path,"NOGATE") != NULL )
  {
    // "NOGATE" was found in the header.  Don't gate it.
    if (log_igate && (debug_level & 1024) )
    {
      xastir_snprintf(temp,
                      sizeof(temp),
                      "NWS IGATE NET->RF(%c):%s\n",
                      third_party ? 'T':'N',
                      line);
      log_data( log_file_path, temp );

      xastir_snprintf(temp,
                      sizeof(temp),
                      "REJECT: NOGATE found in path!\n");
      log_data( log_file_path, temp );
      fprintf(stderr, "%s", temp);
    }
    return;
  }

  // see if we can gate NWS messages
  if (!filethere(nws_file_path))
  {
    if (log_igate && (debug_level & 1024) )
    {
      xastir_snprintf(temp,
                      sizeof(temp),
                      "NWS IGATE NET->RF(%c):%s\n",
                      third_party ? 'T':'N',
                      line);
      log_data( log_file_path, temp );

      xastir_snprintf(temp,
                      sizeof(temp),
                      "REJECT: No nws-stations.txt file!\n");
      log_data( log_file_path, temp );
      fprintf(stderr, "%s", temp);
    }
    return;
  }

  // check to see if the nws-stations file is newer than last read
  if (last_nws_stations_file_time < file_time(nws_file_path))
  {
    last_nws_stations_file_time = file_time(nws_file_path);
    load_NWS_stations(nws_file_path);
    //fprintf(stderr,"NWS Station file time is old\n");
  }

  // Look for NWS station in file data
  if (!check_NWS_stations(from) || !group_active(from))  // Couldn't find the station
  {

    if (log_igate && (debug_level & 1024) )
    {
      xastir_snprintf(temp,
                      sizeof(temp),
                      "NWS IGATE NET->RF(%c):%s\n",
                      third_party ? 'T':'N',
                      line);
      log_data( log_file_path, temp );

      xastir_snprintf(temp,
                      sizeof(temp),
                      "REJECT: No matching station in nws-stations.txt file!\n");
      log_data( log_file_path, temp );
      fprintf(stderr, "%s", temp);
    }
    return; // Match for station not found in file
  }

  //fprintf(stderr,"SENDING NWS VIA TNC!!!!\n");
  // write data out to interfaces
  for (x=0; x<MAX_IFACE_DEVICES; x++)
  {

//WE7U
    // Check here against "heard" queue for each interface.
    // Drop the packet on the floor if it was already received
    // on this interface within the last XX seconds.  This means
    // that some other igate beat us to the punch.  The receive
    // routines have to fill this queue.  Prune outdated
    // records.
    //
    // Also check here against "sent" queue for each interface.
    // Drop the packet on the floor if already sent within the
    // last XX seconds.  Add this packet to the queue if it
    // isn't already in it.  Prune outdated records.
    if (x != port
        && not_a_dupe(HEARD, x, line, NO_FORCED_INSERT)
        && not_a_dupe( SENT, x, line, NO_FORCED_INSERT) )
    {

      switch (port_data[x].device_type)
      {

        case DEVICE_SERIAL_TNC_AUX_GPS:
        case DEVICE_SERIAL_TNC_HSP_GPS:
        case DEVICE_SERIAL_TNC:
        case DEVICE_AX25_TNC:
        case DEVICE_SERIAL_KISS_TNC:
        case DEVICE_SERIAL_MKISS_TNC:
        case DEVICE_NET_AGWPE:

          begin_critical_section(&devices_lock, "igate.c:output_nws_igate_rf" );

          if (devices[x].igate_options>1
              && port_data[x].status==DEVICE_UP)
          {

            // log traffic for first "up" interface only
            if (log_igate && first)
            {
              xastir_snprintf(temp,
                              sizeof(temp),
                              "NWS IGATE NET->RF(%c):%s\n",
                              third_party ? 'T':'N',
                              line);
              log_data( log_file_path, temp );
              first = 0;
            }

            xastir_snprintf(temp,
                            sizeof(temp),
                            "TRANSMIT: IGate NET->RF packet on device:%d\n",
                            x);

            // log output
            if (log_igate)
            {
              log_data( log_file_path, temp );
            }

            if (debug_level & 1024)
            {
              fprintf(stderr, "%s", temp);
            }

            // ok write this data out to the RF port

            end_critical_section(&devices_lock, "igate.c:output_nws_igate_rf" );

            // First "0" means "cooked"
            // format, last digit: use
            // unproto_igate path
            output_my_data(line,x,0,0,1,NULL);

            begin_critical_section(&devices_lock, "igate.c:output_nws_igate_rf" );

          }
          else
          {
            if (log_igate && (debug_level & 1024) )
            {
              xastir_snprintf(temp,
                              sizeof(temp),
                              "NWS IGATE NET->RF(%c):%s\n",
                              third_party ? 'T':'N',
                              line);
              log_data( log_file_path, temp );

              xastir_snprintf(temp,
                              sizeof(temp),
                              "REJECT: NET->RF on port [%d]!\n",
                              x);
              log_data( log_file_path, temp );
              fprintf(stderr, "%s", temp);
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


