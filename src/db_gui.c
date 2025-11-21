#include <math.h>
#include <ctype.h>

#include "database.h"
#include "db_gis.h"
#include "db_funcs.h"
#include "db_gui.h"
#include "snprintf.h"
#include "xastir.h"
#include "main.h"
#include "draw_symbols.h"
#include "alert.h"
#include "util.h"
#include "tactical_call_utils.h"
#include "mutex_utils.h"
#include "bulletin_gui.h"
#include "fcc_data.h"
#include "geo.h"
#include "gps.h"
#include "rac_data.h"
#include "interface.h"
#include "maps.h"
#include "wx.h"
#include "igate.h"
#include "list_gui.h"
#include "objects.h"
#include "objects_gui.h"
#include "track_gui.h"
#include "xa_config.h"
#include "x_spider.h"
#include "sound.h"
#include "mgrs_utils.h"

// Must be last include file
#include "leak_detection.h"


// ========================================================================
// Global Variable Declarations
// ========================================================================

// External declarations
extern XmFontList fontlist1;    // Menu/System fontlist

// Widget globals
Widget si_text;
Widget db_station_info  = (Widget)NULL;
Widget db_station_popup = (Widget)NULL;
Widget SiS_symb;
Widget station_list;
Widget button_store_track;
Widget change_tactical_dialog = (Widget)NULL;
Widget tactical_text = (Widget)NULL;

// Pixmap globals
Pixmap SiS_icon0, SiS_icon;

// DataRow pointer
DataRow *tactical_pointer = NULL;

// Window decoration and position tracking
int decoration_offset_x = 0;
int decoration_offset_y = 0;
int last_station_info_x = 0;
int last_station_info_y = 0;

// Station Info
char *db_station_info_callsign = NULL;

// Global parameter so that we can pass another value to the below
// function from the Station_info() function.  We need to be able to
// pass this value off to the Station_data() function for special
// operations like moves, where objects are on top of each other.
XtPointer station_info_select_global = NULL;

// Static mutex globals
static xastir_mutex db_station_info_lock;
static xastir_mutex db_station_popup_lock;


// ========================================================================
// INITIALIZATION FUNCTIONS
// ========================================================================

/* Initialize the GUI components for the database */
void db_gui_init(void) {
  init_critical_section( &db_station_info_lock );
  init_critical_section( &db_station_popup_lock );
}


// ========================================================================
// WINDOW DECORATION UTILITIES
// ========================================================================

static void PosTestExpose(Widget parent, XtPointer UNUSED(clientData), XEvent * UNUSED(event), Boolean * UNUSED(continueToDispatch) )
{
  Position x, y;

  XtVaGetValues(parent, XmNx, &x, XmNy, &y, NULL);

  if (debug_level & 1)
  {
    fprintf(stderr,"Window Decoration Offsets:  X:%d\tY:%d\n", x, y);
  }

  // Store the new-found offets in global variables
  decoration_offset_x = (int)x;
  decoration_offset_y = (int)y;

  // Get rid of the event handler and the test dialog
  XtRemoveEventHandler(parent, ExposureMask, True, (XtEventHandler) PosTestExpose, (XtPointer)NULL);
  //    XtRemoveGrab(XtParent(parent));  // Not needed?
  XtDestroyWidget(XtParent(parent));
}



// Here's a stupid trick that we have to do in order to find out how big
// window decorations are.  We need to know this information in order to
// be able to kill/recreate dialogs in the same place each time.  If we
// were to just get and set the X/Y values of the dialog, we would creep
// across the screen by the size of the decorations each time.
// I've seen it.  It's ugly.
//
void compute_decorations( void )
{
  Widget cdtest = (Widget)NULL;
  Widget cdform = (Widget)NULL;
  Cardinal n = 0;
  Arg args[50];


  // We'll create a dummy dialog at 0,0, then query its
  // position.  That'll give us back the position of the
  // widget.  Subtract 0,0 from it (easy huh?) and we get
  // the size of the window decorations.  Store these values
  // in global variables for later use.

  n = 0;
  XtSetArg(args[n], XmNx, 0);
  n++;
  XtSetArg(args[n], XmNy, 0);
  n++;

  cdtest = (Widget) XtVaCreatePopupShell("compute_decorations test",
                                         xmDialogShellWidgetClass,
                                         appshell,
                                         args, n,
                                         NULL);

  n = 0;
  XtSetArg(args[n], XmNwidth, 0);
  n++;    // Make it tiny
  XtSetArg(args[n], XmNheight, 0);
  n++;   // Make it tiny
  cdform = XmCreateForm(cdtest, "compute_decorations test form", args, n);

  XtAddEventHandler(cdform, ExposureMask, True, (XtEventHandler) PosTestExpose,
                    (XtPointer)NULL);

  XtManageChild(cdform);
  XtManageChild(cdtest);
}


// ========================================================================
// STATION TRACKING FUNCTIONS
// ========================================================================

/*
 *  Change map position if necessary while tracking a station
 *      we call it with defined station call and position
 */
void track_station(Widget w, char * UNUSED(call_tracked), DataRow *p_station)
{
  long x_ofs, y_ofs;
  long new_lat, new_lon;

  if ( is_tracked_station(p_station->call_sign) )     // We want to track this station
  {
    new_lat = p_station->coord_lat;                 // center map to station position as default
    new_lon = p_station->coord_lon;
    x_ofs = new_lon - center_longitude;            // current offset from screen center
    y_ofs = new_lat - center_latitude;
    if ((labs(x_ofs) > (screen_width*scale_x/3)) || (labs(y_ofs) > (screen_height*scale_y/3)))
    {
      // only redraw map if near border (margin 1/6 of screen at each side)
      if (labs(y_ofs) < (screen_height*scale_y/2))
      {
        new_lat  += y_ofs/2;  // give more space in driving direction
      }
      if (labs(x_ofs) < (screen_width*scale_x/2))
      {
        new_lon += x_ofs/2;
      }

      set_map_position(w, new_lat, new_lon);      // center map to new position

    }
    search_tracked_station(&p_station);
  }
}


/*
 * Track from Station_data
 *
 * Called by Station_data function below from the Track Station
 * button in Station Info.
 */
void Track_from_Station_data(Widget UNUSED(w), XtPointer clientData, XtPointer UNUSED(calldata) )
{
  DataRow *p_station = clientData;

  if (p_station->call_sign[0] != '\0')
  {
    xastir_snprintf(tracking_station_call,
                    sizeof(tracking_station_call),
                    "%s",
                    p_station->call_sign);
    track_station_on = 1;
  }
  else
  {
    tracking_station_call[0] = '\0';
  }
}


// ========================================================================
// DRAWING AND RENDERING FUNCTIONS
// ========================================================================

/*
 *  Draw trail on screen.  If solid=1, draw type LineSolid, else
 *  draw type LineOnOffDash.
 *
 *  If label_all_trackpoints=1, add the callsign next to each
 *  trackpoint.  We may modify this and just add the callsign at the
 *  start/end of each new track segment.
 *
 */
void draw_trail(Widget w, DataRow *fill, int solid)
{
  char short_dashed[2]  = {(char)1,(char)5};
  char medium_dashed[2] = {(char)5,(char)5};
  unsigned long lat0, lon0, lat1, lon1;        // trail segment points
  int col_trail, col_dot;
  XColor rgb;
  long brightness;
  char flag1;
  TrackRow *ptr;


  if (!ok_to_draw_station(fill))
  {
    return;
  }

  // Expire old trackpoints first.  We use the
  // remove-station-from-display time as the expire time for
  // trackpoints.  This can be set from the Configure->Defaults
  // dialog.
  expire_trail_points(fill, sec_clear);

  ptr = fill->newest_trackpoint;

  // Trail should have at least two points
  if ( (ptr != NULL) && (ptr->prev != NULL) )
  {
    int skip_dupes = 0; // Don't skip points first time through

    if (debug_level & 256)
    {
      fprintf(stderr,"draw_trail called for %s with %s.\n",
              fill->call_sign, (solid? "Solid" : "Non-Solid"));
    }

    col_trail = trail_colors[fill->trail_color];

    // define color of position dots in trail
    rgb.pixel = col_trail;
    XQueryColor(XtDisplay(w),cmap,&rgb);

    brightness = (long)(0.3*rgb.red + 0.55*rgb.green + 0.15*rgb.blue);
    if (brightness > 32000l)
    {
      col_dot = trail_colors[0x05];   // black dot on light trails
    }
    else
    {
      col_dot = trail_colors[0x06];   // white dot on dark trail
    }

    if (solid)
      // Used to be "JoinMiter" and "CapButt" below
    {
      (void)XSetLineAttributes(XtDisplay(w), gc, 3, LineSolid, CapRound, JoinRound);
    }
    else
    {
      // Another choice is LineDoubleDash
      (void)XSetLineAttributes(XtDisplay(w), gc, 3, LineOnOffDash, CapRound, JoinRound);
      (void)XSetDashes(XtDisplay(w), gc, 0, short_dashed, 2);
    }

    // Traverse linked list of trail points from newest to
    // oldest
    while ( (ptr != NULL) && (ptr->prev != NULL) )
    {
      lon0 = ptr->trail_long_pos;         // Trail segment start
      lat0 = ptr->trail_lat_pos;
      lon1 = ptr->prev->trail_long_pos;   // Trail segment end
      lat1 = ptr->prev->trail_lat_pos;
      flag1 = ptr->flag; // Are we at the start of a new trail?

      if ((flag1 & TR_NEWTRK) == '\0')
      {
        int lon0_screen, lat0_screen, lon1_screen, lat1_screen;

        // draw trail segment
        //
        (void)XSetForeground(XtDisplay(w),gc,col_trail);
        draw_vector(da,
                    lon0,
                    lat0,
                    lon1,
                    lat1,
                    gc,
                    pixmap_final,
                    skip_dupes);

        // draw position point itself
        //
        (void)XSetForeground(XtDisplay(w),gc,col_dot);
        draw_point(w,
                   lon0,
                   lat0,
                   gc,
                   pixmap_final,
                   skip_dupes);

        // Draw the callsign to go with the point if
        // label_all_trackpoints=1
        //
        if (Display_.callsign && Display_.label_all_trackpoints)
        {

          // Convert to screen coordinates
          lon0_screen = (lon0 - NW_corner_longitude) / scale_x;
          lat0_screen = (lat0 - NW_corner_latitude) / scale_y;

          // Convert to screen coordinates.
          lon1_screen = (lon1 - NW_corner_longitude) / scale_x;
          lat1_screen = (lat1 - NW_corner_latitude)  / scale_y;

          // The last position already gets its callsign
          // string drawn, plus that gets shifted based on
          // other parameters.  Draw both points of all
          // line segments except that one.  This will
          // result in strings getting drawn twice at
          // times, but they overlay on top of each other
          // so no big deal.
          //
          if (ptr != fill->newest_trackpoint)
          {

            draw_nice_string(da,
                             pixmap_final,
                             letter_style,
                             lon0_screen+10,
                             lat0_screen,
                             fill->call_sign,
                             0x08,
                             0x0f,
                             strlen(fill->call_sign));

            // If not same screen position as last drawn
            if (lon0_screen != lon1_screen
                && lat0_screen != lat1_screen)
            {

              draw_nice_string(da,
                               pixmap_final,
                               letter_style,
                               lon1_screen+10,
                               lat1_screen,
                               fill->call_sign,
                               0x08,
                               0x0f,
                               strlen(fill->call_sign));
            }
          }
        }
      }
      ptr = ptr->prev;
      skip_dupes = 1;
    }
    (void)XSetDashes(XtDisplay(w), gc, 0, medium_dashed, 2);
  }
  else if (debug_level & 256)
  {
    fprintf(stderr,"Trail for %s does not contain 2 or more points.\n",
            fill->call_sign);
  }
}


