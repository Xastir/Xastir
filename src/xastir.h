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

/* All of the misc entry points to be included for all packages */

#ifndef _XASTIR_H
#define _XASTIR_H



// Macros that help us avoid warnings on 64-bit CPU's.
// Borrowed from the freeciv project (also a GPL project).
#define INT_TO_XTPOINTER(m_i)  ((XtPointer)((long)(m_i)))
#define XTPOINTER_TO_INT(m_p)  ((int)((long)(m_p)))


// Defines we can use to mark functions and parameters as "unused" to the compiler
#ifdef __GNUC__
  #define UNUSED(x) UNUSED_ ## x __attribute__((__unused__))
#else
  #define UNUSED(x) UNUSED_ ## x
#endif

#ifdef __GNUC__
  #define UNUSED_FUNCTION(x) __attribute__((__unused__)) UNUSED_ ## x
#else
  #define UNUSED_FUNCTION(x) UNUSED_ ## x
#endif


#define SERIAL_KISS_RELAY_DIGI


#include <X11/Intrinsic.h>

//#include "database.h"
#include "util.h"
#include "messages.h"
#include "fcc_data.h"
#include "rac_data.h"


#define MAX_CALLSIGN 9       // Objects are up to 9 chars


// black
#define MY_FG_COLOR             colors[0x08]
#define MY_FOREGROUND_COLOR     XmNforeground,colors[0x08]
// gray73
#define MY_BG_COLOR             colors[0xff]
#define MY_BACKGROUND_COLOR     XmNbackground,colors[0xff]

// Latitude and longitude string formats.
#define CONVERT_HP_NORMAL       0
#define CONVERT_HP_NOSP         1
#define CONVERT_LP_NORMAL       2
#define CONVERT_LP_NOSP         3
#define CONVERT_DEC_DEG         4
#define CONVERT_UP_TRK          5
#define CONVERT_DMS_NORMAL      6
#define CONVERT_VHP_NOSP        7
#define CONVERT_DMS_NORMAL_FORMATED      8
#define CONVERT_HP_NORMAL_FORMATED       9

#ifndef M_PI                      /* if not defined in math.h */
  #define M_PI 3.14159265358979323846
#endif  // M_PI

#define SPEECH_TEST_STRING      "Greeteengz frum eggzaster"

/* GLOBAL DEFINES */
extern char dangerous_operation[200];
extern GC gc;
extern Pixmap  pixmap;
extern Pixmap  pixmap_final;
extern Pixmap  pixmap_alerts;
extern Pixmap  pixmap_50pct_stipple;
extern Pixmap  pixmap_25pct_stipple;
extern Pixmap  pixmap_13pct_stipple;
extern Pixmap  pixmap_wx_stipple;

extern Widget appshell;


extern int wait_to_redraw;
/*extern char my_callsign[MAX_CALLSIGN+1];*/

extern void pad_callsign(char *callsignout, char *callsignin);



extern char *to_upper(char *data);

extern void Send_message(Widget w, XtPointer clientData, XtPointer callData);

extern void create_gc(Widget w);

extern void Station_info(Widget w, XtPointer clientData, XtPointer calldata);

extern void fix_dialog_size(Widget w);
extern void fix_dialog_vsize(Widget w);

extern int debug_level;
extern GC gc;
extern Pixel colors[];


extern float f_center_longitude;   // Floating point map center longitude
extern float f_center_latitude;    // Floating point map center latitude
extern float f_NW_corner_longitude;// longitude of NW corner
extern float f_NW_corner_latitude; // latitude of NW corner
extern float f_SE_corner_longitude;// longitude of SE corner
extern float f_SE_corner_latitude; // latitude of SE corner

extern long center_longitude;      // Longitude at center of map
extern long center_latitude;       // Latitude at center of map
extern long NW_corner_longitude;   // longitude of NW corner
extern long NW_corner_latitude;    // latitude of NW corner
extern long SE_corner_longitude;   // longitude of SE corner
extern long SE_corner_latitude;    // latitude of SE corner

