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

#include <Xm/XmAll.h>

#ifdef HAVE_XBAE_MATRIX_H
#include <Xbae/Matrix.h>
#endif

#include <X11/Xatom.h>
#include <X11/Shell.h>

#ifdef    HAVE_LIBINTL_H
#include <libintl.h>
#define    _(x)    gettext(x)
#else
#define    _(x)    (x)
#endif

#include "xastir.h"
#include "main.h"
#include "bulletin_gui.h"
#include "db.h"
#include "interface.h"
#include "util.h"



Widget Display_bulletins_dialog = NULL;
Widget Display_bulletins_text = NULL;
Widget dist_data = NULL;

static xastir_mutex display_bulletins_dialog_lock;

int bulletin_range;
int new_bulletin_flag = 0;
int new_bulletin_count = 0;
time_t first_new_bulletin_time = 0;
time_t last_new_bulletin_time = 0;





void bulletin_gui_init(void)
{
    init_critical_section(&display_bulletins_dialog_lock);
}





// Function called from check_for_new_bulletins() if a new bulletin
// has come in that's within our range and we have
// pop_up_new_bulletins enabled.  This causes the Bulletins() dialog
// to come up and rescan the message database for all bulletins that
// are within the radius specified.  By the time this gets called
// we've already waited a few seconds to try to get the posit to
// come in that matches the bulletin, and have then checked the
// database to make sure that the new bulletins received are still
// within our range.
void popup_bulletins(void) {
    if ((Display_bulletins_dialog == NULL)) {   // Dialog not up

        // Bring up the dialog
        Bulletins( (Widget)NULL, (XtPointer)NULL, (XtPointer)NULL );
    }
}





void  bulletin_message(/*@unused@*/ char from, char *call_sign, char *tag, char *packet_message, time_t sec_heard) {
    char temp[200];
    char temp_my_course[10];
    char temp_text[30];
    double distance;
    XmTextPosition pos, eol;
    struct tm *tmp;
    time_t timehd;
    char time_str[20];
    timehd=sec_heard;
    tmp = localtime(&timehd);

    if ( (packet_message != NULL) && (strlen(packet_message) > MAX_MESSAGE_LENGTH) ) {
        if (debug_level & 1)
            printf("bulletin_message:  Message length too long\n");
        return;
    }

    (void)strftime(time_str,sizeof(time_str),"%b %d %H:%M",tmp);

    distance = distance_from_my_station(call_sign,temp_my_course);
    xastir_snprintf(temp, sizeof(temp), "%-9s:%-4s (%s %6.1f %s) %s\n",
            call_sign, &tag[3], time_str, distance,
            units_english_metric ? langcode("UNIOP00004"): langcode("UNIOP00005"),
            packet_message);

// Operands of <= have incompatible types (double, int):
    if (((int)distance <= bulletin_range) || (bulletin_range == 0)) {

begin_critical_section(&display_bulletins_dialog_lock, "bulletin_gui.c:bulletin_message" );

        if ((Display_bulletins_dialog == NULL)) {   // Dialog not up
        }
 
        if ((Display_bulletins_dialog != NULL)) {
            strncpy(temp_text, temp, 14);
            temp_text[14] = '\0';

            // Look for this bulletin ID.  "pos" will hold the first char position if found.
            if (XmTextFindString(Display_bulletins_text, 0, temp_text, XmTEXT_FORWARD, &pos)) {

                // Found it, so now find the end-of-line for it
                (void)XmTextFindString(Display_bulletins_text, pos, "\n", XmTEXT_FORWARD, &eol);
                eol++;

                // And replace the old bulletin with a new copy
                XmTextReplace(Display_bulletins_text, pos, eol, temp);
            } else {
                for (pos = 0; strlen(temp_text) > 12 && pos < XmTextGetLastPosition(Display_bulletins_text);) {
                    if (XmCOPY_SUCCEEDED == XmTextGetSubstring(Display_bulletins_text, pos, 14, 30, temp_text)) {
                        if (temp_text[0] && strncmp(temp, temp_text, 14) < 0)
                            break;
                    } else
                        break;

                    (void)XmTextFindString(Display_bulletins_text, pos, "\n", XmTEXT_FORWARD, &eol);
                    pos = ++eol;
                }
                XmTextInsert(Display_bulletins_text,pos,temp);
            }
            bulletin_range = atoi(XmTextFieldGetString(dist_data));
        }

end_critical_section(&display_bulletins_dialog_lock, "bulletin_gui.c:bulletin_message" );

    }
}





static void bulletin_line(Message *fill) {
    bulletin_message(fill->data_via, fill->from_call_sign, fill->call_sign, fill->message_line, fill->sec_heard);
}





static void scan_bulletin_file(void) {
    mscan_file(MESSAGE_BULLETIN, bulletin_line);
}