// display_station
//
// single is 1 if the calling station wants to update only a
// single station.  If updating multiple stations in a row, then
// "single" will be passed to us as a zero.
//
// If current course/speed/altitude are absent, we check the last
// track point to try to snag those numbers.
//
void display_station(Widget w, DataRow *p_station, int single)
{
  char temp_altitude[20];
  char temp_course[20];
  char temp_speed[20];
  char dr_speed[20];
  char temp_call[MAX_TACTICAL_CALL+1];
  char wx_tm[50];
  char temp_wx_temp[30];
  char temp_wx_wind[40];
  char temp_my_distance[20];
  char temp_my_course[20];
  char temp1_my_course[20];
  char temp2_my_gauge_data[50];
  time_t temp_sec_heard;
  int temp_show_last_heard;
  long l_lon, l_lat;
  char orient;
  float value;
  char tmp[7+1];
  int speed_ok = 0;
  int course_ok = 0;
  int wx_ghost = 0;
  Pixmap drawing_target;
  WeatherRow *weather = p_station->weather_data;
  time_t secs_now = sec_now();
  int ambiguity_flag;
  long ambiguity_coord_lon, ambiguity_coord_lat;
  size_t temp_len;


  if (debug_level & 128)
  {
    fprintf(stderr,"Display station (%s) called for Single=%d.\n", p_station->call_sign, single);
  }

  if (!ok_to_draw_station(p_station))
  {
    return;
  }

  // Set up call string for display
  if (Display_.callsign)
  {
    if (p_station->tactical_call_sign
        && p_station->tactical_call_sign[0] != '\0')
    {
      // Display tactical callsign instead if it has one
      // defined.
      xastir_snprintf(temp_call,
                      sizeof(temp_call),
                      "%s",
                      p_station->tactical_call_sign);
    }
    else
    {
      // Display normal callsign.
      xastir_snprintf(temp_call,
                      sizeof(temp_call),
                      "%s",
                      p_station->call_sign);
    }
  }
  else
  {
    temp_call[0] = '\0';
  }

  // Set up altitude string for display
  temp_altitude[0] = '\0';

  if (Display_.altitude)
  {
    // Check whether we have altitude in the current data
    if (strlen(p_station->altitude)>0)
    {
      // Found it in the current data
      xastir_snprintf(temp_altitude, sizeof(temp_altitude), "%.0f%s",
                      atof(p_station->altitude) * cvt_m2len, un_alt);
    }

    // Else check whether the previous position had altitude.
    // Note that newest_trackpoint if it exists should be the
    // same as the current data, so we have to go back one
    // further trackpoint.
    else if ( (p_station->newest_trackpoint != NULL)
              && (p_station->newest_trackpoint->prev != NULL) )
    {
      if ( p_station->newest_trackpoint->prev->altitude > -99999l)
      {
        // Found it in the tracklog
        xastir_snprintf(temp_altitude, sizeof(temp_altitude), "%.0f%s",
                        (float)(p_station->newest_trackpoint->prev->altitude * cvt_dm2len),
                        un_alt);

//                fprintf(stderr,"Trail data              with altitude: %s : %s\n",
//                    p_station->call_sign,
//                    temp_altitude);
      }
      else
      {
        //fprintf(stderr,"Trail data w/o altitude                %s\n",
        //    p_station->call_sign);
      }
    }
  }

  // Set up speed and course strings for display
  temp_speed[0] = '\0';
  dr_speed[0] = '\0';
  temp_course[0] = '\0';

  if (Display_.speed || Display_.dr_data)
  {
    // don't display 'fixed' stations speed and course.
    // Check whether we have speed in the current data and it's
    // >= 0.
    if ( (strlen(p_station->speed)>0) && (atof(p_station->speed) >= 0) )
    {
      speed_ok++;
      xastir_snprintf(tmp,
                      sizeof(tmp),
                      "%s",
                      un_spd);
      if (Display_.speed_short)
      {
        tmp[0] = '\0';  // without unit
      }

      xastir_snprintf(temp_speed, sizeof(temp_speed), "%.0f%s",
                      atof(p_station->speed)*cvt_kn2len,tmp);
    }
    // Else check whether the previous position had speed
    // Note that newest_trackpoint if it exists should be the
    // same as the current data, so we have to go back one
    // further trackpoint.
    else if ( (p_station->newest_trackpoint != NULL)
              && (p_station->newest_trackpoint->prev != NULL) )
    {

      xastir_snprintf(tmp,
                      sizeof(tmp),
                      "%s",
                      un_spd);

      if (Display_.speed_short)
      {
        tmp[0] = '\0';  // without unit
      }

      if ( p_station->newest_trackpoint->prev->speed > 0)
      {
        speed_ok++;

        xastir_snprintf(temp_speed, sizeof(temp_speed), "%.0f%s",
                        p_station->newest_trackpoint->prev->speed * cvt_hm2len,
                        tmp);
      }
    }
  }

  if (Display_.course || Display_.dr_data)
  {
    // Check whether we have course in the current data
    if ( (strlen(p_station->course)>0) && (atof(p_station->course) > 0) )
    {
      course_ok++;
      xastir_snprintf(temp_course, sizeof(temp_course), "%.0f\xB0",
                      atof(p_station->course));
    }
    // Else check whether the previous position had a course
    // Note that newest_trackpoint if it exists should be the
    // same as the current data, so we have to go back one
    // further trackpoint.
    else if ( (p_station->newest_trackpoint != NULL)
              && (p_station->newest_trackpoint->prev != NULL) )
    {
      if( p_station->newest_trackpoint->prev->course > 0 )
      {
        course_ok++;
        xastir_snprintf(temp_course, sizeof(temp_course), "%.0f\xB0",
                        (float)p_station->newest_trackpoint->prev->course);
      }
    }
  }

  // Save the speed into the dr string for later
  xastir_snprintf(dr_speed,
                  sizeof(dr_speed),
                  "%s",
                  temp_speed);

  if (!speed_ok  || !Display_.speed)
  {
    temp_speed[0] = '\0';
  }

  if (!course_ok || !Display_.course)
  {
    temp_course[0] = '\0';
  }

  // Set up distance and bearing strings for display
  temp_my_distance[0] = '\0';
  temp_my_course[0] = '\0';

  if (Display_.dist_bearing && strcmp(p_station->call_sign,my_callsign) != 0)
  {
    l_lat = convert_lat_s2l(my_lat);
    l_lon = convert_lon_s2l(my_long);

    // Get distance in nautical miles, convert to current measurement standard
    value = cvt_kn2len * calc_distance_course(l_lat,l_lon,
            p_station->coord_lat,p_station->coord_lon,temp1_my_course,sizeof(temp1_my_course));

    if (value < 5.0)
    {
      sprintf(temp_my_distance,"%0.1f%s",value,un_dst);
    }
    else
    {
      sprintf(temp_my_distance,"%0.0f%s",value,un_dst);
    }

    xastir_snprintf(temp_my_course, sizeof(temp_my_course), "%.0f\xB0",
                    atof(temp1_my_course));
  }

  // Set up weather strings for display
  temp_wx_temp[0] = '\0';
  temp_wx_wind[0] = '\0';

  if (weather != NULL)
  {
    // wx_ghost = 1 if the weather data is too old to display
    wx_ghost = (int)(((sec_old + weather->wx_sec_time)) < secs_now);
  }

  if (Display_.weather
      && Display_.weather_text
      && weather != NULL      // We have weather data
      && !wx_ghost)           // Weather is current, display it
  {

    if (strlen(weather->wx_temp) > 0)
    {
      xastir_snprintf(tmp,
                      sizeof(tmp),
                      "T:");
      if (Display_.temperature_only)
      {
        tmp[0] = '\0';
      }

      if (english_units)
        xastir_snprintf(temp_wx_temp, sizeof(temp_wx_temp), "%s%.0f\xB0%s",
                        tmp, atof(weather->wx_temp),"F ");
      else
        xastir_snprintf(temp_wx_temp, sizeof(temp_wx_temp), "%s%.0f\xB0%s",
                        tmp,((atof(weather->wx_temp)-32.0)*5.0)/9.0,"C ");
    }

    if (!Display_.temperature_only)
    {
      if (strlen(weather->wx_hum) > 0)
      {
        xastir_snprintf(wx_tm, sizeof(wx_tm), "H:%.0f%%", atof(weather->wx_hum));
        strncat(temp_wx_temp,
                wx_tm,
                sizeof(temp_wx_temp) - 1 - strlen(temp_wx_temp));
      }

      if (strlen(weather->wx_speed) > 0)
      {
        xastir_snprintf(temp_wx_wind, sizeof(temp_wx_wind), "S:%.0f%s ",
                        atof(weather->wx_speed)*cvt_mi2len,un_spd);
      }

      if (strlen(weather->wx_gust) > 0)
      {
        xastir_snprintf(wx_tm, sizeof(wx_tm), "G:%.0f%s ",
                        atof(weather->wx_gust)*cvt_mi2len,un_spd);
        strncat(temp_wx_wind,
                wx_tm,
                sizeof(temp_wx_wind) - 1 - strlen(temp_wx_wind));
      }

      if (strlen(weather->wx_course) > 0)
      {
        xastir_snprintf(wx_tm, sizeof(wx_tm), "C:%.0f\xB0", atof(weather->wx_course));
        strncat(temp_wx_wind,
                wx_tm,
                sizeof(temp_wx_wind) - 1 - strlen(temp_wx_wind));
      }

      temp_len = strlen(temp_wx_wind);
      if ((temp_len > 0) && (temp_wx_wind[temp_len-1] == ' '))
      {
        temp_wx_wind[temp_len-1] = '\0';  // delete blank at EOL
      }
    }

    temp_len = strlen(temp_wx_temp);
    if ((temp_len > 0) && (temp_wx_temp[strlen(temp_wx_temp)-1] == ' '))
    {
      temp_wx_temp[temp_len-1] = '\0';  // delete blank at EOL
    }
  }


  (void)remove_trailing_asterisk(p_station->call_sign);  // DK7IN: is this needed here?

  if (Display_.symbol_rotate)
  {
    orient = symbol_orient(p_station->course);  // rotate symbol
  }
  else
  {
    orient = ' ';
  }

  // Prevents my own call from "ghosting"?
  //    temp_sec_heard = (strcmp(p_station->call_sign, my_callsign) == 0) ? secs_now: p_station->sec_heard;
  temp_sec_heard = (is_my_station(p_station)) ? secs_now : p_station->sec_heard;

  // Check whether it's a locally-owned object/item
//    if ( (is_my_call(p_station->origin,1))          // If station is owned by me (including SSID)
//            && ( (p_station->flag & ST_OBJECT)      // And it's an object
//              || (p_station->flag & ST_ITEM) ) ) {  // or an item
//    if ( is_my_object_item(p_station) ) {
//        temp_sec_heard = secs_now; // We don't want our own objects/items to "ghost"
//    }

  // Show last heard times only for others stations and their
  // objects/items.
  //    temp_show_last_heard = (strcmp(p_station->call_sign, my_callsign) == 0) ? 0 : Display_.last_heard;
  temp_show_last_heard = (is_my_station(p_station)) ? 0 : Display_.last_heard;



  //------------------------------------------------------------------------------------------

  // If we're only planning on updating a single station at this time, we go
  // through the drawing calls twice, the first time drawing directly onto
  // the screen.
  if (!pending_ID_message && single)
  {
    drawing_target = XtWindow(da);
  }
  else
  {
    drawing_target = pixmap_final;
  }

  //_do_the_drawing:

  // Check whether it's a locally-owned object/item
//    if ( (is_my_call(p_station->origin,1))                  // If station is owned by me (including SSID)
//            && ( (p_station->flag & ST_OBJECT)       // And it's an object
//              || (p_station->flag & ST_ITEM  ) ) ) { // or an item
//    if ( is_my_object_item(p_station) ) {
//        temp_sec_heard = secs_now; // We don't want our own objects/items to "ghost"
  // This isn't quite right since if it's a moving object, passing an incorrect
  // sec_heard should give the wrong results.
//    }

  ambiguity_flag = 0; // Default

  if (Display_.ambiguity && p_station->pos_amb)
  {
    ambiguity_flag = 1;
    draw_ambiguity(p_station->coord_lon,
                   p_station->coord_lat,
                   p_station->pos_amb,
                   &ambiguity_coord_lon, // New longitude may get passed back to us
                   &ambiguity_coord_lat, // New latitude may get passed back to us
                   temp_sec_heard,
                   drawing_target);
  }

  // Check for DF'ing data, draw DF circles if present and enabled
  if (Display_.df_data && strlen(p_station->signal_gain) == 7)    // There's an SHGD defined
  {
    //fprintf(stderr,"SHGD:%s\n",p_station->signal_gain);
    draw_DF_circle( (ambiguity_flag) ? ambiguity_coord_lon : p_station->coord_lon,
                    (ambiguity_flag) ? ambiguity_coord_lat : p_station->coord_lat,
                    p_station->signal_gain,
                    temp_sec_heard,
                    drawing_target);
  }

  // Check for DF'ing beam heading/NRQ data
  if (Display_.df_data && (strlen(p_station->bearing) == 3) && (strlen(p_station->NRQ) == 3))
  {
    //fprintf(stderr,"Bearing: %s\n",p_station->signal_gain,NRQ);
    if (p_station->df_color == -1)
    {
      p_station->df_color = rand() % MAX_TRAIL_COLORS;
    }

    draw_bearing( (ambiguity_flag) ? ambiguity_coord_lon : p_station->coord_lon,
                  (ambiguity_flag) ? ambiguity_coord_lat : p_station->coord_lat,
                  p_station->course,
                  p_station->bearing,
                  p_station->NRQ,
                  trail_colors[p_station->df_color],
                  Display_.df_beamwidth_data, Display_.df_bearing_data,
                  temp_sec_heard,
                  drawing_target);
  }

  // Check whether to draw dead-reckoning data by KJ5O
  if (Display_.dr_data
      && ( (p_station->flag & ST_MOVING)
           //        && (p_station->newest_trackpoint!=0
           && course_ok
           && speed_ok
           && scale_y < 8000
           && atof(dr_speed) > 0) )
  {

    // Does it make sense to try to do dead-reckoning on an
    // object that has position ambiguity enabled?  I don't
    // think so!
    //
    if ( ! ambiguity_flag && ( (secs_now-temp_sec_heard) < dead_reckoning_timeout) )
    {

      draw_deadreckoning_features(p_station,
                                  drawing_target,
                                  w);
    }
  }

  if (p_station->aprs_symbol.area_object.type != AREA_NONE)
  {
    draw_area( (ambiguity_flag) ? ambiguity_coord_lon : p_station->coord_lon,
               (ambiguity_flag) ? ambiguity_coord_lat : p_station->coord_lat,
               p_station->aprs_symbol.area_object.type,
               p_station->aprs_symbol.area_object.color,
               p_station->aprs_symbol.area_object.sqrt_lat_off,
               p_station->aprs_symbol.area_object.sqrt_lon_off,
               p_station->aprs_symbol.area_object.corridor_width,
               temp_sec_heard,
               drawing_target);
  }


  // Draw additional stuff if this is the tracked station
  if (is_tracked_station(p_station->call_sign))
  {
    //WE7U
    draw_pod_circle( (ambiguity_flag) ? ambiguity_coord_lon : p_station->coord_lon,
                     (ambiguity_flag) ? ambiguity_coord_lat : p_station->coord_lat,
                     0.0020 * scale_y,
                     colors[0x0e],   // Yellow
                     drawing_target,
                     temp_sec_heard);
    draw_pod_circle( (ambiguity_flag) ? ambiguity_coord_lon : p_station->coord_lon,
                     (ambiguity_flag) ? ambiguity_coord_lat : p_station->coord_lat,
                     0.0023 * scale_y,
                     colors[0x44],   // Red
                     drawing_target,
                     temp_sec_heard);
    draw_pod_circle( (ambiguity_flag) ? ambiguity_coord_lon : p_station->coord_lon,
                     (ambiguity_flag) ? ambiguity_coord_lat : p_station->coord_lat,
                     0.0026 * scale_y,
                     colors[0x61],   // Blue
                     drawing_target,
                     temp_sec_heard);
  }


  // Draw additional stuff if this is a storm and the weather data
  // is not too old to display.
  if ( (weather != NULL) && weather->wx_storm && !wx_ghost )
  {
    char temp[4];


    //fprintf(stderr,"Plotting a storm symbol:%s:%s:%s:\n",
    //    weather->wx_hurricane_radius,
    //    weather->wx_trop_storm_radius,
    //    weather->wx_whole_gale_radius);

    // Still need to draw the circles in different colors for the
    // different ranges.  Might be nice to tint it as well.

    xastir_snprintf(temp,
                    sizeof(temp),
                    "%s",
                    weather->wx_hurricane_radius);

    if ( (temp[0] != '\0') && (strncmp(temp,"000",3) != 0) )
    {

      draw_pod_circle( (ambiguity_flag) ? ambiguity_coord_lon : p_station->coord_lon,
                       (ambiguity_flag) ? ambiguity_coord_lat : p_station->coord_lat,
                       atof(temp) * 1.15078, // nautical miles to miles
                       colors[0x44],   // Red
                       drawing_target,
                       temp_sec_heard);
    }

    xastir_snprintf(temp,
                    sizeof(temp),
                    "%s",
                    weather->wx_trop_storm_radius);

    if ( (temp[0] != '\0') && (strncmp(temp,"000",3) != 0) )
    {
      draw_pod_circle( (ambiguity_flag) ? ambiguity_coord_lon : p_station->coord_lon,
                       (ambiguity_flag) ? ambiguity_coord_lat : p_station->coord_lat,
                       atof(temp) * 1.15078, // nautical miles to miles
                       colors[0x0e],   // Yellow
                       drawing_target,
                       temp_sec_heard);
    }

    xastir_snprintf(temp,
                    sizeof(temp),
                    "%s",
                    weather->wx_whole_gale_radius);

    if ( (temp[0] != '\0') && (strncmp(temp,"000",3) != 0) )
    {
      draw_pod_circle( (ambiguity_flag) ? ambiguity_coord_lon : p_station->coord_lon,
                       (ambiguity_flag) ? ambiguity_coord_lat : p_station->coord_lat,
                       atof(temp) * 1.15078, // nautical miles to miles
                       colors[0x0a],   // Green
                       drawing_target,
                       temp_sec_heard);
    }
  }


  // Draw wind barb if selected and we have wind, but not a severe
  // storm (wind barbs just confuse the matter).
  if (Display_.weather && Display_.wind_barb
      && weather != NULL && atoi(weather->wx_speed) >= 5
      && !weather->wx_storm
      && !wx_ghost )
  {
    draw_wind_barb( (ambiguity_flag) ? ambiguity_coord_lon : p_station->coord_lon,
                    (ambiguity_flag) ? ambiguity_coord_lat : p_station->coord_lat,
                    weather->wx_speed,
                    weather->wx_course,
                    temp_sec_heard,
                    drawing_target);
  }


  // WE7U
  //
  // Draw truncation/rounding rectangles plus error ellipses.
  //
  //
  // We need to keep track of ellipse northing/easting radii plus
  // rectangle northing/easting offsets.  If both sets are present
  // we'll need to draw the summation of both geometric figures.
  // Check that the math works at/near the poles.  We may need to keep
  // track of truncation/rounding rectangles separately if some
  // devices or software use one method, some the other.
  //
  if (!ambiguity_flag)
  {

    // Check whether we're at a close enough zoom level to have
    // the ellipses/rectangles be visible, else skip drawing for
    // efficiency.
    //
    //fprintf(stderr,"scale_y: %ld\t", scale_y);
    if (scale_y < 17)   // 60' figures are good out to about zoom 16
    {

      // Here we may have to check what type of device is being used (if
      // possible to determine) to decide whether to draw a truncation/
      // rounding rectangles or GPS error ellipses.  Truncation rectangles
      // have the symbol at one corner, rounding have it in the middle.
      // Based on the precision inherent in the packet we wish to draw a
      // GPS error ellipse instead, the decision point is when the packet
      // precision is adequate to show ~6 meters.
      //
      // OpenTracker APRS:  Truncation, rectangle
      // OpenTracker Base91:Truncation, ellipse
      // OpenTracker OpenTrac: Truncation, ellipse
      // TinyTrak APRS:     Truncation, rectangle
      // TinyTrak NMEA:     Truncation, ellipse/rectangle based on precision
      // TinyTrak Mic-E:    Truncation, rectangle
      // GPGGA:             Truncation, ellipse/rectangle based on precision/HDOP/Augmentation
      // GPRMC:             Truncation, ellipse/rectangle based on precision
      // GPGLL:             Truncation, ellipse/rectangle based on precision
      // Xastir APRS:       Truncation, rectangle
      // Xastir Base91:     Truncation, ellipse
      // UI-View APRS:      ??, rectangle
      // UI-View Base91:    ??, ellipse
      // APRS+SA APRS:      ??, rectangle
      // APRS+SA Base91:    ??, ellipse
      // PocketAPRS:        ??, rectangle
      // SmartAPRS:         ??, rectangle
      // HamHUD:            Truncation, ??
      // HamHUD GPRMC:      Truncation, ellipse/rectangle based on precision
      // Linksys NSLU2:     ??, rectangle
      // AGW Tracker:       ??, ??
      // APRSPoint:         ??, rectangle
      // APRSce:            ??, rectangle
      // APRSdos APRS:      ??, rectangle
      // APRSdos Base91:    ??, ellipse
      // BalloonTrack:      ??, ??
      // DMapper:           ??, ??
      // JavAPRS APRS:      ??, rectangle
      // JavAPRS Base91:    ??, ellipse
      // WinAPRS APRS:      ??, rectangle
      // WinAPRS Base91:    ??, ellipse
      // MacAPRS APRS:      ??, rectangle
      // MacAPRS Base91:    ??, ellipse
      // MacAPRSOSX APRS:   ??, rectangle
      // MacAPRSOSX Base91: ??, ellipse
      // X-APRS APRS:       ??, rectangle
      // X-APRS Base91:     ??, ellipse
      // OziAPRS:           ??, rectangle
      // NetAPRS:           ??, rectangle
      // APRS SCS:          ??, ??
      // RadioMobile:       ??, rectangle
      // KPC-3:             ??, rectangle
      // MicroTNC:          ??, rectangle
      // TigerTrak:         ??, rectangle
      // PicoPacket:        ??, rectangle
      // MIM:               ??, rectangle
      // Mic-Encoder:       ??, rectangle
      // Pic-Encoder:       ??, rectangle
      // Generic Mic-E:     ??, rectangle
      // D7A/D7E:           ??, rectangle
      // D700A:             ??, rectangle
      // Alinco DR-135:     ??, rectangle
      // Alinco DR-620:     ??, rectangle
      // Alinco DR-635:     ??, rectangle
      // Other:             ??, ??


      // Initial try at drawing the error_ellipse_radius
      // circles around the posit.  error_ellipse_radius is in
      // centimeters.  Convert from cm to miles for
      // draw_pod_circle().
      //
      /*
                  draw_pod_circle( p_station->coord_lon,
                                   p_station->coord_lat,
                                   p_station->error_ellipse_radius / 100000.0 * 0.62137, // cm to mi
                                   colors[0x0f],  // White
                                   drawing_target,
                                   temp_sec_heard);
      */
      draw_precision_rectangle( p_station->coord_lon,
                                p_station->coord_lat,
                                p_station->error_ellipse_radius, // centimeters (not implemented yet)
                                p_station->lat_precision, // 100ths of seconds latitude
                                p_station->lon_precision, // 100ths of seconds longitude
                                colors[0x0f],  // White
                                drawing_target);


      // Perhaps draw vectors from the symbol out to the borders of these
      // odd figures?  Draw an outline without vectors to the symbol?
      // Have the color match the track color assigned to that station so
      // the geometric figures can be kept separate from nearby stations?
      //
      // draw_truncation_rectangle + error_ellipse (symbol at corner)
      // draw_rounding_rectangle + error_ellipse (symbol in middle)

    }
  }

  // Zero out the variable in case we don't use it below.
  temp2_my_gauge_data[0] = '\0';

  // If an H2O object, create a timestamp + last comment variable
  // (which should contain gage-height and/or water-flow numbers)
  // for use in the draw_symbol() function below.
  if (p_station->aprs_symbol.aprs_type == '/'
      && p_station->aprs_symbol.aprs_symbol == 'w'
      && (   p_station->flag & ST_OBJECT    // And it's an object
             || p_station->flag & ST_ITEM) )   // or an item
  {

    // NOTE:  Also check whether it was sent by the Firenet GAGE
    // script??  "GAGE-*"

    // NOTE:  Check most recent comment time against
    // p_station->sec_heard.  If they don't match, don't display the
    // comment.  This will make sure that older comment data doesn't get
    // displayed which can be quite misleading for stream gauges.

    // Check whether we have any comment data at all.  If so,
    // the first one will be the most recent comment and the one
    // we wish to display.
    if (p_station->comment_data != NULL)
    {
      CommentRow *ptr;
//            time_t sec;
//            struct tm *time;


      ptr = p_station->comment_data;

      // Check most recent comment's sec_heard time against
      // the station record's sec_heard time.  If they don't
      // match, don't display the comment.  This will make
      // sure that older comment data doesn't get displayed
      // which can be quite misleading for stream gauges.
      if (p_station->sec_heard == ptr->sec_heard)
      {

        // Note that text_ptr can be an empty string.
        // That's ok.

        // Also print the sec_heard timestamp so we know
        // when this particular gauge data was received
        // (Very important!).
//                sec = ptr->sec_heard;
//                time = localtime(&sec);

        xastir_snprintf(temp2_my_gauge_data,
                        sizeof(temp2_my_gauge_data),
                        "%s",
//                    "%02d/%02d %02d:%02d %s",
//                    time->tm_mon + 1,
//                    time->tm_mday,
//                    time->tm_hour,
//                    time->tm_min,
                        ptr->text_ptr);
//fprintf(stderr, "%s\n", temp2_my_gauge_data);
      }
    }
  }

  draw_symbol(w,
              p_station->aprs_symbol.aprs_type,
              p_station->aprs_symbol.aprs_symbol,
              p_station->aprs_symbol.special_overlay,
              (ambiguity_flag) ? ambiguity_coord_lon : p_station->coord_lon,
              (ambiguity_flag) ? ambiguity_coord_lat : p_station->coord_lat,
              temp_call,
              temp_altitude,
              temp_course,    // ??
              temp_speed,     // ??
              temp_my_distance,
              temp_my_course,
              // Display only if wx temp is current
              (wx_ghost) ? "" : temp_wx_temp,
              // Display only if wind speed is current
              (wx_ghost) ? "" : temp_wx_wind,
              temp_sec_heard,
              temp_show_last_heard,
              drawing_target,
              orient,
              p_station->aprs_symbol.area_object.type,
              p_station->signpost,
              temp2_my_gauge_data,
              1); // Increment "currently_selected_stations"

  // If it's a Waypoint symbol, draw a line from it to the
  // transmitting station.
  if (p_station->aprs_symbol.aprs_type == '\\'
      && p_station->aprs_symbol.aprs_symbol == '/')
  {

    draw_WP_line(p_station,
                 ambiguity_flag,
                 ambiguity_coord_lon,
                 ambiguity_coord_lat,
                 drawing_target,
                 w);
  }

  // Draw other points associated with the station, if any.
  // KG4NBB
  if (debug_level & 128)
  {
    fprintf(stderr,"  Number of multipoints = %d\n",p_station->num_multipoints);
  }
  if (p_station->num_multipoints != 0)
  {
    draw_multipoints( (ambiguity_flag) ? ambiguity_coord_lon : p_station->coord_lon,
                      (ambiguity_flag) ? ambiguity_coord_lat : p_station->coord_lat,
                      p_station->num_multipoints,
                      p_station->multipoint_data->multipoints,
                      p_station->type, p_station->style,
                      temp_sec_heard,
                      drawing_target);
  }

  temp_sec_heard = p_station->sec_heard;    // DK7IN: ???

  if (Display_.phg
      && (!(p_station->flag & ST_MOVING) || Display_.phg_of_moving))
  {

    // Check for Map View "eyeball" symbol
    if ( strncmp(p_station->power_gain,"RNG",3) == 0
         && p_station->aprs_symbol.aprs_type == '/'
         && p_station->aprs_symbol.aprs_symbol == 'E' )
    {
      // Map View "eyeball" symbol.  Don't draw the RNG ring
      // for it.
    }
    else if (strlen(p_station->power_gain) == 7)
    {
      // Station has PHG or RNG defined
      //
      draw_phg_rng( (ambiguity_flag) ? ambiguity_coord_lon : p_station->coord_lon,
                    (ambiguity_flag) ? ambiguity_coord_lat : p_station->coord_lat,
                    p_station->power_gain,
                    temp_sec_heard,
                    drawing_target);
    }
    else if (Display_.default_phg && !(p_station->flag & (ST_OBJECT | ST_ITEM)))
    {
      // No PHG defined and not an object/item.  Display a PHG
      // of 3130 as default as specified in the spec:  9W, 3dB
      // omni at 20 feet = 6.2 mile PHG radius.
      //
      draw_phg_rng( (ambiguity_flag) ? ambiguity_coord_lon : p_station->coord_lon,
                    (ambiguity_flag) ? ambiguity_coord_lat : p_station->coord_lat,
                    "PHG3130",
                    temp_sec_heard,
                    drawing_target);
    }
  }


  // Draw minimum proximity circle?
  if (p_station->probability_min[0] != '\0')
  {
    double range = atof(p_station->probability_min);

    // Draw red circle
    draw_pod_circle(p_station->coord_lon,
                    p_station->coord_lat,
                    range,
                    colors[0x44],
                    drawing_target,
                    temp_sec_heard);
  }

  // Draw maximum proximity circle?
  if (p_station->probability_max[0] != '\0')
  {
    double range = atof(p_station->probability_max);

    // Draw red circle
    draw_pod_circle(p_station->coord_lon,
                    p_station->coord_lat,
                    range,
                    colors[0x44],
                    drawing_target,
                    temp_sec_heard);
  }

  // DEBUG STUFF
  //            draw_pod_circle(x_long, y_lat, 1.5, colors[0x44], where);
  //            draw_pod_circle(x_long, y_lat, 3.0, colors[0x44], where);


  // Now if we just did the single drawing, we want to go back and draw
  // the same things onto pixmap_final so that when we do update from it
  // to the screen all of the stuff will be there.
//    if (drawing_target == XtWindow(da)) {
//        drawing_target = pixmap_final;
//        goto _do_the_drawing;
//    }
}


