/*
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

#include "config.h"
#include "snprintf.h"

#include <Xm/XmAll.h>
#include <X11/Xatom.h>
#include <X11/Shell.h>

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>

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

#include "xastir.h"
#include "main.h"
#include "messages.h"
#include "util.h"
#include "interface.h"


char group_data_file[400];
char *group_data_list = NULL;   // Need this NULL for Solaris!
int  group_data_count = 0;
int  group_data_max = 0;

char message_counter[5+1];

int auto_reply;
char auto_reply_message[100];

Message_Window mw[MAX_MESSAGE_WINDOWS+1];   // Send Message widgets

Message_transmit message_pool[MAX_OUTGOING_MESSAGES+1]; // Transmit message queue





void clear_message_windows(void) {
    int i;

begin_critical_section(&send_message_dialog_lock, "messages.c:clear_message_windows" );

    for (i = 0;  i < MAX_MESSAGE_WINDOWS; i++) {

        if (mw[i].send_message_dialog)
            XtDestroyWidget(mw[i].send_message_dialog);

        mw[i].send_message_dialog = (Widget)NULL;
        mw[i].to_call_sign[0] = '\0';
        mw[i].send_message_call_data = (Widget)NULL;
        mw[i].send_message_message_data = (Widget)NULL;
        mw[i].send_message_text = (Widget)NULL;
    }

end_critical_section(&send_message_dialog_lock, "messages.c:clear_message_windows" );

}





static int group_comp(const void *a, const void *b) {
    if (!*(char *)a)
        return ((int)(*(char *)b != '\0'));
    return strcasecmp(a, b);
}





void group_build_list(char *filename) {
    char *ptr;
    FILE *f;
    struct stat group_stat;
    int i;

    if (group_data_count == group_data_max) {
        ptr = realloc(group_data_list, (size_t)(group_data_max+10)*10);

        if (ptr) {
            group_data_list = ptr;
            group_data_max += 10;

//fprintf(stderr,                "group_data_max: %d\n", group_data_max);

        }
        else {
            fprintf(stderr,
                "Unable to allocate more memory for group_data_list (1)\n");
        }
    }


// Make sure we always listen for ourself, XASTIR, & our Version groups
    xastir_snprintf(&group_data_list[0],10,"%s",my_callsign);
    xastir_snprintf(&group_data_list[10],10,"XASTIR");
    xastir_snprintf(&group_data_list[20],10,"%s",XASTIR_TOCALL);
    group_data_count = 3;
// If we are in special group look for messages.
    if (altnet) {
        xastir_snprintf(&group_data_list[group_data_count*10],10,"%s",altnet_call);
        group_data_count++;
    }
//

    if (! stat(filename, &group_stat) )
        f = fopen(filename, "r");   // File exists
    else
        f = fopen(filename, "w+");  // No file.  Create it and open it.

    if (f == NULL) {
        fprintf(stderr,"Couldn't open file for reading -or- appending: %s\n", filename);
        return;
    }

    while (!feof(f)) {
        if (group_data_count == group_data_max) {
            ptr = realloc(group_data_list, (size_t)(group_data_max+10)*10);
            if (ptr) {
                group_data_list = ptr;
                group_data_max += 10;

//fprintf(stderr,                "group_data_max(2): %d\n", group_data_max);

            }
            else {
                fprintf(stderr,
                    "Unable to allocate more memory for group_data_list (2)\n");
            }
        }
        if (group_data_count < group_data_max) {
            group_data_list[group_data_count*10] = '\0';
            (void)fgets(&group_data_list[group_data_count*10], 10, f);
            if ((ptr = strchr(&group_data_list[group_data_count*10], '\n')))
                *ptr = '\0';
            else
                while ((i = fgetc(f)) != EOF && i != '\n'); // clean-up after long group name

            if (group_data_list[group_data_count*10])
                group_data_count++;
        }
    }
    (void)fclose(f);
    qsort(group_data_list, (size_t)group_data_count, 10, group_comp);
    if (debug_level & 2) for (i = 0; i < group_data_count; i++) printf("Group %2d: %s\n", i, &group_data_list[i*10]);
}





static int group_active(char *from) {
    static struct stat current_group_stat;
    struct stat group_stat;
    static char altgroup[10];

    (void)remove_trailing_spaces(from);

    // If we cycle to/from special group or file changes, rebuild group list.
    if ((!stat(group_data_file, &group_stat)
            && (current_group_stat.st_size != group_stat.st_size
                || current_group_stat.st_mtime != group_stat.st_mtime
                || current_group_stat.st_ctime != group_stat.st_ctime))
                || (altgroup && strcasecmp(altgroup, VERSIONFRM))) {
        group_build_list(group_data_file);
        current_group_stat = group_stat;
        xastir_snprintf(altgroup,sizeof(altgroup),"%s",VERSIONFRM);
    }
    if (group_data_list != NULL)    // Causes segfault on Solaris 2.5 without this!
        return (int)(bsearch(from, group_data_list, (size_t)group_data_count, (size_t)10, group_comp) != NULL);
    else
        return(0);
}





int look_for_open_group_data(char *to) {
    int i,found;
    char temp1[MAX_CALLSIGN+1];
    char *temp_ptr;


begin_critical_section(&send_message_dialog_lock, "messages.c:look_for_open_group_data" );

    found = FALSE;
     for(i = 0; i < MAX_MESSAGE_WINDOWS; i++) {
        /* find station  */
        if(mw[i].send_message_dialog != NULL) {

            temp_ptr = XmTextFieldGetString(mw[i].send_message_call_data);
            xastir_snprintf(temp1,
                sizeof(temp1),
                "%s",
                temp_ptr);
            XtFree(temp_ptr);

            (void)to_upper(temp1);
            /*fprintf(stderr,"Looking at call <%s> for <%s>\n",temp1,to);*/
            if(strcmp(temp1,to)==0) {
                found=(int)TRUE;
                break;
            }
        }
    }

