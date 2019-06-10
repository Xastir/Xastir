/*
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
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
#ifndef __TILE_MGMNT_H
#define __TILE_MGMNT_H

#ifdef HAVE_LIBCURL
  #include <curl/curl.h>
#endif // HAVE_LIBCURL

typedef struct tileNum_s
{
  unsigned long x;
  unsigned long y;
} tileNum_t;

typedef struct coord_s
{
  double lat;
  double lon;
} coord_t;

typedef struct tileArea_s
{
  unsigned long startx;
  unsigned long endx;
  unsigned long starty;
  unsigned long endy;
} tileArea_t;

#define MAX_LAT_OSM 85.0511
#define MAX_LON_OSM 180.0

void latLon2tileNum(double lon_deg, double lat_deg, int zoom, tileNum_t *tilenum);
void tile2coord(unsigned long tilex, unsigned long tiley, int zoom, coord_t *NWcorner);
void calcTileArea(double lon_upper_left,double lat_upper_left,double lon_lower_right,double lat_lower_right,int zoom,tileArea_t *tiles);

#ifdef HAVE_LIBCURL
  int getOneTile(CURL *session, char *baseURL, unsigned long x, unsigned long y, int zoom, char *baseDir, char *tileExt);
#else
  int getOneTile(char *baseURL, unsigned long x, unsigned long y, int zoom, char *baseDir, char *tileExt);
#endif // HAVE_LIBCURL

void mkOSMmapDirs(char *baseDir, unsigned long startx, unsigned long endx, int zoom);
int tilesMissing (unsigned long startx, unsigned long endx, unsigned long starty, unsigned long endy, int zoom, char *baseDir, char *tileExt);

#endif // __TILE_MGMNT_H
