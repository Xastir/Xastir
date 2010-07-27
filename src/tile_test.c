/*
 * $Id$
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 2003-2010  The Xastir Group
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif  // HAVE_CONFIG_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <errno.h>

#include "xastir.h"
#include "util.h"
#include "snprintf.h"

#include "tile_mgmnt.h"

// Must be last include file
#include "leak_detection.h"

// This is required for fetch_remote_tile()
int net_map_timeout = 120;
int debug_level = 0;

int main(void) {
    tileArea_t tiles;
    unsigned long tilex, tiley;
    coord_t corner;
    int zoom;
    double rightLon, leftLon, topLat, bottomLat;

    zoom= 12;
    leftLon = -121.1;
    rightLon = -120.9;
    topLat = 50.2; //38.9;
    bottomLat = 50.0; //38.8;

    zoom= 4;
    leftLon = -24.182625;
    rightLon = 23.325485;
    topLat = 14.785486;
    bottomLat = -19.071180;

    (void)calcTileArea(leftLon, topLat, rightLon, bottomLat, zoom, &tiles);
    printf("startx = %ld, starty=%ld\n", tiles.startx, tiles.starty);
    printf("  endx = %ld,   endy=%ld\n", tiles.endx, tiles.endy);

    (void)mkOSMmapDirs("/tmp", tiles.startx, tiles.endx, zoom);

    for (tilex = tiles.startx; tilex <= tiles.endx; tilex++) {
        for (tiley = tiles.starty; tiley <= tiles.endy; tiley++) {
            getOneTile("http://tile.openstreetmap.org/mapnik", tilex, tiley, zoom, "/tmp/");
        }
    }

    printf("Request tiles for lon/lat = %f/%f", leftLon, topLat);
    printf(" %f/%f\n", rightLon, bottomLat);

    tile2coord(tiles.startx, tiles.starty, zoom, &corner);
    printf("    Got tiles for lon/lat = %f/%f", corner.lon, corner.lat);

    tile2coord(tiles.endx + 1, tiles.endy + 1, zoom, &corner); 
    printf(" %f/%f\n", corner.lon, corner.lat);

    printf("image size (width X height) = %li X %li\n",
            256 * (tiles.endx - tiles.startx + 1),
            256 * (tiles.endy - tiles.starty + 1));

    exit(0);
}
