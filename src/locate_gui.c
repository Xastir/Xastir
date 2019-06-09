/*
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2019 The Xastir Group
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

#include <stdio.h>

#include <Xm/XmAll.h>
#ifdef HAVE_XBAE_MATRIX_H
  #include <Xbae/Matrix.h>
#endif  // HAVE_XBAE_MATRIX_H

#include "xastir.h"
#include "main.h"
#include "lang.h"
#include "maps.h"

// Must be last include file
#include "leak_detection.h"

extern XmFontList fontlist1;    // Menu/System fontlist

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
Widget locate_place_list;
Widget  locate_place_chooser = (Widget)NULL;
static xastir_mutex locate_place_chooser_lock;
char match_array_name[50][200];
long match_array_lat[50];
long match_array_long[50];
int match_quantity = 0;





void locate_gui_init(void)
{
  init_critical_section( &locate_station_dialog_lock );
  init_critical_section( &locate_place_dialog_lock );
  init_critical_section( &locate_place_chooser_lock );
  locate_station_call[0] = '\0';
  locate_place_name[0] = '\0';
  locate_state_name[0] = '\0';
  locate_county_name[0] = '\0';
  locate_quad_name[0] = '\0';
  locate_type_name[0] = '\0';
}





/**** LOCATE STATION ******/

void Locate_station_destroy_shell(Widget UNUSED(widget), XtPointer clientData, XtPointer UNUSED(callData) )
{
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

void fcc_rac_lookup(Widget UNUSED(w), XtPointer UNUSED(clientData), XtPointer UNUSED(callData) )
{
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
  (void)remove_trailing_dash_zero(station_call);

  to_upper(station_call);

  switch (station_call[0])
  {
    case 'A':
    case 'K':
    case 'N':
    case 'W':
      if (search_fcc_data_appl(station_call, &my_fcc_data) == 1)
      {

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

        popup_message_always(langcode("WPUPLSP007"),temp);
      }
      else
      {
        xastir_snprintf(temp2,
                        sizeof(temp2),
                        "Callsign Not Found!\n");
        popup_message_always(langcode("POPEM00001"),temp2);
      }
      break;
    case 'V':
      if (search_rac_data(station_call, &my_rac_data) == 1)
      {

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
                  sizeof(temp) - 1 - strlen(temp));

        if (my_rac_data.qual_d[0] == 'D')
          strncat(temp,
                  langcode("STIFCC0009"),
                  sizeof(temp) - 1 - strlen(temp));

        if (my_rac_data.qual_b[0] == 'B' && my_rac_data.qual_c[0] != 'C')
          strncat(temp,
                  langcode("STIFCC0010"),
                  sizeof(temp) - 1 - strlen(temp));

        if (my_rac_data.qual_c[0] == 'C')
          strncat(temp,
                  langcode("STIFCC0011"),
                  sizeof(temp) - 1 - strlen(temp));

        strncat(temp,
                "\n",
                sizeof(temp) - 1 - strlen(temp));

        if (strlen(my_rac_data.club_name) > 1)
        {
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
                  sizeof(temp) - 1 - strlen(temp));
        }


        popup_message_always(langcode("WPUPLSP007"),temp);
      }
      else
      {
        // RAC code does its own popup in this case?
        //fprintf(stderr, "Callsign not found\n");
      }
      break;
    default:
      xastir_snprintf(temp2,
                      sizeof(temp2),
                      "Not an FCC or RAC callsign!\n");
      popup_message_always(langcode("POPEM00001"),temp2);
      break;
  }

  // Don't enable this as then we can't click on the Locate button
  // later.
  //Locate_station_destroy_shell(w, clientData, callData);
}





/*
 *  Locate a station by centering the map at its position
 */
void Locate_station_now(Widget UNUSED(w), XtPointer UNUSED(clientData), XtPointer UNUSED(callData) )
{
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
  (void)remove_trailing_dash_zero(locate_station_call);

  /*fprintf(stderr,"looking for %s\n",locate_station_call);*/
  if (locate_station(da, locate_station_call, (int)XmToggleButtonGetState(locate_case_data),
                     (int)XmToggleButtonGetState(locate_match_data),1) ==0)
  {
    xastir_snprintf(temp2, sizeof(temp2), langcode("POPEM00002"), locate_station_call);
    popup_message_always(langcode("POPEM00001"),temp2);
  }

  // Don't enable this as then we can't click on the FCC/RAC
  // button later, and we'll lose the callsign info if we want to
  // see it again.
  //Locate_station_destroy_shell(w, clientData, callData);
}





