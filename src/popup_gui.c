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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <Xm/XmAll.h>

#include "xastir.h"
#include "main.h"
#include "popup.h"
#include "main.h"
#include "lang.h"
#include "rotated.h"
#include "snprintf.h"

// Must be last include file
#include "leak_detection.h"


extern XmFontList fontlist1;    // Menu/System fontlist

static Popup_Window pw[MAX_POPUPS];
static Popup_Window pwb;

static xastir_mutex popup_message_dialog_lock;





void popup_gui_init(void)
{
  init_critical_section( &popup_message_dialog_lock );
}





/**** Popup Message ******/

void clear_popup_message_windows(void)
{
  int i;

  begin_critical_section(&popup_message_dialog_lock, "popup_gui.c:clear_popup_message_windows" );

  for (i=0; i<MAX_POPUPS; i++)
  {
    pw[i].popup_message_dialog=(Widget)NULL;
    pw[i].popup_message_data=(Widget)NULL;
  }

  end_critical_section(&popup_message_dialog_lock, "popup_gui.c:clear_popup_message_windows" );

  pwb.popup_message_dialog=(Widget)NULL;
  pwb.popup_message_data=(Widget)NULL;
}





static void popup_message_destroy_shell(Widget UNUSED(w),
                                        XtPointer clientData,
                                        XtPointer UNUSED(callData) )
{
  int i;

  i=atoi((char *)clientData);
  XtPopdown(pw[i].popup_message_dialog);

  begin_critical_section(&popup_message_dialog_lock, "popup_gui.c:popup_message_destroy_shell" );

  XtDestroyWidget(pw[i].popup_message_dialog);
  pw[i].popup_message_dialog = (Widget)NULL;
  pw[i].popup_message_data = (Widget)NULL;

  end_critical_section(&popup_message_dialog_lock, "popup_gui.c:popup_message_destroy_shell" );

}





time_t popup_time_out_check_last = (time_t)0l;

