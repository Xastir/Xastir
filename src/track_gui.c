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
#include <stdlib.h>
#include <sys/stat.h>
#include <pwd.h>
#include <unistd.h>
#include <Xm/XmAll.h>

#include "main.h"
#include "xastir.h"
#include "db.h"
#include "main.h"
#include "lang.h"
#include "popup.h"

Widget track_station_dialog = (Widget)NULL;
Widget track_station_data = (Widget)NULL;
Widget download_findu_dialog = (Widget)NULL;

static xastir_mutex track_station_dialog_lock;
static xastir_mutex download_findu_dialog_lock;

// track values
Widget track_case_data, track_match_data;

// Download findu values
Widget download_trail_station_data;
Widget posit_start_value;
Widget posit_length_value;


int track_station_on;           /* used for tracking stations */
int track_me;
int track_case;                 /* used for tracking stations */
int track_match;                /* used for tracking stations */
char tracking_station_call[30]; /* Tracking station callsign */
char download_trail_station_call[30];   /* Trail station callsign */
//N0VH
int posit_start = 336;
int posit_length = 336;


void track_gui_init(void)
{
    init_critical_section( &track_station_dialog_lock );
    init_critical_section( &download_findu_dialog_lock );
    strcpy(tracking_station_call,"");
}


/**** Track STATION ******/


void track_station_destroy_shell( /*@unused@*/ Widget widget, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    Widget shell = (Widget) clientData;
    XtPopdown(shell);

begin_critical_section(&track_station_dialog_lock, "track_gui.c:track_station_destroy_shell" );

    XtDestroyWidget(shell);
    track_station_dialog = (Widget)NULL;

end_critical_section(&track_station_dialog_lock, "track_gui.c:track_station_destroy_shell" );

}





void Track_station_clear(Widget w, XtPointer clientData, XtPointer callData) {
    /* clear station */
    track_station_on=0;
    //track_station_data=NULL;
    //strcpy(tracking_station_call,"");
    track_station_destroy_shell(w, clientData, callData);
    display_zoom_status();
}





void Track_station_now(Widget w, XtPointer clientData, XtPointer callData) {
    char temp[MAX_CALL+1];
    char temp2[200];
    int found = 0;

    /* find station and go there */
    strcpy(temp,XmTextFieldGetString(track_station_data));
    (void)remove_trailing_spaces(temp);
    strcpy(tracking_station_call,temp);
    track_case  = (int)XmToggleButtonGetState(track_case_data);
    track_match = (int)XmToggleButtonGetState(track_match_data);
    found = locate_station(da, temp, track_case, track_match, 0);

    if ( valid_object(tracking_station_call)    // Name of object is legal
            || valid_call(tracking_station_call)
            || valid_item(tracking_station_call ) ) {
        track_station_on = 1;   // Track it whether we've seen it yet or not
        if (!found) {
            xastir_snprintf(temp2, sizeof(temp2), langcode("POPEM00026"), temp);
            popup_message(langcode("POPEM00025"),temp2);
        }
    } else {
        strcpy(tracking_station_call,""); // Empty it out again
        track_station_on = 0;
        xastir_snprintf(temp2, sizeof(temp2), langcode("POPEM00002"), temp);
        popup_message(langcode("POPEM00003"),temp2);
    }

    track_station_destroy_shell(w, clientData, callData);
    display_zoom_status();
}





