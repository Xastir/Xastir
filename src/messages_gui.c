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

#include <stdlib.h>
#include <stdio.h>                      // printf

#include <Xm/XmAll.h>

#include "main.h"
#include "xastir.h"
#include "db.h"
#include "main.h"
#include "lang.h"


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
    for (i = 0; i < strlen(input_string); i++) {
        if (input_string[i] == ',') {
            indexes[j++] = i;
            //printf("%d\n",i);     // Debug code
        }
    }

    // Get rid of asterisks and commas in the original string:
    for (i = 0; i < len; i++) {
        if (input_string[i] == '*' || input_string[i] == ',') {
            input_string[i] = '\0';
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
            strncpy(temp, &input_string[indexes[i]+1], indexes[i+1] - indexes[i]);

            // Massage temp until it resembles something we want to
            // use.
            //  "WIDEn" -> "WIDEn-N,"
            // "TRACEn" -> "WIDEn-N,"
            //  "TRACE" -> "WIDE,"
            if (strncasecmp(temp,"WIDE",4) == 0) {
                if ( (temp[4] != ',') && is_num_chr(temp[4]) ) {
//printf("Found a WIDEn-N\n");
                    xastir_snprintf(temp,
                        sizeof(temp),
                        "WIDE%c-%c",
                        temp[4],
                        temp[4]);
                }
                else {
//printf("Found a WIDE\n");
                    // Leave temp alone, it's just a WIDE
                }
            }
            else if (strncasecmp(temp,"TRACE",5) == 0) {
                if ( (temp[5] != ',') && is_num_chr(temp[5]) ) {
//printf("Found a TRACEn-N\n");
                    xastir_snprintf(temp,
                        sizeof(temp),
                        "WIDE%c-%c",
                        temp[5],
                        temp[5]);
                }
                else {
//printf("Found a TRACE\n");
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
    strcat(input_string,reverse_path);
}





void get_path_data(char *callsign, char *path) {
    DataRow *p_station;


    if (search_station_name(&p_station,callsign,1)) {  // Found callsign
        char new_path[200];

        xastir_snprintf(new_path,sizeof(new_path),p_station->node_path_ptr);

        if(debug_level & 2)
            printf("\nPath from %s: %s\n",
                callsign,
                new_path);

// We need to chop off the first call, remove asterisks and
// injection ID's, and reverse the order of the callsigns.  We need
// to do the same thing in the callback for button_submit_call, so
// that we get a new path whenever the callsign is changed.  Create
// a new TextFieldWidget to hold the path info, which gets filled in
// here (and the callback) but can be changed by the user.  Must
// find a nice way to use this path from output_my_data() as well.

        reverse_path(new_path);

        if (debug_level & 2)
            printf("  Path to %s: %s\n",
                callsign,
                new_path);

        strcpy(path,new_path);
    }
    else {  // Couldn't find callsign.  It's
            // not in our station database.
        if(debug_level & 2)
            printf("Path from %s: No Path Known\n",callsign);

        strcpy(path,"");
    }
}

 



void Send_message_destroy_shell( /*@unused@*/ Widget widget, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    int i; 

    i=atoi((char *)clientData);

begin_critical_section(&send_message_dialog_lock, "messages_gui.c:Send_message_destroy_shell" );

    if (mw[i].send_message_dialog) {
        XtPopdown(mw[i].send_message_dialog);
        XtDestroyWidget(mw[i].send_message_dialog);
    }

    mw[i].send_message_dialog = (Widget)NULL;
    strcpy(mw[i].to_call_sign,"");
    mw[i].send_message_call_data = (Widget)NULL;
    mw[i].send_message_message_data = (Widget)NULL;
    mw[i].send_message_text = (Widget)NULL;

end_critical_section(&send_message_dialog_lock, "messages_gui.c:Send_message_destroy_shell" );

}





void Check_new_call_messages( /*@unused@*/ Widget w, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    int i, pos;
    char call_sign[MAX_CALLSIGN+1];
    char path[200];

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

        // Go get the reverse path for the new callsign entered
        strcpy(call_sign,XmTextFieldGetString(mw[i].send_message_call_data));
        // Try lowercase
        get_path_data(call_sign, path);
        if (strlen(path) == 0) {
            // Try uppercase
            (void)to_upper(call_sign);
            get_path_data(call_sign, path);
        }
        XmTextFieldSetString(mw[i].send_message_path, path);
    }

end_critical_section(&send_message_dialog_lock, "messages_gui.c:Check_new_call_messages" );

    update_messages(1); // Force an immediate update
}





void Clear_messages( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer callData) {
    clear_outgoing_messages();
}





void Send_message_now( /*@unused@*/ Widget w, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    char temp1[MAX_CALL+1];
    char temp2[302];
    char path[200];
    int i;

    i=atoi((char *)clientData);

begin_critical_section(&send_message_dialog_lock, "messages_gui.c:Send_message_now" );

    if (mw[i].send_message_dialog) {

        /* find station and go there */
        strcpy(temp1,XmTextFieldGetString(mw[i].send_message_call_data));
        (void)remove_trailing_spaces(temp1);
        (void)to_upper(temp1);

        strcpy(temp2,XmTextFieldGetString(mw[i].send_message_message_data));
        (void)remove_trailing_spaces(temp2);

        strcpy(path,XmTextFieldGetString(mw[i].send_message_path));
        (void)remove_trailing_spaces(path);
        (void)to_upper(path);

        if(debug_level & 2)
            printf("Send message to <%s> from <%s> :%s\n",temp1,mw[i].to_call_sign,temp2);

        if ( (strlen(temp1) != 0)                       // Callsign field is not blank
                && (strlen(temp2) != 0)                 // Message field is not blank
                && (strcmp(temp1,my_callsign) ) ) {     // And not my own callsign
            /* if you're sending a message you must be at the keyboard */
            auto_reply=0;
            XmToggleButtonSetState(auto_msg_toggle,FALSE,FALSE);
            statusline(langcode("BBARSTA011"),0);       // Auto Reply Messages OFF
            output_message(mw[i].to_call_sign,temp1,temp2,path);
            XmTextFieldSetString(mw[i].send_message_message_data,"");
//            if (mw[i].message_group!=1)
//                XtSetSensitive(mw[i].button_ok,FALSE);
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
    char temp1[MAX_CALL+1];
    int i;
/* int pos;*/

    i=atoi((char *)clientData);

begin_critical_section(&send_message_dialog_lock, "messages_gui.c:Clear_message_from" );

    if (mw[i].send_message_dialog) {
        /* find station and go there */
        strcpy(temp1,XmTextFieldGetString(mw[i].send_message_call_data));
        (void)remove_trailing_spaces(temp1);  
        (void)to_upper(temp1);
        /*printf("Clear message from <%s> to <%s>\n",temp1,my_callsign);*/
        mdelete_messages_from(temp1);
        new_message_data=1;
    }

end_critical_section(&send_message_dialog_lock, "messages_gui.c:Clear_message_from" );

}





void Clear_message_to( /*@unused@*/ Widget w, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    char temp1[MAX_CALL+1];
    int i;

    i=atoi((char *)clientData);

begin_critical_section(&send_message_dialog_lock, "messages_gui.c:Clear_message_to" );

    if (mw[i].send_message_dialog) {
        /* find station and go there */
        strcpy(temp1,XmTextFieldGetString(mw[i].send_message_call_data));
        (void)remove_trailing_spaces(temp1);  
        (void)to_upper(temp1);
        /*printf("Clear message to <%s>\n",temp1);*/
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





void Send_message_call( /*@unused@*/ Widget w, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    char call[20];

    if(clientData !=NULL) {
        strcpy(call,(char *)clientData);
        Send_message(Global.top,call,NULL);
    }           
}





void Send_message( /*@unused@*/ Widget w, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    Arg args[20];
    char temp[60];
    unsigned int n;
    int j,i;
    char group[MAX_CALLSIGN+1];
    int groupon;
    int box_len;
    Atom delw;
//    DataRow *p_station;

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
                strcpy(mw[i].to_call_sign,"***");
                memmove(group, &group[1], strlen(group));
                groupon=1;
                box_len=100;
            } else
                strcpy(mw[i].to_call_sign, my_callsign);
        } else
            strcpy(mw[i].to_call_sign, my_callsign);

        xastir_snprintf(temp, sizeof(temp), langcode(groupon==0 ? "WPUPMSB001": "WPUPMSB002"),
                (i+1));

        mw[i].message_group = groupon;
        mw[i].send_message_dialog = XtVaCreatePopupShell(temp,
                xmDialogShellWidgetClass,
                Global.top,
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
        XtSetArg(args[n], XmNbottomAttachment,XmATTACH_FORM); n++;
        XtSetArg(args[n], XmNbottomOffset, 110); n++;
        XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
        XtSetArg(args[n], XmNleftOffset, 5); n++;
        XtSetArg(args[n], XmNrightAttachment,XmATTACH_FORM); n++;
        XtSetArg(args[n], XmNrightOffset,5); n++;
        XtSetArg(args[n], XmNautoShowCursorPosition, FALSE); n++;
        XtSetArg(args[n], XmNforeground, MY_FG_COLOR); n++;
        XtSetArg(args[n], XmNbackground, MY_BG_COLOR); n++;
 
        mw[i].send_message_text = XmCreateScrolledText(mw[i].form,
                "Send_message smt",
                args,
                n);

        mw[i].call = XtVaCreateManagedWidget(langcode(groupon == 0  ? "WPUPMSB003": "WPUPMSB004"),
                xmLabelWidgetClass, mw[i].form,
                XmNtopAttachment, XmATTACH_NONE,
                XmNbottomAttachment, XmATTACH_FORM,
                XmNbottomOffset, 85,
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
                XmNbottomAttachment, XmATTACH_FORM,
                XmNbottomOffset, 80,
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
                XmNbottomAttachment, XmATTACH_FORM,
                XmNbottomOffset, 80,
                XmNrightAttachment, XmATTACH_NONE,
                XmNnavigationType, XmTAB_GROUP,
                XmNtraversalOn, TRUE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        mw[i].button_clear_msg = XtVaCreateManagedWidget(langcode("WPUPMSB007"),
                xmPushButtonGadgetClass, 
                mw[i].form,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, mw[i].button_submit_call,
                XmNleftOffset, 10,
                XmNtopAttachment, XmATTACH_NONE,
                XmNbottomAttachment, XmATTACH_FORM,
                XmNbottomOffset, 80,
                XmNrightAttachment, XmATTACH_NONE,
                XmNnavigationType, XmTAB_GROUP,
                XmNtraversalOn, TRUE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

//WE7U
        mw[i].path = XtVaCreateManagedWidget(langcode("WPUPMSB010"),
                xmLabelWidgetClass, mw[i].form,
                XmNtopAttachment, XmATTACH_NONE,
                XmNbottomAttachment, XmATTACH_FORM,
                XmNbottomOffset, 85,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, mw[i].button_clear_msg,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        mw[i].send_message_path = XtVaCreateManagedWidget("Send_message path", 
                xmTextFieldWidgetClass, 
                mw[i].form,
                XmNeditable,   TRUE,
                XmNcursorPositionVisible, TRUE,
                XmNsensitive, TRUE,
                XmNshadowThickness,    1,
                XmNcolumns, 26,
                XmNwidth, ((26*7)+2),
                XmNmaxLength, 199,
                XmNbackground, colors[0x0f],
                XmNtopAttachment, XmATTACH_NONE,
                XmNbottomAttachment, XmATTACH_FORM,
                XmNbottomOffset, 80,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, mw[i].path,
                XmNleftOffset, 5,
                XmNrightAttachment,XmATTACH_NONE,
                XmNnavigationType, XmTAB_GROUP,
                XmNtraversalOn, TRUE,
                NULL);

        mw[i].message = XtVaCreateManagedWidget(langcode("WPUPMSB008"),
                xmLabelWidgetClass, 
                mw[i].form,
                XmNtopAttachment, XmATTACH_NONE,
                XmNbottomAttachment, XmATTACH_FORM,
                XmNbottomOffset, 45,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        mw[i].send_message_message_data = XtVaCreateManagedWidget("Send_message smmd", 
                xmTextFieldWidgetClass, 
                mw[i].form,
                XmNeditable,   TRUE,
                XmNcursorPositionVisible, TRUE,
                XmNsensitive, TRUE,
                XmNshadowThickness,    1,
                XmNcolumns, 64,
                XmNwidth, ((64*7)+2),
                XmNmaxLength, 255,
                XmNbackground, colors[0x0f],
                XmNtopAttachment, XmATTACH_NONE,
                XmNbottomAttachment, XmATTACH_FORM,
                XmNbottomOffset, 40,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, mw[i].message,
                XmNleftOffset, 10,
                XmNrightAttachment,XmATTACH_NONE,
                XmNnavigationType, XmTAB_GROUP,
                XmNtraversalOn, TRUE,
                NULL);

        mw[i].button_ok = XtVaCreateManagedWidget(langcode("WPUPMSB009"),
                xmPushButtonGadgetClass, 
                mw[i].form,
                XmNtopAttachment, XmATTACH_NONE,
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

        mw[i].button_cancel = XtVaCreateManagedWidget(langcode("UNIOP00003"),
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

        xastir_snprintf(mw[i].win, sizeof(mw[i].win), "%d", i);
        XtAddCallback(mw[i].button_ok, XmNactivateCallback, Send_message_now, (XtPointer)mw[i].win);
        XtAddCallback(mw[i].send_message_message_data, XmNactivateCallback, Send_message_now, (XtPointer)mw[i].win);
        XtAddCallback(mw[i].button_cancel, XmNactivateCallback, Send_message_destroy_shell,(XtPointer)mw[i].win);
 
// Note group messages isn't implemented fully yet.  When it is, the following might have
// to change again:
//        XtAddCallback(mw[i].button_clear_msg, XmNactivateCallback, groupon == 0 ? Clear_message_from: Clear_message_to,
//                (XtPointer)mw[i].win);
        XtAddCallback(mw[i].button_clear_msg, XmNactivateCallback, Clear_message_to_from, (XtPointer)mw[i].win);

        XtAddCallback(mw[i].button_submit_call, XmNactivateCallback,  Check_new_call_messages, (XtPointer)mw[i].win);

        if (clientData != NULL) {
            char path[200];

            XmTextFieldSetString(mw[i].send_message_call_data, group);
            get_path_data(group, path);
            XmTextFieldSetString(mw[i].send_message_path, path);
        }

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
    int item_no = (int) clientData;

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

    substr(temp,XmTextFieldGetString(auto_msg_set_data),99);
    (void)remove_trailing_spaces(temp);
    strcpy(auto_reply_message,temp);
    Auto_msg_destroy_shell(w, clientData, callData);
}





void Auto_msg_set( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer callData) {
    static Widget  pane, my_form, button_ok, button_cancel, reply;
    Atom delw;

begin_critical_section(&auto_msg_dialog_lock, "messages_gui.c:Auto_msg_set" );

    if(!auto_msg_dialog) {

        auto_msg_dialog = XtVaCreatePopupShell(langcode("WPUPARM001"),
                xmDialogShellWidgetClass,
                Global.top,
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


