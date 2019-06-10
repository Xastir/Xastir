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
#include <stdlib.h>
#include <sys/stat.h>
#include <pwd.h>
#include <unistd.h>
#include <Xm/XmAll.h>

#include "xastir.h"
#include "main.h"
#include "lang.h"
#include "objects.h"
#include "popup.h"
#include "fetch_remote.h"
#include "util.h"
#include "xa_config.h"

// Must be last include file
#include "leak_detection.h"


extern XmFontList fontlist1;    // Menu/System fontlist

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

int fetching_findu_trail_now = 0;

int track_station_on = 0;       /* used for tracking stations */
int track_me;
int track_case;                 /* used for tracking stations */
int track_match;                /* used for tracking stations */
char tracking_station_call[30]; /* Tracking station callsign */
char download_trail_station_call[30];   /* Trail station callsign */
//N0VH
#define MAX_FINDU_DURATION 120
#define MAX_FINDU_START_TIME 336
// Make these two match, as that will be the most desired case: Snag
// the track for as far back as possible up to the present in one
// shot...
int posit_start = MAX_FINDU_DURATION;
int posit_length = MAX_FINDU_DURATION;





void track_gui_init(void)
{
  init_critical_section( &track_station_dialog_lock );
  init_critical_section( &download_findu_dialog_lock );

  if (temp_tracking_station_call[0] != '\0')
  {
    xastir_snprintf(tracking_station_call,
                    sizeof(tracking_station_call),
                    "%s",
                    temp_tracking_station_call);
    track_station_on = 1;
  }
  else
  {
    tracking_station_call[0] = '\0';
  }
}





/**** Track STATION ******/


void track_station_destroy_shell( Widget UNUSED(widget), XtPointer clientData, XtPointer UNUSED(callData) )
{
  Widget shell = (Widget) clientData;
  XtPopdown(shell);

  begin_critical_section(&track_station_dialog_lock, "track_gui.c:track_station_destroy_shell" );

  XtDestroyWidget(shell);
  track_station_dialog = (Widget)NULL;

  end_critical_section(&track_station_dialog_lock, "track_gui.c:track_station_destroy_shell" );

}





void Track_station_clear(Widget w, XtPointer clientData, XtPointer callData)
{

  /* clear station */
  track_station_on=0;
  //track_station_data=NULL;
  //tracking_station_call[0] = '\0';

  // Clear the TrackMe button as well
  XmToggleButtonSetState(trackme_button,FALSE,TRUE);

  track_station_destroy_shell(w, clientData, callData);
  display_zoom_status();
}





void Track_station_now(Widget w, XtPointer clientData, XtPointer callData)
{
  char temp[MAX_CALLSIGN+1];
  char temp2[200];
  int found = 0;
  char *temp_ptr;


  temp_ptr = XmTextFieldGetString(track_station_data);
  xastir_snprintf(temp,
                  sizeof(temp),
                  "%s",
                  temp_ptr);
  XtFree(temp_ptr);

  (void)remove_trailing_spaces(temp);
  (void)remove_trailing_dash_zero(temp);

  xastir_snprintf(tracking_station_call,
                  sizeof(tracking_station_call),
                  "%s",
                  temp);
  track_case  = (int)XmToggleButtonGetState(track_case_data);
  track_match = (int)XmToggleButtonGetState(track_match_data);
  found = locate_station(da, temp, track_case, track_match, 0);

  if ( valid_object(tracking_station_call)    // Name of object is legal
       || valid_call(tracking_station_call)
       || valid_item(tracking_station_call ) )
  {
    track_station_on = 1;   // Track it whether we've seen it yet or not
    if (!found)
    {
      xastir_snprintf(temp2, sizeof(temp2), langcode("POPEM00026"), temp);
      popup_message_always(langcode("POPEM00025"),temp2);
    }
    // Check for exact match, includes SSID
    if ( track_me & !is_my_call( tracking_station_call, 1) )
    {
      XmToggleButtonSetState( trackme_button, FALSE, FALSE );
      track_me = 0;
    }
  }
  else
  {
    tracking_station_call[0] = '\0';    // Empty it out again
    track_station_on = 0;
    xastir_snprintf(temp2, sizeof(temp2), langcode("POPEM00002"), temp);
    popup_message_always(langcode("POPEM00003"),temp2);
  }

  track_station_destroy_shell(w, clientData, callData);
  display_zoom_status();
}