void Track_station( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer callData) {
    static Widget pane, my_form, button_ok, button_close, button_clear, call, sep;
    Atom delw;

    if (!track_station_dialog) {

begin_critical_section(&track_station_dialog_lock, "track_gui.c:Track_station" );

        track_station_dialog = XtVaCreatePopupShell(langcode("WPUPTSP001"),
                xmDialogShellWidgetClass,
                Global.top,
                XmNdeleteResponse,XmDESTROY,
                XmNdefaultPosition, FALSE,
                NULL);

        pane = XtVaCreateWidget("Track_station pane",
                xmPanedWindowWidgetClass, 
                track_station_dialog,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        my_form =  XtVaCreateWidget("Track_station my_form",
                xmFormWidgetClass, 
                pane,
                XmNfractionBase, 3,
                XmNautoUnmanage, FALSE,
                XmNshadowThickness, 1,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        call = XtVaCreateManagedWidget(langcode("WPUPTSP002"),
                xmLabelWidgetClass, 
                my_form,
                XmNtopAttachment, XmATTACH_FORM,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        track_station_data = XtVaCreateManagedWidget("Track_station track locate data", 
                xmTextFieldWidgetClass, 
                my_form,
                XmNeditable,   TRUE,
                XmNcursorPositionVisible, TRUE,
                XmNsensitive, TRUE,
                XmNshadowThickness,    1,
                XmNcolumns, 15,
                XmNwidth, ((15*7)+2),
                XmNmaxLength, 15,
                XmNbackground, colors[0x0f],
                XmNtopAttachment,XmATTACH_FORM,
                XmNtopOffset, 5,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, call,
                XmNleftOffset, 10,
                XmNrightAttachment,XmATTACH_NONE,
                XmNnavigationType, XmTAB_GROUP,
                XmNtraversalOn, TRUE,
                NULL);

        track_case_data  = XtVaCreateManagedWidget(langcode("WPUPTSP003"),
                xmToggleButtonWidgetClass,
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, call,
                XmNtopOffset, 20,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset ,10,
                XmNrightAttachment, XmATTACH_NONE,
                XmNnavigationType, XmTAB_GROUP,
                XmNtraversalOn, TRUE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        track_match_data  = XtVaCreateManagedWidget(langcode("WPUPTSP004"),
                xmToggleButtonWidgetClass,
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, call,
                XmNtopOffset, 20,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget,track_case_data,
                XmNrightOffset ,20,
                XmNrightAttachment, XmATTACH_NONE,
                XmNnavigationType, XmTAB_GROUP,
                XmNtraversalOn, TRUE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        sep = XtVaCreateManagedWidget("Track_station sep", 
                xmSeparatorGadgetClass,
                my_form,
                XmNorientation, XmHORIZONTAL,
                XmNtopAttachment,XmATTACH_WIDGET,
                XmNtopWidget,track_case_data,
                XmNtopOffset, 10,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNrightAttachment,XmATTACH_FORM,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        button_ok = XtVaCreateManagedWidget(langcode("WPUPTSP005"),
                xmPushButtonGadgetClass, 
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, sep,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_FORM,
                XmNbottomOffset, 5,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 0,
                XmNleftOffset, 5,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 1,
                XmNnavigationType, XmTAB_GROUP,
                XmNtraversalOn, TRUE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        button_clear = XtVaCreateManagedWidget(langcode("WPUPTSP006"),
                xmPushButtonGadgetClass, 
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, sep,
                XmNtopOffset, 5,
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

        button_close = XtVaCreateManagedWidget(langcode("UNIOP00003"),
                xmPushButtonGadgetClass, 
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, sep,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_FORM,
                XmNbottomOffset, 5,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 2,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 3,
                XmNrightOffset, 5,
                XmNnavigationType, XmTAB_GROUP,
                XmNtraversalOn, TRUE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        XtAddCallback(button_ok, XmNactivateCallback, Track_station_now, track_station_dialog);
        XtAddCallback(button_close, XmNactivateCallback, track_station_destroy_shell, track_station_dialog);
        XtAddCallback(button_clear, XmNactivateCallback, Track_station_clear, track_station_dialog);

        XmToggleButtonSetState(track_case_data,FALSE,FALSE);
        XmToggleButtonSetState(track_match_data,TRUE,FALSE);

        pos_dialog(track_station_dialog);

        delw = XmInternAtom(XtDisplay(track_station_dialog),"WM_DELETE_WINDOW", FALSE);
        XmAddWMProtocolCallback(track_station_dialog, delw, track_station_destroy_shell, (XtPointer)track_station_dialog);

//        if (track_station_on==1)
            XmTextFieldSetString(track_station_data,tracking_station_call);

        XtManageChild(my_form);
        XtManageChild(pane);

end_critical_section(&track_station_dialog_lock, "track_gui.c:Track_station" );

        XtPopup(track_station_dialog,XtGrabNone);
        fix_dialog_size(track_station_dialog);

        // Move focus to the Cancel button.  This appears to highlight the
        // button fine, but we're not able to hit the <Enter> key to
        // have that default function happen.  Note:  We _can_ hit the
        // <SPACE> key, and that activates the option.
//        XmUpdateDisplay(track_station_dialog);
        XmProcessTraversal(button_close, XmTRAVERSE_CURRENT);

    } else
        (void)XRaiseWindow(XtDisplay(track_station_dialog), XtWindow(track_station_dialog));
}





/***** DOWNLOAD FINDU TRAILS *****/


void Download_trail_destroy_shell( /*@unused@*/ Widget widget, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    Widget shell = (Widget) clientData;
    XtPopdown(shell);

begin_critical_section(&download_findu_dialog_lock, "track_gui.c:Download_trail_destroy_shell" );

    XtDestroyWidget(shell);
    download_findu_dialog = (Widget)NULL;

end_critical_section(&download_findu_dialog_lock, "track_gui.c:Download_trail_destroy_shell" );
}





void Download_trail_now(Widget w, XtPointer clientData, XtPointer callData) {
    char temp[MAX_CALL+1];
    char fileimg[400];
    char tempfile[500];
    uid_t user_id;
    struct passwd *user_info;
    char username[20];
    char log_filename[200];


    busy_cursor(appshell);

    // Fetch my user info
    user_id=getuid();
    user_info=getpwuid(user_id);
    // Fetch the login name
    strcpy(username,user_info->pw_name);

    xastir_snprintf(log_filename, sizeof(log_filename), "/var/tmp/xastir_%s_map.log", username);

    XmScaleGetValue(posit_start_value , &posit_start);
    XmScaleGetValue(posit_length_value , &posit_length);
    /* find station and go there */
    strcpy(temp,XmTextFieldGetString(download_trail_station_data));
    (void)remove_trailing_spaces(temp);
    strcpy(download_trail_station_call,temp);
    //Download_trail_destroy_shell(w, clientData, callData);

    xastir_snprintf(fileimg, sizeof(fileimg),
        "\'http://64.34.101.121/cgi-bin/rawposit.cgi?call=%s&start=%d&length=%d\'",
        download_trail_station_call,posit_start,posit_length);

    xastir_snprintf(tempfile, sizeof(tempfile),
            "wget -S -N -t 1 -T 30 -O %s %s 2> /dev/null\n",
            log_filename,
            fileimg);

    if (debug_level & 2)
        printf("%s",tempfile);

    if ( system(tempfile) ) {   // Go get the file
        printf("Couldn't download the trail\n");
        return;
    }

    // Set permissions on the file so that any user can overwrite it.
    chmod(log_filename, 0666);

    // Here we do the findu stuff, if the findu_flag is set.  Else we do an imagemap.
    // We have the log data we're interested in within the log_filename file.
    // Cause that file to be read by the "File->Open Log File" routine.  HTML
    // tags will be ignored just fine.
    read_file_ptr = fopen(log_filename, "r");

    if (read_file_ptr != NULL)
        read_file = 1;
    else
        printf("Couldn't open file: %s\n", log_filename);

    display_zoom_status();

    Download_trail_destroy_shell(w, clientData, callData);
}


void Reset_posit_length_max(Widget w, XtPointer clientData, XtPointer callData) {

    int temp;

    XmScaleGetValue(posit_length_value, &temp);
    XmScaleGetValue(posit_start_value, &posit_start);
    if (temp >= posit_start) {
        XtVaSetValues(posit_length_value,
		      XmNvalue, posit_start,
		      NULL);
    }
    XtVaSetValues(posit_length_value,
                  XmNmaximum, posit_start,
		  NULL);
}



void Download_findu_trail( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer callData) {
    static Widget pane, my_form, button_ok, button_cancel, call, sep;
    Atom delw;


    if (!download_findu_dialog) {

begin_critical_section(&download_findu_dialog_lock, "track_gui.c:Download_findu_trail" );

        download_findu_dialog = XtVaCreatePopupShell(langcode("WPUPTSP007"),
                xmDialogShellWidgetClass,
                Global.top,
                XmNdeleteResponse,XmDESTROY,
                XmNdefaultPosition, FALSE,
                NULL);

        pane = XtVaCreateWidget("Download_findu_trail pane",
                xmPanedWindowWidgetClass, 
                download_findu_dialog,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        my_form =  XtVaCreateWidget("Download_findu_trail my_form",
                xmFormWidgetClass, 
                pane,
                XmNfractionBase, 2,
                XmNautoUnmanage, FALSE,
                XmNshadowThickness, 1,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        call = XtVaCreateManagedWidget(langcode("WPUPTSP008"),
                xmLabelWidgetClass, 
                my_form,
                XmNtopAttachment, XmATTACH_FORM,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        download_trail_station_data = XtVaCreateManagedWidget("download_trail_station_data", 
                xmTextFieldWidgetClass, 
                my_form,
                XmNeditable,   TRUE,
                XmNcursorPositionVisible, TRUE,
                XmNsensitive, TRUE,
                XmNshadowThickness,    1,
                XmNcolumns, 15,
                XmNwidth, ((15*7)+2),
                XmNmaxLength, 15,
                XmNbackground, colors[0x0f],
                XmNtopAttachment,XmATTACH_FORM,
                XmNtopOffset, 5,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, call,
                XmNleftOffset, 10,
                XmNrightAttachment,XmATTACH_NONE,
                XmNnavigationType, XmTAB_GROUP,
                XmNtraversalOn, TRUE,
                NULL);

        posit_start_value = XtVaCreateManagedWidget("Start of Trail (hrs ago)", 
                xmScaleWidgetClass, 
                my_form,
                XmNtopAttachment,XmATTACH_WIDGET,
				XmNtopWidget, call,
                XmNtopOffset, 15,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 10,
                XmNrightAttachment,XmATTACH_NONE,
                XmNnavigationType, XmTAB_GROUP,
                XmNtraversalOn, TRUE,
				//XmNwidth, 190,
				XmNrightAttachment, XmATTACH_FORM,
                XmNrightOffset, 10,
			    XmNsensitive, TRUE,
				XmNorientation, XmHORIZONTAL,
				XmNborderWidth, 1,
				XmNminimum, 1,
			    XmNmaximum, 336,
				XmNshowValue, TRUE,
				XmNvalue, posit_start,
				XtVaTypedArg, XmNtitleString, XmRString, "Start Trail (hrs ago)", 10,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
				NULL);

        posit_length_value = XtVaCreateManagedWidget("Length of trail (hrs)", 
                xmScaleWidgetClass, 
                my_form,
                XmNtopAttachment,XmATTACH_WIDGET,
				XmNtopWidget, posit_start_value,
                XmNtopOffset, 15,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 10,
                XmNrightAttachment,XmATTACH_NONE,
                XmNnavigationType, XmTAB_GROUP,
                XmNtraversalOn, TRUE,
				//XmNwidth, 190,
				XmNrightAttachment, XmATTACH_FORM,
                XmNrightOffset, 10,
			    XmNsensitive, TRUE,
				XmNorientation, XmHORIZONTAL,
				XmNborderWidth, 1,
				XmNminimum, 1,
			    XmNmaximum, 336,
				XmNshowValue, TRUE,
				XmNvalue, posit_length,
				XtVaTypedArg, XmNtitleString, XmRString, "Trail Length (hrs)", 10,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
				NULL);

        sep = XtVaCreateManagedWidget("Download_findu_trail sep", 
                xmSeparatorGadgetClass,
                my_form,
                XmNorientation, XmHORIZONTAL,
                XmNtopAttachment,XmATTACH_WIDGET,
                XmNtopWidget,posit_length_value,
                XmNtopOffset, 10,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNrightAttachment,XmATTACH_FORM,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        button_ok = XtVaCreateManagedWidget(langcode("WPUPTSP007"),
                xmPushButtonGadgetClass, 
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, sep,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_FORM,
                XmNbottomOffset, 5,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 0,
                XmNleftOffset, 5,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 1,
                XmNnavigationType, XmTAB_GROUP,
                XmNtraversalOn, TRUE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        button_cancel = XtVaCreateManagedWidget(langcode("UNIOP00002"),
                xmPushButtonGadgetClass, 
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, sep,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_FORM,
                XmNbottomOffset, 5,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 1,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 2,
                XmNrightOffset, 5,
                XmNnavigationType, XmTAB_GROUP,
                XmNtraversalOn, TRUE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        XtAddCallback(button_ok, XmNactivateCallback, Download_trail_now, download_findu_dialog);
        XtAddCallback(button_cancel, XmNactivateCallback, Download_trail_destroy_shell, download_findu_dialog);
        XtAddCallback(posit_start_value, XmNvalueChangedCallback, Reset_posit_length_max, download_findu_dialog);

        pos_dialog(download_findu_dialog);

        delw = XmInternAtom(XtDisplay(download_findu_dialog),"WM_DELETE_WINDOW", FALSE);
        XmAddWMProtocolCallback(download_findu_dialog, delw, Download_trail_destroy_shell, (XtPointer)download_findu_dialog);

        XmTextFieldSetString(download_trail_station_data,download_trail_station_call);

        XtManageChild(my_form);
        XtManageChild(pane);

end_critical_section(&download_findu_dialog_lock, "track_gui.c:Download_trail" );

        XtPopup(download_findu_dialog,XtGrabNone);
        fix_dialog_size(download_findu_dialog);

        // Move focus to the Cancel button.  This appears to highlight the
        // button fine, but we're not able to hit the <Enter> key to
        // have that default function happen.  Note:  We _can_ hit the
        // <SPACE> key, and that activates the option.
//        XmUpdateDisplay(download_findu_dialog);
        XmProcessTraversal(button_cancel, XmTRAVERSE_CURRENT);

    } else
        (void)XRaiseWindow(XtDisplay(download_findu_dialog), XtWindow(download_findu_dialog));
}


