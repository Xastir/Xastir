/*
 * $Id$
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2006  The Xastir Group
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

#include <Xm/XmAll.h>

#include "xastir.h"
#include "main.h"
#include "lang.h"

// Must be last include file
#include "leak_detection.h"



#define MAX_PATH 200


Widget auto_msg_on, auto_msg_off;
Widget auto_msg_dialog = (Widget)NULL;
Widget auto_msg_set_data = (Widget)NULL;

static xastir_mutex auto_msg_dialog_lock;
xastir_mutex send_message_dialog_lock;





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
void reverse_path(char *input_string) {
    char reverse_path[200];
    int indexes[20];
    int i, j, len;
    char temp[MAX_CALLSIGN+1];


    // Check for NULL pointer
    if (input_string == NULL)
        return;

    // Check for zero-length string
    len = strlen(input_string);
    if (len == 0)
        return;

    // Initialize
    reverse_path[0] = '\0';
    for (i = 0; i < 20; i++)
        indexes[i] = -1;

    // Add a comma onto the end (makes later code easier)
    input_string[len++] = ',';
    input_string[len] = '\0';   // Terminate it

    // Find each comma 
    j = 0; 
    for (i = 0; i < (int)strlen(input_string); i++) {
        if (input_string[i] == ',') {
            indexes[j++] = i;
            //fprintf(stderr,"%d\n",i);     // Debug code
        }
    }

    // Get rid of asterisks and commas in the original string:
    for (i = 0; i < len; i++) {
        if (input_string[i] == '*' || input_string[i] == ',') {
            input_string[i] = '\0';
        }
    }

    // Go left to right looking for a 3-letter callsign starting
    // with 'q'.  If found readjust 'j' to skip that callsign and
    // everything after it.
    //
    for ( i = 0; i < j; i++) {
        char *c = &input_string[indexes[i] + 1];

//fprintf(stderr,"'%s'\t", c );

        if (c[0] == 'q') {
            if ( strlen(c) == 3 ) { // "qAR"

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
    for ( i = j - 1; i >= 0; i-- ) {
        if ( (input_string[indexes[i]+1] != '\0')
                && (strncasecmp(&input_string[indexes[i]+1],"RELAY",5) != 0)
                && (strncasecmp(&input_string[indexes[i]+1],"TCPIP",5) != 0) ) {

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
            if (strncasecmp(temp,"WIDE",4) == 0) {
                if ( (temp[4] != ',') && is_num_chr(temp[4]) ) {
//fprintf(stderr,"Found a WIDEn-N\n");
                    xastir_snprintf(temp,
                        sizeof(temp),
                        "WIDE%c-%c",
                        temp[4],
                        temp[4]);
                }
                else {
//fprintf(stderr,"Found a WIDE\n");
                    // Leave temp alone, it's just a WIDE
                }
            }
            else if (strncasecmp(temp,"TRACE",5) == 0) {
                if ( (temp[5] != ',') && is_num_chr(temp[5]) ) {
//fprintf(stderr,"Found a TRACEn-N\n");
                    xastir_snprintf(temp,
                        sizeof(temp),
                        "WIDE%c-%c",
                        temp[5],
                        temp[5]);
                }
                else {
//fprintf(stderr,"Found a TRACE\n");
                    // Convert it from TRACE to WIDE
                    xastir_snprintf(temp,
                        sizeof(temp),
                        "WIDE");
                }
            }

            // Add temp to the end of our path:
            strncat(reverse_path,temp,sizeof(temp));
 
            strncat(reverse_path,",",1);
        }
    }

    // Remove the ending comma
    reverse_path[strlen(reverse_path) - 1] = '\0';

    // Save the new path back into the string we were given.
    strncat(input_string, reverse_path, len);
}





void get_path_data(char *callsign, char *path, int max_length) {
    DataRow *p_station;


    if (search_station_name(&p_station,callsign,1)) {  // Found callsign
        char new_path[200];

        if (p_station->node_path_ptr) {
            xastir_snprintf(new_path,sizeof(new_path),p_station->node_path_ptr);
            
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
        } else {
            if (debug_level & 2) 
                fprintf(stderr," Path from %s is (null)\n",callsign);
            path[0]='\0';
        }
    }
    else {  // Couldn't find callsign.  It's
            // not in our station database.
        if(debug_level & 2)
            fprintf(stderr,"Path from %s: No Path Known\n",callsign);
        
        path[0] = '\0';
    }

}





static Widget change_path_dialog = NULL;
static Widget current_path = NULL;
 




void Send_message_change_path_destroy_shell(Widget widget, XtPointer clientData, XtPointer callData) {

    if (change_path_dialog) {
        XtPopdown(change_path_dialog);

        XtDestroyWidget(change_path_dialog);
    }
    change_path_dialog = (Widget)NULL;
}





// Apply button
// Fetch the text from the "current_path" widget and place it into
// the mw[ii].send_message_path widget.
//
void Send_message_change_path_apply(Widget widget, XtPointer clientData, XtPointer callData) {
    char path[MAX_PATH+1];
    char *temp_ptr;

 
    if (current_path != NULL && clientData != NULL) {

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
// Put "DIRECT PATH" in the widgets.  Change the lower-level code to
// recognize a path of "DIRECT PATH" and change to an empty path
// when the transmit actually goes out.  An alternative would be to
// keep "DIRECT PATH" in there all the way to the transmit routines,
// then change it there.
//
void Send_message_change_path_direct(Widget widget, XtPointer clientData, XtPointer callData) {

    if (current_path == NULL || clientData == NULL) {
        Send_message_change_path_destroy_shell(NULL, NULL, NULL);
    }

    // Change current_path widget
    XmTextFieldSetString(current_path, "DIRECT PATH");

    Send_message_change_path_apply(NULL, clientData, NULL);
}
 




// "Default Path(s)" button
//
// Blank out the path so the default paths get used.  We could
// perhaps put something like "*Default Path*" in the widgets.  An
// alternative would be to keep "DEFAULT PATH" in there all the way
// to the transmit routines, then change it there to be a blank.
//
void Send_message_change_path_default(Widget widget, XtPointer clientData, XtPointer callData) {

    if (current_path == NULL || clientData == NULL) {
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
void Send_message_change_path( Widget widget, XtPointer clientData, XtPointer callData) {
    int ii;
    Atom delw;
    Widget pane, form, current_path_label, reverse_path_label,
        reverse_path, button_default, button_direct, button_apply,
        button_cancel;
    char *temp_ptr;
    char temp1[MAX_LINE_SIZE+1];
    char path[MAX_PATH+1];
 

//begin_critical_section(&send_message_dialog_lock, "messages_gui.c:Send_message_change_path" );

    if (clientData == NULL) {
        return;
    }

    if (change_path_dialog) {
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
                NULL);

    // "Use Default Path(s)"
    button_default = XtVaCreateManagedWidget(langcode("WPUPMSB020"),
                xmPushButtonGadgetClass, 
                form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, current_path_label,
                XmNtopOffset, 10,
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
                NULL);

    // "Direct (No path)"
    button_direct = XtVaCreateManagedWidget(langcode("WPUPMSB021"),
                xmPushButtonGadgetClass, 
                form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, current_path_label,
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
                NULL);

    // "Apply"
    button_apply = XtVaCreateManagedWidget(langcode("UNIOP00032"),
                xmPushButtonGadgetClass, 
                form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, current_path_label,
                XmNtopOffset, 10,
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
                NULL);

    // "Close"
    button_cancel = XtVaCreateManagedWidget(langcode("UNIOP00003"),
                xmPushButtonGadgetClass, 
                form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, current_path_label,
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
                NULL);

    XtAddCallback(button_default, XmNactivateCallback, Send_message_change_path_default, (XtPointer)mw[ii].send_message_path);
    XtAddCallback(button_direct, XmNactivateCallback, Send_message_change_path_direct, (XtPointer)mw[ii].send_message_path);
    XtAddCallback(button_apply, XmNactivateCallback, Send_message_change_path_apply, (XtPointer)mw[ii].send_message_path);
    XtAddCallback(button_cancel, XmNactivateCallback, Send_message_change_path_destroy_shell,(XtPointer)mw[ii].win);

    // Fill in the fields 
    if(mw[ii].send_message_dialog != NULL) {
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
        if (strlen(path) == 0) {
            // Try uppercase
            (void)to_upper(call_sign);
            get_path_data(call_sign, path, MAX_PATH);
        }
        XmTextFieldSetString(reverse_path, path);
    }
 
    pos_dialog(change_path_dialog);

    delw = XmInternAtom(XtDisplay(change_path_dialog),"WM_DELETE_WINDOW", FALSE);
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
void get_send_message_path(char *callsign, char *path, int path_size) {
    int ii;
    int found = -1;
    char *temp_ptr;
    char temp1[MAX_LINE_SIZE+1];
    char my_callsign[20];
    

    xastir_snprintf(my_callsign,sizeof(my_callsign),"%s",callsign); 
    remove_trailing_spaces(my_callsign);

//fprintf(stderr,"Looking for %s\n", my_callsign);
    for(ii = 0; ii < MAX_MESSAGE_WINDOWS; ii++) {

        // find matching callsign
        if(mw[ii].send_message_dialog != NULL) {

            temp_ptr = XmTextFieldGetString(mw[ii].send_message_call_data);
            xastir_snprintf(temp1,
                sizeof(temp1),
                "%s",
                temp_ptr);
            XtFree(temp_ptr);

            (void)remove_trailing_dash_zero(temp1);
            (void)to_upper(temp1);

            if(strcmp(temp1,my_callsign)==0) {
                found = ii;
                break;
            }
        }
    }

    if (found == -1) {
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
    if (temp1[0] == '\0') {
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





void Send_message_destroy_shell( /*@unused@*/ Widget widget, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    int ii;
//    char *temp_ptr;
//    char temp1[MAX_LINE_SIZE+1];
 
    ii=atoi((char *)clientData);

