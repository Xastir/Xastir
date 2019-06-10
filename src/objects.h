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

#ifndef XASTIR_OBJECTS_H
#define XASTIR_OBJECTS_H

#include <X11/Intrinsic.h>
#include <stdint.h>


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
extern void Draw_CAD_Objects_erase_dialog(Widget w, XtPointer clientData, XtPointer callData);
extern void Draw_CAD_Objects_list_dialog(Widget w, XtPointer clientData, XtPointer callData);
extern int draw_CAD_objects_flag;
extern int polygon_last_x;
extern int polygon_last_y;
extern int doing_move_operation;
extern char last_object[9+1];
extern char last_obj_grp;
extern char last_obj_sym;
extern int CAD_draw_objects;
extern int CAD_show_label;
extern int CAD_show_raw_probability;
extern int CAD_show_comment;
extern int CAD_show_area;

/* JMT - works in FreeBSD */
// Note: weird conditional thing is there just to shut up
// "ignoring return value" warnings from GCC on newer Linux systems
#define DISABLE_SETUID_PRIVILEGE do { \
if (seteuid(getuid())==0){/* all is well*/} \
if (setegid(getgid())==0){/* all is well*/} \
if (debug_level & 4) { fprintf(stderr, "Changing euid to %d and egid to %d\n", (int)getuid(), (int)getgid()); } \
} while(0)
#define ENABLE_SETUID_PRIVILEGE do { \
if (seteuid(euid)==0){/* all is well*/}     \
if (setegid(egid)==0){/* all is well*/} \
if (debug_level & 4) { fprintf(stderr, "Changing euid to %d and egid to %d\n", (int)euid, (int)egid); } \
} while(0)

// --------------------------------------------------------------------
//
// Function protypes and globals to support predefined SAR/Public service
// objects.
//
//
// MAX_NUMBER_OF_PREDEFINED_OBJECTS is the maximum number of predefined
// objects that can appear on the Create/Move Here popup menu.
#define MAX_NUMBER_OF_PREDEFINED_OBJECTS  11
//
// PREDEFINED_OBJECT_DATA_LENGTH is the maximum length of a string
// that can follow the symbol specifier in a predefined object (such
// as a probability circle definition) plus one (for the terminator).
#define PREDEFINED_OBJECT_DATA_LENGTH 44
//
// number_of_predefined_objects holds the actual number of predefined
// objects available to display on the Create/Move popup menu.
int number_of_predefined_objects;
// File name of ~/.xastir/config file containing definitions for
// a predefined object menu.
extern char predefined_object_definition_filename[256];
// Flag to indicate whether or not to load the predefined objects menu
// from the file specified by predefined_object_definition_filename or
// to use the hardcoded SAR object set.  0=use hardcoded SAR
// 1=use predefined_object_definition_filename
extern int predefined_menu_from_file;

//extern void Set_Del_Object(Widget w, XtPointer clientData, XtPointer calldata);
extern void Create_SAR_Object(Widget w, XtPointer clientData, XtPointer calldata);

typedef struct
{
  char call[MAX_CALLSIGN+1];  // Callsign = object name.
  char page[2];               // APRS symbol code page.
  char symbol[2];             // APRS symbol specifier.
  char data[PREDEFINED_OBJECT_DATA_LENGTH];
  // Data following the symbol.
  char menu_call[26];         // Name to display on menu.
  intptr_t index;             // Index of this object
  // in the predefinedObjects array.
  int show_on_menu;           // !=1 to hide on menu.
  int index_of_child;         // > -1 to create two objects
  // in the same place at the
  // same time, value is the
  // index of the second object
  // in the predefinedObjects array.
} predefinedObject;


// predefinedObjects is an array of predefined object definitions,
// once filled using Populate_predefined_objects it can be traversed
// to build a list of predefined objects for menus, picklists, or
// other user interface controls.
//
extern predefinedObject predefinedObjects[MAX_NUMBER_OF_PREDEFINED_OBJECTS];
extern void Populate_predefined_objects(predefinedObject *predefinedObjects);

// --------------------------------------------------------------------

extern int valid_object(char *name);
extern int valid_item(char *name);
extern void Object_History_Refresh( Widget w, XtPointer clientData, XtPointer callData);
extern void Object_History_Clear( Widget w, XtPointer clientData, XtPointer callData);
extern void  Move_Object( Widget widget, XtPointer clientData, XtPointer callData);
extern void Draw_CAD_Objects_mode( Widget widget, XtPointer clientData, XtPointer callData);
extern void Draw_CAD_Objects_close_polygon(Widget w, XtPointer clientData, XtPointer calldata);
extern void Draw_CAD_Objects_erase(Widget w, XtPointer clientData, XtPointer calldata);
extern void CAD_vertice_allocate(long latitude, long longitude);
extern void CAD_object_allocate(long latitude, long longitude);
extern void Modify_object( Widget w, XtPointer clientData, XtPointer calldata);
extern void Restore_CAD_Objects_from_file(void);
extern void disown_object_item(char *call_sign, char *new_owner);
extern void log_object_item(char *line, int disable_object, char *object_name);
extern void reload_object_item(void);
extern void check_and_transmit_objects_items(time_t time);

#endif /* XASTIR_OBJECTS_H */


