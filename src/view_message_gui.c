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

#include <Xm/XmAll.h>
#include <X11/Xatom.h>
#include <X11/Shell.h>

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_STRING_H
  #include <string.h>
#else
  #include <strings.h>
#endif
#include <ctype.h>
#include <sys/types.h>

#if TIME_WITH_SYS_TIME
  #include <sys/time.h>
  #include <time.h>
#else   // TIME_WITH_SYS_TIME
  #if HAVE_SYS_TIME_H
    #include <sys/time.h>
  #else  // HAVE_SYS_TIME_H
    #include <time.h>
  #endif // HAVE_SYS_TIME_H
#endif  // TIME_WITH_SYS_TIME

#include "xastir.h"
#include "main.h"
#include "util.h"

// Must be last include file
#include "leak_detection.h"


extern XmFontList fontlist1;    // Menu/System fontlist

Widget All_messages_dialog = NULL;
Widget view_messages_text = NULL;
Widget vm_dist_data = NULL;

static xastir_mutex All_messages_dialog_lock;

int vm_range;
int view_message_limit = 10000;
int Read_messages_packet_data_type = 0; // 1=tnc_only, 2=net_only, 0=tnc&net
int Read_messages_mine_only = 0;





void view_message_gui_init(void)
{
  init_critical_section( &All_messages_dialog_lock );
}





void view_message_print_record(Message *m_fill)
{
  int pos;
  char *temp;
  int i;
  int my_size = 200;
  char temp_my_course[10];
  XmTextPosition drop_ptr;
  int distance;


  // Make sure it's within our distance range we have set
  distance = distance_from_my_station(m_fill->from_call_sign,temp_my_course);

  if (Read_messages_mine_only
      || (!Read_messages_mine_only
          && ( (vm_range == 0) || (distance <= vm_range) ) ) )
  {

    // Check that it's coming from the correct type of interface
    // Compare Read_messages_packet_data_type against the port
    // type associated with data_port to determine whether or
    // not to display it.
    //
    // I = Internet
    // L = Local
    // T = TNC
    // F = File
    //
    switch (Read_messages_packet_data_type)
    {

      case 2:     // Display NET data only
        // if not network_interface, return
        if (m_fill->data_via != 'I')
        {
          return;  // Don't display it
        }
        break;

      case 1:     // Display TNC data only
        // if not local_tnc_interface, return
        if (m_fill->data_via != 'T')
        {
          return;  // Don't display it
        }
        break;

      case 0:     // Display both TNC and NET data
      default:
        break;
    }

    // Check for my stations only if set
    if (Read_messages_mine_only)
    {
      char short_call[MAX_CALLSIGN];
      char *p;

      memcpy(short_call, my_callsign, sizeof(short_call));
      short_call[sizeof(short_call)-1] = '\0';  // Terminate string
      if ( (p = index(short_call,'-')) )
      {
        *p = '\0';  // Terminate it
      }

      if (!strstr(m_fill->call_sign, short_call)
          && !strstr(m_fill->from_call_sign, short_call))
      {
        return;
      }
    }

    if ((temp = malloc((size_t)my_size)) == NULL)
    {
      return;
    }

    sprintf(temp,"%-9s>%-9s %s:%5s %s:%c :%s\n",
            m_fill->from_call_sign,
            m_fill->call_sign,
            langcode("WPUPMSB013"),
            m_fill->seq,
            langcode("WPUPMSB014"),
            m_fill->type,
            m_fill->message_line);

    pos = (int)XmTextGetLastPosition(view_messages_text);


    XmTextInsert(view_messages_text, pos, temp);
    pos += strlen(temp);
    while (pos > view_message_limit)
    {
      for (drop_ptr = i = 0; i < 3; i++)
      {
        (void)XmTextFindString(view_messages_text, drop_ptr, "\n", XmTEXT_FORWARD, &drop_ptr);
        drop_ptr++;
      }
      XmTextReplace(view_messages_text, 0, drop_ptr, "");
      pos = (int)XmTextGetLastPosition(view_messages_text);
    }
    XtVaSetValues(view_messages_text, XmNcursorPosition, pos, NULL);

    free(temp);
  }
}





