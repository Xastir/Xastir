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

#include <stdio.h>

#include <Xm/XmAll.h>
#ifdef HAVE_XBAE_MATRIX_H
#include <Xbae/Matrix.h>
#endif  // HAVE_XBAE_MATRIX_H

#include "main.h"
#include "xastir.h"
#include "lang.h"
#include "maps.h"


Widget locate_station_dialog = (Widget)NULL;
Widget locate_station_data = (Widget)NULL;
char locate_station_call[30];
static xastir_mutex locate_station_dialog_lock;

Widget locate_place_dialog = (Widget)NULL;
Widget locate_place_data = (Widget)NULL;
Widget locate_state_data = (Widget)NULL;
Widget locate_county_data = (Widget)NULL;
Widget locate_quad_data = (Widget)NULL;
Widget locate_type_data = (Widget)NULL;
Widget locate_gnis_file_data = (Widget)NULL;
char locate_place_name[50];
char locate_state_name[50];
char locate_county_name[50];
char locate_quad_name[50];
char locate_type_name[50];
char locate_gnis_filename[200];
static xastir_mutex locate_place_dialog_lock;


/* locate station values */
Widget locate_case_data, locate_match_data;

/* locate place values */
Widget locate_place_case_data, locate_place_match_data;





void locate_gui_init(void)
{
    init_critical_section( &locate_station_dialog_lock );
    init_critical_section( &locate_place_dialog_lock );
    locate_station_call[0] = '\0';
    locate_place_name[0] = '\0';
    locate_state_name[0] = '\0';
    locate_county_name[0] = '\0';
    locate_quad_name[0] = '\0';
    locate_type_name[0] = '\0';
}





/**** LOCATE STATION ******/

void Locate_station_destroy_shell(/*@unused@*/ Widget widget, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    Widget shell = (Widget) clientData;
    XtPopdown(shell);

begin_critical_section(&locate_station_dialog_lock, "locate_gui.c:Locate_station_destroy_shell" );

    XtDestroyWidget(shell);
    locate_station_dialog = (Widget)NULL;

end_critical_section(&locate_station_dialog_lock, "locate_gui.c:Locate_station_destroy_shell" );

}





/*
 * Look up detailed FCC/RAC info about the station
 */

// Determine whether it is a U.S. or Canadian callsign then search
// through the appropriate database and present the results.

void fcc_rac_lookup(Widget w, XtPointer clientData, XtPointer callData) {
    char station_call[200];
    char temp[1000];
    char temp2[300];
    char *temp_ptr;
    FccAppl my_fcc_data;
    rac_record my_rac_data;


    // Snag station call
    temp_ptr = XmTextFieldGetString(locate_station_data);
    xastir_snprintf(station_call,
        sizeof(station_call),
        "%s",
        temp_ptr);
    XtFree(temp_ptr);

    (void)remove_trailing_spaces(station_call);

    to_upper(station_call);

    switch (station_call[0]) {
        case 'A':
        case 'K':
        case 'N':
        case 'W':
            if (search_fcc_data_appl(station_call, &my_fcc_data) == 1) {

                xastir_snprintf(temp,
                    sizeof(temp),
                    "%s\n%s %s\n%s %s %s\n%s %s, %s %s, %s %s\n\n",
                    langcode("STIFCC0001"),
                    langcode("STIFCC0003"),
                    my_fcc_data.name_licensee,
                    langcode("STIFCC0004"),
                    my_fcc_data.text_street,
                    my_fcc_data.text_pobox,
                    langcode("STIFCC0005"),
                    my_fcc_data.city,
                    langcode("STIFCC0006"),
                    my_fcc_data.state,
                    langcode("STIFCC0007"),
                    my_fcc_data.zipcode);

                popup_message(langcode("WPUPLSP007"),temp);
            }
            else {
                xastir_snprintf(temp2,
                    sizeof(temp2),
                    "Callsign Not Found!\n");
                popup_message(langcode("POPEM00001"),temp2);
            }
            break;
        case 'V':
            if (search_rac_data(station_call, &my_rac_data) == 1) {

                xastir_snprintf(temp,
                    sizeof(temp),
                    "%s\n%s %s\n%s\n%s, %s\n%s\n",
                    langcode("STIFCC0002"),
                    my_rac_data.first_name,
                    my_rac_data.last_name,
                    my_rac_data.address,
                    my_rac_data.city,
                    my_rac_data.province,
                    my_rac_data.postal_code);

                    if (my_rac_data.qual_a[0] == 'A')
                        strncat(temp,
                            langcode("STIFCC0008"),
                            sizeof(temp) - strlen(temp));

                    if (my_rac_data.qual_d[0] == 'D')
                        strncat(temp,
                            langcode("STIFCC0009"),
                            sizeof(temp) - strlen(temp));

                    if (my_rac_data.qual_b[0] == 'B' && my_rac_data.qual_c[0] != 'C')
                        strncat(temp,
                            langcode("STIFCC0010"),
                            sizeof(temp) - strlen(temp));

                    if (my_rac_data.qual_c[0] == 'C')
                        strncat(temp,
                            langcode("STIFCC0011"),
                            sizeof(temp) - strlen(temp));

                    strncat(temp,
                        "\n",
                        sizeof(temp) - strlen(temp));

                    if (strlen(my_rac_data.club_name) > 1) {
                        xastir_snprintf(temp2,
                            sizeof(temp2),
                            "%s\n%s\n%s, %s\n%s\n",
                            my_rac_data.club_name,
                            my_rac_data.club_address,
                            my_rac_data.club_city,
                            my_rac_data.club_province,
                            my_rac_data.club_postal_code);
                        strncat(temp,
                            temp2,
                            sizeof(temp) - strlen(temp));
                    }


                popup_message(langcode("WPUPLSP007"),temp);
            }
            else {
                // RAC code does its own popup in this case?
                //fprintf(stderr, "Callsign not found\n");
            }
            break;
        default:
            xastir_snprintf(temp2,
                sizeof(temp2),
                "Not an FCC or RAC callsign!\n");
            popup_message(langcode("POPEM00001"),temp2);
            break;
    }
    Locate_station_destroy_shell(w, clientData, callData);
}





