/*
 * $Id$
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2003  The Xastir Group
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <Xm/XmAll.h>

#include "main.h"
#include "xastir.h"
#include "xa_config.h"
#include "db.h"
#include "util.h"
#include "lang.h"


Widget location_dialog = (Widget)NULL;
Widget location_list;

static xastir_mutex location_dialog_lock;





void location_gui_init(void)
{
    init_critical_section( &location_dialog_lock );
}





/************************************************/
/* button fuction for last location             */
/************************************************/
void Last_location(/*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer callData) {
    map_pos_last_position();
}





/************************************************/
/* manage jump locations                        */
/************************************************/
void location_destroy_shell(/*@unused@*/ Widget widget, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    Widget shell = (Widget) clientData;
    XtPopdown(shell);

begin_critical_section(&location_dialog_lock, "location_gui.c:location_destroy_shell" );

    XtDestroyWidget(shell);
    location_dialog = (Widget)NULL;

end_critical_section(&location_dialog_lock, "location_gui.c:location_destroy_shell" );

}





/************************************************/
/* jump to chosen location/zoom                 */
/************************************************/
void location_view(/*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer callData) {
    int i,x;
    char *location;
    XmString *list;
    int found,done;
    FILE *f;
    char temp[200];
    char name[100];
    char pos[100];
    char *temp_ptr;
    char s_lat[20];
    char s_long[20];
    char s_sz[10];

    found=0;
    XtVaGetValues(location_list,XmNitemCount,&i,XmNitems,&list,NULL);

    for (x=1; x<=i;x++) {
        if (XmListPosSelected(location_list,x)) {
            found=1;
            if (XmStringGetLtoR(list[(x-1)],XmFONTLIST_DEFAULT_TAG,&location))
                x=i+1;
        }
    }

    if (found) {
        f=fopen(get_user_base_dir("config/locations.sys"),"r");
        if (f!=NULL) {
            done=0;
            while (!feof(f) & !done) {
                (void)get_line(f,temp,200);
                if (!feof(f) && strlen(temp)>8) {
                    temp_ptr=strtok(temp,"|");  /* get the name */
                    if (temp_ptr!=NULL) {
                        strncpy(name,temp,100);
                        temp_ptr=strtok(NULL,"|");  /* get the pos */
                        strncpy(pos,temp_ptr,100);
                        if (strcmp(location,name)==0) {
                            (void)sscanf(pos,"%19s %19s %9s", s_lat, s_long, s_sz);
                            map_pos(convert_lat_s2l(s_lat),convert_lon_s2l(s_long),atol(s_sz));
                            done=1;
                        }
                    }
                }
            }
            (void)fclose(f);
        }
        else {
            fprintf(stderr,"Couldn't open file: %s\n", get_user_base_dir("config/locations.sys") );
        }
        XtFree(location);
    }
}





/************************************************/
/* sort and jump locations                      */
/************************************************/
void jump_sort(void) {
    char temp[200];
    char name[100];
    char *temp_ptr;
    FILE *f;

    f=fopen(get_user_base_dir("config/locations.sys"),"r");
    if (f!=NULL) {
        while (!feof(f)) {
            (void)get_line(f,temp,200);
            if (!feof(f) && strlen(temp)>8) {
                temp_ptr=strtok(temp,"|");  /* get the name */
                if (temp_ptr!=NULL) {
                    strncpy(name,temp,100);
                    (void)sort_input_database(get_user_base_dir("data/locations_db.dat"),name,200);
                }
            }
        }
        (void)fclose(f);
    }
    else
        fprintf(stderr,"Couldn't open file: %s\n", get_user_base_dir("config/locations.sys") );
}





/************************************************/
/* delete location/zoom                         */
/************************************************/
void location_delete(/*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer callData) {
    int i,x;
    char *location;
    XmString *list;
    int found,ok;
    FILE *f,*fout;
    char temp[200];
    char name[100];
    char pos[100];
    char *temp_ptr;
    char filen[400];
    char filen_bak[400];

    found=0;
    ok=0;
    XtVaGetValues(location_list,XmNitemCount,&i,XmNitems,&list,NULL);

    for (x=1; x<=i;x++) {
        if (XmListPosSelected(location_list,x)) {
            found=1;
            if (XmStringGetLtoR(list[(x-1)],XmFONTLIST_DEFAULT_TAG,&location)) {
                XmListDeletePos(location_list,x);
                x=i+1;
            }
        }
    }
    if(found) {
        f=fopen(get_user_base_dir("config/locations.sys"),"r");
        if (f!=NULL) {
            fout=fopen(get_user_base_dir("config/locations.sys-tmp"),"a");
            if (fout!=NULL) {
                while (!feof(f)) {
                    (void)get_line(f,temp,200);
                    if (!feof(f) && strlen(temp)>8) {
                        temp_ptr=strtok(temp,"|");  /* get the name */
                        if (temp_ptr!=NULL) {
                            strncpy(name,temp,100);
                            temp_ptr=strtok(NULL,"|");  /* get the pos */
                            strncpy(pos,temp_ptr,100);
                            if (strcmp(location,name)!=0) {
                                fprintf(fout,"%s|%s\n",name,pos);
                            }
                        }
                    }
                }
                (void)fclose(fout);
                ok=1;
            }
            else
                fprintf(stderr,"Couldn't open file: %s\n", get_user_base_dir("config/locations.sys-tmp") );

            (void)fclose(f);
        }
        else {
            fprintf(stderr,"Couldn't open file: %s\n", get_user_base_dir("config/locations.sys") );
        }
        XtFree(location);
    }

    if (ok==1){
        strcpy(filen,get_user_base_dir("config/locations.sys"));
        strcpy(filen_bak,get_user_base_dir("config/locations.sys-tmp"));
        (void)unlink(filen);
        (void)rename(filen_bak,filen);
    }

}