// Find each bulletin that is within our range _and_ within our time
// parameters for new bulletins.  Count them only.  Results returned
// in the new_bulletin_count variable.
void count_bulletin_messages(/*@unused@*/ char from, char *call_sign, char *tag, char *packet_message, time_t sec_heard) {
    char temp[200];
    char temp_my_course[10];
    double distance;
    struct tm *tmp;
    time_t timehd;
    char time_str[20];
    timehd=sec_heard;
    tmp = localtime(&timehd);

    if ( (packet_message != NULL) && (strlen(packet_message) > MAX_MESSAGE_LENGTH) ) {
        if (debug_level & 1)
            printf("bulletin_message:  Message length too long\n");
        return;
    }

    (void)strftime(time_str,sizeof(time_str),"%b %d %H:%M",tmp);

    distance = distance_from_my_station(call_sign,temp_my_course);
    xastir_snprintf(temp, sizeof(temp), "%-9s:%-4s (%s %6.1f %s) %s\n",
            call_sign, &tag[3], time_str, distance,
            units_english_metric ? langcode("UNIOP00004"): langcode("UNIOP00005"),
            packet_message);

// Operands of <= have incompatible types (double, int):
    if (((int)distance <= bulletin_range) || (bulletin_range == 0)) {

        // Is it newer than our first new_bulletin timestamp?
        if (sec_heard >= first_new_bulletin_time) {
            new_bulletin_count ++;
        }
    }
}





static void count_bulletin_line(Message *fill) {
    count_bulletin_messages(fill->data_via, fill->from_call_sign, fill->call_sign, fill->message_line, fill->sec_heard);
}





static void count_new_bulletins(void) {
    mscan_file(MESSAGE_BULLETIN, count_bulletin_line);
}




// Function called from db.c:msg_data_add() when a new bulletin has
// come in and we have pop_up_new_bulletins enabled.  We set some
// variables and then wait for UpdateTime() to check whether any new
// bulletins are within our range after the timestamp recorded here
// has aged a bit.  This gives a few seconds for a posit to come in
// from the callsign as well, which may place the station outside
// our range.
void prep_for_popup_bulletins() {

    // Record the time that this most recent bulletin came in
    last_new_bulletin_time = sec_now();

    // Update the "first" time only if we don't have a timestamp in
    // it already.  This is the time that we _started_ getting this
    // string of new bulletins.  We may get several in a row, in
    // which case "first" and "last" times will differ.
    if (first_new_bulletin_time == 0)
        first_new_bulletin_time = last_new_bulletin_time;

    // Set the flag that gets the whole ball rolling
    new_bulletin_flag = 1;

    //printf("Setting new_bulletin_flag and last_new_bulletin_time\n");
}





// Function called by main.c:UpdateTime().  Checks whether enough
// time has passed since the last new bulletin came in (so that the
// posit for it might come in as well).  If so, checks for bulletins
// that are newer than first_new_bulletin_time and fit within our
// range.  If any found, it updates the Bulletins dialog.
time_t last_bulletin_check = 0;
void check_for_new_bulletins() {

    // Any new bulletins?  If not, return
    if (!new_bulletin_flag) {
        return;
    }

    // Check every two seconds max
    if ( (last_bulletin_check + 2) > sec_now() ) {
        return;
    }

    last_bulletin_check = sec_now();

    // Enough time passed since most recent bulletin?
    if ( (last_new_bulletin_time + 15) > sec_now() ) {

        //printf("Not enough time has passed\n");

        return;
    }

    // If we get to here, then we think we may have at least one new
    // bulletin, and the latest arrived more than XX seconds ago
    // (currently 15 seconds).  Check for bulletins which have
    // timestamps equal to or newer than first_new_bulletin_time and
    // fit within our range.

    new_bulletin_count = 0;

    //printf("Checking for new bulletins\n");

    count_new_bulletins();

    //printf("%d new bulletins found\n",new_bulletin_count);

    if (new_bulletin_count) {
        popup_bulletins();

        if (debug_level & 1)
            printf("New bulletins (%d) caused popup!\n",new_bulletin_count);
    }
    else {
        if (debug_level & 1)
            printf("No bulletin popup generated.\n");
    }

    // Reset so that we can do it all over again later.  We need
    // mutex locks protecting these two variables.
    first_new_bulletin_time = 0;
    new_bulletin_flag = 0;
}





void Display_bulletins_destroy_shell(/*@unused@*/ Widget widget, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    Widget shell = (Widget) clientData;

    // Keep this.  It stores the range in a global variable when we destroy the dialog.
    bulletin_range = atoi(XmTextFieldGetString(dist_data));

    XtPopdown(shell);

begin_critical_section(&display_bulletins_dialog_lock, "bulletin_gui.c:Display_bulletins_destroy_shell" );

    XtDestroyWidget(shell);
    Display_bulletins_dialog = (Widget)NULL;

end_critical_section(&display_bulletins_dialog_lock, "bulletin_gui.c:Display_bulletins_destroy_shell" );

}





void Display_bulletins_change_range(/*@unused@*/ Widget widget, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer callData) {

    // Keep this.  It stores the range in a global variable when we destroy the dialog.
    bulletin_range = atoi(XmTextFieldGetString(dist_data));

    Display_bulletins_destroy_shell( widget, clientData, callData);
    Bulletins( widget, clientData, callData);

}





