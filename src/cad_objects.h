/*
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2026 The Xastir Group
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
#ifndef XASTIR_CAD_OBJECTS_H
#define XASTIR_CAD_OBJECTS_H


extern void Draw_All_CAD_Objects(Widget w);
extern void Draw_CAD_Objects_erase_dialog(Widget w, XtPointer clientData, XtPointer callData);
extern void Draw_CAD_Objects_list_dialog(Widget w, XtPointer clientData, XtPointer callData);
extern int draw_CAD_objects_flag;
extern int polygon_last_x;
extern int polygon_last_y;

extern int CAD_draw_objects;
extern int CAD_show_label;
extern int CAD_show_raw_probability;
extern int CAD_show_comment;
extern int CAD_show_area;
extern void Draw_CAD_Objects_mode( Widget widget, XtPointer clientData, XtPointer callData);
extern void Draw_CAD_Objects_close_polygon(Widget w, XtPointer clientData, XtPointer calldata);
extern void Draw_CAD_Objects_erase(Widget w, XtPointer clientData, XtPointer calldata);
extern void CAD_vertice_allocate(long latitude, long longitude);
extern void CAD_object_allocate(long latitude, long longitude);
extern void Restore_CAD_Objects_from_file(void);
#endif
