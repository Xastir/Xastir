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
#include <sys/types.h>
#include <sys/time.h>

#include "xastir.h"
#include "db.h"
#include "main.h"
#include "util.h"

#ifdef HAVE_DMALLOC
#include <dmalloc.h>
#endif

Widget All_messages_dialog = NULL;
Widget view_messages_text = NULL;
Widget vm_dist_data = NULL;

static xastir_mutex All_messages_dialog_lock;

int vm_range;
int view_message_limit;





void view_message_gui_init(void)
{
    init_critical_section( &All_messages_dialog_lock );
}





void view_message_print_record(Message *m_fill) {
    int pos;
    char *temp;
    int i;
    int my_size = 200;
    char temp_my_course[10];
    XmTextPosition drop_ptr;

    // Make sure it's within our distance range we have set
    if ((vm_range == 0)
            || ((int)distance_from_my_station(m_fill->from_call_sign,temp_my_course) <= vm_range)) {
 
        if ((temp = malloc((size_t)my_size)) == NULL)
            return;

        sprintf(temp,"%-9s>%-9s seq:%5s type:%c :%s\n",
            m_fill->from_call_sign,
            m_fill->call_sign,
            m_fill->seq,
            m_fill->type,
            m_fill->message_line);

        pos = (int)XmTextGetLastPosition(view_messages_text);
        XmTextInsert(view_messages_text, pos, temp);
        pos += strlen(temp);
        while (pos > view_message_limit) {
            for (drop_ptr = i = 0; i < 3; i++) {
                (void)XmTextFindString(view_messages_text, drop_ptr, "\n", XmTEXT_FORWARD, &drop_ptr);
                drop_ptr++;
            }
            XmTextReplace(view_messages_text, 0, drop_ptr, "");
            pos = (int)XmTextGetLastPosition(view_messages_text);
        }
        XtVaSetValues(view_messages_text, XmNcursorPosition, pos, NULL);

        free(temp);
    }
}





void view_message_display_file(char msg_type) {
    int pos;


    if ((All_messages_dialog != NULL)) {
        mscan_file(msg_type, view_message_print_record);
    }
    pos = (int)XmTextGetLastPosition(view_messages_text);
    XmTextShowPosition(view_messages_text, pos);
}





void all_messages(char from, char *call_sign, char *from_call, char *message) {
    char temp_my_course[10];
    char *temp;
    char data1[97];
    char data2[97];
    int pos;
    int i;
    int my_size = 200;
    XmTextPosition drop_ptr;

    if ((temp = malloc((size_t)my_size)) == NULL)
        return;

    if ((vm_range == 0) || ((int)distance_from_my_station(call_sign,temp_my_course) <= vm_range)) {
        if (strlen(message)>95) {
            strncpy(data1, message, 95);
            data1[95]='\0';
            strncpy(data2, message+95, 95);
        } else {
            strcpy(data1, message);
            strcpy(data2, "");
        }

        if (strncmp(call_sign, "java",4) == 0) {
            strncpy(call_sign, "Broadcast", 9);
            xastir_snprintf(temp, my_size, "%s %s\n%s\n%s\n", from_call, call_sign,
                    data1, data2);
        } else if (strncmp(call_sign, "USER", 4) == 0) {
            strncpy(call_sign, "Broadcast", 9);
            xastir_snprintf(temp, my_size, "%s %s\n%s\n%s\n", from_call, call_sign,
                    data1, data2);
        } else
            xastir_snprintf(temp, my_size, "%s to %s via:%c\n%s\n%s\n", from_call,
                    call_sign, from, data1, data2);

        if ((All_messages_dialog != NULL)) {

begin_critical_section(&All_messages_dialog_lock, "view_message_gui.c:all_messages" );

            pos = (int)XmTextGetLastPosition(view_messages_text);
            XmTextInsert(view_messages_text, pos, temp);
            pos += strlen(temp);
            while (pos > view_message_limit) {
                for (drop_ptr = i = 0; i < 3; i++) {
                    (void)XmTextFindString(view_messages_text, drop_ptr, "\n", XmTEXT_FORWARD, &drop_ptr);
                    drop_ptr++;
                }
                XmTextReplace(view_messages_text, 0, drop_ptr, "");
                pos = (int)XmTextGetLastPosition(view_messages_text);
            }
            XtVaSetValues(view_messages_text, XmNcursorPosition, pos, NULL);
            XmTextShowPosition(view_messages_text, pos);

end_critical_section(&All_messages_dialog_lock, "view_message_gui.c:all_messages" );

        }
    }
    free(temp);
}





