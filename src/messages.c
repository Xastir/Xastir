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

#include <Xm/XmAll.h>
#include <X11/Xatom.h>
#include <X11/Shell.h>

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_STRING_H
  #include <string.h>
#else
  #include <strings.h>
#endif
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>

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

#include "xastir.h"
#include "main.h"
#include "messages.h"
#include "util.h"
#include "interface.h"
#include "xa_config.h"

// Must be last include file
#include "leak_detection.h"



char group_data_file[400];
char *group_data_list = NULL;   // Need this NULL for Solaris!
int  group_data_count = 0;
int  group_data_max = 0;

char message_counter[5+1];

int auto_reply;
char auto_reply_message[100];

Message_Window mw[MAX_MESSAGE_WINDOWS+1];   // Send Message widgets

Message_transmit message_pool[MAX_OUTGOING_MESSAGES+1]; // Transmit message queue





void clear_message_windows(void)
{
  int i;

  begin_critical_section(&send_message_dialog_lock, "messages.c:clear_message_windows" );

  for (i = 0;  i < MAX_MESSAGE_WINDOWS; i++)
  {

    if (mw[i].send_message_dialog)
    {
      XtDestroyWidget(mw[i].send_message_dialog);
    }

    mw[i].send_message_dialog = (Widget)NULL;
    mw[i].to_call_sign[0] = '\0';
    mw[i].send_message_call_data = (Widget)NULL;
    mw[i].D700_mode = (Widget)NULL;
    mw[i].D7_mode = (Widget)NULL;
    mw[i].HamHUD_mode = (Widget)NULL;
    mw[i].message_data_line1 = (Widget)NULL;
    mw[i].message_data_line2 = (Widget)NULL;
    mw[i].message_data_line3 = (Widget)NULL;
    mw[i].message_data_line4 = (Widget)NULL;
    mw[i].send_message_text = (Widget)NULL;
  }

  end_critical_section(&send_message_dialog_lock, "messages.c:clear_message_windows" );

}





static int group_comp(const void *a, const void *b)
{
  if (!*(char *)a)
  {
    return ((int)(*(char *)b != '\0'));
  }
  return strcasecmp(a, b);
}





