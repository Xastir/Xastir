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

#ifndef __XASTIR_MAPS_H
#define __XASTIR_MAPS_H

#include <X11/Intrinsic.h>

#define MAX_OUTBOUND 900
#define MAX_MAP_POINTS 100000

#define DRAW_TO_PIXMAP          0
#define DRAW_TO_PIXMAP_FINAL    1
#define DRAW_TO_PIXMAP_ALERTS   2


/* memory structs */

typedef struct {
  unsigned char vector_start_color;
  unsigned char object_behavior;
  unsigned long longitude;
  unsigned long latitude;
} map_vectors;

typedef struct {
  unsigned long longitude;
  unsigned long latitude;
  unsigned int mag;
  char label_text[33];
  unsigned char text_color_quad;
} text_label;

typedef struct {
  unsigned long longitude;
  unsigned long latitude;
  unsigned int mag;
  unsigned char symbol;
  unsigned char aprs_symbol;
  unsigned char text_color;
  char label_text[30];
} symbol_label;

char *get_map_ext (char *filename);
void load_auto_maps(Widget w, char *dir);
void load_maps(Widget w);
void load_alert_maps(Widget w, char *dir);
void draw_grid (Widget w);
void Snapshot(void);
extern int print_rotated;
extern int print_auto_rotation;
extern int print_auto_scale;
extern int print_in_monochrome;
extern int print_invert;
extern int  locate_place(Widget w, char *name, char *state, char *county, char *quad, char* type, char *filename, int follow_case, int get_match);
extern void maps_init(void);


#ifdef HAVE_GEOTIFF
extern float geotiff_map_intensity;
#endif /* HAVE_GEOTIFF */

extern void Print_properties(Widget widget, XtPointer clientData, XtPointer callData);

#endif /* __XASTIR_MAPS_H */