void Bulletins(/*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer callData) {
    Widget pane, form, button_range, button_close, dist, dist_units;
    unsigned int n;
    Arg args[20];
    Atom delw;
    char temp[10];


    if(!Display_bulletins_dialog) {


begin_critical_section(&display_bulletins_dialog_lock, "bulletin_gui.c:Bulletins" );


        Display_bulletins_dialog = XtVaCreatePopupShell(langcode("BULMW00001"),
                xmDialogShellWidgetClass,
                Global.top,
                XmNdeleteResponse,XmDESTROY,
                XmNdefaultPosition, FALSE,
                NULL);

        pane = XtVaCreateWidget("Bulletins pane",
                xmPanedWindowWidgetClass,
                Display_bulletins_dialog,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        form =  XtVaCreateWidget("Bulletins form",
                xmFormWidgetClass,
                pane,
                XmNfractionBase, 5,
                XmNautoUnmanage, FALSE,
                XmNshadowThickness, 1,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        dist = XtVaCreateManagedWidget(langcode("BULMW00002"),
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

        dist_data = XtVaCreateManagedWidget("dist_data", 
                xmTextFieldWidgetClass, 
                form,
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
                XmNnavigationType, XmTAB_GROUP,
                NULL);

        dist_units = XtVaCreateManagedWidget((units_english_metric?langcode("UNIOP00004"):langcode("UNIOP00005")),
                xmLabelWidgetClass, 
                form,
                XmNtopAttachment, XmATTACH_FORM,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, dist_data,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        button_range = XtVaCreateManagedWidget(langcode("BULMW00003"),
                xmPushButtonGadgetClass, 
                form,
                XmNtopAttachment, XmATTACH_FORM,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, dist_units,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_NONE,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        n=0;
        XtSetArg(args[n], XmNrows, 15); n++;
        XtSetArg(args[n], XmNcolumns, 108); n++;
        XtSetArg(args[n], XmNtraversalOn, FALSE); n++;
        XtSetArg(args[n], XmNeditable, FALSE); n++;
        XtSetArg(args[n], XmNeditMode, XmMULTI_LINE_EDIT); n++;
        XtSetArg(args[n], XmNwordWrap, TRUE); n++;
        XtSetArg(args[n], XmNscrollHorizontal, TRUE); n++;
        XtSetArg(args[n], XmNscrollVertical, TRUE); n++;
        XtSetArg(args[n], XmNcursorPositionVisible, FALSE); n++;
        XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
        XtSetArg(args[n], XmNtopWidget, dist); n++;
        XtSetArg(args[n], XmNtopOffset, 20); n++;
        XtSetArg(args[n], XmNbottomAttachment, XmATTACH_NONE); n++;
        XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
        XtSetArg(args[n], XmNleftOffset, 5); n++;
        XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
        XtSetArg(args[n], XmNrightOffset, 5); n++;
        XtSetArg(args[n], XmNforeground, MY_FG_COLOR); n++;
        XtSetArg(args[n], XmNbackground, MY_BG_COLOR); n++;


        Display_bulletins_text = XmCreateScrolledText(form,
                "Bulletins text",
                args,
                n);

        button_close = XtVaCreateManagedWidget(langcode("UNIOP00003"),
                xmPushButtonGadgetClass, 
                form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, XtParent(Display_bulletins_text),
                XmNtopOffset, 2,
                XmNbottomAttachment, XmATTACH_FORM,
                XmNbottomOffset, 5,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 2,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 3,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        XtAddCallback(button_range, XmNactivateCallback, Display_bulletins_change_range, Display_bulletins_dialog);
        XtAddCallback(button_close, XmNactivateCallback, Display_bulletins_destroy_shell, Display_bulletins_dialog);

        pos_dialog(Display_bulletins_dialog);

        delw = XmInternAtom(XtDisplay(Display_bulletins_dialog),"WM_DELETE_WINDOW", FALSE);
        XmAddWMProtocolCallback(Display_bulletins_dialog, delw, Display_bulletins_destroy_shell, (XtPointer)Display_bulletins_dialog);

        xastir_snprintf(temp, sizeof(temp), "%d", bulletin_range);
        XmTextFieldSetString(dist_data, temp);

        XtManageChild(form);
        XtManageChild(Display_bulletins_text);
        XtVaSetValues(Display_bulletins_text, XmNbackground, colors[0x0f], NULL);
        XtManageChild(pane);

        redraw_on_new_packet_data=1;
        XtPopup(Display_bulletins_dialog,XtGrabNone);
        fix_dialog_vsize(Display_bulletins_dialog);


end_critical_section(&display_bulletins_dialog_lock, "bulletin_gui.c:Bulletins" );

        scan_bulletin_file();

        // Move focus to the Close button.  This appears to
        // highlight the button fine, but we're not able to hit the
        // <Enter> key to have that default function happen.  Note:
        // We _can_ hit the <SPACE> key, and that activates the
        // option.
        //XmUpdateDisplay(Display_bulletins_dialog);
        XmProcessTraversal(button_close, XmTRAVERSE_CURRENT);

    }  else {
        (void)XRaiseWindow(XtDisplay(Display_bulletins_dialog), XtWindow(Display_bulletins_dialog));
    }
}


