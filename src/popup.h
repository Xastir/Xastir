/*
 * $Id$
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2009  The Xastir Group
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

#ifndef XASTIR_POPUP_H
#define XASTIR_POPUP_H

#define MAX_POPUPS 30
#define MAX_POPUPS_TIME 600 /* Max time popups will display 600=10min*/

typedef struct
{
  char name[10];
  Widget popup_message_dialog;
  Widget popup_message_data;
  Widget pane, form, button_close;
  time_t sec_opened;

} Popup_Window;

/* from popup_gui.c */
extern void popup_gui_init(void);
extern void clear_popup_message_windows(void);
extern void popup_time_out_check(int curr_sec);

#endif /* XASTIR_POPUP_H */

