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
    char temp1[MAX_CALL+1];
    char temp2[302];
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

        if(debug_level & 2)
            printf("Send message to <%s> from <%s> :%s\n",temp1,mw[i].to_call_sign,temp2);

        if ( (strlen(temp1) != 0)                       // Callsign field is not blank
                && (strlen(temp2) != 0)                 // Message field is not blank
                && (strcmp(temp1,my_callsign) ) ) {     // And not my own callsign
            /* if you're sending a message you must be at the keyboard */
            auto_reply=0;
            XmToggleButtonSetState(auto_msg_toggle,FALSE,FALSE);
            statusline(langcode("BBARSTA011"),0);       // Auto Reply Messages OFF
            output_message(mw[i].to_call_sign,temp1,temp2);
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
        mw[i].send_message_dialog = XtVaCreatePopupShell(temp,xmDialogShellWidgetClass,Global.top,
                                XmNdeleteResponse,XmDESTROY,
                                XmNdefaultPosition, FALSE,
                                XmNtitleString,"Send Message",
                                NULL);

        mw[i].pane = XtVaCreateWidget("Send_message pane",xmPanedWindowWidgetClass, mw[i].send_message_dialog,
                        XmNbackground, colors[0xff],
                        NULL);

        mw[i].form =  XtVaCreateWidget("Send_message form",xmFormWidgetClass,mw[i].pane,
                        XmNfractionBase, 5,
                        XmNautoUnmanage, FALSE,
                        XmNbackground, colors[0xff],
                        XmNshadowThickness, 1,
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
        XtSetArg(args[n], XmNbackground, colors[0xff]); n++;
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

        mw[i].send_message_text = XmCreateScrolledText(mw[i].form,"Send_message smt",args,n);

        mw[i].call = XtVaCreateManagedWidget(langcode(groupon == 0  ? "WPUPMSB003": "WPUPMSB004"),
                            xmLabelWidgetClass, mw[i].form,
                            XmNtopAttachment, XmATTACH_NONE,
                            XmNbottomAttachment, XmATTACH_FORM,
                            XmNbottomOffset, 85,
                            XmNleftAttachment, XmATTACH_FORM,
                            XmNleftOffset, 10,
                            XmNrightAttachment, XmATTACH_NONE,
                            XmNbackground, colors[0xff],
                            NULL);

        mw[i].send_message_call_data = XtVaCreateManagedWidget("Send_message smcd", xmTextFieldWidgetClass, mw[i].form,
                                    XmNeditable,   TRUE,
                                    XmNcursorPositionVisible, TRUE,
                                    XmNsensitive, TRUE,
                                    XmNshadowThickness,    1,
                                    XmNcolumns, 15,
                                    XmNwidth, ((15*7)+2),
                                    XmNmaxLength, 15,
                                    XmNbackground, colors[0x0f],
                                    XmNtopAttachment, XmATTACH_NONE,
                                    XmNbottomAttachment, XmATTACH_FORM,
                                    XmNbottomOffset, 80,
                                    XmNleftAttachment, XmATTACH_WIDGET,
                                    XmNleftWidget, mw[i].call,
                                    XmNleftOffset, 10,
                                    XmNrightAttachment,XmATTACH_NONE,
                                    XmNnavigationType, XmTAB_GROUP,
                                    XmNtraversalOn, TRUE,
                                    NULL);

        xastir_snprintf(temp, sizeof(temp), langcode(groupon == 0 ? "WPUPMSB005": "WPUPMSB006"));

        mw[i].button_submit_call = XtVaCreateManagedWidget(temp,xmPushButtonGadgetClass, mw[i].form,
                                        XmNleftAttachment, XmATTACH_WIDGET,
                                        XmNleftWidget, mw[i].send_message_call_data,
                                        XmNleftOffset, 20,
                                        XmNtopAttachment, XmATTACH_NONE,
                                        XmNbottomAttachment, XmATTACH_FORM,
                                        XmNbottomOffset, 80,
                                        XmNrightAttachment, XmATTACH_NONE,
                                        XmNbackground, colors[0xff],
                                        XmNnavigationType, XmTAB_GROUP,
                                        XmNtraversalOn, TRUE,
                                        NULL);

        mw[i].button_clear_msg = XtVaCreateManagedWidget(langcode("WPUPMSB007"),xmPushButtonGadgetClass, mw[i].form,
                                        XmNleftAttachment, XmATTACH_WIDGET,
                                        XmNleftWidget, mw[i].button_submit_call,
                                        XmNleftOffset, 10,
                                        XmNtopAttachment, XmATTACH_NONE,
                                        XmNbottomAttachment, XmATTACH_FORM,
                                        XmNbottomOffset, 80,
                                        XmNrightAttachment, XmATTACH_NONE,
                                        XmNbackground, colors[0xff],
                                        XmNnavigationType, XmTAB_GROUP,
                                        XmNtraversalOn, TRUE,
                                        NULL);

        mw[i].message = XtVaCreateManagedWidget(langcode("WPUPMSB008"),xmLabelWidgetClass, mw[i].form,
                                        XmNtopAttachment, XmATTACH_NONE,
                                        XmNbottomAttachment, XmATTACH_FORM,
                                        XmNbottomOffset, 45,
                                        XmNleftAttachment, XmATTACH_FORM,
                                        XmNleftOffset, 10,
                                        XmNrightAttachment, XmATTACH_NONE,
                                        XmNbackground, colors[0xff],
                                        NULL);

        mw[i].send_message_message_data = XtVaCreateManagedWidget("Send_message smmd", xmTextFieldWidgetClass, mw[i].form,
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

        mw[i].button_ok = XtVaCreateManagedWidget(langcode("WPUPMSB009"),xmPushButtonGadgetClass, mw[i].form,
                                        XmNtopAttachment, XmATTACH_NONE,
                                        XmNbottomAttachment, XmATTACH_FORM,
                                        XmNbottomOffset, 5,
                                        XmNleftAttachment, XmATTACH_POSITION,
                                        XmNleftPosition, 1,
                                        XmNrightAttachment, XmATTACH_POSITION,
                                        XmNrightPosition, 2,
                                        XmNbackground, colors[0xff],
                                        XmNnavigationType, XmTAB_GROUP,
                                        XmNtraversalOn, TRUE,
                                        NULL);

        mw[i].button_cancel = XtVaCreateManagedWidget(langcode("UNIOP00003"),xmPushButtonGadgetClass, mw[i].form,
                                        XmNtopAttachment, XmATTACH_NONE,
                                        XmNbottomAttachment, XmATTACH_FORM,
                                        XmNbottomOffset, 5,
                                        XmNleftAttachment, XmATTACH_POSITION,
                                        XmNleftPosition, 3,
                                        XmNrightAttachment, XmATTACH_POSITION,
                                        XmNrightPosition, 4,
                                        XmNbackground, colors[0xff],
                                        XmNnavigationType, XmTAB_GROUP,
                                        XmNtraversalOn, TRUE,
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

        if (clientData != NULL)
        XmTextFieldSetString(mw[i].send_message_call_data, group);

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

        auto_msg_dialog = XtVaCreatePopupShell(langcode("WPUPARM001"),xmDialogShellWidgetClass,Global.top,
                            XmNdeleteResponse,XmDESTROY,
                            XmNdefaultPosition, FALSE,
                            NULL);

        pane = XtVaCreateWidget("Auto_msg_set pane",xmPanedWindowWidgetClass, auto_msg_dialog,
                    XmNbackground, colors[0xff],
                    NULL);

        my_form =  XtVaCreateWidget("Auto_msg_set my_form",xmFormWidgetClass, pane,
                    XmNfractionBase, 5,
                    XmNbackground, colors[0xff],
                    XmNautoUnmanage, FALSE,
                    XmNshadowThickness, 1,
                    NULL);

        reply = XtVaCreateManagedWidget(langcode("WPUPARM002"),xmLabelWidgetClass, my_form,
                        XmNtopAttachment, XmATTACH_FORM,
                        XmNtopOffset, 10,
                        XmNbottomAttachment, XmATTACH_NONE,
                        XmNleftAttachment, XmATTACH_FORM,
                        XmNleftOffset, 5,
                        XmNrightAttachment, XmATTACH_NONE,
                        XmNbackground, colors[0xff],
                        NULL);

        auto_msg_set_data = XtVaCreateManagedWidget("Auto_msg_set auto_msg_set_data", xmTextFieldWidgetClass, my_form,
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

        button_ok = XtVaCreateManagedWidget(langcode("UNIOP00001"),xmPushButtonGadgetClass, my_form,
                            XmNtopAttachment, XmATTACH_WIDGET,
                            XmNtopWidget, auto_msg_set_data,
                            XmNtopOffset, 10,
                            XmNbottomAttachment, XmATTACH_FORM,
                            XmNbottomOffset, 5,
                            XmNleftAttachment, XmATTACH_POSITION,
                            XmNleftPosition, 1,
                            XmNrightAttachment, XmATTACH_POSITION,
                            XmNrightPosition, 2,
                            XmNbackground, colors[0xff],
                            XmNnavigationType, XmTAB_GROUP,
                            XmNtraversalOn, TRUE,
                            NULL);

        button_cancel = XtVaCreateManagedWidget(langcode("UNIOP00002"),xmPushButtonGadgetClass, my_form,
                            XmNtopAttachment, XmATTACH_WIDGET,
                            XmNtopWidget, auto_msg_set_data,
                            XmNtopOffset, 10,
                            XmNbottomAttachment, XmATTACH_FORM,
                            XmNbottomOffset, 5,
                            XmNleftAttachment, XmATTACH_POSITION,
                            XmNleftPosition, 3,
                            XmNrightAttachment, XmATTACH_POSITION,
                            XmNrightPosition, 4,
                            XmNbackground, colors[0xff],
                            XmNnavigationType, XmTAB_GROUP,
                            XmNtraversalOn, TRUE,
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