end_critical_section(&send_message_dialog_lock, "messages.c:look_for_open_group_data" );

    return(found);
}





int check_popup_window(char *from_call_sign, int group) {
    int i,found,j,ret;
    char temp1[MAX_CALLSIGN+1];
    char *temp_ptr;


    ret =- 1;
    found =- 1;

begin_critical_section(&send_message_dialog_lock, "messages.c:check_popup_window" );

    for (i = 0; i < MAX_MESSAGE_WINDOWS; i++) {
        /* find station  */
        if (mw[i].send_message_dialog != NULL) {

            temp_ptr = XmTextFieldGetString(mw[i].send_message_call_data);
            xastir_snprintf(temp1,
                sizeof(temp1),
                "%s",
                temp_ptr);
            XtFree(temp_ptr);

            /*fprintf(stderr,"Looking at call <%s> for <%s>\n",temp1,from_call_sign);*/
            if (strcasecmp(temp1, from_call_sign) == 0) {
                found = i;
                break;
            }
        }
    }

end_critical_section(&send_message_dialog_lock, "messages.c:check_popup_window" );

    if (found == -1 && (group == 2 || group_active(from_call_sign))) {
        /* no window found Open one! */

begin_critical_section(&send_message_dialog_lock, "messages.c:check_popup_window2" );

        i=-1;
        for (j=0; j<MAX_MESSAGE_WINDOWS; j++) {
            if (!mw[j].send_message_dialog) {
                i=j;
                break;
            }
        }

end_critical_section(&send_message_dialog_lock, "messages.c:check_popup_window2" );

        if (i!=-1) {
        if (group == 1) {
        temp1[0] = '*';
        temp1[1] = '\0';
        } else
            temp1[0] = '\0';
            strncat(temp1,
                from_call_sign,
                sizeof(temp1) - strlen(temp1));
            Send_message(appshell, temp1, NULL);
            update_messages(1);
            ret=i;
        } else
            fprintf(stderr,"No open windows!\n");
    } else
        /* window open! */
        ret=found;

    return(ret);
}





void clear_outgoing_message(int i) {
    message_pool[i].active=MESSAGE_CLEAR;
    message_pool[i].to_call_sign[0] = '\0';
    message_pool[i].from_call_sign[0] = '\0';
    message_pool[i].message_line[0] = '\0';
    message_pool[i].seq[0] = '\0';
    message_pool[i].active_time=0;;
    message_pool[i].next_time=0l;
    message_pool[i].tries=0;
}





