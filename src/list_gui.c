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
#include <assert.h>
#include <ctype.h>

#ifdef  HAVE_LOCALE_H
  #include <locale.h>
#endif  // HAVE_LOCALE_H

#include <Xm/XmAll.h>

#ifdef HAVE_XBAE_MATRIX_H
  #include <Xbae/Matrix.h>
#endif  // HAVE_XBAE_MATRIX_H

#include "xastir.h"
#include "main.h"
#include "messages.h"
#include "draw_symbols.h"
#include "list_gui.h"
#include "database.h"

#include <stdlib.h>
#include <stdio.h>

// Must be last include file
#include "leak_detection.h"

extern XmFontList fontlist1;    // Menu/System fontlist

#define SL_MAX 20
#define ROWS 17

// List Numbers (defined in list_gui.h)
// 0: LST_ALL   - all stations list
// 1: LST_MOB   - mobile stations list
// 2: LST_WX    - WX stations list
// 3: LST_TNC   - local stations list
// 4: LST_TIM   - last stations
// 5: LST_OBJ   - Objects/Items
// 6: LST_MYOBJ - My Objects/Items
// 7: LST_NUM   - Number of lists; for use in array definitions below

Widget station_list_dialog[LST_NUM];           // store list definitions
static xastir_mutex station_list_dialog_lock;  // Mutex lock for above

Widget SL_list[LST_NUM][SL_MAX];
Widget SL_da[LST_NUM][SL_MAX];
Widget SL_call[LST_NUM][SL_MAX];
char * SL_callback[LST_NUM][SL_MAX];
Pixmap SL_icon[LST_NUM][SL_MAX];        // icons for different lists and list rows
Pixmap blank_icon;                      // holds an empty icon
Widget SL_scroll[LST_NUM];
Widget SL_wx_wind_course[LST_NUM][SL_MAX];
Widget SL_wx_wind_speed[LST_NUM][SL_MAX];
Widget SL_wx_wind_gust[LST_NUM][SL_MAX];
Widget SL_wx_temp[LST_NUM][SL_MAX];
Widget SL_wx_hum[LST_NUM][SL_MAX];
Widget SL_wx_baro[LST_NUM][SL_MAX];
Widget SL_wx_rain_h[LST_NUM][SL_MAX];
Widget SL_wx_rain_00[LST_NUM][SL_MAX];
Widget SL_wx_rain_24[LST_NUM][SL_MAX];
Widget SL_course[LST_NUM][SL_MAX];
Widget SL_speed[LST_NUM][SL_MAX];
Widget SL_alt[LST_NUM][SL_MAX];
Widget SL_lat_long[LST_NUM][SL_MAX];
Widget SL_packets[LST_NUM][SL_MAX];
Widget SL_sats[LST_NUM][SL_MAX];
Widget SL_my_course[LST_NUM][SL_MAX];
Widget SL_my_distance[LST_NUM][SL_MAX];
Widget SL_pos_time[LST_NUM][SL_MAX];
Widget SL_node_path[LST_NUM][SL_MAX];
Widget SL_power_gain[LST_NUM][SL_MAX];
Widget SL_comments[LST_NUM][SL_MAX];
int station_list_first = 1;
int list_size_h[LST_NUM];       // height of entire list widget
int list_size_w[LST_NUM];       // width  of entire list widget
int list_size_i[LST_NUM];       // size initialized, dirty hack, but works...

int last_offset[LST_NUM];
char top_call[LST_NUM][MAX_CALLSIGN+1]; // call of first list entry or empty string for always first call
time_t top_time;                // time of first list entry or 0 for always newest station
int top_sn;                     // serial number for unique time index
time_t last_list_upd;           // time of last list update
int units_last;

#define LIST_UPDATE_CYCLE 2     /* Minimum time between list updates in seconds, we want */
/* immediate update, but not in high traffic situations  */





void list_gui_init(void)
{
  init_critical_section( &station_list_dialog_lock );
}





// get a valid list member, starting from current station in the desired direction
// returns pointer to found member or NULL
void get_list_member(int type, DataRow **p_station, int skip, int forward)
{
  char found;

  if ((*p_station) == NULL)                           // default start value
  {
    if (type == LST_TIM)
    {
      (*p_station) = t_newest;
    }
    else
    {
      (*p_station) = n_first;
    }
  }

  if (skip == 1)                                      // skip before searching
  {
    if (type != LST_TIM)
    {
      if (forward == 1)
      {
        (void)next_station_name(p_station);
      }
      else
      {
        (void)prev_station_name(p_station);
      }
    }
    else
    {
      if (forward == 1)
      {
        (void)prev_station_time(p_station);
      }
      else
      {
        (void)next_station_time(p_station);
      }
    }
  }

  found = (char)FALSE;
  switch (type)               // DK7IN: here I'm trading code size for speed...
  {
    case LST_ALL:
      if (forward == 1)
        while (!found && (*p_station) != NULL)
        {
          if (((*p_station)->flag & ST_ACTIVE) != 0)  // ignore deleted objects
          {
            found = (char)TRUE;
          }
          else
          {
            (*p_station) = (*p_station)->n_next;
          }
        }
      else
        while (!found && (*p_station) != NULL)
        {
          if (((*p_station)->flag & ST_ACTIVE) != 0)
          {
            found = (char)TRUE;
          }
          else
          {
            (*p_station) = (*p_station)->n_prev;
          }
        }
      break;
    case LST_MOB:
      if (forward == 1)
        while (!found && (*p_station) != NULL)
        {
          if (((*p_station)->flag & ST_ACTIVE) != 0 && (*p_station)->newest_trackpoint != NULL)
          {
            found = (char)TRUE;
          }
          else
          {
            (*p_station) = (*p_station)->n_next;
          }
        }
      else
        while (!found && (*p_station) != NULL)
        {
          if (((*p_station)->flag & ST_ACTIVE) != 0 && (*p_station)->newest_trackpoint != NULL)
          {
            found = (char)TRUE;
          }
          else
          {
            (*p_station) = (*p_station)->n_prev;
          }
        }
      break;
    case LST_WX:
      if (forward == 1)
        while (!found && (*p_station) != NULL)
        {
          if (((*p_station)->flag & ST_ACTIVE) != 0 && (*p_station)->weather_data != NULL)
          {
            found = (char)TRUE;
          }
          else
          {
            (*p_station) = (*p_station)->n_next;
          }
        }
      else
        while (!found && (*p_station) != NULL)
        {
          if (((*p_station)->flag & ST_ACTIVE) != 0 && (*p_station)->weather_data != NULL)
          {
            found = (char)TRUE;
          }
          else
          {
            (*p_station) = (*p_station)->n_prev;
          }
        }
      break;
    case LST_TNC:
      if (forward == 1)
        while (!found && (*p_station) != NULL)
        {
          if (((*p_station)->flag & ST_ACTIVE) != 0
              && ((*p_station)->flag & ST_VIATNC) != 0)
          {
            found = (char)TRUE;
          }
          else
          {
            (*p_station) = (*p_station)->n_next;
          }
        }
      else
        while (!found && (*p_station) != NULL)
        {
          if (((*p_station)->flag & ST_ACTIVE) != 0
              && ((*p_station)->flag & ST_VIATNC) != 0)
          {
            found = (char)TRUE;
          }
          else
          {
            (*p_station) = (*p_station)->n_prev;
          }
        }
      break;
    case LST_TIM:
      if (forward == 1)           // forward in list, backward in time
        while (!found && (*p_station) != NULL)
        {
          if (((*p_station)->flag & ST_ACTIVE) != 0)  // ignore deleted objects
          {
            found = (char)TRUE;
          }
          else
          {
            (*p_station) = (*p_station)->t_older;
          }
        }
      else
        while (!found && (*p_station) != NULL)
        {
          if (((*p_station)->flag & ST_ACTIVE) != 0)
          {
            found = (char)TRUE;
          }
          else
          {
            (*p_station) = (*p_station)->t_newer;
          }
        }
      break;
    case LST_OBJ:
      if (forward == 1)
        while (!found && (*p_station) != NULL)
        {
          // Show deleted objects/items as well
          if ( ( (((*p_station)->flag & ST_OBJECT) != 0)
                 || (((*p_station)->flag & ST_ITEM) != 0) ) )
          {
            found = (char)TRUE;
          }
          else
          {
            (*p_station) = (*p_station)->n_next;
          }
        }
      else
        while (!found && (*p_station) != NULL)
        {
          if (((*p_station)->flag & ST_ACTIVE) != 0
              && ( (((*p_station)->flag & ST_VIATNC) != 0)
                   || (((*p_station)->flag & ST_ITEM) != 0) ) )
          {
            found = (char)TRUE;
          }
          else
          {
            (*p_station) = (*p_station)->n_prev;
          }
        }
      break;
    case LST_MYOBJ:

// We should really show the active AND inactive objects.  This is
// so that inactive ones can be resurrected.  Probably should do
// this for all objects, not just ones we own.

      if (forward == 1)
        while (!found && (*p_station) != NULL)
        {
          // Show deleted objects/items as well
          if ( ( (((*p_station)->flag & ST_OBJECT) != 0)
                 || (((*p_station)->flag & ST_ITEM) != 0) )
//                            && ( is_my_call( (*p_station)->origin,1)) ) // Exact match include SSID
               && ( is_my_object_item(*p_station)) ) // Exact match include SSID

          {
            found = (char)TRUE;
          }
          else
          {
            (*p_station) = (*p_station)->n_next;
          }
        }
      else
        while (!found && (*p_station) != NULL)
        {
          if (((*p_station)->flag & ST_ACTIVE) != 0
              && ( (((*p_station)->flag & ST_VIATNC) != 0)
                   || (((*p_station)->flag & ST_ITEM) != 0) )
//                            && ( is_my_call( (*p_station)->origin,1)) ) // Exact match includes SSID
              && ( is_my_object_item(*p_station)) ) // Exact match include SSID

          {
            found = (char)TRUE;
          }
          else
          {
            (*p_station) = (*p_station)->n_prev;
          }
        }
      break;
    default:
      break;
  }
}





// initialization of station list at very first Station List call
void init_station_lists(void)
{
  int type,i;

  blank_icon = XCreatePixmap(XtDisplay(appshell),RootWindowOfScreen(XtScreen(appshell)),20,20,
                             DefaultDepthOfScreen(XtScreen(appshell)));

  begin_critical_section(&station_list_dialog_lock, "list_gui.c:init_station_lists" );

  for (type=0; type<LST_NUM; type++)
  {
    station_list_dialog[type] = NULL;       // set list to undefined
    for (i=0; i<ROWS; i++)
    {
      SL_icon[type][i] = XCreatePixmap(XtDisplay(appshell),RootWindowOfScreen(XtScreen(appshell)),20,20,
                                       DefaultDepthOfScreen(XtScreen(appshell)));
    }
  }
  memset(SL_callback, 0, sizeof(SL_callback));

  end_critical_section(&station_list_dialog_lock, "list_gui.c:init_station_lists" );

}





// check if there is at least one station of a specific type      now used only in db.c
int stations_types(int type)
{
  int st;
  DataRow *p_station;

  st=0;
  for (p_station=n_first; p_station != NULL; p_station=p_station->n_next)
  {
    if ((p_station->flag & ST_ACTIVE) != 0)        // ignore deleted objects
    {
      switch (type)
      {
        case 0:         // all stations list
        case 4:         // last stations
          st++;
          break;
        case 1:         // mobile stations list
          if (p_station->newest_trackpoint != NULL)
          {
            st++;
          }
          break;
        case 2:         // WX stations list
          if (p_station->weather_data != NULL)
          {
            st++;
          }
          break;
        case 3:         // local stations list
          if ((p_station->flag & ST_VIATNC) != 0)
          {
            st++;
          }
          break;
        case 5:         // Object/Item list
          if ( ((p_station->flag & ST_OBJECT) != 0) ||
               ((p_station->flag & ST_ITEM) != 0) )
          {
            st++;
          }
          break;
        default:
          break;
      }
    }
  }
  if (st==0)
  {
    st=1;
  }
  return(st);
}





void station_list_destroy_shell( Widget UNUSED(widget), XtPointer clientData, XtPointer UNUSED(callData) )
{
  int type;
  int i;

  type = atoi((char *)clientData);
  XtPopdown(station_list_dialog[type]);

  begin_critical_section(&station_list_dialog_lock, "list_gui.c:station_list_destroy_shell" );

  for (i = 0; i < ROWS; i++)
  {
    if (SL_callback[type][i])
    {
      XtFree(SL_callback[type][i]);
      SL_callback[type][i] = NULL;
    }
  }

  XtDestroyWidget(station_list_dialog[type]);
  station_list_dialog[type] = (Widget)NULL;           // clear list definition

  end_critical_section(&station_list_dialog_lock, "list_gui.c:station_list_destroy_shell" );

}



