/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
 * $Id$
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2004  The Xastir Group
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


// Used only for special debugging of message/station expiration.
// Leave commented out for normal operation.
//#define EXPIRE_DEBUG


#include "config.h"
#include "snprintf.h"

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <strings.h>
#include <math.h>

#include <Xm/XmAll.h>

#include "xastir.h"
#include "main.h"
#include "draw_symbols.h"
#include "alert.h"
#include "util.h"
#include "bulletin_gui.h"
#include "fcc_data.h"
#include "geo.h"
#include "gps.h"
#include "rac_data.h"
#include "interface.h"
#include "wx.h"
#include "igate.h"
#include "list_gui.h"
#include "track_gui.h"
#include "xa_config.h"
#include "x_spider.h"

#ifdef  WITH_DMALLOC
#include <dmalloc.h>
#endif  // WITH_DMALLOC

#define CHECKMALLOC(m)  if (!m) { fprintf(stderr, "***** Malloc Failed *****\n"); exit(0); }


#define STATION_REMOVE_CYCLE 300    /* check station remove in seconds (every 5 minutes) */
#define MESSAGE_REMOVE_CYCLE 600    /* check message remove in seconds (every 10 minutes) */
#define IN_VIEW_MIN         600l    /* margin for off-screen stations, with possible trails on screen, in minutes */
#define TRAIL_POINT_MARGIN   30l    /* margin for off-screen trails points, for segment to be drawn, in minutes */
#define TRAIL_MAX_SPEED      900    /* max. acceptible speed for drawing trails, in mph */
#define MY_TRAIL_COLOR      0x16    /* trail color index reserved for my station */
#define MY_TRAIL_DIFF_COLOR    0    /* If 0, all my calls (SSIDs) will use one special color (0/1) */
#define TRAIL_ECHO_TIME       30    /* check for delayed echos during last 30 minutes */

// Station Info
Widget  db_station_popup = (Widget)NULL;
char *db_station_info_callsign = NULL;
Pixmap  SiS_icon0, SiS_icon;
Widget  SiS_symb;
Widget  station_list;
Widget  button_store_track;
int station_data_auto_update = 0;

Widget si_text;
Widget db_station_info  = (Widget)NULL;

static xastir_mutex db_station_info_lock;
static xastir_mutex db_station_popup_lock;

void redraw_symbols(Widget w);
int  delete_weather(DataRow *fill);
int  delete_multipoints(DataRow *fill);
void Station_data_destroy_track(Widget widget, XtPointer clientData, XtPointer callData);
void my_station_gps_change(char *pos_long, char *pos_lat, char *course, char *speed, char speedu, char *alt, char *sats);
void station_shortcuts_update_function(int hash_key, DataRow *p_rem);

int  extract_speed_course(char *info, char *speed, char *course);
int  extract_bearing_NRQ(char *info, char *bearing, char *nrq);

int skip_dupe_checking;
int  tracked_stations = 0;       // A count variable used in debug code only
void track_station(Widget w, char *call_tracked, DataRow *p_station);

int  new_message_data;
time_t last_message_remove;     // last time we did a check for message removing

// Save most recent 100 packets in an array called packet_data[]
#define MAX_PACKET_DATA_DISPLAY 100
char packet_data[MAX_PACKET_DATA_DISPLAY][MAX_LINE_SIZE+1];
char packet_data_string[MAX_PACKET_DATA_DISPLAY * (MAX_LINE_SIZE+1)];
int  packet_data_display;   // Last line filled in array (high water mark)
int  redraw_on_new_packet_data;

long stations;                  // number of stored stations
DataRow *n_first;               // pointer to first element in name sorted station list
DataRow *n_last;                // pointer to last  element in name sorted station list
DataRow *t_first;               // pointer to first element in time sorted station list (oldest)
DataRow *t_last;                // pointer to last  element in time sorted station list (newest)
time_t last_station_remove;     // last time we did a check for station removing
time_t last_sec,curr_sec;       // for comparing if seconds in time have changed
int next_time_sn;               // time serial number for unique time index

CADRow *CAD_list_head = NULL;   // pointer to first element in CAD objects list

void draw_trail(Widget w, DataRow *fill, int solid);
void export_trail(DataRow *p_station);

int decoration_offset_x = 0;
int decoration_offset_y = 0;
int last_station_info_x = 0;
int last_station_info_y = 0;
int fcc_lookup_pushed = 0;
int rac_lookup_pushed = 0;

time_t last_object_check = 0;   // Used to determine when to re-transmit objects/items

time_t last_emergency_time = 0;
char last_emergency_callsign[MAX_CALLSIGN+1];
int st_direct_timeout = 60 * 60;        // 60 minutes.

// Used in search_station_name() function.  Shortcuts into the
// station list based on the least-significant 7 bits of the first
// two letters of the callsign/object name.
DataRow *station_shortcuts[16384];





void db_init(void)
{
    init_critical_section( &db_station_info_lock );
    init_critical_section( &db_station_popup_lock );
    last_emergency_callsign[0] = '\0';

    // Seed the random number generator
    srand(1);
}





///////////////////////////////////  Utilities  ////////////////////////////////////////////////////





/*
 *  Check whether callsign is mine.  "exact == 1" checks the SSID
 *  for a match as well.  "exact == 0" checks only the base
 *  callsign.
 */
int is_my_call(char *call, int exact) {
    char *p_del;
    int ok;


    // U.S. special-event callsigns can be as short as three
    // characters, any less and we don't have a valid callsign.
    // We're only doing a fast check on the first two characters
    // though before the string compares, so we only need to check
    // for a terminators in the first position.  If a terminator in
    // the second position, we'll fail the match properly in the
    // string compares.


    // Check first letter.  If a match, go on to strcmp() functions,
    // which are more costly time-wise.  If not, we're done.  This
    // is much faster than the string compares below, so this was
    // purely added for efficiency improvements.
    if (call[0] != my_callsign[0]) {
        return(0);
    }


    // We need this next bit of code so that we don't run off the
    // end of the string when we compare the 2nd char position.
    //
    if (!call[0])   // Empty string
        return(0);

    if (!my_callsign[0])    // Empty string
        return(0);


    // Same for second letter.  This is much faster than the string
    // compares below, so this was purely added for efficiency
    // improvements.
    if (call[1] != my_callsign[1]) {
        return(0);
    }


    if (exact) {
        // We're looking for an exact match
        ok = (int)( !strcmp(call,my_callsign) );
        if (ok) {
            // Ok so far.  Now check that the length of each is the
            // same.
            if (strlen(call) != strlen(my_callsign)) {
                ok = 0; // Not the same!
            }
            else {
//fprintf(stderr,"My exact call found: %s\n",call);
            }
        }
    }

    else {
        // We're looking for a similar match.  Compare only up to
        // the '-' in each (if present).
        int len1,len2;

        p_del = index(call,'-');
        if (p_del == NULL)
            len1 = (int)strlen(call);
        else
            len1 = p_del - call;

        p_del = index(my_callsign,'-');
        if (p_del == NULL)
            len2 = (int)strlen(my_callsign);
        else
            len2 = p_del - my_callsign;

        ok = (int)(len1 == len2 && !strncmp(call,my_callsign,(size_t)len1));
//fprintf(stderr,"My base call found: %s\n",call);
    }
 
    return(ok);
}


        


/*
 *  Change map position if neccessary while tracking a station
 *      we call it with defined station call and position
 */
int is_tracked_station(char *call_sign) {
    int found;
    char call_find[MAX_CALLSIGN+1];
    int ii;
    int call_len;

    if (!track_station_on)
        return(0);

    call_len = 0;
    found = 0;

    if (!track_case) {
        for ( ii = 0; ii < (int)strlen(tracking_station_call); ii++ ) {
            if (isalpha((int)tracking_station_call[ii]))
                call_find[ii] = toupper((int)tracking_station_call[ii]);
            else
                call_find[ii] = tracking_station_call[ii];
        }
        call_find[ii] = '\0';
    } else {
        xastir_snprintf(call_find,
            sizeof(call_find),
            "%s",
            tracking_station_call);
    }

    if (debug_level & 256) {
        fprintf(stderr,"is_tracked_station(): CALL %s %s %s\n",
            tracking_station_call,
            call_find, call_sign);
    }

    if (track_match) {
        if (strcmp(call_find,call_sign) == 0) { // we want an exact match
            found = 1;
        }
    }
    else {
        found = 0;
        call_len = (int)(strlen(call_sign) - strlen(call_find));
        if (strlen(call_find) <= strlen(call_sign)) {
            found = 1;
            for ( ii = 0; ii <= call_len; ii++ ) {
                if (!track_case) {
                    if (!strncasecmp(call_find,call_sign+ii,strlen(call_find)) == 0) {
                        found = 0;  // Found a mis-match
                    }
                }
                else {
                    if (!strncmp(call_find,call_sign+ii,strlen(call_find)) == 0) {
                        found = 0;
                    }
                }
            }
        }
    }
    return(found);
}





/////////////////////////////////////////// Messages ///////////////////////////////////////////

static long *msg_index;
static long msg_index_end;
static long msg_index_max;

static Message *msg_data; // Array containing all messages,
                          // including ones we've transmitted (via
                          // loopback in the code)

time_t last_message_update = 0;
ack_record *ack_list_head = NULL;  // Head of linked list storing most recent ack's
int satellite_ack_mode;


// How often update_messages() will run, in seconds.
// This is necessary because routines like UpdateTime()
// call update_messages() VERY OFTEN.
//
// Actually, we just changed the code around so that we only call
// update_messages() with the force option, and only when we receive a
// message.  message_update_delay is no longer used, and we don't call
// update_messages() from UpdateTime() anymore.
static int message_update_delay = 300;





// Saves latest ack in a linked list.  We need this value in order
// to use Reply/Ack protocol when sending out messages.
void store_most_recent_ack(char *callsign, char *ack) {
    ack_record *p;
    int done = 0;
    char call[MAX_CALLSIGN+1];
    char new_ack[5+1];

    xastir_snprintf(call,
        sizeof(call),
        "%s",
        callsign);
    remove_trailing_spaces(call);

    // Get a copy of "ack".  We might need to change it.
    xastir_snprintf(new_ack,
        sizeof(new_ack),
        "%s",
        ack);

    // If it's more than 2 characters long, we can't use it for
    // Reply/Ack protocol as there's only space enough for two.
    // In this case we need to make sure that we blank out any
    // former ack that was 1 or 2 characters, so that communications
    // doesn't stop.
    if ( strlen(new_ack) > 2 ) {
        // It's too long, blank it out so that gets saved as "",
        // which will overwrite any previously saved ack's that were
        // short enough to use.
        new_ack[0] = '\0';
    }

    // Search for matching callsign through linked list
    p = ack_list_head;
    while ( !done && (p != NULL) ) {
        if (strcasecmp(call,p->callsign) == 0) {
            done++;
        }
        else {
            p = p->next;
        }
    }

    if (done) { // Found it.  Update the ack field.
        //fprintf(stderr,"Found callsign %s on recent ack list, Old:%s, New:%s\n",call,p->ack,new_ack);
        xastir_snprintf(p->ack,sizeof(p->ack),"%s",new_ack);
    }
    else {  // Not found.  Add a new record to the beginning of the
            // list.
        //fprintf(stderr,"New callsign %s, adding to list.  Ack: %s\n",call,new_ack);
        p = (ack_record *)malloc(sizeof(ack_record));
        CHECKMALLOC(p);

        xastir_snprintf(p->callsign,sizeof(p->callsign),"%s",call);
        xastir_snprintf(p->ack,sizeof(p->ack),"%s",new_ack);
        p->next = ack_list_head;
        ack_list_head = p;
    }
}





// Gets latest ack by callsign
char *get_most_recent_ack(char *callsign) {
    ack_record *p;
    int done = 0;
    char call[MAX_CALLSIGN+1];

    xastir_snprintf(call,
        sizeof(call),
        "%s",
        callsign);
    remove_trailing_spaces(call);

    // Search for matching callsign through linked list
    p = ack_list_head;
    while ( !done && (p != NULL) ) {
        if (strcasecmp(call,p->callsign) == 0) {
            done++;
        }
        else {
            p = p->next;
        }
    }

    if (done) { // Found it.  Return pointer to ack string.
        //fprintf(stderr,"Found callsign %s on linked list, returning ack: %s\n",call,p->ack);
        return(&p->ack[0]);
    }
    else {
        //fprintf(stderr,"Callsign %s not found\n",call);
        return(NULL);
    }
}





void init_message_data(void) {  // called at start of main

    new_message_data = 0;
    last_message_remove = sec_now();
}





#ifdef MSG_DEBUG
void msg_clear_data(Message *clear) {
    int size;
    int i;
    unsigned char *data_ptr;

    data_ptr = (unsigned char *)clear;
    size=sizeof(Message);
    for(i=0;i<size;i++)
        *data_ptr++ = 0;
}





void msg_copy_data(Message *to, Message *from) {
    int size;
    int i;
    unsigned char *data_ptr;
    unsigned char *data_ptr_from;

    data_ptr = (unsigned char *)to;
    data_ptr_from = (unsigned char *)from;
    size=sizeof(Message);
    for(i=0;i<size;i++)
        *data_ptr++ = *data_ptr_from++;
}
#endif /* MSG_DEBUG */





// Returns 1 if it's time to update the messages again
int message_update_time (void) {
    if ( sec_now() > (last_message_update + message_update_delay) )
        return(1);
    else
        return(0);
}





int msg_comp_active(const void *a, const void *b) {
    char temp_a[MAX_CALLSIGN+MAX_CALLSIGN+MAX_MESSAGE_ORDER+2];
    char temp_b[MAX_CALLSIGN+MAX_CALLSIGN+MAX_MESSAGE_ORDER+2];

    xastir_snprintf(temp_a, sizeof(temp_a), "%c%s%s%s",
            ((Message*)a)->active, ((Message*)a)->call_sign,
            ((Message*)a)->from_call_sign,
            ((Message*)a)->seq);
    xastir_snprintf(temp_b, sizeof(temp_b), "%c%s%s%s",
            ((Message*)b)->active, ((Message*)b)->call_sign,
            ((Message*)b)->from_call_sign,
            ((Message*)b)->seq);

    return(strcmp(temp_a, temp_b));
}





int msg_comp_data(const void *a, const void *b) {
    char temp_a[MAX_CALLSIGN+MAX_CALLSIGN+MAX_MESSAGE_ORDER+1];
    char temp_b[MAX_CALLSIGN+MAX_CALLSIGN+MAX_MESSAGE_ORDER+1];

    xastir_snprintf(temp_a, sizeof(temp_a), "%s%s%s",
            msg_data[*(long*)a].call_sign, msg_data[*(long *)a].from_call_sign,
            msg_data[*(long *)a].seq);
    xastir_snprintf(temp_b, sizeof(temp_b), "%s%s%s", msg_data[*(long*)b].call_sign,
            msg_data[*(long *)b].from_call_sign, msg_data[*(long *)b].seq);

    return(strcmp(temp_a, temp_b));
}





void msg_input_database(Message *m_fill) {
    void *m_ptr;
    long i;

    if (msg_index_end == msg_index_max) {
        for (i = 0; i < msg_index_end; i++) {

            // Check for a record that is marked RECORD_NOTACTIVE.
            // If found, use that record instead of malloc'ing a new
            // one.
            if (msg_data[msg_index[i]].active == RECORD_NOTACTIVE) {

                // Found an unused record.  Fill it in.
                memcpy(&msg_data[msg_index[i]], m_fill, sizeof(Message));

// Sort msg_data
                qsort(msg_data, (size_t)msg_index_end, sizeof(Message), msg_comp_active);

                for (i = 0; i < msg_index_end; i++) {
                    msg_index[i] = i;
                    if (msg_data[i].active == RECORD_NOTACTIVE) {
                        msg_index_end = i;
                        break;
                    }
                }

// Sort msg_index
                qsort(msg_index, (size_t)msg_index_end, sizeof(long *), msg_comp_data);

                // All done with this message.
                return;
            }
        }

        // Didn't find free message record.  Fetch some more space.
        // Get more msg_data space.
        m_ptr = realloc(msg_data, (msg_index_max+MSG_INCREMENT)*sizeof(Message));
        if (m_ptr) {
            msg_data = m_ptr;

            // Get more msg_index space
            m_ptr = realloc(msg_index, (msg_index_max+MSG_INCREMENT)*sizeof(Message *));
            if (m_ptr) {
                msg_index = m_ptr;
                msg_index_max += MSG_INCREMENT;

//fprintf(stderr, "Max Message Array: %ld\n", msg_index_max);

            } else {
                XtWarning("Unable to allocate more space for message index.\n");
            }
        } else {
            XtWarning("Unable to allocate more space for message database.\n");
        }
    }
    if (msg_index_end < msg_index_max) {
        msg_index[msg_index_end] = msg_index_end;

        // Copy message data into new message record.
        memcpy(&msg_data[msg_index_end++], m_fill, sizeof(Message));

// Sort msg_index
        qsort(msg_index, (size_t)msg_index_end, sizeof(long *), msg_comp_data);
    }
}





// Does a binary search through a sorted message database looking
// for a string match.
//
// If two or more messages match, this routine _should_ return the
// message with the latest timestamp.  This will ensure that earlier
// messages don't get mistaken for current messages, for the case
// where the remote station did a restart and is using the same
// sequence numbers over again.
//
long msg_find_data(Message *m_fill) {
    long record_start, record_mid, record_end, return_record, done;
    char tempfile[MAX_CALLSIGN+MAX_CALLSIGN+MAX_MESSAGE_ORDER+1];
    char tempfill[MAX_CALLSIGN+MAX_CALLSIGN+MAX_MESSAGE_ORDER+1];


    xastir_snprintf(tempfill, sizeof(tempfill), "%s%s%s",
            m_fill->call_sign,
            m_fill->from_call_sign,
            m_fill->seq);

    return_record = -1L;
    if (msg_index && msg_index_end >= 1) {
        /* more than one record */
         record_start=0L;
         record_end = (msg_index_end - 1);
         record_mid=(record_end-record_start)/2;

         done=0;
         while (!done) {

            /* get data for record start */
            xastir_snprintf(tempfile, sizeof(tempfile), "%s%s%s",
                    msg_data[msg_index[record_start]].call_sign,
                    msg_data[msg_index[record_start]].from_call_sign,
                    msg_data[msg_index[record_start]].seq);

            if (strcmp(tempfill, tempfile) < 0) {
                /* filename comes before */
                /*fprintf(stderr,"Before No data found!!\n");*/
                done=1;
                break;
            } else { /* get data for record end */

                xastir_snprintf(tempfile, sizeof(tempfile), "%s%s%s",
                        msg_data[msg_index[record_end]].call_sign,
                        msg_data[msg_index[record_end]].from_call_sign,
                        msg_data[msg_index[record_end]].seq);

                if (strcmp(tempfill,tempfile)>=0) { /* at end or beyond */
                    if (strcmp(tempfill, tempfile) == 0) {
                        return_record = record_end;
//fprintf(stderr,"record %ld",return_record);
                    }

                    done=1;
                    break;
                } else if ((record_mid == record_start) || (record_mid == record_end)) {
                    /* no mid for compare check to see if in the middle */
                    done=1;
                    xastir_snprintf(tempfile, sizeof(tempfile), "%s%s%s",
                            msg_data[msg_index[record_mid]].call_sign,
                            msg_data[msg_index[record_mid]].from_call_sign,
                            msg_data[msg_index[record_mid]].seq);
                    if (strcmp(tempfill,tempfile)==0) {
                        return_record = record_mid;
//fprintf(stderr,"record: %ld",return_record);
                    }
                }
            }
            if (!done) { /* get data for record mid */
                xastir_snprintf(tempfile, sizeof(tempfile), "%s%s%s",
                        msg_data[msg_index[record_mid]].call_sign,
                        msg_data[msg_index[record_mid]].from_call_sign,
                        msg_data[msg_index[record_mid]].seq);

                if (strcmp(tempfill, tempfile) == 0) {
                    return_record = record_mid;
//fprintf(stderr,"record %ld",return_record);
                    done = 1;
                    break;
                }

                if(strcmp(tempfill, tempfile)<0)
                    record_end = record_mid;
                else
                    record_start = record_mid;

                record_mid = record_start+(record_end-record_start)/2;
            }
        }
    }
    return(return_record);
}





void msg_replace_data(Message *m_fill, long record_num) {
    memcpy(&msg_data[msg_index[record_num]], m_fill, sizeof(Message));
}





void msg_get_data(Message *m_fill, long record_num) {
    memcpy(m_fill, &msg_data[msg_index[record_num]], sizeof(Message));
}





void msg_update_ack_stamp(long record_num) {

    //fprintf(stderr,"Attempting to update ack stamp: %ld\n",record_num);
    if ( (record_num >= 0) && (record_num < msg_index_end) ) {
        msg_data[msg_index[record_num]].last_ack_sent = sec_now();
        //fprintf(stderr,"Ack stamp: %ld\n",msg_data[msg_index[record_num]].last_ack_sent);
    }
    //fprintf(stderr,"\n\n\n*** Record: %ld ***\n\n\n",record_num);
}





// Called when we receive an ACK.  Sets the "acked" field in a
// Message which gets rid of the highlighting in the Send Message
// dialog for that line.  This lets us know which messages have
// been acked and which have not.  If timeout is non-zero, then
// set acked to 2:  We use this in update_messages() to flag that
// "*TIMEOUT*" should prefix the string.  If cancelled is non-zero,
// set acked to 3:  We use this in update_messages() to flag that
// "*CANCELLED*" should prefix the string.
//
void msg_record_ack(char *to_call_sign,
                    char *my_call,
                    char *seq,
                    int timeout,
                    int cancel) {
    Message m_fill;
    long record;
    int do_update = 0;

    if (debug_level & 1) {
        fprintf(stderr,"Recording ack for message to: %s, seq: %s\n",
            to_call_sign,
            seq);
    }

    // Find the corresponding message in msg_data[i], set the
    // "acked" field to one.

    substr(m_fill.call_sign, to_call_sign, MAX_CALLSIGN);
    (void)remove_trailing_asterisk(m_fill.call_sign);

    substr(m_fill.from_call_sign, my_call, MAX_CALLSIGN);
    (void)remove_trailing_asterisk(m_fill.from_call_sign);

    substr(m_fill.seq, seq, MAX_MESSAGE_ORDER);
    (void)remove_trailing_spaces(m_fill.seq);
    (void)remove_leading_spaces(m_fill.seq);

    // Look for a message with the same to_call_sign, my_call,
    // and seq number
    record = msg_find_data(&m_fill);
    if(record != -1L) {     // Found a match!
        if (debug_level & 1) {
            fprintf(stderr,"Found in msg db, updating acked field %d -> 1, seq %s, record %ld\n",
                msg_data[msg_index[record]].acked,
                seq,
                record);
        }
        // Only cause an update if this is the first ack.  This
        // reduces dialog "flashing" a great deal
        if ( msg_data[msg_index[record]].acked == 0 ) {

            // Check for my callsign.  If found, update any open message
            // dialogs
            if (is_my_call(msg_data[msg_index[record]].from_call_sign, 1) ) {

                //fprintf(stderr,"From: %s\tTo: %s\n",
                //    msg_data[msg_index[record]].from_call_sign,
                //    msg_data[msg_index[record]].call_sign);

                do_update++;
            }
        }
        else {  // This message has already been acked.
        }

        if (cancel)
            msg_data[msg_index[record]].acked = (char)3;
        else if (timeout)
            msg_data[msg_index[record]].acked = (char)2;
        else
            msg_data[msg_index[record]].acked = (char)1;

        // Set the interval to zero so that we don't display it
        // anymore in the dialog.  Same for tries.
        msg_data[msg_index[record]].interval = 0;
        msg_data[msg_index[record]].tries = 0;

        if (debug_level & 1) {
            fprintf(stderr,"Found in msg db, updating acked field %d -> 1, seq %s, record %ld\n\n",
                msg_data[msg_index[record]].acked,
                seq,
                record);
        }
    }
    else {
        if (debug_level & 1)
            fprintf(stderr,"Matching message not found\n");
    }

    if (do_update) {
        update_messages(1); // Force an update
    }
}





// Called from check_and_transmit_messages().  Updates the interval
// field in our message record for the message currently being
// transmitted.  We'll use this in the Send Message dialog to
// display the current message interval.
//
void msg_record_interval_tries(char *to_call_sign,
                    char *my_call,
                    char *seq,
                    time_t interval,
                    int tries) {
    Message m_fill;
    long record;

    if (debug_level & 1) {
        fprintf(stderr,"Recording interval for message to: %s, seq: %s\n",
            to_call_sign,
            seq);
    }

    // Find the corresponding message in msg_data[i]

    substr(m_fill.call_sign, to_call_sign, MAX_CALLSIGN);
    (void)remove_trailing_asterisk(m_fill.call_sign);

    substr(m_fill.from_call_sign, my_call, MAX_CALLSIGN);
    (void)remove_trailing_asterisk(m_fill.from_call_sign);

    substr(m_fill.seq, seq, MAX_MESSAGE_ORDER);
    (void)remove_trailing_spaces(m_fill.seq);
    (void)remove_leading_spaces(m_fill.seq);

    // Look for a message with the same to_call_sign, my_call,
    // and seq number
    record = msg_find_data(&m_fill);
    if(record != -1L) {     // Found a match!
        if (debug_level & 1) {
            fprintf(stderr,
                "Found in msg db, updating interval field %ld -> 1, seq %s, record %ld\n",
                msg_data[msg_index[record]].interval,
                seq,
                record);
        }

        msg_data[msg_index[record]].interval = interval;
        msg_data[msg_index[record]].tries = tries;
    }
    else {
        if (debug_level & 1)
            fprintf(stderr,"Matching message not found\n");
    }

    update_messages(1); // Force an update
}





// Returns the time_t for last_ack_sent, or 0.
// Also returns the record number found if not passed a NULL pointer
// in record_out or -1L if it's a new record.
time_t msg_data_add(char *call_sign, char *from_call, char *data,
        char *seq, char type, char from, long *record_out) {
    Message m_fill;
    long record;
    char time_data[MAX_TIME];
    int do_msg_update = 0;
    time_t last_ack_sent;
    int distance = -1;
    char temp[10];


    if (debug_level & 1)
        fprintf(stderr,"msg_data_add start\n");

    if ( (data != NULL) && (strlen(data) > MAX_MESSAGE_LENGTH) ) {
        if (debug_level & 2)
            fprintf(stderr,"msg_data_add:  Message length too long\n");

        *record_out = -1L;
        return((time_t)0l);
    }

    substr(m_fill.call_sign, call_sign, MAX_CALLSIGN);
    (void)remove_trailing_asterisk(m_fill.call_sign);

    substr(m_fill.from_call_sign, from_call, MAX_CALLSIGN);
    (void)remove_trailing_asterisk(m_fill.call_sign);

    substr(m_fill.seq, seq, MAX_MESSAGE_ORDER);
    (void)remove_trailing_spaces(m_fill.seq);
    (void)remove_leading_spaces(m_fill.seq);

    // Look for a message with the same call_sign, from_call_sign,
    // and seq number
    record = msg_find_data(&m_fill);
//fprintf(stderr,"RECORD %ld  \n",record);
    msg_clear_data(&m_fill);
    if(record != -1L) { /* fill old data */
        msg_get_data(&m_fill, record);
        last_ack_sent = m_fill.last_ack_sent;
        //fprintf(stderr,"Found: last_ack_sent: %ld\n",m_fill.last_ack_sent);

        //fprintf(stderr,"Found a duplicate message.  Updating fields, seq %s\n",seq);

        // If message is different this time, do an update to the
        // send message window and update the sec_heard field.  The
        // remote station must have restarted and is re-using the
        // sequence numbers.  What a pain!
        if (strcmp(m_fill.message_line,data) != 0) {
            m_fill.sec_heard = sec_now();
            last_ack_sent = (time_t)0;
            //fprintf(stderr,"Message is different this time: Setting last_ack_sent to 0\n");
 
            if (type != MESSAGE_BULLETIN) { // Not a bulletin
                do_msg_update++;
            }
        }

        // If message is the same, but the sec_heard field is quite
        // old (more than 8 hours), the remote station must have
        // restarted, is re-using the sequence numbers, and just
        // happened to send the same message with the same sequence
        // number.  Again, what a pain!  Either that, or we
        // connected to a spigot with a _really_ long queue!
        if (m_fill.sec_heard < (sec_now() - (8 * 60 * 60) )) {
            m_fill.sec_heard = sec_now();
            last_ack_sent = (time_t)0;
            //fprintf(stderr,"Found >8hrs old: Setting last_ack_sent to 0\n");

            if (type != MESSAGE_BULLETIN) { // Not a bulletin
                do_msg_update++;
            }
        }

        // Check for zero time
        if (m_fill.sec_heard == (time_t)0) {
            m_fill.sec_heard = sec_now();
            fprintf(stderr,"Zero time on a previous message.\n");
        }
    }
    else {
        // Only do this if it's a new message.  This keeps things
        // more in sequence by not updating the time stamps
        // constantly on old messages that don't get ack'ed.
        m_fill.sec_heard = sec_now();
        last_ack_sent = (time_t)0;
        //fprintf(stderr,"New msg: Setting last_ack_sent to 0\n");

        if (type != MESSAGE_BULLETIN) { // Not a bulletin
            do_msg_update++;    // Always do an update to the
                                // message window for new messages
        }
    }

    /* FROM */
    m_fill.data_via=from;
    m_fill.active=RECORD_ACTIVE;
    m_fill.type=type;
    if (m_fill.heard_via_tnc != VIA_TNC)
        m_fill.heard_via_tnc = (from == 'T') ? VIA_TNC : NOT_VIA_TNC;

    distance = (int)(distance_from_my_station(from_call,temp) + 0.9999);
 
    if (distance != 0) {    // Have a posit from the sending station
        m_fill.position_known = 1;
        //fprintf(stderr,"Position known: %s\n",from_call);
    }
    else {
        //fprintf(stderr,"Position not known: %s\n",from_call);
    }

    substr(m_fill.call_sign,call_sign,MAX_CALLSIGN);
    (void)remove_trailing_asterisk(m_fill.call_sign);

    substr(m_fill.from_call_sign,from_call,MAX_CALLSIGN);
    (void)remove_trailing_asterisk(m_fill.from_call_sign);

    // Update the message field
    substr(m_fill.message_line,data,MAX_MESSAGE_LENGTH);

    substr(m_fill.seq,seq,MAX_MESSAGE_ORDER);
    (void)remove_trailing_spaces(m_fill.seq);
    (void)remove_leading_spaces(m_fill.seq);

    // Create a timestamp from the current time
    xastir_snprintf(m_fill.packet_time,
        sizeof(m_fill.packet_time),
        "%s",
        get_time(time_data));

    if(record == -1L) {     // No old record found
        m_fill.acked = 0;   // We can't have been acked yet
        m_fill.interval = 0;
        m_fill.tries = 0;

        // We'll be sending an ack right away if this is a new
        // message, so might as well set the time now so that we
        // don't care about failing to set it in
        // msg_update_ack_stamp due to the record number being -1.
        m_fill.last_ack_sent = sec_now();

        msg_input_database(&m_fill);    // Create a new entry
        //fprintf(stderr,"No record found: Setting last_ack_sent to sec_now()00\n");
    }
    else {  // Old record found
        //fprintf(stderr,"Replacing the message in the database, seq %s\n",seq);
        msg_replace_data(&m_fill, record);  // Copy fields from m_fill to record
    }

    /* display messages */
    if (type == MESSAGE_MESSAGE)
        all_messages(from,call_sign,from_call,data);

    // Check for my callsign.  If found, update any open message
    // dialogs
    if (       is_my_call(m_fill.from_call_sign, 1)
            || is_my_call(m_fill.call_sign, 1) ) {

        if (do_msg_update) {
            update_messages(1); // Force an update
        }
    }
 
    if (debug_level & 1)
        fprintf(stderr,"msg_data_add end\n");

    // Return the important variables we'll need
    *record_out = record;
//fprintf(stderr,"\nrecord_out:%ld record %ld\n",*record_out,record);
    return(last_ack_sent);

}   // End of msg_data_add()





// alert_data_add:  Function which adds NWS weather alerts to the
// alert_list.
//
// This function adds alerts directly to the alert_list, bypassing
// the message list and associated message-scan functions.
//
void alert_data_add(char *call_sign, char *from_call, char *data,
        char *seq, char type, char from) {
    Message m_fill;
    char time_data[MAX_TIME];


    if (debug_level & 1)
        fprintf(stderr,"alert_data_add start\n");

    if ( (data != NULL) && (strlen(data) > MAX_MESSAGE_LENGTH) ) {
        if (debug_level & 2)
            fprintf(stderr,"alert_data_add:  Message length too long\n");
        return;
    }

    substr(m_fill.call_sign, call_sign, MAX_CALLSIGN);
    (void)remove_trailing_asterisk(m_fill.call_sign);

    substr(m_fill.from_call_sign, from_call, MAX_CALLSIGN);
    (void)remove_trailing_asterisk(m_fill.call_sign);

    substr(m_fill.seq, seq, MAX_MESSAGE_ORDER);
    (void)remove_trailing_spaces(m_fill.seq);
    (void)remove_leading_spaces(m_fill.seq);

    m_fill.sec_heard = sec_now();

    /* FROM */
    m_fill.data_via=from;
    m_fill.active=RECORD_ACTIVE;
    m_fill.type=type;

    // We don't have a value filled in yet here!
    //if (m_fill.heard_via_tnc != VIA_TNC)
    m_fill.heard_via_tnc = (from == 'T') ? VIA_TNC : NOT_VIA_TNC;

    substr(m_fill.call_sign,call_sign,MAX_CALLSIGN);
    (void)remove_trailing_asterisk(m_fill.call_sign);

    substr(m_fill.from_call_sign,from_call,MAX_CALLSIGN);
    (void)remove_trailing_asterisk(m_fill.from_call_sign);

    // Update the message field
    substr(m_fill.message_line,data,MAX_MESSAGE_LENGTH);

    substr(m_fill.seq,seq,MAX_MESSAGE_ORDER);
    (void)remove_trailing_spaces(m_fill.seq);
    (void)remove_leading_spaces(m_fill.seq);

    // Create a timestamp from the current time
    xastir_snprintf(m_fill.packet_time,
        sizeof(m_fill.packet_time),
        "%s",
        get_time(time_data));

    // Go try to add it to our alert_list.  alert_build_list() will
    // check for duplicates before adding it.

    alert_build_list(&m_fill);

    // This function fills in the Shapefile filename and index
    // so that we can later draw it.
    fill_in_new_alert_entries(da, ALERT_MAP_DIR);

    if (debug_level & 1)
        fprintf(stderr,"alert_data_add end\n");

}   // End of alert_data_add()
 




// What I'd like to do for the following routine:  Use
// XmTextGetInsertionPosition() or XmTextGetCursorPosition() to
// find the last of the text.  Could also save the position for
// each SendMessage window.  Compare the timestamps of messages
// found with the last update time.  If newer, then add them to
// the end.  This should stop the incessant scrolling.

// Another idea, easier method:  Create a buffer.  Snag out the
// messages from the array and sort by time.  Put them into a
// buffer.  Figure out the length of the text widget, and append
// the extra length of the buffer onto the end of the text widget.
// Once the message data is turned into a linked list, it might
// be sorted already by time, so this window will look better
// anyway.

// Calling update_messages with force == 1 will cause an update
// no matter what message_update_time() says.
void update_messages(int force) {
    static XmTextPosition pos;
    char temp1[MAX_CALLSIGN+1];
    char temp2[500];
    char stemp[20];
    long i;
    int mw_p;
    char *temp_ptr;


    if ( message_update_time() || force) {

        //fprintf(stderr,"Um %d\n",(int)sec_now() );

        /* go through all mw_p's! */

        // Perform this for each message window
        for (mw_p=0; msg_index && mw_p < MAX_MESSAGE_WINDOWS; mw_p++) {
            //pos=0;

begin_critical_section(&send_message_dialog_lock, "db.c:update_messages" );

            if (mw[mw_p].send_message_dialog!=NULL/* && mw[mw_p].message_group==1*/) {

//fprintf(stderr,"\n");

                // Clear the text from message window
                XmTextReplace(mw[mw_p].send_message_text,
                    (XmTextPosition) 0,
                    XmTextGetLastPosition(mw[mw_p].send_message_text),
                    "");

                // Snag the callsign you're dealing with from the message dialogue
                if (mw[mw_p].send_message_call_data != NULL) {
                    temp_ptr = XmTextFieldGetString(mw[mw_p].send_message_call_data);
                    xastir_snprintf(temp1,
                        sizeof(temp1),
                        "%s",
                        temp_ptr);
                    XtFree(temp_ptr);

                    new_message_data--;
                    if (new_message_data<0)
                        new_message_data=0;

                    if(strlen(temp1)>0) {   // We got a callsign from the dialog so
                        // create a linked list of the message indexes in time-sorted order

                        typedef struct _index_record {
                            int index;
                            time_t sec_heard;
                            struct _index_record *next;
                        } index_record;
                        index_record *head = NULL;
                        index_record *p_prev = NULL;
                        index_record *p_next = NULL;

                        // Allocate the first record (a dummy record)
                        head = (index_record *)malloc(sizeof(index_record));
                        CHECKMALLOC(head);

                        head->index = -1;
                        head->sec_heard = (time_t)0;
                        head->next = NULL;

                        (void)remove_trailing_spaces(temp1);
                        (void)to_upper(temp1);

                        pos = 0;
                        // Loop through looking for messages to/from that callsign
                        for (i = 0; i < msg_index_end; i++) {
                            if (msg_data[msg_index[i]].active == RECORD_ACTIVE
                                    && (strcmp(temp1, msg_data[msg_index[i]].from_call_sign) == 0
                                        || strcmp(temp1,msg_data[msg_index[i]].call_sign) == 0)
                                    && (is_my_call(msg_data[msg_index[i]].from_call_sign, 1)
                                        || is_my_call(msg_data[msg_index[i]].call_sign, 1)
                                        || mw[mw_p].message_group ) ) {
                                int done = 0;

                                // Message matches our parameters so
                                // save the relevant data about the
                                // message in our linked list.  Compare
                                // the sec_heard field to see whether
                                // we're higher or lower, and insert the
                                // record at the correct spot in the
                                // list.  We end up with a time-sorted
                                // list.
                                p_prev  = head;
                                p_next = p_prev->next;
                                while (!done && (p_next != NULL)) {  // Loop until end of list or record inserted

                                    //fprintf(stderr,"Looping, looking for insertion spot\n");

                                    if (p_next->sec_heard <= msg_data[msg_index[i]].sec_heard) {
                                        // Advance one record
                                        p_prev = p_next;
                                        p_next = p_prev->next;
                                    }
                                    else {  // We found the correct insertion spot
                                        done++;
                                    }
                                }

                                //fprintf(stderr,"Inserting\n");

                                // Add the record in between p_prev and
                                // p_next, even if we're at the end of
                                // the list (in that case p_next will be
                                // NULL.
                                p_prev->next = (index_record *)malloc(sizeof(index_record));
                                CHECKMALLOC(p_prev->next);

                                p_prev->next->next = p_next; // Link to rest of records or NULL
                                p_prev->next->index = i;
                                p_prev->next->sec_heard = msg_data[msg_index[i]].sec_heard;
// Remember to free this entire linked list before exiting the loop for
// this message window!
                            }
                        }
                        // Done processing the entire list for this
                        // message window.

                        //fprintf(stderr,"Done inserting/looping\n");

                        if (head->next != NULL) {   // We have messages to display
                            int done = 0;

                            //fprintf(stderr,"We have messages to display\n");

                            // Run through the linked list and dump the
                            // info out.  It's now in time-sorted order.

// Another optimization would be to keep a count of records added, then
// later when we were dumping it out to the window, only dump the last
// XX records out.

                            p_prev = head->next;    // Skip the first dummy record
                            p_next = p_prev->next;
                            while (!done && (p_prev != NULL)) {  // Loop until end of list
                                int j = p_prev->index;  // Snag the index out of the record
                                char prefix[50];
                                char interval_str[50];
                                int offset = 23;    // Offset for highlighting


                                //fprintf(stderr,"\nLooping through, reading messages\n");
 
//fprintf(stderr,"acked: %d\n",msg_data[msg_index[j]].acked);
 
                                // Message matches so snag the important pieces into a string
                                xastir_snprintf(stemp, sizeof(stemp),
                                    "%c%c/%c%c %c%c:%c%c",
                                    msg_data[msg_index[j]].packet_time[0],
                                    msg_data[msg_index[j]].packet_time[1],
                                    msg_data[msg_index[j]].packet_time[2],
                                    msg_data[msg_index[j]].packet_time[3],
                                    msg_data[msg_index[j]].packet_time[8],
                                    msg_data[msg_index[j]].packet_time[9],
                                    msg_data[msg_index[j]].packet_time[10],
                                    msg_data[msg_index[j]].packet_time[11]
                                );

// Somewhere in here we appear to be losing the first message.  It
// doesn't get written to the window later in the QSO.  Same for
// closing the window and re-opening it, putting the same callsign
// in and pressing "New Call" button.  First message is missing.

                                // Label the message line with who sent it.
                                // If acked = 2 a timeout has occurred
                                // If acked = 3 a cancel has occurred
                                if (msg_data[msg_index[j]].acked == 2) {
                                    xastir_snprintf(prefix,
                                        sizeof(prefix),
                                        "*TIMEOUT* ");
                                }
                                else if (msg_data[msg_index[j]].acked == 3) {
                                    xastir_snprintf(prefix,
                                        sizeof(prefix),
                                        "*CANCELLED* ");
                                }
                                else prefix[0] = '\0';

                                if (msg_data[msg_index[j]].interval) {
                                    xastir_snprintf(interval_str,
                                        sizeof(interval_str),
                                        ">%d @ %ldsecs",
                                        msg_data[msg_index[j]].tries + 1,
                                        msg_data[msg_index[j]].interval);

                                    // Don't highlight the interval
                                    // value
                                    offset = offset + strlen(interval_str);
                                }
                                else {
                                    interval_str[0] = '\0';
                                }

                                xastir_snprintf(temp2, sizeof(temp2),
                                    "%s  %-9s%s>%s%s\n",
                                    // Debug code.  Trying to find sorting error
                                    //"%ld  %s  %-9s>%s\n",
                                    //msg_data[msg_index[j]].sec_heard,
                                    stemp,
                                    msg_data[msg_index[j]].from_call_sign,
                                    interval_str,
                                    prefix,
                                    msg_data[msg_index[j]].message_line);

//fprintf(stderr,"update_message: %s|%s", temp1, temp2);
 
                                if (debug_level & 2) fprintf(stderr,"update_message: %s|%s\n", temp1, temp2);
                                // Replace the text from pos to pos+strlen(temp2) by the string "temp2"
                                if (mw[mw_p].send_message_text != NULL) {

                                    // Insert the text at the end
//                                    XmTextReplace(mw[mw_p].send_message_text,
//                                            pos,
//                                            pos+strlen(temp2),
//                                            temp2);

                                    XmTextInsert(mw[mw_p].send_message_text,
                                            pos,
                                            temp2);
 
                                    // Set highlighting based on the "acked" field
//fprintf(stderr,"acked: %d\t",msg_data[msg_index[j]].acked);
                                    if ( (msg_data[msg_index[j]].acked == 0)    // Not acked yet
                                            && ( is_my_call(msg_data[msg_index[j]].from_call_sign, 1)) ) {
//fprintf(stderr,"Setting underline\t");
                                        XmTextSetHighlight(mw[mw_p].send_message_text,
                                            pos+offset,
                                            pos+strlen(temp2),
                                            //XmHIGHLIGHT_SECONDARY_SELECTED); // Underlining
                                            XmHIGHLIGHT_SELECTED);         // Reverse Video
                                    }
                                    else {  // Message was acked, get rid of highlighting
//fprintf(stderr,"Setting normal\t");
                                        XmTextSetHighlight(mw[mw_p].send_message_text,
                                            pos+offset,
                                            pos+strlen(temp2),
                                            XmHIGHLIGHT_NORMAL);
                                    }

//fprintf(stderr,"Text: %s\n",temp2); 

                                    pos += strlen(temp2);

                                }

                                // Advance to the next record in the list
                                p_prev = p_next;
                                if (p_next != NULL)
                                    p_next = p_prev->next;

                            }   // End of while
                        }   // End of if
                        else {  // No messages matched, list is empty
                        }

// What does this do?  Move all of the text?
//                        if (pos > 0) {
//                            if (mw[mw_p].send_message_text != NULL) {
//                                XmTextReplace(mw[mw_p].send_message_text,
//                                        --pos,
//                                        XmTextGetLastPosition(mw[mw_p].send_message_text),
//                                        "");
//                            }
//                        }

                        //fprintf(stderr,"Free'ing list\n");

                        // De-allocate the linked list
                        p_prev = head;
                        while (p_prev != NULL) {

                            //fprintf(stderr,"You're free!\n");

                            p_next = p_prev->next;
                            free(p_prev);
                            p_prev = p_next;
                        }

                        // Show the last added message in the window
                        XmTextShowPosition(mw[mw_p].send_message_text,
                            pos);
                    }
                }
            }

end_critical_section(&send_message_dialog_lock, "db.c:update_messages" );

        }
        last_message_update = sec_now();

//fprintf(stderr,"Message index end: %ld\n",msg_index_end);
 
    }
}





void mdelete_messages_from(char *from) {
    long i;

    // Mark message records with RECORD_NOTACTIVE.  This will mark
    // them for re-use.
    for (i = 0; msg_index && i < msg_index_end; i++)
        if (strcmp(msg_data[i].call_sign, my_callsign) == 0 && strcmp(msg_data[i].from_call_sign, from) == 0)
            msg_data[i].active = RECORD_NOTACTIVE;
}





void mdelete_messages_to(char *to) {
    long i;

    // Mark message records with RECORD_NOTACTIVE.  This will mark
    // them for re-use.
    for (i = 0; msg_index && i < msg_index_end; i++)
        if (strcmp(msg_data[i].call_sign, to) == 0)
            msg_data[i].active = RECORD_NOTACTIVE;
}





void mdelete_messages(char *to_from) {
    long i;

    // Mark message records with RECORD_NOTACTIVE.  This will mark
    // them for re-use.
    for (i = 0; msg_index && i < msg_index_end; i++)
        if (strcmp(msg_data[i].call_sign, to_from) == 0 || strcmp(msg_data[i].from_call_sign, to_from) == 0)
            msg_data[i].active = RECORD_NOTACTIVE;
}





void mdata_delete_type(const char msg_type, const time_t reference_time) {
    long i;

    // Mark message records with RECORD_NOTACTIVE.  This will mark
    // them for re-use.
    for (i = 0; msg_index && i < msg_index_end; i++)

        if ((msg_type == '\0' || msg_type == msg_data[i].type)
                && msg_data[i].active == RECORD_ACTIVE
                && msg_data[i].sec_heard < reference_time)

            msg_data[i].active = RECORD_NOTACTIVE;
}





void check_message_remove(void) {       // called in timing loop

    // Time to check for old messages again?  (Currently every ten
    // minutes)
#ifdef EXPIRE_DEBUG
    if ( last_message_remove < (sec_now() - 15) ) {
#else // EXPIRE_DEBUG
    if ( last_message_remove < (sec_now() - MESSAGE_REMOVE_CYCLE) ) {
#endif

        // Yes it is.  Mark all messages that are older than
        // sec_remove with the RECORD_NOTACTIVE flag.  This will
        // mark them for re-use.
#ifdef EXPIRE_DEBUG
        mdata_delete_type('\0', sec_now()-15);
#else   // EXPIRE_DEBUG
        mdata_delete_type('\0', sec_now()-sec_remove);
#endif

        last_message_remove = sec_now();
    }

    // Should we sort them at this point so that the unused ones are
    // near the end?  It looks like the message input functions do
    // this, so I guess we don't need to do it here.
}





void mscan_file(char msg_type, void (*function)(Message *)) {
    long i;

    for (i = 0; msg_index && i < msg_index_end; i++)
        if ((msg_type == '\0' || msg_type == msg_data[msg_index[i]].type) &&
                msg_data[msg_index[i]].active == RECORD_ACTIVE)
            function(&msg_data[msg_index[i]]);
}





void mprint_record(Message *m_fill) {
    fprintf(stderr,"%-9s>%-9s seq:%5s type:%c :%s\n", m_fill->from_call_sign, m_fill->call_sign,
        m_fill->seq, m_fill->type, m_fill->message_line);
}





void mdisplay_file(char msg_type) {
    fprintf(stderr,"\n\n");
    mscan_file(msg_type, mprint_record);
    fprintf(stderr,"\tmsg_index_end %ld, msg_index_max %ld\n", msg_index_end, msg_index_max);
}





/////////////////////////////////////// Station Data ///////////////////////////////////////////





void pad_callsign(char *callsignout, char *callsignin) {
    int i,l;

    l=(int)strlen(callsignin);
    for(i=0; i<9;i++) {
        if(i<l) {
            if(isalnum((int)callsignin[i]) || callsignin[i]=='-') {
                callsignout[i]=callsignin[i];
            }
            else {
                callsignout[i] = ' ';
            }
        }
        else {
            callsignout[i] = ' ';
        }
    }
    callsignout[i] = '\0';
}





// Check for valid overlay characters:  'A-Z', '0-9', and 'a-j'.  If
// 'a-j', it's from a compressed posit, and we need to convert it to
// '0-9'.
void overlay_symbol(char symbol, char data, DataRow *fill) {

    if ( data != '/' && data !='\\') {  // Symbol overlay

        if (data >= 'a' && data <= 'j') {
            // Found a compressed posit numerical overlay
            data = data - 'a';  // Convert to a digit
        }
        else if ( (data >= '0' && data <= '9')
                || (data >= 'A' && data <= 'Z') ) {
            // Found normal overlay character
            fill->aprs_symbol.aprs_type = '\\';
            fill->aprs_symbol.special_overlay = data;
        }
        else {
            // Bad overlay character.  Don't use it.  Insert the
            // normal alternate table character instead.
            fill->aprs_symbol.aprs_type = '\\';
            fill->aprs_symbol.special_overlay='\0';
        }
    } else {    // No overlay character
        fill->aprs_symbol.aprs_type = data;
        fill->aprs_symbol.special_overlay='\0';
    }
    fill->aprs_symbol.aprs_symbol = symbol;
}





APRS_Symbol *id_callsign(char *call_sign, char * to_call) {
    char *ptr;
    char *id = "/aUfbYX's><OjRkv";
    char hold[MAX_CALLSIGN+1];
    int index;
    static APRS_Symbol symbol;

    symbol.aprs_symbol = '/';
    symbol.special_overlay = '\0';
    symbol.aprs_type ='/';
    ptr=strchr(call_sign,'-');
    if(ptr!=NULL)                      /* get symbol from SSID */
        if((index=atoi(ptr+1))<= 15)
            symbol.aprs_symbol = id[index];

    if (strncmp(to_call, "GPS", 3) == 0 || strncmp(to_call, "SPC", 3) == 0 || strncmp(to_call, "SYM", 3) == 0) {
        substr(hold, to_call+3, 3);
        if ((ptr = strpbrk(hold, "->,")) != NULL)
            *ptr = '\0';

        if (strlen(hold) >= 2) {
            switch (hold[0]) {
                case 'A':
                    symbol.aprs_type = '\\';

                case 'P':
                    if (('0' <= hold[1] && hold[1] <= '9') || ('A' <= hold[1] && hold[1] <= 'Z'))
                        symbol.aprs_symbol = hold[1];

                    break;

                case 'O':
                    symbol.aprs_type = '\\';

                case 'B':
                    switch (hold[1]) {
                        case 'B':
                            symbol.aprs_symbol = '!';
                            break;
                        case 'C':
                            symbol.aprs_symbol = '"';
                            break;
                        case 'D':
                            symbol.aprs_symbol = '#';
                            break;
                        case 'E':
                            symbol.aprs_symbol = '$';
                            break;
                        case 'F':
                            symbol.aprs_symbol = '%';
                            break;
                        case 'G':
                            symbol.aprs_symbol = '&';
                            break;
                        case 'H':
                            symbol.aprs_symbol = '\'';
                            break;
                        case 'I':
                            symbol.aprs_symbol = '(';
                            break;
                        case 'J':
                            symbol.aprs_symbol = ')';
                            break;
                        case 'K':
                            symbol.aprs_symbol = '*';
                            break;
                        case 'L':
                            symbol.aprs_symbol = '+';
                            break;
                        case 'M':
                            symbol.aprs_symbol = ',';
                            break;
                        case 'N':
                            symbol.aprs_symbol = '-';
                            break;
                        case 'O':
                            symbol.aprs_symbol = '.';
                            break;
                        case 'P':
                            symbol.aprs_symbol = '/';
                            break;
                    }
                    break;

                case 'D':
                    symbol.aprs_type = '\\';

                case 'H':
                    switch (hold[1]) {
                        case 'S':
                            symbol.aprs_symbol = '[';
                            break;
                        case 'T':
                            symbol.aprs_symbol = '\\';
                            break;
                        case 'U':
                            symbol.aprs_symbol = ']';
                            break;
                        case 'V':
                            symbol.aprs_symbol = '^';
                            break;
                        case 'W':
                            symbol.aprs_symbol = '_';
                            break;
                        case 'X':
                            symbol.aprs_symbol = '`';
                            break;
                    }
                    break;

                case 'N':
                    symbol.aprs_type = '\\';

                case 'M':
                    switch (hold[1]) {
                        case 'R':
                            symbol.aprs_symbol = ':';
                            break;
                        case 'S':
                            symbol.aprs_symbol = ';';
                            break;
                        case 'T':
                            symbol.aprs_symbol = '<';
                            break;
                        case 'U':
                            symbol.aprs_symbol = '=';
                            break;
                        case 'V':
                            symbol.aprs_symbol = '>';
                            break;
                        case 'W':
                            symbol.aprs_symbol = '?';
                            break;
                        case 'X':
                            symbol.aprs_symbol = '@';
                            break;
                    }
                    break;

                case 'Q':
                    symbol.aprs_type = '\\';

                case 'J':
                    switch (hold[1]) {
                        case '1':
                            symbol.aprs_symbol = '{';
                            break;
                        case '2':
                            symbol.aprs_symbol = '|';
                            break;
                        case '3':
                            symbol.aprs_symbol = '}';
                            break;
                        case '4':
                            symbol.aprs_symbol = '~';
                            break;
                    }
                    break;

                case 'S':
                    symbol.aprs_type = '\\';

                case 'L':
                    if ('A' <= hold[1] && hold[1] <= 'Z')
                        symbol.aprs_symbol = tolower((int)hold[1]);

                    break;
            }
            if (hold[2]) {
                if (hold[2] >= 'a' && hold[2] <= 'j') {
                    // Compressed mode numeric overlay
                    symbol.special_overlay = hold[2] - 'a';
                }
                else if ( (hold[2] >= '0' && hold[2] <= '9')
                        || (hold[2] >= 'A' && hold[2] <= 'Z') ) {
                    // Normal overlay character
                    symbol.special_overlay = hold[2];
                }
                else {
                    // Bad overlay character found
                    symbol.special_overlay = '\0';
                }
            }
            else {
                // No overlay character found
                symbol.special_overlay = '\0';
            }
        }
    }
    return(&symbol);
}





/******************************** Sort begin *************************** ****/





void  clear_sort_file(char *filename) {
    char ptr_filename[400];

    xastir_snprintf(ptr_filename, sizeof(ptr_filename), "%s-ptr", filename);
    (void)unlink(filename);
    (void)unlink(ptr_filename);
}





void sort_reset_pointers(FILE *pointer,long new_data_ptr,long records, int type, long start_ptr) {
    long cp;
    long temp[13000];
    long buffn,start_buffn;
    long cp_records;
    long max_buffer;
    int my_size;

    my_size=(int)sizeof(new_data_ptr);
    max_buffer=13000l;
    if(type==0) {
        /* before start_ptr */
        /* copy back pointers */
        cp=start_ptr;
        for(buffn=records; buffn > start_ptr; buffn-=max_buffer) {
            start_buffn=buffn-max_buffer;
            if(start_buffn<start_ptr)
                start_buffn=start_ptr;

            cp_records=buffn-start_buffn;
            (void)fseek(pointer,(my_size*start_buffn),SEEK_SET);
            if(fread(&temp,(my_size*cp_records),1,pointer)==1) {
                (void)fseek(pointer,(my_size*(start_buffn+1)),SEEK_SET);
                (void)fwrite(&temp,(my_size*cp_records),1,pointer);
            }
        }
        /* copy new pointer in */
        (void)fseek(pointer,(my_size*start_ptr),SEEK_SET);
        (void)fwrite(&new_data_ptr,(size_t)my_size,1,pointer);
    }
}





long sort_input_database(char *filename, char *fill, int size) {
    FILE *my_data;
    FILE *pointer;
    char file_data[2000];

    char ptr_filename[400];

    char tempfile[2000];
    char tempfill[2000];

    int ptr_size;
    long data_ptr;
    long new_data_ptr;
    long return_records;
    long records;
    long record_start;
    long record_end;
    long record_mid;
    int done;

    ptr_size=(int)sizeof(new_data_ptr);
    xastir_snprintf(ptr_filename, sizeof(ptr_filename), "%s-ptr", filename);
    /* get first string to sort on */
    (void)sscanf(fill,"%1999s",tempfill);

    data_ptr=0l;
    my_data=NULL;
    return_records=0l;
    pointer = fopen(ptr_filename,"r+");
    /* check if file is there */
    if(pointer == NULL)
      pointer = fopen(ptr_filename,"a+");

    if(pointer!=NULL) {
        my_data = fopen(filename,"a+");
        if(my_data!=NULL) {

            // Next statement needed for Solaris 7, as the fopen above
            // doesn't put the filepointer at the end of the file.
            (void) fseek(my_data,0l,SEEK_END);  //KD6ZWR

            new_data_ptr = data_ptr = ftell(my_data);
            (void)fwrite(fill,(size_t)size,1,my_data);
            records = (data_ptr/size);
            return_records=records+1;
            if(records<1) {
                /* no data yet */
                (void)fseek(pointer,0l,SEEK_SET);
                (void)fwrite(&data_ptr,(size_t)ptr_size,1,pointer);
            } else {
                /* more than one record*/
                (void)fseek(pointer,(ptr_size*records),SEEK_SET);
                (void)fwrite(&data_ptr,(size_t)ptr_size,1,pointer);
                record_start=0l;
                record_end=records;
                record_mid=(record_end-record_start)/2;
                done=0;
                while(!done) {
                    /*fprintf(stderr,"Records Start %ld, Mid %ld, End %ld\n",record_start,record_mid,record_end);*/
                    /* get data for record start */
                    (void)fseek(pointer,(ptr_size*record_start),SEEK_SET);
                    (void)fread(&data_ptr,(size_t)ptr_size,1,pointer);
                    (void)fseek(my_data,data_ptr,SEEK_SET);
                    if(fread(file_data,(size_t)size,1,my_data)==1) {
                        /* COMPARE HERE */
                        (void)sscanf(file_data,"%1999s",tempfile);
                        if(strcasecmp(tempfill,tempfile)<0) {
                            /* file name comes before */
                            /*fprintf(stderr,"END - Before start\n");*/
                            done=1;
                            /* now place pointer before start*/
                            sort_reset_pointers(pointer,new_data_ptr,records,0,record_start);
                        } else {
                            /* get data for record end */
                            (void)fseek(pointer,(ptr_size*record_end),SEEK_SET);
                            (void)fread(&data_ptr,(size_t)ptr_size,1,pointer);
                            (void)fseek(my_data,data_ptr,SEEK_SET);
                            if(fread(file_data,(size_t)size,1,my_data)==1) {
                                /* COMPARE HERE */
                                (void)sscanf(file_data,"%1999s",tempfile);
                                if(strcasecmp(tempfill,tempfile)>0) {
                                    /* file name comes after */
                                    /*fprintf(stderr,"END - After end\n");*/
                                    done=1;
                                    /* now place pointer after end */
                                } else {
                                    if((record_mid==record_start) || (record_mid==record_end)) {
                                        /* no mid for compare check to see if in the middle */
                                        /*fprintf(stderr,"END - NO Middle\n");*/
                                        done=1;
                                        /* now place pointer before start*/
                                        if (record_mid==record_start)
                                            sort_reset_pointers(pointer,new_data_ptr,records,0,record_mid+1);
                                        else
                                            sort_reset_pointers(pointer,new_data_ptr,records,0,record_mid-1);
                                    } else {
                                        /* get data for record mid */
                                        (void)fseek(pointer,(ptr_size*record_mid),SEEK_SET);
                                        (void)fread(&data_ptr,(size_t)ptr_size,1,pointer);
                                        (void)fseek(my_data,data_ptr,SEEK_SET);
                                        if(fread(file_data,(size_t)size,1,my_data)==1) {
                                            /* COMPARE HERE */
                                            (void)sscanf(file_data,"%1999s",tempfile);
                                            if(strcasecmp(tempfill,tempfile)<0) {
                                                /* checking comes before */
                                                /*record_start=0l;*/
                                                record_end=record_mid;
                                                record_mid=record_start+(record_end-record_start)/2;
                                                /*fprintf(stderr,"TOP %ld, mid %ld\n",record_mid,record_end);*/
                                            } else {
                                                /* checking comes after*/
                                                record_start=record_mid;
                                                /*record_end=end*/
                                                record_mid=record_start+(record_end-record_start)/2;
                                                /*fprintf(stderr,"BOTTOM start %ld, mid %ld\n",record_start,record_mid);*/
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        } else
            fprintf(stderr,"Could not open file %s\n",filename);
    } else
        fprintf(stderr,"Could not open file %s\n",filename);

    if(my_data!=NULL)
        (void)fclose(my_data);

    if(pointer!=NULL)
        (void)fclose(pointer);

    return(return_records);
}





/******************** sort end **********************/



// is_altnet()
//
// Returns true if station fits the current altnet description.
//
int is_altnet(DataRow *p_station) {
    char temp_altnet_call[20+1];
    char temp2[20+1];
    char *net_ptr;
    int  altnet_match;
    int  result;


    // Snag a possible altnet call out of the record for later use
    if (p_station->node_path_ptr != NULL)
        substr(temp_altnet_call, p_station->node_path_ptr, MAX_CALLSIGN);
    else
        temp_altnet_call[0] = '\0';

    // Save for later
    xastir_snprintf(temp2,
        sizeof(temp2),
        "%s",
        temp_altnet_call);

    if ((net_ptr = strchr(temp_altnet_call, ',')))
        *net_ptr = '\0';    // Chop the string at the first ',' character

    for (altnet_match = (int)strlen(altnet_call); altnet && altnet_call[altnet_match-1] == '*'; altnet_match--);

    result = (!strncmp(temp_altnet_call, altnet_call, (size_t)altnet_match)
                 || !strcmp(temp_altnet_call, "local")
                 || !strncmp(temp_altnet_call, "SPC", 3)
                 || !strcmp(temp_altnet_call, "SPECL")
                 || is_my_call(p_station->call_sign,1));

    if ( (debug_level & 1) && result )
        fprintf(stderr,"%s  %-9s  %s\n", altnet_call, temp_altnet_call, temp2 );

    return(result);
}





// Function which checks various filtering criteria (the Select struct)
// and decides whether this station/object should be displayed.
//
// 0 = don't draw this station/object
// 1 = ok to draw this station/object
//
int ok_to_draw_station(DataRow *p_station) {
    // Check overall flag
    if (Select_.none)
        return 0;

    // Check tactical flag
    if (Select_.tactical
            && p_station->tactical_call_sign == NULL)
        return 0;

    // Check for my station and my objects/items
    if (strcmp(p_station->call_sign, my_callsign) == 0
        || (is_my_call(p_station->origin, 1)        // If station is owned by me
            && (   p_station->flag & ST_OBJECT      // And it's an object
                || p_station->flag & ST_ITEM) ) ) { // or an item
        if (!Select_.mine)
            return 0;
    }
    // Not mine, so check these next things
    else {
        // Check whether we wish to display TNC heard stations
        if (p_station->flag & ST_VIATNC) {
            if (!Select_.tnc)
                return 0;

            // Check whether we wish to display directly heard stations
            if (p_station->flag & ST_DIRECT
                    && sec_now() < (p_station->direct_heard + st_direct_timeout)) {
                if (!Select_.direct)
                    return 0;
            }
            // Check whether we wish to display stations heard via a digi
            else {
                if (!Select_.via_digi)
                    return 0;
            }
        }
        // Check whether we wish to display net stations
        else {
            if (!Select_.net)
                return 0;
        }

        // Check if we want to display data past the clear time
        if (!Select_.old_data) {
            if ((p_station->sec_heard + sec_clear) < sec_now())
                return 0;
        }
    }


    // Check whether object or item
    if (p_station->flag & (ST_OBJECT | ST_ITEM)) {
        // Check whether we wish to display objects/items
        if (!Select_.objects ||
            (!Select_.weather_objects && !Select_.gauge_objects && !Select_.other_objects))
            return 0;

        // Check if WX info and we wish to see it
        if (p_station->weather_data) {
            return Select_.weather_objects;
        }
        // Check if water gage and we wish to see it
        else if (p_station->aprs_symbol.aprs_type == '/'
                 && p_station->aprs_symbol.aprs_symbol == 'w') {
            return Select_.gauge_objects;
        }
        // Check if we wish to see other objects/items
        else {
            return Select_.other_objects;
        }
    }
    else {    // Not an object or item
        if (!Select_.stations ||
            (!Select_.fixed_stations && !Select_.moving_stations && !Select_.weather_stations))
            return 0;

        // Check if we wish to see weather stations
        if (p_station->weather_data) {
            // We have weather data

            // Check whether it is a citizen's weather station
            if (strncasecmp(p_station->call_sign,"CW",2) == 0) {
                return(Select_.weather_stations && Select_.CWOP_wx_stations);
            }
            else {
                return Select_.weather_stations;
            }
        }
        // Check if we wish to see other stations
        else {
            if (p_station->flag & ST_MOVING) {
                return Select_.moving_stations;
            }
            else {
                return Select_.fixed_stations;
            }
        }
    }
}





// display_station
//
// single is 1 if the calling station wants to update only a
// single station.  If updating multiple stations in a row, then
// "single" will be passed to us as a zero.
//
// If current course/speed/altitude are absent, we check the last
// track point to try to snag those numbers.
//
void display_station(Widget w, DataRow *p_station, int single) {
    char temp_altitude[20];
    char temp_course[20];
    char temp_speed[20];
    char dr_speed[20];
    char temp_call[MAX_TACTICAL_CALL+1];
    char wx_tm[50];
    char temp_wx_temp[30];
    char temp_wx_wind[40];
    char temp_my_distance[20];
    char temp_my_course[20];
    char temp1_my_course[20];
    time_t temp_sec_heard;
    int temp_show_last_heard;
    long l_lon, l_lat;
    char orient;
    float value;
    char tmp[7+1];
    int speed_ok = 0;
    int course_ok = 0;
    int wx_ghost = 0;
    Pixmap drawing_target;
    WeatherRow *weather = p_station->weather_data;


    if (debug_level & 128)
        fprintf(stderr,"Display station (%s) called for Single=%d.\n", p_station->call_sign, single);

    if (!ok_to_draw_station(p_station))
        return;

    // Set up call string for display
    if (Display_.callsign) {
        if (p_station->tactical_call_sign) {
            // Display tactical callsign instead if it has one
            // defined.
            xastir_snprintf(temp_call,
                sizeof(temp_call),
                "%s",
                p_station->tactical_call_sign);
        }
        else {
            // Display normal callsign.
            xastir_snprintf(temp_call,
                sizeof(temp_call),
                "%s",
                p_station->call_sign);
        }
    }
    else {
        temp_call[0] = '\0';
    }

    // Set up altitude string for display
    temp_altitude[0] = '\0';

    if (Display_.altitude) {
        // Check whether we have altitude in the current data
        if (strlen(p_station->altitude)>0) {
            // Found it in the current data
            xastir_snprintf(temp_altitude, sizeof(temp_altitude), "%.0f%s",
                atof(p_station->altitude) * cvt_m2len, un_alt);
        }

        // Else check whether the previous position had altitude.
        // Note that newest_trackpoint if it exists should be the
        // same as the current data, so we have to go back one
        // further trackpoint.
        else if ( (p_station->newest_trackpoint != NULL)
                && (p_station->newest_trackpoint->prev != NULL) ) {
            if ( p_station->newest_trackpoint->prev->altitude > -99999l) {
                // Found it in the tracklog
                xastir_snprintf(temp_altitude, sizeof(temp_altitude), "%.0f%s",
                    (float)(p_station->newest_trackpoint->prev->altitude * cvt_dm2len),
                    un_alt);

//                fprintf(stderr,"Trail data              with altitude: %s : %s\n",
//                    p_station->call_sign,
//                    temp_altitude);
            }
            else {
                //fprintf(stderr,"Trail data w/o altitude                %s\n",
                //    p_station->call_sign);
            }
        }
    }

    // Set up speed and course strings for display
    temp_speed[0] = '\0';
    dr_speed[0] = '\0';
    temp_course[0] = '\0';

    if (Display_.speed || Display_.dr_data) {
        // don't display 'fixed' stations speed and course.
        // Check whether we have speed in the current data and it's
        // >= 0.
        if ( (strlen(p_station->speed)>0) && (atof(p_station->speed) >= 0) ) {
            speed_ok++;
            xastir_snprintf(tmp,
                sizeof(tmp),
                "%s",
                un_spd);
            if (Display_.speed_short)
                tmp[0] = '\0';          // without unit

            xastir_snprintf(temp_speed, sizeof(temp_speed), "%.0f%s",
                            atof(p_station->speed)*cvt_kn2len,tmp);
        }
        // Else check whether the previous position had speed
        // Note that newest_trackpoint if it exists should be the
        // same as the current data, so we have to go back one
        // further trackpoint.
        else if ( (p_station->newest_trackpoint != NULL)
                  && (p_station->newest_trackpoint->prev != NULL) ) {

            xastir_snprintf(tmp,
                sizeof(tmp),
                "%s",
                un_spd);

            if (Display_.speed_short)
                tmp[0] = '\0';          // without unit

            if ( p_station->newest_trackpoint->prev->speed > 0) {
                speed_ok++;

                xastir_snprintf(temp_speed, sizeof(temp_speed), "%.0f%s",
                                p_station->newest_trackpoint->prev->speed * cvt_hm2len,
                                tmp);
            }
        }
    }

    if (Display_.course || Display_.dr_data) {
        // Check whether we have course in the current data
        if ( (strlen(p_station->course)>0) && (atof(p_station->course) > 0) ) {
            course_ok++;
            xastir_snprintf(temp_course, sizeof(temp_course), "%.0f°",
                            atof(p_station->course));
        }
        // Else check whether the previous position had a course
        // Note that newest_trackpoint if it exists should be the
        // same as the current data, so we have to go back one
        // further trackpoint.
        else if ( (p_station->newest_trackpoint != NULL)
                  && (p_station->newest_trackpoint->prev != NULL) ) {
            if( p_station->newest_trackpoint->prev->course > 0 ) {
                course_ok++;
                xastir_snprintf(temp_course, sizeof(temp_course), "%.0f°",
                                (float)p_station->newest_trackpoint->prev->course);
            }
        }
    }

    // Save the speed into the dr string for later
    xastir_snprintf(dr_speed,
        sizeof(dr_speed),
        "%s",
        temp_speed);

    if (!speed_ok  || !Display_.speed)
        temp_speed[0] = '\0';

    if (!course_ok || !Display_.course)
        temp_course[0] = '\0';

    // Set up distance and bearing strings for display
    temp_my_distance[0] = '\0';
    temp_my_course[0] = '\0';

    if (Display_.dist_bearing && strcmp(p_station->call_sign,my_callsign) != 0) {
        l_lat = convert_lat_s2l(my_lat);
        l_lon = convert_lon_s2l(my_long);

        // Get distance in nautical miles, convert to current measurement standard
        value = cvt_kn2len * calc_distance_course(l_lat,l_lon,
                p_station->coord_lat,p_station->coord_lon,temp1_my_course,sizeof(temp1_my_course));

        if (value < 5.0)
            sprintf(temp_my_distance,"%0.1f%s",value,un_dst);
        else
            sprintf(temp_my_distance,"%0.0f%s",value,un_dst);

        xastir_snprintf(temp_my_course, sizeof(temp_my_course), "%.0f°",
                atof(temp1_my_course));
    }

    // Set up weather strings for display
    temp_wx_temp[0] = '\0';
    temp_wx_wind[0] = '\0';

    if (weather != NULL) {
        // wx_ghost = 1 if the weather data is too old to display
        wx_ghost = (int)(((sec_old + weather->wx_sec_time)) < sec_now());
    }

    if (Display_.weather
            && Display_.weather_text
            && weather != NULL      // We have weather data
            && !wx_ghost) {         // Weather is current, display it

        if (strlen(weather->wx_temp) > 0) {
            xastir_snprintf(tmp,
                sizeof(tmp),
                "T:");
            if (Display_.temperature_only)
                tmp[0] = '\0';

            if (english_units)
                xastir_snprintf(temp_wx_temp, sizeof(temp_wx_temp), "%s%.0f°F ",
                                tmp, atof(weather->wx_temp));
            else
                xastir_snprintf(temp_wx_temp, sizeof(temp_wx_temp), "%s%.0f°C ",
                                tmp,((atof(weather->wx_temp)-32.0)*5.0)/9.0);
        }

        if (!Display_.temperature_only) {
            if (strlen(weather->wx_hum) > 0) {
                xastir_snprintf(wx_tm, sizeof(wx_tm), "H:%.0f%%", atof(weather->wx_hum));
                strncat(temp_wx_temp,
                    wx_tm,
                    sizeof(temp_wx_temp) - strlen(temp_wx_temp));
            }

            if (strlen(weather->wx_speed) > 0) {
                xastir_snprintf(temp_wx_wind, sizeof(temp_wx_wind), "S:%.0f%s ",
                                atof(weather->wx_speed)*cvt_mi2len,un_spd);
            }

            if (strlen(weather->wx_gust) > 0) {
                xastir_snprintf(wx_tm, sizeof(wx_tm), "G:%.0f%s ",
                                atof(weather->wx_gust)*cvt_mi2len,un_spd);
                strncat(temp_wx_wind,
                    wx_tm,
                    sizeof(temp_wx_wind) - strlen(temp_wx_wind));
            }

            if (strlen(weather->wx_course) > 0) {
                xastir_snprintf(wx_tm, sizeof(wx_tm), "C:%.0f°", atof(weather->wx_course));
                strncat(temp_wx_wind,
                    wx_tm,
                    sizeof(temp_wx_wind) - strlen(temp_wx_wind));
            }

            if (temp_wx_wind[strlen(temp_wx_wind)-1] == ' ') {
                temp_wx_wind[strlen(temp_wx_wind)-1] = '\0';  // delete blank at EOL
            }
        }

        if (temp_wx_temp[strlen(temp_wx_temp)-1] == ' ')
            temp_wx_temp[strlen(temp_wx_temp)-1] = '\0';  // delete blank at EOL
    }


    (void)remove_trailing_asterisk(p_station->call_sign);  // DK7IN: is this needed here?

    if (Display_.symbol_rotate)
        orient = symbol_orient(p_station->course);   // rotate symbol
    else
        orient = ' ';

    // Prevents my own call from "ghosting"?
    temp_sec_heard = (strcmp(p_station->call_sign, my_callsign) == 0) ? sec_now(): p_station->sec_heard;


    // Check whether it's a locally-owned object/item
    if ( (is_my_call(p_station->origin,1))                  // If station is owned by me
            && ( ((p_station->flag & ST_OBJECT) != 0)       // And it's an object
              || ((p_station->flag & ST_ITEM  ) != 0) ) ) { // or an item
        temp_sec_heard = sec_now(); // We don't want our own objects/items to "ghost"
    }

    // Show last heard times only for others stations and their
    // objects/items.
    temp_show_last_heard = (strcmp(p_station->call_sign, my_callsign) == 0) ? 0 : Display_.last_heard;


//------------------------------------------------------------------------------------------

    // If we're only planning on updating a single station at this time, we go
    // through the drawing calls twice, the first time drawing directly onto
    // the screen.
    if (!pending_ID_message && single)
        drawing_target = XtWindow(da);
    else
        drawing_target = pixmap_final;

_do_the_drawing:
    // Check whether it's a locally-owned object/item
    if ( (is_my_call(p_station->origin,1))                  // If station is owned by me
            && ( ((p_station->flag & ST_OBJECT) != 0)       // And it's an object
              || ((p_station->flag & ST_ITEM  ) != 0) ) ) { // or an item
        temp_sec_heard = sec_now(); // We don't want our own objects/items to "ghost"
        // This isn't quite right since if it's a moving object, passing an incorrect
        // sec_heard should give the wrong results.
    }

    if (Display_.ambiguity && p_station->pos_amb)
        draw_ambiguity(p_station->coord_lon, p_station->coord_lat,
                       p_station->pos_amb,temp_sec_heard,drawing_target);

    // Check for DF'ing data, draw DF circles if present and enabled
    if (Display_.df_data && strlen(p_station->signal_gain) == 7) {  // There's an SHGD defined
        //fprintf(stderr,"SHGD:%s\n",p_station->signal_gain);
        draw_DF_circle(p_station->coord_lon,
                       p_station->coord_lat,
                       p_station->signal_gain,
                       temp_sec_heard,
                       drawing_target);
    }

    // Check for DF'ing beam heading/NRQ data
    if (Display_.df_data && (strlen(p_station->bearing) == 3) && (strlen(p_station->NRQ) == 3)) {
        //fprintf(stderr,"Bearing: %s\n",p_station->signal_gain,NRQ);
        if (p_station->df_color == -1)
            p_station->df_color = rand() % 32;

        draw_bearing(p_station->coord_lon,
                     p_station->coord_lat,
                     p_station->course,
                     p_station->bearing,
                     p_station->NRQ,
                     trail_colors[p_station->df_color],
                     temp_sec_heard,
                     drawing_target);
    }

    // Check whether to draw dead-reckoning data by KJ5O
    if (Display_.dr_data
        && ( (p_station->flag & ST_MOVING)
//        && (p_station->newest_trackpoint!=0
             && course_ok
             && speed_ok
             && atof(dr_speed) > 0) ) {
        if ( (sec_now()-temp_sec_heard) < dead_reckoning_timeout ) {
            draw_deadreckoning_features(p_station,
                                        drawing_target,
                                        w);
        }
    }

    if (p_station->aprs_symbol.area_object.type != AREA_NONE)
        draw_area(p_station->coord_lon, p_station->coord_lat,
                  p_station->aprs_symbol.area_object.type,
                  p_station->aprs_symbol.area_object.color,
                  p_station->aprs_symbol.area_object.sqrt_lat_off,
                  p_station->aprs_symbol.area_object.sqrt_lon_off,
                  p_station->aprs_symbol.area_object.corridor_width,
                  temp_sec_heard,
                  drawing_target);


    // Draw additional stuff if this is the tracked station
    if (is_tracked_station(p_station->call_sign)) {
//WE7U
        draw_pod_circle(p_station->coord_lon,
                        p_station->coord_lat,
                        0.0020 * scale_y,
                        colors[0x0e],   // Yellow
                        drawing_target);
        draw_pod_circle(p_station->coord_lon,
                        p_station->coord_lat,
                        0.0023 * scale_y,
                        colors[0x44],   // Red
                        drawing_target);
        draw_pod_circle(p_station->coord_lon,
                        p_station->coord_lat,
                        0.0026 * scale_y,
                        colors[0x61],   // Blue
                        drawing_target);
    }


    // Draw additional stuff if this is a storm and the weather data
    // is not too old to display.
    if ( (weather != NULL) && weather->wx_storm && !wx_ghost ) {
        char temp[4];


        //fprintf(stderr,"Plotting a storm symbol:%s:%s:%s:\n",
        //    weather->wx_hurricane_radius,
        //    weather->wx_trop_storm_radius,
        //    weather->wx_whole_gale_radius);

// Still need to draw the circles in different colors for the
// different ranges.  Might be nice to tint it as well.

        xastir_snprintf(temp,
            sizeof(temp),
            "%s",
            weather->wx_hurricane_radius);

        if ( (temp[0] != '\0') && (strncmp(temp,"000",3) != 0) ) {

            draw_pod_circle(p_station->coord_lon,
                            p_station->coord_lat,
                            atof(temp) * 1.15078, // nautical miles to miles
                            colors[0x44],   // Red
                            drawing_target);
        }

        xastir_snprintf(temp,
            sizeof(temp),
            "%s",
            weather->wx_trop_storm_radius);

        if ( (temp[0] != '\0') && (strncmp(temp,"000",3) != 0) ) {
            draw_pod_circle(p_station->coord_lon,
                            p_station->coord_lat,
                            atof(temp) * 1.15078, // nautical miles to miles
                            colors[0x0e],   // Yellow
                            drawing_target);
        }

        xastir_snprintf(temp,
            sizeof(temp),
            "%s",
            weather->wx_whole_gale_radius);

        if ( (temp[0] != '\0') && (strncmp(temp,"000",3) != 0) ) {
            draw_pod_circle(p_station->coord_lon,
                            p_station->coord_lat,
                            atof(temp) * 1.15078, // nautical miles to miles
                            colors[0x0a],   // Green
                            drawing_target);
        }
    }


    // Draw wind barb if selected and we have wind, but not a severe
    // storm (wind barbs just confuse the matter).
    if (Display_.weather && Display_.wind_barb
            && weather != NULL && atoi(weather->wx_speed) >= 5
            && !weather->wx_storm
            && !wx_ghost ) {
        draw_wind_barb(p_station->coord_lon,
                       p_station->coord_lat,
                       weather->wx_speed,
                       weather->wx_course,
                       temp_sec_heard,
                       drawing_target);
    }


    draw_symbol(w,
                p_station->aprs_symbol.aprs_type,
                p_station->aprs_symbol.aprs_symbol,
                p_station->aprs_symbol.special_overlay,
                p_station->coord_lon,
                p_station->coord_lat,
                temp_call,
                temp_altitude,
                temp_course,    // ??
                temp_speed,     // ??
                temp_my_distance,
                temp_my_course,
// Display only if wx temp is current
                (wx_ghost) ? "" : temp_wx_temp,
// Display only if if wind speed is current
                (wx_ghost) ? "" : temp_wx_wind,
                temp_sec_heard,
                temp_show_last_heard,
                drawing_target,
                orient,
                p_station->aprs_symbol.area_object.type,
                p_station->signpost,
                p_station->probability_min,
                p_station->probability_max);


    // Draw other points associated with the station, if any.
    // KG4NBB
    if (p_station->num_multipoints != 0) {
        draw_multipoints(p_station->coord_lon, p_station->coord_lat,
                         p_station->num_multipoints,
                         p_station->multipoint_data->multipoints,
                         p_station->type, p_station->style,
                         temp_sec_heard,
                         drawing_target);
    }

    temp_sec_heard = p_station->sec_heard;    // DK7IN: ???

    if (Display_.phg
        && (!(p_station->flag & ST_MOVING) || Display_.phg_of_moving)) {

        // Check for Map View "eyeball" symbol
        if ( strncmp(p_station->power_gain,"RNG",3) == 0
                && p_station->aprs_symbol.aprs_type == '/'
                && p_station->aprs_symbol.aprs_symbol == 'E' ) {
            // Map View "eyeball" symbol.  Don't draw the RNG ring
            // for it.
        }
        else if (strlen(p_station->power_gain) == 7) {
            // Station has PHG or RNG defined
            //
            draw_phg_rng(p_station->coord_lon,p_station->coord_lat,
                         p_station->power_gain,temp_sec_heard,drawing_target);
        }
        else if (Display_.default_phg && !(p_station->flag & (ST_OBJECT|ST_ITEM))) {
            // No PHG defined and not an object/item.  Display a PHG
            // of 3130 as default as specified in the spec:  9W, 3dB
            // omni at 20 feet = 6.2 mile PHG radius.
            //
            draw_phg_rng(p_station->coord_lon,p_station->coord_lat,
                         "PHG3130",temp_sec_heard,drawing_target);
        }
    }


    // Now if we just did the single drawing, we want to go back and draw
    // the same things onto pixmap_final so that when we do update from it
    // to the screen all of the stuff will be there.
    if (drawing_target == XtWindow(da)) {
        drawing_target = pixmap_final;
        goto _do_the_drawing;
    }
}





// draw line relativ
void draw_test_line(Widget w, long x, long y, long dx, long dy, long ofs) {

    x += screen_width  - 10 - ofs;
    y += screen_height - 10;
    (void)XDrawLine(XtDisplay(w),pixmap_final,gc,x,y,x+dx,y+dy);
}





// draw text
void draw_ruler_text(Widget w, char * text, long ofs) {
    int x,y;
    int len;

    len = (int)strlen(text);
    x = screen_width  - 10 - ofs / 2;
    y = screen_height - 10;
    x -= len * 3;
    y -= 3;
    draw_nice_string(w,pixmap_final,0,x,y,text,0x10,0x20,len);
}





// Compute Range Scale in miles or kilometers.
//
// For this we need to figure out x-distance and y-distance across
// the screen.  Take the smaller of the two, then figure out which
// power of 2 miles fits from the center to the edge of the screen.
// "For metric, use the nearest whole number kilometer in powers of
// two of 1.5 km above the 1 mile scale.  At 1 mile and below, do
// the conversion to meters where 1 mi is equal to 1600m..." (Bob
// Bruninga's words).
void draw_range_scale(Widget w) {
    Dimension width, height;
    long x, x0, y, y0;
    double x_miles_km, y_miles_km, distance;
    char temp_course[10];
    long temp;
    double temp2;
    long range;
    int small_flag = 0;
    int x_screen, y_screen;
    int len;
    char text[80];


    // Find out the screen values
    XtVaGetValues(da,XmNwidth, &width, XmNheight, &height, NULL);

    // Convert points to Xastir coordinate system

    // X
    x = mid_x_long_offset  - ((width *scale_x)/2);
    x0 = mid_x_long_offset; // Center of screen

    // Y
    y = mid_y_lat_offset   - ((height*scale_y)/2);
    y0 = mid_y_lat_offset;  // Center of screen

    // Compute distance from center to each edge

    // X distance.  Keep Y constant.
    x_miles_km = cvt_kn2len * calc_distance_course(y0,x0,y0,x,temp_course,sizeof(temp_course));
 
    // Y distance.  Keep X constant.
    y_miles_km = cvt_kn2len * calc_distance_course(y0,x0,y,x0,temp_course,sizeof(temp_course));

    // Choose the smaller distance
    if (x_miles_km < y_miles_km) {
        distance = x_miles_km;
    }
    else {
        distance = y_miles_km;
    }

    // Convert it to nearest power of two that fits inside

    if (english_units) { // English units
        if (distance >= 1.0) {
            // Shift it right until it is less than 2.
            temp = (long)distance;
            range = 1;
            while (temp >= 2) {
                temp = temp / 2;
                range = range * 2;
            }
        }
        else {  // Distance is less than one
            // divide 1.0 by 2 until distance is greater
            small_flag++;
            temp2 = 1.0;
            range = 1;
            while (temp2 > distance) {
                //fprintf(stderr,"temp2: %f,  distance: %f\n", temp2, distance);
                temp2 = temp2 / 2.0;
                range = range * 2;
            }
        }
    }
    else {  // Metric units
        if (distance >= 12800.0)
            range = 12800;
        else if (distance >= 6400.0)
            range = 6400;
        else if (distance >= 3200.0)
            range = 3200;
        else if (distance >= 1600.0)
            range = 1600;
        else if (distance >= 800.0)
            range = 800;
        else if (distance >= 400.0)
            range = 400;
        else if (distance >= 200.0)
            range = 200;
        else if (distance >= 100.0)
            range = 100;
        else if (distance >= 50.0)
            range = 50;
        else if (distance >= 25.0)
            range = 25;
        else if (distance >= 12.0)
            range = 12;
        else if (distance >= 6.0)
            range = 6;
        else if (distance >= 3.0)
            range = 3;
        else {
            small_flag++;
            if (distance >= 1.6)
                range = 1600;
            else if (distance >= 0.8)
                range = 800;
            else if (distance >= 0.4)
                range = 400;
            else if (distance >= 0.2)
                range = 200;
            else if (distance >= 0.1)
                range = 100;
            else if (distance >= 0.05)
                range = 50;
            else if (distance >= 0.025)
                range = 25;
            else range = 12;
        }
    }

    //fprintf(stderr,"Distance: %f\t", distance);
    //fprintf(stderr,"Range: %ld\n", range);

    if (english_units) { // English units
        if (small_flag) {
            xastir_snprintf(text,sizeof(text),"RANGE SCALE 1/%ld mi", range);
        }
        else {
            xastir_snprintf(text,sizeof(text),"RANGE SCALE %ld mi", range);
        }
    }
    else {  // Metric units
        if (small_flag) {
            xastir_snprintf(text,sizeof(text),"RANGE SCALE %ld m", range);
        }
        else {
            xastir_snprintf(text,sizeof(text),"RANGE SCALE %ld km", range);
        }
    }

    // Draw it on the screen
    len = (int)strlen(text);
    x_screen = 10;
    y_screen = screen_height - 5;
    draw_nice_string(w,pixmap_final,0,x_screen,y_screen,text,0x10,0x20,len);
}





/*
 *  Calculate and draw ruler on right bottom of screen
 */
void draw_ruler(Widget w) {
    int ruler_pix;      // min size of ruler in pixel
    char unit[5+1];     // units
    char text[20];      // ruler text
    double ruler_siz;   // len of ruler in meters etc.
    int mag;
    int i;
    int dx, dy;

    ruler_pix = (int)(screen_width / 9);        // ruler size (in pixels)
    ruler_siz = ruler_pix * scale_x * calc_dscale_x(mid_x_long_offset,mid_y_lat_offset); // size in meter

    if(english_units) {
        if (ruler_siz > 1609.3/2) {
            xastir_snprintf(unit,
                sizeof(unit),
                "mi");
            ruler_siz /= 1609.3;
        } else {
            xastir_snprintf(unit,
                sizeof(unit),
                "ft");
            ruler_siz /= 0.3048;
        }
    } else {
        xastir_snprintf(unit,
            sizeof(unit),
            "m");
        if (ruler_siz > 1000/2) {
            xastir_snprintf(unit,
                sizeof(unit),
                "km");
            ruler_siz /= 1000.0;
        }
    }

    mag = 1;
    while (ruler_siz > 5.0) {             // get magnitude
        ruler_siz /= 10.0;
        mag *= 10;
    }
    // select best value and adjust ruler length
    if (ruler_siz > 2.0) {
        ruler_pix = (int)(ruler_pix * 5.0 / ruler_siz +0.5);
        ruler_siz = 5.0 * mag;
    } else {
        if (ruler_siz > 1.0) {
            ruler_pix = (int)(ruler_pix * 2.0 / ruler_siz +0.5);
            ruler_siz = 2.0 * mag;
        } else {
            ruler_pix = (int)(ruler_pix * 1.0 / ruler_siz +0.5);
            ruler_siz = 1.0 * mag;
        }
    }
    xastir_snprintf(text, sizeof(text), "%.0f %s",ruler_siz,unit);      // Set up string
    //fprintf(stderr,"Ruler: %s, %d\n",text,ruler_pix);

    (void)XSetLineAttributes(XtDisplay(w),gc,1,LineSolid,CapRound,JoinRound);
    (void)XSetForeground(XtDisplay(w),gc,colors[0x20]);         // white
    for (i = 8; i >= 0; i--) {
        dx = (((i / 3)+1) % 3)-1;         // looks complicated...
        dy = (((i % 3)+1) % 3)-1;         // I want 0 / 0 as last entry

        if (i == 0)
            (void)XSetForeground(XtDisplay(w),gc,colors[0x10]);         // black

        draw_test_line(w,dx,dy,          ruler_pix,0,ruler_pix);        // hor line
        draw_test_line(w,dx,dy,              0,5,    ruler_pix);        // ver left
        draw_test_line(w,dx+ruler_pix,dy,    0,5,    ruler_pix);        // ver right
        if (text[0] == '2')
            draw_test_line(w,dx+0.5*ruler_pix,dy,0,3,ruler_pix);        // ver middle

        if (text[0] == '5') {
            draw_test_line(w,dx+0.2*ruler_pix,dy,0,3,ruler_pix);        // ver middle
            draw_test_line(w,dx+0.4*ruler_pix,dy,0,3,ruler_pix);        // ver middle
            draw_test_line(w,dx+0.6*ruler_pix,dy,0,3,ruler_pix);        // ver middle
            draw_test_line(w,dx+0.8*ruler_pix,dy,0,3,ruler_pix);        // ver middle
        }
    }

    draw_ruler_text(w,text,ruler_pix);

    draw_range_scale(w);
}





/*
 *  Display all stations on screen (trail, symbol, info text)
 */
void display_file(Widget w) {
    DataRow *p_station;         // pointer to station data
    time_t temp_sec_heard;      // time last heard
    time_t t_clr, t_old;

    if(debug_level & 1)
        fprintf(stderr,"Display File Start\n");

// Draw probability of detection circle, if enabled
//draw_pod_circle(64000000l, 32400000l, 10, colors[0x44], pixmap_final);

    t_old = sec_now() - sec_old;        // precalc compare times
    t_clr = sec_now() - sec_clear;
    temp_sec_heard = 0l;
    p_station = t_first;                // start with oldest station, have newest on top at t_last
    while (p_station != NULL) {
        if (debug_level & 64) {
            fprintf(stderr,"display_file: Examining %s\n", p_station->call_sign);
        }
        if ((p_station->flag & ST_ACTIVE) != 0) {       // ignore deleted objects

            temp_sec_heard = (is_my_call(p_station->call_sign,1))? sec_now(): p_station->sec_heard;

            // Check for my objects/items as well
            if ( (is_my_call(p_station->origin, 1)        // If station is owned by me
                    && (   p_station->flag & ST_OBJECT      // And it's an object
                        || p_station->flag & ST_ITEM) ) ) { // or an item
                temp_sec_heard = sec_now();
            }
 
            if ((p_station->flag & ST_INVIEW) != 0) {  // skip far away stations...
                // we make better use of the In View flag in the future
                if ( !altnet || is_altnet(p_station) ) {
                    if (debug_level & 256) {
                        fprintf(stderr,"display_file:  Inview, check for trail\n");
                    }
                    if (Display_.trail && p_station->newest_trackpoint != NULL) {
                        // ????????????   what is the difference? :
                        if (debug_level & 256) {
                            fprintf(stderr,"%s:    Trails on and have track data\n",
                                    "display_file");
                        }
                        if (temp_sec_heard > t_clr) {
                            // Not too old, so draw trail
                            if (temp_sec_heard > t_old) {
                                // New trail, so draw solid trail
                                if (debug_level & 256) {
                                    fprintf(stderr,"Drawing Solid trail for %s, secs old: %ld\n",
                                        p_station->call_sign,
                                        sec_now() - temp_sec_heard);
                                }
                                draw_trail(w,p_station,1);
                            }
                            else {
                                if (debug_level & 256) {
                                    fprintf(stderr,"Drawing trail for %s, secs old: %ld\n",
                                        p_station->call_sign,
                                        sec_now() - temp_sec_heard);
                                }
                                draw_trail(w,p_station,0);
                            }
                        }
                        else if (debug_level & 256) {
                            fprintf(stderr,"Station too old\n");
                        }
                    }
                    else if (debug_level & 256) {
                        fprintf(stderr,"Station trails %d, track data %x\n",
                            Display_.trail, (int)p_station->newest_trackpoint);
                    }
                    if (debug_level & 256)
                        fprintf(stderr,"calling display_station()\n");

                    display_station(w,p_station,0);
                }
                else if (debug_level & 64) {
                    fprintf(stderr,"display_file: Station %s skipped altnet\n",
                        p_station->call_sign);
                }
            }
            else if (debug_level & 256) {
                    fprintf(stderr,"display_file: Station outside viewport\n");
            }
        }
        else if (debug_level & 64) {
            fprintf(stderr,"display_file: ignored deleted %s\n", p_station->call_sign);
        }
        p_station = p_station->t_next;  // next station
    }

    draw_ruler(w);

    Draw_All_CAD_Objects(w);        // Draw all CAD objects, duh.

    // Check if we should mark where we found an address
    if (mark_destination && show_destination_mark) {
        int offset;

        // Set the line width in the GC.  Make it nice and fat.
        (void)XSetLineAttributes (XtDisplay (w), gc_tint, 7, LineSolid, CapButt,JoinMiter);
        (void)XSetForeground (XtDisplay (w), gc_tint, colors[0x27]);
        (void)(void)XSetFunction (XtDisplay (da), gc_tint, GXxor);

        // Scale it so that the 'X' stays the same size at all zoom
        // levels.
        offset = 25 * scale_y;

        // Make a big 'X'
        draw_vector(w,
            destination_coord_lon-offset,  // x1
            destination_coord_lat-offset,  // y1
            destination_coord_lon+offset,  // x2
            destination_coord_lat+offset,  // y2
            gc_tint,
            pixmap_final);

        draw_vector(w,
            destination_coord_lon+offset,  // x1
            destination_coord_lat-offset,  // y1
            destination_coord_lon-offset,  // x2
            destination_coord_lat+offset,  // y2
            gc_tint,
            pixmap_final);
    }

    if (debug_level & 1)
        fprintf(stderr,"Display File Stop\n");
}





//////////////////////////////  Station Info  /////////////////////////////////////





/*
 *  Delete Station Info PopUp
 */
void Station_data_destroy_shell(/*@unused@*/ Widget widget, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    Widget shell = (Widget) clientData;
    XtPopdown(shell);

begin_critical_section(&db_station_info_lock, "db.c:Station_data_destroy_shell" );

    XtDestroyWidget(shell);
    db_station_info = (Widget)NULL;

end_critical_section(&db_station_info_lock, "db.c:Station_data_destroy_shell" );

}





/*
 *  Store track data for current station
 */
void Station_data_store_track(Widget w, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    DataRow *p_station = clientData;

    //busy_cursor(XtParent(w));
    busy_cursor(appshell);

    // Grey-out button so it doesn't get pressed twice
    XtSetSensitive(button_store_track,FALSE);

    // Store trail to file
    export_trail(p_station);

#ifdef HAVE_LIBSHP
    // Save trail as a Shapefile map
    create_map_from_trail(p_station->call_sign);
#endif  // HAVE_LIBSHP
}





/*
 *  Delete tracklog for current station
 */
void Station_data_destroy_track( /*@unused@*/ Widget widget, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    DataRow *p_station = clientData;

    if (delete_trail(p_station))
        redraw_on_new_data = 2;         // redraw immediately
}





// This function merely reformats the button callback in order to
// call wx_alert_double_click_action, which expects the paramter in
// calldata instead of in clientData.
//
void Station_data_wx_alert(Widget w, XtPointer clientData, /*@unused@*/ XtPointer calldata) {
//fprintf(stderr, "Station_data_wx_alert start\n");
    wx_alert_finger_output( w, clientData);
//fprintf(stderr, "Station_data_wx_alert end\n");
}





void Station_data_add_fcc(Widget w, XtPointer clientData, /*@unused@*/ XtPointer calldata) {
    char temp[500];
    FccAppl my_data;
    char *station = (char *) clientData;

    (void)check_fcc_data();
    //busy_cursor(XtParent(w));
    busy_cursor(appshell);
    if (search_fcc_data_appl(station, &my_data)==1) {
        /*fprintf(stderr,"FCC call %s\n",station);*/
        xastir_snprintf(temp, sizeof(temp), "%s\n%s %s\n%s %s %s\n%s %s, %s %s, %s %s\n\n",
            langcode("STIFCC0001"),
            langcode("STIFCC0003"),my_data.name_licensee,
            langcode("STIFCC0004"),my_data.text_street,my_data.text_pobox,
            langcode("STIFCC0005"),my_data.city,
            langcode("STIFCC0006"),my_data.state,
            langcode("STIFCC0007"),my_data.zipcode);
            XmTextInsert(si_text,0,temp);
            XmTextShowPosition(si_text,0);

        fcc_lookup_pushed = 1;
    }
}





void Station_data_add_rac(Widget w, XtPointer clientData, /*@unused@*/ XtPointer calldata) {
    char temp[512];
    char club[512];
    rac_record my_data;
    char *station = (char *) clientData;

    xastir_snprintf(temp,
        sizeof(temp),
        " ");
    (void)check_rac_data();
    //busy_cursor(XtParent(w));
    busy_cursor(appshell);
    if (search_rac_data(station, &my_data)==1) {
        /*fprintf(stderr,"IC call %s\n",station);*/
        xastir_snprintf(temp, sizeof(temp), "%s\n%s %s\n%s\n%s, %s\n%s\n",
                langcode("STIFCC0002"),my_data.first_name,my_data.last_name,my_data.address,
                my_data.city,my_data.province,my_data.postal_code);

        if (my_data.qual_a[0] == 'A')
            strncat(temp,
                langcode("STIFCC0008"),
                sizeof(temp) - strlen(temp));

        if (my_data.qual_d[0] == 'D')
            strncat(temp,
                langcode("STIFCC0009"),
                sizeof(temp) - strlen(temp));

        if (my_data.qual_b[0] == 'B' && my_data.qual_c[0] != 'C')
            strncat(temp,
                langcode("STIFCC0010"),
                sizeof(temp) - strlen(temp));

        if (my_data.qual_c[0] == 'C')
            strncat(temp,
                langcode("STIFCC0011"),
                sizeof(temp) - strlen(temp));

        strncat(temp,
            "\n",
            sizeof(temp) - strlen(temp));

        if (strlen(my_data.club_name) > 1){
            xastir_snprintf(club, sizeof(club), "%s\n%s\n%s, %s\n%s\n",
                    my_data.club_name, my_data.club_address,
                    my_data.club_city, my_data.club_province, my_data.club_postal_code);
            strncat(temp,
                club,
                sizeof(temp) - strlen(temp));
        }
        strncat(temp,
            "\n",
            sizeof(temp) - strlen(temp));
        XmTextInsert(si_text,0,temp);
        XmTextShowPosition(si_text,0);

        rac_lookup_pushed = 1;
    }
}





void Station_query_trace(/*@unused@*/ Widget w, XtPointer clientData, /*@unused@*/ XtPointer calldata) {
    char *station = (char *) clientData;
    char temp[50];
    char call[25];

    pad_callsign(call,station);
    xastir_snprintf(temp, sizeof(temp), ":%s:?APRST", call);

// WE7U: It'd be nice to return via the reverse path here
    transmit_message_data(station,temp,NULL);
}





void Station_query_messages(/*@unused@*/ Widget w, XtPointer clientData, /*@unused@*/ XtPointer calldata) {
    char *station = (char *) clientData;
    char temp[50];
    char call[25];

    pad_callsign(call,station);
    xastir_snprintf(temp, sizeof(temp), ":%s:?APRSM", call);

// WE7U: It'd be nice to return via the reverse path here
    transmit_message_data(station,temp,NULL);
}





void Station_query_direct(/*@unused@*/ Widget w, XtPointer clientData, /*@unused@*/ XtPointer calldata) {
    char *station = (char *) clientData;
    char temp[50];
    char call[25];

    pad_callsign(call,station);
    xastir_snprintf(temp, sizeof(temp), ":%s:?APRSD", call);

// WE7U:It'd be nice to return via the reverse path here
    transmit_message_data(station,temp,NULL);
}





void Station_query_version(/*@unused@*/ Widget w, XtPointer clientData, /*@unused@*/ XtPointer calldata) {
    char *station = (char *) clientData;
    char temp[50];
    char call[25];

    pad_callsign(call,station);
    xastir_snprintf(temp, sizeof(temp), ":%s:?VER", call);

// WE7U:It'd be nice to return via the reverse path here
    transmit_message_data(station,temp,NULL);
}





void General_query(/*@unused@*/ Widget w, XtPointer clientData, /*@unused@*/ XtPointer calldata) {
    char *location = (char *) clientData;
    char temp[50];

    xastir_snprintf(temp, sizeof(temp), "?APRS?%s", location);
    output_my_data(temp,-1,0,0,0,NULL);  // Not igating
}





void IGate_query(/*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer calldata) {
    output_my_data("?IGATE?",-1,0,0,0,NULL); // Not igating
}





void WX_query(/*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer calldata) {
    output_my_data("?WX?",-1,0,0,0,NULL);    // Not igating
}





/*
 *  Change data for current object
 *  If calldata = 2, we're doing a move object operation.  Pass the
 *  value on through to Set_Del_Object.
 */
void Modify_object( Widget w, XtPointer clientData, XtPointer calldata) {
    DataRow *p_station = clientData;

    //if (calldata != NULL)
    //    fprintf(stderr,"Modify_object:  calldata:  %s\n", (char *)calldata);

    //fprintf(stderr,"Object Name: %s\n", p_station->call_sign);

    // Only move the object if it is our callsign, else force the
    // user to adopt the object first, then move it.
    //
    // Not exact match (this one is useful for testing)
//    if (!is_my_call(p_station->origin,1)) {
    //
    // Exact match (this one is for production code)
    if (!is_my_call(p_station->origin,0)) {

        // It's not from my callsign
        if (strncmp(calldata,"2",1) == 0) {

            // We're doing a Move Object operation
            //fprintf(stderr,"Modify_object:  Object not owned by me!\n");
            popup_message_always(langcode("POPEM00035"),
                "Object not owned by me!\nTry adopting the object first.\n");
            return;
        }
    }

    Set_Del_Object( w, p_station, calldata );
}





// Global variables for use with routines following
Widget change_tactical_dialog = (Widget)NULL;
Widget tactical_text = (Widget)NULL;
DataRow *tactical_pointer = NULL;


void Change_tactical_destroy_shell( /*@unused@*/ Widget widget, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    Widget shell = (Widget) clientData;
    XtPopdown(shell);
    XtDestroyWidget(shell);
    change_tactical_dialog = (Widget)NULL;
}





void Change_tactical_change_data(Widget widget, XtPointer clientData, XtPointer callData) {
    char *temp;

    temp = XmTextGetString(tactical_text);

    if (tactical_pointer->tactical_call_sign == NULL) {
        // Malloc some memory to hold it.
        tactical_pointer->tactical_call_sign = (char *)malloc(MAX_TACTICAL_CALL+1);
    }

    if (tactical_pointer->tactical_call_sign != NULL) {

        // Check for blank tactical call.  If so, free the space.
        if (temp[0] == '\0') {
            free(tactical_pointer->tactical_call_sign);
            tactical_pointer->tactical_call_sign = NULL;
        }
        else {
            xastir_snprintf(tactical_pointer->tactical_call_sign,
                MAX_TACTICAL_CALL,
                "%s",
                temp);
        }

        fprintf(stderr,
            "Assigned tactical call \"%s\" to %s\n",
            temp,
            tactical_pointer->call_sign);

        // Log the change in the tactical_calls.log file
        log_tactical_call(tactical_pointer->call_sign,
            tactical_pointer->tactical_call_sign);
    }
    else {
        fprintf(stderr,
            "Couldn't malloc space for tactical callsign\n");
    }

    XtFree(temp);

    redraw_on_new_data = 2;  // redraw now

    Change_tactical_destroy_shell(widget,clientData,callData);
}





void Change_tactical(Widget w, XtPointer clientData, XtPointer callData) {
    static Widget  pane, my_form, button_ok, button_close, label;
    Atom delw;
    Arg al[2];                    /* Arg List */
    register unsigned int ac = 0;           /* Arg Count */

    if (!change_tactical_dialog) {
        change_tactical_dialog =
            XtVaCreatePopupShell(langcode("WPUPSTI065"),
                xmDialogShellWidgetClass,
                Global.top,
                XmNdeleteResponse,XmDESTROY,
                XmNdefaultPosition, FALSE,
                NULL);

        pane = XtVaCreateWidget("Change Tactical pane",
                xmPanedWindowWidgetClass,
                change_tactical_dialog,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        my_form =  XtVaCreateWidget("Change Tactical my_form",
                xmFormWidgetClass,
                pane,
                XmNfractionBase, 3,
                XmNautoUnmanage, FALSE,
                XmNshadowThickness, 1,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);


        /*set args for color */
        ac=0;
        XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
        XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;

        // Display the callsign or object/item name we're working on
        // in a label at the top of the dialog.  Otherwise we don't
        // know what station we're operating on.
        //
        label = XtVaCreateManagedWidget(tactical_pointer->call_sign,
                xmLabelWidgetClass,
                my_form,
                XmNtopAttachment, XmATTACH_FORM,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 5,
                XmNrightAttachment, XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        tactical_text = XtVaCreateManagedWidget("Change_Tactical text",
                xmTextWidgetClass,
                my_form,
                XmNeditable,   TRUE,
                XmNcursorPositionVisible, TRUE,
                XmNsensitive, TRUE,
                XmNshadowThickness,    1,
                XmNcolumns, MAX_TACTICAL_CALL,
                XmNwidth, ((MAX_TACTICAL_CALL*7)+2),
                XmNmaxLength, MAX_TACTICAL_CALL,
                XmNbackground, colors[0x0f],
                XmNtopOffset, 5,
                XmNtopAttachment,XmATTACH_WIDGET,
                XmNtopWidget, label,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNrightAttachment,XmATTACH_NONE,
                XmNnavigationType, XmTAB_GROUP,
                NULL);

        // Fill in the current value of tactical callsign
        XmTextSetString(tactical_text, tactical_pointer->tactical_call_sign);

        button_ok = XtVaCreateManagedWidget(langcode("UNIOP00001"),
                xmPushButtonGadgetClass,
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, tactical_text,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_FORM,
                XmNbottomOffset, 5,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 0,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 1,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);


        button_close = XtVaCreateManagedWidget(langcode("UNIOP00003"),
                xmPushButtonGadgetClass,
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, tactical_text,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_FORM,
                XmNbottomOffset, 5,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 2,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 3,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        XtAddCallback(button_ok,
            XmNactivateCallback,
            Change_tactical_change_data,
            change_tactical_dialog);
        XtAddCallback(button_close,
            XmNactivateCallback,
            Change_tactical_destroy_shell,
            change_tactical_dialog);

        pos_dialog(change_tactical_dialog);

        delw = XmInternAtom(XtDisplay(change_tactical_dialog),"WM_DELETE_WINDOW", FALSE);
        XmAddWMProtocolCallback(change_tactical_dialog, delw, Change_tactical_destroy_shell, (XtPointer)change_tactical_dialog);

        XtManageChild(my_form);
        XtManageChild(pane);

        XtPopup(change_tactical_dialog,XtGrabNone);
        fix_dialog_size(change_tactical_dialog);

        // Move focus to the Close button.  This appears to
        // highlight the
        // button fine, but we're not able to hit the <Enter> key to
        // have that default function happen.  Note:  We _can_ hit
        // the
        // <SPACE> key, and that activates the option.
//        XmUpdateDisplay(change_tactical_dialog);
        XmProcessTraversal(button_close, XmTRAVERSE_CURRENT);

    } else {
        (void)XRaiseWindow(XtDisplay(change_tactical_dialog),
            XtWindow(change_tactical_dialog));
    }
}





/*
 *  Assign a tactical call to a station
 */
void Assign_Tactical_Call( Widget w, XtPointer clientData, XtPointer calldata) {
    DataRow *p_station = clientData;

    //fprintf(stderr,"Object Name: %s\n", p_station->call_sign);
    tactical_pointer = p_station;
    Change_tactical(w, p_station, NULL);
}





static void PosTestExpose(Widget parent, XtPointer clientData, XEvent *event, Boolean * continueToDispatch) {
    Position x, y;

    XtVaGetValues(parent, XmNx, &x, XmNy, &y, NULL);

    if (debug_level & 1)
        fprintf(stderr,"Window Decoration Offsets:  X:%d\tY:%d\n", x, y);

    // Store the new-found offets in global variables
    decoration_offset_x = (int)x;
    decoration_offset_y = (int)y;

    // Get rid of the event handler and the test dialog
    XtRemoveEventHandler(parent, ExposureMask, True, (XtEventHandler) PosTestExpose, (XtPointer)NULL);
    XtRemoveGrab(XtParent(parent));
    XtDestroyWidget(XtParent(parent));
}





// Here's a stupid trick that we have to do in order to find out how big
// window decorations are.  We need to know this information in order to
// be able to kill/recreate dialogs in the same place each time.  If we
// were to just get and set the X/Y values of the dialog, we would creep
// across the screen by the size of the decorations each time.
// I've seen it.  It's ugly.
//
void compute_decorations( void ) {
    Widget cdtest = (Widget)NULL;
    Widget cdform = (Widget)NULL;
    Cardinal n = 0;
    Arg args[20];
 

    // We'll create a dummy dialog at 0,0, then query its
    // position.  That'll give us back the position of the
    // widget.  Subtract 0,0 from it (easy huh?) and we get
    // the size of the window decorations.  Store these values
    // in global variables for later use.

    n = 0;
    XtSetArg(args[n], XmNx, 0); n++;
    XtSetArg(args[n], XmNy, 0); n++;

    cdtest = (Widget) XtVaCreatePopupShell("compute_decorations test",
                        xmDialogShellWidgetClass,Global.top,
                        args, n,
                        NULL);
 
    n = 0;
    XtSetArg(args[n], XmNwidth, 0); n++;    // Make it tiny
    XtSetArg(args[n], XmNheight, 0); n++;   // Make it tiny
    cdform = XmCreateForm(cdtest, "compute_decorations test form", args, n);

    XtAddEventHandler(cdform, ExposureMask, True, (XtEventHandler) PosTestExpose,
        (XtPointer)NULL);

    XtManageChild(cdform);
    XtManageChild(cdtest);
}





// Enable/disable auto-update of Station_data dialog
void station_data_auto_update_toggle ( /*@unused@*/ Widget widget, /*@unused@*/ XtPointer clientData, XtPointer callData) {
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        station_data_auto_update = 1;
    else
        station_data_auto_update = 0;
}





// Fill in the station data window with real data
void station_data_fill_in ( /*@unused@*/ Widget w, XtPointer clientData, XtPointer calldata ) {
    DataRow *p_station;
    char *station = (char *) clientData;
    char temp[300];
    int pos, last_pos;
    char temp_my_distance[20];
    char temp_my_course[20];
    char temp1_my_course[20];
    float temp_out_C, e, humidex;
    long l_lat, l_lon;
    float value;
    WeatherRow *weather;
    time_t sec;
    struct tm *time;
    int i;
    int track_count = 0;

// Maximum tracks listed in Station Info dialog.  This prevents
// lockups on extremely long tracks.
#define MAX_TRACK_LIST 50


    db_station_info_callsign = (char *) clientData; // Used for auto-updating this dialog
    temp_out_C=0;
    pos=0;

begin_critical_section(&db_station_info_lock, "db.c:Station_data" );

    if (db_station_info == NULL) {  // We don't have a dialog to write to

end_critical_section(&db_station_info_lock, "db.c:Station_data" );

        return;
    }

    if (!search_station_name(&p_station,station,1)  // Can't find call,
            || (p_station->flag & ST_ACTIVE) == 0) {  // or found deleted objects

end_critical_section(&db_station_info_lock, "db.c:Station_data" );

        return;
    }


    // Clear the text
    XmTextSetString(si_text,NULL);


    // Weather Data ...
    if (p_station->weather_data != NULL
            // Make sure the timestamp on the weather is current
            && (int)(((sec_old + p_station->weather_data->wx_sec_time)) >= sec_now()) ) {

        last_pos = pos;

        weather = p_station->weather_data;

        pos += strlen(temp);
        xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI024"),weather->wx_type,weather->wx_station);
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);
        sprintf(temp, "\n");
        xastir_snprintf(temp, sizeof(temp), "\n");
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);
        if (english_units)
            xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI026"),weather->wx_course,weather->wx_speed);
        else
            xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI025"),weather->wx_course,(int)(atof(weather->wx_speed)*1.6094));

        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);

        if (strlen(weather->wx_gust) > 0) {
            if (english_units)
                xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI028"),weather->wx_gust);
            else
                xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI027"),(int)(atof(weather->wx_gust)*1.6094));

            strncat(temp,
                "\n",
                sizeof(temp) - strlen(temp));
        } else
            xastir_snprintf(temp, sizeof(temp), "\n");

        XmTextInsert(si_text, pos, temp);
        pos += strlen(temp);

        if (strlen(weather->wx_temp) > 0) {
            if (english_units)
                xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI030"),weather->wx_temp);
            else {
                temp_out_C =(((atof(weather->wx_temp)-32)*5.0)/9.0);
                xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI029"),temp_out_C);
            }
            XmTextInsert(si_text,pos,temp);
            pos += strlen(temp);
        }

        if (strlen(weather->wx_hum) > 0) {
            xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI031"),weather->wx_hum);
            XmTextInsert(si_text,pos,temp);
            pos += strlen(temp);
        }

// NOTE:  The below (Humidex) is not coded for english units, only for metric.
// What is Humidex anyway?  Heat Index?  Wind Chill? --we7u

        // DK7IN: ??? english_units ???
        if (strlen(weather->wx_hum) > 0
                && strlen(weather->wx_temp) > 0
                && (!english_units) &&
                (atof(weather->wx_hum) > 0.0) ) {

            e = (float)(6.112 * pow(10,(7.5 * temp_out_C)/(237.7 + temp_out_C)) * atof(weather->wx_hum) / 100.0);
            humidex = (temp_out_C + ((5.0/9.0) * (e-10.0)));

            xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI032"),humidex);
            XmTextInsert(si_text,pos,temp);
            pos += strlen(temp);
        }

        if (strlen(weather->wx_baro) > 0) {
            if (!english_units) {  // hPa
                xastir_snprintf(temp, sizeof(temp),
                    langcode("WPUPSTI033"),
                    weather->wx_baro);
            }
            else {  // Inches Mercury
                xastir_snprintf(temp, sizeof(temp),
                    langcode("WPUPSTI063"),
                    atof(weather->wx_baro)*0.02953);
            }
            XmTextInsert(si_text,pos,temp);
            pos += strlen(temp);
            xastir_snprintf(temp, sizeof(temp), "\n");
            XmTextInsert(si_text,pos,temp);
            pos += strlen(temp);
        } else {
            if(last_pos!=pos) {
                xastir_snprintf(temp, sizeof(temp), "\n");
                XmTextInsert(si_text,pos,temp);
                pos += strlen(temp);
            }
        }

        if (strlen(weather->wx_snow) > 0) {
            if(english_units)
                xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI035"),atof(weather->wx_snow));
            else
                xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI034"),atof(weather->wx_snow)*2.54);
            XmTextInsert(si_text,pos,temp);
            pos += strlen(temp);
            xastir_snprintf(temp, sizeof(temp), "\n");
            XmTextInsert(si_text,pos,temp);
            pos += strlen(temp);
        }

        if (strlen(weather->wx_rain) > 0 || strlen(weather->wx_prec_00) > 0
                || strlen(weather->wx_prec_24) > 0) {
            xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI036"));
            XmTextInsert(si_text,pos,temp);
            pos += strlen(temp);
        }

        if (strlen(weather->wx_rain) > 0) {
            if (english_units)
                xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI038"),atof(weather->wx_rain)/100.0);
            else
                xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI037"),atof(weather->wx_rain)*.254);

            XmTextInsert(si_text,pos,temp);
            pos += strlen(temp);
        }

        if (strlen(weather->wx_prec_24) > 0) {
            if(english_units)
                xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI040"),atof(weather->wx_prec_24)/100.0);
            else
                xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI039"),atof(weather->wx_prec_24)*.254);

            XmTextInsert(si_text,pos,temp);
            pos += strlen(temp);
        }

        if (strlen(weather->wx_prec_00) > 0) {
            if (english_units)
                xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI042"),atof(weather->wx_prec_00)/100.0);
            else
                xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI041"),atof(weather->wx_prec_00)*.254);

            XmTextInsert(si_text,pos,temp);
            pos += strlen(temp);
        }

        if (strlen(weather->wx_rain_total) > 0) {
            xastir_snprintf(temp, sizeof(temp), "\n%s",langcode("WPUPSTI046"));
            XmTextInsert(si_text,pos,temp);
            pos += strlen(temp);
            if (english_units)
                xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI048"),atof(weather->wx_rain_total)/100.0);
            else
                xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI047"),atof(weather->wx_rain_total)*.254);

            XmTextInsert(si_text,pos,temp);
            pos += strlen(temp);
        }

        // Fuel temp/moisture for RAWS weather stations
        if (strlen(weather->wx_fuel_temp) > 0) {
            if (english_units)
                xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI061"),weather->wx_fuel_temp);
            else {
                temp_out_C =(((atof(weather->wx_fuel_temp)-32)*5.0)/9.0);
                xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI060"),temp_out_C);
            }
            XmTextInsert(si_text,pos,temp);
            pos += strlen(temp);
        }

        if (strlen(weather->wx_fuel_moisture) > 0) {
            xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI062"),weather->wx_fuel_moisture);
            XmTextInsert(si_text,pos,temp);
            pos += strlen(temp);
        }

        xastir_snprintf(temp, sizeof(temp), "\n\n");
 
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);
    }


    // Packets received ... 
    xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI005"),p_station->num_packets);
    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);

    xastir_snprintf(temp,
        sizeof(temp),
        "%s",
        p_station->packet_time);
    temp[2]='/';
    temp[3]='\0';
    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);

    xastir_snprintf(temp,
        sizeof(temp),
        "%s",
        p_station->packet_time+2);
    temp[2]='/';
    temp[3]='\0';
    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);

    xastir_snprintf(temp,
        sizeof(temp),
        "%s",
        p_station->packet_time+4);
    temp[4]=' ';
    temp[5]='\0';
    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);

    xastir_snprintf(temp,
        sizeof(temp),
        "%s",
        p_station->packet_time+8);
    temp[2]=':';
    temp[3]='\0';
    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);

    xastir_snprintf(temp,
        sizeof(temp),
        "%s",
        p_station->packet_time+10);
    temp[2]=':';
    temp[3]='\0';
    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);

    xastir_snprintf(temp,
        sizeof(temp),
        "%s",
        p_station->packet_time+12);
    temp[2]='\n';
    temp[3]='\0';
    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);

    // Object
    if (strlen(p_station->origin) > 0) {
        xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI000"),p_station->origin);
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);
        xastir_snprintf(temp, sizeof(temp), "\n");
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);
    }

    // Print the tactical call, if any
    if (p_station->tactical_call_sign) {
        xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI065"), p_station->tactical_call_sign);
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);
        xastir_snprintf(temp, sizeof(temp), "\t");
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);
    }

    // Heard via TNC ...
    if ((p_station->flag & ST_VIATNC) != 0) {        // test "via TNC" flag
        xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI006"),p_station->heard_via_tnc_port);
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);
    } else {
        xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI007"));
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);
    }

    switch(p_station->data_via) {
        case('L'):
            xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI008"));
            break;

        case('T'):
            xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI009"),p_station->last_port_heard);
            break;

        case('I'):
            xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI010"),p_station->last_port_heard);
            break;

        case('F'):
            xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI011"));
            break;

        default:
            xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI012"));
            break;
    }
    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);

    if (p_station->newest_trackpoint != NULL) {
        xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI013"));
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);
    }
    xastir_snprintf(temp, sizeof(temp), "\n");
    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);

    // Echoed from: ...
    if (is_my_call(p_station->call_sign,1)) {
        xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI055"));
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);
        for (i=0;i<6;i++) {
            if (echo_digis[i][0] == '\0')
                break;

            xastir_snprintf(temp, sizeof(temp), " %s",echo_digis[i]);
            XmTextInsert(si_text,pos,temp);
            pos += strlen(temp);
        }
        xastir_snprintf(temp, sizeof(temp), "\n");
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);
    }

    // Data Path ...
    if (p_station->node_path_ptr != NULL)
        xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI043"),p_station->node_path_ptr);
    else
        xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI043"), "");

    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);
    xastir_snprintf(temp, sizeof(temp), "\n");
    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);

    // Status ...
    if(p_station->status_data != NULL) {   // Found at least one record
        CommentRow *ptr;

        ptr = p_station->status_data;

        while (ptr != NULL) {
            // We don't care if the pointer is NULL.  This will
            // succeed anyway.  It'll just make an empty string.

            // Note that text_ptr may be an empty string.  That's
            // ok.

            //Also print the sec_heard timestamp.
            sec = ptr->sec_heard;
            time = localtime(&sec);

            xastir_snprintf(temp,
                sizeof(temp),
                langcode("WPUPSTI059"),
                time->tm_mon + 1,
                time->tm_mday,
                time->tm_hour,
                time->tm_min,
                ptr->text_ptr);
            XmTextInsert(si_text,pos,temp);
            pos += strlen(temp);

            xastir_snprintf(temp, sizeof(temp), "\n");

            XmTextInsert(si_text,pos,temp);
            pos += strlen(temp);
            ptr = ptr->next;    // Advance to next record (if any)
        }
    }


//    // Comments ...
//    if(strlen(p_station->comments)>0) {
//        xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI044"),p_station->comments);
//        XmTextInsert(si_text,pos,temp);
//        pos += strlen(temp);
//        xastir_snprintf(temp, sizeof(temp), "\n");
//        XmTextInsert(si_text,pos,temp);
//        pos += strlen(temp);
//    }

    // Comments ...
    if(p_station->comment_data != NULL) {   // Found at least one record
        CommentRow *ptr;

        ptr = p_station->comment_data;

        while (ptr != NULL) {
            // We don't care if the pointer is NULL.  This will
            // succeed anyway.  It'll just make an empty string.

            // Note that text_ptr can be an empty string.  That's
            // ok.

            //Also print the sec_heard timestamp.
            sec = ptr->sec_heard;
            time = localtime(&sec);

            xastir_snprintf(temp,
                sizeof(temp),
                langcode("WPUPSTI044"),
                time->tm_mon + 1,
                time->tm_mday,
                time->tm_hour,
                time->tm_min,
                ptr->text_ptr);
            XmTextInsert(si_text,pos,temp);
            pos += strlen(temp);

            xastir_snprintf(temp, sizeof(temp), "\n");

            XmTextInsert(si_text,pos,temp);
            pos += strlen(temp);
            ptr = ptr->next;    // Advance to next record (if any)
        }
    }

    // Current Power Gain ...
    if (strlen(p_station->power_gain) == 7) {
        // Check for RNG instead of PHG
        if (p_station->power_gain[0] == 'R') {
            // Found a Range
            xastir_snprintf(temp,
                sizeof(temp),
                langcode("WPUPSTI067"),
                atoi(&p_station->power_gain[3]));
        }
        else {
            // Found PHG
            phg_decode(langcode("WPUPSTI014"),
                p_station->power_gain,
                temp,
                sizeof(temp) );
        }

        // Check for Map View symbol:  Eyeball symbol with // RNG
        // extension.
        if ( strncmp(p_station->power_gain,"RNG",3) == 0
                && p_station->aprs_symbol.aprs_type == '/'
                && p_station->aprs_symbol.aprs_symbol == 'E' ) {

//fprintf(stderr,"Found a Map View 'eyeball' symbol!\n");

            // Center_Zoom() normally fills in the values with the
            // current zoom/center for the map window.  We want to
            // be able to override these with our own values in this
            // case, derived from the object info.
            center_zoom_override++;
            Center_Zoom(w,NULL,(XtPointer)p_station);
        }
    }
    else if (p_station->flag & (ST_OBJECT|ST_ITEM))
        xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI014"), "none");
    else if (english_units)
        xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI014"), "default (9W @ 20ft HAAT, 3dB omni, range 6.2mi)");
    else
        xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI014"), "default (9W @ 6.1m HAAT, 3dB omni, range 10.0km)");

    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);
    xastir_snprintf(temp, sizeof(temp), "\n");
    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);

    // Current DF Info ...
    if (strlen(p_station->signal_gain) == 7) {
        shg_decode(langcode("WPUPSTI057"), p_station->signal_gain, temp, sizeof(temp) );
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);
        xastir_snprintf(temp, sizeof(temp), "\n");
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);
    }
    if (strlen(p_station->bearing) == 3) {
        bearing_decode(langcode("WPUPSTI058"), p_station->bearing, p_station->NRQ, temp, sizeof(temp) );
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);
        xastir_snprintf(temp, sizeof(temp), "\n");
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);
    }

    // Signpost Data
    if (strlen(p_station->signpost) > 0) {
        xastir_snprintf(temp, sizeof(temp), "%s: %s",langcode("POPUPOB029"), p_station->signpost);
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);
        xastir_snprintf(temp, sizeof(temp), "\n");
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);
    }

    // Altitude ...
    last_pos=pos;
    if (strlen(p_station->altitude) > 0) {
        if (english_units)
            xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI016"),atof(p_station->altitude)*3.28084,"ft");
        else
            xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI016"),atof(p_station->altitude),"m");

        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);
    }

    // Course ...
    if (strlen(p_station->course) > 0) {
        xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI017"),p_station->course);
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);
    }

    // Speed ...
    if (strlen(p_station->speed) > 0) {
        if (english_units)
            xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI019"),atof(p_station->speed)*1.1508);

        else
            xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI018"),atof(p_station->speed)*1.852);

        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);
    }

    if (last_pos!=pos) {
        xastir_snprintf(temp, sizeof(temp), "\n");
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);
    }

    // Distance ...
    last_pos = pos;
    // do my course
    if (!is_my_call(p_station->call_sign,1)) {
        l_lat = convert_lat_s2l(my_lat);
        l_lon = convert_lon_s2l(my_long);

        // Get distance in nautical miles.
        value = (float)calc_distance_course(l_lat,l_lon,p_station->coord_lat,
            p_station->coord_lon,temp1_my_course,sizeof(temp1_my_course));

        // n7tap: This is a quick hack to get some more useful values for
        //        distance to near ojects.
        if (english_units) {
            if (value*1.15078 < 0.99) {
                xastir_snprintf(temp_my_distance,
                    sizeof(temp_my_distance),
                    "%d %s",
                    (int)(value*1.15078*1760),
                    langcode("SPCHSTR004"));    // yards
            }
            else {
                xastir_snprintf(temp_my_distance,
                    sizeof(temp_my_distance),
                    langcode("WPUPSTI020"),     // miles
                    value*1.15078);
            }
        }
        else {
            if (value*1.852 < 0.99) {
                xastir_snprintf(temp_my_distance,
                    sizeof(temp_my_distance),
                    "%d %s",
                    (int)(value*1.852*1000),
                    langcode("UNIOP00031"));    // 'm' as in meters
            }
            else {
                xastir_snprintf(temp_my_distance,
                    sizeof(temp_my_distance),
                    langcode("WPUPSTI021"),     // km
                    value*1.852);
            }
        }
        xastir_snprintf(temp_my_course, sizeof(temp_my_course), "%s°",temp1_my_course);
        xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI022"),temp_my_distance,temp_my_course);
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);
    }

    if(last_pos!=pos) {
        xastir_snprintf(temp, sizeof(temp), "\n");
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);
    }

    // Last Position
    sec  = p_station->sec_heard;
    time = localtime(&sec);
    xastir_snprintf(temp, sizeof(temp), "%s%02d/%02d  %02d:%02d   ",langcode("WPUPSTI023"),
        time->tm_mon + 1, time->tm_mday,time->tm_hour,time->tm_min);
    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);

    if (coordinate_system == USE_UTM
            || coordinate_system == USE_UTM_SPECIAL) {
        convert_xastir_to_UTM_str(temp, sizeof(temp),
            p_station->coord_lon, p_station->coord_lat);
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);
    }
    else if (coordinate_system == USE_MGRS) {
        convert_xastir_to_MGRS_str(temp,
            sizeof(temp),
            p_station->coord_lon,
            p_station->coord_lat,
            0);
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);
    }
    else {
        if (coordinate_system == USE_DDDDDD) {
            convert_lat_l2s(p_station->coord_lat, temp, sizeof(temp), CONVERT_DEC_DEG);
        }
        else if (coordinate_system == USE_DDMMSS) {
            convert_lat_l2s(p_station->coord_lat, temp, sizeof(temp), CONVERT_DMS_NORMAL);
        }
        else {  // Assume coordinate_system == USE_DDMMMM
            convert_lat_l2s(p_station->coord_lat, temp, sizeof(temp), CONVERT_HP_NORMAL);
        }
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);

        xastir_snprintf(temp, sizeof(temp), "  ");
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);

        if (coordinate_system == USE_DDDDDD) {
            convert_lon_l2s(p_station->coord_lon, temp, sizeof(temp), CONVERT_DEC_DEG);
        }
        else if (coordinate_system == USE_DDMMSS) {
            convert_lon_l2s(p_station->coord_lon, temp, sizeof(temp), CONVERT_DMS_NORMAL);
        }
        else {  // Assume coordinate_system == USE_DDMMMM
            convert_lon_l2s(p_station->coord_lon, temp, sizeof(temp), CONVERT_HP_NORMAL);
        }
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);
    }

    if (p_station->altitude[0] != '\0')
        xastir_snprintf(temp, sizeof(temp), " %5.0f%s", atof(p_station->altitude)*cvt_m2len, un_alt);
    else
        substr(temp,"        ",1+5+strlen(un_alt));
    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);

    if (p_station->speed[0] != '\0')
        xastir_snprintf(temp, sizeof(temp), " %4.0f%s",atof(p_station->speed)*cvt_kn2len,un_spd);
    else
        substr(temp,"         ",1+4+strlen(un_spd));
    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);

    if (p_station->course[0] != '\0')
        xastir_snprintf(temp, sizeof(temp), " %3d°",atoi(p_station->course));
    else
        xastir_snprintf(temp, sizeof(temp), "     ");

    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);

    // dl9sau
    // Maidenhead Grid Locator
    xastir_snprintf(temp, sizeof(temp), "  %s", sec_to_loc(p_station->coord_lon, p_station->coord_lat) );
    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);

    if ((p_station->flag & ST_DIRECT) != 0)
        xastir_snprintf(temp, sizeof(temp), " *\n");

    else
        xastir_snprintf(temp, sizeof(temp), "  \n");

    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);

    // list rest of trail data
    if (p_station->newest_trackpoint != NULL) {
        TrackRow *ptr;

        ptr = p_station->newest_trackpoint;

        // Skip the first (latest) trackpoint as if it exists, it'll
        // be the same as the data in the station record, which we
        // just printed out.
        if (ptr->prev != NULL)
           ptr = ptr->prev;
 
        while ( (ptr != NULL) && (track_count <= MAX_TRACK_LIST) ) {

            track_count++;

            sec  = ptr->sec;
            time = localtime(&sec);
            if ((ptr->flag & TR_NEWTRK) != '\0')
                xastir_snprintf(temp, sizeof(temp), "            +  %02d/%02d  %02d:%02d   ",
                    time->tm_mon + 1,time->tm_mday,time->tm_hour,time->tm_min);
            else
                xastir_snprintf(temp, sizeof(temp), "               %02d/%02d  %02d:%02d   ",
                    time->tm_mon + 1,time->tm_mday,time->tm_hour,time->tm_min);

            XmTextInsert(si_text,pos,temp);
            pos += strlen(temp);

            if (coordinate_system == USE_UTM
                    || coordinate_system == USE_UTM_SPECIAL) {
                convert_xastir_to_UTM_str(temp, sizeof(temp),
                    ptr->trail_long_pos,
                    ptr->trail_lat_pos);
                XmTextInsert(si_text,pos,temp);
                pos += strlen(temp);
            }
            else if (coordinate_system == USE_MGRS) {
                convert_xastir_to_MGRS_str(temp,
                    sizeof(temp),
                    ptr->trail_long_pos,
                    ptr->trail_lat_pos,
                    0);
                XmTextInsert(si_text,pos,temp);
                pos += strlen(temp);
            }
            else {
                if (coordinate_system == USE_DDDDDD) {
                    convert_lat_l2s(ptr->trail_lat_pos,
                        temp,
                        sizeof(temp),
                        CONVERT_DEC_DEG);
                }
                else if (coordinate_system == USE_DDMMSS) {
                    convert_lat_l2s(ptr->trail_lat_pos,
                        temp,
                        sizeof(temp),
                        CONVERT_DMS_NORMAL);
                }
                else {  // Assume coordinate_system == USE_DDMMMM
                    convert_lat_l2s(ptr->trail_lat_pos,
                        temp,
                        sizeof(temp),
                        CONVERT_HP_NORMAL);
                }
                XmTextInsert(si_text,pos,temp);
                pos += strlen(temp);

                xastir_snprintf(temp, sizeof(temp), "  ");
                XmTextInsert(si_text,pos,temp);
                pos += strlen(temp);

                if (coordinate_system == USE_DDDDDD) {
                    convert_lon_l2s(ptr->trail_long_pos,
                        temp,
                        sizeof(temp),
                        CONVERT_DEC_DEG);
                }
                else if (coordinate_system == USE_DDMMSS) {
                    convert_lon_l2s(ptr->trail_long_pos,
                        temp,
                        sizeof(temp),
                        CONVERT_DMS_NORMAL);
                }
                else {  // Assume coordinate_system == USE_DDMMMM
                    convert_lon_l2s(ptr->trail_long_pos,
                        temp,
                        sizeof(temp),
                        CONVERT_HP_NORMAL);
                }
                XmTextInsert(si_text,pos,temp);
                pos += strlen(temp);
            }

            if (ptr->altitude > -99999l)
                xastir_snprintf(temp, sizeof(temp), " %5.0f%s",
                    ptr->altitude * cvt_dm2len,
                    un_alt);
            else
                substr(temp,"         ",1+5+strlen(un_alt));

            XmTextInsert(si_text,pos,temp);
            pos += strlen(temp);

            if (ptr->speed >= 0)
                xastir_snprintf(temp, sizeof(temp), " %4.0f%s",
                    ptr->speed * cvt_hm2len,
                    un_spd);
            else
                substr(temp,"         ",1+4+strlen(un_spd));

            XmTextInsert(si_text,pos,temp);
            pos += strlen(temp);

            if (ptr->course >= 0)
                xastir_snprintf(temp, sizeof(temp), " %3d°",
                    ptr->course);
            else
                xastir_snprintf(temp, sizeof(temp), "     ");

            XmTextInsert(si_text,pos,temp);
            pos += strlen(temp);

            // dl9sau
            xastir_snprintf(temp, sizeof(temp), "  %s",
                sec_to_loc(ptr->trail_long_pos,
                    ptr->trail_lat_pos) );
            XmTextInsert(si_text,pos,temp); 
            pos += strlen(temp);

            if ((ptr->flag & TR_LOCAL) != '\0')
                xastir_snprintf(temp, sizeof(temp), " *\n");
            else
                xastir_snprintf(temp, sizeof(temp), "  \n");

            XmTextInsert(si_text,pos,temp);
            pos += strlen(temp);

            // Go back in time one trackpoint
            ptr = ptr->prev;
        }
    }


    if (fcc_lookup_pushed) {
        Station_data_add_fcc(w, clientData, calldata);
    }
    else if (rac_lookup_pushed) {
        Station_data_add_rac(w, clientData, calldata);
    }

    XmTextShowPosition(si_text,0);

end_critical_section(&db_station_info_lock, "db.c:Station_data" );

}





/*
 *  List station info and trail
 *  If calldata is non-NULL, then we drop straight through to the
 *  Modify->Object or Assign_Tactical_Call dialogs.
 */
void Station_data(/*@unused@*/ Widget w, XtPointer clientData, XtPointer calldata) {
    DataRow *p_station;
    char *station = (char *) clientData;
    static char local_station[25];
    char temp[300];
    unsigned int n;
    Atom delw;
    static Widget  pane, form, button_cancel, button_message,
        button_nws, button_fcc, button_rac, button_clear_track,
        button_trace, button_messages, button_object_modify,
        button_direct, button_version, station_icon, station_call,
        station_type, station_data_auto_update_w,
        button_tactical;
    Arg args[20];
    Pixmap icon;
    Position x,y;    // For saving current dialog position
    int restore_position = 0;


    busy_cursor(appshell);

    db_station_info_callsign = (char *) clientData; // Used for auto-updating this dialog


    // Make a copy of the name.
    xastir_snprintf(local_station,sizeof(local_station),"%s",station);

    if (search_station_name(&p_station,station,1)   // find call
        && (p_station->flag & ST_ACTIVE) != 0) {    // ignore deleted objects
    }
    else {
        fprintf(stderr,"Couldn't find station in database\n");
        return; // Don't update current/create new dialog
    }

 
    // If we haven't calculated our decoration offsets yet, do so now
    if ( (decoration_offset_x == 0) && (decoration_offset_y == 0) ) {
        compute_decorations();
    }
 
    if (db_station_info != NULL) {  // We already have a dialog
        restore_position = 1;

        // This is a pain.  We can get the X/Y position, but when
        // we restore the new dialog to the same position we're
        // off by the width/height of our window decorations.  Call
        // above was added to pre-compute the offsets that we'll need.
        XtVaGetValues(db_station_info, XmNx, &x, XmNy, &y, NULL);

        // This call doesn't work.  It returns the widget location,
        // just like the XtVaGetValues call does.  I need the window
        // decoration location instead.
        //XtTranslateCoords(db_station_info, 0, 0, &xnew, &ynew);
        //fprintf(stderr,"%d:%d\t%d:%d\n", x, xnew, y, ynew);

        if (last_station_info_x == 0)
            last_station_info_x = x - decoration_offset_x;

        if (last_station_info_y == 0)
            last_station_info_y = y - decoration_offset_y;

        // Now get rid of the old dialog
       Station_data_destroy_shell(db_station_info, db_station_info, NULL);
    }
    else {
        // Clear the global state variables
        fcc_lookup_pushed = 0;
        rac_lookup_pushed = 0;
    }


begin_critical_section(&db_station_info_lock, "db.c:Station_data" );


    if (db_station_info == NULL) {
        // Start building the dialog from the bottom up.  That way
        // we can keep the buttons attached to the bottom of the
        // form and the correct height, and let the text widget
        // grow/shrink as the dialog is resized.

        db_station_info = XtVaCreatePopupShell(langcode("WPUPSTI001"),xmDialogShellWidgetClass,Global.top,
                        XmNdeleteResponse,XmDESTROY,
                        XmNdefaultPosition, FALSE,
                        NULL);

        pane = XtVaCreateWidget("Station Data pane",xmPanedWindowWidgetClass, db_station_info,
                        XmNbackground, colors[0xff],
                        NULL);

        form =  XtVaCreateWidget("Station Data form",xmFormWidgetClass, pane,
                        XmNfractionBase, 4,
                        XmNbackground, colors[0xff],
                        XmNautoUnmanage, FALSE,
                        XmNshadowThickness, 1,
                        NULL);


// Start with the bottom row, left button


        button_clear_track = NULL;  // Need this later, don't delete!
        if (p_station->newest_trackpoint != NULL) {
            // [ Clear Track ]
            button_clear_track = XtVaCreateManagedWidget(langcode("WPUPSTI045"),xmPushButtonGadgetClass, form,
                            XmNtopAttachment, XmATTACH_NONE,
                            XmNbottomAttachment, XmATTACH_FORM,
                            XmNbottomOffset,5,
                            XmNleftAttachment, XmATTACH_FORM,
                            XmNleftOffset,5,
                            XmNrightAttachment, XmATTACH_POSITION,
                            XmNrightPosition, 1,
                            XmNbackground, colors[0xff],
                            XmNnavigationType, XmTAB_GROUP,
                            NULL);
            XtAddCallback(button_clear_track, XmNactivateCallback, Station_data_destroy_track ,(XtPointer)p_station);

        } else {
            // DK7IN: I drop the version button for mobile stations
            // we just have too much buttons...
            // and should find another solution
            // [ Station Version Query ]
            button_version = XtVaCreateManagedWidget(langcode("WPUPSTI052"),xmPushButtonGadgetClass, form,
                            XmNtopAttachment, XmATTACH_NONE,
                            XmNbottomAttachment, XmATTACH_FORM,
                            XmNbottomOffset,5,
                            XmNleftAttachment, XmATTACH_FORM,
                            XmNleftOffset,5,
                            XmNrightAttachment, XmATTACH_POSITION,
                            XmNrightPosition, 1,
                            XmNbackground, colors[0xff],
                            XmNnavigationType, XmTAB_GROUP,
                            NULL);
            XtAddCallback(button_version, XmNactivateCallback, Station_query_version ,(XtPointer)p_station->call_sign);
        }            

        // [ Trace Query ]
        button_trace = XtVaCreateManagedWidget(langcode("WPUPSTI049"),xmPushButtonGadgetClass, form,
                                XmNtopAttachment, XmATTACH_NONE,
                                XmNbottomAttachment, XmATTACH_FORM,
                                XmNbottomOffset,5,
                                XmNleftAttachment, XmATTACH_POSITION,
                                XmNleftPosition, 1,
                                XmNrightAttachment, XmATTACH_POSITION,
                                XmNrightPosition, 2,
                                XmNbackground, colors[0xff],
                                XmNnavigationType, XmTAB_GROUP,
                                NULL);
        XtAddCallback(button_trace, XmNactivateCallback, Station_query_trace ,(XtPointer)p_station->call_sign);

        // [ Un-Acked Messages Query ]
        button_messages = XtVaCreateManagedWidget(langcode("WPUPSTI050"),xmPushButtonGadgetClass, form,
                                XmNtopAttachment, XmATTACH_NONE,
                                XmNbottomAttachment, XmATTACH_FORM,
                                XmNbottomOffset,5,
                                XmNleftAttachment, XmATTACH_POSITION,
                                XmNleftPosition, 2,
                                XmNrightAttachment, XmATTACH_POSITION,
                                XmNrightPosition, 3,
                                XmNbackground, colors[0xff],
                                XmNnavigationType, XmTAB_GROUP,
                                NULL);
        XtAddCallback(button_messages, XmNactivateCallback, Station_query_messages ,(XtPointer)p_station->call_sign);

        // [ Direct Stations Query ]
        button_direct = XtVaCreateManagedWidget(langcode("WPUPSTI051"),xmPushButtonGadgetClass, form,
                                XmNtopAttachment, XmATTACH_NONE,
                                XmNbottomAttachment, XmATTACH_FORM,
                                XmNbottomOffset,5,
                                XmNleftAttachment, XmATTACH_POSITION,
                                XmNleftPosition, 3,
                                XmNrightAttachment, XmATTACH_POSITION,
                                XmNrightPosition, 4,
                                XmNbackground, colors[0xff],
                                XmNnavigationType, XmTAB_GROUP,
                                NULL);
        XtAddCallback(button_direct, XmNactivateCallback, Station_query_direct ,(XtPointer)p_station->call_sign);


// Now proceed to the row above it, left button first


        // [ Store Track ] or single Position
        button_store_track = XtVaCreateManagedWidget(langcode("WPUPSTI054"),xmPushButtonGadgetClass, form,
                            XmNtopAttachment, XmATTACH_NONE,
//XmNtopWidget,XtParent(si_text),
                            XmNbottomAttachment, XmATTACH_WIDGET,
                            XmNbottomWidget, (button_clear_track) ? button_clear_track : button_version,
                            XmNbottomOffset, 1,
                            XmNleftAttachment, XmATTACH_FORM,
                            XmNleftOffset,5,
                            XmNrightAttachment, XmATTACH_POSITION,
                            XmNrightPosition, 1,
                            XmNbackground, colors[0xff],
                            XmNnavigationType, XmTAB_GROUP,
                            NULL);
        XtAddCallback(button_store_track,   XmNactivateCallback, Station_data_store_track ,(XtPointer)p_station);

        if ( ((p_station->flag & ST_OBJECT) == 0) && ((p_station->flag & ST_ITEM) == 0) ) { // Not an object/
            // fprintf(stderr,"Not an object or item...\n");
            // [Send Message]
            button_message = XtVaCreateManagedWidget(langcode("WPUPSTI002"),xmPushButtonGadgetClass, form,
                            XmNtopAttachment, XmATTACH_NONE,
                            XmNbottomAttachment, XmATTACH_WIDGET,
                            XmNbottomWidget, button_trace,
                            XmNbottomOffset, 1,
                            XmNleftAttachment, XmATTACH_POSITION,
                            XmNleftPosition, 1,
                            XmNrightAttachment, XmATTACH_POSITION,
                            XmNrightPosition, 2,
                            XmNbackground, colors[0xff],
                            XmNnavigationType, XmTAB_GROUP,
                            NULL);
            XtAddCallback(button_message, XmNactivateCallback, Send_message_call ,(XtPointer)p_station->call_sign);
        } else {
            // fprintf(stderr,"Found an object or item...\n");
            button_object_modify = XtVaCreateManagedWidget(langcode("WPUPSTI053"),xmPushButtonGadgetClass, form,
                            XmNtopAttachment, XmATTACH_NONE,
                            XmNbottomAttachment, XmATTACH_WIDGET,
                            XmNbottomWidget, button_trace,
                            XmNbottomOffset, 1,
                            XmNleftAttachment, XmATTACH_POSITION,
                            XmNleftPosition, 1,
                            XmNrightAttachment, XmATTACH_POSITION,
                            XmNrightPosition, 2,
                            XmNbackground, colors[0xff],
                            XmNnavigationType, XmTAB_GROUP,
                            NULL);
            XtAddCallback(button_object_modify,
                XmNactivateCallback,
                Modify_object,
                (XtPointer)p_station);
        }


        // Check whether it is a non-weather alert object/item.  If
        // so, try to use the origin callsign instead of the object
        // for FCC/RAC lookups.
        if ( (p_station->flag & ST_OBJECT) || (p_station->flag & ST_ITEM) ) {
            xastir_snprintf(local_station,sizeof(local_station),"%s",p_station->origin);
        }


        // Add "Fetch NWS Info" button if it is an object or item
        // and has "WXSVR" in its path somewhere.
        //
        // Note from Dale Huguley:
        //   "I would say an object with 6 upper alpha chars for the
        //   "from" call and " {AAAAA" (space curly 5 alphanumerics)
        //   at the end is almost guaranteed to be from Wxsvr.
        //   Fingering for the six alphas and the first three
        //   characters after the curly brace should be a reliable
        //   finger - as in SEWSVR>APRS::a_bunch_of_info_in_here_
        //   {H45AA finger SEWSVRH45@wxsvr.net"
        //
        // Note from Curt:  I had to remove the space from the
        // search as well, 'cuz the multipoint objects don't have
        // the space before the final curly-brace.
        //
        if ( ( (p_station->flag & ST_OBJECT) || (p_station->flag & ST_ITEM) )
                && (p_station->comment_data != NULL)
                && ( strstr(p_station->comment_data->text_ptr, "{") != NULL ) ) {

            static char temp[25];
            char *ptr3;


            button_nws = XtVaCreateManagedWidget(langcode("WPUPSTI064"),xmPushButtonGadgetClass, form,
                            XmNtopAttachment, XmATTACH_NONE,
                            XmNbottomAttachment, XmATTACH_WIDGET,
                            XmNbottomWidget, button_messages,
                            XmNbottomOffset, 1,
                            XmNleftAttachment, XmATTACH_POSITION,
                            XmNleftPosition, 2,
                            XmNrightAttachment, XmATTACH_POSITION,
                            XmNrightPosition, 3,
                            XmNbackground, colors[0xff],
                            XmNnavigationType, XmTAB_GROUP,
                            NULL);

            // We need to contruct the "special" finger address.
            // We'll use the FROM callsign and the first three chars
            // of the curly-brace portion of the comment field.
            // Callsign in this case is from the "origin" field.
            // The curly-brace text is at the end of one of the
            // "comment_data" records, hopefully the first one
            // checked (most recent).
            //

            xastir_snprintf(temp,
                sizeof(temp),
                "%s",
                p_station->origin);
            temp[6] = '\0';
            ptr3 = strstr(p_station->comment_data->text_ptr,"{");
            ptr3++; // Skip over the '{' character
            strncat(temp,ptr3,3);

//fprintf(stderr,"New Handle: %s\n", temp);

            XtAddCallback(button_nws,
                XmNactivateCallback,
                Station_data_wx_alert,
                (XtPointer)temp);
        }


        // Add FCC button only if probable match
        else if ((! strncmp(local_station,"A",1)) || (!  strncmp(local_station,"K",1)) ||
            (! strncmp(local_station,"N",1)) || (! strncmp(local_station,"W",1))  ) {
            button_fcc = XtVaCreateManagedWidget(langcode("WPUPSTI003"),xmPushButtonGadgetClass, form,
                            XmNtopAttachment, XmATTACH_NONE,
                            XmNbottomAttachment, XmATTACH_WIDGET,
                            XmNbottomWidget, button_messages,
                            XmNbottomOffset, 1,
                            XmNleftAttachment, XmATTACH_POSITION,
                            XmNleftPosition, 2,
                            XmNrightAttachment, XmATTACH_POSITION,
                            XmNrightPosition, 3,
                            XmNbackground, colors[0xff],
                            XmNnavigationType, XmTAB_GROUP,
                            NULL);
            XtAddCallback(button_fcc,
                XmNactivateCallback,
                Station_data_add_fcc,
                (XtPointer)local_station);

            if ( ! check_fcc_data())
                XtSetSensitive(button_fcc,FALSE);
        }


        // Add RAC button only if probable match
        else if (!strncmp(local_station,"VE",2) || !strncmp(local_station,"VA",2)) {
            button_rac = XtVaCreateManagedWidget(langcode("WPUPSTI004"),xmPushButtonGadgetClass, form,
                            XmNtopAttachment, XmATTACH_NONE,
                            XmNbottomAttachment, XmATTACH_WIDGET,
                            XmNbottomWidget, button_messages,
                            XmNbottomOffset, 1,
                            XmNleftAttachment, XmATTACH_POSITION,
                            XmNleftPosition, 2,
                            XmNrightAttachment, XmATTACH_POSITION,
                            XmNrightPosition, 3,
                            XmNbackground, colors[0xff],
                            XmNnavigationType, XmTAB_GROUP,
                            NULL);
            XtAddCallback(button_rac,
                XmNactivateCallback,
                Station_data_add_rac,
                (XtPointer)local_station);

            if ( ! check_rac_data())
                XtSetSensitive(button_rac,FALSE);
        }

        button_cancel = XtVaCreateManagedWidget(langcode("UNIOP00003"),xmPushButtonGadgetClass, form,
                            XmNtopAttachment, XmATTACH_NONE,
                            XmNbottomAttachment, XmATTACH_WIDGET,
                            XmNbottomWidget, button_direct,
                            XmNbottomOffset, 1,
                            XmNleftAttachment, XmATTACH_POSITION,
                            XmNleftPosition, 3,
                            XmNrightAttachment, XmATTACH_POSITION,
                            XmNrightPosition, 4,
                            XmNrightOffset, 5,
                            XmNbackground, colors[0xff],
                            XmNnavigationType, XmTAB_GROUP,
                            NULL);
        XtAddCallback(button_cancel, XmNactivateCallback, Station_data_destroy_shell, db_station_info);


// Now build from the top of the dialog to the buttons.


        icon = XCreatePixmap(XtDisplay(appshell),RootWindowOfScreen(XtScreen(appshell)),
                    20,20,DefaultDepthOfScreen(XtScreen(appshell)));

        symbol(db_station_info,0,p_station->aprs_symbol.aprs_type,
            p_station->aprs_symbol.aprs_symbol,
            p_station->aprs_symbol.special_overlay,icon,0,0,0,' ');

        station_icon = XtVaCreateManagedWidget("Station Data icon", xmLabelWidgetClass, form,
                                XmNtopAttachment, XmATTACH_FORM,
                                XmNtopOffset, 2,
                                XmNbottomAttachment, XmATTACH_NONE,
                                XmNleftAttachment, XmATTACH_FORM,
                                XmNleftOffset, 5,
                                XmNrightAttachment, XmATTACH_NONE,
                                XmNlabelType, XmPIXMAP,
                                XmNlabelPixmap,icon,
                                XmNbackground, colors[0xff],
                                NULL);

        station_type = XtVaCreateManagedWidget("Station Data type", xmTextFieldWidgetClass, form,
                                XmNeditable,   FALSE,
                                XmNcursorPositionVisible, FALSE,
                                XmNtraversalOn, FALSE,
                                XmNshadowThickness,       0,
                                XmNcolumns,5,
                                XmNwidth,((5*7)+2),
                                XmNbackground, colors[0xff],
                                XmNtopAttachment,XmATTACH_FORM,
                                XmNtopOffset, 2,
                                XmNbottomAttachment,XmATTACH_NONE,
                                XmNleftAttachment, XmATTACH_WIDGET,
                                XmNleftWidget,station_icon,
                                XmNleftOffset,10,
                                XmNrightAttachment,XmATTACH_NONE,
                                NULL);

        xastir_snprintf(temp, sizeof(temp), "%c%c%c", p_station->aprs_symbol.aprs_type,
            p_station->aprs_symbol.aprs_symbol,
            p_station->aprs_symbol.special_overlay);

        XmTextFieldSetString(station_type, temp);
        XtManageChild(station_type);

        station_call = XtVaCreateManagedWidget("Station Data call", xmTextFieldWidgetClass, form,
                                XmNeditable,   FALSE,
                                XmNcursorPositionVisible, FALSE,
                                XmNtraversalOn, FALSE,
                                XmNshadowThickness,       0,
                                XmNcolumns,15,
                                XmNwidth,((15*7)+2),
                                XmNbackground, colors[0xff],
                                XmNtopAttachment,XmATTACH_FORM,
                                XmNtopOffset, 2,
                                XmNbottomAttachment,XmATTACH_NONE,
                                XmNleftAttachment, XmATTACH_WIDGET,
                                XmNleftWidget, station_type,
                                XmNleftOffset,10,
                                XmNrightAttachment,XmATTACH_NONE,
                                NULL);

        XmTextFieldSetString(station_call,p_station->call_sign);
        XtManageChild(station_call);

        station_data_auto_update_w = XtVaCreateManagedWidget(langcode("WPUPSTI056"),
                                xmToggleButtonGadgetClass, form,
                                XmNtopAttachment,XmATTACH_FORM,
                                XmNtopOffset, 2,
                                XmNbottomAttachment,XmATTACH_NONE,
                                XmNleftAttachment, XmATTACH_WIDGET,
                                XmNleftWidget, station_call,
                                XmNleftOffset,10,
                                XmNrightAttachment,XmATTACH_NONE,
                                XmNbackground,colors[0xff],
                                NULL);
        XtAddCallback(station_data_auto_update_w,XmNvalueChangedCallback,station_data_auto_update_toggle,"1");

        //Add tactical button at the top/right
        // "Assign Tactical Call"
        button_tactical = XtVaCreateManagedWidget(langcode("WPUPSTI066"),xmPushButtonGadgetClass, form,
            XmNtopAttachment, XmATTACH_FORM,
            XmNtopOffset, 5,
            XmNbottomAttachment, XmATTACH_NONE,
            XmNleftAttachment, XmATTACH_WIDGET,
            XmNleftOffset, 20,
            XmNleftWidget, station_data_auto_update_w,
            XmNrightAttachment, XmATTACH_NONE,
            XmNbackground, colors[0xff],
            XmNnavigationType, XmTAB_GROUP,
            NULL);
        XtAddCallback(button_tactical,
            XmNactivateCallback,
            Assign_Tactical_Call,
            (XtPointer)p_station);
        if (p_station->flag & (ST_OBJECT|ST_ITEM)) {
            // We don't allow setting tac-calls for objects/items,
            // so make the button insensitive.
            XtSetSensitive(button_tactical,FALSE);
        }
 
        n=0;
        XtSetArg(args[n], XmNrows, 15); n++;
        XtSetArg(args[n], XmNcolumns, 100); n++;
        XtSetArg(args[n], XmNeditable, FALSE); n++;
        XtSetArg(args[n], XmNtraversalOn, FALSE); n++;
        XtSetArg(args[n], XmNeditMode, XmMULTI_LINE_EDIT); n++;
        XtSetArg(args[n], XmNwordWrap, TRUE); n++;
        XtSetArg(args[n], XmNbackground, colors[0xff]); n++;
        XtSetArg(args[n], XmNscrollHorizontal, FALSE); n++;
        XtSetArg(args[n], XmNcursorPositionVisible, FALSE); n++;
        XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
        XtSetArg(args[n], XmNtopWidget, station_icon); n++;
        XtSetArg(args[n], XmNtopOffset, 5); n++;
        XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
        XtSetArg(args[n], XmNbottomWidget, button_store_track); n++;
        XtSetArg(args[n], XmNbottomOffset, 1); n++;
        XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
        XtSetArg(args[n], XmNleftOffset, 5); n++;
        XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
        XtSetArg(args[n], XmNrightOffset, 5); n++;

        si_text = NULL;
        si_text = XmCreateScrolledText(form,"Station_data",args,n);

end_critical_section(&db_station_info_lock, "db.c:Station_data" );

    // Fill in the si_text widget with real data
    station_data_fill_in( w, (XtPointer)db_station_info_callsign, NULL);
 
begin_critical_section(&db_station_info_lock, "db.c:Station_data" );

        if (restore_position) {
            XtVaSetValues(db_station_info,XmNx,x - decoration_offset_x,XmNy,y - decoration_offset_y,NULL);
        }
        else {
            pos_dialog(db_station_info);
        }

        delw = XmInternAtom(XtDisplay(db_station_info),"WM_DELETE_WINDOW", FALSE);
        XmAddWMProtocolCallback(db_station_info, delw, Station_data_destroy_shell, (XtPointer)db_station_info);

        XtManageChild(form);
        XtManageChild(si_text);
        XtVaSetValues(si_text, XmNbackground, colors[0x0f], NULL);
        XtManageChild(pane);

        if (station_data_auto_update)
            XmToggleButtonSetState(station_data_auto_update_w,TRUE,FALSE);

        if (calldata == NULL) { // We're not going straight to the Modify dialog
                                // and will actually use the dialog we just drew

            XtPopup(db_station_info,XtGrabNone);

//            fix_dialog_size(db_station_info);
            XmTextShowPosition(si_text,0);

            // Move focus to the Cancel button.  This appears to highlight t
            // button fine, but we're not able to hit the <Enter> key to
            // have that default function happen.  Note:  We _can_ hit the
            // <SPACE> key, and that activates the option.
            XmProcessTraversal(button_cancel, XmTRAVERSE_CURRENT);
        }
    }

end_critical_section(&db_station_info_lock, "db.c:Station_data" );

    if (calldata != NULL) {     // We were called from the
                                // Object->Modify or Assign
                                // Tactical Call menu items
        if (strncmp(calldata,"1",1) == 0) {
            Modify_object(w, (XtPointer)p_station, calldata);
        }
        else if (strncmp(calldata,"2",1) == 0) {
            Modify_object(w, (XtPointer)p_station, calldata);
        }
        else if (strncmp(calldata,"3",1) == 0) {
            Assign_Tactical_Call(w, (XtPointer)p_station, calldata);
        }
        // We just drew all of the above, but now we wish to destroy it and
        // just leave the modify dialog in place.
        Station_data_destroy_shell(w, (XtPointer)db_station_info, NULL);
    }
}





// Used for auto-refreshing the Station_info dialog.  Called from
// main.c:UpdateTime() every 30 seconds.
//
void update_station_info(Widget w) {

begin_critical_section(&db_station_info_lock, "db.c:update_station_info" );

    // If we have a dialog to update and a callsign to pass to it
    if (( db_station_info != NULL)
            && (db_station_info_callsign != NULL)
            && (strlen(db_station_info_callsign) != 0) ) {

end_critical_section(&db_station_info_lock, "db.c:update_station_info" );

        // Fill in the si_text widget with real data
        station_data_fill_in( w, (XtPointer)db_station_info_callsign, NULL);
    }
    else {

end_critical_section(&db_station_info_lock, "db.c:update_station_info" );

    }
}





/*
 *  Station Info Selection PopUp window: Canceled
 */
void Station_info_destroy_shell(/*@unused@*/ Widget widget, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    Widget shell = (Widget) clientData;

    if (db_station_info!=NULL)
        Station_data_destroy_shell(db_station_info, db_station_info, NULL);

    XtPopdown(shell);
    (void)XFreePixmap(XtDisplay(appshell),SiS_icon0);  
    (void)XFreePixmap(XtDisplay(appshell),SiS_icon);  

begin_critical_section(&db_station_popup_lock, "db.c:Station_info_destroy_shell" );

    XtDestroyWidget(shell);
    db_station_popup = (Widget)NULL;

end_critical_section(&db_station_popup_lock, "db.c:Station_info_destroy_shell" );

}





// Global parameter so that we can pass another value to the below
// function from the Station_info() function.  We need to be able to
// pass this value off to the Station_data() function for special
// operations like moves, where objects are on top of each other.
//
XtPointer station_info_select_global = NULL;





/*
 *  Station Info Selection PopUp window: Quit with selected station
 */
void Station_info_select_destroy_shell(Widget widget, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer callData) {
    int i,x;
    char *temp;
    char temp2[50];
    XmString *list;
    int found;
    //Widget shell = (Widget) clientData;

    found=0;

begin_critical_section(&db_station_popup_lock, "db.c:Station_info_select_destroy_shell" );

    if (db_station_popup) {
        XtVaGetValues(station_list,XmNitemCount,&i,XmNitems,&list,NULL);

        for (x=1; x<=i;x++) {
            if (XmListPosSelected(station_list,x)) {
                found=1;
                if (XmStringGetLtoR(list[(x-1)],XmFONTLIST_DEFAULT_TAG,&temp))
                    x=i+1;
            }
        }

        // DK7IN ?? should we not first close the PopUp, then call Station_data ??
        if (found) {
            xastir_snprintf(temp2, sizeof(temp2), "%s", temp);

            // Call it with the global parameter at the last, so we
            // can pass special parameters down that we couldn't
            // directly pass to Station_info_select_destroy_shell().
            Station_data(widget, temp2, station_info_select_global);

            // Clear the global variable so that nothing else calls
            // it with the wrong parameter
            station_info_select_global = NULL;

            XtFree(temp);
        }
/*
        // This section of code gets rid of the Station Chooser.  Frank wanted to
        // be able to leave the Station Chooser up after selection so that other
        // stations could be selected, therefore I commented this out.
        XtPopdown(shell);                   // Get rid of the station chooser popup here
        (void)XFreePixmap(XtDisplay(appshell),SiS_icon0);
        (void)XFreePixmap(XtDisplay(appshell),SiS_icon);
        XtDestroyWidget(shell);             // and here
        db_station_popup = (Widget)NULL;    // and here
*/
    }

end_critical_section(&db_station_popup_lock, "db.c:Station_info_select_destroy_shell" );

}





/*
 *  Station Info PopUp
 *  if only one station in view it shows the data with Station_data()
 *  otherwise we get a selection box
 *  clientData will be non-null if we wish to drop through to the object->modify
 *  or Assign Tactical Call dialogs.
 */
void Station_info(Widget w, /*@unused@*/ XtPointer clientData, XtPointer calldata) {
    DataRow *p_station;
    DataRow *p_found;
    int num_found = 0;
    unsigned long min_diff_x, diff_x, min_diff_y, diff_y;
    XmString str_ptr;
    unsigned int n;
    Atom delw;
    static Widget pane, form, button_ok, button_cancel;
    Arg al[20];                    /* Arg List */
    register unsigned int ac = 0;           /* Arg Count */


    busy_cursor(appshell);

    min_diff_y = scale_y * 20;  // Pixels each way in y-direction.
    min_diff_x = scale_x * 20;  // Pixels each way in x-direction.
    p_found = NULL;
    p_station = n_first;

    // Here we just count them.  We go through the same type of code
    // again later if we find more than one station.
    while (p_station != NULL) {    // search through database for nearby stations

        if ( ( (p_station->flag & ST_INVIEW) != 0)
                && ok_to_draw_station(p_station) ) { // only test stations in view

            if (!altnet || is_altnet(p_station)) {

                // Here we calculate diff in terms of XX pixels,
                // changed into lat/long values.  This keeps the
                // affected rectangle the same at any zoom level.
                // scale_y/scale_x is Xastir units/pixel.  Xastir
                // units are in 1/100 of a second.  If we want to go
                // 10 pixels in any direction (roughly, scale_x
                // varies by latitude), then we want (10 * scale_y),
                // and (10 * scale_x) if we want to make a very
                // accurate square.

                diff_y = (unsigned long)( labs((y_lat_offset+(menu_y*scale_y))
                                          - p_station->coord_lat));

                diff_x = (unsigned long)( labs((x_long_offset+(menu_x*scale_x))
                                          - p_station->coord_lon));

                // If the station fits within our bounding box,
                // count it 
                if ((diff_y < min_diff_y) && (diff_x < min_diff_x)) {
                    p_found = p_station;
                    num_found++;
                }
            }
        }
        p_station = p_station->n_next;
    }

    if (p_found != NULL) {  // We found at least one station

        if (num_found == 1) {
            // We only found one station, so it's easy
            Station_data(w,p_found->call_sign,clientData);
        }
        else {  // We found more: create dialog to choose a station

            // Set up this global variable so that we can pass it
            // off to Station_data from the
            // Station_info_select_destroy_shell() function above.
            // Without this global variable we don't have enough
            // parameters passed to the final routine, so we can't
            // move an object that is on top of another.  With it,
            // we can.
            station_info_select_global = clientData;

            if (db_station_popup != NULL)
                Station_info_destroy_shell(db_station_popup, db_station_popup, NULL);

begin_critical_section(&db_station_popup_lock, "db.c:Station_info" );

            if (db_station_popup == NULL) {
                // Set up a selection box:
                db_station_popup = XtVaCreatePopupShell(langcode("STCHO00001"),xmDialogShellWidgetClass,Global.top,
                                    XmNdeleteResponse,XmDESTROY,
                                    XmNdefaultPosition, FALSE,
                                    XmNbackground, colors[0xff],
                                    NULL);

                pane = XtVaCreateWidget("Station_info pane",xmPanedWindowWidgetClass, db_station_popup,
                            XmNbackground, colors[0xff],
                            NULL);

                form =  XtVaCreateWidget("Station_info form",xmFormWidgetClass, pane,
                            XmNfractionBase, 5,
                            XmNbackground, colors[0xff],
                            XmNautoUnmanage, FALSE,
                            XmNshadowThickness, 1,
                            NULL);

                // Attach buttons first to the bottom of the form,
                // so that we'll be able to stretch this thing
                // vertically to see all the callsigns.
                //
                button_ok = XtVaCreateManagedWidget("Info",xmPushButtonGadgetClass, form,
                                XmNtopAttachment, XmATTACH_NONE,
                                XmNbottomAttachment, XmATTACH_FORM,
                                XmNbottomOffset, 5,
                                XmNleftAttachment, XmATTACH_POSITION,
                                XmNleftPosition, 1,
                                XmNrightAttachment, XmATTACH_POSITION,
                                XmNrightPosition, 2,
                                XmNnavigationType, XmTAB_GROUP,
                                NULL);

                button_cancel = XtVaCreateManagedWidget(langcode("UNIOP00003"),xmPushButtonGadgetClass, form,
                                    XmNtopAttachment, XmATTACH_NONE,
                                    XmNbottomAttachment, XmATTACH_FORM,
                                    XmNbottomOffset, 5,
                                    XmNleftAttachment, XmATTACH_POSITION,
                                    XmNleftPosition, 3,
                                    XmNrightAttachment, XmATTACH_POSITION,
                                    XmNrightPosition, 4,
                                    XmNnavigationType, XmTAB_GROUP,
                                    NULL);

                XtAddCallback(button_cancel, XmNactivateCallback, Station_info_destroy_shell, db_station_popup);
                XtAddCallback(button_ok, XmNactivateCallback, Station_info_select_destroy_shell, db_station_popup);


                /*set args for color */
                ac = 0;
                XtSetArg(al[ac], XmNbackground, colors[0xff]); ac++;
                XtSetArg(al[ac], XmNvisibleItemCount, 6); ac++;
                XtSetArg(al[ac], XmNtraversalOn, TRUE); ac++;
                XtSetArg(al[ac], XmNshadowThickness, 3); ac++;
                XtSetArg(al[ac], XmNselectionPolicy, XmSINGLE_SELECT); ac++;
                XtSetArg(al[ac], XmNscrollBarPlacement, XmBOTTOM_RIGHT); ac++;
                XtSetArg(al[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
                XtSetArg(al[ac], XmNtopOffset, 5); ac++;
                XtSetArg(al[ac], XmNbottomAttachment, XmATTACH_WIDGET); ac++;
                XtSetArg(al[ac], XmNbottomWidget, button_ok); ac++;
                XtSetArg(al[ac], XmNbottomOffset, 5); ac++;
                XtSetArg(al[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
                XtSetArg(al[ac], XmNrightOffset, 5); ac++;
                XtSetArg(al[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
                XtSetArg(al[ac], XmNleftOffset, 5); ac++;

                station_list = XmCreateScrolledList(form,"Station_info list",al,ac);

// DK7IN: I want to add the symbol in front of the call...
        // icon
        SiS_icon0 = XCreatePixmap(XtDisplay(appshell),RootWindowOfScreen(XtScreen(appshell)),20,20,
                    DefaultDepthOfScreen(XtScreen(appshell)));
        SiS_icon  = XCreatePixmap(XtDisplay(appshell),RootWindowOfScreen(XtScreen(appshell)),20,20,
                    DefaultDepthOfScreen(XtScreen(appshell)));
/*      SiS_symb  = XtVaCreateManagedWidget("Station_info icon", xmLabelWidgetClass, ob_form1,
                            XmNlabelType,               XmPIXMAP,
                            XmNlabelPixmap,             SiS_icon,
                            XmNbackground,              colors[0xff],
                            XmNleftAttachment,          XmATTACH_FORM,
                            XmNtopAttachment,           XmATTACH_FORM,
                            XmNbottomAttachment,        XmATTACH_NONE,
                            XmNrightAttachment,         XmATTACH_NONE,
                            NULL);
*/

                /*fprintf(stderr,"What station\n");*/
                n = 1;
                p_station = n_first;
                while (p_station != NULL) {    // search through database for nearby stations

                    if ( ( (p_station->flag & ST_INVIEW) != 0)
                            && ok_to_draw_station(p_station) ) { // only test stations in view

                        if (!altnet || is_altnet(p_station)) {

                            diff_y = (unsigned long)( labs((y_lat_offset+(menu_y*scale_y))
                                                      - p_station->coord_lat));

                            diff_x = (unsigned long)( labs((x_long_offset+(menu_x*scale_x))
                                                      - p_station->coord_lon));

                            // If the station fits within our
                            // bounding box, count it.
                            if ((diff_y < min_diff_y) && (diff_x < min_diff_x)) {
                                /*fprintf(stderr,"Station %s\n",p_station->call_sign);*/
                                XmListAddItem(station_list, str_ptr = XmStringCreateLtoR(p_station->call_sign,
                                    XmFONTLIST_DEFAULT_TAG), (int)n++);
                                XmStringFree(str_ptr);
                            }
                        }
                    }
                    p_station = p_station->n_next;
                }


                pos_dialog(db_station_popup);

                delw = XmInternAtom(XtDisplay(db_station_popup),"WM_DELETE_WINDOW", FALSE);
                XmAddWMProtocolCallback(db_station_popup, delw, Station_info_destroy_shell, (XtPointer)db_station_popup);

                XtManageChild(form);
                XtManageChild(station_list);
                XtVaSetValues(station_list, XmNbackground, colors[0x0f], NULL);
                XtManageChild(pane);

                XtPopup(db_station_popup,XtGrabNone);

                // Move focus to the Cancel button.  This appears to highlight t
                // button fine, but we're not able to hit the <Enter> key to
                // have that default function happen.  Note:  We _can_ hit the
                // <SPACE> key, and that activates the option.
                XmProcessTraversal(button_cancel, XmTRAVERSE_CURRENT);

            }

end_critical_section(&db_station_popup_lock, "db.c:Station_info" );

        }
    }
}





int heard_via_tnc_in_past_hour(char *call) {
    DataRow *p_station;
    int in_hour;

    in_hour=0;
    if (search_station_name(&p_station,call,1)) {  // find call

        // Check the heard_via_tnc_last_time timestamp.  This is a
        // timestamp that is saved each time a station is heard via
        // RF.  It is initially set to 0.  It does not get reset
        // when a packet comes in via a non-TNC interface.
        //
        if (p_station->heard_via_tnc_last_time) {   // non-zero entry

            // Should we check to see if the last packet was message
            // capable?

            // Decide whether it was heard on a TNC interface within
            // the hour
            in_hour = (int)((p_station->heard_via_tnc_last_time+3600l) > sec_now());

            if(debug_level & 2)
                fprintf(stderr, "Call %s: %ld %ld ok %d\n",
                    call,
                    (long)(p_station->heard_via_tnc_last_time),
                    (long)sec_now(),
                    in_hour);

        }
        else {
            if (debug_level & 2)
                fprintf(stderr,"Call %s Not heard via tnc\n",call);
        }
    }
    else {
        if (debug_level & 2)
            fprintf(stderr,"IG:station not found\n");
    }
    return(in_hour);
}





//////////////////////////////////// Weather Data //////////////////////////////////////////////////





/* valid characters for APRS weather data fields */
int is_aprs_chr(char ch) {

    if (isdigit((int)ch) || ch==' ' || ch=='.' || ch=='-')
    return(1);
    else
    return(0);
}





int count_filler_chars(char ch) {

    if (isdigit((int)ch) || ch==' ' || ch=='.' || ch=='-')
    return(1);
    else
    return(0);
}





/* check data format    123 ___ ... */
// We wish to count how many ' ' or '.' characters we find.  If it
// equals zero or the field width, it might be a weather field.  If
// not, then it might be part of a comment field or something else.
//
int is_weather_data(char *data, int len) {
    int ok = 1;
    int i;
    int count = 0;

    for (i=0;ok && i<len;i++)
        if (!is_aprs_chr(data[i]))
            ok = 0;

    // Count filler characters.  Must equal zero or field width to
    // be a weather field.  There doesn't appear to be a case where
    // a single period is allowed in any weather-related fields.
    //
    for (i=0;ok && i<len;i++) {
        if (data[i] == ' ' || data[i] == '.') {
            count++;
        }
    }
    if (count != 0 && count != len) {
        ok = 0;
    }

    return(ok);
}





// Extract single weather data item from "data".  Returns it in
// "temp".  Modifies "data" to remove the found data from the
// string.  Returns a 1 if found, 0 if not found.
//
// PE1DNN
// If the item is contained in the string but does not contain a
// value then regard the item as "not found" in the weather string.
//
int extract_weather_item(char *data, char type, int datalen, char *temp) {
    int i,ofs,found,len;


//fprintf(stderr,"%s\n",data);

    found=0;
    len = (int)strlen(data);
    for(ofs=0; !found && ofs<len-datalen; ofs++)      // search for start sequence
        if (data[ofs]==type) {
            found=1;
            if (!is_weather_data(data+ofs+1, datalen))
                found=0;
        }
    if (found) {   // ofs now points after type character
        substr(temp,data+ofs,datalen);
        for (i=ofs-1;i<len-datalen;i++)        // delete item from info field
            data[i] = data[i+datalen+1];
        if((temp[0] == ' ') || (temp[0] == '.')) {
            // found it, but it doesn't contain a value!
            // Clean up and report "not found" - PE1DNN
            temp[0] = '\0';
            found = 0;
        }
        else
        {
            if (debug_level & 2) {
                fprintf(stderr,"extract_weather_item: %s\n",temp);
            }
        }
    } else
        temp[0] = '\0';
    return(found);
}





// test-extract single weather data item from information field.  In
// other words:  Does not change the input string, but does test
// whether the data is present.  Returns a 1 if found, 0 if not
// found.
//
// PE1DNN
// If the item is contained in the string but does not contain a
// value then regard the item as "not found" in the weather string.
//
int test_extract_weather_item(char *data, char type, int datalen) {
    int ofs,found,len;

    found=0;
    len = (int)strlen(data);
    for(ofs=0; !found && ofs<len-datalen; ofs++)      // search for start sequence
        if (data[ofs]==type) {
            found=1;
            if (!is_weather_data(data+ofs+1, datalen))
                found=0;
        }

    // We really should test for numbers here (with an optional
    // leading '-'), and test across the length of the substring.
    //
    if(found && ((data[ofs+1] == ' ') || (data[ofs+1] == '.'))) {
        // found it, but it doesn't contain a value!
        // report "not found" - PE1DNN
        found = 0;
    }

    //fprintf(stderr,"test_extract: %c %d\n",type,found);
    return(found);
}





// DK7IN 77
// raw weather report            in information field
// positionless weather report   in information field
// complete weather report       with lat/lon
//  see APRS Reference page 62ff
//
// Added 'F' for Fuel Temp and 'f' for Fuel Moisture in order to
// decode these two new parameters used for RAWS weather station
// objects.
//
// By the time we call this function we've already extracted any
// time/position info at the beginning of the string.
//
int extract_weather(DataRow *p_station, char *data, int compr) {
    char time_data[MAX_TIME];
    char temp[5];
    int  ok = 1;
    WeatherRow *weather;
    char course[4];
    char speed[4];
    int in_knots = 0;

//WE7U
// Try copying the string to a temporary string, then do some
// extractions to see if a few weather items are present?  This
// would allow us to have the weather items in any order, and if
// enough of them were present, we consider it to be a weather
// packet?  We'd need to qualify all of the data to make sure we had
// the proper number of digits for each.  The trick is to make sure
// we don't decide it's a weather packet if it's not.  We don't know
// what people might send in packets in the future.

    if (compr) {        // compressed position report
        // Look for weather data in fixed locations first
        if (strlen(data) >= 8
                && data[0] =='g' && is_weather_data(&data[1],3)
                && data[4] =='t' && is_weather_data(&data[5],3)) {

            // Snag WX course/speed from compressed position data.
            // This speed is in knots.  This assumes that we've
            // already extracted speed/course from the compressed
            // packet.  extract_comp_position() extracts
            // course/speed as well.
            xastir_snprintf(speed,
                sizeof(speed),
                "%s",
                p_station->speed);
            xastir_snprintf(course,
                sizeof(course),
                "%s",
                p_station->course);
            in_knots = 1;

            //fprintf(stderr,"Found compressed wx\n");
        }
        // Look for weather data in non-fixed locations (RAWS WX
        // Stations?)
        else if ( strlen(data) >= 8
                && test_extract_weather_item(data,'g',3)
                && test_extract_weather_item(data,'t',3) ) {

            // Snag WX course/speed from compressed position data.
            // This speed is in knots.  This assumes that we've
            // already extracted speed/course from the compressed
            // packet.  extract_comp_position() extracts
            // course/speed as well.
            xastir_snprintf(speed,
                sizeof(speed),
                "%s",
                p_station->speed);
            xastir_snprintf(course,
                sizeof(course),
                "%s",
                p_station->course);
            in_knots = 1;

            //fprintf(stderr,"Found compressed WX in non-fixed locations! %s:%s\n",
            //    p_station->call_sign,data);

        }
        else {  // No weather data found
            ok = 0;

            //fprintf(stderr,"No compressed wx\n");
        }
    } else {    // Look for non-compressed weather data
        // Look for weather data in defined locations first
        if (strlen(data)>=15 && data[3]=='/'
                && is_weather_data(data,3) && is_weather_data(&data[4],3)
                && data[7] =='g' && is_weather_data(&data[8], 3)
                && data[11]=='t' && is_weather_data(&data[12],3)) {    // Complete Weather Report

            // Get speed/course.  Speed is in knots.
            (void)extract_speed_course(data,speed,course);
            in_knots = 1;

            // Either one not found?  Try again.
            if ( (speed[0] == '\0') || (course[0] == '\0') ) {

                // Try to get speed/course from 's' and 'c' fields
                // (another wx format).  Speed is in mph.
                (void)extract_weather_item(data,'c',3,course); // wind direction (in degrees)
                (void)extract_weather_item(data,'s',3,speed);  // sustained one-minute wind speed (in mph)
                in_knots = 0;
            }

            //fprintf(stderr,"Found Complete Weather Report\n");
        }
        // Look for date/time and weather in fixed locations first
        else if (strlen(data)>=16
                && data[0] =='c' && is_weather_data(&data[1], 3)
                && data[4] =='s' && is_weather_data(&data[5], 3)
                && data[8] =='g' && is_weather_data(&data[9], 3)
                && data[12]=='t' && is_weather_data(&data[13],3)) { // Positionless Weather Data
//fprintf(stderr,"Found positionless wx data\n");
            // Try to snag speed/course out of first 7 bytes.  Speed
            // is in knots.
            (void)extract_speed_course(data,speed,course);
            in_knots = 1;

            // Either one not found?  Try again.
            if ( (speed[0] == '\0') || (course[0] == '\0') ) {
//fprintf(stderr,"Trying again for course/speed\n");
                // Also try to get speed/course from 's' and 'c' fields
                // (another wx format)
                (void)extract_weather_item(data,'c',3,course); // wind direction (in degrees)
                (void)extract_weather_item(data,'s',3,speed);  // sustained one-minute wind speed (in mph)
                in_knots = 0;
            }

            //fprintf(stderr,"Found weather\n");
        }
        // Look for weather data in non-fixed locations (RAWS WX
        // Stations?)
        else if (strlen (data) >= 16
                && test_extract_weather_item(data,'h',2)
                && test_extract_weather_item(data,'g',3)
                && test_extract_weather_item(data,'t',3) ) {

            // Try to snag speed/course out of first 7 bytes.  Speed
            // is in knots.
            (void)extract_speed_course(data,speed,course);
            in_knots = 1;

            // Either one not found?  Try again.
            if ( (speed[0] == '\0') || (course[0] == '\0') ) {

                // Also try to get speed/course from 's' and 'c' fields
                // (another wx format)
                (void)extract_weather_item(data,'c',3,course); // wind direction (in degrees)
                (void)extract_weather_item(data,'s',3,speed);  // sustained one-minute wind speed (in mph)
                in_knots = 0;
            }
 
            //fprintf(stderr,"Found WX in non-fixed locations!  %s:%s\n",
            //    p_station->call_sign,data);
        }
        else {  // No weather data found
            ok = 0;

            //fprintf(stderr,"No wx found\n");
        }
    }

    if (ok) {
        ok = get_weather_record(p_station);     // get existing or create new weather record
    }

    if (ok) {
        weather = p_station->weather_data;

        // Copy into weather speed variable.  Convert knots to mph
        // if necessary.
        if (in_knots) {
            xastir_snprintf(weather->wx_speed,
                sizeof(weather->wx_speed),
                "%03.0f",
                atoi(speed) * 1.1508);  // Convert knots to mph
        }
        else {
            // Already in mph.  Copy w/no conversion.
            xastir_snprintf(weather->wx_speed,
                sizeof(weather->wx_speed),
                "%s",
                speed);
        }

        xastir_snprintf(weather->wx_course,
            sizeof(weather->wx_course),
            "%s",
            course);

        if (compr) {        // course/speed was taken from normal data, delete that
            // fix me: we delete a potential real speed/course now
            // we should differentiate between normal and weather data in compressed position decoding...
//            p_station->speed_time[0]     = '\0';
            p_station->speed[0]          = '\0';
            p_station->course[0]         = '\0';
        }

        (void)extract_weather_item(data,'g',3,weather->wx_gust);      // gust (peak wind speed in mph in the last 5 minutes)

        (void)extract_weather_item(data,'t',3,weather->wx_temp);      // temperature (in deg Fahrenheit), could be negative

        (void)extract_weather_item(data,'r',3,weather->wx_rain);      // rainfall (1/100 inch) in the last hour

        (void)extract_weather_item(data,'p',3,weather->wx_prec_24);   // rainfall (1/100 inch) in the last 24 hours

        (void)extract_weather_item(data,'P',3,weather->wx_prec_00);   // rainfall (1/100 inch) since midnight

        if (extract_weather_item(data,'h',2,weather->wx_hum))         // humidity (in %, 00 = 100%)
                xastir_snprintf(weather->wx_hum, sizeof(weather->wx_hum), "%03d",(atoi(weather->wx_hum)+99)%100+1);

        if (extract_weather_item(data,'b',5,weather->wx_baro))  // barometric pressure (1/10 mbar / 1/10 hPascal)
            xastir_snprintf(weather->wx_baro,
                sizeof(weather->wx_baro),
                "%0.1f",
                (float)(atoi(weather->wx_baro)/10.0));

        // If we parsed a speed/course, a second 's' parameter means
        // snowfall.  Try to parse it, but only in the case where
        // we've parsed speed out of this packet already.
        if ( (speed[0] != '\0') && (course[0] != '\0') ) {
            (void)extract_weather_item(data,'s',3,weather->wx_snow);      // snowfall, inches in the last 24 hours
        }

        (void)extract_weather_item(data,'L',3,temp);                  // luminosity (in watts per square meter) 999 and below

        (void)extract_weather_item(data,'l',3,temp);                  // luminosity (in watts per square meter) 1000 and above

        (void)extract_weather_item(data,'#',3,temp);                  // raw rain counter

        (void)extract_weather_item(data,'F',3,weather->wx_fuel_temp); // Fuel Temperature in °F (RAWS)

        if (extract_weather_item(data,'f',2,weather->wx_fuel_moisture))// Fuel Moisture (RAWS) (in %, 00 = 100%)
            xastir_snprintf(weather->wx_fuel_moisture,
                sizeof(weather->wx_fuel_moisture),
                "%03d",
                (atoi(weather->wx_fuel_moisture)+99)%100+1);

//    extract_weather_item(data,'w',3,temp);                          // ?? text wUII

    // now there should be the name of the weather station...

        // Create a timestamp from the current time
        xastir_snprintf(weather->wx_time,
            sizeof(weather->wx_time),
            "%s",
            get_time(time_data));

        // Set the timestamp in the weather record so that we can
        // decide whether or not to "ghost" the weather data later.
        weather->wx_sec_time=sec_now();
//        weather->wx_data=1;  // we don't need this

//        case ('.'):/* skip */
//            wx_strpos+=4;
//            break;

//        default:
//            wx_done=1;
//            weather->wx_type=data[wx_strpos];
//            if(strlen(data)>wx_strpos+1)
//                xastir_snprintf(weather->wx_station,    
//                    sizeof(weather->wx_station),
//                    "%s",
//                    data+wx_strpos+1);
//            break;
    }
    return(ok);
}





// Initial attempt at decoding tropical storm, tropical depression,
// and hurricane data.
//
// This data can be in an Object report, but can also be in an Item
// or position report.
// "/TS" = Tropical Storm
// "/HC" = Hurricane
// "/TD" = Tropical Depression
// "/TY" = Typhoon
// "/ST" = Super Typhoon
// "/SC" = Severe Cyclone

// The symbol will be either "\@" for current position, or "/@" for
// predicted position.
//
int extract_storm(DataRow *p_station, char *data, int compr) {
    char time_data[MAX_TIME];
    int  ok = 1;
    WeatherRow *weather;
    char course[4];
    char speed[4];  // Speed in knots
    char *p, *p2;


// Should probably encode the storm type in the weather object and
// print it out in plain text in the Station Info dialog.

    if ((p = strstr(data, "/TS")) != NULL) {
        // We have a Tropical Storm
//fprintf(stderr,"Tropical Storm! %s\n",data);
    }
    else if ((p = strstr(data, "/TD")) != NULL) {
        // We have a Tropical Depression
//fprintf(stderr,"Tropical Depression! %s\n",data);
    }
    else if ((p = strstr(data, "/HC")) != NULL) {
        // We have a Hurricane
//fprintf(stderr,"Hurricane! %s\n",data);
    }
    else if ((p = strstr(data, "/TY")) != NULL) {
        // We have a Typhoon
//fprintf(stderr,"Hurricane! %s\n",data);
    }
    else if ((p = strstr(data, "/ST")) != NULL) {
        // We have a Super Typhoon
//fprintf(stderr,"Hurricane! %s\n",data);
    }
    else if ((p = strstr(data, "/SC")) != NULL) {
        // We have a Severe Cyclone
//fprintf(stderr,"Hurricane! %s\n",data);
    }
    else {  // Not one of the three we're trying to decode
        ok = 0;
        return(ok);
    }

//fprintf(stderr,"\n%s\n",data);

    // Back up 7 spots to try to extract the next items
    p2 = p - 7;
    if (p2 >= data) {
        // Attempt to extract course/speed.  Speed in knots.
        if (!extract_speed_course(p2,speed,course)) {
            // No speed/course to extract
//fprintf(stderr,"No speed/course found\n");
            ok = 0;
            return(ok);
        }
    }
    else {  // Not enough characters for speed/course.  Must have
            // guessed wrong on what type of data it is.
//fprintf(stderr,"No speed/course found 2\n");
        ok = 0;
        return(ok);
    }


//fprintf(stderr,"%s\n",data);

    if (ok) {

        // If we got this far, we have speed/course and know what type
        // of storm it is.
//fprintf(stderr,"Speed: %s, Course: %s\n",speed,course);

        ok = get_weather_record(p_station);     // get existing or create new weather record
    }

    if (ok) {
//        p_station->speed_time[0]     = '\0';

        p_station->weather_data->wx_storm = 1;  // We found a storm

        // Note that speed is in knots.  If we were stuffing it into
        // "wx_speed" we'd have to convert it to MPH.
        if (strcmp(speed,"   ") != 0 && strcmp(speed,"...") != 0) {
            xastir_snprintf(p_station->speed,
                sizeof(p_station->speed),
                "%s",
                speed);
        }
        else {
            p_station->speed[0] = '\0';
        }

        if (strcmp(course,"   ") != 0 && strcmp(course,"...") != 0)
            xastir_snprintf(p_station->course,
                sizeof(p_station->course),
                "%s",
                course);
        else
            p_station->course[0] = '\0';
 
        weather = p_station->weather_data;
 
        p2++;   // Skip the description text, "/TS", "/HC", "/TD", "/TY", "/ST", or "/SC"

        // Extract the sustained wind speed in knots
        if(extract_weather_item(p2,'/',3,weather->wx_speed))
            // Convert from knots to MPH
            xastir_snprintf(weather->wx_speed,
                sizeof(weather->wx_speed),
                "%0.1f",
                (float)(atoi(weather->wx_speed)) * 1.1508);

//fprintf(stderr,"%s\n",data);

        // Extract gust speed in knots
        if (extract_weather_item(p2,'^',3,weather->wx_gust)) // gust (peak wind speed in knots)
            // Convert from knots to MPH
            xastir_snprintf(weather->wx_gust,
                sizeof(weather->wx_gust),
                "%0.1f",
                (float)(atoi(weather->wx_gust)) * 1.1508);

//fprintf(stderr,"%s\n",data);

        // Pressure is already in millibars/hPa.  No conversion
        // needed.
        if (extract_weather_item(p2,'/',4,weather->wx_baro))  // barometric pressure (1/10 mbar / 1/10 hPascal)
            xastir_snprintf(weather->wx_baro,
                sizeof(weather->wx_baro),
                "%0.1f",
                (float)(atoi(weather->wx_baro)));

//fprintf(stderr,"%s\n",data);

        (void)extract_weather_item(p2,'>',3,weather->wx_hurricane_radius); // Nautical miles

//fprintf(stderr,"%s\n",data);

        (void)extract_weather_item(p2,'&',3,weather->wx_trop_storm_radius); // Nautical miles

//fprintf(stderr,"%s\n",data);

        (void)extract_weather_item(p2,'%',3,weather->wx_whole_gale_radius); // Nautical miles

//fprintf(stderr,"%s\n",data);

        // Create a timestamp from the current time
        xastir_snprintf(weather->wx_time,
            sizeof(weather->wx_time),
            "%s",
            get_time(time_data));

        // Set the timestamp in the weather record so that we can
        // decide whether or not to "ghost" the weather data later.
        weather->wx_sec_time=sec_now();
    }
    return(ok);
}





/*
 * Look for information about other points associated with this station.
 * If found, compute the coordinates and save the information in the
 * station structure.
 * KG4NBB
 */
// If remove_string == 0, don't remove the string from the comment
// field.  Useful for objects/items where we need to retransmit the
// string unchanged.

#define MULTI_DEBUG 2048
#define LBRACE '{'
#define RBRACE '}'
#define START_STR " }"

static void extract_multipoints(DataRow *p_station,
        char *data,
        int type,
        int remove_string) {
    // If they're in there, the multipoints start with the
    // sequence <space><rbrace><lower><digit> and end with a <lbrace>.
    // In addition, there must be no spaces in there, and there
    // must be an even number of characters (after the lead-in).

    char *p, *p2;
    int found = 0;
    char *end = data + (strlen(data) - 7);  // 7 == 3 lead-in chars, plus 2 points
    int data_size = strlen(data);


//fprintf(stderr,"Data: %s\t\t", data);

    p_station->num_multipoints = 0;

    /*
    for (p = data; !found && p <= end; ++p) {
        if (*p == ' ' && *(p+1) == RBRACE && islower((int)*(p+2)) && isdigit((int)*(p+3)) && 
                            (p2 = strchr(p+4, LBRACE)) != NULL && ((p2 - p) % 2) == 1) {
            found = 1;
        }
    }
    */

    // Start looking at the beginning of the data.

    p = data;

    // Look for the opening string.

    while (!found && p < end && (p = strstr(p, START_STR)) != NULL) {
        // The opening string was found. Check the following information.

        if (islower((int)*(p+2)) && isdigit((int)*(p+3)) && (p2 = strchr(p+4, LBRACE)) != NULL && ((p2 - p) % 2) == 1) {
            // It all looks good!

            found = 1;
        }
        else {
            // The following characters are not right. Advance and
            // look again.

            ++p;
        }
    }

    if (found) {
        long multiplier;
        double d;
        char *m_start = p;    // Start of multipoint string
        char ok = 1;
 

        if (debug_level & MULTI_DEBUG)
            fprintf(stderr,"station %s contains \"%s\"\n", p_station->call_sign, p);

        // The second character (the lowercase) indicates additional style information,
        // such as color, line type, etc.

        p_station->style = *(p+2);

        // The third character (the digit) indicates the way the points should be
        // used. They may be used to draw a closed polygon, a series of line segments,
        // etc.

        p_station->type = *(p+3);

        // The fourth character indicates the scale of the coordinates that
        // follow. It may range from '!' to 'z'. The value represents the
        // unit of measure (1, 0.1, 0.001, etc., in degrees) used in the offsets.
        //
        // Use the following formula to convert the char to the value:
        // (10 ^ ((c - 33) / 20)) / 10000 degrees
        //
        // Finally we have to convert to Xastir units. Xastir stores coordinates
        // as hudredths of seconds. There are 360,000 of those per degree, so we
        // need to multiply by that factor so our numbers will be converted to
        // Xastir units.

        p = p + 4;

        if (*p < '!' || *p > 'z') {
            fprintf(stderr,"extract_multipoints: invalid scale character %d\n", *p);
            ok = 0; // Failure
        }
        else {
            d = (double)(*p);
            d = pow(10.0, ((d - 33) / 20)) / 10000.0 * 360000.0;
            multiplier = (long)d;
            if (debug_level & MULTI_DEBUG)
                fprintf(stderr,"    multiplier factor is: %c %d %f (%ld)\n", *p, *p, d, multiplier);

            ++p;

            // The remaining characters are in pairs. Each pair is the
            // offset lat and lon for one of the points. (The offset is
            // from the actual location of the object.) Convert each
            // character to its numeric value and save it.

            while (*p != LBRACE && p_station->num_multipoints < MAX_MULTIPOINTS) {
                // The characters are in the range '"' (34 decimal) to 'z' (122). They
                // encode values in the range -44 to +44. To convert to the correct
                // value 78 is subtracted from the character's value.

                int lat_val = *p - 78;
                int lon_val = *(p+1) - 78;

                // Check for correct values.

                if (lon_val < -44 || lon_val > 44 || lat_val < -44 || lat_val > 44) {
                    char temp[MAX_LINE_SIZE+1];
                    int i;

                    // Filter the string so we don't send strange
                    // chars to the xterm
                    for (i = 0; i < (int)strlen(data); i++) {
                        temp[i] = data[i] & 0x7f;
                        if ( (temp[i] < 0x20) || (temp[i] > 0x7e) )
                            temp[i] = ' ';
                    }
                    temp[strlen(data)] = '\0';
                    
                    fprintf(stderr,"extract_multipoints: invalid value in (filtered) \"%s\": %d,%d\n",
                        temp,
                        lat_val,
                        lon_val);

                    p_station->num_multipoints = 0;     // forget any points we already set
                    ok = 0; // Failure to decode
                    break;
                }

                // Malloc the storage area for this if we don't have
                // it yet.
                if (p_station->multipoint_data == NULL) {
//fprintf(stderr, "Malloc'ing MultipointRow record, %s\n", p_station->call_sign);
                    p_station->multipoint_data = malloc(sizeof(MultipointRow));
                    if (p_station->multipoint_data == NULL) {
                        p_station->num_multipoints = 0;
                        fprintf(stderr,"Couldn't malloc MultipointRow'\n");
                        return;
                    }
                }
 
                if (debug_level & MULTI_DEBUG)
                    fprintf(stderr,"computed offset %d,%d\n", lat_val, lon_val);

                // Add the offset to the object's position to obtain the position of the point.
                // Note that we're working in Xastir coordinates, and in North America they
                // are exactly opposite to lat/lon (larger numbers are farther east and south).
                // An offset with a positive value means that the point should be north and/or
                // west of the object, so we have to *subtract* the offset to get the correct
                // placement in Xastir coordinates.
                // TODO: Consider what we should do in the other geographic quadrants. Should we
                // check here for the correct sign of the offset? Or should the program that
                // creates the offsets take that into account?

                p_station->multipoint_data->multipoints[p_station->num_multipoints][0]
                    = p_station->coord_lon - (lon_val * multiplier);
                p_station->multipoint_data->multipoints[p_station->num_multipoints][1]
                    = p_station->coord_lat - (lat_val * multiplier);

                if (debug_level & MULTI_DEBUG)
                    fprintf(stderr,
                        "computed point %ld, %ld\n",
                        p_station->multipoint_data->multipoints[p_station->num_multipoints][0],
                        p_station->multipoint_data->multipoints[p_station->num_multipoints][1]);
                p += 2;
                ++p_station->num_multipoints;
            }   // End of while loop
        }

        if (ok && remove_string) {
            // We've successfully decoded a multipoint object?
            // Remove the multipoint strings (and the sequence
            // number at the end if present) from the data string.
            // m_start points to the first character (a space).  'p'
            // should be pointing at the LBRACE character.

            // Make 'p' point to just after the end of the chars
            while ( (p < data+strlen(data)) && (*p != ' ') ) {
                p++;
            }
            // The string that 'p' points to now may be empty

            // Truncate "data" at the starting brace - 1
            *m_start = '\0';

            // Now we have two strings inside "data".  Copy the 2nd
            // string directly onto the end of the first.
            strncat(data, p, data_size+1);

            // The multipoint string and sequence number should be
            // erased now from "data".
//fprintf(stderr,"New Data: %s\n", data);
        }

        if (debug_level & MULTI_DEBUG)
            fprintf(stderr,"    station has %d points\n", p_station->num_multipoints);
    }
}

#undef MULTI_DEBUG



////////////////////////////////////////////////////////////////////////////////////////////////////



void init_weather(WeatherRow *weather) {    // clear weather data

    weather->wx_sec_time             = (time_t)0;
    weather->wx_storm                = 0;
    weather->wx_time[0]              = '\0';
    weather->wx_course[0]            = '\0';
    weather->wx_speed[0]             = '\0';
    weather->wx_speed_sec_time       = 0; // ??
    weather->wx_gust[0]              = '\0';
    weather->wx_hurricane_radius[0]  = '\0';
    weather->wx_trop_storm_radius[0] = '\0';
    weather->wx_whole_gale_radius[0] = '\0';
    weather->wx_temp[0]              = '\0';
    weather->wx_rain[0]              = '\0';
    weather->wx_rain_total[0]        = '\0';
    weather->wx_snow[0]              = '\0';
    weather->wx_prec_24[0]           = '\0';
    weather->wx_prec_00[0]           = '\0';
    weather->wx_hum[0]               = '\0';
    weather->wx_baro[0]              = '\0';
    weather->wx_fuel_temp[0]         = '\0';
    weather->wx_fuel_moisture[0]     = '\0';
    weather->wx_type                 = '\0';
    weather->wx_station[0]           = '\0';
}





int get_weather_record(DataRow *fill) {    // get or create weather storage
    int ok=1;

    if (fill->weather_data == NULL) {      // new weather data, allocate storage and init
        fill->weather_data = malloc(sizeof(WeatherRow));
        if (fill->weather_data == NULL) {
            fprintf(stderr,"Couldn't allocate memory in get_weather_record()\n");
            ok = 0;
        }
        else {
            init_weather(fill->weather_data);
        }
    }
    return(ok);
}





int delete_weather(DataRow *fill) {    // delete weather storage, if allocated

    if (fill->weather_data != NULL) {
        free(fill->weather_data);
        fill->weather_data = NULL;
        return(1);
    }
    return(0);
}





int delete_multipoints(DataRow *fill) { // delete multipoint storage, if allocated

    if (fill->multipoint_data != NULL) {
//fprintf(stderr,"Removing multipoint data, %s\n", fill->call_sign);
        free(fill->multipoint_data);
        fill->multipoint_data = NULL;
        fill->num_multipoints = 0;
        return(1);
    }
    return(0);
}



////////////////////////////////////////// Trails //////////////////////////////////////////////////



/*
 *  See if current color is defined as active trail color
 */
int trail_color_active(int color_index) {

    // this should be made configurable...
    // to select trail colors to use
    
    return(1);          // accept this color
}





/*
 *  Get new trail color for a call
 */
int new_trail_color(char *call) {
    int color, found, i;

    // If MY_TRAIL_DIFF_COLOR is defined to be a 0, then we'll
    // assign one special color to every SSID from our callsign.  If
    // 1, they get the next color available (round-robin style) just
    // like all the other stations.
    //
    if (is_my_call(call,MY_TRAIL_DIFF_COLOR)) {
        color = MY_TRAIL_COLOR;    // It's my call, so use special color
    } else {
        // all other callsigns get some other color out of the color table
        color = current_trail_color;
        for(i=0,found=0;!found && i<max_trail_colors;i++) {
            color = (color + 1) % max_trail_colors; // try next color in list
            // skip special and or inactive colors.
            if (color != MY_TRAIL_COLOR && trail_color_active(color))
                found = 1;
        }
        if (found)
            current_trail_color = color;        // save it for next time
        else
            color = current_trail_color;        // keep old color
    }
    return(color);
}





//
// Store one trail point.  Allocate storage for the new data.
//
// We now store track data in a doubly-linked list.  Each record has a
// pointer to the previous and the next record in the list.  The main
// station record has a pointer to the oldest and the newest end of the
// chain, and the chain can be traversed in either order.
//
int store_trail_point(DataRow *p_station, long lon, long lat, time_t sec, char *alt, char *speed, char *course, short stn_flag) {
    char flag;
    TrackRow *ptr;

    //fprintf(stderr,"store_trail_point: %s\n",p_station->call_sign);

    if (debug_level & 256) {
        fprintf(stderr,"store_trail_point: for %s\n", p_station->call_sign);
    }

    // Allocate storage for the new track point
    ptr = malloc(sizeof(TrackRow));
    if (ptr == NULL) {
        if (debug_level & 256) {
            fprintf(stderr,"store_trail_point: MALLOC failed for trail.\n");
        }
        return(0); // Failed due to malloc
    }
 
    // Check whether we have any track data saved
    if (p_station->newest_trackpoint == NULL) {
        // new trail, do initialization

        if (debug_level & 256) {
            fprintf(stderr,"Creating new trail.\n");
        }
        tracked_stations++;

        // Assign a new trail color 'cuz it's a new trail
        p_station->trail_color = new_trail_color(p_station->call_sign);
    }

    // Start linking the record to the new end of the chain
    ptr->prev = p_station->newest_trackpoint;   // Link to record or NULL
    ptr->next = NULL;   // Newest end of chain

    // Have an older record already?
    if (p_station->newest_trackpoint != NULL) { // Yes
        p_station->newest_trackpoint->next = ptr;
    }
    else {  // No, this is our first record
        p_station->oldest_trackpoint = ptr;
    }

    // Link it in as our newest record
    p_station->newest_trackpoint = ptr;

    if (debug_level & 256) {
        fprintf(stderr,"store_trail_point: Storing data for %s\n", p_station->call_sign);
    }

    ptr->trail_long_pos = lon;
    ptr->trail_lat_pos  = lat;
    ptr->sec            = sec;

    if (alt[0] != '\0')
            ptr->altitude = atoi(alt)*10;
    else            
            ptr->altitude = -99999l;

    if (speed[0] != '\0')
            ptr->speed  = (long)(atof(speed)*18.52);
    else
            ptr->speed  = -1;

    if (course[0] != '\0')
            ptr->course = (int)(atof(course) + 0.5);    // Poor man's rounding
    else
            ptr->course = -1;

    flag = '\0';                    // init flags

    if ((stn_flag & ST_DIRECT) != 0)
            flag |= TR_LOCAL;           // set "local" flag

    if (ptr->prev != NULL) {    // we have at least two points...
        // Check whether distance between points is too far.  We
        // must convert from degrees to the Xastir coordinate system
        // units, which are 100th of a second.
        if (    abs(lon - ptr->prev->trail_long_pos) > (trail_segment_distance * 60*60*100) ||
                abs(lat - ptr->prev->trail_lat_pos)  > (trail_segment_distance * 60*60*100) ) {

            // Set "new track" flag if there's
            // "trail_segment_distance" degrees or more between
            // points.  Originally was hard-coded to one degree, now
            // set by a slider in the timing dialog.
            flag |= TR_NEWTRK;
        }
        else {
            // Check whether trail went above our maximum time
            // between points.  If so, don't draw segment.
            if (abs(sec - ptr->prev->sec) > (trail_segment_time *60)) {

                // Set "new track" flag if long delay between
                // reception of two points.  Time is set by a slider
                // in the timing dialog.
                flag |= TR_NEWTRK;
            }
        }
    }
    else {
        // Set "new track" flag for first point received.
        flag |= TR_NEWTRK;
    }
    ptr->flag = flag;

    return(1);  // We succeeded
}





/*
 *  Check if current packet is a delayed echo
 */
int is_trailpoint_echo(DataRow *p_station) {
    int packets = 1;
    time_t checktime;
    char temp[50];
    TrackRow *ptr;


    // Check whether we're to skip checking for dupes (reading in
    // objects/items from file is one such case).
    //
    if (skip_dupe_checking) {
        return(0);  // Say that it isn't an echo
    }

    // Start at newest end of linked list and compare.  Return if we're
    // beyond the checktime.
    ptr = p_station->newest_trackpoint;

    if (ptr == NULL)
        return(0);  // first point couldn't be an echo

    checktime = p_station->sec_heard - TRAIL_ECHO_TIME*60;

    while (ptr != NULL) {

        if (ptr->sec < checktime)
            return(0);  // outside time frame, no echo found

        if ((p_station->coord_lon == ptr->trail_long_pos)
                && (p_station->coord_lat == ptr->trail_lat_pos)
                && (p_station->speed == '\0' || ptr->speed < 0
                        || (long)(atof(p_station->speed)*18.52) == ptr->speed)
                        // current: char knots, trail: long 0.1m (-1 is undef)
                && (p_station->course == '\0' || ptr->course <= 0
                        || atoi(p_station->course) == ptr->course)
                        // current: char, trail: int (-1 is undef)
                && (p_station->altitude == '\0' || ptr->altitude <= -99999l
                        || atoi(p_station->altitude)*10 == ptr->altitude)) {
                        // current: char, trail: int (-99999l is undef)
            if (debug_level & 1) {
                fprintf(stderr,"delayed echo for %s",p_station->call_sign);
                convert_lat_l2s(p_station->coord_lat, temp, sizeof(temp), CONVERT_HP_NORMAL);
                fprintf(stderr," at %s",temp);
                convert_lon_l2s(p_station->coord_lon, temp, sizeof(temp), CONVERT_HP_NORMAL);
                fprintf(stderr," %s, already heard %d packets ago\n",temp,packets);
            }
            return(1);              // we found a delayed echo
        }
        ptr = ptr->prev;
        packets++;
    }
    return(0);                      // no echo found
}





//
//  Expire trail points.
//
// We now store track data in a doubly-linked list.  Each record has a
// pointer to the previous and the next record in the list.  The main
// station record has a pointer to the oldest and the newest end of the
// chain, and the chain can be traversed in either order.  We use
// this to advantage by adding records at one end of the list and
// expiring them at the other.
//
void expire_trail_points(DataRow *p_station, time_t sec) {
    int i = 0;
    int done = 0;
    TrackRow *ptr;

    //fprintf(stderr,"expire_trail_points: %s\n",p_station->call_sign);

    if (debug_level & 256) {
        fprintf(stderr,"expire_trail_points: %s\n",p_station->call_sign);
    }

    // Check whether we have any track data saved
    if (p_station->oldest_trackpoint == NULL) {
        return;     // Nothing to expire
    }

    // Iterate from oldest->newest trackpoints
    while (!done && p_station->oldest_trackpoint != NULL) {
        ptr = p_station->oldest_trackpoint;
        if ( (ptr->sec + sec) >= sec_now() ) {
            // New trackpoint, within expire time.  Quit checking
            // the rest of the trackpoints for this station.
            done++;
        }
        else {
            //fprintf(stderr,"Found old trackpoint\n");

            // Track too old.  Unlink this trackpoint and free it.
            p_station->oldest_trackpoint = ptr->next;

            // End of chain in this direction
            if (p_station->oldest_trackpoint != NULL) {
                p_station->oldest_trackpoint->prev = NULL;
            }
            else {
                p_station->newest_trackpoint = NULL;
            }

            // Free up the space used by the expired trackpoint
            free(ptr);

            //fprintf(stderr,"Free'ing a trackpoint\n");

            i++;

            // Reduce our count of mobile stations if the size of
            // the track just went to zero.
            if (p_station->oldest_trackpoint == NULL)
                tracked_stations--;
        }
    }

    if ( (debug_level & 256) && i )
        fprintf(stderr,"expire_trail_points: %d trackpoints free'd for %s\n",i,p_station->call_sign);
}





/*
 *  Delete comment records and free memory
 */
int delete_comments_and_status(DataRow *fill) {

    // If the pointers are empty, we're done
    if (       (fill->comment_data == NULL)
            && (fill->status_data  == NULL) ) {
        return(0);
    }

    if (fill->comment_data != NULL) {   // We have comment records
        CommentRow *ptr;
        CommentRow *ptr_next;

        ptr = fill->comment_data;
        ptr_next = ptr->next;
        while (ptr != NULL) {
            // Free the actual text string that we malloc'ed
            if (ptr->text_ptr != NULL) {
                free(ptr->text_ptr);
            }
            free(ptr);
            ptr = ptr_next; // Advance to next record
            if (ptr != NULL)
                ptr_next = ptr->next;
            else
                ptr_next = NULL;
        }
    }
    if (fill->status_data != NULL) {   // We have status records
        CommentRow *ptr;
        CommentRow *ptr_next;

        ptr = fill->status_data;
        ptr_next = ptr->next;
        while (ptr != NULL) {
            // Free the actual text string that we malloc'ed
            if (ptr->text_ptr != NULL) {
                free(ptr->text_ptr);
            }
            free(ptr);
            ptr = ptr_next; // Advance to next record
            if (ptr != NULL)
                ptr_next = ptr->next;
            else
                ptr_next = NULL;
        }
    }
    return(1);
}





/*
 *  Delete trail and free memory
 */
int delete_trail(DataRow *fill) {

    if (fill->newest_trackpoint != NULL) {
        TrackRow *current;
        TrackRow *next;

        // Free the TrackRow records
        current = fill->oldest_trackpoint;
        while (current != NULL) {
            next = current->next;
            free(current);
            current = next;
        }

        fill->oldest_trackpoint = NULL;
        fill->newest_trackpoint = NULL;
        tracked_stations--;
        return(1);
    }
    return(0);
}





/*
 *  Draw trail on screen.  If solid=1, draw type LineSolid, else
 *  draw type LineOnOffDash.
 *
 *  If label_all_trackpoints=1, add the callsign next to each
 *  trackpoint.  We may modify this and just add the callsign at the
 *  start/end of each new track segment.
 *
 */
void draw_trail(Widget w, DataRow *fill, int solid) {
    char short_dashed[2]  = {(char)1,(char)5};
    char medium_dashed[2] = {(char)5,(char)5};
    long lat0, lon0, lat1, lon1;        // trail segment points
    long marg_lat, marg_lon;
    int col_trail, col_dot;
    XColor rgb;
/*    Colormap cmap;  KD6ZWR - Now set in main() */
    long brightness;
    char flag1;
    TrackRow *ptr;


    if (!ok_to_draw_station(fill))
        return;

    // Expire old trackpoints first.  We use the
    // remove-station-from-display time as the expire time for
    // trackpoints.  This can be set from the Configure->Defaults
    // dialog.
    expire_trail_points(fill, sec_clear);

    ptr = fill->newest_trackpoint;

    // Trail should have at least two points
    if ( (ptr != NULL) && (ptr->prev != NULL) ) {

        if (debug_level & 256) {
            fprintf(stderr,"draw_trail called for %s with %s.\n",
                fill->call_sign, (solid? "Solid" : "Non-Solid"));
        }

        col_trail = trail_colors[fill->trail_color];

        // define color of position dots in trail
        rgb.pixel = col_trail;
/*      cmap = DefaultColormap(XtDisplay(w), DefaultScreen(XtDisplay(w)));  KD6ZWR - Now set in main() */
        XQueryColor(XtDisplay(w),cmap,&rgb);

        brightness = (long)(0.3*rgb.red + 0.55*rgb.green + 0.15*rgb.blue);
        if (brightness > 32000) {
            col_dot = trail_colors[0x05];   // black dot on light trails
        } else {
            col_dot = trail_colors[0x06];   // white dot on dark trail
        }

        if (solid)
            // Used to be "JoinMiter" and "CapButt" below
            (void)XSetLineAttributes(XtDisplay(w), gc, 3, LineSolid, CapRound, JoinRound);
        else {
            // Another choice is LineDoubleDash
            (void)XSetLineAttributes(XtDisplay(w), gc, 3, LineOnOffDash, CapRound, JoinRound);
            (void)XSetDashes(XtDisplay(w), gc, 0, short_dashed , 2);
        }

        marg_lat = screen_height * scale_y;                 // Set up area for acceptable trail points
        if (marg_lat < TRAIL_POINT_MARGIN*60*100)           // on-screen plus 50% margin
            marg_lat = TRAIL_POINT_MARGIN*60*100;           // but at least a minimum margin
        marg_lon = screen_width  * scale_x;
        if (marg_lon < TRAIL_POINT_MARGIN*60*100)
            marg_lon = TRAIL_POINT_MARGIN*60*100;

        // Traverse linked list of trail points from newest to
        // oldest
        while ( (ptr != NULL) && (ptr->prev != NULL) ) {
            lon0 = ptr->trail_long_pos;         // Trail segment start
            lat0 = ptr->trail_lat_pos;
            lon1 = ptr->prev->trail_long_pos;   // Trail segment end
            lat1 = ptr->prev->trail_lat_pos;
            flag1 = ptr->flag; // Are we at the start of a new trail?

            if ( (abs(lon0 - mid_x_long_offset) < marg_lon) &&  // trail points have to
                (abs(lon1 - mid_x_long_offset) < marg_lon) &&  // be in margin area
                (abs(lat0 - mid_y_lat_offset)  < marg_lat) &&
                (abs(lat1 - mid_y_lat_offset)  < marg_lat) ) {

                if ((flag1 & TR_NEWTRK) == '\0') {
                    // Possible drawing problems, if numbers too big ????
                    lon0 = (lon0 - x_long_offset) / scale_x;    // check arguments for
                    lat0 = (lat0 - y_lat_offset)  / scale_y;    // XDrawLine()
                    lon1 = (lon1 - x_long_offset) / scale_x;
                    lat1 = (lat1 - y_lat_offset)  / scale_y;

                    if (abs(lon0) < 32700 && abs(lon1) < 32700 &&
                            abs(lat0) < 32700 && abs(lat1) < 32700) {
                        // draw trail segment
                        (void)XSetForeground(XtDisplay(w),gc,col_trail);
                        (void)XDrawLine(XtDisplay(w),pixmap_final,gc,lon0,lat0,lon1,lat1);
                        // draw position point itself
                        (void)XSetForeground(XtDisplay(w),gc,col_dot);
                        (void)XDrawPoint(XtDisplay(w),pixmap_final,gc,lon0,lat0);

                        // Draw the callsign to go with the point if
                        // label_all_trackpoints=1.
                        //
                        if (Display_.callsign && Display_.label_all_trackpoints) {

                            // The last position already gets its
                            // callsign string drawn, plus that gets
                            // shifted based on other parameters.
                            // Draw both points of all line segments
                            // except that one.  This will result in
                            // strings getting drawn twice at times,
                            // but they overlay on top of each other
                            // so no big deal.
                            //
                            if (ptr != fill->newest_trackpoint) {
                                draw_nice_string(da,
                                    pixmap_final,
                                    letter_style,
                                    lon0+10,
                                    lat0,
                                    fill->call_sign,
                                    0x08,
                                    0x0f,
                                    strlen(fill->call_sign));

                                draw_nice_string(da,
                                    pixmap_final,
                                    letter_style,
                                    lon1+10,
                                    lat1,
                                    fill->call_sign,
                                    0x08,
                                    0x0f,
                                    strlen(fill->call_sign));
                            }
                        }

                    }
/*                  (void)XDrawLine(XtDisplay(w), pixmap_final, gc,         // draw trail segment
                        (lon0-x_long_offset)/scale_x,(lat0-y_lat_offset)/scale_y,
                        (lon1-x_long_offset)/scale_x,(lat1-y_lat_offset)/scale_y);
*/
                }
            }
            ptr = ptr->prev;
        }
        (void)XSetDashes(XtDisplay(w), gc, 0, medium_dashed , 2);
    }
    else if (debug_level & 256) {
        fprintf(stderr,"Trail for %s does not contain 2 or more points.\n",
            fill->call_sign);
    }
}





// DK7IN: there should be some library functions for the next two,
//        but I don't have any documentation while being in holidays...
void month2str(int month, char *str, int str_size) {

    switch (month) {
        case  0: xastir_snprintf(str,str_size,"Jan"); break;
        case  1: xastir_snprintf(str,str_size,"Feb"); break;
        case  2: xastir_snprintf(str,str_size,"Mar"); break;
        case  3: xastir_snprintf(str,str_size,"Apr"); break;
        case  4: xastir_snprintf(str,str_size,"May"); break;
        case  5: xastir_snprintf(str,str_size,"Jun"); break;
        case  6: xastir_snprintf(str,str_size,"Jul"); break;
        case  7: xastir_snprintf(str,str_size,"Aug"); break;
        case  8: xastir_snprintf(str,str_size,"Sep"); break;
        case  9: xastir_snprintf(str,str_size,"Oct"); break;
        case 10: xastir_snprintf(str,str_size,"Nov"); break;
        case 11: xastir_snprintf(str,str_size,"Dec"); break;
        default: xastir_snprintf(str,str_size,"   "); break;
    }
}





void wday2str(int wday, char *str, int str_size) {

    switch (wday) {
        case  0: xastir_snprintf(str,str_size,"Sun"); break;
        case  1: xastir_snprintf(str,str_size,"Mon"); break;
        case  2: xastir_snprintf(str,str_size,"Tue"); break;
        case  3: xastir_snprintf(str,str_size,"Wed"); break;
        case  4: xastir_snprintf(str,str_size,"Thu"); break;
        case  5: xastir_snprintf(str,str_size,"Fri"); break;
        case  6: xastir_snprintf(str,str_size,"Sat"); break;
        default: xastir_snprintf(str,str_size,"   "); break;
    }
}





/*
 *  Export trail point to file
 */
void exp_trailpos(FILE *f,long lat,long lon,time_t sec,long speed,int course,long alt,int newtrk) {
    struct tm *time;
    char temp[12+1];
    char month[3+1];
    char wday[3+1];

    if (newtrk)
        fprintf(f,"\nN  New Track Start\n");

    // DK7IN: The format may change in the near future !
    //        Are there any standards? I want to be able to be compatible to
    //        GPS data formats (e.g. G7TO) for easy interchange from/to GPS
    //        How should we present undefined data? (speed/course/altitude)
    convert_lat_l2s(lat, temp, sizeof(temp), CONVERT_UP_TRK);
    fprintf(f,"T  %s",temp);

    convert_lon_l2s(lon, temp, sizeof(temp), CONVERT_UP_TRK);
    fprintf(f," %s",temp);

    time  = gmtime(&sec);
    month2str(time->tm_mon, month, sizeof(month));
    wday2str(time->tm_wday, wday, sizeof(wday));
    fprintf(f," %s %s %02d %02d:%02d:%02d %04d",wday,month,time->tm_mday,time->tm_hour,time->tm_min,time->tm_sec,time->tm_year+1900);

    if (alt > -99999l)
        fprintf(f,"  %5.0fm",(float)(alt/10.0));
    else        // undefined
        fprintf(f,"        ");

    if (speed >= 0)
        fprintf(f," %4.0fkm/h",(float)(speed/10.0));
    else        // undefined
        fprintf(f,"          ");

    if (course >= 0)                    // DK7IN: is 0 undefined ?? 1..360 ?
        fprintf(f," %3d°\n",course);
    else        // undefined
        fprintf(f,"     \n");
}





/*
 *  Export trail for one station to file
 */
void exp_trailstation(FILE *f, DataRow *p_station) {
    long lat0, lon0;
    int newtrk;
    time_t sec;
    long speed;         // 0.1km/h
    int  course;        // degrees
    long alt;           // 0.1m
    TrackRow *current;

    if (p_station->origin == NULL || p_station->origin[0] == '\0')
        fprintf(f,"\n#C %s\n",p_station->call_sign);
    else
        fprintf(f,"\n#O %s %s\n",p_station->call_sign,p_station->origin);

    newtrk = 1;

    current = p_station->oldest_trackpoint;

    // A trail must have at least two points:  One in the struct,
    // and one in the tracklog.  If the station only has one point,
    // there won't be a tracklog.  If the station has moved, then
    // it'll have both.

    if (current != NULL) {  // We have trail points, loop through
                            // them.  Skip the most current position
                            // because it is included in the
                            // tracklog (if we have a tracklog!).

        while (current != NULL) {
            lon0   = current->trail_long_pos;                   // Trail segment start
            lat0   = current->trail_lat_pos;
            sec    = current->sec;
            speed  = current->speed;
            course = current->course;
            alt    = current->altitude;
            if ((current->flag & TR_NEWTRK) != '\0')
                newtrk = 1;
 
            exp_trailpos(f,lat0,lon0,sec,speed,course,alt,newtrk);

            newtrk = 0;

            // Advance to the next point
            current = current->next;
        }
    }
    else {  // We don't have any tracklog, so write out the most
            // current position only.
    
        if (p_station->altitude[0] != '\0')
            alt = atoi(p_station->altitude)*10;
        else            
            alt = -99999l;

        if (p_station->speed[0] != '\0')
            speed = (long)(atof(p_station->speed)*18.52);
        else
            speed = -1;

        if (p_station->course[0] != '\0')
            course = atoi(p_station->course);
        else
            course = -1;

        exp_trailpos(f,p_station->coord_lat,p_station->coord_lon,p_station->sec_heard,speed,course,alt,newtrk);
    }

    fprintf(f,"\n");
}





//
// Export trail data for one or all stations to file
//
// If p_station == NULL, store all stations, else store only one
// station.
//
void export_trail(DataRow *p_station) {
    char file[420];
    FILE *f;
    time_t sec;
    struct tm *time;
    int storeall;

    sec = sec_now();
    time  = gmtime(&sec);

    if (p_station == NULL)
        storeall = 1;
    else
        storeall = 0;

    if (storeall) {
        // define filename for storing all station
        xastir_snprintf(file, sizeof(file),
            "%s/%04d%02d%02d-%02d%02d%02d.trk",
            get_user_base_dir("tracklogs"),
            time->tm_year+1900,
            time->tm_mon+1,
            time->tm_mday,
            time->tm_hour,
            time->tm_min,
            time->tm_sec);
    }
    else {
        // define filename for current station
        xastir_snprintf(file, sizeof(file), "%s/%s.trk", get_user_base_dir("tracklogs"), p_station->call_sign);
    }

    // create or open file
    (void)filecreate(file);     // create empty file if it doesn't exist
    // DK7IN: owner should better be set to user, it is now root with kernel AX.25!

    f=fopen(file,"a");          // open file for append
    if (f != NULL) {

        fprintf(f,
            "# WGS-84 tracklog created by Xastir %04d/%02d/%02d %02d:%02d\n",
            time->tm_year+1900,
            time->tm_mon+1,
            time->tm_mday,
            time->tm_hour,
            time->tm_min);

        if (storeall) {
            p_station = n_first;
            while (p_station != NULL) {
                exp_trailstation(f,p_station);
                p_station = p_station->n_next;
            }
        }
        else {
            exp_trailstation(f,p_station);
        }
        (void)fclose(f);
    } else
        fprintf(stderr,"Couldn't create or open tracklog file %s\n",file);
}




//////////////////////////////////////  Station storage  ///////////////////////////////////////////

// Station storage is done in a double-linked list. In fact there are two such
// pointer structures, one for sorting by name and one for sorting by time.
// We store both the pointers to the next and to the previous elements.  DK7IN

/*
 *  Setup station storage structure
 */
void init_station_data(void) {

    stations = 0;                       // empty station list
    n_first = NULL;                     // pointer to next element in name sorted list
    n_last  = NULL;                     // pointer to previous element in name sorted list
    t_first = NULL;                     // pointer to oldest element in time sorted list
    t_last  = NULL;                     // pointer to newest element in time sorted list
    last_sec = sec_now();               // check value for detecting changed seconds in time
    next_time_sn = 0;                   // serial number for unique time index
    current_trail_color = 0x00;         // first trail color used will be 0x01
    last_station_remove = sec_now();    // last time we checked for stations to remove
    track_station_on = 0;               // no tracking at startup
}





/*
 *  Initialize station data
 */        
void init_station(DataRow *p_station) {
    // the list pointers should already be set
    p_station->oldest_trackpoint  = NULL;         // no trail
    p_station->newest_trackpoint  = NULL;         // no trail
    p_station->trail_color        = 0;
    p_station->weather_data       = NULL;         // no weather
    p_station->coord_lat          = 0l;           //  90°N  \ undefined
    p_station->coord_lon          = 0l;           // 180°W  / position
    p_station->pos_amb            = 0;            // No ambiguity
    p_station->call_sign[0]       = '\0';         // ?????
    p_station->tactical_call_sign = NULL;
    p_station->sec_heard          = 0;
    p_station->time_sn            = 0;
    p_station->flag               = 0;            // set all flags to inactive
    p_station->object_retransmit  = -1;           // transmit forever
    p_station->last_transmit_time = sec_now();    // Used for object/item decaying algorithm
    p_station->transmit_time_increment = 0;       // Used in data_add()
    p_station->record_type        = '\0';
    p_station->data_via           = '\0';         // L local, T TNC, I internet, F file
    p_station->heard_via_tnc_port = 0;
    p_station->heard_via_tnc_last_time = 0;
    p_station->last_port_heard    = 0;
    p_station->num_packets        = 0;
    p_station->aprs_symbol.aprs_type = '\0';
    p_station->aprs_symbol.aprs_symbol = '\0';
    p_station->aprs_symbol.special_overlay = '\0';
    p_station->aprs_symbol.area_object.type           = AREA_NONE;
    p_station->aprs_symbol.area_object.color          = AREA_GRAY_LO;
    p_station->aprs_symbol.area_object.sqrt_lat_off   = 0;
    p_station->aprs_symbol.area_object.sqrt_lon_off   = 0;
    p_station->aprs_symbol.area_object.corridor_width = 0;
//    p_station->station_time_type  = '\0';
    p_station->origin[0]          = '\0';        // no object
    p_station->packet_time[0]     = '\0';
    p_station->node_path_ptr      = NULL;
    p_station->pos_time[0]        = '\0';
//    p_station->altitude_time[0]   = '\0';
    p_station->altitude[0]        = '\0';
//    p_station->speed_time[0]      = '\0';
    p_station->speed[0]           = '\0';
    p_station->course[0]          = '\0';
    p_station->bearing[0]         = '\0';
    p_station->NRQ[0]             = '\0';
    p_station->power_gain[0]      = '\0';
    p_station->signal_gain[0]     = '\0';
    p_station->signpost[0]        = '\0';
    p_station->probability_min[0] = '\0';
    p_station->probability_max[0] = '\0';
//    p_station->station_time[0]    = '\0';
    p_station->sats_visible[0]    = '\0';
    p_station->status_data        = NULL;
    p_station->comment_data       = NULL;
    p_station->df_color           = -1;
    
    // Show that there are no other points associated with this
    // station. We could also zero all the entries of the 
    // multipoints[][] array, but nobody should be looking there
    // unless this is non-zero.
    // KG4NBB
    
    p_station->num_multipoints = 0;
    p_station->multipoint_data = NULL;
}





/*
 *  Remove element from name ordered list
 */
void remove_name(DataRow *p_rem) {      // todo: return pointer to next element
    int update_shortcuts = 0;
    int hash_key;   // We use a 14-bit hash key


    // Do a quick check to see if we're removing a station record
    // that is pointed to by our pointer shortcuts array.
    // If so, update our pointer shortcuts after we're done.
    //
    // We create the hash key out of the lower 7 bits of the first
    // two characters, creating a 14-bit key (1 of 16384)
    //
    hash_key = (int)((p_rem->call_sign[0] & 0x7f) << 7);
    hash_key = hash_key | (int)(p_rem->call_sign[1] & 0x7f);

    if (station_shortcuts[hash_key] == p_rem) {
        // Yes, we're trying to remove a record that a hash key
        // directly points to.  We'll need to redo that hash key
        // after we remove the record.
        update_shortcuts++;
    }


    // Proceed to the station record removal
    if (p_rem->n_prev == NULL)          // first element
        n_first = p_rem->n_next;
    else
        p_rem->n_prev->n_next = p_rem->n_next;

    if (p_rem->n_next == NULL)          // last element
        n_last = p_rem->n_prev;
    else
        p_rem->n_next->n_prev = p_rem->n_prev;


    // Update our pointer shortcuts.  Pass the removed hash_key to
    // the function so that we can try to redo just that hash_key
    // pointer.
    if (update_shortcuts) {
//fprintf(stderr,"\t\t\t\t\t\tRemoval of hash key: %i\n", hash_key);

        // The -1 tells the function to redo all of the hash table
        // pointers because we deleted one of them.  Later we could
        // optimize this so that only the specific pointer is fixed
        // up.
        station_shortcuts_update_function(-1, NULL);
    }
}





/*
 *  Remove element from time ordered list
 */
void remove_time(DataRow *p_rem) {      // todo: return pointer to next element
    if (p_rem->t_prev == NULL)          // first element
        t_first = p_rem->t_next;
    else
        p_rem->t_prev->t_next = p_rem->t_next;

    if (p_rem->t_next == NULL)          // last element
        t_last = p_rem->t_prev;
    else
        p_rem->t_next->t_prev = p_rem->t_prev;
}





/*
 *  Insert existing element into name ordered list before p_name
 */
void insert_name(DataRow *p_new, DataRow *p_name) {
    p_new->n_next = p_name;
    if (p_name == NULL) {               // add to end of list
        p_new->n_prev = n_last;
        if (n_last == NULL)             // add to empty list
            n_first = p_new;
        else
            n_last->n_next = p_new;
        n_last = p_new;
    } else {
        p_new->n_prev = p_name->n_prev;
        if (p_name->n_prev == NULL)     // add to begin of list
            n_first = p_new;
        else
            p_name->n_prev->n_next = p_new;
        p_name->n_prev = p_new;
    }
}





/*
 *  Insert existing element into time ordered list before p_time
 *  The p_new record ends up being on the "older" side of p_time when
 *  all done inserting (closer in the list to the t_first pointer).
 */
void insert_time(DataRow *p_new, DataRow *p_time) {
    p_new->t_next = p_time;
    if (p_time == NULL) {               // add to end of list (becomes newest station)
        p_new->t_prev = t_last;         // connect to previous end of list
        if (t_last == NULL)             // if list empty, create list
            t_first = p_new;            // it's now our only station on the list
        else
            t_last->t_next = p_new;     // list not empty, link original last record to our new one
        t_last = p_new;                 // end of list (newest record pointer) points to our new record
    } else {                            // Else we're inserting into the middle of the list somewhere
        p_new->t_prev = p_time->t_prev;
        if (p_time->t_prev == NULL)     // add to begin of list (new record becomes oldest station)
            t_first = p_new;
        else
            p_time->t_prev->t_next = p_new; // else 
        p_time->t_prev = p_new;
    }
}





/*
 *  Free station memory for one entry
 */
void delete_station_memory(DataRow *p_del) {
    remove_name(p_del);
    remove_time(p_del);
    free(p_del);
    stations--;
}





/*
 *  Create new uninitialized element in station list
 *  and insert it before p_name and p_time entries
 */
/*@null@*/ DataRow *insert_new_station(DataRow *p_name, DataRow *p_time) {
    DataRow *p_new;

    p_new = (DataRow *)malloc(sizeof(DataRow));
    if (p_new != NULL) {                // we really got the memory
        p_new->call_sign[0] = '\0';     // just to be sure
        insert_name(p_new,p_name);      // insert element into name ordered list
        insert_time(p_new,p_time);      // insert element into time ordered list
    }
    if (p_new == NULL)
        fprintf(stderr,"ERROR: we got no memory for station storage\n");
    return(p_new);                      // return pointer to new element
}





/*
 *  Create new initialized element for call in station list
 *  and insert it before p_name and p_time entries
 */
/*@null@*/ DataRow *add_new_station(DataRow *p_name, DataRow *p_time, char *call) {
    DataRow *p_new;
    int hash_key;   // We use a 14-bit hash key


    if (call[0] == '\0') {
        // Do nothing.  No update needed.  Callsign is empty.
        return(NULL);
    }

//fprintf(stderr,"Adding new station: %s\n",call);
  
    p_new = insert_new_station(p_name,p_time);  // allocate memory

    if (p_new == NULL) {
        // Couldn't allocate space for the station
        return(NULL);
    }

    init_station(p_new);                    // initialize new station record
    xastir_snprintf(p_new->call_sign,
        sizeof(p_new->call_sign),
        "%s",
        call);
    stations++;

    // Do some quick checks to see if we just inserted a new hash
    // key or inserted at the beginning of a hash key (making the
    // old pointer incorrect).  If so, update our pointers to match.

    // We create the hash key out of the lower 7 bits of the first
    // two characters, creating a 14-bit key (1 of 16384)
    //
    hash_key = (int)((call[0] & 0x7f) << 7);
    hash_key = hash_key | (int)(call[1] & 0x7f);

    if (station_shortcuts[hash_key] == NULL) {
        // New hash key entry point found.  Fill in the pointer.
//fprintf(stderr,"New hash key: %i, call: %s\n",
//    hash_key,
//    call);

        station_shortcuts_update_function(hash_key, p_new);
    }
    else if (p_new->n_prev == NULL) {
        // We just inserted at the beginning of the list.  Assume
        // that we inserted at the beginning of our hash_key
        // segment.
//fprintf(stderr,"Start of list hash_key: %i, call: %s\n",
//    hash_key,
//    call);

        station_shortcuts_update_function(hash_key, p_new);
    }
    else {
        // Check whether either of the first two chars of the new
        // callsign and the previous callsign are different.  If so,
        // we need to update the hash table entry for our new record
        // 'cuz we're at the start of a new hash table entry.
        if (p_new->n_prev->call_sign[0] != call[0]
                || p_new->n_prev->call_sign[1] != call[1]) {
//fprintf(stderr,"Hash segment start: %i, call: %s\n",
//    hash_key,
//    call);

            station_shortcuts_update_function(hash_key, p_new);
        }
    }

//if (p_new->n_prev != NULL) {
//    fprintf(stderr,"\tprev: %s",
//        p_new->n_prev->call_sign);
//}

//if (p_new->n_next != NULL) {
//    fprintf(stderr,"\t\tnext: %s",
//        p_new->n_next->call_sign);
//}
//
//fprintf(stderr,"\n");

    return(p_new);                      // return pointer to new element
}





/*
 *  Move station record before p_time in time ordered list
 */
void move_station_time(DataRow *p_curr, DataRow *p_time) {

    if (p_curr != NULL) {               // need a valid record
        remove_time(p_curr);
        insert_time(p_curr,p_time);
    }
}





/*
 *  Move station record before p_name in name ordered list
 */
void move_station_name(DataRow *p_curr, DataRow *p_name) {

    if (p_curr != NULL) {               // need a valid record
        remove_name(p_curr);
        insert_name(p_curr,p_name);
    }
}





// Update all of the pointers so that they accurately reflect the
// current state of the station database.
//
// NOTE:  This part of the code could be made smarter so that the
// pointers are updated whenever they are found to be out of whack,
// instead of zeroing all of them and starting from scratch each
// time.  Alternate:  Follow the current pointer if non-NULL then go
// up/down the list to find the current switchover point between
// letters.
//
// Better:  Tie into the station insert function.  If a new letter
// is inserted, or a new station at the beginning of a letter group,
// run this function to keep things up to date.  That way we won't
// have to traverse in both directions to find a callsign in the
// search_station_name() function.
//
// If hash_key_in is -1, we need to redo all of the hash keys.  If
// it is between 0 and 16383, then we need to redo just that one
// hash key.  The 2nd parameter is either NULL for a removed record,
// or a pointer to a new station record in the case of an addition.
//
void station_shortcuts_update_function(int hash_key_in, DataRow *p_rem) {
    int ii;
    DataRow *ptr;
    int prev_hash_key = 0x0000;
    int hash_key;


// I just changed the function so that we can pass in the hash_key
// that we wish to update:  We should be able to speed things up by
// updating one hash key instead of all 16384 pointers.

    if ( (hash_key_in != -1)
            && (hash_key_in >= 0)
            && (hash_key_in < 16384) ) {

        // We're adding/changing a hash key entry
        station_shortcuts[hash_key_in] = p_rem;
//fprintf(stderr,"%i ",hash_key_in);
    }
    else {  // We're removing a hash key entry.

        // Clear and rebuild the entire hash table.

//??????????????????????????????????????????????????
    // Clear all of the pointers before we begin????
//??????????????????????????????????????????????????
        for (ii = 0; ii < 16384; ii++) {
            station_shortcuts[ii] = NULL;
        }

        ptr = n_first;  // Start of list


        // Loop through entire list, writing the pointer into the
        // station_shortcuts array whenever a new character is
        // encountered.  Do this until the end of the array or the end
        // of the list.
        //
        while ( (ptr != NULL) && (prev_hash_key < 16384) ) {

            // We create the hash key out of the lower 7 bits of the
            // first two characters, creating a 14-bit key (1 of 16384)
            //
            hash_key = (int)((ptr->call_sign[0] & 0x7f) << 7);
            hash_key = hash_key | (int)(ptr->call_sign[1] & 0x7f);

            if (hash_key > prev_hash_key) {

                // We found the next hash_key.  Store the pointer at the
                // correct location.
                if (hash_key < 16384) {
                    station_shortcuts[hash_key] = ptr;
//fprintf(stderr,"%i ", hash_key);
                }
                prev_hash_key = hash_key;
            }
            ptr = ptr->n_next;
        }
//fprintf(stderr,"\n");

    }

}





//
// Search station record by callsign
// Returns a station with a call equal or after the searched one
//
// We use a doubly-linked list for the stations, so we can traverse
// in either direction.  We also use a 14-bit hash table created
// from the first two letters of the call to dump us into the
// beginning of the correct area that may hold the callsign, which
// reduces search time quite a bit.  We end up doing a linear search
// only through a small area of the linked list.
//
// DK7IN:  I don't look at case, objects and internet names could
// have lower case.
//
int search_station_name(DataRow **p_name, char *call, int exact) {
    int kk;
    int hash_key;
    int result;
    int ok = 1;


    (*p_name) = n_first;                                // start of alphabet

    if (call[0] == '\0') {
        // If call we're searching for is empty, return n_first as
        // the pointer.
        return(0);
    }

    // We create the hash key out of the lower 7 bits of the first
    // two characters, creating a 14-bit key (1 of 16384)
    //
    hash_key = (int)((call[0] & 0x7f) << 7);
    hash_key = hash_key | (int)(call[1] & 0x7f);

    // Look for a match using hash table lookup
    //
    (*p_name) = station_shortcuts[hash_key];

    if ((*p_name) == NULL) {    // No hash-table entry found.
        int mm;

//fprintf(stderr,"No hash-table entry found: call:%s\n",call);


        // No index found for that letter.  Walk the array until
        // we find an entry that is filled.  That'll be our
        // potential insertion point (insertion into the list will
        // occur just ahead of the hash entry).
        for (mm = hash_key; mm < 16384; mm++) {
            if (station_shortcuts[mm] != NULL) {
                (*p_name) = station_shortcuts[mm];
                break;
            }
        }
    }
//    else {
//fprintf(stderr,"Hash key %d=%s, searching for call: %s\n",
//    hash_key,
//    (*p_name)->call_sign,
//    call);
//    }

    // If we got to this point, we either have a NULL pointer or a
    // real hash-table pointer entry.  A non-NULL pointer means that
    // we have a match for the lower seven bits of the first two
    // characters of the callsign.  Check the rest of the callsign,
    // and jump out of the loop if we get outside the linear search
    // area (if first two chars are different).

    kk = (int)strlen(call);

    // Search linearly through list.  Stop at end of list or break.
    while ( (*p_name) != NULL) {

        if (exact) {
            // Check entire string for exact match
            result = strcmp( call, (*p_name)->call_sign );
        }
        else {
            // Check first part of string for match
            result = strncmp( call, (*p_name)->call_sign, kk );
        }

        if (result < 0) {   // We went past the right location.
                            // We're done.
            ok = 0;
//fprintf(stderr,"Went past possible entry point, searching for call: %s\n",call);
            break;
        }
        else if (result == 0) { // Found a possible match
//fprintf(stderr,"Found possible match: list:%s call:%s\n",
//    (*p_name)->call_sign,
//    call);
            break;
        }
        else {  // Result > 0.  We haven't found it yet.
            (*p_name) = (*p_name)->n_next;  // Next element in list
        }
    }

    // Did we find anything?
    if ( (*p_name) == NULL) {
        ok = 0;
//fprintf(stderr,"End of list reached, call: %s\n",call);
        return(ok); // Nope.  No match found.
    }

    // If "exact" is set, check that the string lengths match as
    // well.  If not, we didn't find it.
    if (exact && ok && strlen((*p_name)->call_sign) != strlen(call))
        ok = 0;

    return(ok);         // if not ok: p_name points to correct insert position in name list
}





/*    
 *  Search station record by time and time serial number, serial ignored if -1
 *  Returns a station that is equal or older than the search criterium
 */
int search_station_time(DataRow **p_time, time_t heard, int serial) {
    int ok = 1;

    (*p_time) = t_last;                                 // newest station
    if (heard == 0) {                                   // we want the newest station
        if (t_last == 0)
            ok = 0;                                     // empty list
    }
    else {
        while((*p_time) != NULL) {                      // check time
            if ((*p_time)->sec_heard <= heard)          // compare
                break;                                  // found time or earlier
            (*p_time) = (*p_time)->t_prev;              // next element
        }
        // we now probably have found the entry
        if ((*p_time) != NULL && (*p_time)->sec_heard == heard) {
            // we got a match, but there may be more of them
            if (serial >= 0) {                          // check serial number, ignored if -1
                while((*p_time) != NULL) {              // for unique time index
                    if ((*p_time)->sec_heard == heard && (*p_time)->time_sn <= serial)  // compare
                        break;                          // found it (same time, maybe earlier SN)
                    if ((*p_time)->sec_heard < heard)   // compare
                        break;                          // found it (earlier time)
                    (*p_time) = (*p_time)->t_prev;      // consider next element
                }
                if ((*p_time) == NULL || (*p_time)->sec_heard != heard || (*p_time)->time_sn != serial)
                    ok = 0;                             // no perfect match
            }
        } else
            ok = 0;                                     // no perfect match
    }
    return(ok);
}





/*
 *  Get pointer to next station in name sorted list
 */
int next_station_name(DataRow **p_curr) {
    
    if ((*p_curr) == NULL)
        (*p_curr) = n_first;
    else
        (*p_curr) = (*p_curr)->n_next;
    if ((*p_curr) != NULL)
        return(1);
    else
        return(0);
}





/*
 *  Get pointer to previous station in name sorted list
 */
int prev_station_name(DataRow **p_curr) {
    
    if ((*p_curr) == NULL)
        (*p_curr) = n_last;
    else
        (*p_curr) = (*p_curr)->n_prev;
    if ((*p_curr) != NULL)
        return(1);
    else
        return(0);
}



    

/*
 *  Get pointer to newer station in time sorted list
 */
int next_station_time(DataRow **p_curr) {

    if ((*p_curr) == NULL)
        (*p_curr) = t_first;    // Grab oldest station if NULL passed to us???
    else
        (*p_curr) = (*p_curr)->t_next;  // Else grab newer station
    if ((*p_curr) != NULL)
        return(1);
    else
        return(0);
}





/*
 *  Get pointer to older station in time sorted list
 */
int prev_station_time(DataRow **p_curr) {
    
    if ((*p_curr) == NULL)
        (*p_curr) = t_last; // Grab newest station if NULL passed to us???
    else
        (*p_curr) = (*p_curr)->t_prev;
    if ((*p_curr) != NULL)
        return(1);
    else
        return(0);
}





/*
 *  Set flag for all stations in current view area or a margin area around it
 *  That are the stations we look at, if we want to draw symbols or trails
 */
void setup_in_view(void) {
    DataRow *p_station;
    long min_lat, max_lat;                      // screen borders plus space
    long min_lon, max_lon;                      // for trails from off-screen stations
    long marg_lat, marg_lon;                    // margin around screen

    marg_lat = (long)(3 * screen_height * scale_y/2);
    marg_lon = (long)(3 * screen_width  * scale_x/2);
    if (marg_lat < IN_VIEW_MIN*60*100)          // allow a minimum area, 
        marg_lat = IN_VIEW_MIN*60*100;          // there could be outside stations
    if (marg_lon < IN_VIEW_MIN*60*100)          // with trail parts on screen
        marg_lon = IN_VIEW_MIN*60*100;

    // Only screen view
    // min_lat = y_lat_offset  + (long)(screen_height * scale_y);
    // max_lat = y_lat_offset;
    // min_lon = x_long_offset;
    // max_lon = x_long_offset + (long)(screen_width  * scale_x);

    // Screen view plus one screen wide margin
    // There could be stations off screen with on screen trails
    // See also the use of position_on_extd_screen()
    min_lat = mid_y_lat_offset  - marg_lat;
    max_lat = mid_y_lat_offset  + marg_lat;
    min_lon = mid_x_long_offset - marg_lon;
    max_lon = mid_x_long_offset + marg_lon;

    p_station = n_first;
    while (p_station != NULL) {
        if ((p_station->flag & ST_ACTIVE) == 0        // ignore deleted objects
                  || p_station->coord_lon < min_lon || p_station->coord_lon > max_lon
                  || p_station->coord_lat < min_lat || p_station->coord_lat > max_lat
                  || (p_station->coord_lat == 0 && p_station->coord_lon == 0)) {
            // outside view and undefined stations:
            p_station->flag &= (~ST_INVIEW);        // clear "In View" flag
        } else
            p_station->flag |= ST_INVIEW;           // set "In View" flag
        p_station = p_station->n_next;
    }
}





/*
 *  Check if position is inside screen borders
 */
int position_on_screen(long lat, long lon) {

    if (   lon > x_long_offset && lon < (x_long_offset+(long)(screen_width *scale_x))
        && lat > y_lat_offset  && lat < (y_lat_offset +(long)(screen_height*scale_y)) 
        && !(lat == 0 && lon == 0))     // discard undef positions from screen
        return(1);                      // position is inside the screen
    else
        return(0);
}





/*
 *  Check if position is inside extended screen borders
 *  (real screen + one screen margin for trails)
 *  used for station "In View" flag
 */
int position_on_extd_screen(long lat, long lon) {
    long marg_lat, marg_lon;                    // margin around screen

    marg_lat = (long)(3 * screen_height * scale_y/2);
    marg_lon = (long)(3 * screen_width  * scale_x/2);
    if (marg_lat < IN_VIEW_MIN*60*100)          // allow a minimum area, 
        marg_lat = IN_VIEW_MIN*60*100;          // there could be outside stations
    if (marg_lon < IN_VIEW_MIN*60*100)          // with trail parts on screen
        marg_lon = IN_VIEW_MIN*60*100;

    if (    abs(lon - mid_x_long_offset) < marg_lon
         && abs(lat - mid_y_lat_offset)  < marg_lat
         && !(lat == 0 && lon == 0))    // discard undef positions from screen
        return(1);                      // position is inside the area
    else
        return(0);
}





/*
 *  Check if position is inside inner screen area
 *  (real screen + minus 1/6 screen margin)
 *  used for station tracking
 */
int position_on_inner_screen(long lat, long lon) {

    if (    lon > mid_x_long_offset-(long)(screen_width *scale_x/3)
         && lon < mid_x_long_offset+(long)(screen_width *scale_x/3)
         && lat > mid_y_lat_offset -(long)(screen_height*scale_y/3)
         && lat < mid_y_lat_offset +(long)(screen_height*scale_y/3)
         && !(lat == 0 && lon == 0))    // discard undef positions from screen
        return(1);                      // position is inside the area
    else
        return(0);
}





/*
 *  Delete single station with all its data    ?? delete messages ??
 *  This function is called with a callsign parameter.  Only used for
 *  my callsign, not for any other.
 */
void station_del(char *call) {
    DataRow *p_name;                      // DK7IN: do it with move... ?

    if (search_station_name(&p_name, call, 1)) {
        (void)delete_trail(p_name);       // Free track storage if it exists.
        (void)delete_weather(p_name);     // Free weather memory, if allocated
        (void)delete_multipoints(p_name); // Free multipoint memory, if allocated
        (void)delete_comments_and_status(p_name);  // Free comment storage if it exists
        if (p_name->node_path_ptr != NULL)// Free malloc'ed path
            free(p_name->node_path_ptr);
        if (p_name->tactical_call_sign != NULL)
            free(p_name->tactical_call_sign);
        delete_station_memory(p_name);    // Free memory
    }
}





/*
 *  Delete single station with all its data    ?? delete messages ??
 *  This function is called with a pointer instead of a callsign.
 */
void station_del_ptr(DataRow *p_name) {

//fprintf(stderr,"db.c:station_del_ptr(): %s\n",p_name->call_sign);

    if (p_name != NULL) {

        // A bit of debug code:  Attempting to find out if we're
        // deleting our own objects from time to time.  Leave this
        // in until we're sure the problem has been fixed.
        if (is_my_call(p_name->origin,1)) {
            fprintf(stderr,"station_del_ptr: Removing my own object: %s\n",
                p_name->call_sign);
        }

#ifdef EXPIRE_DEBUG
        fprintf(stderr,"Removing: %s heard %d seconds ago\n",p_name->call_sign, (int)(sec_now() - p_name->sec_heard));
#endif

        (void)delete_trail(p_name);     // Free track storage if it exists.
        (void)delete_weather(p_name);   // free weather memory, if allocated
        (void)delete_multipoints(p_name); // Free multipoint memory, if allocated
        (void)delete_comments_and_status(p_name);  // Free comment storage if it exists
        if (p_name->node_path_ptr != NULL)  // Free malloc'ed path
            free(p_name->node_path_ptr);
        if (p_name->tactical_call_sign != NULL)
            free(p_name->tactical_call_sign);
        delete_station_memory(p_name);  // free memory

//fprintf(stderr,"db.c:station_del_ptr(): Deleted station\n");

    }
}





/*
 *  Delete all stations             ?? delete messages ??
 */
void delete_all_stations(void) {
    DataRow *p_name;
    DataRow *p_curr;
    int ii;
 

    // Clear all of the pointers before we begin
    for (ii = 0; ii < 16384; ii++) {
        station_shortcuts[ii] = NULL;
    }
   
    p_name = n_first;
    while (p_name != NULL) {
        p_curr = p_name;
        p_name = p_name->n_next;
        station_del_ptr(p_curr);
        //(void)delete_trail(p_curr);     // free trail memory, if allocated
        //(void)delete_weather(p_curr);   // free weather memory, if allocated
        //(void)delete_multipoints(p_curr);// Free multipoint memory, if allocated
        //delete_station_memory(p_curr);  // free station memory
    }
    if (stations != 0) {
        fprintf(stderr,"ERROR: stations should be 0 after stations delete, is %ld\n",stations);
        stations = 0;
    }    
}





/*
 *  Check if we have to delete old stations.
 *
 *  Called from main.c:UpdateTime() on a periodic basis.
 *
 */
void check_station_remove(void) {
    DataRow *p_station, *p_station_t_next;
    time_t t_rem;
    int done = 0;


    // Only run through this routine ever STATION_REMOVE_CYCLE
    // seconds (currently every five minutes)
#ifdef EXPIRE_DEBUG
    // Check every 15 seconds, useful for debug only.
    if (last_station_remove < (sec_now() - 15)) {   // DEBUG
#else
    if (last_station_remove < (sec_now() - STATION_REMOVE_CYCLE)) {
#endif


//fprintf(stderr,"db.c:check_station_remove() is running\n");

        // Compute the cutoff time.  Any stations older than t_rem
        // will be removed, unless they have a tactical call or
        // belong to us.
        t_rem = sec_now() - sec_remove;

#ifdef EXPIRE_DEBUG
        // Expire every 15 seconds, useful for debug only.
        t_rem = sec_now() - (1 * 15);
#endif

        p_station = t_first;    // Oldest station in our list
        while (p_station != NULL && !done) {

            // Save a pointer to the next record in time-order
            // before we delete a record and lose it.
            p_station_t_next = p_station->t_next;

            if (p_station->sec_heard < t_rem) {

                if ( (is_my_call(p_station->call_sign,1))                   // It's my station or
                        || ( (is_my_call(p_station->origin,1))              // Station is owned by me
                          && ( ((p_station->flag & ST_OBJECT) != 0)         // and it's an object
                            || ((p_station->flag & ST_ITEM  ) != 0) ) ) ) { // or an item

                    // It's one of mine, leave it alone!

#ifdef EXPIRE_DEBUG
                    fprintf(stderr,"found old station: %s\t\t",p_station->call_sign);
                    fprintf(stderr,"mine\n");
#endif

                }

                else if (p_station->tactical_call_sign) {
                    // Station has a tactical callsign assigned,
                    // don't delete it.

#ifdef EXPIRE_DEBUG
                    fprintf(stderr,"found old station: %s\t\t",p_station->call_sign);
                    fprintf(stderr,"tactical\n");
#endif

                }

                else {  // Not one of mine, doesn't have a tactical
                        // callsign assigned, so start deleting
 
                    mdelete_messages(p_station->call_sign); // Delete messages
                    station_del_ptr(p_station);
                    //(void)delete_trail(p_station);        // Free track storage if it exists.
                    //(void)delete_weather(p_station);      // Free weather memory, if allocated
                    //(void)delete_multipoints(p_station);  // Free multipoint memory, if allocated
                    //delete_station_memory(p_station);     // Free memory

#ifdef EXPIRE_DEBUG
                    fprintf(stderr,"found old station: %s\t\t",p_station->call_sign);
                    fprintf(stderr,"deleting\n");
#endif

                }
            } else {
                done++;                                         // all other stations are newer...
            }
            p_station = p_station_t_next;
        }
        last_station_remove = sec_now();
    }
}





/*
 *  Delete an object (mark it as deleted)
 */
void delete_object(char *name) {
    DataRow *p_station;

    p_station = NULL;
    if (search_station_name(&p_station,name,1)) {       // find object name
        p_station->flag &= (~ST_ACTIVE);                // clear flag
        p_station->flag &= (~ST_INVIEW);                // clear "In View" flag
        if (position_on_screen(p_station->coord_lat,p_station->coord_lon))
            redraw_on_new_data = 2;                     // redraw now
            // there is some problem...  it is not redrawn immediately! ????
            // but deleted from list immediatetly
        redo_list = (int)TRUE;                          // and update lists
    }
}





///////////////////////////////////////  APRS Decoding  ////////////////////////////////////////////


/*
 *  Extract Uncompressed Position Report from begin of line
 *
 * If a position is found, it is deleted from the data.
 */
int extract_position(DataRow *p_station, char **info, int type) {
    int ok;
    char temp_lat[8+1];
    char temp_lon[9+1];
    char temp_grid[8+1];
    char *my_data;
    float gridlat;
    float gridlon;
    my_data = (*info);

    if (type != APRS_GRID){ // Not a grid
        ok = (int)(strlen(my_data) >= 19);
        ok = (int)(ok && my_data[4]=='.' && my_data[14]=='.'
            && (toupper(my_data[7]) =='N' || toupper(my_data[7]) =='S')
            && (toupper(my_data[17])=='E' || toupper(my_data[17])=='W'));
        // errors found:  [4]: X   [7]: n s   [17]: w e
        if (ok) {
            ok =             is_num_chr(my_data[0]);           // 5230.31N/01316.88E>
            ok = (int)(ok && is_num_chr(my_data[1]));          // 0123456789012345678
            ok = (int)(ok && is_num_or_sp(my_data[2]));
            ok = (int)(ok && is_num_or_sp(my_data[3]));
            ok = (int)(ok && is_num_or_sp(my_data[5]));
            ok = (int)(ok && is_num_or_sp(my_data[6]));
            ok = (int)(ok && is_num_chr(my_data[9]));
            ok = (int)(ok && is_num_chr(my_data[10]));
            ok = (int)(ok && is_num_chr(my_data[11]));
            ok = (int)(ok && is_num_or_sp(my_data[12]));
            ok = (int)(ok && is_num_or_sp(my_data[13]));
            ok = (int)(ok && is_num_or_sp(my_data[15]));
            ok = (int)(ok && is_num_or_sp(my_data[16]));
        }
                                            
        if (ok) {
            overlay_symbol(my_data[18], my_data[8], p_station);
            p_station->pos_amb = 0;
            // spaces in latitude set position ambiguity, spaces in longitude do not matter
            // we will adjust the lat/long to the center of the rectangle of ambiguity
            if (my_data[2] == ' ') {      // nearest degree
                p_station->pos_amb = 4;
                my_data[2]  = my_data[12] = '3';
                my_data[3]  = my_data[5]  = my_data[6]  = '0';
                my_data[13] = my_data[15] = my_data[16] = '0';
            }
            else if (my_data[3] == ' ') { // nearest 10 minutes
                p_station->pos_amb = 3;
                my_data[3]  = my_data[13] = '5';
                my_data[5]  = my_data[6]  = '0';
                my_data[15] = my_data[16] = '0';
            }
            else if (my_data[5] == ' ') { // nearest minute
                p_station->pos_amb = 2;
                my_data[5]  = my_data[15] = '5';
                my_data[6]  = '0';
                my_data[16] = '0';
            }
            else if (my_data[6] == ' ') { // nearest 1/10th minute
                p_station->pos_amb = 1;
                my_data[6]  = my_data[16] = '5';
            }

            xastir_snprintf(temp_lat,
                sizeof(temp_lat),
                "%s",
                my_data);
            temp_lat[7] = toupper(my_data[7]);
            temp_lat[8] = '\0';

            xastir_snprintf(temp_lon,
                sizeof(temp_lon),
                "%s",
                my_data+9);
            temp_lon[8] = toupper(my_data[17]);
            temp_lon[9] = '\0';

            if (!is_my_call(p_station->call_sign,1)) {      // don't change my position, I know it better...
                p_station->coord_lat = convert_lat_s2l(temp_lat);   // ...in case of position ambiguity
                p_station->coord_lon = convert_lon_s2l(temp_lon);
            }

            (*info) += 19;                  // delete position from comment
        }
    } else { // It is a grid 
        // first sanity checks, need more
        ok = (int)(is_num_chr(my_data[2]));
        ok = (int)(ok && is_num_chr(my_data[3]));
        ok = (int)(ok && ((my_data[0]>='A')&&(my_data[0]<='R')));
        ok = (int)(ok && ((my_data[1]>='A')&&(my_data[1]<='R')));
        if (ok) {
            xastir_snprintf(temp_grid,
                sizeof(temp_grid),
                "%s",
                my_data);
            // this test treats >6 digit grids as 4 digit grids; >6 are uncommon.
            // the spec mentioned 4 or 6, I'm not sure >6 is even allowed.
            if ( (temp_grid[6] != ']') || (temp_grid[4] == 0) || (temp_grid[5] == 0)){
                p_station->pos_amb = 6; // 1deg lat x 2deg lon 
                temp_grid[4] = 'L';
                temp_grid[5] = 'L';
            } else {
                p_station->pos_amb = 5; // 2.5min lat x 5min lon
                temp_grid[4] = toupper(temp_grid[4]); 
                temp_grid[5] = toupper(temp_grid[5]);
            }
            // These equations came from what I read in the qgrid source code and
            // various mailing list archives.
            gridlon= (20.*((float)temp_grid[0]-65.) + 2.*((float)temp_grid[2]-48.) + 5.*((float)temp_grid[4]-65.)/60.) - 180.;
            gridlat= (10.*((float)temp_grid[1]-65.) + ((float)temp_grid[3]-48.) + 5.*(temp_grid[5]-65.)/120.) - 90.;
            // could check for my callsign here, and avoid changing it...
            p_station->coord_lat = (unsigned long)(32400000l + (360000.0 * (-gridlat)));
            p_station->coord_lon = (unsigned long)(64800000l + (360000.0 * gridlon));
            p_station->aprs_symbol.aprs_type = '/';
            p_station->aprs_symbol.aprs_symbol = 'G';
        }        // is it valid grid or not - "ok"
        // could cut off the grid square from the comment here, but why bother?
    } // is it grid or not
    return(ok);
}





// DK7IN 99
/*
 *  Extract Compressed Position Report Data Formats from begin of line
 *    [APRS Reference, chapter 9]
 *
 * If a position is found, it is deleted from the data.
 */
int extract_comp_position(DataRow *p_station, char **info, /*@unused@*/ int type) {
    int ok;
    int x1, x2, x3, x4, y1, y2, y3, y4;
    int c = 0;
    int s = 0;
    int T = 0;
    int len;
    char *my_data;
    float lon = 0;
    float lat = 0;
    float range;
    int skip = 0;


    if (debug_level & 1)
        fprintf(stderr,"extract_comp_position: Start\n");

    //fprintf(stderr,"extract_comp_position start: %s\n",*info);

    // compressed data format  /YYYYXXXX$csT  is a fixed 13-character field
    // used for ! / @ = data IDs
    //   /     Symbol Table ID
    //   YYYY  compressed latitude
    //   XXXX  compressed longitude
    //   $     Symbol Code
    //   cs    compressed
    //            course/speed
    //            radio range
    //            altitude
    //   T     compression type ID

    my_data = (*info);

    //fprintf(stderr,"my_data: %s\n",my_data);

    // If c = space, csT bytes are ignored.  Minimum length:  8
    // bytes for lat/lon, 2 for symbol, 3 for csT for a total of 13.
    len = strlen(my_data);
    ok = (int)(len >= 13);

    if (ok) {
        y1 = (int)my_data[1] - '!';
        y2 = (int)my_data[2] - '!';
        y3 = (int)my_data[3] - '!';
        y4 = (int)my_data[4] - '!';
        x1 = (int)my_data[5] - '!';
        x2 = (int)my_data[6] - '!';
        x3 = (int)my_data[7] - '!';
        x4 = (int)my_data[8] - '!';

        // csT bytes
        if (my_data[10] == ' ') // Space
            c = -1; // This causes us to ignore csT
        else {
            c = (int)my_data[10] - '!';
            s = (int)my_data[11] - '!';
            T = (int)my_data[12] - '!';
        }
        skip = 13;

        // Convert ' ' to '0'.  Not specified in APRS Reference!  Do
        // we need it?
        if (x1 == -1) x1 = '\0';
        if (x2 == -1) x2 = '\0';
        if (x3 == -1) x3 = '\0';
        if (x4 == -1) x4 = '\0';
        if (y1 == -1) y1 = '\0';
        if (y2 == -1) y2 = '\0';
        if (y3 == -1) y3 = '\0';
        if (y4 == -1) y4 = '\0';

        ok = (int)(ok && (x1 >= '\0' && x1 < 91));  //  /YYYYXXXX$csT
        ok = (int)(ok && (x2 >= '\0' && x2 < 91));  //  0123456789012
        ok = (int)(ok && (x3 >= '\0' && x3 < 91));
        ok = (int)(ok && (x4 >= '\0' && x4 < 91));
        ok = (int)(ok && (y1 >= '\0' && y1 < 91));
        ok = (int)(ok && (y2 >= '\0' && y2 < 91));
        ok = (int)(ok && (y3 >= '\0' && y3 < 91));
        ok = (int)(ok && (y4 >= '\0' && y4 < 91));

        T &= 0x3F;      // DK7IN: force Compression Byte to valid format
                        // mask off upper two unused bits, they should be zero!?

        ok = (int)(ok && (c == -1 || ((c >=0 && c < 91) && (s >= 0 && s < 91) && (T >= 0 && T < 64))));

        if (ok) {
            lat = (((y1 * 91 + y2) * 91 + y3) * 91 + y4 ) / 380926.0; // in deg, 0:  90°N
            lon = (((x1 * 91 + x2) * 91 + x3) * 91 + x4 ) / 190463.0; // in deg, 0: 180°W
            lat *= 60 * 60 * 100;                       // in 1/100 sec
            lon *= 60 * 60 * 100;                       // in 1/100 sec

            // The below check should _not_ be done.  Compressed
            // format can resolve down to about 1 foot worldwide
            // (0.3 meters).
            //if ((((long)(lat+4) % 60) > 8) || (((long)(lon+4) % 60) > 8))
            //    ok = 0;   // check max resolution 0.01 min to
                            // catch even more errors
        }
    }

    if (ok) {
        overlay_symbol(my_data[9], my_data[0], p_station);      // Symbol / Table
        if (!is_my_call(p_station->call_sign,1)) {  // don't change my position, I know it better...
            // Record the uncompressed lat/long that we just
            // computed.
            p_station->coord_lat = (long)((lat));               // in 1/100 sec
            p_station->coord_lon = (long)((lon));               // in 1/100 sec
        }

        if (c >= 0) {                                   // ignore csT if c = ' '
            if (c < 90) {   // Found course/speed or altitude bytes
                if ((T & 0x18) == 0x10) {   // check for GGA (with altitude)
                    xastir_snprintf(p_station->altitude, sizeof(p_station->altitude), "%06.0f",pow(1.002,(double)(c*91+s))*0.3048);
                } else { // Found compressed course/speed bytes

                    // Convert 0 degrees to 360 degrees so that
                    // Xastir will see it as a valid course and do
                    // dead-reckoning properly on this station
                    if (c == 0) {
                        c = 90;
                    }

                    // Compute course in degrees
                    xastir_snprintf(p_station->course,
                        sizeof(p_station->course),
                        "%03d",
                        c*4);

                    // Compute speed in knots
                    xastir_snprintf(p_station->speed,
                        sizeof(p_station->speed),
                        "%03.0f",
                        pow( 1.08,(double)s ) - 1.0);

                    //fprintf(stderr,"Decoded speed:%s, course:%s\n",p_station->speed,p_station->course);

                }
            } else {    // Found pre-calculated radio range bytes
                if (c == 90) {
                    // pre-calculated radio range
                    range = 2 * pow(1.08,(double)s);    // miles

                    // DK7IN: dirty hack...  but better than nothing
                    if (s <= 5)                         // 2.9387 mi
                        xastir_snprintf(p_station->power_gain, sizeof(p_station->power_gain), "PHG%s0", "000");
                    else if (s <= 17)                   // 7.40 mi
                        xastir_snprintf(p_station->power_gain, sizeof(p_station->power_gain), "PHG%s0", "111");
                    else if (s <= 36)                   // 31.936 mi
                        xastir_snprintf(p_station->power_gain, sizeof(p_station->power_gain), "PHG%s0", "222");
                    else if (s <= 75)                   // 642.41 mi
                        xastir_snprintf(p_station->power_gain, sizeof(p_station->power_gain), "PHG%s0", "333");
                    else                       // max 90:  2037.8 mi
                        xastir_snprintf(p_station->power_gain, sizeof(p_station->power_gain), "PHG%s0", "444");
                }
            }
        }
        (*info) += skip;    // delete position from comment
    }

    if (debug_level & 1) {
        if (ok) {
            fprintf(stderr,"*** extract_comp_position: Succeeded: %ld\t%ld\n",
                p_station->coord_lat,
                p_station->coord_lon);
        }
        else {
            fprintf(stderr,"*** extract_comp_position: Failed!\n");
        }
    }

    //fprintf(stderr,"  extract_comp_position end: %s\n",*info);

    return(ok);
}





//
//  Extract speed and/or course from beginning of info field
//
// Returns course in degrees, speed in KNOTS.
//
int extract_speed_course(char *info, char *speed, char *course) {
    int i,found,len;

    len = (int)strlen(info);
    found = 0;
    if (len >= 7) {
        found = 1;
        for(i=0; found && i<7; i++) {           // check data format
            if (i==3) {                         // check separator
                if (info[i]!='/')
                    found = 0;
            } else {
                if( !( isdigit((int)info[i])
                        || (info[i] == ' ')     // Spaces and periods are allowed.  Need these
                        || (info[i] == '.') ) ) // here so that we can get the field deleted
                    found = 0;
            }
        }
    }
    if (found) {
        substr(course,info,3);
        substr(speed,info+4,3);
        for (i=0;i<=len-7;i++)        // delete speed/course from info field
            info[i] = info[i+7];
    }
    if (!found || atoi(course) < 1) {   // course 0 means undefined
//        speed[0] ='\0';   // Don't do this!  We can have a valid
//        speed without a valid course.
        course[0]='\0';
    }
    else {  // recheck data format looking for undefined fields
        for(i=0; i<2; i++) {
            if( !(isdigit((int)speed[i]) ) )
                speed[0] = '\0';
            if( !(isdigit((int)course[i]) ) )
                course[0] = '\0';
        }
    }

    return(found);
}





/*
 *  Extract bearing and number/range/quality from beginning of info field
 */
int extract_bearing_NRQ(char *info, char *bearing, char *nrq) {
    int i,found,len;

    len = (int)strlen(info);
    found = 0;
    if (len >= 8) {
        found = 1;
        for(i=1; found && i<8; i++)         // check data format
            if(!(isdigit((int)info[i]) || (i==4 && info[i]=='/')))
                found=0;
    }
    if (found) {
        substr(bearing,info+1,3);
        substr(nrq,info+5,3);

        //fprintf(stderr,"Bearing: %s\tNRQ: %s\n", bearing, nrq);

        for (i=0;i<=len-8;i++)        // delete bearing/nrq from info field
            info[i] = info[i+8];
    }
    if (!found || nrq[2] == '0') {   // Q of 0 means useless bearing
        bearing[0] ='\0';
        nrq[0]='\0';
    }
    return(found);
}





/*
 *  Extract altitude from APRS info field          "/A=012345"    in feet
 */
int extract_altitude(char *info, char *altitude) {
    int i,ofs,found,len;

    found=0;
    len = (int)strlen(info);
    for(ofs=0; !found && ofs<len-8; ofs++)      // search for start sequence
        if (strncmp(info+ofs,"/A=",3)==0) {
            found=1;
            // Are negative altitudes even defined?  Yes!  In Mic-E spec to -10,000 meters
            if(!isdigit((int)info[ofs+3]) && info[ofs+3]!='-')  // First char must be digit or '-'
                found=0;
            for(i=4; found && i<9; i++)         // check data format for next 5 chars
                if(!isdigit((int)info[ofs+i]))
                    found=0;
    }
    if (found) {
        ofs--;  // was one too much on exit from for loop
        substr(altitude,info+ofs+3,6);
        for (i=ofs;i<=len-9;i++)        // delete altitude from info field
            info[i] = info[i+9];
    } else
        altitude[0] = '\0';
    return(found);
}





// TODO:
// Comment Field




 
/*
 *  Extract powergain and/or range from APRS info field:
 * "PHG1234/", "PHG1234", or "RNG1234" from APRS data extension.
 */
int extract_powergain_range(char *info, char *phgd) {
    int i,found,len;
    char *info2;


//fprintf(stderr,"Info:%s\n",info);

    // Check whether two strings of interest are present and snag a
    // pointer to them.
    info2 = strstr(info,"RNG");
    if (!info2)
        info2 = strstr(info,"PHG");
    if (!info2) {
        phgd[0] = '\0';
        return(0);
    }

    found=0;
    len = (int)strlen(info2);

    if (len >= 9 && strncmp(info2,"PHG",3)==0
            && info2[7]=='/'
            && info2[8]!='A'  // trailing '/' not defined in Reference...
            && isdigit((int)info2[3])
            && isdigit((int)info2[4])
            && isdigit((int)info2[5])
            && isdigit((int)info2[6])) {
        substr(phgd,info2,7);
        found = 1;
        for (i=0;i<=len-8;i++)        // delete powergain from data extension field
            info2[i] = info2[i+8];
    }
    else {
        if (len >= 7 && strncmp(info2,"PHG",3)==0
                && isdigit((int)info2[3])
                && isdigit((int)info2[4])
                && isdigit((int)info2[5])
                && isdigit((int)info2[6])) {
            substr(phgd,info2,7);
            found = 1;
            for (i=0;i<=len-7;i++)        // delete powergain from data extension field
                info2[i] = info2[i+7];
        }
        else if (len >= 7 && strncmp(info2,"RNG",3)==0
                && isdigit((int)info2[3])
                && isdigit((int)info2[4])
                && isdigit((int)info2[5])
                && isdigit((int)info2[6])) {
            substr(phgd,info2,7);
            found = 1;
            for (i=0;i<=len-7;i++)        // delete powergain from data extension field
                info2[i] = info2[i+7];
        } else {
            phgd[0] = '\0';
        }
    }
    return(found);
}





/*
 *  Extract omnidf from APRS info field          "DFS1234/"    from APRS data extension
 */
int extract_omnidf(char *info, char *phgd) {
    int i,found,len;

    found=0;
    len = (int)strlen(info);
    if (len >= 8 && strncmp(info,"DFS",3)==0 && info[7]=='/'    // trailing '/' not defined in Reference...
                 && isdigit((int)info[3]) && isdigit((int)info[5]) && isdigit((int)info[6])) {
        substr(phgd,info,7);
        for (i=0;i<=len-8;i++)        // delete omnidf from data extension field
            info[i] = info[i+8];
        return(1);
    } else {
        phgd[0] = '\0';
        return(0);
    }
}





/*
 *  Extract signpost data from APRS info field: "{123}", an APRS data extension
 *  Format can be {1}, {12}, or {123}.  Letters or digits are ok.
 */
int extract_signpost(char *info, char *signpost) {
    int i,found,len,done;

//0123456
//{1}
//{12}
//{121}

    found=0;
    len = (int)strlen(info);
    if ( (len > 2)
            && (info[0] == '{')
            && ( (info[2] == '}' ) || (info[3] == '}' ) || (info[4] == '}' ) ) ) {

        i = 1;
        done = 0;
        while (!done) {                 // Snag up to three digits
            if (info[i] == '}') {       // We're done
                found = i;              // found = position of '}' character
                done++;
            }
            else {
                signpost[i-1] = info[i];
            }

            i++;

            if ( (i > 4) && !done) {    // Something is wrong, we should be done by now
                done++;
                signpost[0] = '\0';
                return(0);
            }
        }
        substr(signpost,info+1,found-1);
        found++;
        for (i=0;i<=len-found;i++) {    // delete omnidf from data extension field
            info[i] = info[i+found];
        }
        return(1);
    } else {
        signpost[0] = '\0';
        return(0);
    }
}





/*
 *  Extract probability_min data from APRS info field: "Pmin1.23,"
 *  Please note the ending comma.  We use it to delimit the field.
 */
int extract_probability_min(char *info, char *prob_min, int prob_min_size) {
    int len,done;
    char *c;
    char *d;


//fprintf(stderr,"%s\n",info);
 
    len = (int)strlen(info);
    if (len < 6) {          // Too short
//fprintf(stderr,"Pmin too short: %s\n",info);
        prob_min[0] = '\0';
        return(0);
    }

    c = strstr(info,"Pmin");
    if (c == NULL) {        // Pmin not found
//fprintf(stderr,"Pmin not found: %s\n",info);
        prob_min[0] = '\0';
        return(0);
    }

    c = c+4;    // Skip the Pmin part
    // Find the ending comma
    d = c;
    done = 0;
    while (!done) {
        if (*d == ',') {    // We're done
            done++;
        }
        else {
            d++;
        }

        // Check for string too long
        if ( ((d-c) > 10) && !done) {    // Something is wrong, we should be done by now
//fprintf(stderr,"Pmin too long: %d,%s\n",d-c,info);
            prob_min[0] = '\0';
            return(0);
        }
    }

    // Copy the substring across
    xastir_snprintf(prob_min,
        prob_min_size,
        "%s",
        c);
    prob_min[d-c] = '\0';
    prob_min[10] = '\0';    // Just to make sure

    // Delete data from data extension field 
    d++;    // Skip the comma
    done = 0;
    while (!done) {
        *(c-4) = *d;
        if (*d == '\0')
            done++;
        c++;
        d++;
    }

    return(1);
}





/*
 *  Extract probability_max data from APRS info field: "Pmax1.23,"
 *  Please note the ending comma.  We use it to delimit the field.
 */
int extract_probability_max(char *info, char *prob_max, int prob_max_size) {
    int len,done;
    char *c;
    char *d;


//fprintf(stderr,"%s\n",info);

    len = (int)strlen(info);
    if (len < 6) {          // Too short
//fprintf(stderr,"Pmax too short: %s\n",info);
        prob_max[0] = '\0';
        return(0);
    }

    c = strstr(info,"Pmax");
    if (c == NULL) {        // Pmax not found
//fprintf(stderr,"Pmax not found: %s\n",info);
        prob_max[0] = '\0';
        return(0);
    }

    c = c+4;    // Skip the Pmax part
    // Find the ending comma
    d = c;
    done = 0;
    while (!done) {
        if (*d == ',') {    // We're done
            done++;
        }
        else {
            d++;
        }

        // Check for string too long
        if ( ((d-c) > 10) && !done) {    // Something is wrong, we should be done by now
//fprintf(stderr,"Pmax too long: %d,%s\n",d-c,info);
            prob_max[0] = '\0';
            return(0);
        }
    }

    // Copy the substring across
    xastir_snprintf(prob_max,
        prob_max_size,
        "%s",
        c);
    prob_max[d-c] = '\0';
    prob_max[10] = '\0';    // Just to make sure

    // Delete data from data extension field 
    d++;    // Skip the comma
    done = 0;
    while (!done) {
        *(c-4) = *d;
        if (*d == '\0')
            done++;
        c++;
        d++;
    }
 
    return(1);
}





static void clear_area(DataRow *p_station) {
    p_station->aprs_symbol.area_object.type           = AREA_NONE;
    p_station->aprs_symbol.area_object.color          = AREA_GRAY_LO;
    p_station->aprs_symbol.area_object.sqrt_lat_off   = 0;
    p_station->aprs_symbol.area_object.sqrt_lon_off   = 0;
    p_station->aprs_symbol.area_object.corridor_width = 0;
}





/*
 *  Extract Area Object
 */
void extract_area(DataRow *p_station, char *data) {
    int i, val, len;
    unsigned int uval;
    AreaObject temp_area;

    /* NOTE: If we are here, the symbol was the area symbol.  But if this
       is a slightly corrupted packet, we shouldn't blow away the area info
       for this station, since it could be from a previously received good
       packet.  So we will work on temp_area and only copy to p_station at
       the end, returning on any error as we parse. N7TAP */

    //fprintf(stderr,"Area Data: %s\n", data);

    len = (int)strlen(data);
    val = data[0] - '0';
    if (val >= 0 && val <= AREA_MAX) {
        temp_area.type = val;
        val = data[4] - '0';
        if (data[3] == '/') {
            if (val >=0 && val <= 9) {
                temp_area.color = val;
            }
            else {
                if (debug_level & 2)
                    fprintf(stderr,"Bad area color (/)");
                return;
            }
        }
        else if (data[3] == '1') {
            if (val >=0 && val <= 5) {
                temp_area.color = 10 + val;
            }
            else {
                if (debug_level & 2)
                    fprintf(stderr,"Bad area color (1)");
                return;
            }
        }

        val = 0;
        if (isdigit((int)data[1]) && isdigit((int)data[2])) {
            val = (10 * (data[1] - '0')) + (data[2] - '0');
        }
        else {
            if (debug_level & 2)
                fprintf(stderr,"Bad area sqrt_lat_off");
            return;
        }
        temp_area.sqrt_lat_off = val;

        val = 0;
        if (isdigit((int)data[5]) && isdigit((int)data[6])) {
            val = (10 * (data[5] - '0')) + (data[6] - '0');
        }
        else {
            if (debug_level & 2)
                fprintf(stderr,"Bad area sqrt_lon_off");
            return;
        }
        temp_area.sqrt_lon_off = val;

        for (i = 0; i <= len-7; i++) // delete area object from data extension field
            data[i] = data[i+7];
        len -= 7;

        if (temp_area.type == AREA_LINE_RIGHT || temp_area.type == AREA_LINE_LEFT) {
            if (data[0] == '{') {
                if (sscanf(data, "{%u}", &uval) == 1) {
                    temp_area.corridor_width = uval & 0xffff;
                    for (i = 0; i <= len; i++)
                        if (data[i] == '}')
                            break;
                    uval = i+1;
                    for (i = 0; i <= (int)(len-uval); i++)
                        data[i] = data[i+uval]; // delete corridor width
                }
                else {
                    if (debug_level & 2)
                        fprintf(stderr,"Bad corridor width identifier");
                    temp_area.corridor_width = 0;
                    return;
                }
            }
            else {
                if (debug_level & 2)
                    fprintf(stderr,"No corridor width specified");
                temp_area.corridor_width = 0;
            }
        }
        else {
            temp_area.corridor_width = 0;
        }
    }
    else {
        if (debug_level & 2)
            fprintf(stderr,"Bad area type: %c\n", data[0]);
        return;
    }

    memcpy(&(p_station->aprs_symbol.area_object), &temp_area, sizeof(AreaObject));

    if (debug_level & 2) {
        fprintf(stderr,"AreaObject: type=%d color=%d sqrt_lat_off=%d sqrt_lon_off=%d corridor_width=%d\n",
                p_station->aprs_symbol.area_object.type,
                p_station->aprs_symbol.area_object.color,
                p_station->aprs_symbol.area_object.sqrt_lat_off,
                p_station->aprs_symbol.area_object.sqrt_lon_off,
                p_station->aprs_symbol.area_object.corridor_width);
    }
}





/*
 *  Extract Time from begin of line      [APRS Reference, chapter 6]
 *
 * If a time string is found in "data", it is deleted from the
 * beginning of the string.
 */
int extract_time(DataRow *p_station, char *data, int type) {
    int len, i;
    int ok = 0;

    // todo: better check of time data ranges
    len = (int)strlen(data);
    if (type == APRS_WX2) {
        // 8 digit time from stand-alone positionless weather stations...
        if (len > 8) {
            // MMDDHHMM   zulu time
            // MM 01-12         todo: better check of time data ranges
            // DD 01-31
            // HH 01-23
            // MM 01-59
            ok = 1;
            for (i=0;ok && i<8;i++)
                if (!isdigit((int)data[i]))
                    ok = 0;
            if (ok) {
//                substr(p_station->station_time,data+2,6);
//                p_station->station_time_type = 'z';
                for (i=0;i<=len-8;i++)         // delete time from data
                    data[i] = data[i+8];
            }
        }
    } else {
        if (len > 6) {
            // Status messages only with optional zulu format
            // DK7IN: APRS ref says one of 'z' '/' 'h', but I found 'c' at HB9TJM-8   ???
            if (toupper(data[6])=='Z' || data[6]=='/' || toupper(data[6])=='H')
                ok = 1;
            for (i=0;ok && i<6;i++)
                if (!isdigit((int)data[i]))
                    ok = 0;
            if (ok) {
//                substr(p_station->station_time,data,6);
//                p_station->station_time_type = data[6];
                for (i=0;i<=len-7;i++)         // delete time from data
                    data[i] = data[i+7];
            }
        }
    }
    return(ok);
}





// APRS Data Extensions               [APRS Reference p.27]
//  .../...  Course & Speed, may be followed by others (see p.27)
//  .../...  Wind Dir and Speed
//  PHG....  Station Power and Effective Antenna Height/Gain
//  RNG....  Pre-Calculated Radio Range
//  DFS....  DF Signal Strength and Effective Antenna Height/Gain
//  T../C..  Area Object Descriptor

/* Extract one of several possible APRS Data Extensions */
void process_data_extension(DataRow *p_station, char *data, /*@unused@*/ int type) {
    char temp1[7+1];
    char temp2[3+1];
    char temp3[10+1];
    char bearing[3+1];
    char nrq[3+1];

    if (p_station->aprs_symbol.aprs_type == '\\' && p_station->aprs_symbol.aprs_symbol == 'l') {
            /* This check needs to come first because the area object extension can look
               exactly like what extract_speed_course will attempt to decode. */
            extract_area(p_station, data);
    } else {
            clear_area(p_station); // we got a packet with a non area symbol, so clear the data

            if (extract_speed_course(data,temp1,temp2)) {  // ... from Mic-E, etc.
            //fprintf(stderr,"extracted speed/course\n");
                if (atof(temp2) > 0) {
                //fprintf(stderr,"course is non-zero\n");
                xastir_snprintf(p_station->speed,
                    sizeof(p_station->speed),
                    "%06.2f",
                    atof(temp1));
                xastir_snprintf(p_station->course,  // in degrees
                    sizeof(p_station->course),
                    "%s",
                    temp2);
            }

            if (extract_bearing_NRQ(data, bearing, nrq)) {  // Beam headings from DF'ing
                //fprintf(stderr,"extracted bearing and NRQ\n");
                xastir_snprintf(p_station->bearing,
                    sizeof(p_station->bearing),
                    "%s",
                    bearing);
                xastir_snprintf(p_station->NRQ,
                    sizeof(p_station->NRQ),
                    "%s",
                    nrq);
                p_station->signal_gain[0] = '\0';   // And blank out the shgd values
                }
            }
            else {
                if (extract_powergain_range(data,temp1)) {

//fprintf(stderr,"Found power_gain: %s\n", temp1);

                    xastir_snprintf(p_station->power_gain,
                        sizeof(p_station->power_gain),
                        "%s",
                        temp1);

                if (extract_bearing_NRQ(data, bearing, nrq)) {  // Beam headings from DF'ing
                    //fprintf(stderr,"extracted bearing and NRQ\n");
                    xastir_snprintf(p_station->bearing,
                        sizeof(p_station->bearing),
                        "%s",
                        bearing);
                    xastir_snprintf(p_station->NRQ,
                        sizeof(p_station->NRQ),
                        "%s",
                        nrq);
                    p_station->signal_gain[0] = '\0';   // And blank out the shgd values
                }
                }
            else {
                        if (extract_omnidf(data,temp1)) {
                    xastir_snprintf(p_station->signal_gain,
                        sizeof(p_station->signal_gain),
                        "%s",
                        temp1);   // Grab the SHGD values
                    p_station->bearing[0] = '\0';   // And blank out the bearing/NRQ values
                    p_station->NRQ[0] = '\0';

                    // The spec shows speed/course before DFS, but example packets that
                    // come with DOSaprs show DFSxxxx/speed/course.  We'll take care of
                    // that possibility by trying to decode speed/course again.
                        if (extract_speed_course(data,temp1,temp2)) {  // ... from Mic-E, etc.
                        //fprintf(stderr,"extracted speed/course\n");
                            if (atof(temp2) > 0) {
                            //fprintf(stderr,"course is non-zero\n");
                            xastir_snprintf(p_station->speed,
                                sizeof(p_station->speed),
                                "%06.2f",
                                atof(temp1));
                            xastir_snprintf(p_station->course,
                                sizeof(p_station->course),
                                "%s",
                                temp2);                    // in degrees
                        }
                    }

                    // The spec shows that omnidf and bearing/NRQ can be in the same
                    // packet, which makes no sense, but we'll try to decode it that
                    // way anyway.
                    if (extract_bearing_NRQ(data, bearing, nrq)) {  // Beam headings from DF'ing
                        //fprintf(stderr,"extracted bearing and NRQ\n");
                        xastir_snprintf(p_station->bearing,
                            sizeof(p_station->bearing),
                            "%s",
                            bearing);
                        xastir_snprintf(p_station->NRQ,
                            sizeof(p_station->NRQ),
                            "%s",
                            nrq);
                        //p_station->signal_gain[0] = '\0';   // And blank out the shgd values
                    }
                }
                }
            }
        if (extract_signpost(data, temp2)) {
            //fprintf(stderr,"extracted signpost data\n");
            xastir_snprintf(p_station->signpost,
                sizeof(p_station->signpost),
                "%s",
                temp2);
        }

        if (extract_probability_min(data, temp3, sizeof(temp3))) {
            //fprintf(stderr,"extracted probability_min data: %s\n",temp3);
            xastir_snprintf(p_station->probability_min,
                sizeof(p_station->probability_min),
                "%s",
                temp3);
        }
 
        if (extract_probability_max(data, temp3, sizeof(temp3))) {
            //fprintf(stderr,"extracted probability_max data: %s\n",temp3);
            xastir_snprintf(p_station->probability_max,
                sizeof(p_station->probability_max),
                "%s",
                temp3);
        }
    }
}





/* extract all available information from info field */
void process_info_field(DataRow *p_station, char *info, /*@unused@*/ int type) {
    char temp_data[6+1];
//    char time_data[MAX_TIME];

    if (extract_altitude(info,temp_data)) {                         // get altitude
        xastir_snprintf(p_station->altitude, sizeof(p_station->altitude), "%.2f",atof(temp_data)*0.3048);
        //fprintf(stderr,"%.2f\n",atof(temp_data)*0.3048);
    }
    // do other things...
}





////////////////////////////////////////////////////////////////////////////////////////////////////


// type: 18
// call_sign: VE6GRR-15
// path: GPSLV,TCPIP,VE7DIE*
// data: GPRMC,034728,A,5101.016,N,11359.464,W,000.0,284.9,110701,018.0,
// from: T
// port: 0
// origin: 
// third_party: 1





//
//  Extract data for $GPRMC, it fails if there is no position!!
//
int extract_RMC(DataRow *p_station, char *data, char *call_sign, char *path) {
    char temp_data[40]; // short term string storage, MAX_CALLSIGN, ...  ???
    char lat_s[20];
    char long_s[20];
    int ok;
    char *Substring[12];  // Pointers to substrings parsed by split_string()
    char temp_string[MAX_MESSAGE_LENGTH+1];
    char temp_char;


    if (debug_level & 256)
        fprintf(stderr,"extract_RMC\n");

    // should we copy it before processing? it changes data: ',' gets substituted by '\0' !!
    ok = 0; // Start out as invalid.  If we get enough info, we change this to a 1.

    if ( (data == NULL) || (strlen(data) < 34) ) {  // Not enough data to parse position from.
        if (debug_level & 256)
            fprintf(stderr,"Invalid RMC string: Too short\n");
        return(ok);
    }

    p_station->record_type = NORMAL_GPS_RMC;
    // Create a timestamp from the current time
    // get_time saves the time in temp_data
    xastir_snprintf(p_station->pos_time,
        sizeof(p_station->pos_time),
        "%s",
        get_time(temp_data));
    p_station->flag &= (~ST_MSGCAP);    // clear "message capable" flag

    /* check aprs type on call sign */
    p_station->aprs_symbol = *id_callsign(call_sign, path);

    // Make a copy of the incoming data.  The string passed to
    // split_string() gets destroyed.
    xastir_snprintf(temp_string,
        sizeof(temp_string),
        "%s",
        data);
    split_string(temp_string, Substring, 12);

    // The Substring[] array contains pointers to each substring in
    // the original data string.

// GPRMC,034728,A,5101.016,N,11359.464,W,000.0,284.9,110701,018.0,E*7D
//   0     1    2    3     4    5      6   7    8      9     10    11

    if (Substring[0] == NULL)   // No GPRMC string
        return(ok);

    if (Substring[1] == NULL)   // No time string
        return(ok);

    if (Substring[2] == NULL)   // No valid fix char
        return(ok);

    if (Substring[2][0] != 'A' && Substring[2][0] != 'V')
        return(ok);
// V is a warning but we can get good data still ?
// DK7IN: got no position with 'V' !

    if (Substring[3] == NULL)   // No latitude string
        return(ok);

    if (Substring[4] == NULL)   // No latitude N/S
        return(ok);

// Need to check lat_s for validity here.  Note that some GPS's put out another digit of precision
// (4801.1234) or leave one out (4801.12).  Next character after digits should be a ','

    temp_char = toupper((int)Substring[4][0]);

    if (temp_char != 'N' && temp_char != 'S')   // Bad N/S
        return(ok);

    xastir_snprintf(lat_s,
        sizeof(lat_s),
        "%s%c",
        Substring[3],
        temp_char);

    if (Substring[5] == NULL)   // No longitude string
        return(ok);

    if (Substring[6] == NULL)   // No longitude E/W
        return(ok);

// Need to check long_s for validity here.  Should be all digits.  Note that some GPS's put out another
// digit of precision.  (12201.1234).  Next character after digits should be a ','

    temp_char = toupper((int)Substring[6][0]);

    if (temp_char != 'E' && temp_char != 'W')   // Bad E/W
        return(ok);

    xastir_snprintf(long_s,
        sizeof(long_s),
        "%s%c",
        Substring[5],
        temp_char);

    p_station->coord_lat = convert_lat_s2l(lat_s);
    p_station->coord_lon = convert_lon_s2l(long_s);

    // If we've made it this far, We have enough for a position now!
    ok = 1;

    // Now that we have a basic position, let's see what other data
    // can be parsed from the packet.  The rest of it can still be
    // corrupt, so we're proceeding carefully under yellow alert on
    // impulse engines only.

// GPRMC,034728,A,5101.016,N,11359.464,W,000.0,284.9,110701,018.0,E*7D
//   0     1    2    3     4    5      6   7    8      9     10    11

    if (Substring[7] == NULL) { // No speed string
        p_station->speed[0] = '\0'; // No speed available
        return(ok);
    }
    else {
        xastir_snprintf(p_station->speed,
            MAX_SPEED,
            "%s",
            Substring[7]);
        // Is it always knots, otherwise we need a conversion!
    }

    if (Substring[8] == NULL) { // No course string
        xastir_snprintf(p_station->course,
            sizeof(p_station->course),
            "000.0");  // No course available
        return(ok);
    }
    else {
        xastir_snprintf(p_station->course,
            MAX_COURSE,
            "%s",
            Substring[8]);
    }

    if (debug_level & 256) {
        if (ok)
            fprintf(stderr,"extract_RMC succeeded\n");
        else
            fprintf(stderr,"extract_RMC failed\n");
    }

    return(ok);
}





//
//  Extract data for $GPGGA
//
// GPGGA,hhmmss,ddmm.mmm[m],{N|S},dddmm.mmm[m],{E|W},{0|1|2},nsat,hdop,mmm.m,M,mm.m,M,,[*CHK]
//
// hhmmss = UTC
// ddmm.mmm[m] = Degrees(dd) Minutes (mm.mmm[m]) (may be 1 to ?? digits of precision) Lattitude (N|S=North/South)
// dddmm.mmm[m] = Degrees (ddd) Minutes (mm.mmm[m]) (may be 1 to ?? digits of precision) Longitude (E|W=East/West)
// 0|1|2 == invalid/GPS/DGPS
// nsat=Number of Sattelites being tracked
// mmm.m,M=Meters MSL
// mm.mM = Meters above GPS Ellipsoid
//
//
int extract_GGA(DataRow *p_station,char *data,char *call_sign, char *path) {
    char temp_data[40]; // short term string storage, MAX_CALLSIGN, ...  ???
    char lat_s[20];
    char long_s[20];
    int  ok;
    char *Substring[15];  // Pointers to substrings parsed by split_string()
    char temp_string[MAX_MESSAGE_LENGTH+1];
    char temp_char;
    int  temp_num;


    if (debug_level & 256)
        fprintf(stderr, "extract_GGA\n");

    ok = 0; // Start out as invalid.  If we get enough info, we change this to a 1.
 
    if ( (data == NULL) || (strlen(data) < 32) )  // Not enough data to parse position from.
        return(ok);

    p_station->record_type = NORMAL_GPS_GGA;
    // Create a timestamp from the current time
    // get_time saves the time in temp_data
    xastir_snprintf(p_station->pos_time,
        sizeof(p_station->pos_time),
        "%s",
        get_time(temp_data));
    p_station->flag &= (~ST_MSGCAP);    // clear "message capable" flag

    /* check aprs type on call sign */
    p_station->aprs_symbol = *id_callsign(call_sign, path);

    // Make a copy of the incoming data.  The string passed to
    // split_string() gets destroyed.
    xastir_snprintf(temp_string,
        sizeof(temp_string),
        "%s",
        data);
    split_string(temp_string, Substring, 15);

    // The Substring[] array contains pointers to each substring in
    // the original data string.


// GPGGA,hhmmss,ddmm.mmm[m],{N|S},dddmm.mmm[m],{E|W},{0|1|2},nsat,hdop,mmm.m,M,mm.m,M,,[*CHK]
//   0     1        2         3        4         5      6     7    8     9   1  1   1 1 1
//                                                                           0  1   2 3 4

    if (Substring[0] == NULL)  // No GPGGA string
        return(ok);

    if (Substring[1] == NULL)  // No time string
        return(ok);

    if (Substring[2] == NULL)   // No latitude string
        return(ok);

    if (Substring[3] == NULL)   // No latitude N/S
        return(ok);

// Need to check lat_s for validity here.  Note that some GPS's put out another digit of precision
// (4801.1234).  Next character after digits should be a ','

    temp_char = toupper((int)Substring[3][0]);

    if (temp_char != 'N' && temp_char != 'S')   // Bad N/S
        return(ok);

    xastir_snprintf(lat_s,
        sizeof(lat_s),
        "%s%c",
        Substring[2],
        temp_char);

    if (Substring[4] == NULL)   // No longitude string
        return(ok);

    if (Substring[5] == NULL)   // No longitude E/W
        return(ok);

// Need to check long_s for validity here.  Should be all digits.  Note that some GPS's put out another
// digit of precision.  (12201.1234).  Next character after digits should be a ','

    temp_char = toupper((int)Substring[5][0]);

    if (temp_char != 'E' && temp_char != 'W')   // Bad E/W
        return(ok);

    xastir_snprintf(long_s,
        sizeof(long_s),
        "%s%c",
        Substring[4],
        temp_char);

    p_station->coord_lat = convert_lat_s2l(lat_s);
    p_station->coord_lon = convert_lon_s2l(long_s);

    // If we've made it this far, We have enough for a position now!
    ok = 1;


    // Now that we have a basic position, let's see what other data
    // can be parsed from the packet.  The rest of it can still be
    // corrupt, so we're proceeding carefully under yellow alert on
    // impulse engines only.

// What are the possible values for fix quality anyway?

    // Check for valid fix {
    if (Substring[6] == NULL
            || Substring[6][0] == '0'      // Fix quality
            || Substring[7] == NULL        // Sat number
            || Substring[8] == NULL        // hdop
            || Substring[9] == NULL) {     // Altitude in meters
        p_station->sats_visible[0] = '\0'; // Store empty sats visible
        p_station->altitude[0] = '\0';;    // Store empty altitude
        return(ok); // A field between fix quality and altitude is missing
    }

// Need to check for validity of this number.  Should be 0-12?  Perhaps a few more with WAAS, GLONASS, etc?
    temp_num = atoi(Substring[7]);
    if (temp_num < 0 || temp_num > 30) {
        return(ok); // Number of satellites not valid
    }
    else {
        // Store 
        xastir_snprintf(p_station->sats_visible,
            sizeof(p_station->sats_visible),
            "%d",
            temp_num);
    }


// Check for valid number for HDOP instead of just throwing it away?


    xastir_snprintf(p_station->altitude,
        sizeof(p_station->altitude),
        "%s",
        Substring[9]); // Get altitude

// Need to check for valid altitude before conversion

    // unit is in meters, if not adjust value ???

    if (Substring[10] == NULL)  // No units for altitude
        return(ok);

    if (Substring[10][0] != 'M') {
        //fprintf(stderr,"ERROR: should adjust altitude for meters\n");
        //} else {  // Altitude units wrong.  Assume altitude bad
        p_station->altitude[0] = '\0';
    }

    if (debug_level & 256) {
        if (ok)
            fprintf(stderr,"extract_GGA succeeded\n");
        else
            fprintf(stderr,"extract_GGA failed\n");
    }

    return(ok);
}





//
//  Extract data for $GPGLL
//
// $GPGLL,4748.811,N,12219.564,W,033850,A*3C
// lat, long, UTCtime in hhmmss, A=Valid, checksum
//
// GPGLL,4748.811,N,12219.564,W,033850,A*3C
//   0       1    2      3    4    5   6
//
int extract_GLL(DataRow *p_station,char *data,char *call_sign, char *path) {
    char temp_data[40]; // short term string storage, MAX_CALLSIGN, ...  ???
    char lat_s[20];
    char long_s[20];
    int ok;
    char *Substring[7];  // Pointers to substrings parsed by split_string()
    char temp_string[MAX_MESSAGE_LENGTH+1];
    char temp_char;


    if (debug_level & 256)
        fprintf(stderr, "extract_GLL\n");

    ok = 0; // Start out as invalid.  If we get enough info, we change this to a 1.
  
    if ( (data == NULL) || (strlen(data) < 28) )  // Not enough data to parse position from.
        return(ok);

    p_station->record_type = NORMAL_GPS_GLL;
    // Create a timestamp from the current time
    // get_time saves the time in temp_data
    xastir_snprintf(p_station->pos_time,
        sizeof(p_station->pos_time),
        "%s",
        get_time(temp_data));
    p_station->flag &= (~ST_MSGCAP);    // clear "message capable" flag

    /* check aprs type on call sign */
    p_station->aprs_symbol = *id_callsign(call_sign, path);

    // Make a copy of the incoming data.  The string passed to
    // split_string() gets destroyed.
    xastir_snprintf(temp_string,
        sizeof(temp_string),
        "%s",
        data);
    split_string(temp_string, Substring, 7);

    // The Substring[] array contains pointers to each substring in
    // the original data string.

    if (Substring[0] == NULL)  // No GPGGA string
        return(ok);

    if (Substring[1] == NULL)  // No latitude string
        return(ok);

    if (Substring[2] == NULL)   // No N/S string
        return(ok);

    if (Substring[3] == NULL)   // No longitude string
        return(ok);

    if (Substring[4] == NULL)   // No E/W string
        return(ok);

    temp_char = toupper((int)Substring[2][0]);
    if (temp_char != 'N' && temp_char != 'S')
        return(ok);

    xastir_snprintf(lat_s,
        sizeof(lat_s),
        "%s%c",
        Substring[1],
        temp_char);
// Need to check lat_s for validity here.  Note that some GPS's put out another digit of precision
// (4801.1234).  Next character after digits should be a ','

    temp_char = toupper((int)Substring[4][0]);
    if (temp_char != 'E' && temp_char != 'W')
        return(ok);

    xastir_snprintf(long_s,
        sizeof(long_s),
        "%s%c",
        Substring[3],
        temp_char);
// Need to check long_s for validity here.  Should be all digits.  Note that some GPS's put out another
// digit of precision.  (12201.1234).  Next character after digits should be a ','

    p_station->coord_lat = convert_lat_s2l(lat_s);
    p_station->coord_lon = convert_lon_s2l(long_s);
    ok = 1; // We have enough for a position now

    xastir_snprintf(p_station->course,
        sizeof(p_station->course),
        "000.0");  // Fill in with dummy values
    p_station->speed[0] = '\0';        // Fill in with dummy values

    // A is valid, V is a warning but we can get good data still?
    // We don't currently check the data valid flag.

    return(ok);
}





// Add a status line to the linked-list of status records
// associated with a station.  Note that a blank status line is
// allowed, but we don't store that unless we have seen a non-blank
// status line previously.
//
void add_status(DataRow *p_station, char *status_string) {
    CommentRow *ptr;
    int add_it = 0;
    int len;


    len = strlen(status_string);

    // Eliminate line-end chars
    if (len > 1) {
        if ( (status_string[len-1] == '\n')
                || (status_string[len-1] == '\r') ) {
            status_string[len-1] = '\0';
        }
    }

    // Shorten it
    //fprintf(stderr,"1Status: (%s)\n",status_string);
    (void)remove_trailing_spaces(status_string);
    //fprintf(stderr,"2Status: (%s)\n",status_string);
    (void)remove_leading_spaces(status_string);
    //fprintf(stderr,"3Status: (%s)\n",status_string);
 
    len = strlen(status_string);

    // Check for valid pointer
    if (p_station != NULL) {

// We should probably create a new station record for this station
// if there isn't one.  This allows us to collect as much info about
// a station as we can until a posit comes in for it.  Right now we
// don't do this.  If we decide to do this in the future, we also
// need a method to find out the info about that station without
// having to click on an icon, 'cuz the symbol won't be on our map
// until we have a posit.

        //fprintf(stderr,"Station:%s\tStatus:%s\n",p_station->call_sign,status_string);

        // Check whether we have any data stored for this station
        if (p_station->status_data == NULL) {
            if (len > 0) {
                // No status stored yet and new status is non-NULL,
                // so add it to the list.
                add_it++;
            }
        }
        else {  // We have status data stored already
                // Check for an identical string
            CommentRow *ptr2;
            int ii = 0;
 
            ptr = p_station->status_data;
            ptr2 = ptr;
            while (ptr != NULL) {

                // Note that both text_ptr and comment_string can be
                // empty strings.

                if (strcasecmp(ptr->text_ptr, status_string) == 0) {
                    // Found a matching string
                    //fprintf(stderr,"Found match:
                    //%s:%s\n",p_station->call_sign,status_string);

// Instead of updating the timestamp, we'll delete the record from
// the list and add it to the top in the code below.  Make sure to
// tweak the "ii" pointer so that we don't end up shortening the
// list unnecessarily.
                    if (ptr == p_station->status_data) {

                        // Only update the timestamp: We're at the
                        // beginning of the list already.
                        ptr->sec_heard = sec_now();

                        return; // No need to add a new record
                    }
                    else {  // Delete the record
                        CommentRow *ptr3;

                        // Keep a pointer to the record
                        ptr3 = ptr;

                        // Close the chain, skipping this record
                        ptr2->next = ptr3->next;

                        // Skip "ptr" over the record we wish to
                        // delete
                        ptr = ptr3->next;

                        // Free the record
                        free(ptr3->text_ptr);
                        free(ptr3);

                        // Muck with the counter 'cuz we just
                        // deleted one record
                        ii--;
                    }
                }
                ptr2 = ptr; // Back one record
                if (ptr != NULL) {
                    ptr = ptr->next;
                }
                ii++;
            }


            // No matching string found, or new timestamp found for
            // old record.  Add it to the top of the list.
            add_it++;
            //fprintf(stderr,"No match:
            //%s:%s\n",p_station->call_sign,status_string);

            // We counted the records.  If we have more than
            // MAX_STATUS_LINES records we'll delete/free the last
            // one to make room for the next.  This keeps us from
            // storing unique status records ad infinitum for active
            // stations, limiting the total space used.
            //
            if (ii >= MAX_STATUS_LINES) {
                // We know we didn't get a match, and that our list
                // is full (as full as we want it to be).  Traverse
                // the list again, looking for ptr2->next->next ==
                // NULL.  If found, free last record and set the
                // ptr2->next pointer to NULL.
                ptr2 = p_station->status_data;
                while (ptr2->next->next != NULL) {
                    ptr2 = ptr2->next;
                }
                // At this point, we have a pointer to the last
                // record in ptr2->next.  Free it and the text
                // string in it.
                free(ptr2->next->text_ptr);
                free(ptr2->next);
                ptr2->next = NULL;
            } 
        }

        if (add_it) {   // We add to the beginning so we don't have
                        // to traverse the linked list.  This also
                        // puts new records at the beginning of the
                        // list to keep them in sorted order.

            ptr = p_station->status_data;  // Save old pointer to records
            p_station->status_data = (CommentRow *)malloc(sizeof(CommentRow));
            CHECKMALLOC(p_station->status_data);

            p_station->status_data->next = ptr;    // Link in old records or NULL

            // Malloc the string space we'll need, attach it to our
            // new record
            p_station->status_data->text_ptr = (char *)malloc(sizeof(char) * (len+1));
            CHECKMALLOC(p_station->status_data->text_ptr);

            // Fill in the string
            xastir_snprintf(p_station->status_data->text_ptr,
                len+1,
                "%s",
                status_string);

            // Fill in the timestamp
            p_station->status_data->sec_heard = sec_now();

            //fprintf(stderr,"Station:%s\tStatus:%s\n\n",p_station->call_sign,p_station->status_data->text_ptr);
        }
    }
}




 
// Add a comment line to the linked-list of comment records
// associated with a station.  Note that a blank comment is allowed
// and necessary for the times when we wish to blank out the comment
// on an object/item, but we don't store that unless we have seen a
// non-blank comment line previously.
//
void add_comment(DataRow *p_station, char *comment_string) {
    CommentRow *ptr;
    int add_it = 0;
    int len;


    len = strlen(comment_string);

    // Eliminate line-end chars
    if (len > 1) {
        if ( (comment_string[len-1] == '\n')
                || (comment_string[len-1] == '\r') ) {
            comment_string[len-1] = '\0';
        }
    }

    // Shorten it
    //fprintf(stderr,"1Comment: (%s)\n",comment_string);
    (void)remove_trailing_spaces(comment_string);
    //fprintf(stderr,"2Comment: (%s)\n",comment_string);
    (void)remove_leading_spaces(comment_string);
    //fprintf(stderr,"3Comment: (%s)\n",comment_string);
 
    len = strlen(comment_string);

    // Check for valid pointer
    if (p_station != NULL) {

        // Check whether we have any data stored for this station
        if (p_station->comment_data == NULL) {
            if (len > 0) {
                // No comments stored yet and new comment is
                // non-NULL, so add it to the list.
                add_it++;
            }
        }
        else {  // We have comment data stored already
                // Check for an identical string
            CommentRow *ptr2;
            int ii = 0;
 
            ptr = p_station->comment_data;
            ptr2 = ptr;
            while (ptr != NULL) {

                // Note that both text_ptr and comment_string can be
                // empty strings.

                if (strcasecmp(ptr->text_ptr, comment_string) == 0) {
                    // Found a matching string
//fprintf(stderr,"Found match: %s:%s\n",p_station->call_sign,comment_string);

// Instead of updating the timestamp, we'll delete the record from
// the list and add it to the top in the code below.  Make sure to
// tweak the "ii" pointer so that we don't end up shortening the
// list unnecessarily.
                    if (ptr == p_station->comment_data) {
                        // Only update the timestamp:  We're at the
                        // beginning of the list already.
                        ptr->sec_heard = sec_now();

                        return; // No need to add a new record
                    }
                    else {  // Delete the record
                        CommentRow *ptr3;

                        // Keep a pointer to the record
                        ptr3 = ptr;

                        // Close the chain, skipping this record
                        ptr2->next = ptr3->next;

                        // Skip "ptr" over the record we with to
                        // delete
                        ptr = ptr3->next;

                        // Free the record
                        free(ptr3->text_ptr);
                        free(ptr3);

                        // Muck with the counter 'cuz we just
                        // deleted one record
                        ii--;
                    }
                }
                ptr2 = ptr; // Keep this pointer one record back as
                            // we progress.

                if (ptr != NULL) {
                    ptr = ptr->next;
                }

                ii++;
            }
            // No matching string found, or new timestamp found for
            // old record.  Add it to the top of the list.
            add_it++;
            //fprintf(stderr,"No match: %s:%s\n",p_station->call_sign,comment_string);

            // We counted the records.  If we have more than
            // MAX_COMMENT_LINES records we'll delete/free the last
            // one to make room for the next.  This keeps us from
            // storing unique comment records ad infinitum for
            // active stations, limiting the total space used.
            //
            if (ii >= MAX_COMMENT_LINES) {

                // We know we didn't get a match, and that our list
                // is full (as we want it to be).  Traverse the list
                // again, looking for ptr2->next->next == NULL.  If
                // found, free that last record and set the
                // ptr2->next pointer to NULL.
                ptr2 = p_station->comment_data;
                while (ptr2->next->next != NULL) {
                    ptr2 = ptr2->next;
                }
                // At this point, we have a pointer to the last
                // record in ptr2->next.  Free it and the text
                // string in it.
                free(ptr2->next->text_ptr);
                free(ptr2->next);
                ptr2->next = NULL;
            } 
        }

        if (add_it) {   // We add to the beginning so we don't have
                        // to traverse the linked list.  This also
                        // puts new records at the beginning of the
                        // list to keep them in sorted order.

            ptr = p_station->comment_data;  // Save old pointer to records
            p_station->comment_data = (CommentRow *)malloc(sizeof(CommentRow));
            CHECKMALLOC(p_station->comment_data);

            p_station->comment_data->next = ptr;    // Link in old records or NULL

            // Malloc the string space we'll need, attach it to our
            // new record
            p_station->comment_data->text_ptr = (char *)malloc(sizeof(char) * (len+1));
            CHECKMALLOC(p_station->comment_data->text_ptr);

            // Fill in the string
            xastir_snprintf(p_station->comment_data->text_ptr,
                len+1,
                "%s",
                comment_string);

            // Fill in the timestamp
            p_station->comment_data->sec_heard = sec_now();
        }
    }
}
 




/*
 *  Add data from APRS information field to station database
 *  Returns a 1 if successful
 */
int data_add(int type ,char *call_sign, char *path, char *data, char from, int port, char *origin, int third_party) {
    DataRow *p_station;
    DataRow *p_time;
    char call[MAX_CALLSIGN+1];
    char new_station;
    long last_lat, last_lon;
    char last_alt[MAX_ALTITUDE];
    char last_speed[MAX_SPEED+1];
    char last_course[MAX_COURSE+1];
    time_t last_stn_sec;
    short last_flag;
    char temp_data[40]; // short term string storage, MAX_CALLSIGN, ...
    long l_lat, l_lon;
    double distance;
    char station_id[600];
    int found_pos;
    float value;
    WeatherRow *weather;
    int moving;
    int changed_pos;
    int screen_update;
    int ok, store;
    int ok_to_display;
    int compr_pos;
    int my_object = 0;
    char *p = NULL; // KC2ELS - used for WIDEn-N
    int direct = 0;

    // call and path had been validated before
    // Check "data" against the max APRS length, and dump the packet if too long.
    if ( (data != NULL) && (strlen(data) > MAX_INFO_FIELD_SIZE) ) {   // Overly long packet.  Throw it away.
        if (debug_level & 1)
            fprintf(stderr,"data_add: Overly long packet.  Throwing it away.\n");
        return(0);  // Not an ok packet
    }

    if (debug_level & 1)
        fprintf(stderr,"data_add:\n\ttype: %d\n\tcall_sign: %s\n\tpath: %s\n\tdata: %s\n\tfrom: %c\n\tport: %d\n\torigin: %s\n\tthird_party: %d\n",
            type,
            call_sign,
            path,
            data ? data : "NULL",       // This parameter may be NULL, if exp1 then exp2 else exp3 
            from,
            port,
            origin ? origin : "NULL",   // This parameter may be NULL
            third_party);

    weather = NULL; // only to make the compiler happy...
    found_pos = 1;
    xastir_snprintf(call,
        sizeof(call),
        "%s",
        call_sign);
    p_station = NULL;
    new_station = (char)FALSE;                          // to make the compiler happy...
    last_lat = 0L;
    last_lon = 0L;
    last_stn_sec = sec_now();
    last_alt[0]    = '\0';
    last_speed[0]  = '\0';
    last_course[0] = '\0';
    last_flag      = 0;
    ok = 0;
    store = 0;
    p_time = NULL;                                      // add to end of time sorted list (newest)
    compr_pos = 0;

    if (search_station_name(&p_station,call,1)) {       // If we found the station in our list

        // Check whether it's a locally-owned object/item
        if ( (is_my_call(p_station->origin,1))                  // If station is owned by me
                && ( ((p_station->flag & ST_OBJECT) != 0)       // And it's an object
                  || ((p_station->flag & ST_ITEM) != 0) ) ) {   // or an item
            // We don't want to re-order it in the time-ordered list
            // so that it'll expire from the queue normally.  Don't
            // call "move_station_time()" here.

            // We need an exception later in this function for the
            // case where we've moved an object/item (by how much?).
            // We need to update the time in this case so that it'll
            // expire later (in fact it could already be expired
            // when we move it).  We should be able to move expired
            // objects/items to make them active again.  Perhaps
            // some other method as well?

            new_station = (char)FALSE;
            my_object++;
        }
        else {
            move_station_time(p_station,p_time);        // update time, change position in time sorted list
            new_station = (char)FALSE;                  // we have seen this one before
        }
    } else {
//fprintf(stderr,"data_add()\n");
        p_station = add_new_station(p_station,p_time,call);     // create storage
        new_station = (char)TRUE;                       // for new station
    }

    if (p_station != NULL) {
        last_lat = p_station->coord_lat;                // remember last position
        last_lon = p_station->coord_lon;
        last_stn_sec = p_station->sec_heard;
        xastir_snprintf(last_alt,
            sizeof(last_alt),
            "%s",
            p_station->altitude);
        xastir_snprintf(last_speed,
            sizeof(last_speed),
            "%s",
            p_station->speed);
        xastir_snprintf(last_course,
            sizeof(last_course),    
            "%s",
            p_station->course);
        last_flag = p_station->flag;

        // Wipe out old data so that it doesn't hang around forever
        p_station->altitude[0] = '\0';
        p_station->speed[0] = '\0';
        p_station->course[0] = '\0';

        ok = 1;                         // succeed as default
        switch (type) {

            case (APRS_MICE):           // Mic-E format
            case (APRS_FIXED):          // '!'
            case (APRS_MSGCAP):         // '='
                if (!extract_position(p_station,&data,type)) {          // uncompressed lat/lon
                    compr_pos = 1;
                    if (!extract_comp_position(p_station,&data,type))   // compressed lat/lon
                        ok = 0;
                    else
                        p_station->pos_amb = 0; // No ambiguity in compressed posits
                }
                if (ok) {
                    // Create a timestamp from the current time
                    xastir_snprintf(p_station->pos_time,
                        sizeof(p_station->pos_time),
                        "%s",
                        get_time(temp_data));
                    (void)extract_storm(p_station,data,compr_pos);
                    (void)extract_weather(p_station,data,compr_pos);    // look for weather data first
                    process_data_extension(p_station,data,type);        // PHG, speed, etc.
                    process_info_field(p_station,data,type);            // altitude

                    if ( (p_station->coord_lat > 0) && (p_station->coord_lon > 0) )
                        extract_multipoints(p_station, data, type, 1);
 
                    add_comment(p_station,data);

                    p_station->record_type = NORMAL_APRS;
                    if (type == APRS_MSGCAP)
                        p_station->flag |= ST_MSGCAP;           // set "message capable" flag
                    else
                        p_station->flag &= (~ST_MSGCAP);        // clear "message capable" flag
                }
                break;

            case (APRS_DOWN):           // '/'
                ok = extract_time(p_station, data, type);               // we need a time
                if (ok) {
                    if (!extract_position(p_station,&data,type)) {      // uncompressed lat/lon
                        compr_pos = 1;
                        if (!extract_comp_position(p_station,&data,type)) // compressed lat/lon
                            ok = 0;
                        else
                            p_station->pos_amb = 0; // No ambiguity in compressed posits
                    }
                }
                if (ok) {
                    // Create a timestamp from the current time
                    xastir_snprintf(p_station->pos_time,
                        sizeof(p_station->pos_time),
                        "%s",
                        get_time(temp_data));
                    process_data_extension(p_station,data,type);        // PHG, speed, etc.
                    process_info_field(p_station,data,type);

                    if ( (p_station->coord_lat > 0) && (p_station->coord_lon > 0) )
                        extract_multipoints(p_station, data, type, 1);
 
                    add_comment(p_station,data);

                    p_station->record_type = DOWN_APRS;
                    p_station->flag &= (~ST_MSGCAP);            // clear "message capable" flag
                }
                break;

            case (APRS_DF):             // '@'
            case (APRS_MOBILE):         // '@'
                ok = extract_time(p_station, data, type);               // we need a time
                if (ok) {
                    if (!extract_position(p_station,&data,type)) {      // uncompressed lat/lon
                        compr_pos = 1;
                        if (!extract_comp_position(p_station,&data,type)) // compressed lat/lon
                            ok = 0;
                        else
                            p_station->pos_amb = 0; // No ambiguity in compressed posits
                    }
                }
                if (ok) {
                    process_data_extension(p_station,data,type);        // PHG, speed, etc.
                    process_info_field(p_station,data,type);

                    if ( (p_station->coord_lat > 0) && (p_station->coord_lon > 0) )
                        extract_multipoints(p_station, data, type, 1);
 
                    add_comment(p_station,data);

                    if(type == APRS_MOBILE)
                        p_station->record_type = MOBILE_APRS;
                    else
                        p_station->record_type = DF_APRS;
                    //@ stations have messaging per spec
                    p_station->flag |= (ST_MSGCAP);            // set "message capable" flag
                }
                break;

            case (APRS_GRID):
                ok = extract_position(p_station, &data, type);
                if (ok) { 
                    if (debug_level & 1)
                        fprintf(stderr,"data_add: Got grid data for %s\n", call);

                    if ( (p_station->coord_lat > 0) && (p_station->coord_lon > 0) )
                        extract_multipoints(p_station, data, type, 1);
 
                    add_comment(p_station,data);

                } else {
                    if (debug_level & 1)
                        fprintf(stderr,"data_add: Bad grid data for %s : %s\n", call, data);
                }
                break;

            case (STATION_CALL_DATA):
                p_station->record_type = NORMAL_APRS;
                found_pos = 0;
                break;

            case (APRS_STATUS):         // '>' Status Reports     [APRS Reference, chapter 16]
                (void)extract_time(p_station, data, type);              // we need a time
                // todo: could contain Maidenhead or beam heading+power

                if ( (p_station->coord_lat > 0) && (p_station->coord_lon > 0) )
                    extract_multipoints(p_station, data, type, 1);
 
                add_status(p_station,data);

                p_station->flag |= (ST_STATUS);                         // set "Status" flag
                p_station->record_type = NORMAL_APRS;                   // ???
                found_pos = 0;
                break;

            case (OTHER_DATA):          // Other Packets          [APRS Reference, chapter 19]
                // non-APRS beacons, treated as status reports until we get a real one

                if ( (p_station->coord_lat > 0) && (p_station->coord_lon > 0) )
                    extract_multipoints(p_station, data, type, 1);
 
                if ((p_station->flag & (~ST_STATUS)) == 0) {            // only store if no status yet

                    add_status(p_station,data);

                    p_station->record_type = NORMAL_APRS;               // ???
                }
                found_pos = 0;
                break;

            case (APRS_OBJECT):
                // If old match is a killed Object (owner doesn't
                // matter), new one is an active Object and owned by
                // us, remove the old record and create a new one
                // for storing this Object.  Do the same for Items
                // in the next section below.
                //
                // The easiest implementation might be to remove the
                // old record and then call this routine again with
                // the same parameters, which will cause a brand-new
                // record to be created.
                //
                // The new record we're processing is an active
                // object, as data_add() won't be called on a killed
                // object.
                //
                if ( is_my_call(origin,1)  // If new Object is owned by me
                        && !(p_station->flag & ST_ACTIVE)
                        && (p_station->flag & ST_OBJECT) ) {  // Old record was a killed Object
                    remove_name(p_station);  // Remove old killed Object
                    redo_list = (int)TRUE;
                    return( data_add(type, call_sign, path, data, from, port, origin, third_party) );
                }
 
                ok = extract_time(p_station, data, type);               // we need a time
                if (ok) {
                    if (!extract_position(p_station,&data,type)) {      // uncompressed lat/lon
                        compr_pos = 1;
                        if (!extract_comp_position(p_station,&data,type)) // compressed lat/lon
                            ok = 0;
                        else
                            p_station->pos_amb = 0; // No ambiguity in compressed posits
                    }
                }
                p_station->flag |= ST_OBJECT;                           // Set "Object" flag
                if (ok) {

                    // If object was owned by me but another station
                    // is transmitting it now, write entries into
                    // the object.log file showing that we don't own
                    // this object anymore.
                    if ( (is_my_call(p_station->origin,1))  // If station was owned by me
                            && (!is_my_call(origin,1)) ) {  // But isn't now
                        disown_object_item(call_sign,origin);
                    }

                    // If station is owned by me but it's a new
                    // object/item
                    if ( (is_my_call(p_station->origin,1))
                            && (p_station->transmit_time_increment == 0) ) {
                        // This will get us transmitting this object
                        // on the decaying algorithm schedule.
                        // We've transmitted it once if we've just
                        // gotten to this code.
                        p_station->transmit_time_increment = OBJECT_CHECK_RATE;
//fprintf(stderr,"data_add(): Setting transmit_time_increment to %d\n", OBJECT_CHECK_RATE);
                    }
 
                    // Create a timestamp from the current time
                    xastir_snprintf(p_station->pos_time,
                        sizeof(p_station->pos_time),
                        "%s",
                        get_time(temp_data));

                    xastir_snprintf(p_station->origin,
                        sizeof(p_station->origin),
                        "%s",
                        origin);                   // define it as object
                    (void)extract_storm(p_station,data,compr_pos);
                    (void)extract_weather(p_station,data,compr_pos);    // look for wx info
                    process_data_extension(p_station,data,type);        // PHG, speed, etc.
                    process_info_field(p_station,data,type);

                    if ( (p_station->coord_lat > 0) && (p_station->coord_lon > 0) )
                        extract_multipoints(p_station, data, type, 0);
 
                    add_comment(p_station,data);

                    // the last char always was missing...
                    //p_station->comments[ strlen(p_station->comments) - 1 ] = '\0';  // Wipe out '\n'
                    // moved that to decode_ax25_line
                    // and don't added a '\n' in interface.c
                    p_station->record_type = NORMAL_APRS;
                    p_station->flag &= (~ST_MSGCAP);            // clear "message capable" flag
                }

                break;

            case (APRS_ITEM):
                // If old match is a killed Item (owner doesn't
                // matter), new one is an active Item and owned by
                // us, remove the old record and create a new one
                // for storing this Item.  Do the same for Objects
                // in the previous section above.
                //
                // The easiest implementation might be to remove the
                // old record and then call this routine again with
                // the same parameters, which will cause a brand-new
                // record to be created.
                //
                // The new record we're processing is an active
                // Item, as data_add() won't be called on a killed
                // Item.
                //
                if ( is_my_call(origin,1)  // If new Item is owned by me
                        && !(p_station->flag & ST_ACTIVE)
                        && (p_station->flag & ST_ITEM) ) {  // Old record was a killed Item
 
                    remove_name(p_station);  // Remove old killed Item
                    redo_list = (int)TRUE;
                    return( data_add(type, call_sign, path, data, from, port, origin, third_party) );
                }
 
                if (!extract_position(p_station,&data,type)) {          // uncompressed lat/lon
                    compr_pos = 1;
                    if (!extract_comp_position(p_station,&data,type))   // compressed lat/lon
                        ok = 0;
                    else
                        p_station->pos_amb = 0; // No ambiguity in compressed posits
                }
                p_station->flag |= ST_ITEM;                             // Set "Item" flag
                if (ok) {

                    // If item was owned by me but another station
                    // is transmitting it now, write entries into
                    // the object.log file showing that we don't own
                    // this item anymore.
                    if ( (is_my_call(p_station->origin,1))  // If station was owned by me
                            && (!is_my_call(origin,1)) ) {  // But isn't now
                        disown_object_item(call_sign,origin);
                    }
 
                    // If station is owned by me but it's a new
                    // object/item
                    if ( (is_my_call(p_station->origin,1))
                            && (p_station->transmit_time_increment == 0) ) {
                        // This will get us transmitting this object
                        // on the decaying algorithm schedule.
                        // We've transmitted it once if we've just
                        // gotten to this code.
                        p_station->transmit_time_increment = OBJECT_CHECK_RATE;
//fprintf(stderr,"data_add(): Setting transmit_time_increment to %d\n", OBJECT_CHECK_RATE);
                    }
 
                    // Create a timestamp from the current time
                    xastir_snprintf(p_station->pos_time,
                        sizeof(p_station->pos_time),
                        "%s",
                        get_time(temp_data));
                    xastir_snprintf(p_station->origin,
                        sizeof(p_station->origin),
                        "%s",
                        origin);                   // define it as item
                    (void)extract_storm(p_station,data,compr_pos);
                    (void)extract_weather(p_station,data,compr_pos);    // look for wx info
                    process_data_extension(p_station,data,type);        // PHG, speed, etc.
                    process_info_field(p_station,data,type);

                    if ( (p_station->coord_lat > 0) && (p_station->coord_lon > 0) )
                        extract_multipoints(p_station, data, type, 0);
 
                    add_comment(p_station,data);

                    // the last char always was missing...
                    //p_station->comments[ strlen(p_station->comments) - 1 ] = '\0';  // Wipe out '\n'
                    // moved that to decode_ax25_line
                    // and don't added a '\n' in interface.c
                    p_station->record_type = NORMAL_APRS;
                    p_station->flag &= (~ST_MSGCAP);            // clear "message capable" flag
                }
                break;

            case (APRS_WX1):    // weather in '@' packet
                ok = extract_time(p_station, data, type);               // we need a time
                if (ok) {
                    if (!extract_position(p_station,&data,type)) {      // uncompressed lat/lon
                        compr_pos = 1;
                        if (!extract_comp_position(p_station,&data,type)) // compressed lat/lon
                            ok = 0;
                        else
                            p_station->pos_amb = 0; // No ambiguity in compressed posits
                    }
                }
                if (ok) {
                    (void)extract_storm(p_station,data,compr_pos);
                    (void)extract_weather(p_station,data,compr_pos);
                    p_station->record_type = (char)APRS_WX1;

                    if ( (p_station->coord_lat > 0) && (p_station->coord_lon > 0) )
                        extract_multipoints(p_station, data, type, 1);
 
                    add_comment(p_station,data);
                }
                break;

            case (APRS_WX2):            // '_'
                ok = extract_time(p_station, data, type);               // we need a time
                if (ok) {
                    (void)extract_storm(p_station,data,compr_pos);
                    (void)extract_weather(p_station,data,0);            // look for weather data first
                    p_station->record_type = (char)APRS_WX2;
                    found_pos = 0;

                    if ( (p_station->coord_lat > 0) && (p_station->coord_lon > 0) )
                        extract_multipoints(p_station, data, type, 1);
                }
                break;

            case (APRS_WX4):            // '#'          Peet Bros U-II (mph)
            case (APRS_WX6):            // '*'          Peet Bros U-II (km/h)
            case (APRS_WX3):            // '!'          Peet Bros Ultimeter 2000 (data logging mode)
            case (APRS_WX5):            // '$ULTW'      Peet Bros Ultimeter 2000 (packet mode)
                if (get_weather_record(p_station)) {    // get existing or create new weather record
                    weather = p_station->weather_data;
                    if (type == APRS_WX3)   // Peet Bros Ultimeter 2000 data logging mode
                        decode_U2000_L(1,(unsigned char *)data,weather);
                    else if (type == APRS_WX5) // Peet Bros Ultimeter 2000 packet mode
                        decode_U2000_P(1,(unsigned char *)data,weather);
                    else    // Peet Bros Ultimeter-II
                        decode_Peet_Bros(1,(unsigned char *)data,weather,type);
                    p_station->record_type = (char)type;
                    // Create a timestamp from the current time
                    xastir_snprintf(weather->wx_time,
                        sizeof(weather->wx_time),
                        "%s",
                        get_time(temp_data));
                    weather->wx_sec_time = sec_now();
                    found_pos = 0;
                }
                break;

            case (GPS_RMC):             // $GPRMC
                ok = extract_RMC(p_station,data,call_sign,path);
                break;

            case (GPS_GGA):             // $GPGGA
                ok = extract_GGA(p_station,data,call_sign,path);
                break;

            case (GPS_GLL):             // $GPGLL
                ok = extract_GLL(p_station,data,call_sign,path);
                break;

            default:
                fprintf(stderr,"ERROR: UNKNOWN TYPE in data_add\n");
                ok = 0;
                break;
        }

        // Left this one in, just in case.  Perhaps somebody might
        // attach a multipoint string onto the end of a packet we
        // might not expect.  For this case we need to check whether
        // we have multipoints first, as we don't want to erase the
        // work we might have done with a previous call to
        // extract_multipoints().
        if (ok && (p_station->coord_lat > 0)
                && (p_station->coord_lon > 0)
                && (p_station->num_multipoints == 0) )  // No multipoints found yet
            extract_multipoints(p_station, data, type, 0);
    }

    if (!ok) {  // non-APRS beacon, treat it as Other Packet   [APRS Reference, chapter 19]

        if (debug_level & 1) {
            char filtered_data[MAX_LINE_SIZE + 1];

            xastir_snprintf(filtered_data,
                sizeof(filtered_data),
                "%s",
                data-1);
            makePrintable(filtered_data);
            fprintf(stderr,"store non-APRS data as status: %s: |%s|\n",call,filtered_data);
        }

        // GPRMC etc. without a position is here too, but it should not be stored as status!
        
        // store it as status report until we get a real one
        if ((p_station->flag & (~ST_STATUS)) == 0) {         // only store it if no status yet

            add_status(p_station,data-1);

            p_station->record_type = NORMAL_APRS;               // ???
        }
        ok = 1;            
        found_pos = 0;
    }

    curr_sec = sec_now();
    if (ok) {
        // data packet is valid
        // announce own echo, we soon discard that packet...
        if (!new_station && is_my_call(p_station->call_sign,1)
                && strchr(path,'*') != NULL) {
            upd_echo(path);   // store digi that echoes my signal...
            statusline(langcode("BBARSTA033"),0);   // Echo from digipeater
        }
        // check if data is just a secondary echo from another digi
        if ((last_flag & ST_VIATNC) == 0
                || (curr_sec - last_stn_sec) > 15
                || p_station->coord_lon != last_lon 
                || p_station->coord_lat != last_lat)
            store = 1;                     // don't store secondary echos
    }

    if (!ok && new_station)
        delete_station_memory(p_station);       // remove unused record

    if (store) {
        // we now have valid data to store into database
        // make time index unique by adding a serial number

        // Check whether it's a locally-owned object/item
        if (my_object) {
            // Do nothing.  We don't want to update the last-heard time
            // so that it'll expire from the queue normally.

            // We need an exception later in this function for the
            // case where we've moved an object/item (by how much?).
            // We need to update the time in this case so that it'll
            // expire later (in fact it could already be expired
            // when we move it).  We should be able to move expired
            // objects/items to make them active again.  Perhaps
            // some other method as well?.
        }
        else {
            p_station->sec_heard = curr_sec;    // Give it a new timestamp
        }

        if (curr_sec != last_sec) {     // todo: if old time calculate time_sn from database
            last_sec = curr_sec;
            next_time_sn = 0;           // restart time serial number
        }

        p_station->time_sn = next_time_sn++;            // todo: warning if serial number too high
        if (from == DATA_VIA_TNC) {                     // heard via TNC
            if (!third_party) { // Not a third-party packet
                p_station->flag |= ST_VIATNC;               // set "via TNC" flag
                p_station->heard_via_tnc_last_time = sec_now();
                p_station->heard_via_tnc_port = port;
            }
            else {  // Third-party packet
                // Leave the previous setting of "flag" alone.
                // Specifically do NOT set the ST_VIATNC flag if it
                // was a third-party packet.
            }
        } else {  // heard other than TNC
            if (new_station) {  // new station
                p_station->flag &= (~ST_VIATNC);  // clear "via TNC" flag
                p_station->heard_via_tnc_last_time = 0l;
            }
        }
        p_station->last_port_heard = port;
        p_station->data_via = from;
        // Create a timestamp from the current time
        xastir_snprintf(p_station->packet_time,
            sizeof(p_station->packet_time),
            "%s",
            get_time(temp_data)); // get_time returns value in temp_data

        p_station->flag |= ST_ACTIVE;
        if (third_party)
            p_station->flag |= ST_3RD_PT;               // set "third party" flag
        else
            p_station->flag &= (~ST_3RD_PT);            // clear "third party" flag
        if (origin != NULL && strcmp(origin,"INET") == 0)  // special treatment for inet names
            xastir_snprintf(p_station->origin,
                sizeof(p_station->origin),
                "%s",
                origin);           // to keep them separated from calls
        if (origin != NULL && strcmp(origin,"INET-NWS") == 0)  // special treatment for NWS
            xastir_snprintf(p_station->origin,
                sizeof(p_station->origin),
                "%s",
                origin);           // to keep them separated from calls
        if (origin == NULL || origin[0] == '\0')        // normal call
            p_station->origin[0] = '\0';                // undefine possible former object with same name


        //--------------------------------------------------------------------

        // KC2ELS
        // Okay, here are the standards for ST_DIRECT:
        // 1.  The packet must have been received via TNC.
        // 2.  The packet must not have any * flags.
        // 3.  If present, the first WIDEn-N (or TRACEn-N) must have n=N.
        // A station retains the ST_DIRECT setting.  If
        // "st_direct_timeout" seconds have passed since we set
        // that bit then APRSD queries and displays based on the
        // ST_DIRECT bit will skip that station.

// In order to make this scheme work for stations that straddle both
// RF and INET, we need to make sure that node_path_ptr doesn't get
// overwritten with an INET path if there's an RF path already in
// there and it has been less than st_direct_timeout seconds since
// the station was last heard on RF.

        if ((from == DATA_VIA_TNC)             // Heard via TNC
                && !third_party                // Not a 3RD-Party packet
                && path != NULL                // Path is not NULL
                && strchr(path,'*') == NULL) { // No asterisk found

            // Look for WIDE or TRACE
            if ((((p = strstr(path,"WIDE")) != NULL) 
                    && (p+=4)) || 
                    (((p = strstr(path,"TRACE")) != NULL) 
                    && (p+=5))) {

                // Look for n=N on WIDEn-N/TRACEn-N digi field
                if ((*p != '\0') && isdigit((int)*p)) {
                    if ((*(p+1) != '\0') && (*(p+1) == '-')) {
                        if ((*(p+2) != '\0') && isdigit((int)*(p+2))) {
                            if (*(p) == *(p+2)) {
                                direct = 1;
                            }
                            else {
                                direct = 0;
                            }
                        }
                        else {
                            direct = 0;
                        }
                    }
                    else {
                        direct = 0;
                    }
                }
                else {
                    direct = 1;
                }
            }
            else {
                direct = 1;
            }
        }
        else {
            direct = 0;
        }

        if (direct == 1) {
            // This packet was heard direct.  Set the ST_DIRECT bit.
            if (debug_level & 1) {
                fprintf(stderr,"Setting ST_DIRECT for station %s\n", 
                    p_station->call_sign);
            }
            p_station->direct_heard = sec_now();
            p_station->flag |= (ST_DIRECT);
        }
        else {
            // This packet was NOT heard direct.  Check whether we
            // need to expire the ST_DIRECT bit.  A lot of fixed
            // stations transmit every 30 minutes.  One hour gives
            // us time to receive a direct packet from them among
            // all the digipeated packets.

            if ((p_station->flag & ST_DIRECT) != 0 &&
                    sec_now() > (p_station->direct_heard + st_direct_timeout)) {
                if (debug_level & 1)
                    fprintf(stderr,"Clearing ST_DIRECT for station %s\n", 
                        p_station->call_sign);
                p_station->flag &= (~ST_DIRECT);
            }
        }

        // If heard on TNC then overwrite node_path_ptr.  If heard
        // on INET then overwrite node_path_ptr only if
        // heard_via_tnc_last_time is older than one hour (zero
        // counts as well!), plus clear the ST_DIRECT and ST_VIATNC
        // bits in this case.  This makes us keep the RF path around
        // for at least one hour after the station is heard.
        //
        if ((from == DATA_VIA_TNC)  // Heard via TNC
                && !third_party     // Not a 3RD-Party packet
                && path != NULL) {  // Path is not NULL

            // Heard on TNC interface
 
            // Free any old path we might have
            if (p_station->node_path_ptr != NULL)
                free(p_station->node_path_ptr);
            // Malloc and store the new path
            p_station->node_path_ptr = (char *)malloc(strlen(path) + 1);
            CHECKMALLOC(p_station->node_path_ptr);

            substr(p_station->node_path_ptr,path,strlen(path));
        }
        else if (from != DATA_VIA_TNC  // From an INET interface
                && !third_party        // Not a 3RD-Party packet
                && path != NULL) {     // Path is not NULL

            // Heard on INET interface.  Check if
            // heard_via_tnc_last_time is older than an hour.  If
            // so, overwrite the path and clear a few bits to show
            // that it has timed out on RF and we're now receiving
            // that station from the INET feeds.
            //
            if (sec_now() > (p_station->heard_via_tnc_last_time + 60*60)) {

                // Yep, more than one hour old or is a zero,
                // overwrite the node_path_ptr variable with the new
                // one.  We're only hearing this station on INET
                // now.

                // Free any old path we might have
                if (p_station->node_path_ptr != NULL)
                    free(p_station->node_path_ptr);
                // Malloc and store the new path
                p_station->node_path_ptr = (char *)malloc(strlen(path) + 1);
                CHECKMALLOC(p_station->node_path_ptr);

                substr(p_station->node_path_ptr,path,strlen(path));

                // Clear the ST_VIATNC bit
                p_station->flag &= ~ST_VIATNC;
            }

            // If direct_heard is over an hour old, clear the
            // ST_DIRECT flag.  We're only hearing this station on
            // INET now.
            // 
            if (sec_now() > (p_station->direct_heard + st_direct_timeout)) {

                // Yep, more than one hour old or is a zero, clear
                // the ST_DIRECT flag.
                p_station->flag &= ~ST_DIRECT;
            }
        }
 
 
        //---------------------------------------------------------------------

        p_station->num_packets += 1;
        redo_list = (int)TRUE;          // we may need to update the lists

        if (found_pos) {        // if station has a position with the data
            if (position_on_extd_screen(p_station->coord_lat,p_station->coord_lon)) {
                p_station->flag |= (ST_INVIEW);   // set   "In View" flag
                if (debug_level & 256)
                    fprintf(stderr,"Setting ST_INVIEW flag\n");
            }
            else {
                p_station->flag &= (~ST_INVIEW);  // clear "In View" flag
                if (debug_level & 256)
                    fprintf(stderr,"Clearing ST_INVIEW flag\n");
            }
        }

        screen_update = 0;
        if (new_station) {
            if (debug_level & 256) {
                fprintf(stderr,"New Station %s\n", p_station->call_sign);
            }
            if (strlen(p_station->speed) > 0 && atof(p_station->speed) > 0) {
                p_station->flag |= (ST_MOVING); // it has a speed, so it's moving
                moving = 1;
            }
            if (position_on_screen(p_station->coord_lat,p_station->coord_lon)) {

                if (p_station->coord_lat != 0 && p_station->coord_lon != 0) {   // discard undef positions from screen
                    if (!altnet || is_altnet(p_station) ) {
                        display_station(da,p_station,1);
                        screen_update = 1;  // ???
                    }
                }
            }
        } else {        // we had seen this station before...
            if (debug_level & 256) {
                fprintf(stderr,"New Data for %s %ld %ld\n", p_station->call_sign,
                    p_station->coord_lat, p_station->coord_lon);
            }
            if (found_pos && position_defined(p_station->coord_lat,p_station->coord_lon,1)) { // ignore undefined and 0N/0E
                if (debug_level & 256) {
                    fprintf(stderr,"  Valid position for %s\n",
                        p_station->call_sign);
                }
                if (p_station->newest_trackpoint != NULL) {
                    if (debug_level & 256) {
                        fprintf(stderr,"Station has a trail: %s\n",
                            p_station->call_sign);
                    }
                    moving = 1;                         // it's moving if it has a trail
                }
                else {
                    if (strlen(p_station->speed) > 0 && atof(p_station->speed) > 0) {
                        if (debug_level & 256) {
                            fprintf(stderr,"Speed detected on %s\n",
                                p_station->call_sign);
                        }
                        moving = 1;                     // declare it moving, if it has a speed
                    }
                    else {
                        if (debug_level & 256) {
                            fprintf(stderr,"Position defined: %d, Changed: %s\n",
                                position_defined(last_lat, last_lon, 1),
                                (p_station->coord_lat != last_lat ||
                                p_station->coord_lon != last_lon) ?
                                "Yes" : "No");
                        }

                        // Here's where we detect movement
                        if (position_defined(last_lat,last_lon,1)
                                && (p_station->coord_lat != last_lat || p_station->coord_lon != last_lon)) {
                            if (debug_level & 256) {
                                fprintf(stderr,"Position Change detected on %s\n",
                                    p_station->call_sign);
                            }
                            moving = 1;                 // it's moving if it has changed the position
                        }
                        else {
                            if (debug_level & 256) {
                                fprintf(stderr,"Station %s still appears stationary.\n",
                                    p_station->call_sign);
                                fprintf(stderr," %s stationary at %ld %ld (%ld %ld)\n",
                                    p_station->call_sign,
                                    p_station->coord_lat, p_station->coord_lon,
                                    last_lat,             last_lon);
                            }
                            moving = 0;
                        }
                    }
                }
                changed_pos = 0;
                if (moving == 1) {                      
                    p_station->flag |= (ST_MOVING);
                    // we have a moving station, process trails
                    if (atoi(p_station->speed) < TRAIL_MAX_SPEED) {     // reject high speed data (undef gives 0)
                        // we now may already have the 2nd position, so store the old one first
                        if (debug_level & 256) {
                            fprintf(stderr,"Station %s valid speed %s\n",
                                p_station->call_sign, p_station->speed);
                        }
                        if (p_station->newest_trackpoint == NULL) {
                            if (debug_level & 256) {
                                fprintf(stderr,"Station %s no trail history.\n",
                                    p_station->call_sign);
                            }
                            if (position_defined(last_lat,last_lon,1)) {  // ignore undefined and 0N/0E
                                if (debug_level & 256) {
                                    fprintf(stderr,"Storing old position for %s\n",
                                        p_station->call_sign);
                                }
                                (void)store_trail_point(p_station,last_lon,last_lat,last_stn_sec,last_alt,last_speed,last_course,last_flag);
                            }
                        }
                        //if (   p_station->coord_lon != last_lon
                        //    || p_station->coord_lat != last_lat ) {
                        // we don't store redundant points (may change this
                                                // later ?)
                                                //
                        // There are often echoes delayed 15 minutes
                        // or so it looks ugly on the trail, so I
                        // want to discard them This also discards
                        // immediate echoes. Duplicates back in time
                        // up to TRAIL_ECHO_TIME minutes are
                        // discarded.
                        //
                        if (!is_trailpoint_echo(p_station)) {
                            (void)store_trail_point(p_station,
                                p_station->coord_lon,
                                p_station->coord_lat,
                                p_station->sec_heard,
                                p_station->altitude,
                                p_station->speed,
                                p_station->course,
                                p_station->flag);
                            changed_pos = 1;

                            // Check whether it's a locally-owned object/item
                            if (my_object) {

                                // Update time, change position in
                                // time-sorted list to change
                                // expiration time.
                                move_station_time(p_station,p_time);

                                // Give it a new timestamp
                                p_station->sec_heard = curr_sec;
                                //fprintf(stderr,"Updating last heard time\n");
                            }
                        }
                        else if (debug_level & 256) {
                            fprintf(stderr,"Trailpoint echo detected for %s\n",
                                p_station->call_sign);
                        }
                    }
                    else {
                        if (debug_level & 256 || debug_level & 1)
                            fprintf(stderr,"Speed over %d mph\n",TRAIL_MAX_SPEED);
                    }

                    if (track_station_on == 1)          // maybe we are tracking a station
                        track_station(da,tracking_station_call,p_station);
                } // moving...

                // now do the drawing to the screen
                ok_to_display = !altnet || is_altnet(p_station); // Optimization step, needed twice below.
                screen_update = 0;
                if (changed_pos == 1 && Display_.trail && ((p_station->flag & ST_INVIEW) != 0)) {
                    if (ok_to_display) {
                        if (debug_level & 256) {
                            fprintf(stderr,"Adding Solid Trail for %s\n",
                            p_station->call_sign);
                        }
                        draw_trail(da,p_station,1);         // update trail
                        screen_update = 1;
                    }
                    else if (debug_level & 256) {
                        fprintf(stderr,"Skipped trail for %s (altnet)\n",
                            p_station->call_sign);
                    }
                }
                if (position_on_screen(p_station->coord_lat,p_station->coord_lon)) {
                    if (changed_pos == 1 || !position_defined(last_lat,last_lon,0)) {
                        if (ok_to_display) {
                            display_station(da,p_station,1);// update symbol
                            screen_update = 1;
                        }
                    }
                }
            } // defined position
        }

        if (screen_update) {
            if (p_station->data_via == 'T') {   // Data from local TNC
//WE7U
// or data_via == 'I' and last_port_heard == AGWPE interface
                redraw_on_new_data = 2; // Update all symbols NOW!
            }
            else if (p_station->data_via == 'F') {  // If data from file
                redraw_on_new_data = 1; // Update each 2 secs
            }
//            else if (scale_y > 2048) {  // Wider area of world
            else {
                redraw_on_new_data = 0; // Update each 60 secs
            }
        }

        // announce stations in the status line
        if (!is_my_call(p_station->call_sign,1)
                && !is_my_call(p_station->origin,1)
                && !wait_to_redraw) {
            if (new_station) {
                if (p_station->origin[0] == '\0')   // new station
                    xastir_snprintf(station_id, sizeof(station_id), langcode("BBARSTA001"),p_station->call_sign);
                else                                // new object
                    xastir_snprintf(station_id, sizeof(station_id), langcode("BBARSTA000"),p_station->call_sign);
            } else                                  // updated data
                xastir_snprintf(station_id, sizeof(station_id), langcode("BBARSTA002"),p_station->call_sign);

            statusline(station_id,0);
        }

        // announce new station with sound file or speech synthesis
        if (new_station && !wait_to_redraw) {   // && !is_my_call(p_station->call_sign,1) // ???
            if (sound_play_new_station)
                play_sound(sound_command,sound_new_station);

#ifdef HAVE_FESTIVAL
            if (festival_speak_new_station) {
                char speech_callsign[50];

                xastir_snprintf(speech_callsign,
                    sizeof(speech_callsign),
                    "%s",
                    p_station->call_sign);
                spell_it_out(speech_callsign, 50);

                xastir_snprintf(station_id,
                    sizeof(station_id),
                    "%s, %s",
                    langcode("SPCHSTR010"),
                    speech_callsign);
                SayText(station_id);
            }
#endif  // HAVE_FESTIVAL
        }

        // check for range and DX
        if (found_pos && !is_my_call(p_station->call_sign,1)) {
            // if station has a position with the data
            /* Check Audio Alarms based on incoming packet */
            /* FG don't care if this is on screen or off get position */
            l_lat = convert_lat_s2l(my_lat);
            l_lon = convert_lon_s2l(my_long);

            // Get distance in nautical miles.
            value = (float)calc_distance_course(l_lat,l_lon,
                    p_station->coord_lat,p_station->coord_lon,temp_data,sizeof(temp_data));

            // Convert to whatever measurement value we're currently using
            distance = value * cvt_kn2len;
        
            /* check ranges */
            if ((distance > atof(prox_min)) && (distance < atof(prox_max)) && sound_play_prox_message) {
                xastir_snprintf(station_id, sizeof(station_id), "%s < %.3f %s",p_station->call_sign, distance,
                        english_units?langcode("UNIOP00004"):langcode("UNIOP00005"));
                statusline(station_id,0);
                play_sound(sound_command,sound_prox_message);
                /*fprintf(stderr,"%s> PROX distance %f\n",p_station->call_sign, distance);*/

            //fprintf(stderr,"Station within proximity circle, creating waypoint\n");
            create_garmin_waypoint(p_station->coord_lat,p_station->coord_lon,p_station->call_sign);

            }
#ifdef HAVE_FESTIVAL
            if ((distance > atof(prox_min)) && (distance < atof(prox_max)) && festival_speak_proximity_alert) {
                char speech_callsign[50];

                xastir_snprintf(speech_callsign,
                    sizeof(speech_callsign),
                    "%s",
                    p_station->call_sign);
                spell_it_out(speech_callsign, 50);

                if (english_units) {
                    if (distance < 1.0)
                xastir_snprintf(station_id, sizeof(station_id), langcode("SPCHSTR005"), speech_callsign,
                            (int)(distance * 1760), langcode("SPCHSTR004")); // say it in yards
                    else if ((int)((distance * 10) + 0.5) % 10)
                xastir_snprintf(station_id, sizeof(station_id), langcode("SPCHSTR006"), speech_callsign, distance,
                        langcode("SPCHSTR003")); // say it in miles with one decimal
                    else
                xastir_snprintf(station_id, sizeof(station_id), langcode("SPCHSTR005"), speech_callsign, (int)(distance + 0.5),
                        langcode("SPCHSTR003")); // say it in miles with no decimal
                } else {
                    if (distance < 1.0)
                xastir_snprintf(station_id, sizeof(station_id), langcode("SPCHSTR005"), speech_callsign,
                        (int)(distance * 1000), langcode("SPCHSTR002")); // say it in meters
                    else if ((int)((distance * 10) + 0.5) % 10)
                xastir_snprintf(station_id, sizeof(station_id), langcode("SPCHSTR006"), speech_callsign, distance,
                        langcode("SPCHSTR001")); // say it in kilometers with one decimal
                    else
                xastir_snprintf(station_id, sizeof(station_id), langcode("SPCHSTR005"), speech_callsign, (int)(distance + 0.5),
                    langcode("SPCHSTR001")); // say it in kilometers with no decimal
                }
                SayText(station_id);
            }
#endif  // HAVE_FESTIVAL
            /* FG really should check the path before we do this and add setup for these ranges */
            if ((distance > atof(bando_min)) && (distance < atof(bando_max)) &&
                    sound_play_band_open_message && from == DATA_VIA_TNC) {
                xastir_snprintf(station_id, sizeof(station_id), "%s %s %.1f %s",p_station->call_sign, langcode("UMBNDO0001"),
                        distance, english_units?langcode("UNIOP00004"):langcode("UNIOP00005"));
                statusline(station_id,0);
                play_sound(sound_command,sound_band_open_message);
                /*fprintf(stderr,"%s> BO distance %f\n",p_station->call_sign, distance);*/
            }
#ifdef HAVE_FESTIVAL
            if ((distance > atof(bando_min)) && (distance < atof(bando_max)) &&
                   festival_speak_band_opening && from == DATA_VIA_TNC) {
                char speech_callsign[50];

                xastir_snprintf(speech_callsign,
                    sizeof(speech_callsign),
                    "%s",
                    p_station->call_sign);
                spell_it_out(speech_callsign, 50);

                xastir_snprintf(station_id,
                    sizeof(station_id),
                    langcode("SPCHSTR011"),
                    speech_callsign,
                    distance,
                    english_units?langcode("SPCHSTR003"):langcode("SPCHSTR001"));
                SayText(station_id);
            }
#endif  // HAVE_FESTIVAL
        } // end found_pos

    }   // valid data into database

    return(ok);
}   // End of data_add() function





// Code to compute SmartBeaconing(tm) rates.
//
// SmartBeaconing(tm) was invented by Steve Bragg (KA9MVA) and Tony Arnerich
// (KD7TA).  Its main goal is to change the beacon rate based on speed
// and cornering.  It does speed-variant corner pegging and
// speed-variant posit rate.

// Some tweaks have been added to the generic SmartBeaconing(tm) algorithm,
// but are current labeled as experimental and commented out:  1) We do
// a posit as soon as we first cross below the sb_low_speed_limit, and
// 2) We do a posit as soon as we cross above the sb_low_speed_limit if
// we haven't done a posit for sb_turn_time seconds.  These tweaks are
// intended to help show that the mobile station has stopped (so that
// dead-reckoning doesn't keep it moving across the map on other
// people's displays) and to more quickly show that the station is
// moving again (for the case where they're in stop-and-go traffic
// perhaps).
//
// It's possible that these new tweaks won't work well for the case
// where a station is traveling near the speed of sb_low_speed_limit.
// In this case they'll generate a posit each time they go below it and
// every time they go above it if they haven't done a posit in
// sb_turn_time seconds.  This could result in a lot of posits very
// quickly.  We may need to add yet another limit just above the
// sb_low_speed_limit for hysteresis, and not posit until we cross above
// that new limit.
//
// Several special SmartBeaconing(tm) parameters come into play here:
//
// sb_turn_min          Minimum degrees at which corner pegging will
//                      occur.  The next parameter affects this for
//                      lower speeds.
//
// sb_turn_slope        Fudget factor for making turns less sensitive at
//                      lower speeds.  No real units on this one.
//                      It ends up being non-linear over the speed
//                      range the way the original SmartBeaconing(tm)
//                      algorithm works.
//
// sb_turn_time         Dead-time before/after a corner peg beacon.
//                      Units are in seconds.
//
// sb_posit_fast        Fast posit rate, used if >= sb_high_speed_limit.
//                      Units are in seconds.
//
// sb_posit_slow        Slow posit rate, used if <= sb_low_speed_limit.
//                      Units are in minutes.
//
// sb_low_speed_limit   Low speed limit, units are in Mph.
//
// sb_high_speed_limit  High speed limit, units are in Mph.
//
//
//  Input: Course in degrees
//         Speed in knots
//
// Output: May force beacons by setting posit_next_time to various
//         values.
//
// Modify: sb_POSIT_rate
//         sb_current_heading
//         sb_last_heading
//         posit_next_time
//
//
// With the defaults compiled into the code, here are the
// turn_thresholds for a few speeds:
//
// Example: sb_turn_min = 20
//          sb_turn_slope = 25
//          sb_high_speed_limit = 60
//
//      > 60mph  20 degrees
//        50mph  25 degrees
//        40mph  26 degrees
//        30mph  28 degrees
//        20mph  33 degrees
//        10mph  45 degrees
//        3mph 103 degrees (we limit it to 80 now)
//        2mph 145 degrees (we limit it to 80 now)
//
// I added a max threshold of 80 degrees into the code.  145 degrees
// is unreasonable to expect except for perhaps switchback or 'U'
// turns.
//
// It'd probably be better to do a linear interpolation of
// turn_threshold based on min/max speed and min/max turns.  That's
// not how the SmartBeaconing(tm) algorithm coders implemented it in
// the HamHud though.
//
void compute_smart_beacon(char *current_course, char *current_speed) {
    int course;
    int speed;
    int turn_threshold;
    time_t secs_since_beacon;
    int heading_change_since_beacon;
    int beacon_now = 0;


    // Don't compute SmartBeaconing(tm) parameters or force any beacons
    // if we're not in that mode!
    if (!smart_beaconing)
        return;

    // Convert from knots to mph/kph (whichever is selected)
    speed = (int)(atof(current_speed) * cvt_kn2len + 0.5); // Poor man's rounding

    course = atoi(current_course);
 
    secs_since_beacon = sec_now() - posit_last_time;
 
    // Check for the low speed threshold, set to slow posit rate if
    // we're going slow.
    if (speed <= sb_low_speed_limit) {
        //fprintf(stderr,"Slow speed\n");


// EXPERIMENTAL!!!
////////////////////////////////////////////////////////////////////
        // Check to see if we're just crossing the threshold, if so,
        // beacon.  This keeps dead-reckoning working properly on
        // other people's displays.  Be careful for speeds near this
        // threshold though.  We really need a slow-speed rate and a
        // stop rate, with some distance between them, in order to
        // have some hysteresis for these posits.
//        if (sb_POSIT_rate != (sb_posit_slow * 60) ) { // Previous rate was _not_ the slow rate
//            beacon_now++; // Force a posit right away
//            //fprintf(stderr,"Stopping, POSIT!\n");
//        }
////////////////////////////////////////////////////////////////////


        // Set to slow posit rate
        sb_POSIT_rate = sb_posit_slow * 60; // Convert to seconds
    }
    else {  // We're moving faster than the low speed limit


// EXPERIMENTAL!!!
////////////////////////////////////////////////////////////////////
        // Check to see if we're just starting to move.  Again, we
        // probably need yet-another-speed-limit here to provide
        // some hysteresis.
//        if ( (secs_since_beacon > sb_turn_time)    // Haven't beaconed for a bit
//                && (sb_POSIT_rate == (sb_posit_slow * 60) ) ) { // Last rate was the slow rate
//            beacon_now++; // Force a posit right away
//            //fprintf(stderr,"Starting to move, POSIT!\n");
//        }
////////////////////////////////////////////////////////////////////


        // Start with turn_min degrees as the threshold
        turn_threshold = sb_turn_min;

        // Adjust rate according to speed
        if (speed > sb_high_speed_limit) {  // We're above the high limit
            sb_POSIT_rate = sb_posit_fast;
            //fprintf(stderr,"Setting fast rate\n");
        }
        else {  // We're between the high/low limits.  Set a between rate
            sb_POSIT_rate = (sb_posit_fast * sb_high_speed_limit) / speed;
            //fprintf(stderr,"Setting medium rate\n");

            // Adjust turn threshold according to speed
            turn_threshold += (int)( (sb_turn_slope * 10) / speed);
        }

        // Force a maximum turn threshold of 80 degrees (still too
        // high?)
        if (turn_threshold > 80)
            turn_threshold = 80;
 
        // Check to see if we've written anything into
        // sb_last_heading variable yet.  If not, write the current
        // course into it.
        if (sb_last_heading == -1)
            sb_last_heading = course;

        // Corner-pegging.  Note that we don't corner-peg if we're
        // below the low-speed threshold.
        heading_change_since_beacon = abs(course - sb_last_heading);
        if (heading_change_since_beacon > 180)
            heading_change_since_beacon = 360 - heading_change_since_beacon;

        //fprintf(stderr,"course change:%d\n",heading_change_since_beacon);

        if ( (heading_change_since_beacon > turn_threshold)
                && (secs_since_beacon > sb_turn_time) ) {
            beacon_now++;   // Force a posit right away

            //fprintf(stderr,"Corner, POSIT!\tOld:%d\tNew:%d\tDifference:%d\tSpeed: %d\tTurn Threshold:%d\n",
            //    sb_last_heading,
            //    course,
            //    heading_change_since_beacon,
            //    speed,
            //    turn_threshold);
        }


// EXPERIMENTAL
////////////////////////////////////////////////////////////////////
        // If we haven't beaconed for a bit (3 * sb_turn_time?), and
        // just completed a turn, check to see if our heading has
        // stabilized yet.  If so, beacon the latest heading.  We'll
        // have to save another variable which says whether the last
        // beacon was caused by corner-pegging.  The net effect is
        // that we'll get an extra posit coming out of a turn that
        // specifies our correct course and probably a more accurate
        // speed until the next posit.  This should make
        // dead-reckoning work even better.
        if (0) {
        }
////////////////////////////////////////////////////////////////////


    }

    // Check to see whether we've sped up sufficiently for the
    // posit_next_time variable to be too far out.  If so, shorten
    // that interval to match the current speed.
    if ( (posit_next_time - sec_now()) > sb_POSIT_rate)
        posit_next_time = sec_now() + sb_POSIT_rate;


    if (beacon_now) {
        posit_next_time = 0;    // Force a posit right away
    }

    // Should we also check for a rate too fast for the current
    // speed?  Probably not.  It'll get modified at the next beacon
    // time, which will happen quickly.

    // Save course for use later.  It gets put into sb_last_heading
    // in UpdateTime() if a beacon occurs.  We then use it above to
    // determine the course deviation since the last time we
    // beaconed.
    sb_current_heading = course;
}





// Speed is in knots
void my_station_gps_change(char *pos_long, char *pos_lat, char *course, char *speed, /*@unused@*/ char speedu, char *alt, char *sats) {
    long pos_long_temp, pos_lat_temp;
    char temp_data[40];   // short term string storage
    char temp_lat[12];
    char temp_long[12];
    DataRow *p_station;
    DataRow *p_time;

    // Note that speed will be in knots 'cuz it was derived from a
    // GPRMC string without modification.

    // Recompute the SmartBeaconing(tm) parameters based on current/past
    // course & speed.  Sending the speed in knots.
    //fprintf(stderr,"Speed: %s\n",speed);
    compute_smart_beacon(course, speed);

    p_station = NULL;
    if (!search_station_name(&p_station,my_callsign,1)) {  // find my data in the database
        p_time = NULL;          // add to end of time sorted list
//fprintf(stderr,"my_station_gps_change()\n");
        p_station = add_new_station(p_station,p_time,my_callsign);
    }
    p_station->flag |= ST_ACTIVE;
    p_station->data_via = 'L';
    p_station->flag &= (~ST_3RD_PT);            // clear "third party" flag
    p_station->record_type = NORMAL_APRS;

    // Free any old path we might have
    if (p_station->node_path_ptr != NULL)
        free(p_station->node_path_ptr);
    // Malloc and store the new path
    p_station->node_path_ptr = (char *)malloc(strlen("local") + 1);
    CHECKMALLOC(p_station->node_path_ptr);

    substr(p_station->node_path_ptr,"local",strlen("local"));
 
    // Create a timestamp from the current time
    xastir_snprintf(p_station->packet_time,
        sizeof(p_station->packet_time),
        "%s",
        get_time(temp_data));
    // Create a timestamp from the current time
    xastir_snprintf(p_station->pos_time,
        sizeof(p_station->pos_time),
        "%s",
        get_time(temp_data));
    p_station->flag |= ST_MSGCAP;               // set "message capable" flag

    /* convert to long and weed out any odd data */
    pos_long_temp = convert_lon_s2l(pos_long);
    pos_lat_temp  = convert_lat_s2l(pos_lat);

    /* convert to back to clean string for config data */
    convert_lon_l2s(pos_long_temp, temp_data, sizeof(temp_data), CONVERT_HP_NORMAL);
    xastir_snprintf(temp_long, sizeof(temp_long), "%c%c%c%c%c.%c%c%c%c",temp_data[0],temp_data[1],temp_data[2], temp_data[4],temp_data[5],
            temp_data[7],temp_data[8], temp_data[9], temp_data[10]);
    convert_lat_l2s(pos_lat_temp, temp_data, sizeof(temp_data), CONVERT_HP_NORMAL);
    xastir_snprintf(temp_lat, sizeof(temp_lat), "%c%c%c%c.%c%c%c%c",temp_data[0],temp_data[1],temp_data[3],temp_data[4], temp_data[6],
            temp_data[7], temp_data[8],temp_data[9]);

    /* fill the data in */    // ???????????????
    xastir_snprintf(my_lat,
        sizeof(my_lat),
        "%s",
        temp_lat);
    xastir_snprintf(my_long,
        sizeof(my_long),
        "%s",
        temp_long);
    p_station->coord_lat = convert_lat_s2l(my_lat);
    p_station->coord_lon = convert_lon_s2l(my_long);

    if ((p_station->coord_lon != pos_long_temp) || (p_station->coord_lat != pos_lat_temp)) {
        /* check to see if enough to change pos on screen */
        if ((pos_long_temp>x_long_offset) && (pos_long_temp<(x_long_offset+(long)(screen_width *scale_x)))) {
            if ((pos_lat_temp>y_lat_offset) && (pos_lat_temp<(y_lat_offset+(long)(screen_height*scale_y)))) {
                if((labs((p_station->coord_lon+(scale_x/2))-pos_long_temp)/scale_x)>0
                        || (labs((p_station->coord_lat+(scale_y/2))-pos_lat_temp)/scale_y)>0) {
                    //redraw_on_new_data = 1;   // redraw next chance
                    //redraw_on_new_data = 2;     // better response?
                    if (debug_level & 256) {
                        fprintf(stderr,"Redraw on new gps data \n");
                    }
                    statusline(langcode("BBARSTA038"),0);
                }
                else if (debug_level & 256) {
                    fprintf(stderr,"New Position same pixel as old.\n");
                }
            }
            else if (debug_level & 256) {
                fprintf(stderr,"New Position is off edge of screen.\n");
            }
        }
        else if (debug_level & 256) {
            fprintf(stderr,"New position is off side of screen.\n");
        }
    }

    p_station->coord_lat = pos_lat_temp;    // DK7IN: we have it already !??
    p_station->coord_lon = pos_long_temp;

    my_last_altitude_time = sec_now();
    xastir_snprintf(p_station->speed,
        sizeof(p_station->speed),
        "%s",
        speed);
    // is speed always in knots, otherwise we need a conversion!
    xastir_snprintf(p_station->course,
        sizeof(p_station->course),
        "%s",
        course);
    xastir_snprintf(p_station->altitude,
        sizeof(p_station->altitude),
        "%s",
        alt);
    // altu;    unit should always be meters  ????

    if(debug_level & 256)
        fprintf(stderr,"GPS MY_LAT <%s> MY_LONG <%s> MY_ALT <%s>\n",
            my_lat, my_long, alt);

    /* get my last altitude meters to feet */
    my_last_altitude=(long)(atof(alt)*3.28084);

    /* get my last course in deg */
    my_last_course=atoi(course);

    /* get my last speed in knots */
    my_last_speed=(int)(atof(speed));
    xastir_snprintf(p_station->sats_visible,
        sizeof(p_station->sats_visible),
        "%s",
        sats);

    // Update "heard" time for our new position
    p_station->sec_heard = sec_now();

    //if (   p_station->coord_lon != last_lon
    //    || p_station->coord_lat != last_lat ) {
    // we don't store redundant points (may change this later ?)
    // There are often echoes delayed 15 minutes or so it looks ugly
    // on the trail, so I want to discard them This also discards
    // immediate echoes.  Duplicates back in time up to
    // TRAIL_ECHO_TIME minutes are discarded.
    //
    if (!is_trailpoint_echo(p_station)) {
        (void)store_trail_point(p_station,
                                p_station->coord_lon,
                                p_station->coord_lat,
                                p_station->sec_heard,
                                p_station->altitude,
                                p_station->speed,
                                p_station->course,
                                p_station->flag);
    }
    if (debug_level & 256) {
        fprintf(stderr,"Adding Solid Trail for %s\n",
        p_station->call_sign);
    }
    draw_trail(da,p_station,1);         // update trail
    display_station(da,p_station,1);    // update symbol

    if (track_station_on == 1)          // maybe we are tracking ourselves?
        track_station(da,tracking_station_call,p_station);

    // We parsed a good GPS string, so allow beaconing to proceed
    // normally for a while.
    my_position_valid = 3;
    //fprintf(stderr,"Valid GPS input: my_position_valid = 3\n");
 
    //redraw_on_new_data = 1;   // redraw next chance
    redraw_on_new_data = 2;     // Immediate update of symbols/tracks
}





void my_station_add(char *my_callsign, char my_group, char my_symbol, char *my_long, char *my_lat, char *my_phg, char *my_comment, char my_amb) {
    DataRow *p_station;
    DataRow *p_time;
    char temp_data[40];   // short term string storage
    char *strp;

    p_station = NULL;
    if (!search_station_name(&p_station,my_callsign,1)) {  // find call 
        p_time = NULL;          // add to end of time sorted list
//fprintf(stderr,"my_station_add()\n");
        p_station = add_new_station(p_station,p_time,my_callsign);
    }
    p_station->flag |= ST_ACTIVE;
    p_station->data_via = 'L';
    p_station->flag &= (~ST_3RD_PT);            // clear "third party" flag
    p_station->record_type = NORMAL_APRS;
 
    // Free any old path we might have
    if (p_station->node_path_ptr != NULL)
        free(p_station->node_path_ptr);
    // Malloc and store the new path
    p_station->node_path_ptr = (char *)malloc(strlen("local") + 1);
    CHECKMALLOC(p_station->node_path_ptr);

    substr(p_station->node_path_ptr,"local",strlen("local"));

    // Create a timestamp from the current time
    xastir_snprintf(p_station->packet_time,
        sizeof(p_station->packet_time),
        "%s",
        get_time(temp_data));
    // Create a timestamp from the current time
    xastir_snprintf(p_station->pos_time,
        sizeof(p_station->pos_time),
        "%s",
        get_time(temp_data));
    p_station->flag |= ST_MSGCAP;               // set "message capable" flag

    /* Symbol overlay */
    if(my_group != '/' && my_group != '\\') {
        // Found an overlay character.  Check it.
        if ( (my_group >= '0' && my_group <= '9')
                || (my_group >= 'A' && my_group <= 'Z') ) {
            // Overlay character is good
            p_station->aprs_symbol.aprs_type = '\\';
            p_station->aprs_symbol.special_overlay = my_group;
        }
        else {
            // Found a bad overlay character
            p_station->aprs_symbol.aprs_type = my_group;
            p_station->aprs_symbol.special_overlay = '\0';
        }
    } else {    // Normal symbol, no overlay
        p_station->aprs_symbol.aprs_type = my_group;
        p_station->aprs_symbol.special_overlay = '\0';
    }
    p_station->aprs_symbol.aprs_symbol = my_symbol;

    p_station->pos_amb = my_amb;
    xastir_snprintf(temp_data,
        sizeof(temp_data),
        "%s",
        my_lat);

//fprintf(stderr," my_lat:%s\n",temp_data);

    temp_data[9] = '\0';

    strp = &temp_data[20];
    xastir_snprintf(strp,
//        sizeof(strp),   // No good, as strp is a pointer
        (int)(sizeof(temp_data) / 2),
        "%s",
        my_long);
    strp[10] = '\0';

//fprintf(stderr,"my_long:%s\n",my_long);
//fprintf(stderr,"my_long:%s\n",strp);

    switch (my_amb) {
    case 1: // 1/10th minute
        temp_data[6] = strp[7] = '5';
        break;
    case 2: // 1 minute
        temp_data[5] = strp[6] = '5';
        temp_data[6] = '0';
        strp[7]      = '0';
        break;
    case 3: // 10 minutes
        temp_data[3] = strp[4] = '5';
        temp_data[5] = temp_data[6] = '0';
        strp[6]      = strp[7]      = '0';
        break;
    case 4: // 1 degree
        temp_data[2] = strp[3] = '3';
        temp_data[3] = temp_data[5] = temp_data[6] = '0';
        strp[4]      = strp[6]      = strp[7]      = '0';
        break;
    case 0:
    default:
        break;
    }
    p_station->coord_lat = convert_lat_s2l(temp_data);
    p_station->coord_lon = convert_lon_s2l(strp);

    if (position_on_extd_screen(p_station->coord_lat,p_station->coord_lon)) {
        p_station->flag |= (ST_INVIEW);   // set   "In View" flag
    } else {
        p_station->flag &= (~ST_INVIEW);  // clear "In View" flag
    }

    substr(p_station->power_gain,my_phg,7);

    add_comment(p_station,my_comment);

    my_last_course = 0;         // set my last course in deg to zero
    redo_list = (int)TRUE;      // update active station lists
}





// Write the text from the packet_data_string out to the dialog if
// the dialog exists.  The user can contract/expand the dialog and
// always have it filled with the most current data out of the
// string.
//
void display_packet_data(void) {

    if( (Display_data_dialog != NULL)
            && (redraw_on_new_packet_data==1)) {
        int pos;
        int last_char;


        // Find out the last character position in the dialog text
        // area.
        last_char = XmTextGetLastPosition(Display_data_text);

        pos=0;
        XtVaSetValues(Display_data_text,XmNcursorPosition,&pos,NULL);

        // Clear the dialog text area.
        // Known with some versions of Motif to have a memory leak:
        //XmTextSetString(Display_data_text,"");
        XmTextReplace(Display_data_text,
            (XmTextPosition) 0,
            last_char,
            packet_data_string);

        // Find out the last character position in the dialog text
        // area.
        last_char = XmTextGetLastPosition(Display_data_text);

        XmTextShowPosition(Display_data_text,last_char);
    }
    redraw_on_new_packet_data=0;
}





//
// Note that the length of "line" can be up to MAX_DEVICE_BUFFER,
// which is currently set to 4096.
//
// data_port == -1 for x_spider port, normal interface number
// otherwise.
//
void packet_data_add(char *from, char *line, int data_port) {
    int i;
    int offset;
    char prefix[3] = "";


    if (data_port == -1)    // x_spider port (server port)
        xastir_snprintf(prefix,sizeof(prefix),"sp");
    else
        xastir_snprintf(prefix,sizeof(prefix),"%2d",data_port);


    offset=0;
    if (line[0]==(char)3)
        offset=1;

    if ( (Display_packet_data_type==1 && strcmp(from,langcode("WPUPDPD005"))==0) ||
            (Display_packet_data_type==2 && strcmp(from,langcode("WPUPDPD006"))==0)
            || Display_packet_data_type==0) {

        redraw_on_new_packet_data=1;
        if (packet_data_display < MAX_PACKET_DATA_DISPLAY) {
            // Array is not filled yet.  Add the new text at the
            // next position in the array.
            xastir_snprintf(packet_data[packet_data_display],
                sizeof(packet_data[packet_data_display]),
                "%s:%s-> %s\n",
                prefix,
                from,
                line+offset);
            packet_data_display++;
        }
        else {
            // Move everything up one and add the new text at the
            // last position.
            packet_data_display = MAX_PACKET_DATA_DISPLAY;
            for ( i = 0; i < (MAX_PACKET_DATA_DISPLAY - 1); i++ ) {
                xastir_snprintf(packet_data[i],
                    sizeof(packet_data[i]),
                    "%s",
                    packet_data[i+1]);
            }

            xastir_snprintf(packet_data[MAX_PACKET_DATA_DISPLAY-1],
                sizeof(packet_data[MAX_PACKET_DATA_DISPLAY-1]),
                "%s:%s-> %s\n",
                prefix,
                from,
                line+offset);
        }

        // Write the current data in the array out to the
        // packet_data_string variable for use in the
        // display_packet_data() function above.
        packet_data_string[0] = '\0';   // Empty the string
        for ( i = 0; i <= packet_data_display; i++ ) {
            int string_end = strlen(packet_data_string);

            xastir_snprintf(&packet_data_string[string_end],
                sizeof(packet_data_string) - string_end,
                "%s",
                packet_data[i]);
        }
    }
}





/*
 *  Decode Mic-E encoded data
 */
int decode_Mic_E(char *call_sign,char *path,char *info,char from,int port,int third_party) {
    int  i;
    int  offset;
    unsigned char s_b1;
    unsigned char s_b2;
    unsigned char s_b3;
    unsigned char s_b4;
    unsigned char s_b5;
    unsigned char s_b6;
    unsigned char s_b7;
    int  north,west,long_offset;
    int  d,m,h;
    char temp[MAX_TNC_LINE_SIZE+1];     // Note: Must be big in case we get long concatenated packets
    char new_info[MAX_TNC_LINE_SIZE+1]; // Note: Must be big in case we get long concatenated packets
    int  course;
    int  speed;
    int  msg1,msg2,msg3,msg;
    int  info_size;
    long alt;
    int  msgtyp;
    char rig_type[10];
    int ok;
        
    // MIC-E Data Format   [APRS Reference, chapter 10]

    // todo:  error check
    //        drop wrong positions from receive errors...
    //        drop 0N/0E position (p.25)
    
    /* First 7 bytes of info[] contains the APRS data type ID,    */
    /* longitude, speed, course.                    */
    /* The 6-byte destination field of path[] contains latitude,    */
    /* N/S bit, E/W bit, longitude offset, message code.        */
    /*

    MIC-E Destination Field Format:
    -------------------------------
    Ar1DDDD0 Br1DDDD0 Cr1MMMM0 Nr1MMMM0 Lr1HHHH0 Wr1HHHH0 CrrSSID0
    D = Latitude Degrees.
    M = Latitude Minutes.
    H = Latitude Hundredths of Minutes.
    ABC = Message bits, complemented.
    N = N/S latitude bit (N=1).
    W = E/W longitude bit (W=1).
    L = 100's of longitude degrees (L=1 means add 100 degrees to longitude
    in the Info field).
    C = Command/Response flag (see AX.25 specification).
    r = reserved for future use (currently 0).
    
    */
    /****************************************************************************
    * I still don't handle:                                                     *
    *    Custom message bits                                                    *
    *    SSID special routing                                                   *
    *    Beta versions of the MIC-E (which use a slightly different format).    *
    *                                                                           *
    * DK7IN : lat/long with custom msg works, altitude/course/speed works       *
    *****************************************************************************/

    if (debug_level & 1)
        fprintf(stderr,"decode_Mic_E:  FOUND MIC-E\n");

    // Note that the first MIC-E character was not passed to us, so we're
    // starting just past it.
    // Check for valid symbol table character.  Should be '/' or '\' only.
    if (info[7] != '/' && info[7] != '\\') {        // Symbol table char not in correct spot
        if (info[6] == '/' || info[6] == '\\') {    // Found it back one char in string
            // Don't print out the full info string here because it
            // can contain unprintable characters.  In fact, we
            // should check the chars we do print out to make sure
            // they're printable, else print a space char.
            if (debug_level & 1) {
                fprintf(stderr,"decode_Mic_E: Symbol table (%c), symbol (%c) swapped or corrupted packet?  Call=%s, Path=%s\n",
                    ((info[7] > 0x1f) && (info[7] < 0x7f)) ? info[7] : ' ',
                    ((info[6] > 0x1f) && (info[6] < 0x7f)) ? info[6] : ' ',
                    call_sign,
                    path);
                fprintf(stderr,"Returned from data_add, invalid symbol table character: %c\n",info[7]);
            }
        }
        return(1);  // No good, not MIC-E format or corrupted packet.  Return 1
                    // so that it won't get added to the database at all.
    }

    // Check for valid symbol.  Should be between '!' and '~' only.
    if (info[6] < '!' || info[6] > '~') {
        if (debug_level & 1)
            fprintf(stderr,"Returned from data_add, invalid symbol\n");

        return(1);  // No good, not MIC-E format or corrupted packet.  Return 1
                    // so that it won't get added to the database at all.
    }

    // Check for minimum MIC-E size.
    if (strlen(info) < 8) {
        if (debug_level & 1)
            fprintf(stderr,"Returned from data_add, packet too short\n");

        return(1);  // No good, not MIC-E format or corrupted packet.  Return 1
                    // so that it won't get added to the database at all.
    }

    //fprintf(stderr,"Path1:%s\n",path);

    msg1 = (int)( ((unsigned char)path[0] & 0x40) >>4 );
    msg2 = (int)( ((unsigned char)path[1] & 0x40) >>5 );
    msg3 = (int)( ((unsigned char)path[2] & 0x40) >>6 );
    msg = msg1 | msg2 | msg3;   // We now have the complemented message number in one variable
    msg = msg ^ 0x07;           // And this is now the normal message number
    msgtyp = 0;                 // DK7IN: Std message, I have to add custom msg decoding

    //fprintf(stderr,"Msg: %d\n",msg);

    /* Snag the latitude from the destination field, Assume TAPR-2 */
    /* DK7IN: latitude now works with custom message */
    s_b1 = (unsigned char)( (path[0] & 0x0f) + (char)0x2f );
    //fprintf(stderr,"path0:%c\ts_b1:%c\n",path[0],s_b1);
    if (path[0] & 0x10)     // A-J
        s_b1 += (unsigned char)1;

    if (s_b1 > (unsigned char)0x39)        // K,L,Z
        s_b1 = (unsigned char)0x20;
    //fprintf(stderr,"s_b1:%c\n",s_b1);
 
    s_b2 = (unsigned char)( (path[1] & 0x0f) + (char)0x2f );
    //fprintf(stderr,"path1:%c\ts_b2:%c\n",path[1],s_b2);
    if (path[1] & 0x10)     // A-J
        s_b2 += (unsigned char)1;

    if (s_b2 > (unsigned char)0x39)        // K,L,Z
        s_b2 = (unsigned char)0x20;
    //fprintf(stderr,"s_b2:%c\n",s_b2);
 
    s_b3 = (unsigned char)( (path[2] & (char)0x0f) + (char)0x2f );
    //fprintf(stderr,"path2:%c\ts_b3:%c\n",path[2],s_b3);
    if (path[2] & 0x10)     // A-J
        s_b3 += (unsigned char)1;

    if (s_b3 > (unsigned char)0x39)        // K,L,Z
        s_b3 = (unsigned char)0x20;
    //fprintf(stderr,"s_b3:%c\n",s_b3);
 
    s_b4 = (unsigned char)( (path[3] & 0x0f) + (char)0x30 );
    //fprintf(stderr,"path3:%c\ts_b4:%c\n",path[3],s_b4);
    if (s_b4 > (unsigned char)0x39)        // L,Z
        s_b4 = (unsigned char)0x20;
    //fprintf(stderr,"s_b4:%c\n",s_b4);
 
    s_b5 = (unsigned char)( (path[4] & 0x0f) + (char)0x30 );
    //fprintf(stderr,"path4:%c\ts_b5:%c\n",path[4],s_b5);
    if (s_b5 > (unsigned char)0x39)        // L,Z
        s_b5 = (unsigned char)0x20;
    //fprintf(stderr,"s_b5:%c\n",s_b5);
 
    s_b6 = (unsigned char)( (path[5] & 0x0f) + (char)0x30 );
    //fprintf(stderr,"path5:%c\ts_b6:%c\n",path[5],s_b6);
    if (s_b6 > (unsigned char)0x39)        // L,Z
        s_b6 = (unsigned char)0x20;
    //fprintf(stderr,"s_b6:%c\n",s_b6);
 
    s_b7 =  (unsigned char)path[6];        // SSID, not used here
    //fprintf(stderr,"path6:%c\ts_b7:%c\n",path[6],s_b7);
 
    //fprintf(stderr,"\n");

    // Special tests for 'L' due to position ambiguity deviances in
    // the APRS spec table.  'L' has the 0x40 bit set, but they
    // chose in the spec to have that represent position ambiguity
    // _without_ the North/West/Long Offset bit being set.  Yuk!
    // Please also note that the tapr.org Mic-E document (not the
    // APRS spec) has the state of the bit wrong in columns 2 and 3
    // of their table.  Reverse them.
    if (path[3] == 'L')
        north = 0;
    else 
        north = (int)((path[3] & 0x40) == (char)0x40);  // N/S Lat Indicator

    if (path[4] == 'L')
        long_offset = 0;
    else
        long_offset = (int)((path[4] & 0x40) == (char)0x40);  // Longitude Offset

    if (path[5] == 'L')
        west = 0;
    else
        west = (int)((path[5] & 0x40) == (char)0x40);  // W/E Long Indicator

    //fprintf(stderr,"north:%c->%d\tlat:%c->%d\twest:%c->%d\n",path[3],north,path[4],long_offset,path[5],west);

    /* Put the latitude string into the temp variable */
    xastir_snprintf(temp, sizeof(temp), "%c%c%c%c.%c%c%c%c",s_b1,s_b2,s_b3,s_b4,s_b5,s_b6,
            (north ? 'N': 'S'), info[7]);   // info[7] = symbol table

    /* Compute degrees longitude */
    xastir_snprintf(new_info,
        sizeof(new_info),
        "%s",
        temp);
    d = (int) info[0]-28;

    if (long_offset)
        d += 100;

    if ((180<=d)&&(d<=189))  // ??
        d -= 80;

    if ((190<=d)&&(d<=199))  // ??
        d -= 190;

    /* Compute minutes longitude */
    m = (int) info[1]-28;
    if (m>=60)
        m -= 60;

    /* Compute hundredths of minutes longitude */
    h = (int) info[2]-28;
    /* Add the longitude string into the temp variable */
    xastir_snprintf(temp, sizeof(temp), "%03d%02d.%02d%c%c",d,m,h,(west ? 'W': 'E'), info[6]);
    strncat(new_info,
        temp,
        sizeof(new_info) - strlen(new_info));

    /* Compute speed in knots */
    speed = (int)( ( info[3] - (char)28 ) * (char)10 );
    speed += ( (int)( (info[4] - (char)28) / (char)10) );
    if (speed >= 800)
        speed -= 800;       // in knots

    /* Compute course */
    course = (int)( ( ( (info[4] - (char)28) % 10) * (char)100) + (info[5] - (char)28) );
    if (course >= 400)
        course -= 400;

    /*  ???
        fprintf(stderr,"info[4]-28 mod 10 - 4 = %d\n",( ( (int)info[4]) - 28) % 10 - 4);
        fprintf(stderr,"info[5]-28 = %d\n", ( (int)info[5]) - 28 );
    */
    xastir_snprintf(temp, sizeof(temp), "%03d/%03d",course,speed);
    strncat(new_info,
        temp,
        sizeof(new_info) - strlen(new_info));
    offset = 8;   // start of rest of info

    /* search for rig type in Mic-E data */
    rig_type[0] = '\0';
    if (info[offset] != '\0' && (info[offset] == '>' || info[offset] == ']')) {
        /* detected type code:     > TH-D7    ] TM-D700 */
        if (info[offset] == '>')
            xastir_snprintf(rig_type,
                sizeof(rig_type),
                " TH-D7");
        else
            xastir_snprintf(rig_type,
                sizeof(rig_type),
                " TM-D700");

        offset++;
    }

    info_size = (int)strlen(info);
    /* search for compressed altitude in Mic-E data */  // {
    if (info_size >= offset+4 && info[offset+3] == '}') {  // {
        /* detected altitude  ___} */
        alt = ((((long)info[offset] - (long)33) * (long)91 +(long)info[offset+1] - (long)33) * (long)91
                    + (long)info[offset+2] - (long)33) - 10000;  // altitude in meters
        alt /= 0.3048;                                // altitude in feet, as in normal APRS

        //32808 is -10000 meters, or 10 km (deepest ocean), which is as low as a MIC-E
        //packet may go.  Upper limit is mostly a guess.
        if ( (alt > 500000) || (alt < -32809) ) {  // Altitude is whacko.  Skip it.
            if (debug_level & 1)
                fprintf(stderr,"decode_Mic_E:  Altitude is whacko:  %ld feet, skipping altitude...\n", alt);
            offset += 4;
        }
        else {  // Altitude is ok
            xastir_snprintf(temp, sizeof(temp), " /A=%06ld",alt);
            offset += 4;
            strncat(new_info,
                temp,
                sizeof(new_info) - strlen(new_info));
        }
    }

    /* start of comment */
    if (strlen(rig_type) > 0) {
        xastir_snprintf(temp, sizeof(temp), "%s",rig_type);
        strncat(new_info,
            temp,
            sizeof(new_info) - strlen(new_info));
    }

    strncat(new_info,
        " Mic-E ",
        sizeof(new_info) - strlen(new_info));
    if (msgtyp == 0) {
        switch (msg) {
            case 1:
                strncat(new_info,
                    "Enroute",
                    sizeof(new_info) - strlen(new_info));
                break;

            case 2:
                strncat(new_info,
                    "In Service",
                    sizeof(new_info) - strlen(new_info));
                break;

            case 3:
                strncat(new_info,
                    "Returning",
                    sizeof(new_info) - strlen(new_info));
                break;

            case 4:
                strncat(new_info,
                    "Committed",
                    sizeof(new_info) - strlen(new_info));
                break;

            case 5:
                strncat(new_info,
                    "Special",
                    sizeof(new_info) - strlen(new_info));
                break;

            case 6:
                strncat(new_info,
                    "Priority",
                    sizeof(new_info) - strlen(new_info));
                break;

            case 7:
                strncat(new_info,
                    "Emergency",
                    sizeof(new_info) - strlen(new_info));

                // Do a popup to alert the operator to this
                // condition.  Make sure we haven't popped up an
                // emergency message for this station within the
                // last 30 minutes.  If we pop these up constantly
                // it gets quite annoying.
                if ( (strncmp(call_sign, last_emergency_callsign, strlen(call_sign)) != 0)
                        || ((last_emergency_time + 60*30) < sec_now()) ) {

                    // Callsign is different or enough time has
                    // passed

                    last_emergency_time = sec_now();
                    xastir_snprintf(last_emergency_callsign,
                        sizeof(last_emergency_callsign),
                        "%s",
                        call_sign);

                    // Bring up the Find Station dialog so that the
                    // operator can go to the location quickly
                    xastir_snprintf(locate_station_call,
                        sizeof(locate_station_call),
                        "%s",
                        call_sign);

                    Locate_station( (Widget)NULL, (XtPointer)NULL, (XtPointer)1 );
                }
                break;

            default:
                strncat(new_info,
                    "Off Duty",
                    sizeof(new_info) - strlen(new_info));
        }
    } else {
        xastir_snprintf(temp, sizeof(temp), "Custom%d",msg);
        strncat(new_info,
            temp,
            sizeof(new_info) - strlen(new_info));
    }

    if (info[offset] != '\0') {
        /* Append the rest of the message to the expanded MIC-E message */
        for (i=offset; i<info_size; i++)
            temp[i-offset] = info[i];

        temp[info_size-offset] = '\0';
        strncat(new_info,
            " ",
            sizeof(new_info) - strlen(new_info));
        strncat(new_info,
            temp,
            sizeof(new_info) - strlen(new_info));
    }

    if (debug_level & 1) {
        fprintf(stderr,"decode_Mic_E:  Done decoding MIC-E\n");
        fprintf(stderr,"APRS_MICE, %s, %s, %s, %d, %d, NULL, %d\n",call_sign,path,new_info,from,port,third_party);
        // type:        APRS_MICE,
        // callsign:    N0EST-9,
        // path:        TTPQ9P,W0MXW-1,WIDE,N0QK-1*,
        // new_info:    4401.90N/09228.79W>278/007 /A=-05685 TM-D700 Mic-E Off Duty N0EST  ,
        // from:        70,
        // port:        -1,
        //              NULL,
        // third_party: 0
    }

    ok = data_add(APRS_MICE,call_sign,path,new_info,from,port,NULL,third_party);

    if (debug_level & 1)
        fprintf(stderr,"Returned from data_add, end of function\n");

    return(ok);
}   // End of decode_Mic_E()





/*
 *  Directed Station Query (query to my station)   [APRS Reference, chapter 15]
 */
int process_directed_query(char *call,char *path,char *message,char from) {
    DataRow *p_station;
    char from_call[MAX_CALLSIGN+1];
    char temp[100];
    int ok = 0;


    if (debug_level & 1)
        fprintf(stderr,"process_directed_query: %s\n",message);

    // Check for proper usage of the APRSD query
    if (!ok && strncmp(message,"APRSD",5) == 0 && from != 'F') {  // stations heard direct
        pad_callsign(from_call,call);
        xastir_snprintf(temp, sizeof(temp), ":%s:Directs=",from_call);
        p_station = n_first;
        while (p_station != NULL) {
            if ((p_station->flag & ST_ACTIVE) != 0) {       // ignore deleted objects
                if ( ((p_station->flag & ST_VIATNC) != 0)   // test "via TNC" flag
                     && ((p_station->flag & ST_DIRECT) != 0) // And "direct" flag
                     && sec_now() > (p_station->direct_heard + st_direct_timeout) // Within the last hour
                     && !is_my_call(p_station->call_sign,1) ) { // and not me
                    if (strlen(temp)+strlen(p_station->call_sign) < 65) {
                        strncat(temp,
                            " ",
                            sizeof(temp) - strlen(temp));
                        strncat(temp,
                            p_station->call_sign,
                            sizeof(temp) - strlen(temp));
                    } else {
                        transmit_message_data(call,temp,NULL);
                        xastir_snprintf(temp, sizeof(temp), 
                                        ":%s:Directs=",from_call);
                        strncat(temp,
                            " ",
                            sizeof(temp) - strlen(temp));
                        strncat(temp,
                            p_station->call_sign,
                            sizeof(temp) - strlen(temp));
                    }
                }
            }
            p_station = p_station->n_next;
        }

// WE7U:It'd be nice to return via the reverse path here
        transmit_message_data(call,temp,NULL);
        ok = 1;
    }
    // Check for illegal case for the APRSD query
    if (!ok && strncasecmp(message,"APRSD",5) == 0 && from != 'F') {  // stations heard direct
        fprintf(stderr,
            "%s just queried us with an illegal query: %s\n",
            call,
            message),
        fprintf(stderr,
            "Consider sending a message, asking them to follow the spec\n");
        ok = 1;
    }


// NOT IMPLEMENTED YET
    // Check for proper usage of the APRSH query
    if (!ok && strncmp(message,"APRSH",5)==0) {
        ok = 1;
    }
    // Check for illegal case for the APRSH query
    if (!ok && strncasecmp(message,"APRSH",5)==0) {
//        fprintf(stderr,
//            "%s just queried us with an illegal query: %s\n",
//            call,
//            message),
//        fprintf(stderr,
//            "Consider sending a message, asking them to follow the spec\n");
        ok = 1;
    }


// NOT IMPLEMENTED YET
    // Check for proper usage of the APRSM query
    if (!ok && strncmp(message,"APRSM",5)==0) {
        ok = 1;
    }
    // Check for illegal case for the APRSM query
    if (!ok && strncasecmp(message,"APRSM",5)==0) {
//        fprintf(stderr,
//            "%s just queried us with an illegal query: %s\n",
//            call,
//            message),
//        fprintf(stderr,
//            "Consider sending a message, asking them to follow the spec\n");
        ok = 1;
    }


// NOT IMPLEMENTED YET
    // Check for proper usage of the APRSO query
    if (!ok && strncmp(message,"APRSO",5)==0) {
        ok = 1;
    }
    // Check for illegal case for the APRSO query
    if (!ok && strncasecmp(message,"APRSO",5)==0) {
//        fprintf(stderr,
//            "%s just queried us with an illegal query: %s\n",
//            call,
//            message),
//        fprintf(stderr,
//            "Consider sending a message, asking them to follow the spec\n");
        ok = 1;
    }


    // Check for proper usage of the APRSP query
    if (!ok && strncmp(message,"APRSP",5) == 0 && from != 'F') {
        transmit_now = 1;       //send position
        ok = 1;
    }
    // Check for illegal case for the APRSP query
    if (!ok && strncasecmp(message,"APRSP",5) == 0 && from != 'F') {
        fprintf(stderr,
            "%s just queried us with an illegal query: %s\n",
            call,
            message),
        fprintf(stderr,
            "Consider sending a message, asking them to follow the spec\n");
        ok = 1;
    }


// NOT IMPLEMENTED YET
    // Check for proper usage of the APRSS query
    if (!ok && strncmp(message,"APRSS",5)==0) {
        ok = 1;
    }
    // Check for illegal case for the APRSS query
    if (!ok && strncasecmp(message,"APRSS",5)==0) {
//        fprintf(stderr,
//            "%s just queried us with an illegal query: %s\n",
//            call,
//            message),
//        fprintf(stderr,
//            "Consider sending a message, asking them to follow the spec\n");
        ok = 1;
    }


    // Check for proper usage of the APRST/PING? queries
    if (!ok && (strncmp(message,"APRST",5)==0
            ||  strncmp(message,"PING?",5)==0) && from != 'F') {
        pad_callsign(from_call,call);
        xastir_snprintf(temp, sizeof(temp), ":%s:PATH= %s>%s",from_call,call,path);    // correct format ?????

// WE7U:It'd be nice to return via the reverse path here
        transmit_message_data(call,temp,NULL);
        ok = 1;
    }
    // Check for illegal case for the APRST/PING? queries
    if (!ok && (strncasecmp(message,"APRST",5)==0
            ||  strncasecmp(message,"PING?",5)==0) && from != 'F') {
        fprintf(stderr,
            "%s just queried us with an illegal query: %s\n",
            call,
            message),
        fprintf(stderr,
            "Consider sending a message, asking them to follow the spec\n");
        ok = 1;
    }


    // Check for proper usage of the VER query (either case?)
    if (!ok && strncasecmp("VER",message,3) == 0 && from != 'F') { // not in Reference !???
        pad_callsign(from_call,call);
        xastir_snprintf(temp, sizeof(temp), ":%s:%s",from_call,VERSIONLABEL);

// WE7U:It'd be nice to return via the reverse path here
        transmit_message_data(call,temp,NULL);
        if (debug_level & 1)
            fprintf(stderr,"Sent to %s:%s\n",call,temp);
    }

    return(ok);
}





/*
 *  Station Capabilities, Queries and Responses      [APRS Reference, chapter 15]
 */
int process_query( /*@unused@*/ char *call_sign, /*@unused@*/ char *path,char *message,char from,int port, /*@unused@*/ int third_party) {
    char temp[100];
    int ok = 0;


// NOT IMPLEMENTED YET
    // Check for proper usage of the APRS? query
    if (!ok && strncmp(message,"APRS?",5)==0) {
        ok = 1;
    }
    // Check for illegal case for the APRS? query
    if (!ok && strncasecmp(message,"APRS?",5)==0) {
        ok = 1;
//        fprintf(stderr,
//            "%s just queried us with an illegal query: %s\n",
//            call_sign,
//            message),
//        fprintf(stderr,
//            "Consider sending a message, asking them to follow the spec\n");
    }


    // Check for proper usage of the IGATE? query
    if (!ok
            && strncmp(message,"IGATE?",6)==0
            && port != -1) {    // Not from a log file
        if (operate_as_an_igate && from != 'F') {
            xastir_snprintf(temp, sizeof(temp), "<IGATE,MSG_CNT=%d,LOC_CNT=%d",(int)igate_msgs_tx,stations_types(3));
            output_my_data(temp,port,0,0,0,NULL);    // Not igating
        }
        ok = 1;
    }
    // Check for illegal case for the IGATE? query
    if (!ok
            && strncasecmp(message,"IGATE?",6)==0
            && port != -1) {    // Not from a log file
        if (operate_as_an_igate && from != 'F') {
            fprintf(stderr,
                "%s just queried us with an illegal query: %s\n",
                call_sign,
                message),
            fprintf(stderr,
                "Consider sending a message, asking them to follow the spec\n");
        }
        ok = 1;
    }


// NOT IMPLEMENTED YET
    // Check for proper usage of the WX? query
    if (!ok && strncmp(message,"WX?",3)==0) {
        ok = 1;
    }
    // Check for illegal case for the WX? query
    if (!ok && strncasecmp(message,"WX?",3)==0) {
        ok = 1;
//        fprintf(stderr,
//            "%s just queried us with an illegal query: %s\n",
//            call_sign,
//            message),
//        fprintf(stderr,
//            "Consider sending a message, asking them to follow the spec\n");
    }

    return(ok);
}





/*
 *  Status Reports                              [APRS Reference, chapter 16]
 */
int process_status( /*@unused@*/ char *call_sign, /*@unused@*/ char *path, /*@unused@*/ char *message, /*@unused@*/ char from, /*@unused@*/ int port, /*@unused@*/ int third_party) {

//    popup_message(langcode("POPEM00018"),message);  // What is it ???
    return(1);
}





/*
 *  shorten_path
 *
 * What to do with this one?
 *      APW250,TCPIP*,ZZ2RMV-5*
 * We currently convert it to:
 *      APW250
 * It's a packet that went across the 'net, then to RF, then back to
 * the 'net.  We should probably drop it altogether?
 *
 *  Gets rid of unused digipeater fields (after the asterisk) and the
 *  TCPIP field if it exists.  Used for creating the third-party
 *  headers for igating purposes.  Note that for TRACEn-N and WIDEn-N
 *  digi's, it's impossible to tell via the '*' character whether that
 *  part of the path was used, but we can tell by the difference of
 *  'n' and 'N'.  If they're different, then that part of the path was
 *  used.  If it has counted down to just a TRACE or a WIDE (or TRACE7
 *  or WIDE5), then it should have a '*' after it like normal.
 */
void shorten_path( char *path, char *short_path, int short_path_size ) {
    int i,j,found_trace_wide,found_asterisk;
    char *ptr;


    if ( (path != NULL) && (strlen(path) >= 1) ) {

        xastir_snprintf(short_path,
            short_path_size,
            "%s",
            path);

        // Terminate the path at the end of the last used digipeater
        // This is trickier than it seems due to WIDEn-N and TRACEn-N
        // digipeaters.

        // Take a run through the entire path string looking for unused
        // TRACE/WIDE paths.
        for ( i = (strlen(path)-1); i >= 0; i-- ) { // Count backwards
            // If we find ",WIDE3-3" or ",TRACE7-7" (numbers match),
            // jam '\0' in at the comma.  These are unused digipeaters.
            if (   (strstr(&short_path[i],",WIDE7-7") != NULL)
                || (strstr(&short_path[i],",WIDE6-6") != NULL)
                || (strstr(&short_path[i],",WIDE5-5") != NULL)
                || (strstr(&short_path[i],",WIDE4-4") != NULL)
                || (strstr(&short_path[i],",WIDE3-3") != NULL)
                || (strstr(&short_path[i],",WIDE2-2") != NULL)
                || (strstr(&short_path[i],",WIDE1-1") != NULL)
                || (strstr(&short_path[i],",TRACE7-7") != NULL)
                || (strstr(&short_path[i],",TRACE6-6") != NULL)
                || (strstr(&short_path[i],",TRACE5-5") != NULL)
                || (strstr(&short_path[i],",TRACE4-4") != NULL)
                || (strstr(&short_path[i],",TRACE3-3") != NULL)
                || (strstr(&short_path[i],",TRACE2-2") != NULL)
                || (strstr(&short_path[i],",TRACE1-1") != NULL) ) {
                short_path[i] = '\0';
            }
        }


        // Take another run through short_string looking for used
        // TRACE/WIDE paths.  Also look for '*' characters and flag
        // if we see any.  If no '*' found, but a used TRACE/WIDE
        // path found, chop the path after the used TRACE/WIDE.  This
        // is to modify paths like this:
        //     APRS,PY1AYH-15*,RELAY,WIDE3-2,PY1EU-1
        // to this:
        //     APRS,PY1AYH-15*,RELAY,WIDE3-2
        j = 0;
        found_trace_wide = 0;
        found_asterisk = 0;
        for ( i = (strlen(short_path)-1); i >= 0; i-- ) { // Count backwards

            if (short_path[i] == '*')
                found_asterisk++;

            // Search for TRACEn/WIDEn.  If found (N!=n is guaranteed
            // by the previous loop) set the lower increment for the next
            // loop just past the last TRACEn/WIDEn found.  The used part
            // of the TRACEn/WIDEn will still remain in our shorter path.
            if (   (strstr(&short_path[i],"WIDE7") != NULL)
                || (strstr(&short_path[i],"WIDE6") != NULL)
                || (strstr(&short_path[i],"WIDE5") != NULL)
                || (strstr(&short_path[i],"WIDE4") != NULL)
                || (strstr(&short_path[i],"WIDE3") != NULL)
                || (strstr(&short_path[i],"WIDE2") != NULL)
                || (strstr(&short_path[i],"WIDE1") != NULL)
                || (strstr(&short_path[i],"TRACE7") != NULL)
                || (strstr(&short_path[i],"TRACE6") != NULL)
                || (strstr(&short_path[i],"TRACE5") != NULL)
                || (strstr(&short_path[i],"TRACE4") != NULL)
                || (strstr(&short_path[i],"TRACE3") != NULL)
                || (strstr(&short_path[i],"TRACE2") != NULL)
                || (strstr(&short_path[i],"TRACE1") != NULL) ) {
                j = i;
                found_trace_wide++;
                break;  // We only want to find the right-most one.
                        // We've found a used digipeater!
            }
        }


        // Chop off any unused digi's after a used TRACEn/WIDEn
        if (!found_asterisk && found_trace_wide) {
            for ( i = (strlen(short_path)-1); i >= j; i-- ) { // Count backwards
                if (short_path[i] == ',') {
                    short_path[i] = '\0';   // Terminate the string
                }
            }
        }


        // At this point, if we found a TRACEn or WIDEn, the "j"
        // variable will be non-zero.  If not then it'll be zero and
        // we'll run completely through the shorter path converting
        // '*' characters to '\0'.
        found_asterisk = 0;
        for ( i = (strlen(short_path)-1); i >= j; i-- ) { // Count backwards
            if (short_path[i] == '*') {
                short_path[i] = '\0';   // Terminate the string
                found_asterisk++;
            }
        }


        // Check for TCPIP or TCPXX as the last digipeater.  If present,
        // remove them.  TCPXX means that the packet came from an unregistered
        // user, and those packets will be rejected in igate.c before they're
        // sent to RF anyway.  igate.c will check for its presence in path,
        // not in short_path, so we're ok here to get rid of it in short_path.
        if (strlen(short_path) >= 5) {  // Get rid of "TCPIP" & "TCPXX"

            ptr = &short_path[strlen(short_path) - 5];
            if (   (strcasecmp(ptr,"TCPIP") == 0)
                || (strcasecmp(ptr,"TCPXX") == 0) ) {
                *ptr = '\0';
            }
            if ( (strlen(short_path) >= 1)  // Get rid of possible ending comma
                    && (short_path[strlen(short_path) - 1] == ',') ) {
                short_path[strlen(short_path) - 1] = '\0';
            }
        }


        // We might have a string with zero used digipeaters.  In this case
        // we will have no '*' characters and no WIDEn-N/TRACEn-N digis.
        // Get rid of everything except the destination call.  These packets
        // must have been heard directly by an igate station.
        if (!found_trace_wide && !found_asterisk) {
            for ( i = (strlen(short_path)-1); i >= j; i-- ) { // Count backwards
                if (short_path[i] == ',') {
                    short_path[i] = '\0';   // Terminate the string
                }
            }
        }


        // The final step:  Remove any asterisks in the path.
        // We'll insert our own on the way out to RF again.
        for ( i = 0; i < (int)(strlen(short_path) - 1); i++ ) {
            if (short_path[i] == '*') {
                for (j = i; j <= (int)(strlen(short_path) - 1); j++ ) {
                  short_path[j] = short_path[j+1];  // Shift left by one char
                }
            }
        }
 
    
    }
    else {
        short_path[0] = '\0';   // We were passed an empty string or a NULL.
    }

    if (debug_level & 1) {
        fprintf(stderr,"%s\n",path);
        fprintf(stderr,"%s\n\n",short_path);
    }
}





/*
 *  Mesages, Bulletins and Announcements         [APRS Reference, chapter 14]
 */
int decode_message(char *call,char *path,char *message,char from,int port,int third_party) {
    char *temp_ptr;
    char ipacket_message[300];
    char from_call[MAX_CALLSIGN+1];
    char ack[20];
    int ok, len;
    char addr[9+1];
    char addr9[9+1];
    char msg_id[5+1];
    char orig_msg_id[5+1];
    char ack_string[6];
    int done;


    // :xxxxxxxxx:____0-67____             message              printable, except '|', '~', '{'
    // :BLNn     :____0-67____             general bulletin     printable, except '|', '~'
    // :BLNnxxxxx:____0-67____           + Group Bulletin
    // :BLNX     :____0-67____             Announcement
    // :NWS-xxxxx:____0-67____             NWS Service Bulletin
    // :NWS_xxxxx:____0-67____             NWS Service Bulletin
    // :xxxxxxxxx:ackn1-5n               + ack
    // :xxxxxxxxx:rejn1-5n               + rej
    // :xxxxxxxxx:____0-67____{n1-5n     + message
    // :NTS....
    //  01234567890123456
    // 01234567890123456    old
    // we get message with already extracted data ID

    if (debug_level & 1)
        fprintf(stderr,"decode_message: start\n");

    if (debug_level & 1) {
        if ( (message != NULL) && (strlen(message) > (MAX_MESSAGE_LENGTH + 10) ) ) { // Overly long message.  Throw it away.  We're done.
            fprintf(stderr,"decode_message: LONG message.  Dumping it.\n");
            return(0);
        }
    }

    ack_string[0] = '\0';   // Clear out the Reply/Ack result string

    len = (int)strlen(message);
    ok = (int)(len > 9 && message[9] == ':');
    if (ok) {
        substr(addr9,message,9);                // extract addressee
        xastir_snprintf(addr,
            sizeof(addr),
            "%s",
            addr9);
        (void)remove_trailing_spaces(addr);
        message = message + 10;                 // pointer to message text

        temp_ptr = strrchr(message,'{');         // look for message ID after
                                                 //*last* { in message.
        msg_id[0] = '\0';
        if (temp_ptr != NULL) {
            substr(msg_id,temp_ptr+1,5);        // extract message ID, could be non-digit
            temp_ptr[0] = '\0';                 // adjust message end (chops off message ID)
        }

        // Save the original msg_id away.
        xastir_snprintf(orig_msg_id,
            sizeof(orig_msg_id),
            "%s",
            msg_id);

        // Check for Reply/Ack protocol in msg_id, which looks like
        // this:  "{XX}BB", where XX is the sequence number for the
        // message, and BB is the ack for the previous message from
        // my station.  I've also seen this from APRS+: "{XX}B", so
        // perhaps this is also possible "{X}B" or "{X}BB}".  We can
        // also get auto-reply responses from APRS+ that just have
        // "}X" or "}XX" at the end.  We decode those as well.
        //
        temp_ptr = strstr(msg_id,"}"); // look for Reply Ack in msg_id

        if (temp_ptr != NULL) { // Found Reply/Ack protocol!
            int zz = 1;
            int yy = 0;

            if ( (debug_level & 1) && (is_my_call(addr,1)) ) {
                fprintf(stderr,"Found Reply/Ack:%s\n",message);
                fprintf(stderr,"Orig_msg_id:%s\t",msg_id);
            }

// Put this code into the UI message area as well (if applicable).

            // Separate out the extra ack so that we can deal with
            // it properly.
            while (temp_ptr[zz] != '\0') {
                ack_string[yy++] = temp_ptr[zz++];
            }
            ack_string[yy] = '\0';  // Terminate the string

            // Terminate it here so that rest of decode works
            // properly.  We can get duplicate messages
            // otherwise.
            temp_ptr[0] = '\0'; // adjust msg_id end

            if ( (debug_level & 1) && (is_my_call(addr,1)) ) {
                fprintf(stderr,"New_msg_id:%s\tReply_ack:%s\n\n",msg_id,ack_string);
            }

        }
        else {  // Look for Reply Ack in message without sequence
                // number
            temp_ptr = strstr(message,"}");

            if (temp_ptr != NULL) {
                int zz = 1;
                int yy = 0;

                if ( (debug_level & 1) && (is_my_call(addr,1)) ) {
                    fprintf(stderr,"Found Reply/Ack:%s\n",message);
                }

// Put this code into the UI message area as well (if applicable).

                while (temp_ptr[zz] != '\0') {
                    ack_string[yy++] = temp_ptr[zz++];
                }
                ack_string[yy] = '\0';  // Terminate the string

                // Terminate it here so that rest of decode works
                // properly.  We can get duplicate messages
                // otherwise.
                temp_ptr[0] = '\0'; // adjust message end

                if ( (debug_level & 1) && (is_my_call(addr,1)) ) {
                    fprintf(stderr,"Reply_ack:%s\n\n",ack_string);
                }
            } 
        }
 
        done = 0;
    } else
        done = 1;                               // fall through...
    if (debug_level & 1)
        fprintf(stderr,"1\n");
    len = (int)strlen(message);
    //--------------------------------------------------------------------------
    if (!done && len > 3 && strncmp(message,"ack",3) == 0) {              // ACK
        substr(msg_id,message+3,5);
        // fprintf(stderr,"ACK: %s: |%s| |%s|\n",call,addr,msg_id);
        if (is_my_call(addr,1)) {
            clear_acked_message(call,addr,msg_id);  // got an ACK for me
            msg_record_ack(call,addr,msg_id,0,0);   // Record the ack for this message
        }
        else {  // ACK is for another station
            /* Now if I have Igate on and I allow to retransmit station data           */
            /* check if this message is to a person I have heard on my TNC within an X */
            /* time frame. If if is a station I heard and all the conditions are ok    */
            /* spit the ACK out on the TNC -FG                                         */
            if (operate_as_an_igate>1
                    && from==DATA_VIA_NET
                    && !is_my_call(call,1)
                    && port != -1) {    // Not from a log file
                char short_path[100];

                /*fprintf(stderr,"Igate check o:%d f:%c myc:%s cf:%s ct:%s\n",operate_as_an_igate,from,my_callsign,call,addr); { */
                shorten_path(path,short_path,sizeof(short_path));
                xastir_snprintf(ipacket_message,
                    sizeof(ipacket_message),
                    "}%s>%s,TCPIP,%s*::%s:%s",
                    call,
                    short_path,
                    my_callsign,
                    addr9,
                    message);

//fprintf(stderr,"Attempting to send ACK to RF\n");

                output_igate_rf(call,addr,path,ipacket_message,port,third_party);
                igate_msgs_tx++;
            }
        }
        done = 1;
    }
    if (debug_level & 1)
        fprintf(stderr,"2\n");
    //--------------------------------------------------------------------------
    if (!done && len > 3 && strncmp(message,"rej",3) == 0) {              // REJ
        substr(msg_id,message+3,5);
        // fprintf(stderr,"REJ: %s: |%s| |%s|\n",call,addr,msg_id);
        // we ignore it
        done = 1;
    }
    if (debug_level & 1)
        fprintf(stderr,"3\n");
    //--------------------------------------------------------------------------
    if (!done && strncmp(addr,"BLN",3) == 0) {                       // Bulletin
        // fprintf(stderr,"found BLN: |%s| |%s|\n",addr,message);
        bulletin_data_add(addr,call,message,"",MESSAGE_BULLETIN,from);
        done = 1;
    }
    if (debug_level & 1)
        fprintf(stderr,"4\n");

    //--------------------------------------------------------------------------
    if (!done && strlen(msg_id) > 0 && is_my_call(addr,1)) { // Message for me with msg_id
                                                             // (sequence number)
        time_t last_ack_sent;
        long record;

// Remember to put this code into the UI message area as well (if
// applicable).

        // Check for Reply/Ack
        if (strlen(ack_string) != 0) {  // Have a free-ride ack to deal with
            clear_acked_message(call,addr,ack_string);  // got an ACK for me
            msg_record_ack(call,addr,ack_string,0,0);   // Record the ack for this message
        }

        // Save the ack 'cuz we might need it while talking to this
        // station.  We need it to implement Reply/Ack protocol.
        store_most_recent_ack(call,msg_id);
 
        // fprintf(stderr,"found Msg w line to me: |%s| |%s|\n",message,msg_id);
        last_ack_sent = msg_data_add(addr,call,message,msg_id,MESSAGE_MESSAGE,from,&record); // id_fixed

        new_message_data += 1;
        (void)check_popup_window(call, 2);  // Calls update_messages()
        //update_messages(1); // Force an update
        if (sound_play_new_message)
            play_sound(sound_command,sound_new_message);
#ifdef HAVE_FESTIVAL
/* I re-use ipacket_message as my string buffer */
        if (festival_speak_new_message_alert) {
            xastir_snprintf(ipacket_message,
                sizeof(ipacket_message),
                "You have a new message from %s.",
                call);
            SayText(ipacket_message);
        }
        if (festival_speak_new_message_body) {
            xastir_snprintf(ipacket_message,
                sizeof(ipacket_message),
                " %s",
                message);
            SayText(ipacket_message);
        }

#endif  // HAVE_FESTIVAL

        // Only send an ack out once per 30 seconds
        if ( from != 'F'  // Not from a log file
                && (last_ack_sent + 30 ) < sec_now()
                && !satellite_ack_mode // Disable separate ack's for satellite work
                && port != -1 ) {   // Not from a log file

            //fprintf(stderr,"Sending ack: %ld %ld %ld\n",last_ack_sent,sec_now(),record);

            // Update the last_ack_sent field for the message
            msg_update_ack_stamp(record);

            pad_callsign(from_call,call);         /* ack the message */

            // In this case we want to send orig_msg_id back, not
            // the (possibly) truncated msg_id.  This is per Bob B's
            // Reply/Ack spec, sent to xastir-dev on Nov 14, 2001.
            xastir_snprintf(ack, sizeof(ack), ":%s:ack%s",from_call,orig_msg_id);

//WE7U Need to figure out the reverse path for this one instead of
//passing a NULL for the path:
            transmit_message_data(call,ack,NULL);
            if (auto_reply == 1) {

                xastir_snprintf(ipacket_message,
                    sizeof(ipacket_message), "AA:%s", auto_reply_message);

                if (debug_level & 2)
                    fprintf(stderr,"Send autoreply to <%s> from <%s> :%s\n",
                        call, my_callsign, ipacket_message);

                if (!is_my_call(call,1))
                    output_message(my_callsign, call, ipacket_message, "");
            }
        }

else {
//fprintf(stderr,"Skipping ack: %ld %ld\n",last_ack_sent,sec_now());
}

        done = 1;
    }
    if (debug_level & 1)
        fprintf(stderr,"5\n");
    //--------------------------------------------------------------------------
    if (!done
            && ( (strncmp(addr,"NWS-",4) == 0)          // NWS weather alert
              || (strncmp(addr,"NWS_",4) == 0) ) ) {    // NWS weather alert compressed

        //fprintf(stderr,"found NWS: |%s| |%s| |%s|\n",addr,message,msg_id);      // could have sort of line number

        (void)alert_data_add(addr,call,message,msg_id,MESSAGE_NWS,from);

        done = 1;
        if (operate_as_an_igate>1
                && from==DATA_VIA_NET
                && !is_my_call(call,1)
                && port != -1) { // Not from a log file
            char short_path[100];

            shorten_path(path,short_path,sizeof(short_path));
            xastir_snprintf(ipacket_message,
                sizeof(ipacket_message),
                "}%s>%s,TCPIP,%s*::%s:%s",
                call,
                short_path,
                my_callsign,
                addr9,
                message);
            output_nws_igate_rf(call,path,ipacket_message,port,third_party);
        }
    }
    if (debug_level & 1)
        fprintf(stderr,"6a\n");
    //--------------------------------------------------------------------------
    if (!done && strncmp(addr,"SKY",3) == 0) {  // NWS weather alert additional info

        //fprintf(stderr,"found SKY: |%s| |%s| |%s|\n",addr,message,msg_id);      // could have sort of line number

        (void)alert_data_add(addr,call,message,msg_id,MESSAGE_NWS,from);

        done = 1;
        if (operate_as_an_igate>1
                && from==DATA_VIA_NET
                && !is_my_call(call,1)
                && port != -1) { // Not from a log file
            char short_path[100];

            shorten_path(path,short_path,sizeof(short_path));
            xastir_snprintf(ipacket_message,
                sizeof(ipacket_message),
                "}%s>%s,TCPIP,%s*::%s:%s",
                call,
                short_path,
                my_callsign,
                addr9,
                message);
            output_nws_igate_rf(call,path,ipacket_message,port,third_party);
        }
    }
    if (debug_level & 1)
        fprintf(stderr,"6b\n");
    //--------------------------------------------------------------------------
    if (!done && strlen(msg_id) > 0) {          // other message with linenumber
        long dummy;

        if (debug_level & 2) fprintf(stderr,"found Msg w line: |%s| |%s| |%s|\n",addr,message,msg_id);
        (void)msg_data_add(addr,call,message,msg_id,MESSAGE_MESSAGE,from,&dummy);
        new_message_data += look_for_open_group_data(addr);
        if ((is_my_call(call,1) && check_popup_window(addr, 2) != -1)
                || check_popup_window(call, 0) != -1
                || check_popup_window(addr, 1) != -1) {
            //update_messages(1); // Force an update
        }

        /* Now if I have Igate on and I allow to retransmit station data           */
        /* check if this message is to a person I have heard on my TNC within an X */
        /* time frame. If if is a station I heard and all the conditions are ok    */
        /* spit the message out on the TNC -FG                                     */
        if (operate_as_an_igate>1
                && from==DATA_VIA_NET
                && !is_my_call(call,1)
                && !is_my_call(addr,1)
                && port != -1) {    // Not from a log file
            char short_path[100];

            /*fprintf(stderr,"Igate check o:%d f:%c myc:%s cf:%s ct:%s\n",operate_as_an_igate,from,my_callsign,
                        call,addr);*/     // {
            shorten_path(path,short_path,sizeof(short_path));
            xastir_snprintf(ipacket_message,
                sizeof(ipacket_message),
                "}%s>%s,TCPIP,%s*::%s:%s{%s",
                call,
                short_path,
                my_callsign,
                addr9,
                message,
                msg_id);

//fprintf(stderr,"Attempting to send message to RF\n");

            output_igate_rf(call,addr,path,ipacket_message,port,third_party);
            igate_msgs_tx++;
        }
        done = 1;
    }
    if (debug_level & 1)
        fprintf(stderr,"7\n");
    //--------------------------------------------------------------------------
    if (!done && len > 2
            && message[0] == '?'
            && port != -1   // Not from a log file
            && is_my_call(addr,1)) { // directed query
        // Smallest query known is "?WX".
        if (debug_level & 1)
            fprintf(stderr,"Received a directed query\n");
        done = process_directed_query(call,path,message+1,from);
    }
    if (debug_level & 1)
        fprintf(stderr,"8\n");
    //--------------------------------------------------------------------------
    // special treatment required?: Msg w/o line for me ?  Can also
    // be messages between other people (much more common).
    //--------------------------------------------------------------------------
    if (!done) {                                   // message without line number
        long dummy;

        if (debug_level & 4)
            fprintf(stderr,"found Msg: |%s| |%s|\n",addr,message);

        (void)msg_data_add(addr,call,message,"",MESSAGE_MESSAGE,from,&dummy);
        new_message_data++;      // ??????
        if (check_popup_window(addr, 1) != -1) {
            //update_messages(1); // Force an update
        }

        // Could be response to a query.  Popup a messsage.

// Check addr for my_call and !third_party, then check later in the
// packet for my_call if it is a third_party message?  Depends on
// what the packet looks like by this point.
        if ( (message[0] != '?') && is_my_call(addr,1) ) {
            popup_message(langcode("POPEM00018"),message);

            // Check for Reply/Ack.  APRS+ sends an AA: response back
            // for auto-reply, with an embedded free-ride Ack.
            if (strlen(ack_string) != 0) {  // Have an extra ack to deal with
                clear_acked_message(call,addr,ack_string);  // got an ACK for me
                msg_record_ack(call,addr,ack_string,0,0);   // Record the ack for this message
            }
        }
 
        // done = 1;
    }
    if (debug_level & 1)
        fprintf(stderr,"9\n");
    //--------------------------------------------------------------------------
    if (ok)
        (void)data_add(STATION_CALL_DATA,call,path,message,from,port,NULL,third_party);

    if (debug_level & 1)
        fprintf(stderr,"decode_message: finish\n");

    return(ok);
}





/*
 *  UI-View format messages, not relevant for APRS, format is not specified in APRS Reference
 *
 * This function is not currently called anywhere in the code.
 */
int decode_UI_message(char *call,char *path,char *message,char from,int port,int third_party) {
    char *temp_ptr;
    char from_call[MAX_CALLSIGN+1];
    char ack[20];
    char addr[9+1];
    int ok, len;
    char msg_id[5+1];
    int done;

    // I'm not sure, but I think they use 2 digit line numbers only
    // extract addr from path
    substr(addr,path,9);
    ok = (int)(strlen(addr) > 0);
    if (ok) {
        temp_ptr = strstr(addr,",");         // look for end of first call
        if (temp_ptr != NULL)
            temp_ptr[0] = '\0';                 // adjust addr end
        ok = (int)(strlen(addr) > 0);
    }
    len = (int)strlen(message);
    ok = (int)(len >= 2);
    if (ok) {
        temp_ptr = strstr(message,"~");         // look for message ID
        msg_id[0] = '\0';
        if (temp_ptr != NULL) {
            substr(msg_id,temp_ptr+1,2);        // extract message ID, could be non-digit
            temp_ptr[0] = '\0';                 // adjust message end
        }
        done = 0;
    } else
        done = 1;                               // fall through...
    len = (int)strlen(message);
    //--------------------------------------------------------------------------
    if (!done && msg_id[0] != '\0' && is_my_call(addr,1)) {      // message for me
        time_t last_ack_sent;
        long record;

        last_ack_sent = msg_data_add(addr,call,message,msg_id,MESSAGE_MESSAGE,from,&record);
        new_message_data += 1;
        (void)check_popup_window(call, 2);
        //update_messages(1); // Force an update
        if (sound_play_new_message)
            play_sound(sound_command,sound_new_message);

        // Only send an ack or autoresponse once per 30 seconds
        if ( (from != 'F')
                && ( (last_ack_sent + 30) < sec_now()) ) {

            //fprintf(stderr,"Sending ack: %ld %ld %ld\n",last_ack_sent,sec_now(),record);

            // Record the fact that we're sending an ack now
            msg_update_ack_stamp(record);

            pad_callsign(from_call,call);         /* ack the message */
            xastir_snprintf(ack, sizeof(ack), ":%s:ack%s",from_call,msg_id);

//WE7U:  Need to figure out the reverse path instead of using a null
//path here:
            transmit_message_data(call,ack,NULL);
            if (auto_reply == 1) {
                char temp[300];

                xastir_snprintf(temp, sizeof(temp), "AA:%s", auto_reply_message);

                if (debug_level & 2)
                    fprintf(stderr,"Send autoreply to <%s> from <%s> :%s\n",
                        call, my_callsign, temp);

                if (!is_my_call(call,1))
                    output_message(my_callsign, call, temp, "");
            }
        }
        done = 1;
    }
    //--------------------------------------------------------------------------
    if (!done && len == 2 && msg_id[0] == '\0') {                // ACK
        substr(msg_id,message,5);
        if (is_my_call(addr,1)) {
            clear_acked_message(call,addr,msg_id); // got an ACK for me
            msg_record_ack(call,addr,msg_id,0,0);  // Record the ack for this message
        }
//        else {                                          // ACK for other station
            /* Now if I have Igate on and I allow to retransmit station data           */
            /* check if this message is to a person I have heard on my TNC within an X */
            /* time frame. If if is a station I heard and all the conditions are ok    */
            /* spit the ACK out on the TNC -FG                                         */
//            if (operate_as_an_igate>1 && from==DATA_VIA_NET && !is_my_call(call,1)) {
//                char short_path[100];
                //fprintf(stderr,"Igate check o:%d f:%c myc:%s cf:%s ct:%s\n",operate_as_an_igate,from,my_callsign,call,addr); {

//                shorten_path(path,short_path,sizeof(short_path));
                //sprintf(ipacket_message,"}%s>%s:%s:%s",call,path,addr9,message);
//                sprintf(ipacket_message,"}%s>%s,TCPIP,%s*::%s:%s",call,short_path,my_callsign,addr9,message);
//                output_igate_rf(call,addr,path,ipacket_message,port,third_party);
//                igate_msgs_tx++;
//            }
//        }
        done = 1;
    }
    //--------------------------------------------------------------------------
    if (ok)
        (void)data_add(STATION_CALL_DATA,call,path,message,from,port,NULL,third_party);
    return(ok);
}





/*
 *  Decode APRS Information Field and dispatch it depending on the Data Type ID
 */
void decode_info_field(char *call, char *path, char *message, char *origin, char from, int port, int third_party) {
    char line[MAX_TNC_LINE_SIZE+1];
    char my_data[MAX_TNC_LINE_SIZE+1];
    int  ok_igate_net;
    int  done, ignore;
    char data_id;

    /* remember fixed format starts with ! and can be up to 24 chars in the message */ // ???
    if (debug_level & 1)
        fprintf(stderr,"decode_info_field: c:%s p:%s m:%s f:%c\n",call,path,message,from);
    if (debug_level & 1)
        fprintf(stderr,"decode_info_field: Past check\n");

    done         = 0;                                   // if 1, packet was decoded
    ignore       = 0;                                   // if 1, don't treat undecoded packets as status text
    ok_igate_net = 0;                                   // if 1, send packet to internet
    my_data[0]   = '\0';                                // not really needed...

    if ( (message != NULL) && (strlen(message) > MAX_TNC_LINE_SIZE) ) { // Overly long message, throw it away.
        if (debug_level & 1)
            fprintf(stderr,"decode_info_field: Overly long message.  Throwing it away.\n");
        done = 1;
    }
    else if (message == NULL || strlen(message) == 0) {      // we could have an empty message
        (void)data_add(STATION_CALL_DATA,call,path,NULL,from,port,origin,third_party);
        done = 1;                                       // don't report it to internet
    } else
        xastir_snprintf(my_data,
            sizeof(my_data),
            "%s",
            message);                        // copy message for later internet transmit

    // special treatment for objects
    if (!done && strlen(origin) > 0 && strncmp(origin,"INET",4)!=0) { // special treatment for object or item
        if (message[0] == '*') {                        // set object
            (void)data_add(APRS_OBJECT,call,path,message+1,from,port,origin,third_party);
            ok_igate_net = 1;                           // report it to internet
        } else if (message[0] == '!') {                 // set item
            (void)data_add(APRS_ITEM,call,path,message+1,from,port,origin,third_party);
            ok_igate_net = 1;                           // report it to internet
        } else
            if (message[0] == '_') {                    // delete object/item
                delete_object(call);                    // ?? does not vanish from map immediately !!???
                ok_igate_net = 1;                       // report it to internet
            }
        done = 1;
    }

    if (!done) {
        data_id = message[0];           // look at the APRS Data Type ID (first char in information field)
        message += 1;                   // extract data ID from information field
        ok_igate_net = 1;               // as default report packet to internet

        if (debug_level & 1) {
            if (ok_igate_net)
                fprintf(stderr,"decode_info_field: ok_igate_net can be read\n");
        }

        switch (data_id) {
            case '=':   // Position without timestamp (with APRS messaging)
                if (debug_level & 1)
                    fprintf(stderr,"decode_info_field: = (position w/o timestamp)\n");
                done = data_add(APRS_MSGCAP,call,path,message,from,port,origin,third_party);
                break;

            case '!':   // Position without timestamp (no APRS messaging) or Ultimeter 2000 WX
                if (debug_level & 1)
                    fprintf(stderr,"decode_info_field: ! (position w/o timestamp or Ultimeter 2000 WX)\n");
                if (message[0] == '!' && is_xnum_or_dash(message+1,40))   // Ultimeter 2000 WX
                    done = data_add(APRS_WX3,call,path,message+1,from,port,origin,third_party);
                else
                    done = data_add(APRS_FIXED,call,path,message,from,port,origin,third_party);
                break;

            case '/':   // Position with timestamp (no APRS messaging)
                if (debug_level & 1)
                    fprintf(stderr,"decode_info_field: / (position w/timestamp)\n");
                done = data_add(APRS_DOWN,call,path,message,from,port,origin,third_party);
                break;

            case '@':   // Position with timestamp (with APRS messaging)
                // DK7IN: could we need to test the message length first?
                if ((message[14] == 'N' || message[14] == 'S') &&
                    (message[24] == 'W' || message[24] == 'E')) {       // uncompressed format
                    if (debug_level & 1)
                        fprintf(stderr,"decode_info_field: @ (uncompressed position w/timestamp)\n");
                    if (message[29] == '/') {
                        if (message[33] == 'g' && message[37] == 't')
                            done = data_add(APRS_WX1,call,path,message,from,port,origin,third_party);
                        else
                            done = data_add(APRS_MOBILE,call,path,message,from,port,origin,third_party);
                    } else
                        done = data_add(APRS_DF,call,path,message,from,port,origin,third_party);
                } else {                                                // compressed format
                    if (debug_level & 1)
                        fprintf(stderr,"decode_info_field: @ (compressed position w/timestamp)\n");
                    if (message[16] >= '!' && message[16] <= 'z') {     // csT is speed/course
                        if (message[20] == 'g' && message[24] == 't')   // Wx data
                            done = data_add(APRS_WX1,call,path,message,from,port,origin,third_party);
                        else
                            done = data_add(APRS_MOBILE,call,path,message,from,port,origin,third_party);
                    } else
                        done = data_add(APRS_DF,call,path,message,from,port,origin,third_party);
                }
                break;

            case '[':   // Maidenhead grid locator beacon (obsolete- but used for meteor scatter)
              done = data_add(APRS_GRID,call,path,message,from,port,origin,third_party);
                break;
            case 0x27:  // Mic-E  Old GPS data (or current GPS data in Kenwood TM-D700)
            case 0x60:  // Mic-E  Current GPS data (but not used in Kennwood TM-D700)
            //case 0x1c:// Mic-E  Current GPS data (Rev. 0 beta units only)
            //case 0x1d:// Mic-E  Old GPS data (Rev. 0 beta units only)
                if (debug_level & 1)
                    fprintf(stderr,"decode_info_field: 0x27 or 0x60 (Mic-E)\n");
                done = decode_Mic_E(call,path,message,from,port,third_party);
                break;

            case '_':   // Positionless weather data                [APRS Reference, chapter 12]
                if (debug_level & 1)
                    fprintf(stderr,"decode_info_field: _ (positionless wx data)\n");
                done = data_add(APRS_WX2,call,path,message,from,port,origin,third_party);
                break;

            case '#':   // Peet Bros U-II Weather Station (mph)     [APRS Reference, chapter 12]
                if (debug_level & 1)
                    fprintf(stderr,"decode_info_field: # (peet bros u-II wx station)\n");
                if (is_xnum_or_dash(message,13))
                    done = data_add(APRS_WX4,call,path,message,from,port,origin,third_party);
                break;

            case '*':   // Peet Bros U-II Weather Station (km/h)
                if (debug_level & 1)
                    fprintf(stderr,"decode_info_field: * (peet bros u-II wx station)\n");
                if (is_xnum_or_dash(message,13))
                    done = data_add(APRS_WX6,call,path,message,from,port,origin,third_party);
                break;

            case '$':   // Raw GPS data or Ultimeter 2000
                if (debug_level & 1)
                    fprintf(stderr,"decode_info_field: $ (raw gps or ultimeter 2000)\n");
                if (strncmp("ULTW",message,4) == 0 && is_xnum_or_dash(message+4,44))
                    done = data_add(APRS_WX5,call,path,message+4,from,port,origin,third_party);
                else if (strncmp("GPGGA",message,5) == 0)
                    done = data_add(GPS_GGA,call,path,message,from,port,origin,third_party);
                else if (strncmp("GPRMC",message,5) == 0)
                    done = data_add(GPS_RMC,call,path,message,from,port,origin,third_party);
                else if (strncmp("GPGLL",message,5) == 0)
                    done = data_add(GPS_GLL,call,path,message,from,port,origin,third_party);
                else {
                        // handle VTG and WPT too  (APRS Ref p.25)
                }
                break;

            case ':':   // Message
                if (debug_level & 1)
                    fprintf(stderr,"decode_info_field: : (message)\n");
                done = decode_message(call,path,message,from,port,third_party);
                // there could be messages I should not retransmit to internet... ??? Queries to me...
                break;

            case '>':   // Status                                   [APRS Reference, chapter 16]
                if (debug_level & 1)
                    fprintf(stderr,"decode_info_field: > (status)\n");
                done = data_add(APRS_STATUS,call,path,message,from,port,origin,third_party);
                break;

            case '?':   // Query
                if (debug_level & 1)
                    fprintf(stderr,"decode_info_field: ? (query)\n");
                done = process_query(call,path,message,from,port,third_party);
                ignore = 1;     // don't treat undecoded packets as status text
                break;

            case '~':   // UI-format messages, not relevant for APRS ("Do not use" in Reference)
            case ',':   // Invalid data or test packets             [APRS Reference, chapter 19]
            case '<':   // Station capabilities                     [APRS Reference, chapter 15]
            case '{':   // User-defined APRS packet format     //}
            case '%':   // Agrelo DFJr / MicroFinder
            case '&':   // Reserved -- Map Feature
            case 'T':   // Telemetrie data                          [APRS Reference, chapter 13]
                if (debug_level & 1)
                    fprintf(stderr,"decode_info_field: ~,<{%%&T[\n");
                ignore = 1;     // don't treat undecoded packets as status text
                break;
        }

        if (debug_level & 1) {
            if (done)
                fprintf(stderr,"decode_info_field: done = 1\n");
            else
                fprintf(stderr,"decode_info_field: done = 0\n");
            if (ok_igate_net)
                fprintf(stderr,"decode_info_field: ok_igate_net can be read 2\n");
        }

        if (debug_level & 1)
            fprintf(stderr,"decode_info_field: done with big switch\n");

        if (!done && !ignore) {         // Other Packets        [APRS Reference, chapter 19]
            done = data_add(OTHER_DATA,call,path,message-1,from,port,origin,third_party);
            ok_igate_net = 0;           // don't put data on internet       ????
            if (debug_level & 1)
                fprintf(stderr,"decode_info_field: done with data_add(OTHER_DATA)\n");
        }

        if (!done) {                    // data that we do ignore...
            //fprintf(stderr,"decode_info_field: not decoding info: Call:%s ID:%c Msg:|%s|\n",call,data_id,message);
            ok_igate_net = 0;           // don't put data on internet
            if (debug_level & 1)
                fprintf(stderr,"decode_info_field: done with ignored data\n");
        }
    }

    if (third_party)
        ok_igate_net = 0;   // don't put third party traffic on internet

    if (is_my_call(call,1))
        ok_igate_net = 0;   // don't put my data on internet     ???

    if (ok_igate_net) {

        if (debug_level & 1)
            fprintf(stderr,"decode_info_field: ok_igate_net start\n");

        if ( (from == DATA_VIA_TNC) // Came in via a TNC
                && (strlen(my_data) > 0) ) { // Not empty

            // Here's where we inject our own callsign like this:
            // "WE7U-15*,I" in order to provide injection ID for our
            // igate.  We need to add a '*' character after our
            // callsign as we inject, to show it came through us.
            // On the way back out of the internet it can get a '*'
            // added after the 'I' perhaps.
            xastir_snprintf(line,
                sizeof(line),
                "%s>%s,%s*,I:%s",
                call,
                path,
                my_callsign,
                my_data);

//fprintf(stderr,"decode_info_field: IGATE>NET %s\n",line);
            output_igate_net(line, port, third_party);
        }
    }
    if (debug_level & 1)
        fprintf(stderr,"decode_info_field: done\n");
}





/*
 *  Extract object or item data from information field before processing
 *
 *  Returns 1 if valid object found, else returns 0.
 *
 */
int extract_object(char *call, char **info, char *origin) {
    int ok, i;

    // Object and Item Reports     [APRS Reference, chapter 11]
    ok = 0;
    // todo: add station originator to database
    if ((*info)[0] == ';') {                    // object
        // fixed 9 character object name with any printable ASCII character
        if (strlen((*info)) > 1+9) {
            substr(call,(*info)+1,9);           // extract object name
            (*info) = (*info) + 10;
            // Remove leading spaces ? They look bad, but are allowed by the APRS Reference ???
            (void)remove_trailing_spaces(call);
            if (valid_object(call)) {
                // info length is at least 1
                ok = 1;
            }
        }
    } else if ((*info)[0] == ')') {             // item
        // 3 - 9 character item name with any printable ASCII character
        if (strlen((*info)) > 1+3) {
            for (i = 1; i <= 9; i++) {
                if ((*info)[i] == '!' || (*info)[i] == '_') {
                    call[i-1] = '\0';
                    break;
                }
                call[i-1] = (*info)[i];
            }
            call[9] = '\0';  // In case we never saw '!' || '_'
            (*info) = &(*info)[i];
            // Remove leading spaces ? They look bad, but are allowed by the APRS Reference ???
            //(void)remove_trailing_spaces(call);   // This statement messed up our searching!!! Don't use it!
            if (valid_object(call)) {
                // info length is at least 1
                ok = 1;
            }
        }
    } else {
        fprintf(stderr,"Not an object, nor an item!!! call=%s, info=%s, origin=%s.\n",
               call, *info, origin);
    }
    return(ok);
}





/*
 *  Extract third-party traffic from information field before processing
 */
int extract_third_party(char *call,
                        char *path,
                        int path_size,
                        char **info,
                        char *origin,
                        int origin_size) {
    int ok;
    char *p_call;
    char *p_path;

    p_call = NULL;                              // to make the compiler happy...
    p_path = NULL;                              // to make the compiler happy...
    ok = 0;
    if (!is_my_call(call,1)) {
        // todo: add reporting station call to database ??
        //       but only if not identical to reported call
        (*info) = (*info) +1;                   // strip '}' character
        p_call = strtok((*info),">");           // extract call
        if (p_call != NULL) {
            p_path = strtok(NULL,":");          // extract path
            if (p_path != NULL) {
                (*info) = strtok(NULL,"");      // rest is information field
                if ((*info) != NULL)            // the above looks dangerous, but works on same string
                    if (strlen(p_path) < 100)
                        ok = 1;                 // we have found all three components
            }
        }
    }

    if ((debug_level & 1) && !ok)
        fprintf(stderr,"extract_third_party: invalid format from %s\n",call);

    if (ok) {

        xastir_snprintf(path,
            path_size,
            "%s",
            p_path);

        ok = valid_path(path);                  // check the path and convert it to TAPR format
        // Note that valid_path() also removes igate injection identifiers

        if ((debug_level & 1) && !ok) {
            char filtered_data[MAX_LINE_SIZE + 1];

            xastir_snprintf(filtered_data,
                sizeof(filtered_data),
                "%s",
                path);
            makePrintable(filtered_data);
            fprintf(stderr,"extract_third_party: invalid path: %s\n",filtered_data);
        }
    }

    if (ok) {                                         // check callsign
        (void)remove_trailing_asterisk(p_call);       // is an asterisk valid here ???
        if (valid_inet_name(p_call,(*info),origin,origin_size)) { // accept some of the names used in internet
            // treat is as object with special origin
            xastir_snprintf(call,
                MAX_CALLSIGN+1,
                "%s",
                p_call);
        } else if (valid_call(p_call)) {              // accept real AX.25 calls
            xastir_snprintf(call,
                MAX_CALLSIGN+1,
                "%s",
                p_call);
        } else {
            ok = 0;
            if (debug_level & 1) {
                char filtered_data[MAX_LINE_SIZE + 1];

                xastir_snprintf(filtered_data,
                    sizeof(filtered_data),
                    "%s",
                    p_call);
                makePrintable(filtered_data);
                fprintf(stderr,"extract_third_party: invalid call: %s\n",filtered_data);
            }
        }
    }
    return(ok);
}





/*
 *  Extract text inserted by TNC X-1J4 from start of info line
 */
void extract_TNC_text(char *info) {
    int i,j,len;

    if (strncasecmp(info,"thenet ",7) == 0) {   // 1st match
        len = strlen(info)-1;
        for (i=7;i<len;i++) {
            if (info[i] == ')')
                break;
        }
        len++;
        if (i>7 && info[i] == ')' && info[i+1] == ' ') {        // found
            i += 2;
            for (j=0;i<=len;i++,j++) {
                info[j] = info[i];
            }
        }
    }
}





//WE7U2
// We feed a raw 7-byte string into this routine.  It decodes the
// callsign-SSID and tells us whether there are more callsigns after
// this.  If the "asterisk" input parameter is nonzero it'll add an
// asterisk to the callsign if it has been digipeated.  This
// function is called by the decode_ax25_header() function.
//
// Inputs:  string          Raw input string
//          asterisk        1 = add "digipeated" asterisk
//
// Outputs: callsign        Processed string
//          returned int    1=more callsigns follow, 0=end of address field
//
int decode_ax25_address(char *string, char *callsign, int asterisk) {
    int i,j;
    char ssid;
    char t;
    int more = 0;
    int digipeated = 0;

    // Shift each of the six callsign characters right one bit to
    // convert to ASCII.  We also get rid of the extra spaces here.
    j = 0;
    for (i = 0; i < 6; i++) {
        t = ((unsigned char)string[i] >> 1) & 0x7f;
        if (t != ' ') {
            callsign[j++] = t;
        }
    }

    // Snag out the SSID byte to play with.  We need more than just
    // the 4 SSID bits out of it.
    ssid = (unsigned char)string[6];

    // Check the digipeat bit
    if ( (ssid & 0x80) && asterisk)
        digipeated++;   // Has been digipeated

    // Check whether it is the end of the address field
    if ( !(ssid & 0x01) )
        more++; // More callsigns to come after this one

    // Snag the four SSID bits
    ssid = (ssid >> 1) & 0x0f;

    // Construct the SSID number and add it to the end of the
    // callsign if non-zero.  If it's zero we don't add it.
    if (ssid) {
        callsign[j++] = '-';
        if (ssid > 9) {
            callsign[j++] = '1';
        }
        ssid = ssid % 10;
        callsign[j++] = '0' + ssid;
    }

    // Add an asterisk if the packet has been digipeated through
    // this callsign
    if (digipeated)
        callsign[j++] = '*';

    // Terminate the string
    callsign[j] = '\0';

    return(more);
}





// Function which receives raw AX.25 packets from a KISS interface and
// converts them to a printable TAPR-2 (more or less) style string.
// We receive the packet with a KISS Frame End character at the
// beginning and a "\0" character at the end.  We can end up with
// multiple asterisks, one for each callsign that the packet was
// digipeated through.  A few other TNC's put out this same sort of
// format.
//
//WE7U
// Some versions of KISS can encode the radio channel (for
// multi-port TNC's) in the command byte.  How do we know we're
// running those versions of KISS though?  Here are the KISS
// variants that I've been able to discover to date:
//
// KISS               No CRC, one radio port
//
// SMACK              16-bit CRC, multiport TNC's
//
// KISS-CRC
//
// 6-PACK
//
// Multi-Drop KISS    8-bit XOR Checksum, multiport TNC's -,
// G8BPQ KISS         8-bit XOR Checksum, multiport TNC's -|-- All the same!
// XKISS (Kantronics) 8-bit XOR Checksum, multiport TNC's -'
//
// MKISS              Linux driver which supports KISS/BPQ and
//                    hardware handshaking?  Also Paccomm command to
//                    immediately enter KISS mode.
//
// FlexKISS           -,
// FlexCRC            -|-- These are all the same!
// RMNC-KISS          -|
// CRC-RMNC           -'
//
//
// It appears that none of the above protocols implement any form of
// hardware flow control.
// 
// 
// Compare this function with interface.c:process_ax25_packet() to
// see if we're missing anything important.
//
//
// Inputs:  incoming_data       Raw string
//          length              Length of raw string
//
// Outputs: int                 0 if it is a bad packet,
//                              1 if it is good
//          incoming_data       Processed string
//
int decode_ax25_header(unsigned char *incoming_data, int length) {
    char temp[20];
    char result[MAX_LINE_SIZE+100];
    char dest[15];
    int i, ptr;
    char callsign[15];
    char more;
    char num_digis = 0;


    // Do we have a string at all?
    if (incoming_data == NULL)
        return(0);

    // Drop the packet if it is too long.  Note that for KISS packets
    // we can't use strlen() as there can be 0x00 bytes in the
    // data itself.
    if (length > 1024) {
        incoming_data[0] = '\0';
        return(0);
    }

    // Start with an empty string for the result
    result[0] = '\0';

    ptr = 0;

    // Process the destination address
    for (i = 0; i < 7; i++)
        temp[i] = incoming_data[ptr++];
    temp[7] = '\0';
    more = decode_ax25_address(temp, callsign, 0); // No asterisk
    xastir_snprintf(dest,sizeof(dest),"%s",callsign);

    // Process the source address
    for (i = 0; i < 7; i++)
        temp[i] = incoming_data[ptr++];
    temp[7] = '\0';
    more = decode_ax25_address(temp, callsign, 0); // No asterisk

    // Store the two callsigns we have into "result" in the correct
    // order
    xastir_snprintf(result,sizeof(result),"%s>%s",callsign,dest);

    // Process the digipeater addresses (if any)
    num_digis = 0;
    while (more && num_digis < 8) {
        for (i = 0; i < 7; i++)
            temp[i] = incoming_data[ptr++];
        temp[7] = '\0';

        more = decode_ax25_address(temp, callsign, 1); // Add asterisk
        strncat(result,
            ",",
            sizeof(result) - strlen(result));
        strncat(result,
            callsign,
            sizeof(result) - strlen(result));
        num_digis++;
    }

    strncat(result,
        ":",
        sizeof(result) - strlen(result));


    // Check the Control and PID bytes and toss packets that are
    // AX.25 connect/disconnect or information packets.  We only
    // want to process UI packets in Xastir.


    // Control byte should be 0x03 (UI Frame).  Strip the poll-bit
    // from the PID byte before doing the comparison.
    if ( (incoming_data[ptr++] & (~0x10)) != 0x03) {
        return(0);
    }


    // PID byte should be 0xf0 (normal AX.25 text)
    if (incoming_data[ptr++] != 0xf0)
        return(0);


// WE7U:  We get multiple concatenated KISS packets sometimes.  Look
// for that here and flag when it happens (so we know about it and
// can fix it someplace earlier in the process).  Correct the
// current packet so we don't get the extra garbage tacked onto the
// end.
    for (i = ptr; i < length; i++) {
        if (incoming_data[i] == KISS_FEND) {
            fprintf(stderr,"***Found concatenated KISS packets:***\n");
            incoming_data[i] = '\0';    // Truncate the string
            break;
        }
    }

    // Add the Info field to the decoded header info
    strncat(result,
        &incoming_data[ptr],
        sizeof(result) - strlen(result));

    // Copy the result onto the top of the input data.  Note that
    // the length can sometimes be longer than the input string, so
    // we can't just use the "length" variable here or we'll
    // truncate our string.
    //
    xastir_snprintf((char *)incoming_data,
        MAX_LINE_SIZE,
        "%s",
        result);

//fprintf(stderr,"%s\n",incoming_data);

    return(1);
}





// RELAY the packet back out onto RF if received on a port with
// digipeat enabled and the packet header has a non-digipeated RELAY
// or my_callsign entry.  This is for AX.25 kernel networking ports
// or Serial KISS TNC ports only:  Regular serial TNC's have these
// features enabled/disabled through the startup/shutdown files.
//
// Adding asterisks:
// Keep whatever digipeated fields have already been set.  If
// there's a "RELAY" or my_callsign entry that hasn't been
// digipeated yet, change it to my_callsign*.
//
// This might be much easier to code into the routine that first
// receives the packet (for Serial KISS TNC's).  There we'd have
// access to every digipeated bit directly instead of parsing
// asterisks out of a string.
//
// NOTE:  We don't handle this case properly:  Multiple RELAY's or
// multiple my_callsign's in the path, where one of the earlier
// matching callsigns has been digipeated, but a later one has not.
// We'll find the first matching callsign and the last digi, and we
// won't relay the packet.  This probably won't happen much in the
// real world.
//
// We could also do premptive digipeating here and skip over
// callsigns that haven't been digipeated yet.  Should we set the
// digipeated bits on everything before it?  Probably.  Either that
// or remove the callsigns ahead of it in the list that weren't
// digipeated.
//
void relay_digipeat(char *call, char *path, char *info, int port) {
    char new_path[110+1];
    char new_digi[MAX_CALLSIGN+2];  // Need extra for '*' storage
    int  ii;
    int  done;
    char destination[MAX_CALLSIGN+1];
#define MAX_RELAY_SUBSTRINGS 10
    char *Substring[MAX_RELAY_SUBSTRINGS];  // Pointers to substrings parsed by split_string()
    char temp_string[MAX_MESSAGE_LENGTH+1];

    // These strings are debugging tools
    char small_string[200];
    char big_string[2000];


    // Check whether relay_digipeat has been enabled for this interface.
    // If not, get out while the gettin's good.
    if (devices[port].relay_digipeat != 1) {
        return;
    }

    // Check whether transmit has been enabled for this interface.
    // If not, get out while the gettin's good.
    if (devices[port].transmit_data != 1) {
        return;
    }

    // Check for the only four types of interfaces where we might
    // want to do RELAY digipeating.  If not one of these, go
    // bye-bye.
    if (       (devices[port].device_type != DEVICE_SERIAL_KISS_TNC)
            && (devices[port].device_type != DEVICE_SERIAL_MKISS_TNC)
            && (devices[port].device_type != DEVICE_AX25_TNC)
            && (devices[port].device_type != DEVICE_NET_AGWPE) ) {
        return;
    }


sprintf(big_string,"\nrelay_digipeat: inputs:\n\tport: %d\n\tcall: %s\n\tpath: %s\n\tinfo: %s\n",
    port, call, path, info);


    // Check to see if this is my own transmitted packet (in some
    // cases you get your own packets back from interfaces)
    if (!strcasecmp(call, my_callsign)) {
        //fprintf(stderr,"relay_digipeat: packet was mine, don't digipeat it!\n");
        return;
    }


    // Make a copy of the incoming path.  The string passed to
    // split_string() gets destroyed.
    xastir_snprintf(temp_string,
        sizeof(temp_string),
        "%s",
        path);
    split_string(temp_string, Substring, MAX_RELAY_SUBSTRINGS);
    // Each element in the path is now pointed to by a char ptr in
    // the Substring array.  If a NULL is found in the array, that's
    // the end of the path.


    if (Substring[0] == NULL) {
        // Something's wrong!  Couldn't find anything in the path
        // string, not even a destination callsign?
//fprintf(stderr, "\t\tNo path: %s\n", path);
        return;
    }
    else {  // Save the destination callsign away
        xastir_snprintf(destination,
            sizeof(destination),
            "%s",
            Substring[0]);
        //fprintf(stderr,"Destination: %s\n",destination);
    }
    // We'll skip the first call in the path (pointed to by
    // Substring[0]) in the loops below.  That's the destination
    // call and we don't want to look for RELAY or my_callsign
    // there.


    // Check to see if we just ran out of path
    if (Substring[1] == NULL) { // No digipeaters listed
        //fprintf(stderr,"relay_digipeat: ran out of path, don't digipeat it!\n");
//fprintf(stderr, "\t\tNo digi's listed: %s\n", path);
        return;
    }


    //fprintf(stderr,"      Path: %s\n",path);
    // We could also loop through the array and dump them out until
    // we hit a NULL if necessary.


    // Find the first digipeater callsign _after_ any digis that
    // have asterisks.  Run through the array in reverse, looking
    // for the digi callsign with an asterisk after it that's
    // closest to the end of the path.
    ii = MAX_RELAY_SUBSTRINGS - 1;
    done = 0;
    while (!done && ii > 0) {

        if (Substring[ii] != NULL) {

            if (strstr(Substring[ii],"*")) {
                ii++;   // Found an asterisk:  Used digi.  Point to
                        // the digi _after_ this one.
                done++; // We found what we're looking for!
            }
            else {  // No asterisk found yet.
                ii--;
            }
        }
        else {  // No filled-in digipeater field found yet.
            ii--;
        }
    }

    if (Substring[ii] == NULL) {    // No unused digi's found.
                                    // We're done here.
//fprintf(stderr, "\t\tPath used up: %s\n", path);
        return;
    }

    if (ii == 0) {  // No asterisks found.  Entire path unused?
        // Set ii to first actual digi field instead of the
        // destination callsign.
        ii = 1;
    }
    else {  // ii points to first unused digi field.
    }

//fprintf(stderr,"\t\tUnused digi: %s\tPath: %s\n", Substring[ii], path);

    // Check for RELAY or my_callsign in the strings.  If neither
    // found then exit this routine.
    if (       (strcmp(Substring[ii], "RELAY")     != 0)
            && (strcmp(Substring[ii], my_callsign) != 0) ) {
        // Some other callsign found in this digi field.  Don't
        // relay the packet.
//fprintf(stderr,"Not relay or %s, skipping\n", my_callsign);
        return;
    }


    // Ok, we made it!  We have either RELAY or my_callsign that
    // hasn't been digipeated through, and we wish to change that
    // fact.  Put in our callsign for both cases and add an asterisk
    // to the end of the call.  Also had to fix up the KISS transmit
    // routine so that it'll set the digipeated bit for each
    // callsign that has an asterisk.

    // Contruct the new digi call, with the trailing asterisk
    xastir_snprintf(new_digi,
        sizeof(new_digi),
        "%s*",
        my_callsign);
    Substring[ii] = new_digi; // Point to new digi string instead of old
    //fprintf(stderr,"*** new_digi: %s\tSubstring: %s\n",
    //    new_digi,
    //    Substring[ii]);

    // Construct the new path, substituting the correct portion.
    // Start with the first digi and a comma:
    xastir_snprintf(new_path,
        sizeof(new_path),
        "%s,",
        Substring[1]);
    
    ii = 2;
    while ( (Substring[ii] != NULL)
            && (ii < MAX_RELAY_SUBSTRINGS) ) {
        strncat(new_path,
            Substring[ii],
            sizeof(new_path) - strlen(new_path));
        ii++;
        if (Substring[ii] != NULL)  // Add a comma if more to come
            strncat(new_path,
                ",",
                sizeof(new_path) - strlen(new_path));
    }

//fprintf(stderr,"*** New Path: %s,%s\n", destination, new_path);


    if (       (devices[port].device_type == DEVICE_SERIAL_KISS_TNC)
            || (devices[port].device_type == DEVICE_SERIAL_MKISS_TNC) ) {


#ifdef SERIAL_KISS_RELAY_DIGI
//        fprintf(stderr,"KISS RELAY short_path: %s\n", short_path);
//        fprintf(stderr,"KISS RELAY   new_path: %s\n", new_path);
        send_ax25_frame(port, call, destination, new_path, info);
#endif

    }
    else if (devices[port].device_type == DEVICE_AX25_TNC) {
        char header_txt[MAX_LINE_SIZE+5];

        //fprintf(stderr,"AX25 RELAY   new_path: %s\n", new_path);

        // set from call
        xastir_snprintf(header_txt, sizeof(header_txt), "%c%s %s\r", '\3', "MYCALL", call);
        if (port_data[port].status == DEVICE_UP)
            port_write_string(port, header_txt);
        // set path
        xastir_snprintf(header_txt, sizeof(header_txt), "%c%s %s VIA %s\r", '\3', "UNPROTO",
            destination, new_path);
        if (port_data[port].status == DEVICE_UP)
            port_write_string(port, header_txt);
        // set converse mode
        xastir_snprintf(header_txt, sizeof(header_txt), "%c%s\r", '\3', "CONV");
        if (port_data[port].status == DEVICE_UP)
            port_write_string(port, header_txt);
        // send packet
        if (port_data[port].status == DEVICE_UP)
            port_write_string(port, info);
    }
    else if (devices[port].device_type == DEVICE_NET_AGWPE) {
        send_agwpe_packet(port, // Xastir interface port
            atoi(devices[port].device_host_filter_string), // AGWPE RadioPort
            '\0',                         // Type of frame (data)
            (unsigned char *)call,        // source
            (unsigned char *)destination, // destination
            (unsigned char *)new_path,    // Path,
            (unsigned char *)info,
            strlen(info));
    }


//WE7U
sprintf(small_string,"relay_digipeat: outputs:\n\tport: %d\n\tcall: %s\n\tdest: %s\n\tpath: %s\n\tinfo: %s\n",
    port, call, destination, new_path, info);
strncat(big_string,
    small_string,
    sizeof(big_string) - strlen(big_string));
//fprintf(stderr,"%s",big_string);


// Example packet:
//K7FZO>APW251,SEATAC*,WIDE4-1:=4728.00N/12140.83W;PHG3030/Middle Fork Snoqualmie River -251-<630>

}





/*
 *  Decode AX.25 line
 *  \r and \n should already be stripped from end of line
 *  line should not be NULL
 *
 * If dbadd is set, add to database.  Otherwise, just return true/false
 * to indicate whether input is valid AX25 line.
 */
//
// Note that the length of "line" can be up to MAX_DEVICE_BUFFER,
// which is currently set to 4096.
//
int decode_ax25_line(char *line, char from, int port, int dbadd) {
    char *call_sign;
    char *path0;
    char path[100+1];           // new one, we may add an '*'
    char *info;
    char info_copy[MAX_LINE_SIZE+1];
    char call[MAX_CALLSIGN+1];
    char origin[MAX_CALLSIGN+1];
    int ok;
    int third_party;
    char backup[MAX_LINE_SIZE+1];
    char tmp_line[MAX_LINE_SIZE+1];


    xastir_snprintf(backup,
        sizeof(backup),
        "%s",
        line);

    if (debug_level & 1) {
        char filtered_data[MAX_LINE_SIZE+1];

        xastir_snprintf(filtered_data,
            sizeof(filtered_data),
            "%s",
            line);
        filtered_data[MAX_LINE_SIZE] = '\0';    // Terminate it

        makePrintable(filtered_data);
        fprintf(stderr,"decode_ax25_line: start parsing %s\n", filtered_data);
    }

    if (line == NULL) {
        fprintf(stderr,"decode_ax25_line: line == NULL.\n");
        return(FALSE);
    }

    if ( (line != NULL) && (strlen(line) > MAX_TNC_LINE_SIZE) ) { // Overly long message, throw it away.  We're done.
        if (debug_level & 1)
            fprintf(stderr,"\ndecode_ax25_line: LONG packet.  Dumping it:\n%s\n",line);
        return(FALSE);
    }

    if (line[strlen(line)-1] == '\n')           // better: look at other places,
                                                // so that we don't get it here...
        line[strlen(line)-1] = '\0';            // Wipe out '\n', to be sure
    if (line[strlen(line)-1] == '\r')
        line[strlen(line)-1] = '\0';            // Wipe out '\r'

    call_sign   = NULL;
    path0       = NULL;
    info        = NULL;
    origin[0]   = '\0';
    call[0]     = '\0';
    path[0]     = '\0';
    third_party = 0;

    // CALL>PATH:APRS-INFO-FIELD                // split line into components
    //     ^    ^
    ok = 0;
    call_sign = strtok(line,">");               // extract call from AX.25 line
    if (call_sign != NULL) {
        path0 = strtok(NULL,":");               // extract path from AX.25 line
        if (path0 != NULL) {
            info = strtok(NULL,"");             // rest is info_field
            if (info != NULL) {
                if ((info - path0) < 100) {     // check if path could be copied
                    ok = 1;                     // we have found all three components
                }
            }
        }
    }

    if (ok) {

        xastir_snprintf(path,
            sizeof(path),
            "%s",
            path0);

        xastir_snprintf(info_copy,
            sizeof(info_copy),
            "%s",
            info);

        ok = valid_path(path);                  // check the path and convert it to TAPR format
        // Note that valid_path() also removes igate injection identifiers

        if ((debug_level & 1) && !ok) {
            char filtered_data[MAX_LINE_SIZE + 1];

            xastir_snprintf(filtered_data,
                sizeof(filtered_data),
                "%s",
                path);
            makePrintable(filtered_data);
            fprintf(stderr,"decode_ax25_line: invalid path: %s\n",filtered_data);
        }
    }

    if (ok) {

        // Attempt to digipeat this packet if we should.  If port=-2,
        // we received this packet from the x_spider server and we
        // should not attempt to digipeat it.  If port=-1, it's from
        // a log file.  Again, don't digipeat it.
        if (port >= 0)
            relay_digipeat(call_sign, path, info, port);
 
        extract_TNC_text(info);                 // extract leading text from TNC X-1J4
        if (strlen(info) > 256)                 // first check if information field conforms to APRS specs
            ok = 0;                             // drop packets too long
        if ((debug_level & 1) && !ok) {
            char filtered_data[MAX_LINE_SIZE + 1];

            xastir_snprintf(filtered_data,
                sizeof(filtered_data),
                "%s",
                info);
            makePrintable(filtered_data);
            fprintf(stderr,"decode_ax25_line: info field too long: %s\n",filtered_data);
        }
    }

    if (ok) {                                                   // check callsign
        (void)remove_trailing_asterisk(call_sign);              // is an asterisk valid here ???
        if (valid_inet_name(call_sign,info,origin,sizeof(origin))) { // accept some of the names used in internet
            xastir_snprintf(call,
                sizeof(call),
                "%s",
                call_sign);
        } else if (valid_call(call_sign)) {                     // accept real AX.25 calls
            xastir_snprintf(call,
                sizeof(call),
                "%s",
                call_sign);
        } else {
            ok = 0;
            if (debug_level & 1) {
                char filtered_data[MAX_LINE_SIZE + 1];

                xastir_snprintf(filtered_data,
                    sizeof(filtered_data),
                    "%s",
                    call_sign);
                makePrintable(filtered_data);
                fprintf(stderr,"decode_ax25_line: invalid call: %s\n",filtered_data);
            }
        }
    }

    if (!dbadd)
    {
        if (debug_level & 1)
            fprintf(stderr,"decode_ax25_line: exiting\n");

        return(ok);
    }

    if (ok && info[0] == '}') {                                 // look for third-party traffic
        ok = extract_third_party(call,path,sizeof(path),&info,origin,sizeof(origin)); // extract third-party data
        third_party = 1;

        // Add it to the HEARD queue for this interface.  We use this
        // for igating purposes.  If some other igate beat us to this
        // packet, we don't want to duplicate it over the air.  If
        // port=-2, we received it from the x_spider server and we
        // should not save it in the queue.  If port=-1, the packet
        // came from a log file and again we shouldn't save it to
        // the queue.
        if (port >= 0)
            insert_into_heard_queue(port, backup);
    }

    if (ok && (info[0] == ';' || info[0] == ')')) {             // look for objects or items
        xastir_snprintf(origin,
            sizeof(origin),
            "%s",
            call);
        ok = extract_object(call,&info,origin);                 // extract object data
    }

    if (ok) {
        // decode APRS information field, always called with valid call and path
        // info is a string with 0 - 256 bytes
        // fprintf(stderr,"dec: %s (%s) %s\n",call,origin,info);
        if (debug_level & 1) {
            char filtered_data[MAX_LINE_SIZE+80];
            sprintf(filtered_data,
                "Registering data %s %s %s %s %c %d %d",
                call, path, info, origin, from, port, third_party);
            makePrintable(filtered_data);
            fprintf(stderr,"c/p/i/o fr pt tp: %s\n", filtered_data);
        }
        decode_info_field(call,path,info,origin,from,port,third_party);
    }


    if (port == -2) {    // We received this packet from an x_spider
        // server.  We need to dump it out all of our
        // transmit-enabled ports.

        // If the string starts with "user" or "pass", it's an
        // authentication string.  We need to send those through as
        // well so that the user gets logged into the internet
        // server and can send/receive packets/messages.  We also
        // dump it to our console so that we can see who logged in
        // to us.
        if (strncasecmp(line,"user",4) == 0
                || strncasecmp(line,"pass",4 == 0)
                || strncasecmp(line,"filter",6 == 0)) {
            fprintf(stderr,"\tLogged on: %s\n", line);

// If the line has a "filter" parameter in it, we need to remove it,
// else a client may change our filtering parameters.  Perhaps we
// should skip the authentication stuff as well, as the servers
// might get confused if we pass two different authentications on
// the same socket?

        }
        else if (strlen(line) > 0) {    // Not empty
            // Send the packet unchanged out all of our
            // transmit-enabled ports.  We should send it as
            // third-party igated packets if we're sending to
            // servers, send it unchanged if sending through TNC?

//fprintf(stderr,"Retransmitting x_spider packet: %s\n", line);

            // Here's where we inject our own callsign like this:
            // "WE7U-15*,I" in order to provide injection ID for our
            // igate.  We need to add a '*' character after our
            // callsign as we inject, to show it came through us.
            // On the way back out of the internet it can get a '*'
            // added after the 'I' perhaps.
            xastir_snprintf(tmp_line,
                sizeof(tmp_line),
                "%s>%s,%s*,I:%s",
                call_sign,
                path,
                my_callsign,
                info_copy);

//fprintf(stderr,"decode_ax25_line: IGATE>NET %s\n",tmp_line);
//fprintf(stderr,"call: %s\tcall_sign: %s\n", call, call_sign);

            output_igate_net(tmp_line, port, 0); // 0="not third-party"
        }
    }


    if (debug_level & 1)
        fprintf(stderr,"decode_ax25_line: exiting\n");

    return(ok);
}





/*
 *  Read a line from file.  We use this to read in log files and to
 *  read in findu track files.  For findu track files we need to get
 *  rid of the <br> at the end of the lines, else it shows up in our
 *  comment lines in Station_info.
 */
void  read_file_line(FILE *f) {
    char line[MAX_LINE_SIZE+1];
    char cin;
    int pos;

    pos = 0;
    line[0] = '\0';
    while (!feof(f)) {
        if (fread(&cin,1,1,f) == 1) {
            if (cin != (char)10 && cin != (char)13) {   // normal characters
                if (pos < MAX_LINE_SIZE) {
                    line[pos++] = cin;
                 }
            } else {                                    // CR or LF
                if (cin == (char)10) {                  // Found LF as EOL char
                    char *ptr;

                    line[pos] = '\0';                   // Always add a terminating zero after last char
                    pos = 0;                            // start next line

                    // Get rid of <br> HTML tag at end of line here.
                    // Findu track files have them.
                    ptr = strstr(line, "<br>");
                    if (ptr) {  // Found one of them
                        *ptr = '\0';  // Terminate the line at that point
                    }

                    // Save a backup copy of the incoming string.
                    // Used for debugging purposes.  If we get a
                    // segfault, we can print out the last message
                    // received.
                    xastir_snprintf((char *)incoming_data_copy,
                        MAX_LINE_SIZE,
                        "%s",
                        line);

                    decode_ax25_line(line,'F',-1, 1);   // Decode the packet
                    return;                             // only read line by line
                }
            }
        }
    }
    if (feof(f)) {                                      // Close file if at the end
        (void)fclose(f);
        read_file = 0;
        statusline(langcode("BBARSTA012"),0);           // File done..
        redraw_on_new_data = 2;                         // redraw immediately after finish
    }
}





/*
 *  Center map to new position
 */
void set_map_position(Widget w, long lat, long lon) {
    // see also map_pos() in location.c

    // Set interrupt_drawing_now because conditions have changed
    // (new map center).
    interrupt_drawing_now++;

    set_last_position();
    mid_y_lat_offset  = lat;
    mid_x_long_offset = lon;
    setup_in_view();  // flag all stations in new screen view

    // Request that a new image be created.  Calls create_image,
    // XCopyArea, and display_zoom_status.
    request_new_image++;

//    if (create_image(w)) {
//        (void)XCopyArea(XtDisplay(w),pixmap_final,XtWindow(w),gc,0,0,screen_width,screen_height,0,0);
//    }
}





/*
 *  Search for a station to be located (for Tracking and Find Station)
 */
int locate_station(Widget w, char *call, int follow_case, int get_match, int center_map) {
    DataRow *p_station;
    char call_find[MAX_CALLSIGN+1];
    char call_find1[MAX_CALLSIGN+1];
    int ii;
    int call_len;

    call_len = 0;
    if (!follow_case) {
        for (ii=0; ii<(int)strlen(call); ii++) {
            if (isalpha((int)call[ii]))
                call_find[ii] = toupper((int)call[ii]);         // Problem with lowercase objects/names!!
            else
                call_find[ii] = call[ii];
        }
        call_find[ii] = '\0';
        xastir_snprintf(call_find1,
            sizeof(call_find1),
            "%s",
            call_find);
    } else
        xastir_snprintf(call_find1,
            sizeof(call_find1),
            "%s",
            call);

    if (search_station_name(&p_station,call_find1,get_match)) {
        if (position_defined(p_station->coord_lat,p_station->coord_lon,0)) {
            if (center_map || !position_on_inner_screen(p_station->coord_lat,p_station->coord_lon))
                 // only change map if really neccessary
                set_map_position(w, p_station->coord_lat, p_station->coord_lon);
            return(1);                  // we found it
        }
    }
    return(0);
}





/*
 *  Look for other stations that the tracked one has gotten close to.
 *  and speak a proximity warning.
 *   TODO: 
 *    - sort matches by distance
 *    - set upper bound on number of matches so we don't speak forever
 *    - use different proximity distances for different station types?
 *    - look for proximity to embedded map objects
 */
void search_tracked_station(DataRow **p_tracked) {
    DataRow *t = (*p_tracked);
    DataRow *curr = NULL;


    if (debug_level & 1) {
        char lat[20],lon[20];
    
        convert_lat_l2s(t->coord_lat,
                        lat,
                        sizeof(lat),
                        CONVERT_HP_NORMAL);
        convert_lon_l2s(t->coord_lon,
                        lon,
                        sizeof(lat),
                        CONVERT_HP_NORMAL);

        fprintf(stderr,"Searching for stations close to tracked station %s at %s %s ...\n",
                t->call_sign,lat,lon);
    }

    while (next_station_time(&curr)) {
        if (curr != t && curr->flag&ST_ACTIVE) {

            float distance; // Distance in whatever measurement
                            // units we're currently using.
            char bearing[10];
            char station_id[600];

            distance =  (float)calc_distance_course(t->coord_lat,
                                   t->coord_lon, 
                                   curr->coord_lat,
                                   curr->coord_lon,
                                   bearing,
                                   sizeof(bearing)) * cvt_kn2len;

            if (debug_level & 1) 
                fprintf(stderr,"Looking at %s: distance %.3f bearing %s (%s)\n",
                        curr->call_sign,distance,bearing,convert_bearing_to_name(bearing,1));

            /* check ranges (copied from earlier prox alert code, above) */
            if ((distance > atof(prox_min)) && (distance < atof(prox_max))) {
                if (debug_level & 1) 
                        fprintf(stderr," tracked station is near %s!\n",curr->call_sign);

                if (sound_play_prox_message) {
                        sprintf(station_id,"%s < %.3f %s from %s",t->call_sign, distance,
                                english_units?langcode("UNIOP00004"):langcode("UNIOP00005"),
                                curr->call_sign);
                        statusline(station_id,0);
                        play_sound(sound_command,sound_prox_message);
                }
#ifdef HAVE_FESTIVAL
                if (festival_speak_tracked_proximity_alert) {
                        if (english_units) {
                            if (distance < 1.0)
                                sprintf(station_id, langcode("SPCHSTR007"), t->call_sign,
                                        (int)(distance * 1760), langcode("SPCHSTR004"),
                                        convert_bearing_to_name(bearing,1), curr->call_sign);
                            else if ((int)((distance * 10) + 0.5) % 10)
                                sprintf(station_id, langcode("SPCHSTR008"), t->call_sign, distance,
                                        langcode("SPCHSTR003"), convert_bearing_to_name(bearing,1),
                                        curr->call_sign);
                            else
                                sprintf(station_id, langcode("SPCHSTR007"), t->call_sign, (int)(distance + 0.5),
                                        langcode("SPCHSTR003"), convert_bearing_to_name(bearing,1),
                                        curr->call_sign);
                        } else {                /* metric */
                            if (distance < 1.0)
                                sprintf(station_id, langcode("SPCHSTR007"), t->call_sign,
                                        (int)(distance * 1000), langcode("SPCHSTR002"), 
                                        convert_bearing_to_name(bearing,1), curr->call_sign);
                            else if ((int)((distance * 10) + 0.5) % 10)
                                sprintf(station_id, langcode("SPCHSTR008"), t->call_sign, distance,
                                        langcode("SPCHSTR001"), 
                                        convert_bearing_to_name(bearing,1), curr->call_sign);
                            else
                                sprintf(station_id, langcode("SPCHSTR007"), t->call_sign, (int)(distance + 0.5),
                                        langcode("SPCHSTR001"), convert_bearing_to_name(bearing,1),
                                        curr->call_sign);
                        }
                    if (debug_level & 1) 
                        fprintf(stderr," %s\n",station_id);
                    SayText(station_id);
                }
#endif  /* HAVE_FESTIVAL */
            }
        }
    }   // end of while
}





/*
 *  Change map position if neccessary while tracking a station
 *      we call it with defined station call and position
 */
void track_station(Widget w, char *call_tracked, DataRow *p_station) {
    long x_ofs, y_ofs;
    long new_lat, new_lon;

    if ( is_tracked_station(p_station->call_sign) ) {   // We want to track this station
        new_lat = p_station->coord_lat;                 // center map to station position as default
        new_lon = p_station->coord_lon;
        x_ofs = new_lon - mid_x_long_offset;            // current offset from screen center
        y_ofs = new_lat - mid_y_lat_offset;
        if ((labs(x_ofs) > (screen_width*scale_x/3)) || (labs(y_ofs) > (screen_height*scale_y/3))) {
            // only redraw map if near border (margin 1/6 of screen at each side)
            if (labs(y_ofs) < (screen_height*scale_y/2))
                new_lat  += y_ofs/2;                    // give more space in driving direction
            if (labs(x_ofs) < (screen_width*scale_x/2))
                new_lon += x_ofs/2;

            set_map_position(w, new_lat, new_lon);      // center map to new position

        }
        search_tracked_station(&p_station);
    }
}





// We have a lot of code duplication between Setup_object_data,
// Setup_item_data, and Create_object_item_tx_string.
//
// Make sure to look at the "transmit_compressed_objects_items" variable
// to decide whether to send a compressed packet.
/*
 *  Create the transmit string for Objects/Items.
 *  Input is a DataRow struct, output is both an integer that says
 *  whether it completed successfully, and a char* that has the
 *  output tx string in it.
 *
 *  Returns 0 if there's a problem.
 */
int Create_object_item_tx_string(DataRow *p_station, char *line, int line_length) {
    int i, done;
    char lat_str[MAX_LAT];
    char lon_str[MAX_LONG];
    char comment[43+1];                 // max 43 characters of comment
    char comment2[43+1];
    char time[7+1];
    struct tm *day_time;
    time_t sec;
    char complete_area_color[3];
    int complete_area_type;
    int lat_offset, lon_offset;
    char complete_corridor[6];
    char altitude[10];
    char speed_course[8];
    int speed;
    int course;
    int temp;
    long temp2;
    char signpost[6];
    int bearing;
    char tempstr[50];
    char object_group;
    char object_symbol;
    int killed = 0;


    (void)remove_trailing_spaces(p_station->call_sign);
    //(void)to_upper(p_station->call_sign);     Not per spec.  Don't use this.

    if ((p_station->flag & ST_OBJECT) != 0) {   // We have an object
        if (!valid_object(p_station->call_sign)) {
            line[0] = '\0';
            return(0);
        }
    }
    else if ((p_station->flag & ST_ITEM) != 0) {    // We have an item
        xastir_snprintf(tempstr,
            sizeof(tempstr),
            "%s",
            p_station->call_sign);
        if (strlen(tempstr) == 1)  // Add two spaces (to make 3 minimum chars)
            xastir_snprintf(p_station->call_sign, sizeof(p_station->call_sign), "%s  ",tempstr);
        else if (strlen(tempstr) == 2) // Add one space (to make 3 minimum chars)
            xastir_snprintf(p_station->call_sign, sizeof(p_station->call_sign), "%s ",tempstr);

        if (!valid_item(p_station->call_sign)) {
            line[0] = '\0';
            return(0);
        }
    }
    else {  // Not an item or an object, what are we doing here!
        line[0] = '\0';
        return(0);
    }

    // Lat/lon are in Xastir coordinates, so we need to convert
    // them to APRS string format here.
    convert_lat_l2s(p_station->coord_lat, lat_str, sizeof(lat_str), CONVERT_LP_NOSP);
    convert_lon_l2s(p_station->coord_lon, lon_str, sizeof(lon_str), CONVERT_LP_NOSP);

    // Check for an overlay character.  Replace the group character
    // (table char) with the overlay if present.
    if (p_station->aprs_symbol.special_overlay != '\0') {
        // Overlay character found
        object_group = p_station->aprs_symbol.special_overlay;
        if ( (object_group >= '0' && object_group <= '9')
                || (object_group >= 'A' && object_group <= 'Z') ) {
            // Valid overlay character, use what we have
        }
        else {
            // Bad overlay character, throw it away
            object_group = '\\';
        }
    }
    else {    // No overlay character
        object_group = p_station->aprs_symbol.aprs_type;
    }

    object_symbol = p_station->aprs_symbol.aprs_symbol;

    // In this case we grab only the first comment field (if it
    // exists) for the object/item
    if ( (p_station->comment_data != NULL)
            && (p_station->comment_data->text_ptr != NULL) ){
        xastir_snprintf(comment,
            sizeof(comment),
            "%s",
            p_station->comment_data->text_ptr);
    }
    else {
        comment[0] = '\0';  // Empty string
    }

//WE7U
    if ( (p_station->probability_min[0] != '\0')
            || (p_station->probability_max[0] != '\0') ) {

        if (p_station->probability_max[0] == '\0') {
            // Only have probability_min
            xastir_snprintf(comment2,
                sizeof(comment2)," Pmin%s, %s",
                p_station->probability_min,
                comment);
        }
        else if (p_station->probability_min[0] == '\0') {
            // Only have probability_max
            xastir_snprintf(comment2,
                sizeof(comment2)," Pmax%s, %s",
                p_station->probability_max,
                comment);
        }
        else {  // Have both
            xastir_snprintf(comment2,
                sizeof(comment2)," Pmin%s, Pmax%s, %s",
                p_station->probability_min,
                p_station->probability_max,
                comment);
        }
        xastir_snprintf(comment,sizeof(comment),comment2);
    }


    // Put RNG or PHG at the beginning of the comment
    xastir_snprintf(comment2,
        sizeof(comment2),
        "%s%s",
        p_station->power_gain,
        comment);
    xastir_snprintf(comment,
        sizeof(comment),
        "%s",
        comment2);
   
 
    (void)remove_trailing_spaces(comment);


    // This is for objects only, not items.  Uses current time but
    // should use the transmitted time from the DataRow struct.
    // Which time field in the struct would that be?  Have to find out
    // from the extract_?? code.
    if ((p_station->flag & ST_OBJECT) != 0) {
        sec = sec_now();
        day_time = gmtime(&sec);
        xastir_snprintf(time,
            sizeof(time),
            "%02d%02d%02dz",
            day_time->tm_mday,
            day_time->tm_hour,
            day_time->tm_min);
    }


// Handle Generic Options


    // Speed/Course Fields
    xastir_snprintf(speed_course, sizeof(speed_course), ".../");   // Start with invalid-data string
    course = 0;
    if (strlen(p_station->course) != 0) {    // Course was entered
        // Need to check for 1 to three digits only, and 001-360 degrees)
        temp = atoi(p_station->course);
        if ( (temp >= 1) && (temp <= 360) ) {
            xastir_snprintf(speed_course, sizeof(speed_course), "%03d/",temp);
            course = temp;
        }
        else if (temp == 0) {   // Spec says 001 to 360 degrees...
            xastir_snprintf(speed_course, sizeof(speed_course), "360/");
        }
    }
    speed = 0;
    if (strlen(p_station->speed) != 0) { // Speed was entered (we only handle knots currently)
        // Need to check for 1 to three digits, no alpha characters
        temp = atoi(p_station->speed);
        if ( (temp >= 0) && (temp <= 999) ) {
            xastir_snprintf(tempstr, sizeof(tempstr), "%03d",temp);
            strncat(speed_course,
                tempstr,
                sizeof(speed_course) - strlen(speed_course));
            speed = temp;
        }
        else {
            strncat(speed_course,
                "...",
                sizeof(speed_course) - strlen(speed_course));
        }
    }
    else {  // No speed entered, blank it out
        strncat(speed_course,
            "...",
            sizeof(speed_course) - strlen(speed_course));
    }
    if ( (speed_course[0] == '.') && (speed_course[4] == '.') ) {
        speed_course[0] = '\0'; // No speed or course entered, so blank it
    }
    if (p_station->aprs_symbol.area_object.type != AREA_NONE) {  // It's an area object
        speed_course[0] = '\0'; // Course/Speed not allowed if Area Object
    }

    // Altitude Field
    altitude[0] = '\0'; // Start with empty string
    if (strlen(p_station->altitude) != 0) {   // Altitude was entered (we only handle feet currently)
        // Need to check for all digits, and 1 to 6 digits
        if (isdigit((int)p_station->altitude[0])) {
            // Must convert from meters to feet before transmitting
            temp2 = (int)( (atof(p_station->altitude) / 0.3048) + 0.5);
            if ( (temp2 >= 0) && (temp2 <= 99999l) ) {
                xastir_snprintf(altitude, sizeof(altitude), "/A=%06ld",temp2);
            }
        }
    }


// Handle Specific Options


    // Area Objects
    if (p_station->aprs_symbol.area_object.type != AREA_NONE) { // It's an area object

        // Note that transmitted color consists of two characters, from "/0" to "15"
        xastir_snprintf(complete_area_color, sizeof(complete_area_color), "%02d", p_station->aprs_symbol.area_object.color);
        if (complete_area_color[0] == '0')
            complete_area_color[0] = '/';

        complete_area_type = p_station->aprs_symbol.area_object.type;

        lat_offset = p_station->aprs_symbol.area_object.sqrt_lat_off;
        lon_offset = p_station->aprs_symbol.area_object.sqrt_lon_off;

        // Corridor
        complete_corridor[0] = '\0';
        if ( (complete_area_type == 1) || (complete_area_type == 6) ) {
            if (p_station->aprs_symbol.area_object.corridor_width > 0) {
                xastir_snprintf(complete_corridor, sizeof(complete_corridor), "{%d}",
                        p_station->aprs_symbol.area_object.corridor_width);
            }
        }

        if ((p_station->flag & ST_OBJECT) != 0) {   // It's an object
            xastir_snprintf(line, line_length, ";%-9s*%s%s%c%s%c%1d%02d%2s%02d%s%s%s",
                p_station->call_sign,
                time,
                lat_str,
                object_group,
                lon_str,
                object_symbol,
                complete_area_type,
                lat_offset,
                complete_area_color,
                lon_offset,
                speed_course,
                complete_corridor,
                altitude);
        }
        else  {     // It's an item
            xastir_snprintf(line, line_length, ")%s!%s%c%s%c%1d%02d%2s%02d%s%s%s",
                p_station->call_sign,
                lat_str,
                object_group,
                lon_str,
                object_symbol,
                complete_area_type,
                lat_offset,
                complete_area_color,
                lon_offset,
                speed_course,
                complete_corridor,
                altitude);
        }
    }

    else if ( (p_station->aprs_symbol.aprs_type == '\\') // We have a signpost object
            && (p_station->aprs_symbol.aprs_symbol == 'm' ) ) {
        if (strlen(p_station->signpost) > 0) {
            xastir_snprintf(signpost, sizeof(signpost), "{%s}", p_station->signpost);
        }
        else {  // No signpost data entered, blank it out
            signpost[0] = '\0';
        }
        if ((p_station->flag & ST_OBJECT) != 0) {   // It's an object
            xastir_snprintf(line, line_length, ";%-9s*%s%s%c%s%c%s%s%s",
                p_station->call_sign,
                time,
                lat_str,
                object_group,
                lon_str,
                object_symbol,
                speed_course,
                altitude,
                signpost);
        }
        else {  // It's an item
            xastir_snprintf(line, line_length, ")%s!%s%c%s%c%s%s%s",
                p_station->call_sign,
                lat_str,
                object_group,
                lon_str,
                object_symbol,
                speed_course,
                altitude,
                signpost);
        }
    }

    else if (p_station->signal_gain[0] != '\0') { // Must be an Omni-DF object/item
        if ((p_station->flag & ST_OBJECT) != 0) {   // It's an object
            xastir_snprintf(line, line_length, ";%-9s*%s%s%c%s%c%s/%s%s",
                p_station->call_sign,
                time,
                lat_str,
                object_group,
                lon_str,
                object_symbol,
                p_station->signal_gain,
                speed_course,
                altitude);
        }
        else {  // It's an item
            xastir_snprintf(line, line_length, ")%s!%s%c%s%c%s/%s%s",
                p_station->call_sign,
                lat_str,
                object_group,
                lon_str,
                object_symbol,
                p_station->signal_gain,
                speed_course,
                altitude);
        }
    }
    else if (p_station->NRQ[0] != 0) {  // It's a Beam Heading DFS object/item

        if (strlen(speed_course) != 7)
                xastir_snprintf(speed_course,
                    sizeof(speed_course),
                    "000/000");

        bearing = atoi(p_station->bearing);
        if ( (bearing < 1) || (bearing > 360) )
                bearing = 360;

        if ((p_station->flag & ST_OBJECT) != 0) {   // It's an object
            xastir_snprintf(line, line_length, ";%-9s*%s%s%c%s%c%s/%03i/%s%s",
                p_station->call_sign,
                time,
                lat_str,
                object_group,
                lon_str,
                object_symbol,
                speed_course,
                bearing,
                p_station->NRQ,
                altitude);
        }
        else {  // It's an item
            xastir_snprintf(line, line_length, ")%s!%s%c%s%c%s/%03i/%s%s",
                p_station->call_sign,
                lat_str,
                object_group,
                lon_str,
                object_symbol,
                speed_course,
                bearing,
                p_station->NRQ,
                altitude);
        }
    }

    else {  // Else it's a normal object/item
        if ((p_station->flag & ST_OBJECT) != 0) {   // It's an object
            if (transmit_compressed_objects_items) {
                char temp_group = object_group;

                // If we have a numeric overlay, we need to convert
                // it to 'a-j' for compressed objects.
                if (temp_group >= '0' && temp_group <= '9') {
                    temp_group = temp_group + 'a';
                }

                // We need higher precision lat/lon strings than
                // those created above.
                convert_lat_l2s(p_station->coord_lat, lat_str, sizeof(lat_str), CONVERT_HP_NOSP);
                convert_lon_l2s(p_station->coord_lon, lon_str, sizeof(lon_str), CONVERT_HP_NOSP);

                xastir_snprintf(line, line_length, ";%-9s*%s%s%s",
                    p_station->call_sign,
                    time,
                    compress_posit(lat_str,
                        temp_group,
                        lon_str,
                        object_symbol,
                        course,
                        speed,  // In knots
                        ""),    // PHG, must be blank
                        altitude);
            }
            else {  // Non-compressed posit object
                xastir_snprintf(line, line_length, ";%-9s*%s%s%c%s%c%s%s",
                    p_station->call_sign,
                    time,
                    lat_str,
                    object_group,
                    lon_str,
                    object_symbol,
                    speed_course,
                    altitude);
            }
        }
        else {  // It's an item
            if (transmit_compressed_objects_items) {
                char temp_group = object_group;

                // If we have a numeric overlay, we need to convert
                // it to 'a-j' for compressed objects.
                if (temp_group >= '0' && temp_group <= '9') {
                    temp_group = temp_group + 'a';
                }

                // We need higher precision lat/lon strings than
                // those created above.
                convert_lat_l2s(p_station->coord_lat, lat_str, sizeof(lat_str), CONVERT_HP_NOSP);
                convert_lon_l2s(p_station->coord_lon, lon_str, sizeof(lon_str), CONVERT_HP_NOSP);
 
                xastir_snprintf(line, line_length, ")%s!%s%s",
                    p_station->call_sign,
                    compress_posit(lat_str,
                        temp_group,
                        lon_str,
                        object_symbol,
                        course,
                        speed,  // In knots
                        ""),    // PHG, must be blank
                        altitude);
            }
            else {  // Non-compressed item
                xastir_snprintf(line, line_length, ")%s!%s%c%s%c%s%s",
                    p_station->call_sign,
                    lat_str,
                    object_group,
                    lon_str,
                    object_symbol,
                    speed_course,
                    altitude);
            }
        }
    }

    // If it's a "killed" object, change '*' to an '_'
    if ((p_station->flag & ST_OBJECT) != 0) {               // It's an object
        if ((p_station->flag & ST_ACTIVE) != ST_ACTIVE) {   // It's been killed
            line[10] = '_';
            killed++;
        }
    }
    // If it's a "killed" item, change '!' to an '_'
    else {                                                  // It's an item
        if ((p_station->flag & ST_ACTIVE) != ST_ACTIVE) {   // It's been killed
            killed++;
            done = 0;
            i = 0;
            while ( (!done) && (i < 11) ) {
                if (line[i] == '!') {
                    line[i] = '_';          // mark as deleted object
                    done++;                 // Exit from loop
                }
                i++;
            }
        }
    }

    // Check whether we need to stop transmitting particular killed
    // object/items now.
    if (killed) {
        // Check whether we should decrement the object_retransmit
        // counter so that we will eventually stop sending this
        // object/item.
        if (p_station->object_retransmit == 0) {
            // We shouldn't be transmitting this killed object/item
            // anymore.  We're already done transmitting it.

//fprintf(stderr, "Done transmitting this object: %s,  %d\n",
//p_station->call_sign,
//p_station->object_retransmit);

            return(0);
        }

        // Check whether the timeout has been set yet on this killed
        // object/item.  If not, change it from -1 (continuous
        // transmit of non-killed objects) to
        // MAX_KILLED_OBJECT_RETRANSMIT.
        if (p_station->object_retransmit <= -1) {

//fprintf(stderr, "Killed object %s, setting retries, %d -> %d\n",
//p_station->call_sign,
//p_station->object_retransmit,
//MAX_KILLED_OBJECT_RETRANSMIT - 1);

                if ((MAX_KILLED_OBJECT_RETRANSMIT - 1) < 0) {
                    p_station->object_retransmit = 0;
                    return(0);  // No retransmits desired
                }
                else {
                    p_station->object_retransmit = MAX_KILLED_OBJECT_RETRANSMIT - 1;
                }
        }
        else {
            // Decrement the timeout if it is a positive number.
            if (p_station->object_retransmit > 0) {

//fprintf(stderr, "Killed object %s, decrementing retries, %d -> %d\n",
//p_station->call_sign,
//p_station->object_retransmit,
//p_station->object_retransmit - 1);

                p_station->object_retransmit--;
            }
        }
    }
 
    // We need to tack the comment on the end, but need to make
    // sure we don't go over the maximum length for an object/item.
    if (strlen(comment) != 0) {
        temp = 0;
        if ((p_station->flag & ST_OBJECT) != 0) {
            while ( (strlen(line) < 80) && (temp < (int)strlen(comment)) ) {
                //fprintf(stderr,"temp: %d->%d\t%c\n", temp, strlen(line), comment[temp]);
                line[strlen(line) + 1] = '\0';
                line[strlen(line)] = comment[temp++];
            }
        }
        else {  // It's an item
            while ( (strlen(line) < (64 + strlen(p_station->call_sign))) && (temp < (int)strlen(comment)) ) {
                //fprintf(stderr,"temp: %d->%d\t%c\n", temp, strlen(line), comment[temp]);
                line[strlen(line) + 1] = '\0';
                line[strlen(line)] = comment[temp++];
            }
        }
    }

    //fprintf(stderr,"line: %s\n",line);

// NOTE:  Compressed mode will be shorter still.  Account
// for that when compressed mode is implemented for objects/items.

    return(1);
}





// check_and_transmit_objects_items
//
// This function checks the last_transmit_time for each
// locally-owned object/item.  If it has been at least the
// transmit_time_increment since the last transmit, the increment is
// doubled and the object/item transmitted.
//
// Killed objects/items are transmitted for
// MAX_KILLED_OBJECT_RETRANSMIT times and then transmitting of those
// objects ceases.
//
// This would be a good place to implement auto-expiration of
// objects that's been discussed on the mailing lists.
//
// This function depends on the local loopback that is in
// interface.c.  If we don't hear & decode our own packets, we won't
// have our own objects/items in our list.
//
// We need to check DataRow objects for ST_OBJECT or ST_ITEM types
// that were transmitted by our callsign & SSID.  We might also need
// to modify the remove_time() and check_station_remove functions in
// order not to delete our own objects/items too quickly.
//
// insert_time/remove_time/next_station_time/prev_station_time
//
// It would be nice if the create/modify object dialog and this routine went
// through the same functions to create the transmitted packets:
//      main.c:Setup_object_data
//      main.c:Setup_item_data
// Unfortunately those routines snag their data directly from the dialog.
// In order to make them use the same code we'd have to separate out the
// fetch-from-dialog code from the create-transmit-packet code.
//
// This is what aprsDOS does, from Bob's APRS.TXT file:  "a
// fundamental precept is that old data is less important than new
// data."  "Each new packet is transmitted immediately, then 20
// seconds later.  After every transmission, the period is doubled.
// After 20 minutes only six packets have been transmitted.  From
// then on the rate remains at 10 minutes times the number of
// digipeater hops you are using."
// Actually, talking to Bob, he's used a period of 15 seconds as his
// base unit.  We now do the same using the OBJECT_CHECK_RATE define
// to set the initial timing.

//
// Added these to database.h:DataRow struct:
// time_t last_transmit_time;          // Time we last transmitted an object/item.  Used to
//                                     // implement decaying transmit time algorithm
// short transmit_time_increment;      // Seconds to add to transmit next time around.  Used
//                                     // to implement decaying transmit time algorithm
//
// The earlier code here transmitted objects/items at a specified
// rate.  This can cause large transmissions every OBJECT_rate
// seconds, as all objects/items are transmitted at once.  With the
// new code, the objects/items may be spaced a bit from each other
// time-wise, plus they are transmitted less and less often with
// each transmission until they hit the max interval specified by
// the "Object/Item TX Interval" slider.  When they hit that max
// interval, they are transmitted at the constant interval until
// killed.  When they are killed, they are transmitted for
// MAX_KILLED_OBJECT_RETRANSMIT iterations using the decaying
// algorithm, then transmissions cease.
//
void check_and_transmit_objects_items(time_t time) {
    DataRow *p_station;     // pointer to station data
    char line[256];
    int first = 1;  // Used to output debug message only once
    int increment;


    // Time to re-transmit objects/items?
    // Check every OBJECT_CHECK_RATE seconds - 20%.  No faster else
    // we'll be running through the station list too often and
    // wasting cycles.
    if (sec_now() < (last_object_check + (int)(4.0 * OBJECT_CHECK_RATE/5.0 + 1.0) ) )
        return;

//fprintf(stderr,"check_and_transmit_objects_items\n");

    // Set up timer for next go-around
    last_object_check = sec_now();

    if (debug_level & 1)
        fprintf(stderr,"Checking whether to retransmit any objects/items\n");

// We could speed things up quite a bit here by either keeping a
// separate list of our own objects/items, or going through the list
// of stations by time instead of by name (If by time, only check
// backwards from the current time by the max transmit interval plus
// some increment.  Watch out for the user changing the slider).

    p_station = n_first;    // Go through received stations alphabetically
    while (p_station != NULL) {

        //fprintf(stderr,"%s\t%s\n",p_station->call_sign,p_station->origin);

        // If station is owned by me
        if (is_my_call(p_station->origin,1)) {

            // and it's an object or item
            if (p_station->flag & (ST_OBJECT|ST_ITEM)) {

                if (debug_level & 1) {
                    fprintf(stderr,
                        "Found a locally-owned object or item: %s\n",
                        p_station->call_sign);
                }


// Implementing sped-up transmission of new objects, regular
// transmission of old objects (decaying algorithm).  We'll do this
// by keeping a last_transmit_time variable and a
// transmit_time_increment with each DataRow struct.  If the
// last_transmit_time is older than the transmit_time_increment, we
// transmit the object and double the increment variable, until we
// hit the OBJECT_rate limit for the increment.  This will make
// newer objects/items transmit more often, and will also space out
// the transmissions of old objects so they're not transmitted all
// at once in a group.  Each time a new object/item is created that
// is owned by us, it needs to have it's timer set to 20 (seconds).
// If an object/item is touched, it needs to again be set to 20
// seconds.
///////////////////////////////////

// Run through the station list.

// Transmit any objects/items that have equalled or gone past
// (last_transmit_time + transmit_time_increment).  Update the
// last_transmit_time to current time.
//
// Double the transmit_time_increment.  If it has gone beyond
// OBJECT_rate, set it to OBJECT_rate instead.
//
///////////////////////////////////


                // Check for the case where the timing slider has
                // been reduced and the expire time is too long.
                // Reset it to the current max expire time so that
                // it'll get transmitted more quickly.
                if (p_station->transmit_time_increment > OBJECT_rate)
                    p_station->transmit_time_increment = OBJECT_rate;

 
                increment = p_station->transmit_time_increment;

                if ( ( p_station->last_transmit_time + increment) <= sec_now() ) {
                    // We should transmit this object/item as it has
                    // hit its transmit interval.

                    float randomize;
                    int one_fifth_increment;
                    int new_increment;


                    if (first) {    // "Transmitting objects/items"
                        statusline(langcode("BBARSTA042"),1);
                        first = 0;
                    }

                    // Set up the new doubling increment
                    increment = increment * 2;
                    if (increment > OBJECT_rate) {
                        increment = OBJECT_rate;
                    }

                    // Randomize the distribution a bit, so that all
                    // objects are not transmitted at the same time.
                    // Allow the random number to vary over 20%
                    // (one-fifth) of the newly computed increment.
                    one_fifth_increment = (int)((increment / 5) + 0.5);
//fprintf(stderr,"one_fifth_increment: %d\n", one_fifth_increment);

                    // Scale the random number from 0.0 to 1.0.
                    // Must convert at least one of the numbers to a
                    // float else randomize will be zero every time.
                    randomize = rand() / (float)RAND_MAX;
//fprintf(stderr,"randomize: %f\n", randomize);

                    // Scale it to the range we want (0% to 20% of
                    // the interval)
                    randomize = randomize * one_fifth_increment;
//fprintf(stderr,"scaled randomize: %f\n", randomize);

                    // Subtract it from the increment, use
                    // poor-man's rounding to turn the random number
                    // into an int (so we get the full range).
                    new_increment = increment - (int)(randomize + 0.5);
                    p_station->transmit_time_increment = (short)new_increment;

//fprintf(stderr,"check_and_transmit_objects_items():Setting tx_increment to %d:%s\n",
//    new_increment,
//    p_station->call_sign);

                    // Set the last transmit time into the object.
                    // Keep this based off the time the object was
                    // last created/modified/deleted, so that we
                    // don't end up with a bunch of them transmitted
                    // together.
                    p_station->last_transmit_time = p_station->last_transmit_time + new_increment;
 
                    // Here we need to re-assemble and re-transmit the object or item
                    // Check whether it is a "live" or "killed" object and vary the
                    // number of retransmits accordingly.  Actually we should be able
                    // to keep retransmitting "killed" objects until they expire out of
                    // our station queue with no problems.  If someone wants to ressurect
                    // the object we'll get new info into our struct and this function will
                    // ignore that object from then on, unless we again snatch control of
                    // the object.

                    // if signpost, area object, df object, or generic object
                    // check p_station->APRS_Symbol->aprs_type:
                    //   APRS_OBJECT
                    //   APRS_ITEM
                    //   APRS_DF (looks like I didn't use this one when I implemented DF objects)

                    //   Whether area, df, signpost.
                    // Check ->signpost for signpost data.  Check ->df_color also.

                    // call_sign, sec_heard, coord_lon, coord_lat, packet_time, origin,
                    // aprs_symbol, pos_time, altitude, speed, course, bearing, NRQ,
                    // power_gain, signal_gain, signpost, station_time, station_time_type,
                    // comments, df_color
                    if (Create_object_item_tx_string(p_station, line, sizeof(line)) ) {
//fprintf(stderr,"Transmitting: %s\n",line);
                        // Attempt to transmit the object/item again
                        if (object_tx_disable) {
                            output_my_data(line,-1,0,1,0,NULL);    // Local loopback only, not igating
                        }
                        else {
                            output_my_data(line,-1,0,0,0,NULL);    // Transmit/loopback object data, not igating
                        }
                    }
                    else {
//fprintf(stderr,"Create_object_item_tx_string returned a 0\n");
                        // Don't transmit it.
                    }
                }
                else {  // Not time to transmit it yet
//fprintf(stderr,"Not time to TX yet: %s\t%s\t",p_station->call_sign,p_station->origin);
//fprintf(stderr, "%ld secs to go\n", p_station->last_transmit_time + increment - sec_now() );
                }
            }
        }
        p_station = p_station->n_next;  // Get next station
    }
//fprintf(stderr,"Exiting check_and_transmit_objects_items\n");
}


