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

#include <stdlib.h>
#include <stdio.h>                      // printf
#include <stdint.h>

#include <Xm/XmAll.h>

#include "xastir.h"
#include "main.h"
#include "lang.h"
#include "xa_config.h"

// Must be last include file
#include "leak_detection.h"

extern XmFontList fontlist1;    // Menu/System fontlist

#if defined(__LSB__) || defined(LESSTIF_VERSION)
  #define NO_DYNAMIC_WIDGETS 1
#endif


#define MAX_PATH 200


Widget auto_msg_on, auto_msg_off;
Widget auto_msg_dialog = (Widget)NULL;
Widget auto_msg_set_data = (Widget)NULL;

static xastir_mutex auto_msg_dialog_lock;
xastir_mutex send_message_dialog_lock;

void select_station_type(int ii);





void messages_gui_init(void)
{
  init_critical_section( &auto_msg_dialog_lock );
  init_critical_section( &send_message_dialog_lock );
}





/**** Send Message ******/



// This function chops off the first callsign, then returns a string
// containing the same string in reversed callsign order.  Note that
// if RELAY was used by the sending station and that RELAY did _not_
// do callsign substitution, that part of the path will be chopped
// heading back.  If the sending station really needed that relay
// station in order to receive the reply, he/she should have put in
// a callsign instead of RELAY.  We can't in good conscience use
// RELAY on the end of the return path.
//
// We also chop off anything after comma "q" two letters, and a
// comma.  This is the injection-ID called the Q-construct, which
// lets us know how a signal was injected into the NET and by whom.
//
void reverse_path(char *input_string)
{
  char reverse_path[200];
  int indexes[20];
  int i, j, len;
  char temp[MAX_CALLSIGN+1];


  // Check for NULL pointer
  if (input_string == NULL)
  {
    return;
  }

  // Check for zero-length string
  len = strlen(input_string);
  if (len == 0)
  {
    return;
  }

  // Initialize
  reverse_path[0] = '\0';
  for (i = 0; i < 20; i++)
  {
    indexes[i] = -1;
  }

  // Add a comma onto the end (makes later code easier)
  input_string[len++] = ',';
  input_string[len] = '\0';   // Terminate it

  // Find each comma
  j = 0;
  for (i = 0; i < (int)strlen(input_string); i++)
  {
    if (input_string[i] == ',')
    {
      indexes[j++] = i;
      //fprintf(stderr,"%d\n",i);     // Debug code
    }
  }

  // Get rid of asterisks and commas in the original string:
  for (i = 0; i < len; i++)
  {
    if (input_string[i] == '*' || input_string[i] == ',')
    {
      input_string[i] = '\0';
    }
  }

  // Go left to right looking for a 3-letter callsign starting
  // with 'q'.  If found readjust 'j' to skip that callsign and
  // everything after it.
  //
  for ( i = 0; i < j; i++)
  {
    char *c = &input_string[indexes[i] + 1];

//fprintf(stderr,"'%s'\t", c );

    if (c[0] == 'q')
    {
      if ( strlen(c) == 3 )   // "qAR"
      {

//fprintf(stderr,"Found:%s\n", c);

        j = i;
      }
    }
  }

  // Convert used "WIDEn-N"/"TRACEn-N" paths back to their
  // original glory.  Convert "TRACE" to "WIDE" as well.  We could
  // also choose to change "WIDEn-N" to a slimmer version based on
  // how many digi's were used, for instance "WIDE7-6" could
  // change to "WIDE1-1" or "WIDE7-1", and "WIDE5-2" could change
  // to "WIDE3-3" or "WIDE5-3".
//    for ()

  // j now tells us how many were found.  Now go in the reverse
  // order and concatenate the substrings together.  Get rid of
  // "RELAY" and "TCPIP" calls as we're doing it.
  input_string[0] = '\0'; // Clear out the old string first
  for ( i = j - 1; i >= 0; i-- )
  {
    if ( (input_string[indexes[i]+1] != '\0')
         && (strncasecmp(&input_string[indexes[i]+1],"RELAY",5) != 0)
         && (strncasecmp(&input_string[indexes[i]+1],"TCPIP",5) != 0) )
    {

      // Snag each callsign into temp:
      xastir_snprintf(temp,
                      sizeof(temp),
                      "%s",
                      &input_string[indexes[i]+1]);

      // Massage temp until it resembles something we want to
      // use.
      //  "WIDEn" -> "WIDEn-N,"
      // "TRACEn" -> "WIDEn-N,"
      //  "TRACE" -> "WIDE,"
      if (strncasecmp(temp,"WIDE",4) == 0)
      {
        if ( (temp[4] != ',') && is_num_chr(temp[4]) )
        {
//fprintf(stderr,"Found a WIDEn-N\n");
          xastir_snprintf(temp,
                          sizeof(temp),
                          "WIDE%c-%c",
                          temp[4],
                          temp[4]);
        }
        else
        {
//fprintf(stderr,"Found a WIDE\n");
          // Leave temp alone, it's just a WIDE
        }
      }
      else if (strncasecmp(temp,"TRACE",5) == 0)
      {
        if ( (temp[5] != ',') && is_num_chr(temp[5]) )
        {
//fprintf(stderr,"Found a TRACEn-N\n");
          xastir_snprintf(temp,
                          sizeof(temp),
                          "WIDE%c-%c",
                          temp[5],
                          temp[5]);
        }
        else
        {
//fprintf(stderr,"Found a TRACE\n");
          // Convert it from TRACE to WIDE
          xastir_snprintf(temp,
                          sizeof(temp),
                          "WIDE");
        }
      }

      // Add temp to the end of our path:
      strncat(reverse_path,temp,sizeof(reverse_path)-strlen(reverse_path)-1);

      strncat(reverse_path,",",sizeof(reverse_path)-strlen(reverse_path)-1);
    }
  }

  // Remove the ending comma
  reverse_path[strlen(reverse_path) - 1] = '\0';

  // Save the new path back into the string we were given.
  strncat(input_string, reverse_path, len);
}





void get_path_data(char *callsign, char *path, int max_length)
{
  DataRow *p_station;


  if (search_station_name(&p_station,callsign,1))    // Found callsign
  {
    char new_path[200];

    if (p_station->node_path_ptr)
    {
      xastir_snprintf(new_path,sizeof(new_path), "%s", p_station->node_path_ptr);

      if(debug_level & 2)
        fprintf(stderr,"\nPath from %s: %s\n",
                callsign,
                new_path);

      // We need to chop off the first call, remove asterisks
      // and injection ID's, and reverse the order of the
      // callsigns.  We need to do the same thing in the
      // callback for button_submit_call, so that we get a new
      // path whenever the callsign is changed.  Create a new
      // TextFieldWidget to hold the path info, which gets
      // filled in here (and the callback) but can be changed by
      // the user.  Must find a nice way to use this path from
      // output_my_data() as well.

      reverse_path(new_path);

      if (debug_level & 2)
        fprintf(stderr,"  Path to %s: %s\n",
                callsign,
                new_path);

      xastir_snprintf(path,
                      max_length,
                      "%s",
                      new_path);
    }
    else
    {
      if (debug_level & 2)
      {
        fprintf(stderr," Path from %s is (null)\n",callsign);
      }
      path[0]='\0';
    }
  }
  else    // Couldn't find callsign.  It's
  {
    // not in our station database.
    if(debug_level & 2)
    {
      fprintf(stderr,"Path from %s: No Path Known\n",callsign);
    }

    path[0] = '\0';
  }

}





static Widget change_path_dialog = NULL;
static Widget current_path = NULL;





void Send_message_change_path_destroy_shell(Widget UNUSED(widget), XtPointer UNUSED(clientData), XtPointer UNUSED(callData) )
{

  if (change_path_dialog)
  {
    XtPopdown(change_path_dialog);

    XtDestroyWidget(change_path_dialog);
  }
  change_path_dialog = (Widget)NULL;
}





// Apply button
// Fetch the text from the "current_path" widget and place it into
// the mw[ii].send_message_path widget.
//
void Send_message_change_path_apply(Widget UNUSED(widget), XtPointer clientData, XtPointer UNUSED(callData) )
{
  char path[MAX_PATH+1];
  char *temp_ptr;


  if (current_path != NULL && clientData != NULL)
  {

    temp_ptr = XmTextFieldGetString(current_path);
    xastir_snprintf(path,
                    sizeof(path),
                    "%s",
                    temp_ptr);
    XtFree(temp_ptr);

    (void)remove_trailing_spaces(path);
    (void)to_upper(path);


// Check here for "DIRECT PATH" or "DEFAULT PATH".  If one of them,
// do some special processing if need be so that lower layers will
// interpret it correctly.

    XmTextFieldSetString(clientData, path);

    Send_message_change_path_destroy_shell(NULL, NULL, NULL);
  }
}





// "Direct Path" button
//
// Put "DIRECT PATH" in the widgets.  We pass "DIRECT PATH" all the
// way to the transmit routines, then change it to an empty path
// when the transmit actually goes out.
//
void Send_message_change_path_direct(Widget UNUSED(widget), XtPointer clientData, XtPointer UNUSED(callData) )
{

  if (current_path == NULL || clientData == NULL)
  {
    Send_message_change_path_destroy_shell(NULL, NULL, NULL);
  }

  // Change current_path widget
  XmTextFieldSetString(current_path, "DIRECT PATH");

  Send_message_change_path_apply(NULL, clientData, NULL);
}





// "Default Path(s)" button
//
// Blank out the path so the default paths get used.  We pass
// "DEFAULT PATH" all the way to the transmit routines, then change
// it there to be a blank.
//
void Send_message_change_path_default(Widget UNUSED(widget), XtPointer clientData, XtPointer UNUSED(callData) )
{

  if (current_path == NULL || clientData == NULL)
  {
    Send_message_change_path_destroy_shell(NULL, NULL, NULL);
  }

  // Change current_path widget
  XmTextFieldSetString(current_path, "DEFAULT PATH");

  Send_message_change_path_apply(NULL, clientData, NULL);
}





