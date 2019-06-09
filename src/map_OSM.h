/*
 *
 * Copyright (C) 2000-2019 The Xastir Group
 *
 * This file was contributed by Jerry Dunmire, KA6HLD.
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
 *
 */
#ifndef OSM_H
#define OSM_H

#include <X11/X.h>           // for KeySym

#define MAX_OSMSTYLE 1000  // max characters in the a OSM style argument
#define MAX_OSM_URL  1000  // max characters for a OSM URL
#define MAX_OSMEXT     10  // max characters for a tilename extension

void adj_to_OSM_level(
  long *new_scale_x,
  long *new_scale_y);

void draw_OSM_map(Widget w,
                  char *filenm,
                  int destination_pixmap,
                  char *url,
                  char *style,
                  int nocache);

void draw_OSM_tiles(Widget w,
                    char *filenm,
                    int destination_pixmap,
                    char *server_url,
                    char *tileCacheDir,
                    char *mapName,
                    char *tileExt);

unsigned int osm_zoom_level(long scale_x);
void init_OSM_values(void);
int OSM_optimize_key(KeySym key);
void set_OSM_optimize_key(KeySym key);
int OSM_report_scale_key(KeySym key);
void set_OSM_report_scale_key(KeySym key);

#endif //OSM_H