// Clear all pending transmit messages that are from us and to the
// callsign listed.  Perhaps it'd be better to time it out instead
// so that it still shows up in the message window?  Here we just
// erase it.
//
void clear_outgoing_messages_to(char *callsign) {
    int ii;


//    fprintf(stderr,"Callsign: %s\n", callsign);

    // Run through the entire outgoing message queue
    for (ii = 0; ii < MAX_OUTGOING_MESSAGES; ii++) {

        // If it matches the callsign we're talking to
        if (strcasecmp(message_pool[ii].to_call_sign,callsign) == 0) {

            // Clear it out.
            message_pool[ii].active=MESSAGE_CLEAR;
            message_pool[ii].to_call_sign[0] = '\0';
            message_pool[ii].from_call_sign[0] = '\0';
            message_pool[ii].message_line[0] = '\0';
            message_pool[ii].seq[0] = '\0';
            message_pool[ii].active_time=0;;
            message_pool[ii].next_time=0l;
            message_pool[ii].tries=0;
        }
    }
}





void reset_outgoing_messages(void) {
    int i;

    for(i=0;i<MAX_OUTGOING_MESSAGES;i++)
        clear_outgoing_message(i);
}





void clear_outgoing_messages(void) {
    int i;

    for (i=0;i<MAX_OUTGOING_MESSAGES;i++)
        clear_outgoing_message(i);

begin_critical_section(&send_message_dialog_lock, "messages.c:clear_outgoing_messages" );

    /* clear message send buttons */
    for (i=0;i<MAX_MESSAGE_WINDOWS;i++) {
        /* find station  */
//        if (mw[i].send_message_dialog!=NULL) /* clear submit */
//            XtSetSensitive(mw[i].button_ok,TRUE);
    }

end_critical_section(&send_message_dialog_lock, "messages.c:clear_outgoing_messages" );

}





// Adds a message to the outgoing message queue.  Doesn't actually
// cause a transmit.  "check_and_transmit_messages()" is the
// function which actually gets things moving.
void output_message(char *from, char *to, char *message, char *path) {
    int ok,i,j;
    char message_out[MAX_MESSAGE_OUTPUT_LENGTH+1];
    int last_space, message_ptr, space_loc;
    int wait_on_first_ack;
    int error;

    message_ptr=0;
    last_space=0;
    ok=0;
    error=0;
    if (debug_level & 2)
        fprintf(stderr,"Output Message from <%s>  to <%s>\n",from,to);

    while (!error && (message_ptr < (int)strlen(message))) {
        ok=0;
        space_loc=0;
        for (j=0;j<MAX_MESSAGE_OUTPUT_LENGTH;j++) {
            if(message[j+message_ptr] != '\0') {
                if(message[j+message_ptr]==' ') {
                    last_space=j+message_ptr+1;
                    space_loc=j;
                }
                if ((j+1)!=MAX_MESSAGE_OUTPUT_LENGTH) {
                    message_out[j]=message[j+message_ptr];
                    message_out[j+1] = '\0';
                } else {
                    if(space_loc!=0)
                        message_out[space_loc] = '\0';
                    else
                        last_space=j+message_ptr;
                }
            } else {
                j=MAX_MESSAGE_OUTPUT_LENGTH+1;
                last_space=strlen(message)+1;
            }
        }
        if (debug_level & 2)
            fprintf(stderr,"MESSAGE <%s> %d %d\n",message_out,message_ptr,last_space);

        message_ptr=last_space;
        /* check for others in the queue */
        wait_on_first_ack=0;
        for (i=0; i<MAX_OUTGOING_MESSAGES; i++) {
            if (message_pool[i].active == MESSAGE_ACTIVE
                    && strcmp(to, message_pool[i].to_call_sign) == 0
                    && strcmp(from, "***") != 0) {
                wait_on_first_ack=1;
                i=MAX_OUTGOING_MESSAGES+1;  // Done with loop
            }
        }
        for (i=0; i<MAX_OUTGOING_MESSAGES && !ok ;i++) {
            /* Check for clear position*/
            if (message_pool[i].active==MESSAGE_CLEAR) {
                /* found a spot */
                ok=1;

                // Roll over message_counter if we hit the max.  Now
                // with Reply/Ack protocol the max is only two
                // characters worth.  We changed to sending the
                // sequence number in Base-?? format in order to get
                // more range from the 2-character variable.

                message_counter[2] = '\0';  // Terminate at 2 chars

                // Increment the least significant digit
                message_counter[1]++;

                // Span the gaps between the correct ranges
                if (message_counter[1] == ':')
                    message_counter[1] = 'A';

                if (message_counter[1] == '[')
                    message_counter[1] = 'a';

                if (message_counter[1] == '{') {
                    message_counter[1] = '0';
                    message_counter[0]++;   // Roll over to next char
                }

                // Span the gaps between the correct ranges
                if (message_counter[0] == ':')
                    message_counter[0] = 'A';

                if (message_counter[0] == '[')
                    message_counter[0] = 'a';

                if (message_counter[0] == '{') {
                    message_counter[0] = '0';
                    message_counter[0]++;
                }


// Note that Xastir's messaging can lock up if we do a rollover and
// have unacked messages on each side of the rollover.  This is due
// to the logic in db.c that looks for the lowest numbered unacked
// message.  We get stuck on both sides of the fence at once.  To
// avoid this condition we could reduce the compare number (8100) to
// a smaller value, and only roll over when there are no unacked
// messages?  Another way to do it would be to write a "0" to the
// config file if we're more than 1000 when we quit Xastir?  That
// would probably be easier.  It's still possible to get to 8100
// messages during one runtime though.  Unlikely, but possible.

                message_pool[i].active = MESSAGE_ACTIVE;
                message_pool[i].wait_on_first_ack = wait_on_first_ack;
                xastir_snprintf(message_pool[i].to_call_sign,
                    sizeof(message_pool[i].to_call_sign),
                    "%s",
                    to);
                xastir_snprintf(message_pool[i].from_call_sign,
                    sizeof(message_pool[i].from_call_sign),
                    "%s",
                    from);
                xastir_snprintf(message_pool[i].message_line,
                    sizeof(message_pool[i].message_line),
                    "%s",
                    message_out);

                if (path != NULL)
                    xastir_snprintf(message_pool[i].path,
                        sizeof(message_pool[i].path),
                        "%s",
                        path);
                else
                    message_pool[i].path[0] = '\0';

//                // We compute the base-90 sequence number here
//                // This allows it to range from "!!" to "zz"
//                xastir_snprintf(message_pool[i].seq,
//                    sizeof(message_pool[i].seq),
//                    "%c%c",
//                    (char)(((message_counter / 90) % 90) + 33),
//                    (char)((message_counter % 90) + 33));

                xastir_snprintf(message_pool[i].seq,
                    sizeof(message_pool[i].seq),
                    "%c%c",
                    message_counter[0],
                    message_counter[1]);

                message_pool[i].active_time=0;
                message_pool[i].next_time = (time_t)7l;

                if (strcmp(from,"***")!= 0)
                    message_pool[i].tries = 0;
                else
                    message_pool[i].tries = MAX_TRIES-1;
            }
        }
        if(!ok) {
            fprintf(stderr,"Output message queue is full!\n");
            error=1;
        }
    }
}





