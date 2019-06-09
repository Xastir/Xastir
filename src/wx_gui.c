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
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifdef  HAVE_LOCALE_H
  #include <locale.h>
#endif  // HAVE_LOCALE_H

#ifdef HAVE_MATH_H
  #include <math.h>
#endif  // HAVE_MATH_H

#include "xastir.h"
#include "wx.h"
#include "main.h"
#include "alert.h"
#include "lang.h"

#include <Xm/XmAll.h>
#ifdef HAVE_XBAE_MATRIX_H
  #include <Xbae/Matrix.h>
#endif  // HAVE_XBAE_MATRIX_H
#include <X11/cursorfont.h>

// Must be last include file
#include "leak_detection.h"


extern XmFontList fontlist1;    // Menu/System fontlist

/************ Weather Alerts ****************/
Widget wx_alert_shell = (Widget)NULL;
Widget wx_detailed_alert_shell = (Widget)NULL;
static Widget wx_alert_list;

static xastir_mutex wx_alert_shell_lock;
static xastir_mutex wx_detailed_alert_shell_lock;
static xastir_mutex wx_station_dialog_lock;





void wx_gui_init(void)
{
  init_critical_section( &wx_alert_shell_lock );
  init_critical_section( &wx_detailed_alert_shell_lock );
  init_critical_section( &wx_station_dialog_lock );
}





void wx_detailed_alert_destroy_shell( Widget UNUSED(widget), XtPointer clientData, XtPointer UNUSED(callData) )
{

  Widget shell = (Widget) clientData;
  XtPopdown(shell);

  begin_critical_section(&wx_detailed_alert_shell_lock, "wx_gui.c:wx_detailed_alert_destroy_shell" );

  XtDestroyWidget(shell);
  wx_detailed_alert_shell = (Widget)NULL;

  end_critical_section(&wx_detailed_alert_shell_lock, "wx_gui.c:wx_detailed_alert_destroy_shell" );

}