// TODO:  Change the "Path:" box so that clicking or double-clicking
// on it will bring up a "Change Path" dialog.  Could also use a
// "Change" or "Change Path" button if easier.  This new dialog
// should have the current path (editable), the reverse path (not
// editable), and these buttons:
//
//      "DIRECT path"
//      "DEFAULT path(s)"
//      "Apply"
//      "Cancel"
//
// Of course the underlying code will have to tweaked to be able to
// pass an EMPTY path all the way down through the layers.  We can't
// currently do that.  We'll have to define a specific string for
// that.  Insert the text "--DEFAULT--", "--BLANK--", or the actual
// path in the editable box and in the "Path:" box on the Send
// Message dialog so that the user knows which one is in effect.
//
// Remember to close the Change Path dialog if we close the Send
// Message dialog which corresponds to it.
//
// Adding this new CHANGE PATH dialog will allow us to get rid of
// three bugs on the active bug-list:  #1499820, #1326975, and
// #1326973.
//
void Send_message_change_path( Widget UNUSED(widget), XtPointer clientData, XtPointer UNUSED(callData) )
{
  int ii;
  //Atom delw;
  Widget pane, form, current_path_label, reverse_path_label,
         reverse_path, button_default, button_direct, button_apply,
         button_cancel;
  Widget button_wide11, button_wide21, button_wide22, button_nogate;
  char *temp_ptr;
  char temp1[MAX_LINE_SIZE+1];
  char path[MAX_PATH+1];


//begin_critical_section(&send_message_dialog_lock, "messages_gui.c:Send_message_change_path" );

  if (clientData == NULL)
  {
    return;
  }

  if (change_path_dialog)
  {
    // Destroy the old one before creating a new one
    Send_message_change_path_destroy_shell(NULL, NULL, NULL);
  }

  // Fetch Send Message dialog number from clientData, store in
  // "ii".
  //
  ii = atoi(clientData);

  // "Change Path"
  change_path_dialog = XtVaCreatePopupShell(langcode("WPUPMSB019"),
                       xmDialogShellWidgetClass, appshell,
                       XmNdeleteResponse,XmDESTROY,
                       XmNdefaultPosition, FALSE,
                       XmNtitleString,"Change Path",
                       XmNfontList, fontlist1,
                       NULL);

  pane = XtVaCreateWidget("Send_message_change_path pane",
                          xmPanedWindowWidgetClass,
                          change_path_dialog,
                          MY_FOREGROUND_COLOR,
                          MY_BACKGROUND_COLOR,
                          NULL);

  form =  XtVaCreateWidget("Send_message_change_path form",
                           xmFormWidgetClass,
                           pane,
                           XmNfractionBase, 4,
                           XmNautoUnmanage, FALSE,
                           XmNshadowThickness, 1,
                           MY_FOREGROUND_COLOR,
                           MY_BACKGROUND_COLOR,
                           NULL);

  // "Path:"
  current_path_label = XtVaCreateManagedWidget(langcode("WPUPMSB010"),
                       xmLabelWidgetClass,
                       form,
                       XmNtopAttachment, XmATTACH_FORM,
                       XmNtopOffset, 10,
                       XmNbottomAttachment, XmATTACH_NONE,
                       XmNleftAttachment, XmATTACH_FORM,
                       XmNleftOffset, 10,
                       XmNrightAttachment, XmATTACH_NONE,
                       MY_FOREGROUND_COLOR,
                       MY_BACKGROUND_COLOR,
                       XmNfontList, fontlist1,
                       NULL);

  current_path = XtVaCreateManagedWidget("Send_message_change_path path",
                                         xmTextFieldWidgetClass,
                                         form,
                                         XmNeditable,   TRUE,
                                         XmNcursorPositionVisible, TRUE,
                                         XmNsensitive, TRUE,
                                         XmNshadowThickness,    1,
                                         XmNcolumns, 26,
                                         XmNwidth, ((26*7)+2),
                                         XmNmaxLength, 199,
                                         XmNbackground, colors[0x0f],
                                         XmNtopAttachment, XmATTACH_FORM,
                                         XmNbottomAttachment, XmATTACH_NONE,
                                         XmNleftAttachment, XmATTACH_WIDGET,
                                         XmNleftWidget, current_path_label,
                                         XmNleftOffset, 5,
                                         XmNrightAttachment,XmATTACH_NONE,
                                         XmNnavigationType, XmTAB_GROUP,
                                         XmNtraversalOn, TRUE,
                                         XmNfontList, fontlist1,
                                         NULL);

  // "Reverse Path:"
  reverse_path_label = XtVaCreateManagedWidget(langcode("WPUPMSB022"),
                       xmLabelWidgetClass,
                       form,
                       XmNtopAttachment, XmATTACH_FORM,
                       XmNtopOffset, 10,
                       XmNbottomAttachment, XmATTACH_NONE,
                       XmNleftAttachment, XmATTACH_WIDGET,
                       XmNleftWidget, current_path,
                       XmNleftOffset, 10,
                       XmNrightAttachment, XmATTACH_NONE,
                       MY_FOREGROUND_COLOR,
                       MY_BACKGROUND_COLOR,
                       XmNfontList, fontlist1,
                       NULL);

  reverse_path = XtVaCreateManagedWidget("Send_message_change_path reverse path",
                                         xmTextFieldWidgetClass,
                                         form,
                                         XmNeditable,   FALSE,
                                         XmNcursorPositionVisible, FALSE,
                                         XmNsensitive, TRUE,
                                         XmNshadowThickness,    1,
                                         XmNcolumns, 26,
                                         XmNwidth, ((26*7)+2),
                                         XmNmaxLength, 199,
                                         XmNbackground, colors[0x0f],
                                         XmNtopAttachment, XmATTACH_FORM,
                                         XmNbottomAttachment, XmATTACH_NONE,
                                         XmNleftAttachment, XmATTACH_WIDGET,
                                         XmNleftWidget, reverse_path_label,
                                         XmNleftOffset, 5,
                                         XmNrightAttachment,XmATTACH_FORM,
                                         XmNrightOffset, 5,
                                         XmNnavigationType, XmTAB_GROUP,
                                         XmNtraversalOn, FALSE,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);

  button_wide11 = XtVaCreateManagedWidget("WIDE1-1",
                                          xmPushButtonGadgetClass,
                                          form,
                                          XmNtopAttachment, XmATTACH_WIDGET,
                                          XmNtopWidget, current_path_label,
                                          XmNtopOffset, 10,
                                          XmNbottomAttachment, XmATTACH_NONE,
                                          XmNleftAttachment, XmATTACH_POSITION,
                                          XmNleftPosition, 0,
                                          XmNrightAttachment, XmATTACH_POSITION,
                                          XmNrightPosition, 1,
                                          XmNnavigationType, XmTAB_GROUP,
                                          XmNtraversalOn, TRUE,
                                          MY_FOREGROUND_COLOR,
                                          MY_BACKGROUND_COLOR,
                                          XmNfontList, fontlist1,
                                          NULL);

  button_wide21 = XtVaCreateManagedWidget("WIDE2-1",
                                          xmPushButtonGadgetClass,
                                          form,
                                          XmNtopAttachment, XmATTACH_WIDGET,
                                          XmNtopWidget, current_path_label,
                                          XmNtopOffset, 10,
                                          XmNbottomAttachment, XmATTACH_NONE,
                                          XmNleftAttachment, XmATTACH_POSITION,
                                          XmNleftPosition, 1,
                                          XmNrightAttachment, XmATTACH_POSITION,
                                          XmNrightPosition, 2,
                                          XmNnavigationType, XmTAB_GROUP,
                                          XmNtraversalOn, TRUE,
                                          MY_FOREGROUND_COLOR,
                                          MY_BACKGROUND_COLOR,
                                          XmNfontList, fontlist1,
                                          NULL);

  button_wide22 = XtVaCreateManagedWidget("WIDE2-2",
                                          xmPushButtonGadgetClass,
                                          form,
                                          XmNtopAttachment, XmATTACH_WIDGET,
                                          XmNtopWidget, current_path_label,
                                          XmNtopOffset, 10,
                                          XmNbottomAttachment, XmATTACH_NONE,
                                          XmNleftAttachment, XmATTACH_POSITION,
                                          XmNleftPosition, 2,
                                          XmNrightAttachment, XmATTACH_POSITION,
                                          XmNrightPosition, 3,
                                          XmNnavigationType, XmTAB_GROUP,
                                          XmNtraversalOn, TRUE,
                                          MY_FOREGROUND_COLOR,
                                          MY_BACKGROUND_COLOR,
                                          XmNfontList, fontlist1,
                                          NULL);

  button_nogate = XtVaCreateManagedWidget("NOGATE",
                                          xmPushButtonGadgetClass,
                                          form,
                                          XmNtopAttachment, XmATTACH_WIDGET,
                                          XmNtopWidget, current_path_label,
                                          XmNtopOffset, 10,
                                          XmNbottomAttachment, XmATTACH_NONE,
                                          XmNleftAttachment, XmATTACH_POSITION,
                                          XmNleftPosition, 3,
                                          XmNrightAttachment, XmATTACH_POSITION,
                                          XmNrightPosition, 4,
                                          XmNnavigationType, XmTAB_GROUP,
                                          XmNtraversalOn, TRUE,
                                          MY_FOREGROUND_COLOR,
                                          MY_BACKGROUND_COLOR,
                                          XmNfontList, fontlist1,
                                          NULL);

  XtSetSensitive(button_wide11, FALSE);
  XtSetSensitive(button_wide21, FALSE);
  XtSetSensitive(button_wide22, FALSE);
  XtSetSensitive(button_nogate, FALSE);

  // "Use Default Path(s)"
  button_default = XtVaCreateManagedWidget(langcode("WPUPMSB020"),
                   xmPushButtonGadgetClass,
                   form,
                   XmNtopAttachment, XmATTACH_WIDGET,
                   XmNtopWidget, button_wide11,
                   XmNtopOffset, 0,
                   XmNbottomAttachment, XmATTACH_FORM,
                   XmNbottomOffset, 5,
                   XmNleftAttachment, XmATTACH_POSITION,
                   XmNleftPosition, 0,
                   XmNrightAttachment, XmATTACH_POSITION,
                   XmNrightPosition, 1,
                   XmNnavigationType, XmTAB_GROUP,
                   XmNtraversalOn, TRUE,
                   MY_FOREGROUND_COLOR,
                   MY_BACKGROUND_COLOR,
                   XmNfontList, fontlist1,
                   NULL);

  // "Direct (No path)"
  button_direct = XtVaCreateManagedWidget(langcode("WPUPMSB021"),
                                          xmPushButtonGadgetClass,
                                          form,
                                          XmNtopAttachment, XmATTACH_WIDGET,
                                          XmNtopWidget, button_wide11,
                                          XmNtopOffset, 0,
                                          XmNbottomAttachment, XmATTACH_FORM,
                                          XmNbottomOffset, 5,
                                          XmNleftAttachment, XmATTACH_POSITION,
                                          XmNleftPosition, 1,
                                          XmNrightAttachment, XmATTACH_POSITION,
                                          XmNrightPosition, 2,
                                          XmNnavigationType, XmTAB_GROUP,
                                          XmNtraversalOn, TRUE,
                                          MY_FOREGROUND_COLOR,
                                          MY_BACKGROUND_COLOR,
                                          XmNfontList, fontlist1,
                                          NULL);

  // "Apply"
  button_apply = XtVaCreateManagedWidget(langcode("UNIOP00032"),
                                         xmPushButtonGadgetClass,
                                         form,
                                         XmNtopAttachment, XmATTACH_WIDGET,
                                         XmNtopWidget, button_wide11,
                                         XmNtopOffset, 0,
                                         XmNbottomAttachment, XmATTACH_FORM,
                                         XmNbottomOffset, 5,
                                         XmNleftAttachment, XmATTACH_POSITION,
                                         XmNleftPosition, 2,
                                         XmNrightAttachment, XmATTACH_POSITION,
                                         XmNrightPosition, 3,
                                         XmNnavigationType, XmTAB_GROUP,
                                         XmNtraversalOn, TRUE,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);

  // "Close"
  button_cancel = XtVaCreateManagedWidget(langcode("UNIOP00003"),
                                          xmPushButtonGadgetClass,
                                          form,
                                          XmNtopAttachment, XmATTACH_WIDGET,
                                          XmNtopWidget, button_wide11,
                                          XmNtopOffset, 0,
                                          XmNbottomAttachment, XmATTACH_FORM,
                                          XmNbottomOffset, 5,
                                          XmNleftAttachment, XmATTACH_POSITION,
                                          XmNleftPosition, 3,
                                          XmNrightAttachment, XmATTACH_POSITION,
                                          XmNrightPosition, 4,
                                          XmNnavigationType, XmTAB_GROUP,
                                          XmNtraversalOn, TRUE,
                                          MY_FOREGROUND_COLOR,
                                          MY_BACKGROUND_COLOR,
                                          XmNfontList, fontlist1,
                                          NULL);

  XtAddCallback(button_default, XmNactivateCallback, Send_message_change_path_default, (XtPointer)mw[ii].send_message_path);
  XtAddCallback(button_direct, XmNactivateCallback, Send_message_change_path_direct, (XtPointer)mw[ii].send_message_path);
  XtAddCallback(button_apply, XmNactivateCallback, Send_message_change_path_apply, (XtPointer)mw[ii].send_message_path);
  XtAddCallback(button_cancel, XmNactivateCallback, Send_message_change_path_destroy_shell,(XtPointer)mw[ii].win);

  // Fill in the fields
  if(mw[ii].send_message_dialog != NULL)
  {
    char call_sign[MAX_CALLSIGN+1];

    temp_ptr = XmTextFieldGetString(mw[ii].send_message_path);
    xastir_snprintf(temp1,
                    sizeof(temp1),
                    "%s",
                    temp_ptr);
    XtFree(temp_ptr);
    (void)to_upper(temp1);
    XmTextFieldSetString(current_path, temp1);

    // Go get the reverse path.  Start with the callsign.
    temp_ptr = XmTextFieldGetString(mw[ii].send_message_call_data);
    xastir_snprintf(call_sign,
                    sizeof(call_sign),
                    "%s",
                    temp_ptr);
    XtFree(temp_ptr);

    (void)remove_trailing_dash_zero(call_sign);

    // Try lowercase
    get_path_data(call_sign, path, MAX_PATH);
    if (strlen(path) == 0)
    {
      // Try uppercase
      (void)to_upper(call_sign);
      get_path_data(call_sign, path, MAX_PATH);
    }
    XmTextFieldSetString(reverse_path, path);
  }

  pos_dialog(change_path_dialog);

  //delw
  (void)XmInternAtom(XtDisplay(change_path_dialog),"WM_DELETE_WINDOW", FALSE);
//    XmAddWMProtocolCallback(change_path_dialog, delw, Send_message_destroy_shell, (XtPointer)mw[ii].win);

  XtManageChild(form);
  XtManageChild(pane);

  XtPopup(change_path_dialog,XtGrabNone);

  // Move focus to the Cancel button.  This appears to highlight the
  // button fine, but we're not able to hit the <Enter> key to
  // have that default function happen.  Note:  We _can_ hit the
  // <SPACE> key, and that activates the option.
//    XmUpdateDisplay(change_path_dialog);
  XmProcessTraversal(button_cancel, XmTRAVERSE_CURRENT);

//end_critical_section(&send_message_dialog_lock, "messages_gui.c:Send_message_change_path" );

}