// draw line relative
void draw_test_line(Widget w, long x, long y, long dx, long dy, long ofs)
{

  x += screen_width  - 10 - ofs;
  y += screen_height - 10;
  (void)XDrawLine(XtDisplay(w),
                  pixmap_final,
                  gc,
                  l16(x),
                  l16(y),
                  l16(x+dx),
                  l16(y+dy));
}

// draw text
void draw_ruler_text(Widget w, char * text, long ofs)
{
  int x,y;
  int len;

  len = (int)strlen(text);
  x = screen_width  - 10 - ofs / 2;
  y = screen_height - 10;
  x -= len * 3;
  y -= 3;
  if (draw_labeled_grid_border==TRUE)
  {
    // move text up a few pixels to leave space for labeled border
    y = y - 15;
    x = x - 10;
  }
  draw_nice_string(w,pixmap_final,letter_style,x,y,text,0x10,0x20,len);
}




// Compute Range Scale in miles or kilometers.
//
// For this we need to figure out x-distance and y-distance across
// the screen.  Take the smaller of the two, then figure out which
// power of 2 miles fits from the center to the edge of the screen.
// "For metric, use the nearest whole number kilometer in powers of
// two of 1.5 km above the 1 mile scale.  At 1 mile and below, do
// the conversion to meters where 1 mi is equal to 1600m..." (Bob
// Bruninga's words).
void draw_range_scale(Widget w)
{
  Dimension width, height;
  long x, x0, y, y0;
  double x_miles_km, y_miles_km, distance;
  char temp_course[10];
  long temp;
  double temp2;
  long range;
  int small_flag = 0;
  int x_screen, y_screen;
  int len;
  char text[80];
  int border_offset = 0;  // number of pixels to offset the scale if a labeled map border is drawn


  // Find out the screen values
  XtVaGetValues(da,XmNwidth, &width, XmNheight, &height, NULL);

  // Convert points to Xastir coordinate system

  // X
  x = center_longitude  - ((width *scale_x)/2);
  x0 = center_longitude; // Center of screen

  // Y
  y = center_latitude   - ((height*scale_y)/2);
  y0 = center_latitude;  // Center of screen

  // Compute distance from center to each edge

  // X distance.  Keep Y constant.
  x_miles_km = cvt_kn2len * calc_distance_course(y0,x0,y0,x,temp_course,sizeof(temp_course));

  // Y distance.  Keep X constant.
  y_miles_km = cvt_kn2len * calc_distance_course(y0,x0,y,x0,temp_course,sizeof(temp_course));

  // Choose the smaller distance
  if (x_miles_km < y_miles_km)
  {
    distance = x_miles_km;
  }
  else
  {
    distance = y_miles_km;
  }

  // Convert it to nearest power of two that fits inside

  if (english_units)   // English units
  {
    if (distance >= 1.0)
    {
      // Shift it right until it is less than 2.
      temp = (long)distance;
      range = 1;
      while (temp >= 2)
      {
        temp = temp / 2;
        range = range * 2;
      }
    }
    else    // Distance is less than one
    {
      // divide 1.0 by 2 until distance is greater
      small_flag++;
      temp2 = 1.0;
      range = 1;
      while (temp2 > distance)
      {
        //fprintf(stderr,"temp2: %f,  distance: %f\n", temp2, distance);
        temp2 = temp2 / 2.0;
        range = range * 2;
      }
    }
  }
  else    // Metric units
  {
    if (distance >= 12800.0)
    {
      range = 12800;
    }
    else if (distance >= 6400.0)
    {
      range = 6400;
    }
    else if (distance >= 3200.0)
    {
      range = 3200;
    }
    else if (distance >= 1600.0)
    {
      range = 1600;
    }
    else if (distance >= 800.0)
    {
      range = 800;
    }
    else if (distance >= 400.0)
    {
      range = 400;
    }
    else if (distance >= 200.0)
    {
      range = 200;
    }
    else if (distance >= 100.0)
    {
      range = 100;
    }
    else if (distance >= 50.0)
    {
      range = 50;
    }
    else if (distance >= 25.0)
    {
      range = 25;
    }
    else if (distance >= 12.0)
    {
      range = 12;
    }
    else if (distance >= 6.0)
    {
      range = 6;
    }
    else if (distance >= 3.0)
    {
      range = 3;
    }
    else
    {
      small_flag++;
      if (distance >= 1.6)
      {
        range = 1600;
      }
      else if (distance >= 0.8)
      {
        range = 800;
      }
      else if (distance >= 0.4)
      {
        range = 400;
      }
      else if (distance >= 0.2)
      {
        range = 200;
      }
      else if (distance >= 0.1)
      {
        range = 100;
      }
      else if (distance >= 0.05)
      {
        range = 50;
      }
      else if (distance >= 0.025)
      {
        range = 25;
      }
      else
      {
        range = 12;
      }
    }
  }

  //fprintf(stderr,"Distance: %f\t", distance);
  //fprintf(stderr,"Range: %ld\n", range);

  if (english_units)   // English units
  {
    if (small_flag)
    {
      xastir_snprintf(text,
                      sizeof(text),
                      "%s 1/%ld mi",
                      langcode("RANGE001"),   // "RANGE SCALE"
                      range);
    }
    else
    {
      xastir_snprintf(text,
                      sizeof(text),
                      "%s %ld mi",
                      langcode("RANGE001"),   // "RANGE SCALE"
                      range);
    }
  }
  else    // Metric units
  {
    if (small_flag)
    {
      xastir_snprintf(text,
                      sizeof(text),
                      "%s %ld m",
                      langcode("RANGE001"),   // "RANGE SCALE"
                      range);
    }
    else
    {
      xastir_snprintf(text,
                      sizeof(text),
                      "%s %ld km",
                      langcode("RANGE001"),   // "RANGE SCALE"
                      range);
    }
  }

  // Draw it on the screen
  len = (int)strlen(text);
  x_screen = 10;
  y_screen = screen_height - 5;
  if ((draw_labeled_grid_border==TRUE) && long_lat_grid)
  {
    border_offset = get_rotated_label_text_length_pixels(w, "0", FONT_BORDER) + 3;
    // don't draw range scale right on top of labeled border, move into map
    draw_nice_string(w,pixmap_final,letter_style,x_screen+border_offset,y_screen-border_offset-3,text,0x10,0x20,len);
  }
  else
  {
    // draw range scale in lower left corder of map
    draw_nice_string(w,pixmap_final,letter_style,x_screen,y_screen,text,0x10,0x20,len);
  }

}





/*
 *  Calculate and draw ruler on right bottom of screen
 */
void draw_ruler(Widget w)
{
  int ruler_pix;      // min size of ruler in pixel
  char unit[5+1];     // units
  char text[20];      // ruler text
  double ruler_siz;   // len of ruler in meters etc.
  int mag;
  int i;
  int dx, dy;
  int border_offset = 0;  // number of pixels to offset the scale if a labeled map border is drawn

  ruler_pix = (int)(screen_width / 9);        // ruler size (in pixels)
  ruler_siz = ruler_pix * scale_x * calc_dscale_x(center_longitude,center_latitude); // size in meter

  if(english_units)
  {
    if (ruler_siz > 1609.3/2)
    {
      xastir_snprintf(unit,
                      sizeof(unit),
                      "mi");
      ruler_siz /= 1609.3;
    }
    else
    {
      xastir_snprintf(unit,
                      sizeof(unit),
                      "ft");
      ruler_siz /= 0.3048;
    }
  }
  else
  {
    xastir_snprintf(unit,
                    sizeof(unit),
                    "m");
    if (ruler_siz > 1000/2)
    {
      xastir_snprintf(unit,
                      sizeof(unit),
                      "km");
      ruler_siz /= 1000.0;
    }
  }

  mag = 1;
  while (ruler_siz > 5.0)               // get magnitude
  {
    ruler_siz /= 10.0;
    mag *= 10;
  }
  // select best value and adjust ruler length
  if (ruler_siz > 2.0)
  {
    ruler_pix = (int)(ruler_pix * 5.0 / ruler_siz +0.5);
    ruler_siz = 5.0 * mag;
  }
  else
  {
    if (ruler_siz > 1.0)
    {
      ruler_pix = (int)(ruler_pix * 2.0 / ruler_siz +0.5);
      ruler_siz = 2.0 * mag;
    }
    else
    {
      ruler_pix = (int)(ruler_pix * 1.0 / ruler_siz +0.5);
      ruler_siz = 1.0 * mag;
    }
  }
  xastir_snprintf(text, sizeof(text), "%.0f %s",ruler_siz,unit);      // Set up string
  //fprintf(stderr,"Ruler: %s, %d\n",text,ruler_pix);

  (void)XSetLineAttributes(XtDisplay(w),gc,1,LineSolid,CapRound,JoinRound);
  (void)XSetForeground(XtDisplay(w),gc,colors[0x20]);         // white
  for (i = 8; i >= 0; i--)
  {
    dx = (((i / 3)+1) % 3)-1;         // looks complicated...
    dy = (((i % 3)+1) % 3)-1;         // I want 0 / 0 as last entry
    if ((draw_labeled_grid_border==TRUE) && long_lat_grid)
    {
      // move ruler up a few pixels to leave space for labeled border
      border_offset = get_rotated_label_text_length_pixels(w, "0", FONT_BORDER) + 3;
      dy = dy - border_offset - 3;
      dx = dx - border_offset - 3;
    }

    // If text on black background style selected, draw a black
    // rectangle in that corner of the map first so that the
    // scale lines show up well.
    //
    // If first time through and text-on-black style
    if ( (i == 8) && (letter_style == 2) )
    {
      XSetForeground(XtDisplay(w),gc,colors[0x10]);   // black
      (void)XSetLineAttributes(XtDisplay(w),gc,20,LineSolid,CapProjecting,JoinMiter);
      draw_test_line(w, dx, dy+5, ruler_pix, 0, ruler_pix);

      // Reset to needed parameters for drawing the scale
      (void)XSetLineAttributes(XtDisplay(w),gc,1,LineSolid,CapRound,JoinRound);
      (void)XSetForeground(XtDisplay(w),gc,colors[0x20]);         // white
    }

    if (i == 0)
    {
      (void)XSetForeground(XtDisplay(w),gc,colors[0x10]);  // black
    }

    draw_test_line(w,dx,dy,          ruler_pix,0,ruler_pix);        // hor line
    draw_test_line(w,dx,dy,              0,5,    ruler_pix);        // ver left
    draw_test_line(w,dx+ruler_pix,dy,    0,5,    ruler_pix);        // ver right
    if (text[0] == '2')
    {
      draw_test_line(w,dx+0.5*ruler_pix,dy,0,3,ruler_pix);  // ver middle
    }

    if (text[0] == '5')
    {
      draw_test_line(w,dx+0.2*ruler_pix,dy,0,3,ruler_pix);        // ver middle
      draw_test_line(w,dx+0.4*ruler_pix,dy,0,3,ruler_pix);        // ver middle
      draw_test_line(w,dx+0.6*ruler_pix,dy,0,3,ruler_pix);        // ver middle
      draw_test_line(w,dx+0.8*ruler_pix,dy,0,3,ruler_pix);        // ver middle
    }
  }

  draw_ruler_text(w,text,ruler_pix);

  draw_range_scale(w);
}


/*
 *  Display all stations on screen (trail, symbol, info text)
 */
void display_file(Widget w)
{
  DataRow *p_station;         // pointer to station data
  time_t temp_sec_heard;      // time last heard
  time_t t_clr, t_old, now;

  if(debug_level & 1)
  {
    fprintf(stderr,"Display File Start\n");
  }

  // Keep track of how many station we are currently displaying on
  // the screen.  We'll display this number and the total number
  // of objects in the database as displayed/total on the status
  // line.  Each time we call display_station() we'll bump this
  // number.
  currently_selected_stations = 0;

  // Draw probability of detection circle, if enabled
  //draw_pod_circle(64000000l, 32400000l, 10, colors[0x44], pixmap_final);

  now = sec_now();
  t_old = now - sec_old;        // precalc compare times
  t_clr = now - sec_clear;
  temp_sec_heard = 0l;
  p_station = t_oldest;                // start with oldest station, have newest on top at t_newest

  while (p_station != NULL)
  {

    if (debug_level & 64)
    {
      fprintf(stderr,"display_file: Examining %s\n", p_station->call_sign);
    }

    // Skip deleted stations
    if ( !(p_station->flag & ST_ACTIVE) )
    {

      if (debug_level & 64)
      {
        fprintf(stderr,"display_file: ignored deleted %s\n", p_station->call_sign);
      }

      // Skip to the next station in the list
      p_station = p_station->t_newer;  // next station
      continue;
    }

    // Check for my objects/items
//        if ( (is_my_call(p_station->origin, 1)        // If station is owned by me (including SSID)
//                && (   p_station->flag & ST_OBJECT    // And it's an object
//                    || p_station->flag & ST_ITEM) ) ) { // or an item
//
    // This case is covered by the is_my_station() call, so we
    // don't need it here.
//        if (is_my_object_item(p_station) ) {
//            temp_sec_heard = now;
//        }
//        else {
    // Callsign match here includes checking SSID
//            temp_sec_heard = (is_my_call(p_station->call_sign,1))?  now: p_station->sec_heard;
    temp_sec_heard = (is_my_station(p_station)) ? now : p_station->sec_heard;
//        }

    // Skip far away station
    if ((p_station->flag & ST_INVIEW) == 0)
    {
      // we make better use of the In View flag in the future

      if (debug_level & 256)
      {
        fprintf(stderr,"display_file: Station outside viewport\n");
      }

      // Skip to the next station in the list
      p_station = p_station->t_newer;  // next station
      continue;
    }

    // Skip if we're running an altnet and this station's not in
    // it
    if ( altnet && !is_altnet(p_station) )
    {

      if (debug_level & 64)
      {
        fprintf(stderr,"display_file: Station %s skipped altnet\n",
                p_station->call_sign);
      }

      // Skip to the next station in the list
      p_station = p_station->t_newer;  // next station
      continue;
    }

    if (debug_level & 256)
    {
      fprintf(stderr,"display_file:  Inview, check for trail\n");
    }

    // Display trail if we should
    if (Display_.trail && p_station->newest_trackpoint != NULL)
    {
      // ????????????   what is the difference? :

      if (debug_level & 256)
      {
        fprintf(stderr,"%s:    Trails on and have track data\n",
                "display_file");
      }

      if (temp_sec_heard > t_clr)
      {
        // Not too old, so draw trail

        if (temp_sec_heard > t_old)
        {
          // New trail, so draw solid trail

          if (debug_level & 256)
          {
            fprintf(stderr,"Drawing Solid trail for %s, secs old: %ld\n",
                    p_station->call_sign,
                    (long)(now - temp_sec_heard) );
          }
          draw_trail(w,p_station,1);
        }
        else
        {

          if (debug_level & 256)
          {
            fprintf(stderr,"Drawing trail for %s, secs old: %ld\n",
                    p_station->call_sign,
                    (long)(now - temp_sec_heard) );
          }
          draw_trail(w,p_station,0);
        }
      }
      else
      {
        if (debug_level & 256)
        {
          fprintf(stderr,"Station too old\n");
        }
      }
    }
    else
    {
      if (debug_level & 256)
      {
        fprintf(stderr,"Station trails %d, track data %lx\n",
                Display_.trail, (long int)p_station->newest_trackpoint);
      }
    }

    if (debug_level & 256)
    {
      fprintf(stderr,"calling display_station()\n");
    }

    // This routine will also update the
    // currently_selected_stations variable, if we're
    // updating all of the stations at once.
    display_station(w,p_station,0);

    p_station = p_station->t_newer;  // next station
  }

  draw_ruler(w);

  Draw_All_CAD_Objects(w);        // Draw all CAD objects, duh.

  // Check if we should mark where we found an address
  if (mark_destination && show_destination_mark)
  {
    int offset;

    // Set the line width in the GC.  Make it nice and fat.
    (void)XSetLineAttributes (XtDisplay (w), gc_tint, 7, LineSolid, CapButt,JoinMiter);
    (void)XSetForeground (XtDisplay (w), gc_tint, colors[0x27]);
    (void)(void)XSetFunction (XtDisplay (da), gc_tint, GXxor);

    // Scale it so that the 'X' stays the same size at all zoom
    // levels.
    offset = 25 * scale_y;

    // Make a big 'X'
    draw_vector(w,
                destination_coord_lon-offset,  // x1
                destination_coord_lat-offset,  // y1
                destination_coord_lon+offset,  // x2
                destination_coord_lat+offset,  // y2
                gc_tint,
                pixmap_final,
                0);

    draw_vector(w,
                destination_coord_lon+offset,  // x1
                destination_coord_lat-offset,  // y1
                destination_coord_lon-offset,  // x2
                destination_coord_lat+offset,  // y2
                gc_tint,
                pixmap_final,
                0);
  }

  // And last, draw the ALOHA circle
  if (Display_.aloha_circle)
  {
    if (aloha_radius != -1)
    {
      // if we actually have an aloha radius calculated already
      long l_lat,l_lon;

      l_lat = convert_lat_s2l(my_lat);
      l_lon = convert_lon_s2l(my_long);
      draw_aloha_circle(l_lon,
                        l_lat,
                        aloha_radius,
                        colors[0x0e],
                        pixmap_final);
    }
  }

  // Check whether currently_selected_stations has changed.  If
  // so, set station_count_save to 0 so that main.c will come
  // along and update the counts on the status line.
  if (currently_selected_stations != currently_selected_stations_save)
  {
    station_count_save = 0;   // Cause an update to occur
  }
  currently_selected_stations_save = currently_selected_stations;


  if (debug_level & 1)
  {
    fprintf(stderr,"Display File Stop\n");
  }
}


