/*
 * $Id$
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2005  The Xastir Group 
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
#include <unistd.h>
#include <termios.h>
#include <Xm/XmAll.h>

#include "xastir.h"
#include "main.h"
#include "xa_config.h"
#include "interface.h"
#include "wx.h"
#include "draw_symbols.h"
#include "util.h"

// Must be last include file
#include "leak_detection.h"



Widget configure_interface_dialog = NULL;
Widget choose_interface_dialog = NULL;
Widget interface_type_list = NULL;
Widget control_interface_dialog = NULL;
Widget control_iface_list = NULL;


static xastir_mutex control_interface_dialog_lock;

ioparam devices[MAX_IFACE_DEVICES];
xastir_mutex devices_lock;

void Choose_interface_destroy_shell(Widget widget, XtPointer clientData, XtPointer callData);
void modify_device_list(int option, int port);





void interface_gui_init(void)
{
    init_critical_section( &control_interface_dialog_lock );
    init_critical_section( &devices_lock );
}





/*****************************************************/
/* Universal Serial GUI                              */
/*****************************************************/
int device_speed;
int device_style;
int device_igate_options;
int device_data_type;





void speed_toggle( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;

    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;
    if (state->set)
        device_speed = atoi(which);
    else
        device_speed = 0;
}





void style_toggle( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;

    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if (state->set)
        device_style = atoi(which);
    else
        device_style = 0;
}





void data_toggle( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;

    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if (state->set)
        device_data_type = atoi(which);
    else
        device_data_type = 0;
}





void rain_gauge_toggle( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;

    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if (state->set)
        WX_rain_gauge_type = atoi(which);
    else
        WX_rain_gauge_type = 0;
}





void igate_toggle( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;

    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if (state->set)
        device_igate_options = atoi(which);
    else
        device_igate_options = 0;
}





void set_port_speed(int port) {

    switch (device_speed){
        case(1):
            devices[port].sp=B300;
            break;

        case(2):
            devices[port].sp=B1200;
            break;

        case(3):
            devices[port].sp=B2400;
            break;

        case(4):
            devices[port].sp=B4800;
            break;

        case(5):
            devices[port].sp=B9600;
            break;

        case(6):
            devices[port].sp=B19200;
            break;

        case(7):
            devices[port].sp=B38400;
            break;

        case(8):
            devices[port].sp=B57600;
            break;

        case(9):
            devices[port].sp=B115200;
            break;

        case(10):
#ifndef B230400
            devices[port].sp=B115200;
#else   // B230400
            devices[port].sp=B230400;
#endif  // B230400
            break;

        default:
            break;
    }
}







/*****************************************************/
/* Configure Serial TNC GUI                          */
/*****************************************************/

/**** TNC CONFIGURE ******/
int TNC_port;
int TNC_device;
Widget config_TNC_dialog = (Widget)NULL;
Widget TNC_active_on_startup;
Widget TNC_transmit_data;
Widget TNC_device_name_data;
Widget TNC_radio_port_data; // Used only for Multi-Port TNC's
Widget TNC_comment;
Widget TNC_unproto1_data;
Widget TNC_unproto2_data;
Widget TNC_unproto3_data;
Widget TNC_igate_data;
Widget TNC_up_file_data;
Widget TNC_down_file_data;
Widget TNC_txdelay;
Widget TNC_persistence;
Widget TNC_slottime;
Widget TNC_txtail;
Widget TNC_init_kiss;   // Used to initialize KISS-Mode
Widget TNC_fullduplex;
Widget TNC_GPS_set_time;
Widget TNC_relay_digipeat;




 
void Config_TNC_destroy_shell( /*@unused@*/ Widget widget, XtPointer clientData,  /*@unused@*/ XtPointer callData) {
    Widget shell = (Widget) clientData;
    XtPopdown(shell);
    XtDestroyWidget(shell);
    config_TNC_dialog = (Widget)NULL;
    if (choose_interface_dialog != NULL)
        Choose_interface_destroy_shell(choose_interface_dialog,choose_interface_dialog,NULL);
    choose_interface_dialog = (Widget)NULL;
}





void Config_TNC_change_data(Widget widget, XtPointer clientData, XtPointer callData) {
    int type;
    int was_up;
    char *temp_ptr;


    busy_cursor(appshell);

    was_up=0;
    if (get_device_status(TNC_port) == DEVICE_IN_USE) {
        /* if active shutdown before changes are made */
        /*fprintf(stderr,"Device is up, shutting down\n");*/

//WE7U:  Modify for MKISS?
        (void)del_device(TNC_port);

        was_up=1;
        usleep(1000000); // Wait for one second
    }

    /* device type */
    type=DEVICE_SERIAL_TNC; // Default in case not defined next
    if (TNC_device)
        type=TNC_device; // Modified to support more than two types

begin_critical_section(&devices_lock, "interface_gui.c:Config_TNC_change_data" );

    temp_ptr = XmTextFieldGetString(TNC_device_name_data);
    xastir_snprintf(devices[TNC_port].device_name,
        sizeof(devices[TNC_port].device_name),
        "%s",
        temp_ptr);
    XtFree(temp_ptr);

    (void)remove_trailing_spaces(devices[TNC_port].device_name);

    temp_ptr = XmTextFieldGetString(TNC_comment);
    xastir_snprintf(devices[TNC_port].comment,
        sizeof(devices[TNC_port].comment),
        "%s",
        temp_ptr);
    XtFree(temp_ptr);
    
    (void)remove_trailing_spaces(devices[TNC_port].comment);

    if (devices[TNC_port].device_type == DEVICE_SERIAL_MKISS_TNC) {

        // If MKISS, fetch "radio_port".  If empty, store a zero.
        temp_ptr = XmTextFieldGetString(TNC_radio_port_data);
        xastir_snprintf(devices[TNC_port].radio_port,
            sizeof(devices[TNC_port].radio_port),
            "%s",
            temp_ptr);
        XtFree(temp_ptr);

        (void)remove_trailing_spaces(devices[TNC_port].radio_port);

        if (strcmp(devices[TNC_port].radio_port,"") == 0) {
            xastir_snprintf(devices[TNC_port].radio_port,
                sizeof(devices[TNC_port].radio_port),
                "0");
        }
//fprintf(stderr,"Radio Port: %s\n",devices[TNC_port].radio_port);
    }

    if (XmToggleButtonGetState(TNC_active_on_startup))
        devices[TNC_port].connect_on_startup=1;
    else
        devices[TNC_port].connect_on_startup=0;

    if(XmToggleButtonGetState(TNC_transmit_data)) {
        devices[TNC_port].transmit_data=1;
        if ( (devices[TNC_port].device_type == DEVICE_SERIAL_KISS_TNC)
                || (devices[TNC_port].device_type == DEVICE_SERIAL_MKISS_TNC) ) {

#ifdef SERIAL_KISS_RELAY_DIGI
            XtSetSensitive(TNC_relay_digipeat, TRUE);
#else
            XtSetSensitive(TNC_relay_digipeat, FALSE);
#endif  // SERIAL_KISS_RELAY_DIGI

        }

    }
    else {
        devices[TNC_port].transmit_data=0;
        if ( (devices[TNC_port].device_type == DEVICE_SERIAL_KISS_TNC)
                || (devices[TNC_port].device_type == DEVICE_SERIAL_MKISS_TNC) ) {
            XtSetSensitive(TNC_relay_digipeat, FALSE);
        }
    }

    if ( (type == DEVICE_SERIAL_KISS_TNC)
            || (type == DEVICE_SERIAL_MKISS_TNC) ) {

        if (XmToggleButtonGetState(TNC_relay_digipeat))
            devices[TNC_port].relay_digipeat=1;
        else
            devices[TNC_port].relay_digipeat=0;
    }

    switch(type) {

        case DEVICE_SERIAL_TNC_HSP_GPS:
        case DEVICE_SERIAL_TNC_AUX_GPS:
            if (XmToggleButtonGetState(TNC_GPS_set_time))
                devices[TNC_port].set_time=1;
            else
                devices[TNC_port].set_time=0;
            break;

        case DEVICE_SERIAL_TNC:
        case DEVICE_SERIAL_KISS_TNC:
        case DEVICE_SERIAL_MKISS_TNC:
        default:
            break;
    }

    set_port_speed(TNC_port);

    devices[TNC_port].style=device_style;
    devices[TNC_port].igate_options=device_igate_options;

    temp_ptr = XmTextFieldGetString(TNC_unproto1_data);
    xastir_snprintf(devices[TNC_port].unproto1,
        sizeof(devices[TNC_port].unproto1),
        "%s",
        temp_ptr);
    XtFree(temp_ptr);

    (void)remove_trailing_spaces(devices[TNC_port].unproto1);

    if(check_unproto_path(devices[TNC_port].unproto1)) {
        popup_message_always(langcode("WPUPCFT042"),
            langcode("WPUPCFT043"));
    }

    temp_ptr = XmTextFieldGetString(TNC_unproto2_data);
    xastir_snprintf(devices[TNC_port].unproto2,
        sizeof(devices[TNC_port].unproto2),
        "%s",
        temp_ptr);
    XtFree(temp_ptr);

    (void)remove_trailing_spaces(devices[TNC_port].unproto2);

    if(check_unproto_path(devices[TNC_port].unproto2)) {
        popup_message_always(langcode("WPUPCFT042"),
            langcode("WPUPCFT043"));
    }

    temp_ptr = XmTextFieldGetString(TNC_unproto3_data);
    xastir_snprintf(devices[TNC_port].unproto3,
        sizeof(devices[TNC_port].unproto3),
        "%s",
        temp_ptr);
    XtFree(temp_ptr);

    (void)remove_trailing_spaces(devices[TNC_port].unproto3);

    if(check_unproto_path(devices[TNC_port].unproto3)) {
        popup_message_always(langcode("WPUPCFT042"),
            langcode("WPUPCFT043"));
    }

    temp_ptr = XmTextFieldGetString(TNC_igate_data);
    xastir_snprintf(devices[TNC_port].unproto_igate,
        sizeof(devices[TNC_port].unproto_igate),
        "%s",
        temp_ptr);
    XtFree(temp_ptr);

    (void)remove_trailing_spaces(devices[TNC_port].unproto_igate);

    if(check_unproto_path(devices[TNC_port].unproto_igate)) {
        popup_message_always(langcode("WPUPCFT044"),
            langcode("WPUPCFT043"));
    }


    if ( (type == DEVICE_SERIAL_KISS_TNC)
            || (type == DEVICE_SERIAL_MKISS_TNC) ) {

        // KISS TNC, no up/down files for this one!
        devices[TNC_port].tnc_up_file[0] = '\0';
        devices[TNC_port].tnc_down_file[0] = '\0';

        // Instead we have KISS parameters to set

// We really should do some validation of these strings

//WE7U:  Modify for MKISS:  Must send to the proper Radio Port.
        temp_ptr = XmTextFieldGetString(TNC_txdelay);
        xastir_snprintf(devices[TNC_port].txdelay,
            sizeof(devices[TNC_port].txdelay),
            "%s",
            temp_ptr);
        XtFree(temp_ptr);

        send_kiss_config(TNC_port,0,0x01,atoi(devices[TNC_port].txdelay));

        temp_ptr = XmTextFieldGetString(TNC_persistence);
        xastir_snprintf(devices[TNC_port].persistence,
            sizeof(devices[TNC_port].persistence),
            "%s",
            temp_ptr);
        XtFree(temp_ptr);

        send_kiss_config(TNC_port,0,0x02,atoi(devices[TNC_port].persistence));

        temp_ptr = XmTextFieldGetString(TNC_slottime);
        xastir_snprintf(devices[TNC_port].slottime,
            sizeof(devices[TNC_port].slottime),
            "%s",
            temp_ptr);
        XtFree(temp_ptr);

        send_kiss_config(TNC_port,0,0x03,atoi(devices[TNC_port].slottime));

        temp_ptr = XmTextFieldGetString(TNC_txtail);
        xastir_snprintf(devices[TNC_port].txtail,
            sizeof(devices[TNC_port].txtail),
            "%s",
            temp_ptr);
        XtFree(temp_ptr);

        send_kiss_config(TNC_port,0,0x04,atoi(devices[TNC_port].txtail));
 
        if (XmToggleButtonGetState(TNC_fullduplex))
            devices[TNC_port].fullduplex=1;
        else
            devices[TNC_port].fullduplex=0;
        send_kiss_config(TNC_port,0,0x05,devices[TNC_port].fullduplex);

        // For KISS-mode
        if (XmToggleButtonGetState(TNC_init_kiss))
            devices[TNC_port].init_kiss=1;
        else
            devices[TNC_port].init_kiss=0;
    }
    else {
        temp_ptr = XmTextFieldGetString(TNC_up_file_data);
        xastir_snprintf(devices[TNC_port].tnc_up_file,
            sizeof(devices[TNC_port].tnc_up_file),
            "%s",
            temp_ptr);
        XtFree(temp_ptr);

        (void)remove_trailing_spaces(devices[TNC_port].tnc_up_file);

        temp_ptr = XmTextFieldGetString(TNC_down_file_data);
        xastir_snprintf(devices[TNC_port].tnc_down_file,
            sizeof(devices[TNC_port].tnc_down_file),
            "%s",
            temp_ptr);
        XtFree(temp_ptr);

        (void)remove_trailing_spaces(devices[TNC_port].tnc_down_file);
    }

//WE7U:  Modify for MKISS?
    /* reopen port*/
    if (was_up) {
        (void)add_device(TNC_port,
            type,
            devices[TNC_port].device_name,
            "",
            -1,
            devices[TNC_port].sp,
            devices[TNC_port].style,
            0,
            "");
    }

    /* delete list */
//    modify_device_list(4,0);


    /* add device type */
    devices[TNC_port].device_type=type;

    /* rebuild list */
//    modify_device_list(3,0);

end_critical_section(&devices_lock, "interface_gui.c:Config_TNC_change_data" );

    // Rebuild the interface control list
    update_interface_list();

    Config_TNC_destroy_shell(widget,clientData,callData);
}





