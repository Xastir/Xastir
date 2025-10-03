/*
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2025 The Xastir Group
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

/* Note: this file should be called db.h, but was renamed to db_funcs.h
 * to avoid conflicts with the Berkeley DB package.  */

#ifndef XASTIR_DB_FUNCS_H
#define XASTIR_DB_FUNCS_H
#include <time.h>
#include <stdio.h>

#include "database.h"

// These don't seem to be used
// extern void clean_data_file(void);
//extern void mem_display(void);
//extern void sort_display_file(char *filename, int size);
//extern unsigned long max_stations;

extern void db_init(void);
extern int is_my_call(char *call, int exact);
extern int is_my_station(DataRow *p_station);
extern int is_my_object_item(DataRow *p_station);
extern int is_tracked_station(char *call_sign);
extern char *get_most_recent_ack(char *callsign);
extern void init_message_data(void);
extern int message_update_time(void);
extern void msg_record_ack(char *to_call_sign, char *my_call, char *seq,
                           int timeout, int cancel);
extern void msg_record_interval_tries(char *to_call_sign, char *my_call,
                                      char *seq, time_t interval, int tries);
extern time_t msg_data_add(char *call_sign, char *from_call, char *data,
                           char *seq, char type, char from, long *record_out);
extern void update_messages(int force);
extern void mdelete_messages_from(char *from);
extern void mdelete_messages_to(char *to);
extern void check_message_remove(time_t curr_sec);
void mscan_file(char msg_type, void (*function)(Message *fill));
extern void mdisplay_file(char msg_type);
extern void pad_callsign(char *callsignout, char *callsignin);
extern void clear_sort_file(char *filename);
extern long sort_input_database(char *filename, char *fill, int size);
int is_altnet(DataRow *p_station);
int ok_to_draw_station(DataRow *p_station);
extern int  heard_via_tnc_in_past_hour(char *call);
extern int  get_weather_record(DataRow *fill);
extern int store_trail_point(DataRow *p_station, long lon, long lat, time_t sec, char *alt, char *speed, char *course, short stn_flag);
void expire_trail_points(DataRow *p_station, time_t sec);
extern int  delete_trail(DataRow *fill);
void export_trail(DataRow *p_station);
extern void export_trail_as_kml(DataRow *p_station);   // export trail of one or all stations to kml file
extern void init_station_data(void);
extern int  search_station_name(DataRow **p_name, char *call, int exact);
extern int  search_station_time(DataRow **p_time, time_t heard, int serial);
extern int  next_station_name(DataRow **p_curr);
extern int  prev_station_name(DataRow **p_curr);
extern int  next_station_time(DataRow **p_curr);
extern int  prev_station_time(DataRow **p_curr);
extern void setup_in_view(void);
extern void station_del(char *callsign);
extern void delete_all_stations(void);
extern void check_station_remove(time_t curr_sec);
extern void my_station_add(char *my_call_sign, char my_group, char my_symbol,
                           char *my_long, char *my_lat, char *my_phg,
                           char *my_comment, char my_amb);
extern void my_station_gps_change(char *pos_long, char *pos_lat, char *course,
                                  char *speed, char speedu, char *alt,
                                  char *sats);
extern void packet_data_add(char *from, char *line, int data_port);
extern void display_packet_data(void);
extern int decode_ax25_header(unsigned char *data_string, int *length);
extern int decode_ax25_line(char *line, char from, int port, int dbadd);
extern void read_file_line(FILE *f);
extern void search_tracked_station(DataRow **p_tracked);
double calc_aloha_distance(void); //meat
int comp_by_dist(const void *,const void *);// used only for qsort
void calc_aloha(int curr_sec); // periodic function
void dump_time_sorted_list(void);

#ifdef DATA_DEBUG
extern void clear_data(DataRow *clear);
extern void copy_data(DataRow *to, DataRow *from);
#else   // DATA_DEBUG
#define clear_data(clear) memset((DataRow *)clear, 0, sizeof(DataRow))
#define copy_data(to, from) memmove((DataRow *)to, (DataRow *)from, \
                                    sizeof(DataRow))
#endif /* DATA_DEBUG */

DataRow * sanity_check_time_list(time_t); // used only for debugging

///////////////////////////////////////// Global variables
extern int  redraw_on_new_packet_data;
extern int  new_message_data;
extern CADRow *CAD_list_head;
extern int station_data_auto_update;
extern int fcc_lookup_pushed;
extern int rac_lookup_pushed;

// calculate every half hour, display in status line every 5 minutes
#define ALOHA_CALC_INTERVAL 1800
#define ALOHA_STATUS_INTERVAL 300
extern time_t aloha_time;
extern time_t aloha_status_time;
extern double aloha_radius;  // in miles
extern aloha_stats the_aloha_stats;

// stations
extern int st_direct_timeout;   // Interval that ST_DIRECT flag stays set
extern int station_count;       // Count of stations in the database
extern int station_count_save;  // Old copy of the above
extern DataRow *n_first;  // pointer to first element in name ordered station
// list
extern DataRow *n_last;   // pointer to last element in name ordered station
// list
extern DataRow *t_oldest; // pointer to first element in time ordered station
// list
extern DataRow *t_newest; // pointer to last element in time ordered station 
// objects/items
extern time_t last_object_check;

#endif