/************************************************/
/* add location/zoom                            */
/************************************************/
void location_add(/*@unused@*/ Widget w, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    char name[100];
    char s_long[20];
    char s_lat[20];
    FILE *f, *fout;
    char temp[200];
    char *temp_ptr;
    Widget my_text = (Widget) clientData;
    int len,n,found;

    strcpy(name,XmTextFieldGetString(my_text));
    (void)remove_trailing_spaces(name);
    XmTextFieldSetString(my_text,"");
    /* should check for name used already */
    found=0;
    f=fopen(get_user_base_dir("config/locations.sys"),"r");
    if (f!=NULL) {
        while (!feof(f) && !found) {
            (void)get_line(f,temp,200);
            if (!feof(f) && strlen(temp)>8) {
                temp_ptr=strtok(temp,"|");  /* get the name */
                if (temp_ptr!=NULL) {
                    if (strcmp(name,temp)==0)
                        found=1;
                }
            }
        }
        (void)fclose(f);
    }
    else
        fprintf(stderr,"Couldn't open file: %s\n", get_user_base_dir("config/locations.sys") );

    if (!found) {
        /* delete entire list available */
        XmListDeleteAllItems(location_list);
        len = (int)strlen(name);
        if (len>0 && len<100){
            fout = fopen(get_user_base_dir("config/locations.sys"),"a");
            if (fout!=NULL) {
                convert_lat_l2s(mid_y_lat_offset, s_lat, sizeof(s_lat), CONVERT_HP_NOSP);
                //strcpy(s_lat+2,s_lat+3);
                convert_lon_l2s(mid_x_long_offset, s_long, sizeof(s_long), CONVERT_HP_NOSP);
                //strcpy(s_long+3,s_long+4);
                fprintf(fout,"%s|%s %s %ld\n",name,s_lat,s_long,scale_y);
                (void)fclose(fout);
            }
            else
                fprintf(stderr,"Couldn't open file: %s\n", get_user_base_dir("config/locations.sys") );
        } else
            popup_message(langcode("POPEM00022"),langcode("POPEM00023"));

        /* resort the list and put it back up */
        n=1;
        clear_sort_file(get_user_base_dir("data/locations_db.dat"));
        jump_sort();
        sort_list(get_user_base_dir("data/locations_db.dat"),200,location_list,&n);
    } else
        popup_message(langcode("POPEM00022"),langcode("POPEM00024")); /* dupe name */
}





