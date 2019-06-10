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

#ifndef __XASTIR_ALERT_H
#define __XASTIR_ALERT_H


#if TIME_WITH_SYS_TIME
  #include <sys/time.h>
  #include <time.h>
#else
  #if HAVE_SYS_TIME_H
    #include <sys/time.h>
  #else
    #include <time.h>
  #endif
#endif


#include "database.h"
//#include "maps.h"


// How many alerts we add storage for each time we're short.
#define ALERT_COUNT_INCREMENT 200


typedef enum
{
  ALERT_TITLE,
  ALERT_TAG,
  ALERT_TO,
  ALERT_FROM
} alert_match_level;

#define ALERT_ALL ALERT_FROM
enum flag_list
{
  on_screen,
  source,
  max_flag=16
};

typedef struct
{
  char unique_string[50];
  double top_boundary, left_boundary, bottom_boundary, right_boundary;
  time_t expiration;  // In local time (secs since epoch)
  char activity[21];
  char alert_tag[21];
  char title[33];
  char alert_level;
  char from[10];
  char to[10];
  /* referenced flags
     0 - on screen
     1 - source
  */
  char flags[max_flag];
  char filename[64];
  int  index;         // Index into shapefile
  char seq[10];
  char issue_date_time[10];
  char desc0[68];     // Space for additional text.
  char desc1[68];     // Spec allows 67 chars per
  char desc2[68];     // message.
  char desc3[68];     //
} alert_entry;


extern void alert_print_list(void);
extern int alert_active(alert_entry *alert, alert_match_level match_level);
extern int alert_display_request(void);
extern int alert_on_screen(void);
extern int alert_redraw_on_update;
extern int alert_expire(int curr_sec);
extern void alert_build_list(Message *fill);
extern struct hashtable_itr *create_wx_alert_iterator(void);
extern alert_entry *get_next_wx_alert(struct hashtable_itr *iterator);
extern int alert_list_count(void);

#endif /* __XASTIR_ALERT_H */


