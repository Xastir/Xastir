/*
 * $Id$
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2003  The Xastir Group
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

#ifndef XASTIR_MAIN_H
#define XASTIR_MAIN_H

#include <X11/Intrinsic.h>


// For mutex debugging with Linux threads only
#ifdef __linux__
  #define MUTEX_DEBUG 1
#endif  // __linux__


// To enable the "Transmit Raw WX data" button in
// Configure->Defaults dialog.  Warning:  If you enable this, enable
// the button in Configure->Defaults, and are running a weather
// station that puts out raw data that is NOT allowed by the APRS
// spec, you'll be putting out garbage and trashing the channel.
//
//#define TRANSMIT_RAW_WX


extern char altnet_call[];
extern int altnet;
extern Widget iface_da;
extern FILE *read_file_ptr;
extern int interrupt_drawing_now;

 
#define VERSIONFRM  (!altnet?XASTIR_TOCALL:altnet_call) /* Packet version info */


#ifdef __LCLINT__
#define PACKAGE "xastir"
#define VERSION "lclint"
#define VERSIONTXT "xastir lclint debug version"
#else   // __LCLINT__
#define VERSIONTXT PACKAGE " " VERSION
#endif  // __LCLINT__


#define VERSIONLABEL VERSIONTXT
#define VERSIONMESSAGE "XASTIR Version: " VERSION "\n\nAmateur Station Tracking and Information Reporting\nby Frank Giannandrea, KC2GJS was KC0DGE\n   Code added by:\n   Richard Hagemeyer - VE3UNW,  Curt Mills - WE7U,\n   Mike Sims - KA9KIM,   Gerald Stueve - KE4NFJ,\n   Mark Grennan - KD5AMB,  Henk de Groot - PE1DNN,\n   Jim Sevilla - KD6VPE,  Jose R. Marte A. - HI8GN,\n   Michael G. Petry - N3NYN,   Lloyd Miller - VE6LFM,\n   Alessandro Frigeri - IK0YUP,\n   Chuck Byam - KG4IJB,   Rolf Bleher - DK7IN,   Ken Koster - N7IPB"

#define ABOUTGNUL "XASTIR, Copyright (C) 1999, 2000 Frank Giannandrea\nXASTIR comes with ABSOLUTELY NO WARRANTY;\nThis is free software, and you are welcome\nto redistribute it under certain conditions;\nsee the GNU LICENSE for details.\n"

#define MAX_CALL    20
#define MAX_PHG     8
#define MAX_COMMENT 80

#define MAX_UNPROTO 100

#define MAX_TNC_CHARS 128
#define MAX_NET_CHARS 128
#define MAX_GPS_CHARS 128
#define MAX_WX_CHARS 4

extern int re_sort_maps;
extern Widget trackme_button;
extern int debug_level;
extern int my_position_valid;
extern int using_gps_position;
extern int transmit_now;
extern char DATABASE_FILE[];
extern char DATABASE_POINTER_FILE[];
extern char DATABASE_POINTER_TEMP[];
extern char ALERT_MAP_DIR[];
extern char SELECTED_MAP_DIR[];
extern char SELECTED_MAP_DATA[];
extern char MAP_INDEX_DATA[];
extern char AUTO_MAP_DIR[];
extern char SYMBOLS_DIR[];
extern char LOGFILE_IGATE[];
extern char LOGFILE_TNC[];
extern char LOGFILE_NET[];
extern char LOGFILE_WX[];
extern char HELP_FILE[];
extern char SOUND_DIR[];
extern time_t WX_ALERTS_REFRESH_TIME;
extern time_t remove_ID_message_time;
extern int pending_ID_message;
extern time_t gps_time;
extern time_t POSIT_rate;
extern time_t OBJECT_rate;
extern time_t update_DR_rate;

extern time_t posit_last_time;
extern time_t posit_next_time;

extern int smart_beaconing;
extern int sb_POSIT_rate;
extern int sb_last_heading;
extern int sb_current_heading;
extern time_t sb_last_posit_time;
extern int sb_turn_min;
extern int sb_turn_slope;
extern int sb_turn_time;
extern int sb_posit_fast;
extern int sb_posit_slow;
extern int sb_low_speed_limit;
extern int sb_high_speed_limit;

extern int pop_up_new_bulletins;
extern int view_zero_distance_bulletins;
extern int warn_about_mouse_modifiers;

extern int output_station_type;

typedef struct _selections {
    int none;
    int mine;
    int tnc;
    int direct;
    int via_digi;
    int net;
    int old_data;

    int stations;
    int fixed_stations;
    int moving_stations;
    int weather_stations;
    int objects;
    int weather_objects;
    int gauge_objects;
    int other_objects;
} Selections;
extern Selections Select_;

typedef struct _what_to_display {
    int callsign;
    int symbol;
    int symbol_rotate;
    int trail;

    int course;
    int speed;
    int speed_short;
    int altitude;

    int weather;
    int weather_text;
    int temperature_only;
    int wind_barb;

    int ambiguity;
    int phg;
    int default_phg;
    int phg_of_moving;

    int df_data;
    int dr_data;
    int dr_arc;
    int dr_course;
    int dr_symbol;

    int dist_bearing;
    int last_heard;
} What_to_display;
extern What_to_display Display_;


extern int colors[256];
extern int max_trail_colors;
extern int trail_colors[32];
extern int current_trail_color;
extern int units_english_metric;
extern int do_dbstatus;
extern int redraw_on_new_data;
extern int redo_list;
extern int operate_as_an_igate;