void transmit_message_data(char *to, char *message, char *path) {
    DataRow *p_station;

    if (debug_level & 2)
        fprintf(stderr,"Transmitting data to %s : %s\n",to,message);

    p_station = NULL;
    if (search_station_name(&p_station,to,1)) {
        if (debug_level & 2)
            fprintf(stderr,"found station %s\n",p_station->call_sign);

        if (strcmp(to,my_callsign)!=0) {
            /* check to see if it was heard via a TNC otherwise send it to the last port heard */
            if ((p_station->flag & ST_VIATNC) != 0) {        // heard via TNC ?
                output_my_data(message,p_station->heard_via_tnc_port,0,0,0,path);
                /* station heard via tnc but in the past hour? */
                /* if not heard with in the hour try via net */
                if (!heard_via_tnc_in_past_hour(to)) {
                    if (p_station->data_via==DATA_VIA_NET) {
                        /* try last port heard */
                        output_my_data(message,p_station->last_port_heard,0,0,0,path);
                    }
                }
            } else {
                /* if not a TNC then a NET port? */
                if (p_station->data_via==DATA_VIA_NET) {
                    /* try last port herd */
                    output_my_data(message,p_station->last_port_heard,0,0,0,path);
                } else {
                    /* Not a TNC or a NET try all possible */
                    if (debug_level & 2)
                        fprintf(stderr,"VIA any way\n");

                    output_my_data(message,-1,0,0,0,path);
                }
            }
        } else {
            /* my station message */
            /* try all possible */
            if (debug_level & 2)
                fprintf(stderr,"My call VIA any way\n");

            output_my_data(message,-1,0,0,0,path);
        }
    } else {
        /* no data found try every way*/
        /* try all possible */
        if (debug_level & 2)
            fprintf(stderr,"VIA any way\n");

        output_my_data(message,-1,0,0,0,path);
    }
}