void view_message_display_file(char msg_type)
{
  int pos;

  if ((All_messages_dialog != NULL))
  {
    mscan_file(msg_type, view_message_print_record);
  }
  pos = (int)XmTextGetLastPosition(view_messages_text);
  XmTextShowPosition(view_messages_text, pos);
}





void all_messages(char from, char *call_sign, char *from_call, char *message)
{
  char temp_my_course[10];
  char *temp;
  char data1[97];
  char data2[97];
  int pos;
  int i;
  int my_size = 200;
  XmTextPosition drop_ptr;


  if (Read_messages_mine_only
      || (!Read_messages_mine_only
          && ((vm_range == 0)
              || (distance_from_my_station(call_sign,temp_my_course) <= vm_range)) ) )
  {

    // Check that it's coming from the correct type of interface
    // Compare Read_messages_packet_data_type against the port
    // type associated with data_port to determine whether or
    // not to display it.
    //
    // I = Internet
    // L = Local
    // T = TNC
    // F = File
    //
    switch (Read_messages_packet_data_type)
    {

      case 2:     // Display NET data only
        // if not network_interface, return
        if (from != 'I')
        {
          return;  // Don't display it
        }
        break;

      case 1:     // Display TNC data only
        // if not local_tnc_interface, return
        if (from != 'T')
        {
          return;  // Don't display it
        }
        break;

      case 0:     // Display both TNC and NET data
      default:
        break;
    }

    // Check for my stations only if set
    if (Read_messages_mine_only)
    {
      char short_call[MAX_CALLSIGN];
      char *p;

      memcpy(short_call, my_callsign, sizeof(short_call));
      short_call[sizeof(short_call)-1] = '\0';  // Terminate string
      if ( (p = index(short_call,'-')) )
      {
        *p = '\0';  // Terminate it
      }

      if (!strstr(call_sign, short_call)
          && !strstr(from_call, short_call))
      {
        return;
      }
    }

    if ((temp = malloc((size_t)my_size)) == NULL)
    {
      return;
    }

    if (strlen(message)>95)
    {
      xastir_snprintf(data1,
                      sizeof(data1),
                      "%s",
                      message);
      data1[95]='\0';
      xastir_snprintf(data2,
                      sizeof(data2),
                      "\n\t%s",
                      message+95);
    }
    else
    {
      xastir_snprintf(data1,
                      sizeof(data1),
                      "%s",
                      message);
      data2[0] = '\0';
    }

    if (strncmp(call_sign, "java",4) == 0)
    {
      xastir_snprintf(call_sign,
                      MAX_CALLSIGN+1,
                      "%s", langcode("WPUPMSB015") );   // Broadcast
      xastir_snprintf(temp,
                      my_size,
                      "%s %s\t%s%s\n",
                      from_call,
                      call_sign,
                      data1,
                      data2);
    }
    else if (strncmp(call_sign, "USER", 4) == 0)
    {
      xastir_snprintf(call_sign,
                      MAX_CALLSIGN+1,
                      "%s", langcode("WPUPMSB015") );   // Broadcast
      xastir_snprintf(temp,
                      my_size,
                      "%s %s\t%s%s\n",
                      from_call,
                      call_sign,
                      data1,
                      data2);
    }
    else
    {
      char from_str[10];

      xastir_snprintf(from_str, sizeof(from_str), "%c", from);

      strcpy(temp, from_call);
      temp[sizeof(temp)-1] = '\0';  // Terminate string
      strcat(temp, " to ");
      temp[sizeof(temp)-1] = '\0';  // Terminate string
      strcat(temp, call_sign);
      temp[sizeof(temp)-1] = '\0';  // Terminate string
      strcat(temp, " via:");
      temp[sizeof(temp)-1] = '\0';  // Terminate string
      strcat(temp, from_str);
      temp[sizeof(temp)-1] = '\0';  // Terminate string
      strcat(temp, "\t");
      temp[sizeof(temp)-1] = '\0';  // Terminate string
      strcat(temp, data1);
      temp[sizeof(temp)-1] = '\0';  // Terminate string
      strcat(temp, data2);
      temp[sizeof(temp)-1] = '\0';  // Terminate string
      strcat(temp, "\n");
      temp[sizeof(temp)-1] = '\0';  // Terminate string
    }

    if ((All_messages_dialog != NULL))
    {

      begin_critical_section(&All_messages_dialog_lock, "view_message_gui.c:all_messages" );

      pos = (int)XmTextGetLastPosition(view_messages_text);
      XmTextInsert(view_messages_text, pos, temp);
      pos += strlen(temp);
      while (pos > view_message_limit)
      {
        for (drop_ptr = i = 0; i < 3; i++)
        {
          (void)XmTextFindString(view_messages_text, drop_ptr, "\n", XmTEXT_FORWARD, &drop_ptr);
          drop_ptr++;
        }
        XmTextReplace(view_messages_text, 0, drop_ptr, "");
        pos = (int)XmTextGetLastPosition(view_messages_text);
      }
      XtVaSetValues(view_messages_text, XmNcursorPosition, pos, NULL);
      XmTextShowPosition(view_messages_text, pos);

      end_critical_section(&All_messages_dialog_lock, "view_message_gui.c:all_messages" );

    }
    free(temp);
  }
}