#ifdef TRANSMIT_RAW_WX
extern int transmit_raw_wx;
#endif  // TRANSMIT_RAW_WX

extern int transmit_compressed_posit;
extern int transmit_compressed_objects_items;
extern int log_igate;
extern int log_tnc_data;
extern int log_net_data;
extern int log_wx;
extern int snapshots_enabled;
extern char user_dir[];
extern char lang_to_use[];
extern char my_group;
extern char my_symbol;
extern char my_phg[];
extern char my_comment[];
extern int map_background_color;
extern int map_color_fill;
extern int letter_style;
extern int wx_alert_style;
extern time_t map_refresh_interval;
extern time_t map_refresh_time;
extern char sound_command[];
extern pid_t last_sound_pid;
extern int sound_play_new_station;
extern int sound_play_new_message;
extern int sound_play_prox_message;
extern int sound_play_band_open_message;
extern int sound_play_wx_alert_message;
extern char sound_new_station[];
extern char sound_new_message[];
extern char sound_prox_message[];
extern char sound_band_open_message[];
extern char sound_wx_alert_message[];
extern int ATV_screen_ID;
extern int festival_speak_ID;
extern int festival_speak_new_station;
extern int festival_speak_proximity_alert;
extern int festival_speak_tracked_proximity_alert;
extern int festival_speak_band_opening;
extern int festival_speak_new_message_alert;
extern int festival_speak_new_message_body;
extern int festival_speak_new_weather_alert;
extern char prox_min[];
extern char prox_max[];
extern time_t sec_old;
extern time_t sec_clear;
extern int dead_reckoning_timeout;
extern char bando_min[];
extern char bando_max[];
extern int Display_packet_data_type;
extern int menu_x;
extern int menu_y;
extern long my_last_altitude;
extern time_t my_last_altitude_time;
extern int my_last_course;
extern int my_last_speed;   // in knots
extern unsigned igate_msgs_tx;
extern int symbols_loaded;
extern GC gc2;
extern GC gc_tint;
extern GC gc_stipple;
extern GC gc_bigfont;
extern int read_file;
extern float x_screen_distance;
extern time_t max_transmit_time;
extern int transmit_disable;
extern int posit_tx_disable;
extern int object_tx_disable;
extern int map_chooser_expand_dirs;

extern int coordinate_system;
#define USE_DDDDDD      0
#define USE_DDMMMM      1
#define USE_DDMMSS      2
#define USE_UTM         3

extern void HandlePendingEvents(XtAppContext app);
extern void create_gc(Widget w);
extern void Station_info(Widget w, XtPointer clientData, XtPointer calldata);
extern void Station_List(Widget w, XtPointer clientData, XtPointer calldata);
extern void Window_Quit(Widget w, XtPointer client, XtPointer call);
extern void Tracks_All_Clear(Widget w, XtPointer clientData, XtPointer callData);
extern void Locate_station(Widget w, XtPointer clientData, XtPointer callData);
extern void Locate_place(Widget w, XtPointer clientData, XtPointer callData);
extern void Display_Wx_Alert(Widget w, XtPointer clientData, XtPointer callData);
extern void Auto_msg_option(Widget w, XtPointer clientData, XtPointer calldata);
extern void Auto_msg_set(Widget w, XtPointer clientData, XtPointer calldata);
extern void Bulletins(Widget w, XtPointer clientData, XtPointer callData);
extern void on_off_switch(int switchpos, Widget first, Widget second);
extern void busy_cursor(Widget w);
extern void pos_dialog(Widget w);
extern int create_image(Widget w);
extern void draw_tiger_map(Widget w);

extern void locate_gui_init(void);
extern void location_gui_init(void);
extern void view_message_gui_init(void);
extern void wx_gui_init(void);
extern long get_x_scale(long x, long y, long ysc);

extern Widget Display_data_dialog;
extern Widget Display_data_text;
extern Widget text3;
extern Widget text4;
extern Widget log_indicator;
extern void display_zoom_status(void);
extern void statusline(char *status_text,int update);
extern int SayTextInit(void);
extern int SayText(char *text);
extern Widget auto_msg_toggle;

// Symbol update stuff
extern Widget configure_station_dialog;
extern Widget station_config_group_data;
extern Widget station_config_symbol_data;
extern void updateSymbolPictureCallback(Widget w,XtPointer clientData,XtPointer callData);

extern Widget object_dialog;
extern Widget object_group_data;
extern Widget object_symbol_data;
extern void updateObjectPictureCallback(Widget w,XtPointer clientData,XtPointer callData);


// unit conversion
extern char un_alt[2+1];
extern char un_dst[2+1];
extern char un_spd[4+1];
extern double cvt_m2len;
extern double cvt_dm2len;
extern double cvt_hm2len;
extern double cvt_kn2len;
extern double cvt_mi2len;

// euid and egid
extern uid_t euid;
extern gid_t egid;

/* JMT - works in FreeBSD */
#define DISABLE_SETUID_PRIVILEGE do { \
seteuid(getuid()); \
setegid(getgid()); \
if (debug_level & 4) { fprintf(stderr, "Changing euid to %d and egid to %d\n", (int)getuid(), (int)getgid()); } \
} while(0)
#define ENABLE_SETUID_PRIVILEGE do { \
seteuid(euid); \
setegid(egid); \
if (debug_level & 4) { fprintf(stderr, "Changing euid to %d and egid to %d\n", (int)euid, (int)egid); } \
} while(0)

#ifdef HAVE_LIBSHP
extern void create_map_from_trail(char *call_sign);
#endif  // HAVE_LIBSHP


#endif /* XASTIR_MAIN_H */