// ========================================================================
// STATION DATA DIALOG FUNCTIONS
// ========================================================================

/*
 *  Delete tracklog for current station
 */
void Station_data_destroy_track( Widget UNUSED(widget), XtPointer clientData, XtPointer UNUSED(callData) )
{
  DataRow *p_station = clientData;

  if (delete_trail(p_station))
  {
    redraw_on_new_data = 2;  // redraw immediately
  }
}


/*
 *  Delete Station Info PopUp
 */
void Station_data_destroy_shell(Widget UNUSED(widget), XtPointer clientData, XtPointer UNUSED(callData) )
{
  Widget shell = (Widget) clientData;
  XtPopdown(shell);

  begin_critical_section(&db_station_info_lock, "db.c:Station_data_destroy_shell" );

  XtDestroyWidget(shell);
  db_station_info = (Widget)NULL;

  end_critical_section(&db_station_info_lock, "db.c:Station_data_destroy_shell" );

}





/*
 *  Store track data for current station
 */
void Station_data_store_track(Widget UNUSED(w), XtPointer clientData, XtPointer UNUSED(callData) )
{
  DataRow *p_station = clientData;

  //busy_cursor(XtParent(w));
  busy_cursor(appshell);

  // Grey-out button so it doesn't get pressed twice
  XtSetSensitive(button_store_track,FALSE);

  // Store trail to file
  export_trail(p_station);

#ifdef HAVE_LIBSHP
  // Save trail as a Shapefile map
  create_map_from_trail(p_station->call_sign);
#endif  // HAVE_LIBSHP

  // store trail to kml file
  export_trail_as_kml(p_station);
}


// This function merely reformats the button callback in order to
// call wx_alert_double_click_action, which expects the parameter in
// calldata instead of in clientData.
//
void Station_data_wx_alert(Widget w, XtPointer clientData, XtPointer UNUSED(calldata) )
{
  //fprintf(stderr, "Station_data_wx_alert start\n");
  wx_alert_finger_output( w, clientData);
  //fprintf(stderr, "Station_data_wx_alert end\n");
}





void Station_data_add_fcc(Widget UNUSED(w), XtPointer clientData, XtPointer UNUSED(calldata) )
{
  char temp[500];
  FccAppl my_data;
  char *station = (char *) clientData;

  (void)check_fcc_data();
  //busy_cursor(XtParent(w));
  busy_cursor(appshell);
  if (search_fcc_data_appl(station, &my_data)==1)
  {
    /*fprintf(stderr,"FCC call %s\n",station);*/
    xastir_snprintf(temp, sizeof(temp), "%s\n%s %s\n%s %s %s\n%s %s, %s %s, %s %s\n\n",
                    langcode("STIFCC0001"),
                    langcode("STIFCC0003"),my_data.name_licensee,
                    langcode("STIFCC0004"),my_data.text_street,my_data.text_pobox,
                    langcode("STIFCC0005"),my_data.city,
                    langcode("STIFCC0006"),my_data.state,
                    langcode("STIFCC0007"),my_data.zipcode);
    XmTextInsert(si_text,0,temp);
    XmTextShowPosition(si_text,0);

    fcc_lookup_pushed = 1;
  }
}





void Station_data_add_rac(Widget UNUSED(w), XtPointer clientData, XtPointer UNUSED(calldata) )
{
  char temp[512];
  char club[512];
  rac_record my_data;
  char *station = (char *) clientData;

  xastir_snprintf(temp,
                  sizeof(temp),
                  " ");
  (void)check_rac_data();
  //busy_cursor(XtParent(w));
  busy_cursor(appshell);
  if (search_rac_data(station, &my_data)==1)
  {
    /*fprintf(stderr,"IC call %s\n",station);*/
    xastir_snprintf(temp, sizeof(temp), "%s\n%s %s\n%s\n%s, %s\n%s\n",
                    langcode("STIFCC0002"),my_data.first_name,my_data.last_name,my_data.address,
                    my_data.city,my_data.province,my_data.postal_code);

    if (my_data.qual_a[0] == 'A')
      strncat(temp,
              langcode("STIFCC0008"),
              sizeof(temp) - 1 - strlen(temp));

    if (my_data.qual_d[0] == 'D')
      strncat(temp,
              langcode("STIFCC0009"),
              sizeof(temp) - 1 - strlen(temp));

    if (my_data.qual_b[0] == 'B' && my_data.qual_c[0] != 'C')
      strncat(temp,
              langcode("STIFCC0010"),
              sizeof(temp) - 1 - strlen(temp));

    if (my_data.qual_c[0] == 'C')
      strncat(temp,
              langcode("STIFCC0011"),
              sizeof(temp) - 1 - strlen(temp));

    strncat(temp,
            "\n",
            sizeof(temp) - 1 - strlen(temp));

    if (strlen(my_data.club_name) > 1)
    {
      xastir_snprintf(club, sizeof(club), "%s\n%s\n%s, %s\n%s\n",
                      my_data.club_name, my_data.club_address,
                      my_data.club_city, my_data.club_province, my_data.club_postal_code);
      strncat(temp,
              club,
              sizeof(temp) - 1 - strlen(temp));
    }
    strncat(temp,
            "\n",
            sizeof(temp) - 1 - strlen(temp));
    XmTextInsert(si_text,0,temp);
    XmTextShowPosition(si_text,0);

    rac_lookup_pushed = 1;
  }
}


// Enable/disable auto-update of Station_data dialog
void station_data_auto_update_toggle ( Widget UNUSED(widget), XtPointer UNUSED(clientData), XtPointer callData)
{
  XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

  if(state->set)
  {
    station_data_auto_update = 1;
  }
  else
  {
    station_data_auto_update = 0;
  }
}


/*
 *  Change the trail color for a station
 */
void Change_trail_color( Widget UNUSED(w), XtPointer clientData, XtPointer UNUSED(calldata) )
{
  DataRow *p_station = clientData;
  int temp;

  temp = p_station->trail_color;

  // Increment to the next color, round-robin style
  temp = (temp + 1) % MAX_TRAIL_COLORS;

  // Test for and skip if my trail color
  if (temp == MY_TRAIL_COLOR)
  {
    temp = (temp + 1) % MAX_TRAIL_COLORS;
  }

  p_station->trail_color = temp;

  redraw_on_new_data = 2; // redraw symbols now
}


/*
 * Clear DF from Station_data
 *
 * Called by Station_data function below from the Clear DF Bearing
 * button in Station Info.
 */
void Clear_DF_from_Station_data(Widget UNUSED(w), XtPointer clientData, XtPointer UNUSED(calldata) )
{
  DataRow *p_station = clientData;

  if (strlen(p_station->bearing) == 3)
  {
    // we have DF data to clear
    p_station->bearing[0]='\0';
    p_station->NRQ[0]='\0';
  }
}


// ========================================================================
// STATION QUERY FUNCTIONS
// ========================================================================

void Station_query_trace(Widget UNUSED(w), XtPointer clientData, XtPointer UNUSED(calldata) )
{
  char *station = (char *) clientData;
  char temp[50];
  char call[25];

  pad_callsign(call,station);
  xastir_snprintf(temp, sizeof(temp), ":%s:?APRST", call);

  // Nice to return via the reverse path here?  No!  Better to use the
  // default paths instead of a calculated reverse path.

  transmit_message_data(station,temp,NULL);
}





void Station_query_messages(Widget UNUSED(w), XtPointer clientData, XtPointer UNUSED(calldata) )
{
  char *station = (char *) clientData;
  char temp[50];
  char call[25];

  pad_callsign(call,station);
  xastir_snprintf(temp, sizeof(temp), ":%s:?APRSM", call);

  // Nice to return via the reverse path here?  No!  Better to use the
  // default paths instead of a calculated reverse path.

  transmit_message_data(station,temp,NULL);
}





void Station_query_direct(Widget UNUSED(w), XtPointer clientData, XtPointer UNUSED(calldata) )
{
  char *station = (char *) clientData;
  char temp[50];
  char call[25];

  pad_callsign(call,station);
  xastir_snprintf(temp, sizeof(temp), ":%s:?APRSD", call);

  // Nice to return via the reverse path here?  No!  Better to use the
  // default paths instead of a calculated reverse path.

  transmit_message_data(station,temp,NULL);
}





void Station_query_version(Widget UNUSED(w), XtPointer clientData, XtPointer UNUSED(calldata) )
{
  char *station = (char *) clientData;
  char temp[50];
  char call[25];

  pad_callsign(call,station);
  xastir_snprintf(temp, sizeof(temp), ":%s:?VER", call);

  // Nice to return via the reverse path here?  No!  Better to use the
  // default paths instead of a calculated reverse path.

  transmit_message_data(station,temp,NULL);
}


void General_query(Widget UNUSED(w), XtPointer clientData, XtPointer UNUSED(calldata) )
{
  char *location = (char *) clientData;
  char temp[50];

  xastir_snprintf(temp, sizeof(temp), "?APRS?%s", location);
  output_my_data(temp,-1,0,0,0,NULL);  // Not igating
}


void IGate_query(Widget UNUSED(w), XtPointer UNUSED(clientData), XtPointer UNUSED(calldata) )
{
  output_my_data("?IGATE?",-1,0,0,0,NULL); // Not igating
}


void WX_query(Widget UNUSED(w), XtPointer UNUSED(clientData), XtPointer UNUSED(calldata) )
{
  output_my_data("?WX?",-1,0,0,0,NULL);    // Not igating
}


// popup window on menu request
void Show_Aloha_Stats(Widget UNUSED(w), XtPointer UNUSED(clientData), XtPointer UNUSED(callData) )
{

  char temp[2000];
  char format[1000];

  unsigned long time_since_aloha_update;
  int minutes, hours;
  char Hours[7];
  char Minutes[9];

  if (aloha_radius != -1)
  {
    // we've done at least one interval, and aloha_time is the time
    // for the *next* one.  We want the time since the last one.
    time_since_aloha_update = sec_now()-(aloha_time-ALOHA_CALC_INTERVAL);


    hours = time_since_aloha_update/3600;
    time_since_aloha_update -= hours*3600;
    minutes = time_since_aloha_update/60;

    if (hours == 1)
      xastir_snprintf(Hours,sizeof(Hours),"%s",
                      langcode("TIME003")); // Hour
    else
      xastir_snprintf(Hours,sizeof(Hours),"%s",
                      langcode("TIME004")); // Hours


    if (minutes == 1)
      xastir_snprintf(Minutes,sizeof(Minutes),"%s",
                      langcode("TIME005")); // Minute
    else
      xastir_snprintf(Minutes,sizeof(Minutes),"%s",
                      langcode("TIME006")); // Minutes

    // Build up the whole format string
    // "Aloha radius %d"
    xastir_snprintf(format,sizeof(format),"%s",langcode("WPUPALO001"));
    strncat(format,"\n",sizeof(format) - 1 - strlen(format));
    // "Stations inside...: %d"
    strncat(format,langcode("WPUPALO002"),sizeof(format) - 1 - strlen(format));
    strncat(format,"\n",sizeof(format) - 1 - strlen(format));
    //" Digis:               %d"
    strncat(format,langcode("WPUPALO003"),sizeof(format) - 1 - strlen(format));
    strncat(format,"\n",sizeof(format) - 1 - strlen(format));
    //" Mobiles (in motion): %d"
    strncat(format,langcode("WPUPALO004"),sizeof(format) - 1 - strlen(format));
    strncat(format,"\n",sizeof(format) - 1 - strlen(format));
    //" Mobiles (other):     %d"
    strncat(format,langcode("WPUPALO005"),sizeof(format) - 1 - strlen(format));
    strncat(format,"\n",sizeof(format) - 1 - strlen(format));
    //" WX stations:         %d"
    strncat(format,langcode("WPUPALO006"),sizeof(format) - 1 - strlen(format));
    strncat(format,"\n",sizeof(format) - 1 - strlen(format));
    //" Home stations:       %d"
    strncat(format,langcode("WPUPALO007"),sizeof(format) - 1 - strlen(format));
    strncat(format,"\n",sizeof(format) - 1 - strlen(format));
    //"Last calculated %s ago."
    strncat(format,langcode("WPUPALO008"),sizeof(format) - 1 - strlen(format));
    strncat(format,"\n",sizeof(format) - 1 - strlen(format));

    // We now have the whole format string, now print using it:
    xastir_snprintf(temp,sizeof(temp),format,
                    (english_units) ? (int)aloha_radius : (int)(aloha_radius * cvt_mi2len),
                    (english_units)?" miles":" km",
                    the_aloha_stats.total,
                    the_aloha_stats.digis,
                    the_aloha_stats.mobiles_in_motion,
                    the_aloha_stats.other_mobiles,
                    the_aloha_stats.wxs,
                    the_aloha_stats.homes,
                    hours, Hours,
                    minutes, Minutes);

    popup_message_always(langcode("PULDNVI016"),temp);
  }
  else
  {
    // Not calculated yet
    popup_message_always(langcode("PULDNVI016"),langcode("WPUPALO666"));
  }
}


// ========================================================================
// STATION DATA FILL-IN AND DISPLAY
// ========================================================================