// Find a custom path set in a Send Message dialog, using the remote
// callsign as the key.  If no custom path, sets path to '\0'.
//
void get_send_message_path(char *callsign, char *path, int path_size)
{
  int ii;
  int found = -1;
  char *temp_ptr;
  char temp1[MAX_LINE_SIZE+1];
  char my_callsign[20];


  xastir_snprintf(my_callsign,sizeof(my_callsign),"%s",callsign);
  remove_trailing_spaces(my_callsign);

//fprintf(stderr,"Looking for %s\n", my_callsign);
  for(ii = 0; ii < MAX_MESSAGE_WINDOWS; ii++)
  {

    // find matching callsign
    if(mw[ii].send_message_dialog != NULL)
    {

      temp_ptr = XmTextFieldGetString(mw[ii].send_message_call_data);
      xastir_snprintf(temp1,
                      sizeof(temp1),
                      "%s",
                      temp_ptr);
      XtFree(temp_ptr);

      (void)remove_trailing_dash_zero(temp1);
      (void)to_upper(temp1);

      if(strcmp(temp1,my_callsign)==0)
      {
        found = ii;
        break;
      }
    }
  }

  if (found == -1)
  {
//fprintf(stderr,"Didn't find dialog\n");
    path[0] = '\0';
    return;
  }

  // We have the correct Send Message dialog.  Snag the path.
  //
  temp_ptr = XmTextFieldGetString(mw[ii].send_message_path);
  xastir_snprintf(temp1,
                  sizeof(temp1),
                  "%s",
                  temp_ptr);
  XtFree(temp_ptr);

  (void)to_upper(temp1);
  (void)remove_leading_spaces(temp1);
  (void)remove_trailing_spaces(temp1);

  // Path empty?
  if (temp1[0] == '\0')
  {
//fprintf(stderr,"Didn't find custom path\n");
    path[0] = '\0';
    return;
  }

  // We have a real path!  Stuff it into the path variable.
  xastir_snprintf(path,
                  path_size,
                  "%s",
                  temp1);
//fprintf(stderr,"Found custom path: %s\n", path);
}





void Send_message_destroy_shell( Widget UNUSED(widget), XtPointer clientData, XtPointer UNUSED(callData))
{
  int ii;
//    char *temp_ptr;
//    char temp1[MAX_LINE_SIZE+1];

//fprintf(stderr,"3Send_message_destroy_shell() start\n");

  ii=atoi((char *)clientData);

  begin_critical_section(&send_message_dialog_lock, "messages_gui.c:Send_message_destroy_shell" );

  if (mw[ii].send_message_dialog)
  {

    /*
            // Check whether the send_message_call_data field has a
            // custom path entered.
            temp_ptr = XmTextFieldGetString(mw[ii].send_message_call_data);
            xastir_snprintf(temp1,
                    sizeof(temp1),
                    "%s",
                    temp_ptr);
            XtFree(temp_ptr);

            (void)remove_leading_spaces(temp1);
            (void)remove_trailing_spaces(temp1);
            (void)remove_trailing_dash_zero(temp1);

            if(temp1[0] != '\0') {
                // Yep, we have a custom path.  Warn the user that
                // they're going to lose this path by destroying the
                // dialog.
                popup_message_always(langcode("POPEM00036"),
                    langcode("POPEM00040"));
            }
    */

    XtPopdown(mw[ii].send_message_dialog);
    XtDestroyWidget(mw[ii].send_message_dialog);
  }


  mw[ii].send_message_dialog = (Widget)NULL;
  mw[ii].to_call_sign[0] = '\0';
  mw[ii].send_message_call_data = (Widget)NULL;
  mw[ii].D700_mode = (Widget)NULL;
  mw[ii].D7_mode = (Widget)NULL;
  mw[ii].HamHUD_mode = (Widget)NULL;
  mw[ii].message_data_line1 = (Widget)NULL;
  mw[ii].message_data_line2 = (Widget)NULL;
  mw[ii].message_data_line3 = (Widget)NULL;
  mw[ii].message_data_line4 = (Widget)NULL;
  mw[ii].send_message_text = (Widget)NULL;

  Send_message_change_path_destroy_shell(NULL, NULL, NULL);

  end_critical_section(&send_message_dialog_lock, "messages_gui.c:Send_message_destroy_shell" );

//fprintf(stderr,"3Send_message_destroy_shell() finished\n");

}





void Check_new_call_messages( Widget UNUSED(w), XtPointer clientData, XtPointer UNUSED(callData) )
{
  int pos;
  intptr_t i;

  i=(intptr_t)clientData;

  /* clear window*/
  pos=0;

  begin_critical_section(&send_message_dialog_lock, "messages_gui.c:Check_new_call_messages" );

  if (mw[i].send_message_dialog)
  {
    // If we have a dialog already, clear out the message area
    // from 0 to the last text position.

    // Known to have memory leaks with some versions of Motif:
    //XmTextSetString(mw[i].send_message_text,"");

    XmTextReplace(mw[i].send_message_text, (XmTextPosition) 0,
                  XmTextGetLastPosition(mw[i].send_message_text), "");

    // Set the cursor position to 0
    XtVaSetValues(mw[i].send_message_text,XmNcursorPosition,pos,NULL);
  }

  end_critical_section(&send_message_dialog_lock, "messages_gui.c:Check_new_call_messages" );

  update_messages(1); // Force an immediate update

  if (mw[i].send_message_dialog)
  {
    // Re-arrange the outgoing message boxes based on the type of device we're talking to.
    select_station_type(i);
  }
}





void Clear_messages( Widget UNUSED(w), XtPointer UNUSED(clientData), XtPointer UNUSED(callData) )
{
  clear_outgoing_messages();
}





void Send_message_now( Widget UNUSED(w), XtPointer clientData, XtPointer UNUSED(callData) )
{
  char temp1[MAX_CALLSIGN+1];
  char temp2[121];
  char temp_line1[68] = "";

#ifndef NO_DYNAMIC_WIDGETS
  char temp_line2[23] = "";
  char temp_line3[23] = "";
  char temp_line4[10] = "";
#endif    // NO_DYNAMIC_WIDGETS

  char path[200];
  int ii, jj;
  char *temp_ptr;
  int substitution_made = 0;
  int d700;
  int d7;
  int hamhud;
  char temp_file_path[MAX_VALUE];

  ii=atoi((char *)clientData);

  begin_critical_section(&send_message_dialog_lock, "messages_gui.c:Send_message_now" );

  if (mw[ii].send_message_dialog)
  {

    d700 = XmToggleButtonGetState(mw[ii].D700_mode);
    d7 = XmToggleButtonGetState(mw[ii].D7_mode);
    hamhud = XmToggleButtonGetState(mw[ii].HamHUD_mode);

    temp_ptr = XmTextFieldGetString(mw[ii].send_message_call_data);
    xastir_snprintf(temp1,
                    sizeof(temp1),
                    "%s",
                    temp_ptr);
    XtFree(temp_ptr);

    (void)remove_trailing_spaces(temp1);
    (void)to_upper(temp1);
    (void)remove_trailing_dash_zero(temp1);

    // Fetch message_data_line1 in all cases
    temp_ptr = XmTextFieldGetString(mw[ii].message_data_line1);
    xastir_snprintf(temp_line1,
                    sizeof(temp_line1),
                    "%s",
                    temp_ptr);
    XtFree(temp_ptr);

#ifndef NO_DYNAMIC_WIDGETS

    // If D700/D7 mode, fetch message_data_line2
    if (d700 || d7)
    {
      temp_ptr = XmTextFieldGetString(mw[ii].message_data_line2);
      xastir_snprintf(temp_line2,
                      sizeof(temp_line2),
                      "%s",
                      temp_ptr);
      XtFree(temp_ptr);
    }

    // If D700/D7 mode, fetch message_data_line3
    if (d700 || d7)
    {
      temp_ptr = XmTextFieldGetString(mw[ii].message_data_line3);
      xastir_snprintf(temp_line3,
                      sizeof(temp_line3),
                      "%s",
                      temp_ptr);
      XtFree(temp_ptr);
    }

    // If D7 mode, fetch message_data_line4
    if (d7)
    {
      temp_ptr = XmTextFieldGetString(mw[ii].message_data_line4);
      xastir_snprintf(temp_line4,
                      sizeof(temp_line4),
                      "%s",
                      temp_ptr);
      XtFree(temp_ptr);
    }

    // Construct the entire message now
    if (hamhud)   // Combine two lines together
    {
      xastir_snprintf(temp2,
                      sizeof(temp2),
                      "%-20s%-47s",
                      temp_line1,
                      temp_line2);
    }
    else if (d700)   // Combine three lines together
    {
      xastir_snprintf(temp2,
                      sizeof(temp2),
                      "%-22s%-22s%-20s",
                      temp_line1,
                      temp_line2,
                      temp_line3);
    }
    else if (d7)    // Combine four lines together
    {
      xastir_snprintf(temp2,
                      sizeof(temp2),
                      "%-12s%-12s%-12s%-9s",
                      temp_line1,
                      temp_line2,
                      temp_line3,
                      temp_line4);
    }
    else

#endif  // NO_DYNAMIC_WIDGETS 

    {
      // Use line1 only
      xastir_snprintf(temp2,
                      sizeof(temp2),
                      "%s",
                      temp_line1);
    }

    // We have the message text now.  Check it for illegal
    // characters, remove them and substitute '.' if found.
    // Illegal characters are '|', '{', and '~' for messaging.
    for (jj = 0; jj < (int)strlen(temp2); jj++)
    {
      if (       temp2[jj] == '|'
                 || temp2[jj] == '{'
                 || temp2[jj] == '~' )
      {
        temp2[jj] = '.';    // Replace with a dot
        substitution_made++;
      }
    }
    if (substitution_made)
    {
      popup_message_always(langcode("POPEM00022"),
                           langcode("POPEM00039"));
    }

    (void)remove_trailing_spaces(temp2);

    temp_ptr = XmTextFieldGetString(mw[ii].send_message_path);
    xastir_snprintf(path,
                    sizeof(path),
                    "%s",
                    temp_ptr);
    XtFree(temp_ptr);

    (void)remove_trailing_spaces(path);
    (void)to_upper(path);

    if(debug_level & 2)
      fprintf(stderr,
              "Send message to <%s> from <%s> :%s\n",
              temp1,
              mw[ii].to_call_sign,
              temp2);

    if ( (strlen(temp1) != 0)                       // Callsign field is not blank
         && (strlen(temp2) != 0)                 // Message field is not blank
         && (strcmp(temp1,my_callsign) ) )       // And not my own callsign
    {
      /* if you're sending a message you must be at the keyboard */
      auto_reply=0;
      XmToggleButtonSetState(auto_msg_toggle,FALSE,FALSE);
      statusline(langcode("BBARSTA011"),0);       // Auto Reply Messages OFF
      output_message(mw[ii].to_call_sign,temp1,temp2,path);

//fprintf(stderr,"         1111111111222222222233333333334444444444555555555566666666\n");
//fprintf(stderr,"1234567890123456789012345678901234567890123456789012345678901234567\n");
//fprintf(stderr,"%s\n",temp2);

      XmTextFieldSetString(mw[ii].message_data_line1,"");

      if (mw[ii].message_data_line2) // If exists, blank it
      {
        XmTextFieldSetString(mw[ii].message_data_line2,"");
      }

      if (mw[ii].message_data_line3) // If exists, blank it
      {
        XmTextFieldSetString(mw[ii].message_data_line3,"");
      }

      if (mw[ii].message_data_line4) // If exists, blank it
      {
        XmTextFieldSetString(mw[ii].message_data_line4,"");
      }

//            if (mw[ii].message_group!=1)
//                XtSetSensitive(mw[ii].button_ok,FALSE);

      // Do message logging if that feature is enabled.
      if (log_message_data)
      {
        char temp_msg[MAX_MESSAGE_LENGTH+1];

        strcpy(temp_msg, mw[ii].to_call_sign);// To
        temp_msg[sizeof(temp_msg)-1] = '\0';  // Terminate string
        strcat(temp_msg, ">");
        temp_msg[sizeof(temp_msg)-1] = '\0';  // Terminate string
        strcat(temp_msg, temp1);              // From
        temp_msg[sizeof(temp_msg)-1] = '\0';  // Terminate string
        strcat(temp_msg, ",");
        temp_msg[sizeof(temp_msg)-1] = '\0';  // Terminate string
        strcat(temp_msg, path);               // Path
        temp_msg[sizeof(temp_msg)-1] = '\0';  // Terminate string
        strcat(temp_msg, ":");
        temp_msg[sizeof(temp_msg)-1] = '\0';  // Terminate string
        strcat(temp_msg, temp2);              // Message
        temp_msg[sizeof(temp_msg)-1] = '\0';  // Terminate string

        log_data( get_user_base_dir(LOGFILE_MESSAGE, temp_file_path,
                                    sizeof(temp_file_path)),
                  temp_msg );
      }
    }
    else
    {
      if ( strcmp(temp1,my_callsign) == 0 )   // It's my own callsign
      {
        popup_message_always(langcode("POPEM00022"),
                             langcode("POPEM00054"));   // We're trying to talk to ourselves!
      }
      else if ( strlen(temp1) == 0 )   // Callsign field is blank
      {
        popup_message_always(langcode("POPEM00022"),
                             langcode("POPEM00052"));   // Callsign is EMPTY!
      }
      else if ( strlen(temp2) == 0 )   // Message field is blank
      {
        popup_message_always(langcode("POPEM00022"),
                             langcode("POPEM00053"));   // Message is EMPTY!
      }
    }
  }

  // Move focus to the first message box to make typing the next
  // message easier.
  XmProcessTraversal(mw[ii].message_data_line1, XmTRAVERSE_CURRENT);

  end_critical_section(&send_message_dialog_lock, "messages_gui.c:Send_message_now" );

}