void All_messages_destroy_shell( /*@unused@*/ Widget widget, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    Widget shell = (Widget) clientData;
    vm_range = atoi(XmTextFieldGetString(vm_dist_data));
    XtPopdown(shell);

begin_critical_section(&All_messages_dialog_lock, "view_message_gui.c:All_messages_destroy_shell" );

    XtDestroyWidget(shell);
    All_messages_dialog = (Widget)NULL;

end_critical_section(&All_messages_dialog_lock, "view_message_gui.c:All_messages_destroy_shell" );

}





void All_messages_change_range( /*@unused@*/ Widget widget, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    Widget shell = (Widget) clientData;
    vm_range = atoi(XmTextFieldGetString(vm_dist_data));
    XtPopdown(shell);

    All_messages_destroy_shell(widget, clientData, callData);
    view_all_messages(widget, clientData, callData);
}





void view_all_messages( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer callData) {
    Widget pane, my_form, button_range, button_close, dist, dist_units;
    unsigned int n;
    Arg args[20];
    Atom delw;
    char temp[10];

    if (!All_messages_dialog) {

begin_critical_section(&All_messages_dialog_lock, "view_message_gui.c:view_all_messages" );

        All_messages_dialog = XtVaCreatePopupShell(langcode("AMTMW00001"),xmDialogShellWidgetClass,Global.top,
                                  XmNdeleteResponse,XmDESTROY,
                                  XmNdefaultPosition, FALSE,
                                  NULL);

        pane = XtVaCreateWidget("view_all_messages pane",xmPanedWindowWidgetClass, All_messages_dialog,
                          XmNbackground, colors[0xff],
                          NULL);

        my_form =  XtVaCreateWidget("view_all_messages my_form",xmFormWidgetClass, pane,
                            XmNfractionBase, 5,
                            XmNbackground, colors[0xff],
                            XmNautoUnmanage, FALSE,
                            XmNshadowThickness, 1,
                            NULL);

        dist = XtVaCreateManagedWidget(langcode("AMTMW00002"),xmLabelWidgetClass, my_form,
                            XmNtopAttachment, XmATTACH_FORM,
                            XmNtopOffset, 10,
                            XmNbottomAttachment, XmATTACH_NONE,
                            XmNleftAttachment, XmATTACH_FORM,
                            XmNleftOffset, 10,
                            XmNrightAttachment, XmATTACH_NONE,
                            XmNbackground, colors[0xff],
                            XmNtraversalOn, FALSE,
                            NULL);

        vm_dist_data = XtVaCreateManagedWidget("view_all_messages dist_data", xmTextFieldWidgetClass, my_form,
                                      XmNeditable,   TRUE,
                                      XmNcursorPositionVisible, TRUE,
                                      XmNsensitive, TRUE,
                                      XmNshadowThickness,    1,
                                      XmNcolumns, 8,
                                      XmNwidth, ((8*7)+2),
                                      XmNmaxLength, 8,
                                      XmNbackground, colors[0x0f],
                                      XmNtopAttachment, XmATTACH_FORM,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, dist,
                                      XmNleftOffset, 10,
                                      XmNrightAttachment,XmATTACH_NONE,
                                      NULL);

        dist_units = XtVaCreateManagedWidget((units_english_metric?langcode("UNIOP00004"):langcode("UNIOP00005")),xmLabelWidgetClass, my_form,
                            XmNtopAttachment, XmATTACH_FORM,
                            XmNtopOffset, 10,
                            XmNbottomAttachment, XmATTACH_NONE,
                            XmNleftAttachment, XmATTACH_WIDGET,
                            XmNleftWidget, vm_dist_data,
                            XmNleftOffset, 10,
                            XmNrightAttachment, XmATTACH_NONE,
                            XmNbackground, colors[0xff],
                            XmNtraversalOn, FALSE,
                            NULL);

        button_range = XtVaCreateManagedWidget(langcode("BULMW00003"),xmPushButtonGadgetClass, my_form,
                                      XmNtopAttachment, XmATTACH_FORM,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, dist_units,
                                      XmNleftOffset, 10,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      XmNnavigationType, XmTAB_GROUP,
                                      NULL);

        n=0;
        XtSetArg(args[n], XmNrows, 25); n++;
        XtSetArg(args[n], XmNcolumns, 85); n++;
        XtSetArg(args[n], XmNeditable, FALSE); n++;
        XtSetArg(args[n], XmNtraversalOn, TRUE); n++;
        XtSetArg(args[n], XmNlistSizePolicy, XmVARIABLE); n++;
        XtSetArg(args[n], XmNeditMode, XmMULTI_LINE_EDIT); n++;
        XtSetArg(args[n], XmNwordWrap, TRUE); n++;
        XtSetArg(args[n], XmNbackground, colors[0xff]); n++;
        XtSetArg(args[n], XmNscrollHorizontal, TRUE); n++;
        XtSetArg(args[n], XmNscrollVertical, TRUE); n++;
//        XtSetArg(args[n], XmNscrollBarDisplayPolicy, XmSTATIC); n++;
        XtSetArg(args[n], XmNselectionPolicy, XmMULTIPLE_SELECT); n++;
        XtSetArg(args[n], XmNcursorPositionVisible, FALSE); n++;
        XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
        XtSetArg(args[n], XmNtopWidget, dist); n++;
        XtSetArg(args[n], XmNtopOffset, 20); n++;
        XtSetArg(args[n], XmNbottomAttachment, XmATTACH_NONE); n++;
        XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
        XtSetArg(args[n], XmNleftOffset, 5); n++;
        XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
        XtSetArg(args[n], XmNrightOffset, 5); n++;

        view_messages_text = XmCreateScrolledText(my_form,"view_all_messages text",args,n);

// WE7U
// It's hard to get tab groups working with ScrolledText widgets.  Tab'ing in is
// fine, but then I'm stuck in insert mode and it absorbs the tabs and beeps.

        button_close = XtVaCreateManagedWidget(langcode("UNIOP00003"),xmPushButtonGadgetClass, my_form,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, XtParent(view_messages_text),
                                      XmNtopOffset, 2,
                                      XmNbottomAttachment, XmATTACH_FORM,
                                      XmNbottomOffset, 5,
                                      XmNleftAttachment, XmATTACH_POSITION,
                                      XmNleftPosition, 2,
                                      XmNrightAttachment, XmATTACH_POSITION,
                                      XmNrightPosition, 3,
                                      XmNbackground, colors[0xff],
                                      XmNnavigationType, XmTAB_GROUP,
                                      NULL);

        XtAddCallback(button_close, XmNactivateCallback, All_messages_destroy_shell, All_messages_dialog);
        XtAddCallback(button_range, XmNactivateCallback, All_messages_change_range, All_messages_dialog);

        pos_dialog(All_messages_dialog);

        delw = XmInternAtom(XtDisplay(All_messages_dialog),"WM_DELETE_WINDOW", FALSE);
        XmAddWMProtocolCallback(All_messages_dialog, delw, All_messages_destroy_shell, (XtPointer)All_messages_dialog);

        sprintf(temp,"%d",vm_range);
        XmTextFieldSetString(vm_dist_data,temp);

        XtManageChild(my_form);
        XtManageChild(view_messages_text);
        XtVaSetValues(view_messages_text, XmNbackground, colors[0x0f], NULL);
        XtManageChild(pane);

        redraw_on_new_packet_data=1;

        // Dump all currently active messages to the new window
        view_message_display_file('M');

end_critical_section(&All_messages_dialog_lock, "view_message_gui.c:view_all_messages" );

        XtPopup(All_messages_dialog,XtGrabNone);
        fix_dialog_vsize(All_messages_dialog);

        // Move focus to the Close button.  This appears to highlight the
        // button fine, but we're not able to hit the <Enter> key to
        // have that default function happen.  Note:  We _can_ hit the
        // <SPACE> key, and that activates the option.
//        XmUpdateDisplay(All_messages_dialog);
        XmProcessTraversal(button_close, XmTRAVERSE_CURRENT);

    }  else
        (void)XRaiseWindow(XtDisplay(All_messages_dialog), XtWindow(All_messages_dialog));
}