void group_build_list(char *filename)
{
  char *ptr;
  FILE *f;
  struct stat group_stat;
  int i;

  if (group_data_count == group_data_max)
  {
    ptr = realloc(group_data_list, (size_t)(group_data_max+10)*10);

    if (ptr)
    {
      group_data_list = ptr;
      group_data_max += 10;

//fprintf(stderr,                "group_data_max: %d\n", group_data_max);

    }
    else
    {
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
  if (altnet)
  {
    xastir_snprintf(&group_data_list[group_data_count*10],10,"%s",altnet_call);
    group_data_count++;
  }
//

  if (! stat(filename, &group_stat) )
  {
    f = fopen(filename, "r");  // File exists
  }
  else
  {
    f = fopen(filename, "w+");  // No file.  Create it and open it.
  }

  if (f == NULL)
  {
    fprintf(stderr,"Couldn't open file for reading -or- appending: %s\n", filename);
    return;
  }

  while (!feof(f))
  {
    if (group_data_count == group_data_max)
    {
      ptr = realloc(group_data_list, (size_t)(group_data_max+10)*10);
      if (ptr)
      {
        group_data_list = ptr;
        group_data_max += 10;

//fprintf(stderr,                "group_data_max(2): %d\n", group_data_max);

      }
      else
      {
        fprintf(stderr,
                "Unable to allocate more memory for group_data_list (2)\n");
      }
    }
    if (group_data_count < group_data_max)
    {
      group_data_list[group_data_count*10] = '\0';
      if (fgets(&group_data_list[group_data_count*10], 10, f) == NULL)
      {
        // Error reading file or end-of-file. Continue processing
        // the (partial?) string we just read, in the code below.
      }
      if ((ptr = strchr(&group_data_list[group_data_count*10], '\n')))
      {
        *ptr = '\0';
      }
      else
        while ((i = fgetc(f)) != EOF && i != '\n'); // clean-up after long group name

      // check for DOS EOL markup!
      if ((ptr = strchr(&group_data_list[group_data_count*10], '\r')))
      {
        *ptr = '\0';
      }
      if (group_data_list[group_data_count*10])
      {
        group_data_count++;
      }
    }
  }
  (void)fclose(f);
  qsort(group_data_list, (size_t)group_data_count, 10, group_comp);

  if (debug_level & 2)
  {
    for (i = 0; i < group_data_count; i++)
    {
      fprintf(stderr,"Group %2d: %s\n", i, &group_data_list[i*10]);
    }
  }
}





int group_active(char *from)
{
  static struct stat current_group_stat;
  struct stat group_stat;
  static char altgroup[10];
  char group_data_path[MAX_VALUE];

  get_user_base_dir(group_data_file, group_data_path,
                    sizeof(group_data_path));

  (void)remove_trailing_spaces(from);

  // If we cycle to/from special group or file changes, rebuild group list.
  if ((!stat( group_data_path, &group_stat )
       && (current_group_stat.st_size != group_stat.st_size
           || current_group_stat.st_mtime != group_stat.st_mtime
           || current_group_stat.st_ctime != group_stat.st_ctime)))
  {

// altgroup equates to "address of altgroup" which always evaluates
// as true.  Commenting it out of the conditional.  --we7u.
//                || (altgroup && strcasecmp(altgroup, VERSIONFRM))) {

    group_build_list( group_data_path );
    current_group_stat = group_stat;
    xastir_snprintf(altgroup,sizeof(altgroup),"%s",VERSIONFRM);
  }
  if (group_data_list != NULL)    // Causes segfault on Solaris 2.5 without this!
  {
    return (int)(bsearch(from, group_data_list, (size_t)group_data_count, (size_t)10, group_comp) != NULL);
  }
  else
  {
    return(0);
  }
}





int look_for_open_group_data(char *to)
{
  int i,found;
  char temp1[MAX_CALLSIGN+1];
  char *temp_ptr;


  begin_critical_section(&send_message_dialog_lock, "messages.c:look_for_open_group_data" );

  found = FALSE;
  for(i = 0; i < MAX_MESSAGE_WINDOWS; i++)
  {
    /* find station  */
    if(mw[i].send_message_dialog != NULL)
    {

      temp_ptr = XmTextFieldGetString(mw[i].send_message_call_data);
      xastir_snprintf(temp1,
                      sizeof(temp1),
                      "%s",
                      temp_ptr);
      XtFree(temp_ptr);

      (void)to_upper(temp1);
      /*fprintf(stderr,"Looking at call <%s> for <%s>\n",temp1,to);*/
      if(strcmp(temp1,to)==0)
      {
        found=(int)TRUE;
        break;
      }
    }
  }

  end_critical_section(&send_message_dialog_lock, "messages.c:look_for_open_group_data" );

  return(found);
}





// What we wish to do here:  Check for an active Send Message dialog
// that contains the callsign of interest.  If one doesn't exist,
// create one and pop it up.  We don't want to do this for duplicate
// message lines or duplicate acks for any particular QSO.  To do so
// would cause the Send Message dialog to pop up on every such
// received message, which is VERY annoying (that was the default in
// Xastir for years, and nobody liked it!).
//
int check_popup_window(char *from_call_sign, int group)
{
  int i,found,j,ret;
  char temp1[MAX_CALLSIGN+1];
  char *temp_ptr;


//fprintf(stderr,"\tcheck_popup_window()\n");

  ret = -1;
  found = -1;

  begin_critical_section(&send_message_dialog_lock, "messages.c:check_popup_window" );

  // Check for an already-created dialog for talking to this
  // particular call_sign.
  //
  for (i = 0; i < MAX_MESSAGE_WINDOWS; i++)
  {

    if (mw[i].send_message_dialog != NULL)   // If dialog created
    {

      temp_ptr = XmTextFieldGetString(mw[i].send_message_call_data);
      xastir_snprintf(temp1,
                      sizeof(temp1),
                      "%s",
                      temp_ptr);
      XtFree(temp_ptr);

      /*fprintf(stderr,"Looking at call <%s> for <%s>\n",temp1,from_call_sign);*/
      if (strcasecmp(temp1, from_call_sign) == 0)
      {
        // Found a call_sign match in a Send Message dialog!

//fprintf(stderr,"\tFound a Send_message dialog match\n");

        found = i;
        break;
      }
    }
  }

  end_critical_section(&send_message_dialog_lock, "messages.c:check_popup_window" );

  // If found == -1 at this point, we haven't found a Send Message
  // dialog that contains the call_sign of interest.
  //
  if (found == -1 && (group == 2 || group_active(from_call_sign)))
  {
    /* no window found Open one! */

//fprintf(stderr,"\tNo Send Message dialog found, creating one\n");

    begin_critical_section(&send_message_dialog_lock, "messages.c:check_popup_window2" );

    i= -1;
    for (j=0; j<MAX_MESSAGE_WINDOWS; j++)
    {
      if (!mw[j].send_message_dialog)
      {
        i=j;
        break;
      }
    }

    end_critical_section(&send_message_dialog_lock, "messages.c:check_popup_window2" );

    if (i!= -1)
    {

      if (group == 1)
      {
        temp1[0] = '*';
        temp1[1] = '\0';
      }
      else
      {
        temp1[0] = '\0';
      }

      strncat(temp1,
              from_call_sign,
              sizeof(temp1) - 1 - strlen(temp1));

      if (!disable_all_popups)
      {
        Send_message(appshell, temp1, NULL);
      }

      update_messages(1);

      ret=i;
    }
    else
    {
      fprintf(stderr,"No open windows!\n");
    }
  }
  else
  {
    /* window open! */
    // Pop it up
    ret=found;
  }

  if (found != -1)    // Already have a window
  {
    XtPopup(mw[i].send_message_dialog,XtGrabNone);
  }

  return(ret);
}





void clear_outgoing_message(int i)
{
  message_pool[i].active=MESSAGE_CLEAR;
  message_pool[i].to_call_sign[0] = '\0';
  message_pool[i].from_call_sign[0] = '\0';
  message_pool[i].message_line[0] = '\0';
  message_pool[i].seq[0] = '\0';
  message_pool[i].active_time=0;;
  message_pool[i].next_time=0L;
  message_pool[i].tries=0;
}





// Clear all pending transmit messages that are from us and to the
// callsign listed.  Perhaps it'd be better to time it out instead
// so that it still shows up in the message window?  Here we just
// erase it.
//
void clear_outgoing_messages_to(char *callsign)
{
  int ii;


//    fprintf(stderr,"Callsign: %s\n", callsign);

  // Run through the entire outgoing message queue
  for (ii = 0; ii < MAX_OUTGOING_MESSAGES; ii++)
  {

    // If it matches the callsign we're talking to
    if (strcasecmp(message_pool[ii].to_call_sign,callsign) == 0)
    {

      // Record a fake ack and add "*CANCELLED*" to the
      // message.  This will be displayed in the Send Message
      // dialog.
      msg_record_ack(message_pool[ii].to_call_sign,
                     message_pool[ii].from_call_sign,
                     message_pool[ii].seq,
                     0,  // Not a timeout
                     1); // Record a cancel

      // Clear it out.
      message_pool[ii].active=MESSAGE_CLEAR;
      message_pool[ii].to_call_sign[0] = '\0';
      message_pool[ii].from_call_sign[0] = '\0';
      message_pool[ii].message_line[0] = '\0';
      message_pool[ii].seq[0] = '\0';
      message_pool[ii].active_time=0;;
      message_pool[ii].next_time=0L;
      message_pool[ii].tries=0;
    }
  }
}





// Change path on all pending transmit messages that are from us and
// to the callsign listed.
//
void change_path_outgoing_messages_to(char *callsign, char *new_path)
{
  int ii;
  char my_callsign[20];


//fprintf(stderr,
//    "Changing all outgoing msgs to %s to new path: %s\n",
//    callsign,
//    new_path);

  xastir_snprintf(my_callsign,
                  sizeof(my_callsign),
                  "%s",
                  callsign);

  remove_trailing_spaces(my_callsign);

  // Run through the entire outgoing message queue
  for (ii = 0; ii < MAX_OUTGOING_MESSAGES; ii++)
  {

    if (message_pool[ii].active == MESSAGE_ACTIVE)
    {

//fprintf(stderr,"\t'%s'\n\t'%s'\n",
//    message_pool[ii].to_call_sign,
//    my_callsign);

      // If it matches the callsign we're talking to
      if (strcasecmp(message_pool[ii].to_call_sign,my_callsign) == 0)
      {

//fprintf(stderr,"\tFound an outgoing queued msg to change path on.\n");

        xastir_snprintf(message_pool[ii].path,
                        sizeof(message_pool[ii].path),
                        "%s",
                        new_path);
      }
    }
  }
}





time_t last_check_and_transmit = (time_t)0L;


// Kick the interval timer back to 7 and tries back to 1 for
// messages in this QSO.  Used to get a QSO going again when the
// interval timer has gotten large, but the message is important to
// get through quickly.
//
void kick_outgoing_timer(char *callsign)
{
  int ii;


//    fprintf(stderr,"Callsign: %s\n", callsign);

  // Run through the entire outgoing message queue
  for (ii = 0; ii < MAX_OUTGOING_MESSAGES; ii++)
  {

    // If it matches the callsign we're talking to
    if (strcasecmp(message_pool[ii].to_call_sign,callsign) == 0)
    {
      message_pool[ii].next_time = (time_t)7L;
      message_pool[ii].tries = 0;
      message_pool[ii].active_time = (time_t)0L;
    }
  }

  // Cause the transmit routine to get called again
  last_check_and_transmit = (time_t)0L;
}





void reset_outgoing_messages(void)
{
  int i;

  for(i=0; i<MAX_OUTGOING_MESSAGES; i++)
  {
    clear_outgoing_message(i);
  }
}





void clear_outgoing_messages(void)
{
  int i;

  for (i=0; i<MAX_OUTGOING_MESSAGES; i++)
  {
    clear_outgoing_message(i);
  }

  begin_critical_section(&send_message_dialog_lock, "messages.c:clear_outgoing_messages" );

  /* clear message send buttons */
  for (i=0; i<MAX_MESSAGE_WINDOWS; i++)
  {
    /* find station  */
//        if (mw[i].send_message_dialog!=NULL) /* clear submit */
//            XtSetSensitive(mw[i].button_ok,TRUE);
  }

  end_critical_section(&send_message_dialog_lock, "messages.c:clear_outgoing_messages" );

}





// Bumps message sequence ID up to the next value.
//
// Roll over message_counter if we hit the max.  Now with Reply/Ack
// protocol the max is only two characters worth.  We changed to
// sending the sequence number in Base-?? format in order to get
// more range from the 2-character variable.
//
int bump_message_counter(char *message_counter)
{

  int bump_warning = 0;
  message_counter[2] = '\0';  // Terminate at 2 chars

  // Increment the least significant digit
  message_counter[1]++;

  // Span the gaps between the correct ranges
  if (message_counter[1] == ':')
  {
    message_counter[1] = 'A';
  }

  if (message_counter[1] == '[')
  {
    message_counter[1] = 'a';
  }

  if (message_counter[1] == '{')
  {
    message_counter[1] = '0';
    message_counter[0]++;   // Roll over to next char
  }

  // Span the gaps between the correct ranges
  if (message_counter[0] == ':')
  {
    message_counter[0] = 'A';
  }

  if (message_counter[0] == '[')
  {
    message_counter[0] = 'a';
  }

  if (message_counter[0] == '{')
  {
    message_counter[0] = '0';
    bump_warning = 1;
  }
  return bump_warning;
}





// Adds a message to the outgoing message queue.  Doesn't actually
// cause a transmit.  "check_and_transmit_messages()" is the
// function which actually gets things moving.
//
// We also stuff the message into the main message queue so that the
// queued messages will appear in the Send Message box.
//
void output_message(char *from, char *to, char *message, char *path)
{
  int ok,i,j;
  char message_out[MAX_MESSAGE_OUTPUT_LENGTH+1+5+1]; // +'{' +msg_id +terminator
  int last_space, message_ptr, space_loc;
  int wait_on_first_ack;
  int error;
  long record;


//fprintf(stderr,"output_message:%s\n", message);

  message_ptr=0;
  last_space=0;
  ok=0;
  error=0;

  if (debug_level & 2)
  {
    fprintf(stderr,"Output Message from <%s>  to <%s>\n",from,to);
  }

  // Repeat until we process the entire message.  We'll process it
  // a chunk at a time, size of chunk to correspond to max APRS
  // message line length.
  //
  while (!error && (message_ptr < (int)strlen(message)))
  {
    ok=0;
    space_loc=0;

    // Break a long message into smaller chunks that can be
    // processed into APRS messages.  Break at a space character
    // if possible.
    //
    for (j=0; j<MAX_MESSAGE_OUTPUT_LENGTH; j++)
    {

      if(message[j+message_ptr] != '\0')
      {

        if(message[j+message_ptr]==' ')
        {
          last_space=j+message_ptr+1;
          space_loc=j;
        }

        if (j!=MAX_MESSAGE_OUTPUT_LENGTH)
        {
          message_out[j]=message[j+message_ptr];
          message_out[j+1] = '\0';
        }
        else
        {

          if(space_loc!=0)
          {
            message_out[space_loc] = '\0';
          }
          else
          {
            last_space=j+message_ptr;
          }
        }
      }
      else
      {
        j=MAX_MESSAGE_OUTPUT_LENGTH+1;
        last_space=strlen(message)+1;
      }
    }

//fprintf(stderr,"message_out: %s\n", message_out);

    if (debug_level & 2)
    {
      fprintf(stderr,"MESSAGE <%s> %d %d\n",message_out,message_ptr,last_space);
    }

    if (j >= MAX_MESSAGE_OUTPUT_LENGTH)
    {
      message_ptr = MAX_MESSAGE_OUTPUT_LENGTH;
    }
    else
    {
      message_ptr=last_space;
    }

    /* check for others in the queue */
    wait_on_first_ack=0;
    for (i=0; i<MAX_OUTGOING_MESSAGES; i++)
    {
      if (message_pool[i].active == MESSAGE_ACTIVE
          && strcmp(to, message_pool[i].to_call_sign) == 0
          && strcmp(from, "***") != 0)
      {
        wait_on_first_ack=1;
        i=MAX_OUTGOING_MESSAGES+1;  // Done with loop
      }
    }

    for (i=0; i<MAX_OUTGOING_MESSAGES && !ok ; i++)
    {
      /* Check for clear position*/
      if (message_pool[i].active==MESSAGE_CLEAR)
      {
        /* found a spot */
        ok=1;

        // Increment the message sequence ID variable
        if (bump_message_counter(message_counter))
        {
          fprintf(stderr, "!WARNING!: Wrap around Message Counter");
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
        memcpy(message_pool[i].message_line, message_out, sizeof(message_pool[i].message_line));
        // Terminate line
        message_pool[i].message_line[sizeof(message_pool[i].message_line)-1] = '\0';

        if (path != NULL)
          xastir_snprintf(message_pool[i].path,
                          sizeof(message_pool[i].path),
                          "%s",
                          path);
        else
        {
          message_pool[i].path[0] = '\0';
        }

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
        message_pool[i].next_time = (time_t)7L;

        if (strcmp(from,"***")!= 0)
        {
          message_pool[i].tries = 0;
        }
        else
        {
          message_pool[i].tries = MAX_TRIES-1;
        }

        // Cause the message to get added to the main
        // message queue as well, with the proper sequence
        // number, so queued messages will appear in the
        // Send Message box as unacked messages.
        //

// We must get rid of the lock we already have for a moment, as
// update_messages(), which is called by msg_data_add(), also snags
// this lock.
        end_critical_section(&send_message_dialog_lock, "db.c:update_messages" );

        (void)msg_data_add(to,
                           from,
                           message_out,
                           message_pool[i].seq,
                           MESSAGE_MESSAGE,
                           'L',    // From the Local system
                           &record);
        /*
                        fprintf(stderr,"msg_data_add %s %s %s %s\n",
                            to,
                            from,
                            message_out,
                            message_pool[i].seq);
        */

// Regain the lock we had before
        begin_critical_section(&send_message_dialog_lock, "db.c:update_messages" );

      }
    }
    if(!ok)
    {
      fprintf(stderr,"Output message queue is full!\n");
      error=1;
    }
  }
}





// Here we're doing some routing of the transmitted packets.  We
// want to keep Xastir from transmitting on ports that aren't
// actively being used in the QSO, but also cover the case where
// ports can go up/down during the QSO.
//
// Note that igates might get into the act quite a bit for RF<->RF
// QSO's if we're sending to the internet too, but that's a bug in
// the igate software, and not something that Xastir should try to
// correct itself.
//
void transmit_message_data(char *to, char *message, char *path)
{
  DataRow *p_station;

  if (debug_level & 2)
  {
    fprintf(stderr,"Transmitting data to %s : %s\n",to,message);
  }

  p_station = NULL;


  if (strcmp(to, my_callsign) == 0)   // My station message
  {

    // Send out all active ports

    if (debug_level & 2)
    {
      fprintf(stderr,"My call VIA any way\n");
    }

    output_my_data(message,-1,0,0,0,path);

    // All done
    return;
  }


  if (!search_station_name(&p_station,to,1))
  {

    // No data record found for this station.  Send to all
    // active ports.

    if (debug_level & 2)
    {
      fprintf(stderr,"VIA any way\n");
    }

    output_my_data(message,-1,0,0,0,path);

    // All done
    return;
  }


  if (debug_level & 2)
  {
    fprintf(stderr,"found station %s\n",p_station->call_sign);
  }


  // It's not being sent to my callsign but to somebody else
  // "out there".  Because the truth is...


  if ( ((p_station->flag & ST_VIATNC) != 0)
       && (heard_via_tnc_in_past_hour(to)) )
  {

    int port_num;


    // Station was heard via a TNC port within the previous
    // hour.  Send to TNC port it was heard on.
    //
    output_my_data(message,p_station->heard_via_tnc_port,0,0,0,path);

    // Send to all internet ports.  Iterate through the port
    // definitions looking for internet ports, send the message
    // out once to each.
    //
    for (port_num = 0; port_num < MAX_IFACE_DEVICES; port_num++)
    {

      // If it's an internet port, send the message.
      if (port_data[port_num].device_type == DEVICE_NET_STREAM)
      {
        output_my_data(message,port_num,0,0,0,path);
      }
    }

    // All done
    return;
  }

  else if (p_station->data_via==DATA_VIA_NET)
  {
    int port_num;
    int active_internet_ports_found = 0;


    // Station was heard over an internet interface.  Check
    // whether we have any internet interfaces available with TX
    // enabled.  If so, send out those ports.  Else drop through
    // and hit the TRANSMIT-ALL clause at the end of this
    // function.

    // Iterate through the port definitions looking for internet
    // ports with transmit enabled.  Send the message out once
    // to each.
    //
    for (port_num = 0; port_num < MAX_IFACE_DEVICES; port_num++)
    {

      // If it's an internet port and transmit is enabled,
      // send the message and set the flag.
      if ( (port_data[port_num].device_type == DEVICE_NET_STREAM)
           && (port_data[port_num].active == DEVICE_IN_USE)
           && (port_data[port_num].status == DEVICE_UP)
           && (devices[port_num].transmit_data == 1) )
      {

        // Found a tx-enabled internet port that was up and
        // running.  Send the message out this port.
        output_my_data(message,port_num,0,0,0,path);

        active_internet_ports_found++;
      }
    }

    if (active_internet_ports_found)
    {
      // We found at least one tx-enabled internet interface
      // that was up and running.

      // All done.
      return;
    }
    else    // No active tx-enabled internet ports were found.
    {
      // Drop through to the TRANSMIT-ALL clause below.
    }
  }


  // We've NOT heard this station on a TNC port within the
  // last hour and have no active tx-enabled internet ports to
  // send to.  Send to ALL active ports.

  if (debug_level & 2)
  {
    fprintf(stderr,"VIA any way\n");
  }

  output_my_data(message,-1,0,0,0,path);
}





// The below variables and functions implement the capability to
// schedule ACK's some number of seconds out from the current time.
// We use it to schedule duplicate ACK's at 30/60/120 seconds out,
// but only if we see duplicate message lines from remote stations.
//
// Create a struct to hold the delayed ack's.
typedef struct _delayed_ack_record
{
  char to_call_sign[MAX_CALLSIGN+1];
  char message_line[MAX_MESSAGE_OUTPUT_LENGTH+1+5+1];
  char path[200];
  time_t active_time;
  struct _delayed_ack_record *next;
} delayed_ack_record, *delayed_ack_record_p;

// And a pointer to a list of them.
delayed_ack_record_p delayed_ack_list_head = NULL;


void transmit_message_data_delayed(char *to, char *message,
                                   char *path, time_t when)
{
  delayed_ack_record_p ptr = delayed_ack_list_head;


  // We need to run down the current list looking for any records
  // that are identical and within 30 seconds time-wise of this
  // one.  If so, don't allocate a new record.  This keeps the
  // dupes down on transmit so that at the most we transmit one
  // ack per 30 seconds per QSO, except of course for real-time
  // ack's which don't go through this function.

  // Run through the queue and check each record
  while (ptr != NULL)
  {

    if ( strcmp(ptr->to_call_sign,to) == 0
         && strcmp(ptr->message_line,message) == 0 )
    {

      //
      // We have matches on call_sign and message.  Check the
      // time next.
      //
      if (labs(when - ptr->active_time) < 30)
      {
        //
        // We're within 30 seconds of an identical ack.
        // Drop this new one (don't add it).
        //

//fprintf(stderr,"Dropping delayed ack: Too close timewise to another: %s, %s\n",
//    to, message);

        return; // Don't allocate new record on queue
      }
    }
    ptr = ptr->next;
  }

  // If we made it to here, there aren't any queued ACK's that are
  // close enough in time to drop this new one.  Add it to the
  // queue.

//fprintf(stderr, "Queuing ACK for delayed transmit: %s, %s\n",
//    to, message);

  // Allocate a record to hold it
  ptr = (delayed_ack_record_p)malloc(sizeof(delayed_ack_record));

  // Fill in the record
  xastir_snprintf(ptr->to_call_sign,
                  sizeof(ptr->to_call_sign),
                  "%s",
                  to);

  xastir_snprintf(ptr->message_line,
                  sizeof(ptr->message_line),
                  "%s",
                  message);

  if (path == NULL)
  {
    ptr->path[0] = '\0';
  }
  else
  {
    xastir_snprintf(ptr->path,
                    sizeof(ptr->path),
                    "%s",
                    path);
  }

  ptr->active_time = when;

  // Add the record to the head of the list
  ptr->next = delayed_ack_list_head;
  delayed_ack_list_head = ptr;
}





time_t delayed_transmit_last_check = (time_t)0;


void check_delayed_transmit_queue(int curr_sec)
{
  delayed_ack_record_p ptr = delayed_ack_list_head;
  int active_records = 0;


  // Skip this function if we did it during this second already.
  if (delayed_transmit_last_check == curr_sec)
  {
    return;
  }
  delayed_transmit_last_check = curr_sec;

//fprintf(stderr, "Checking delayed TX queue for something to transmit.\n");
//fprintf(stderr, ".");

  // Run down the linked list checking every record.
  while (ptr != NULL)
  {
    if (ptr->active_time != 0)     // Active record
    {
      char new_path[MAX_LINE_SIZE+1];


//fprintf(stderr, "Found active record\n");

      active_records++;


      // Check for a custom path having been set in the Send
      // Message dialog.  If so, use this for our outgoing
      // path instead and reset all of the queued message
      // paths to this station to this new path.
      //
      get_send_message_path(ptr->to_call_sign,
                            new_path,
                            sizeof(new_path));

      if (new_path[0] != '\0'
          && strcmp(new_path, ptr->path) != 0)
      {

        // We have a custom path set which is different than
        // the path saved with the outgoing message.  Change
        // the path to match the new path.
        //
//fprintf(stderr,
//    "Changing queued ack's to new path: %s\n",
//    new_path);

        memcpy(ptr->path, new_path, sizeof(ptr->path));
        ptr->path[sizeof(ptr->path)-1] = '\0';  // Terminate string
      }


      if (ptr->active_time <= sec_now())
      {
        // Transmit it
//fprintf(stderr,"Found something delayed to transmit!  %ld\n",sec_now());

        if (ptr->path[0] == '\0')
        {
          transmit_message_data(ptr->to_call_sign,
                                ptr->message_line,
                                NULL);
        }
        else
        {
          transmit_message_data(ptr->to_call_sign,
                                ptr->message_line,
                                ptr->path);
        }

        ptr->active_time = (time_t)0;
      }
    }
    ptr = ptr->next;
  }

  // Check if entire list contains inactive records.  If so,
  // delete the list.
  //
  if (!active_records && (delayed_ack_list_head != NULL))
  {
    // No active records, but the list isn't empty.  Reclaim the
    // records in the list.
    while (delayed_ack_list_head != NULL)
    {
      ptr = delayed_ack_list_head->next;
      free(delayed_ack_list_head);
//fprintf(stderr,"Free'ing delayed_ack record\n");
      delayed_ack_list_head = ptr;
    }
  }
}





void check_and_transmit_messages(time_t time)
{
  int i;
  char temp[200];
  char to_call[40];


  // Skip this function if we did it during this second already.
  if (last_check_and_transmit == time)
  {
    return;
  }
  last_check_and_transmit = time;

  for (i=0; i<MAX_OUTGOING_MESSAGES; i++)
  {
    if (message_pool[i].active==MESSAGE_ACTIVE)
    {
      if (message_pool[i].wait_on_first_ack!=1)   // Tx only if 0
      {
        if (message_pool[i].active_time < time)
        {
          char *last_ack_ptr;
          char last_ack[5+1];


          if (message_pool[i].tries < MAX_TRIES)
          {
            char new_path[MAX_LINE_SIZE+1];


            /* sending message let the tnc and net transmits check to see if we should */
            if (debug_level & 2)
              fprintf(stderr,
                      "Time %ld Active time %ld next time %ld\n",
                      (long)time,
                      (long)message_pool[i].active_time,
                      (long)message_pool[i].next_time);

            if (debug_level & 2)
              fprintf(stderr,"Send message#%d to <%s> from <%s>:%s-%s\n",
                      message_pool[i].tries,
                      message_pool[i].to_call_sign,
                      message_pool[i].from_call_sign,
                      message_pool[i].message_line,
                      message_pool[i].seq);

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
            {
              last_ack[0] = '\0';
            }

            xastir_snprintf(temp, sizeof(temp), ":%s:%s{%s}%s",
                            to_call,
                            message_pool[i].message_line,
                            message_pool[i].seq,
                            last_ack);

            if (debug_level & 2)
            {
              fprintf(stderr,"MESSAGE OUT>%s<\n",temp);
            }


            // Check for a custom path having been set
            // in the Send Message dialog.  If so, use
            // this for our outgoing path instead and
            // reset all of the queued message paths to
            // this station to this new path.
            //
            get_send_message_path(to_call,
                                  new_path,
                                  sizeof(new_path));

//fprintf(stderr,"get_send_message_path(%s) returned: %s\n",to_call,new_path);

            if (new_path[0] != '\0'
                && strcmp(new_path,message_pool[i].path) != 0)
            {

              // We have a custom path set which is
              // different than the path saved with
              // the outgoing message.
              //
              // Change all messages to that callsign
              // to match the new path.
              //
              change_path_outgoing_messages_to(to_call,new_path);
            }


            // Transmit the message
            transmit_message_data(message_pool[i].to_call_sign,
                                  temp,
                                  message_pool[i].path);


            message_pool[i].active_time = time + message_pool[i].next_time;

            //fprintf(stderr,"%d\n",(int)message_pool[i].next_time);
          }

          /*
          fprintf(stderr,
              "Msg Interval = %3ld seconds or %4.1f minutes\n",
              message_pool[i].next_time,
              message_pool[i].next_time / 60.0);
          */

          // Record the interval we're using.  Put it with
          // the message in the general message pool, so
          // that the Send Message dialog can display it.
          // It will only display it if the message is
          // actively being transmitted.  If it has been
          // cancelled, timed out, or hasn't made it to
          // the transmit position yet, it won't be shown.
          //
          msg_record_interval_tries(message_pool[i].to_call_sign,
                                    message_pool[i].from_call_sign,
                                    message_pool[i].seq,
                                    message_pool[i].next_time,  // Interval
                                    message_pool[i].tries);     // Tries

          // Start at 7 seconds for the interval.  We set
          // it to 7 seconds in output_message() above.
          // Double the interval each retry until we hit
          // 10 minutes.  Keep transmitting at 10 minute
          // intervals until we hit MAX_TRIES.

          // Double the interval between messages
          message_pool[i].next_time = message_pool[i].next_time * 2;

          // Limit the max interval to 10 minutes
          if (message_pool[i].next_time > (time_t)600L)
          {
            message_pool[i].next_time = (time_t)600L;
          }

          message_pool[i].tries++;

          // Expire it if we hit the limit
          if (message_pool[i].tries > MAX_TRIES)
          {
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

            // Record a fake ack and add "*TIMEOUT*" to
            // the message.  This will be displayed in
            // the Send Message dialog.
            msg_record_ack(temp_to,
                           message_pool[i].from_call_sign,
                           message_pool[i].seq,
                           1,  // "1" specifies a timeout
                           0); // Not a cancel

            clear_acked_message(temp_to,
                                message_pool[i].from_call_sign,
                                message_pool[i].seq);

//                        if (mw[i].send_message_dialog!=NULL) /* clear submit */
//                            XtSetSensitive(mw[i].button_ok,TRUE);
          }
        }
      }
      else
      {
        if (debug_level & 2)
        {
          fprintf(stderr,"Message #%s is waiting to have a previous one cleared\n",message_pool[i].seq);
        }
      }
    }
  }
}





// Function which marks a message as ack'ed in the transmit queue
// and releases the next message to allow it to be transmitted.
// Handles REPLY-ACK format or normal ACK format just fine.
//
void clear_acked_message(char *from, char *to, char *seq)
{
  int i,ii;
  int found;
  char lowest[3];
  char temp1[MAX_CALLSIGN+1];
  char *temp_ptr;
  char msg_id[5+1];


  // Copy seq into local variable
  xastir_snprintf(msg_id,
                  sizeof(msg_id),
                  "%s",
                  seq);

  // Check for REPLY-ACK protocol.  If found, terminate at the end
  // of the first ack.
  temp_ptr = strchr(msg_id, '}');
  if (temp_ptr)
  {
    *temp_ptr = '\0';
  }

  (void)remove_trailing_spaces(msg_id);  // This is IMPORTANT here!!!

  //lowest=100000;
  // Highest Base-90 2-char string
  xastir_snprintf(lowest,sizeof(lowest),"zz");
  found= -1;
  for (i=0; i<MAX_OUTGOING_MESSAGES; i++)
  {
    if (message_pool[i].active==MESSAGE_ACTIVE)
    {

      if (debug_level & 1)
        fprintf(stderr,
                "TO <%s> <%s> from <%s> <%s> seq <%s> <%s>\n",
                to,
                message_pool[i].to_call_sign,
                from,
                message_pool[i].from_call_sign,
                msg_id,
                message_pool[i].seq);

      if (strcmp(message_pool[i].to_call_sign,from)==0)
      {
        if (debug_level & 1)
        {
          fprintf(stderr,"Matched message to_call_sign\n");
        }
        if (strcmp(message_pool[i].from_call_sign,to)==0)
        {
          if (debug_level & 1)
          {
            fprintf(stderr,"Matched message from_call_sign\n");
          }
          if (strcmp(message_pool[i].seq,msg_id)==0)
          {
            if (debug_level & 2)
            {
              fprintf(stderr,"Found and cleared\n");
            }

            clear_outgoing_message(i);
            // now find and release next message, look for the lowest sequence?
// What about when the sequence rolls over?
            for (i=0; i<MAX_OUTGOING_MESSAGES; i++)
            {
              if (message_pool[i].active==MESSAGE_ACTIVE)
              {
                if (strcmp(message_pool[i].to_call_sign,from)==0)
                {
// Need to change this to a string compare instead of an integer
// compare.  We are using base-90 encoding now.
                  //if (atoi(message_pool[i].seq)<lowest) {
                  if (strncmp(message_pool[i].seq,lowest,2) < 0)
                  {
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
            if (found!= -1)
            {
              message_pool[found].wait_on_first_ack=0;
            }
            else
            {
              /* if no more clear the send button */

              begin_critical_section(&send_message_dialog_lock, "messages.c:clear_acked_message" );

              for (ii=0; ii<MAX_MESSAGE_WINDOWS; ii++)
              {
                /* find station  */
                if (mw[ii].send_message_dialog!=NULL)
                {

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
          else
          {
            if (debug_level & 1)
            {
              fprintf(stderr,"Sequences didn't match: %s %s\n",message_pool[i].seq,msg_id);
            }
          }
        }
      }
    }
  }
}





// This routine is not currently used.
//
void send_queued(char *to)
{
  int i;

  for (i=0; i<MAX_OUTGOING_MESSAGES ; i++)
  {
    /* Check for messages to call */
    if (message_pool[i].active==MESSAGE_ACTIVE)
      if (strcmp(to,message_pool[i].to_call_sign)==0)
      {
        message_pool[i].active_time=0;
      }

  }
}

