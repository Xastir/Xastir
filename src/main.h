/*
 * $Id$
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000,2001,2002  The Xastir Group
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
#endif


extern char altnet_call[];
extern int altnet;
extern Widget iface_da;
extern FILE *read_file_ptr;

#define VERSIONFRM  (!altnet?XASTIR_TOCALL:altnet_call) /* Packet version info */


#ifdef __LCLINT__
#define PACKAGE "xastir"
#define VERSION "lclint"
#define VERSIONTXT "xastir lclint debug version"
#else
#define VERSIONTXT PACKAGE " " VERSION
#endif  // __LCLINT__


#define VERSIONLABEL VERSIONTXT
#define VERSIONMESSAGE "XASTIR Version: " VERSION "\n\nAmateur Station Tracking and Information Reporting\nby Frank Giannandrea, KC2GJS was KC0DGE\n   Code added by:\n   Richard Hagemeyer - VE3UNW,  Curt Mills - WE7U,\n   Mike Sims - KA9KIM,   Gerald Stueve - KE4NFJ,\n   Mark Grennan - KD5AMB,  Henk de Groot - PE1DNN,\n   Jim Sevilla - KD6VPE,  Jose R. Marte A. - HI8GN,\n   Michael G. Petry - N3NYN,   Lloyd Miller - VE6LFM,\n   Alessandro Frigeri - IK0YUP,\n   Chuck Byam - KG4IJB,   Rolf Bleher - DK7IN,   Ken Koster - N7IPB"

#define ABOUTGNUL "XASTIR, Copyright (C) 1999, 2000 Frank Giannandrea\nXASTIR comes with ABSOLUTELY NO WARRANTY;\nThis is free software, and you are welcome\nto redistribute it under certain conditions;\nsee the GNU LICENSE for details.\n"

#define MAX_CALL    20
#define MAX_PHG     8
#define MAX_COMMENT 80

#define MAX_UNPROTO 100
#define MAX_LINE_SIZE 300

#define MAX_TNC_CHARS 128
#define MAX_NET_CHARS 128
#define MAX_GPS_CHARS 128
#define MAX_WX_CHARS 4

extern int debug_level;
extern int transmit_now;
extern char DATABASE_FILE[];
extern char DATABASE_POINTER_FILE[];
extern char DATABASE_POINTER_TEMP[];
extern char ALERT_MAP_DIR[];
extern char WIN_MAP_DIR[];
extern char WIN_MAP_DATA[];
extern char AUTO_MAP_DIR[];
extern char SYMBOLS_DIR[];
extern char LOGFILE_IGATE[];
extern char LOGFILE_TNC[];
extern char LOGFILE_NET[];
extern char LOGFILE_WX[];
extern char HELP_FILE[];
extern char SOUND_DIR[];
extern time_t WX_ALERTS_REFRESH_TIME;
extern time_t gps_time;
extern time_t POSIT_rate;
extern int output_station_type;
extern int symbol_display_enable;
extern int symbol_display_rotate;
extern int symbol_alt_display;
extern int symbol_course_display;
extern int symbol_speed_display;
extern int speed_display_enable;
extern int speed_display_short;
extern int symbol_dist_course_display;
extern int symbol_weather_display;
extern int symbol_display;
extern int symbol_rotate;
extern int wx_display_enable;
extern int wx_display_short;
extern int colors[256];
extern int max_trail_colors;
extern int trail_colors[32];
extern int current_trail_color;
extern int station_trails;
extern int units_english_metric;
extern int redraw_on_new_data;
extern int redo_list;
extern int operate_as_an_igate;
extern int transmit_raw_wx;
extern int transmit_compressed_posit;
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
extern int show_phg;
extern int show_DF;
extern int show_phg_mobiles;
extern int show_phg_default;
extern int show_amb;
extern int show_old_data;
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
extern char bando_min[];
extern char bando_max[];
extern int Display_packet_data_type;
extern int menu_x;
extern int menu_y;
extern long my_last_altitude;
extern time_t my_last_altitude_time;
extern int my_last_course;
extern int my_last_speed;
extern unsigned igate_msgs_tx;
extern int symbols_loaded;
extern GC gc2;
extern GC gc_tint;
extern GC gc_stipple;
extern int read_file;
extern float x_screen_distance;
extern time_t max_transmit_time;
extern int transmit_disable;
extern int posit_tx_disable;
extern int object_tx_disable;

extern int coordinate_system;
#define USE_DDDDDD      0
#define USE_DDMMMM      1
#define USE_DDMMSS      2
#define USE_UTM         3

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
extern void create_image(Widget w);

extern void locate_gui_init(void);
extern void location_gui_init(void);
extern void view_message_gui_init(void);
extern void wx_gui_init(void);
extern long get_x_scale(long x, long y, long ysc);

extern Widget Display_data_dialog;
extern Widget Display_data_text;
extern Widget text3;
extern Widget text4;
extern void display_zoom_status(void);
extern void statusline(char *status_text,int update);
extern int SayTextInit();
extern int SayText();
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
if (debug_level & 4) { fprintf(stderr, "Changing euid to %d and egid to %d\n", getuid(), getgid()); } \
} while(0)
#define ENABLE_SETUID_PRIVILEGE do { \
seteuid(euid); \
setegid(egid); \
if (debug_level & 4) { fprintf(stderr, "Changing euid to %d and egid to %d\n", euid, egid); } \
} while(0)

#endif /* XASTIR_MAIN_H */