void popup_time_out_check(int curr_sec)
{
  int i;

  // Check only every two minutes or so
  if (popup_time_out_check_last + 120 < curr_sec)
  {
    popup_time_out_check_last = curr_sec;

    for (i=0; i<MAX_POPUPS; i++)
    {
      if (pw[i].popup_message_dialog!=NULL)
      {
        if ((sec_now()-pw[i].sec_opened)>MAX_POPUPS_TIME)
        {
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
}





void popup_message_always(char *banner, char *message)
{
  XmString msg_str;
  int j,i;
  Atom delw;


  if (disable_all_popups)
  {
    return;
  }

  if (banner == NULL || message == NULL)
  {
    return;
  }

  i=0;
  for (j=0; j<MAX_POPUPS; j++)
  {
    if (!pw[j].popup_message_dialog)
    {
      i=j;
      j=MAX_POPUPS+1;
    }
  }

  if(!pw[i].popup_message_dialog)
  {
    if (banner!=NULL && message!=NULL)
    {

      begin_critical_section(&popup_message_dialog_lock, "popup_gui.c:popup_message" );

      pw[i].popup_message_dialog = XtVaCreatePopupShell(banner,
                                   xmDialogShellWidgetClass, appshell,
                                   XmNdeleteResponse, XmDESTROY,
                                   XmNdefaultPosition, FALSE,
                                   XmNtitleString,banner,
// An half-hearted attempt at fixing the problem where a popup
// comes up extremely small.  Setting a minimum size for the popup.
                                   XmNminWidth, 220,
                                   XmNminHeight, 80,
                                   XmNfontList, fontlist1,
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
                                 XmNfontList, fontlist1,
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
                           XmNfontList, fontlist1,
                           NULL);

      xastir_snprintf(pw[i].name,10,"%9d",i%1000);

      msg_str=XmStringCreateLtoR(message,XmFONTLIST_DEFAULT_TAG);
      XtVaSetValues(pw[i].popup_message_data,XmNlabelString,msg_str,NULL);
      XmStringFree(msg_str);

      XtAddCallback(pw[i].button_close, XmNactivateCallback, popup_message_destroy_shell,(XtPointer)pw[i].name);

      delw = XmInternAtom(XtDisplay(pw[i].popup_message_dialog),"WM_DELETE_WINDOW", FALSE);

      XmAddWMProtocolCallback(pw[i].popup_message_dialog, delw, popup_message_destroy_shell, (XtPointer)pw[i].name);

      pos_dialog(pw[i].popup_message_dialog);

      XtManageChild(pw[i].form);
      XtManageChild(pw[i].pane);

      end_critical_section(&popup_message_dialog_lock, "popup_gui.c:popup_message" );

      XtPopup(pw[i].popup_message_dialog,XtGrabNone);

// An half-hearted attempt at fixing the problem where a popup
// comes up extremely small.  Commenting out the below so we can
// change the size if necessary to read the message.
//            fix_dialog_size(pw[i].popup_message_dialog);

      pw[i].sec_opened=sec_now();
    }
  }
}





#ifndef HAVE_ERROR_POPUPS
//
// We'll write to STDERR instead since the user doesn't want to see
// any error popups.  Add a timestamp to the front so we know when
// the errors happened.
//
void popup_message(char *banner, char *message)
{
  char timestring[110];


  if (disable_all_popups)
  {
    return;
  }

  if (banner == NULL || message == NULL)
  {
    return;
  }

  get_timestamp(timestring);
  fprintf(stderr, "%s:\n\t%s  %s\n\n", timestring, banner, message);
}
#else   // HAVE_ERROR_POPUPS
//
// The user wishes to see popup error messages.  Call the routine
// above which does so.
//
void popup_message(char *banner, char *message)
{
  popup_message_always(banner, message);
}
#endif  // HAVE_ERROR_POPUPS






// Must make sure that fonts are not loaded again and again, as this
// takes a big chunk of memory each time.  Can you say "memory
// leak"?
//
static XFontStruct *id_font=NULL;





// Routine which pops up a large message for a few seconds in the
// middle of the screen, then removes it.
//
void popup_ID_message(char * UNUSED(banner), char *message)
{
  float my_rotation = 0.0;
  int x = (int)(screen_width/10);
  int y = (int)(screen_height/2);

  if (ATV_screen_ID)
  {

    // Fill the pixmap with grey so that the black ID text will
    // be seen.
    (void)XSetForeground(XtDisplay(da),gc,MY_BG_COLOR); // Not a mistake!
    (void)XSetBackground(XtDisplay(da),gc,MY_BG_COLOR);
    (void)XFillRectangle(XtDisplay(appshell),
                         pixmap_alerts,
                         gc,
                         0,
                         0,
                         (unsigned int)screen_width,
                         (unsigned int)screen_height);

    /* load font */
    if(!id_font)
    {
      id_font=(XFontStruct *)XLoadQueryFont (XtDisplay(da), rotated_label_fontname[FONT_ATV_ID]);

      if (id_font == NULL)      // Couldn't get the font!!!
      {
        fprintf(stderr,"popup_ID_message: Couldn't get ATV ID font %s\n",
                rotated_label_fontname[FONT_ATV_ID]);
        pending_ID_message = 0;
        return;
      }
    }

    (void)XSetForeground (XtDisplay(da), gc, colors[0x08]);

    //fprintf(stderr,"%0.1f\t%s\n",my_rotation,label_text);

    if (       ( (my_rotation < -90.0) && (my_rotation > -270.0) )
               || ( (my_rotation >  90.0) && (my_rotation <  270.0) ) )
    {
      my_rotation = my_rotation + 180.0;
      (void)XRotDrawAlignedString(XtDisplay(da),
                                  id_font,
                                  my_rotation,
                                  pixmap_alerts,
                                  gc,
                                  x,
                                  y,
                                  message,
                                  BRIGHT);
    }
    else
    {
      (void)XRotDrawAlignedString(XtDisplay(da),
                                  id_font,
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
    (void)XCopyArea(XtDisplay(da),
                    pixmap_alerts,
                    XtWindow(da),
                    gc,
                    0,
                    0,
                    (unsigned int)screen_width,
                    (unsigned int)screen_height,
                    0,
                    0);
  }
  else    // ATV Screen ID is not enabled
  {
    pending_ID_message = 0;
  }
}


