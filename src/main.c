/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
 * $Id$
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2008  The Xastir Group
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



// This is for debug.  If defined to 1, Xastir will display
// coordinates in the Xastir coordinate system inside the text2
// widget.
//
static int DISPLAY_XASTIR_COORDINATES = 0;



#ifdef HAVE_CONFIG_H
#include "config.h"
#endif  // HAVE_CONFIG_H

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
#include <locale.h>

// Needed for Solaris
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif  // HAVE_STRINGS_H

#include <sys/wait.h>
#include <errno.h>

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else   // TIME_WITH_SYS_TIME
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else  // HAVE_SYS_TIME_H
#  include <time.h>
# endif // HAVE_SYS_TIME_H
#endif  // TIME_WITH_SYS_TIME

// TVR -- stupid, stupid ImageMagick
char *xastir_package=PACKAGE;
char *xastir_version=VERSION;
#undef PACKAGE
#undef VERSION

#ifdef HAVE_MAGICK
#include <sys/types.h>
#undef RETSIGTYPE
/* JMT - stupid ImageMagick */
#define XASTIR_PACKAGE_BUGREPORT PACKAGE_BUGREPORT
#undef PACKAGE_BUGREPORT
#define XASTIR_PACKAGE_NAME PACKAGE_NAME
#undef PACKAGE_NAME
#define XASTIR_PACKAGE_STRING PACKAGE_STRING
#undef PACKAGE_STRING
#define XASTIR_PACKAGE_TARNAME PACKAGE_TARNAME
#undef PACKAGE_TARNAME
#define XASTIR_PACKAGE_VERSION PACKAGE_VERSION
#undef PACKAGE_VERSION
#include <magick/api.h>
#undef PACKAGE_BUGREPORT
#define PACKAGE_BUGREPORT XASTIR_PACKAGE_BUGREPORT
#undef XASTIR_PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#define PACKAGE_NAME XASTIR_PACKAGE_NAME
#undef XASTIR_PACKAGE_NAME
#undef PACKAGE_STRING
#define PACKAGE_STRING XASTIR_PACKAGE_STRING
#undef XASTIR_PACKAGE_STRING
#undef PACKAGE_TARNAME
#define PACKAGE_TARNAME XASTIR_PACKAGE_TARNAME
#undef XASTIR_PACKAGE_TARNAME
#undef PACKAGE_VERSION
#define PACKAGE_VERSION XASTIR_PACKAGE_VERSION
#undef XASTIR_PACKAGE_VERSION
#endif // HAVE_MAGICK

#ifdef  HAVE_LIBINTL_H
#include <libintl.h>
#define _(x)        gettext(x)
#else   // HAVE_LIBINTL_H
#define _(x)        (x)
#endif  // HAVE_LIBINTL_H

//#ifdef HAVE_NETAX25_AXLIB_H
//#include <netax25/axlib.h>
//#endif    // HAVE_NETAX25_AXLIB_H

#ifdef HAVE_LIBCURL
#include <curl/curl.h>
#endif

#ifdef HAVE_LIBGDAL
// WE7U - stupid ImageMagick
#define XASTIR_PACKAGE_BUGREPORT PACKAGE_BUGREPORT
#undef PACKAGE_BUGREPORT
#define XASTIR_PACKAGE_NAME PACKAGE_NAME
#undef PACKAGE_NAME
#define XASTIR_PACKAGE_STRING PACKAGE_STRING
#undef PACKAGE_STRING
#define XASTIR_PACKAGE_TARNAME PACKAGE_TARNAME
#undef PACKAGE_TARNAME
#define XASTIR_PACKAGE_VERSION PACKAGE_VERSION
#undef PACKAGE_VERSION
#include <ogr_api.h>
#undef PACKAGE_BUGREPORT
#define PACKAGE_BUGREPORT XASTIR_PACKAGE_BUGREPORT
#undef XASTIR_PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#define PACKAGE_NAME XASTIR_PACKAGE_NAME
#undef XASTIR_PACKAGE_NAME
#undef PACKAGE_STRING
#define PACKAGE_STRING XASTIR_PACKAGE_STRING
#undef XASTIR_PACKAGE_STRING
#undef PACKAGE_TARNAME
#define PACKAGE_TARNAME XASTIR_PACKAGE_TARNAME
#undef XASTIR_PACKAGE_TARNAME
#undef PACKAGE_VERSION
#define PACKAGE_VERSION XASTIR_PACKAGE_VERSION
#undef XASTIR_PACKAGE_VERSION
#include <gdal.h>
#endif

#include "xastir.h"
#include "draw_symbols.h"
#include "main.h"
#include "xa_config.h"
#include "maps.h"
#include "alert.h"
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
#include "objects.h"
#include "db_gis.h"

#ifdef HAVE_LIBSHP
  #include "shp_hash.h"
#endif  // HAVE_LIBSHP

#include "x_spider.h"
#include "map_cache.h"

#include <Xm/XmAll.h>
#include <X11/cursorfont.h>
#include <Xm/ComboBox.h>

// Must be last include file
#include "leak_detection.h"



// Copyright 2008.
// Added the above "Copyright" just so that grep would find these
// lines and we could update the years in the Help->About message
// each time.  Otherwise it often gets missed when we're updating
// the years.
#define ABOUT_MSG "X Amateur Station Tracking and Information Reporting\n\n        http://www.xastir.org\n\nCopyright (C) 1999-2000  Frank Giannandrea\nCopyright (C) 1999-2008  The Xastir Group\nSee the \"LICENSE\" file for other applicable copyrights"


// Define this if you want an xastir.pid file created in the
// ~/.xastir directory and want to check that there's not another
// copy of Xastir running before a new one starts up.  You can also
// use this to send SIGHUP or SIGUSR1 signals to a running Xastir
// from scripts.
#define USE_PID_FILE_CHECK 1


#define DOS_HDR_LINES 8

#define STATUSLINE_ACTIVE 10    /* status line is cleared after 10 seconds */
#define REPLAY_DELAY       0    /* delay between replayed packets in sec, 0 is ok */
#define REDRAW_WAIT        3    /* delay between consecutive redraws in seconds (file load) */



// FONTS FONTS FONTS FONTS FONTS
//
// NOTE:  See the main() function at the bottom of this module for
// the default font definition.  xa_config.c is where fonts get
// saved/restored for user-defined fonts.
// This one is not used anymore:
//#define XmFONTLIST_DEFAULT_MY "-misc-fixed-*-r-*-*-10-*-*-*-*-*-*-*"


// This one goes right along with smaller system fonts on fixed-size
// LCD screens.  Fix new dialogs to the upper left of the main
// window, don't have them cycle through multiple possible positions
// as each new dialog is drawn.
//
//#define FIXED_DIALOG_STARTUP

// Yet another useful item:  Puts the mouse menu on button 1 instead
// of button3.  Useful for one-button devices like touchscreens.
//
//#define SWAP_MOUSE_BUTTONS
 
// If next line uncommented, Xastir will display the status line
// in 2 rows instead of the normal single row.  Formatted especially
// for 640 pixel wide screens.  It also gives a little extra room for 
// the number of stations and the Zoom factor.
// #define USE_TWO_STATUS_LINES

// Enable this next line to set all flags properly for a 640x480
// touch-screen:  Makes the main window smaller due to the reduced
// font sizes, makes all dialogs come up at the upper-left of the
// main Xastir screen, reverses buttons 1 and 3 so that the more
// important mouse menus are accessible via the touch-screen, and
// sets it for 2 status lines.  Make sure to change the system font
// size smaller than the default.
//
//#define LCD640x480TOUCH
//
#ifdef LCD640x480TOUCH
  #define FIXED_DIALOG_STARTUP
  #define SWAP_MOUSE_BUTTONS
  #define USE_TWO_STATUS_LINES
#endif


#define LINE_WIDTH 1

#define ARROWS     1            // Arrow buttons on menubar

// TVR 26 July 2005
// Moved this magic number to a #define --- there were numerous places 
// where this constant was hard coded, making it difficult to change the
// map properties line format without breaking something.  Now it can live
// in one place that needs to be updated when the properties line is changed.
// At the time of writing, the properties line had the followign format:
// min max lyr fil drg amap name
// %5d %5d %5d %5c %5c %5c  %s
// placing the name at offset 37
#define MPD_FILENAME_OFFSET 37


// Define the ICON, created with the "bitmap" editor:
#include "icon.xbm"

// lesstif (at least as of version 0.94 in 2008), doesn't
// have full implementation of combo boxes.
#ifndef USE_COMBO_BOX
#if (XmVERSION >= 2 && !defined(LESSTIF_VERSION))
#  define USE_COMBO_BOX 1
#endif
#endif  // USE_COMBO_BOX

int geometry_x, geometry_y;
unsigned int geometry_width, geometry_height;
int geometry_flags;

static int initial_load = 1;
int first_time_run = 0;

/* JMT - works under FreeBSD */
uid_t euid;
gid_t egid;


int   my_argc;
char **my_argv;
char **my_envp;
int  restart_xastir_now = 0;


// A count of the stations currently on the screen.  Counted by
// db.c:display_file() routine.
int currently_selected_stations      = 0;
int currently_selected_stations_save = 0;

// If my_trail_diff_color is 0, all my calls (SSIDs) will use MY_TRAIL_COLOR.
// If my_trail_diff_color = 1 then each different ssid for my callsign will use a different color.
int my_trail_diff_color = 0;  


// Used in segfault handler
char dangerous_operation[200];

FILE *file_wx_test;

int tcp_server_pid = 0;
int udp_server_pid = 0;

int serial_char_pacing;  // Inter-char delay in ms for serial ports.
int dtr_on = 1;
time_t sec_last_dtr = (time_t)0;

time_t last_updatetime = (time_t)0;
int time_went_backwards = 0;

/* language in use */
char lang_to_use[30];

/* version info in main.h */
int  altnet;
char altnet_call[MAX_CALLSIGN+1];

void da_input(Widget w, XtPointer client_data, XtPointer call_data);
void da_resize(Widget w, XtPointer client_data, XtPointer call_data);
void da_expose(Widget w, XtPointer client_data, XtPointer call_data);

void BuildPredefinedSARMenu_UI(Widget *parent_menu);
Widget *predefined_object_menu_parent;
Widget sar_object_sub;
Widget predefined_object_menu_items[MAX_NUMBER_OF_PREDEFINED_OBJECTS];

int debug_level;

//Widget hidden_shell;
Widget appshell;
Widget form;
Widget da;
Widget text;
Widget text2;
Widget text3;
Widget text4;
Widget log_indicator;
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
Widget cad_draw_button;

Widget CAD_close_polygon_menu_item;

int Station_transmit_type;
int Igate_type;

Widget Display_data_dialog  = (Widget)NULL;
Widget Display_data_text;
int Display_packet_data_type;
int show_only_station_capabilities = 0;
int Display_packet_data_mine_only = 0;

Widget configure_defaults_dialog = (Widget)NULL;
Widget configure_timing_dialog = (Widget)NULL;
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
coordinate_calc_array_type coordinate_calc_array;



// --------------------------- help menu -----------------------------
Widget help_list;
Widget help_index_dialog = (Widget)NULL;
Widget help_view_dialog  = (Widget)NULL;
Widget emergency_beacon_toggle;
int emergency_beacon = 0;
static void Help_About(Widget w, XtPointer clientData, XtPointer callData);
static void Help_Index(Widget w, XtPointer clientData, XtPointer callData);
void  Emergency_beacon_toggle( Widget widget, XtPointer clientData, XtPointer callData);

// ----------------------------- map ---------------------------------
Widget map_list;
Widget map_properties_list;
void map_index_update_temp_select(char *filename, map_index_record **current);
void map_index_temp_select_clear(void);
 
void map_chooser_fill_in (void);
int map_chooser_expand_dirs = 0;

void map_chooser_init (void);

Widget map_chooser_dialog = (Widget)NULL;
Widget map_chooser_button_ok = (Widget)NULL;
Widget map_chooser_button_cancel = (Widget)NULL;
 
Widget map_properties_dialog = (Widget)NULL;
static void Map_chooser(Widget w, XtPointer clientData, XtPointer callData);
Widget map_chooser_maps_selected_data = (Widget)NULL;
int re_sort_maps = 1;

#ifdef HAVE_MAGICK
static void Config_tiger(Widget w, XtPointer clientData, XtPointer callData);
#endif  // HAVE_MAGICK

#ifdef HAVE_LIBGEOTIFF
static void Config_DRG(Widget w, XtPointer clientData, XtPointer callData);
#endif  // HAVE_LIBGEOTIFF

Widget grid_on, grid_off;
static void Grid_toggle( Widget w, XtPointer clientData, XtPointer calldata);
int long_lat_grid;              // Switch for Map Lat and Long grid display

void Map_border_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData);
int draw_labeled_grid_border = FALSE;   // Toggle labeled border around map.


static void CAD_draw_toggle( Widget w, XtPointer clientData, XtPointer calldata);
int disable_all_maps = 0;
static void Map_disable_toggle( Widget w, XtPointer clientData, XtPointer calldata);

static void Map_auto_toggle( Widget w, XtPointer clientData, XtPointer calldata);
int map_auto_maps;              /* toggle use of auto_maps */
static void Map_auto_skip_raster_toggle( Widget w, XtPointer clientData, XtPointer calldata);
int auto_maps_skip_raster;
Widget map_auto_skip_raster_button;
Widget map_border_button;

Widget map_levels_on, map_levels_off;
static void Map_levels_toggle( Widget w, XtPointer clientData, XtPointer calldata);
int map_color_levels;           /* toggle use of map_color_levels */

Widget map_labels_on, map_labels_off;
static void Map_labels_toggle( Widget w, XtPointer clientData, XtPointer calldata);
int map_labels;                 // toggle use of map_labels */

Widget map_fill_on, map_fill_off;
static void Map_fill_toggle( Widget w, XtPointer clientData, XtPointer calldata);
int map_color_fill;             /* Whether or not to fill in map polygons with solid color */

int index_maps_on_startup;      // Index maps on startup
static void Index_maps_on_startup_toggle(Widget w, XtPointer clientData, XtPointer calldata);

Widget map_bgcolor[12];
static void Map_background(Widget w, XtPointer clientData, XtPointer calldata);
int map_background_color;       /* Background color for maps */

#if !defined(NO_GRAPHICS)
Widget raster_intensity[11];
static void Raster_intensity(Widget w, XtPointer clientData, XtPointer calldata);
#if defined(HAVE_MAGICK)
Widget gamma_adjust_dialog = (Widget)NULL;
Widget gamma_adjust_text;
#endif  // HAVE_MAGICK
#endif  // NO_GRAPHICS

Widget map_font_dialog = (Widget)NULL;
Widget map_font_text[FONT_MAX];


Widget map_station_label0,map_station_label1,map_station_label2;
static void Map_station_label(Widget w, XtPointer clientData, XtPointer calldata);
int letter_style;               /* Station Letter style */

Widget map_icon_outline0,map_icon_outline1,map_icon_outline2,map_icon_outline3;
static void Map_icon_outline(Widget w, XtPointer clientData, XtPointer calldata);
int icon_outline_style;         /* Icon Outline style */

Widget map_wx_alerts_0,map_wx_alerts_1;
static void Map_wx_alerts_toggle(Widget w, XtPointer clientData, XtPointer calldata);
int wx_alert_style;             /* WX alert map style */
time_t map_refresh_interval = 0; /* how often to refresh maps, seconds */
time_t map_refresh_time = 0;     /* when to refresh maps next, seconds */

// ------------------------ Filter and Display menus -----------------------------
Selections Select_ = { 0, // none
                       1, // mine
                       1, // tnc
                       1, // direct
                       1, // via_digi
                       1, // net
                       0, // tactical only 
                       1, // old_data

                       1, // stations
                       1, // fixed_stations
                       1, // moving_stations
                       1, // weather_stations
                       1, // CWOP_wx_stations
                       1, // objects
                       1, // weather_objects
                       1, // gauge_objects
                       1, // other_objects
};

What_to_display Display_ = { 1, // callsign
                             1, // label_all_trackpoints
                             1, // symbol
                             1, // symbol_rotate
                             1, // trail

                             1, // course
                             1, // speed
                             1, // speed_short
                             1, // altitude

                             1, // weather
                             1, // weather_text
                             1, // temperature_only
                             1, // wind_barb

                             1, // aloha_circle
                             1, // ambiguity
                             1, // phg
                             1, // default_phg
                             1, // phg_of_moving

                             1, // df_data
                             1, // dr_data
                             1, // dr_arc
                             1, // dr_course
                             1, // dr_symbol

                             1, // dist_bearing
                             1, // last_heard
};

Widget select_none_button;
Widget select_mine_button;
Widget select_tnc_button;
Widget select_direct_button;
Widget select_via_digi_button;
Widget select_net_button;
Widget select_tactical_button;
Widget select_old_data_button;

Widget select_stations_button;
Widget select_fixed_stations_button;
Widget select_moving_stations_button;
Widget select_weather_stations_button;
Widget select_CWOP_wx_stations_button;
Widget select_objects_button;
Widget select_weather_objects_button;
Widget select_gauge_objects_button;
Widget select_other_objects_button;


Widget display_callsign_button;
Widget display_label_all_trackpoints_button;
Widget display_symbol_button;
Widget display_symbol_rotate_button;
Widget display_trail_button;

Widget display_course_button;
Widget display_speed_button;
Widget display_speed_short_button;
Widget display_altitude_button;

Widget display_weather_button;
Widget display_weather_text_button;
Widget display_temperature_only_button;
Widget display_wind_barb_button;

Widget display_aloha_circle_button;
Widget display_ambiguity_button;
Widget display_phg_button;
Widget display_default_phg_button;
Widget display_phg_of_moving_button;

Widget display_df_data_button;
Widget display_dr_data_button;
Widget display_dr_arc_button;
Widget display_dr_course_button;
Widget display_dr_symbol_button;

Widget display_dist_bearing_button;
Widget display_last_heard_button;


static void Select_none_toggle(Widget w, XtPointer clientData, XtPointer calldata);
static void Select_mine_toggle(Widget w, XtPointer clientData, XtPointer calldata);
static void Select_tnc_toggle(Widget w, XtPointer clientData, XtPointer calldata);
static void Select_direct_toggle(Widget w, XtPointer clientData, XtPointer calldata);
static void Select_via_digi_toggle(Widget w, XtPointer clientData, XtPointer calldata);
static void Select_net_toggle(Widget w, XtPointer clientData, XtPointer calldata);
static void Select_tactical_toggle(Widget w, XtPointer clientData, XtPointer calldata);
static void Select_old_data_toggle(Widget w, XtPointer clientData, XtPointer calldata);

static void Select_stations_toggle(Widget w, XtPointer clientData, XtPointer calldata);
static void Select_fixed_stations_toggle(Widget w, XtPointer clientData, XtPointer calldata);
static void Select_moving_stations_toggle(Widget w, XtPointer clientData, XtPointer calldata);
static void Select_weather_stations_toggle(Widget w, XtPointer clientData, XtPointer calldata);
static void Select_CWOP_wx_stations_toggle(Widget w, XtPointer clientData, XtPointer calldata);
static void Select_objects_toggle(Widget w, XtPointer clientData, XtPointer calldata);
static void Select_weather_objects_toggle(Widget w, XtPointer clientData, XtPointer calldata);
static void Select_other_objects_toggle(Widget w, XtPointer clientData, XtPointer calldata);
static void Select_gauge_objects_toggle(Widget w, XtPointer clientData, XtPointer calldata);


static void Display_callsign_toggle(Widget w, XtPointer clientData, XtPointer calldata);
static void Display_label_all_trackpoints_toggle(Widget w, XtPointer clientData, XtPointer calldata);
static void Display_symbol_toggle(Widget w, XtPointer clientData, XtPointer calldata);
static void Display_symbol_rotate_toggle(Widget w, XtPointer clientData, XtPointer calldata);
static void Display_trail_toggle(Widget w, XtPointer clientData, XtPointer calldata);

static void Display_course_toggle(Widget w, XtPointer clientData, XtPointer calldata);
static void Display_speed_toggle(Widget w, XtPointer clientData, XtPointer calldata);
static void Display_speed_short_toggle(Widget w, XtPointer clientData, XtPointer calldata);
static void Display_altitude_toggle(Widget w, XtPointer clientData, XtPointer calldata);

static void Display_weather_toggle(Widget w, XtPointer clientData, XtPointer calldata);
static void Display_weather_text_toggle(Widget w, XtPointer clientData, XtPointer calldata);
static void Display_temperature_only_toggle(Widget w, XtPointer clientData, XtPointer calldata);
static void Display_wind_barb_toggle(Widget w, XtPointer clientData, XtPointer calldata);

static void Display_aloha_circle_toggle(Widget w, XtPointer clientData, XtPointer calldata);
static void Display_ambiguity_toggle(Widget w, XtPointer clientData, XtPointer calldata);
static void Display_phg_toggle(Widget w, XtPointer clientData, XtPointer calldata);
static void Display_default_phg_toggle(Widget w, XtPointer clientData, XtPointer calldata);
static void Display_phg_of_moving_toggle(Widget w, XtPointer clientData, XtPointer calldata);

static void Display_df_data_toggle(Widget w, XtPointer clientData, XtPointer calldata);
static void Display_dr_data_toggle(Widget w, XtPointer clientData, XtPointer calldata);
static void Display_dr_arc_toggle(Widget w, XtPointer clientData, XtPointer calldata);
static void Display_dr_course_toggle(Widget w, XtPointer clientData, XtPointer calldata);
static void Display_dr_symbol_toggle(Widget w, XtPointer clientData, XtPointer calldata);

static void Display_dist_bearing_toggle(Widget w, XtPointer clientData, XtPointer calldata);
static void Display_last_heard_toggle(Widget w, XtPointer clientData, XtPointer calldata);


// ------------------------ Interfaces --------------------------
static void  Transmit_disable_toggle( Widget widget, XtPointer clientData, XtPointer callData);
static void  Posit_tx_disable_toggle( Widget widget, XtPointer clientData, XtPointer callData);
static void  Object_tx_disable_toggle( Widget widget, XtPointer clientData, XtPointer callData);
static void  Server_port_toggle( Widget widget, XtPointer clientData, XtPointer callData);
int transmit_disable;
int posit_tx_disable;
int object_tx_disable;
int enable_server_port = 0;
Widget iface_transmit_now, posit_tx_disable_toggle, object_tx_disable_toggle;
Widget server_port_toggle;

#ifdef HAVE_GPSMAN
Widget Fetch_gps_track, Fetch_gps_route, Fetch_gps_waypoints;
Widget Fetch_RINO_waypoints;
Widget Send_gps_track, Send_gps_route, Send_gps_waypoints;
int gps_got_data_from = 0;          // We got data from a GPS
int gps_operation_pending = 0;      // A GPS transfer is happening
int gps_details_selected = 0;       // Whether name/color have been selected yet
Widget gpsfilename_text;            // Short name of gps map (no color/type)
char gps_map_filename[MAX_FILENAME];// Chosen name of gps map (including color)
char gps_map_filename_base[MAX_FILENAME];   // Same minus ".shp"
char gps_map_filename_base2[MAX_FILENAME];   // Same minus ".shp" and color
char gps_temp_map_filename[MAX_FILENAME];
char gps_temp_map_filename_base[MAX_FILENAME];  // Same minus ".shp"
char gps_dbfawk_format[]="BEGIN_RECORD {key=\"\"; lanes=3; color=%d; name=\"%s\"; filled=0; pattern=1; display_level=65536; label_level=128; label_color=8; symbol=\"\"}\n";
int gps_map_color = 0;              // Chosen color of gps map
int gps_map_color_offset;           // offset into colors array of that color.
char gps_map_type[30];              // Type of GPS download
void check_for_new_gps_map(int curr_sec);
Widget GPS_operations_dialog = (Widget)NULL;
#endif  // HAVE_GPSMAN

// ------------------------ unit conversion --------------------------
static void Units_choice_toggle(Widget w, XtPointer clientData, XtPointer calldata);

// 0: metric, 1: english, (2: nautical, not fully implemented)
int english_units;

char un_alt[2+1];   // m / ft
char un_dst[2+1];   // mi / km      (..nm)
char un_spd[4+1];   // mph / km/h   (..kn)
double cvt_m2len;   // from meter
double cvt_kn2len;  // from knots
double cvt_mi2len;  // from miles
double cvt_dm2len;  // from decimeter
double cvt_hm2len;  // from hectometer

void update_units(void);

// dist/bearing on status line
static void Dbstatus_choice_toggle(Widget w, XtPointer clientData, XtPointer calldata);

int do_dbstatus;


// Coordinate System
int coordinate_system = USE_DDMMMM; // Default, used for most APRS systems


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
//#endif    // HAVE_FESTIVAL
int ATV_screen_ID;

#ifdef HAVE_MAGICK //N0VH
Widget configure_tiger_dialog = (Widget) NULL;
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
       tiger_misc;

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
#endif  // HAVE_MAGICK


#ifdef HAVE_LIBGEOTIFF
Widget configure_DRG_dialog = (Widget) NULL;
Widget DRG_XOR,
       DRG_color0,
       DRG_color1,
       DRG_color2,
       DRG_color3,
       DRG_color4,
       DRG_color5,
       DRG_color6,
       DRG_color7,
       DRG_color8,
       DRG_color9,
       DRG_color10,
       DRG_color11,
       DRG_color12;

int DRG_XOR_colors = 0;
int DRG_show_colors[13];
#endif  // HAVE_LIBGEOTIFF


// -------------------------------------------------------------------


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
#endif  // TRANSMIT_RAW_WX
Widget compressed_posit_tx;
Widget compressed_objects_items_tx;
Widget new_bulletin_popup_enable;
Widget zero_bulletin_popup_enable;
Widget warn_about_mouse_modifiers_enable;
Widget my_trail_diff_color_enable;
Widget load_predefined_objects_menu_from_file_enable;
#ifdef USE_COMBO_BOX
Widget load_predefined_objects_menu_from_file; // combo box widget
#else
int lpomff_value;  // replacement value for predefined menu file combo box
#endif // USE_COMBO_BOX
int pop_up_new_bulletins = 0;
int view_zero_distance_bulletins = 0;
int warn_about_mouse_modifiers = 1;
Widget altnet_active;
Widget altnet_text;
Widget disable_dupe_check;
Widget new_map_layer_text = (Widget)NULL;
Widget new_max_zoom_text = (Widget)NULL;
Widget new_min_zoom_text = (Widget)NULL;
Widget debug_level_text;
static int sec_last_dr_update = 0;


FILE *f_xfontsel_pipe[FONT_MAX];
int xfontsel_query = 0;


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

static void SetMyPosition( Widget w, XtPointer clientData, XtPointer calldata);

static void Pan_ctr(Widget w, XtPointer clientData, XtPointer calldata);
static void Pan_up(Widget w, XtPointer clientData, XtPointer calldata);
static void Pan_up_less(Widget w, XtPointer clientData, XtPointer calldata);
static void Pan_down(Widget w, XtPointer clientData, XtPointer calldata);
static void Pan_down_less(Widget w, XtPointer clientData, XtPointer calldata);
static void Pan_left(Widget w, XtPointer clientData, XtPointer calldata);
static void Pan_left_less(Widget w, XtPointer clientData, XtPointer calldata);
static void Pan_right(Widget w, XtPointer clientData, XtPointer calldata);
static void Pan_right_less(Widget w, XtPointer clientData, XtPointer calldata);
void Center_Zoom(Widget w, XtPointer clientData, XtPointer calldata);
void Go_Home(Widget w, XtPointer clientData, XtPointer calldata);
int center_zoom_override = 0;
Widget center_zoom_dialog = (Widget)NULL;
Widget custom_zoom_dialog = (Widget)NULL;

static void Menu_Quit(Widget w, XtPointer clientData, XtPointer calldata);

static void TNC_Logging_toggle(Widget w, XtPointer clientData, XtPointer calldata);
static void TNC_Transmit_now(Widget w, XtPointer clientData, XtPointer calldata);

#ifdef HAVE_GPSMAN
static void GPS_operations(Widget w, XtPointer clientData, XtPointer calldata);
#endif  // HAVE_GPSMAN

static void Net_Logging_toggle(Widget w, XtPointer clientData, XtPointer calldata);

static void IGate_Logging_toggle(Widget w, XtPointer clientData, XtPointer calldata);

static void Message_Logging_toggle(Widget w, XtPointer clientData, XtPointer calldata);

static void WX_Logging_toggle(Widget w, XtPointer clientData, XtPointer calldata);

static void WX_Alert_Logging_toggle(Widget w, XtPointer clientData, XtPointer calldata);

void on_off_switch(int switchpos, Widget first, Widget second);
void sel3_switch(int switchpos, Widget first, Widget second, Widget third);
void sel4_switch(int switchpos, Widget first, Widget second, Widget third, Widget fourth);

static void Configure_station(Widget w, XtPointer clientData, XtPointer callData);

static void Configure_defaults(Widget w, XtPointer clientData, XtPointer callData);

static void Configure_timing(Widget w, XtPointer clientData, XtPointer callData);

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

// Global variable, so we can set it up once check it from then on,
// preventing memory leaks from repeatedly setting up the same
// XFontStruct.
XFontStruct *station_font = NULL;   // Station font
XFontStruct *font1;                 // Menu/System font
XmFontList fontlist1;               // Menu/System fontlist

Pixmap  pixmap_50pct_stipple; // 50% pixels used for position ambiguity, DF circle, etc.
Pixmap  pixmap_25pct_stipple; // 25% pixels used for large position ambiguity
Pixmap  pixmap_13pct_stipple; // 12.5% pixels used for larger position ambiguity
Pixmap  pixmap_wx_stipple;  // Used for weather alerts

int interrupt_drawing_now = 0;  // Flag used to interrupt map drawing
int request_resize = 0;         // Flag used to request a resize operation
int request_new_image = 0;      // Flag used to request a create_image operation
//time_t last_input_event = (time_t)0;  // Time of last mouse/keyboard event
extern void new_image(Widget da);


typedef struct XastirGlobal {
    Widget  top;    // top level shell
} XastirGlobal;
XastirGlobal Global;


char *database_ptr;             /* database pointers */


//---------------------------------------------------------------------------------------------
//
// These describe the current map window.  They must be kept
// up-to-date when we zoom/pan/resize the window.
//
float f_center_longitude;    // Floating point map center longitude, updated by new_image()
float f_center_latitude;     // Floating point map center latitude , updated by new_image()
float f_NW_corner_longitude; // longitude of NW corner, updated by create_image(), refresh_image()
float f_NW_corner_latitude;  // latitude  of NW corner, updated by create_image(), refresh_image()
float f_SE_corner_longitude; // longitude of SE corner, updated by create_image(), refresh_image()
float f_SE_corner_latitude;  // latitude  of SE corner, updated by create_image(), refresh_image()

long center_longitude;       // Longitude at center of map, updated by display_zoom_image()
long center_latitude;        // Latitude  at center of map, updated by display_zoom_image()
long NW_corner_longitude;    // Longitude at NW corner, updated by create_image(), refresh_image()
long NW_corner_latitude;     // Latitude  at NW corner, updated by create_image(), refresh_image()
long SE_corner_longitude;    // Longitude at SE corner, updated by create_image(), refresh_image()
long SE_corner_latitude;     // Latitude  at SE corner, updated by create_image(), refresh_image()

long scale_x;                // x scaling in 1/100 sec per pixel, calculated from scale_y
long scale_y;                // y scaling in 1/100 sec per pixel

long new_mid_x, new_mid_y;   // Check values used before applying real change
long new_scale_x;
long new_scale_y;
long screen_width;           // Screen width,  map area without border (in pixels)
long screen_height;          // Screen height, map area without border (in pixels)
Position screen_x_offset;
Position screen_y_offset;
float d_screen_distance;     // Diag screen distance
float x_screen_distance;     // x screen distance
//---------------------------------------------------------------------------------------------


char user_dir[1000];            /* user directory file */
int delay_time;                 /* used to delay display data */
time_t last_weather_cycle;      // Time of last call to cycle_weather()
Pixel colors[256];              /* screen colors */
Pixel trail_colors[MAX_TRAIL_COLORS]; /* station trail colors, duh */
int current_trail_color;        /* what color to draw station trails with */
Pixel_Format visual_type = NOT_TRUE_NOR_DIRECT;
int install_colormap;           /* if visual_type == NOT_TRUE..., should we install priv cmap */
Colormap cmap;                  /* current colormap */

int redo_list;                  // Station List update request
int redraw_on_new_data;         // Station redraw request
int wait_to_redraw;             /* wait to redraw until system is up */
int display_up = 0;             /* display up? */
int display_up_first = 0;       /* display up first */

time_t max_transmit_time;       /* max time between transmits */
time_t last_alert_redraw;       /* last time alert caused a redraw */
time_t sec_next_gps;            /* next gps check */
time_t gps_time;                /* gps delay time */
char gprmc_save_string[MAX_LINE_SIZE+1];
char gpgga_save_string[MAX_LINE_SIZE+1];
int gps_port_save;
time_t POSIT_rate;              // Posit TX rate timer
time_t OBJECT_rate;             // Object/Item TX rate timer
time_t update_DR_rate;          // How often to call draw_symbols if DR enabled
time_t remove_ID_message_time;  // Time to get rid of large msg on screen.
int pending_ID_message = 0;     // Variable turning on/off this function


// SmartBeaconing(tm) stuff.  If enabled, POSIT_rate won't be used
// for timing posits. sb_POSIT_rate computed via SmartBeaconing(tm)
// will be used instead.
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


Widget ghosting_time = (Widget)NULL;
Widget clearing_time = (Widget)NULL;
Widget removal_time = (Widget)NULL;
Widget posit_interval = (Widget)NULL;
Widget gps_interval = (Widget)NULL;
Widget dead_reckoning_time = (Widget)NULL;
Widget object_item_interval = (Widget)NULL;
Widget serial_pacing_time = (Widget)NULL;
Widget trail_segment_timeout = (Widget)NULL;
Widget trail_segment_distance_max = (Widget)NULL;
Widget RINO_download_timeout = (Widget)NULL;
Widget net_map_slider = (Widget)NULL;
Widget snapshot_interval_slider = (Widget)NULL;
int net_map_timeout = 120;



time_t GPS_time;                /* gps time out */
time_t last_statusline;         // last update of statusline or 0 if inactive
time_t last_id_time;            // Time of last ID message to statusline
time_t sec_old;                 /* station old after */
time_t sec_clear;               /* station cleared after */
time_t sec_remove;              /* Station removed after */
int trail_segment_time;         // Segment missing if above this time (mins)
int trail_segment_distance;     // Segment missing if greater distance
int RINO_download_interval;     // Interval at which to download RINO waypoints,
                                // creating APRS Objects from them.
time_t last_RINO_download = (time_t)0;
time_t sec_next_raw_wx;         /* raw wx transmit data */
int dead_reckoning_timeout = 60 * 10;   // 10 minutes;

#ifdef TRANSMIT_RAW_WX
int transmit_raw_wx;            /* transmit raw wx data? */
#endif  // TRANSMIT_RAW_WX

int transmit_compressed_posit;  // transmit location in compressed format?
int transmit_compressed_objects_items;  // Same for objects & items

int output_station_type;        /* Broadcast station type */

int Configure_station_pos_amb;  /* Broadcast station position ambiguity */

long max_vectors_allowed;       /* max map vectors allowed */
long max_text_labels_allowed;   /* max map text labels allowed */
long max_symbol_labels_allowed; /* max map symbol labels allowed */

time_t net_last_time;           /* reconnect last time in seconds */
time_t net_next_time;           /* reconnect Next update delay time */

#ifdef USING_LIBGC
time_t gc_next_time = 0L;       // Garbage collection next time
#endif  // USING_LIBGC

time_t posit_last_time;
time_t posit_next_time;         /* time at which next posit TX will occur */

time_t last_time;               /* last time in seconds */
time_t next_time;               /* Next update delay time */

time_t next_redraw;             /* Next update time */
time_t last_redraw;             /* Time of last redraw */

char aprs_station_message_type = '='; // station message-capable or not

int transmit_now;               /* set to transmit now (push on moment) */
int my_position_valid = 1;      /* Don't send posits if this is zero */
int using_gps_position = 0;     /* Set to one if a GPS port is active */
int operate_as_an_igate;        /* toggle igate operations for net connections */
unsigned igate_msgs_tx;         /* current total of igate messages transmitted */

int log_tnc_data;               /* log data */
int log_net_data;               /* log data */
int log_igate;                  /* toggle to allow igate logging */
int log_wx;                     /* toggle to allow wx logging */
int log_message_data;           /* toggle to allow message logging */
int log_wx_alert_data;          /* toggle to allow wx alert logging */


int snapshots_enabled = 0;      // toggle to allow creating .png snapshots on a regular basis
int kmlsnapshots_enabled = 0;   // toggle to allow creating .kml snapshots on a regular basis 

time_t WX_ALERTS_REFRESH_TIME;  /* Minimum WX alert map refresh time in seconds */

/* button zoom */
int menu_x;
int menu_y;
int possible_zoom_function = 0;
int zoom_box_x1 = -1;           // Stores one corner of zoom box
int zoom_box_y1 = -1;
int zoom_box_x2 = -1;           // Stores one corner of zoom box
int zoom_box_y2 = -1;
int mouse_zoom = 0;

// log file replay
int read_file;
FILE *read_file_ptr;
time_t next_file_read;

// Data for own station
char my_callsign[MAX_CALLSIGN+1];
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
char LOGFILE_MESSAGE[400];
char LOGFILE_WX[400];
char LOGFILE_WX_ALERT[400];

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


int input_x = 0;
int input_y = 0;

XtAppContext app_context;
Display *display;       /*  Display             */

/* dialog popup last */
int last_popup_x;
int last_popup_y;

int disable_all_popups = 0;
char temp_tracking_station_call[30] = "";

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
        switch (english_units) {
            case 0: // Metric:  Convert from KPH to MPH for storage
                i = (int)((i * 0.62137) + 0.5);
                break;
            case 1: // English
            case 2: // Nautical
            default:    // No conversion necessary
                break;
        }
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
        switch (english_units) {
            case 0: // Metric:  Convert from KPH to MPH for storage
                i = (int)((i * 0.62137) + 0.5);
                break;
            case 1: // English
            case 2: // Nautical
            default:    // No conversion necessary
                break;
        }
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
    char temp_label_string[100];


    // Destroy the dialog if it exists.  This is to make sure the
    // title is correct based on the last dialog that called us.
    if (smart_beacon_dialog) {
        Smart_Beacon_destroy_shell( w, smart_beacon_dialog, callData);
    }

    if (!smart_beacon_dialog) {

        smart_beacon_dialog = XtVaCreatePopupShell(langcode("SMARTB001"),
                xmDialogShellWidgetClass, appshell,
                XmNdeleteResponse, XmDESTROY,
                XmNdefaultPosition, FALSE,
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 1,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
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
                XmNtopAttachment,XmATTACH_WIDGET,
                XmNtopWidget, smart_beacon_enable,
                XmNtopOffset, 5,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 1,
                XmNrightAttachment,XmATTACH_FORM,
                XmNrightOffset, 10,
                XmNnavigationType, XmTAB_GROUP,
                XmNfontList, fontlist1,
                NULL);

        switch (english_units) {
            case 0: // Metric
                xastir_snprintf(temp_label_string,
                    sizeof(temp_label_string),
                    langcode("SMARTB004") );
                break;
            case 1: // English
            case 2: // Nautical
            default:
                xastir_snprintf(temp_label_string,
                    sizeof(temp_label_string),
                    langcode("SMARTB003") );
                break;
        }

        // High Speed (mph) / (kph)
        label2 = XtVaCreateManagedWidget(temp_label_string,
                xmLabelWidgetClass,
                form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, label1,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 1,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
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
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 1,
                XmNrightAttachment,XmATTACH_FORM,
                XmNrightOffset, 10,
                XmNnavigationType, XmTAB_GROUP,
                XmNfontList, fontlist1,
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
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 1,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
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
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 1,
                XmNrightAttachment,XmATTACH_FORM,
                XmNrightOffset, 10,
                XmNnavigationType, XmTAB_GROUP,
                XmNfontList, fontlist1,
                NULL);

        switch (english_units) {
            case 0: // Metric
                xastir_snprintf(temp_label_string,
                    sizeof(temp_label_string),
                    langcode("SMARTB007") );
                break;
            case 1: // English
            case 2: // Nautical
            default:
                xastir_snprintf(temp_label_string,
                    sizeof(temp_label_string),
                    langcode("SMARTB006") );
                break;
        }

        // Low Speed (mph) / (kph)
        label4 = XtVaCreateManagedWidget(temp_label_string,
                xmLabelWidgetClass,
                form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, label3,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 1,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
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
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 1,
                XmNrightAttachment,XmATTACH_FORM,
                XmNrightOffset, 10,
                XmNnavigationType, XmTAB_GROUP,
                XmNfontList, fontlist1,
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
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 1,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
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
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 1,
                XmNrightAttachment,XmATTACH_FORM,
                XmNrightOffset, 10,
                XmNnavigationType, XmTAB_GROUP,
                XmNfontList, fontlist1,
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
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 1,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
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
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 1,
                XmNrightAttachment,XmATTACH_FORM,
                XmNrightOffset, 10,
                XmNnavigationType, XmTAB_GROUP,
                XmNfontList, fontlist1,
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
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 1,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
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
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 1,
                XmNrightAttachment,XmATTACH_FORM,
                XmNrightOffset, 10,
                XmNnavigationType, XmTAB_GROUP,
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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

        switch (english_units) {
            case 0: // Metric:  Convert from MPH to KPH for display
                xastir_snprintf(temp_string,
                    sizeof(temp_string),
                    "%d",
                    (int)((sb_high_speed_limit * 1.6094) + 0.5) );
                break;
            case 1: // English
            case 2: // Nautical
            default:    // No conversion necessary
                xastir_snprintf(temp_string,
                    sizeof(temp_string),
                    "%d",
                    sb_high_speed_limit);
                break;
        }
        XmTextSetString(sb_hi_mph_data, temp_string);

        xastir_snprintf(temp_string, sizeof(temp_string), "%d", sb_posit_slow);
        XmTextSetString(sb_lo_rate_data, temp_string);

        switch (english_units) {
            case 0: // Metric:  Convert from MPH to KPH for display
                xastir_snprintf(temp_string,
                    sizeof(temp_string),
                    "%d",
                    (int)((sb_low_speed_limit * 1.6094) + 0.5) );
                break;
            case 1: // English
            case 2: // Nautical
            default:    // No conversion necessary
                xastir_snprintf(temp_string,
                    sizeof(temp_string),
                    "%d",
                    sb_low_speed_limit);
                break;
        }
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





// Causes the current set of internet-based maps to be snagged from
// the 'net instead of from cache.  Once downloaded they get written
// to the cache, overwriting possibly corrupted maps already in the
// cache (why else would you invoke this function?).  This is a
// method of getting rid of corrupted maps without having to wipe
// out the entire cache.
//
void Re_Download_Maps_Now(Widget w, XtPointer clientData, XtPointer callData) {

#ifdef USE_MAP_CACHE
    // Disable reads from the map cache
    map_cache_fetch_disable = 1;
#endif

    // Show a busy cursor while the map is being downloaded
    busy_cursor(appshell);

    // Cause maps to be refreshed
    new_image(da);

#ifdef USE_MAP_CACHE
    //Enable reads from the map cache
    map_cache_fetch_disable = 0;
#endif
}





// Removes all files in the ~/.xastir/map_cache directory.  Does not
// recurse down into subdirectories, but it shouldn't have to.
//
void Flush_Entire_Map_Queue(Widget w, XtPointer clientData, XtPointer callData) {
    struct dirent *dl = NULL;
    DIR *dm;
    char fullpath[MAX_FILENAME];
    char dir[MAX_FILENAME];
    struct stat nfile;


    xastir_snprintf(dir,
        sizeof(dir),
        "%s",
        get_user_base_dir("map_cache"));

    dm = opendir(dir);
    if (!dm) {  // Couldn't open directory
        fprintf(stderr,"Flush_Entire_Map_Queue: Couldn't open directory\n");
        return;
    }

    // Read the directory contents, delete each file found, skip
    // directories.
    //
    while ((dl = readdir(dm))) {

        //Construct the entire path/filename
        xastir_snprintf(fullpath,
            sizeof(fullpath),
            "%s/%s",
            dir,
            dl->d_name);

        if (stat(fullpath, &nfile) == 0) {
            if ((nfile.st_mode & S_IFMT) == S_IFREG) {
                // It's a regular file

                // Remove the file
                if (debug_level & 512)                
                    fprintf(stderr,"Deleting file:  %s\n", fullpath);

                unlink(fullpath);
            }
        }
    }
    (void)closedir(dm);
}





// Find the extents of every map we have.  This is the callback for
// the "Re-Index Maps" button.
//
// If passed a NULL in the callback, we do a smart reindexing:  Only
// reindex the files that are new or have changed.
// If passed a "1" in the callback, we do a full reindexing:  Delete
// the in-memory index and start indexing from scratch.
// 
void Index_Maps_Now(Widget w, XtPointer clientData, XtPointer callData) {
    int parameter = 0;  // Default:  Smart timestamp-checking indexing


    if (clientData != NULL) {

        parameter = atoi((char *)clientData);

        if (parameter != 1) {   // Our only option
            parameter = 0;
        }
    }

    // Update the list and write it to file.
    map_indexer(parameter);
}





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
            popup_message_always( langcode("POPEM00030"), langcode("POPEM00031") );
        }
    }
}





void check_nws_weather_symbol(void) {
    if ( (my_symbol == 'W')
            && ( (my_group == '\\') || (my_group == '/') ) ) {

        // Notify the operator that they're trying to be an NWS
        // weather station.
        popup_message_always( langcode("POPEM00030"), langcode("POPEM00032") );
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





// Computes all five coordinate representations for displaying in
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
// Outputs: global variables and "result" textField, full_zone.
// full_zone should be a string of at least size 4.
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
    char MGRS_str[50];
    double double_easting, double_northing;
 

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


    double_easting = (double)easting;
    double_northing = (double)northing;
    convert_UTM_to_xastir(double_easting,
        double_northing,
        full_zone,
        &xastir_lon,
        &xastir_lat);

//fprintf(stderr,"%s  %f  %f\t\t%lu %lu\n",
//full_zone,
//double_easting,
//double_northing,
//xastir_lat,
//xastir_lon);

 
    // Compute MGRS coordinates.
    convert_xastir_to_MGRS_str(MGRS_str,
        sizeof(MGRS_str),
        xastir_lon,
        xastir_lat,
        1); // Format with leading spaces plus spaces between
            // easting and northing, so that it lines up with UTM
            // strings.


    // Compute Maidenhead Grid Locator.  Note that the sec_to_loc()
    // function expects lat/lon in Xastir coordinate system.
    xastir_snprintf(maidenhead_grid,
        sizeof(maidenhead_grid),
        "%s",
        sec_to_loc( xastir_lon, xastir_lat ) );


    if (strlen(full_zone) == 1) {
        xastir_snprintf(temp_string,
            sizeof(temp_string),
            "  %s",
            full_zone);
        xastir_snprintf(full_zone,
            4,
            "%s",
            temp_string);
    }
    else if (strlen(full_zone) == 2) {
        xastir_snprintf(temp_string,
            sizeof(temp_string),
            " %s",
            full_zone);
        xastir_snprintf(full_zone,
            4,
            "%s",
            temp_string);
    }
        

    // Put the four different representations of the coordinate into
    // the "result" textField.
    xastir_snprintf(temp_string,
        sizeof(temp_string),
        "%s%8.5f%c   %9.5f%c\n%s%02d %06.3f%c  %03d %06.3f%c\n%s%02d %02d %04.1f%c %03d %02d %04.1f%c\n%s%3s  %07lu  %07lu\n%s%s\n%s%s",
        langcode("COORD011"), // "Decimal Degrees:",
        lat_deg_int+lat_min/60.0, (south) ? 'S':'N',
        lon_deg_int+lon_min/60.0, (west) ?  'W':'E',
        langcode("COORD012"), // "Degrees/Decimal Minutes:",
        lat_deg_int, lat_min, (south) ? 'S':'N',
        lon_deg_int, lon_min, (west) ?  'W':'E',
        langcode("COORD013"), // "Degrees/Minutes/Dec. Seconds:",
        lat_deg_int, lat_min_int, lat_sec, (south) ? 'S':'N',
        lon_deg_int, lon_min_int, lon_sec, (west) ?  'W':'E',
        langcode("COORD014"), // "Universal Transverse Mercator:",
        full_zone, easting, northing,
        langcode("COORD015"), // "Military Grid Reference System:",
        MGRS_str,
        langcode("COORD016"), // "Maidenhead Grid Locator:",
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


    // Check for something in the zone field that looks like a valid
    // UTM zone.
    str_ptr = XmTextGetString(coordinate_calc_zone);
    i = strlen(str_ptr);
    have_utm = 1;   // Wishful thinking.  We'll zero it later if not.
    if ( (i >= 1) && (i <= 3) ) {
        // String is the correct length.  Can have just A/B/Y/Z, or
        // else one or two digits plus one letter.
        int j;

        for (j = 0; j < (i-1); j++) {
            if ( (str_ptr[j] < '0') && (str_ptr[j] > '9') ) {
                // Not UTM, need either one or two digits first if
                // we have 2 or 3 chars.
                have_utm = 0;
            }
        }
        if ( ( (str_ptr[i-1] < 'A') || (str_ptr[i-1] > 'Z') )
            && ( (str_ptr[i-1] < 'a') || (str_ptr[i-1] > 'z') ) ) {
            // Not UTM, zone character isn't correct
            have_utm = 0;
        }
    }
    else {  // Not a valid UTM zone, wrong length.
        have_utm = 0;
    }

    // If we've made it to this point and have_utm == 1, then zone looks
    // like a UTM zone.
    if (have_utm) {
        zone_letter = toupper(str_ptr[i-1]);
        zone_number = atoi(str_ptr);
        //fprintf(stderr,"Zone Number: %d,  Zone Letter: %c\n", zone_number, zone_letter);
        // Save it away for later use
        if (zone_number == 0) { // We're in a UPS area
            xastir_snprintf(full_zone,
                sizeof(full_zone),
                "  %c",
                zone_letter);
        }
        else {  // UTM area
            xastir_snprintf(full_zone,
                sizeof(full_zone),
                "%02d%c",
                zone_number,
                zone_letter);
        }
        have_lat_lon = 0;
    }
    else {
        //fprintf(stderr,"Bad zone, not a UTM coordinate\n");
        // Skip zone widget for lat/lon, it's not used.
        have_lat_lon = 1;   // Wishful thinking.  We'll zero it later if not.
    }
    // We're done with that variable.  Free the space.
    XtFree(str_ptr);


    str_ptr = XmTextGetString(coordinate_calc_latitude_easting);
    i = strlen(str_ptr);
    // Check for exactly six or seven chars.  If seven, first one must
    // be a zero (Not true!  UPS coordinates have digits there!).
    if ( have_utm && (i != 6) && (i != 7) ) {
        have_utm = 0;
        //fprintf(stderr,"Bad Easting value: Not 6 or 7 chars\n");
    }
//    if ( have_utm && (i == 7) && (str_ptr[0] != '0') ) {
//        have_utm = 0;
//        //fprintf(stderr,"Bad Easting value: 7 chars but first one not 0\n");
//    }
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
            //fprintf(stderr,"Easting: %lu\n",easting);
        }
        else {
            //fprintf(stderr,"Bad Easting value\n");
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
            //fprintf(stderr,"Found an 'N', converted to %s\n", temp_string);
        }
        ptr = rindex(temp_string, 'S');
        if (ptr != NULL) {  // Found an 'S'
            *ptr = ' ';     // Convert it to a space
            south++;
            //fprintf(stderr,"Found an 'S', converted to %s\n", temp_string);
        }
        ptr = rindex(temp_string, '-');
        if (ptr != NULL) {  // Found an '-'
            *ptr = ' ';     // Convert it to a space
            south++;
            //fprintf(stderr,"Found an '-', converted to %s\n", temp_string);
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
                    //fprintf(stderr,"%s",&temp_string[j+1]);
                }
            }
        }

        // temp[] array now contains indexes into all of the
        // substrings.  Some may contain empty strings.

        //fprintf(stderr,"Substrings: %d\n", substring);
        //fprintf(stderr,"temp_string: %s\n",temp_string);


        //for (j = 0; j < substring; j++) {
        //    if (strlen(&temp_string[temp[j]]) > 0) {
        //        fprintf(stderr,"%s\n", &temp_string[temp[j]]);
        //    }
        //}

        piece = 0;
        have_lat_lon = 0;

        for (j = 0; j < substring; j++) {
            if (strlen(&temp_string[temp[j]]) > 0) {
                double kk;

                piece++;    // Found the next piece
                kk = atof(&temp_string[temp[j]]);

                switch (piece) {
                    case (1) :  // Degrees
                        latitude = kk;
                        have_lat_lon = 1;
                        break;
                    case (2) :  // Minutes
                        if ( (kk < 0.0) || (kk >= 60.0) ) {
                            fprintf(stderr,"Warning:  Bad latitude minutes value\n");
                            // Set variables so that we'll get error output.
                            have_lat_lon = 0;
                            have_utm = 0;
                        }
                        else {
                            latitude = latitude + ( kk / 60.0 );
                        }
                        break;
                    case (3) :  // Seconds
                        if ( (kk < 0.0) || (kk >= 60.0)) {
                            fprintf(stderr,"Warning:  Bad latitude seconds value\n");
                            // Set variables so that we'll get error output.
                            have_lat_lon = 0;
                            have_utm = 0;
                        }
                        else {
                            latitude = latitude + ( kk / 3600.0 );
                        }
                        break;
                    default :
                        break;
                }
            }
        }

        if (south) {
            latitude = -latitude;
        }
        //fprintf(stderr,"%f\n", latitude);

        // Test for valid values of latitude
        if ( have_lat_lon && ((latitude < -90.0) || (latitude > 90.0)) ) {
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
        //fprintf(stderr,"Bad Northing value: Not 7 chars\n");
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
            //fprintf(stderr,"Northing: %lu\n",northing);
        }
        else {
            //fprintf(stderr,"Bad Northing value\n");
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
            //fprintf(stderr,"Found an 'W', converted to %s\n", temp_string);
        }
        ptr = rindex(temp_string, 'E');
        if (ptr != NULL) {  // Found an 'E'
            *ptr = ' ';     // Convert it to a space
            //fprintf(stderr,"Found an 'E', converted to %s\n", temp_string);
        }
        ptr = index(temp_string, '-');
        if (ptr != NULL) {  // Found an '-'
            *ptr = ' ';     // Convert it to a space
            west++;
            //fprintf(stderr,"Found an '-', converted to %s\n", temp_string);
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
                    //fprintf(stderr,"%s",&temp_string[j+1]);
                }
            }
        }

        // temp[] array now contains indexes into all of the
        // substrings.  Some may contain empty strings.

        //fprintf(stderr,"Substrings: %d\n", substring);
        //fprintf(stderr,"temp_string: %s\n",temp_string);


        //for (j = 0; j < substring; j++) {
        //    if (strlen(&temp_string[temp[j]]) > 0) {
        //        fprintf(stderr,"%s\n", &temp_string[temp[j]]);
        //    }
        //}
        piece = 0;
        have_lat_lon = 0;

        for (j = 0; j < substring; j++) {
            if (strlen(&temp_string[temp[j]]) > 0) {
                double kk;

                piece++;    // Found the next piece
                kk = atof(&temp_string[temp[j]]);

                switch (piece) {
                    case (1) :  // Degrees
                        longitude = kk;
                        have_lat_lon = 1;
                        break;
                    case (2) :  // Minutes
                        if ( (kk < 0.0) || (kk >= 60.0) ) {
                            fprintf(stderr,"Warning:  Bad longitude minutes value\n");
                            // Set variables so that we'll get error output.
                            have_lat_lon = 0;
                            have_utm = 0;
                        }
                        else {
                            longitude = longitude + ( kk / 60.0 );
                        }
                        break;
                    case (3) :  // Seconds
                        if ( (kk < 0.0) || (kk >= 60.0) ) {
                            fprintf(stderr,"Warning:  Bad longitude seconds value\n");
                            // Set variables so that we'll get error output.
                            have_lat_lon = 0;
                            have_utm = 0;
                        }
                        else {
                            longitude = longitude + ( kk / 3600.0 );
                        }
                        break;
                    default :
                        break;
                }
            }
        }

        if (west) {
            longitude = -longitude;
        }
        //fprintf(stderr,"%f\n", longitude);


        // Test for valid values of longitude
        if (have_lat_lon && ((longitude < -180.0) || (longitude > 180.0)) ) {
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
//fprintf(stderr,"Processing 'good' UTM values\n");
        // Process UTM values
        utm_ups_to_ll(E_WGS_84,
            (double)northing,
            (double)easting,
            full_zone,
            &latitude,
            &longitude);
        if (debug_level & 1)
            fprintf(stderr,"Latitude: %f, Longitude: %f\n",latitude,longitude);
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
        ll_to_utm_ups(E_WGS_84,
            (double)latitude,
            (double)longitude,
            &double_northing,
            &double_easting,
            full_zone,
            sizeof(full_zone));
        if (debug_level & 1)
            fprintf(stderr,"Zone: %s, Easting: %f, Northing: %f\n", full_zone, double_easting, double_northing);
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
//            " **       Sorry, your input was not recognized!        **",
            langcode("COORD017"),
//            " **   Please use one of the following input formats:   **",
            langcode("COORD018"),
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
    //fprintf(stderr,"%s\n",coordinate_calc_lat_deg);

    if ( (coordinate_calc_array.calling_dialog != NULL)
            && (coordinate_calc_array.input_lat_min != NULL) )
        XmTextSetString(coordinate_calc_array.input_lat_min, coordinate_calc_lat_min);
    //fprintf(stderr,"%s\n",coordinate_calc_lat_min);

    if ( (coordinate_calc_array.calling_dialog != NULL)
            && (coordinate_calc_array.input_lat_dir != NULL) )
        XmTextSetString(coordinate_calc_array.input_lat_dir, coordinate_calc_lat_dir);
    //fprintf(stderr,"%s\n",coordinate_calc_lat_dir);

    if ( (coordinate_calc_array.calling_dialog != NULL)
            && (coordinate_calc_array.input_lon_deg != NULL) )
        XmTextSetString(coordinate_calc_array.input_lon_deg, coordinate_calc_lon_deg);
    //fprintf(stderr,"%s\n",coordinate_calc_lon_deg);

    if ( (coordinate_calc_array.calling_dialog != NULL)
            && (coordinate_calc_array.input_lon_min != NULL) )
        XmTextSetString(coordinate_calc_array.input_lon_min, coordinate_calc_lon_min);
    //fprintf(stderr,"%s\n",coordinate_calc_lon_min);

    if ( (coordinate_calc_array.calling_dialog != NULL)
            && (coordinate_calc_array.input_lon_dir != NULL) )
        XmTextSetString(coordinate_calc_array.input_lon_dir, coordinate_calc_lon_dir);
    //fprintf(stderr,"%s\n",coordinate_calc_lon_dir);

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
    Arg args[50];                    // Arg List
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
                xmDialogShellWidgetClass, appshell,
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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
                NULL);

//        xastir_snprintf(temp_string, sizeof(temp_string), "%d", temp);
//        XmTextSetString(coordinate_calc_text, temp_string);

        coordinate_calc_result_text = NULL;
        coordinate_calc_result_text = XtVaCreateManagedWidget("Coordinate_calc results",
                xmTextWidgetClass,
                form,
                XmNrows, 6,
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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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
        //fprintf(stderr,"String: %s\n", temp_string);
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
        //fprintf(stderr,"String: %s\n", temp_string);
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

    // This X11 function gets invoked when X11 decides that it has
    // some free time.  We use that to advantage in making the busy
    // cursor go away "magically" when we're not so busy.
    //
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
// We check for interrupt_drawing_now flag being set, and exit
// nicely if so.  That flag means that some other drawing operation
// needs to happen.
//
// Returns 0 if it gets interrupted, 1 if it completes.
//
int create_image(Widget w) {
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
        fprintf(stderr,"Create image start\n");

    HandlePendingEvents(app_context);
    if (interrupt_drawing_now)
        return(0);

    // If we're in the middle of ID'ing, wait a bit.
    if (ATV_screen_ID && pending_ID_message)
        usleep(2000000);    // 2 seconds

    HandlePendingEvents(app_context);
    if (interrupt_drawing_now)
        return(0);

    /* First get the various dimensions */
    XtVaGetValues(w,
              XmNwidth,         &width,
              XmNheight,        &height,
              XmNmarginWidth,   &margin_width,
              XmNmarginHeight,  &margin_height,
              XmNunitType,      &unit_type,
              NULL);

    (void)XSetDashes(XtDisplay(w), gc, 0, medium_dashed , 2);

    screen_width  = (long)width;
    screen_height = (long)height;
    long_offset_temp = NW_corner_longitude = center_longitude - (screen_width  * scale_x / 2);  // NW corner
    lat_offset_temp  = NW_corner_latitude  = center_latitude  - (screen_height * scale_y / 2);

    SE_corner_longitude = center_longitude + (screen_width * scale_x / 2);
    SE_corner_latitude = center_latitude + (screen_height * scale_y / 2);

    // Set up floating point lat/long values to match Xastir
    // coordinates.
    convert_from_xastir_coordinates(&f_NW_corner_longitude,
        &f_NW_corner_latitude,
        NW_corner_longitude,
        NW_corner_latitude);
    convert_from_xastir_coordinates(&f_SE_corner_longitude,
        &f_SE_corner_latitude,
        SE_corner_longitude,
        SE_corner_latitude);

    /* map default background color */
    switch (map_background_color){
        case 0 :
            colors[0xfd] = GetPixelByName(appshell,"gray73");
            break;

        case 1 :
            colors[0xfd] = GetPixelByName(w,"MistyRose");
            break;

        case 2 :
            colors[0xfd] = GetPixelByName(w,"NavyBlue");
            break;

        case 3 :
            colors[0xfd] = GetPixelByName(w,"SteelBlue");
            break;

        case 4 :
            colors[0xfd] = GetPixelByName(w,"MediumSeaGreen");
            break;

        case 5 :
            colors[0xfd] = GetPixelByName(w,"PaleGreen");
            break;

        case 6 :
            colors[0xfd] = GetPixelByName(w,"PaleGoldenrod");
            break;

        case 7 :
            colors[0xfd] = GetPixelByName(w,"LightGoldenrodYellow");
            break;

        case 8 :
            colors[0xfd] = GetPixelByName(w,"RosyBrown");
            break;

        case 9 :
            colors[0xfd] = GetPixelByName(w,"firebrick");
            break;

        case 10 :
            colors[0xfd] = GetPixelByName(w,"white");
            break;

        case 11 :
            colors[0xfd] = GetPixelByName(w, "black");
            break;

        default:
            colors[0xfd] = GetPixelByName(appshell,"gray73");
            map_background_color=0;
            break;
    }

    HandlePendingEvents(app_context);
    if (interrupt_drawing_now)
        return(0);
 
    (void)XSetForeground(XtDisplay(w),gc,colors[0xfd]);
    (void)XSetBackground(XtDisplay(w),gc,colors[0xfd]);

    (void)XFillRectangle(XtDisplay(w),
        pixmap,
        gc,
        0,
        0,
        (unsigned int)screen_width,
        (unsigned int)screen_height);

    HandlePendingEvents(app_context);
    if (interrupt_drawing_now)
        return(0);
 
    statusline(langcode("BBARSTA003"),1);       // Loading Maps

    HandlePendingEvents(app_context);
    if (interrupt_drawing_now)
        return(0);
 
    if (display_up_first != 0) {
        if (map_auto_maps && !disable_all_maps)
            load_auto_maps(w,AUTO_MAP_DIR);
        else if (!disable_all_maps)
            load_maps(w);
    }

    if (!wx_alert_style)
        statusline(langcode("BBARSTA034"),1);

    // Update to screen
//    (void)XCopyArea(XtDisplay(da),pixmap,XtWindow(da),gc,0,0,(unsigned int)screen_width,(unsigned int)screen_height,0,0);

    HandlePendingEvents(app_context);
    if (interrupt_drawing_now)
        return(0);
 
    /* copy map data to alert pixmap */
    (void)XCopyArea(XtDisplay(w),pixmap,pixmap_alerts,gc,0,0,(unsigned int)screen_width,(unsigned int)screen_height,0,0);

    HandlePendingEvents(app_context);
    if (interrupt_drawing_now)
        return(0);
 
    if (!wx_alert_style && !disable_all_maps)
        load_alert_maps(w, ALERT_MAP_DIR);  // These write onto pixmap_alerts

    // Update to screen
//    (void)XCopyArea(XtDisplay(da),pixmap_alerts,XtWindow(da),gc,0,0,(unsigned int)screen_width,(unsigned int)screen_height,0,0);

    HandlePendingEvents(app_context);
    if (interrupt_drawing_now)
        return(0);
 
    /* copy map and alert data to final pixmap */
    (void)XCopyArea(XtDisplay(w),pixmap_alerts,pixmap_final,gc,0,0,(unsigned int)screen_width,(unsigned int)screen_height,0,0);

    HandlePendingEvents(app_context);
    if (interrupt_drawing_now)
        return(0);
 
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

    HandlePendingEvents(app_context);
    if (interrupt_drawing_now)
        return(0);
 
    draw_grid(w);                       // Draw grid if enabled

    HandlePendingEvents(app_context);
    if (interrupt_drawing_now)
        return(0);
 
    display_file(w);                    // display stations (symbols, info, trails)

    last_alert_redraw=sec_now();        // set last time of screen redraw

    if (debug_level & 4)
        fprintf(stderr,"Create image stop\n");

    return(1);
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
        fprintf(stderr,"Refresh image start\n");

    // If we're in the middle of ID'ing, wait a bit.
    if (ATV_screen_ID && pending_ID_message) 
        usleep(2000000);    // 2 seconds

    /* First get the various dimensions */
    XtVaGetValues(w,
              XmNwidth,         &width,
              XmNheight,        &height,
              XmNmarginWidth,   &margin_width,
              XmNmarginHeight,  &margin_height,
              XmNunitType,      &unit_type,
              NULL);

    (void)XSetDashes(XtDisplay(w), gc, 0, medium_dashed , 2);

    screen_width  = (long)width;
    screen_height = (long)height;

    long_offset_temp = NW_corner_longitude = center_longitude - (screen_width * scale_x / 2);
    NW_corner_latitude     = center_latitude - (screen_height * scale_y / 2);
    lat_offset_temp  = center_latitude;

    SE_corner_longitude = center_longitude + (screen_width * scale_x / 2);
    SE_corner_latitude = center_latitude + (screen_height * scale_y / 2);

    // Set up floating point lat/long values to match Xastir
    // coordinates.
    convert_from_xastir_coordinates(&f_NW_corner_longitude,
        &f_NW_corner_latitude,
        NW_corner_longitude,
        NW_corner_latitude);
    convert_from_xastir_coordinates(&f_SE_corner_longitude,
        &f_SE_corner_latitude,
        SE_corner_longitude,
        SE_corner_latitude);

    (void)XSetForeground(XtDisplay(w),gc,colors[0xfd]);
    (void)XSetBackground(XtDisplay(w),gc,colors[0xfd]);

    /* copy over map data to alert pixmap */
    (void)XCopyArea(XtDisplay(w),pixmap,pixmap_alerts,gc,0,0,(unsigned int)screen_width,(unsigned int)screen_height,0,0);

    if (!wx_alert_style && !disable_all_maps) {
            statusline(langcode("BBARSTA034"),1);
            load_alert_maps(w, ALERT_MAP_DIR);  // These write onto pixmap_alerts
    }

    /* copy over map and alert data to final pixmap */
    (void)XCopyArea(XtDisplay(w),pixmap_alerts,pixmap_final,gc,0,0,(unsigned int)screen_width,(unsigned int)screen_height,0,0);

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

    HandlePendingEvents(app_context);
    if (interrupt_drawing_now)
        return;
 
    /* display icons */
    display_file(w);

    /* set last time of screen redraw*/
    last_alert_redraw=sec_now();

    // We just refreshed the screen, so don't try to erase any
    // zoom-in boxes via XOR.
    zoom_box_x1 = -1;

    if (debug_level & 4)
        fprintf(stderr,"Refresh image stop\n");
}





// And this function is even faster yet.  It snags "pixmap_alerts",
// which already has map and alert data drawn into it, copies it to
// pixmap_final, then draws symbols and tracks on top of it.  When
// done it copies the image to the drawing area, making it visible.
void redraw_symbols(Widget w) {


    HandlePendingEvents(app_context);
    if (interrupt_drawing_now)
        return;

    // If we're in the middle of ID'ing, wait a bit.
    if (ATV_screen_ID && pending_ID_message)
        usleep(2000000);    // 2 seconds

    /* copy over map and alert data to final pixmap */
    if(!wait_to_redraw) {

        (void)XCopyArea(XtDisplay(w),pixmap_alerts,pixmap_final,gc,0,0,(unsigned int)screen_width,(unsigned int)screen_height,0,0);

        draw_grid(w);           // draw grid if enabled

        HandlePendingEvents(app_context);
        if (interrupt_drawing_now)
            return;

        display_file(w);        // display stations (symbols, info, trails)

        (void)XCopyArea(XtDisplay(w),pixmap_final,XtWindow(w),gc,0,0,(unsigned int)screen_width,(unsigned int)screen_height,0,0);
    }
    else {
        fprintf(stderr,"wait_to_redraw\n");
    }

    // We just refreshed the screen, so don't try to erase any
    // zoom-in boxes via XOR.
    zoom_box_x1 = -1;
}





static void TrackMouse( /*@unused@*/ Widget w, XtPointer clientData, XEvent *event, /*@unused@*/ Boolean *flag) {
    char my_text[50];
    char str_lat[20];
    char str_long[20];
    long x, y;
    //beg
    char temp_my_distance[20];
    char temp_my_course[20];
    char temp1_my_course[20];
    long ml_lat, ml_lon;
    float value;
    //end


    Widget textarea = (Widget) clientData;

    x = (center_longitude - ((screen_width  * scale_x)/2) + (event->xmotion.x * scale_x));
    y = (center_latitude  - ((screen_height * scale_y)/2) + (event->xmotion.y * scale_y));

    if (x < 0)
//        x = 0l;                 // 180°W
        return;

    if (x > 129600000l)
//        x = 129600000l;         // 180°E
        return;

    if (y < 0)
//        y = 0l;                 //  90°N
        return;

    if (y > 64800000l)
//        y = 64800000l;          //  90°S
        return;

    if (DISPLAY_XASTIR_COORDINATES) {
        xastir_snprintf(my_text, sizeof(my_text), "%ld  %ld", y, x);
    }
    else if (coordinate_system == USE_UTM
            || coordinate_system == USE_UTM_SPECIAL) {
        // Create a UTM string from coordinate in Xastir coordinate
        // system.
        convert_xastir_to_UTM_str(my_text, sizeof(my_text), x, y);
    }
    else if (coordinate_system == USE_MGRS) {
        // Create an MGRS string from coordinate in Xastir
        // coordinate system.
        convert_xastir_to_MGRS_str(my_text, sizeof(my_text), x, y, 0);
    }
    else {
        // Create a lat/lon string from coordinate in Xastir
        // coordinate system.
        if (coordinate_system == USE_DDDDDD) {
            convert_lat_l2s(y, str_lat, sizeof(str_lat), CONVERT_DEC_DEG);
            convert_lon_l2s(x, str_long, sizeof(str_long), CONVERT_DEC_DEG);
        } else if (coordinate_system == USE_DDMMSS) {
            convert_lat_l2s(y, str_lat, sizeof(str_lat), CONVERT_DMS_NORMAL_FORMATED);
            convert_lon_l2s(x, str_long, sizeof(str_long), CONVERT_DMS_NORMAL_FORMATED);
            //str_lat[2]='°'; str_long[3]='°';
            //str_lat[5]='\''; str_long[6]='\'';
        } else {    // Assume coordinate_system == USE_DDMMMM
            convert_lat_l2s(y, str_lat, sizeof(str_lat), CONVERT_HP_NORMAL_FORMATED);
            convert_lon_l2s(x, str_long, sizeof(str_long), CONVERT_HP_NORMAL_FORMATED);
            //str_lat[2]='°'; str_long[3]='°';
        }
        xastir_snprintf(my_text, sizeof(my_text), "%s  %s", str_lat, str_long);
    }

    strncat(my_text,
        "  ",
        sizeof(my_text) - strlen(my_text));

    strncat(my_text,
        sec_to_loc(x,y),
        sizeof(my_text) - strlen(my_text));

    // begin dist/bearing
    if ( do_dbstatus ) {
        ml_lat = convert_lat_s2l(my_lat);
        ml_lon = convert_lon_s2l(my_long);

        // Get distance in nautical miles.
        value = (float)calc_distance_course(ml_lat,ml_lon,y,x,
                temp1_my_course,sizeof(temp1_my_course));

        // n7tap: This is a quick hack to get some more useful values for
        //        distance to near ojects.
        // (copied from db.c:station_data_fill_in)
        if (english_units) {
            if (value*1.15078 < 0.99) {
                xastir_snprintf(temp_my_distance,
                    sizeof(temp_my_distance),
                    "%d %s",
                    (int)(value*1.15078*1760),
                    langcode("SPCHSTR004"));    // yards
            } 
            else {
                xastir_snprintf(temp_my_distance,
                    sizeof(temp_my_distance),
                    langcode("WPUPSTI020"),     // miles
                    value*1.15078);
            }
        }
        else {
            if (value*1.852 < 0.99) {
                xastir_snprintf(temp_my_distance,
                    sizeof(temp_my_distance),
                    "%d %s",
                    (int)(value*1.852*1000),
                    langcode("UNIOP00031"));    // 'm' as in meters
            }
            else {
                xastir_snprintf(temp_my_distance,
                    sizeof(temp_my_distance),
                    langcode("WPUPSTI021"),     // km
                    value*1.852);
            }
        }
        xastir_snprintf(temp_my_course, sizeof(temp_my_course), "%s°",temp1_my_course);


        strncat(my_text,
            " ",
            sizeof(my_text) - strlen(my_text));

        strncat(my_text,
            temp_my_distance,
            sizeof(my_text) - strlen(my_text));

        strncat(my_text,
            " ",
            sizeof(my_text) - strlen(my_text));

        strncat(my_text,
            temp_my_course,
            sizeof(my_text) - strlen(my_text));
    }

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





// Get a pointer to the first station record.  Loop through all
// station records and clear out the tactical_call_sign fields in
// each.
// 
void clear_all_tactical(void) {
    DataRow *p_station = n_first;

    // Run through the name-ordered list of records
    while (p_station != 0) {
        if (p_station->tactical_call_sign) {
            // One found.  Free it.
            free(p_station->tactical_call_sign);
            p_station->tactical_call_sign = NULL;
        }
        p_station = p_station->n_next;
    }
    fprintf(stderr,"Cleared all tactical calls\n");
}





/*
 *  Clear all tactical callsigns from the station records.  Comment
 *  out the active records in the log file.
 */
void Tactical_Callsign_Clear( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer callData) {
    char *ptr;
    char file[200];
    char file_temp[200];
    FILE *f;
    FILE *f_temp;
    char line[300];
    int ret;


    // Loop through all station records and clear out the
    // tactical_call_sign fields in each.
    clear_all_tactical();

    // Get rid of the tactical callsign hash here
    destroy_tactical_hash();

    ptr = get_user_base_dir("config/tactical_calls.log");
    xastir_snprintf(file,sizeof(file),"%s",ptr);

    ptr = get_user_base_dir("config/tactical_calls-temp.log");
    xastir_snprintf(file_temp,sizeof(file_temp),"%s",ptr);

    // Our own internal function from util.c
    ret = copy_file(file, file_temp);
    if (ret) {
        fprintf(stderr,"\n\nCouldn't create temp file %s!\n\n\n",
            file_temp);
        return;
    }

    // Comment out all active lines in the log file via a '#' mark.
    // Read one line at a time from the temp file.  Add a '#'
    // mark to the line if it doesn't already have it, then write
    // that line to the original file.
    f_temp=fopen(file_temp,"r");
    f=fopen(file,"w");

    if (f == NULL) {
        fprintf(stderr,"Couldn't open %s\n",file);
        return;
    }
    if (f_temp == NULL) {
        fprintf(stderr,"Couldn't open %s\n",file_temp);
        return;
    }

    // Read lines from the temp file and write them to the standard
    // file, modifying them as necessary.
    while (fgets(line, 300, f_temp) != NULL) {

        if (line[0] != '#') {
            fprintf(f,"#%s",line);
        }
        else {
            fprintf(f,"%s",line);
        }
    }
    fclose(f);
    fclose(f_temp);
}





/*
 *  Clear out tactical callsign log file
 */
void Tactical_Callsign_History_Clear( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer callData) {
    char *file;
    FILE *f;


    // Loop through all station records and clear out the
    // tactical_call_sign fields in each.
    clear_all_tactical();

    // Get rid of the tactical callsign hash here
    destroy_tactical_hash();

    // Wipe out the log file.
    file = get_user_base_dir("config/tactical_calls.log");

    f=fopen(file,"w");
    if (f!=NULL) {
        (void)fclose(f);

        if (debug_level & 1)
            fprintf(stderr,"Clearing tactical callsign file...\n");
    }
    else {
        fprintf(stderr,"Couldn't open file for writing: %s\n", file);
    }

    fprintf(stderr,"Cleared tactical call history file\n");
}





/*
 *  Display text in the status line, text is removed after timeout
 */
void statusline(char *status_text,int update) {

    XmTextFieldSetString (text, status_text);
    last_statusline = sec_now();    // Used for auto-ID timeout
//    if (update != 0)
//        XmUpdateDisplay(text);      // do an immediate update
}





/*  print a message on standard error and display the same message
 *   on the status line of the user interface.
*/
void stderr_and_statusline(char *message) { 
   fprintf(stderr,"%s",message);
   if (message[strlen(message)-1]=='\n') { 
       // if there is a terminal new line character remove it.
       message[strlen(message)-1]='\0';
   }
   statusline(message,1);
   XmUpdateDisplay(text);      // force an immediate update
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
void check_statusline_timeout(int curr_sec) {
    char status_text[100];
    int id_interval = (int)(9.5 * 60);
//    int id_interval = (int)(1 * 5);  // Debug


    if ( (last_statusline != 0
            && (last_statusline < curr_sec - STATUSLINE_ACTIVE))
        || (last_id_time < curr_sec - id_interval) ) {


        // We save last_id_time and identify for a few seconds if
        // we haven't identified for the last nine minutes or so.

        xastir_snprintf(status_text,
            sizeof(status_text),
            langcode ("BBARSTA040"),
            my_callsign);

        XmTextFieldSetString(text, status_text);

        if (last_id_time < curr_sec - id_interval) {
            popup_ID_message(langcode("BBARSTA040"),status_text);
#ifdef HAVE_FESTIVAL
            if (festival_speak_ID) {
                char my_speech_callsign[100];

                xastir_snprintf(my_speech_callsign,
                    sizeof(my_speech_callsign),
                    "%s",
                    my_callsign);
                spell_it_out(my_speech_callsign, 100);
                xastir_snprintf(status_text,
                    sizeof(status_text),
                    langcode ("BBARSTA040"),
                    my_speech_callsign);
                SayText(status_text);
            }
#endif  // HAVE_FESTIVAL
        }

        last_statusline = 0;    // now inactive

        // Guarantee that the ID text will be viewable for a few
        // seconds if we haven't identified recently.  Note that the
        // sleep statement puts the entire main thread to sleep for
        // that amount of time.  The application will be unresponsive
        // during that time.

        if (last_id_time < (curr_sec - (9 * 60))) {
            //fprintf(stderr,"Identifying at nine minutes\n");
            //sleep(1);
        }

        last_id_time = curr_sec;
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





void Change_debug_level_reset(Widget widget, XtPointer clientData, XtPointer callData) {
    debug_level = 0;
    XmTextSetString(debug_level_text, "0");
//    Change_debug_level_destroy_shell(widget,clientData,callData);
}





void Change_debug_level_change_data(Widget widget, XtPointer clientData, XtPointer callData) {
    char *temp;
    char temp_string[10];

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
    static Widget  pane, my_form, button_ok, button_close,
        button_reset;
    Atom delw;
    Arg al[50];                    /* Arg List */
    register unsigned int ac = 0;           /* Arg Count */
    char temp_string[10];

    if (!change_debug_level_dialog) {
        change_debug_level_dialog = XtVaCreatePopupShell(langcode("PULDNFI007"),
                xmDialogShellWidgetClass, appshell,
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
                XmNfractionBase, 3,
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
                XmNleftPosition, 1,
                XmNrightAttachment,XmATTACH_POSITION,
                XmNrightPosition, 2,
                XmNnavigationType, XmTAB_GROUP,
                XmNfontList, fontlist1,
                NULL);

        // Fill in the current value of debug_level
        xastir_snprintf(temp_string, sizeof(temp_string), "%d", debug_level);
        XmTextSetString(debug_level_text, temp_string);

        button_reset = XtVaCreateManagedWidget(langcode("UNIOP00033"),
                xmPushButtonGadgetClass,
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, debug_level_text,
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
                XmNfontList, fontlist1,
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
                XmNleftPosition, 2,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 3,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        XtAddCallback(button_reset, XmNactivateCallback, Change_debug_level_reset, change_debug_level_dialog);
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





#if !defined(NO_GRAPHICS) && defined(HAVE_MAGICK)
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

    // Set interrupt_drawing_now because conditions have changed
    // (new map center).
    interrupt_drawing_now++;

    // Request that a new image be created.  Calls create_image,
    // XCopyArea, and display_zoom_status.
    request_new_image++;

//    if (create_image(da)) {
//        XCopyArea(XtDisplay(da),pixmap_final,XtWindow(da),gc,0,0,(unsigned int)screen_width,(unsigned int)screen_height,0,0);
//    }
}





void Gamma_adjust(Widget w, XtPointer clientData, XtPointer callData) {
    static Widget  pane, my_form, button_ok, button_close;
    Atom delw;
    char temp_string[10];

    if (!gamma_adjust_dialog) {
        // Gamma Correction
        gamma_adjust_dialog = XtVaCreatePopupShell(langcode("GAMMA002"),
                xmDialogShellWidgetClass, appshell,
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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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
#endif  // NO_GRAPHICS && HAVE_MAGICK





void Load_station_font(void) {
    // Assign a font (or a different font) to the GC

    // Free any old font first.  If we fail to assign a new font in
    // the code here, can we get in a sitation where we are trying
    // to draw without a font?
    if (station_font != NULL) {
        XFreeFont(XtDisplay(da), station_font);
    }

    // Load the new font from the FONT_STATION string
    station_font = (XFontStruct *)XLoadQueryFont(XtDisplay(da), rotated_label_fontname[FONT_STATION]);

    if (station_font == NULL) {    // Couldn't get the font!!!
        char tempy[100];

        fprintf(stderr,"Map_font_change_data: Couldn't load station font %s.  ",
            rotated_label_fontname[FONT_STATION]);
        fprintf(stderr,"Loading default station font instead.\n");

        xastir_snprintf(tempy,
            sizeof(tempy),
            "Couldn't get font %s.  Loading default font instead.",
            rotated_label_fontname[FONT_STATION]);
        popup_message_always(langcode("POPEM00035"), tempy);

        station_font = (XFontStruct *)XLoadQueryFont(XtDisplay(da), "fixed");
        if (station_font == NULL) {    // Couldn't get the font!!!
            fprintf(stderr,"Map_font_change_data: Couldn't load default station font.\n");

            popup_message_always(langcode("POPEM00035"),
                "Couldn't load default station font.");
        }
    }

    // Assign the font to the GC.
    if (station_font != NULL) {
        XSetFont(XtDisplay(da), gc, station_font->fid);
    }
}





// chose map label font
void Map_font_destroy_shell( /*@unused@*/ Widget widget, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    Widget shell = (Widget) clientData;

    xfontsel_query = 0;
    XtPopdown(shell);
    XtDestroyWidget(shell);
    map_font_dialog = (Widget)NULL;
}





// Function called by UpdateTime when xfontsel_query is non-zero.
// Checks the pipe to see if xfontsel has sent anything to us yet.
// If we get anything from the read, we should wait a small amount
// of time and try another read, to make sure we don't get a partial
// read the first time and quit.
//
void Query_xfontsel_pipe (void) {
    char xfontsel_font[FONT_MAX][sizeof(rotated_label_fontname[0])];
    struct timeval tmv;
    fd_set rd;
    int retval;
    int fd;
    int i;

    for (i = 0; i < FONT_MAX; i++) {

        //    if (fgets(xfontsel_font,sizeof(xfontsel_font),f_xfontsel_pipe)) {

        // Find out the file descriptor associated with our pipe.
        if (!f_xfontsel_pipe[i])
            continue;
        fd = fileno(f_xfontsel_pipe[i]);

        FD_ZERO(&rd);
        FD_SET(fd, &rd);
        tmv.tv_sec = 0;
        tmv.tv_usec = 1;    // 1 usec

        // Do a non-blocking check of the read end of the pipe.
        retval = select(fd+1,&rd,NULL,NULL,&tmv);

        //fprintf(stderr,"1\n");

        if (retval) {
            int l = strlen(xfontsel_font[i]);

            // We have something to process.  Wait a bit, then snag the
            // data.
            usleep(250000); // 250ms
            
            fgets(xfontsel_font[i],sizeof(xfontsel_font[0]),f_xfontsel_pipe[i]);
 
            if (xfontsel_font[i][l-1] == '\n')
                xfontsel_font[i][l-1] = '\0';
            if (map_font_text[i] != NULL) {
                XmTextSetString(map_font_text[i], xfontsel_font[i]);
            }
            pclose(f_xfontsel_pipe[i]);
            f_xfontsel_pipe[i] = 0;
            //fprintf(stderr,"Resetting xfontset_query\n");
            xfontsel_query = 0;
        }
        else {
            // Read nothing.  Let UpdateTime run this function again
            // shortly.
        }
    }
}




 
void Map_font_xfontsel(Widget widget, XtPointer clientData, XtPointer callData) {

#if defined(HAVE_XFONTSEL)
 
    int fontsize = XTPOINTER_TO_INT(clientData);
    char xfontsel[50];

    /* invoke xfontsel -print and stick into map_font_text */
    sprintf(xfontsel,
        "%s -print -title \"xfontsel %d\"",
        XFONTSEL_PATH,
        fontsize);
    if ((f_xfontsel_pipe[fontsize] = popen(xfontsel,"r"))) {

        // Request UpdateTime to keep checking the pipe periodically
        // using non-blocking reads.
//fprintf(stderr,"Setting xfontsel_query\n");
        xfontsel_query++;

    } else {
        perror("xfontsel");
    }
#endif  // HAVE_XFONTSEL
}





void Map_font_change_data(Widget widget, XtPointer clientData, XtPointer callData) {
    char *temp;
    Widget shell = (Widget) clientData;
    int i;


    xfontsel_query = 0;

    for (i = 0; i < FONT_MAX; i++) {
        temp = XmTextGetString(map_font_text[i]);

        xastir_snprintf(rotated_label_fontname[i],
            sizeof(rotated_label_fontname[i]),
            "%s",
            temp);

        XtFree(temp);
        XmTextSetString(map_font_text[i], rotated_label_fontname[i]);
    }

    // Load a new font into the GC for the station font
    Load_station_font();

    // Set interrupt_drawing_now because conditions have changed
    // (new map center).
    interrupt_drawing_now++;

    // Request that a new image be created.  Calls create_image,
    // XCopyArea, and display_zoom_status.
    request_new_image++;

    XtPopdown(shell);
    XtDestroyWidget(shell);
    map_font_dialog = (Widget)NULL;
}





void Map_font(Widget w, XtPointer clientData, XtPointer callData) {
    static Widget  pane, my_form, fontname[FONT_MAX], button_ok,
                button_cancel,button_xfontsel[FONT_MAX];
    Atom delw;
    int i;
    Arg al[50];                 /* Arg List */
    register unsigned int ac = 0; /* Arg Count */


    if (!map_font_dialog) {
        map_font_dialog = XtVaCreatePopupShell(langcode("MAPFONT002"),
                xmDialogShellWidgetClass, appshell,
                XmNdeleteResponse,        XmDESTROY,
                XmNdefaultPosition,       FALSE,
                XmNfontList, fontlist1,
                NULL);

        pane = XtVaCreateWidget("Choose map labels font",
                xmPanedWindowWidgetClass, map_font_dialog,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        my_form =  XtVaCreateWidget("Map font my_form",
                xmFormWidgetClass,  pane,
                XmNfractionBase,    3,
                XmNautoUnmanage,    FALSE,
                XmNshadowThickness, 1,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        //        ac=0;
        //        XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
        //        XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;


        for (i = 0; i < FONT_MAX; i++) {
            char *fonttitle[FONT_MAX] = {"MAPFONT009","MAPFONT010","MAPFONT003",
                                         "MAPFONT004","MAPFONT005","MAPFONT006",
                                         "MAPFONT007","MAPFONT008","MAPFONT011"};
            ac = 0;
            if (i == 0) {
                XtSetArg(al[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
                XtSetArg(al[ac], XmNtopOffset, 10); ac++;
            } else {
                XtSetArg(al[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
                XtSetArg(al[ac], XmNtopWidget, fontname[i-1]); ac++;
            }
            XtSetArg(al[ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
            XtSetArg(al[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
            XtSetArg(al[ac], XmNleftOffset, 5); ac++;
            XtSetArg(al[ac], XmNwidth, 150); ac++;
            XtSetArg(al[ac], XmNheight, 40); ac++;
            XtSetArg(al[ac], XmNrightAttachment, XmATTACH_NONE); ac++;
            XtSetArg(al[ac], XmNforeground,colors[0x08]); ac++;
            XtSetArg(al[ac], XmNbackground,colors[0xff]); ac++;
            XtSetArg(al[ac], XmNfontList, fontlist1); ac++;
            fontname[i] = XtCreateManagedWidget(langcode(fonttitle[i]),
                                                xmLabelWidgetClass, 
                                                my_form,
                                                al, ac);
            ac = 0;
            if (i == 0) {
                XtSetArg(al[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
                XtSetArg(al[ac], XmNtopOffset, 10); ac++;
            } else {
                XtSetArg(al[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
                XtSetArg(al[ac], XmNtopWidget, map_font_text[i-1]); ac++;
            }
            XtSetArg(al[ac], XmNeditable,              TRUE); ac++;
            XtSetArg(al[ac], XmNcursorPositionVisible, TRUE); ac++;
            XtSetArg(al[ac], XmNsensitive,             TRUE); ac++;
            XtSetArg(al[ac], XmNshadowThickness,       1); ac++;
            XtSetArg(al[ac], XmNcolumns,               60); ac++;
            XtSetArg(al[ac], XmNwidth,                 (60*7)+2); ac++;
            XtSetArg(al[ac], XmNmaxLength,             128); ac++;
            XtSetArg(al[ac], XmNbackground,            colors[0x0f]); ac++;
            XtSetArg(al[ac], XmNbottomAttachment,XmATTACH_NONE); ac++;
            XtSetArg(al[ac], XmNleftAttachment, XmATTACH_WIDGET); ac++;
            XtSetArg(al[ac], XmNleftWidget, fontname[i]); ac++;
            XtSetArg(al[ac], XmNleftOffset, 10); ac++;
            XtSetArg(al[ac], XmNheight, 40); ac++;
            XtSetArg(al[ac], XmNrightAttachment,XmATTACH_NONE); ac++;
            XtSetArg(al[ac], XmNnavigationType, XmTAB_GROUP); ac++;
            XtSetArg(al[ac], XmNtraversalOn, TRUE); ac++;
            XtSetArg(al[ac], XmNfontList, fontlist1); ac++;
            map_font_text[i] = XtCreateManagedWidget("Map font text",
                                                       xmTextFieldWidgetClass, my_form,
                                                       al, ac);

            XmTextSetString(map_font_text[i], rotated_label_fontname[i]);

            // Xfontsel
            ac = 0;
            if (i == 0) {
                XtSetArg(al[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
                XtSetArg(al[ac], XmNtopOffset, 10); ac++;
            } else {
                XtSetArg(al[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
                XtSetArg(al[ac], XmNtopWidget, button_xfontsel[i-1]); ac++;
            }
            XtSetArg(al[ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
            XtSetArg(al[ac], XmNleftAttachment, XmATTACH_WIDGET); ac++;
            XtSetArg(al[ac], XmNleftWidget, map_font_text[i]); ac++;
            XtSetArg(al[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
            XtSetArg(al[ac], XmNrightOffset, 10); ac++;
            XtSetArg(al[ac], XmNheight, 40); ac++;
            XtSetArg(al[ac], XmNnavigationType, XmTAB_GROUP); ac++;
            XtSetArg(al[ac], XmNforeground,colors[0x08]); ac++;
            XtSetArg(al[ac], XmNbackground,colors[0xff]); ac++;
            XtSetArg(al[ac], XmNfontList, fontlist1); ac++;
            button_xfontsel[i] = XtCreateManagedWidget(langcode("PULDNMP015"),
                                                         xmPushButtonGadgetClass, my_form,
                                                         al,ac);

#if defined(HAVE_XFONTSEL)
            XtSetSensitive(button_xfontsel[i],TRUE);
#else   // HAVE_FONTSEL
            XtSetSensitive(button_xfontsel[i],FALSE);
#endif  // HAVE_FONTSEL
 
            XtAddCallback(button_xfontsel[i],
                XmNactivateCallback,
                Map_font_xfontsel,
                INT_TO_XTPOINTER(i) );

        }
        button_ok = XtVaCreateManagedWidget(langcode("UNIOP00001"),
                xmPushButtonGadgetClass, my_form,
                XmNtopAttachment,        XmATTACH_WIDGET,
                XmNtopWidget,            map_font_text[FONT_MAX-1],
                XmNtopOffset,            10,
                XmNbottomAttachment,     XmATTACH_FORM,
                XmNbottomOffset,         5,
                XmNleftAttachment,       XmATTACH_POSITION,
                XmNleftPosition,         1,
                XmNrightAttachment,      XmATTACH_POSITION,
                XmNrightPosition,        2,
                XmNnavigationType,       XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        button_cancel = XtVaCreateManagedWidget(langcode("UNIOP00002"),
                xmPushButtonGadgetClass, my_form,
                XmNtopAttachment,        XmATTACH_WIDGET,
                XmNtopWidget,            map_font_text[FONT_MAX-1],
                XmNtopOffset,            10,
                XmNbottomAttachment,     XmATTACH_FORM,
                XmNbottomOffset,         5,
                XmNleftAttachment,       XmATTACH_POSITION,
                XmNleftPosition,         2,
                XmNrightAttachment,      XmATTACH_POSITION,
                XmNrightPosition,        3,
                XmNnavigationType,       XmTAB_GROUP,
                XmNtraversalOn, TRUE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        XtAddCallback(button_ok,
                      XmNactivateCallback, Map_font_change_data,   map_font_dialog);
        XtAddCallback(button_cancel,
                      XmNactivateCallback, Map_font_destroy_shell, map_font_dialog);

        pos_dialog(map_font_dialog);

        delw = XmInternAtom(XtDisplay(map_font_dialog), "WM_DELETE_WINDOW", FALSE);
        XmAddWMProtocolCallback(map_font_dialog,
                                delw, Map_font_destroy_shell, (XtPointer)map_font_dialog);

        XtManageChild(my_form);
        XtManageChild(pane);

        XtPopup(map_font_dialog, XtGrabNone);
        fix_dialog_size(map_font_dialog);

        // Move focus to the Close button.  This appears to highlight the
        // button fine, but we're not able to hit the <Enter> key to
        // have that default function happen.  Note:  We _can_ hit the
        // <SPACE> key, and that activates the option.
//        XmUpdateDisplay(map_font_dialog);
        XmProcessTraversal(button_cancel, XmTRAVERSE_CURRENT);

    } else
        (void)XRaiseWindow(XtDisplay(map_font_dialog), XtWindow(map_font_dialog));
}





// Used by view_gps_status() function below.  We must either expire
// this data or associate a time with it on the display.
char gps_status_save[100];
time_t gps_status_save_time = 0;





char *report_gps_status(void) {
    static char gps_temp[100];
    char temp2[20];

    switch (gps_valid) {

        case 8: // Simulation Mode
            xastir_snprintf(temp2,
                sizeof(temp2),
                "%s",
                langcode("GPSS008") );  // "Simulation"
            break;

        case 7: // Manual Input Mode
            xastir_snprintf(temp2,
                sizeof(temp2),
                "%s",
                langcode("GPSS009") );  // "Manual"
            break;
 
        case 6: // Estimated Fix (dead reckoning)
            xastir_snprintf(temp2,
                sizeof(temp2),
                "%s",
                langcode("GPSS010") );  // "Estimated"
            break;

        case 5: // Float RTK
            xastir_snprintf(temp2,
                sizeof(temp2),
                "%s",
                langcode("GPSS011") );  // "Float RTK"
            break;

        case 4: // RTK
            xastir_snprintf(temp2,
                sizeof(temp2),
                "%s",
                langcode("GPSS012") );  // "RTK"
            break;
 
        case 3: // WAAS or PPS Fix
            xastir_snprintf(temp2,
                sizeof(temp2),
                "%s",
                langcode("GPSS001") );  // "WAAS or PPS"
            break;

        case 2: // DGPS Fix
            xastir_snprintf(temp2,
                sizeof(temp2),
                "%s",
                langcode("GPSS002") );  // "DGPS"
            break;

        case 1: // Valid SPS Fix
            xastir_snprintf(temp2,
                sizeof(temp2),
                "%s",
                langcode("GPSS003") );  // "Valid SPS"
            break;

        case 0: // Invalid
        default:
            xastir_snprintf(temp2,
                sizeof(temp2),
                "%s",
                langcode("GPSS004") );  // "Invalid"
            break;
    }

    xastir_snprintf(gps_temp,
        sizeof(gps_temp),
        "%s:%s %s:%s",
        langcode("GPSS005"),    // "Sats/View"
        gps_sats,
        langcode("GPSS006"),    // "Fix"
        temp2);

    // Save it in global variable in case we request status via the
    // menus.
    xastir_snprintf(gps_status_save,
        sizeof(gps_status_save),
        "%s",
        gps_temp);

    gps_status_save_time = sec_now();
 
    // Reset the variables.
    xastir_snprintf(gps_sats, sizeof(gps_sats), "00");
    gps_valid = 0;

    return(gps_temp);
}





void view_gps_status(Widget w, XtPointer clientData, XtPointer callData) {

    // GPS status data too old?
    if ((gps_status_save_time + 30) >= sec_now()) {
        // Nope, within 30 seconds
        popup_message_always(langcode("PULDNVI015"),
            gps_status_save);
    }
    else {
        // Yes, GPS status data is old
        popup_message_always(langcode("PULDNVI015"),
            langcode("GPSS007") );
            // "!GPS data is older than 30 seconds!"
    }
}





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
        xastir_snprintf(Days,sizeof(Days),"%s",langcode("TIME001")); // Day
    else
        xastir_snprintf(Days,sizeof(Days),"%s",langcode("TIME002")); // Days


    if (hours == 1)
        xastir_snprintf(Hours,sizeof(Hours),"%s",langcode("TIME003")); // Hour
    else
        xastir_snprintf(Hours,sizeof(Hours),"%s",langcode("TIME004")); // Hours


    if (minutes == 1)
        xastir_snprintf(Minutes,sizeof(Minutes),"%s",langcode("TIME005")); // Minute
    else
        xastir_snprintf(Minutes,sizeof(Minutes),"%s",langcode("TIME006")); // Minutes
 

    if (seconds == 1)
        xastir_snprintf(Seconds,sizeof(Seconds),"%s",langcode("TIME007")); // Second
    else
        xastir_snprintf(Seconds,sizeof(Seconds),"%s",langcode("TIME008")); // Seconds


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
    popup_message_always(langcode("PULDNVI014"),temp);
}





void Mouse_button_handler (Widget w, Widget popup, XButtonEvent *event) {

    // Snag the current pointer position
    input_x = event->x;
    input_y = event->y;

    if (event->type == ButtonPress) {
        //fprintf(stderr,"Mouse_button_handler, button pressed %d\n", event->button);
    }

    if (event->type == ButtonRelease) {
        //fprintf(stderr,"Mouse_button_handler, button released %d\n", event->button);
        return;
    }

#ifdef SWAP_MOUSE_BUTTONS
    if (event->button != Button1) {
        //fprintf(stderr,"Pressed a mouse button, but not Button1: %x\n",event->button);
#else   // SWAP_MOUSE_BUTTONS
    if (event->button != Button3) {
        //fprintf(stderr,"Pressed a mouse button, but not Button3: %x\n",event->button);
#endif  // SWAP_MOUSE_BUTTONS
        return;
    }

    // Right mouse button press
    menu_x=input_x;
    menu_y=input_y;
    if (right_menu_popup != NULL) { // If popup menu defined

#ifdef SWAP_MOUSE_BUTTONS
        // This gets the menus out of the way that are on pointer
        // button1 if SWAP_MOUSE_BUTTONS is enabled.  If it's not
        // enabled, they don't interfere with each other anyway.
        if (!measuring_distance && !moving_object) {
#else   // SWAP_MOUSE_BUTTONS
        if (1) {    // Always bring up the menu if SWAP is disabled
#endif  // SWAP_MOUSE_BUTTONS

            // Bring up the popup menu
            XmMenuPosition(right_menu_popup,(XButtonPressedEvent *)event);
            XtManageChild(right_menu_popup);

            // Check whether any modifiers are pressed.
            // If so, pop up a warning message.
            if ( (event->state != 0) && warn_about_mouse_modifiers) {
                popup_message_always(langcode("POPUPMA023"),langcode("POPUPMA024"));
            }
        }
    }
}





void menu_link_for_mouse_menu(Widget w, XtPointer clientData, XtPointer callData) {
    if (right_menu_popup!=NULL) {
        //XmMenuPosition(right_menu_popup,(XButtonPressedEvent *)event);
        XtManageChild(right_menu_popup);
    }
}





void store_all_kml_callback(/*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    export_trail_as_kml(NULL);
    last_kmlsnapshot = sec_now();
}




void KML_Snapshots_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    // Whether we're setting or unsetting it, set the timer such
    // that a snapshot will occur immediately once the button is set
    // again.
    last_kmlsnapshot = 0;

    if(state->set)
        kmlsnapshots_enabled = atoi(which);
    else
        kmlsnapshots_enabled = 0;
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





inline int no_data_selected(void)
{
    return (
        Select_.none || (
            !Select_.mine &&
            !Select_.net  && (
                !Select_.tnc || (
                    !Select_.direct && !Select_.via_digi
                )
            )
        )
    );
}





void create_appshell( /*@unused@*/ Display *display, char *app_name, /*@unused@*/ int app_argc, char ** app_argv) {
    Pixmap icon_pixmap;
    Atom WM_DELETE_WINDOW;
    Widget children[9];         /* Children to manage */
    Arg al[100];                 /* Arg List */
    register unsigned int ac;   /* Arg Count */
    /*popup menu widgets */
    Widget zoom_in, zoom_out, zoom_sub, zoom_level, zl1, zl2, zl3,
        zl4, zl5, zl6, zl7, zl8, zl9, zlC;
    Widget sar_object_menu;
    Widget CAD_sub, CAD1, CAD3, CAD4;
    Widget pan_sub, pan_menu;
    Widget move_my_sub, move_my_menu;
    Widget pan_ctr, last_loc, station_info, send_message_to;
    Widget set_object, modify_object;
    Widget setmyposition, pan_up, pan_down, pan_left, pan_right;
    /*menu widgets */
    Widget sep;
    Widget filepane, configpane, exitpane, mappane, viewpane,
        stationspane, messagepane, ifacepane, helppane,
        filter_data_pane, filter_display_pane, map_config_pane,
        station_config_pane,
        help_emergency_pane, help_emergency_button;

    Widget display_button,
        track_button, download_trail_button, station_clear_button,
        tracks_clear_button, object_history_refresh_button,
        object_history_clear_button, tactical_clear_button,
        tactical_history_clear_button, uptime_button, aloha_button,
        save_button,
        file_button, open_file_button, exit_button,
        view_button, view_messages_button, gps_status_button,
        bullet_button, packet_data_button, mobile_button,
        stations_button, localstations_button, laststations_button,
        objectstations_button, objectmystations_button,
        weather_button, wx_station_button, locate_button, geocode_place_button,
        locate_place_button, jump_button, jump_button2, alert_button,
        config_button, defaults_button, timing_button,
        coordinates_button, station_button, map_disable_button,
        map_button, map_auto_button, map_chooser_button,
        map_grid_button, map_levels_button, map_labels_button,
        map_fill_button, coordinate_calculator_button,
        center_zoom_button,
        Map_background_color_Pane, map_background_button,
        map_pointer_menu_button, map_config_button,
        station_config_button,
        cad_draw_button, cad_show_label_button,
        cad_show_probability_button, cad_show_area_button,
        cad_show_comment_button,
#if !defined(NO_GRAPHICS)
        Raster_intensity_Pane, raster_intensity_button,
#if defined(HAVE_MAGICK)
        gamma_adjust_button, tiger_config_button,
#endif  // HAVE_MAGICK
#ifdef HAVE_LIBGEOTIFF
        drg_config_button,
#endif  // HAVE_LIBGEOTIFF
#endif  // NO_GRAPHICS
        font_button,
        Map_station_label_Pane, map_station_label_button,
        Map_icon_outline_Pane, map_icon_outline_button,
        map_wx_alerts_button, index_maps_on_startup_button,
        redownload_maps_button, flush_map_cache_button,
        units_choice_button, dbstatus_choice_button,
        iface_button, iface_connect_button,
        tnc_logging, transmit_disable_toggle, net_logging,
        igate_logging, wx_logging, message_logging,
        wx_alert_logging, enable_snapshots, print_button,
        test_button, debug_level_button, aa_button, speech_button,
        smart_beacon_button, map_indexer_button,
        map_all_indexer_button, auto_msg_set_button,
        message_button, send_message_to_button,
        show_pending_messages_button, enable_kmlsnapshots,
        open_messages_group_button, clear_messages_button,
        General_q_button, IGate_q_button, WX_q_button,
        filter_data_button, filter_display_button,
        draw_CAD_objects_menu,
        store_data_pane, store_data_button, store_all_kml_button,
#ifdef HAVE_DB        
        store_all_db_button,
#endif  // HAVE_DB
#ifdef ARROWS
        pan_up_menu, pan_down_menu, pan_left_menu, pan_right_menu,
        zoom_in_menu, zoom_out_menu, 
#endif // ARROWS
        help_button, help_about, help_help;

    char *title, *t;
    int t_size;
//    XWMHints *wm_hints; // Used for window manager hints
    Dimension my_appshell_width, my_appshell_height;
    Dimension da_width, da_height;


    if(debug_level & 8)
        fprintf(stderr,"Create appshell start\n");

/*
    wm_hints = XAllocWMHints();
    if (!wm_hints) {
        fprintf(stderr,"Failure allocating memory: wm_hints\n");
        exit(0);
    }

    // Set up the wm_hints struct
    wm_hints->initial_state = NormalState;
    wm_hints->input = True;
    wm_hints->flags = StateHint | InputHint;
*/


    t = _("X Amateur Station Tracking and Information Reporting");
    title = (char *)malloc(t_size = (strlen(t) + 42 + strlen(xastir_package)));
    if (!title) {
        fprintf(stderr,"Couldn't allocate memory for title\n");
    }
    else {
        xastir_snprintf(title, t_size, "XASTIR");
        strncat(title, " - ", t_size - strlen(title));
        strncat(title, t, t_size - strlen(title));
        strncat(title, " @ ", t_size - strlen(title));
        (void)gethostname(&title[strlen(title)], 28);
    }

    // Allocate a couple of colors that we'll need before we get
    // around to calling create_gc(), which creates the rest.
    //
    colors[0x08] = GetPixelByName(appshell,"black");
    colors[0x0c] = GetPixelByName(appshell,"red");
    colors[0xff] = GetPixelByName(appshell,"gray73");


    ac = 0;



// Snag border widths so that we can use them in the calculations
// below.  If we fail to do this the size and offsets will be off by
// the width of the borders added by the window manager.
//
// if (XGetGeometry(XtDisplay(da),
//     XtWindow(appshell),
//     &root_return,
//     &x_return,
//     &y_return,
//     &width_return,
//     &height_return,
//     &border_width_return,
//     &depth_return) ) == False) {
//     fprintf(stderr,"Couldn't get window attributes\n");
// }
//
// Another method:
//
// XWindowAttributes windowattr; // Defined in Xlib.h
// Struct has x/y/width/height/border_width/depth fields.
// if (XGetWindowAttributes(display,XtWindow(appshell),&windowattr) == 0) {
//     fprintf(stderr,"Couldn't get window attributes\n")
// }



    // Set up the main window X/Y sizes and the minimum sizes
    // allowable.
    //
    if ( (WidthValue|HeightValue) & geometry_flags ) {
        //
        // Size of Xastir was specified with a -geometry setting.
        // Set up the window size.
        //

        my_appshell_width = (Dimension)geometry_width; // Used in offset equations below
        my_appshell_height = (Dimension)geometry_height; // Used in offset equations below
//fprintf(stderr,"gW:%d  gH:%d\n", geometry_width, geometry_height);
//fprintf(stderr,"tW:%d  tH:%d\n", (int)my_appshell_width, (int)my_appshell_height);
        if (my_appshell_width < 61)
            my_appshell_width = 61;
        if (my_appshell_height < 61)
            my_appshell_height = 61;
//fprintf(stderr,"tW:%d  tH:%d\n", (int)my_appshell_width, (int)my_appshell_height);
        XtSetArg(al[ac], XmNwidth,  my_appshell_width);    ac++;
        XtSetArg(al[ac], XmNheight, my_appshell_height);   ac++;
//        XtSetArg(al[ac], XmNminWidth,         61);             ac++;
//        XtSetArg(al[ac], XmNminHeight,        61);             ac++;
// Lock the min size to the specified initial size for now, release
// later after creating initial window.  Snagged this idea from the
// Lincity project where they do similar things in "lcx11.c"
//        XtSetArg(al[ac], XmNminWidth,  my_appshell_width);  ac++;
//        XtSetArg(al[ac], XmNminHeight, my_appshell_height); ac++;
    }
    else {
        // Size was NOT specified in a -geometry string.  Set to the
        // size specified in the config file instead.
        //
        my_appshell_width = (Dimension)screen_width;
        my_appshell_height = (Dimension)(screen_height + 60);
        XtSetArg(al[ac], XmNwidth,  my_appshell_width);  ac++;
        XtSetArg(al[ac], XmNheight, my_appshell_height); ac++;
//        XtSetArg(al[ac], XmNminWidth,         61);             ac++;
//        XtSetArg(al[ac], XmNminHeight,        61);             ac++;
// Lock the min size to the specified initial size for now, release
// later after creating initial window.  Got this idea from the
// Lincity project where they do the similar things in "lcx11.c"
//        XtSetArg(al[ac], XmNminWidth,  my_appshell_width);  ac++;
//        XtSetArg(al[ac], XmNminHeight, my_appshell_height); ac++;
    }


// Set up default font
    font1 = XLoadQueryFont(display, rotated_label_fontname[FONT_SYSTEM]);

    if (font1 == NULL) {    // Couldn't get the font!!!
        fprintf(stderr,"create_appshell: Couldn't load system font %s.  ",
            rotated_label_fontname[FONT_SYSTEM]);
        fprintf(stderr,"Loading default system font instead.\n");
        font1 = XLoadQueryFont(display, "fixed");
        if (font1 == NULL) {    // Couldn't get the font!!!
            fprintf(stderr,"create_appshell: Couldn't load default system font, exiting.\n");
            exit(1);
        }
        else {
            // _Now_ we can do a popup message about the first error
            // as we have a font to work with!
            char tempy[100];

            xastir_snprintf(tempy,
                sizeof(tempy),
                "Couldn't get font %s.  Loading default font instead.",
                rotated_label_fontname[FONT_SYSTEM]);
            popup_message_always(langcode("POPEM00035"), tempy);
        }
    }

    fontlist1 = XmFontListCreate(font1, "chset1");
    XtSetArg(al[ac], XmNfontList, fontlist1); ac++;


    // Set up the X/Y offsets for the main window
    //
    if ( (XValue|YValue) & geometry_flags ) {
        Position my_x, my_y;

        //
        // Position of Xastir was specified with a -geometry setting.
        // 
        if (XNegative & geometry_flags) {
            geometry_x = DisplayWidth(display, DefaultScreen(display) )
                + geometry_x - (int)my_appshell_width;
        }
        if (YNegative & geometry_flags) {
            geometry_y = DisplayHeight(display, DefaultScreen(display) )
                + geometry_y - (int)my_appshell_height;
        }
        my_x = (Position)geometry_x;
        my_y = (Position)geometry_y;
        XtSetArg(al[ac], XmNx, my_x); ac++;
        XtSetArg(al[ac], XmNy, my_y); ac++;
    }
    else {
        //
        // Position of Xastir was not specified.  Use the values
        // from the config file
        //
/*
// This doesn't position the widget in fvwm2.  Would hate to go back
// to XSizeHints in order to make this work.
fprintf(stderr,"Setting up widget's X/Y position at X:%d  Y:%d\n",
    (int)screen_x_offset,
    (int)screen_y_offset);

        XtSetArg(al[ac], XmNx, screen_x_offset); ac++;  // Doesn't work here
        XtSetArg(al[ac], XmNy, screen_y_offset); ac++;  // Doesn't work here
*/
    }


    XtSetArg(al[ac], XmNallowShellResize, TRUE);            ac++;

    if (title)
        XtSetArg(al[ac], XmNtitle,        title);           ac++;
 
    XtSetArg(al[ac], XmNdefaultPosition,  FALSE);           ac++;
    XtSetArg(al[ac], XmNfontList,         fontlist1);       ac++;
    XtSetArg(al[ac], XmNforeground,       MY_FG_COLOR);     ac++;
    XtSetArg(al[ac], XmNbackground,       MY_BG_COLOR);     ac++;
    //
    // Set the above values into the appshell widget
    //
    XtSetValues(appshell, al, ac);


    // Make at least one Motif call so that the next function won't
    // result in this problem:  'Error: atttempt to add non-widget
    // child "DropSiteManager" to parent "xastir"'.
    //
    (void) XmIsMotifWMRunning(appshell);


    form = XtVaCreateWidget("create_appshell form",
            xmFormWidgetClass,
            appshell,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);


    /* Menu Bar */
    ac = 0;
    XtSetArg(al[ac], XmNshadowThickness, 1);                     ac++;
    XtSetArg(al[ac], XmNalignment,       XmALIGNMENT_BEGINNING); ac++;
    XtSetArg(al[ac], XmNleftAttachment,  XmATTACH_FORM);         ac++;
    XtSetArg(al[ac], XmNtopAttachment,   XmATTACH_FORM);         ac++;
    XtSetArg(al[ac], XmNrightAttachment, XmATTACH_NONE);         ac++;
    XtSetArg(al[ac], XmNbottomAttachment,XmATTACH_NONE);         ac++;
    XtSetArg(al[ac], XmNforeground,      MY_FG_COLOR);           ac++;
    XtSetArg(al[ac], XmNbackground,      MY_BG_COLOR);           ac++;
    XtSetArg(al[ac], XmNfontList,        fontlist1);             ac++;


    menubar = XmCreateMenuBar(form,
            "create_appshell menubar",
            al,
            ac);

    /*set args for color */
    ac = 0;
    XtSetArg(al[ac], XmNforeground,   MY_FG_COLOR);        ac++;
    XtSetArg(al[ac], XmNbackground,   MY_BG_COLOR);        ac++;
    XtSetArg(al[ac], XmNtearOffModel, XmTEAR_OFF_ENABLED); ac++;
    XtSetArg(al[ac], XmNfontList,     fontlist1);          ac++;


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
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    view_button = XtVaCreateManagedWidget(langcode("MENUTB0002"),
            xmCascadeButtonGadgetClass,
            menubar,
            XmNsubMenuId,viewpane,
            XmNmnemonic,langcode_hotkey("MENUTB0002"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    map_button = XtVaCreateManagedWidget(langcode("MENUTB0004"),
            xmCascadeButtonGadgetClass,
            menubar,
            XmNsubMenuId,mappane,
            XmNmnemonic,langcode_hotkey("MENUTB0004"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    display_button = XtVaCreateManagedWidget(langcode("MENUTB0005"),
            xmCascadeButtonGadgetClass,
            menubar,
            XmNsubMenuId,stationspane,
            XmNmnemonic,langcode_hotkey("MENUTB0005"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    message_button = XtVaCreateManagedWidget(langcode("MENUTB0006"),
            xmCascadeButtonGadgetClass,
            menubar,
            XmNsubMenuId,messagepane,
            XmNmnemonic,langcode_hotkey("MENUTB0006"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    iface_button = XtVaCreateManagedWidget(langcode("MENUTB0010"),
            xmCascadeButtonGadgetClass,
            menubar,
            XmNsubMenuId,ifacepane,
            XmNmnemonic,langcode_hotkey("MENUTB0010"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    help_button = XtVaCreateManagedWidget(langcode("MENUTB0009"),
            xmCascadeButtonGadgetClass,
            menubar,
            XmNsubMenuId,helppane,
            XmNmnemonic,langcode_hotkey("MENUTB0009"),
            XmNfontList, fontlist1,
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
    XtSetArg(al[ac], XmNfontList,     fontlist1);          ac++;


    configpane  = XmCreatePulldownMenu(filepane,
            "configpane",
            al,
            ac);

    // Print button
    print_button = XtVaCreateManagedWidget(langcode("PULDNFI015"),
            xmPushButtonWidgetClass,
            filepane,
            XmNmnemonic, langcode_hotkey("PULDNFI015"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback( print_button, XmNactivateCallback, Print_Postscript, NULL );

    config_button = XtVaCreateManagedWidget(langcode("PULDNFI001"),
            xmCascadeButtonGadgetClass,
            filepane,
            XmNsubMenuId,configpane,
            XmNmnemonic,langcode_hotkey("PULDNFI001"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

   (void)XtVaCreateManagedWidget("create_appshell sep1",
            xmSeparatorGadgetClass,
            filepane,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    open_file_button = XtVaCreateManagedWidget(langcode("PULDNFI002"),
            xmPushButtonGadgetClass,
            filepane,
            XmNmnemonic,langcode_hotkey("PULDNFI002"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);


    tnc_logging = XtVaCreateManagedWidget(langcode("PULDNFI010"),
            xmToggleButtonGadgetClass,
            filepane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
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
            XmNfontList, fontlist1,
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
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(igate_logging,XmNvalueChangedCallback,IGate_Logging_toggle,"1");
    if (log_igate)
        XmToggleButtonSetState(igate_logging,TRUE,FALSE);

//    message_logging = XtVaCreateManagedWidget(langcode("PULDNFI012"),
    message_logging = XtVaCreateManagedWidget("Message Logging",
            xmToggleButtonGadgetClass,
            filepane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(message_logging,XmNvalueChangedCallback,Message_Logging_toggle,"1");
    if (log_message_data)
        XmToggleButtonSetState(message_logging,TRUE,FALSE);

    wx_logging = XtVaCreateManagedWidget(langcode("PULDNFI013"),
            xmToggleButtonGadgetClass,
            filepane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(wx_logging,XmNvalueChangedCallback,WX_Logging_toggle,"1");
    if (log_wx)
        XmToggleButtonSetState(wx_logging,TRUE,FALSE);

//    wx_alert_logging = XtVaCreateManagedWidget(langcode("PULDNFI013"),
    wx_alert_logging = XtVaCreateManagedWidget("WX Alert Logging",
            xmToggleButtonGadgetClass,
            filepane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(wx_alert_logging,XmNvalueChangedCallback,WX_Alert_Logging_toggle,"1");
    if (log_wx_alert_data)
        XmToggleButtonSetState(wx_alert_logging,TRUE,FALSE);

    enable_snapshots = XtVaCreateManagedWidget(langcode("PULDNFI014"),
            xmToggleButtonGadgetClass,
            filepane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(enable_snapshots,XmNvalueChangedCallback,Snapshots_toggle,"1");
    if (snapshots_enabled)
        XmToggleButtonSetState(enable_snapshots,TRUE,FALSE);

    // enable kml snapshots
    enable_kmlsnapshots = XtVaCreateManagedWidget(langcode("PULDNFI016"),
            xmToggleButtonGadgetClass,
            filepane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(enable_kmlsnapshots,XmNvalueChangedCallback,KML_Snapshots_toggle,"1");
    if (kmlsnapshots_enabled)
        XmToggleButtonSetState(enable_kmlsnapshots,TRUE,FALSE);


    (void)XtVaCreateManagedWidget("create_appshell sep1a",
            xmSeparatorGadgetClass,
            filepane,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);


    (void)XtVaCreateManagedWidget("create_appshell sep1b",
            xmSeparatorGadgetClass,
            filepane,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);


    exitpane  = XmCreatePulldownMenu(filepane,
            "exitpane",
            al,
            ac);

    exit_button = XtVaCreateManagedWidget(langcode("PULDNFI004"),
            xmPushButtonWidgetClass,
            filepane,
            XmNmnemonic,langcode_hotkey("PULDNFI004"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    /* View */
    packet_data_button = XtVaCreateManagedWidget(langcode("PULDNVI002"),
            xmPushButtonGadgetClass,
            viewpane,
            XmNmnemonic,langcode_hotkey("PULDNVI002"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    view_messages_button = XtVaCreateManagedWidget(langcode("PULDNVI011"),
            xmPushButtonGadgetClass,
            viewpane,
            XmNmnemonic,langcode_hotkey("PULDNVI011"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    bullet_button = XtVaCreateManagedWidget(langcode("PULDNVI001"),
            xmPushButtonGadgetClass,
            viewpane,
            XmNmnemonic,langcode_hotkey("PULDNVI001"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
 
    (void)XtVaCreateManagedWidget("create_appshell sep?",
            xmSeparatorGadgetClass,
            viewpane,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    mobile_button = XtVaCreateManagedWidget(langcode("PULDNVI003"),
            xmPushButtonGadgetClass,
            viewpane,
            XmNmnemonic,langcode_hotkey("PULDNVI003"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    stations_button = XtVaCreateManagedWidget(langcode("PULDNVI004"),
            xmPushButtonGadgetClass,
            viewpane,
            XmNmnemonic,langcode_hotkey("PULDNVI004"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    localstations_button = XtVaCreateManagedWidget(langcode("PULDNVI009"),
            xmPushButtonGadgetClass,
            viewpane,
            XmNmnemonic,langcode_hotkey("PULDNVI009"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    laststations_button = XtVaCreateManagedWidget(langcode("PULDNVI012"),
            xmPushButtonGadgetClass,
            viewpane,
            XmNmnemonic,langcode_hotkey("PULDNVI012"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    (void)XtVaCreateManagedWidget("create_appshell sep1?",
            xmSeparatorGadgetClass,
            viewpane,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    objectstations_button = XtVaCreateManagedWidget(langcode("LHPUPNI005"),
            xmPushButtonGadgetClass,
            viewpane,
            XmNmnemonic,langcode_hotkey("LHPUPNI005"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    objectmystations_button = XtVaCreateManagedWidget(langcode("LHPUPNI006"),
            xmPushButtonGadgetClass,
            viewpane,
            XmNmnemonic,langcode_hotkey("LHPUPNI006"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    // "List All CAD Polygons"
    CAD1 = XtVaCreateManagedWidget(langcode("POPUPMA046"),
            xmPushButtonGadgetClass,
            viewpane,
            XmNmnemonic,langcode_hotkey("POPUPMA046"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    (void)XtVaCreateManagedWidget("create_appshell sep2?",
            xmSeparatorGadgetClass,
            viewpane,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    weather_button = XtVaCreateManagedWidget(langcode("PULDNVI005"),
            xmPushButtonGadgetClass,
            viewpane,
            XmNmnemonic,langcode_hotkey("PULDNVI005"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    wx_station_button = XtVaCreateManagedWidget(langcode("PULDNVI008"),
            xmPushButtonGadgetClass,
            viewpane,
            XmNmnemonic,langcode_hotkey("PULDNVI008"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    alert_button = XtVaCreateManagedWidget(langcode("PULDNVI007"),
            xmPushButtonGadgetClass,
            viewpane,
            XmNmnemonic,langcode_hotkey("PULDNVI007"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    (void)XtVaCreateManagedWidget("create_appshell sep3?",
            xmSeparatorGadgetClass,
            viewpane,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    gps_status_button = XtVaCreateManagedWidget(langcode("PULDNVI015"),
            xmPushButtonGadgetClass,
            viewpane,
            XmNmnemonic,langcode_hotkey("PULDNVI015"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    uptime_button = XtVaCreateManagedWidget(langcode("PULDNVI013"),
            xmPushButtonWidgetClass,
            viewpane,
            XmNmnemonic, langcode_hotkey("PULDNVI013"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    aloha_button = XtVaCreateManagedWidget(langcode("PULDNVI016"),
            xmPushButtonWidgetClass,
            viewpane,
            XmNmnemonic, langcode_hotkey("PULDNVI016"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    /* Configure */
    station_button = XtVaCreateManagedWidget(langcode("PULDNCF004"),
            xmPushButtonGadgetClass,
            configpane,
            XmNmnemonic,langcode_hotkey("PULDNCF004"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    defaults_button = XtVaCreateManagedWidget(langcode("PULDNCF001"),
            xmPushButtonGadgetClass,
            configpane,
            XmNmnemonic,langcode_hotkey("PULDNCF001"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    timing_button = XtVaCreateManagedWidget(langcode("PULDNCF003"),
            xmPushButtonGadgetClass,
            configpane,
            XmNmnemonic,langcode_hotkey("PULDNCF003"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    coordinates_button = XtVaCreateManagedWidget(langcode("PULDNCF002"),
            xmPushButtonGadgetClass,
            configpane,
            XmNmnemonic,langcode_hotkey("PULDNCF002"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    aa_button = XtVaCreateManagedWidget(langcode("PULDNCF006"),
            xmPushButtonGadgetClass,
            configpane,
            XmNmnemonic,langcode_hotkey("PULDNCF006"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    speech_button = XtVaCreateManagedWidget(langcode("PULDNCF007"),
            xmPushButtonGadgetClass,
            configpane,
            XmNmnemonic,langcode_hotkey("PULDNCF007"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    smart_beacon_button = XtVaCreateManagedWidget(langcode("SMARTB001"),
            xmPushButtonGadgetClass,
            configpane,
            XmNmnemonic,langcode_hotkey("SMARTB001"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    // map label font select
    font_button = XtVaCreateManagedWidget(langcode("PULDNMP025"),
            xmPushButtonWidgetClass, configpane,
            XmNmnemonic,langcode_hotkey("PULDNMP025"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(font_button, XmNactivateCallback, Map_font, NULL);

    test_button = XtVaCreateManagedWidget(langcode("PULDNFI003"),
            xmPushButtonWidgetClass,
            configpane,
            XmNmnemonic, langcode_hotkey("PULDNFI003"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    debug_level_button = XtVaCreateManagedWidget(langcode("PULDNFI007"),
            xmPushButtonWidgetClass,
            configpane,
            XmNmnemonic, langcode_hotkey("PULDNFI007"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    units_choice_button = XtVaCreateManagedWidget(langcode("PULDNUT001"),
            xmToggleButtonGadgetClass,
            configpane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(units_choice_button,XmNvalueChangedCallback,Units_choice_toggle,"1");
    if (english_units)
        XmToggleButtonSetState(units_choice_button,TRUE,FALSE);

    dbstatus_choice_button = XtVaCreateManagedWidget(langcode("PULDNDB001"),
            xmToggleButtonGadgetClass,
            configpane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(dbstatus_choice_button,XmNvalueChangedCallback,Dbstatus_choice_toggle,"1");
    if (do_dbstatus)
        XmToggleButtonSetState(dbstatus_choice_button,TRUE,FALSE);



    (void)XtVaCreateManagedWidget("create_appshell sep1d",
            xmSeparatorGadgetClass,
            configpane,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    save_button = XtVaCreateManagedWidget(langcode("PULDNCF008"),
            xmPushButtonGadgetClass,
            configpane,
            XmNmnemonic, langcode_hotkey("PULDNCF008"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);



//- Maps -------------------------------------------------------------

    map_chooser_button = XtVaCreateManagedWidget(langcode("PULDNMP001"),
            xmPushButtonGadgetClass,
            mappane,
            XmNmnemonic,langcode_hotkey("PULDNMP001"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(map_chooser_button,   XmNactivateCallback,Map_chooser,NULL);

    // Map Display Bookmarks
    jump_button = XtVaCreateManagedWidget(langcode("PULDNMP012"),
            xmPushButtonGadgetClass,
            mappane,
            XmNmnemonic,langcode_hotkey("PULDNMP012"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    locate_place_button = XtVaCreateManagedWidget(langcode("PULDNMP014"),
            xmPushButtonGadgetClass,
            mappane,
            XmNmnemonic,langcode_hotkey("PULDNMP014"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    geocode_place_button = XtVaCreateManagedWidget(langcode("PULDNMP029"),
            xmPushButtonGadgetClass,
            mappane,
            XmNmnemonic,langcode_hotkey("PULDNMP029"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    coordinate_calculator_button = XtVaCreateManagedWidget(langcode("COORD001"),
            xmPushButtonGadgetClass,mappane,
            XmNmnemonic, langcode_hotkey("COORD001"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    center_zoom_button=XtVaCreateManagedWidget(langcode("POPUPMA026"),
            xmPushButtonGadgetClass, mappane,
            XmNmnemonic, langcode_hotkey("POPUPMA026"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(center_zoom_button,XmNactivateCallback,Center_Zoom,NULL);

    ac = 0;
    XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
    XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
    XtSetArg(al[ac], XmNnavigationType, XmTAB_GROUP); ac++;
    XtSetArg(al[ac], XmNtraversalOn, TRUE); ac++;
    XtSetArg(al[ac], XmNtearOffModel, XmTEAR_OFF_ENABLED); ac++;
    XtSetArg(al[ac], XmNfontList,        fontlist1);             ac++;


    CAD_sub=XmCreatePulldownMenu(mappane,
            "create_appshell CAD sub",
            al,
            ac);

    // "Draw CAD Objects"
    draw_CAD_objects_menu=XtVaCreateManagedWidget(langcode("POPUPMA029"),
            xmCascadeButtonGadgetClass,
            mappane,
            XmNsubMenuId,CAD_sub,
//            XmNmnemonic,langcode_hotkey("POPUPMA029"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    // "Draw Mode"
    ac = 0;
    XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
    XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
    XtSetArg(al[ac], XmNnavigationType, XmTAB_GROUP); ac++;
    XtSetArg(al[ac], XmNtraversalOn, TRUE); ac++;
    XtSetArg(al[ac], XmNfontList,        fontlist1);             ac++;


//    XtSetArg(al[ac], XmNmnemonic, langcode_hotkey("POPUPMA031")); ac++;

    // "Close Polygon"
    CAD_close_polygon_menu_item=XtCreateManagedWidget(langcode("POPUPMA031"),
            xmPushButtonGadgetClass,
            CAD_sub,
            al,
            ac);
    XtAddCallback(CAD_close_polygon_menu_item,XmNactivateCallback,Draw_CAD_Objects_close_polygon,NULL);
    // disable the close polygon menu item if not in draw mode
    if (draw_CAD_objects_flag==1)
        XtSetSensitive(CAD_close_polygon_menu_item,TRUE);
    if (draw_CAD_objects_flag==0)
        XtSetSensitive(CAD_close_polygon_menu_item,FALSE);

    ac = 0;
    XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
    XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
    XtSetArg(al[ac], XmNnavigationType, XmTAB_GROUP); ac++;
    XtSetArg(al[ac], XmNtraversalOn, TRUE); ac++;
//    XtSetArg(al[ac], XmNmnemonic, langcode_hotkey("POPUPMA032")); ac++;
    XtSetArg(al[ac], XmNfontList,        fontlist1);             ac++;


    // "Erase CAD Polygons"
    CAD3=XtCreateManagedWidget(langcode("POPUPMA032"),
            xmPushButtonGadgetClass,
            CAD_sub,
            al,
            ac);
    XtAddCallback(CAD3,XmNactivateCallback,Draw_CAD_Objects_erase_dialog,NULL);

    // "List All CAD Polygons"
    CAD4 = XtVaCreateManagedWidget(langcode("POPUPMA046"),
            xmPushButtonGadgetClass,
            CAD_sub,
            XmNmnemonic,langcode_hotkey("POPUPMA046"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(CAD4,XmNactivateCallback,Draw_CAD_Objects_list_dialog,NULL);
   
    // Toggles for CAD object information display on map
    // Draw CAD Objects 
    cad_draw_button = XtVaCreateManagedWidget(langcode("POPUPMA047"),
            xmToggleButtonGadgetClass,
            CAD_sub,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(cad_draw_button,XmNvalueChangedCallback,CAD_draw_toggle,"CAD_draw_objects");
    if (CAD_draw_objects==TRUE)
        XmToggleButtonSetState(cad_draw_button,TRUE,FALSE);

    // Draw CAD Labels 
    cad_show_label_button = XtVaCreateManagedWidget(langcode("POPUPMA048"),
            xmToggleButtonGadgetClass,
            CAD_sub,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(cad_show_label_button,XmNvalueChangedCallback,CAD_draw_toggle,"CAD_show_label");
    if (CAD_show_label==TRUE)
        XmToggleButtonSetState(cad_show_label_button,TRUE,FALSE);

    // Draw CAD Probability
    cad_show_probability_button = XtVaCreateManagedWidget(langcode("POPUPMA050"),
            xmToggleButtonGadgetClass,
            CAD_sub,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(cad_show_probability_button,XmNvalueChangedCallback,CAD_draw_toggle,"CAD_show_raw_probability");
    if (CAD_show_raw_probability==TRUE)
        XmToggleButtonSetState(cad_show_probability_button,TRUE,FALSE);

    // Draw CAD Comments
    cad_show_comment_button = XtVaCreateManagedWidget(langcode("POPUPMA049"),
            xmToggleButtonGadgetClass,
            CAD_sub,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(cad_show_comment_button,XmNvalueChangedCallback,CAD_draw_toggle,"CAD_show_comment");
    if (CAD_show_comment==TRUE)
        XmToggleButtonSetState(cad_show_comment_button,TRUE,FALSE);

    // Draw CAD Size of Area
    cad_show_area_button = XtVaCreateManagedWidget(langcode("POPUPMA051"),
            xmToggleButtonGadgetClass,
            CAD_sub,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(cad_show_area_button,XmNvalueChangedCallback,CAD_draw_toggle,"CAD_show_area");
    if (CAD_show_area==TRUE)
        XmToggleButtonSetState(cad_show_area_button,TRUE,FALSE);

    (void)XtVaCreateManagedWidget("create_appshell sep2",
            xmSeparatorGadgetClass,
            mappane,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    
    map_disable_button = XtVaCreateManagedWidget(langcode("PULDNMP013"),
            xmToggleButtonGadgetClass,
            mappane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
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
            XmNfontList, fontlist1,
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
            XmNfontList, fontlist1,
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
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(map_grid_button,XmNvalueChangedCallback,Grid_toggle,"1");
    if (long_lat_grid)
        XmToggleButtonSetState(map_grid_button,TRUE,FALSE);

    // Enable Map Border
    map_border_button = XtVaCreateManagedWidget(langcode("PULDNMP031"),
            xmToggleButtonGadgetClass,
            mappane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(map_border_button,XmNvalueChangedCallback,Map_border_toggle,"1");
    if (draw_labeled_grid_border)  
        XmToggleButtonSetState(map_border_button,TRUE,FALSE);
    if (!long_lat_grid) 
        XtSetSensitive(map_border_button,FALSE);
    else 
        XtSetSensitive(map_border_button,TRUE);


    map_levels_button = XtVaCreateManagedWidget(langcode("PULDNMP004"),
            xmToggleButtonGadgetClass,
            mappane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
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
            XmNfontList, fontlist1,
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
            XmNfontList, fontlist1,
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
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(map_wx_alerts_button,XmNvalueChangedCallback,Map_wx_alerts_toggle,"1");
    if (!wx_alert_style)
        XmToggleButtonSetState(map_wx_alerts_button,TRUE,FALSE);
#ifndef HAVE_LIBSHP
    // If we don't have Shapelib compiled in, grey-out the weather
    // alerts button.
    XtSetSensitive(map_wx_alerts_button, FALSE);
#endif  // HAVE_LIBSHP


    (void)XtVaCreateManagedWidget("create_appshell sep2b",
            xmSeparatorGadgetClass,
            mappane,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    (void)XtVaCreateManagedWidget("create_appshell sep2c",
            xmSeparatorGadgetClass,
            mappane,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    ac = 0;
    XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
    XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
    XtSetArg(al[ac], XmNnavigationType, XmTAB_GROUP); ac++;
    XtSetArg(al[ac], XmNtraversalOn, TRUE); ac++;
    XtSetArg(al[ac], XmNtearOffModel, XmTEAR_OFF_ENABLED); ac++;
    XtSetArg(al[ac], XmNfontList,        fontlist1);             ac++;


    map_config_pane  = XmCreatePulldownMenu(mappane,
            "map_config_pane",
            al,
            ac);

    map_config_button = XtVaCreateManagedWidget(langcode("PULDNFI001"),
            xmCascadeButtonGadgetClass,
            mappane,
            XmNsubMenuId,map_config_pane,
            XmNmnemonic,langcode_hotkey("PULDNFI001"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    ac = 0;
    XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
    XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
    XtSetArg(al[ac], XmNnavigationType, XmTAB_GROUP); ac++;
    XtSetArg(al[ac], XmNtraversalOn, TRUE); ac++;
    XtSetArg(al[ac], XmNtearOffModel, XmTEAR_OFF_ENABLED); ac++;
    XtSetArg(al[ac], XmNfontList,        fontlist1);             ac++;


    // These go into the map config submenu
    Map_background_color_Pane = XmCreatePulldownMenu(map_config_pane,
            "create_appshell map_background_color",
            al,
            ac);

    map_background_button = XtVaCreateManagedWidget(langcode("PULDNMP005"),
            xmCascadeButtonWidgetClass,
            map_config_pane,
            XmNsubMenuId, Map_background_color_Pane,
            XmNmnemonic, langcode_hotkey("PULDNMP005"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    map_bgcolor[10] = XtVaCreateManagedWidget(langcode("PULDNMBC11"),
            xmPushButtonGadgetClass,
            Map_background_color_Pane,
            XmNmnemonic,langcode_hotkey("PULDNMBC11"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    map_bgcolor[11] = XtVaCreateManagedWidget(langcode("PULDNMBC12"),
            xmPushButtonGadgetClass,
            Map_background_color_Pane,
            XmNmnemonic,langcode_hotkey("PULDNMBC12"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    map_bgcolor[0] = XtVaCreateManagedWidget(langcode("PULDNMBC01"),
            xmPushButtonGadgetClass,
            Map_background_color_Pane,
            XmNmnemonic,langcode_hotkey("PULDNMBC01"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    map_bgcolor[1] = XtVaCreateManagedWidget(langcode("PULDNMBC02"),
            xmPushButtonGadgetClass,
            Map_background_color_Pane,
            XmNmnemonic,langcode_hotkey("PULDNMBC02"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    map_bgcolor[2] = XtVaCreateManagedWidget(langcode("PULDNMBC03"),
            xmPushButtonGadgetClass,
            Map_background_color_Pane,
            XmNmnemonic,langcode_hotkey("PULDNMBC03"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    map_bgcolor[3] = XtVaCreateManagedWidget(langcode("PULDNMBC04"),
            xmPushButtonGadgetClass,
            Map_background_color_Pane,
            XmNmnemonic,langcode_hotkey("PULDNMBC04"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    map_bgcolor[4] = XtVaCreateManagedWidget(langcode("PULDNMBC05"),
            xmPushButtonGadgetClass,
            Map_background_color_Pane,
            XmNmnemonic,langcode_hotkey("PULDNMBC05"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    map_bgcolor[5] = XtVaCreateManagedWidget(langcode("PULDNMBC06"),
            xmPushButtonGadgetClass,
            Map_background_color_Pane,
            XmNmnemonic,langcode_hotkey("PULDNMBC06"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    map_bgcolor[6] = XtVaCreateManagedWidget(langcode("PULDNMBC07"),
            xmPushButtonGadgetClass,
            Map_background_color_Pane,
            XmNmnemonic,langcode_hotkey("PULDNMBC07"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    map_bgcolor[7] = XtVaCreateManagedWidget(langcode("PULDNMBC08"),
            xmPushButtonGadgetClass,
            Map_background_color_Pane,
            XmNmnemonic,langcode_hotkey("PULDNMBC08"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    map_bgcolor[8] = XtVaCreateManagedWidget(langcode("PULDNMBC09"),
            xmPushButtonGadgetClass,
            Map_background_color_Pane,
            XmNmnemonic,langcode_hotkey("PULDNMBC09"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    map_bgcolor[9] = XtVaCreateManagedWidget(langcode("PULDNMBC10"),
            xmPushButtonGadgetClass,
            Map_background_color_Pane,
            XmNmnemonic,langcode_hotkey("PULDNMBC10"),
            XmNfontList, fontlist1,
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

    ac = 0;
    XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
    XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
    XtSetArg(al[ac], XmNnavigationType, XmTAB_GROUP); ac++;
    XtSetArg(al[ac], XmNtraversalOn, TRUE); ac++;
    XtSetArg(al[ac], XmNtearOffModel, XmTEAR_OFF_ENABLED); ac++;
    XtSetArg(al[ac], XmNfontList,        fontlist1);             ac++;


    Raster_intensity_Pane = XmCreatePulldownMenu(map_config_pane,
            "create_appshell raster_intensity",
            al,
            ac);

    raster_intensity_button = XtVaCreateManagedWidget(langcode("PULDNMP008"),
            xmCascadeButtonWidgetClass,
            map_config_pane,
            XmNsubMenuId,
            Raster_intensity_Pane,
            XmNmnemonic, langcode_hotkey("PULDNMP008"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    raster_intensity[0] = XtVaCreateManagedWidget("0%",
            xmPushButtonGadgetClass,
            Raster_intensity_Pane,
            XmNmnemonic,"0%",
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    raster_intensity[1] = XtVaCreateManagedWidget("10%",
            xmPushButtonGadgetClass,
            Raster_intensity_Pane,
            XmNmnemonic,"10%",
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    raster_intensity[2] = XtVaCreateManagedWidget("20%",
            xmPushButtonGadgetClass,
            Raster_intensity_Pane,
            XmNmnemonic,"20%",
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    raster_intensity[3] = XtVaCreateManagedWidget("30%",
            xmPushButtonGadgetClass,
            Raster_intensity_Pane,
            XmNmnemonic,"30%",
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    raster_intensity[4] = XtVaCreateManagedWidget("40%",
            xmPushButtonGadgetClass,
            Raster_intensity_Pane,
            XmNmnemonic,"40%",
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    raster_intensity[5] = XtVaCreateManagedWidget("50%",
            xmPushButtonGadgetClass,
            Raster_intensity_Pane,
            XmNmnemonic,"50%",
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    raster_intensity[6] = XtVaCreateManagedWidget("60%",
            xmPushButtonGadgetClass,
            Raster_intensity_Pane,
            XmNmnemonic,"60%",
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    raster_intensity[7] = XtVaCreateManagedWidget("70%",
            xmPushButtonGadgetClass,
            Raster_intensity_Pane,
            XmNmnemonic,"70%",
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    raster_intensity[8] = XtVaCreateManagedWidget("80%",
            xmPushButtonGadgetClass,
            Raster_intensity_Pane,
            XmNmnemonic,"80%",
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    raster_intensity[9] = XtVaCreateManagedWidget("90%",
            xmPushButtonGadgetClass,
            Raster_intensity_Pane,
            XmNmnemonic,"90%",
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    raster_intensity[10] = XtVaCreateManagedWidget("100%",
            xmPushButtonGadgetClass,
            Raster_intensity_Pane,
            XmNmnemonic,"100%",
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtSetSensitive(raster_intensity[(int)(raster_map_intensity * 10.0)],FALSE);

    //fprintf(stderr,"raster index = %d\n",
    //    (int)(raster_map_intensity * 10.01) );

    XtAddCallback(raster_intensity[0],  XmNactivateCallback,Raster_intensity,"0.0");
    XtAddCallback(raster_intensity[1],  XmNactivateCallback,Raster_intensity,"0.1");
    XtAddCallback(raster_intensity[2],  XmNactivateCallback,Raster_intensity,"0.2");
    XtAddCallback(raster_intensity[3],  XmNactivateCallback,Raster_intensity,"0.3");
    XtAddCallback(raster_intensity[4],  XmNactivateCallback,Raster_intensity,"0.4");
    XtAddCallback(raster_intensity[5],  XmNactivateCallback,Raster_intensity,"0.5");
    XtAddCallback(raster_intensity[6],  XmNactivateCallback,Raster_intensity,"0.6");
    XtAddCallback(raster_intensity[7],  XmNactivateCallback,Raster_intensity,"0.7");
    XtAddCallback(raster_intensity[8],  XmNactivateCallback,Raster_intensity,"0.8");
    XtAddCallback(raster_intensity[9],  XmNactivateCallback,Raster_intensity,"0.9");
    XtAddCallback(raster_intensity[10], XmNactivateCallback,Raster_intensity,"1.0");
#if defined(HAVE_MAGICK)
    // Adjust Gamma Correction
    gamma_adjust_button = XtVaCreateManagedWidget(langcode("GAMMA001"),
            xmPushButtonWidgetClass, map_config_pane,
            XmNmnemonic,langcode_hotkey("GAMMA001"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(gamma_adjust_button, XmNactivateCallback, Gamma_adjust, NULL);
#endif  // HAVE_MAGICK
#endif  // NO_GRAPHICS

    ac = 0;
    XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
    XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
    XtSetArg(al[ac], XmNnavigationType, XmTAB_GROUP); ac++;
    XtSetArg(al[ac], XmNtraversalOn, TRUE); ac++;
    XtSetArg(al[ac], XmNtearOffModel, XmTEAR_OFF_ENABLED); ac++;
    XtSetArg(al[ac], XmNfontList,        fontlist1);             ac++;


    Map_station_label_Pane = XmCreatePulldownMenu(map_config_pane,
            "create_appshell map_station_label",
            al,
            ac);
    map_station_label_button = XtVaCreateManagedWidget(langcode("PULDNMP006"),
            xmCascadeButtonWidgetClass,
            map_config_pane,
            XmNsubMenuId, Map_station_label_Pane,
            XmNmnemonic,langcode_hotkey("PULDNMP006"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    map_station_label0 = XtVaCreateManagedWidget(langcode("PULDNMSL01"),
            xmPushButtonGadgetClass,
            Map_station_label_Pane,
            XmNmnemonic,langcode_hotkey("PULDNMSL01"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    map_station_label1 = XtVaCreateManagedWidget(langcode("PULDNMSL02"),
            xmPushButtonGadgetClass,
            Map_station_label_Pane,
            XmNmnemonic,langcode_hotkey("PULDNMSL02"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    map_station_label2 = XtVaCreateManagedWidget(langcode("PULDNMSL03"),
            xmPushButtonGadgetClass,
            Map_station_label_Pane,
            XmNmnemonic,langcode_hotkey("PULDNMSL03"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    sel3_switch(letter_style,map_station_label2,map_station_label1,map_station_label0);
    XtAddCallback(map_station_label0,   XmNactivateCallback,Map_station_label,"0");
    XtAddCallback(map_station_label1,   XmNactivateCallback,Map_station_label,"1");
    XtAddCallback(map_station_label2,   XmNactivateCallback,Map_station_label,"2");

    ac = 0;
    XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
    XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
    XtSetArg(al[ac], XmNnavigationType, XmTAB_GROUP); ac++;
    XtSetArg(al[ac], XmNtraversalOn, TRUE); ac++;
    XtSetArg(al[ac], XmNtearOffModel, XmTEAR_OFF_ENABLED); ac++;
    XtSetArg(al[ac], XmNfontList,        fontlist1);             ac++;


    Map_icon_outline_Pane = XmCreatePulldownMenu(map_config_pane,
            "create_appshell map_icon_outline",
            al,
            ac);
    map_icon_outline_button = XtVaCreateManagedWidget(langcode("PULDNMP026"),
            xmCascadeButtonWidgetClass,
            map_config_pane,
            XmNsubMenuId, Map_icon_outline_Pane,
            XmNmnemonic,langcode_hotkey("PULDNMP026"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    map_icon_outline0 = XtVaCreateManagedWidget(langcode("PULDNMIO01"),
            xmPushButtonGadgetClass,
            Map_icon_outline_Pane,
            XmNmnemonic,langcode_hotkey("PULDNMIO01"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    map_icon_outline1 = XtVaCreateManagedWidget(langcode("PULDNMIO02"),
            xmPushButtonGadgetClass,
            Map_icon_outline_Pane,
            XmNmnemonic,langcode_hotkey("PULDNMIO02"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    map_icon_outline2 = XtVaCreateManagedWidget(langcode("PULDNMIO03"),
            xmPushButtonGadgetClass,
            Map_icon_outline_Pane,
            XmNmnemonic,langcode_hotkey("PULDNMIO03"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    map_icon_outline3 = XtVaCreateManagedWidget(langcode("PULDNMIO04"),
            xmPushButtonGadgetClass,
            Map_icon_outline_Pane,
            XmNmnemonic,langcode_hotkey("PULDNMIO04"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    sel4_switch(icon_outline_style,map_icon_outline3,map_icon_outline2,map_icon_outline1,map_icon_outline0);
    XtAddCallback(map_icon_outline0,   XmNactivateCallback,Map_icon_outline,"0");
    XtAddCallback(map_icon_outline1,   XmNactivateCallback,Map_icon_outline,"1");
    XtAddCallback(map_icon_outline2,   XmNactivateCallback,Map_icon_outline,"2");
    XtAddCallback(map_icon_outline3,   XmNactivateCallback,Map_icon_outline,"3");


#if defined(HAVE_MAGICK)
    tiger_config_button= XtVaCreateManagedWidget(langcode("PULDNMP020"),
            xmPushButtonGadgetClass,
            map_config_pane,
            XmNmnemonic,langcode_hotkey("PULDNMP020"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(tiger_config_button,   XmNactivateCallback,Config_tiger,NULL);
#endif  // HAVE_MAGICK

#ifdef HAVE_LIBGEOTIFF
    drg_config_button= XtVaCreateManagedWidget(langcode("PULDNMP030"),
            xmPushButtonGadgetClass,
            map_config_pane,
            XmNmnemonic,langcode_hotkey("PULDNMP030"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(drg_config_button, XmNactivateCallback,Config_DRG,NULL);
#endif  // HAVE_LIBGEOTIFF


    (void)XtVaCreateManagedWidget("create_appshell sep2d",
            xmSeparatorGadgetClass,
            map_config_pane,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);


// Re-download Maps (Not from cache)
    redownload_maps_button = XtVaCreateManagedWidget(langcode("PULDNMP027"),
          xmPushButtonGadgetClass,
          map_config_pane,
          XmNmnemonic,langcode_hotkey("PULDNMP027"),
          XmNfontList, fontlist1,
          MY_FOREGROUND_COLOR,
          MY_BACKGROUND_COLOR,
          NULL);
    XtAddCallback(redownload_maps_button, XmNactivateCallback,Re_Download_Maps_Now,NULL);
 

// Flush Entire Map Cache!
    flush_map_cache_button = XtVaCreateManagedWidget(langcode("PULDNMP028"),
          xmPushButtonGadgetClass,
          map_config_pane,
          XmNmnemonic,langcode_hotkey("PULDNMP028"),
          XmNfontList, fontlist1,
          MY_FOREGROUND_COLOR,
          MY_BACKGROUND_COLOR,
          NULL);
    XtAddCallback(flush_map_cache_button, XmNactivateCallback,Flush_Entire_Map_Queue,NULL);
 

    //Index Maps on startup
    index_maps_on_startup_button = XtVaCreateManagedWidget(langcode("PULDNMP022"),
            xmToggleButtonGadgetClass,
            map_config_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(index_maps_on_startup_button,XmNvalueChangedCallback,Index_maps_on_startup_toggle,"1");
    if (index_maps_on_startup)
        XmToggleButtonSetState(index_maps_on_startup_button,TRUE,FALSE);


        map_indexer_button = XtVaCreateManagedWidget(langcode("PULDNMP023"),
          xmPushButtonGadgetClass,
          map_config_pane,
          XmNmnemonic,langcode_hotkey("PULDNMP023"),
          XmNfontList, fontlist1,
          MY_FOREGROUND_COLOR,
          MY_BACKGROUND_COLOR,
          NULL);

        map_all_indexer_button = XtVaCreateManagedWidget(langcode("PULDNMP024"),
          xmPushButtonGadgetClass,
          map_config_pane,
          XmNmnemonic,langcode_hotkey("PULDNMP024"),
          XmNfontList, fontlist1,
          MY_FOREGROUND_COLOR,
          MY_BACKGROUND_COLOR,
          NULL);


    (void)XtVaCreateManagedWidget("create_appshell sep2e",
            xmSeparatorGadgetClass,
            mappane,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);


    map_pointer_menu_button = XtVaCreateManagedWidget(langcode("PULDNMP011"),
            xmPushButtonGadgetClass,
            mappane,
            XmNmnemonic,langcode_hotkey("PULDNMP011"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);


//- Stations Menu -----------------------------------------------------
    locate_button = XtVaCreateManagedWidget(langcode("PULDNDP014"),
            xmPushButtonGadgetClass,
            stationspane,
            XmNmnemonic,langcode_hotkey("PULDNDP014"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    track_button = XtVaCreateManagedWidget(langcode("PULDNDP001"),
            xmPushButtonGadgetClass,
            stationspane,
            XmNmnemonic,langcode_hotkey("PULDNDP001"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(track_button, XmNactivateCallback,Track_station,NULL);

    download_trail_button = XtVaCreateManagedWidget(langcode("PULDNDP022"),
            xmPushButtonGadgetClass,
            stationspane,
            XmNmnemonic,langcode_hotkey("PULDNDP022"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(download_trail_button, XmNactivateCallback,Download_findu_trail,NULL);

 
    ac = 0;
    XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
    XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
    XtSetArg(al[ac], XmNnavigationType, XmTAB_GROUP); ac++;
    XtSetArg(al[ac], XmNtraversalOn, TRUE); ac++;
    XtSetArg(al[ac], XmNtearOffModel, XmTEAR_OFF_ENABLED); ac++;
    XtSetArg(al[ac], XmNfontList,        fontlist1);             ac++;



    // Store Data pulldown/tearoff
    store_data_pane = XmCreatePulldownMenu(stationspane,
            "store_data_pane",
            al,
            ac);

    // Export all > 
    store_data_button = XtVaCreateManagedWidget(langcode("PULDNDP055"),
            xmCascadeButtonGadgetClass,
            stationspane,
            XmNsubMenuId, store_data_pane,
            XmNmnemonic, langcode_hotkey("PULDNDP055"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    // Export to KML file
    store_all_kml_button = XtVaCreateManagedWidget(langcode("PULDNDP056"),
            xmPushButtonGadgetClass,
            store_data_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(store_all_kml_button, XmNactivateCallback, store_all_kml_callback, NULL);
    
#ifdef HAVE_DB
    // store to  open databases
    store_all_db_button = XtVaCreateManagedWidget("Store to open databases",
            xmPushButtonGadgetClass,
            store_data_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    //XtAddCallback(store_all_db_button, XmNvalueChangedCallback, store_all_db_button_callback, "1");
    XtSetSensitive(store_all_db_button,FALSE);
#endif // HAVE_DB


    (void)XtVaCreateManagedWidget("create_appshell sep3",
            xmSeparatorGadgetClass,
            stationspane,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    ac = 0;
    XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
    XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
    XtSetArg(al[ac], XmNnavigationType, XmTAB_GROUP); ac++;
    XtSetArg(al[ac], XmNtraversalOn, TRUE); ac++;
    XtSetArg(al[ac], XmNtearOffModel, XmTEAR_OFF_ENABLED); ac++;
    XtSetArg(al[ac], XmNfontList,        fontlist1);             ac++;


    // Filter Data pulldown/tearoff
    filter_data_pane = XmCreatePulldownMenu(stationspane,
            "filter_data_pane",
            al,
            ac);

    filter_data_button = XtVaCreateManagedWidget(langcode("PULDNDP032"),
            xmCascadeButtonGadgetClass,
            stationspane,
            XmNsubMenuId, filter_data_pane,
            XmNmnemonic, langcode_hotkey("PULDNDP032"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    select_none_button = XtVaCreateManagedWidget(langcode("PULDNDP040"),
            xmToggleButtonGadgetClass,
            filter_data_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(select_none_button, XmNvalueChangedCallback, Select_none_toggle, "1");
    if (Select_.none)
        XmToggleButtonSetState(select_none_button, TRUE, FALSE);


    select_mine_button = XtVaCreateManagedWidget(langcode("PULDNDP041"),
            xmToggleButtonGadgetClass,
            filter_data_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(select_mine_button, XmNvalueChangedCallback, Select_mine_toggle, "1");
    if (Select_.mine)
        XmToggleButtonSetState(select_mine_button, TRUE, FALSE);
    if (Select_.none)
        XtSetSensitive(select_mine_button, FALSE);


    select_tnc_button = XtVaCreateManagedWidget(langcode("PULDNDP042"),
            xmToggleButtonGadgetClass,
            filter_data_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(select_tnc_button, XmNvalueChangedCallback, Select_tnc_toggle, "1");
    if (Select_.tnc)
        XmToggleButtonSetState(select_tnc_button, TRUE, FALSE);
    if (Select_.none)
        XtSetSensitive(select_tnc_button, FALSE);


    select_direct_button = XtVaCreateManagedWidget(langcode("PULDNDP027"),
            xmToggleButtonGadgetClass,
            filter_data_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(select_direct_button, XmNvalueChangedCallback, Select_direct_toggle, "1");
    if (Select_.direct)
        XmToggleButtonSetState(select_direct_button, TRUE, FALSE);
    if (!Select_.tnc || Select_.none)
        XtSetSensitive(select_direct_button, FALSE);


    select_via_digi_button = XtVaCreateManagedWidget(langcode("PULDNDP043"),
            xmToggleButtonGadgetClass,
            filter_data_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(select_via_digi_button, XmNvalueChangedCallback, Select_via_digi_toggle, "1");
    if (Select_.via_digi)
        XmToggleButtonSetState(select_via_digi_button, TRUE, FALSE);
    if (!Select_.tnc || Select_.none)
        XtSetSensitive(select_via_digi_button, FALSE);


    select_net_button = XtVaCreateManagedWidget(langcode("PULDNDP034"),
            xmToggleButtonGadgetClass,
            filter_data_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(select_net_button, XmNvalueChangedCallback, Select_net_toggle, "1");
    if (Select_.net)
        XmToggleButtonSetState(select_net_button, TRUE, FALSE);
    if (Select_.none)
        XtSetSensitive(select_net_button, FALSE);


    // "Select Tactical Calls Only"
    select_tactical_button = XtVaCreateManagedWidget(langcode("PULDNDP051"),
            xmToggleButtonGadgetClass,
            filter_data_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(select_tactical_button, XmNvalueChangedCallback, Select_tactical_toggle, "1");
    if (Select_.tactical)
        XmToggleButtonSetState(select_tactical_button, TRUE, FALSE);
    if (Select_.none)
        XtSetSensitive(select_tactical_button, FALSE);


    select_old_data_button = XtVaCreateManagedWidget(langcode("PULDNDP019"),
            xmToggleButtonGadgetClass,
            filter_data_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(select_old_data_button, XmNvalueChangedCallback, Select_old_data_toggle, "1");
    if (Select_.old_data)
        XmToggleButtonSetState(select_old_data_button, TRUE, FALSE);
    if (no_data_selected())
        XtSetSensitive(select_old_data_button, FALSE);


    (void)XtVaCreateManagedWidget("create_appshell sep3a",
            xmSeparatorGadgetClass,
            filter_data_pane,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);


    select_stations_button = XtVaCreateManagedWidget(langcode("PULDNDP044"),
            xmToggleButtonGadgetClass,
            filter_data_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(select_stations_button, XmNvalueChangedCallback,
                  Select_stations_toggle, "1");
    if (Select_.stations)
        XmToggleButtonSetState(select_stations_button, TRUE, FALSE);
    if (no_data_selected())
        XtSetSensitive(select_stations_button, FALSE);


    select_fixed_stations_button = XtVaCreateManagedWidget(langcode("PULDNDP028"),
            xmToggleButtonGadgetClass,
            filter_data_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(select_fixed_stations_button, XmNvalueChangedCallback,
                  Select_fixed_stations_toggle, "1");
    if (Select_.fixed_stations)
        XmToggleButtonSetState(select_fixed_stations_button, TRUE, FALSE);
    if (!Select_.stations || no_data_selected())
        XtSetSensitive(select_fixed_stations_button, FALSE);


    select_moving_stations_button = XtVaCreateManagedWidget(langcode("PULDNDP029"),
            xmToggleButtonGadgetClass,
            filter_data_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(select_moving_stations_button, XmNvalueChangedCallback,
                  Select_moving_stations_toggle, "1");
    if (Select_.moving_stations)
        XmToggleButtonSetState(select_moving_stations_button, TRUE, FALSE);
    if (!Select_.stations || no_data_selected())
        XtSetSensitive(select_moving_stations_button, FALSE);


    select_weather_stations_button = XtVaCreateManagedWidget(langcode("PULDNDP030"),
            xmToggleButtonGadgetClass,
            filter_data_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(select_weather_stations_button, XmNvalueChangedCallback,
                  Select_weather_stations_toggle, "1");
    if (Select_.weather_stations)
        XmToggleButtonSetState(select_weather_stations_button, TRUE, FALSE);
    if (!Select_.stations || no_data_selected())
        XtSetSensitive(select_weather_stations_button, FALSE);


    select_CWOP_wx_stations_button = XtVaCreateManagedWidget(langcode("PULDNDP053"),
            xmToggleButtonGadgetClass,
            filter_data_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(select_CWOP_wx_stations_button, XmNvalueChangedCallback,
                  Select_CWOP_wx_stations_toggle, "1");
    if (Select_.CWOP_wx_stations)
        XmToggleButtonSetState(select_CWOP_wx_stations_button, TRUE, FALSE);
    if (!Select_.stations || no_data_selected() || !Select_.weather_stations)
        XtSetSensitive(select_CWOP_wx_stations_button, FALSE);
    else
        XtSetSensitive(select_CWOP_wx_stations_button, TRUE);


    select_objects_button = XtVaCreateManagedWidget(langcode("PULDNDP045"),
            xmToggleButtonGadgetClass,
            filter_data_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(select_objects_button, XmNvalueChangedCallback,
                  Select_objects_toggle, "1");
    if (Select_.objects)
        XmToggleButtonSetState(select_objects_button, TRUE, FALSE);
    if (no_data_selected())
        XtSetSensitive(select_objects_button, FALSE);


    select_weather_objects_button = XtVaCreateManagedWidget(langcode("PULDNDP026"),
            xmToggleButtonGadgetClass,
            filter_data_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(select_weather_objects_button, XmNvalueChangedCallback,
                  Select_weather_objects_toggle, "1");
    if (Select_.weather_objects)
        XmToggleButtonSetState(select_weather_objects_button, TRUE, FALSE);
    if (!Select_.objects || no_data_selected())
        XtSetSensitive(select_weather_objects_button, FALSE);


    select_gauge_objects_button = XtVaCreateManagedWidget(langcode("PULDNDP039"),
            xmToggleButtonGadgetClass,
            filter_data_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(select_gauge_objects_button, XmNvalueChangedCallback,
                  Select_gauge_objects_toggle, "1");
    if (Select_.gauge_objects)
        XmToggleButtonSetState(select_gauge_objects_button, TRUE, FALSE);
    if (!Select_.objects || no_data_selected())
        XtSetSensitive(select_gauge_objects_button, FALSE);


    select_other_objects_button = XtVaCreateManagedWidget(langcode("PULDNDP031"),
            xmToggleButtonGadgetClass,
            filter_data_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(select_other_objects_button, XmNvalueChangedCallback,
                  Select_other_objects_toggle, "1");
    if (Select_.other_objects)
        XmToggleButtonSetState(select_other_objects_button, TRUE, FALSE);
    if (!Select_.objects || no_data_selected())
        XtSetSensitive(select_other_objects_button, FALSE);


    // End of Data Filtering

    ac = 0;
    XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
    XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
    XtSetArg(al[ac], XmNnavigationType, XmTAB_GROUP); ac++;
    XtSetArg(al[ac], XmNtraversalOn, TRUE); ac++;
    XtSetArg(al[ac], XmNtearOffModel, XmTEAR_OFF_ENABLED); ac++;
    XtSetArg(al[ac], XmNfontList,        fontlist1);             ac++;


    // Displayed Info Filtering
    filter_display_pane = XmCreatePulldownMenu(stationspane,
            "filter_display_pane",
            al,
            ac);

    filter_display_button = XtVaCreateManagedWidget(langcode("PULDNDP033"),
            xmCascadeButtonGadgetClass,
            stationspane,
            XmNsubMenuId, filter_display_pane,
            XmNmnemonic, langcode_hotkey("PULDNDP033"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);


    display_callsign_button = XtVaCreateManagedWidget(langcode("PULDNDP010"),
            xmToggleButtonGadgetClass,
            filter_display_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(display_callsign_button, XmNvalueChangedCallback, Display_callsign_toggle, "1");
    if (Display_.callsign)
        XmToggleButtonSetState(display_callsign_button, TRUE, FALSE);
    if (no_data_selected())
        XtSetSensitive(display_callsign_button, FALSE);

    display_label_all_trackpoints_button = XtVaCreateManagedWidget(langcode("PULDNDP052"),
            xmToggleButtonGadgetClass,
            filter_display_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(display_label_all_trackpoints_button, XmNvalueChangedCallback, Display_label_all_trackpoints_toggle, "1");
    if (Display_.label_all_trackpoints)
        XmToggleButtonSetState(display_label_all_trackpoints_button, TRUE, FALSE);
    if (!Display_.callsign || no_data_selected())
        XtSetSensitive(display_label_all_trackpoints_button, FALSE);

    display_symbol_button = XtVaCreateManagedWidget(langcode("PULDNDP012"),
            xmToggleButtonGadgetClass,
            filter_display_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(display_symbol_button, XmNvalueChangedCallback, Display_symbol_toggle, "1");
    if (Display_.symbol)
        XmToggleButtonSetState(display_symbol_button, TRUE, FALSE);
    if (no_data_selected())
        XtSetSensitive(display_symbol_button, FALSE);


    display_symbol_rotate_button = XtVaCreateManagedWidget(langcode("PULDNDP011"),
            xmToggleButtonGadgetClass,
            filter_display_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(display_symbol_rotate_button, XmNvalueChangedCallback, Display_symbol_rotate_toggle, "1");
    if (Display_.symbol_rotate)
        XmToggleButtonSetState(display_symbol_rotate_button, TRUE, FALSE);
    if (!Display_.symbol || no_data_selected())
        XtSetSensitive(display_symbol_rotate_button, FALSE);


    display_trail_button = XtVaCreateManagedWidget(langcode("PULDNDP007"),
            xmToggleButtonGadgetClass,
            filter_display_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(display_trail_button, XmNvalueChangedCallback, Display_trail_toggle, "1");
    if (Display_.trail)
        XmToggleButtonSetState(display_trail_button, TRUE, FALSE);
    if (no_data_selected())
        XtSetSensitive(display_trail_button, FALSE);


    (void)XtVaCreateManagedWidget("create_appshell sep3b",
            xmSeparatorGadgetClass,
            filter_display_pane,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);


    display_course_button = XtVaCreateManagedWidget(langcode("PULDNDP003"),
            xmToggleButtonGadgetClass,
            filter_display_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(display_course_button, XmNvalueChangedCallback, Display_course_toggle, "1");
    if (Display_.course)
        XmToggleButtonSetState(display_course_button, TRUE, FALSE);
    if (no_data_selected())
        XtSetSensitive(display_course_button, FALSE);


    display_speed_button = XtVaCreateManagedWidget(langcode("PULDNDP004"),
            xmToggleButtonGadgetClass,
            filter_display_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(display_speed_button, XmNvalueChangedCallback, Display_speed_toggle, "1");
    if (Display_.speed)
        XmToggleButtonSetState(display_speed_button, TRUE, FALSE);
    if (no_data_selected())
        XtSetSensitive(display_speed_button, FALSE);


    display_speed_short_button = XtVaCreateManagedWidget(langcode("PULDNDP017"),
            xmToggleButtonGadgetClass,
            filter_display_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(display_speed_short_button, XmNvalueChangedCallback, Display_speed_short_toggle, "1");
    if (Display_.speed_short)
        XmToggleButtonSetState(display_speed_short_button, TRUE, FALSE);
    if (!Display_.speed || no_data_selected())
        XtSetSensitive(display_speed_short_button, FALSE);


    display_altitude_button = XtVaCreateManagedWidget(langcode("PULDNDP002"),
            xmToggleButtonGadgetClass,
            filter_display_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(display_altitude_button, XmNvalueChangedCallback, Display_altitude_toggle, "1");
    if (Display_.altitude)
        XmToggleButtonSetState(display_altitude_button, TRUE, FALSE);
    if (no_data_selected())
        XtSetSensitive(display_altitude_button, FALSE);


    (void)XtVaCreateManagedWidget("create_appshell sep3c",
            xmSeparatorGadgetClass,
            filter_display_pane,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);


    display_weather_button = XtVaCreateManagedWidget(langcode("PULDNDP009"),
            xmToggleButtonGadgetClass,
            filter_display_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(display_weather_button, XmNvalueChangedCallback, Display_weather_toggle, "1");
    if (Display_.weather)
        XmToggleButtonSetState(display_weather_button, TRUE, FALSE);
    if (no_data_selected())
        XtSetSensitive(display_weather_button, FALSE);


    display_weather_text_button = XtVaCreateManagedWidget(langcode("PULDNDP046"),
            xmToggleButtonGadgetClass,
            filter_display_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(display_weather_text_button, XmNvalueChangedCallback, Display_weather_text_toggle, "1");
    if (Display_.weather_text)
        XmToggleButtonSetState(display_weather_text_button, TRUE, FALSE);
    if (!Display_.weather || no_data_selected())
        XtSetSensitive(display_weather_text_button, FALSE);


    display_temperature_only_button = XtVaCreateManagedWidget(langcode("PULDNDP018"),
            xmToggleButtonGadgetClass,
            filter_display_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(display_temperature_only_button, XmNvalueChangedCallback, Display_temperature_only_toggle, "1");
    if (Display_.temperature_only)
        XmToggleButtonSetState(display_temperature_only_button, TRUE, FALSE);
    if (!Display_.weather || !Display_.weather_text || no_data_selected())
        XtSetSensitive(display_temperature_only_button, FALSE);


    display_wind_barb_button = XtVaCreateManagedWidget(langcode("PULDNDP047"),
            xmToggleButtonGadgetClass,
            filter_display_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(display_wind_barb_button, XmNvalueChangedCallback, Display_wind_barb_toggle, "1");
    if (Display_.wind_barb)
        XmToggleButtonSetState(display_wind_barb_button, TRUE, FALSE);
    if (!Display_.weather || no_data_selected())
        XtSetSensitive(display_wind_barb_button, FALSE);


    (void)XtVaCreateManagedWidget("create_appshell sep3d",
            xmSeparatorGadgetClass,
            filter_display_pane,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);


    display_aloha_circle_button = XtVaCreateManagedWidget(langcode("PULDNDP054"),
            xmToggleButtonGadgetClass,
            filter_display_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(display_aloha_circle_button, XmNvalueChangedCallback, Display_aloha_circle_toggle, "1");
    if (Display_.aloha_circle)
        XmToggleButtonSetState(display_aloha_circle_button, TRUE, FALSE);


    display_ambiguity_button = XtVaCreateManagedWidget(langcode("PULDNDP013"),
            xmToggleButtonGadgetClass,
            filter_display_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(display_ambiguity_button, XmNvalueChangedCallback, Display_ambiguity_toggle, "1");
    if (Display_.ambiguity)
        XmToggleButtonSetState(display_ambiguity_button, TRUE, FALSE);
    if (no_data_selected())
        XtSetSensitive(display_ambiguity_button, FALSE);


    display_phg_button = XtVaCreateManagedWidget(langcode("PULDNDP008"),
            xmToggleButtonGadgetClass,
            filter_display_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(display_phg_button, XmNvalueChangedCallback, Display_phg_toggle, "1");
    if (Display_.phg)
        XmToggleButtonSetState(display_phg_button, TRUE, FALSE);
    if (no_data_selected())
        XtSetSensitive(display_phg_button, FALSE);


    display_default_phg_button = XtVaCreateManagedWidget(langcode("PULDNDP021"),
            xmToggleButtonGadgetClass,
            filter_display_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(display_default_phg_button, XmNvalueChangedCallback, Display_default_phg_toggle, "1");
    if (Display_.default_phg)
        XmToggleButtonSetState(display_default_phg_button, TRUE, FALSE);
    if (!Display_.phg || no_data_selected())
        XtSetSensitive(display_default_phg_button, FALSE);


    display_phg_of_moving_button = XtVaCreateManagedWidget(langcode("PULDNDP020"),
            xmToggleButtonGadgetClass,
            filter_display_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(display_phg_of_moving_button, XmNvalueChangedCallback, Display_phg_of_moving_toggle, "1");
    if (Display_.phg_of_moving)
        XmToggleButtonSetState(display_phg_of_moving_button, TRUE, FALSE);
    if (!Display_.phg || no_data_selected())
        XtSetSensitive(display_phg_of_moving_button, FALSE);


    (void)XtVaCreateManagedWidget("create_appshell sep3e",
            xmSeparatorGadgetClass,
            filter_display_pane,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);


    display_df_data_button = XtVaCreateManagedWidget(langcode("PULDNDP023"),
            xmToggleButtonGadgetClass,
            filter_display_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(display_df_data_button, XmNvalueChangedCallback, Display_df_data_toggle, "1");
    if (Display_.df_data)
        XmToggleButtonSetState(display_df_data_button, TRUE, FALSE);
    if (no_data_selected())
        XtSetSensitive(display_df_data_button, FALSE);


    display_dr_data_button = XtVaCreateManagedWidget(langcode("PULDNDP035"),
            xmToggleButtonGadgetClass,
            filter_display_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(display_dr_data_button, XmNvalueChangedCallback, Display_dr_data_toggle, "1");
    if (Display_.dr_data)
        XmToggleButtonSetState(display_dr_data_button, TRUE, FALSE);
    if (no_data_selected())
        XtSetSensitive(display_dr_data_button, FALSE);


    display_dr_arc_button = XtVaCreateManagedWidget(langcode("PULDNDP036"),
            xmToggleButtonGadgetClass,
            filter_display_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(display_dr_arc_button, XmNvalueChangedCallback, Display_dr_arc_toggle, "1");
    if (Display_.dr_arc)
        XmToggleButtonSetState(display_dr_arc_button, TRUE, FALSE);
    if (!Display_.dr_data || no_data_selected())
        XtSetSensitive(display_dr_arc_button, FALSE);


    display_dr_course_button = XtVaCreateManagedWidget(langcode("PULDNDP037"),
            xmToggleButtonGadgetClass,
            filter_display_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(display_dr_course_button, XmNvalueChangedCallback, Display_dr_course_toggle, "1");
    if (Display_.dr_course)
        XmToggleButtonSetState(display_dr_course_button, TRUE, FALSE);
    if (!Display_.dr_data || no_data_selected())
        XtSetSensitive(display_dr_course_button, FALSE);


    display_dr_symbol_button = XtVaCreateManagedWidget(langcode("PULDNDP038"),
            xmToggleButtonGadgetClass,
            filter_display_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(display_dr_symbol_button, XmNvalueChangedCallback, Display_dr_symbol_toggle, "1");
    if (Display_.dr_symbol)
        XmToggleButtonSetState(display_dr_symbol_button, TRUE, FALSE);
    if (!Display_.dr_data || no_data_selected())
        XtSetSensitive(display_dr_symbol_button, FALSE);


    (void)XtVaCreateManagedWidget("create_appshell sep3f",
            xmSeparatorGadgetClass,
            filter_display_pane,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);


    display_dist_bearing_button = XtVaCreateManagedWidget(langcode("PULDNDP005"),
            xmToggleButtonGadgetClass,
            filter_display_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(display_dist_bearing_button, XmNvalueChangedCallback, Display_dist_bearing_toggle, "1");
    if (Display_.dist_bearing)
        XmToggleButtonSetState(display_dist_bearing_button, TRUE, FALSE);
    if (no_data_selected())
        XtSetSensitive(display_dist_bearing_button, FALSE);


    display_last_heard_button = XtVaCreateManagedWidget(langcode("PULDNDP024"),
            xmToggleButtonGadgetClass,
            filter_display_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(display_last_heard_button, XmNvalueChangedCallback, Display_last_heard_toggle, "1");
    if (Display_.last_heard)
        XmToggleButtonSetState(display_last_heard_button, TRUE, FALSE);
    if (no_data_selected())
        XtSetSensitive(display_last_heard_button, FALSE);


    // End of Displayed Info Filtering



    (void)XtVaCreateManagedWidget("create_appshell sep3g",
            xmSeparatorGadgetClass,
            stationspane,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    (void)XtVaCreateManagedWidget("create_appshell sep3h",
            xmSeparatorGadgetClass,
            stationspane,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    ac = 0;
    XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
    XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
    XtSetArg(al[ac], XmNnavigationType, XmTAB_GROUP); ac++;
    XtSetArg(al[ac], XmNtraversalOn, TRUE); ac++;
    XtSetArg(al[ac], XmNtearOffModel, XmTEAR_OFF_ENABLED); ac++;
    XtSetArg(al[ac], XmNfontList,        fontlist1);             ac++;


    station_config_pane  = XmCreatePulldownMenu(stationspane,
            "stations_config_pane",
            al,
            ac);

    station_config_button = XtVaCreateManagedWidget(langcode("PULDNFI001"),
            xmCascadeButtonGadgetClass,
            stationspane,
            XmNsubMenuId,station_config_pane,
            XmNmnemonic,langcode_hotkey("PULDNFI001"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);


    object_history_refresh_button = XtVaCreateManagedWidget(langcode("PULDNDP048"),
            xmPushButtonGadgetClass,
            station_config_pane,
            XmNmnemonic,langcode_hotkey("PULDNDP048"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    object_history_clear_button = XtVaCreateManagedWidget(langcode("PULDNDP025"),
            xmPushButtonGadgetClass,
            station_config_pane,
            XmNmnemonic,langcode_hotkey("PULDNDP025"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    // "Clear All Tactical Calls"
    tactical_clear_button = XtVaCreateManagedWidget(langcode("PULDNDP049"),
            xmPushButtonGadgetClass,
            station_config_pane,
//            XmNmnemonic,langcode_hotkey("PULDNDP049"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);


    // "Clear Tactical Call History"
    tactical_history_clear_button = XtVaCreateManagedWidget(langcode("PULDNDP050"),
            xmPushButtonGadgetClass,
            station_config_pane,
//            XmNmnemonic,langcode_hotkey("PULDNDP050"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    tracks_clear_button = XtVaCreateManagedWidget(langcode("PULDNDP016"),
            xmPushButtonGadgetClass,
            station_config_pane,
            XmNmnemonic,langcode_hotkey("PULDNDP016"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    station_clear_button = XtVaCreateManagedWidget(langcode("PULDNDP015"),
            xmPushButtonGadgetClass,
            station_config_pane,
            XmNmnemonic,langcode_hotkey("PULDNDP015"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

//--------------------------------------------------------------------

    /* Messages */
    send_message_to_button = XtVaCreateManagedWidget(langcode("PULDNMG001"),
            xmPushButtonGadgetClass,
            messagepane,
            XmNmnemonic,langcode_hotkey("PULDNMG001"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    show_pending_messages_button = XtVaCreateManagedWidget(langcode("PULDNMG007"),
            xmPushButtonGadgetClass,
            messagepane,
            XmNmnemonic,langcode_hotkey("PULDNMG007"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    open_messages_group_button = XtVaCreateManagedWidget(langcode("PULDNMG002"),
            xmPushButtonGadgetClass,
            messagepane,
            XmNmnemonic,langcode_hotkey("PULDNMG002"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    clear_messages_button= XtVaCreateManagedWidget(langcode("PULDNMG003"),
            xmPushButtonGadgetClass,
            messagepane,
            XmNmnemonic,langcode_hotkey("PULDNMG003"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    (void)XtVaCreateManagedWidget("create_appshell sep4",
            xmSeparatorGadgetClass,
            messagepane,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    General_q_button = XtVaCreateManagedWidget(langcode("PULDQUS001"),
            xmPushButtonGadgetClass,
            messagepane,
            XmNmnemonic,langcode_hotkey("PULDQUS001"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    IGate_q_button = XtVaCreateManagedWidget(langcode("PULDQUS002"),
            xmPushButtonGadgetClass,
            messagepane,
            XmNmnemonic,langcode_hotkey("PULDQUS002"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    WX_q_button = XtVaCreateManagedWidget(langcode("PULDQUS003"),
            xmPushButtonGadgetClass,
            messagepane,
            XmNmnemonic,langcode_hotkey("PULDQUS003"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

   (void)XtVaCreateManagedWidget("create_appshell sep4a",
            xmSeparatorGadgetClass,
            messagepane,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    auto_msg_set_button = XtVaCreateManagedWidget(langcode("PULDNMG004"),
            xmPushButtonGadgetClass,
            messagepane,
            XmNmnemonic,langcode_hotkey("PULDNMG004"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);


    auto_msg_toggle = XtVaCreateManagedWidget(langcode("PULDNMG005"),
            xmToggleButtonGadgetClass,
            messagepane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(auto_msg_toggle,XmNvalueChangedCallback,Auto_msg_toggle,"1");

   (void)XtVaCreateManagedWidget("create_appshell sep5",
            xmSeparatorGadgetClass,
            messagepane,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    satellite_msg_ack_toggle = XtVaCreateManagedWidget(langcode("PULDNMG006"),
            xmToggleButtonGadgetClass,
            messagepane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(satellite_msg_ack_toggle,XmNvalueChangedCallback,Satellite_msg_ack_toggle,"1");



    /* Interface */
    iface_connect_button = XtVaCreateManagedWidget(langcode("PULDNTNT04"),
            xmPushButtonGadgetClass,
            ifacepane,
            XmNmnemonic,langcode_hotkey("PULDNTNT04"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    (void)XtVaCreateManagedWidget("create_appshell sep5a",
            xmSeparatorGadgetClass,
            ifacepane,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);


    transmit_disable_toggle =  XtVaCreateManagedWidget(langcode("PULDNTNT03"),
            xmToggleButtonGadgetClass,
            ifacepane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
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
            XmNfontList, fontlist1,
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
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(object_tx_disable_toggle,XmNvalueChangedCallback,Object_tx_disable_toggle,"1");
    if (object_tx_disable)
        XmToggleButtonSetState(object_tx_disable_toggle,TRUE,FALSE);
    if (transmit_disable)
        XtSetSensitive(object_tx_disable_toggle,FALSE);


    server_port_toggle = XtVaCreateManagedWidget(langcode("PULDNTNT11"),
            xmToggleButtonGadgetClass,
            ifacepane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(server_port_toggle,XmNvalueChangedCallback,Server_port_toggle,"1");
    if (enable_server_port)
        XmToggleButtonSetState(server_port_toggle,TRUE,FALSE);


    (void)XtVaCreateManagedWidget("create_appshell sep5b",
            xmSeparatorGadgetClass,
            ifacepane,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);


    iface_transmit_now = XtVaCreateManagedWidget(langcode("PULDNTNT01"),
            xmPushButtonGadgetClass,
            ifacepane,
            XmNmnemonic,langcode_hotkey("PULDNTNT01"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    if (transmit_disable)
        XtSetSensitive(iface_transmit_now,FALSE);

#ifdef HAVE_GPSMAN
    Fetch_gps_track = XtVaCreateManagedWidget(langcode("PULDNTNT07"),
            xmPushButtonGadgetClass,
            ifacepane,
            XmNmnemonic,langcode_hotkey("PULDNTNT07"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    Fetch_gps_route = XtVaCreateManagedWidget(langcode("PULDNTNT08"),
            xmPushButtonGadgetClass,
            ifacepane,
            XmNmnemonic,langcode_hotkey("PULDNTNT08"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    Fetch_gps_waypoints = XtVaCreateManagedWidget(langcode("PULDNTNT09"),
            xmPushButtonGadgetClass,
            ifacepane,
            XmNmnemonic,langcode_hotkey("PULDNTNT09"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

/*
    Send_gps_track = XtVaCreateManagedWidget(langcode("Send_Tr"),
            xmPushButtonGadgetClass,
            ifacepane,
            XmNmnemonic,langcode_hotkey("Send_Tr"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    Send_gps_route = XtVaCreateManagedWidget(langcode("Send_Rt"), 
            xmPushButtonGadgetClass,
            ifacepane,
            XmNmnemonic,langcode_hotkey("Send_Rt"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    Send_gps_waypoints = XtVaCreateManagedWidget(langcode("Send_Wp"),
            xmPushButtonGadgetClass,
            ifacepane,
            XmNmnemonic,langcode_hotkey("Send_Wp"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
*/

    Fetch_RINO_waypoints = XtVaCreateManagedWidget(langcode("PULDNTNT10"),
            xmPushButtonGadgetClass,
            ifacepane,
            XmNmnemonic,langcode_hotkey("PULDNTNT10"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

#endif  // HAVE_GPSMAN 

    /* Help*/
    help_about = XtVaCreateManagedWidget(langcode("PULDNHEL01"),
            xmPushButtonGadgetClass,
            helppane,
            XmNmnemonic,langcode_hotkey("PULDNHEL01"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    help_help = XtVaCreateManagedWidget(langcode("PULDNHEL02"),
            xmPushButtonGadgetClass,
            helppane,
            XmNmnemonic,langcode_hotkey("PULDNHEL02"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    (void)XtVaCreateManagedWidget("create_appshell sephelp",
            xmSeparatorGadgetClass,
            helppane,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    ac = 0;
    XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
    XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
    XtSetArg(al[ac], XmNnavigationType, XmTAB_GROUP); ac++;
    XtSetArg(al[ac], XmNtraversalOn, TRUE); ac++;
    XtSetArg(al[ac], XmNfontList,        fontlist1);             ac++;


    help_emergency_pane = XmCreatePulldownMenu(helppane,
            "help_emergency_pane",
            al,
            ac);

    help_emergency_button = XtVaCreateManagedWidget(langcode("PULDNHEL03"),
            xmCascadeButtonGadgetClass,
            helppane,
            XmNsubMenuId,help_emergency_pane,
            XmNmnemonic,langcode_hotkey("PULDNHEL03"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    emergency_beacon_toggle =  XtVaCreateManagedWidget(langcode("PULDNHEL04"),
            xmToggleButtonGadgetClass,
            help_emergency_pane,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(emergency_beacon_toggle,XmNvalueChangedCallback,Emergency_beacon_toggle,"1");
    if (emergency_beacon)
        XmToggleButtonSetState(emergency_beacon_toggle,TRUE,FALSE);


    /* view */
    XtAddCallback(stations_button,      XmNactivateCallback,Station_List,"0");
    XtAddCallback(mobile_button,        XmNactivateCallback,Station_List,"1");
    XtAddCallback(weather_button,       XmNactivateCallback,Station_List,"2");
    XtAddCallback(localstations_button, XmNactivateCallback,Station_List,"3");
    XtAddCallback(laststations_button,  XmNactivateCallback,Station_List,"4");
    XtAddCallback(objectstations_button,XmNactivateCallback,Station_List,"5");
    XtAddCallback(objectmystations_button,XmNactivateCallback,Station_List,"6");
    XtAddCallback(CAD1,XmNactivateCallback,Draw_CAD_Objects_list_dialog,NULL);

    /* button callbacks */
    XtAddCallback(General_q_button,     XmNactivateCallback,General_query,"");
    XtAddCallback(IGate_q_button,       XmNactivateCallback,IGate_query,NULL);
    XtAddCallback(WX_q_button,          XmNactivateCallback,WX_query,NULL);
    XtAddCallback(station_clear_button, XmNactivateCallback,Stations_Clear,NULL);
    XtAddCallback(tracks_clear_button,  XmNactivateCallback,Tracks_All_Clear,NULL);
    XtAddCallback(object_history_refresh_button, XmNactivateCallback,Object_History_Refresh,NULL);
    XtAddCallback(object_history_clear_button, XmNactivateCallback,Object_History_Clear,NULL);
    XtAddCallback(tactical_clear_button, XmNactivateCallback,Tactical_Callsign_Clear,NULL);
    XtAddCallback(tactical_history_clear_button, XmNactivateCallback,Tactical_Callsign_History_Clear,NULL);
    XtAddCallback(exit_button,   XmNactivateCallback,Menu_Quit,NULL);

    XtAddCallback(defaults_button,      XmNactivateCallback,Configure_defaults,NULL);
    XtAddCallback(timing_button,        XmNactivateCallback,Configure_timing,NULL);
    XtAddCallback(coordinates_button,   XmNactivateCallback,Configure_coordinates,NULL);
    XtAddCallback(aa_button,            XmNactivateCallback,Configure_audio_alarms,NULL);
    XtAddCallback(speech_button,        XmNactivateCallback,Configure_speech,NULL);
    XtAddCallback(smart_beacon_button,  XmNactivateCallback,Smart_Beacon,NULL);
    XtAddCallback(map_indexer_button,   XmNactivateCallback,Index_Maps_Now,NULL);
    XtAddCallback(map_all_indexer_button,XmNactivateCallback,Index_Maps_Now,"1");
    XtAddCallback(station_button,       XmNactivateCallback,Configure_station,NULL);

    XtAddCallback(help_about,           XmNactivateCallback,Help_About,NULL);
    XtAddCallback(help_help,            XmNactivateCallback,Help_Index,NULL);

    /* TNC */
    XtAddCallback(iface_transmit_now,   XmNactivateCallback,TNC_Transmit_now,NULL);

#ifdef HAVE_GPSMAN
    XtAddCallback(Fetch_gps_track,      XmNactivateCallback,GPS_operations,"1");
    XtAddCallback(Fetch_gps_route,      XmNactivateCallback,GPS_operations,"2");
    XtAddCallback(Fetch_gps_waypoints,  XmNactivateCallback,GPS_operations,"3");
//    XtAddCallback(Send_gps_track,       XmNactivateCallback,GPS_operations,"4");
//    XtAddCallback(Send_gps_route,       XmNactivateCallback,GPS_operations,"5");
//    XtAddCallback(Send_gps_waypoints,   XmNactivateCallback,GPS_operations,"6");
    XtAddCallback(Fetch_RINO_waypoints, XmNactivateCallback,GPS_operations,"7");
#endif  // HAVE_GPSMAN

    XtAddCallback(auto_msg_set_button,XmNactivateCallback,Auto_msg_set,NULL);

    XtAddCallback(iface_connect_button, XmNactivateCallback,control_interface,NULL);

    XtAddCallback(open_file_button,     XmNactivateCallback,Read_File_Selection,NULL);

    XtAddCallback(bullet_button,        XmNactivateCallback,Bulletins,NULL);
    XtAddCallback(packet_data_button,   XmNactivateCallback,Display_data,NULL);
    XtAddCallback(locate_button,        XmNactivateCallback,Locate_station,NULL);
    XtAddCallback(alert_button,         XmNactivateCallback,Display_Wx_Alert,NULL);
    XtAddCallback(view_messages_button, XmNactivateCallback,view_all_messages,NULL);
    XtAddCallback(gps_status_button,XmNactivateCallback,view_gps_status,NULL);

    XtAddCallback(map_pointer_menu_button, XmNactivateCallback,menu_link_for_mouse_menu,NULL);

    XtAddCallback(wx_station_button,    XmNactivateCallback,WX_station,NULL);
    XtAddCallback(jump_button,          XmNactivateCallback, Jump_location, NULL);
    XtAddCallback(locate_place_button,  XmNactivateCallback,Locate_place,NULL);
    XtAddCallback(geocode_place_button,  XmNactivateCallback,Geocoder_place,NULL);
    XtAddCallback(coordinate_calculator_button, XmNactivateCallback,Coordinate_calc,"");

    XtAddCallback(send_message_to_button,       XmNactivateCallback,Send_message,NULL);
    XtAddCallback(show_pending_messages_button, XmNactivateCallback,Show_pending_messages,NULL);
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
    XtAddCallback(aloha_button,   XmNactivateCallback, Show_Aloha_Stats,NULL);
    //XtSetSensitive(uptime_button, False);



    // Toolbar
    toolbar = XtVaCreateWidget("Toolbar form",
            xmFormWidgetClass,
            form,
            XmNtopAttachment, XmATTACH_FORM,
            XmNtopOffset, 0,
            XmNbottomAttachment, XmATTACH_NONE,
            XmNleftAttachment, XmATTACH_WIDGET,
            XmNleftWidget, menubar,
            XmNleftOffset, 0,
            XmNrightAttachment, XmATTACH_NONE,
            XmNautoUnmanage, FALSE,
            XmNshadowThickness, 1,
            XmNnavigationType, XmTAB_GROUP,
            XmNtraversalOn, FALSE,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    trackme_button=XtVaCreateManagedWidget(langcode("POPUPMA022"),
            xmToggleButtonGadgetClass,
            toolbar,
            XmNtopAttachment, XmATTACH_FORM,
            XmNtopOffset, -5,
            XmNbottomAttachment, XmATTACH_NONE,
            XmNbottomOffset, 0,
            XmNleftAttachment, XmATTACH_FORM,
            XmNleftOffset, 0,
            XmNrightAttachment, XmATTACH_NONE,
            XmNrightOffset, 0,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNnavigationType, XmTAB_GROUP,
            XmNtraversalOn, FALSE,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(trackme_button,XmNvalueChangedCallback,Track_Me,"1");

    measure_button=XtVaCreateManagedWidget(langcode("POPUPMA020"),
            xmToggleButtonGadgetClass,
            toolbar,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, trackme_button,
            XmNtopOffset, -7,
            XmNbottomAttachment, XmATTACH_FORM,
            XmNbottomOffset, -5,
            XmNleftAttachment, XmATTACH_FORM,
            XmNleftOffset, 0,
            XmNrightAttachment, XmATTACH_NONE,
            XmNrightOffset, 0,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNnavigationType, XmTAB_GROUP,
            XmNtraversalOn, FALSE,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(measure_button,XmNvalueChangedCallback,Measure_Distance,"1");

    cad_draw_button=XtVaCreateManagedWidget(langcode("POPUPMA030"),
            xmToggleButtonGadgetClass,
            toolbar,
            XmNtopAttachment, XmATTACH_FORM,
            XmNtopOffset, -5,
            XmNbottomAttachment, XmATTACH_NONE,
            XmNbottomOffset, 0,
            XmNleftAttachment, XmATTACH_WIDGET,
            XmNleftWidget, trackme_button,
            XmNleftOffset, 0,
            XmNrightAttachment, XmATTACH_NONE,
            XmNrightOffset, 0,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNnavigationType, XmTAB_GROUP,
            XmNtraversalOn, FALSE,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(cad_draw_button,XmNvalueChangedCallback,Draw_CAD_Objects_mode,NULL);
 
    move_button=XtVaCreateManagedWidget(langcode("POPUPMA021"),
            xmToggleButtonGadgetClass,
            toolbar,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, cad_draw_button,
            XmNtopOffset, -7,
            XmNbottomAttachment, XmATTACH_FORM,
            XmNbottomOffset, -5,
            XmNleftAttachment, XmATTACH_WIDGET,
            XmNleftWidget, trackme_button,
            XmNleftOffset, 0,
            XmNrightAttachment, XmATTACH_NONE,
            XmNrightOffset, 0,
            XmNvisibleWhenOff, TRUE,
            XmNindicatorSize, 12,
            XmNnavigationType, XmTAB_GROUP,
            XmNtraversalOn, FALSE,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(move_button,XmNvalueChangedCallback,Move_Object,"1");



#ifdef ARROWS
    zoom_in_menu=XtVaCreateManagedWidget(langcode("POPUPMA002"),
            xmPushButtonWidgetClass,
            toolbar,
            XmNtopAttachment, XmATTACH_FORM,
            XmNbottomAttachment, XmATTACH_FORM,
            XmNleftAttachment, XmATTACH_WIDGET,
            XmNleftWidget, cad_draw_button,
            XmNleftOffset, 0,
            XmNrightAttachment, XmATTACH_NONE,
            XmNnavigationType, XmTAB_GROUP,
            XmNtraversalOn, FALSE,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(zoom_in_menu,XmNactivateCallback,Zoom_in_no_pan,NULL);

    zoom_out_menu=XtVaCreateManagedWidget(langcode("POPUPMA003"),
            xmPushButtonWidgetClass,
            toolbar,
            XmNtopAttachment, XmATTACH_FORM,
            XmNbottomAttachment, XmATTACH_FORM,
            XmNleftAttachment, XmATTACH_WIDGET,
            XmNleftWidget, zoom_in_menu,
            XmNleftOffset, 0,
            XmNrightAttachment, XmATTACH_NONE,
            XmNnavigationType, XmTAB_GROUP,
            XmNtraversalOn, FALSE,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);
    XtAddCallback(zoom_out_menu,XmNactivateCallback,Zoom_out_no_pan,NULL);

    pan_left_menu=XtVaCreateManagedWidget("create_appshell arrow1_menu",
            xmArrowButtonGadgetClass,
            toolbar,
            XmNtopAttachment, XmATTACH_FORM,
            XmNbottomAttachment, XmATTACH_FORM,
            XmNleftAttachment, XmATTACH_WIDGET,
            XmNleftWidget, zoom_out_menu,
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

    // Create bottom text area
    //
#ifdef USE_TWO_STATUS_LINES

    // Quantity of stations box, Bottom left corner
    text3 = XtVaCreateWidget("create_appshell text_output3",
            xmTextFieldWidgetClass,
            form,
            XmNeditable,            FALSE,
            XmNcursorPositionVisible, FALSE,
            XmNsensitive,           STIPPLE,
            XmNshadowThickness,     1,
            XmNcolumns,             14,
            XmNwidth,               ((22*FONT_WIDTH)+2),
            XmNtopAttachment,       XmATTACH_NONE,
            XmNbottomAttachment,    XmATTACH_FORM,
            XmNleftAttachment,      XmATTACH_FORM,
            XmNrightAttachment,     XmATTACH_NONE,
            XmNnavigationType,      XmTAB_GROUP,
            XmNtraversalOn,         FALSE,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    // Zoom level box, Bottom second from left
    text4 = XtVaCreateWidget("create_appshell text_output4",
            xmTextFieldWidgetClass,
            form,
            XmNeditable,            FALSE,
            XmNcursorPositionVisible, FALSE,
            XmNsensitive,           STIPPLE,
            XmNshadowThickness,     1,
            XmNcolumns,             10,
            XmNwidth,               ((15*FONT_WIDTH)+2),
            XmNtopAttachment,       XmATTACH_NONE,
            XmNbottomAttachment,    XmATTACH_FORM,
            XmNleftAttachment,      XmATTACH_WIDGET,
            XmNleftWidget,          text3,
            XmNrightAttachment,     XmATTACH_NONE,
            XmNnavigationType,      XmTAB_GROUP,
            XmNtraversalOn,         FALSE,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    // Log indicator box, Bottom second from right
    log_indicator = XtVaCreateWidget(langcode("BBARSTA043"),
            xmTextFieldWidgetClass,
            form,
            XmNeditable,            FALSE,
            XmNcursorPositionVisible, FALSE,
            XmNsensitive,           STIPPLE,
            XmNshadowThickness,     1,
            XmNcolumns,             8,
            XmNwidth,               ((8*FONT_WIDTH)),
            XmNtopAttachment,       XmATTACH_NONE,
            XmNbottomAttachment,    XmATTACH_FORM,
            XmNleftAttachment,      XmATTACH_WIDGET,
            XmNleftWidget,          text4,
            XmNrightAttachment,     XmATTACH_NONE,
            XmNnavigationType,      XmTAB_GROUP,
            XmNtraversalOn,         FALSE,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    // Interface status indicators, Bottom right corner
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
            XmNleftWidget,          log_indicator,
            XmNrightAttachment,     XmATTACH_NONE,
            XmNnavigationType,      XmTAB_GROUP,
            XmNtraversalOn,         FALSE,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    // Message box, on Top left
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
            XmNbottomAttachment,    XmATTACH_WIDGET,
            XmNbottomWidget,        text3,
            XmNleftAttachment,      XmATTACH_FORM,
            XmNrightAttachment,     XmATTACH_NONE,
            XmNnavigationType,      XmTAB_GROUP,
            XmNtraversalOn,         FALSE,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    // Coordinate display box, Top right
    text2 = XtVaCreateWidget("create_appshell text_output2",
            xmTextFieldWidgetClass,
            form,
            XmNeditable,            FALSE,
            XmNcursorPositionVisible, FALSE,
            XmNsensitive,           STIPPLE,
            XmNshadowThickness,     1,
            XmNcolumns,             35,
            XmNwidth,   do_dbstatus?((37*FONT_WIDTH)+2):((24*FONT_WIDTH)+2),
            XmNtopAttachment,       XmATTACH_NONE,
            XmNbottomAttachment,    XmATTACH_WIDGET,
            XmNbottomWidget,        text3,
            XmNleftAttachment,      XmATTACH_WIDGET,
            XmNleftWidget,          text,
            XmNrightAttachment,     XmATTACH_NONE,
            XmNnavigationType,      XmTAB_GROUP,
            XmNtraversalOn,         FALSE,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

#else

    // Message box, on left
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
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    // Coordinate display box, 2nd from left
    text2 = XtVaCreateWidget("create_appshell text_output2",
            xmTextFieldWidgetClass,
            form,
            XmNeditable,            FALSE,
            XmNcursorPositionVisible, FALSE,
            XmNsensitive,           STIPPLE,
            XmNshadowThickness,     1,
            XmNcolumns,             35,
            XmNwidth,   do_dbstatus?((37*FONT_WIDTH)+2):((24*FONT_WIDTH)+2),
            XmNtopAttachment,       XmATTACH_NONE,
            XmNbottomAttachment,    XmATTACH_FORM,
            XmNleftAttachment,      XmATTACH_WIDGET,
            XmNleftWidget,          text,
            XmNrightAttachment,     XmATTACH_NONE,
            XmNnavigationType,      XmTAB_GROUP,
            XmNtraversalOn,         FALSE,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    // Quantity of stations box, 3rd from left
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
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    // Zoom level box, 3rd from right
    text4 = XtVaCreateWidget("create_appshell text_output4",
            xmTextFieldWidgetClass,
            form,
            XmNeditable,            FALSE,
            XmNcursorPositionVisible, FALSE,
            XmNsensitive,           STIPPLE,
            XmNshadowThickness,     1,
            XmNcolumns,             10,
            XmNwidth,               ((8*FONT_WIDTH)+2),
            XmNtopAttachment,       XmATTACH_NONE,
            XmNbottomAttachment,    XmATTACH_FORM,
            XmNleftAttachment,      XmATTACH_WIDGET,
            XmNleftWidget,          text3,
            XmNrightAttachment,     XmATTACH_NONE,
            XmNnavigationType,      XmTAB_GROUP,
            XmNtraversalOn,         FALSE,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    // Log indicator box, 2nd from right
    log_indicator = XtVaCreateWidget(langcode("BBARSTA043"),
            xmTextFieldWidgetClass,
            form,
            XmNeditable,            FALSE,
            XmNcursorPositionVisible, FALSE,
            XmNsensitive,           STIPPLE,
            XmNshadowThickness,     1,
            XmNcolumns,             8,
            XmNwidth,               ((8*FONT_WIDTH)),
            XmNtopAttachment,       XmATTACH_NONE,
            XmNbottomAttachment,    XmATTACH_FORM,
            XmNleftAttachment,      XmATTACH_WIDGET,
            XmNleftWidget,          text4,
            XmNrightAttachment,     XmATTACH_NONE,
            XmNnavigationType,      XmTAB_GROUP,
            XmNtraversalOn,         FALSE,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    // Interface status indicators, on right
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
            XmNleftWidget,          log_indicator,
            XmNrightAttachment,     XmATTACH_NONE,
            XmNnavigationType,      XmTAB_GROUP,
            XmNtraversalOn,         FALSE,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

#endif // USE_TWO_STATUS_LINES

    // The separator goes on top of the text box no matter how
    // many lines the status bar is, so I'm putting if after the
    // endif statement  - DCR
    sep = XtVaCreateManagedWidget("create_appshell sep6",
            xmSeparatorGadgetClass,
            form,
            XmNorientation,         XmHORIZONTAL,
            XmNtopAttachment,       XmATTACH_NONE,
            XmNbottomAttachment,    XmATTACH_WIDGET,
            XmNbottomWidget,        text,
            XmNleftAttachment,      XmATTACH_FORM,
            XmNrightAttachment,     XmATTACH_FORM,
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    // Map drawing area
    da_width = (Dimension)screen_width;
    da_height = (Dimension)screen_height;
    da = XtVaCreateWidget("create_appshell da",
            xmDrawingAreaWidgetClass,
            form,
            XmNwidth,               da_width,
            XmNheight,              da_height,
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
    XtSetArg(al[ac], XmNfontList,        fontlist1);             ac++;



#ifdef SWAP_MOUSE_BUTTONS
    //
    // Motif 2.2.2 (and perhaps earlier, back to version 2.0) has a
    // problem where the XmNmenuPost doesn't work properly for
    // modifiers (CapsLock/ScrollLock/NumLock/etc).  We're reverting
    // back to the Motif 1.x method of doing things.  It works!
    //
    //XtSetArg(al[ac], XmNmenuPost, "<Btn1Down>"); ac++;  // Set for popup menu on button 1
    XtSetArg(al[ac], XmNwhichButton, 1); ac++;  // Enable popup menu on button 1

#else   // SWAP_MOUSE_BUTTONS
    //
    // Motif 2.2.2 (and perhaps earlier, back to version 2.0) has a
    // problem where the XmNmenuPost doesn't work properly for
    // modifiers (CapsLock/ScrollLock/NumLock/etc).  We're reverting
    // back to the Motif 1.x method of doing things.  It works!
    //
    //XtSetArg(al[ac], XmNmenuPost, "<Btn3Down>"); ac++;  // Set for popup menu on button 3
    XtSetArg(al[ac], XmNwhichButton, 3); ac++;  // Enable popup menu on button 3
#endif  // SWAP_MOUSE_BUTTONS


    XtSetArg(al[ac], XmNnavigationType, XmTAB_GROUP); ac++;
    XtSetArg(al[ac], XmNtraversalOn, TRUE); ac++;
    XtSetArg(al[ac], XmNtearOffModel, XmTEAR_OFF_ENABLED); ac++;

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
    XtSetArg(al[ac], XmNfontList,        fontlist1);             ac++;


    // "Options"
    (void)XtCreateManagedWidget(langcode("POPUPMA001"),
            xmLabelWidgetClass,
            right_menu_popup,
            al,
            ac);
    (void)XtCreateManagedWidget("create_appshell sep7",
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
    XtSetArg(al[ac], XmNfontList,        fontlist1);             ac++;
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
    XtSetArg(al[ac], XmNfontList,        fontlist1);             ac++;
    station_info=XtCreateManagedWidget(langcode("POPUPMA015"),
            xmPushButtonGadgetClass,
            right_menu_popup,
            al,
            ac);
    XtAddCallback(station_info,XmNactivateCallback,Station_info,NULL);

    // Send Message To
    ac = 0;
    XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
    XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
    XtSetArg(al[ac], XmNnavigationType, XmTAB_GROUP); ac++;
    XtSetArg(al[ac], XmNtraversalOn, TRUE); ac++;
    XtSetArg(al[ac], XmNmnemonic, langcode_hotkey("PULDNMG001")); ac++;
    XtSetArg(al[ac], XmNfontList,        fontlist1);             ac++;
    send_message_to=XtCreateManagedWidget(langcode("PULDNMG001"),
            xmPushButtonGadgetClass,
            right_menu_popup,
            al,
            ac);
    XtAddCallback(send_message_to,XmNactivateCallback,Station_info,"4");

    // Map Bookmarks
    ac = 0;
    XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
    XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
    XtSetArg(al[ac], XmNnavigationType, XmTAB_GROUP); ac++;
    XtSetArg(al[ac], XmNtraversalOn, TRUE); ac++;
    XtSetArg(al[ac], XmNmnemonic, langcode_hotkey("PULDNMP012")); ac++;
    XtSetArg(al[ac], XmNfontList,        fontlist1);             ac++;
    jump_button2=XtCreateManagedWidget(langcode("PULDNMP012"),
            xmPushButtonGadgetClass,
            right_menu_popup,
            al,
            ac);
    XtAddCallback(jump_button2,XmNactivateCallback,Jump_location, NULL);
 

    ac = 0;
    XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
    XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
    XtSetArg(al[ac], XmNnavigationType, XmTAB_GROUP); ac++;
    XtSetArg(al[ac], XmNtraversalOn, TRUE); ac++;
    XtSetArg(al[ac], XmNtearOffModel, XmTEAR_OFF_ENABLED); ac++;
    XtSetArg(al[ac], XmNfontList,        fontlist1);             ac++;

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
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    // Zoom in"
    ac = 0;
    XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
    XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
    XtSetArg(al[ac], XmNnavigationType, XmTAB_GROUP); ac++;
    XtSetArg(al[ac], XmNtraversalOn, TRUE); ac++;
    XtSetArg(al[ac], XmNmnemonic, langcode_hotkey("POPUPMA002")); ac++;
    XtSetArg(al[ac], XmNfontList,        fontlist1);             ac++;

    zoom_in=XtCreateManagedWidget(langcode("POPUPMA002"),
            xmPushButtonGadgetClass,
            zoom_sub,
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
    XtSetArg(al[ac], XmNfontList,        fontlist1);             ac++;

    zoom_out=XtCreateManagedWidget(langcode("POPUPMA003"),
            xmPushButtonGadgetClass,
            zoom_sub,
            al,
            ac);
    XtAddCallback(zoom_out,XmNactivateCallback,Zoom_out,NULL);

    // "1"
    ac = 0;
    XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
    XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
    XtSetArg(al[ac], XmNnavigationType, XmTAB_GROUP); ac++;
    XtSetArg(al[ac], XmNtraversalOn, TRUE); ac++;
    XtSetArg(al[ac], XmNmnemonic, langcode_hotkey("POPUPMA005")); ac++;
    XtSetArg(al[ac], XmNfontList,        fontlist1);             ac++;

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
    XtSetArg(al[ac], XmNfontList,        fontlist1);             ac++;

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
    XtSetArg(al[ac], XmNfontList,        fontlist1);             ac++;

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
    XtSetArg(al[ac], XmNfontList,        fontlist1);             ac++;

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
    XtSetArg(al[ac], XmNfontList,        fontlist1);             ac++;

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
    XtSetArg(al[ac], XmNfontList,        fontlist1);             ac++;

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
    XtSetArg(al[ac], XmNfontList,        fontlist1);             ac++;

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
    XtSetArg(al[ac], XmNfontList,        fontlist1);             ac++;

    zl8=XtCreateManagedWidget(langcode("POPUPMA035"),   // 10% out
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
    XtSetArg(al[ac], XmNfontList,        fontlist1);             ac++;

    zl9=XtCreateManagedWidget(langcode("POPUPMA036"),   // 10% in
            xmPushButtonGadgetClass,
            zoom_sub,
            al,
            ac);
    XtAddCallback(zl9,XmNactivateCallback,Zoom_level,"9");

    // "Custom Zoom Level"
    ac = 0;
    XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
    XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
    XtSetArg(al[ac], XmNnavigationType, XmTAB_GROUP); ac++;
    XtSetArg(al[ac], XmNtraversalOn, TRUE); ac++;
    XtSetArg(al[ac], XmNmnemonic, 'i'); ac++;
    XtSetArg(al[ac], XmNfontList,        fontlist1);             ac++;

    zlC=XtCreateManagedWidget(langcode("POPUPMA034"),
            xmPushButtonGadgetClass,
            zoom_sub,
            al,
            ac);
    XtAddCallback(zlC,XmNactivateCallback,Zoom_level,"10");

    // "Last map pos/zoom"
    ac = 0;
    XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
    XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
    XtSetArg(al[ac], XmNnavigationType, XmTAB_GROUP); ac++;
    XtSetArg(al[ac], XmNtraversalOn, TRUE); ac++;
    XtSetArg(al[ac], XmNmnemonic, langcode_hotkey("POPUPMA016")); ac++;
    XtSetArg(al[ac], XmNfontList,        fontlist1);             ac++;

    last_loc=XtCreateManagedWidget(langcode("POPUPMA016"),
            xmPushButtonGadgetClass,
            right_menu_popup,
            al,
            ac);
    XtAddCallback(last_loc,XmNactivateCallback,Last_location,NULL);

    (void)XtCreateManagedWidget("create_appshell sep7a",
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
    XtSetArg(al[ac], XmNfontList,        fontlist1);             ac++;
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
    XtSetArg(al[ac], XmNfontList,        fontlist1);             ac++;
    modify_object=XtCreateManagedWidget(langcode("POPUPMA019"),
            xmPushButtonGadgetClass,
            right_menu_popup,
            al,
            ac);
    XtAddCallback(modify_object,XmNactivateCallback,Station_info,"1");

    ac = 0;
    XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
    XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
    XtSetArg(al[ac], XmNnavigationType, XmTAB_GROUP); ac++;
    XtSetArg(al[ac], XmNtraversalOn, TRUE); ac++;
    XtSetArg(al[ac], XmNtearOffModel, XmTEAR_OFF_ENABLED); ac++;
    XtSetArg(al[ac], XmNfontList,        fontlist1);             ac++;

    // Display a list of predefined SAR or Public service event objects.
    sar_object_sub=XmCreatePulldownMenu(right_menu_popup,
            "create_appshell sar_object_sub",
            al,
            ac);

    // "Predefined Objects"
    sar_object_menu=XtVaCreateManagedWidget(langcode("POPUPMA045"),
            xmCascadeButtonGadgetClass,
            right_menu_popup,
            XmNsubMenuId,sar_object_sub,
//            XmNmnemonic,langcode_hotkey("POPUPMA045"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    BuildPredefinedSARMenu_UI(&sar_object_sub);

    ac = 0;
    XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
    XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
    XtSetArg(al[ac], XmNnavigationType, XmTAB_GROUP); ac++;
    XtSetArg(al[ac], XmNtraversalOn, TRUE); ac++;
    XtSetArg(al[ac], XmNfontList,        fontlist1);             ac++;

    XtCreateManagedWidget("create_appshell sep7b",
            xmSeparatorWidgetClass,
            right_menu_popup,
            al,
            ac);

    XtCreateManagedWidget("create_appshell sep7c",
            xmSeparatorWidgetClass,
            right_menu_popup,
            al,
            ac);

    ac = 0;
    XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
    XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
    XtSetArg(al[ac], XmNnavigationType, XmTAB_GROUP); ac++;
    XtSetArg(al[ac], XmNtraversalOn, TRUE); ac++;
    XtSetArg(al[ac], XmNtearOffModel, XmTEAR_OFF_ENABLED); ac++;
    XtSetArg(al[ac], XmNfontList,        fontlist1);             ac++;

    pan_sub=XmCreatePulldownMenu(right_menu_popup,
            "create_appshell pan sub",
            al,
            ac);

    // "Pan"
//    pan_menu=XtVaCreateManagedWidget(langcode(""),
    pan_menu=XtVaCreateManagedWidget("Pan",
            xmCascadeButtonGadgetClass,
            right_menu_popup,
            XmNsubMenuId,pan_sub,
//            XmNmnemonic,langcode_hotkey("POPUPMA004"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    // "Pan Up"
    ac = 0;
    XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
    XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
    XtSetArg(al[ac], XmNnavigationType, XmTAB_GROUP); ac++;
    XtSetArg(al[ac], XmNtraversalOn, TRUE); ac++;
    XtSetArg(al[ac], XmNmnemonic, langcode_hotkey("POPUPMA011")); ac++;
    XtSetArg(al[ac], XmNfontList,        fontlist1);             ac++;
    pan_up=XtCreateManagedWidget(langcode("POPUPMA011"),
            xmPushButtonGadgetClass,
            pan_sub,
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
    XtSetArg(al[ac], XmNfontList,        fontlist1);             ac++;
    pan_left=XtCreateManagedWidget(langcode("POPUPMA013"),
            xmPushButtonGadgetClass,
            pan_sub,
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
    XtSetArg(al[ac], XmNfontList,        fontlist1);             ac++;
    pan_right=XtCreateManagedWidget(langcode("POPUPMA014"),
            xmPushButtonGadgetClass,
            pan_sub,
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
    XtSetArg(al[ac], XmNfontList,        fontlist1);             ac++;
    pan_down=XtCreateManagedWidget(langcode("POPUPMA012"),
            xmPushButtonGadgetClass,
            pan_sub,
            al,
            ac);
    //pan_down=XtVaCreateManagedWidget("create_appshell arrow2",
    //    xmArrowButtonGadgetClass,
    //    right_menu_popup,
    //    XmNarrowDirection,  XmARROW_DOWN,
    //    NULL);
    XtAddCallback(pan_down,XmNactivateCallback,Pan_down,NULL);

    XtCreateManagedWidget("create_appshell sep7d",
            xmSeparatorWidgetClass,
            right_menu_popup,
            al,
            ac);

    move_my_sub=XmCreatePulldownMenu(right_menu_popup,
            "create_appshell move_my sub",
            al,
            ac);

    move_my_menu=XtVaCreateManagedWidget(langcode("POPUPMA025"),
            xmCascadeButtonGadgetClass,
            right_menu_popup,
            XmNsubMenuId,move_my_sub,
            XmNmnemonic,langcode_hotkey("POPUPMA025"),
            XmNfontList, fontlist1,
            MY_FOREGROUND_COLOR,
            MY_BACKGROUND_COLOR,
            NULL);

    // "Move my station here"
    ac = 0;
    XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
    XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
    XtSetArg(al[ac], XmNnavigationType, XmTAB_GROUP); ac++;
    XtSetArg(al[ac], XmNtraversalOn, TRUE); ac++;
    XtSetArg(al[ac], XmNmnemonic, langcode_hotkey("POPUPMA025")); ac++;
    XtSetArg(al[ac], XmNfontList,        fontlist1);             ac++;
    setmyposition=XtCreateManagedWidget(langcode("POPUPMA025"),
            xmPushButtonGadgetClass,
            move_my_sub,
            al,
            ac);
    XtAddCallback(setmyposition,XmNactivateCallback,SetMyPosition,"1");


//-------------------------------------------------------------------------


    /* mouse tracking */
    XtAddEventHandler(da,LeaveWindowMask,FALSE,ClearTrackMouse,(XtPointer)text2);
    XtAddEventHandler(da,PointerMotionMask,FALSE,TrackMouse,(XtPointer)text2);

    // Popup menus
    XtAddEventHandler(da, ButtonPressMask, FALSE, (XtEventHandler)Mouse_button_handler, NULL);
    //XtAddEventHandler(da,ButtonReleaseMask,FALSE,(XtEventHandler)Mouse_button_handler,NULL);


    // If adding any more widgets here, increase the size of the
    // children[] array above.
    ac = 0;
    children[ac++] = text;
    children[ac++] = text2;
    children[ac++] = text3;
    children[ac++] = text4;
    children[ac++] = log_indicator;
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

// We get this error on some systems if XtRealizeWidget() is called
// without setting width/height values first:
// "Error: Shell widget xastir has zero width and/or height"

    XtRealizeWidget (appshell);


    // Flush all pending requests to the X server.
    XFlush(display);


    create_gc(da);

    // Fill the drawing area with the background color.
    (void)XSetForeground(XtDisplay(da),gc,MY_BG_COLOR); // Not a mistake!
    (void)XSetBackground(XtDisplay(da),gc,MY_BG_COLOR);
    (void)XFillRectangle(XtDisplay(appshell),
        XtWindow(da),
        gc,
        0,
        0,
        (unsigned int)screen_width,
        (unsigned int)screen_height);


    XtAddCallback (da, XmNinputCallback,  da_input,NULL);
    XtAddCallback (da, XmNresizeCallback, da_resize,NULL);
    XtAddCallback (da, XmNexposeCallback, da_expose,(XtPointer)text);


    // Don't discard events in X11 queue, but wait for the X11
    // server to catch up.
    (void)XSync(XtDisplay(appshell), False);


    // Send the window manager hints
//    XSetWMHints(display, XtWindow(appshell), wm_hints);


    // Set up the icon
    icon_pixmap = XCreateBitmapFromData(display,
                        XtWindow(appshell),
                        (char *)icon_bits,
                        icon_width,     // icon_bitmap_width,
                        icon_height);   // icon_bitmap_height

    XSetStandardProperties(display,
        XtWindow(appshell),
        title ? title: "Xastir", // window name
        "Xastir",       // icon name
        icon_pixmap,    // pixmap for icon
        0, 0,           // argv and argc for restarting
        NULL);          // size hints

    if (title)
        free(title);

    if (track_me)
        XmToggleButtonSetState(trackme_button,TRUE,TRUE);
    else
        XmToggleButtonSetState(trackme_button,FALSE,TRUE);



    // Flush all pending requests to the X server.
    XFlush(display);

    // Don't discard events in X11 queue, but wait for the X11
    // server to catch up.
    (void)XSync(XtDisplay(appshell), False);



    // Reset the minimum window size so that we can adjust the
    // window downwards again, but only down to size 61.  If we go
    // any smaller height-wise then we end up getting segfaults,
    // probably because we're trying to update some widgets that
    // aren't visible at that point.
    //
    XtVaSetValues(appshell,
        XmNminWidth,  61,
        XmNminHeight, 61,
        XmNwidth,     my_appshell_width,
        XmNheight,    my_appshell_height,
//        XmNx,         screen_x_offset,    // Doesn't work here
//        XmNy,         screen_y_offset,    // Doesn't work here
        NULL);
// Lincity method of locking down the min_width/height when
// instantiating the window, then releasing it later:
// http://pingus.seul.org/~grumbel/tmp/lincity-source/lcx11_8c-source.html



     // Actually show the draw and show the window on the display
    XMapRaised(XtDisplay(appshell), XtWindow(appshell));


    // Free the allocated struct.  We won't need it again.
//    XFree(wm_hints);    // We're not currently using this struct

    if(debug_level & 8)
        fprintf(stderr,"Create appshell stop\n");
}   // end of create_appshell()





void BuildPredefinedSARMenu_UI(Widget *parent_menu) {
    int i;   // number of items in menu
    int ac;  // number of arguments
    Arg al[100];  // arguments


    // Set standard menu item arguments to use with each widget.
    ac = 0;
    XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
    XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
    XtSetArg(al[ac], XmNnavigationType, XmTAB_GROUP); ac++;
    XtSetArg(al[ac], XmNtraversalOn, TRUE); ac++;
    XtSetArg(al[ac], XmNfontList, fontlist1); ac++;

    // Before building menu, make sure that any existing menu items are removed
    // this allows the menu to be changed on the fly while the program is running.
    //
    for (i = 0; i < MAX_NUMBER_OF_PREDEFINED_OBJECTS; i++) {
        if (predefined_object_menu_items[i] != NULL) {
             XtDestroyWidget(predefined_object_menu_items[i]);
             predefined_object_menu_items[i] = NULL;
        }
    }
    // Now build a menu item for each entry in the predefinedObjects array.
    for (i = 0; i < number_of_predefined_objects; i++) {

        // Walk through array of predefined objects and
        // build a menu item for each predefined object.
        //
        if (predefinedObjects[i].show_on_menu == 1) {

            // Some predefined objects are hidden to allow construction
            // of two predefined objects in the same place at the same
            // time with one menu item.

            if(debug_level & 1)
                fprintf(stderr,
                    "Menu item with name: %s and index_of_child=%d\n",
                    predefinedObjects[i].menu_call, predefinedObjects[i].index_of_child);

            predefined_object_menu_items[i]=XtCreateManagedWidget(predefinedObjects[i].menu_call,
                xmPushButtonGadgetClass,
                *parent_menu,
                al,
                ac);

            XtAddCallback(predefined_object_menu_items[i],
                XmNactivateCallback,
                Create_SAR_Object,
                (int *)predefinedObjects[i].index);

            if (predefinedObjects[i].index_of_child > -1) {

                // This second callback allows stacking of two
                // objects such as a PLS with 0.25 and 0.5 and a
                // PLS_ with 0.75 and 1.0 mile probability circles.
                //
                if (predefinedObjects[i].index_of_child < number_of_predefined_objects) {
                    XtAddCallback(predefined_object_menu_items[i],
                        XmNactivateCallback,
                        Create_SAR_Object,
                        (int *)predefinedObjects[predefinedObjects[i].index_of_child].index);
                }
            }
        }
    }
}





void create_gc(Widget w) {
    XGCValues values;
    Display *my_display = XtDisplay(w);
    int mask = 0;
    Pixmap pix;
    unsigned int _w, _h;
    int _xh, _yh;
    char xbm_path[500];
    int ret_val;


    if (debug_level & 8)
        fprintf(stderr,"Create gc start\n");

    if (gc != 0)
        return;

    memset(&values, 0, sizeof(values));

    // Allocate colors
    // Note that the names here are the ones given in xastir.rgb
    colors[0x00] = GetPixelByName(w,"DarkGreen");  // was darkgreen (same)
    colors[0x01] = GetPixelByName(w,"purple");
    colors[0x02] = GetPixelByName(w,"DarkGreen");  // was darkgreen (same)
    colors[0x03] = GetPixelByName(w,"cyan");
    colors[0x04] = GetPixelByName(w,"brown");
    colors[0x05] = GetPixelByName(w,"plum");       // light magenta
    colors[0x06] = GetPixelByName(w,"orange");
    colors[0x07] = GetPixelByName(w,"darkgray");
    colors[0x08] = GetPixelByName(w,"black");      // Foreground font color
    colors[0x09] = GetPixelByName(w,"blue");
    colors[0x0a] = GetPixelByName(w,"green");              // PHG (old)
    colors[0x0b] = GetPixelByName(w,"mediumorchid"); // light purple
    colors[0x0c] = GetPixelByName(w,"red");
    colors[0x0d] = GetPixelByName(w,"magenta");
    colors[0x0e] = GetPixelByName(w,"yellow");
    colors[0x0f] = GetPixelByName(w,"white");              //
    colors[0x10] = GetPixelByName(w,"black");
    colors[0x11] = GetPixelByName(w,"black");
    colors[0x12] = GetPixelByName(w,"black");
    colors[0x13] = GetPixelByName(w,"black");
    colors[0x14] = GetPixelByName(w,"lightgray");
    colors[0x15] = GetPixelByName(w,"magenta");
    colors[0x16] = GetPixelByName(w,"mediumorchid"); // light purple
    colors[0x17] = GetPixelByName(w,"lightblue");
    colors[0x18] = GetPixelByName(w,"purple");
    colors[0x19] = GetPixelByName(w,"orange2");    // light orange
    colors[0x1a] = GetPixelByName(w,"SteelBlue");
    colors[0x20] = GetPixelByName(w,"white");

    // Area object colors.  Order must not be changed. If beginning moves,
    // update draw_area and draw_map.
    // High
    colors[0x21] = GetPixelByName(w,"black");   // AREA_BLACK_HI
    colors[0x22] = GetPixelByName(w,"blue");    // AREA_BLUE_HI
    colors[0x23] = GetPixelByName(w,"green");   // AREA_GREEN_HI
    colors[0x24] = GetPixelByName(w,"cyan3");    // AREA_CYAN_HI
    colors[0x25] = GetPixelByName(w,"red");     // AREA_RED_HI
    colors[0x26] = GetPixelByName(w,"magenta"); // AREA_VIOLET_HI
    colors[0x27] = GetPixelByName(w,"yellow");  // AREA_YELLOW_HI
    colors[0x28] = GetPixelByName(w,"gray35");  // AREA_GRAY_HI
    // Low
    colors[0x29] = GetPixelByName(w,"gray27");   // AREA_BLACK_LO
    colors[0x2a] = GetPixelByName(w,"blue4");    // AREA_BLUE_LO
    colors[0x2b] = GetPixelByName(w,"green4");   // AREA_GREEN_LO
    colors[0x2c] = GetPixelByName(w,"cyan4");    // AREA_CYAN_LO
    colors[0x2d] = GetPixelByName(w,"red4");     // AREA_RED_LO
    colors[0x2e] = GetPixelByName(w,"magenta4"); // AREA_VIOLET_LO
    colors[0x2f] = GetPixelByName(w,"yellow4");  // AREA_YELLOW_LO
    colors[0x30] = GetPixelByName(w,"gray53"); // AREA_GRAY_LO

    colors[0x40] = GetPixelByName(w,"yellow");     // symbols ...
    colors[0x41] = GetPixelByName(w,"DarkOrange3");
    colors[0x42] = GetPixelByName(w,"purple");
    colors[0x43] = GetPixelByName(w,"gray80");
    colors[0x44] = GetPixelByName(w,"red3");
    colors[0x45] = GetPixelByName(w,"brown1");
    colors[0x46] = GetPixelByName(w,"brown3");
    colors[0x47] = GetPixelByName(w,"blue4");
    colors[0x48] = GetPixelByName(w,"DeepSkyBlue");
    colors[0x49] = GetPixelByName(w,"DarkGreen");
    colors[0x4a] = GetPixelByName(w,"red2");
    colors[0x4b] = GetPixelByName(w,"green3");
    colors[0x4c] = GetPixelByName(w,"MediumBlue");
    colors[0x4d] = GetPixelByName(w,"white");
    colors[0x4e] = GetPixelByName(w,"gray53");
    colors[0x4f] = GetPixelByName(w,"gray35");
    colors[0x50] = GetPixelByName(w,"gray27");
    colors[0x51] = GetPixelByName(w,"black");      // ... symbols

    colors[0x52] = GetPixelByName(w,"LimeGreen");  // PHG, symbols

    colors[0xfe] = GetPixelByName(w,"pink");

    // map solid colors
    colors[0x60] = GetPixelByName(w,"HotPink");
    colors[0x61] = GetPixelByName(w,"RoyalBlue");
    colors[0x62] = GetPixelByName(w,"orange3");
    colors[0x63] = GetPixelByName(w,"yellow3");
    colors[0x64] = GetPixelByName(w,"ForestGreen");
    colors[0x65] = GetPixelByName(w,"DodgerBlue");
    colors[0x66] = GetPixelByName(w,"cyan2");
    colors[0x67] = GetPixelByName(w,"plum2");
    colors[0x68] = GetPixelByName(w,"MediumBlue"); // was blue3 (the same!)
    colors[0x69] = GetPixelByName(w,"gray86");

    // These colors added to make it possible to color local shapefile tiger
    //  maps similar to on-line ones.
    colors[0x70] = GetPixelByName(w,"RosyBrown2");
    colors[0x71] = GetPixelByName(w,"gray81");
    colors[0x72] = GetPixelByName(w,"tgr_park_1");
    colors[0x73] = GetPixelByName(w,"tgr_city_1");
    colors[0x74] = GetPixelByName(w,"tgr_forest_1");
    colors[0x75] = GetPixelByName(w,"tgr_water_1");

    // tracking trail colors
    // set color for your own station with  #define MY_TRAIL_COLOR  in db.c
    trail_colors[0x00] = GetPixelByName(w,"yellow");
    trail_colors[0x01] = GetPixelByName(w,"blue");
    trail_colors[0x02] = GetPixelByName(w,"green");
    trail_colors[0x03] = GetPixelByName(w,"red");
    trail_colors[0x04] = GetPixelByName(w,"magenta");
    trail_colors[0x05] = GetPixelByName(w,"black");
    trail_colors[0x06] = GetPixelByName(w,"white");
    trail_colors[0x07] = GetPixelByName(w,"DarkOrchid");
    trail_colors[0x08] = GetPixelByName(w,"purple");      // very similar to DarkOrchid...
    trail_colors[0x09] = GetPixelByName(w,"OrangeRed");
    trail_colors[0x0a] = GetPixelByName(w,"brown");
    trail_colors[0x0b] = GetPixelByName(w,"DarkGreen");    // was darkgreen (same)
    trail_colors[0x0c] = GetPixelByName(w,"MediumBlue");
    trail_colors[0x0d] = GetPixelByName(w,"ForestGreen");
    trail_colors[0x0e] = GetPixelByName(w,"chartreuse");
    trail_colors[0x0f] = GetPixelByName(w,"cornsilk");
    trail_colors[0x10] = GetPixelByName(w,"LightCyan");
    trail_colors[0x11] = GetPixelByName(w,"cyan");
    trail_colors[0x12] = GetPixelByName(w,"DarkSlateGray");
    trail_colors[0x13] = GetPixelByName(w,"NavyBlue");
    trail_colors[0x14] = GetPixelByName(w,"DarkOrange3");
    trail_colors[0x15] = GetPixelByName(w,"gray27");
    trail_colors[0x16] = GetPixelByName(w,"RoyalBlue");
    trail_colors[0x17] = GetPixelByName(w,"yellow2");
    trail_colors[0x18] = GetPixelByName(w,"DodgerBlue");
    trail_colors[0x19] = GetPixelByName(w,"cyan2");
    trail_colors[0x1a] = GetPixelByName(w,"MediumBlue"); // was blue3 (the same!)
    trail_colors[0x1b] = GetPixelByName(w,"gray86");
    trail_colors[0x1c] = GetPixelByName(w,"SteelBlue");
    trail_colors[0x1d] = GetPixelByName(w,"PaleGreen");
    trail_colors[0x1e] = GetPixelByName(w,"RosyBrown");
    trail_colors[0x1f] = GetPixelByName(w,"DeepSkyBlue");

    values.background=GetPixelByName(w,"darkgray");

    gc = XCreateGC(my_display, XtWindow(w), mask, &values);

    // Load a new font into the GC for the station font
    Load_station_font();

    gc_tint = XCreateGC(my_display, XtWindow(w), mask, &values);

    gc_stipple = XCreateGC(my_display, XtWindow(w), mask, &values);

    gc_bigfont = XCreateGC(my_display, XtWindow(w), mask, &values);

    pix =  XCreatePixmap(XtDisplay(w), RootWindowOfScreen(XtScreen(w)), 20, 20, 1);
    values.function = GXcopy;
    gc2 = XCreateGC(XtDisplay(w), pix,GCForeground|GCBackground|GCFunction, &values);

    pixmap=XCreatePixmap(XtDisplay(w),
                        DefaultRootWindow(XtDisplay(w)),
                        (unsigned int)screen_width,(unsigned int)screen_height,
                        DefaultDepthOfScreen(XtScreen(w)));

    pixmap_final=XCreatePixmap(XtDisplay(w),
                        DefaultRootWindow(XtDisplay(w)),
                        (unsigned int)screen_width,(unsigned int)screen_height,
                        DefaultDepthOfScreen(XtScreen(w)));

    pixmap_alerts=XCreatePixmap(XtDisplay(w),
                        DefaultRootWindow(XtDisplay(w)),
                        (unsigned int)screen_width,(unsigned int)screen_height,
                        DefaultDepthOfScreen(XtScreen(w)));

    xastir_snprintf(xbm_path, sizeof(xbm_path), "%s/%s", SYMBOLS_DIR, "2x2.xbm");
    ret_val = XReadBitmapFile(XtDisplay(w), DefaultRootWindow(XtDisplay(w)),
                    xbm_path, &_w, &_h, &pixmap_50pct_stipple, &_xh, &_yh);

    if (ret_val != 0) {
        fprintf(stderr,"XReadBitmapFile() failed: Bitmap not found? %s\n",xbm_path);
        exit(1);    // 2x2.xbm couldn't be loaded
    }

    xastir_snprintf(xbm_path, sizeof(xbm_path), "%s/%s", SYMBOLS_DIR, "25pct.xbm");
    ret_val = XReadBitmapFile(XtDisplay(w), DefaultRootWindow(XtDisplay(w)),
                    xbm_path, &_w, &_h, &pixmap_25pct_stipple, &_xh, &_yh);

    if (ret_val != 0) {
        fprintf(stderr,"XReadBitmapFile() failed: Bitmap not found? %s\n",xbm_path);
        exit(1);    // 25pct.xbm couldn't be loaded
    }

    xastir_snprintf(xbm_path, sizeof(xbm_path), "%s/%s", SYMBOLS_DIR, "13pct.xbm");
    ret_val = XReadBitmapFile(XtDisplay(w), DefaultRootWindow(XtDisplay(w)),
                    xbm_path, &_w, &_h, &pixmap_13pct_stipple, &_xh, &_yh);

    if (ret_val != 0) {
        fprintf(stderr,"XReadBitmapFile() failed: Bitmap not found? %s\n",xbm_path);
        exit(1);    // 13pct.xbm couldn't be loaded
    }

    xastir_snprintf(xbm_path, sizeof(xbm_path), "%s/%s", SYMBOLS_DIR, "alert.xbm");
    ret_val = XReadBitmapFile(XtDisplay(w), DefaultRootWindow(XtDisplay(w)),
                    xbm_path, &_w, &_h, &pixmap_wx_stipple, &_xh, &_yh);

    if (ret_val != 0) {
        fprintf(stderr,"XReadBitmapFile() failed: Bitmap not found? %s\n",xbm_path);
        exit(1);    // alert.xbm couldn't be loaded
    }

    display_up=1;

    wait_to_redraw=0;

    if (debug_level & 8)
        fprintf(stderr,"Create gc stop\n");
}   // create_gc()





// This routine just copies an area from pixmap_final to the
// display, so we won't go through all the trouble of making this
// interruptable.  Just get on with it, perform the operation, and
// return to X.  If it did any map drawing, we'd make it
// interruptable.
//
void da_expose(Widget w, /*@unused@*/ XtPointer client_data, XtPointer call_data) {
    Dimension width, height, margin_width, margin_height;
    XmDrawingAreaCallbackStruct *db = (XmDrawingAreaCallbackStruct *)call_data;
    XExposeEvent *event = (XExposeEvent *) db->event;
    unsigned char   unit_type;

    //fprintf(stderr,"Expose event\n");*/

    /* Call a routine to create a Graphics Context */
    create_gc(w);

    /* First get the various dimensions */
    XtVaGetValues(w,
        XmNwidth, &width,
        XmNheight, &height,
        XmNmarginWidth, &margin_width,
        XmNmarginHeight, &margin_height,
        XmNunitType, &unit_type,
        NULL);

    (void)XCopyArea(XtDisplay(w),
        pixmap_final,
        XtWindow(w),
        gc,
        event->x,
        event->y,
        event->width,
        event->height,
        event->x,
        event->y);

    // We just refreshed the screen, so don't try to erase any
    // zoom-in boxes via XOR.
    zoom_box_x1 = -1;
}





// The work function for resizing.  This one will be called by
// UpdateTime if certain flags have been set my da_resize.  This
// function and the functions it calls that are CPU intensive should
// be made interruptable:  They should check interrupt_drawing_now
// flag periodically and exit nicely if it is set.
//
void da_resize_execute(Widget w) {
    Dimension width, height;


    busy_cursor(appshell);

    // Reset the flags that may have brought us here.
    interrupt_drawing_now = 0;
    request_resize = 0;

    if (XtIsRealized(w)){
        /* First get the various dimensions */
        XtVaGetValues(w,
            XmNwidth,   &width,
            XmNheight,  &height,
            NULL);

        screen_width  = (long)width;
        screen_height = (long)height;
        XtVaSetValues(w,
            XmNwidth,  width,
            XmNheight, height,
            NULL);

        /*  fprintf(stderr,"Size x:%ld, y:%ld\n",screen_width,screen_height);*/
        if (pixmap)
            (void)XFreePixmap(XtDisplay(w),pixmap);

        if(pixmap_final)
            (void)XFreePixmap(XtDisplay(w),pixmap_final);

        if(pixmap_alerts)
            (void)XFreePixmap(XtDisplay(w),pixmap_alerts);

        pixmap=XCreatePixmap(XtDisplay(w),
                DefaultRootWindow(XtDisplay(w)),
                (unsigned int)width,(unsigned int)height,
                DefaultDepthOfScreen(XtScreen(w)));

        pixmap_final=XCreatePixmap(XtDisplay(w),
                DefaultRootWindow(XtDisplay(w)),
                (unsigned int)width,(unsigned int)height,
                DefaultDepthOfScreen(XtScreen(w)));

        pixmap_alerts=XCreatePixmap(XtDisplay(w),
                DefaultRootWindow(XtDisplay(w)),
                (unsigned int)width,(unsigned int)height,
                DefaultDepthOfScreen(XtScreen(w)));

        HandlePendingEvents(app_context);
        if (interrupt_drawing_now)
            return;

        setup_in_view();    // flag stations that are in screen view

        HandlePendingEvents(app_context);
        if (interrupt_drawing_now)
            return;

        // Reload maps
        // Set interrupt_drawing_now because conditions have
        // changed.
        interrupt_drawing_now++;

        // Request that a new image be created.  Calls create_image,
        // XCopyArea, and display_zoom_status.
        request_new_image++;

//        if (create_image(w)) {
//            (void)XCopyArea(XtDisplay(w),pixmap_final,XtWindow(w),gc,0,0,(unsigned int)screen_width,(unsigned int)screen_height,0,0);
//        }
    }
}





// We got a resize callback.  Set flags.  UpdateTime will come
// along in a bit and perform the resize.  With this method, the
// resize can be made interruptable.  We merely need to check for
// the interrupt_drawing_now flag periodically while doing the
// resize drawing.
//
void da_resize(Widget w, /*@unused@*/ XtPointer client_data, /*@unused@*/ XtPointer call_data) {

    // Set the interrupt_drawing_now flag
    interrupt_drawing_now++;

    // Set the request_resize flag
    request_resize++;
//    last_input_event = sec_now() + 2;
}





// We got a mouse or keyboard callback.  Set flags.  UpdateTime
// will come along in a bit and perform the screen redraw.  With
// this method, the redraw can be made interruptable.  We merely
// need to check for the interrupt_drawing_now flag periodically
// while doing the redraw.
//
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
    double area;
    float area_acres; // area in acres
    long a_x, a_y, b_x, b_y;
    char str_lat[20];
    char str_long[20];
    long x,y;
    int done = 0;
    long lat, lon;


    XtVaGetValues(w,
            XmNwidth, &width,
            XmNheight, &height,
            NULL);

    /*fprintf(stderr,"input event %d %d\n",event->type,ButtonPress);*/
    redraw=0;

    // Snag the current pointer position
    input_x = event->xbutton.x;
    input_y = event->xbutton.y;


/////////////////
// CAD OBJECTS //
/////////////////

    // Start of CAD Objects code.  We have both ButtonPress and
    // ButtonRelease code handlers here, for this mode only.
    // Check whether we're in CAD Object draw mode first
    if (draw_CAD_objects_flag && event->xbutton.button == Button2) {

        if (event->type == ButtonRelease) {
            // We don't want to do anything for ButtonRelease.  Most
            // of all, we don't want another point drawn for both
            // press and release, and we don't want other GUI
            // actions performed on release when in CAD Draw mode.
            done++;
        }
        else {  // ButtonPress for Button2

            // We need to check to see whether we're dragging the
            // pointer, and then need to save the points away (in
            // Xastir lat/long format), drawing lines between the
            // points whenever we do a pixmap_final refresh to the
            // screen.

            // Check whether we just did the first mouse button down
            // while in CAD draw mode.  If so, save away the mouse
            // pointer and get out.  We'll use that mouse pointer
            // the next time a mouse button gets pressed in order to
            // draw a line.

            // We're going to use gc_tint with an XOR bitblit here
            // to make sure that any lines we draw will be easily
            // seen, no matter what colors we're drawing on top of.
            //

            // If we have a valid saved position already from our
            // first click, then we must be on the 2nd or later
            // click.  Draw a line.
            if (polygon_last_x != -1 && polygon_last_y != -1) {

                // Convert from screen coordinates to Xastir
                // coordinate system and save in the object->vertice
                // list.
                convert_screen_to_xastir_coordinates(input_x,
                    input_y,
                    &lat,
                    &lon);
                CAD_vertice_allocate(lat, lon);
                // Reload symbols/tracks/CAD objects
                redraw_symbols(da);
            }
            else {  // First point of a polygon.  Save it.

                // Figure out the real lat/long from the screen
                // coordinates.  Create a new object to hold the
                // point.
                convert_screen_to_xastir_coordinates(input_x,
                    input_y,
                    &lat,
                    &lon);
                CAD_object_allocate(lat, lon);
            }

            // Save current point away for the next draw.
            polygon_last_x = input_x;
            polygon_last_y = input_y;

            done++;
        }
    }
// End of CAD Objects code.


/////////////////////////////////
// Start of ButtonRelease code //
/////////////////////////////////

    if (!done && event->type == ButtonRelease) {
        //fprintf(stderr,"ButtonRelease %d %d\n",event->xbutton.button,Button3);

#ifdef SWAP_MOUSE_BUTTONS
        if (event->xbutton.button == Button3) {
            // Right mouse button release
#else   // SWAP_MOUSE_BUTTONS
        if (event->xbutton.button == Button1) {
            // Left mouse button release
#endif  // SWAP_MOUSE_BUTTONS

            // If no drag, Center the map on the mouse pointer
            // If drag, compute new zoom factor/center and redraw
            // -OR- measure distances.


/////////////////////////
// CENTER MAP FUNCTION //
/////////////////////////

            // Check for "Center Map" function.  Must be within 15
            // pixels of where the button press occurred to qualify.
            if ( mouse_zoom && !measuring_distance && !moving_object
                    && (abs(menu_x - input_x) < 15)
                    && (abs(menu_y - input_y) < 15) ) {
           /*     if(display_up) {
                    XtVaGetValues(da,XmNwidth, &width,XmNheight, &height,NULL);
                    new_mid_x = center_longitude - ((width *scale_x)/2) + (menu_x*scale_x);
                    new_mid_y = center_latitude  - ((height*scale_y)/2) + (menu_y*scale_y);
                    new_scale_y = scale_y;          // keep size
                    display_zoom_image(1);          // check range and do display, recenter
                }
            */
                // Reset the zoom-box variables
                possible_zoom_function = 0;
                zoom_box_x1 = -1;
            }
            // It's not the center function because the mouse moved more than 15 pixels.
            // It must be either the "Compute new zoom/center" -OR- the "Measure distance"
            // -OR- "Move distance" functions..  The "measuring_distance" or "moving_object"
            // variables will tell us.

            else {
                // At this stage we have menu_x/menu_y where the button press occurred,
                // and input_x/input_y where the button release occurred.


//////////////////////
// MEASURE DISTANCE //
//////////////////////

                if (measuring_distance) {   // Measure distance function
                    double R = EARTH_RADIUS_METERS;


                    // Check whether we already have a box on screen
                    // that we need to erase.
                    if (zoom_box_x1 != -1) {
//fprintf(stderr,"erasing\n");
                        // Remove the last box drawn via the XOR
                        // function.
                        XDrawLine(XtDisplay(da),
                            XtWindow(da),
                            gc_tint,
                            l16(zoom_box_x1),    // Keep x constant
                            l16(zoom_box_y1),
                            l16(zoom_box_x1),
                            l16(zoom_box_y2));
                        XDrawLine(XtDisplay(da),
                            XtWindow(da),
                            gc_tint,
                            l16(zoom_box_x1),
                            l16(zoom_box_y1),    // Keep y constant
                            l16(zoom_box_x2),
                            l16(zoom_box_y1));
                        XDrawLine(XtDisplay(da),
                            XtWindow(da),
                            gc_tint,
                            l16(zoom_box_x2),    // Keep x constant
                            l16(zoom_box_y1),
                            l16(zoom_box_x2),
                            l16(zoom_box_y2));
                        XDrawLine(XtDisplay(da),
                            XtWindow(da),
                            gc_tint,
                            l16(zoom_box_x1),
                            l16(zoom_box_y2),    // Keep y constant
                            l16(zoom_box_x2),
                            l16(zoom_box_y2));
                    }

                    // Reset the zoom-box variables
                    possible_zoom_function = 0;
                    zoom_box_x1 = -1;

                    x_distance = abs(menu_x - input_x);
                    y_distance = abs(menu_y - input_y);

                    // Here we need to convert to either English or Metric units of distance.

                    //(temp,"Distance x:%d pixels,  y:%d pixels\n",x_distance,y_distance);
                    //popup_message_always(langcode("POPUPMA020"),temp);

                    XtVaGetValues(da,XmNwidth, &width,XmNheight, &height,NULL);
                    a_x = center_longitude  - ((width *scale_x)/2) + (menu_x*scale_x);
                    a_y = center_latitude   - ((height*scale_y)/2) + (menu_y*scale_y);

                    b_x = center_longitude  - ((width *scale_x)/2) + (input_x*scale_x);
                    b_y = center_latitude   - ((height*scale_y)/2) + (input_y*scale_y);

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
                        switch (english_units) {
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

// See this URL for a method of calculating the area of a lat/long
// rectangle on a sphere:
// http://mathforum.org/library/drmath/view/63767.html
// area = (pi/180)*R^2 * abs(sin(lat1)-sin(lat2)) * abs(lon1-lon2)
//
// Their formula is incorrect due to the mistake of mixing radians
// and degrees.  The correct formula is (WE7U):
// area = R^2 * abs(sin(lat1)-sin(lat2)) * abs(lon1-lon2)

                        // Compute correct units
                        switch (english_units) {
                            case 1:     // English
                                R = EARTH_RADIUS_MILES * 5280.0;    // feet
                                break;
                            case 2:     // Nautical miles and knots
                                R = EARTH_RADIUS_MILES * 5280.0;    // feet
                                break;
                            default:    // Metric
                                R = EARTH_RADIUS_METERS; // Meters
                                break;
                        }

                        // Compute the total area in feet or meters

                        // New method using area on a sphere:
                        area = R*R
                            * fabs( sin(convert_lat_l2r(a_y)) - sin(convert_lat_l2r(b_y)) )
                            * fabs( convert_lon_l2r(a_x) - convert_lon_l2r(b_x) );

                        // Old method using planar geometry:
                        //area = x_distance_real * y_distance_real;

                        // calculate area in acres
                        switch (english_units) { 
                           case 1:
                           case 2:
                               area_acres = area * 2.2956749e-05;
                               break;
                           default:  // Metric
                               area_acres = area * 2.4710439e-04;
                               break;
                        }
                        //if (area_acres<0.1) 
                        //     area_acres = 0;

//fprintf(stderr,"Old method: %f\nNew method: %f\n\n",
//    x_distance_real * y_distance_real,
//    area);

                        

// NOTE:  Angles currently change at zoom==1, so we purposely don't
// give an angle in that measurement instance below.
//
                        xastir_snprintf(temp,
                            sizeof(temp),
                            "%0.2f %s, x=%0.2f %s, y=%0.2f %s, %0.2f %s %s (%0.2f %s), %s: %s %s",
                            full_distance, un_alt,      // feet/meters
                            x_distance_real, un_alt,    // feet/meters
                            y_distance_real, un_alt,    // feet/meters
                            area,
                            langcode("POPUPMA038"), // square
                            un_alt,
                            area_acres,
                            "acres",
                            langcode("POPUPMA041"), // Bearing
                            (scale_y == 1) ? "??" : temp_course, // Fix for zoom==1
                            langcode("POPUPMA042") );   // degrees
                    }
                    else {
                        // Compute the total area in miles or
                        // kilometers

                        // Compute correct units
                        switch (english_units) {
                            case 1:     // English
                                R = EARTH_RADIUS_MILES; // Statute miles
                                break;
                            case 2:     // Nautical miles and knots
                                R = EARTH_RADIUS_KILOMETERS/1.852; // Nautical miles
                                break;
                            default:    // Metric
                                R = EARTH_RADIUS_KILOMETERS; // kilometers
                                break;
                        }

                        // New method, area on a sphere:
                        area = R*R
                            * fabs(sin(convert_lat_l2r(a_y))-sin(convert_lat_l2r(b_y)))
                            * fabs(convert_lon_l2r(a_x)-convert_lon_l2r(b_x));

                        // Old method using planar geometry:
                        //area = x_distance_real * y_distance_real;

//fprintf(stderr,"Old method: %f\nNew method: %f\n\n",
//    x_distance_real * y_distance_real,
//    area);

                        xastir_snprintf(temp,
                            sizeof(temp),
                            "%0.2f %s, x=%0.2f %s, y=%0.2f %s, %0.2f %s %s, %s: %s %s",
                            full_distance, un_dst,      // miles/kilometers
                            x_distance_real, un_dst,    // miles/kilometers
                            y_distance_real, un_dst,    // miles/kilometers
                            area,
                            langcode("POPUPMA038"), // square
                            un_dst,
                            langcode("POPUPMA041"), // Bearing
                            temp_course,
                            langcode("POPUPMA042") ); // degrees
                    }
                    popup_message_always(langcode("POPUPMA020"),temp);
                }


///////////////////
// MOVING OBJECT //
///////////////////

                else if (moving_object) {   // Move function
                    // For this function we need to:
                    //   Determine which icon is closest to the mouse pointer press position.
                    //     We'll use Station_info to select the icon for us.
                    //   Determine whether it is our object.  If not, force
                    //     the user to "adopt" the object before moving it.
                    //   Compute the lat/lon of the mouse pointer release position.
                    //   Put the new value of lat/lon into the object data.
                    //   Cause symbols to get redrawn.

                    // Reset the zoom-box variables
                    possible_zoom_function = 0;
                    zoom_box_x1 = -1;

                    x = (center_longitude - ((screen_width  * scale_x)/2) + (event->xmotion.x * scale_x));
                    y = (center_latitude  - ((screen_height * scale_y)/2) + (event->xmotion.y * scale_y));

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
                        //fprintf(stderr,"%s  %s\n", str_lat, str_long);
                    }

                    // Effect the change in the object/item's
                    // position.
                    //
                    doing_move_operation++;
                    Station_info(w, "2", NULL);
                    doing_move_operation = 0;
                }


/////////////////////////////
// COMPUTE NEW CENTER/ZOOM //
/////////////////////////////

                else {  // Must be "Compute new center/zoom" function
                    float ratio;

                    // We need to compute a new center and a new scale, then
                    // cause the new image to be created.
                    // Compute new center.  It'll be the average of the two points
                    x_center = (menu_x + input_x) /2;
                    y_center = (menu_y + input_y) /2;

                    XtVaGetValues(da,XmNwidth, &width,XmNheight, &height,NULL);
                    new_mid_x = center_longitude - ((width *scale_x)/2) + (x_center*scale_x);
                    new_mid_y = center_latitude  - ((height*scale_y)/2) + (y_center*scale_y);

                    //
                    // What Rolf had to say:
                    //
                    // Calculate center of mouse-marked area and get the scaling relation
                    // between x and y for that position. This position will be the new
                    // center, so that lattitude-dependent relation does not change with
                    // a zoom-in. For both x and y calculate a new zoom factor necessary
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
                    //fprintf(stderr,"scale_x:%ld\tscale_y:%ld\n", get_x_scale(new_mid_x, new_mid_y, scale_y), scale_y );
                    ratio = ((float)get_x_scale(new_mid_x,new_mid_y,scale_y) / (float)scale_y);

                    //fprintf(stderr,"Ratio: %f\n", ratio);
                    //fprintf(stderr,"x:%ld\ty:%ld\n", new_scale_x, new_scale_y);
                    if ( new_scale_y < (long)((new_scale_x / ratio) + 0.5) ) {
                        new_scale_y =  (long)((new_scale_x / ratio) + 0.5);
                        //fprintf(stderr,"Changed y\n");
                    }
                    //fprintf(stderr,"x:%ld\ty:%ld\n", new_scale_x, new_scale_y);

                    display_zoom_image(1);          // Check range and do display, recenter

                    menu_x = input_x;
                    menu_y = input_y;
                    //fprintf(stderr,"Drag/zoom/center happened\n");

                    // Reset the zoom-box variables
                    possible_zoom_function = 0;
                    zoom_box_x1 = -1;
                }
            }
            mouse_zoom = 0;
        }   // End of Button1 release code


//////////////
// ZOOM OUT //
//////////////

        else if (event->xbutton.button == Button2) {
            // Middle mouse button release

            // Zoom out 2x with panning
            menu_x=input_x;
            menu_y=input_y;
            Zoom_out( w, client_data, call_data );

            // Simple zoom out, keeping map center at current position
            //Zoom_out_no_pan( w, client_data, call_data );
            mouse_zoom = 0;
        }   // End of Button2 release code


////////////////////////////////
// THIRD MOUSE BUTTON RELEASE // 
////////////////////////////////

#ifdef SWAP_MOUSE_BUTTONS
        else if (event->xbutton.button == Button1) {
            // Left mouse button release
#else   // SWAP_MOUSE_BUTTONS
        else if (event->xbutton.button == Button3) {
            // Right mouse button release
#endif  // SWAP_MOUSE_BUTTONS

            // Do nothing.  We have a popup tied into the button press anyway.
            // (Mouse_button_handler & right_menu_popup).
            // Leave the button release alone in this case.
            mouse_zoom = 0;
        }   // End of Button3 release code


///////////////
// SCROLL UP //
///////////////

        else if (event->xbutton.button == Button4) {
// Scroll up
            menu_x=input_x;
            menu_y=input_y;
            Pan_up(w, client_data, call_data);
        }   // End of Button4 release code


/////////////////
// SCROLL DOWN //
/////////////////

        else if (event->xbutton.button == Button5) {
// Scroll down
            menu_x=input_x;
            menu_y=input_y;
            Pan_down(w, client_data, call_data);
        }   // End of Button5 release code


////////////////////////////////////
// YET MORE MOUSE BUTTON RELEASES //
////////////////////////////////////

        else if (event->xbutton.button == 6) {
// Mouse button 6 release
            menu_x=input_x;
            menu_y=input_y;
            Zoom_out_no_pan(w, client_data, call_data);
            mouse_zoom = 0;
        }   // End of Button6 code

        else if (event->xbutton.button == 7) {
// Mouse button 7 release
            menu_x=input_x;
            menu_y=input_y;
            Zoom_in_no_pan(w, client_data, call_data);
            mouse_zoom = 0;
        }   // End of Button7 release code
    }
// End of ButtonRelease code



///////////////////////////////
// Start of ButtonPress code //
///////////////////////////////

    else if (!done && event->type == ButtonPress) {
        //fprintf(stderr,"ButtonPress %d %d\n",event->xbutton.button,Button3);

#ifdef SWAP_MOUSE_BUTTONS
        if (event->xbutton.button == Button3) {
// Right mouse button press
#else   // SWAP_MOUSE_BUTTONS
        if (event->xbutton.button == Button1) {
// Left mouse button press
#endif  // SWAP_MOUSE_BUTTONS

            // Mark the position for possible drag function
            menu_x=input_x;
            menu_y=input_y;
            mouse_zoom = 1;

            if (!moving_object) {
                // Not moving an object/item, so allow the zoom-in
                // box to display.
                possible_zoom_function++;
            }
        }   // End of Button1 Press code

        else if (event->xbutton.button == Button2) {
// Middle mouse button or both right/left mouse buttons press

            // Nothing attached here.
            mouse_zoom = 0;
        }   // End of Button2 Press code

#ifdef SWAP_MOUSE_BUTTONS
        else if (event->xbutton.button == Button1) {
// Left mouse button press
#else   // SWAP_MOUSE_BUTTONS
        else if (event->xbutton.button == Button3) {
// Right mouse button press
#endif  // SWAP_MOUSE_BUTTONS

            // Nothing attached here.
            mouse_zoom = 0;
        }   // End of Button3 Press code
    }
// End of ButtonPress code


////////////////////////////
// Start of KeyPress code //
////////////////////////////

    else if (!done && event->type == KeyPress) {

        // We want to branch from the keysym instead of the keycode
        (void)XLookupString( (XKeyEvent *)event, buffer, bufsize, &key, &compose );
	//fprintf(stderr,"main.c:da_input():keycode %d\tkeysym %ld\t%s\n", event->xkey.keycode, key, buffer);

        // keycode ??, keysym 65360 is Home (0x???? on sun kbd)
//        if ((key == 65360) || (key == 0x????)) {
        if (key == 65360) {
            Go_Home(w, NULL, NULL);
        }

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
// End of KeyPress code


//////////////////////////////////
// START OF SOMETHING ELSE CODE //
//////////////////////////////////
    else if (!done) {  // Something else
        if (event->type == MotionNotify) {
            input_x = event->xmotion.x;
            input_y = event->xmotion.y;
//fprintf(stderr,"da_input2 x %d y %d\n",input_x,input_y);
        }
    }   // End of SomethingElse code



    if (redraw) {
        /*fprintf(stderr,"Current x %ld y * %ld\n",center_longitude,center_latitude);*/

        // Set the interrupt_drawing_now flag
        interrupt_drawing_now++;

        // Request that a new image be created.  Calls create_image,
        // XCopyArea, and display_zoom_status.
        request_new_image++;
//        last_input_event = sec_now() + 2;
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





// This function snags the current pointer information and tries to
// determine whether we're doing some sort of draw or zoom function.
// If so, draws the appropriate temporary squares or lines that the
// operator expects.
//
void check_pointer_position(void) {
    Window  root_return, child_return;
    int rootx_return, rooty_return;
    int win_x_return, win_y_return;
    unsigned int mask_return;
    Bool ret;
    int x_return;
    int y_return;
    unsigned int width_return;
    unsigned int height_return;
    unsigned int border_width_return;
    unsigned int depth_return;


    // If this variable has not been set, we should not display the
    // box.
    if (!possible_zoom_function) {
        return;
    }

    // Snag the current pointer info
    ret = XQueryPointer(XtDisplay(da),
            XtWindow(da),  // Window we are interested in
            &root_return,  // Root window that pointer is in
            &child_return, // Child windows that pointer is in, if any
            &rootx_return, // Pointer coord. relative to root window
            &rooty_return, // Pointer coord. relative to root window
            &win_x_return, // Pointer coord. relative to specified window
            &win_y_return, // Pointer coord. relative to specified window
            &mask_return); // State of modifier keys and pointer buttons

    switch (ret) {

        case True:
            // If we made it here, we're on the same screen as the
            // specified window.  It's a good start anyway.
//fprintf(stderr, "x:%d  y:%d  ", win_x_return, win_y_return);
//fprintf(stderr, "root:%lx  child:%lx  ", root_return, child_return);
//fprintf(stderr, "mask:%03x  ret:%02x\n", mask_return, ret);

            // Check mask_return to see if button one is being
            // pressed down (a drag operation).  If so, we're doing
            // a zoom-in operation and need to draw a box.  0x100

            // Check if button two (middle button) is being pressed
            // down (a drag operation).  If so, we're doing a CAD
            // Object draw and need to draw a line.  0x200

            // Figure out how to erase previous lines/boxes so that
            // only the current object is shown.  We might need to
            // keep track of earlier vectors and then redraw them
            // with an XOR function to erase.

            // Get the dimensions for the drawing area
            // XGetGeometry(Display *display,
            //     Drawable d,
            //     Window *root_return,
            //     int *x_return,
            //     int *y_return,
            //     unsigned int *width_return,
            //     unsigned int *height_return,
            //     unsigned int *border_width_return,
            //     unsigned int *depth_return);
            XGetGeometry(XtDisplay(da),
                XtWindow(da),
                &root_return,
                &x_return,
                &y_return,
                &width_return,
                &height_return,
                &border_width_return,
                &depth_return);

            // Check that X/Y are positive and below the max size of
            // the child window.
            if ( win_x_return >= (int)width_return
                    || win_y_return >= (int)height_return) {

                /*
                fprintf(stderr, "Out of bounds: %d:%d  %d:%d\n",
                    win_x_return,
                    width_return,
                    win_y_return,
                    height_return);
                */
                return;
            }
            else {
                // Draw what we need to.
                // For CAD objects, polygon_last_x and
                // polygon_last_y contain the last position.
                // For the zoom-in function, menu_x and menu_y
                // contain the last position.

                if (draw_CAD_objects_flag) {
                    // Check if button two (middle button) is being
                    // pressed down (a drag operation).  If so,
                    // we're doing a CAD Object draw and need to
                    // draw a line.  0x200
                    if ( (mask_return & 0x200) == 0) {
                        return;
                    }

                    // Remove the last line drawn (if any).  Draw a
                    // line from polygon_last_x and polygon_last_y
                    // to the current pointer position.
/*
                    (void)XSetLineAttributes(XtDisplay(da), gc_tint, 0, LineSolid, CapButt,JoinMiter);
                    (void)XSetForeground(XtDisplay(da), gc_tint, colors[(int)0x0e]); // yellow
                    XDrawLine(XtDisplay(da),
                        XtWindow(da),
                        gc_tint,
                        l16(polygon_last_x),
                        l16(polygon_last_y),
                        l16(win_x_return),
                        l16(win_y_return));
*/
                    return;
                }
                else {  // Zoom-in function?
                    // Check mask_return to see if button one is
                    // being pressed down (a drag operation).  If
                    // so, we're doing a zoom-in operation and need
                    // to draw a box.  0x100
#ifdef SWAP_MOUSE_BUTTONS
                    if ( (mask_return & 0x400) == 0) {  // Button3
#else   // SWAP_MOUSE_BUTTONS
                    if ( (mask_return & 0x100) == 0) {  // Button1
#endif  // SWAP_MOUSE_BUTTONS
                        return;
                    }

                    (void)XSetLineAttributes(XtDisplay(da), gc_tint, 1, LineSolid, CapButt,JoinMiter);
                    (void)XSetForeground(XtDisplay(da), gc_tint, colors[(int)0x0e]); // yellow
                    (void)XSetFunction(XtDisplay(da), gc_tint, GXxor);
 
                    // Check whether we already have a box on screen
                    // that we need to erase.
                    if (zoom_box_x1 != -1) {
//fprintf(stderr,"erasing\n");
                        // Remove the last box drawn via the XOR
                        // function.
                        XDrawLine(XtDisplay(da),
                            XtWindow(da),
                            gc_tint,
                            l16(zoom_box_x1),    // Keep x constant
                            l16(zoom_box_y1),
                            l16(zoom_box_x1),
                            l16(zoom_box_y2));
                        XDrawLine(XtDisplay(da),
                            XtWindow(da),
                            gc_tint,
                            l16(zoom_box_x1),
                            l16(zoom_box_y1),    // Keep y constant
                            l16(zoom_box_x2),
                            l16(zoom_box_y1));
                        XDrawLine(XtDisplay(da),
                            XtWindow(da),
                            gc_tint,
                            l16(zoom_box_x2),    // Keep x constant
                            l16(zoom_box_y1),
                            l16(zoom_box_x2),
                            l16(zoom_box_y2));
                        XDrawLine(XtDisplay(da),
                            XtWindow(da),
                            gc_tint,
                            l16(zoom_box_x1),
                            l16(zoom_box_y2),    // Keep y constant
                            l16(zoom_box_x2),
                            l16(zoom_box_y2));
                    }

                    // Draw a box around the current zoom area.
                    XDrawLine(XtDisplay(da),
                        XtWindow(da),
                        gc_tint,
                        l16(menu_x),         // Keep x constant
                        l16(menu_y),
                        l16(menu_x),
                        l16(win_y_return));
                    XDrawLine(XtDisplay(da),
                        XtWindow(da),
                        gc_tint,
                        l16(menu_x),
                        l16(menu_y),         // Keep y constant
                        l16(win_x_return),
                        l16(menu_y));
                    XDrawLine(XtDisplay(da),
                        XtWindow(da),
                        gc_tint,
                        l16(win_x_return),   // Keep x constant
                        l16(menu_y),
                        l16(win_x_return),
                        l16(win_y_return));
                    XDrawLine(XtDisplay(da),
                        XtWindow(da),
                        gc_tint,
                        l16(menu_x),
                        l16(win_y_return),   // Keep y constant
                        l16(win_x_return),
                        l16(win_y_return));

                    // Save the values away so that we can erase the
                    // box later.
                    zoom_box_x1 = menu_x;
                    zoom_box_y1 = menu_y;
                    zoom_box_x2 = win_x_return;
                    zoom_box_y2 = win_y_return;

                    return;
                }

            }

        break;

        case BadWindow: // A window passed to the function was no
                        // good.
            fprintf(stderr, "check_pointer_position: BadWindow\n");
            return;
            break;

        case False: // Pointer is not on the same screen as the
                    // specified window.
        default:
            return;
            break;
 
    }
}   // End of check_pointer_position()





// Release ApplicationContext when we are asked to leave
//
void clear_application_context(void)
{
    if (app_context)
      XtDestroyApplicationContext(app_context);
    app_context = NULL;
}



time_t stations_status_time = 0;
static int last_alert_on_screen = -1;
 

// This is the periodic process that updates the maps/symbols/tracks.
// At the end of the function it schedules itself to be run again.
void UpdateTime( XtPointer clientData, /*@unused@*/ XtIntervalId id ) {
    Widget w = (Widget) clientData;
    time_t nexttime;
    int do_time;
    int max;
    int i;
    char station_num[30];
    char line[MAX_LINE_SIZE+1];
    int line_offset = 0;
    int n;
    time_t current_time;
    int data_length;
    int data_port;
    unsigned char data_string[MAX_LINE_SIZE];
#ifdef HAVE_DB
    int got_conn;   // holds result from openConnection() 
#endif // HAVE_DB

    do_time = 0;

    // Start UpdateTime again 10 milliseconds after we've completed.
    // Note:  Setting this too low can cause // some systems
    // (RedHat/FreeBSD) to spin their wheels a lot, using up great
    // amounts of CPU time.  This is heavily dependent on the true
    // value of the "HZ" value, which is reported as "100" on some
    // systems even if the kernel is using another value.
#ifdef __CYGWIN__
    // Cygwin performance is abysmal if nexttime is lower than 50, almost
    // acceptable at 200.
    nexttime = 200;
#else
    // Changed from 2 to 10 to fix high CPU usage problems on
    // FreeBSD.
    nexttime = 10;
#endif // __CYGWIN__



    if (restart_xastir_now) {
        char bin_path[250];

        clear_application_context();
        // Restart Xastir in this process space.  This is triggered
        // by receiving a SIGHUP signal to the main process, which
        // causes the signal handler restart() to run.  restart()
        // shuts down most things nicely and then sets the
        // restart_xastir_now  global variable.
        //
        // We need to snag the path to the executable from somewhere
        // so that we can start up again on a variety of systems.
        // Trying to get it from argv[0] doesn't work as that ends
        // up as "xastir" with no path.  We therefore get it from
        // XASTIR_BIN_PATH which we define in configure.ac
        //
//        execve("/usr/local/bin/xastir", my_argv, my_envp);
        xastir_snprintf(bin_path,
            sizeof(bin_path),
            "%s/bin/xastir",
            XASTIR_BIN_PATH);

        // Restart this Xastir instance
        execve(bin_path, my_argv, my_envp);
    }



    current_time = sec_now();

    if (last_updatetime > current_time) {
        // Time just went in the wrong direction.  Sleep for a bit
        // so that we don't use massive CPU until the time catches
        // up again.
        //
        if (time_went_backwards == 0) {
            char temp[110];

            // This is our first time through UpdateTime() since the
            // time went in the wrong direction.  Dump out a
            // message to the user.
            time_went_backwards++;
            get_timestamp(temp);

            fprintf(stderr,"\n\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
            fprintf(stderr,    "!!  System time jumped backwards %d seconds!\n",
                (int)(last_updatetime - current_time) );
            fprintf(stderr,    "!! Xastir sleeping, else will use excessive CPU\n");
            fprintf(stderr,    "!! %s\n",
                temp);
            fprintf(stderr,    "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n\n");
            time_went_backwards++;
        }
        usleep(1);   // Sleep for 1uS.
    }
    else {
        // Time is behaving normally.
        last_updatetime = current_time;
        if (time_went_backwards) {
            fprintf(stderr,
                "Xastir is done sleeping due to time reversal.\n\n");
        }
        time_went_backwards = 0;
    }


    (void)sound_done();

    if(display_up) {

        if(display_up_first == 0) {             // very first call, do initialization

            display_up_first = 1;
            statusline(langcode("BBARSTA045"), 1); // Loading symbols...
            load_pixmap_symbol_file("symbols.dat", 0);
            statusline(langcode("BBARSTA047"), 1); // Initialize my station...
            my_station_add(my_callsign,my_group,my_symbol,my_long,my_lat,my_phg,my_comment,(char)position_amb_chars);
            da_resize(w, NULL,NULL);            // make sure the size is right after startup & create image
            set_last_position();                // init last map position

#ifdef HAVE_DB
//uncomment to enable hardcoded test of writing station to db
//simpleDbTest();
#endif /* HAVE_DB */
            // Restore weather alerts so that we have a clear
            // picture of the current state.  Do this before we
            // start the interfaces.
            load_wx_alerts_from_log();

            statusline(langcode("BBARSTA048"), 1); // Start interfaces...
            startup_all_or_defined_port(-1);    // start interfaces
        }

        else {  // Not the first time UpdateTime was called.
                // Perform the regular updates.

            if (first_time_run) {
                first_time_run = 0;
                Configure_station(NULL, NULL, NULL);
            }

            popup_time_out_check(current_time);         // clear popup windows after timeout
            check_statusline_timeout(current_time);     // clear statusline after timeout
            check_station_remove(current_time);         // remove old stations
            check_message_remove(current_time);         // remove old messages

#ifdef USE_RTREE
  #ifdef HAVE_LIBSHP
            purge_shp_hash();                   // purge stale rtrees
  #endif // HAVE_LIBSHP
#endif // USE_RTREE


            // We need to always calculate the Aloha circle so that
            // if it is turned on by the user it will be accurate.
            calc_aloha(current_time);


            //if ( (new_message_data > 0) && ( (delay_time % 2) == 0) )
            //update_messages(0);                 // Check Messages, no forced update


            // Check whether it's time to expire some weather
            // alerts.  This function will set redraw_on_new_data
            // and alert_redraw_on_update if any alerts are expired
            // from the list.
            (void)alert_expire(current_time);


#ifdef HAVE_GPSMAN
            // Check whether we've just completed a GPS transfer and
            // have new maps to draw because of it.  This function
            // can cause a complete redraw of the maps.
            check_for_new_gps_map(current_time);


            // Check whether it is time to snag RINO waypoints
            // again, creating APRS Objects out of them.  "0" for
            // the download interval disables this function.
            if (RINO_download_interval > 0) {
                int rino_time = RINO_download_interval * 60;

                if (last_RINO_download + rino_time < current_time) {
                    last_RINO_download = current_time;
                    GPS_operations(NULL, "7", NULL);
                }
            }
#endif  // HAVE_GPSMAN


            if (xfontsel_query) {
                Query_xfontsel_pipe();
            }


            // Check on resize requests
            if (request_resize) {
//                if (last_input_event < current_time) {
                    da_resize_execute(w);
//                }
            }


            if (request_new_image) {
//                if (last_input_event < current_time) {
                    new_image(w);
//                }
            }


            // check on Redraw requests
            if (         ( (redraw_on_new_data > 1)
                        || (redraw_on_new_data && (current_time > last_redraw + REDRAW_WAIT))
                        || (current_time > next_redraw)
                        || (pending_ID_message && (current_time > remove_ID_message_time)) )
                    && !wait_to_redraw) {

                int temp_alert_count;


                //fprintf(stderr,"Redraw on new data\n");

                // Cause refresh_image() to happen if no other
                // triggers occurred, but enough time has passed.
                if (current_time > next_redraw) {
                    alert_redraw_on_update++;
                }

                // check if alert_redraw_on_update is set and it has been at least xx seconds since
                // last weather alert redraw.
                if ( (alert_redraw_on_update
                        && !pending_ID_message
                        && ( current_time > ( last_alert_redraw + WX_ALERTS_REFRESH_TIME ) ))
                      || (pending_ID_message && (current_time > remove_ID_message_time)) ) {


                    // If we got here because of the ID_message
                    // stuff, clear the variable.
                    if (pending_ID_message && (current_time > remove_ID_message_time)) {
                        pending_ID_message = 0;
                    }

                //if (alert_redraw_on_update) {
                    //fprintf(stderr,"Alert redraw on update: %ld\t%ld\t%ld\n",
                    //          current_time, last_alert_redraw, WX_ALERTS_REFRESH_TIME);

                    if (!pending_ID_message) {
                        refresh_image(da);  // Much faster than create_image.
                        (void)XCopyArea(XtDisplay(da),
                            pixmap_final,
                            XtWindow(da),
                            gc,
                            0,
                            0,
                            (unsigned int)screen_width,
                            (unsigned int)screen_height,
                            0,
                            0);

                        // We just refreshed the screen, so don't
                        // try to erase any zoom-in boxes via XOR.
                        zoom_box_x1 = -1;
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
#endif // HAVE_FESTIVAL
                    }
                    last_alert_on_screen = temp_alert_count;
                    alert_redraw_on_update = 0;
                } else {
                    if (!pending_ID_message)
                        redraw_symbols(w);
                }

                redraw_on_new_data = 0;
                next_redraw = current_time+60; // redraw every minute
                last_redraw = current_time;

                // This assures that we periodically check for expired alerts
                // and schedule a screen update if we find any.
                if (alert_display_request())                            // should nor be placed in redraw loop !!???
                    alert_redraw_on_update = redraw_on_new_data = 1;    // ????

            }


            if (initial_load) {

                // Reload saved objects and items from previous runs.
                // This implements persistent objects.
                reload_object_item();

#ifdef HAVE_DB
                // load data from SQL database connections
                // step through interface list
                for (i = 0; i < MAX_IFACE_DEVICES; i++){
                     // if interface is a database and is set to load on start then load
                     if (connections_initialized==0) { 
                           connections_initialized = initConnections();
                     }
                     if (devices[i].device_type == DEVICE_SQL_DATABASE && devices[i].query_on_startup && port_data[i].status==DEVICE_UP) { 
                          // load data
                          if (devices[i].connect_on_startup == 1) { 
                              // there should be an open connection already
                              if (debug_level & 4096) 
                                  fprintf(stderr,"Opening (in main) connection [%d] with existing connection [%p]",i,connections[i]); 
                              if (pingConnection(connections[i])==True) { 
                                  got_conn = 1;
                              } else { 
                                // if (debug_level & 4096) 
                                     fprintf(stderr,"Ping failed opening new connection [%p]",connections[i]);                          
                                 got_conn = openConnection(&devices[i],connections[i]); 
                              }
                          } else { 
                              if (debug_level & 4096) 
                                  fprintf(stderr,"Opening (in main) connection [%d] with new connection [%p]",i,connections[i]);                          
                              got_conn = openConnection(&devices[i],connections[i]); 
                          }
                          if ((got_conn == 1) && (!(connections[i]->type==NULL))) { 
                              getAllSimplePositions(connections[i]);
                              // if connection worked, it is a oneshot upload of data, so we don't 
                              // need to set port_data[].active and .status values here.
                          } else {
                              // report error on this port
                              port_data[i].active = DEVICE_IN_USE;
                              port_data[i].status = DEVICE_ERROR;
                              update_interface_list();
                          }
                     }
                }
#endif /* HAVE_DB */

                // Reload any CAD objects from file.  This implements
                // persistent objects.
                Restore_CAD_Objects_from_file();

                initial_load = 0;   // All done!
            }


            if (Display_.dr_data
                    && ((current_time - sec_last_dr_update) > update_DR_rate) ) {

//WE7U:  Possible slow-down here w.r.t. dead-reckoning?  If
//update_DR_rate is too quick, we end up looking through all of the
//stations in station list much too often and using a lot of CPU.

                redraw_on_new_data = 1;
                sec_last_dr_update = current_time;
            }


            // Look for packet data and check port status
            display_packet_data();
            if (delay_time > 15) {
                interface_status(w);
                delay_time = 0;
                // check station lists
                update_station_scroll_list();   // maybe update lists
            }
            delay_time++;


            // If active HSP ports, check whether we've been sitting
            // for longer than XX seconds waiting for GPS data.  If
            // so, the GPS is powered-down, lost lock, or become
            // disconnected.  Go back to listening on the TNC port.
            //
            if (current_time > (sec_last_dtr + 2)) {   // 2-3 secs

                if (!gps_stop_now) {    // No GPS strings parsed

                    // GPS listen timeout!  Pop us out of GPS listen
                    // mode on all HSP ports.  Listen to the TNC for
                    // a while.
//fprintf(stderr,"1:calling dtr_all_set(0)\n");
                    dtr_all_set(0);
                    sec_last_dtr = current_time;
                }
            }


            // If we parsed valid GPS data, bring all DTR lines back
            // to normal for all HSP interfaces (set to receive from
            // TNC now).
            if (gps_stop_now) {
//fprintf(stderr,"2:calling dtr_all_set(0)\n");
                dtr_all_set(0); // Go back to TNC listen mode
                sec_last_dtr = current_time;
            }


            // Start the GPS listening process again

            // check gps start up, GPS on GPSPORT
            if(current_time > sec_next_gps) {

                // Reset the gps good-data flag
                if (gps_stop_now) {
                    gps_stop_now = 0;
                }

                //fprintf(stderr,"Check GPS\n");

                // Set dtr lines down
                // works for SERIAL_GPS and SERIAL_TNC_HSP_GPS?

                // HSP interfaces:  Set DTR line for all.  DTR will
                // get reset for each line as valid GPS data gets
                // parsed on that interface.
//fprintf(stderr,"3:calling dtr_all_set(1)\n");
                dtr_all_set(1);
                sec_last_dtr = current_time;   // GPS listen timeout

                for(i=0; i<MAX_IFACE_DEVICES; i++) {
                    if (port_data[i].status) {
                        char tmp[3];
                        int ret1 = 0;
                        int ret2 = 0;

                        switch (port_data[i].device_type) {
                            case DEVICE_SERIAL_TNC_AUX_GPS:

                                // Tell TNC to send GPS data
  
                                // Device is correct type and is UP
                                // (or ERROR).  Send character to
                                // device (prefixed with CTRL-C so
                                // that we exit CONV if necessary).
                                //
                                if (debug_level & 128) {
                                    fprintf(stderr,"Retrieving GPS AUX port %d\n", i);
                                }
                                sprintf(tmp, "%c%c",
                                    '\3',
                                    devices[i].gps_retrieve);

                                if (debug_level & 1) {
                                    fprintf(stderr,"Using 0x%02x 0x%02x to retrieve GPS\n",
                                        '\3',
                                        devices[i].gps_retrieve);
                                }
                                port_write_string(i, tmp);

                                // GPS strings are processed in
                                // UpdateTime function via
                                // gps_data_find(), if the incoming
                                // data is GPS data instead of TNC
                                // data.  We need to do nothing
                                // further here.

                                break;

                            case DEVICE_SERIAL_TNC_HSP_GPS:
                                // Check for GPS timing being too
                                // short for HSP interfaces.  If to
                                // short, we'll never receive any
                                // TNC data, just GPS data.
                                if (gps_time < 3) {
                                    gps_time = 3;
                                    popup_message_always(langcode("POPEM00036"),
                                        langcode("POPEM00037"));
                                }

                                // GPS strings are processed in
                                // UpdateTime function via
                                // gps_data_find(), if the incoming
                                // data is GPS data instead of TNC
                                // data.  We need to do nothing
                                // further here.

                                break;

                            case DEVICE_SERIAL_GPS:
                            case DEVICE_NET_GPSD:

// For each of these we wish to dump their queue to be processed at
// the gps_time interval.  At other times they should be overwriting
// old data with new and not processing the strings.

                                // Process the GPS strings saved by
                                // the channel_data() function.
                                if (gprmc_save_string[0] != '\0')
                                    ret1 = gps_data_find(gprmc_save_string,
                                        gps_port_save);
                                if (gpgga_save_string[0] != '\0')
                                    ret2 = gps_data_find(gpgga_save_string,
                                        gps_port_save);

                                // Blank out the global variables
                                // (we just processed them).
                                gprmc_save_string[0] = '\0';
                                gpgga_save_string[0] = '\0';

                                if (ret1 && ret2) {
                                    char temp[200];

                                    xastir_snprintf(temp,
                                        sizeof(temp),
                                        "GPS:GPRMC,GPGGA ");
                                    strncat(temp,
                                        report_gps_status(),
                                        sizeof(temp) - strlen(temp));
                                    statusline(temp, 0);
                                }
                                else if (ret1) {
                                    char temp[200];
 
                                    xastir_snprintf(temp,
                                        sizeof(temp),
                                        "GPS:GPRMC ");
                                    strncat(temp,
                                        report_gps_status(),
                                        sizeof(temp) - strlen(temp));
                                    statusline(temp, 0);
                                }
                                else if (ret2) {
                                    char temp[200];

                                    xastir_snprintf(temp,
                                        sizeof(temp), 
                                        "GPS:GPGGA ");
                                    strncat(temp,
                                        report_gps_status(),
                                        sizeof(temp) - strlen(temp));
                                    statusline(temp, 0);
                                }
                                else {
                                    char temp[200];

                                    xastir_snprintf(temp,
                                        sizeof(temp),
                                        "GPS: ");
                                    strncat(temp,
                                        report_gps_status(),
                                        sizeof(temp) - strlen(temp));
                                    statusline(temp, 0);
                                }

                                break;
                            default:
                                break;
                        }
                    }
                }
                sec_next_gps = current_time+gps_time;
            }

            // Check to reestablish a connection
            if(current_time > net_next_time) {
                net_last_time = current_time;
                net_next_time = net_last_time + 300;    // Check every five minutes
                //net_next_time = net_last_time + 30;   // This statement is for debug

                //fprintf(stderr,"Checking for reconnects\n");
                check_ports();
            }

#ifdef USING_LIBGC
            // Check for leaks?
            if(current_time > gc_next_time) {
                gc_next_time = current_time + 60;  // Check every minute
//fprintf(stderr,"Checking for leaks\n");
                CHECK_LEAKS();
            }
#endif  // USING_LIBGC

            // Check to see if it is time to spit out data
            if(!wait_to_redraw) {
                if (last_time == 0) {
                    // first update
                    next_time = 120;
                    last_time = current_time;
                    do_time = 1;
                } else {
                    // check for update
                    //fprintf(stderr,"Checking --- time %ld time to update %ld\n",current_time,last_time+next_time);
                    if(current_time >= (last_time + next_time)) {
                        next_time += next_time;
                        if (next_time > max_transmit_time)
                            next_time = max_transmit_time;

                        last_time = current_time;
                        do_time = 1;
                    }
                }
            }

            // Time to spit out a posit?   If emergency_beacon is enabled
            // change to a relatively fast fixed beacon rate.  Should be
            // more than a 30-second interval though to avoid digipeater
            // dupe intervals of 30 seconds.
            //
            if ( my_position_valid
                    && (   transmit_now
                        || (emergency_beacon && (current_time > (posit_last_time + 60) ) )
                        || (current_time > posit_next_time) ) ) {

                //fprintf(stderr,"Transmitting posit\n");

                // Check for proper symbol in case we're a weather station
                (void)check_weather_symbol();

                posit_last_time = current_time;

                if (smart_beaconing) {
                    // Schedule next computed posit time based on
                    // speed/turns, etc.
                    posit_next_time = posit_last_time + sb_POSIT_rate;
                    sb_last_heading = sb_current_heading;
                    //fprintf(stderr,"Sending Posit\n");
                }
                else {
                    // Schedule next fixed posit time, set in
                    // Configure->Defaults dialog
                    posit_next_time = posit_last_time + POSIT_rate;
                }

                transmit_now = 0;
                // Output to ALL net/tnc ports that are enabled & have tx enabled
//fprintf(stderr,"Sending posit\n");
                output_my_aprs_data();

                // Decrement the my_position_valid variable if we're
                // using GPS.  This will make sure that positions
                // are valid, as we'll only get four positions out
                // maximum per valid GPS position.  If the GPS
                // position goes stale, we'll stop sending posits.
                // We initialize it to one if we turn on a GPS
                // interface, so we'll get at the very most one
                // posit sent out with a stale position, each time
                // we open a GPS interface.
                if (using_gps_position && my_position_valid) {
                    my_position_valid--;
//fprintf(stderr,"my_position_valid:%d\n",my_position_valid);

                    if (!my_position_valid) {   // We just went to zero!
                        // Waiting for GPS data..
                        statusline(langcode("BBARSTA041"),1);

                        // If the user intends to send posits, GPS
                        // interface is enabled, and we're not
                        // getting GPS data, warn the user that
                        // posits are disabled.
                        if (!transmit_disable && !posit_tx_disable) {
                            popup_message_always(langcode("POPEM00033"),
                                langcode("POPEM00034"));
                        }
//fprintf(stderr,"my_position_valid just went to zero!\n");
                    }
                }
            }
//          if (do_time || transmit_now) {
//              transmit_now = 0;
//              // output to ALL net/tnc ports
//              //fprintf(stderr,"Output data\n");
//              output_my_aprs_data();
//          }

            // Must compute rain on a periodic basis, as some
            // weather daemons don't put out data often enough
            // to rotate through our queues.
            // We also refresh the Station_info dialog here if
            // it is currently drawn.
            if (current_time >= (last_weather_cycle + 30)) {    // Every 30 seconds
                // Note that we also write timestamps out to all of the log files
                // from this routine.  It works out well with the 30 second update
                // rate of cycle_weather().
                (void)cycle_weather();
                last_weather_cycle = current_time;

                if (station_data_auto_update)
                    update_station_info(w);  // Go refresh the Station Info display

                // Time to put out raw WX data ?
                if (current_time > sec_next_raw_wx) {
                    sec_next_raw_wx = current_time+600;

#ifdef TRANSMIT_RAW_WX
                    if (transmit_raw_wx)
                        tx_raw_wx_data();
#endif  // TRANSMIT_RAW_WX

                    // check wx data last received
                    wx_last_data_check();
                }
            }

            // is it time to spit out messages?
            check_and_transmit_messages(current_time);

            // Is it time to spit out any delayed ack's?
            check_delayed_transmit_queue(current_time);

            // Is it time to spit out objects/items?
            check_and_transmit_objects_items(current_time);

            // Do we have any new bulletins to display?
            check_for_new_bulletins(current_time);

            // Is it time to create a JPG snapshot?
            if (snapshots_enabled)
                (void)Snapshot();
            
            // Is it time to create a kml dump of all current stations
            if (kmlsnapshots_enabled) { 
                 if (sec_now() > (last_kmlsnapshot + (snapshot_interval * 60)) ) { 
                    last_kmlsnapshot = sec_now(); // Set up timer for next time
                    export_trail_as_kml(NULL);
                 }
            }

            // Is it time to refresh maps? 
            if ( map_refresh_interval && (current_time > map_refresh_time) ) {

                // Reload maps
                // Set interrupt_drawing_now because conditions have
                // changed.
                interrupt_drawing_now++;

                // Request that a new image be created.  Calls
                // create_image, XCopyArea, and display_zoom_status.
                request_new_image++;

//                if (create_image(da)) {
//                    (void)XCopyArea(XtDisplay(da),pixmap_final,XtWindow(da),gc,0,0,(unsigned int)screen_width,(unsigned int)screen_height,0,0);
//                }

                map_refresh_time = current_time + map_refresh_interval;
            }

            // get data from interfaces
            max=0;
            // Allow multiple packets to be processed inside this
            // loop.  Well, it was a nice idea anyway.  See the
            // below note.

// CAREFUL HERE:  If we try to send to the Spider pipes faster than
// it's reading from the pipes we corrupt the data out our server
// ports.  Having too high of a number here or putting too small of
// a delay down lower causes our server port to server up junk!

            while (max < 1 && !XtAppPending(app_context)) {
                struct timeval tmv;


// Check the x_spider server for incoming data
                if (enable_server_port) {
                    // Check whether the x_spider server pipes have
                    // any data for us.  Process if found.


// Check the TCP pipe
                    n = readline(pipe_tcp_server_to_xastir, line, MAX_LINE_SIZE);
                    if (n == 0) {
                        // Do nothing, empty packet
                    }
                    else if (n < 0) {
                        //fprintf(stderr,"UpdateTime: Readline error: %d\n",errno);
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            // This is normal if we have no data to read
                            //fprintf(stderr,"EAGAIN or EWOULDBLOCK\n");
                        }
                        else {  // Non-normal error.  Report it.
                            fprintf(stderr,"UpdateTime: Readline error: %d\n",errno);
                        }
                    }
                    else {  // We have a good packet
                        // Knock off the linefeed at the end
                        line[n-1] = '\0';

                        if (log_net_data)
                            log_data( get_user_base_dir(LOGFILE_NET),
                                (char *)line);

//fprintf(stderr,"TCP server data:%d: %s\n", n, line);

                        packet_data_add(langcode("WPUPDPD006"),
                            (char *)line,
                            -1);    // data_port -1 signifies x_spider

                        // Set port to -2 here to designate that it
                        // came from x_spider.  -1 = from a log
                        // file, 0 - 14 = from normal interfaces.
                        decode_ax25_line((char *)line,
                            'I',
                            -2, // Port -2 signifies x_spider data
                            1);

                        max++;  // Count the number of packets processed
                    }


// Check the UDP pipe
                    n = readline(pipe_udp_server_to_xastir, line, MAX_LINE_SIZE);
                    if (n == 0) {
                        // Do nothing, empty packet
                    }
                    else if (n < 0) {
                        //fprintf(stderr,"UpdateTime: Readline error: %d\n",errno);
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            // This is normal if we have no data to read
                            //fprintf(stderr,"EAGAIN or EWOULDBLOCK\n");
                        }
                        else {  // Non-normal error.  Report it.
                            fprintf(stderr,"UpdateTime: Readline error: %d\n",errno);
                        }
                    }
                    else {  // We have a good packet
                        char temp_call[10];
                        int skip_decode = 0;


                        // Knock off the linefeed at the end
                        line[n-1] = '\0';

                        // Check for "TO_INET," prefix, then check
                        // for "TO_RF," prefix.  Set appropriate
                        // flags and remove the prefixes if found.
                        // x_spider.c will always put them in that
                        // order if both flags are present, so we
                        // don't need to check for the reverse
                        // order.

                        // Check for "TO_INET," string
                        if (strncmp(line, "TO_INET,", 8) == 0) {
//                            fprintf(stderr,"Xastir received UDP packet with \"TO_INET,\" prefix\n");
                            line_offset += 8;
//
// "TO_INET" found.
// This packet should be gated to the internet if and only if
// igating is enabled.  This may happen automatically as-is, due to
// the decode_ax25_line() call below.  Check whether that's true.
//
// We can always add "NOGATE" or "RFONLY" to the path before we dump
// it to decode_ax25_line() in order to stop this igating...
//
                        }

                        // Check for "TO_RF," string
                        if (strncmp((char *)(line+line_offset), "TO_RF,", 6) == 0) {
//                            fprintf(stderr,"Xastir received UDP packet with \"TO_RF,\" prefix\n");
                            line_offset += 6;
//
// "TO_RF" found.
// This packet should be sent out the local RF ports.  If the
// callsign matches Xastir's (without the SSID), then send it out
// first-person format.  If it doesn't, send it out third-party
// format?
//
                            // Snag FROM callsign and do a non-exact
                            // match on it against "my_callsign"
                            xastir_snprintf(temp_call,
                                sizeof(temp_call),
                                (char *)(line+line_offset));
                            if (strchr(temp_call,'>')) {
                                *strchr(temp_call,'>') = '\0';
                            }

//                            if (is_my_call(temp_call, 0)) { // Match ignoring SSID
//                            exact match
                                // Send to RF as direct packet
//fprintf(stderr,"\tBase callsigns Match!  Send to RF as direct packet\n");
//fprintf(stderr,"\t%s\n", line);

// Change this to go out only RF interfaces so we don't double-up on
// the INET interfaces?  This would require looping on the
// interfaces and checking type and transmit_enable for each, as is
// done in output_igate_rf().  If we change to that method,
// re-enable the decode_ax25_line() call below.
//

// Change to a third-party packet.  In this case we know we have a
// line_offset, so backing up one to insert a char is ok.
*(line + line_offset - 1) = '}';

                            output_my_data(
                                (char *)(line + line_offset - 1),  // Raw data line
                                -1,    // ports, -1=send out all interfaces
                                0,     // type: 0=cooked, 1=raw
                                0,     // loopback_only
                                0,     // use_igate_path
                                NULL); // path

//skip_decode++;

                                igate_msgs_tx++;
//                            }
//                            else {  // Send to RF as 3rd party packet
//fprintf(stderr,
//    "\tBase callsigns don't match. Could send to RF as 3rd party packet, but dropping packet for now...\n");
//fprintf(stderr,"\t%s\n", line);

// Drop the packet for now, until we get more code added to turn it
// into a 3rd party packet

/*
                                output_igate_rf(temp_call,
                                    addr,
                                    path,
                                    (char *)(line + line_offset),
                                    port,
                                    1,
                                    NULL);

                            igate_msgs_tx++;
*/
//continue;
//                            }
                        }

                        if (log_net_data)
                            log_data( get_user_base_dir(LOGFILE_NET),
                                (char *)(line + line_offset));

//fprintf(stderr,"UDP server data:  %s\n", line);
//fprintf(stderr,"\tUDP server data2: %s\n\n", (char *)(line + line_offset));

                        packet_data_add(langcode("WPUPDPD006"),
                            (char *)(line + line_offset),
                            -1);    // data_port -1 signifies x_spider

// We don't need the below if we call output_my_data with -1 for the
// port, as in that case it calls decode_ax25_line directly.

if (!skip_decode) {

                            // Set port to -2 here to designate that it
                            // came from x_spider.  -1 = from a log
                            // file, 0 - 14 = from normal interfaces.
                            decode_ax25_line((char *)(line + line_offset),
                                'I',
                                -2, // Port -2 signifies x_spider data
                                1);

                            max++;  // Count the number of packets processed
}

                    }


                }
// End of x_spider server check code


//if (begin_critical_section(&data_lock, "main.c:UpdateTime(1)" ) > 0)
//    fprintf(stderr,"data_lock\n");


// Check the rest of the ports for incoming data.  Process up to
// 1000 packets here in a loop.

                data_length = pop_incoming_data(data_string, &data_port);
 
                if (data_length != 0) {
                    int data_type;              // 0=AX25, 1=GPS

                    // Terminate the string
                    data_string[data_length] = '\0';

                    //fprintf(stderr,"device_type: %d\n",port_data[data_port].device_type);

                    switch (port_data[data_port].device_type) {
                        // NET Data stream
                        case DEVICE_NET_STREAM:

                            if (log_net_data)
                                log_data( get_user_base_dir(LOGFILE_NET),
                                    (char *)data_string);

                            packet_data_add(langcode("WPUPDPD006"),
                                (char *)data_string,
                                data_port);

//fprintf(stderr,"\n-1 %s", data_string);

                            if (enable_server_port) {
                                char new_string[MAX_LINE_SIZE+1];

                                // Terminate it with a linefeed
                                xastir_snprintf(new_string,
                                    data_length+1,
                                    "%s\n",
                                    data_string);

//fprintf(stderr,"\n-2 %s", new_string);

                                // Send data to the x_spider server
                                if (writen(pipe_xastir_to_tcp_server,
                                        new_string,
                                        data_length+1) != data_length+1) {
                                    if (errno != EPIPE) {
                                        fprintf(stderr,
                                            "UpdateTime: Writen error (Net send x_spider): %d\n",
                                            errno);
                                    }
                                }
//fprintf(stderr,"\n-3 %s", new_string);

                            }
                            // End of x_spider server send code

                            decode_ax25_line((char *)data_string,
                                'I',
                                data_port,
                                1);
                            break;

                        // TNC Devices
                        case DEVICE_SERIAL_KISS_TNC:
                        case DEVICE_SERIAL_MKISS_TNC:

                            // Try to decode header and checksum.  If
                            // bad, break, else continue through to
                            // ASCII logging & decode routines.
                            // Note that the length of data_string
                            // can increase within decode_ax25_header().
                            if ( !decode_ax25_header( (unsigned char *)data_string,
                                    &data_length ) ) {
                                // Had a problem decoding it.  Drop
                                // it on the floor.
                                break;
                            }
                            else {
                                // Good decode.  Drop through to the
                                // next block to log and decode the
                                // packet.
                            }

                        case DEVICE_SERIAL_TNC:
                            tnc_data_clean((char *)data_string);

                        case DEVICE_AX25_TNC:
                        case DEVICE_NET_AGWPE:
                            if (log_tnc_data)
                                log_data( get_user_base_dir(LOGFILE_TNC),
                                    (char *)data_string);

                            packet_data_add(langcode("WPUPDPD005"),
                                (char *)data_string,
                                data_port);

                            if (enable_server_port) {
                                char new_string[MAX_LINE_SIZE+1];

                                // Terminate it with a linefeed
                                xastir_snprintf(new_string,
                                    MAX_LINE_SIZE+1,
                                    "%s\n",
                                    data_string);

                                // Send data to the x_spider server
                                if (writen(pipe_xastir_to_tcp_server,
                                        new_string,
                                        data_length+1) != data_length+1) {
                                    fprintf(stderr,
                                        "UpdateTime: Writen error (TNC Send x_spider): %d\n",
                                        errno);
                                }
                            }
                            // End of x_spider server send code

                            decode_ax25_line((char *)data_string,
                                'T',
                                data_port,
                                1);
                            break;

                        case DEVICE_SERIAL_TNC_HSP_GPS:
                            if (port_data[data_port].dtr==1) { // get GPS data
                                char temp[200];

                                (void)gps_data_find((char *)data_string,
                                    data_port);

                                xastir_snprintf(temp,
                                    sizeof(temp),
                                    "GPS: ");
                                strncat(temp,
                                    report_gps_status(),
                                    sizeof(temp) - strlen(temp));
                                statusline(temp, 0);
                            }
                            else {
                                // get TNC data
                                if (log_tnc_data)
                                    log_data( get_user_base_dir(LOGFILE_TNC),
                                        (char *)data_string);

                                packet_data_add(langcode("WPUPDPD005"),
                                    (char *)data_string,
                                    data_port);

                                if (enable_server_port) {
                                    char new_string[MAX_LINE_SIZE+1];

                                    // Terminate it with a linefeed
                                    xastir_snprintf(new_string,
                                        MAX_LINE_SIZE+1,
                                        "%s\n",
                                        data_string);

                                    // Send data to the x_spider server
                                    if (writen(pipe_xastir_to_tcp_server,
                                            new_string,
                                            data_length+1) != data_length+1) {
                                        fprintf(stderr,
                                            "UpdateTime: Writen error(HSP data): %d\n",
                                            errno);
                                    }
                                }
                                // End of x_spider server send code

                                decode_ax25_line((char *)data_string,
                                    'T',
                                    data_port,
                                    1);
                            }
                            break;

                        case DEVICE_SERIAL_TNC_AUX_GPS:
                            tnc_data_clean((char *)data_string);
                            data_type=tnc_get_data_type((char *)data_string,
                                data_port);
                            if (data_type) {  // GPS Data
                                char temp[200];

                                (void)gps_data_find((char *)data_string,
                                    data_port);

                                xastir_snprintf(temp,
                                    sizeof(temp),
                                    "GPS: ");
                                strncat(temp,
                                    report_gps_status(),
                                    sizeof(temp) - strlen(temp));
                                statusline(temp, 0);
                            }
                            else {          // APRS Data
                                if (log_tnc_data)
                                    log_data( get_user_base_dir(LOGFILE_TNC),
                                        (char *)data_string);

                                packet_data_add(langcode("WPUPDPD005"),
                                    (char *)data_string,
                                    data_port);

                                if (enable_server_port) {
                                    char new_string[MAX_LINE_SIZE+1];

                                    // Terminate it with a linefeed
                                    xastir_snprintf(new_string,
                                        MAX_LINE_SIZE+1,
                                        "%s\n",
                                        data_string);

                                    // Send data to the x_spider server
                                    if (writen(pipe_xastir_to_tcp_server,
                                            new_string,
                                            data_length+1) != data_length+1) {
                                        fprintf(stderr,
                                            "UpdateTime: Writen error(TNC/GPS data): %d\n",
                                            errno);
                                    }
                                }
                                // End of x_spider server send code

                                decode_ax25_line((char *)data_string,
                                    'T',
                                    data_port,
                                    1);
                            }
                            break;

                        // GPS Devices
                        case DEVICE_SERIAL_GPS:

                        case DEVICE_NET_GPSD:
                            //fprintf(stderr,"GPS Data <%s>\n",data_string);
                            (void)gps_data_find((char *)data_string,
                                data_port);
                            {
                                char temp[200];

                                xastir_snprintf(temp,
                                    sizeof(temp),
                                    "GPS: ");
                                strncat(temp,
                                    report_gps_status(),
                                    sizeof(temp) - strlen(temp));
                                statusline(temp, 0);
                            }
                            break;

                        // WX Devices
                        case DEVICE_SERIAL_WX:

                        case DEVICE_NET_WX:
                            if (log_wx)
                                log_data( get_user_base_dir(LOGFILE_WX),
                                    (char *)data_string);

                            wx_decode(data_string,
                                data_port);
                            break;

                        default:
                            fprintf(stderr,"Data from unknown source\n");
                            break;
                    }
                    max++;  // Count the number of packets processed
                } else {
                    max=1000;   // Go straight to "max": Exit loop
                }

//if (end_critical_section(&data_lock, "main.c:UpdateTime(2)" ) > 0)
//    fprintf(stderr,"data_lock\n");

                // Do a usleep() here to give the interface threads
                // time to put something in the queue if they still
                // have data to process.  We also need a delay here
                // to allow the x_spider code to process packets
                // we've sent to it.

// NOTE:  There's a very delicate balance here between x_spider
// server, sched_yield(), the delay below, and nexttime.  If we feed
// packets to the x_spider server faster than it gets to process
// them, we end up with blank lines and corrupted lines going to the
// connected clients.

                sched_yield();  // Yield to the other threads

                if (enable_server_port) {
                    tmv.tv_sec = 0;
                    tmv.tv_usec = 2000; // Delay 2ms
                    (void)select(0,NULL,NULL,NULL,&tmv);
                }

            }   // End of packet processing loop

            // END- get data from interface
            // READ FILE IF OPENED
            if (read_file) {
                if (current_time >= next_file_read) {
                    read_file_line(read_file_ptr);
                    next_file_read = current_time + REPLAY_DELAY;
                }
            }
            // END- READ FILE IF OPENED
        }

        // If number of stations has changed, update the status
        // line, but only once per second max.
        if (station_count != station_count_save
                && stations_status_time != current_time) {
            // show number of stations in status line
            xastir_snprintf(station_num,
                sizeof(station_num),
                langcode("BBARSTH001"),
                currently_selected_stations,
                station_count);
            XmTextFieldSetString(text3, station_num);

            // Set up for next time
            station_count_save = station_count;
            stations_status_time = current_time;
        }

        check_pointer_position();
    }

    sched_yield();  // Yield the processor to another thread

    (void)XtAppAddTimeOut(XtWidgetToApplicationContext(w),
        nexttime,
        (XtTimerCallbackProc)UpdateTime,
        (XtPointer)w);
}





void shut_down_server(void) {

    // Shut down the server if it was enabled
    if (tcp_server_pid || udp_server_pid) {

        // Send a kill to the main server process
        if (tcp_server_pid)
            kill(tcp_server_pid, SIGHUP);

        if (udp_server_pid)
            kill(udp_server_pid, SIGHUP);

        wait(NULL); // Reap the status of the process

        // Send to all processes in our process group.  This will
        // cause the server and all of its children to die.  Also
        // causes Xastir to die!  Don't do it!
        //kill(0, 1);

        sleep(1);

        // Send a more forceful kill signal in case the "nice" kill
        // signal didn't work.
        if (tcp_server_pid)
            kill(tcp_server_pid, SIGKILL);

        if (udp_server_pid)
            kill(udp_server_pid, SIGKILL);
    }
}





// This is the SIGHUP handler.  We restart Xastir if we receive a
// SIGHUP, hopefully with the same environment that the original
// Xastir had.  We set a global variable, then UpdateTime() is the
// process that actually calls execve() in order to replace our
// current process with the new one.  This assures that the signal
// handler gets reset.  We can't call execve() from inside the
// signal handler and have the restart work more than once.
//
// This function should be nearly identical to the quit() function
// below.
//
// One strangeness is that this routine gets called when any of the
// spawned processes get a SIGHUP also, which means when we shut
// down the TCP/UDP servers or similar.  For some reason it still
// appears to work, even though restart() gets called multiple
// times when we shut down Xastir or the servers.  We probably need
// to call signal() from outside any signal handlers to tell it to
// ignore further SIGHUP's.
//
void restart(int sig) {

//    if (debug_level & 1)
        fprintf(stderr,"Shutting down Xastir...\n");

    save_data();

    // shutdown all interfaces
    shutdown_all_active_or_defined_port(-1);

    shut_down_server();

#ifdef USE_PID_FILE_CHECK 
    // remove the PID file
    unlink(get_user_base_dir("xastir.pid")); 
#endif

#ifdef HAVE_LIBCURL
    curl_global_cleanup();
#endif

#ifdef USING_LIBGC
    CHECK_LEAKS();
#endif  // USING LIBGC

//    if (debug_level & 1)
        fprintf(stderr,"Attempting to restart Xastir...\n");

    // Set the global variable which tells UpdateTime() to do a
    // restart.
    //
    restart_xastir_now++;
}





void quit(int sig) {

    if(debug_level & 15)
        fprintf(stderr,"Caught %d\n",sig);

    save_data();

    // shutdown all interfaces
    shutdown_all_active_or_defined_port(-1);

    shut_down_server();


#ifdef USE_PID_FILE_CHECK 
    // remove the PID file
    unlink(get_user_base_dir("xastir.pid")); 
#endif

    if (debug_level & 1)
        fprintf(stderr,"Exiting Xastir...\n");

#ifdef HAVE_LIBCURL
    curl_global_cleanup();
#endif

    clear_application_context();
#ifdef USING_LIBGC
    CHECK_LEAKS();
#endif  // USING LIBGC

    exit(sig);  // Main exit from the program
}





#ifdef USE_PID_FILE_CHECK

int pid_file_check(void){

    int killret=0; 
    int other_pid=0;
    char temp[32] ; 
    FILE * PIDFILE ; 

    /* Save our PID */
    
    char pidfile_name[MAX_FILENAME];

    xastir_snprintf(pidfile_name, sizeof(pidfile_name),
        get_user_base_dir("xastir.pid")); 

    if (filethere(pidfile_name)){
        fprintf(stderr,"Found pid file: %s\n",pidfile_name);
        PIDFILE=fopen(pidfile_name,"r");
        if (PIDFILE!=NULL) {
            if(!feof(PIDFILE)) {
                (void)get_line(PIDFILE,temp,32);
            }
            (void)fclose(PIDFILE);
            other_pid=atoi(temp);
        } else {
            fprintf(stderr,"Couldn't open file: %s\n", pidfile_name);
        }

        // send a ping 
        killret = kill(other_pid,0); 

#ifdef N8YSZ
            fprintf(stderr, "other_pid = <%d> killret == <%d> errno == <%d>\n",
                other_pid,killret,errno); 
#endif
        if ((killret == -1) && (errno == ESRCH )){
            fprintf(stderr,
                "Other Xastir process, pid: %d does not appear be running. \n",
                other_pid);
	    // nuke from orbit
	    if (unlink(pidfile_name)) {
	            fprintf(stderr,"Error unlinking pid file: %s, %d\n",
                        pidfile_name,errno);
            }
        } else {
            fprintf(stderr,
                "Other Xastir process, pid: %d may be running. Exiting..\n",
                other_pid);
        
#ifdef USING_LIBGC
            CHECK_LEAKS();
#endif  // USING LIBGC
            exit(-1);  // Quick exit from the program
        }

    } else {
    	
    // if we're here - ok to truncate & open pidfile. 

#ifdef N8YSZ
        fprintf(stderr, "other_pid = <%d> killret == <%d> errno == <%d>\n",
                other_pid,killret,errno); 
#endif

        PIDFILE = fopen(pidfile_name,"w");
        if(PIDFILE != NULL){
            fprintf(PIDFILE, "%d",getpid()); 
            (void) fclose (PIDFILE);
            return(0); 
        } else {
            fprintf(stderr, "Error opening pidfile: %s\n", strerror(errno) );
            return(errno); 
        } 
        return(0);
    }
    return(0); 
} // end pid_file_check

#endif 


/* handle segfault signal */
void segfault(/*@unused@*/ int sig) {
    fprintf(stderr, "Caught Segfault! Xastir will terminate\n");
    fprintf(stderr, "Previous incoming line was: %s\n", incoming_data_copy_previous);
    fprintf(stderr, "    Last incoming line was: %s\n", incoming_data_copy);
    if (dangerous_operation[0] != '\0')
        fprintf(stderr, "Possibly died at: %s\n", dangerous_operation);
    fprintf(stderr, "%02d:%02d:%02d\n", get_hours(), get_minutes(), get_seconds() );

    shut_down_server();

    quit(-1);
}





/*  
   Added by KB4AMA
   Handle USR1 signal.  This will cause
   a snapshot to be generated.
*/
#ifndef OLD_PTHREADS
void usr1sig(int sig) {
    if (debug_level & 512)
        fprintf(stderr, "Caught Signal USR1, Doing a snapshot! Signal No %d\n", sig);

    last_snapshot = 0;
    (void)Snapshot();
}
#endif  // OLD_PTHREADS





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

#ifdef FIXED_DIALOG_STARTUP
    XtVaSetValues(w,XmNx,x,XmNy,y,NULL);
#else
    XtVaSetValues(w,XmNx,last_popup_x,XmNy,last_popup_y,NULL);
#endif // FIXED_DIALOG_STARTUP

    //fprintf(stderr,"max_x:%d max_y:%d x:%d y:%d wd:%d ht:%d last_x:%d last_y:%d\n",
        //max_x,max_y,x,y,wd,ht,last_popup_x,last_popup_y);
}



/*********************  fix dialog size *************************/

void fix_dialog_size(Widget w) {
    Dimension wd, ht;

    if (XtIsRealized(w)){
        XtVaGetValues(w,
                  XmNwidth, &wd,
                  XmNheight, &ht,
                  NULL);

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
                  NULL);

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
 *  Button callback for 1 out of 4 selection
 */
void sel4_switch(int switchpos, Widget first, Widget second, Widget third, Widget fourth) {
    if(switchpos == 3) {
        XtSetSensitive(first, FALSE);
        XtSetSensitive(second,TRUE);
        XtSetSensitive(third, TRUE);
        XtSetSensitive(fourth, TRUE);
    } else if(switchpos == 2) {
        XtSetSensitive(first, TRUE);
        XtSetSensitive(second,FALSE);
        XtSetSensitive(third, TRUE);
        XtSetSensitive(fourth, TRUE);
    } else if(switchpos == 1) {
        XtSetSensitive(first, TRUE);
        XtSetSensitive(second,TRUE);
        XtSetSensitive(third, FALSE);
        XtSetSensitive(fourth, TRUE);
    } else {
        XtSetSensitive(first, TRUE);
        XtSetSensitive(second,TRUE);
        XtSetSensitive(third, TRUE);
        XtSetSensitive(fourth, FALSE);
    }
}





// Called by UpdateTime when request_new_image flag is set.
void new_image(Widget da) {


    busy_cursor(appshell);

    // Reset flags
    interrupt_drawing_now = 0;
    request_new_image = 0;


    // Set up floating point lat/long values to match Xastir
    // coordinates (speeds things up when dealing with lat/long
    // values later).
    convert_from_xastir_coordinates(&f_center_longitude,
        &f_center_latitude,
        center_longitude,
        center_latitude);

    if (create_image(da)) {
        HandlePendingEvents(app_context);
        if (interrupt_drawing_now)
            return;

        (void)XCopyArea(XtDisplay(da),
            pixmap_final,
            XtWindow(da),
            gc,
            0,
            0,
            (unsigned int)screen_width,
            (unsigned int)screen_height,
            0,
            0);

        // We just refreshed the screen, so don't try to erase any
        // zoom-in boxes via XOR.
        zoom_box_x1 = -1;

        HandlePendingEvents(app_context);
        if (interrupt_drawing_now)
            return;

        display_zoom_status();
    }
}





/*
 *  Keep map in real world space, readjust center and scaling if neccessary
 */
void check_range(void) {
    Dimension width, height;


    XtVaGetValues(da,XmNwidth, &width,XmNheight, &height, NULL);

    // Check the window itself to see if our new y-scale fits it
    //
    if ((height*new_scale_y) > 64800000l) {

        // Center between 90°N and 90°S
        new_mid_y  =  64800000l/2;

        // Adjust y-scaling so that we fit perfectly in the window
        new_scale_y = 64800000l / height;
    }

    if ((new_mid_y < (height*new_scale_y)/2))
            new_mid_y  = (height*new_scale_y)/2;    // upper border max 90°N

    if ((new_mid_y + (height*new_scale_y)/2) > 64800000l)
        new_mid_y = 64800000l-((height*new_scale_y)/2); // lower border max 90°S

    // Adjust scaling based on latitude of new center
    new_scale_x = get_x_scale(new_mid_x,new_mid_y,new_scale_y);  // recalc x scaling depending on position
    //fprintf(stderr,"x:%ld\ty:%ld\n\n",new_scale_x,new_scale_y);

//    // scale_x will always be bigger than scale_y, so no problem here...
//    if ((width*new_scale_x) > 129600000l) {
//        // Center between 180°W and 180°E
//        new_mid_x = 129600000l/2;
//    }


// The below code causes the map image to snap to the left or right
// of the display.  I'd rather see the scale factor changed so that
// the map fits perfectly left/right in the display, so that we
// cannot go past the edges of the earth.  Change the code to work
// this way later.  We'll have to compute new_y_scale from the
// new_x_scale once we scale X appropriately, then will probably
// have to test the y scaling again?


/*
    // Check against left border
    if ((new_mid_x < (width*new_scale_x)/2)) {
        // This will cause the map image to snap to the left of the
        // display.
        new_mid_x = (width*new_scale_x)/2;  // left border max 180°W
    }
    else {
        // Check against right border
        if ((new_mid_x + (width*new_scale_x)/2) > 129600000l)
            // This will cause the map image to snap to the right of
            // the display.
            new_mid_x = 129600000l-((width*new_scale_x)/2); // right border max 180°E
    }
*/


// long NW_corner_longitude;             // Longitude at top NW corner of map screen
// long NW_corner_latitude;              // Latitude  at top NW corner of map screen

/*
if (NW_corner_longitude < 0l) {
//    fprintf(stderr,"left\n");
    NW_corner_longitude = 0l;         // New left viewpoint edge
    new_mid_x = 0l + ((width*new_scale_x) / 2); // New midpoint
}
if ( (NW_corner_longitude + (width*new_scale_x) ) > 129600000l) {
//    fprintf(stderr,"right\n");
    NW_corner_longitude = 129600000l - (width*new_scale_x);   // New left viewpoint edge
    new_mid_x = 129600000l - ((width*new_scale_x) / 2); // New midpoint
}
*/

// Find the four corners of the map in the new scale system.  Make
// sure they are on the display, but not well inside the borders of
// the display.

// We keep getting center_longitude out of range when zooming out
// and having the edge of the world map to the right of the middle
// of the window.  This shows up in new_image() above during the
// convert_from_xastir_coordinates() call.  new_mid_x is the data of
// interest in this routine.

}





/*
 *  Display a new map view after checking the view and scaling
 */
void display_zoom_image(int recenter) {
    Dimension width, height;

    XtVaGetValues(da,XmNwidth, &width,XmNheight, &height, NULL);
//fprintf(stderr,"Before,  x: %lu,  y: %lu\n",new_scale_x,new_scale_y);
    check_range();              // keep map inside world and calc x scaling
//fprintf(stderr,"After,   x: %lu,  y: %lu\n\n",new_scale_x,new_scale_y);
    if (new_mid_x != center_longitude
            || new_mid_y != center_latitude
            || new_scale_x != scale_x
            || new_scale_y != scale_y) {    // If there's been a change in zoom or center

        set_last_position();

        if (recenter) {
            center_longitude = new_mid_x;      // new map center
            center_latitude  = new_mid_y;
        }
        scale_x = new_scale_x;
        scale_y = new_scale_y;

        setup_in_view();    // update "in view" flag for all stations

        // Set the interrupt_drawing_now flag
        interrupt_drawing_now++;

        // Request that a new image be created.  Calls create_image,
        // XCopyArea, and display_zoom_status.
        request_new_image++;
//        last_input_event = sec_now() + 2;
        
    } else {    // No change in zoom or center.  Don't update ANYTHING.
    }
}





void Zoom_in( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer calldata) {
    Dimension width, height;

    if(display_up) {
        XtVaGetValues(da,XmNwidth, &width,XmNheight, &height, NULL);
        new_mid_x = center_longitude - ((width *scale_x)/2) + (menu_x*scale_x);
        new_mid_y = center_latitude  - ((height*scale_y)/2) + (menu_y*scale_y);
        new_scale_y = scale_y / 2;
        if (new_scale_y < 1)
            new_scale_y = 1;            // don't go further in
        display_zoom_image(1);          // check range and do display, recenter
    }
}





void Zoom_in_no_pan( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer calldata) {
    Dimension width, height;

    if(display_up) {
        XtVaGetValues(da,XmNwidth, &width,XmNheight, &height, NULL);
        new_mid_x = center_longitude;
        new_mid_y = center_latitude;
        new_scale_y = scale_y / 2;
        if (new_scale_y < 1)
            new_scale_y = 1;            // don't go further in, scale_x always bigger than scale_y
        display_zoom_image(0);          // check range and do display, keep center
    }
}





void Zoom_out(  /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer calldata) {
    Dimension width, height;

    if(display_up) {
        XtVaGetValues(da,XmNwidth, &width,XmNheight, &height, NULL);
        new_mid_x = center_longitude - ((width *scale_x)/2) + (menu_x*scale_x);
        new_mid_y = center_latitude  - ((height*scale_y)/2) + (menu_y*scale_y);
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
        XtVaGetValues(da,XmNwidth, &width,XmNheight, &height, NULL);
        new_mid_x = center_longitude;
        new_mid_y = center_latitude;
        if (width*scale_x < 129600000l || height*scale_y < 64800000l)
            new_scale_y = scale_y * 2;
        else
            new_scale_y = scale_y;      // don't zoom out if whole world could be shown
        display_zoom_image(0);          // check range and do display, keep center
    }
}





void Custom_Zoom_destroy_shell( /*@unused@*/ Widget widget, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    Widget shell = (Widget) clientData;
    XtPopdown(shell);
    XtDestroyWidget(shell);
    custom_zoom_dialog = (Widget)NULL;
}





static Widget custom_zoom_zoom_level;





void Custom_Zoom_do_it( /*@unused@*/ Widget widget, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    char *temp_ptr;

    temp_ptr = XmTextFieldGetString(custom_zoom_zoom_level);
    scale_y = atoi(temp_ptr);
    XtFree(temp_ptr);

    new_scale_y = scale_y;
    display_zoom_image(1);
}





// Function to bring up a dialog.  User can then select zoom for the
// display directly.
//
void Custom_Zoom( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer calldata) {
    static Widget  pane,form, button_ok, button_cancel, zoom_label;
//    Arg al[50];           /* Arg List */
//    unsigned int ac = 0;           /* Arg Count */
    Atom delw;
    char temp[50];

    if(!custom_zoom_dialog) {

        // "Custom Zoom"
        custom_zoom_dialog = XtVaCreatePopupShell(langcode("POPUPMA034"),
                xmDialogShellWidgetClass,
                appshell,
                XmNdeleteResponse,XmDESTROY,
                XmNdefaultPosition, FALSE,
                XmNresize, FALSE,
                XmNfontList, fontlist1,
                NULL);

        pane = XtVaCreateWidget("Jump_location pane",
                xmPanedWindowWidgetClass,
                custom_zoom_dialog,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        form =  XtVaCreateWidget("Jump_location form",
                xmFormWidgetClass,
                pane,
                XmNfractionBase, 2,
                XmNautoUnmanage, FALSE,
                XmNshadowThickness, 1,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        // "Zoom Level"
        zoom_label = XtVaCreateManagedWidget(langcode("POPUPMA004"),
                xmLabelWidgetClass,
                form,
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

        custom_zoom_zoom_level = XtVaCreateManagedWidget("Custom_Zoom zoom_level",
                xmTextFieldWidgetClass,
                form,
                XmNeditable,   TRUE,
                XmNcursorPositionVisible, TRUE,
                XmNsensitive, TRUE,
                XmNshadowThickness,      1,
                XmNcolumns,6,
                XmNwidth,((6*7)+2),
                XmNbackground, colors[0x0f],
                XmNtopAttachment,XmATTACH_FORM,
                XmNtopOffset, 5,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, zoom_label,
                XmNleftOffset, 10,
                XmNrightAttachment,XmATTACH_FORM,
                XmNrightOffset, 5,
                XmNfontList, fontlist1,
                NULL);

        button_ok = XtVaCreateManagedWidget(langcode("JMLPO00002"),
                xmPushButtonGadgetClass,
                form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, zoom_label,
                XmNtopOffset,15,
                XmNbottomAttachment, XmATTACH_FORM,
                XmNbottomOffset,5,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 0,
                XmNleftOffset, 3,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 1,
                XmNnavigationType, XmTAB_GROUP,
                XmNfontList, fontlist1,
                NULL);

        button_cancel = XtVaCreateManagedWidget(langcode("UNIOP00003"),
                xmPushButtonGadgetClass,
                form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, zoom_label,
                XmNtopOffset,15,
                XmNbottomAttachment, XmATTACH_FORM,
                XmNbottomOffset,5,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 1,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 2,
                XmNrightOffset, 3,
                XmNnavigationType, XmTAB_GROUP,
                XmNfontList, fontlist1,
                NULL);

        XtAddCallback(button_cancel, XmNactivateCallback, Custom_Zoom_destroy_shell, custom_zoom_dialog);
        XtAddCallback(button_ok, XmNactivateCallback, Custom_Zoom_do_it, NULL);

        pos_dialog(custom_zoom_dialog);

        delw = XmInternAtom(XtDisplay(custom_zoom_dialog),"WM_DELETE_WINDOW", FALSE);
        XmAddWMProtocolCallback(custom_zoom_dialog, delw, Custom_Zoom_destroy_shell, (XtPointer)custom_zoom_dialog);


        // Snag the current zoom value, convert them to
        // displayable values, and fill in the fields.
        xastir_snprintf(temp,
            sizeof(temp),
            "%ld",
            scale_y);
        XmTextFieldSetString(custom_zoom_zoom_level, temp); 


        XtManageChild(form);
        XtManageChild(pane);

        XtPopup(custom_zoom_dialog,XtGrabNone);
        fix_dialog_size(custom_zoom_dialog);

        // Move focus to the Close button.  This appears to
        // highlight the
        // button fine, but we're not able to hit the <Enter> key to
        // have that default function happen.  Note:  We _can_ hit
        // the
        // <SPACE> key, and that activates the option.
//        XmUpdateDisplay(custom_zoom_dialog);
        XmProcessTraversal(button_cancel, XmTRAVERSE_CURRENT);

    } else {
        XtPopup(custom_zoom_dialog,XtGrabNone);
        (void)XRaiseWindow(XtDisplay(custom_zoom_dialog), XtWindow(custom_zoom_dialog));
    }
}





void Zoom_level( /*@unused@*/ Widget w, XtPointer clientData, /*@unused@*/ XtPointer calldata) {
    Dimension width, height;
    int level;

    level=atoi((char *)clientData);
    if(display_up) {
        XtVaGetValues(da,XmNwidth, &width,XmNheight, &height, NULL);
        new_mid_x = center_longitude - ((width *scale_x)/2) + (menu_x*scale_x);
        new_mid_y = center_latitude  - ((height*scale_y)/2) + (menu_y*scale_y);
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

//WE7U
// Here it'd be good to calculate what zoom level the entire world
// would best fit in, instead of fixing the scale to a particular
// amount.  Figure out which zoom level would fit in the X and the Y
// direction, and then pick the higher zoom level of the two (to
// make sure the world fits in both).  We should probably center at
// 0.0N/0.0W as well.

                new_scale_y = 500000;

                // Center on Earth (0/0)
//                new_mid_x = 12900000l / 2;
//                new_mid_y = 6480000l  / 2;
                break;

            case(8):    // 10% out
                new_scale_y = (int)(scale_y * 1.1);
                if (new_scale_y == scale_y)
                  new_scale_y++;
                break;

            case(9):    // 10% in
                new_scale_y = (int)(scale_y * 0.9);
                // Don't allow the user to go in further than zoom 1
                if (new_scale_y < 1)
                    new_scale_y = 1;
                break;

            // Pop up a new dialog that allows the user to select
            // the zoom level, then causes that zoom level and the
            // right-click mouse location to take effect on the map
            // window.  Similar to the Center_Zoom function but with
            // the mouse coordinates instead of the center of the
            // screen.
            case(10):   // Custom Zoom Level

                // Pop up a new dialog that allows the user to
                // select the zoom level, then causes that zoom
                // level and the right-click mouse location to take
                // effect on the map window.  Similar to the
                // Center_Zoom function but with the mouse
                // coordinates instead of the center of the screen.

                // Set up new_scale_y for whatever custom zoom level
                // the user has chosen, then call
                // display_zoom_image() from the callback there.
                //
                Custom_Zoom( w, clientData, calldata);
                return;
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
        XtVaGetValues(da,XmNwidth, &width,XmNheight, &height, NULL);
        new_mid_x = center_longitude - ((width *scale_x)/2) + (menu_x*scale_x);
        new_mid_y = center_latitude  - ((height*scale_y)/2) + (menu_y*scale_y);
        new_scale_y = scale_y;          // keep size
        display_zoom_image(1);          // check range and do display, recenter
    }
}





void Pan_up( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer calldata) {
    Dimension width, height;

    if(display_up) {
        XtVaGetValues(da,XmNwidth, &width,XmNheight, &height, NULL);
        new_mid_x = center_longitude;
        new_mid_y = center_latitude  - (height*scale_y/4);
        new_scale_y = scale_y;          // keep size
        display_zoom_image(1);          // check range and do display, recenter
    }
}





void Pan_up_less( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer calldata) {
    Dimension width, height;

    if(display_up) {
        XtVaGetValues(da,XmNwidth, &width,XmNheight, &height, NULL);
        new_mid_x = center_longitude;
        new_mid_y = center_latitude  - (height*scale_y/10);
        new_scale_y = scale_y;          // keep size
        display_zoom_image(1);          // check range and do display, recenter
    }
}





void Pan_down( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer calldata) {
    Dimension width, height;

    if(display_up) {
        XtVaGetValues(da,XmNwidth, &width,XmNheight, &height, NULL);
        new_mid_x = center_longitude;
        new_mid_y = center_latitude  + (height*scale_y/4);
        new_scale_y = scale_y;          // keep size
        display_zoom_image(1);          // check range and do display, recenter
    }
}





void Pan_down_less( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer calldata) {
    Dimension width, height;

    if(display_up) {
        XtVaGetValues(da,XmNwidth, &width,XmNheight, &height, NULL);
        new_mid_x = center_longitude;
        new_mid_y = center_latitude  + (height*scale_y/10);
        new_scale_y = scale_y;          // keep size
        display_zoom_image(1);          // check range and do display, recenter
    }
}





void Pan_left( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer calldata) {
    Dimension width, height;

    if(display_up) {
        XtVaGetValues(da,XmNwidth, &width,XmNheight, &height, NULL);
        new_mid_x = center_longitude - (width*scale_x/4);
        new_mid_y = center_latitude;
        new_scale_y = scale_y;          // keep size
        display_zoom_image(1);          // check range and do display, recenter
    }
}





void Pan_left_less( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer calldata) {
    Dimension width, height;

    if(display_up) {
        XtVaGetValues(da,XmNwidth, &width,XmNheight, &height, NULL);
        new_mid_x = center_longitude - (width*scale_x/10);
        new_mid_y = center_latitude;
        new_scale_y = scale_y;          // keep size
        display_zoom_image(1);          // check range and do display, recenter
    }
}





void Pan_right( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer calldata) {
    Dimension width, height;

    if(display_up) {
        XtVaGetValues(da,XmNwidth, &width,XmNheight, &height, NULL);
        new_mid_x = center_longitude + (width*scale_x/4);
        new_mid_y = center_latitude;
        new_scale_y = scale_y;          // keep size
        display_zoom_image(1);          // check range and do display, recenter
    }
}





void Pan_right_less( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer calldata) {
    Dimension width, height;

    if(display_up) {
        XtVaGetValues(da,XmNwidth, &width,XmNheight, &height, NULL);
        new_mid_x = center_longitude + (width*scale_x/10);
        new_mid_y = center_latitude;
        new_scale_y = scale_y;          // keep size
        display_zoom_image(1);          // check range and do display, recenter
    }
}





void Center_Zoom_destroy_shell( /*@unused@*/ Widget widget, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    Widget shell = (Widget) clientData;
    XtPopdown(shell);
    XtDestroyWidget(shell);
    center_zoom_dialog = (Widget)NULL;
}





static Widget center_zoom_latitude,
            center_zoom_longitude,
            center_zoom_zoom_level;





void Center_Zoom_do_it( /*@unused@*/ Widget widget, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    unsigned long x, y;
    char *temp_ptr;


    temp_ptr = XmTextFieldGetString(center_zoom_latitude);
    f_center_latitude  = atof(temp_ptr); 
    XtFree(temp_ptr);

    temp_ptr = XmTextFieldGetString(center_zoom_longitude);
    f_center_longitude = atof(temp_ptr);
    XtFree(temp_ptr);

    //Convert to Xastir coordinate system for lat/long
    convert_to_xastir_coordinates(&x,
        &y,
        f_center_longitude,
        f_center_latitude);

    temp_ptr = XmTextFieldGetString(center_zoom_zoom_level);
    scale_y = atoi(temp_ptr);
    XtFree(temp_ptr);

    new_mid_x = x;
    new_mid_y = y;
    new_scale_y = scale_y;
    display_zoom_image(1);
}





void Go_Home( Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer callData) {
    DataRow *p_station;

    if (search_station_name(&p_station,my_callsign,1)) {
        set_map_position(w, p_station->coord_lat, p_station->coord_lon);
    }
}





// Function to bring up a dialog.  User can then select the center
// and zoom for the display directly.
//
// Later it would be nice to have a "Calc" button so that the user
// could input lat/long in any of the supported formats.  Right now
// it is DD.DDDD format only.
//
void Center_Zoom( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer calldata) {
    static Widget  pane,form, button_ok, button_cancel,
            lat_label, lon_label, zoom_label;
//    Arg al[50];           /* Arg List */
//    unsigned int ac = 0;           /* Arg Count */
    Atom delw;
    char temp[50];

    if(!center_zoom_dialog) {

        // "Center & Zoom"
        center_zoom_dialog = XtVaCreatePopupShell(langcode("POPUPMA026"),
                xmDialogShellWidgetClass,
                appshell,
                XmNdeleteResponse,XmDESTROY,
                XmNdefaultPosition, FALSE,
                XmNresize, FALSE,
                XmNfontList, fontlist1,
                NULL);

        pane = XtVaCreateWidget("Jump_location pane",
                xmPanedWindowWidgetClass,
                center_zoom_dialog,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        form =  XtVaCreateWidget("Jump_location form",
                xmFormWidgetClass,
                pane,
                XmNfractionBase, 2,
                XmNautoUnmanage, FALSE,
                XmNshadowThickness, 1,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        // "Latitude"
        lat_label = XtVaCreateManagedWidget(langcode("POPUPMA027"),
                xmLabelWidgetClass,
                form,
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

        center_zoom_latitude = XtVaCreateManagedWidget("Center_Zoom latitude",
                xmTextFieldWidgetClass,
                form,
                XmNeditable,   TRUE,
                XmNcursorPositionVisible, TRUE,
                XmNsensitive, TRUE,
                XmNshadowThickness,      1,
                XmNcolumns,21,
                XmNwidth,((21*7)+2),
                XmNbackground, colors[0x0f],
                XmNtopAttachment,XmATTACH_FORM,
                XmNtopOffset, 5,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, lat_label,
                XmNleftOffset, 10,
                XmNrightAttachment,XmATTACH_FORM,
                XmNrightOffset, 5,
                XmNfontList, fontlist1,
                NULL);

        // "Longitude"
        lon_label = XtVaCreateManagedWidget(langcode("POPUPMA028"),
                xmLabelWidgetClass,
                form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, lat_label,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 5,
                XmNrightAttachment, XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        center_zoom_longitude = XtVaCreateManagedWidget("Center_Zoom longitude",
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
                XmNtopWidget, lat_label,
                XmNtopOffset, 5,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, lon_label,
                XmNleftOffset, 10,
                XmNrightAttachment,XmATTACH_FORM,
                XmNrightOffset, 5,
                XmNfontList, fontlist1,
                NULL);

        // "Zoom Level"
        zoom_label = XtVaCreateManagedWidget(langcode("POPUPMA004"),
                xmLabelWidgetClass,
                form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, lon_label,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 5,
                XmNrightAttachment, XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        center_zoom_zoom_level = XtVaCreateManagedWidget("Center_Zoom zoom_level",
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
                XmNtopWidget, lon_label,
                XmNtopOffset, 5,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, zoom_label,
                XmNleftOffset, 10,
                XmNrightAttachment,XmATTACH_FORM,
                XmNrightOffset, 5,
                XmNfontList, fontlist1,
                NULL);

        button_ok = XtVaCreateManagedWidget(langcode("JMLPO00002"),
                xmPushButtonGadgetClass,
                form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, zoom_label,
                XmNtopOffset,15,
                XmNbottomAttachment, XmATTACH_FORM,
                XmNbottomOffset,5,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 0,
                XmNleftOffset, 3,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 1,
                XmNnavigationType, XmTAB_GROUP,
                XmNfontList, fontlist1,
                NULL);

        button_cancel = XtVaCreateManagedWidget(langcode("UNIOP00003"),
                xmPushButtonGadgetClass,
                form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, zoom_label,
                XmNtopOffset,15,
                XmNbottomAttachment, XmATTACH_FORM,
                XmNbottomOffset,5,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 1,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 2,
                XmNrightOffset, 3,
                XmNnavigationType, XmTAB_GROUP,
                XmNfontList, fontlist1,
                NULL);

        XtAddCallback(button_cancel, XmNactivateCallback, Center_Zoom_destroy_shell, center_zoom_dialog);
        XtAddCallback(button_ok, XmNactivateCallback, Center_Zoom_do_it, NULL);

        pos_dialog(center_zoom_dialog);

        delw = XmInternAtom(XtDisplay(center_zoom_dialog),"WM_DELETE_WINDOW", FALSE);
        XmAddWMProtocolCallback(center_zoom_dialog, delw, Center_Zoom_destroy_shell, (XtPointer)center_zoom_dialog);


        if (center_zoom_override) { // We've found a Map View
                                    // "eyeball" object and are
                                    // doing a center/zoom based on
                                    // that object's info.  Grab the
                                    // pointer to the object which
                                    // is in calldata.
            DataRow *p_station = (DataRow *)calldata;
            float f_latitude, f_longitude;
            int range;
            Dimension width, height;
            long x, x0, y, y0;
            double x_miles, y_miles, distance;
            char temp_course[10];
            double scale_factor;
            long my_scale_y;
            int fell_off = 0;


//fprintf(stderr,"Map View Object: %s\n",p_station->call_sign);

            center_zoom_override = 0;


            // Snag the objects values, convert them to displayable
            // values, and fill in the fields.
            convert_from_xastir_coordinates(&f_longitude,
                &f_latitude,
                p_station->coord_lon,
                p_station->coord_lat);

            xastir_snprintf(temp,
                sizeof(temp),
                "%f",
                f_latitude);
            XmTextFieldSetString(center_zoom_latitude, temp); 

            xastir_snprintf(temp,
                sizeof(temp),
                "%f",
                f_longitude);
            XmTextFieldSetString(center_zoom_longitude, temp); 

            // Compute the approximate zoom level we need from the
            // range value in the object.  Range is in miles.
            range = atoi(&p_station->power_gain[3]);

            // We should be able to compute the distance across the
            // screen that we currently have, then compute an
            // accurate zoom level that will give us the range we
            // want.

            // Find out the screen values
            XtVaGetValues(da,XmNwidth, &width, XmNheight, &height, NULL);

            // Convert points to Xastir coordinate system

            // X
            x = center_longitude  - ((width *scale_x)/2);

            // Check for the edge of the earth
            if (x < 0) {
                x = 0;
                fell_off++; // Fell off the edge of the earth
            }

            x0 = center_longitude; // Center of screen

            // Y
            y = center_latitude   - ((height*scale_y)/2);

            // Check for the edge of the earth
            if (y < 0) {
                y = 0;
                fell_off++; // Fell off the edge of the earth
            }

            y0 = center_latitude;  // Center of screen

            // Compute distance from center to each edge

            // X distance.  Keep Y constant.
            x_miles = cvt_kn2len
                * calc_distance_course(y0,
                    x0,
                    y0,
                    x,
                    temp_course,
                    sizeof(temp_course));

            // Y distance.  Keep X constant.
            y_miles = cvt_kn2len
                * calc_distance_course(y0,
                    x0,
                    y,
                    x0,
                    temp_course,
                    sizeof(temp_course));

            // Choose the smaller distance
            if (x_miles < y_miles) {
                distance = x_miles;
            }
            else {
                distance = y_miles;
            }
//fprintf(stderr,"Current screen range: %f\n", distance);
//fprintf(stderr,"Desired screen range: %d\n", range);

// Note that these numbers will be off if we're zoomed out way too
// far (edges of the earth are inside the screen view).

            // Now we know the range of the current screen
            // (distance) in miles.  Compute what we need from
            // "distance" (screen) and "range" (object) in order to
            // get a scale factor we can apply to our zoom numbers.
            if (distance < range) {
//fprintf(stderr,"Zooming out\n");
                scale_factor = (range * 1.0)/distance;
//                fprintf(stderr,"Scale Factor: %f\n", scale_factor);
                my_scale_y = (long)(scale_y * scale_factor);
            }
            else {  // distance > range
//fprintf(stderr,"Zooming in\n");
                scale_factor = distance/(range * 1.0);
//fprintf(stderr,"Scale Factor: %f\n", scale_factor);
                my_scale_y = (long)(scale_y / scale_factor);
            }

            if (my_scale_y < 1)
                my_scale_y = 1;

//fprintf(stderr,"my_scale_y: %ld\n", my_scale_y);

            xastir_snprintf(temp,
                sizeof(temp),
                "%ld",
                my_scale_y);
            XmTextFieldSetString(center_zoom_zoom_level, temp); 
        }
        else {
            // Normal user-initiated center/zoom function

            // Snag the current lat/long/center values, convert them to
            // displayable values, and fill in the fields.
            xastir_snprintf(temp,
                sizeof(temp),
                "%f",
                f_center_latitude);
            XmTextFieldSetString(center_zoom_latitude, temp); 

            xastir_snprintf(temp,
                sizeof(temp),
                "%f",
                f_center_longitude);
            XmTextFieldSetString(center_zoom_longitude, temp); 

            xastir_snprintf(temp,
                sizeof(temp),
                "%ld",
                scale_y);
            XmTextFieldSetString(center_zoom_zoom_level, temp); 
        }


        XtManageChild(form);
        XtManageChild(pane);

        XtPopup(center_zoom_dialog,XtGrabNone);
        fix_dialog_size(center_zoom_dialog);

        // Move focus to the Close button.  This appears to
        // highlight the
        // button fine, but we're not able to hit the <Enter> key to
        // have that default function happen.  Note:  We _can_ hit
        // the
        // <SPACE> key, and that activates the option.
//        XmUpdateDisplay(center_zoom_dialog);
        XmProcessTraversal(button_cancel, XmTRAVERSE_CURRENT);

    } else {
        XtPopup(center_zoom_dialog,XtGrabNone);
        (void)XRaiseWindow(XtDisplay(center_zoom_dialog), XtWindow(center_zoom_dialog));
    }
}





void SetMyPosition( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer calldata) {
    
    Dimension width, height;
    long my_new_latl, my_new_lonl;

    // check for fixed station
    //if ( (output_station_type == 0) || (output_station_type > 3)) {
    //  popup_message( "Modify fixed position", "Are you sure you want to modify your position?");
    //}
    // check for position abiguity
    if ( position_amb_chars > 0 ) { // popup warning that ambiguity is on
        popup_message_always(langcode("POPUPMA043"), // "Modify ambiguous position"
            langcode("POPUPMA044") );   // "Position abiguity is on, your new position may appear to jump."
    }

    if(display_up) {
        XtVaGetValues(da,XmNwidth, &width,XmNheight, &height, NULL);
        my_new_lonl = (center_longitude - ((width *scale_x)/2) + (menu_x*scale_x));
        my_new_latl = (center_latitude  - ((height*scale_y)/2) + (menu_y*scale_y));
        // Check if we are still on the planet... 
        if ( my_new_latl > 64800000l  ) my_new_latl = 64800000l;
        if ( my_new_latl < 0 ) my_new_latl = 0;
        if ( my_new_lonl > 129600000l ) my_new_lonl = 129600000l;
        if ( my_new_lonl < 0 ) my_new_lonl = 0;

        convert_lon_l2s( my_new_lonl, my_long, sizeof(my_long), CONVERT_HP_NOSP);
        convert_lat_l2s( my_new_latl,  my_lat,  sizeof(my_lat),  CONVERT_HP_NOSP);

        // Update my station data with the new lat/lon
            my_station_add(my_callsign,my_group,my_symbol,my_long,my_lat,my_phg,my_comment,(char)position_amb_chars);
            redraw_on_new_data=2;
    }
}





void Window_Quit( /*@unused@*/ Widget w, /*@unused@*/ XtPointer client, /*@unused@*/ XtPointer calldata) {
    quit(0);
}





void Menu_Quit( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer calldata) {
    quit(0);
}





// Turn on or off map border, callback from map_border_button.
void Map_border_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;
    
    if(state->set)
        draw_labeled_grid_border = TRUE;
    else
        draw_labeled_grid_border = FALSE;

    redraw_symbols(da);
}



void Grid_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        long_lat_grid = atoi(which);
    else
        long_lat_grid = 0;

    if (long_lat_grid) {
        statusline(langcode("BBARSTA005"),1);   // Map Lat/Long Grid On
        XtSetSensitive(map_border_button,TRUE);
    }
    else {
        statusline(langcode("BBARSTA006"),2);   // Map Lat/Long Grid Off
        XtSetSensitive(map_border_button,FALSE);
    }

    redraw_symbols(da);
    (void)XCopyArea(XtDisplay(da),
        pixmap_final,
        XtWindow(da),
        gc,
        0,
        0,
        (unsigned int)screen_width,
        (unsigned int)screen_height,
        0,
        0);
}




// Callback from menu buttons that allow user to turn on or off the 
// global display of CAD objects and their metadata on the map.
void  CAD_draw_toggle( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    // turn on or off dsiplay of cad objects 
    if (strcmp(which,"CAD_draw_objects")==0) { 
         CAD_draw_objects = state->set;
    }

    // uurn on or off display of standard metadata
    if (strcmp(which,"CAD_show_label")==0) { 
         CAD_show_label = state->set;
    }
    if (strcmp(which,"CAD_show_raw_probability")==0) { 
         CAD_show_raw_probability = state->set;
    }
    if (strcmp(which,"CAD_show_comment")==0) { 
         CAD_show_comment = state->set;
    }
    if (strcmp(which,"CAD_show_area")==0) { 
         CAD_show_area = state->set;
    }

    // redraw objects on the current base maps
    redraw_symbols(da);
}





void  Map_disable_toggle( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set) {
        disable_all_maps = atoi(which);
    }
    else {
        disable_all_maps = 0;
    }

    request_new_image++;

//    if (create_image(da)) {
//        (void)XCopyArea(XtDisplay(da),pixmap_final,XtWindow(da),gc,0,0,(unsigned int)screen_width,(unsigned int)screen_height,0,0);
//    }
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

    re_sort_maps = 1;

    // Set interrupt_drawing_now because conditions have changed.
    interrupt_drawing_now++;

    // Request that a new image be created.  Calls create_image,
    // XCopyArea, and display_zoom_status.
    request_new_image++;

//    if (create_image(da)) {
//        (void)XCopyArea(XtDisplay(da),pixmap_final,XtWindow(da),gc,0,0,(unsigned int)screen_width,(unsigned int)screen_height,0,0);
//    }

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

    re_sort_maps = 1;

    // Set interrupt_drawing_now because conditions have changed.
    interrupt_drawing_now++;

    // Request that a new image be created.  Calls create_image,
    // XCopyArea, and display_zoom_status.
    request_new_image++;

//    if (create_image(da)) {
//        (void)XCopyArea(XtDisplay(da),pixmap_final,XtWindow(da),gc,0,0,(unsigned int)screen_width,(unsigned int)screen_height,0,0);
//    }
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

    // Set interrupt_drawing_now because conditions have changed.
    interrupt_drawing_now++;

    // Request that a new image be created.  Calls create_image,
    // XCopyArea, and display_zoom_status.
    request_new_image++;

//    if (create_image(da)) {
//        (void)XCopyArea(XtDisplay(da),pixmap_final,XtWindow(da),gc,0,0,(unsigned int)screen_width,(unsigned int)screen_height,0,0);
//    }
}





void  Map_labels_toggle( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        map_labels = atoi(which);
    else
        map_labels = 0;

    // Set interrupt_drawing_now because conditions have changed.
    interrupt_drawing_now++;

    // Request that a new image be created.  Calls create_image,
    // XCopyArea, and display_zoom_status.
    request_new_image++;

//    if (create_image(da)) {
//        (void)XCopyArea(XtDisplay(da),pixmap_final,XtWindow(da),gc,0,0,(unsigned int)screen_width,(unsigned int)screen_height,0,0);
//    }
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

    // Set interrupt_drawing_now because conditions have changed.
    interrupt_drawing_now++;

    // Request that a new image be created.  Calls create_image,
    // XCopyArea, and display_zoom_status.
    request_new_image++;

//    if (create_image(da)) {
//        (void)XCopyArea(XtDisplay(da),pixmap_final,XtWindow(da),gc,0,0,(unsigned int)screen_width,(unsigned int)screen_height,0,0);
//    }
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

        // Set interrupt_drawing_now because conditions have changed.
        interrupt_drawing_now++;

        // Request that a new image be created.  Calls create_image,
        // XCopyArea, and display_zoom_status.
        request_new_image++;

//        if (create_image(da)) {
//            (void)XCopyArea(XtDisplay(da),pixmap_final,XtWindow(da),gc,0,0,(unsigned int)screen_width,(unsigned int)screen_height,0,0);
//        }
    }
}





#if !defined(NO_GRAPHICS)
void Raster_intensity(Widget w, XtPointer clientData, XtPointer calldata) {
    float my_intensity;
    int i;

    my_intensity=atof((char *)clientData);

    if(display_up){
        for (i=0;i<11;i++){
            if (i == (int)((float)(my_intensity * 10.01)) )
                XtSetSensitive(raster_intensity[i],FALSE);
            else
                XtSetSensitive(raster_intensity[i],TRUE);

            //fprintf(stderr,"Change to index: %d\n", (int)((float)(my_intensity * 10.01)));
        }

        raster_map_intensity=my_intensity;
        //fprintf(stderr,"raster_map_intensity = %f\n", raster_map_intensity);

        // Set interrupt_drawing_now because conditions have changed.
        interrupt_drawing_now++;

        // Request that a new image be created.  Calls create_image,
        // XCopyArea, and display_zoom_status.
        request_new_image++;

//        if (create_image(da)) {
//            XCopyArea(XtDisplay(da),pixmap_final,XtWindow(da),gc,0,0,(unsigned int)screen_width,(unsigned int)screen_height,0,0);
//        }
    }
}
#endif  // NO_GRAPHICS





void Map_station_label( /*@unused@*/ Widget w, XtPointer clientData, /*@unused@*/ XtPointer calldata) {
    int style;

    style=atoi((char *)clientData);

    if(display_up){
        letter_style = style;
        sel3_switch(letter_style,map_station_label2,map_station_label1,map_station_label0);
        redraw_symbols(da);
        (void)XCopyArea(XtDisplay(da),
            pixmap_final,
            XtWindow(da),
            gc,
            0,
            0,
            (unsigned int)screen_width,
            (unsigned int)screen_height,
            0,
            0);
    }
}





void Map_icon_outline( /*@unused@*/ Widget w, XtPointer clientData, /*@unused@*/ XtPointer calldata) {
    int style;

    style=atoi((char *)clientData);

    if(display_up){
        icon_outline_style = style;
        sel4_switch(icon_outline_style,map_icon_outline3,map_icon_outline2,map_icon_outline1,map_icon_outline0);
        redraw_symbols(da);
        (void)XCopyArea(XtDisplay(da),
            pixmap_final,
            XtWindow(da),
            gc,
            0,
            0,
            (unsigned int)screen_width,
            (unsigned int)screen_height,
            0,
            0);
    }
    statusline( langcode("BBARSTA046"), 1);   // Reloading symbols...
    load_pixmap_symbol_file("symbols.dat", 1);
    redraw_symbols(da);
    (void)XCopyArea(XtDisplay(da),
        pixmap_final,
        XtWindow(da),
        gc,
        0,
        0,
        (unsigned int)screen_width,
        (unsigned int)screen_height,
        0,
        0);
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
        (void)XCopyArea(XtDisplay(da),
            pixmap_final,
            XtWindow(da),
            gc,
            0,
            0,
            (unsigned int)screen_width,
            (unsigned int)screen_height,
            0,
            0);
    }
}





void  Index_maps_on_startup_toggle( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        index_maps_on_startup = 1;
    else
        index_maps_on_startup = 0;
}





void TNC_Transmit_now( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer calldata) {
    transmit_now = 1;              /* toggle transmission of station now*/
}





/////////////////////////////////////////////////////////////////////
// GPS operations
/////////////////////////////////////////////////////////////////////





#ifdef HAVE_GPSMAN

// Function to process the RINO.gpstrans file.  We'll create APRS
// objects out of them as if our own callsign created them.  Lines
// in the file look like this (spaces removed):
//
// W  N3EG3  20-JUN-02 17:55  07/08/2004 13:03:29  46.1141682  -122.9384817
// W  N3JGI  20-JUN-02 18:29  07/08/2004 13:03:29  48.0021644  -116.0118324
//
// Fields are:
// W  name   Comment          Date/Time            Latitude    Longitude
//
void process_RINO_waypoints(void) {
    FILE *f;
    char temp[MAX_FILENAME * 2];
    char line[301];
    float UTC_Offset;
//    char datum[50];


    // Just to be safe
    line[300] = '\0';

    // Create the full path/filename
    xastir_snprintf(temp,
        sizeof(temp),
        "%s/RINO.gpstrans",
        get_user_base_dir("gps"));

    f=fopen(temp,"r"); // Open for reading

    if (f == NULL) {
        fprintf(stderr,
            "Couldn't open %s file for reading\n",
            temp);
        return;
    }
 
    // Process the file line-by-line here.  The format for gpstrans
    // files as written by GPSMan appears to be:
    //
    //   "W"
    //   Waypoint name
    //   Comment field (Date/Time is default on Garmins)
    //   Date/Time
    //   Decimal latitude
    //   Decimal longitude
    //
    while (fgets(line, 300, f) != NULL) {

        // Snag the "Format:" line at the top of the file:
        // Format: DDD  UTC Offset:  -8.00 hrs  Datum[100]: WGS 84
        //
        if (strncmp(line,"Format:",7) == 0) {
            int i = 7;
            char temp2[50];


            // Find the ':' after "UTC Offset"
            while (line[i] != ':'
                    && line[i] != '\0'
                    && line[i] != '\n'
                    && line[i] != '\r') {
                i++;
            }
            i++;

            // Skip white space
            while (line[i] == ' '
                    && line[i] != '\0'
                    && line[i] != '\n'
                    && line[i] != '\r') {
                i++;
            }

            // Copy UTC offset chars into temp2
            temp2[0] = '\0';
            while (line[i] != '\t'
                    && line[i] != ' '
                    && line[i] != '\0'
                    && line[i] != '\n'
                    && line[i] != '\r') {
                strncat(temp2,&line[i],1);
                i++;
            }

            UTC_Offset = atof(temp2);

//fprintf(stderr,"UTC Offset: %f\n", UTC_Offset);

// NOTE:  This would be the place to snag the datum as well.

        }

        // Check for a waypoint entry
        else if (line[0] == 'W') {
            char name[50];
            char datetime[50];
            char lat_c[20];
            char lon_c[20];
            int i = 1;


// NOTE:  We should check for the end of the string, skipping this
// iteration of the loop if we haven't parsed enough fields.

            // Find non-white-space character
            while ((line[i] == '\t' || line[i] == ' ')
                    && line[i] != '\0'
                    && line[i] != '\n'
                    && line[i] != '\r') {
                i++;
            }

            // Copy into name until tab or whitespace char.  We're
            // assuming that a waypoint name can't have spaces in
            // it.
            name[0] = '\0';
            while (line[i] != '\t'
                    && line[i] != ' '
                    && line[i] != '\0'
                    && line[i] != '\n'
                    && line[i] != '\r') {
                strncat(name,&line[i],1);
                i++;
            }

            // Find tab character at end of name field
            while (line[i] != '\t'
                    && line[i] != '\0'
                    && line[i] != '\n'
                    && line[i] != '\r') {
                i++;
            }
            i++;

            // We skip the comment field, doing nothing with the
            // data.
            //
            // Find tab character at end of comment field
            while (line[i] != '\t'
                    && line[i] != '\0'
                    && line[i] != '\n'
                    && line[i] != '\r') {
                i++;
            }
            i++;

            // Find non-white-space character
            while ((line[i] == '\t' || line[i] == ' ')
                    && line[i] != '\0'
                    && line[i] != '\n'
                    && line[i] != '\r') {
                i++;
            }

// Snag date/time.  Use it in the object date/time field.
            // Copy into datetime until tab char.  Include the space
            // between the time and date portions.
            datetime[0] = '\0';
            while (line[i] != '\t'
                    && line[i] != '\0'
                    && line[i] != '\n'
                    && line[i] != '\r') {
                strncat(datetime,&line[i],1);
                i++;
            }

            // Find tab character at end of date/time field
            while (line[i] != '\t'
                    && line[i] != '\0'
                    && line[i] != '\n'
                    && line[i] != '\r') {
                i++;
            }
            i++;

            // Find non-white-space character
            while ((line[i] == '\t' || line[i] == ' ')
                    && line[i] != '\0'
                    && line[i] != '\n'
                    && line[i] != '\r') {
                i++;
            }

            // Copy into lat_c until white space char
            lat_c[0] = '\0';
            while (line[i] != '\t'
                    && line[i] != ' '
                    && line[i] != '\0'
                    && line[i] != '\n'
                    && line[i] != '\r') {
                strncat(lat_c,&line[i],1);
                i++;
            }

            // Find non-white-space character
            while ((line[i] == '\t' || line[i] == ' ')
                    && line[i] != '\0'
                    && line[i] != '\n'
                    && line[i] != '\r') {
                i++;
            }

            // Copy into lon_c until tab character
            lon_c[0] = '\0';
            while (line[i] != '\t'
                    && line[i] != ' '
                    && line[i] != '\0'
                    && line[i] != '\n'
                    && line[i] != '\r') {
                strncat(lon_c,&line[i],1);
                i++;
            }
            i++;

/*
            fprintf(stderr,
                "%s\t%f\t%f\n",
                name,
                atof(lat_c),
                atof(lon_c));
*/


// For now we're hard-coding the RINO group to "APRS".  Any RINO
// waypoints that begin with these four characters will have those
// four characters chopped and will be turned into our own APRS
// object, which we will then attempt to transmit.
    
            if (strncmp(name,"APRS",4) == 0) {
                // We have a match.  Turn this into an APRS Object
                char line2[100];
                int lat_deg, lon_deg;
                float lat_min, lon_min;
                char lat_dir, lon_dir;
                char temp2[50];
                int date;
                int hour;
                int minute;
// Three variables used for Base-91 compressed strings.  We have a
// bug in this code at the moment so the Base-91 compressed code in
// this routine is commented out.
//                char *compressed_string;
//                char lat_s[50];
//                char lon_s[50];


                // Strip off the "APRS" at the beginning of the
                // name.  Add spaces to flush out the length of an
                // APRS object name.
                xastir_snprintf(temp2,
                    sizeof(temp2),
                    "%s         ",
                    &name[4]);

                // Copy it back to the "name" variable.
                xastir_snprintf(name,
                    sizeof(name),
                    "%s",
                    temp2);

                // Truncate the name at nine characters.
                name[9] = '\0';

                // We can either snag the UTC Offset from the top of
                // the file, or we can put the date/time format into
                // local time.  The spec suggests using zulu time
                // for all future implementations, so we snagged the
                // UTC Offset earlier in this routine.

                // 07/09/2004 09:22:28
//fprintf(stderr,"%s %s", name, datetime);

                xastir_snprintf(temp2,
                    sizeof(temp2),
                    "%s",
                    &datetime[3]);
                temp2[2] = '\0';
                date = atoi(temp2);
//fprintf(stderr, "%02d\n", date);

                xastir_snprintf(temp2,
                    sizeof(temp2),
                    "%s",
                    &datetime[11]);
                temp2[2] = '\0';
                hour = atoi(temp2);

                xastir_snprintf(temp2,
                    sizeof(temp2),
                    "%s",
                    &datetime[14]);
                temp2[2] = '\0';
                minute = atoi(temp2);
//fprintf(stderr,"\t\t%02d%02d%02d/\n", date, hour, minute);

                // We need to remember to bump the date up if we go
                // past midnight adding the UTC offset.  In that
                // case we may need to bump the day as well if we're
                // near the end of the month.  Use the Unix time
                // facilities for this?

                // Here we're assuming that the UTC offset is
                // divisible by one hour.  Always correct?

//                hour = (int)(hour - UTC_Offset);


                lat_deg = atoi(lat_c);
                if (lat_deg < 0)
                    lat_deg = -lat_deg;

                lon_deg = atoi(lon_c);
                if (lon_deg < 0)
                    lon_deg = -lon_deg;

                lat_min = atof(lat_c);
                if (lat_min < 0.0)
                    lat_min = -lat_min;
                lat_min = (lat_min - lat_deg) * 60.0;

                lon_min = atof(lon_c);
                if (lon_min < 0.0)
                    lon_min = -lon_min;
                lon_min = (lon_min - lon_deg) * 60.0;

                if (lat_c[0] == '-')
                    lat_dir = 'S';
                else
                    lat_dir = 'N';

                if (lon_c[0] == '-')
                    lon_dir = 'W';
                else
                    lon_dir = 'E';

                // Non-Compressed version
                xastir_snprintf(line2,
                    sizeof(line2),
                    ";%-9s*%02d%02d%02d/%02d%05.2f%c%c%03d%05.2f%c%c",
                    name,
                    date,
                    hour,
                    minute,
                    lat_deg,    // Degrees
                    lat_min,    // Minutes
                    lat_dir,    // N/S
                    '/',        // Primary symbol table
                    lon_deg,    // Degrees
                    lon_min,    // Minutes
                    lon_dir,    // E/W
                    '[');       // Hiker symbol

/*
                // Compressed version.  Gives us more of the
                // resolution inherent in the RINO waypoints.
                // Doesn't have an affect on whether we transmit
                // compressed objects from Xastir over RF.  That is
                // selected from the File->Configure->Defaults
                // dialog.
                //
                // compress_posit expects its lat/long in //
                // APRS-like format:
                // "%2d%lf%c", &deg, &minutes, &ext

                xastir_snprintf(lat_s,
                    sizeof(lat_s),
                    "%02d%8.5f%c",
                    lat_deg,
                    lat_min,
                    lat_dir);
 
                xastir_snprintf(lon_s,
                    sizeof(lon_s),
                    "%02d%8.5f%c",
                    lon_deg,
                    lon_min,
                    lon_dir);
                    
                compressed_string = compress_posit(lat_s,
                    '/',    // group character
                    lon_s,
                    '[',    // symbol,
                    0,      // course,
                    0,      // speed,
                    "");    // phg

//fprintf(stderr, "compressed: %s\n", compressed_string);

                xastir_snprintf(line2,
                    sizeof(line2),
                    ";%-9s*%02d%02d%02d/%s",
                    name,
                    date,
                    hour,
                    minute,
                    compressed_string);
*/

/*
                fprintf(stderr,
                    "%-9s\t%f\t%f\t\t\t\t\t\t",
                    name,
                    atof(lat_c),
                    atof(lon_c));
                fprintf(stderr,"%s\n",line2);
*/

                // Update this object in our save file
                log_object_item(line2,0,name);

                if (object_tx_disable)
                    output_my_data(line2,-1,0,1,0,NULL);    // Local loopback only, not igating
                else
                    output_my_data(line2,-1,0,0,0,NULL);    // Transmit/loopback object data, not igating
            }
        }
    }

//fprintf(stderr,"\n");

    (void)fclose(f);
}




 
void GPS_operations_destroy_shell( /*@unused@*/ Widget widget, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    Widget shell = (Widget) clientData;
    XtPopdown(shell);
    XtDestroyWidget(shell);
    GPS_operations_dialog = (Widget)NULL;
}





// Set up gps_map_filename based on user preferences for filename
// and map color.
void GPS_operations_change_data(Widget widget, XtPointer clientData, XtPointer callData) {
    char *temp;
    char short_filename[MAX_FILENAME];
    char color_text[50];


    temp = XmTextGetString(gpsfilename_text);
    xastir_snprintf(short_filename,
        sizeof(short_filename),
        "%s",
        temp);
    XtFree(temp);

    // Add date/time to filename if no filename is given
    if (strlen(short_filename) == 0) {
        int ii;

        // Compute the date/time and put in string
        get_timestamp(short_filename);

        // Change spaces to underlines
        // Wed Mar  5 15:24:48 PST 2003
        for (ii = 0; ii < (int)strlen(short_filename); ii++) {
            if (short_filename[ii] == ' ')
                short_filename[ii] = '_';
        }
    }
    
    (void)remove_trailing_spaces(short_filename);

    switch (gps_map_color) {
        case 0: {
            xastir_snprintf(color_text,sizeof(color_text),"Red");
            gps_map_color_offset=0x0c;
            break;
        }
        case 1: {
            xastir_snprintf(color_text,sizeof(color_text),"Green");
            gps_map_color_offset=0x23;
            break;
        }
        case 2: {
            xastir_snprintf(color_text,sizeof(color_text),"Black");
            gps_map_color_offset=0x08;
            break;
        }
        case 3: {
            xastir_snprintf(color_text,sizeof(color_text),"White");
            gps_map_color_offset=0x0f;
            break;
        }
        case 4: {
            xastir_snprintf(color_text,sizeof(color_text),"Orange");
            gps_map_color_offset=0x62;
            break;
        }
        case 5: {
            xastir_snprintf(color_text,sizeof(color_text),"Blue");
            gps_map_color_offset=0x03;
            break;
        }
        case 6: {
            xastir_snprintf(color_text,sizeof(color_text),"Yellow");
            gps_map_color_offset=0x0e;
            break;
        }
        case 7: {
            xastir_snprintf(color_text,sizeof(color_text),"Purple");
            gps_map_color_offset=0x0b;
            break;
        }
        default: {
            xastir_snprintf(color_text,sizeof(color_text),"Red");
            gps_map_color_offset=0x0c;
            break;
        }
    }

    // If doing waypoints, don't add the color onto the end
    if (strcmp("Waypoints",gps_map_type) == 0) {
        xastir_snprintf(gps_map_filename,
            sizeof(gps_map_filename),
            "%s_%s.shp",
            short_filename,
            gps_map_type);

        // Same without ".shp"
        xastir_snprintf(gps_map_filename_base,
            sizeof(gps_map_filename_base),
            "%s_%s",
            short_filename,
            gps_map_type);
    }
    else {  // Doing Tracks/Routes
        xastir_snprintf(gps_map_filename,
            sizeof(gps_map_filename),
            "%s_%s_%s.shp",
            short_filename,
            gps_map_type,
            color_text);

        // Same without ".shp"
        xastir_snprintf(gps_map_filename_base,
            sizeof(gps_map_filename_base),
            "%s_%s_%s",
            short_filename,
            gps_map_type,
            color_text);

        // Same without ".shp" *or* color
        xastir_snprintf(gps_map_filename_base2,
            sizeof(gps_map_filename_base2),
            "%s_%s",
            short_filename,
            gps_map_type);
    }

//fprintf(stderr,"%s\t%s\n",gps_map_filename,gps_map_filename_base);

    // Signify that the user has selected the filename and color for
    // the downloaded file.
    gps_details_selected++;

    GPS_operations_destroy_shell(widget,clientData,callData);
}





void GPS_operations_cancel(Widget widget, XtPointer clientData, XtPointer callData) {

    // Destroy the GPS selection dialog
    GPS_operations_destroy_shell(widget,clientData,callData);

    // Wait for the GPS operation to be finished, then clear out all
    // of the variables.
    while (!gps_got_data_from && gps_operation_pending) {
        usleep(1000000);    // 1 second
    }

    gps_details_selected = 0;
    gps_got_data_from = 0;
    gps_operation_pending = 0;
}





void  GPS_operations_color_toggle( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        gps_map_color = atoi(which);
    else
        gps_map_color = 0;
}





// This routine should be called while the transfer is progressing,
// or perhaps just after it ends.  If we can do it while the
// transfer is ocurring we can save time overall.  Here we'll select
// the color and name for the resulting file, then cause it to be
// selected and displayed on the map screen.
//
void GPS_transfer_select( void ) {
    static Widget pane, my_form, button_select, button_cancel,
                  frame,  color_type, type_box, ctyp0, ctyp1,
                  ctyp2, ctyp3, ctyp4, ctyp5, ctyp6, ctyp7,
                  gpsfilename_label;
    Atom delw;
    Arg al[50];                      // Arg List
    register unsigned int ac = 0;   // Arg Count


    if (!GPS_operations_dialog) {

        // Set args for color
        ac = 0;
        XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
        XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
        XtSetArg(al[ac], XmNfontList, fontlist1); ac++;

        GPS_operations_dialog = XtVaCreatePopupShell(
                langcode("GPS001"),
                xmDialogShellWidgetClass, appshell,
                XmNdeleteResponse, XmDESTROY,
                XmNdefaultPosition, FALSE,
                XmNfontList, fontlist1,
                NULL);

        pane = XtVaCreateWidget(
                "GPS_transfer_select pane",
                xmPanedWindowWidgetClass, 
                GPS_operations_dialog,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        my_form =  XtVaCreateWidget(
                "GPS_transfer_select my_form",
                xmFormWidgetClass, 
                pane,
                XmNfractionBase, 5,
                XmNautoUnmanage, FALSE,
                XmNshadowThickness, 1,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);


        gpsfilename_label = XtVaCreateManagedWidget(    // Filename
                langcode("GPS002"),
                xmLabelWidgetClass,
                my_form,
                XmNchildType, XmFRAME_TITLE_CHILD,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        gpsfilename_text = XtVaCreateManagedWidget(
                "GPS_transfer_select gpsfilename_text", 
                xmTextWidgetClass, 
                my_form,
                XmNeditable,   TRUE,
                XmNcursorPositionVisible, TRUE,
                XmNsensitive, TRUE,
                XmNshadowThickness,    1,
                XmNcolumns, 20,
                XmNwidth, ((20*7)+2),
                XmNmaxLength, 200,
                XmNbackground, colors[0x0f],
                XmNtopAttachment,XmATTACH_FORM,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, gpsfilename_label,
                XmNleftOffset, 10,
                XmNrightAttachment,XmATTACH_NONE,
                XmNrightOffset, 10,
                XmNnavigationType, XmTAB_GROUP,
                XmNfontList, fontlist1,
                NULL);

        frame = XtVaCreateManagedWidget(
                "GPS_transfer_select frame", 
                xmFrameWidgetClass, 
                my_form,
                XmNtopAttachment,XmATTACH_WIDGET,
                XmNtopWidget, gpsfilename_label,
                XmNtopOffset,15,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 10,
                XmNrightAttachment,XmATTACH_FORM,
                XmNrightOffset, 10,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        color_type  = XtVaCreateManagedWidget(  // Select Color
                langcode("GPS003"),
                xmLabelWidgetClass,
                frame,
                XmNchildType, XmFRAME_TITLE_CHILD,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        type_box = XmCreateRadioBox(
                frame,
                "GPS_transfer_select Transmit Options box",
                al,
                ac);

        XtVaSetValues(type_box,
                XmNnumColumns,2,
                NULL);

        ctyp0 = XtVaCreateManagedWidget(    // Red
                langcode("GPS004"),
                xmToggleButtonGadgetClass,
                type_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(ctyp0,XmNvalueChangedCallback,GPS_operations_color_toggle,"0");

        ctyp1 = XtVaCreateManagedWidget(    // Green
                langcode("GPS005"),
                xmToggleButtonGadgetClass,
                type_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(ctyp1,XmNvalueChangedCallback,GPS_operations_color_toggle,"1");

        ctyp2 = XtVaCreateManagedWidget(    // Black
                langcode("GPS006"),
                xmToggleButtonGadgetClass,
                type_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(ctyp2,XmNvalueChangedCallback,GPS_operations_color_toggle,"2");

        ctyp3 = XtVaCreateManagedWidget(    // White
                langcode("GPS007"),
                xmToggleButtonGadgetClass,
                type_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(ctyp3,XmNvalueChangedCallback,GPS_operations_color_toggle,"3");

        ctyp4 = XtVaCreateManagedWidget(    // Orange
                langcode("GPS008"),
                xmToggleButtonGadgetClass,
                type_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(ctyp4,XmNvalueChangedCallback,GPS_operations_color_toggle,"4");

        ctyp5 = XtVaCreateManagedWidget(    // Blue
                langcode("GPS009"),
                xmToggleButtonGadgetClass,
                type_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(ctyp5,XmNvalueChangedCallback,GPS_operations_color_toggle,"5");

        ctyp6 = XtVaCreateManagedWidget(    // Yellow
                langcode("GPS010"),
                xmToggleButtonGadgetClass,
                type_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(ctyp6,XmNvalueChangedCallback,GPS_operations_color_toggle,"6");

        ctyp7 = XtVaCreateManagedWidget(    // Violet
                langcode("GPS011"),
                xmToggleButtonGadgetClass,
                type_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(ctyp7,XmNvalueChangedCallback,GPS_operations_color_toggle,"7");


        button_select = XtVaCreateManagedWidget(
                langcode("WPUPCFS028"),
                xmPushButtonGadgetClass, 
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, frame,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_FORM,
                XmNbottomOffset, 5,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 1,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 2,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);


        button_cancel = XtVaCreateManagedWidget(
                langcode("UNIOP00002"),
                xmPushButtonGadgetClass, 
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, frame,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_FORM,
                XmNbottomOffset, 5,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 3,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 4,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        XtAddCallback(button_select,
            XmNactivateCallback,
            GPS_operations_change_data,
            GPS_operations_dialog);

        XtAddCallback(button_cancel,
            XmNactivateCallback,
            GPS_operations_cancel,
            GPS_operations_dialog);

        // Set default color to first selection
        XmToggleButtonSetState(ctyp0,TRUE,FALSE);
        gps_map_color = 0;

        // De-sensitize the color selections if we're doing
        // waypoints.
        if (strcmp("Waypoints",gps_map_type) == 0) {
            XtSetSensitive(frame, FALSE);
        }

        pos_dialog(GPS_operations_dialog);

        delw = XmInternAtom(
            XtDisplay(GPS_operations_dialog),
            "WM_DELETE_WINDOW",
            FALSE);

        XmAddWMProtocolCallback(
            GPS_operations_dialog,
            delw,
            GPS_operations_destroy_shell,
            (XtPointer)GPS_operations_dialog);

        XtManageChild(my_form);
        XtManageChild(type_box);
        XtManageChild(pane);

        XtPopup(GPS_operations_dialog,XtGrabNone);
        fix_dialog_size(GPS_operations_dialog);

        // Move focus to the Select button.  This appears to highlight the
        // button fine, but we're not able to hit the <Enter> key to
        // have that default function happen.  Note:  We _can_ hit the
        // <SPACE> key, and that activates the option.
//        XmUpdateDisplay(GPS_operations_dialog);
        XmProcessTraversal(button_select, XmTRAVERSE_CURRENT);

    } else {
        (void)XRaiseWindow(
            XtDisplay(GPS_operations_dialog),
            XtWindow(GPS_operations_dialog));
    }
}




time_t check_gps_map_time = (time_t)0;


// Function called by UpdateTime periodically.  Checks whether
// we've just completed a GPS transfer and need to redraw maps as a
// result.
//
void check_for_new_gps_map(int curr_sec) {

    // Only check once per second
    if (check_gps_map_time == curr_sec) {
        return; // Skip it, we already checked once this second.
    }
    check_gps_map_time = curr_sec;


    if ( (gps_operation_pending || gps_got_data_from)
            && !gps_details_selected) {

        // A transfer is underway or has just completed.  The user
        // hasn't selected the filename/color yet.  Bring up the
        // selection dialog so that the user can do so.
        GPS_transfer_select();
    }
    else if (gps_details_selected
            && gps_got_data_from
            && !gps_operation_pending) {
        FILE *f;
        char temp[MAX_FILENAME * 2];


//fprintf(stderr,"check_for_new_gps_map()\n");


        // We have new data from a GPS!  Add the file to the
        // selected maps file, cause a reload of the maps to occur,
        // and then re-index maps (so that map may be deselected by
        // the user).

//
// It would be good to verify that we're not duplicating entries.
// Add code here to read through the file first looking for a
// match before attempting to append any new lines.
//

        // We don't rename if Garmin RINO waypoint file
        if (strcmp(gps_map_type,"RINO") != 0) {
            // Rename the temporary file to the new filename.  We must
            // do this three times, once for each piece of the Shapefile
            // map.
#if defined(HAVE_MV)
            xastir_snprintf(temp,
                sizeof(temp),
                "%s %s/%s %s/%s",
                MV_PATH,
                get_user_base_dir("gps"),
                gps_temp_map_filename,
                get_user_base_dir("gps"),
                gps_map_filename);

            if ( system(temp) ) {
                fprintf(stderr,"Couldn't rename the downloaded GPS map file\n");
                fprintf(stderr,"%s\n",temp);
                gps_got_data_from = 0;
                gps_details_selected = 0;
                return;
            }
            // Done for the ".shp" file.  Now repeat for the ".shx" and
            // ".dbf" files.
            xastir_snprintf(temp,
                sizeof(temp),
                "%s %s/%s.shx %s/%s.shx",
                MV_PATH,
                get_user_base_dir("gps"),
                gps_temp_map_filename_base,
                get_user_base_dir("gps"),
                gps_map_filename_base);

            if ( system(temp) ) {
                fprintf(stderr,"Couldn't rename the downloaded GPS map file\n");
                fprintf(stderr,"%s\n",temp);
                gps_got_data_from = 0;
                gps_details_selected = 0;
                return;
            }
            xastir_snprintf(temp,
                sizeof(temp),
                "%s %s/%s.dbf %s/%s.dbf",
                MV_PATH,
                get_user_base_dir("gps"),
                gps_temp_map_filename_base,
                get_user_base_dir("gps"),
                gps_map_filename_base);

            if ( system(temp) ) {
                fprintf(stderr,"Couldn't rename the downloaded GPS map file\n");
                fprintf(stderr,"%s\n",temp);
                gps_got_data_from = 0;
                gps_details_selected = 0;
                return;
            }
#endif  // HAVE_MV


            // Write out a WKT in a .prj file to go with this shapefile.
            xastir_snprintf(temp,
                            sizeof(temp),
                            "%s/%s.prj",
                            get_user_base_dir("gps"),
                            gps_map_filename_base);
            xastirWriteWKT(temp);

            if (strcmp(gps_map_type,"Waypoints") != 0) {
                // KM5VY: Create a really, really simple dbfawk file to
                // go with the shapefile.  This is a dbfawk file of the
                // "per file" type, with the color hardcoded into it.
                // This will enable downloaded gps shapefiles to have
                // the right color even when they're not placed in the
                // GPS directory.
                // We don't do this for waypoint files, because all we need to
                // do for those is associate the name with the waypoint, and 
                // that can be done by a generic signature-based file.
                xastir_snprintf(temp,
                                sizeof(temp),
                                "%s/%s.dbfawk",
                                get_user_base_dir("gps"),
                                gps_map_filename_base);
                f=fopen(temp,"w"); // open for write
                if (f != NULL) {
                    fprintf(f,gps_dbfawk_format,gps_map_color_offset,
                            gps_map_filename_base2);
                    fclose(f);
                }
            }
            // Add the new gps map to the list of selected maps
            f=fopen( get_user_base_dir(SELECTED_MAP_DATA), "a" ); // Open for appending
            if (f!=NULL) {

//WE7U:  Change this:
                fprintf(f,"GPS/%s\n",gps_map_filename);

                (void)fclose(f);

                // Reindex maps.  Use the smart timestamp-checking indexing.
                map_indexer(0);     // Have to have the new map in the index first before we can select it
                map_chooser_init(); // Re-read the selected_maps.sys file
                re_sort_maps = 1;   // Creates a new sorted list from the selected maps

//
// We should set some flags here instead of doing the map redraw
// ourselves, so that multiple map reloads don't occur sometimes in
// UpdateTime.
//

                // Reload maps
                // Set interrupt_drawing_now because conditions have changed.
                interrupt_drawing_now++;

                // Request that a new image be created.  Calls create_image,
                // XCopyArea, and display_zoom_status.
                request_new_image++;

//                if (create_image(da)) {
//                    (void)XCopyArea(XtDisplay(da),pixmap_final,XtWindow(da),gc,0,0,(unsigned int)screen_width,(unsigned int)screen_height,0,0);
//                }
            }
            else {
                fprintf(stderr,"Couldn't open file: %s\n", get_user_base_dir(SELECTED_MAP_DATA) );
            }
        }
        else {
            // We're dealing with Garmin RINO waypoints.  Go process
            // the RINO.gpstrans file, creating objects out of them.
            process_RINO_waypoints();
        }

        // Set up for the next GPS run
        gps_got_data_from = 0;
        gps_details_selected = 0;
    }
}





// This is the separate execution thread that transfers the data
// to/from the GPS.  The thread is started up by the
// GPS_operations() function below.
//
static void* gps_transfer_thread(void *arg) {
    int input_param;
    char temp[500];
    char prefix[100];
    char postfix[100];
 

    // Set up the prefix string.  Note that we depend on the correct
    // setup of serial ports and such in GPSMan's configs.
    xastir_snprintf(prefix, sizeof(prefix),
        "%s",
        GPSMAN_PATH);

    // Set up the postfix string.  The files will be created in the
    // "~/.xastir/gps/" directory.
    xastir_snprintf(postfix, sizeof(postfix),
        "Shapefile_2D %s/",
        get_user_base_dir("gps"));

    input_param = atoi((char *)arg);

    // The pthread_detach() call means we don't care about the
    // return code and won't use pthread_join() later.  Makes
    // threading more efficient.
    (void)pthread_detach(pthread_self());

    switch (input_param) {

        case 1: // Fetch track from GPS
            // gpsman.tcl -dev /dev/ttyS0 getwrite TR Shapefile_2D track.date

//            fprintf(stderr,"Fetch track from GPS\n");

            xastir_snprintf(gps_temp_map_filename,
                sizeof(gps_temp_map_filename),
                "Unnamed_Track_Red.shp");
            xastir_snprintf(gps_temp_map_filename_base,
                sizeof(gps_temp_map_filename_base),
                "Unnamed_Track_Red");


            xastir_snprintf(gps_map_type,
                sizeof(gps_map_type),
                "Track");
 
            xastir_snprintf(temp,
                sizeof(temp),
                "%s getwrite TR %s%s",
                prefix,
                postfix,
                gps_temp_map_filename);

            if ( system(temp) ) {
                fprintf(stderr,"Couldn't download the gps track\n");
                gps_operation_pending = 0;  // We're done
                return(NULL);
            }
            // Set the got_data flag
            gps_got_data_from++;
            break;

        case 2: // Fetch route from GPS
            // gpsman.tcl -dev /dev/ttyS0 getwrite RT Shapefile_2D routes.date

//            fprintf(stderr,"Fetch routes from GPS\n");

            xastir_snprintf(gps_temp_map_filename,
                sizeof(gps_temp_map_filename),
                "Unnamed_Routes_Red.shp");
            xastir_snprintf(gps_temp_map_filename_base,
                sizeof(gps_temp_map_filename_base),
                "Unnamed_Routes_Red");
 
            xastir_snprintf(gps_map_type,
                sizeof(gps_map_type),
                "Routes");
 
            xastir_snprintf(temp,
                sizeof(temp),
                "%s getwrite RT %s%s",
                prefix,
                postfix,
                gps_temp_map_filename);

            if ( system(temp) ) {
                fprintf(stderr,"Couldn't download the gps routes\n");
                gps_operation_pending = 0;  // We're done
                return(NULL);
            }
            // Set the got_data flag
            gps_got_data_from++;
            break;

        case 3: // Fetch waypoints from GPS
            // gpsman.tcl -dev /dev/ttyS0 getwrite WP Shapefile_2D waypoints.date
 
//            fprintf(stderr,"Fetch waypoints from GPS\n");

            xastir_snprintf(gps_temp_map_filename,
                sizeof(gps_temp_map_filename),
                "Unnamed_Waypoints.shp");
            xastir_snprintf(gps_temp_map_filename_base,
                sizeof(gps_temp_map_filename_base),
                "Unnamed_Waypoints");
 
            xastir_snprintf(gps_map_type,
                sizeof(gps_map_type),
                "Waypoints");
 
            xastir_snprintf(temp,
                sizeof(temp),
                "%s getwrite WP %s%s",
                prefix,
                postfix,
                gps_temp_map_filename);

            if ( system(temp) ) {
                fprintf(stderr,"Couldn't download the gps waypoints\n");
                gps_operation_pending = 0;  // We're done
                return(NULL);
            }
            // Set the got_data flag
            gps_got_data_from++;
            break;

        case 4: // Send track to GPS
            // gpsman.tcl -dev /dev/ttyS0 readput Shapefile_2D track.date TR 

            fprintf(stderr,"Send track to GPS\n");
            fprintf(stderr,"Not implemented yet\n");
            gps_operation_pending = 0;  // We're done
            return(NULL);
            break;

        case 5: // Send route to GPS
            // gpsman.tcl -dev /dev/ttyS0 readput Shapefile_2D routes.date RT 

            fprintf(stderr,"Send route to GPS\n");
            fprintf(stderr,"Not implemented yet\n");
            gps_operation_pending = 0;  // We're done
            return(NULL);
            break;

        case 6: // Send waypoints to GPS
            // gpsman.tcl -dev /dev/ttyS0 readput Shapefile_2D waypoints.date WP 

            fprintf(stderr,"Send waypoints to GPS\n");
            fprintf(stderr,"Not implemented yet\n");
            gps_operation_pending = 0;  // We're done
            return(NULL);
            break;

       case 7: // Fetch waypoints from GPS in GPSTrans format
            // gpsman.tcl -dev /dev/ttyS0 getwrite WP GPStrans waypoints.date

//            fprintf(stderr,"Fetch Garmin RINO waypoints\n");

            // The files will be created in the "~/.xastir/gps/"
            // directory.

            xastir_snprintf(gps_temp_map_filename,
                sizeof(gps_temp_map_filename),
                "RINO.gpstrans");
 
            xastir_snprintf(gps_temp_map_filename_base,
                sizeof(gps_temp_map_filename_base),
                "RINO");
 
            xastir_snprintf(gps_map_type,
                sizeof(gps_map_type),
                "RINO");
 
            xastir_snprintf(temp,
                sizeof(temp),
                "(%s getwrite WP GPStrans %s/%s 2>&1) >/dev/null",
                prefix,
                get_user_base_dir("gps"),
                gps_temp_map_filename);

// Execute the command
system(temp);
//            if ( system(temp) ) {
//                fprintf(stderr,"Couldn't download the gps waypoints\n");
//                gps_operation_pending = 0;  // We're done
//                return(NULL);
//            }
            // Set the got_data flag
            gps_got_data_from++;
            break;

        default:
            fprintf(stderr,"Illegal parameter %d passed to gps_transfer_thread!\n",
                input_param);
            gps_operation_pending = 0;  // We're done
            break;
    }   // End of switch


    // Signal to the main thread that we're all done with the
    // GPS operation.
    gps_operation_pending = 0;  // We're done

    // End the thread
    return(NULL);
}





// GPSMan can't handle multiple '.'s in the filename.  It chops at
// the first one.
//
// Note that the permissions on the "~/.xastir/gps/" directory have to be
// set so that this user (or the root user?) can create files in
// that directory.  The permissions on the resulting files may need
// to be tweaked.
//
// When creating files, we should warn the user of a conflict if the
// filename already exists, then if the user wishes to overwrite it,
// delete the old set of files before downloading the new ones.  We
// should also make sure we're not adding the filename to the
// selected_maps.sys more than once.
//
// Set up default filenames for each, with an autoincrementing
// number at the end?  That'd be one way of getting a maps
// downloaded in a hurry.  Could also ask for a filename after the
// download is complete instead of at the start of the download.  In
// that case, download to a temporary filename and then rename it
// later and reload maps.
//
// Dialog should ask the user to put the unit into Garmin-Garmin
// mode before proceeding.
//
// We could de-sensitize the GPS transfer menu items during a
// transfer operation, to make sure we're not called again until the
// first operation is over.
//
void GPS_operations( /*@unused@*/ Widget w, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    pthread_t gps_operations_thread;
    int parameter;


    if (clientData != NULL) {

        // Check whether we're already doing a GPS operation.
        // Return if so.
        if (gps_operation_pending) {
            fprintf(stderr,"GPS Operation is already pending, can't start another one!\n");
            return;
        }

        parameter = atoi((char *)clientData);

        if ( ((parameter < 1) || (parameter > 3)) && parameter != 7) {
            fprintf(stderr,"GPS_operations: Parameter out-of-range: %d",parameter);
            return;
        }


        gps_operation_pending++;
        gps_got_data_from = 0;

        // We don't need to select filename/color if option 7
        if (parameter == 7) {
            gps_details_selected++;
        }


//----- Start New Thread -----
 
        // Here we start a new thread.  We'll communicate with the
        // main thread via global variables.  Use mutex locks if
        // there might be a conflict as to when/how we're updating
        // those variables.
        //
        if (pthread_create(&gps_operations_thread, NULL, gps_transfer_thread, clientData)) {
            fprintf(stderr,"Error creating gps transfer thread\n");
            gps_got_data_from = 0;      // No data to present
            gps_operation_pending = 0;  // We're done
        }
        else {
            // We're off and running with the new thread!
        }
    }
}
#endif  // HAVE_GPSMAN





/////////////////////////////////////////////////////////////////////
// End of GPS operations
/////////////////////////////////////////////////////////////////////





void Set_Log_Indicator(void) {
    if (       (1==log_tnc_data)
            || (1==log_net_data)
            || (1==log_wx)
            || (1==log_igate)
            || (1==log_wx_alert_data)
            || (1==log_message_data) ) {
        XmTextFieldSetString(log_indicator, langcode("BBARSTA043")); // Logging
        XtVaSetValues(log_indicator,
            XmNbackground, GetPixelByName(appshell,"RosyBrown"),
	    NULL);
    } else {
        XmTextFieldSetString(log_indicator, NULL);
        XtVaSetValues(log_indicator, MY_BACKGROUND_COLOR, NULL);
    }
}





void  TNC_Logging_toggle( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set) 
        log_tnc_data = atoi(which);
    else 
        log_tnc_data = 0;
    Set_Log_Indicator();
}





void Net_Logging_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        log_net_data = atoi(which);
    else
        log_net_data = 0;
    Set_Log_Indicator();
}





void IGate_Logging_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        log_igate = atoi(which);
    else
        log_igate = 0;
    Set_Log_Indicator();
}





void Message_Logging_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        log_message_data = atoi(which);
    else
        log_message_data = 0;
    Set_Log_Indicator();
}





void WX_Logging_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        log_wx = atoi(which);
    else
        log_wx = 0;
    Set_Log_Indicator();
}





void WX_Alert_Logging_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        log_wx_alert_data = atoi(which);
    else
        log_wx_alert_data = 0;
    Set_Log_Indicator();
}





// Filter Data Menu button callbacks

// support functions
void set_sensitive_select_sources(int sensitive)
{
    XtSetSensitive(select_mine_button,     sensitive);
    XtSetSensitive(select_tnc_button,      sensitive);
    if (!Select_.tnc) {
        XtSetSensitive(select_direct_button,   FALSE);
        XtSetSensitive(select_via_digi_button, FALSE);
    }
    else {
        XtSetSensitive(select_direct_button,   sensitive);
        XtSetSensitive(select_via_digi_button, sensitive);
    }
    XtSetSensitive(select_net_button,      sensitive);
    if (no_data_selected())
        XtSetSensitive(select_old_data_button, FALSE);
    else
        XtSetSensitive(select_old_data_button, sensitive);
}

void set_sensitive_select_types(int sensitive)
{
    XtSetSensitive(select_stations_button, sensitive);
    if (!Select_.stations) {
        XtSetSensitive(select_fixed_stations_button, FALSE);
        XtSetSensitive(select_moving_stations_button,     FALSE);
        XtSetSensitive(select_weather_stations_button,    FALSE);
        XtSetSensitive(select_CWOP_wx_stations_button,       FALSE);
    }
    else {
        XtSetSensitive(select_fixed_stations_button, sensitive);
        XtSetSensitive(select_moving_stations_button,     sensitive);
        XtSetSensitive(select_weather_stations_button,    sensitive);
        if (Select_.weather_stations) {
            XtSetSensitive(select_CWOP_wx_stations_button, sensitive);
        }
    }

    XtSetSensitive(select_objects_button, sensitive);

    if (!Select_.objects) {
        XtSetSensitive(select_weather_objects_button, FALSE);
        XtSetSensitive(select_gauge_objects_button,   FALSE);
        XtSetSensitive(select_other_objects_button,   FALSE);
    }
    else {
        XtSetSensitive(select_weather_objects_button, sensitive);
        XtSetSensitive(select_gauge_objects_button,   sensitive);
        XtSetSensitive(select_other_objects_button,   sensitive);
    }
}





void set_sensitive_display(int sensitive)
{
    XtSetSensitive(display_callsign_button,      sensitive);

    if (!Display_.callsign) {
        XtSetSensitive(display_label_all_trackpoints_button, FALSE);
    }
    else {
        XtSetSensitive(display_label_all_trackpoints_button, sensitive);
    }
 
    XtSetSensitive(display_symbol_button,        sensitive);

    if (!Display_.symbol) {
        XtSetSensitive(display_symbol_rotate_button, FALSE);
    }
    else {
        XtSetSensitive(display_symbol_rotate_button, sensitive);
    }

    XtSetSensitive(display_phg_button,           sensitive);

    if (!Display_.phg) {
        XtSetSensitive(display_default_phg_button,   FALSE);
        XtSetSensitive(display_phg_of_moving_button, FALSE);
    }
    else {
        XtSetSensitive(display_default_phg_button,   sensitive);
        XtSetSensitive(display_phg_of_moving_button, sensitive);
    }
    XtSetSensitive(display_altitude_button, sensitive);
    XtSetSensitive(display_course_button,   sensitive);
    XtSetSensitive(display_speed_button,    sensitive);
    if (!Display_.speed) {
        XtSetSensitive(display_speed_short_button, FALSE);
    }
    else {
        XtSetSensitive(display_speed_short_button, sensitive);
    }
    XtSetSensitive(display_dist_bearing_button, sensitive);
    XtSetSensitive(display_weather_button,      sensitive);
    if (!Display_.weather) {
        XtSetSensitive(display_weather_text_button,     FALSE);
        XtSetSensitive(display_temperature_only_button, FALSE);
        XtSetSensitive(display_wind_barb_button,        FALSE);
    }
    else {
        XtSetSensitive(display_weather_text_button, sensitive);
        if (!Display_.weather_text) {
            XtSetSensitive(display_temperature_only_button, FALSE);
        }
        else {
            XtSetSensitive(display_temperature_only_button, sensitive);
        }
        XtSetSensitive(display_wind_barb_button, sensitive);
    }
    XtSetSensitive(display_trail_button,      sensitive);
    XtSetSensitive(display_last_heard_button, sensitive);
    XtSetSensitive(display_ambiguity_button,  sensitive);
    XtSetSensitive(display_df_data_button,    sensitive);
    XtSetSensitive(display_dr_data_button,    sensitive);
    if (!Display_.dr_data) {
        XtSetSensitive(display_dr_arc_button,    FALSE);
        XtSetSensitive(display_dr_course_button, FALSE);
        XtSetSensitive(display_dr_symbol_button, FALSE);
    }
    else {
        XtSetSensitive(display_dr_arc_button,    sensitive);
        XtSetSensitive(display_dr_course_button, sensitive);
        XtSetSensitive(display_dr_symbol_button, sensitive);
    }
}





void Select_none_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if (state->set) {
        Select_.none = atoi(which);
        set_sensitive_select_sources(FALSE);
        set_sensitive_select_types(FALSE);
        set_sensitive_display(FALSE);
    }
    else {
        Select_.none = 0;
        set_sensitive_select_sources(TRUE);
        if (no_data_selected()) {
            set_sensitive_select_types(FALSE);
            set_sensitive_display(FALSE);
        }
        else {
            set_sensitive_select_types(TRUE);
            set_sensitive_display(TRUE);
        }
    }

    redraw_on_new_data = 2;     // Immediate screen update
}






void Select_mine_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if (state->set) {
        Select_.mine = atoi(which);
        XtSetSensitive(select_old_data_button, TRUE);
        set_sensitive_select_types(TRUE);
        set_sensitive_display(TRUE);
    }
    else {
        Select_.mine = 0;
        if (no_data_selected()) {
            XtSetSensitive(select_old_data_button, FALSE);
            set_sensitive_select_types(FALSE);
            set_sensitive_display(FALSE);
        }
    }

    redraw_on_new_data = 2;     // Immediate screen update
}





void Select_tnc_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if (state->set) {
        Select_.tnc = atoi(which);
        XtSetSensitive(select_direct_button,   TRUE);
        XtSetSensitive(select_via_digi_button, TRUE);

        if (!no_data_selected()) {
            XtSetSensitive(select_old_data_button, TRUE);
            set_sensitive_select_types(TRUE);
            set_sensitive_display(TRUE);
        }
    }
    else {
        Select_.tnc = 0;
        XtSetSensitive(select_direct_button,   FALSE);
        XtSetSensitive(select_via_digi_button, FALSE);
        if (no_data_selected()) {
            XtSetSensitive(select_old_data_button, FALSE);
            set_sensitive_select_types(FALSE);
            set_sensitive_display(FALSE);
        }
    }

    redraw_on_new_data = 2;     // Immediate screen update
}





void Select_direct_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if (state->set) {
        Select_.direct = atoi(which);
        XtSetSensitive(select_old_data_button, TRUE);
        set_sensitive_select_types(TRUE);
        set_sensitive_display(TRUE);
    }
    else {
        Select_.direct = 0;
        if (no_data_selected()) {
            XtSetSensitive(select_old_data_button, FALSE);
            set_sensitive_select_types(FALSE);
            set_sensitive_display(FALSE);
        }
    }

    redraw_on_new_data = 2;     // Immediate screen update
}





void Select_via_digi_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if (state->set) {
        Select_.via_digi = atoi(which);
        XtSetSensitive(select_old_data_button, TRUE);
        set_sensitive_select_types(TRUE);
        set_sensitive_display(TRUE);
    }
    else {
        Select_.via_digi = 0;
        if (no_data_selected()) {
            XtSetSensitive(select_old_data_button, FALSE);
            set_sensitive_select_types(FALSE);
            set_sensitive_display(FALSE);
        }
    }

    redraw_on_new_data = 2;     // Immediate screen update
}





void Select_net_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if (state->set) {
        Select_.net = atoi(which);
        XtSetSensitive(select_old_data_button, TRUE);
        set_sensitive_select_types(TRUE);
        set_sensitive_display(TRUE);
    }
    else {
        Select_.net = 0;
        if (no_data_selected()) {
            XtSetSensitive(select_old_data_button, FALSE);
            set_sensitive_select_types(FALSE);
            set_sensitive_display(FALSE);
        }
    }
    redraw_on_new_data = 2;     // Immediate screen update
}





void Select_tactical_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if (state->set) {
        Select_.tactical = atoi(which);
        XtSetSensitive(select_old_data_button, TRUE);
        set_sensitive_select_types(TRUE);
        set_sensitive_display(TRUE);
    }
    else {
        Select_.tactical = 0;
        if (no_data_selected()) {
            XtSetSensitive(select_old_data_button, FALSE);
            set_sensitive_select_types(FALSE);
            set_sensitive_display(FALSE);
        }
    }
    redraw_on_new_data = 2;     // Immediate screen update
}





void Select_old_data_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if (state->set)
        Select_.old_data = atoi(which);
    else
        Select_.old_data = 0;

    redraw_on_new_data = 2;     // Immediate screen update
}





void Select_stations_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if (state->set) {
        Select_.stations = atoi(which);
        XtSetSensitive(select_fixed_stations_button,   TRUE);
        XtSetSensitive(select_moving_stations_button,  TRUE);
        XtSetSensitive(select_weather_stations_button, TRUE);
        if (Select_.weather_stations) {
            XtSetSensitive(select_CWOP_wx_stations_button, TRUE);
        }
    }
    else {
        Select_.stations = 0;
        XtSetSensitive(select_fixed_stations_button,   FALSE);
        XtSetSensitive(select_moving_stations_button,  FALSE);
        XtSetSensitive(select_weather_stations_button, FALSE);
        XtSetSensitive(select_CWOP_wx_stations_button, FALSE);
    }

    redraw_on_new_data = 2;     // Immediate screen update
}





void Select_fixed_stations_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if (state->set)
        Select_.fixed_stations = atoi(which);
    else
        Select_.fixed_stations = 0;

    redraw_on_new_data = 2;     // Immediate screen update
}





void Select_moving_stations_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if (state->set)
        Select_.moving_stations = atoi(which);
    else
        Select_.moving_stations = 0;

    redraw_on_new_data = 2;     // Immediate screen update
}





void Select_weather_stations_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if (state->set) {
        Select_.weather_stations = atoi(which);
        XtSetSensitive(select_CWOP_wx_stations_button, atoi(which));
    }
    else {
        Select_.weather_stations = 0;
        XtSetSensitive(select_CWOP_wx_stations_button, FALSE);
    }

    redraw_on_new_data = 2;     // Immediate screen update
}





void Select_CWOP_wx_stations_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if (state->set)
        Select_.CWOP_wx_stations = atoi(which);
    else
        Select_.CWOP_wx_stations = 0;

    redraw_on_new_data = 2;     // Immediate screen update
}





void Select_objects_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if (state->set) {
        Select_.objects = atoi(which);
        XtSetSensitive(select_weather_objects_button, TRUE);
        XtSetSensitive(select_gauge_objects_button,   TRUE);
        XtSetSensitive(select_other_objects_button,   TRUE);
    }
    else {
        Select_.objects = 0;
        XtSetSensitive(select_weather_objects_button, FALSE);
        XtSetSensitive(select_gauge_objects_button,   FALSE);
        XtSetSensitive(select_other_objects_button,   FALSE);
    }

    redraw_on_new_data = 2;     // Immediate screen update
}





void Select_weather_objects_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if (state->set)
        Select_.weather_objects = atoi(which);
    else
        Select_.weather_objects = 0;

    redraw_on_new_data = 2;     // Immediate screen update
}





void Select_gauge_objects_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if (state->set)
        Select_.gauge_objects = atoi(which);
    else
        Select_.gauge_objects = 0;

    redraw_on_new_data = 2;     // Immediate screen update
}





void Select_other_objects_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if (state->set)
        Select_.other_objects = atoi(which);
    else
        Select_.other_objects = 0;

    redraw_on_new_data = 2;     // Immediate screen update
}





// Display Menu button callbacks

void Display_callsign_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if (state->set) {
        Display_.callsign = atoi(which);
        XtSetSensitive(display_label_all_trackpoints_button, TRUE);
    }
    else {
        Display_.callsign = 0;
        XtSetSensitive(display_label_all_trackpoints_button, FALSE);
    }

    redraw_on_new_data = 2;     // Immediate screen update
}





void Display_label_all_trackpoints_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if (state->set)
        Display_.label_all_trackpoints = atoi(which);
    else
        Display_.label_all_trackpoints = 0;

    redraw_on_new_data = 2;     // Immediate screen update
}





void Display_symbol_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if (state->set) {
        Display_.symbol = atoi(which);
        XtSetSensitive(display_symbol_rotate_button, TRUE);
    }
    else {
        Display_.symbol = 0;
        XtSetSensitive(display_symbol_rotate_button, FALSE);
    }

    redraw_on_new_data = 2;     // Immediate screen update
}





void Display_symbol_rotate_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if (state->set)
        Display_.symbol_rotate = atoi(which);
    else
        Display_.symbol_rotate = 0;

    redraw_on_new_data = 2;     // Immediate screen update
}





void Display_trail_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if (state->set)
        Display_.trail = atoi(which);
    else
        Display_.trail = 0;

    redraw_on_new_data = 2;     // Immediate screen update
}





void Display_course_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if (state->set)
        Display_.course = atoi(which);
    else
        Display_.course = 0;

    redraw_on_new_data = 2;     // Immediate screen update
}





void Display_speed_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if (state->set) {
        Display_.speed = atoi(which);
        XtSetSensitive(display_speed_short_button, TRUE);
    }
    else {
        Display_.speed = 0;
        XtSetSensitive(display_speed_short_button, FALSE);
    }

    redraw_on_new_data = 2;     // Immediate screen update
}





void Display_speed_short_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if (state->set)
        Display_.speed_short = atoi(which);
    else
        Display_.speed_short = 0;

    redraw_on_new_data = 2;     // Immediate screen update
}





void Display_altitude_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if (state->set)
        Display_.altitude = atoi(which);
    else
        Display_.altitude = 0;

    redraw_on_new_data = 2;     // Immediate screen update
}





void Display_weather_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if (state->set) {
        Display_.weather = atoi(which);
        XtSetSensitive(display_weather_text_button,     TRUE);
        XtSetSensitive(display_temperature_only_button, TRUE);
        XtSetSensitive(display_wind_barb_button,        TRUE);
    }
    else {
        Display_.weather = 0;
        XtSetSensitive(display_weather_text_button,     FALSE);
        XtSetSensitive(display_temperature_only_button, FALSE);
        XtSetSensitive(display_wind_barb_button,        FALSE);
    }

    redraw_on_new_data = 2;     // Immediate screen update
}





void Display_weather_text_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if (state->set) {
        Display_.weather_text = atoi(which);
        XtSetSensitive(display_temperature_only_button, TRUE);
    }
    else {
        Display_.weather_text = 0;
        XtSetSensitive(display_temperature_only_button, FALSE);
    }

    redraw_on_new_data = 2;     // Immediate screen update
}





void Display_temperature_only_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if (state->set)
        Display_.temperature_only = atoi(which);
    else
        Display_.temperature_only = 0;

    redraw_on_new_data = 2;     // Immediate screen update
}





void Display_wind_barb_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if (state->set)
        Display_.wind_barb = atoi(which);
    else
        Display_.wind_barb = 0;

    redraw_on_new_data = 2;     // Immediate screen update
}





void Display_aloha_circle_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if (state->set)
        Display_.aloha_circle = atoi(which);
    else
        Display_.aloha_circle = 0;

    redraw_on_new_data = 2;     // Immediate screen update
}





void Display_ambiguity_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if (state->set)
        Display_.ambiguity = atoi(which);
    else
        Display_.ambiguity = 0;

    redraw_on_new_data = 2;     // Immediate screen update
}





void Display_phg_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if (state->set) {
        Display_.phg = atoi(which);
        XtSetSensitive(display_default_phg_button,   TRUE);
        XtSetSensitive(display_phg_of_moving_button, TRUE);
    }
    else {
        Display_.phg = 0;
        XtSetSensitive(display_default_phg_button,   FALSE);
        XtSetSensitive(display_phg_of_moving_button, FALSE);
    }

    redraw_on_new_data = 2;     // Immediate screen update
}





void Display_default_phg_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if (state->set)
        Display_.default_phg = atoi(which);
    else
        Display_.default_phg = 0;

    redraw_on_new_data = 2;     // Immediate screen update
}





void Display_phg_of_moving_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if (state->set)
        Display_.phg_of_moving = atoi(which);
    else
        Display_.phg_of_moving = 0;

    redraw_on_new_data = 2;     // Immediate screen update
}





void Display_df_data_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if (state->set)
        Display_.df_data = atoi(which);
    else
        Display_.df_data = 0;

    redraw_on_new_data = 2;     // Immediate screen update
}





void Display_dr_data_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if (state->set) {
        Display_.dr_data = atoi(which);
        XtSetSensitive(display_dr_arc_button,    TRUE);
        XtSetSensitive(display_dr_course_button, TRUE);
        XtSetSensitive(display_dr_symbol_button, TRUE);
    }
    else {
        Display_.dr_data = 0;
        XtSetSensitive(display_dr_arc_button,    FALSE);
        XtSetSensitive(display_dr_course_button, FALSE);
        XtSetSensitive(display_dr_symbol_button, FALSE);
    }

    redraw_on_new_data = 2;     // Immediate screen update
}





void Display_dr_arc_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if (state->set)
        Display_.dr_arc = atoi(which);
    else
        Display_.dr_arc = 0;

    redraw_on_new_data = 2;     // Immediate screen update
}





void Display_dr_course_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if (state->set)
        Display_.dr_course = atoi(which);
    else
        Display_.dr_course = 0;

    redraw_on_new_data = 2;     // Immediate screen update
}





void Display_dr_symbol_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if (state->set)
        Display_.dr_symbol = atoi(which);
    else
        Display_.dr_symbol = 0;

    redraw_on_new_data = 2;     // Immediate screen update
}





void Display_dist_bearing_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if (state->set)
        Display_.dist_bearing = atoi(which);
    else
        Display_.dist_bearing = 0;

    redraw_on_new_data = 2;     // Immediate screen update
}





void Display_last_heard_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if (state->set)
        Display_.last_heard = atoi(which);
    else
        Display_.last_heard = 0;

    redraw_on_new_data = 2;     // Immediate screen update
}





/*
 *  Toggle unit system (button callbacks)
 */
void  Units_choice_toggle( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        english_units = atoi(which);
    else
        english_units = 0;

    redraw_on_new_data=2;
    update_units();                     // setup conversion
    fill_wx_data();
}





/*
 *  Toggle dist/bearing status (button callbacks)
 */
void  Dbstatus_choice_toggle( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        do_dbstatus = atoi(which);
    else
        do_dbstatus = 0;
    // we need to rebuild main window now???
    XtVaSetValues(text2, XmNwidth, do_dbstatus?((37*FONT_WIDTH)+2):((24*FONT_WIDTH)+2), NULL);

    redraw_on_new_data=2;
}






/*
 *  Update global variables for unit conversion
 *
 * Be careful using these, as they change based on the value of
 * english_units!  These variable should only be used when
 * DISPLAYING data, not when converting numbers for use in internal
 * storage or equations.
 *
 */
void update_units(void) {

    switch (english_units) {
        case 1:     // English
            xastir_snprintf(un_alt,sizeof(un_alt),"ft");
            xastir_snprintf(un_dst,sizeof(un_dst),"mi");
            xastir_snprintf(un_spd,sizeof(un_spd),"mph");
            cvt_m2len  = 3.28084;   // m to ft
            cvt_dm2len = 0.328084;  // dm to ft
            cvt_hm2len = 0.0621504; // hm to mi
            cvt_kn2len = 1.1508;    // knots to mph
            cvt_mi2len = 1.0;       // mph to mph
            break;
        case 2:     // Nautical
            // add nautical miles and knots for future use
            xastir_snprintf(un_alt,sizeof(un_alt),"ft");
            xastir_snprintf(un_dst,sizeof(un_dst),"nm");
            xastir_snprintf(un_spd,sizeof(un_spd),"kn");
            cvt_m2len  = 3.28084;   // m to ft
            cvt_dm2len = 0.328084;  // dm to ft
            cvt_hm2len = 0.0539957; // hm to nm
            cvt_kn2len = 1.0;       // knots to knots
            cvt_mi2len = 0.8689607; // mph to knots / mi to nm
            break;
        default:    // Metric
            xastir_snprintf(un_alt,sizeof(un_alt),"m");
            xastir_snprintf(un_dst,sizeof(un_dst),"km");
            xastir_snprintf(un_spd,sizeof(un_spd),"km/h");
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





void  Emergency_beacon_toggle( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set) {
        emergency_beacon = atoi(which);

//WE7U
// We need to send a posit or two immediately, shorten the interval
// between posits, and add the string "EMERGENCY" to the posit
// before anything else in the comment field.

        transmit_now = 1;

    }
    else {
        emergency_beacon = 0;
    }
}





void  Server_port_toggle( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set) {
        // Start the listening socket.  If we fork it early we end
        // up with much smaller process memory allocated for it and
        // all its children.  The server will authenticate each
        // client that connects.  We'll share all of our data with
        // the server, which will send it to all
        // connected/authenticated clients.  Anything transmitted by
        // the clients will come back to us and standard igating
        // rules should apply from there.
        //
        enable_server_port = atoi(which);
        tcp_server_pid = Fork_TCP_server(my_argc, my_argv, my_envp);
        udp_server_pid = Fork_UDP_server(my_argc, my_argv, my_envp);
    }
    else {
        enable_server_port = 0;
        shut_down_server();
    }
}




void Help_About( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer callData) {
    Widget d;
    Widget child;
    XmString xms, xa, xb;
    Arg al[400];
    unsigned int ac;
    float version;
    char string1[200];
    char string2[200];
    extern char compiledate[];
   
    xastir_snprintf(string2, sizeof(string2),"\nXastir V%s \n",xastir_version);
    xb = XmStringCreateLtoR(string2, XmFONTLIST_DEFAULT_TAG);
    xa = XmStringCreateLtoR(compiledate, XmFONTLIST_DEFAULT_TAG);
    xms = XmStringConcat(xb, xa);
    XmStringFree(xa);
    XmStringFree(xb);
    //xms is still defined

    xa = XmStringCreateLtoR("\n\n" ABOUT_MSG "\n\nLibraries used: " XASTIR_INSTALLED_LIBS, XmFONTLIST_DEFAULT_TAG);  // Add some newlines
    xb = XmStringConcat(xms, xa);
    XmStringFree(xa);
    XmStringFree(xms);
    //xb is still defined

    xa = XmStringCreateLtoR("\n" XmVERSION_STRING, XmFONTLIST_DEFAULT_TAG);  // Add the Motif version string
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
#endif  // AVE_NETAX25_AXLIB_H

#ifdef HAVE_MAGICK
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
#endif  // HAVE_MAGICK
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
    // "About Xastir"
    XtSetArg(al[ac], XmNtitle, langcode("PULDNHEL05") ); ac++;
    XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
    XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
    XtSetArg(al[ac], XmNfontList, fontlist1); ac++;

    // "About Xastir" 
    d = XmCreateInformationDialog(appshell, langcode("PULDNHEL05"), al, ac);
    XmStringFree(xms);
    XtDestroyWidget(XmMessageBoxGetChild(d, (unsigned char)XmDIALOG_CANCEL_BUTTON));
    XtDestroyWidget(XmMessageBoxGetChild(d, (unsigned char)XmDIALOG_HELP_BUTTON));

    child = XmMessageBoxGetChild(d, XmDIALOG_MESSAGE_LABEL);
    XtVaSetValues(child, XmNfontList, fontlist1, NULL);

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





// Turn on or off "Station Capabilities", callback for capabilities
// button.
//
void Capabilities_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        show_only_station_capabilities = TRUE;
    else
        show_only_station_capabilities = FALSE;
}





// Turn on or off "Mine Only"
//
void Display_packet_mine_only_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        Display_packet_data_mine_only = TRUE;
    else
        Display_packet_data_mine_only = FALSE;
}





void Display_data( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer callData) {
    Widget pane, my_form, button_close, option_box, tnc_data,
        net_data, tnc_net_data, capabilities_button,
        mine_only_button;
    unsigned int n;
    Arg args[50];
    Atom delw;

    if (!Display_data_dialog) {
        Display_data_dialog = XtVaCreatePopupShell(langcode("WPUPDPD001"),
                xmDialogShellWidgetClass, appshell,
                XmNdeleteResponse, XmDESTROY,
                XmNdefaultPosition, FALSE,
                XmNfontList, fontlist1,
                NULL);

        pane = XtVaCreateWidget("Display_data pane",
                xmPanedWindowWidgetClass, 
                Display_data_dialog,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        my_form =  XtVaCreateWidget("Display_data my_form",
                xmFormWidgetClass,
                pane,
                XmNfractionBase, 5,
                XmNautoUnmanage, FALSE,
                XmNshadowThickness, 1,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);


        /* set colors */
        n=0;
        XtSetArg(args[n],XmNforeground, MY_FG_COLOR); n++;
        XtSetArg(args[n],XmNbackground, MY_BG_COLOR); n++;
        XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
        XtSetArg(args[n], XmNtopOffset, 5); n++;
        XtSetArg(args[n], XmNbottomAttachment, XmATTACH_NONE); n++;
        XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
        XtSetArg(args[n], XmNleftOffset, 5); n++;
        XtSetArg(args[n], XmNrightAttachment, XmATTACH_NONE); n++;
        XtSetArg(args[n], XmNfontList, fontlist1); n++;
 
        option_box = XmCreateRadioBox(my_form,
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
                XmNfontList, fontlist1,
                NULL);

        XtAddCallback(tnc_data,XmNvalueChangedCallback,Display_packet_toggle,"1");

        net_data = XtVaCreateManagedWidget(langcode("WPUPDPD003"),
                xmToggleButtonGadgetClass,
                option_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        XtAddCallback(net_data,XmNvalueChangedCallback,Display_packet_toggle,"2");

        tnc_net_data = XtVaCreateManagedWidget(langcode("WPUPDPD004"),
                xmToggleButtonGadgetClass,
                option_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        XtAddCallback(tnc_net_data,XmNvalueChangedCallback,Display_packet_toggle,"0");

        capabilities_button = XtVaCreateManagedWidget(langcode("WPUPDPD007"),
                xmToggleButtonGadgetClass,
                my_form,
                XmNvisibleWhenOff, TRUE,
                XmNindicatorSize, 12,
                XmNtopAttachment, XmATTACH_FORM,
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

        XtAddCallback(capabilities_button, XmNvalueChangedCallback,Capabilities_toggle,"1");
 
        mine_only_button = XtVaCreateManagedWidget(langcode("WPUPDPD008"),
                xmToggleButtonGadgetClass,
                my_form,
                XmNvisibleWhenOff, TRUE,
                XmNindicatorSize, 12,
                XmNtopAttachment, XmATTACH_FORM,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, capabilities_button,
                XmNleftOffset, 20,
                XmNrightAttachment, XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        XtAddCallback(mine_only_button, XmNvalueChangedCallback,Display_packet_mine_only_toggle,"1");

        n=0;
        XtSetArg(args[n], XmNrows, 15); n++;
        XtSetArg(args[n], XmNcolumns, 100); n++;
        XtSetArg(args[n], XmNeditable, FALSE); n++;
        XtSetArg(args[n], XmNeditMode, XmMULTI_LINE_EDIT); n++;
        XtSetArg(args[n], XmNwordWrap, TRUE); n++;
        XtSetArg(args[n], XmNforeground, MY_FG_COLOR); n++;
        XtSetArg(args[n], XmNbackground, MY_BG_COLOR); n++;
        XtSetArg(args[n], XmNscrollHorizontal, TRUE); n++;
        XtSetArg(args[n], XmNscrollVertical, TRUE); n++;
        XtSetArg(args[n], XmNcursorPositionVisible, FALSE); n++;
        XtSetArg(args[n], XmNtraversalOn, FALSE); n++;
        XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
        XtSetArg(args[n], XmNtopWidget, option_box); n++;
        XtSetArg(args[n], XmNtopOffset, 5); n++;
        XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
        XtSetArg(args[n], XmNbottomOffset, 30); n++;
        XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
        XtSetArg(args[n], XmNleftOffset, 5); n++;
        XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
        XtSetArg(args[n], XmNrightOffset, 5); n++;
        XtSetArg(args[n], XmNfontList, fontlist1); n++;

 
//        XtSetArg(args[n], XmNnavigationType, XmTAB_GROUP); n++;
        Display_data_text=NULL;
        Display_data_text = XmCreateScrolledText(my_form,
                "Display_data text",
                args,
                n);
 

        button_close = XtVaCreateManagedWidget(langcode("UNIOP00003"),
                xmPushButtonGadgetClass, 
                my_form,
                XmNtopAttachment, XmATTACH_NONE,
                XmNbottomAttachment, XmATTACH_FORM,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 2,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 3,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        XtAddCallback(button_close, XmNactivateCallback, Display_data_destroy_shell, Display_data_dialog);


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

        if (show_only_station_capabilities)
            XmToggleButtonSetState(capabilities_button,TRUE,FALSE);
        else
            XmToggleButtonSetState(capabilities_button,FALSE,FALSE);

        if (Display_packet_data_mine_only)
            XmToggleButtonSetState(mine_only_button,TRUE,FALSE);
        else
            XmToggleButtonSetState(mine_only_button,FALSE,FALSE);

        XtManageChild(option_box);
        XtManageChild(Display_data_text);
        XtVaSetValues(Display_data_text, XmNbackground, colors[0x0f], NULL);
        XtManageChild(my_form);
        XtManageChild(pane);

        redraw_on_new_packet_data=1;
        XtPopup(Display_data_dialog,XtGrabNone);

//        fix_dialog_vsize(Display_data_dialog);

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
    int i;
    Position x, y;
    unsigned int n;
    char *temp;
    char title[200];
    char temp2[200];
    char temp3[200];
    FILE *f;
    XmString *list;
    int open;
    Arg args[50];
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
                    xmDialogShellWidgetClass, appshell,
                    XmNdeleteResponse,XmDESTROY,
                    XmNdefaultPosition, FALSE,
                    XmNfontList, fontlist1,
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
            XtSetArg(args[n], XmNfontList, fontlist1); n++;

            help_text=NULL;
            help_text = XmCreateScrolledText(my_form,
                    "help_view help text",
                    args,
                    n);

            f=fopen( get_user_base_dir(HELP_FILE), "r" );
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
                fprintf(stderr,"Couldn't open file: %s\n", get_user_base_dir(HELP_FILE) );

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
                    XmNfontList, fontlist1,
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
    Arg al[50];           /* Arg List */
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
                XmNfontList, fontlist1,
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
        XtSetArg(al[ac], XmNfontList, fontlist1); ac++;

        help_list = XmCreateScrolledList(my_form,
                "Help_Index list",
                al,
                ac);

        n=1;
        f=fopen( get_user_base_dir(HELP_FILE), "r" );
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
            fprintf(stderr,"Couldn't open file: %s\n", get_user_base_dir(HELP_FILE) );

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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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

    // Reload saved objects and items from previous runs.
    // This implements persistent objects.
    reload_object_item();

    redraw_on_new_data=2;
}





/************************* Map Properties*********************************/
/*************************************************************************/

// Destroys the Map Properties dialog
void map_properties_destroy_shell( /*@unused@*/ Widget widget, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    Widget shell = (Widget) clientData;
    XtPopdown(shell);
    XtDestroyWidget(shell);
    map_properties_dialog = (Widget)NULL;
    re_sort_maps = 1;

    if (map_chooser_dialog) {
        XtSetSensitive(map_chooser_button_ok, TRUE);
        XtSetSensitive(map_chooser_button_cancel, TRUE);
    }
}





//WE7U
// Possible changes:
// *) Save/restore the map selections while changing properties.
//    Malloc a char array the size of the map_properties_list and
//    fill it in based on the current highlighting.  Free it when
//    we're done.
// *) Change the labels at the top into buttons?  Zoom/Layer buttons
//    would pop up a dialog asking for the number.  Others would
//    just toggle the feature.
// *) Change to a single "Apply" button.  This won't allow us to
//    easily change only some parameters unless we skip input fields
//    that are blank.  Run through highlighted items, fill in input
//    fields if the parameter is the same for all.  If different for
//    some, leave input field blank.
// *) Bring up an "Abandon Changes?" confirmation dialog if input
//    fields are filled in but "Cancel" was pressed instead of
//    "Apply".



// Fills in the map properties file entries.
//
void map_properties_fill_in (void) {
    int n,i;
    XmString str_ptr;
    map_index_record *current = map_index_head;
    int top_position;


    busy_cursor(appshell);

    i=0;
    if (map_properties_dialog) {
        char *current_selections = NULL;
        int kk, mm;


        // Save our current place in the dialog
        XtVaGetValues(map_properties_list,
            XmNtopItemPosition, &top_position,
            NULL);

        // Get the list count from the dialog
        XtVaGetValues(map_properties_list,
            XmNitemCount,&kk,
            NULL);

        if (kk) {   // If list is not empty
            // Allocate enough chars to hold the highlighting info
            current_selections = (char *)malloc(sizeof(char) * kk);
//fprintf(stderr,"List entries:%d\n", kk);

            // Iterate over the list, saving the highlighting values
            for (mm = 0; mm < kk; mm++) {
                if (XmListPosSelected(map_properties_list, mm))
                    current_selections[mm] = 1;
                else
                    current_selections[mm] = 0;
            }
        }

//        fprintf(stderr,"Top Position: %d\n",top_position);

        // Empty the map_properties_list widget first
        XmListDeleteAllItems(map_properties_list);

        // Put all the map files/dirs in the map_index into the Map
        // Properties dialog list (map_properties_list).  Include
        // the map_layer and draw_filled variables on the line.
        n=1;


//
// We wish to only show the files that are currently selected in the
// map_chooser_dialog's map_list widget.  We'll need to run down
// that widget's entries, checking whether each line is selected,
// and only display it in the map_properties_list widget if selected
// and a match with our string.
//
// One method would be to make the Map Chooser selections set a bit
// in the in-memory index, so that we can tell which ones are
// selected without a bunch of string compares.  The bit would need
// to be tweaked on starting up the map chooser (setting the
// selected entries that match the selected_maps.sys file), and when
// the user tweaked a selection.
//
// What we don't want to get into is an n*n set of string compares
// between two lists, which would be very slow.  If they're both
// ordered lists though, we'll end up with at most a 2n multiplier,
// which is much better.  If we can pass the info between the lists
// with a special entry in the record, we don't slow down at all.
//
// Reasonably fast method:  Create a new list that contains only the
// selected items from map_list.  Run through this list as we
// populate map_properties_list from the big linked list.
//
// Actually, it's probably just as fast to run down through
// map_list, looking up records for every line that's selected.
// Just keep the pointers incrementing for each list instead of
// running through the entire in-memory linked list for every
// selected item in map_list.
//
// For selected directories, we need to add each file that has that
// initial directory name.  We should be able to do this with a
// match that stops at the end of the directory name.
//
// We need to grey-out the buttons in the Map Chooser until the
// Properties dialog closes.  Otherwise we might not able to get to
// to the map_list widget to re-do the Properties list when a button
// is pressed in the Properties dialog (if the user closes the Map
// Chooser).
//


        // Set all of the temp_select bits to zero in the in-memory
        // map index.
        map_index_temp_select_clear();

        if (map_chooser_dialog) {
            map_index_record *ptr = map_index_head;
            int jj, x;
            XmString *list;
            char *temp;

            // Get the list and list count from the Map Chooser
            // dialog.
            XtVaGetValues(map_list,
                XmNitemCount,&jj,
                XmNitems,&list,
                NULL);

            // Find all selected files/directories in the Map
            // Chooser.  Set the "temp_select" bits in the in-memory
            // map index to correspond.
            //
            for(x=1; x<=jj; x++)
            {
                if (XmListPosSelected(map_list,x)) {
                    // Snag the filename portion from the line
                    if (XmStringGetLtoR(list[(x-1)],XmFONTLIST_DEFAULT_TAG,&temp)) {

                        // Update this file or directory in the in-memory
                        // map index, setting the "temp_select" field to 1.
                        map_index_update_temp_select(temp, &ptr);
                        XtFree(temp);
                    }
                }
            }
        }

        // We should now have all of the files/directories marked in
        // the in-memory map index.  Files underneath selected
        // directories should also be marked by this point, as the
        // map_index_update_temp_select() routine should assure
        // this.

        while (current != NULL) {

if (current->temp_select) {

            //fprintf(stderr,"%s\n",current->filename);

            // Make sure it's a file and not a directory
            if (current->filename[strlen(current->filename)-1] != '/') {
                char temp[MAX_FILENAME];
                char temp_layer[10];
                char temp_max_zoom[10];
                char temp_min_zoom[10];
                char temp_filled[20];
                char temp_drg[20];
                char temp_auto[20];
                int len, start;


                // We have a file.  Construct the line that we wish
                // to place in the list

                // JMT - this is a guess
                if (current->max_zoom == 0) {
                    xastir_snprintf(temp_max_zoom,
                        sizeof(temp_max_zoom),
                        "  -  ");
                }
                else {
                    xastir_snprintf(temp_max_zoom,
                    sizeof(temp_max_zoom),
                    "%5d",
                    current->max_zoom);
                }

                if (current->min_zoom == 0) {
                    xastir_snprintf(temp_min_zoom,
                        sizeof(temp_min_zoom),
                        "  -  ");
                }
                else {
                    xastir_snprintf(temp_min_zoom,
                    sizeof(temp_min_zoom),
                    "%5d",
                    current->min_zoom);
                }

                if (current->map_layer == 0) {
                    xastir_snprintf(temp_layer,
                        sizeof(temp_layer),
                        "  -  ");
                }
                else {
                    xastir_snprintf(temp_layer,
                    sizeof(temp_layer),
                    "%5d",
                    current->map_layer);
                }

                xastir_snprintf(temp_filled,
                    sizeof(temp_filled),
                    "     ");

                switch (current->draw_filled) {

                    case 0: // Global No-Fill (vector)

                        // Center the string in the column
                        len = strlen(langcode("MAPP007"));
                        start = (int)( (5 - len) / 2 + 0.5);

                        if (start < 0)
                            start = 0;

                        // Insert the string.  Fill with spaces
                        // on the end.
                        xastir_snprintf(&temp_filled[start],
                            sizeof(temp_filled)-start,
                            "%s     ",
                            langcode("MAPP007"));   // "No"

                        break;

                    case 1: // Global Fill

                        // Center the string in the column
                        len = strlen(langcode("MAPP006"));
                        start = (int)( (5 - len) / 2 + 0.5);

                        if (start < 0)
                            start = 0;

                        // Insert the string.  Fill with spaces
                        // on the end.
                        xastir_snprintf(&temp_filled[start],
                            sizeof(temp_filled)-start,
                            "%s     ",
                            langcode("MAPP006"));   // "Yes"

                        break;

                    case 2: // Auto
                    default:

                        // Center the string in the column
                        len = strlen(langcode("MAPP011"));
                        start = (int)( (5 - len) / 2 + 1.5);

                        if (start < 0)
                            start = 0;

                        // Insert the string.  Fill with spaces
                        // on the end.
                        xastir_snprintf(&temp_filled[start],
                            sizeof(temp_filled)-start,
                            "%s     ",
                            langcode("MAPP011"));   // "Auto"

                        break;

                    }   // End of switch

                    // Truncate it so it fits our column width.
                    temp_filled[5] = '\0';

                xastir_snprintf(temp_drg,
                    sizeof(temp_drg),
                    "     ");

                switch (current->usgs_drg) {

                    case 0: // No

                        // Center the string in the column
                        len = strlen(langcode("MAPP007"));
                        start = (int)( (5 - len) / 2 + 0.5);

                        if (start < 0)
                            start = 0;

                        // Insert the string.  Fill with spaces
                        // on the end.
                        xastir_snprintf(&temp_drg[start],
                            sizeof(temp_drg)-start,
                            "%s     ",
                            langcode("MAPP007"));   // "No"

                        break;

                    case 1: // Yes

                        // Center the string in the column
                        len = strlen(langcode("MAPP006"));
                        start = (int)( (5 - len) / 2 + 0.5);

                        if (start < 0)
                            start = 0;

                        // Insert the string.  Fill with spaces
                        // on the end.
                        xastir_snprintf(&temp_drg[start],
                            sizeof(temp_drg)-start,
                            "%s     ",
                            langcode("MAPP006"));   // "Yes"

                        break;

                    case 2: // Auto
                    default:

                        // Center the string in the column
                        len = strlen(langcode("MAPP011"));
                        start = (int)( (5 - len) / 2 + 1.5);

                        if (start < 0)
                            start = 0;

                        // Insert the string.  Fill with spaces
                        // on the end.
                        xastir_snprintf(&temp_drg[start],
                            sizeof(temp_drg)-start,
                            "%s     ",
                            langcode("MAPP011"));   // "Auto"

                        break;

                    }   // End of switch

                    // Truncate it so it fits our column width.
                    temp_drg[5] = '\0';

                xastir_snprintf(temp_auto,
                    sizeof(temp_auto),
                    "     ");

                if (current->auto_maps) {
                    int len, start;

                    // Center the string in the column
                    len = strlen(langcode("MAPP006"));
                    start = (int)( (5 - len) / 2 + 0.5);

                    if (start < 0)
                        start = 0;

                    // Insert the string.  Fill with spaces on the
                    // end.
                    xastir_snprintf(&temp_auto[start],
                        sizeof(temp_filled)-start,
                        "%s     ",
                        langcode("MAPP006"));

                    // Truncate it so it fits our column width.
                    temp_auto[5] = '\0';
                }

                //WARNING WARNING WARNING --- changing this format string
                // REQUIRES changing the defined constant MPD_FILENAME_OFFSET
                // at the top of this file, or all the routines that try
                // to decode the string will be wrong!
                xastir_snprintf(temp,
                    sizeof(temp),
                    "%s %s %s %s %s %s  %s",
                    temp_max_zoom,
                    temp_min_zoom,
                    temp_layer,
                    temp_filled,
                    temp_drg,
                    temp_auto,
                    current->filename);

                XmListAddItem(map_properties_list,
                    str_ptr = XmStringCreateLtoR(temp,
                                 XmFONTLIST_DEFAULT_TAG),
                                 n);
                n++;
                XmStringFree(str_ptr);
            }
}

            current = current->next;
        }

        if (kk) {   // If list is not empty
            // Restore the highlighting values
            for (mm = 0; mm < kk; mm++) {
                if (current_selections[mm])
                    XmListSelectPos(map_properties_list,mm,TRUE);
            }
            // Free the highlighting array we allocated
            free(current_selections);
        }

        // Restore our place in the dialog
        XtVaSetValues(map_properties_list,
            XmNtopItemPosition, top_position,
            NULL);
    }
}





// Removes the highlighting for maps in the current view of the map
// properties list.
//
void map_properties_deselect_maps(Widget widget, XtPointer clientData, XtPointer callData) {
    int i, x;
    XmString *list;

    // Get the list and the count from the dialog
    XtVaGetValues(map_properties_list,
               XmNitemCount,&i,
               XmNitems,&list,
               NULL);

    // Run through the widget's list, deselecting every line
    for(x=1; x<=i;x++) {
        if (XmListPosSelected(map_properties_list,x)) {
            XmListDeselectPos(map_properties_list,x);
        }
    }
}





// Selects all maps in the current view of the map properties list.
//
void map_properties_select_all_maps(Widget widget, XtPointer clientData, XtPointer
 callData) {
    int i, x;
    XmString *list;

    // Get the list and the count from the dialog
    XtVaGetValues(map_properties_list,
               XmNitemCount,&i,
               XmNitems,&list,
               NULL);

    // Run through the widget's list, selecting every line
    for(x=1; x<=i;x++) {
        // Deselect each one first, in case already selected
        XmListDeselectPos(map_properties_list,x);

        // Select/highlight that position
        XmListSelectPos(map_properties_list,x,TRUE);
    }
}





// Change the "draw_filled" field in the in-memory map_index to a
// two.
void map_index_update_filled_auto(char *filename) {
    map_index_record *current = map_index_head;

    while (current != NULL) {
        if (strcmp(current->filename,filename) == 0) {
            // Found a match.  Update the field and return.
            current->draw_filled = 2;
            return;
        }
        current = current->next;
    }
}





// Change the "draw_filled" field in the in-memory map_index to a
// one.
void map_index_update_filled_yes(char *filename) {
    map_index_record *current = map_index_head;

    while (current != NULL) {
        if (strcmp(current->filename,filename) == 0) {
            // Found a match.  Update the field and return.
            current->draw_filled = 1;
            return;
        }
        current = current->next;
    }
}





// Change the "draw_filled" field in the in-memory map_index to a
// zero.
void map_index_update_filled_no(char *filename) {
    map_index_record *current = map_index_head;

    while (current != NULL) {
        if (strcmp(current->filename,filename) == 0) {
            // Found a match.  Update the field and return.
            current->draw_filled = 0;
            return;
        }
        current = current->next;
    }
}





void map_properties_filled_auto(Widget widget, XtPointer clientData, XtPointer callData) {
    int i, x;
    XmString *list;
    char *temp;


    // Get the list and the count from the dialog
    XtVaGetValues(map_properties_list,
               XmNitemCount,&i,
               XmNitems,&list,
               NULL);

    // Run through the widget's list, changing the filled field on
    // every one that is selected.
    for(x=1; x<=i;x++)
    {
        // If the line was selected
        if ( XmListPosSelected(map_properties_list,x) ) {
 
            // Snag the filename portion from the line
            if (XmStringGetLtoR(list[(x-1)],XmFONTLIST_DEFAULT_TAG,&temp)) {
                char *temp2;

                // Need to get rid of the first XX characters on the
                // line in order to come up with just the
                // path/filename portion.
//OFFSET IS CRITICAL HERE!!!  If we change how the strings are
//printed into the map_properties_list, we have to change this
//offset.
                temp2 = temp + MPD_FILENAME_OFFSET;

//fprintf(stderr,"New string:%s\n",temp2);

                // Update this file or directory in the in-memory
                // map index, setting the "draw_filled" field to 2.
                map_index_update_filled_auto(temp2);
                XtFree(temp);
            }
        }
    }

    // Delete all entries in the list and re-create anew.
    map_properties_fill_in();

    // Save the updated index to the file
    index_save_to_file();
}





void map_properties_filled_yes(Widget widget, XtPointer clientData, XtPointer callData) {
    int i, x;
    XmString *list;
    char *temp;


    // Get the list and the count from the dialog
    XtVaGetValues(map_properties_list,
               XmNitemCount,&i,
               XmNitems,&list,
               NULL);

    // Run through the widget's list, changing the filled field on
    // every one that is selected.
    for(x=1; x<=i;x++)
    {
        // If the line was selected
        if ( XmListPosSelected(map_properties_list,x) ) {
 
            // Snag the filename portion from the line
            if (XmStringGetLtoR(list[(x-1)],XmFONTLIST_DEFAULT_TAG,&temp)) {
                char *temp2;

                // Need to get rid of the first XX characters on the
                // line in order to come up with just the
                // path/filename portion.
//OFFSET IS CRITICAL HERE!!!  If we change how the strings are
//printed into the map_properties_list, we have to change this
//offset.
                temp2 = temp + MPD_FILENAME_OFFSET;

//fprintf(stderr,"New string:%s\n",temp2);

                // Update this file or directory in the in-memory
                // map index, setting the "draw_filled" field to 1.
                map_index_update_filled_yes(temp2);
                XtFree(temp);
            }
        }
    }

    // Delete all entries in the list and re-create anew.
    map_properties_fill_in();

    // Save the updated index to the file
    index_save_to_file();
}





void map_properties_filled_no(Widget widget, XtPointer clientData, XtPointer callData) {
    int i, x;
    XmString *list;
    char *temp;


    // Get the list and the count from the dialog
    XtVaGetValues(map_properties_list,
               XmNitemCount,&i,
               XmNitems,&list,
               NULL);

    // Run through the widget's list, changing the filled field on
    // every one that is selected.
    for(x=1; x<=i;x++)
    {
        // If the line was selected
        if ( XmListPosSelected(map_properties_list,x) ) {
 
            // Snag the filename portion from the line
            if (XmStringGetLtoR(list[(x-1)],XmFONTLIST_DEFAULT_TAG,&temp)) {
                char *temp2;

                // Need to get rid of the first XX characters on the
                // line in order to come up with just the
                // path/filename portion.
//OFFSET IS CRITICAL HERE!!!  If we change how the strings are
//printed into the map_properties_list, we have to change this
//offset.
                temp2 = temp + MPD_FILENAME_OFFSET;

//fprintf(stderr,"New string:%s\n",temp2);

                // Update this file or directory in the in-memory
                // map index, setting the "draw_filled" field to 0.
                map_index_update_filled_no(temp2);
                XtFree(temp);
            }
        }
    }

    // Delete all entries in the list and re-create anew.
    map_properties_fill_in();

    // Save the updated index to the file
    index_save_to_file();
}



// Change the "usgs_drg" field in the in-memory map_index to a
// specified value.
void map_index_update_usgs_drg(char *filename, int drg_setting) {
    map_index_record *current = map_index_head;

    while (current != NULL) {
        if (strcmp(current->filename,filename) == 0) {
            // Found a match.  Update the field and return.
            current->usgs_drg = drg_setting;
            return;
        }
        current = current->next;
    }
}




// common functionality of all the callbacks.  Probably don't even need 
// all the X data here, either
void map_properties_usgs_drg(Widget widget, XtPointer clientData, XtPointer callData, int drg_setting) {
    int i, x;
    XmString *list;
    char *temp;

    // Get the list and the count from the dialog
    XtVaGetValues(map_properties_list,
               XmNitemCount,&i,
               XmNitems,&list,
               NULL);

    // Run through the widget's list, changing the usgs_drg field on
    // every one that is selected.
    for(x=1; x<=i;x++)
    {
        // If the line was selected
        if ( XmListPosSelected(map_properties_list,x) ) {
 
            // Snag the filename portion from the line
            if (XmStringGetLtoR(list[(x-1)],XmFONTLIST_DEFAULT_TAG,&temp)) {
                char *temp2;

                // Need to get rid of the first XX characters on the
                // line in order to come up with just the
                // path/filename portion.
//OFFSET IS CRITICAL HERE!!!  If we change how the strings are
//printed into the map_properties_list, we have to change this
//offset.
                temp2 = temp + MPD_FILENAME_OFFSET;

//fprintf(stderr,"New string:%s\n",temp2);

                // Update this file or directory in the in-memory
                // map index, setting the "usgs_drg" field to drg_setting.
                map_index_update_usgs_drg(temp2,drg_setting);
                XtFree(temp);
            }
        }
    }

    // Delete all entries in the list and re-create anew.
    map_properties_fill_in();

    // Save the updated index to the file
    index_save_to_file();
}

// the real callbacks
void map_properties_usgs_drg_auto(Widget widget, XtPointer clientData, XtPointer callData) {
    map_properties_usgs_drg(widget, clientData, callData, 2);
}
void map_properties_usgs_drg_yes(Widget widget, XtPointer clientData, XtPointer callData) {
    map_properties_usgs_drg(widget, clientData, callData, 1);
}
void map_properties_usgs_drg_no(Widget widget, XtPointer clientData, XtPointer callData) {
    map_properties_usgs_drg(widget, clientData, callData, 0);
}





// Change the "auto_maps" field in the in-memory map_index to a one.
void map_index_update_auto_maps_yes(char *filename) {
    map_index_record *current = map_index_head;

    while (current != NULL) {
        if (strcmp(current->filename,filename) == 0) {
            // Found a match.  Update the field and return.
            current->auto_maps = 1;
            return;
        }
        current = current->next;
    }
}





// Change the "auto_maps" field in the in-memory map_index to a
// zero.
void map_index_update_auto_maps_no(char *filename) {
    map_index_record *current = map_index_head;

    while (current != NULL) {
        if (strcmp(current->filename,filename) == 0) {
            // Found a match.  Update the field and return.
            current->auto_maps = 0;
            return;
        }
        current = current->next;
    }
}





void map_properties_auto_maps_yes(Widget widget, XtPointer clientData, XtPointer callData) {
    int i, x;
    XmString *list;
    char *temp;


    // Get the list and the count from the dialog
    XtVaGetValues(map_properties_list,
               XmNitemCount,&i,
               XmNitems,&list,
               NULL);

    // Run through the widget's list, changing the auto_maps field
    // on every one that is selected.
    for(x=1; x<=i;x++)
    {
        // If the line was selected
        if ( XmListPosSelected(map_properties_list,x) ) {
 
            // Snag the filename portion from the line
            if (XmStringGetLtoR(list[(x-1)],XmFONTLIST_DEFAULT_TAG,&temp)) {
                char *temp2;

                // Need to get rid of the first XX characters on the
                // line in order to come up with just the
                // path/filename portion.
//OFFSET IS CRITICAL HERE!!!  If we change how the strings are
//printed into the map_properties_list, we have to change this
//offset.
                temp2 = temp + MPD_FILENAME_OFFSET;

//fprintf(stderr,"New string:%s\n",temp2);

                // Update this file or directory in the in-memory
                // map index, setting the "auto_maps" field to 1.
                map_index_update_auto_maps_yes(temp2);
                XtFree(temp);
            }
        }
    }

    // Delete all entries in the list and re-create anew.
    map_properties_fill_in();

    // Save the updated index to the file
    index_save_to_file();
}





void map_properties_auto_maps_no(Widget widget, XtPointer clientData, XtPointer callData) {
    int i, x;
    XmString *list;
    char *temp;


    // Get the list and the count from the dialog
    XtVaGetValues(map_properties_list,
               XmNitemCount,&i,
               XmNitems,&list,
               NULL);

    // Run through the widget's list, changing the auto_maps field
    // on every one that is selected.
    for(x=1; x<=i;x++)
    {
        // If the line was selected
        if ( XmListPosSelected(map_properties_list,x) ) {
 
            // Snag the filename portion from the line
            if (XmStringGetLtoR(list[(x-1)],XmFONTLIST_DEFAULT_TAG,&temp)) {
                char *temp2;

                // Need to get rid of the first XX characters on the
                // line in order to come up with just the
                // path/filename portion.
//OFFSET IS CRITICAL HERE!!!  If we change how the strings are
//printed into the map_properties_list, we have to change this
//offset.
                temp2 = temp + MPD_FILENAME_OFFSET;

//fprintf(stderr,"New string:%s\n",temp2);

                // Update this file or directory in the in-memory
                // map index, setting the "auto_maps" field to 0.
                map_index_update_auto_maps_no(temp2);
                XtFree(temp);
            }
        }
    }

    // Delete all entries in the list and re-create anew.
    map_properties_fill_in();

    // Save the updated index to the file
    index_save_to_file();
}





// Update the "map_layer" field in the in-memory map_index based on
// the "map_layer" input parameter.
void map_index_update_layer(char *filename, int map_layer) {
    map_index_record *current = map_index_head;

    while (current != NULL) {
        if (strcmp(current->filename,filename) == 0) {
            // Found a match.  Update the field and return.
            current->map_layer = map_layer;
            return;
        }
        current = current->next;
    }
}





void map_properties_layer_change(Widget widget, XtPointer clientData, XtPointer callData) {
    int i, x, new_layer;
    XmString *list;
    char *temp;


    // Get new layer selection in the form of an int
    temp = XmTextGetString(new_map_layer_text);
    new_layer = atoi(temp);
    XtFree(temp);

//fprintf(stderr,"New layer selected is: %d\n", new_layer);

    // Get the list and the count from the dialog
    XtVaGetValues(map_properties_list,
               XmNitemCount,&i,
               XmNitems,&list,
               NULL);

    // Run through the widget's list, changing the layer on every
    // one that is selected.
    for(x=1; x<=i;x++)
    {
        // If the line was selected
        if ( XmListPosSelected(map_properties_list,x) ) {
 
            // Snag the filename portion from the line
            if (XmStringGetLtoR(list[(x-1)],XmFONTLIST_DEFAULT_TAG,&temp)) {
                char *temp2;

                // Need to get rid of the first XX characters on the
                // line in order to come up with just the
                // path/filename portion.
//OFFSET IS CRITICAL HERE!!!  If we change how the strings are
//printed into the map_properties_list, we have to change this
//offset.
                temp2 = temp + MPD_FILENAME_OFFSET;

//fprintf(stderr,"New string:%s\n",temp2);

                // Update this file or directory in the in-memory
                // map index, setting/resetting the "selected" field
                // as appropriate.
                map_index_update_layer(temp2, new_layer);
                XtFree(temp);
            }
        }
    }

    // Delete all entries in the list and re-create anew.
    map_properties_fill_in();

    // Save the updated index to the file
    index_save_to_file();
}





// Update the "max_zoom" field in the in-memory map_index based on
// the "max_zoom" input parameter.
void map_index_update_max_zoom(char *filename, int max_zoom) {
    map_index_record *current = map_index_head;

    while (current != NULL) {
        if (strcmp(current->filename,filename) == 0) {
            // Found a match.  Update the field and return.
            current->max_zoom = max_zoom;
            return;
        }
        current = current->next;
    }
}





void map_properties_max_zoom_change(Widget widget, XtPointer clientData, XtPointer callData) {
    int i, x, new_max_zoom;
    XmString *list;
    char *temp;


    // Get new layer selection in the form of an int
    temp = XmTextGetString(new_max_zoom_text);
    new_max_zoom = atoi(temp);
    XtFree(temp);

//    fprintf(stderr,"New max_zoom selected is: %d\n", new_max_zoom);

    // Get the list and the count from the dialog
    XtVaGetValues(map_properties_list,
               XmNitemCount,&i,
               XmNitems,&list,
               NULL);

    // Run through the widget's list, changing the layer on every
    // one that is selected.
    for(x=1; x<=i;x++)
    {
        // If the line was selected
        if ( XmListPosSelected(map_properties_list,x) ) {
 
            // Snag the filename portion from the line
            if (XmStringGetLtoR(list[(x-1)],XmFONTLIST_DEFAULT_TAG,&temp)) {
                char *temp2;

                // Need to get rid of the first XX characters on the
                // line in order to come up with just the
                // path/filename portion.
//OFFSET IS CRITICAL HERE!!!  If we change how the strings are
//printed into the map_properties_list, we have to change this
//offset.
                temp2 = temp + MPD_FILENAME_OFFSET;

//                fprintf(stderr,"New string:%s\n",temp2);

                // Update this file or directory in the in-memory
                // map index, setting/resetting the "selected" field
                // as appropriate.
                map_index_update_max_zoom(temp2, new_max_zoom);
                XtFree(temp);
            }
        }
    }

    // Delete all entries in the list and re-create anew.
    map_properties_fill_in();

    // Save the updated index to the file
    index_save_to_file();
}





// Update the "min_zoom" field in the in-memory map_index based on
// the "min_zoom" input parameter.
void map_index_update_min_zoom(char *filename, int min_zoom) {
    map_index_record *current = map_index_head;

    while (current != NULL) {
        if (strcmp(current->filename,filename) == 0) {
            // Found a match.  Update the field and return.
            current->min_zoom = min_zoom;
            return;
        }
        current = current->next;
    }
}





void map_properties_min_zoom_change(Widget widget, XtPointer clientData, XtPointer callData) {
    int i, x, new_min_zoom;
    XmString *list;
    char *temp;


    // Get new layer selection in the form of an int
    temp = XmTextGetString(new_min_zoom_text);
    new_min_zoom = atoi(temp);
    XtFree(temp);

//fprintf(stderr,"New layer selected is: %d\n", new_layer);

    // Get the list and the count from the dialog
    XtVaGetValues(map_properties_list,
               XmNitemCount,&i,
               XmNitems,&list,
               NULL);

    // Run through the widget's list, changing the layer on every
    // one that is selected.
    for(x=1; x<=i;x++)
    {
        // If the line was selected
        if ( XmListPosSelected(map_properties_list,x) ) {
 
            // Snag the filename portion from the line
            if (XmStringGetLtoR(list[(x-1)],XmFONTLIST_DEFAULT_TAG,&temp)) {
                char *temp2;

                // Need to get rid of the first XX characters on the
                // line in order to come up with just the
                // path/filename portion.
//OFFSET IS CRITICAL HERE!!!  If we change how the strings are
//printed into the map_properties_list, we have to change this
//offset.
                temp2 = temp + MPD_FILENAME_OFFSET;

//fprintf(stderr,"New string:%s\n",temp2);

                // Update this file or directory in the in-memory
                // map index, setting/resetting the "selected" field
                // as appropriate.
                map_index_update_min_zoom(temp2, new_min_zoom);
                XtFree(temp);
            }
        }
    }

    // Delete all entries in the list and re-create anew.
    map_properties_fill_in();

    // Save the updated index to the file
    index_save_to_file();
}





// JMT -- now supports max and min zoom levels

// Allows setting map layer and filled polygon properties for maps
// selected in the map chooser.  Show a warning or bring up a
// confirmation dialog if more than one map is selected when this
// function is entered.  This is the callback function for the
// Properties button in the Map Chooser.
//
// If the map_layer is a range of values, inform the user here
// via a popup, so that they don't make a mistake and change too
// many different types of maps to the same map layer.
//
// We could either show all maps here and allow changing each
// one, or just show min/max map_layer draw_filled properties
// for the maps selected in the Map Chooser.
//
// Create the properties dialog.  Show the map_layer and
// draw_filled properties for the maps.
//
// Could still create Cancel and OK buttons.  Cancel would wipe the
// in-memory list and fetch it from file again.  OK would write the
// in-memory list to disk.
//
void map_properties( /*@unused@*/ Widget widget, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    int i;
//    int x;
//    char *temp;
//    XmString *list;
    static Widget pane, my_form, button_clear, button_close,
        rowcol1, rowcol2, rowcol3, label1, label2, label3, label4, label5,
        button_filled_auto, button_filled_yes, button_filled_no,
        button_usgs_drg_auto, button_usgs_drg_yes, button_usgs_drg_no,
        button_layer_change,
        button_auto_maps_yes, button_auto_maps_no,
        button_max_zoom_change, button_min_zoom_change,
        button_select_all;
    Atom delw;
    Arg al[50];                     // Arg List
    register unsigned int ac = 0;   // Arg Count


    busy_cursor(appshell);

    if (map_chooser_dialog) {
        XtSetSensitive(map_chooser_button_ok, FALSE);
        XtSetSensitive(map_chooser_button_cancel, FALSE);
    }

    i=0;
    if (!map_properties_dialog) {

        map_properties_dialog = XtVaCreatePopupShell(langcode("MAPP001"),
                xmDialogShellWidgetClass, appshell,
                XmNdeleteResponse,XmDESTROY,
                XmNdefaultPosition, FALSE,
                XmNfontList, fontlist1,
                NULL);

        pane = XtVaCreateWidget("Map_properties pane",
                xmPanedWindowWidgetClass, 
                map_properties_dialog,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        my_form =  XtVaCreateWidget("Map_properties my_form",
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
        XtSetArg(al[ac], XmNfontList, fontlist1); ac++;

        map_properties_list = XmCreateScrolledList(my_form,
                "Map_properties list",
                al,
                ac);

        // Find the names of all the map files on disk and put them
        // into map_properties_list
        map_properties_fill_in();

        // Attach a rowcolumn manager widget to my_form to handle
        // the third button row.  Attach it to the bottom of the
        // form.
        rowcol3 = XtVaCreateManagedWidget("Map properties rowcol3", 
                xmRowColumnWidgetClass, 
                my_form,
                XmNorientation, XmHORIZONTAL,
                XmNtopAttachment, XmATTACH_NONE,
                XmNbottomAttachment, XmATTACH_FORM,
                XmNleftAttachment, XmATTACH_FORM,
                XmNrightAttachment, XmATTACH_FORM,
                XmNkeyboardFocusPolicy, XmEXPLICIT,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        // Attach a rowcolumn manager widget to my_form to handle
        // the second button row.  Attach it to the top of rowcol3.
        rowcol2 = XtVaCreateManagedWidget("Map properties rowcol2", 
                xmRowColumnWidgetClass, 
                my_form,
                XmNorientation, XmHORIZONTAL,
                XmNtopAttachment, XmATTACH_NONE,
                XmNbottomAttachment, XmATTACH_WIDGET,
                XmNbottomWidget, rowcol3,
                XmNleftAttachment, XmATTACH_FORM,
                XmNrightAttachment, XmATTACH_FORM,
                XmNkeyboardFocusPolicy, XmEXPLICIT,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        // Attach a rowcolumn manager widget to my_form to handle
        // the first button row.
        rowcol1 = XtVaCreateManagedWidget("Map properties rowcol1", 
                xmRowColumnWidgetClass, 
                my_form,
                XmNorientation, XmHORIZONTAL,
                XmNtopAttachment, XmATTACH_NONE,
                XmNbottomAttachment, XmATTACH_WIDGET,
                XmNbottomWidget, rowcol2,
                XmNleftAttachment, XmATTACH_FORM,
                XmNrightAttachment, XmATTACH_FORM,
                XmNkeyboardFocusPolicy, XmEXPLICIT,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        label1  = XtVaCreateManagedWidget(langcode("MAPP002"),
                xmLabelWidgetClass,
                my_form,
                XmNtopAttachment, XmATTACH_FORM,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset ,10,
                XmNrightAttachment, XmATTACH_NONE,
                XmNsensitive, TRUE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        label2  = XtVaCreateManagedWidget(langcode("MAPP003"),
 
                xmLabelWidgetClass,
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, label1,
                XmNtopOffset, 0,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset ,10,
                XmNrightAttachment, XmATTACH_NONE,
                XmNsensitive, TRUE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        XtVaSetValues(XtParent(map_properties_list),
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, label2,
                XmNtopOffset, 2,
                XmNbottomAttachment, XmATTACH_WIDGET,
                XmNbottomWidget, rowcol1,
                XmNbottomOffset, 2,
                XmNrightAttachment, XmATTACH_FORM,
                XmNrightOffset, 5,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 5,
                XmNfontList, fontlist1,
                NULL);

        // JMT -- this is a guess
// "Max Zoom" stolen from "Change Layer"
        button_max_zoom_change = XtVaCreateManagedWidget(langcode("MAPP009"),
                xmPushButtonGadgetClass, 
                rowcol1,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        new_max_zoom_text = XtVaCreateManagedWidget("Map Properties max zoom number",
                xmTextWidgetClass,
                rowcol1,
                XmNeditable,   TRUE,
                XmNcursorPositionVisible, TRUE,
                XmNsensitive, TRUE,
                XmNshadowThickness,    1,
                XmNcolumns, 6,
                XmNwidth, ((7*7)+2),
                XmNmaxLength, 5,
                XmNbackground, colors[0x0f],
                XmNrightOffset, 1,
                XmNnavigationType, XmTAB_GROUP,
                XmNfontList, fontlist1,
                NULL);

// "Min Zoom" stolen from "Change Layer"
        button_min_zoom_change = XtVaCreateManagedWidget(langcode("MAPP010"),
                xmPushButtonGadgetClass, 
                rowcol1,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        new_min_zoom_text = XtVaCreateManagedWidget("Map Properties min zoom number",
                xmTextWidgetClass,
                rowcol1,
                XmNeditable,   TRUE,
                XmNcursorPositionVisible, TRUE,
                XmNsensitive, TRUE,
                XmNshadowThickness,    1,
                XmNcolumns, 6,
                XmNwidth, ((7*7)+2),
                XmNmaxLength, 5,
                XmNbackground, colors[0x0f],
                XmNrightOffset, 1,
                XmNnavigationType, XmTAB_GROUP,
                XmNfontList, fontlist1,
                NULL);


// "Change Layer"
        button_layer_change = XtVaCreateManagedWidget(langcode("MAPP004"),
                xmPushButtonGadgetClass, 
                rowcol1,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        new_map_layer_text = XtVaCreateManagedWidget("Map Properties new layer number",
                xmTextWidgetClass,
                rowcol1,
                XmNeditable,   TRUE,
                XmNcursorPositionVisible, TRUE,
                XmNsensitive, TRUE,
                XmNshadowThickness,    1,
                XmNcolumns, 6,
                XmNwidth, ((7*7)+2),
                XmNmaxLength, 5,
                XmNbackground, colors[0x0f],
                XmNrightOffset, 1,
                XmNnavigationType, XmTAB_GROUP,
                XmNfontList, fontlist1,
                NULL);

        label3  = XtVaCreateManagedWidget(langcode("MAPP005"),
                xmLabelWidgetClass,
                rowcol2,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

// "Filled-Auto"
        button_filled_auto = XtVaCreateManagedWidget(langcode("MAPP011"),
                xmPushButtonGadgetClass, 
                rowcol2,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

// "Filled-Yes"
        button_filled_yes = XtVaCreateManagedWidget(langcode("MAPP006"),
                xmPushButtonGadgetClass, 
                rowcol2,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

// "Filled-No"
        button_filled_no = XtVaCreateManagedWidget(langcode("MAPP007"),
                xmPushButtonGadgetClass, 
                rowcol2,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

// Automaps
        label4  = XtVaCreateManagedWidget(langcode("MAPP008"),
                xmLabelWidgetClass,
                rowcol2,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

// "Automaps-Yes"
        button_auto_maps_yes = XtVaCreateManagedWidget(langcode("MAPP006"),
                xmPushButtonGadgetClass, 
                rowcol2,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

// "Automaps-No"
        button_auto_maps_no = XtVaCreateManagedWidget(langcode("MAPP007"),
                xmPushButtonGadgetClass, 
                rowcol2,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

// USGS DRG->
        label5  = XtVaCreateManagedWidget(langcode("MAPP012"),
                xmLabelWidgetClass,
                rowcol2,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

// "USGS DRG Auto"
        button_usgs_drg_auto = XtVaCreateManagedWidget(langcode("MAPP011"),
                xmPushButtonGadgetClass, 
                rowcol2,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

// "USGS DRG Yes"
        button_usgs_drg_yes = XtVaCreateManagedWidget(langcode("MAPP006"),
                xmPushButtonGadgetClass, 
                rowcol2,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

// "USGS DRG No"
        button_usgs_drg_no = XtVaCreateManagedWidget(langcode("MAPP007"),
                xmPushButtonGadgetClass, 
                rowcol2,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

// "Select All"
        button_select_all = XtVaCreateManagedWidget(langcode("PULDNMMC09"),
                xmPushButtonGadgetClass,
                rowcol3,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

// "Clear"
        button_clear = XtVaCreateManagedWidget(langcode("PULDNMMC01"),
                xmPushButtonGadgetClass, 
                rowcol3,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

// "Close"
        button_close = XtVaCreateManagedWidget(langcode("UNIOP00003"),
                xmPushButtonGadgetClass, 
                rowcol3,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        XtAddCallback(button_close, XmNactivateCallback, map_properties_destroy_shell, map_properties_dialog);
        XtAddCallback(button_clear, XmNactivateCallback, map_properties_deselect_maps, map_properties_dialog);
        XtAddCallback(button_select_all, XmNactivateCallback, map_properties_select_all_maps, map_properties_dialog);
        XtAddCallback(button_filled_auto, XmNactivateCallback, map_properties_filled_auto, map_properties_dialog);
        XtAddCallback(button_filled_yes, XmNactivateCallback, map_properties_filled_yes, map_properties_dialog);
        XtAddCallback(button_filled_no, XmNactivateCallback, map_properties_filled_no, map_properties_dialog);
        XtAddCallback(button_usgs_drg_auto, XmNactivateCallback, map_properties_usgs_drg_auto, map_properties_dialog);
        XtAddCallback(button_usgs_drg_yes, XmNactivateCallback, map_properties_usgs_drg_yes, map_properties_dialog);
        XtAddCallback(button_usgs_drg_no, XmNactivateCallback, map_properties_usgs_drg_no, map_properties_dialog);
        XtAddCallback(button_max_zoom_change, XmNactivateCallback, map_properties_max_zoom_change, map_properties_dialog);
        XtAddCallback(button_min_zoom_change, XmNactivateCallback, map_properties_min_zoom_change, map_properties_dialog);
        XtAddCallback(button_layer_change, XmNactivateCallback, map_properties_layer_change, map_properties_dialog);
        XtAddCallback(button_auto_maps_yes, XmNactivateCallback, map_properties_auto_maps_yes, map_properties_dialog);
        XtAddCallback(button_auto_maps_no, XmNactivateCallback, map_properties_auto_maps_no, map_properties_dialog);

        pos_dialog(map_properties_dialog);

        delw = XmInternAtom(XtDisplay(map_properties_dialog),"WM_DELETE_WINDOW", FALSE);
        XmAddWMProtocolCallback(map_properties_dialog, delw, map_properties_destroy_shell, (XtPointer)map_properties_dialog);

        XtManageChild(rowcol1);
        XtManageChild(rowcol2);
        XtManageChild(rowcol3);
        XtManageChild(my_form);
        XtManageChild(map_properties_list);
        XtVaSetValues(map_properties_list, XmNbackground, colors[0x0f], NULL);
        XtManageChild(pane);

        XmTextSetString(new_map_layer_text, "0");

        XtPopup(map_properties_dialog,XtGrabNone);

        // Move focus to the OK button.  This appears to highlight the
        // button fine, but we're not able to hit the <Enter> key to
        // have that default function happen.  Note:  We _can_ hit the
        // <SPACE> key, and that activates the option.
//        XmUpdateDisplay(map_properties_dialog);
        XmProcessTraversal(button_close, XmTRAVERSE_CURRENT);

    } else {
        (void)XRaiseWindow(XtDisplay(map_properties_dialog), XtWindow(map_properties_dialog));
    }
}





/************************* Map Chooser ***********************************/
/*************************************************************************/

// Destroys the Map Chooser dialog
void map_chooser_destroy_shell( /*@unused@*/ Widget widget, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    Widget shell = (Widget) clientData;
    XtPopdown(shell);
    XtDestroyWidget(shell);
    map_chooser_dialog = (Widget)NULL;
}





// Update the "selected" field in the in-memory map_index based on
// the "selected" input parameter.
void map_index_update_selected(char *filename, int selected, map_index_record **current) {

    // If we're passed a NULL pointer, start at the head of the
    // in-memory linked list.
    //
    if ( (*current) == NULL) {
        (*current) = map_index_head;
    }

    // Start searching through the list at the pointer we were
    // given.
    //
    while ( (*current) != NULL) {
        if (strcmp( (*current)->filename,filename) == 0) {
            // Found a match.  Update the field and return.
            (*current)->selected = selected;
            return;
        }
        (*current) = (*current)->next;
    }
}





// Update the "temp_select" field in the in-memory map_index.
void map_index_update_temp_select(char *filename, map_index_record **current) {
    int result;

    // If we're passed a NULL pointer, start at the head of the
    // in-memory linked list.
    //
    if ( (*current) == NULL) {
        (*current) = map_index_head;
    }

    // Start searching through the list at the pointer we were
    // given.  We need to do a loose match here for directories.  If
    // a selected directory is contained in a filepath, select that
    // file as well.  For the directory case, once we find a match
    // in the file path, keep walking down the list until we get a
    // non-match.
    //
    while ( (*current) != NULL) {

        result = strncmp( (*current)->filename,filename,strlen(filename));

        if (result == 0) {
            // Found a match.  Update the field.
            (*current)->temp_select = 1;
        }
        else if (result > 0) {  // We passsed the relevant area.
                                // All done for now.
            return;
        }
        (*current) = (*current)->next;
    }
}





// Clear all of the temp_select bits in the in-memory map index
void map_index_temp_select_clear(void) {
    map_index_record *current;

    current = map_index_head;
    while (current != NULL) {
        current->temp_select = 0;
        current = current->next;
    }
}





// Gets the list of selected maps out of the dialog, writes them to
// the selected maps disk file, destroys the dialog, then calls
// create_image() with the newly selected map set in place.  This
// should be the _only_ routine in this set of functions that
// actually changes the selected maps disk file.  The others should
// merely manipulate the list in the map chooser dialog.  This
// function is attached to the "OK" button in the Map Chooser dialog.
//
// What we'll do here is set/reset the "selected" field in the
// in-memory map_index list, then write the info out to the
// selected_maps.sys file.  Only set the file entries if in file
// mode, dir entries if in dir mode.  When writing out to file,
// write them both out.
//
// In order to make this fast, we'll send a start pointer to
// map_index_update_selected() which is the "next" pointer from the
// previous hit.  We're relying on the fact that the Map Chooser
// list and the in-memory linked list are in the same search order,
// so this way we don't search through the entire linked list for
// each update.  With 30,000 maps, it ended up being up to 30,000 *
// 30,000 for the loop iterations, which was unwieldy.
//
void map_chooser_select_maps(Widget widget, XtPointer clientData, XtPointer callData) {
    int i, x;
    char *temp;
    XmString *list;
    FILE *f;
    map_index_record *ptr = map_index_head;
   

// It'd be nice to turn off auto-maps here, or better perhaps would
// be if any button were chosen other than "Cancel".


    // reset map_refresh in case we no longer have a refreshed map selected
    map_refresh_interval = 0;

    // Cause load_maps() and load_automaps() to re-sort the selected
    // maps by layer.
    re_sort_maps = 1;

    // Get the list and the list count from the dialog
    XtVaGetValues(map_list,
               XmNitemCount,&i,
               XmNitems,&list,
               NULL);

    // Run through the list, updating the equivalent entries in the
    // in-memory map index.  If we're in "directory" mode we'll only
    // update the directory entries.  In "Expanded dirs" mode, we'll
    // update both file and directory entries.
    // The end result is that both directories and files may be
    // selected, not either/or as the code was written earlier.
    //
    // Here we basically walk both lists together, the List widget
    // and the in-memory linked list, as they're both in the same
    // sort order.  We do this by passing "ptr" back and forth, and
    // updating it to point to one after the last one found each
    // time.  That turns and N*N search into an N search and is a
    // big speed improvement when you have 1000's of maps.
    //
    for(x=1; x<=i;x++) {
        if (XmStringGetLtoR(list[(x-1)],XmFONTLIST_DEFAULT_TAG,&temp)) {
            // Update this file or directory in the in-memory map
            // index, setting/resetting the "selected" field as
            // appropriate.
            map_index_update_selected(temp,
                XmListPosSelected(map_list,x),
                &ptr);
            XtFree(temp);
        }
//fprintf(stderr,"Passed back: %s\n", ptr->filename);
        ptr = ptr->next;
    }

    // Now we have all of the updates done to the in-memory map
    // index.  Write out the selected maps to disk, overwriting
    // whatever was there before.

    ptr = map_index_head;

    f=fopen( get_user_base_dir(SELECTED_MAP_DATA), "w+" );
    if (f!=NULL) {

        while (ptr != NULL) {
            // Write only selected files/directories out to the disk
            // file.
            if (ptr->selected) {
                fprintf(f,"%s\n",ptr->filename);
            }
            ptr = ptr->next;
        }
        (void)fclose(f);
    }
    else
        fprintf(stderr,"Couldn't open file: %s\n", get_user_base_dir(SELECTED_MAP_DATA) );

    map_chooser_destroy_shell(widget,clientData,callData);

    // Set interrupt_drawing_now because conditions have changed.
    interrupt_drawing_now++;

    // Request that a new image be created.  Calls create_image,
    // XCopyArea, and display_zoom_status.
    request_new_image++;

//    if (create_image(da)) {
//        (void)XCopyArea(XtDisplay(da),pixmap_final,XtWindow(da),gc,0,0,(unsigned int)screen_width,(unsigned int)screen_height,0,0;
//    }
}





// Same as map_chooser_select_maps, but doesn't destroy the Map
// Chooser dialog.
void map_chooser_apply_maps(Widget widget, XtPointer clientData, XtPointer callData) {
    int i, x;
    char *temp;
    XmString *list;
    FILE *f;
    map_index_record *ptr = map_index_head;
   

// It'd be nice to turn off auto-maps here, or better perhaps would
// be if any button were chosen other than "Cancel".


    // reset map_refresh in case we no longer have a refreshed map selected
    map_refresh_interval = 0;

    // Cause load_maps() and load_automaps() to re-sort the selected
    // maps by layer.
    re_sort_maps = 1;

    // Get the list and the list count from the dialog
    XtVaGetValues(map_list,
               XmNitemCount,&i,
               XmNitems,&list,
               NULL);

    // Run through the list, updating the equivalent entries in the
    // in-memory map index.  If we're in "directory" mode we'll only
    // update the directory entries.  In "Expanded dirs" mode, we'll
    // update both file and directory entries.
    // The end result is that both directories and files may be
    // selected, not either/or as the code was written earlier.
    //
    // Here we basically walk both lists together, the List widget
    // and the in-memory linked list, as they're both in the same
    // sort order.  We do this by passing "ptr" back and forth, and
    // updating it to point to one after the last one found each
    // time.  That turns and N*N search into an N search and is a
    // big speed improvement when you have 1000's of maps.
    //
    for(x=1; x<=i;x++) {
        if (XmStringGetLtoR(list[(x-1)],XmFONTLIST_DEFAULT_TAG,&temp)) {
            // Update this file or directory in the in-memory map
            // index, setting/resetting the "selected" field as
            // appropriate.
            map_index_update_selected(temp,
                XmListPosSelected(map_list,x),
                &ptr);
            XtFree(temp);
        }
//fprintf(stderr,"Passed back: %s\n", ptr->filename);
        ptr = ptr->next;
    }

    // Now we have all of the updates done to the in-memory map
    // index.  Write out the selected maps to disk, overwriting
    // whatever was there before.

    ptr = map_index_head;

    f=fopen( get_user_base_dir(SELECTED_MAP_DATA), "w+" );
    if (f!=NULL) {

        while (ptr != NULL) {
            // Write only selected files/directories out to the disk
            // file.
            if (ptr->selected) {
                fprintf(f,"%s\n",ptr->filename);
            }
            ptr = ptr->next;
        }
        (void)fclose(f);
    }
    else
        fprintf(stderr,"Couldn't open file: %s\n", get_user_base_dir(SELECTED_MAP_DATA) );

//    map_chooser_destroy_shell(widget,clientData,callData);

    // Set interrupt_drawing_now because conditions have changed.
    interrupt_drawing_now++;

    // Request that a new image be created.  Calls create_image,
    // XCopyArea, and display_zoom_status.
    request_new_image++;

//    if (create_image(da)) {
//        (void)XCopyArea(XtDisplay(da),pixmap_final,XtWindow(da),gc,0,0,(unsigned int)screen_width,(unsigned int)screen_height,0,0);
//    }
}





// Counts the number of "selected" fields with a value of 1 in the
// in-memory map index.  Updates the "Dirs/Maps Selected" count in
// the map chooser.
void map_chooser_update_quantity(void) {
    int dir_quantity = 0;
    int map_quantity = 0;
    static char str_quantity[100];
    map_index_record *current = map_index_head;
    XmString x_str;

    // Count the "selected" fields in the map index with value of 1
    while (current != NULL) {
        if (current->selected) {

            if (current->filename[strlen(current->filename)-1] == '/') {
                // It's a directory
                dir_quantity++;
            }
            else {
                // It's a map
                map_quantity++;
            }
        }
        current = current->next;
    }
   
    // Update the "Dirs/Maps Selected" label in the Map Chooser
    xastir_snprintf(str_quantity,
        sizeof(str_quantity),
        "%d/%d",
        dir_quantity,
        map_quantity);
    x_str = XmStringCreateLocalized(str_quantity);
    XtVaSetValues(map_chooser_maps_selected_data,
        XmNlabelString, x_str,
        NULL);
    XmStringFree(x_str);
}
 
 



void map_chooser_select_vector_maps(Widget widget, XtPointer clientData, XtPointer callData) {
    int i, x;
    char *temp;
    char *ext;
    XmString *list;

    // Get the list and the count from the dialog
    XtVaGetValues(map_list,
               XmNitemCount,&i,
               XmNitems,&list,
               NULL);

    // Run through the list looking for matching file extensions
    for(x=1; x<=i;x++) {

//        // Deselect all currently selected maps
//        if (XmListPosSelected(map_list,x)) {
//            XmListDeselectPos(map_list,x);
//        }

        if(XmStringGetLtoR(list[(x-1)],XmFONTLIST_DEFAULT_TAG,&temp)) {
            ext = get_map_ext (temp);
            if ( (ext != NULL)
                    && (   (strcasecmp(ext,"map") == 0)
                        || (strcasecmp(ext,"shp") == 0)
                        || (strcasecmp(ext,"pdb") == 0)
                        || (strcasecmp(ext,"gnis") == 0)
                        || (strcasecmp(ext,"rt1") == 0)
                        || (strcasecmp(ext,"rt2") == 0)
                        || (strcasecmp(ext,"rt4") == 0)
                        || (strcasecmp(ext,"rt5") == 0)
                        || (strcasecmp(ext,"rt6") == 0)
                        || (strcasecmp(ext,"rt7") == 0)
                        || (strcasecmp(ext,"rt8") == 0)
                        || (strcasecmp(ext,"rta") == 0)
                        || (strcasecmp(ext,"rtc") == 0)
                        || (strcasecmp(ext,"rth") == 0)
                        || (strcasecmp(ext,"rti") == 0)
                        || (strcasecmp(ext,"rtp") == 0)
                        || (strcasecmp(ext,"rtr") == 0)
                        || (strcasecmp(ext,"rts") == 0)
                        || (strcasecmp(ext,"rtt") == 0)
                        || (strcasecmp(ext,"rtz") == 0)
                        || (strcasecmp(ext,"tab") == 0) ) ) {
                XmListSelectPos(map_list,x,TRUE);
            }
            XtFree(temp);
        }
    }

    map_chooser_update_quantity();
}





void map_chooser_select_250k_maps(Widget widget, XtPointer clientData, XtPointer callData) {
    int i, x, length;
    char *temp;
    char *ext;
    XmString *list;

    // Get the list and the count from the dialog
    XtVaGetValues(map_list,
               XmNitemCount,&i,
               XmNitems,&list,
               NULL);

    // Run through the list looking for matching file extensions
    for(x=1; x<=i;x++) {

//        // Deselect all currently selected maps
//        if (XmListPosSelected(map_list,x)) {
//            XmListDeselectPos(map_list,x);
//        }

        if(XmStringGetLtoR(list[(x-1)],XmFONTLIST_DEFAULT_TAG,&temp)) {
            ext = get_map_ext (temp);
            length = (int)strlen(temp);
            if ( (ext != NULL) && (strcasecmp (ext, "tif") == 0)
                    && (length >= 12)   // "o48122h3.tif", we might have subdirectories also
                    && ( (temp[length - 12] == 'c') || (temp[length - 12] == 'C') ) ) {
                XmListSelectPos(map_list,x,TRUE);
            }
            XtFree(temp);
        }
    }

    map_chooser_update_quantity();
}





void map_chooser_select_100k_maps(Widget widget, XtPointer clientData, XtPointer callData) {
    int i, x, length;
    char *temp;
    char *ext;
    XmString *list;

    // Get the list and the count from the dialog
    XtVaGetValues(map_list,
               XmNitemCount,&i,
               XmNitems,&list,
               NULL);

    // Run through the list looking for matching file extensions
    for(x=1; x<=i;x++) {

//        // Deselect all currently selected maps
//        if (XmListPosSelected(map_list,x)) {
//            XmListDeselectPos(map_list,x);
//        }

        if(XmStringGetLtoR(list[(x-1)],XmFONTLIST_DEFAULT_TAG,&temp)) {
            ext = get_map_ext (temp);
            length = (int)strlen(temp);
            if ( (ext != NULL) && (strcasecmp (ext, "tif") == 0)
                    && (length >= 12)   // "o48122h3.tif", we might have subdirectories also
                    && ( (temp[length - 12] == 'f') || (temp[length - 12] == 'F') ) ) {
                XmListSelectPos(map_list,x,TRUE);
            }
            XtFree(temp);
        }
    }

    map_chooser_update_quantity();
}





void map_chooser_select_24k_maps(Widget widget, XtPointer clientData, XtPointer callData) {
    int i, x, length;
    char *temp;
    char *ext;
    XmString *list;

    // Get the list and the count from the dialog
    XtVaGetValues(map_list,
               XmNitemCount,&i,
               XmNitems,&list,
               NULL);

    // Run through the list looking for matching file extensions
    for(x=1; x<=i;x++) {

//        // Deselect all currently selected maps
//        if (XmListPosSelected(map_list,x)) {
//            XmListDeselectPos(map_list,x);
//        }

        if(XmStringGetLtoR(list[(x-1)],XmFONTLIST_DEFAULT_TAG,&temp)) {
            ext = get_map_ext (temp);
            length = (int)strlen(temp);
            if ( (ext != NULL) && (strcasecmp (ext, "tif") == 0)
                        && (length >= 12)   // "o48122h3.tif", we might have subdirectories also
                        && ( (temp[length - 12] == 'o') || (temp[length - 12] == 'O')
                            || (temp[length - 12] == 'k') || (temp[length - 12] == 'K') ) ) {
                XmListSelectPos(map_list,x,TRUE);
            }
            XtFree(temp);
        }
    }

    map_chooser_update_quantity();
}





// Removes the highlighting for maps in the current view of the map
// chooser.  In order to de-select all maps, must flip through both
// map chooser views and hit the "none" button each time, then hit
// the "ok" button.
//
// Changed the code to clear all of the "selected" bits in the
// in-memory map index as well.  The "None" and "OK" buttons take
// immediate effect, all others do not (until the "OK" button is
// pressed).  Decided that this was too inconsistent, so changed it
// back and changed "None" to "Clear", which means to clear the
// currently seen selections, but not the selections in the other
// mode.
//
void map_chooser_deselect_maps(Widget widget, XtPointer clientData, XtPointer callData) {
    int i, x;
    XmString *list;
//    map_index_record *current = map_index_head;

    // Get the list and the count from the dialog
    XtVaGetValues(map_list,
               XmNitemCount,&i,
               XmNitems,&list,
               NULL);

    // Run through the widget's list, deselecting every line
    for(x=1; x<=i;x++)
    {
        if (XmListPosSelected(map_list,x)) {
            XmListDeselectPos(map_list,x);
        }
    }

/*
    // Run through the in-memory map list, deselecting every line
    while (current != NULL) {
        current->selected = 0;    // Not Selected
        current = current->next;
    }
*/

    map_chooser_update_quantity();
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
        fprintf(stderr,"Couldn't open file: %s\n", ptr_filename);


    if(f_data!=NULL)
        (void)fclose(f_data);
    else
        fprintf(stderr,"Couldn't open file: %s\n", filename);
}





// Mark the "selected" field in the in-memory map index based on the
// contents of the selected_maps.sys file.  Called from main() right
// after map_indexer() is called on startup.
void map_chooser_init (void) {
    FILE *f;
    char temp[600];
    map_index_record *current;


    busy_cursor(appshell);

    // First run through our in-memory map index, clearing all of
    // the selected bits.
    current = map_index_head;
    while (current != NULL) {
        current->selected = 0;
        current = current->next;
    }

    (void)filecreate( get_user_base_dir(SELECTED_MAP_DATA) );   // Create empty file if it doesn't exist

    f=fopen( get_user_base_dir(SELECTED_MAP_DATA), "r" );
    if (f!=NULL) {
        while(!feof(f)) {
            int done;

            (void)get_line(f,temp,600);

            // We have a line from the file.  Find the matching line
            // in the in-memory map index.
            current = map_index_head;
            done = 0;
            while (current != NULL && !done) {
                //fprintf(stderr,"%s\n",current->filename);

                if (strcmp(temp,current->filename) == 0) {
                    current->selected = 1;
                    done++;
                }
                current = current->next;
            }
        }
        (void)fclose(f);
    }
    else {
        fprintf(stderr,"Couldn't open file: %s\n", get_user_base_dir(SELECTED_MAP_DATA) );
    }
}





// Fills in the map chooser file/directory entries based on the
// current view and whether the "selected" field in the in-memory
// map_index is set for each file/directory.
//
// We also check the XmStringPtr field in the map index records.  If
// NULL, then we call XmStringCreateLtoR() to allocate and fill in
// the XmString value corresponding to the filename.  We use that to
// speed up Map Chooser later.
//
void map_chooser_fill_in (void) {
    int n,i;
    map_index_record *current = map_index_head;


    busy_cursor(appshell);

    i=0;
    if (map_chooser_dialog) {

        // Empty the map_list widget first
        XmListDeleteAllItems(map_list);

        // Put all the map files/dirs in the map_index into the Map
        // Chooser dialog list (map_list).
        n=1;

        while (current != NULL) {

            //fprintf(stderr,"%s\n",current->filename);

            // Check whether we're supposed to show dirs and files or
            // just dirs.  Directories are always shown.
            if (map_chooser_expand_dirs // Show all
                    || current->filename[strlen(current->filename)-1] == '/') {


// Try XmListAddItems() here?  Could also create XmString's for each
// filename and keep them in the map index.  Then we wouldn't have to
// free that malloc/free that storage space all the time.
// XmListAddItems()
// XmListAddItemsUnselected()
// XmListReplaceItems()
// XmListReplaceItemsUnselected()


                // If pointer is NULL, malloc and create the
                // XmString corresponding to the filename, attach it
                // to the record.  The 2nd and succeeding times we
                // bring up Map Chooser, things will be faster.
                if (current->XmStringPtr == NULL) {
                    current->XmStringPtr = XmStringCreateLtoR(current->filename,
                        XmFONTLIST_DEFAULT_TAG);
                }

                XmListAddItem(map_list,
                    current->XmStringPtr,
                    n);

                // If a selected map, hilight it in the list
                if (current->selected) {
                    XmListSelectPos(map_list,i,TRUE);
                }
 
                n++;
            }
            current = current->next;
        }
    }
}





///////////////////////////////////////  Configure Tigermaps Dialog //////////////////////////////////////////////
//N0VH
#if defined(HAVE_MAGICK)



void Configure_tiger_destroy_shell( /*@unused@*/ Widget widget, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    Widget shell = (Widget) clientData;

    XtPopdown(shell);
    XtDestroyWidget(shell);
    configure_tiger_dialog = (Widget)NULL;
}





void Configure_tiger_change_data(Widget widget, XtPointer clientData, XtPointer callData) {

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

    Configure_tiger_destroy_shell(widget,clientData,callData);

    // Reload maps
    // Set interrupt_drawing_now because conditions have
    // changed.
    interrupt_drawing_now++;

    // Request that a new image be created.  Calls
    // create_image,
    // XCopyArea, and display_zoom_status.
    request_new_image++;

//    if (create_image(da)) {
//        (void)XCopyArea(XtDisplay(da),pixmap_final,XtWindow(da),gc,0,0,(unsigned int)screen_width,(unsigned int)screen_height,0,0);
//    }
}





////////////////////////////////////////////// Config_tiger//////////////////////////////////////////////////////
//
//
void Config_tiger( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer callData) {
    static Widget tiger_pane, tiger_form, button_ok, button_cancel, tiger_label1, sep;

    Atom delw;

    if (!configure_tiger_dialog) {

        configure_tiger_dialog = XtVaCreatePopupShell(langcode("PULDNMP020"),
                xmDialogShellWidgetClass, appshell,
                XmNdeleteResponse,XmDESTROY,
                XmNdefaultPosition, FALSE,
                XmNfontList, fontlist1,
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
                XmNfractionBase, 3,
                XmNautoUnmanage, FALSE,
                XmNshadowThickness, 1,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        tiger_label1  = XtVaCreateManagedWidget(langcode("MPUPTGR012"),
                xmLabelWidgetClass,
                tiger_form,
                XmNtopAttachment, XmATTACH_FORM,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 0,
                XmNleftOffset ,10,
                XmNrightAttachment, XmATTACH_NONE,
                XmNsensitive, TRUE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
                NULL);

        tiger_counties  = XtVaCreateManagedWidget(langcode("MPUPTGR002"),
                xmToggleButtonWidgetClass,
                tiger_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, tiger_label1,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 1,
                XmNrightAttachment, XmATTACH_NONE,
                XmNsensitive, TRUE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        tiger_states  = XtVaCreateManagedWidget(langcode("MPUPTGR008"),
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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
                NULL);

        tiger_places  = XtVaCreateManagedWidget(langcode("MPUPTGR004"),
                xmToggleButtonWidgetClass,
                tiger_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, tiger_grid,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 1,
                XmNrightAttachment, XmATTACH_NONE,
                XmNsensitive, TRUE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        tiger_interstate  = XtVaCreateManagedWidget(langcode("MPUPTGR009"),
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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
                NULL);

        tiger_streets  = XtVaCreateManagedWidget(langcode("MPUPTGR006"),
                xmToggleButtonWidgetClass,
                tiger_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, tiger_cities,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 1,
                XmNrightAttachment, XmATTACH_NONE,
                XmNsensitive, TRUE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        tiger_ushwy  = XtVaCreateManagedWidget(langcode("MPUPTGR010"),
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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
                NULL);

        tiger_water  = XtVaCreateManagedWidget(langcode("MPUPTGR013"),
                xmToggleButtonWidgetClass,
                tiger_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, tiger_majroads,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 1,
                XmNrightAttachment, XmATTACH_NONE,
                XmNsensitive, TRUE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        tiger_statehwy  = XtVaCreateManagedWidget(langcode("MPUPTGR011"),
                xmToggleButtonWidgetClass,
                tiger_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, tiger_majroads,
                XmNtopOffset, 4,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 2,
                XmNrightAttachment, XmATTACH_NONE,
                XmNsensitive, TRUE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
                NULL);

       tiger_misc  = XtVaCreateManagedWidget(langcode("MPUPTGR015"),
                xmToggleButtonWidgetClass,
                tiger_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, tiger_railroad,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 1,
                XmNrightAttachment, XmATTACH_NONE,
                XmNsensitive, TRUE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        sep = XtVaCreateManagedWidget("Config Tigermap sep", 
                xmSeparatorGadgetClass,
                tiger_form,
                XmNorientation, XmHORIZONTAL,
                XmNtopAttachment,XmATTACH_WIDGET,
                XmNtopWidget, tiger_lakes,
                XmNtopOffset, 10,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNrightAttachment,XmATTACH_FORM,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        button_ok = XtVaCreateManagedWidget(langcode("UNIOP00001"),
                xmPushButtonGadgetClass, 
                tiger_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, sep,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_FORM,
                XmNbottomOffset, 5,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 0,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 1,
                XmNrightOffset, 0,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        button_cancel = XtVaCreateManagedWidget(langcode("UNIOP00002"),
                xmPushButtonGadgetClass, 
                tiger_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, sep,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_FORM,
                XmNbottomOffset, 5,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 2,
                XmNleftOffset, 0,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 3,
                XmNrightOffset, 10,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        XtAddCallback(button_ok, XmNactivateCallback, Configure_tiger_change_data, configure_tiger_dialog);

        XtAddCallback(button_cancel, XmNactivateCallback, Configure_tiger_destroy_shell, configure_tiger_dialog);

        pos_dialog(configure_tiger_dialog);

        delw = XmInternAtom(XtDisplay(configure_tiger_dialog),"WM_DELETE_WINDOW", FALSE);
        XmAddWMProtocolCallback(configure_tiger_dialog, delw, Configure_tiger_destroy_shell,
                (XtPointer)configure_tiger_dialog);

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
#endif // HAVE_MAGICK
///////////////////////// End of Configure Tiger code ///////////////////////////////////





///////////////////////////////////////  Configure DRG Dialog //////////////////////////////////////////////
#if defined(HAVE_LIBGEOTIFF)
void Configure_DRG_destroy_shell( /*@unused@*/ Widget widget, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    Widget shell = (Widget) clientData;

    if (configure_DRG_dialog) {
        XtPopdown(shell);
        XtDestroyWidget(shell);
        configure_DRG_dialog = (Widget)NULL;
    }
}





void Configure_DRG_change_data(Widget widget, XtPointer clientData, XtPointer callData) {

    if (configure_DRG_dialog) {
 
        if(XmToggleButtonGetState(DRG_XOR))
            DRG_XOR_colors=TRUE;
        else
            DRG_XOR_colors=FALSE;

        if(XmToggleButtonGetState(DRG_color0))
            DRG_show_colors[0]=TRUE;
        else
            DRG_show_colors[0]=FALSE;

        if(XmToggleButtonGetState(DRG_color1))
            DRG_show_colors[1]=TRUE;
        else
            DRG_show_colors[1]=FALSE;

        if(XmToggleButtonGetState(DRG_color2))
            DRG_show_colors[2]=TRUE;
        else
            DRG_show_colors[2]=FALSE;

        if(XmToggleButtonGetState(DRG_color3))
            DRG_show_colors[3]=TRUE;
        else
            DRG_show_colors[3]=FALSE;

        if(XmToggleButtonGetState(DRG_color4))
            DRG_show_colors[4]=TRUE;
        else
            DRG_show_colors[4]=FALSE;

        if(XmToggleButtonGetState(DRG_color5))
            DRG_show_colors[5]=TRUE;
        else
            DRG_show_colors[5]=FALSE;

        if(XmToggleButtonGetState(DRG_color6))
            DRG_show_colors[6]=TRUE;
        else
            DRG_show_colors[6]=FALSE;

        if(XmToggleButtonGetState(DRG_color7))
            DRG_show_colors[7]=TRUE;
        else
            DRG_show_colors[7]=FALSE;

        if(XmToggleButtonGetState(DRG_color8))
            DRG_show_colors[8]=TRUE;
        else
            DRG_show_colors[8]=FALSE;

        if(XmToggleButtonGetState(DRG_color9))
            DRG_show_colors[9]=TRUE;
        else
            DRG_show_colors[9]=FALSE;

        if(XmToggleButtonGetState(DRG_color10))
            DRG_show_colors[10]=TRUE;
        else
            DRG_show_colors[10]=FALSE;

        if(XmToggleButtonGetState(DRG_color11))
            DRG_show_colors[11]=TRUE;
        else
            DRG_show_colors[11]=FALSE;

        if(XmToggleButtonGetState(DRG_color12))
            DRG_show_colors[12]=TRUE;
        else
            DRG_show_colors[12]=FALSE;

        Configure_DRG_destroy_shell(widget,clientData,callData);

        // Reload maps
        // Set interrupt_drawing_now because conditions have
        // changed.
        interrupt_drawing_now++;

        // Request that a new image be created.  Calls
        // create_image,
        // XCopyArea, and display_zoom_status.
        request_new_image++;

    //    if (create_image(da)) {
    //        (void)XCopyArea(XtDisplay(da),pixmap_final,XtWindow(da),gc,0,0,(unsigned int)screen_width,(unsigned int)screen_height,0,0);
    //    }

    }
}





void Configure_DRG_all(Widget widget, XtPointer clientData, XtPointer callData) {

    if (configure_DRG_dialog) {
        XmToggleButtonSetState(DRG_color0,TRUE,FALSE);
        XmToggleButtonSetState(DRG_color1,TRUE,FALSE);
        XmToggleButtonSetState(DRG_color2,TRUE,FALSE);
        XmToggleButtonSetState(DRG_color3,TRUE,FALSE);
        XmToggleButtonSetState(DRG_color4,TRUE,FALSE);
        XmToggleButtonSetState(DRG_color5,TRUE,FALSE);
        XmToggleButtonSetState(DRG_color6,TRUE,FALSE);
        XmToggleButtonSetState(DRG_color7,TRUE,FALSE);
        XmToggleButtonSetState(DRG_color8,TRUE,FALSE);
        XmToggleButtonSetState(DRG_color9,TRUE,FALSE);
        XmToggleButtonSetState(DRG_color10,TRUE,FALSE);
        XmToggleButtonSetState(DRG_color11,TRUE,FALSE);
        XmToggleButtonSetState(DRG_color12,TRUE,FALSE);
    }
}





void Configure_DRG_none(Widget widget, XtPointer clientData, XtPointer callData) {

    if (configure_DRG_dialog) {
        XmToggleButtonSetState(DRG_color0,FALSE,FALSE);
        XmToggleButtonSetState(DRG_color1,FALSE,FALSE);
        XmToggleButtonSetState(DRG_color2,FALSE,FALSE);
        XmToggleButtonSetState(DRG_color3,FALSE,FALSE);
        XmToggleButtonSetState(DRG_color4,FALSE,FALSE);
        XmToggleButtonSetState(DRG_color5,FALSE,FALSE);
        XmToggleButtonSetState(DRG_color6,FALSE,FALSE);
        XmToggleButtonSetState(DRG_color7,FALSE,FALSE);
        XmToggleButtonSetState(DRG_color8,FALSE,FALSE);
        XmToggleButtonSetState(DRG_color9,FALSE,FALSE);
        XmToggleButtonSetState(DRG_color10,FALSE,FALSE);
        XmToggleButtonSetState(DRG_color11,FALSE,FALSE);
        XmToggleButtonSetState(DRG_color12,FALSE,FALSE);
    }
}





void Config_DRG( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer callData) {
    static Widget DRG_pane, DRG_form, button_ok, button_cancel,
        DRG_label1, sep1, sep2, button_all, button_none;

    Atom delw;

    if (!configure_DRG_dialog) {

        configure_DRG_dialog = XtVaCreatePopupShell(langcode("PULDNMP030"),
                xmDialogShellWidgetClass, appshell,
                XmNdeleteResponse,XmDESTROY,
                XmNdefaultPosition, FALSE,
                XmNfontList, fontlist1,
                NULL);

        DRG_pane = XtVaCreateWidget("Configure_DRG pane",
                xmPanedWindowWidgetClass, 
                configure_DRG_dialog,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        DRG_form =  XtVaCreateWidget("Configure_DRG DRG_form",
                xmFormWidgetClass, 
                DRG_pane,
                XmNfractionBase, 3,
                XmNautoUnmanage, FALSE,
                XmNshadowThickness, 1,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        DRG_XOR  = XtVaCreateManagedWidget(langcode("MPUPDRG002"),
                xmToggleButtonWidgetClass,
                DRG_form,
                XmNtopAttachment, XmATTACH_FORM,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 0,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_NONE,
                XmNsensitive, TRUE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        sep1 = XtVaCreateManagedWidget("Config Tigermap sep1", 
                xmSeparatorGadgetClass,
                DRG_form,
                XmNorientation, XmHORIZONTAL,
                XmNtopAttachment,XmATTACH_WIDGET,
                XmNtopWidget, DRG_XOR,
                XmNtopOffset, 10,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNrightAttachment,XmATTACH_FORM,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        DRG_label1  = XtVaCreateManagedWidget(langcode("MPUPDRG001"),
                xmLabelWidgetClass,
                DRG_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, sep1,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 0,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_NONE,
                XmNsensitive, TRUE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);


// Column 1
        // Black
        DRG_color0  = XtVaCreateManagedWidget(langcode("MPUPDRG003"),
                xmToggleButtonWidgetClass,
                DRG_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, DRG_label1,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 0,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_NONE,
                XmNsensitive, TRUE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        // Blue
        DRG_color2  = XtVaCreateManagedWidget(langcode("MPUPDRG005"),
                xmToggleButtonWidgetClass,
                DRG_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, DRG_color0,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 0,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_NONE,
                XmNsensitive, TRUE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        // Red
        DRG_color3  = XtVaCreateManagedWidget(langcode("MPUPDRG006"),
                xmToggleButtonWidgetClass,
                DRG_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, DRG_color2,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 0,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_NONE,
                XmNsensitive, TRUE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        // Brown
        DRG_color4  = XtVaCreateManagedWidget(langcode("MPUPDRG007"),
                xmToggleButtonWidgetClass,
                DRG_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, DRG_color3,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 0,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_NONE,
                XmNsensitive, TRUE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        // Purple
        DRG_color6  = XtVaCreateManagedWidget(langcode("MPUPDRG009"),
                xmToggleButtonWidgetClass,
                DRG_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, DRG_color4,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 0,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_NONE,
                XmNsensitive, TRUE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

// Column 2
         // Light Gray
        DRG_color11  = XtVaCreateManagedWidget(langcode("MPUPDRG014"),
                xmToggleButtonWidgetClass,
                DRG_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, DRG_label1,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 1,
                XmNrightAttachment, XmATTACH_NONE,
                XmNsensitive, TRUE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        // Light Blue
        DRG_color8  = XtVaCreateManagedWidget(langcode("MPUPDRG011"),
                xmToggleButtonWidgetClass,
                DRG_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, DRG_color11,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 1,
                XmNrightAttachment, XmATTACH_NONE,
                XmNsensitive, TRUE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        // Light Red
        DRG_color9  = XtVaCreateManagedWidget(langcode("MPUPDRG012"),
                xmToggleButtonWidgetClass,
                DRG_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, DRG_color8,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 1,
                XmNrightAttachment, XmATTACH_NONE,
                XmNsensitive, TRUE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        // Light Brown
        DRG_color12  = XtVaCreateManagedWidget(langcode("MPUPDRG015"),
                xmToggleButtonWidgetClass,
                DRG_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, DRG_color9,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 1,
                XmNrightAttachment, XmATTACH_NONE,
                XmNsensitive, TRUE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        // Light Purple
        DRG_color10  = XtVaCreateManagedWidget(langcode("MPUPDRG013"),
                xmToggleButtonWidgetClass,
                DRG_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, DRG_color12,
                XmNtopOffset, 4,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 1,
                XmNrightAttachment, XmATTACH_NONE,
                XmNsensitive, TRUE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

// Column 3
        // White
        DRG_color1  = XtVaCreateManagedWidget(langcode("MPUPDRG004"),
                xmToggleButtonWidgetClass,
                DRG_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, DRG_label1,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 2,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_NONE,
                XmNsensitive, TRUE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        // Green
        DRG_color5  = XtVaCreateManagedWidget(langcode("MPUPDRG008"),
                xmToggleButtonWidgetClass,
                DRG_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, DRG_color1,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 2,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_NONE,
                XmNsensitive, TRUE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        // Yellow
        DRG_color7  = XtVaCreateManagedWidget(langcode("MPUPDRG010"),
                xmToggleButtonWidgetClass,
                DRG_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, DRG_color5,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 2,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_NONE,
                XmNsensitive, TRUE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        sep2 = XtVaCreateManagedWidget("Config Tigermap sep2", 
                xmSeparatorGadgetClass,
                DRG_form,
                XmNorientation, XmHORIZONTAL,
                XmNtopAttachment,XmATTACH_WIDGET,
                XmNtopWidget, DRG_color6,
                XmNtopOffset, 10,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNrightAttachment,XmATTACH_FORM,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        button_all = XtVaCreateManagedWidget(langcode("PULDNMMC09"),
                xmPushButtonGadgetClass, 
                DRG_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, sep2,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_FORM,
                XmNbottomOffset, 5,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 0,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_NONE,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        button_none = XtVaCreateManagedWidget(langcode("PULDNDP040"),
                xmPushButtonGadgetClass, 
                DRG_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, sep2,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_FORM,
                XmNbottomOffset, 5,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, button_all,
                XmNrightAttachment, XmATTACH_NONE,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        button_ok = XtVaCreateManagedWidget(langcode("UNIOP00001"),
                xmPushButtonGadgetClass, 
                DRG_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, sep2,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_FORM,
                XmNbottomOffset, 5,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, button_none,
                XmNrightAttachment, XmATTACH_NONE,
                XmNrightOffset, 10,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        button_cancel = XtVaCreateManagedWidget(langcode("UNIOP00002"),
                xmPushButtonGadgetClass, 
                DRG_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, sep2,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_FORM,
                XmNbottomOffset, 5,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, button_ok,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 3,
                XmNrightOffset, 10,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        XtAddCallback(button_all,
            XmNactivateCallback,
            Configure_DRG_all,
            configure_DRG_dialog);

        XtAddCallback(button_none,
            XmNactivateCallback,
            Configure_DRG_none,
            configure_DRG_dialog);

        XtAddCallback(button_ok,
            XmNactivateCallback,
            Configure_DRG_change_data,
            configure_DRG_dialog);

        XtAddCallback(button_cancel,
            XmNactivateCallback,
            Configure_DRG_destroy_shell,
            configure_DRG_dialog);

        pos_dialog(configure_DRG_dialog);

        delw = XmInternAtom(XtDisplay(configure_DRG_dialog),
            "WM_DELETE_WINDOW",
            FALSE);
        XmAddWMProtocolCallback(configure_DRG_dialog,
            delw,
            Configure_DRG_destroy_shell,
            (XtPointer)configure_DRG_dialog);

        if(DRG_XOR_colors)
            XmToggleButtonSetState(DRG_XOR,TRUE,FALSE);
        else
            XmToggleButtonSetState(DRG_XOR,FALSE,FALSE);

        if(DRG_show_colors[0])
            XmToggleButtonSetState(DRG_color0,TRUE,FALSE);
        else
            XmToggleButtonSetState(DRG_color0,FALSE,FALSE);

        if(DRG_show_colors[1])
            XmToggleButtonSetState(DRG_color1,TRUE,FALSE);
        else
            XmToggleButtonSetState(DRG_color1,FALSE,FALSE);

        if(DRG_show_colors[2])
            XmToggleButtonSetState(DRG_color2,TRUE,FALSE);
        else
            XmToggleButtonSetState(DRG_color2,FALSE,FALSE);

        if(DRG_show_colors[3])
            XmToggleButtonSetState(DRG_color3,TRUE,FALSE);
        else
            XmToggleButtonSetState(DRG_color3,FALSE,FALSE);

        if(DRG_show_colors[4])
            XmToggleButtonSetState(DRG_color4,TRUE,FALSE);
        else
            XmToggleButtonSetState(DRG_color4,FALSE,FALSE);

        if(DRG_show_colors[5])
            XmToggleButtonSetState(DRG_color5,TRUE,FALSE);
        else
            XmToggleButtonSetState(DRG_color5,FALSE,FALSE);

        if(DRG_show_colors[6])
            XmToggleButtonSetState(DRG_color6,TRUE,FALSE);
        else
            XmToggleButtonSetState(DRG_color6,FALSE,FALSE);

        if(DRG_show_colors[7])
            XmToggleButtonSetState(DRG_color7,TRUE,FALSE);
        else
            XmToggleButtonSetState(DRG_color7,FALSE,FALSE);

        if(DRG_show_colors[8])
            XmToggleButtonSetState(DRG_color8,TRUE,FALSE);
        else
            XmToggleButtonSetState(DRG_color8,FALSE,FALSE);

        if(DRG_show_colors[9])
            XmToggleButtonSetState(DRG_color9,TRUE,FALSE);
        else
            XmToggleButtonSetState(DRG_color9,FALSE,FALSE);

        if(DRG_show_colors[10])
            XmToggleButtonSetState(DRG_color10,TRUE,FALSE);
        else
            XmToggleButtonSetState(DRG_color10,FALSE,FALSE);

        if(DRG_show_colors[11])
            XmToggleButtonSetState(DRG_color11,TRUE,FALSE);
        else
            XmToggleButtonSetState(DRG_color11,FALSE,FALSE);

        if(DRG_show_colors[12])
            XmToggleButtonSetState(DRG_color12,TRUE,FALSE);
        else
            XmToggleButtonSetState(DRG_color12,FALSE,FALSE);

        XtManageChild(DRG_form);
        XtManageChild(DRG_pane);

        XtPopup(configure_DRG_dialog,XtGrabNone);
        fix_dialog_size(configure_DRG_dialog);

        XmProcessTraversal(button_ok, XmTRAVERSE_CURRENT);

    } else
        (void)XRaiseWindow(XtDisplay(configure_DRG_dialog),
            XtWindow(configure_DRG_dialog));
}
#endif // HAVE_LIBGEOTIFF
///////////////////////// End of Configure DRG code ///////////////////////////////////





void Expand_Dirs_toggle( /*@unused@*/ Widget w, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        map_chooser_expand_dirs = atoi(which);
    else
        map_chooser_expand_dirs = 0;

    // Kill/resurrect the Map Chooser so that the changes take
    // effect.
    map_chooser_destroy_shell( w, map_chooser_dialog, (XtPointer) NULL);
    Map_chooser( w, (XtPointer)NULL, (XtPointer) NULL);

    map_chooser_update_quantity();
}





void Map_chooser( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer callData) {
    static Widget  pane, my_form, button_clear, button_V,
            button_C, button_F, button_O,
            rowcol, expand_dirs_button, button_properties,
            maps_selected_label, button_apply;
    Atom delw;
    int i;
    Arg al[50];                    /* Arg List */
    register unsigned int ac = 0;           /* Arg Count */


    busy_cursor(appshell);

    i=0;
    if (!map_chooser_dialog) {
        map_chooser_dialog = XtVaCreatePopupShell(langcode("WPUPMCP001"),
                xmDialogShellWidgetClass, appshell,
                XmNdeleteResponse,XmDESTROY,
                XmNdefaultPosition, FALSE,
                XmNfontList, fontlist1,
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
        XtSetArg(al[ac], XmNfontList, fontlist1); ac++;

        map_list = XmCreateScrolledList(my_form,
                "Map_chooser list",
                al,
                ac);

        // Find the names of all the map files on disk and put them into map_list
        map_chooser_fill_in();

        expand_dirs_button = XtVaCreateManagedWidget(langcode("PULDNMMC06"),
                xmToggleButtonWidgetClass,
                my_form,
                XmNtopAttachment, XmATTACH_FORM,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset ,10,
                XmNrightAttachment, XmATTACH_NONE,
                XmNsensitive, TRUE,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(expand_dirs_button,XmNvalueChangedCallback,Expand_Dirs_toggle,"1");
        if(map_chooser_expand_dirs)
            XmToggleButtonSetState(expand_dirs_button,TRUE,FALSE);
        else
            XmToggleButtonSetState(expand_dirs_button,FALSE,FALSE);

        maps_selected_label = XtVaCreateManagedWidget(langcode("PULDNMMC07"),
                xmLabelWidgetClass,
                my_form,
                XmNtopAttachment,           XmATTACH_FORM,
                XmNtopOffset,               10,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              expand_dirs_button,
                XmNleftOffset,              15,
                XmNrightAttachment,         XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        map_chooser_maps_selected_data = XtVaCreateManagedWidget("0/0",
                xmLabelWidgetClass,
                my_form,
                XmNtopAttachment,           XmATTACH_FORM,
                XmNtopOffset,               10,
                XmNbottomAttachment,        XmATTACH_NONE,
                XmNleftAttachment,          XmATTACH_WIDGET,
                XmNleftWidget,              maps_selected_label,
                XmNleftOffset,              2,
                XmNrightAttachment,         XmATTACH_NONE,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        // Button for configuring properties
        button_properties = XtVaCreateManagedWidget(langcode("UNIOP00009"),
                xmPushButtonGadgetClass, 
                my_form,
                XmNtopAttachment, XmATTACH_FORM,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_NONE,
                XmNrightAttachment, XmATTACH_FORM,
                XmNrightOffset, 10,
                XmNsensitive, TRUE,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(button_properties, XmNactivateCallback, map_properties, map_chooser_dialog);
 
        // Attach a rowcolumn manager widget to my_form to handle all of the buttons
        rowcol = XtVaCreateManagedWidget("Map Chooser rowcol", 
                xmRowColumnWidgetClass, 
                my_form,
                XmNorientation, XmHORIZONTAL,
                XmNtopAttachment, XmATTACH_NONE,
                XmNbottomAttachment, XmATTACH_FORM,
                XmNleftAttachment, XmATTACH_FORM,
                XmNrightAttachment, XmATTACH_FORM,
                XmNkeyboardFocusPolicy, XmEXPLICIT,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        XtVaSetValues(XtParent(map_list),
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, expand_dirs_button,
                XmNtopOffset, 2,
                XmNbottomAttachment, XmATTACH_WIDGET,
                XmNbottomWidget, rowcol,
                XmNbottomOffset, 2,
                XmNrightAttachment, XmATTACH_FORM,
                XmNrightOffset, 5,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 5,
                XmNfontList, fontlist1,
                NULL);

// "Clear"
        if(map_chooser_expand_dirs) {   // "Clear"
            button_clear = XtVaCreateManagedWidget(langcode("PULDNMMC01"),
                xmPushButtonGadgetClass, 
                rowcol,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        }
        else {  // "Clear Dirs"
            button_clear = XtVaCreateManagedWidget(langcode("PULDNMMC08"),
                xmPushButtonGadgetClass, 
                rowcol,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        }


// "Vector Maps"
        button_V = XtVaCreateManagedWidget(langcode("PULDNMMC02"),
                xmPushButtonGadgetClass, 
                rowcol,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

// "250k Topos"
        button_C = XtVaCreateManagedWidget(langcode("PULDNMMC03"),
                xmPushButtonGadgetClass, 
                rowcol,
                XmNnavigationType, XmTAB_GROUP,
#ifndef HAVE_LIBGEOTIFF
                XmNsensitive, FALSE,
#endif /* HAVE_LIBGEOTIFF */
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

// "100k Topos"
        button_F = XtVaCreateManagedWidget(langcode("PULDNMMC04"),
                xmPushButtonGadgetClass, 
                rowcol,
                XmNnavigationType, XmTAB_GROUP,
#ifndef HAVE_LIBGEOTIFF
                XmNsensitive, FALSE,
#endif /* HAVE_LIBGEOTIFF */
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

// "24k Topos"
        button_O = XtVaCreateManagedWidget(langcode("PULDNMMC05"),
                xmPushButtonGadgetClass, 
                rowcol,
                XmNnavigationType, XmTAB_GROUP,
#ifndef HAVE_LIBGEOTIFF
                XmNsensitive, FALSE,
#endif /* HAVE_LIBGEOTIFF */
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

// "Apply"
        button_apply = XtVaCreateManagedWidget(langcode("UNIOP00032"),
                xmPushButtonGadgetClass,
                rowcol,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

// "OK"
        map_chooser_button_ok = XtVaCreateManagedWidget(langcode("UNIOP00001"),
                xmPushButtonGadgetClass, 
                rowcol,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

// "Cancel"
        map_chooser_button_cancel = XtVaCreateManagedWidget(langcode("UNIOP00002"),
                xmPushButtonGadgetClass, 
                rowcol,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        XtAddCallback(button_apply, XmNactivateCallback, map_chooser_apply_maps, map_chooser_dialog);
        XtAddCallback(map_chooser_button_cancel, XmNactivateCallback, map_chooser_destroy_shell, map_chooser_dialog);
        XtAddCallback(map_chooser_button_ok, XmNactivateCallback, map_chooser_select_maps, map_chooser_dialog);
        XtAddCallback(button_clear, XmNactivateCallback, map_chooser_deselect_maps, map_chooser_dialog);
        XtAddCallback(button_V, XmNactivateCallback, map_chooser_select_vector_maps, map_chooser_dialog);
#ifdef HAVE_LIBGEOTIFF
        XtAddCallback(button_C, XmNactivateCallback, map_chooser_select_250k_maps, map_chooser_dialog);
        XtAddCallback(button_F, XmNactivateCallback, map_chooser_select_100k_maps, map_chooser_dialog);
        XtAddCallback(button_O, XmNactivateCallback, map_chooser_select_24k_maps, map_chooser_dialog);
#endif /* HAVE_LIBGEOTIFF */

        if(!map_chooser_expand_dirs) {
            XtSetSensitive(button_V, FALSE);
            XtSetSensitive(button_C, FALSE);
            XtSetSensitive(button_F, FALSE);
            XtSetSensitive(button_O, FALSE);
        }
 
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
//        fix_dialog_vsize(map_chooser_dialog);

        // Move focus to the OK button.  This appears to highlight the
        // button fine, but we're not able to hit the <Enter> key to
        // have that default function happen.  Note:  We _can_ hit the
        // <SPACE> key, and that activates the option.
//        XmUpdateDisplay(map_chooser_dialog);
        XmProcessTraversal(map_chooser_button_ok, XmTRAVERSE_CURRENT);

   } else {
        (void)XRaiseWindow(XtDisplay(map_chooser_dialog), XtWindow(map_chooser_dialog));
    }

   map_chooser_update_quantity();
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
        // fprintf(stderr,"FILE is %s\n",file);

        // Make sure we're not already reading a file and the user actually
        // selected a file (if not, the last character will be a '/').
        if ( (!read_file) && (file[strlen(file) - 1] != '/') ) {

            /* do read file start */
            read_file_ptr = fopen(file,"r");
            if (read_file_ptr != NULL)
                read_file = 1;
            else
                fprintf(stderr,"Couldn't open file: %s\n", file);

        }
        XtFree(file);
    }
    read_file_selection_destroy_shell(w, clientData, callData);

    // Note that we leave the file in the "open" state.  UpdateTime
    // comes along shortly and reads the file.
}





void Read_File_Selection( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer callData) {
    Arg al[50];                    /* Arg List */
    register unsigned int ac = 0;           /* Arg Count */
    Widget fs;
    Widget child;


    if (read_selection_dialog!=NULL)
        read_file_selection_destroy_shell(read_selection_dialog, read_selection_dialog, NULL);

    if(read_selection_dialog==NULL) {

        // This is necessary because the resources for setting the
        // directory in the FileSelectionDialog aren't working in Lesstif.
        chdir( get_user_base_dir("logs") );

        /*set args for color */
        ac=0;
        XtSetArg(al[ac], XmNtitle, langcode("PULDNFI002")); ac++;  // Open Log File
        XtSetArg(al[ac], XmNtraversalOn, TRUE); ac++;
        XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
        XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
        XtSetArg(al[ac], XmNfontList, fontlist1); ac++;
        //XtSetArg(al[ac], XmNdirMask, "/home/hacker/.xastir/logs/*"); ac++;
        //XtSetArg(al[ac], XmNdirectory, "/home/hacker/.xastir/logs/"); ac++;
        //XtSetArg(al[ac], XmNpattern, "*"); ac++;
        //XtSetArg(al[ac], XmNdirMask, ".xastir/logs/*"); ac++;
        read_selection_dialog = XmCreateFileSelectionDialog(appshell,
                "filesb",
                al,
                ac);

        // Change back to the base directory 
        chdir( get_user_base_dir("") );

        fs=XmFileSelectionBoxGetChild(read_selection_dialog,(unsigned char)XmDIALOG_TEXT);
        XtVaSetValues(fs,XmNbackground, colors[0x0f],NULL);
        XtVaSetValues(fs,XmNfontList,fontlist1,NULL);

        fs=XmFileSelectionBoxGetChild(read_selection_dialog,(unsigned char)XmDIALOG_FILTER_TEXT);
        XtVaSetValues(fs,XmNbackground, colors[0x0f],NULL);
        XtVaSetValues(fs,XmNfontList,fontlist1,NULL);

        fs=XmFileSelectionBoxGetChild(read_selection_dialog,(unsigned char)XmDIALOG_DIR_LIST);
        XtVaSetValues(fs,XmNbackground, colors[0x0f],NULL);
        XtVaSetValues(fs,XmNfontList,fontlist1,NULL);

        fs=XmFileSelectionBoxGetChild(read_selection_dialog,(unsigned char)XmDIALOG_LIST);
        XtVaSetValues(fs,XmNbackground, colors[0x0f],NULL);
        XtVaSetValues(fs,XmNfontList,fontlist1,NULL);

        //XtVaSetValues(read_selection_dialog, XmNdirMask, "/home/hacker/.xastir/logs/*", NULL);

        child = XmFileSelectionBoxGetChild(read_selection_dialog, XmDIALOG_FILTER_LABEL);
        XtVaSetValues(child,XmNfontList,fontlist1,NULL);

        child = XmFileSelectionBoxGetChild(read_selection_dialog, XmDIALOG_DIR_LIST_LABEL);
        XtVaSetValues(child,XmNfontList,fontlist1,NULL);

        child = XmFileSelectionBoxGetChild(read_selection_dialog, XmDIALOG_LIST_LABEL);
        XtVaSetValues(child,XmNfontList,fontlist1,NULL);

        child = XmFileSelectionBoxGetChild(read_selection_dialog, XmDIALOG_SELECTION_LABEL);
        XtVaSetValues(child,XmNfontList,fontlist1,NULL);

        child = XmFileSelectionBoxGetChild(read_selection_dialog, XmDIALOG_OK_BUTTON);
        XtVaSetValues(child,XmNfontList,fontlist1,NULL);

        child = XmFileSelectionBoxGetChild(read_selection_dialog, XmDIALOG_APPLY_BUTTON);
        XtVaSetValues(child,XmNfontList,fontlist1,NULL);

        child = XmFileSelectionBoxGetChild(read_selection_dialog, XmDIALOG_CANCEL_BUTTON);
        XtVaSetValues(child,XmNfontList,fontlist1,NULL);

        child = XmFileSelectionBoxGetChild(read_selection_dialog, XmDIALOG_HELP_BUTTON);
        XtVaSetValues(child,XmNfontList,fontlist1,NULL);

        XtAddCallback(read_selection_dialog, XmNcancelCallback,read_file_selection_destroy_shell,read_selection_dialog);
        XtAddCallback(read_selection_dialog, XmNokCallback,read_file_selection_now,read_selection_dialog);

        XtAddCallback(read_selection_dialog, XmNhelpCallback, Help_Index, read_selection_dialog);

        XtManageChild(read_selection_dialog);
        pos_dialog(read_selection_dialog);
    }
}





void Test(Widget w, XtPointer clientData, XtPointer callData) {
//    static char temp[256];
//    int port = 7;


    mdisplay_file(0);
//    mem_display();
    alert_print_list();

/*
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
*/

//    fprintf(stderr,"view_zero_distance_bulletins = %d\n",
//        view_zero_distance_bulletins);


/*
    // Simulate data coming in from a TNC in order to test igating.
    // Port 7 in this case is a serial TNC port (in my current test
    // configuration).
    xastir_snprintf(temp,
        sizeof(temp),
        "WE7U-4>APOT01,SUMAS*,WIDE2-2:!4757.28N/12212.00Wv178/057/A=000208 14.0V 30C\r");

    if (begin_critical_section(&data_lock, "main.c" ) > 0)
        fprintf(stderr,"data_lock, Port = %d\n", port);

    incoming_data=temp;
    incoming_data_length = strlen(temp);
    data_port = port;
    data_avail = 1;

    if (end_critical_section(&data_lock, "main.c" ) > 0)
        fprintf(stderr,"data_lock, Port = %d\n", port);

    fprintf(stderr, "Sent: %s\n", temp);
*/


//    (void)XCopyArea(XtDisplay(da),pixmap_final,XtWindow(da),gc,0,0,(unsigned int)screen_width,(unsigned int)screen_height,0,0);
}





/****** Save Config data **********/

void Save_Config( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer callData) {
    save_data();
}





///////////////////////////////////   Configure Defaults Dialog   //////////////////////////////////


void Configure_defaults_destroy_shell( /*@unused@*/ Widget widget, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    Widget shell = (Widget) clientData;
    XtPopdown(shell);
    XtDestroyWidget(shell);
    configure_defaults_dialog = (Widget)NULL;
}





void Configure_defaults_change_data(Widget widget, XtPointer clientData, XtPointer callData) {
    char *temp;
    int load_predefined_cb_selected;
    XmString load_predefined_cb_selection;


    output_station_type = Station_transmit_type;
    if ((output_station_type >= 1) && (output_station_type <= 3)) {
        next_time = 60;
        max_transmit_time = (time_t)120l;       // shorter beacon interval for mobile stations
    }
    else {
        max_transmit_time = (time_t)900l;
    }

    // Check for proper symbol in case we're a weather station
    (void)check_weather_symbol();

    // Check for NWS symbol and print warning if so
    (void)check_nws_weather_symbol();

#ifdef TRANSMIT_RAW_WX
    transmit_raw_wx = (int)XmToggleButtonGetState(raw_wx_tx);
#endif  // TRANSMIT_RAW_WX

    transmit_compressed_objects_items = (int)XmToggleButtonGetState(compressed_objects_items_tx);

    pop_up_new_bulletins = (int)XmToggleButtonGetState(new_bulletin_popup_enable);
    view_zero_distance_bulletins = (int)XmToggleButtonGetState(zero_bulletin_popup_enable);

    // user interface refers to all my trails in one color
    // my trail diff color as 0 means my trails in one color, so select
    // button when my trail diff color is 0 rather than 1.
    my_trail_diff_color = !(int)XmToggleButtonGetState(my_trail_diff_color_enable);

    // Predefined (SAR/EVENT) objects menu loading (default hardcoded SAR objects or objects from file)
    predefined_menu_from_file = (int)XmToggleButtonGetState(load_predefined_objects_menu_from_file_enable);

    // Use the file specified on the picklist if one is selected.
    load_predefined_cb_selected = 0;
#ifdef USE_COMBO_BOX
    XtVaGetValues(load_predefined_objects_menu_from_file, 
        XmNselectedPosition,
        &load_predefined_cb_selected,
        NULL);
#else
    load_predefined_cb_selected = lpomff_value;
#endif //USE_COMBO_BOX

    // Use the file specified on the picklist if one is selected.
    if (load_predefined_cb_selected > 0) {

        // XtVaGetValues() expects to be able to write into
        // allocated memory.
        //
        load_predefined_cb_selection = (XmString)malloc(MAX_FILENAME);

#ifdef USE_COMBO_BOX
        XtVaGetValues(load_predefined_objects_menu_from_file, 
            XmNselectedItem,
            &load_predefined_cb_selection,
            NULL);
#else
        switch (load_predefined_cb_selected) { 
           case 1:
               load_predefined_cb_selection = XmStringCreateLtoR("predefined_SAR.sys", XmFONTLIST_DEFAULT_TAG);
              break;
           case 2:
               load_predefined_cb_selection = XmStringCreateLtoR("predefined_EVENT.sys", XmFONTLIST_DEFAULT_TAG);
               break;
           case 3:
               load_predefined_cb_selection = XmStringCreateLtoR("predefined_USER.sys", XmFONTLIST_DEFAULT_TAG);
               break;
           case 4:
               load_predefined_cb_selection = XmStringCreateLtoR(predefined_object_definition_filename, XmFONTLIST_DEFAULT_TAG);
               break;
           default:
               load_predefined_cb_selection = XmStringCreateLtoR("predefined_SAR.sys", XmFONTLIST_DEFAULT_TAG);
        }
#endif //USE_COMBO_BOX
    }
    else {

        // ??????????????
        // Should this be XmStringCreateLocalized, or another
        // XmString creation function with XmCHARSET_TEXT??  Not
        // sure of the implications of using localization or not
        // when creating and extracting the picklist values.
        // XmStringCreateLtoR, allthough deprecated appears to be in
        // standard use in Xastir, use it pending global
        // conversion.
        // ??????????????

        load_predefined_cb_selection = XmStringCreateLtoR("predefined_SAR.sys", XmFONTLIST_DEFAULT_TAG);
    }

    xastir_snprintf(predefined_object_definition_filename,
        sizeof(predefined_object_definition_filename),
        XmStringUnparse(load_predefined_cb_selection,
            NULL,
            XmCHARSET_TEXT,
            XmCHARSET_TEXT,
            NULL,
            0,
            XmOUTPUT_ALL) );

        //XmStringGetLtoR(load_predefined_cb_selection,XmFONTLIST_DEFAULT_TAG,temp));

    XmStringFree(load_predefined_cb_selection);

    // Repopulate the predefined object (SAR/Public service) struct
    Populate_predefined_objects(predefinedObjects); 

    // Rebuild the predefined objects SAR/Public service menu.
    BuildPredefinedSARMenu_UI(&sar_object_sub);

    warn_about_mouse_modifiers = (int)XmToggleButtonGetState(warn_about_mouse_modifiers_enable);

    altnet = (int)(XmToggleButtonGetState(altnet_active));

    skip_dupe_checking = (int)(XmToggleButtonGetState(disable_dupe_check));

    temp = XmTextGetString(altnet_text);
    xastir_snprintf(altnet_call,
        sizeof(altnet_call),
        "%s",
        temp);
    XtFree(temp);
    
    (void)remove_trailing_spaces(altnet_call);
    if (strlen(altnet_call)==0) {
        altnet = FALSE;
        xastir_snprintf(altnet_call,
            sizeof(altnet_call),
            "XASTIR");
    }

    operate_as_an_igate=Igate_type;

    redraw_on_new_data=2;
    Configure_defaults_destroy_shell(widget,clientData,callData);
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


#ifndef USE_COMBO_BOX
void lpomff_menuCallback(Widget widget, XtPointer ptr, XtPointer callData) {
    XtPointer userData;

    XtVaGetValues(widget, XmNuserData, &userData, NULL);
    //clsd_menu is zero based, cad_line_style_data constants are one based. 
    lpomff_value = (int)userData + 1;
    if (debug_level & 1)
        fprintf(stderr,"Selected value on cad line type pulldown: %d\n",lpomff_value);
}
#endif  // !USE_COMBO_BOX





void Configure_defaults( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer callData) {
    static Widget  pane, my_form, button_ok, button_cancel,
                frame4, frame5,
                station_type, type_box,
                styp1, styp2, styp3, styp4, styp5, styp6,
                igate_option, igate_box,
                igtyp0, igtyp1, igtyp2, altnet_label;
    Atom delw;
    Arg al[50];                      /* Arg List */
    register unsigned int ac = 0;   /* Arg Count */
#ifndef USE_COMBO_BOX
    Widget lpomff_menuPane;  
    Widget lpomff_button;
    Widget lpomff_buttons[4];
    Widget lpomff_menu;
    char buf[18];   
    int x;
#endif // !USE_COMBO_BOX
    Widget lpomff_widget;
    int i;
    XmString cb_items[4];

                                                                                                                        

    if (!configure_defaults_dialog) {
        char loadfrom[300];

        // Set args for color
        ac = 0;
        XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
        XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
        XtSetArg(al[ac], XmNfontList, fontlist1); ac++;

        configure_defaults_dialog = XtVaCreatePopupShell(langcode("WPUPCFD001"),
                xmDialogShellWidgetClass, appshell,
                XmNdeleteResponse, XmDESTROY,
                XmNdefaultPosition, FALSE,
                XmNfontList, fontlist1,
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

        // Transmit Station Options
        frame4 = XtVaCreateManagedWidget("Configure_defaults frame4", 
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

        station_type  = XtVaCreateManagedWidget(langcode("WPUPCFD015"),
                xmLabelWidgetClass,
                frame4,
                XmNchildType, XmFRAME_TITLE_CHILD,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(styp1,XmNvalueChangedCallback,station_type_toggle,"0");

        styp2 = XtVaCreateManagedWidget(langcode("WPUPCFD017"),
                xmToggleButtonGadgetClass,
                type_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(styp2,XmNvalueChangedCallback,station_type_toggle,"1");

        styp3 = XtVaCreateManagedWidget(langcode("WPUPCFD018"),
                xmToggleButtonGadgetClass,
                type_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(styp3,XmNvalueChangedCallback,station_type_toggle,"2");

        styp4 = XtVaCreateManagedWidget(langcode("WPUPCFD019"),
                xmToggleButtonGadgetClass,
                type_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(styp4,XmNvalueChangedCallback,station_type_toggle,"3");

        styp5 = XtVaCreateManagedWidget(langcode("WPUPCFD021"),
                xmToggleButtonGadgetClass,
                type_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(styp5,XmNvalueChangedCallback,station_type_toggle,"4");

        styp6 = XtVaCreateManagedWidget(langcode("WPUPCFD022"),
                xmToggleButtonGadgetClass,
                type_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
                NULL);

        XtAddCallback(igtyp0,XmNvalueChangedCallback,igate_type_toggle,"0");

        igtyp1 = XtVaCreateManagedWidget(langcode("IGPUPCF002"),
                xmToggleButtonGadgetClass,
                igate_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        XtAddCallback(igtyp1,XmNvalueChangedCallback,igate_type_toggle ,"1");

        igtyp2 = XtVaCreateManagedWidget(langcode("IGPUPCF003"),
                xmToggleButtonGadgetClass,
                igate_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
                NULL);

        warn_about_mouse_modifiers_enable = XtVaCreateManagedWidget(langcode("WPUPCFD028"),
                xmToggleButtonWidgetClass,
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, new_bulletin_popup_enable,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, zero_bulletin_popup_enable,
                XmNleftOffset,10,
                XmNrightAttachment, XmATTACH_NONE,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        // Show all My trails in one color
        my_trail_diff_color_enable = XtVaCreateManagedWidget(langcode("WPUPCFD032"),
                xmToggleButtonWidgetClass,
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, new_bulletin_popup_enable,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, warn_about_mouse_modifiers_enable,
                XmNleftOffset,10,
                XmNrightAttachment, XmATTACH_NONE,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
       
       
        // Check box to load predefined (SAR/Event) objects menu from a file or not.
        xastir_snprintf(loadfrom,
                        sizeof(loadfrom),
                        "%s %s",
                        langcode("WPUPCFD031"),
#ifdef OBJECT_DEF_FILE_USER_BASE
                        get_user_base_dir("config"));
#else   // OBJECT_DEF_FILE_USER_BASE
                        get_data_base_dir("config"));
#endif  // OBJECT_DEF_FILE_USER_BASE

        load_predefined_objects_menu_from_file_enable = XtVaCreateManagedWidget(loadfrom,
                xmToggleButtonWidgetClass,
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, zero_bulletin_popup_enable,
                XmNtopOffset,5,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset,10,
                XmNrightAttachment, XmATTACH_NONE,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
         
        // lesstif as of 0.95 in 2008 doesn't fully support combo boxes
        // 
        // Need to replace combo boxes with a pull down menu when lesstif is used.
        // See xpdf's  XPDFViewer.cc/XPDFViewer.h for an example.

        cb_items[0] = XmStringCreateLtoR("predefined_SAR.sys", XmFONTLIST_DEFAULT_TAG);
        cb_items[1] = XmStringCreateLtoR("predefined_EVENT.sys", XmFONTLIST_DEFAULT_TAG);
        cb_items[2] = XmStringCreateLtoR("predefined_USER.sys", XmFONTLIST_DEFAULT_TAG);
#ifdef USE_COMBO_BOX
        // Combo box to pick file from which to load predefined objects menu
        load_predefined_objects_menu_from_file = XtVaCreateManagedWidget("Load objects menu filename ComboBox",
                xmComboBoxWidgetClass,
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, zero_bulletin_popup_enable,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, load_predefined_objects_menu_from_file_enable,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_NONE,
                XmNnavigationType, XmTAB_GROUP,
                XmNcomboBoxType, XmDROP_DOWN_LIST,
                XmNpositionMode, XmONE_BASED, 
                XmNvisibleItemCount, 3,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XmComboBoxAddItem(load_predefined_objects_menu_from_file,cb_items[0],1,1);  
        XmComboBoxAddItem(load_predefined_objects_menu_from_file,cb_items[1],2,1);  
        XmComboBoxAddItem(load_predefined_objects_menu_from_file,cb_items[2],3,1);  

        lpomff_widget = load_predefined_objects_menu_from_file;
#else
        // Menu replacement for combo box when using lesstif.
        // Not a full replacement, as combo box in motif can have editable values, 
        // not just selection from predefined list as is the case here.
        ac = 0;
        XtSetArg(al[ac], XmNmarginWidth, 0); ac++;
        XtSetArg(al[ac], XmNmarginHeight, 0); ac++;
        XtSetArg(al[ac], XmNfontList, fontlist1); ac++;
        lpomff_menuPane = XmCreatePulldownMenu(my_form,"lpomff_menuPane", al, ac);
        //lpomff_menu is zero based, constants for filenames are one based
        //lpomff_value is set to match constants in callback.
        for (i=0;i<3;i++) { 
            ac = 0;
            XtSetArg(al[ac], XmNlabelString, cb_items[i]); ac++;
            XtSetArg(al[ac], XmNuserData, (XtPointer)i); ac++;
            sprintf(buf,"button%d",i);
            lpomff_button = XmCreatePushButton(lpomff_menuPane, buf, al, ac);
            XtManageChild(lpomff_button);
            XtAddCallback(lpomff_button, XmNactivateCallback, lpomff_menuCallback, Configure_defaults);
            lpomff_buttons[i] = lpomff_button;
        }
        ac = 0;
        XtSetArg(al[ac], XmNleftAttachment, XmATTACH_WIDGET); ++ac;
        XtSetArg(al[ac], XmNleftWidget, load_predefined_objects_menu_from_file_enable); ++ac;
        XtSetArg(al[ac], XmNtopAttachment, XmATTACH_WIDGET); ++ac;
        XtSetArg(al[ac], XmNtopWidget, zero_bulletin_popup_enable); ++ac;
        XtSetArg(al[ac], XmNmarginWidth, 0); ++ac;
        XtSetArg(al[ac], XmNmarginHeight, 0); ++ac;
        XtSetArg(al[ac], XmNtopOffset, 5); ++ac;
        XtSetArg(al[ac], XmNleftOffset, 10); ++ac;
        XtSetArg(al[ac], XmNsubMenuId, lpomff_menuPane); ++ac;
        XtSetArg(al[ac], XmNfontList, fontlist1); ac++;
        lpomff_menu = XmCreateOptionMenu(my_form, "sddd_Menu", al, ac);
        XtManageChild(lpomff_menu);
        lpomff_value = 2;   // set a default value (line on off dash)
        lpomff_widget = lpomff_menu;
#endif  // USE_COMBO_BOX
        // free up space from combo box strings 
        for (i=0;i<3;i++) {
           XmStringFree(cb_items[i]);
        }


#ifdef TRANSMIT_RAW_WX
        raw_wx_tx  = XtVaCreateManagedWidget(langcode("WPUPCFD023"),
                xmToggleButtonWidgetClass,
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, lpomff_widget,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_NONE,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
#endif  // TRANSMIT_RAW_WX

        altnet_active  = XtVaCreateManagedWidget(langcode("WPUPCFD025"),
                xmToggleButtonWidgetClass,
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, frame5,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, compressed_objects_items_tx,
                XmNrightAttachment, XmATTACH_NONE,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        // "ALTNET:"
        altnet_label = XtVaCreateManagedWidget(langcode("WPUPCFD033"),
                xmLabelWidgetClass,
                my_form,
                XmNchildType, XmFRAME_TITLE_CHILD,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNtopAttachment,XmATTACH_WIDGET,
                XmNtopWidget, altnet_active,
                XmNtopOffset, 5,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, compressed_objects_items_tx,
                XmNrightAttachment, XmATTACH_NONE,
                XmNfontList, fontlist1,
                NULL);

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
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, altnet_label,
                XmNrightAttachment, XmATTACH_NONE,
                XmNnavigationType, XmTAB_GROUP,
                XmNfontList, fontlist1,
                NULL);

        disable_dupe_check = XtVaCreateManagedWidget(langcode("WPUPCFD030"),
                xmToggleButtonWidgetClass,
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, frame5,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, altnet_active,
                XmNrightAttachment, XmATTACH_NONE,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        button_ok = XtVaCreateManagedWidget(langcode("UNIOP00001"),
                xmPushButtonGadgetClass, 
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
#ifdef TRANSMIT_RAW_WX
                XmNtopWidget, raw_wx_tx,
#else   // TRANSMIT_RAW_WX
                XmNtopWidget, lpomff_widget,
#endif  // TRANSMIT_RAW_WX
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
                XmNfontList, fontlist1,
                NULL);


        button_cancel = XtVaCreateManagedWidget(langcode("UNIOP00002"),
                xmPushButtonGadgetClass, 
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
#ifdef TRANSMIT_RAW_WX
                XmNtopWidget, raw_wx_tx,
#else   // TRANSMIT_RAW_WX
                XmNtopWidget, lpomff_widget,
#endif  // TRANSMIT_RAW_WX
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
                XmNfontList, fontlist1,
                NULL);

        XtAddCallback(button_ok, XmNactivateCallback, Configure_defaults_change_data, configure_defaults_dialog);
        XtAddCallback(button_cancel, XmNactivateCallback, Configure_defaults_destroy_shell, configure_defaults_dialog);

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
#endif  // TRANSMIT_RAW_WX

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

        // user interface refers to all my trails in one color
        // my trail diff color as 0 means my trails in one color, so select
        // button when my trail diff color is 0 rather than 1.
        if(my_trail_diff_color)  
            XmToggleButtonSetState(my_trail_diff_color_enable,FALSE,FALSE);
        else
            XmToggleButtonSetState(my_trail_diff_color_enable,TRUE,FALSE);

        if(predefined_menu_from_file) {
            // Option to load the predefined SAR objects menu items from a file.
            // Display the filename if one is currently selected and option is enabled.
            if (predefined_object_definition_filename != NULL ) { 

#ifdef USE_COMBO_BOX            
                XmString tempSelection = XmStringCreateLtoR(predefined_object_definition_filename, XmFONTLIST_DEFAULT_TAG);
                XmComboBoxSelectItem(load_predefined_objects_menu_from_file, tempSelection);
                XmStringFree(tempSelection);
#else
                x = -1;
                if (predefined_object_definition_filename=="predefined_SAR.sys") 
                    x = 0;
                if (predefined_object_definition_filename=="predefined_EVENT.sys")
                    x = 1;
                if (predefined_object_definition_filename=="predefined_USER.sys")
                    x = 2;
                i = 3;
                // allow display of another filename from the config file.
                // user won't be able to edit it, but they will see it.
                if (x==-1) { 
                    ac = 0;
                    cb_items[i] = XmStringCreateLtoR(predefined_object_definition_filename, XmFONTLIST_DEFAULT_TAG);
                    XtSetArg(al[ac], XmNlabelString, cb_items[i]); ac++;
                    XtSetArg(al[ac], XmNuserData, (XtPointer)i); ac++;
                    XtSetArg(al[ac], XmNfontList, fontlist1); ac++;
                    sprintf(buf,"button%d",i);
                    lpomff_button = XmCreatePushButton(lpomff_menuPane, buf, al, ac);
                    XtManageChild(lpomff_button);
                    XtAddCallback(lpomff_button, XmNactivateCallback, lpomff_menuCallback, Configure_defaults);
                    lpomff_buttons[i] = lpomff_button;
                    XmStringFree(cb_items[i]);
                    x = i;
                }
                XtVaSetValues(lpomff_menu, XmNmenuHistory, lpomff_buttons[x], NULL);
                lpomff_value = x+1;
#endif // USE_COMBO_BOX

            }
            XmToggleButtonSetState(load_predefined_objects_menu_from_file_enable,TRUE,FALSE);
        } else {
            // by default combo box is created with no selection
            // make sure that toggle button is unchecked
            XmToggleButtonSetState(load_predefined_objects_menu_from_file_enable,FALSE,FALSE);
        }
 
        XmToggleButtonSetState(altnet_active, altnet, FALSE);

        XmToggleButtonSetState(disable_dupe_check, skip_dupe_checking, FALSE);

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





///////////////////////////////////   Configure Timing Dialog   //////////////////////////////////


void Configure_timing_destroy_shell( /*@unused@*/ Widget widget, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    Widget shell = (Widget) clientData;
    XtPopdown(shell);
    XtDestroyWidget(shell);
    configure_timing_dialog = (Widget)NULL;
}





void Configure_timing_change_data(Widget widget, XtPointer clientData, XtPointer callData) {
    int value;


    XmScaleGetValue(ghosting_time, &value);     // Minutes
    sec_old = (time_t)(value * 60);             // Convert to seconds

    XmScaleGetValue(clearing_time, &value);     // Hours
    sec_clear = (time_t)(value * 60 * 60);      // Convert to seconds

    XmScaleGetValue(posit_interval, &value);    // Minutes * 10
    POSIT_rate = (long)(value * 60 / 10);       // Convert to seconds

    XmScaleGetValue(gps_interval, &value);      // Seconds
    gps_time = (long)value;

    XmScaleGetValue(dead_reckoning_time, &value);// Minutes
    dead_reckoning_timeout = value * 60;        // Convert to seconds

    XmScaleGetValue(object_item_interval, &value);// Minutes
    OBJECT_rate = value * 60;                   // Convert to seconds

    XmScaleGetValue(removal_time, &value);      // Days
    sec_remove = (time_t)(value * 60 * 60 * 24);// Convert to seconds

    // Set the new posit rate into effect immediately
    posit_next_time = posit_last_time + POSIT_rate;

    // Set the new GPS rate into effect immediately
    sec_next_gps = sec_now() + gps_time;

    // Set the serial port inter-character delay
    XmScaleGetValue(serial_pacing_time, &serial_char_pacing);     // Milliseconds

    XmScaleGetValue(trail_segment_timeout, &value); // Minutes
    trail_segment_time = (int)value;

    XmScaleGetValue(trail_segment_distance_max, &value); // Degrees
    trail_segment_distance = (int)value;

#ifdef HAVE_GPSMAN
    XmScaleGetValue(RINO_download_timeout, &value); // Degrees
    RINO_download_interval = (int)value;
#endif  // HAVE_GPSMAN

    XmScaleGetValue(net_map_slider, &net_map_timeout);

    XmScaleGetValue(snapshot_interval_slider, &snapshot_interval);

    redraw_on_new_data=2;
    Configure_timing_destroy_shell(widget,clientData,callData);
}





void Configure_timing( /*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer callData) {
    static Widget  pane, my_form, button_ok, button_cancel;
    Atom delw;
    int length;
    int timeout_length;
    XmString x_str;


    if (!configure_timing_dialog) {
        configure_timing_dialog = XtVaCreatePopupShell(langcode("WPUPCFTM01"),
                xmDialogShellWidgetClass, appshell,
                XmNdeleteResponse, XmDESTROY,
                XmNdefaultPosition, FALSE,
                XmNfontList, fontlist1,
                NULL);

        pane = XtVaCreateWidget("Configure_timing pane",
                xmPanedWindowWidgetClass, 
                configure_timing_dialog,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        my_form =  XtVaCreateWidget("Configure_timing my_form",
                xmFormWidgetClass, 
                pane,
                XmNfractionBase, 2,
                XmNautoUnmanage, FALSE,
                XmNshadowThickness, 1,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        // Posit Time
        length = strlen(langcode("WPUPCFTM02")) + 1;
        x_str = XmStringCreateLocalized(langcode("WPUPCFTM02"));
        posit_interval = XtVaCreateManagedWidget("Posit Interval",
                xmScaleWidgetClass,
                my_form,
                XmNtopAttachment, XmATTACH_FORM,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 0,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 1,
                XmNrightOffset, 5,
                XmNsensitive, TRUE,
                XmNorientation, XmHORIZONTAL,
                XmNborderWidth, 1,
                XmNminimum, 5,          // 0.5 = Thirty seconds
                XmNmaximum, 60*10,      // 60 minutes
                XmNdecimalPoints, 1,    // Move decimal point over one
                XmNshowValue, TRUE,
                XmNvalue, (int)((POSIT_rate * 10) / 60),  // Minutes * 10
// Note:  Some versions of OpenMotif (distributed with Fedora,
// perhaps others) don't work properly with XtVaTypedArg() as used
// here, instead showing blank labels for the Scale widgets.
//                XtVaTypedArg, XmNtitleString, XmRString, langcode("WPUPCFTM02"), length,
                XmNtitleString, x_str,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XmStringFree(x_str);


        // Interval for stations being considered old (symbol ghosting)
        length = strlen(langcode("WPUPCFTM03")) + 1;
        x_str = XmStringCreateLocalized(langcode("WPUPCFTM03"));
        ghosting_time = XtVaCreateManagedWidget("Station Ghosting Time",
                xmScaleWidgetClass,
                my_form,
                XmNtopAttachment, XmATTACH_FORM,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 1,
                XmNleftOffset, 5,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 2,
                XmNrightOffset, 10,
                XmNsensitive, TRUE,
                XmNorientation, XmHORIZONTAL,
                XmNborderWidth, 1,
                XmNminimum, 1,      // One minute
                XmNmaximum, 3*60,   // Three hours
                XmNshowValue, TRUE,
                XmNvalue, (int)(sec_old/60),
// Note:  Some versions of OpenMotif (distributed with Fedora,
// perhaps others) don't work properly with XtVaTypedArg() as used
// here, instead showing blank labels for the Scale widgets.
//                XtVaTypedArg, XmNtitleString, XmRString, langcode("WPUPCFTM03"), length,
                XmNtitleString, x_str,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XmStringFree(x_str);

        // Object Item Transmit Interval
        length = strlen(langcode("WPUPCFTM04")) + 1;
        x_str = XmStringCreateLocalized(langcode("WPUPCFTM04"));
        object_item_interval = XtVaCreateManagedWidget("Object/Item Transmit Interval (min)",
                xmScaleWidgetClass,
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, posit_interval,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 0,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 1,
                XmNrightOffset, 5,
                XmNsensitive, TRUE,
                XmNorientation, XmHORIZONTAL,
                XmNborderWidth, 1,
                XmNminimum, 5,      // Five minutes
                XmNmaximum, 120,    // 120 minutes
                XmNshowValue, TRUE,
                XmNvalue, (int)(OBJECT_rate / 60),
// Note:  Some versions of OpenMotif (distributed with Fedora,
// perhaps others) don't work properly with XtVaTypedArg() as used
// here, instead showing blank labels for the Scale widgets.
//                XtVaTypedArg, XmNtitleString, XmRString, langcode("WPUPCFTM04"), length,
                XmNtitleString, x_str,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XmStringFree(x_str);

        // Interval for station not being displayed
        length = strlen(langcode("WPUPCFTM05")) + 1;
        x_str = XmStringCreateLocalized(langcode("WPUPCFTM05"));
        clearing_time = XtVaCreateManagedWidget("Station Clear Time",
                xmScaleWidgetClass,
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, posit_interval,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 1,
                XmNleftOffset, 5,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 2,
                XmNrightOffset, 10,
                XmNsensitive, TRUE,
                XmNorientation, XmHORIZONTAL,
                XmNborderWidth, 1,
                XmNminimum, 1,      // One hour
                XmNmaximum, 24*7,   // One week
                XmNshowValue, TRUE,
                XmNvalue, (int)(sec_clear/(60*60)),
// Note:  Some versions of OpenMotif (distributed with Fedora,
// perhaps others) don't work properly with XtVaTypedArg() as used
// here, instead showing blank labels for the Scale widgets.
//                XtVaTypedArg, XmNtitleString, XmRString, langcode("WPUPCFTM05"), length,
                XmNtitleString, x_str,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XmStringFree(x_str);

        // GPS Time
        length = strlen(langcode("WPUPCFTM06")) + 1;
        x_str = XmStringCreateLocalized(langcode("WPUPCFTM06"));
        gps_interval = XtVaCreateManagedWidget("GPS Interval",
                xmScaleWidgetClass,
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, object_item_interval,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 0,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 1,
                XmNrightOffset, 5,
                XmNsensitive, TRUE,
                XmNorientation, XmHORIZONTAL,
                XmNborderWidth, 1,
                XmNminimum, 1,      // One second
                XmNmaximum, 60,     // Sixty seconds
                XmNshowValue, TRUE,
                XmNvalue, (int)gps_time,
// Note:  Some versions of OpenMotif (distributed with Fedora,
// perhaps others) don't work properly with XtVaTypedArg() as used
// here, instead showing blank labels for the Scale widgets.
//                XtVaTypedArg, XmNtitleString, XmRString, langcode("WPUPCFTM06"), length,
                XmNtitleString, x_str,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XmStringFree(x_str);

        // Interval for station being removed from database
        length = strlen(langcode("WPUPCFTM07")) + 1;
        x_str = XmStringCreateLocalized(langcode("WPUPCFTM07"));
        removal_time = XtVaCreateManagedWidget("Station Delete Time",
                xmScaleWidgetClass,
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, object_item_interval,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 1,
                XmNleftOffset, 5,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 2,
                XmNrightOffset, 10,
                XmNsensitive, TRUE,
                XmNorientation, XmHORIZONTAL,
                XmNborderWidth, 1,
                XmNminimum, 1,      // One Day
                XmNmaximum, 14,     // Two weeks
                XmNshowValue, TRUE,
                XmNvalue, (int)(sec_remove/(60*60*24)),
// Note:  Some versions of OpenMotif (distributed with Fedora,
// perhaps others) don't work properly with XtVaTypedArg() as used
// here, instead showing blank labels for the Scale widgets.
//                XtVaTypedArg, XmNtitleString, XmRString, langcode("WPUPCFTM07"), length,
                XmNtitleString, x_str,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XmStringFree(x_str);

        // Dead Reckoning Timeout
        length = strlen(langcode("WPUPCFTM08")) + 1;
        x_str = XmStringCreateLocalized(langcode("WPUPCFTM08"));
        dead_reckoning_time = XtVaCreateManagedWidget("DR Time (min)",
                xmScaleWidgetClass,
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, gps_interval,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 0,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 1,
                XmNrightOffset, 5,
                XmNsensitive, TRUE,
                XmNorientation, XmHORIZONTAL,
                XmNborderWidth, 1,
                XmNminimum, 1,      // One minute
                XmNmaximum, 60,     // Sixty minutes
                XmNshowValue, TRUE,
                XmNvalue, (int)(dead_reckoning_timeout / 60),
// Note:  Some versions of OpenMotif (distributed with Fedora,
// perhaps others) don't work properly with XtVaTypedArg() as used
// here, instead showing blank labels for the Scale widgets.
//                XtVaTypedArg, XmNtitleString, XmRString, langcode("WPUPCFTM08"), length,
                XmNtitleString, x_str,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XmStringFree(x_str);

        // Serial Pacing Time (delay between each serial character)
        length = strlen(langcode("WPUPCFTM09")) + 1;
        x_str = XmStringCreateLocalized(langcode("WPUPCFTM09"));
        serial_pacing_time = XtVaCreateManagedWidget("Serial Pacing Time (ms)",
                xmScaleWidgetClass,
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, removal_time,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 1,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 2,
                XmNrightOffset, 5,
                XmNsensitive, TRUE,
                XmNorientation, XmHORIZONTAL,
                XmNborderWidth, 1,
                XmNminimum, 0,      // Zero
                XmNmaximum, 50,     // Fifty milliseconds
                XmNshowValue, TRUE,
                XmNvalue, (int)(serial_char_pacing),
// Note:  Some versions of OpenMotif (distributed with Fedora,
// perhaps others) don't work properly with XtVaTypedArg() as used
// here, instead showing blank labels for the Scale widgets.
//                XtVaTypedArg, XmNtitleString, XmRString, langcode("WPUPCFTM09"), length,
                XmNtitleString, x_str,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XmStringFree(x_str);

        // Time below which track segment will get drawn, in minutes
        length = strlen(langcode("WPUPCFTM10")) + 1;
        x_str = XmStringCreateLocalized(langcode("WPUPCFTM10"));
        trail_segment_timeout = XtVaCreateManagedWidget("Trail segment timeout",
                xmScaleWidgetClass,
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, dead_reckoning_time,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 0,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 1,
                XmNrightOffset, 5,
                XmNsensitive, TRUE,
                XmNorientation, XmHORIZONTAL,
                XmNborderWidth, 1,
                XmNminimum, 0,     // Zero minutes
                XmNmaximum, 12*60, // 12 hours
                XmNshowValue, TRUE,
                XmNvalue, trail_segment_time,
// Note:  Some versions of OpenMotif (distributed with Fedora,
// perhaps others) don't work properly with XtVaTypedArg() as used
// here, instead showing blank labels for the Scale widgets.
//                XtVaTypedArg, XmNtitleString, XmRString, langcode("WPUPCFTM10"), length,
                XmNtitleString, x_str,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XmStringFree(x_str);

        // Interval at track segment will not get drawn, in degrees
        length = strlen(langcode("WPUPCFTM11")) + 1;
        x_str = XmStringCreateLocalized(langcode("WPUPCFTM11"));
        trail_segment_distance_max = XtVaCreateManagedWidget("Trail segment interval degrees",
                xmScaleWidgetClass,
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, dead_reckoning_time,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 1,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 2,
                XmNrightOffset, 5,
                XmNsensitive, TRUE,
                XmNorientation, XmHORIZONTAL,
                XmNborderWidth, 1,
                XmNminimum, 0,  // Zero degrees
                XmNmaximum, 45, // 90 degrees
                XmNshowValue, TRUE,
                XmNvalue, trail_segment_distance,
// Note:  Some versions of OpenMotif (distributed with Fedora,
// perhaps others) don't work properly with XtVaTypedArg() as used
// here, instead showing blank labels for the Scale widgets.
//                XtVaTypedArg, XmNtitleString, XmRString, langcode("WPUPCFTM11"), length,
                XmNtitleString, x_str,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XmStringFree(x_str);

        // Time below which track segment will get drawn, in minutes
        length = strlen(langcode("WPUPCFTM12")) + 1;
        x_str = XmStringCreateLocalized(langcode("WPUPCFTM12"));
        RINO_download_timeout = XtVaCreateManagedWidget("RINO download interval",
                xmScaleWidgetClass,
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, trail_segment_timeout,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 0,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 1,
                XmNrightOffset, 5,
                XmNsensitive, TRUE,
                XmNorientation, XmHORIZONTAL,
                XmNborderWidth, 1,
                XmNminimum, 0,      // Zero minutes (disables the function)
                XmNmaximum, 30,     // 30 minutes
                XmNshowValue, TRUE,
                XmNvalue, RINO_download_interval,
// Note:  Some versions of OpenMotif (distributed with Fedora,
// perhaps others) don't work properly with XtVaTypedArg() as used
// here, instead showing blank labels for the Scale widgets.
//                XtVaTypedArg, XmNtitleString, XmRString, langcode("WPUPCFTM12"), length,
                XmNtitleString, x_str,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XmStringFree(x_str);

#ifndef HAVE_GPSMAN
XtSetSensitive(RINO_download_timeout, FALSE);
#endif  // HAVE_GPSMAN

       timeout_length = strlen(langcode("MPUPTGR017")) + 1;
       x_str = XmStringCreateLocalized(langcode("MPUPTGR017"));
       net_map_slider  = XtVaCreateManagedWidget("Net Map Timeout",
                xmScaleWidgetClass,
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, trail_segment_timeout,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 1,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 2,
                XmNrightOffset, 5,
                XmNsensitive, TRUE,
                XmNorientation, XmHORIZONTAL,
                XmNborderWidth, 1,
                XmNminimum, 10,
                XmNmaximum, 300,
                XmNshowValue, TRUE,
                XmNvalue, net_map_timeout,
// Note:  Some versions of OpenMotif (distributed with Fedora,
// perhaps others) don't work properly with XtVaTypedArg() as used
// here, instead showing blank labels for the Scale widgets.
//                XtVaTypedArg, XmNtitleString, XmRString, langcode("MPUPTGR017"), timeout_length,
                XmNtitleString, x_str,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XmStringFree(x_str);

        // Interval at which snapshots will be taken, in minutes
        length = strlen(langcode("WPUPCFTM13")) + 1;
        x_str = XmStringCreateLocalized(langcode("WPUPCFTM13"));
        snapshot_interval_slider = XtVaCreateManagedWidget("Snapshot interval",
                xmScaleWidgetClass,
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, RINO_download_timeout,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 0,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 1,
                XmNrightOffset, 5,
                XmNsensitive, TRUE,
                XmNorientation, XmHORIZONTAL,
                XmNborderWidth, 1,
                XmNminimum, 1,    // 0.5 minutes
                XmNmaximum, 30,     // 30 minutes
                XmNshowValue, TRUE,
                XmNvalue, snapshot_interval,
// Note:  Some versions of OpenMotif (distributed with Fedora,
// perhaps others) don't work properly with XtVaTypedArg() as used
// here, instead showing blank labels for the Scale widgets.
//                XtVaTypedArg, XmNtitleString, XmRString, langcode("WPUPCFTM13"), length,
                XmNtitleString, x_str,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XmStringFree(x_str);

        button_ok = XtVaCreateManagedWidget(langcode("UNIOP00001"),
                xmPushButtonGadgetClass, 
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, snapshot_interval_slider,
                XmNtopOffset, 10,
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


        button_cancel = XtVaCreateManagedWidget(langcode("UNIOP00002"),
                xmPushButtonGadgetClass, 
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, snapshot_interval_slider,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_FORM,
                XmNbottomOffset, 5,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 1,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 2,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        XtAddCallback(button_ok, XmNactivateCallback, Configure_timing_change_data, configure_timing_dialog);
        XtAddCallback(button_cancel, XmNactivateCallback, Configure_timing_destroy_shell, configure_timing_dialog);

        pos_dialog(configure_timing_dialog);

        delw = XmInternAtom(XtDisplay(configure_timing_dialog),"WM_DELETE_WINDOW", FALSE);
        XmAddWMProtocolCallback(configure_timing_dialog, delw, Configure_timing_destroy_shell, (XtPointer)configure_timing_dialog);

        XtManageChild(my_form);
        XtManageChild(pane);

        XtPopup(configure_timing_dialog,XtGrabNone);
        fix_dialog_size(configure_timing_dialog);

        // Move focus to the Close button.  This appears to highlight the
        // button fine, but we're not able to hit the <Enter> key to
        // have that default function happen.  Note:  We _can_ hit the
        // <SPACE> key, and that activates the option.
//        XmUpdateDisplay(configure_timing_dialog);
        XmProcessTraversal(button_cancel, XmTRAVERSE_CURRENT);

    } else
        (void)XRaiseWindow(XtDisplay(configure_timing_dialog), XtWindow(configure_timing_dialog));
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
                label, coord_box, coord_0, coord_1, coord_2,
                coord_3, coord_4, coord_5;
    Atom delw;
    Arg al[50];                    /* Arg List */
    register unsigned int ac = 0;           /* Arg Count */

    if (!configure_coordinates_dialog) {
        configure_coordinates_dialog = XtVaCreatePopupShell(langcode("WPUPCFC001"),
                xmDialogShellWidgetClass, appshell,
                XmNdeleteResponse, XmDESTROY,
                XmNdefaultPosition, FALSE,
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
                NULL);
        /*set args for color */
        ac=0;
        XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
        XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
        XtSetArg(al[ac], XmNfontList, fontlist1); ac++;

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
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(coord_0,XmNvalueChangedCallback,coordinates_toggle,"0");


        coord_1 = XtVaCreateManagedWidget(langcode("WPUPCFC004"),
                xmToggleButtonGadgetClass,
                coord_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(coord_1,XmNvalueChangedCallback,coordinates_toggle,"1");


        coord_2 = XtVaCreateManagedWidget(langcode("WPUPCFC005"),
                xmToggleButtonGadgetClass,
                coord_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(coord_2,XmNvalueChangedCallback,coordinates_toggle,"2");


        coord_3 = XtVaCreateManagedWidget(langcode("WPUPCFC006"),
                xmToggleButtonGadgetClass,
                coord_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(coord_3,XmNvalueChangedCallback,coordinates_toggle,"3");


        coord_4 = XtVaCreateManagedWidget(langcode("WPUPCFC008"),
                xmToggleButtonGadgetClass,
                coord_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(coord_4,XmNvalueChangedCallback,coordinates_toggle,"4");


        coord_5 = XtVaCreateManagedWidget(langcode("WPUPCFC007"),
                xmToggleButtonGadgetClass,
                coord_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(coord_5,XmNvalueChangedCallback,coordinates_toggle,"5");


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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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

            case(USE_UTM_SPECIAL):
                XmToggleButtonSetState(coord_4,TRUE,FALSE);
                break;

            case(USE_MGRS):
                XmToggleButtonSetState(coord_5,TRUE,FALSE);
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
    char *temp_ptr;


    temp_ptr = XmTextFieldGetString(audio_alarm_config_play_data);
    xastir_snprintf(sound_command,
        sizeof(sound_command),
        "%s",
        temp_ptr);
    XtFree(temp_ptr);

    (void)remove_trailing_spaces(sound_command);

    temp_ptr = XmTextFieldGetString(audio_alarm_config_play_ons_data);
    xastir_snprintf(sound_new_station,
        sizeof(sound_new_station),
        "%s",
        temp_ptr);
    XtFree(temp_ptr);

    (void)remove_trailing_spaces(sound_new_station);

    temp_ptr = XmTextFieldGetString(audio_alarm_config_play_onm_data);
    xastir_snprintf(sound_new_message,
        sizeof(sound_new_message),
        "%s",
        temp_ptr);
    XtFree(temp_ptr);

    (void)remove_trailing_spaces(sound_new_message);

    temp_ptr = XmTextFieldGetString(audio_alarm_config_play_onpx_data);
    xastir_snprintf(sound_prox_message,
        sizeof(sound_prox_message),
        "%s",
        temp_ptr);
    XtFree(temp_ptr);

    (void)remove_trailing_spaces(sound_prox_message);

    temp_ptr = XmTextFieldGetString(prox_min_data);
    xastir_snprintf(prox_min,
        sizeof(prox_min),
        "%s",
        temp_ptr);
    XtFree(temp_ptr);

    (void)remove_trailing_spaces(prox_min);

    temp_ptr = XmTextFieldGetString(prox_max_data);
    xastir_snprintf(prox_max,
        sizeof(prox_max),
        "%s",
        temp_ptr);
    XtFree(temp_ptr);

    (void)remove_trailing_spaces(prox_max);

    temp_ptr = XmTextFieldGetString(audio_alarm_config_play_onbo_data);
    xastir_snprintf(sound_band_open_message,
        sizeof(sound_band_open_message),
        "%s",
        temp_ptr);
    XtFree(temp_ptr);

    (void)remove_trailing_spaces(sound_band_open_message);

    temp_ptr = XmTextFieldGetString(bando_min_data);
    xastir_snprintf(bando_min,
        sizeof(bando_min),
        "%s",
        temp_ptr);
    XtFree(temp_ptr);

    (void)remove_trailing_spaces(bando_min);

    temp_ptr = XmTextFieldGetString(bando_max_data);
    xastir_snprintf(bando_max,
        sizeof(bando_max),
        "%s",
        temp_ptr);
    XtFree(temp_ptr);

    (void)remove_trailing_spaces(bando_max);

    temp_ptr = XmTextFieldGetString(audio_alarm_config_wx_alert_data);
    xastir_snprintf(sound_wx_alert_message,
        sizeof(sound_wx_alert_message),
        "%s",
        temp_ptr);
    XtFree(temp_ptr);

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
                xmDialogShellWidgetClass, appshell,
                XmNdeleteResponse, XmDESTROY,
                XmNdefaultPosition, FALSE,
                XmNfontList, fontlist1,
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
                XmNfractionBase, 3,
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
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 1,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        audio_alarm_config_play_data = XtVaCreateManagedWidget("Configure_audio_alarms Play Command", 
                xmTextFieldWidgetClass, 
                my_form,
                XmNeditable,   TRUE,
                XmNcursorPositionVisible, TRUE,
                XmNsensitive, TRUE,
                XmNshadowThickness,    1,
                XmNcolumns, 40,
                XmNwidth, ((40*7)+2),
                XmNmaxLength, 80,
                XmNbackground, colors[0x0f],
                XmNtopAttachment,XmATTACH_FORM,
                XmNtopOffset, 5,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 1,
                XmNrightAttachment,XmATTACH_FORM,
                XmNrightOffset, 10,
                XmNfontList, fontlist1,
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
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 1,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        file2 = XtVaCreateManagedWidget(langcode("WPUPCFA004"),
                xmLabelWidgetClass, 
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, audio_play,
                XmNtopOffset, 20,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 1,
                XmNrightAttachment, XmATTACH_FORM,
                XmNrightOffset, 10,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        audio_alarm_config_play_on_new_station = XtVaCreateManagedWidget(langcode("WPUPCFA005"),
                xmToggleButtonWidgetClass,
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, file1,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 1,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
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
                XmNleftPosition, 1,
                XmNleftWidget, audio_alarm_config_play_on_new_station,
                XmNrightAttachment,XmATTACH_FORM,
                XmNrightOffset, 10,
                XmNfontList, fontlist1,
                NULL);

        audio_alarm_config_play_on_new_message  = XtVaCreateManagedWidget(langcode("WPUPCFA006"),
                xmToggleButtonWidgetClass,
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, audio_alarm_config_play_on_new_station,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 1,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
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
                XmNleftPosition, 1,
                XmNrightAttachment,XmATTACH_FORM,
                XmNrightOffset, 10,
                XmNfontList, fontlist1,
                NULL);

        audio_alarm_config_play_on_prox  = XtVaCreateManagedWidget(langcode("WPUPCFA007"),
                xmToggleButtonWidgetClass,
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, audio_alarm_config_play_on_new_message,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset ,10,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 1,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
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
                XmNleftPosition, 1,
                XmNrightAttachment,XmATTACH_FORM,
                XmNrightOffset, 10,
                XmNfontList, fontlist1,
                NULL);

        min1 = XtVaCreateManagedWidget(langcode("WPUPCFA009"),
                xmLabelWidgetClass, 
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, audio_alarm_config_play_on_prox,
                XmNtopOffset, 8,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 1,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
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
                XmNleftPosition, 1,
                XmNrightAttachment,XmATTACH_POSITION,
                XmNrightPosition, 2,
                XmNfontList, fontlist1,
                NULL);

        min2 = XtVaCreateManagedWidget(english_units?langcode("UNIOP00004"):langcode("UNIOP00005"),
                xmLabelWidgetClass, 
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, audio_alarm_config_play_on_prox,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 2,
                XmNrightAttachment, XmATTACH_FORM,
                XmNrightOffset, 10,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        max1 = XtVaCreateManagedWidget(langcode("WPUPCFA010"),
                xmLabelWidgetClass, 
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, min1,
                XmNtopOffset, 12,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 1,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
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
                XmNleftPosition, 1,
                XmNrightAttachment,XmATTACH_POSITION,
                XmNrightPosition, 2,
                XmNfontList, fontlist1,
                NULL);

        max2 = XtVaCreateManagedWidget(english_units?langcode("UNIOP00004"):langcode("UNIOP00005"),
                xmLabelWidgetClass, 
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, min1,
                XmNtopOffset, 14,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 2,
                XmNrightAttachment, XmATTACH_FORM,
                XmNrightOffset, 10,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        audio_alarm_config_play_on_bando  = XtVaCreateManagedWidget(langcode("WPUPCFA008"),
                xmToggleButtonWidgetClass,
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, max1,
                XmNtopOffset, 12,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 1,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
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
                XmNleftPosition, 1,
                XmNrightAttachment,XmATTACH_FORM,
                XmNrightOffset, 10,
                XmNfontList, fontlist1,
                NULL);

        minb1 = XtVaCreateManagedWidget(langcode("WPUPCFA009"),
                xmLabelWidgetClass, 
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, audio_alarm_config_play_on_bando,
                XmNtopOffset, 12,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 1,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
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
                XmNleftPosition, 1,
                XmNrightAttachment,XmATTACH_POSITION,
                XmNrightPosition, 2,
                XmNfontList, fontlist1,
                NULL);

        minb2 = XtVaCreateManagedWidget(english_units?langcode("UNIOP00004"):langcode("UNIOP00005"),
                xmLabelWidgetClass, 
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, audio_alarm_config_play_on_bando,
                XmNtopOffset, 14,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 2,
                XmNrightAttachment, XmATTACH_FORM,
                XmNrightOffset, 10,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        maxb1 = XtVaCreateManagedWidget(langcode("WPUPCFA010"),
                xmLabelWidgetClass, 
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, minb1,
                XmNtopOffset, 12,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 1,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
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
                XmNleftPosition, 1,
                XmNrightAttachment,XmATTACH_POSITION,
                XmNrightPosition, 2,
                XmNfontList, fontlist1,
                NULL);

        maxb2 = XtVaCreateManagedWidget(english_units?langcode("UNIOP00004"):langcode("UNIOP00005"),
                xmLabelWidgetClass, 
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, minb1,
                XmNtopOffset, 14,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 2,
                XmNrightAttachment, XmATTACH_FORM,
                XmNrightOffset, 10,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        audio_alarm_config_play_on_wx_alert  = XtVaCreateManagedWidget(langcode("WPUPCFA011"),
                xmToggleButtonWidgetClass,
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, maxb2,
                XmNtopOffset, 12,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 1,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
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
                XmNleftPosition, 1,
                XmNrightAttachment, XmATTACH_FORM,
                XmNrightOffset, 10,
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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
                XmNleftPosition, 0,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 1,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
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
                XmNleftPosition, 2,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 3,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
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





void Test_speech(Widget widget, XtPointer clientData, XtPointer callData) {
    SayText(SPEECH_TEST_STRING);
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
    static Widget  pane, my_form, button_ok, button_cancel, file1,
        sep, button_test;
    Atom delw;

    if (!configure_speech_dialog) {
        configure_speech_dialog = XtVaCreatePopupShell(langcode("WPUPCFSP01"),
                xmDialogShellWidgetClass, appshell,
                XmNdeleteResponse, XmDESTROY,
                XmNdefaultPosition, FALSE,
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
                NULL);

        button_test = XtVaCreateManagedWidget(langcode("PULDNFI003"),
                xmPushButtonGadgetClass, 
                my_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, sep,
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

        button_ok = XtVaCreateManagedWidget(langcode("UNIOP00001"),
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
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
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
                XmNleftPosition, 4,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 5,
                XmNnavigationType, XmTAB_GROUP,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);

        XtAddCallback(button_test, XmNactivateCallback, Test_speech, configure_speech_dialog);
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
        xastir_snprintf(tracking_station_call,
            sizeof(tracking_station_call),
            "%s",
            my_callsign);
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





// Pointer to the Move/Measure cursor object
static Cursor cs_move_measure = (Cursor)NULL;



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

        // Change the cursor
            if(!cs_move_measure) {
            cs_move_measure=XCreateFontCursor(XtDisplay(da),XC_crosshair);
        }

        (void)XDefineCursor(XtDisplay(da),XtWindow(da),cs_move_measure);
        (void)XFlush(XtDisplay(da));
    }
    else {
        moving_object = 0;

        // Remove the special "crosshair" cursor
        (void)XUndefineCursor(XtDisplay(da),XtWindow(da));
        (void)XFlush(XtDisplay(da));
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

        // Change the cursor
        if(!cs_move_measure) {
            cs_move_measure=XCreateFontCursor(XtDisplay(da),XC_crosshair);
        }

        (void)XDefineCursor(XtDisplay(da),XtWindow(da),cs_move_measure);
        (void)XFlush(XtDisplay(da));
    }
    else {
        measuring_distance = 0;

        // Remove the special "crosshair" cursor
        (void)XUndefineCursor(XtDisplay(da),XtWindow(da));
        (void)XFlush(XtDisplay(da));
    }
}



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
    char old_callsign[MAX_CALLSIGN+1];
    int ok = 1;
    int temp2;
    int temp3;
    char *temp_ptr;
    char *temp_ptr2;


    transmit_compressed_posit = (int)XmToggleButtonGetState(compressed_posit_tx);

    xastir_snprintf(old_callsign,
        sizeof(old_callsign),
        "%s",
        my_callsign);

    /*fprintf(stderr,"Changing Configure station data\n");*/

    temp_ptr = XmTextFieldGetString(station_config_call_data);
    xastir_snprintf(my_callsign,
        sizeof(my_callsign),
        "%s",
        temp_ptr);
    XtFree(temp_ptr);

    (void)remove_trailing_spaces(my_callsign);
    (void)to_upper(my_callsign);
    (void)remove_trailing_dash_zero(my_callsign);

    // Enter NOCALL if there's nothing left.
    if (my_callsign[0] == '\0')
        xastir_snprintf(my_callsign,
            sizeof(my_callsign),
            "NOCALL");

    temp_ptr = XmTextFieldGetString(station_config_slat_data_ns);
    if((char)toupper((int)temp_ptr[0])=='S')
        temp[0]='S';
    else
        temp[0]='N';
    XtFree(temp_ptr);

    // Check latitude for out-of-bounds
    temp_ptr = XmTextFieldGetString(station_config_slat_data_deg);
    temp2 = atoi(temp_ptr);
    XtFree(temp_ptr);

    if ( (temp2 > 90) || (temp2 < 0) )
        ok = 0;

    temp_ptr = XmTextFieldGetString(station_config_slat_data_min);
    temp3 = atof(temp_ptr);
    XtFree(temp_ptr);

    if ( (temp3 >= 60.0) || (temp3 < 0.0) )
        ok = 0;

    if ( (temp2 == 90) && (temp3 != 0.0) )
        ok = 0;

    temp_ptr = XmTextFieldGetString(station_config_slat_data_deg);
    temp_ptr2 = XmTextFieldGetString(station_config_slat_data_min);
    xastir_snprintf(my_lat, sizeof(my_lat), "%02d%06.3f%c",
        atoi(temp_ptr),
        atof(temp_ptr2),temp[0]);
    XtFree(temp_ptr);
    XtFree(temp_ptr2);

    temp_ptr = XmTextFieldGetString(station_config_slong_data_ew);
    if((char)toupper((int)temp_ptr[0])=='E')
        temp[0]='E';
    else
        temp[0]='W';
    XtFree(temp_ptr);

    // Check longitude for out-of-bounds
    temp_ptr = XmTextFieldGetString(station_config_slong_data_deg);
    temp2 = atoi(temp_ptr);
    XtFree(temp_ptr);

    if ( (temp2 > 180) || (temp2 < 0) )
        ok = 0;

    temp_ptr = XmTextFieldGetString(station_config_slong_data_min);
    temp3 = atof(temp_ptr);
    XtFree(temp_ptr);

    if ( (temp3 >= 60.0) || (temp3 < 0.0) )
        ok = 0;

    if ( (temp2 == 180) && (temp3 != 0.0) )
        ok = 0;

    temp_ptr = XmTextFieldGetString(station_config_slong_data_deg);
    temp_ptr2 = XmTextFieldGetString(station_config_slong_data_min);
    xastir_snprintf(my_long, sizeof(my_long), "%03d%06.3f%c",
            atoi(temp_ptr),
            atof(temp_ptr2),temp[0]);
    XtFree(temp_ptr);
    XtFree(temp_ptr2);

    temp_ptr = XmTextFieldGetString(station_config_group_data);
    my_group=temp_ptr[0];
    if(isalpha((int)my_group))
        my_group = toupper((int)temp_ptr[0]);
    XtFree(temp_ptr);

    temp_ptr = XmTextFieldGetString(station_config_symbol_data);
    my_symbol = temp_ptr[0];
    XtFree(temp_ptr);

    if(isdigit((int)my_phg[3]) && isdigit((int)my_phg[4]) && isdigit((int)my_phg[5]) && isdigit((int)my_phg[6])) {
        my_phg[0] = 'P';
        my_phg[1] = 'H';
        my_phg[2] = 'G';
        my_phg[7] = '\0';
    } else
        my_phg[0]='\0';

    /* set station ambiguity*/
    position_amb_chars = Configure_station_pos_amb;

    if (transmit_compressed_posit) {
        position_amb_chars = 0;
    }

    temp_ptr = XmTextFieldGetString(station_config_comment_data);
    xastir_snprintf(my_comment,
        sizeof(my_comment),
        "%s",
        temp_ptr);
    XtFree(temp_ptr);

    (void)remove_trailing_spaces(my_comment);

    /* TO DO: KILL only my station data? */
    if (ok) {   // If entered data was valid

        // Check whether we changed our callsign
        if (strcasecmp(old_callsign,my_callsign) != 0) {
            station_del(old_callsign);  // move to new sort location...

            // If TrackMe is enabled, copy the new callsign into the
            // track_station_call variable.  If we don't do this, we
            // will still be tracking our old callsign.
            if (track_me) {
                xastir_snprintf(tracking_station_call,
                    sizeof(tracking_station_call),
                    "%s",
                    my_callsign);
            }
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
    char *temp_ptr;


    XtVaSetValues(station_config_icon, XmNlabelPixmap, CS_icon0, NULL);         // clear old icon
    XtManageChild(station_config_icon);

    temp_ptr = XmTextFieldGetString(station_config_group_data);
    group = temp_ptr[0];
    XtFree(temp_ptr);

    temp_ptr = XmTextFieldGetString(station_config_symbol_data);
    symb  = temp_ptr[0];
    XtFree(temp_ptr);

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

    //fprintf(stderr,"PHG: %s\n",my_phg);
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

    //fprintf(stderr,"PHG: %s\n",my_phg);
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

    //fprintf(stderr,"PHG: %s\n",my_phg);
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

    //fprintf(stderr,"PHG: %s\n",my_phg);
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

    //fprintf(stderr,"Trying to change a symbol\n");
    symbol_change_requested_from = 1;   // Tell Select_symbol who to return the data to
    Select_symbol(widget, clientData, callData);
}






/*
 *  Setup Configure Station dialog
 */
void Configure_station( /*@unused@*/ Widget ww, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer callData) {
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
    Arg al[50];                    /* Arg List */
    register unsigned int ac = 0;           /* Arg Count */


    if(!configure_station_dialog) {
        configure_station_dialog = XtVaCreatePopupShell(langcode("WPUPCFS001"),
                xmDialogShellWidgetClass,   appshell,
                XmNdeleteResponse,          XmDESTROY,
                XmNdefaultPosition,         FALSE,
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
                NULL);

        cs_form1 =  XtVaCreateWidget("Configure_station cs_form1",
                xmFormWidgetClass, 
                frame,
                XmNfractionBase,            5,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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
        XtSetArg(al[ac], XmNfontList, fontlist1); ac++;

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
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(poption0,XmNvalueChangedCallback,Power_toggle,"x");

        // 0 Watts
        poption1 = XtVaCreateManagedWidget("0W",
                xmToggleButtonGadgetClass,
                power_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(poption1,XmNvalueChangedCallback,Power_toggle,"0");

        // 1 Watt
        poption2 = XtVaCreateManagedWidget("1W",
                xmToggleButtonGadgetClass,
                power_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(poption2,XmNvalueChangedCallback,Power_toggle,"1");

        // 4 Watts
        poption3 = XtVaCreateManagedWidget("4W",
                xmToggleButtonGadgetClass,
                power_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(poption3,XmNvalueChangedCallback,Power_toggle,"2");

        // 9 Watts
        poption4 = XtVaCreateManagedWidget("9W",
                xmToggleButtonGadgetClass,
                power_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(poption4,XmNvalueChangedCallback,Power_toggle,"3");

        // 16 Watts
        poption5 = XtVaCreateManagedWidget("16W",
                xmToggleButtonGadgetClass,
                power_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(poption5,XmNvalueChangedCallback,Power_toggle,"4");

        // 25 Watts
        poption6 = XtVaCreateManagedWidget("25W",
                xmToggleButtonGadgetClass,
                power_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(poption6,XmNvalueChangedCallback,Power_toggle,"5");

        // 36 Watts
        poption7 = XtVaCreateManagedWidget("36W",
                xmToggleButtonGadgetClass,
                power_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(poption7,XmNvalueChangedCallback,Power_toggle,"6");

        // 49 Watts
        poption8 = XtVaCreateManagedWidget("49W",
                xmToggleButtonGadgetClass,
                power_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(poption8,XmNvalueChangedCallback,Power_toggle,"7");

        // 64 Watts
        poption9 = XtVaCreateManagedWidget("64W",
                xmToggleButtonGadgetClass,
                power_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(poption9,XmNvalueChangedCallback,Power_toggle,"8");

        // 81 Watts
        poption10 = XtVaCreateManagedWidget("81W",
                xmToggleButtonGadgetClass,
                power_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
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
        hoption1 = XtVaCreateManagedWidget(
                (english_units) ? "10ft" : "3m",
                xmToggleButtonGadgetClass,
                height_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(hoption1,XmNvalueChangedCallback,Height_toggle,"0");

        // 20 Feet
        hoption2 = XtVaCreateManagedWidget(
                (english_units) ? "20ft" : "6m",
                xmToggleButtonGadgetClass,
                height_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(hoption2,XmNvalueChangedCallback,Height_toggle,"1");

        // 40 Feet
        hoption3 = XtVaCreateManagedWidget(
                (english_units) ? "40ft" : "12m",
                xmToggleButtonGadgetClass,
                height_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(hoption3,XmNvalueChangedCallback,Height_toggle,"2");

        // 80 Feet
        hoption4 = XtVaCreateManagedWidget(
                (english_units) ? "80ft" : "24m",
                xmToggleButtonGadgetClass,
                height_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(hoption4,XmNvalueChangedCallback,Height_toggle,"3");

        // 160 Feet
        hoption5 = XtVaCreateManagedWidget(
                (english_units) ? "160ft" : "49m",
                xmToggleButtonGadgetClass,
                height_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(hoption5,XmNvalueChangedCallback,Height_toggle,"4");

        // 320 Feet
        hoption6 = XtVaCreateManagedWidget(
                (english_units) ? "320ft" : "98m",
                xmToggleButtonGadgetClass,
                height_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(hoption6,XmNvalueChangedCallback,Height_toggle,"5");

        // 640 Feet
        hoption7 = XtVaCreateManagedWidget(
                (english_units) ? "640ft" : "195m",
                xmToggleButtonGadgetClass,
                height_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(hoption7,XmNvalueChangedCallback,Height_toggle,"6");

        // 1280 Feet
        hoption8 = XtVaCreateManagedWidget(
                (english_units) ? "1280ft" : "390m",
                xmToggleButtonGadgetClass,
                height_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(hoption8,XmNvalueChangedCallback,Height_toggle,"7");

        // 2560 Feet
        hoption9 = XtVaCreateManagedWidget(
                (english_units) ? "2560ft" : "780m",
                xmToggleButtonGadgetClass,
                height_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(hoption9,XmNvalueChangedCallback,Height_toggle,"8");

        // 5120 Feet
        hoption10 = XtVaCreateManagedWidget(
                (english_units) ? "5120ft" : "1561m",
                xmToggleButtonGadgetClass,
                height_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(goption1,XmNvalueChangedCallback,Gain_toggle,"0");

        // 1 dB
        goption2 = XtVaCreateManagedWidget("1dB",
                xmToggleButtonGadgetClass,
                gain_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(goption2,XmNvalueChangedCallback,Gain_toggle,"1");

        // 2 dB
        goption3 = XtVaCreateManagedWidget("2dB",
                xmToggleButtonGadgetClass,
                gain_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(goption3,XmNvalueChangedCallback,Gain_toggle,"2");

        // 3 dB
        goption4 = XtVaCreateManagedWidget("3dB",
                xmToggleButtonGadgetClass,
                gain_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(goption4,XmNvalueChangedCallback,Gain_toggle,"3");

        // 4 dB
        goption5 = XtVaCreateManagedWidget("4dB",
                xmToggleButtonGadgetClass,
                gain_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(goption5,XmNvalueChangedCallback,Gain_toggle,"4");

        // 5 dB
        goption6 = XtVaCreateManagedWidget("5dB",
                xmToggleButtonGadgetClass,
                gain_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(goption6,XmNvalueChangedCallback,Gain_toggle,"5");

        // 6 dB
        goption7 = XtVaCreateManagedWidget("6dB",
                xmToggleButtonGadgetClass,
                gain_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(goption7,XmNvalueChangedCallback,Gain_toggle,"6");

        // 7 dB
        goption8 = XtVaCreateManagedWidget("7dB",
                xmToggleButtonGadgetClass,
                gain_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(goption8,XmNvalueChangedCallback,Gain_toggle,"7");

        // 8 dB
        goption9 = XtVaCreateManagedWidget("8dB",
                xmToggleButtonGadgetClass,
                gain_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(goption9,XmNvalueChangedCallback,Gain_toggle,"8");

        // 9 dB
        goption10 = XtVaCreateManagedWidget("9dB",
                xmToggleButtonGadgetClass,
                gain_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
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
        doption1 = XtVaCreateManagedWidget(langcode("WPUPCFS016"), // Omni
                xmToggleButtonGadgetClass,
                directivity_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(doption1,XmNvalueChangedCallback,Directivity_toggle,"0");

        // 45 NE
        doption2 = XtVaCreateManagedWidget("45°",
                xmToggleButtonGadgetClass,
                directivity_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(doption2,XmNvalueChangedCallback,Directivity_toggle,"1");

        // 90 E
        doption3 = XtVaCreateManagedWidget("90°",
                xmToggleButtonGadgetClass,
                directivity_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(doption3,XmNvalueChangedCallback,Directivity_toggle,"2");

        // 135 SE
        doption4 = XtVaCreateManagedWidget("135°",
                xmToggleButtonGadgetClass,
                directivity_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(doption4,XmNvalueChangedCallback,Directivity_toggle,"3");

        // 180 S
        doption5 = XtVaCreateManagedWidget("180°",
                xmToggleButtonGadgetClass,
                directivity_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(doption5,XmNvalueChangedCallback,Directivity_toggle,"4");

        // 225 SW
        doption6 = XtVaCreateManagedWidget("225°",
                xmToggleButtonGadgetClass,
                directivity_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(doption6,XmNvalueChangedCallback,Directivity_toggle,"5");

        // 270 W
        doption7 = XtVaCreateManagedWidget("270°",
                xmToggleButtonGadgetClass,
                directivity_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(doption7,XmNvalueChangedCallback,Directivity_toggle,"6");

        // 315 NW
        doption8 = XtVaCreateManagedWidget("315°",
                xmToggleButtonGadgetClass,
                directivity_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(doption8,XmNvalueChangedCallback,Directivity_toggle,"7");

        // 360 N
        doption9 = XtVaCreateManagedWidget("360°",
                xmToggleButtonGadgetClass,
                directivity_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
                NULL);

        /*set args for color */
        ac=0;
        XtSetArg(al[ac], XmNforeground, MY_FG_COLOR); ac++;
        XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
        XtSetArg(al[ac], XmNfontList, fontlist1); ac++;

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
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(posamb0,XmNvalueChangedCallback,Configure_station_toggle,"0");

        posamb1 = XtVaCreateManagedWidget(english_units?langcode("WPUPCFS020"):langcode("WPUPCFS024"),
                xmToggleButtonGadgetClass,
                option_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(posamb1,XmNvalueChangedCallback,Configure_station_toggle,"1");


        posamb2 = XtVaCreateManagedWidget(english_units?langcode("WPUPCFS021"):langcode("WPUPCFS025"),
                xmToggleButtonGadgetClass,
                option_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(posamb2,XmNvalueChangedCallback,Configure_station_toggle,"2");

        posamb3 = XtVaCreateManagedWidget(english_units?langcode("WPUPCFS022"):langcode("WPUPCFS026"),
                xmToggleButtonGadgetClass,
                option_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
                NULL);
        XtAddCallback(posamb3,XmNvalueChangedCallback,Configure_station_toggle,"3");

        posamb4 = XtVaCreateManagedWidget(english_units?langcode("WPUPCFS023"):langcode("WPUPCFS027"),
                xmToggleButtonGadgetClass,
                option_box,
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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
                XmNfontList, fontlist1,
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





// Borrowed from the libiberty library, a GPL'ed program.
//
void freeargv (char **vector) {
    register char **scan;

    if (vector != NULL) {
        for (scan = vector; *scan != NULL; scan++) {
            free (*scan);
        }
        free (vector);
    }
}





// Borrowed from the libiberty library, a GPL'ed program.
//
char **dupargv (char **argv) {
    int argc;
    char **copy;
  
    if (argv == NULL)
        return NULL;
  
    /* the vector */
    for (argc = 0; argv[argc] != NULL; argc++);

    copy = (char **) malloc ((argc + 1) * sizeof (char *));

    if (copy == NULL)
        return NULL;
  
    /* the strings */
    for (argc = 0; argv[argc] != NULL; argc++) {
        int len = strlen (argv[argc]);

        copy[argc] = malloc (sizeof (char *) * (len + 1));
        if (copy[argc] == NULL) {
            freeargv (copy);
            return NULL;
        }
        strcpy (copy[argc], argv[argc]);
    }
    copy[argc] = NULL;
    return copy;
}





/////////////////////////////////////////////   main   /////////////////////////////////////////////


// Third argument is now deprecated
//int main(int argc, char *argv[], char *envp[]) {

int main(int argc, char *argv[], char *envp[]) {
    int ag, ag_error, trap_segfault, deselect_maps_on_startup;
    uid_t user_id;
    struct passwd *user_info;
    static char lang_to_use_or[30];
    char temp[100];
    static char *Geometry = NULL;

    // Define some overriding resources for the widgets.
    // Look at files in /usr/X11/lib/X11/app-defaults for ideas.
    String fallback_resources[] = {

//        "Mwm*iconImageForeground: #000000000000\n",

        "*initialResourcesPersistent: False\n",

// Default font if nothing else overrides it:
        "*.fontList:    fixed\n",

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


#ifndef optarg
    extern char *optarg;
#endif  // optarg

#ifdef USING_LIBGC
    GC_find_leak = 1;
    GC_INIT();
#endif  // USING_LIBGC


    // Make copies of argc/argv/envp so that we can start other
    // processes and know the environment we were started with. 
    //
    my_argc = argc;
//    my_argv = argv;
    my_argv = dupargv(argv);
    my_envp = (void *)&envp[0];



    euid = geteuid();
    egid = getegid();

    DISABLE_SETUID_PRIVILEGE;

    program_start_time = sec_now(); // For use by "Display Uptime"

#ifdef HAVE_LIBCURL
    curl_global_init(CURL_GLOBAL_ALL);
#endif


#ifdef HAVE_GRAPHICSMAGICK
    InitializeMagick(*argv);
#else   // HAVE_GRAPHICSMAGICK
    #ifdef HAVE_IMAGEMAGICK
        #if (MagickLibVersion < 0x0538)
            MagickIncarnate(*argv);
        #else   // MagickLibVersion < 0x0538
            InitializeMagick(*argv);
        #endif  // MagickLibVersion < 0x0538
    #endif  // HAVE_IMAGEMAGICK
#endif  //HAVE_GRAPHICSMAGICK


    /* check fhs directories ?*/

    /* setup values */
    redo_list = FALSE;          // init lists

    xa_config_dir[0]='\0';

    delay_time = 0;
    last_weather_cycle = sec_now();
    redraw_on_new_packet_data = 0;
    next_file_read = sec_now();         // init file replay timing
    redraw_on_new_data = 0;
    display_up = 0;
    display_up_first = 0;
    max_transmit_time = (time_t)900l;
    sec_next_gps = 0l;
    gprmc_save_string[0] = '\0';
    gpgga_save_string[0] = '\0';
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

    last_popup_x = 0;
    last_popup_y = 0;
    trap_segfault = 0;          // Default is to dump core
    deselect_maps_on_startup = 0;
    debug_level = 0;
    install_colormap = 0;
    last_sound_pid = 0;

    my_last_course = 0;
    my_last_speed = 0;
    my_last_altitude = 0l;
    my_last_altitude_time = 0l;

    wx_station_type[0] = '\0';
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

    lang_to_use_or[0] = '\0';
    ag_error=0;

    // Reset the gps variables.
    xastir_snprintf(gps_sats,
        sizeof(gps_sats),
        "00");
    gps_valid = 0;

    memset(&appshell, 0, sizeof(appshell));

    // Here we had to add "g:" in order to allow -geometry to be
    // used, which is actually parsed out by the XtIntrinsics code,
    // not directly in Xastir code.
    //
    while ((ag = getopt(argc, argv, "c:f:v:l:g:012346789timp")) != EOF) {

        switch (ag) {
            
            case 'c': 
            	if (optarg) {
	                xastir_snprintf(xa_config_dir,sizeof(xa_config_dir),optarg);
        	        fprintf(stderr,"Using config dir %s\n",xa_config_dir);
            	}
                break;

            case 'f':   // Track callsign
                if (optarg) {
                    xastir_snprintf(temp_tracking_station_call,
                        sizeof(temp_tracking_station_call),
                        optarg);
                    fprintf(stderr,
                        "Tracking callsign %s\n",
                        temp_tracking_station_call);
                    (void)remove_leading_spaces(temp_tracking_station_call);
                    (void)remove_trailing_spaces(temp_tracking_station_call);
                    (void)remove_trailing_dash_zero(temp_tracking_station_call);
                    (void)to_upper(temp_tracking_station_call);
                }
                break;

            case 't':
                fprintf(stderr,"Internal SIGSEGV handler enabled\n");
                trap_segfault = 1;
                break;

            case 'v':
                fprintf(stderr,"debug");
                if (optarg) {
                    debug_level = atoi(optarg);
                    fprintf(stderr," level %d", debug_level);
                }
                fprintf(stderr,"\n");
                break;

            case 'l':
                fprintf(stderr,"Language is");
                if (optarg) {
                    lang_to_use_or[0] = '\0';
                    if        (strncasecmp(optarg,"ENGLISH",    7) == 0) {
                        xastir_snprintf(lang_to_use_or, sizeof(lang_to_use_or), "English");
                    } else if (strncasecmp(optarg,"DUTCH",      5) == 0) {
                        xastir_snprintf(lang_to_use_or, sizeof(lang_to_use_or), "Dutch");
                    } else if (strncasecmp(optarg,"FRENCH",     6) == 0) {
                        xastir_snprintf(lang_to_use_or, sizeof(lang_to_use_or), "French");
                    } else if (strncasecmp(optarg,"GERMAN",     6) == 0) {
                        xastir_snprintf(lang_to_use_or, sizeof(lang_to_use_or), "German");
                    } else if (strncasecmp(optarg,"SPANISH",    7) == 0) {
                        xastir_snprintf(lang_to_use_or, sizeof(lang_to_use_or), "Spanish");
                    } else if (strncasecmp(optarg,"ITALIAN",    7) == 0) {
                        xastir_snprintf(lang_to_use_or, sizeof(lang_to_use_or), "Italian");
                    } else if (strncasecmp(optarg,"PORTUGUESE",10) == 0) {
                        xastir_snprintf(lang_to_use_or, sizeof(lang_to_use_or), "Portuguese");
                    } else if (strncasecmp(optarg,"ELMERFUDD",10) == 0) {
                        xastir_snprintf(lang_to_use_or, sizeof(lang_to_use_or), "ElmerFudd");
                    } else if (strncasecmp(optarg,"MUPPETSCHEF",10) == 0) {
                        xastir_snprintf(lang_to_use_or, sizeof(lang_to_use_or), "MuppetsChef");
                    } else if (strncasecmp(optarg,"OLDEENGLISH",10) == 0) {
                        xastir_snprintf(lang_to_use_or, sizeof(lang_to_use_or), "OldeEnglish");
                    } else if (strncasecmp(optarg,"PIGLATIN",10) == 0) {
                        xastir_snprintf(lang_to_use_or, sizeof(lang_to_use_or), "PigLatin");
                    } else if (strncasecmp(optarg,"PIRATEENGLISH",10) == 0) {
                        xastir_snprintf(lang_to_use_or, sizeof(lang_to_use_or), "PirateEnglish");
                    } else {
                        ag_error++;
                        fprintf(stderr," INVALID");
                    }
                    if (!ag_error)
                        fprintf(stderr," %s", lang_to_use_or);
                    }
                fprintf(stderr,"\n");
                break;

            case 'i':
                fprintf(stderr,"Install Colormap\n");
                install_colormap = (int)TRUE;
                break;

            case 'm':
                fprintf(stderr,"De-select Maps\n");
                deselect_maps_on_startup = (int)TRUE;
                break;

            case 'g':   // -geometry
                    Geometry = argv[optind++];
                break;

            case 'p':   // Disable popups
                disable_all_popups = 1;
                pop_up_new_bulletins = 0;
                warn_about_mouse_modifiers = 0;
                break;

            default:
                ag_error++;
                break;
        }
    }


    if (ag_error){
        fprintf(stderr,"\nXastir Command line Options\n\n");
        fprintf(stderr,"-c /path/dir       Xastir config dir\n");
        fprintf(stderr,"-f callsign        Track callsign\n");
        fprintf(stderr,"-i                 Install private Colormap\n");
        fprintf(stderr,"-geometry WxH+X+Y  Set Window Geometry\n");
        fprintf(stderr,"-l Dutch           Set the language to Dutch\n");
        fprintf(stderr,"-l English         Set the language to English\n");
        fprintf(stderr,"-l French          Set the language to French\n");
        fprintf(stderr,"-l German          Set the language to German\n");
        fprintf(stderr,"-l Italian         Set the language to Italian\n");
        fprintf(stderr,"-l Portuguese      Set the language to Portuguese\n");
        fprintf(stderr,"-l Spanish         Set the language to Spanish\n");
        fprintf(stderr,"-l ElmerFudd       Set the language to ElmerFudd\n");
        fprintf(stderr,"-l MuppetsChef     Set the language to MuppetsChef\n");
        fprintf(stderr,"-l OldeEnglish     Set the language to OldeEnglish\n");
        fprintf(stderr,"-l PigLatin        Set the language to PigLatin\n");
        fprintf(stderr,"-l PirateEnglish   Set the language to PirateEnglish\n");
        fprintf(stderr,"-m                 Deselect Maps\n");
        fprintf(stderr,"-p                 Disable popups\n");
        fprintf(stderr,"-t                 Internal SIGSEGV handler enabled\n");
        fprintf(stderr,"-v level           Set the debug level\n\n");
        fprintf(stderr,"\n");
        exit(0);    // Exiting after dumping out command-line options
    }


    if (Geometry) {
        //
        // Really we should be merging with the RDB database as well
        // and then parsing the final geometry (Xlib Programming
        // Manual section 14.4.3 and 14.4.4).
        //
        geometry_flags = XParseGeometry(Geometry,
            &geometry_x,
            &geometry_y,
            &geometry_width,
            &geometry_height);

/*
        if ((WidthValue|HeightValue) & geometry_flags) {
            fprintf(stderr,"Found width/height\n");
        }
        if (XValue & geometry_flags) {
            fprintf(stderr,"Found X-offset\n");
        }
        if (YValue & geometry_flags) {
            fprintf(stderr,"Found Y-offset\n");
        }
        fprintf(stderr,
            "appshell:  Width:%4d  Height:%4d  X-offset:%4d  Y-offset:%4d\n",
            (int)geometry_width,
            (int)geometry_height,
            (int)geometry_x,
            (int)geometry_y);
*/
    }


    /* get User info */
    user_id   = getuid();
    user_info = getpwuid(user_id);
    xastir_snprintf(user_dir,
        sizeof(user_dir),
        "%s",
        user_info->pw_dir);

    /*
        fprintf(stderr,"User %s, Dir %s\n",user_info->pw_name,user_dir);
        fprintf(stderr,"User dir %s\n",get_user_base_dir(""));
        fprintf(stderr,"Data dir %s\n",get_data_base_dir(""));
    */

    /* check user directories */
    if (filethere(get_user_base_dir("")) != 1) {
        fprintf(stderr,"Making user dir\n");
        if (mkdir(get_user_base_dir(""),S_IRWXU) !=0){
                fprintf(stderr,"Fatal error making user dir '%s':\n\t%s \n", 
                    get_user_base_dir(""), strerror(errno) );

               	// Creature to feep later? 
               	// needs <libgen.h> 
                // fprintf(stderr,"Check to see if '%s' exists \n", 
                //    dirname(get_user_base_dir("")) );

            exit(errno);
        }
       
    }

    if (filethere(get_user_base_dir("config")) != 1) {
        fprintf(stderr,"Making user config dir\n");
        if (mkdir(get_user_base_dir("config"),S_IRWXU) !=0){
            fprintf(stderr,"Fatal error making user dir '%s':\n\t%s \n", 
                get_user_base_dir("config"), strerror(errno) );
            exit(errno);
        }        	
    }

    if (filethere(get_user_base_dir("data")) != 1) {
        fprintf(stderr,"Making user data dir\n");
        if (mkdir(get_user_base_dir("data"),S_IRWXU) !=0){
            fprintf(stderr,"Fatal error making user dir '%s':\n\t%s \n", 
                get_user_base_dir("data"), strerror(errno) );
            exit(errno);
        }
    }

    if (filethere(get_user_base_dir("logs")) != 1) {
        fprintf(stderr,"Making user log dir\n");
        if (mkdir(get_user_base_dir("logs"),S_IRWXU) !=0 ){
            fprintf(stderr,"Fatal error making user dir '%s':\n\t%s \n", 
                get_user_base_dir("logs"), strerror(errno) );
            exit(errno);
        }
    }

    if (filethere(get_user_base_dir("tracklogs")) != 1) {
        fprintf(stderr,"Making user tracklogs dir\n");
        if (mkdir(get_user_base_dir("tracklogs"),S_IRWXU) !=0 ){
            fprintf(stderr,"Fatal error making user dir '%s':\n\t%s \n", 
                get_user_base_dir("tracklogs"), strerror(errno) );
            exit(errno);
        }        	
    }

    if (filethere(get_user_base_dir("tmp")) != 1) {
        fprintf(stderr,"Making user tmp dir\n");
        if (mkdir(get_user_base_dir("tmp"),S_IRWXU) !=0 ){
            fprintf(stderr,"Fatal error making user dir '%s':\n\t%s \n", 
                get_user_base_dir("tmp"), strerror(errno) );
            exit(errno);
        }        	
    }

    if (filethere(get_user_base_dir("gps")) != 1) {
        fprintf(stderr,"Making user gps dir\n");
        if ( mkdir(get_user_base_dir("gps"),S_IRWXU) !=0 ){
            fprintf(stderr,"Fatal error making user dir '%s':\n\t%s \n", 
                get_user_base_dir("gps"), strerror(errno) );
            exit(errno);
        }
    }

    if (filethere(get_user_base_dir("map_cache")) != 1) {
        fprintf(stderr,"Making map_cache dir\n");
        if (mkdir(get_user_base_dir("map_cache"),S_IRWXU) !=0 ){
            fprintf(stderr,"Fatal error making user dir '%s':\n\t%s \n", 
                get_user_base_dir("map_cache"), strerror(errno) );
            exit(errno);
        }
    }


    /* done checking user dirs */





#ifdef USE_PID_FILE_CHECK

    if (pid_file_check() !=0 ){
        fprintf(stderr,"pid_file_check failed:\t%s \n", strerror(errno) );
            exit(errno);
    }

#endif 



    // initialize interfaces
    init_critical_section(&port_data_lock);   // Protects the port_data[] array of structs
    init_critical_section(&output_data_lock); // Protects interface.c:channel_data() function only
    init_critical_section(&data_lock);        // Protects global incoming_data_queue
    init_critical_section(&connect_lock);     // Protects port_data[].thread_status and port_data[].connect_status
// We should probably protect redraw_on_new_data, alert_redraw_on_update, and
// redraw_on_new_packet_data variables as well?
// Also need to protect dialogs.

#ifdef HAVE_DB
   connections_initialized = 0;
#endif // HAVE_DB

#ifdef USE_MAP_CACHE
    map_cache_init();
#endif  // USE_MAP_CACHE

    (void)bulletin_gui_init();
    (void)db_init();
    (void)draw_symbols_init();
    (void)interface_gui_init();
    (void)list_gui_init();
    (void)locate_gui_init();
    (void)geocoder_gui_init();
    (void)location_gui_init();
    (void)maps_init();
    (void)map_gdal_init();
    (void)messages_gui_init();
    (void)popup_gui_init();
    (void)track_gui_init();
    (void)view_message_gui_init();
    (void)wx_gui_init();
    (void)igate_init();

    clear_all_port_data();              // clear interface port data

    (void) signal(SIGINT,quit);         // set signal on stop
    (void) signal(SIGQUIT,quit);
    (void) signal(SIGTERM,quit);


    // Make sure that we reset to SIG_DFL handler any time we spawn
    // a child process.  This is so the child process doesn't call
    // restart() as well.  Only the main process needs to call
    // restart() on receiving a SIGHUP.  We can do this via the
    // following call:
    //
    //          (void)signal(SIGHUP,SIG_DFL);
    //
    (void) signal(SIGHUP,restart);      // Shut down/restart if SIGHUP received


#ifndef OLD_PTHREADS
    (void) signal(SIGUSR1,usr1sig);     // take a snapshot on demand
#else   // OLD_PTHREADS
#   warning ***** Old kernel detected: Disabling SIGUSR1 handler (snapshot on demand) *****
#endif  // OLD_PTHREADS

#ifdef HAVE_SIGIGNORE
    (void) sigignore(SIGPIPE);
#else   // HAVE_SIGIGNORE
    (void) signal(SIGPIPE,SIG_IGN);     // set sigpipe signal to ignore
#endif  // HAVE_SIGIGNORE

    if (trap_segfault)
        (void) signal(SIGSEGV,segfault);// set segfault signal to check


    // Load program parameters or set to default values
    load_data_or_default();


    // Start the listening socket.  If we fork it early we end up
    // with much smaller process memory allocated for it and all its
    // children.  The server will authenticate each client that
    // connects.  We'll share all of our data with the server, which
    // will send it to all connected/authenticated clients.
    // Anything transmitted by the clients will come back to us and
    // standard igating rules should apply from there.
    if (enable_server_port) {
        tcp_server_pid = Fork_TCP_server(my_argc, my_argv, my_envp);
        udp_server_pid = Fork_UDP_server(my_argc, my_argv, my_envp);
    }


    if (deselect_maps_on_startup) {
        unlink( get_user_base_dir(SELECTED_MAP_DATA) );  // Remove the selected_maps.sys file
    }

    update_units(); // set up conversion factors and strings

    /* do language links */
    if (strlen(lang_to_use_or) > 0)
        xastir_snprintf(lang_to_use,
            sizeof(lang_to_use),
            "%s",
            lang_to_use_or);

    xastir_snprintf(temp, sizeof(temp), "help/help-%s.dat", lang_to_use);
    (void)unlink(get_user_base_dir("config/help.dat"));

    // Note that this symlink will probably not fail.  It's easy to
    // create a symlink that points to nowhere.
    if (symlink(get_data_base_dir(temp),get_user_base_dir("config/help.dat")) == -1) {
        fprintf(stderr,"Error creating database link for help.dat\n");
        fprintf(stderr,
            "Couldn't create symlink: %s -> %s\n",
            get_user_base_dir("config/help.dat"),
            get_data_base_dir(temp));
        exit(0);  // Exiting 'cuz online help won't work.
    }

    xastir_snprintf(temp, sizeof(temp), "config/language-%s.sys", lang_to_use);
    (void)unlink(get_user_base_dir("config/language.sys"));

    // Note that this symlink will probably not fail.  It's easy to
    // create a symlink that points to nowhere.
    if (symlink(get_data_base_dir(temp),get_user_base_dir("config/language.sys")) == -1) {
        fprintf(stderr,"Error creating database link for language.sys\n");
        fprintf(stderr,
            "Couldn't create symlink: %s -> %s\n",
            get_user_base_dir("config/language.sys"),
            get_data_base_dir(temp));
        exit(0);    // We can't set our language, so exit.
    }

    /* (NEW) set help file area */
    xastir_snprintf(HELP_FILE,
        sizeof(HELP_FILE),
        "%s",
        "config/help.dat");

#ifdef HAVE_FESTIVAL
    /* Initialize the festival speech synthesis port */
    if (SayTextInit())
    {
        fprintf(stderr,"Error connecting to Festival speech server.\n");
        //exit(0);  // Not worth exiting just because we can't talk!
    }  
#endif  // HAVE_FESTIVAL


    /* populate the predefined object (SAR/Public service) struct */
    Populate_predefined_objects(predefinedObjects); 

    if (load_color_file()) {
        if (load_language_file(get_user_base_dir("config/language.sys"))) {
            init_device_names();                // set interface names
            clear_message_windows();
            clear_popup_message_windows();
            init_station_data();                // init station storage
            init_message_data();                // init messages
            reset_outgoing_messages();


            // If we don't make this call, we can't access Xt or
            // Xlib calls from multiple threads at the same time.
            // Note that Motif from the OSF (including OpenMotif)
            // still can't handle multiple threads talking to it at
            // the same time.  See:
            // http://www.faqs.org/faqs/x-faq/part7/section-46.html
            // We'll probably have to add in a global mutex lock in
            // order to keep from accessing the widget set from more
            // than one thread.
            //
            XInitThreads();


            // Set language attribs.  Must be called prior to
            // XtVaOpenApplication().
            (void)XtSetLanguageProc((XtAppContext) NULL,
                (XtLanguageProc) NULL,
                (XtPointer) NULL );


            // This convenience function calls (in turn):
            //      XtToolkitInitialize()
            //      XtCreateApplicationContext()
            //      XtOpenDisplay()
            //      XtAppCreateShell().
            //
            appshell = XtVaOpenApplication(
                &app_context,
                "Xastir",
                NULL,
                0,
                &argc,
                argv,
                fallback_resources,
                applicationShellWidgetClass,
                XmNmappedWhenManaged, FALSE,
                NULL);

            display = XtDisplay(appshell);

            if (!display) {
                fprintf(stderr,"%s: can't open display, exiting...\n", argv[0]);
                exit(-1);   // Must exit here as we can't get our display.
            }

            XtSetValues(XmGetXmDisplay(display), NULL, 0);

            // DK7IN: now scanf and printf use "," instead of "."
            // that leads to several problems in the initialization.
            //
            // DK7IN: inserted next line here for avoiding scanf
            // errors during init!
            //
            (void)setlocale(LC_NUMERIC, "C");       // DK7IN: It's now ok


            setup_visual_info(display, DefaultScreen(display));


            // Get colormap (N7TAP: do we need this if the screen
            // visual is TRUE or DIRECT?
            //
            cmap = DefaultColormapOfScreen(XtScreen(appshell));
            if (visual_type == NOT_TRUE_NOR_DIRECT) {
                if (install_colormap) {
                    cmap = XCopyColormapAndFree(display, cmap);
                    XtVaSetValues(appshell, XmNcolormap, cmap, NULL);
                }
            }


//fprintf(stderr,"***index_restore_from_file\n");

            // Read the current map index file into the index linked list
            index_restore_from_file();


            // Reload tactical calls.  This implements persistence
            // for this type.
            reload_tactical_calls();


//fprintf(stderr,"***create_appshell\n");


            // This call fills in the top-level window and then
            // calls XtRealize.
            //
            create_appshell(display, argv[0], argc, argv);


            // reset language attribs for numeric, program needs
            // decimal in US for all data!
            (void)setlocale(LC_NUMERIC, "C");
            // DK7IN: now scanf and printf work as wanted...


//fprintf(stderr,"***check_fcc_data\n");

            /* check for ham databases */
            (void)check_rac_data();
            (void)check_fcc_data();


            // Find the extents of every map we have.  Use the smart
            // timestamp-checking reindexing (quicker).
            if ( index_maps_on_startup ) {
              map_indexer(0);
            }


            // Check whether we're running Xastir for the first time.
            // If so, my_callsign will be "NOCALL".   In this case
            // write "worldhi.map" into ~/.xastir/config/selected_maps.sys
            // so that we get the default map on startup.  Also
            // request to bring up the Configure->Station dialog in
            // this case.
            //
            if (strncasecmp(my_callsign,"NOCALL",6) == 0) {
                FILE *ff;
 
//                fprintf(stderr,"***** First time run *****\n");

                // Set the flag
                first_time_run = 1;
 
                // Write the default map into the selected map file
                ff = fopen( get_user_base_dir(SELECTED_MAP_DATA), "a");
                if (ff != NULL) {
                    fprintf(ff,"worldhi.map\n");
                    (void)fclose(ff);
                }
            }


            // Mark the "selected" field in the in-memory map index
            // to correspond to the selected_maps.sys file.
            map_chooser_init();


            // Warn the user if altnet is enabled on startup.  This
            // is so that the people who are button pushers/knob turners
            // will know what's wrong when they don't see stations on their
            // screen anymore.
            //
            if (altnet) {
                // "Warning"
                // "Altnet is enabled (File->Configure->Defaults dialog)");
                popup_message_always( langcode("POPEM00035"),
                    langcode("POPEM00051") );
            }


            // Start UpdateTime.  It schedules itself to be run
            // again each time.  This is also the process that
            // starts up the interfaces.
            UpdateTime( (XtPointer) da , (XtIntervalId) NULL );


            // Update the logging indicator 
            Set_Log_Indicator();

 
            XtAppMainLoop(app_context);


        } else
            fprintf(stderr,"Error in language file! Exiting...\n");

    } else
        fprintf(stderr,"Error in Color file! Exiting...\n");

    quit(0);
    return 0;
}