// Fill in the station data window with real data
void station_data_fill_in ( Widget w, XtPointer clientData, XtPointer calldata )
{
  DataRow *p_station;
  char *station = (char *) clientData;
  char temp[300];
  int pos, last_pos;
  char temp_my_distance[20];
  char temp_my_course[25];
  char temp1_my_course[20];
  float temp_out_C, e, humidex;
  long l_lat, l_lon;
  float value;
  WeatherRow *weather;
  time_t sec;
  struct tm *time;
  int i;
  int track_count = 0;

  // Maximum tracks listed in Station Info dialog.  This prevents
  // lockups on extremely long tracks.
#define MAX_TRACK_LIST 50


  db_station_info_callsign = (char *) clientData; // Used for auto-updating this dialog
  temp_out_C=0;
  pos=0;

  begin_critical_section(&db_station_info_lock, "db.c:Station_data" );

  if (db_station_info == NULL)    // We don't have a dialog to write to
  {

    end_critical_section(&db_station_info_lock, "db.c:Station_data" );

    return;
  }

  if (!search_station_name(&p_station,station,1)  // Can't find call,
      || (p_station->flag & ST_ACTIVE) == 0)    // or found deleted objects
  {

    end_critical_section(&db_station_info_lock, "db.c:Station_data" );

    return;
  }


  // Clear the text
  XmTextSetString(si_text,NULL);


  // Weather Data ...
  if (p_station->weather_data != NULL
      // Make sure the timestamp on the weather is current
      && (int)(((sec_old + p_station->weather_data->wx_sec_time)) >= sec_now()) )
  {

    last_pos = pos;

    weather = p_station->weather_data;

    pos += strlen(temp);
    xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI024"),weather->wx_type,weather->wx_station);
    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);
    sprintf(temp, "\n");
    xastir_snprintf(temp, sizeof(temp), "\n");
    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);
    if (english_units)
    {
      xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI026"),weather->wx_course,weather->wx_speed);
    }
    else
    {
      xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI025"),weather->wx_course,(int)(atof(weather->wx_speed)*1.6094));
    }

    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);

    if (strlen(weather->wx_gust) > 0)
    {
      if (english_units)
      {
        xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI028"),weather->wx_gust);
      }
      else
      {
        xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI027"),(int)(atof(weather->wx_gust)*1.6094));
      }

      strncat(temp,
              "\n",
              sizeof(temp) - 1 - strlen(temp));
    }
    else
    {
      xastir_snprintf(temp, sizeof(temp), "\n");
    }

    XmTextInsert(si_text, pos, temp);
    pos += strlen(temp);

    if (strlen(weather->wx_temp) > 0)
    {
      if (english_units)
      {
        xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI030"),weather->wx_temp);
      }
      else
      {
        temp_out_C =(((atof(weather->wx_temp)-32)*5.0)/9.0);
        xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI029"),temp_out_C);
      }
      XmTextInsert(si_text,pos,temp);
      pos += strlen(temp);
    }

    if (strlen(weather->wx_hum) > 0)
    {
      xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI031"),weather->wx_hum);
      XmTextInsert(si_text,pos,temp);
      pos += strlen(temp);
    }

    // NOTE:  The below (Humidex) is not coded for english units, only for metric.
    // What is Humidex anyway?  Heat Index?  Wind Chill? --we7u

    // DK7IN: ??? english_units ???
    if (strlen(weather->wx_hum) > 0
        && strlen(weather->wx_temp) > 0
        && (!english_units) &&
        (atof(weather->wx_hum) > 0.0) )
    {

      e = (float)(6.112 * pow(10,(7.5 * temp_out_C)/(237.7 + temp_out_C)) * atof(weather->wx_hum) / 100.0);
      humidex = (temp_out_C + ((5.0/9.0) * (e-10.0)));

      xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI032"),humidex);
      XmTextInsert(si_text,pos,temp);
      pos += strlen(temp);
    }

    if (strlen(weather->wx_baro) > 0)
    {
      if (!english_units)    // hPa
      {
        xastir_snprintf(temp, sizeof(temp),
                        langcode("WPUPSTI033"),
                        weather->wx_baro);
      }
      else    // Inches Mercury
      {
        xastir_snprintf(temp, sizeof(temp),
                        langcode("WPUPSTI063"),
                        atof(weather->wx_baro)*0.02953);
      }
      XmTextInsert(si_text,pos,temp);
      pos += strlen(temp);
      xastir_snprintf(temp, sizeof(temp), "\n");
      XmTextInsert(si_text,pos,temp);
      pos += strlen(temp);
    }
    else
    {
      if(last_pos!=pos)
      {
        xastir_snprintf(temp, sizeof(temp), "\n");
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);
      }
    }

    if (strlen(weather->wx_snow) > 0)
    {
      if(english_units)
      {
        xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI035"),atof(weather->wx_snow));
      }
      else
      {
        xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI034"),atof(weather->wx_snow)*2.54);
      }
      XmTextInsert(si_text,pos,temp);
      pos += strlen(temp);
      xastir_snprintf(temp, sizeof(temp), "\n");
      XmTextInsert(si_text,pos,temp);
      pos += strlen(temp);
    }

    if (strlen(weather->wx_rain) > 0 || strlen(weather->wx_prec_00) > 0
        || strlen(weather->wx_prec_24) > 0)
    {
      xastir_snprintf(temp, sizeof(temp), "%s", langcode("WPUPSTI036"));
      XmTextInsert(si_text,pos,temp);
      pos += strlen(temp);
    }

    if (strlen(weather->wx_rain) > 0)
    {
      if (english_units)
      {
        xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI038"),atof(weather->wx_rain)/100.0);
      }
      else
      {
        xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI037"),atof(weather->wx_rain)*.254);
      }

      XmTextInsert(si_text,pos,temp);
      pos += strlen(temp);
    }

    if (strlen(weather->wx_prec_24) > 0)
    {
      if(english_units)
      {
        xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI040"),atof(weather->wx_prec_24)/100.0);
      }
      else
      {
        xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI039"),atof(weather->wx_prec_24)*.254);
      }

      XmTextInsert(si_text,pos,temp);
      pos += strlen(temp);
    }

    if (strlen(weather->wx_prec_00) > 0)
    {
      if (english_units)
      {
        xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI042"),atof(weather->wx_prec_00)/100.0);
      }
      else
      {
        xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI041"),atof(weather->wx_prec_00)*.254);
      }

      XmTextInsert(si_text,pos,temp);
      pos += strlen(temp);
    }

    if (strlen(weather->wx_rain_total) > 0)
    {
      xastir_snprintf(temp, sizeof(temp), "\n%s",langcode("WPUPSTI046"));
      XmTextInsert(si_text,pos,temp);
      pos += strlen(temp);
      if (english_units)
      {
        xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI048"),atof(weather->wx_rain_total)/100.0);
      }
      else
      {
        xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI047"),atof(weather->wx_rain_total)*.254);
      }

      XmTextInsert(si_text,pos,temp);
      pos += strlen(temp);
    }

    // Fuel temp/moisture for RAWS weather stations
    if (strlen(weather->wx_fuel_temp) > 0)
    {
      if (english_units)
      {
        xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI061"),weather->wx_fuel_temp);
      }
      else
      {
        temp_out_C =(((atof(weather->wx_fuel_temp)-32)*5.0)/9.0);
        xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI060"),temp_out_C);
      }
      XmTextInsert(si_text,pos,temp);
      pos += strlen(temp);
    }

    if (strlen(weather->wx_fuel_moisture) > 0)
    {
      xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI062"),weather->wx_fuel_moisture);
      XmTextInsert(si_text,pos,temp);
      pos += strlen(temp);
    }

    xastir_snprintf(temp, sizeof(temp), "\n\n");

    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);
  }


  // Packets received ...
  xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI005"),p_station->num_packets);
  XmTextInsert(si_text,pos,temp);
  pos += strlen(temp);

  xastir_snprintf(temp,
                  sizeof(temp),
                  "%s",
                  p_station->packet_time);
  temp[2]='/';
  temp[3]='\0';
  XmTextInsert(si_text,pos,temp);
  pos += strlen(temp);

  xastir_snprintf(temp,
                  sizeof(temp),
                  "%s",
                  p_station->packet_time+2);
  temp[2]='/';
  temp[3]='\0';
  XmTextInsert(si_text,pos,temp);
  pos += strlen(temp);

  xastir_snprintf(temp,
                  sizeof(temp),
                  "%s",
                  p_station->packet_time+4);
  temp[4]=' ';
  temp[5]='\0';
  XmTextInsert(si_text,pos,temp);
  pos += strlen(temp);

  xastir_snprintf(temp,
                  sizeof(temp),
                  "%s",
                  p_station->packet_time+8);
  temp[2]=':';
  temp[3]='\0';
  XmTextInsert(si_text,pos,temp);
  pos += strlen(temp);

  xastir_snprintf(temp,
                  sizeof(temp),
                  "%s",
                  p_station->packet_time+10);
  temp[2]=':';
  temp[3]='\0';
  XmTextInsert(si_text,pos,temp);
  pos += strlen(temp);

  xastir_snprintf(temp,
                  sizeof(temp),
                  "%s",
                  p_station->packet_time+12);
  temp[2]='\n';
  temp[3]='\0';
  XmTextInsert(si_text,pos,temp);
  pos += strlen(temp);

  // Object
  if (strlen(p_station->origin) > 0)
  {
    xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI000"),p_station->origin);
    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);
    xastir_snprintf(temp, sizeof(temp), "\n");
    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);
  }

  // Print the tactical call, if any
  if (p_station->tactical_call_sign
      && p_station->tactical_call_sign[0] != '\0')
  {
    xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI065"), p_station->tactical_call_sign);
    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);
    xastir_snprintf(temp, sizeof(temp), "\n");
    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);
  }

  // Heard via TNC ...
  if ((p_station->flag & ST_VIATNC) != 0)          // test "via TNC" flag
  {
    xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI006"),p_station->heard_via_tnc_port);
    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);
  }
  else
  {
    xastir_snprintf(temp, sizeof(temp), "%s", langcode("WPUPSTI007"));
    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);
  }

  switch(p_station->data_via)
  {
    case('L'):
      xastir_snprintf(temp, sizeof(temp), "%s", langcode("WPUPSTI008"));
      break;

    case('T'):
      xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI009"),p_station->last_port_heard);
      break;

    case('I'):
      xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI010"),p_station->last_port_heard);
      break;

    case('F'):
      xastir_snprintf(temp, sizeof(temp), "%s", langcode("WPUPSTI011"));
      break;

    case(DATA_VIA_DATABASE):
      xastir_snprintf(temp, sizeof(temp), "last via db on interface %d",p_station->last_port_heard);
      break;

    default:
      xastir_snprintf(temp, sizeof(temp), "%s", langcode("WPUPSTI012"));
      break;
  }
  XmTextInsert(si_text,pos,temp);
  pos += strlen(temp);

  if (p_station->newest_trackpoint != NULL)
  {
    xastir_snprintf(temp, sizeof(temp), "%s", langcode("WPUPSTI013"));
    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);
  }
  xastir_snprintf(temp, sizeof(temp), "\n");
  XmTextInsert(si_text,pos,temp);
  pos += strlen(temp);

  // Echoed from: ...
  // Callsign check here includes checking SSID
  //    if (is_my_call(p_station->call_sign,1)) {
  if ( is_my_station(p_station) )
  {
    xastir_snprintf(temp, sizeof(temp), "%s", langcode("WPUPSTI055"));
    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);
    for (i=0; i<6; i++)
    {
      if (echo_digis[i][0] == '\0')
      {
        break;
      }

      xastir_snprintf(temp, sizeof(temp), " %s",echo_digis[i]);
      XmTextInsert(si_text,pos,temp);
      pos += strlen(temp);
    }
    xastir_snprintf(temp, sizeof(temp), "\n");
    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);
  }

  // Data Path ...
  if (p_station->node_path_ptr != NULL)
  {
    xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI043"),p_station->node_path_ptr);
  }
  else
  {
    xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI043"), "");
  }

  XmTextInsert(si_text,pos,temp);
  pos += strlen(temp);
  xastir_snprintf(temp, sizeof(temp), "\n");
  XmTextInsert(si_text,pos,temp);
  pos += strlen(temp);

  // Status ...
  if(p_station->status_data != NULL)     // Found at least one record
  {
    CommentRow *ptr;

    ptr = p_station->status_data;

    while (ptr != NULL)
    {
      // We don't care if the pointer is NULL.  This will
      // succeed anyway.  It'll just make an empty string.

      // Note that text_ptr may be an empty string.  That's
      // ok.

      //Also print the sec_heard timestamp.
      sec = ptr->sec_heard;
      time = localtime(&sec);

      xastir_snprintf(temp,
                      sizeof(temp),
                      langcode("WPUPSTI059"),
                      time->tm_mon + 1,
                      time->tm_mday,
                      time->tm_hour,
                      time->tm_min,
                      ptr->text_ptr);
      XmTextInsert(si_text,pos,temp);
      pos += strlen(temp);

      xastir_snprintf(temp, sizeof(temp), "\n");

      XmTextInsert(si_text,pos,temp);
      pos += strlen(temp);
      ptr = ptr->next;    // Advance to next record (if any)
    }
  }


//    // Comments ...
//    if(strlen(p_station->comments)>0) {
//        xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI044"),p_station->comments);
//        XmTextInsert(si_text,pos,temp);
//        pos += strlen(temp);
//        xastir_snprintf(temp, sizeof(temp), "\n");
//        XmTextInsert(si_text,pos,temp);
//        pos += strlen(temp);
//    }

  // Comments ...
  if(p_station->comment_data != NULL)     // Found at least one record
  {
    CommentRow *ptr;

    ptr = p_station->comment_data;

    while (ptr != NULL)
    {
      // We don't care if the pointer is NULL.  This will
      // succeed anyway.  It'll just make an empty string.

      // Note that text_ptr can be an empty string.  That's
      // ok.

      //Also print the sec_heard timestamp.
      sec = ptr->sec_heard;
      time = localtime(&sec);

      xastir_snprintf(temp,
                      sizeof(temp),
                      langcode("WPUPSTI044"),
                      time->tm_mon + 1,
                      time->tm_mday,
                      time->tm_hour,
                      time->tm_min,
                      ptr->text_ptr);
      XmTextInsert(si_text,pos,temp);
      pos += strlen(temp);

      xastir_snprintf(temp, sizeof(temp), "\n");

      XmTextInsert(si_text,pos,temp);
      pos += strlen(temp);
      ptr = ptr->next;    // Advance to next record (if any)
    }
  }

  // Current Power Gain ...
  if (strlen(p_station->power_gain) == 7)
  {
    // Check for RNG instead of PHG
    if (p_station->power_gain[0] == 'R')
    {
      // Found a Range
      xastir_snprintf(temp,
                      sizeof(temp),
                      langcode("WPUPSTI067"),
                      atoi(&p_station->power_gain[3]));
    }
    else
    {
      // Found PHG
      phg_decode(langcode("WPUPSTI014"), // "Current Power Gain"
                 p_station->power_gain,
                 temp,
                 sizeof(temp), english_units );
    }

    // Check for Map View symbol:  Eyeball symbol with // RNG
    // extension.
    if ( strncmp(p_station->power_gain,"RNG",3) == 0
         && p_station->aprs_symbol.aprs_type == '/'
         && p_station->aprs_symbol.aprs_symbol == 'E' )
    {

      //fprintf(stderr,"Found a Map View 'eyeball' symbol!\n");

      // Center_Zoom() normally fills in the values with the
      // current zoom/center for the map window.  We want to
      // be able to override these with our own values in this
      // case, derived from the object info.
      center_zoom_override++;
      Center_Zoom(w,NULL,(XtPointer)p_station);
    }
  }
  else if (p_station->flag & (ST_OBJECT | ST_ITEM))
  {
    xastir_snprintf(temp,
                    sizeof(temp),
                    "%s %s",
                    langcode("WPUPSTI014"), // "Current Power Gain:"
                    langcode("WPUPSTI068") );   // "none"
  }
  else if (english_units)
  {
    xastir_snprintf(temp,
                    sizeof(temp),
                    "%s %s (9W @ 20ft %s, 3dB %s, %s 6.2mi)",
                    langcode("WPUPSTI014"), // "Current Power Gain:"
                    langcode("WPUPSTI069"), // "default"
                    langcode("WPUPSTI070"), // "HAAT"
                    langcode("WPUPSTI071"), // "omni"
                    langcode("WPUPSTI072") ); // "range"
    //          "default (9W @ 20ft HAAT, 3dB omni, range 6.2mi)");
  }
  else
  {
    xastir_snprintf(temp,
                    sizeof(temp),
                    "%s %s (9W @ 6.1m %s, 3dB %s, %s 10.0km)",
                    langcode("WPUPSTI014"), // "Current Power Gain:"
                    langcode("WPUPSTI069"), // "default"
                    langcode("WPUPSTI070"), // "HAAT"
                    langcode("WPUPSTI071"), // "omni"
                    langcode("WPUPSTI072") ); // "range"
    //          "default (9W @ 6.1m HAAT, 3dB omni, range 10.0km)");

  }

  XmTextInsert(si_text,pos,temp);
  pos += strlen(temp);
  xastir_snprintf(temp, sizeof(temp), "\n");
  XmTextInsert(si_text,pos,temp);
  pos += strlen(temp);

  // Current DF Info ...
  if (strlen(p_station->signal_gain) == 7)
  {
    shg_decode(langcode("WPUPSTI057"), p_station->signal_gain, temp, sizeof(temp) , english_units);
    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);
    xastir_snprintf(temp, sizeof(temp), "\n");
    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);
  }
  if (strlen(p_station->bearing) == 3)
  {
    bearing_decode(langcode("WPUPSTI058"), p_station->bearing, p_station->NRQ, temp, sizeof(temp), english_units );
    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);
    xastir_snprintf(temp, sizeof(temp), "\n");
    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);
  }

  // Signpost Data
  if (strlen(p_station->signpost) > 0)
  {
    xastir_snprintf(temp, sizeof(temp), "%s: %s",langcode("POPUPOB029"), p_station->signpost);
    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);
    xastir_snprintf(temp, sizeof(temp), "\n");
    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);
  }

  // Altitude ...
  last_pos=pos;
  if (strlen(p_station->altitude) > 0)
  {
    if (english_units)
    {
      xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI016"),atof(p_station->altitude)*3.28084,"ft");
    }
    else
    {
      xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI016"),atof(p_station->altitude),"m");
    }

    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);
  }

  // Course ...
  if (strlen(p_station->course) > 0)
  {
    xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI017"),p_station->course);
    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);
  }

  // Speed ...
  if (strlen(p_station->speed) > 0)
  {
    if (english_units)
    {
      xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI019"),atof(p_station->speed)*1.1508);
    }

    else
    {
      xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI018"),atof(p_station->speed)*1.852);
    }

    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);
  }

  if (last_pos!=pos)
  {
    xastir_snprintf(temp, sizeof(temp), "\n");
    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);
  }

  // Distance ...
  last_pos = pos;

  // do my course
  //    if (!is_my_call(p_station->call_sign,1)) { // Checks SSID as well
  if ( !(is_my_station(p_station)) )   // Checks SSID as well
  {

    l_lat = convert_lat_s2l(my_lat);
    l_lon = convert_lon_s2l(my_long);

    // Get distance in nautical miles.
    value = (float)calc_distance_course(l_lat,l_lon,p_station->coord_lat,
                                        p_station->coord_lon,temp1_my_course,sizeof(temp1_my_course));

    // n7tap: This is a quick hack to get some more useful values for
    //        distance to near objects.
    if (english_units)
    {
      if (value*1.15078 < 0.99)
      {
        xastir_snprintf(temp_my_distance,
                        sizeof(temp_my_distance),
                        "%d %s",
                        (int)(value*1.15078*1760),
                        langcode("SPCHSTR004"));    // yards
      }
      else
      {
        xastir_snprintf(temp_my_distance,
                        sizeof(temp_my_distance),
                        langcode("WPUPSTI020"),     // miles
                        value*1.15078);
      }
    }
    else
    {
      if (value*1.852 < 0.99)
      {
        xastir_snprintf(temp_my_distance,
                        sizeof(temp_my_distance),
                        "%d %s",
                        (int)(value*1.852*1000),
                        langcode("UNIOP00031"));    // 'm' as in meters
      }
      else
      {
        xastir_snprintf(temp_my_distance,
                        sizeof(temp_my_distance),
                        langcode("WPUPSTI021"),     // km
                        value*1.852);
      }
    }
    xastir_snprintf(temp_my_course, sizeof(temp_my_course), "%s\xB0",temp1_my_course);
    xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI022"),temp_my_distance,temp_my_course);
    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);
  }

  if(last_pos!=pos)
  {
    xastir_snprintf(temp, sizeof(temp), "\n");
    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);
  }

  // Last Position
  sec  = p_station->sec_heard;
  time = localtime(&sec);
  xastir_snprintf(temp, sizeof(temp), "%s%02d/%02d  %02d:%02d   ",langcode("WPUPSTI023"),
                  time->tm_mon + 1, time->tm_mday,time->tm_hour,time->tm_min);
  XmTextInsert(si_text,pos,temp);
  pos += strlen(temp);

  if (coordinate_system == USE_UTM
      || coordinate_system == USE_UTM_SPECIAL)
  {
    convert_xastir_to_UTM_str(temp, sizeof(temp),
                              p_station->coord_lon, p_station->coord_lat);
    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);
  }
  else if (coordinate_system == USE_MGRS)
  {
    convert_xastir_to_MGRS_str(temp,
                               sizeof(temp),
                               p_station->coord_lon,
                               p_station->coord_lat,
                               0);
    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);
  }
  else
  {
    if (coordinate_system == USE_DDDDDD)
    {
      convert_lat_l2s(p_station->coord_lat, temp, sizeof(temp), CONVERT_DEC_DEG);
    }
    else if (coordinate_system == USE_DDMMSS)
    {
      convert_lat_l2s(p_station->coord_lat, temp, sizeof(temp), CONVERT_DMS_NORMAL);
    }
    else    // Assume coordinate_system == USE_DDMMMM
    {
      convert_lat_l2s(p_station->coord_lat, temp, sizeof(temp), CONVERT_HP_NORMAL);
    }
    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);

    xastir_snprintf(temp, sizeof(temp), "  ");
    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);

    if (coordinate_system == USE_DDDDDD)
    {
      convert_lon_l2s(p_station->coord_lon, temp, sizeof(temp), CONVERT_DEC_DEG);
    }
    else if (coordinate_system == USE_DDMMSS)
    {
      convert_lon_l2s(p_station->coord_lon, temp, sizeof(temp), CONVERT_DMS_NORMAL);
    }
    else    // Assume coordinate_system == USE_DDMMMM
    {
      convert_lon_l2s(p_station->coord_lon, temp, sizeof(temp), CONVERT_HP_NORMAL);
    }
    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);
  }

  if (p_station->altitude[0] != '\0')
  {
    xastir_snprintf(temp, sizeof(temp), " %5.0f%s", atof(p_station->altitude)*cvt_m2len, un_alt);
  }
  else
  {
    substr(temp,"        ",1+5+strlen(un_alt));
  }
  XmTextInsert(si_text,pos,temp);
  pos += strlen(temp);

  if (p_station->speed[0] != '\0')
  {
    xastir_snprintf(temp, sizeof(temp), " %4.0f%s",atof(p_station->speed)*cvt_kn2len,un_spd);
  }
  else
  {
    substr(temp,"         ",1+4+strlen(un_spd));
  }
  XmTextInsert(si_text,pos,temp);
  pos += strlen(temp);

  if (p_station->course[0] != '\0')
  {
    xastir_snprintf(temp, sizeof(temp), " %3d\xB0",atoi(p_station->course));
  }
  else
  {
    xastir_snprintf(temp, sizeof(temp), "     ");
  }

  XmTextInsert(si_text,pos,temp);
  pos += strlen(temp);

  // dl9sau
  // Maidenhead Grid Locator
  xastir_snprintf(temp, sizeof(temp), "  %s", sec_to_loc(p_station->coord_lon, p_station->coord_lat) );
  XmTextInsert(si_text,pos,temp);
  pos += strlen(temp);

  if ((p_station->flag & ST_DIRECT) != 0)
  {
    xastir_snprintf(temp, sizeof(temp), " *\n");
  }

  else
  {
    xastir_snprintf(temp, sizeof(temp), "  \n");
  }

  XmTextInsert(si_text,pos,temp);
  pos += strlen(temp);

  // list rest of trail data
  if (p_station->newest_trackpoint != NULL)
  {
    TrackRow *ptr;

    ptr = p_station->newest_trackpoint;

    // Skip the first (latest) trackpoint as if it exists, it'll
    // be the same as the data in the station record, which we
    // just printed out.
    if (ptr->prev != NULL)
    {
      ptr = ptr->prev;
    }

    while ( (ptr != NULL) && (track_count <= MAX_TRACK_LIST) )
    {

      track_count++;

      sec  = ptr->sec;
      time = localtime(&sec);
      if ((ptr->flag & TR_NEWTRK) != '\0')
        xastir_snprintf(temp, sizeof(temp), "            +  %02d/%02d  %02d:%02d   ",
                        time->tm_mon + 1,time->tm_mday,time->tm_hour,time->tm_min);
      else
        xastir_snprintf(temp, sizeof(temp), "               %02d/%02d  %02d:%02d   ",
                        time->tm_mon + 1,time->tm_mday,time->tm_hour,time->tm_min);

      XmTextInsert(si_text,pos,temp);
      pos += strlen(temp);

      if (coordinate_system == USE_UTM
          || coordinate_system == USE_UTM_SPECIAL)
      {
        convert_xastir_to_UTM_str(temp, sizeof(temp),
                                  ptr->trail_long_pos,
                                  ptr->trail_lat_pos);
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);
      }
      else if (coordinate_system == USE_MGRS)
      {
        convert_xastir_to_MGRS_str(temp,
                                   sizeof(temp),
                                   ptr->trail_long_pos,
                                   ptr->trail_lat_pos,
                                   0);
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);
      }
      else
      {
        if (coordinate_system == USE_DDDDDD)
        {
          convert_lat_l2s(ptr->trail_lat_pos,
                          temp,
                          sizeof(temp),
                          CONVERT_DEC_DEG);
        }
        else if (coordinate_system == USE_DDMMSS)
        {
          convert_lat_l2s(ptr->trail_lat_pos,
                          temp,
                          sizeof(temp),
                          CONVERT_DMS_NORMAL);
        }
        else    // Assume coordinate_system == USE_DDMMMM
        {
          convert_lat_l2s(ptr->trail_lat_pos,
                          temp,
                          sizeof(temp),
                          CONVERT_HP_NORMAL);
        }
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);

        xastir_snprintf(temp, sizeof(temp), "  ");
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);

        if (coordinate_system == USE_DDDDDD)
        {
          convert_lon_l2s(ptr->trail_long_pos,
                          temp,
                          sizeof(temp),
                          CONVERT_DEC_DEG);
        }
        else if (coordinate_system == USE_DDMMSS)
        {
          convert_lon_l2s(ptr->trail_long_pos,
                          temp,
                          sizeof(temp),
                          CONVERT_DMS_NORMAL);
        }
        else    // Assume coordinate_system == USE_DDMMMM
        {
          convert_lon_l2s(ptr->trail_long_pos,
                          temp,
                          sizeof(temp),
                          CONVERT_HP_NORMAL);
        }
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);
      }

      if (ptr->altitude > -99999l)
        xastir_snprintf(temp, sizeof(temp), " %5.0f%s",
                        ptr->altitude * cvt_dm2len,
                        un_alt);
      else
      {
        substr(temp,"         ",1+5+strlen(un_alt));
      }

      XmTextInsert(si_text,pos,temp);
      pos += strlen(temp);

      if (ptr->speed >= 0)
        xastir_snprintf(temp, sizeof(temp), " %4.0f%s",
                        ptr->speed * cvt_hm2len,
                        un_spd);
      else
      {
        substr(temp,"         ",1+4+strlen(un_spd));
      }

      XmTextInsert(si_text,pos,temp);
      pos += strlen(temp);

      if (ptr->course >= 0)
        xastir_snprintf(temp, sizeof(temp), " %3d\xB0",
                        ptr->course);
      else
      {
        xastir_snprintf(temp, sizeof(temp), "     ");
      }

      XmTextInsert(si_text,pos,temp);
      pos += strlen(temp);

      // dl9sau
      xastir_snprintf(temp, sizeof(temp), "  %s",
                      sec_to_loc(ptr->trail_long_pos,
                                 ptr->trail_lat_pos) );
      XmTextInsert(si_text,pos,temp);
      pos += strlen(temp);

      if ((ptr->flag & TR_LOCAL) != '\0')
      {
        xastir_snprintf(temp, sizeof(temp), " *\n");
      }
      else
      {
        xastir_snprintf(temp, sizeof(temp), "  \n");
      }

      XmTextInsert(si_text,pos,temp);
      pos += strlen(temp);

      // Go back in time one trackpoint
      ptr = ptr->prev;
    }
  }


  if (fcc_lookup_pushed)
  {
    Station_data_add_fcc(w, clientData, calldata);
  }
  else if (rac_lookup_pushed)
  {
    Station_data_add_rac(w, clientData, calldata);
  }

  XmTextShowPosition(si_text,0);

  end_critical_section(&db_station_info_lock, "db.c:Station_data" );

}