begin_critical_section(&send_message_dialog_lock, "messages_gui.c:Send_message_destroy_shell" );

    if (mw[ii].send_message_dialog) {

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
    mw[ii].send_message_message_data = (Widget)NULL;
    mw[ii].send_message_text = (Widget)NULL;

    Send_message_change_path_destroy_shell(NULL, NULL, NULL);
 
end_critical_section(&send_message_dialog_lock, "messages_gui.c:Send_message_destroy_shell" );

}





void Check_new_call_messages( /*@unused@*/ Widget w, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    int i, pos;


    i=atoi((char *)clientData);
    /* clear window*/
    pos=0;

begin_critical_section(&send_message_dialog_lock, "messages_gui.c:Check_new_call_messages" );

    if (mw[i].send_message_dialog) {
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
}





void Clear_messages( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer callData) {
    clear_outgoing_messages();
}





void Send_message_now( /*@unused@*/ Widget w, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    char temp1[MAX_CALLSIGN+1];
    char temp2[302];
    char path[200];
    int ii, jj;
    char *temp_ptr;
    int substitution_made = 0;


    ii=atoi((char *)clientData);

begin_critical_section(&send_message_dialog_lock, "messages_gui.c:Send_message_now" );

    if (mw[ii].send_message_dialog) {

        temp_ptr = XmTextFieldGetString(mw[ii].send_message_call_data);
        xastir_snprintf(temp1,
            sizeof(temp1),
            "%s",
            temp_ptr);
        XtFree(temp_ptr);

        (void)remove_trailing_spaces(temp1);
        (void)to_upper(temp1);
        (void)remove_trailing_dash_zero(temp1);

        temp_ptr = XmTextFieldGetString(mw[ii].send_message_message_data);
        xastir_snprintf(temp2,
            sizeof(temp2),
            "%s",
            temp_ptr);
        XtFree(temp_ptr);

        // We have the message text now.  Check it for illegal
        // characters, remove them and substitute spaces if found.
        // Illegal characters are '|', '{', and '~' for messaging.
        for (jj = 0; jj < (int)strlen(temp2); jj++) {
            if (       temp2[jj] == '|'
                    || temp2[jj] == '{'
                    || temp2[jj] == '~' ) {
                temp2[jj] = '.';    // Replace with a dot
                substitution_made++;
            }
        }
        if (substitution_made) {
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
                && (strcmp(temp1,my_callsign) ) ) {     // And not my own callsign
            /* if you're sending a message you must be at the keyboard */
            auto_reply=0;
            XmToggleButtonSetState(auto_msg_toggle,FALSE,FALSE);
            statusline(langcode("BBARSTA011"),0);       // Auto Reply Messages OFF
            output_message(mw[ii].to_call_sign,temp1,temp2,path);
            XmTextFieldSetString(mw[ii].send_message_message_data,"");
//            if (mw[ii].message_group!=1)
//                XtSetSensitive(mw[ii].button_ok,FALSE);
        }
        else {
            // Could add a popup here someday that says something about
            // either the callsign or the message were blank, or we're
            // trying to talk to ourselves.
        }
    }

end_critical_section(&send_message_dialog_lock, "messages_gui.c:Send_message_now" );

}





void Clear_message_from( /*@unused@*/ Widget w, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    char temp1[MAX_CALLSIGN+1];
    int i;
    char *temp_ptr;
/* int pos;*/


    i=atoi((char *)clientData);

begin_critical_section(&send_message_dialog_lock, "messages_gui.c:Clear_message_from" );

    if (mw[i].send_message_dialog) {

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





void Clear_message_to( /*@unused@*/ Widget w, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    char temp1[MAX_CALLSIGN+1];
    int i;
    char *temp_ptr;


    i=atoi((char *)clientData);

begin_critical_section(&send_message_dialog_lock, "messages_gui.c:Clear_message_to" );

    if (mw[i].send_message_dialog) {

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





void Clear_message_to_from( /*@unused@*/ Widget w, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    int i, pos;

    Clear_message_to(w, clientData, callData);
    Clear_message_from(w, clientData, callData);

    i=atoi((char *)clientData);
    /* clear window*/
    pos=0;

begin_critical_section(&send_message_dialog_lock, "messages_gui.c:Clear_message_to_from" );

    if (mw[i].send_message_dialog) {

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





void Kick_timer( /*@unused@*/ Widget w, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    char *temp_ptr;
    char temp1[MAX_CALLSIGN+1];


    temp_ptr = XmTextFieldGetString(mw[atoi(clientData)].send_message_call_data);
    xastir_snprintf(temp1,
        sizeof(temp1),
        "%s",
        temp_ptr);
    XtFree(temp_ptr);

    (void)remove_trailing_dash_zero(temp1);

    kick_outgoing_timer(temp1);
}





void Clear_messages_to( /*@unused@*/ Widget w, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    char *temp_ptr;
    char temp1[MAX_CALLSIGN+1];


    temp_ptr = XmTextFieldGetString(mw[atoi(clientData)].send_message_call_data);
        xastir_snprintf(temp1,
            sizeof(temp1),
            "%s",
            temp_ptr);
    XtFree(temp_ptr);

    (void)remove_trailing_dash_zero(temp1);

    clear_outgoing_messages_to(temp1);
    update_messages(1); // Force an update to the window
}





void Send_message_call( /*@unused@*/ Widget w, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    char call[20];

    if(clientData !=NULL) {
        xastir_snprintf(call,
            sizeof(call),
            "%s",
            (char *)clientData);
        Send_message(appshell, call, NULL);
    }           
}





// The main Send Message dialog.  db.c:update_messages() is the
// function which fills in the message history information.
//
// TODO:  Change the "Path:" box so that clicking or double-clicking
// on it will bring up a "Change Path" dialog.  Could also use a
// "Change" or "Change Path" button if easier.  This new dialog
// should have the current path (editable), the reverse path (not
// editable), and these buttons:
//
//      "Set EMPTY path"
//      "Set DEFAULT path"
//      "Apply" or "Apply New Path"
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
// Message dialog
//
// Adding this new CHANGE PATH dialog will allow us to get rid of
// three bugs on the active bug-list:  #1499820, #1326975, and
// #1326973.
//
void Send_message( /*@unused@*/ Widget w, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    Arg args[50];
    char temp[60];
    unsigned int n;
    int j,i;
    char group[MAX_CALLSIGN+1];
    int groupon;
    int box_len;
    Atom delw;
//    DataRow *p_station;


//fprintf(stderr,"Send_message\n");

    groupon=0;
    box_len=90;
    i=0;

begin_critical_section(&send_message_dialog_lock, "messages_gui.c:Send_message" );

    for(j=0; j<MAX_MESSAGE_WINDOWS; j++) {
        if(!mw[j].send_message_dialog) {
            i=j;
            break;
        }
    }

    if(!mw[i].send_message_dialog) {

        if (clientData != NULL) {
            substr(group,(char *)clientData, MAX_CALLSIGN);
            if (group[0] == '*') {
                xastir_snprintf(mw[i].to_call_sign,
                    sizeof(mw[i].to_call_sign),
                    "***");
                mw[i].to_call_sign[3] = '\0';   // Terminate it
                memmove(group, &group[1], strlen(group));
                groupon=1;
                box_len=100;
            } else {
                xastir_snprintf(mw[i].to_call_sign,
                    sizeof(mw[i].to_call_sign),
                    "%s",
                    my_callsign);
                mw[i].to_call_sign[MAX_CALLSIGN] = '\0';    // Terminate it
            }
        } else {
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
                NULL);

        mw[i].pane = XtVaCreateWidget("Send_message pane",
                xmPanedWindowWidgetClass, 
                mw[i].send_message_dialog,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        mw[i].form =  XtVaCreateWidget("Send_message form",
                xmFormWidgetClass,
                mw[i].pane,
                XmNfractionBase, 5,
                XmNautoUnmanage, FALSE,
                XmNshadowThickness, 1,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
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
                NULL);

// Row 2 (1 up from bottom row)
        mw[i].message = XtVaCreateManagedWidget(langcode("WPUPMSB008"),
                xmLabelWidgetClass, 
                mw[i].form,
                XmNtopAttachment, XmATTACH_NONE,
                XmNbottomAttachment, XmATTACH_WIDGET,
                XmNbottomWidget, mw[i].button_clear_old_msgs,
                XmNbottomOffset, 10,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        mw[i].send_message_message_data = XtVaCreateManagedWidget("Send_message smmd", 
//                xmTextFieldWidgetClass, 
                xmTextWidgetClass, 
                mw[i].form,
                XmNeditable,   TRUE,
                XmNcursorPositionVisible, TRUE,
                XmNsensitive, TRUE,
                XmNshadowThickness,    1,
                XmNwordWrap, FALSE,
                XmNscrollHorizontal, FALSE,
                XmNscrollVertical, FALSE,
//
// 67 is max length of APRS message.
// Size of Kenwood screens:
// TH-D7A/E: Arranged as 12,12,12,9 (45 chars) 
// TM-D700A: Arranged as 22,22,20   (64 chars)
//
// Standard APRS:
                XmNcolumns, 67,
                XmNwidth, ((67*7)+2),
                XmNmaxLength, 67,
                XmNeditMode, XmSINGLE_LINE_EDIT,
//
// Kenwood TH-D7A/E:
//                XmNcolumns, 12,
//                XmNrows, 4,
//                XmNminHeight, 4,
//                XmNmaxHeight, 4,
//                XmNwidth, ((12*7)+2),
//                XmNmaxLength, 45,
//                XmNeditMode, XmMULTI_LINE_EDIT,
//
// Kenwood TM-D700A:
//                XmNcolumns, 22,
//                XmNminWidth, 22,
//                XmNmaxWidth, 22,
//                XmNwidth, ((22*7)),
//                XmNrows, 3,
//                XmNminHeight, 3,
//                XmNmaxHeight, 3,
//                XmNmaxLength, 64,
//                XmNeditMode, XmMULTI_LINE_EDIT,
//
//
                XmNbackground, colors[0x0f],
                XmNtopAttachment, XmATTACH_NONE,
                XmNbottomAttachment, XmATTACH_WIDGET,
                XmNbottomWidget, mw[i].button_clear_old_msgs,
                XmNbottomOffset, 5,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, mw[i].message,
                XmNleftOffset, 10,
                XmNrightAttachment,XmATTACH_NONE,
                XmNnavigationType, XmTAB_GROUP,
                XmNtraversalOn, TRUE,
                NULL);



// mw[i].send_message_message_data2 = XtVaCreateManagedWidget("Send_message smmd", 
// mw[i].send_message_message_data3 = XtVaCreateManagedWidget("Send_message smmd", 
// mw[i].send_message_message_data4 = XtVaCreateManagedWidget("Send_message smmd", 
// Could perhaps have a Kenwood preview window that shows how it
// would be formatted for TM-D700A/TH-D7A/TH-D7E, but leave the
// input field as a single TextFieldWidget.


 
// Row 3 (2 up from bottom row)
        mw[i].call = XtVaCreateManagedWidget(langcode(groupon == 0  ? "WPUPMSB003": "WPUPMSB004"),
                xmLabelWidgetClass, mw[i].form,
                XmNtopAttachment, XmATTACH_NONE,
                XmNbottomAttachment, XmATTACH_WIDGET,
                XmNbottomWidget, mw[i].message,
                XmNbottomOffset, 10,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
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
                XmNbottomOffset, 5,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, mw[i].call,
                XmNleftOffset, 5,
                XmNrightAttachment,XmATTACH_NONE,
                XmNnavigationType, XmTAB_GROUP,
                XmNtraversalOn, TRUE,
                NULL);

        xastir_snprintf(temp, sizeof(temp), langcode(groupon == 0 ? "WPUPMSB005": "WPUPMSB006"));

        mw[i].button_submit_call = XtVaCreateManagedWidget(temp,
                xmPushButtonGadgetClass, 
                mw[i].form,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, mw[i].send_message_call_data,
                XmNleftOffset, 10,
                XmNtopAttachment, XmATTACH_NONE,
                XmNbottomAttachment, XmATTACH_WIDGET,
                XmNbottomWidget, mw[i].message,
                XmNbottomOffset, 7,
                XmNrightAttachment, XmATTACH_NONE,
                XmNnavigationType, XmTAB_GROUP,
                XmNtraversalOn, TRUE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        mw[i].path = XtVaCreateManagedWidget(langcode("WPUPMSB010"),
                xmLabelWidgetClass, mw[i].form,
                XmNtopAttachment, XmATTACH_NONE,
                XmNbottomAttachment, XmATTACH_WIDGET,
                XmNbottomWidget, mw[i].message,
                XmNbottomOffset, 10,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, mw[i].button_submit_call,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
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
                XmNbottomOffset, 7,
                XmNnavigationType, XmTAB_GROUP,
                XmNtraversalOn, TRUE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
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
                XmNbottomOffset, 5,
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
                NULL);

// Row 4 (3 up from bottom row).  Message area.
        n=0;
        XtSetArg(args[n], XmNrows, 10); n++;
        XtSetArg(args[n], XmNmaxHeight, 170); n++;
        XtSetArg(args[n], XmNcolumns, box_len); n++;
        XtSetArg(args[n], XmNeditable, FALSE); n++;
        XtSetArg(args[n], XmNtraversalOn, FALSE); n++;
        XtSetArg(args[n], XmNeditMode, XmMULTI_LINE_EDIT); n++;
        XtSetArg(args[n], XmNwordWrap, TRUE); n++;
        XtSetArg(args[n], XmNshadowThickness, 3); n++;
        XtSetArg(args[n], XmNscrollHorizontal, FALSE); n++;
        XtSetArg(args[n], XmNcursorPositionVisible, FALSE); n++;
        XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
        XtSetArg(args[n], XmNtopOffset, 5); n++;
        XtSetArg(args[n], XmNbottomAttachment,XmATTACH_WIDGET); n++;
        XtSetArg(args[n], XmNbottomWidget,mw[i].call); n++;
        XtSetArg(args[n], XmNbottomOffset, 10); n++;
        XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
        XtSetArg(args[n], XmNleftOffset, 5); n++;
        XtSetArg(args[n], XmNrightAttachment,XmATTACH_FORM); n++;
        XtSetArg(args[n], XmNrightOffset,5); n++;
        XtSetArg(args[n], XmNautoShowCursorPosition, FALSE); n++;
        XtSetArg(args[n], XmNforeground, MY_FG_COLOR); n++;

// This one causes segfaults, why?  Answer: args[] was set to 20
// (too small)
//        XtSetArg(args[n], XmNbackground, MY_BG_COLOR); n++;
 
        mw[i].send_message_text = XmCreateScrolledText(mw[i].form,
                "Send_message smt",
                args,
                n);

        xastir_snprintf(mw[i].win, sizeof(mw[i].win), "%d", i);

        XtAddCallback(mw[i].send_message_change_path, XmNactivateCallback, Send_message_change_path, (XtPointer)mw[i].win);
 
        XtAddCallback(mw[i].button_ok, XmNactivateCallback, Send_message_now, (XtPointer)mw[i].win);
        XtAddCallback(mw[i].send_message_message_data, XmNactivateCallback, Send_message_now, (XtPointer)mw[i].win);
        XtAddCallback(mw[i].button_cancel, XmNactivateCallback, Send_message_destroy_shell,(XtPointer)mw[i].win);
 
// Note group messages isn't implemented fully yet.  When it is, the following might have
// to change again:
//        XtAddCallback(mw[i].button_clear_old_msgs, XmNactivateCallback, groupon == 0 ? Clear_message_from: Clear_message_to,
//                (XtPointer)mw[i].win);
        XtAddCallback(mw[i].button_clear_old_msgs, XmNactivateCallback, Clear_message_to_from, (XtPointer)mw[i].win);

        XtAddCallback(mw[i].button_clear_pending_msgs, XmNactivateCallback, Clear_messages_to, (XtPointer)mw[i].win);

        XtAddCallback(mw[i].button_kick_timer, XmNactivateCallback, Kick_timer, (XtPointer)mw[i].win);

        XtAddCallback(mw[i].button_submit_call, XmNactivateCallback, Check_new_call_messages, (XtPointer)mw[i].call);

        if (clientData != NULL) {
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

end_critical_section(&send_message_dialog_lock, "messages_gui.c:Send_message" );

}





/************************* Auto msg **************************************/
/*************************************************************************/
void Auto_msg_option( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer calldata) {
    int item_no = XTPOINTER_TO_INT(clientData);

    if (item_no)
        auto_reply = 1;
    else
        auto_reply = 0;
}





void Auto_msg_destroy_shell( /*@unused@*/ Widget widget, XtPointer clientData, /*@unused@*/ XtPointer callData) { 
    Widget shell = (Widget) clientData;
    XtPopdown(shell);

begin_critical_section(&auto_msg_dialog_lock, "messages_gui.c:Auto_msg_destroy_shell" );

    XtDestroyWidget(shell);
    auto_msg_dialog = (Widget)NULL;

end_critical_section(&auto_msg_dialog_lock, "messages_gui.c:Auto_msg_destroy_shell" );

}





void Auto_msg_set_now(Widget w, XtPointer clientData, XtPointer callData) {
    char temp[110];
    char *temp_ptr;


    temp_ptr = XmTextFieldGetString(auto_msg_set_data);
    substr(temp, temp_ptr, 99);
    XtFree(temp_ptr);

    (void)remove_trailing_spaces(temp);
    xastir_snprintf(auto_reply_message,
        sizeof(auto_reply_message),
        "%s",
        temp);
    Auto_msg_destroy_shell(w, clientData, callData);
}





void Auto_msg_set( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer callData) {
    static Widget  pane, my_form, button_ok, button_cancel, reply;
    Atom delw;

begin_critical_section(&auto_msg_dialog_lock, "messages_gui.c:Auto_msg_set" );

    if(!auto_msg_dialog) {

        auto_msg_dialog = XtVaCreatePopupShell(langcode("WPUPARM001"),
                xmDialogShellWidgetClass, appshell,
                XmNdeleteResponse,XmDESTROY,
                XmNdefaultPosition, FALSE,
                NULL);

        pane = XtVaCreateWidget("Auto_msg_set pane",
                xmPanedWindowWidgetClass, 
                auto_msg_dialog,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        my_form =  XtVaCreateWidget("Auto_msg_set my_form",
                xmFormWidgetClass, 
                pane,
                XmNfractionBase, 5,
                XmNautoUnmanage, FALSE,
                XmNshadowThickness, 1,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
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

    } else
        (void)XRaiseWindow(XtDisplay(auto_msg_dialog), XtWindow(auto_msg_dialog));

end_critical_section(&auto_msg_dialog_lock, "messages_gui.c:Auto_msg_set" );

}