void Clear_message_from( Widget UNUSED(w), XtPointer clientData, XtPointer UNUSED(callData) )
{
  char temp1[MAX_CALLSIGN+1];
  int i;
  char *temp_ptr;
  /* int pos;*/


  i=atoi((char *)clientData);

  begin_critical_section(&send_message_dialog_lock, "messages_gui.c:Clear_message_from" );

  if (mw[i].send_message_dialog)
  {

    temp_ptr = XmTextFieldGetString(mw[i].send_message_call_data);
    xastir_snprintf(temp1,
                    sizeof(temp1),
                    "%s",
                    temp_ptr);
    XtFree(temp_ptr);

    (void)remove_trailing_spaces(temp1);
    (void)to_upper(temp1);
    (void)remove_trailing_dash_zero(temp1);

    /*fprintf(stderr,"Clear message from <%s> to <%s>\n",temp1,my_callsign);*/
    mdelete_messages_from(temp1);
    new_message_data=1;
  }

  end_critical_section(&send_message_dialog_lock, "messages_gui.c:Clear_message_from" );

}





void Clear_message_to( Widget UNUSED(w), XtPointer clientData, XtPointer UNUSED(callData) )
{
  char temp1[MAX_CALLSIGN+1];
  int i;
  char *temp_ptr;


  i=atoi((char *)clientData);

  begin_critical_section(&send_message_dialog_lock, "messages_gui.c:Clear_message_to" );

  if (mw[i].send_message_dialog)
  {

    temp_ptr = XmTextFieldGetString(mw[i].send_message_call_data);
    xastir_snprintf(temp1,
                    sizeof(temp1),
                    "%s",
                    temp_ptr);
    XtFree(temp_ptr);

    (void)remove_trailing_spaces(temp1);
    (void)to_upper(temp1);
    (void)remove_trailing_dash_zero(temp1);

    /*fprintf(stderr,"Clear message to <%s>\n",temp1);*/
    mdelete_messages_to(temp1);
    new_message_data=1;
  }

  end_critical_section(&send_message_dialog_lock, "messages_gui.c:Clear_message_to" );

}





void Clear_message_to_from( Widget w, XtPointer clientData, XtPointer callData)
{
  int i, pos;

  Clear_message_to(w, clientData, callData);
  Clear_message_from(w, clientData, callData);

  i=atoi((char *)clientData);
  /* clear window*/
  pos=0;

  begin_critical_section(&send_message_dialog_lock, "messages_gui.c:Clear_message_to_from" );

  if (mw[i].send_message_dialog)
  {

    // Known to have memory leaks with some versions of Motif:
    //XmTextSetString(mw[i].send_message_text,"");

    // Clear out the message window
    XmTextReplace(mw[i].send_message_text, (XmTextPosition) 0,
                  XmTextGetLastPosition(mw[i].send_message_text), "");

    // Set the cursor position to 0
    XtVaSetValues(mw[i].send_message_text,XmNcursorPosition,pos,NULL);

    end_critical_section(&send_message_dialog_lock, "messages_gui.c:Clear_message_to_from" );

    update_messages(1);     // Force an update to clear the window

    begin_critical_section(&send_message_dialog_lock, "messages_gui.c:Clear_message_to_from" );

  }

  end_critical_section(&send_message_dialog_lock, "messages_gui.c:Clear_message_to_from" );

}





void Kick_timer( Widget UNUSED(w), XtPointer clientData, XtPointer UNUSED(callData) )
{
  char *temp_ptr;
  char temp1[MAX_CALLSIGN+1];


  temp_ptr = XmTextFieldGetString(mw[atoi(clientData)].send_message_call_data);
  xastir_snprintf(temp1,
                  sizeof(temp1),
                  "%s",
                  temp_ptr);
  XtFree(temp_ptr);

  (void)remove_trailing_spaces(temp1);
  (void)to_upper(temp1);
  (void)remove_trailing_dash_zero(temp1);

  kick_outgoing_timer(temp1);
}





void Clear_messages_to( Widget UNUSED(w), XtPointer clientData, XtPointer UNUSED(callData) )
{
  char *temp_ptr;
  char temp1[MAX_CALLSIGN+1];


  temp_ptr = XmTextFieldGetString(mw[atoi(clientData)].send_message_call_data);
  xastir_snprintf(temp1,
                  sizeof(temp1),
                  "%s",
                  temp_ptr);
  XtFree(temp_ptr);

  (void)remove_trailing_spaces(temp1);
  (void)to_upper(temp1);
  (void)remove_trailing_dash_zero(temp1);

  clear_outgoing_messages_to(temp1);
  update_messages(1); // Force an update to the window
}





void Send_message_call( Widget UNUSED(w), XtPointer clientData, XtPointer UNUSED(callData) )
{
  char call[20];

  if(clientData != NULL)
  {
    xastir_snprintf(call,
                    sizeof(call),
                    "%s",
                    (char *)clientData);
    Send_message(appshell, call, NULL);
  }
}





// These are used for callbacks below so that we have a unique
// identifier for the XtRemoveCallback() function to work with.  The
// last two parameters of an XtAddCallback() or XtRemoveCallback()
// must be unique.
//
void Send_message_now_1( Widget w, XtPointer clientData, XtPointer callData)
{
  Send_message_now( w, clientData, callData);
}
void Send_message_now_2( Widget w, XtPointer clientData, XtPointer callData)
{
  Send_message_now( w, clientData, callData);
}
void Send_message_now_3( Widget w, XtPointer clientData, XtPointer callData)
{
  Send_message_now( w, clientData, callData);
}
void Send_message_now_4( Widget w, XtPointer clientData, XtPointer callData)
{
  Send_message_now( w, clientData, callData);
}





