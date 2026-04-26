/*
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2026 The Xastir Group
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
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include <ctype.h>
#include <sys/types.h>

#if HAVE_SYS_TIME_H
  #include <sys/time.h>
#endif // HAVE_SYS_TIME_H
#include <time.h>

#include <Xm/XmAll.h>

#ifdef HAVE_XBAE_MATRIX_H
  #include <Xbae/Matrix.h>
#endif  // HAVE_XBAE_MATRIX_H

#include <X11/Xatom.h>
#include <X11/Shell.h>

#include "xastir.h"
#include "main.h"
#include "bulletin_gui.h"
#include "bulletin_controller.h"
#include "interface.h"
#include "util.h"
#include "mutex_utils.h"
#include "db_funcs.h"

// Must be last include file
#include "leak_detection.h"

extern XmFontList fontlist1;    // Menu/System fontlist
Widget Display_bulletins_dialog = NULL;
Widget Display_bulletins_text = NULL;
Widget dist_data = NULL;
Widget zero_bulletin_data = NULL;

static xastir_mutex display_bulletins_dialog_lock;

int bulletin_range;
int new_bulletin_flag = 0;
int new_bulletin_count = 0;
static time_t first_new_bulletin_time = 0;
static time_t last_new_bulletin_time = 0;

/* Presentation/logic controller — mirrors the persistent globals.
 * Synced from globals before every logical operation.
 * A future pass will have xa_config write directly to this struct. */
static bulletin_controller_t bc;



