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


#ifndef __XASTIR_LIST_GUI_H
#define __XASTIR_LIST_GUI_H

// different list types:
#define LST_ALL 0
#define LST_MOB 1
#define LST_WX  2
#define LST_TNC 3
#define LST_TIM 4
#define LST_OBJ 5
#define LST_MYOBJ 6
#define LST_NUM 7

extern int list_size_h[];
extern int list_size_w[];

/* from list_gui.c */
extern void list_gui_init(void);
extern void update_station_scroll_list(void);
extern int stations_types(int type);
extern void Station_List_fill(int type, int new_offset);
#endif
