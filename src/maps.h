/* -*- c-basic-indent: 4; indent-tabs-mode: nil -*-
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

#ifndef __XASTIR_MAPS_H
#define __XASTIR_MAPS_H

#include <X11/Intrinsic.h>

#define MAX_OUTBOUND 900
#define MAX_MAP_POINTS 100000
#define MAX_FILENAME 2000

#define DRAW_TO_PIXMAP          0
#define DRAW_TO_PIXMAP_FINAL    1
#define DRAW_TO_PIXMAP_ALERTS   2
#define INDEX_CHECK_TIMESTAMPS  9998
#define INDEX_NO_TIMESTAMPS     9999


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

typedef struct _map_index_record{
    char filename[MAX_FILENAME];
    unsigned long bottom;
    unsigned long top;
    unsigned long left;
    unsigned long right;
    int accessed;
    int map_layer;      // For future expansion.  Specify which layer to draw the map on.
    int draw_filled;    // For future expansion.  Specify whether to fill polygons when drawing.
    int selected;       // Specifies if map is currently selected
    int auto_maps;      // Specifies if map included in automaps function
    struct _map_index_record *next;
} map_index_record;
extern map_index_record *map_index_head;

char *get_map_ext (char *filename);
void load_auto_maps(Widget w, char *dir);
void load_maps(Widget w);
void fill_in_new_alert_entries(Widget w, char *dir);
void load_alert_maps(Widget w, char *dir);
void  index_update_xastir(char *filename, unsigned long bottom, unsigned long top, unsigned long left, unsigned long right);
void  index_update_ll(char *filename, double bottom, double top, double left, double right);
void draw_grid (Widget w);
void Snapshot(void);
int index_retrieve(char *filename, unsigned long *bottom,
    unsigned long *top, unsigned long *left, unsigned long *right,
    int *map_layer, int *draw_filled, int *automaps);
void index_restore_from_file(void);
void index_save_to_file(void);
void map_indexer(void);

extern int print_rotated;
extern int print_auto_rotation;
extern int print_auto_scale;
extern int print_in_monochrome;
extern int print_invert;
extern int  locate_place(Widget w, char *name, char *state, char *county, char *quad, char* type, char *filename, int follow_case, int get_match);
extern void maps_init(void);
extern time_t last_snapshot;

extern int grid_size;

#if !defined(NO_GRAPHICS)
  #if defined(HAVE_IMAGEMAGICK)
    extern float imagemagick_gamma_adjust;
  #endif    // HAVE_IMAGEMAGICK
  #if defined(HAVE_GEOTIFF)
    extern float geotiff_map_intensity;
  #endif    // HAVE_GEOTIFF
#endif  // NO_GRAPHICS

extern void Print_properties(Widget widget, XtPointer clientData, XtPointer callData);

#endif /* __XASTIR_MAPS_H */