void bulletin_gui_init(void)
{
  init_critical_section(&display_bulletins_dialog_lock);
  bulletin_controller_init(&bc);
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
void popup_bulletins(void)
{
  if ( Display_bulletins_dialog == NULL )     // Dialog not up
  {

    // Bring up the dialog
    Bulletins( (Widget)NULL, (XtPointer)NULL, (XtPointer)NULL );
  }
}





void bulletin_message(char *call_sign, char *tag, char *packet_message, time_t sec_heard)
{
  char temp[200];
  char temp_my_course[10];
  char temp_text[30];
  double distance;
  XmTextPosition pos, eol, eod;
  struct tm *tmp;
  time_t timehd;
  char time_str[20];
  char *temp_ptr;

  timehd = sec_heard;
  tmp = localtime(&timehd);

  if ( (packet_message != NULL) && (strlen(packet_message) > MAX_MESSAGE_LENGTH) )
  {
    if (debug_level & 1)
    {
      fprintf(stderr,"bulletin_message:  Message length too long\n");
    }
    return;
  }

  (void)strftime(time_str, sizeof(time_str), "%b %d %H:%M", tmp);

  distance = distance_from_my_station(call_sign, temp_my_course, english_units);

  /* Sync controller from persistent globals before filtering. */
  bc.range              = bulletin_range;
  bc.view_zero_distance = view_zero_distance_bulletins;

  if (bulletin_format_line(call_sign, tag, time_str, distance,
                           english_units ? langcode("UNIOP00004") : langcode("UNIOP00005"),
                           packet_message,
                           temp, sizeof(temp)) != 0)
  {
    return;
  }

  if (!bulletin_controller_in_range(&bc, distance))
  {
    return;
  }

  begin_critical_section(&display_bulletins_dialog_lock, "bulletin_gui.c:bulletin_message" );

  if ((Display_bulletins_dialog != NULL) && Display_bulletins_text != NULL)     // Dialog is up
  {

    eod = XmTextGetLastPosition(Display_bulletins_text);
    memcpy(temp_text, temp, 15);
    temp_text[14] = '\0'; // Terminate string

    // Look for this bulletin ID.  "pos" will hold the first char position if found.
    if (XmTextFindString(Display_bulletins_text, 0, temp_text, XmTEXT_FORWARD, &pos))
    {

      // Found it, so now find the end-of-line for it
      if (XmTextFindString(Display_bulletins_text, pos, "\n", XmTEXT_FORWARD, &eol))
      {
        eol++;
      }
      else
      {
        eol = eod;
      }

      // And replace the old bulletin with a new copy
      if (eol == eod)
      {
        temp[strlen(temp)-1] = '\0';
      }
      XmTextReplace(Display_bulletins_text, pos, eol, temp);
    }
    else
    {
      for (pos = 0; strlen(temp_text) > 12 && pos < eod;)
      {
        if (XmCOPY_SUCCEEDED == XmTextGetSubstring(Display_bulletins_text, pos, 14, 30, temp_text))
        {
          if (temp_text[0] && strncmp(temp, temp_text, 14) < 0)
          {
            break;
          }
        }
        else
        {
          break;
        }

        if (XmTextFindString(Display_bulletins_text, pos, "\n", XmTEXT_FORWARD, &eol))
        {
          pos = ++eol;
        }
        else
        {
          pos = eod;
        }
      }
      if (pos == eod)
      {
        temp[strlen(temp)-1] = '\0'; // End-of-Data remove trailing LF
        if (pos > 0)   // Already have text. Need to insert LF between items
        {
          memmove(&temp[1], temp, strlen(temp));
          temp[0] = '\n';
        }
      }
      XmTextInsert(Display_bulletins_text, pos, temp);
    }
    temp_ptr = XmTextFieldGetString(dist_data);
    bulletin_range = atoi(temp_ptr);
    XtFree(temp_ptr);
  }

  end_critical_section(&display_bulletins_dialog_lock, "bulletin_gui.c:bulletin_message" );
}





static void bulletin_line(Message *fill)
{
  bulletin_message(fill->from_call_sign, fill->call_sign, fill->message_line, fill->sec_heard);
}





static void scan_bulletin_file(void)
{
  mscan_file(MESSAGE_BULLETIN, bulletin_line);
}





// bulletin_data_add
//
// Adds the bulletin to the message database.  Updates the Bulletins
// dialog if it is up.  Causes Bulletins dialog to pop up if the
// bulletin matches certain parameters.
//
long temp_bulletin_record;

void bulletin_data_add(char *call_sign, char *from_call, char *data,
                       char *seq, char type, char from)
{
  int distance = -1;

  // Add to the message database
  (void)msg_data_add(call_sign,
                     from_call,
                     data,
                     " ",    // Need something here.  Empty string no good.
                     MESSAGE_BULLETIN,
                     from,
                     &temp_bulletin_record);

  // If we received a NEW bulletin
  if (temp_bulletin_record == -1L)
  {
    char temp[10];

    //fprintf(stderr,"We think it's a new bulletin!\n");

    // We add to the distance in order to come up with 0.0
    // if the distance is not known at all (no position
    // found yet).
    distance = (int)(distance_from_my_station(from_call, temp, english_units) + 0.9999);

    /* Sync controller from persistent globals before filtering. */
    bc.range              = bulletin_range;
    bc.view_zero_distance = view_zero_distance_bulletins;

    if (bulletin_controller_in_range(&bc, (double)distance))
    {
      // We have a _new_ bulletin that's within our
      // current range setting.

      /* Record it in the controller; write back to legacy globals. */
      bulletin_controller_record_new(&bc, sec_now());
      first_new_bulletin_time = bc.first_new_time;
      last_new_bulletin_time  = bc.last_new_time;
      new_bulletin_flag       = bc.new_flag;

      if (debug_level & 1)
      {
        fprintf(stderr,"New bulletin:");
        fprintf(stderr,"%05d:%9s:%c:%c:%9s:%s:%s  ",
                distance,
                call_sign,
                type,
                from,
                from_call,
                data,
                seq);
        fprintf(stderr,"  Distance ok:%d miles",distance);
      }

      bc.pop_up_enabled = pop_up_new_bulletins;
      if (bc.pop_up_enabled)
      {
        //fprintf(stderr,"bulletin_data_add: popping up bulletins\n");
        popup_bulletins();
        if (debug_level & 1)
        {
          fprintf(stderr,"\n");
        }
      }
      else
      {
        if (debug_level & 1)
        {
          fprintf(stderr,", but popups disabled!\n");
        }
      }
    }
    else
    {
      //            fprintf(stderr,", but distance didn't work out!\n");
    }
  }
  // Update the View->Bulletins dialog if it's up
  bulletin_message(from_call,
                   call_sign,
                   data,
                   sec_now());

}





// Find each bulletin that is within our range _and_ within our time
// parameters for new bulletins.  Count them only.  Results returned
// in the new_bulletin_count variable.
void count_bulletin_messages(char *call_sign, char *packet_message, time_t sec_heard)
{
  char temp_my_course[10];
  double distance;

  if ( (packet_message != NULL) && (strlen(packet_message) > MAX_MESSAGE_LENGTH) )
  {
    if (debug_level & 1)
    {
      fprintf(stderr,"bulletin_message:  Message length too long\n");
    }
    return;
  }

  distance = distance_from_my_station(call_sign, temp_my_course, english_units);

  /* Sync controller from persistent globals before filtering. */
  bc.range              = bulletin_range;
  bc.view_zero_distance = view_zero_distance_bulletins;
  bc.first_new_time     = first_new_bulletin_time;

  bulletin_controller_count_one(&bc, distance, sec_heard);

  /* Write count back to the legacy global so the rest of the code sees it. */
  new_bulletin_count = bc.new_count;
}





static void count_bulletin_line(Message *fill)
{
  count_bulletin_messages(fill->from_call_sign, fill->message_line, fill->sec_heard);
}





static void count_new_bulletins(void)
{
  mscan_file(MESSAGE_BULLETIN, count_bulletin_line);
}





// Function called by mscan_file for each bulletin with zero for the
// position_known flag.  See next function find_zero_position_bulletins()
//
static void zero_bulletin_processing(Message *fill)
{
  DataRow *p_station; // Pointer to station data

  if (!fill->position_known)
  {

    //fprintf(stderr,"Position unknown: %s:%s\n",
    //    fill->from_call_sign,
    //    fill->message_line);

    if ( search_station_name(&p_station, fill->from_call_sign, 1) )
    {
      if ( (p_station->coord_lon == 0l)
           && (p_station->coord_lat == 0l) )
      {
        //fprintf(stderr,"Found it but still no valid position!\n");
      }
      else   // Found valid position for this bulletin
      {
        //fprintf(stderr,"Found it now! %s:%s\n",
        //    fill->from_call_sign,
        //    fill->message_line);

        fill->position_known = 1;

        /* Extend first_new_time backwards to include this bulletin. */
        if (first_new_bulletin_time == (time_t)0
            || first_new_bulletin_time > fill->sec_heard)
        {
          first_new_bulletin_time = fill->sec_heard;
          bc.first_new_time = first_new_bulletin_time;
        }

        if (pop_up_new_bulletins)
        {
          int distance;
          char temp_my_course[10];

          distance = (int)(distance_from_my_station(fill->from_call_sign,
                                                    temp_my_course,
                                                    english_units) + 0.9999);

          /* Sync controller and use it for the range check. */
          bc.range              = bulletin_range;
          bc.view_zero_distance = view_zero_distance_bulletins;

          if (bulletin_controller_in_range(&bc, (double)distance))
          {
            if (debug_level & 1)
            {
              fprintf(stderr,"Filled in distance for earlier bulletin:%d miles\n",
                      distance);
            }

            if (!view_zero_distance_bulletins)
            {
              //fprintf(stderr,"zero_bulletin_processing: popping up bulletins\n");
              popup_bulletins();
            }
          }
        }
      }
    }
    else
    {
      // No position known for the bulletin.  Skip it for now.
      //fprintf(stderr,"Still not found\n");
    }
  }
}





// Find all bulletins that have a zero for the position_known flag.
// Calls the function above for each bulletin.
//
static void find_zero_position_bulletins(void)
{
  mscan_file(MESSAGE_BULLETIN, zero_bulletin_processing);
}





// Function called by main.c:UpdateTime().  Checks whether enough
// time has passed since the last new bulletin came in (so that the
// posit for it might come in as well).  If so, checks for bulletins
// that are newer than first_new_bulletin_time and fit within our
// range.  If any found, it updates the Bulletins dialog.
time_t last_bulletin_check = (time_t)0l;

void check_for_new_bulletins(int curr_sec)
{
  /* Sync controller from persistent globals before the timing gate. */
  bc.new_flag       = new_bulletin_flag;
  bc.last_new_time  = last_new_bulletin_time;
  bc.last_check     = last_bulletin_check;

  /* Outer throttle: check at most every BULLETIN_CHECK_INTERVAL seconds. */
  if ( (last_bulletin_check + BULLETIN_CHECK_INTERVAL) > curr_sec )
  {
    return;
  }
  bulletin_controller_mark_checked(&bc, (time_t)curr_sec);
  last_bulletin_check = bc.last_check;

  // Look first to see if we might be able to fill in positions on
  // any older bulletins, then cause a popup for those that fit
  // our parameters.  The below function sets new_bulletin_flag if
  // it is able to fill in a distance for an older bulletin.
  // Note:  This is time-consuming!
  find_zero_position_bulletins();

  // Any new bulletins to check?  If not, return
  if (!new_bulletin_flag)
  {
    return;
  }

  // Enough time passed since most recent bulletin?  Need to have
  // enough time to perhaps fill in a distance for each bulletin.
  if ( (last_new_bulletin_time + BULLETIN_SETTLE_TIME) > curr_sec )
  {
    //fprintf(stderr,"Not enough time has passed\n");
    return;
  }

  // If we get to here, then we think we may have at least one new
  // bulletin, and the latest arrived more than XX seconds ago
  // (currently 15 seconds).  Check for bulletins which have
  // timestamps equal to or newer than first_new_bulletin_time and
  // fit within our range.

  new_bulletin_count = 0;
  bc.new_count       = 0;

  //fprintf(stderr,"Checking for new bulletins\n");

  count_new_bulletins();

  //fprintf(stderr,"%d new bulletins found\n",new_bulletin_count);

  if (new_bulletin_count)
  {
    //fprintf(stderr,"check_for_new_bulletins: popping up bulletins\n");
    popup_bulletins();

    if (debug_level & 1)
    {
      fprintf(stderr,"New bulletins (%d) caused popup!\n",new_bulletin_count);
    }
  }
  else
  {
    if (debug_level & 1)
    {
      fprintf(stderr,"No bulletin popup generated.\n");
    }
  }

  // Reset so that we can do it all over again later.
  bc.last_new_time = last_new_bulletin_time;
  bulletin_controller_reset_batch(&bc);
  first_new_bulletin_time = bc.first_new_time;
  new_bulletin_flag       = bc.new_flag;
  new_bulletin_count      = bc.new_count;
}





void Display_bulletins_destroy_shell(Widget UNUSED(widget), XtPointer clientData, XtPointer UNUSED(callData) )
{
  Widget shell = (Widget) clientData;
  char *temp_ptr;


  // Keep this.  It stores the range in a global variable when we destroy the dialog.
  temp_ptr = XmTextFieldGetString(dist_data);
  bulletin_range = atoi(temp_ptr);
  XtFree(temp_ptr);

  XtPopdown(shell);

  begin_critical_section(&display_bulletins_dialog_lock, "bulletin_gui.c:Display_bulletins_destroy_shell" );

  XtDestroyWidget(shell);
  Display_bulletins_dialog = (Widget)NULL;

  end_critical_section(&display_bulletins_dialog_lock, "bulletin_gui.c:Display_bulletins_destroy_shell" );

}





void Display_bulletins_change_range(Widget widget, XtPointer clientData, XtPointer callData)
{
  char *temp_ptr;


  // Keep this.  It stores the range in a global variable when we destroy the dialog.
  temp_ptr = XmTextFieldGetString(dist_data);
  bulletin_range = atoi(temp_ptr);
  XtFree(temp_ptr);

  view_zero_distance_bulletins = (int)XmToggleButtonGetState(zero_bulletin_data);
  //fprintf(stderr,"%d\n",view_zero_distance_bulletins);

  Display_bulletins_destroy_shell( widget, clientData, callData);
  Bulletins( widget, clientData, callData);
}





void  Zero_Bulletin_Data_toggle( Widget widget, XtPointer clientData, XtPointer callData)
{
  char *which = (char *)clientData;
  XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

  if(state->set)
  {
    view_zero_distance_bulletins = atoi(which);
  }
  else
  {
    view_zero_distance_bulletins = 0;
  }

  Display_bulletins_destroy_shell( widget, Display_bulletins_dialog, callData);
  Bulletins( widget, clientData, callData);
}





void Bulletins(Widget UNUSED(w), XtPointer UNUSED(clientData), XtPointer UNUSED(callData) )
{
  Widget pane, form, button_range, button_close, dist, dist_units;
  unsigned int n;
  Arg args[50];
  Atom delw;
  char temp[10];


  if(!Display_bulletins_dialog)
  {


    begin_critical_section(&display_bulletins_dialog_lock, "bulletin_gui.c:Bulletins" );


    Display_bulletins_dialog = XtVaCreatePopupShell(langcode("BULMW00001"),
                               xmDialogShellWidgetClass,
                               appshell,
                               XmNdeleteResponse,XmDESTROY,
                               XmNdefaultPosition, FALSE,
                               XmNfontList, fontlist1,
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
                                   XmNfontList, fontlist1,
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
                                        XmNfontList, fontlist1,
                                        NULL);

    dist_units = XtVaCreateManagedWidget((english_units?langcode("UNIOP00004"):langcode("UNIOP00005")),
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
                                         XmNfontList, fontlist1,
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
                                           XmNfontList, fontlist1,
                                           NULL);

    zero_bulletin_data = XtVaCreateManagedWidget(langcode("WPUPCFD029"),
                         xmToggleButtonWidgetClass,
                         form,
                         XmNtopAttachment, XmATTACH_FORM,
                         XmNtopOffset, 5,
                         XmNbottomAttachment, XmATTACH_NONE,
                         XmNleftAttachment, XmATTACH_WIDGET,
                         XmNleftWidget, button_range,
                         XmNleftOffset,10,
                         XmNrightAttachment, XmATTACH_NONE,
                         XmNnavigationType, XmTAB_GROUP,
                         MY_FOREGROUND_COLOR,
                         MY_BACKGROUND_COLOR,
                         XmNfontList, fontlist1,
                         NULL);
    XtAddCallback(zero_bulletin_data,XmNvalueChangedCallback,Zero_Bulletin_Data_toggle,"1");
    if (view_zero_distance_bulletins)
    {
      XmToggleButtonSetState(zero_bulletin_data,TRUE,FALSE);
    }
    else
    {
      XmToggleButtonSetState(zero_bulletin_data,FALSE,FALSE);
    }

    n=0;
    XtSetArg(args[n], XmNrows, 15);
    n++;
    XtSetArg(args[n], XmNcolumns, 108);
    n++;
    XtSetArg(args[n], XmNtraversalOn, FALSE);
    n++;
    XtSetArg(args[n], XmNeditable, FALSE);
    n++;
    XtSetArg(args[n], XmNeditMode, XmMULTI_LINE_EDIT);
    n++;
    XtSetArg(args[n], XmNwordWrap, TRUE);
    n++;
    XtSetArg(args[n], XmNscrollHorizontal, TRUE);
    n++;
    XtSetArg(args[n], XmNscrollVertical, TRUE);
    n++;
    XtSetArg(args[n], XmNcursorPositionVisible, FALSE);
    n++;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET);
    n++;
    XtSetArg(args[n], XmNtopWidget, dist);
    n++;
    XtSetArg(args[n], XmNtopOffset, 20);
    n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM);
    n++;
    XtSetArg(args[n], XmNbottomOffset, 30);
    n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM);
    n++;
    XtSetArg(args[n], XmNleftOffset, 5);
    n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM);
    n++;
    XtSetArg(args[n], XmNrightOffset, 5);
    n++;
    XtSetArg(args[n], XmNforeground, MY_FG_COLOR);
    n++;
    XtSetArg(args[n], XmNbackground, MY_BG_COLOR);
    n++;
    XtSetArg(args[n], XmNfontList, fontlist1);
    n++;


    Display_bulletins_text = XmCreateScrolledText(form,
                             "Bulletins text",
                             args,
                             n);

    button_close = XtVaCreateManagedWidget(langcode("UNIOP00003"),
                                           xmPushButtonGadgetClass,
                                           form,
                                           XmNtopAttachment, XmATTACH_NONE,
                                           XmNbottomAttachment, XmATTACH_FORM,
                                           XmNbottomOffset, 5,
                                           XmNleftAttachment, XmATTACH_POSITION,
                                           XmNleftPosition, 2,
                                           XmNrightAttachment, XmATTACH_POSITION,
                                           XmNrightPosition, 3,
                                           MY_FOREGROUND_COLOR,
                                           MY_BACKGROUND_COLOR,
                                           XmNfontList, fontlist1,
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

    end_critical_section(&display_bulletins_dialog_lock, "bulletin_gui.c:Bulletins" );

    scan_bulletin_file();

    // Move focus to the Close button.  This appears to
    // highlight the button fine, but we're not able to hit the
    // <Enter> key to have that default function happen.  Note:
    // We _can_ hit the <SPACE> key, and that activates the
    // option.
    //XmUpdateDisplay(Display_bulletins_dialog);
    XmProcessTraversal(button_close, XmTRAVERSE_CURRENT);

  }
  else
  {
    (void)XRaiseWindow(XtDisplay(Display_bulletins_dialog), XtWindow(Display_bulletins_dialog));
  }
}