// Here we pass in a 1 in callData if it's an emergency locate,
// for when we've received a Mic-E emergency packet.
//
void Locate_station(Widget UNUSED(w), XtPointer UNUSED(clientData), XtPointer callData)
{
  static Widget pane, form, button_locate, button_cancel, call,
         button_lookup, sep;
  Atom delw;
  int emergency_flag = XTPOINTER_TO_INT(callData);


  if (!locate_station_dialog)
  {

    begin_critical_section(&locate_station_dialog_lock, "locate_gui.c:Locate_station" );

    // Check whether it is an emergency locate function
    if (emergency_flag == 1)
    {
      locate_station_dialog = XtVaCreatePopupShell(langcode("WPUPLSP006"),
                              xmDialogShellWidgetClass, appshell,
                              XmNdeleteResponse, XmDESTROY,
                              XmNdefaultPosition, FALSE,
                              XmNfontList, fontlist1,
                              NULL);
    }
    else    // Non-emergency locate
    {
      locate_station_dialog = XtVaCreatePopupShell(langcode("WPUPLSP001"),
                              xmDialogShellWidgetClass, appshell,
                              XmNdeleteResponse, XmDESTROY,
                              XmNdefaultPosition, FALSE,
                              XmNfontList, fontlist1,
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
                                   XmNfontList, fontlist1,
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
                          XmNfontList, fontlist1,
                          NULL);

    locate_case_data  = XtVaCreateManagedWidget(langcode("WPUPLSP003"),xmToggleButtonWidgetClass,form,
                        XmNtopAttachment, XmATTACH_WIDGET,
                        XmNtopWidget, call,
                        XmNtopOffset, 20,
                        XmNbottomAttachment, XmATTACH_NONE,
                        XmNleftAttachment, XmATTACH_FORM,
                        XmNleftOffset,10,
                        XmNrightAttachment, XmATTACH_NONE,
                        XmNbackground, colors[0xff],
                        XmNnavigationType, XmTAB_GROUP,
                        XmNtraversalOn, TRUE,
                        XmNfontList, fontlist1,
                        NULL);

    locate_match_data  = XtVaCreateManagedWidget(langcode("WPUPLSP004"),xmToggleButtonWidgetClass,form,
                         XmNtopAttachment, XmATTACH_WIDGET,
                         XmNtopWidget, call,
                         XmNtopOffset, 20,
                         XmNbottomAttachment, XmATTACH_NONE,
                         XmNleftAttachment, XmATTACH_WIDGET,
                         XmNleftWidget,locate_case_data,
                         XmNleftOffset,20,
                         XmNrightAttachment, XmATTACH_NONE,
                         XmNbackground, colors[0xff],
                         XmNnavigationType, XmTAB_GROUP,
                         XmNtraversalOn, TRUE,
                         XmNfontList, fontlist1,
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
                                  XmNfontList, fontlist1,
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
                                            XmNfontList, fontlist1,
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
                                            XmNfontList, fontlist1,
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
                                            XmNfontList, fontlist1,
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

  }
  else
  {
    (void)XRaiseWindow(XtDisplay(locate_station_dialog), XtWindow(locate_station_dialog));
  }
}





/*******************************************************************/
/* Locate Place Chooser routines */



/*
 *  Locate Place Chooser PopUp window: Cancelled
 */
void Locate_place_chooser_destroy_shell(Widget UNUSED(widget), XtPointer clientData, XtPointer UNUSED(callData) )
{
  Widget shell = (Widget) clientData;

  begin_critical_section(&locate_place_chooser_lock, "locate_gui.c:Locate_place_chooser_destroy_shell" );

  XtDestroyWidget(shell);
  locate_place_chooser = (Widget)NULL;

  end_critical_section(&locate_place_chooser_lock, "locate_gui.c:Locate_place_chooser_destroy_shell" );

}





/*
 *  Locate Place Selection PopUp window: Map selected place
 */
void Locate_place_chooser_select(Widget widget,
                                 XtPointer UNUSED(clientData),
                                 XtPointer UNUSED(callData) )
{

  int ii, xx;
  char *temp;
  XmString *list;
  int found = 0;
  int index = 0;

  begin_critical_section(&locate_place_chooser_lock, "locate_gui.c:Locate_place_chooser_select" );

  if (locate_place_chooser)
  {
    XtVaGetValues(locate_place_list,
                  XmNitemCount,
                  &ii,
                  XmNitems,
                  &list,
                  NULL);

    for (xx=1; xx<=ii; xx++)
    {
      if (XmListPosSelected(locate_place_list, xx))
      {
        found = 1;
        index = xx;
        if (XmStringGetLtoR(list[(xx-1)], XmFONTLIST_DEFAULT_TAG, &temp))
        {
          xx=ii+1;
        }
      }
    }

    if (found)
    {

      // Center the map at the chosen location
      set_map_position(widget,
                       match_array_lat[index-1],
                       match_array_long[index-1]);

      XtFree(temp);
    }
  }

  end_critical_section(&locate_place_chooser_lock, "locate_gui.c:Locate_place_chooser_select" );

}





void Locate_place_chooser(Widget UNUSED(widget),
                          XtPointer UNUSED(clientData),
                          XtPointer UNUSED(callData) )
{

  Widget pane, form, button_ok, button_cancel;
  Arg al[50];
  register unsigned int ac = 0;
  int ii, nn;
  XmString str_ptr;
  Atom delw;


  if (locate_place_chooser != NULL)
  {
    Locate_place_chooser_destroy_shell(locate_place_chooser, locate_place_chooser, NULL);
  }

  begin_critical_section(&locate_place_chooser_lock, "locate_gui.c:Locate_place_chooser");

  if (locate_place_chooser == NULL)
  {

    // Set up a selection box:
    locate_place_chooser = XtVaCreatePopupShell(langcode("WPUPCFS028"),
                           xmDialogShellWidgetClass, appshell,
                           XmNdeleteResponse, XmDESTROY,
                           XmNdefaultPosition, FALSE,
                           XmNbackground, colors[0xff],
                           XmNfontList, fontlist1,
                           NULL);

    pane = XtVaCreateWidget("Locate_place_chooser pane",xmPanedWindowWidgetClass, locate_place_chooser,
                            XmNbackground, colors[0xff],
                            NULL);

    form =  XtVaCreateWidget("Locate_place_chooser form",xmFormWidgetClass, pane,
                             XmNfractionBase, 5,
                             XmNbackground, colors[0xff],
                             XmNautoUnmanage, FALSE,
                             XmNshadowThickness, 1,
                             NULL);


    // Attach buttons first to the bottom of the form,
    // so that we'll be able to stretch this thing
    // vertically to see all of the entries.
    //
    button_ok = XtVaCreateManagedWidget(langcode("WPUPCFS028"),xmPushButtonGadgetClass, form,
                                        XmNtopAttachment, XmATTACH_NONE,
                                        XmNbottomAttachment, XmATTACH_FORM,
                                        XmNbottomOffset, 5,
                                        XmNleftAttachment, XmATTACH_POSITION,
                                        XmNleftPosition, 1,
                                        XmNrightAttachment, XmATTACH_POSITION,
                                        XmNrightPosition, 2,
                                        XmNnavigationType, XmTAB_GROUP,
                                        XmNfontList, fontlist1,
                                        NULL);

    XtAddCallback(button_ok,
                  XmNactivateCallback,
                  Locate_place_chooser_select,
                  locate_place_chooser);

    button_cancel = XtVaCreateManagedWidget(langcode("UNIOP00003"),xmPushButtonGadgetClass, form,
                                            XmNtopAttachment, XmATTACH_NONE,
                                            XmNbottomAttachment, XmATTACH_FORM,
                                            XmNbottomOffset, 5,
                                            XmNleftAttachment, XmATTACH_POSITION,
                                            XmNleftPosition, 3,
                                            XmNrightAttachment, XmATTACH_POSITION,
                                            XmNrightPosition, 4,
                                            XmNnavigationType, XmTAB_GROUP,
                                            XmNfontList, fontlist1,
                                            NULL);

    XtAddCallback(button_cancel,
                  XmNactivateCallback,
                  Locate_place_chooser_destroy_shell,
                  locate_place_chooser);

    // set args for color
    ac = 0;
    XtSetArg(al[ac], XmNbackground, colors[0xff]);
    ac++;
    XtSetArg(al[ac], XmNvisibleItemCount, 6);
    ac++;
    XtSetArg(al[ac], XmNtraversalOn, TRUE);
    ac++;
    XtSetArg(al[ac], XmNshadowThickness, 3);
    ac++;
    XtSetArg(al[ac], XmNselectionPolicy, XmSINGLE_SELECT);
    ac++;
    XtSetArg(al[ac], XmNscrollBarPlacement, XmBOTTOM_RIGHT);
    ac++;
    XtSetArg(al[ac], XmNtopAttachment, XmATTACH_FORM);
    ac++;
    XtSetArg(al[ac], XmNtopOffset, 5);
    ac++;
    XtSetArg(al[ac], XmNbottomAttachment, XmATTACH_WIDGET);
    ac++;
    XtSetArg(al[ac], XmNbottomWidget, button_ok);
    ac++;
    XtSetArg(al[ac], XmNbottomOffset, 5);
    ac++;
    XtSetArg(al[ac], XmNrightAttachment, XmATTACH_FORM);
    ac++;
    XtSetArg(al[ac], XmNrightOffset, 5);
    ac++;
    XtSetArg(al[ac], XmNleftAttachment, XmATTACH_FORM);
    ac++;
    XtSetArg(al[ac], XmNleftOffset, 5);
    ac++;
    XtSetArg(al[ac], XmNfontList, fontlist1);
    ac++;

    locate_place_list = XmCreateScrolledList(form,"Locate_place_chooser list",al,ac);

    nn = 1;
    for (ii = 0; ii < match_quantity; ii++)
    {
      XmListAddItem(locate_place_list,
                    str_ptr = XmStringCreateLtoR(match_array_name[ii],
                              XmFONTLIST_DEFAULT_TAG),
                    (int)nn++);
      XmStringFree(str_ptr);
    }

    pos_dialog(locate_place_chooser);

    delw = XmInternAtom(XtDisplay(locate_place_chooser),
                        "WM_DELETE_WINDOW",
                        FALSE);

    XmAddWMProtocolCallback(locate_place_chooser,
                            delw,
                            Locate_place_chooser_destroy_shell,
                            (XtPointer)locate_place_chooser);

    XtManageChild(form);
    XtManageChild(locate_place_list);
    XtVaSetValues(locate_place_list, XmNbackground, colors[0x0f], NULL);
    XtManageChild(pane);

    XtPopup(locate_place_chooser,XtGrabNone);

    // Move focus to the Cancel button.  This appears to
    // highlight t
    // button fine, but we're not able to hit the
    // <Enter> key to
    // have that default function happen.  Note:  We
    // _can_ hit the
    // <SPACE> key, and that activates the option.
    XmProcessTraversal(button_cancel, XmTRAVERSE_CURRENT);
  }

  end_critical_section(&locate_place_chooser_lock, "locate_gui.c:Locate_place_chooser" );

}
/*******************************************************************/





/**** LOCATE PLACE ******/

void Locate_place_destroy_shell(Widget UNUSED(widget), XtPointer clientData, XtPointer UNUSED(callData) )
{
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
void Locate_place_now(Widget w, XtPointer clientData, XtPointer callData)
{
  char *temp_ptr;
//    int ii;


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

  match_quantity = gnis_locate_place(da, locate_place_name,
                                     locate_state_name, locate_county_name, locate_quad_name,
                                     locate_type_name, locate_gnis_filename,
                                     (int)XmToggleButtonGetState(locate_place_case_data),
                                     (int)XmToggleButtonGetState(locate_place_match_data),
                                     match_array_name, match_array_lat, match_array_long);

  if (0 == match_quantity) // Try population centers.
    match_quantity = pop_locate_place(da, locate_place_name,
                                      locate_state_name, locate_county_name, locate_quad_name,
                                      locate_type_name, locate_gnis_filename,
                                      (int)XmToggleButtonGetState(locate_place_case_data),
                                      (int)XmToggleButtonGetState(locate_place_match_data),
                                      match_array_name, match_array_lat, match_array_long);

  if (match_quantity)
  {
    // Found some matches!

    // Have a Chooser dialog if more than one match is found,
    // plus the associated callbacks.  Don't center the map
    // unless the user chooses one of the matches.  Leave the
    // chooser dialog up so that the user can click on the
    // matches one at a time until the correct one is found,
    // then he/she can hit the Close button on that dialog to
    // make it go away.

    // Bring up a chooser dialog with the results from the
    // match_array and a close button.  Allow the user to choose
    // which one to center the map to.  Could also allow the
    // user to find out more about each match if we fill the
    // array with more data from the GNIS file.

// Debug:  Print out the contents of the match arrays.
//fprintf(stderr,"Found %d matches!\n", match_quantity);

    /*
    set_dangerous("printing");
    for (ii = 0; ii < match_quantity; ii++) {
        fprintf(stderr,
            "%d, %s, %ld, %ld\n",
            ii,
            match_array_name[ii],
            match_array_lat[ii],
            match_array_long[ii]);
    }
    clear_dangerous();
    */

    // This one pops up the names of whatever we found.
    // "Found It!"
    //popup_message_always( langcode("POPEM00029"), match_array_name[0]);

    // Bring up the new Chooser dialog
    (void)Locate_place_chooser(w, clientData, callData);
  }
  else
  {
    // No matches found.
    popup_message_always(langcode("POPEM00025"),locate_place_name);
  }

  Locate_place_destroy_shell(w, clientData, callData);
}





void Locate_place(Widget UNUSED(w), XtPointer UNUSED(clientData), XtPointer UNUSED(callData) )
{
  static Widget pane, form, button_ok, button_cancel, sep,
         place, state, county, quad, place_type, gnis_file;
  Atom delw;

  if (!locate_place_dialog)
  {

    begin_critical_section(&locate_place_dialog_lock, "locate_gui.c:Locate_place" );

    locate_place_dialog = XtVaCreatePopupShell(langcode("PULDNMP014"),
                          xmDialogShellWidgetClass, appshell,
                          XmNdeleteResponse, XmDESTROY,
                          XmNdefaultPosition, FALSE,
                          XmNfontList, fontlist1,
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
                                    XmNfontList, fontlist1,
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
                        XmNfontList, fontlist1,
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
                                    XmNfontList, fontlist1,
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
                        XmNfontList, fontlist1,
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
                                     XmNfontList, fontlist1,
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
                         XmNfontList, fontlist1,
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
                                   XmNfontList, fontlist1,
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
                       XmNfontList, fontlist1,
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
                                         XmNfontList, fontlist1,
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
                       XmNfontList, fontlist1,
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
                                        XmNfontList, fontlist1,
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
                            XmNfontList, fontlist1,
                            NULL);


    locate_place_case_data  = XtVaCreateManagedWidget(langcode("WPUPLSP003"),xmToggleButtonWidgetClass,form,
                              XmNtopAttachment, XmATTACH_WIDGET,
                              XmNtopWidget, gnis_file,
                              XmNtopOffset, 20,
                              XmNbottomAttachment, XmATTACH_NONE,
                              XmNleftAttachment, XmATTACH_FORM,
                              XmNleftOffset,10,
                              XmNrightAttachment, XmATTACH_NONE,
                              XmNbackground, colors[0xff],
                              XmNnavigationType, XmTAB_GROUP,
                              XmNtraversalOn, TRUE,
                              XmNfontList, fontlist1,
                              NULL);

    locate_place_match_data  = XtVaCreateManagedWidget(langcode("WPUPLSP004"),xmToggleButtonWidgetClass,form,
                               XmNtopAttachment, XmATTACH_WIDGET,
                               XmNtopWidget, gnis_file,
                               XmNtopOffset, 20,
                               XmNbottomAttachment, XmATTACH_NONE,
                               XmNleftAttachment, XmATTACH_WIDGET,
                               XmNleftWidget,locate_place_case_data,
                               XmNleftOffset,20,
                               XmNrightAttachment, XmATTACH_NONE,
                               XmNbackground, colors[0xff],
                               XmNnavigationType, XmTAB_GROUP,
                               XmNtraversalOn, TRUE,
                               XmNfontList, fontlist1,
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
                                  XmNfontList, fontlist1,
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
                                        XmNfontList, fontlist1,
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
                                            XmNfontList, fontlist1,
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

  }
  else
  {
    (void)XRaiseWindow(XtDisplay(locate_place_dialog), XtWindow(locate_place_dialog));
  }
}


