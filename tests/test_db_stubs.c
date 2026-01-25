/*
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 2025-2026 The Xastir Group
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

/* 
 * Stub implementations for symbols referenced by db.o
 * but not used by the unit tests.
 * 
 * These stubs allow us to link with the real db.o for testing
 * without pulling in the entire Xastir codebase.
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "tests/test_framework.h"

#define MAX_CALLSIGN 9       // Objects are up to 9 chars

/* Add stub implementations for all the undefined symbols */
/* Note: These are intentionally empty/abort to catch any unexpected usage */

/* Global variables that db.o references */
int debug_level = 0;
char my_callsign[MAX_CALLSIGN+1] = "TEST";
int altnet = 0;
int track_case = 0;
int track_match = 0;
int track_station_on = 0;
char tracking_station_call[100] = "";
int trail_segment_distance = 0;
int trail_segment_time = 0;
int transmit_disable = 0;
int transmit_now = 0;
int wait_to_redraw = 0;

/* Stub functions */
STUB_IMPL(alert_build_list)
STUB_IMPL(alert_match)
STUB_IMPL(alert_on_alert_list)
STUB_IMPL(all_messages)
STUB_IMPL(bulletin_message_check)
STUB_IMPL(draw_area)
STUB_IMPL(draw_labeled_area)
STUB_IMPL(output_my_data)
STUB_IMPL(request_new_image)
STUB_IMPL(schedule_reboot)
STUB_IMPL(setup_message_data)
STUB_IMPL(sound_play)
STUB_IMPL(transmit_message_data)
STUB_IMPL(transmit_message_data_delayed)
STUB_IMPL(track_station)
STUB_IMPL(valid_call)
STUB_IMPL(valid_inet_name)
STUB_IMPL(valid_object)
STUB_IMPL(valid_path)
STUB_IMPL(split_string)
STUB_IMPL(statusline)
STUB_IMPL(begin_critical_section)
STUB_IMPL(bulletin_data_add)
STUB_IMPL(calc_distance_course)
STUB_IMPL(check_popup_window)
STUB_IMPL(clear_acked_message)
STUB_IMPL(convert_bearing_to_name)
STUB_IMPL(convert_lat_l2s)
STUB_IMPL(convert_lat_s2l)
STUB_IMPL(convert_lon_l2s)
STUB_IMPL(convert_lon_s2l)
STUB_IMPL(create_garmin_waypoint)
STUB_IMPL(decode_Peet_Bros)
STUB_IMPL(decode_U2000_L)
STUB_IMPL(decode_U2000_P)
STUB_IMPL(disown_object_item)
STUB_IMPL(display_station)
STUB_IMPL(distance_from_my_station)
STUB_IMPL(draw_trail)
STUB_IMPL(end_critical_section)
STUB_IMPL(filecreate)
STUB_IMPL(fill_in_new_alert_entries)
STUB_IMPL(get_iso_datetime)
STUB_IMPL(get_send_message_path)
STUB_IMPL(get_tactical_from_hash)
STUB_IMPL(get_time)
STUB_IMPL(get_timestamp)
STUB_IMPL(get_user_base_dir)
STUB_IMPL(get_w3cdtf_datetime)
STUB_IMPL(insert_into_heard_queue)
STUB_IMPL(is_local_interface)
STUB_IMPL(is_network_interface)
STUB_IMPL(is_num_chr)
STUB_IMPL(is_num_or_sp)
STUB_IMPL(is_xnum_or_dash)
STUB_IMPL(langcode)
STUB_IMPL(Locate_station)
STUB_IMPL(log_data)
STUB_IMPL(log_tactical_call)
STUB_IMPL(look_for_open_group_data)
STUB_IMPL(makePrintable)
STUB_IMPL(play_sound)
STUB_IMPL(popup_message)
STUB_IMPL(popup_message_always)
STUB_IMPL(port_write_string)
STUB_IMPL(position_defined)
STUB_IMPL(remove_leading_spaces)
STUB_IMPL(remove_trailing_asterisk)
STUB_IMPL(remove_trailing_spaces)
STUB_IMPL(SayText)
STUB_IMPL(spell_it_out)
STUB_IMPL(sec_now)
STUB_IMPL(send_agwpe_packet)
STUB_IMPL(send_ax25_frame)
STUB_IMPL(stations_types)
STUB_IMPL(output_igate_net)
STUB_IMPL(output_igate_rf)
STUB_IMPL(output_message)
STUB_IMPL(output_nws_igate_rf)