/*
 * Callback for station icon in list.
 * Calls a function to center the map on the selected station from the list.
 */
void Call_locate_station(Widget w, XtPointer clientData, XtPointer UNUSED(callData) )
{
  if (clientData != NULL && strlen(clientData) > 0)
  {
    locate_station(w, clientData, 1,1,1);
  }
}



/*
 * *** This callback is not linked to a control on list yet. ***
 * Invokes the station information dialog for the selected station from the list.
 */
void Call_Station_data(Widget w, XtPointer clientData, XtPointer UNUSED(callData) )
{
  if (clientData != NULL && strlen(clientData) > 0)
  {
    Station_data(w, clientData, NULL);
  }
}



/*
 *  Fill list with new data
 */
void Station_List_fill(int type, int new_offset)
{
  int row;
  char temp[11];
  char *temp_ptr;
  Dimension w,h;            // size of scrollbar in pixel
  Dimension ww,wh;          // size of entire widget in pixel
  Dimension new_h;          // overall height in pixel
  char stemp[400];
  char stemp1[60];
  char stemp2[60];
  char temp_call[MAX_CALLSIGN+1];
  long l_lat, l_lon;
  float value;
  WeatherRow *weather;
  DataRow *p_station;
  DataRow *p_name;
  int cur_offset;
  Dimension count, inview;
  int to_move, rows;
  int strwid;

  assert(ROWS <= SL_MAX);
#define HGT 26
#define FUDGE 78
  // type 0 all, 1: mobile, 2: WX, 3: local, 4: time, 5: Objects/Items
  // offset is the entry that should be displayed in the first line

  w = h = ww = wh = 0;
  if (station_list_dialog[type] != NULL)              // if list is defined
  {
    // the list is first drawn very big then gets initialized to the correct size
    // I don't want the first wrong draw but don't know how to avoid it.  DK7IN

    // there are wrong icons drawn on the first time...  ????
    if (list_size_i[type])          // we are initialized...
    {
      // ww, wh  get width and height of entire widget:
      XtVaGetValues(station_list_dialog[type], XmNwidth,  &ww, XmNheight, &wh, NULL);
      // w, h    get width and height of scrollbar:
      XtVaGetValues(SL_scroll[type], XmNwidth,  &w, XmNheight, &h, NULL);
      rows = (h+10) / HGT;         // number of rows we can display, HGT pixel per row
//            fprintf(stderr,"fill: %d %d %d\n",wh, h, rows);
    }
    else
    {
      if (list_size_w[type] > 0 && list_size_h[type] > 0)
      {
        wh = list_size_h[type];         // restore size
        ww = list_size_w[type];
        rows = (wh -FUDGE+10) / HGT;              // Fudge Factor ???
//                fprintf(stderr,"load: %d       %d\n",wh, rows);
      }
      else
      {
        wh = 500;                       // start with this... ???
        ww = 700;
        rows = (wh -FUDGE+10) / HGT;              // Fudge Factor ???
//                fprintf(stderr,"set:  %d       %d\n",wh, rows);
      }
      XtVaSetValues(station_list_dialog[type], XmNwidth, ww, NULL); // set widget width
    }
    if (rows > ROWS)          // limit vertical size to data structure size for widgets
    {
      rows = ROWS;
    }
    if (rows < 1)
    {
      rows = 1;
    }
    // new_h = (rows*HGT) + (wh-h);    // (rows + border) in pixel
    new_h = (rows*HGT) + FUDGE;    // (rows + border) in pixel
    XtVaSetValues(station_list_dialog[type], XmNheight, new_h, NULL);       // correct widget height

    list_size_h[type] = new_h;                      // remember current size
    list_size_w[type] = ww;
    list_size_i[type] = (int)TRUE;                       // don't init next time

    // look for the station to display on the first row...
    p_station = NULL;
    if (type == LST_TIM)
    {
      (void)search_station_time(&p_station,top_time,top_sn);  // gives match or next station
    }
    else
    {
      (void)search_station_name(&p_station,top_call[type],1);  // gives match or next station
    }
    get_list_member(type, &p_station, 0, 1);        // get member in list for first row

    // get updated statistics, database may have changed since last call
    count = 0;
    cur_offset = 0;
    p_name = NULL;
    get_list_member(type, &p_name, 0, 1);           // get first member in list

    while (p_name != NULL)
    {
      if (p_name == p_station)
      {
        cur_offset = count;  // got offset of first row entry
      }
      count++;                                    // count valid list member
      get_list_member(type, &p_name, 1, 1);       // get next member in list
    }

    // check boundaries
    new_offset += cur_offset - last_offset[type];   // adjust for database changes

    if (count == 0)
    {
      count = 1;  // empty
    }
    if (count - new_offset < rows)                  // bottom
    {
      new_offset = count - rows;  // keep page filled, if possible
    }
    if (new_offset < 0)                             // top
    {
      new_offset = 0;
    }
    inview = rows;                                  // number of stations in view
    if (inview > count)                             // partially filled
    {
      inview = count;
    }

    // update scrollbar parameters
    XtVaSetValues(SL_scroll[type],
                  XmNmaximum, count,
                  XmNpageIncrement, inview,
                  XmNvalue, new_offset,
                  XmNsliderSize, inview,
                  NULL);

    if (new_offset == 0)                            // stay at first member
    {
      p_station = NULL;
      get_list_member(type, &p_station, 0, 1);    // get first member in list
    }
    else
    {
      // if database changed, adjust first entry accordingly
      to_move = new_offset - cur_offset;
      while (to_move > 0)                                 // move down, if neccessary
      {
        if (p_station != 0)
        {
          get_list_member(type, &p_station, 1, 1);  // gets next member in list
        }
        to_move--;
      }
      while (to_move < 0)                                 // move up, if neccessary
      {
        if (p_station != 0)
        {
          get_list_member(type, &p_station, 1, 0);  // gets previous member in list
        }
        to_move++;
      }
    }

    // store current start position, we need a unique index for this to work (time?)
    if (new_offset == 0 || p_station == NULL)               // keep it at top
    {
      if (type != LST_TIM)
      {
        top_call[type][0] = '\0';
      }
      else
      {
        top_time =  0;
        top_sn   = -1;
      }
    }
    else
    {
      if (type != LST_TIM)
      {
        xastir_snprintf(top_call[type],
                        MAX_CALLSIGN+1,
                        "%s",
                        p_station->call_sign);  // remember call at list top
      }
      else
      {
        top_time = p_station->sec_heard;                // remember time station was heard
        top_sn   = p_station->time_sn;                  // remember time serial number
      }
    }
    last_offset[type] = new_offset;

    // now fill the list rows
    xastir_snprintf(temp, sizeof(temp), "%d", (rows+new_offset));                   // calculate needed string width
    strwid = (int)strlen(temp);                             // to keep it right justified

    begin_critical_section(&station_list_dialog_lock, "list_gui.c:Station_List_fill" );

    // Start filling in the rows of the widget
    for (row=0; row<rows; row++)                            // loop over display lines
    {
      if (p_station != NULL)                              // we have data...
      {
        int ghost;

        // icon
        XtVaSetValues(SL_da[type][row],XmNlabelPixmap, blank_icon,NULL);
        XtManageChild(SL_da[type][row]);

        if (type == LST_OBJ || type == LST_MYOBJ)
        {
          if (p_station->flag & ST_ACTIVE)
          {
            ghost = 0;      // Active object/item
          }
          else
          {
            ghost = 1;      // Deleted object/item
          }
        }
        else
        {
          ghost = 0;          // Not an object
        }

        // Blank out the icon first
        XtVaSetValues(SL_da[type][row],XmNlabelPixmap, blank_icon,NULL);
        XtManageChild(SL_da[type][row]);
        symbol(SL_da[type][row],0,'~','$','\0',SL_icon[type][row],0,0,0,' ');
        XtVaSetValues(SL_da[type][row],XmNlabelPixmap, SL_icon[type][row],NULL);
        XtManageChild(SL_da[type][row]);

        // Now redraw it
        symbol(SL_da[type][row],ghost,p_station->aprs_symbol.aprs_type,
               p_station->aprs_symbol.aprs_symbol,
               p_station->aprs_symbol.special_overlay,SL_icon[type][row],ghost,0,0,' ');

        XtVaSetValues(SL_da[type][row],XmNlabelPixmap, SL_icon[type][row],NULL);
        XtManageChild(SL_da[type][row]);

        if (SL_callback[type][row])
        {
          XtRemoveCallback((XtPointer)SL_da[type][row],
                           XmNactivateCallback,
                           Call_locate_station,
                           SL_callback[type][row]);
          XtFree(SL_callback[type][row]);
        }
        SL_callback[type][row] = XmTextFieldGetString(SL_call[type][row]);

        // Pressing the icon button centers the map on the station.
        XtAddCallback( (XtPointer)SL_da[type][row],
                       XmNactivateCallback,
                       Call_locate_station,
                       SL_callback[type][row]
                     );

        // number in list
        xastir_snprintf(temp, sizeof(temp), "%*d", strwid, (row+1+new_offset));
        XmTextFieldSetString(SL_list[type][row],temp);
        XtManageChild(SL_list[type][row]);

        // call (or object/item name)
        /* check to see if string changed and over write */
        temp_ptr = XmTextFieldGetString(SL_call[type][row]);
        xastir_snprintf(temp_call, sizeof(temp_call), "%s", temp_ptr);
        XtFree(temp_ptr);

        if (strcmp(temp_call,p_station->call_sign) !=0 )
        {
          XmTextFieldSetString(SL_call[type][row],p_station->call_sign);
          if (ghost)
          {
            XtSetSensitive(SL_call[type][row],FALSE);
          }
          else
          {
            XtSetSensitive(SL_call[type][row],TRUE);
          }
          XtManageChild(SL_call[type][row]);
        }

        switch (type)
        {
          case LST_TNC:                       // local stations list
          case LST_TIM:                       // last stations list
          case LST_ALL:                       // stations list
          case LST_OBJ:                       // objects/items
          case LST_MYOBJ:                     // my objects/items
            xastir_snprintf(stemp, sizeof(stemp), "%5d",
                            (int)p_station->num_packets);
            XmTextFieldSetString(SL_packets[type][row],stemp);
            XtManageChild(SL_packets[type][row]);

            if (strlen(p_station->pos_time) > 13)
            {
              xastir_snprintf(stemp, sizeof(stemp), "%c%c/%c%c %c%c:%c%c",
                              //sprintf(stemp,"%c%c/%c%c/%c%c%c%c %c%c:%c%c",
                              p_station->pos_time[0],
                              p_station->pos_time[1],
                              p_station->pos_time[2],
                              p_station->pos_time[3],
                              //p_station->pos_time[4],
                              //p_station->pos_time[5],
                              //p_station->pos_time[6],
                              //p_station->pos_time[7],
                              p_station->pos_time[8],
                              p_station->pos_time[9],
                              p_station->pos_time[10],
                              p_station->pos_time[11]);
            }
            else
            {
              xastir_snprintf(stemp, sizeof(stemp), " ");
            }

            XmTextFieldSetString(SL_pos_time[type][row],stemp);
            XtManageChild(SL_pos_time[type][row]);

            xastir_snprintf(stemp, sizeof(stemp), "%s", p_station->node_path_ptr);
            XmTextFieldSetString(SL_node_path[type][row],stemp);
            XtManageChild(SL_node_path[type][row]);

            xastir_snprintf(stemp, sizeof(stemp), "%s", p_station->power_gain);
            XmTextFieldSetString(SL_power_gain[type][row],stemp);
            XtManageChild(SL_power_gain[type][row]);

// Should we display only the first comment field we have stored, or
// concatenate all of them up to the limit of stemp?
            //xastir_snprintf(stemp, sizeof(stemp), "%s", p_station->comments);
            if ( (p_station->comment_data != NULL)
                 && (p_station->comment_data->text_ptr != NULL) )
            {
              xastir_snprintf(stemp, sizeof(stemp), "%s", p_station->comment_data->text_ptr);
            }
            else
            {
              stemp[0] = '\0';  // Empty string
            }

            XmTextFieldSetString(SL_comments[type][row],stemp);
            XtManageChild(SL_comments[type][row]);

            break;

          case LST_MOB:                       // mobile stations list
            if (strlen(p_station->course)>0)
            {
              XmTextFieldSetString(SL_course[type][row],p_station->course);
            }
            else
            {
              XmTextFieldSetString(SL_course[type][row],"");
            }

            XtManageChild(SL_course[type][row]);
            if (strlen(p_station->speed)>0)
            {
              if (!english_units)
                xastir_snprintf(stemp, sizeof(stemp), "%.1f",
                                atof(p_station->speed)*1.852);
              else
                xastir_snprintf(stemp, sizeof(stemp), "%.1f",
                                atof(p_station->speed)*1.1508);

              XmTextFieldSetString(SL_speed[type][row],stemp);
            }
            else
            {
              XmTextFieldSetString(SL_speed[type][row],"");
            }

            XtManageChild(SL_speed[type][row]);

            if (strlen(p_station->altitude)>0)
            {
              if (!english_units)
              {
                xastir_snprintf(stemp, sizeof(stemp), "%s", p_station->altitude);
              }
              else
              {
                xastir_snprintf(stemp, sizeof(stemp), "%.1f", atof(p_station->altitude)*3.28084);
              }

              XmTextFieldSetString(SL_alt[type][row],stemp);
            }
            else
            {
              XmTextFieldSetString(SL_alt[type][row],"");
            }

            XtManageChild(SL_alt[type][row]);

            if (coordinate_system == USE_UTM
                || coordinate_system == USE_UTM_SPECIAL)
            {
              // Create a UTM string from coordinates
              // in Xastir coordinate system.
              convert_xastir_to_UTM_str(stemp, sizeof(stemp), p_station->coord_lon, p_station->coord_lat);
              XmTextFieldSetString(SL_lat_long[type][row],stemp);
              XtManageChild(SL_lat_long[type][row]);
            }
            else if (coordinate_system == USE_MGRS)
            {
              // Create an MGRS string from
              // coordinates in Xastir coordinate
              // system.
              convert_xastir_to_MGRS_str(stemp,
                                         sizeof(stemp),
                                         p_station->coord_lon,
                                         p_station->coord_lat,
                                         0);
              XmTextFieldSetString(SL_lat_long[type][row],stemp);
              XtManageChild(SL_lat_long[type][row]);
            }
            else
            {
              // Create lat/lon strings from coordinates
              // in Xastir coordinate system.
              if (coordinate_system == USE_DDDDDD)
              {
                convert_lat_l2s(p_station->coord_lat, stemp1, sizeof(stemp1), CONVERT_DEC_DEG);
                convert_lon_l2s(p_station->coord_lon, stemp2, sizeof(stemp2), CONVERT_DEC_DEG);
              }
              else if (coordinate_system == USE_DDMMSS)
              {
                convert_lat_l2s(p_station->coord_lat, stemp1, sizeof(stemp1), CONVERT_DMS_NORMAL);
                convert_lon_l2s(p_station->coord_lon, stemp2, sizeof(stemp2), CONVERT_DMS_NORMAL);
              }
              else        // Assume coordinate_system == USE_DDMMMM
              {
                convert_lat_l2s(p_station->coord_lat, stemp1, sizeof(stemp1), CONVERT_HP_NORMAL);
                convert_lon_l2s(p_station->coord_lon, stemp2, sizeof(stemp2), CONVERT_HP_NORMAL);
              }
              xastir_snprintf(stemp, sizeof(stemp), "%s  %s", stemp1, stemp2);
              XmTextFieldSetString(SL_lat_long[type][row],stemp);
              XtManageChild(SL_lat_long[type][row]);
            }

            xastir_snprintf(stemp, sizeof(stemp), "%d",
                            (int)p_station->num_packets);
            XmTextFieldSetString(SL_packets[type][row],stemp);
            XtManageChild(SL_packets[type][row]);

            if (strlen(p_station->sats_visible)>0)
            {
              xastir_snprintf(stemp, sizeof(stemp), "%d", atoi(p_station->sats_visible));
              XmTextFieldSetString(SL_sats[type][row],stemp);
            }
            else
            {
              XmTextFieldSetString(SL_sats[type][row],"");
            }

            XtManageChild(SL_sats[type][row]);

            l_lat = convert_lat_s2l(my_lat);
            l_lon = convert_lon_s2l(my_long);

            // Get distance in nautical miles
            value = (float)calc_distance_course(l_lat,l_lon,
                                                p_station->coord_lat,p_station->coord_lon,stemp,sizeof(stemp));

            if (english_units)
            {
              xastir_snprintf(stemp1, sizeof(stemp1), "%0.1f", (value * 1.15078));
            }
            else
            {
              xastir_snprintf(stemp1, sizeof(stemp1), "%0.1f", (value * 1.852));
            }

            XmTextFieldSetString(SL_my_course[type][row],stemp);
            XtManageChild(SL_my_course[type][row]);
            XmTextFieldSetString(SL_my_distance[type][row],stemp1);
            XtManageChild(SL_my_distance[type][row]);

            break;

          case LST_WX:                        // weather stations list

            weather = p_station->weather_data;

            if ((int)(((sec_old + weather->wx_sec_time)) < sec_now()))
            {
              break;  // Weather data is too old
            }

            if (strlen(weather->wx_course) > 0)
            {
              XmTextFieldSetString(SL_wx_wind_course[type][row],weather->wx_course);
            }
            else
            {
              XmTextFieldSetString(SL_wx_wind_course[type][row],"");
            }

            XtManageChild(SL_wx_wind_course[type][row]);

            if (strlen(weather->wx_speed) > 0)
            {
              if (english_units)
              {
                xastir_snprintf(stemp, sizeof(stemp), "%d", (int)atoi(weather->wx_speed));
              }
              else
              {
                xastir_snprintf(stemp, sizeof(stemp), "%d", (int)(atof(weather->wx_speed)*1.6094));
              }

              XmTextFieldSetString(SL_wx_wind_speed[type][row],stemp);
            }
            else
            {
              XmTextFieldSetString(SL_wx_wind_speed[type][row],"");
            }

            XtManageChild(SL_wx_wind_speed[type][row]);

            if (strlen(weather->wx_gust) > 0)
            {
              if (english_units)
              {
                xastir_snprintf(stemp, sizeof(stemp), "%d", atoi(weather->wx_gust));
              }
              else
              {
                xastir_snprintf(stemp, sizeof(stemp), "%d", (int)(atof(weather->wx_gust)*1.6094));
              }

              XmTextFieldSetString(SL_wx_wind_gust[type][row],stemp);
            }
            else
            {
              XmTextFieldSetString(SL_wx_wind_gust[type][row],"");
            }

            XtManageChild(SL_wx_wind_gust[type][row]);

            if (strlen(weather->wx_temp) > 0)
            {
              if (english_units)
              {
                xastir_snprintf(stemp, sizeof(stemp), "%d", atoi(weather->wx_temp));
              }
              else
              {
                xastir_snprintf(stemp, sizeof(stemp), "%d", (int)(((atof(weather->wx_temp)-32)*5.0)/9.0));
              }

              XmTextFieldSetString(SL_wx_temp[type][row],stemp);
            }
            else
            {
              XmTextFieldSetString(SL_wx_temp[type][row],"");
            }

            XtManageChild(SL_wx_temp[type][row]);

            if (strlen(weather->wx_hum) > 0)
            {
              XmTextFieldSetString(SL_wx_hum[type][row],weather->wx_hum);
            }
            else
            {
              XmTextFieldSetString(SL_wx_hum[type][row],"");
            }

            XtManageChild(SL_wx_hum[type][row]);

//WE7U
// Change this to inches mercury when English Units is selected
            if (strlen(weather->wx_baro) > 0)
            {
              if (!english_units)      // hPa
              {
                XmTextFieldSetString(SL_wx_baro[type][row],
                                     weather->wx_baro);
              }
              else    // Inches Mercury
              {
                float tempf;
                char temp2[15];

                tempf = atof(weather->wx_baro)*0.02953;
                xastir_snprintf(temp2,
                                sizeof(temp2),
                                "%0.2f",
                                tempf);
                XmTextFieldSetString(SL_wx_baro[type][row],
                                     temp2);
              }
            }
            else
            {
              XmTextFieldSetString(SL_wx_baro[type][row],"");
            }

            XtManageChild(SL_wx_baro[type][row]);

            if (strlen(weather->wx_rain) > 0)
            {
              if (english_units)
              {
                xastir_snprintf(stemp, sizeof(stemp), "%0.2f", atof(weather->wx_rain)/100.0);
              }
              else
              {
                xastir_snprintf(stemp, sizeof(stemp), "%0.2f", atof(weather->wx_rain)*.254);
              }

              XmTextFieldSetString(SL_wx_rain_h[type][row],stemp);
            }
            else
            {
              XmTextFieldSetString(SL_wx_rain_h[type][row],"");
            }

            XtManageChild(SL_wx_rain_h[type][row]);

            if (strlen(weather->wx_prec_00) > 0)
            {
              if (english_units)
              {
                xastir_snprintf(stemp, sizeof(stemp), "%0.2f", atof(weather->wx_prec_00)/100.0);
              }
              else
              {
                xastir_snprintf(stemp, sizeof(stemp), "%0.2f", atof(weather->wx_prec_00)*.254);
              }

              XmTextFieldSetString(SL_wx_rain_00[type][row],stemp);
            }
            else
            {
              XmTextFieldSetString(SL_wx_rain_00[type][row],"");
            }

            XtManageChild(SL_wx_rain_00[type][row]);

            if (strlen(weather->wx_prec_24) > 0)
            {
              if (english_units)
              {
                xastir_snprintf(stemp, sizeof(stemp), "%0.2f", atof(weather->wx_prec_24)/100.0);
              }
              else
              {
                xastir_snprintf(stemp, sizeof(stemp), "%0.2f", atof(weather->wx_prec_24)*.254);
              }

              XmTextFieldSetString(SL_wx_rain_24[type][row],stemp);
            }
            else
            {
              XmTextFieldSetString(SL_wx_rain_24[type][row],"");
            }

            XtManageChild(SL_wx_rain_24[type][row]);
            break;

          default:
            break;
        }
      }
      else                                                // no data, empty row
      {
        XtVaSetValues(SL_da[type][row],XmNlabelPixmap, blank_icon,NULL);
        XtManageChild(SL_da[type][row]);
        symbol(SL_da[type][row],0,'~','$','\0',SL_icon[type][row],0,0,0,' ');
        XtVaSetValues(SL_da[type][row],XmNlabelPixmap, SL_icon[type][row],NULL);
        XtManageChild(SL_da[type][row]);

        xastir_snprintf(temp, sizeof(temp), "%*d", strwid, (row+1+new_offset));
        XmTextFieldSetString(SL_list[type][row],temp);
        XtManageChild(SL_list[type][row]);

        XmTextFieldSetString(SL_call[type][row],"");
        XtManageChild(SL_call[type][row]);

        switch (type)
        {
          case LST_TNC:       // local stations list
          case LST_TIM:
          case LST_ALL:       // stations list
          case LST_OBJ:       // Objects/Items list
          case LST_MYOBJ:     // My objects/Items
            XmTextFieldSetString(SL_packets[type][row],"");
            XtManageChild(SL_packets[type][row]);
            XmTextFieldSetString(SL_pos_time[type][row],"");
            XtManageChild(SL_pos_time[type][row]);
            XmTextFieldSetString(SL_node_path[type][row],"");
            XtManageChild(SL_node_path[type][row]);
            XmTextFieldSetString(SL_power_gain[type][row],"");
            XtManageChild(SL_power_gain[type][row]);
            XmTextFieldSetString(SL_comments[type][row],"");
            XtManageChild(SL_comments[type][row]);
            break;

          case LST_MOB:       // mobile stations list
            XmTextFieldSetString(SL_course[type][row],"");
            XtManageChild(SL_course[type][row]);
            XmTextFieldSetString(SL_speed[type][row],"");
            XtManageChild(SL_speed[type][row]);
            XmTextFieldSetString(SL_alt[type][row],"");
            XtManageChild(SL_alt[type][row]);
            XmTextFieldSetString(SL_lat_long[type][row],"");
            XtManageChild(SL_lat_long[type][row]);
            XmTextFieldSetString(SL_packets[type][row],"");
            XtManageChild(SL_packets[type][row]);
            XmTextFieldSetString(SL_sats[type][row],"");
            XtManageChild(SL_sats[type][row]);
            XmTextFieldSetString(SL_my_course[type][row],"");
            XtManageChild(SL_my_course[type][row]);
            XmTextFieldSetString(SL_my_distance[type][row],"");
            XtManageChild(SL_my_distance[type][row]);
            break;

          case LST_WX:   /*WX stations list */
            XmTextFieldSetString(SL_wx_wind_course[type][row],"");
            XtManageChild(SL_wx_wind_course[type][row]);
            XmTextFieldSetString(SL_wx_wind_speed[type][row],"");
            XtManageChild(SL_wx_wind_speed[type][row]);
            XmTextFieldSetString(SL_wx_wind_gust[type][row],"");
            XtManageChild(SL_wx_wind_gust[type][row]);
            XmTextFieldSetString(SL_wx_temp[type][row],"");
            XtManageChild(SL_wx_temp[type][row]);
            XmTextFieldSetString(SL_wx_hum[type][row],"");
            XtManageChild(SL_wx_hum[type][row]);
            XmTextFieldSetString(SL_wx_baro[type][row],"");
            XtManageChild(SL_wx_baro[type][row]);
            XmTextFieldSetString(SL_wx_rain_h[type][row],"");
            XtManageChild(SL_wx_rain_h[type][row]);
            XmTextFieldSetString(SL_wx_rain_00[type][row],"");
            XtManageChild(SL_wx_rain_00[type][row]);
            XmTextFieldSetString(SL_wx_rain_24[type][row],"");
            XtManageChild(SL_wx_rain_24[type][row]);
            break;

          default:
            break;
        }
      }           // empty line
      if (p_station != NULL)
      {
        get_list_member(type, &p_station, 1, 1);  // get next member in list
      }
    }  // loop over display lines

    end_critical_section(&station_list_dialog_lock, "list_gui.c:Station_List_fill" );

  }  // if list is defined
}