void build_send_message_input_boxes(int i, int hamhud, int d700, int d7)
{

//fprintf(stderr, "\n  build:   i:%d  hamhud:%d  d700:%d  d7:%d\n", i, hamhud, d700, d7);


// Skip most of these sections and go to the default section if
// using LSB.  We have problems with Lesstif segfaulting on us
// otherwise.

#ifndef NO_DYNAMIC_WIDGETS

  // HamHUD mode (Here we're assuming the 4x20 LCD in the HamHUD-II)
  if (hamhud)
  {
    // Draw a textfield widget of size 20
    mw[i].message_data_line1 = XtVaCreateManagedWidget("Send_message smmd",
                               xmTextFieldWidgetClass,
                               mw[i].form,
                               XmNeditable,   TRUE,
                               XmNcursorPositionVisible, TRUE,
                               XmNsensitive, TRUE,
                               XmNshadowThickness,    1,
                               XmNcolumns, 22,
                               XmNwidth, ((22*7)),
                               XmNmaxLength, 20,
                               XmNbackground, colors[0x0f],
                               XmNtopAttachment, XmATTACH_NONE,
                               XmNbottomAttachment, XmATTACH_WIDGET,
                               XmNbottomWidget, mw[i].button_clear_old_msgs,
                               XmNbottomOffset, 5,
                               XmNleftAttachment, XmATTACH_WIDGET,
                               XmNleftWidget, mw[i].message,
                               XmNleftOffset, 5,
                               XmNrightAttachment,XmATTACH_NONE,
                               XmNnavigationType, XmTAB_GROUP,
                               XmNtraversalOn, TRUE,
                               XmNfontList, fontlist1,
                               NULL);

// HamHUD will display all 67 chars, but only stores the first 20,
// therefore if you want the recipient to be able to look at it
// later, only send them 20 chars per message.
    /*
            // Draw another textfield widget of size 47
            mw[i].message_data_line2 = XtVaCreateManagedWidget("Send_message line2",
                xmTextFieldWidgetClass,
                mw[i].form,
                XmNeditable,   TRUE,
                XmNcursorPositionVisible, TRUE,
                XmNsensitive, TRUE,
                XmNshadowThickness,    1,
                XmNcolumns, 43,
                XmNwidth, ((43*7)),
                XmNmaxLength, 47,
                XmNbackground, colors[0x0f],
                XmNtopAttachment, XmATTACH_NONE,
                XmNbottomAttachment, XmATTACH_WIDGET,
                XmNbottomWidget, mw[i].button_clear_old_msgs,
                XmNbottomOffset, 5,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, mw[i].message_data_line1,
                XmNleftOffset, 2,
                XmNrightAttachment,XmATTACH_NONE,
                XmNnavigationType, XmTAB_GROUP,
                XmNtraversalOn, TRUE,
                XmNfontList, fontlist1,
                NULL);
    */
  }

  // D700A mode
  else if (d700)
  {

    // Draw a textfield widget of size 22
    mw[i].message_data_line1 = XtVaCreateManagedWidget("Send_message smmd",
                               xmTextFieldWidgetClass,
                               mw[i].form,
                               XmNeditable,   TRUE,
                               XmNcursorPositionVisible, TRUE,
                               XmNsensitive, TRUE,
                               XmNshadowThickness,    1,
                               XmNcolumns, 22,
                               XmNwidth, ((22*7)),
                               XmNmaxLength, 22,
                               XmNbackground, colors[0x0f],
                               XmNtopAttachment, XmATTACH_NONE,
                               XmNbottomAttachment, XmATTACH_WIDGET,
                               XmNbottomWidget, mw[i].button_clear_old_msgs,
                               XmNbottomOffset, 5,
                               XmNleftAttachment, XmATTACH_WIDGET,
                               XmNleftWidget, mw[i].message,
                               XmNleftOffset, 5,
                               XmNrightAttachment,XmATTACH_NONE,
                               XmNnavigationType, XmTAB_GROUP,
                               XmNtraversalOn, TRUE,
                               XmNfontList, fontlist1,
                               NULL);

    // Draw another textfield widget of size 22
    mw[i].message_data_line2 = XtVaCreateManagedWidget("Send_message line2",
                               xmTextFieldWidgetClass,
                               mw[i].form,
                               XmNeditable,   TRUE,
                               XmNcursorPositionVisible, TRUE,
                               XmNsensitive, TRUE,
                               XmNshadowThickness,    1,
                               XmNcolumns, 22,
                               XmNwidth, ((22*7)),
                               XmNmaxLength, 22,
                               XmNbackground, colors[0x0f],
                               XmNtopAttachment, XmATTACH_NONE,
                               XmNbottomAttachment, XmATTACH_WIDGET,
                               XmNbottomWidget, mw[i].button_clear_old_msgs,
                               XmNbottomOffset, 5,
                               XmNleftAttachment, XmATTACH_WIDGET,
                               XmNleftWidget, mw[i].message_data_line1,
                               XmNleftOffset, 2,
                               XmNrightAttachment,XmATTACH_NONE,
                               XmNnavigationType, XmTAB_GROUP,
                               XmNtraversalOn, TRUE,
                               XmNfontList, fontlist1,
                               NULL);

    // Draw another textfield widget of size 20
    mw[i].message_data_line3 = XtVaCreateManagedWidget("Send_message line3",
                               xmTextFieldWidgetClass,
                               mw[i].form,
                               XmNeditable,   TRUE,
                               XmNcursorPositionVisible, TRUE,
                               XmNsensitive, TRUE,
                               XmNshadowThickness,    1,
                               XmNcolumns, 21,
                               XmNwidth, ((21*7)),
                               XmNmaxLength, 20,
                               XmNbackground, colors[0x0f],
                               XmNtopAttachment, XmATTACH_NONE,
                               XmNbottomAttachment, XmATTACH_WIDGET,
                               XmNbottomWidget, mw[i].button_clear_old_msgs,
                               XmNbottomOffset, 5,
                               XmNleftAttachment, XmATTACH_WIDGET,
                               XmNleftWidget, mw[i].message_data_line2,
                               XmNleftOffset, 2,
                               XmNrightAttachment,XmATTACH_NONE,
                               XmNnavigationType, XmTAB_GROUP,
                               XmNtraversalOn, TRUE,
                               XmNfontList, fontlist1,
                               NULL);
  }

  // D7A/D7E Mode
  else if (d7)
  {

    // Draw a textfield widget of size 12
    mw[i].message_data_line1 = XtVaCreateManagedWidget("Send_message smmd",
                               xmTextFieldWidgetClass,
                               mw[i].form,
                               XmNeditable,   TRUE,
                               XmNcursorPositionVisible, TRUE,
                               XmNsensitive, TRUE,
                               XmNshadowThickness,    1,
                               XmNcolumns, 12,
                               XmNwidth, ((12*7)+4),
                               XmNmaxLength, 12,
                               XmNbackground, colors[0x0f],
                               XmNtopAttachment, XmATTACH_NONE,
                               XmNbottomAttachment, XmATTACH_WIDGET,
                               XmNbottomWidget, mw[i].button_clear_old_msgs,
                               XmNbottomOffset, 5,
                               XmNleftAttachment, XmATTACH_WIDGET,
                               XmNleftWidget, mw[i].message,
                               XmNleftOffset, 5,
                               XmNrightAttachment,XmATTACH_NONE,
                               XmNnavigationType, XmTAB_GROUP,
                               XmNtraversalOn, TRUE,
                               XmNfontList, fontlist1,
                               NULL);

    // Draw a textfield widget of size 12
    mw[i].message_data_line2 = XtVaCreateManagedWidget("Send_message line2",
                               xmTextFieldWidgetClass,
                               mw[i].form,
                               XmNeditable,   TRUE,
                               XmNcursorPositionVisible, TRUE,
                               XmNsensitive, TRUE,
                               XmNshadowThickness,    1,
                               XmNcolumns, 12,
                               XmNwidth, ((12*7)+4),
                               XmNmaxLength, 12,
                               XmNbackground, colors[0x0f],
                               XmNtopAttachment, XmATTACH_NONE,
                               XmNbottomAttachment, XmATTACH_WIDGET,
                               XmNbottomWidget, mw[i].button_clear_old_msgs,
                               XmNbottomOffset, 5,
                               XmNleftAttachment, XmATTACH_WIDGET,
                               XmNleftWidget, mw[i].message_data_line1,
                               XmNleftOffset, 2,
                               XmNrightAttachment,XmATTACH_NONE,
                               XmNnavigationType, XmTAB_GROUP,
                               XmNtraversalOn, TRUE,
                               XmNfontList, fontlist1,
                               NULL);

// Separate the next two from the previous two for a bit, to help
// indicate that the first two are one screen on a TH-D7, the next
// two are another screen.

    // Draw a textfield widget of size 12
    mw[i].message_data_line3 = XtVaCreateManagedWidget("Send_message line3",
                               xmTextFieldWidgetClass,
                               mw[i].form,
                               XmNeditable,   TRUE,
                               XmNcursorPositionVisible, TRUE,
                               XmNsensitive, TRUE,
                               XmNshadowThickness,    1,
                               XmNcolumns, 12,
                               XmNwidth, ((12*7)+4),
                               XmNmaxLength, 12,
                               XmNbackground, colors[0x0f],
                               XmNtopAttachment, XmATTACH_NONE,
                               XmNbottomAttachment, XmATTACH_WIDGET,
                               XmNbottomWidget, mw[i].button_clear_old_msgs,
                               XmNbottomOffset, 5,
                               XmNleftAttachment, XmATTACH_WIDGET,
                               XmNleftWidget, mw[i].message_data_line2,
                               XmNleftOffset, 15,
                               XmNrightAttachment,XmATTACH_NONE,
                               XmNnavigationType, XmTAB_GROUP,
                               XmNtraversalOn, TRUE,
                               XmNfontList, fontlist1,
                               NULL);

    // Draw a textfield widget of size 9
    mw[i].message_data_line4 = XtVaCreateManagedWidget("Send_message line4",
                               xmTextFieldWidgetClass,
                               mw[i].form,
                               XmNeditable,   TRUE,
                               XmNcursorPositionVisible, TRUE,
                               XmNsensitive, TRUE,
                               XmNshadowThickness,    1,
                               XmNcolumns, 9,
                               XmNwidth, ((10*7)+0),
                               XmNmaxLength, 9,
                               XmNbackground, colors[0x0f],
                               XmNtopAttachment, XmATTACH_NONE,
                               XmNbottomAttachment, XmATTACH_WIDGET,
                               XmNbottomWidget, mw[i].button_clear_old_msgs,
                               XmNbottomOffset, 5,
                               XmNleftAttachment, XmATTACH_WIDGET,
                               XmNleftWidget, mw[i].message_data_line3,
                               XmNleftOffset, 2,
                               XmNrightAttachment,XmATTACH_NONE,
                               XmNnavigationType, XmTAB_GROUP,
                               XmNtraversalOn, TRUE,
                               XmNfontList, fontlist1,
                               NULL);
  }

  // Standard APRS Mode
  else // Standard APRS message box size (67)

#endif  // NO_DYNAMIC_WIDGETS

  {

    mw[i].message_data_line1 = XtVaCreateManagedWidget("Send_message smmd",
                               xmTextFieldWidgetClass,
                               mw[i].form,
                               XmNeditable,   TRUE,
                               XmNcursorPositionVisible, TRUE,
                               XmNsensitive, TRUE,
                               XmNshadowThickness,    1,
                               XmNcolumns, 67,
                               XmNwidth, ((65*7)+2),
                               XmNmaxLength, 67,
                               XmNbackground, colors[0x0f],
                               XmNtopAttachment, XmATTACH_NONE,
                               XmNbottomAttachment, XmATTACH_WIDGET,
                               XmNbottomWidget, mw[i].button_clear_old_msgs,
                               XmNbottomOffset, 5,
                               XmNleftAttachment, XmATTACH_WIDGET,
                               XmNleftWidget, mw[i].message,
                               XmNleftOffset, 5,
                               XmNrightAttachment,XmATTACH_NONE,
                               XmNnavigationType, XmTAB_GROUP,
                               XmNtraversalOn, TRUE,
                               XmNfontList, fontlist1,
                               NULL);
  }


//fprintf(stderr,"Starting to add callbacks\n");


  if (mw[i].message_data_line1) // If exists, add another callback
  {
    XtAddCallback(mw[i].message_data_line1, XmNactivateCallback, Send_message_now_1, (XtPointer)mw[i].win);
  }

  if (mw[i].message_data_line2) // If exists, add another callback
  {
    XtAddCallback(mw[i].message_data_line2, XmNactivateCallback, Send_message_now_2, (XtPointer)mw[i].win);
  }

  if (mw[i].message_data_line3) // If exists, add another callback
  {
    XtAddCallback(mw[i].message_data_line3, XmNactivateCallback, Send_message_now_3, (XtPointer)mw[i].win);
  }

  if (mw[i].message_data_line4) // If exists, add another callback
  {
    XtAddCallback(mw[i].message_data_line4, XmNactivateCallback, Send_message_now_4, (XtPointer)mw[i].win);
  }

//fprintf(stderr,"Exiting build_send_message_input_boxes()\n");

}





void rebuild_send_message_input_boxes(int ii, int hamhud, int d700, int d7)
{

//fprintf(stderr, "\nrebuild:  ii:%d  hamhud:%d  d700:%d  d7:%d\n", ii, hamhud, d700, d7);


// Lesstif appears to have a problem with removing/adding widgets to
// a dialog that's already been created and will segfault in this
// case.  In order to make LSB-Xastir more reliable we disable the
// dynamically-created widget code here and stick with the default
// setup (one long TextField widget for input).
//
// Perhaps we need to do a Lesstif detect and do the same thing
// anytime Lesstif is used as well?

#ifndef NO_DYNAMIC_WIDGETS

  // Remove the current message widgets
  if (mw[ii].message_data_line4)
  {
    XtDestroyWidget(mw[ii].message_data_line4);
  }

  if (mw[ii].message_data_line3)
  {
    XtDestroyWidget(mw[ii].message_data_line3);
  }

  if (mw[ii].message_data_line2)
  {
    XtDestroyWidget(mw[ii].message_data_line2);
  }

  if (mw[ii].message_data_line1)
  {
    XtDestroyWidget(mw[ii].message_data_line1);
  }


  mw[ii].message_data_line1 = (Widget)NULL;
  mw[ii].message_data_line2 = (Widget)NULL;
  mw[ii].message_data_line3 = (Widget)NULL;
  mw[ii].message_data_line4 = (Widget)NULL;


  // Build the new boxes
  build_send_message_input_boxes(ii, hamhud, d700, d7);

#endif  // NO_DYNAMIC_WIDGETS

}





void HamHUD_Msg( Widget UNUSED(widget), XtPointer clientData, XtPointer UNUSED(callData) )
{
  intptr_t ii =(intptr_t)clientData;
  int hamhud;

  hamhud = XmToggleButtonGetState(mw[ii].HamHUD_mode);

  if (hamhud)
  {
    XmToggleButtonSetState(mw[ii].D700_mode,FALSE,FALSE);
    XmToggleButtonSetState(mw[ii].D7_mode,FALSE,FALSE);
  }
  rebuild_send_message_input_boxes(ii, hamhud, 0, 0);
}





void D700_Msg( Widget UNUSED(widget), XtPointer clientData, XtPointer UNUSED(callData) )
{
  intptr_t ii = (intptr_t)clientData;
  int d700;

  d700 = XmToggleButtonGetState(mw[ii].D700_mode);

  if (d700)
  {
    XmToggleButtonSetState(mw[ii].HamHUD_mode,FALSE,FALSE);
    XmToggleButtonSetState(mw[ii].D7_mode,FALSE,FALSE);
  }
  rebuild_send_message_input_boxes(ii, 0, d700, 0);
}





