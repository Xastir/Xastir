/*
 * $Id$
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000,2001,2002  The Xastir Group
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
#endif

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <Xm/XmAll.h>

#include "main.h"
#include "xastir.h"
#include "popup.h"
#include "main.h"
#include "lang.h"
#include "rotated.h"
#include "snprintf.h"


static Popup_Window pw[MAX_POPUPS];
static Popup_Window pwb;

static xastir_mutex popup_message_dialog_lock;





void popup_gui_init(void)
{
    init_critical_section( &popup_message_dialog_lock );
}





/**** Popup Message ******/

void clear_popup_message_windows(void) {
    int i;

begin_critical_section(&popup_message_dialog_lock, "popup_gui.c:clear_popup_message_windows" );

    for (i=0;i<MAX_POPUPS;i++) {
        pw[i].popup_message_dialog=(Widget)NULL;
        pw[i].popup_message_data=(Widget)NULL;
    }

end_critical_section(&popup_message_dialog_lock, "popup_gui.c:clear_popup_message_windows" );

    pwb.popup_message_dialog=(Widget)NULL;
    pwb.popup_message_data=(Widget)NULL;
}





static void popup_message_destroy_shell(/*@unused@*/ Widget w,
                                XtPointer clientData,
                                /*@unused@*/ XtPointer callData) {
    int i;

    i=atoi((char *)clientData);
    XtPopdown(pw[i].popup_message_dialog);

begin_critical_section(&popup_message_dialog_lock, "popup_gui.c:popup_message_destroy_shell" );

    XtDestroyWidget(pw[i].popup_message_dialog);
    pw[i].popup_message_dialog = (Widget)NULL;
    pw[i].popup_message_data = (Widget)NULL;

end_critical_section(&popup_message_dialog_lock, "popup_gui.c:popup_message_destroy_shell" );

}





void popup_time_out_check(void) {
    int i;

    for (i=0;i<MAX_POPUPS;i++) {
        if (pw[i].popup_message_dialog!=NULL) {
            if ((sec_now()-pw[i].sec_opened)>MAX_POPUPS_TIME) {
                XtPopdown(pw[i].popup_message_dialog);

begin_critical_section(&popup_message_dialog_lock, "popup_gui.c:popup_time_out_check" );

                XtDestroyWidget(pw[i].popup_message_dialog);
                pw[i].popup_message_dialog = (Widget)NULL;
                pw[i].popup_message_data = (Widget)NULL;

end_critical_section(&popup_message_dialog_lock, "popup_gui.c:popup_time_out_check" );

            }
        }
    }
}





void popup_message(char *banner, char *message) {
    XmString msg_str;
    int j,i;
    Atom delw;

    i=0;
    for (j=0; j<MAX_POPUPS; j++) {
        if (!pw[j].popup_message_dialog) {
            i=j;
            j=MAX_POPUPS+1;
        }
    }

    if(!pw[i].popup_message_dialog) {
        if (banner!=NULL && message!=NULL) {

begin_critical_section(&popup_message_dialog_lock, "popup_gui.c:popup_message" );

            pw[i].popup_message_dialog = XtVaCreatePopupShell(banner,xmDialogShellWidgetClass,Global.top,
                                  XmNdeleteResponse,XmDESTROY,
                                  XmNdefaultPosition, FALSE,
                                  XmNtitleString,banner,
                                  NULL);

            pw[i].pane = XtVaCreateWidget("popup_message pane",xmPanedWindowWidgetClass, pw[i].popup_message_dialog,
                          XmNbackground, colors[0xff],
                          NULL);

            pw[i].form =  XtVaCreateWidget("popup_message form",xmFormWidgetClass, pw[i].pane,
                            XmNfractionBase, 5,
                            XmNbackground, colors[0xff],
                            XmNautoUnmanage, FALSE,
                            XmNshadowThickness, 1,
                            NULL);

            pw[i].popup_message_data = XtVaCreateManagedWidget("popup_message message",xmLabelWidgetClass, pw[i].form,
                                      XmNtopAttachment, XmATTACH_FORM,
                                      XmNtopOffset, 10,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset, 10,
                                      XmNrightAttachment, XmATTACH_FORM,
                                      XmNrightOffset, 10,
                                      XmNbackground, colors[0xff],
                                      NULL);

            pw[i].button_close = XtVaCreateManagedWidget(langcode("UNIOP00003"),xmPushButtonGadgetClass, pw[i].form,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, pw[i].popup_message_data,
                                      XmNtopOffset, 10,
                                      XmNbottomAttachment, XmATTACH_FORM,
                                      XmNbottomOffset, 5,
                                      XmNleftAttachment, XmATTACH_POSITION,
                                      XmNleftPosition, 2,
                                      XmNrightAttachment, XmATTACH_POSITION,
                                      XmNrightPosition, 3,
                                      XmNbackground, colors[0xff],
                                      NULL);

            sprintf(pw[i].name,"%d",i);

            XtAddCallback(pw[i].button_close, XmNactivateCallback, popup_message_destroy_shell,(XtPointer)pw[i].name);

            pos_dialog(pw[i].popup_message_dialog);

            delw = XmInternAtom(XtDisplay(pw[i].popup_message_dialog),"WM_DELETE_WINDOW", FALSE);

            XmAddWMProtocolCallback(pw[i].popup_message_dialog, delw, popup_message_destroy_shell, (XtPointer)pw[i].name);

            msg_str=XmStringCreateLtoR(message,XmFONTLIST_DEFAULT_TAG);
            XtVaSetValues(pw[i].popup_message_data,XmNlabelString,msg_str,NULL);
            XmStringFree(msg_str);

            XtManageChild(pw[i].form);
            XtManageChild(pw[i].pane);

end_critical_section(&popup_message_dialog_lock, "popup_gui.c:popup_message" );

            XtPopup(pw[i].popup_message_dialog,XtGrabNone);
            fix_dialog_size(pw[i].popup_message_dialog);
            pw[i].sec_opened=sec_now();
        }
    }
}