/*
 *  Check if we have to update an active list, do it if necessary
 */
void update_station_scroll_list(void)           // called from UpdateTime() [main.c] in timing loop
{
  int i;
  int pos;
  Dimension last_h, last_w;
  int last;
  int ok;

  last_h = last_w = 0;
  ok = 0;
  for (i=0; i<LST_NUM; i++)                 // update all active lists
  {
    if (station_list_dialog[i] != NULL)
    {
      XtVaGetValues(station_list_dialog[i], XmNheight, &last_h, XmNwidth, &last_w, NULL);
      XtVaGetValues(SL_scroll[i], XmNmaximum,&last,XmNvalue, &pos, NULL);
      if ((redo_list && (sec_now() - last_list_upd > LIST_UPDATE_CYCLE))
          || (last_h!=list_size_h[i]) || (last_w!=list_size_w[i])
          || units_last!=english_units)
      {
        Station_List_fill(i,pos);     // update list contents
        ok = 1;
      }
    }
  }
  if (ok == 1)
  {
    last_list_upd = sec_now();
    redo_list = FALSE;
  }
  units_last = english_units;
}





void dragCallback(Widget UNUSED(w), XtPointer clientData, XtPointer callData)
{
  int i;

  XmScrollBarCallbackStruct *cbs = (XmScrollBarCallbackStruct *)callData;
  i = atoi((char *)clientData);
  // DK7IN:
  // todo: We should only do the update if no other list navigation command is
  //       waiting, otherwise we only should update the offset value.
  //       Same with all other callbacks below...
  Station_List_fill(i,cbs->value);
}