void Config_TNC( /*@unused@*/ Widget w, int device_type, int config_type, int port) {
    static Widget  pane, form, form2, button_ok, button_cancel,
                frame, frame2, frame3, frame4,
                setup, setup1, setup2, setup3, setup4,
                device, comment, speed, speed_box,
                speed_300, speed_1200, speed_2400, speed_4800, speed_9600,
                speed_19200, speed_38400, speed_57600, speed_115200, speed_230400,
                style, style_box,
                style_8n1, style_7e1, style_7o1,
                igate, igate_box,
                igate_o_0, igate_o_1, igate_o_2,
                igate_label,
                proto, proto1, proto2, proto3,
                radio_port_label;
    char temp[50];
    Atom delw;
    Arg al[50];                      /* Arg List */
    register unsigned int ac = 0;   /* Arg Count */
        register char *tmp;

        tmp=(char *)NULL;

    if(!config_TNC_dialog) {
        TNC_port=port;
        TNC_device=device_type;
/* config_TNC_dialog = XtVaCreatePopupShell(device_type ? langcode("WPUPCFT023"):langcode("WPUPCFT001"),
 -- replaced by KB6MER with the lines below for adding AUX GPS type TNC
*/
        switch(device_type) {
            case DEVICE_SERIAL_TNC:
                tmp=langcode("WPUPCFT001");
                break;

            case DEVICE_SERIAL_KISS_TNC:
                tmp=langcode("WPUPCFT030");
                break;

            case DEVICE_SERIAL_MKISS_TNC:
                tmp=langcode("WPUPCFT040");
                break;

            case DEVICE_SERIAL_TNC_HSP_GPS:
                tmp=langcode("WPUPCFT023");
                break;

            case DEVICE_SERIAL_TNC_AUX_GPS:
                tmp=langcode("WPUPCFT028");
                if (debug_level & 128) {
                    fprintf(stderr,"Storing %d to gps_retrieve for %d\n",
                    DEFAULT_GPS_RETR, port);
                }
                devices[port].gps_retrieve=DEFAULT_GPS_RETR;
                break;

            default:
                sprintf(tmp, langcode("WPUPCFT029"), (int)device_type);
                break;
        }

        config_TNC_dialog = XtVaCreatePopupShell(
                            tmp,
                            xmDialogShellWidgetClass, appshell,
                            XmNdeleteResponse,XmDESTROY,
                            XmNdefaultPosition, FALSE,
                            NULL);

        pane = XtVaCreateWidget("Config_TNC pane",xmPanedWindowWidgetClass, config_TNC_dialog,
                          XmNbackground, colors[0xff],
                          NULL);

        form =  XtVaCreateWidget("Config_TNC form",xmFormWidgetClass, pane,
                            XmNfractionBase, 5,
                            XmNbackground, colors[0xff],
                            XmNautoUnmanage, FALSE,
                            XmNshadowThickness, 1,
                            NULL);

        TNC_active_on_startup = XtVaCreateManagedWidget(langcode("UNIOP00011"),xmToggleButtonWidgetClass,form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNtopAttachment, XmATTACH_FORM,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset ,10,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        TNC_transmit_data = XtVaCreateManagedWidget(langcode("UNIOP00010"),xmToggleButtonWidgetClass,form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNtopAttachment, XmATTACH_FORM,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, TNC_active_on_startup,
                                      XmNleftOffset ,35,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        switch(device_type) {
            case DEVICE_SERIAL_TNC_HSP_GPS:
            case DEVICE_SERIAL_TNC_AUX_GPS:
                TNC_GPS_set_time  = XtVaCreateManagedWidget(langcode("UNIOP00029"), xmToggleButtonWidgetClass, form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNtopAttachment, XmATTACH_FORM,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, TNC_transmit_data,
                                      XmNleftOffset ,35,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

// We can only set the time properly on Linux systems
#ifndef HAVE_SETTIMEOFDAY
                XtSetSensitive(TNC_GPS_set_time,FALSE);
#endif  // HAVE_SETTIMEOFDAY
#ifdef __CYGWIN__
                XtSetSensitive(TNC_GPS_set_time,FALSE);
#endif  // __CYGWIN__

                break;
            case DEVICE_SERIAL_KISS_TNC:
            case DEVICE_SERIAL_MKISS_TNC:
                // Add a "RELAY Digipeat?" button for KISS/MKISS TNC's
                TNC_relay_digipeat = XtVaCreateManagedWidget(langcode("UNIOP00030"),xmToggleButtonWidgetClass,form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNtopAttachment, XmATTACH_FORM,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, TNC_transmit_data,
                                      XmNleftOffset ,35,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

#ifdef SERIAL_KISS_RELAY_DIGI
                XtSetSensitive(TNC_relay_digipeat, TRUE);
#else
                XtSetSensitive(TNC_relay_digipeat, FALSE);
#endif  // SERIAL_KISS_RELAY_DIGIPEAT

                break;
 
            case DEVICE_SERIAL_TNC:
            default:
                break;
        }

        device = XtVaCreateManagedWidget(langcode("WPUPCFT003"),xmLabelWidgetClass, form,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, TNC_active_on_startup,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset, 10,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        TNC_device_name_data = XtVaCreateManagedWidget("Config_TNC device_data", xmTextFieldWidgetClass, form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNeditable,   TRUE,
                                      XmNcursorPositionVisible, TRUE,
                                      XmNsensitive, TRUE,
                                      XmNshadowThickness,    1,
                                      XmNcolumns, 15,
                                      XmNwidth, ((15*7)+2),
                                      XmNmaxLength, 40,
                                      XmNbackground, colors[0x0f],
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, TNC_active_on_startup,
                                      XmNtopOffset, 2,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, device,
                                      XmNleftOffset, 12,
                                      XmNrightAttachment,XmATTACH_NONE,
                                      NULL);

        comment = XtVaCreateManagedWidget(langcode("WPUPCFS017"),xmLabelWidgetClass, form,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, TNC_active_on_startup,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, TNC_device_name_data,
                                      XmNleftOffset, 10,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        TNC_comment = XtVaCreateManagedWidget("Config_TNC comment", xmTextFieldWidgetClass, form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNeditable,   TRUE,
                                      XmNcursorPositionVisible, TRUE,
                                      XmNsensitive, TRUE,
                                      XmNshadowThickness,    1,
                                      XmNcolumns, 15,
                                      XmNwidth, ((15*7)+2),
                                      XmNmaxLength, 49,
                                      XmNbackground, colors[0x0f],
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, TNC_active_on_startup,
                                      XmNtopOffset, 2,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, comment,
                                      XmNleftOffset, 12,
                                      XmNrightAttachment,XmATTACH_NONE,
                                      NULL);

        if (device_type ==  DEVICE_SERIAL_MKISS_TNC) {
            // "Radio Port" field for Multi-Port KISS TNC's.

            radio_port_label = XtVaCreateManagedWidget(langcode("WPUPCFT041"),
                                      xmLabelWidgetClass, form,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, TNC_active_on_startup,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, TNC_comment,
                                      XmNleftOffset, 10,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

            TNC_radio_port_data = XtVaCreateManagedWidget("Config_TNC device_data",
                                      xmTextFieldWidgetClass, form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNeditable,   TRUE,
                                      XmNcursorPositionVisible, TRUE,
                                      XmNsensitive, TRUE,
                                      XmNshadowThickness,    1,
                                      XmNcolumns, 5,
                                      XmNwidth, ((5*7)+2),
                                      XmNmaxLength, 2,
                                      XmNbackground, colors[0x0f],
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, TNC_active_on_startup,
                                      XmNtopOffset, 2,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, radio_port_label,
                                      XmNleftOffset, 12,
                                      XmNrightAttachment,XmATTACH_NONE,
                                      NULL);
        }

        frame = XtVaCreateManagedWidget("Config_TNC frame", xmFrameWidgetClass, form,
                                    XmNtopAttachment,XmATTACH_WIDGET,
                                    XmNtopOffset, 10,
                                    XmNtopWidget, device,
                                    XmNbottomAttachment,XmATTACH_NONE,
                                    XmNleftAttachment, XmATTACH_FORM,
                                    XmNleftOffset, 10,
                                    XmNrightAttachment,XmATTACH_FORM,
                                    XmNrightOffset, 10,
                                    XmNbackground, colors[0xff],
                                    NULL);

        speed = XtVaCreateManagedWidget(langcode("WPUPCFT004"),xmLabelWidgetClass, frame,
                                    XmNchildType, XmFRAME_TITLE_CHILD,
                                    XmNbackground, colors[0xff],
                                    NULL);

        /*set args for color */
        ac=0;
        XtSetArg(al[ac], XmNbackground, colors[0xff]); ac++;

        speed_box = XmCreateRadioBox(frame,"Config_TNC Speed_box",al,ac);
        XtVaSetValues(speed_box,XmNnumColumns,5,NULL);

        speed_300 = XtVaCreateManagedWidget(langcode("WPUPCFT005"),xmToggleButtonGadgetClass,
                                        speed_box,
                                        XmNbackground, colors[0xff],
                                        NULL);

        XtAddCallback(speed_300,XmNvalueChangedCallback,speed_toggle,"1");

        speed_1200 = XtVaCreateManagedWidget(langcode("WPUPCFT006"),xmToggleButtonGadgetClass,
                                        speed_box,
                                        XmNbackground, colors[0xff],
                                        NULL);
        XtAddCallback(speed_1200,XmNvalueChangedCallback,speed_toggle,"2");


        speed_2400 = XtVaCreateManagedWidget(langcode("WPUPCFT007"),xmToggleButtonGadgetClass,
                                        speed_box,
                                        XmNbackground, colors[0xff],
                                        NULL);
        XtAddCallback(speed_2400,XmNvalueChangedCallback,speed_toggle,"3");


        speed_4800 = XtVaCreateManagedWidget(langcode("WPUPCFT008"),xmToggleButtonGadgetClass,
                                        speed_box,
                                        XmNbackground, colors[0xff],
                                        NULL);
        XtAddCallback(speed_4800,XmNvalueChangedCallback,speed_toggle,"4");

        speed_9600 = XtVaCreateManagedWidget(langcode("WPUPCFT009"),xmToggleButtonGadgetClass,
                                        speed_box,
                                        XmNbackground, colors[0xff],
                                        NULL);
        XtAddCallback(speed_9600,XmNvalueChangedCallback,speed_toggle,"5");

        speed_19200 = XtVaCreateManagedWidget(langcode("WPUPCFT010"),xmToggleButtonGadgetClass,
                                        speed_box,
                                        XmNbackground, colors[0xff],
                                        NULL);
        XtAddCallback(speed_19200,XmNvalueChangedCallback,speed_toggle,"6");

        speed_38400 = XtVaCreateManagedWidget(langcode("WPUPCFT019"),xmToggleButtonGadgetClass,
                                        speed_box,
                                        XmNbackground, colors[0xff],
                                        NULL);
        XtAddCallback(speed_38400,XmNvalueChangedCallback,speed_toggle,"7");

        speed_57600 = XtVaCreateManagedWidget(langcode("WPUPCFT020"),xmToggleButtonGadgetClass,
                                        speed_box,
                                        XmNbackground, colors[0xff],
                                        NULL);
        XtAddCallback(speed_57600,XmNvalueChangedCallback,speed_toggle,"8");

        speed_115200 = XtVaCreateManagedWidget(langcode("WPUPCFT021"),xmToggleButtonGadgetClass,
                                        speed_box,
                                        XmNbackground, colors[0xff],
                                        NULL);
        XtAddCallback(speed_115200,XmNvalueChangedCallback,speed_toggle,"9");

        speed_230400 = XtVaCreateManagedWidget(langcode("WPUPCFT022"),xmToggleButtonGadgetClass,
                                        speed_box,
                                        XmNbackground, colors[0xff],
                                        NULL);
        XtAddCallback(speed_230400,XmNvalueChangedCallback,speed_toggle,"10");

        switch(device_type) {
            case DEVICE_SERIAL_KISS_TNC:
            case DEVICE_SERIAL_MKISS_TNC:
                break;
            default:
                frame2 = XtVaCreateManagedWidget("Config_TNC frame2", xmFrameWidgetClass, form,
                                     XmNtopAttachment, XmATTACH_WIDGET,
                                     XmNtopWidget, frame,
                                     XmNtopOffset, 10,
                                     XmNbottomAttachment, XmATTACH_NONE,
                                     XmNleftAttachment, XmATTACH_FORM,
                                     XmNleftOffset, 10,
                                     XmNrightAttachment, XmATTACH_FORM,
                                     XmNrightOffset, 10,
                                     XmNbackground, colors[0xff],
                                     NULL);

                style = XtVaCreateManagedWidget(langcode("WPUPCFT015"),xmLabelWidgetClass, frame2,
                                    XmNchildType, XmFRAME_TITLE_CHILD,
                                    XmNbackground, colors[0xff],
                                    NULL);


                style_box = XmCreateRadioBox(frame2,"Config_TNC Style box",al,ac);

                XtVaSetValues(style_box,XmNorientation, XmHORIZONTAL,NULL);

                style_8n1 = XtVaCreateManagedWidget(langcode("WPUPCFT016"),xmToggleButtonGadgetClass,
                                        style_box,
                                        XmNbackground, colors[0xff],
                                        NULL);
                XtAddCallback(style_8n1,XmNvalueChangedCallback,style_toggle,"0");

                style_7e1 = XtVaCreateManagedWidget(langcode("WPUPCFT017"),xmToggleButtonGadgetClass,
                                        style_box,
                                        XmNbackground, colors[0xff],
                                        NULL);
                XtAddCallback(style_7e1,XmNvalueChangedCallback,style_toggle,"1");

                style_7o1 = XtVaCreateManagedWidget(langcode("WPUPCFT018"),xmToggleButtonGadgetClass,
                                        style_box,
                                        XmNbackground, colors[0xff],
                                        NULL);
                XtAddCallback(style_7o1,XmNvalueChangedCallback,style_toggle,"2");
            break;
        }

        frame4 = XtVaCreateManagedWidget("Config_TNC frame4", xmFrameWidgetClass, form,
                                     XmNtopAttachment, XmATTACH_WIDGET,
XmNtopWidget, (device_type == DEVICE_SERIAL_KISS_TNC || device_type == DEVICE_SERIAL_MKISS_TNC) ? frame : frame2,
                                     XmNtopOffset, 10,
                                     XmNbottomAttachment, XmATTACH_NONE,
                                     XmNleftAttachment, XmATTACH_FORM,
                                     XmNleftOffset, 10,
                                     XmNrightAttachment, XmATTACH_FORM,
                                     XmNrightOffset, 10,
                                     XmNbackground, colors[0xff],
                                     NULL);

        igate = XtVaCreateManagedWidget(langcode("IGPUPCF000"),xmLabelWidgetClass, frame4,
                                    XmNchildType, XmFRAME_TITLE_CHILD,
                                    XmNbackground, colors[0xff],
                                    NULL);

        igate_box = XmCreateRadioBox(frame4,"Config_TNC IGate box",al,ac);

        XtVaSetValues(igate_box,XmNorientation, XmVERTICAL,XmNnumColumns,2,NULL);

        igate_o_0 = XtVaCreateManagedWidget(langcode("IGPUPCF001"),xmToggleButtonGadgetClass,
                                        igate_box,
                                        XmNbackground, colors[0xff],
                                        NULL);
        XtAddCallback(igate_o_0,XmNvalueChangedCallback,igate_toggle,"0");

        igate_o_1 = XtVaCreateManagedWidget(langcode("IGPUPCF002"),xmToggleButtonGadgetClass,
                                        igate_box,
                                        XmNbackground, colors[0xff],
                                        NULL);
        XtAddCallback(igate_o_1,XmNvalueChangedCallback,igate_toggle,"1");

        igate_o_2 = XtVaCreateManagedWidget(langcode("IGPUPCF003"),xmToggleButtonGadgetClass,
                                        igate_box,
                                        XmNbackground, colors[0xff],
                                        NULL);
        XtAddCallback(igate_o_2,XmNvalueChangedCallback,igate_toggle,"2");

        proto = XtVaCreateManagedWidget(langcode("WPUPCFT011"), xmLabelWidgetClass, form,
                                      XmNorientation, XmHORIZONTAL,
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, frame4,
                                      XmNtopOffset, 10,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset, 5,
                                      XmNrightAttachment,XmATTACH_FORM,
                                      XmNrightOffset, 5,
                                      XmNbackground, colors[0xff],
                                      NULL);

        xastir_snprintf(temp, sizeof(temp), langcode("WPUPCFT012"), VERSIONFRM);

        proto1 = XtVaCreateManagedWidget(temp, xmLabelWidgetClass, form,
                                      XmNorientation, XmHORIZONTAL,
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, proto,
                                      XmNtopOffset, 12,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset, 45,
                                      XmNrightAttachment,XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        TNC_unproto1_data = XtVaCreateManagedWidget("Config_TNC protopath1", xmTextFieldWidgetClass, form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNeditable,   TRUE,
                                      XmNcursorPositionVisible, TRUE,
                                      XmNsensitive, TRUE,
                                      XmNshadowThickness,    1,
                                      XmNcolumns, 25,
                                      XmNwidth, ((25*7)+2),
                                      XmNmaxLength, 40,
                                      XmNbackground, colors[0x0f],
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, proto,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment,XmATTACH_WIDGET,
                                      XmNleftWidget, proto1,
                                      XmNleftOffset, 5,
                                      XmNrightAttachment,XmATTACH_NONE,
                                      NULL);

        xastir_snprintf(temp, sizeof(temp), langcode("WPUPCFT013"), VERSIONFRM);

        proto2 = XtVaCreateManagedWidget(temp, xmLabelWidgetClass, form,
                                      XmNorientation, XmHORIZONTAL,
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, proto1,
                                      XmNtopOffset, 15,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset, 45,
                                      XmNrightAttachment,XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        TNC_unproto2_data = XtVaCreateManagedWidget("Config_TNC protopath2", xmTextFieldWidgetClass, form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNeditable,   TRUE,
                                      XmNcursorPositionVisible, TRUE,
                                      XmNsensitive, TRUE,
                                      XmNshadowThickness,    1,
                                      XmNcolumns, 25,
                                      XmNwidth, ((25*7)+2),
                                      XmNmaxLength, 40,
                                      XmNbackground, colors[0x0f],
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, TNC_unproto1_data,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, proto2,
                                      XmNleftOffset, 5,
                                      XmNrightAttachment,XmATTACH_NONE,
                                      NULL);

        xastir_snprintf(temp, sizeof(temp), langcode("WPUPCFT014"), VERSIONFRM);

        proto3 = XtVaCreateManagedWidget(temp, xmLabelWidgetClass, form,
                                      XmNorientation, XmHORIZONTAL,
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, proto2,
                                      XmNtopOffset, 15,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset, 45,
                                      XmNrightAttachment,XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        TNC_unproto3_data = XtVaCreateManagedWidget("Config_TNC protopath3", xmTextFieldWidgetClass, form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNeditable,   TRUE,
                                      XmNcursorPositionVisible, TRUE,
                                      XmNsensitive, TRUE,
                                      XmNshadowThickness,    1,
                                      XmNcolumns, 25,
                                      XmNwidth, ((25*7)+2),
                                      XmNmaxLength, 40,
                                      XmNbackground, colors[0x0f],
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, TNC_unproto2_data,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment,XmATTACH_WIDGET,
                                      XmNleftWidget, proto3,
                                      XmNleftOffset, 5,
                                      XmNrightAttachment,XmATTACH_NONE,
                                      NULL);

        xastir_snprintf(temp, sizeof(temp), langcode("IGPUPCF004"));
        igate_label = XtVaCreateManagedWidget(temp, xmLabelWidgetClass, form,
                                      XmNorientation, XmHORIZONTAL,
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, proto3,
                                      XmNtopOffset, 15,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset, 45,
                                      XmNrightAttachment,XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        TNC_igate_data = XtVaCreateManagedWidget("Config_TNC igate_data", xmTextFieldWidgetClass, form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNeditable,   TRUE,
                                      XmNcursorPositionVisible, TRUE,
                                      XmNsensitive, TRUE,
                                      XmNshadowThickness,    1,
                                      XmNcolumns, 25,
                                      XmNwidth, ((25*7)+2),
                                      XmNmaxLength, 40,
                                      XmNbackground, colors[0x0f],
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, TNC_unproto3_data,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment,XmATTACH_WIDGET,
                                      XmNleftWidget, igate_label,
                                      XmNleftOffset, 5,
                                      XmNrightAttachment,XmATTACH_NONE,
                                      NULL);

// Draw a different frame3 for Serial KISS/MKISS TNC interfaces
        switch(device_type) {
            case DEVICE_SERIAL_KISS_TNC:
            case DEVICE_SERIAL_MKISS_TNC:
                frame3 = XtVaCreateManagedWidget("Config_TNC frame3", xmFrameWidgetClass, form,
                                    XmNtopAttachment,XmATTACH_WIDGET,
                                    XmNtopOffset,10,
                                    XmNtopWidget, TNC_igate_data,
                                    XmNbottomAttachment,XmATTACH_NONE,
                                    XmNleftAttachment, XmATTACH_FORM,
                                    XmNleftOffset, 10,
                                    XmNrightAttachment,XmATTACH_FORM,
                                    XmNrightOffset, 10,
                                    XmNbackground, colors[0xff],
                                    NULL);

                // KISS Parameters
                setup = XtVaCreateManagedWidget(langcode("WPUPCFT034"),xmLabelWidgetClass, frame3,
                                    XmNchildType, XmFRAME_TITLE_CHILD,
                                    XmNbackground, colors[0xff],
                                    NULL);

                form2 =  XtVaCreateWidget("Config_TNC form2",xmFormWidgetClass, frame3,
                                    XmNfractionBase, 6,
                                    XmNbackground, colors[0xff],
                                    NULL);

                // TXDelay (10 ms units)
                setup1 = XtVaCreateManagedWidget(langcode("WPUPCFT035"), xmLabelWidgetClass, form2,
                                    XmNtopAttachment,XmATTACH_FORM,
                                    XmNtopOffset, 10,
                                    XmNbottomAttachment,XmATTACH_NONE,
                                    XmNleftAttachment, XmATTACH_FORM,
                                    XmNleftOffset, 10,
                                    XmNrightAttachment,XmATTACH_NONE,
                                    XmNbackground, colors[0xff],
                                    NULL);

                TNC_txdelay = XtVaCreateManagedWidget("Config_TNC TNC_txdelay", xmTextFieldWidgetClass, form2,
                                    XmNnavigationType, XmTAB_GROUP,
                                    XmNtraversalOn, TRUE,
                                    XmNeditable,   TRUE,
                                    XmNcursorPositionVisible, TRUE,
                                    XmNsensitive, TRUE,
                                    XmNshadowThickness,    1,
                                    XmNcolumns, 3,
                                    XmNwidth, ((6*7)+2),
                                    XmNmaxLength, 3,
                                    XmNbackground, colors[0x0f],
                                    XmNtopAttachment,XmATTACH_FORM,
                                    XmNtopOffset, 5,
                                    XmNbottomAttachment,XmATTACH_NONE,
                                    XmNleftAttachment,XmATTACH_POSITION,
                                    XmNleftPosition, 2,
                                    XmNrightAttachment,XmATTACH_NONE,
                                    NULL);

                // Persistence (0 to 255)
                setup2 = XtVaCreateManagedWidget(langcode("WPUPCFT036"), xmLabelWidgetClass, form2,
                                    XmNtopAttachment,XmATTACH_WIDGET,
                                    XmNtopWidget, setup1,
                                    XmNtopOffset, 10,
                                    XmNbottomAttachment,XmATTACH_NONE,
                                    XmNleftAttachment, XmATTACH_FORM,
                                    XmNleftOffset, 10,
                                    XmNrightAttachment,XmATTACH_NONE,
                                    XmNbackground, colors[0xff],
                                    NULL);

                TNC_persistence = XtVaCreateManagedWidget("Config_TNC persistence", xmTextFieldWidgetClass, form2,
                                    XmNnavigationType, XmTAB_GROUP,
                                    XmNtraversalOn, TRUE,
                                    XmNeditable,   TRUE,
                                    XmNcursorPositionVisible, TRUE,
                                    XmNsensitive, TRUE,
                                    XmNshadowThickness,    1,
                                    XmNcolumns, 3,
                                    XmNwidth, ((6*7)+2),
                                    XmNmaxLength, 3,
                                    XmNbackground, colors[0x0f],
                                    XmNtopAttachment,XmATTACH_WIDGET,
                                    XmNtopWidget, setup1,
                                    XmNtopOffset, 5,
                                    XmNbottomAttachment,XmATTACH_NONE,
                                    XmNleftAttachment,XmATTACH_POSITION,
                                    XmNleftPosition, 2,
                                    XmNrightAttachment,XmATTACH_NONE,
                                    NULL);

                // SlotTime (10 ms units)
                setup3 = XtVaCreateManagedWidget(langcode("WPUPCFT037"), xmLabelWidgetClass, form2,
                                    XmNtopAttachment,XmATTACH_FORM,
                                    XmNtopOffset, 10,
                                    XmNbottomAttachment,XmATTACH_NONE,
                                    XmNleftAttachment, XmATTACH_POSITION,
                                    XmNleftPosition, 3,
                                    XmNrightAttachment,XmATTACH_NONE,
                                    XmNbackground, colors[0xff],
                                    NULL);

                TNC_slottime = XtVaCreateManagedWidget("Config_TNC slottime", xmTextFieldWidgetClass, form2,
                                    XmNnavigationType, XmTAB_GROUP,
                                    XmNtraversalOn, TRUE,
                                    XmNeditable,   TRUE,
                                    XmNcursorPositionVisible, TRUE,
                                    XmNsensitive, TRUE,
                                    XmNshadowThickness,    1,
                                    XmNcolumns, 3,
                                    XmNwidth, ((6*7)+2),
                                    XmNmaxLength, 3,
                                    XmNbackground, colors[0x0f],
                                    XmNtopAttachment,XmATTACH_FORM,
                                    XmNtopOffset, 5,
                                    XmNbottomAttachment,XmATTACH_NONE,
                                    XmNleftAttachment,XmATTACH_POSITION,
                                    XmNleftPosition, 5,
                                    XmNrightAttachment,XmATTACH_NONE,
                                    NULL);

                // TxTail (10 ms units)
                setup4 = XtVaCreateManagedWidget(langcode("WPUPCFT038"), xmLabelWidgetClass, form2,
                                    XmNtopAttachment,XmATTACH_WIDGET,
                                    XmNtopWidget, setup3,
                                    XmNtopOffset, 10,
                                    XmNbottomAttachment,XmATTACH_NONE,
                                    XmNleftAttachment, XmATTACH_POSITION,
                                    XmNleftPosition, 3,
                                    XmNrightAttachment,XmATTACH_NONE,
                                    XmNbackground, colors[0xff],
                                    NULL);

                TNC_txtail = XtVaCreateManagedWidget("Config_TNC TxTail", xmTextFieldWidgetClass, form2,
                                    XmNnavigationType, XmTAB_GROUP,
                                    XmNtraversalOn, TRUE,
                                    XmNeditable,   TRUE,
                                    XmNcursorPositionVisible, TRUE,
                                    XmNsensitive, TRUE,
                                    XmNshadowThickness,    1,
                                    XmNcolumns, 3,
                                    XmNwidth, ((6*7)+2),
                                    XmNmaxLength, 3,
                                    XmNbackground, colors[0x0f],
                                    XmNtopAttachment,XmATTACH_WIDGET,
                                    XmNtopWidget, setup3,
                                    XmNtopOffset, 5,
                                    XmNbottomAttachment,XmATTACH_NONE,
                                    XmNleftAttachment,XmATTACH_POSITION,
                                    XmNleftPosition, 5,
                                    XmNrightAttachment,XmATTACH_NONE,
                                    NULL);

                // Full Duplex
                TNC_fullduplex = XtVaCreateManagedWidget(langcode("WPUPCFT039"),xmToggleButtonWidgetClass,form2,
                                    XmNnavigationType, XmTAB_GROUP,
                                    XmNtraversalOn, TRUE,
                                    XmNtopAttachment, XmATTACH_WIDGET,
                                    XmNtopWidget, setup4,
                                    XmNtopOffset, 5,
                                    XmNbottomAttachment, XmATTACH_FORM,
                                    XmNbottomOffset, 5,
                                    XmNleftAttachment, XmATTACH_FORM,
                                    XmNleftOffset, 10,
                                    XmNrightAttachment, XmATTACH_NONE,
                                    XmNbackground, colors[0xff],
                                    NULL);

                // Button to enable KISS-mode at startup
                TNC_init_kiss = XtVaCreateManagedWidget(langcode("WPUPCFT047"),xmToggleButtonWidgetClass,form2,
                                    XmNnavigationType, XmTAB_GROUP,
                                    XmNtraversalOn, TRUE,
                                    XmNtopAttachment, XmATTACH_WIDGET,
                                    XmNtopWidget, setup4,
                                    XmNtopOffset, 5,
                                    XmNbottomAttachment, XmATTACH_FORM,
                                    XmNbottomOffset, 5,
                                    XmNleftAttachment, XmATTACH_FORM,
                                    XmNleftOffset, 135,
                                    XmNrightAttachment, XmATTACH_NONE,
                                    XmNbackground, colors[0xff],
                                    NULL);
                break;
            default:
                frame3 = XtVaCreateManagedWidget("Config_TNC frame3", xmFrameWidgetClass, form,
                                    XmNtopAttachment,XmATTACH_WIDGET,
                                    XmNtopOffset,10,
                                    XmNtopWidget, TNC_igate_data,
                                    XmNbottomAttachment,XmATTACH_NONE,
                                    XmNleftAttachment, XmATTACH_FORM,
                                    XmNleftOffset, 10,
                                    XmNrightAttachment,XmATTACH_FORM,
                                    XmNrightOffset, 10,
                                    XmNbackground, colors[0xff],
                                    NULL);

                setup = XtVaCreateManagedWidget(langcode("WPUPCFT031"),xmLabelWidgetClass, frame3,
                                    XmNchildType, XmFRAME_TITLE_CHILD,
                                    XmNbackground, colors[0xff],
                                    NULL);

                form2 =  XtVaCreateWidget("Config_TNC form2",xmFormWidgetClass, frame3,
                                    XmNfractionBase, 5,
                                    XmNbackground, colors[0xff],
                                    NULL);

                setup1 = XtVaCreateManagedWidget(langcode("WPUPCFT032"), xmLabelWidgetClass, form2,
                                    XmNtopAttachment,XmATTACH_FORM,
                                    XmNtopOffset, 10,
                                    XmNbottomAttachment,XmATTACH_NONE,
                                    XmNleftAttachment, XmATTACH_FORM,
                                    XmNrightAttachment,XmATTACH_NONE,
                                    XmNbackground, colors[0xff],
                                    NULL);

                TNC_up_file_data = XtVaCreateManagedWidget("Config_TNC up_file", xmTextFieldWidgetClass, form2,
                                    XmNnavigationType, XmTAB_GROUP,
                                    XmNtraversalOn, TRUE,
                                    XmNeditable,   TRUE,
                                    XmNcursorPositionVisible, TRUE,
                                    XmNsensitive, TRUE,
                                    XmNshadowThickness,    1,
                                    XmNcolumns, 25,
                                    XmNwidth, ((25*7)+2),
                                    XmNmaxLength, 80,
                                    XmNbackground, colors[0x0f],
                                    XmNtopAttachment,XmATTACH_FORM,
                                    XmNtopOffset, 5,
                                    XmNbottomAttachment,XmATTACH_NONE,
                                    XmNleftAttachment,XmATTACH_POSITION,
                                    XmNleftPosition, 2,
                                    XmNrightAttachment,XmATTACH_NONE,
                                    NULL);

                setup2 = XtVaCreateManagedWidget(langcode("WPUPCFT033"), xmLabelWidgetClass, form2,
                                    XmNtopAttachment,XmATTACH_WIDGET,
                                    XmNtopWidget, setup1,
                                    XmNtopOffset, 10,
                                    XmNbottomAttachment,XmATTACH_NONE,
                                    XmNleftAttachment, XmATTACH_FORM,
                                    XmNrightAttachment,XmATTACH_NONE,
                                    XmNbackground, colors[0xff],
                                    NULL);

                TNC_down_file_data = XtVaCreateManagedWidget("Config_TNC down_file", xmTextFieldWidgetClass, form2,
                                    XmNnavigationType, XmTAB_GROUP,
                                    XmNtraversalOn, TRUE,
                                    XmNeditable,   TRUE,
                                    XmNcursorPositionVisible, TRUE,
                                    XmNsensitive, TRUE,
                                    XmNshadowThickness,    1,
                                    XmNcolumns, 25,
                                    XmNwidth, ((25*7)+2),
                                    XmNmaxLength, 80,
                                    XmNbackground, colors[0x0f],
                                    XmNtopAttachment,XmATTACH_WIDGET,
                                    XmNtopWidget, setup1,
                                    XmNtopOffset, 5,
                                    XmNbottomAttachment,XmATTACH_NONE,
                                    XmNleftAttachment,XmATTACH_POSITION,
                                    XmNleftPosition, 2,
                                    XmNrightAttachment,XmATTACH_NONE,
                                    NULL);

                break;
        }


//------------------------------------------------------------

        button_ok = XtVaCreateManagedWidget(langcode("UNIOP00001"),xmPushButtonGadgetClass, form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, frame3,
                                      XmNtopOffset, 10,
                                      XmNbottomAttachment, XmATTACH_FORM,
                                      XmNbottomOffset, 5,
                                      XmNleftAttachment, XmATTACH_POSITION,
                                      XmNleftPosition, 1,
                                      XmNrightAttachment, XmATTACH_POSITION,
                                      XmNrightPosition, 2,
                                      XmNbackground, colors[0xff],
                                      NULL);

        button_cancel = XtVaCreateManagedWidget(langcode("UNIOP00002"),xmPushButtonGadgetClass, form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, frame3,
                                      XmNtopOffset, 10,
                                      XmNbottomAttachment, XmATTACH_FORM,
                                      XmNbottomOffset, 5,
                                      XmNleftAttachment, XmATTACH_POSITION,
                                      XmNleftPosition, 3,
                                      XmNrightAttachment, XmATTACH_POSITION,
                                      XmNrightPosition, 4,
                                      XmNbackground, colors[0xff],
                                      NULL);

        XtAddCallback(button_ok, XmNactivateCallback, Config_TNC_change_data, config_TNC_dialog);
        XtAddCallback(button_cancel, XmNactivateCallback, Config_TNC_destroy_shell, config_TNC_dialog);

        pos_dialog(config_TNC_dialog);

        delw = XmInternAtom(XtDisplay(config_TNC_dialog),"WM_DELETE_WINDOW", FALSE);
        XmAddWMProtocolCallback(config_TNC_dialog, delw, Config_TNC_destroy_shell, (XtPointer)config_TNC_dialog);

        if (config_type==0) {
            /* first time port */
            devices[TNC_port].gps_retrieve=DEFAULT_GPS_RETR;
            if (debug_level & 128) {
                fprintf(stderr,"Storing %d to gps_retrieve for %d\n",
                DEFAULT_GPS_RETR, port);
            }

            XmTextFieldSetString(TNC_device_name_data,TNC_PORT);

            XmTextFieldSetString(TNC_comment,"");

            if (device_type == DEVICE_SERIAL_MKISS_TNC) {
                XmTextFieldSetString(TNC_radio_port_data,"0");
//fprintf(stderr,"Assigning default '0' to radio port\n");
            }

            XmToggleButtonSetState(TNC_active_on_startup,TRUE,FALSE);
            XmToggleButtonSetState(TNC_transmit_data,TRUE,FALSE);

            switch(device_type) {
                case DEVICE_SERIAL_TNC_HSP_GPS:
                case DEVICE_SERIAL_TNC_AUX_GPS:
                    XmToggleButtonSetState(TNC_GPS_set_time, FALSE, FALSE);
                    break;
                case DEVICE_SERIAL_KISS_TNC:
                case DEVICE_SERIAL_MKISS_TNC:
                    XmToggleButtonSetState(TNC_relay_digipeat, FALSE, FALSE);
                    XmToggleButtonSetState(TNC_fullduplex, FALSE, FALSE);
                    XmToggleButtonSetState(TNC_init_kiss, FALSE, FALSE); // For KISS-Mode
                    break;
                case DEVICE_SERIAL_TNC:
                default:
                    break;
            }

            XmToggleButtonSetState(speed_4800,TRUE,FALSE);
            device_speed=4;

            if ( (device_type != DEVICE_SERIAL_KISS_TNC)
                    && (device_type != DEVICE_SERIAL_MKISS_TNC) ) {
                XmToggleButtonSetState(style_8n1,TRUE,FALSE);
            }

            device_style=0;

            device_igate_options=0;
            XmToggleButtonSetState(igate_o_0,TRUE,FALSE);
            XmTextFieldSetString(TNC_unproto1_data,"WIDE2-2");
            XmTextFieldSetString(TNC_unproto2_data,"");
            XmTextFieldSetString(TNC_unproto3_data,"");
            XmTextFieldSetString(TNC_igate_data,"");

//WE7U
            if ( (device_type == DEVICE_SERIAL_KISS_TNC)
                    || (device_type == DEVICE_SERIAL_MKISS_TNC) ) {
                // We don't allow changing the selection for KISS
                // TNC's, as they require 8N1
                device_style = 0;
                XmTextFieldSetString(TNC_txdelay,"40");
                XmTextFieldSetString(TNC_persistence,"63");
                XmTextFieldSetString(TNC_slottime,"20");
                XmTextFieldSetString(TNC_txtail,"30");
            }
            else {
                XmTextFieldSetString(TNC_up_file_data,"tnc-startup.sys");
                XmTextFieldSetString(TNC_down_file_data,"tnc-stop.sys");
            }

        } else {
            /* reconfig */

            if (debug_level & 128)
                fprintf(stderr,"Reconfiguring interface\n");

begin_critical_section(&devices_lock, "interface_gui.c:Config_TNC" );

            XmTextFieldSetString(TNC_device_name_data,devices[TNC_port].device_name);

            XmTextFieldSetString(TNC_comment,devices[TNC_port].comment);

            if (device_type == DEVICE_SERIAL_MKISS_TNC) {
                XmTextFieldSetString(TNC_radio_port_data, devices[TNC_port].radio_port);
//fprintf(stderr,"Reconfig: %s\n", devices[TNC_port].radio_port);
            }

            if (devices[TNC_port].connect_on_startup)
                XmToggleButtonSetState(TNC_active_on_startup,TRUE,FALSE);
            else
                XmToggleButtonSetState(TNC_active_on_startup,FALSE,FALSE);

            if (devices[TNC_port].transmit_data)
                XmToggleButtonSetState(TNC_transmit_data,TRUE,FALSE);
            else
                XmToggleButtonSetState(TNC_transmit_data,FALSE,FALSE);

            switch(device_type) {

                case DEVICE_SERIAL_TNC_HSP_GPS:
                case DEVICE_SERIAL_TNC_AUX_GPS:
                    if (devices[TNC_port].set_time)
                        XmToggleButtonSetState(TNC_GPS_set_time, TRUE, FALSE);
                    else
                        XmToggleButtonSetState(TNC_GPS_set_time, FALSE, FALSE);
                    break;

                case  DEVICE_SERIAL_KISS_TNC:
                case  DEVICE_SERIAL_MKISS_TNC:

                    if (devices[TNC_port].relay_digipeat)
                        XmToggleButtonSetState(TNC_relay_digipeat, TRUE, FALSE);
                    else
                        XmToggleButtonSetState(TNC_relay_digipeat, FALSE, FALSE);

                    if (devices[TNC_port].fullduplex)
                        XmToggleButtonSetState(TNC_fullduplex, TRUE, FALSE);
                    else
                        XmToggleButtonSetState(TNC_fullduplex, FALSE, FALSE);

                    // For KISS-Mode
                    if (devices[TNC_port].init_kiss)
                        XmToggleButtonSetState(TNC_init_kiss, TRUE, FALSE);
                    else
                        XmToggleButtonSetState(TNC_init_kiss, FALSE, FALSE);

                    if (devices[TNC_port].transmit_data) {

#ifdef SERIAL_KISS_RELAY_DIGI
                        XtSetSensitive(TNC_relay_digipeat, TRUE);
#else
                        XtSetSensitive(TNC_relay_digipeat, FALSE);
#endif  // SERIAL_KISS_RELAY_DIGI

                    }
                    else
                        XtSetSensitive(TNC_relay_digipeat, FALSE);
                    break;

                case DEVICE_SERIAL_TNC:
                default:
                    break;
            }

            switch (devices[TNC_port].sp) {
                case(B300):
                    XmToggleButtonSetState(speed_300,TRUE,FALSE);
                    device_speed=1;
                    break;

                case(B1200):
                    XmToggleButtonSetState(speed_1200,TRUE,FALSE);
                    device_speed=2;
                    break;

                case(B2400):
                    XmToggleButtonSetState(speed_2400,TRUE,FALSE);
                    device_speed=3;
                    break;

                case(B4800):
                    XmToggleButtonSetState(speed_4800,TRUE,FALSE);
                    device_speed=4;
                    break;

                case(B9600):
                    XmToggleButtonSetState(speed_9600,TRUE,FALSE);
                    device_speed=5;
                    break;

                case(B19200):
                    XmToggleButtonSetState(speed_19200,TRUE,FALSE);
                    device_speed=6;
                    break;

                case(B38400):
                    XmToggleButtonSetState(speed_38400,TRUE,FALSE);
                    device_speed=7;
                    break;

                case(B57600):
                    XmToggleButtonSetState(speed_57600,TRUE,FALSE);
                    device_speed=8;
                    break;

                case(B115200):
                    XmToggleButtonSetState(speed_115200,TRUE,FALSE);
                    device_speed=9;
                    break;

#ifdef B230400
                case(B230400):
                    XmToggleButtonSetState(speed_230400,TRUE,FALSE);
                    device_speed=10;
                    break;
#endif  // B230400

                default:
                    XmToggleButtonSetState(speed_4800,TRUE,FALSE);
                    device_speed=4;
                    break;
            }

            if ( (device_type == DEVICE_SERIAL_KISS_TNC)
                    || (device_type == DEVICE_SERIAL_MKISS_TNC) ) {
                // We don't allow changing the selection for KISS
                // TNC's, as they require 8N1
                device_style = 0;
            }
            else {
                switch (devices[TNC_port].style) {
                    case(0):
                        XmToggleButtonSetState(style_8n1,TRUE,FALSE);
                        device_style=0;
                        break;
    
                    case(1):
                        XmToggleButtonSetState(style_7e1,TRUE,FALSE);
                        device_style=1;
                        break;

                    case(2):
                        XmToggleButtonSetState(style_7o1,TRUE,FALSE);
                        device_style=2;
                        break;

                    default:
                        XmToggleButtonSetState(style_8n1,TRUE,FALSE);
                        device_style=0;
                        break;
                }
            }

            switch (devices[TNC_port].igate_options) {
                case(0):
                    XmToggleButtonSetState(igate_o_0,TRUE,FALSE);
                    device_igate_options=0;
                    break;

                case(1):
                    XmToggleButtonSetState(igate_o_1,TRUE,FALSE);
                    device_igate_options=1;
                    break;

                case(2):
                    XmToggleButtonSetState(igate_o_2,TRUE,FALSE);
                    device_igate_options=2;
                    break;

                default:
                    XmToggleButtonSetState(igate_o_0,TRUE,FALSE);
                    device_igate_options=0;
                    break;
            }
            XmTextFieldSetString(TNC_unproto1_data,devices[TNC_port].unproto1);
            XmTextFieldSetString(TNC_unproto2_data,devices[TNC_port].unproto2);
            XmTextFieldSetString(TNC_unproto3_data,devices[TNC_port].unproto3);
            XmTextFieldSetString(TNC_igate_data,devices[TNC_port].unproto_igate);

            if ( (device_type == DEVICE_SERIAL_KISS_TNC)
                    || (device_type == DEVICE_SERIAL_MKISS_TNC) ) {
                XmTextFieldSetString(TNC_txdelay,devices[TNC_port].txdelay);
                XmTextFieldSetString(TNC_persistence,devices[TNC_port].persistence);
                XmTextFieldSetString(TNC_slottime,devices[TNC_port].slottime);
                XmTextFieldSetString(TNC_txtail,devices[TNC_port].txtail);
            }
            else {
                XmTextFieldSetString(TNC_up_file_data,devices[TNC_port].tnc_up_file);
                XmTextFieldSetString(TNC_down_file_data,devices[TNC_port].tnc_down_file);
            }

end_critical_section(&devices_lock, "interface_gui.c:Config_TNC" );

        }

        XtManageChild(form);

        XtManageChild(form2);

        XtManageChild(speed_box);

        if ( (device_type != DEVICE_SERIAL_KISS_TNC)
                && (device_type != DEVICE_SERIAL_MKISS_TNC) ) {
            XtManageChild(style_box);
        }

        XtManageChild(igate_box);
        XtManageChild(pane);

        XtPopup(config_TNC_dialog,XtGrabNone);
        fix_dialog_size(config_TNC_dialog);
    }
    else {
        (void)XRaiseWindow(XtDisplay(config_TNC_dialog), XtWindow(config_TNC_dialog));
    }
}





/*****************************************************/
/* Configure Serial GPS GUI                          */
/*****************************************************/

/**** GPS CONFIGURE ******/
int GPS_port;
Widget config_GPS_dialog = (Widget)NULL;
Widget GPS_device_name_data;
Widget GPS_comment;
Widget GPS_active_on_startup;
Widget GPS_set_time;





void Config_GPS_destroy_shell( /*@unused@*/ Widget widget, XtPointer clientData,  /*@unused@*/ XtPointer callData) {
    Widget shell = (Widget) clientData;
    XtPopdown(shell);
    XtDestroyWidget(shell);
    config_GPS_dialog = (Widget)NULL;
    if (choose_interface_dialog != NULL)
        Choose_interface_destroy_shell(choose_interface_dialog,choose_interface_dialog,NULL);
    choose_interface_dialog = (Widget)NULL;
}





void Config_GPS_change_data(Widget widget, XtPointer clientData, XtPointer callData) {
    int was_up;
    char *temp_ptr;


    busy_cursor(appshell);
    was_up=0;
    if (get_device_status(GPS_port) == DEVICE_IN_USE) {
        /* if active shutdown before changes are made */
        /*fprintf(stderr,"Device is up, shutting down\n");*/
        (void)del_device(GPS_port);
        was_up=1;
        usleep(1000000); // Wait for one second
    }

begin_critical_section(&devices_lock, "interface_gui.c:Config_GPS_change_data" );

    temp_ptr = XmTextFieldGetString(GPS_device_name_data);
    xastir_snprintf(devices[GPS_port].device_name,
        sizeof(devices[GPS_port].device_name),
        "%s",
        temp_ptr);
    XtFree(temp_ptr);

    (void)remove_trailing_spaces(devices[GPS_port].device_name);

    temp_ptr = XmTextFieldGetString(GPS_comment);
    xastir_snprintf(devices[GPS_port].comment,
        sizeof(devices[GPS_port].comment),
        "%s",
        temp_ptr);
    XtFree(temp_ptr);

    (void)remove_trailing_spaces(devices[GPS_port].comment);

    if(XmToggleButtonGetState(GPS_active_on_startup))
        devices[GPS_port].connect_on_startup=1;
    else
        devices[GPS_port].connect_on_startup=0;

    if (XmToggleButtonGetState(GPS_set_time))
        devices[GPS_port].set_time=1;
    else
        devices[GPS_port].set_time=0;

    set_port_speed(GPS_port);
    devices[GPS_port].style=device_style;
    /* reopen */
    if ( was_up ) {
        (void)add_device(GPS_port,
            DEVICE_SERIAL_GPS,
            devices[GPS_port].device_name,
            "",
            -1,
            devices[GPS_port].sp,
            devices[GPS_port].style,
            0,
            "");
    }

    /* delete list */
//    modify_device_list(4,0);


    /* add device type */
    devices[GPS_port].device_type=DEVICE_SERIAL_GPS;

    /* rebuild list */
//    modify_device_list(3,0);


end_critical_section(&devices_lock, "interface_gui.c:Config_GPS_change_data" );

    // Rebuild the interface control list
    update_interface_list();

    Config_GPS_destroy_shell(widget,clientData,callData);
}





void Config_GPS( /*@unused@*/ Widget w, int config_type, int port) {
    static Widget  pane, form, button_ok, button_cancel,
                frame, frame2,
                device, comment, speed, speed_box,
                speed_300, speed_1200, speed_2400, speed_4800, speed_9600,
                speed_19200, speed_38400, speed_57600, speed_115200, speed_230400,
                style, style_box,
                style_8n1, style_7e1, style_7o1,
                sep;
    Atom delw;
    Arg al[50];                    /* Arg List */
    register unsigned int ac = 0;           /* Arg Count */

    if(!config_GPS_dialog) {
        GPS_port=port;
        config_GPS_dialog = XtVaCreatePopupShell(langcode("WPUPCFG001"),
            xmDialogShellWidgetClass, appshell,
            XmNdeleteResponse,XmDESTROY,
            XmNdefaultPosition, FALSE,
            NULL);

        pane = XtVaCreateWidget("Config_GPS pane",xmPanedWindowWidgetClass, config_GPS_dialog,
            XmNbackground, colors[0xff],
            NULL);

        form =  XtVaCreateWidget("Config_GPS form",xmFormWidgetClass, pane,
            XmNfractionBase, 5,
            XmNbackground, colors[0xff],
            XmNautoUnmanage, FALSE,
            XmNshadowThickness, 1,
            NULL);

        device = XtVaCreateManagedWidget(langcode("WPUPCFG003"),xmLabelWidgetClass, form,
            XmNtopAttachment, XmATTACH_FORM,
            XmNtopOffset, 10,
            XmNbottomAttachment, XmATTACH_NONE,
            XmNleftAttachment, XmATTACH_FORM,
            XmNleftOffset, 10,
            XmNrightAttachment, XmATTACH_NONE,
            XmNbackground, colors[0xff],
            NULL);

        GPS_device_name_data = XtVaCreateManagedWidget("Config_GPS device_data", xmTextFieldWidgetClass, form,
            XmNnavigationType, XmTAB_GROUP,
            XmNtraversalOn, TRUE,
            XmNeditable,   TRUE,
            XmNcursorPositionVisible, TRUE,
            XmNsensitive, TRUE,
            XmNshadowThickness,    1,
            XmNcolumns, 25,
            XmNwidth, ((25*7)+2),
            XmNmaxLength, 40,
            XmNbackground, colors[0x0f],
            XmNtopAttachment,XmATTACH_FORM,
            XmNtopOffset, 5,
            XmNbottomAttachment,XmATTACH_NONE,
            XmNleftAttachment, XmATTACH_WIDGET,
            XmNleftWidget, device,
            XmNleftOffset, 10,
            XmNrightAttachment,XmATTACH_FORM,
            XmNrightOffset, 5,
            NULL);

        comment = XtVaCreateManagedWidget(langcode("WPUPCFS017"),xmLabelWidgetClass, form,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, device,
            XmNtopOffset, 10,
            XmNbottomAttachment, XmATTACH_NONE,
            XmNleftAttachment, XmATTACH_FORM,
            XmNleftOffset, 15,
            XmNrightAttachment, XmATTACH_NONE,
            XmNbackground, colors[0xff],
            NULL);

        GPS_comment = XtVaCreateManagedWidget("Config_GPS comment", xmTextFieldWidgetClass, form,
            XmNnavigationType, XmTAB_GROUP,
            XmNtraversalOn, TRUE,
            XmNeditable,   TRUE,
            XmNcursorPositionVisible, TRUE,
            XmNsensitive, TRUE,
            XmNshadowThickness,    1,
            XmNcolumns, 25,
            XmNwidth, ((25*7)+2),
            XmNmaxLength, 49,
            XmNbackground, colors[0x0f],
            XmNtopAttachment,XmATTACH_WIDGET,
            XmNtopWidget, device,
            XmNtopOffset, 10,
            XmNbottomAttachment,XmATTACH_NONE,
            XmNleftAttachment, XmATTACH_WIDGET,
            XmNleftWidget, comment,
            XmNleftOffset, 10,
            XmNrightAttachment,XmATTACH_FORM,
            XmNrightOffset, 5,
            NULL);

        GPS_active_on_startup = XtVaCreateManagedWidget(langcode("UNIOP00011"),xmToggleButtonWidgetClass,form,
            XmNnavigationType, XmTAB_GROUP,
            XmNtraversalOn, TRUE,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, comment,
            XmNtopOffset, 10,
            XmNbottomAttachment, XmATTACH_NONE,
            XmNleftAttachment, XmATTACH_FORM,
            XmNleftOffset ,10,
            XmNrightAttachment, XmATTACH_NONE,
            XmNbackground, colors[0xff],
            NULL);

        GPS_set_time  = XtVaCreateManagedWidget(langcode("UNIOP00029"), xmToggleButtonWidgetClass, form,
            XmNnavigationType, XmTAB_GROUP,
            XmNtraversalOn, TRUE,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, GPS_active_on_startup,
            XmNtopOffset, 7,
            XmNbottomAttachment, XmATTACH_NONE,
            XmNleftAttachment, XmATTACH_FORM,
            XmNleftOffset ,10,
            XmNrightAttachment, XmATTACH_NONE,
            XmNbackground, colors[0xff],
            NULL);

// We can only set the time properly on Linux systems
#ifndef HAVE_SETTIMEOFDAY
        XtSetSensitive(GPS_set_time,FALSE);
#endif  // HAVE_SETTIMEOFDAY
#ifdef __CYGWIN__
        XtSetSensitive(GPS_set_time,FALSE);
#endif  // __CYGWIN__


 
        frame = XtVaCreateManagedWidget("Config_GPS frame", xmFrameWidgetClass, form,
            XmNtopAttachment,XmATTACH_WIDGET,
            XmNtopOffset,10,
            XmNtopWidget, GPS_set_time,
            XmNbottomAttachment,XmATTACH_NONE,
            XmNleftAttachment, XmATTACH_FORM,
            XmNleftOffset, 10,
            XmNrightAttachment,XmATTACH_FORM,
            XmNrightOffset, 10,
            XmNbackground, colors[0xff],
            NULL);

        speed = XtVaCreateManagedWidget(langcode("WPUPCFT004"),xmLabelWidgetClass, frame,
            XmNchildType, XmFRAME_TITLE_CHILD,
            XmNbackground, colors[0xff],
            NULL);

        /*set args for color */
        ac=0;
        XtSetArg(al[ac], XmNbackground, colors[0xff]); ac++;


        speed_box = XmCreateRadioBox(frame,"Config_GPS Speed_box",al,ac);

        XtVaSetValues(speed_box,XmNnumColumns,3,NULL);

        speed_300 = XtVaCreateManagedWidget(langcode("WPUPCFT005"),xmToggleButtonGadgetClass,
            speed_box,
            XmNbackground, colors[0xff],
            NULL);
        XtAddCallback(speed_300,XmNvalueChangedCallback,speed_toggle,"1");

        speed_1200 = XtVaCreateManagedWidget(langcode("WPUPCFT006"),xmToggleButtonGadgetClass,
            speed_box,
            XmNbackground, colors[0xff],
            NULL);
        XtAddCallback(speed_1200,XmNvalueChangedCallback,speed_toggle,"2");

        speed_2400 = XtVaCreateManagedWidget(langcode("WPUPCFT007"),xmToggleButtonGadgetClass,
            speed_box,
            XmNbackground, colors[0xff],
            NULL);
        XtAddCallback(speed_2400,XmNvalueChangedCallback,speed_toggle,"3");

        speed_4800 = XtVaCreateManagedWidget(langcode("WPUPCFT008"),xmToggleButtonGadgetClass,
            speed_box,
            XmNbackground, colors[0xff],
            NULL);
        XtAddCallback(speed_4800,XmNvalueChangedCallback,speed_toggle,"4");

        speed_9600 = XtVaCreateManagedWidget(langcode("WPUPCFT009"),xmToggleButtonGadgetClass,
            speed_box,
            XmNbackground, colors[0xff],
            NULL);
        XtAddCallback(speed_9600,XmNvalueChangedCallback,speed_toggle,"5");

        speed_19200 = XtVaCreateManagedWidget(langcode("WPUPCFT010"),xmToggleButtonGadgetClass,
            speed_box,
            XmNbackground, colors[0xff],
            NULL);
        XtAddCallback(speed_19200,XmNvalueChangedCallback,speed_toggle,"6");

        speed_38400 = XtVaCreateManagedWidget(langcode("WPUPCFT019"),xmToggleButtonGadgetClass,
            speed_box,
            XmNbackground, colors[0xff],
            NULL);
        XtAddCallback(speed_38400,XmNvalueChangedCallback,speed_toggle,"7");

        speed_57600 = XtVaCreateManagedWidget(langcode("WPUPCFT020"),xmToggleButtonGadgetClass,
            speed_box,
            XmNbackground, colors[0xff],
            NULL);
        XtAddCallback(speed_57600,XmNvalueChangedCallback,speed_toggle,"8");

        speed_115200 = XtVaCreateManagedWidget(langcode("WPUPCFT021"),xmToggleButtonGadgetClass,
            speed_box,
            XmNbackground, colors[0xff],
            NULL);
        XtAddCallback(speed_115200,XmNvalueChangedCallback,speed_toggle,"9");

        speed_230400 = XtVaCreateManagedWidget(langcode("WPUPCFT022"),xmToggleButtonGadgetClass,
            speed_box,
            XmNbackground, colors[0xff],
            NULL);
        XtAddCallback(speed_230400,XmNvalueChangedCallback,speed_toggle,"10");

        frame2 = XtVaCreateManagedWidget("Config_GPS frame2", xmFrameWidgetClass, form,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, frame,
            XmNtopOffset, 10,
            XmNbottomAttachment, XmATTACH_NONE,
            XmNleftAttachment, XmATTACH_FORM,
            XmNleftOffset, 10,
            XmNrightAttachment, XmATTACH_FORM,
            XmNrightOffset, 10,
            XmNbackground, colors[0xff],
            NULL);

        style = XtVaCreateManagedWidget(langcode("WPUPCFT015"),xmLabelWidgetClass, frame2,
            XmNchildType, XmFRAME_TITLE_CHILD,
            XmNbackground, colors[0xff],
            NULL);

        style_box = XmCreateRadioBox(frame2,"Config_GPS Style box",al,ac);

        XtVaSetValues(style_box,
            XmNorientation, XmHORIZONTAL,
            NULL);

        style_8n1 = XtVaCreateManagedWidget(langcode("WPUPCFT016"),xmToggleButtonGadgetClass,
            style_box,
            XmNbackground, colors[0xff],
            NULL);
        XtAddCallback(style_8n1,XmNvalueChangedCallback,style_toggle,"0");

        style_7e1 = XtVaCreateManagedWidget(langcode("WPUPCFT017"),xmToggleButtonGadgetClass,
            style_box,
            XmNbackground, colors[0xff],
            NULL);
        XtAddCallback(style_7e1,XmNvalueChangedCallback,style_toggle,"1");

        style_7o1 = XtVaCreateManagedWidget(langcode("WPUPCFT018"),xmToggleButtonGadgetClass,
            style_box,
            XmNbackground, colors[0xff],
            NULL);
        XtAddCallback(style_7o1,XmNvalueChangedCallback,style_toggle,"2");

        sep = XtVaCreateManagedWidget("Config_GPS sep", xmSeparatorGadgetClass,form,
            XmNorientation, XmHORIZONTAL,
            XmNtopAttachment,XmATTACH_WIDGET,
            XmNtopWidget, frame2,
            XmNtopOffset, 20,
            XmNbottomAttachment,XmATTACH_NONE,
            XmNleftAttachment, XmATTACH_FORM,
            XmNrightAttachment,XmATTACH_FORM,
            XmNbackground, colors[0xff],
            NULL);

        button_ok = XtVaCreateManagedWidget(langcode("UNIOP00001"),xmPushButtonGadgetClass, form,
            XmNnavigationType, XmTAB_GROUP,
            XmNtraversalOn, TRUE,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, sep,
            XmNtopOffset, 10,
            XmNbottomAttachment, XmATTACH_FORM,
            XmNbottomOffset, 5,
            XmNleftAttachment, XmATTACH_POSITION,
            XmNleftPosition, 1,
            XmNrightAttachment, XmATTACH_POSITION,
            XmNrightPosition, 2,
            XmNbackground, colors[0xff],
            NULL);

        button_cancel = XtVaCreateManagedWidget(langcode("UNIOP00002"),xmPushButtonGadgetClass, form,
            XmNnavigationType, XmTAB_GROUP,
            XmNtraversalOn, TRUE,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, sep,
            XmNtopOffset, 10,
            XmNbottomAttachment, XmATTACH_FORM,
            XmNbottomOffset, 5,
            XmNleftAttachment, XmATTACH_POSITION,
            XmNleftPosition, 3,
            XmNrightAttachment, XmATTACH_POSITION,
            XmNrightPosition, 4,
            XmNbackground, colors[0xff],
            NULL);

        XtAddCallback(button_ok, XmNactivateCallback, Config_GPS_change_data, config_GPS_dialog);
        XtAddCallback(button_cancel, XmNactivateCallback, Config_GPS_destroy_shell, config_GPS_dialog);

        pos_dialog(config_GPS_dialog);

        delw = XmInternAtom(XtDisplay(config_GPS_dialog),"WM_DELETE_WINDOW", FALSE);
        XmAddWMProtocolCallback(config_GPS_dialog, delw, Config_GPS_destroy_shell, (XtPointer)config_GPS_dialog);

        if (config_type==0) {
            /* first time port */
            XmTextFieldSetString(GPS_device_name_data,GPS_PORT);
            XmTextFieldSetString(GPS_comment,"");
            XmToggleButtonSetState(GPS_active_on_startup,TRUE,FALSE);
            XmToggleButtonSetState(GPS_set_time, FALSE, FALSE);
            XmToggleButtonSetState(speed_4800,TRUE,FALSE);
            device_speed=4;
            XmToggleButtonSetState(style_8n1,TRUE,FALSE);
            device_style=0;
        } else {
            /* reconfig */

begin_critical_section(&devices_lock, "interface_gui.c:Config_GPS" );

            XmTextFieldSetString(GPS_device_name_data,devices[GPS_port].device_name);
            XmTextFieldSetString(GPS_comment,devices[GPS_port].comment);
 
            if (devices[GPS_port].connect_on_startup)
                XmToggleButtonSetState(GPS_active_on_startup,TRUE,FALSE);
            else
                XmToggleButtonSetState(GPS_active_on_startup,FALSE,FALSE);

            if (devices[GPS_port].set_time)
                XmToggleButtonSetState(GPS_set_time,TRUE,FALSE);
            else
                XmToggleButtonSetState(GPS_set_time,FALSE,FALSE);

            switch (devices[GPS_port].sp) {
                case(B300):
                    XmToggleButtonSetState(speed_300,TRUE,FALSE);
                    device_speed=1;
                    break;

                case(B1200):
                    XmToggleButtonSetState(speed_1200,TRUE,FALSE);
                    device_speed=2;
                    break;

                case(B2400):
                    XmToggleButtonSetState(speed_2400,TRUE,FALSE);
                    device_speed=3;
                    break;

                case(B4800):
                    XmToggleButtonSetState(speed_4800,TRUE,FALSE);
                    device_speed=4;
                    break;

                case(B9600):
                    XmToggleButtonSetState(speed_9600,TRUE,FALSE);
                    device_speed=5;
                    break;

                case(B19200):
                    XmToggleButtonSetState(speed_19200,TRUE,FALSE);
                    device_speed=6;
                    break;

                case(B38400):
                    XmToggleButtonSetState(speed_38400,TRUE,FALSE);
                    device_speed=7;
                    break;

                case(B57600):
                    XmToggleButtonSetState(speed_57600,TRUE,FALSE);
                    device_speed=8;
                    break;

                case(B115200):
                    XmToggleButtonSetState(speed_115200,TRUE,FALSE);
                    device_speed=9;
                    break;

#ifdef B230400
                case(B230400):
                    XmToggleButtonSetState(speed_230400,TRUE,FALSE);
                    device_speed=10;
                    break;
#endif  // B230400

                default:
                    XmToggleButtonSetState(speed_4800,TRUE,FALSE);
                    device_speed=4;
                    break;
            }
            switch (devices[GPS_port].style) {
                case(0):
                    XmToggleButtonSetState(style_8n1,TRUE,FALSE);
                    device_style=0;
                    break;

                case(1):
                    XmToggleButtonSetState(style_7e1,TRUE,FALSE);
                    device_style=1;
                    break;

                case(2):
                    XmToggleButtonSetState(style_7o1,TRUE,FALSE);
                    device_style=2;
                    break;

                default:
                    XmToggleButtonSetState(style_8n1,TRUE,FALSE);
                    device_style=0;
                    break;
            }

end_critical_section(&devices_lock, "interface_gui.c:Config_GPS" );

        }
        XtManageChild(form);
        XtManageChild(speed_box);
        XtManageChild(style_box);
        XtManageChild(pane);

        XtPopup(config_GPS_dialog,XtGrabNone);
        fix_dialog_size(config_GPS_dialog);
    } else
        (void)XRaiseWindow(XtDisplay(config_GPS_dialog), XtWindow(config_GPS_dialog));
}





/*****************************************************/
/* Configure Serial WX GUI                          */
/*****************************************************/

/**** WX CONFIGURE ******/
int WX_port;
int WX_rain_gauge_type;
Widget config_WX_dialog = (Widget)NULL;
Widget WX_transmit_data;
Widget WX_device_name_data;
Widget WX_comment;
Widget WX_active_on_startup;
Widget WX_tenths, WX_hundredths, WX_millimeters;
 




void Config_WX_destroy_shell( /*@unused@*/ Widget widget, XtPointer clientData,  /*@unused@*/ XtPointer callData) {
    Widget shell = (Widget) clientData;
    XtPopdown(shell);
    XtDestroyWidget(shell);
    config_WX_dialog = (Widget)NULL;
    if (choose_interface_dialog != NULL)
        Choose_interface_destroy_shell(choose_interface_dialog,choose_interface_dialog,NULL);
 
    choose_interface_dialog = (Widget)NULL;
}





void Config_WX_change_data(Widget widget, XtPointer clientData, XtPointer callData) {
    int was_up;
    char *temp_ptr;


    busy_cursor(appshell);
    was_up=0;
    if (get_device_status(WX_port) == DEVICE_IN_USE) {
        /* if active shutdown before changes are made */
        /*fprintf(stderr,"Device is up, shutting down\n");*/
        (void)del_device(WX_port);
        was_up=1;
        usleep(1000000); // Wait for one second
    }

begin_critical_section(&devices_lock, "interface_gui.c:Config_WX_change_data" );

    temp_ptr = XmTextFieldGetString(WX_device_name_data);
    xastir_snprintf(devices[WX_port].device_name,
        sizeof(devices[WX_port].device_name),
        "%s",
        temp_ptr);
    XtFree(temp_ptr);

    (void)remove_trailing_spaces(devices[WX_port].device_name);

    temp_ptr = XmTextFieldGetString(WX_comment);
    xastir_snprintf(devices[WX_port].comment,
        sizeof(devices[WX_port].comment),
        "%s",
        temp_ptr);
    XtFree(temp_ptr);

    (void)remove_trailing_spaces(devices[WX_port].comment);

    if(XmToggleButtonGetState(WX_active_on_startup))
        devices[WX_port].connect_on_startup=1;
    else
        devices[WX_port].connect_on_startup=0;

    set_port_speed(WX_port);
    devices[WX_port].style=device_style;

    xastir_snprintf(devices[WX_port].device_host_pswd,
            sizeof( devices[WX_port].device_host_pswd), "%d", device_data_type);

    /* reopen */
    if ( was_up) {
        (void)add_device(WX_port,
            DEVICE_SERIAL_WX,
            devices[WX_port].device_name,
            devices[WX_port].device_host_pswd,
            -1,
            devices[WX_port].sp,
            devices[WX_port].style,
            0,
            "");
    }

    /* delete list */
//    modify_device_list(4,0);


    /* add device type */
    devices[WX_port].device_type=DEVICE_SERIAL_WX;

    /* rebuild list */
//    modify_device_list(3,0);


end_critical_section(&devices_lock, "interface_gui.c:Config_WX_change_data" );

    // Rebuild the interface control list
    update_interface_list();

    Config_WX_destroy_shell(widget,clientData,callData);
}





void Config_WX( /*@unused@*/ Widget w, int config_type, int port) {
    static Widget  pane, form, button_ok, button_cancel,
                frame, frame2, frame3, frame4, WX_none,
                device, comment, speed, speed_box,
                speed_300, speed_1200, speed_2400, speed_4800, speed_9600,
                speed_19200, speed_38400, speed_57600, speed_115200, speed_230400,
                style, style_box,
                style_8n1, style_7e1, style_7o1,
                data_type, data_box,
                data_auto, data_bin, data_ascii,
                gauge_type, gauge_box,
                sep;
    Atom delw;
    Arg al[50];                    /* Arg List */
    register unsigned int ac = 0;           /* Arg Count */

    if(!config_WX_dialog) {
        WX_port=port;
        config_WX_dialog = XtVaCreatePopupShell(langcode("WPUPCFWX01"),
            xmDialogShellWidgetClass, appshell,
            XmNdeleteResponse, XmDESTROY,
            XmNdefaultPosition, FALSE,
            NULL);

        pane = XtVaCreateWidget("Config_WX pane",xmPanedWindowWidgetClass, config_WX_dialog,
                          XmNbackground, colors[0xff],
                          NULL);

        form =  XtVaCreateWidget("Config_WX form",xmFormWidgetClass, pane,
                            XmNfractionBase, 5,
                            XmNbackground, colors[0xff],
                            XmNautoUnmanage, FALSE,
                            XmNshadowThickness, 1,
                            NULL);

        device = XtVaCreateManagedWidget(langcode("WPUPCFWX02"),xmLabelWidgetClass, form,
                                      XmNtopAttachment, XmATTACH_FORM,
                                      XmNtopOffset, 10,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset, 10,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        WX_device_name_data = XtVaCreateManagedWidget("Config_WX device_data", xmTextFieldWidgetClass, form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNeditable,   TRUE,
                                      XmNcursorPositionVisible, TRUE,
                                      XmNsensitive, TRUE,
                                      XmNshadowThickness,    1,
                                      XmNcolumns, 15,
                                      XmNwidth, ((15*7)+2),
                                      XmNmaxLength, 40,
                                      XmNbackground, colors[0x0f],
                                      XmNtopAttachment,XmATTACH_FORM,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, device,
                                      XmNleftOffset, 10,
                                      XmNrightAttachment,XmATTACH_FORM,
                                      XmNrightOffset, 5,
                                      NULL);

        comment = XtVaCreateManagedWidget(langcode("WPUPCFS017"),xmLabelWidgetClass, form,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, device,
                                      XmNtopOffset, 15,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset, 10,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        WX_comment = XtVaCreateManagedWidget("Config_WX comment", xmTextFieldWidgetClass, form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNeditable,   TRUE,
                                      XmNcursorPositionVisible, TRUE,
                                      XmNsensitive, TRUE,
                                      XmNshadowThickness,    1,
                                      XmNcolumns, 15,
                                      XmNwidth, ((15*7)+2),
                                      XmNmaxLength, 49,
                                      XmNbackground, colors[0x0f],
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, device,
                                      XmNtopOffset, 10,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, comment,
                                      XmNleftOffset, 10,
                                      XmNrightAttachment,XmATTACH_FORM,
                                      XmNrightOffset, 5,
                                      NULL);

        WX_active_on_startup = XtVaCreateManagedWidget(langcode("UNIOP00011"),xmToggleButtonWidgetClass,form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, comment,
                                      XmNtopOffset, 10,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset ,10,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        frame = XtVaCreateManagedWidget("Config_WX frame", xmFrameWidgetClass, form,
                                    XmNtopAttachment,XmATTACH_WIDGET,
                                    XmNtopOffset,10,
                                    XmNtopWidget, WX_active_on_startup,
                                    XmNbottomAttachment,XmATTACH_NONE,
                                    XmNleftAttachment, XmATTACH_FORM,
                                    XmNleftOffset, 10,
                                    XmNrightAttachment,XmATTACH_FORM,
                                    XmNrightOffset, 10,
                                    XmNbackground, colors[0xff],
                                    NULL);

        speed = XtVaCreateManagedWidget(langcode("WPUPCFT004"),xmLabelWidgetClass, frame,
                                    XmNchildType, XmFRAME_TITLE_CHILD,
                                    XmNbackground, colors[0xff],
                                    NULL);

        /*set args for color */
        ac=0;
        XtSetArg(al[ac], XmNbackground, colors[0xff]); ac++;


        speed_box = XmCreateRadioBox(frame,"Config_WX Speed_box",al,ac);

        XtVaSetValues(speed_box,
                  XmNnumColumns,3,
                  NULL);

        speed_300 = XtVaCreateManagedWidget(langcode("WPUPCFT005"),xmToggleButtonGadgetClass,
                                        speed_box,
                                        XmNbackground, colors[0xff],
                                        NULL);
        XtAddCallback(speed_300,XmNvalueChangedCallback,speed_toggle,"1");

        speed_1200 = XtVaCreateManagedWidget(langcode("WPUPCFT006"),xmToggleButtonGadgetClass,
                                        speed_box,
                                        XmNbackground, colors[0xff],
                                        NULL);
        XtAddCallback(speed_1200,XmNvalueChangedCallback,speed_toggle,"2");

        speed_2400 = XtVaCreateManagedWidget(langcode("WPUPCFT007"),xmToggleButtonGadgetClass,
                                        speed_box,
                                        XmNbackground, colors[0xff],
                                        NULL);
        XtAddCallback(speed_2400,XmNvalueChangedCallback,speed_toggle,"3");

        speed_4800 = XtVaCreateManagedWidget(langcode("WPUPCFT008"),xmToggleButtonGadgetClass,
                                        speed_box,
                                        XmNbackground, colors[0xff],
                                        NULL);
        XtAddCallback(speed_4800,XmNvalueChangedCallback,speed_toggle,"4");

        speed_9600 = XtVaCreateManagedWidget(langcode("WPUPCFT009"),xmToggleButtonGadgetClass,
                                        speed_box,
                                        XmNbackground, colors[0xff],
                                        NULL);
        XtAddCallback(speed_9600,XmNvalueChangedCallback,speed_toggle,"5");

        speed_19200 = XtVaCreateManagedWidget(langcode("WPUPCFT010"),xmToggleButtonGadgetClass,
                                        speed_box,
                                        XmNbackground, colors[0xff],
                                        NULL);
        XtAddCallback(speed_19200,XmNvalueChangedCallback,speed_toggle,"6");

        speed_38400 = XtVaCreateManagedWidget(langcode("WPUPCFT019"),xmToggleButtonGadgetClass,
                                        speed_box,
                                        XmNbackground, colors[0xff],
                                        NULL);
        XtAddCallback(speed_38400,XmNvalueChangedCallback,speed_toggle,"7");

        speed_57600 = XtVaCreateManagedWidget(langcode("WPUPCFT020"),xmToggleButtonGadgetClass,
                                        speed_box,
                                        XmNbackground, colors[0xff],
                                        NULL);
        XtAddCallback(speed_57600,XmNvalueChangedCallback,speed_toggle,"8");

        speed_115200 = XtVaCreateManagedWidget(langcode("WPUPCFT021"),xmToggleButtonGadgetClass,
                                        speed_box,
                                        XmNbackground, colors[0xff],
                                        NULL);
        XtAddCallback(speed_115200,XmNvalueChangedCallback,speed_toggle,"9");

        speed_230400 = XtVaCreateManagedWidget(langcode("WPUPCFT022"),xmToggleButtonGadgetClass,
                                        speed_box,
                                        XmNbackground, colors[0xff],
                                        NULL);
        XtAddCallback(speed_230400,XmNvalueChangedCallback,speed_toggle,"10");

        frame2 = XtVaCreateManagedWidget("Config_WX frame2", xmFrameWidgetClass, form,
                                     XmNtopAttachment, XmATTACH_WIDGET,
                                     XmNtopWidget, frame,
                                     XmNtopOffset, 10,
                                     XmNbottomAttachment, XmATTACH_NONE,
                                     XmNleftAttachment, XmATTACH_FORM,
                                     XmNleftOffset, 10,
                                     XmNrightAttachment, XmATTACH_FORM,
                                     XmNrightOffset, 10,
                                     XmNbackground, colors[0xff],
                                     NULL);

        style = XtVaCreateManagedWidget(langcode("WPUPCFT015"),xmLabelWidgetClass, frame2,
                                    XmNchildType, XmFRAME_TITLE_CHILD,
                                    XmNbackground, colors[0xff],
                                    NULL);

        style_box = XmCreateRadioBox(frame2,"Config_WX Style box",al,ac);

        XtVaSetValues(style_box,
                  XmNorientation, XmHORIZONTAL,
                  NULL);

        style_8n1 = XtVaCreateManagedWidget(langcode("WPUPCFT016"),xmToggleButtonGadgetClass,
                                        style_box,
                                        XmNbackground, colors[0xff],
                                        NULL);
        XtAddCallback(style_8n1,XmNvalueChangedCallback,style_toggle,"0");

        style_7e1 = XtVaCreateManagedWidget(langcode("WPUPCFT017"),xmToggleButtonGadgetClass,
                                        style_box,
                                        XmNbackground, colors[0xff],
                                        NULL);
        XtAddCallback(style_7e1,XmNvalueChangedCallback,style_toggle,"1");

        style_7o1 = XtVaCreateManagedWidget(langcode("WPUPCFT018"),xmToggleButtonGadgetClass,
                                        style_box,
                                        XmNbackground, colors[0xff],
                                        NULL);
        XtAddCallback(style_7o1,XmNvalueChangedCallback,style_toggle,"2");

        frame3 = XtVaCreateManagedWidget("Config_WX frame3", xmFrameWidgetClass, form,
                                     XmNtopAttachment, XmATTACH_WIDGET,
                                     XmNtopWidget, frame2,
                                     XmNtopOffset, 10,
                                     XmNbottomAttachment, XmATTACH_NONE,
                                     XmNleftAttachment, XmATTACH_FORM,
                                     XmNleftOffset, 10,
                                     XmNrightAttachment, XmATTACH_FORM,
                                     XmNrightOffset, 10,
                                     XmNbackground, colors[0xff],
                                     NULL);

        data_type= XtVaCreateManagedWidget(langcode("WPUPCFT024"),xmLabelWidgetClass, frame3,
                                    XmNchildType, XmFRAME_TITLE_CHILD,
                                    XmNbackground, colors[0xff],
                                    NULL);

        data_box = XmCreateRadioBox(frame3,"Config_WX data box",al,ac);

        XtVaSetValues(data_box,
                  XmNorientation, XmHORIZONTAL,
                  NULL);

        data_auto = XtVaCreateManagedWidget(langcode("WPUPCFT025"),xmToggleButtonGadgetClass,
                                        data_box,
                                        XmNbackground, colors[0xff],
                                        NULL);
        XtAddCallback(data_auto,XmNvalueChangedCallback,data_toggle,"0");

        data_bin = XtVaCreateManagedWidget(langcode("WPUPCFT026"),xmToggleButtonGadgetClass,
                                        data_box,
                                        XmNbackground, colors[0xff],
                                        NULL);
        XtAddCallback(data_bin,XmNvalueChangedCallback,data_toggle,"1");

        data_ascii = XtVaCreateManagedWidget(langcode("WPUPCFT027"),xmToggleButtonGadgetClass,
                                        data_box,
                                        XmNbackground, colors[0xff],
                                        NULL);
        XtAddCallback(data_ascii,XmNvalueChangedCallback,data_toggle,"2");

        frame4 = XtVaCreateManagedWidget("Config_WX frame4", xmFrameWidgetClass, form,
                                        XmNtopAttachment, XmATTACH_WIDGET,
                                        XmNtopWidget, frame3,
                                        XmNtopOffset, 10,
                                        XmNbottomAttachment, XmATTACH_NONE,
                                        XmNleftAttachment, XmATTACH_FORM,
                                        XmNleftOffset, 10,
                                        XmNrightAttachment, XmATTACH_FORM,
                                        XmNrightOffset, 10,
                                        XmNbackground, colors[0xff],
                                        NULL);

        // Rain Gauge Type
        gauge_type= XtVaCreateManagedWidget(langcode("WPUPCFWX03"),xmLabelWidgetClass, frame4,
                                        XmNchildType, XmFRAME_TITLE_CHILD,
                                        XmNbackground, colors[0xff],
                                        NULL);

        gauge_box = XmCreateRadioBox(frame4,"Config_WX gauge box",al,ac);

        XtVaSetValues(gauge_box,
            XmNorientation, XmHORIZONTAL,
            NULL);

        WX_none = XtVaCreateManagedWidget(langcode("WPUPCFWX07"),xmToggleButtonGadgetClass,
                                      gauge_box,
                                      XmNbackground, colors[0xff],
                                      NULL);
        WX_tenths = XtVaCreateManagedWidget(langcode("WPUPCFWX04"),xmToggleButtonGadgetClass,
                                      gauge_box,
                                      XmNbackground, colors[0xff],
                                      NULL);
        WX_hundredths = XtVaCreateManagedWidget(langcode("WPUPCFWX05"),xmToggleButtonGadgetClass,
                                      gauge_box,
                                      XmNbackground, colors[0xff],
                                      NULL);
        WX_millimeters = XtVaCreateManagedWidget(langcode("WPUPCFWX06"),xmToggleButtonGadgetClass,
                                      gauge_box,
                                      XmNbackground, colors[0xff],
                                      NULL);

        XtAddCallback(WX_none,XmNvalueChangedCallback,rain_gauge_toggle,"0");
        XtAddCallback(WX_tenths,XmNvalueChangedCallback,rain_gauge_toggle,"1");
        XtAddCallback(WX_hundredths,XmNvalueChangedCallback,rain_gauge_toggle,"2");
        XtAddCallback(WX_millimeters,XmNvalueChangedCallback,rain_gauge_toggle,"3");

        sep = XtVaCreateManagedWidget("Config_WX sep", xmSeparatorGadgetClass,form,
                                      XmNorientation, XmHORIZONTAL,
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, frame4,
                                      XmNtopOffset, 20,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNrightAttachment,XmATTACH_FORM,
                                      XmNbackground, colors[0xff],
                                      NULL);

        button_ok = XtVaCreateManagedWidget(langcode("UNIOP00001"),xmPushButtonGadgetClass, form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, sep,
                                      XmNtopOffset, 10,
                                      XmNbottomAttachment, XmATTACH_FORM,
                                      XmNbottomOffset, 5,
                                      XmNleftAttachment, XmATTACH_POSITION,
                                      XmNleftPosition, 1,
                                      XmNrightAttachment, XmATTACH_POSITION,
                                      XmNrightPosition, 2,
                                      XmNbackground, colors[0xff],
                                      NULL);

        button_cancel = XtVaCreateManagedWidget(langcode("UNIOP00002"),xmPushButtonGadgetClass, form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, sep,
                                      XmNtopOffset, 10,
                                      XmNbottomAttachment, XmATTACH_FORM,
                                      XmNbottomOffset, 5,
                                      XmNleftAttachment, XmATTACH_POSITION,
                                      XmNleftPosition, 3,
                                      XmNrightAttachment, XmATTACH_POSITION,
                                      XmNrightPosition, 4,
                                      XmNbackground, colors[0xff],
                                      NULL);

        XtAddCallback(button_ok, XmNactivateCallback, Config_WX_change_data, config_WX_dialog);
        XtAddCallback(button_cancel, XmNactivateCallback, Config_WX_destroy_shell, config_WX_dialog);

        pos_dialog(config_WX_dialog);

        delw = XmInternAtom(XtDisplay(config_WX_dialog),"WM_DELETE_WINDOW", FALSE);
        XmAddWMProtocolCallback(config_WX_dialog, delw, Config_WX_destroy_shell, (XtPointer)config_WX_dialog);

begin_critical_section(&devices_lock, "interface_gui.c:Config_WX" );

        if (config_type==0) {
            /* first time port */
            XmTextFieldSetString(WX_device_name_data,GPS_PORT);
            XmTextFieldSetString(WX_comment,"");
            XmToggleButtonSetState(WX_active_on_startup,TRUE,FALSE);
            XmToggleButtonSetState(speed_2400,TRUE,FALSE);
            device_speed=3;
            XmToggleButtonSetState(style_8n1,TRUE,FALSE);
            device_style=0;
            device_data_type=0;
            XmToggleButtonSetState(data_auto,TRUE,FALSE);
        } else {
            /* reconfig */
            XmTextFieldSetString(WX_device_name_data,devices[WX_port].device_name);
            XmTextFieldSetString(WX_comment,devices[WX_port].comment);
 
            if (devices[WX_port].connect_on_startup)
                XmToggleButtonSetState(WX_active_on_startup,TRUE,FALSE);
            else
                XmToggleButtonSetState(WX_active_on_startup,FALSE,FALSE);

            switch (devices[WX_port].sp) {
                case(B300):
                    XmToggleButtonSetState(speed_300,TRUE,FALSE);
                    device_speed=1;
                    break;

                case(B1200):
                    XmToggleButtonSetState(speed_1200,TRUE,FALSE);
                    device_speed=2;
                    break;

                case(B2400):
                    XmToggleButtonSetState(speed_2400,TRUE,FALSE);
                    device_speed=3;
                    break;

                case(B4800):
                    XmToggleButtonSetState(speed_4800,TRUE,FALSE);
                    device_speed=4;
                    break;

                case(B9600):
                    XmToggleButtonSetState(speed_9600,TRUE,FALSE);
                    device_speed=5;
                    break;

                case(B19200):
                    XmToggleButtonSetState(speed_19200,TRUE,FALSE);
                    device_speed=6;
                    break;

                case(B38400):
                    XmToggleButtonSetState(speed_38400,TRUE,FALSE);
                    device_speed=7;
                    break;

                case(B57600):
                    XmToggleButtonSetState(speed_57600,TRUE,FALSE);
                    device_speed=8;
                    break;

                case(B115200):
                    XmToggleButtonSetState(speed_115200,TRUE,FALSE);
                    device_speed=9;
                    break;

#ifdef B230400
                case(B230400):
                    XmToggleButtonSetState(speed_230400,TRUE,FALSE);
                    device_speed=10;
                    break;
#endif  // B230400

                default:
                    XmToggleButtonSetState(speed_4800,TRUE,FALSE);
                    device_speed=4;
                    break;
            }
            switch (devices[WX_port].style) {
                case(0):
                    XmToggleButtonSetState(style_8n1,TRUE,FALSE);
                    device_style=0;
                    break;

                case(1):
                    XmToggleButtonSetState(style_7e1,TRUE,FALSE);
                    device_style=1;
                    break;

                case(2):
                    XmToggleButtonSetState(style_7o1,TRUE,FALSE);
                    device_style=2;
                    break;

                default:
                    XmToggleButtonSetState(style_8n1,TRUE,FALSE);
                    device_style=0;
                    break;
            }
            switch (atoi(devices[WX_port].device_host_pswd)) {
                case(0):
                    XmToggleButtonSetState(data_auto,TRUE,FALSE);
                    device_data_type=0;
                    break;

                case(1):
                    XmToggleButtonSetState(data_bin,TRUE,FALSE);
                    device_data_type=1;
                    break;

                case(2):
                    XmToggleButtonSetState(data_ascii,TRUE,FALSE);
                    device_data_type=2;
                    break;

                default:
                    device_data_type=0;
                    break;
            }
        }

end_critical_section(&devices_lock, "interface_gui.c:Config_WX" );

        XmToggleButtonSetState(WX_none,FALSE,FALSE);
        XmToggleButtonSetState(WX_tenths,FALSE,FALSE);
        XmToggleButtonSetState(WX_hundredths,FALSE,FALSE);
        XmToggleButtonSetState(WX_millimeters,FALSE,FALSE);
        switch (WX_rain_gauge_type) {
            case(1):
                XmToggleButtonSetState(WX_tenths,TRUE,FALSE);
                break;
            case(2):
                XmToggleButtonSetState(WX_hundredths,TRUE,FALSE);
                break;
            case(3):
                XmToggleButtonSetState(WX_millimeters,TRUE,FALSE);
                break;
            default:
                XmToggleButtonSetState(WX_none,TRUE,FALSE);
                break;
        }

        XtManageChild(form);
        XtManageChild(speed_box);
        XtManageChild(style_box);
        XtManageChild(data_box);
        XtManageChild(gauge_box);
        XtManageChild(pane);

        XtPopup(config_WX_dialog,XtGrabNone);
        fix_dialog_size(config_WX_dialog);
    } else
        (void)XRaiseWindow(XtDisplay(config_WX_dialog), XtWindow(config_WX_dialog));

}





/**** net WX CONFIGURE ******/
int NWX_port;
Widget config_NWX_dialog = (Widget)NULL;
Widget NWX_host_name_data;
Widget NWX_host_port_data;
Widget NWX_comment;
Widget NWX_active_on_startup;
Widget NWX_host_reconnect_data;





void Config_NWX_destroy_shell( /*@unused@*/ Widget widget, XtPointer clientData,  /*@unused@*/ XtPointer callData) {
    Widget shell = (Widget) clientData;
    XtPopdown(shell);
    XtDestroyWidget(shell);
    config_NWX_dialog = (Widget)NULL;

    if (choose_interface_dialog != NULL)
        Choose_interface_destroy_shell(choose_interface_dialog,choose_interface_dialog,NULL);

    choose_interface_dialog = (Widget)NULL;
}





void Config_NWX_change_data(Widget widget, XtPointer clientData, XtPointer callData) {
    int was_up;
    char *temp_ptr;


    busy_cursor(appshell);

    was_up=0;
    if (get_device_status(NWX_port) == DEVICE_IN_USE) {
        /* if active shutdown before changes are made */
        /*fprintf(stderr,"Device is up, shutting down\n");*/
        (void)del_device(NWX_port);
        was_up=1;
        usleep(1000000); // Wait for one second
    }

begin_critical_section(&devices_lock, "interface_gui.c:Config_NWX_change_data" );

    temp_ptr = XmTextFieldGetString(NWX_host_name_data);
    xastir_snprintf(devices[NWX_port].device_host_name,
        sizeof(devices[NWX_port].device_host_name),
        "%s",
        temp_ptr);
    XtFree(temp_ptr);

    (void)remove_trailing_spaces(devices[NWX_port].device_host_name);

    temp_ptr = XmTextFieldGetString(NWX_host_port_data);
    devices[NWX_port].sp=atoi(temp_ptr);
    XtFree(temp_ptr);

    temp_ptr = XmTextFieldGetString(NWX_comment);
    xastir_snprintf(devices[NWX_port].comment,
        sizeof(devices[NWX_port].comment),
        "%s",
        temp_ptr);
    XtFree(temp_ptr);

    (void)remove_trailing_spaces(devices[NWX_port].comment);

    if (XmToggleButtonGetState(NWX_active_on_startup))
        devices[NWX_port].connect_on_startup=1;
    else
        devices[NWX_port].connect_on_startup=0;

    if(XmToggleButtonGetState(NWX_host_reconnect_data))
        devices[NWX_port].reconnect=1;
    else
        devices[NWX_port].reconnect=0;

    xastir_snprintf(devices[NWX_port].device_host_pswd,
            sizeof(devices[NWX_port].device_host_pswd), "%d", device_data_type);

    /* reopen if was up*/
    if ( was_up) {
        (void)add_device(NWX_port,
            DEVICE_NET_WX,
            devices[NWX_port].device_host_name,
            devices[NWX_port].device_host_pswd,
            devices[NWX_port].sp,
            0,
            0,
            devices[NWX_port].reconnect,
            "");
    }


    /* delete list */
//    modify_device_list(4,0);


    /* add device type */
    devices[NWX_port].device_type=DEVICE_NET_WX;

    /* rebuild list */
//    modify_device_list(3,0);


end_critical_section(&devices_lock, "interface_gui.c:Config_NWX_change_data" );

    // Rebuild the interface control list
    update_interface_list();

    Config_NWX_destroy_shell(widget,clientData,callData);
}





void Config_NWX( /*@unused@*/ Widget w, int config_type, int port) {
    static Widget  pane, form, frame3, frame4, WX_none,
                button_ok, button_cancel,
                hostn, portn, comment,
                data_type, data_box,
                data_auto, data_bin, data_ascii,
                gauge_type, gauge_box,
                sep;
    char temp[20];
    Arg al[50];                    /* Arg List */
    register unsigned int ac = 0;           /* Arg Count */
    Atom delw;

    if(!config_NWX_dialog) {
        NWX_port=port;
        config_NWX_dialog = XtVaCreatePopupShell(langcode("WPUPCFG021"),
            xmDialogShellWidgetClass, appshell,
            XmNdeleteResponse, XmDESTROY,
            XmNdefaultPosition, FALSE,
            NULL);

        pane = XtVaCreateWidget("Config_NWX pane",xmPanedWindowWidgetClass, config_NWX_dialog,
                          XmNbackground, colors[0xff],
                          NULL);

        form =  XtVaCreateWidget("Config_NWX form",xmFormWidgetClass, pane,
                            XmNfractionBase, 5,
                            XmNbackground, colors[0xff],
                            XmNautoUnmanage, FALSE,
                            XmNshadowThickness, 1,
                            NULL);

        hostn = XtVaCreateManagedWidget(langcode("WPUPCFG022"),xmLabelWidgetClass, form,
                                      XmNtopAttachment, XmATTACH_FORM,
                                      XmNtopOffset, 10,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset, 10,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        NWX_host_name_data = XtVaCreateManagedWidget("Config_NWX host_data", xmTextFieldWidgetClass, form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNeditable,   TRUE,
                                      XmNcursorPositionVisible, TRUE,
                                      XmNsensitive, TRUE,
                                      XmNshadowThickness,    1,
                                      XmNcolumns, 25,
                                      XmNwidth, ((25*7)+2),
                                      XmNmaxLength, 40,
                                      XmNbackground, colors[0x0f],
                                      XmNtopAttachment,XmATTACH_FORM,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, hostn,
                                      XmNleftOffset, 10,
                                      XmNrightAttachment,XmATTACH_FORM,
                                      XmNrightOffset, 5,
                                      NULL);

        portn = XtVaCreateManagedWidget(langcode("WPUPCFG023"),xmLabelWidgetClass, form,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, hostn,
                                      XmNtopOffset, 12,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset, 10,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        NWX_host_port_data = XtVaCreateManagedWidget("Config_NWX port_data", xmTextFieldWidgetClass, form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNeditable,   TRUE,
                                      XmNcursorPositionVisible, TRUE,
                                      XmNsensitive, TRUE,
                                      XmNshadowThickness,    1,
                                      XmNcolumns, 25,
                                      XmNwidth, ((25*7)+2),
                                      XmNmaxLength, 40,
                                      XmNbackground, colors[0x0f],
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, hostn,
                                      XmNtopOffset, 8,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, portn,
                                      XmNleftOffset, 10,
                                      XmNrightAttachment,XmATTACH_FORM,
                                      XmNrightOffset, 5,
                                      NULL);

        comment = XtVaCreateManagedWidget(langcode("WPUPCFS017"),xmLabelWidgetClass, form,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, portn,
                                      XmNtopOffset, 12,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset, 10,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        NWX_comment = XtVaCreateManagedWidget("Config_NWX comment", xmTextFieldWidgetClass, form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNeditable,   TRUE,
                                      XmNcursorPositionVisible, TRUE,
                                      XmNsensitive, TRUE,
                                      XmNshadowThickness,    1,
                                      XmNcolumns, 25,
                                      XmNwidth, ((25*7)+2),
                                      XmNmaxLength, 49,
                                      XmNbackground, colors[0x0f],
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, portn,
                                      XmNtopOffset, 8,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, comment,
                                      XmNleftOffset, 10,
                                      XmNrightAttachment,XmATTACH_FORM,
                                      XmNrightOffset, 5,
                                      NULL);

        NWX_active_on_startup  = XtVaCreateManagedWidget(langcode("UNIOP00011"),xmToggleButtonWidgetClass,form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, comment,
                                      XmNtopOffset, 15,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset ,10,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        NWX_host_reconnect_data  = XtVaCreateManagedWidget(langcode("WPUPCFG020"),xmToggleButtonWidgetClass,form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, NWX_active_on_startup,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset ,10,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        /*set args for color */
        ac=0;
        XtSetArg(al[ac], XmNbackground, colors[0xff]); ac++;


        frame3 = XtVaCreateManagedWidget("Config_NWX frame3", xmFrameWidgetClass, form,
                                     XmNtopAttachment, XmATTACH_WIDGET,
                                     XmNtopWidget, NWX_host_reconnect_data,
                                     XmNtopOffset, 10,
                                     XmNbottomAttachment, XmATTACH_NONE,
                                     XmNleftAttachment, XmATTACH_FORM,
                                     XmNleftOffset, 10,
                                     XmNrightAttachment, XmATTACH_FORM,
                                     XmNrightOffset, 10,
                                     XmNbackground, colors[0xff],
                                     NULL);

        data_type= XtVaCreateManagedWidget(langcode("WPUPCFT024"),xmLabelWidgetClass, frame3,
                                    XmNchildType, XmFRAME_TITLE_CHILD,
                                    XmNbackground, colors[0xff],
                                    NULL);

        data_box = XmCreateRadioBox(frame3,"Config_NWX data box",al,ac);

        XtVaSetValues(data_box,
                  XmNorientation, XmHORIZONTAL,
                  NULL);

        data_auto = XtVaCreateManagedWidget(langcode("WPUPCFT025"),xmToggleButtonGadgetClass,
                                        data_box,
                                        XmNbackground, colors[0xff],
                                        NULL);
        XtAddCallback(data_auto,XmNvalueChangedCallback,data_toggle,"0");

        data_bin = XtVaCreateManagedWidget(langcode("WPUPCFT026"),xmToggleButtonGadgetClass,
                                        data_box,
                                        XmNbackground, colors[0xff],
                                        NULL);
        XtAddCallback(data_bin,XmNvalueChangedCallback,data_toggle,"1");

        data_ascii = XtVaCreateManagedWidget(langcode("WPUPCFT027"),xmToggleButtonGadgetClass,
                                        data_box,
                                        XmNbackground, colors[0xff],
                                        NULL);
        XtAddCallback(data_ascii,XmNvalueChangedCallback,data_toggle,"2");

        frame4 = XtVaCreateManagedWidget("Config_WX frame4", xmFrameWidgetClass, form,
                                        XmNtopAttachment, XmATTACH_WIDGET,
                                        XmNtopWidget, frame3,
                                        XmNtopOffset, 10,
                                        XmNbottomAttachment, XmATTACH_NONE,
                                        XmNleftAttachment, XmATTACH_FORM,
                                        XmNleftOffset, 10,
                                        XmNrightAttachment, XmATTACH_FORM,
                                        XmNrightOffset, 10,
                                        XmNbackground, colors[0xff],
                                        NULL);

        // Rain Gauge Type
        gauge_type= XtVaCreateManagedWidget(langcode("WPUPCFWX03"),xmLabelWidgetClass, frame4,
                                        XmNchildType, XmFRAME_TITLE_CHILD,
                                        XmNbackground, colors[0xff],
                                        NULL);

        gauge_box = XmCreateRadioBox(frame4,"Config_WX gauge box",al,ac);

        XtVaSetValues(gauge_box,
            XmNorientation, XmHORIZONTAL,
            NULL);

        WX_none = XtVaCreateManagedWidget(langcode("WPUPCFWX07"),xmToggleButtonGadgetClass,
                                      gauge_box,
                                      XmNbackground, colors[0xff],
                                      NULL);
        WX_tenths = XtVaCreateManagedWidget(langcode("WPUPCFWX04"),xmToggleButtonGadgetClass,
                                      gauge_box,
                                      XmNbackground, colors[0xff],
                                      NULL);
        WX_hundredths = XtVaCreateManagedWidget(langcode("WPUPCFWX05"),xmToggleButtonGadgetClass,
                                      gauge_box,
                                      XmNbackground, colors[0xff],
                                      NULL);
        WX_millimeters = XtVaCreateManagedWidget(langcode("WPUPCFWX06"),xmToggleButtonGadgetClass,
                                      gauge_box,
                                      XmNbackground, colors[0xff],
                                      NULL);

        XtAddCallback(WX_none,XmNvalueChangedCallback,rain_gauge_toggle,"0");
        XtAddCallback(WX_tenths,XmNvalueChangedCallback,rain_gauge_toggle,"1");
        XtAddCallback(WX_hundredths,XmNvalueChangedCallback,rain_gauge_toggle,"2");
        XtAddCallback(WX_millimeters,XmNvalueChangedCallback,rain_gauge_toggle,"3");


        sep = XtVaCreateManagedWidget("Config_NWX sep", xmSeparatorGadgetClass,form,
                                      XmNorientation, XmHORIZONTAL,
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, frame4,
                                      XmNtopOffset, 20,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNrightAttachment,XmATTACH_FORM,
                                      XmNbackground, colors[0xff],
                                      NULL);

        button_ok = XtVaCreateManagedWidget(langcode("UNIOP00001"),xmPushButtonGadgetClass, form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, sep,
                                      XmNtopOffset, 10,
                                      XmNbottomAttachment, XmATTACH_FORM,
                                      XmNbottomOffset, 5,
                                      XmNleftAttachment, XmATTACH_POSITION,
                                      XmNleftPosition, 1,
                                      XmNrightAttachment, XmATTACH_POSITION,
                                      XmNrightPosition, 2,
                                      XmNbackground, colors[0xff],
                                      NULL);

        button_cancel = XtVaCreateManagedWidget(langcode("UNIOP00002"),xmPushButtonGadgetClass, form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, sep,
                                      XmNtopOffset, 10,
                                      XmNbottomAttachment, XmATTACH_FORM,
                                      XmNbottomOffset, 5,
                                      XmNleftAttachment, XmATTACH_POSITION,
                                      XmNleftPosition, 3,
                                      XmNrightAttachment, XmATTACH_POSITION,
                                      XmNrightPosition, 4,
                                      XmNbackground, colors[0xff],
                                      NULL);

        XtAddCallback(button_ok, XmNactivateCallback, Config_NWX_change_data, config_NWX_dialog);
        XtAddCallback(button_cancel, XmNactivateCallback, Config_NWX_destroy_shell, config_NWX_dialog);

        pos_dialog(config_NWX_dialog);

        delw = XmInternAtom(XtDisplay(config_NWX_dialog),"WM_DELETE_WINDOW", FALSE);
        XmAddWMProtocolCallback(config_NWX_dialog, delw, Config_NWX_destroy_shell, (XtPointer)config_NWX_dialog);

begin_critical_section(&devices_lock, "interface_gui.c:Config_NWX" );

        if (config_type==0) {
            /* first time port */
            XmTextFieldSetString(NWX_host_name_data,"localhost");
            XmTextFieldSetString(NWX_host_port_data,"1234");
            XmTextFieldSetString(NWX_comment,"");
            XmToggleButtonSetState(NWX_active_on_startup,TRUE,FALSE);
            XmToggleButtonSetState(NWX_host_reconnect_data,TRUE,FALSE);
            device_data_type=0;
            XmToggleButtonSetState(data_auto,TRUE,FALSE);
        } else {
            /* reconfig */

            XmTextFieldSetString(NWX_host_name_data,devices[NWX_port].device_host_name);
            xastir_snprintf(temp, sizeof(temp), "%d", devices[NWX_port].sp); /* port number */
            XmTextFieldSetString(NWX_host_port_data,temp);

            XmTextFieldSetString(NWX_comment,devices[NWX_port].comment);
 
            if (devices[NWX_port].connect_on_startup)
                XmToggleButtonSetState(NWX_active_on_startup,TRUE,FALSE);
            else
                XmToggleButtonSetState(NWX_active_on_startup,FALSE,FALSE);

            if (devices[NWX_port].reconnect)
                XmToggleButtonSetState(NWX_host_reconnect_data,TRUE,FALSE);
            else
                XmToggleButtonSetState(NWX_host_reconnect_data,FALSE,FALSE);

            switch (atoi(devices[NWX_port].device_host_pswd)) {
                case(0):
                    XmToggleButtonSetState(data_auto,TRUE,FALSE);
                    device_data_type=0;
                    break;

                case(1):
                    XmToggleButtonSetState(data_bin,TRUE,FALSE);
                    device_data_type=1;
                    break;

                case(2):
                    XmToggleButtonSetState(data_ascii,TRUE,FALSE);
                    device_data_type=2;
                    break;

                default:
                    device_data_type=0;
                    break;
            }
        }

        XmToggleButtonSetState(WX_none,FALSE,FALSE);
        XmToggleButtonSetState(WX_tenths,FALSE,FALSE);
        XmToggleButtonSetState(WX_hundredths,FALSE,FALSE);
        XmToggleButtonSetState(WX_millimeters,FALSE,FALSE);
        switch (WX_rain_gauge_type) {
            case(1):
                XmToggleButtonSetState(WX_tenths,TRUE,FALSE);
                break;
            case(2):
                XmToggleButtonSetState(WX_hundredths,TRUE,FALSE);
                break;
            case(3):
                XmToggleButtonSetState(WX_millimeters,TRUE,FALSE);
                break;
            default:
                XmToggleButtonSetState(WX_none,TRUE,FALSE);
                break;
        }

end_critical_section(&devices_lock, "interface_gui.c:Config_NWX" );

        XtManageChild(form);
        XtManageChild(data_box);
        XtManageChild(frame3);
        XtManageChild(gauge_box);
        XtManageChild(pane);

        XtPopup(config_NWX_dialog,XtGrabNone);
        fix_dialog_size(config_NWX_dialog);
    } else {
        (void)XRaiseWindow(XtDisplay(config_NWX_dialog), XtWindow(config_NWX_dialog));
    }
}





/*****************************************************/
/* Configure net GPS GUI                             */
/*****************************************************/

/**** net GPS CONFIGURE ******/
int NGPS_port;
Widget config_NGPS_dialog = (Widget)NULL;
Widget NGPS_host_name_data;
Widget NGPS_host_port_data;
Widget NGPS_comment;
Widget NGPS_active_on_startup;
Widget NGPS_host_reconnect_data;
Widget NGPS_set_time;






void Config_NGPS_destroy_shell( /*@unused@*/ Widget widget, XtPointer clientData,  /*@unused@*/ XtPointer callData) {
    Widget shell = (Widget) clientData;
    XtPopdown(shell);
    XtDestroyWidget(shell);
    config_NGPS_dialog = (Widget)NULL;
    if (choose_interface_dialog != NULL)
        Choose_interface_destroy_shell(choose_interface_dialog,choose_interface_dialog,NULL);

    choose_interface_dialog = (Widget)NULL;
}





void Config_NGPS_change_data(Widget widget, XtPointer clientData, XtPointer callData) {
    int was_up;
    char *temp_ptr;


    busy_cursor(appshell);
    was_up=0;
    if (get_device_status(NGPS_port) == DEVICE_IN_USE) {
        /* if active shutdown before changes are made */
        /*fprintf(stderr,"Device is up, shutting down\n");*/
        (void)del_device(NGPS_port);
        was_up=1;
        usleep(1000000); // Wait for one second
    }

begin_critical_section(&devices_lock, "interface_gui.c:Config_NGPS_change_data" );

    temp_ptr = XmTextFieldGetString(NGPS_host_name_data);
    xastir_snprintf(devices[NGPS_port].device_host_name,
        sizeof(devices[NGPS_port].device_host_name),
        "%s",
        temp_ptr);
    XtFree(temp_ptr);

    (void)remove_trailing_spaces(devices[NGPS_port].device_host_name);

    temp_ptr = XmTextFieldGetString(NGPS_host_port_data);
    devices[NGPS_port].sp=atoi(temp_ptr);
    XtFree(temp_ptr);

    temp_ptr = XmTextFieldGetString(NGPS_comment);
    xastir_snprintf(devices[NGPS_port].comment,
        sizeof(devices[NGPS_port].comment),
        "%s",
        temp_ptr);
    XtFree(temp_ptr);

    (void)remove_trailing_spaces(devices[NGPS_port].comment);

    if(XmToggleButtonGetState(NGPS_active_on_startup))
        devices[NGPS_port].connect_on_startup=1;
    else
        devices[NGPS_port].connect_on_startup=0;

    if (XmToggleButtonGetState(NGPS_host_reconnect_data))
        devices[NGPS_port].reconnect=1;
    else
        devices[NGPS_port].reconnect=0;

    if (XmToggleButtonGetState(NGPS_set_time))
        devices[NGPS_port].set_time=1;
    else
        devices[NGPS_port].set_time=0;

    /* reopen */
    if ( was_up ) {
        (void)add_device(NGPS_port,
            DEVICE_NET_GPSD,
            devices[NGPS_port].device_host_name,
            "",
            devices[NGPS_port].sp,
            0,
            0,
            devices[NGPS_port].reconnect,
            "");
    }


    /* delete list */
//    modify_device_list(4,0);


    /* add device type */
    devices[NGPS_port].device_type=DEVICE_NET_GPSD;

    /* rebuild list */
//    modify_device_list(3,0);


end_critical_section(&devices_lock, "interface_gui.c:Config_NGPS_change_data" );

    // Rebuild the interface control list
    update_interface_list();

    Config_NGPS_destroy_shell(widget,clientData,callData);
}





void Config_NGPS( /*@unused@*/ Widget w, int config_type, int port) {
    static Widget  pane, form, button_ok, button_cancel,
                hostn, portn, comment,
                sep;
    char temp[20];
    Atom delw;

    if (!config_NGPS_dialog) {
        NGPS_port=port;
        config_NGPS_dialog = XtVaCreatePopupShell(langcode("WPUPCFG019"),
            xmDialogShellWidgetClass, appshell,
            XmNdeleteResponse, XmDESTROY,
            XmNdefaultPosition, FALSE,
            NULL);

        pane = XtVaCreateWidget("Config_NGPS pane",xmPanedWindowWidgetClass, config_NGPS_dialog,
                          XmNbackground, colors[0xff],
                          NULL);

        form =  XtVaCreateWidget("Config_NGPS form",xmFormWidgetClass, pane,
                            XmNfractionBase, 5,
                            XmNbackground, colors[0xff],
                            XmNautoUnmanage, FALSE,
                            XmNshadowThickness, 1,
                            NULL);

        hostn = XtVaCreateManagedWidget(langcode("WPUPCFG017"),xmLabelWidgetClass, form,
                                      XmNtopAttachment, XmATTACH_FORM,
                                      XmNtopOffset, 10,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset, 10,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        NGPS_host_name_data = XtVaCreateManagedWidget("Config_NGPS host_data", xmTextFieldWidgetClass, form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNeditable,   TRUE,
                                      XmNcursorPositionVisible, TRUE,
                                      XmNsensitive, TRUE,
                                      XmNshadowThickness,    1,
                                      XmNcolumns, 25,
                                      XmNwidth, ((25*7)+2),
                                      XmNmaxLength, 40,
                                      XmNbackground, colors[0x0f],
                                      XmNtopAttachment,XmATTACH_FORM,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, hostn,
                                      XmNleftOffset, 10,
                                      XmNrightAttachment,XmATTACH_FORM,
                                      XmNrightOffset, 5,
                                      NULL);

        portn = XtVaCreateManagedWidget(langcode("WPUPCFG018"),xmLabelWidgetClass, form,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, hostn,
                                      XmNtopOffset, 12,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset, 10,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        NGPS_host_port_data = XtVaCreateManagedWidget("Config_NGPS port_data", xmTextFieldWidgetClass, form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNeditable,   TRUE,
                                      XmNcursorPositionVisible, TRUE,
                                      XmNsensitive, TRUE,
                                      XmNshadowThickness,    1,
                                      XmNcolumns, 25,
                                      XmNwidth, ((25*7)+2),
                                      XmNmaxLength, 40,
                                      XmNbackground, colors[0x0f],
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, hostn,
                                      XmNtopOffset, 8,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, portn,
                                      XmNleftOffset, 10,
                                      XmNrightAttachment,XmATTACH_FORM,
                                      XmNrightOffset, 5,
                                      NULL);

        comment = XtVaCreateManagedWidget(langcode("WPUPCFS017"),xmLabelWidgetClass, form,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, portn,
                                      XmNtopOffset, 12,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset, 10,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        NGPS_comment = XtVaCreateManagedWidget("Config_NGPS comment", xmTextFieldWidgetClass, form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNeditable,   TRUE,
                                      XmNcursorPositionVisible, TRUE,
                                      XmNsensitive, TRUE,
                                      XmNshadowThickness,    1,
                                      XmNcolumns, 25,
                                      XmNwidth, ((25*7)+2),
                                      XmNmaxLength, 49,
                                      XmNbackground, colors[0x0f],
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, portn,
                                      XmNtopOffset, 8,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, comment,
                                      XmNleftOffset, 10,
                                      XmNrightAttachment,XmATTACH_FORM,
                                      XmNrightOffset, 5,
                                      NULL);

        NGPS_active_on_startup  = XtVaCreateManagedWidget(langcode("UNIOP00011"),xmToggleButtonWidgetClass,form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, comment,
                                      XmNtopOffset, 15,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset ,10,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        NGPS_host_reconnect_data  = XtVaCreateManagedWidget(langcode("WPUPCFG020"),xmToggleButtonWidgetClass,form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, NGPS_active_on_startup,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset ,10,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        NGPS_set_time  = XtVaCreateManagedWidget(langcode("UNIOP00029"), xmToggleButtonWidgetClass, form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, NGPS_host_reconnect_data,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset ,10,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

// We can only set the time properly on Linux systems
#ifndef HAVE_SETTIMEOFDAY
        XtSetSensitive(NGPS_set_time,FALSE);
#endif  // HAVE_SETTIMEOFDAY
#ifdef __CYGWIN__
        XtSetSensitive(NGPS_set_time,FALSE);
#endif  // __CYGWIN__


 
        sep = XtVaCreateManagedWidget("Config_NGPS sep", xmSeparatorGadgetClass,form,
                                      XmNorientation, XmHORIZONTAL,
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, NGPS_set_time,
                                      XmNtopOffset, 20,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNrightAttachment,XmATTACH_FORM,
                                      XmNbackground, colors[0xff],
                                      NULL);

        button_ok = XtVaCreateManagedWidget(langcode("UNIOP00001"),xmPushButtonGadgetClass, form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, sep,
                                      XmNtopOffset, 10,
                                      XmNbottomAttachment, XmATTACH_FORM,
                                      XmNbottomOffset, 5,
                                      XmNleftAttachment, XmATTACH_POSITION,
                                      XmNleftPosition, 1,
                                      XmNrightAttachment, XmATTACH_POSITION,
                                      XmNrightPosition, 2,
                                      XmNbackground, colors[0xff],
                                      NULL);

        button_cancel = XtVaCreateManagedWidget(langcode("UNIOP00002"),xmPushButtonGadgetClass, form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, sep,
                                      XmNtopOffset, 10,
                                      XmNbottomAttachment, XmATTACH_FORM,
                                      XmNbottomOffset, 5,
                                      XmNleftAttachment, XmATTACH_POSITION,
                                      XmNleftPosition, 3,
                                      XmNrightAttachment, XmATTACH_POSITION,
                                      XmNrightPosition, 4,
                                      XmNbackground, colors[0xff],
                                      NULL);

        XtAddCallback(button_ok, XmNactivateCallback, Config_NGPS_change_data, config_NGPS_dialog);
        XtAddCallback(button_cancel, XmNactivateCallback, Config_NGPS_destroy_shell, config_NGPS_dialog);

        pos_dialog(config_NGPS_dialog);

        delw = XmInternAtom(XtDisplay(config_NGPS_dialog),"WM_DELETE_WINDOW", FALSE);
        XmAddWMProtocolCallback(config_NGPS_dialog, delw, Config_NGPS_destroy_shell, (XtPointer)config_NGPS_dialog);

        if (config_type==0) {
            /* first time port */
            XmTextFieldSetString(NGPS_host_name_data,"localhost");
            XmTextFieldSetString(NGPS_host_port_data,"1234");
            XmTextFieldSetString(NGPS_comment,"");
            XmToggleButtonSetState(NGPS_active_on_startup,TRUE,FALSE);
            XmToggleButtonSetState(NGPS_host_reconnect_data,TRUE,FALSE);
            XmToggleButtonSetState(NGPS_set_time, FALSE, FALSE);
        } else {
            /* reconfig */

begin_critical_section(&devices_lock, "interface_gui.c:Config_NGPS" );

            XmTextFieldSetString(NGPS_host_name_data,devices[NGPS_port].device_host_name);
            xastir_snprintf(temp, sizeof(temp), "%d", devices[NGPS_port].sp); /* port number */
            XmTextFieldSetString(NGPS_host_port_data,temp);
            XmTextFieldSetString(NGPS_comment,devices[NGPS_port].comment);

            if (devices[NGPS_port].connect_on_startup)
                XmToggleButtonSetState(NGPS_active_on_startup,TRUE,FALSE);
            else
                XmToggleButtonSetState(NGPS_active_on_startup,FALSE,FALSE);

            if (devices[NGPS_port].reconnect)
                XmToggleButtonSetState(NGPS_host_reconnect_data,TRUE,FALSE);
            else
                XmToggleButtonSetState(NGPS_host_reconnect_data,FALSE,FALSE);

            if (devices[NGPS_port].set_time)
                XmToggleButtonSetState(NGPS_set_time, TRUE, FALSE);
            else
                XmToggleButtonSetState(NGPS_set_time, FALSE, FALSE);
 
end_critical_section(&devices_lock, "interface_gui.c:Config_NGPS" );

        }
        XtManageChild(form);
        XtManageChild(pane);

        XtPopup(config_NGPS_dialog,XtGrabNone);
        fix_dialog_size(config_NGPS_dialog);
    } else
        (void)XRaiseWindow(XtDisplay(config_NGPS_dialog), XtWindow(config_NGPS_dialog));
}





/*****************************************************/
/* Configure AX.25 TNC GUI                           */
/*****************************************************/

/**** AX.25 CONFIGURE ******/
int AX25_port;
Widget config_AX25_dialog = (Widget)NULL;
Widget AX25_device_name_data;
Widget AX25_comment;
Widget AX25_unproto1_data;
Widget AX25_unproto2_data;
Widget AX25_unproto3_data;
Widget AX25_igate_data;
Widget AX25_active_on_startup;
Widget AX25_transmit_data;
Widget AX25_relay_digipeat;





void Config_AX25_destroy_shell( /*@unused@*/ Widget widget, XtPointer clientData,  /*@unused@*/ XtPointer callData) {
    Widget shell = (Widget) clientData;
    XtPopdown(shell);
    XtDestroyWidget(shell);
    config_AX25_dialog = (Widget)NULL;
    if (choose_interface_dialog != NULL)
        Choose_interface_destroy_shell(choose_interface_dialog,choose_interface_dialog,NULL);

    choose_interface_dialog = (Widget)NULL;
}





void Config_AX25_change_data(Widget widget, XtPointer clientData, XtPointer callData) {
    int was_up;
    char *temp_ptr;

    busy_cursor(appshell);

    was_up=0;
    if (get_device_status(AX25_port) == DEVICE_IN_USE) {
        /* if active shutdown before changes are made */
        /*fprintf(stderr,"Device is up, shutting down\n");*/
        (void)del_device(AX25_port);
        was_up=1;
        usleep(1000000); // Wait for one second
    }

begin_critical_section(&devices_lock, "interface_gui.c:Config_AX25_change_data" );

    temp_ptr = XmTextFieldGetString(AX25_device_name_data);
    xastir_snprintf(devices[AX25_port].device_name,
        sizeof(devices[AX25_port].device_name),
        "%s",
        temp_ptr);
    XtFree(temp_ptr);

    (void)remove_trailing_spaces(devices[AX25_port].device_name);

    temp_ptr = XmTextFieldGetString(AX25_comment);
    xastir_snprintf(devices[AX25_port].comment,
        sizeof(devices[AX25_port].comment),
        "%s",
        temp_ptr);
    XtFree(temp_ptr);

    (void)remove_trailing_spaces(devices[AX25_port].comment);

    if(XmToggleButtonGetState(AX25_active_on_startup))
        devices[AX25_port].connect_on_startup=1;
    else
        devices[AX25_port].connect_on_startup=0;

    if(XmToggleButtonGetState(AX25_transmit_data)) {
        devices[AX25_port].transmit_data=1;
        XtSetSensitive(AX25_relay_digipeat, TRUE);
    }
    else {
        devices[AX25_port].transmit_data=0;
        XtSetSensitive(AX25_relay_digipeat, FALSE);
    }

    if(XmToggleButtonGetState(AX25_relay_digipeat))
        devices[AX25_port].relay_digipeat=1;
    else
        devices[AX25_port].relay_digipeat=0;

    devices[AX25_port].igate_options=device_igate_options;

    temp_ptr = XmTextFieldGetString(AX25_unproto1_data);
    xastir_snprintf(devices[AX25_port].unproto1,
        sizeof(devices[AX25_port].unproto1),
        "%s",
        temp_ptr);
    XtFree(temp_ptr);

    (void)remove_trailing_spaces(devices[AX25_port].unproto1);

    if(check_unproto_path(devices[AX25_port].unproto1)) {
        popup_message_always(langcode("WPUPCFT042"),
            langcode("WPUPCFT043"));
    }

    temp_ptr = XmTextFieldGetString(AX25_unproto2_data);
    xastir_snprintf(devices[AX25_port].unproto2,
        sizeof(devices[AX25_port].unproto2),
        "%s",
        temp_ptr);
    XtFree(temp_ptr);
    
    (void)remove_trailing_spaces(devices[AX25_port].unproto2);

    if(check_unproto_path(devices[AX25_port].unproto2)) {
        popup_message_always(langcode("WPUPCFT042"),
            langcode("WPUPCFT043"));
    }

    temp_ptr = XmTextFieldGetString(AX25_unproto3_data);
    xastir_snprintf(devices[AX25_port].unproto3,
        sizeof(devices[AX25_port].unproto3),
        "%s",
        temp_ptr);
    XtFree(temp_ptr);
 
    (void)remove_trailing_spaces(devices[AX25_port].unproto3);

    if(check_unproto_path(devices[AX25_port].unproto3)) {
        popup_message_always(langcode("WPUPCFT042"),
            langcode("WPUPCFT043"));
    }

    temp_ptr = XmTextFieldGetString(AX25_igate_data);
    xastir_snprintf(devices[AX25_port].unproto_igate,
        sizeof(devices[AX25_port].unproto_igate),
        "%s",
        temp_ptr);
    XtFree(temp_ptr);

    (void)remove_trailing_spaces(devices[AX25_port].unproto_igate);

    if(check_unproto_path(devices[AX25_port].unproto_igate)) {
        popup_message_always(langcode("WPUPCFT044"),
            langcode("WPUPCFT043"));
    }


    devices[AX25_port].reconnect=1;

// reopen if open before - n8ysz 20041213
//    if (devices[AX25_port].connect_on_startup==1 || was_up) {
    if ( was_up) {
        (void)add_device(AX25_port,
            DEVICE_AX25_TNC,
            devices[AX25_port].device_name,
            "",
            -1,
            -1,
            -1,
            0,
            "");
    }


    /* delete list */
//    modify_device_list(4,0);


    /* add device type */
    devices[AX25_port].device_type=DEVICE_AX25_TNC;

    /* rebuild list */
//    modify_device_list(3,0);


end_critical_section(&devices_lock, "interface_gui.c:Config_AX25_change_data" );

    // Rebuild the interface control list
    update_interface_list();

    Config_AX25_destroy_shell(widget,clientData,callData);
}





void Config_AX25( /*@unused@*/ Widget w, int config_type, int port) {
    static Widget  pane, form, button_ok, button_cancel, frame,
                devn, comment,
                proto, proto1, proto2, proto3,
                igate, igate_box,
                igate_o_0, igate_o_1, igate_o_2,
                igate_label,
                sep;

    char temp[50];
    Atom delw;
    Arg al[50];                    /* Arg List */
    register unsigned int ac = 0;           /* Arg Count */

    if(!config_AX25_dialog) {
        AX25_port=port;
        config_AX25_dialog = XtVaCreatePopupShell(langcode("WPUPCAX001"),
            xmDialogShellWidgetClass, appshell,
            XmNdeleteResponse, XmDESTROY,
            XmNdefaultPosition, FALSE,
            NULL);

        pane = XtVaCreateWidget("Config_AX25 pane",xmPanedWindowWidgetClass, config_AX25_dialog,
                          XmNbackground, colors[0xff],
                          NULL);

        form =  XtVaCreateWidget("Config_AX25 form",xmFormWidgetClass, pane,
                            XmNfractionBase, 5,
                            XmNbackground, colors[0xff],
                            XmNautoUnmanage, FALSE,
                            XmNshadowThickness, 1,
                            NULL);

        AX25_active_on_startup  = XtVaCreateManagedWidget(langcode("UNIOP00011"),xmToggleButtonWidgetClass,form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNtopAttachment, XmATTACH_FORM,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset ,10,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        AX25_transmit_data = XtVaCreateManagedWidget(langcode("UNIOP00010"),xmToggleButtonWidgetClass,form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNtopAttachment, XmATTACH_FORM,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, AX25_active_on_startup,
                                      XmNleftOffset ,35,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        AX25_relay_digipeat = XtVaCreateManagedWidget(langcode("UNIOP00030"),xmToggleButtonWidgetClass,form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNtopAttachment, XmATTACH_FORM,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, AX25_transmit_data,
                                      XmNleftOffset ,35,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        devn = XtVaCreateManagedWidget(langcode("WPUPCAX002"),xmLabelWidgetClass, form,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, AX25_active_on_startup,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset, 10,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        AX25_device_name_data = XtVaCreateManagedWidget("Config_AX25 device_data", xmTextFieldWidgetClass, form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNeditable,   TRUE,
                                      XmNcursorPositionVisible, TRUE,
                                      XmNsensitive, TRUE,
                                      XmNshadowThickness,    1,
                                      XmNcolumns, 15,
                                      XmNwidth, ((15*7)+2),
                                      XmNmaxLength, 40,
                                      XmNbackground, colors[0x0f],
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, AX25_active_on_startup,
                                      XmNtopOffset, 2,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, devn,
                                      XmNleftOffset, 10,
                                      XmNrightAttachment,XmATTACH_NONE,
                                      NULL);

        comment = XtVaCreateManagedWidget(langcode("WPUPCFS017"),xmLabelWidgetClass, form,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, AX25_active_on_startup,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, AX25_device_name_data,
                                      XmNleftOffset, 10,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        AX25_comment = XtVaCreateManagedWidget("Config_AX25 comment", xmTextFieldWidgetClass, form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNeditable,   TRUE,
                                      XmNcursorPositionVisible, TRUE,
                                      XmNsensitive, TRUE,
                                      XmNshadowThickness,    1,
                                      XmNcolumns, 15,
                                      XmNwidth, ((15*7)+2),
                                      XmNmaxLength, 40,
                                      XmNbackground, colors[0x0f],
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, AX25_active_on_startup,
                                      XmNtopOffset, 2,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, comment,
                                      XmNleftOffset, 10,
                                      XmNrightAttachment,XmATTACH_NONE,
                                      NULL);

        frame = XtVaCreateManagedWidget("Config_AX25 frame", xmFrameWidgetClass, form,
                                     XmNtopAttachment, XmATTACH_WIDGET,
                                     XmNtopWidget, devn,
                                     XmNtopOffset, 10,
                                     XmNbottomAttachment, XmATTACH_NONE,
                                     XmNleftAttachment, XmATTACH_FORM,
                                     XmNleftOffset, 10,
                                     XmNrightAttachment, XmATTACH_FORM,
                                     XmNrightOffset, 10,
                                     XmNbackground, colors[0xff],
                                     NULL);

        igate = XtVaCreateManagedWidget(langcode("IGPUPCF000"),xmLabelWidgetClass, frame,
                                    XmNchildType, XmFRAME_TITLE_CHILD,
                                    XmNbackground, colors[0xff],
                                    NULL);

        /* set args for color */
        ac=0;
        XtSetArg(al[ac], XmNbackground, colors[0xff]); ac++;


        igate_box = XmCreateRadioBox(frame,"Config_AX25 IGate box",al,ac);

        XtVaSetValues(igate_box,
                  XmNorientation, XmVERTICAL,
                  XmNnumColumns,2,
                  NULL);

        igate_o_0 = XtVaCreateManagedWidget(langcode("IGPUPCF001"),xmToggleButtonGadgetClass,
                                        igate_box,
                                        XmNbackground, colors[0xff],
                                        NULL);

        XtAddCallback(igate_o_0,XmNvalueChangedCallback,igate_toggle,"0");

        igate_o_1 = XtVaCreateManagedWidget(langcode("IGPUPCF002"),xmToggleButtonGadgetClass,
                                        igate_box,
                                        XmNbackground, colors[0xff],
                                        NULL);

        XtAddCallback(igate_o_1,XmNvalueChangedCallback,igate_toggle,"1");

        igate_o_2 = XtVaCreateManagedWidget(langcode("IGPUPCF003"),xmToggleButtonGadgetClass,
                                        igate_box,
                                        XmNbackground, colors[0xff],
                                        NULL);

        XtAddCallback(igate_o_2,XmNvalueChangedCallback,igate_toggle,"2");

        proto = XtVaCreateManagedWidget(langcode("WPUPCFT011"), xmLabelWidgetClass, form,
                                      XmNorientation, XmHORIZONTAL,
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, frame,
                                      XmNtopOffset, 10,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset,5,
                                      XmNrightAttachment,XmATTACH_FORM,
                                      XmNrightOffset,5,
                                      XmNbackground, colors[0xff],
                                      NULL);

        xastir_snprintf(temp, sizeof(temp), langcode("WPUPCFT012"), VERSIONFRM);

        proto1 = XtVaCreateManagedWidget(temp, xmLabelWidgetClass, form,
                                      XmNorientation, XmHORIZONTAL,
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, proto,
                                      XmNtopOffset, 12,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset, 60,
                                      XmNrightAttachment,XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        AX25_unproto1_data = XtVaCreateManagedWidget("Config_AX25 protopath1", xmTextFieldWidgetClass, form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNeditable,   TRUE,
                                      XmNcursorPositionVisible, TRUE,
                                      XmNsensitive, TRUE,
                                      XmNshadowThickness,    1,
                                      XmNcolumns, 25,
                                      XmNwidth, ((25*7)+2),
                                      XmNmaxLength, 40,
                                      XmNbackground, colors[0x0f],
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, proto,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment,XmATTACH_WIDGET,
                                      XmNleftWidget, proto1,
                                      XmNleftOffset, 5,
                                      XmNrightAttachment,XmATTACH_NONE,
                                      NULL);

        xastir_snprintf(temp, sizeof(temp), langcode("WPUPCFT013"), VERSIONFRM);

        proto2 = XtVaCreateManagedWidget(temp, xmLabelWidgetClass, form,
                                      XmNorientation, XmHORIZONTAL,
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, proto1,
                                      XmNtopOffset, 15,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset, 60,
                                      XmNrightAttachment,XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        AX25_unproto2_data = XtVaCreateManagedWidget("Config_AX25 protopath2", xmTextFieldWidgetClass, form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNeditable,   TRUE,
                                      XmNcursorPositionVisible, TRUE,
                                      XmNsensitive, TRUE,
                                      XmNshadowThickness,    1,
                                      XmNcolumns, 25,
                                      XmNwidth, ((25*7)+2),
                                      XmNmaxLength, 40,
                                      XmNbackground, colors[0x0f],
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, AX25_unproto1_data,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, proto2,
                                      XmNleftOffset, 5,
                                      XmNrightAttachment,XmATTACH_NONE,
                                      NULL);

        xastir_snprintf(temp, sizeof(temp), langcode("WPUPCFT014"), VERSIONFRM);

        proto3 = XtVaCreateManagedWidget(temp, xmLabelWidgetClass, form,
                                      XmNorientation, XmHORIZONTAL,
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, proto2,
                                      XmNtopOffset, 15,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset, 60,
                                      XmNrightAttachment,XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        AX25_unproto3_data = XtVaCreateManagedWidget("Config_AX25 protopath3", xmTextFieldWidgetClass, form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNeditable,   TRUE,
                                      XmNcursorPositionVisible, TRUE,
                                      XmNsensitive, TRUE,
                                      XmNshadowThickness,    1,
                                      XmNcolumns, 25,
                                      XmNwidth, ((25*7)+2),
                                      XmNmaxLength, 40,
                                      XmNbackground, colors[0x0f],
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, AX25_unproto2_data,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment,XmATTACH_WIDGET,
                                      XmNleftWidget, proto3,
                                      XmNleftOffset, 5,
                                      XmNrightAttachment,XmATTACH_NONE,
                                      NULL);

        xastir_snprintf(temp, sizeof(temp), langcode("IGPUPCF004"));
        igate_label = XtVaCreateManagedWidget(temp, xmLabelWidgetClass, form,
                                      XmNorientation, XmHORIZONTAL,
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, proto3,
                                      XmNtopOffset, 15,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset, 60,
                                      XmNrightAttachment,XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        AX25_igate_data = XtVaCreateManagedWidget("Config_TNC igate_data", xmTextFieldWidgetClass, form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNeditable,   TRUE,
                                      XmNcursorPositionVisible, TRUE,
                                      XmNsensitive, TRUE,
                                      XmNshadowThickness,    1,
                                      XmNcolumns, 25,
                                      XmNwidth, ((25*7)+2),
                                      XmNmaxLength, 40,
                                      XmNbackground, colors[0x0f],
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, AX25_unproto3_data,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment,XmATTACH_WIDGET,
                                      XmNleftWidget, igate_label,
                                      XmNleftOffset, 5,
                                      XmNrightAttachment,XmATTACH_NONE,
                                      NULL);

        sep = XtVaCreateManagedWidget("Config_AX25 sep", xmSeparatorGadgetClass,form,
                                      XmNorientation, XmHORIZONTAL,
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, igate_label,
                                      XmNtopOffset, 20,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNrightAttachment,XmATTACH_FORM,
                                      XmNbackground, colors[0xff],
                                      NULL);

        button_ok = XtVaCreateManagedWidget(langcode("UNIOP00001"),xmPushButtonGadgetClass, form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, sep,
                                      XmNtopOffset, 10,
                                      XmNbottomAttachment, XmATTACH_FORM,
                                      XmNbottomOffset, 5,
                                      XmNleftAttachment, XmATTACH_POSITION,
                                      XmNleftPosition, 1,
                                      XmNrightAttachment, XmATTACH_POSITION,
                                      XmNrightPosition, 2,
                                      XmNbackground, colors[0xff],
                                      NULL);

        button_cancel = XtVaCreateManagedWidget(langcode("UNIOP00002"),xmPushButtonGadgetClass, form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, sep,
                                      XmNtopOffset, 10,
                                      XmNbottomAttachment, XmATTACH_FORM,
                                      XmNbottomOffset, 5,
                                      XmNleftAttachment, XmATTACH_POSITION,
                                      XmNleftPosition, 3,
                                      XmNrightAttachment, XmATTACH_POSITION,
                                      XmNrightPosition, 4,
                                      XmNbackground, colors[0xff],
                                      NULL);

#ifdef HAVE_LIBAX25
        XtAddCallback(button_ok, XmNactivateCallback, Config_AX25_change_data, config_AX25_dialog);
#endif /* USE_AX25 */
        XtAddCallback(button_cancel, XmNactivateCallback, Config_AX25_destroy_shell, config_AX25_dialog);

        pos_dialog(config_AX25_dialog);

        delw = XmInternAtom(XtDisplay(config_AX25_dialog),"WM_DELETE_WINDOW", FALSE);
        XmAddWMProtocolCallback(config_AX25_dialog, delw, Config_AX25_destroy_shell, (XtPointer)config_AX25_dialog);

        if (config_type==0) {
            /* first time port */
            XmToggleButtonSetState(AX25_active_on_startup,TRUE,FALSE);
            XmToggleButtonSetState(AX25_transmit_data,TRUE,FALSE);
            XmToggleButtonSetState(AX25_relay_digipeat,FALSE,FALSE);
            XmTextFieldSetString(AX25_device_name_data,"");
            XmTextFieldSetString(AX25_comment,"");
            device_igate_options=0;
            XmToggleButtonSetState(igate_o_0,TRUE,FALSE);
            XmTextFieldSetString(AX25_unproto1_data,"WIDE2-2");
            XmTextFieldSetString(AX25_unproto2_data,"");
            XmTextFieldSetString(AX25_unproto3_data,"");
            XmTextFieldSetString(AX25_igate_data,"");
        } else {
            /* reconfig */

begin_critical_section(&devices_lock, "interface_gui.c:Config_AX25" );

            if (devices[AX25_port].connect_on_startup)
                XmToggleButtonSetState(AX25_active_on_startup,TRUE,FALSE);
            else
                XmToggleButtonSetState(AX25_active_on_startup,FALSE,FALSE);

            switch (devices[AX25_port].igate_options) {
                case(0):
                    XmToggleButtonSetState(igate_o_0,TRUE,FALSE);
                    device_igate_options=0;
                    break;

                case(1):
                    XmToggleButtonSetState(igate_o_1,TRUE,FALSE);
                    device_igate_options=1;
                    break;

                case(2):
                    XmToggleButtonSetState(igate_o_2,TRUE,FALSE);
                    device_igate_options=2;
                    break;

                default:
                    XmToggleButtonSetState(igate_o_0,TRUE,FALSE);
                    device_igate_options=0;
                    break;
            }
            if (devices[AX25_port].transmit_data)
                XmToggleButtonSetState(AX25_transmit_data,TRUE,FALSE);
            else
                XmToggleButtonSetState(AX25_transmit_data,FALSE,FALSE);

            if (devices[AX25_port].relay_digipeat)
                XmToggleButtonSetState(AX25_relay_digipeat,TRUE,FALSE);
            else
                XmToggleButtonSetState(AX25_relay_digipeat,FALSE,FALSE);

            XmTextFieldSetString(AX25_device_name_data,devices[AX25_port].device_name);
            XmTextFieldSetString(AX25_comment,devices[AX25_port].comment);
            XmTextFieldSetString(AX25_unproto1_data,devices[AX25_port].unproto1);
            XmTextFieldSetString(AX25_unproto2_data,devices[AX25_port].unproto2);
            XmTextFieldSetString(AX25_unproto3_data,devices[AX25_port].unproto3);
            XmTextFieldSetString(AX25_igate_data,devices[AX25_port].unproto_igate);
 
end_critical_section(&devices_lock, "interface_gui.c:Config_AX25" );

        }
        XtManageChild(form);
        XtManageChild(igate_box);
        XtManageChild(pane);

        XtPopup(config_AX25_dialog,XtGrabNone);
        fix_dialog_size(config_AX25_dialog);
    } else {
        (void)XRaiseWindow(XtDisplay(config_AX25_dialog), XtWindow(config_AX25_dialog));
    }
}





/*****************************************************/
/* Configure Network server GUI                      */
/*****************************************************/

/**** INTERNET CONFIGURE ******/
Widget config_Inet_dialog = (Widget)NULL;
Widget Inet_active_on_startup;
Widget Inet_host_data;
Widget Inet_port_data;
Widget Inet_comment;
Widget Inet_password_data;
Widget Inet_filter_data;
Widget Inet_transmit_data;
Widget Inet_reconnect_data;
int    Inet_port;





void Inet_destroy_shell( /*@unused@*/ Widget widget, XtPointer clientData,  /*@unused@*/ XtPointer callData) {
    Widget shell = (Widget) clientData;
    XtPopdown(shell);
    XtDestroyWidget(shell);
    config_Inet_dialog = (Widget)NULL;
    if (choose_interface_dialog != NULL)
        Choose_interface_destroy_shell(choose_interface_dialog,choose_interface_dialog,NULL);

    choose_interface_dialog = (Widget)NULL;
}





void Inet_change_data(Widget widget, XtPointer clientData, XtPointer callData) {
    int was_up;
    char *temp_ptr;


    busy_cursor(appshell);
    was_up=0;
    if (get_device_status(Inet_port) == DEVICE_IN_USE) {
        /* if active shutdown before changes are made */
        /*fprintf(stderr,"Device is up, shutting down\n");*/
        (void)del_device(Inet_port);
        was_up=1;
        usleep(1000000); // Wait for one second
    }

begin_critical_section(&devices_lock, "interface_gui.c:Inet_change_data" );

    temp_ptr = XmTextFieldGetString(Inet_host_data);
    xastir_snprintf(devices[Inet_port].device_host_name,
        sizeof(devices[Inet_port].device_host_name),
        "%s",
        temp_ptr);
    XtFree(temp_ptr);

    (void)remove_trailing_spaces(devices[Inet_port].device_host_name);

    temp_ptr = XmTextFieldGetString(Inet_password_data);
    xastir_snprintf(devices[Inet_port].device_host_pswd,
        sizeof(devices[Inet_port].device_host_pswd),
        "%s",
        temp_ptr);
    XtFree(temp_ptr);

    (void)remove_trailing_spaces(devices[Inet_port].device_host_pswd);

    temp_ptr = XmTextFieldGetString(Inet_filter_data);
    xastir_snprintf(devices[Inet_port].device_host_filter_string,
        sizeof(devices[Inet_port].device_host_filter_string),
        "%s",
        temp_ptr);
    XtFree(temp_ptr);

    (void)remove_trailing_spaces(devices[Inet_port].device_host_filter_string);

    temp_ptr = XmTextFieldGetString(Inet_comment);
    xastir_snprintf(devices[Inet_port].comment,
        sizeof(devices[Inet_port].comment),
        "%s",
        temp_ptr);
    XtFree(temp_ptr);
    
    (void)remove_trailing_spaces(devices[Inet_port].comment);

    temp_ptr = XmTextFieldGetString(Inet_port_data);
    devices[Inet_port].sp=atoi(temp_ptr);
    XtFree(temp_ptr);

    if(XmToggleButtonGetState(Inet_active_on_startup))
        devices[Inet_port].connect_on_startup=1;
    else
        devices[Inet_port].connect_on_startup=0;

    if(XmToggleButtonGetState(Inet_transmit_data))
        devices[Inet_port].transmit_data=1;
    else
        devices[Inet_port].transmit_data=0;

    if(XmToggleButtonGetState(Inet_reconnect_data))
        devices[Inet_port].reconnect=1;
    else
        devices[Inet_port].reconnect=0;

//    Changed 20041213 per emails with we7u - n8ysz
//    if (devices[Inet_port].connect_on_startup==1 || was_up) {

    if ( was_up) {
        (void)add_device(Inet_port,
            DEVICE_NET_STREAM,
            devices[Inet_port].device_host_name,
            devices[Inet_port].device_host_pswd,
            devices[Inet_port].sp,
            0,
            0,
            devices[Inet_port].reconnect,
            devices[Inet_port].device_host_filter_string);
    }

    /* delete list */
//    modify_device_list(4,0);


    /* add device type */
    devices[Inet_port].device_type=DEVICE_NET_STREAM;

    /* rebuild list */
//    modify_device_list(3,0);


end_critical_section(&devices_lock, "interface_gui.c:Inet_change_data" );

    // Rebuild the interface control list
    update_interface_list();

    Inet_destroy_shell(widget,clientData,callData);
}





void Config_Inet( /*@unused@*/ Widget w, int config_type, int port) {
    static Widget  pane, form, button_ok, button_cancel,
                ihost, iport, password, password_fl,
                filter, comment, sep;

    Atom delw;
    char temp[40];

    if(!config_Inet_dialog) {
        Inet_port=port;
        config_Inet_dialog = XtVaCreatePopupShell(langcode("WPUPCFI001"),
            xmDialogShellWidgetClass, appshell,
            XmNdeleteResponse, XmDESTROY,
            XmNdefaultPosition, FALSE,
            NULL);

        pane = XtVaCreateWidget("Config_Inet pane",xmPanedWindowWidgetClass, config_Inet_dialog,
                          XmNbackground, colors[0xff],
                          NULL);

        form =  XtVaCreateWidget("Config_Inet form",xmFormWidgetClass, pane,
                            XmNfractionBase, 5,
                            XmNbackground, colors[0xff],
                            XmNautoUnmanage, FALSE,
                            XmNshadowThickness, 1,
                            NULL);

        Inet_active_on_startup  = XtVaCreateManagedWidget(langcode("UNIOP00011"),xmToggleButtonWidgetClass,form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNtopAttachment, XmATTACH_FORM,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset ,10,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        Inet_transmit_data  = XtVaCreateManagedWidget(langcode("UNIOP00010"),xmToggleButtonWidgetClass,form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, Inet_active_on_startup,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset ,10,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        ihost = XtVaCreateManagedWidget(langcode("WPUPCFI002"),xmLabelWidgetClass, form,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, Inet_transmit_data,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset, 10,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        Inet_host_data = XtVaCreateManagedWidget("Config_Inet host_data", xmTextFieldWidgetClass, form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNeditable,   TRUE,
                                      XmNcursorPositionVisible, TRUE,
                                      XmNsensitive, TRUE,
                                      XmNshadowThickness,    1,
                                      XmNcolumns, 25,
                                      XmNwidth, ((25*7)+2),
                                      XmNmaxLength, 30,
                                      XmNbackground, colors[0x0f],
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, Inet_transmit_data,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, ihost,
                                      XmNrightAttachment,XmATTACH_NONE,
                                      NULL);

        iport = XtVaCreateManagedWidget(langcode("WPUPCFI003"),xmLabelWidgetClass, form,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget,Inet_transmit_data,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget,Inet_host_data,
                                      XmNleftOffset, 20,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        Inet_port_data = XtVaCreateManagedWidget("Config_Inet port_data", xmTextFieldWidgetClass, form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNeditable,   TRUE,
                                      XmNcursorPositionVisible, TRUE,
                                      XmNsensitive, TRUE,
                                      XmNshadowThickness,    1,
                                      XmNcolumns, 5,
                                      XmNmaxLength, 6,
                                      XmNbackground, colors[0x0f],
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, Inet_transmit_data,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, iport,
                                      XmNrightAttachment,XmATTACH_FORM,
                                      XmNrightOffset,10,
                                      NULL);

        password = XtVaCreateManagedWidget(langcode("WPUPCFI009"),xmLabelWidgetClass, form,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, ihost,
                                      XmNtopOffset, 20,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset, 10,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        Inet_password_data = XtVaCreateManagedWidget("Config_Inet password_data", xmTextFieldWidgetClass, form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNeditable,   TRUE,
                                      XmNcursorPositionVisible, TRUE,
                                      XmNsensitive, TRUE,
                                      XmNshadowThickness,    1,
                                      XmNcolumns, 20,
                                      XmNmaxLength, 20,
                                      XmNbackground, colors[0x0f],
                                      XmNleftAttachment,XmATTACH_WIDGET,
                                      XmNleftWidget, password,
                                      XmNleftOffset, 10,
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, ihost,
                                      XmNtopOffset, 15,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNrightAttachment,XmATTACH_NONE,
                                      NULL);

        password_fl = XtVaCreateManagedWidget(langcode("WPUPCFI010"),xmLabelWidgetClass, form,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, ihost,
                                      XmNtopOffset, 20,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget,Inet_password_data,
                                      XmNleftOffset,20,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        filter = XtVaCreateManagedWidget(langcode("WPUPCFI015"),xmLabelWidgetClass, form,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, password,
                                      XmNtopOffset, 20,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset, 10,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        Inet_filter_data = XtVaCreateManagedWidget("Config_Inet filter_data", xmTextFieldWidgetClass, form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNeditable,   TRUE,
                                      XmNcursorPositionVisible, TRUE,
                                      XmNsensitive, TRUE,
                                      XmNshadowThickness,    1,
                                      XmNcolumns, 30,
                                      XmNmaxLength, 190,
                                      XmNbackground, colors[0x0f],
                                      XmNleftAttachment,XmATTACH_WIDGET,
                                      XmNleftWidget, filter,
                                      XmNleftOffset, 10,
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, password,
                                      XmNtopOffset, 15,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNrightAttachment,XmATTACH_NONE,
                                      NULL);

        comment = XtVaCreateManagedWidget(langcode("WPUPCFS017"),xmLabelWidgetClass, form,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, Inet_filter_data,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset, 10,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        Inet_comment = XtVaCreateManagedWidget("Config_Inet comment", xmTextFieldWidgetClass, form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNeditable,   TRUE,
                                      XmNcursorPositionVisible, TRUE,
                                      XmNsensitive, TRUE,
                                      XmNshadowThickness,    1,
                                      XmNcolumns, 25,
                                      XmNwidth, ((25*7)+2),
                                      XmNmaxLength, 49,
                                      XmNbackground, colors[0x0f],
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, Inet_filter_data,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, comment,
                                      XmNrightAttachment,XmATTACH_NONE,
                                      NULL);

        Inet_reconnect_data = XtVaCreateManagedWidget(langcode("WPUPCFI011"),xmToggleButtonWidgetClass,form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, comment,
                                      XmNtopOffset, 20,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset ,10,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        sep = XtVaCreateManagedWidget("Config_Inet sep", xmSeparatorGadgetClass,form,
                                      XmNorientation, XmHORIZONTAL,
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, Inet_reconnect_data,
                                      XmNtopOffset, 14,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNrightAttachment,XmATTACH_FORM,
                                      XmNbackground, colors[0xff],
                                      NULL);

        button_ok = XtVaCreateManagedWidget(langcode("UNIOP00001"),xmPushButtonGadgetClass, form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, sep,
                                      XmNtopOffset, 10,
                                      XmNbottomAttachment, XmATTACH_FORM,
                                      XmNbottomOffset, 5,
                                      XmNleftAttachment, XmATTACH_POSITION,
                                      XmNleftPosition, 1,
                                      XmNrightAttachment, XmATTACH_POSITION,
                                      XmNrightPosition, 2,
                                      XmNbackground, colors[0xff],
                                      NULL);

        button_cancel = XtVaCreateManagedWidget(langcode("UNIOP00002"),xmPushButtonGadgetClass, form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, sep,
                                      XmNtopOffset, 10,
                                      XmNbottomAttachment, XmATTACH_FORM,
                                      XmNbottomOffset, 5,
                                      XmNleftAttachment, XmATTACH_POSITION,
                                      XmNleftPosition, 3,
                                      XmNrightAttachment, XmATTACH_POSITION,
                                      XmNrightPosition, 4,
                                      XmNbackground, colors[0xff],
                                      NULL);

        XtAddCallback(button_ok, XmNactivateCallback, Inet_change_data, config_Inet_dialog);
        XtAddCallback(button_cancel, XmNactivateCallback, Inet_destroy_shell, config_Inet_dialog);

        pos_dialog(config_Inet_dialog);

        delw = XmInternAtom(XtDisplay(config_Inet_dialog),"WM_DELETE_WINDOW", FALSE);
        XmAddWMProtocolCallback(config_Inet_dialog, delw, Inet_destroy_shell, (XtPointer)config_Inet_dialog);

        if (config_type==0) {
            /* first time port */
            XmToggleButtonSetState(Inet_active_on_startup,TRUE,FALSE);
            XmToggleButtonSetState(Inet_transmit_data,TRUE,FALSE);
            //XmTextFieldSetString(Inet_host_data,"first.aprs.net");
            XmTextFieldSetString(Inet_host_data,"");
            XmTextFieldSetString(Inet_port_data,"10151");
            XmTextFieldSetString(Inet_filter_data,"");
            XmTextFieldSetString(Inet_comment,"");
            XmToggleButtonSetState(Inet_reconnect_data,FALSE,FALSE);
        } else {
            /* reconfig */

begin_critical_section(&devices_lock, "interface_gui.c:Config_Inet" );

           if (devices[Inet_port].connect_on_startup)
                XmToggleButtonSetState(Inet_active_on_startup,TRUE,FALSE);
            else
                XmToggleButtonSetState(Inet_active_on_startup,FALSE,FALSE);

            if (devices[Inet_port].transmit_data)
                XmToggleButtonSetState(Inet_transmit_data,TRUE,FALSE);
            else
                XmToggleButtonSetState(Inet_transmit_data,FALSE,FALSE);

            XmTextFieldSetString(Inet_host_data,devices[Inet_port].device_host_name);
            xastir_snprintf(temp, sizeof(temp), "%d", devices[Inet_port].sp);
            XmTextFieldSetString(Inet_port_data,temp);
            XmTextFieldSetString(Inet_password_data,devices[Inet_port].device_host_pswd);
            XmTextFieldSetString(Inet_filter_data,devices[Inet_port].device_host_filter_string);
            XmTextFieldSetString(Inet_comment,devices[Inet_port].comment);

            if (devices[Inet_port].reconnect)
                XmToggleButtonSetState(Inet_reconnect_data,TRUE,FALSE);
            else
                XmToggleButtonSetState(Inet_reconnect_data,FALSE,FALSE);

end_critical_section(&devices_lock, "interface_gui.c:Config_Inet" );

        }
        XtManageChild(form);
        XtManageChild(pane);

        XtPopup(config_Inet_dialog,XtGrabNone);
        fix_dialog_size(config_Inet_dialog);
    } else {
        (void)XRaiseWindow(XtDisplay(config_Inet_dialog), XtWindow(config_Inet_dialog));
    }
}





//WE7U-DATABASE
/*****************************************************/
/* Configure Database Server GUI                     */
/*****************************************************/

/**** DATABASE CONFIGURE ******/
Widget config_Database_dialog = (Widget)NULL;
Widget Database_active_on_startup;
Widget Database_host_data;
Widget Database_port_data;
Widget Database_comment;
Widget Database_password_data;
Widget Database_filter_data;
Widget Database_transmit_data;
Widget Database_reconnect_data;
int    Database_port;





void Database_destroy_shell( /*@unused@*/ Widget widget, XtPointer clientData,  /*@unused@*/ XtPointer callData) {
    Widget shell = (Widget) clientData;
    XtPopdown(shell);
    XtDestroyWidget(shell);
    config_Database_dialog = (Widget)NULL;
    if (choose_interface_dialog != NULL)
        Choose_interface_destroy_shell(choose_interface_dialog,choose_interface_dialog,NULL);

    choose_interface_dialog = (Widget)NULL;
}





void Database_change_data(Widget widget, XtPointer clientData, XtPointer callData) {
    int was_up;
    char *temp_ptr;


    busy_cursor(appshell);
    was_up=0;
    if (get_device_status(Database_port) == DEVICE_IN_USE) {
        /* if active shutdown before changes are made */
        /*fprintf(stderr,"Device is up, shutting down\n");*/
        (void)del_device(Database_port);
        was_up=1;
        usleep(1000000); // Wait for one second
    }

begin_critical_section(&devices_lock, "interface_gui.c:Database_change_data" );

    temp_ptr = XmTextFieldGetString(Database_host_data);
    xastir_snprintf(devices[Database_port].device_host_name,
        sizeof(devices[Database_port].device_host_name),
        "%s",
        temp_ptr);
    XtFree(temp_ptr);

    (void)remove_trailing_spaces(devices[Database_port].device_host_name);

    temp_ptr = XmTextFieldGetString(Database_password_data);
    xastir_snprintf(devices[Database_port].device_host_pswd,
        sizeof(devices[Database_port].device_host_pswd),
        "%s",
        temp_ptr);
    XtFree(temp_ptr);

    (void)remove_trailing_spaces(devices[Database_port].device_host_pswd);

    temp_ptr = XmTextFieldGetString(Database_filter_data);
    xastir_snprintf(devices[Database_port].device_host_filter_string,
        sizeof(devices[Database_port].device_host_filter_string),
        "%s",
        temp_ptr);
    XtFree(temp_ptr);

    (void)remove_trailing_spaces(devices[Database_port].device_host_filter_string);

    temp_ptr = XmTextFieldGetString(Database_comment);
    xastir_snprintf(devices[Database_port].comment,
        sizeof(devices[Database_port].comment),
        "%s",
        temp_ptr);
    XtFree(temp_ptr);

    (void)remove_trailing_spaces(devices[Database_port].comment);

    temp_ptr = XmTextFieldGetString(Database_port_data);
    devices[Database_port].sp=atoi(temp_ptr);
    XtFree(temp_ptr);

    if(XmToggleButtonGetState(Database_active_on_startup))
        devices[Database_port].connect_on_startup=1;
    else
        devices[Database_port].connect_on_startup=0;

    if(XmToggleButtonGetState(Database_transmit_data))
        devices[Database_port].transmit_data=1;
    else
        devices[Database_port].transmit_data=0;

    if(XmToggleButtonGetState(Database_reconnect_data))
        devices[Database_port].reconnect=1;
    else
        devices[Database_port].reconnect=0;

//    n8ysz 20041213
//    if (devices[Database_port].connect_on_startup==1 || was_up) {  
    if ( was_up) {
        (void)add_device(Database_port,
            DEVICE_NET_DATABASE,
            devices[Database_port].device_host_name,
            devices[Database_port].device_host_pswd,
            devices[Database_port].sp,
            0,
            0,
            devices[Database_port].reconnect,
            devices[Database_port].device_host_filter_string);
    }

    /* delete list */
//    modify_device_list(4,0);


    /* add device type */
    devices[Database_port].device_type=DEVICE_NET_DATABASE;

    /* rebuild list */
//    modify_device_list(3,0);


end_critical_section(&devices_lock, "interface_gui.c:Database_change_data" );

    // Rebuild the interface control list
    update_interface_list();

    Database_destroy_shell(widget,clientData,callData);
}





void Config_Database( /*@unused@*/ Widget w, int config_type, int port) {
    static Widget  pane, form, button_ok, button_cancel,
                ihost, iport, password, password_fl,
                filter, sep, comment;

    Atom delw;
    char temp[40];

    if(!config_Database_dialog) {
        Database_port=port;
        config_Database_dialog = XtVaCreatePopupShell(langcode("WPUPCFID01"),
            xmDialogShellWidgetClass, appshell,
            XmNdeleteResponse, XmDESTROY,
            XmNdefaultPosition, FALSE,
            NULL);

        pane = XtVaCreateWidget("Config_Database pane",xmPanedWindowWidgetClass, config_Database_dialog,
                          XmNbackground, colors[0xff],
                          NULL);

        form =  XtVaCreateWidget("Config_Database form",xmFormWidgetClass, pane,
                            XmNfractionBase, 5,
                            XmNbackground, colors[0xff],
                            XmNautoUnmanage, FALSE,
                            XmNshadowThickness, 1,
                            NULL);

        Database_active_on_startup  = XtVaCreateManagedWidget(langcode("UNIOP00011"),xmToggleButtonWidgetClass,form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNtopAttachment, XmATTACH_FORM,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset ,10,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        Database_transmit_data  = XtVaCreateManagedWidget(langcode("UNIOP00010"),xmToggleButtonWidgetClass,form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, Database_active_on_startup,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset ,10,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        ihost = XtVaCreateManagedWidget(langcode("WPUPCFID02"),xmLabelWidgetClass, form,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, Database_transmit_data,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset, 10,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        Database_host_data = XtVaCreateManagedWidget("Config_Database host_data", xmTextFieldWidgetClass, form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNeditable,   TRUE,
                                      XmNcursorPositionVisible, TRUE,
                                      XmNsensitive, TRUE,
                                      XmNshadowThickness,    1,
                                      XmNcolumns, 25,
                                      XmNwidth, ((25*7)+2),
                                      XmNmaxLength, 30,
                                      XmNbackground, colors[0x0f],
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, Database_transmit_data,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, ihost,
                                      XmNrightAttachment,XmATTACH_NONE,
                                      NULL);

        iport = XtVaCreateManagedWidget(langcode("WPUPCFID03"),xmLabelWidgetClass, form,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget,Database_transmit_data,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget,Database_host_data,
                                      XmNleftOffset, 20,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        Database_port_data = XtVaCreateManagedWidget("Config_Database port_data", xmTextFieldWidgetClass, form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNeditable,   TRUE,
                                      XmNcursorPositionVisible, TRUE,
                                      XmNsensitive, TRUE,
                                      XmNshadowThickness,    1,
                                      XmNcolumns, 5,
                                      XmNmaxLength, 6,
                                      XmNbackground, colors[0x0f],
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, Database_transmit_data,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, iport,
                                      XmNrightAttachment,XmATTACH_FORM,
                                      XmNrightOffset,10,
                                      NULL);

        password = XtVaCreateManagedWidget(langcode("WPUPCFID09"),xmLabelWidgetClass, form,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, ihost,
                                      XmNtopOffset, 20,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset, 10,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        Database_password_data = XtVaCreateManagedWidget("Config_Database password_data", xmTextFieldWidgetClass, form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNeditable,   TRUE,
                                      XmNcursorPositionVisible, TRUE,
                                      XmNsensitive, TRUE,
                                      XmNshadowThickness,    1,
                                      XmNcolumns, 20,
                                      XmNmaxLength, 20,
                                      XmNbackground, colors[0x0f],
                                      XmNleftAttachment,XmATTACH_WIDGET,
                                      XmNleftWidget, password,
                                      XmNleftOffset, 10,
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, ihost,
                                      XmNtopOffset, 15,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNrightAttachment,XmATTACH_NONE,
                                      NULL);

        password_fl = XtVaCreateManagedWidget(langcode("WPUPCFID10"),xmLabelWidgetClass, form,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, ihost,
                                      XmNtopOffset, 20,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget,Database_password_data,
                                      XmNleftOffset,20,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        filter = XtVaCreateManagedWidget(langcode("WPUPCFID15"),xmLabelWidgetClass, form,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, password,
                                      XmNtopOffset, 20,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset, 10,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        Database_filter_data = XtVaCreateManagedWidget("Config_Database filter_data", xmTextFieldWidgetClass, form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNeditable,   TRUE,
                                      XmNcursorPositionVisible, TRUE,
                                      XmNsensitive, TRUE,
                                      XmNshadowThickness,    1,
                                      XmNcolumns, 30,
                                      XmNmaxLength, 190,
                                      XmNbackground, colors[0x0f],
                                      XmNleftAttachment,XmATTACH_WIDGET,
                                      XmNleftWidget, filter,
                                      XmNleftOffset, 10,
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, password,
                                      XmNtopOffset, 15,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNrightAttachment,XmATTACH_NONE,
                                      NULL);

        comment = XtVaCreateManagedWidget(langcode("WPUPCFS017"),xmLabelWidgetClass, form,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, filter,
                                      XmNtopOffset, 20,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset, 10,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        Database_comment = XtVaCreateManagedWidget("Config_Database comment", xmTextFieldWidgetClass, form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNeditable,   TRUE,
                                      XmNcursorPositionVisible, TRUE,
                                      XmNsensitive, TRUE,
                                      XmNshadowThickness,    1,
                                      XmNcolumns, 25,
                                      XmNwidth, ((25*7)+2),
                                      XmNmaxLength, 49,
                                      XmNbackground, colors[0x0f],
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, filter,
                                      XmNtopOffset, 15,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, comment,
                                      XmNrightAttachment,XmATTACH_NONE,
                                      NULL);

        Database_reconnect_data = XtVaCreateManagedWidget(langcode("WPUPCFID11"),xmToggleButtonWidgetClass,form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, comment,
                                      XmNtopOffset, 20,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset ,10,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        sep = XtVaCreateManagedWidget("Config_Database sep", xmSeparatorGadgetClass,form,
                                      XmNorientation, XmHORIZONTAL,
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, Database_reconnect_data,
                                      XmNtopOffset, 14,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNrightAttachment,XmATTACH_FORM,
                                      XmNbackground, colors[0xff],
                                      NULL);

        button_ok = XtVaCreateManagedWidget(langcode("UNIOP00001"),xmPushButtonGadgetClass, form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, sep,
                                      XmNtopOffset, 10,
                                      XmNbottomAttachment, XmATTACH_FORM,
                                      XmNbottomOffset, 5,
                                      XmNleftAttachment, XmATTACH_POSITION,
                                      XmNleftPosition, 1,
                                      XmNrightAttachment, XmATTACH_POSITION,
                                      XmNrightPosition, 2,
                                      XmNbackground, colors[0xff],
                                      NULL);

        button_cancel = XtVaCreateManagedWidget(langcode("UNIOP00002"),xmPushButtonGadgetClass, form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, sep,
                                      XmNtopOffset, 10,
                                      XmNbottomAttachment, XmATTACH_FORM,
                                      XmNbottomOffset, 5,
                                      XmNleftAttachment, XmATTACH_POSITION,
                                      XmNleftPosition, 3,
                                      XmNrightAttachment, XmATTACH_POSITION,
                                      XmNrightPosition, 4,
                                      XmNbackground, colors[0xff],
                                      NULL);

        XtAddCallback(button_ok, XmNactivateCallback, Database_change_data, config_Database_dialog);
        XtAddCallback(button_cancel, XmNactivateCallback, Database_destroy_shell, config_Database_dialog);

        pos_dialog(config_Database_dialog);

        delw = XmInternAtom(XtDisplay(config_Database_dialog),"WM_DELETE_WINDOW", FALSE);
        XmAddWMProtocolCallback(config_Database_dialog, delw, Database_destroy_shell, (XtPointer)config_Database_dialog);

        if (config_type==0) {
            /* first time port */
            XmToggleButtonSetState(Database_active_on_startup,TRUE,FALSE);
            XmToggleButtonSetState(Database_transmit_data,TRUE,FALSE);
            //XmTextFieldSetString(Database_host_data,"first.aprs.net");
            XmTextFieldSetString(Database_host_data,"");
            XmTextFieldSetString(Database_port_data,"10151");
            XmTextFieldSetString(Database_filter_data,"");
            XmTextFieldSetString(Database_comment,"");
            XmToggleButtonSetState(Database_reconnect_data,FALSE,FALSE);
        } else {
            /* reconfig */

begin_critical_section(&devices_lock, "interface_gui.c:Config_Database" );

           if (devices[Database_port].connect_on_startup)
                XmToggleButtonSetState(Database_active_on_startup,TRUE,FALSE);
            else
                XmToggleButtonSetState(Database_active_on_startup,FALSE,FALSE);

            if (devices[Database_port].transmit_data)
                XmToggleButtonSetState(Database_transmit_data,TRUE,FALSE);
            else
                XmToggleButtonSetState(Database_transmit_data,FALSE,FALSE);

            XmTextFieldSetString(Database_host_data,devices[Database_port].device_host_name);
            xastir_snprintf(temp, sizeof(temp), "%d", devices[Database_port].sp);
            XmTextFieldSetString(Database_port_data,temp);
            XmTextFieldSetString(Database_password_data,devices[Database_port].device_host_pswd);
            XmTextFieldSetString(Database_filter_data,devices[Database_port].device_host_filter_string);
            XmTextFieldSetString(Database_comment,devices[Database_port].comment);

            if (devices[Database_port].reconnect)
                XmToggleButtonSetState(Database_reconnect_data,TRUE,FALSE);
            else
                XmToggleButtonSetState(Database_reconnect_data,FALSE,FALSE);

end_critical_section(&devices_lock, "interface_gui.c:Config_Database" );

        }
        XtManageChild(form);
        XtManageChild(pane);

        XtPopup(config_Database_dialog,XtGrabNone);
        fix_dialog_size(config_Database_dialog);
    } else {
        (void)XRaiseWindow(XtDisplay(config_Database_dialog), XtWindow(config_Database_dialog));
    }
}





/*****************************************************/
/* Configure AGWPE Server GUI                        */
/*****************************************************/

/**** AGWPE CONFIGURE ******/
Widget config_AGWPE_dialog = (Widget)NULL;
Widget AGWPE_active_on_startup;
Widget AGWPE_host_data;
Widget AGWPE_port_data;
Widget AGWPE_comment;
Widget AGWPE_password_data;
Widget AGWPE_transmit_data;
Widget AGWPE_igate_data;
Widget AGWPE_reconnect_data;
Widget AGWPE_unproto1_data;
Widget AGWPE_unproto2_data;
Widget AGWPE_unproto3_data;
Widget AGWPE_relay_digipeat;
Widget AGWPE_radioport_data;
int    AGWPE_port;





void AGWPE_destroy_shell( /*@unused@*/ Widget widget, XtPointer clientData,  /*@unused@*/ XtPointer callData) {
    Widget shell = (Widget) clientData;
    XtPopdown(shell);
    XtDestroyWidget(shell);
    config_AGWPE_dialog = (Widget)NULL;
    if (choose_interface_dialog != NULL)
        Choose_interface_destroy_shell(choose_interface_dialog,choose_interface_dialog,NULL);

    choose_interface_dialog = (Widget)NULL;
}





void AGWPE_change_data(Widget widget, XtPointer clientData, XtPointer callData) {
    int was_up;
    char *temp_ptr;


    busy_cursor(appshell);
    was_up=0;
    if (get_device_status(AGWPE_port) == DEVICE_IN_USE) {
        /* if active shutdown before changes are made */
        /*fprintf(stderr,"Device is up, shutting down\n");*/
        (void)del_device(AGWPE_port);
        was_up=1;
        usleep(1000000); // Wait for one second
    }

begin_critical_section(&devices_lock, "interface_gui.c:AGWPE_change_data" );

    devices[AGWPE_port].igate_options=device_igate_options;

    temp_ptr = XmTextFieldGetString(AGWPE_host_data);
    xastir_snprintf(devices[AGWPE_port].device_host_name,
        sizeof(devices[AGWPE_port].device_host_name),
        "%s",
        temp_ptr);
    XtFree(temp_ptr);

    (void)remove_trailing_spaces(devices[AGWPE_port].device_host_name);

    temp_ptr = XmTextFieldGetString(AGWPE_password_data);
    xastir_snprintf(devices[AGWPE_port].device_host_pswd,
        sizeof(devices[AGWPE_port].device_host_pswd),
        "%s",
        temp_ptr);
    XtFree(temp_ptr);

    (void)remove_trailing_spaces(devices[AGWPE_port].device_host_pswd);

    temp_ptr = XmTextFieldGetString(AGWPE_comment);
    xastir_snprintf(devices[AGWPE_port].comment,
        sizeof(devices[AGWPE_port].comment),
        "%s",
        temp_ptr);
    XtFree(temp_ptr);

    (void)remove_trailing_spaces(devices[AGWPE_port].comment);

    temp_ptr = XmTextFieldGetString(AGWPE_port_data);
    devices[AGWPE_port].sp=atoi(temp_ptr);
    XtFree(temp_ptr);

    temp_ptr = XmTextFieldGetString(AGWPE_unproto1_data);
    xastir_snprintf(devices[AGWPE_port].unproto1,
        sizeof(devices[AGWPE_port].unproto1),
        "%s",
        temp_ptr);
    XtFree(temp_ptr);
    
    (void)remove_trailing_spaces(devices[AGWPE_port].unproto1);

    if(check_unproto_path(devices[AGWPE_port].unproto1)) {
        popup_message_always(langcode("WPUPCFT042"),
            langcode("WPUPCFT043"));
    }

    temp_ptr = XmTextFieldGetString(AGWPE_unproto2_data);
    xastir_snprintf(devices[AGWPE_port].unproto2,
        sizeof(devices[AGWPE_port].unproto2),
        "%s",
        temp_ptr);
    XtFree(temp_ptr);

    (void)remove_trailing_spaces(devices[AGWPE_port].unproto2);

    if(check_unproto_path(devices[AGWPE_port].unproto2)) {
        popup_message_always(langcode("WPUPCFT042"),
            langcode("WPUPCFT043"));
    }

    temp_ptr = XmTextFieldGetString(AGWPE_unproto3_data);
    xastir_snprintf(devices[AGWPE_port].unproto3,
        sizeof(devices[AGWPE_port].unproto3),
        "%s",
        temp_ptr);
    XtFree(temp_ptr);
        
    (void)remove_trailing_spaces(devices[AGWPE_port].unproto3);

    if(check_unproto_path(devices[AGWPE_port].unproto3)) {
        popup_message_always(langcode("WPUPCFT042"),
            langcode("WPUPCFT043"));
    }

    temp_ptr = XmTextFieldGetString(AGWPE_igate_data);
    xastir_snprintf(devices[AGWPE_port].unproto_igate,
        sizeof(devices[AGWPE_port].unproto_igate),
        "%s",
        temp_ptr);
    XtFree(temp_ptr);

    (void)remove_trailing_spaces(devices[AGWPE_port].unproto_igate);

    if(check_unproto_path(devices[AGWPE_port].unproto_igate)) {
        popup_message_always(langcode("WPUPCFT044"),
            langcode("WPUPCFT043"));
    }


    temp_ptr = XmTextFieldGetString(AGWPE_radioport_data);
    xastir_snprintf(devices[AGWPE_port].device_host_filter_string,
        sizeof(devices[AGWPE_port].device_host_filter_string),
        "%s",
        temp_ptr);
    XtFree(temp_ptr);

    (void)remove_trailing_spaces(devices[AGWPE_port].device_host_filter_string);
 
    if(XmToggleButtonGetState(AGWPE_active_on_startup))
        devices[AGWPE_port].connect_on_startup=1;
    else
        devices[AGWPE_port].connect_on_startup=0;

    if(XmToggleButtonGetState(AGWPE_transmit_data))
        devices[AGWPE_port].transmit_data=1;
    else
        devices[AGWPE_port].transmit_data=0;

//WE7U
//    if(XmToggleButtonGetState(AGWPE_relay_digipeat))
//        devices[AGWPE_port].relay_digipeat=1;
//    else
        devices[AGWPE_port].relay_digipeat=0;

    if(XmToggleButtonGetState(AGWPE_reconnect_data))
        devices[AGWPE_port].reconnect=1;
    else
        devices[AGWPE_port].reconnect=0;

//  n8ysz 20041213
//    if (devices[AGWPE_port].connect_on_startup==1 || was_up) {
    if ( was_up) {
        (void)add_device(AGWPE_port,
            DEVICE_NET_AGWPE,
            devices[AGWPE_port].device_host_name,
            devices[AGWPE_port].device_host_pswd,
            devices[AGWPE_port].sp,
            0,
            0,
            devices[AGWPE_port].reconnect,
            "");
    }

    /* delete list */
//    modify_device_list(4,0);


    /* add device type */
    devices[AGWPE_port].device_type=DEVICE_NET_AGWPE;

    /* rebuild list */
//    modify_device_list(3,0);


end_critical_section(&devices_lock, "interface_gui.c:AGWPE_change_data" );

    // Rebuild the interface control list
    update_interface_list();

    AGWPE_destroy_shell(widget,clientData,callData);
}





void Config_AGWPE( /*@unused@*/ Widget w, int config_type, int port) {
    static Widget  pane, form, button_ok, button_cancel,
                ihost, iport, password, password_fl, sep,
                igate, igate_box, igate_o_0, igate_o_1, igate_o_2,
                igate_label, frame, proto, proto1, proto2, proto3,
                radioport_label, comment;
    Atom delw;
    char temp[40];
    Arg al[50];                      // Arg list
    register unsigned int ac = 0;    // Arg Count

    if(!config_AGWPE_dialog) {
        AGWPE_port=port;
        config_AGWPE_dialog = XtVaCreatePopupShell(langcode("WPUPCFIA01"),
            xmDialogShellWidgetClass, appshell,
            XmNdeleteResponse, XmDESTROY,
            XmNdefaultPosition, FALSE,
            NULL);

        pane = XtVaCreateWidget("Config_AGWPE pane",xmPanedWindowWidgetClass, config_AGWPE_dialog,
                          XmNbackground, colors[0xff],
                          NULL);

        form =  XtVaCreateWidget("Config_AGWPE form",xmFormWidgetClass, pane,
                            XmNfractionBase, 5,
                            XmNbackground, colors[0xff],
                            XmNautoUnmanage, FALSE,
                            XmNshadowThickness, 1,
                            NULL);

        AGWPE_active_on_startup  = XtVaCreateManagedWidget(langcode("UNIOP00011"),xmToggleButtonWidgetClass,form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNtopAttachment, XmATTACH_FORM,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset ,10,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        AGWPE_transmit_data  = XtVaCreateManagedWidget(langcode("UNIOP00010"),xmToggleButtonWidgetClass,form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNtopAttachment, XmATTACH_FORM,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, AGWPE_active_on_startup,
                                      XmNleftOffset ,35,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        AGWPE_relay_digipeat = XtVaCreateManagedWidget(langcode("UNIOP00030"),xmToggleButtonWidgetClass,form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNtopAttachment, XmATTACH_FORM,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, AGWPE_transmit_data,
                                      XmNleftOffset ,35,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        ihost = XtVaCreateManagedWidget(langcode("WPUPCFIA02"),xmLabelWidgetClass, form,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, AGWPE_transmit_data,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset, 10,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        AGWPE_host_data = XtVaCreateManagedWidget("Config_AGWPE host_data", xmTextFieldWidgetClass, form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNeditable,   TRUE,
                                      XmNcursorPositionVisible, TRUE,
                                      XmNsensitive, TRUE,
                                      XmNshadowThickness,    1,
                                      XmNcolumns, 25,
                                      XmNwidth, ((25*7)+2),
                                      XmNmaxLength, 30,
                                      XmNbackground, colors[0x0f],
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, AGWPE_transmit_data,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, ihost,
                                      XmNrightAttachment,XmATTACH_NONE,
                                      NULL);

        iport = XtVaCreateManagedWidget(langcode("WPUPCFIA03"),xmLabelWidgetClass, form,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget,AGWPE_transmit_data,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget,AGWPE_host_data,
                                      XmNleftOffset, 10,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        AGWPE_port_data = XtVaCreateManagedWidget("Config_AGWPE port_data", xmTextFieldWidgetClass, form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNeditable,   TRUE,
                                      XmNcursorPositionVisible, TRUE,
                                      XmNsensitive, TRUE,
                                      XmNshadowThickness,    1,
                                      XmNcolumns, 5,
                                      XmNwidth, ((5*7)+2),
                                      XmNmaxLength, 6,
                                      XmNbackground, colors[0x0f],
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, AGWPE_transmit_data,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, iport,
                                      XmNrightAttachment,XmATTACH_NONE,
                                      NULL);

        comment = XtVaCreateManagedWidget(langcode("WPUPCFS017"),xmLabelWidgetClass, form,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, AGWPE_transmit_data,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, AGWPE_port_data,
                                      XmNleftOffset, 10,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        AGWPE_comment = XtVaCreateManagedWidget("Config_AGWPE comment", xmTextFieldWidgetClass, form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNeditable,   TRUE,
                                      XmNcursorPositionVisible, TRUE,
                                      XmNsensitive, TRUE,
                                      XmNshadowThickness,    1,
                                      XmNcolumns, 25,
                                      XmNwidth, ((25*7)+2),
                                      XmNmaxLength, 49,
                                      XmNbackground, colors[0x0f],
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, AGWPE_transmit_data,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, comment,
                                      XmNrightAttachment,XmATTACH_NONE,
                                      NULL);

        password = XtVaCreateManagedWidget(langcode("WPUPCFIA09"),xmLabelWidgetClass, form,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, ihost,
                                      XmNtopOffset, 20,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset, 10,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        AGWPE_password_data = XtVaCreateManagedWidget("Config_AGWPE password_data", xmTextFieldWidgetClass, form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNeditable,   TRUE,
                                      XmNcursorPositionVisible, TRUE,
                                      XmNsensitive, TRUE,
                                      XmNshadowThickness,    1,
                                      XmNcolumns, 20,
                                      XmNmaxLength, 20,
                                      XmNbackground, colors[0x0f],
                                      XmNleftAttachment,XmATTACH_WIDGET,
                                      XmNleftWidget, password,
                                      XmNleftOffset, 10,
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, ihost,
                                      XmNtopOffset, 15,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNrightAttachment,XmATTACH_NONE,
                                      NULL);

        password_fl = XtVaCreateManagedWidget(langcode("WPUPCFIA10"),xmLabelWidgetClass, form,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, ihost,
                                      XmNtopOffset, 20,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget,AGWPE_password_data,
                                      XmNleftOffset,20,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        AGWPE_reconnect_data = XtVaCreateManagedWidget(langcode("WPUPCFIA11"),xmToggleButtonWidgetClass,form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, password,
                                      XmNtopOffset, 20,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset ,10,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        radioport_label = XtVaCreateManagedWidget(langcode("WPUPCFIA15"),xmLabelWidgetClass, form,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, password,
                                      XmNtopOffset, 25,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, AGWPE_reconnect_data,
                                      XmNleftOffset, 50,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        AGWPE_radioport_data = XtVaCreateManagedWidget("Config_AGWPE radioport_data", xmTextFieldWidgetClass, form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNeditable,   TRUE,
                                      XmNcursorPositionVisible, TRUE,
                                      XmNsensitive, TRUE,
                                      XmNshadowThickness,    1,
                                      XmNcolumns, 3,
                                      XmNmaxLength, 3 ,
                                      XmNbackground, colors[0x0f],
                                      XmNleftAttachment,XmATTACH_WIDGET,
                                      XmNleftWidget, radioport_label,
                                      XmNleftOffset, 10,
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, password,
                                      XmNtopOffset, 20,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNrightAttachment,XmATTACH_NONE,
                                      NULL);

        frame = XtVaCreateManagedWidget("Config_AGWPE frame", xmFrameWidgetClass, form,
                                     XmNtopAttachment, XmATTACH_WIDGET,
                                     XmNtopWidget, AGWPE_reconnect_data,
                                     XmNtopOffset, 10,
                                     XmNbottomAttachment, XmATTACH_NONE,
                                     XmNleftAttachment, XmATTACH_FORM,
                                     XmNleftOffset, 10,
                                     XmNrightAttachment, XmATTACH_FORM,
                                     XmNrightOffset, 10,
                                     XmNbackground, colors[0xff],
                                     NULL);

        igate = XtVaCreateManagedWidget(langcode("IGPUPCF000"),xmLabelWidgetClass, frame,
                                    XmNchildType, XmFRAME_TITLE_CHILD,
                                    XmNbackground, colors[0xff],
                                    NULL);

        // Set args for color
        ac=0;
        XtSetArg(al[ac], XmNbackground, colors[0xff]); ac++;


        igate_box = XmCreateRadioBox(frame,"Config_AGWPE IGate box",al,ac);

        XtVaSetValues(igate_box,XmNorientation, XmVERTICAL,XmNnumColumns,2,NULL);

        igate_o_0 = XtVaCreateManagedWidget(langcode("IGPUPCF001"),xmToggleButtonGadgetClass,
                                        igate_box,
                                        XmNbackground, colors[0xff],
                                        NULL);
        XtAddCallback(igate_o_0,XmNvalueChangedCallback,igate_toggle,"0");

        igate_o_1 = XtVaCreateManagedWidget(langcode("IGPUPCF002"),xmToggleButtonGadgetClass,
                                        igate_box,
                                        XmNbackground, colors[0xff],
                                        NULL);
        XtAddCallback(igate_o_1,XmNvalueChangedCallback,igate_toggle,"1");

        igate_o_2 = XtVaCreateManagedWidget(langcode("IGPUPCF003"),xmToggleButtonGadgetClass,
                                        igate_box,
                                        XmNbackground, colors[0xff],
                                        NULL);
        XtAddCallback(igate_o_2,XmNvalueChangedCallback,igate_toggle,"2");

        proto = XtVaCreateManagedWidget(langcode("WPUPCFT011"), xmLabelWidgetClass, form,
                                      XmNorientation, XmHORIZONTAL,
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, frame,
                                      XmNtopOffset, 10,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset, 5,
                                      XmNrightAttachment,XmATTACH_FORM,
                                      XmNrightOffset, 5,
                                      XmNbackground, colors[0xff],
                                      NULL);

        xastir_snprintf(temp, sizeof(temp), langcode("WPUPCFT012"), VERSIONFRM);

        proto1 = XtVaCreateManagedWidget(temp, xmLabelWidgetClass, form,
                                      XmNorientation, XmHORIZONTAL,
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, proto,
                                      XmNtopOffset, 12,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset, 45,
                                      XmNrightAttachment,XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        AGWPE_unproto1_data = XtVaCreateManagedWidget("Config_AGWPE protopath1", xmTextFieldWidgetClass, form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNeditable,   TRUE,
                                      XmNcursorPositionVisible, TRUE,
                                      XmNsensitive, TRUE,
                                      XmNshadowThickness,    1,
                                      XmNcolumns, 25,
                                      XmNwidth, ((25*7)+2),
                                      XmNmaxLength, 40,
                                      XmNbackground, colors[0x0f],
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, proto,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment,XmATTACH_WIDGET,
                                      XmNleftWidget, proto1,
                                      XmNleftOffset, 5,
                                      XmNrightAttachment,XmATTACH_NONE,
                                      NULL);

        xastir_snprintf(temp, sizeof(temp), langcode("WPUPCFT013"), VERSIONFRM);

        proto2 = XtVaCreateManagedWidget(temp, xmLabelWidgetClass, form,
                                      XmNorientation, XmHORIZONTAL,
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, proto1,
                                      XmNtopOffset, 15,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset, 45,
                                      XmNrightAttachment,XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        AGWPE_unproto2_data = XtVaCreateManagedWidget("Config_AGWPE protopath2", xmTextFieldWidgetClass, form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNeditable,   TRUE,
                                      XmNcursorPositionVisible, TRUE,
                                      XmNsensitive, TRUE,
                                      XmNshadowThickness,    1,
                                      XmNcolumns, 25,
                                      XmNwidth, ((25*7)+2),
                                      XmNmaxLength, 40,
                                      XmNbackground, colors[0x0f],
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, AGWPE_unproto1_data,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, proto2,
                                      XmNleftOffset, 5,
                                      XmNrightAttachment,XmATTACH_NONE,
                                      NULL);

        xastir_snprintf(temp, sizeof(temp), langcode("WPUPCFT014"), VERSIONFRM);

        proto3 = XtVaCreateManagedWidget(temp, xmLabelWidgetClass, form,
                                      XmNorientation, XmHORIZONTAL,
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, proto2,
                                      XmNtopOffset, 15,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset, 45,
                                      XmNrightAttachment,XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);


        AGWPE_unproto3_data = XtVaCreateManagedWidget("Config_AGWPE protopath3", xmTextFieldWidgetClass, form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNeditable,   TRUE,
                                      XmNcursorPositionVisible, TRUE,
                                      XmNsensitive, TRUE,
                                      XmNshadowThickness,    1,
                                      XmNcolumns, 25,
                                      XmNwidth, ((25*7)+2),
                                      XmNmaxLength, 40,
                                      XmNbackground, colors[0x0f],
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, AGWPE_unproto2_data,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment,XmATTACH_WIDGET,
                                      XmNleftWidget, proto3,
                                      XmNleftOffset, 5,
                                      XmNrightAttachment,XmATTACH_NONE,
                                      NULL);


        xastir_snprintf(temp, sizeof(temp), langcode("IGPUPCF004"));
        igate_label = XtVaCreateManagedWidget(temp, xmLabelWidgetClass, form,
                                      XmNorientation, XmHORIZONTAL,
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, proto3,
                                      XmNtopOffset, 15,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset, 45,
                                      XmNrightAttachment,XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);

        AGWPE_igate_data = XtVaCreateManagedWidget("Config_AGWPE igate_data", xmTextFieldWidgetClass, form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNeditable,   TRUE,
                                      XmNcursorPositionVisible, TRUE,
                                      XmNsensitive, TRUE,
                                      XmNshadowThickness,    1,
                                      XmNcolumns, 25,
                                      XmNwidth, ((25*7)+2),
                                      XmNmaxLength, 40,
                                      XmNbackground, colors[0x0f],
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, AGWPE_unproto3_data,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment,XmATTACH_WIDGET,
                                      XmNleftWidget, igate_label,
                                      XmNleftOffset, 5,
                                      XmNrightAttachment,XmATTACH_NONE,
                                      NULL);


        sep = XtVaCreateManagedWidget("Config_AGWPE sep", xmSeparatorGadgetClass,form,
                                      XmNorientation, XmHORIZONTAL,
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, AGWPE_igate_data,
                                      XmNtopOffset, 14,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNrightAttachment,XmATTACH_FORM,
                                      XmNbackground, colors[0xff],
                                      NULL);

        button_ok = XtVaCreateManagedWidget(langcode("UNIOP00001"),xmPushButtonGadgetClass, form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, sep,
                                      XmNtopOffset, 10,
                                      XmNbottomAttachment, XmATTACH_FORM,
                                      XmNbottomOffset, 5,
                                      XmNleftAttachment, XmATTACH_POSITION,
                                      XmNleftPosition, 1,
                                      XmNrightAttachment, XmATTACH_POSITION,
                                      XmNrightPosition, 2,
                                      XmNbackground, colors[0xff],
                                      NULL);

        button_cancel = XtVaCreateManagedWidget(langcode("UNIOP00002"),xmPushButtonGadgetClass, form,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, sep,
                                      XmNtopOffset, 10,
                                      XmNbottomAttachment, XmATTACH_FORM,
                                      XmNbottomOffset, 5,
                                      XmNleftAttachment, XmATTACH_POSITION,
                                      XmNleftPosition, 3,
                                      XmNrightAttachment, XmATTACH_POSITION,
                                      XmNrightPosition, 4,
                                      XmNbackground, colors[0xff],
                                      NULL);

        XtAddCallback(button_ok, XmNactivateCallback, AGWPE_change_data, config_AGWPE_dialog);
        XtAddCallback(button_cancel, XmNactivateCallback, AGWPE_destroy_shell, config_AGWPE_dialog);

        pos_dialog(config_AGWPE_dialog);

        delw = XmInternAtom(XtDisplay(config_AGWPE_dialog),"WM_DELETE_WINDOW", FALSE);
        XmAddWMProtocolCallback(config_AGWPE_dialog, delw, AGWPE_destroy_shell, (XtPointer)config_AGWPE_dialog);

        if (config_type==0) {
            /* first time port */
            XmToggleButtonSetState(AGWPE_active_on_startup,TRUE,FALSE);
            XmToggleButtonSetState(AGWPE_transmit_data,TRUE,FALSE);
            //XmTextFieldSetString(AGWPE_host_data,"first.aprs.net");
            XmTextFieldSetString(AGWPE_host_data,"localhost");
            XmTextFieldSetString(AGWPE_port_data,"8000");
            XmTextFieldSetString(AGWPE_comment,"");
            XmToggleButtonSetState(AGWPE_reconnect_data,FALSE,FALSE);
            XmToggleButtonSetState(AGWPE_relay_digipeat, FALSE, FALSE);
            device_igate_options=0;
            XmToggleButtonSetState(igate_o_0,TRUE,FALSE);
            XmTextFieldSetString(AGWPE_unproto1_data,"WIDE2-2");
            XmTextFieldSetString(AGWPE_unproto2_data,"");
            XmTextFieldSetString(AGWPE_unproto3_data,"");
            XmTextFieldSetString(AGWPE_igate_data,"");
            XmTextFieldSetString(AGWPE_radioport_data,"1");

//WE7U
// Keep this statement until we get relay digipeating functional for
// this interface.
XtSetSensitive(AGWPE_relay_digipeat, FALSE);

        }
        else {
            /* reconfig */

begin_critical_section(&devices_lock, "interface_gui.c:Config_AGWPE" );

           if (devices[AGWPE_port].connect_on_startup)
                XmToggleButtonSetState(AGWPE_active_on_startup,TRUE,FALSE);
            else
                XmToggleButtonSetState(AGWPE_active_on_startup,FALSE,FALSE);

            if (devices[AGWPE_port].transmit_data)
                XmToggleButtonSetState(AGWPE_transmit_data,TRUE,FALSE);
            else
                XmToggleButtonSetState(AGWPE_transmit_data,FALSE,FALSE);

//            if (devices[AGWPE_port].relay_digipeat)
//                XmToggleButtonSetState(AGWPE_relay_digipeat, TRUE, FALSE);
//            else
                XmToggleButtonSetState(AGWPE_relay_digipeat, FALSE, FALSE);

//            if (devices[AGWPE_port].transmit_data) {
//                XtSetSensitive(AGWPE_relay_digipeat, TRUE);
//            }
//            else
                XtSetSensitive(AGWPE_relay_digipeat, FALSE);

            XmTextFieldSetString(AGWPE_host_data,devices[AGWPE_port].device_host_name);
            xastir_snprintf(temp, sizeof(temp), "%d", devices[AGWPE_port].sp);
            XmTextFieldSetString(AGWPE_port_data,temp);
            XmTextFieldSetString(AGWPE_password_data,devices[AGWPE_port].device_host_pswd);
            XmTextFieldSetString(AGWPE_comment,devices[AGWPE_port].comment);

            if (devices[AGWPE_port].reconnect)
                XmToggleButtonSetState(AGWPE_reconnect_data,TRUE,FALSE);
            else
                XmToggleButtonSetState(AGWPE_reconnect_data,FALSE,FALSE);

            XmTextFieldSetString(AGWPE_radioport_data,devices[AGWPE_port].device_host_filter_string);
 
            switch (devices[AGWPE_port].igate_options) {
                case(0):
                    XmToggleButtonSetState(igate_o_0,TRUE,FALSE);
                    device_igate_options=0;
                    break;

                case(1):
                    XmToggleButtonSetState(igate_o_1,TRUE,FALSE);
                    device_igate_options=1;
                    break;

                case(2):
                    XmToggleButtonSetState(igate_o_2,TRUE,FALSE);
                    device_igate_options=2;
                    break;

                default:
                    XmToggleButtonSetState(igate_o_0,TRUE,FALSE);
                    device_igate_options=0;
                    break;
            }

            XmTextFieldSetString(AGWPE_unproto1_data,devices[AGWPE_port].unproto1);
            XmTextFieldSetString(AGWPE_unproto2_data,devices[AGWPE_port].unproto2);
            XmTextFieldSetString(AGWPE_unproto3_data,devices[AGWPE_port].unproto3);
            XmTextFieldSetString(AGWPE_igate_data,devices[AGWPE_port].unproto_igate);

end_critical_section(&devices_lock, "interface_gui.c:Config_AGWPE" );

        }
        XtManageChild(igate_box);
        XtManageChild(form);
        XtManageChild(pane);

        XtPopup(config_AGWPE_dialog,XtGrabNone);
        fix_dialog_size(config_AGWPE_dialog);
    } else {
        (void)XRaiseWindow(XtDisplay(config_AGWPE_dialog), XtWindow(config_AGWPE_dialog));
    }
}





/*****************************************************/
/* Configure Interface GUI                           */
/*****************************************************/

int are_shells_up(void) {
    int up;

    up=1;
    if (config_TNC_dialog) {
        (void)XRaiseWindow(XtDisplay(config_TNC_dialog), XtWindow(config_TNC_dialog));
    } else {
        if (config_GPS_dialog) {
            (void)XRaiseWindow(XtDisplay(config_GPS_dialog), XtWindow(config_GPS_dialog));
        } else {
            if (config_WX_dialog) {
                (void)XRaiseWindow(XtDisplay(config_WX_dialog), XtWindow(config_WX_dialog));
            } else {
                if (config_NGPS_dialog) {
                    (void)XRaiseWindow(XtDisplay(config_NGPS_dialog), XtWindow(config_NGPS_dialog));
                } else {
                    if (config_AX25_dialog) {
                        (void)XRaiseWindow(XtDisplay(config_AX25_dialog), XtWindow(config_AX25_dialog));
                    } else {
                        if (config_Inet_dialog) {
                            (void)XRaiseWindow(XtDisplay(config_Inet_dialog), XtWindow(config_Inet_dialog));
                        } else {
                            if (config_NWX_dialog) {
                                (void)XRaiseWindow(XtDisplay(config_NWX_dialog), XtWindow(config_NWX_dialog));
                            } else {
                                if (config_Database_dialog) {
                                    (void)XRaiseWindow(XtDisplay(config_Database_dialog), XtWindow(config_Database_dialog));
                                } else {
                                    if (config_AGWPE_dialog) {
                                        (void)XRaiseWindow(XtDisplay(config_AGWPE_dialog), XtWindow(config_AGWPE_dialog));
                                    } else up=0;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return(up);
}





void Choose_interface_destroy_shell( /*@unused@*/ Widget widget, XtPointer clientData,  /*@unused@*/ XtPointer callData) {
    Widget shell = (Widget) clientData;
    if (are_shells_up()==0) {
        XtPopdown(shell);
        XtDestroyWidget(shell);
        choose_interface_dialog = (Widget)NULL;
    }
}





void modify_device_list(int option, int port) {
    int i,n;
    char temp[150];
    char temp2[150];
    XmString str_ptr;


    n=1;
    for (i=0; i < MAX_IFACE_DEVICES; i++) {
        if (devices[i].device_type!=DEVICE_NONE) {
            switch (option) {
                case 0 :
                    /* delete entire list available */
                    XmListDeleteAllItems(control_iface_list);
                    return; // Exit routine
                    break;

                case 1 :
                    /* delete item pointed to by port */
                    if (i==port)
                        XmListDeletePos(control_iface_list,n);
                    n++;
                    break;

                case 2 :
                    /* create item list */
                    /* format list for device modify*/
                    switch (devices[i].device_type) {
                        case DEVICE_SERIAL_TNC:
                        case DEVICE_SERIAL_TNC_HSP_GPS:
                        case DEVICE_SERIAL_TNC_AUX_GPS:
                        case DEVICE_SERIAL_KISS_TNC:
                        case DEVICE_SERIAL_GPS:
                        case DEVICE_SERIAL_WX:
                            xastir_snprintf(temp,
                                sizeof(temp),
                                langcode("IFDIN00000"),
                                langcode("UNIOP00006"),
                                i,
                                dtype[devices[i].device_type].device_name,
                                devices[i].device_name,
                                devices[i].comment);
                            strncat(temp,
                                "    ",
                                sizeof(temp) - strlen(temp));
                            break;

                        case DEVICE_SERIAL_MKISS_TNC:
                            xastir_snprintf(temp,
                                sizeof(temp),
                                langcode("IFDIN00001"),
                                langcode("UNIOP00006"),
                                i,
                                dtype[devices[i].device_type].device_name,
                                devices[i].device_name,
                                atoi(devices[i].radio_port),
                                devices[i].comment);
                            strncat(temp,
                                "    ",
                                sizeof(temp) - strlen(temp));
                            break;

                        case DEVICE_NET_DATABASE:
                        case DEVICE_NET_STREAM:
                        case DEVICE_NET_GPSD:
                        case DEVICE_NET_WX:
                        case DEVICE_NET_AGWPE:
                            xastir_snprintf(temp,
                                sizeof(temp),
                                langcode("IFDIN00001"),
                                langcode("UNIOP00006"),
                                i,
                                dtype[devices[i].device_type].device_name,
                                devices[i].device_host_name,
                                devices[i].sp,
                                devices[i].comment);
                            strncat(temp,
                                "    ",
                                sizeof(temp) - strlen(temp));
                            break;

                        case DEVICE_AX25_TNC:
                            xastir_snprintf(temp,
                                sizeof(temp),
                                langcode("IFDIN00002"),
                                langcode("UNIOP00006"),
                                i,
                                dtype[devices[i].device_type].device_name,
                                devices[i].device_name,
                                devices[i].comment);
                            strncat(temp,
                                "    ",
                                sizeof(temp) - strlen(temp));
                            break;

                        default:
                            break;
                    }
                    /* look at list data (Must be "Device" port#) */
                    XmListAddItem(control_iface_list, str_ptr = XmStringCreateLtoR(temp,XmFONTLIST_DEFAULT_TAG),n++);
 
                    XmStringFree(str_ptr);
                    break;

                case 3 :
                    /* create item list */
                    /* format list for device control*/
                    if (port_data[i].active==DEVICE_IN_USE) {
                        switch (port_data[i].status) {
                            case DEVICE_DOWN:
                                xastir_snprintf(temp2,
                                    sizeof(temp2),
                                    "%s",
                                    langcode("IFDIN00006"));
                                break;

                            case DEVICE_UP:
                                xastir_snprintf(temp2,
                                    sizeof(temp2),
                                    "%s",
                                    langcode("IFDIN00007"));
                                break;

                            case DEVICE_ERROR:
                                xastir_snprintf(temp2,
                                    sizeof(temp2),
                                    "%s",
                                    langcode("IFDIN00008"));
                                break;

                            default:
                                xastir_snprintf(temp2,
                                    sizeof(temp2),
                                    "%s",
                                    langcode("IFDIN00009"));
                                break;
                        }
                    } else {
                        xastir_snprintf(temp2,
                            sizeof(temp2),
                            "%s",
                            langcode("IFDIN00006"));
                    }
                    switch (devices[i].device_type) {
                        case DEVICE_SERIAL_TNC:
                        case DEVICE_SERIAL_KISS_TNC:
                        case DEVICE_SERIAL_TNC_HSP_GPS:
                        case DEVICE_SERIAL_TNC_AUX_GPS:
                        case DEVICE_SERIAL_GPS:
                        case DEVICE_SERIAL_WX:
                            xastir_snprintf(temp,
                                sizeof(temp),
                                langcode("IFDIN00003"),
                                langcode("UNIOP00006"),
                                i,
                                temp2,
                                dtype[devices[i].device_type].device_name,
                                devices[i].device_name,
                                devices[i].comment);
                            strncat(temp,
                                "    ",
                                sizeof(temp) - strlen(temp));
                            break;

                        case DEVICE_SERIAL_MKISS_TNC:
                            xastir_snprintf(temp,
                                sizeof(temp),
                                langcode("IFDIN00004"),
                                langcode("UNIOP00006"),
                                i,
                                temp2,
                                dtype[devices[i].device_type].device_name,
                                devices[i].device_name,
                                atoi(devices[i].radio_port),
                                devices[i].comment);
                            strncat(temp,
                                "    ",
                                sizeof(temp) - strlen(temp));
                            break;

                        case DEVICE_NET_DATABASE:
                        case DEVICE_NET_STREAM:
                        case DEVICE_NET_GPSD:
                        case DEVICE_NET_WX:
                        case DEVICE_NET_AGWPE:
                            xastir_snprintf(temp,
                                sizeof(temp),
                                langcode("IFDIN00004"),
                                langcode("UNIOP00006"),
                                i,
                                temp2,
                                dtype[devices[i].device_type].device_name,
                                devices[i].device_host_name,
                                devices[i].sp,
                                devices[i].comment);
                            strncat(temp,
                                "    ",
                                sizeof(temp) - strlen(temp));
                            break;

                        case DEVICE_AX25_TNC:
                            xastir_snprintf(temp,
                                sizeof(temp),
                                langcode("IFDIN00005"),
                                langcode("UNIOP00006"),
                                i,
                                temp2,
                                dtype[devices[i].device_type].device_name,
                                devices[i].device_name,
                                devices[i].comment);
                            strncat(temp,
                                "    ",
                                sizeof(temp) - strlen(temp));
                            break;

                        default:
                            break;
                    }
                    /* look at list data (Must be "Device" port#) */
                    XmListAddItem(control_iface_list, str_ptr = XmStringCreateLtoR(temp,XmFONTLIST_DEFAULT_TAG),n++);
                    XmStringFree(str_ptr);
                    break;

                case 4 :
                    /* delete entire list available */
                    XmListDeleteAllItems(control_iface_list);
                    return; // Exit routine
                    break;

                default:
                    break;
            }
        }
    }
}





// Rebuild the list in the interface control dialog so that the
// current status of each interface is shown.
//
void update_interface_list(void) {

    // If the interface control dialog exists
    if (control_interface_dialog) {

        // Delete the entire list
        modify_device_list(4,0);

        // Create the list again with updated values
        modify_device_list(3,0);
    }
}





void interface_setup(Widget w, XtPointer clientData,  /*@unused@*/ XtPointer callData) {
    char *what = (char *)clientData;
    int x,i,do_w;
    char *temp;
    /*char temp2[100];*/
    XmString *list;
    int port;
    int found;

    port=-1;
    found=0;
    do_w=atoi(what);

    /* get option selected */
    XtVaGetValues(interface_type_list,
               XmNitemCount,&i,
               XmNitems,&list,
               NULL);

    for (x=1; x<=i;x++) {
        if (XmListPosSelected(interface_type_list,x)) {
            found=x;
            if (XmStringGetLtoR(list[(x-1)],XmFONTLIST_DEFAULT_TAG,&temp))
                x=i+1;
        }
    }

    /* if selection was made */
    if (found) {
        if (do_w==0) {  // Add an interface
            /* add */
            /*fprintf(stderr,"ADD DEVICE\n");*/

            /* delete list */

begin_critical_section(&devices_lock, "interface_gui.c:interface_setup" );
            modify_device_list(0,0);
end_critical_section(&devices_lock, "interface_gui.c:interface_setup" );

            port=get_open_device();     // Find an unused port number
            /*fprintf(stderr,"Open_port %d\n",port);*/
            if(port!=-1) {
                /*devices[port].device_type=found;*/
                /*fprintf(stderr,"adding device %s on port %d\n",dtype[found].device_name,port);*/
                switch (found) {

//WE7U:  Set up for new KISS device type
                    case DEVICE_SERIAL_KISS_TNC:
                        // configure this port
                        if (debug_level & 1)
                            fprintf(stderr,"ADD SERIAL KISS TNC\n");
                        Config_TNC(w, DEVICE_SERIAL_KISS_TNC, 0, port);
                        break;

                    case DEVICE_SERIAL_MKISS_TNC:
                        // configure this port
                        if (debug_level & 1)
                            fprintf(stderr,"ADD SERIAL MKISS TNC\n");
                        Config_TNC(w, DEVICE_SERIAL_MKISS_TNC, 0, port);
                        break;

                    case DEVICE_SERIAL_TNC:
                        /* configure this port */
                        if (debug_level & 1)
                            fprintf(stderr,"ADD SERIAL TNC\n");
                        Config_TNC(w, DEVICE_SERIAL_TNC, 0, port);
                        break;

                    case DEVICE_SERIAL_TNC_HSP_GPS:
                        /* configure this port */
                        if (debug_level & 1)
                            fprintf(stderr,"ADD SERIAL TNC w HSP GPS\n");
                        Config_TNC(w, DEVICE_SERIAL_TNC_HSP_GPS, 0, port);
                        break;

                    case DEVICE_SERIAL_TNC_AUX_GPS:
                        /* configure this port */
                        if (debug_level & 1)
                            fprintf(stderr,"ADD SERIAL TNC w AUX GPS\n");
                        Config_TNC(w, DEVICE_SERIAL_TNC_AUX_GPS, 0, port);
                        break;

                    case DEVICE_SERIAL_GPS:
                        /* configure this port */
                        if (debug_level & 1)
                            fprintf(stderr,"ADD SERIAL GPS\n");
                        Config_GPS(w, 0, port);
                        break;

                    case DEVICE_SERIAL_WX:
                        /* configure this port */
                        if (debug_level & 1)
                            fprintf(stderr,"ADD SERIAL WX\n");
                        Config_WX(w, 0, port);
                        break;

                    case DEVICE_NET_WX:
                        /* configure this port */
                        if (debug_level & 1)
                            fprintf(stderr,"ADD Network WX\n");
                        Config_NWX(w, 0, port);
                        break;

                    case DEVICE_NET_GPSD:
                        /* configure this port */
                        if (debug_level & 1)
                            fprintf(stderr,"ADD Network GPS\n");
                        Config_NGPS(w, 0, port);
                        break;

                    case DEVICE_AX25_TNC:
                        /* configure this port */
                        if (debug_level & 1)
#ifdef HAVE_LIBAX25
                            fprintf(stderr,"ADD AX.25 TNC\n");
                        Config_AX25(w, 0, port);
#else   // HAVE_LIBAX25
                        fprintf(stderr,"AX.25 support not compiled into Xastir!\n");
                        popup_message(langcode("POPEM00004"),langcode("POPEM00021"));

#endif  // HAVE_LIBAX25
                        break;

                    case DEVICE_NET_STREAM:
                        /* configure this port */
                        if (debug_level & 1)
                            fprintf(stderr,"ADD NET STREAM\n");
                        Config_Inet(w, 0, port);
                        break;

                    case DEVICE_NET_DATABASE:
                        /* configure this port */
                        if (debug_level & 1)
                            fprintf(stderr,"ADD NET DATABASE\n");
                        Config_Database(w, 0, port);
                        break;

                    case DEVICE_NET_AGWPE:
                        /* configure this port */
                        if (debug_level & 1)
                            fprintf(stderr,"ADD NET AGWPE\n");
                        Config_AGWPE(w, 0, port);
                        break;

                    default:
                        break;
                }
            }

            /* rebuild list */

begin_critical_section(&devices_lock, "interface_gui.c:interface_setup" );
            modify_device_list(2,0);
end_critical_section(&devices_lock, "interface_gui.c:interface_setup" );

        }
        /*fprintf(stderr,"SELECTION is %s\n",temp);*/
        XtFree(temp);
    }
}





// clientData:
//      0 = Add
//      1 = Delete
//      2 = Properties
//
void interface_option(Widget w, XtPointer clientData,  /*@unused@*/ XtPointer callData) {
    Widget pane, form, label, button_add, button_cancel;
    char *what = (char *)clientData;
    int i,x,n,do_w;
    char *temp;
    char temp2[50];
    int port;
    XmString *list;
    int data_on,pos;
    int found;
    Atom delw;
    XmString str_ptr;
    Arg al[50];                    /* Arg List */
    register unsigned int ac = 0;           /* Arg Count */

    data_on=0;
    pos=0;
    found=0;
    do_w=atoi(what);
    switch (do_w) {
        case 0:/* add interface */
            if (!choose_interface_dialog) {
                choose_interface_dialog = XtVaCreatePopupShell(langcode("WPUPCIF002"),
                    xmDialogShellWidgetClass, appshell,
                    XmNdeleteResponse, XmDESTROY,
                    XmNdefaultPosition, FALSE,
                    XmNresize, FALSE,
                    NULL);

                pane = XtVaCreateWidget("interface_option pane",xmPanedWindowWidgetClass, choose_interface_dialog,
                    XmNbackground, colors[0xff],
                    NULL);

                form =  XtVaCreateWidget("interface_option form",xmFormWidgetClass, pane,
                        XmNfractionBase, 5,
                        XmNbackground, colors[0xff],
                        XmNautoUnmanage, FALSE,
                        XmNshadowThickness, 1,
                        NULL);

                label = XtVaCreateManagedWidget(langcode("WPUPCIF002"),xmLabelWidgetClass, form,
                                XmNtopAttachment, XmATTACH_FORM,
                                XmNtopOffset, 10,
                                XmNbottomAttachment, XmATTACH_NONE,
                                XmNleftAttachment, XmATTACH_FORM,
                                XmNleftOffset, 5,
                                XmNrightAttachment, XmATTACH_NONE,
                                XmNbackground, colors[0xff],
                                NULL);

                /*set args for color */
                ac=0;
                XtSetArg(al[ac], XmNbackground, colors[0xff]); ac++;
                XtSetArg(al[ac], XmNvisibleItemCount, MAX_IFACE_DEVICE_TYPES); ac++;
                XtSetArg(al[ac], XmNtraversalOn, TRUE); ac++;
                XtSetArg(al[ac], XmNshadowThickness, 3); ac++;
                XtSetArg(al[ac], XmNselectionPolicy, XmSINGLE_SELECT); ac++;
                XtSetArg(al[ac], XmNscrollBarPlacement, XmBOTTOM_RIGHT); ac++;
                XtSetArg(al[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
                XtSetArg(al[ac], XmNtopWidget, label); ac++;
                XtSetArg(al[ac], XmNtopOffset, 5); ac++;
                XtSetArg(al[ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
                XtSetArg(al[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
                XtSetArg(al[ac], XmNrightOffset, 5); ac++;
                XtSetArg(al[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
                XtSetArg(al[ac], XmNleftOffset, 5); ac++;

                interface_type_list = XmCreateScrolledList(form,"interface_option list",al,ac);
                n=1;
                for (i=1; i<MAX_IFACE_DEVICE_TYPES; i++) {
                    XmListAddItem(interface_type_list, str_ptr = XmStringCreateLtoR(dtype[i].device_name,XmFONTLIST_DEFAULT_TAG),n++);
                    XmStringFree(str_ptr);
                }
                button_add = XtVaCreateManagedWidget(langcode("UNIOP00007"),xmPushButtonGadgetClass, form,
                                XmNnavigationType, XmTAB_GROUP,
                                XmNtraversalOn, TRUE,
                                XmNtopAttachment, XmATTACH_WIDGET,
                                XmNtopWidget, XtParent(interface_type_list),
                                XmNtopOffset,10,
                                XmNbottomAttachment, XmATTACH_FORM,
                                XmNbottomOffset, 5,
                                XmNleftAttachment, XmATTACH_POSITION,
                                XmNleftPosition, 1,
                                XmNrightAttachment, XmATTACH_POSITION,
                                XmNrightPosition, 2,
                                XmNbackground, colors[0xff],
                                NULL);

                button_cancel = XtVaCreateManagedWidget(langcode("UNIOP00003"),xmPushButtonGadgetClass, form,
                                XmNnavigationType, XmTAB_GROUP,
                                XmNtraversalOn, TRUE,
                                XmNtopAttachment, XmATTACH_WIDGET,
                                XmNtopWidget, XtParent(interface_type_list),
                                XmNtopOffset,10,
                                XmNbottomAttachment, XmATTACH_FORM,
                                XmNbottomOffset, 5,
                                XmNleftAttachment, XmATTACH_POSITION,
                                XmNleftPosition, 3,
                                XmNrightAttachment, XmATTACH_POSITION,
                                XmNrightPosition, 4,
                                XmNbackground, colors[0xff],
                                NULL);

                XtAddCallback(button_cancel, XmNactivateCallback, Choose_interface_destroy_shell, choose_interface_dialog);
                XtAddCallback(button_add, XmNactivateCallback, interface_setup, "0");

                pos_dialog(choose_interface_dialog);

                delw = XmInternAtom(XtDisplay(choose_interface_dialog),"WM_DELETE_WINDOW", FALSE);
                XmAddWMProtocolCallback(choose_interface_dialog, delw, Choose_interface_destroy_shell, (XtPointer)configure_interface_dialog);

                XtManageChild(form);
                XtManageChild(interface_type_list);
                XtVaSetValues(interface_type_list, XmNbackground, colors[0x0f], NULL);
                XtManageChild(pane);

                XtPopup(choose_interface_dialog,XtGrabNone);
                fix_dialog_size(choose_interface_dialog);

                // Move focus to the Cancel button.  This appears to highlight the
                // button fine, but we're not able to hit the <Enter> key to
                // have that default function happen.  Note:  We _can_ hit the
                // <SPACE> key, and that activates the option.
//                XmUpdateDisplay(choose_interface_dialog);
                XmProcessTraversal(button_cancel, XmTRAVERSE_CURRENT);

           } else {
                (void)XRaiseWindow(XtDisplay(choose_interface_dialog), XtWindow(choose_interface_dialog));
            }
            break;

        case 1:/* delete interface */

        case 2:/* interface properties */
            /* get option selected */
            XtVaGetValues(control_iface_list,
                    XmNitemCount,&i,
                    XmNitems,&list,
                    NULL);

            for (x=1; x<=i;x++) {
                if(XmListPosSelected(control_iface_list,x)) {
                    found=1;
                    if (XmStringGetLtoR(list[(x-1)],XmFONTLIST_DEFAULT_TAG,&temp))
                        x=i+1;
                }
            }

            /* if selection was made */
            if (found) {
                /* look at list data (Must be "Device" port#) */
                if (2 != sscanf(temp,"%49s %d",temp2,&port)) {
                    fprintf(stderr,"interface_option:sscanf parsing error\n");
                }
                if(do_w==1) {
                    /* delete interface */
                    /*fprintf(stderr,"delete interface port %d\n",port);*/

                    if (port_data[port].active==DEVICE_IN_USE) {
                        /* shut down and delete port */
                        /*fprintf(stderr,"Shutting down port %d\n",port);*/
                        (void)del_device(port);
                    }

begin_critical_section(&devices_lock, "interface_gui.c:interface_option" );

                    /* delete item at that port */
                    modify_device_list(1,port);
                    /* Clear device */
                    devices[port].device_type=DEVICE_NONE;
                    devices[port].device_name[0] = '\0';
                    devices[port].radio_port[0] = '\0';
                    devices[port].device_host_name[0] = '\0';
                    devices[port].device_host_pswd[0] = '\0';
                    devices[port].device_host_filter_string[0] = '\0';
                    devices[port].comment[0] = '\0';
                    devices[port].unproto1[0] = '\0';
                    devices[port].unproto2[0] = '\0';
                    devices[port].unproto3[0] = '\0';
                    devices[port].unproto_igate[0] = '\0';
                    devices[port].style=0;
                    devices[port].igate_options=0;
                    devices[port].transmit_data=0;
                    devices[port].reconnect=0;
                    devices[port].connect_on_startup=0;

end_critical_section(&devices_lock, "interface_gui.c:interface_option" );

                } else {
                    /* Properties */

begin_critical_section(&devices_lock, "interface_gui.c:interface_option" );

                    if (debug_level & 1) {
                        fprintf(stderr,"Changing device  %s on port %d\n",
                            dtype[devices[port].device_type].device_name,port);
                    }
                    switch (devices[port].device_type) {
                        case DEVICE_SERIAL_TNC:

end_critical_section(&devices_lock, "interface_gui.c:interface_option" );

                            /* configure this port */
                            if (debug_level & 1)
                                fprintf(stderr,"Modify SERIAL TNC\n");
                            Config_TNC(w, DEVICE_SERIAL_TNC, 1, port);
                            break;

                        case DEVICE_SERIAL_KISS_TNC:

end_critical_section(&devices_lock, "interface_gui.c:interface_option" );

                            /* configure this port */
                            if (debug_level & 1)
                                fprintf(stderr,"Modify SERIAL KISS TNC\n");
                            Config_TNC(w, DEVICE_SERIAL_KISS_TNC, 1, port);
                            break;

                        case DEVICE_SERIAL_MKISS_TNC:

end_critical_section(&devices_lock, "interface_gui.c:interface_option" );

                            /* configure this port */
                            if (debug_level & 1)
                                fprintf(stderr,"Modify SERIAL MKISS TNC\n");
                            Config_TNC(w, DEVICE_SERIAL_MKISS_TNC, 1, port);
                            break;

                        case DEVICE_SERIAL_TNC_HSP_GPS:
 
end_critical_section(&devices_lock, "interface_gui.c:interface_option" );

                            /* configure this port */
                            if (debug_level & 1)
                                fprintf(stderr,"Modify SERIAL TNC with HSP GPS\n");
                            Config_TNC(w, DEVICE_SERIAL_TNC_HSP_GPS, 1, port);
                            break;

                        case DEVICE_SERIAL_TNC_AUX_GPS:

end_critical_section(&devices_lock, "interface_gui.c:interface_option" );

                            /* configure this port */
                            if (debug_level & 1)
                                fprintf(stderr,"Modify SERIAL TNC with AUX GPS\n");
                            Config_TNC(w, DEVICE_SERIAL_TNC_AUX_GPS, 1, port);
                            break;

                        case DEVICE_SERIAL_GPS:
 
end_critical_section(&devices_lock, "interface_gui.c:interface_option" );

                            /* configure this port */
                            if (debug_level & 1)
                                fprintf(stderr,"Modify SERIAL GPS\n");
                            Config_GPS(w, 1, port);
                            break;

                        case DEVICE_SERIAL_WX:
 
end_critical_section(&devices_lock, "interface_gui.c:interface_option" );

                            /* configure this port */
                            if (debug_level & 1)
                                fprintf(stderr,"Modify SERIAL WX\n");
                            Config_WX(w, 1, port);
                            break;

                        case DEVICE_NET_WX:
 
end_critical_section(&devices_lock, "interface_gui.c:interface_option" );

                            /* configure this port */
                            if (debug_level & 1)
                                fprintf(stderr,"Modify Network WX\n");
                            Config_NWX(w, 1, port);
                            break;

                        case DEVICE_NET_GPSD:
 
end_critical_section(&devices_lock, "interface_gui.c:interface_option" );

                            /* configure this port */
                            if (debug_level & 1)
                                fprintf(stderr,"Modify Network GPS\n");
                            Config_NGPS(w, 1, port);
                            break;

                        case DEVICE_AX25_TNC:
 
end_critical_section(&devices_lock, "interface_gui.c:interface_option" );

                            /* configure this port */
                            if (debug_level & 1)
                                fprintf(stderr,"Modify AX.25 TNC\n");
                            Config_AX25(w, 1, port);
                            break;

                        case DEVICE_NET_STREAM:
 
end_critical_section(&devices_lock, "interface_gui.c:interface_option" );

                            /* configure this port */
                            if (debug_level & 1)
                                fprintf(stderr,"Modify NET STREAM\n");
                            Config_Inet(w, 1, port);
                            break;

                        case DEVICE_NET_DATABASE:
 
end_critical_section(&devices_lock, "interface_gui.c:interface_option" );

                            /* configure this port */
                            if (debug_level & 1)
                                fprintf(stderr,"Modify NET DATABASE\n");
                            Config_Database(w, 1, port);
                            break;

                        case DEVICE_NET_AGWPE:
 
end_critical_section(&devices_lock, "interface_gui.c:interface_option" );

                            /* configure this port */
                            if (debug_level & 1)
                                fprintf(stderr,"Modify NET AGWPE\n");
                            Config_AGWPE(w, 1, port);
                            break;

                        default:
 
end_critical_section(&devices_lock, "interface_gui.c:interface_option" );

                            break;
                    }
                }
                /*fprintf(stderr,"interface - %s\n",temp);*/
                XtFree(temp);
            }
            break;

        default:
            break;
    }
}





/*****************************************************/
/* Control Interface GUI                           */
/*****************************************************/
extern void startup_all_or_defined_port(int port);
extern void shutdown_all_active_or_defined_port(int port);





void start_stop_interface( /*@unused@*/ Widget widget, XtPointer clientData,  /*@unused@*/ XtPointer callData) {
    char *which = (char *)clientData;
    int do_w;
    char temp2[50];
    int i,x;
    char *temp;
    int port;
    XmString *list;
    int found;

    busy_cursor(appshell);

    found=0;
    /* get option selected */
    XtVaGetValues(control_iface_list,
               XmNitemCount,&i,
               XmNitems,&list,
               NULL);

    for (x=1; x<=i;x++) {
        if (XmListPosSelected(control_iface_list,x)) {
            found=1;
            if (XmStringGetLtoR(list[(x-1)],XmFONTLIST_DEFAULT_TAG,&temp))
                x=i+1;
        }
    }

    /* if selection was made */
    if (found) {

        /* delete list */

//begin_critical_section(&devices_lock, "interface_gui.c:start_stop_interface" );
//        modify_device_list(4,0);
//end_critical_section(&devices_lock, "interface_gui.c:start_stop_interface" );

        /* look at list data (Must be "Device" port#) */
        if (2 != sscanf(temp,"%49s %d",temp2,&port)) {
            fprintf(stderr,"start_stop_interface:sscanf parsing error\n");
        }
        /*fprintf(stderr,"Port to change %d\n",port);*/
        do_w = atoi(which);
        if (do_w) {
            shutdown_all_active_or_defined_port(port);
        } else {
            /*fprintf(stderr,"DO port up\n");*/
            if (port_data[port].active==DEVICE_IN_USE) {
                /*fprintf(stderr,"Device was up, Shutting down\n");*/
                shutdown_all_active_or_defined_port(port);
            }
            /* now start port */
            startup_all_or_defined_port(port);
        }
        /* rebuild list */

//begin_critical_section(&devices_lock, "interface_gui.c:start_stop_interface" );
//        modify_device_list(3,0);
//end_critical_section(&devices_lock, "interface_gui.c:start_stop_interface" );

        // Rebuild the interface control list
        update_interface_list();

        XtFree(temp);
    }
}





void start_stop_all_interfaces( /*@unused@*/ Widget widget, XtPointer clientData,  /*@unused@*/ XtPointer callData) {
    char *which = (char *)clientData;   // Whether to start or stop the interfaces
    int do_w;

    busy_cursor(appshell);

//begin_critical_section(&devices_lock, "interface_gui.c:start_stop_all_interfaces" );
//    modify_device_list(4,0);
//end_critical_section(&devices_lock, "interface_gui.c:start_stop_all_interfaces" );

    do_w = atoi(which);
    if (do_w) {     // We wish to shut down all ports
        shutdown_all_active_or_defined_port(-1);
    }
    else {        // We wish to start up all ports
        startup_all_or_defined_port(-2);
    }
    /* rebuild list */

//begin_critical_section(&devices_lock, "interface_gui.c:start_stop_all_interfaces" );
//    modify_device_list(3,0);
//end_critical_section(&devices_lock, "interface_gui.c:start_stop_all_interfaces" );

    // Rebuild the interface control list
    update_interface_list();

}





void Control_interface_destroy_shell( /*@unused@*/ Widget widget, XtPointer clientData,  /*@unused@*/ XtPointer callData) {
    Widget shell = (Widget) clientData;
    XtPopdown(shell);

begin_critical_section(&control_interface_dialog_lock, "interface_gui.c:Control_interface_destroy_shell" );

    XtDestroyWidget(shell);
    control_interface_dialog = (Widget)NULL;

end_critical_section(&control_interface_dialog_lock, "interface_gui.c:Control_interface_destroy_shell" );

}





void control_interface( /*@unused@*/ Widget w,  /*@unused@*/ XtPointer clientData,  /*@unused@*/ XtPointer callData) {
    static Widget rowcol, form, button_start, button_stop, button_start_all, button_stop_all, button_cancel;
    static Widget button_add, button_delete, button_properties;
    Atom delw;
    Arg al[50];                    /* Arg List */
    register unsigned int ac = 0;           /* Arg Count */


    if(!control_interface_dialog) {

begin_critical_section(&control_interface_dialog_lock, "interface_gui.c:control_interface" );

        control_interface_dialog = XtVaCreatePopupShell(langcode("IFPUPCT000"),
            xmDialogShellWidgetClass, appshell,
            XmNdeleteResponse, XmDESTROY,
            XmNdefaultPosition, FALSE,
            XmNresize, TRUE,
            NULL);

        rowcol =  XtVaCreateWidget("control_interface rowcol",xmRowColumnWidgetClass, control_interface_dialog,
                            XmNorientation, XmVERTICAL,
                            XmNnumColumns, 1,
                            XmNpacking, XmPACK_TIGHT,
                            XmNisAligned, TRUE,
                            XmNentryAlignment, XmALIGNMENT_CENTER,
                            XmNkeyboardFocusPolicy, XmEXPLICIT,
                            XmNbackground, colors[0xff],
                            XmNautoUnmanage, FALSE,
                            XmNshadowThickness, 1,
                            NULL);
 
        /*set args for color */
        ac=0;
        XtSetArg(al[ac], XmNbackground, colors[0xff]); ac++;
        XtSetArg(al[ac], XmNvisibleItemCount, MAX_IFACE_DEVICES); ac++;
        XtSetArg(al[ac], XmNtraversalOn, TRUE); ac++;
        XtSetArg(al[ac], XmNshadowThickness, 3); ac++;
        XtSetArg(al[ac], XmNselectionPolicy, XmSINGLE_SELECT); ac++;
        XtSetArg(al[ac], XmNscrollBarPlacement, XmBOTTOM_RIGHT); ac++;
        XtSetArg(al[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
        XtSetArg(al[ac], XmNtopOffset, 5); ac++;
        XtSetArg(al[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
        XtSetArg(al[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
        XtSetArg(al[ac], XmNrightOffset, 5); ac++;
        XtSetArg(al[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
        XtSetArg(al[ac], XmNleftOffset, 5); ac++;
        control_iface_list = XmCreateScrolledList(rowcol,"control_interface list",al,ac);

        /* build device list */

begin_critical_section(&devices_lock, "interface_gui.c:control_interface" );
        modify_device_list(3,0);
end_critical_section(&devices_lock, "interface_gui.c:control_interface" );

        form =  XtVaCreateWidget("control_interface form",xmFormWidgetClass, rowcol,
                    XmNfractionBase, 4,
                    XmNbackground, colors[0xff],
                    XmNautoUnmanage, FALSE,
                    XmNshadowThickness, 1,
                    NULL);

        button_start = XtVaCreateManagedWidget(langcode("IFPUPCT001"),xmPushButtonGadgetClass, form,
                            XmNnavigationType, XmTAB_GROUP,
                            XmNtraversalOn, TRUE,
                            XmNtopAttachment, XmATTACH_FORM,
                            XmNtopOffset, 5,
                            XmNleftAttachment, XmATTACH_FORM,
                            XmNrightAttachment, XmATTACH_POSITION,
                            XmNrightPosition, 1,
                            XmNbottomAttachment, XmATTACH_NONE,
                            XmNbackground, colors[0xff],
                            XmNnavigationType, XmTAB_GROUP,
                            NULL);

        button_start_all = XtVaCreateManagedWidget(langcode("IFPUPCT003"),xmPushButtonGadgetClass, form,
                            XmNnavigationType, XmTAB_GROUP,
                            XmNtraversalOn, TRUE,
                            XmNtopAttachment, XmATTACH_FORM,
                            XmNtopOffset, 5,
                            XmNleftAttachment, XmATTACH_POSITION,
                            XmNleftPosition, 1,
                            XmNrightAttachment, XmATTACH_POSITION,
                            XmNrightPosition, 2,
                            XmNbottomAttachment, XmATTACH_NONE,
                            XmNbackground, colors[0xff],
                            XmNnavigationType, XmTAB_GROUP,
                            NULL);

        button_add = XtVaCreateManagedWidget(langcode("UNIOP00007"),xmPushButtonGadgetClass, form,
                            XmNnavigationType, XmTAB_GROUP,
                            XmNtraversalOn, TRUE,
                            XmNtopAttachment, XmATTACH_FORM,
                            XmNtopOffset,5,
                            XmNbottomAttachment, XmATTACH_NONE,
                            XmNleftAttachment, XmATTACH_POSITION,
                            XmNleftPosition, 2,
                            XmNrightAttachment, XmATTACH_POSITION,
                            XmNrightPosition, 3,
                            XmNbackground, colors[0xff],
                            NULL);

        button_delete = XtVaCreateManagedWidget(langcode("UNIOP00008"),xmPushButtonGadgetClass, form,
                            XmNnavigationType, XmTAB_GROUP,
                            XmNtraversalOn, TRUE,
                            XmNtopAttachment, XmATTACH_FORM,
                            XmNtopOffset,5,
                            XmNbottomAttachment, XmATTACH_NONE,
                            XmNleftAttachment, XmATTACH_POSITION,
                            XmNleftPosition, 3,
                            XmNrightAttachment, XmATTACH_POSITION,
                            XmNrightPosition, 4,
                            XmNbackground, colors[0xff],
                            NULL);

        button_stop = XtVaCreateManagedWidget(langcode("IFPUPCT002"),xmPushButtonGadgetClass, form,
                            XmNnavigationType, XmTAB_GROUP,
                            XmNtraversalOn, TRUE,
                            XmNtopAttachment, XmATTACH_WIDGET,
                            XmNtopWidget, button_start,
                            XmNleftAttachment, XmATTACH_POSITION,
                            XmNleftPosition, 0,
                            XmNrightAttachment, XmATTACH_POSITION,
                            XmNrightPosition, 1,
                            XmNbottomAttachment, XmATTACH_FORM,
                            XmNbackground, colors[0xff],
                            XmNnavigationType, XmTAB_GROUP,
                            NULL);

        button_stop_all = XtVaCreateManagedWidget(langcode("IFPUPCT004"),xmPushButtonGadgetClass, form,
                            XmNnavigationType, XmTAB_GROUP,
                            XmNtraversalOn, TRUE,
                            XmNtopAttachment, XmATTACH_WIDGET,
                            XmNtopWidget, button_start,
                            XmNleftAttachment, XmATTACH_POSITION,
                            XmNleftPosition, 1,
                            XmNrightAttachment, XmATTACH_POSITION,
                            XmNrightPosition, 2,
                            XmNbottomAttachment, XmATTACH_FORM,
                            XmNbackground, colors[0xff],
                            XmNnavigationType, XmTAB_GROUP,
                            NULL);

        button_properties = XtVaCreateManagedWidget(langcode("UNIOP00009"),xmPushButtonGadgetClass, form,
                            XmNnavigationType, XmTAB_GROUP,
                            XmNtraversalOn, TRUE,
                            XmNtopAttachment, XmATTACH_WIDGET,
                            XmNtopWidget, button_start,
                            XmNbottomAttachment, XmATTACH_FORM,
                            XmNleftAttachment, XmATTACH_POSITION,
                            XmNleftPosition, 2,
                            XmNrightAttachment, XmATTACH_POSITION,
                            XmNrightPosition, 3,
                            XmNbackground, colors[0xff],
                            NULL);

        button_cancel = XtVaCreateManagedWidget(langcode("UNIOP00003"),xmPushButtonGadgetClass, form,
                            XmNnavigationType, XmTAB_GROUP,
                            XmNtraversalOn, TRUE,
                            XmNtopAttachment, XmATTACH_WIDGET,
                            XmNtopWidget, button_start,
                            XmNrightAttachment, XmATTACH_FORM,
                            XmNleftAttachment, XmATTACH_POSITION,
                            XmNleftPosition, 3,
                            XmNbottomAttachment, XmATTACH_FORM,
                            XmNbackground, colors[0xff],
                            XmNnavigationType, XmTAB_GROUP,
                            NULL);

        XtAddCallback(button_add, XmNactivateCallback, interface_option, "0");
        XtAddCallback(button_delete, XmNactivateCallback, interface_option, "1");
        XtAddCallback(button_properties, XmNactivateCallback, interface_option, "2");

        XtAddCallback(button_cancel, XmNactivateCallback, Control_interface_destroy_shell, control_interface_dialog);
        XtAddCallback(button_start, XmNactivateCallback, start_stop_interface, "0");
        XtAddCallback(button_stop, XmNactivateCallback, start_stop_interface, "1");

        XtAddCallback(button_start_all, XmNactivateCallback, start_stop_all_interfaces, "0");
        XtAddCallback(button_stop_all, XmNactivateCallback, start_stop_all_interfaces, "1");

        delw = XmInternAtom(XtDisplay(control_interface_dialog),"WM_DELETE_WINDOW", FALSE);
        XmAddWMProtocolCallback(control_interface_dialog, delw, Control_interface_destroy_shell, (XtPointer)control_interface_dialog);

        XtVaSetValues(control_iface_list, XmNbackground, colors[0x0f], NULL);
 
        pos_dialog(control_interface_dialog);

        XtManageChild(control_iface_list);
        XtManageChild(form);
        XtManageChild(rowcol);

end_critical_section(&control_interface_dialog_lock, "interface_gui.c:control_interface" );

        XtPopup(control_interface_dialog,XtGrabNone);
        fix_dialog_size(control_interface_dialog);

        // Move focus to the Cancel button.  This appears to highlight the
        // button fine, but we're not able to hit the <Enter> key to
        // have that default function happen.  Note:  We _can_ hit the
        // <SPACE> key, and that activates the option.
//        XmUpdateDisplay(control_interface_dialog);
        XmProcessTraversal(button_cancel, XmTRAVERSE_CURRENT);

   } else {
        (void)XRaiseWindow(XtDisplay(control_interface_dialog), XtWindow(control_interface_dialog));
    }
}





void interface_status(Widget w) {
    int i;
    char s;
    char opt;
    int read_data;
    int write_data;

    read_data=0;
    write_data=0;
    s='\0';

begin_critical_section(&devices_lock, "interface_gui.c:interface_status" );

    for (i=0; i < MAX_IFACE_DEVICES; i++) {
        read_data=0;
        write_data=0;
        opt='\0';

        if (devices[i].device_type!=DEVICE_NONE) {
            switch(devices[i].device_type) {
                case DEVICE_SERIAL_TNC:
                    s='0';  // Select icon for status bar
                    break;

                case DEVICE_SERIAL_TNC_HSP_GPS:
                    s='1';  // Select icon for status bar
                    break;

                case DEVICE_SERIAL_GPS:
                    s='2';  // Select icon for status bar
                    break;

                case DEVICE_SERIAL_WX:
                case DEVICE_NET_WX:
                    s='3';  // Select icon for status bar
                    break;

                case DEVICE_NET_DATABASE:
                case DEVICE_NET_STREAM:
                case DEVICE_NET_AGWPE:
                    s='4';  // Select icon for status bar
                    break;

                case DEVICE_AX25_TNC:
                case DEVICE_SERIAL_KISS_TNC:
                case DEVICE_SERIAL_MKISS_TNC:
                    s='5';  // Select icon for status bar
                    break;

                case DEVICE_NET_GPSD:
                    s='6';  // Select icon for status bar
                    break;

                case DEVICE_SERIAL_TNC_AUX_GPS:
                    s='7';  // Select icon for status bar
                    break;

                default:
                    break;
            }
            if (port_data[i].active==DEVICE_IN_USE) {
                if (port_data[i].status==DEVICE_UP) {
                    if (port_data[i].bytes_input_last != port_data[i].bytes_input) {
if (begin_critical_section(&port_data_lock, "interface_gui.c:interface_status(1)" ) > 0)
    fprintf(stderr,"port_data_lock, Port = %d\n", i);
                        port_data[i].bytes_input_last = port_data[i].bytes_input;
                        port_data[i].port_activity = 1;
if (end_critical_section(&port_data_lock, "interface_gui.c:interface_status(2)" ) > 0)
    fprintf(stderr,"port_data_lock, Port = %d\n", i);
                        read_data=1;
                    }
                    if (port_data[i].bytes_output_last != port_data[i].bytes_output) {
if (begin_critical_section(&port_data_lock, "interface_gui.c:interface_status(3)" ) > 0)
    fprintf(stderr,"port_data_lock, Port = %d\n", i);
                        port_data[i].bytes_output_last = port_data[i].bytes_output;
                        port_data[i].port_activity = 1;
if (end_critical_section(&port_data_lock, "interface_gui.c:interface_status(4)" ) > 0)
    fprintf(stderr,"port_data_lock, Port = %d\n", i);
                        write_data=1;
                    }
                    if (write_data)
                        opt='>';
                    else {
                        if (read_data)
                            opt='<';
                        else
                            opt='^';
                    }
                } else
                    opt='*';
            } else
                opt='\0';
            symbol(w,0,'~',s,opt,XtWindow(iface_da),0,(i*10),0,' ');
        } else
            symbol(w,0,'~','#','\0',XtWindow(iface_da),0,(i*10),0,' ');
    }

end_critical_section(&devices_lock, "interface_gui.c:interface_option" );

}