void D7_Msg( Widget UNUSED(widget), XtPointer clientData, XtPointer UNUSED(callData) )
{
  intptr_t ii = (intptr_t)clientData;
  int d7;

  d7 = XmToggleButtonGetState(mw[ii].D7_mode);

  if (d7)
  {
    XmToggleButtonSetState(mw[ii].HamHUD_mode,FALSE,FALSE);
    XmToggleButtonSetState(mw[ii].D700_mode,FALSE,FALSE);
  }
  rebuild_send_message_input_boxes(ii, 0, 0, d7);
}





// Select HamHUD/D7/D700/Normal APRS messaging based on info
// available in the station record.  Change the Send Message dialog
// to match.
//
// Only call this when the Send Message dialog is first constructed
// or when we hit the New/Refresh Call button.  We don't want to
// have the user fighting against this function every sent or
// received packet if this function happens to guess wrong.
//
void select_station_type(int ii)
{
  DataRow *p_station;
  char call_sign[MAX_CALLSIGN+1];
  char *temp_ptr;


  if (mw[ii].send_message_call_data == NULL)
  {
    fprintf(stderr,"messages_gui.c:select_station_type():mw[ii].send_message_call_data is NULL\n");
    return;
  }

  temp_ptr = XmTextFieldGetString(mw[ii].send_message_call_data);
  xastir_snprintf(call_sign,
                  sizeof(call_sign),
                  "%s",
                  temp_ptr);
  XtFree(temp_ptr);

  (void)remove_trailing_spaces(call_sign);
  (void)to_upper(call_sign);

  // We have the callsign.  See if we have the station name in our
  // database.
  if (call_sign[0] != '\0'
      && search_station_name(&p_station, call_sign, 1) )   // Exact match
  {

    int hamhud = 0;
    int d700 = 0;
    int d7 = 0;
//        int tx_only = 0;


//fprintf(stderr,"Found callsign: %s\n", call_sign);

    // check if station appears to be a transmit only station such as the TinyTrak and OpenTrak trackers.
// ********* wrong test **********
//if (p_station->aprs_symbol != NULL) {
//   if (p_station->aprs_symbol.aprs_type != NULL) {
//      fprintf(stderr,"Has Type: %s\n", p_station->aprs_symbol.aprs_type);
//        if is_tx_only(p_station) {
//            tx_only++;
//        }
//   }
//}

    // Check first two comment records, if they exist
    if (p_station->comment_data != NULL)
    {

      // Check first comment record
      if (strstr(p_station->comment_data->text_ptr,"TM-D700"))
      {
        d700++;
      }
      else if (strstr(p_station->comment_data->text_ptr,"TH-D7"))
      {
        d7++;
      }
      else if (p_station->comment_data->next)
      {
        // Check second comment record
        if (strstr(p_station->comment_data->next->text_ptr,"TM-D700"))
        {
          d700++;
        }
        else if (strstr(p_station->comment_data->next->text_ptr,"TH-D7"))
        {
          d7++;
        }
      }
    }

    // If not D700 or D7, check for HamHUD in the TOCALL.
    // APHH2/APRHH2.  We'll skip the version number so-as to
    // catch future versions as well.
    //
    if (!d700 && !d7)
    {
      if (p_station->node_path_ptr)
      {
        if (strncmp(p_station->node_path_ptr,"APRHH",5) == 0
            || strncmp(p_station->node_path_ptr,"APHH",4) == 0)
        {
          hamhud++;
        }
      }
    }


    if (hamhud)
    {
//            fprintf(stderr,"HamHUD found\n");
      XmToggleButtonSetState(mw[ii].HamHUD_mode,TRUE,FALSE);
      XmToggleButtonSetState(mw[ii].D700_mode,FALSE,FALSE);
      XmToggleButtonSetState(mw[ii].D7_mode,FALSE,FALSE);
      rebuild_send_message_input_boxes(ii, hamhud, 0, 0);
    }
    else if (d700)
    {
//            fprintf(stderr,"D700 found\n");
      XmToggleButtonSetState(mw[ii].HamHUD_mode,FALSE,FALSE);
      XmToggleButtonSetState(mw[ii].D700_mode,TRUE,FALSE);
      XmToggleButtonSetState(mw[ii].D7_mode,FALSE,FALSE);
      rebuild_send_message_input_boxes(ii, 0, d700, 0);
    }
    else if (d7)
    {
//            fprintf(stderr,"D7 found\n");
      XmToggleButtonSetState(mw[ii].HamHUD_mode,FALSE,FALSE);
      XmToggleButtonSetState(mw[ii].D700_mode,FALSE,FALSE);
      XmToggleButtonSetState(mw[ii].D7_mode,TRUE,FALSE);
      rebuild_send_message_input_boxes(ii, 0, 0, d7);
    }
    else
    {
//            fprintf(stderr,"Standard APRS found\n");
      XmToggleButtonSetState(mw[ii].HamHUD_mode,FALSE,FALSE);
      XmToggleButtonSetState(mw[ii].D700_mode,FALSE,FALSE);
      XmToggleButtonSetState(mw[ii].D7_mode,FALSE,FALSE);
      rebuild_send_message_input_boxes(ii, 0, 0, d7);
    }
  }
}