void All_messages_destroy_shell( Widget UNUSED(widget), XtPointer clientData, XtPointer UNUSED(callData) )
{
  Widget shell = (Widget) clientData;
  char *temp_ptr;


  temp_ptr = XmTextFieldGetString(vm_dist_data);
  vm_range = atoi(temp_ptr);
  XtFree(temp_ptr);

  XtPopdown(shell);

  begin_critical_section(&All_messages_dialog_lock, "view_message_gui.c:All_messages_destroy_shell" );

  XtDestroyWidget(shell);
  All_messages_dialog = (Widget)NULL;

  end_critical_section(&All_messages_dialog_lock, "view_message_gui.c:All_messages_destroy_shell" );

}





void All_messages_change_range( Widget widget, XtPointer clientData, XtPointer callData)
{
  Widget shell = (Widget) clientData;
  char *temp_ptr;


  temp_ptr = XmTextFieldGetString(vm_dist_data);
  vm_range = atoi(temp_ptr);
  XtFree(temp_ptr);

  XtPopdown(shell);

  All_messages_destroy_shell(widget, clientData, callData);
  view_all_messages(widget, clientData, callData);
}





void  Read_messages_packet_toggle( Widget UNUSED(widget), XtPointer clientData, XtPointer callData)
{
  char *which = (char *)clientData;
  XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

  if(state->set)
  {
    Read_messages_packet_data_type = atoi(which);
  }
  else
  {
    Read_messages_packet_data_type = 0;
  }
}





Widget button_range;





void  Read_messages_mine_only_toggle( Widget UNUSED(widget), XtPointer clientData, XtPointer callData)
{
  char *which = (char *)clientData;
  XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

  if(state->set)
  {
    Read_messages_mine_only = atoi(which);
    XtSetSensitive(vm_dist_data, FALSE);
  }
  else
  {
    Read_messages_mine_only = 0;
    XtSetSensitive(vm_dist_data, TRUE);
  }
}





