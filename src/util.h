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


#ifndef __XASTIR_UTIL_H
#define __XASTIR_UTIL_H

#include "db.h"
#include <pthread.h>

extern int  position_amb_chars;
extern int get_hours(void);
extern int get_minutes(void);
extern int get_seconds(void);
extern char *output_lat(char *in_lat, int comp_pos);
extern char *output_long(char *in_long, int comp_pos);
extern double phg_range(char p, char h, char g);
extern double shg_range(char s, char h, char g);
extern void phg_decode(const char *langstr, const char *phg, char *phg_decoded, int phg_decoded_length);
extern void shg_decode(const char *langstr, const char *shg, char *shg_decoded, int shg_decoded_length);
extern void bearing_decode(const char *langstr, const char *bearing_str, const char *NRQ, char *bearing_decoded, int bearing_decoded_length);
extern char *get_line(FILE *f, char *linedata, int maxline);
extern time_t time_from_aprsstring(char *timestamp);
extern char *compress_posit(const char *lat, const char group, const char *lon, const char symbol,
                const int course, const int speed, const char *phg);

extern int  position_defined(long lat, long lon, int strict);
extern void convert_xastir_to_UTM_str(char *str, int str_len, long x, long y);
extern void convert_xastir_to_UTM(double *easting, double *northing, char *zone, int zone_len, long x, long y);
extern void convert_UTM_to_xastir(double easting, double northing, char *zone, long *x, long *y);
extern void convert_lat_l2s(long lat, char *str, int str_len, int type);
extern void convert_lon_l2s(long lon, char *str, int str_len, int type);
extern long convert_lat_s2l(char *lat);
extern long convert_lon_s2l(char *lon);

extern double calc_distance(long lat1, long lon1, long lat2, long lon2);
extern double calc_distance_course(long lat1, long lon1, long lat2, long lon2, char *course_deg, int course_deg_length);
extern double distance_from_my_station(char *call_sign, char *course_deg);

extern char *convert_bearing_to_name(char *bearing, int opposite);

extern int  filethere(char *fn);
extern int  filecreate(char *fn);
extern time_t file_time(char *fn);
extern void log_data(char *file, char *line);
extern time_t sec_now(void);
extern char *get_time(char *time_here);

extern void substr(char *dest, char *src, int size);
extern int  valid_path(char *path);
extern int  valid_call(char *call);
extern int  valid_object(char *name);
extern int  valid_item(char *name);
extern int  valid_inet_name(char *name, char *info, char *origin);

extern char echo_digis[6][9+1];
extern void upd_echo(char *path);

extern char *to_upper(char *data);
extern int  is_num_chr(char ch);
extern int  is_num_or_sp(char ch);
extern int  is_xnum_or_dash(char *data, int max);
extern void removeCtrlCodes(char *cp);
extern void makePrintable(char *cp);

typedef struct
{
    pthread_mutex_t lock;
    pthread_t threadID;
} xastir_mutex;

extern void init_critical_section(xastir_mutex *lock);
extern int begin_critical_section(xastir_mutex *lock, char *text);
extern int end_critical_section(xastir_mutex *lock, char *text);

//#define TIMING_DEBUG
#ifdef TIMING_DEBUG
void time_mark(int start);
#endif

#endif // __XASTIR_UTIL_H