// The main Send Message dialog.  db.c:update_messages() is the
// function which fills in the message history information.
//
// The underlying code has been tweaked so that we can pass an EMPTY
// path all the way down through the layers.  We defined special
// strings for that and for setting default paths, and display these
// to the user in this and the Change Path dialogs:
//
//  "DEFAULT PATH"
//  "DIRECT PATH"
//
// A note from Jim Fuller, N7VR:
//
//   "I just set a test setup to my TH-D7GA.  First Display shows
//   two lines 12 characters each, with the a third line of 12
//   character and fourth line of 9 characters on the second display
//   by pressing ok.  This means 45 characters for max message.
//
//   On my D-700, main first display is like the TH-D7GA.  But when
//   I go to the message display it displays three lines.  The first
//   two are 22 characters and the last is 20 characters.  This
//   means 64 characters total.
//
//   I would only send a 24 character message to field troops, based
//   on first read capability."
//
//
void Send_message( Widget UNUSED(w), XtPointer clientData, XtPointer UNUSED(callData) )
{
  Arg args[50];
  char temp[60];
  unsigned int n;
  int j;
  intptr_t i;
  char group[MAX_CALLSIGN+1];
  int groupon;
  int box_len;
  Atom delw;


//fprintf(stderr,"\n1Send_message\n");

  groupon=0;
  box_len=105;
  i=0;

  begin_critical_section(&send_message_dialog_lock, "messages_gui.c:Send_message" );

  for(j=0; j<MAX_MESSAGE_WINDOWS; j++)
  {
    if(!mw[j].send_message_dialog)
    {
      i=j;
      break;
    }
  }

  if(!mw[i].send_message_dialog)
  {

    if (clientData != NULL)
    {
      substr(group,(char *)clientData, MAX_CALLSIGN);
      if (group[0] == '*')
      {
        xastir_snprintf(mw[i].to_call_sign,
                        sizeof(mw[i].to_call_sign),
                        "***");
        mw[i].to_call_sign[3] = '\0';   // Terminate it
        memmove(group, &group[1], strlen(group));
        groupon=1;
        box_len=100;
      }
      else
      {
        xastir_snprintf(mw[i].to_call_sign,
                        sizeof(mw[i].to_call_sign),
                        "%s",
                        my_callsign);
        mw[i].to_call_sign[MAX_CALLSIGN] = '\0';    // Terminate it
      }
    }
    else
    {
      xastir_snprintf(mw[i].to_call_sign,
                      sizeof(mw[i].to_call_sign),
                      "%s",
                      my_callsign);
      mw[i].to_call_sign[MAX_CALLSIGN] = '\0';    // Terminate it
    }

    xastir_snprintf(temp, sizeof(temp), langcode(groupon==0 ? "WPUPMSB001": "WPUPMSB002"),
                    (i+1));

    mw[i].message_group = groupon;
    mw[i].send_message_dialog = XtVaCreatePopupShell(temp,
                                xmDialogShellWidgetClass, appshell,
                                XmNdeleteResponse,XmDESTROY,
                                XmNdefaultPosition, FALSE,
                                XmNtitleString,"Send Message",
                                XmNfontList, fontlist1,
                                NULL);

    mw[i].pane = XtVaCreateWidget("Send_message pane",
                                  xmPanedWindowWidgetClass,
                                  mw[i].send_message_dialog,
                                  MY_FOREGROUND_COLOR,
                                  MY_BACKGROUND_COLOR,
                                  XmNfontList, fontlist1,
                                  NULL);

    mw[i].form =  XtVaCreateWidget("Send_message form",
                                   xmFormWidgetClass,
                                   mw[i].pane,
                                   XmNfractionBase, 5,
                                   XmNautoUnmanage, FALSE,
                                   XmNshadowThickness, 1,
                                   MY_FOREGROUND_COLOR,
                                   MY_BACKGROUND_COLOR,
                                   XmNfontList, fontlist1,
                                   NULL);

// Row 1 (bottom)
    mw[i].button_clear_old_msgs = XtVaCreateManagedWidget(langcode("WPUPMSB007"),
                                  xmPushButtonGadgetClass,
                                  mw[i].form,
                                  XmNleftAttachment, XmATTACH_POSITION,
                                  XmNleftPosition, 0,
                                  XmNrightAttachment, XmATTACH_POSITION,
                                  XmNrightPosition, 1,
                                  XmNtopAttachment, XmATTACH_NONE,
                                  XmNbottomAttachment, XmATTACH_FORM,
                                  XmNbottomOffset, 5,
                                  XmNnavigationType, XmTAB_GROUP,
                                  XmNtraversalOn, TRUE,
                                  MY_FOREGROUND_COLOR,
                                  MY_BACKGROUND_COLOR,
                                  XmNfontList, fontlist1,
                                  NULL);

    mw[i].button_clear_pending_msgs = XtVaCreateManagedWidget(langcode("WPUPMSB011"),
                                      xmPushButtonGadgetClass,
                                      mw[i].form,
                                      XmNleftAttachment, XmATTACH_POSITION,
                                      XmNleftPosition, 1,
                                      XmNrightAttachment, XmATTACH_POSITION,
                                      XmNrightPosition, 2,
                                      XmNtopAttachment, XmATTACH_NONE,
                                      XmNbottomAttachment, XmATTACH_FORM,
                                      XmNbottomOffset, 5,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      MY_FOREGROUND_COLOR,
                                      MY_BACKGROUND_COLOR,
                                      XmNfontList, fontlist1,
                                      NULL);

    mw[i].button_kick_timer = XtVaCreateManagedWidget(langcode("WPUPMSB012"),
                              xmPushButtonGadgetClass,
                              mw[i].form,
                              XmNleftAttachment, XmATTACH_POSITION,
                              XmNleftPosition, 2,
                              XmNrightAttachment, XmATTACH_POSITION,
                              XmNrightPosition, 3,
                              XmNtopAttachment, XmATTACH_NONE,
                              XmNbottomAttachment, XmATTACH_FORM,
                              XmNbottomOffset, 5,
                              XmNnavigationType, XmTAB_GROUP,
                              XmNtraversalOn, TRUE,
                              MY_FOREGROUND_COLOR,
                              MY_BACKGROUND_COLOR,
                              XmNfontList, fontlist1,
                              NULL);

    mw[i].button_ok = XtVaCreateManagedWidget(langcode("WPUPMSB009"),
                      xmPushButtonGadgetClass,
                      mw[i].form,
                      XmNtopAttachment, XmATTACH_NONE,
                      XmNbottomAttachment, XmATTACH_FORM,
                      XmNbottomOffset, 5,
                      XmNleftAttachment, XmATTACH_POSITION,
                      XmNleftPosition, 3,
                      XmNrightAttachment, XmATTACH_POSITION,
                      XmNrightPosition, 4,
                      XmNnavigationType, XmTAB_GROUP,
                      XmNtraversalOn, TRUE,
                      MY_FOREGROUND_COLOR,
                      MY_BACKGROUND_COLOR,
                      XmNfontList, fontlist1,
                      NULL);

    mw[i].button_cancel = XtVaCreateManagedWidget(langcode("UNIOP00003"),
                          xmPushButtonGadgetClass,
                          mw[i].form,
                          XmNtopAttachment, XmATTACH_NONE,
                          XmNbottomAttachment, XmATTACH_FORM,
                          XmNbottomOffset, 5,
                          XmNleftAttachment, XmATTACH_POSITION,
                          XmNleftPosition, 4,
                          XmNrightAttachment, XmATTACH_POSITION,
                          XmNrightPosition, 5,
                          XmNnavigationType, XmTAB_GROUP,
                          XmNtraversalOn, TRUE,
                          MY_FOREGROUND_COLOR,
                          MY_BACKGROUND_COLOR,
                          XmNfontList, fontlist1,
                          NULL);

// Row 2 (1 up from bottom row)

    // If D700 or D7 mode is enabled we break up the message
    // box into 3 or 4 boxes with the lengths appropriate for
    // those devices, else we use a single box which is 67 chars
    // long, the maximum for an APRS message.
    //
    // Size of Kenwood screens:
    // TH-D7A/E: Arranged as 12,12,12,9 (45 chars)
    // TM-D700A: Arranged as 22,22,20   (64 chars)
    //
    // Size of HamHUD screen:
    // 20 char fits on one screen, any more gets scrolled.
    //

// First have radio buttons to change how we'll draw the message
// input widgets:

    mw[i].D7_mode =XtVaCreateManagedWidget("D7",
                                           xmToggleButtonGadgetClass,
                                           mw[i].form,
                                           XmNtopAttachment, XmATTACH_NONE,
                                           XmNbottomAttachment, XmATTACH_WIDGET,
                                           XmNbottomWidget, mw[i].button_clear_old_msgs,
                                           XmNbottomOffset, 0,
                                           XmNleftAttachment, XmATTACH_FORM,
                                           XmNleftOffset, 5,
                                           XmNrightAttachment, XmATTACH_NONE,
                                           XmNvisibleWhenOff, TRUE,
                                           XmNindicatorSize, 12,
                                           XmNnavigationType, XmTAB_GROUP,
                                           XmNtraversalOn, FALSE,
                                           MY_FOREGROUND_COLOR,
                                           MY_BACKGROUND_COLOR,
                                           XmNfontList, fontlist1,
                                           NULL);
#ifndef NO_DYNAMIC_WIDGETS
    XtAddCallback(mw[i].D7_mode,XmNvalueChangedCallback,D7_Msg,(XtPointer)i);
#else   // NO_DYNAMIC_WIDGETS
    XtSetSensitive(mw[i].D7_mode, FALSE);
#endif  // NO_DYNAMIC_WIDGETS

    mw[i].D700_mode =XtVaCreateManagedWidget("D700",
                     xmToggleButtonGadgetClass,
                     mw[i].form,
                     XmNtopAttachment, XmATTACH_NONE,
                     XmNbottomAttachment, XmATTACH_WIDGET,
                     XmNbottomWidget, mw[i].D7_mode,
                     XmNbottomOffset, 0,
                     XmNleftAttachment, XmATTACH_FORM,
                     XmNleftOffset, 5,
                     XmNrightAttachment, XmATTACH_NONE,
                     XmNvisibleWhenOff, TRUE,
                     XmNindicatorSize, 12,
                     XmNnavigationType, XmTAB_GROUP,
                     XmNtraversalOn, FALSE,
                     MY_FOREGROUND_COLOR,
                     MY_BACKGROUND_COLOR,
                     XmNfontList, fontlist1,
                     NULL);

#ifndef NO_DYNAMIC_WIDGETS
    XtAddCallback(mw[i].D700_mode,XmNvalueChangedCallback,D700_Msg,(XtPointer)i);
#else   // NO_DYNAMIC_WIDGETS
    XtSetSensitive(mw[i].D700_mode, FALSE);
#endif  // NO_DYNAMIC_WIDGETS

    mw[i].HamHUD_mode =XtVaCreateManagedWidget("HamHUD",
                       xmToggleButtonGadgetClass,
                       mw[i].form,
                       XmNtopAttachment, XmATTACH_NONE,
                       XmNbottomAttachment, XmATTACH_WIDGET,
                       XmNbottomWidget, mw[i].D700_mode,
                       XmNbottomOffset, 0,
                       XmNleftAttachment, XmATTACH_FORM,
                       XmNleftOffset, 5,
                       XmNrightAttachment, XmATTACH_NONE,
                       XmNvisibleWhenOff, TRUE,
                       XmNindicatorSize, 12,
                       XmNnavigationType, XmTAB_GROUP,
                       XmNtraversalOn, FALSE,
                       MY_FOREGROUND_COLOR,
                       MY_BACKGROUND_COLOR,
                       XmNfontList, fontlist1,
                       NULL);

#ifndef NO_DYNAMIC_WIDGETS
    XtAddCallback(mw[i].HamHUD_mode,XmNvalueChangedCallback,HamHUD_Msg,(XtPointer)i);
#else   // NO_DYNAMIC_WIDGETS
    XtSetSensitive(mw[i].HamHUD_mode, FALSE);
#endif  // NO_DYNAMIC_WIDGETS

    mw[i].message = XtVaCreateManagedWidget(langcode("WPUPMSB008"),
                                            xmLabelWidgetClass,
                                            mw[i].form,
                                            XmNtopAttachment, XmATTACH_NONE,
                                            XmNbottomAttachment, XmATTACH_WIDGET,
                                            XmNbottomWidget, mw[i].button_clear_old_msgs,
                                            XmNbottomOffset, 10,
                                            XmNleftAttachment, XmATTACH_WIDGET,
                                            XmNleftWidget, mw[i].HamHUD_mode,
                                            XmNleftOffset, 3,
                                            XmNrightAttachment, XmATTACH_NONE,
                                            MY_FOREGROUND_COLOR,
                                            MY_BACKGROUND_COLOR,
                                            XmNfontList, fontlist1,
                                            NULL);


// Default:  No D700 or D7 mode selected.  Standard APRS mode.
// Go build the proper input box for standard APRS mode, 67
// chars long.
//
    build_send_message_input_boxes(i, 0, 0, 0);


// Row 3 (2 up from bottom row)
    mw[i].call = XtVaCreateManagedWidget(langcode(groupon == 0  ? "WPUPMSB003": "WPUPMSB004"),
                                         xmLabelWidgetClass, mw[i].form,
                                         XmNtopAttachment, XmATTACH_NONE,
                                         XmNbottomAttachment, XmATTACH_WIDGET,
                                         XmNbottomWidget, mw[i].message,
                                         XmNbottomOffset, 15,
                                         XmNleftAttachment, XmATTACH_WIDGET,
                                         XmNleftWidget, mw[i].HamHUD_mode,
                                         XmNleftOffset, 5,
                                         XmNrightAttachment, XmATTACH_NONE,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);

    mw[i].send_message_call_data = XtVaCreateManagedWidget("Send_message smcd",
                                   xmTextFieldWidgetClass,
                                   mw[i].form,
                                   XmNeditable,   TRUE,
                                   XmNcursorPositionVisible, TRUE,
                                   XmNsensitive, TRUE,
                                   XmNshadowThickness,    1,
                                   XmNcolumns, 10,
                                   XmNwidth, ((10*7)+2),
                                   XmNmaxLength, 10,
                                   XmNbackground, colors[0x0f],
                                   XmNtopAttachment, XmATTACH_NONE,
                                   XmNbottomAttachment, XmATTACH_WIDGET,
                                   XmNbottomWidget, mw[i].message,
                                   XmNbottomOffset, 10,
                                   XmNleftAttachment, XmATTACH_WIDGET,
                                   XmNleftWidget, mw[i].call,
                                   XmNleftOffset, 5,
                                   XmNrightAttachment,XmATTACH_NONE,
                                   XmNnavigationType, XmTAB_GROUP,
                                   XmNtraversalOn, TRUE,
                                   XmNfontList, fontlist1,
                                   NULL);

    xastir_snprintf(temp, sizeof(temp), "%s", langcode(groupon == 0 ? "WPUPMSB005": "WPUPMSB006"));

    mw[i].button_submit_call = XtVaCreateManagedWidget(temp,
                               xmPushButtonGadgetClass,
                               mw[i].form,
                               XmNleftAttachment, XmATTACH_WIDGET,
                               XmNleftWidget, mw[i].send_message_call_data,
                               XmNleftOffset, 10,
                               XmNtopAttachment, XmATTACH_NONE,
                               XmNbottomAttachment, XmATTACH_WIDGET,
                               XmNbottomWidget, mw[i].message,
                               XmNbottomOffset, 12,
                               XmNrightAttachment, XmATTACH_NONE,
                               XmNnavigationType, XmTAB_GROUP,
                               XmNtraversalOn, TRUE,
                               MY_FOREGROUND_COLOR,
                               MY_BACKGROUND_COLOR,
                               XmNfontList, fontlist1,
                               NULL);

    mw[i].path = XtVaCreateManagedWidget(langcode("WPUPMSB010"),
                                         xmLabelWidgetClass, mw[i].form,
                                         XmNtopAttachment, XmATTACH_NONE,
                                         XmNbottomAttachment, XmATTACH_WIDGET,
                                         XmNbottomWidget, mw[i].message,
                                         XmNbottomOffset, 15,
                                         XmNleftAttachment, XmATTACH_WIDGET,
                                         XmNleftWidget, mw[i].button_submit_call,
                                         XmNleftOffset, 10,
                                         XmNrightAttachment, XmATTACH_NONE,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);

    mw[i].send_message_change_path = XtVaCreateManagedWidget(langcode("WPUPMSB019"),
                                     xmPushButtonGadgetClass,
                                     mw[i].form,
                                     XmNleftAttachment, XmATTACH_NONE,
                                     XmNrightAttachment, XmATTACH_FORM,
                                     XmNrightOffset, 5,
                                     XmNtopAttachment, XmATTACH_NONE,
                                     XmNbottomAttachment, XmATTACH_WIDGET,
                                     XmNbottomWidget, mw[i].message,
                                     XmNbottomOffset, 12,
                                     XmNnavigationType, XmTAB_GROUP,
                                     XmNtraversalOn, TRUE,
                                     MY_FOREGROUND_COLOR,
                                     MY_BACKGROUND_COLOR,
                                     XmNfontList, fontlist1,
                                     NULL);

    mw[i].send_message_path = XtVaCreateManagedWidget("Send_message path",
                              xmTextFieldWidgetClass,
                              mw[i].form,
                              XmNeditable,   FALSE,
                              XmNcursorPositionVisible, FALSE,
                              XmNsensitive, TRUE,
                              XmNshadowThickness,    1,
                              XmNcolumns, 26,
                              XmNwidth, ((26*7)+2),
                              XmNmaxLength, 199,
                              XmNbackground, colors[0x0f],
                              XmNtopAttachment, XmATTACH_NONE,
                              XmNbottomAttachment, XmATTACH_WIDGET,
                              XmNbottomWidget, mw[i].message,
                              XmNbottomOffset, 10,
                              XmNleftAttachment, XmATTACH_WIDGET,
                              XmNleftWidget, mw[i].path,
                              XmNleftOffset, 10,
                              XmNrightAttachment,XmATTACH_WIDGET,
                              XmNrightWidget, mw[i].send_message_change_path,
                              XmNrightOffset, 10,
                              XmNnavigationType, XmTAB_GROUP,
                              XmNtraversalOn, FALSE,
                              MY_FOREGROUND_COLOR,
                              MY_BACKGROUND_COLOR,
                              XmNfontList, fontlist1,
                              NULL);

// Row 4 (3 up from bottom row).  Message area.
    n=0;
    XtSetArg(args[n], XmNrows, 10);
    n++;
    XtSetArg(args[n], XmNmaxHeight, 170);
    n++;
    XtSetArg(args[n], XmNcolumns, box_len);
    n++;
    XtSetArg(args[n], XmNeditable, FALSE);
    n++;
    XtSetArg(args[n], XmNtraversalOn, FALSE);
    n++;
    XtSetArg(args[n], XmNeditMode, XmMULTI_LINE_EDIT);
    n++;
    XtSetArg(args[n], XmNwordWrap, TRUE);
    n++;
    XtSetArg(args[n], XmNshadowThickness, 3);
    n++;
    XtSetArg(args[n], XmNscrollHorizontal, FALSE);
    n++;
    XtSetArg(args[n], XmNcursorPositionVisible, FALSE);
    n++;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM);
    n++;
    XtSetArg(args[n], XmNtopOffset, 5);
    n++;
    XtSetArg(args[n], XmNbottomAttachment,XmATTACH_WIDGET);
    n++;
    XtSetArg(args[n], XmNbottomWidget,mw[i].call);
    n++;
    XtSetArg(args[n], XmNbottomOffset, 10);
    n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM);
    n++;
    XtSetArg(args[n], XmNleftOffset, 5);
    n++;
    XtSetArg(args[n], XmNrightAttachment,XmATTACH_FORM);
    n++;
    XtSetArg(args[n], XmNrightOffset,5);
    n++;
    XtSetArg(args[n], XmNautoShowCursorPosition, FALSE);
    n++;
    XtSetArg(args[n], XmNforeground, MY_FG_COLOR);
    n++;
    XtSetArg(args[n], XmNfontList, fontlist1);
    n++;