extern long scale_x;               // x scaling in 1/100 sec per pixel
extern long scale_y;               // y scaling in 1/100 sec per pixel


extern long screen_width;
extern long screen_height;
extern Position screen_x_offset;
extern Position screen_y_offset;
extern int long_lat_grid;
//extern Pixmap  pixmap;
//extern Pixmap  pixmap_final;
//extern Pixmap  pixmap_alerts;
extern int map_color_levels;
extern int map_labels;
extern int map_lock_pan_zoom;
extern int map_auto_maps;
extern int auto_maps_skip_raster;
extern time_t sec_remove;
extern Widget da;
extern Widget text;
extern XtAppContext app_context;
extern int redraw_on_new_data;
//extern Widget hidden_shell;
extern int index_maps_on_startup;
#define MAX_LABEL_FONTNAME 256
#define FONT_SYSTEM  0
#define FONT_STATION 1
#define FONT_TINY    2
#define FONT_SMALL   3
#define FONT_MEDIUM  4
#define FONT_LARGE   5
#define FONT_HUGE    6
#define FONT_BORDER  7
#define FONT_ATV_ID  8
#define FONT_MAX     9
#define FONT_DEFAULT FONT_MEDIUM
#define MAX_FONTNAME 256
extern char rotated_label_fontname[FONT_MAX][MAX_LABEL_FONTNAME];

#ifdef HAVE_LIBGEOTIFF
  extern int DRG_XOR_colors;
  extern int DRG_show_colors[13];
#endif  // HAVE_LIBGEOTIFF


extern int net_map_timeout;

extern void sort_list(char *filename,int size, Widget list, int *item);
extern void redraw_symbols(Widget w);

extern Colormap cmap;



/* from messages.c */
extern char  message_counter[5+1];
extern int  auto_reply;
extern char auto_reply_message[100];
extern int  satellite_ack_mode;
extern void clear_outgoing_messages(void);
extern void reset_outgoing_messages(void);
extern void output_message(char *from, char *to, char *message, char *path);
extern void check_and_transmit_messages(time_t time);
extern Message_Window mw[MAX_MESSAGE_WINDOWS+1];
extern void clear_message_windows(void);

/* from sound.c */
extern pid_t play_sound(char *sound_cmd, char *soundfile);
extern int sound_done(void);

/* from fcc_data.c */



/* from rac_data.c */

/* from lang.c */
extern int load_language_file(char *filename);
extern char *langcode(char *code);
extern char langcode_hotkey(char *code);

/* from sound.c */
extern pid_t play_sound(char *sound_cmd, char *soundfile);

/* from location.c */
extern void set_last_position(void);
extern void map_pos_last_position(void);

/* from location_gui.c */
extern char locate_station_call[30];
extern void Last_location(Widget w, XtPointer clientData, XtPointer callData);
extern void Jump_location(Widget w, XtPointer clientData, XtPointer callData);
extern void map_pos(long mid_y, long mid_x, long sz);
extern char locate_gnis_filename[200];

// This needs to be quite long for some of the weather station
// serial data to get through ok (Peet Bros U2k Complete Record Mode
// for one).
#define MAX_LINE_SIZE 512

// from main.c
extern char gprmc_save_string[MAX_LINE_SIZE+1];
extern char gpgga_save_string[MAX_LINE_SIZE+1];
extern int gps_port_save;

// from map.c
extern double calc_dscale_x(long x, long y);

/* from popup_gui.c */
extern void popup_message_always(char *banner, char *message);
extern void popup_message(char *banner, char *message);
extern void popup_ID_message(char *banner, char *message);


/* from view_messages.c */
extern void all_messages(char from, char *call_sign, char *from_call, char *message);
extern void view_all_messages(Widget w, XtPointer clientData, XtPointer callData);

/* from db.c */
extern void setup_in_view(void);

#endif /* XASTIR_H */


