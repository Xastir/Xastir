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

#ifndef __XASTIR_BULLETIN_GUI_H
#define __XASTIR_BULLETIN_GUI_H

extern int bulletin_range;


/* from bulletin.c */
extern void bulletin_message(char from, char *call_sign, char *tag, char *packet_message, time_t sec_heard);

// From bulletin_gui.c
extern void prep_for_popup_bulletins();
extern void check_for_new_bulletins();
extern void popup_bulletins(void);
extern void bulletin_gui_init(void);

#endif