/************************************************/
/* manage jump locations                        */
/************************************************/
void Jump_location(/*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer callData) {
    static Widget  pane,form, button_ok, button_add, button_delete, button_cancel, locdata, location_name;
    int n;
    Arg al[20];           /* Arg List */
    unsigned int ac = 0;           /* Arg Count */
    Atom delw;

    if(!location_dialog) {

begin_critical_section(&location_dialog_lock, "location_gui.c:Jump_location" );

        location_dialog = XtVaCreatePopupShell(langcode("JMLPO00001"),
                xmDialogShellWidgetClass,
                appshell,
                XmNdeleteResponse,XmDESTROY,
                XmNdefaultPosition, FALSE,
                XmNresize, FALSE,
                NULL);

        pane = XtVaCreateWidget("Jump_location pane",
                xmPanedWindowWidgetClass,
                location_dialog,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        form =  XtVaCreateWidget("Jump_location form",
                xmFormWidgetClass,
                pane,
                XmNfractionBase, 5,
                XmNautoUnmanage, FALSE,
                XmNshadowThickness, 1,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        /*set args for color */
        ac=0;
        XtSetArg(al[ac], XmNvisibleItemCount, 11); ac++;
        XtSetArg(al[ac], XmNtraversalOn, TRUE); ac++;
        XtSetArg(al[ac], XmNshadowThickness, 3); ac++;
        XtSetArg(al[ac], XmNbackground, colors[0x0ff]); ac++;
        XtSetArg(al[ac], XmNselectionPolicy, XmSINGLE_SELECT); ac++;
        XtSetArg(al[ac], XmNscrollBarPlacement, XmBOTTOM_RIGHT); ac++;
        XtSetArg(al[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
        XtSetArg(al[ac], XmNtopOffset, 5); ac++;
        XtSetArg(al[ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
        XtSetArg(al[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
        XtSetArg(al[ac], XmNrightOffset, 5); ac++;
        XtSetArg(al[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
        XtSetArg(al[ac], XmNleftOffset, 5); ac++;
        XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
        XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
 
        location_list = XmCreateScrolledList(form,
                "Jump_location list",
                al,
                ac);

        n=1;
        clear_sort_file(get_user_base_dir("data/locations_db.dat"));
        jump_sort();
        sort_list(get_user_base_dir("data/locations_db.dat"),200,location_list,&n);

        locdata = XtVaCreateManagedWidget(langcode("JMLPO00003"),
                xmLabelWidgetClass, 
                form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, XtParent(location_list),
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 5,
                XmNrightAttachment, XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        location_name = XtVaCreateManagedWidget("Jump_location Location_name", 
                xmTextFieldWidgetClass, 
                form,
                XmNeditable,   TRUE,
                XmNcursorPositionVisible, TRUE,
                XmNsensitive, TRUE,
                XmNshadowThickness,      1,
                XmNcolumns,21,
                XmNwidth,((21*7)+2),
                XmNbackground, colors[0x0f],
                XmNtopAttachment,XmATTACH_WIDGET,
                XmNtopWidget, XtParent(location_list),
                XmNtopOffset, 5,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget,locdata,
                XmNrightAttachment,XmATTACH_FORM,
                XmNrightOffset, 5,
                NULL);

        button_ok = XtVaCreateManagedWidget(langcode("JMLPO00002"),
                xmPushButtonGadgetClass, 
                form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, locdata,
                XmNtopOffset,15,
                XmNbottomAttachment, XmATTACH_FORM,
                XmNbottomOffset,5,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 0,
                XmNleftOffset, 3,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 1,
                XmNnavigationType, XmTAB_GROUP,
                NULL);

        button_add = XtVaCreateManagedWidget(langcode("UNIOP00007"),
                xmPushButtonGadgetClass, 
                form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, locdata,
                XmNtopOffset,15,
                XmNbottomAttachment, XmATTACH_FORM,
                XmNbottomOffset,5,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 1,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 2,
                XmNnavigationType, XmTAB_GROUP,
                NULL);

        button_delete = XtVaCreateManagedWidget(langcode("UNIOP00008"),
                xmPushButtonGadgetClass, 
                form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, locdata,
                XmNtopOffset,15,
                XmNbottomAttachment, XmATTACH_FORM,
                XmNbottomOffset,5,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 2,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 3,
                XmNnavigationType, XmTAB_GROUP,
                NULL);

        button_cancel = XtVaCreateManagedWidget(langcode("UNIOP00003"),
                xmPushButtonGadgetClass, 
                form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, locdata,
                XmNtopOffset,15,
                XmNbottomAttachment, XmATTACH_FORM,
                XmNbottomOffset,5,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 4,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 5,
                XmNrightOffset, 3,
                XmNnavigationType, XmTAB_GROUP,
                NULL);

        XtAddCallback(button_cancel, XmNactivateCallback, location_destroy_shell, location_dialog);
        XtAddCallback(button_ok, XmNactivateCallback, location_view, NULL);
        XtAddCallback(button_add, XmNactivateCallback, location_add, location_name);
        XtAddCallback(button_delete, XmNactivateCallback, location_delete, NULL);

        pos_dialog(location_dialog);

        delw = XmInternAtom(XtDisplay(location_dialog),"WM_DELETE_WINDOW", FALSE);
        XmAddWMProtocolCallback(location_dialog, delw, location_destroy_shell, (XtPointer)location_dialog);

        XtManageChild(form);
        XtManageChild(location_list);
        XtVaSetValues(location_list, XmNbackground, colors[0x0f], NULL);
        XtManageChild(pane);

end_critical_section(&location_dialog_lock, "location_gui.c:location_destroy_shell" );

        XtPopup(location_dialog,XtGrabNone);
        fix_dialog_size(location_dialog);

        // Move focus to the Close button.  This appears to highlight the
        // button fine, but we're not able to hit the <Enter> key to
        // have that default function happen.  Note:  We _can_ hit the
        // <SPACE> key, and that activates the option.
//        XmUpdateDisplay(location_dialog);
        XmProcessTraversal(button_cancel, XmTRAVERSE_CURRENT);

    } else {
        XtPopup(location_dialog,XtGrabNone);
        (void)XRaiseWindow(XtDisplay(location_dialog), XtWindow(location_dialog));
    }
}