// ========================================================================
// TACTICAL CALL MANAGEMENT FUNCTIONS
// ========================================================================

void Change_tactical_destroy_shell( Widget UNUSED(widget), XtPointer clientData, XtPointer UNUSED(callData) )
{
  Widget shell = (Widget) clientData;
  XtPopdown(shell);
  XtDestroyWidget(shell);
  change_tactical_dialog = (Widget)NULL;
}

void Change_tactical_change_data(Widget widget, XtPointer clientData, XtPointer callData)
{
  char *temp;

  temp = XmTextGetString(tactical_text);

  if (tactical_pointer->tactical_call_sign == NULL)
  {
    // Malloc some memory to hold it.
    tactical_pointer->tactical_call_sign = (char *)malloc(MAX_TACTICAL_CALL+1);
  }

  if (tactical_pointer->tactical_call_sign != NULL)
  {

    // Check for blank tactical call.  If so, free the space.
    if (temp[0] == '\0')
    {
      free(tactical_pointer->tactical_call_sign);
      tactical_pointer->tactical_call_sign = NULL;
    }
    else
    {
      xastir_snprintf(tactical_pointer->tactical_call_sign,
                      MAX_TACTICAL_CALL+1,
                      "%s",
                      temp);
    }

    fprintf(stderr,
            "Assigned tactical call \"%s\" to %s\n",
            temp,
            tactical_pointer->call_sign);

    // Log the change in the tactical_calls.log file.
    // Also adds it to the tactical callsign hash here.
    log_tactical_call(tactical_pointer->call_sign,
                      tactical_pointer->tactical_call_sign);
  }
  else
  {
    fprintf(stderr,
            "Couldn't malloc space for tactical callsign\n");
  }

  XtFree(temp);

  redraw_on_new_data = 2;  // redraw now

  Change_tactical_destroy_shell(widget,clientData,callData);
}





void Change_tactical(Widget UNUSED(w), XtPointer UNUSED(clientData), XtPointer UNUSED(callData) )
{
  static Widget pane, my_form, button_ok, button_close, label, scrollwindow;
  Atom delw;
  Arg al[50];                     // Arg List
  register unsigned int ac = 0;   // Arg Count

  if (!change_tactical_dialog)
  {
    change_tactical_dialog =
      XtVaCreatePopupShell(langcode("WPUPSTI065"),
                           xmDialogShellWidgetClass,
                           appshell,
                           XmNdeleteResponse,XmDESTROY,
                           XmNdefaultPosition, FALSE,
                           XmNfontList, fontlist1,
                           NULL);

    pane = XtVaCreateWidget("Change Tactical pane",
                            xmPanedWindowWidgetClass,
                            change_tactical_dialog,
                            MY_FOREGROUND_COLOR,
                            MY_BACKGROUND_COLOR,
                            NULL);

    scrollwindow = XtVaCreateManagedWidget("Change Tactical scrollwindow",
                                           xmScrolledWindowWidgetClass,
                                           pane,
                                           XmNscrollingPolicy, XmAUTOMATIC,
                                           NULL);

    my_form =  XtVaCreateWidget("Change Tactical my_form",
                                xmFormWidgetClass,
                                scrollwindow,
                                XmNfractionBase, 3,
                                XmNautoUnmanage, FALSE,
                                XmNshadowThickness, 1,
                                MY_FOREGROUND_COLOR,
                                MY_BACKGROUND_COLOR,
                                NULL);


    // set args for color
    ac=0;
    XtSetArg(al[ac], XmNforeground, MY_FG_COLOR);
    ac++;
    XtSetArg(al[ac], XmNbackground, MY_BG_COLOR);
    ac++;
    XtSetArg(al[ac], XmNfontList, fontlist1);
    ac++;

    // Display the callsign or object/item name we're working on
    // in a label at the top of the dialog.  Otherwise we don't
    // know what station we're operating on.
    //
    label = XtVaCreateManagedWidget(tactical_pointer->call_sign,
                                    xmLabelWidgetClass,
                                    my_form,
                                    XmNtopAttachment, XmATTACH_FORM,
                                    XmNtopOffset, 10,
                                    XmNbottomAttachment, XmATTACH_NONE,
                                    XmNleftAttachment, XmATTACH_FORM,
                                    XmNleftOffset, 5,
                                    XmNrightAttachment, XmATTACH_NONE,
                                    MY_FOREGROUND_COLOR,
                                    MY_BACKGROUND_COLOR,
                                    XmNfontList, fontlist1,
                                    NULL);

    tactical_text = XtVaCreateManagedWidget("Change_Tactical text",
                                            xmTextWidgetClass,
                                            my_form,
                                            XmNeditable,   TRUE,
                                            XmNcursorPositionVisible, TRUE,
                                            XmNsensitive, TRUE,
                                            XmNshadowThickness,    1,
                                            XmNcolumns, MAX_TACTICAL_CALL,
                                            XmNwidth, ((MAX_TACTICAL_CALL*7)+2),
                                            XmNmaxLength, MAX_TACTICAL_CALL,
                                            XmNbackground, colors[0x0f],
                                            XmNtopOffset, 5,
                                            XmNtopAttachment,XmATTACH_WIDGET,
                                            XmNtopWidget, label,
                                            XmNbottomAttachment,XmATTACH_NONE,
                                            XmNleftAttachment, XmATTACH_FORM,
                                            XmNrightAttachment,XmATTACH_NONE,
                                            XmNnavigationType, XmTAB_GROUP,
                                            XmNfontList, fontlist1,
                                            NULL);

    // Fill in the current value of tactical callsign
    XmTextSetString(tactical_text, tactical_pointer->tactical_call_sign);

    button_ok = XtVaCreateManagedWidget(langcode("UNIOP00001"),
                                        xmPushButtonGadgetClass,
                                        my_form,
                                        XmNtopAttachment, XmATTACH_WIDGET,
                                        XmNtopWidget, tactical_text,
                                        XmNtopOffset, 5,
                                        XmNbottomAttachment, XmATTACH_FORM,
                                        XmNbottomOffset, 5,
                                        XmNleftAttachment, XmATTACH_POSITION,
                                        XmNleftPosition, 0,
                                        XmNrightAttachment, XmATTACH_POSITION,
                                        XmNrightPosition, 1,
                                        XmNnavigationType, XmTAB_GROUP,
                                        MY_FOREGROUND_COLOR,
                                        MY_BACKGROUND_COLOR,
                                        XmNfontList, fontlist1,
                                        NULL);


    button_close = XtVaCreateManagedWidget(langcode("UNIOP00003"),
                                           xmPushButtonGadgetClass,
                                           my_form,
                                           XmNtopAttachment, XmATTACH_WIDGET,
                                           XmNtopWidget, tactical_text,
                                           XmNtopOffset, 5,
                                           XmNbottomAttachment, XmATTACH_FORM,
                                           XmNbottomOffset, 5,
                                           XmNleftAttachment, XmATTACH_POSITION,
                                           XmNleftPosition, 2,
                                           XmNrightAttachment, XmATTACH_POSITION,
                                           XmNrightPosition, 3,
                                           XmNnavigationType, XmTAB_GROUP,
                                           MY_FOREGROUND_COLOR,
                                           MY_BACKGROUND_COLOR,
                                           XmNfontList, fontlist1,
                                           NULL);

    XtAddCallback(button_ok,
                  XmNactivateCallback,
                  Change_tactical_change_data,
                  change_tactical_dialog);
    XtAddCallback(button_close,
                  XmNactivateCallback,
                  Change_tactical_destroy_shell,
                  change_tactical_dialog);

    pos_dialog(change_tactical_dialog);

    delw = XmInternAtom(XtDisplay(change_tactical_dialog),"WM_DELETE_WINDOW", FALSE);
    XmAddWMProtocolCallback(change_tactical_dialog, delw, Change_tactical_destroy_shell, (XtPointer)change_tactical_dialog);

    XtManageChild(my_form);
    XtManageChild(pane);

    resize_dialog(my_form, change_tactical_dialog);

    XtPopup(change_tactical_dialog,XtGrabNone);

    // Move focus to the Close button.  This appears to
    // highlight the
    // button fine, but we're not able to hit the <Enter> key to
    // have that default function happen.  Note:  We _can_ hit
    // the
    // <SPACE> key, and that activates the option.
    //        XmUpdateDisplay(change_tactical_dialog);
    XmProcessTraversal(button_close, XmTRAVERSE_CURRENT);

  }
  else
  {
    (void)XRaiseWindow(XtDisplay(change_tactical_dialog),
                       XtWindow(change_tactical_dialog));
  }
}



/*
 *  Assign a tactical call to a station
 */
void Assign_Tactical_Call( Widget w, XtPointer clientData, XtPointer UNUSED(calldata) )
{
  DataRow *p_station = clientData;

  //fprintf(stderr,"Object Name: %s\n", p_station->call_sign);
  tactical_pointer = p_station;
  Change_tactical(w, p_station, NULL);
}


// ========================================================================
// MAIN STATION DATA DIALOG
// ========================================================================

/*
 *  List station info and trail
 *  If calldata is non-NULL, then we drop straight through to the
 *  Modify->Object or Assign_Tactical_Call dialogs.
 *
 * Input parameters:
 *     clientData:  Station callsign
 *
 *     calldata: NULL = Station Info
 *               "1"  = Object -> Modify
 *               "2"  = Move Object
 *               "3"  = Assign Tactical Call
 *               "4"  = Send Message To
 *
 */