time_t last_check_and_transmit = (time_t)0l;

void check_and_transmit_messages(time_t time) {
    int i;
    char temp[200];
    char to_call[40];


    // Skip this function if we did it during this second already.
    if (last_check_and_transmit == sec_now()) {
        return;
    }
    last_check_and_transmit = sec_now();

    for (i=0; i<MAX_OUTGOING_MESSAGES;i++) {
        if (message_pool[i].active==MESSAGE_ACTIVE) {
            if (message_pool[i].wait_on_first_ack!=1) {
                if (message_pool[i].active_time < time) {
                    char *last_ack_ptr;
                    char last_ack[5+1];

                    /* sending message let the tnc and net transmits check to see if we should */
                    if (debug_level & 2)
                        fprintf(stderr,"Time %ld Active time %ld next time %ld\n",(long)time,(long)message_pool[i].active_time,(long)message_pool[i].next_time);

                    if (debug_level & 2)
                        fprintf(stderr,"Send message#%d to <%s> from <%s>:%s-%s\n",
                            message_pool[i].tries,message_pool[i].to_call_sign,message_pool[i].from_call_sign,message_pool[i].message_line,message_pool[i].seq);

                    pad_callsign(to_call,message_pool[i].to_call_sign);

                    // Add Leading ":" as per APRS Spec.
                    // Add trailing '}' to signify that we're
                    // Reply/Ack protocol capable.
                    last_ack_ptr = get_most_recent_ack(to_call);
                    if (last_ack_ptr != NULL)
                        xastir_snprintf(last_ack,
                            sizeof(last_ack),
                            "%s",
                            last_ack_ptr);
                    else
                        last_ack[0] = '\0';
                        
                    xastir_snprintf(temp, sizeof(temp), ":%s:%s{%s}%s",
                            to_call,
                            message_pool[i].message_line,
                            message_pool[i].seq,
                            last_ack);

                    if (debug_level & 2)
                        fprintf(stderr,"MESSAGE OUT>%s<\n",temp);

                    transmit_message_data(message_pool[i].to_call_sign,temp,message_pool[i].path);

                    message_pool[i].active_time = time + message_pool[i].next_time;

                    //fprintf(stderr,"%d\n",(int)message_pool[i].next_time);

                    // Start at 7 seconds for the interval.  Add 5 seconds to the
                    // interval each retry until we hit 90 seconds.  90 second
                    // intervals until retry 30, then start adding 30 seconds to
                    // the interval each time until we get to a 10 minute interval
                    // rate.  Keep transmitting at 10 minute intervals until we
                    // hit MAX_TRIES.

                    // Increase interval by 5 seconds each time
                    // until we hit 90 seconds
                    if ( message_pool[i].next_time < (time_t)90l )
                         message_pool[i].next_time += 5;

                    // Increase the interval by 30 seconds each time
                    // after we hit 30 retries
                    if ( message_pool[i].tries >= 30 )
                        message_pool[i].next_time += 30;

                    // Limit the max interval to 10 minutes
                    if (message_pool[i].next_time > (time_t)600l)
                        message_pool[i].next_time = (time_t)600l;

                    message_pool[i].tries++;

                    // Expire it if we hit the limit
                    if (message_pool[i].tries > MAX_TRIES) {
                        char temp[150];
                        char temp_to[20];

                        xastir_snprintf(temp,sizeof(temp),"To: %s, Msg: %s",
                            message_pool[i].to_call_sign,
                            message_pool[i].message_line);
                        //popup_message(langcode("POPEM00004"),langcode("POPEM00017"));
                        popup_message( "Retries Exceeded!", temp );

                        // Fake the system out: We're pretending
                        // that we got an ACK back from it so that
                        // we can either release the next message to
                        // go out, or at least make the send button
                        // sensitive again.
                        // We need to copy the to_call_sign into
                        // another variable because the
                        // clear_acked_message() function clears out
                        // the message then needs this parameter to
                        // do another compare (to enable the Send Msg
                        // button again).
                        xastir_snprintf(temp_to,
                            sizeof(temp_to),
                            "%s",
                            message_pool[i].to_call_sign);

                        // Record a fake ack and add "TIMEOUT:" to
                        // the message.  This will be displayed in
                        // the Send Message dialog.
                        msg_record_ack(temp_to,
                            message_pool[i].from_call_sign,
                            message_pool[i].seq,
                            1); // "1" specifies a timeout

                        clear_acked_message(temp_to,
                            message_pool[i].from_call_sign,
                            message_pool[i].seq);

//                        if (mw[i].send_message_dialog!=NULL) /* clear submit */
//                            XtSetSensitive(mw[i].button_ok,TRUE);
                    }
                }
            } else {
                if (debug_level & 2)
                    fprintf(stderr,"Message #%s is waiting to have a previous one cleared\n",message_pool[i].seq);
            }
        }
    }
}