void view_all_messages( Widget UNUSED(w), XtPointer UNUSED(clientData), XtPointer UNUSED(callData) )
{
  Widget pane, my_form, button_close, dist, dist_units;
  Widget option_box, tnc_data, net_data, tnc_net_data,
         read_mine_only_button;
  unsigned int n;
#define NCNT 50
#define IncN(n) if (n< NCNT) n++; else fprintf(stderr, "Oops, too many arguments for array!\a")
  Arg args[NCNT];
  Atom delw;
  char temp[10];

  if (!All_messages_dialog)
  {

    begin_critical_section(&All_messages_dialog_lock, "view_message_gui.c:view_all_messages" );

    All_messages_dialog = XtVaCreatePopupShell(langcode("AMTMW00001"),
                          xmDialogShellWidgetClass, appshell,
                          XmNdeleteResponse, XmDESTROY,
                          XmNdefaultPosition, FALSE,
                          XmNfontList, fontlist1,
                          NULL);

    pane = XtVaCreateWidget("view_all_messages pane",
                            xmPanedWindowWidgetClass,
                            All_messages_dialog,
                            MY_FOREGROUND_COLOR,
                            MY_BACKGROUND_COLOR,
                            NULL);

    my_form =  XtVaCreateWidget("view_all_messages my_form",
                                xmFormWidgetClass,
                                pane,
                                XmNfractionBase, 5,
                                XmNautoUnmanage, FALSE,
                                XmNshadowThickness, 1,
                                MY_FOREGROUND_COLOR,
                                MY_BACKGROUND_COLOR,
                                NULL);

    dist = XtVaCreateManagedWidget(langcode("AMTMW00002"),
                                   xmLabelWidgetClass,
                                   my_form,
                                   XmNtopAttachment, XmATTACH_FORM,
                                   XmNtopOffset, 10,
                                   XmNbottomAttachment, XmATTACH_NONE,
                                   XmNleftAttachment, XmATTACH_FORM,
                                   XmNleftOffset, 10,
                                   XmNrightAttachment, XmATTACH_NONE,
                                   XmNtraversalOn, FALSE,
                                   MY_FOREGROUND_COLOR,
                                   MY_BACKGROUND_COLOR,
                                   XmNfontList, fontlist1,
                                   NULL);

    vm_dist_data = XtVaCreateManagedWidget("view_all_messages dist_data",
                                           xmTextFieldWidgetClass,
                                           my_form,
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
                                           XmNfontList, fontlist1,
                                           NULL);

    dist_units = XtVaCreateManagedWidget((english_units?langcode("UNIOP00004"):langcode("UNIOP00005")),
                                         xmLabelWidgetClass,
                                         my_form,
                                         XmNtopAttachment, XmATTACH_FORM,
                                         XmNtopOffset, 10,
                                         XmNbottomAttachment, XmATTACH_NONE,
                                         XmNleftAttachment, XmATTACH_WIDGET,
                                         XmNleftWidget, vm_dist_data,
                                         XmNleftOffset, 10,
                                         XmNrightAttachment, XmATTACH_NONE,
                                         XmNtraversalOn, FALSE,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);

    button_range = XtVaCreateManagedWidget(langcode("BULMW00003"),
                                           xmPushButtonGadgetClass,
                                           my_form,
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

    XtAddCallback(button_range, XmNactivateCallback, All_messages_change_range, All_messages_dialog);

    button_close = XtVaCreateManagedWidget(langcode("UNIOP00003"),
                                           xmPushButtonGadgetClass,
                                           my_form,
                                           XmNtopAttachment, XmATTACH_FORM,
                                           XmNtopOffset, 5,
                                           XmNbottomAttachment, XmATTACH_NONE,
                                           XmNleftAttachment, XmATTACH_WIDGET,
                                           XmNleftWidget, button_range,
                                           XmNleftOffset, 10,
                                           XmNrightAttachment, XmATTACH_FORM,
                                           XmNnavigationType, XmTAB_GROUP,
                                           MY_FOREGROUND_COLOR,
                                           MY_BACKGROUND_COLOR,
                                           XmNfontList, fontlist1,
                                           NULL);

    XtAddCallback(button_close, XmNactivateCallback, All_messages_destroy_shell, All_messages_dialog);

    n=0;
    XtSetArg(args[n],XmNforeground, MY_FG_COLOR);
    n++;
    XtSetArg(args[n],XmNbackground, MY_BG_COLOR);
    n++;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET);
    n++;
    XtSetArg(args[n], XmNtopWidget, dist);
    n++;
    XtSetArg(args[n], XmNtopOffset, 5);
    n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_NONE);
    n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM);
    n++;
    XtSetArg(args[n], XmNleftOffset, 5);
    n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_NONE);
    n++;
    XtSetArg(args[n], XmNfontList, fontlist1);
    n++;

    option_box = XmCreateRadioBox(my_form,
                                  "Vew Messages option box",
                                  args,
                                  n);

    XtVaSetValues(option_box,
                  XmNpacking, XmPACK_TIGHT,
                  XmNorientation, XmHORIZONTAL,
                  NULL);

    tnc_data = XtVaCreateManagedWidget(langcode("WPUPDPD002"),
                                       xmToggleButtonGadgetClass,
                                       option_box,
                                       MY_FOREGROUND_COLOR,
                                       MY_BACKGROUND_COLOR,
                                       XmNfontList, fontlist1,
                                       NULL);

    XtAddCallback(tnc_data,XmNvalueChangedCallback,Read_messages_packet_toggle,"1");

    net_data = XtVaCreateManagedWidget(langcode("WPUPDPD003"),
                                       xmToggleButtonGadgetClass,
                                       option_box,
                                       MY_FOREGROUND_COLOR,
                                       MY_BACKGROUND_COLOR,
                                       XmNfontList, fontlist1,
                                       NULL);

    XtAddCallback(net_data,XmNvalueChangedCallback,Read_messages_packet_toggle,"2");

    tnc_net_data = XtVaCreateManagedWidget(langcode("WPUPDPD004"),
                                           xmToggleButtonGadgetClass,
                                           option_box,
                                           MY_FOREGROUND_COLOR,
                                           MY_BACKGROUND_COLOR,
                                           XmNfontList, fontlist1,
                                           NULL);

    XtAddCallback(tnc_net_data,XmNvalueChangedCallback,Read_messages_packet_toggle,"0");

    read_mine_only_button = XtVaCreateManagedWidget(langcode("WPUPDPD008"),
                            xmToggleButtonGadgetClass,
                            my_form,
                            XmNvisibleWhenOff, TRUE,
                            XmNindicatorSize, 12,
                            XmNtopAttachment, XmATTACH_WIDGET,
                            XmNtopWidget, dist,
                            XmNtopOffset, 10,
                            XmNbottomAttachment, XmATTACH_NONE,
                            XmNleftAttachment, XmATTACH_WIDGET,
                            XmNleftWidget, option_box,
                            XmNleftOffset, 20,
                            XmNrightAttachment, XmATTACH_NONE,
                            MY_FOREGROUND_COLOR,
                            MY_BACKGROUND_COLOR,
                            XmNfontList, fontlist1,
                            NULL);

    XtAddCallback(read_mine_only_button,XmNvalueChangedCallback,Read_messages_mine_only_toggle,"1");

    n=0;
    XtSetArg(args[n], XmNrows, 15);
    IncN(n);
    XtSetArg(args[n], XmNcolumns, 85);
    IncN(n);
    XtSetArg(args[n], XmNeditable, FALSE);
    IncN(n);
    XtSetArg(args[n], XmNtraversalOn, TRUE);
    IncN(n);
    XtSetArg(args[n], XmNlistSizePolicy, XmVARIABLE);
    IncN(n);
    XtSetArg(args[n], XmNeditMode, XmMULTI_LINE_EDIT);
    IncN(n);
    XtSetArg(args[n], XmNwordWrap, TRUE);
    IncN(n);
    XtSetArg(args[n], XmNscrollHorizontal, TRUE);
    IncN(n);
    XtSetArg(args[n], XmNscrollVertical, TRUE);
    IncN(n);
