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

#ifndef __XASTIR_MAPS_H
#define __XASTIR_MAPS_H

#include <X11/Intrinsic.h>
#include <Xm/Xm.h>

#define MAX_OUTBOUND 900
#define MAX_MAP_POINTS 100000
#define MAX_FILENAME 2000

#define DRAW_TO_PIXMAP          0
#define DRAW_TO_PIXMAP_FINAL    1
#define DRAW_TO_PIXMAP_ALERTS   2
#define INDEX_CHECK_TIMESTAMPS  9998
#define INDEX_NO_TIMESTAMPS     9999


/* memory structs */

typedef struct
{
  unsigned char vector_start_color;
  unsigned char object_behavior;
  unsigned long longitude;
  unsigned long latitude;
} map_vectors;

typedef struct
{
  unsigned long longitude;
  unsigned long latitude;
  unsigned int mag;
  char label_text[33];
  unsigned char text_color_quad;
} text_label;

typedef struct
{
  unsigned long longitude;
  unsigned long latitude;
  unsigned int mag;
  unsigned char symbol;
  unsigned char aprs_symbol;
  unsigned char text_color;
  char label_text[30];
} symbol_label;

typedef struct _map_index_record
{
  char filename[MAX_FILENAME];
  XmString XmStringPtr;
  unsigned long bottom;
  unsigned long top;
  unsigned long left;
  unsigned long right;
  int accessed;
  int max_zoom;       // Specify maximum zoom at which this layer is drawn.
  int min_zoom;       // Specify minimum zoom at which this layer is drawn.
  int map_layer;      // Specify which layer to draw the map on.
  int draw_filled;    // Specify whether to fill polygons when drawing.
  // 0 = Global No-Fill (Vector)
  // 1 = Global Fill
  // 2 = Auto (dbfawk controls it if present)
  int usgs_drg;       // Specify whether the map has USGS DRG colormap
  // and should have color configuration applied
  // 0 = No
  // 1 = Yes
  // 2 = Auto (detect from TIFFTAG_IMAGEDESCRIPTION)
  int selected;       // Specifies if map is currently selected
  int temp_select;    // Temporary selection used in map properties dialog
  int auto_maps;      // Specifies if map included in automaps function
  struct _map_index_record *next;
} map_index_record;
extern map_index_record *map_index_head;

typedef struct
{
  int img_x;
  int img_y;
  unsigned long x_long;
  unsigned long y_lat;
} tiepoint;

void draw_point(Widget w,
                unsigned long x1,
                unsigned long y1,
                GC gc,
                Pixmap which_pixmap,
                int skip_duplicates);

void draw_point_ll(Widget w,
                   float y1,
                   float x1,
                   GC gc,
                   Pixmap which_pixmap,
                   int skip_duplicates);

void draw_vector(Widget w,
                 unsigned long x1,
                 unsigned long y1,
                 unsigned long x2,
                 unsigned long y2,
                 GC gc,
                 Pixmap which_pixmap,
                 int skip_duplicates);

void draw_vector_ll(Widget w,
                    float y1,
                    float x1,
                    float y2,
                    float x2,
                    GC gc,
                    Pixmap which_pixmap,
                    int skip_duplicates);

char *get_map_ext (char *filename);
char *get_map_dir (char *fullpath);
void load_auto_maps(Widget w, char *dir);
void load_maps(Widget w);
void fill_in_new_alert_entries(void);
void load_alert_maps(Widget w, char *dir);
void  index_update_xastir(char *filename, unsigned long bottom, unsigned long top, unsigned long left, unsigned long right, int default_map_layer);
void  index_update_ll(char *filename, double bottom, double top, double left, double right, int default_map_layer);
extern void get_horizontal_datum(char *datum, int sizeof_datum);
void draw_grid (Widget w);
void Snapshot(void);
extern int index_retrieve(char *filename, unsigned long *bottom,
                          unsigned long *top, unsigned long *left, unsigned long *right,
                          int *max_zoom, int *min_zoom, int *map_layer, int *draw_filled,
                          int *usgs_drg, int *automaps);
extern void index_restore_from_file(void);
extern void index_save_to_file(void);
extern void map_indexer(int parameter);
extern void get_viewport_lat_lon(double *xmin,
                                 double *ymin,
                                 double *xmax,
                                 double *ymax);
extern int map_visible (unsigned long bottom_map_boundary,
                        unsigned long top_map_boundary,
                        unsigned long left_map_boundary,
                        unsigned long right_map_boundary);
extern int map_visible_lat_lon (double f_bottom_map_boundary,
                                double f_top_map_boundary,
                                double f_left_map_boundary,
                                double f_right_map_boundary);
extern int map_inside_viewport_lat_lon(double map_min_y,
                                       double map_max_y,
                                       double map_min_x,
                                       double map_max_x);
extern void draw_label_text (Widget w, int x, int y, int label_length, int color, char *label_text);
extern void draw_rotated_label_text (Widget w, int rotation, int x, int y, int label_length, int color, char *label_text, int fontsize);
extern int get_rotated_label_text_length_pixels(Widget w, char *label_text, int fontsize);
extern void draw_centered_label_text (Widget w, int rotation, int x, int y, int label_length, int color, char *label_text, int fontsize);
extern void  Monochrome( Widget widget, XtPointer clientData, XtPointer callData);
extern void Snapshot(void);
extern void clean_string(char *input);
extern int print_rotated;
extern int print_auto_rotation;
extern int print_auto_scale;
extern int print_in_monochrome;
extern int print_invert;
extern char printer_program[MAX_FILENAME+1];
extern char previewer_program[MAX_FILENAME+1];

extern int  gnis_locate_place(Widget w, char *name, char *state,
                              char *county, char *quad, char* type, char *filename, int
                              follow_case, int get_match, char match_array_name[50][200], long
                              match_array_lat[50], long match_array_long[50]);

extern int  pop_locate_place(Widget w, char *name, char *state,
                             char *county, char *quad, char* type, char *filename, int
                             follow_case, int get_match, char match_array_name[50][200], long
                             match_array_lat[50], long match_array_long[50]);


extern void maps_init(void);
enum map_onscreen_enum {MAP_NOT_VIS=0,MAP_IS_VIS,MAP_NOT_INDEXED};
extern enum map_onscreen_enum map_onscreen(long left, long right, long top, long bottom, int checkpercentage);
extern enum map_onscreen_enum map_onscreen_index(char *filename);
extern time_t last_snapshot;
extern time_t last_kmlsnapshot;
extern int snapshot_interval;

extern int grid_size;

#if !defined(NO_GRAPHICS)
  #if defined(HAVE_MAGICK)
    extern float imagemagick_gamma_adjust;
  #endif    // HAVE_MAGICK
#endif  // NO_GRAPHICS

extern float raster_map_intensity;

extern void Print_Postscript(Widget widget, XtPointer clientData, XtPointer callData);

extern void map_plot (Widget w, long max_x, long max_y, long x_long_cord, long y_lat_cord, unsigned char color, long object_behavior, int destination_pixmap, int draw_filled);

// A struct to pass down in to map driver functions so they can have
// driver-specific flags.  Most drivers won't care about any (or even all)
// of the flags, but this way we can just pass a single pointer rather than
// adding new arguments to the generic interface each time we want new flags
typedef struct
{
  int draw_filled;
  int usgs_drg;
} map_draw_flags;

#endif /* __XASTIR_MAPS_H */