// This gets/displays the "finger" output from the WXSVR.  Called
// from both the Station Info "NWS" button and by double-clicking on
// the view weather alerts window.
//
void wx_alert_finger_output( Widget UNUSED(widget), char *handle)
{
  static Widget pane, my_form, mess, button_cancel,wx_detailed_alert_list;
  Atom delw;
  Arg al[50];             // Arg List
  unsigned int ac = 0;    // Arg Count
  char temp[1024];
  XmString item;
  FILE *fd;
  int ret;
  int server_fd;
  struct sockaddr_in serv_addr;
  struct hostent *serverhost;


  if (debug_level & 1)
  {
    fprintf(stderr,"Handle: %s\n",handle);
  }

  if(!wx_detailed_alert_shell)
  {

    begin_critical_section(&wx_detailed_alert_shell_lock, "wx_gui.c:wx_alert_double_click_action" );

    wx_detailed_alert_shell = XtVaCreatePopupShell(langcode("WPUPWXA001"),
                              xmDialogShellWidgetClass, appshell,
                              XmNdeleteResponse, XmDESTROY,
                              XmNdefaultPosition, FALSE,
                              XmNminWidth, 600,
                              XmNfontList, fontlist1,
                              NULL);

    pane = XtVaCreateWidget("wx_alert_double_click_action pane",xmPanedWindowWidgetClass, wx_detailed_alert_shell,
                            XmNbackground, colors[0xff],
                            NULL);

    my_form =  XtVaCreateWidget("wx_alert_double_click_action my_form", xmFormWidgetClass, pane,
                                XmNtraversalOn, TRUE,
                                XmNfractionBase, 5,
                                XmNbackground, colors[0xff],
                                XmNwidth, 600,
                                XmNautoUnmanage, FALSE,
                                XmNshadowThickness, 1,
                                NULL);

    mess = XtVaCreateManagedWidget(langcode("WPUPWXA002"), xmLabelWidgetClass, my_form,
                                   XmNtraversalOn, FALSE,
                                   XmNtopAttachment, XmATTACH_FORM,
                                   XmNtopOffset, 5,
                                   XmNbottomAttachment, XmATTACH_NONE,
                                   XmNleftAttachment, XmATTACH_FORM,
                                   XmNleftOffset, 5,
                                   XmNrightAttachment, XmATTACH_FORM,
                                   XmNrightOffset, 5,
                                   XmNbackground, colors[0xff],
                                   XmNfontList, fontlist1,
                                   NULL);


    /* set args for color */
    ac=0;
    XtSetArg(al[ac], XmNbackground, colors[0xff]);
    ac++;
    XtSetArg(al[ac], XmNvisibleItemCount, 13);
    ac++;
    XtSetArg(al[ac], XmNtraversalOn, TRUE);
    ac++;
    XtSetArg(al[ac], XmNshadowThickness, 3);
    ac++;
    XtSetArg(al[ac], XmNselectionPolicy, XmSINGLE_SELECT);
    ac++;

    XtSetArg(al[ac], XmNvisualPolicy, XmCONSTANT);
    ac++;
    XtSetArg(al[ac], XmNscrollingPolicy,XmAUTOMATIC);
    ac++;
    XtSetArg(al[ac], XmNscrollBarPlacement, XmBOTTOM_RIGHT);
    ac++;
    XtSetArg(al[ac], XmNscrollBarDisplayPolicy,XmAS_NEEDED);
    ac++;
    XtSetArg(al[ac], XmNlistSizePolicy, XmCONSTANT);
    ac++;

    XtSetArg(al[ac], XmNtopAttachment, XmATTACH_WIDGET);
    ac++;
    XtSetArg(al[ac], XmNtopWidget, mess);
    ac++;
    XtSetArg(al[ac], XmNtopOffset, 5);
    ac++;
    XtSetArg(al[ac], XmNbottomAttachment, XmATTACH_FORM);
    ac++;
    XtSetArg(al[ac], XmNbottomOffset, 45);
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

    wx_detailed_alert_list = XmCreateScrolledList(my_form, "wx_alert_double_click_action wx_detailed_alert_list", al, ac);

    button_cancel = XtVaCreateManagedWidget(langcode("UNIOP00003"),xmPushButtonGadgetClass, my_form,
                                            XmNtopAttachment, XmATTACH_NONE,
                                            XmNbottomAttachment, XmATTACH_FORM,
                                            XmNbottomOffset,10,
                                            XmNleftAttachment, XmATTACH_POSITION,
                                            XmNleftPosition, 2,
                                            XmNrightAttachment, XmATTACH_POSITION,
                                            XmNrightPosition, 3,
                                            XmNbackground, colors[0xff],
                                            XmNnavigationType, XmTAB_GROUP,
                                            XmNfontList, fontlist1,
                                            NULL);

    XtAddCallback(button_cancel, XmNactivateCallback, wx_detailed_alert_destroy_shell, wx_detailed_alert_shell);

    end_critical_section(&wx_detailed_alert_shell_lock, "wx_gui.c:wx_alert_double_click_action" );

    pos_dialog(wx_detailed_alert_shell);

    delw = XmInternAtom(XtDisplay(wx_detailed_alert_shell), "WM_DELETE_WINDOW", FALSE);
    XmAddWMProtocolCallback(wx_detailed_alert_shell, delw, wx_detailed_alert_destroy_shell, (XtPointer)wx_detailed_alert_shell);

    XtManageChild(my_form);
    XtManageChild(wx_detailed_alert_list);
    XtVaSetValues(wx_detailed_alert_list, XmNbackground, colors[0x0f], NULL);
    XtManageChild(pane);

    XtPopup(wx_detailed_alert_shell, XtGrabNone);
//        fix_dialog_vsize(wx_detailed_alert_shell);

    // Move focus to the Cancel button.  This appears to highlight the
    // button fine, but we're not able to hit the <Enter> key to
    // have that default function happen.  Note:  We _can_ hit the
    // <SPACE> key, and that activates the option.
//        XmUpdateDisplay(wx_detailed_alert_shell);
    XmProcessTraversal(button_cancel, XmTRAVERSE_CURRENT);
  }
  else
  {
    (void)XRaiseWindow(XtDisplay(wx_detailed_alert_shell), XtWindow(wx_detailed_alert_shell));
  }

  // Erase the entire list before we start writing to it in
  // case it was left up from a previous query.
  XmListDeleteAllItems(wx_detailed_alert_list);

  // Perform a "finger" command, which is really just a telnet
  // with a single line command sent to the remote host, then a
  // bunch of text sent back.  We implement it here via our own
  // TCP code, as it's really very simple.

  // Allocate a socket for our use
  if ((server_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
  {
//        fprintf(stderr,"wx_alert_finger_output: can't get socket\n");
    xastir_snprintf(temp,
                    sizeof(temp),
                    "wx_alert_finger_output: can't get socket");
    item = XmStringGenerate(temp, XmFONTLIST_DEFAULT_TAG, XmCHARSET_TEXT, NULL);
    XmListAddItemUnselected(wx_detailed_alert_list, item, 0);
    XmStringFree(item);
    return;
  }

  memset(&serv_addr, 0, sizeof(serv_addr));


  // Changing Finger host because WXSVR.net has been down for a
  // month or more and Pete, AE5PL, has a replacement online that
  // performs this function.
  //serverhost = gethostbyname("wxsvr.net");
  serverhost = gethostbyname("wx.aprs-is.net");


  if (serverhost == (struct hostent *)0)
  {
//        fprintf(stderr,"wx_alert_finger_output: gethostbyname failed\n");
    xastir_snprintf(temp,
                    sizeof(temp),
                    "wx_alert_finger_output: gethostbyname failed");
    item = XmStringGenerate(temp, XmFONTLIST_DEFAULT_TAG, XmCHARSET_TEXT, NULL);
    XmListAddItemUnselected(wx_detailed_alert_list, item, 0);
    XmStringFree(item);
    (void)close(server_fd); // Close the socket
    return;
  }
  memmove(&serv_addr.sin_addr,serverhost->h_addr, (size_t)serverhost->h_length);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(79); // Finger protocol uses port 79

  if (connect(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) != 0)
  {
//        fprintf(stderr,"wx_alert_finger_output: connect to server failed\n");
    xastir_snprintf(temp,
                    sizeof(temp),
                    "wx_alert_finger_output: connect to server failed");
    item = XmStringGenerate(temp, XmFONTLIST_DEFAULT_TAG, XmCHARSET_TEXT, NULL);
    XmListAddItemUnselected(wx_detailed_alert_list, item, 0);
    XmStringFree(item);
    (void)close(server_fd);    // Close the socket
    return;
  }

  // Create a file descriptor for the socket
  fd = fdopen(dup(server_fd),"wb");
  if (fd == NULL)
  {
//        fprintf(stderr,"Couldn't create duplicate write socket\n");
    xastir_snprintf(temp,
                    sizeof(temp),
                    "Couldn't create duplicate write socket");
    item = XmStringGenerate(temp, XmFONTLIST_DEFAULT_TAG, XmCHARSET_TEXT, NULL);
    XmListAddItemUnselected(wx_detailed_alert_list, item, 0);
    XmStringFree(item);
    (void)close(server_fd); // Close the socket
    return;
  }

  // Set up the text we're going to send to the remote finger
  // server.
  xastir_snprintf(temp,
                  sizeof(temp),
                  "%s\r\n",
                  handle);

  // Send the request text out the socket
  ret = fprintf(fd, "%s", temp);

  if (ret == 0 || ret == -1)
  {
//        fprintf(stderr,"Couldn't send finger command to wxsvr\n");
    xastir_snprintf(temp,
                    sizeof(temp),
                    "Couldn't send finger command to wxsvr");
    item = XmStringGenerate(temp, XmFONTLIST_DEFAULT_TAG, XmCHARSET_TEXT, NULL);
    XmListAddItemUnselected(wx_detailed_alert_list, item, 0);
    XmStringFree(item);
    (void)fclose(fd);
    (void)close(server_fd); // Close the socket
    return;
  }

  // Close the duplicate port we used for writing
  (void)fclose(fd);

//
// Read back the results from the socket
//

  // Create a file descriptor for the socket
  fd = fdopen(dup(server_fd),"rb");
  if (fd == NULL)
  {
//        fprintf(stderr,"Couldn't create duplicate read socket\n");
    xastir_snprintf(temp,
                    sizeof(temp),
                    "Couldn't create duplicate read socket");
    item = XmStringGenerate(temp, XmFONTLIST_DEFAULT_TAG, XmCHARSET_TEXT, NULL);
    XmListAddItemUnselected(wx_detailed_alert_list, item, 0);
    XmStringFree(item);
    (void)close(server_fd); // Close the socket
    return;
  }

  // Process the data we received from the remote finger server
  //
  while (fgets (temp, sizeof (temp), fd))     // While we have data to process
  {
    char *ptr;

    // Remove any linefeeds or carriage returns from each
    // string.
    ptr = temp;
    while ( (ptr = strpbrk(temp, "\n\r")) )
    {
      memmove(ptr, ptr+1, strlen(ptr)+1);
    }

    if (debug_level & 1)
    {
      fprintf(stderr,"%s\n",temp);
    }

    // Create an XmString for each line and add it to the
    // end of the list.
    item = XmStringGenerate(temp, XmFONTLIST_DEFAULT_TAG, XmCHARSET_TEXT, NULL);
    XmListAddItemUnselected(wx_detailed_alert_list, item, 0);
    XmStringFree(item);
  }

  // All done!
  fclose(fd);
  (void)close(server_fd); // Close the socket
}





void wx_alert_double_click_action( Widget widget, XtPointer UNUSED(clientData), XtPointer callData)
{
  char *choice;
  XmListCallbackStruct *selection = callData;
  char handle[14];
  char *ptr;


  XmStringGetLtoR(selection->item, XmFONTLIST_DEFAULT_TAG, &choice);
  //fprintf(stderr,"Selected item %d (%s)\n", selection->item_position, choice);

  // Grab the first 13 characters.  Remove spaces.  This is our handle
  // into the weather server for the full weather alert text.

  xastir_snprintf(handle,
                  sizeof(handle),
                  "%s",
                  choice);

  XtFree(choice);    // Release as soon as we're done!

  handle[13] = '\0';  // Terminate the string
  // Remove spaces
  ptr = handle;
  while ( (ptr = strpbrk(handle, " ")) )
  {
    memmove(ptr, ptr+1, strlen(ptr)+1);
  }
  handle[9] = '\0';   // Terminate after first 9 chars

  if (debug_level & 1)
  {
    fprintf(stderr,"Handle: %s\n",handle);
  }

  wx_alert_finger_output( widget, handle);
}





void wx_alert_destroy_shell( Widget UNUSED(widget), XtPointer clientData, XtPointer UNUSED(callData) )
{

  Widget shell = (Widget) clientData;
  XtPopdown(shell);

  begin_critical_section(&wx_alert_shell_lock, "wx_gui.c:wx_alert_destroy_shell" );

  XtDestroyWidget(shell);
  wx_alert_shell = (Widget)NULL;

  end_critical_section(&wx_alert_shell_lock, "wx_gui.c:wx_alert_destroy_shell" );

}


static int alert_comp(const void *a, const void *b)
{
  alert_entry *a_entry = *(alert_entry **)a;
  alert_entry *b_entry = *(alert_entry **)b;
  int a_active, b_active;

  if (a_entry->title[on_screen] && !b_entry->title[on_screen])
  {
    return (-1);
  }

  if (!a_entry->title[0] && b_entry->title[0])
  {
    return (1);
  }

  if (a_entry->flags[on_screen] == 'Y' && b_entry->flags[on_screen] != 'Y')
  {
    return (-1);
  }

  if (a_entry->flags[on_screen] != 'Y' && b_entry->flags[on_screen] == 'Y')
  {
    return (1);
  }

  if (a_entry->flags[on_screen] == '?' && b_entry->flags[on_screen] == 'N')
  {
    return (-1);
  }

  if (a_entry->flags[on_screen] == 'N' && b_entry->flags[on_screen] == '?')
  {
    return (1);
  }

  a_active = alert_active(a_entry, ALERT_ALL);
  b_active = alert_active(b_entry, ALERT_ALL);
  if (a_active && b_active)
  {
    if (a_active - b_active)
    {
      return (a_active - b_active);
    }
  }
  else if (a_active)
  {
    return (-1);
  }
  else if (b_active)
  {
    return (1);
  }

  return (strcmp(a_entry->title, b_entry->title));
}


void wx_alert_update_list(void)
{
  int nn;         // index into alert table.  Starts at 0
  int ii;         // index into dialog lines.  Starts at 1
  int max_item_count; // max dialog lines
  char temp[600];
  XmString item;
  static alert_entry **alert_list;
  static int alert_list_limit;

  if (wx_alert_shell)
  {
    struct hashtable_itr *iterator;
    alert_entry *alert;

    begin_critical_section(&wx_alert_shell_lock, "wx_gui.c:wx_alert_update_list" );

    // Get the previous alert count from the alert list window
    XtVaGetValues(wx_alert_list, XmNitemCount, &max_item_count, NULL);

    if ((nn = alert_list_count()) > alert_list_limit)
    {
      alert_entry **tmp = realloc(alert_list, nn * sizeof(alert_entry *));
      if (tmp)
      {
        alert_list = tmp;
        alert_list_limit = nn;
      }
      else
      {
        fprintf(stderr, "wx_gui: Alert list allocation error\n");
        exit(1);
      }

    }
    // Iterate through the alert hash.  Create a string for each
    // non-expired/non-blank entry.
    iterator = create_wx_alert_iterator();
    for (nn = 0, alert = get_next_wx_alert(iterator); iterator != NULL && alert; alert = get_next_wx_alert(iterator))
    {

      // Check whether alert record is empty/filled.  This
      // code is from the earlier array implementation.  If
      // we're expiring records from our hash properly we
      // probably don't need this anymore.
      //
//            if (alert->title[0] == '\0') {    // It's empty
//        fprintf(stderr, "wx_gui:alert->title NULL\n");
//                break;
//            }
      alert_list[nn++] = alert;
    }
    qsort(alert_list, nn, sizeof(alert_entry *), alert_comp);
    for (ii = 0; ii < nn;
        )
    {
      alert = alert_list[ii];
      // AFGNPW      NWS-WARN    Until: 191500z   AK_Z213   WIND               P7IAA
      // TSATOR      NWS-ADVIS   Until: 190315z   OK_C127   TORNDO             H2VAA
      //xastir_snprintf(temp, sizeof(temp), "%-9s   %-9s   Until: %-7s   %-7s   %-20s   %s",

      xastir_snprintf(temp,
                      sizeof(temp),
                      "%-9s %-5s %-9s %c%c @%c%c%c%cz ==> %c%c @%c%c%c%cz %c%c %-7s %s %s%s%s%s",
                      alert->from,
                      alert->seq,
                      alert->to,
                      alert->issue_date_time[0],
                      alert->issue_date_time[1],
                      alert->issue_date_time[2],
                      alert->issue_date_time[3],
                      alert->issue_date_time[4],
                      alert->issue_date_time[5],
                      alert->activity[0],
                      alert->activity[1],
                      alert->activity[2],
                      alert->activity[3],
                      alert->activity[4],
                      alert->activity[5],
                      alert->flags[on_screen],
                      alert->flags[source],
                      alert->title,
                      alert->alert_tag,
                      alert->desc0,
                      alert->desc1,
                      alert->desc2,
                      alert->desc3);

      item = XmStringGenerate(temp, XmFONTLIST_DEFAULT_TAG, XmCHARSET_TEXT, NULL);

      ii++;

      // It looks like if we are higher than 'max_item_count',
      // it must be a new entry that we haven't written to the
      // window yet.  Add it.
      if (max_item_count < ii)
      {
        XmListAddItemUnselected(wx_alert_list, item, 0);
      }
      else
      {
        // Replace it in the window.  Note: This might re-order the list each time.
        XmListReplaceItemsPos(wx_alert_list, &item, 1, ii);
      }

      XmStringFree(item);

    }   // End of for loop
#ifndef USING_LIBGC
//fprintf(stderr,"free iterator 9\n");
    if (iterator)
    {
      free(iterator);
    }
#endif  // USING_LIBGC

    // If we have fewer alerts now, delete the extras from the window
    if (ii < max_item_count)
    {
      XmListDeleteItemsPos(wx_alert_list, max_item_count - ii, ii+1);
    }

    end_critical_section(&wx_alert_shell_lock, "wx_gui.c:wx_alert_update_list" );

  }
}





void Display_Wx_Alert( Widget UNUSED(wdgt), XtPointer UNUSED(clientData), XtPointer UNUSED(callData) )
{
  static Widget pane, my_form, mess, button_cancel;
  Atom delw;
  Arg al[50];                    /* Arg List */
  unsigned int ac = 0;           /* Arg Count */

  if(!wx_alert_shell)
  {

    begin_critical_section(&wx_alert_shell_lock, "wx_gui.c:Display_Wx_Alert" );

    wx_alert_shell = XtVaCreatePopupShell(langcode("WPUPWXA001"),
                                          xmDialogShellWidgetClass, appshell,
                                          XmNdeleteResponse, XmDESTROY,
                                          XmNdefaultPosition, FALSE,
                                          XmNminWidth, 600,
                                          XmNfontList, fontlist1,
                                          NULL);

    pane = XtVaCreateWidget("Display_Wx_Alert pane",xmPanedWindowWidgetClass, wx_alert_shell,
                            XmNbackground, colors[0xff],
                            NULL);

    my_form =  XtVaCreateWidget("Display_Wx_Alert my_form", xmFormWidgetClass, pane,
                                XmNtraversalOn, TRUE,
                                XmNfractionBase, 5,
                                XmNbackground, colors[0xff],
                                XmNwidth, 600,
                                XmNautoUnmanage, FALSE,
                                XmNshadowThickness, 1,
                                NULL);

    mess = XtVaCreateManagedWidget(langcode("WPUPWXA002"), xmLabelWidgetClass, my_form,
                                   XmNtraversalOn, FALSE,
                                   XmNtopAttachment, XmATTACH_FORM,
                                   XmNtopOffset, 5,
                                   XmNbottomAttachment, XmATTACH_NONE,
                                   XmNleftAttachment, XmATTACH_FORM,
                                   XmNleftOffset, 5,
                                   XmNrightAttachment, XmATTACH_FORM,
                                   XmNrightOffset, 5,
                                   XmNbackground, colors[0xff],
                                   XmNfontList, fontlist1,
                                   NULL);


    /* set args for color */
    ac=0;
    XtSetArg(al[ac], XmNbackground, colors[0xff]);
    ac++;
    XtSetArg(al[ac], XmNvisibleItemCount, 13);
    ac++;
    XtSetArg(al[ac], XmNtraversalOn, TRUE);
    ac++;
    XtSetArg(al[ac], XmNshadowThickness, 3);
    ac++;
//        XtSetArg(al[ac], XmNselectionPolicy, XmMULTIPLE_SELECT); ac++;
    XtSetArg(al[ac], XmNselectionPolicy, XmSINGLE_SELECT);
    ac++;

    XtSetArg(al[ac], XmNvisualPolicy, XmCONSTANT);
    ac++;
    XtSetArg(al[ac], XmNscrollingPolicy,XmAUTOMATIC);
    ac++;
    XtSetArg(al[ac], XmNscrollBarPlacement, XmBOTTOM_RIGHT);
    ac++;
    XtSetArg(al[ac], XmNscrollBarDisplayPolicy,XmAS_NEEDED);
    ac++;
//        XtSetArg(al[ac], XmNscrollBarDisplayPolicy, XmSTATIC); ac++;
    XtSetArg(al[ac], XmNlistSizePolicy, XmCONSTANT);
    ac++;
//        XtSetArg(al[ac], XmNlistSizePolicy, XmVARIABLE); ac++;

    XtSetArg(al[ac], XmNtopAttachment, XmATTACH_WIDGET);
    ac++;
    XtSetArg(al[ac], XmNtopWidget, mess);
    ac++;
    XtSetArg(al[ac], XmNtopOffset, 5);
    ac++;
    XtSetArg(al[ac], XmNbottomAttachment, XmATTACH_FORM);
    ac++;
    XtSetArg(al[ac], XmNbottomOffset, 45);
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

    wx_alert_list = XmCreateScrolledList(my_form, "Display_Wx_Alert wx_alert_list", al, ac);

    end_critical_section(&wx_alert_shell_lock, "wx_gui.c:Display_Wx_Alert" );

    wx_alert_update_list();

    button_cancel = XtVaCreateManagedWidget(langcode("UNIOP00003"),xmPushButtonGadgetClass, my_form,
                                            XmNtopAttachment, XmATTACH_NONE,
//                        XmNtopOffset, 265,
                                            XmNbottomAttachment, XmATTACH_FORM,
                                            XmNbottomOffset,10,
                                            XmNleftAttachment, XmATTACH_POSITION,
                                            XmNleftPosition, 2,
                                            XmNrightAttachment, XmATTACH_POSITION,
                                            XmNrightPosition, 3,
                                            XmNbackground, colors[0xff],
                                            XmNnavigationType, XmTAB_GROUP,
                                            XmNfontList, fontlist1,
                                            NULL);

    XtAddCallback(button_cancel, XmNactivateCallback, wx_alert_destroy_shell, wx_alert_shell);
    XtAddCallback(wx_alert_list, XmNdefaultActionCallback, wx_alert_double_click_action, NULL);

    pos_dialog(wx_alert_shell);

    delw = XmInternAtom(XtDisplay(wx_alert_shell), "WM_DELETE_WINDOW", FALSE);
    XmAddWMProtocolCallback(wx_alert_shell, delw, wx_alert_destroy_shell, (XtPointer)wx_alert_shell);

    XtManageChild(my_form);
    XtManageChild(wx_alert_list);
    XtVaSetValues(wx_alert_list, XmNbackground, colors[0x0f], NULL);
    XtManageChild(pane);

    XtPopup(wx_alert_shell, XtGrabNone);
//        fix_dialog_vsize(wx_alert_shell);

    // Move focus to the Cancel button.  This appears to highlight the
    // button fine, but we're not able to hit the <Enter> key to
    // have that default function happen.  Note:  We _can_ hit the
    // <SPACE> key, and that activates the option.
//        XmUpdateDisplay(wx_alert_shell);
    XmProcessTraversal(button_cancel, XmTRAVERSE_CURRENT);
  }
  else
  {
    (void)XRaiseWindow(XtDisplay(wx_alert_shell), XtWindow(wx_alert_shell));
  }
} /* Display_Wx_Alert */





/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////





/**** WX Station *******/
Widget wx_station_dialog=(Widget)NULL;
Widget WX_type_data;
Widget WX_temp_data;
Widget WX_wind_cse_data;
Widget WX_wind_spd_data;
Widget WX_wind_gst_data;
Widget WX_rain_data;
Widget WX_to_rain_data;
Widget WX_rain_h_data;
Widget WX_rain_24_data;
Widget WX_humidity_data;
Widget WX_speed_label;
Widget WX_gust_label;
Widget WX_temp_label;
Widget WX_rain_label;
Widget WX_to_rain_label;
Widget WX_rain_h_label;
Widget WX_rain_24_label;
Widget WX_dew_point_data;
Widget WX_high_wind_data;
Widget WX_wind_chill_data;
Widget WX_heat_index_data;
Widget WX_baro_data;
Widget WX_baro_label;
Widget WX_three_hour_baro_data;
Widget WX_three_hour_baro_label;
Widget WX_hi_temp_data;
Widget WX_low_temp_data;
Widget WX_dew_point_label;
Widget WX_wind_chill_label;
Widget WX_heat_index_label;
Widget WX_hi_temp_label;
Widget WX_low_temp_label;
Widget WX_high_wind_label;





void WX_station_destroy_shell( Widget UNUSED(widget), XtPointer clientData, XtPointer UNUSED(callData) )
{

  Widget shell = (Widget) clientData;
  XtPopdown(shell);

  begin_critical_section(&wx_station_dialog_lock, "wx_gui.c:WX_station_destroy_shell" );

  XtDestroyWidget(shell);
  wx_station_dialog = (Widget)NULL;

  end_critical_section(&wx_station_dialog_lock, "wx_gui.c:WX_station_destroy_shell" );

}





void WX_station_change_data(Widget widget, XtPointer clientData, XtPointer callData)
{

  WX_station_destroy_shell(widget,clientData,callData);
}





// This is the "Own Weather Station" Dialog
//
void WX_station( Widget UNUSED(w), XtPointer UNUSED(clientData), XtPointer UNUSED(callData) )
{
  static Widget  pane, my_form, form1, button_close, frame,
         WX_type, temp, wind_cse, wind_spd, wind_gst,
         my_rain, to_rain, rain_h, my_rain_24, baro,
         dew_point,
         high_wind, wind_chill,
         heat_index, three_hour_baro,
         hi_temp;

  Atom delw;

  if(!wx_station_dialog)
  {

    begin_critical_section(&wx_station_dialog_lock, "wx_gui.c:WX_station" );

    wx_station_dialog = XtVaCreatePopupShell(langcode("WXPUPSI000"),
                        xmDialogShellWidgetClass, appshell,
                        XmNdeleteResponse, XmDESTROY,
                        XmNdefaultPosition, FALSE,
                        XmNfontList, fontlist1,
                        NULL);

    pane = XtVaCreateWidget("WX_station pane",xmPanedWindowWidgetClass, wx_station_dialog,
                            XmNbackground, colors[0xff],
                            NULL);

    my_form =  XtVaCreateWidget("WX_station my_form",xmFormWidgetClass, pane,
                                XmNfractionBase,7,
                                XmNbackground, colors[0xff],
                                XmNautoUnmanage, FALSE,
                                XmNshadowThickness, 1,
                                NULL);

    WX_type = XtVaCreateManagedWidget(langcode("WXPUPSI001"),xmLabelWidgetClass, my_form,
                                      XmNtraversalOn, FALSE,
                                      XmNtopAttachment, XmATTACH_FORM,
                                      XmNtopOffset, 10,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset, 10,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      XmNfontList, fontlist1,
                                      NULL);


    WX_type_data = XtVaCreateManagedWidget("WX_station type data", xmTextFieldWidgetClass, my_form,
                                           XmNtraversalOn, FALSE,
                                           XmNeditable,   FALSE,
                                           XmNcursorPositionVisible, FALSE,
                                           XmNsensitive, STIPPLE,
                                           XmNshadowThickness,    1,
                                           XmNcolumns, 70,
                                           XmNtopOffset, 6,
                                           XmNbackground, colors[0x0f],
                                           XmNleftAttachment,XmATTACH_WIDGET,
                                           XmNleftWidget, WX_type,
                                           XmNleftOffset, 5,
                                           XmNtopAttachment,XmATTACH_FORM,
                                           XmNbottomAttachment,XmATTACH_NONE,
                                           XmNrightAttachment,XmATTACH_FORM,
                                           XmNrightOffset, 30,
                                           XmNfontList, fontlist1,
                                           NULL);

    frame = XtVaCreateManagedWidget("WX_station frame", xmFrameWidgetClass, my_form,
                                    XmNtopAttachment,XmATTACH_WIDGET,
                                    XmNtopWidget, WX_type_data,
                                    XmNtopOffset,10,
                                    XmNbottomAttachment,XmATTACH_NONE,
                                    XmNleftAttachment, XmATTACH_FORM,
                                    XmNleftOffset, 10,
                                    XmNrightAttachment,XmATTACH_FORM,
                                    XmNrightOffset, 10,
                                    XmNbackground, colors[0xff],
                                    NULL);

    // sts
    (void)XtVaCreateManagedWidget(langcode("WXPUPSI002"),xmLabelWidgetClass,frame,
                                  XmNtraversalOn, FALSE,
                                  XmNchildType, XmFRAME_TITLE_CHILD,
                                  XmNbackground, colors[0xff],
                                  XmNfontList, fontlist1,
                                  NULL);

    form1 =  XtVaCreateWidget("WX_station form1",xmFormWidgetClass, frame,
                              XmNtraversalOn, FALSE,
                              XmNfractionBase, 7,
                              XmNbackground, colors[0xff],
                              XmNtopAttachment, XmATTACH_FORM,
                              XmNbottomAttachment, XmATTACH_FORM,
                              XmNleftAttachment, XmATTACH_FORM,
                              XmNrightAttachment, XmATTACH_FORM,
                              XmNfontList, fontlist1,
                              NULL);


    wind_cse = XtVaCreateManagedWidget(langcode("WXPUPSI003"),xmLabelWidgetClass, form1,
                                       XmNtopAttachment, XmATTACH_FORM,
                                       XmNtopOffset, 10,
                                       XmNbottomAttachment, XmATTACH_NONE,
                                       XmNleftAttachment, XmATTACH_FORM,
                                       XmNleftOffset, 5,
                                       XmNrightAttachment, XmATTACH_NONE,
                                       XmNbackground, colors[0xff],
                                       XmNfontList, fontlist1,
                                       NULL);


    WX_wind_cse_data = XtVaCreateManagedWidget("WX_station wc data", xmTextFieldWidgetClass, form1,
                       XmNeditable,   FALSE,
                       XmNcursorPositionVisible, FALSE,
                       XmNsensitive, TRUE,
                       XmNshadowThickness,    1,
                       XmNcolumns, 6,
                       XmNmaxLength, 3,
                       XmNtopOffset, 6,
                       XmNbackground, colors[0x0f],
                       XmNleftAttachment, XmATTACH_POSITION,
                       XmNleftPosition, 2,
                       XmNrightAttachment, XmATTACH_NONE,
                       XmNtopAttachment,XmATTACH_FORM,
                       XmNbottomAttachment,XmATTACH_NONE,
                       XmNfontList, fontlist1,
                       NULL);

    // wind_deg
    (void)XtVaCreateManagedWidget(langcode("UNIOP00024"),xmLabelWidgetClass, form1,
                                  XmNtopAttachment, XmATTACH_FORM,
                                  XmNtopOffset, 12,
                                  XmNbottomAttachment, XmATTACH_NONE,
                                  XmNleftAttachment, XmATTACH_WIDGET,
                                  XmNleftWidget, WX_wind_cse_data,
                                  XmNleftOffset, 5,
                                  XmNrightAttachment, XmATTACH_NONE,
                                  XmNbackground, colors[0xff],
                                  XmNfontList, fontlist1,
                                  NULL);

    wind_spd = XtVaCreateManagedWidget(langcode("WXPUPSI004"),xmLabelWidgetClass, form1,
                                       XmNtopAttachment, XmATTACH_WIDGET,
                                       XmNtopWidget, wind_cse,
                                       XmNtopOffset, 11,
                                       XmNbottomAttachment, XmATTACH_NONE,
                                       XmNleftAttachment, XmATTACH_FORM,
                                       XmNleftOffset, 5,
                                       XmNrightAttachment, XmATTACH_NONE,
                                       XmNbackground, colors[0xff],
                                       XmNfontList, fontlist1,
                                       NULL);


    WX_wind_spd_data = XtVaCreateManagedWidget("WX_station ws data", xmTextFieldWidgetClass, form1,
                       XmNeditable,   FALSE,
                       XmNcursorPositionVisible, FALSE,
                       XmNsensitive, TRUE,
                       XmNshadowThickness,    1,
                       XmNcolumns, 6,
                       XmNmaxLength, 3,
                       XmNtopOffset, 7,
                       XmNbackground, colors[0x0f],
                       XmNleftAttachment, XmATTACH_POSITION,
                       XmNleftPosition, 2,
                       XmNrightAttachment, XmATTACH_NONE,
                       XmNtopAttachment,XmATTACH_WIDGET,
                       XmNtopWidget, wind_cse,
                       XmNbottomAttachment,XmATTACH_NONE,
                       XmNfontList, fontlist1,
                       NULL);

    WX_speed_label= XtVaCreateManagedWidget("WX_station speed label",xmTextFieldWidgetClass, form1,
                                            XmNeditable,   FALSE,
                                            XmNcursorPositionVisible, FALSE,
                                            XmNsensitive, STIPPLE,
                                            XmNshadowThickness,      0,
                                            XmNcolumns,5,
                                            XmNtopAttachment, XmATTACH_WIDGET,
                                            XmNtopWidget, wind_cse,
                                            XmNtopOffset, 9,
                                            XmNbottomAttachment, XmATTACH_NONE,
                                            XmNleftAttachment, XmATTACH_WIDGET,
                                            XmNleftWidget, WX_wind_spd_data,
                                            XmNleftOffset, 0,
                                            XmNrightAttachment, XmATTACH_NONE,
                                            XmNbackground, colors[0xff],
                                            XmNfontList, fontlist1,
                                            NULL);

    wind_gst = XtVaCreateManagedWidget(langcode("WXPUPSI005"),xmLabelWidgetClass, form1,
                                       XmNtopAttachment, XmATTACH_WIDGET,
                                       XmNtopWidget, wind_spd,
                                       XmNtopOffset, 11,
                                       XmNbottomAttachment, XmATTACH_NONE,
                                       XmNleftAttachment, XmATTACH_FORM,
                                       XmNleftOffset, 5,
                                       XmNrightAttachment, XmATTACH_NONE,
                                       XmNbackground, colors[0xff],
                                       XmNfontList, fontlist1,
                                       NULL);


    WX_wind_gst_data = XtVaCreateManagedWidget("WX_station wg data", xmTextFieldWidgetClass, form1,
                       XmNeditable,   FALSE,
                       XmNcursorPositionVisible, FALSE,
                       XmNsensitive, TRUE,
                       XmNshadowThickness,    1,
                       XmNcolumns, 6,
                       XmNmaxLength, 3,
                       XmNtopOffset, 7,
                       XmNbackground, colors[0x0f],
                       XmNleftAttachment, XmATTACH_POSITION,
                       XmNleftPosition, 2,
                       XmNrightAttachment, XmATTACH_NONE,
                       XmNtopAttachment,XmATTACH_WIDGET,
                       XmNtopWidget, wind_spd,
                       XmNbottomAttachment,XmATTACH_NONE,
                       XmNfontList, fontlist1,
                       NULL);

    WX_gust_label= XtVaCreateManagedWidget("WX_station gust label",xmTextFieldWidgetClass, form1,
                                           XmNeditable,   FALSE,
                                           XmNcursorPositionVisible, FALSE,
                                           XmNsensitive, STIPPLE,
                                           XmNshadowThickness,      0,
                                           XmNcolumns,5,
                                           XmNtopAttachment, XmATTACH_WIDGET,
                                           XmNtopWidget, wind_spd,
                                           XmNtopOffset, 9,
                                           XmNbottomAttachment, XmATTACH_NONE,
                                           XmNleftAttachment, XmATTACH_WIDGET,
                                           XmNleftWidget, WX_wind_gst_data,
                                           XmNleftOffset, 0,
                                           XmNrightAttachment, XmATTACH_NONE,
                                           XmNbackground, colors[0xff],
                                           XmNfontList, fontlist1,
                                           NULL);

    temp = XtVaCreateManagedWidget(langcode("WXPUPSI006"),xmLabelWidgetClass, form1,
                                   XmNtopAttachment, XmATTACH_WIDGET,
                                   XmNtopWidget, wind_gst,
                                   XmNtopOffset, 11,
                                   XmNbottomAttachment, XmATTACH_NONE,
                                   XmNleftAttachment, XmATTACH_FORM,
                                   XmNleftOffset, 5,
                                   XmNrightAttachment, XmATTACH_NONE,
                                   XmNbackground, colors[0xff],
                                   XmNfontList, fontlist1,
                                   NULL);


    WX_temp_data = XtVaCreateManagedWidget("WX_station temp data", xmTextFieldWidgetClass, form1,
                                           XmNeditable,   FALSE,
                                           XmNcursorPositionVisible, FALSE,
                                           XmNsensitive, TRUE,
                                           XmNshadowThickness,    1,
                                           XmNcolumns, 6,
                                           XmNmaxLength, 8,
                                           XmNtopOffset, 7,
                                           XmNbackground, colors[0x0f],
                                           XmNleftAttachment, XmATTACH_POSITION,
                                           XmNleftPosition, 2,
                                           XmNrightAttachment, XmATTACH_NONE,
                                           XmNtopAttachment,XmATTACH_WIDGET,
                                           XmNtopWidget, wind_gst,
                                           XmNbottomAttachment,XmATTACH_NONE,
                                           XmNfontList, fontlist1,
                                           NULL);

    WX_temp_label= XtVaCreateManagedWidget("WX_station temp label",xmTextFieldWidgetClass, form1,
                                           XmNeditable,   FALSE,
                                           XmNcursorPositionVisible, FALSE,
                                           XmNsensitive, STIPPLE,
                                           XmNshadowThickness,      0,
                                           XmNcolumns,5,
                                           XmNtopAttachment, XmATTACH_WIDGET,
                                           XmNtopWidget, wind_gst,
                                           XmNtopOffset, 9,
                                           XmNbottomAttachment, XmATTACH_NONE,
                                           XmNleftAttachment, XmATTACH_WIDGET,
                                           XmNleftWidget, WX_temp_data,
                                           XmNleftOffset, 0,
                                           XmNrightAttachment, XmATTACH_NONE,
                                           XmNbackground, colors[0xff],
                                           XmNfontList, fontlist1,
                                           NULL);

    my_rain = XtVaCreateManagedWidget(langcode("WXPUPSI007"),xmLabelWidgetClass, form1,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, temp,
                                      XmNtopOffset, 11,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset, 5,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      XmNfontList, fontlist1,
                                      NULL);


    WX_rain_data = XtVaCreateManagedWidget("WX_station rain data", xmTextFieldWidgetClass, form1,
                                           XmNeditable,   FALSE,
                                           XmNcursorPositionVisible, FALSE,
                                           XmNsensitive, TRUE,
                                           XmNshadowThickness,    1,
                                           XmNcolumns, 6,
                                           XmNmaxLength, 10,
                                           XmNtopOffset, 7,
                                           XmNbackground, colors[0x0f],
                                           XmNleftAttachment, XmATTACH_POSITION,
                                           XmNleftPosition, 2,
                                           XmNrightAttachment, XmATTACH_NONE,
                                           XmNtopAttachment,XmATTACH_WIDGET,
                                           XmNtopWidget, temp,
                                           XmNbottomAttachment,XmATTACH_NONE,
                                           XmNfontList, fontlist1,
                                           NULL);

    WX_rain_label= XtVaCreateManagedWidget("WX_station rain label",xmTextFieldWidgetClass, form1,
                                           XmNeditable,   FALSE,
                                           XmNcursorPositionVisible, FALSE,
                                           XmNsensitive, STIPPLE,
                                           XmNshadowThickness,      0,
                                           XmNcolumns,5,
                                           XmNtopAttachment, XmATTACH_WIDGET,
                                           XmNtopWidget, temp,
                                           XmNtopOffset, 9,
                                           XmNbottomAttachment, XmATTACH_NONE,
                                           XmNleftAttachment, XmATTACH_WIDGET,
                                           XmNleftWidget, WX_rain_data,
                                           XmNleftOffset, 0,
                                           XmNrightAttachment, XmATTACH_NONE,
                                           XmNbackground, colors[0xff],
                                           XmNfontList, fontlist1,
                                           NULL);

    to_rain = XtVaCreateManagedWidget(langcode("WXPUPSI008"),xmLabelWidgetClass, form1,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, my_rain,
                                      XmNtopOffset, 11,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset, 5,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      XmNfontList, fontlist1,
                                      NULL);


    WX_to_rain_data = XtVaCreateManagedWidget("WX_station today rain data", xmTextFieldWidgetClass, form1,
                      XmNeditable,   FALSE,
                      XmNcursorPositionVisible, FALSE,
                      XmNsensitive, TRUE,
                      XmNshadowThickness,    1,
                      XmNcolumns, 6,
                      XmNmaxLength, 10,
                      XmNtopOffset, 7,
                      XmNbackground, colors[0x0f],
                      XmNleftAttachment, XmATTACH_POSITION,
                      XmNleftPosition, 2,
                      XmNrightAttachment, XmATTACH_NONE,
                      XmNtopAttachment,XmATTACH_WIDGET,
                      XmNtopWidget, my_rain,
                      XmNbottomAttachment,XmATTACH_NONE,
                      XmNfontList, fontlist1,
                      NULL);


    WX_to_rain_label= XtVaCreateManagedWidget("WX_station to label",xmTextFieldWidgetClass, form1,
                      XmNeditable,   FALSE,
                      XmNcursorPositionVisible, FALSE,
                      XmNsensitive, STIPPLE,
                      XmNshadowThickness,      0,
                      XmNcolumns,10,
                      XmNtopAttachment, XmATTACH_WIDGET,
                      XmNtopWidget, my_rain,
                      XmNtopOffset, 9,
                      XmNbottomAttachment, XmATTACH_NONE,
                      XmNleftAttachment, XmATTACH_WIDGET,
                      XmNleftWidget, WX_to_rain_data,
                      XmNleftOffset, 0,
                      XmNrightAttachment, XmATTACH_NONE,
                      XmNbackground, colors[0xff],
                      XmNfontList, fontlist1,
                      NULL);

    rain_h = XtVaCreateManagedWidget(langcode("WXPUPSI014"),xmLabelWidgetClass, form1,
                                     XmNtopAttachment, XmATTACH_WIDGET,
                                     XmNtopWidget, to_rain,
                                     XmNtopOffset, 11,
                                     XmNbottomAttachment, XmATTACH_NONE,
                                     XmNleftAttachment, XmATTACH_FORM,
                                     XmNleftOffset, 5,
                                     XmNrightAttachment, XmATTACH_NONE,
                                     XmNbackground, colors[0xff],
                                     XmNfontList, fontlist1,
                                     NULL);


    WX_rain_h_data = XtVaCreateManagedWidget("WX_station hour rain data", xmTextFieldWidgetClass, form1,
                     XmNeditable,   FALSE,
                     XmNcursorPositionVisible, FALSE,
                     XmNsensitive, TRUE,
                     XmNshadowThickness,    1,
                     XmNcolumns, 6,
                     XmNmaxLength, 10,
                     XmNtopOffset, 7,
                     XmNbackground, colors[0x0f],
                     XmNleftAttachment, XmATTACH_POSITION,
                     XmNleftPosition, 2,
                     XmNrightAttachment, XmATTACH_NONE,
                     XmNtopAttachment,XmATTACH_WIDGET,
                     XmNtopWidget, to_rain,
                     XmNbottomAttachment,XmATTACH_NONE,
                     XmNfontList, fontlist1,
                     NULL);


    WX_rain_h_label= XtVaCreateManagedWidget("WX_station hour label",xmTextFieldWidgetClass, form1,
                     XmNeditable,   FALSE,
                     XmNcursorPositionVisible, FALSE,
                     XmNsensitive, STIPPLE,
                     XmNshadowThickness,      0,
                     XmNcolumns,10,
                     XmNtopAttachment, XmATTACH_WIDGET,
                     XmNtopWidget, to_rain,
                     XmNtopOffset, 9,
                     XmNbottomAttachment, XmATTACH_NONE,
                     XmNleftAttachment, XmATTACH_WIDGET,
                     XmNleftWidget, WX_rain_h_data,
                     XmNleftOffset, 0,
                     XmNrightAttachment, XmATTACH_NONE,
                     XmNbackground, colors[0xff],
                     XmNfontList, fontlist1,
                     NULL);

    my_rain_24 = XtVaCreateManagedWidget(langcode("WXPUPSI015"),xmLabelWidgetClass, form1,
                                         XmNtopAttachment, XmATTACH_WIDGET,
                                         XmNtopWidget, rain_h,
                                         XmNtopOffset, 11,
                                         XmNbottomAttachment, XmATTACH_NONE,
                                         XmNleftAttachment, XmATTACH_FORM,
                                         XmNleftOffset, 5,
                                         XmNrightAttachment, XmATTACH_NONE,
                                         XmNbackground, colors[0xff],
                                         XmNfontList, fontlist1,
                                         NULL);


    WX_rain_24_data = XtVaCreateManagedWidget("WX_station 24h rain data", xmTextFieldWidgetClass, form1,
                      XmNeditable,   FALSE,
                      XmNcursorPositionVisible, FALSE,
                      XmNsensitive, TRUE,
                      XmNshadowThickness,    1,
                      XmNcolumns, 6,
                      XmNmaxLength, 10,
                      XmNtopOffset, 7,
                      XmNbackground, colors[0x0f],
                      XmNleftAttachment, XmATTACH_POSITION,
                      XmNleftPosition, 2,
                      XmNrightAttachment, XmATTACH_NONE,
                      XmNtopAttachment,XmATTACH_WIDGET,
                      XmNtopWidget, rain_h,
                      XmNbottomAttachment,XmATTACH_NONE,
                      XmNfontList, fontlist1,
                      NULL);


    WX_rain_24_label= XtVaCreateManagedWidget("WX_station 24h label",xmTextFieldWidgetClass, form1,
                      XmNeditable,   FALSE,
                      XmNcursorPositionVisible, FALSE,
                      XmNsensitive, STIPPLE,
                      XmNshadowThickness,      0,
                      XmNcolumns,10,
                      XmNtopAttachment, XmATTACH_WIDGET,
                      XmNtopWidget, rain_h,
                      XmNtopOffset, 9,
                      XmNbottomAttachment, XmATTACH_NONE,
                      XmNleftAttachment, XmATTACH_WIDGET,
                      XmNleftWidget, WX_rain_24_data,
                      XmNleftOffset, 0,
                      XmNrightAttachment, XmATTACH_NONE,
                      XmNbackground, colors[0xff],
                      XmNfontList, fontlist1,
                      NULL);

    // humidity
    (void)XtVaCreateManagedWidget(langcode("WXPUPSI010"),xmLabelWidgetClass, form1,
                                  XmNtopAttachment, XmATTACH_WIDGET,
                                  XmNtopWidget, my_rain_24,
                                  XmNtopOffset, 11,
                                  XmNbottomAttachment, XmATTACH_NONE,
                                  XmNleftAttachment, XmATTACH_FORM,
                                  XmNleftOffset, 5,
                                  XmNrightAttachment, XmATTACH_NONE,
                                  XmNbackground, colors[0xff],
                                  XmNfontList, fontlist1,
                                  NULL);


    WX_humidity_data = XtVaCreateManagedWidget("WX_station Humidity data", xmTextFieldWidgetClass, form1,
                       XmNeditable,   FALSE,
                       XmNcursorPositionVisible, FALSE,
                       XmNsensitive, TRUE,
                       XmNshadowThickness,    1,
                       XmNcolumns, 6,
                       XmNmaxLength, 8,
                       XmNtopOffset, 7,
                       XmNbackground, colors[0x0f],
                       XmNleftAttachment, XmATTACH_POSITION,
                       XmNleftPosition, 2,
                       XmNrightAttachment, XmATTACH_NONE,
                       XmNtopAttachment,XmATTACH_WIDGET,
                       XmNtopWidget, my_rain_24,
                       XmNbottomAttachment,XmATTACH_NONE,
                       XmNfontList, fontlist1,
                       NULL);

    // humidity_n
    (void)XtVaCreateManagedWidget(langcode("UNIOP00026"),xmLabelWidgetClass, form1,
                                  XmNtopAttachment, XmATTACH_WIDGET,
                                  XmNtopWidget, my_rain_24,
                                  XmNtopOffset, 12,
                                  XmNbottomAttachment, XmATTACH_NONE,
                                  XmNleftAttachment, XmATTACH_WIDGET,
                                  XmNleftWidget, WX_humidity_data,
                                  XmNleftOffset, 5,
                                  XmNrightAttachment, XmATTACH_NONE,
                                  XmNbackground, colors[0xff],
                                  XmNfontList, fontlist1,
                                  NULL);


    dew_point = XtVaCreateManagedWidget(langcode("WXPUPSI018"),xmLabelWidgetClass, form1,
                                        XmNtopAttachment, XmATTACH_FORM,
                                        XmNtopOffset, 12,
                                        XmNbottomAttachment, XmATTACH_NONE,
                                        XmNleftAttachment, XmATTACH_POSITION,
                                        XmNleftPosition, 4,
                                        XmNrightAttachment, XmATTACH_NONE,
                                        XmNbackground, colors[0xff],
                                        XmNfontList, fontlist1,
                                        NULL);

    WX_dew_point_data = XtVaCreateManagedWidget("WX_station dew point", xmTextFieldWidgetClass, form1,
                        XmNeditable,   FALSE,
                        XmNcursorPositionVisible, FALSE,
                        XmNsensitive, STIPPLE,
                        XmNshadowThickness,1,
                        XmNcolumns, 6,
                        XmNtopAttachment, XmATTACH_FORM,
                        XmNtopOffset, 8,
                        XmNbottomAttachment, XmATTACH_NONE,
                        XmNleftAttachment, XmATTACH_POSITION,
                        XmNleftPosition, 5,
                        XmNrightAttachment, XmATTACH_NONE,
                        XmNbackground, colors[0x0f],
                        XmNfontList, fontlist1,
                        NULL);

    WX_dew_point_label = XtVaCreateManagedWidget("WX_station dew label",xmTextFieldWidgetClass, form1,
                         XmNeditable,   FALSE,
                         XmNcursorPositionVisible, FALSE,
                         XmNsensitive, STIPPLE,
                         XmNshadowThickness,0,
                         XmNtopAttachment, XmATTACH_FORM,
                         XmNtopOffset, 10,
                         XmNbottomAttachment, XmATTACH_NONE,
                         XmNleftAttachment, XmATTACH_WIDGET,
                         XmNleftWidget, WX_dew_point_data,
                         XmNleftOffset, 5,
                         XmNrightAttachment, XmATTACH_NONE,
                         XmNbackground, colors[0xff],
                         XmNfontList, fontlist1,
                         NULL);

    high_wind = XtVaCreateManagedWidget(langcode("WXPUPSI019"),xmLabelWidgetClass, form1,
                                        XmNtopAttachment, XmATTACH_WIDGET,
                                        XmNtopWidget, dew_point,
                                        XmNtopOffset, 11,
                                        XmNbottomAttachment, XmATTACH_NONE,
                                        XmNleftAttachment, XmATTACH_POSITION,
                                        XmNleftPosition, 4,
                                        XmNrightAttachment, XmATTACH_NONE,
                                        XmNbackground, colors[0xff],
                                        XmNfontList, fontlist1,
                                        NULL);

    WX_high_wind_data = XtVaCreateManagedWidget("WX_station High Wind", xmTextFieldWidgetClass, form1,
                        XmNeditable,   FALSE,
                        XmNcursorPositionVisible, FALSE,
                        XmNcolumns, 6,
                        XmNsensitive, TRUE,
                        XmNshadowThickness,1,
                        XmNleftAttachment, XmATTACH_POSITION,
                        XmNleftPosition, 5,
                        XmNrightAttachment, XmATTACH_NONE,
                        XmNtopAttachment,XmATTACH_WIDGET,
                        XmNtopWidget, dew_point,
                        XmNtopOffset, 7,
                        XmNbottomAttachment,XmATTACH_NONE,
                        XmNbackground, colors[0x0f],
                        XmNfontList, fontlist1,
                        NULL);

    WX_high_wind_label = XtVaCreateManagedWidget("WX_station high wind label",xmTextFieldWidgetClass, form1,
                         XmNeditable,   FALSE,
                         XmNcursorPositionVisible, FALSE,
                         XmNsensitive, STIPPLE,
                         XmNshadowThickness,0,
                         XmNtopAttachment, XmATTACH_WIDGET,
                         XmNtopWidget,dew_point,
                         XmNtopOffset, 8,
                         XmNbottomAttachment, XmATTACH_NONE,
                         XmNleftAttachment, XmATTACH_WIDGET,
                         XmNleftWidget, WX_high_wind_data,
                         XmNleftOffset, 5,
                         XmNrightAttachment, XmATTACH_NONE,
                         XmNbackground, colors[0xff],
                         XmNfontList, fontlist1,
                         NULL);

    wind_chill = XtVaCreateManagedWidget(langcode("WXPUPSI020"),xmLabelWidgetClass, form1,
                                         XmNtopAttachment, XmATTACH_WIDGET,
                                         XmNtopWidget, high_wind,
                                         XmNtopOffset, 11,
                                         XmNbottomAttachment, XmATTACH_NONE,
                                         XmNleftAttachment, XmATTACH_POSITION,
                                         XmNleftPosition, 4,
                                         XmNrightAttachment, XmATTACH_NONE,
                                         XmNbackground, colors[0xff],
                                         XmNfontList, fontlist1,
                                         NULL);

    WX_wind_chill_data = XtVaCreateManagedWidget("WX_station Wind Chill", xmTextFieldWidgetClass, form1,
                         XmNeditable,   FALSE,
                         XmNcursorPositionVisible, FALSE,
                         XmNcolumns, 6,
                         XmNsensitive,TRUE,
                         XmNshadowThickness,1,
                         XmNleftAttachment, XmATTACH_POSITION,
                         XmNleftPosition, 5,
                         XmNrightAttachment, XmATTACH_NONE,
                         XmNtopAttachment,XmATTACH_WIDGET,
                         XmNtopWidget, high_wind,
                         XmNtopOffset, 7,
                         XmNbottomAttachment,XmATTACH_NONE,
                         XmNbackground, colors[0x0f],
                         XmNfontList, fontlist1,
                         NULL);

    WX_wind_chill_label = XtVaCreateManagedWidget("WX_station wind label",xmTextFieldWidgetClass, form1,
                          XmNeditable,   FALSE,
                          XmNcursorPositionVisible, FALSE,
                          XmNsensitive, STIPPLE,
                          XmNshadowThickness,0,
                          XmNtopAttachment, XmATTACH_WIDGET,
                          XmNtopWidget,high_wind,
                          XmNtopOffset, 8,
                          XmNbottomAttachment, XmATTACH_NONE,
                          XmNleftAttachment, XmATTACH_WIDGET,
                          XmNleftWidget, WX_wind_chill_data,
                          XmNleftOffset, 5,
                          XmNrightAttachment, XmATTACH_NONE,
                          XmNbackground, colors[0xff],
                          XmNfontList, fontlist1,
                          NULL);

    heat_index = XtVaCreateManagedWidget(langcode("WXPUPSI021"),xmLabelWidgetClass, form1,
                                         XmNtopAttachment, XmATTACH_WIDGET,
                                         XmNtopWidget, wind_chill,
                                         XmNtopOffset, 11,
                                         XmNbottomAttachment, XmATTACH_NONE,
                                         XmNleftAttachment, XmATTACH_POSITION,
                                         XmNleftPosition, 4,
                                         XmNrightAttachment, XmATTACH_NONE,
                                         XmNbackground, colors[0xff],
                                         XmNfontList, fontlist1,
                                         NULL);

    WX_heat_index_data = XtVaCreateManagedWidget("WX_station Heat Index", xmTextFieldWidgetClass, form1,
                         XmNeditable,   FALSE,
                         XmNcursorPositionVisible, FALSE,
                         XmNcolumns, 6,
                         XmNsensitive,TRUE,
                         XmNshadowThickness,1,
                         XmNleftAttachment, XmATTACH_POSITION,
                         XmNleftPosition, 5,
                         XmNrightAttachment, XmATTACH_NONE,
                         XmNtopAttachment,XmATTACH_WIDGET,
                         XmNtopWidget, wind_chill,
                         XmNtopOffset, 7,
                         XmNbottomAttachment,XmATTACH_NONE,
                         XmNbackground, colors[0x0f],
                         XmNfontList, fontlist1,
                         NULL);

    WX_heat_index_label = XtVaCreateManagedWidget("WX_station heat label",xmTextFieldWidgetClass, form1,
                          XmNeditable,   FALSE,
                          XmNcursorPositionVisible, FALSE,
                          XmNsensitive, STIPPLE,
                          XmNshadowThickness,0,
                          XmNtopAttachment, XmATTACH_WIDGET,
                          XmNtopWidget,wind_chill,
                          XmNtopOffset, 8,
                          XmNbottomAttachment, XmATTACH_NONE,
                          XmNleftAttachment, XmATTACH_WIDGET,
                          XmNleftWidget, WX_heat_index_data,
                          XmNleftOffset, 5,
                          XmNrightAttachment, XmATTACH_NONE,
                          XmNbackground, colors[0xff],
                          XmNfontList, fontlist1,
                          NULL);

    baro = XtVaCreateManagedWidget(langcode("WXPUPSI009"),xmLabelWidgetClass, form1,
                                   XmNtopAttachment, XmATTACH_WIDGET,
                                   XmNtopWidget, heat_index,
                                   XmNtopOffset, 11,
                                   XmNbottomAttachment, XmATTACH_NONE,
                                   XmNleftAttachment, XmATTACH_POSITION,
                                   XmNleftPosition, 4,
                                   XmNrightAttachment, XmATTACH_NONE,
                                   XmNbackground, colors[0xff],
                                   XmNfontList, fontlist1,
                                   NULL);

    WX_baro_data = XtVaCreateManagedWidget("WX_station Baro", xmTextFieldWidgetClass, form1,
                                           XmNeditable,   FALSE,
                                           XmNcursorPositionVisible, FALSE,
                                           XmNcolumns, 6,
                                           XmNsensitive,TRUE,
                                           XmNshadowThickness,1,
                                           XmNleftAttachment, XmATTACH_POSITION,
                                           XmNleftPosition, 5,
                                           XmNrightAttachment, XmATTACH_NONE,
                                           XmNtopAttachment,XmATTACH_WIDGET,
                                           XmNtopWidget, heat_index,
                                           XmNtopOffset, 7,
                                           XmNbottomAttachment,XmATTACH_NONE,
                                           XmNbackground, colors[0x0f],
                                           XmNfontList, fontlist1,
                                           NULL);

    WX_baro_label = XtVaCreateManagedWidget("WX_Station baro unit label",xmTextFieldWidgetClass, form1,
                                            XmNeditable,   FALSE,
                                            XmNcursorPositionVisible, FALSE,
                                            XmNsensitive, STIPPLE,
                                            XmNshadowThickness,0,
                                            XmNtopAttachment, XmATTACH_WIDGET,
                                            XmNtopWidget,heat_index,
                                            XmNtopOffset, 12,
                                            XmNbottomAttachment, XmATTACH_NONE,
                                            XmNleftAttachment, XmATTACH_WIDGET,
                                            XmNleftWidget, WX_baro_data,
                                            XmNleftOffset, 5,
                                            XmNrightAttachment, XmATTACH_NONE,
                                            XmNbackground, colors[0xff],
                                            XmNfontList, fontlist1,
                                            NULL);


    three_hour_baro = XtVaCreateManagedWidget(langcode("WXPUPSI022"),xmLabelWidgetClass, form1,
                      XmNtopAttachment, XmATTACH_WIDGET,
                      XmNtopWidget, baro,
                      XmNtopOffset, 11,
                      XmNbottomAttachment, XmATTACH_NONE,
                      XmNleftAttachment, XmATTACH_POSITION,
                      XmNleftPosition, 4,
                      XmNrightAttachment, XmATTACH_NONE,
                      XmNbackground, colors[0xff],
                      XmNfontList, fontlist1,
                      NULL);

    WX_three_hour_baro_data = XtVaCreateManagedWidget("WX_station 3-Hr Baro", xmTextFieldWidgetClass, form1,
                              XmNeditable,   FALSE,
                              XmNcursorPositionVisible, FALSE,
                              XmNcolumns, 6,
                              XmNsensitive,TRUE,
                              XmNshadowThickness,1,
                              XmNleftAttachment, XmATTACH_POSITION,
                              XmNleftPosition, 5,
                              XmNrightAttachment, XmATTACH_NONE,
                              XmNtopAttachment,XmATTACH_WIDGET,
                              XmNtopWidget, baro,
                              XmNtopOffset, 7,
                              XmNbottomAttachment,XmATTACH_NONE,
                              XmNbackground, colors[0x0f],
                              XmNfontList, fontlist1,
                              NULL);

    WX_three_hour_baro_label = XtVaCreateManagedWidget("WX_station 3hr baro unit label",xmTextFieldWidgetClass, form1,
                               XmNeditable,   FALSE,
                               XmNcursorPositionVisible, FALSE,
                               XmNsensitive, STIPPLE,
                               XmNshadowThickness,0,
                               XmNtopAttachment, XmATTACH_WIDGET,
                               XmNtopWidget,  baro,
                               XmNtopOffset, 12,
                               XmNbottomAttachment, XmATTACH_NONE,
                               XmNleftAttachment, XmATTACH_WIDGET,
                               XmNleftWidget, WX_three_hour_baro_data,
                               XmNleftOffset, 5,
                               XmNrightAttachment, XmATTACH_NONE,
                               XmNbackground, colors[0xff],
                               XmNfontList, fontlist1,
                               NULL);

    hi_temp = XtVaCreateManagedWidget(langcode("WXPUPSI023"),xmLabelWidgetClass, form1,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, three_hour_baro,
                                      XmNtopOffset, 11,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_POSITION,
                                      XmNleftPosition, 4,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      XmNfontList, fontlist1,
                                      NULL);

    WX_hi_temp_data = XtVaCreateManagedWidget("WX_station Today's High Temp", xmTextFieldWidgetClass, form1,
                      XmNeditable,   FALSE,
                      XmNcursorPositionVisible, FALSE,
                      XmNcolumns, 6,
                      XmNsensitive,TRUE,
                      XmNshadowThickness,1,
                      XmNleftAttachment, XmATTACH_POSITION,
                      XmNleftPosition, 5,
                      XmNrightAttachment, XmATTACH_NONE,
                      XmNtopAttachment,XmATTACH_WIDGET,
                      XmNtopWidget, three_hour_baro,
                      XmNtopOffset, 7,
                      XmNbottomAttachment,XmATTACH_NONE,
                      XmNbackground, colors[0x0f],
                      XmNfontList, fontlist1,
                      NULL);

    WX_hi_temp_label = XtVaCreateManagedWidget("WX_station high temp label",xmTextFieldWidgetClass, form1,
                       XmNeditable,   FALSE,
                       XmNcursorPositionVisible, FALSE,
                       XmNsensitive, STIPPLE,
                       XmNshadowThickness,0,
                       XmNtopAttachment, XmATTACH_WIDGET,
                       XmNtopWidget,  three_hour_baro,
                       XmNtopOffset, 8,
                       XmNbottomAttachment, XmATTACH_NONE,
                       XmNleftAttachment, XmATTACH_WIDGET,
                       XmNleftWidget, WX_hi_temp_data,
                       XmNleftOffset, 5,
                       XmNrightAttachment, XmATTACH_NONE,
                       XmNbackground, colors[0xff],
                       XmNfontList, fontlist1,
                       NULL);

    // low_temp
    (void)XtVaCreateManagedWidget(langcode("WXPUPSI024"),xmLabelWidgetClass, form1,
                                  XmNtopAttachment, XmATTACH_WIDGET,
                                  XmNtopWidget, hi_temp,
                                  XmNtopOffset, 11,
                                  XmNbottomAttachment, XmATTACH_NONE,
                                  XmNleftAttachment, XmATTACH_POSITION,
                                  XmNleftPosition, 4,
                                  XmNrightAttachment, XmATTACH_NONE,
                                  XmNbackground, colors[0xff],
                                  XmNfontList, fontlist1,
                                  NULL);

    WX_low_temp_data = XtVaCreateManagedWidget("WX_station Today's Low Temp", xmTextFieldWidgetClass, form1,
                       XmNeditable,   FALSE,
                       XmNcursorPositionVisible, FALSE,
                       XmNcolumns, 6,
                       XmNsensitive,TRUE,
                       XmNshadowThickness,1,
                       XmNleftAttachment, XmATTACH_POSITION,
                       XmNleftPosition, 5,
                       XmNrightAttachment, XmATTACH_NONE,
                       XmNtopAttachment,XmATTACH_WIDGET,
                       XmNtopWidget, hi_temp,
                       XmNtopOffset, 7,
                       XmNbottomAttachment,XmATTACH_NONE,
                       XmNbackground, colors[0x0f],
                       XmNfontList, fontlist1,
                       NULL);

    WX_low_temp_label = XtVaCreateManagedWidget("WX_station low temp label",xmTextFieldWidgetClass, form1,
                        XmNeditable,   FALSE,
                        XmNcursorPositionVisible, FALSE,
                        XmNsensitive, STIPPLE,
                        XmNshadowThickness,0,
                        XmNtopAttachment, XmATTACH_WIDGET,
                        XmNtopWidget,  hi_temp,
                        XmNtopOffset, 8,
                        XmNbottomAttachment, XmATTACH_NONE,
                        XmNleftAttachment, XmATTACH_WIDGET,
                        XmNleftWidget, WX_low_temp_data,
                        XmNleftOffset, 5,
                        XmNrightAttachment, XmATTACH_NONE,
                        XmNbackground, colors[0xff],
                        XmNfontList, fontlist1,
                        NULL);


    button_close = XtVaCreateManagedWidget(langcode("UNIOP00003"),xmPushButtonGadgetClass, my_form,
                                           XmNtraversalOn, TRUE,
                                           XmNtopAttachment, XmATTACH_WIDGET,
                                           XmNtopWidget, frame,
                                           XmNtopOffset, 10,
                                           XmNbottomAttachment, XmATTACH_FORM,
                                           XmNbottomOffset,10,
                                           XmNleftAttachment, XmATTACH_POSITION,
                                           XmNleftPosition, 3,
                                           XmNrightAttachment, XmATTACH_POSITION,
                                           XmNrightPosition, 4,
                                           XmNbackground, colors[0xff],
                                           XmNfontList, fontlist1,
                                           NULL);

    XtAddCallback(button_close, XmNactivateCallback, WX_station_destroy_shell, wx_station_dialog);

    pos_dialog(wx_station_dialog);

    delw = XmInternAtom(XtDisplay(wx_station_dialog),"WM_DELETE_WINDOW", FALSE);
    XmAddWMProtocolCallback(wx_station_dialog, delw, WX_station_destroy_shell, (XtPointer)wx_station_dialog);

    XtManageChild(my_form);
    XtManageChild(form1);
    XtManageChild(pane);

    end_critical_section(&wx_station_dialog_lock, "wx_gui.c:WX_station" );

    XtPopup(wx_station_dialog,XtGrabNone);
    fix_dialog_size(wx_station_dialog);
    fill_wx_data();
  }
  else
  {
    (void)XRaiseWindow(XtDisplay(wx_station_dialog), XtWindow(wx_station_dialog));
  }
}





void fill_wx_data(void)
{
  DataRow *p_station;
  char temp[20];
  WeatherRow *weather;

  if (wx_station_dialog != NULL)
  {

    begin_critical_section(&wx_station_dialog_lock, "wx_gui.c:fill_wx_data" );

    if (search_station_name(&p_station,my_callsign,1))
    {
      if (get_weather_record(p_station))      // DK7IN: only add record if we found something...
      {
        weather = p_station->weather_data;

        if (strlen(wx_station_type) > 1)
        {
          XmTextFieldSetString(WX_type_data,wx_station_type);
        }
        else
        {
          XmTextFieldSetString(WX_type_data,"");
        }
        XtManageChild(WX_type_data);

        if (weather != 0)    // we have weather data
        {
          if (strlen(weather->wx_temp) > 0)
          {
            if (!english_units)
            {
              xastir_snprintf(temp,
                              sizeof(temp),
                              "%03d",
                              (int)(((atof(weather->wx_temp)-32)*5.0)/9.0));

              XmTextFieldSetString(WX_temp_data,temp);
            }
            else
            {
              XmTextFieldSetString(WX_temp_data,weather->wx_temp);
            }
          }
          else
          {
            XmTextFieldSetString(WX_temp_data,"");
          }
          XtManageChild(WX_temp_data);

          if (strlen(weather->wx_course) > 0)
          {
            XmTextFieldSetString(WX_wind_cse_data,weather->wx_course);
          }
          else
          {
            XmTextFieldSetString(WX_wind_cse_data,"");
          }

          XtManageChild(WX_wind_cse_data);

          if (strlen(weather->wx_speed) > 0)
          {
            if (!english_units)
            {
              xastir_snprintf(temp,
                              sizeof(temp),
                              "%03d",
                              (int)(atof(weather->wx_speed)*1.6094));

              XmTextFieldSetString(WX_wind_spd_data,temp);
            }
            else
            {
              XmTextFieldSetString(WX_wind_spd_data,weather->wx_speed);
            }
          }
          else
          {
            XmTextFieldSetString(WX_wind_spd_data,"");
          }

          XtManageChild(WX_wind_spd_data);

          if (strlen(weather->wx_gust) > 0)
          {
            if (!english_units)
            {
              xastir_snprintf(temp,
                              sizeof(temp),
                              "%03d",
                              (int)(atof(weather->wx_gust)*1.6094));

              XmTextFieldSetString(WX_wind_gst_data,temp);
            }
            else
            {
              XmTextFieldSetString(WX_wind_gst_data,weather->wx_gust);
            }
          }
          else
          {
            XmTextFieldSetString(WX_wind_gst_data,"");
          }

          XtManageChild(WX_wind_gst_data);

          if (strlen(weather->wx_rain_total) > 0)
          {
            if (!english_units)
              xastir_snprintf(temp,
                              sizeof(temp),
                              "%0.2f",
                              atof(weather->wx_rain_total)*.254);
            else
              xastir_snprintf(temp,
                              sizeof(temp),
                              "%0.2f",
                              atof(weather->wx_rain_total)/100.0);

            XmTextFieldSetString(WX_rain_data,temp);
          }
          else
          {
            XmTextFieldSetString(WX_rain_data,"");
          }

          XtManageChild(WX_rain_data);

          if (strlen(weather->wx_rain) > 0)
          {
            if (!english_units)
              xastir_snprintf(temp,
                              sizeof(temp),
                              "%0.2f",
                              atof(weather->wx_rain)*.254);
            else
              xastir_snprintf(temp,
                              sizeof(temp),
                              "%0.2f",
                              atof(weather->wx_rain)/100.0);

            XmTextFieldSetString(WX_rain_h_data,temp);
          }
          else
          {
            XmTextFieldSetString(WX_rain_h_data,"");
          }

          XtManageChild(WX_rain_h_data);

          if (strlen(weather->wx_prec_24) > 0)
          {
            if (!english_units)
              xastir_snprintf(temp,
                              sizeof(temp),
                              "%0.2f",
                              atof(weather->wx_prec_24)*.254);
            else
              xastir_snprintf(temp,
                              sizeof(temp),
                              "%0.2f",
                              atof(weather->wx_prec_24)/100.0);

            XmTextFieldSetString(WX_rain_24_data,temp);
          }
          else
          {
            XmTextFieldSetString(WX_rain_24_data,"");
          }

          XtManageChild(WX_rain_24_data);

          if (strlen(weather->wx_prec_00) > 0)
          {
            if (!english_units)
              xastir_snprintf(temp,
                              sizeof(temp),
                              "%0.2f",
                              atof(weather->wx_prec_00)*.254);
            else
              xastir_snprintf(temp,
                              sizeof(temp),
                              "%0.2f",
                              atof(weather->wx_prec_00)/100.0);

            XmTextFieldSetString(WX_to_rain_data,temp);
          }
          else
          {
            XmTextFieldSetString(WX_to_rain_data,"");
          }

          XtManageChild(WX_rain_data);

          if (strlen(weather->wx_hum) > 0)
          {
            XmTextFieldSetString(WX_humidity_data,weather->wx_hum);
          }
          else
          {
            XmTextFieldSetString(WX_humidity_data,"");
          }

          XtManageChild(WX_humidity_data);

          if (strlen(wx_dew_point) > 0)
          {
            if (!english_units)
            {
              xastir_snprintf(temp,
                              sizeof(temp),
                              "%03d",
                              (int)(((atof(wx_dew_point)-32)*5.0)/9.0));
              XmTextFieldSetString(WX_dew_point_data,temp);
            }
            else
            {
              XmTextFieldSetString(WX_dew_point_data,wx_dew_point);
            }
          }
          else
          {
            XmTextFieldSetString(WX_dew_point_data,"");
          }

          XtManageChild(WX_dew_point_data);

          if (strlen(wx_high_wind) > 0)
          {
            if (!english_units)
            {
              xastir_snprintf(temp,
                              sizeof(temp),
                              "%03d",
                              (int)(atof(wx_high_wind)*1.6094));
              XmTextFieldSetString(WX_high_wind_data,temp);
            }
            else
            {
              XmTextFieldSetString(WX_high_wind_data,wx_high_wind);
            }
          }
          else
          {
            XmTextFieldSetString(WX_high_wind_data,"");
          }

          XtManageChild(WX_high_wind_data);

          if (strlen(wx_wind_chill) > 0)
          {
            if (!english_units)
            {
              xastir_snprintf(temp,
                              sizeof(temp),
                              "%03d",
                              (int)(((atof(wx_wind_chill)-32)*5.0)/9.0));
              XmTextFieldSetString(WX_wind_chill_data,temp);
            }
            else
            {
              XmTextFieldSetString(WX_wind_chill_data,wx_wind_chill);
            }
          }
          else
          {
            XmTextFieldSetString(WX_wind_chill_data,"");
          }

          XtManageChild(WX_wind_chill_data);

          if (strlen(weather->wx_baro) > 0)
          {
            if (!english_units)
            {
              //xastir_snprintf(temp, sizeof(temp), "%0.0f",
              //        atof(wx_baro_inHg)*25.4); // inch Hg -> mm Hg
              //XmTextFieldSetString(WX_baro_data,temp);
              XmTextFieldSetString(WX_baro_data,weather->wx_baro); // hPa
            }
            else    // inches mercury
            {
              xastir_snprintf(temp,
                              sizeof(temp),
                              "%0.2f",
                              (atof(weather->wx_baro)*0.02953));
              XmTextFieldSetString(WX_baro_data,temp);
            }
          }
          else
          {
            XmTextFieldSetString(WX_baro_data,"");
          }

          XtManageChild(WX_baro_data);

          if (wx_three_hour_baro_on)
          {
            if (!english_units)    // hPa
            {
              //xastir_snprintf(temp, sizeof(temp), "%0.0f",
              //        atof(wx_three_hour_baro)*25.4); // inch Hg -> mm Hg
              XmTextFieldSetString(WX_three_hour_baro_data,wx_three_hour_baro);
            }
            else      // inches mercury
            {
              xastir_snprintf(temp,
                              sizeof(temp),
                              "%0.2f",
                              (atof(wx_three_hour_baro)*0.02953));
              XmTextFieldSetString(WX_three_hour_baro_data,temp);
            }
          }
          else
          {
            XmTextFieldSetString(WX_three_hour_baro_data,"");
          }

          XtManageChild(WX_three_hour_baro_data);

          if (wx_hi_temp_on)
          {
            if (!english_units)
            {
              xastir_snprintf(temp,
                              sizeof(temp),
                              "%03d",
                              (int)(((atof(wx_hi_temp)-32)*5.0)/9.0));
              XmTextFieldSetString(WX_hi_temp_data,temp);
            }
            else
            {
              XmTextFieldSetString(WX_hi_temp_data,wx_hi_temp);
            }
          }
          else
          {
            XmTextFieldSetString(WX_hi_temp_data,"");
          }

          XtManageChild(WX_hi_temp_data);

          if (wx_low_temp_on)
          {
            if (!english_units)
            {
              xastir_snprintf(temp,
                              sizeof(temp),
                              "%03d",
                              (int)(((atof(wx_low_temp)-32)*5.0)/9.0));
              XmTextFieldSetString(WX_low_temp_data,temp);
            }
            else
            {
              XmTextFieldSetString(WX_low_temp_data,wx_low_temp);
            }
          }
          else
          {
            XmTextFieldSetString(WX_low_temp_data,"");
          }

          XtManageChild(WX_low_temp_data);

          if (wx_heat_index_on)
          {
            if (!english_units)
            {
              xastir_snprintf(temp,
                              sizeof(temp),
                              "%03d",
                              (int)(((atof(wx_heat_index)-32)*5.0)/9.0));
              XmTextFieldSetString(WX_heat_index_data,temp);
            }
            else
            {
              XmTextFieldSetString(WX_heat_index_data,wx_heat_index);
            }
          }
          else
          {
            XmTextFieldSetString(WX_heat_index_data,"");
          }

          XtManageChild(WX_heat_index_data);
        }
      }
    }

    /* labels */
    if (!english_units)
    {
      XmTextFieldSetString(WX_speed_label,langcode("UNIOP00012"));
    }
    else
    {
      XmTextFieldSetString(WX_speed_label,langcode("UNIOP00013"));
    }

    XtManageChild(WX_speed_label);

    if (!english_units)
    {
      XmTextFieldSetString(WX_gust_label,langcode("UNIOP00012"));
    }
    else
    {
      XmTextFieldSetString(WX_gust_label,langcode("UNIOP00013"));
    }

    XtManageChild(WX_gust_label);

    if (!english_units)
    {
      XmTextFieldSetString(WX_temp_label,langcode("UNIOP00014"));
    }
    else
    {
      XmTextFieldSetString(WX_temp_label,langcode("UNIOP00015"));
    }

    XtManageChild(WX_temp_label);

    if (!english_units)
    {
      XmTextFieldSetString(WX_rain_label,langcode("UNIOP00016"));
    }
    else
    {
      XmTextFieldSetString(WX_rain_label,langcode("UNIOP00017"));
    }

    XtManageChild(WX_rain_label);

    if (!english_units)
    {
      XmTextFieldSetString(WX_to_rain_label,langcode("UNIOP00022"));
    }
    else
    {
      XmTextFieldSetString(WX_to_rain_label,langcode("UNIOP00023"));
    }

    XtManageChild(WX_to_rain_label);

    if (!english_units)
    {
      XmTextFieldSetString(WX_rain_h_label,langcode("UNIOP00020"));
    }
    else
    {
      XmTextFieldSetString(WX_rain_h_label,langcode("UNIOP00021"));
    }

    XtManageChild(WX_rain_h_label);

    if (!english_units)
    {
      XmTextFieldSetString(WX_rain_24_label,langcode("UNIOP00018"));
    }
    else
    {
      XmTextFieldSetString(WX_rain_24_label,langcode("UNIOP00019"));
    }

    XtManageChild(WX_rain_24_label);

    if (!english_units)
    {
      XmTextFieldSetString(WX_dew_point_label,langcode("UNIOP00014"));
    }
    else
    {
      XmTextFieldSetString(WX_dew_point_label,langcode("UNIOP00015"));
    }

    XtManageChild(WX_dew_point_label);

    if (!english_units)
    {
      XmTextFieldSetString(WX_wind_chill_label,langcode("UNIOP00014"));
    }
    else
    {
      XmTextFieldSetString(WX_wind_chill_label,langcode("UNIOP00015"));
    }

    XtManageChild(WX_wind_chill_label);

    if (!english_units)
    {
      XmTextFieldSetString(WX_heat_index_label,langcode("UNIOP00014"));
    }
    else
    {
      XmTextFieldSetString(WX_heat_index_label,langcode("UNIOP00015"));
    }

    XtManageChild(WX_heat_index_label);

    if (!english_units)
    {
      XmTextFieldSetString(WX_hi_temp_label,langcode("UNIOP00014"));
    }
    else
    {
      XmTextFieldSetString(WX_hi_temp_label,langcode("UNIOP00015"));
    }

    XtManageChild(WX_hi_temp_label);

    if (!english_units)
    {
      XmTextFieldSetString(WX_low_temp_label,langcode("UNIOP00014"));
    }
    else
    {
      XmTextFieldSetString(WX_low_temp_label,langcode("UNIOP00015"));
    }

    XtManageChild(WX_low_temp_label);

    if (!english_units)
    {
      XmTextFieldSetString(WX_high_wind_label,langcode("UNIOP00012"));
    }
    else
    {
      XmTextFieldSetString(WX_high_wind_label,langcode("UNIOP00013"));
    }

    XtManageChild(WX_high_wind_label);

    if (!english_units)
    {
      XmTextFieldSetString(WX_baro_label,langcode("UNIOP00025"));
    }
    else
    {
      XmTextFieldSetString(WX_baro_label,langcode("UNIOP00027"));
    }

    XtManageChild(WX_baro_label);

    if (!english_units)
    {
      XmTextFieldSetString(WX_three_hour_baro_label,langcode("UNIOP00025"));
    }
    else
    {
      XmTextFieldSetString(WX_three_hour_baro_label,langcode("UNIOP00027"));
    }

    XtManageChild(WX_three_hour_baro_label);

    end_critical_section(&wx_station_dialog_lock, "wx_gui.c:fill_wx_data" );

  }
}


