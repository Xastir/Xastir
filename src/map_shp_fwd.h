/*
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2023 The Xastir Group
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

/* This file is intended to be included only in map_shp.c
 * It contains forward declarations of functions used only internally to
 * map_shp.c, so that the order of function definitions in that file
 * doesn't matter, and all functions inside the file can call any other
 * function inside map_shp.c irrespective of where they appear in the file.
 */
void free_dbfawk_infos(dbfawk_field_info *fld_info, dbfawk_sig_info *sig_info);
void free_dbfawk_sig_info(dbfawk_sig_info *sig_info);
dbfawk_sig_info *initialize_dbfawk_default_sig(void);
awk_symtab *initialize_dbfawk_symbol_table(char *dbffields, size_t dbffields_s,
                                           int *color, int *lanes,
                                           char *name, size_t name_s,
                                           char *key, size_t key_s,
                                           char *sym, size_t sym_s,
                                           int *filled,
                                           int *fill_style,
                                           int *fill_color, int *fill_stipple,
                                           int *pattern, int *display_level,
                                           int *label_level,
                                           int *label_color,
                                           int *font_size);
int find_wx_alert_shape(alert_entry *alert, DBFHandle hDBF, int recordcount,
                        dbfawk_sig_info *sig_info, dbfawk_field_info *fld_info);
void getViewportRect(struct Rect *viewportRect);
char *getShapeTypeString(int nShapeType);
void get_alert_xbm_path(char *xbm_path, size_t xbm_path_size,
                        alert_entry *alert);
void get_gps_color_and_label(char *filename, char *gps_label,
                             size_t gps_label_size, int *gps_color);
int convert_ll_to_screen_coords(long *x, long *y, float lon, float lat);
int get_vertices_screen_coords_XPoints(SHPObject *object, int partStart,
                                        int nVertices, XPoint *points,
                                        int *high_water_mark_index);
int get_vertex_screen_coords_XPoint(SHPObject *object, int vertex,
                                    XPoint *points, int index,
                                    int *high_water_mark_index);
int get_vertex_screen_coords(SHPObject *object, int vertex, long *x, long *y);
int select_arc_label_mod(void);
int check_label_skip(label_string **label_hash, const char *label_text,
                     int mod_number, int *skip_label);
void add_label_to_label_hash(label_string **label_hash, const char *label_text);
float get_label_angle(int x0, int x1, int y0, int y1);
void set_shpt_arc_attributes(Widget w, int color, int lanes, int pattern);
void set_shpt_polygon_fill_stipple(Widget w, int fill_style, int fill_stipple,
                                   int draw_filled);
int preprocess_shp_polygon_holes(SHPObject *object, int *polygon_hole_storage);
GC get_hole_clipping_context(Widget w, SHPObject *object,
                             int *polygon_hole_storage,
                             int *high_water_mark_index);
int clip_x_y_pair(long *x, long *y, long x_min, long x_max, long y_min, long y_max);