// This one causes segfaults, why?  Answer: args[] was set to 20
// (too small)
//        XtSetArg(args[n], XmNbackground, MY_BG_COLOR); n++;

    mw[i].send_message_text = XmCreateScrolledText(mw[i].form,
                              "Send_message smt",
                              args,
                              n);

    xastir_snprintf(mw[i].win, sizeof(mw[i].win), "%ld", i);

    XtAddCallback(mw[i].send_message_change_path, XmNactivateCallback, Send_message_change_path, (XtPointer)mw[i].win);

    XtAddCallback(mw[i].button_ok, XmNactivateCallback, Send_message_now, (XtPointer)mw[i].win);

    XtAddCallback(mw[i].button_cancel, XmNactivateCallback, Send_message_destroy_shell, (XtPointer)mw[i].win);

// Note group messages isn't implemented fully yet.  When it is, the following might have
// to change again:
//        XtAddCallback(mw[i].button_clear_old_msgs, XmNactivateCallback, groupon == 0 ? Clear_message_from: Clear_message_to,
//                (XtPointer)mw[i].win);
    XtAddCallback(mw[i].button_clear_old_msgs, XmNactivateCallback, Clear_message_to_from, (XtPointer)mw[i].win);

    XtAddCallback(mw[i].button_clear_pending_msgs, XmNactivateCallback, Clear_messages_to, (XtPointer)mw[i].win);

    XtAddCallback(mw[i].button_kick_timer, XmNactivateCallback, Kick_timer, (XtPointer)mw[i].win);

    XtAddCallback(mw[i].button_submit_call, XmNactivateCallback, Check_new_call_messages, (XtPointer)i);

    if (clientData != NULL)
    {
      XmTextFieldSetString(mw[i].send_message_call_data, group);
    }

    // Set "DEFAULT PATH" into the path field
    XmTextFieldSetString(mw[i].send_message_path,"DEFAULT PATH");

    pos_dialog(mw[i].send_message_dialog);

    delw = XmInternAtom(XtDisplay(mw[i].send_message_dialog),"WM_DELETE_WINDOW", FALSE);
    XmAddWMProtocolCallback(mw[i].send_message_dialog, delw, Send_message_destroy_shell, (XtPointer)mw[i].win);

    XtManageChild(mw[i].form);
    XtManageChild(mw[i].send_message_text);
    XtVaSetValues(mw[i].send_message_text, XmNbackground, colors[0x0f], NULL);
    XtManageChild(mw[i].pane);

    XtPopup(mw[i].send_message_dialog,XtGrabNone);

    // Move focus to the Cancel button.  This appears to highlight the
    // button fine, but we're not able to hit the <Enter> key to
    // have that default function happen.  Note:  We _can_ hit the
    // <SPACE> key, and that activates the option.
//        XmUpdateDisplay(mw[i].send_message_dialog);
    XmProcessTraversal(mw[i].button_cancel, XmTRAVERSE_CURRENT);

  }

//fprintf(stderr,"2calling select_station_type()\n");

  // Re-arrange the outgoing message boxes based on the type of
  // device we're talking to.
  select_station_type(i);

//fprintf(stderr,"2returned from select_station_type()\n");
//fprintf(stderr,"1end of Send_message()\n");

  end_critical_section(&send_message_dialog_lock, "messages_gui.c:Send_message" );

}





// Bring up a Send Message dialog for each QSO that has pending
// outbound messages.
//
void Show_pending_messages( Widget UNUSED(w), XtPointer UNUSED(clientData), XtPointer UNUSED(callData) )
{
  intptr_t ii;
  int msgs_found = 0;


  // Look through the outgoing message queue.  Find all callsigns
  // that we're currently trying to send messages to.
  //
  for (ii = 0; ii < MAX_OUTGOING_MESSAGES; ii++)
  {

    // If it matches the callsign we're talking to
    if (message_pool[ii].active==MESSAGE_ACTIVE)
    {

      msgs_found++;

      // Bring up a Send Message box for each callsign found.
      Send_message_call(NULL, message_pool[ii].to_call_sign, NULL);

      // Fill in the old data in case it doesn't auto-fill
      Check_new_call_messages(NULL, (XtPointer)ii, NULL);
    }
  }

  if (msgs_found == 0)
  {
    fprintf(stderr, "No Pending Messages.\n");
  }
}





/************************* Auto msg **************************************/
/*************************************************************************/
void Auto_msg_option( Widget UNUSED(w), XtPointer clientData, XtPointer UNUSED(calldata) )
{
  int item_no = XTPOINTER_TO_INT(clientData);

  if (item_no)
  {
    auto_reply = 1;
  }
  else
  {
    auto_reply = 0;
  }
}





void Auto_msg_destroy_shell( Widget UNUSED(widget), XtPointer clientData, XtPointer UNUSED(callData) )
{
  Widget shell = (Widget) clientData;
  XtPopdown(shell);

  begin_critical_section(&auto_msg_dialog_lock, "messages_gui.c:Auto_msg_destroy_shell" );

  XtDestroyWidget(shell);
  auto_msg_dialog = (Widget)NULL;

  end_critical_section(&auto_msg_dialog_lock, "messages_gui.c:Auto_msg_destroy_shell" );

}





void Auto_msg_set_now(Widget w, XtPointer clientData, XtPointer callData)
{
  char temp[110];
  char *temp_ptr;


  temp_ptr = XmTextFieldGetString(auto_msg_set_data);
  substr(temp, temp_ptr, 99);
  XtFree(temp_ptr);

  (void)remove_trailing_spaces(temp);
  memcpy(auto_reply_message, temp, sizeof(auto_reply_message));
  auto_reply_message[sizeof(auto_reply_message)-1] = '\0';  // Terminate string
  Auto_msg_destroy_shell(w, clientData, callData);
}





void Auto_msg_set( Widget UNUSED(w), XtPointer UNUSED(clientData), XtPointer UNUSED(callData) )
{
  static Widget  pane, my_form, button_ok, button_cancel, reply;
  Atom delw;

  begin_critical_section(&auto_msg_dialog_lock, "messages_gui.c:Auto_msg_set" );

  if(!auto_msg_dialog)
  {

    auto_msg_dialog = XtVaCreatePopupShell(langcode("WPUPARM001"),
                                           xmDialogShellWidgetClass, appshell,
                                           XmNdeleteResponse,XmDESTROY,
                                           XmNdefaultPosition, FALSE,
                                           XmNfontList, fontlist1,
                                           NULL);

    pane = XtVaCreateWidget("Auto_msg_set pane",
                            xmPanedWindowWidgetClass,
                            auto_msg_dialog,
                            MY_FOREGROUND_COLOR,
                            MY_BACKGROUND_COLOR,
                            XmNfontList, fontlist1,
                            NULL);

    my_form =  XtVaCreateWidget("Auto_msg_set my_form",
                                xmFormWidgetClass,
                                pane,
                                XmNfractionBase, 5,
                                XmNautoUnmanage, FALSE,
                                XmNshadowThickness, 1,
                                MY_FOREGROUND_COLOR,
                                MY_BACKGROUND_COLOR,
                                XmNfontList, fontlist1,
                                NULL);

    reply = XtVaCreateManagedWidget(langcode("WPUPARM002"),
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
                                    XmNfontList, fontlist1,
                                    NULL);

    auto_msg_set_data = XtVaCreateManagedWidget("Auto_msg_set auto_msg_set_data",
                        xmTextFieldWidgetClass,
                        my_form,
                        XmNeditable,   TRUE,
                        XmNcursorPositionVisible, TRUE,
                        XmNsensitive, TRUE,
                        XmNshadowThickness,    1,
                        XmNcolumns, 40,
                        XmNwidth, ((40*7)+2),
                        XmNmaxLength, 100,
                        XmNbackground, colors[0x0f],
                        XmNtopAttachment,XmATTACH_FORM,
                        XmNtopOffset, 5,
                        XmNbottomAttachment,XmATTACH_NONE,
                        XmNleftAttachment, XmATTACH_WIDGET,
                        XmNleftWidget, reply,
                        XmNleftOffset, 10,
                        XmNrightAttachment,XmATTACH_FORM,
                        XmNrightOffset, 5,
                        XmNnavigationType, XmTAB_GROUP,
                        XmNtraversalOn, TRUE,
                        XmNfontList, fontlist1,
                        NULL);

    button_ok = XtVaCreateManagedWidget(langcode("UNIOP00001"),
                                        xmPushButtonGadgetClass,
                                        my_form,
                                        XmNtopAttachment, XmATTACH_WIDGET,
                                        XmNtopWidget, auto_msg_set_data,
                                        XmNtopOffset, 10,
                                        XmNbottomAttachment, XmATTACH_FORM,
                                        XmNbottomOffset, 5,
                                        XmNleftAttachment, XmATTACH_POSITION,
                                        XmNleftPosition, 1,
                                        XmNrightAttachment, XmATTACH_POSITION,
                                        XmNrightPosition, 2,
                                        XmNnavigationType, XmTAB_GROUP,
                                        XmNtraversalOn, TRUE,
                                        MY_FOREGROUND_COLOR,
                                        MY_BACKGROUND_COLOR,
                                        XmNfontList, fontlist1,
                                        NULL);

    button_cancel = XtVaCreateManagedWidget(langcode("UNIOP00002"),
                                            xmPushButtonGadgetClass,
                                            my_form,
                                            XmNtopAttachment, XmATTACH_WIDGET,
                                            XmNtopWidget, auto_msg_set_data,
                                            XmNtopOffset, 10,
                                            XmNbottomAttachment, XmATTACH_FORM,
                                            XmNbottomOffset, 5,
                                            XmNleftAttachment, XmATTACH_POSITION,
                                            XmNleftPosition, 3,
                                            XmNrightAttachment, XmATTACH_POSITION,
                                            XmNrightPosition, 4,
                                            XmNnavigationType, XmTAB_GROUP,
                                            XmNtraversalOn, TRUE,
                                            MY_FOREGROUND_COLOR,
                                            MY_BACKGROUND_COLOR,
                                            XmNfontList, fontlist1,
                                            NULL);

    XtAddCallback(button_ok, XmNactivateCallback, Auto_msg_set_now, auto_msg_dialog);
    XtAddCallback(button_cancel, XmNactivateCallback, Auto_msg_destroy_shell, auto_msg_dialog);

    pos_dialog(auto_msg_dialog);

    delw = XmInternAtom(XtDisplay(auto_msg_dialog),"WM_DELETE_WINDOW", FALSE);
    XmAddWMProtocolCallback(auto_msg_dialog, delw, Auto_msg_destroy_shell, (XtPointer)auto_msg_dialog);
    XmTextFieldSetString(auto_msg_set_data,auto_reply_message);

    XtManageChild(my_form);
    XtManageChild(pane);

    XtPopup(auto_msg_dialog,XtGrabNone);
    fix_dialog_size(auto_msg_dialog);

    // Move focus to the Cancel button.  This appears to highlight the
    // button fine, but we're not able to hit the <Enter> key to
    // have that default function happen.  Note:  We _can_ hit the
    // <SPACE> key, and that activates the option.
//        XmUpdateDisplay(auto_msg_dialog);
    XmProcessTraversal(button_cancel, XmTRAVERSE_CURRENT);

  }
  else
  {
    (void)XRaiseWindow(XtDisplay(auto_msg_dialog), XtWindow(auto_msg_dialog));
  }

  end_critical_section(&auto_msg_dialog_lock, "messages_gui.c:Auto_msg_set" );

}