void Station_data(Widget w, XtPointer clientData, XtPointer calldata)
{
  DataRow *p_station;
  char *station = (char *) clientData;
  static char local_station[25];
  char temp[300];
  unsigned int n;
  Atom delw;
  static Widget  pane, form, button_cancel, button_message,
         button_nws, button_fcc, button_rac, button_clear_track,
         button_trace, button_messages, button_object_modify,
         button_direct, button_version, station_icon, station_call,
         station_type, station_data_auto_update_w,
         button_tactical, button_change_trail_color,
         button_track_station,button_clear_df,scrollwindow;
  Arg args[50];
  Pixmap icon;
  Position x,y;    // For saving current dialog position


  //fprintf(stderr,"db.c:Station_data start\n");

  busy_cursor(appshell);

  db_station_info_callsign = (char *) clientData; // Used for auto-updating this dialog


  // Make a copy of the name.
  xastir_snprintf(local_station,sizeof(local_station),"%s",station);

  if (search_station_name(&p_station,station,1)   // find call
      && (p_station->flag & ST_ACTIVE) != 0)      // ignore deleted objects
  {
  }
  else
  {
    fprintf(stderr,"Couldn't find station in database\n");
    return; // Don't update current/create new dialog
  }


  if (calldata != NULL)   // We were called from the
  {
    // Object->Modify, Assign Tactical Call,
    // or Send Message To menu items.
    if (strncmp(calldata,"1",1) == 0)
    {
      Modify_object(w, (XtPointer)p_station, calldata);
    }
    else if (strncmp(calldata,"2",1) == 0)
    {
      Modify_object(w, (XtPointer)p_station, calldata);
    }
    else if (strncmp(calldata,"3",1) == 0)
    {
      Assign_Tactical_Call(w, (XtPointer)p_station, calldata);
    }
    else if (strncmp(calldata,"4",1) == 0)
    {
      //fprintf(stderr,"Send Message To: %s\n", p_station->call_sign);
      Send_message_call(NULL, (XtPointer) p_station->call_sign, NULL);
    }
    return;
  }


  // If we haven't calculated our decoration offsets yet, do so now
  if ( (decoration_offset_x == 0) && (decoration_offset_y == 0) )
  {
    compute_decorations();
  }

  if (db_station_info != NULL)    // We already have a dialog
  {

    // This is a pain.  We can get the X/Y position, but when
    // we restore the new dialog to the same position we're
    // off by the width/height of our window decorations.  Call
    // above was added to pre-compute the offsets that we'll need.
    XtVaGetValues(db_station_info, XmNx, &x, XmNy, &y, NULL);

    // This call doesn't work.  It returns the widget location,
    // just like the XtVaGetValues call does.  I need the window
    // decoration location instead.
    //XtTranslateCoords(db_station_info, 0, 0, &xnew, &ynew);
    //fprintf(stderr,"%d:%d\t%d:%d\n", x, xnew, y, ynew);

    if (last_station_info_x == 0)
    {
      last_station_info_x = x - decoration_offset_x;
    }

    if (last_station_info_y == 0)
    {
      last_station_info_y = y - decoration_offset_y;
    }

    // Now get rid of the old dialog
    Station_data_destroy_shell(db_station_info, db_station_info, NULL);
  }
  else
  {
    // Clear the global state variables
    fcc_lookup_pushed = 0;
    rac_lookup_pushed = 0;
  }


  begin_critical_section(&db_station_info_lock, "db.c:Station_data" );


  if (db_station_info == NULL)
  {
    // Start building the dialog from the bottom up.  That way
    // we can keep the buttons attached to the bottom of the
    // form and the correct height, and let the text widget
    // grow/shrink as the dialog is resized.

    db_station_info = XtVaCreatePopupShell(langcode("WPUPSTI001"),
                                           xmDialogShellWidgetClass, appshell,
                                           XmNdeleteResponse, XmDESTROY,
                                           XmNdefaultPosition, FALSE,
                                           XmNfontList, fontlist1,
                                           NULL);

    pane = XtVaCreateWidget("Station Data pane",
                            xmPanedWindowWidgetClass, db_station_info,
                            XmNbackground, colors[0xff],
                            NULL);

    scrollwindow = XtVaCreateManagedWidget("State Data scrollwindow",
                                           xmScrolledWindowWidgetClass,
                                           pane,
                                           XmNscrollingPolicy, XmAUTOMATIC,
                                           NULL);

    form =  XtVaCreateWidget("Station Data form",
                             xmFormWidgetClass,
                             scrollwindow,
                             XmNfractionBase, 4,
                             XmNbackground, colors[0xff],
                             XmNautoUnmanage, FALSE,
                             XmNshadowThickness, 1,
                             NULL);


    // Start with the bottom row, left button


    button_clear_track = NULL;  // Need this later, don't delete!
    if (p_station->newest_trackpoint != NULL)
    {
      // [ Clear Track ]
      button_clear_track = XtVaCreateManagedWidget(langcode("WPUPSTI045"),xmPushButtonGadgetClass, form,
                           XmNtopAttachment, XmATTACH_NONE,
                           XmNbottomAttachment, XmATTACH_FORM,
                           XmNbottomOffset,5,
                           XmNleftAttachment, XmATTACH_FORM,
                           XmNleftOffset,5,
                           XmNrightAttachment, XmATTACH_POSITION,
                           XmNrightPosition, 1,
                           XmNbackground, colors[0xff],
                           XmNnavigationType, XmTAB_GROUP,
                           XmNfontList, fontlist1,
                           NULL);
      XtAddCallback(button_clear_track, XmNactivateCallback, Station_data_destroy_track,(XtPointer)p_station);

    }
    else
    {
      // DK7IN: I drop the version button for mobile stations
      // we just have too much buttons...
      // and should find another solution
      // [ Station Version Query ]
      button_version = XtVaCreateManagedWidget(langcode("WPUPSTI052"),xmPushButtonGadgetClass, form,
                       XmNtopAttachment, XmATTACH_NONE,
                       XmNbottomAttachment, XmATTACH_FORM,
                       XmNbottomOffset,5,
                       XmNleftAttachment, XmATTACH_FORM,
                       XmNleftOffset,5,
                       XmNrightAttachment, XmATTACH_POSITION,
                       XmNrightPosition, 1,
                       XmNbackground, colors[0xff],
                       XmNnavigationType, XmTAB_GROUP,
                       XmNfontList, fontlist1,
                       NULL);
      XtAddCallback(button_version, XmNactivateCallback, Station_query_version,(XtPointer)p_station->call_sign);
    }

    // [ Trace Query ]
    button_trace = XtVaCreateManagedWidget(langcode("WPUPSTI049"),xmPushButtonGadgetClass, form,
                                           XmNtopAttachment, XmATTACH_NONE,
                                           XmNbottomAttachment, XmATTACH_FORM,
                                           XmNbottomOffset,5,
                                           XmNleftAttachment, XmATTACH_POSITION,
                                           XmNleftPosition, 1,
                                           XmNrightAttachment, XmATTACH_POSITION,
                                           XmNrightPosition, 2,
                                           XmNbackground, colors[0xff],
                                           XmNnavigationType, XmTAB_GROUP,
                                           XmNfontList, fontlist1,
                                           NULL);
    XtAddCallback(button_trace, XmNactivateCallback, Station_query_trace,(XtPointer)p_station->call_sign);

    // [ Un-Acked Messages Query ]
    button_messages = XtVaCreateManagedWidget(langcode("WPUPSTI050"),xmPushButtonGadgetClass, form,
                      XmNtopAttachment, XmATTACH_NONE,
                      XmNbottomAttachment, XmATTACH_FORM,
                      XmNbottomOffset,5,
                      XmNleftAttachment, XmATTACH_POSITION,
                      XmNleftPosition, 2,
                      XmNrightAttachment, XmATTACH_POSITION,
                      XmNrightPosition, 3,
                      XmNbackground, colors[0xff],
                      XmNnavigationType, XmTAB_GROUP,
                      XmNfontList, fontlist1,
                      NULL);
    XtAddCallback(button_messages, XmNactivateCallback, Station_query_messages,(XtPointer)p_station->call_sign);

    // [ Direct Stations Query ]
    button_direct = XtVaCreateManagedWidget(langcode("WPUPSTI051"),xmPushButtonGadgetClass, form,
                                            XmNtopAttachment, XmATTACH_NONE,
                                            XmNbottomAttachment, XmATTACH_FORM,
                                            XmNbottomOffset,5,
                                            XmNleftAttachment, XmATTACH_POSITION,
                                            XmNleftPosition, 3,
                                            XmNrightAttachment, XmATTACH_POSITION,
                                            XmNrightPosition, 4,
                                            XmNbackground, colors[0xff],
                                            XmNnavigationType, XmTAB_GROUP,
                                            XmNfontList, fontlist1,
                                            NULL);
    XtAddCallback(button_direct, XmNactivateCallback, Station_query_direct,(XtPointer)p_station->call_sign);


    // Now proceed to the row above it, left button first


    // [ Store Track ] or single Position
    button_store_track = XtVaCreateManagedWidget(langcode("WPUPSTI054"),xmPushButtonGadgetClass, form,
                         XmNtopAttachment, XmATTACH_NONE,
                         //XmNtopWidget,XtParent(si_text),
                         XmNbottomAttachment, XmATTACH_WIDGET,
                         XmNbottomWidget, (button_clear_track) ? button_clear_track : button_version,
                         XmNbottomOffset, 1,
                         XmNleftAttachment, XmATTACH_FORM,
                         XmNleftOffset,5,
                         XmNrightAttachment, XmATTACH_POSITION,
                         XmNrightPosition, 1,
                         XmNbackground, colors[0xff],
                         XmNnavigationType, XmTAB_GROUP,
                         XmNfontList, fontlist1,
                         NULL);
    XtAddCallback(button_store_track,   XmNactivateCallback, Station_data_store_track,(XtPointer)p_station);

    if ( ((p_station->flag & ST_OBJECT) == 0) && ((p_station->flag & ST_ITEM) == 0) )   // Not an object/
    {
      // fprintf(stderr,"Not an object or item...\n");
      // [Send Message]
      button_message = XtVaCreateManagedWidget(langcode("WPUPSTI002"),xmPushButtonGadgetClass, form,
                       XmNtopAttachment, XmATTACH_NONE,
                       XmNbottomAttachment, XmATTACH_WIDGET,
                       XmNbottomWidget, button_trace,
                       XmNbottomOffset, 1,
                       XmNleftAttachment, XmATTACH_POSITION,
                       XmNleftPosition, 1,
                       XmNrightAttachment, XmATTACH_POSITION,
                       XmNrightPosition, 2,
                       XmNbackground, colors[0xff],
                       XmNnavigationType, XmTAB_GROUP,
                       XmNfontList, fontlist1,
                       NULL);
      XtAddCallback(button_message, XmNactivateCallback, Send_message_call,(XtPointer)p_station->call_sign);
    }
    else
    {
      // fprintf(stderr,"Found an object or item...\n");
      button_object_modify = XtVaCreateManagedWidget(langcode("WPUPSTI053"),xmPushButtonGadgetClass, form,
                             XmNtopAttachment, XmATTACH_NONE,
                             XmNbottomAttachment, XmATTACH_WIDGET,
                             XmNbottomWidget, button_trace,
                             XmNbottomOffset, 1,
                             XmNleftAttachment, XmATTACH_POSITION,
                             XmNleftPosition, 1,
                             XmNrightAttachment, XmATTACH_POSITION,
                             XmNrightPosition, 2,
                             XmNbackground, colors[0xff],
                             XmNnavigationType, XmTAB_GROUP,
                             XmNfontList, fontlist1,
                             NULL);
      XtAddCallback(button_object_modify,
                    XmNactivateCallback,
                    Modify_object,
                    (XtPointer)p_station);
    }


    // Check whether it is a non-weather alert object/item.  If
    // so, try to use the origin callsign instead of the object
    // for FCC/RAC lookups.
    //
    if ( (p_station->flag & ST_OBJECT) || (p_station->flag & ST_ITEM) )
    {

      // It turns out that objects transmitted by a station
      // called "WINLINK" are what mess up the RAC button for
      // Canadian stations.  Xastir sees the 'W' of WINLINK
      // (the originating station) and assumes it is a U.S.
      // station.  Here's a sample packet:
      //
      // WINLINK>APWL2K,TCPIP*,qAC,T2MIDWEST:;VE7SEP-10*240521z4826.2 NW12322.5 Wa145.690MHz 1200b R11m RMSPacket EMCOMM
      //
      // If match on "WINLINK":  Don't copy origin callsign
      // into local_station.  Use the object name instead
      // which should be a callsign.
      if (strncmp(p_station->origin,"WINLINK",7))
      {
        xastir_snprintf(local_station,sizeof(local_station),"%s",p_station->origin);
      }
    }


    // Add "Fetch NWS Info" button if it is an object or item
    // and has "WXSVR" in its path somewhere.
    //
    // Note from Dale Huguley:
    //   "I would say an object with 6 upper alpha chars for the
    //   "from" call and " {AAAAA" (space curly 5 alphanumerics)
    //   at the end is almost guaranteed to be from Wxsvr.
    //   Fingering for the six alphas and the first three
    //   characters after the curly brace should be a reliable
    //   finger - as in SEWSVR>APRS::a_bunch_of_info_in_here_
    //   {H45AA finger SEWSVRH45@wxsvr.net"
    //
    // Note from Curt:  I had to remove the space from the
    // search as well, 'cuz the multipoint objects don't have
    // the space before the final curly-brace.
    //
    if ( ( (p_station->flag & ST_OBJECT) || (p_station->flag & ST_ITEM) )
         && (p_station->comment_data != NULL)
         && ( strstr(p_station->comment_data->text_ptr, "{") != NULL ) )
    {

      static char temp[25];
      char *ptr3;


      button_nws = XtVaCreateManagedWidget(langcode("WPUPSTI064"),xmPushButtonGadgetClass, form,
                                           XmNtopAttachment, XmATTACH_NONE,
                                           XmNbottomAttachment, XmATTACH_WIDGET,
                                           XmNbottomWidget, button_messages,
                                           XmNbottomOffset, 1,
                                           XmNleftAttachment, XmATTACH_POSITION,
                                           XmNleftPosition, 2,
                                           XmNrightAttachment, XmATTACH_POSITION,
                                           XmNrightPosition, 3,
                                           XmNbackground, colors[0xff],
                                           XmNnavigationType, XmTAB_GROUP,
                                           XmNfontList, fontlist1,
                                           NULL);

      // We need to construct the "special" finger address.
      // We'll use the FROM callsign and the first three chars
      // of the curly-brace portion of the comment field.
      // Callsign in this case is from the "origin" field.
      // The curly-brace text is at the end of one of the
      // "comment_data" records, hopefully the first one
      // checked (most recent).
      //

      xastir_snprintf(temp,
                      sizeof(temp),
                      "%s",
                      p_station->origin);
      temp[6] = '\0';
      ptr3 = strstr(p_station->comment_data->text_ptr,"{");
      ptr3++; // Skip over the '{' character
      strncat(temp,ptr3,3);

      //fprintf(stderr,"New Handle: %s\n", temp);

      XtAddCallback(button_nws,
                    XmNactivateCallback,
                    Station_data_wx_alert,
                    (XtPointer)temp);
    }


    // Add FCC button only if probable match.  The U.S. has
    // these prefixes assigned but not all are used for amateur
    // stations:
    //
    //   AAA-ALZ
    //   KAA-KZZ
    //   NAA-NZZ
    //   WAA-WZZ
    //
    else if ((! strncmp(local_station,"A",1)) || (!  strncmp(local_station,"K",1)) ||
             (! strncmp(local_station,"N",1)) || (! strncmp(local_station,"W",1))  )
    {

      button_fcc = XtVaCreateManagedWidget(langcode("WPUPSTI003"),xmPushButtonGadgetClass, form,
                                           XmNtopAttachment, XmATTACH_NONE,
                                           XmNbottomAttachment, XmATTACH_WIDGET,
                                           XmNbottomWidget, button_messages,
                                           XmNbottomOffset, 1,
                                           XmNleftAttachment, XmATTACH_POSITION,
                                           XmNleftPosition, 2,
                                           XmNrightAttachment, XmATTACH_POSITION,
                                           XmNrightPosition, 3,
                                           XmNbackground, colors[0xff],
                                           XmNnavigationType, XmTAB_GROUP,
                                           XmNfontList, fontlist1,
                                           NULL);
      XtAddCallback(button_fcc,
                    XmNactivateCallback,
                    Station_data_add_fcc,
                    (XtPointer)local_station);

      if ( ! check_fcc_data())
      {
        XtSetSensitive(button_fcc,FALSE);
      }
    }


    // Add RAC button only if probable match.  Canada has these
    // prefixes assigned but not all are used for amateur
    // stations:
    //
    //   CFA-CKZ
    //   CYA-CZZ
    //   VAA-VGZ
    //   VOA-VOZ
    //   VXA-VYZ
    //   XJA-XOZ
    //
    else if (!strncmp(local_station,"VA",2) || !strncmp(local_station,"VE",2) || !strncmp(local_station,"VO",2) || !strncmp(local_station,"VY",2))
    {
      button_rac = XtVaCreateManagedWidget(langcode("WPUPSTI004"),xmPushButtonGadgetClass, form,
                                           XmNtopAttachment, XmATTACH_NONE,
                                           XmNbottomAttachment, XmATTACH_WIDGET,
                                           XmNbottomWidget, button_messages,
                                           XmNbottomOffset, 1,
                                           XmNleftAttachment, XmATTACH_POSITION,
                                           XmNleftPosition, 2,
                                           XmNrightAttachment, XmATTACH_POSITION,
                                           XmNrightPosition, 3,
                                           XmNbackground, colors[0xff],
                                           XmNnavigationType, XmTAB_GROUP,
                                           XmNfontList, fontlist1,
                                           NULL);
      XtAddCallback(button_rac,
                    XmNactivateCallback,
                    Station_data_add_rac,
                    (XtPointer)local_station);

      if ( ! check_rac_data())
      {
        XtSetSensitive(button_rac,FALSE);
      }
    }

    button_cancel = XtVaCreateManagedWidget(langcode("UNIOP00003"),xmPushButtonGadgetClass, form,
                                            XmNtopAttachment, XmATTACH_NONE,
                                            XmNbottomAttachment, XmATTACH_WIDGET,
                                            XmNbottomWidget, button_direct,
                                            XmNbottomOffset, 1,
                                            XmNleftAttachment, XmATTACH_POSITION,
                                            XmNleftPosition, 3,
                                            XmNrightAttachment, XmATTACH_POSITION,
                                            XmNrightPosition, 4,
                                            XmNrightOffset, 5,
                                            XmNbackground, colors[0xff],
                                            XmNnavigationType, XmTAB_GROUP,
                                            XmNfontList, fontlist1,
                                            NULL);
    XtAddCallback(button_cancel, XmNactivateCallback, Station_data_destroy_shell, db_station_info);

    // Button to clear DF bearing data if we actually have some.
    if (strlen(p_station->bearing) == 3)
    {
      button_clear_df = XtVaCreateManagedWidget(langcode("WPUPSTI092"),xmPushButtonGadgetClass, form,
                        XmNtopAttachment, XmATTACH_NONE,
                        XmNbottomAttachment, XmATTACH_WIDGET,
                        XmNbottomWidget, button_cancel,
                        XmNbottomOffset, 1,
                        XmNleftAttachment, XmATTACH_POSITION,
                        XmNleftPosition, 3,
                        XmNrightAttachment, XmATTACH_POSITION,
                        XmNrightPosition, 4,
                        XmNrightOffset, 5,
                        XmNbackground, colors[0xff],
                        XmNnavigationType, XmTAB_GROUP,
                        XmNfontList, fontlist1,
                        NULL);
      XtAddCallback(button_clear_df, XmNactivateCallback,Clear_DF_from_Station_data, (XtPointer)p_station);
    }

    button_track_station = XtVaCreateManagedWidget(langcode("WPUPTSP001"),xmPushButtonGadgetClass, form,
                           XmNtopAttachment, XmATTACH_NONE,
                           XmNbottomAttachment, XmATTACH_WIDGET,
                           XmNbottomWidget, button_store_track,
                           XmNbottomOffset, 1,
                           XmNleftAttachment, XmATTACH_POSITION,
                           XmNleftPosition, 0,
                           XmNleftOffset,5,
                           XmNrightAttachment, XmATTACH_POSITION,
                           XmNrightPosition, 1,
                           //                            XmNrightOffset, 5,
                           XmNbackground, colors[0xff],
                           XmNnavigationType, XmTAB_GROUP,
                           XmNfontList, fontlist1,
                           NULL);
    XtAddCallback(button_track_station, XmNactivateCallback,Track_from_Station_data, (XtPointer)p_station);



    // Now build from the top of the dialog to the buttons.


    icon = XCreatePixmap(XtDisplay(appshell),RootWindowOfScreen(XtScreen(appshell)),
                         20,20,DefaultDepthOfScreen(XtScreen(appshell)));

    symbol(db_station_info,0,p_station->aprs_symbol.aprs_type,
           p_station->aprs_symbol.aprs_symbol,
           p_station->aprs_symbol.special_overlay,icon,0,0,0,' ');

    station_icon = XtVaCreateManagedWidget("Station Data icon", xmLabelWidgetClass, form,
                                           XmNtopAttachment, XmATTACH_FORM,
                                           XmNtopOffset, 2,
                                           XmNbottomAttachment, XmATTACH_NONE,
                                           XmNleftAttachment, XmATTACH_FORM,
                                           XmNleftOffset, 5,
                                           XmNrightAttachment, XmATTACH_NONE,
                                           XmNlabelType, XmPIXMAP,
                                           XmNlabelPixmap,icon,
                                           XmNbackground, colors[0xff],
                                           XmNfontList, fontlist1,
                                           NULL);

    station_type = XtVaCreateManagedWidget("Station Data type", xmTextFieldWidgetClass, form,
                                           XmNeditable,   FALSE,
                                           XmNcursorPositionVisible, FALSE,
                                           XmNtraversalOn, FALSE,
                                           XmNshadowThickness,       0,
                                           XmNcolumns,5,
                                           XmNwidth,((5*7)+2),
                                           XmNbackground, colors[0xff],
                                           XmNtopAttachment,XmATTACH_FORM,
                                           XmNtopOffset, 2,
                                           XmNbottomAttachment,XmATTACH_NONE,
                                           XmNleftAttachment, XmATTACH_WIDGET,
                                           XmNleftWidget,station_icon,
                                           XmNleftOffset,10,
                                           XmNrightAttachment,XmATTACH_NONE,
                                           XmNfontList, fontlist1,
                                           NULL);

    xastir_snprintf(temp, sizeof(temp), "%c%c%c", p_station->aprs_symbol.aprs_type,
                    p_station->aprs_symbol.aprs_symbol,
                    p_station->aprs_symbol.special_overlay);

    XmTextFieldSetString(station_type, temp);
    XtManageChild(station_type);

    station_call = XtVaCreateManagedWidget("Station Data call", xmTextFieldWidgetClass, form,
                                           XmNeditable,   FALSE,
                                           XmNcursorPositionVisible, FALSE,
                                           XmNtraversalOn, FALSE,
                                           XmNshadowThickness,       0,
                                           XmNcolumns,10,
                                           XmNwidth,((10*7)+2),
                                           XmNbackground, colors[0xff],
                                           XmNtopAttachment,XmATTACH_FORM,
                                           XmNtopOffset, 2,
                                           XmNbottomAttachment,XmATTACH_NONE,
                                           XmNleftAttachment, XmATTACH_WIDGET,
                                           XmNleftWidget, station_type,
                                           XmNleftOffset,10,
                                           XmNrightAttachment,XmATTACH_NONE,
                                           XmNfontList, fontlist1,
                                           NULL);

    XmTextFieldSetString(station_call,p_station->call_sign);
    XtManageChild(station_call);

    station_data_auto_update_w = XtVaCreateManagedWidget(langcode("WPUPSTI056"),
                                 xmToggleButtonGadgetClass, form,
                                 XmNtopAttachment,XmATTACH_FORM,
                                 XmNtopOffset, 2,
                                 XmNbottomAttachment,XmATTACH_NONE,
                                 XmNleftAttachment, XmATTACH_WIDGET,
                                 XmNleftWidget, station_call,
                                 XmNleftOffset,10,
                                 XmNrightAttachment,XmATTACH_NONE,
                                 XmNbackground,colors[0xff],
                                 XmNfontList, fontlist1,
                                 NULL);
    XtAddCallback(station_data_auto_update_w,XmNvalueChangedCallback,station_data_auto_update_toggle,"1");

    //Add tactical button at the top/right
    // "Assign Tactical Call"
    button_tactical = XtVaCreateManagedWidget(langcode("WPUPSTI066"),xmPushButtonGadgetClass, form,
                      XmNtopAttachment, XmATTACH_FORM,
                      XmNtopOffset, 5,
                      XmNbottomAttachment, XmATTACH_NONE,
                      XmNleftAttachment, XmATTACH_WIDGET,
                      XmNleftOffset, 10,
                      XmNleftWidget, station_data_auto_update_w,
                      XmNrightAttachment, XmATTACH_NONE,
                      XmNbackground, colors[0xff],
                      XmNnavigationType, XmTAB_GROUP,
                      XmNfontList, fontlist1,
                      NULL);
    XtAddCallback(button_tactical,
                  XmNactivateCallback,
                  Assign_Tactical_Call,
                  (XtPointer)p_station);
    if (p_station->flag & (ST_OBJECT | ST_ITEM))
    {
      // We don't allow setting tac-calls for objects/items,
      // so make the button insensitive.
      XtSetSensitive(button_tactical,FALSE);
    }

    //Add change_trail_color button at the top/right
    // "Trail Color"
    button_change_trail_color = XtVaCreateManagedWidget(langcode("WPUPSTI091"),
                                xmPushButtonGadgetClass, form,
                                XmNtopAttachment, XmATTACH_FORM,
                                XmNtopOffset, 5,
                                XmNbottomAttachment, XmATTACH_NONE,
                                XmNleftAttachment, XmATTACH_WIDGET,
                                XmNleftOffset, 10,
                                XmNleftWidget, button_tactical,
                                XmNrightAttachment, XmATTACH_NONE,
                                XmNbackground, colors[0xff],
                                XmNnavigationType, XmTAB_GROUP,
                                XmNfontList, fontlist1,
                                NULL);
    XtAddCallback(button_change_trail_color,
                  XmNactivateCallback,
                  Change_trail_color,
                  (XtPointer)p_station);

    n=0;
    XtSetArg(args[n], XmNrows, 15);
    n++;
    XtSetArg(args[n], XmNcolumns, 100);
    n++;
    XtSetArg(args[n], XmNeditable, FALSE);
    n++;
    XtSetArg(args[n], XmNtraversalOn, FALSE);
    n++;
    XtSetArg(args[n], XmNeditMode, XmMULTI_LINE_EDIT);
    n++;
    XtSetArg(args[n], XmNwordWrap, TRUE);
    n++;
    XtSetArg(args[n], XmNbackground, colors[0xff]);
    n++;
    XtSetArg(args[n], XmNscrollHorizontal, FALSE);
    n++;
    XtSetArg(args[n], XmNcursorPositionVisible, FALSE);
    n++;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET);
    n++;
    XtSetArg(args[n], XmNtopWidget, station_icon);
    n++;
    XtSetArg(args[n], XmNtopOffset, 5);
    n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET);
    n++;
    //        XtSetArg(args[n], XmNbottomWidget, button_store_track); n++;
    XtSetArg(args[n], XmNbottomWidget, button_track_station);
    n++;
    XtSetArg(args[n], XmNbottomOffset, 1);
    n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM);
    n++;
    XtSetArg(args[n], XmNleftOffset, 5);
    n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM);
    n++;
    XtSetArg(args[n], XmNrightOffset, 5);
    n++;
    XtSetArg(args[n], XmNfontList, fontlist1);
    n++;

    si_text = NULL;
    si_text = XmCreateScrolledText(form,"Station_data",args,n);

    end_critical_section(&db_station_info_lock, "db.c:Station_data" );

    // Fill in the si_text widget with real data
    station_data_fill_in( w, (XtPointer)db_station_info_callsign, NULL);

    begin_critical_section(&db_station_info_lock, "db.c:Station_data" );

    pos_dialog(db_station_info);

    delw = XmInternAtom(XtDisplay(db_station_info),"WM_DELETE_WINDOW", FALSE);
    XmAddWMProtocolCallback(db_station_info, delw, Station_data_destroy_shell, (XtPointer)db_station_info);

    XtManageChild(form);
    XtManageChild(si_text);
    XtVaSetValues(si_text, XmNbackground, colors[0x0f], NULL);
    XtManageChild(pane);

    resize_dialog(form, db_station_info);

    if (station_data_auto_update)
    {
      XmToggleButtonSetState(station_data_auto_update_w,TRUE,FALSE);
    }

    if (calldata == NULL)   // We're not going straight to the Modify dialog
    {
      // and will actually use the dialog we just drew

      XtPopup(db_station_info,XtGrabNone);

      XmTextShowPosition(si_text,0);

      // Move focus to the Cancel button.  This appears to highlight the
      // button fine, but we're not able to hit the <Enter> key to
      // have that default function happen.  Note:  We _can_ hit the
      // <SPACE> key, and that activates the option.
      XmProcessTraversal(button_cancel, XmTRAVERSE_CURRENT);
    }
  }

  end_critical_section(&db_station_info_lock, "db.c:Station_data" );

}