/*
 *  Locate a station by centering the map at its position
 */
void Locate_station_now(Widget w, XtPointer clientData, XtPointer callData) {
    char temp2[200];
    char *temp_ptr;


    /* find station and go there */
    temp_ptr = XmTextFieldGetString(locate_station_data);
    xastir_snprintf(locate_station_call,
        sizeof(locate_station_call),
        "%s",
        temp_ptr);
    XtFree(temp_ptr);

    (void)remove_trailing_spaces(locate_station_call);
    /*fprintf(stderr,"looking for %s\n",locate_station_call);*/
    if (locate_station(da, locate_station_call, (int)XmToggleButtonGetState(locate_case_data),
                    (int)XmToggleButtonGetState(locate_match_data),1) ==0) {
        xastir_snprintf(temp2, sizeof(temp2), langcode("POPEM00002"), locate_station_call);
        popup_message(langcode("POPEM00001"),temp2);
    }
    Locate_station_destroy_shell(w, clientData, callData);
}





// Here we pass in a 1 in callData if it's an emergency locate,
// for when we've received a Mic-E emergency packet.
//
void Locate_station(/*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, XtPointer callData) {
    static Widget pane, form, button_locate, button_cancel, call,
        button_lookup, sep;
    Atom delw;
    int emergency_flag = (int) callData;

 
    if (!locate_station_dialog) {

begin_critical_section(&locate_station_dialog_lock, "locate_gui.c:Locate_station" );

        if (emergency_flag == 1) {
                locate_station_dialog = XtVaCreatePopupShell(langcode("WPUPLSP006"),
                xmDialogShellWidgetClass,Global.top,
                XmNdeleteResponse,XmDESTROY,
                XmNdefaultPosition, FALSE,
                NULL);
        }
        else {  // Non-emergency locate
            locate_station_dialog = XtVaCreatePopupShell(langcode("WPUPLSP001"),
                xmDialogShellWidgetClass,Global.top,
                XmNdeleteResponse,XmDESTROY,
                XmNdefaultPosition, FALSE,
                NULL);
        }


        pane = XtVaCreateWidget("Locate_station pane",xmPanedWindowWidgetClass, locate_station_dialog,
                          XmNbackground, colors[0xff],
                          NULL);

        form =  XtVaCreateWidget("Locate_station form",xmFormWidgetClass, pane,
                            XmNfractionBase, 3,
                            XmNbackground, colors[0xff],
                            XmNautoUnmanage, FALSE,
                            XmNshadowThickness, 1,
                            NULL);

        call = XtVaCreateManagedWidget(langcode("WPUPLSP002"),xmLabelWidgetClass, form,
                                      XmNtopAttachment, XmATTACH_FORM,
                                      XmNtopOffset, 10,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset, 10,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        locate_station_data = XtVaCreateManagedWidget("Locate_station data", xmTextFieldWidgetClass, form,
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
                                      XmNrightAttachment,XmATTACH_FORM,
                                      XmNrightOffset, 10,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      NULL);

        locate_case_data  = XtVaCreateManagedWidget(langcode("WPUPLSP003"),xmToggleButtonWidgetClass,form,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, call,
                                      XmNtopOffset, 20,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset ,10,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      NULL);

        locate_match_data  = XtVaCreateManagedWidget(langcode("WPUPLSP004"),xmToggleButtonWidgetClass,form,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, call,
                                      XmNtopOffset, 20,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget,locate_case_data,
                                      XmNleftOffset ,20,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      NULL);

        sep = XtVaCreateManagedWidget("Locate_station sep", xmSeparatorGadgetClass,form,
                                      XmNorientation, XmHORIZONTAL,
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget,locate_case_data,
                                      XmNtopOffset, 10,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNrightAttachment,XmATTACH_FORM,
                                      XmNbackground, colors[0xff],
                                      NULL);

        button_lookup = XtVaCreateManagedWidget(langcode("WPUPLSP007"),xmPushButtonGadgetClass, form,
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
                                      XmNbackground, colors[0xff],
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      NULL);

        button_locate = XtVaCreateManagedWidget(langcode("WPUPLSP005"),xmPushButtonGadgetClass, form,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, sep,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment, XmATTACH_FORM,
                                      XmNbottomOffset, 5,
                                      XmNleftAttachment, XmATTACH_POSITION,
                                      XmNleftPosition, 1,
                                      XmNleftOffset, 5,
                                      XmNrightAttachment, XmATTACH_POSITION,
                                      XmNrightPosition, 2,
                                      XmNbackground, colors[0xff],
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      NULL);

        button_cancel = XtVaCreateManagedWidget(langcode("UNIOP00002"),xmPushButtonGadgetClass, form,
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
                                      XmNbackground, colors[0xff],
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      NULL);

        XtAddCallback(button_lookup, XmNactivateCallback, fcc_rac_lookup, locate_station_dialog);
        XtAddCallback(button_locate, XmNactivateCallback, Locate_station_now, locate_station_dialog);
        XtAddCallback(button_cancel, XmNactivateCallback, Locate_station_destroy_shell, locate_station_dialog);

        XmToggleButtonSetState(locate_case_data,FALSE,FALSE);
        XmToggleButtonSetState(locate_match_data,TRUE,FALSE);

        XmTextFieldSetString(locate_station_data,locate_station_call);

        pos_dialog(locate_station_dialog);

        delw = XmInternAtom(XtDisplay(locate_station_dialog),"WM_DELETE_WINDOW", FALSE);
        XmAddWMProtocolCallback(locate_station_dialog, delw, Locate_station_destroy_shell, (XtPointer)locate_station_dialog);

        XtManageChild(form);
        XtManageChild(pane);

end_critical_section(&locate_station_dialog_lock, "locate_gui.c:Locate_station" );

        XtPopup(locate_station_dialog,XtGrabNone);
        fix_dialog_size(locate_station_dialog);

        // Move focus to the Cancel button.  This appears to highlight the
        // button fine, but we're not able to hit the <Enter> key to
        // have that default function happen.  Note:  We _can_ hit the
        // <SPACE> key, and that activates the option.
//        XmUpdateDisplay(locate_station_dialog);
        XmProcessTraversal(button_cancel, XmTRAVERSE_CURRENT);

    } else {
        (void)XRaiseWindow(XtDisplay(locate_station_dialog), XtWindow(locate_station_dialog));
    }
}





/**** LOCATE PLACE ******/

void Locate_place_destroy_shell(/*@unused@*/ Widget widget, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    Widget shell = (Widget) clientData;
    XtPopdown(shell);

begin_critical_section(&locate_place_dialog_lock, "locate_gui.c:Locate_place_destroy_shell" );

    XtDestroyWidget(shell);
    locate_place_dialog = (Widget)NULL;

end_critical_section(&locate_place_dialog_lock, "locate_gui.c:Locate_place_destroy_shell" );

}





/*
 *  Locate a place by centering the map at its position
 */
void Locate_place_now(Widget w, XtPointer clientData, XtPointer callData) {
    char *temp_ptr;


    /* find place and go there */
    temp_ptr = XmTextFieldGetString(locate_place_data);
    xastir_snprintf(locate_place_name,
        sizeof(locate_place_name),
        "%s",
        temp_ptr);
    XtFree(temp_ptr);

    temp_ptr = XmTextFieldGetString(locate_state_data);
    xastir_snprintf(locate_state_name,
        sizeof(locate_state_name),
        "%s",
        temp_ptr);
    XtFree(temp_ptr);

    temp_ptr = XmTextFieldGetString(locate_county_data);
    xastir_snprintf(locate_county_name,
        sizeof(locate_county_name),
        "%s",
        temp_ptr);
    XtFree(temp_ptr);

    temp_ptr = XmTextFieldGetString(locate_quad_data);
    xastir_snprintf(locate_quad_name,
        sizeof(locate_quad_name),
        "%s",
        temp_ptr);
    XtFree(temp_ptr);

    temp_ptr = XmTextFieldGetString(locate_type_data);
    xastir_snprintf(locate_type_name,
        sizeof(locate_type_name),
        "%s",
        temp_ptr);
    XtFree(temp_ptr);

    temp_ptr = XmTextFieldGetString(locate_gnis_file_data);
    xastir_snprintf(locate_gnis_filename,
        sizeof(locate_gnis_filename),
        "%s",
        temp_ptr);
    XtFree(temp_ptr);

    (void)remove_trailing_spaces(locate_place_name);
    (void)remove_trailing_spaces(locate_state_name);
    (void)remove_trailing_spaces(locate_county_name);
    (void)remove_trailing_spaces(locate_quad_name);
    (void)remove_trailing_spaces(locate_type_name);

    /*fprintf(stderr,"looking for %s\n",locate_place_name);*/
    if (locate_place(da,
            locate_place_name,
            locate_state_name,
            locate_county_name,
            locate_quad_name,
            locate_type_name,
            locate_gnis_filename,
            (int)XmToggleButtonGetState(locate_place_case_data),
            (int)XmToggleButtonGetState(locate_place_match_data)) ==0) {
        popup_message(langcode("POPEM00025"),locate_place_name);
    }
    Locate_place_destroy_shell(w, clientData, callData);
}





void Locate_place(/*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer callData) {
    static Widget pane, form, button_ok, button_cancel, sep,
        place, state, county, quad, place_type, gnis_file;
    Atom delw;

    if (!locate_place_dialog) {

begin_critical_section(&locate_place_dialog_lock, "locate_gui.c:Locate_place" );

        locate_place_dialog = XtVaCreatePopupShell(langcode("PULDNMP014"),
                xmDialogShellWidgetClass,Global.top,
                XmNdeleteResponse,XmDESTROY,
                XmNdefaultPosition, FALSE,
                NULL);

        pane = XtVaCreateWidget("Locate_place pane",xmPanedWindowWidgetClass, locate_place_dialog,
                XmNbackground, colors[0xff],
                NULL);

        form =  XtVaCreateWidget("Locate_place form",xmFormWidgetClass, pane,
                XmNfractionBase, 2,
                XmNbackground, colors[0xff],
                XmNautoUnmanage, FALSE,
                XmNshadowThickness, 1,
                NULL);

        place = XtVaCreateManagedWidget(langcode("FEATURE001"),xmLabelWidgetClass, form,
                XmNtopAttachment, XmATTACH_FORM,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_NONE,
                XmNbackground, colors[0xff],
                NULL);

        locate_place_data = XtVaCreateManagedWidget("Locate_place_data", xmTextFieldWidgetClass, form,
                XmNeditable,   TRUE,
                XmNcursorPositionVisible, TRUE,
                XmNsensitive, TRUE,
                XmNshadowThickness,    1,
                XmNcolumns, 32,
                XmNwidth, ((32*7)+2),
                XmNmaxLength, 30,
                XmNbackground, colors[0x0f],
                XmNtopAttachment,XmATTACH_FORM,
                XmNtopOffset, 5,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, place,
                XmNleftOffset, 10,
                XmNrightAttachment,XmATTACH_FORM,
                XmNrightOffset, 10,
                XmNnavigationType, XmTAB_GROUP,
                XmNtraversalOn, TRUE,
                NULL);

        state = XtVaCreateManagedWidget(langcode("FEATURE002"),xmLabelWidgetClass, form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, place,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_NONE,
                XmNbackground, colors[0xff],
                NULL);

        locate_state_data = XtVaCreateManagedWidget("Locate_state_data", xmTextFieldWidgetClass, form,
                XmNeditable,   TRUE,
                XmNcursorPositionVisible, TRUE,
                XmNsensitive, TRUE,
                XmNshadowThickness,    1,
                XmNcolumns, 4,
                XmNwidth, ((4*7)+2),
                XmNmaxLength, 2,
                XmNbackground, colors[0x0f],
                XmNtopAttachment,XmATTACH_WIDGET,
                XmNtopWidget, place,
                XmNtopOffset, 5,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, state,
                XmNleftOffset, 10,
                XmNrightAttachment,XmATTACH_NONE,
                XmNrightOffset, 10,
                XmNnavigationType, XmTAB_GROUP,
                XmNtraversalOn, TRUE,
                NULL);

        county = XtVaCreateManagedWidget(langcode("FEATURE003"),xmLabelWidgetClass, form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, state,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_NONE,
                XmNbackground, colors[0xff],
                NULL);

        locate_county_data = XtVaCreateManagedWidget("Locate_county_data", xmTextFieldWidgetClass, form,
                XmNeditable,   TRUE,
                XmNcursorPositionVisible, TRUE,
                XmNsensitive, TRUE,
                XmNshadowThickness,    1,
                XmNcolumns, 32,
                XmNwidth, ((32*7)+2),
                XmNmaxLength, 30,
                XmNbackground, colors[0x0f],
                XmNtopAttachment,XmATTACH_WIDGET,
                XmNtopWidget, state,
                XmNtopOffset, 5,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, county,
                XmNleftOffset, 10,
                XmNrightAttachment,XmATTACH_FORM,
                XmNrightOffset, 10,
                XmNnavigationType, XmTAB_GROUP,
                XmNtraversalOn, TRUE,
                NULL);

        quad = XtVaCreateManagedWidget(langcode("FEATURE004"),xmLabelWidgetClass, form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, county,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_NONE,
                XmNbackground, colors[0xff],
                NULL);

        locate_quad_data = XtVaCreateManagedWidget("Locate_quad_data", xmTextFieldWidgetClass, form,
                XmNeditable,   TRUE,
                XmNcursorPositionVisible, TRUE,
                XmNsensitive, TRUE,
                XmNshadowThickness,    1,
                XmNcolumns, 32,
                XmNwidth, ((32*7)+2),
                XmNmaxLength, 30,
                XmNbackground, colors[0x0f],
                XmNtopAttachment,XmATTACH_WIDGET,
                XmNtopWidget, county,
                XmNtopOffset, 5,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, quad,
                XmNleftOffset, 10,
                XmNrightAttachment,XmATTACH_FORM,
                XmNrightOffset, 10,
                XmNnavigationType, XmTAB_GROUP,
                XmNtraversalOn, TRUE,
                NULL);

        place_type = XtVaCreateManagedWidget(langcode("FEATURE005"),xmLabelWidgetClass, form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, quad,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_NONE,
                XmNbackground, colors[0xff],
                NULL);

        locate_type_data = XtVaCreateManagedWidget("Locate_type_data", xmTextFieldWidgetClass, form,
                XmNeditable,   TRUE,
                XmNcursorPositionVisible, TRUE,
                XmNsensitive, TRUE,
                XmNshadowThickness,    1,
                XmNcolumns, 32,
                XmNwidth, ((32*7)+2),
                XmNmaxLength, 30,
                XmNbackground, colors[0x0f],
                XmNtopAttachment,XmATTACH_WIDGET,
                XmNtopWidget, quad,
                XmNtopOffset, 5,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, place_type,
                XmNleftOffset, 10,
                XmNrightAttachment,XmATTACH_FORM,
                XmNrightOffset, 10,
                XmNnavigationType, XmTAB_GROUP,
                XmNtraversalOn, TRUE,
                NULL);

        gnis_file = XtVaCreateManagedWidget(langcode("FEATURE006"),xmLabelWidgetClass, form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, place_type,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_NONE,
                XmNbackground, colors[0xff],
                NULL);

        locate_gnis_file_data = XtVaCreateManagedWidget("locate_gnis_file_data", xmTextFieldWidgetClass, form,
                XmNeditable,   TRUE,
                XmNcursorPositionVisible, TRUE,
                XmNsensitive, TRUE,
                XmNshadowThickness,    1,
                XmNcolumns, 40,
                XmNwidth, ((40*7)+2),
                XmNmaxLength, 199,
                XmNbackground, colors[0x0f],
                XmNtopAttachment,XmATTACH_WIDGET,
                XmNtopWidget, place_type,
                XmNtopOffset, 5,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, gnis_file,
                XmNleftOffset, 10,
                XmNrightAttachment,XmATTACH_FORM,
                XmNrightOffset, 10,
                XmNnavigationType, XmTAB_GROUP,
                XmNtraversalOn, TRUE,
                NULL);


        locate_place_case_data  = XtVaCreateManagedWidget(langcode("WPUPLSP003"),xmToggleButtonWidgetClass,form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, gnis_file,
                XmNtopOffset, 20,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset ,10,
                XmNrightAttachment, XmATTACH_NONE,
                XmNbackground, colors[0xff],
                XmNnavigationType, XmTAB_GROUP,
                XmNtraversalOn, TRUE,
                NULL);

        locate_place_match_data  = XtVaCreateManagedWidget(langcode("WPUPLSP004"),xmToggleButtonWidgetClass,form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, gnis_file,
                XmNtopOffset, 20,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget,locate_place_case_data,
                XmNleftOffset ,20,
                XmNrightAttachment, XmATTACH_NONE,
                XmNbackground, colors[0xff],
                XmNnavigationType, XmTAB_GROUP,
                XmNtraversalOn, TRUE,
                NULL);

        sep = XtVaCreateManagedWidget("Locate_place sep", xmSeparatorGadgetClass,form,
                XmNorientation, XmHORIZONTAL,
                XmNtopAttachment,XmATTACH_WIDGET,
                XmNtopWidget,locate_place_case_data,
                XmNtopOffset, 10,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNrightAttachment,XmATTACH_FORM,
                XmNbackground, colors[0xff],
                NULL);

        button_ok = XtVaCreateManagedWidget(langcode("WPUPLSP005"),xmPushButtonGadgetClass, form,
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
                XmNbackground, colors[0xff],
                XmNnavigationType, XmTAB_GROUP,
                XmNtraversalOn, TRUE,
                NULL);

        button_cancel = XtVaCreateManagedWidget(langcode("UNIOP00002"),xmPushButtonGadgetClass, form,
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
                XmNbackground, colors[0xff],
                XmNnavigationType, XmTAB_GROUP,
                XmNtraversalOn, TRUE,
                NULL);

        XtAddCallback(button_ok, XmNactivateCallback, Locate_place_now, locate_place_dialog);
        XtAddCallback(button_cancel, XmNactivateCallback, Locate_place_destroy_shell, locate_place_dialog);

        XmToggleButtonSetState(locate_place_case_data,FALSE,FALSE);
        XmToggleButtonSetState(locate_place_match_data,FALSE,FALSE);
//        XtSetSensitive(locate_place_match_data,FALSE);

        XmTextFieldSetString(locate_place_data,locate_place_name);
        XmTextFieldSetString(locate_state_data,locate_state_name);
        XmTextFieldSetString(locate_county_data,locate_county_name);
        XmTextFieldSetString(locate_quad_data,locate_quad_name);
        XmTextFieldSetString(locate_type_data,locate_type_name);
        XmTextFieldSetString(locate_gnis_file_data,locate_gnis_filename);

        pos_dialog(locate_place_dialog);

        delw = XmInternAtom(XtDisplay(locate_place_dialog),"WM_DELETE_WINDOW", FALSE);
        XmAddWMProtocolCallback(locate_place_dialog, delw, Locate_place_destroy_shell, (XtPointer)locate_place_dialog);

        XtManageChild(form);
        XtManageChild(pane);

end_critical_section(&locate_place_dialog_lock, "locate_gui.c:Locate_place" );

        XtPopup(locate_place_dialog,XtGrabNone);
        fix_dialog_size(locate_place_dialog);

        // Move focus to the Locate Now! button.  This appears to highlight the
        // button fine, but we're not able to hit the <Enter> key to
        // have that default function happen.  Note:  We _can_ hit the
        // <SPACE> key, and that activates the option.
//        XmUpdateDisplay(locate_place_dialog);
        XmProcessTraversal(button_ok, XmTRAVERSE_CURRENT);

    } else {
        (void)XRaiseWindow(XtDisplay(locate_place_dialog), XtWindow(locate_place_dialog));
    }
}