void decrementCallback(Widget UNUSED(w), XtPointer clientData, XtPointer callData)
{
  int i;

  XmScrollBarCallbackStruct *cbs = (XmScrollBarCallbackStruct *)callData;
  i = atoi((char *)clientData);
  Station_List_fill(i,cbs->value);
}





void incrementCallback(Widget UNUSED(w), XtPointer clientData, XtPointer callData)
{
  int i;

  XmScrollBarCallbackStruct *cbs = (XmScrollBarCallbackStruct *)callData;
  i = atoi((char *)clientData);
  Station_List_fill(i,cbs->value);
}





void pageDecrementCallback(Widget UNUSED(w), XtPointer clientData, XtPointer callData)
{
  int i;

  XmScrollBarCallbackStruct *cbs = (XmScrollBarCallbackStruct *)callData;
  i = atoi((char *)clientData);
  Station_List_fill(i,cbs->value);
}





void pageIncrementCallback(Widget UNUSED(w), XtPointer clientData, XtPointer callData)
{
  int i;

  XmScrollBarCallbackStruct *cbs = (XmScrollBarCallbackStruct *)callData;
  i = atoi((char *)clientData);
  Station_List_fill(i,cbs->value);
}





void mouseScrollHandler(Widget UNUSED(w), XtPointer clientData, XButtonEvent* event, Boolean * UNUSED(continueToDispatch))
{
  int i = atoi((char*)clientData);
  int lines = 2;
  // no modifier moves 2 lines
  // shift moves 1 line
  // control moves 10 lines

  if (event->type == ButtonRelease)
  {
    if (event->state & ControlMask)
    {
      lines = 10;
    }
    else if (event->state & ShiftMask)
    {
      lines = 1;
    }

    if (event->button == Button4)           // Scroll up
    {
      if (last_offset[i] > 0)
      {
        if ((last_offset[i] - lines) < 0)
        {
          Station_List_fill(i, 0);
        }
        else
        {
          Station_List_fill(i, last_offset[i] - lines);
        }
      }
    }
    else if (event->button == Button5)      // Scroll down
    {
      Station_List_fill(i, last_offset[i] + lines);
    }
  }
}





void valueChangedCallback(Widget UNUSED(w), XtPointer clientData, XtPointer callData)
{
  int i;

  XmScrollBarCallbackStruct *cbs = (XmScrollBarCallbackStruct *)callData;
  i = atoi((char *)clientData);
  Station_List_fill(i,cbs->value);
}





/*
 *  Setup the various list layouts
 */