// ========================================================================
// STATION INFO DIALOG FUNCTIONS
// ========================================================================

/*
 *  Station Info Selection PopUp window: Canceled
 */
void Station_info_destroy_shell(Widget UNUSED(widget), XtPointer clientData, XtPointer UNUSED(callData) )
{
  Widget shell = (Widget) clientData;

  // We used to close the detailed Station Info dialog here too, which
  // makes no sense.  Commenting this out so that we can close the
  // Station Chooser but leave the Station Info dialog open.
  //
  //    if (db_station_info!=NULL)
  //        Station_data_destroy_shell(db_station_info, db_station_info, NULL);

  XtPopdown(shell);
  (void)XFreePixmap(XtDisplay(appshell),SiS_icon0);
  (void)XFreePixmap(XtDisplay(appshell),SiS_icon);

  begin_critical_section(&db_station_popup_lock, "db.c:Station_info_destroy_shell" );

  XtDestroyWidget(shell);
  db_station_popup = (Widget)NULL;

  end_critical_section(&db_station_popup_lock, "db.c:Station_info_destroy_shell" );

}




// Used for auto-refreshing the Station_info dialog.  Called from
// main.c:UpdateTime() every 30 seconds.
//
void update_station_info(Widget w)
{

  begin_critical_section(&db_station_info_lock, "db.c:update_station_info" );

  // If we have a dialog to update and a callsign to pass to it
  if (( db_station_info != NULL)
      && (db_station_info_callsign != NULL)
      && (strlen(db_station_info_callsign) != 0) )
  {

    end_critical_section(&db_station_info_lock, "db.c:update_station_info" );

    // Fill in the si_text widget with real data
    station_data_fill_in( w, (XtPointer)db_station_info_callsign, NULL);
  }
  else
  {

    end_critical_section(&db_station_info_lock, "db.c:update_station_info" );

  }
}


/*
 *  Station Info Selection PopUp window: Quit with selected station
 */
void Station_info_select_destroy_shell(Widget widget, XtPointer UNUSED(clientData), XtPointer UNUSED(callData) )
{
  int i,x;
  char *temp;
  char temp2[50];
  XmString *list;
  int found;
  //Widget shell = (Widget) clientData;

  found=0;

  begin_critical_section(&db_station_popup_lock, "db.c:Station_info_select_destroy_shell" );

  if (db_station_popup)
  {
    XtVaGetValues(station_list,XmNitemCount,&i,XmNitems,&list,NULL);

    for (x=1; x<=i; x++)
    {
      if (XmListPosSelected(station_list,x))
      {
        found=1;
        if (XmStringGetLtoR(list[(x-1)],XmFONTLIST_DEFAULT_TAG,&temp))
        {
          x=i+1;
        }
      }
    }

    // DK7IN ?? should we not first close the PopUp, then call Station_data ??
    if (found)
    {
      xastir_snprintf(temp2, sizeof(temp2), "%s", temp);
      // Only keep the station info, remove Tactical Call Sign
      temp2[strcspn(temp2, "(")] = '\0';
      remove_trailing_spaces(temp2);

      // Call it with the global parameter at the last, so we
      // can pass special parameters down that we couldn't
      // directly pass to Station_info_select_destroy_shell().
      Station_data(widget, temp2, station_info_select_global);

      // Clear the global variable so that nothing else calls
      // it with the wrong parameter
      station_info_select_global = NULL;

      XtFree(temp);
    }
    /*
    // This section of code gets rid of the Station Chooser.  Frank wanted to
    // be able to leave the Station Chooser up after selection so that other
    // stations could be selected, therefore I commented this out.
    XtPopdown(shell);                   // Get rid of the station chooser popup here
    (void)XFreePixmap(XtDisplay(appshell),SiS_icon0);
    (void)XFreePixmap(XtDisplay(appshell),SiS_icon);
    XtDestroyWidget(shell);             // and here
    db_station_popup = (Widget)NULL;    // and here
    */
  }

  end_critical_section(&db_station_popup_lock, "db.c:Station_info_select_destroy_shell" );

}





/*
 *  Station Info PopUp
 *  if only one station in view it shows the data with Station_data()
 *  otherwise we get a selection box
 *  clientData will be non-null if we wish to drop through to the object->modify
 *  or Assign Tactical Call dialogs.
 *
 * clientData: NULL = Station Info
 *             "1"  = Object -> Modify
 *             "2"  = Move Object
 *             "3"  = Assign Tactical Call
 *             "4"  = Send Message To
 */
void Station_info(Widget w, XtPointer clientData, XtPointer UNUSED(calldata) )
{
  DataRow *p_station;
  DataRow *p_found;
  int num_found = 0;
  unsigned long min_diff_x, diff_x, min_diff_y, diff_y;
  XmString str_ptr;
  unsigned int n;
  Atom delw;
  static Widget pane, form, button_ok, button_cancel;
  Arg al[50];                    /* Arg List */
  register unsigned int ac = 0;           /* Arg Count */
  char tactical_string[50];


  busy_cursor(appshell);

  min_diff_y = scale_y * 20;  // Pixels each way in y-direction.
  min_diff_x = scale_x * 20;  // Pixels each way in x-direction.
  p_found = NULL;
  p_station = n_first;

  // Here we just count them.  We go through the same type of code
  // again later if we find more than one station.
  while (p_station != NULL)      // search through database for nearby stations
  {

    if ( ( (p_station->flag & ST_INVIEW) != 0)
         && ok_to_draw_station(p_station) )   // only test stations in view
    {

      if (!altnet || is_altnet(p_station))
      {

        // Here we calculate diff in terms of XX pixels,
        // changed into lat/long values.  This keeps the
        // affected rectangle the same at any zoom level.
        // scale_y/scale_x is Xastir units/pixel.  Xastir
        // units are in 1/100 of a second.  If we want to go
        // 10 pixels in any direction (roughly, scale_x
        // varies by latitude), then we want (10 * scale_y),
        // and (10 * scale_x) if we want to make a very
        // accurate square.

        diff_y = (unsigned long)( labs((NW_corner_latitude+(menu_y*scale_y))
                                       - p_station->coord_lat));

        diff_x = (unsigned long)( labs((NW_corner_longitude+(menu_x*scale_x))
                                       - p_station->coord_lon));

        // If the station fits within our bounding box,
        // count it
        if ((diff_y < min_diff_y) && (diff_x < min_diff_x))
        {
          p_found = p_station;
          num_found++;
        }
      }
    }
    p_station = p_station->n_next;
  }

  if (p_found != NULL)    // We found at least one station
  {

    if (num_found == 1)
    {
      // We only found one station, so it's easy
      Station_data(w,p_found->call_sign,clientData);
    }
    else    // We found more: create dialog to choose a station
    {

      // Set up this global variable so that we can pass it
      // off to Station_data from the
      // Station_info_select_destroy_shell() function above.
      // Without this global variable we don't have enough
      // parameters passed to the final routine, so we can't
      // move an object that is on top of another.  With it,
      // we can.
      station_info_select_global = clientData;

      if (db_station_popup != NULL)
      {
        Station_info_destroy_shell(db_station_popup, db_station_popup, NULL);
      }

      begin_critical_section(&db_station_popup_lock, "db.c:Station_info" );

      if (db_station_popup == NULL)
      {
        // Set up a selection box:
        db_station_popup = XtVaCreatePopupShell(langcode("STCHO00001"),
                                                xmDialogShellWidgetClass, appshell,
                                                XmNdeleteResponse, XmDESTROY,
                                                XmNdefaultPosition, FALSE,
                                                XmNbackground, colors[0xff],
                                                XmNfontList, fontlist1,
                                                NULL);

        pane = XtVaCreateWidget("Station_info pane",xmPanedWindowWidgetClass, db_station_popup,
                                XmNbackground, colors[0xff],
                                NULL);

        form =  XtVaCreateWidget("Station_info form",xmFormWidgetClass, pane,
                                 XmNfractionBase, 5,
                                 XmNbackground, colors[0xff],
                                 XmNautoUnmanage, FALSE,
                                 XmNshadowThickness, 1,
                                 NULL);

        // Attach buttons first to the bottom of the form,
        // so that we'll be able to stretch this thing
        // vertically to see all the callsigns.
        //
        button_ok = XtVaCreateManagedWidget("Info",xmPushButtonGadgetClass, form,
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

        XtAddCallback(button_cancel, XmNactivateCallback, Station_info_destroy_shell, db_station_popup);
        XtAddCallback(button_ok, XmNactivateCallback, Station_info_select_destroy_shell, db_station_popup);


        /*set args for color */
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

        station_list = XmCreateScrolledList(form,"Station_info list",al,ac);

// DK7IN: I want to add the symbol in front of the call...
        // icon
        SiS_icon0 = XCreatePixmap(XtDisplay(appshell),RootWindowOfScreen(XtScreen(appshell)),20,20,
                                  DefaultDepthOfScreen(XtScreen(appshell)));
        SiS_icon  = XCreatePixmap(XtDisplay(appshell),RootWindowOfScreen(XtScreen(appshell)),20,20,
                                  DefaultDepthOfScreen(XtScreen(appshell)));
        /*      SiS_symb  = XtVaCreateManagedWidget("Station_info icon", xmLabelWidgetClass, ob_form1,
                                    XmNlabelType,               XmPIXMAP,
                                    XmNlabelPixmap,             SiS_icon,
                                    XmNbackground,              colors[0xff],
                                    XmNleftAttachment,          XmATTACH_FORM,
                                    XmNtopAttachment,           XmATTACH_FORM,
                                    XmNbottomAttachment,        XmATTACH_NONE,
                                    XmNrightAttachment,         XmATTACH_NONE,
                                    NULL);
        */

        /*fprintf(stderr,"What station\n");*/
        n = 1;
        p_station = n_first;
        while (p_station != NULL)      // search through database for nearby stations
        {

          if ( ( (p_station->flag & ST_INVIEW) != 0)
               && ok_to_draw_station(p_station) )   // only test stations in view
          {

            if (!altnet || is_altnet(p_station))
            {

              diff_y = (unsigned long)( labs((NW_corner_latitude+(menu_y*scale_y))
                                             - p_station->coord_lat));

              diff_x = (unsigned long)( labs((NW_corner_longitude+(menu_x*scale_x))
                                             - p_station->coord_lon));

              // If the station fits within our
              // bounding box, count it.
              if ((diff_y < min_diff_y) && (diff_x < min_diff_x))
              {
                /*fprintf(stderr,"Station %s\n",p_station->call_sign);*/
                if (p_station->tactical_call_sign)
                {
                  xastir_snprintf(tactical_string, sizeof(tactical_string), "%s (%s)", p_station->call_sign,
                                  p_station->tactical_call_sign);
                  XmListAddItem(station_list, str_ptr = XmStringCreateLtoR(tactical_string,
                                                        XmFONTLIST_DEFAULT_TAG), (int)n++);
                }
                else
                {
                  XmListAddItem(station_list, str_ptr = XmStringCreateLtoR(p_station->call_sign,
                                                        XmFONTLIST_DEFAULT_TAG), (int)n++);
                }
                XmStringFree(str_ptr);
              }
            }
          }
          p_station = p_station->n_next;
        }


        pos_dialog(db_station_popup);

        delw = XmInternAtom(XtDisplay(db_station_popup),"WM_DELETE_WINDOW", FALSE);
        XmAddWMProtocolCallback(db_station_popup, delw, Station_info_destroy_shell, (XtPointer)db_station_popup);

        XtManageChild(form);
        XtManageChild(station_list);
        XtVaSetValues(station_list, XmNbackground, colors[0x0f], NULL);
        XtManageChild(pane);

        XtPopup(db_station_popup,XtGrabNone);

        // Move focus to the Cancel button.  This appears to highlight the
        // button fine, but we're not able to hit the <Enter> key to
        // have that default function happen.  Note:  We _can_ hit the
        // <SPACE> key, and that activates the option.
        XmProcessTraversal(button_cancel, XmTRAVERSE_CURRENT);

      }

      end_critical_section(&db_station_popup_lock, "db.c:Station_info" );

    }
  }
}


// ========================================================================
// MAP POSITION AND LOCATION UTILITIES
// ========================================================================

/*
 *  Center map to new position
 */
void set_map_position(Widget UNUSED(w), long lat, long lon)
{
  // see also map_pos() in location.c

  // Set interrupt_drawing_now because conditions have changed
  // (new map center).
  interrupt_drawing_now++;

  set_last_position();
  center_latitude  = lat;
  center_longitude = lon;
  setup_in_view();  // flag all stations in new screen view

  // Request that a new image be created.  Calls create_image,
  // XCopyArea, and display_zoom_status.
  request_new_image++;

  //    if (create_image(w)) {
  //        (void)XCopyArea(XtDisplay(w),pixmap_final,XtWindow(w),gc,0,0,(unsigned int)screen_width,(unsigned int)screen_height,0,0);
  //    }
}

/*
 *  Check if position is inside inner screen area
 *  (real screen + minus 1/6 screen margin)
 *  used for station tracking
 */
int position_on_inner_screen(long lat, long lon)
{

  if (    lon > center_longitude-(long)(screen_width *scale_x/3)
          && lon < center_longitude+(long)(screen_width *scale_x/3)
          && lat > center_latitude -(long)(screen_height*scale_y/3)
          && lat < center_latitude +(long)(screen_height*scale_y/3)
          && !(lat == 0 && lon == 0))    // discard undef positions from screen
  {
    return(1);  // position is inside the area
  }
  else
  {
    return(0);
  }
}




/*
 *  Search for a station to be located (for Tracking and Find Station)
 */
int locate_station(Widget w, char *call, int follow_case, int get_match, int center_map)
{
  DataRow *p_station;
  char call_find[MAX_CALLSIGN+1];
  char call_find1[MAX_CALLSIGN+1];
  int ii;

  if (!follow_case)
  {
    for (ii=0; ii<(int)strlen(call); ii++)
    {
      if (isalpha((int)call[ii]))
      {
        call_find[ii] = toupper((int)call[ii]);  // Problem with lowercase objects/names!!
      }
      else
      {
        call_find[ii] = call[ii];
      }
    }
    call_find[ii] = '\0';
    xastir_snprintf(call_find1,
                    sizeof(call_find1),
                    "%s",
                    call_find);
  }
  else
    xastir_snprintf(call_find1,
                    sizeof(call_find1),
                    "%s",
                    call);

  if (search_station_name(&p_station,call_find1,get_match))
  {
    if (position_defined(p_station->coord_lat,p_station->coord_lon,0))
    {
      if (center_map || !position_on_inner_screen(p_station->coord_lat,p_station->coord_lon))
        // only change map if really necessary
      {
        set_map_position(w, p_station->coord_lat, p_station->coord_lon);
      }
      return(1);                  // we found it
    }
  }
  return(0);
}


