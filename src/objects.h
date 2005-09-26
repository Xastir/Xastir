/*
 * $Id$
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2005  The Xastir Group
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

#ifndef XASTIR_OBJECTS_H
#define XASTIR_OBJECTS_H

#include <X11/Intrinsic.h>


// For mutex debugging with Linux threads only
#ifdef __linux__
  #define MUTEX_DEBUG 1
#endif  // __linux__


#ifdef __LCLINT__
#define PACKAGE "xastir"
#define VERSION "lclint"
#define VERSIONTXT "xastir lclint debug version"
#else   // __LCLINT__
#define VERSIONTXT PACKAGE " " VERSION
#endif  // __LCLINT__


extern Widget object_dialog;
extern Widget object_group_data;
extern Widget object_symbol_data;
extern void updateObjectPictureCallback(Widget w,XtPointer clientData,XtPointer callData);

extern void Draw_All_CAD_Objects(Widget w);
extern int draw_CAD_objects_flag;
extern int polygon_last_x;
extern int polygon_last_y;
extern int doing_move_operation;
extern char last_object[9+1];
extern char last_obj_grp;
extern char last_obj_sym;


/* JMT - works in FreeBSD */
#define DISABLE_SETUID_PRIVILEGE do { \
seteuid(getuid()); \
setegid(getgid()); \
if (debug_level & 4) { fprintf(stderr, "Changing euid to %d and egid to %d\n", (int)getuid(), (int)getgid()); } \
} while(0)
#define ENABLE_SETUID_PRIVILEGE do { \
seteuid(euid); \
setegid(egid); \
if (debug_level & 4) { fprintf(stderr, "Changing euid to %d and egid to %d\n", (int)euid, (int)egid); } \
} while(0)



extern int valid_object(char *name);
extern int valid_item(char *name);
extern void Object_History_Refresh( Widget w, XtPointer clientData, XtPointer callData);
extern void Object_History_Clear( Widget w, XtPointer clientData, XtPointer callData);
extern void  Move_Object( Widget widget, XtPointer clientData, XtPointer callData);
extern void Draw_CAD_Objects_start_mode(Widget w, XtPointer clientData, XtPointer calldata);
extern void Draw_CAD_Objects_close_polygon(Widget w, XtPointer clientData, XtPointer calldata);
extern void Draw_CAD_Objects_erase(Widget w, XtPointer clientData, XtPointer calldata);
extern void Draw_CAD_Objects_end_mode(Widget w, XtPointer clientData, XtPointer calldata);
extern void CAD_vertice_allocate(long latitude, long longitude);
extern void CAD_object_allocate(long latitude, long longitude);
extern void Modify_object( Widget w, XtPointer clientData, XtPointer calldata);
extern void Restore_CAD_Objects_from_file(void);
extern void disown_object_item(char *call_sign, char *new_owner);
extern void log_object_item(char *line, int disable_object, char *object_name);
extern void reload_object_item(void);
extern void check_and_transmit_objects_items(time_t time);

#endif /* XASTIR_OBJECTS_H */