void Track_station( Widget UNUSED(w), XtPointer UNUSED(clientData), XtPointer UNUSED(callData) )
{
  static Widget pane, my_form, button_ok, button_close, button_clear, call, sep;
  Atom delw;

  if (!track_station_dialog)
  {

    begin_critical_section(&track_station_dialog_lock, "track_gui.c:Track_station" );

    track_station_dialog = XtVaCreatePopupShell(langcode("WPUPTSP001"),
                           xmDialogShellWidgetClass, appshell,
                           XmNdeleteResponse, XmDESTROY,
                           XmNdefaultPosition, FALSE,
                           XmNfontList, fontlist1,
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
                                   XmNfontList, fontlist1,
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
                         XmNfontList, fontlist1,
                         NULL);

    track_case_data  = XtVaCreateManagedWidget(langcode("WPUPTSP003"),
                       xmToggleButtonWidgetClass,
                       my_form,
                       XmNtopAttachment, XmATTACH_WIDGET,
                       XmNtopWidget, call,
                       XmNtopOffset, 20,
                       XmNbottomAttachment, XmATTACH_NONE,
                       XmNleftAttachment, XmATTACH_FORM,
                       XmNleftOffset,10,
                       XmNrightAttachment, XmATTACH_NONE,
                       XmNnavigationType, XmTAB_GROUP,
                       XmNtraversalOn, TRUE,
                       MY_FOREGROUND_COLOR,
                       MY_BACKGROUND_COLOR,
                       XmNfontList, fontlist1,
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
                        XmNrightOffset,20,
                        XmNrightAttachment, XmATTACH_NONE,
                        XmNnavigationType, XmTAB_GROUP,
                        XmNtraversalOn, TRUE,
                        MY_FOREGROUND_COLOR,
                        MY_BACKGROUND_COLOR,
                        XmNfontList, fontlist1,
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
                                  XmNfontList, fontlist1,
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
                                        XmNfontList, fontlist1,
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
                                           XmNfontList, fontlist1,
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
                                           XmNfontList, fontlist1,
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

  }
  else
  {
    (void)XRaiseWindow(XtDisplay(track_station_dialog), XtWindow(track_station_dialog));
  }
}





/***** DOWNLOAD FINDU TRAILS *****/



// Struct used for passing two parameters to findu_transfer_thread
typedef struct
{
  char *download_client_ptrs[2];
} track_ptrs_struct;
track_ptrs_struct track_ptrs;



// This is the separate execution thread that fetches the track from
// findu.  The thread is started up by the Download_trail_now()
// function below.
//
static void* findu_transfer_thread(void *arg)
{
  char *fileimg;
  char *log_filename;
//    char log_filename_tmp[210];
  char **ptrs;
  char sys_cmd[128];


  // Get fileimg and log_filename from parameters
  ptrs = arg;
  log_filename = ptrs[0];
  fileimg = ptrs[1];

  // Set global "busy" variable
  fetching_findu_trail_now = 1;

  if (fetch_remote_file(fileimg, log_filename))
  {

    // Had trouble getting the file.  Abort.

// We may not be able to do any GUI stuff from multiple
// threads/processes at the same time.  If that is found to be the
// case we can write to STDERR instead.

    // Dump a message to STDERR
//        fprintf(stderr,
//            "%s  %s\n",
//            langcode("POPEM00035"),
//            langcode("POPEM00044"));

    // Fetch Findu Trail: Failed
    popup_message_always(langcode("POPEM00035"),
                         langcode("POPEM00044"));

    // Reset global "busy" variable
    fetching_findu_trail_now = 0;

    // End the thread
    return(NULL);
  }

// We need to move this message up to the main thread if possible so
// that we don't have multiple threads writing to the GUI.  This
// sort of operation can cause segfaults.  In practice I haven't
// seen any segfaults due to this particular popup though.

  // Fetch Findu Trail: Complete
  popup_message_always(langcode("POPEM00036"),
                       langcode("POPEM00045"));

  // Set permissions on the file so that any user can overwrite it.
  chmod(log_filename, 0666);

#if defined(HAVE_SED)
  // Add three spaces before each "<br>" and axe the "<br>".  This
  // is so that Base-91 compressed packets with no comment and no
  // speed/course/altitude will get decoded ok.  Otherwise the
  // spaces that are critical to the Base-91 packets won't be
  // there due to findu.com filtering them out.
  //
// "sed -i -e \"s/<br>/   <br>/\" %s",
  sprintf(sys_cmd,
          "%s -i -e \"s/<br>/   /\" %s",
          SED_PATH,
          log_filename);
  if (system(sys_cmd))
  {
    fprintf(stderr,"Couldn't execute command: %s\n", sys_cmd);
  }

// Greater-than symbol '>'
  sprintf(sys_cmd,
          "%s -i -e \"s/&gt;/>/\" %s",
          SED_PATH,
          log_filename);
  if (system(sys_cmd))
  {
    fprintf(stderr,"Couldn't execute command: %s\n", sys_cmd);
  }

// Less-than symbol '<'
  sprintf(sys_cmd,
          "%s -i -e \"s/&lt;/</\" %s",
          SED_PATH,
          log_filename);
  if (system(sys_cmd))
  {
    fprintf(stderr,"Couldn't execute command: %s\n", sys_cmd);
  }

// Ampersand '&' (A difficult character to escape from the shell!)
  sprintf(sys_cmd,
          "%s -i -e \"s/&amp;/\\&/\" %s",
          SED_PATH,
          log_filename);
  if (system(sys_cmd))
  {
    fprintf(stderr,"Couldn't execute command: %s\n", sys_cmd);
  }

// Double-quote symbol '"'
  sprintf(sys_cmd,
          "%s -i -e \"s/&quot;/""/\" %s",
          SED_PATH,
          log_filename);
  if (system(sys_cmd))
  {
    fprintf(stderr,"Couldn't execute command: %s\n", sys_cmd);
  }

// Remove whitespace at the start of a line
// sed 's/^[ \t]*//'
  sprintf(sys_cmd,
          "%s -i -e \"s/^[ \t]*//\" %s",
          SED_PATH,
          log_filename);
  if (system(sys_cmd))
  {
    fprintf(stderr,"Couldn't execute command: %s\n", sys_cmd);
  }

// Remove any lines that start with '<'
  sprintf(sys_cmd,
          "%s -i -e \"s/^<.*$//\" %s",
          SED_PATH,
          log_filename);
  if (system(sys_cmd))
  {
    fprintf(stderr,"Couldn't execute command: %s\n", sys_cmd);
  }

// Remove any lines that start with '"http'
  sprintf(sys_cmd,
          "%s -i -e \"/^\\\"http.*$/d\" %s",
          SED_PATH,
          log_filename);
  if (system(sys_cmd))
  {
    fprintf(stderr,"Couldn't execute command: %s\n", sys_cmd);
  }

// Remove any blank lines from the file
// sed '/^$/d'
  sprintf(sys_cmd,
          "%s -i -e \"/^$/d\" %s",
          SED_PATH,
          log_filename);
  if (system(sys_cmd))
  {
    fprintf(stderr,"Couldn't execute command: %s\n", sys_cmd);
  }

#endif  // HAVE_SED

  /*
  #ifdef HAVE_HTML2TEXT
      // Create temp filename
      xastir_snprintf(log_filename_tmp, sizeof(log_filename_tmp), "%s%s",
          log_filename,
          ".tmp");
      // Create html2text command
      sprintf(sys_cmd,
          "%s -width 300 -o %s %s",
          HTML2TEXT_PATH,
          log_filename_tmp,
          log_filename);
      // Convert the newly-downloaded file from html to text format
      (void)system(sys_cmd);
      // Rename the file so that we can keep the static char* name
      // pointing to it, needed for the read_file_ptr code in the main
      // thread.
  #if defined(HAVE_MV)
      sprintf(sys_cmd,
          "%s %s %s",
          MV_PATH,
          log_filename_tmp,
          log_filename);
      (void)system(sys_cmd);
  #endif  // HAVE_MV
  #endif  // HAVE_HTML2TEXT
  */

  // Here we do the findu stuff, if the findu_flag is set.  Else we do an imagemap.
  // We have the log data we're interested in within the log_filename file.
  // Cause that file to be read by the "File->Open Log File" routine.  HTML
  // tags will be ignored just fine.
  read_file_ptr = fopen(log_filename, "r");

  if (read_file_ptr != NULL)
  {
    read_file = 1;
  }
  else
  {
    fprintf(stderr,"Couldn't open file: %s\n", log_filename);
  }

  // Reset global "busy" variable
  fetching_findu_trail_now = 0;

  // End the thread
  return(NULL);
}





void Download_trail_destroy_shell( Widget UNUSED(widget), XtPointer clientData, XtPointer UNUSED(callData) )
{
  Widget shell = (Widget) clientData;
  XtPopdown(shell);

  begin_critical_section(&download_findu_dialog_lock, "track_gui.c:Download_trail_destroy_shell" );

  XtDestroyWidget(shell);
  download_findu_dialog = (Widget)NULL;

  end_critical_section(&download_findu_dialog_lock, "track_gui.c:Download_trail_destroy_shell" );
}





void Download_trail_now(Widget w, XtPointer clientData, XtPointer callData)
{
  char temp[MAX_CALLSIGN+1];
  static char fileimg[400];
  static char log_filename[200];
  char *temp_ptr;
  pthread_t download_trail_thread;
  static XtPointer download_client_data = NULL;

  char tmp_base_dir[MAX_VALUE];

  get_user_base_dir("tmp",tmp_base_dir, sizeof(tmp_base_dir));

  // If we're already fetching a trail, we shouldn't be calling
  // this callback function.  Get out.
  if (fetching_findu_trail_now)
  {
    return;
  }

  // Pass two parameters to findu_transfer_thread via a struct
  track_ptrs.download_client_ptrs[0] = log_filename;
  track_ptrs.download_client_ptrs[1] = fileimg;
  download_client_data = &track_ptrs;

  // Check whether it's ok to do a download currently.
  if (read_file)
  {
    // No, we're already in the middle of reading in some file.
    // Skip trying to download yet another file.

    fprintf(stderr,"Processing another file.  Wait a bit, then try again\n");

    popup_message_always(langcode("POPEM00035"),
                         langcode("POPEM00041"));

    return;
  }

//    busy_cursor(appshell);

  strcpy(log_filename, tmp_base_dir);
  log_filename[sizeof(log_filename)-1] = '\0';  // Terminate string
  strcat(log_filename, "/map.log");
  log_filename[sizeof(log_filename)-1] = '\0';  // Terminate string

  // Erase any previously existing local file by the same name.
  // This avoids the problem of having an old tracklog here and
  // the code trying to display it when the download fails.
  unlink( log_filename );


  XmScaleGetValue(posit_start_value, &posit_start);
  XmScaleGetValue(posit_length_value, &posit_length);

  temp_ptr = XmTextFieldGetString(download_trail_station_data);
  xastir_snprintf(temp,
                  sizeof(temp),
                  "%s",
                  temp_ptr);
  XtFree(temp_ptr);

  (void)remove_trailing_spaces(temp);
  (void)remove_trailing_dash_zero(temp);

  xastir_snprintf(download_trail_station_call,
                  sizeof(download_trail_station_call),
                  "%s",
                  temp);
  //Download_trail_destroy_shell(w, clientData, callData);


// New URL's for findu.  The second one looks very promising.
//http://www.findu.com/cgi-bin/raw.cgi?call=k4hg-8&time=1
//http://www.findu.com/cgi-bin/rawposit.cgi?call=k4hg-8&time=1
//
// The last adds this to the beginning of the line:
//
//      "20030619235323,"
//
// which is a date/timestamp.  We'll need to do some extra stuff
// here in order to actually use that date/timestamp though.
// Setting the read_file_ptr to the downloaded file won't do it.



//        "http://www.findu.com/cgi-bin/rawposit.cgi?call=%s&start=%d&length=%d&time=1", // New, with timestamp
  xastir_snprintf(fileimg, sizeof(fileimg),
                  //
                  // Posits only:
                  // "http://www.findu.com/cgi-bin/rawposit.cgi?call=%s&start=%d&length=%d",
                  //
                  // Posits plus timestamps (we can't handle timestamps yet):
                  // "http://www.findu.com/cgi-bin/rawposit.cgi?call=%s&start=%d&length=%d&time=1", // New, with timestamp
                  //
                  // Download all packets, not just posits:
                  "http://www.findu.com/cgi-bin/raw.cgi?call=%s&start=%d&length=%d",
                  //
                  download_trail_station_call,posit_start,posit_length);

  if (debug_level & 1024)
  {
    fprintf(stderr, "%s\n", fileimg);
  }


//----- Start New Thread -----

  if (pthread_create(&download_trail_thread,
                     NULL,
                     findu_transfer_thread,
                     download_client_data))
  {

    fprintf(stderr,"Error creating findu transfer thread\n");
  }
  else
  {
    // We're off and running with the new thread!
  }

  display_zoom_status();

  Download_trail_destroy_shell(w, clientData, callData);
}





void Reset_posit_length_max(Widget UNUSED(w), XtPointer UNUSED(clientData), XtPointer UNUSED(callData) )
{

  XmScaleGetValue(posit_length_value, &posit_length);
  XmScaleGetValue(posit_start_value, &posit_start);

  // Check whether start hours is greater than max findu allows
  // for duration
  //
  if (posit_start > MAX_FINDU_DURATION)      // Set the duration slider to
  {
    // findu's max duration hours

    XtVaSetValues(posit_length_value,
                  XmNvalue, MAX_FINDU_DURATION,
                  NULL);
    posit_length = MAX_FINDU_DURATION;
  }
  else    // Not near the max, so set the duration slider to match
  {
    // the start hours

    XtVaSetValues(posit_length_value,
                  XmNvalue, posit_start,
                  NULL);
    posit_length = posit_start;
  }
}





void Download_findu_trail( Widget UNUSED(w), XtPointer UNUSED(clientData), XtPointer UNUSED(callData) )
{
  static Widget pane, my_form, button_ok, button_cancel, call, sep;
  Atom delw;
  XmString x_str;


  if (!download_findu_dialog)
  {

    begin_critical_section(&download_findu_dialog_lock, "track_gui.c:Download_findu_trail" );

    download_findu_dialog = XtVaCreatePopupShell(langcode("WPUPTSP007"),
                            xmDialogShellWidgetClass, appshell,
                            XmNdeleteResponse,XmDESTROY,
                            XmNdefaultPosition, FALSE,
                            XmNfontList, fontlist1,
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
                                   XmNfontList, fontlist1,
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
                                  XmNfontList, fontlist1,
                                  NULL);

    x_str = XmStringCreateLocalized(langcode("WPUPTSP009"));
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
                        XmNmaximum, MAX_FINDU_START_TIME,
                        XmNshowValue, TRUE,
                        XmNvalue, posit_start,
// Note:  Some versions of OpenMotif (distributed with Fedora,
// perhaps others) don't work properly with XtVaTypedArg() as used
// here, instead showing blank labels for the Scale widgets.
//                XtVaTypedArg, XmNtitleString, XmRString, langcode("WPUPTSP009"), 22,
                        XmNtitleString, x_str,
                        MY_FOREGROUND_COLOR,
                        MY_BACKGROUND_COLOR,
                        XmNfontList, fontlist1,
                        NULL);
    XmStringFree(x_str);

    x_str = XmStringCreateLocalized(langcode("WPUPTSP010"));
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
                         XmNmaximum, MAX_FINDU_DURATION,
                         XmNshowValue, TRUE,
                         XmNvalue, posit_length,
// Note:  Some versions of OpenMotif (distributed with Fedora,
// perhaps others) don't work properly with XtVaTypedArg() as used
// here, instead showing blank labels for the Scale widgets.
//                XtVaTypedArg, XmNtitleString, XmRString, langcode("WPUPTSP010"), 19,
                         XmNtitleString, x_str,
                         MY_FOREGROUND_COLOR,
                         MY_BACKGROUND_COLOR,
                         XmNfontList, fontlist1,
                         NULL);
    XmStringFree(x_str);

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
                                  XmNfontList, fontlist1,
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
                                        XmNfontList, fontlist1,
                                        NULL);
    if (fetching_findu_trail_now)
    {
      XtSetSensitive(button_ok, FALSE);
    }

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
                                            XmNfontList, fontlist1,
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

  }
  else
  {
    (void)XRaiseWindow(XtDisplay(download_findu_dialog), XtWindow(download_findu_dialog));
  }
}