//        XtSetArg(args[n], XmNscrollBarDisplayPolicy, XmSTATIC); IncN(n);
    XtSetArg(args[n], XmNselectionPolicy, XmMULTIPLE_SELECT);
    IncN(n);
    XtSetArg(args[n], XmNcursorPositionVisible, FALSE);
    IncN(n);
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET);
    IncN(n);
    XtSetArg(args[n], XmNtopWidget, option_box);
    IncN(n);
    XtSetArg(args[n], XmNtopOffset, 5);
    IncN(n);
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM);
    IncN(n);
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM);
    IncN(n);
    XtSetArg(args[n], XmNleftOffset, 5);
    IncN(n);
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM);
    IncN(n);
    XtSetArg(args[n], XmNrightOffset, 5);
    IncN(n);
    XtSetArg(args[n], XmNforeground, MY_FG_COLOR);
    IncN(n);
    XtSetArg(args[n], XmNbackground, MY_BG_COLOR);
    IncN(n);
    XtSetArg(args[n], XmNfontList, fontlist1);
    n++;

    view_messages_text = XmCreateScrolledText(my_form,
                         "view_all_messages text",
                         args,
                         n);

// It's hard to get tab groups working with ScrolledText widgets.  Tab'ing in is
// fine, but then I'm stuck in insert mode and it absorbs the tabs and beeps.

    pos_dialog(All_messages_dialog);

    delw = XmInternAtom(XtDisplay(All_messages_dialog),"WM_DELETE_WINDOW", FALSE);
    XmAddWMProtocolCallback(All_messages_dialog, delw, All_messages_destroy_shell, (XtPointer)All_messages_dialog);

    sprintf(temp,"%d",vm_range);
    XmTextFieldSetString(vm_dist_data,temp);

    switch (Read_messages_packet_data_type)
    {
      case(0):
        XmToggleButtonSetState(tnc_net_data,TRUE,FALSE);
        break;

      case(1):
        XmToggleButtonSetState(tnc_data,TRUE,FALSE);
        break;

      case(2):
        XmToggleButtonSetState(net_data,TRUE,FALSE);
        break;

      default:
        XmToggleButtonSetState(tnc_net_data,TRUE,FALSE);
        break;
    }

    if (Read_messages_mine_only)
    {
      XmToggleButtonSetState(read_mine_only_button,TRUE,FALSE);
      XtSetSensitive(vm_dist_data, FALSE);
    }
    else
    {
      XmToggleButtonSetState(read_mine_only_button,FALSE,FALSE);
      XtSetSensitive(vm_dist_data, TRUE);
    }

    XtManageChild(option_box);
    XtManageChild(view_messages_text);
    XtVaSetValues(view_messages_text, XmNbackground, colors[0x0f], NULL);
    XtManageChild(my_form);
    XtManageChild(pane);

    redraw_on_new_packet_data=1;

    // Dump all currently active messages to the new window
    view_message_display_file('M');

    end_critical_section(&All_messages_dialog_lock, "view_message_gui.c:view_all_messages" );

    XtPopup(All_messages_dialog,XtGrabNone);
//        fix_dialog_vsize(All_messages_dialog);

    // Move focus to the Close button.  This appears to highlight the
    // button fine, but we're not able to hit the <Enter> key to
    // have that default function happen.  Note:  We _can_ hit the
    // <SPACE> key, and that activates the option.
//        XmUpdateDisplay(All_messages_dialog);
    XmProcessTraversal(button_close, XmTRAVERSE_CURRENT);


  }
  else
  {
    (void)XRaiseWindow(XtDisplay(All_messages_dialog), XtWindow(All_messages_dialog));
  }
}