// Function which marks a message as ack'ed in the transmit queue
// and releases the next message to allow it to be transmitted.
void clear_acked_message(char *from, char *to, char *seq) {
    int i,ii;
    int found;
    char lowest[3];
    char temp1[MAX_CALLSIGN+1];
    char *temp_ptr;


    (void)remove_trailing_spaces(seq);  // This is IMPORTANT here!!!

    //lowest=100000;
    // Highest Base-90 2-char string
    xastir_snprintf(lowest,sizeof(lowest),"zz");
    found=-1;
    for (i=0; i<MAX_OUTGOING_MESSAGES;i++) {
        if (message_pool[i].active==MESSAGE_ACTIVE) {

            if (debug_level & 1)
                fprintf(stderr,"TO <%s> <%s> from <%s> <%s> seq <%s> <%s>\n",to,message_pool[i].to_call_sign,from,message_pool[i].from_call_sign,seq,message_pool[i].seq);

            if (strcmp(message_pool[i].to_call_sign,from)==0) {
                if (debug_level & 1)
                    fprintf(stderr,"Matched message to_call_sign\n");
                if (strcmp(message_pool[i].from_call_sign,to)==0) {
                    if (debug_level & 1)
                        fprintf(stderr,"Matched message from_call_sign\n");
                    if (strcmp(message_pool[i].seq,seq)==0) {
                        if (debug_level & 2)
                            fprintf(stderr,"Found and cleared\n");

                        clear_outgoing_message(i);
                        // now find and release next message, look for the lowest sequence?
// What about when the sequence rolls over?
                        for (i=0; i<MAX_OUTGOING_MESSAGES;i++) {
                            if (message_pool[i].active==MESSAGE_ACTIVE) {
                                if (strcmp(message_pool[i].to_call_sign,from)==0) {
// Need to change this to a string compare instead of an integer
// compare.  We are using base-90 encoding now.
                                    //if (atoi(message_pool[i].seq)<lowest) {
                                    if (strncmp(message_pool[i].seq,lowest,2) < 0) {
                                        //lowest=atoi(message_pool[i].seq);
                                        xastir_snprintf(lowest,
                                            sizeof(lowest),
                                            "%s",
                                            message_pool[i].seq);
                                        found=i;
                                    }
                                }
                            }
                        }
                        // Release the next message in the queue for transmission
                        if (found!=-1) {
                            message_pool[found].wait_on_first_ack=0;
                        }
                        else {
                            /* if no more clear the send button */

begin_critical_section(&send_message_dialog_lock, "messages.c:clear_acked_message" );

                            for (ii=0;ii<MAX_MESSAGE_WINDOWS;ii++) {
                                /* find station  */
                                if (mw[ii].send_message_dialog!=NULL) {

                                    temp_ptr = XmTextFieldGetString(mw[ii].send_message_call_data);
                                    xastir_snprintf(temp1,
                                        sizeof(temp1),
                                        "%s",
                                        temp_ptr);
                                    XtFree(temp_ptr);

                                    (void)to_upper(temp1);
                                    //fprintf(stderr,"%s\t%s\n",temp1,from);
//                                    if (strcmp(temp1,from)==0) {
                                        /*clear submit*/
//                                        XtSetSensitive(mw[ii].button_ok,TRUE);
//                                    }
                                }
                            }

end_critical_section(&send_message_dialog_lock, "messages.c:clear_acked_message" );

                        }
                    }
                    else {
                        if (debug_level & 1)
                            fprintf(stderr,"Sequences didn't match: %s %s\n",message_pool[i].seq,seq);
                    }
                }
            }
        }
    }
}





// This routine is not currently used.
//
void send_queued(char *to) {
    int i;

    for (i=0; i<MAX_OUTGOING_MESSAGES ;i++) {
        /* Check for messages to call */
        if (message_pool[i].active==MESSAGE_ACTIVE)
            if (strcmp(to,message_pool[i].to_call_sign)==0)
                message_pool[i].active_time=0;

    }
}