void Station_List(Widget UNUSED(w), XtPointer clientData, XtPointer UNUSED(callData) )
{
  int i;
  Widget pane, form, win_list, form2, button_close;
  Widget numl,call,sep,sep2;
  Widget it1, it2, it3, it4, it5, it6, it7, it8, it9;
  Widget seps[40];
  Atom delw;
  int type;
  char temp[400];

  if (station_list_first)
  {
    memset(&SL_scroll, 0, sizeof(SL_scroll));
    init_station_lists();           // init icons at very first list call
    station_list_first=0;
  }
  type=atoi((char *)clientData);
  switch(type)
  {
    case LST_ALL:
      xastir_snprintf(temp,
                      sizeof(temp),
                      "%s",
                      langcode("LHPUPNI000"));        // All Stations
      break;

    case LST_MOB:
      xastir_snprintf(temp,
                      sizeof(temp),
                      "%s",
                      langcode("LHPUPNI001"));        // Mobile Stations
      break;

    case LST_WX:
      xastir_snprintf(temp,
                      sizeof(temp),
                      "%s",
                      langcode("LHPUPNI002"));        // Weather Stations
      break;

    case LST_TNC:
      xastir_snprintf(temp,
                      sizeof(temp),
                      "%s",
                      langcode("LHPUPNI003"));        // Local Stations
      break;

    case LST_TIM:
      xastir_snprintf(temp,
                      sizeof(temp),
                      "%s",
                      langcode("LHPUPNI004"));        // Last Stations
      break;

    case LST_OBJ:
      xastir_snprintf(temp,
                      sizeof(temp),
                      "%s",
                      langcode("LHPUPNI005"));        // Objects/Items
      break;

    case LST_MYOBJ:
      xastir_snprintf(temp,
                      sizeof(temp),
                      "%s",
                      langcode("LHPUPNI006"));        // My Objects/Items
      break;

    default:
      return;
  }

  if (!station_list_dialog[type])     // setup list area if not previously done
  {
    // DK7IN: we should destroy those Widgets to get the
    // memory back, and rebuild it on the next call.   ????
    // I don't exactly know what's going on, but we lose memory
    // every time we call it.

    begin_critical_section(&station_list_dialog_lock, "list_gui.c:Station_List" );

    station_list_dialog[type]= XtVaCreatePopupShell(temp,
                               xmDialogShellWidgetClass, appshell,
                               XmNdeleteResponse,      XmDESTROY,
                               XmNdefaultPosition,     FALSE,
                               XmNminWidth,            274,
                               XmNmaxHeight,           ROWS*HGT+FUDGE,
                               XmNminHeight,           95,
//          XmNheight,             230,
                               XmNfontList, fontlist1,
                               NULL);

    pane = XtVaCreateWidget("Station_List pane",xmPanedWindowWidgetClass, station_list_dialog[type],
                            XmNbackground, colors[0xff],
                            NULL);

    form = XtVaCreateWidget("Station_List form",xmFormWidgetClass, pane,
                            XmNfractionBase,        5,
                            XmNshadowType,          XmSHADOW_OUT,
                            XmNshadowThickness,     1,
                            XmNbackground,          colors[0xff],
                            XmNautoUnmanage,        FALSE,
                            XmNshadowThickness,     1,
                            NULL);

    // station number in list
    numl = XtVaCreateManagedWidget(langcode("LHPUPNI010"), xmTextFieldWidgetClass, form,
                                   XmNeditable,            FALSE,
                                   XmNcursorPositionVisible, FALSE,
                                   XmNtraversalOn,         FALSE,
                                   XmNsensitive,           STIPPLE,
                                   XmNshadowThickness,     0,
                                   XmNcolumns,             5,
                                   XmNtopAttachment,       XmATTACH_FORM,
                                   XmNtopOffset,           2,
                                   XmNbottomAttachment,    XmATTACH_NONE,
                                   XmNleftAttachment,      XmATTACH_FORM,
                                   XmNleftOffset,          3,
                                   XmNrightAttachment,     XmATTACH_NONE,
                                   XmNbackground,          colors[0xff],
                                   XmNfontList, fontlist1,
                                   NULL);
    XmTextFieldSetString(numl,langcode("LHPUPNI010"));              // #

    // icon

    // call
    call = XtVaCreateManagedWidget(langcode("LHPUPNI011"), xmTextFieldWidgetClass, form,
                                   XmNeditable,            FALSE,
                                   XmNcursorPositionVisible, FALSE,
                                   XmNtraversalOn,         FALSE,
                                   XmNsensitive,           STIPPLE,
                                   XmNshadowThickness,     0,
                                   XmNcolumns,             9,      // 12,
                                   XmNtopAttachment,       XmATTACH_FORM,
                                   XmNtopOffset,           2,
                                   XmNbottomAttachment,    XmATTACH_NONE,
                                   XmNleftAttachment,      XmATTACH_WIDGET,
                                   XmNleftWidget,          numl,
                                   XmNleftOffset,          23,     // 22,
                                   XmNrightAttachment,     XmATTACH_NONE,
                                   XmNbackground,          colors[0xff],
                                   XmNfontList, fontlist1,
                                   NULL);
    XmTextFieldSetString(call,langcode("LHPUPNI011"));              // Call Sign

    switch (type)
    {
      case LST_ALL:       // All Stations
      case LST_TNC:       // Local Stations [via TNC]
      case LST_TIM:       // Last Stations
      case LST_OBJ:       // Objects/Item
      case LST_MYOBJ:     // My objects/items

        // number of packets heard
        it1 = XtVaCreateManagedWidget(langcode("LHPUPNI012"), xmTextFieldWidgetClass, form,
                                      XmNeditable,                   FALSE,
                                      XmNcursorPositionVisible,      FALSE,
                                      XmNtraversalOn,         FALSE,
                                      XmNsensitive,                  STIPPLE,
                                      XmNshadowThickness,            0,
                                      XmNcolumns,                    5,
                                      XmNtopAttachment,              XmATTACH_FORM,
                                      XmNtopOffset,                  2,
                                      XmNbottomAttachment,           XmATTACH_NONE,
                                      XmNleftAttachment,             XmATTACH_WIDGET,
                                      XmNleftWidget,                 call,
                                      XmNleftOffset,                 1,
                                      XmNrightAttachment,            XmATTACH_NONE,
                                      XmNbackground,                 colors[0xff],
                                      XmNfontList, fontlist1,
                                      NULL);
        XmTextFieldSetString(it1,langcode("LHPUPNI012"));       // #Pack

        // Last time of position
        it2 = XtVaCreateManagedWidget(langcode("LHPUPNI013"), xmTextFieldWidgetClass, form,
                                      XmNeditable,                   FALSE,
                                      XmNcursorPositionVisible,      FALSE,
                                      XmNtraversalOn,         FALSE,
                                      XmNsensitive,                  STIPPLE,
                                      XmNshadowThickness,            0,
                                      XmNcolumns,                    11,     //16,        //17,
                                      XmNtopAttachment,              XmATTACH_FORM,
                                      XmNtopOffset,                  2,
                                      XmNbottomAttachment,           XmATTACH_NONE,
                                      XmNleftAttachment,             XmATTACH_WIDGET,
                                      XmNleftWidget,                 it1,
                                      XmNleftOffset,                 0,      // 1,
                                      XmNrightAttachment,            XmATTACH_NONE,
                                      XmNbackground,                 colors[0xff],
                                      XmNfontList, fontlist1,
                                      NULL);
        XmTextFieldSetString(it2,langcode("LHPUPNI013"));       // Last Position Time

        // Path
        it3 = XtVaCreateManagedWidget(langcode("LHPUPNI014"), xmTextFieldWidgetClass, form,
                                      XmNeditable,                   FALSE,
                                      XmNcursorPositionVisible,      FALSE,
                                      XmNtraversalOn,         FALSE,
                                      XmNsensitive,                  STIPPLE,
                                      XmNshadowThickness,            0,
                                      XmNcolumns,                    30,
                                      XmNtopAttachment,              XmATTACH_FORM,
                                      XmNtopOffset,                  2,
                                      XmNbottomAttachment,           XmATTACH_NONE,
                                      XmNleftAttachment,             XmATTACH_WIDGET,
                                      XmNleftWidget,                 it2,
                                      XmNleftOffset,                 0,      // 1,
                                      XmNrightAttachment,            XmATTACH_NONE,
                                      XmNbackground,                 colors[0xff],
                                      XmNfontList, fontlist1,
                                      NULL);
        XmTextFieldSetString(it3,langcode("LHPUPNI014"));       // Path

        // PHG
        it4 = XtVaCreateManagedWidget(langcode("LHPUPNI015"), xmTextFieldWidgetClass, form,
                                      XmNeditable,                   FALSE,
                                      XmNcursorPositionVisible,      FALSE,
                                      XmNtraversalOn,         FALSE,
                                      XmNsensitive,                  STIPPLE,
                                      XmNshadowThickness,            0,
                                      XmNcolumns,                    7,
                                      XmNtopAttachment,              XmATTACH_FORM,
                                      XmNtopOffset,                  2,
                                      XmNbottomAttachment,           XmATTACH_NONE,
                                      XmNleftAttachment,             XmATTACH_WIDGET,
                                      XmNleftWidget,                 it3,
                                      XmNleftOffset,                 0,      // 1,
                                      XmNrightAttachment,            XmATTACH_NONE,
                                      XmNbackground,                 colors[0xff],
                                      XmNfontList, fontlist1,
                                      NULL);
        XmTextFieldSetString(it4,langcode("LHPUPNI015"));       // PHG

        // Comments
        it5 = XtVaCreateManagedWidget(langcode("LHPUPNI016"), xmTextFieldWidgetClass, form,
                                      XmNeditable,                   FALSE,
                                      XmNcursorPositionVisible,      FALSE,
                                      XmNtraversalOn,         FALSE,
                                      XmNsensitive,                  STIPPLE,
                                      XmNshadowThickness,            0,
                                      XmNcolumns,                    40,
                                      XmNtopAttachment,              XmATTACH_FORM,
                                      XmNtopOffset,                  2,
                                      XmNbottomAttachment,           XmATTACH_NONE,
                                      XmNleftAttachment,             XmATTACH_WIDGET,
                                      XmNleftWidget,                 it4,
                                      XmNleftOffset,                 0,      // 1,
                                      XmNrightAttachment,            XmATTACH_NONE,
                                      XmNbackground,                 colors[0xff],
                                      XmNfontList, fontlist1,
                                      NULL);
        XmTextFieldSetString(it5,langcode("LHPUPNI016"));       // Comments
        break;

      case LST_MOB:  /*mobile list */
        it1 = XtVaCreateManagedWidget(langcode("LHPUPNI100"), xmTextFieldWidgetClass, form,
                                      XmNeditable,   FALSE,
                                      XmNcursorPositionVisible, FALSE,
                                      XmNtraversalOn,         FALSE,
                                      XmNsensitive, STIPPLE,
                                      XmNshadowThickness, 0,
                                      XmNcolumns, 3,
                                      XmNtopAttachment, XmATTACH_FORM,
                                      XmNtopOffset,2,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, call,
                                      XmNleftOffset, 1,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      XmNfontList, fontlist1,
                                      NULL);

        XmTextFieldSetString(it1,langcode("LHPUPNI100"));       // CSE

        it2 = XtVaCreateManagedWidget(langcode("LHPUPNI101"), xmTextFieldWidgetClass, form,
                                      XmNeditable,   FALSE,
                                      XmNcursorPositionVisible, FALSE,
                                      XmNtraversalOn,         FALSE,
                                      XmNsensitive, STIPPLE,
                                      XmNshadowThickness, 0,
                                      XmNcolumns, 4,
                                      XmNtopAttachment, XmATTACH_FORM,
                                      XmNtopOffset,2,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, it1,
                                      XmNleftOffset, 1,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      XmNfontList, fontlist1,
                                      NULL);

        XmTextFieldSetString(it2,langcode("LHPUPNI101"));       // SPD

        it3 = XtVaCreateManagedWidget(langcode("LHPUPNI102"), xmTextFieldWidgetClass, form,
                                      XmNeditable,   FALSE,
                                      XmNcursorPositionVisible, FALSE,
                                      XmNtraversalOn,         FALSE,
                                      XmNsensitive, STIPPLE,
                                      XmNshadowThickness, 0,
                                      XmNcolumns, 8,
                                      XmNtopAttachment, XmATTACH_FORM,
                                      XmNtopOffset,2,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, it2,
                                      XmNleftOffset, 1,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      XmNfontList, fontlist1,
                                      NULL);

        XmTextFieldSetString(it3,langcode("LHPUPNI102"));       // ALT.

        it4 = XtVaCreateManagedWidget(langcode("LHPUPNI103"), xmTextFieldWidgetClass, form,
                                      XmNeditable,   FALSE,
                                      XmNcursorPositionVisible, FALSE,
                                      XmNtraversalOn,         FALSE,
                                      XmNsensitive, STIPPLE,
                                      XmNshadowThickness, 0,
                                      XmNcolumns, 25,
                                      XmNtopAttachment, XmATTACH_FORM,
                                      XmNtopOffset,2,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, it3,
                                      XmNleftOffset, 1,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      XmNfontList, fontlist1,
                                      NULL);

        XmTextFieldSetString(it4,langcode("LHPUPNI209")); // Lat/Lon or UTM

        it6 = XtVaCreateManagedWidget(langcode("LHPUPNI105"), xmTextFieldWidgetClass, form,
                                      XmNeditable,   FALSE,
                                      XmNcursorPositionVisible, FALSE,
                                      XmNtraversalOn,         FALSE,
                                      XmNsensitive, STIPPLE,
                                      XmNshadowThickness, 0,
                                      XmNcolumns, 5,
                                      XmNtopAttachment, XmATTACH_FORM,
                                      XmNtopOffset,2,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, it4,
                                      XmNleftOffset, 1,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      XmNfontList, fontlist1,
                                      NULL);

        XmTextFieldSetString(it6,langcode("LHPUPNI105"));       // #Pack

        it7 = XtVaCreateManagedWidget(langcode("LHPUPNI106"), xmTextFieldWidgetClass, form,
                                      XmNeditable,   FALSE,
                                      XmNcursorPositionVisible, FALSE,
                                      XmNtraversalOn,         FALSE,
                                      XmNsensitive, STIPPLE,
                                      XmNshadowThickness, 0,
                                      XmNcolumns, 3,
                                      XmNtopAttachment, XmATTACH_FORM,
                                      XmNtopOffset,2,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, it6,
                                      XmNleftOffset, 1,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      XmNfontList, fontlist1,
                                      NULL);

        XmTextFieldSetString(it7,langcode("LHPUPNI106"));       // LSV

        it8 = XtVaCreateManagedWidget(langcode("LHPUPNI107"), xmTextFieldWidgetClass, form,
                                      XmNeditable,   FALSE,
                                      XmNcursorPositionVisible, FALSE,
                                      XmNtraversalOn,         FALSE,
                                      XmNsensitive, STIPPLE,
                                      XmNshadowThickness, 0,
                                      XmNcolumns, 5,
                                      XmNtopAttachment, XmATTACH_FORM,
                                      XmNtopOffset,2,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, it7,
                                      XmNleftOffset, 1,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      XmNfontList, fontlist1,
                                      NULL);

        XmTextFieldSetString(it8,langcode("LHPUPNI107"));       // CFMS

        it9 = XtVaCreateManagedWidget(langcode("LHPUPNI108"), xmTextFieldWidgetClass, form,
                                      XmNeditable,   FALSE,
                                      XmNcursorPositionVisible, FALSE,
                                      XmNtraversalOn,         FALSE,
                                      XmNsensitive, STIPPLE,
                                      XmNshadowThickness, 0,
                                      XmNcolumns, 6,
                                      XmNtopAttachment, XmATTACH_FORM,
                                      XmNtopOffset,2,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, it8,
                                      XmNleftOffset, 1,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      XmNfontList, fontlist1,
                                      NULL);

        XmTextFieldSetString(it9,langcode("LHPUPNI108"));       // DFMS

        break;

      case LST_WX:   /*wx list */
        it1 = XtVaCreateManagedWidget(langcode("LHPUPNI200"), xmTextFieldWidgetClass, form,
                                      XmNeditable,   FALSE,
                                      XmNcursorPositionVisible, FALSE,
                                      XmNtraversalOn,         FALSE,
                                      XmNsensitive, STIPPLE,
                                      XmNshadowThickness, 0,
                                      XmNcolumns, 3,
                                      XmNtopAttachment, XmATTACH_FORM,
                                      XmNtopOffset,2,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, call,
                                      XmNleftOffset, 1,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      XmNfontList, fontlist1,
                                      NULL);

        XmTextFieldSetString(it1,langcode("LHPUPNI200"));       // CSE

        it2 = XtVaCreateManagedWidget(langcode("LHPUPNI201"), xmTextFieldWidgetClass, form,
                                      XmNeditable,   FALSE,
                                      XmNcursorPositionVisible, FALSE,
                                      XmNtraversalOn,         FALSE,
                                      XmNsensitive, STIPPLE,
                                      XmNshadowThickness, 0,
                                      XmNcolumns, 3,
                                      XmNtopAttachment, XmATTACH_FORM,
                                      XmNtopOffset,2,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, it1,
                                      XmNleftOffset, 1,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      XmNfontList, fontlist1,
                                      NULL);

        XmTextFieldSetString(it2,langcode("LHPUPNI201"));       // SPD

        it3 = XtVaCreateManagedWidget(langcode("LHPUPNI202"), xmTextFieldWidgetClass, form,
                                      XmNeditable,   FALSE,
                                      XmNcursorPositionVisible, FALSE,
                                      XmNtraversalOn,         FALSE,
                                      XmNsensitive, STIPPLE,
                                      XmNshadowThickness, 0,
                                      XmNcolumns, 3,
                                      XmNtopAttachment, XmATTACH_FORM,
                                      XmNtopOffset,2,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, it2,
                                      XmNleftOffset, 1,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      XmNfontList, fontlist1,
                                      NULL);

        XmTextFieldSetString(it3,langcode("LHPUPNI202"));       // GST

        it4 = XtVaCreateManagedWidget(langcode("LHPUPNI203"), xmTextFieldWidgetClass, form,
                                      XmNeditable,   FALSE,
                                      XmNcursorPositionVisible, FALSE,
                                      XmNtraversalOn,         FALSE,
                                      XmNsensitive, STIPPLE,
                                      XmNshadowThickness, 0,
                                      XmNcolumns, 4,
                                      XmNtopAttachment, XmATTACH_FORM,
                                      XmNtopOffset,2,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, it3,
                                      XmNleftOffset, 1,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      XmNfontList, fontlist1,
                                      NULL);

        XmTextFieldSetString(it4,langcode("LHPUPNI203"));       // Temp

        it5 = XtVaCreateManagedWidget(langcode("LHPUPNI204"), xmTextFieldWidgetClass, form,
                                      XmNeditable,   FALSE,
                                      XmNcursorPositionVisible, FALSE,
                                      XmNtraversalOn,         FALSE,
                                      XmNsensitive, STIPPLE,
                                      XmNshadowThickness, 0,
                                      XmNcolumns, 3,
                                      XmNtopAttachment, XmATTACH_FORM,
                                      XmNtopOffset,2,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, it4,
                                      XmNleftOffset, 1,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      XmNfontList, fontlist1,
                                      NULL);

        XmTextFieldSetString(it5,langcode("LHPUPNI204"));       // Hum

        it6 = XtVaCreateManagedWidget(langcode("LHPUPNI205"), xmTextFieldWidgetClass, form,
                                      XmNeditable,   FALSE,
                                      XmNcursorPositionVisible, FALSE,
                                      XmNtraversalOn,         FALSE,
                                      XmNsensitive, STIPPLE,
                                      XmNshadowThickness, 0,
                                      XmNcolumns, 6,
                                      XmNtopAttachment, XmATTACH_FORM,
                                      XmNtopOffset,2,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, it5,
                                      XmNleftOffset, 1,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      XmNfontList, fontlist1,
                                      NULL);

        XmTextFieldSetString(it6,langcode("LHPUPNI205"));       // Baro

        it7 = XtVaCreateManagedWidget(langcode("LHPUPNI206"), xmTextFieldWidgetClass, form,
                                      XmNeditable,   FALSE,
                                      XmNcursorPositionVisible, FALSE,
                                      XmNtraversalOn,         FALSE,
                                      XmNsensitive, STIPPLE,
                                      XmNshadowThickness, 0,
                                      XmNcolumns, 5,
                                      XmNtopAttachment, XmATTACH_FORM,
                                      XmNtopOffset,2,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, it6,
                                      XmNleftOffset, 1,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      XmNfontList, fontlist1,
                                      NULL);

        XmTextFieldSetString(it7,langcode("LHPUPNI206"));       // RN-H

        it8 = XtVaCreateManagedWidget(langcode("LHPUPNI207"), xmTextFieldWidgetClass, form,
                                      XmNeditable,   FALSE,
                                      XmNcursorPositionVisible, FALSE,
                                      XmNtraversalOn,         FALSE,
                                      XmNsensitive, STIPPLE,
                                      XmNtraversalOn,         FALSE,
                                      XmNshadowThickness, 0,
                                      XmNcolumns, 5,
                                      XmNtopAttachment, XmATTACH_FORM,
                                      XmNtopOffset,2,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, it7,
                                      XmNleftOffset, 1,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      XmNfontList, fontlist1,
                                      NULL);

        XmTextFieldSetString(it8,langcode("LHPUPNI207"));       // RNSM

        it9 = XtVaCreateManagedWidget(langcode("LHPUPNI208"), xmTextFieldWidgetClass, form,
                                      XmNeditable,   FALSE,
                                      XmNcursorPositionVisible, FALSE,
                                      XmNtraversalOn,         FALSE,
                                      XmNsensitive, STIPPLE,
                                      XmNshadowThickness, 0,
                                      XmNcolumns, 5,
                                      XmNtopAttachment, XmATTACH_FORM,
                                      XmNtopOffset,2,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, it8,
                                      XmNleftOffset, 1,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      XmNfontList, fontlist1,
                                      NULL);

        XmTextFieldSetString(it9,langcode("LHPUPNI208"));       // RN24

        break;

      default:
        break;
    } // if (!station_list_dialog[type])...    from some kilometers above...  ;-)

    sep = XtVaCreateManagedWidget("Station_List sep", xmSeparatorGadgetClass,form,
                                  XmNorientation,             XmHORIZONTAL,
                                  XmNtopAttachment,           XmATTACH_WIDGET,
                                  XmNtopWidget,               numl,
                                  XmNtopOffset,               2,
                                  XmNbottomAttachment,        XmATTACH_NONE,
                                  XmNleftAttachment,          XmATTACH_FORM,
                                  XmNrightAttachment,         XmATTACH_FORM,
                                  XmNfontList, fontlist1,
                                  NULL);

    SL_scroll[type] = XtVaCreateManagedWidget("Station_List SL_scroll", xmScrollBarWidgetClass,form,
                      XmNorientation,             XmVERTICAL,
                      XmNtraversalOn,             TRUE,
                      XmNmaximum,                 10,
//                                    XmNmaximum,                 stations_types(type),
//                                    XmNsliderSize,              rows,        //
//                                    XmNpageIncrement,           rows,      // was 18
                      XmNheight,                  145,        // test
                      XmNsliderSize,              10,        //
                      XmNpageIncrement,           10,      // was 18
                      XmNprocessingDirection,     XmMAX_ON_BOTTOM,
                      XmNtopAttachment,           XmATTACH_WIDGET,
                      XmNtopWidget,               sep,
                      XmNtopOffset,               0,
                      XmNbottomAttachment,        XmATTACH_FORM,
                      XmNbottomOffset,            42,
                      XmNleftAttachment,          XmATTACH_NONE,
                      XmNrightAttachment,         XmATTACH_FORM,
                      XmNbackground,              colors[0xff],
                      XmNfontList, fontlist1,
                      NULL);
    XtAddEventHandler(SL_scroll[type], ButtonReleaseMask, FALSE,
                      (XtEventHandler)mouseScrollHandler, (char*)clientData);

    win_list =  XtVaCreateWidget("Station_List win_list",xmFormWidgetClass, form,
                                 XmNtopAttachment,           XmATTACH_WIDGET,
                                 XmNtopWidget,               sep,
                                 XmNtopOffset,               2,
                                 XmNbottomAttachment,        XmATTACH_FORM,
                                 XmNbottomOffset,            42,
                                 XmNleftAttachment,          XmATTACH_FORM,
                                 XmNleftOffset,              2,
                                 XmNrightAttachment,         XmATTACH_WIDGET,
                                 XmNrightWidget,             SL_scroll[type],
                                 XmNbackground,              colors[0xff],
                                 XmNfontList, fontlist1,
                                 NULL);
    XtAddEventHandler(win_list, ButtonReleaseMask, FALSE,
                      (XtEventHandler)mouseScrollHandler, (char*)clientData);

    for (i=0; i<ROWS; i++)    // setup widgets for maximum number of rows
    {
      if (i != 0)           // not first row
      {

        seps[i] = XtVaCreateManagedWidget("Station_List seps", xmSeparatorGadgetClass, win_list,
                                          XmNorientation,           XmHORIZONTAL,
                                          XmNleftAttachment,        XmATTACH_FORM,
                                          XmNrightAttachment,       XmATTACH_FORM,
                                          XmNtopAttachment,         XmATTACH_WIDGET,
                                          XmNtopWidget,             SL_list[type][i-1],
                                          XmNbackground,            colors[0xff],
                                          XmNfontList, fontlist1,
                                          NULL);
        // line
        SL_list[type][i]= XtVaCreateManagedWidget("Station_List line data", xmTextFieldWidgetClass, win_list,
                          XmNeditable,              FALSE,
                          XmNcursorPositionVisible, FALSE,
                          XmNtraversalOn,           FALSE,
                          XmNsensitive,             STIPPLE,
                          XmNalignment,             XmALIGNMENT_END,
                          XmNshadowThickness,       0,
                          XmNcolumns,               5,
                          XmNleftAttachment,        XmATTACH_FORM,
                          XmNleftOffset,            2,
                          XmNtopAttachment,         XmATTACH_WIDGET,
                          XmNtopWidget,             seps[i],
                          XmNbottomAttachment,      XmATTACH_NONE,
                          XmNrightAttachment,       XmATTACH_NONE,
                          XmNbackground,            colors[0xff],
                          XmNfontList, fontlist1,
                          NULL);

      }
      else                // all except first row
      {
        // station number in list
        SL_list[type][i]= XtVaCreateManagedWidget("Station_List line data", xmTextFieldWidgetClass, win_list,
                          XmNeditable,              FALSE,
                          XmNcursorPositionVisible, FALSE,
                          XmNtraversalOn,           FALSE,
                          XmNsensitive,             STIPPLE,
                          XmNalignment,             XmALIGNMENT_END,
                          XmNshadowThickness,       0,
                          XmNcolumns,               5,
                          XmNleftAttachment,        XmATTACH_FORM,
                          XmNleftOffset,            2,
                          XmNtopAttachment,         XmATTACH_FORM,
                          XmNbottomAttachment,      XmATTACH_NONE,
                          XmNrightAttachment,       XmATTACH_NONE,
                          XmNbackground,            colors[0xff],
                          XmNfontList, fontlist1,
                          NULL);
      }
      XtAddEventHandler(SL_list[type][i], ButtonReleaseMask, FALSE,
                        (XtEventHandler)mouseScrollHandler, (char*)clientData);

      // station symbol graphics
      SL_da[type][i] = XtVaCreateManagedWidget("Station_List icon", xmPushButtonWidgetClass, win_list,
                       XmNtopAttachment,         XmATTACH_OPPOSITE_WIDGET,
                       XmNtopWidget,             SL_list[type][i],
                       XmNtopOffset,             -5,             // Align with top of row, not top of text.
                       XmNbottomAttachment,      XmATTACH_NONE,
                       XmNleftAttachment,        XmATTACH_WIDGET,
                       XmNleftWidget,            SL_list[type][i],
                       XmNrightAttachment,       XmATTACH_NONE,
                       XmNlabelType,             XmPIXMAP,
                       XmNlabelPixmap,           SL_icon[type][i],
                       XmNbackground,            colors[0xff],
                       XmNfontList, fontlist1,
                       NULL);
      //XtAddEventHandler(SL_da[type][i], ButtonReleaseMask, FALSE,
      //                  (XtEventHandler)mouseScrollHandler, (char*)clientData);

      // call sign
      SL_call[type][i]= XtVaCreateManagedWidget("Station_List call data", xmTextFieldWidgetClass, win_list,
                        XmNeditable,              FALSE,
                        XmNcursorPositionVisible, FALSE,
                        XmNtraversalOn,           FALSE,
                        XmNsensitive,             TRUE,
                        XmNshadowThickness,       0,
                        XmNcolumns,               9,      // 12,
                        XmNbackground,            colors[0x0f],
                        XmNleftAttachment,        XmATTACH_WIDGET,
                        XmNleftWidget,            SL_da[type][i],
                        XmNleftOffset,            0,      // 1,
                        XmNtopAttachment,         XmATTACH_OPPOSITE_WIDGET,
                        XmNtopWidget,             SL_list[type][i],
                        XmNbottomAttachment,      XmATTACH_NONE,
                        XmNrightAttachment,       XmATTACH_NONE,
                        XmNfontList, fontlist1,
                        NULL);
      XtAddEventHandler(SL_da[type][i], ButtonReleaseMask, FALSE,
                        (XtEventHandler)mouseScrollHandler, (char*)clientData);

      switch (type)
      {
        case LST_ALL:   // station list
        case LST_TNC:   // local station list
        case LST_TIM:
        case LST_OBJ:   // Objects/Items
        case LST_MYOBJ: // My objects/items
          // number of packets received
          SL_packets[type][i] = XtVaCreateManagedWidget("Station_List packets", xmTextFieldWidgetClass, win_list,
                                XmNeditable,              FALSE,
                                XmNcursorPositionVisible, FALSE,
                                XmNtraversalOn,           FALSE,
                                XmNsensitive,             STIPPLE,
                                XmNshadowThickness,       0,
                                XmNcolumns,               5,
                                XmNbackground,            colors[0x0f],
                                XmNalignment,             XmALIGNMENT_END,
                                XmNleftAttachment,        XmATTACH_WIDGET,
                                XmNleftWidget,            SL_call[type][i],
                                XmNleftOffset,            0,      //1,
                                XmNtopAttachment,         XmATTACH_OPPOSITE_WIDGET,
                                XmNtopWidget,             SL_list[type][i],
                                XmNbottomAttachment,      XmATTACH_NONE,
                                XmNrightAttachment,       XmATTACH_NONE,
                                XmNfontList, fontlist1,
                                NULL);
          XtAddEventHandler(SL_packets[type][i], ButtonReleaseMask, FALSE,
                            (XtEventHandler)mouseScrollHandler, (char*)clientData);

          // Last time of position report
          SL_pos_time[type][i] = XtVaCreateManagedWidget("Station_List pos_time", xmTextFieldWidgetClass, win_list,
                                 XmNeditable,              FALSE,
                                 XmNcursorPositionVisible, FALSE,
                                 XmNtraversalOn,           FALSE,
                                 XmNsensitive,             STIPPLE,
                                 XmNshadowThickness,       0,
                                 XmNcolumns,               11,     //16,     // 17,
                                 XmNbackground,            colors[0x0f],
                                 XmNalignment,             XmALIGNMENT_END,
                                 XmNleftAttachment,        XmATTACH_WIDGET,
                                 XmNleftWidget,            SL_packets[type][i],
                                 XmNleftOffset,            0,      // 1,
                                 XmNtopAttachment,         XmATTACH_OPPOSITE_WIDGET,
                                 XmNtopWidget,             SL_list[type][i],
                                 XmNbottomAttachment,      XmATTACH_NONE,
                                 XmNrightAttachment,       XmATTACH_NONE,
                                 XmNfontList, fontlist1,
                                 NULL);
          XtAddEventHandler(SL_pos_time[type][i], ButtonReleaseMask, FALSE,
                            (XtEventHandler)mouseScrollHandler, (char*)clientData);

          // path
          SL_node_path[type][i] = XtVaCreateManagedWidget("Station_List node_path", xmTextFieldWidgetClass, win_list,
                                  XmNeditable,              FALSE,
                                  XmNcursorPositionVisible, FALSE,
                                  XmNtraversalOn,           FALSE,
                                  XmNsensitive,             STIPPLE,
                                  XmNshadowThickness,       0,
                                  XmNcolumns,               30,
                                  XmNbackground,            colors[0x0f],
                                  XmNalignment,             XmALIGNMENT_END,
                                  XmNleftAttachment,        XmATTACH_WIDGET,
                                  XmNleftWidget,            SL_pos_time[type][i],
                                  XmNleftOffset,            0,      // 1,
                                  XmNtopAttachment,         XmATTACH_OPPOSITE_WIDGET,
                                  XmNtopWidget,             SL_list[type][i],
                                  XmNbottomAttachment,      XmATTACH_NONE,
                                  XmNrightAttachment,       XmATTACH_NONE,
                                  XmNfontList, fontlist1,
                                  NULL);
          XtAddEventHandler(SL_node_path[type][i], ButtonReleaseMask, FALSE,
                            (XtEventHandler)mouseScrollHandler, (char*)clientData);

          // PHG
          SL_power_gain[type][i] = XtVaCreateManagedWidget("Station_List packets", xmTextFieldWidgetClass, win_list,
                                   XmNeditable,                FALSE,
                                   XmNcursorPositionVisible,   FALSE,
                                   XmNtraversalOn,           FALSE,
                                   XmNsensitive,               STIPPLE,
                                   XmNshadowThickness,         0,
                                   XmNcolumns,                 7,
                                   XmNbackground,              colors[0x0f],
                                   XmNalignment,               XmALIGNMENT_END,
                                   XmNleftAttachment,          XmATTACH_WIDGET,
                                   XmNleftWidget,              SL_node_path[type][i],
                                   XmNleftOffset,              0,      // 1,
                                   XmNtopAttachment,           XmATTACH_OPPOSITE_WIDGET,
                                   XmNtopWidget,               SL_list[type][i],
                                   XmNbottomAttachment,        XmATTACH_NONE,
                                   XmNrightAttachment,         XmATTACH_NONE,
                                   XmNfontList, fontlist1,
                                   NULL);
          XtAddEventHandler(SL_power_gain[type][i], ButtonReleaseMask, FALSE,
                            (XtEventHandler)mouseScrollHandler, (char*)clientData);

          // Comment
          SL_comments[type][i] = XtVaCreateManagedWidget("Station_List comments", xmTextFieldWidgetClass, win_list,
                                 XmNeditable,                FALSE,
                                 XmNcursorPositionVisible,   FALSE,
                                 XmNtraversalOn,             FALSE,
                                 XmNsensitive,               STIPPLE,
                                 XmNshadowThickness,         0,
                                 XmNcolumns,                 40,
                                 XmNbackground,              colors[0x0f],
                                 XmNalignment,               XmALIGNMENT_END,
                                 XmNleftAttachment,          XmATTACH_WIDGET,
                                 XmNleftWidget,              SL_power_gain[type][i],
                                 XmNleftOffset,              0,      // 1,
                                 XmNtopAttachment,           XmATTACH_OPPOSITE_WIDGET,
                                 XmNtopWidget,               SL_list[type][i],
                                 XmNbottomAttachment,        XmATTACH_NONE,
                                 XmNrightAttachment,         XmATTACH_NONE,
                                 XmNfontList, fontlist1,
                                 NULL);
          XtAddEventHandler(SL_comments[type][i], ButtonReleaseMask, FALSE,
                            (XtEventHandler)mouseScrollHandler, (char*)clientData);
          break;

        case LST_MOB:   /*mobile list */
          SL_course[type][i] = XtVaCreateManagedWidget("Station_List course", xmTextFieldWidgetClass, win_list,
                               XmNeditable,   FALSE,
                               XmNcursorPositionVisible, FALSE,
                               XmNtraversalOn,             FALSE,
                               XmNsensitive, STIPPLE,
                               XmNshadowThickness, 0,
                               XmNcolumns, 3,
                               XmNbackground, colors[0x0f],
                               XmNalignment, XmALIGNMENT_END,
                               XmNleftAttachment,XmATTACH_WIDGET,
                               XmNleftWidget, SL_call[type][i],
                               XmNleftOffset, 1,
                               XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
                               XmNtopWidget, SL_list[type][i],
                               XmNbottomAttachment,XmATTACH_NONE,
                               XmNrightAttachment,XmATTACH_NONE,
                               XmNfontList, fontlist1,
                               NULL);
          XtAddEventHandler(SL_course[type][i], ButtonReleaseMask, FALSE,
                            (XtEventHandler)mouseScrollHandler, (char*)clientData);

          SL_speed[type][i] = XtVaCreateManagedWidget("Station_List speed", xmTextFieldWidgetClass, win_list,
                              XmNeditable,   FALSE,
                              XmNcursorPositionVisible, FALSE,
                              XmNtraversalOn, FALSE,
                              XmNsensitive, STIPPLE,
                              XmNshadowThickness, 0,
                              XmNcolumns, 4,
                              XmNbackground, colors[0x0f],
                              XmNalignment, XmALIGNMENT_END,
                              XmNleftAttachment,XmATTACH_WIDGET,
                              XmNleftWidget, SL_course[type][i],
                              XmNleftOffset, 1,
                              XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
                              XmNtopWidget, SL_list[type][i],
                              XmNbottomAttachment,XmATTACH_NONE,
                              XmNrightAttachment,XmATTACH_NONE,
                              XmNfontList, fontlist1,
                              NULL);
          XtAddEventHandler(SL_speed[type][i], ButtonReleaseMask, FALSE,
                            (XtEventHandler)mouseScrollHandler, (char*)clientData);

          SL_alt[type][i] = XtVaCreateManagedWidget("Station_List alt", xmTextFieldWidgetClass, win_list,
                            XmNeditable,   FALSE,
                            XmNcursorPositionVisible, FALSE,
                            XmNtraversalOn, FALSE,
                            XmNsensitive, STIPPLE,
                            XmNshadowThickness, 0,
                            XmNcolumns, 8,
                            XmNbackground, colors[0x0f],
                            XmNalignment, XmALIGNMENT_END,
                            XmNleftAttachment,XmATTACH_WIDGET,
                            XmNleftWidget, SL_speed[type][i],
                            XmNleftOffset, 1,
                            XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
                            XmNtopWidget, SL_list[type][i],
                            XmNbottomAttachment,XmATTACH_NONE,
                            XmNrightAttachment,XmATTACH_NONE,
                            XmNfontList, fontlist1,
                            NULL);
          XtAddEventHandler(SL_alt[type][i], ButtonReleaseMask, FALSE,
                            (XtEventHandler)mouseScrollHandler, (char*)clientData);

          SL_lat_long[type][i] = XtVaCreateManagedWidget("Station_List lat/lon", xmTextFieldWidgetClass, win_list,
                                 XmNeditable,   FALSE,
                                 XmNcursorPositionVisible, FALSE,
                                 XmNtraversalOn, FALSE,
                                 XmNsensitive, STIPPLE,
                                 XmNshadowThickness, 0,
                                 XmNcolumns, 25,
                                 XmNbackground, colors[0x0f],
                                 XmNalignment, XmALIGNMENT_END,
                                 XmNleftAttachment,XmATTACH_WIDGET,
                                 XmNleftWidget, SL_alt[type][i],
                                 XmNleftOffset, 1,
                                 XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
                                 XmNtopWidget, SL_list[type][i],
                                 XmNbottomAttachment,XmATTACH_NONE,
                                 XmNrightAttachment,XmATTACH_NONE,
                                 XmNfontList, fontlist1,
                                 NULL);
          XtAddEventHandler(SL_lat_long[type][i], ButtonReleaseMask, FALSE,
                            (XtEventHandler)mouseScrollHandler, (char*)clientData);

          SL_packets[type][i] = XtVaCreateManagedWidget("Station_List packets", xmTextFieldWidgetClass, win_list,
                                XmNeditable,   FALSE,
                                XmNcursorPositionVisible, FALSE,
                                XmNtraversalOn, FALSE,
                                XmNsensitive, STIPPLE,
                                XmNshadowThickness, 0,
                                XmNcolumns, 5,
                                XmNbackground, colors[0x0f],
                                XmNalignment, XmALIGNMENT_END,
                                XmNleftAttachment,XmATTACH_WIDGET,
                                XmNleftWidget, SL_lat_long[type][i],
                                XmNleftOffset, 1,
                                XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
                                XmNtopWidget, SL_list[type][i],
                                XmNbottomAttachment,XmATTACH_NONE,
                                XmNrightAttachment,XmATTACH_NONE,
                                XmNfontList, fontlist1,
                                NULL);
          XtAddEventHandler(SL_packets[type][i], ButtonReleaseMask, FALSE,
                            (XtEventHandler)mouseScrollHandler, (char*)clientData);

          SL_sats[type][i] = XtVaCreateManagedWidget("Station_List sats", xmTextFieldWidgetClass, win_list,
                             XmNeditable,   FALSE,
                             XmNcursorPositionVisible, FALSE,
                             XmNtraversalOn, FALSE,
                             XmNsensitive, STIPPLE,
                             XmNshadowThickness, 0,
                             XmNcolumns, 3,
                             XmNbackground, colors[0x0f],
                             XmNalignment, XmALIGNMENT_END,
                             XmNleftAttachment,XmATTACH_WIDGET,
                             XmNleftWidget, SL_packets[type][i],
                             XmNleftOffset, 1,
                             XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
                             XmNtopWidget, SL_list[type][i],
                             XmNbottomAttachment,XmATTACH_NONE,
                             XmNrightAttachment,XmATTACH_NONE,
                             XmNfontList, fontlist1,
                             NULL);
          XtAddEventHandler(SL_sats[type][i], ButtonReleaseMask, FALSE,
                            (XtEventHandler)mouseScrollHandler, (char*)clientData);

          SL_my_course[type][i] = XtVaCreateManagedWidget("Station_List my course", xmTextFieldWidgetClass, win_list,
                                  XmNeditable,   FALSE,
                                  XmNcursorPositionVisible, FALSE,
                                  XmNtraversalOn, FALSE,
                                  XmNsensitive, STIPPLE,
                                  XmNshadowThickness, 0,
                                  XmNcolumns, 5,
                                  XmNbackground, colors[0x0f],
                                  XmNalignment, XmALIGNMENT_END,
                                  XmNleftAttachment,XmATTACH_WIDGET,
                                  XmNleftWidget, SL_sats[type][i],
                                  XmNleftOffset, 1,
                                  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
                                  XmNtopWidget, SL_list[type][i],
                                  XmNbottomAttachment,XmATTACH_NONE,
                                  XmNrightAttachment,XmATTACH_NONE,
                                  XmNfontList, fontlist1,
                                  NULL);
          XtAddEventHandler(SL_my_course[type][i], ButtonReleaseMask, FALSE,
                            (XtEventHandler)mouseScrollHandler, (char*)clientData);

          SL_my_distance[type][i] = XtVaCreateManagedWidget("Station_List my distance", xmTextFieldWidgetClass, win_list,
                                    XmNeditable,   FALSE,
                                    XmNcursorPositionVisible, FALSE,
                                    XmNtraversalOn, FALSE,
                                    XmNsensitive, STIPPLE,
                                    XmNshadowThickness, 0,
                                    XmNcolumns, 6,
                                    XmNbackground, colors[0x0f],
                                    XmNalignment, XmALIGNMENT_END,
                                    XmNleftAttachment,XmATTACH_WIDGET,
                                    XmNleftWidget, SL_my_course[type][i],
                                    XmNleftOffset, 1,
                                    XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
                                    XmNtopWidget, SL_list[type][i],
                                    XmNbottomAttachment,XmATTACH_NONE,
                                    XmNrightAttachment,XmATTACH_NONE,
                                    XmNfontList, fontlist1,
                                    NULL);
          XtAddEventHandler(SL_my_distance[type][i], ButtonReleaseMask, FALSE,
                            (XtEventHandler)mouseScrollHandler, (char*)clientData);

          break;

        case LST_WX:   /*wx list */
          SL_wx_wind_course[type][i] = XtVaCreateManagedWidget("Station_List wind course", xmTextFieldWidgetClass, win_list,
                                       XmNeditable,   FALSE,
                                       XmNcursorPositionVisible, FALSE,
                                       XmNtraversalOn, FALSE,
                                       XmNsensitive, STIPPLE,
                                       XmNshadowThickness, 0,
                                       XmNcolumns, 3,
                                       XmNbackground, colors[0x0f],
                                       XmNalignment, XmALIGNMENT_END,
                                       XmNleftAttachment,XmATTACH_WIDGET,
                                       XmNleftWidget, SL_call[type][i],
                                       XmNleftOffset, 1,
                                       XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
                                       XmNtopWidget, SL_list[type][i],
                                       XmNbottomAttachment,XmATTACH_NONE,
                                       XmNrightAttachment,XmATTACH_NONE,
                                       XmNfontList, fontlist1,
                                       NULL);
          XtAddEventHandler(SL_wx_wind_course[type][i], ButtonReleaseMask, FALSE,
                            (XtEventHandler)mouseScrollHandler, (char*)clientData);

          SL_wx_wind_speed[type][i] = XtVaCreateManagedWidget("Station_List wind speed", xmTextFieldWidgetClass, win_list,
                                      XmNeditable,   FALSE,
                                      XmNcursorPositionVisible, FALSE,
                                      XmNtraversalOn, FALSE,
                                      XmNsensitive, STIPPLE,
                                      XmNshadowThickness, 0,
                                      XmNcolumns, 3,
                                      XmNbackground, colors[0x0f],
                                      XmNalignment, XmALIGNMENT_END,
                                      XmNleftAttachment,XmATTACH_WIDGET,
                                      XmNleftWidget, SL_wx_wind_course[type][i],
                                      XmNleftOffset, 1,
                                      XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
                                      XmNtopWidget, SL_list[type][i],
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNrightAttachment,XmATTACH_NONE,
                                      XmNfontList, fontlist1,
                                      NULL);
          XtAddEventHandler(SL_wx_wind_speed[type][i], ButtonReleaseMask, FALSE,
                            (XtEventHandler)mouseScrollHandler, (char*)clientData);


          SL_wx_wind_gust[type][i] = XtVaCreateManagedWidget("Station_List wind gust", xmTextFieldWidgetClass, win_list,
                                     XmNeditable,   FALSE,
                                     XmNcursorPositionVisible, FALSE,
                                     XmNtraversalOn, FALSE,
                                     XmNsensitive, STIPPLE,
                                     XmNshadowThickness, 0,
                                     XmNcolumns, 3,
                                     XmNbackground, colors[0x0f],
                                     XmNalignment, XmALIGNMENT_END,
                                     XmNleftAttachment,XmATTACH_WIDGET,
                                     XmNleftWidget, SL_wx_wind_speed[type][i],
                                     XmNleftOffset, 1,
                                     XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
                                     XmNtopWidget, SL_list[type][i],
                                     XmNbottomAttachment,XmATTACH_NONE,
                                     XmNrightAttachment,XmATTACH_NONE,
                                     XmNfontList, fontlist1,
                                     NULL);
          XtAddEventHandler(SL_wx_wind_gust[type][i], ButtonReleaseMask, FALSE,
                            (XtEventHandler)mouseScrollHandler, (char*)clientData);

          SL_wx_temp[type][i] = XtVaCreateManagedWidget("Station_List temp", xmTextFieldWidgetClass, win_list,
                                XmNeditable,   FALSE,
                                XmNcursorPositionVisible, FALSE,
                                XmNtraversalOn, FALSE,
                                XmNsensitive, STIPPLE,
                                XmNshadowThickness, 0,
                                XmNcolumns, 4,
                                XmNbackground, colors[0x0f],
                                XmNalignment, XmALIGNMENT_END,
                                XmNleftAttachment,XmATTACH_WIDGET,
                                XmNleftWidget, SL_wx_wind_gust[type][i],
                                XmNleftOffset, 1,
                                XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
                                XmNtopWidget, SL_list[type][i],
                                XmNbottomAttachment,XmATTACH_NONE,
                                XmNrightAttachment,XmATTACH_NONE,
                                XmNfontList, fontlist1,
                                NULL);
          XtAddEventHandler(SL_wx_temp[type][i], ButtonReleaseMask, FALSE,
                            (XtEventHandler)mouseScrollHandler, (char*)clientData);

          SL_wx_hum[type][i] = XtVaCreateManagedWidget("Station_List humidity", xmTextFieldWidgetClass, win_list,
                               XmNeditable,   FALSE,
                               XmNcursorPositionVisible, FALSE,
                               XmNtraversalOn, FALSE,
                               XmNsensitive, STIPPLE,
                               XmNshadowThickness, 0,
                               XmNcolumns, 3,
                               XmNbackground, colors[0x0f],
                               XmNalignment, XmALIGNMENT_END,
                               XmNleftAttachment,XmATTACH_WIDGET,
                               XmNleftWidget, SL_wx_temp[type][i],
                               XmNleftOffset, 1,
                               XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
                               XmNtopWidget, SL_list[type][i],
                               XmNbottomAttachment,XmATTACH_NONE,
                               XmNrightAttachment,XmATTACH_NONE,
                               XmNfontList, fontlist1,
                               NULL);
          XtAddEventHandler(SL_wx_hum[type][i], ButtonReleaseMask, FALSE,
                            (XtEventHandler)mouseScrollHandler, (char*)clientData);

          SL_wx_baro[type][i] = XtVaCreateManagedWidget("Station_List wx baro", xmTextFieldWidgetClass, win_list,
                                XmNeditable,   FALSE,
                                XmNcursorPositionVisible, FALSE,
                                XmNtraversalOn, FALSE,
                                XmNsensitive, STIPPLE,
                                XmNshadowThickness, 0,
                                XmNcolumns, 6,
                                XmNbackground, colors[0x0f],
                                XmNalignment, XmALIGNMENT_END,
                                XmNleftAttachment,XmATTACH_WIDGET,
                                XmNleftWidget, SL_wx_hum[type][i],
                                XmNleftOffset, 1,
                                XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
                                XmNtopWidget, SL_list[type][i],
                                XmNbottomAttachment,XmATTACH_NONE,
                                XmNrightAttachment,XmATTACH_NONE,
                                XmNfontList, fontlist1,
                                NULL);
          XtAddEventHandler(SL_wx_baro[type][i], ButtonReleaseMask, FALSE,
                            (XtEventHandler)mouseScrollHandler, (char*)clientData);

          SL_wx_rain_h[type][i] = XtVaCreateManagedWidget("Station_List rain hour", xmTextFieldWidgetClass, win_list,
                                  XmNeditable,   FALSE,
                                  XmNcursorPositionVisible, FALSE,
                                  XmNtraversalOn, FALSE,
                                  XmNsensitive, STIPPLE,
                                  XmNshadowThickness, 0,
                                  XmNcolumns, 5,
                                  XmNbackground, colors[0x0f],
                                  XmNalignment, XmALIGNMENT_END,
                                  XmNleftAttachment,XmATTACH_WIDGET,
                                  XmNleftWidget, SL_wx_baro[type][i],
                                  XmNleftOffset, 1,
                                  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
                                  XmNtopWidget, SL_list[type][i],
                                  XmNbottomAttachment,XmATTACH_NONE,
                                  XmNrightAttachment,XmATTACH_NONE,
                                  XmNfontList, fontlist1,
                                  NULL);
          XtAddEventHandler(SL_wx_rain_h[type][i], ButtonReleaseMask, FALSE,
                            (XtEventHandler)mouseScrollHandler, (char*)clientData);

          SL_wx_rain_00[type][i] = XtVaCreateManagedWidget("Station_List rain since mid", xmTextFieldWidgetClass, win_list,
                                   XmNeditable,   FALSE,
                                   XmNcursorPositionVisible, FALSE,
                                   XmNtraversalOn, FALSE,
                                   XmNsensitive, STIPPLE,
                                   XmNshadowThickness, 0,
                                   XmNcolumns, 5,
                                   XmNbackground, colors[0x0f],
                                   XmNalignment, XmALIGNMENT_END,
                                   XmNleftAttachment,XmATTACH_WIDGET,
                                   XmNleftWidget, SL_wx_rain_h[type][i],
                                   XmNleftOffset, 1,
                                   XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
                                   XmNtopWidget, SL_list[type][i],
                                   XmNbottomAttachment,XmATTACH_NONE,
                                   XmNrightAttachment,XmATTACH_NONE,
                                   XmNfontList, fontlist1,
                                   NULL);
          XtAddEventHandler(SL_wx_rain_00[type][i], ButtonReleaseMask, FALSE,
                            (XtEventHandler)mouseScrollHandler, (char*)clientData);

          SL_wx_rain_24[type][i] = XtVaCreateManagedWidget("Station_List rain last 24", xmTextFieldWidgetClass, win_list,
                                   XmNeditable,   FALSE,
                                   XmNcursorPositionVisible, FALSE,
                                   XmNtraversalOn, FALSE,
                                   XmNsensitive, STIPPLE,
                                   XmNshadowThickness, 0,
                                   XmNcolumns, 5,
                                   XmNbackground, colors[0x0f],
                                   XmNalignment, XmALIGNMENT_END,
                                   XmNleftAttachment,XmATTACH_WIDGET,
                                   XmNleftWidget, SL_wx_rain_00[type][i],
                                   XmNleftOffset, 1,
                                   XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
                                   XmNtopWidget, SL_list[type][i],
                                   XmNbottomAttachment,XmATTACH_NONE,
                                   XmNrightAttachment,XmATTACH_NONE,
                                   XmNfontList, fontlist1,
                                   NULL);
          XtAddEventHandler(SL_wx_rain_24[type][i], ButtonReleaseMask, FALSE,
                            (XtEventHandler)mouseScrollHandler, (char*)clientData);

          break;

        default:
          break;
      }
    }  // each row

    form2 = XtVaCreateWidget("Station_List form2",xmFormWidgetClass, form,
                             XmNfractionBase,            5,
                             XmNtopAttachment,           XmATTACH_WIDGET,
                             XmNtopWidget,               SL_scroll[type],
                             XmNbottomAttachment,        XmATTACH_FORM,
                             XmNrightAttachment,         XmATTACH_FORM,
                             XmNleftAttachment,          XmATTACH_FORM,
                             XmNbackground,              colors[0xff],
                             NULL);

    sep2 = XtVaCreateManagedWidget("Station_List sep2", xmSeparatorGadgetClass,form2,
                                   XmNorientation,             XmHORIZONTAL,
                                   XmNtopAttachment,           XmATTACH_FORM,
                                   XmNbottomAttachment,        XmATTACH_NONE,
                                   XmNrightAttachment,         XmATTACH_FORM,
                                   XmNleftAttachment,          XmATTACH_FORM,
                                   XmNbackground,              colors[0xff],
                                   XmNfontList, fontlist1,
                                   NULL);

    button_close = XtVaCreateManagedWidget(langcode("UNIOP00003"),xmPushButtonGadgetClass,form2,
                                           XmNtopAttachment,   XmATTACH_WIDGET,
                                           XmNtopWidget,               sep2,
                                           XmNtopOffset,               4,
                                           XmNbottomAttachment,        XmATTACH_FORM,
                                           XmNbottomOffset,            2,
                                           XmNleftAttachment,          XmATTACH_POSITION,
                                           XmNleftPosition,            2,
                                           XmNrightAttachment,         XmATTACH_POSITION,
                                           XmNrightPosition,           3,
                                           XmNbackground,              colors[0xff],
                                           XmNfontList, fontlist1,
                                           NULL);

    pos_dialog(station_list_dialog[type]);  // calculate position
    delw = XmInternAtom(XtDisplay(station_list_dialog[type]), "WM_DELETE_WINDOW", FALSE);

    /* call backs */
    XtAddCallback(SL_scroll[type], XmNdecrementCallback,     decrementCallback,     (char *)clientData);
    XtAddCallback(SL_scroll[type], XmNdragCallback,          dragCallback,          (char *)clientData);
    XtAddCallback(SL_scroll[type], XmNincrementCallback,     incrementCallback,     (char *)clientData);
    XtAddCallback(SL_scroll[type], XmNpageDecrementCallback, pageDecrementCallback, (char *)clientData);
    XtAddCallback(SL_scroll[type], XmNpageIncrementCallback, pageIncrementCallback, (char *)clientData);
    XtAddCallback(SL_scroll[type], XmNvalueChangedCallback,  valueChangedCallback,  (char *)clientData);

    XtAddCallback(button_close, XmNactivateCallback, station_list_destroy_shell,    (char *)clientData);
    XmAddWMProtocolCallback(station_list_dialog[type], delw, station_list_destroy_shell, (char *)clientData);

    XtManageChild(form);
    XtManageChild(win_list);
    XtManageChild(form2);
    XtManageChild(pane);

    end_critical_section(&station_list_dialog_lock, "list_gui.c:Station_List" );

    XtPopup(station_list_dialog[type], XtGrabNone);

    // Move focus to the Close button.  This appears to highlight the
    // button fine, but we're not able to hit the <Enter> key to
    // have that default function happen.  Note:  We _can_ hit the
    // <SPACE> key, and that activates the option.
//        XmUpdateDisplay(station_list_dialog[type]);
    XmProcessTraversal(button_close, XmTRAVERSE_CURRENT);


// Note:  If adding new lists, make sure to tweak xa_config.c to
// increment the number.  If not, you'll get an X-Windows error at
// this point when trying to resize the window:

    /* set last size if there was one */    // done in list_fill
    if (list_size_w[type] > 0 && list_size_h[type] > 0)
      XtVaSetValues(station_list_dialog[type],
                    XmNwidth,  list_size_w[type],
                    XmNheight, list_size_h[type],
                    NULL);

    if (type != LST_TIM)
    {
      top_call[type][0] = '\0';  // start at top
    }
    else
    {
      top_time =  0;
      top_sn   = -1;
    }
    last_offset[type] = 0;
    last_list_upd = sec_now();
    list_size_i[type] = FALSE;
    redo_list = (int)TRUE;

    Station_List_fill(type,0);      // start with top of list

  }
  else            //  if (!station_list_dialog[type])
    // we already have an initialized widget
  {
    (void)XRaiseWindow(XtDisplay(station_list_dialog[type]), XtWindow(station_list_dialog[type]));
  }
}


