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

#ifndef __XASTIR_ALERT_H
#define __XASTIR_ALERT_H

#include <time.h>
#include "db.h"
#include "maps.h"

typedef enum {
    ALERT_TITLE,
    ALERT_TAG,
    ALERT_TO,
    ALERT_FROM
} alert_match_level;

#define ALERT_ALL ALERT_FROM

typedef struct {
    unsigned long top_boundary, left_boundary, bottom_boundary, right_boundary;
    time_t expiration;
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
    char flags[16];
    char filename[64];
} alert_entry;

extern alert_entry *alert_list;
extern int alert_list_count;
extern char *alert_tag;

extern void alert_update_list(alert_entry * alert, alert_match_level match_level);
extern void alert_print_list(void);
extern void alert_sort_active(void);
extern int alert_active(alert_entry *alert, alert_match_level match_level);
extern int alert_display_request(void);
extern int alert_on_screen(void);
extern int alert_message_scan(void);
extern int alert_redraw_on_update;


#endif /* __XASTIR_ALERT_H */
