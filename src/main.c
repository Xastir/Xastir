/* -*- c-basic-indent: 4; indent-tabs-mode: nil -*-
 * $Id$
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2002  The Xastir Group
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
#include "snprintf.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <unistd.h>
#include <dirent.h>
#include <signal.h>
#include <termios.h>
#include <pwd.h>
#include <pthread.h>
#include <locale.h>
#include <strings.h>

#ifdef HAVE_IMAGEMAGICK
#include <time.h>
#include <sys/types.h>
#undef RETSIGTYPE
#include <magick/api.h>
#endif

#ifdef  HAVE_LIBINTL_H
#include <libintl.h>
#define _(x)        gettext(x)
#else
#define _(x)        (x)
#endif

//#ifdef HAVE_NETAX25_AXLIB_H
//#include <netax25/axlib.h>
//#endif

#include "xastir.h"
#include "draw_symbols.h"
#include "main.h"
#include "xa_config.h"
#include "maps.h"
#include "alert.h"
#include "db.h"
#include "interface.h"
#include "wx.h"
#include "popup.h"
#include "track_gui.h"
#include "list_gui.h"
#include "util.h"
#include "color.h"
#include "gps.h"
#include "bulletin_gui.h"
#include "rotated.h"
#include "datum.h"
#include "igate.h"


#include <Xm/XmAll.h>
#include <X11/cursorfont.h>

#define DOS_HDR_LINES 8

#define STATUSLINE_ACTIVE 10    /* status line is cleared after 10 seconds */
#define REPLAY_DELAY       0    /* delay between replayed packets in sec, 0 is ok */
#define REDRAW_WAIT        2    /* delay between consecutive redraws in seconds (file load) */


#define XmFONTLIST_DEFAULT_MY "fixed"

#define LINE_WIDTH 1

#define ARROWS     1            // Arrow buttons on menubar

/* JMT - works under FreeBSD */
uid_t euid;
gid_t egid;

FILE *file_wx_test;

int dtr_on = 1;

/* language in use */
char lang_to_use[30];

/* version info in main.h */
int  altnet;
char altnet_call[MAX_CALL];

void da_input(Widget w, XtPointer client_data, XtPointer call_data);
void da_resize(Widget w, XtPointer client_data, XtPointer call_data);
void da_expose(Widget w, XtPointer client_data, XtPointer call_data);

int debug_level;

//Widget hidden_shell = (Widget) NULL;
Widget appshell = (Widget) NULL;
Widget form = (Widget) NULL;
Widget da = (Widget) NULL;
Widget text;
Widget text2;
Widget text3;
Widget text4;
Widget iface_da;
Widget menubar;
Widget toolbar;

Widget configure_station_dialog     = (Widget)NULL;
Widget right_menu_popup              = (Widget)NULL;    // Button one or left mouse button
//Widget middle_menu_popup=(Widget)NULL;  // Button two or middle mouse button
//Widget right_menu_popup=(Widget)NULL;   // Button three or right mouse button
Widget trackme_button;
Widget measure_button;
Widget move_button;


int Station_transmit_type;
int Igate_type;

Widget Display_data_dialog  = (Widget)NULL;
Widget Display_data_text;
int Display_packet_data_type;

Widget configure_defaults_dialog = (Widget)NULL;
Widget configure_coordinates_dialog = (Widget)NULL;
Widget coordinate_calc_button_ok = (Widget)NULL;
Widget change_debug_level_dialog = (Widget)NULL;


Widget coordinate_calc_dialog = (Widget)NULL;
Widget coordinate_calc_zone = (Widget)NULL;
Widget coordinate_calc_latitude_easting = (Widget)NULL;
Widget coordinate_calc_longitude_northing = (Widget)NULL;
Widget coordinate_calc_result_text = (Widget)NULL;
static char coordinate_calc_lat_deg[5];
static char coordinate_calc_lat_min[15];
static char coordinate_calc_lat_dir[5];
static char coordinate_calc_lon_deg[5];
static char coordinate_calc_lon_min[15];
static char coordinate_calc_lon_dir[5];
static struct {
    Widget calling_dialog;  // NULL if the calling dialog has been closed.
    Widget input_lat_deg;   // Pointers to calling dialog's widgets
    Widget input_lat_min;   // (Where to get/put the data)
    Widget input_lat_dir;
    Widget input_lon_deg;
    Widget input_lon_min;
    Widget input_lon_dir;
} coordinate_calc_array;


int old_station_item;
int clear_station_item;

// --------------------------- help menu -----------------------------
Widget help_list;
Widget help_index_dialog = (Widget)NULL;
Widget help_view_dialog  = (Widget)NULL;
static void Help_About(Widget w, XtPointer clientData, XtPointer callData);
static void Help_Index(Widget w, XtPointer clientData, XtPointer callData);

// ----------------------------- map ---------------------------------
Widget map_list;

void map_chooser_fill_in (void);
 
Widget map_chooser_dialog = (Widget)NULL;
static void Map_chooser(Widget w, XtPointer clientData, XtPointer callData);

#ifdef HAVE_IMAGEMAGICK
static void Config_tiger(Widget w, XtPointer clientData, XtPointer callData);
#endif

Widget grid_on, grid_off;
static void Grid_toggle( Widget w, XtPointer clientData, XtPointer calldata);
int long_lat_grid;              // Switch for Map Lat and Long grid display

int disable_all_maps = 0;
static void Map_disable_toggle( Widget w, XtPointer clientData, XtPointer calldata);

static void Map_auto_toggle( Widget w, XtPointer clientData, XtPointer calldata);
int map_auto_maps;              /* toggle use of auto_maps */
static void Map_auto_skip_raster_toggle( Widget w, XtPointer clientData, XtPointer calldata);
int auto_maps_skip_raster;
Widget map_auto_skip_raster_button;

Widget map_levels_on, map_levels_off;
static void Map_levels_toggle( Widget w, XtPointer clientData, XtPointer calldata);
int map_color_levels;           /* toggle use of map_color_levels */

Widget map_labels_on, map_labels_off;
static void Map_labels_toggle( Widget w, XtPointer clientData, XtPointer calldata);
int map_labels;                 // toggle use of map_labels */

Widget map_fill_on, map_fill_off;
static void Map_fill_toggle( Widget w, XtPointer clientData, XtPointer calldata);
int map_color_fill;             /* Whether or not to fill in map polygons with solid color */

Widget map_bgcolor[12];
static void Map_background(Widget w, XtPointer clientData, XtPointer calldata);
int map_background_color;       /* Background color for maps */

int tiger_flag;
#if !defined(NO_GRAPHICS)
#if defined(HAVE_GEOTIFF)
Widget geotiff_intensity[11];
static void Geotiff_intensity(Widget w, XtPointer clientData, XtPointer calldata);
#endif
#if defined(HAVE_IMAGEMAGICK)
Widget gamma_adjust_dialog = (Widget)NULL;
Widget gamma_adjust_text;
#endif
#endif

Widget map_station_label0,map_station_label1,map_station_label2;
static void Map_station_label(Widget w, XtPointer clientData, XtPointer calldata);
int letter_style;               /* Station Letter style */

Widget map_wx_alerts_0,map_wx_alerts_1;
static void Map_wx_alerts_toggle(Widget w, XtPointer clientData, XtPointer calldata);
int wx_alert_style;             /* WX alert map style */

// ------------------------ Stations menu -----------------------------
Widget symbols_rot, symbols_on, symbols_off;
static void Symbols_toggle(Widget w, XtPointer clientData, XtPointer calldata);
static void Symbols_rotate_toggle(Widget w, XtPointer clientData, XtPointer calldata);

static void Symbols_local_stations_toggle(Widget w, XtPointer clientData, XtPointer calldata);
static void Symbols_nonlocal_stations_toggle(Widget w, XtPointer clientData, XtPointer calldata);
static void Symbols_stationary_toggle(Widget w, XtPointer clientData, XtPointer calldata);
static void Symbols_moving_toggle(Widget w, XtPointer clientData, XtPointer calldata);
static void Symbols_WX_toggle(Widget w, XtPointer clientData, XtPointer calldata);
static void Symbols_objects_toggle(Widget w, XtPointer clientData, XtPointer calldata);

int symbol_display;             // Switch for Symbols (and text) display
int symbol_display_enable;  // Set by togglebutton
int symbol_display_rotate;  // Set by togglebutton

int symbol_display_local_stations = 1;
int symbol_display_nonlocal_stations = 1;
int symbol_display_stationary = 1;
int symbol_display_moving = 1;
int symbol_display_WX = 1;
int symbol_display_objects = 1;

Widget symbols_local_stations_button;
Widget symbols_nonlocal_stations_button;
Widget symbols_stationary_button;
Widget symbols_moving_button;
Widget symbols_WX_button;
Widget symbols_objects_button;
Widget symbols_rotate_button;
Widget alt_button;
Widget call_button;
Widget speed_enable_button;
Widget course_button;
Widget dist_course_button;
Widget wx_enable_button;
Widget wx_obj_enable_button;
Widget station_phg_button;
Widget station_phg_mobiles_button;
Widget station_phg_default_button;
Widget station_amb_button;
Widget station_old_data_button;
Widget station_DF_button;
Widget station_last_heard_button;
 
Widget call_on, call_off;
static void Call_toggle(Widget w, XtPointer clientData, XtPointer calldata);
int symbol_callsign_display;    // Switch for Callsign display

Widget speed_on, speed_short, speed_off;
static void Speed_enable_toggle(Widget w, XtPointer clientData, XtPointer calldata);
static void Speed_short_toggle(Widget w, XtPointer clientData, XtPointer calldata);
int symbol_speed_display;       // Switch for Speed display
int speed_display_enable;   // Set by togglebutton
int speed_display_short;    // Set by togglebutton
Widget speed_short_button;

Widget alt_on, alt_off;
static void Alt_toggle(Widget w, XtPointer clientData, XtPointer calldata);
int symbol_alt_display;         // Switch for Altitude display

Widget course_on, course_off;
static void Course_toggle(Widget w, XtPointer clientData, XtPointer calldata);
int symbol_course_display;      // Switch for Course display

Widget dist_course_on, dist_course_off;
static void Dist_course_toggle(Widget w, XtPointer clientData, XtPointer calldata);
int symbol_dist_course_display; // Switch for Distance and Course display

Widget wx_on, wx_short, wx_off;
int symbol_weather_display;     // Switch for Weather info display
Widget wx_short_button;
static void WX_enable_toggle(Widget w, XtPointer clientData, XtPointer calldata);
static void WX_short_toggle(Widget w, XtPointer clientData, XtPointer calldata);
static void WX_obj_enable_toggle(Widget w, XtPointer clientData, XtPointer calldata);
int wx_display_enable;
int wx_display_short;
int wx_obj_display_enable;

Widget phg_on, phg_off;
static void Station_phg_toggle(Widget w, XtPointer clientData, XtPointer calldata);
static void Station_phg_mobiles_toggle(Widget w, XtPointer clientData, XtPointer calldata);
static void Station_phg_default_toggle(Widget w, XtPointer clientData, XtPointer calldata);
int show_phg;                   // Switch for Power/Height/Gain Circle display
int show_phg_mobiles;
int show_phg_default;

Widget amb_on, amb_off;
static void Station_amb_toggle(Widget w, XtPointer clientData, XtPointer calldata);
int show_amb;                   // Switch for Ambiguity rectangle display

static void Station_old_data_toggle(Widget w, XtPointer clientData, XtPointer calldata);
int show_old_data;              // Switch for displaying older station data

static void Station_DF_toggle(Widget w, XtPointer clientData, XtPointer calldata);
int show_DF;              // Switch for displaying older station data

static void Station_Last_Heard_toggle(Widget w, XtPointer clientData, XtPointer calldata);
int show_last_heard;    // Switch for displaying time since last-heard

static void Dead_Reckoning_toggle(Widget w, XtPointer clientData, XtPointer calldata);
static void Dead_Reckoning_Arc_toggle(Widget w, XtPointer clientData, XtPointer calldata);
static void Dead_Reckoning_Course_toggle(Widget w, XtPointer clientData, XtPointer calldata);
static void Dead_Reckoning_Symbol_toggle(Widget w, XtPointer clientData, XtPointer calldata);


Widget trails_on, trails_off;
static void Station_trails_toggle(Widget w, XtPointer clientData, XtPointer calldata);
int station_trails;             // Switch for Trail display


// ------------------------ Interfaces --------------------------
static void  Transmit_disable_toggle( Widget widget, XtPointer clientData, XtPointer callData);
static void  Posit_tx_disable_toggle( Widget widget, XtPointer clientData, XtPointer callData);
static void  Object_tx_disable_toggle( Widget widget, XtPointer clientData, XtPointer callData);
int transmit_disable;
int posit_tx_disable;
int object_tx_disable;
Widget iface_transmit_now, posit_tx_disable_toggle, object_tx_disable_toggle;

// ------------------------ unit conversion --------------------------
static void Units_choice_toggle(Widget w, XtPointer clientData, XtPointer calldata);

int units_english_metric;       // 0: metric, 1: english, (2: nautical)
char un_alt[2+1];               // m / ft
char un_dst[2+1];               // mi / km      (..nm)
char un_spd[4+1];               // mph / km/h   (..kn)
double cvt_m2len;               // from meter
double cvt_kn2len;              // from knots
double cvt_mi2len;              // from miles
double cvt_dm2len;              // from decimeter
double cvt_hm2len;              // from hectometer

void update_units(void);


// Coordinate System
int coordinate_system = USE_DDMMMM; // Default, used for most APRS systems


// ---------------------------- object -------------------------------
Widget object_dialog = (Widget)NULL;
Widget df_object_dialog = (Widget)NULL;
Widget object_name_data,
       object_lat_data_deg, object_lat_data_min, object_lat_data_ns, 
       object_lon_data_deg, object_lon_data_min, object_lon_data_ew,
       object_group_data, object_symbol_data, object_icon, 
       object_comment_data, ob_frame, ob_group, ob_symbol,
       signpost_frame, area_frame, area_toggle, signpost_toggle, df_bearing_toggle,
       ob_bearing_data, frameomni, framebeam,
       ob_speed, ob_speed_data, ob_course, ob_course_data, ob_altitude_data, signpost_data,
       open_filled_toggle, ob_lat_offset_data, ob_lon_offset_data,
       ob_corridor, ob_corridor_data, ob_corridor_miles,
       omni_antenna_toggle, beam_antenna_toggle;
Pixmap Ob_icon0, Ob_icon;
void Set_Del_Object(Widget w, XtPointer clientData, XtPointer calldata);
int Area_object_enabled = 0;
int Area_type = 0;
char Area_color[3] = "/0";
int Area_bright = 0;
int Area_filled = 0;
int Signpost_object_enabled = 0;
int DF_object_enabled = 0;
int Omni_antenna_enabled = 0;
int Beam_antenna_enabled = 0;
char object_shgd[5] = "0000\0";
char object_NRQ[4] = "960\0";
XtPointer global_parameter1 = (XtPointer)NULL;
XtPointer global_parameter2 = (XtPointer)NULL;


// ------------------------- audio alarms ----------------------------
Widget configure_audio_alarm_dialog = (Widget)NULL;
Widget audio_alarm_config_play_data,
       audio_alarm_config_play_on_new_station, audio_alarm_config_play_ons_data,
       audio_alarm_config_play_on_new_message, audio_alarm_config_play_onm_data,
       audio_alarm_config_play_on_prox, audio_alarm_config_play_onpx_data,
       audio_alarm_config_play_on_bando, audio_alarm_config_play_onbo_data,
       prox_min_data, prox_max_data, bando_min_data, bando_max_data,
       audio_alarm_config_play_on_wx_alert, audio_alarm_config_wx_alert_data;
static void Configure_audio_alarms(Widget w, XtPointer clientData, XtPointer callData);

// ---------------------------- speech -------------------------------
Widget configure_speech_dialog      = (Widget)NULL;
Widget speech_config_play_on_new_station,
       speech_config_play_on_new_message_alert,
       speech_config_play_on_new_message_body,
       speech_config_play_on_prox,
       speech_config_play_on_trak,
       speech_config_play_on_bando,
       speech_config_play_on_new_wx_alert;

static void Configure_speech(Widget w, XtPointer clientData, XtPointer callData);

//#ifdef HAVE_FESTIVAL
/* WARNING - new station is initialized to FALSE for a reason            */
/* If you're tempted to make it something that can be saved and restored */
/* beware, Speech cannot keep up with the initial flow of data from an   */
/* Internet connection that has buffered data. An unbuffered connection  */
/* yes, but not a buffered one.  Ken,  N7IPB                             */
int festival_speak_new_station = FALSE;
int festival_speak_proximity_alert;
int festival_speak_tracked_proximity_alert;
int festival_speak_band_opening;
int festival_speak_new_message_alert;
int festival_speak_new_message_body;
int festival_speak_new_weather_alert;
int festival_speak_ID;
//#endif
int ATV_screen_ID;

#ifdef HAVE_IMAGEMAGICK //N0VH
Widget configure_tiger_dialog = (Widget) NULL;
Widget tiger_enable_widget;
Widget tiger_cities,
       tiger_grid,
       tiger_counties,
       tiger_majroads,
       tiger_places,
       tiger_railroad,
       tiger_streets,
       tiger_interstate,
       tiger_statehwy,
       tiger_states,
       tiger_ushwy,
       tiger_water,
       tiger_lakes,
       tiger_misc,
       tiger_intensity,
       tiger_timeout;

int tiger_show_grid = TRUE;
int tiger_show_counties = TRUE;
int tiger_show_cities = TRUE;
int tiger_show_places = TRUE;
int tiger_show_majroads = TRUE;
int tiger_show_streets = FALSE;
int tiger_show_railroad = TRUE;
int tiger_show_states = FALSE;
int tiger_show_interstate = TRUE;
int tiger_show_ushwy = TRUE;
int tiger_show_statehwy = TRUE;
int tiger_show_water = TRUE;
int tiger_show_lakes = TRUE;
int tiger_show_misc = TRUE;
int tigermap_intensity = 100;
int tigermap_timeout = 30;

#endif


// -------------------------------------------------------------------

Widget tnc_logging_on, tnc_logging_off;
Widget net_logging_on, net_logging_off;
Widget igate_logging_on, igate_logging_off;
Widget wx_logging_on, wx_logging_off;

Widget read_selection_dialog = (Widget)NULL;

// config station values
Widget station_config_call_data, station_config_slat_data_deg, station_config_slat_data_min,
       station_config_slat_data_ns, station_config_slong_data_deg, station_config_slong_data_min,
       station_config_slong_data_ew, station_config_group_data, station_config_symbol_data,
       station_config_icon, station_config_comment_data;
Pixmap CS_icon0, CS_icon;

/* defaults*/
#ifdef TRANSMIT_RAW_WX
Widget raw_wx_tx;
#endif
Widget compressed_posit_tx;
Widget compressed_objects_items_tx;
Widget new_bulletin_popup_enable;
Widget zero_bulletin_popup_enable;
Widget warn_about_mouse_modifiers_enable;
int pop_up_new_bulletins = 0;
int view_zero_distance_bulletins = 0;
int warn_about_mouse_modifiers = 1;
Widget altnet_active;
Widget altnet_text;
Widget debug_level_text;
int show_DR;
int show_DR_arc;
int show_DR_course;
int show_DR_symbol;
Widget dead_reckoning_button;
Widget dead_reckoning_arc_button;
Widget dead_reckoning_course_button;
Widget dead_reckoning_symbol_button;


// -------------------------------------------------------------------
static void UpdateTime( XtPointer clientData, XtIntervalId id );
void pos_dialog(Widget w);

static void Zoom_in(Widget w, XtPointer clientData, XtPointer calldata);
static void Zoom_in_no_pan(Widget w, XtPointer clientData, XtPointer calldata);
static void Zoom_out(Widget w, XtPointer clientData, XtPointer calldata);
static void Zoom_out_no_pan(Widget w, XtPointer clientData, XtPointer calldata);
static void Zoom_level(Widget w, XtPointer clientData, XtPointer calldata);
static void display_zoom_image(int recenter);
static void Track_Me( Widget w, XtPointer clientData, XtPointer calldata);
static void Measure_Distance( Widget w, XtPointer clientData, XtPointer calldata);
static void Move_Object( Widget w, XtPointer clientData, XtPointer calldata);
 
static void Pan_ctr(Widget w, XtPointer clientData, XtPointer calldata);
static void Pan_up(Widget w, XtPointer clientData, XtPointer calldata);
static void Pan_up_less(Widget w, XtPointer clientData, XtPointer calldata);
static void Pan_down(Widget w, XtPointer clientData, XtPointer calldata);
static void Pan_down_less(Widget w, XtPointer clientData, XtPointer calldata);
static void Pan_left(Widget w, XtPointer clientData, XtPointer calldata);
static void Pan_left_less(Widget w, XtPointer clientData, XtPointer calldata);
static void Pan_right(Widget w, XtPointer clientData, XtPointer calldata);
static void Pan_right_less(Widget w, XtPointer clientData, XtPointer calldata);

static void Menu_Quit(Widget w, XtPointer clientData, XtPointer calldata);

static void TNC_Logging_toggle(Widget w, XtPointer clientData, XtPointer calldata);
static void TNC_Transmit_now(Widget w, XtPointer clientData, XtPointer calldata);

static void Net_Logging_toggle(Widget w, XtPointer clientData, XtPointer calldata);

static void IGate_Logging_toggle(Widget w, XtPointer clientData, XtPointer calldata);

static void WX_Logging_toggle(Widget w, XtPointer clientData, XtPointer calldata);

void on_off_switch(int switchpos, Widget first, Widget second);
void sel3_switch(int switchpos, Widget first, Widget second, Widget third);

static void Configure_station(Widget w, XtPointer clientData, XtPointer callData);



static void Configure_defaults(Widget w, XtPointer clientData, XtPointer callData);

static void Configure_coordinates(Widget w, XtPointer clientData, XtPointer callData);

static void Stations_Clear(Widget w, XtPointer clientData, XtPointer callData);

static void Test(Widget w, XtPointer clientData, XtPointer callData);

static void Save_Config(Widget w, XtPointer clientData, XtPointer callData);

static void Read_File_Selection(Widget w, XtPointer clientData, XtPointer callData);

static void Display_data(Widget w, XtPointer clientData, XtPointer callData);

static void Auto_msg_toggle( Widget widget, XtPointer clientData, XtPointer callData);
static void  Satellite_msg_ack_toggle( Widget widget, XtPointer clientData, XtPointer callData);
 
Widget auto_msg_toggle;
Widget satellite_msg_ack_toggle;
Widget posamb0,posamb1,posamb2,posamb3,posamb4;
 

////////////////////////////////////////////////////////////////////////////////////////////////////

/* GLOBAL DEFINES */
GC gc=0;                // Used for drawing maps
GC gc2=0;               // Used for drawing symbols
GC gc_tint=0;           // Used for tinting maps & symbols
GC gc_stipple=0;        // Used for drawing symbols
GC gc_bigfont=0;
Pixmap  pixmap;
Pixmap  pixmap_alerts;
Pixmap  pixmap_final;

Pixmap  pixmap_50pct_stipple; // 50% pixels used for position ambiguity, DF circle, etc.
Pixmap  pixmap_25pct_stipple; // 25% pixels used for large position ambiguity
Pixmap  pixmap_13pct_stipple; // 12.5% pixels used for larger position ambiguity
Pixmap  pixmap_wx_stipple;  // Used for weather alerts

XastirGlobal Global;

char *database_ptr;             /* database pointers */

long mid_x_long_offset;         // Longitude at center of map
long mid_y_lat_offset;          // Latitude  at center of map
long new_mid_x, new_mid_y;      // Check values used before applying real change
long x_long_offset;             // Longitude at top NW corner of map screen
long y_lat_offset;              // Latitude  at top NW corner of map screen
long scale_x;                   // x scaling in 1/100 sec per pixel, calculated from scale_y
long scale_y;                   // y scaling in 1/100 sec per pixel
long new_scale_x;
long new_scale_y;
long screen_width;              // Screen width,  map area without border (in pixel)
long screen_height;             // Screen height, map area without border (in pixel)
float d_screen_distance;        /* Diag screen distance */
float x_screen_distance;        /* x screen distance */

char user_dir[1000];            /* user directory file */
int delay_time;                 /* used to delay display data */
time_t last_weather_cycle;      // Time of last call to cycle_weather()
int colors[256];                /* screen colors */
int trail_colors[32];           /* station trail colors, duh */
int current_trail_color;        /* what color to draw station trails with */
int max_trail_colors = 32;
Pixel_Format visual_type = NOT_TRUE_NOR_DIRECT;
int install_colormap;           /* if visual_type == NOT_TRUE..., should we install priv cmap */
Colormap cmap;                  /* current colormap */

int redo_list;                  // Station List update request
int redraw_on_new_data;         // Station redraw request
int wait_to_redraw;             /* wait to redraw until system is up */
int display_up;                 /* display up? */
int display_up_first;           /* display up first */

time_t max_transmit_time;       /* max time between transmits */
time_t last_alert_redraw;       /* last time alert caused a redraw */
time_t sec_next_gps;            /* next gps check */
time_t gps_time;                /* gps delay time */
time_t POSIT_rate;              // Posit & object/item rate timer
time_t update_DR_rate;          // How often to call draw_symbols if DR enabled
time_t remove_ID_message_time;  // Time to get rid of large msg on screen.
int pending_ID_message = 0;     // Variable turning on/off this function


// SmartBeaconing(tm) stuff.  If enabled, POSIT_rate is only used for
// objects & items, sb_POSIT_rate computed via SmartBeaconing(tm) will
// be used for posits.
int smart_beaconing;            // Master enable/disable for SmartBeaconing(tm) mode
int sb_POSIT_rate = 30 * 60;    // Computed SmartBeaconing(tm) posit rate (secs)
int sb_last_heading = -1;       // Heading at time of last posit
int sb_current_heading = -1;    // Most recent heading parsed from GPS sentence
int sb_turn_min = 20;           // Min threshold for corner pegging (degrees)
int sb_turn_slope = 25;         // Threshold slope for corner pegging (degrees/mph)
int sb_turn_time = 5;           // Time between other beacon & turn beacon (secs)
int sb_posit_fast = 90;         // Fast beacon rate (secs)
int sb_posit_slow = 30;         // Slow beacon rate (mins)
int sb_low_speed_limit = 2;     // Speed below which SmartBeaconing(tm) is disabled &
                                // we'll beacon at the POSIT_slow rate (mph)
int sb_high_speed_limit = 60;   // Speed above which we'll beacon at the
                                // POSIT_fast rate (mph)
Widget smart_beacon_dialog = (Widget)NULL;
Widget smart_beacon_enable = (Widget)NULL;
Widget sb_hi_rate_data = (Widget)NULL;
Widget sb_hi_mph_data = (Widget)NULL;
Widget sb_lo_rate_data = (Widget)NULL;
Widget sb_lo_mph_data = (Widget)NULL;
Widget sb_min_turn_data = (Widget)NULL;
Widget sb_turn_slope_data = (Widget)NULL;
Widget sb_wait_time_data = (Widget)NULL;



time_t GPS_time;                /* gps time out */
time_t last_statusline;         // last update of statusline or 0 if inactive
time_t last_id_time;            // Time of last ID message to statusline
time_t sec_old;                 /* station old after */
time_t sec_clear;               /* station cleared after */
time_t sec_remove;              /* Station removed after */
time_t sec_next_raw_wx;         /* raw wx transmit data */

#ifdef TRANSMIT_RAW_WX
int transmit_raw_wx;            /* transmit raw wx data? */
#endif

int transmit_compressed_posit;  // transmit location in compressed format?
int transmit_compressed_objects_items;  // Same for objects & items

int output_station_type;        /* Broadcast station type */

int Configure_station_pos_amb;  /* Broadcast station position ambiguity */

long max_vectors_allowed;       /* max map vectors allowed */
long max_text_labels_allowed;   /* max map text labels allowed */
long max_symbol_labels_allowed; /* max map symbol labels allowed */

time_t net_last_time;           /* reconnect last time in seconds */
time_t net_next_time;           /* reconnect Next update delay time */

time_t posit_last_time;
time_t posit_next_time;         /* time at which next posit will occur */

time_t last_time;               /* last time in seconds */
time_t next_time;               /* Next update delay time */

time_t next_redraw;             /* Next update time */
time_t last_redraw;             /* Time of last redraw */

char aprs_station_message_type; /* aprs station message type message capable or not */

int transmit_now;               /* set to transmit now (push on moment) */
int my_position_valid = 1;      /* Don't send posits if this is zero */
int using_gps_position = 0;     /* Set to one if a GPS port is active */
int operate_as_an_igate;        /* toggle igate operations for net connections */
unsigned igate_msgs_tx;         /* current total of igate messages transmitted */

int log_tnc_data;               /* log data */
int log_net_data;               /* log data */
int log_igate;                  /* toggle to allow igate logging */
int log_wx;                     /* toggle to allow wx logging */

int snapshots_enabled = 0;      // toggle to allow creating .png snapshots on a regular basis

time_t WX_ALERTS_REFRESH_TIME;  /* Minimum WX alert map refresh time in seconds */

/* button zoom */
int menu_x;
int menu_y;
int mouse_zoom = 0;

// log file replay
int read_file;
FILE *read_file_ptr;
time_t next_file_read;

// Data for own station
char my_callsign[MAX_CALL+1];
char my_lat[MAX_LAT];
char my_long[MAX_LONG];
char my_group;
char my_symbol;
char my_phg[MAX_PHG+1];
char my_comment[MAX_COMMENT+1];
int  my_last_course;
int  my_last_speed;
long my_last_altitude;
time_t my_last_altitude_time;

/* Symbols */
SymbolData symbol_data[MAX_SYMBOLS];

/* sound run */
pid_t last_sound_pid;

/* Default directories */

char AUTO_MAP_DIR[400];
char ALERT_MAP_DIR[400];
char SELECTED_MAP_DIR[400];
char SELECTED_MAP_DATA[400];
char MAP_INDEX_DATA[400];
char SYMBOLS_DIR[400];
char HELP_FILE[400];
char SOUND_DIR[400];

char LOGFILE_TNC[400];
char LOGFILE_NET[400];
char LOGFILE_IGATE[400];
char LOGFILE_WX[400];

/* sound data */
char sound_command[90];
int  sound_play_new_station;
char sound_new_station[90];
int  sound_play_new_message;
char sound_new_message[90];

int  sound_play_prox_message;
char sound_prox_message[90];
char prox_min[30];
char prox_max[30];
int  sound_play_band_open_message;
char sound_band_open_message[90];
char bando_min[30];
char bando_max[30];
int  sound_play_wx_alert_message;
char sound_wx_alert_message[90];


static int      input_x = 0;
static int      input_y = 0;

XtAppContext app_context;
Display *display;       /*  Display             */

/* dialog popup last */
int last_popup_x;
int last_popup_y;

// Init values for Objects dialog
char last_object[9+1];
char last_obj_grp;
char last_obj_sym;
char last_obj_comment[34+1];

time_t program_start_time;
int measuring_distance = 0;
int moving_object = 0;



/////////////////////////////////////////////////////////////////////////





void Smart_Beacon_destroy_shell( /*@unused@*/ Widget widget, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    Widget shell = (Widget) clientData;
    XtPopdown(shell);
    XtDestroyWidget(shell);
    smart_beacon_dialog = (Widget)NULL;
}





// Still need to do some bounds checking on the values here.
//
// If the user enters 0's or non-numeric data, this function sets the
// values to reasonable defaults.
//
// Another thing that'd be good to do is to recalculate the next
// beacon time if one of the posit rates is shortened.  Otherwise we
// might be waiting a while to get into the "right rhythm".
//
void Smart_Beacon_change_data(Widget widget, XtPointer clientData, XtPointer callData) {

    // Snag the XmTextString data and write it into the variables
    if (smart_beacon_dialog != NULL) {
        char *str_ptr1;
        int i;

        smart_beaconing = (int)XmToggleButtonGetState(smart_beacon_enable);

        str_ptr1 = XmTextGetString(sb_hi_rate_data);
        i = atoi(str_ptr1);
        if (i == 0)
            i = 90;
        sb_posit_fast = i;
        // Free the space.
        XtFree(str_ptr1);
 
        str_ptr1 = XmTextGetString(sb_hi_mph_data);
        i = atoi(str_ptr1);
        if (i == 0)
            i = 60;
        sb_high_speed_limit = i;
        // Free the space.
        XtFree(str_ptr1);
 
        str_ptr1 = XmTextGetString(sb_lo_rate_data);
        i = atoi(str_ptr1);
        if (i == 0)
            i = 30;
        sb_posit_slow = i;
        // Free the space.
        XtFree(str_ptr1);
 
        str_ptr1 = XmTextGetString(sb_lo_mph_data);
        i = atoi(str_ptr1);
        if (i == 0)
            i = 2;
        sb_low_speed_limit = i;
        // Free the space.
        XtFree(str_ptr1);
 
        str_ptr1 = XmTextGetString(sb_min_turn_data);
        i = atoi(str_ptr1);
        if (i == 0)
            i = 20;
        sb_turn_min = i;
        // Free the space.
        XtFree(str_ptr1);
 
        str_ptr1 = XmTextGetString(sb_turn_slope_data);
        i = atoi(str_ptr1);
        if (i == 0)
            i = 25;
        sb_turn_slope = i;
        // Free the space.
        XtFree(str_ptr1);
 
        str_ptr1 = XmTextGetString(sb_wait_time_data);
        i = atoi(str_ptr1);
        if (i == 0)
            i = 5;
        sb_turn_time = i;
        // Free the space.
        XtFree(str_ptr1);
 
        Smart_Beacon_destroy_shell(widget,clientData,callData);
    }
}





void Smart_Beacon(Widget w, XtPointer clientData, XtPointer callData) {
    static Widget  pane, form, label1, label2, label3,
        label4, label5, label6, label7,
        button_ok, button_cancel;

    Atom delw;
    char temp_string[10];

    // Destroy the dialog if it exists.  This is to make sure the
    // title is correct based on the last dialog that called us.
    if (smart_beacon_dialog) {
        Smart_Beacon_destroy_shell( w, smart_beacon_dialog, callData);
    }
 
    if (!smart_beacon_dialog) {

        smart_beacon_dialog = XtVaCreatePopupShell(langcode("SMARTB001"),
                xmDialogShellWidgetClass,Global.top,
                XmNdeleteResponse,XmDESTROY,
                XmNdefaultPosition, FALSE,
                NULL);

        pane = XtVaCreateWidget("Smart_Beacon pane",
                xmPanedWindowWidgetClass,
                smart_beacon_dialog,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        form =  XtVaCreateWidget("Smart_Beacon form",
                xmFormWidgetClass,
                pane,
                XmNfractionBase, 2,
                XmNautoUnmanage, FALSE,
                XmNshadowThickness, 1,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        smart_beacon_enable = XtVaCreateManagedWidget(langcode("SMARTB011"),
                xmToggleButtonWidgetClass,form,
                XmNtopAttachment, XmATTACH_FORM,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset,5,
                XmNrightAttachment, XmATTACH_NONE,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        label1 = XtVaCreateManagedWidget(langcode("SMARTB002"),
                xmLabelWidgetClass,
                form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, smart_beacon_enable,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        sb_hi_rate_data = XtVaCreateManagedWidget("Smart_Beacon hi_rate_data", 
                xmTextWidgetClass,
                form,
                XmNeditable,   TRUE,
                XmNcursorPositionVisible, TRUE,
                XmNsensitive, TRUE,
                XmNshadowThickness,    1,
                XmNcolumns, 6,
                XmNwidth, ((6*7)+2),
                XmNmaxLength, 5,
                XmNbackground, colors[0x0f],
                XmNtopOffset, 5,
                XmNtopAttachment,XmATTACH_WIDGET,
                XmNtopWidget, smart_beacon_enable,
                XmNtopOffset, 10,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, label1,
                XmNrightAttachment,XmATTACH_FORM,
                XmNrightOffset, 10,
                XmNnavigationType, XmTAB_GROUP,
                NULL);

        label2 = XtVaCreateManagedWidget(langcode("SMARTB003"), 
                xmLabelWidgetClass, 
                form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, label1,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        sb_hi_mph_data = XtVaCreateManagedWidget("Smart_Beacon hi_mph_data", 
                xmTextWidgetClass, 
                form,
                XmNeditable,   TRUE,
                XmNcursorPositionVisible, TRUE,
                XmNsensitive, TRUE,
                XmNshadowThickness,    1,
                XmNcolumns, 6,
                XmNwidth, ((6*7)+2),
                XmNmaxLength, 3,
                XmNbackground, colors[0x0f],
                XmNtopOffset, 5,
                XmNtopAttachment,XmATTACH_WIDGET,
                XmNtopWidget, label1,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, label2,
                XmNrightAttachment,XmATTACH_FORM,
                XmNrightOffset, 10,
                XmNnavigationType, XmTAB_GROUP,
                NULL);

        label3 = XtVaCreateManagedWidget(langcode("SMARTB005"), 
                xmLabelWidgetClass, 
                form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, label2,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        sb_lo_rate_data = XtVaCreateManagedWidget("Smart_Beacon lo_rate_data", 
                xmTextWidgetClass, 
                form,
                XmNeditable,   TRUE,
                XmNcursorPositionVisible, TRUE,
                XmNsensitive, TRUE,
                XmNshadowThickness,    1,
                XmNcolumns, 6,
                XmNwidth, ((6*7)+2),
                XmNmaxLength, 3,
                XmNbackground, colors[0x0f],
                XmNtopOffset, 5,
                XmNtopAttachment,XmATTACH_WIDGET,
                XmNtopWidget, label2,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, label3,
                XmNrightAttachment,XmATTACH_FORM,
                XmNrightOffset, 10,
                XmNnavigationType, XmTAB_GROUP,
                NULL);

        label4 = XtVaCreateManagedWidget(langcode("SMARTB006"), 
                xmLabelWidgetClass, 
                form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, label3,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        sb_lo_mph_data = XtVaCreateManagedWidget("Smart_Beacon lo_mph_data", 
                xmTextWidgetClass, 
                form,
                XmNeditable,   TRUE,
                XmNcursorPositionVisible, TRUE,
                XmNsensitive, TRUE,
                XmNshadowThickness,    1,
                XmNcolumns, 6,
                XmNwidth, ((6*7)+2),
                XmNmaxLength, 3,
                XmNbackground, colors[0x0f],
                XmNtopOffset, 5,
                XmNtopAttachment,XmATTACH_WIDGET,
                XmNtopWidget, label3,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, label4,
                XmNrightAttachment,XmATTACH_FORM,
                XmNrightOffset, 10,
                XmNnavigationType, XmTAB_GROUP,
                NULL);

        label5 = XtVaCreateManagedWidget(langcode("SMARTB008"), 
                xmLabelWidgetClass, 
                form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, label4,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        sb_min_turn_data = XtVaCreateManagedWidget("Smart_Beacon min_turn_data", 
                xmTextWidgetClass, 
                form,
                XmNeditable,   TRUE,
                XmNcursorPositionVisible, TRUE,
                XmNsensitive, TRUE,
                XmNshadowThickness,    1,
                XmNcolumns, 6,
                XmNwidth, ((6*7)+2),
                XmNmaxLength, 3,
                XmNbackground, colors[0x0f],
                XmNtopOffset, 5,
                XmNtopAttachment,XmATTACH_WIDGET,
                XmNtopWidget, label4,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, label5,
                XmNrightAttachment,XmATTACH_FORM,
                XmNrightOffset, 10,
                XmNnavigationType, XmTAB_GROUP,
                NULL);

        label6 = XtVaCreateManagedWidget(langcode("SMARTB009"), 
                xmLabelWidgetClass, 
                form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, label5,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        sb_turn_slope_data = XtVaCreateManagedWidget("Smart_Beacon turn_slope_data", 
                xmTextWidgetClass, 
                form,
                XmNeditable,   TRUE,
                XmNcursorPositionVisible, TRUE,
                XmNsensitive, TRUE,
                XmNshadowThickness,    1,
                XmNcolumns, 6,
                XmNwidth, ((6*7)+2),
                XmNmaxLength, 5,
                XmNbackground, colors[0x0f],
                XmNtopOffset, 5,
                XmNtopAttachment,XmATTACH_WIDGET,
                XmNtopWidget, label5,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, label6,
                XmNrightAttachment,XmATTACH_FORM,
                XmNrightOffset, 10,
                XmNnavigationType, XmTAB_GROUP,
                NULL);

        label7 = XtVaCreateManagedWidget(langcode("SMARTB010"), 
                xmLabelWidgetClass, 
                form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, label6,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        sb_wait_time_data = XtVaCreateManagedWidget("Smart_Beacon wait_time_data", 
                xmTextWidgetClass, 
                form,
                XmNeditable,   TRUE,
                XmNcursorPositionVisible, TRUE,
                XmNsensitive, TRUE,
                XmNshadowThickness,    1,
                XmNcolumns, 6,
                XmNwidth, ((6*7)+2),
                XmNmaxLength, 3,
                XmNbackground, colors[0x0f],
                XmNtopOffset, 5,
                XmNtopAttachment,XmATTACH_WIDGET,
                XmNtopWidget, label6,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, label7,
                XmNrightAttachment,XmATTACH_FORM,
                XmNrightOffset, 10,
                XmNnavigationType, XmTAB_GROUP,
                NULL);

        button_ok = XtVaCreateManagedWidget(langcode("UNIOP00001"),
                xmPushButtonGadgetClass, 
                form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, sb_wait_time_data,
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
                NULL);
        XtAddCallback(button_ok, XmNactivateCallback, Smart_Beacon_change_data, smart_beacon_dialog);

        button_cancel = XtVaCreateManagedWidget(langcode("UNIOP00003"),
                xmPushButtonGadgetClass, 
                form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, sb_wait_time_data,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_FORM,
                XmNbottomOffset, 5,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 1,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 2,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(button_cancel, XmNactivateCallback, Smart_Beacon_destroy_shell, smart_beacon_dialog);

        pos_dialog(smart_beacon_dialog);

        delw = XmInternAtom(XtDisplay(smart_beacon_dialog),"WM_DELETE_WINDOW", FALSE);
        XmAddWMProtocolCallback(smart_beacon_dialog, delw, Smart_Beacon_destroy_shell, (XtPointer)smart_beacon_dialog);

        XtManageChild(form);
        XtManageChild(pane);
        XtPopup(smart_beacon_dialog,XtGrabNone);
        fix_dialog_size(smart_beacon_dialog);

        // Move focus to the Close button.  This appears to highlight the
        // button fine, but we're not able to hit the <Enter> key to
        // have that default function happen.  Note:  We _can_ hit the
        // <SPACE> key, and that activates the option.
//        XmUpdateDisplay(smart_beacon_dialog);
        XmProcessTraversal(button_cancel, XmTRAVERSE_CURRENT);

    } else {
        (void)XRaiseWindow(XtDisplay(smart_beacon_dialog), XtWindow(smart_beacon_dialog));
    }

    // Fill in the current values
    if (smart_beacon_dialog != NULL) {

        if(smart_beaconing)
            XmToggleButtonSetState(smart_beacon_enable,TRUE,FALSE);
        else
            XmToggleButtonSetState(smart_beacon_enable,FALSE,FALSE);

        xastir_snprintf(temp_string, sizeof(temp_string), "%d", sb_posit_fast);
        XmTextSetString(sb_hi_rate_data, temp_string);

        xastir_snprintf(temp_string, sizeof(temp_string), "%d", sb_high_speed_limit);
        XmTextSetString(sb_hi_mph_data, temp_string);

        xastir_snprintf(temp_string, sizeof(temp_string), "%d", sb_posit_slow);
        XmTextSetString(sb_lo_rate_data, temp_string);

        xastir_snprintf(temp_string, sizeof(temp_string), "%d", sb_low_speed_limit);
        XmTextSetString(sb_lo_mph_data, temp_string);

        xastir_snprintf(temp_string, sizeof(temp_string), "%d", sb_turn_min);
        XmTextSetString(sb_min_turn_data, temp_string);

        xastir_snprintf(temp_string, sizeof(temp_string), "%d", sb_turn_slope);
        XmTextSetString(sb_turn_slope_data, temp_string);

        xastir_snprintf(temp_string, sizeof(temp_string), "%d", sb_turn_time);
        XmTextSetString(sb_wait_time_data, temp_string);
    }
}





/////////////////////////////////////////////////////////////////////////





void check_weather_symbol(void) {
    // Check for weather station, if so, make sure symbol is proper type
    if ( (output_station_type == 4) || (output_station_type == 5) ) {
        // Need one of these symbols if a weather station: /_   \_   /W   \W
        if ( ( (my_symbol != '_') && (my_symbol != 'W') )
            || ( (my_group != '\\') && (my_group != '/') ) ) {

            // Force it to '/_'
            my_group = '/';
            my_symbol = '_';

            // Update my station data with the new symbol
            my_station_add(my_callsign,my_group,my_symbol,my_long,my_lat,my_phg,my_comment,(char)position_amb_chars);
            redraw_on_new_data=2;

            // Notify the operator that the symbol has been changed
            // "Weather Station", "Changed to WX symbol '/_', other option is '\\_'"
            popup_message( langcode("POPEM00030"), langcode("POPEM00031") );
        }
    }
}





void check_nws_weather_symbol(void) {
    if ( (my_symbol == 'W')
            && ( (my_group == '\\') || (my_group == '/') ) ) {

        // Notify the operator that they're trying to be an NWS
        // weather station.
        popup_message( langcode("POPEM00030"), langcode("POPEM00032") );
    }
}





void Coordinate_calc_destroy_shell( /*@unused@*/ Widget widget, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    Widget shell = (Widget) clientData;
    XtPopdown(shell);
    XtDestroyWidget(shell);
    coordinate_calc_dialog = (Widget)NULL;
}





// Clears out the dialog's input textFields
void Coordinate_calc_clear_data(Widget widget, XtPointer clientData, XtPointer callData) {
    XmTextSetString(coordinate_calc_zone, "");
    XmTextSetString(coordinate_calc_latitude_easting, "");
    XmTextSetString(coordinate_calc_longitude_northing, "");
    XmTextSetString(coordinate_calc_result_text, "");
    XtSetSensitive(coordinate_calc_button_ok,FALSE);
}





// Computes all four coordinate representations for displaying in
// the "result" textField.  Also fills in the global variables for
// possible later use when passing results back to the calling
// dialog.  We can't use the util.c:*_l2s routines for the
// conversions here because the util.c routines use Xastir
// coordinate system as inputs instead of normal lat/lon.  Had to
// home-grow our solution here.
//
// Inputs:  full_zone, northing, easting, latitude, longitude.  UTM
// inputs are output directly.  Latitude/longitude are converted to
// the various different lat/lon representations.
//
// Outputs: global variables and "result" textField.
//
void Coordinate_calc_output(char *full_zone, long northing,
            long easting, double latitude, double longitude) {
    char temp_string[1024];
    int south = 0;
    int west = 0;
    double lat_min,lon_min,lat_sec,lon_sec;
    int lat_deg_int,lat_min_int;
    int lon_deg_int,lon_min_int;
    char maidenhead_grid[50];
    long temp;
    long xastir_lat;
    long xastir_lon;


    // Latitude:  Switch to integer arithmetic to avoid
    // floating-point rounding errors.
    // We _do_ need to round it first though so that we don't lose
    // accuracy.
    xastir_snprintf(temp_string,sizeof(temp_string),"%8.0f",latitude * 100000.0);
    temp = atol(temp_string);
    if (temp < 0) {
        south++;
        temp = labs(temp);
    }
    lat_deg_int = (int)temp / 100000;
    lat_min = (temp % 100000) * 60.0 / 100000.0;

    // Again switch to integer arithmetic to avoid floating-point
    // rounding errors.
    temp = (long)(lat_min * 1000);
    lat_min_int = (int)(temp / 1000);
    lat_sec = (temp % 1000) * 60.0 / 1000.0;


    // Longitude:  Switch to integer arithmetic to avoid
    // floating-point rounding errors.
    // We _do_ need to round it first though so that we don't lose
    // accuracy.
    xastir_snprintf(temp_string,sizeof(temp_string),"%9.0f",longitude * 100000.0);
    temp = atol(temp_string);
    if (temp < 0) {
        west++;
        temp = labs(temp);
    }
    lon_deg_int = (int)temp / 100000;
    lon_min = (temp % 100000) * 60.0 / 100000.0;

    // Again switch to integer arithmetic to avoid floating-point
    // rounding errors.
    temp = (long)(lon_min * 1000);
    lon_min_int = (int)(temp / 1000);
    lon_sec = (temp % 1000) * 60.0 / 1000.0;

    // Compute Maidenhead Grid Locator.  Note that the sec_to_loc()
    // function expects lat/lon in Xastir coordinate system.
    convert_UTM_to_xastir(easting,
        northing,
        full_zone,
        &xastir_lon,
        &xastir_lat);
    xastir_snprintf(maidenhead_grid,
        sizeof(maidenhead_grid),
        "%s",
        sec_to_loc( xastir_lon, xastir_lat ) );

    // Put the four different representations of the coordinate into
    // the "result" textField.
    xastir_snprintf(temp_string,
        sizeof(temp_string),
        "%s%8.5f%c   %9.5f%c\n%s%02d %06.3f%c  %03d %06.3f%c\n%s%02d %02d %04.1f%c %03d %02d %04.1f%c\n%s%3s  %07lu  %07lu\n%s%s",
        "               Decimal Degrees:  ",
        lat_deg_int+lat_min/60.0, (south) ? 'S':'N',
        lon_deg_int+lon_min/60.0, (west) ?  'W':'E',
        "       Degrees/Decimal Minutes:  ",
        lat_deg_int, lat_min, (south) ? 'S':'N',
        lon_deg_int, lon_min, (west) ?  'W':'E',
        "  Degrees/Minutes/Dec. Seconds:  ",
        lat_deg_int, lat_min_int, lat_sec, (south) ? 'S':'N',
        lon_deg_int, lon_min_int, lon_sec, (west) ?  'W':'E',
        " Universal Transverse Mercator:  ",
        full_zone, easting, northing,
        "       Maidenhead Grid Locator:  ",
        maidenhead_grid);
    XmTextSetString(coordinate_calc_result_text, temp_string);

    // Fill in the global dd mm.mmm values in case we wish to write
    // the result back to the calling dialog.
    xastir_snprintf(coordinate_calc_lat_deg, sizeof(coordinate_calc_lat_deg),
        "%02d", lat_deg_int);
    xastir_snprintf(coordinate_calc_lat_min, sizeof(coordinate_calc_lat_min),
        "%06.3f", lat_min);
    xastir_snprintf(coordinate_calc_lat_dir, sizeof(coordinate_calc_lat_dir),
        "%c", (south) ? 'S':'N');
    xastir_snprintf(coordinate_calc_lon_deg, sizeof(coordinate_calc_lon_deg),
        "%03d", lon_deg_int);
    xastir_snprintf(coordinate_calc_lon_min, sizeof(coordinate_calc_lon_min),
        "%06.3f", lon_min);
    xastir_snprintf(coordinate_calc_lon_dir, sizeof(coordinate_calc_lon_dir),
        "%c", (west) ? 'W':'E');
}





// Coordinate_calc_compute
//
// Inputs:  coordinate_calc_zone textField
//          coordinate_calc_latitude_easting textField
//          coordinate_calc_longitude_northing textField
//
// Output:  coordinate_calc_result_text only if the inputs are not
// recognized, then it outputs help text to the textField.  If
// inputs are good it calls Coordinate_calc_output() to format and
// save/output the results.
//
void Coordinate_calc_compute(Widget widget, XtPointer clientData, XtPointer callData) {
    char *str_ptr;
    char zone_letter;
    int zone_number = 0;
    char full_zone[5];
    int i;
    int have_utm;
    int have_lat_lon;
    long easting = 0;
    long northing = 0;
    double double_easting;
    double double_northing;
    double latitude;
    double longitude;
    char temp_string[1024];


    // Goal is to suck in the format provided, figure out what
    // format it is, then convert to the four major formats we
    // support and put all four into the output window, each on a
    // different line.

    // These are the formats that I'd like to be able to
    // auto-recognize and support:

    // ddN          dddW            IMPLEMENTED
    // dd N         ddd W           IMPLEMENTED
    // -dd          -ddd            IMPLEMENTED

    // dd.ddddN     ddd.ddddW       IMPLEMENTED
    // dd.dddd N    ddd.dddd W      IMPLEMENTED
    // -dd.dddd     -ddd.dddd       IMPLEMENTED

    // dd mmN       ddd mmW         IMPLEMENTED
    // dd mm N      ddd mm W        IMPLEMENTED
    // -dd mm       -ddd mm         IMPLEMENTED

    // dd mm.mmmN   ddd mm.mmmW     IMPLEMENTED
    // dd mm.mmm N  ddd mm.mmm W    IMPLEMENTED
    // -dd mm.mmm   -ddd mm.mmm     IMPLEMENTED

    // dd mm ssN    ddd mm ssW      IMPLEMENTED
    // dd mm ss N   ddd mm ss W     IMPLEMENTED
    // -dd mm ss    -ddd mm ss      IMPLEMENTED

    // dd mm ss.sN  ddd mm ss.sW    IMPLEMENTED
    // dd mm ss.s N ddd mm ss.s W   IMPLEMENTED
    // -dd mm ss.s  -ddd mm ss.s    IMPLEMENTED

    // 10T  0123456     1234567     IMPLEMENTED
    // 10T   123456     1234567     IMPLEMENTED
    // 10T  012 3456    123 4567
    // 10T   12 3456    123 4567

    // Once the four major formats are created and written to the
    // output test widget, the dd mm.mmmN/ddd mm.mmmW formatted
    // output should also be saved for later pasting into the
    // calling dialog's input fields.  DONE!
    //
    // Must also make sure that the calling dialog is still up and
    // active before we try to write to it's widgets.  DONE!


    // First check for something in the zone field that looks like a
    // valid zone.
    str_ptr = XmTextGetString(coordinate_calc_zone);
    i = strlen(str_ptr);
    have_utm = 1;   // Wishful thinking.  We'll zero it later if not.
    if ( (i == 2) || (i == 3) ) {   // String is the correct length: Has
                                    // to be either 2 or 3 chars total.
        int j;
        for (j = 0; j < (i-1); j++) {
            if ( (str_ptr[j] < '0') && (str_ptr[j] > '9') ) {
                // Not UTM, need either one or two digits first
                have_utm = 0;
            }
        }
        if ( ( (str_ptr[i-1] < 'A') || (str_ptr[i-1] > 'Z') )
            && ( (str_ptr[i-1] < 'a') || (str_ptr[i-1] > 'z') ) ) {
            // Not UTM, zone character isn't correct
            have_utm = 0;
        }
    }
    else {  // Not UTM, wrong length for zone
        have_utm = 0;
    }
    // If we've made it to this point and have_utm == 1, then zone looks
    // like a UTM zone.
    if (have_utm) {
        zone_letter = toupper(str_ptr[i-1]);
        zone_number = atoi(str_ptr);
        //printf("Zone Number: %d,  Zone Letter: %c\n", zone_number, zone_letter);
        // Save it away for later use
        xastir_snprintf(full_zone,
            sizeof(full_zone),
            "%d%c",
            zone_number,
            zone_letter);
        have_lat_lon = 0;
    }
    else {
        //printf("Bad zone, not a UTM coordinate\n");
        // Skip zone widget for lat/lon, it's not used.
        have_lat_lon = 1;   // Wishful thinking.  We'll zero it later if not.
    }
    // We're done with that variable.  Free the space.
    XtFree(str_ptr);


    str_ptr = XmTextGetString(coordinate_calc_latitude_easting);
    i = strlen(str_ptr);
    // Check for exactly six or seven chars.  If seven, first one must
    // be a zero.
    if ( have_utm && (i != 6) && (i != 7) ) {
        have_utm = 0;
        //printf("Bad Easting value: Not 6 or 7 chars\n");
    }
    if ( have_utm && (i == 7) && (str_ptr[0] != '0') ) {
        have_utm = 0;
        //printf("Bad Easting value: 7 chars but first one not 0\n");
    }
    if (have_utm) {
        int j;

        // Might be good to get rid of spaces at this point as we think
        // it's a UTM number.  Might have to put it in our own string
        // first though to do that.

        for (j = 0; j < i; j++) {
            if ( (str_ptr[j] < '0') || (str_ptr[j] > '9') ) {
                // Not UTM, found a non-number
                have_utm = 0;
            }
        }
        if (have_utm) { // If we still think it's a valid UTM number
            easting = atol(str_ptr);
            //printf("Easting: %lu\n",easting);
        }
        else {
            have_utm = 0;
            //printf("Bad Easting value\n");
        }
    }
    else if (have_lat_lon) {
        // Process the string to see if it's a valid latitude value.
        // Convert it into a double if so and store it in
        // "latitude".
        int j, substring;
        int south = 0;
        int temp[10];  // indexes to substrings
        char *ptr;
        char temp_string[30];
        int piece;

        // Copy the string so we can change it.
        xastir_snprintf(temp_string,sizeof(temp_string),"%s",str_ptr);

        for (j = 0; j < i; j++) {
           temp_string[j] = toupper(temp_string[j]);
        }

        // Search for 'N' or 'S'.
        ptr = rindex(temp_string, 'N');
        if (ptr != NULL) {  // Found an 'N'
            *ptr = ' ';     // Convert it to a space
            //printf("Found an 'N', converted to %s\n", temp_string);
        }
        ptr = rindex(temp_string, 'S');
        if (ptr != NULL) {  // Found an 'S'
            *ptr = ' ';     // Convert it to a space
            south++;
            //printf("Found an 'S', converted to %s\n", temp_string);
        }
        ptr = rindex(temp_string, '-');
        if (ptr != NULL) {  // Found an '-'
            *ptr = ' ';     // Convert it to a space
            south++;
            //printf("Found an '-', converted to %s\n", temp_string);
        }

        // Tokenize the string 

        // Find the space characters
        temp[0] = 0;        // First index is to start of entire string
        substring = 1;
        for (j = 1; j < i; j++) {
            if (temp_string[j] == ' ') {        // Found a space
                temp_string[j] = '\0';          // Terminate the substring
                if ( (j + 1) < i) {             // If not at the end
                    temp[substring++] = j + 1;  // Save an index to the new substring
                    //printf("%s",&temp_string[j+1]);
                }
            }
        }

        // temp[] array now contains indexes into all of the
        // substrings.  Some may contain empty strings.

        //printf("Substrings: %d\n", substring);
        //printf("temp_string: %s\n",temp_string);


        //for (j = 0; j < substring; j++) {
        //    if (strlen(&temp_string[temp[j]]) > 0) {
        //        printf("%s\n", &temp_string[temp[j]]);
        //    }
        //}

        piece = 0;
        for (j = 0; j < substring; j++) {
            if (strlen(&temp_string[temp[j]]) > 0) {
                piece++;    // Found the next piece
                switch (piece) {
                    case (1) :  // Degrees
                        latitude = atof(&temp_string[temp[j]]);
                        break;
                    case (2) :  // Minutes
                        latitude = latitude + ( atof(&temp_string[temp[j]]) / 60.0 );
                        break;
                    case (3) :  // Seconds
                        latitude = latitude + ( atof(&temp_string[temp[j]]) / 3600.0 );
                        break;
                    default :
                        break;
                }
            }
        }

        if (south) {
            latitude = -latitude;
        }
        //printf("%f\n", latitude);

        // Test for valid values of latitude
        if ( (latitude < -90.0) || (latitude > 90.0) ) {
            have_lat_lon = 0;
        }
        if (strlen(str_ptr) == 0) {
            have_lat_lon = 0;
        }
    }
    // We're done with that variable.  Free the space.
    XtFree(str_ptr); 


    str_ptr = XmTextGetString(coordinate_calc_longitude_northing);
    i = strlen(str_ptr);
    // Check for exactly seven chars.
    if (have_utm && (i != 7) ) {
        have_utm = 0;
        //printf("Bad Northing value: Not 7 chars\n");
    }
    if (have_utm) {
        int j;

        // Might be good to get rid of spaces at this point as we think
        // it's a UTM number.  Might have to put it in our own string
        // first though to do that.

        for (j = 0; j< i; j++) {
            if ( (str_ptr[j] < '0') || (str_ptr[j] > '9') ) {
                // Not UTM, found a non-number
                have_utm = 0;
            }
        }
        if (have_utm) { // If we still think it's a valid UTM number
            northing = atol(str_ptr);
            //printf("Northing: %lu\n",northing);
        }
        else {
            have_utm = 0;
            //printf("Bad Northing value\n");
        }
    }
    else if (have_lat_lon) {
        // Process the string to see if it's a valid longitude
        // value.  Convert it into a double if so and store it in
        // "longitude".
        int j, substring;
        int west = 0;
        int temp[10];  // indexes to substrings
        char *ptr;
        char temp_string[30];
        int piece;

        // Copy the string so we can change it.
        xastir_snprintf(temp_string,sizeof(temp_string),"%s",str_ptr);

        for (j = 0; j < i; j++) {
           temp_string[j] = toupper(temp_string[j]);
        }

        // Search for 'W' or 'E'.
        ptr = rindex(temp_string, 'W');
        if (ptr != NULL) {  // Found an 'W'
            *ptr = ' ';     // Convert it to a space
            west++;
            //printf("Found an 'W', converted to %s\n", temp_string);
        }
        ptr = rindex(temp_string, 'E');
        if (ptr != NULL) {  // Found an 'E'
            *ptr = ' ';     // Convert it to a space
            //printf("Found an 'E', converted to %s\n", temp_string);
        }
        ptr = index(temp_string, '-');
        if (ptr != NULL) {  // Found an '-'
            *ptr = ' ';     // Convert it to a space
            west++;
            //printf("Found an '-', converted to %s\n", temp_string);
        }
 
        // Tokenize the string 

        // Find the space characters
        temp[0] = 0;        // First index is to start of entire string
        substring = 1;
        for (j = 1; j < i; j++) {
            if (temp_string[j] == ' ') {        // Found a space
                temp_string[j] = '\0';          // Terminate the substring
                if ( (j + 1) < i) {             // If not at the end
                    temp[substring++] = j + 1;  // Save an index to the new substring
                    //printf("%s",&temp_string[j+1]);
                }
            }
        }

        // temp[] array now contains indexes into all of the
        // substrings.  Some may contain empty strings.

        //printf("Substrings: %d\n", substring);
        //printf("temp_string: %s\n",temp_string);


        //for (j = 0; j < substring; j++) {
        //    if (strlen(&temp_string[temp[j]]) > 0) {
        //        printf("%s\n", &temp_string[temp[j]]);
        //    }
        //}

        piece = 0;
        for (j = 0; j < substring; j++) {
            if (strlen(&temp_string[temp[j]]) > 0) {
                piece++;    // Found the next piece
                switch (piece) {
                    case (1) :  // Degrees
                        longitude = atof(&temp_string[temp[j]]);
                        break;
                    case (2) :  // Minutes
                        longitude = longitude + ( atof(&temp_string[temp[j]]) / 60.0 );
                        break;
                    case (3) :  // Seconds
                        longitude = longitude + ( atof(&temp_string[temp[j]]) / 3600.0 );
                        break;
                    default :
                        break;
                }
            }
        }

        if (west) {
            longitude = -longitude;
        }
        //printf("%f\n", longitude);


        // Test for valid values of longitude
        if ( (longitude < -180.0) || (longitude > 180.0) ) {
            have_lat_lon = 0;
        }
        if (strlen(str_ptr) == 0) {
            have_lat_lon = 0;
        }
    }
    // We're done with that variable.  Free the space.
    XtFree(str_ptr);

    // If we get to this point and have_utm == 1, then we're fairly sure
    // we have a good value and can convert it to the other formats for
    // display.
    if (have_utm) {
        // Process UTM values
        utm_to_ll(E_WGS_84,
            (double)northing,
            (double)easting,
            full_zone,
            &latitude,
            &longitude);
        if (debug_level & 1)
            printf("Latitude: %f, Longitude: %f\n",latitude,longitude); 
        Coordinate_calc_output(full_zone,
            northing,
            easting,
            latitude,
            longitude);
        XtSetSensitive(coordinate_calc_button_ok,TRUE);
    }
    else if (have_lat_lon) {
        // Process lat/lon values
        double_northing = (double)northing;
        double_easting = (double)easting;
        ll_to_utm(E_WGS_84,
            (double)latitude,
            (double)longitude,
            &double_northing,
            &double_easting,
            full_zone,
            sizeof(full_zone));
        if (debug_level & 1)
            printf("Zone: %s, Easting: %f, Northing: %f\n", full_zone, double_easting, double_northing); 
        // Round the UTM values as we convert them to longs
        xastir_snprintf(temp_string,sizeof(temp_string),"%7.0f",double_northing);
        northing = (long)(atof(temp_string));
        xastir_snprintf(temp_string,sizeof(temp_string),"%7.0f",double_easting);
        easting  = (long)(atof(temp_string));
        Coordinate_calc_output(full_zone,
            (long)northing,
            (long)easting,
            latitude,
            longitude);
        XtSetSensitive(coordinate_calc_button_ok,TRUE);
    }
    else {  // Dump out some helpful text
        xastir_snprintf(temp_string,
            sizeof(temp_string),
            "%s\n%s\n%s\n%s",
            " **       Sorry, your input was not recognized!        **",
            " **   Please use one of the following input formats:   **",
            " ** 47.99999N  121.99999W,   47 59.999N   121 59.999W  **",
            " ** 10T  0574599  5316887,   47 59 59.9N  121 59 59.9W **");
        XmTextSetString(coordinate_calc_result_text, temp_string);
        XtSetSensitive(coordinate_calc_button_ok,FALSE);
    }
}





// Input:  Values from the coordinate_calc_array struct.
//
// Output:  Writes data back to the calling dialog's input fields if
// the calling dialog still exists at this point.
//
// Make sure that if an error occurs during computation we don't
// write a bad value back to the calling widget.  DONE.
//
void Coordinate_calc_change_data(Widget widget, XtPointer clientData, XtPointer callData) {

    // Write output directly to the XmTextStrings pointed to by our array
    if ( (coordinate_calc_array.calling_dialog != NULL)
            && (coordinate_calc_array.input_lat_deg != NULL) )
        XmTextSetString(coordinate_calc_array.input_lat_deg, coordinate_calc_lat_deg);
    //printf("%s\n",coordinate_calc_lat_deg);

    if ( (coordinate_calc_array.calling_dialog != NULL)
            && (coordinate_calc_array.input_lat_min != NULL) )
        XmTextSetString(coordinate_calc_array.input_lat_min, coordinate_calc_lat_min);
    //printf("%s\n",coordinate_calc_lat_min);

    if ( (coordinate_calc_array.calling_dialog != NULL)
            && (coordinate_calc_array.input_lat_dir != NULL) )
        XmTextSetString(coordinate_calc_array.input_lat_dir, coordinate_calc_lat_dir);
    //printf("%s\n",coordinate_calc_lat_dir);

    if ( (coordinate_calc_array.calling_dialog != NULL)
            && (coordinate_calc_array.input_lon_deg != NULL) )
        XmTextSetString(coordinate_calc_array.input_lon_deg, coordinate_calc_lon_deg);
    //printf("%s\n",coordinate_calc_lon_deg);

    if ( (coordinate_calc_array.calling_dialog != NULL)
            && (coordinate_calc_array.input_lon_min != NULL) )
        XmTextSetString(coordinate_calc_array.input_lon_min, coordinate_calc_lon_min);
    //printf("%s\n",coordinate_calc_lon_min);

    if ( (coordinate_calc_array.calling_dialog != NULL)
            && (coordinate_calc_array.input_lon_dir != NULL) )
        XmTextSetString(coordinate_calc_array.input_lon_dir, coordinate_calc_lon_dir);
    //printf("%s\n",coordinate_calc_lon_dir);

    Coordinate_calc_destroy_shell(widget,clientData,callData);
}





// Coordinate Calculator
//
// Change the title based on what dialog is calling us?
//
// We want all four possible coordinate formats displayed
// simultaneously.  DONE.
//
// Hitting enter or "Calculate" will cause all of the fields to be
// updated.  DONE (for Calculate button).
//
// The fields should be filled in when this is first called.
// When done, this routine will pass back values via a static array
// of Widget pointers to the calling dialog's fields.  DONE.
//
// We could grey-out the OK button until we have a successful
// calculation, and when the "Clear" button is pressed.  This
// would make sure that an invalid location doesn't
// get written to the calling dialog.  Would have to have a
// successful conversion before we could write the value back.
//
void Coordinate_calc(Widget w, XtPointer clientData, XtPointer callData) {
    static Widget  pane, form, label1, label2, label3,
        label4, label5, label6,
        button_clear, button_calculate, button_cancel;
    Atom delw;
    Arg args[2];                    // Arg List
    register unsigned int n = 0;    // Arg Count
    char temp_string[50];

    // Destroy the dialog if it exists.  This is to make sure the
    // title is correct based on the last dialog that called us.
    if (coordinate_calc_dialog) {
        Coordinate_calc_destroy_shell( w, coordinate_calc_dialog, callData);
    }
 
    if (!coordinate_calc_dialog) {

        // We change the title based on who's calling us.
        // clientData supplies the string we use for the label, and
        // is sent to us by the calling dialog.
        xastir_snprintf( temp_string, sizeof(temp_string), "%s %s", (char *)clientData, langcode("COORD001") );

        coordinate_calc_dialog = XtVaCreatePopupShell(temp_string,
                xmDialogShellWidgetClass,
                Global.top,
                XmNdeleteResponse,XmDESTROY,
                XmNdefaultPosition, FALSE,
                NULL);

        pane = XtVaCreateWidget("Coordinate_calc pane",
                xmPanedWindowWidgetClass, 
                coordinate_calc_dialog,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        form =  XtVaCreateWidget("Coordinate_calc form",
                xmFormWidgetClass, 
                pane,
                XmNfractionBase, 4,
                XmNautoUnmanage, FALSE,
                XmNshadowThickness, 1,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        label1 = XtVaCreateManagedWidget(langcode("COORD005"), 
                xmLabelWidgetClass, 
                form,
                XmNtopAttachment, XmATTACH_FORM,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        label2 = XtVaCreateManagedWidget(langcode("COORD006"), 
                xmLabelWidgetClass, 
                form,
                XmNtopAttachment, XmATTACH_FORM,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 70,
                XmNrightAttachment, XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        label3 = XtVaCreateManagedWidget(langcode("COORD007"), 
                xmLabelWidgetClass, 
                form,
                XmNtopAttachment, XmATTACH_FORM,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 200,
                XmNrightAttachment, XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        label4 = XtVaCreateManagedWidget(langcode("COORD008"), 
                xmLabelWidgetClass, 
                form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, label1,
                XmNtopOffset, 2,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        label5 = XtVaCreateManagedWidget(langcode("COORD009"), 
                xmLabelWidgetClass, 
                form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, label1,
                XmNtopOffset, 2,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 70,
                XmNrightAttachment, XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        label6 = XtVaCreateManagedWidget(langcode("COORD010"), 
                xmLabelWidgetClass, 
                form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, label1,
                XmNtopOffset, 2,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 200,
                XmNrightAttachment, XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);


        /*set args for color */
        n=0;
        XtSetArg(args[n], XmNforeground, MY_FG_COLOR); n++;
        XtSetArg(args[n], XmNbackground, MY_BG_COLOR); n++;


        coordinate_calc_zone = XtVaCreateManagedWidget("Coordinate_calc zone", 
                xmTextWidgetClass, 
                form,
                XmNeditable,   TRUE,
                XmNcursorPositionVisible, TRUE,
                XmNsensitive, TRUE,
                XmNshadowThickness,    1,
                XmNcolumns, 4,
                XmNwidth, ((5*7)+2),
                XmNmaxLength, 3,
                XmNbackground, colors[0x0f],
                XmNtopOffset, 5,
                XmNtopAttachment,XmATTACH_WIDGET,
                XmNtopWidget, label4,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 5,
                XmNrightAttachment,XmATTACH_NONE,
                XmNnavigationType, XmTAB_GROUP,
                NULL);

       coordinate_calc_latitude_easting = XtVaCreateManagedWidget("Coordinate_calc lat", 
                xmTextWidgetClass, 
                form,
                XmNeditable,   TRUE,
                XmNcursorPositionVisible, TRUE,
                XmNsensitive, TRUE,
                XmNshadowThickness,    1,
                XmNcolumns, 13,
                XmNwidth, ((13*7)+2),
                XmNmaxLength, 12,
                XmNbackground, colors[0x0f],
                XmNtopOffset, 5,
                XmNtopAttachment,XmATTACH_WIDGET,
                XmNtopWidget, label4,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 65,
                XmNrightAttachment,XmATTACH_NONE,
                XmNnavigationType, XmTAB_GROUP,
                NULL);

        coordinate_calc_longitude_northing = XtVaCreateManagedWidget("Coordinate_calc lon", 
                xmTextWidgetClass, 
                form,
                XmNeditable,   TRUE,
                XmNcursorPositionVisible, TRUE,
                XmNsensitive, TRUE,
                XmNshadowThickness,    1,
                XmNcolumns, 13,
                XmNwidth, ((14*7)+2),
                XmNmaxLength, 13,
                XmNbackground, colors[0x0f],
                XmNtopOffset, 5,
                XmNtopAttachment,XmATTACH_WIDGET,
                XmNtopWidget, label4,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 195,
                XmNrightAttachment,XmATTACH_NONE,
                XmNnavigationType, XmTAB_GROUP,
                NULL);

//        xastir_snprintf(temp_string, sizeof(temp_string), "%d", temp);
//        XmTextSetString(coordinate_calc_text, temp_string);

        coordinate_calc_result_text = NULL;
        coordinate_calc_result_text = XtVaCreateManagedWidget("Coordinate_calc results",
                xmTextWidgetClass,
                form,
                XmNrows, 5,
                XmNcolumns, 58,
                XmNeditable, FALSE,
                XmNtraversalOn, FALSE,
                XmNeditMode, XmMULTI_LINE_EDIT,
                XmNwordWrap, TRUE,
//                XmNscrollHorizontal, FALSE,
                XmNcursorPositionVisible, FALSE,
                XmNautoShowCursorPosition, True,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, coordinate_calc_zone,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 5,
                XmNrightAttachment, XmATTACH_FORM,
                XmNrightOffset, 5,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        button_clear = XtVaCreateManagedWidget(langcode("COORD004"),
                xmPushButtonGadgetClass, 
                form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, coordinate_calc_result_text,
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
                NULL);
        XtAddCallback(button_clear, XmNactivateCallback, Coordinate_calc_clear_data, coordinate_calc_dialog);

        button_calculate = XtVaCreateManagedWidget(langcode("COORD003"),
                xmPushButtonGadgetClass, 
                form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, coordinate_calc_result_text,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_FORM,
                XmNbottomOffset, 5,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 1,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 2,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(button_calculate, XmNactivateCallback, Coordinate_calc_compute, coordinate_calc_dialog);

        coordinate_calc_button_ok = XtVaCreateManagedWidget(langcode("UNIOP00001"),
                xmPushButtonGadgetClass, 
                form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, coordinate_calc_result_text,
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
                NULL);
        XtAddCallback(coordinate_calc_button_ok, XmNactivateCallback, Coordinate_calc_change_data, coordinate_calc_dialog);
        XtSetSensitive(coordinate_calc_button_ok,FALSE);

        button_cancel = XtVaCreateManagedWidget(langcode("UNIOP00003"),
                xmPushButtonGadgetClass, 
                form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, coordinate_calc_result_text,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_FORM,
                XmNbottomOffset, 5,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 3,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 4,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(button_cancel, XmNactivateCallback, Coordinate_calc_destroy_shell, coordinate_calc_dialog);

        pos_dialog(coordinate_calc_dialog);

        delw = XmInternAtom(XtDisplay(coordinate_calc_dialog),"WM_DELETE_WINDOW", FALSE);
        XmAddWMProtocolCallback(coordinate_calc_dialog, delw, Coordinate_calc_destroy_shell, (XtPointer)coordinate_calc_dialog);

        XtManageChild(form);
        XtManageChild(pane);
        XtPopup(coordinate_calc_dialog,XtGrabNone);
        fix_dialog_size(coordinate_calc_dialog);

        // Move focus to the Close button.  This appears to highlight the
        // button fine, but we're not able to hit the <Enter> key to
        // have that default function happen.  Note:  We _can_ hit the
        // <SPACE> key, and that activates the option.
//        XmUpdateDisplay(coordinate_calc_dialog);
        XmProcessTraversal(button_cancel, XmTRAVERSE_CURRENT);

    } else {
        (void)XRaiseWindow(XtDisplay(coordinate_calc_dialog), XtWindow(coordinate_calc_dialog));
    }

    // Fill in the latitude values if they're available
    if ( (coordinate_calc_array.calling_dialog != NULL)
            && (coordinate_calc_array.input_lat_deg != NULL)
            && (coordinate_calc_array.input_lat_min != NULL)
            && (coordinate_calc_array.input_lat_dir != NULL) )
    {
        char *str_ptr1;
        char *str_ptr2;
        char *str_ptr3;

        str_ptr1 = XmTextGetString(coordinate_calc_array.input_lat_deg);
        str_ptr2 = XmTextGetString(coordinate_calc_array.input_lat_min);
        str_ptr3 = XmTextGetString(coordinate_calc_array.input_lat_dir);

        xastir_snprintf(temp_string, sizeof(temp_string), "%s %s%s",
            str_ptr1, str_ptr2, str_ptr3);
        XmTextSetString(coordinate_calc_latitude_easting, temp_string);
        //printf("String: %s\n", temp_string);
        // We're done with these variables.  Free the space.
        XtFree(str_ptr1);
        XtFree(str_ptr2);
        XtFree(str_ptr3);
    }

    // Fill in the longitude values if they're available
    if ( (coordinate_calc_array.calling_dialog != NULL)
            && (coordinate_calc_array.input_lon_deg != NULL)
            && (coordinate_calc_array.input_lon_min != NULL)
            && (coordinate_calc_array.input_lon_dir != NULL) )
    {
        char *str_ptr1;
        char *str_ptr2;
        char *str_ptr3;

        str_ptr1 = XmTextGetString(coordinate_calc_array.input_lon_deg);
        str_ptr2 = XmTextGetString(coordinate_calc_array.input_lon_min);
        str_ptr3 = XmTextGetString(coordinate_calc_array.input_lon_dir);

        xastir_snprintf(temp_string, sizeof(temp_string), "%s %s%s",
            str_ptr1, str_ptr2, str_ptr3);
        XmTextSetString(coordinate_calc_longitude_northing, temp_string);
        //printf("String: %s\n", temp_string);
        // We're done with these variables.  Free the space.
        XtFree(str_ptr1);
        XtFree(str_ptr2);
        XtFree(str_ptr3);
    }
}





////////////////////////////////////////////////////////////////////////////////////////////////////





void HandlePendingEvents( XtAppContext app) {
    XEvent event;

    while(XtAppPending(app)) {
        XtAppNextEvent(app,&event);
        (void)XtDispatchEvent(&event);
    }
}





Boolean unbusy_cursor(XtPointer clientdata) {
    Widget w = (Widget)clientdata;

    (void)XUndefineCursor(XtDisplay(w),XtWindow(w));
    return((Boolean)TRUE);
}





void busy_cursor(Widget w) {
    static Cursor cs = (Cursor)NULL;

    if(!cs)
        cs=XCreateFontCursor(XtDisplay(w),XC_watch);

    (void)XDefineCursor(XtDisplay(w),XtWindow(w),cs);
    (void)XFlush(XtDisplay(w));
    (void)XtAppAddWorkProc(XtWidgetToApplicationContext(w),unbusy_cursor,(XtPointer)w);
}





// This function:
// Draws the map data into "pixmap", copies "pixmap" to
// "pixmap_alerts", draws alerts into "pixmap_alerts", copies
// "pixmap_alerts" to "pixmap_final", draws symbols/tracks into
// "pixmap_final" via a call to display_file().
//
// Other functions which call this function are responsible for
// copying the image from pixmap_final() to the screen's drawing
// area.
//
void create_image(Widget w) {
    Dimension width, height, margin_width, margin_height;
    long lat_offset_temp;
    long long_offset_temp;
    char temp_course[20];
    unsigned char   unit_type;
    char medium_dashed[2] = {(char)5,(char)5};
    long pos1_lat, pos1_lon, pos2_lat, pos2_lon;


    //busy_cursor(w);
    busy_cursor(appshell);

    if (debug_level & 4)
        printf("Create image start\n");

    // If we're in the middle of ID'ing, wait a bit.
    if (pending_ID_message)
        usleep(2000000);

    /* First get the various dimensions */
    XtVaGetValues(w,
              XmNwidth,         &width,
              XmNheight,        &height,
              XmNmarginWidth,   &margin_width,
              XmNmarginHeight,  &margin_height,
              XmNunitType,      &unit_type,
              0);

    (void)XSetDashes(XtDisplay(w), gc, 0, medium_dashed , 2);

    screen_width  = (long)width;
    screen_height = (long)height;
    long_offset_temp = x_long_offset = mid_x_long_offset - (screen_width  * scale_x / 2);  // NW corner
    lat_offset_temp  = y_lat_offset  = mid_y_lat_offset  - (screen_height * scale_y / 2);

    /* map default background color */
    switch (map_background_color){
        case 0 :
            colors[0xfd] = (int)GetPixelByName(appshell,"gray73");
            break;

        case 1 :
            colors[0xfd] = (int)GetPixelByName(w,"MistyRose");
            break;

        case 2 :
            colors[0xfd] = (int)GetPixelByName(w,"NavyBlue");
            break;

        case 3 :
            colors[0xfd] = (int)GetPixelByName(w,"SteelBlue");
            break;

        case 4 :
            colors[0xfd] = (int)GetPixelByName(w,"MediumSeaGreen");
            break;

        case 5 :
            colors[0xfd] = (int)GetPixelByName(w,"PaleGreen");
            break;

        case 6 :
            colors[0xfd] = (int)GetPixelByName(w,"PaleGoldenrod");
            break;

        case 7 :
            colors[0xfd] = (int)GetPixelByName(w,"LightGoldenrodYellow");
            break;

        case 8 :
            colors[0xfd] = (int)GetPixelByName(w,"RosyBrown");
            break;

        case 9 :
            colors[0xfd] = (int)GetPixelByName(w,"firebrick");
            break;

        case 10 :
            colors[0xfd] = (int)GetPixelByName(w,"white");
            break;

        case 11 :
            colors[0xfd] = (int)GetPixelByName(w, "black");
            break;

        default:
            colors[0xfd] = (int)GetPixelByName(appshell,"gray73");
            map_background_color=0;
            break;
    }

    (void)XSetForeground(XtDisplay(w),gc,colors[0xfd]);
    (void)XSetBackground(XtDisplay(w),gc,colors[0xfd]);

    (void)XFillRectangle(XtDisplay(w), pixmap,gc,0,0,screen_width,screen_height);

    statusline(langcode("BBARSTA003"),1);       // Loading Maps

#ifdef HAVE_IMAGEMAGICK
    //
    //  If tigermaps are enabled, then load them.
    //
    if (tiger_flag && !disable_all_maps)
        draw_tiger_map(w);
#endif // HAVE_IMAGEMAGICK

    if (display_up_first != 0) {
        if (map_auto_maps && !disable_all_maps)
            load_auto_maps(w,AUTO_MAP_DIR);
        else if (!disable_all_maps)
            load_maps(w);
    }

    if (!wx_alert_style)
        statusline(langcode("BBARSTA034"),1);

    // Update to screen
//    (void)XCopyArea(XtDisplay(da),pixmap,XtWindow(da),gc,0,0,screen_width,screen_height,0,0);

    /* copy map data to alert pixmap */
    (void)XCopyArea(XtDisplay(w),pixmap,pixmap_alerts,gc,0,0,screen_width,screen_height,0,0);

    if (!wx_alert_style && !disable_all_maps)
        load_alert_maps(w, ALERT_MAP_DIR);  // These write onto pixmap_alerts

    // Update to screen
//    (void)XCopyArea(XtDisplay(da),pixmap_alerts,XtWindow(da),gc,0,0,screen_width,screen_height,0,0);

    /* copy map and alert data to final pixmap */
    (void)XCopyArea(XtDisplay(w),pixmap_alerts,pixmap_final,gc,0,0,screen_width,screen_height,0,0);

    wx_alert_update_list();

    /* Compute distance */
    if (lat_offset_temp<0l)
        lat_offset_temp=0l;                     // max 90°N
    else
        if (lat_offset_temp>64800000l)
            lat_offset_temp=64800000l;          // max 90°S

    if(long_offset_temp<0l)
        long_offset_temp=0l;                    // max 180°W
    else
        if (long_offset_temp>129600000l)
            long_offset_temp=129600000l;        // max 180°E

    pos1_lat = lat_offset_temp;
    pos1_lon = long_offset_temp;
    pos2_lat = lat_offset_temp;      // ??
    pos2_lon = long_offset_temp+(50.0*scale_x);

//    long_offset_temp = long_offset_temp+(50*scale_x);  // ??

    if(pos2_lat < 0l)     // ??
        pos2_lat = 0l;
    else
        if (pos2_lat > 64799999l)
            pos2_lat = 64799999l;

    if (pos2_lon < 0l)
        pos2_lon = 0l;
    else
        if (pos2_lon > 129599999l)
            pos2_lon = 129599999l;

    // Get distance in nautical miles
    x_screen_distance = (float)calc_distance_course(pos1_lat,
        pos1_lon,
        pos2_lat,
        pos2_lon,
        temp_course,
        sizeof(temp_course) );

    draw_grid(w);                       // Draw grid if enabled

    display_file(w);                    // display stations (symbols, info, trails)

    last_alert_redraw=sec_now();        // set last time of screen redraw

    if (debug_level & 4)
        printf("Create image stop\n");
}





// Routine used to refresh image WITHOUT reading regular map files
// from disk.  It starts with the map data already in "pixmap",
// copies "pixmap" to "pixmap_alerts", draws alerts into
// "pixmap_alerts", copies "pixmap_alerts" to "pixmap_final", draws
// symbols/tracks into "pixmap_final" via a call to display_file().
//
// Other functions which call this function are responsible for
// copying the image from pixmap_final() to the screen's drawing
// area.
//
void refresh_image(Widget w) {
    Dimension width, height, margin_width, margin_height;
    long lat_offset_temp;
    long long_offset_temp;
    char temp_course[20];
    unsigned char   unit_type;
    char medium_dashed[2] = {(char)5,(char)5};
    long pos1_lat, pos1_lon, pos2_lat, pos2_lon;


    //busy_cursor(w);
    busy_cursor(appshell);

    if (debug_level & 4)
        printf("Refresh image start\n");

    // If we're in the middle of ID'ing, wait a bit.
    if (pending_ID_message)
        usleep(2000000);

    /* First get the various dimensions */
    XtVaGetValues(w,
              XmNwidth,         &width,
              XmNheight,        &height,
              XmNmarginWidth,   &margin_width,
              XmNmarginHeight,  &margin_height,
              XmNunitType,      &unit_type,
              0);

    (void)XSetDashes(XtDisplay(w), gc, 0, medium_dashed , 2);

    screen_width  = (long)width;
    screen_height = (long)height;

    long_offset_temp = x_long_offset = mid_x_long_offset - (screen_width * scale_x / 2);
    y_lat_offset     = mid_y_lat_offset - (screen_height * scale_y / 2);
    lat_offset_temp  = mid_y_lat_offset;

    (void)XSetForeground(XtDisplay(w),gc,colors[0xfd]);
    (void)XSetBackground(XtDisplay(w),gc,colors[0xfd]);

    /* copy over map data to alert pixmap */
    (void)XCopyArea(XtDisplay(w),pixmap,pixmap_alerts,gc,0,0,screen_width,screen_height,0,0);

    if (!wx_alert_style && !disable_all_maps) {
            statusline(langcode("BBARSTA034"),1);
            load_alert_maps(w, ALERT_MAP_DIR);  // These write onto pixmap_alerts
    }

    /* copy over map and alert data to final pixmap */
    (void)XCopyArea(XtDisplay(w),pixmap_alerts,pixmap_final,gc,0,0,screen_width,screen_height,0,0);

//    statusline("Weather Alert Maps Loaded",1);

    wx_alert_update_list();

    /* Compute distance */
    if (lat_offset_temp<0l)
        lat_offset_temp=0l;                     // max 90°N
    else
        if (lat_offset_temp>64800000l)
            lat_offset_temp=64800000l;          // max 90°S

    if(long_offset_temp<0l)
        long_offset_temp=0l;                    // max 180°W
    else
        if (long_offset_temp>129600000l)
            long_offset_temp=129600000l;        // max 180°E

    pos1_lat = lat_offset_temp;
    pos1_lon = long_offset_temp;
    pos2_lat = lat_offset_temp;      // ??
    pos2_lon = long_offset_temp+(50.0*scale_x);

//    long_offset_temp = long_offset_temp+(50*scale_x);  // ??

    if(pos2_lat < 0l)     // ??
        pos2_lat = 0l;
    else
        if (pos2_lat > 64799999l)
            pos2_lat = 64799999l;

    if (pos2_lon < 0l)
        pos2_lon = 0l;
    else
        if (pos2_lon > 129599999l)
            pos2_lon = 129599999l;

    // Get distance in nautical miles
    x_screen_distance = (float)calc_distance_course(pos1_lat,
        pos1_lon,
        pos2_lat,
        pos2_lon,
        temp_course,
        sizeof(temp_course) );

    // Draw grid if enabled
    draw_grid(w);

    /* display icons */
    display_file(w);

    /* set last time of screen redraw*/
    last_alert_redraw=sec_now();

    if (debug_level & 4)
        printf("Refresh image stop\n");
}





// And this function is even faster yet.  It snags "pixmap_alerts",
// which already has map and alert data drawn into it, copies it to
// pixmap_final, then draws symbols and tracks on top of it.  When
// done it copies the image to the drawing area, making it visible.
void redraw_symbols(Widget w) {

    // If we're in the middle of ID'ing, wait a bit.
    if (pending_ID_message)
        usleep(2000000);

    /* copy over map and alert data to final pixmap */
    if(!wait_to_redraw) {
        (void)XCopyArea(XtDisplay(w),pixmap_alerts,pixmap_final,gc,0,0,screen_width,screen_height,0,0);
        draw_grid(w);           // draw grid if enabled
        display_file(w);        // display stations (symbols, info, trails)
        (void)XCopyArea(XtDisplay(w),pixmap_final,XtWindow(w),gc,0,0,screen_width,screen_height,0,0);
    }
else
    printf("wait_to_redraw\n");
}





static void TrackMouse( /*@unused@*/ Widget w, XtPointer clientData, XEvent *event, /*@unused@*/ Boolean *flag) {
    char my_text[50];
    char str_lat[20];
    char str_long[20];
    long x, y;

    Widget textarea = (Widget) clientData;

    x = (mid_x_long_offset - ((screen_width  * scale_x)/2) + (event->xmotion.x * scale_x));
    y = (mid_y_lat_offset  - ((screen_height * scale_y)/2) + (event->xmotion.y * scale_y));

    if (x < 0)
        x = 0l;                 // 180°W

    if (x > 129600000l)
        x = 129600000l;         // 180°E

    if (y < 0)
        y = 0l;                 //  90°N

    if (y > 64800000l)
        y = 64800000l;          //  90°S

    if (coordinate_system == USE_UTM) {
        // Create a UTM string from coordinate in Xastir coordinate
        // system.
        convert_xastir_to_UTM_str(my_text, sizeof(my_text), x, y);
    }
    else {
        // Create a lat/lon string from coordinate in Xastir
        // coordinate system.
        if (coordinate_system == USE_DDDDDD) {
            convert_lat_l2s(y, str_lat, sizeof(str_lat), CONVERT_DEC_DEG);
            convert_lon_l2s(x, str_long, sizeof(str_long), CONVERT_DEC_DEG);
        } else if (coordinate_system == USE_DDMMSS) {
            convert_lat_l2s(y, str_lat, sizeof(str_lat), CONVERT_DMS_NORMAL);
            convert_lon_l2s(x, str_long, sizeof(str_long), CONVERT_DMS_NORMAL);
            str_lat[2]='°'; str_long[3]='°';
            str_lat[5]='\''; str_long[6]='\'';
        } else {    // Assume coordinate_system == USE_DDMMMM
            convert_lat_l2s(y, str_lat, sizeof(str_lat), CONVERT_HP_NORMAL);
            convert_lon_l2s(x, str_long, sizeof(str_long), CONVERT_HP_NORMAL);
            str_lat[2]='°'; str_long[3]='°';
        }
        xastir_snprintf(my_text, sizeof(my_text), "%s  %s", str_lat, str_long);
    }

    strcat(my_text,"  ");
    strcat(my_text,sec_to_loc(x,y));
    XmTextFieldSetString(textarea, my_text);
    XtManageChild(textarea);
}





static void ClearTrackMouse( /*@unused@*/ Widget w, XtPointer clientData, /*@unused@*/ XEvent *event, /*@unused@*/ Boolean *flag) {
// N7TAP: In my opinion, it is handy to have the cursor position still displayed
//        in the xastir window when I switch to another (like to write it down...)
//    Widget textarea = (Widget) clientData;
//    XmTextFieldSetString(textarea," ");
//    XtManageChild(textarea);
}





/*
 *  Delete tracks of all stations
 */
void Tracks_All_Clear( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer callData) {
    DataRow *p_station;

    p_station = n_first;
    while (p_station != 0) {
        if (delete_trail(p_station))
        redraw_on_new_data=2;
        p_station = p_station->n_next;
    }
}





/*
 *  Clear out object/item history log file
 */
void Object_History_Clear( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer callData) {
    char *file;
    FILE *f;

    file = get_user_base_dir("config/object.log");

    f=fopen(file,"w");
    if (f!=NULL) {
        (void)fclose(f);

        if (debug_level & 1)
            printf("Clearing Object/Item history file...\n");
    }
    else {
        printf("Couldn't open file for writing: %s\n", file);
    }
}





/*
 *  Display text in the status line, text is removed after timeout
 */
void statusline(char *status_text,int update) {

    XmTextFieldSetString (text, status_text);
    last_statusline = sec_now();
    if (update != 0)
        XmUpdateDisplay(text);          // do an immediate update
}





//
// Check for statusline timeout and replace statusline text with a
// station identification message.
//
// ID was requested so that Xastir could be used for a live fast-scan
// TV display over amateur radio without having to identify the
// station in some other manner.  As long as we guarantee that we'll
// see this line for a few seconds each 10 minutes (30 minutes for
// Canada), we should be within the ID rules.
//
void check_statusline_timeout(void) {
    char status_text[100];
    int id_interval = (int)(9.5 * 60);
//    int id_interval = (int)(1 * 5);  // Debug


    if ( (last_statusline != 0
            && (last_statusline < sec_now() - STATUSLINE_ACTIVE))
        || (last_id_time < sec_now() - id_interval) ) {


        // We save last_id_time and identify for a few seconds if
        // we haven't identified for the last nine minutes or so.

        xastir_snprintf(status_text,
            sizeof(status_text),
            langcode ("BBARSTA040"),
            my_callsign);

        XmTextFieldSetString(text, status_text);

        if (last_id_time < sec_now() - id_interval) {
            popup_ID_message(langcode("BBARSTA040"),status_text);
#ifdef HAVE_FESTIVAL
            if (festival_speak_ID) {
                char my_speech_callsign[100];

                strcpy(my_speech_callsign,my_callsign);
                spell_it_out(my_speech_callsign);
                xastir_snprintf(status_text,
                    sizeof(status_text),
                    langcode ("BBARSTA040"),
                    my_speech_callsign);
                SayText(status_text);
            }
#endif
        }

        last_statusline = 0;	// now inactive

        // Guarantee that the ID text will be viewable for a few
        // seconds if we haven't identified recently.  Note that the
        // sleep statement puts the entire main thread to sleep for
        // that amount of time.  The application will be unresponsive
        // during that time.

        if (last_id_time < (sec_now() - (9 * 60))) {
            //printf("Identifying at nine minutes\n");
            //sleep(1);
        }

        last_id_time = sec_now();
    }
}





/*
 *  Display current zoom factor
 *
 *  DK7IN: we should find a new measure, we now have different x/y scaling!
 *         I now only use the y value
 */
void display_zoom_status(void) {
    char zoom[30];
    char siz_str[6];

    if (scale_y < 9000)
        xastir_snprintf(siz_str, sizeof(siz_str), "%ld", scale_y);
    else
        xastir_snprintf(siz_str, sizeof(siz_str), "%ldk", scale_y/1024);

    if (track_station_on == 1)
        xastir_snprintf(zoom, sizeof(zoom), langcode("BBARZM0002"), siz_str);
    else
        xastir_snprintf(zoom, sizeof(zoom), langcode("BBARZM0001"), siz_str);

    XmTextFieldSetString(text4,zoom);
}





void Change_debug_level_destroy_shell( /*@unused@*/ Widget widget, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    Widget shell = (Widget) clientData;
    XtPopdown(shell);
    XtDestroyWidget(shell);
    change_debug_level_dialog = (Widget)NULL;
}





void Change_debug_level_change_data(Widget widget, XtPointer clientData, XtPointer callData) {
    char *temp;
    char temp_string[10];

    // Small memory leak in below statement:
    //strcpy(altnet_call, XmTextGetString(debug_level_text));
    // Change to:
    temp = XmTextGetString(debug_level_text);

    debug_level = atoi(temp);
    if ( (debug_level < 0) || (debug_level > 32767) )
        debug_level = 0;

    XtFree(temp);

    // Fill in the current value of debug_level
    xastir_snprintf(temp_string, sizeof(temp_string), "%d", debug_level);
    XmTextSetString(debug_level_text, temp_string);

//    Change_debug_level_destroy_shell(widget,clientData,callData);
}





void Change_Debug_Level(Widget w, XtPointer clientData, XtPointer callData) {
    static Widget  pane, my_form, button_ok, button_close;
    Atom delw;
    Arg al[2];                    /* Arg List */
    register unsigned int ac = 0;           /* Arg Count */
    char temp_string[10];

    if (!change_debug_level_dialog) {
        change_debug_level_dialog = XtVaCreatePopupShell(langcode("PULDNFI007"),
                xmDialogShellWidgetClass,
                Global.top,
                XmNdeleteResponse,XmDESTROY,
                XmNdefaultPosition, FALSE,
                NULL);

        pane = XtVaCreateWidget("Change Debug Level pane",
                xmPanedWindowWidgetClass, 
                change_debug_level_dialog,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        my_form =  XtVaCreateWidget("Change Debug Level my_form",
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
        XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
        XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;


        debug_level_text = XtVaCreateManagedWidget("Change_Debug_Level debug text", 
                xmTextWidgetClass, 
                my_form,
                XmNeditable,   TRUE,
                XmNcursorPositionVisible, TRUE,
                XmNsensitive, TRUE,
                XmNshadowThickness,    1,
                XmNcolumns, 4,
                XmNwidth, ((5*7)+2),
                XmNmaxLength, 4,
                XmNbackground, colors[0x0f],
                XmNtopOffset, 5,
                XmNtopAttachment,XmATTACH_FORM,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 2,
                XmNrightAttachment,XmATTACH_POSITION,
                XmNrightPosition, 3,
                XmNnavigationType, XmTAB_GROUP,
                NULL);

        // Fill in the current value of debug_level
        xastir_snprintf(temp_string, sizeof(temp_string), "%d", debug_level);
        XmTextSetString(debug_level_text, temp_string);

        button_ok = XtVaCreateManagedWidget(langcode("UNIOP00001"),
                xmPushButtonGadgetClass, 
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, debug_level_text,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_FORM,
                XmNbottomOffset, 5,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 1,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 2,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);


        button_close = XtVaCreateManagedWidget(langcode("UNIOP00003"),
                xmPushButtonGadgetClass, 
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, debug_level_text,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_FORM,
                XmNbottomOffset, 5,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 3,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 4,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        XtAddCallback(button_ok, XmNactivateCallback, Change_debug_level_change_data, change_debug_level_dialog);
        XtAddCallback(button_close, XmNactivateCallback, Change_debug_level_destroy_shell, change_debug_level_dialog);

        pos_dialog(change_debug_level_dialog);

        delw = XmInternAtom(XtDisplay(change_debug_level_dialog),"WM_DELETE_WINDOW", FALSE);
        XmAddWMProtocolCallback(change_debug_level_dialog, delw, Change_debug_level_destroy_shell, (XtPointer)change_debug_level_dialog);

        XtManageChild(my_form);
        XtManageChild(pane);

        XtPopup(change_debug_level_dialog,XtGrabNone);
        fix_dialog_size(change_debug_level_dialog);

        // Move focus to the Close button.  This appears to highlight the
        // button fine, but we're not able to hit the <Enter> key to
        // have that default function happen.  Note:  We _can_ hit the
        // <SPACE> key, and that activates the option.
//        XmUpdateDisplay(change_debug_level_dialog);
        XmProcessTraversal(button_close, XmTRAVERSE_CURRENT);

    } else
        (void)XRaiseWindow(XtDisplay(change_debug_level_dialog), XtWindow(change_debug_level_dialog));
}





#if !defined(NO_GRAPHICS) && defined(HAVE_IMAGEMAGICK)
void Gamma_adjust_destroy_shell( /*@unused@*/ Widget widget, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    Widget shell = (Widget) clientData;
    XtPopdown(shell);
    XtDestroyWidget(shell);
    gamma_adjust_dialog = (Widget)NULL;
}





void Gamma_adjust_change_data(Widget widget, XtPointer clientData, XtPointer callData) {
    char *temp;
    char temp_string[10];

    temp = XmTextGetString(gamma_adjust_text);

    imagemagick_gamma_adjust = atof(temp);
    if (imagemagick_gamma_adjust < -9.9)
        imagemagick_gamma_adjust = -9.9;
    else if (imagemagick_gamma_adjust > 9.9)
        imagemagick_gamma_adjust = 9.9;

    XtFree(temp);

    xastir_snprintf(temp_string, sizeof(temp_string), "%+.1f", imagemagick_gamma_adjust);
    XmTextSetString(gamma_adjust_text, temp_string);

    create_image(da);
    XCopyArea(XtDisplay(da),pixmap_final,XtWindow(da),gc,0,0,screen_width,screen_height,0,0);
}





void Gamma_adjust(Widget w, XtPointer clientData, XtPointer callData) {
    static Widget  pane, my_form, button_ok, button_close;
    Atom delw;
    Arg al[2];
    register unsigned int ac = 0;
    char temp_string[10];

    if (!gamma_adjust_dialog) {
        // Gamma Correction
        gamma_adjust_dialog = XtVaCreatePopupShell(langcode("GAMMA002"),
                xmDialogShellWidgetClass, Global.top,
                XmNdeleteResponse,        XmDESTROY,
                XmNdefaultPosition,       FALSE,
                NULL);

        pane = XtVaCreateWidget("Adjust Gamma pane",
                xmPanedWindowWidgetClass, gamma_adjust_dialog,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        my_form =  XtVaCreateWidget("Adjust Gamma my_form",
                xmFormWidgetClass,  pane,
                XmNfractionBase,    5,
                XmNautoUnmanage,    FALSE,
                XmNshadowThickness, 1,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        ac=0;
        XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
        XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;


        gamma_adjust_text = XtVaCreateManagedWidget("Adjust Gamma text",
                xmTextWidgetClass,        my_form,
                XmNeditable,              TRUE,
                XmNcursorPositionVisible, TRUE,
                XmNsensitive,             TRUE,
                XmNshadowThickness,       1,
                XmNcolumns,               4,
                XmNwidth,                 4*10,
                XmNmaxLength,             4,
                XmNbackground,            colors[0x0f],
                XmNtopOffset,             5,
                XmNtopAttachment,         XmATTACH_FORM,
                XmNbottomAttachment,      XmATTACH_NONE,
                XmNleftAttachment,        XmATTACH_POSITION,
                XmNleftPosition,          2,
                XmNrightAttachment,       XmATTACH_POSITION,
                XmNrightPosition,         3,
                XmNnavigationType,        XmTAB_GROUP,
                NULL);

        xastir_snprintf(temp_string, sizeof(temp_string), "%+.1f", imagemagick_gamma_adjust);
        XmTextSetString(gamma_adjust_text, temp_string);

        button_ok = XtVaCreateManagedWidget(langcode("UNIOP00001"),
                xmPushButtonGadgetClass, my_form,
                XmNtopAttachment,        XmATTACH_WIDGET,
                XmNtopWidget,            gamma_adjust_text,
                XmNtopOffset,            5,
                XmNbottomAttachment,     XmATTACH_FORM,
                XmNbottomOffset,         5,
                XmNleftAttachment,       XmATTACH_POSITION,
                XmNleftPosition,         1,
                XmNrightAttachment,      XmATTACH_POSITION,
                XmNrightPosition,        2,
                XmNnavigationType,       XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        button_close = XtVaCreateManagedWidget(langcode("UNIOP00003"),
                xmPushButtonGadgetClass, my_form,
                XmNtopAttachment,        XmATTACH_WIDGET,
                XmNtopWidget,            gamma_adjust_text,
                XmNtopOffset,            5,
                XmNbottomAttachment,     XmATTACH_FORM,
                XmNbottomOffset,         5,
                XmNleftAttachment,       XmATTACH_POSITION,
                XmNleftPosition,         3,
                XmNrightAttachment,      XmATTACH_POSITION,
                XmNrightPosition,        4,
                XmNnavigationType,       XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        XtAddCallback(button_ok,
                      XmNactivateCallback, Gamma_adjust_change_data,   gamma_adjust_dialog);
        XtAddCallback(button_close,
                      XmNactivateCallback, Gamma_adjust_destroy_shell, gamma_adjust_dialog);

        pos_dialog(gamma_adjust_dialog);

        delw = XmInternAtom(XtDisplay(gamma_adjust_dialog), "WM_DELETE_WINDOW", FALSE);
        XmAddWMProtocolCallback(gamma_adjust_dialog,
                                delw, Gamma_adjust_destroy_shell, (XtPointer)gamma_adjust_dialog);

        XtManageChild(my_form);
        XtManageChild(pane);

        XtPopup(gamma_adjust_dialog, XtGrabNone);
        fix_dialog_size(gamma_adjust_dialog);

        // Move focus to the Close button.  This appears to highlight the
        // button fine, but we're not able to hit the <Enter> key to
        // have that default function happen.  Note:  We _can_ hit the
        // <SPACE> key, and that activates the option.
//        XmUpdateDisplay(gamma_adjust_dialog);
        XmProcessTraversal(button_close, XmTRAVERSE_CURRENT);

    } else
        (void)XRaiseWindow(XtDisplay(gamma_adjust_dialog), XtWindow(gamma_adjust_dialog));
}
#endif





void Compute_Uptime(Widget w, XtPointer clientData, XtPointer callData) {
    char temp[200];
    unsigned long runtime;
    int days, hours, minutes, seconds;
    char Days[6];
    char Hours[7];
    char Minutes[9];
    char Seconds[9];

    runtime = sec_now() - program_start_time;
    days = runtime / 86400;
    runtime = runtime - (days * 86400);
    hours = runtime / 3600;
    runtime = runtime - (hours * 3600);
    minutes = runtime / 60;
    seconds = runtime - (minutes * 60);

    if (days == 1)
        strcpy(Days,"Day");
    else
        strcpy(Days,"Days");

    if (hours == 1)
        strcpy(Hours,"Hour");
    else
        strcpy(Hours,"Hours");

   if (minutes == 1)
        strcpy(Minutes,"Minute");
    else
        strcpy(Minutes,"Minutes");

   if (seconds == 1)
        strcpy(Seconds,"Second");
    else
        strcpy(Seconds,"Seconds");


    if (days != 0) {
        xastir_snprintf(temp, sizeof(temp), "%d %s, %d %s, %d %s, %d %s",
            days, Days, hours, Hours, minutes, Minutes, seconds, Seconds);
    } else if (hours != 0) {
        xastir_snprintf(temp, sizeof(temp), "%d %s, %d %s, %d %s",
           hours, Hours, minutes, Minutes, seconds, Seconds);
    } else if (minutes != 0) {
        xastir_snprintf(temp, sizeof(temp), "%d %s, %d %s", minutes, Minutes, seconds, Seconds);
    } else {
        xastir_snprintf(temp, sizeof(temp), "%d %s", seconds, Seconds);
    }
    popup_message(langcode("PULDNVI014"),temp);
}





void Mouse_button_handler (Widget w, Widget popup, XButtonEvent *event) {

    // Snag the current pointer position
    input_x = event->x;
    input_y = event->y;

    if (event->type == ButtonPress) {
        //printf("Mouse_button_handler, button pressed %d\n", event->button);
    }

    if (event->type == ButtonRelease) {
        //printf("Mouse_button_handler, button released %d\n", event->button);
        return;
    }

    if (event->button != Button3) {
        //printf("Pressed a mouse button, but not Button3: %x\n",event->button);
        return;
    }

    // Right mouse button press
    menu_x=input_x;
    menu_y=input_y;
    if (right_menu_popup != NULL) { // If popup menu defined

        // Bring up the popup menu
        XmMenuPosition(right_menu_popup,(XButtonPressedEvent *)event);
        XtManageChild(right_menu_popup);

        // Check whether any modifiers are pressed.
        // If so, pop up a warning message.
        if ( (event->state != 0) && warn_about_mouse_modifiers) {
            popup_message(langcode("POPUPMA023"),langcode("POPUPMA024"));
        }
    }
}




 
void menu_link_for_mouse_menu(Widget w, XtPointer clientData, XtPointer callData) {
    if (right_menu_popup!=NULL) {
        //XmMenuPosition(right_menu_popup,(XButtonPressedEvent *)event);
        XtManageChild(right_menu_popup);
    }
}





void Snapshots_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    // Whether we're setting or unsetting it, set the timer such
    // that a snapshot will occur immediately once the button is set
    // again.
    last_snapshot = 0;

    if(state->set)
        snapshots_enabled = atoi(which);
    else
        snapshots_enabled = 0;
}

 



void create_appshell( /*@unused@*/ Display *display, char *app_name, /*@unused@*/ int app_argc, char ** app_argv) {
    Atom WM_DELETE_WINDOW;
    Widget children[8];         /* Children to manage */
    Arg al[64];                 /* Arg List */
    register unsigned int ac;   /* Arg Count */
    /*popup menu widgets */
    Widget zoom_in, zoom_out, zoom_sub, zoom_level, zl1, zl2, zl3, zl4, zl5, zl6, zl7, zl8, zl9;
    Widget pan_ctr, last_loc, station_info, set_object, modify_object;
    Widget pan_up, pan_down, pan_left, pan_right;
    /*menu widgets */
    Widget sep;
    Widget filepane, configpane, exitpane, mappane, viewpane,
        stationspane, messagepane, ifacepane, helppane,
        symbol_filter_pane, info_filter_pane;

    Widget trackme_frame, measure_frame, move_frame, display_button,
        track_button, download_trail_button,
        symbols_button, station_trails_button,
        station_clear_button, tracks_clear_button, object_history_clear_button, uptime_button,
        save_button,file_button, open_file_button, exit_button, really_exit_button,
        view_button, view_messages_button, bullet_button, packet_data_button, mobile_button, stations_button,
        localstations_button, laststations_button, objectstations_button, objectmystations_button,
        weather_button, wx_station_button, locate_button, locate_place_button, jump_button, alert_button,
        config_button, defaults_button, coordinates_button, station_button,
        map_disable_button, map_button, map_auto_button, map_chooser_button, map_grid_button,
        map_levels_button, map_labels_button, map_fill_button, coordinate_calculator_button,
        Map_background_color_Pane, map_background_button, map_pointer_menu_button,
#if !defined(NO_GRAPHICS)
#if defined(HAVE_GEOTIFF)
        Geotiff_intensity_Pane, geotiff_intensity_button,
#endif
#if defined(HAVE_IMAGEMAGICK)
        gamma_adjust_button, tiger_config_button,
#endif
#endif
        Map_station_label_Pane, map_station_label_button,
        map_wx_alerts_button,
        units_choice_button,
        device_config_button,
        iface_button, iface_connect_button, tnc_logging,
        transmit_disable_toggle,
        net_logging, igate_logging, wx_logging, enable_snapshots, print_button,
        test_button, debug_level_button, aa_button, speech_button,
        smart_beacon_button, auto_msg_set_button,
        message_button, send_message_to_button, open_messages_group_button,
        clear_messages_button,
        General_q_button, IGate_q_button, WX_q_button,
        symbol_filter_button, info_filter_button,

#ifdef ARROWS
        pan_up_menu, pan_down_menu, pan_left_menu, pan_right_menu,
#endif // ARROWS

        help_button, help_about, help_help;
    char *title, *t;

    if(debug_level & 8)
        printf("Create appshell start\n");


    t = _("X Amateur Station Tracking and Information Reporting");
    title = (char *)malloc(strlen(t) + 42 + strlen(PACKAGE));
    strcpy(title, "XASTIR ");
    strcat(title, " - ");
    strcat(title, t);
    strcat(title, " @ ");
    (void)gethostname(&title[strlen(title)], 28);

    // Allocate a couple of colors that we'll need before we get
    // around to calling create_gc(), which creates the rest.
    //
    colors[0x08] = (int)GetPixelByName(Global.top,"black");
    colors[0x0c] = (int)GetPixelByName(Global.top,"red");
    colors[0xff] = (int)GetPixelByName(Global.top,"gray73");

    ac = 0;
    XtSetArg(al[ac], XmNallowShellResize, TRUE);            ac++;
    XtSetArg(al[ac], XmNtitle,            title);           ac++;
    XtSetArg(al[ac], XmNargv,             app_argv);        ac++;
    XtSetArg(al[ac], XmNminWidth,         640);             ac++;
    XtSetArg(al[ac], XmNminHeight,        380);             ac++;
    XtSetArg(al[ac], XmNdefaultPosition,  FALSE);           ac++;
    XtSetArg(al[ac], XmNforeground,       MY_FG_COLOR);     ac++;
    XtSetArg(al[ac], XmNbackground,       MY_BG_COLOR);     ac++;

    appshell= XtCreatePopupShell (app_name,
            topLevelShellWidgetClass,
            Global.top,
            al,
            ac);

    form = XtVaCreateWidget("create_appshell form",
            xmFormWidgetClass,
            appshell,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    /* Menu Bar */
    ac = 0;
    XtSetArg(al[ac],XmNshadowThickness, 1);                     ac++;
    XtSetArg(al[ac],XmNalignment,       XmALIGNMENT_BEGINNING); ac++;
    XtSetArg(al[ac],XmNleftAttachment,  XmATTACH_FORM);         ac++;
    XtSetArg(al[ac],XmNtopAttachment,   XmATTACH_FORM);         ac++;
    XtSetArg(al[ac],XmNrightAttachment, XmATTACH_NONE);         ac++;
    XtSetArg(al[ac],XmNbottomAttachment,XmATTACH_NONE);         ac++;
    XtSetArg(al[ac],XmNforeground,      MY_FG_COLOR);           ac++;
    XtSetArg(al[ac],XmNbackground,      MY_BG_COLOR);           ac++;

    menubar = XmCreateMenuBar(form,
            "create_appshell menubar",
            al,
            ac);

    /*set args for color */
    ac = 0;
    XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
    XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
    XtSetArg(al[ac], XmNtearOffModel, XmTEAR_OFF_ENABLED); ac++;

    /* menu bar */
    filepane    = XmCreatePulldownMenu(menubar,"filepane",    al, ac);
    viewpane    = XmCreatePulldownMenu(menubar,"viewpane",    al, ac);
    mappane     = XmCreatePulldownMenu(menubar,"mappane",     al, ac);
    stationspane= XmCreatePulldownMenu(menubar,"stationspane",al, ac);
    messagepane = XmCreatePulldownMenu(menubar,"messagepane", al, ac);
    ifacepane   = XmCreatePulldownMenu(menubar,"ifacepane",   al, ac);
    helppane    = XmCreatePulldownMenu(menubar,"helppane",    al, ac);

    file_button = XtVaCreateManagedWidget(langcode("MENUTB0001"),
            xmCascadeButtonGadgetClass,
            menubar,
            XmNsubMenuId, filepane,
            XmNmnemonic,langcode_hotkey("MENUTB0001"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    view_button = XtVaCreateManagedWidget(langcode("MENUTB0002"),
            xmCascadeButtonGadgetClass,
            menubar,
            XmNsubMenuId,viewpane,
            XmNmnemonic,langcode_hotkey("MENUTB0002"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    map_button = XtVaCreateManagedWidget(langcode("MENUTB0004"),
            xmCascadeButtonGadgetClass,
            menubar,
            XmNsubMenuId,mappane,
            XmNmnemonic,langcode_hotkey("MENUTB0004"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    display_button = XtVaCreateManagedWidget(langcode("MENUTB0005"),
            xmCascadeButtonGadgetClass,
            menubar,
            XmNsubMenuId,stationspane,
            XmNmnemonic,langcode_hotkey("MENUTB0005"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    message_button = XtVaCreateManagedWidget(langcode("MENUTB0006"),
            xmCascadeButtonGadgetClass,
            menubar,
            XmNsubMenuId,messagepane,
            XmNmnemonic,langcode_hotkey("MENUTB0006"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    iface_button = XtVaCreateManagedWidget(langcode("MENUTB0010"),
            xmCascadeButtonGadgetClass,
            menubar,
            XmNsubMenuId,ifacepane,
            XmNmnemonic,langcode_hotkey("MENUTB0010"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    help_button = XtVaCreateManagedWidget(langcode("MENUTB0009"),
            xmCascadeButtonGadgetClass,
            menubar,
            XmNsubMenuId,helppane,
            XmNmnemonic,langcode_hotkey("MENUTB0009"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtVaSetValues (menubar,XmNmenuHelpWidget,help_button,NULL);
    /* end bar */

    /* File */
    ac = 0;
    XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
    XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
    XtSetArg(al[ac], XmNtearOffModel, XmTEAR_OFF_ENABLED); ac++;

    configpane  = XmCreatePulldownMenu(filepane,
            "configpane",
            al,
            ac);

    config_button = XtVaCreateManagedWidget(langcode("PULDNFI001"),
            xmCascadeButtonGadgetClass,
            filepane,
            XmNsubMenuId,configpane,
            XmNmnemonic,langcode_hotkey("PULDNFI001"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
 
   (void)XtVaCreateManagedWidget("create_appshell sep1",
            xmSeparatorGadgetClass,
            filepane,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    open_file_button = XtVaCreateManagedWidget(langcode("PULDNFI002"),
            xmPushButtonGadgetClass,
            filepane,
            XmNmnemonic,langcode_hotkey("PULDNFI002"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);


    tnc_logging = XtVaCreateManagedWidget(langcode("PULDNFI010"),
            xmToggleButtonGadgetClass,
            filepane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(tnc_logging,XmNvalueChangedCallback,TNC_Logging_toggle,"1");
    if (log_tnc_data)
        XmToggleButtonSetState(tnc_logging,TRUE,FALSE);



    net_logging = XtVaCreateManagedWidget(langcode("PULDNFI011"),
            xmToggleButtonGadgetClass,
            filepane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(net_logging,XmNvalueChangedCallback,Net_Logging_toggle,"1");
    if (log_net_data)
        XmToggleButtonSetState(net_logging,TRUE,FALSE);


    igate_logging = XtVaCreateManagedWidget(langcode("PULDNFI012"),
            xmToggleButtonGadgetClass,
            filepane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(igate_logging,XmNvalueChangedCallback,IGate_Logging_toggle,"1");
    if (log_igate)
        XmToggleButtonSetState(igate_logging,TRUE,FALSE);


    wx_logging = XtVaCreateManagedWidget(langcode("PULDNFI013"),
            xmToggleButtonGadgetClass,
            filepane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(wx_logging,XmNvalueChangedCallback,WX_Logging_toggle,"1");
    if (log_wx)
        XmToggleButtonSetState(wx_logging,TRUE,FALSE);


    enable_snapshots = XtVaCreateManagedWidget(langcode("PULDNFI014"),
            xmToggleButtonGadgetClass,
            filepane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(enable_snapshots,XmNvalueChangedCallback,Snapshots_toggle,"1");
    if (snapshots_enabled)
        XmToggleButtonSetState(enable_snapshots,TRUE,FALSE);



    (void)XtVaCreateManagedWidget("create_appshell sep1a",
            xmSeparatorGadgetClass,
            filepane,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);



    // Print button
    print_button = XtVaCreateManagedWidget(langcode("PULDNFI015"),
            xmPushButtonWidgetClass,
            filepane,
            XmNmnemonic, langcode_hotkey("PULDNFI015"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback( print_button, XmNactivateCallback, Print_properties, NULL );

    (void)XtVaCreateManagedWidget("create_appshell sep1b",
            xmSeparatorGadgetClass,
            filepane,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);



    test_button = XtVaCreateManagedWidget(langcode("PULDNFI003"),
            xmPushButtonWidgetClass,
            filepane,
            XmNmnemonic, langcode_hotkey("PULDNFI003"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    debug_level_button = XtVaCreateManagedWidget(langcode("PULDNFI007"),
            xmPushButtonWidgetClass,
            filepane,
            XmNmnemonic, langcode_hotkey("PULDNFI007"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    (void)XtVaCreateManagedWidget("create_appshell sep1c",
            xmSeparatorGadgetClass,
            filepane,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    exitpane  = XmCreatePulldownMenu(filepane,
            "exitpane",
            al,
            ac);

    exit_button = XtVaCreateManagedWidget(langcode("PULDNFI004"),
            xmCascadeButtonGadgetClass,
            filepane,
            XmNsubMenuId,exitpane,
            XmNmnemonic,langcode_hotkey("PULDNFI004"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
 
/*
    exit_button = XtVaCreateManagedWidget(langcode("PULDNFI004"),
            xmPushButtonWidgetClass,
            filepane,
            XmNmnemonic,langcode_hotkey("PULDNFI004"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
*/

    /* View */
    bullet_button = XtVaCreateManagedWidget(langcode("PULDNVI001"),
            xmPushButtonGadgetClass,
            viewpane,
            XmNmnemonic,langcode_hotkey("PULDNVI001"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    packet_data_button = XtVaCreateManagedWidget(langcode("PULDNVI002"),
            xmPushButtonGadgetClass,
            viewpane,
            XmNmnemonic,langcode_hotkey("PULDNVI002"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    mobile_button = XtVaCreateManagedWidget(langcode("PULDNVI003"),
            xmPushButtonGadgetClass,
            viewpane,
            XmNmnemonic,langcode_hotkey("PULDNVI003"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    stations_button = XtVaCreateManagedWidget(langcode("PULDNVI004"),
            xmPushButtonGadgetClass,
            viewpane,
            XmNmnemonic,langcode_hotkey("PULDNVI004"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    localstations_button = XtVaCreateManagedWidget(langcode("PULDNVI009"),
            xmPushButtonGadgetClass,
            viewpane,
            XmNmnemonic,langcode_hotkey("PULDNVI009"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    laststations_button = XtVaCreateManagedWidget(langcode("PULDNVI012"),
            xmPushButtonGadgetClass,
            viewpane,
            XmNmnemonic,langcode_hotkey("PULDNVI012"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    objectstations_button = XtVaCreateManagedWidget(langcode("LHPUPNI005"),
            xmPushButtonGadgetClass,
            viewpane,
            XmNmnemonic,langcode_hotkey("LHPUPNI005"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    objectmystations_button = XtVaCreateManagedWidget(langcode("LHPUPNI006"),
            xmPushButtonGadgetClass,
            viewpane,
            XmNmnemonic,langcode_hotkey("LHPUPNI006"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    weather_button = XtVaCreateManagedWidget(langcode("PULDNVI005"),
            xmPushButtonGadgetClass,
            viewpane,
            XmNmnemonic,langcode_hotkey("PULDNVI005"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    wx_station_button = XtVaCreateManagedWidget(langcode("PULDNVI008"),
            xmPushButtonGadgetClass,
            viewpane,
            XmNmnemonic,langcode_hotkey("PULDNVI008"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    alert_button = XtVaCreateManagedWidget(langcode("PULDNVI007"),
            xmPushButtonGadgetClass,
            viewpane,
            XmNmnemonic,langcode_hotkey("PULDNVI007"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    view_messages_button = XtVaCreateManagedWidget(langcode("PULDNVI011"),
            xmPushButtonGadgetClass,
            viewpane,
            XmNmnemonic,langcode_hotkey("PULDNVI011"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    uptime_button = XtVaCreateManagedWidget(langcode("PULDNVI013"),
            xmPushButtonWidgetClass,
            viewpane,
            XmNmnemonic, langcode_hotkey("PULDNVI013"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    /* Configure */
    station_button = XtVaCreateManagedWidget(langcode("PULDNCF004"),
            xmPushButtonGadgetClass,
            configpane,
            XmNmnemonic,langcode_hotkey("PULDNCF004"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    defaults_button = XtVaCreateManagedWidget(langcode("PULDNCF001"),
            xmPushButtonGadgetClass,
            configpane,
            XmNmnemonic,langcode_hotkey("PULDNCF001"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    coordinates_button = XtVaCreateManagedWidget(langcode("PULDNCF002"),
            xmPushButtonGadgetClass,
            configpane,
            XmNmnemonic,langcode_hotkey("PULDNCF002"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    aa_button = XtVaCreateManagedWidget(langcode("PULDNCF006"),
            xmPushButtonGadgetClass,
            configpane,
            XmNmnemonic,langcode_hotkey("PULDNCF006"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    speech_button = XtVaCreateManagedWidget(langcode("PULDNCF007"),
            xmPushButtonGadgetClass,
            configpane,
            XmNmnemonic,langcode_hotkey("PULDNCF007"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    smart_beacon_button = XtVaCreateManagedWidget("Smart Beaconing",
            xmPushButtonGadgetClass,
            configpane,
            XmNmnemonic,"Smart Beaconing",
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);


    units_choice_button = XtVaCreateManagedWidget(langcode("PULDNUT001"),
            xmToggleButtonGadgetClass,
            configpane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(units_choice_button,XmNvalueChangedCallback,Units_choice_toggle,"1");
    if (units_english_metric)
        XmToggleButtonSetState(units_choice_button,TRUE,FALSE);



    (void)XtVaCreateManagedWidget("create_appshell sep1b",
            xmSeparatorGadgetClass,
            configpane,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    save_button = XtVaCreateManagedWidget(langcode("PULDNCF008"),
            xmPushButtonGadgetClass,
            configpane,
            XmNmnemonic, langcode_hotkey("PULDNCF008"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);


    /* Exit */
    really_exit_button = XtVaCreateManagedWidget(langcode("PULDNFI005"),
            xmPushButtonWidgetClass,
            exitpane,
            XmNmnemonic,langcode_hotkey("PULDNFI005"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);


//- Maps -------------------------------------------------------------

    map_chooser_button = XtVaCreateManagedWidget(langcode("PULDNMP001"),
            xmPushButtonGadgetClass,
            mappane,
            XmNmnemonic,langcode_hotkey("PULDNMP001"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(map_chooser_button,   XmNactivateCallback,Map_chooser,NULL);

#if defined(HAVE_IMAGEMAGICK)
    tiger_config_button= XtVaCreateManagedWidget(langcode("PULDNMP020"),
            xmPushButtonGadgetClass,
            mappane,
            XmNmnemonic,langcode_hotkey("PULDNMP020"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(tiger_config_button,   XmNactivateCallback,Config_tiger,NULL);
#endif


    jump_button = XtVaCreateManagedWidget(langcode("PULDNMP012"),
            xmPushButtonGadgetClass,
            mappane,
            XmNmnemonic,langcode_hotkey("PULDNMP012"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    locate_place_button = XtVaCreateManagedWidget(langcode("PULDNMP014"),
            xmPushButtonGadgetClass,
            mappane,
            XmNmnemonic,langcode_hotkey("PULDNMP014"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    coordinate_calculator_button = XtVaCreateManagedWidget(langcode("COORD001"),
            xmPushButtonGadgetClass,mappane,
            XmNmnemonic, langcode_hotkey("COORD001"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    (void)XtVaCreateManagedWidget("create_appshell sep2",
            xmSeparatorGadgetClass,
            mappane,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);


    map_disable_button = XtVaCreateManagedWidget(langcode("PULDNMP013"),
            xmToggleButtonGadgetClass,
            mappane,
            XmNvisibleWhenOff, TRUE,                        
            XmNindicatorSize, 12,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(map_disable_button,XmNvalueChangedCallback,Map_disable_toggle,"1");
    if (disable_all_maps)
        XmToggleButtonSetState(map_disable_button,TRUE,FALSE);


    map_auto_button = XtVaCreateManagedWidget(langcode("PULDNMP002"),
            xmToggleButtonGadgetClass,
            mappane,
            XmNvisibleWhenOff, TRUE,                        
            XmNindicatorSize, 12,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(map_auto_button,XmNvalueChangedCallback,Map_auto_toggle,"1");
    if (map_auto_maps)
        XmToggleButtonSetState(map_auto_button,TRUE,FALSE);


    map_auto_skip_raster_button = XtVaCreateManagedWidget(langcode("PULDNMP021"),
            xmToggleButtonGadgetClass,
            mappane,
            XmNvisibleWhenOff, TRUE,                        
            XmNindicatorSize, 12,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(map_auto_skip_raster_button,XmNvalueChangedCallback,Map_auto_skip_raster_toggle,"1");
    if (auto_maps_skip_raster)
        XmToggleButtonSetState(map_auto_skip_raster_button,TRUE,FALSE);
    if (!map_auto_maps)
        XtSetSensitive(map_auto_skip_raster_button,FALSE);


    map_grid_button = XtVaCreateManagedWidget(langcode("PULDNMP003"),
            xmToggleButtonGadgetClass,
            mappane,
            XmNvisibleWhenOff, TRUE,                        
            XmNindicatorSize, 12,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(map_grid_button,XmNvalueChangedCallback,Grid_toggle,"1");
    if (long_lat_grid)
        XmToggleButtonSetState(map_grid_button,TRUE,FALSE);


    map_levels_button = XtVaCreateManagedWidget(langcode("PULDNMP004"),
            xmToggleButtonGadgetClass,
            mappane,
            XmNvisibleWhenOff, TRUE,                        
            XmNindicatorSize, 12,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(map_levels_button,XmNvalueChangedCallback,Map_levels_toggle,"1");
    if (map_color_levels)
        XmToggleButtonSetState(map_levels_button,TRUE,FALSE);


    map_labels_button = XtVaCreateManagedWidget(langcode("PULDNMP010"),
            xmToggleButtonGadgetClass,
            mappane,
            XmNvisibleWhenOff, TRUE,                        
            XmNindicatorSize, 12,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(map_labels_button,XmNvalueChangedCallback,Map_labels_toggle,"1");
    if (map_labels)
        XmToggleButtonSetState(map_labels_button,TRUE,FALSE);


    map_fill_button = XtVaCreateManagedWidget(langcode("PULDNMP009"),
            xmToggleButtonGadgetClass,
            mappane,
            XmNvisibleWhenOff, TRUE,                        
            XmNindicatorSize, 12,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(map_fill_button,XmNvalueChangedCallback,Map_fill_toggle,"1");
    if (map_color_fill)
        XmToggleButtonSetState(map_fill_button,TRUE,FALSE);


    map_wx_alerts_button = XtVaCreateManagedWidget(langcode("PULDNMP007"),
            xmToggleButtonGadgetClass,
            mappane,
            XmNvisibleWhenOff, TRUE,                        
            XmNindicatorSize, 12,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(map_wx_alerts_button,XmNvalueChangedCallback,Map_wx_alerts_toggle,"1");
    if (!wx_alert_style)
        XmToggleButtonSetState(map_wx_alerts_button,TRUE,FALSE);


    (void)XtVaCreateManagedWidget("create_appshell sep2b",
            xmSeparatorGadgetClass,
            mappane,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);


    Map_background_color_Pane = XmCreatePulldownMenu(mappane,
            "create_appshell map_background_color",
            al,
            ac);
 
    map_background_button = XtVaCreateManagedWidget(langcode("PULDNMP005"),
            xmCascadeButtonWidgetClass,
            mappane,
            XmNsubMenuId, Map_background_color_Pane,
            XmNmnemonic, langcode_hotkey("PULDNMP005"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    map_bgcolor[10] = XtVaCreateManagedWidget(langcode("PULDNMBC11"),
            xmPushButtonGadgetClass,
            Map_background_color_Pane,
            XmNmnemonic,langcode_hotkey("PULDNMBC11"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    map_bgcolor[11] = XtVaCreateManagedWidget(langcode("PULDNMBC12"),
            xmPushButtonGadgetClass,
            Map_background_color_Pane,
            XmNmnemonic,langcode_hotkey("PULDNMBC12"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    map_bgcolor[0] = XtVaCreateManagedWidget(langcode("PULDNMBC01"),
            xmPushButtonGadgetClass,
            Map_background_color_Pane,
            XmNmnemonic,langcode_hotkey("PULDNMBC01"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    map_bgcolor[1] = XtVaCreateManagedWidget(langcode("PULDNMBC02"),
            xmPushButtonGadgetClass,
            Map_background_color_Pane,
            XmNmnemonic,langcode_hotkey("PULDNMBC02"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    map_bgcolor[2] = XtVaCreateManagedWidget(langcode("PULDNMBC03"),
            xmPushButtonGadgetClass,
            Map_background_color_Pane,
            XmNmnemonic,langcode_hotkey("PULDNMBC03"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    map_bgcolor[3] = XtVaCreateManagedWidget(langcode("PULDNMBC04"),
            xmPushButtonGadgetClass,
            Map_background_color_Pane,
            XmNmnemonic,langcode_hotkey("PULDNMBC04"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    map_bgcolor[4] = XtVaCreateManagedWidget(langcode("PULDNMBC05"),
            xmPushButtonGadgetClass,
            Map_background_color_Pane,
            XmNmnemonic,langcode_hotkey("PULDNMBC05"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    map_bgcolor[5] = XtVaCreateManagedWidget(langcode("PULDNMBC06"),
            xmPushButtonGadgetClass,
            Map_background_color_Pane,
            XmNmnemonic,langcode_hotkey("PULDNMBC06"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    map_bgcolor[6] = XtVaCreateManagedWidget(langcode("PULDNMBC07"),
            xmPushButtonGadgetClass,
            Map_background_color_Pane,
            XmNmnemonic,langcode_hotkey("PULDNMBC07"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    map_bgcolor[7] = XtVaCreateManagedWidget(langcode("PULDNMBC08"),
            xmPushButtonGadgetClass,
            Map_background_color_Pane,
            XmNmnemonic,langcode_hotkey("PULDNMBC08"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    map_bgcolor[8] = XtVaCreateManagedWidget(langcode("PULDNMBC09"),
            xmPushButtonGadgetClass,
            Map_background_color_Pane,
            XmNmnemonic,langcode_hotkey("PULDNMBC09"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    map_bgcolor[9] = XtVaCreateManagedWidget(langcode("PULDNMBC10"),
            xmPushButtonGadgetClass,
            Map_background_color_Pane,
            XmNmnemonic,langcode_hotkey("PULDNMBC10"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtSetSensitive(map_bgcolor[map_background_color],FALSE);
    XtAddCallback(map_bgcolor[10], XmNactivateCallback,Map_background,"10");
    XtAddCallback(map_bgcolor[11], XmNactivateCallback,Map_background,"11");
    XtAddCallback(map_bgcolor[0],  XmNactivateCallback,Map_background,"0");
    XtAddCallback(map_bgcolor[1],  XmNactivateCallback,Map_background,"1");
    XtAddCallback(map_bgcolor[2],  XmNactivateCallback,Map_background,"2");
    XtAddCallback(map_bgcolor[3],  XmNactivateCallback,Map_background,"3");
    XtAddCallback(map_bgcolor[4],  XmNactivateCallback,Map_background,"4");
    XtAddCallback(map_bgcolor[5],  XmNactivateCallback,Map_background,"5");
    XtAddCallback(map_bgcolor[6],  XmNactivateCallback,Map_background,"6");
    XtAddCallback(map_bgcolor[7],  XmNactivateCallback,Map_background,"7");
    XtAddCallback(map_bgcolor[8],  XmNactivateCallback,Map_background,"8");
    XtAddCallback(map_bgcolor[9],  XmNactivateCallback,Map_background,"9");

#if !defined(NO_GRAPHICS)
#if defined(HAVE_GEOTIFF)
    Geotiff_intensity_Pane = XmCreatePulldownMenu(mappane,
            "create_appshell geotiff_intensity",
            al,
            ac);

    geotiff_intensity_button = XtVaCreateManagedWidget(langcode("PULDNMP008"),
            xmCascadeButtonWidgetClass,
            mappane,
            XmNsubMenuId,
            Geotiff_intensity_Pane,
            XmNmnemonic, langcode_hotkey("PULDNMP008"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    geotiff_intensity[0] = XtVaCreateManagedWidget("0%",
            xmPushButtonGadgetClass,
            Geotiff_intensity_Pane,
            XmNmnemonic,"0%",
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    geotiff_intensity[1] = XtVaCreateManagedWidget("10%",
            xmPushButtonGadgetClass,
            Geotiff_intensity_Pane,
            XmNmnemonic,"10%",
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    geotiff_intensity[2] = XtVaCreateManagedWidget("20%",
            xmPushButtonGadgetClass,
            Geotiff_intensity_Pane,
            XmNmnemonic,"20%",
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    geotiff_intensity[3] = XtVaCreateManagedWidget("30%",
            xmPushButtonGadgetClass,
            Geotiff_intensity_Pane,
            XmNmnemonic,"30%",
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    geotiff_intensity[4] = XtVaCreateManagedWidget("40%",
            xmPushButtonGadgetClass,
            Geotiff_intensity_Pane,
            XmNmnemonic,"40%",
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    geotiff_intensity[5] = XtVaCreateManagedWidget("50%",
            xmPushButtonGadgetClass,
            Geotiff_intensity_Pane,
            XmNmnemonic,"50%",
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    geotiff_intensity[6] = XtVaCreateManagedWidget("60%",
            xmPushButtonGadgetClass,
            Geotiff_intensity_Pane,
            XmNmnemonic,"60%",
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    geotiff_intensity[7] = XtVaCreateManagedWidget("70%",
            xmPushButtonGadgetClass,
            Geotiff_intensity_Pane,
            XmNmnemonic,"70%",
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    geotiff_intensity[8] = XtVaCreateManagedWidget("80%",
            xmPushButtonGadgetClass,
            Geotiff_intensity_Pane,
            XmNmnemonic,"80%",
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    geotiff_intensity[9] = XtVaCreateManagedWidget("90%",
            xmPushButtonGadgetClass,
            Geotiff_intensity_Pane,
            XmNmnemonic,"90%",
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    geotiff_intensity[10] = XtVaCreateManagedWidget("100%",
            xmPushButtonGadgetClass,
            Geotiff_intensity_Pane,
            XmNmnemonic,"100%",
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtSetSensitive(geotiff_intensity[(int)(geotiff_map_intensity * 10.0)],FALSE);
    XtAddCallback(geotiff_intensity[0],  XmNactivateCallback,Geotiff_intensity,"0.0");
    XtAddCallback(geotiff_intensity[1],  XmNactivateCallback,Geotiff_intensity,"0.1");
    XtAddCallback(geotiff_intensity[2],  XmNactivateCallback,Geotiff_intensity,"0.2");
    XtAddCallback(geotiff_intensity[3],  XmNactivateCallback,Geotiff_intensity,"0.3");
    XtAddCallback(geotiff_intensity[4],  XmNactivateCallback,Geotiff_intensity,"0.4");
    XtAddCallback(geotiff_intensity[5],  XmNactivateCallback,Geotiff_intensity,"0.5");
    XtAddCallback(geotiff_intensity[6],  XmNactivateCallback,Geotiff_intensity,"0.6");
    XtAddCallback(geotiff_intensity[7],  XmNactivateCallback,Geotiff_intensity,"0.7");
    XtAddCallback(geotiff_intensity[8],  XmNactivateCallback,Geotiff_intensity,"0.8");
    XtAddCallback(geotiff_intensity[9],  XmNactivateCallback,Geotiff_intensity,"0.9");
    XtAddCallback(geotiff_intensity[10], XmNactivateCallback,Geotiff_intensity,"1.0");
#endif
#if defined(HAVE_IMAGEMAGICK)
    // Adjust Gamma Correction
    gamma_adjust_button = XtVaCreateManagedWidget(langcode("GAMMA001"),
            xmPushButtonWidgetClass, mappane,
            XmNmnemonic,             "G",
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(gamma_adjust_button, XmNactivateCallback, Gamma_adjust, NULL);
#endif
#endif

    Map_station_label_Pane = XmCreatePulldownMenu(mappane,
            "create_appshell map_station_label",
            al,
            ac);
    map_station_label_button = XtVaCreateManagedWidget(langcode("PULDNMP006"),
            xmCascadeButtonWidgetClass,
            mappane,
            XmNsubMenuId, Map_station_label_Pane,
            XmNmnemonic,langcode_hotkey("PULDNMP006"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    map_station_label0 = XtVaCreateManagedWidget(langcode("PULDNMSL01"),
            xmPushButtonGadgetClass,
            Map_station_label_Pane,
            XmNmnemonic,langcode_hotkey("PULDNMSL01"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    map_station_label1 = XtVaCreateManagedWidget(langcode("PULDNMSL02"),
            xmPushButtonGadgetClass,
            Map_station_label_Pane,
            XmNmnemonic,langcode_hotkey("PULDNMSL02"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    map_station_label2 = XtVaCreateManagedWidget(langcode("PULDNMSL03"),
            xmPushButtonGadgetClass,
            Map_station_label_Pane,
            XmNmnemonic,langcode_hotkey("PULDNMSL03"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    sel3_switch(letter_style,map_station_label2,map_station_label1,map_station_label0);
    XtAddCallback(map_station_label0,   XmNactivateCallback,Map_station_label,"0");
    XtAddCallback(map_station_label1,   XmNactivateCallback,Map_station_label,"1");
    XtAddCallback(map_station_label2,   XmNactivateCallback,Map_station_label,"2");



    (void)XtVaCreateManagedWidget("create_appshell sep_map",
            xmSeparatorGadgetClass,
            mappane,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);


    map_pointer_menu_button = XtVaCreateManagedWidget(langcode("PULDNMP011"),
            xmPushButtonGadgetClass,
            mappane,
            XmNmnemonic,langcode_hotkey("PULDNMP011"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);


//- Stations Menu -----------------------------------------------------
    locate_button = XtVaCreateManagedWidget(langcode("PULDNDP014"),
            xmPushButtonGadgetClass,
            stationspane,
            XmNmnemonic,langcode_hotkey("PULDNDP014"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    track_button = XtVaCreateManagedWidget(langcode("PULDNDP001"),
            xmPushButtonGadgetClass,
            stationspane,
            XmNmnemonic,langcode_hotkey("PULDNDP001"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(track_button, XmNactivateCallback,Track_station,NULL);

    download_trail_button = XtVaCreateManagedWidget(langcode("PULDNDP022"),
            xmPushButtonGadgetClass,
            stationspane,
            XmNmnemonic,langcode_hotkey("PULDNDP022"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(download_trail_button, XmNactivateCallback,Download_findu_trail,NULL);



    (void)XtVaCreateManagedWidget("create_appshell sep3",
            xmSeparatorGadgetClass,
            stationspane,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);


    symbol_filter_pane = XmCreatePulldownMenu(stationspane,
            "symbol_filter_pane",
            al,
            ac);

    symbol_filter_button = XtVaCreateManagedWidget(langcode("PULDNDP032"),
            xmCascadeButtonGadgetClass,
            stationspane,
            XmNsubMenuId,symbol_filter_pane,
            XmNmnemonic,langcode_hotkey("PULDNDP032"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    // Symbol Filtering
    symbols_button = XtVaCreateManagedWidget(langcode("PULDNDP012"),
            xmToggleButtonGadgetClass,
            symbol_filter_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(symbols_button,XmNvalueChangedCallback,Symbols_toggle,"1");
    if (symbol_display_enable)
        XmToggleButtonSetState(symbols_button,TRUE,FALSE);


    symbols_local_stations_button = XtVaCreateManagedWidget(langcode("PULDNDP027"),
            xmToggleButtonGadgetClass,
            symbol_filter_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(symbols_local_stations_button,XmNvalueChangedCallback,
                  Symbols_local_stations_toggle,"1");
    if (symbol_display_local_stations)
        XmToggleButtonSetState(symbols_local_stations_button,TRUE,FALSE);
    if (!symbol_display_enable)
        XtSetSensitive(symbols_local_stations_button,FALSE);


    symbols_nonlocal_stations_button = XtVaCreateManagedWidget(langcode("PULDNDP034"),
            xmToggleButtonGadgetClass,
            symbol_filter_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(symbols_nonlocal_stations_button,XmNvalueChangedCallback,
                  Symbols_nonlocal_stations_toggle,"1");
    if (symbol_display_nonlocal_stations)
        XmToggleButtonSetState(symbols_nonlocal_stations_button,TRUE,FALSE);
    if (!symbol_display_enable)
        XtSetSensitive(symbols_nonlocal_stations_button,FALSE);


    symbols_stationary_button = XtVaCreateManagedWidget(langcode("PULDNDP028"),
            xmToggleButtonGadgetClass,
            symbol_filter_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(symbols_stationary_button,XmNvalueChangedCallback,Symbols_stationary_toggle,"1");
    if (symbol_display_stationary)
        XmToggleButtonSetState(symbols_stationary_button,TRUE,FALSE);
    if (!symbol_display_enable ||
        (!symbol_display_local_stations && !symbol_display_nonlocal_stations))
        XtSetSensitive(symbols_stationary_button,FALSE);


    symbols_moving_button = XtVaCreateManagedWidget(langcode("PULDNDP029"),
            xmToggleButtonGadgetClass,
            symbol_filter_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(symbols_moving_button,XmNvalueChangedCallback,Symbols_moving_toggle,"1");
    if (symbol_display_moving)
        XmToggleButtonSetState(symbols_moving_button,TRUE,FALSE);
    if (!symbol_display_enable ||
        (!symbol_display_local_stations && !symbol_display_nonlocal_stations))
        XtSetSensitive(symbols_moving_button,FALSE);


    symbols_WX_button = XtVaCreateManagedWidget(langcode("PULDNDP030"),
            xmToggleButtonGadgetClass,
            symbol_filter_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(symbols_WX_button,XmNvalueChangedCallback,Symbols_WX_toggle,"1");
    if (symbol_display_WX)
        XmToggleButtonSetState(symbols_WX_button,TRUE,FALSE);
    if (!symbol_display_enable ||
        (!symbol_display_local_stations && !symbol_display_nonlocal_stations))
        XtSetSensitive(symbols_WX_button,FALSE);


    symbols_objects_button = XtVaCreateManagedWidget(langcode("PULDNDP031"),
            xmToggleButtonGadgetClass,
            symbol_filter_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(symbols_objects_button,XmNvalueChangedCallback,Symbols_objects_toggle,"1");
    if (symbol_display_objects)
        XmToggleButtonSetState(symbols_objects_button,TRUE,FALSE);
    if (!symbol_display_enable)
        XtSetSensitive(symbols_objects_button,FALSE);


    wx_obj_enable_button = XtVaCreateManagedWidget(langcode("PULDNDP026"),
            xmToggleButtonGadgetClass,
            symbol_filter_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(wx_obj_enable_button,XmNvalueChangedCallback,WX_obj_enable_toggle,"1");
    if (wx_obj_display_enable)
        XmToggleButtonSetState(wx_obj_enable_button,TRUE,FALSE);
    if (!symbol_display_enable || !symbol_display_objects)
        XtSetSensitive(wx_obj_enable_button,FALSE);
    // End of Symbol Filtering


    // Displayed Info Filtering
    info_filter_pane = XmCreatePulldownMenu(stationspane,
            "symbol_filter_pane",
            al,
            ac);

    info_filter_button = XtVaCreateManagedWidget(langcode("PULDNDP033"),
            xmCascadeButtonGadgetClass,
            stationspane,
            XmNsubMenuId,info_filter_pane,
            XmNmnemonic,langcode_hotkey("PULDNDP033"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    symbols_rotate_button = XtVaCreateManagedWidget(langcode("PULDNDP011"),
            xmToggleButtonGadgetClass,
            info_filter_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(symbols_rotate_button,XmNvalueChangedCallback,Symbols_rotate_toggle,"1");
    if (symbol_display_rotate)
        XmToggleButtonSetState(symbols_rotate_button,TRUE,FALSE);
    if (!symbol_display_enable)
        XtSetSensitive(symbols_rotate_button,FALSE);


    call_button = XtVaCreateManagedWidget(langcode("PULDNDP010"),
            xmToggleButtonGadgetClass,
            info_filter_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(call_button,XmNvalueChangedCallback,Call_toggle,"1");
    if (symbol_callsign_display)
        XmToggleButtonSetState(call_button,TRUE,FALSE);
    if (!symbol_display_enable)
        XtSetSensitive(call_button,FALSE);


    speed_enable_button = XtVaCreateManagedWidget(langcode("PULDNDP004"),
            xmToggleButtonGadgetClass,
            info_filter_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(speed_enable_button,XmNvalueChangedCallback,Speed_enable_toggle,"1");
    if (speed_display_enable)
        XmToggleButtonSetState(speed_enable_button,TRUE,FALSE);
    if (!symbol_display_enable)
        XtSetSensitive(speed_enable_button,FALSE);


    speed_short_button = XtVaCreateManagedWidget(langcode("PULDNDP017"),
            xmToggleButtonGadgetClass,
            info_filter_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(speed_short_button,XmNvalueChangedCallback,Speed_short_toggle,"1");
    if (speed_display_short)
        XmToggleButtonSetState(speed_short_button,TRUE,FALSE);
    if (!speed_display_enable || !symbol_display_enable)
        XtSetSensitive(speed_short_button,FALSE);


    alt_button = XtVaCreateManagedWidget(langcode("PULDNDP002"),
            xmToggleButtonGadgetClass,
            info_filter_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(alt_button,XmNvalueChangedCallback,Alt_toggle,"1");
    if (symbol_alt_display)
        XmToggleButtonSetState(alt_button,TRUE,FALSE);
    if (!symbol_display_enable)
        XtSetSensitive(alt_button,FALSE);


    course_button = XtVaCreateManagedWidget(langcode("PULDNDP003"),
            xmToggleButtonGadgetClass,
            info_filter_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(course_button,XmNvalueChangedCallback,Course_toggle,"1");
    if (symbol_course_display)
        XmToggleButtonSetState(course_button,TRUE,FALSE);
    if (!symbol_display_enable)
        XtSetSensitive(course_button,FALSE);


    dist_course_button = XtVaCreateManagedWidget(langcode("PULDNDP005"),
            xmToggleButtonGadgetClass,
            info_filter_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(dist_course_button,XmNvalueChangedCallback,Dist_course_toggle,"1");
    if (symbol_dist_course_display)
        XmToggleButtonSetState(dist_course_button,TRUE,FALSE);
    if (!symbol_display_enable)
        XtSetSensitive(dist_course_button,FALSE);


    wx_enable_button = XtVaCreateManagedWidget(langcode("PULDNDP009"),
            xmToggleButtonGadgetClass,
            info_filter_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(wx_enable_button,XmNvalueChangedCallback,WX_enable_toggle,"1");
    if (wx_display_enable)
        XmToggleButtonSetState(wx_enable_button,TRUE,FALSE);
    if (!symbol_display_enable)
        XtSetSensitive(wx_enable_button,FALSE);


    wx_short_button = XtVaCreateManagedWidget(langcode("PULDNDP018"),
            xmToggleButtonGadgetClass,
            info_filter_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(wx_short_button,XmNvalueChangedCallback,WX_short_toggle,"1");
    if (wx_display_short)
        XmToggleButtonSetState(wx_short_button,TRUE,FALSE);
    if (!wx_display_enable || !symbol_display_enable)
        XtSetSensitive(wx_short_button,FALSE);


    station_phg_button = XtVaCreateManagedWidget(langcode("PULDNDP008"),
            xmToggleButtonGadgetClass,
            info_filter_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(station_phg_button,XmNvalueChangedCallback,Station_phg_toggle,"1");
    if (show_phg)
        XmToggleButtonSetState(station_phg_button,TRUE,FALSE);
    if (!symbol_display_enable)
        XtSetSensitive(station_phg_button,FALSE);


    station_phg_default_button = XtVaCreateManagedWidget(langcode("PULDNDP021"),
            xmToggleButtonGadgetClass,
            info_filter_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(station_phg_default_button,XmNvalueChangedCallback,Station_phg_default_toggle,"1");
    if (show_phg_default)
        XmToggleButtonSetState(station_phg_default_button,TRUE,FALSE);
    if (!show_phg || !symbol_display_enable)
        XtSetSensitive(station_phg_default_button,FALSE);


    station_phg_mobiles_button = XtVaCreateManagedWidget(langcode("PULDNDP020"),
            xmToggleButtonGadgetClass,
            info_filter_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(station_phg_mobiles_button,XmNvalueChangedCallback,Station_phg_mobiles_toggle,"1");
    if (show_phg_mobiles)
        XmToggleButtonSetState(station_phg_mobiles_button,TRUE,FALSE);
    if (!show_phg || !symbol_display_enable)
        XtSetSensitive(station_phg_mobiles_button,FALSE);


    station_amb_button = XtVaCreateManagedWidget(langcode("PULDNDP013"),
            xmToggleButtonGadgetClass,
            info_filter_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(station_amb_button,XmNvalueChangedCallback,Station_amb_toggle,"1");
    if (show_amb)
        XmToggleButtonSetState(station_amb_button,TRUE,FALSE);
    if (!symbol_display_enable)
        XtSetSensitive(station_amb_button,FALSE);


    station_old_data_button = XtVaCreateManagedWidget(langcode("PULDNDP019"),
            xmToggleButtonGadgetClass,
            info_filter_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(station_old_data_button,XmNvalueChangedCallback,Station_old_data_toggle,"1");
    if (show_old_data)
        XmToggleButtonSetState(station_old_data_button,TRUE,FALSE);
    if (!symbol_display_enable)
        XtSetSensitive(station_old_data_button,FALSE);

    station_DF_button = XtVaCreateManagedWidget(langcode("PULDNDP023"),
            xmToggleButtonGadgetClass,
            info_filter_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(station_DF_button,XmNvalueChangedCallback,Station_DF_toggle,"1");
    if (show_DF)
        XmToggleButtonSetState(station_DF_button,TRUE,FALSE);
    if (!symbol_display_enable)
        XtSetSensitive(station_DF_button,FALSE);

    station_last_heard_button = XtVaCreateManagedWidget(langcode("PULDNDP024"),
            xmToggleButtonGadgetClass,
            info_filter_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(station_last_heard_button,XmNvalueChangedCallback,Station_Last_Heard_toggle,"1");
    if (show_last_heard)
        XmToggleButtonSetState(station_last_heard_button,TRUE,FALSE);
    if (!symbol_display_enable)
        XtSetSensitive(station_last_heard_button,FALSE);

    dead_reckoning_button = XtVaCreateManagedWidget(langcode("PULDNDP035"),
            xmToggleButtonGadgetClass,
            info_filter_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(dead_reckoning_button,XmNvalueChangedCallback,Dead_Reckoning_toggle,"1");
    if (show_DR)
        XmToggleButtonSetState(dead_reckoning_button,TRUE,FALSE);
    if (!symbol_display_enable)
        XtSetSensitive(dead_reckoning_button,FALSE);

    dead_reckoning_arc_button = XtVaCreateManagedWidget(langcode("PULDNDP036"),
            xmToggleButtonGadgetClass,
            info_filter_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(dead_reckoning_arc_button,XmNvalueChangedCallback,Dead_Reckoning_Arc_toggle,"1");
    if (show_DR_arc)
        XmToggleButtonSetState(dead_reckoning_arc_button,TRUE,FALSE);
    if (!symbol_display_enable || !show_DR)
        XtSetSensitive(dead_reckoning_arc_button,FALSE);

    dead_reckoning_course_button = XtVaCreateManagedWidget(langcode("PULDNDP037"),
            xmToggleButtonGadgetClass,
            info_filter_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(dead_reckoning_course_button,XmNvalueChangedCallback,Dead_Reckoning_Course_toggle,"1");
    if (show_DR_course)
        XmToggleButtonSetState(dead_reckoning_course_button,TRUE,FALSE);
    if (!symbol_display_enable || !show_DR)
        XtSetSensitive(dead_reckoning_course_button,FALSE);
 
    dead_reckoning_symbol_button = XtVaCreateManagedWidget(langcode("PULDNDP038"),
            xmToggleButtonGadgetClass,
            info_filter_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(dead_reckoning_symbol_button,XmNvalueChangedCallback,Dead_Reckoning_Symbol_toggle,"1");
    if (show_DR_symbol)
        XmToggleButtonSetState(dead_reckoning_symbol_button,TRUE,FALSE);
    if (!symbol_display_enable || !show_DR)
        XtSetSensitive(dead_reckoning_symbol_button,FALSE);

    station_trails_button = XtVaCreateManagedWidget(langcode("PULDNDP007"),
            xmToggleButtonGadgetClass,
            info_filter_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(station_trails_button,XmNvalueChangedCallback,Station_trails_toggle,"1");
    if (station_trails)
        XmToggleButtonSetState(station_trails_button,TRUE,FALSE);
    // End of Displayed Info Filtering



    (void)XtVaCreateManagedWidget("create_appshell sep3",
            xmSeparatorGadgetClass,
            stationspane,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    station_clear_button = XtVaCreateManagedWidget(langcode("PULDNDP015"),
            xmPushButtonGadgetClass,
            stationspane,
            XmNmnemonic,langcode_hotkey("PULDNDP015"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    tracks_clear_button = XtVaCreateManagedWidget(langcode("PULDNDP016"),
            xmPushButtonGadgetClass,
            stationspane,
            XmNmnemonic,langcode_hotkey("PULDNDP016"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    object_history_clear_button = XtVaCreateManagedWidget(langcode("PULDNDP025"),
            xmPushButtonGadgetClass,
            stationspane,
            XmNmnemonic,langcode_hotkey("PULDNDP025"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

//--------------------------------------------------------------------

    /* Messages */
    send_message_to_button = XtVaCreateManagedWidget(langcode("PULDNMG001"),
            xmPushButtonGadgetClass,
            messagepane,
            XmNmnemonic,langcode_hotkey("PULDNMG001"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    open_messages_group_button = XtVaCreateManagedWidget(langcode("PULDNMG002"),
            xmPushButtonGadgetClass,
            messagepane,
            XmNmnemonic,langcode_hotkey("PULDNMG002"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    clear_messages_button= XtVaCreateManagedWidget(langcode("PULDNMG003"),
            xmPushButtonGadgetClass,
            messagepane,
            XmNmnemonic,langcode_hotkey("PULDNMG003"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    (void)XtVaCreateManagedWidget("create_appshell sep4",
            xmSeparatorGadgetClass,
            messagepane,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    General_q_button = XtVaCreateManagedWidget(langcode("PULDQUS001"),
            xmPushButtonGadgetClass,
            messagepane,
            XmNmnemonic,langcode_hotkey("PULDQUS001"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    IGate_q_button = XtVaCreateManagedWidget(langcode("PULDQUS002"),
            xmPushButtonGadgetClass,
            messagepane,
            XmNmnemonic,langcode_hotkey("PULDQUS002"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    WX_q_button = XtVaCreateManagedWidget(langcode("PULDQUS003"),
            xmPushButtonGadgetClass,
            messagepane,
            XmNmnemonic,langcode_hotkey("PULDQUS003"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

   (void)XtVaCreateManagedWidget("create_appshell sep4a",
            xmSeparatorGadgetClass,
            messagepane,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    auto_msg_set_button = XtVaCreateManagedWidget(langcode("PULDNMG004"),
            xmPushButtonGadgetClass,
            messagepane,
            XmNmnemonic,langcode_hotkey("PULDNMG004"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);


    auto_msg_toggle = XtVaCreateManagedWidget(langcode("PULDNMG005"),
            xmToggleButtonGadgetClass,
            messagepane,
            XmNvisibleWhenOff, TRUE,                        
            XmNindicatorSize, 12,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(auto_msg_toggle,XmNvalueChangedCallback,Auto_msg_toggle,"1");

   (void)XtVaCreateManagedWidget("create_appshell sep5a",
            xmSeparatorGadgetClass,
            messagepane,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    satellite_msg_ack_toggle = XtVaCreateManagedWidget(langcode("PULDNMG006"),
            xmToggleButtonGadgetClass,
            messagepane,
            XmNvisibleWhenOff, TRUE,                        
            XmNindicatorSize, 12,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(satellite_msg_ack_toggle,XmNvalueChangedCallback,Satellite_msg_ack_toggle,"1");



    /* Interface */
    iface_connect_button = XtVaCreateManagedWidget(langcode("PULDNTNT04"),
            xmPushButtonGadgetClass,
            ifacepane,
                            
            XmNmnemonic,langcode_hotkey("PULDNTNT04"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    device_config_button = XtVaCreateManagedWidget(langcode("PULDNTNT02"),
            xmPushButtonGadgetClass,
            ifacepane,
            XmNmnemonic,langcode_hotkey("PULDNTNT02"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    (void)XtVaCreateManagedWidget("create_appshell sep5",
            xmSeparatorGadgetClass,
            ifacepane,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);


    transmit_disable_toggle =  XtVaCreateManagedWidget(langcode("PULDNTNT03"),
            xmToggleButtonGadgetClass,
            ifacepane,
            XmNvisibleWhenOff, TRUE,                        
            XmNindicatorSize, 12,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(transmit_disable_toggle,XmNvalueChangedCallback,Transmit_disable_toggle,"1");
    if (transmit_disable)
        XmToggleButtonSetState(transmit_disable_toggle,TRUE,FALSE);
 

    posit_tx_disable_toggle = XtVaCreateManagedWidget(langcode("PULDNTNT05"),
            xmToggleButtonGadgetClass,
            ifacepane,
            XmNvisibleWhenOff, TRUE,                        
            XmNindicatorSize, 12,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(posit_tx_disable_toggle,XmNvalueChangedCallback,Posit_tx_disable_toggle,"1");
    if (posit_tx_disable)
        XmToggleButtonSetState(posit_tx_disable_toggle,TRUE,FALSE);
    if (transmit_disable)
        XtSetSensitive(posit_tx_disable_toggle,FALSE);


    object_tx_disable_toggle = XtVaCreateManagedWidget(langcode("PULDNTNT06"),
            xmToggleButtonGadgetClass,
            ifacepane,
            XmNvisibleWhenOff, TRUE,                        
            XmNindicatorSize, 12,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(object_tx_disable_toggle,XmNvalueChangedCallback,Object_tx_disable_toggle,"1");
    if (object_tx_disable)
        XmToggleButtonSetState(object_tx_disable_toggle,TRUE,FALSE);
    if (transmit_disable)
        XtSetSensitive(object_tx_disable_toggle,FALSE);


    (void)XtVaCreateManagedWidget("create_appshell sep6",
            xmSeparatorGadgetClass,
            ifacepane,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);


    iface_transmit_now = XtVaCreateManagedWidget(langcode("PULDNTNC01"),
            xmPushButtonGadgetClass,
            ifacepane,
            XmNmnemonic,langcode_hotkey("PULDNTNC01"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    if (transmit_disable)
        XtSetSensitive(iface_transmit_now,FALSE);


    /* Help*/
    help_about = XtVaCreateManagedWidget(langcode("PULDNHEL01"),
            xmPushButtonGadgetClass,
            helppane,
            XmNmnemonic,langcode_hotkey("PULDNHEL01"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    help_help = XtVaCreateManagedWidget(langcode("PULDNHEL02"),
            xmPushButtonGadgetClass,
            helppane,
            XmNmnemonic,langcode_hotkey("PULDNHEL02"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    /* view */
    XtAddCallback(stations_button,      XmNactivateCallback,Station_List,"0");
    XtAddCallback(mobile_button,        XmNactivateCallback,Station_List,"1");
    XtAddCallback(weather_button,       XmNactivateCallback,Station_List,"2");
    XtAddCallback(localstations_button, XmNactivateCallback,Station_List,"3");
    XtAddCallback(laststations_button,  XmNactivateCallback,Station_List,"4");
    XtAddCallback(objectstations_button,XmNactivateCallback,Station_List,"5");
    XtAddCallback(objectmystations_button,XmNactivateCallback,Station_List,"6");

    /* button callbacks */
    XtAddCallback(General_q_button,     XmNactivateCallback,General_query,"");
    XtAddCallback(IGate_q_button,       XmNactivateCallback,IGate_query,NULL);
    XtAddCallback(WX_q_button,          XmNactivateCallback,WX_query,NULL);
    XtAddCallback(station_clear_button, XmNactivateCallback,Stations_Clear,NULL);
    XtAddCallback(tracks_clear_button,  XmNactivateCallback,Tracks_All_Clear,NULL);
    XtAddCallback(object_history_clear_button, XmNactivateCallback,Object_History_Clear,NULL);
    XtAddCallback(really_exit_button,   XmNactivateCallback,Menu_Quit,NULL);

    XtAddCallback(defaults_button,      XmNactivateCallback,Configure_defaults,NULL);
    XtAddCallback(coordinates_button,   XmNactivateCallback,Configure_coordinates,NULL);
    XtAddCallback(aa_button,            XmNactivateCallback,Configure_audio_alarms,NULL);
    XtAddCallback(speech_button,        XmNactivateCallback,Configure_speech,NULL);
    XtAddCallback(smart_beacon_button,  XmNactivateCallback,Smart_Beacon,NULL);
    XtAddCallback(station_button,       XmNactivateCallback,Configure_station,NULL);

    XtAddCallback(help_about,           XmNactivateCallback,Help_About,NULL);
    XtAddCallback(help_help,            XmNactivateCallback,Help_Index,NULL);

    /* Interface */
    XtAddCallback(device_config_button, XmNactivateCallback,Configure_interface,NULL);

    /* TNC */
    XtAddCallback(iface_transmit_now,   XmNactivateCallback,TNC_Transmit_now,NULL);


    XtAddCallback(auto_msg_set_button,XmNactivateCallback,Auto_msg_set,NULL);

    XtAddCallback(iface_connect_button, XmNactivateCallback,control_interface,NULL);

    XtAddCallback(open_file_button,     XmNactivateCallback,Read_File_Selection,NULL);

    XtAddCallback(bullet_button,        XmNactivateCallback,Bulletins,NULL);
    XtAddCallback(packet_data_button,   XmNactivateCallback,Display_data,NULL);
    XtAddCallback(locate_button,        XmNactivateCallback,Locate_station,NULL);
    XtAddCallback(alert_button,         XmNactivateCallback,Display_Wx_Alert,NULL);
    XtAddCallback(view_messages_button, XmNactivateCallback,view_all_messages,NULL);

    XtAddCallback(map_pointer_menu_button, XmNactivateCallback,menu_link_for_mouse_menu,NULL);

    XtAddCallback(wx_station_button,    XmNactivateCallback,WX_station,NULL);
    XtAddCallback(jump_button,          XmNactivateCallback, Jump_location, NULL);
    XtAddCallback(locate_place_button,  XmNactivateCallback,Locate_place,NULL);
    XtAddCallback(coordinate_calculator_button, XmNactivateCallback,Coordinate_calc,"");
 
    XtAddCallback(send_message_to_button,       XmNactivateCallback,Send_message,NULL);
    XtAddCallback(open_messages_group_button,   XmNactivateCallback,Send_message,"*");
    XtAddCallback(clear_messages_button,XmNactivateCallback,Clear_messages,NULL);
    XtAddCallback(save_button,          XmNactivateCallback,Save_Config,NULL);
    XtAddCallback(test_button,          XmNactivateCallback,Test,NULL);
    if (!debug_level) {
            XtSetSensitive(test_button, False);
    }

    XtAddCallback(debug_level_button,   XmNactivateCallback, Change_Debug_Level,NULL);
//    XtSetSensitive(debug_level_button, False);

    XtAddCallback(uptime_button,   XmNactivateCallback, Compute_Uptime,NULL);
    //XtSetSensitive(uptime_button, False);



    // Toolbar
    toolbar = XtVaCreateWidget("Toolbar form",
            xmFormWidgetClass, 
            form,
            XmNtopAttachment, XmATTACH_FORM,
            XmNbottomAttachment, XmATTACH_NONE,
            XmNleftAttachment, XmATTACH_WIDGET,
            XmNleftWidget, menubar,
            XmNleftOffset, 0,
            XmNrightAttachment, XmATTACH_NONE,
            XmNfractionBase, 3,
            XmNautoUnmanage, FALSE,
            XmNshadowThickness, 1,
            XmNnavigationType, XmTAB_GROUP,
            XmNtraversalOn, FALSE,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    trackme_frame = XtVaCreateManagedWidget("Trackme frame",
            xmFrameWidgetClass,
            toolbar,
            XmNtopAttachment, XmATTACH_FORM,
            XmNbottomAttachment, XmATTACH_FORM,
            XmNleftAttachment, XmATTACH_FORM,
            XmNleftOffset, 0,
            XmNrightAttachment, XmATTACH_NONE,
            XmNnavigationType, XmTAB_GROUP,
            XmNtraversalOn, FALSE,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    trackme_button=XtVaCreateManagedWidget(langcode("POPUPMA022"),
            xmToggleButtonGadgetClass,
            trackme_frame,
            XmNtopAttachment, XmATTACH_FORM,
            XmNbottomAttachment, XmATTACH_FORM,
            XmNleftAttachment, XmATTACH_FORM,
            XmNleftOffset, 0,
            XmNrightAttachment, XmATTACH_FORM,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNnavigationType, XmTAB_GROUP,
            XmNtraversalOn, FALSE,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(trackme_button,XmNvalueChangedCallback,Track_Me,"1");

    measure_frame = XtVaCreateManagedWidget("Measure frame",
            xmFrameWidgetClass,
            toolbar,
            XmNtopAttachment, XmATTACH_FORM,
            XmNbottomAttachment, XmATTACH_FORM,
            XmNleftAttachment, XmATTACH_WIDGET,
            XmNleftWidget, trackme_frame,
            XmNleftOffset, 0,
            XmNrightAttachment, XmATTACH_NONE,
            XmNnavigationType, XmTAB_GROUP,
            XmNtraversalOn, FALSE,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    measure_button=XtVaCreateManagedWidget(langcode("POPUPMA020"),
            xmToggleButtonGadgetClass,
            measure_frame,
            XmNtopAttachment, XmATTACH_FORM,
            XmNbottomAttachment, XmATTACH_FORM,
            XmNleftAttachment, XmATTACH_FORM,
            XmNleftOffset, 0,
            XmNrightAttachment, XmATTACH_FORM,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNnavigationType, XmTAB_GROUP,
            XmNtraversalOn, FALSE,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(measure_button,XmNvalueChangedCallback,Measure_Distance,"1");

    move_frame = XtVaCreateManagedWidget("Move frame",
            xmFrameWidgetClass,
            toolbar,
            XmNtopAttachment, XmATTACH_FORM,
            XmNbottomAttachment, XmATTACH_FORM,
            XmNleftAttachment, XmATTACH_WIDGET,
            XmNleftWidget, measure_frame,
            XmNleftOffset, 0,
            XmNrightAttachment, XmATTACH_NONE,
            XmNnavigationType, XmTAB_GROUP,
            XmNtraversalOn, FALSE,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    move_button=XtVaCreateManagedWidget(langcode("POPUPMA021"),
            xmToggleButtonGadgetClass,
            move_frame,
            XmNtopAttachment, XmATTACH_FORM,
            XmNbottomAttachment, XmATTACH_FORM,
            XmNleftAttachment, XmATTACH_FORM,
            XmNleftOffset, 0,
            XmNrightAttachment, XmATTACH_FORM,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNnavigationType, XmTAB_GROUP,
            XmNtraversalOn, FALSE,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(move_button,XmNvalueChangedCallback,Move_Object,"1");



#ifdef ARROWS
    pan_left_menu=XtVaCreateManagedWidget("create_appshell arrow1_menu",
            xmArrowButtonGadgetClass,
            toolbar,
            XmNtopAttachment, XmATTACH_FORM,
            XmNbottomAttachment, XmATTACH_FORM,
            XmNleftAttachment, XmATTACH_WIDGET,
            XmNleftWidget, move_frame,
            XmNleftOffset, 0,
            XmNrightAttachment, XmATTACH_NONE,
            XmNarrowDirection,  XmARROW_LEFT,
            XmNnavigationType, XmTAB_GROUP,
            XmNtraversalOn, FALSE,
            NULL);
    XtAddCallback(pan_left_menu,XmNactivateCallback,Pan_left,NULL);

    pan_up_menu=XtVaCreateManagedWidget("create_appshell arrow2_menu",
            xmArrowButtonGadgetClass, 
            toolbar,
            XmNtopAttachment, XmATTACH_FORM,
            XmNbottomAttachment, XmATTACH_FORM,
            XmNleftAttachment, XmATTACH_WIDGET,
            XmNleftWidget, pan_left_menu,
            XmNleftOffset, 0,
            XmNrightAttachment, XmATTACH_NONE,
            XmNarrowDirection,  XmARROW_UP,
            XmNnavigationType, XmTAB_GROUP,
            XmNtraversalOn, FALSE,
            NULL);
    XtAddCallback(pan_up_menu,XmNactivateCallback,Pan_up,NULL);

    pan_down_menu=XtVaCreateManagedWidget("create_appshell arrow3_menu",
            xmArrowButtonGadgetClass, 
            toolbar,
            XmNtopAttachment, XmATTACH_FORM,
            XmNbottomAttachment, XmATTACH_FORM,
            XmNleftAttachment, XmATTACH_WIDGET,
            XmNleftWidget, pan_up_menu,
            XmNleftOffset, 0,
            XmNrightAttachment, XmATTACH_NONE,
            XmNarrowDirection,  XmARROW_DOWN,
            XmNnavigationType, XmTAB_GROUP,
            XmNtraversalOn, FALSE,
            NULL);
    XtAddCallback(pan_down_menu,XmNactivateCallback,Pan_down,NULL);

    pan_right_menu=XtVaCreateManagedWidget("create_appshell arrow4_menu",
            xmArrowButtonGadgetClass, 
            toolbar,
            XmNtopAttachment, XmATTACH_FORM,
            XmNbottomAttachment, XmATTACH_FORM,
            XmNleftAttachment, XmATTACH_WIDGET,
            XmNleftWidget, pan_down_menu,
            XmNleftOffset, 0,
            XmNrightAttachment, XmATTACH_NONE,
            XmNarrowDirection,  XmARROW_RIGHT,
            XmNnavigationType, XmTAB_GROUP,
            XmNtraversalOn, FALSE,
            NULL);
    XtAddCallback(pan_right_menu,XmNactivateCallback,Pan_right,NULL);
#endif // ARROWS



#define FONT_WIDTH 9

    /* Create bottom text area */
    text = XtVaCreateWidget("create_appshell text_output", 
            xmTextFieldWidgetClass,
            form,
            XmNeditable,            FALSE,
            XmNcursorPositionVisible, FALSE,
            XmNsensitive,           STIPPLE,
            XmNshadowThickness,     1,
            XmNcolumns,             30,
            XmNwidth,               ((29*FONT_WIDTH)+2),
            XmNtopOffset,           4,
            XmNtopAttachment,       XmATTACH_NONE,
            XmNbottomAttachment,    XmATTACH_FORM,
            XmNleftAttachment,      XmATTACH_FORM,
            XmNrightAttachment,     XmATTACH_NONE,
            XmNnavigationType,      XmTAB_GROUP,
            XmNtraversalOn,         FALSE,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    text2 = XtVaCreateWidget("create_appshell text_output2",
            xmTextFieldWidgetClass,
            form,
            XmNeditable,            FALSE,
            XmNcursorPositionVisible, FALSE,
            XmNsensitive,           STIPPLE,
            XmNshadowThickness,     1,
            XmNcolumns,             35,
            XmNwidth,               ((24*FONT_WIDTH)+2),
            XmNtopAttachment,       XmATTACH_NONE,
            XmNbottomAttachment,    XmATTACH_FORM,
            XmNleftAttachment,      XmATTACH_WIDGET,
            XmNleftWidget,          text,
            XmNrightAttachment,     XmATTACH_NONE,
            XmNnavigationType,      XmTAB_GROUP,
            XmNtraversalOn,         FALSE,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    text3 = XtVaCreateWidget("create_appshell text_output3", 
            xmTextFieldWidgetClass, 
            form,
            XmNeditable,            FALSE,
            XmNcursorPositionVisible, FALSE,
            XmNsensitive,           STIPPLE,
            XmNshadowThickness,     1,
            XmNcolumns,             14,
            XmNwidth,               ((10*FONT_WIDTH)+2),
            XmNtopAttachment,       XmATTACH_NONE,
            XmNbottomAttachment,    XmATTACH_FORM,
            XmNleftAttachment,      XmATTACH_WIDGET,
            XmNleftWidget,          text2,
            XmNrightAttachment,     XmATTACH_NONE,
            XmNnavigationType,      XmTAB_GROUP,
            XmNtraversalOn,         FALSE,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    text4 = XtVaCreateWidget("create_appshell text_output4", 
            xmTextFieldWidgetClass, 
            form,
            XmNeditable,            FALSE,
            XmNcursorPositionVisible, FALSE,
            XmNsensitive,           STIPPLE,
            XmNshadowThickness,     1,
            XmNcolumns,             12,
            XmNwidth,               ((10*FONT_WIDTH)+2),
            XmNtopAttachment,       XmATTACH_NONE,
            XmNbottomAttachment,    XmATTACH_FORM,
            XmNleftAttachment,      XmATTACH_WIDGET,
            XmNleftWidget,          text3,
            XmNrightAttachment,     XmATTACH_NONE,
            XmNnavigationType,      XmTAB_GROUP,
            XmNtraversalOn,         FALSE,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    iface_da = XtVaCreateWidget("create_appshell iface", 
            xmDrawingAreaWidgetClass,
            form,
            XmNwidth,               22*(MAX_IFACE_DEVICES/2),
            XmNheight,              20,
            XmNunitType,            XmPIXELS,
            XmNtopAttachment,       XmATTACH_NONE,
            XmNbottomAttachment,    XmATTACH_FORM,
            XmNbottomOffset,        5,
            XmNleftAttachment,      XmATTACH_WIDGET,
            XmNleftWidget,          text4,
            XmNrightAttachment,     XmATTACH_NONE,
            XmNnavigationType,      XmTAB_GROUP,
            XmNtraversalOn,         FALSE,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    sep = XtVaCreateManagedWidget("create_appshell sep", 
            xmSeparatorGadgetClass,
            form,
            XmNorientation,         XmHORIZONTAL,
            XmNtopAttachment,       XmATTACH_NONE,
            XmNbottomAttachment,    XmATTACH_WIDGET,
            XmNbottomWidget,        text,
            XmNleftAttachment,      XmATTACH_FORM,
            XmNrightAttachment,     XmATTACH_FORM,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    /* Do drawing map area */
    da = XtVaCreateWidget("create_appshell da",
            xmDrawingAreaWidgetClass,
            form,
            XmNwidth,               screen_width,
            XmNheight,              screen_height,
            XmNunitType,            XmPIXELS,
            XmNtopAttachment,       XmATTACH_WIDGET,
            XmNtopWidget,           menubar,
            XmNbottomAttachment,    XmATTACH_WIDGET,
            XmNbottomWidget,        sep,
            XmNleftAttachment,      XmATTACH_FORM,
            XmNrightAttachment,     XmATTACH_FORM,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);


//-------------------------------------------------------------------------


    // Create the mouse menus here
    ac = 0;
    XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
    XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
    XtSetArg(al[ac], XmNmenuPost, "<Btn3Down>"); ac++;  // Set for popup menu on button 3
    XtSetArg(al[ac], XmNnavigationType, XmTAB_GROUP); ac++;
    XtSetArg(al[ac], XmNtraversalOn, TRUE); ac++;

    // Right menu popup (right mouse button or button 3)
    right_menu_popup = XmCreatePopupMenu(da,
            "create_appshell Menu Popup",
            al,
            ac);
    //XtVaSetValues(right_menu_popup, XmNwhichButton, 3, NULL);

    ac = 0;
    XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
    XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
    XtSetArg(al[ac], XmNnavigationType, XmTAB_GROUP); ac++;
    XtSetArg(al[ac], XmNtraversalOn, TRUE); ac++;

    // "Options"
    (void)XtCreateManagedWidget(langcode("POPUPMA001"),
            xmLabelWidgetClass,
            right_menu_popup,
            al,
            ac);
    (void)XtCreateManagedWidget("create_appshell sep",
            xmSeparatorWidgetClass,
            right_menu_popup,
            al,
            ac);

    // "Center"
    ac = 0;
    XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
    XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
    XtSetArg(al[ac], XmNnavigationType, XmTAB_GROUP); ac++;
    XtSetArg(al[ac], XmNtraversalOn, TRUE); ac++;
    XtSetArg(al[ac], XmNmnemonic, langcode_hotkey("POPUPMA00c")); ac++;
    pan_ctr=XtCreateManagedWidget(langcode("POPUPMA00c"),
            xmPushButtonGadgetClass,
            right_menu_popup,
            al,
            ac);
    XtAddCallback(pan_ctr,XmNactivateCallback,Pan_ctr,NULL);

    // "Station info"
    ac = 0;
    XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
    XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
    XtSetArg(al[ac], XmNnavigationType, XmTAB_GROUP); ac++;
    XtSetArg(al[ac], XmNtraversalOn, TRUE); ac++;
    XtSetArg(al[ac], XmNmnemonic, langcode_hotkey("POPUPMA015")); ac++;
    station_info=XtCreateManagedWidget(langcode("POPUPMA015"),
            xmPushButtonGadgetClass,
            right_menu_popup,
            al,
            ac);
    XtAddCallback(station_info,XmNactivateCallback,Station_info,NULL);

    // Zoom in"
    ac = 0;
    XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
    XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
    XtSetArg(al[ac], XmNnavigationType, XmTAB_GROUP); ac++;
    XtSetArg(al[ac], XmNtraversalOn, TRUE); ac++;
    XtSetArg(al[ac], XmNmnemonic, langcode_hotkey("POPUPMA002")); ac++;
    zoom_in=XtCreateManagedWidget(langcode("POPUPMA002"),
            xmPushButtonGadgetClass,
            right_menu_popup,
            al,
            ac);
    XtAddCallback(zoom_in,XmNactivateCallback,Zoom_in,NULL);

    // Zoom out" 
    ac = 0;
    XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
    XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
    XtSetArg(al[ac], XmNnavigationType, XmTAB_GROUP); ac++;
    XtSetArg(al[ac], XmNtraversalOn, TRUE); ac++;
    XtSetArg(al[ac], XmNmnemonic, langcode_hotkey("POPUPMA003")); ac++;
    zoom_out=XtCreateManagedWidget(langcode("POPUPMA003"),
            xmPushButtonGadgetClass,
            right_menu_popup,
            al,
            ac);
    XtAddCallback(zoom_out,XmNactivateCallback,Zoom_out,NULL);

    zoom_sub=XmCreatePulldownMenu(right_menu_popup,
            "create_appshell zoom sub",
            al,
            ac);
 
    // "Zoom level"
    zoom_level=XtVaCreateManagedWidget(langcode("POPUPMA004"),
            xmCascadeButtonGadgetClass,
            right_menu_popup,
            XmNsubMenuId,zoom_sub,
            XmNmnemonic,langcode_hotkey("POPUPMA004"),
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    // "1"
    ac = 0;
    XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
    XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
    XtSetArg(al[ac], XmNnavigationType, XmTAB_GROUP); ac++;
    XtSetArg(al[ac], XmNtraversalOn, TRUE); ac++;
    XtSetArg(al[ac], XmNmnemonic, langcode_hotkey("POPUPMA005")); ac++;
    zl1=XtCreateManagedWidget(langcode("POPUPMA005"),
            xmPushButtonGadgetClass,
            zoom_sub,
            al,
            ac);
    XtAddCallback(zl1,XmNactivateCallback,Zoom_level,"1");

    // "16"
    ac = 0;
    XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
    XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
    XtSetArg(al[ac], XmNnavigationType, XmTAB_GROUP); ac++;
    XtSetArg(al[ac], XmNtraversalOn, TRUE); ac++;
    XtSetArg(al[ac], XmNmnemonic, langcode_hotkey("POPUPMA006")); ac++;
    zl2=XtCreateManagedWidget(langcode("POPUPMA006"),
            xmPushButtonGadgetClass,
            zoom_sub,
            al,
            ac);
    XtAddCallback(zl2,XmNactivateCallback,Zoom_level,"2");

    // "64"
    ac = 0;
    XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
    XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
    XtSetArg(al[ac], XmNnavigationType, XmTAB_GROUP); ac++;
    XtSetArg(al[ac], XmNtraversalOn, TRUE); ac++;
    XtSetArg(al[ac], XmNmnemonic, langcode_hotkey("POPUPMA007")); ac++;
    zl3=XtCreateManagedWidget(langcode("POPUPMA007"),
            xmPushButtonGadgetClass,
            zoom_sub,
            al,
            ac);
    XtAddCallback(zl3,XmNactivateCallback,Zoom_level,"3");

    // "256"
    ac = 0;
    XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
    XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
    XtSetArg(al[ac], XmNnavigationType, XmTAB_GROUP); ac++;
    XtSetArg(al[ac], XmNtraversalOn, TRUE); ac++;
    XtSetArg(al[ac], XmNmnemonic, langcode_hotkey("POPUPMA008")); ac++;
    zl4=XtCreateManagedWidget(langcode("POPUPMA008"),
            xmPushButtonGadgetClass,
            zoom_sub,
            al,
            ac);
    XtAddCallback(zl4,XmNactivateCallback,Zoom_level,"4");

    // "1024"
    ac = 0;
    XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
    XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
    XtSetArg(al[ac], XmNnavigationType, XmTAB_GROUP); ac++;
    XtSetArg(al[ac], XmNtraversalOn, TRUE); ac++;
    XtSetArg(al[ac], XmNmnemonic, langcode_hotkey("POPUPMA009")); ac++;
    zl5=XtCreateManagedWidget(langcode("POPUPMA009"),
            xmPushButtonGadgetClass,
            zoom_sub,
            al,
            ac);
    XtAddCallback(zl5,XmNactivateCallback,Zoom_level,"5");

    // "8192"
    ac = 0;
    XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
    XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
    XtSetArg(al[ac], XmNnavigationType, XmTAB_GROUP); ac++;
    XtSetArg(al[ac], XmNtraversalOn, TRUE); ac++;
    XtSetArg(al[ac], XmNmnemonic, langcode_hotkey("POPUPMA010")); ac++;
    zl6=XtCreateManagedWidget(langcode("POPUPMA010"),
            xmPushButtonGadgetClass,
            zoom_sub,
            al,
            ac);
    XtAddCallback(zl6,XmNactivateCallback,Zoom_level,"6");

    // "Entire World"
    ac = 0;
    XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
    XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
    XtSetArg(al[ac], XmNnavigationType, XmTAB_GROUP); ac++;
    XtSetArg(al[ac], XmNtraversalOn, TRUE); ac++;
    XtSetArg(al[ac], XmNmnemonic, langcode_hotkey("POPUPMA017")); ac++;
    zl7=XtCreateManagedWidget(langcode("POPUPMA017"),
            xmPushButtonGadgetClass,
            zoom_sub,
            al,
            ac);
    XtAddCallback(zl7,XmNactivateCallback,Zoom_level,"7");

    // "10% out"
    ac = 0;
    XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
    XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
    XtSetArg(al[ac], XmNnavigationType, XmTAB_GROUP); ac++;
    XtSetArg(al[ac], XmNtraversalOn, TRUE); ac++;
    XtSetArg(al[ac], XmNmnemonic, 'o'); ac++;
    zl8=XtCreateManagedWidget("10% out" ,
            xmPushButtonGadgetClass,
            zoom_sub,
            al,
            ac);
    XtAddCallback(zl8,XmNactivateCallback,Zoom_level,"8");

    // "10% in"
    ac = 0;
    XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
    XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
    XtSetArg(al[ac], XmNnavigationType, XmTAB_GROUP); ac++;
    XtSetArg(al[ac], XmNtraversalOn, TRUE); ac++;
    XtSetArg(al[ac], XmNmnemonic, 'i'); ac++;
    zl9=XtCreateManagedWidget("10% in" ,
            xmPushButtonGadgetClass,
            zoom_sub,
            al,
            ac);
    XtAddCallback(zl9,XmNactivateCallback,Zoom_level,"9");

    // "Last map pos/zoom"
    ac = 0;
    XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
    XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
    XtSetArg(al[ac], XmNnavigationType, XmTAB_GROUP); ac++;
    XtSetArg(al[ac], XmNtraversalOn, TRUE); ac++;
    XtSetArg(al[ac], XmNmnemonic, langcode_hotkey("POPUPMA016")); ac++;
    last_loc=XtCreateManagedWidget(langcode("POPUPMA016"),
            xmPushButtonGadgetClass,
            right_menu_popup,
            al,
            ac);
    XtAddCallback(last_loc,XmNactivateCallback,Last_location,NULL);

    (void)XtCreateManagedWidget("create_appshell sep",
            xmSeparatorWidgetClass,
            right_menu_popup,
            al,
            ac);

    // "Object -> Create"
    ac = 0;
    XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
    XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
    XtSetArg(al[ac], XmNnavigationType, XmTAB_GROUP); ac++;
    XtSetArg(al[ac], XmNtraversalOn, TRUE); ac++;
    XtSetArg(al[ac], XmNmnemonic, langcode_hotkey("POPUPMA018")); ac++;
    set_object=XtCreateManagedWidget(langcode("POPUPMA018"),
            xmPushButtonGadgetClass,
            right_menu_popup,
            al,
            ac);
    XtAddCallback(set_object,XmNactivateCallback,Set_Del_Object,NULL);

    // "Object -> Modify"
    ac = 0;
    XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
    XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
    XtSetArg(al[ac], XmNnavigationType, XmTAB_GROUP); ac++;
    XtSetArg(al[ac], XmNtraversalOn, TRUE); ac++;
    XtSetArg(al[ac], XmNmnemonic, langcode_hotkey("POPUPMA019")); ac++;
    modify_object=XtCreateManagedWidget(langcode("POPUPMA019"),
            xmPushButtonGadgetClass,
            right_menu_popup,
            al,
            ac);
    XtAddCallback(modify_object,XmNactivateCallback,Station_info,"1");

    XtCreateManagedWidget("create_appshell sep",
            xmSeparatorWidgetClass,
            right_menu_popup,
            al,
            ac);

    // "Pan Up"
    ac = 0;
    XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
    XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
    XtSetArg(al[ac], XmNnavigationType, XmTAB_GROUP); ac++;
    XtSetArg(al[ac], XmNtraversalOn, TRUE); ac++;
    XtSetArg(al[ac], XmNmnemonic, langcode_hotkey("POPUPMA011")); ac++;
    pan_up=XtCreateManagedWidget(langcode("POPUPMA011"),
            xmPushButtonGadgetClass,
            right_menu_popup,
            al,
            ac);
    //pan_up=XtVaCreateManagedWidget("create_appshell arrow1",
    //    xmArrowButtonGadgetClass, 
    //    right_menu_popup,
    //    XmNarrowDirection,  XmARROW_UP,
    //    NULL);
    XtAddCallback(pan_up,XmNactivateCallback,Pan_up,NULL);

    // "Pan Left"
    ac = 0;
    XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
    XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
    XtSetArg(al[ac], XmNnavigationType, XmTAB_GROUP); ac++;
    XtSetArg(al[ac], XmNtraversalOn, TRUE); ac++;
    XtSetArg(al[ac], XmNmnemonic, langcode_hotkey("POPUPMA013")); ac++;
    pan_left=XtCreateManagedWidget(langcode("POPUPMA013"),
            xmPushButtonGadgetClass,
            right_menu_popup,
            al,
            ac);
    //pan_left=XtVaCreateManagedWidget("create_appshell arrow3",
    //    xmArrowButtonGadgetClass, 
    //    right_menu_popup,
    //    XmNarrowDirection,  XmARROW_LEFT,
    //    NULL);
    XtAddCallback(pan_left,XmNactivateCallback,Pan_left,NULL);

    // "Pan Right"
    ac = 0;
    XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
    XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
    XtSetArg(al[ac], XmNnavigationType, XmTAB_GROUP); ac++;
    XtSetArg(al[ac], XmNtraversalOn, TRUE); ac++;
    XtSetArg(al[ac], XmNmnemonic, langcode_hotkey("POPUPMA014")); ac++;
    pan_right=XtCreateManagedWidget(langcode("POPUPMA014"),
            xmPushButtonGadgetClass,
            right_menu_popup,
            al,
            ac);
    //pan_right=XtVaCreateManagedWidget("create_appshell arrow4",
    //    xmArrowButtonGadgetClass, 
    //    right_menu_popup,
    //    XmNarrowDirection,  XmARROW_RIGHT,
    //    NULL);
    XtAddCallback(pan_right,XmNactivateCallback,Pan_right,NULL);

    // "Pan Down"
    ac = 0;
    XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
    XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
    XtSetArg(al[ac], XmNnavigationType, XmTAB_GROUP); ac++;
    XtSetArg(al[ac], XmNtraversalOn, TRUE); ac++;
    XtSetArg(al[ac], XmNmnemonic, langcode_hotkey("POPUPMA012")); ac++;
    pan_down=XtCreateManagedWidget(langcode("POPUPMA012"),
            xmPushButtonGadgetClass,
            right_menu_popup,
            al,
            ac);
    //pan_down=XtVaCreateManagedWidget("create_appshell arrow2",
    //    xmArrowButtonGadgetClass, 
    //    right_menu_popup,
    //    XmNarrowDirection,  XmARROW_DOWN,
    //    NULL);
    XtAddCallback(pan_down,XmNactivateCallback,Pan_down,NULL);


//-------------------------------------------------------------------------


    /* mouse tracking */
    XtAddEventHandler(da,LeaveWindowMask,FALSE,ClearTrackMouse,(XtPointer)text2);
    XtAddEventHandler(da,PointerMotionMask,FALSE,TrackMouse,(XtPointer)text2);

    // Popup menus
    XtAddEventHandler(da, ButtonPressMask, FALSE, (XtEventHandler)Mouse_button_handler, NULL);
    //XtAddEventHandler(da,ButtonReleaseMask,FALSE,(XtEventHandler)Mouse_button_handler,NULL);


    ac = 0;
    children[ac++] = text;
    children[ac++] = text2;
    children[ac++] = text3;
    children[ac++] = text4;
    children[ac++] = iface_da;
    children[ac++] = menubar;
    children[ac++] = toolbar;
    children[ac++] = da;
 
    XtManageChildren(children, ac);
    ac = 0;

    // This one needs to be done after all of
    // the above 'cuz it contains all of them.
    XtManageChild(form);

    WM_DELETE_WINDOW = XmInternAtom(XtDisplay(appshell),"WM_DELETE_WINDOW", FALSE);
    XmAddWMProtocolCallback(appshell, WM_DELETE_WINDOW, Window_Quit, (XtPointer) NULL);

    XmTextFieldSetString(text,"");
    XtManageChild(text);

    display_zoom_status();

    XtManageChild(text);

    (void)XtCreateManagedWidget("MAIN",
            xmMainWindowWidgetClass,
            appshell,
            NULL,
            0);

    XtRealizeWidget (appshell);

    create_gc(da);

    // Fill the drawing area with the background color.
    (void)XSetForeground(XtDisplay(da),gc,MY_BG_COLOR); // Not a mistake!
    (void)XSetBackground(XtDisplay(da),gc,MY_BG_COLOR);
    (void)XFillRectangle(XtDisplay(appshell),XtWindow(da),gc,0,0,screen_width,screen_height);

    // Set to the proper size before we make the window visible on the screen
    XtVaSetValues(appshell,
                XmNx,           1,
                XmNy,           1,
                XmNwidth,       screen_width,
//              XmNheight,      (screen_height+60+2),   // DK7IN: added 2 because height had been smaller everytime
                XmNheight,      (screen_height + 60),   // WE7U:  Above statement makes mine grow by 2 each time
                NULL);

    // Show the window
    XtPopup(appshell,XtGrabNone);

    XtAddCallback (da, XmNinputCallback,  da_input,NULL);
    XtAddCallback (da, XmNresizeCallback, da_resize,NULL);
    XtAddCallback (da, XmNexposeCallback, da_expose,(XtPointer)text);

    if (track_me)
        XmToggleButtonSetState(trackme_button,TRUE,TRUE);
    else
        XmToggleButtonSetState(trackme_button,FALSE,TRUE);

    if(debug_level & 8)
        printf("Create appshell stop\n");
}   // end of create_appshell()





void create_gc(Widget w) {
    XGCValues values;
    Display *my_display = XtDisplay(w);
    int mask = 0;
    Pixmap pix;
    int _w, _h, _xh, _yh;
    char xbm_path[500];

    if (debug_level & 8)
        printf("Create gc start\n");

    if (gc != 0)
        return;

    // Allocate colors
    colors[0x00] = (int)GetPixelByName(w,"DarkGreen");  // was darkgreen (same)
    colors[0x01] = (int)GetPixelByName(w,"purple");
    colors[0x02] = (int)GetPixelByName(w,"DarkGreen");  // was darkgreen (same)
    colors[0x03] = (int)GetPixelByName(w,"cyan");
    colors[0x04] = (int)GetPixelByName(w,"brown");
    colors[0x05] = (int)GetPixelByName(w,"plum");       // light magenta
    colors[0x06] = (int)GetPixelByName(w,"orange");
    colors[0x07] = (int)GetPixelByName(w,"darkgray");
    colors[0x08] = (int)GetPixelByName(w,"black");      // Foreground font color
    colors[0x09] = (int)GetPixelByName(w,"blue");
    colors[0x0a] = (int)GetPixelByName(w,"green");              // PHG (old)
    colors[0x0b] = (int)GetPixelByName(w,"mediumorchid"); // light purple
    colors[0x0c] = (int)GetPixelByName(w,"red");
    colors[0x0d] = (int)GetPixelByName(w,"magenta");
    colors[0x0e] = (int)GetPixelByName(w,"yellow");
    colors[0x0f] = (int)GetPixelByName(w,"white");              // 
    colors[0x10] = (int)GetPixelByName(w,"black");
    colors[0x11] = (int)GetPixelByName(w,"black");
    colors[0x12] = (int)GetPixelByName(w,"black");
    colors[0x13] = (int)GetPixelByName(w,"black");
    colors[0x14] = (int)GetPixelByName(w,"lightgray");
    colors[0x15] = (int)GetPixelByName(w,"magenta");
    colors[0x16] = (int)GetPixelByName(w,"mediumorchid"); // light purple
    colors[0x17] = (int)GetPixelByName(w,"lightblue");
    colors[0x18] = (int)GetPixelByName(w,"purple");
    colors[0x19] = (int)GetPixelByName(w,"orange2");    // light orange
    colors[0x1a] = (int)GetPixelByName(w,"SteelBlue");
    colors[0x20] = (int)GetPixelByName(w,"white");

    // Area object colors.  Order must not be changed. If beginning moves,
    // update draw_area and draw_map.
    // High
    colors[0x21] = (int)GetPixelByName(w,"black");   // AREA_BLACK_HI
    colors[0x22] = (int)GetPixelByName(w,"blue");    // AREA_BLUE_HI
    colors[0x23] = (int)GetPixelByName(w,"green");   // AREA_GREEN_HI
    colors[0x24] = (int)GetPixelByName(w,"cyan3");    // AREA_CYAN_HI
    colors[0x25] = (int)GetPixelByName(w,"red");     // AREA_RED_HI
    colors[0x26] = (int)GetPixelByName(w,"magenta"); // AREA_VIOLET_HI
    colors[0x27] = (int)GetPixelByName(w,"yellow");  // AREA_YELLOW_HI
    colors[0x28] = (int)GetPixelByName(w,"gray35");  // AREA_GRAY_HI
    // Low
    colors[0x29] = (int)GetPixelByName(w,"gray27");   // AREA_BLACK_LO
    colors[0x2a] = (int)GetPixelByName(w,"blue4");    // AREA_BLUE_LO
    colors[0x2b] = (int)GetPixelByName(w,"green4");   // AREA_GREEN_LO
    colors[0x2c] = (int)GetPixelByName(w,"cyan4");    // AREA_CYAN_LO
    colors[0x2d] = (int)GetPixelByName(w,"red4");     // AREA_RED_LO
    colors[0x2e] = (int)GetPixelByName(w,"magenta4"); // AREA_VIOLET_LO
    colors[0x2f] = (int)GetPixelByName(w,"yellow4");  // AREA_YELLOW_LO
    colors[0x30] = (int)GetPixelByName(w,"gray53"); // AREA_GRAY_LO

    colors[0x40] = (int)GetPixelByName(w,"yellow");     // symbols ...
    colors[0x41] = (int)GetPixelByName(w,"DarkOrange3");
    colors[0x42] = (int)GetPixelByName(w,"purple");
    colors[0x43] = (int)GetPixelByName(w,"gray80");
    colors[0x44] = (int)GetPixelByName(w,"red3");
    colors[0x45] = (int)GetPixelByName(w,"brown1");
    colors[0x46] = (int)GetPixelByName(w,"brown3");
    colors[0x47] = (int)GetPixelByName(w,"blue4");
    colors[0x48] = (int)GetPixelByName(w,"DeepSkyBlue");
    colors[0x49] = (int)GetPixelByName(w,"DarkGreen");
    colors[0x4a] = (int)GetPixelByName(w,"red2");
    colors[0x4b] = (int)GetPixelByName(w,"green3");
    colors[0x4c] = (int)GetPixelByName(w,"MediumBlue");
    colors[0x4d] = (int)GetPixelByName(w,"white");
    colors[0x4e] = (int)GetPixelByName(w,"gray53");
    colors[0x4f] = (int)GetPixelByName(w,"gray35");
    colors[0x50] = (int)GetPixelByName(w,"gray27");
    colors[0x51] = (int)GetPixelByName(w,"black");      // ... symbols

    colors[0x52] = (int)GetPixelByName(w,"LimeGreen");  // PHG, symbols

    colors[0xfe] = (int)GetPixelByName(w,"pink");

    // map solid colors
    colors[0x60] = (int)GetPixelByName(w,"HotPink");
    colors[0x61] = (int)GetPixelByName(w,"RoyalBlue");
    colors[0x62] = (int)GetPixelByName(w,"orange3");
    colors[0x63] = (int)GetPixelByName(w,"yellow2");
    colors[0x64] = (int)GetPixelByName(w,"ForestGreen");
    colors[0x65] = (int)GetPixelByName(w,"DodgerBlue");
    colors[0x66] = (int)GetPixelByName(w,"cyan2");
    colors[0x67] = (int)GetPixelByName(w,"plum2");
    colors[0x68] = (int)GetPixelByName(w,"MediumBlue"); // was blue3 (the same!)
    colors[0x69] = (int)GetPixelByName(w,"gray86");

    // tracking trail colors
    // set color for your own station with  #define MY_TRAIL_COLOR  in db.c
    trail_colors[0x00] = (int)GetPixelByName(w,"yellow");
    trail_colors[0x01] = (int)GetPixelByName(w,"blue");
    trail_colors[0x02] = (int)GetPixelByName(w,"green");
    trail_colors[0x03] = (int)GetPixelByName(w,"red");
    trail_colors[0x04] = (int)GetPixelByName(w,"magenta");
    trail_colors[0x05] = (int)GetPixelByName(w,"black");
    trail_colors[0x06] = (int)GetPixelByName(w,"white");
    trail_colors[0x07] = (int)GetPixelByName(w,"DarkOrchid");
    trail_colors[0x08] = (int)GetPixelByName(w,"purple");      // very similar to DarkOrchid...
    trail_colors[0x09] = (int)GetPixelByName(w,"OrangeRed");
    trail_colors[0x0a] = (int)GetPixelByName(w,"brown");
    trail_colors[0x0b] = (int)GetPixelByName(w,"DarkGreen");    // was darkgreen (same)
    trail_colors[0x0c] = (int)GetPixelByName(w,"MediumBlue");
    trail_colors[0x0d] = (int)GetPixelByName(w,"ForestGreen");
    trail_colors[0x0e] = (int)GetPixelByName(w,"chartreuse");
    trail_colors[0x0f] = (int)GetPixelByName(w,"cornsilk");
    trail_colors[0x10] = (int)GetPixelByName(w,"LightCyan");
    trail_colors[0x11] = (int)GetPixelByName(w,"cyan");
    trail_colors[0x12] = (int)GetPixelByName(w,"DarkSlateGray");
    trail_colors[0x13] = (int)GetPixelByName(w,"NavyBlue");
    trail_colors[0x14] = (int)GetPixelByName(w,"DarkOrange3");
    trail_colors[0x15] = (int)GetPixelByName(w,"gray27");
    trail_colors[0x16] = (int)GetPixelByName(w,"RoyalBlue");
    trail_colors[0x17] = (int)GetPixelByName(w,"yellow2");
    trail_colors[0x18] = (int)GetPixelByName(w,"DodgerBlue");
    trail_colors[0x19] = (int)GetPixelByName(w,"cyan2");
    trail_colors[0x1a] = (int)GetPixelByName(w,"MediumBlue"); // was blue3 (the same!)
    trail_colors[0x1b] = (int)GetPixelByName(w,"gray86");
    trail_colors[0x1c] = (int)GetPixelByName(w,"SteelBlue");
    trail_colors[0x1d] = (int)GetPixelByName(w,"PaleGreen");
    trail_colors[0x1e] = (int)GetPixelByName(w,"RosyBrown");
    trail_colors[0x1f] = (int)GetPixelByName(w,"DeepSkyBlue");

    values.background=GetPixelByName(w,"darkgray");

    gc = XCreateGC(my_display, XtWindow(w), mask, &values);

    gc_tint = XCreateGC(my_display, XtWindow(w), mask, &values);

    gc_stipple = XCreateGC(my_display, XtWindow(w), mask, &values);

    gc_bigfont = XCreateGC(my_display, XtWindow(w), mask, &values);

    pix =  XCreatePixmap(XtDisplay(w), RootWindowOfScreen(XtScreen(w)), 20, 20, 1);
    values.function = GXcopy;
    gc2 = XCreateGC(XtDisplay(w), pix,GCForeground|GCBackground|GCFunction, &values);

    pixmap=XCreatePixmap(XtDisplay(w),
                        DefaultRootWindow(XtDisplay(w)),
                        screen_width,screen_height,
                        DefaultDepthOfScreen(XtScreen(w)));

    pixmap_final=XCreatePixmap(XtDisplay(w),
                        DefaultRootWindow(XtDisplay(w)),
                        screen_width,screen_height,
                        DefaultDepthOfScreen(XtScreen(w)));

    pixmap_alerts=XCreatePixmap(XtDisplay(w),
                        DefaultRootWindow(XtDisplay(w)),
                        screen_width,screen_height,
                        DefaultDepthOfScreen(XtScreen(w)));

    xastir_snprintf(xbm_path, sizeof(xbm_path), "%s/%s", SYMBOLS_DIR, "2x2.xbm");
    XReadBitmapFile(XtDisplay(w), DefaultRootWindow(XtDisplay(w)),
                    xbm_path, &_w, &_h, &pixmap_50pct_stipple, &_xh, &_yh);

    xastir_snprintf(xbm_path, sizeof(xbm_path), "%s/%s", SYMBOLS_DIR, "25pct.xbm");
    XReadBitmapFile(XtDisplay(w), DefaultRootWindow(XtDisplay(w)),
                    xbm_path, &_w, &_h, &pixmap_25pct_stipple, &_xh, &_yh);

    xastir_snprintf(xbm_path, sizeof(xbm_path), "%s/%s", SYMBOLS_DIR, "13pct.xbm");
    XReadBitmapFile(XtDisplay(w), DefaultRootWindow(XtDisplay(w)),
                    xbm_path, &_w, &_h, &pixmap_13pct_stipple, &_xh, &_yh);

    xastir_snprintf(xbm_path, sizeof(xbm_path), "%s/%s", SYMBOLS_DIR, "alert.xbm");
    XReadBitmapFile(XtDisplay(w), DefaultRootWindow(XtDisplay(w)),
                    xbm_path, &_w, &_h, &pixmap_wx_stipple, &_xh, &_yh);

    display_up=1;

    wait_to_redraw=0;

    if (debug_level & 8)
        printf("Create gc stop\n");
}





void da_expose(Widget w, /*@unused@*/ XtPointer client_data, XtPointer call_data) {
    Dimension width, height, margin_width, margin_height;
    XmDrawingAreaCallbackStruct *db = (XmDrawingAreaCallbackStruct *)call_data;
    XExposeEvent *event = (XExposeEvent *) db->event;
    unsigned char   unit_type;

    /* Call a routine to create a Graphics Context */
    create_gc(w);
    /*printf("Expose event\n");*/
    /* First get the various dimensions */
    XtVaGetValues(w,
                XmNwidth, &width,
                XmNheight, &height,
                XmNmarginWidth, &margin_width,
                XmNmarginHeight, &margin_height,
                XmNunitType, &unit_type,
                0);

    (void)XCopyArea(XtDisplay(w),pixmap_final,XtWindow(w),gc,event->x,event->y,event->width,event->height,event->x,event->y);
}





void da_resize(Widget w, /*@unused@*/ XtPointer client_data, /*@unused@*/ XtPointer call_data) {
    Dimension width, height;

    if (XtIsRealized(w)){
        /* First get the various dimensions */
        XtVaGetValues(w,
            XmNwidth,   &width,
            XmNheight,  &height,
            0);

        screen_width  = (long)width;
        screen_height = (long)height;
        XtVaSetValues(w,
            XmNwidth,   screen_width,
            XmNheight,  screen_height,
            NULL);

        /*  printf("Size x:%ld, y:%ld\n",screen_width,screen_height);*/
        if (pixmap)
            (void)XFreePixmap(XtDisplay(w),pixmap);

        if(pixmap_final)
            (void)XFreePixmap(XtDisplay(w),pixmap_final);

        if(pixmap_alerts)
            (void)XFreePixmap(XtDisplay(w),pixmap_alerts);

        pixmap=XCreatePixmap(XtDisplay(w),
                DefaultRootWindow(XtDisplay(w)),
                width,height,
                DefaultDepthOfScreen(XtScreen(w)));

        pixmap_final=XCreatePixmap(XtDisplay(w),
                DefaultRootWindow(XtDisplay(w)),
                width,height,
                DefaultDepthOfScreen(XtScreen(w)));

        pixmap_alerts=XCreatePixmap(XtDisplay(w),
                DefaultRootWindow(XtDisplay(w)),
                width,height,
                DefaultDepthOfScreen(XtScreen(w)));

        setup_in_view();    // flag stations that are in screen view
        create_image(w);
        (void)XCopyArea(XtDisplay(w),pixmap_final,XtWindow(w),gc,0,0,screen_width,screen_height,0,0);
    }
}





void da_input(Widget w, XtPointer client_data, XtPointer call_data) {
    XEvent *event = ((XmDrawingAreaCallbackStruct *) call_data)->event;
    Dimension width, height;
    int redraw;
    char buffer[20];
    int bufsize = 20;
    char temp[200];
    char temp_course[20];
    KeySym key;
    XComposeStatus compose;
    int x_center;
    int y_center;
    int x_distance;
    int y_distance;
    float x_distance_real;
    float y_distance_real;
    float full_distance;
    float area;
    long a_x, a_y, b_x, b_y;
    char str_lat[20];
    char str_long[20];
    long x,y;


    XtVaGetValues(w,
            XmNwidth, &width,
            XmNheight, &height,
            0);

    /*printf("input event %d %d\n",event->type,ButtonPress);*/
    redraw=0;

    // Snag the current pointer position
    input_x = event->xbutton.x;
    input_y = event->xbutton.y;

    if (event->type == ButtonRelease) {
        //printf("ButtonRelease %d %d\n",event->xbutton.button,Button3);
        if (event->xbutton.button == Button1) {

// Left mouse button release

            // If no drag, Center the map on the mouse pointer
            // If drag, compute new zoom factor/center and redraw
            // -OR- measure distances.

            // Check for "Center Map" function.  Must be within 15
            // pixels of where the button press occurred to qualify.
            if ( mouse_zoom && !measuring_distance && !moving_object
                    && (abs(menu_x - input_x) < 15)
                    && (abs(menu_y - input_y) < 15) ) {
           /*     if(display_up) {
                    XtVaGetValues(da,XmNwidth, &width,XmNheight, &height,0);
                    new_mid_x = mid_x_long_offset - ((width *scale_x)/2) + (menu_x*scale_x);
                    new_mid_y = mid_y_lat_offset  - ((height*scale_y)/2) + (menu_y*scale_y);
                    new_scale_y = scale_y;          // keep size
                    display_zoom_image(1);          // check range and do display, recenter
                }
            */
            }
            // It's not the center function because the mouse moved more than 15 pixels.
            // It must be either the "Compute new zoom/center" -OR- the "Measure distance"
            // -OR- "Move distance" functions..  The "measuring_distance" or "moving_object"
            // variables will tell us.

            else {
                // At this stage we have menu_x/menu_y where the button press occurred,
                // and input_x/input_y where the button release occurred.

                if (measuring_distance) {   // Measure distance function
                    x_distance = abs(menu_x - input_x);
                    y_distance = abs(menu_y - input_y);

                    // Here we need to convert to either English or Metric units of distance.

                    //(temp,"Distance x:%d pixels,  y:%d pixels\n",x_distance,y_distance);
                    //popup_message(langcode("POPUPMA020"),temp);

                    XtVaGetValues(da,XmNwidth, &width,XmNheight, &height,0);
                    a_x = mid_x_long_offset  - ((width *scale_x)/2) + (menu_x*scale_x);
                    a_y = mid_y_lat_offset   - ((height*scale_y)/2) + (menu_y*scale_y);

                    b_x = mid_x_long_offset  - ((width *scale_x)/2) + (input_x*scale_x);
                    b_y = mid_y_lat_offset   - ((height*scale_y)/2) + (input_y*scale_y);

                    // Keep y constant to get x distance.  Convert
                    // to the current measurement units for display.
                    x_distance_real = cvt_kn2len * calc_distance_course(a_y,a_x,a_y,b_x,temp_course,sizeof(temp_course));
                    // Keep x constant to get y distance.  Convert
                    // to the current measurement units for display.
                    y_distance_real = cvt_kn2len * calc_distance_course(a_y,a_x,b_y,a_x,temp_course,sizeof(temp_course));
                    // Compute the total distance and course.
                    // Convert to the current measurement units for
                    // display.
                    full_distance = cvt_kn2len * calc_distance_course(a_y,a_x,b_y,b_x,temp_course,sizeof(temp_course));
 
                    if (full_distance < 1.0) {
                        switch (units_english_metric) {
                            case 1:     // English
                                full_distance   = full_distance   * 5280;   // convert from miles to feet
                                x_distance_real = x_distance_real * 5280;   // convert from miles to feet
                                y_distance_real = y_distance_real * 5280;   // convert from miles to feet
                                break;
                            case 2:     // Nautical miles and knots
                                full_distance   = full_distance   * 6076;   // convert from miles to feet
                                x_distance_real = x_distance_real * 6076;   // convert from miles to feet
                                y_distance_real = y_distance_real * 6076;   // convert from miles to feet
                                break;
                            default:    // Metric
                                full_distance   = full_distance   * 1000;   // convert from kilometers to meters
                                x_distance_real = x_distance_real * 1000;   // convert from kilometers to meters
                                y_distance_real = y_distance_real * 1000;   // convert from kilometers to meters
                                break;
                        }
                        // Compute the total area
                        area = x_distance_real * y_distance_real;
                            xastir_snprintf(temp,
                            sizeof(temp),
                            "%0.2f %s,  x=%0.2f %s,  y=%0.2f %s, %0.2f square %s,  Bearing: %s degrees",
                            full_distance, un_alt,      // feet/meters
                            x_distance_real, un_alt,    // feet/meters
                            y_distance_real, un_alt,    // feet/meters
                            area, un_alt,
                            temp_course);
                    }
                    else {
                        // Compute the total area
                        area = x_distance_real * y_distance_real;
                        xastir_snprintf(temp,
                            sizeof(temp),
                            "%0.2f %s,  x=%0.2f %s,  y=%0.2f %s, %0.2f square %s,  Bearing: %s degrees",
                            full_distance, un_dst,      // miles/kilometers
                            x_distance_real, un_dst,    // miles/kilometers
                            y_distance_real, un_dst,    // miles/kilometers
                            area, un_dst,
                            temp_course);
                    }
                    popup_message(langcode("POPUPMA020"),temp);
                }

                else if (moving_object) {   // Move Distance function
                    // For this function we need to:
                    //      Determine which icon is closest to the mouse pointer press position.
                    //          We'll use Station_info to select the icon for us.
                    //      Compute the lat/lon of the the mouse pointer release position.
                    //      Put the new value of lat/lon into the object data.
                    //      Cause symbols to get redrawn.

                    x = (mid_x_long_offset - ((screen_width  * scale_x)/2) + (event->xmotion.x * scale_x));
                    y = (mid_y_lat_offset  - ((screen_height * scale_y)/2) + (event->xmotion.y * scale_y));

                    if (x < 0)
                    x = 0l;                 // 180°W

                    if (x > 129600000l)
                    x = 129600000l;         // 180°E

                    if (y < 0)
                    y = 0l;                 //  90°N

                    if (y > 64800000l)
                    y = 64800000l;          //  90°S

                    if (debug_level & 1) {
                        // This math is only used for the debug mode printf below.
                        if (coordinate_system == USE_DDDDDD) {
                            convert_lat_l2s(y, str_lat, sizeof(str_lat), CONVERT_DEC_DEG);
                            convert_lon_l2s(x, str_long, sizeof(str_long), CONVERT_DEC_DEG);
                        } else if (coordinate_system == USE_DDMMSS) {
                            convert_lat_l2s(y, str_lat, sizeof(str_lat), CONVERT_DMS_NORMAL);
                            convert_lon_l2s(x, str_long, sizeof(str_long), CONVERT_DMS_NORMAL);
                        } else {    // Assume coordinate_system == USE_DDMMMM
                            convert_lat_l2s(y, str_lat, sizeof(str_lat), CONVERT_HP_NORMAL);
                            convert_lon_l2s(x, str_long, sizeof(str_long), CONVERT_HP_NORMAL);
                        }
                        //printf("%s  %s\n", str_lat, str_long);
                    }

                    Station_info(w, "2", NULL);
                }

                else {  // Must be "Compute new center/zoom" function
                    float ratio;

                    // We need to compute a new center and a new scale, then
                    // cause the new image to be created.
                    // Compute new center.  It'll be the average of the two points
                    x_center = (menu_x + input_x) /2;
                    y_center = (menu_y + input_y) /2;

                    XtVaGetValues(da,XmNwidth, &width,XmNheight, &height,0);
                    new_mid_x = mid_x_long_offset - ((width *scale_x)/2) + (x_center*scale_x);
                    new_mid_y = mid_y_lat_offset  - ((height*scale_y)/2) + (y_center*scale_y);

                    //
                    // What Rolf had to say:
                    //
                    // Calculate center of mouse-marked area and get the scaling relation
                    // between x and y for that position. This position will be the new
                    // center, so that lattitude-dependant relation does not change with
                    // a zoom-in. For both x and y calculate a new zoom factor neccessary
                    // to fit that screen direction. Select the one that allows both x
                    // and y part to fall into the screen area. Draw the new screen with
                    // new center and new zoom factor.
                    //

                    // Compute the new scale, or as close to it as we can get
                    //new_scale_y = scale_y / 2;    // Zoom in by a factor of 2
                    new_scale_y = (long)( (((float)abs(menu_y - input_y) / (float)height ) * (float)scale_y ) + 0.5);
                    new_scale_x = (long)( (((float)abs(menu_x - input_x) / (float)width  ) * (float)scale_x ) + 0.5);

                    if (new_scale_y < 1)
                        new_scale_y = 1;            // Don't go further in than zoom 1

                    if (new_scale_x < 1)
                        new_scale_x = 1;            // Don't go further in than zoom 1

                    // We now know approximately the scales we need
                    // in order to view all of the pixels just
                    // selected in the drag operation.  Now set
                    // new_scale_y to the highest number of the two,
                    // which will make sure the entire drag
                    // selection will be seen at the new zoom level.
                    // Use the new ratio between scales to compute
                    // this, computed from the new midpoint.
                    //
                    //printf("scale_x:%ld\tscale_y:%ld\n", get_x_scale(new_mid_x, new_mid_y, scale_y), scale_y );
                    ratio = ((float)get_x_scale(new_mid_x,new_mid_y,scale_y) / (float)scale_y);

                    //printf("Ratio: %f\n", ratio);
                    //printf("x:%ld\ty:%ld\n", new_scale_x, new_scale_y);
                    if ( new_scale_y < (long)((new_scale_x / ratio) + 0.5) ) {
                        new_scale_y =  (long)((new_scale_x / ratio) + 0.5);
                        //printf("Changed y\n");
                    }
                    //printf("x:%ld\ty:%ld\n", new_scale_x, new_scale_y);

                    display_zoom_image(1);          // Check range and do display, recenter

                    menu_x = input_x;
                    menu_y = input_y;
                    //printf("Drag/zoom/center happened\n");
                }
            }
            mouse_zoom = 0;
        }
        else if (event->xbutton.button == Button2) {

// Middle mouse button release

            // Zoom out 2x with panning
            menu_x=input_x;
            menu_y=input_y;
            Zoom_out( w, client_data, call_data );

            // Simple zoom out, keeping map center at current position
            //Zoom_out_no_pan( w, client_data, call_data );
            mouse_zoom = 0;
        }
        else if (event->xbutton.button == Button3) {

// Right mouse button release

            // Do nothing.  We have a popup tied into the button press anyway.
            // (Mouse_button_handler & right_menu_popup).
            // Leave the button release alone in this case.
            mouse_zoom = 0;
        }
        else if (event->xbutton.button == Button4) {
// Scroll up
            menu_x=input_x;
            menu_y=input_y;
            Pan_up(w, client_data, call_data);
        }
        else if (event->xbutton.button == Button5) {
// Scroll down
            menu_x=input_x;
            menu_y=input_y;
            Pan_down(w, client_data, call_data);
        }
        else if (event->xbutton.button == 6) {
// Mouse button 6 release
            menu_x=input_x;
            menu_y=input_y;
            Zoom_out_no_pan(w, client_data, call_data);
            mouse_zoom = 0;
        }
        else if (event->xbutton.button == 7) {
// Mouse button 7 release
            menu_x=input_x;
            menu_y=input_y;
            Zoom_in_no_pan(w, client_data, call_data);
            mouse_zoom = 0;
        }
    }
    else if (event->type == ButtonPress) {
        //printf("ButtonPress %d %d\n",event->xbutton.button,Button3);
        if (event->xbutton.button == Button1) {

// Left mouse button press

            // Mark the position for possible drag function
            menu_x=input_x;
            menu_y=input_y;
            mouse_zoom = 1;
        }
        else if (event->xbutton.button == Button2) {

// Middle mouse button or both right/left mouse buttons press

            // Nothing attached here.
            mouse_zoom = 0;
        }
        else if (event->xbutton.button == Button3) {

// Right mouse button press

            // Nothing attached here.
            mouse_zoom = 0;
        }
    }
    else if (event->type == KeyPress) {

        // We want to branch from the keysym instead of the keycode
        (void)XLookupString( (XKeyEvent *)event, buffer, bufsize, &key, &compose );
        //printf("main.c:da_input():keycode %d\tkeysym %ld\t%s\n", event->xkey.keycode, key, buffer);

        // keycode 99, keysym 65365 is PageUp (0xffda on sun kbd)
        if ((key == 65365) || (key == 0xffda)) {
            menu_x=input_x;
            menu_y=input_y;
            Zoom_out_no_pan( w, client_data, call_data );
            TrackMouse(w, (XtPointer)text2, event, NULL);
        }

        // keycode 105, keysym 65366 is PageDown (0xffe0 on sun kbd)
        if ((key == 65366) || (key == 0xffe0)) {
            menu_x=input_x;
            menu_y=input_y;
            Zoom_in_no_pan( w, client_data, call_data );
            TrackMouse(w, (XtPointer)text2, event, NULL);
        }

        // keycode 100, keysym 65361 is left-arrow
        if ( (key == 65361)
            || ( (key == 65361) && (event->xkey.state & ShiftMask) ) ) {    // Doesn't work yet.
            menu_x=input_x;
            menu_y=input_y;
            if (event->xbutton.state & ShiftMask)   // This doesn't work yet
                Pan_left_less( w, client_data, call_data);
            else
                Pan_left( w, client_data, call_data );
            TrackMouse(w, (XtPointer)text2, event, NULL);
        }

        // keycode 102, keysym 65363 is right-arrow
        if ( (key == 65363)
            || ( (key == 65363) && (event->xkey.state & ShiftMask) ) ) {    // Doesn't work yet.
            menu_x=input_x;
            menu_y=input_y;
            if (event->xbutton.state & ShiftMask)   // This doesn't work yet
                Pan_right_less( w, client_data, call_data);
            else
                Pan_right( w, client_data, call_data );
            TrackMouse(w, (XtPointer)text2, event, NULL);
        }

        // keycode 98, keysym 65362 is up-arrow
        if ( (key == 65362)
            || ( (key == 65362) && (event->xkey.state & ShiftMask) ) ) {    // Doesn't work yet.
            menu_x=input_x;
            menu_y=input_y;
            if (event->xbutton.state & ShiftMask)   // This doesn't work yet
                Pan_up_less( w, client_data, call_data);
            else
                Pan_up( w, client_data, call_data );
            TrackMouse(w, (XtPointer)text2, event, NULL);
        }

        // keycode 105, keysym 65364 is down-arrow
        if ( (key == 65364)
            || ( (key == 65364) && (event->xkey.state & ShiftMask) ) ) {    // Doesn't work yet.
            menu_x=input_x;
            menu_y=input_y;
            if (event->xbutton.state & ShiftMask)   // This doesn't work yet
                Pan_down_less( w, client_data, call_data);
            else
                Pan_down( w, client_data, call_data );
            TrackMouse(w, (XtPointer)text2, event, NULL);
        }

        // keycode 35, keysym 61 is Equals
        // keycode 35, keysim 43 is Plus
        // keycode 86, keysim 65451 is KP_Add
        if (key == 61 || key == 43 || key == 65451) {
            grid_size++;
            redraw = 1;
        }

        // keycode 48, keysym 45 is Minus
        // keycode 82, keysym 65453 is KP_Subtract
        if (key == 45 || key == 65453) {
            grid_size--;
            redraw = 1;
        }
    }
    else {
        if (event->type == MotionNotify) {
            input_x = event->xmotion.x;
            input_y = event->xmotion.y;
            /*printf("da_input2 x %d y %d\n",input_x,input_y);*/
        }
    }

    if (redraw) {
        create_image(w);
        (void)XCopyArea(XtDisplay(w),pixmap_final,XtWindow(w),gc,0,0,screen_width,screen_height,0,0);
        /*printf("Current x %ld y %ld\n",mid_x_long_offset,mid_y_lat_offset);*/
    }
}





// DK7IN: this function is unused...
//void wait_sec(int dt) {
//    time_t ct;
//
//    ct = sec_now() + dt;
//    while (ct < sec_now()) {
//    }
//}





// This is the periodic process that updates the maps/symbols/tracks.
// At the end of the function it schedules itself to be run again.
void UpdateTime( XtPointer clientData, /*@unused@*/ XtIntervalId id ) {
    Widget w = (Widget) clientData;
    time_t nexttime;
    int do_time;
    int max;
        int i;
    static int last_alert_on_screen;
    static int sec_last_dr_update = 0;

    do_time = 0;
    nexttime = 10;  // Start UpdateTime again 10 milliseconds after we've completed

    (void)sound_done();

    if(display_up) {
        if(display_up_first == 0) {             // very first call, do initialization
            display_up_first = 1;
            statusline("Loading symbols...",1);
            load_pixmap_symbol_file("symbols.dat");
            statusline("Initialize my station...",1);
            my_station_add(my_callsign,my_group,my_symbol,my_long,my_lat,my_phg,my_comment,(char)position_amb_chars);
            da_resize(w, NULL,NULL);            // make sure the size is right after startup & create image
            set_last_position();                // init last map position
            statusline("Start interfaces...",1);
            startup_all_or_defined_port(-1);    // start interfaces
        } else {
            popup_time_out_check();             // clear popup windows after timeout
            check_statusline_timeout();         // clear statusline after timeout
            check_station_remove();             // remove old stations
            check_message_remove();             // remove old messages

            //if ( (new_message_data > 0) && ( (delay_time % 2) == 0) )
            //update_messages(0);                 // Check Messages, no forced update

            /* check on Redraw requests */
            if (         ( (redraw_on_new_data > 1)
                        || (redraw_on_new_data && (sec_now() > last_redraw + REDRAW_WAIT))
                        || (sec_now() > next_redraw)
                        || (pending_ID_message && (sec_now() > remove_ID_message_time)) )
                    && !wait_to_redraw) {

                int temp_alert_count;

                //printf("Redraw on new data\n");

                // check if alert_redraw_on_update is set and it has been at least xx seconds since
                // last weather alert redraw.
                if ( (alert_redraw_on_update
                        && !pending_ID_message
                        && ( sec_now() > ( last_alert_redraw + WX_ALERTS_REFRESH_TIME ) ))
                      || (pending_ID_message && (sec_now() > remove_ID_message_time)) ) {


                    // If we got here because of the ID_message
                    // stuff, clear the variable.
                    if (pending_ID_message && (sec_now() > remove_ID_message_time)) {
                        pending_ID_message = 0;
                    }

                //if (alert_redraw_on_update) {
                    //printf("Alert redraw on update: %ld\t%ld\t%ld\n",sec_now(),last_alert_redraw,WX_ALERTS_REFRESH_TIME);

                    if (!pending_ID_message) {
                        refresh_image(da);  // Much faster than create_image.
                        (void)XCopyArea(XtDisplay(da),pixmap_final,XtWindow(da),gc,0,0,screen_width,screen_height,0,0);
                    }

                    // Here we use temp_alert_count as a temp holding place for the
                    // count of active alerts. Sound alarm if new alerts are displayed.
                    if ((temp_alert_count = alert_on_screen()) > last_alert_on_screen) {
                        if (sound_play_wx_alert_message)
                            play_sound(sound_command, sound_wx_alert_message);
#ifdef HAVE_FESTIVAL
                        if (festival_speak_new_weather_alert) {
                            char station_id[50];
                            xastir_snprintf(station_id,
                                sizeof(station_id), "%s, %d",
                                langcode("SPCHSTR009"),
                                temp_alert_count);
                            SayText(station_id);
                        }
#endif /* HAVE_FESTIVAL */
                    }
                    last_alert_on_screen = alert_redraw_on_update;
                    alert_redraw_on_update = 0;
                } else {
                    if (!pending_ID_message)
                        redraw_symbols(w);
                }

                redraw_on_new_data = 0;
                next_redraw = sec_now()+60;     // redraw every minute, do we need that?
                last_redraw = sec_now();

                // This assures that we periodically check for expired alerts
                // and schedule a screen update if we find any.
                if (alert_display_request())                            // should nor be placed in redraw loop !!???
                    alert_redraw_on_update = redraw_on_new_data = 1;    // ????

            }

            if (show_DR && sec_now() - sec_last_dr_update > update_DR_rate) {
                if (!pending_ID_message) {
                    redraw_symbols(w);
                    sec_last_dr_update = sec_now();
                }
            }


            /* Look for packet data and check port status */
            if (delay_time > 15) {
                display_packet_data();
                interface_status(w);
                delay_time = 0;
                /* check station lists */
                update_station_scroll_list();   // maybe update lists
            }
            delay_time++;

            /* check gps start up, GPS on GPSPORT */
            if(sec_now() > sec_next_gps) {
                sec_next_gps = sec_now()+gps_time;
                /*printf("Check GPS\n");*/
                /* set dtr lines down */
                                /* works for SERIAL_GPS and SERIAL_TNC_HSP_GPS */
                dtr_all_set(1);
                if(gps_stop_now)
                    gps_stop_now = 0;

                /* Tell TNC to send GPS data for SERIAL_TNC_AUX_GPS */
                for(i=0; i<MAX_IFACE_DEVICES; i++) {
                    if (port_data[i].status &&
                            port_data[i].device_type == DEVICE_SERIAL_TNC_AUX_GPS) {
                        /* Device is correct type and is UP (or ERROR) */

                        char tmp[3];

                        /* Send character to device (prefixed with CTRL-C
                         * so that we exit CONV if necessary
                         */
                        if (debug_level & 128) {
                            printf( "Retrieving GPS AUX port %d\n", i);
                        }
                        sprintf(tmp, "%c%c",
                            '\3',
                            devices[i].gps_retrieve);

                        if (debug_level & 1) {
                            printf( "Using %d %d to retrieve GPS\n",
                                '\3',
                                devices[i].gps_retrieve);
                        }
                        port_write_string(i, tmp);
                    }
                }
                sec_next_gps = sec_now()+gps_time;
            }

            /* Check to reestablish a connection */
            if(sec_now() > net_next_time) {
                net_last_time = sec_now();
                net_next_time = net_last_time + 450;    // Check every 7.5 minutes
                //net_next_time = net_last_time + 30;   // This statement is for debug

                /*printf("Checking for reconnects\n");*/
                check_ports();
            }

            /* Check to see if it is time to spit out data */
            if(!wait_to_redraw) {
                if (last_time == 0) {
                    /* first update */
                    next_time = 120;
                    last_time = sec_now();
                    do_time = 1;
                } else {
                    /* check for update */
                    /*printf("Checking --- time %ld time to update %ld\n",sec_now(),last_time+next_time);*/
                    if(sec_now() >= (last_time + next_time)) {
                        next_time += next_time;
                        if (next_time > max_transmit_time)
                            next_time = max_transmit_time;

                        last_time = sec_now();
                        do_time = 1;
                    }
                }
            }

            // Time to spit out a posit?
            if ( my_position_valid
                    && (transmit_now || (sec_now() > posit_next_time) ) ) {

                //printf("Transmitting posit\n");

                // Check for proper symbol in case we're a weather station
                (void)check_weather_symbol();

                posit_last_time = sec_now();

                if (smart_beaconing) {
                    // Schedule next computed posit time based on
                    // speed/turns, etc.
                    posit_next_time = posit_last_time + sb_POSIT_rate;
                    sb_last_heading = sb_current_heading;
                    //printf("Sending Posit\n");
                }
                else {
                    // Schedule next fixed posit time, set in
                    // Configure->Defaults dialog
                    posit_next_time = posit_last_time + POSIT_rate;
                }

                transmit_now = 0;
                // Output to ALL net/tnc ports that are enabled & have tx enabled
//printf("Sending posit\n");
                output_my_aprs_data();

                // Decrement the my_position_valid variable if we're
                // using GPS.  This will make sure that positions
                // are valid, as we'll only get four positions out
                // maximum per valid GPS position.  If the GPS
                // position goes stale, we'll stop sending posits.
                if (using_gps_position && my_position_valid) {
                    my_position_valid--;
//printf("my_position_valid:%d\n",my_position_valid);

                    if (!my_position_valid) { // We just went to zero!
                        // Waiting for GPS data..
                        statusline(langcode("BBARSTA041"),1);
//printf("my_position_valid just went to zero!\n");
                    }
                }
            }
//          if (do_time || transmit_now) {
//              transmit_now = 0;
//              /* output to ALL net/tnc ports */
//              /*printf("Output data\n");*/
//              output_my_aprs_data();
//          }

            // Must compute rain on a periodic basis, as some
            // weather daemons don't put out data often enough
            // to rotate through our queues.
            // We also refresh the Station_info dialog here if
            // it is currently drawn.
            if (sec_now() >= (last_weather_cycle + 30)) {    // Every 30 seconds
                // Note that we also write timestamps out to all of the log files
                // from this routine.  It works out well with the 30 second update
                // rate of cycle_weather().
                (void)cycle_weather();
                last_weather_cycle = sec_now();

                if (station_data_auto_update)
                    update_station_info(w);  // Go refresh the Station Info display
            }

            /* Time to put out raw WX data ? */
            if (sec_now() > sec_next_raw_wx) {
                sec_next_raw_wx = sec_now()+600;

#ifdef TRANSMIT_RAW_WX
                if (transmit_raw_wx)
                    tx_raw_wx_data();
#endif

                /* check wx data last received */
                wx_last_data_check();
            }

            /* is it time to spit out messages? */
            check_and_transmit_messages(sec_now());

            // Is it time to spit out objects/items?
            check_and_transmit_objects_items(sec_now());

            // Do we have any new bulletins to display?
            check_for_new_bulletins();

            // Is it time to create a JPG snapshot?
            if (snapshots_enabled)
                (void)Snapshot();

            /* get data from interface */
            max=0;
            while (max < 20 && !XtAppPending(app_context)) {

if (begin_critical_section(&data_lock, "main.c:UpdateTime(1)" ) > 0)
    printf("data_lock\n");

                if (data_avail) {
                    int data_type;              /* 0=AX25, 1=GPS */

                    //printf("device_type: %d\n",port_data[data_port].device_type);

                    switch (port_data[data_port].device_type) {
                        /* NET Data stream */
                        case DEVICE_NET_STREAM:
                            if (log_net_data)
                                log_data(LOGFILE_NET,(char *)incoming_data);

                            packet_data_add(langcode("WPUPDPD006"),(char *)incoming_data);
                            decode_ax25_line((char *)incoming_data,'I',data_port, 1);
                            break;

                        /* TNC Devices */
                        case DEVICE_SERIAL_KISS_TNC:

                            // Try to decode header and checksum.  If
                            // bad, break, else continue through to
                            // ASCII logging & decode routines.
                            if ( !decode_ax25_header( (char *)incoming_data, incoming_data_length ) ) {
                                // Had a problem decoding it.  Drop
                                // it on the floor.
                                break;
                            }
                            else {
                                // Good decode.  Drop through to the
                                // next block to log and decode the
                                // packet.
                            }

                        case DEVICE_AX25_TNC:

                        case DEVICE_SERIAL_TNC:
                            if (log_tnc_data)
                                log_data(LOGFILE_TNC,(char *)incoming_data);

                            packet_data_add(langcode("WPUPDPD005"),(char *)incoming_data);
                            decode_ax25_line((char *)incoming_data,'T',data_port, 1);
                            break;

                        case DEVICE_SERIAL_TNC_HSP_GPS:
                            if (port_data[data_port].dtr==1) // get GPS data
                                gps_data_find((char *)incoming_data,data_port);
                            else {
                                /* get TNC data */
                                if (log_tnc_data)
                                    log_data(LOGFILE_TNC,(char *)incoming_data);

                                packet_data_add(langcode("WPUPDPD005"),(char *)incoming_data);
                                decode_ax25_line((char *)incoming_data,'T',data_port, 1);
                            }
                            break;

                        case DEVICE_SERIAL_TNC_AUX_GPS:
                            tnc_data_clean((char *)incoming_data);
                            data_type=tnc_get_data_type((char *)incoming_data, data_port);
                            if (data_type)  // GPS Data
                                gps_data_find((char *)incoming_data, data_port);
                            else {          // APRS Data
                                if (log_tnc_data)
                                    log_data(LOGFILE_TNC, (char *)incoming_data);

                                packet_data_add(langcode("WPUPDPD005"),
                                    (char *)incoming_data);
                                decode_ax25_line((char *)incoming_data, 'T', data_port, 1);
                            }
                            break;

                        /* GPS Devices */
                        case DEVICE_SERIAL_GPS:

                        case DEVICE_NET_GPSD:
                            /*printf("GPS Data <%s>\n",incoming_data);*/
                            gps_data_find((char *)incoming_data,data_port);
                            break;

                        /* WX Devices */
                        case DEVICE_SERIAL_WX:

                        case DEVICE_NET_WX:
                            if (log_wx)
                                log_data(LOGFILE_WX,(char *)incoming_data);

                            wx_decode(incoming_data,data_port);
                            break;

                        default:
                            printf("Data from unknown source\n");
                            break;
                    }
                    data_avail=0;
                    max++;
                } else
                    max=20;

if (end_critical_section(&data_lock, "main.c:UpdateTime(2)" ) > 0)
    printf("data_lock\n");

            }
            /* END- get data from interface */
            /* READ FILE IF OPENED */
            if (read_file) {
                if (sec_now() >= next_file_read) {
                    read_file_line(read_file_ptr);
                    next_file_read = sec_now() + REPLAY_DELAY;
                }
            }
            /* END- READ FILE IF OPENED */
        }
    }
    (void)XtAppAddTimeOut(XtWidgetToApplicationContext(w),nexttime,(XtTimerCallbackProc)
    UpdateTime,(XtPointer)w);
}





void quit(int sig) {
    if(debug_level & 15)
        printf("Caught %d\n",sig);

    save_data();

    /* shutdown all interfaces */
    shutdown_all_active_or_defined_port(-1);

    if (debug_level & 1)
        printf("Exiting..\n");

    exit (sig);
}





/* handle segfault signal */
void segfault(/*@unused@*/ int sig) {
    fprintf(stderr, "Caught Segfault! Xastir will terminate\n");
    fprintf(stderr, "%02d:%02d:%02d\n", get_hours(), get_minutes(), get_seconds() );
    quit(-1);
}





/*********************  dialog position *************************/

void pos_dialog(Widget w) {
    static Position x,y;
    Dimension wd, ht;
    int max_x, max_y;

    XtVaGetValues(appshell, XmNx, &x, XmNy, &y, NULL);
    XtVaGetValues(appshell, XmNwidth, &wd, XmNheight, &ht, NULL);

    if (x > 1280)   // We sometimes get strange values for X/Y
        x = 300;

    if (y > 1024)   // We sometimes get strange values for X/Y
        y = 200;

    if (wd > 1280)  // And for width and height
        wd = 640;

    if (ht > 1024)  // And for width and height
        ht = 480;

    max_x = x + wd - (wd / 5);
//    max_y = y + ht - (ht / 5);
    max_y = y + ht/3;

    // Check for proper values for last stored position
    if (   (last_popup_x < x)
        || (last_popup_y < y)
        || (last_popup_x > max_x)
        || (last_popup_y > max_y) ) {
        last_popup_x = x + 20;  // Go to initial position again
        last_popup_y = y + 30;  // Go to initial position again
    } else {
        last_popup_x += 10;     // Increment slightly for next dialog
        last_popup_y += 20;     // Increment slightly for next dialog
    }

    if ((last_popup_y+50) > max_y) {
        last_popup_x = x + 20;  // Go to initial position again
        last_popup_y = y + 30;  // Go to initial position again
    }

    if ((last_popup_x+50) > max_x) {
        last_popup_x = x + 20;  // Go to initial position again
        last_popup_y = y + 30;  // Go to initial position again
    }
    XtVaSetValues(w,XmNx,last_popup_x,XmNy,last_popup_y,NULL);

    //printf("max_x:%d max_y:%d x:%d y:%d wd:%d ht:%d last_x:%d last_y:%d\n",
        //max_x,max_y,x,y,wd,ht,last_popup_x,last_popup_y);
}



/*********************  fix dialog size *************************/

void fix_dialog_size(Widget w) {
    Dimension wd, ht;

    if (XtIsRealized(w)){
        XtVaGetValues(w,
                  XmNwidth, &wd,
                  XmNheight, &ht,
                  0);

        XtVaSetValues(w,
                  XmNminWidth,wd,
                  XmNminHeight,ht,
                  XmNmaxWidth,wd,
                  XmNmaxHeight,ht,
                  NULL);
    }
}



/******************** fix dialog size vertically only *************/

void fix_dialog_vsize(Widget w) {
    Dimension ht;

    if (XtIsRealized(w)){
        XtVaGetValues(w,
                  XmNheight, &ht,
                  0);

        XtVaSetValues(w,
                  XmNminHeight,ht,
                  XmNmaxHeight,ht,
                  NULL);
    }
}



/**************************************** Button CallBacks *************************************/
/***********************************************************************************************/


/*
 *  Button callback for 1 out of 2 selection
 */
void on_off_switch(int switchpos, Widget first, Widget second) {
    if(switchpos) {
        XtSetSensitive(first, FALSE);
        XtSetSensitive(second,TRUE);
    } else {
        XtSetSensitive(first, TRUE);
        XtSetSensitive(second,FALSE);
    }
}





/*
 *  Button callback for 1 out of 3 selection
 */
void sel3_switch(int switchpos, Widget first, Widget second, Widget third) {
    if(switchpos == 2) {
        XtSetSensitive(first, FALSE);
        XtSetSensitive(second,TRUE);
        XtSetSensitive(third, TRUE);
    } else if(switchpos == 1) {
        XtSetSensitive(first, TRUE);
        XtSetSensitive(second,FALSE);
        XtSetSensitive(third, TRUE);
    } else {
        XtSetSensitive(first, TRUE);
        XtSetSensitive(second,TRUE);
        XtSetSensitive(third, FALSE);
    }
}





/*
 *  Keep map in real world space, readjust center and scaling if neccessary
 */
void check_range(void) {
    Dimension width, height;

    XtVaGetValues(da,XmNwidth, &width,XmNheight, &height,0);

    if ((height*new_scale_y) > 64800000l)
        new_mid_y  =  64800000l/2;                               // center between 90°N and 90°S
    else
        if ((new_mid_y - (height*new_scale_y)/2) < 0)
            new_mid_y  = (height*new_scale_y)/2;                 // upper border max 90°N
        else
            if ((new_mid_y + (height*new_scale_y)/2) > 64800000l)
                new_mid_y = 64800000l-((height*new_scale_y)/2);  // lower border max 90°S

    // Adjust scaling based on latitude of new center
    new_scale_x = get_x_scale(new_mid_x,new_mid_y,new_scale_y);  // recalc x scaling depending on position
    //printf("x:%ld\ty:%ld\n\n",new_scale_x,new_scale_y);

    // scale_x will always be bigger than scale_y, so no problem here...
    if ((width*new_scale_x) > 129600000l)
        new_mid_x = 129600000l/2;                                // center between 180°W and 180°E
    else
        if ((new_mid_x - (width*new_scale_x)/2) < 0)
            new_mid_x = (width*new_scale_x)/2;                   // left border max 180°W
        else
            if ((new_mid_x + (width*new_scale_x)/2) > 129600000l)
                new_mid_x = 129600000l-((width*new_scale_x)/2);  // right border max 180°E
}





/*
 *  Display a new map view after checking the view and scaling
 */
void display_zoom_image(int recenter) {
    Dimension width, height;

    XtVaGetValues(da,XmNwidth, &width,XmNheight, &height,0);
    //printf("Before,  x: %lu,  y: %lu\n",new_scale_x,new_scale_y);
    check_range();              // keep map inside world and calc x scaling
    //printf("After,   x: %lu,  y: %lu\n\n",new_scale_x,new_scale_y);
    if (new_mid_x != mid_x_long_offset
            || new_mid_y != mid_y_lat_offset
            || new_scale_x != scale_x
            || new_scale_y != scale_y) {    // If there's been a change in zoom or center
        set_last_position();
        if (recenter) {
            mid_x_long_offset = new_mid_x;      // new map center
            mid_y_lat_offset  = new_mid_y;
        }
        scale_x = new_scale_x;
        scale_y = new_scale_y;
        setup_in_view();    // update "in view" flag for all stations
        create_image(da);
    } else {    // No change in zoom or center.  Don't update ANYTHING.
        //refresh_image(da);    // No reason to do this.
    }
    (void)XCopyArea(XtDisplay(da),pixmap_final,XtWindow(da),gc,0,0,screen_width,screen_height,0,0);
    display_zoom_status();
}





void Zoom_in( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer calldata) {
    Dimension width, height;

    if(display_up) {
        XtVaGetValues(da,XmNwidth, &width,XmNheight, &height,0);
        new_mid_x = mid_x_long_offset - ((width *scale_x)/2) + (menu_x*scale_x);
        new_mid_y = mid_y_lat_offset  - ((height*scale_y)/2) + (menu_y*scale_y);
        new_scale_y = scale_y / 2;
        if (new_scale_y < 1)
            new_scale_y = 1;            // don't go further in
        display_zoom_image(1);          // check range and do display, recenter
    }
}





void Zoom_in_no_pan( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer calldata) {
    Dimension width, height;

    if(display_up) {
        XtVaGetValues(da,XmNwidth, &width,XmNheight, &height,0);
        new_mid_x = mid_x_long_offset;
        new_mid_y = mid_y_lat_offset;
        new_scale_y = scale_y / 2;
        if (new_scale_y < 1)
            new_scale_y = 1;            // don't go further in, scale_x always bigger than scale_y
        display_zoom_image(0);          // check range and do display, keep center
    }
}





void Zoom_out(  /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer calldata) {
    Dimension width, height;

    if(display_up) {
        XtVaGetValues(da,XmNwidth, &width,XmNheight, &height,0);
        new_mid_x = mid_x_long_offset - ((width *scale_x)/2) + (menu_x*scale_x);
        new_mid_y = mid_y_lat_offset  - ((height*scale_y)/2) + (menu_y*scale_y);
        if (width*scale_x < 129600000l || height*scale_y < 64800000l)
            new_scale_y = scale_y * 2;
        else
            new_scale_y = scale_y;      // don't zoom out if whole world could be shown
        display_zoom_image(1);          // check range and do display, recenter
    }
}





void Zoom_out_no_pan( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer calldata) {
    Dimension width, height;

    if(display_up) {
        XtVaGetValues(da,XmNwidth, &width,XmNheight, &height,0);
        new_mid_x = mid_x_long_offset;
        new_mid_y = mid_y_lat_offset;
        if (width*scale_x < 129600000l || height*scale_y < 64800000l)
            new_scale_y = scale_y * 2;
        else
            new_scale_y = scale_y;      // don't zoom out if whole world could be shown
        display_zoom_image(0);          // check range and do display, keep center
    }
}





void Zoom_level( /*@unused@*/ Widget w, XtPointer clientData, /*@unused@*/ XtPointer calldata) {
    Dimension width, height;
    int level;

    level=atoi((char *)clientData);
    if(display_up) {
        XtVaGetValues(da,XmNwidth, &width,XmNheight, &height,0);
        new_mid_x = mid_x_long_offset - ((width *scale_x)/2) + (menu_x*scale_x);
        new_mid_y = mid_y_lat_offset  - ((height*scale_y)/2) + (menu_y*scale_y);
        switch(level) {
            case(1):
                new_scale_y = 1;
                break;

            case(2):
                new_scale_y = 16;
                break;

            case(3):
                new_scale_y = 64;
                break;

            case(4):
                new_scale_y = 256;
                break;

            case(5):
                new_scale_y = 1024;
                break;

            case(6):
                new_scale_y = 8192;
                break;

            case(7):
                new_scale_y = 262144;
                break;

            case(8):
                new_scale_y = (int)(scale_y * 1.1);
                break;

            case(9):
                new_scale_y = (int)(scale_y * 0.9);
                break;

            default:
                break;
        }
        display_zoom_image(1);          // check range and do display, recenter
    }
}





void Pan_ctr( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer calldata) {
    Dimension width, height;

    if(display_up) {
        XtVaGetValues(da,XmNwidth, &width,XmNheight, &height,0);
        new_mid_x = mid_x_long_offset - ((width *scale_x)/2) + (menu_x*scale_x);
        new_mid_y = mid_y_lat_offset  - ((height*scale_y)/2) + (menu_y*scale_y);
        new_scale_y = scale_y;          // keep size
        display_zoom_image(1);          // check range and do display, recenter
    }
}





void Pan_up( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer calldata) {
    Dimension width, height;

    if(display_up) {
        XtVaGetValues(da,XmNwidth, &width,XmNheight, &height,0);
        new_mid_x = mid_x_long_offset;
        new_mid_y = mid_y_lat_offset  - (height*scale_y/4);
        new_scale_y = scale_y;          // keep size
        display_zoom_image(1);          // check range and do display, recenter
    }
}





void Pan_up_less( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer calldata) {
    Dimension width, height;

    if(display_up) {
        XtVaGetValues(da,XmNwidth, &width,XmNheight, &height,0);
        new_mid_x = mid_x_long_offset;
        new_mid_y = mid_y_lat_offset  - (height*scale_y/10);
        new_scale_y = scale_y;          // keep size
        display_zoom_image(1);          // check range and do display, recenter
    }
}





void Pan_down( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer calldata) {
    Dimension width, height;

    if(display_up) {
        XtVaGetValues(da,XmNwidth, &width,XmNheight, &height,0);
        new_mid_x = mid_x_long_offset;
        new_mid_y = mid_y_lat_offset  + (height*scale_y/4);
        new_scale_y = scale_y;          // keep size
        display_zoom_image(1);          // check range and do display, recenter
    }
}





void Pan_down_less( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer calldata) {
    Dimension width, height;

    if(display_up) {
        XtVaGetValues(da,XmNwidth, &width,XmNheight, &height,0);
        new_mid_x = mid_x_long_offset;
        new_mid_y = mid_y_lat_offset  + (height*scale_y/10);
        new_scale_y = scale_y;          // keep size
        display_zoom_image(1);          // check range and do display, recenter
    }
}





void Pan_left( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer calldata) {
    Dimension width, height;

    if(display_up) {
        XtVaGetValues(da,XmNwidth, &width,XmNheight, &height,0);
        new_mid_x = mid_x_long_offset - (width*scale_x/4);
        new_mid_y = mid_y_lat_offset;
        new_scale_y = scale_y;          // keep size
        display_zoom_image(1);          // check range and do display, recenter
    }
}





void Pan_left_less( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer calldata) {
    Dimension width, height;

    if(display_up) {
        XtVaGetValues(da,XmNwidth, &width,XmNheight, &height,0);
        new_mid_x = mid_x_long_offset - (width*scale_x/10);
        new_mid_y = mid_y_lat_offset;
        new_scale_y = scale_y;          // keep size
        display_zoom_image(1);          // check range and do display, recenter
    }
}





void Pan_right( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer calldata) {
    Dimension width, height;

    if(display_up) {
        XtVaGetValues(da,XmNwidth, &width,XmNheight, &height,0);
        new_mid_x = mid_x_long_offset + (width*scale_x/4);
        new_mid_y = mid_y_lat_offset;
        new_scale_y = scale_y;          // keep size
        display_zoom_image(1);          // check range and do display, recenter
    }
}





void Pan_right_less( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer calldata) {
    Dimension width, height;

    if(display_up) {
        XtVaGetValues(da,XmNwidth, &width,XmNheight, &height,0);
        new_mid_x = mid_x_long_offset + (width*scale_x/10);
        new_mid_y = mid_y_lat_offset;
        new_scale_y = scale_y;          // keep size
        display_zoom_image(1);          // check range and do display, recenter
    }
}





void Window_Quit( /*@unused@*/ Widget w, /*@unused@*/ XtPointer client, /*@unused@*/ XtPointer calldata) {
    quit(0);
}





void Menu_Quit( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer calldata) {
    quit(0);
}





void Grid_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        long_lat_grid = atoi(which);
    else
        long_lat_grid = 0;

    if (long_lat_grid)
        statusline(langcode("BBARSTA005"),1);   // Map Lat/Long Grid On
    else
        statusline(langcode("BBARSTA006"),2);   // Map Lat/Long Grid Off

    redraw_symbols(da);
    (void)XCopyArea(XtDisplay(da),pixmap_final,XtWindow(da),gc,0,0,screen_width,screen_height,0,0);
}





void  Map_disable_toggle( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        disable_all_maps = atoi(which);
    else
        disable_all_maps = 0;

    create_image(da);
    (void)XCopyArea(XtDisplay(da),pixmap_final,XtWindow(da),gc,0,0,screen_width,screen_height,0,0);
}





void  Map_auto_toggle( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set) {
        map_auto_maps = atoi(which);
        XtSetSensitive(map_auto_skip_raster_button,TRUE);
    }
    else {
        map_auto_maps = 0;
        XtSetSensitive(map_auto_skip_raster_button,FALSE);
    }

    create_image(da);
    (void)XCopyArea(XtDisplay(da),pixmap_final,XtWindow(da),gc,0,0,screen_width,screen_height,0,0);
    if (map_auto_maps)
        statusline(langcode("BBARSTA007"),1);   // The use of Auto Maps is now on
    else
        statusline(langcode("BBARSTA008"),2);   // The use of Auto Maps is now off
}





void  Map_auto_skip_raster_toggle( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        auto_maps_skip_raster = atoi(which);
    else
        auto_maps_skip_raster = 0;

    create_image(da);
    (void)XCopyArea(XtDisplay(da),pixmap_final,XtWindow(da),gc,0,0,screen_width,screen_height,0,0);
}





void  Map_levels_toggle( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        map_color_levels = atoi(which);
    else
        map_color_levels = 0;

    if (map_color_levels)
        statusline(langcode("BBARSTA009"),1);   // The use of Auto Maps is now on
    else
        statusline(langcode("BBARSTA010"),2);   // The use of Auto Maps is now off
    
    create_image(da);
    (void)XCopyArea(XtDisplay(da),pixmap_final,XtWindow(da),gc,0,0,screen_width,screen_height,0,0);
}





void  Map_labels_toggle( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        map_labels = atoi(which);
    else
        map_labels = 0;

    create_image(da);
    (void)XCopyArea(XtDisplay(da),pixmap_final,XtWindow(da),gc,0,0,screen_width,screen_height,0,0);
}





void  Map_fill_toggle( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        map_color_fill = atoi(which);
    else
        map_color_fill = 0;

    if (map_color_fill)
        statusline(langcode("BBARSTA009"),1);       // The use of Map Color Fill is now On
    else
        statusline(langcode("BBARSTA010"),1);       // The use of Map Color Fill is now Off

    create_image(da);
    (void)XCopyArea(XtDisplay(da),pixmap_final,XtWindow(da),gc,0,0,screen_width,screen_height,0,0);
}





void Map_background( /*@unused@*/ Widget w, XtPointer clientData, /*@unused@*/ XtPointer calldata) {
    int bgcolor;
    int i;

    bgcolor=atoi((char *)clientData);

    if(display_up){
        for (i=0;i<12;i++){
            if (i == bgcolor)
                XtSetSensitive(map_bgcolor[i],FALSE);
            else
                XtSetSensitive(map_bgcolor[i],TRUE);
        }
        map_background_color=bgcolor;
        create_image(da);
        (void)XCopyArea(XtDisplay(da),pixmap_final,XtWindow(da),gc,0,0,screen_width,screen_height,0,0);
    }
}





#if !defined(NO_GRAPHICS) && defined(HAVE_GEOTIFF)
void Geotiff_intensity(Widget w, XtPointer clientData, XtPointer calldata) {
    float my_intensity;
    int i;

    my_intensity=atof((char *)clientData);

    if(display_up){
        for (i=0;i<11;i++){
            if (i == (int)((float)(my_intensity * 10.0)) )
                XtSetSensitive(geotiff_intensity[i],FALSE);
            else
                XtSetSensitive(geotiff_intensity[i],TRUE);
        }

        geotiff_map_intensity=my_intensity;
        create_image(da);
        XCopyArea(XtDisplay(da),pixmap_final,XtWindow(da),gc,0,0,screen_width,screen_height,0,0);
    }
}
#endif





void Map_station_label( /*@unused@*/ Widget w, XtPointer clientData, /*@unused@*/ XtPointer calldata) {
    int style;

    style=atoi((char *)clientData);

    if(display_up){
        letter_style = style;
        sel3_switch(letter_style,map_station_label2,map_station_label1,map_station_label0);
        redraw_symbols(da);
        (void)XCopyArea(XtDisplay(da),pixmap_final,XtWindow(da),gc,0,0,screen_width,screen_height,0,0);
    }
}





void  Map_wx_alerts_toggle( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        wx_alert_style = !(atoi(which));
    else
        wx_alert_style = 1;

    if (display_up) {
        refresh_image(da);
        (void)XCopyArea(XtDisplay(da),pixmap_final,XtWindow(da),gc,0,0,screen_width,screen_height,0,0);
    }
}





void TNC_Transmit_now( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer calldata) {
    transmit_now = 1;              /* toggle transmission of station now*/
}





void  TNC_Logging_toggle( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        log_tnc_data = atoi(which);
    else
        log_tnc_data = 0;
}





void Net_Logging_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        log_net_data = atoi(which);
    else
        log_net_data = 0;
}





void IGate_Logging_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        log_igate = atoi(which);
    else
        log_igate = 0;
}





void WX_Logging_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        log_wx = atoi(which);
    else
        log_wx = 0;
}





// Display Menu button callbacks

/*
 *  Toggle Callsign Display (button callbacks)
 */
void Call_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        symbol_callsign_display = atoi(which);
    else
        symbol_callsign_display = 0;

    redraw_on_new_data = 2;     // Immediate screen update
}





/*
 *  Toggle Speed Display (button callbacks)
 */
void Speed_enable_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set) {
        speed_display_enable = atoi(which);
        XtSetSensitive(speed_short_button, TRUE);
    }
    else {
        speed_display_enable = 0;
        XtSetSensitive(speed_short_button, FALSE);
    }

    symbol_speed_display = speed_display_enable;
    if (symbol_speed_display && !speed_display_short)
        symbol_speed_display = 2;

    redraw_on_new_data = 2;     // Immediate screen update
}





void Speed_short_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        speed_display_short = atoi(which);
    else
        speed_display_short = 0;

    symbol_speed_display = speed_display_enable;
    if (symbol_speed_display && !speed_display_short)
        symbol_speed_display = 2;

    redraw_on_new_data = 2;     // Immediate screen update
}





/*
 *  Toggle Altitude Display (button callbacks)
 */
void Alt_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        symbol_alt_display = atoi(which);
    else
        symbol_alt_display = 0;

    redraw_on_new_data = 2;     // Immediate screen update
}





/*
 *  Toggle Symbol Display (button callbacks)
 */
void Symbols_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set) {
        symbol_display_enable = atoi(which);
        XtSetSensitive(symbols_local_stations_button, TRUE);
        XtSetSensitive(symbols_nonlocal_stations_button, TRUE);
        if (symbol_display_local_stations || symbol_display_nonlocal_stations) {
            XtSetSensitive(symbols_stationary_button, TRUE);
            XtSetSensitive(symbols_moving_button, TRUE);
            XtSetSensitive(symbols_WX_button, TRUE);
        }
        XtSetSensitive(symbols_objects_button, TRUE);
        if (symbol_display_objects)
            XtSetSensitive(wx_obj_enable_button,TRUE);
        XtSetSensitive(symbols_rotate_button, TRUE);
        XtSetSensitive(call_button,TRUE);
        XtSetSensitive(speed_enable_button,TRUE);
        if (speed_display_enable)
            XtSetSensitive(speed_short_button,TRUE);
        XtSetSensitive(alt_button,TRUE);
        XtSetSensitive(course_button,TRUE);
        XtSetSensitive(dist_course_button,TRUE);
        XtSetSensitive(wx_enable_button,TRUE);
        if (wx_display_enable)
            XtSetSensitive(wx_short_button,TRUE);
        XtSetSensitive(station_phg_button,TRUE);
        if (show_phg) {
            XtSetSensitive(station_phg_mobiles_button,TRUE);
            XtSetSensitive(station_phg_default_button,TRUE);
        }
        XtSetSensitive(station_amb_button,TRUE);
        XtSetSensitive(station_old_data_button,TRUE);
        XtSetSensitive(station_DF_button,TRUE);
        XtSetSensitive(station_last_heard_button,TRUE);
        XtSetSensitive(dead_reckoning_button,TRUE);
        XtSetSensitive(dead_reckoning_arc_button,TRUE);
        XtSetSensitive(dead_reckoning_course_button,TRUE);
        XtSetSensitive(dead_reckoning_symbol_button,TRUE);
    }
    else {
        symbol_display_enable = 0;
        XtSetSensitive(symbols_local_stations_button, FALSE);
        XtSetSensitive(symbols_nonlocal_stations_button, FALSE);
        XtSetSensitive(symbols_stationary_button, FALSE);
        XtSetSensitive(symbols_moving_button, FALSE);
        XtSetSensitive(symbols_WX_button, FALSE);
        XtSetSensitive(symbols_objects_button, FALSE);
        XtSetSensitive(wx_obj_enable_button,FALSE);
        XtSetSensitive(symbols_rotate_button, FALSE);
        XtSetSensitive(call_button,FALSE);
        XtSetSensitive(speed_enable_button,FALSE);
        XtSetSensitive(speed_short_button,FALSE);
        XtSetSensitive(alt_button,FALSE);
        XtSetSensitive(course_button,FALSE);
        XtSetSensitive(dist_course_button,FALSE);
        XtSetSensitive(wx_enable_button,FALSE);
        XtSetSensitive(wx_short_button,FALSE);
        XtSetSensitive(station_phg_button,FALSE);
        XtSetSensitive(station_phg_mobiles_button,FALSE);
        XtSetSensitive(station_phg_default_button,FALSE);
        XtSetSensitive(station_amb_button,FALSE);
        XtSetSensitive(station_old_data_button,FALSE);
        XtSetSensitive(station_DF_button,FALSE);
        XtSetSensitive(station_last_heard_button,FALSE);
        XtSetSensitive(dead_reckoning_button,FALSE);
        XtSetSensitive(dead_reckoning_arc_button,FALSE);
        XtSetSensitive(dead_reckoning_course_button,FALSE);
        XtSetSensitive(dead_reckoning_symbol_button,FALSE);
   }

    symbol_display = symbol_display_enable;
    if (symbol_display_enable && symbol_display_rotate)
        symbol_display = 2;

    redraw_on_new_data = 2;     // Immediate screen update
}





void Symbols_local_stations_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set) {
        symbol_display_local_stations = atoi(which);
    }
    else {
        symbol_display_local_stations = 0;
    }
    if (symbol_display_local_stations || symbol_display_nonlocal_stations) {
        XtSetSensitive(symbols_stationary_button, TRUE);
        XtSetSensitive(symbols_moving_button, TRUE);
        XtSetSensitive(symbols_WX_button, TRUE);
    }
    else {
        XtSetSensitive(symbols_stationary_button, FALSE);
        XtSetSensitive(symbols_moving_button, FALSE);
        XtSetSensitive(symbols_WX_button, FALSE);
    }

    redraw_on_new_data = 2;     // Immediate screen update
}





void Symbols_nonlocal_stations_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set) {
        symbol_display_nonlocal_stations = atoi(which);
    }
    else {
        symbol_display_nonlocal_stations = 0;
    }
    if (symbol_display_local_stations || symbol_display_nonlocal_stations) {
        XtSetSensitive(symbols_stationary_button, TRUE);
        XtSetSensitive(symbols_moving_button, TRUE);
        XtSetSensitive(symbols_WX_button, TRUE);
    }
    else {
        XtSetSensitive(symbols_stationary_button, FALSE);
        XtSetSensitive(symbols_moving_button, FALSE);
        XtSetSensitive(symbols_WX_button, FALSE);
    }

    redraw_on_new_data = 2;     // Immediate screen update
}





void Symbols_stationary_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        symbol_display_stationary = atoi(which);
    else
        symbol_display_stationary = 0;

    redraw_on_new_data = 2;     // Immediate screen update
}





void Symbols_moving_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        symbol_display_moving = atoi(which);
    else
        symbol_display_moving = 0;

    redraw_on_new_data = 2;     // Immediate screen update
}





void Symbols_WX_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        symbol_display_WX = atoi(which);
    else
        symbol_display_WX = 0;

    redraw_on_new_data = 2;     // Immediate screen update
}





void Symbols_objects_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set) {
        symbol_display_objects = atoi(which);
        XtSetSensitive(wx_obj_enable_button, TRUE);
    }
    else {
        symbol_display_objects = 0;
        XtSetSensitive(wx_obj_enable_button, FALSE);
    }

    redraw_on_new_data = 2;     // Immediate screen update
}





void Symbols_rotate_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        symbol_display_rotate = atoi(which);
    else
        symbol_display_rotate = 0;

    symbol_display = symbol_display_enable;
    if (symbol_display_enable && symbol_display_rotate)
        symbol_display = 2;

    redraw_on_new_data = 2;     // Immediate screen update
}





/*
 *  Toggle Dead-Reckoning Display (button callbacks)
 */
void Dead_Reckoning_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set) {
        show_DR = atoi(which);
        XtSetSensitive(dead_reckoning_arc_button,TRUE);
        XtSetSensitive(dead_reckoning_course_button,TRUE);
        XtSetSensitive(dead_reckoning_symbol_button,TRUE);
    }
    else {
        show_DR = 0;
        XtSetSensitive(dead_reckoning_arc_button,FALSE);
        XtSetSensitive(dead_reckoning_course_button,FALSE);
        XtSetSensitive(dead_reckoning_symbol_button,FALSE);
    }

    redraw_on_new_data = 2;     // Immediate screen update
}





/*
 *  Toggle Dead-Reckoning Arc Display (button callbacks)
 */
void Dead_Reckoning_Arc_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        show_DR_arc = atoi(which);
    else
        show_DR_arc = 0;

    redraw_on_new_data = 2;     // Immediate screen update
}





/*
 *  Toggle Dead-Reckoning Course Display (button callbacks)
 */
void Dead_Reckoning_Course_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        show_DR_course = atoi(which);
    else
        show_DR_course = 0;

    redraw_on_new_data = 2;     // Immediate screen update
}





/*
 *  Toggle Dead-Reckoning Symbol Display (button callbacks)
 */
void Dead_Reckoning_Symbol_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        show_DR_symbol = atoi(which);
    else
        show_DR_symbol = 0;

    redraw_on_new_data = 2;     // Immediate screen update
}





/*
 *  Toggle Trail Display (button callbacks)
 */
void Station_trails_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        station_trails = atoi(which);
    else
        station_trails = 0;

    redraw_on_new_data = 2;     // Immediate screen update
}





void Course_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        symbol_course_display = atoi(which);
    else
        symbol_course_display = 0;

    redraw_on_new_data = 2;     // Immediate screen update
}





void Dist_course_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        symbol_dist_course_display = atoi(which);
    else
        symbol_dist_course_display = 0;

    redraw_on_new_data = 2;     // Immediate screen update
}





/*
 *  Toggle Weather Info Display (button callbacks)
 */
void WX_enable_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set) {
        wx_display_enable = atoi(which);
        XtSetSensitive(wx_short_button, TRUE);
    }
    else {
        wx_display_enable = 0;
        XtSetSensitive(wx_short_button, FALSE);
    }

    symbol_weather_display = wx_display_enable;
    if (wx_display_enable && !wx_display_short)
        symbol_weather_display = 2;

    redraw_on_new_data = 2;     // Immediate screen update
}





void WX_short_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        wx_display_short = atoi(which);
    else
        wx_display_short = 0;

    symbol_weather_display = wx_display_enable;
    if (wx_display_enable && !wx_display_short)
        symbol_weather_display = 2;

    redraw_on_new_data = 2;     // Immediate screen update
}





void WX_obj_enable_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        wx_obj_display_enable = atoi(which);
    else
        wx_obj_display_enable = 0;

    redraw_on_new_data = 2;     // Immediate screen update
}





void Station_phg_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set) {
        show_phg = atoi(which);
        XtSetSensitive(station_phg_mobiles_button, TRUE);
        XtSetSensitive(station_phg_default_button, TRUE);
    }
    else {
        show_phg = 0;
        XtSetSensitive(station_phg_mobiles_button, FALSE);
        XtSetSensitive(station_phg_default_button, FALSE);
    }
 
    redraw_on_new_data = 2;     // Immediate screen update
}





void Station_phg_default_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        show_phg_default = atoi(which);
    else
        show_phg_default = 0;

    redraw_on_new_data = 2;     // Immediate screen update
}





void Station_phg_mobiles_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        show_phg_mobiles = atoi(which);
    else
        show_phg_mobiles = 0;

    redraw_on_new_data = 2;     // Immediate screen update
}





void Station_amb_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        show_amb = atoi(which);
    else
        show_amb = 0;

    redraw_on_new_data = 2;     // Immediate screen update
}





void Station_old_data_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        show_old_data = atoi(which);
    else
        show_old_data = 0;

    redraw_on_new_data = 2;     // Immediate screen update
}





void Station_DF_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        show_DF = atoi(which);
    else
        show_DF = 0;

    redraw_on_new_data = 2;     // Immediate screen update
}





void Station_Last_Heard_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        show_last_heard = atoi(which);
    else
        show_last_heard = 0;

    redraw_on_new_data = 2;     // Immediate screen update
}





/*
 *  Toggle unit system (button callbacks)
 */
void  Units_choice_toggle( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        units_english_metric = atoi(which);
    else
        units_english_metric = 0;

    redraw_on_new_data=2;
    update_units();                     // setup conversion
    fill_wx_data();
}






/*
 *  Update global variables for unit conversion
 *
 * Be careful using these, as they change based on the value of
 * units_english_metric!  These variable should only be used when
 * DISPLAYING data, not when converting numbers for use in internal
 * storage or equations.
 *
 */
void update_units(void) {

    switch (units_english_metric) {
        case 1:
            strcpy(un_alt,"ft");
            strcpy(un_dst,"mi");
            strcpy(un_spd,"mph");
            cvt_m2len  = 3.28084;   // m to ft
            cvt_dm2len = 0.328084;  // dm to ft
            cvt_hm2len = 0.0621504; // hm to mi
            cvt_kn2len = 1.1508;    // knots to mph
            cvt_mi2len = 1.0;       // mph to mph
            break;
        case 2:
            // add nautical miles and knots for future use
            strcpy(un_alt,"ft");
            strcpy(un_dst,"nm");
            strcpy(un_spd,"kn");
            cvt_m2len  = 3.28084;   // m to ft
            cvt_dm2len = 0.328084;  // dm to ft
            cvt_hm2len = 0.0539957; // hm to nm
            cvt_kn2len = 1.0;       // knots to knots
            cvt_mi2len = 0.8689607; // mph to knots / mi to nm
            break;
        default:
            strcpy(un_alt,"m");
            strcpy(un_dst,"km");
            strcpy(un_spd,"km/h");
            cvt_m2len  = 1.0;       // m to m
            cvt_dm2len = 0.1;       // dm to m
            cvt_hm2len = 0.1;       // hm to km
            cvt_kn2len = 1.852;     // knots to km/h
            cvt_mi2len = 1.609;     // mph to km/h
            break;
    }
}





void  Auto_msg_toggle( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        auto_reply = atoi(which);
    else
        auto_reply = 0;
}





void  Satellite_msg_ack_toggle( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        satellite_ack_mode = atoi(which);
    else
        satellite_ack_mode = 0;
}





void  Transmit_disable_toggle( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set) {
        transmit_disable = atoi(which);
        XtSetSensitive(iface_transmit_now,FALSE);
        XtSetSensitive(object_tx_disable_toggle,FALSE);
        XtSetSensitive(posit_tx_disable_toggle,FALSE);
    }
    else {
        transmit_disable = 0;
        XtSetSensitive(iface_transmit_now,TRUE);
        XtSetSensitive(object_tx_disable_toggle,TRUE);
        XtSetSensitive(posit_tx_disable_toggle,TRUE);
    }
}





void  Posit_tx_disable_toggle( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        posit_tx_disable = atoi(which);
    else
        posit_tx_disable = 0;
}





void  Object_tx_disable_toggle( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        object_tx_disable = atoi(which);
    else
        object_tx_disable = 0;
}





void Help_About( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer callData) {
    Widget d;
    XmString xms, xa, xb;
    Arg al[10];
    unsigned int ac;
    float version;
    char string1[100];
    char string2[100];
#define ABOUT_MSG "X Amateur Station Tracking and Information Reporting\n\nhttp://www.xastir.org\n\n1999-2002, The Xastir Group"


    xb = XmStringCreateLtoR("\nXastir V" VERSION "\n\n" ABOUT_MSG, XmFONTLIST_DEFAULT_TAG);

    xa = XmStringCreateLtoR(_("\n\n\nLibraries used: \n"), XmFONTLIST_DEFAULT_TAG);
    xms = XmStringConcat(xb, xa);
    XmStringFree(xa);
    XmStringFree(xb);
    //xms is still defined

    xa = XmStringCreateLtoR("\n", XmFONTLIST_DEFAULT_TAG);  // Add a newline
    xb = XmStringConcat(xms, xa);
    XmStringFree(xa);
    XmStringFree(xms);
    //xb is still defined

    xa = XmStringCreateLtoR(XmVERSION_STRING, XmFONTLIST_DEFAULT_TAG);  // Add the Motif version string
    xms = XmStringConcat(xb, xa);
    XmStringFree(xa);
    XmStringFree(xb);
    //xms is still defined

    xa = XmStringCreateLtoR("\n", XmFONTLIST_DEFAULT_TAG);  // Add a newline
    xb = XmStringConcat(xms, xa);
    XmStringFree(xa);
    XmStringFree(xms);
    //xb is still defined

    xms = XmStringCopy(xb);
    XmStringFree(xb);
    //xms is still defined

#ifdef HAVE_NETAX25_AXLIB_H
    //if (libax25_version != NULL) {
    xb = XmStringCreateLtoR("\n", XmFONTLIST_DEFAULT_TAG);
    xa = XmStringConcat(xb, xms);
    XmStringFree(xb);
    XmStringFree(xms);
    xb = XmStringCreateLtoR("@(#)LibAX25 (ax25lib_version is broken. Complain to the authors.)\n", XmFONTLIST_DEFAULT_TAG);
    xms = XmStringConcat(xa, xb);
    XmStringFree(xa);
    XmStringFree(xb);
    //}
#endif

#ifdef HAVE_IMAGEMAGICK
    xb = XmStringCreateLtoR("\n", XmFONTLIST_DEFAULT_TAG);  // Add a newline
    xa = XmStringConcat(xb, xms);
    XmStringFree(xb);
    XmStringFree(xms);
    //xa is still defined

    xb = XmStringCreateLtoR( MagickVersion, XmFONTLIST_DEFAULT_TAG);    // Add ImageMagick version string
    xms = XmStringConcat(xa, xb);
    XmStringFree(xa);
    XmStringFree(xb);
    //xms is still defined
#endif
    xb = XmStringCreateLtoR("\n", XmFONTLIST_DEFAULT_TAG);  // Add a newline
    xa = XmStringConcat(xb, xms);
    XmStringFree(xb);
    XmStringFree(xms);
    //xa is still defined

    version = XRotVersion( string1, 99 );
    xastir_snprintf(string2, sizeof(string2), "\n%s, Version %0.2f", string1, version);
    xb = XmStringCreateLtoR( string2, XmFONTLIST_DEFAULT_TAG);    // Add Xvertext version string
    xms = XmStringConcat(xa, xb);
    XmStringFree(xa);
    XmStringFree(xb);
    //xms is still defined

    ac = 0;
    XtSetArg(al[ac], XmNmessageString, xms); ac++;
    XtSetArg(al[ac], XmNtitle, "About Xastir"); ac++;
    XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
    XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
 
    d = XmCreateInformationDialog(Global.top, "About Xastir", al, ac);
    XmStringFree(xms);
    XtDestroyWidget(XmMessageBoxGetChild(d, (unsigned char)XmDIALOG_CANCEL_BUTTON));
    XtDestroyWidget(XmMessageBoxGetChild(d, (unsigned char)XmDIALOG_HELP_BUTTON));
    XtManageChild(d);
    pos_dialog(d);
    fix_dialog_size(d);
}





Widget GetTopShell(Widget w) {
    while(w && !XtIsWMShell(w))
        w = XtParent(w);

    return(w);
}





/*********************** Display incoming data*******************************/
/****************************************************************************/

void Display_data_destroy_shell( /*@unused@*/ Widget widget, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    Widget shell = (Widget) clientData;
    XtPopdown(shell);
    XtDestroyWidget(shell);
    Display_data_dialog = (Widget)NULL;
}





void  Display_packet_toggle( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        Display_packet_data_type = atoi(which);
    else
        Display_packet_data_type = 0;

    redraw_on_new_packet_data=1;
}





void Display_data( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer callData) {
    Widget pane, rowcol, button_close, option_box, tnc_data, net_data, tnc_net_data;
    unsigned int n;
    Arg args[20];
    Atom delw;

    if (!Display_data_dialog) {
        Display_data_dialog = XtVaCreatePopupShell(langcode("WPUPDPD001"),
                xmDialogShellWidgetClass,
                Global.top,
                XmNdeleteResponse,XmDESTROY,
                XmNdefaultPosition, FALSE,
                NULL);

        pane = XtVaCreateWidget("Display_data pane",
                xmPanedWindowWidgetClass, 
                Display_data_dialog,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        rowcol =  XtVaCreateWidget("Display_data rowcol",
                xmRowColumnWidgetClass, 
                pane,
                XmNtraversalOn, TRUE,
                XmNorientation, XmVERTICAL,
                XmNpacking, XmPACK_TIGHT,
                XmNisAligned, TRUE,
                XmNentryAlignment, XmALIGNMENT_CENTER,
                XmNkeyboardFocusPolicy, XmEXPLICIT,
                XmNautoUnmanage, FALSE,
                XmNshadowThickness, 1,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        n=0;
        XtSetArg(args[n], XmNrows, 15); n++;
        XtSetArg(args[n], XmNcolumns, 100); n++;
        XtSetArg(args[n], XmNeditable, FALSE); n++;
        XtSetArg(args[n], XmNeditMode, XmMULTI_LINE_EDIT); n++;
        XtSetArg(args[n], XmNwordWrap, TRUE); n++;
        XtSetArg(args[n], XmNforeground, MY_FG_COLOR); n++;
        XtSetArg(args[n], XmNbackground, MY_BG_COLOR); n++;
        XtSetArg(args[n], XmNscrollHorizontal, TRUE); n++;
        XtSetArg(args[n], XmNscrollVertical, FALSE); n++;
        XtSetArg(args[n], XmNcursorPositionVisible, FALSE); n++;
        XtSetArg(args[n], XmNtraversalOn, FALSE); n++;
//        XtSetArg(args[n], XmNnavigationType, XmTAB_GROUP); n++;
        Display_data_text=NULL;
        Display_data_text = XmCreateScrolledText(rowcol,
                "Display_data text",
                args,
                n);
   
// I haven't figured out how to get the scrollbars to allow keyboard traversal.
// When the ScrolledText widget is in the tab group, once you get there you can't
// get out and it beeps at you when you try.  Frustrating.   For this dialog it's
// probably not important enough to worry about.
// I now have it set to allow TAB'ing into the ScrolledText widget, but to get
// out you must do a <Shift><TAB>.  This sucks.  Even if you enable the
// ScrolledText widget in the tab group, the scrollbars don't work with the
// arrow keys.
// ScrolledList works.  I need to convert to ScrolledList if
// possible for output-only windows.

        /* set colors */
        n=0;
        XtSetArg(args[n],XmNforeground, MY_FG_COLOR); n++;
        XtSetArg(args[n],XmNbackground, MY_BG_COLOR); n++;
        option_box = XmCreateRadioBox(rowcol,
                "Display_data option box",
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
                NULL);

        XtAddCallback(tnc_data,XmNvalueChangedCallback,Display_packet_toggle,"1");

        net_data = XtVaCreateManagedWidget(langcode("WPUPDPD003"),
                xmToggleButtonGadgetClass,
                option_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        XtAddCallback(net_data,XmNvalueChangedCallback,Display_packet_toggle,"2");

        tnc_net_data = XtVaCreateManagedWidget(langcode("WPUPDPD004"),
                xmToggleButtonGadgetClass,
                option_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        XtAddCallback(tnc_net_data,XmNvalueChangedCallback,Display_packet_toggle,"0");


        button_close = XtVaCreateManagedWidget(langcode("UNIOP00003"),
                xmPushButtonGadgetClass, 
                rowcol,
                XmNtopAttachment, XmATTACH_FORM,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_FORM,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 2,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 3,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);


        XtAddCallback(button_close, XmNactivateCallback, Display_data_destroy_shell, Display_data_dialog);

        pos_dialog(Display_data_dialog);

        delw = XmInternAtom(XtDisplay(Display_data_dialog),"WM_DELETE_WINDOW", FALSE);
        XmAddWMProtocolCallback(Display_data_dialog,
                            delw,
                            Display_data_destroy_shell,
                            (XtPointer)Display_data_dialog);

        switch (Display_packet_data_type) {
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

        XtManageChild(option_box);
        XtManageChild(Display_data_text);
        XtVaSetValues(Display_data_text, XmNbackground, colors[0x0f], NULL);
        XtManageChild(rowcol);
        XtManageChild(pane);

        redraw_on_new_packet_data=1;
        XtPopup(Display_data_dialog,XtGrabNone);

        fix_dialog_vsize(Display_data_dialog);

        // Move focus to the Close button.  This appears to highlight the
        // button fine, but we're not able to hit the <Enter> key to
        // have that default function happen.  Note:  We _can_ hit the
        // <SPACE> key, and that activates the option.
//        XmUpdateDisplay(Display_data_dialog);
        XmProcessTraversal(button_close, XmTRAVERSE_CURRENT);

    }  else
        (void)XRaiseWindow(XtDisplay(Display_data_dialog), XtWindow(Display_data_dialog));

}





/****************************** Help menu ***********************************/
/****************************************************************************/

void help_view_destroy_shell( /*@unused@*/ Widget widget, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    Widget shell = (Widget) clientData;
    XtPopdown(shell);
    XtDestroyWidget(shell);
    help_view_dialog = (Widget)NULL;
}



void help_index_destroy_shell( /*@unused@*/ Widget widget, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    Widget shell = (Widget) clientData;

    if (help_view_dialog)
        help_view_destroy_shell(help_view_dialog, help_view_dialog, NULL);

    help_view_dialog = (Widget)NULL;

    XtPopdown(shell);
    XtDestroyWidget(shell);
    help_index_dialog = (Widget)NULL;
}



void help_view( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer callData) {
    Widget pane, my_form, button_close,help_text;
    int i,x,y;
    unsigned int n;
    char *temp;
    char title[200];
    char temp2[200];
    char temp3[200];
    FILE *f;
    XmString *list;
    int open;
    Arg args[20];
    int data_on,pos;
    int found;
    Atom delw;

    data_on=0;
    pos=0;
    found=0;

    XtVaGetValues(help_list,
               XmNitemCount,&i,
               XmNitems,&list,
               NULL);

    for (x=1; x<=i;x++) {
        if (XmListPosSelected(help_list,x)) {
            found=1;
            if (XmStringGetLtoR(list[(x-1)],XmFONTLIST_DEFAULT_TAG,&temp))
                x=i+1;
        }
    }
    open=0;

    if (found) {
        if (help_view_dialog) {
            XtVaGetValues(help_view_dialog, XmNx, &x, XmNy, &y, NULL);
            help_view_destroy_shell(help_view_dialog, help_view_dialog, NULL);
            help_view_dialog = (Widget)NULL;
            open=1;
        }
        if (!help_view_dialog) {
            xastir_snprintf(title, sizeof(title), "%s - %s", langcode("MENUTB0009"), temp);
            help_view_dialog = XtVaCreatePopupShell(title,
                    xmDialogShellWidgetClass,
                    Global.top,
                    XmNdeleteResponse,XmDESTROY,
                    XmNdefaultPosition, FALSE,
                    NULL);
            pane = XtVaCreateWidget("help_view pane",
                    xmPanedWindowWidgetClass, 
                    help_view_dialog,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                    NULL);

            my_form =  XtVaCreateWidget("help_view my_form",
                    xmFormWidgetClass, 
                    pane,
                    XmNfractionBase, 5,
                    XmNautoUnmanage, FALSE,
                    XmNshadowThickness, 1,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                    NULL);

            n=0;
            XtSetArg(args[n], XmNrows, 20); n++;
            XtSetArg(args[n], XmNcolumns, 80); n++;
            XtSetArg(args[n], XmNeditable, FALSE); n++;
            XtSetArg(args[n], XmNeditMode, XmMULTI_LINE_EDIT); n++;
            XtSetArg(args[n], XmNwordWrap, TRUE); n++;
            XtSetArg(args[n], XmNscrollHorizontal, FALSE); n++;
            XtSetArg(args[n], XmNcursorPositionVisible, FALSE); n++;
            XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
            XtSetArg(args[n], XmNtopOffset, 5); n++;
            XtSetArg(args[n], XmNbottomAttachment, XmATTACH_NONE); n++;
            XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
            XtSetArg(args[n], XmNleftOffset, 5); n++;
            XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
            XtSetArg(args[n], XmNrightOffset, 5); n++;
            XtSetArg(args[n], XmNforeground, MY_FG_COLOR); n++;
            XtSetArg(args[n], XmNbackground, MY_BG_COLOR); n++;

            help_text=NULL;
            help_text = XmCreateScrolledText(my_form,
                    "help_view help text",
                    args,
                    n);

            f=fopen(HELP_FILE,"r");
            if (f!=NULL) {
                while(!feof(f)) {
                    (void)get_line(f,temp2,200);
                    if (strncmp(temp2,"HELP-INDEX>",11)==0) {
                        if(strcmp((temp2+11),temp)==0)
                            data_on=1;
                        else
                            data_on=0;
                    } else {
                        if (data_on) {
                            xastir_snprintf(temp3, sizeof(temp3), "%s\n", temp2);
                            XmTextInsert(help_text,pos,temp3);
                            pos += strlen(temp3);
                            XmTextShowPosition(help_text,0);
                        }
                    }
                }
                (void)fclose(f);
            }
            else
                printf("Couldn't open file: %s\n", HELP_FILE);

            button_close = XtVaCreateManagedWidget(langcode("UNIOP00003"),
                    xmPushButtonGadgetClass, 
                    my_form,
                    XmNtopAttachment, XmATTACH_WIDGET,
                    XmNtopWidget, XtParent(help_text),
                    XmNtopOffset, 5,
                    XmNbottomAttachment, XmATTACH_FORM,
                    XmNbottomOffset, 5,
                    XmNleftAttachment, XmATTACH_POSITION,
                    XmNleftPosition, 2,
                    XmNrightAttachment, XmATTACH_POSITION,
                    XmNrightPosition, 3,
                    NULL);

            XtAddCallback(button_close, XmNactivateCallback, help_view_destroy_shell, help_view_dialog);

            if (!open)
                pos_dialog(help_view_dialog);
            else
                XtVaSetValues(help_view_dialog, XmNx, x, XmNy, y, NULL);

            delw = XmInternAtom(XtDisplay(help_view_dialog),"WM_DELETE_WINDOW", FALSE);
            XmAddWMProtocolCallback(help_view_dialog, delw, help_view_destroy_shell, (XtPointer)help_view_dialog);

            XtManageChild(my_form);
            XtManageChild(help_text);
            XtVaSetValues(help_text, XmNbackground, colors[0x0f], NULL);
            XtManageChild(pane);

            XtPopup(help_view_dialog,XtGrabNone);
            fix_dialog_size(help_view_dialog);
            XmTextShowPosition(help_text,0);
        }
        XtFree(temp);   // Free up the space allocated
    }

}



void Help_Index( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer callData) {
    static Widget  pane, my_form, button_ok, button_cancel;
    int n;
    char temp[600];
    FILE *f;
    Arg al[20];           /* Arg List */
    unsigned int ac = 0;           /* Arg Count */
    Atom delw;
    XmString str_ptr;

    if(!help_index_dialog) {
        help_index_dialog = XtVaCreatePopupShell(langcode("WPUPHPI001"),
                xmDialogShellWidgetClass,
                appshell,
                XmNdeleteResponse,XmDESTROY,
                XmNdefaultPosition, FALSE,
                XmNresize, FALSE,
                NULL);

        pane = XtVaCreateWidget("Help_Index pane",
                xmPanedWindowWidgetClass, 
                help_index_dialog,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        my_form =  XtVaCreateWidget("Help_Index my_form",
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


        help_list = XmCreateScrolledList(my_form,
                "Help_Index list",
                al,
                ac);

        n=1;
        f=fopen(HELP_FILE,"r");
        if (f!=NULL) {
            while (!feof(f)) {
                (void)get_line(f,temp,600);
                if (strncmp(temp,"HELP-INDEX>",11)==0) {
                    XmListAddItem(help_list, str_ptr = XmStringCreateLtoR((temp+11),XmFONTLIST_DEFAULT_TAG),n++);
                    XmStringFree(str_ptr);
                }
            }
            (void)fclose(f);
        }
        else
            printf("Couldn't open file: %s\n", HELP_FILE);

        button_ok = XtVaCreateManagedWidget(langcode("WPUPHPI002"),
                xmPushButtonGadgetClass, 
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, XtParent(help_list),
                XmNtopOffset,5,
                XmNbottomAttachment, XmATTACH_FORM,
                XmNbottomOffset,5,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 1,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 2,
                XmNnavigationType, XmTAB_GROUP,
                NULL);

        button_cancel = XtVaCreateManagedWidget(langcode("UNIOP00003"),
                xmPushButtonGadgetClass, 
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, XtParent(help_list),
                XmNtopOffset,5,
                XmNbottomAttachment, XmATTACH_FORM,
                XmNbottomOffset,5,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 3,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 4,
                XmNnavigationType, XmTAB_GROUP,
                NULL);

        XtAddCallback(button_cancel, XmNactivateCallback, help_index_destroy_shell, help_index_dialog);
        XtAddCallback(button_ok, XmNactivateCallback, help_view, NULL);

        pos_dialog(help_index_dialog);

        delw = XmInternAtom(XtDisplay(help_index_dialog),"WM_DELETE_WINDOW", FALSE);
        XmAddWMProtocolCallback(help_index_dialog, delw, help_index_destroy_shell, (XtPointer)help_index_dialog);

        XtManageChild(my_form);
        XtManageChild(help_list);
        XtVaSetValues(help_list, XmNbackground, colors[0x0f], NULL);
        XtManageChild(pane);

        XtPopup(help_index_dialog,XtGrabNone);
        fix_dialog_size(help_index_dialog);

        // Move focus to the Cancel button.  This appears to highlight the
        // button fine, but we're not able to hit the <Enter> key to
        // have that default function happen.  Note:  We _can_ hit the
        // <SPACE> key, and that activates the option.
//        XmUpdateDisplay(help_index_dialog);
        XmProcessTraversal(button_cancel, XmTRAVERSE_CURRENT);

    } else {
        XtPopup(help_index_dialog,XtGrabNone);
        (void)XRaiseWindow(XtDisplay(help_index_dialog), XtWindow(help_index_dialog));
    }
}




/************************** Clear stations *******************************/
/*************************************************************************/

void Stations_Clear( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer callData) {

    delete_all_stations();
    my_station_add(my_callsign,my_group,my_symbol,my_long,my_lat,my_phg,my_comment,(char)position_amb_chars);
    current_trail_color = 0x00;  // restart
    redraw_on_new_data=2;
}




/************************* Map Chooser ***********************************/
/*************************************************************************/

void map_chooser_destroy_shell( /*@unused@*/ Widget widget, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    Widget shell = (Widget) clientData;
    XtPopdown(shell);
    XtDestroyWidget(shell);
    map_chooser_dialog = (Widget)NULL;
}





void map_chooser_select_maps(Widget widget, XtPointer clientData, XtPointer callData) {
    int i,x;
    char *temp;
    XmString *list;
    FILE *f;

    // It'd be nice to turn off auto-maps here, or better perhaps would
    // be if any button were chosen other than "Cancel".

    XtVaGetValues(map_list,
               XmNitemCount,&i,
               XmNitems,&list,
               NULL);

    f=fopen(SELECTED_MAP_DATA,"w+");
    if (f!=NULL) {
        for(x=1; x<=i;x++) {
            if(XmListPosSelected(map_list,x)) {
                if(XmStringGetLtoR(list[(x-1)],XmFONTLIST_DEFAULT_TAG,&temp)) {
                    fprintf(f,"%s\n",temp);
                    XtFree(temp);
                }
            }
        }
        (void)fclose(f);
    }
    else
        printf("Couldn't open file: %s\n", SELECTED_MAP_DATA);

    map_chooser_destroy_shell(widget,clientData,callData);
    create_image(da);
    (void)XCopyArea(XtDisplay(da),pixmap_final,XtWindow(da),gc,0,0,screen_width,screen_height,0,0);
}





void map_chooser_select_vector_maps(Widget widget, XtPointer clientData, XtPointer callData) {
    int i,x;
    char *temp;
    char *ext;
    XmString *list;
    FILE *f;

    XtVaGetValues(map_list,
               XmNitemCount,&i,
               XmNitems,&list,
               NULL);

    f=fopen(SELECTED_MAP_DATA,"w+");
    if (f!=NULL) {
        for(x=1; x<=i;x++) {
            if(XmStringGetLtoR(list[(x-1)],XmFONTLIST_DEFAULT_TAG,&temp))
            {
                ext = get_map_ext (temp);
                if ( (ext != NULL)
                        && (   (strcasecmp(ext,"map") == 0)
                            || (strcasecmp(ext,"shp") == 0)
                            || (strcasecmp(ext,"pdb") == 0)
                            || (strcasecmp(ext,"gnis") == 0) ) ) {
                    fprintf(f,"%s\n",temp);
                }
                XtFree(temp);
            }
        }
        (void)fclose(f);
        map_chooser_fill_in();
    }
    else
        printf("Couldn't open file: %s\n", SELECTED_MAP_DATA);
 
//    map_chooser_destroy_shell(widget,clientData,callData);
//    create_image(da);
//    (void)XCopyArea(XtDisplay(da),pixmap_final,XtWindow(da),gc,0,0,screen_width,screen_height,0,0);
}





void map_chooser_select_250k_maps(Widget widget, XtPointer clientData, XtPointer callData) {
    int i,x,length;
    char *temp;
    char *ext;
    XmString *list;
    FILE *f;

    XtVaGetValues(map_list,
               XmNitemCount,&i,
               XmNitems,&list,
               NULL);

    f=fopen(SELECTED_MAP_DATA,"w+");
    if (f!=NULL) {
        for(x=1; x<=i;x++) {
            if(XmStringGetLtoR(list[(x-1)],XmFONTLIST_DEFAULT_TAG,&temp))
            {
                ext = get_map_ext (temp);
                length = (int)strlen(temp);
                if ( (ext != NULL) && (strcasecmp (ext, "tif") == 0)
                        && (length >= 12)   // "o48122h3.tif", we might have subdirectories also
                        && ( (temp[length - 12] == 'c') || (temp[length - 12] == 'C') ) ) {
                    fprintf(f,"%s\n",temp);
                }
                XtFree(temp);
            }
        }
        (void)fclose(f);
        map_chooser_fill_in();
    }
    else
        printf("Couldn't open file: %s\n", SELECTED_MAP_DATA);

//    map_chooser_destroy_shell(widget,clientData,callData);
//    create_image(da);
//    (void)XCopyArea(XtDisplay(da),pixmap_final,XtWindow(da),gc,0,0,screen_width,screen_height,0,0);
}





void map_chooser_select_100k_maps(Widget widget, XtPointer clientData, XtPointer callData) {
    int i,x,length;
    char *temp;
    char *ext;
    XmString *list;
    FILE *f;

    XtVaGetValues(map_list,
               XmNitemCount,&i,
               XmNitems,&list,
               NULL);

    f=fopen(SELECTED_MAP_DATA,"w+");
    if (f!=NULL)
    {
        for(x=1; x<=i;x++)
        {
            if(XmStringGetLtoR(list[(x-1)],XmFONTLIST_DEFAULT_TAG,&temp))
            {
                ext = get_map_ext (temp);
                length = (int)strlen(temp);
                if ( (ext != NULL) && (strcasecmp (ext, "tif") == 0)
                        && (length >= 12)   // "o48122h3.tif", we might have subdirectories also
                        && ( (temp[length - 12] == 'f') || (temp[length - 12] == 'F') ) ) {
                    fprintf(f,"%s\n",temp);
                }
                XtFree(temp);
            }
        }
        (void)fclose(f);
        map_chooser_fill_in();
    }
    else
        printf("Couldn't open file: %s\n", SELECTED_MAP_DATA);

//    map_chooser_destroy_shell(widget,clientData,callData);
//    create_image(da);
//    (void)XCopyArea(XtDisplay(da),pixmap_final,XtWindow(da),gc,0,0,screen_width,screen_height,0,0);
}





void map_chooser_select_24k_maps(Widget widget, XtPointer clientData, XtPointer callData) {
    int i,x,length;
    char *temp;
    char *ext;
    XmString *list;
    FILE *f;

    XtVaGetValues(map_list,
               XmNitemCount,&i,
               XmNitems,&list,
               NULL);

    f=fopen(SELECTED_MAP_DATA,"w+");
    if (f!=NULL) {
        for(x=1; x<=i;x++)
        {
            if(XmStringGetLtoR(list[(x-1)],XmFONTLIST_DEFAULT_TAG,&temp))
            {
                ext = get_map_ext (temp);
                length = (int)strlen(temp);
                if ( (ext != NULL) && (strcasecmp (ext, "tif") == 0)
                        && (length >= 12)   // "o48122h3.tif", we might have subdirectories also
                        && ( (temp[length - 12] == 'o') || (temp[length - 12] == 'O')
                            || (temp[length - 12] == 'k') || (temp[length - 12] == 'K') ) ) {
                    fprintf(f,"%s\n",temp);
                }
                XtFree(temp);
            }
        }
        (void)fclose(f);
        map_chooser_fill_in();
    }
    else
        printf("Couldn't open file: %s\n", SELECTED_MAP_DATA);

//    map_chooser_destroy_shell(widget,clientData,callData);
//    create_image(da);
//    (void)XCopyArea(XtDisplay(da),pixmap_final,XtWindow(da),gc,0,0,screen_width,screen_height,0,0);
}





void map_chooser_deselect_maps(Widget widget, XtPointer clientData, XtPointer callData) {
    FILE *f;

    // Empty the file (no maps listed as selected)
    f=fopen(SELECTED_MAP_DATA,"w+");
    if(f!=NULL)
        (void)fclose(f);
    else
        printf("Couldn't zero file: %s\n", SELECTED_MAP_DATA);

    map_chooser_fill_in();

//    map_chooser_destroy_shell(widget,clientData,callData);
//    create_image(da);
//    (void)XCopyArea(XtDisplay(da),pixmap_final,XtWindow(da),gc,0,0,screen_width,screen_height,0,0);
}





void dir_sort(char *dir) {
    struct dirent *dl;
    DIR *dm;
    char fullpath[8000];
    struct stat nfile;
    const time_t *ftime;
    int my_size;
    char *ext;

    my_size=strlen(SELECTED_MAP_DIR)+1;
    dm = opendir(dir);
    if(!dm)
        //perror("maps");
        perror(dir);
    else {
        while((dl = readdir(dm))) {
            xastir_snprintf(fullpath, sizeof(fullpath), "%s/%s", dir, dl->d_name);
            if (stat(fullpath,&nfile)==0) {
                ftime=(time_t *)&nfile.st_ctime;
                switch (nfile.st_mode & S_IFMT) {
                    case(S_IFDIR):  // We found a directory
                        /*printf("file %c letter %c\n",dl->d_name[0],letter);*/
                        if((strcmp(dl->d_name,".") !=0) && (strcmp(dl->d_name,"..") !=0))
                            dir_sort(fullpath); // Recursively call dir_sort again

                        break;

                    case(S_IFREG):  // We found a regular file
                        /*printf("FILE %d<%s>\n",*item,fullpath+my_size);*/

                        // Check whether it's a map file before shoving it into the
                        // database.  This filters out non-maps from the listing.
                        ext = get_map_ext(dl->d_name);  // Get the file extension
                        if ( (ext != NULL)
                                && (   (strcasecmp(ext, "MAP") == 0)
                                    || (strcasecmp(ext, "GEO") == 0)
                                    || (strcasecmp(ext, "PDB") == 0)
                                    || (strcasecmp(ext, "GNIS") == 0)
                                    || (strcasecmp(ext, "TIF") == 0)
                                    || (strcasecmp(ext, "SHP") == 0) ) ) {
                            (void)sort_input_database(get_user_base_dir("data/sort_maps_db.dat"),
                                    (fullpath+my_size),1000);
                        }
                        break;

                    default:
                        break;
                }
            }
        }
    }
    (void)closedir(dm);
}





void sort_list(char *filename,int size, Widget list, int *item) {
    FILE *f_data;
    FILE *f_pointer;
    char fill[2000];
    long file_ptr;
    long ptr;
    char ptr_filename[400];
    XmString str_ptr;

    // Clear the list widget first
    XmListDeleteAllItems(list);

    xastir_snprintf(ptr_filename, sizeof(ptr_filename), "%s-ptr", filename);
    f_pointer=fopen(ptr_filename,"r");
    f_data=fopen(filename,"r");
    if (f_pointer!=NULL && f_data !=NULL) {
        while (!feof(f_pointer)) {
            ptr=ftell(f_pointer);
            if (fread(&file_ptr,sizeof(file_ptr),1,f_pointer)==1) {
                (void)fseek(f_data,file_ptr,SEEK_SET);
                if (fread(fill,(size_t)size,1,f_data)==1) {
                    XmListAddItem(list, str_ptr = XmStringCreateLtoR(fill,XmFONTLIST_DEFAULT_TAG),*item);
                    XmStringFree(str_ptr);
                    (*item)++;
                }
            }
        }
    }
    if(f_pointer!=NULL)
        (void)fclose(f_pointer);
    else
        printf("Couldn't open file: %s\n", ptr_filename);


    if(f_data!=NULL)
        (void)fclose(f_data);
    else
        printf("Couldn't open file: %s\n", filename);
}





void map_chooser_fill_in (void) {
    int n,i;
    FILE *f;
    char temp[600];
    XmString str_ptr;
    map_index_record *current = map_index_head;


    busy_cursor(appshell);

    i=0;
    if (map_chooser_dialog) {

	// Empty the map_list widget first
	XmListDeleteAllItems(map_list);

        // Find the names of all the map files on disk and put them into map_list
        n=1;

        while (current != NULL) {

            //printf("%s\n",current->filename);

            XmListAddItem(map_list,
                str_ptr = XmStringCreateLtoR(current->filename,
                                XmFONTLIST_DEFAULT_TAG),
                                n);
            n++;
            current = current->next;
            XmStringFree(str_ptr);
        }

        (void)filecreate(SELECTED_MAP_DATA);   // Create empty file if it doesn't exist

        // Compare the maps in the list to those we've already selected
        // and highlight those that match.
        f=fopen(SELECTED_MAP_DATA,"r");
        if (f!=NULL) {
            while(!feof(f)) {
                (void)get_line(f,temp,600);
                i=XmListItemPos(map_list, str_ptr = XmStringCreateLtoR(temp,XmFONTLIST_DEFAULT_TAG));
                XmStringFree(str_ptr);
                if (i!=0)
                    XmListSelectPos(map_list,i,TRUE);
            }
            (void)fclose(f);
        }
        else {
            printf("Couldn't open file: %s\n", SELECTED_MAP_DATA);
        }
    }
}





///////////////////////////////////////  Configure Tigermaps Dialog //////////////////////////////////////////////
//N0VH
#if defined(HAVE_IMAGEMAGICK)



void Configure_tiger_destroy_shell( /*@unused@*/ Widget widget, XtPointer clientData, /*@unused@*/ XtPointer callData) {
	Widget shell = (Widget) clientData;
	XtPopdown(shell);
	XtDestroyWidget(shell);
	configure_tiger_dialog = (Widget)NULL;
}





void Configure_tiger_change_data(Widget widget, XtPointer clientData, XtPointer callData) {

	if(XmToggleButtonGetState(tiger_enable_widget))
		tiger_flag=TRUE;
     	else
	  	tiger_flag=FALSE;

	if(XmToggleButtonGetState(tiger_grid))
		tiger_show_grid=TRUE;
     	else
	  	tiger_show_grid=FALSE;

	if(XmToggleButtonGetState(tiger_counties))
		tiger_show_counties=TRUE;
     	else
	  	tiger_show_counties=FALSE;

	if(XmToggleButtonGetState(tiger_cities))
		tiger_show_cities=TRUE;
     	else
	  	tiger_show_cities=FALSE;

	if(XmToggleButtonGetState(tiger_places))
		tiger_show_places=TRUE;
     	else
	  	tiger_show_places=FALSE;

	if(XmToggleButtonGetState(tiger_majroads))
		tiger_show_majroads=TRUE;
     	else
	  	tiger_show_majroads=FALSE;

	if(XmToggleButtonGetState(tiger_streets))
		tiger_show_streets=TRUE;
     	else
	  	tiger_show_streets=FALSE;

	if(XmToggleButtonGetState(tiger_railroad))
		tiger_show_railroad=TRUE;
     	else
	  	tiger_show_railroad=FALSE;

	if(XmToggleButtonGetState(tiger_water))
		tiger_show_water=TRUE;
     	else
	  	tiger_show_water=FALSE;

	if(XmToggleButtonGetState(tiger_lakes))
		tiger_show_lakes=TRUE;
     	else
	  	tiger_show_lakes=FALSE;

	if(XmToggleButtonGetState(tiger_misc))
		tiger_show_misc=TRUE;
     	else
	  	tiger_show_misc=FALSE;

	if(XmToggleButtonGetState(tiger_states))
		tiger_show_states=TRUE;
     	else
	  	tiger_show_states=FALSE;

	if(XmToggleButtonGetState(tiger_interstate))
		tiger_show_interstate=TRUE;
     	else
	  	tiger_show_interstate=FALSE;

	if(XmToggleButtonGetState(tiger_ushwy))
		tiger_show_ushwy=TRUE;
     	else
	  	tiger_show_ushwy=FALSE;

	if(XmToggleButtonGetState(tiger_statehwy))
		tiger_show_statehwy=TRUE;
     	else
	  	tiger_show_statehwy=FALSE;

	XmScaleGetValue(tiger_intensity, &tigermap_intensity);
	XmScaleGetValue(tiger_timeout, &tigermap_timeout);

	Configure_tiger_destroy_shell(widget,clientData,callData);
        create_image(da);
        (void)XCopyArea(XtDisplay(da),pixmap_final,XtWindow(da),gc,0,0,screen_width,screen_height,0,0);
}





////////////////////////////////////////////// Config_tiger//////////////////////////////////////////////////////
//
//
void Config_tiger( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer callData) {
    static Widget tiger_pane, tiger_form, button_ok, button_cancel, tiger_label1, sep, sep2;

    Atom delw;

    if (!configure_tiger_dialog) {

        configure_tiger_dialog = XtVaCreatePopupShell(langcode("PULDNMP020"),
                xmDialogShellWidgetClass,
                Global.top,
                XmNdeleteResponse,XmDESTROY,
                XmNdefaultPosition, FALSE,
                NULL);

        tiger_pane = XtVaCreateWidget("Configure_tiger pane",
                xmPanedWindowWidgetClass, 
                configure_tiger_dialog,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        tiger_form =  XtVaCreateWidget("Configure_tiger tiger_form",
                xmFormWidgetClass, 
                tiger_pane,
                XmNfractionBase, 5,
                XmNautoUnmanage, FALSE,
                XmNshadowThickness, 1,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        tiger_enable_widget = XtVaCreateManagedWidget("Enable Tiger Maps",
                xmToggleButtonWidgetClass, 
                tiger_form,
                XmNtopAttachment, XmATTACH_FORM,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        sep = XtVaCreateManagedWidget("Config Tigermap sep", 
                xmSeparatorGadgetClass,
                tiger_form,
                XmNorientation, XmHORIZONTAL,
                XmNtopAttachment,XmATTACH_WIDGET,
                XmNtopWidget, tiger_enable_widget,
                XmNtopOffset, 5,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNrightAttachment,XmATTACH_FORM,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        tiger_label1  = XtVaCreateManagedWidget(langcode("MPUPTGR012"),
                xmLabelWidgetClass,
                tiger_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, sep,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 0,
                XmNleftOffset ,10,
                XmNrightAttachment, XmATTACH_NONE,
                XmNsensitive, TRUE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        tiger_grid  = XtVaCreateManagedWidget(langcode("MPUPTGR001"),
                xmToggleButtonWidgetClass,
                tiger_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, tiger_label1,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 0,
                XmNleftOffset ,10,
                XmNrightAttachment, XmATTACH_NONE,
                XmNsensitive, TRUE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        tiger_counties  = XtVaCreateManagedWidget(langcode("MPUPTGR002"),
                xmToggleButtonWidgetClass,
                tiger_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, tiger_label1,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 2,
                XmNrightAttachment, XmATTACH_NONE,
                XmNsensitive, TRUE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        tiger_states  = XtVaCreateManagedWidget(langcode("MPUPTGR008"),
                xmToggleButtonWidgetClass,
                tiger_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, tiger_label1,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 4,
                XmNrightAttachment, XmATTACH_NONE,
                XmNsensitive, TRUE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        tiger_cities  = XtVaCreateManagedWidget(langcode("MPUPTGR003"),
                xmToggleButtonWidgetClass,
                tiger_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, tiger_grid,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 0,
                XmNleftOffset ,10,
                XmNrightAttachment, XmATTACH_NONE,
                XmNsensitive, TRUE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        tiger_places  = XtVaCreateManagedWidget(langcode("MPUPTGR004"),
                xmToggleButtonWidgetClass,
                tiger_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, tiger_grid,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 2,
                XmNrightAttachment, XmATTACH_NONE,
                XmNsensitive, TRUE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        tiger_interstate  = XtVaCreateManagedWidget(langcode("MPUPTGR009"),
                xmToggleButtonWidgetClass,
                tiger_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, tiger_grid,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 4,
                XmNrightAttachment, XmATTACH_NONE,
                XmNsensitive, TRUE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        tiger_majroads  = XtVaCreateManagedWidget(langcode("MPUPTGR005"),
                xmToggleButtonWidgetClass,
                tiger_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, tiger_cities,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 0,
                XmNleftOffset ,10,
                XmNrightAttachment, XmATTACH_NONE,
                XmNsensitive, TRUE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        tiger_streets  = XtVaCreateManagedWidget(langcode("MPUPTGR006"),
                xmToggleButtonWidgetClass,
                tiger_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, tiger_cities,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 2,
                XmNrightAttachment, XmATTACH_NONE,
                XmNsensitive, TRUE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        tiger_ushwy  = XtVaCreateManagedWidget(langcode("MPUPTGR010"),
                xmToggleButtonWidgetClass,
                tiger_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, tiger_cities,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 4,
                XmNrightAttachment, XmATTACH_NONE,
                XmNsensitive, TRUE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        tiger_railroad  = XtVaCreateManagedWidget(langcode("MPUPTGR007"),
                xmToggleButtonWidgetClass,
                tiger_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, tiger_majroads,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 0,
                XmNleftOffset ,10,
                XmNrightAttachment, XmATTACH_NONE,
                XmNsensitive, TRUE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        tiger_water  = XtVaCreateManagedWidget(langcode("MPUPTGR013"),
                xmToggleButtonWidgetClass,
                tiger_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, tiger_majroads,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 2,
                XmNrightAttachment, XmATTACH_NONE,
                XmNsensitive, TRUE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        tiger_statehwy  = XtVaCreateManagedWidget(langcode("MPUPTGR011"),
                xmToggleButtonWidgetClass,
                tiger_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, tiger_majroads,
                XmNtopOffset, 4,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 4,
                XmNrightAttachment, XmATTACH_NONE,
                XmNsensitive, TRUE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

       tiger_lakes  = XtVaCreateManagedWidget(langcode("MPUPTGR014"),
                xmToggleButtonWidgetClass,
                tiger_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, tiger_railroad,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 0,
                XmNleftOffset ,10,
                XmNrightAttachment, XmATTACH_NONE,
                XmNsensitive, TRUE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

       tiger_misc  = XtVaCreateManagedWidget(langcode("MPUPTGR015"),
                xmToggleButtonWidgetClass,
                tiger_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, tiger_railroad,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 2,
                XmNrightAttachment, XmATTACH_NONE,
                XmNsensitive, TRUE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

       tiger_intensity  = XtVaCreateManagedWidget("Intensity", 
                xmScaleWidgetClass,
                tiger_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, tiger_lakes,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 0,
                XmNleftOffset, 20,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 2,
                XmNrightOffset, 0,
                XmNsensitive, TRUE,
                XmNorientation, XmHORIZONTAL,
                XmNborderWidth, 1,
                XmNminimum, 50,
                XmNmaximum, 100,
                XmNshowValue, TRUE,
                XmNvalue, tigermap_intensity,
                XtVaTypedArg, XmNtitleString, XmRString, "Intensity", 10,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

       tiger_timeout  = XtVaCreateManagedWidget("Timeout", 
                xmScaleWidgetClass,
                tiger_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, tiger_lakes,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 3,
                XmNleftOffset, 0,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 5,
                XmNrightOffset, 20,
                XmNsensitive, TRUE,
                XmNorientation, XmHORIZONTAL,
                XmNborderWidth, 1,
                XmNminimum, 10,
                XmNmaximum, 120,
                XmNshowValue, TRUE,
                XmNvalue, tigermap_timeout,
                XtVaTypedArg, XmNtitleString, XmRString, "Timeout (sec)", 10,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        sep2 = XtVaCreateManagedWidget("Config Tigermap sep2", 
                xmSeparatorGadgetClass,
                tiger_form,
                XmNorientation, XmHORIZONTAL,
                XmNtopAttachment,XmATTACH_WIDGET,
                XmNtopWidget, tiger_timeout,
                XmNtopOffset, 10,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNrightAttachment,XmATTACH_FORM,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        button_ok = XtVaCreateManagedWidget(langcode("UNIOP00001"),
                xmPushButtonGadgetClass, 
                tiger_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, sep2,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_FORM,
                XmNbottomOffset, 5,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 0,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 2,
                XmNrightOffset, 0,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        button_cancel = XtVaCreateManagedWidget(langcode("UNIOP00002"),
                xmPushButtonGadgetClass, 
                tiger_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, sep2,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_FORM,
                XmNbottomOffset, 5,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 3,
                XmNleftOffset, 0,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 5,
                XmNrightOffset, 10,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        XtAddCallback(button_ok, XmNactivateCallback, Configure_tiger_change_data, configure_tiger_dialog);

        XtAddCallback(button_cancel, XmNactivateCallback, Configure_tiger_destroy_shell, configure_tiger_dialog);

        pos_dialog(configure_tiger_dialog);

        delw = XmInternAtom(XtDisplay(configure_tiger_dialog),"WM_DELETE_WINDOW", FALSE);
        XmAddWMProtocolCallback(configure_tiger_dialog, delw, Configure_tiger_destroy_shell,
                (XtPointer)configure_tiger_dialog);

        if(tiger_flag)
            XmToggleButtonSetState(tiger_enable_widget,TRUE,FALSE);
        else
            XmToggleButtonSetState(tiger_enable_widget,FALSE,FALSE);

        if(tiger_show_grid)
            XmToggleButtonSetState(tiger_grid,TRUE,FALSE);
        else
            XmToggleButtonSetState(tiger_grid,FALSE,FALSE);

        if(tiger_show_counties)
            XmToggleButtonSetState(tiger_counties,TRUE,FALSE);
        else
            XmToggleButtonSetState(tiger_counties,FALSE,FALSE);

        if(tiger_show_cities)
            XmToggleButtonSetState(tiger_cities,TRUE,FALSE);
        else
            XmToggleButtonSetState(tiger_cities,FALSE,FALSE);

        if(tiger_show_places)
            XmToggleButtonSetState(tiger_places,TRUE,FALSE);
        else
            XmToggleButtonSetState(tiger_places,FALSE,FALSE);

        if(tiger_show_majroads)
            XmToggleButtonSetState(tiger_majroads,TRUE,FALSE);
        else
            XmToggleButtonSetState(tiger_majroads,FALSE,FALSE);

        if(tiger_show_streets)
            XmToggleButtonSetState(tiger_streets,TRUE,FALSE);
        else
            XmToggleButtonSetState(tiger_streets,FALSE,FALSE);

        if(tiger_show_railroad)
            XmToggleButtonSetState(tiger_railroad,TRUE,FALSE);
        else
            XmToggleButtonSetState(tiger_railroad,FALSE,FALSE);

        if(tiger_show_water)
            XmToggleButtonSetState(tiger_water,TRUE,FALSE);
        else
            XmToggleButtonSetState(tiger_water,FALSE,FALSE);

        if(tiger_show_lakes)
            XmToggleButtonSetState(tiger_lakes,TRUE,FALSE);
        else
            XmToggleButtonSetState(tiger_lakes,FALSE,FALSE);

        if(tiger_show_misc)
            XmToggleButtonSetState(tiger_misc,TRUE,FALSE);
        else
            XmToggleButtonSetState(tiger_misc,FALSE,FALSE);

        if(tiger_show_states)
            XmToggleButtonSetState(tiger_states,TRUE,FALSE);
        else
            XmToggleButtonSetState(tiger_states,FALSE,FALSE);

        if(tiger_show_interstate)
            XmToggleButtonSetState(tiger_interstate,TRUE,FALSE);
        else
            XmToggleButtonSetState(tiger_interstate,FALSE,FALSE);

        if(tiger_show_ushwy)
            XmToggleButtonSetState(tiger_ushwy,TRUE,FALSE);
        else
            XmToggleButtonSetState(tiger_ushwy,FALSE,FALSE);

        if(tiger_show_statehwy)
            XmToggleButtonSetState(tiger_statehwy,TRUE,FALSE);
        else
            XmToggleButtonSetState(tiger_statehwy,FALSE,FALSE);

        XtManageChild(tiger_form);
        XtManageChild(tiger_pane);

        XtPopup(configure_tiger_dialog,XtGrabNone);
        fix_dialog_size(configure_tiger_dialog);

        XmProcessTraversal(button_ok, XmTRAVERSE_CURRENT);

    } else
        (void)XRaiseWindow(XtDisplay(configure_tiger_dialog), XtWindow(configure_tiger_dialog));
}
#endif // HAVE_IMAGEMAGICK





void Map_chooser( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer callData) {
    static Widget  pane, my_form, button_none, button_V, button_ok,
            button_cancel, mess, button_C, button_F, button_O, rowcol;
    Atom delw;
    int i;
    Arg al[10];                    /* Arg List */
    register unsigned int ac = 0;           /* Arg Count */


    busy_cursor(appshell);

    i=0;
    if (!map_chooser_dialog) {
        map_chooser_dialog = XtVaCreatePopupShell(langcode("WPUPMCP001"),
                xmDialogShellWidgetClass,
                Global.top,
                XmNdeleteResponse,XmDESTROY,
                XmNdefaultPosition, FALSE,
                NULL);

        pane = XtVaCreateWidget("Map_chooser pane",
                xmPanedWindowWidgetClass, 
                map_chooser_dialog,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        my_form =  XtVaCreateWidget("Map_chooser my_form",
                xmFormWidgetClass, 
                pane,
                XmNfractionBase, 7,
                XmNautoUnmanage, FALSE,
                XmNshadowThickness, 1,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        /*set args for color */
        ac=0;
        XtSetArg(al[ac], XmNvisibleItemCount, 13); ac++;
        XtSetArg(al[ac], XmNshadowThickness, 3); ac++;
        XtSetArg(al[ac], XmNselectionPolicy, XmMULTIPLE_SELECT); ac++;
        XtSetArg(al[ac], XmNscrollBarPlacement, XmBOTTOM_RIGHT); ac++;
        XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
        XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;


        map_list = XmCreateScrolledList(my_form,
                "Map_chooser list",
                al,
                ac);

        // Find the names of all the map files on disk and put them into map_list
        map_chooser_fill_in();

        // This is the label at the top: "Select Maps"
        mess = XtVaCreateManagedWidget(langcode("WPUPMCP002"),
                xmLabelWidgetClass, 
                my_form,
                XmNtopAttachment, XmATTACH_FORM,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 5,
                XmNrightAttachment, XmATTACH_FORM,
                XmNrightOffset, 5,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        XtVaSetValues(XtParent(map_list),
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget,mess,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNrightAttachment, XmATTACH_FORM,
                XmNrightOffset, 5,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 5,
                NULL);


        // Attach a rowcolumn manager widget to my_form to handle all of the buttons
        rowcol = XtVaCreateManagedWidget("Map Chooser rowcol", 
                xmRowColumnWidgetClass, 
                my_form,
                XmNorientation, XmHORIZONTAL,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, XtParent(map_list),
                XmNbottomAttachment, XmATTACH_FORM,
                XmNleftAttachment, XmATTACH_FORM,
                XmNrightAttachment, XmATTACH_FORM,
                XmNkeyboardFocusPolicy, XmEXPLICIT,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);



// "None"
        button_none = XtVaCreateManagedWidget(langcode("PULDNMMC01"),
                xmPushButtonGadgetClass, 
                rowcol,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);


// "Vector Maps"
        button_V = XtVaCreateManagedWidget(langcode("PULDNMMC02"),
                xmPushButtonGadgetClass, 
                rowcol,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

// "250k Topos"
        button_C = XtVaCreateManagedWidget(langcode("PULDNMMC03"),
                xmPushButtonGadgetClass, 
                rowcol,
                XmNnavigationType, XmTAB_GROUP,
#ifndef HAVE_GEOTIFF
                XmNsensitive, FALSE,
#endif /* HAVE_GEOTIFF */
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

// "100k Topos"
        button_F = XtVaCreateManagedWidget(langcode("PULDNMMC04"),
                xmPushButtonGadgetClass, 
                rowcol,
                XmNnavigationType, XmTAB_GROUP,
#ifndef HAVE_GEOTIFF
                XmNsensitive, FALSE,
#endif /* HAVE_GEOTIFF */
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

// "24k Topos"
        button_O = XtVaCreateManagedWidget(langcode("PULDNMMC05"),
                xmPushButtonGadgetClass, 
                rowcol,
                XmNnavigationType, XmTAB_GROUP,
#ifndef HAVE_GEOTIFF
                XmNsensitive, FALSE,
#endif /* HAVE_GEOTIFF */
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

// "OK"
        button_ok = XtVaCreateManagedWidget(langcode("UNIOP00001"),
                xmPushButtonGadgetClass, 
                rowcol,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

// "Cancel"
        button_cancel = XtVaCreateManagedWidget(langcode("UNIOP00002"),
                xmPushButtonGadgetClass, 
                rowcol,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        XtAddCallback(button_cancel, XmNactivateCallback, map_chooser_destroy_shell, map_chooser_dialog);
        XtAddCallback(button_ok, XmNactivateCallback, map_chooser_select_maps, map_chooser_dialog);
        XtAddCallback(button_none, XmNactivateCallback, map_chooser_deselect_maps, map_chooser_dialog);
        XtAddCallback(button_V, XmNactivateCallback, map_chooser_select_vector_maps, map_chooser_dialog);
#ifdef HAVE_GEOTIFF
        XtAddCallback(button_C, XmNactivateCallback, map_chooser_select_250k_maps, map_chooser_dialog);
        XtAddCallback(button_F, XmNactivateCallback, map_chooser_select_100k_maps, map_chooser_dialog);
        XtAddCallback(button_O, XmNactivateCallback, map_chooser_select_24k_maps, map_chooser_dialog);
#endif /* HAVE_GEOTIFF */

        pos_dialog(map_chooser_dialog);

        delw = XmInternAtom(XtDisplay(map_chooser_dialog),"WM_DELETE_WINDOW", FALSE);
        XmAddWMProtocolCallback(map_chooser_dialog, delw, map_chooser_destroy_shell, (XtPointer)map_chooser_dialog);

        XtManageChild(rowcol);
        XtManageChild(my_form);
        XtManageChild(map_list);
        XtVaSetValues(map_list, XmNbackground, colors[0x0f], NULL);
        XtManageChild(pane);

        XtPopup(map_chooser_dialog,XtGrabNone);

        // Fix the dialog height only, allow the width to vary.
        fix_dialog_vsize(map_chooser_dialog);

        // Move focus to the OK button.  This appears to highlight the
        // button fine, but we're not able to hit the <Enter> key to
        // have that default function happen.  Note:  We _can_ hit the
        // <SPACE> key, and that activates the option.
//        XmUpdateDisplay(map_chooser_dialog);
        XmProcessTraversal(button_ok, XmTRAVERSE_CURRENT);

   } else {
        (void)XRaiseWindow(XtDisplay(map_chooser_dialog), XtWindow(map_chooser_dialog));
    }
}





/****** Read in file **********/

void read_file_selection_destroy_shell( /*@unused@*/ Widget widget, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    Widget shell = (Widget) clientData;
    XtDestroyWidget(shell);
    read_selection_dialog = (Widget)NULL;
}





void read_file_selection_now(Widget w, XtPointer clientData, XtPointer callData) {
    char *file;
    XmFileSelectionBoxCallbackStruct *cbs =(XmFileSelectionBoxCallbackStruct*)callData;

    if(XmStringGetLtoR(cbs->value,XmFONTLIST_DEFAULT_TAG,&file)) {
        // printf("FILE is %s\n",file);

        // Make sure we're not already reading a file and the user actually
        // selected a file (if not, the last character will be a '/').
        if ( (!read_file) && (file[strlen(file) - 1] != '/') ) {

            /* do read file start */
            read_file_ptr = fopen(file,"r");
            if (read_file_ptr != NULL)
                read_file = 1;
            else
                printf("Couldn't open file: %s\n", file);

        }
        XtFree(file);
    }
    read_file_selection_destroy_shell(w, clientData, callData);

    // Note that we leave the file in the "open" state.  UpdateTime()
    // comes along shortly and reads the file.
}





void Read_File_Selection( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer callData) {
    Arg al[10];                    /* Arg List */
    register unsigned int ac = 0;           /* Arg Count */
    Widget fs;

    if (read_selection_dialog!=NULL)
        read_file_selection_destroy_shell(read_selection_dialog, read_selection_dialog, NULL);

    if(read_selection_dialog==NULL) {

        // This is necessary because the resources for setting the
        // directory in the FileSelectionDialog aren't working in Lesstif.
        chdir( get_user_base_dir("logs") );

        /*set args for color */
        ac=0;
        XtSetArg(al[ac], XmNtitle, "Open Log File"); ac++;
        XtSetArg(al[ac], XmNtraversalOn, TRUE); ac++;
        XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
        XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;

        //XtSetArg(al[ac], XmNdirMask, "/home/hacker/.xastir/logs/*"); ac++;
        //XtSetArg(al[ac], XmNdirectory, "/home/hacker/.xastir/logs/"); ac++;
        //XtSetArg(al[ac], XmNpattern, "*"); ac++;
        //XtSetArg(al[ac], XmNdirMask, ".xastir/logs/*"); ac++;
        read_selection_dialog = XmCreateFileSelectionDialog(Global.top,
                "filesb",
                al,
                ac);

        // Change back to the base directory 
        chdir( get_user_base_dir("") );

        fs=XmFileSelectionBoxGetChild(read_selection_dialog,(unsigned char)XmDIALOG_TEXT);
        XtVaSetValues(fs,XmNbackground, colors[0x0f],NULL);

        fs=XmFileSelectionBoxGetChild(read_selection_dialog,(unsigned char)XmDIALOG_FILTER_TEXT);
        XtVaSetValues(fs,XmNbackground, colors[0x0f],NULL);

        fs=XmFileSelectionBoxGetChild(read_selection_dialog,(unsigned char)XmDIALOG_DIR_LIST);
        XtVaSetValues(fs,XmNbackground, colors[0x0f],NULL);

        fs=XmFileSelectionBoxGetChild(read_selection_dialog,(unsigned char)XmDIALOG_LIST);
        XtVaSetValues(fs,XmNbackground, colors[0x0f],NULL);

        //XtVaSetValues(read_selection_dialog, XmNdirMask, "/home/hacker/.xastir/logs/*", NULL);

        XtAddCallback(read_selection_dialog, XmNcancelCallback,read_file_selection_destroy_shell,read_selection_dialog);
        XtAddCallback(read_selection_dialog, XmNokCallback,read_file_selection_now,read_selection_dialog);

        XtAddCallback(read_selection_dialog, XmNhelpCallback, Help_Index, read_selection_dialog);

        XtManageChild(read_selection_dialog);
        pos_dialog(read_selection_dialog);
    }
}





void Test(Widget w, XtPointer clientData, XtPointer callData) {
    mdisplay_file(0);
    //mem_display();
    alert_print_list();

    draw_wind_barb(50000000l,   // long x_long,
        32000000l,              // long y_lat,
        "169",                  // char *speed,
        "005",                  // char *course,
        sec_now(),              // time_t sec_heard,
        pixmap_final);          // Pixmap where);

    draw_wind_barb(60000000l,   // long x_long,
        32000000l,              // long y_lat,
        "009",                  // char *speed,
        "123",                  // char *course,
        sec_now(),              // time_t sec_heard,
        pixmap_final);          // Pixmap where);

    draw_wind_barb(70000000l,   // long x_long,
        32000000l,              // long y_lat,
        "109",                  // char *speed,
        "185",                  // char *course,
        sec_now(),              // time_t sec_heard,
        pixmap_final);          // Pixmap where);

    draw_wind_barb(80000000l,   // long x_long,
        32000000l,              // long y_lat,
        "079",                  // char *speed,
        "275",                  // char *course,
        sec_now(),              // time_t sec_heard,
        pixmap_final);          // Pixmap where);

    (void)XCopyArea(XtDisplay(da),pixmap_final,XtWindow(da),gc,0,0,screen_width,screen_height,0,0);
}





/****** Save Config data **********/

void Save_Config( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer callData) {
    save_data();
}





///////////////////////////////////   Configure Defaults Dialog   //////////////////////////////////


void defaults_old_toggle( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if (state->set)
        old_station_item = atoi(which);
    else
        old_station_item = 0;

}





void defaults_clear_toggle( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        clear_station_item = atoi(which);
    else
        clear_station_item = 0;

}





void Configure_defaults_destroy_shell( /*@unused@*/ Widget widget, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    Widget shell = (Widget) clientData;
    XtPopdown(shell);
    XtDestroyWidget(shell);
    configure_defaults_dialog = (Widget)NULL;
}





void Configure_defaults_change_data(Widget widget, XtPointer clientData, XtPointer callData) {
    char *temp;

    switch(old_station_item) {
        case(1):
            sec_old=(time_t)900l;
            break;

        case(2):
            sec_old=(time_t)1800l;
            break;

        case(3):
            sec_old=(time_t)2700l;
            break;

        case(4):
            sec_old=(time_t)3600l;
            break;

        case(5):
            sec_old=(time_t)5400l;
            break;

        case(6):
            sec_old=(time_t)7200l;
            break;

        default:
            break;
    }

    switch (clear_station_item) {
        case(1):
            sec_clear=(time_t)21600l;
            break;

        case(2):
            sec_clear=(time_t)43200l;
            break;

        case(3):
            sec_clear=(time_t)86400l;
            break;

        case(4):
            sec_clear=(time_t)172800l;
            break;

        case(5):
            sec_clear=(time_t)604800l;
            break;

        default:
            break;
    }
    sec_remove = sec_clear*2;
    if (sec_remove < (time_t)(24*3600))
        sec_remove = (time_t)(24*3600);

    output_station_type = Station_transmit_type;
    if ((output_station_type >= 1) && (output_station_type <= 3)) {
        next_time = 60;
        max_transmit_time = (time_t)120l;       // shorter beacon interval for mobile stations
    } else {
        max_transmit_time = (time_t)900l;
    }

    // Check for proper symbol in case we're a weather station
    (void)check_weather_symbol();

    // Check for NWS symbol and print warning if so
    (void)check_nws_weather_symbol();

#ifdef TRANSMIT_RAW_WX
    transmit_raw_wx = (int)XmToggleButtonGetState(raw_wx_tx);
#endif

    transmit_compressed_objects_items = (int)XmToggleButtonGetState(compressed_objects_items_tx);

    pop_up_new_bulletins = (int)XmToggleButtonGetState(new_bulletin_popup_enable);
    view_zero_distance_bulletins = (int)XmToggleButtonGetState(zero_bulletin_popup_enable);

    warn_about_mouse_modifiers = (int)XmToggleButtonGetState(warn_about_mouse_modifiers_enable);

    altnet = (int)(XmToggleButtonGetState(altnet_active));

    // Small memory leak in below statement:
    //strcpy(altnet_call, XmTextGetString(altnet_text));
    // Change to:
    temp = XmTextGetString(altnet_text);
    strcpy(altnet_call,temp);
    XtFree(temp);
    
    (void)remove_trailing_spaces(altnet_call);
    if (strlen(altnet_call)==0) {
        altnet = FALSE;
        strcpy(altnet_call, "XASTIR");
    }

    operate_as_an_igate=Igate_type;

    redraw_on_new_data=2;
    Configure_defaults_destroy_shell(widget,clientData,callData);
}





/* Posit rate radio buttons */
void posit_time_toggle( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        POSIT_rate = (time_t)atoi(which);
    else
        POSIT_rate = (time_t)30*60l;    // Default 30 minutes

    // Set the new posit rate into effect immediately
    posit_next_time = posit_last_time + POSIT_rate;
}





/* GPS sample time radio buttons */
void gps_time_toggle( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        GPS_time = (time_t)atoi(which);
    else
        GPS_time = (time_t)30l;

    switch(GPS_time) {
        case(5):
            gps_time=(time_t)5l;
            break;

        case(15):
            gps_time=(time_t)15l;
            break;

        case(30):
            gps_time=(time_t)30l;
            break;

        case(60):
            gps_time=(time_t)60l;
            break;

        case(120):
            gps_time=(time_t)120l;
            break;

        case(300):
            gps_time=(time_t)300l;
            break;

        case(600):
            gps_time=(time_t)600l;
            break;

        default:
            gps_time=(time_t)30l;
            break;
    }
    // Set the new GPS rate into effect immediately
    sec_next_gps = sec_now() + gps_time;
}





/* Station_transmit type radio buttons */
void station_type_toggle( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        Station_transmit_type = atoi(which);
    else
        Station_transmit_type = 0;

}





/* Igate type radio buttons */
void igate_type_toggle( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        Igate_type = atoi(which);
    else
        Igate_type = 0;
}





void Configure_defaults( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer callData) {
    static Widget  pane, my_form, button_ok, button_cancel, frame, frame2, frame3, frame4, frame5, frame6,
                time, time_box,
                time_15, time_30, time_45, time_60, time_90, time_120,
                time2, time_box2,
                time_6h, time_12h, time_24h, time_48h, time_168h,
                time3, time_box3,
                toption1, toption2, toption3, toption4, toption5, toption6, toption7,
                station_type, type_box,
                styp1, styp2, styp3, styp4, styp5, styp6,
                igate_option, igate_box,
                igtyp0, igtyp1, igtyp2,
                posit_label, posit_box,
                poption1, poption2, poption3, poption4, poption5, poption6, poption7, poption8;
    Atom delw;
    Arg al[2];                    /* Arg List */
    register unsigned int ac = 0;           /* Arg Count */

    if (!configure_defaults_dialog) {
        configure_defaults_dialog = XtVaCreatePopupShell(langcode("WPUPCFD001"),
                xmDialogShellWidgetClass,
                Global.top,
                XmNdeleteResponse,XmDESTROY,
                XmNdefaultPosition, FALSE,
                NULL);

        pane = XtVaCreateWidget("Configure_defaults pane",
                xmPanedWindowWidgetClass, 
                configure_defaults_dialog,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        my_form =  XtVaCreateWidget("Configure_defaults my_form",
                xmFormWidgetClass, 
                pane,
                XmNfractionBase, 5,
                XmNautoUnmanage, FALSE,
                XmNshadowThickness, 1,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);


        // Interval for station being considered old
        frame = XtVaCreateManagedWidget("Configure_defaults frame", 
                xmFrameWidgetClass, 
                my_form,
                XmNtopAttachment,XmATTACH_FORM,
                XmNtopOffset,10,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 10,
                XmNrightAttachment,XmATTACH_FORM,
                XmNrightOffset, 10,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        time = XtVaCreateManagedWidget(langcode("WPUPCFD002"),
                xmLabelWidgetClass,
                frame,
                XmNchildType, XmFRAME_TITLE_CHILD,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        /*set args for color */
        ac=0;
        XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
        XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;


        time_box = XmCreateRadioBox(frame,
                "Configure_defaults Time box",
                al,
                ac);

        XtVaSetValues(time_box,
                XmNpacking, XmPACK_TIGHT,
                XmNorientation, XmHORIZONTAL,
                XmNnumColumns,5,
                NULL);


        time_15 = XtVaCreateManagedWidget(langcode("WPUPCFD003"),
                xmToggleButtonGadgetClass,
                time_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(time_15,XmNvalueChangedCallback,defaults_old_toggle,"1");


        time_30 = XtVaCreateManagedWidget(langcode("WPUPCFD004"),
                xmToggleButtonGadgetClass,
                time_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(time_30,XmNvalueChangedCallback,defaults_old_toggle,"2");


        time_45 = XtVaCreateManagedWidget(langcode("WPUPCFD005"),
                xmToggleButtonGadgetClass,
                time_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(time_45,XmNvalueChangedCallback,defaults_old_toggle,"3");


        time_60 = XtVaCreateManagedWidget(langcode("WPUPCFD006"),
                xmToggleButtonGadgetClass,
                time_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(time_60,XmNvalueChangedCallback,defaults_old_toggle,"4");


        time_90 = XtVaCreateManagedWidget(langcode("WPUPCFD007"),
                xmToggleButtonGadgetClass,
                time_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(time_90,XmNvalueChangedCallback,defaults_old_toggle,"5");


        time_120 = XtVaCreateManagedWidget(langcode("WPUPCFD008"),
                xmToggleButtonGadgetClass,
                time_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(time_120,XmNvalueChangedCallback,defaults_old_toggle,"6");


        // Interval for station not being displayed
        frame2 = XtVaCreateManagedWidget("Configure_defaults frame2", 
                xmFrameWidgetClass, 
                my_form,
                XmNtopAttachment,XmATTACH_WIDGET,
                XmNtopWidget, frame,
                XmNtopOffset,10,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 10,
                XmNrightAttachment,XmATTACH_FORM,
                XmNrightOffset, 10,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        time2 = XtVaCreateManagedWidget(langcode("WPUPCFD009"),
                xmLabelWidgetClass,
                frame2,
                XmNchildType, XmFRAME_TITLE_CHILD,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        time_box2 = XmCreateRadioBox(frame2,
                "Configure_defaults Time box2",
                al,
                ac);

        XtVaSetValues(time_box2,
                XmNpacking, XmPACK_TIGHT,
                XmNorientation, XmHORIZONTAL,
                XmNnumColumns,5,
                NULL);

        time_6h = XtVaCreateManagedWidget(langcode("WPUPCFD010"),
                xmToggleButtonGadgetClass,
                time_box2,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(time_6h,XmNvalueChangedCallback,defaults_clear_toggle,"1");


        time_12h = XtVaCreateManagedWidget(langcode("WPUPCFD011"),
                xmToggleButtonGadgetClass,
                time_box2,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(time_12h,XmNvalueChangedCallback,defaults_clear_toggle,"2");


        time_24h = XtVaCreateManagedWidget(langcode("WPUPCFD012"),
                xmToggleButtonGadgetClass,
                time_box2,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(time_24h,XmNvalueChangedCallback,defaults_clear_toggle,"3");


        time_48h = XtVaCreateManagedWidget(langcode("WPUPCFD013"),
                xmToggleButtonGadgetClass,
                time_box2,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(time_48h,XmNvalueChangedCallback,defaults_clear_toggle,"4");


        time_168h = XtVaCreateManagedWidget(langcode("WPUPCFD014"),
                xmToggleButtonGadgetClass,
                time_box2,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(time_168h,XmNvalueChangedCallback,defaults_clear_toggle,"5");


        // Posit Time
        frame6 = XtVaCreateManagedWidget("Configure_defaults frame6", 
                xmFrameWidgetClass, 
                my_form,
                XmNtopAttachment,XmATTACH_WIDGET,
                XmNtopWidget, frame2,
                XmNtopOffset,10,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 10,
                XmNrightAttachment,XmATTACH_FORM,
                XmNrightOffset, 10,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        posit_label  = XtVaCreateManagedWidget(langcode("WPUPCFD026"),
                xmLabelWidgetClass,
                frame6,
                XmNchildType, XmFRAME_TITLE_CHILD,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        posit_box = XmCreateRadioBox(frame6,
                "Configure_defaults Posit Options box",
                al,
                ac);

        XtVaSetValues(posit_box,
                XmNpacking, XmPACK_TIGHT,
                XmNorientation, XmHORIZONTAL,
                XmNnumColumns,7,
                NULL);

        // 30 seconds
        poption1 = XtVaCreateManagedWidget(langcode("WPUPCFG011"),
                xmToggleButtonGadgetClass,
                posit_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(poption1,XmNvalueChangedCallback,posit_time_toggle,"30");

        // 60 seconds
        poption2 = XtVaCreateManagedWidget(langcode("WPUPCFG012"),
                xmToggleButtonGadgetClass,
                posit_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(poption2,XmNvalueChangedCallback,posit_time_toggle,"60");

        // 2 minutes
        poption3 = XtVaCreateManagedWidget(langcode("WPUPCFG013"),
                xmToggleButtonGadgetClass,
                posit_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(poption3,XmNvalueChangedCallback,posit_time_toggle,"120");

        // 5 minutes
        poption4 = XtVaCreateManagedWidget(langcode("WPUPCFG014"),
                xmToggleButtonGadgetClass,
                posit_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(poption4,XmNvalueChangedCallback,posit_time_toggle,"300");

        // 10 minutes
        poption5 = XtVaCreateManagedWidget(langcode("WPUPCFG015"),
                xmToggleButtonGadgetClass,
                posit_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(poption5,XmNvalueChangedCallback,posit_time_toggle,"600");

        // 15 minutes
        poption6 = XtVaCreateManagedWidget(langcode("WPUPCFD003"),
                xmToggleButtonGadgetClass,
                posit_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(poption6,XmNvalueChangedCallback,posit_time_toggle,"900");

        // 30 minutes
        poption7 = XtVaCreateManagedWidget(langcode("WPUPCFD004"),
                xmToggleButtonGadgetClass,
                posit_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(poption7,XmNvalueChangedCallback,posit_time_toggle,"1800");

        // 45 minutes
        poption8 = XtVaCreateManagedWidget(langcode("WPUPCFD005"),
                xmToggleButtonGadgetClass,
                posit_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(poption8,XmNvalueChangedCallback,posit_time_toggle,"2700");


        // GPS Time
        frame3 = XtVaCreateManagedWidget("Configure_defaults frame3", 
                xmFrameWidgetClass, 
                my_form,
                XmNtopAttachment,XmATTACH_WIDGET,
                XmNtopWidget, frame6,
                XmNtopOffset,10,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 10,
                XmNrightAttachment,XmATTACH_FORM,
                XmNrightOffset, 10,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        time3  = XtVaCreateManagedWidget(langcode("WPUPCFG008"),
                xmLabelWidgetClass,
                frame3,
                XmNchildType, XmFRAME_TITLE_CHILD,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        time_box3 = XmCreateRadioBox(frame3,
                "Configure_defaults Time Options box",
                al,
                ac);

        XtVaSetValues(time_box3,
                XmNpacking, XmPACK_TIGHT,
                XmNorientation, XmHORIZONTAL,
                XmNnumColumns,7,
                NULL);

        toption1 = XtVaCreateManagedWidget(langcode("WPUPCFG009"),
                xmToggleButtonGadgetClass,
                time_box3,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(toption1,XmNvalueChangedCallback,gps_time_toggle,"5");


        toption2 = XtVaCreateManagedWidget(langcode("WPUPCFG010"),
                xmToggleButtonGadgetClass,
                time_box3,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(toption2,XmNvalueChangedCallback,gps_time_toggle,"15");


        toption3 = XtVaCreateManagedWidget(langcode("WPUPCFG011"),
                xmToggleButtonGadgetClass,
                time_box3,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(toption3,XmNvalueChangedCallback,gps_time_toggle,"30");

        toption4 = XtVaCreateManagedWidget(langcode("WPUPCFG012"),
                xmToggleButtonGadgetClass,
                time_box3,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(toption4,XmNvalueChangedCallback,gps_time_toggle,"60");

        toption5 = XtVaCreateManagedWidget(langcode("WPUPCFG013"),
                xmToggleButtonGadgetClass,
                time_box3,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(toption5,XmNvalueChangedCallback,gps_time_toggle,"120");

        toption6 = XtVaCreateManagedWidget(langcode("WPUPCFG014"),
                xmToggleButtonGadgetClass,
                time_box3,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(toption6,XmNvalueChangedCallback,gps_time_toggle,"300");

        toption7 = XtVaCreateManagedWidget(langcode("WPUPCFG015"),
                xmToggleButtonGadgetClass,
                time_box3,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(toption7,XmNvalueChangedCallback,gps_time_toggle,"600");


        // Transmit Station Options
        frame4 = XtVaCreateManagedWidget("Configure_defaults frame4", 
                xmFrameWidgetClass, 
                my_form,
                XmNtopAttachment,XmATTACH_WIDGET,
                XmNtopWidget, frame3,
                XmNtopOffset,10,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 10,
                XmNrightAttachment,XmATTACH_FORM,
                XmNrightOffset, 10,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        station_type  = XtVaCreateManagedWidget(langcode("WPUPCFD015"),
                xmLabelWidgetClass,
                frame4,
                XmNchildType, XmFRAME_TITLE_CHILD,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        type_box = XmCreateRadioBox(frame4,
                "Configure_defaults Transmit Options box",
                al,
                ac);

        XtVaSetValues(type_box,
                XmNnumColumns,2,
                NULL);

        styp1 = XtVaCreateManagedWidget(langcode("WPUPCFD016"),
                xmToggleButtonGadgetClass,
                type_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(styp1,XmNvalueChangedCallback,station_type_toggle,"0");

        styp2 = XtVaCreateManagedWidget(langcode("WPUPCFD017"),
                xmToggleButtonGadgetClass,
                type_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(styp2,XmNvalueChangedCallback,station_type_toggle,"1");

        styp3 = XtVaCreateManagedWidget(langcode("WPUPCFD018"),
                xmToggleButtonGadgetClass,
                type_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(styp3,XmNvalueChangedCallback,station_type_toggle,"2");

        styp4 = XtVaCreateManagedWidget(langcode("WPUPCFD019"),
                xmToggleButtonGadgetClass,
                type_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(styp4,XmNvalueChangedCallback,station_type_toggle,"3");

        styp5 = XtVaCreateManagedWidget(langcode("WPUPCFD021"),
                xmToggleButtonGadgetClass,
                type_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(styp5,XmNvalueChangedCallback,station_type_toggle,"4");

        styp6 = XtVaCreateManagedWidget(langcode("WPUPCFD022"),
                xmToggleButtonGadgetClass,
                type_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(styp6,XmNvalueChangedCallback,station_type_toggle,"5");


        // Igate Options
        frame5 = XtVaCreateManagedWidget("Configure_defaults frame5", 
                xmFrameWidgetClass, 
                my_form,
                XmNtopAttachment,XmATTACH_WIDGET,
                XmNtopWidget, frame4,
                XmNtopOffset,10,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 10,
                XmNrightAttachment,XmATTACH_FORM,
                XmNrightOffset, 10,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        igate_option  = XtVaCreateManagedWidget(langcode("IGPUPCF000"),
                xmLabelWidgetClass,
                frame5,
                XmNchildType, XmFRAME_TITLE_CHILD,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        igate_box = XmCreateRadioBox(frame5,
                "Configure_defaults Igate Options box",
                al,
                ac);

        XtVaSetValues(igate_box,
                XmNnumColumns,2,
                NULL);

        igtyp0 = XtVaCreateManagedWidget(langcode("IGPUPCF001"),
                xmToggleButtonGadgetClass,
                igate_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        XtAddCallback(igtyp0,XmNvalueChangedCallback,igate_type_toggle,"0");

        igtyp1 = XtVaCreateManagedWidget(langcode("IGPUPCF002"),
                xmToggleButtonGadgetClass,
                igate_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        XtAddCallback(igtyp1,XmNvalueChangedCallback,igate_type_toggle ,"1");

        igtyp2 = XtVaCreateManagedWidget(langcode("IGPUPCF003"),
                xmToggleButtonGadgetClass,
                igate_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        XtAddCallback(igtyp2,XmNvalueChangedCallback,igate_type_toggle,"2");


        // Miscellaneous Options
        compressed_objects_items_tx = XtVaCreateManagedWidget(langcode("WPUPCFD024"),
                xmToggleButtonWidgetClass,
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, frame5,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset,10,
                XmNrightAttachment, XmATTACH_NONE,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        new_bulletin_popup_enable = XtVaCreateManagedWidget(langcode("WPUPCFD027"),
                xmToggleButtonWidgetClass,
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, compressed_objects_items_tx,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset,10,
                XmNrightAttachment, XmATTACH_NONE,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        warn_about_mouse_modifiers_enable = XtVaCreateManagedWidget(langcode("WPUPCFD028"),
                xmToggleButtonWidgetClass,
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, compressed_objects_items_tx,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, new_bulletin_popup_enable,
                XmNleftOffset,10,
                XmNrightAttachment, XmATTACH_NONE,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        zero_bulletin_popup_enable = XtVaCreateManagedWidget(langcode("WPUPCFD029"),
                xmToggleButtonWidgetClass,
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, new_bulletin_popup_enable,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset,10,
                XmNrightAttachment, XmATTACH_NONE,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);


#ifdef TRANSMIT_RAW_WX
        raw_wx_tx  = XtVaCreateManagedWidget(langcode("WPUPCFD023"),
                xmToggleButtonWidgetClass,
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, zero_bulletin_popup_enable,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_NONE,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
#endif

        altnet_active  = XtVaCreateManagedWidget(langcode("WPUPCFD025"),
                xmToggleButtonWidgetClass,
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, frame5,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_NONE,
                XmNrightAttachment, XmATTACH_FORM,
                XmNrightOffset, 100,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

//        altnet_text = XtVaCreateManagedWidget("Configure_defaults Altnet_text", 
//                xmTextFieldWidgetClass, 
//                my_form,
        altnet_text = XtVaCreateManagedWidget("Configure_defaults Altnet_text", 
                xmTextWidgetClass, 
                my_form,
                XmNeditable,   TRUE,
                XmNcursorPositionVisible, TRUE,
                XmNsensitive, TRUE,
                XmNshadowThickness,    1,
                XmNcolumns, 9,
                XmNwidth, ((10*7)+2),
                XmNmaxLength, 9,
                XmNbackground, colors[0x0f],
                XmNtopAttachment,XmATTACH_WIDGET,
                XmNtopWidget, altnet_active,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_NONE,
                XmNrightAttachment,XmATTACH_FORM,
                XmNrightOffset, 160,
                XmNnavigationType, XmTAB_GROUP,
                NULL);


        button_ok = XtVaCreateManagedWidget(langcode("UNIOP00001"),
                xmPushButtonGadgetClass, 
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
#ifdef TRANSMIT_RAW_WX
                XmNtopWidget, raw_wx_tx,
#else
                XmNtopWidget, zero_bulletin_popup_enable,
#endif
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_FORM,
                XmNbottomOffset, 5,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 1,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 2,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);


        button_cancel = XtVaCreateManagedWidget(langcode("UNIOP00002"),
                xmPushButtonGadgetClass, 
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
#ifdef TRANSMIT_RAW_WX
                XmNtopWidget, raw_wx_tx,
#else
                XmNtopWidget, zero_bulletin_popup_enable,
#endif
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_FORM,
                XmNbottomOffset, 5,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 3,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 4,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        XtAddCallback(button_ok, XmNactivateCallback, Configure_defaults_change_data, configure_defaults_dialog);
        XtAddCallback(button_cancel, XmNactivateCallback, Configure_defaults_destroy_shell, configure_defaults_dialog);

        // Set the toggle buttons based on current data
        switch (sec_old) {
            case(900l):
                XmToggleButtonSetState(time_15,TRUE,FALSE);
                old_station_item=1;
                break;

            case(1800l):
                XmToggleButtonSetState(time_30,TRUE,FALSE);
                old_station_item=2;
                break;

            case(2700l):
                XmToggleButtonSetState(time_45,TRUE,FALSE);
                old_station_item=3;
                break;

            case(3600l):
                XmToggleButtonSetState(time_60,TRUE,FALSE);
                old_station_item=4;
                break;

            case(5400l):
                XmToggleButtonSetState(time_90,TRUE,FALSE);
                old_station_item=5;
                break;

            case(7200l):
                XmToggleButtonSetState(time_120,TRUE,FALSE);
                old_station_item=6;
                break;

            default:
                XmToggleButtonSetState(time_120,TRUE,FALSE);
                old_station_item=6;
                break;
        }

        switch(sec_clear) {
            case(21600l):
                XmToggleButtonSetState(time_6h,TRUE,FALSE);
                clear_station_item=1;
                break;

            case(43200l):
                XmToggleButtonSetState(time_12h,TRUE,FALSE);
                clear_station_item=2;
                break;

            case(86400l):
                XmToggleButtonSetState(time_24h,TRUE,FALSE);
                clear_station_item=3;
                break;

            case(172800l):
                XmToggleButtonSetState(time_48h,TRUE,FALSE);
                clear_station_item=4;
                break;

            case(604800l):
                XmToggleButtonSetState(time_168h,TRUE,FALSE);
                clear_station_item=5;
                break;

            default:
                XmToggleButtonSetState(time_168h,TRUE,FALSE);
                clear_station_item=5;
                break;
        }

        switch(POSIT_rate) {
            case(30):
                XmToggleButtonSetState(poption1,TRUE,FALSE);
                break;

            case(60):
                XmToggleButtonSetState(poption2,TRUE,FALSE);
                break;

            case(120):
                XmToggleButtonSetState(poption3,TRUE,FALSE);
                break;

            case(300):
                XmToggleButtonSetState(poption4,TRUE,FALSE);
                break;

            case(600):
                XmToggleButtonSetState(poption5,TRUE,FALSE);
                break;

            case(900):
                XmToggleButtonSetState(poption6,TRUE,FALSE);
                break;

            case(1800):
                XmToggleButtonSetState(poption7,TRUE,FALSE);
                break;

            case(2700):
                XmToggleButtonSetState(poption8, TRUE,FALSE);
                break;

            default:
                XmToggleButtonSetState(poption7,TRUE,FALSE);    // 30 minutes
                break;
        }


        switch(gps_time) {
            case(5):
                XmToggleButtonSetState(toption1,TRUE,FALSE);
                GPS_time=(time_t)5l;
                break;

            case(15):
                XmToggleButtonSetState(toption2,TRUE,FALSE);
                GPS_time=(time_t)15l;
                break;

            case(30):
                XmToggleButtonSetState(toption3,TRUE,FALSE);
                GPS_time=(time_t)30l;
                break;

            case(60):
                XmToggleButtonSetState(toption4,TRUE,FALSE);
                GPS_time=(time_t)60l;
                break;

            case(120):
                XmToggleButtonSetState(toption5,TRUE,FALSE);
                GPS_time=(time_t)120l;
                break;

            case(300):
                XmToggleButtonSetState(toption6,TRUE,FALSE);
                GPS_time=(time_t)300l;
                break;

            case(600):
                XmToggleButtonSetState(toption7,TRUE,FALSE);
                GPS_time=(time_t)600l;
                break;

            default:
                XmToggleButtonSetState(toption1,TRUE,FALSE);
                GPS_time=30;
                break;
        }

        switch(output_station_type) {
            case(0):
                XmToggleButtonSetState(styp1,TRUE,FALSE);
                Station_transmit_type=0;
                break;

            case(1):
                XmToggleButtonSetState(styp2,TRUE,FALSE);
                Station_transmit_type=1;
                break;

            case(2):
                XmToggleButtonSetState(styp3,TRUE,FALSE);
                Station_transmit_type=2;
                break;

            case(3):
                XmToggleButtonSetState(styp4,TRUE,FALSE);
                Station_transmit_type=3;
                break;

            case(4):
                XmToggleButtonSetState(styp5,TRUE,FALSE);
                Station_transmit_type=4;
                break;

            case(5):
                XmToggleButtonSetState(styp6,TRUE,FALSE);
                Station_transmit_type=5;
                break;

            default:
                XmToggleButtonSetState(styp1,TRUE,FALSE);
                Station_transmit_type=0;
                break;
        }

#ifdef TRANSMIT_RAW_WX
        if (transmit_raw_wx)
            XmToggleButtonSetState(raw_wx_tx,TRUE,FALSE);
        else
            XmToggleButtonSetState(raw_wx_tx,FALSE,FALSE);
#endif

        if(transmit_compressed_objects_items)
            XmToggleButtonSetState(compressed_objects_items_tx,TRUE,FALSE);
        else
            XmToggleButtonSetState(compressed_objects_items_tx,FALSE,FALSE);

        if(pop_up_new_bulletins)
            XmToggleButtonSetState(new_bulletin_popup_enable,TRUE,FALSE);
        else
            XmToggleButtonSetState(new_bulletin_popup_enable,FALSE,FALSE);

        if(view_zero_distance_bulletins)
            XmToggleButtonSetState(zero_bulletin_popup_enable,TRUE,FALSE);
        else
            XmToggleButtonSetState(zero_bulletin_popup_enable,FALSE,FALSE);

        if(warn_about_mouse_modifiers)
            XmToggleButtonSetState(warn_about_mouse_modifiers_enable,TRUE,FALSE);
        else
            XmToggleButtonSetState(warn_about_mouse_modifiers_enable,FALSE,FALSE);

        XmToggleButtonSetState(altnet_active, altnet, FALSE);

        // Known to have memory leaks in some Motif versions:
        //XmTextSetString(altnet_text, altnet_call);
        XmTextReplace(altnet_text, (XmTextPosition) 0,
            XmTextGetLastPosition(altnet_text), altnet_call);

        switch(operate_as_an_igate) {
            case(0):
                XmToggleButtonSetState(igtyp0,TRUE,FALSE);
                Igate_type=0;
                break;

            case(1):
                XmToggleButtonSetState(igtyp1,TRUE,FALSE);
                Igate_type=1;
                break;

            case(2):
                XmToggleButtonSetState(igtyp2,TRUE,FALSE);
                Igate_type=2;
                break;

            default:
                XmToggleButtonSetState(igtyp0,TRUE,FALSE);
                Igate_type=0;
                break;
        }
        pos_dialog(configure_defaults_dialog);

        delw = XmInternAtom(XtDisplay(configure_defaults_dialog),"WM_DELETE_WINDOW", FALSE);
        XmAddWMProtocolCallback(configure_defaults_dialog, delw, Configure_defaults_destroy_shell, (XtPointer)configure_defaults_dialog);

        XtManageChild(my_form);
        XtManageChild(time_box);
        XtManageChild(time_box2);
        XtManageChild(posit_box);
        XtManageChild(time_box3);
        XtManageChild(type_box);
        XtManageChild(igate_box);
        XtManageChild(pane);

        XtPopup(configure_defaults_dialog,XtGrabNone);
        fix_dialog_size(configure_defaults_dialog);

        // Move focus to the Close button.  This appears to highlight the
        // button fine, but we're not able to hit the <Enter> key to
        // have that default function happen.  Note:  We _can_ hit the
        // <SPACE> key, and that activates the option.
//        XmUpdateDisplay(configure_defaults_dialog);
        XmProcessTraversal(button_cancel, XmTRAVERSE_CURRENT);

    } else
        (void)XRaiseWindow(XtDisplay(configure_defaults_dialog), XtWindow(configure_defaults_dialog));
}





///////////////////////////////////   Configure Coordinates Dialog   //////////////////////////////////

void coordinates_toggle( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if (state->set)
        coordinate_system = atoi(which);
    else
        coordinate_system = USE_DDMMMM;

    // Update any active view lists so their coordinates get updated
    Station_List_fill(1,0);     // Update View->Mobile Station list (has lat/lon or UTM info on it)
    // Force redraw
    redraw_on_new_data = 2;
}





void Configure_coordinates_destroy_shell( /*@unused@*/ Widget widget, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    Widget shell = (Widget) clientData;
    XtPopdown(shell);
    XtDestroyWidget(shell);
    configure_coordinates_dialog = (Widget)NULL;
}





void Configure_coordinates( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer callData) {
    static Widget  pane, my_form, button_ok, button_cancel, frame,
                label, coord_box, coord_0, coord_1, coord_2, coord_3;
    Atom delw;
    Arg al[2];                    /* Arg List */
    register unsigned int ac = 0;           /* Arg Count */

    if (!configure_coordinates_dialog) {
        configure_coordinates_dialog = XtVaCreatePopupShell(langcode("WPUPCFC001"),
                xmDialogShellWidgetClass,
                Global.top,
                XmNdeleteResponse,XmDESTROY,
                XmNdefaultPosition, FALSE,
                NULL);

        pane = XtVaCreateWidget("Configure_coordinates pane",
                xmPanedWindowWidgetClass, 
                configure_coordinates_dialog,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        my_form =  XtVaCreateWidget("Configure_coordinates my_form",
                xmFormWidgetClass, 
                pane,
                XmNfractionBase, 5,
                XmNautoUnmanage, FALSE,
                XmNshadowThickness, 1,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);


        // Interval for station being considered old
        frame = XtVaCreateManagedWidget("Configure_coordinates frame", 
                xmFrameWidgetClass, 
                my_form,
                XmNtopAttachment,XmATTACH_FORM,
                XmNtopOffset,10,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 10,
                XmNrightAttachment,XmATTACH_FORM,
                XmNrightOffset, 10,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        label = XtVaCreateManagedWidget(langcode("WPUPCFC002"),
                xmLabelWidgetClass,
                frame,
                XmNchildType, XmFRAME_TITLE_CHILD,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        /*set args for color */
        ac=0;
        XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
        XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;


        coord_box = XmCreateRadioBox(frame,"Configure_coordinates coord_box",
                al,
                ac);

        XtVaSetValues(coord_box,
                XmNpacking, XmPACK_TIGHT,
                XmNorientation, XmHORIZONTAL,
                XmNnumColumns,5,
                NULL);


        coord_0 = XtVaCreateManagedWidget(langcode("WPUPCFC003"),
                xmToggleButtonGadgetClass,
                coord_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(coord_0,XmNvalueChangedCallback,coordinates_toggle,"0");


        coord_1 = XtVaCreateManagedWidget(langcode("WPUPCFC004"),
                xmToggleButtonGadgetClass,
                coord_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(coord_1,XmNvalueChangedCallback,coordinates_toggle,"1");


        coord_2 = XtVaCreateManagedWidget(langcode("WPUPCFC005"),
                xmToggleButtonGadgetClass,
                coord_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(coord_2,XmNvalueChangedCallback,coordinates_toggle,"2");


        coord_3 = XtVaCreateManagedWidget(langcode("WPUPCFC006"),
                xmToggleButtonGadgetClass,
                coord_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(coord_3,XmNvalueChangedCallback,coordinates_toggle,"3");


        button_ok = XtVaCreateManagedWidget(langcode("UNIOP00001"),
                xmPushButtonGadgetClass, 
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, frame,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_FORM,
                XmNbottomOffset, 5,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 1,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 2,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);


        button_cancel = XtVaCreateManagedWidget(langcode("UNIOP00002"),
                xmPushButtonGadgetClass, 
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, frame,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_FORM,
                XmNbottomOffset, 5,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 3,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 4,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        XtAddCallback(button_ok, XmNactivateCallback, Configure_coordinates_destroy_shell, configure_coordinates_dialog);
        XtAddCallback(button_cancel, XmNactivateCallback, Configure_coordinates_destroy_shell, configure_coordinates_dialog);

        // Set the toggle buttons based on current data
        switch (coordinate_system) {
            case(USE_DDDDDD):
                XmToggleButtonSetState(coord_0,TRUE,FALSE);
                break;

            case(USE_DDMMSS):
                XmToggleButtonSetState(coord_2,TRUE,FALSE);
                break;

            case(USE_UTM):
                XmToggleButtonSetState(coord_3,TRUE,FALSE);
                break;

            case(USE_DDMMMM):
            default:
                XmToggleButtonSetState(coord_1,TRUE,FALSE);
                break;
        }

        pos_dialog(configure_coordinates_dialog);

        delw = XmInternAtom(XtDisplay(configure_coordinates_dialog),"WM_DELETE_WINDOW", FALSE);
        XmAddWMProtocolCallback(configure_coordinates_dialog, delw, Configure_coordinates_destroy_shell, (XtPointer)configure_coordinates_dialog);

        XtManageChild(my_form);
        XtManageChild(coord_box);
        XtManageChild(pane);

        XtPopup(configure_coordinates_dialog,XtGrabNone);
        fix_dialog_size(configure_coordinates_dialog);

        // Move focus to the Close button.  This appears to highlight the
        // button fine, but we're not able to hit the <Enter> key to
        // have that default function happen.  Note:  We _can_ hit the
        // <SPACE> key, and that activates the option.
//        XmUpdateDisplay(configure_coordinates_dialog);
        XmProcessTraversal(button_cancel, XmTRAVERSE_CURRENT);

    } else
        (void)XRaiseWindow(XtDisplay(configure_coordinates_dialog), XtWindow(configure_coordinates_dialog));
}






/////////////////////////////////   Configure Audio Alarms Dialog   ////////////////////////////////

void Configure_audio_alarm_destroy_shell( /*@unused@*/ Widget widget, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    Widget shell = (Widget) clientData;
    XtPopdown(shell);
    XtDestroyWidget(shell);
    configure_audio_alarm_dialog = (Widget)NULL;
}





void Configure_audio_alarm_change_data(Widget widget, XtPointer clientData, XtPointer callData) {
    strcpy(sound_command,XmTextFieldGetString(audio_alarm_config_play_data));
    (void)remove_trailing_spaces(sound_command);
    strcpy(sound_new_station,XmTextFieldGetString(audio_alarm_config_play_ons_data));
    (void)remove_trailing_spaces(sound_new_station);
    strcpy(sound_new_message,XmTextFieldGetString(audio_alarm_config_play_onm_data));
    (void)remove_trailing_spaces(sound_new_message);
    strcpy(sound_prox_message,XmTextFieldGetString(audio_alarm_config_play_onpx_data));
    (void)remove_trailing_spaces(sound_prox_message);
    strcpy(prox_min,XmTextFieldGetString(prox_min_data));
    (void)remove_trailing_spaces(prox_min);
    strcpy(prox_max,XmTextFieldGetString(prox_max_data));
    (void)remove_trailing_spaces(prox_max);
    strcpy(sound_band_open_message,XmTextFieldGetString(audio_alarm_config_play_onbo_data));
    (void)remove_trailing_spaces(sound_band_open_message);
    strcpy(bando_min,XmTextFieldGetString(bando_min_data));
    (void)remove_trailing_spaces(bando_min);
    strcpy(bando_max,XmTextFieldGetString(bando_max_data));
    (void)remove_trailing_spaces(bando_max);
    strcpy(sound_wx_alert_message,XmTextFieldGetString(audio_alarm_config_wx_alert_data));
    (void)remove_trailing_spaces(sound_wx_alert_message);

    if(XmToggleButtonGetState(audio_alarm_config_play_on_new_station))
        sound_play_new_station=1;
    else
        sound_play_new_station=0;

    if(XmToggleButtonGetState(audio_alarm_config_play_on_new_message))
        sound_play_new_message=1;
    else
        sound_play_new_message=0;

    if(XmToggleButtonGetState(audio_alarm_config_play_on_prox))
        sound_play_prox_message=1;
    else
        sound_play_prox_message=0;

    if(XmToggleButtonGetState(audio_alarm_config_play_on_bando))
        sound_play_band_open_message=1;
    else
        sound_play_band_open_message=0;

    if(XmToggleButtonGetState(audio_alarm_config_play_on_wx_alert))
        sound_play_wx_alert_message=1;
    else
        sound_play_wx_alert_message=0;

    Configure_audio_alarm_destroy_shell(widget,clientData,callData);
}





void Configure_audio_alarms( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer callData) {
    static Widget  pane, my_form, button_ok, button_cancel,
                audio_play, file1, file2,
                min1, min2, max1, max2,
                minb1, minb2, maxb1, maxb2,
                sep;
    Atom delw;

    if (!configure_audio_alarm_dialog) {
        configure_audio_alarm_dialog = XtVaCreatePopupShell(langcode("WPUPCFA001"),
                xmDialogShellWidgetClass,
                Global.top,
                XmNdeleteResponse,XmDESTROY,
                XmNdefaultPosition, FALSE,
                NULL);

        pane = XtVaCreateWidget("Configure_audio_alarms pane",
                xmPanedWindowWidgetClass, 
                configure_audio_alarm_dialog,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        my_form =  XtVaCreateWidget("Configure_audio_alarms my_form",
                xmFormWidgetClass, 
                pane,
                XmNfractionBase, 5,
                XmNautoUnmanage, FALSE,
                XmNshadowThickness, 1,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        audio_play = XtVaCreateManagedWidget(langcode("WPUPCFA002"),
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
                NULL);

        audio_alarm_config_play_data = XtVaCreateManagedWidget("Configure_audio_alarms Play Command", 
                xmTextFieldWidgetClass, 
                my_form,
                XmNeditable,   TRUE,
                XmNcursorPositionVisible, TRUE,
                XmNsensitive, TRUE,
                XmNshadowThickness,    1,
                XmNcolumns, 25,
                XmNwidth, ((25*7)+2),
                XmNmaxLength, 80,
                XmNbackground, colors[0x0f],
                XmNtopAttachment,XmATTACH_FORM,
                XmNtopOffset, 5,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, audio_play,
                XmNrightAttachment,XmATTACH_FORM,
                XmNrightOffset, 10,
                NULL);


        file1 = XtVaCreateManagedWidget(langcode("WPUPCFA003"),
                xmLabelWidgetClass, 
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, audio_play,
                XmNtopOffset, 20,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        file2 = XtVaCreateManagedWidget(langcode("WPUPCFA004"),
                xmLabelWidgetClass, 
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, audio_play,
                XmNtopOffset, 20,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 2,
                XmNrightAttachment, XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        audio_alarm_config_play_on_new_station = XtVaCreateManagedWidget(langcode("WPUPCFA005"),
                xmToggleButtonWidgetClass,
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, file1,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset ,10,
                XmNrightAttachment, XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        audio_alarm_config_play_ons_data = XtVaCreateManagedWidget("Configure_audio_alarms Play Command NS", 
                xmTextFieldWidgetClass, 
                my_form,
                XmNeditable,   TRUE,
                XmNcursorPositionVisible, TRUE,
                XmNsensitive, TRUE,
                XmNshadowThickness,    1,
                XmNcolumns, 25,
                XmNwidth, ((25*7)+2),
                XmNmaxLength, 80,
                XmNbackground, colors[0x0f],
                XmNtopAttachment,XmATTACH_WIDGET,
                XmNtopWidget, file2,
                XmNtopOffset, 5,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 2,
                XmNleftWidget, audio_alarm_config_play_on_new_station,
                XmNrightAttachment,XmATTACH_NONE,
                NULL);

        audio_alarm_config_play_on_new_message  = XtVaCreateManagedWidget(langcode("WPUPCFA006"),
                xmToggleButtonWidgetClass,
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, audio_alarm_config_play_on_new_station,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 0,
                XmNleftOffset ,10,
                XmNrightAttachment, XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        audio_alarm_config_play_onm_data = XtVaCreateManagedWidget("Configure_audio_alarms Play Command NM", 
                xmTextFieldWidgetClass, 
                my_form,
                XmNeditable,   TRUE,
                XmNcursorPositionVisible, TRUE,
                XmNsensitive, TRUE,
                XmNshadowThickness,    1,
                XmNcolumns, 25,
                XmNwidth, ((25*7)+2),
                XmNmaxLength, 80,
                XmNbackground, colors[0x0f],
                XmNtopAttachment,XmATTACH_WIDGET,
                XmNtopWidget, audio_alarm_config_play_on_new_station,
                XmNtopOffset, 5,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 2,
                XmNrightAttachment,XmATTACH_NONE,
                NULL);

        audio_alarm_config_play_on_prox  = XtVaCreateManagedWidget(langcode("WPUPCFA007"),
                xmToggleButtonWidgetClass,
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, audio_alarm_config_play_on_new_message,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 0,
                XmNleftOffset ,10,
                XmNrightAttachment, XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        audio_alarm_config_play_onpx_data = XtVaCreateManagedWidget("Configure_audio_alarms Play Command PROX", 
                xmTextFieldWidgetClass, 
                my_form,
                XmNeditable,   TRUE,
                XmNcursorPositionVisible, TRUE,
                XmNsensitive, TRUE,
                XmNshadowThickness,    1,
                XmNcolumns, 25,
                XmNwidth, ((25*7)+2),
                XmNmaxLength, 80,
                XmNbackground, colors[0x0f],
                XmNtopAttachment,XmATTACH_WIDGET,
                XmNtopWidget, audio_alarm_config_play_on_new_message,
                XmNtopOffset, 5,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 2,
                XmNrightAttachment,XmATTACH_NONE,
                NULL);

        min1 = XtVaCreateManagedWidget(langcode("WPUPCFA009"),
                xmLabelWidgetClass, 
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, audio_alarm_config_play_on_prox,
                XmNtopOffset, 8,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 0,
                XmNleftOffset, 30,
                XmNrightAttachment, XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        prox_min_data = XtVaCreateManagedWidget("Configure_audio_alarms prox min", 
                xmTextFieldWidgetClass, 
                my_form,
                XmNeditable,   TRUE,
                XmNcursorPositionVisible, TRUE,
                XmNsensitive, TRUE,
                XmNshadowThickness,    1,
                XmNcolumns, 10,
                XmNwidth, ((10*7)+2),
                XmNmaxLength, 20,
                XmNbackground, colors[0x0f],
                XmNtopAttachment,XmATTACH_WIDGET,
                XmNtopWidget, audio_alarm_config_play_onpx_data,
                XmNtopOffset, 2,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 2,
                XmNleftOffset, 20,
                XmNrightAttachment,XmATTACH_NONE,
                NULL);

        min2 = XtVaCreateManagedWidget(units_english_metric?langcode("UNIOP00004"):langcode("UNIOP00005"),
                xmLabelWidgetClass, 
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, audio_alarm_config_play_on_prox,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, prox_min_data,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        max1 = XtVaCreateManagedWidget(langcode("WPUPCFA010"),
                xmLabelWidgetClass, 
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, min1,
                XmNtopOffset, 12,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 0,
                XmNleftOffset, 30,
                XmNrightAttachment, XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        prox_max_data = XtVaCreateManagedWidget("Configure_audio_alarms prox max", 
                xmTextFieldWidgetClass, 
                my_form,
                XmNeditable,   TRUE,
                XmNcursorPositionVisible, TRUE,
                XmNsensitive, TRUE,
                XmNshadowThickness,    1,
                XmNcolumns, 10,
                XmNwidth, ((10*7)+2),
                XmNmaxLength, 20,
                XmNbackground, colors[0x0f],
                XmNtopAttachment,XmATTACH_WIDGET,
                XmNtopWidget, prox_min_data,
                XmNtopOffset, 2,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 2,
                XmNleftOffset, 20,
                XmNrightAttachment,XmATTACH_NONE,
                NULL);

        max2 = XtVaCreateManagedWidget(units_english_metric?langcode("UNIOP00004"):langcode("UNIOP00005"),
                xmLabelWidgetClass, 
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, min1,
                XmNtopOffset, 14,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, prox_max_data,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        audio_alarm_config_play_on_bando  = XtVaCreateManagedWidget(langcode("WPUPCFA008"),
                xmToggleButtonWidgetClass,
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, max1,
                XmNtopOffset, 12,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 0,
                XmNleftOffset ,10,
                XmNrightAttachment, XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        audio_alarm_config_play_onbo_data = XtVaCreateManagedWidget("Configure_audio_alarms Play Command BAND", 
                xmTextFieldWidgetClass, 
                my_form,
                XmNeditable,   TRUE,
                XmNcursorPositionVisible, TRUE,
                XmNsensitive, TRUE,
                XmNshadowThickness,    1,
                XmNcolumns, 25,
                XmNwidth, ((25*7)+2),
                XmNmaxLength, 80,
                XmNbackground, colors[0x0f],
                XmNtopAttachment,XmATTACH_WIDGET,
                XmNtopWidget, prox_max_data,
                XmNtopOffset, 5,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 2,
                XmNrightAttachment,XmATTACH_NONE,
                NULL);

        minb1 = XtVaCreateManagedWidget(langcode("WPUPCFA009"),
                xmLabelWidgetClass, 
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, audio_alarm_config_play_on_bando,
                XmNtopOffset, 12,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 0,
                XmNleftOffset, 30,
                XmNrightAttachment, XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        bando_min_data = XtVaCreateManagedWidget("Configure_audio_alarms bando min", 
                xmTextFieldWidgetClass, 
                my_form,
                XmNeditable,   TRUE,
                XmNcursorPositionVisible, TRUE,
                XmNsensitive, TRUE,
                XmNshadowThickness,    1,
                XmNcolumns, 12,
                XmNwidth, ((10*7)+2),
                XmNmaxLength, 20,
                XmNbackground, colors[0x0f],
                XmNtopAttachment,XmATTACH_WIDGET,
                XmNtopWidget, audio_alarm_config_play_onbo_data,
                XmNtopOffset, 2,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 2,
                XmNleftOffset, 20,
                XmNrightAttachment,XmATTACH_NONE,
                NULL);

        minb2 = XtVaCreateManagedWidget(units_english_metric?langcode("UNIOP00004"):langcode("UNIOP00005"),
                xmLabelWidgetClass, 
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, audio_alarm_config_play_on_bando,
                XmNtopOffset, 14,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, bando_min_data,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        maxb1 = XtVaCreateManagedWidget(langcode("WPUPCFA010"),
                xmLabelWidgetClass, 
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, minb1,
                XmNtopOffset, 12,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 0,
                XmNleftOffset, 30,
                XmNrightAttachment, XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        bando_max_data = XtVaCreateManagedWidget("Configure_audio_alarms bando max", 
                xmTextFieldWidgetClass, 
                my_form,
                XmNeditable,   TRUE,
                XmNcursorPositionVisible, TRUE,
                XmNsensitive, TRUE,
                XmNshadowThickness,    1,
                XmNcolumns, 10,
                XmNwidth, ((10*7)+2),
                XmNmaxLength, 20,
                XmNbackground, colors[0x0f],
                XmNtopAttachment,XmATTACH_WIDGET,
                XmNtopWidget, bando_min_data,
                XmNtopOffset, 2,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 2,
                XmNleftOffset, 20,
                XmNrightAttachment,XmATTACH_NONE,
                NULL);

        maxb2 = XtVaCreateManagedWidget(units_english_metric?langcode("UNIOP00004"):langcode("UNIOP00005"),
                xmLabelWidgetClass, 
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, minb1,
                XmNtopOffset, 14,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, bando_max_data,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        audio_alarm_config_play_on_wx_alert  = XtVaCreateManagedWidget(langcode("WPUPCFA011"),
                xmToggleButtonWidgetClass,
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, maxb2,
                XmNtopOffset, 12,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 0,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        audio_alarm_config_wx_alert_data = XtVaCreateManagedWidget("Configure_audio_alarms Play Command WxAlert", 
                xmTextFieldWidgetClass, 
                my_form,
                XmNeditable, TRUE,
                XmNcursorPositionVisible, TRUE,
                XmNsensitive, TRUE,
                XmNshadowThickness, 1,
                XmNcolumns, 25,
                XmNwidth, ((25*7)+2),
                XmNmaxLength, 80,
                XmNbackground, colors[0x0f],
                XmNtopAttachment,XmATTACH_WIDGET,
                XmNtopWidget, bando_max_data,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 2,
                XmNrightAttachment, XmATTACH_NONE,
                NULL);

        sep = XtVaCreateManagedWidget("Configure_audio_alarms sep", 
                xmSeparatorGadgetClass,
                my_form,
                XmNorientation, XmHORIZONTAL,
                XmNtopAttachment,XmATTACH_WIDGET,
                XmNtopWidget, audio_alarm_config_play_on_wx_alert,
                XmNtopOffset, 20,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNrightAttachment,XmATTACH_FORM,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        button_ok = XtVaCreateManagedWidget(langcode("UNIOP00001"),
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
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        button_cancel = XtVaCreateManagedWidget(langcode("UNIOP00002"),
                xmPushButtonGadgetClass, 
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, sep,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_FORM,
                XmNbottomOffset, 5,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 3,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 4,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        XtAddCallback(button_ok, XmNactivateCallback, Configure_audio_alarm_change_data, configure_audio_alarm_dialog);
        XtAddCallback(button_cancel, XmNactivateCallback, Configure_audio_alarm_destroy_shell, configure_audio_alarm_dialog);

        pos_dialog(configure_audio_alarm_dialog);

        delw = XmInternAtom(XtDisplay(configure_audio_alarm_dialog),"WM_DELETE_WINDOW", FALSE);
        XmAddWMProtocolCallback(configure_audio_alarm_dialog, delw, Configure_audio_alarm_destroy_shell,
                (XtPointer)configure_audio_alarm_dialog);

        XmTextFieldSetString(audio_alarm_config_play_data,sound_command);
        XmTextFieldSetString(audio_alarm_config_play_ons_data,sound_new_station);
        XmTextFieldSetString(audio_alarm_config_play_onm_data,sound_new_message);
        XmTextFieldSetString(audio_alarm_config_play_onpx_data,sound_prox_message);
        XmTextFieldSetString(prox_min_data,prox_min);
        XmTextFieldSetString(prox_max_data,prox_max);
        XmTextFieldSetString(audio_alarm_config_play_onbo_data,sound_band_open_message);
        XmTextFieldSetString(bando_min_data,bando_min);
        XmTextFieldSetString(bando_max_data,bando_max);
        XmTextFieldSetString(audio_alarm_config_wx_alert_data, sound_wx_alert_message);

        if(sound_play_new_station)
            XmToggleButtonSetState(audio_alarm_config_play_on_new_station,TRUE,FALSE);
        else
            XmToggleButtonSetState(audio_alarm_config_play_on_new_station,FALSE,FALSE);

        if(sound_play_new_message)
            XmToggleButtonSetState(audio_alarm_config_play_on_new_message,TRUE,FALSE);
        else
            XmToggleButtonSetState(audio_alarm_config_play_on_new_message,FALSE,FALSE);

        if(sound_play_prox_message)
            XmToggleButtonSetState(audio_alarm_config_play_on_prox,TRUE,FALSE);
        else
            XmToggleButtonSetState(audio_alarm_config_play_on_prox,FALSE,FALSE);

        if(sound_play_band_open_message)
            XmToggleButtonSetState(audio_alarm_config_play_on_bando,TRUE,FALSE);
        else
            XmToggleButtonSetState(audio_alarm_config_play_on_bando,FALSE,FALSE);

        if (sound_play_wx_alert_message)
            XmToggleButtonSetState(audio_alarm_config_play_on_wx_alert, TRUE, FALSE);
        else
            XmToggleButtonSetState(audio_alarm_config_play_on_wx_alert, FALSE, FALSE);

        XtManageChild(my_form);
        XtManageChild(pane);

        XtPopup(configure_audio_alarm_dialog,XtGrabNone);
        fix_dialog_size(configure_audio_alarm_dialog);

        // Move focus to the Cancel button.  This appears to highlight the
        // button fine, but we're not able to hit the <Enter> key to
        // have that default function happen.  Note:  We _can_ hit the
        // <SPACE> key, and that activates the option.
//        XmUpdateDisplay(configure_audio_alarm_dialog);
        XmProcessTraversal(button_cancel, XmTRAVERSE_CURRENT);

    } else
        (void)XRaiseWindow(XtDisplay(configure_audio_alarm_dialog), XtWindow(configure_audio_alarm_dialog));

}





/////////////////////////////////////   Configure Speech Dialog   //////////////////////////////////


void Configure_speech_destroy_shell( /*@unused@*/ Widget widget, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    Widget shell = (Widget) clientData;
    XtPopdown(shell);
    XtDestroyWidget(shell);
    configure_speech_dialog = (Widget)NULL;
}


void Configure_speech_change_data(Widget widget, XtPointer clientData, XtPointer callData) {

    if(XmToggleButtonGetState(speech_config_play_on_new_station))
        festival_speak_new_station=1;
    else
        festival_speak_new_station=0;

    if(XmToggleButtonGetState(speech_config_play_on_new_message_alert))
        festival_speak_new_message_alert=1;
    else
        festival_speak_new_message_alert=0;

    if(XmToggleButtonGetState(speech_config_play_on_new_message_body))
        festival_speak_new_message_body=1;
    else
        festival_speak_new_message_body=0;

    if(XmToggleButtonGetState(speech_config_play_on_prox))
        festival_speak_proximity_alert=1;
    else
        festival_speak_proximity_alert=0;

    if(XmToggleButtonGetState(speech_config_play_on_trak))
        festival_speak_tracked_proximity_alert=1;
    else
        festival_speak_tracked_proximity_alert=0;

    if(XmToggleButtonGetState(speech_config_play_on_bando))
        festival_speak_band_opening=1;
    else
        festival_speak_band_opening=0;

    if(XmToggleButtonGetState(speech_config_play_on_new_wx_alert))
        festival_speak_new_weather_alert=1;
    else
        festival_speak_new_weather_alert=0;

    Configure_speech_destroy_shell(widget,clientData,callData);
}



//Make it helpful - Gray the config selections, but add a choice
//that basicly pops up a box that says where to get Festival, have
//it be ungrayed if Festival isn't installed.

void Configure_speech( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer callData) {
    static Widget  pane, my_form, button_ok, button_cancel, file1, sep;
    Atom delw;

    if (!configure_speech_dialog) {
        configure_speech_dialog = XtVaCreatePopupShell(langcode("WPUPCFSP01"),
                xmDialogShellWidgetClass,
                Global.top,
                XmNdeleteResponse,XmDESTROY,
                XmNdefaultPosition, FALSE,
                NULL);

        pane = XtVaCreateWidget("Configure_speech pane",
                xmPanedWindowWidgetClass, 
                configure_speech_dialog,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        my_form =  XtVaCreateWidget("Configure_speech my_form",
                xmFormWidgetClass, 
                pane,
                XmNfractionBase, 5,
                XmNautoUnmanage, FALSE,
                XmNshadowThickness, 1,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        file1 = XtVaCreateManagedWidget(langcode("WPUPCFSP02"),
                xmLabelWidgetClass, 
                my_form,
                XmNtopAttachment, XmATTACH_FORM,
                XmNtopOffset, 20,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        speech_config_play_on_new_station  = XtVaCreateManagedWidget(langcode("WPUPCFSP03"),
                xmToggleButtonWidgetClass,
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, file1,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset ,10,
                XmNrightAttachment, XmATTACH_NONE,
#ifndef HAVE_FESTIVAL
                XmNsensitive, FALSE,
#endif /* HAVE_FESTIVAL */
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        speech_config_play_on_new_message_alert  = XtVaCreateManagedWidget(langcode("WPUPCFSP04"),
                xmToggleButtonWidgetClass,
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, speech_config_play_on_new_station,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 0,
                XmNleftOffset ,10,
                XmNrightAttachment, XmATTACH_NONE,
#ifndef HAVE_FESTIVAL
                XmNsensitive, FALSE,
#endif /* HAVE_FESTIVAL */
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        speech_config_play_on_new_message_body  = XtVaCreateManagedWidget(langcode("WPUPCFSP05"),
                xmToggleButtonWidgetClass,
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, speech_config_play_on_new_message_alert,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 0,
                XmNleftOffset ,10,
                XmNrightAttachment, XmATTACH_NONE,
#ifndef HAVE_FESTIVAL
                XmNsensitive, FALSE,
#endif /* HAVE_FESTIVAL */
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        speech_config_play_on_prox  = XtVaCreateManagedWidget(langcode("WPUPCFSP06"),
                xmToggleButtonWidgetClass,
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, speech_config_play_on_new_message_body,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 0,
                XmNleftOffset ,10,
                XmNrightAttachment, XmATTACH_NONE,
#ifndef HAVE_FESTIVAL
                XmNsensitive, FALSE,
#endif /* HAVE_FESTIVAL */
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        speech_config_play_on_trak  = XtVaCreateManagedWidget(langcode("WPUPCFSP09"),
                xmToggleButtonWidgetClass,
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, speech_config_play_on_prox,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 0,
                XmNleftOffset ,10,
                XmNrightAttachment, XmATTACH_NONE,
#ifndef HAVE_FESTIVAL
                XmNsensitive, FALSE,
#endif /* HAVE_FESTIVAL */
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        speech_config_play_on_bando  = XtVaCreateManagedWidget(langcode("WPUPCFSP07"),
                xmToggleButtonWidgetClass,
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, speech_config_play_on_trak,
                XmNtopOffset, 12,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 0,
                XmNleftOffset ,10,
                XmNrightAttachment, XmATTACH_NONE,
#ifndef HAVE_FESTIVAL
                XmNsensitive, FALSE,
#endif /* HAVE_FESTIVAL */
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        speech_config_play_on_new_wx_alert  = XtVaCreateManagedWidget(langcode("WPUPCFSP08"),
                xmToggleButtonWidgetClass,
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, speech_config_play_on_bando,
                XmNtopOffset, 12,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 0,
                XmNleftOffset ,10,
                XmNrightAttachment, XmATTACH_NONE,
#ifndef HAVE_FESTIVAL
                XmNsensitive, FALSE,
#endif /* HAVE_FESTIVAL */
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);


        sep = XtVaCreateManagedWidget("Configure_speech sep", 
                xmSeparatorGadgetClass,
                my_form,
                XmNorientation, XmHORIZONTAL,
                XmNtopAttachment,XmATTACH_WIDGET,
                XmNtopWidget, speech_config_play_on_new_wx_alert,
                XmNtopOffset, 20,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNrightAttachment,XmATTACH_FORM,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        button_ok = XtVaCreateManagedWidget(langcode("UNIOP00001"),
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
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        button_cancel = XtVaCreateManagedWidget(langcode("UNIOP00002"),
                xmPushButtonGadgetClass, 
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, sep,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_FORM,
                XmNbottomOffset, 5,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 3,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 4,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        XtAddCallback(button_ok, XmNactivateCallback, Configure_speech_change_data, configure_speech_dialog);
        XtAddCallback(button_cancel, XmNactivateCallback, Configure_speech_destroy_shell, configure_speech_dialog);

        pos_dialog(configure_speech_dialog);

        delw = XmInternAtom(XtDisplay(configure_speech_dialog),"WM_DELETE_WINDOW", FALSE);
        XmAddWMProtocolCallback(configure_speech_dialog, delw, Configure_speech_destroy_shell,
                (XtPointer)configure_speech_dialog);


        if(festival_speak_new_station)
            XmToggleButtonSetState(speech_config_play_on_new_station,TRUE,FALSE);
        else
            XmToggleButtonSetState(speech_config_play_on_new_station,FALSE,FALSE);

        if(festival_speak_new_message_alert)
            XmToggleButtonSetState(speech_config_play_on_new_message_alert,TRUE,FALSE);
        else
            XmToggleButtonSetState(speech_config_play_on_new_message_alert,FALSE,FALSE);

        if(festival_speak_new_message_body)
            XmToggleButtonSetState(speech_config_play_on_new_message_body,TRUE,FALSE);
        else
            XmToggleButtonSetState(speech_config_play_on_new_message_body,FALSE,FALSE);

        if(festival_speak_proximity_alert)
            XmToggleButtonSetState(speech_config_play_on_prox,TRUE,FALSE);
        else
            XmToggleButtonSetState(speech_config_play_on_prox,FALSE,FALSE);

        if(festival_speak_tracked_proximity_alert)
            XmToggleButtonSetState(speech_config_play_on_trak,TRUE,FALSE);
        else
            XmToggleButtonSetState(speech_config_play_on_trak,FALSE,FALSE);

        if(festival_speak_band_opening)
            XmToggleButtonSetState(speech_config_play_on_bando,TRUE,FALSE);
        else
            XmToggleButtonSetState(speech_config_play_on_bando,FALSE,FALSE);

        if(festival_speak_new_weather_alert)
            XmToggleButtonSetState(speech_config_play_on_new_wx_alert,TRUE,FALSE);
        else
            XmToggleButtonSetState(speech_config_play_on_new_wx_alert,FALSE,FALSE);

        XtManageChild(my_form);
        XtManageChild(pane);

        XtPopup(configure_speech_dialog,XtGrabNone);
        fix_dialog_size(configure_speech_dialog);

        // Move focus to the Cancel button.  This appears to highlight the
        // button fine, but we're not able to hit the <Enter> key to
        // have that default function happen.  Note:  We _can_ hit the
        // <SPACE> key, and that activates the option.
//        XmUpdateDisplay(configure_speech_dialog);
        XmProcessTraversal(button_cancel, XmTRAVERSE_CURRENT);

    } else
        (void)XRaiseWindow(XtDisplay(configure_speech_dialog), XtWindow(configure_speech_dialog));

}





/*
 *  Track_Me
 *
 */
void Track_Me( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set) {
        strcpy(tracking_station_call, my_callsign);
        track_me = atoi(which);
        track_station_on = atoi(which);
        display_zoom_status();
    }
    else {
        track_me = 0;
        track_station_on = 0;
        display_zoom_status();
    }
}





/*
 *  Measure_Distance
 *
 */
void  Measure_Distance( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set) {
        measuring_distance = atoi(which);
        XmToggleButtonSetState(move_button, FALSE, FALSE);
        moving_object = 0;
    }
    else {
        measuring_distance = 0;
    }
}





/*
 *  Move_Object
 *
 */
void  Move_Object( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set) {
        moving_object = atoi(which);
        XmToggleButtonSetState(measure_button, FALSE, FALSE);
        measuring_distance = 0;
    }
    else {
        moving_object = 0;
    }
}





/////////////////////////////////////////   Object Dialog   //////////////////////////////////////////


/*
 *  Destroy Object Dialog Popup Window
 */
void Object_destroy_shell( /*@unused@*/ Widget widget, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    Widget shell = (Widget) clientData;
    XtPopdown(shell);
    (void)XFreePixmap(XtDisplay(appshell),Ob_icon0);
    (void)XFreePixmap(XtDisplay(appshell),Ob_icon);
    XtDestroyWidget(shell);
    object_dialog = (Widget)NULL;

    // Take down the symbol selection dialog as well (if it's up)
    if (select_symbol_dialog) {
        Select_symbol_destroy_shell( widget, select_symbol_dialog, callData);
    }

    // NULL out the dialog field in the global struct used for
    // Coordinate Calculator.  Prevents segfaults if the calculator is
    // still up and trying to write to us.
    coordinate_calc_array.calling_dialog = NULL;
}





// Convert this eventually to populate a DataRow struct, then call
// data.c:Create_object_item_tx_string().  Right now we have a lot
// of code duplication between Setup_object_data, Setup_item_data,
// and Create_object_item_tx_string.
//
// Make sure to look at the "transmit_compressed_objects_items" variable
// to decide whether to send a compressed packet.
/*
 *  Setup APRS Information Field for Objects
 */
int Setup_object_data(char *line, int line_length) {
    char lat_str[MAX_LAT];
    char lon_str[MAX_LONG];
    char ext_lat_str[20];
    char ext_lon_str[20];
    char comment[43+1];                 // max 43 characters of comment
    char time[7+1];
    struct tm *day_time;
    time_t sec;
    char complete_area_color[3];
    int complete_area_type;
    int lat_offset, lon_offset;
    char complete_corridor[6];
    char altitude[10];
    char speed_course[8];
    int speed;
    int course;
    int temp;
    long temp2;
    float temp3;
    char signpost[6];
    int bearing;


    //printf("Setup_object_data\n");

    xastir_snprintf(line, line_length, "%s", XmTextFieldGetString(object_name_data));
    (void)remove_trailing_spaces(line);
    //(void)to_upper(line);      Not per spec.  Don't use this.
    if (!valid_object(line))
        return(0);

    strcpy(last_object,line);

    strcpy(line,XmTextFieldGetString(object_lat_data_ns));
    if((char)toupper((int)line[0]) == 'S')
        line[0] = 'S';
    else
        line[0] = 'N';

    // Check latitude for out-of-bounds
    temp = atoi(XmTextFieldGetString(object_lat_data_deg));
    if ( (temp > 90) || (temp < 0) )
        return(0);
    temp3 = atof(XmTextFieldGetString(object_lat_data_min));
    if ( (temp3 >= 60.0) || (temp3 < 0.0) )
        return(0);
    if ( (temp == 90) && (temp3 != 0.0) )
        return(0);

    xastir_snprintf(lat_str, sizeof(lat_str), "%02d%05.2f%c",
        atoi(XmTextFieldGetString(object_lat_data_deg)),
        atof(XmTextFieldGetString(object_lat_data_min)), line[0]);

    xastir_snprintf(ext_lat_str, sizeof(ext_lat_str), "%02d%05.3f%c",
        atoi(XmTextFieldGetString(object_lat_data_deg)),
        atof(XmTextFieldGetString(object_lat_data_min)), line[0]);

    strcpy(line,XmTextFieldGetString(object_lon_data_ew));
    if((char)toupper((int)line[0]) == 'E')
        line[0] = 'E';
    else
        line[0] = 'W';

    // Check longitude for out-of-bounds
    temp = atoi(XmTextFieldGetString(object_lon_data_deg));
    if ( (temp > 180) || (temp < 0) )
        return(0);

    temp3 = atof(XmTextFieldGetString(object_lon_data_min));
    if ( (temp3 >= 60.0) || (temp3 < 0.0) )
        return(0);

    if ( (temp == 180) && (temp3 != 0.0) )
        return(0);

    xastir_snprintf(lon_str, sizeof(lon_str), "%03d%05.2f%c",
        atoi(XmTextFieldGetString(object_lon_data_deg)),
        atof(XmTextFieldGetString(object_lon_data_min)), line[0]);

    xastir_snprintf(ext_lon_str, sizeof(ext_lon_str), "%03d%05.3f%c",
        atoi(XmTextFieldGetString(object_lon_data_deg)),
        atof(XmTextFieldGetString(object_lon_data_min)), line[0]);

    strcpy(line,XmTextFieldGetString(object_group_data));
    last_obj_grp = line[0];
    if(isalpha((int)last_obj_grp))
        last_obj_grp = toupper((int)line[0]);          // todo: toupper in dialog

    strcpy(line,XmTextFieldGetString(object_symbol_data));
    last_obj_sym = line[0];

    strcpy(comment,XmTextFieldGetString(object_comment_data));
    (void)remove_trailing_spaces(comment);
    //printf("Comment Field was: %s\n",comment);

    sec = sec_now();
    day_time = gmtime(&sec);
    xastir_snprintf(time, sizeof(time), "%02d%02d%02dz", day_time->tm_mday, day_time->tm_hour,
            day_time->tm_min);

    // Handle Generic Options

    // Speed/Course Fields
    xastir_snprintf(line, line_length, "%s", XmTextFieldGetString(ob_course_data));
    xastir_snprintf(speed_course, sizeof(speed_course), "%s", ".../");
    course = 0;
    if (strlen(line) != 0) {    // Course was entered
        // Need to check for 1 to three digits only, and 001-360 degrees)
        temp = atoi(line);
        if ( (temp >= 1) && (temp <= 360) ) {
            xastir_snprintf(speed_course, sizeof(speed_course), "%03d/", temp);
            course = temp;
        } else if (temp == 0) {   // Spec says 001 to 360 degrees...
            xastir_snprintf(speed_course, sizeof(speed_course), "%s", "360/");
        }
    }
    xastir_snprintf(line, line_length, "%s", XmTextFieldGetString(ob_speed_data));
    speed = 0;
    if (strlen(line) != 0) { // Speed was entered (we only handle knots currently)
        // Need to check for 1 to three digits, no alpha characters
        temp = atoi(line);
        if ( (temp >= 0) && (temp <= 999) ) {
            xastir_snprintf(line, line_length, "%03d", temp);
            strcat(speed_course,line);
            speed = temp;
        }
        else {
            strcat(speed_course,"...");
        }
    }
    else {  // No speed entered, blank it out
        strcat(speed_course,"...");
    }
    if ( (speed_course[0] == '.') && (speed_course[4] == '.') ) {
        speed_course[0] = '\0'; // No speed or course entered, so blank it
    }
    if (Area_object_enabled) {
        speed_course[0] = '\0'; // Course/Speed not allowed if Area Object
    }

    // Altitude Field
    xastir_snprintf(line, line_length, "%s", XmTextFieldGetString(ob_altitude_data));
    //printf("Altitude entered: %s\n", line);
    altitude[0] = '\0'; // Start with empty string
    if (strlen(line) != 0) {   // Altitude was entered (we only handle feet currently)
        // Need to check for all digits, and 1 to 6 digits
        if (isdigit((int)line[0])) {
            temp2 = atoi(line);
            if ( (temp2 >= 0) && (temp2 <= 999999) ) {
                xastir_snprintf(altitude, sizeof(altitude), "/A=%06ld", temp2);
            }
        }
    }

    // Handle Specific Options

    // Area Objects
    if (Area_object_enabled) {
        //printf("Area_bright: %d\n", Area_bright);
        //printf("Area_filled: %d\n", Area_filled);
       if (Area_bright) {  // Bright color
            xastir_snprintf(complete_area_color, sizeof(complete_area_color), "%2s", Area_color);
        } else {              // Dim color
            xastir_snprintf(complete_area_color, sizeof(complete_area_color), "%02d",
                    atoi(&Area_color[1]) + 8);

            if ( (Area_color[1] == '0') || (Area_color[1] == '1') ) {
                complete_area_color[0] = '/';
            }
        }
        if ( (Area_filled) && (Area_type != 1) && (Area_type != 6) ) {
            complete_area_type = Area_type + 5;
        } else {  // Can't fill in a line
            complete_area_type = Area_type;
        }
        xastir_snprintf(line, line_length, "%s", XmTextFieldGetString(ob_lat_offset_data));
        lat_offset = (int)sqrt(atof(line));
        if (lat_offset > 99)
            lat_offset = 99;
        //printf("Line: %s\tlat_offset: %d\n", line, lat_offset);
        xastir_snprintf(line, line_length, "%s", XmTextFieldGetString(ob_lon_offset_data));
        lon_offset = (int)sqrt(atof(line));
        if (lon_offset > 99)
            lon_offset = 99;
        //printf("Line: %s\tlon_offset: %d\n", line, lon_offset);

        //printf("Corridor Field: %s\n", XmTextFieldGetString(ob_corridor_data) );
        // Corridor
        complete_corridor[0] = '\0';
        if ( (Area_type == 1) || (Area_type == 6) ) {
            xastir_snprintf(line, line_length, "%s", XmTextFieldGetString(ob_corridor_data));
            if (strlen(line) != 0) {    // We have a line and some corridor data
                // Need to check for 1 to three digits only
                temp = atoi(line);
                if ( (temp > 0) && (temp <= 999) ) {
                    xastir_snprintf(complete_corridor, sizeof(complete_corridor), "{%d}", temp);
                    //printf("%s\n",complete_corridor);
                }
            }
        }

        //printf("Complete_corridor: %s\n", complete_corridor);

        xastir_snprintf(line, line_length, ";%-9s*%s%s%c%s%c%1d%02d%2s%02d%s%s%s",
            last_object,
            time,
            lat_str,
            last_obj_grp,
            lon_str,
            last_obj_sym,
            complete_area_type,
            lat_offset,
            complete_area_color,
            lon_offset,
            speed_course,
            complete_corridor,
            altitude);

        //printf("String is: %s\n", line);
    } else if (Signpost_object_enabled) {
        xastir_snprintf(line, line_length, "%s", XmTextFieldGetString(signpost_data));
        //printf("Signpost entered: %s\n", line);
        if (strlen(line) != 0) {   // Signpost data was entered
            // Need to check for all digits, and 1 to 3 digits
            temp = atoi(line);
            if ( (temp >= 0) && (temp <= 999) ) {
                xastir_snprintf(signpost, sizeof(signpost), "{%d}", temp);
            } else {
                signpost[0] = '\0';
            }
        } else {  // No signpost data entered, blank it out
            signpost[0] = '\0';
        }
        xastir_snprintf(line, line_length, ";%-9s*%s%s%c%s%c%s%s%s",
            last_object,
            time,
            lat_str,
            last_obj_grp,
            lon_str,
            last_obj_sym,
            speed_course,
            altitude,
            signpost);
    } else if (DF_object_enabled) {   // A DF'ing object of some type
        if (Omni_antenna_enabled) {
            xastir_snprintf(line, line_length, ";%-9s*%s%s%c%s%cDFS%s/%s%s",
                last_object,
                time,
                lat_str,
                last_obj_grp,
                lon_str,
                last_obj_sym,
                object_shgd,
                speed_course,
                altitude);
        } else {  // Beam Heading DFS object
            if (strlen(speed_course) != 7)
                strcpy(speed_course,"000/000");

            bearing = atoi(XmTextFieldGetString(ob_bearing_data));
            if ( (bearing < 1) || (bearing > 360) )
                bearing = 360;

            xastir_snprintf(line, line_length, ";%-9s*%s%s%c%s%c%s/%03i/%s%s",
                last_object,
                time,
                lat_str,
                last_obj_grp,
                lon_str,
                last_obj_sym,
                speed_course,
                bearing,
                object_NRQ,
                altitude);
        }
    } else {  // Else it's a normal object
        if (transmit_compressed_objects_items) {

// Need to compute "csT" at some point and add it to the object
            xastir_snprintf(line, line_length, ";%-9s*%s%s",
                last_object,
                time,
                compress_posit(ext_lat_str,
                    last_obj_grp,
                    ext_lon_str,
                    last_obj_sym,
                    course,
                    speed,  // In knots
                    ""));    // PHG, must be blank in this case
        }
        else {
            xastir_snprintf(line, line_length, ";%-9s*%s%s%c%s%c%s%s",
                last_object,
                time,
                lat_str,
                last_obj_grp,
                lon_str,
                last_obj_sym,
                speed_course,
                altitude);
        }
    }

    // We need to tack the comment on the end, but need to make
    // sure we don't go over the maximum length for an object.
    //printf("Comment: %s\n",comment);
    if (strlen(comment) != 0) {
        temp = 0;
        while ( (strlen(line) < 80) && (temp < (int)strlen(comment)) ) {
            //printf("temp: %d->%d\t%c\n", temp, strlen(line), comment[temp]);
            line[strlen(line) + 1] = '\0';
            line[strlen(line)] = comment[temp++];
        }
    }
    //printf("line: %s\n",line);

// NOTE:  Compressed mode will be shorter still.  Account
// for that when compressed mode is implemented for objects.

    return(1);
}





// Convert this eventually to populate a DataRow struct, then call
// data.c:Create_object_item_tx_string().  Right now we have a lot
// of code duplication between Setup_object_data, Setup_item_data,
// and Create_object_item_tx_string.
//
// Make sure to look at the "transmit_compressed_objects_items" variable
// to decide whether to send a compressed packet.
/*
 *  Setup APRS Information Field for Items
 */
int Setup_item_data(char *line, int line_length) {
    char lat_str[MAX_LAT];
    char lon_str[MAX_LONG];
    char ext_lat_str[20];
    char ext_lon_str[20];
    char comment[43+1];                 // max 43 characters of comment
    char complete_area_color[3];
    int complete_area_type;
    int lat_offset, lon_offset;
    char complete_corridor[6];
    char altitude[10];
    char speed_course[8];
    int speed;
    int course;
    int temp;
    long temp2;
    float temp3;
    char tempstr[MAX_CALLSIGN+1];
    char signpost[6];
    int bearing;


    xastir_snprintf(line, line_length, "%s", XmTextFieldGetString(object_name_data));
    (void)remove_trailing_spaces(line);
    //(void)to_upper(line);     Not per spec.  Don't use this.

    strcpy(tempstr,line);

    if (strlen(line) == 1)  // Add two spaces (to make 3 minimum chars)
        xastir_snprintf(line, line_length, "%s  ", tempstr);

    else if (strlen(line) == 2) // Add one space (to make 3 minimum chars)
        xastir_snprintf(line, line_length, "%s ", tempstr);

    if (!valid_item(line))
        return(0);

    strcpy(last_object,line);

    strcpy(line,XmTextFieldGetString(object_lat_data_ns));
    if((char)toupper((int)line[0]) == 'S')
        line[0] = 'S';
    else
        line[0] = 'N';

    // Check latitude for out-of-bounds
    temp = atoi(XmTextFieldGetString(object_lat_data_deg));
    if ( (temp > 90) || (temp < 0) )
        return(0);

    temp3 = atof(XmTextFieldGetString(object_lat_data_min));
    if ( (temp3 >= 60.0) || (temp3 < 0.0) )
        return(0);

    if ( (temp == 90) && (temp3 != 0.0) )
        return(0);

    xastir_snprintf(lat_str, sizeof(lat_str), "%02d%05.2f%c",
            atoi(XmTextFieldGetString(object_lat_data_deg)),
            atof(XmTextFieldGetString(object_lat_data_min)),line[0]);

    xastir_snprintf(ext_lat_str, sizeof(ext_lat_str), "%02d%05.3f%c",
            atoi(XmTextFieldGetString(object_lat_data_deg)),
            atof(XmTextFieldGetString(object_lat_data_min)),line[0]);

    strcpy(line,XmTextFieldGetString(object_lon_data_ew));
    if((char)toupper((int)line[0]) == 'E')
        line[0] = 'E';
    else
        line[0] = 'W';

    // Check longitude for out-of-bounds
    temp = atoi(XmTextFieldGetString(object_lon_data_deg));
    if ( (temp > 180) || (temp < 0) )
        return(0);

    temp3 = atof(XmTextFieldGetString(object_lon_data_min));

    if ( (temp3 >= 60.0) || (temp3 < 0.0) )
        return(0);

    if ( (temp == 180) && (temp3 != 0.0) )
        return(0);

    xastir_snprintf(lon_str, sizeof(lon_str), "%03d%05.2f%c",
            atoi(XmTextFieldGetString(object_lon_data_deg)),
            atof(XmTextFieldGetString(object_lon_data_min)),line[0]);

    xastir_snprintf(ext_lon_str, sizeof(ext_lon_str), "%03d%05.3f%c",
            atoi(XmTextFieldGetString(object_lon_data_deg)),
            atof(XmTextFieldGetString(object_lon_data_min)),line[0]);

    strcpy(line,XmTextFieldGetString(object_group_data));
    last_obj_grp = line[0];
    if(isalpha((int)last_obj_grp))
        last_obj_grp = toupper((int)line[0]);          // todo: toupper in dialog

    strcpy(line,XmTextFieldGetString(object_symbol_data));
    last_obj_sym = line[0];

    strcpy(comment,XmTextFieldGetString(object_comment_data));
    (void)remove_trailing_spaces(comment);

    // Handle Generic Options

    // Speed/Course Fields
    xastir_snprintf(line, line_length, "%s", XmTextFieldGetString(ob_course_data));
    sprintf(speed_course,".../");   // Start with invalid-data string
    course = 0;
    if (strlen(line) != 0) {    // Course was entered
        // Need to check for 1 to three digits only, and 001-360 degrees)
        temp = atoi(line);
        if ( (temp >= 1) && (temp <= 360) ) {
            xastir_snprintf(speed_course, sizeof(speed_course), "%03d/", temp);
            course = temp;
        } else if (temp == 0) {   // Spec says 001 to 360 degrees...
            sprintf(speed_course, "360/");
        }
    }
    xastir_snprintf(line, line_length, "%s", XmTextFieldGetString(ob_speed_data));
    speed = 0;
    if (strlen(line) != 0) { // Speed was entered (we only handle knots currently)
        // Need to check for 1 to three digits, no alpha characters
        temp = atoi(line);
        if ( (temp >= 0) && (temp <= 999) ) {
            xastir_snprintf(line, line_length, "%03d", temp);
            strcat(speed_course,line);
            speed = temp;
        } else {
            strcat(speed_course,"...");
        }
    } else {  // No speed entered, blank it out
        strcat(speed_course,"...");
    }
    if ( (speed_course[0] == '.') && (speed_course[4] == '.') ) {
        speed_course[0] = '\0'; // No speed or course entered, so blank it
    }

    if (Area_object_enabled) {
        speed_course[0] = '\0'; // Course/Speed not allowed if Area Object
    }

    // Altitude Field
    xastir_snprintf(line, line_length, "%s", XmTextFieldGetString(ob_altitude_data));
    //printf("Altitude entered: %s\n", line);
    altitude[0] = '\0'; // Start with empty string
    if (strlen(line) != 0) {   // Altitude was entered (we only handle feet currently)
        // Need to check for all digits, and 1 to 6 digits
        if (isdigit((int)line[0])) {
            temp2 = atoi(line);
            if ((temp2 >= 0) && (temp2 <= 999999)) {
                xastir_snprintf(altitude, sizeof(altitude), "/A=%06ld",temp2);
            }
        }
    }

    // Handle Specific Options

    // Area Items
    if (Area_object_enabled) {
        if (Area_bright) {  // Bright color
            xastir_snprintf(complete_area_color, sizeof(complete_area_color), "%2s",
                    Area_color);
        } else {              // Dim color
            xastir_snprintf(complete_area_color, sizeof(complete_area_color), "%02d",
                    atoi(&Area_color[1]) + 8);
            if ((Area_color[1] == '0') || (Area_color[1] == '1')) {
                complete_area_color[0] = '/';
            }
        }
        if ( (Area_filled) && (Area_type != 1) && (Area_type != 6) ) {
            complete_area_type = Area_type + 5;
        } else {  // Can't fill in a line
            complete_area_type = Area_type;
        }
        xastir_snprintf(line, line_length, "%s", XmTextFieldGetString(ob_lat_offset_data));
        lat_offset = (int)sqrt(atof(line));
        if (lat_offset > 99)
            lat_offset = 99;
        //printf("Line: %s\tlat_offset: %d\n", line, lat_offset);
        xastir_snprintf(line, line_length, "%s", XmTextFieldGetString(ob_lon_offset_data));
        lon_offset = (int)sqrt(atof(line));
        if (lon_offset > 99)
            lon_offset = 99;
        //printf("Line: %s\tlon_offset: %d\n", line, lon_offset);

        // Corridor
        complete_corridor[0] = '\0';
        if ( (Area_type == 1) || (Area_type == 6) ) {
            xastir_snprintf(line, line_length, "%s", XmTextFieldGetString(ob_corridor_data));
            if (strlen(line) != 0) {    // We have a line and some corridor data
                // Need to check for 1 to three digits only
                temp = atoi(line);
                if ( (temp > 0) && (temp <= 999) ) {
                    xastir_snprintf(complete_corridor, sizeof(complete_corridor), "{%d}", temp);
                    //printf("%s\n",complete_corridor);
                }
            }
        }
        xastir_snprintf(line, line_length, ")%s!%s%c%s%c%1d%02d%2s%02d%s%s%s",
            last_object,
            lat_str,
            last_obj_grp,
            lon_str,
            last_obj_sym,
            complete_area_type,
            lat_offset,
            complete_area_color,
            lon_offset,
            speed_course,
            complete_corridor,
            altitude);
    } else if (Signpost_object_enabled) {
        xastir_snprintf(line, line_length, "%s", XmTextFieldGetString(signpost_data));
        //printf("Signpost entered: %s\n", line);
        if (strlen(line) != 0) {   // Signpost data was entered
            // Need to check for all digits, and 1 to 3 digits
            temp = atoi(line);
            if ( (temp >= 0) && (temp <= 999) ) {
                xastir_snprintf(signpost, sizeof(signpost), "{%d}", temp);
            } else {
                signpost[0] = '\0';
            }
        } else {  // No signpost data entered, blank it out
            signpost[0] = '\0';
        }
        xastir_snprintf(line, line_length, ")%s!%s%c%s%c%s%s%s",
            last_object,
            lat_str,
            last_obj_grp,
            lon_str,
            last_obj_sym,
            speed_course,
            altitude,
            signpost);
    } else if (DF_object_enabled) {   // A DF'ing item of some type
        if (Omni_antenna_enabled) {
            xastir_snprintf(line, line_length, ")%s!%s%c%s%cDFS%s/%s%s",
                last_object,
                lat_str,
                last_obj_grp,
                lon_str,
                last_obj_sym,
                object_shgd,
                speed_course,
                altitude);
        } else {  // Beam Heading DFS item
            if (strlen(speed_course) != 7)
                strcpy(speed_course,"000/000");

            bearing = atoi(XmTextFieldGetString(ob_bearing_data));
            if ( (bearing < 1) || (bearing > 360) )
                bearing = 360;

            xastir_snprintf(line, line_length, ")%s!%s%c%s%c%s/%03i/%s%s",
                last_object,
                lat_str,
                last_obj_grp,
                lon_str,
                last_obj_sym,
                speed_course,
                bearing,
                object_NRQ,
                altitude);
        }
    } else {  // Else it's a normal item
        if (transmit_compressed_objects_items) {

// Need to compute "csT" at some point and add it to the item
            xastir_snprintf(line, line_length, ")%s!%s",
                last_object,
                compress_posit(ext_lat_str,
                    last_obj_grp,
                    ext_lon_str,
                    last_obj_sym,
                    course,
                    speed,  // In knots
                    ""));    // PHG, must be blank in this case
        }
        else {
            xastir_snprintf(line, line_length, ")%s!%s%c%s%c%s%s",
                last_object,
                lat_str,
                last_obj_grp,
                lon_str,
                last_obj_sym,
                speed_course,
                altitude);
        }
    }

    // We need to tack the comment on the end, but need to make
    // sure we don't go over the maximum length for an item.
    //printf("Comment: %s\n",comment);
    if (strlen(comment) != 0) {
        temp = 0;
        while ( (strlen(line) < (64 + strlen(last_object))) && (temp < (int)strlen(comment)) ) {
            //printf("temp: %d->%d\t%c\n", temp, strlen(line), comment[temp]);
            line[strlen(line) + 1] = '\0';
            line[strlen(line)] = comment[temp++];
        }
    }
    //printf("line: %s\n",line);

// NOTE:  Compressed mode will be shorter still.  Account
// for that when compressed mode is implemented for items.

    return(1);
}






/*
 *  Set an Object
 */
void Object_change_data_set(/*@unused@*/ Widget widget, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer callData) {
    char line[43+1+40];                 // ???

    //printf("Object_change_data_set\n");

    if (Setup_object_data(line, sizeof(line))) {

        // Update this object in our save file
        log_object_item(line,0,last_object);

        if (object_tx_disable)
            output_my_data(line,-1,0,1,0,NULL);    // Local loopback only, not igating
        else
            output_my_data(line,-1,0,0,0,NULL);    // Transmit/loopback object data, not igating

        sched_yield();                  // Wait for transmitted data to get processed
        Object_destroy_shell(widget,clientData,NULL);
        redraw_symbols(da);
        (void)XCopyArea(XtDisplay(da),pixmap_final,XtWindow(da),gc,0,0,screen_width,screen_height,0,0);
    } else {
        // error message
        popup_message(langcode("POPEM00022"),langcode("POPEM00027"));
    }
}





/*
 *  Set an Item
 */
void Item_change_data_set(/*@unused@*/ Widget widget, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer callData) {
    char line[43+1+40];                 // ???
    
    if (Setup_item_data(line,sizeof(line))) {

        // Update this item in our save file
        log_object_item(line,0,last_object);

        if (object_tx_disable)
            output_my_data(line,-1,0,1,0,NULL);    // Local loopback only, not igating
        else
            output_my_data(line,-1,0,0,0,NULL);    // Transmit/loopback item data, not igating

        sched_yield();                  // Wait for transmitted data to get processed
        Object_destroy_shell(widget,clientData,NULL);
        redraw_symbols(da);
        (void)XCopyArea(XtDisplay(da),pixmap_final,XtWindow(da),gc,0,0,screen_width,screen_height,0,0);
    } else {
        // error message
        popup_message(langcode("POPEM00022"),langcode("POPEM00027"));
    }
}





/*
 *  Delete an Object
 */
void Object_change_data_del(/*@unused@*/ Widget widget, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer callData) {
    char line[43+1+40];                 // ???
    
    if (Setup_object_data(line, sizeof(line))) {

        line[10] = '_';                         // mark as deleted object

        // Update this object in our save file, then comment all
        // instances out in the file.
        log_object_item(line,1,last_object);

        if (object_tx_disable)
            output_my_data(line,-1,0,1,0,NULL);    // Local loopback only, not igating
        else
            output_my_data(line,-1,0,0,0,NULL);    // Transmit object data, not igating

        Object_destroy_shell(widget,clientData,NULL);
    }
}





/*
 *  Delete an Item
 */
void Item_change_data_del(/*@unused@*/ Widget widget, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer callData) {
    char line[43+1+40];                 // ???
    int i, done;
    
    if (Setup_item_data(line,sizeof(line))) {

        done = 0;
        i = 0;
        while ( (!done) && (i < 11) ) {
            if (line[i] == '!') {
                line[i] = '_';          // mark as deleted object
                done++;                 // Exit from loop
            }
            i++;
        }

        // Update this item in our save file, then comment all
        // instances out in the file.
        log_object_item(line,1,last_object);

        if (object_tx_disable)
            output_my_data(line,-1,0,1,0,NULL);    // Local loopback only, not igating
        else
            output_my_data(line,-1,0,0,0,NULL);    // Transmit item data, not igating

        Object_destroy_shell(widget,clientData,NULL);
    }
}





/*
 *  Select a symbol graphically
 */
void Ob_change_symbol(/*@unused@*/ Widget widget, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer callData) {
    //printf("Trying to change a symbol\n");
    symbol_change_requested_from = 2;   // Tell Select_symbol who to return the data to
    Select_symbol(widget, clientData, callData);
}





/*
 *  Update symbol picture for changed symbol or table
 */
void updateObjectPictureCallback(/*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer callData) {
    char table, overlay;
    char symb, group;

    XtVaSetValues(object_icon, XmNlabelPixmap, Ob_icon0, NULL);         // clear old icon
    XtManageChild(object_icon);

    group = (XmTextFieldGetString(object_group_data))[0];
    symb  = (XmTextFieldGetString(object_symbol_data))[0];
    if (group == '/' || group == '\\') {
        table   = group;
        overlay = ' ';
    } else {
        table   = '\\';
        overlay = group;
    }
    symbol(object_icon,0,table,symb,overlay,Ob_icon,0,0,0,' ');         // create icon

    XtVaSetValues(object_icon,XmNlabelPixmap,Ob_icon,NULL);             // draw new icon
    XtManageChild(object_icon);
}





// Handler for "Signpost" toggle button
void  Signpost_object_toggle( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;
    char temp_data[40];
 
    if(state->set) {
        Signpost_object_enabled = 1;
        Area_object_enabled = 0;
        DF_object_enabled = 0;

        //printf("Signpost Objects are ENABLED\n");

        // Call Set_Del_Object again, causing it to redraw with the new options.
        //Set_Del_Object( widget, clientData, callData );
        Set_Del_Object( widget, global_parameter1, global_parameter2 );

        XmToggleButtonSetState(area_toggle, FALSE, FALSE);
        XmToggleButtonSetState(df_bearing_toggle, FALSE, FALSE);

        temp_data[0] = '\\';
        temp_data[1] = '\0';
        XmTextFieldSetString(object_group_data,temp_data);

        temp_data[0] = 'm';
        temp_data[1] = '\0';
        XmTextFieldSetString(object_symbol_data,temp_data);

        XtSetSensitive(ob_frame,FALSE);

        // update symbol picture
        (void)updateObjectPictureCallback((Widget)NULL,(XtPointer)NULL,(XtPointer)NULL);
   }
    else {
        Signpost_object_enabled = 0;

        //printf("Signpost Objects are DISABLED\n");

        // Call Set_Del_Object again, causing it to redraw with the new options.
        //Set_Del_Object( widget, clientData, callData );
        Set_Del_Object( widget, global_parameter1, global_parameter2 );


        temp_data[0] = '\\';
        temp_data[1] = '\0';
        XmTextFieldSetString(object_group_data,temp_data);

        temp_data[0] = '!';
        temp_data[1] = '\0';
        XmTextFieldSetString(object_symbol_data,temp_data);

        XtSetSensitive(ob_frame,TRUE);

        // update symbol picture
        (void)updateObjectPictureCallback((Widget)NULL,(XtPointer)NULL,(XtPointer)NULL);
  }
}





// Handler for "Enable Area Type" toggle button
void  Area_object_toggle( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;
    char temp_data[40];
 
    if(state->set) {
        Area_object_enabled = 1;
        Signpost_object_enabled = 0;
        DF_object_enabled = 0;

        //printf("Area Objects are ENABLED\n");

        // Call Set_Del_Object again, causing it to redraw with the new options.
        //Set_Del_Object( widget, clientData, callData );
        Set_Del_Object( widget, global_parameter1, global_parameter2 );


        XmToggleButtonSetState(signpost_toggle, FALSE, FALSE);
        XmToggleButtonSetState(df_bearing_toggle, FALSE, FALSE);
        XtSetSensitive(ob_speed,FALSE);
        XtSetSensitive(ob_speed_data,FALSE);
        XtSetSensitive(ob_course,FALSE);
        XtSetSensitive(ob_course_data,FALSE);

        temp_data[0] = '\\';
        temp_data[1] = '\0';
        XmTextFieldSetString(object_group_data,temp_data);

        temp_data[0] = 'l';
        temp_data[1] = '\0';
        XmTextFieldSetString(object_symbol_data,temp_data);

        XtSetSensitive(ob_frame,FALSE);

        // update symbol picture
        (void)updateObjectPictureCallback((Widget)NULL,(XtPointer)NULL,(XtPointer)NULL);
   }
    else {
        Area_object_enabled = 0;

        //printf("Area Objects are DISABLED\n");

        // Call Set_Del_Object again, causing it to redraw with the new options.
        //Set_Del_Object( widget, clientData, callData );
        Set_Del_Object( widget, global_parameter1, global_parameter2 );


        XtSetSensitive(ob_speed,TRUE);
        XtSetSensitive(ob_speed_data,TRUE);
        XtSetSensitive(ob_course,TRUE);
        XtSetSensitive(ob_course_data,TRUE);

        temp_data[0] = '\\';
        temp_data[1] = '\0';
        XmTextFieldSetString(object_group_data,temp_data);

        temp_data[0] = '!';
        temp_data[1] = '\0';
        XmTextFieldSetString(object_symbol_data,temp_data);

        XtSetSensitive(ob_frame,TRUE);

        // update symbol picture
        (void)updateObjectPictureCallback((Widget)NULL,(XtPointer)NULL,(XtPointer)NULL);
    }
}





// Handler for "DF Bearing Object" toggle button
void  DF_bearing_object_toggle( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;
    char temp_data[40];
 
    if(state->set) {
        Area_object_enabled = 0;
        Signpost_object_enabled = 0;
        DF_object_enabled = 1;

        //printf("DF Objects are ENABLED\n");

        // Call Set_Del_Object again, causing it to redraw with the new options.
        //Set_Del_Object( widget, clientData, callData );
        Set_Del_Object( widget, global_parameter1, global_parameter2 );


        XmToggleButtonSetState(signpost_toggle, FALSE, FALSE);
        XmToggleButtonSetState(area_toggle, FALSE, FALSE);
        XtSetSensitive(ob_speed,TRUE);
        XtSetSensitive(ob_speed_data,TRUE);
        XtSetSensitive(ob_course,TRUE);
        XtSetSensitive(ob_course_data,TRUE);
        XtSetSensitive(frameomni,FALSE);
        XtSetSensitive(framebeam,FALSE);
        Omni_antenna_enabled = 0;
        Beam_antenna_enabled = 0;

        temp_data[0] = '/';
        temp_data[1] = '\0';
        XmTextFieldSetString(object_group_data,temp_data);

        temp_data[0] = '\\';
        temp_data[1] = '\0';
        XmTextFieldSetString(object_symbol_data,temp_data);

        XtSetSensitive(ob_frame,FALSE);

        // update symbol picture
        (void)updateObjectPictureCallback((Widget)NULL,(XtPointer)NULL,(XtPointer)NULL);
   }
    else {
        DF_object_enabled = 0;

        //printf("DF Objects are DISABLED\n");

        // Call Set_Del_Object again, causing it to redraw with the new options.
        //Set_Del_Object( widget, clientData, callData );
        Set_Del_Object( widget, global_parameter1, global_parameter2 );


        temp_data[0] = '\\';
        temp_data[1] = '\0';
        XmTextFieldSetString(object_group_data,temp_data);

        temp_data[0] = '!';
        temp_data[1] = '\0';
        XmTextFieldSetString(object_symbol_data,temp_data);

        XtSetSensitive(ob_frame,TRUE);

        // update symbol picture
        (void)updateObjectPictureCallback((Widget)NULL,(XtPointer)NULL,(XtPointer)NULL);
    }
}





/* Area object type radio buttons */
void Area_type_toggle( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set) {
        Area_type = atoi(which);    // Set to shape desired
        if ( (Area_type == 1) || (Area_type == 6) ) {   // If either line type
            //printf("Line type: %d\n", Area_type);
            XtSetSensitive(ob_corridor,TRUE);
            XtSetSensitive(ob_corridor_data,TRUE);
            XtSetSensitive(ob_corridor_miles,TRUE);
            XtSetSensitive(open_filled_toggle,FALSE);
        }
        else {  // Not line type
            //printf("Not line type: %d\n", Area_type);
            XtSetSensitive(ob_corridor,FALSE);
            XtSetSensitive(ob_corridor_data,FALSE);
            XtSetSensitive(ob_corridor_miles,FALSE);
            XtSetSensitive(open_filled_toggle,TRUE);
        }
    }
    else {
        Area_type = 0;  // Open circle
        //printf("Type zero\n");
    }
    //printf("Area type: %d\n", Area_type);
}





/* Area object color radio buttons */
void Area_color_toggle( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set) {
        Area_color[0] = which[0];   // Set to color desired.
        Area_color[1] = which[1];
        Area_color[2] = '\0';
    }
    else {
        Area_color[0] = '/';
        Area_color[1] = '0';    // Black
        Area_color[2] = '\0';
    }
    //printf("Area color: %s\n", Area_color);
}





/* Area bright color enable button */
void Area_bright_dim_toggle( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set) {
        Area_bright = atoi(which);
        //printf("Bright colors are ENABLED: %d\n", Area_bright);
    }
    else {
        Area_bright = 0;
        //printf("Bright colors are DISABLED: %d\n", Area_bright);
    }
}





/* Area filled enable button */
void Area_open_filled_toggle( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set) {
        Area_filled = atoi(which);
        //printf("Filled shapes ENABLED: %d\n", Area_filled);
    }
    else {
        Area_filled = 0;
        //printf("Filled shapes DISABLED: %d\n", Area_filled);
    }
}





// Handler for "Omni Antenna" toggle button
void  Omni_antenna_toggle( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;
 
    if(state->set) {
        //printf("Omni Antenna ENABLED\n");
        XmToggleButtonSetState(beam_antenna_toggle, FALSE, FALSE);
        XtSetSensitive(frameomni,TRUE);
        XtSetSensitive(framebeam,FALSE);
        Omni_antenna_enabled = 1;
        Beam_antenna_enabled = 0;
   }
    else {
        //printf("Omni Antenna DISABLED\n");
        XtSetSensitive(frameomni,FALSE);
        Omni_antenna_enabled = 0;
    }
}





// Handler for "Beam Antenna" toggle button
void  Beam_antenna_toggle( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;
 
    if(state->set) {
        //printf("Beam Antenna ENABLED\n");
        XmToggleButtonSetState(omni_antenna_toggle, FALSE, FALSE);
        XtSetSensitive(frameomni,FALSE);
        XtSetSensitive(framebeam,TRUE);
        Omni_antenna_enabled = 0;
        Beam_antenna_enabled = 1;
   }
    else {
        //printf("Beam Antenna DISABLED\n");
        XtSetSensitive(framebeam,FALSE);
        Beam_antenna_enabled = 0;
    }
}





/* Object signal radio buttons */
void Ob_signal_toggle( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set) {
        object_shgd[0] = which[0];  // Set to signal quality heard
    }
    else {
        object_shgd[0] = '0';       // 0 (Signal not heard)
    }
    object_shgd[4] = '\0';

    //printf("SHGD: %s\n",object_shgd);
}





/* Object height radio buttons */
void Ob_height_toggle( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set) {
        object_shgd[1] = which[0];  // Set to height desired
    }
    else {
        object_shgd[1] = '0';       // 0 (10ft HAAT)
    }
    object_shgd[4] = '\0';

    //printf("SHGD: %s\n",object_shgd);
}





/* Object gain radio buttons */
void Ob_gain_toggle( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set) {
        object_shgd[2] = which[0];  // Set to antenna gain desired
    }
    else {
        object_shgd[2] = '0';       // 0dB gain
    }
    object_shgd[4] = '\0';

    //printf("SHGD: %s\n",object_shgd);
}





/* Object directivity radio buttons */
void Ob_directivity_toggle( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set) {
        object_shgd[3] = which[0];  // Set to antenna pattern desired
    }
    else {
        object_shgd[3] = '0';       // Omni-directional pattern
    }
    object_shgd[4] = '\0';

    //printf("SHGD: %s\n",object_shgd);
}





/* Object beamwidth radio buttons */
void Ob_width_toggle( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set) {
        object_NRQ[2] = which[0];  // Set to antenna beamwidth desired
    }
    else {
        object_NRQ[2] = '0';       // Beamwidth = "Useless"
    }
    object_NRQ[3] = '\0';

    //printf("NRQ: %s\n", object_NRQ);
}





// Fill in fields from an existing object/item or create the proper
// transmit string for a new object/item from these fields.
/*
 *  Setup Object/Item Dialog
 *  clientData = pointer to object struct, if it's a modify or a move operation,
 *  else it's NULL.
 *  If calldata = 2, then we're doing a move object operation.  We want in that
 *  case to fill in the new values for lat/long and make them take effect.
 *  If calldata = 1, then we've dropped through Station_info/Station_data
 *  on the way to Modify->Object.
 *  Need to put the tests for the different types of objects
 *  at the top of this function, then the dialog will build properly for
 *  the type of object initially.
 */
void Set_Del_Object( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, XtPointer calldata) {
    Dimension width, height;
    long lat,lon;
    char lat_str[MAX_LAT];
    char lon_str[MAX_LONG];
    static Widget ob_pane, ob_form,
                ob_name,ob_latlon_frame,ob_latlon_form,ob_latlon_ts,
                ob_lat, ob_lat_deg, ob_lat_min, ob_lat_ns,
                ob_lon, ob_lon_deg, ob_lon_min, ob_lon_ew,
                ob_form1, ob_ts,
                signpost_form,signpost_ts,
                signpost_label,
                ob_option_frame,ob_option_ts,ob_option_form,
                ob_altitude,
                ob_comment, area_ts, area_form,
                bright_dim_toggle,
                shape_box,toption1,toption2,toption3,toption4,toption5,
                color_box,coption1,coption2,coption3,coption4,coption5,coption6,coption7,coption8,
                omnilabel,formomni,
                signal_box,soption0,soption1,soption2,soption3,soption4,soption5,soption6,soption7,soption8,soption9,
                height_box,hoption0,hoption1,hoption2,hoption3,hoption4,hoption5,hoption6,hoption7,hoption8,hoption9,
                gain_box,goption0,goption1,goption2,goption3,goption4,goption5,goption6,goption7,goption8,goption9,
                directivity_box,doption0,doption1,doption2,doption3,doption4,doption5,doption6,doption7,doption8,
                beamlabel,formbeam,
                width_box,woption0,woption1,woption2,woption3,woption4,woption5,woption6,woption7,woption8,woption9,
                ob_bearing,
                ob_lat_offset,ob_lon_offset,
                ob_sep, ob_sep2, ob_button_set, ob_button_del,ob_button_cancel,it_button_set,
                ob_button_symbol,
                compute_button;
    char temp_data[40];
    Atom delw;
    DataRow *p_station = (DataRow *)clientData;
    Arg al[2];                    /* Arg List */
    register unsigned int ac;     /* Arg Count */
    long x,y;


/*
    if (p_station != NULL)
        printf("Have a pointer to an object.  ");
    else
        printf("No pointer, new object?       ");
    if (calldata != NULL) {
        if (strcmp(calldata,"2") == 0)
            printf("Set_Del_Object: calldata: 2.  Move object.\n");
        else if (strcmp(calldata,"1") == 0)
            printf("Set_Del_Object: calldata: 1.  Modify object.\n");
        else if (strcmp(calldata,"0") == 0)
            printf("Set_Del_Object: calldata: 0.  New object.\n");
        else
            printf("Set_Del_Object: calldata: invalid.  New object.\n");
    }
*/


    // Save the data so that other routines can access it.  Some of the
    // callbacks can only handle one parameter, and we need two.
    if (p_station != NULL)
        global_parameter1 = clientData;
    else
        global_parameter1 = NULL;
    global_parameter2 = calldata;
    


    // This function can be invoked from the mouse menus or by being called
    // directly by other routines.  We look at the p_station pointer to decide
    // how we were called.
    //
    if (p_station != NULL) {  // We were called from the Modify_object() function
        //printf("Got a pointer!\n");
        lon = p_station->coord_lon;     // Fill in values from the original object
        lat = p_station->coord_lat;
    }
    else {  // We were called from the "Create New Object" mouse menu or by the "Move" option
        // get default position for object, the position we have clicked at
        XtVaGetValues(da,XmNwidth, &width,XmNheight, &height,0);
        lon = mid_x_long_offset - ((width *scale_x)/2) + (menu_x*scale_x);
        lat = mid_y_lat_offset  - ((height*scale_y)/2) + (menu_y*scale_y);
    }


    // If the object dialog is up, we need to kill it and draw a new
    // one so that we have the correct values filled in.
    if (object_dialog)
        Object_destroy_shell( w, object_dialog, NULL);


    // Check for the three "Special" types of objects we deal with and set
    // the global variables for them here.  This will result in the correct
    // type of dialog being drawn for each type of object.
// Question:  What about for Modify->Object where we're trying to change
// the type of the object?

    if (p_station != NULL) {

/*
        if (calldata != NULL) {
            if (strcmp(calldata,"2") == 0)
                printf("Set_Del_Object: calldata: 2.  Move object.\n");
            else if (strcmp(calldata,"1") == 0)
                printf("Set_Del_Object: calldata: 1.  Modify object.\n");
            else if (strcmp(calldata,"0") == 0)
                printf("Set_Del_Object: calldata: 0.  New object.\n");
            else
                printf("Set_Del_Object: calldata: invalid.  New object.\n");
        }
*/


        // Check to see whether we should even be here at all!
        if ( !(p_station->flag & ST_OBJECT)
                && !(p_station->flag & ST_ITEM)) {  // Not an object or item
            //printf("flag: %i\n", (int)p_station->flag);
            popup_message(langcode("POPEM00022"), "Not an Object/Item!");
            return;
        }

        // Set to known defaults first
        Area_object_enabled = 0;
        Signpost_object_enabled = 0;
        DF_object_enabled = 0;
 
        if (p_station->aprs_symbol.area_object.type != AREA_NONE) { // Found an area object
            Area_object_enabled = 1;
        }
        else if ( (p_station->aprs_symbol.aprs_symbol == 'm')   // Found a signpost object
                && (p_station->aprs_symbol.aprs_type == '\\') ) {
            Signpost_object_enabled = 1;
        }
        else if ( (p_station->aprs_symbol.aprs_symbol == '\\')   // Found a DF object
                && (p_station->aprs_symbol.aprs_type == '/')
                && ((strlen(p_station->signal_gain) == 7)          // That has data associated with it
                    || (strlen(p_station->bearing) == 3)
                    || (strlen(p_station->NRQ) == 3) ) ) {
            DF_object_enabled = 1;
        }
    }

    //printf("Area:Signpost:DF  %i:%i:%i\n",Area_object_enabled,Signpost_object_enabled,DF_object_enabled);

// Ok.  The stage is now set to draw the proper type of dialog for the
// type of object we're interested in currently.


    if(object_dialog)           // it is already open
        (void)XRaiseWindow(XtDisplay(object_dialog), XtWindow(object_dialog));
    else {                      // create new popup window
        object_dialog = XtVaCreatePopupShell(langcode("POPUPOB001"),
                xmDialogShellWidgetClass,
                Global.top,
                XmNdeleteResponse,          XmDESTROY,
                XmNdefaultPosition,         FALSE,
                NULL);

        ob_pane = XtVaCreateWidget("Set_Del_Object pane",
                xmPanedWindowWidgetClass, 
                object_dialog,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        ob_form =  XtVaCreateWidget("Set_Del_Object ob_form",
                xmFormWidgetClass, 
                ob_pane,
                XmNfractionBase,            3,
                XmNautoUnmanage,            FALSE,
                XmNshadowThickness,         1,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        // "Name"
        ob_name = XtVaCreateManagedWidget(langcode("POPUPOB002"),
                xmLabelWidgetClass, 
                ob_form,
                XmNtopAttachment,           XmATTACH_FORM,
                XmNtopOffset,               10,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNleftAttachment,          XmATTACH_FORM,
                XmNleftOffset,              10,
                XmNrightAttachment,         XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        // object name
        object_name_data = XtVaCreateManagedWidget("Set_Del_Object name_data", 
                xmTextFieldWidgetClass, 
                ob_form,
                XmNeditable,                TRUE,
                XmNcursorPositionVisible,   TRUE,
                XmNsensitive,               TRUE,
                XmNshadowThickness,         1,
                XmNcolumns,                 9,
                XmNmaxLength,               9,
                XmNtopAttachment,           XmATTACH_FORM,
                XmNtopOffset,               5,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              ob_name,
                XmNrightAttachment,         XmATTACH_NONE,
                XmNbackground,              colors[0x0f],
                NULL);


//----- Frame for table / symbol
        ob_frame = XtVaCreateManagedWidget("Set_Del_Object ob_frame", 
                xmFrameWidgetClass, 
                ob_form,
                XmNtopAttachment,           XmATTACH_WIDGET,
                XmNtopWidget,               object_name_data,
                XmNtopOffset,               10,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNleftAttachment,          XmATTACH_FORM,
                XmNleftOffset,              10,
                XmNrightAttachment,         XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        // "Station Symbol"
        ob_ts  = XtVaCreateManagedWidget(langcode("WPUPCFS009"),
                xmLabelWidgetClass,
                ob_frame,
                XmNchildType,               XmFRAME_TITLE_CHILD,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        ob_form1 =  XtVaCreateWidget("Set_Del_Object form1",
                xmFormWidgetClass, 
                ob_frame,
                XmNfractionBase,            5,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        // "Group/overlay"
        ob_group = XtVaCreateManagedWidget(langcode("WPUPCFS010"),
                xmLabelWidgetClass, 
                ob_form1,
                XmNtopAttachment,           XmATTACH_FORM,
                XmNtopOffset,               8,
                XmNbottomAttachment,        XmATTACH_FORM,
                XmNbottomOffset,            10,
                XmNleftAttachment,          XmATTACH_FORM,
                XmNleftOffset,              10,
                XmNrightAttachment,         XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        // table
        object_group_data = XtVaCreateManagedWidget("Set_Del_Object group", 
                xmTextFieldWidgetClass, 
                ob_form1,
                XmNeditable,                TRUE,
                XmNcursorPositionVisible,   FALSE,
                XmNsensitive,               TRUE,
                XmNshadowThickness,         1,
                XmNcolumns,                 1,
                XmNmaxLength,               1,
                XmNtopOffset,               3,
                XmNbackground,              colors[0x0f],
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              ob_group,
                XmNleftOffset,              5,
                XmNtopAttachment,           XmATTACH_FORM,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNrightAttachment,         XmATTACH_NONE,
                NULL);

        // "Symbol"
        ob_symbol = XtVaCreateManagedWidget(langcode("WPUPCFS011"),
                xmLabelWidgetClass, 
                ob_form1,
                XmNtopAttachment,           XmATTACH_FORM,
                XmNtopOffset,               8,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              object_group_data,
                XmNleftOffset,              20,
                XmNrightAttachment,         XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        // symbol
        object_symbol_data = XtVaCreateManagedWidget("Set_Del_Object symbol", 
                xmTextFieldWidgetClass, 
                ob_form1,
                XmNeditable,                TRUE,
                XmNcursorPositionVisible,   FALSE,
                XmNsensitive,               TRUE,
                XmNshadowThickness,         1,
                XmNcolumns,                 1,
                XmNmaxLength,               1,
                XmNtopOffset,               3,
                XmNbackground,              colors[0x0f],
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              ob_symbol,
                XmNleftOffset,              5,
                XmNtopAttachment,           XmATTACH_FORM,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNrightAttachment,         XmATTACH_NONE,
                NULL);

        // icon
        Ob_icon0 = XCreatePixmap(XtDisplay(appshell),
                RootWindowOfScreen(XtScreen(appshell)),
                20,
                20,
                DefaultDepthOfScreen(XtScreen(appshell)));
        Ob_icon  = XCreatePixmap(XtDisplay(appshell),
                RootWindowOfScreen(XtScreen(appshell)),
                20,
                20,
                DefaultDepthOfScreen(XtScreen(appshell)));
        object_icon = XtVaCreateManagedWidget("Set_Del_Object icon", 
                xmLabelWidgetClass, 
                ob_form1,
                XmNlabelType,               XmPIXMAP,
                XmNlabelPixmap,             Ob_icon,
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              object_symbol_data,
                XmNleftOffset,              15,
                XmNtopAttachment,           XmATTACH_FORM,
                XmNtopOffset,               8,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNrightAttachment,         XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        ob_button_symbol = XtVaCreateManagedWidget(langcode("WPUPCFS028"),
                xmPushButtonGadgetClass, 
                ob_form1,
                XmNtopAttachment,           XmATTACH_FORM,
                XmNtopOffset,               2,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              object_icon,
                XmNleftOffset,              5,
                XmNrightAttachment,         XmATTACH_FORM,
                XmNrightOffset,             5,
                XmNnavigationType,          XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(ob_button_symbol, XmNactivateCallback, Ob_change_symbol, object_dialog);
 
//----- Frame for Lat/Long
        ob_latlon_frame = XtVaCreateManagedWidget("Set_Del_Object ob_latlon_frame", 
                xmFrameWidgetClass, 
                ob_form,
                XmNtopAttachment,           XmATTACH_FORM,
                XmNtopOffset,               10,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              ob_frame,
                XmNleftOffset,              10,
                XmNrightAttachment,         XmATTACH_FORM,
                XmNrightOffset,             10,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        // "Location"
        ob_latlon_ts  = XtVaCreateManagedWidget(langcode("POPUPOB028"),
                xmLabelWidgetClass,
                ob_latlon_frame,
                XmNchildType,               XmFRAME_TITLE_CHILD,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        ob_latlon_form =  XtVaCreateWidget("Set_Del_Object ob_latlon_form",
                xmFormWidgetClass, 
                ob_latlon_frame,
                XmNfractionBase,            5,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        // "LAT"
        ob_lat = XtVaCreateManagedWidget(langcode("WPUPCFS003"),
                xmLabelWidgetClass, 
                ob_latlon_form,
                XmNtopAttachment,           XmATTACH_FORM,
                XmNtopOffset,               10,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNleftAttachment,          XmATTACH_FORM,
                XmNleftOffset,              15,
                XmNrightAttachment,         XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        // lat deg
        object_lat_data_deg = XtVaCreateManagedWidget("Set_Del_Object lat_deg", 
                xmTextFieldWidgetClass, 
                ob_latlon_form,
                XmNeditable,                TRUE,
                XmNcursorPositionVisible,   TRUE,
                XmNsensitive,               TRUE,
                XmNshadowThickness,         1,
                XmNcolumns,                 3,
                XmNmaxLength,               2,
                XmNtopOffset,               5,
                XmNtopAttachment,           XmATTACH_FORM,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              ob_lat,
                XmNrightAttachment,         XmATTACH_NONE,
                XmNbackground,              colors[0x0f],
                NULL);
        // "deg"
        ob_lat_deg = XtVaCreateManagedWidget(langcode("WPUPCFS004"),
                xmLabelWidgetClass, 
                ob_latlon_form,
                XmNtopAttachment,           XmATTACH_FORM,
                XmNtopOffset,               10,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              object_lat_data_deg,
                XmNrightAttachment,         XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        // lat min
        object_lat_data_min = XtVaCreateManagedWidget("Set_Del_Object lat_min", 
                xmTextFieldWidgetClass, 
                ob_latlon_form,
                XmNeditable,                TRUE,
                XmNcursorPositionVisible,   TRUE,
                XmNsensitive,               TRUE,
                XmNshadowThickness,         1,
                XmNcolumns,                 6,
                XmNmaxLength,               6,
                XmNtopOffset,               5,
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              ob_lat_deg,
                XmNleftOffset,              10,
                XmNtopAttachment,           XmATTACH_FORM,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNrightAttachment,         XmATTACH_NONE,
                XmNbackground,              colors[0x0f],
                NULL);
        // "min"
        ob_lat_min = XtVaCreateManagedWidget(langcode("WPUPCFS005"),
                xmLabelWidgetClass, 
                ob_latlon_form,
                XmNtopAttachment,           XmATTACH_FORM,
                XmNtopOffset,               10,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              object_lat_data_min,
                XmNrightAttachment,         XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        // N/S
        object_lat_data_ns = XtVaCreateManagedWidget("Set_Del_Object lat_ns", 
                xmTextFieldWidgetClass, 
                ob_latlon_form,
                XmNeditable,                TRUE,
                XmNcursorPositionVisible,   FALSE,
                XmNsensitive,               TRUE,
                XmNshadowThickness,         1,
                XmNcolumns,                 1,
                XmNmaxLength,               1,
                XmNtopOffset,               5,
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              ob_lat_min,
                XmNleftOffset,              10,
                XmNtopAttachment,           XmATTACH_FORM,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNrightAttachment,         XmATTACH_NONE,
                XmNbackground,              colors[0x0f],
                NULL);
        // "(N/S)"
        ob_lat_ns = XtVaCreateManagedWidget(langcode("WPUPCFS006"),
                xmLabelWidgetClass, 
                ob_latlon_form,
                XmNtopAttachment,           XmATTACH_FORM,
                XmNtopOffset,               10,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              object_lat_data_ns,
                XmNrightAttachment,         XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        // "LONG"
        ob_lon = XtVaCreateManagedWidget(langcode("WPUPCFS007"),
                xmLabelWidgetClass, 
                ob_latlon_form,
                XmNtopAttachment,           XmATTACH_WIDGET,
                XmNtopWidget,               ob_lat,
                XmNtopOffset,               20,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNleftAttachment,          XmATTACH_FORM,
                XmNleftOffset,              10,
                XmNrightAttachment,         XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        // long
        object_lon_data_deg = XtVaCreateManagedWidget("Set_Del_Object long_deg", 
                xmTextFieldWidgetClass, 
                ob_latlon_form,
                XmNeditable,                TRUE,
                XmNcursorPositionVisible,   TRUE,
                XmNsensitive,               TRUE,
                XmNshadowThickness,         1,
                XmNcolumns,                 3,
                XmNmaxLength,               3,
                XmNtopOffset,               14,
                XmNtopAttachment,           XmATTACH_WIDGET,
                XmNtopWidget,               ob_lat,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              ob_lon,
                XmNrightAttachment,         XmATTACH_NONE,
                XmNbackground,              colors[0x0f],
                NULL);
        // "deg"
        ob_lon_deg = XtVaCreateManagedWidget(langcode("WPUPCFS004"),
                xmLabelWidgetClass, 
                ob_latlon_form,
                XmNtopAttachment,           XmATTACH_WIDGET,
                XmNtopWidget,               ob_lat,
                XmNtopOffset,               20,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              object_lon_data_deg,
                XmNrightAttachment,         XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        // min
        object_lon_data_min = XtVaCreateManagedWidget("Set_Del_Object long_min", 
                xmTextFieldWidgetClass, 
                ob_latlon_form,
                XmNeditable,                TRUE,
                XmNcursorPositionVisible,   TRUE,
                XmNsensitive,               TRUE,
                XmNshadowThickness,         1,
                XmNcolumns,                 6,
                XmNmaxLength,               6,
                XmNtopOffset,               14,
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              ob_lon_deg,
                XmNleftOffset,              10,
                XmNtopAttachment,           XmATTACH_WIDGET,
                XmNtopWidget,               ob_lat,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNrightAttachment,         XmATTACH_NONE,
                XmNbackground,              colors[0x0f],
                NULL);
        // "min"
        ob_lon_min = XtVaCreateManagedWidget(langcode("WPUPCFS005"),
                xmLabelWidgetClass, 
                ob_latlon_form,
                XmNtopAttachment,           XmATTACH_WIDGET,
                XmNtopWidget,               ob_lat,
                XmNtopOffset,               20,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              object_lon_data_min,
                XmNrightAttachment,         XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        // E/W
        object_lon_data_ew = XtVaCreateManagedWidget("Set_Del_Object long_ew", 
                xmTextFieldWidgetClass, 
                ob_latlon_form,
                XmNeditable,                TRUE,
                XmNcursorPositionVisible,   FALSE,
                XmNsensitive,               TRUE,
                XmNshadowThickness,         1,
                XmNcolumns,                 1,
                XmNmaxLength,               1,
                XmNtopOffset,               14,
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              ob_lon_min,
                XmNleftOffset,              10,
                XmNtopAttachment,           XmATTACH_WIDGET,
                XmNtopWidget,               ob_lat,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNrightAttachment,         XmATTACH_NONE,
                XmNbackground,              colors[0x0f],
                NULL);
        // "(E/W)"
        ob_lon_ew = XtVaCreateManagedWidget(langcode("WPUPCFS008"),
                xmLabelWidgetClass, 
                ob_latlon_form,
                XmNtopAttachment,           XmATTACH_WIDGET,
                XmNtopWidget,               ob_lat,
                XmNtopOffset,               20,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              object_lon_data_ew,
                XmNrightAttachment,         XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        compute_button = XtVaCreateManagedWidget(langcode("COORD002"),
                xmPushButtonGadgetClass, 
                ob_latlon_form,
                XmNtopAttachment,           XmATTACH_WIDGET,
                XmNtopWidget,               ob_lat,
                XmNtopOffset,               20,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              ob_lon_ew,
                XmNleftOffset,              10,
                XmNrightAttachment,         XmATTACH_NONE,
                XmNnavigationType,          XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        // Fill in the pointers to our input textfields so that the coordinate
        // calculator can fiddle with them.
        coordinate_calc_array.calling_dialog = object_dialog;
        coordinate_calc_array.input_lat_deg = object_lat_data_deg;
        coordinate_calc_array.input_lat_min = object_lat_data_min;
        coordinate_calc_array.input_lat_dir = object_lat_data_ns;
        coordinate_calc_array.input_lon_deg = object_lon_data_deg;
        coordinate_calc_array.input_lon_min = object_lon_data_min;
        coordinate_calc_array.input_lon_dir = object_lon_data_ew;
//        XtAddCallback(compute_button, XmNactivateCallback, Coordinate_calc, ob_latlon_form);
//        XtAddCallback(compute_button, XmNactivateCallback, Coordinate_calc, "Set_Del_Object");
        XtAddCallback(compute_button, XmNactivateCallback, Coordinate_calc, langcode("POPUPOB001"));

//----- Frame for generic options
        ob_option_frame = XtVaCreateManagedWidget("Set_Del_Object ob_option_frame", 
                xmFrameWidgetClass, 
                ob_form,
                XmNtopAttachment,           XmATTACH_WIDGET,
                XmNtopWidget,               ob_frame,
                XmNtopOffset,               10,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNleftAttachment,          XmATTACH_FORM,
                XmNleftOffset,              10,
                XmNrightAttachment,         XmATTACH_NONE,
                XmNrightOffset,             10,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        // "Generic Options"
        ob_option_ts  = XtVaCreateManagedWidget(langcode("POPUPOB027"),
                xmLabelWidgetClass,
                ob_option_frame,
                XmNchildType,               XmFRAME_TITLE_CHILD,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        ob_option_form =  XtVaCreateWidget("Set_Del_Object ob_option_form",
                xmFormWidgetClass, 
                ob_option_frame,
                XmNfractionBase,            5,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        // "Speed"
        ob_speed = XtVaCreateManagedWidget(langcode("POPUPOB036"),
                xmLabelWidgetClass, 
                ob_option_form,
                XmNtopAttachment,           XmATTACH_FORM,
                XmNtopOffset,               8,
                XmNbottomAttachment,        XmATTACH_FORM,
                XmNbottomOffset,            10,
                XmNleftAttachment,          XmATTACH_FORM,
                XmNleftOffset,              5,
                XmNrightAttachment,         XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        ob_speed_data = XtVaCreateManagedWidget("Set_Del_Object ob_speed_data", 
                xmTextFieldWidgetClass, 
                ob_option_form,
                XmNeditable,                TRUE,
                XmNcursorPositionVisible,   TRUE,
                XmNsensitive,               TRUE,
                XmNshadowThickness,         1,
                XmNcolumns,                 3,
                XmNmaxLength,               3,
                XmNtopOffset,               3,
                XmNbackground,              colors[0x0f],
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              ob_speed,
                XmNtopAttachment,           XmATTACH_FORM,
                XmNbottomAttachment,        XmATTACH_FORM,
                XmNrightAttachment,         XmATTACH_NONE,
                NULL);

        // "Course"
        ob_course = XtVaCreateManagedWidget(langcode("POPUPOB037"),
                xmLabelWidgetClass, 
                ob_option_form,
                XmNtopAttachment,           XmATTACH_FORM,
                XmNtopOffset,               8,
                XmNbottomAttachment,        XmATTACH_FORM,
                XmNbottomOffset,            10,
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              ob_speed_data,
                XmNleftOffset,              10,
                XmNrightAttachment,         XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        ob_course_data = XtVaCreateManagedWidget("Set_Del_Object ob_course_data", 
                xmTextFieldWidgetClass, 
                ob_option_form,
                XmNeditable,                TRUE,
                XmNcursorPositionVisible,   TRUE,
                XmNsensitive,               TRUE,
                XmNshadowThickness,         1,
                XmNcolumns,                 3,
                XmNmaxLength,               3,
                XmNtopOffset,               3,
                XmNbackground,              colors[0x0f],
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              ob_course,
                XmNtopAttachment,           XmATTACH_FORM,
                XmNbottomAttachment,        XmATTACH_FORM,
                XmNrightAttachment,         XmATTACH_NONE,
                NULL);

        // "Altitude"
        ob_altitude = XtVaCreateManagedWidget(langcode("POPUPOB035"),
                xmLabelWidgetClass, 
                ob_option_form,
                XmNtopAttachment,           XmATTACH_FORM,
                XmNtopOffset,               8,
                XmNbottomAttachment,        XmATTACH_FORM,
                XmNbottomOffset,            10,
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              ob_course_data,
                XmNleftOffset,              10,
                XmNrightAttachment,         XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        ob_altitude_data = XtVaCreateManagedWidget("Set_Del_Object ob_altitude_data", 
                xmTextFieldWidgetClass, 
                ob_option_form,
                XmNeditable,                TRUE,
                XmNcursorPositionVisible,   TRUE,
                XmNsensitive,               TRUE,
                XmNshadowThickness,         1,
                XmNcolumns,                 6,
                XmNmaxLength,               6,
                XmNtopOffset,               3,
                XmNbackground,              colors[0x0f],
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              ob_altitude,
                XmNtopAttachment,           XmATTACH_FORM,
                XmNbottomAttachment,        XmATTACH_FORM,
                XmNrightAttachment,         XmATTACH_NONE,
                NULL);


        // "Signpost Enable"
        signpost_toggle = XtVaCreateManagedWidget(langcode("POPUPOB029"),
                xmToggleButtonGadgetClass,
                ob_form,
                XmNtopAttachment,           XmATTACH_WIDGET,
                XmNtopWidget,               ob_latlon_frame,
                XmNtopOffset,               2,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNbottomOffset,            0,
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              ob_option_frame,
                XmNleftOffset,              10,
                XmNrightAttachment,         XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(signpost_toggle,XmNvalueChangedCallback,Signpost_object_toggle,(XtPointer)p_station);


 
        // "Area Enable"
        area_toggle = XtVaCreateManagedWidget(langcode("POPUPOB008"),
                xmToggleButtonGadgetClass,
                ob_form,
                XmNtopAttachment,           XmATTACH_WIDGET,
                XmNtopWidget,               signpost_toggle,
                XmNtopOffset,               0,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNbottomOffset,            0,
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              ob_option_frame,
                XmNleftOffset,              10,
                XmNrightAttachment,         XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(area_toggle,XmNvalueChangedCallback,Area_object_toggle,(XtPointer)p_station);



        // "Area Enable"
        df_bearing_toggle = XtVaCreateManagedWidget(langcode("POPUPOB038"),
                xmToggleButtonGadgetClass,
                ob_form,
                XmNtopAttachment,           XmATTACH_WIDGET,
                XmNtopWidget,               area_toggle,
                XmNtopOffset,               0,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNbottomOffset,            0,
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              ob_option_frame,
                XmNleftOffset,              10,
                XmNrightAttachment,         XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(df_bearing_toggle,XmNvalueChangedCallback,DF_bearing_object_toggle,(XtPointer)p_station);



//----- Frame for signpost info
if (Signpost_object_enabled) {

        //printf("Drawing signpost data\n");

        signpost_frame = XtVaCreateManagedWidget("Set_Del_Object signpost_frame", 
                xmFrameWidgetClass, 
                ob_form,
                XmNtopAttachment,           XmATTACH_WIDGET,
                XmNtopWidget,               ob_option_frame,
                XmNtopOffset,               10,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNleftAttachment,          XmATTACH_FORM,
                XmNleftOffset,              10,
                XmNrightAttachment,         XmATTACH_FORM,
                XmNrightOffset,             10,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        // "Signpost"
        signpost_ts  = XtVaCreateManagedWidget(langcode("POPUPOB031"),
                xmLabelWidgetClass,
                signpost_frame,
                XmNchildType,               XmFRAME_TITLE_CHILD,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        signpost_form =  XtVaCreateWidget("Set_Del_Object signpost_form",
                xmFormWidgetClass, 
                signpost_frame,
                XmNfractionBase,            5,
                XmNautoUnmanage,            FALSE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        // "Signpost Data"
        signpost_label = XtVaCreateManagedWidget(langcode("POPUPOB030"),
                xmLabelWidgetClass, 
                signpost_form,
                XmNtopAttachment,           XmATTACH_FORM,
                XmNtopOffset,               8,
                XmNbottomAttachment,        XmATTACH_FORM,
                XmNbottomOffset,            10,
                XmNleftAttachment,          XmATTACH_FORM,
                XmNleftOffset,              10,
                XmNrightAttachment,         XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        signpost_data = XtVaCreateManagedWidget("Set_Del_Object signpost_data", 
                xmTextFieldWidgetClass, 
                signpost_form,
                XmNeditable,                TRUE,
                XmNcursorPositionVisible,   TRUE,
                XmNsensitive,               TRUE,
                XmNshadowThickness,         1,
                XmNcolumns,                 3,
                XmNmaxLength,               3,
                XmNtopOffset,               3,
                XmNbackground,              colors[0x0f],
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              signpost_label,
                XmNtopAttachment,           XmATTACH_FORM,
                XmNbottomAttachment,        XmATTACH_FORM,
                XmNrightAttachment,         XmATTACH_NONE,
                NULL);

        ob_sep = XtVaCreateManagedWidget("Set_Del_Object ob_sep", 
                xmSeparatorGadgetClass,
                ob_form,
                XmNorientation,             XmHORIZONTAL,
                XmNtopAttachment,           XmATTACH_WIDGET,
                XmNtopWidget,               signpost_frame,
                XmNtopOffset,               14,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNleftAttachment,          XmATTACH_FORM,
                XmNrightAttachment,         XmATTACH_FORM,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
}



//----- Frame for area info
else if (Area_object_enabled) {

        //printf("Drawing Area data\n");

        area_frame = XtVaCreateManagedWidget("Set_Del_Object area_frame", 
                xmFrameWidgetClass, 
                ob_form,
                XmNtopAttachment,           XmATTACH_WIDGET,
                XmNtopWidget,               ob_option_frame,
                XmNtopOffset,               10,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNleftAttachment,          XmATTACH_FORM,
                XmNleftOffset,              10,
                XmNrightAttachment,         XmATTACH_FORM,
                XmNrightOffset,             10,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        // "Area Options"
        area_ts  = XtVaCreateManagedWidget(langcode("POPUPOB007"),
                xmLabelWidgetClass,
                area_frame,
                XmNchildType,               XmFRAME_TITLE_CHILD,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        area_form =  XtVaCreateWidget("Set_Del_Object area_form",
                xmFormWidgetClass, 
                area_frame,
                XmNfractionBase,            5,
                XmNautoUnmanage,            FALSE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        // "Bright Color Enable"
        bright_dim_toggle = XtVaCreateManagedWidget(langcode("POPUPOB009"),
                xmToggleButtonGadgetClass,
                area_form,
                XmNtopAttachment,           XmATTACH_FORM,
                XmNtopOffset,               8,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNleftAttachment,          XmATTACH_FORM,
                XmNleftOffset,              10,
                XmNrightAttachment,         XmATTACH_NONE,
                XmNset,                     FALSE,   // Select default to be OFF
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(bright_dim_toggle,XmNvalueChangedCallback,Area_bright_dim_toggle,"1");
        Area_bright = 0;    // Set to default each time dialog is created

        // "Color-Fill Enable"
        open_filled_toggle = XtVaCreateManagedWidget(langcode("POPUPOB010"),
                xmToggleButtonGadgetClass,
                area_form,
                XmNtopAttachment,           XmATTACH_FORM,
                XmNtopOffset,               8,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              bright_dim_toggle,
                XmNleftOffset,              10,
                XmNrightAttachment,         XmATTACH_NONE,
                XmNset,                     FALSE,   // Select default to be OFF
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(open_filled_toggle,XmNvalueChangedCallback,Area_open_filled_toggle,"1");
        Area_filled = 0;    // Set to default each time dialog is created


// Shape of object
        ac = 0;
        XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
        XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;

        shape_box = XmCreateRadioBox(area_form,
                "Set_Del_Object Shape Options box",
                al,
                ac);

        XtVaSetValues(shape_box,
                XmNpacking, XmPACK_TIGHT,
                XmNorientation, XmHORIZONTAL,
                XmNtopAttachment,XmATTACH_WIDGET,
                XmNtopWidget,  bright_dim_toggle,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 5,
                XmNrightAttachment,XmATTACH_FORM,
                XmNrightOffset, 5,
                XmNnumColumns,1,
                NULL);


        // "Circle"
        toption1 = XtVaCreateManagedWidget(langcode("POPUPOB011"),
                xmToggleButtonGadgetClass,
                shape_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(toption1,XmNvalueChangedCallback,Area_type_toggle,"0");

        // "Line-Right '/'"
        toption2 = XtVaCreateManagedWidget(langcode("POPUPOB012"),
                xmToggleButtonGadgetClass,
                shape_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(toption2,XmNvalueChangedCallback,Area_type_toggle,"1");

        // "Line-Left '\'
        toption3 = XtVaCreateManagedWidget(langcode("POPUPOB013"),
                xmToggleButtonGadgetClass,
                shape_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(toption3,XmNvalueChangedCallback,Area_type_toggle,"6");

        // "Triangle"
        toption4 = XtVaCreateManagedWidget(langcode("POPUPOB014"),
                xmToggleButtonGadgetClass,
                shape_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(toption4,XmNvalueChangedCallback,Area_type_toggle,"3");

        // "Rectangle"
        toption5 = XtVaCreateManagedWidget(langcode("POPUPOB015"),
                xmToggleButtonGadgetClass,
                shape_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(toption5,XmNvalueChangedCallback,Area_type_toggle,"4");


// Color of object
        color_box = XmCreateRadioBox(area_form,
                "Set_Del_Object Color Options box",
                al,
                ac);

        XtVaSetValues(color_box,
                XmNpacking, XmPACK_TIGHT,
                XmNorientation, XmHORIZONTAL,
                XmNtopAttachment,XmATTACH_WIDGET,
                XmNtopWidget, shape_box,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 5,
                XmNrightAttachment,XmATTACH_FORM,
                XmNrightOffset, 5,
                NULL);


        XtVaSetValues(color_box,
                XmNnumColumns,4,
                NULL);

        // "Black"
        coption1 = XtVaCreateManagedWidget(langcode("POPUPOB016"),
                xmToggleButtonGadgetClass,
                color_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(coption1,XmNvalueChangedCallback,Area_color_toggle,"/0");

        // "Blue"
        coption2 = XtVaCreateManagedWidget(langcode("POPUPOB017"),
                xmToggleButtonGadgetClass,
                color_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(coption2,XmNvalueChangedCallback,Area_color_toggle,"/1");

        // "Green"
        coption3 = XtVaCreateManagedWidget(langcode("POPUPOB018"),
                xmToggleButtonGadgetClass,
                color_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(coption3,XmNvalueChangedCallback,Area_color_toggle,"/2");

        // "Cyan"
        coption4 = XtVaCreateManagedWidget(langcode("POPUPOB019"),
                xmToggleButtonGadgetClass,
                color_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(coption4,XmNvalueChangedCallback,Area_color_toggle,"/3");

        // "Red"
        coption5 = XtVaCreateManagedWidget(langcode("POPUPOB020"),
                xmToggleButtonGadgetClass,
                color_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(coption5,XmNvalueChangedCallback,Area_color_toggle,"/4");

        // "Violet"
        coption6 = XtVaCreateManagedWidget(langcode("POPUPOB021"),
                xmToggleButtonGadgetClass,
                color_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(coption6,XmNvalueChangedCallback,Area_color_toggle,"/5");

        // "Yellow"
        coption7 = XtVaCreateManagedWidget(langcode("POPUPOB022"),
                xmToggleButtonGadgetClass,
                color_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(coption7,XmNvalueChangedCallback,Area_color_toggle,"/6");

        // "Grey"
        coption8 = XtVaCreateManagedWidget(langcode("POPUPOB023"),
                xmToggleButtonGadgetClass,
                color_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(coption8,XmNvalueChangedCallback,Area_color_toggle,"/7");

// Latitude offset
        // "Offset Up"
        ob_lat_offset = XtVaCreateManagedWidget(langcode("POPUPOB024"),
                xmLabelWidgetClass, 
                area_form,
                XmNtopAttachment,           XmATTACH_WIDGET,
                XmNtopWidget,               color_box,
                XmNtopOffset,               10,
                XmNbottomAttachment,        XmATTACH_FORM,
                XmNbottomOffset,            10,
                XmNleftAttachment,          XmATTACH_FORM,
                XmNleftOffset,              10,
                XmNrightAttachment,         XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        ob_lat_offset_data = XtVaCreateManagedWidget("Set_Del_Object lat offset", 
                xmTextFieldWidgetClass, 
                area_form,
                XmNeditable,                TRUE,
                XmNcursorPositionVisible,   TRUE,
                XmNsensitive,               TRUE,
                XmNshadowThickness,         1,
                XmNcolumns,                 4,
                XmNmaxLength,               4,
                XmNtopOffset,               5,
                XmNbackground,              colors[0x0f],
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              ob_lat_offset,
                XmNtopAttachment,           XmATTACH_WIDGET,
                XmNtopWidget,               color_box,
                XmNbottomAttachment,        XmATTACH_FORM,
                XmNrightAttachment,         XmATTACH_NONE,
                NULL);

// Longitude offset
        // "Offset Left (except for '/')"
        ob_lon_offset = XtVaCreateManagedWidget(langcode("POPUPOB025"),
                xmLabelWidgetClass, 
                area_form,
                XmNtopAttachment,           XmATTACH_WIDGET,
                XmNtopWidget,               color_box,
                XmNtopOffset,               10,
                XmNbottomAttachment,        XmATTACH_FORM,
                XmNbottomOffset,            10,
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              ob_lat_offset_data,
                XmNleftOffset,              10,
                XmNrightAttachment,         XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        ob_lon_offset_data = XtVaCreateManagedWidget("Set_Del_Object long offset", 
                xmTextFieldWidgetClass, 
                area_form,
                XmNeditable,                TRUE,
                XmNcursorPositionVisible,   TRUE,
                XmNsensitive,               TRUE,
                XmNshadowThickness,         1,
                XmNcolumns,                 4,
                XmNmaxLength,               4,
                XmNtopOffset,               5,
                XmNbackground,              colors[0x0f],
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              ob_lon_offset,
                XmNtopAttachment,           XmATTACH_WIDGET,
                XmNtopWidget,               color_box,
                XmNbottomAttachment,        XmATTACH_FORM,
                XmNrightAttachment,         XmATTACH_NONE,
                NULL);

        // "Corridor (Lines only)"
        ob_corridor = XtVaCreateManagedWidget(langcode("POPUPOB026"),
                xmLabelWidgetClass, 
                area_form,
                XmNtopAttachment,           XmATTACH_WIDGET,
                XmNtopWidget,               color_box,
                XmNtopOffset,               10,
                XmNbottomAttachment,        XmATTACH_FORM,
                XmNbottomOffset,            10,
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              ob_lon_offset_data,
                XmNleftOffset,              10,
                XmNrightAttachment,         XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        ob_corridor_data = XtVaCreateManagedWidget("Set_Del_Object lat offset", 
                xmTextFieldWidgetClass, 
                area_form,
                XmNeditable,                TRUE,
                XmNcursorPositionVisible,   TRUE,
                XmNsensitive,               TRUE,
                XmNshadowThickness,         1,
                XmNcolumns,                 3,
                XmNmaxLength,               3,
                XmNtopOffset,               5,
                XmNbackground,              colors[0x0f],
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              ob_corridor,
                XmNtopAttachment,           XmATTACH_WIDGET,
                XmNtopWidget,               color_box,
                XmNbottomAttachment,        XmATTACH_FORM,
                XmNrightAttachment,         XmATTACH_NONE,
                NULL);

        // "Miles"
        ob_corridor_miles = XtVaCreateManagedWidget(langcode("UNIOP00004"),
                xmLabelWidgetClass, 
                area_form,
                XmNtopAttachment,           XmATTACH_WIDGET,
                XmNtopWidget,               color_box,
                XmNtopOffset,               10,
                XmNbottomAttachment,        XmATTACH_FORM,
                XmNbottomOffset,            10,
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              ob_corridor_data,
                XmNrightAttachment,         XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtSetSensitive(ob_corridor,FALSE);
        XtSetSensitive(ob_corridor_data,FALSE);
        XtSetSensitive(ob_corridor_miles,FALSE);


        ob_sep = XtVaCreateManagedWidget("Set_Del_Object ob_sep", 
                xmSeparatorGadgetClass,
                ob_form,
                XmNorientation,             XmHORIZONTAL,
                XmNtopAttachment,           XmATTACH_WIDGET,
                XmNtopWidget,               area_frame,
                XmNtopOffset,               14,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNleftAttachment,          XmATTACH_FORM,
                XmNrightAttachment,         XmATTACH_FORM,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
}



//----- Frame for DF-omni info
else if (DF_object_enabled) {

        //printf("Drawing DF data\n");

        // "Omni Antenna"
        omni_antenna_toggle = XtVaCreateManagedWidget(langcode("POPUPOB041"),
                xmToggleButtonGadgetClass,
                ob_form,
                XmNtopAttachment,           XmATTACH_WIDGET,
                XmNtopWidget,               signpost_toggle,
                XmNtopOffset,               0,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNbottomOffset,            0,
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              area_toggle,
                XmNleftOffset,              10,
                XmNrightAttachment,         XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(omni_antenna_toggle,XmNvalueChangedCallback,Omni_antenna_toggle,(XtPointer)p_station);


        // "Beam Antenna"
        beam_antenna_toggle = XtVaCreateManagedWidget(langcode("POPUPOB042"),
                xmToggleButtonGadgetClass,
                ob_form,
                XmNtopAttachment,           XmATTACH_WIDGET,
                XmNtopWidget,               omni_antenna_toggle,
                XmNtopOffset,               0,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNbottomOffset,            0,
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              area_toggle,
                XmNleftOffset,              10,
                XmNrightAttachment,         XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(beam_antenna_toggle,XmNvalueChangedCallback,Beam_antenna_toggle,(XtPointer)p_station);


        frameomni = XtVaCreateManagedWidget("Set_Del_Object frameomni", 
                xmFrameWidgetClass, 
                ob_form,
                XmNtopAttachment,           XmATTACH_WIDGET,
                XmNtopWidget,               ob_option_frame,
                XmNtopOffset,               10,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNleftAttachment,          XmATTACH_FORM,
                XmNleftOffset,              10,
                XmNrightAttachment,         XmATTACH_FORM,
                XmNrightOffset,             10,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        omnilabel  = XtVaCreateManagedWidget(langcode("POPUPOB039"),
                xmLabelWidgetClass,
                frameomni,
                XmNchildType,               XmFRAME_TITLE_CHILD,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        formomni =  XtVaCreateWidget("Set_Del_Object formomni",
                xmFormWidgetClass,
                frameomni,
                XmNfractionBase,            5,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);


        // Power
        ac = 0;
        XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
        XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;

        signal_box = XmCreateRadioBox(formomni,
                "Set_Del_Object Power Radio Box",
                al,
                ac);

        XtVaSetValues(signal_box,
                XmNpacking,               XmPACK_TIGHT,
                XmNorientation,           XmHORIZONTAL,
                XmNtopAttachment,         XmATTACH_FORM,
                XmNtopOffset,             5,
                XmNbottomAttachment,      XmATTACH_NONE,
                XmNleftAttachment,        XmATTACH_FORM,
                XmNleftOffset,            5,
                XmNrightAttachment,       XmATTACH_FORM,
                XmNrightOffset,           5,
                XmNnumColumns,            11,
                NULL);

        // No signal detected what-so-ever
        soption0 = XtVaCreateManagedWidget("0",
                xmToggleButtonGadgetClass,
                signal_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(soption0,XmNvalueChangedCallback,Ob_signal_toggle,"0");

        // Detectible signal (Maybe)
        soption1 = XtVaCreateManagedWidget("1",
                xmToggleButtonGadgetClass,
                signal_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(soption1,XmNvalueChangedCallback,Ob_signal_toggle,"1");

        // Detectible signal (certain but not copyable)
        soption2 = XtVaCreateManagedWidget("2",
                xmToggleButtonGadgetClass,
                signal_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(soption2,XmNvalueChangedCallback,Ob_signal_toggle,"2");

        // Weak signal marginally readable
        soption3 = XtVaCreateManagedWidget("3",
                xmToggleButtonGadgetClass,
                signal_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(soption3,XmNvalueChangedCallback,Ob_signal_toggle,"3");

        // Noisy but copyable
        soption4 = XtVaCreateManagedWidget("4",
                xmToggleButtonGadgetClass,
                signal_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(soption4,XmNvalueChangedCallback,Ob_signal_toggle,"4");

        // Some noise but easy to copy
        soption5 = XtVaCreateManagedWidget("5",
                xmToggleButtonGadgetClass,
                signal_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(soption5,XmNvalueChangedCallback,Ob_signal_toggle,"5");

        // Good signal with detectible noise
        soption6 = XtVaCreateManagedWidget("6",
                xmToggleButtonGadgetClass,
                signal_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(soption6,XmNvalueChangedCallback,Ob_signal_toggle,"6");

        // Near full-quieting signal
        soption7 = XtVaCreateManagedWidget("7",
                xmToggleButtonGadgetClass,
                signal_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(soption7,XmNvalueChangedCallback,Ob_signal_toggle,"7");

        // Dead full-quieting signal, no noise detectible
        soption8 = XtVaCreateManagedWidget("8",
                xmToggleButtonGadgetClass,
                signal_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(soption8,XmNvalueChangedCallback,Ob_signal_toggle,"8");

        // Extremely strong signal "pins the meter"
        soption9 = XtVaCreateManagedWidget("9",
                xmToggleButtonGadgetClass,
                signal_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(soption9,XmNvalueChangedCallback,Ob_signal_toggle,"9");


        // Height
        height_box = XmCreateRadioBox(formomni,
                "Set_Del_Object Height Radio Box",
                al,
                ac);

        XtVaSetValues(height_box,
                XmNpacking, XmPACK_TIGHT,
                XmNorientation, XmHORIZONTAL,
                XmNtopAttachment,XmATTACH_WIDGET,
                XmNtopWidget,signal_box,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 5,
                XmNrightAttachment,XmATTACH_FORM,
                XmNrightOffset, 5,
                XmNnumColumns,10,
                NULL);


        // 10 Feet
        hoption0 = XtVaCreateManagedWidget("10ft",
                xmToggleButtonGadgetClass,
                height_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(hoption0,XmNvalueChangedCallback,Ob_height_toggle,"0");

        // 20 Feet
        hoption1 = XtVaCreateManagedWidget("20ft",
                xmToggleButtonGadgetClass,
                height_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(hoption1,XmNvalueChangedCallback,Ob_height_toggle,"1");

        // 40 Feet
        hoption2 = XtVaCreateManagedWidget("40ft",
                xmToggleButtonGadgetClass,
                height_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(hoption2,XmNvalueChangedCallback,Ob_height_toggle,"2");

        // 80 Feet
        hoption3 = XtVaCreateManagedWidget("80ft",
                xmToggleButtonGadgetClass,
                height_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(hoption3,XmNvalueChangedCallback,Ob_height_toggle,"3");

        // 160 Feet
        hoption4 = XtVaCreateManagedWidget("160ft",
                xmToggleButtonGadgetClass,
                height_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(hoption4,XmNvalueChangedCallback,Ob_height_toggle,"4");

        // 320 Feet
        hoption5 = XtVaCreateManagedWidget("320ft",
                xmToggleButtonGadgetClass,
                height_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(hoption5,XmNvalueChangedCallback,Ob_height_toggle,"5");

        // 640 Feet
        hoption6 = XtVaCreateManagedWidget("640ft",
                xmToggleButtonGadgetClass,
                height_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(hoption6,XmNvalueChangedCallback,Ob_height_toggle,"6");

        // 1280 Feet
        hoption7 = XtVaCreateManagedWidget("1280ft",
                xmToggleButtonGadgetClass,
                height_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(hoption7,XmNvalueChangedCallback,Ob_height_toggle,"7");

        // 2560 Feet
        hoption8 = XtVaCreateManagedWidget("2560ft",
                xmToggleButtonGadgetClass,
                height_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(hoption8,XmNvalueChangedCallback,Ob_height_toggle,"8");

        // 5120 Feet
        hoption9 = XtVaCreateManagedWidget("5120ft",
                xmToggleButtonGadgetClass,
                height_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(hoption9,XmNvalueChangedCallback,Ob_height_toggle,"9");


        // Gain
        gain_box = XmCreateRadioBox(formomni,
                "Set_Del_Object Gain Radio Box",
                al,
                ac);

        XtVaSetValues(gain_box,
                XmNpacking, XmPACK_TIGHT,
                XmNorientation, XmHORIZONTAL,
                XmNtopAttachment,XmATTACH_WIDGET,
                XmNtopWidget,height_box,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 5,
                XmNrightAttachment,XmATTACH_FORM,
                XmNrightOffset, 5,
                XmNnumColumns,10,
                NULL);


        // 0 dB
        goption0 = XtVaCreateManagedWidget("0dB",
                xmToggleButtonGadgetClass,
                gain_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(goption0,XmNvalueChangedCallback,Ob_gain_toggle,"0");

        // 1 dB
        goption1 = XtVaCreateManagedWidget("1dB",
                xmToggleButtonGadgetClass,
                gain_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(goption1,XmNvalueChangedCallback,Ob_gain_toggle,"1");

        // 2 dB
        goption2 = XtVaCreateManagedWidget("2dB",
                xmToggleButtonGadgetClass,
                gain_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(goption2,XmNvalueChangedCallback,Ob_gain_toggle,"2");

        // 3 dB
        goption3 = XtVaCreateManagedWidget("3dB",
                xmToggleButtonGadgetClass,
                gain_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(goption3,XmNvalueChangedCallback,Ob_gain_toggle,"3");

        // 4 dB
        goption4 = XtVaCreateManagedWidget("4dB",
                xmToggleButtonGadgetClass,
                gain_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(goption4,XmNvalueChangedCallback,Ob_gain_toggle,"4");

        // 5 dB
        goption5 = XtVaCreateManagedWidget("5dB",
                xmToggleButtonGadgetClass,
                gain_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(goption5,XmNvalueChangedCallback,Ob_gain_toggle,"5");

        // 6 dB
        goption6 = XtVaCreateManagedWidget("6dB",
                xmToggleButtonGadgetClass,
                gain_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(goption6,XmNvalueChangedCallback,Ob_gain_toggle,"6");

        // 7 dB
        goption7 = XtVaCreateManagedWidget("7dB",
                xmToggleButtonGadgetClass,
                gain_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(goption7,XmNvalueChangedCallback,Ob_gain_toggle,"7");

        // 8 dB
        goption8 = XtVaCreateManagedWidget("8dB",
                xmToggleButtonGadgetClass,
                gain_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(goption8,XmNvalueChangedCallback,Ob_gain_toggle,"8");

        // 9 dB
        goption9 = XtVaCreateManagedWidget("9dB",
                xmToggleButtonGadgetClass,
                gain_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(goption9,XmNvalueChangedCallback,Ob_gain_toggle,"9");


        // Gain
        directivity_box = XmCreateRadioBox(formomni,
                "Set_Del_Object Directivity Radio Box",
                al,
                ac);

        XtVaSetValues(directivity_box,
                XmNpacking, XmPACK_TIGHT,
                XmNorientation, XmHORIZONTAL,
                XmNtopAttachment,XmATTACH_WIDGET,
                XmNtopWidget,gain_box,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 5,
                XmNrightAttachment,XmATTACH_FORM,
                XmNrightOffset, 5,
                XmNnumColumns,10,
                NULL);


        // Omni-directional
        doption0 = XtVaCreateManagedWidget(langcode("WPUPCFS016"),
                xmToggleButtonGadgetClass,
                directivity_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(doption0,XmNvalueChangedCallback,Ob_directivity_toggle,"0");

        // 45 NE
        doption1 = XtVaCreateManagedWidget("45°",
                xmToggleButtonGadgetClass,
                directivity_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(doption1,XmNvalueChangedCallback,Ob_directivity_toggle,"1");

        // 90 E
        doption2 = XtVaCreateManagedWidget("90°",
                xmToggleButtonGadgetClass,
                directivity_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(doption2,XmNvalueChangedCallback,Ob_directivity_toggle,"2");

        // 135 SE
        doption3 = XtVaCreateManagedWidget("135°",
                xmToggleButtonGadgetClass,
                directivity_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(doption3,XmNvalueChangedCallback,Ob_directivity_toggle,"3");

        // 180 S
        doption4 = XtVaCreateManagedWidget("180°",
                xmToggleButtonGadgetClass,
                directivity_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(doption4,XmNvalueChangedCallback,Ob_directivity_toggle,"4");

        // 225 SW
        doption5 = XtVaCreateManagedWidget("225°",
                xmToggleButtonGadgetClass,
                directivity_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(doption5,XmNvalueChangedCallback,Ob_directivity_toggle,"5");

        // 270 W
        doption6 = XtVaCreateManagedWidget("270°",
                xmToggleButtonGadgetClass,
                directivity_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(doption6,XmNvalueChangedCallback,Ob_directivity_toggle,"6");

        // 315 NW
        doption7 = XtVaCreateManagedWidget("315°",
                xmToggleButtonGadgetClass,
                directivity_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(doption7,XmNvalueChangedCallback,Ob_directivity_toggle,"7");

        // 360 N
        doption8 = XtVaCreateManagedWidget("360°",
                xmToggleButtonGadgetClass,
                directivity_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(doption8,XmNvalueChangedCallback,Ob_directivity_toggle,"8");


//----- Frame for DF-beam info
        framebeam = XtVaCreateManagedWidget("Set_Del_Object framebeam", 
                xmFrameWidgetClass, 
                ob_form,
                XmNtopAttachment,           XmATTACH_WIDGET,
                XmNtopWidget,               frameomni,
                XmNtopOffset,               10,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNleftAttachment,          XmATTACH_FORM,
                XmNleftOffset,              10,
                XmNrightAttachment,         XmATTACH_FORM,
                XmNrightOffset,             10,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        beamlabel  = XtVaCreateManagedWidget(langcode("POPUPOB040"),
                xmLabelWidgetClass,
                framebeam,
                XmNchildType,               XmFRAME_TITLE_CHILD,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        formbeam =  XtVaCreateWidget("Set_Del_Object formbeam",
                xmFormWidgetClass,
                framebeam,
                XmNfractionBase,            5,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);


        // Beam width
        ac = 0;
        XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
        XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;

        width_box = XmCreateRadioBox(formbeam,
                "Set_Del_Object Width Box",
                al,
                ac);

        XtVaSetValues(width_box,
                XmNpacking,               XmPACK_TIGHT,
                XmNorientation,           XmHORIZONTAL,
                XmNtopAttachment,         XmATTACH_FORM,
                XmNtopOffset,             5,
                XmNbottomAttachment,      XmATTACH_NONE,
                XmNleftAttachment,        XmATTACH_FORM,
                XmNleftOffset,            5,
                XmNrightAttachment,       XmATTACH_FORM,
                XmNrightOffset,           5,
                XmNnumColumns,            11,
                NULL);

        // Useless
        woption0 = XtVaCreateManagedWidget("Useless",
                xmToggleButtonGadgetClass,
                width_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(woption0,XmNvalueChangedCallback,Ob_width_toggle,"0");

        // < 240 Degrees
        woption1 = XtVaCreateManagedWidget("<240°",
                xmToggleButtonGadgetClass,
                width_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(woption1,XmNvalueChangedCallback,Ob_width_toggle,"1");

        // < 120 Degrees
        woption2 = XtVaCreateManagedWidget("<120°",
                xmToggleButtonGadgetClass,
                width_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(woption2,XmNvalueChangedCallback,Ob_width_toggle,"2");

        // < 64 Degrees
        woption3 = XtVaCreateManagedWidget("<64°",
                xmToggleButtonGadgetClass,
                width_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(woption3,XmNvalueChangedCallback,Ob_width_toggle,"3");

        // < 32 Degrees
        woption4 = XtVaCreateManagedWidget("<32°",
                xmToggleButtonGadgetClass,
                width_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(woption4,XmNvalueChangedCallback,Ob_width_toggle,"4");

        // < 16 Degrees
        woption5 = XtVaCreateManagedWidget("<16°",
                xmToggleButtonGadgetClass,
                width_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(woption5,XmNvalueChangedCallback,Ob_width_toggle,"5");

        // < 8 Degrees
        woption6 = XtVaCreateManagedWidget("<8°",
                xmToggleButtonGadgetClass,
                width_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(woption6,XmNvalueChangedCallback,Ob_width_toggle,"6");

        // < 4 Degrees
        woption7 = XtVaCreateManagedWidget("<4°",
                xmToggleButtonGadgetClass,
                width_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(woption7,XmNvalueChangedCallback,Ob_width_toggle,"7");

        // < 2 Degrees
        woption8 = XtVaCreateManagedWidget("<2°",
                xmToggleButtonGadgetClass,
                width_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(woption8,XmNvalueChangedCallback,Ob_width_toggle,"8");

        // < 1 Degrees
        woption9 = XtVaCreateManagedWidget("<1°",
                xmToggleButtonGadgetClass,
                width_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(woption9,XmNvalueChangedCallback,Ob_width_toggle,"9");


        // "Bearing"
        ob_bearing = XtVaCreateManagedWidget(langcode("WPUPSTI058"),
                xmLabelWidgetClass, 
                formbeam,
                XmNtopAttachment,           XmATTACH_WIDGET,
                XmNtopWidget,               width_box,
                XmNtopOffset,               10,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNleftAttachment,          XmATTACH_FORM,
                XmNleftOffset,              10,
                XmNrightAttachment,         XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        // Bearing data
        ob_bearing_data = XtVaCreateManagedWidget("Set_Del_Object ob_bearing_data", 
                xmTextFieldWidgetClass, 
                formbeam,
                XmNeditable,                TRUE,
                XmNcursorPositionVisible,   TRUE,
                XmNsensitive,               TRUE,
                XmNshadowThickness,         1,
                XmNcolumns,                 9,
                XmNmaxLength,               9,
                XmNtopAttachment,           XmATTACH_WIDGET,
                XmNtopWidget,               width_box,
                XmNtopOffset,               5,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              ob_bearing,
                XmNrightAttachment,         XmATTACH_NONE,
                XmNbackground,              colors[0x0f],
                NULL);


        XtSetSensitive(frameomni,FALSE);
        XtSetSensitive(framebeam,FALSE);
        Omni_antenna_enabled = 0;
        Beam_antenna_enabled = 0;


        ob_sep = XtVaCreateManagedWidget("Set_Del_Object ob_sep", 
                xmSeparatorGadgetClass,
                ob_form,
                XmNorientation,             XmHORIZONTAL,
                XmNtopAttachment,           XmATTACH_WIDGET,
                XmNtopWidget,               framebeam,
                XmNtopOffset,               14,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNleftAttachment,          XmATTACH_FORM,
                XmNrightAttachment,         XmATTACH_FORM,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
}
// End of DF-specific widgets



//----- No Special options selected.  We need a widget here for the next widget to attach to.
        if (!DF_object_enabled && !Area_object_enabled && !Signpost_object_enabled) {

            //printf("No special object types\n");

            ob_sep = XtVaCreateManagedWidget("Set_Del_Object ob_sep", 
                    xmSeparatorGadgetClass,
                    ob_form,
                    XmNorientation,             XmHORIZONTAL,
                    XmNtopAttachment,           XmATTACH_WIDGET,
                    XmNtopWidget,               ob_option_frame,
                    XmNtopOffset,               14,
                    XmNbottomAttachment,        XmATTACH_NONE,
                    XmNleftAttachment,          XmATTACH_FORM,
                    XmNrightAttachment,         XmATTACH_FORM,
                    MY_FOREGROUND_COLOR,
                    MY_BACKGROUND_COLOR,
                    NULL);
        }



//----- Comment Field
        // "Comment:"
        ob_comment = XtVaCreateManagedWidget(langcode("WPUPCFS017"),
                xmLabelWidgetClass, 
                ob_form,
                XmNtopAttachment,           XmATTACH_WIDGET,
                XmNtopWidget,               ob_sep,
                XmNtopOffset,               10,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNleftAttachment,          XmATTACH_FORM,
                XmNleftOffset,              10,
                XmNrightAttachment,         XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        object_comment_data = XtVaCreateManagedWidget("Set_Del_Object comment", 
                xmTextFieldWidgetClass, 
                ob_form,
                XmNeditable,                TRUE,
                XmNcursorPositionVisible,   TRUE,
                XmNsensitive,               TRUE,
                XmNshadowThickness,         1,
                XmNcolumns,                 43,     // max 43 without Data Extension
                XmNmaxLength,               43,
                XmNtopOffset,               6,
                XmNbackground,              colors[0x0f],
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              ob_comment,
                XmNleftOffset,              5,
                XmNtopAttachment,           XmATTACH_WIDGET,
                XmNtopWidget,               ob_sep,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNrightAttachment,         XmATTACH_NONE,
                NULL);


        ob_sep2 = XtVaCreateManagedWidget("Set_Del_Object ob_sep2", 
                xmSeparatorGadgetClass,
                ob_form,
                XmNorientation,             XmHORIZONTAL,
                XmNtopAttachment,           XmATTACH_WIDGET,
                XmNtopWidget,               ob_comment,
                XmNtopOffset,               14,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNleftAttachment,          XmATTACH_FORM,
                XmNrightAttachment,         XmATTACH_FORM,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

//----- Buttons
        if (p_station != NULL) {  // We were called from the Modify_object() or Move function

            // Change the buttons/callbacks based on whether we're dealing with an item or an object
            if ((p_station->flag & ST_ITEM) != 0) {     // Modifying an Item
                // Here we need Modify Item/Delete Item/Cancel buttons
                ob_button_set = XtVaCreateManagedWidget(langcode("POPUPOB034"),
                        xmPushButtonGadgetClass, 
                        ob_form,
                        XmNtopAttachment,           XmATTACH_WIDGET,
                        XmNtopWidget,               ob_sep2,
                        XmNtopOffset,               5,
                        XmNbottomAttachment,        XmATTACH_FORM,
                        XmNbottomOffset,            5,
                        XmNleftAttachment,          XmATTACH_POSITION,
                        XmNleftPosition,            0,
                        XmNrightAttachment,         XmATTACH_POSITION,
                        XmNrightPosition,           1,
                        XmNnavigationType,          XmTAB_GROUP,
                        MY_FOREGROUND_COLOR,
                        MY_BACKGROUND_COLOR,
                        NULL);

                ob_button_del = XtVaCreateManagedWidget(langcode("POPUPOB033"),
                        xmPushButtonGadgetClass, 
                        ob_form,
                        XmNtopAttachment,           XmATTACH_WIDGET,
                        XmNtopWidget,               ob_sep2,
                        XmNtopOffset,               5,
                        XmNbottomAttachment,        XmATTACH_FORM,
                        XmNbottomOffset,            5,
                        XmNleftAttachment,          XmATTACH_POSITION,
                        XmNleftPosition,            1,
                        XmNrightAttachment,         XmATTACH_POSITION,
                        XmNrightPosition,           2,
                        XmNnavigationType,          XmTAB_GROUP,
                        MY_FOREGROUND_COLOR,
                        MY_BACKGROUND_COLOR,
                        NULL);
                XtAddCallback(ob_button_set, XmNactivateCallback, Item_change_data_set, object_dialog);
                XtAddCallback(ob_button_del, XmNactivateCallback, Item_change_data_del, object_dialog);
            }
            else {  // Modifying an Object
                // Here we need Modify Object/Delete Object/Cancel buttons
                ob_button_set = XtVaCreateManagedWidget(langcode("POPUPOB005"),
                        xmPushButtonGadgetClass, 
                        ob_form,
                        XmNtopAttachment,           XmATTACH_WIDGET,
                        XmNtopWidget,               ob_sep2,
                        XmNtopOffset,               5,
                        XmNbottomAttachment,        XmATTACH_FORM,
                        XmNbottomOffset,            5,
                        XmNleftAttachment,          XmATTACH_POSITION,
                        XmNleftPosition,            0,
                        XmNrightAttachment,         XmATTACH_POSITION,
                        XmNrightPosition,           1,
                        XmNnavigationType,          XmTAB_GROUP,
                        MY_FOREGROUND_COLOR,
                        MY_BACKGROUND_COLOR,
                        NULL);

                ob_button_del = XtVaCreateManagedWidget(langcode("POPUPOB004"),
                        xmPushButtonGadgetClass, 
                        ob_form,
                        XmNtopAttachment,           XmATTACH_WIDGET,
                        XmNtopWidget,               ob_sep2,
                        XmNtopOffset,               5,
                        XmNbottomAttachment,        XmATTACH_FORM,
                        XmNbottomOffset,            5,
                        XmNleftAttachment,          XmATTACH_POSITION,
                        XmNleftPosition,            1,
                        XmNrightAttachment,         XmATTACH_POSITION,
                        XmNrightPosition,           2,
                        XmNnavigationType,          XmTAB_GROUP,
                        MY_FOREGROUND_COLOR,
                        MY_BACKGROUND_COLOR,
                        NULL);
               XtAddCallback(ob_button_set, XmNactivateCallback, Object_change_data_set, object_dialog);
               XtAddCallback(ob_button_del, XmNactivateCallback, Object_change_data_del, object_dialog);
            }
        }
        else {  // We were called from Create->Object mouse menu
            ob_button_set = XtVaCreateManagedWidget(langcode("POPUPOB003"),xmPushButtonGadgetClass, ob_form,
                    XmNtopAttachment,           XmATTACH_WIDGET,
                    XmNtopWidget,               ob_sep2,
                    XmNtopOffset,               5,
                    XmNbottomAttachment,        XmATTACH_FORM,
                    XmNbottomOffset,            5,
                    XmNleftAttachment,          XmATTACH_POSITION,
                    XmNleftPosition,            0,
                    XmNrightAttachment,         XmATTACH_POSITION,
                    XmNrightPosition,           1,
                    XmNnavigationType,          XmTAB_GROUP,
                    MY_FOREGROUND_COLOR,
                    MY_BACKGROUND_COLOR,
                    NULL);

            it_button_set = XtVaCreateManagedWidget(langcode("POPUPOB006"),xmPushButtonGadgetClass, ob_form,
                    XmNtopAttachment,           XmATTACH_WIDGET,
                    XmNtopWidget,               ob_sep2,
                    XmNtopOffset,               5,
                    XmNbottomAttachment,        XmATTACH_FORM,
                    XmNbottomOffset,            5,
                    XmNleftAttachment,          XmATTACH_POSITION,
                    XmNleftPosition,            1,
                    XmNrightAttachment,         XmATTACH_POSITION,
                    XmNrightPosition,           2,
                    XmNnavigationType,          XmTAB_GROUP,
                    MY_FOREGROUND_COLOR,
                    MY_BACKGROUND_COLOR,
                    NULL);

            XtAddCallback(ob_button_set, XmNactivateCallback, Object_change_data_set, object_dialog);
            XtAddCallback(it_button_set, XmNactivateCallback,   Item_change_data_set, object_dialog);
        }
        
        ob_button_cancel = XtVaCreateManagedWidget(langcode("UNIOP00002"),xmPushButtonGadgetClass, ob_form,
                XmNtopAttachment,           XmATTACH_WIDGET,
                XmNtopWidget,               ob_sep2,
                XmNtopOffset,               5,
                XmNbottomAttachment,        XmATTACH_FORM,
                XmNbottomOffset,            5,
                XmNleftAttachment,          XmATTACH_POSITION,
                XmNleftPosition,            2,
                XmNrightAttachment,         XmATTACH_POSITION,
                XmNrightPosition,           3,
                XmNnavigationType,          XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(ob_button_cancel, XmNactivateCallback, Object_destroy_shell,   object_dialog);


        // Set ToggleButtons for the current state.  Don't notify the callback
        // functions associated with them or we'll be in an infinite loop.
        if (Area_object_enabled)
            XmToggleButtonSetState(area_toggle, TRUE, FALSE);
        if (Signpost_object_enabled)
            XmToggleButtonSetState(signpost_toggle, TRUE, FALSE);
        if (DF_object_enabled)
            XmToggleButtonSetState(df_bearing_toggle, TRUE, FALSE);


// Fill in current data if object already exists
        if (p_station != NULL) {  // We were called from the Modify_object() or Move functions

            // Don't allow changing types if the object is already created.
            // Already tried allowing it, and it causes other problems.
            XtSetSensitive(area_toggle, FALSE);
            XtSetSensitive(signpost_toggle, FALSE);
            XtSetSensitive(df_bearing_toggle, FALSE);

            XmTextFieldSetString(object_name_data,p_station->call_sign);
            // Need to make the above field non-editable 'cuz we're trying to modify
            // _parameters_ of the object and the name has to stay the same in order
            // to do this.  Change the name and we'll be creating a new object instead
            // of modifying an old one.
            XtSetSensitive(object_name_data, FALSE);
            // Would be nice to change the colors too

            temp_data[0] = p_station->aprs_symbol.aprs_type;
            temp_data[1] = '\0';
            XmTextFieldSetString(object_group_data,temp_data);

            temp_data[0] = p_station->aprs_symbol.aprs_symbol;
            temp_data[1] = '\0';
            XmTextFieldSetString(object_symbol_data,temp_data);

            // We only check the first possible comment string in
            // the record
            //if (strlen(p_station->comments) > 0)
            if ( (p_station->comment_data != NULL)
                    && (p_station->comment_data->text_ptr != NULL) ) {
                //XmTextFieldSetString(object_comment_data,p_station->comments);
                XmTextFieldSetString(object_comment_data,p_station->comment_data->text_ptr);
            }
            else {
                XmTextFieldSetString(object_comment_data,"");
            }


//            if ( (p_station->aprs_symbol.area_object.type != AREA_NONE) // Found an area object
//                    && Area_object_enabled ) {
if (Area_object_enabled) {

                XtSetSensitive(ob_frame,FALSE);
                XtSetSensitive(area_frame,TRUE);

                switch (p_station->aprs_symbol.area_object.type) {
                    case (1):   // Line '/'
                        XmToggleButtonSetState(toption2, TRUE, TRUE);
                        XmToggleButtonGadgetSetState(open_filled_toggle, FALSE, TRUE);
                        if (p_station->aprs_symbol.area_object.corridor_width > 0)
                            xastir_snprintf(temp_data, sizeof(temp_data), "%d",
                                    p_station->aprs_symbol.area_object.corridor_width );
                        else
                            temp_data[0] = '\0';    // Empty string

                        XmTextFieldSetString( ob_corridor_data, temp_data );
                        break;
                    case (6):   // Line '\'
                        XmToggleButtonGadgetSetState(toption3, TRUE, TRUE);
                        XmToggleButtonGadgetSetState(open_filled_toggle, FALSE, TRUE);
                        if (p_station->aprs_symbol.area_object.corridor_width > 0)
                            xastir_snprintf(temp_data, sizeof(temp_data), "%d",
                                    p_station->aprs_symbol.area_object.corridor_width );
                        else
                            temp_data[0] = '\0';    // Empty string

                        XmTextFieldSetString( ob_corridor_data, temp_data );
                        break;
                    case (3):   // Open Triangle
                        XmToggleButtonGadgetSetState(toption4, TRUE, TRUE);
                        XmToggleButtonGadgetSetState(open_filled_toggle, FALSE, TRUE);
                        break;
                    case (4):   // Open Rectangle
                        XmToggleButtonGadgetSetState(toption5, TRUE, TRUE);
                        XmToggleButtonGadgetSetState(open_filled_toggle, FALSE, TRUE);
                        break;
                    case (5):   // Filled Circle
                        XmToggleButtonGadgetSetState(toption1, TRUE, TRUE);
                        XmToggleButtonGadgetSetState(open_filled_toggle, TRUE, TRUE);
                        break;
                    case (8):   // Filled Triangle
                        XmToggleButtonGadgetSetState(toption4, TRUE, TRUE);
                        XmToggleButtonGadgetSetState(open_filled_toggle, TRUE, TRUE);
                        break;
                    case (9):   // Filled Rectangle
                        XmToggleButtonGadgetSetState(toption5, TRUE, TRUE);
                        XmToggleButtonGadgetSetState(open_filled_toggle, TRUE, TRUE);
                        break;
                    case (0):   // Open Circle
                    default:
                        XmToggleButtonGadgetSetState(toption1, TRUE, TRUE);
                        XmToggleButtonGadgetSetState(open_filled_toggle, FALSE, TRUE);
                        break;
                }

                switch (p_station->aprs_symbol.area_object.color) {
                    case (1):   // Blue Bright
                        XmToggleButtonGadgetSetState(coption2, TRUE, TRUE);
                        XmToggleButtonGadgetSetState(bright_dim_toggle, TRUE, TRUE);
                        break;
                    case (2):   // Green Bright
                        XmToggleButtonGadgetSetState(coption3, TRUE, TRUE);
                        XmToggleButtonGadgetSetState(bright_dim_toggle, TRUE, TRUE);
                        break;
                    case (3):   // Cyan Bright
                        XmToggleButtonGadgetSetState(coption4, TRUE, TRUE);
                        XmToggleButtonGadgetSetState(bright_dim_toggle, TRUE, TRUE);
                        break;
                    case (4):   // Red Bright
                        XmToggleButtonGadgetSetState(coption5, TRUE, TRUE);
                        XmToggleButtonGadgetSetState(bright_dim_toggle, TRUE, TRUE);
                        break;
                    case (5):   // Violet Bright
                        XmToggleButtonGadgetSetState(coption6, TRUE, TRUE);
                        XmToggleButtonGadgetSetState(bright_dim_toggle, TRUE, TRUE);
                        break;
                    case (6):   // Yellow Bright
                        XmToggleButtonGadgetSetState(coption7, TRUE, TRUE);
                        XmToggleButtonGadgetSetState(bright_dim_toggle, TRUE, TRUE);
                        break;
                    case (7):   // Gray Bright
                        XmToggleButtonGadgetSetState(coption8, TRUE, TRUE);
                        XmToggleButtonGadgetSetState(bright_dim_toggle, TRUE, TRUE);
                        break;
                    case (8):   // Black Dim
                        XmToggleButtonGadgetSetState(coption1, TRUE, TRUE);
                        XmToggleButtonGadgetSetState(bright_dim_toggle, FALSE, TRUE);
                        break;
                    case (9):   // Blue Dim
                        XmToggleButtonGadgetSetState(coption2, TRUE, TRUE);
                        XmToggleButtonGadgetSetState(bright_dim_toggle, FALSE, TRUE);
                        break;
                    case (10):  // Green Dim
                        XmToggleButtonGadgetSetState(coption3, TRUE, TRUE);
                        XmToggleButtonGadgetSetState(bright_dim_toggle, FALSE, TRUE);
                        break;
                    case (11):  // Cyan Dim
                        XmToggleButtonGadgetSetState(coption4, TRUE, TRUE);
                        XmToggleButtonGadgetSetState(bright_dim_toggle, FALSE, TRUE);
                        break;
                    case (12):  // Red Dim
                        XmToggleButtonGadgetSetState(coption5, TRUE, TRUE);
                        XmToggleButtonGadgetSetState(bright_dim_toggle, FALSE, TRUE);
                        break;
                    case (13):  // Violet Dim
                        XmToggleButtonGadgetSetState(coption6, TRUE, TRUE);
                        XmToggleButtonGadgetSetState(bright_dim_toggle, FALSE, TRUE);
                        break;
                    case (14):  // Yellow Dim
                        XmToggleButtonGadgetSetState(coption7, TRUE, TRUE);
                        XmToggleButtonGadgetSetState(bright_dim_toggle, FALSE, TRUE);
                        break;
                    case (15):  // Gray Dim
                        XmToggleButtonGadgetSetState(coption8, TRUE, TRUE);
                        XmToggleButtonGadgetSetState(bright_dim_toggle, FALSE, TRUE);
                        break;
                    case (0):   // Black Bright
                    default:
                        XmToggleButtonGadgetSetState(coption1, TRUE, TRUE);
                        XmToggleButtonGadgetSetState(bright_dim_toggle, TRUE, TRUE);
                        break;
                }

                xastir_snprintf(temp_data, sizeof(temp_data), "%d",
                        p_station->aprs_symbol.area_object.sqrt_lat_off
                        * p_station->aprs_symbol.area_object.sqrt_lat_off );

                XmTextFieldSetString( ob_lat_offset_data, temp_data );

                xastir_snprintf(temp_data, sizeof(temp_data), "%d",
                        p_station->aprs_symbol.area_object.sqrt_lon_off
                        * p_station->aprs_symbol.area_object.sqrt_lon_off );

                XmTextFieldSetString( ob_lon_offset_data, temp_data );

            }   // Done with filling in Area Objects

            else {  // Signpost or Normal Object

                // Handle Generic Options (common to Signpost/Normal Objects)
                if (strlen(p_station->speed) != 0) {
                    xastir_snprintf(temp_data, sizeof(temp_data), "%d",
                            (int)(atof(p_station->speed) + 0.5) );

                    XmTextFieldSetString( ob_speed_data, temp_data );
                } else
                    XmTextFieldSetString( ob_speed_data, "" );

                if (strlen(p_station->course) != 0)
                    XmTextFieldSetString( ob_course_data, p_station->course);
                else
                    XmTextFieldSetString( ob_course_data, "" );

//                if ( (p_station->aprs_symbol.aprs_symbol == 'm')   // Found a signpost object
//                        && (p_station->aprs_symbol.aprs_type == '\\')
//                        && Signpost_object_enabled) {
                if (Signpost_object_enabled) {
                    XtSetSensitive(ob_frame,FALSE);
                    XtSetSensitive(signpost_frame,TRUE);
                    Signpost_object_enabled = 1;
                    XmTextFieldSetString( signpost_data, p_station->signpost);
                }   // Done with filling in Signpost Objects

//                else if ( (p_station->aprs_symbol.aprs_type == '/') // Found a DF object
//                        && (p_station->aprs_symbol.aprs_symbol == '\\' )) {
                if (DF_object_enabled) {
                    XtSetSensitive(ob_frame,FALSE);
                    //printf("Found a DF object\n");

                    // Decide if it was an omni-DF object or a beam heading object
                    if (p_station->NRQ[0] == '\0') {    // Must be an omni-DF object
                        //printf("omni-DF\n");
                        //printf("Signal_gain: %s\n", p_station->signal_gain);

                        XmToggleButtonSetState(omni_antenna_toggle, TRUE, TRUE);

                        // Set the received signal quality toggle
                        switch (p_station->signal_gain[3]) {
                            case ('1'):   // 1
                                XmToggleButtonGadgetSetState(soption1, TRUE, TRUE);
                                break;
                            case ('2'):   // 2
                                XmToggleButtonGadgetSetState(soption2, TRUE, TRUE);
                                break;
                            case ('3'):   // 3
                                XmToggleButtonGadgetSetState(soption3, TRUE, TRUE);
                                break;
                            case ('4'):   // 4
                                XmToggleButtonGadgetSetState(soption4, TRUE, TRUE);
                                break;
                            case ('5'):   // 5
                                XmToggleButtonGadgetSetState(soption5, TRUE, TRUE);
                                break;
                            case ('6'):   // 6
                                XmToggleButtonGadgetSetState(soption6, TRUE, TRUE);
                                break;
                            case ('7'):   // 7
                                XmToggleButtonGadgetSetState(soption7, TRUE, TRUE);
                                break;
                            case ('8'):   // 8
                                XmToggleButtonGadgetSetState(soption8, TRUE, TRUE);
                                break;
                            case ('9'):   // 9
                                XmToggleButtonGadgetSetState(soption9, TRUE, TRUE);
                                break;
                            case ('0'):   // 0
                            default:
                                XmToggleButtonGadgetSetState(soption0, TRUE, TRUE);
                                break;
                        }

                        // Set the HAAT toggle
                        switch (p_station->signal_gain[4]) {
                            case ('1'):   // 20ft
                                XmToggleButtonGadgetSetState(hoption1, TRUE, TRUE);
                                break;
                            case ('2'):   // 40ft
                                XmToggleButtonGadgetSetState(hoption2, TRUE, TRUE);
                                break;
                            case ('3'):   // 80ft
                                XmToggleButtonGadgetSetState(hoption3, TRUE, TRUE);
                                break;
                            case ('4'):   // 160ft
                                XmToggleButtonGadgetSetState(hoption4, TRUE, TRUE);
                                break;
                            case ('5'):   // 320ft
                                XmToggleButtonGadgetSetState(hoption5, TRUE, TRUE);
                                break;
                            case ('6'):   // 640ft
                                XmToggleButtonGadgetSetState(hoption6, TRUE, TRUE);
                                break;
                            case ('7'):   // 1280ft
                                XmToggleButtonGadgetSetState(hoption7, TRUE, TRUE);
                                break;
                            case ('8'):   // 2560ft
                                XmToggleButtonGadgetSetState(hoption8, TRUE, TRUE);
                                break;
                            case ('9'):   // 5120ft
                                XmToggleButtonGadgetSetState(hoption9, TRUE, TRUE);
                                break;
                            case ('0'):   // 10ft
                            default:
                                XmToggleButtonGadgetSetState(hoption0, TRUE, TRUE);
                                break;
                        }

                        // Set the antenna gain toggle
                        switch (p_station->signal_gain[5]) {
                            case ('1'):   // 1dB
                                XmToggleButtonGadgetSetState(goption1, TRUE, TRUE);
                                break;
                            case ('2'):   // 2dB
                                XmToggleButtonGadgetSetState(goption2, TRUE, TRUE);
                                break;
                            case ('3'):   // 3dB
                                XmToggleButtonGadgetSetState(goption3, TRUE, TRUE);
                                break;
                            case ('4'):   // 4dB
                                XmToggleButtonGadgetSetState(goption4, TRUE, TRUE);
                                break;
                            case ('5'):   // 5dB
                                XmToggleButtonGadgetSetState(goption5, TRUE, TRUE);
                                break;
                            case ('6'):   // 6dB
                                XmToggleButtonGadgetSetState(goption6, TRUE, TRUE);
                                break;
                            case ('7'):   // 7dB
                                XmToggleButtonGadgetSetState(goption7, TRUE, TRUE);
                                break;
                            case ('8'):   // 8dB
                                XmToggleButtonGadgetSetState(goption8, TRUE, TRUE);
                                break;
                            case ('9'):   // 9dB
                                XmToggleButtonGadgetSetState(goption9, TRUE, TRUE);
                                break;
                            case ('0'):   // 0dB
                            default:
                                XmToggleButtonGadgetSetState(goption0, TRUE, TRUE);
                                break;
                        }

                        // Set the antenna directivity toggle
                        switch (p_station->signal_gain[6]) {
                            case ('1'):   // 45
                                XmToggleButtonGadgetSetState(doption1, TRUE, TRUE);
                                break;
                            case ('2'):   // 90
                                XmToggleButtonGadgetSetState(doption2, TRUE, TRUE);
                                break;
                            case ('3'):   // 135
                                XmToggleButtonGadgetSetState(doption3, TRUE, TRUE);
                                break;
                            case ('4'):   // 180
                                XmToggleButtonGadgetSetState(doption4, TRUE, TRUE);
                                break;
                            case ('5'):   // 225
                                XmToggleButtonGadgetSetState(doption5, TRUE, TRUE);
                                break;
                            case ('6'):   // 270
                                XmToggleButtonGadgetSetState(doption6, TRUE, TRUE);
                                break;
                            case ('7'):   // 315
                                XmToggleButtonGadgetSetState(doption7, TRUE, TRUE);
                                break;
                            case ('8'):   // 360
                                XmToggleButtonGadgetSetState(doption8, TRUE, TRUE);
                                break;
                            case ('0'):   // Omni
                            default:
                                XmToggleButtonGadgetSetState(doption0, TRUE, TRUE);
                                break;
                        }
                    }
                    else {  // Must be a beam-heading object
                        //printf("beam-heading\n");

                        XmToggleButtonSetState(beam_antenna_toggle, TRUE, TRUE);

                        XmTextFieldSetString(ob_bearing_data, p_station->bearing);

                        switch (p_station->NRQ[2]) {
                            case ('1'):   // 240°
                                XmToggleButtonGadgetSetState(woption1, TRUE, TRUE);
                                break;
                            case ('2'):   // 120°
                                XmToggleButtonGadgetSetState(woption2, TRUE, TRUE);
                                break;
                            case ('3'):   // 64°
                                XmToggleButtonGadgetSetState(woption3, TRUE, TRUE);
                                break;
                            case ('4'):   // 32°
                                XmToggleButtonGadgetSetState(woption4, TRUE, TRUE);
                                break;
                            case ('5'):   // 16°
                                XmToggleButtonGadgetSetState(woption5, TRUE, TRUE);
                                break;
                            case ('6'):   // 8°
                                XmToggleButtonGadgetSetState(woption6, TRUE, TRUE);
                                break;
                            case ('7'):   // 4°
                                XmToggleButtonGadgetSetState(woption7, TRUE, TRUE);
                                break;
                            case ('8'):   // 2°
                                XmToggleButtonGadgetSetState(woption8, TRUE, TRUE);
                                break;
                            case ('9'):   // 1°
                                XmToggleButtonGadgetSetState(woption9, TRUE, TRUE);
                                break;
                            case ('0'):   // Useless
                            default:
                                XmToggleButtonGadgetSetState(woption0, TRUE, TRUE);
                                break;
                        }
                    }
                } else {  // Found a normal object
                    // Nothing needed in this block currently
                } // Done with filling in Normal objects

            } // Done with filling in Signpost, DF, or Normal Objects

            // Handle Generic Options (common to all)
            // Convert altitude from meters to feet
            if (strlen(p_station->altitude) != 0) {
                xastir_snprintf(temp_data, sizeof(temp_data), "%d",
                        (int)((atof(p_station->altitude) / 0.3048) + 0.5) );

                XmTextFieldSetString( ob_altitude_data, temp_data );
            } else
                XmTextFieldSetString( ob_altitude_data, "" );
        }

// Else we're creating a new object from scratch:  p_station == NULL
        else {
            if (Area_object_enabled) {
                XmToggleButtonGadgetSetState(coption1, TRUE, TRUE);             // Black
                XmToggleButtonGadgetSetState(bright_dim_toggle, FALSE, TRUE);   // Dim
                XmToggleButtonGadgetSetState(toption1, TRUE, TRUE);             // Circle
            }

            XmTextFieldSetString(object_name_data,"");

            // Set the symbol type based on the global variables
            if (Area_object_enabled) {
                temp_data[0] = '\\';
                temp_data[1] = '\0';
                XmTextFieldSetString(object_group_data,temp_data);

                temp_data[0] = 'l';
                temp_data[1] = '\0';
                XmTextFieldSetString(object_symbol_data,temp_data);

                XtSetSensitive(ob_frame,FALSE);
            } else if (Signpost_object_enabled) {
                temp_data[0] = '\\';
                temp_data[1] = '\0';
                XmTextFieldSetString(object_group_data,temp_data);

                temp_data[0] = 'm';
                temp_data[1] = '\0';
                XmTextFieldSetString(object_symbol_data,temp_data);

                XmTextFieldSetString( signpost_data, "" );

                XtSetSensitive(ob_frame,FALSE);
            } else if (DF_object_enabled) {
                temp_data[0] = '/';
                temp_data[1] = '\0';
                XmTextFieldSetString(object_group_data,temp_data);

                temp_data[0] = '\\';
                temp_data[1] = '\0';
                XmTextFieldSetString(object_symbol_data,temp_data);

                // Defaults Omni-DF
                XmToggleButtonGadgetSetState(soption0, TRUE, TRUE); // Nothing heard
                XmToggleButtonGadgetSetState(hoption1, TRUE, TRUE); // 20 feet HAAT
                XmToggleButtonGadgetSetState(goption3, TRUE, TRUE); // 3dB gain antenna
                XmToggleButtonGadgetSetState(doption0, TRUE, TRUE); // No directivity

                // Defaults Beam-DF
                XmToggleButtonGadgetSetState(woption5, TRUE, TRUE); // 16 degree beamwidth

                XtSetSensitive(ob_frame,FALSE);
            } else {  // Normal object/item
                temp_data[0] = '\\';
                temp_data[1] = '\0';
                XmTextFieldSetString(object_group_data,temp_data);

                temp_data[0] = '!';
                temp_data[1] = '\0';
                XmTextFieldSetString(object_symbol_data,temp_data);
            }

            XmTextFieldSetString(object_comment_data,"");
        }


        if (strcmp(calldata,"2") != 0) {  // Normal Modify->Object or Create->Object behavior
            // Fill in original lat/lon values
            convert_lat_l2s(lat, lat_str, sizeof(lat_str), CONVERT_HP_NOSP);
            substr(temp_data,lat_str,2);
            XmTextFieldSetString(object_lat_data_deg,temp_data);
            substr(temp_data,lat_str+2,6);
            XmTextFieldSetString(object_lat_data_min,temp_data);
            substr(temp_data,lat_str+8,1);
            XmTextFieldSetString(object_lat_data_ns,temp_data);

            convert_lon_l2s(lon, lon_str, sizeof(lon_str), CONVERT_HP_NOSP);
            substr(temp_data,lon_str,3);
            XmTextFieldSetString(object_lon_data_deg,temp_data);
            substr(temp_data,lon_str+3,6);
            XmTextFieldSetString(object_lon_data_min,temp_data);
            substr(temp_data,lon_str+9,1);
            XmTextFieldSetString(object_lon_data_ew,temp_data);
        }

        else  { // We're in the middle of moving an object, calldata was "2"
            // Fill in new lat/long values
            //printf("Here we will fill in the new lat/long values\n");
            x = (mid_x_long_offset - ((screen_width  * scale_x)/2) + (input_x * scale_x));
            y = (mid_y_lat_offset  - ((screen_height * scale_y)/2) + (input_y * scale_y));
            if (x < 0)
                x = 0l;                 // 180°W

            if (x > 129600000l)
                x = 129600000l;         // 180°E

            if (y < 0)
                y = 0l;                 //  90°N

            if (y > 64800000l)
                y = 64800000l;          //  90°S

            convert_lat_l2s(y, lat_str, sizeof(lat_str), CONVERT_HP_NOSP);
            substr(temp_data,lat_str,2);
            XmTextFieldSetString(object_lat_data_deg,temp_data);
            substr(temp_data,lat_str+2,6);
            XmTextFieldSetString(object_lat_data_min,temp_data);
            substr(temp_data,lat_str+8,1);
            XmTextFieldSetString(object_lat_data_ns,temp_data);

            convert_lon_l2s(x, lon_str, sizeof(lon_str), CONVERT_HP_NOSP);
            substr(temp_data,lon_str,3);
            XmTextFieldSetString(object_lon_data_deg,temp_data);
            substr(temp_data,lon_str+3,6);
            XmTextFieldSetString(object_lon_data_min,temp_data);
            substr(temp_data,lon_str+9,1);
            XmTextFieldSetString(object_lon_data_ew,temp_data);
        }

        XtAddCallback(object_group_data, XmNvalueChangedCallback, updateObjectPictureCallback, object_dialog);
        XtAddCallback(object_symbol_data, XmNvalueChangedCallback, updateObjectPictureCallback, object_dialog);

        // update symbol picture
        (void)updateObjectPictureCallback((Widget)NULL,(XtPointer)NULL,(XtPointer)NULL);

        pos_dialog(object_dialog);

        delw = XmInternAtom(XtDisplay(object_dialog),"WM_DELETE_WINDOW", FALSE);
        XmAddWMProtocolCallback(object_dialog, delw, Object_destroy_shell, (XtPointer)object_dialog);

        if (Signpost_object_enabled) {
            XtManageChild(signpost_form);
        } else if (Area_object_enabled) {
            XtManageChild(shape_box);
            XtManageChild(color_box);
            XtManageChild(area_form);
        } else if (DF_object_enabled) {
            XtManageChild(signal_box);
            XtManageChild(height_box);
            XtManageChild(gain_box);
            XtManageChild(directivity_box);
            XtManageChild(width_box);
            XtManageChild(formomni);
            XtManageChild(formbeam);
        }

        XtManageChild(ob_latlon_form);
        XtManageChild(ob_option_form);
        XtManageChild(ob_form1);
        XtManageChild(ob_form);
        XtManageChild(ob_pane);

        XtPopup(object_dialog,XtGrabNone);
        fix_dialog_size(object_dialog);         // don't allow a resize

        // Move focus to the Cancel button.  This appears to highlight t
        // button fine, but we're not able to hit the <Enter> key to
        // have that default function happen.  Note:  We _can_ hit the
        // <SPACE> key, and that activates the option.
//        XmUpdateDisplay(object_dialog);
        XmProcessTraversal(ob_button_cancel, XmTRAVERSE_CURRENT);
    }

/*
    // This will cause the move to happen quickly without any button presses.
    if (calldata != NULL) { // If we're doing a "move" operation
        if (strcmp(calldata,"2") == 0) {
            if ((p_station->flag & ST_ITEM) != 0) {     // Moving an Item
                //printf("Item\n");
                Item_change_data_set(w,object_dialog,object_dialog);    // Move it now!
            }
            else {                                      // Moving an Object
                //printf("Object\n");
                Object_change_data_set(w,object_dialog,object_dialog);  // Move it now!
            }
        }
    }
*/

}   // End of Set_Del_Object





/////////////////////////////////////////////////////////////////////////





/*
 *  Destroy Configure Station Dialog Popup Window
 */
void Configure_station_destroy_shell( /*@unused@*/ Widget widget, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    Widget shell = (Widget) clientData;
    XtPopdown(shell);
    (void)XFreePixmap(XtDisplay(appshell),CS_icon0);  // ???? DK7IN: avoid possible memory leak ?
    (void)XFreePixmap(XtDisplay(appshell),CS_icon);
    XtDestroyWidget(shell);
    configure_station_dialog = (Widget)NULL;

    // Take down the symbol selection dialog as well (if it's up)
    if (select_symbol_dialog) {
        Select_symbol_destroy_shell( widget, select_symbol_dialog, callData);
    }

    // NULL out the dialog field in the global struct used for
    // Coordinate Calculator.  Prevents segfaults if the calculator is
    // still up and trying to write to us.
    coordinate_calc_array.calling_dialog = NULL;
}





void  Configure_station_toggle( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        Configure_station_pos_amb = atoi(which);
    else
        Configure_station_pos_amb = 0;
}





/*
 *  Process changes to station data
 */
void Configure_station_change_data(Widget widget, XtPointer clientData, XtPointer callData) {
    char temp[40];
    char old_callsign[MAX_CALL+1];
    int ok = 1;
    int temp2;
    int temp3;

    transmit_compressed_posit = (int)XmToggleButtonGetState(compressed_posit_tx);

    strcpy(old_callsign,my_callsign);
    /*printf("Changing Configure station data\n");*/

    xastir_snprintf(my_callsign, sizeof(my_callsign), "%s",
            XmTextFieldGetString(station_config_call_data));
    (void)remove_trailing_spaces(my_callsign);
    (void)to_upper(my_callsign);

    strcpy(temp, XmTextFieldGetString(station_config_slat_data_ns));
    if((char)toupper((int)temp[0])=='S')
        temp[0]='S';
    else
        temp[0]='N';

    // Check latitude for out-of-bounds
    temp2 = atoi(XmTextFieldGetString(station_config_slat_data_deg));
    if ( (temp2 > 90) || (temp2 < 0) )
        ok = 0;

    temp3 = atof(XmTextFieldGetString(station_config_slat_data_min));
    if ( (temp3 >= 60.0) || (temp3 < 0.0) )
        ok = 0;

    if ( (temp2 == 90) && (temp3 != 0.0) )
        ok = 0;

    xastir_snprintf(my_lat, sizeof(my_lat), "%02d%06.3f%c",
        atoi(XmTextFieldGetString(station_config_slat_data_deg)),
        atof(XmTextFieldGetString(station_config_slat_data_min)),temp[0]);

    strcpy(temp,XmTextFieldGetString(station_config_slong_data_ew));
    if((char)toupper((int)temp[0])=='E')
        temp[0]='E';
    else
        temp[0]='W';

    // Check longitude for out-of-bounds
    temp2 = atoi(XmTextFieldGetString(station_config_slong_data_deg));
    if ( (temp2 > 180) || (temp2 < 0) )
        ok = 0;

    temp3 = atof(XmTextFieldGetString(station_config_slong_data_min));
    if ( (temp3 >= 60.0) || (temp3 < 0.0) )
        ok = 0;

    if ( (temp2 == 180) && (temp3 != 0.0) )
        ok = 0;

    xastir_snprintf(my_long, sizeof(my_long), "%03d%06.3f%c",
            atoi(XmTextFieldGetString(station_config_slong_data_deg)),
            atof(XmTextFieldGetString(station_config_slong_data_min)),temp[0]);

    strcpy(temp,XmTextFieldGetString(station_config_group_data));
    my_group=temp[0];
    if(isalpha((int)my_group))
        my_group = toupper((int)temp[0]);

    strcpy(temp,XmTextFieldGetString(station_config_symbol_data));
    my_symbol = temp[0];

    if(isdigit((int)my_phg[3]) && isdigit((int)my_phg[4]) && isdigit((int)my_phg[5]) && isdigit((int)my_phg[6])) {
        my_phg[0] = 'P';
        my_phg[1] = 'H';
        my_phg[2] = 'G';
        my_phg[7] = '\0';
    } else
        my_phg[0]='\0';

    /* set station ambiguity*/
    position_amb_chars = Configure_station_pos_amb;

    strcpy(my_comment,XmTextFieldGetString(station_config_comment_data));
    (void)remove_trailing_spaces(my_comment);

    /* TO DO: KILL only my station data? */
    if (ok) {   // If entered data was valid

        // Check whether we changed our callsign
        if (strcasecmp(old_callsign,my_callsign) != 0) {
            station_del(old_callsign);  // move to new sort location...
        }

        // Update our parameters
        my_station_add(my_callsign,my_group,my_symbol,my_long,my_lat,my_phg,my_comment,(char)position_amb_chars);

        redraw_on_new_data=2;
        Configure_station_destroy_shell(widget,clientData,callData);
    }

    // Check for proper weather symbols if a weather station
    (void)check_weather_symbol();

    // Check for use of NWS symbols
    (void)check_nws_weather_symbol();
}





/*
 *  Update symbol picture for changed symbol or table
 */
void updateSymbolPictureCallback( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer callData) {
    char table, overlay;
    char symb, group;

    XtVaSetValues(station_config_icon, XmNlabelPixmap, CS_icon0, NULL);         // clear old icon
    XtManageChild(station_config_icon);

    group = (XmTextFieldGetString(station_config_group_data))[0];
    symb  = (XmTextFieldGetString(station_config_symbol_data))[0];
    if (group == '/' || group == '\\') {
        table   = group;
        overlay = ' ';
    } else {
        table   = '\\';
        overlay = group;
    }
    symbol(station_config_icon,0,table,symb,overlay,CS_icon,0,0,0,' ');         // create icon

    XtVaSetValues(station_config_icon,XmNlabelPixmap,CS_icon,NULL);             // draw new icon
    XtManageChild(station_config_icon);
}





/* Power radio buttons */
void Power_toggle( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set) {

        my_phg[3] = which[0];   // Set to power desired

        if (which[0] == 'x') {     // Disable PHG field if 'x' found
            my_phg[0] = '\0';
        }
    } else {
        my_phg[3] = '3';  // 10 Watts (default in spec if none specified)
    }
    my_phg[8] = '\0';

    //printf("PHG: %s\n",my_phg);
}





/* Height radio buttons */
void Height_toggle( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set) {
        my_phg[4] = which[0];    // Set to height desired
    } else {
        my_phg[4] = '1';  // 20 Feet above average terrain (default in spec if none specified)
    }
    my_phg[8] = '\0';

    //printf("PHG: %s\n",my_phg);
}






/* Gain radio buttons */
void Gain_toggle( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set) {
        my_phg[5] = which[0];    // Set to gain desired
    } else {
        my_phg[5] = '3';  // 3dB gain antenna (default in spec if none specified)
    }
    my_phg[8] = '\0';

    //printf("PHG: %s\n",my_phg);
}






/* Directivity radio buttons */
void Directivity_toggle( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set) {
        my_phg[6] = which[0];    // Set to directivity desired
    } else {
        my_phg[6] = '0';  // Omni-directional antenna (default in spec if none specified)
    }
    my_phg[8] = '\0';

    //printf("PHG: %s\n",my_phg);
}





void Posit_compressed_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        transmit_compressed_posit = atoi(which);
    else
        transmit_compressed_posit = 0;

    if(transmit_compressed_posit) {
        // Compressed posits don't allow position ambiguity
        position_amb_chars = 0;
        XtSetSensitive(posamb0,FALSE);
        XtSetSensitive(posamb1,FALSE);
        XtSetSensitive(posamb2,FALSE);
        XtSetSensitive(posamb3,FALSE);
        XtSetSensitive(posamb4,FALSE);
    }
    else {  // Position ambiguity ok for this mode
        XtSetSensitive(posamb0,TRUE);
        XtSetSensitive(posamb1,TRUE);
        XtSetSensitive(posamb2,TRUE);
        XtSetSensitive(posamb3,TRUE);
        XtSetSensitive(posamb4,TRUE);
    }
}





/*
 *  Select a symbol graphically
 */
void Configure_change_symbol(/*@unused@*/ Widget widget, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer callData) {

    //printf("Trying to change a symbol\n");
    symbol_change_requested_from = 1;   // Tell Select_symbol who to return the data to
    Select_symbol(widget, clientData, callData);
}






/*
 *  Setup Configure Station dialog
 */
void Configure_station( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer callData) {
    static Widget  pane, cs_form, cs_form1, button_ok, button_cancel, call, frame, frame2,
                framephg, pg2, formphg,
                power_box,poption0,poption1,poption2,poption3,poption4,poption5,poption6,poption7,poption8,poption9,poption10,
                height_box,hoption1,hoption2,hoption3,hoption4,hoption5,hoption6,hoption7,hoption8,hoption9,hoption10,
                gain_box,goption1,goption2,goption3,goption4,goption5,goption6,goption7,goption8,goption9,goption10,
                directivity_box,doption1,doption2,doption3,doption4,doption5,doption6,doption7,doption8,doption9,
                slat,
                slat_deg,  slat_min, slat_ns,
                slong,
                slong_deg, slong_min, slong_ew,
                sts, group, st_symbol, comment,
                posamb,option_box,
                sep, configure_button_symbol, compute_button;
    char temp_data[40];
    Atom delw;
    Arg al[2];                    /* Arg List */
    register unsigned int ac = 0;           /* Arg Count */

    if(!configure_station_dialog) {
        configure_station_dialog = XtVaCreatePopupShell(langcode("WPUPCFS001"),
                xmDialogShellWidgetClass,
                Global.top,
                XmNdeleteResponse,          XmDESTROY,
                XmNdefaultPosition,         FALSE,
                NULL);

        pane = XtVaCreateWidget("Configure_station pane",
                xmPanedWindowWidgetClass, 
                configure_station_dialog,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        cs_form =  XtVaCreateWidget("Configure_station cs_form",
                xmFormWidgetClass, 
                pane,
                XmNfractionBase,            5,
                XmNautoUnmanage,            FALSE,
                XmNshadowThickness,         1,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        call = XtVaCreateManagedWidget(langcode("WPUPCFS002"),
                xmLabelWidgetClass, 
                cs_form,
                XmNtopAttachment,           XmATTACH_FORM,
                XmNtopOffset,               10,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNleftAttachment,          XmATTACH_FORM,
                XmNleftOffset,              10,
                XmNrightAttachment,         XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        station_config_call_data = XtVaCreateManagedWidget("Configure_station call_data", 
                xmTextFieldWidgetClass, 
                cs_form,
                XmNeditable,                TRUE,
                XmNcursorPositionVisible,   TRUE,
                XmNsensitive,               TRUE,
                XmNshadowThickness,         1,
                XmNcolumns,                 10,
                XmNwidth,                   ((10*7)+2),
                XmNmaxLength,               9,
                XmNbackground,              colors[0x0f],
                XmNtopAttachment,           XmATTACH_FORM,
                XmNtopOffset,               5,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNleftAttachment,          XmATTACH_POSITION,
                XmNleftPosition,            1,
                XmNrightAttachment,         XmATTACH_NONE,
                NULL);

        compressed_posit_tx = XtVaCreateManagedWidget(langcode("WPUPCFS029"),
                xmToggleButtonWidgetClass,
                cs_form,
                XmNtopAttachment,           XmATTACH_FORM,
                XmNtopOffset,               10,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNleftAttachment,          XmATTACH_POSITION,
                XmNleftPosition,            3,
                XmNrightAttachment,         XmATTACH_NONE,
                XmNnavigationType,          XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        XtAddCallback(compressed_posit_tx,XmNvalueChangedCallback,Posit_compressed_toggle,"1");
 
        slat = XtVaCreateManagedWidget(langcode("WPUPCFS003"),
                xmLabelWidgetClass, 
                cs_form,
                XmNtopAttachment,           XmATTACH_WIDGET,
                XmNtopWidget,               call,
                XmNtopOffset,               25,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNleftAttachment,          XmATTACH_FORM,
                XmNleftOffset,              10,
                XmNrightAttachment,         XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        station_config_slat_data_deg = XtVaCreateManagedWidget("Configure_station lat_deg", 
                xmTextFieldWidgetClass, 
                cs_form,
                XmNeditable,                TRUE,
                XmNcursorPositionVisible,   TRUE,
                XmNsensitive,               TRUE,
                XmNshadowThickness,         1,
                XmNcolumns,                 3,
                XmNmaxLength,               2,
                XmNtopOffset,               20,
                XmNbackground,              colors[0x0f],
                XmNtopAttachment,           XmATTACH_WIDGET,
                XmNtopWidget,               call,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNleftAttachment,          XmATTACH_POSITION,
                XmNleftPosition,            1,
                XmNrightAttachment,         XmATTACH_NONE,
                NULL);

        slat_deg = XtVaCreateManagedWidget(langcode("WPUPCFS004"),
                xmLabelWidgetClass, 
                cs_form,
                XmNtopAttachment,           XmATTACH_WIDGET,
                XmNtopWidget,               call,
                XmNtopOffset,               25,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              station_config_slat_data_deg,
                XmNrightAttachment,         XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        station_config_slat_data_min = XtVaCreateManagedWidget("Configure_station lat_min", 
                xmTextFieldWidgetClass, 
                cs_form,
                XmNeditable,                TRUE,
                XmNcursorPositionVisible,   TRUE,
                XmNsensitive,               TRUE,
                XmNshadowThickness,         1,
                XmNcolumns,                 6,
                XmNmaxLength,               6,
                XmNtopOffset,               20,
                XmNbackground,              colors[0x0f],
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              slat_deg,
                XmNleftOffset,              10,
                XmNtopAttachment,           XmATTACH_WIDGET,
                XmNtopWidget,               call,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNrightAttachment,         XmATTACH_NONE,
                NULL);

        slat_min = XtVaCreateManagedWidget(langcode("WPUPCFS005"),
                xmLabelWidgetClass, 
                cs_form,
                XmNtopAttachment,           XmATTACH_WIDGET,
                XmNtopWidget,               call,
                XmNtopOffset,               25,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              station_config_slat_data_min,
                XmNrightAttachment,         XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        station_config_slat_data_ns = XtVaCreateManagedWidget("Configure_station lat_ns", 
                xmTextFieldWidgetClass, 
                cs_form,
                XmNeditable,                TRUE,
                XmNcursorPositionVisible,   FALSE,
                XmNsensitive,               TRUE,
                XmNshadowThickness,         1,
                XmNcolumns,                 1,
                XmNmaxLength,               1,
                XmNtopOffset,               20,
                XmNbackground,              colors[0x0f],
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              slat_min,
                XmNleftOffset,              10,
                XmNtopAttachment,           XmATTACH_WIDGET,
                XmNtopWidget,               call,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNrightAttachment,         XmATTACH_NONE,
                NULL);

        slat_ns = XtVaCreateManagedWidget(langcode("WPUPCFS006"),
                xmLabelWidgetClass, 
                cs_form,
                XmNtopAttachment,           XmATTACH_WIDGET,
                XmNtopWidget,               call,
                XmNtopOffset,               25,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              station_config_slat_data_ns,
                XmNrightAttachment,         XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        slong = XtVaCreateManagedWidget(langcode("WPUPCFS007"),
                xmLabelWidgetClass, 
                cs_form,
                XmNtopAttachment,           XmATTACH_WIDGET,
                XmNtopWidget,               slat,
                XmNtopOffset,               20,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNleftAttachment,          XmATTACH_FORM,
                XmNleftOffset,              10,
                XmNrightAttachment,         XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        station_config_slong_data_deg = XtVaCreateManagedWidget("Configure_station long_deg", 
                xmTextFieldWidgetClass, 
                cs_form,
                XmNeditable,                TRUE,
                XmNcursorPositionVisible,   TRUE,
                XmNsensitive,               TRUE,
                XmNshadowThickness,         1,
                XmNcolumns,                 3,
                XmNmaxLength,               3,
                XmNtopOffset,               14,
                XmNbackground,              colors[0x0f],
                XmNtopAttachment,           XmATTACH_WIDGET,
                XmNtopWidget,               slat,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNleftAttachment,          XmATTACH_POSITION,
                XmNleftPosition,            1,
                XmNrightAttachment,         XmATTACH_NONE,
                NULL);

        slong_deg = XtVaCreateManagedWidget(langcode("WPUPCFS004"),
                xmLabelWidgetClass, 
                cs_form,
                XmNtopAttachment,           XmATTACH_WIDGET,
                XmNtopWidget,               slat,
                XmNtopOffset,               20,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              station_config_slong_data_deg,
                XmNrightAttachment,         XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        station_config_slong_data_min = XtVaCreateManagedWidget("Configure_station long_min", 
                xmTextFieldWidgetClass, 
                cs_form,
                XmNeditable,                TRUE,
                XmNcursorPositionVisible,   TRUE,
                XmNsensitive,               TRUE,
                XmNshadowThickness,         1,
                XmNcolumns,                 6,
                XmNmaxLength,               6,
                XmNtopOffset,               14,
                XmNbackground,              colors[0x0f],
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              slong_deg,
                XmNleftOffset,              10,
                XmNtopAttachment,           XmATTACH_WIDGET,
                XmNtopWidget,               slat,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNrightAttachment,         XmATTACH_NONE,
                NULL);

        slong_min = XtVaCreateManagedWidget(langcode("WPUPCFS005"),
                xmLabelWidgetClass, 
                cs_form,
                XmNtopAttachment,           XmATTACH_WIDGET,
                XmNtopWidget,               slat,
                XmNtopOffset,               20,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              station_config_slong_data_min,
                XmNrightAttachment,         XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        station_config_slong_data_ew = XtVaCreateManagedWidget("Configure_station long_ew", 
                xmTextFieldWidgetClass, 
                cs_form,
                XmNeditable,                TRUE,
                XmNcursorPositionVisible,   FALSE,
                XmNsensitive,               TRUE,
                XmNshadowThickness,         1,
                XmNcolumns,                 1,
                XmNmaxLength,               1,
                XmNtopOffset,               14,
                XmNbackground,              colors[0x0f],
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              slong_min,
                XmNleftOffset,              10,
                XmNtopAttachment,           XmATTACH_WIDGET,
                XmNtopWidget,               slat,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNrightAttachment,         XmATTACH_NONE,
                NULL);

        slong_ew = XtVaCreateManagedWidget(langcode("WPUPCFS008"),
                xmLabelWidgetClass, 
                cs_form,
                XmNtopAttachment,           XmATTACH_WIDGET,
                XmNtopWidget,               slat,
                XmNtopOffset,               20,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              station_config_slong_data_ew,
                XmNrightAttachment,         XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        compute_button = XtVaCreateManagedWidget(langcode("COORD002"),
                xmPushButtonGadgetClass, 
                cs_form,
                XmNtopAttachment,           XmATTACH_WIDGET,
                XmNtopWidget,               slat,
                XmNtopOffset,               20,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              slong_ew,
                XmNleftOffset,              15,
                XmNrightAttachment,         XmATTACH_NONE,
                XmNnavigationType,          XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        // Fill in the pointers to our input textfields so that the
        // coordinate calculator can fiddle with them.
        coordinate_calc_array.calling_dialog = configure_station_dialog;
        coordinate_calc_array.input_lat_deg = station_config_slat_data_deg;
        coordinate_calc_array.input_lat_min = station_config_slat_data_min;
        coordinate_calc_array.input_lat_dir = station_config_slat_data_ns;
        coordinate_calc_array.input_lon_deg = station_config_slong_data_deg;
        coordinate_calc_array.input_lon_min = station_config_slong_data_min;
        coordinate_calc_array.input_lon_dir = station_config_slong_data_ew;
//        XtAddCallback(compute_button, XmNactivateCallback, Coordinate_calc, configure_station_dialog);
//        XtAddCallback(compute_button, XmNactivateCallback, Coordinate_calc, "Configure_station");
        XtAddCallback(compute_button, XmNactivateCallback, Coordinate_calc, langcode("WPUPCFS001"));




//----- Frame for table / symbol
        frame = XtVaCreateManagedWidget("Configure_station frame", 
                xmFrameWidgetClass, 
                cs_form,
                XmNtopAttachment,           XmATTACH_WIDGET,
                XmNtopWidget,               slong_ew,
                XmNtopOffset,               10,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNleftAttachment,          XmATTACH_FORM,
                XmNleftOffset,              10,
                XmNrightAttachment,         XmATTACH_FORM,
                XmNrightOffset,             10,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        // "Station Symbol"
        sts  = XtVaCreateManagedWidget(langcode("WPUPCFS009"),
                xmLabelWidgetClass,
                frame,
                XmNchildType,               XmFRAME_TITLE_CHILD,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        cs_form1 =  XtVaCreateWidget("Configure_station cs_form1",
                xmFormWidgetClass, 
                frame,
                XmNfractionBase,            5,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        // "Group/overlay"
        group = XtVaCreateManagedWidget(langcode("WPUPCFS010"),
                xmLabelWidgetClass, 
                cs_form1,
                XmNtopAttachment,           XmATTACH_FORM,
                XmNtopOffset,               10,
                XmNbottomAttachment,        XmATTACH_FORM,
                XmNbottomOffset,            10,
                XmNleftAttachment,          XmATTACH_FORM,
                XmNleftOffset,              100,
                XmNrightAttachment,         XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        // table
        station_config_group_data = XtVaCreateManagedWidget("Configure_station group", 
                xmTextFieldWidgetClass, 
                cs_form1,
                XmNeditable,                TRUE,
                XmNcursorPositionVisible,   FALSE,
                XmNsensitive,               TRUE,
                XmNshadowThickness,         1,
                XmNcolumns,                 1,
                XmNmaxLength,               1,
                XmNtopOffset,               6,
                XmNbackground,              colors[0x0f],
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              group,
                XmNleftOffset,              5,
                XmNtopAttachment,           XmATTACH_FORM,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNrightAttachment,         XmATTACH_NONE,
                NULL);

        // "Symbol"
        st_symbol = XtVaCreateManagedWidget(langcode("WPUPCFS011"),
                xmLabelWidgetClass, 
                cs_form1,
                XmNtopAttachment,           XmATTACH_FORM,
                XmNtopOffset,               10,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              station_config_group_data,
                XmNleftOffset,              20,
                XmNrightAttachment,         XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        // symbol
        station_config_symbol_data = XtVaCreateManagedWidget("Configure_station symbol", 
                xmTextFieldWidgetClass, 
                cs_form1,
                XmNeditable,                TRUE,
                XmNcursorPositionVisible,   FALSE,
                XmNsensitive,               TRUE,
                XmNshadowThickness,         1,
                XmNcolumns,                 1,
                XmNmaxLength,               1,
                XmNtopOffset,               6,
                XmNbackground,              colors[0x0f],
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              st_symbol,
                XmNleftOffset,              5,
                XmNtopAttachment,           XmATTACH_FORM,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNrightAttachment,         XmATTACH_NONE,
                NULL);

        // icon
        CS_icon0 = XCreatePixmap(XtDisplay(appshell),
                RootWindowOfScreen(XtScreen(appshell)),
                20,
                20,
                DefaultDepthOfScreen(XtScreen(appshell)));
        CS_icon  = XCreatePixmap(XtDisplay(appshell),
                RootWindowOfScreen(XtScreen(appshell)),
                20,
                20,
                DefaultDepthOfScreen(XtScreen(appshell)));
        station_config_icon = XtVaCreateManagedWidget("Configure_station icon", 
                xmLabelWidgetClass, 
                cs_form1,
                XmNlabelType,               XmPIXMAP,
                XmNlabelPixmap,             CS_icon,
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              station_config_symbol_data,
                XmNleftOffset,              15,
                XmNtopAttachment,           XmATTACH_FORM,
                XmNtopOffset,               10,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNrightAttachment,         XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        configure_button_symbol = XtVaCreateManagedWidget(langcode("WPUPCFS028"),
                xmPushButtonGadgetClass, 
                cs_form1,
                XmNtopAttachment,           XmATTACH_FORM,
                XmNtopOffset,               6,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              station_config_icon,
                XmNleftOffset,              15,
                XmNrightAttachment,         XmATTACH_NONE,
                XmNnavigationType,          XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(configure_button_symbol, XmNactivateCallback, Configure_change_symbol, configure_station_dialog);


//----- Frame for Power-Gain
        framephg = XtVaCreateManagedWidget("Configure_station framephg", 
                xmFrameWidgetClass, 
                cs_form,
                XmNtopAttachment,           XmATTACH_WIDGET,
                XmNtopWidget,               frame,
                XmNtopOffset,               10,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNleftAttachment,          XmATTACH_FORM,
                XmNleftOffset,              10,
                XmNrightAttachment,         XmATTACH_FORM,
                XmNrightOffset,             10,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        pg2  = XtVaCreateManagedWidget(langcode("WPUPCFS012"),
                xmLabelWidgetClass,
                framephg,
                XmNchildType,               XmFRAME_TITLE_CHILD,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        formphg =  XtVaCreateWidget("Configure_station power_form",
                xmFormWidgetClass,
                framephg,
                XmNfractionBase,            5,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        // Power
        ac = 0;
        XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
        XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;

        power_box = XmCreateRadioBox(formphg,
                "Configure_station Power Radio Box",
                al,
                ac);

        XtVaSetValues(power_box,
                XmNpacking, XmPACK_TIGHT,
                XmNorientation, XmHORIZONTAL,
                XmNtopAttachment,XmATTACH_FORM,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 5,
                XmNrightAttachment,XmATTACH_FORM,
                XmNrightOffset, 5,
                XmNnumColumns,11,
                NULL);

        poption0 = XtVaCreateManagedWidget(langcode("WPUPCFS013"),
                xmToggleButtonGadgetClass,
                power_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(poption0,XmNvalueChangedCallback,Power_toggle,"x");

        // 0 Watts
        poption1 = XtVaCreateManagedWidget("0W",
                xmToggleButtonGadgetClass,
                power_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(poption1,XmNvalueChangedCallback,Power_toggle,"0");

        // 1 Watt
        poption2 = XtVaCreateManagedWidget("1W",
                xmToggleButtonGadgetClass,
                power_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(poption2,XmNvalueChangedCallback,Power_toggle,"1");

        // 4 Watts
        poption3 = XtVaCreateManagedWidget("4W",
                xmToggleButtonGadgetClass,
                power_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(poption3,XmNvalueChangedCallback,Power_toggle,"2");

        // 9 Watts
        poption4 = XtVaCreateManagedWidget("9W",
                xmToggleButtonGadgetClass,
                power_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(poption4,XmNvalueChangedCallback,Power_toggle,"3");

        // 16 Watts
        poption5 = XtVaCreateManagedWidget("16W",
                xmToggleButtonGadgetClass,
                power_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(poption5,XmNvalueChangedCallback,Power_toggle,"4");

        // 25 Watts
        poption6 = XtVaCreateManagedWidget("25W",
                xmToggleButtonGadgetClass,
                power_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(poption6,XmNvalueChangedCallback,Power_toggle,"5");

        // 36 Watts
        poption7 = XtVaCreateManagedWidget("36W",
                xmToggleButtonGadgetClass,
                power_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(poption7,XmNvalueChangedCallback,Power_toggle,"6");

        // 49 Watts
        poption8 = XtVaCreateManagedWidget("49W",
                xmToggleButtonGadgetClass,
                power_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(poption8,XmNvalueChangedCallback,Power_toggle,"7");

        // 64 Watts
        poption9 = XtVaCreateManagedWidget("64W",
                xmToggleButtonGadgetClass,
                power_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(poption9,XmNvalueChangedCallback,Power_toggle,"8");

        // 81 Watts
        poption10 = XtVaCreateManagedWidget("81W",
                xmToggleButtonGadgetClass,
                power_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(poption10,XmNvalueChangedCallback,Power_toggle,"9");


        // Height
        height_box = XmCreateRadioBox(formphg,
                "Configure_station Height Radio Box",
                al,
                ac);

        XtVaSetValues(height_box,
                XmNpacking, XmPACK_TIGHT,
                XmNorientation, XmHORIZONTAL,
                XmNtopAttachment,XmATTACH_WIDGET,
                XmNtopWidget,power_box,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 5,
                XmNrightAttachment,XmATTACH_FORM,
                XmNrightOffset, 5,
                XmNnumColumns,10,
                NULL);


        // 10 Feet
        hoption1 = XtVaCreateManagedWidget("10ft",
                xmToggleButtonGadgetClass,
                height_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(hoption1,XmNvalueChangedCallback,Height_toggle,"0");

        // 20 Feet
        hoption2 = XtVaCreateManagedWidget("20ft",
                xmToggleButtonGadgetClass,
                height_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(hoption2,XmNvalueChangedCallback,Height_toggle,"1");

        // 40 Feet
        hoption3 = XtVaCreateManagedWidget("40ft",
                xmToggleButtonGadgetClass,
                height_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(hoption3,XmNvalueChangedCallback,Height_toggle,"2");

        // 80 Feet
        hoption4 = XtVaCreateManagedWidget("80ft",
                xmToggleButtonGadgetClass,
                height_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(hoption4,XmNvalueChangedCallback,Height_toggle,"3");

        // 160 Feet
        hoption5 = XtVaCreateManagedWidget("160ft",
                xmToggleButtonGadgetClass,
                height_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(hoption5,XmNvalueChangedCallback,Height_toggle,"4");

        // 320 Feet
        hoption6 = XtVaCreateManagedWidget("320ft",
                xmToggleButtonGadgetClass,
                height_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(hoption6,XmNvalueChangedCallback,Height_toggle,"5");

        // 640 Feet
        hoption7 = XtVaCreateManagedWidget("640ft",
                xmToggleButtonGadgetClass,
                height_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(hoption7,XmNvalueChangedCallback,Height_toggle,"6");

        // 1280 Feet
        hoption8 = XtVaCreateManagedWidget("1280ft",
                xmToggleButtonGadgetClass,
                height_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(hoption8,XmNvalueChangedCallback,Height_toggle,"7");

        // 2560 Feet
        hoption9 = XtVaCreateManagedWidget("2560ft",
                xmToggleButtonGadgetClass,
                height_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(hoption9,XmNvalueChangedCallback,Height_toggle,"8");

        // 5120 Feet
        hoption10 = XtVaCreateManagedWidget("5120ft",
                xmToggleButtonGadgetClass,
                height_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(hoption10,XmNvalueChangedCallback,Height_toggle,"9");


        // Gain
        gain_box = XmCreateRadioBox(formphg,
                "Configure_station Gain Radio Box",
                al,
                ac);

        XtVaSetValues(gain_box,
                XmNpacking, XmPACK_TIGHT,
                XmNorientation, XmHORIZONTAL,
                XmNtopAttachment,XmATTACH_WIDGET,
                XmNtopWidget,height_box,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 5,
                XmNrightAttachment,XmATTACH_FORM,
                XmNrightOffset, 5,
                XmNnumColumns,10,
                NULL);


        // 0 dB
        goption1 = XtVaCreateManagedWidget("0dB",
                xmToggleButtonGadgetClass,
                gain_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(goption1,XmNvalueChangedCallback,Gain_toggle,"0");

        // 1 dB
        goption2 = XtVaCreateManagedWidget("1dB",
                xmToggleButtonGadgetClass,
                gain_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(goption2,XmNvalueChangedCallback,Gain_toggle,"1");

        // 2 dB
        goption3 = XtVaCreateManagedWidget("2dB",
                xmToggleButtonGadgetClass,
                gain_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(goption3,XmNvalueChangedCallback,Gain_toggle,"2");

        // 3 dB
        goption4 = XtVaCreateManagedWidget("3dB",
                xmToggleButtonGadgetClass,
                gain_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(goption4,XmNvalueChangedCallback,Gain_toggle,"3");

        // 4 dB
        goption5 = XtVaCreateManagedWidget("4dB",
                xmToggleButtonGadgetClass,
                gain_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(goption5,XmNvalueChangedCallback,Gain_toggle,"4");

        // 5 dB
        goption6 = XtVaCreateManagedWidget("5dB",
                xmToggleButtonGadgetClass,
                gain_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(goption6,XmNvalueChangedCallback,Gain_toggle,"5");

        // 6 dB
        goption7 = XtVaCreateManagedWidget("6dB",
                xmToggleButtonGadgetClass,
                gain_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(goption7,XmNvalueChangedCallback,Gain_toggle,"6");

        // 7 dB
        goption8 = XtVaCreateManagedWidget("7dB",
                xmToggleButtonGadgetClass,
                gain_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(goption8,XmNvalueChangedCallback,Gain_toggle,"7");

        // 8 dB
        goption9 = XtVaCreateManagedWidget("8dB",
                xmToggleButtonGadgetClass,
                gain_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(goption9,XmNvalueChangedCallback,Gain_toggle,"8");

        // 9 dB
        goption10 = XtVaCreateManagedWidget("9dB",
                xmToggleButtonGadgetClass,
                gain_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(goption10,XmNvalueChangedCallback,Gain_toggle,"9");


        // Gain
        directivity_box = XmCreateRadioBox(formphg,
                "Configure_station Directivity Radio Box",
                al,
                ac);

        XtVaSetValues(directivity_box,
                XmNpacking, XmPACK_TIGHT,
                XmNorientation, XmHORIZONTAL,
                XmNtopAttachment,XmATTACH_WIDGET,
                XmNtopWidget,gain_box,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 5,
                XmNrightAttachment,XmATTACH_FORM,
                XmNrightOffset, 5,
                XmNnumColumns,10,
                NULL);


        // Omni-directional
        doption1 = XtVaCreateManagedWidget("Omni",
                xmToggleButtonGadgetClass,
                directivity_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(doption1,XmNvalueChangedCallback,Directivity_toggle,"0");

        // 45 NE
        doption2 = XtVaCreateManagedWidget("45°",
                xmToggleButtonGadgetClass,
                directivity_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(doption2,XmNvalueChangedCallback,Directivity_toggle,"1");

        // 90 E
        doption3 = XtVaCreateManagedWidget("90°",
                xmToggleButtonGadgetClass,
                directivity_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(doption3,XmNvalueChangedCallback,Directivity_toggle,"2");

        // 135 SE
        doption4 = XtVaCreateManagedWidget("135°",
                xmToggleButtonGadgetClass,
                directivity_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(doption4,XmNvalueChangedCallback,Directivity_toggle,"3");

        // 180 S
        doption5 = XtVaCreateManagedWidget("180°",
                xmToggleButtonGadgetClass,
                directivity_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(doption5,XmNvalueChangedCallback,Directivity_toggle,"4");

        // 225 SW
        doption6 = XtVaCreateManagedWidget("225°",
                xmToggleButtonGadgetClass,
                directivity_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(doption6,XmNvalueChangedCallback,Directivity_toggle,"5");

        // 270 W
        doption7 = XtVaCreateManagedWidget("270°",
                xmToggleButtonGadgetClass,
                directivity_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(doption7,XmNvalueChangedCallback,Directivity_toggle,"6");

        // 315 NW
        doption8 = XtVaCreateManagedWidget("315°",
                xmToggleButtonGadgetClass,
                directivity_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(doption8,XmNvalueChangedCallback,Directivity_toggle,"7");

        // 360 N
        doption9 = XtVaCreateManagedWidget("360°",
                xmToggleButtonGadgetClass,
                directivity_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(doption9,XmNvalueChangedCallback,Directivity_toggle,"8");


//-----------------------
        comment = XtVaCreateManagedWidget(langcode("WPUPCFS017"),
                xmLabelWidgetClass, 
                cs_form,
                XmNtopAttachment,           XmATTACH_WIDGET,
                XmNtopWidget,               framephg,
                XmNtopOffset,               15,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNleftAttachment,          XmATTACH_FORM,
                XmNleftOffset,              10,
                XmNrightAttachment,         XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);


        station_config_comment_data = XtVaCreateManagedWidget("Configure_station comment", 
                xmTextFieldWidgetClass, 
                cs_form,
                XmNeditable,                TRUE,
                XmNcursorPositionVisible,   TRUE,
                XmNsensitive,               TRUE,
                XmNshadowThickness,         1,
                XmNcolumns,                 60,
                XmNwidth,                   ((60*7)+2),
                XmNmaxLength,               MAX_COMMENT,
                XmNtopOffset,               11,
                XmNbackground,              colors[0x0f],
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              comment,
                XmNleftOffset,              5,
                XmNtopAttachment,           XmATTACH_WIDGET,
                XmNtopWidget,               framephg,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNrightAttachment,         XmATTACH_NONE,
                NULL);


//Position Ambiguity Frame
        frame2 = XtVaCreateManagedWidget("Configure_station frame2", 
                xmFrameWidgetClass, 
                cs_form,
                XmNtopAttachment,           XmATTACH_WIDGET,
                XmNtopWidget,               comment,
                XmNtopOffset,               10,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNleftAttachment,          XmATTACH_FORM,
                XmNleftOffset,              10,
                XmNrightAttachment,         XmATTACH_FORM,
                XmNrightOffset,             10,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        posamb  = XtVaCreateManagedWidget(langcode("WPUPCFS018"),
                xmLabelWidgetClass,
                frame2,
                XmNchildType,               XmFRAME_TITLE_CHILD,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        /*set args for color */
        ac=0;
        XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
        XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;

        option_box = XmCreateRadioBox(frame2,
                "Configure_station Option box",
                al,
                ac);

        XtVaSetValues(option_box,
                XmNnumColumns,5,
                NULL);

        posamb0 = XtVaCreateManagedWidget(langcode("WPUPCFS019"),
                xmToggleButtonGadgetClass,
                option_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(posamb0,XmNvalueChangedCallback,Configure_station_toggle,"0");

        posamb1 = XtVaCreateManagedWidget(units_english_metric?langcode("WPUPCFS020"):langcode("WPUPCFS024"),
                xmToggleButtonGadgetClass,
                option_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(posamb1,XmNvalueChangedCallback,Configure_station_toggle,"1");


        posamb2 = XtVaCreateManagedWidget(units_english_metric?langcode("WPUPCFS021"):langcode("WPUPCFS025"),
                xmToggleButtonGadgetClass,
                option_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(posamb2,XmNvalueChangedCallback,Configure_station_toggle,"2");

        posamb3 = XtVaCreateManagedWidget(units_english_metric?langcode("WPUPCFS022"):langcode("WPUPCFS026"),
                xmToggleButtonGadgetClass,
                option_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(posamb3,XmNvalueChangedCallback,Configure_station_toggle,"3");

        posamb4 = XtVaCreateManagedWidget(units_english_metric?langcode("WPUPCFS023"):langcode("WPUPCFS027"),
                xmToggleButtonGadgetClass,
                option_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);
        XtAddCallback(posamb4,XmNvalueChangedCallback,Configure_station_toggle,"4");

        sep = XtVaCreateManagedWidget("Configure_station sep", 
                xmSeparatorGadgetClass,
                cs_form,
                XmNorientation,             XmHORIZONTAL,
                XmNtopAttachment,           XmATTACH_WIDGET,
                XmNtopWidget,               frame2,
                XmNtopOffset,               14,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNleftAttachment,          XmATTACH_FORM,
                XmNrightAttachment,         XmATTACH_FORM,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        button_ok = XtVaCreateManagedWidget(langcode("UNIOP00001"),
                xmPushButtonGadgetClass, 
                cs_form,
                XmNtopAttachment,           XmATTACH_WIDGET,
                XmNtopWidget,               sep,
                XmNtopOffset,               5,
                XmNbottomAttachment,        XmATTACH_FORM,
                XmNbottomOffset,            5,
                XmNleftAttachment,          XmATTACH_POSITION,
                XmNleftPosition,            1,
                XmNrightAttachment,         XmATTACH_POSITION,
                XmNrightPosition,           2,
                XmNnavigationType,          XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        button_cancel = XtVaCreateManagedWidget(langcode("UNIOP00002"),
                xmPushButtonGadgetClass, 
                cs_form,
                XmNtopAttachment,           XmATTACH_WIDGET,
                XmNtopWidget,               sep,
                XmNtopOffset,               5,
                XmNbottomAttachment,        XmATTACH_FORM,
                XmNbottomOffset,            5,
                XmNleftAttachment,          XmATTACH_POSITION,
                XmNleftPosition,            3,
                XmNrightAttachment,         XmATTACH_POSITION,
                XmNrightPosition,           4,
                XmNnavigationType,          XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        XtAddCallback(button_ok, XmNactivateCallback, Configure_station_change_data, configure_station_dialog);
        XtAddCallback(button_cancel, XmNactivateCallback, Configure_station_destroy_shell, configure_station_dialog);

        // fill in current data
        XmTextFieldSetString(station_config_call_data,my_callsign);

        substr(temp_data,my_lat,2);
        XmTextFieldSetString(station_config_slat_data_deg,temp_data);
        substr(temp_data,my_lat+2,6);
        XmTextFieldSetString(station_config_slat_data_min,temp_data);
        substr(temp_data,my_lat+8,1);
        XmTextFieldSetString(station_config_slat_data_ns,temp_data);

        substr(temp_data,my_long,3);
        XmTextFieldSetString(station_config_slong_data_deg,temp_data);
        substr(temp_data,my_long+3,6);
        XmTextFieldSetString(station_config_slong_data_min,temp_data);
        substr(temp_data,my_long+9,1);
        XmTextFieldSetString(station_config_slong_data_ew,temp_data);

        temp_data[0] = my_group;
        temp_data[1] = '\0';
        XmTextFieldSetString(station_config_group_data,temp_data);
        XtAddCallback(station_config_group_data, XmNvalueChangedCallback, updateSymbolPictureCallback, configure_station_dialog);

        temp_data[0] = my_symbol;
        temp_data[1] = '\0';
        XmTextFieldSetString(station_config_symbol_data,temp_data);
        XtAddCallback(station_config_symbol_data, XmNvalueChangedCallback, updateSymbolPictureCallback, configure_station_dialog);

        // update symbol picture
        (void)updateSymbolPictureCallback((Widget)NULL,(XtPointer)NULL,(XtPointer)NULL);


        if (my_phg[0]=='P') {
            switch (my_phg[3]) {
                case '0':
                    XmToggleButtonGadgetSetState(poption1, TRUE, TRUE);
                    break;
                case '1':
                    XmToggleButtonGadgetSetState(poption2, TRUE, TRUE);
                    break;
                case '2':
                    XmToggleButtonGadgetSetState(poption3, TRUE, TRUE);
                    break;
                case '3':
                    XmToggleButtonGadgetSetState(poption4, TRUE, TRUE);
                    break;
                case '4':
                    XmToggleButtonGadgetSetState(poption5, TRUE, TRUE);
                    break;
                case '5':
                    XmToggleButtonGadgetSetState(poption6, TRUE, TRUE);
                    break;
                case '6':
                    XmToggleButtonGadgetSetState(poption7, TRUE, TRUE);
                    break;
                case '7':
                    XmToggleButtonGadgetSetState(poption8, TRUE, TRUE);
                    break;
                case '8':
                    XmToggleButtonGadgetSetState(poption9, TRUE, TRUE);
                    break;
                case '9':
                    XmToggleButtonGadgetSetState(poption10, TRUE, TRUE);
                    break;
                case 'x':
                default:
                    XmToggleButtonGadgetSetState(poption0, TRUE, TRUE);
                    break;
            }
            switch (my_phg[4]) {
                case '0':
                    XmToggleButtonGadgetSetState(hoption1, TRUE, TRUE);
                    break;
                case '1':
                    XmToggleButtonGadgetSetState(hoption2, TRUE, TRUE);
                    break;
                case '2':
                    XmToggleButtonGadgetSetState(hoption3, TRUE, TRUE);
                    break;
                case '3':
                    XmToggleButtonGadgetSetState(hoption4, TRUE, TRUE);
                    break;
                case '4':
                    XmToggleButtonGadgetSetState(hoption5, TRUE, TRUE);
                    break;
                case '5':
                    XmToggleButtonGadgetSetState(hoption6, TRUE, TRUE);
                    break;
                case '6':
                    XmToggleButtonGadgetSetState(hoption7, TRUE, TRUE);
                    break;
                case '7':
                    XmToggleButtonGadgetSetState(hoption8, TRUE, TRUE);
                    break;
                case '8':
                    XmToggleButtonGadgetSetState(hoption9, TRUE, TRUE);
                    break;
                case '9':
                    XmToggleButtonGadgetSetState(hoption10, TRUE, TRUE);
                    break;
                default:
                    break;
            }
            switch (my_phg[5]) {
                case '0':
                    XmToggleButtonGadgetSetState(goption1, TRUE, TRUE);
                    break;
                case '1':
                    XmToggleButtonGadgetSetState(goption2, TRUE, TRUE);
                    break;
                case '2':
                    XmToggleButtonGadgetSetState(goption3, TRUE, TRUE);
                    break;
                case '3':
                    XmToggleButtonGadgetSetState(goption4, TRUE, TRUE);
                    break;
                case '4':
                    XmToggleButtonGadgetSetState(goption5, TRUE, TRUE);
                    break;
                case '5':
                    XmToggleButtonGadgetSetState(goption6, TRUE, TRUE);
                    break;
                case '6':
                    XmToggleButtonGadgetSetState(goption7, TRUE, TRUE);
                    break;
                case '7':
                    XmToggleButtonGadgetSetState(goption8, TRUE, TRUE);
                    break;
                case '8':
                    XmToggleButtonGadgetSetState(goption9, TRUE, TRUE);
                    break;
                case '9':
                    XmToggleButtonGadgetSetState(goption10, TRUE, TRUE);
                    break;
                default:
                    break;
            }
            switch (my_phg[6]) {
                case '0':
                    XmToggleButtonGadgetSetState(doption1, TRUE, TRUE);
                    break;
                case '1':
                    XmToggleButtonGadgetSetState(doption2, TRUE, TRUE);
                    break;
                case '2':
                    XmToggleButtonGadgetSetState(doption3, TRUE, TRUE);
                    break;
                case '3':
                    XmToggleButtonGadgetSetState(doption4, TRUE, TRUE);
                    break;
                case '4':
                    XmToggleButtonGadgetSetState(doption5, TRUE, TRUE);
                    break;
                case '5':
                    XmToggleButtonGadgetSetState(doption6, TRUE, TRUE);
                    break;
                case '6':
                    XmToggleButtonGadgetSetState(doption7, TRUE, TRUE);
                    break;
                case '7':
                    XmToggleButtonGadgetSetState(doption8, TRUE, TRUE);
                    break;
                case '8':
                    XmToggleButtonGadgetSetState(doption9, TRUE, TRUE);
                    break;
                default:
                    break;
            }
        } else {  // PHG field disabled
            XmToggleButtonGadgetSetState(poption0, TRUE, TRUE);
        }


        XmTextFieldSetString(station_config_comment_data,my_comment);


        if(transmit_compressed_posit) {
            // Compressed posits don't allow position ambiguity
            XmToggleButtonSetState(compressed_posit_tx,TRUE,FALSE);
            position_amb_chars = 0;
            XtSetSensitive(posamb0,FALSE);
            XtSetSensitive(posamb1,FALSE);
            XtSetSensitive(posamb2,FALSE);
            XtSetSensitive(posamb3,FALSE);
            XtSetSensitive(posamb4,FALSE);
        }
        else {  // Position ambiguity ok for this mode
            XmToggleButtonSetState(compressed_posit_tx,FALSE,FALSE);

            XtSetSensitive(posamb0,TRUE);
            XtSetSensitive(posamb1,TRUE);
            XtSetSensitive(posamb2,TRUE);
            XtSetSensitive(posamb3,TRUE);
            XtSetSensitive(posamb4,TRUE);
        }
 
        Configure_station_pos_amb = position_amb_chars;
        switch (Configure_station_pos_amb) {
            case(0):
                XmToggleButtonSetState(posamb0,TRUE,FALSE);
                break;

            case(1):
                XmToggleButtonSetState(posamb1,TRUE,FALSE);
                break;

            case(2):
                XmToggleButtonSetState(posamb2,TRUE,FALSE);
                break;

            case(3):
                XmToggleButtonSetState(posamb3,TRUE,FALSE);
                break;

            case(4):
                XmToggleButtonSetState(posamb4,TRUE,FALSE);
                break;

            default:
                XmToggleButtonSetState(posamb0,TRUE,FALSE);
                break;
        }

        pos_dialog(configure_station_dialog);

        delw = XmInternAtom(XtDisplay(configure_station_dialog),"WM_DELETE_WINDOW", FALSE);
        XmAddWMProtocolCallback(configure_station_dialog, delw, Configure_station_destroy_shell, (XtPointer)configure_station_dialog);

        XtManageChild(cs_form);
        XtManageChild(cs_form1);
        XtManageChild(option_box);
        XtManageChild(power_box);
        XtManageChild(height_box);
        XtManageChild(gain_box);
        XtManageChild(directivity_box);
        XtManageChild(formphg);
        XtManageChild(pane);

        XtPopup(configure_station_dialog,XtGrabNone);
        fix_dialog_size(configure_station_dialog);

        // Move focus to the Close button.  This appears to highlight the
        // button fine, but we're not able to hit the <Enter> key to
        // have that default function happen.  Note:  We _can_ hit the
        // <SPACE> key, and that activates the option.
//        XmUpdateDisplay(configure_station_dialog);
        XmProcessTraversal(button_cancel, XmTRAVERSE_CURRENT);

    } else
        (void)XRaiseWindow(XtDisplay(configure_station_dialog), XtWindow(configure_station_dialog));
}





/////////////////////////////////////////////   main   /////////////////////////////////////////////


int main(int argc, char *argv[], char *envp[]) {
    int ag, ag_error, trap_segfault;
    uid_t user_id;
    struct passwd *user_info;
    static char lang_to_use_or[30];
    char temp[100];

#ifdef USING_SOLARIS
    extern char *optarg;
#endif


    // Define some overriding resources for the widgets.
    // Look at files in /usr/X11/lib/X11/app-defaults for ideas.
    String fallback_resources[] = {

        "*initialResourcesPersistent: False\n",

        "*List.Translations: #override \n\
        <Key>Return:    Select(children)\n\
        <Key>space:     Select(children)\n",
 
        "*List.baseTranslations: #override \n\
        <Key>Return:    Select(children)\n\
        <Key>space:     Select(children)\n",
 
        "*XmTextField.translations: #override \
        <Key>Return:    activate()\n\
        <Key>Enter:     activate()\n",

        "*.Text.Translations: #override\n\
        Ctrl<Key>S:     no-op(RingBell)\n\
        Ctrl<Key>R:     no-op(RingBell)\n\
        <Key>space:     next-page()\n\
        <Key>F:         next-page()\n\
        Ctrl<Key>B:     previous-page()\n\
        <Key>B:         previous-page()\n\
        <Key>K:         scroll-one-line-down()\n\
        <Key>Y:         scroll-one-line-down()\n\
        <Key>J:         scroll-one-line-up()\n\
        <Key>E:         scroll-one-line-up()\n\
        <Key>Return:    scroll-one-line-up()\n\
        <Key>q:         quit()\n",
 
        "*.Text.baseTranslations: #override\n\
        <Key>space:     next-page()\n\
        <Key>F:         next-page()\n\
        Ctrl<Key>B:     previous-page()\n\
        <Key>K:         scroll-one-line-down()\n\
        <Key>Y:         scroll-one-line-down()\n\
        <Key>J:         scroll-one-line-up()\n\
        <Key>E:         scroll-one-line-up()\n\
        <Key>Return:    scroll-one-line-up()\n\
        <Key>q:         quit()\n",
 
        "*.vertical.Translations: #override\n\
        <Key>space: StartScroll(Forward)  NotifyScroll(FullLength) EndScroll() \n\
        <Key>Delete: StartScroll(Backward) NotifyScroll(FullLength) EndScroll() \n\
        Ctrl<Key>Up:        KbdScroll(up,50) EndScroll()\n\
        Ctrl<Key>Down:      KbdScroll(down,50) EndScroll()\n\
        <Key>Up:            KbdScroll(up,10) EndScroll()\n\
        <Key>Down:          KbdScroll(down,10) EndScroll()\n\
        <Key>Page_Up:       KbdScroll(up,90) EndScroll()\n\
        <Key>Page_Down:     KbdScroll(down,90) EndScroll()\n",

        "*.horizontal.translations:      #override \n\
        Ctrl<Key>Left:      KbdScroll(left,50) EndScroll()\n\
        Ctrl<Key>Right:     KbdScroll(right,50) EndScroll()\n\
        <Key>Left:          KbdScroll(left,10) EndScroll()\n\
        <Key>Right:         KbdScroll(right,10) EndScroll()\n",

        "*.horizontal.accelerators:      #override \n\
        Ctrl<Key>Left:      KbdScroll(left,50) EndScroll()\n\
        Ctrl<Key>Right:     KbdScroll(right,50) EndScroll()\n\
        <Key>Left:          KbdScroll(left,10) EndScroll()\n\
        <Key>Right:         KbdScroll(right,10) EndScroll()\n",

        "*.vertical.accelerators:        #override \n\
        Ctrl<Key>Up:        KbdScroll(up,50) EndScroll()\n\
        Ctrl<Key>Down:      KbdScroll(down,50) EndScroll()\n\
        <Key>Up:            KbdScroll(up,10) EndScroll()\n\
        <Key>Down:          KbdScroll(down,10) EndScroll()\n\
        <Key>Page_Up:       KbdScroll(up,90) EndScroll()\n\
        <Key>Page_Down:     KbdScroll(down,90) EndScroll()\n",

        "*.nodeText.Translations: #override \n\
        None<Key>b:     beginning-of-file() \n\
        <Key>Home:      beginning-of-file() \n\
        <Key>Delete:    previous-page() \n\
        <Key>Prior:     previous-page() \n\
        <Key>Next:      next-page() \n\
        <Key>space:     next-page() \n\
        None<Btn1Down>: select-end() info_click() \n",

        "*.arg.translations:    #override \n\
        <Key>Return: confirm() \n\
        Ctrl<Key>G:  abort() \n",

        "*.pane.text.translations:         #override \n\
        None<Key>q:     MenuPopdown(help) \n\
        None<Key>b:     beginning-of-file() \n\
        <Key>Home:      beginning-of-file() \n\
        <Key>Delete:    previous-page() \n\
        <Key>Prior:     previous-page() \n\
        <Key>Next:      next-page() \n\
        <Key>space:     next-page() \n",

        "*.dialog.value.translations:     #override \n\
        <Key>Return: confirm() \n\
        Ctrl<Key>G:  abort() \n\
        <Key>Tab: Complete()\n",

        "*.dialog.value.baseTranslations:     #override \n\
        <Key>Return: confirm() \n\
        Ctrl<Key>G:  abort() \n",

        "*.baseTranslations: #override \n\
        Ctrl<Key>q: Quit()\n\
        Ctrl<Key>c: Quit()\n\
        <Key>Return: Accept()\n",

        "*Translations: #override \n\
        <Key>q: Quit()\n\
        <Key>c: Quit()\n\
        Ctrl <Key>n: Next()\n\
        Ctrl <Key>p: Prev()\n",

        NULL
    };

    euid = geteuid();
    egid = getegid();

    DISABLE_SETUID_PRIVILEGE;

    program_start_time = sec_now(); // For use by "Display Uptime"


#ifdef HAVE_IMAGEMAGICK
    #if (MagickLibVersion < 0x0538)
        MagickIncarnate(*argv);
    #else
        InitializeMagick(*argv);
    #endif
#endif

    /* get User info */
    user_id   = getuid();
    user_info = getpwuid(user_id);
    strcpy(user_dir,user_info->pw_dir);

    /*
        printf("User %s, Dir %s\n",user_info->pw_name,user_dir);
        printf("User dir %s\n",get_user_base_dir(""));
        printf("Data dir %s\n",get_data_base_dir(""));
    */

    /* check user directories */
    if (filethere(get_user_base_dir("")) != 1) {
        printf("Making user dir\n");
        (void)mkdir(get_user_base_dir(""),S_IRWXU);
    }

    if (filethere(get_user_base_dir("config")) != 1) {
        printf("Making user config dir\n");
        (void)mkdir(get_user_base_dir("config"),S_IRWXU);
    }

    if (filethere(get_user_base_dir("data")) != 1) {
        printf("Making user data dir\n");
        (void)mkdir(get_user_base_dir("data"),S_IRWXU);
    }

    if (filethere(get_user_base_dir("logs")) != 1) {
        printf("Making user log dir\n");
        (void)mkdir(get_user_base_dir("logs"),S_IRWXU);
    }

    if (filethere(get_user_base_dir("tracklogs")) != 1) {
        printf("Making user tracklogs dir\n");
        (void)mkdir(get_user_base_dir("tracklogs"),S_IRWXU);
    }


    /* done checking user dirs */

    /* check fhs directories ?*/

    /* setup values */
    redo_list = FALSE;          // init lists

    delay_time = 0;
    last_weather_cycle = sec_now();
    packet_data_display = 0;
    redraw_on_new_packet_data = 0;
    next_file_read = sec_now();         // init file replay timing
    redraw_on_new_data = 0;
    display_up = 0;
    display_up_first = 0;
    max_transmit_time = (time_t)900l;
    sec_next_gps = 0l;
    sec_next_raw_wx = 0l;
    auto_reply = 0;
    satellite_ack_mode = 0;
    last_time = 0l;
    next_time = (time_t)120l;
    net_last_time = 0l;
    net_next_time = (time_t)120l;
    posit_last_time = 0l;
    posit_next_time = 0l;
    wait_to_redraw=1;
    tiger_flag = FALSE;

    last_popup_x = 0;
    last_popup_y = 0;
    trap_segfault = 0;
    debug_level = 0;
    install_colormap = 0;
    last_sound_pid = 0;

    my_last_course = 0;
    my_last_speed = 0;
    my_last_altitude = 0l;
    my_last_altitude_time = 0l;

    strcpy(wx_station_type,"");
    last_alert_redraw = 0;
    igate_msgs_tx = 0;
    last_statusline = 0;        // inactive statusline
    
    last_object[0] = '\0';      // initialize object dialog
    last_obj_grp   = '\\';
    last_obj_sym   = '!';

    clear_rain_data();          // init weather data
    clear_local_wx_data();

    next_redraw = sec_now()+60; // init redraw timing
    last_redraw = sec_now();

    strcpy(lang_to_use_or,"");
    ag_error=0;
    while ((ag = getopt(argc, argv, "v:l:012346789ti")) != EOF) {
        switch (ag) {
        case 't':
            printf("Trap segfault\n");
        trap_segfault = (int)TRUE;
        break;
        case 'v':
                printf ("debug");
                if (optarg) {
                    debug_level = atoi(optarg);
                    printf (" level %d", debug_level);
                }
                printf ("\n");
                break;

            case 'l':
                printf ("Language is");
                if (optarg) {
                    strcpy(lang_to_use_or,"");
                    if (strcasecmp(optarg,"ENGLISH") == 0) {
                        strcpy(lang_to_use_or,"English");
                    } else if (strcasecmp(optarg,"DUTCH") == 0) {
                        strcpy(lang_to_use_or,"Dutch");
                    } else if (strcasecmp(optarg,"FRENCH") == 0) {
                        strcpy(lang_to_use_or,"French");
                    } else if (strcasecmp(optarg,"GERMAN") == 0) {
                        strcpy(lang_to_use_or,"German");
                    } else if (strcasecmp(optarg,"SPANISH") == 0) {
                        strcpy(lang_to_use_or,"Spanish");
                    } else if (strcasecmp(optarg,"ITALIAN") == 0) {
                        strcpy(lang_to_use_or,"Italian");
                    } else if (strcasecmp(optarg,"PORTUGUESE") == 0) {
                        strcpy(lang_to_use_or,"Portuguese");
                    } else {
                        ag_error++;
                        printf(" INVALID");
                    }
                    if (!ag_error)
                        printf(" %s", lang_to_use_or);

                }
                printf ("\n");
                break;

        case 'i':
            printf("Install Colormap\n");
            install_colormap = (int)TRUE;
            break;

            default:
                ag_error++;
                break;
        }
    }

    if (ag_error){
        printf("\nXastir Command line Options\n\n");
        printf("-v level      Set the debug level\n\n");
        printf("-lDutch       Set the language to Dutch\n");
        printf("-lEnglish     Set the language to English\n");
        printf("-lFrench      Set the language to French\n");
        printf("-lGerman      Set the language to German\n");
        printf("-lItalian     Set the language to Italian\n");
        printf("-lPortuguese  Set the language to Portuguese\n");
        printf("-lSpanish     Set the language to Spanish\n");
        printf("-i            Install private Colormap\n");
        printf("\n");
        exit(0);
    }

    // initialize interfaces
    init_critical_section(&port_data_lock);   // Protects the port_data[] array of structs
    init_critical_section(&output_data_lock); // Protects interface.c:channel_data() function only
    init_critical_section(&data_lock);        // Protects global data, data_port, data_avail variables
    init_critical_section(&connect_lock);     // Protects port_data[].thread_status and port_data[].connect_status
// We should probably protect redraw_on_new_data, alert_redraw_on_update, and
// redraw_on_new_packet_data variables as well?
// Also need to protect dialogs.

    (void)bulletin_gui_init();
    (void)db_init();
    (void)draw_symbols_init();
    (void)interface_gui_init();
    (void)list_gui_init();
    (void)locate_gui_init();
    (void)location_gui_init();
    (void)maps_init();
    (void)messages_gui_init();
    (void)popup_gui_init();
    (void)track_gui_init();
    (void)view_message_gui_init();
    (void)wx_gui_init();
    (void)igate_init();

    clear_all_port_data();                              // clear interface port data

    (void) signal(SIGINT,quit);                         // set signal on stop
    (void) signal(SIGQUIT,quit);
    (void) signal(SIGTERM,quit);
    (void) signal(SIGPIPE,SIG_IGN);                     // set sigpipe signal to ignore
    if (!trap_segfault)
        (void) signal(SIGSEGV,segfault);                // set segfault signal to check

    load_data_or_default(); // load program parameters or set to default values

    update_units(); // set up conversion factors and strings

    /* do language links */
    if (strlen(lang_to_use_or) > 0)
        strcpy(lang_to_use,lang_to_use_or);

    xastir_snprintf(temp, sizeof(temp), "help/help-%s.dat", lang_to_use);
    (void)unlink(get_user_base_dir("config/help.dat"));

    // Note that this symlink will probably not fail.  It's easy to
    // create a symlink that points to nowhere.
    if (symlink(get_data_base_dir(temp),get_user_base_dir("config/help.dat")) == -1) {
        printf("Error creating database link\n");
        exit(0);
    }

    xastir_snprintf(temp, sizeof(temp), "config/language-%s.sys", lang_to_use);
    (void)unlink(get_user_base_dir("config/language.sys"));

    // Note that this symlink will probably not fail.  It's easy to
    // create a symlink that points to nowhere.
    if (symlink(get_data_base_dir(temp),get_user_base_dir("config/language.sys")) == -1) {
        printf("Error creating database link\n");
        exit(0);
    }

    /* (NEW) set help file area */
    strcpy(HELP_FILE,get_user_base_dir("config/help.dat"));

#ifdef HAVE_FESTIVAL
    /* Initialize the festival speech synthesis port */
    if (SayTextInit())
    {
        printf("Error connecting to Festival speech server.\n");
        exit(0);
    }  
#endif

    if (load_color_file()) {
        if (load_language_file(get_user_base_dir("config/language.sys"))) {
            init_device_names();                // set interface names
            clear_message_windows();
            clear_popup_message_windows();
            init_station_data();                // init station storage
            init_message_data();                // init messages
            reset_outgoing_messages();


            /* set language attribs */
            (void)XtSetLanguageProc((XtAppContext) NULL, (XtLanguageProc) NULL, (XtPointer) NULL );
            XtToolkitInitialize();

            // ERROR:
            Global.top = XtVaAppInitialize(&app_context,"Xastir", NULL, 0,
                                       &argc, argv,
                                       fallback_resources,
                                       XmNmappedWhenManaged, FALSE,
                                       NULL);
            // DK7IN: now scanf and printf use "," instead of "."
            // that leads to several problems in the initialisation

            // DK7IN: inserted next line here for avoiding scanf errors during init!
            (void)setlocale(LC_NUMERIC, "en_US");       // DK7IN: It's now ok

            // DK7IN: XtOpenDisplay again changes the scanf function
            display = XtOpenDisplay(app_context, NULL, argv[0], "XApplication",NULL, 0, &argc, argv);
            (void)setlocale(LC_NUMERIC, "en_US");       // repairs wrong scanf
            // DK7IN: scanf again uses '.' instead of ','

            if (!display) {
                printf("%s: can't open display, exiting...\n", argv[0]);
                exit (-1);
            }

            setup_visual_info(display, DefaultScreen(display));

            /* Get colormap (N7TAP: do we need this if the screen visual is TRUE or DIRECT? */
            cmap = DefaultColormapOfScreen(XtScreen(Global.top));
            if (visual_type == NOT_TRUE_NOR_DIRECT) {
                if (install_colormap) {
                    cmap = XCopyColormapAndFree(display, cmap);
                    XtVaSetValues(Global.top, XmNcolormap, cmap, NULL);
                }
            }

            XtRealizeWidget(Global.top);

            // Read the current map index file into the index linked list
            index_restore_from_file();

            create_appshell(display, argv[0], argc, argv);      // does the init

            /* reset language attribs for numeric, program needs decimal in US for all data! */
//            (void)setlocale(LC_NUMERIC, "en_US");
            // DK7IN: now scanf and printf work as wanted...

            /* check for ham databases */
            (void)check_fcc_data();
            (void)check_rac_data();

            // Find the extents of every map we have
            map_indexer();

            // Start UpdateTime().  It schedules itself to be run
            // again each time.  This is also the process that
            // starts up the interfaces.
            UpdateTime( (XtPointer) da , (XtIntervalId) NULL );

            // Reload saved objects and items from previous runs.
            // This implements persistent objects.
            reload_object_item();


            XtAppMainLoop(app_context);

        } else
            printf("Error in language file! Exiting...\n");

    } else
        printf("Error in Color file! Exiting...\n");

    quit(0);
    return 0;
}