// Routine which pops up a large message for a few seconds in the
// middle of the screen, then removes it.
//
void popup_ID_message(char *banner, char *message) {
    char *fontname=
        //"-adobe-helvetica-medium-o-normal--24-240-75-75-p-130-iso8859-1";
        //"-adobe-helvetica-medium-o-normal--12-120-75-75-p-67-iso8859-1";
        //"-adobe-helvetica-medium-r-*-*-*-130-*-*-*-*-*-*";
        //"-*-times-bold-r-*-*-13-*-*-*-*-80-*-*";
        //"-*-helvetica-bold-r-*-*-14-*-*-*-*-80-*-*";
        //"-*-helvetica-bold-r-*-*-12-*-*-*-*-*-*-*";
        "-*-helvetica-*-*-*-*-*-240-100-100-*-*-*-*";
        //"-*-fixed-*-r-*-*-*-*-*-*-*-200-*-*";
    static XFontStruct *font=NULL;
    float my_rotation = 0.0;
    int x = (int)(screen_width/10);
    int y = (int)(screen_height/2);


    if (ATV_screen_ID) {

        // Fill the pixmap with grey so that the black ID text will
        // be seen.
        (void)XSetForeground(XtDisplay(da),gc,MY_BG_COLOR); // Not a mistake!
        (void)XSetBackground(XtDisplay(da),gc,MY_BG_COLOR);
        (void)XFillRectangle(XtDisplay(appshell),pixmap_alerts,gc,0,0,screen_width,screen_height);

        /* load font */
        if(!font)
            font=(XFontStruct *)XLoadQueryFont (XtDisplay(da), fontname);

        (void)XSetForeground (XtDisplay(da), gc, colors[0x08]);

        //printf("%0.1f\t%s\n",my_rotation,label_text);

        if (       ( (my_rotation < -90.0) && (my_rotation > -270.0) )
                || ( (my_rotation >  90.0) && (my_rotation <  270.0) ) ) {
            my_rotation = my_rotation + 180.0;
            (void)XRotDrawAlignedString(XtDisplay(da),
                font,
                my_rotation,
                pixmap_alerts,
                gc,
                x,
                y,
                message,
                BRIGHT);
        }
        else {
            (void)XRotDrawAlignedString(XtDisplay(da),
                font,
                my_rotation,
                pixmap_alerts,
                gc,
                x,
                y,
                message,
                BLEFT);
        }

        // Schedule a screen update in roughly 3 seconds
        remove_ID_message_time = sec_now() + 3;
        pending_ID_message = 1;

        // Write it to the screen.  Symbols/tracks will disappear during
        // this short interval time.
        (void)XCopyArea(XtDisplay(da),pixmap_alerts,XtWindow(da),gc,0,0,screen_width,screen_height,0,0);
    }
    else {  // ATV Screen ID is not enabled
        pending_ID_message = 0;
    }

#ifdef HAVE_FESTIVAL
    if (ATV_speak_ID) {
        SayText(message);
    }
#endif

}