/* Functions we actually need - not stubs */
void substr(char *dest, char *src, int size)
{
    /* Simple implementation - copy size characters */
    int i;
    for (i = 0; i < size && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    dest[i] = '\0';
}

char *to_upper(char *data)
{
    /* Simple stub - just return data unchanged for now */
    return data;
}


/* Widget and display related globals */
void *Display_ = NULL;
void *Display_data_dialog = NULL;
void *Display_data_text = NULL;
int Display_packet_data_mine_only = 0;
int Display_packet_data_type = 0;
void *LOGFILE_MESSAGE = NULL;
void *LOGFILE_WX_ALERT = NULL;

/* Screen coordinate globals */
long NW_corner_latitude = 0;
long NW_corner_longitude = 0;
long SE_corner_latitude = 0;
long SE_corner_longitude = 0;
long center_latitude = 0;
long center_longitude = 0;

/* Display selection globals */
void *Select_ = NULL;
time_t aircraft_sec_clear = 0;

/* Alert and message globals */
char altnet_call[MAX_CALLSIGN+1] = "";
int auto_reply = 0;
char auto_reply_message[256] = "";

/* Band open globals */
int bando_min = 0;
int bando_max = 0;

/* Device globals */
void *devices = NULL;
void *mw = NULL;
void *port_data = NULL;
int transmit_compressed_posit;

/* Distance and angle globals */
float da = 0.0;

/* English units flag */
int english_units = 0;

/* Igate globals */
int igate_msgs_tx = 0;
int operate_as_an_igate = 0;

/* Data copy buffers */
char incoming_data_copy[1024] = "";
char incoming_data_copy_previous[1024] = "";

/* Locate station globals */
char locate_station_call[MAX_CALLSIGN+1] = "";

/* My station globals */
long my_last_altitude = 0;
time_t my_last_altitude_time = 0;
int my_last_course = 0;
int my_last_speed = 0;
long my_lat = 0;
long my_long = 0;
int my_position_valid = 0;
int my_trail_diff_color = 0;

/* Proximity globals */
int prox_min = 0;
int prox_max = 0;

/* Redraw globals */
int redo_list = 0;
int redraw_on_new_data = 0;

/* Screen dimensions */
unsigned long scale_x = 0;
unsigned long scale_y = 0;
int screen_height = 0;
int screen_width = 0;

/* Station clear time globals */
time_t sec_clear = 0;
time_t sec_remove = 0;

/* Send message dialog lock */
void *send_message_dialog_lock = NULL;

/* Smart beaconing globals */
int smart_beaconing = 0;
int sb_POSIT_rate = 0;
int sb_current_heading = 0;
int sb_high_speed_limit = 0;
int sb_last_heading = 0;
int sb_low_speed_limit = 0;
int sb_posit_fast = 0;
int sb_posit_slow = 0;
int sb_turn_min = 0;
int sb_turn_slope = 0;
time_t sb_turn_time = 0;

/* Position timing globals */
time_t posit_last_time = 0;
time_t posit_next_time = 0;


/* Sound globals */
int sound_band_open_message = 0;
char sound_command[256] = "";
int sound_new_message = 0;
int sound_new_station = 0;
int sound_play_band_open_message;
int sound_play_new_message;
int sound_play_new_station;
int sound_play_prox_message;
int sound_prox_message = 0;

/* Festival speak globals */
int festival_speak_ID=0;
int festival_speak_new_station=0;
int festival_speak_proximity_alert=0;
int festival_speak_tracked_proximity_alert=0;
int festival_speak_band_opening=0;
int festival_speak_new_message_alert=0;
int festival_speak_new_message_body=0;
int festival_speak_new_weather_alert=0;

/* Station capability globals */
int show_only_station_capabilities = 0;

/* Trail color global */
int current_trail_color = 0;

/* Logging globals  */
int log_wx_alert_data;
int log_message_data;
int read_file;

/* Conversion globals */
double cvt_kn2len;  // from knots
double cvt_mi2len;  // from miles
