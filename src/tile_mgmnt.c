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
#include <time.h>
#include <errno.h>

#include "xastir.h"
#include "fetch_remote.h"
#include "util.h"
#include "snprintf.h"
#include "maps.h"

#ifdef HAVE_LIBCURL
  #include <curl/curl.h>
#endif // HAVE_LIBCURL

#include "tile_mgmnt.h"

// Must be last include file
#include "leak_detection.h"

// OSM Minimum cache time
#define SECONDS_7_DAYS (7 * 24 * 3600)

/*
 * latLon2tileNum - calculate tile number
 *
 * The tile number is returned in in the structure pointed to by the
 * tilenum argument.
 */
void latLon2tileNum (double lon_deg,
                     double lat_deg,
                     int zoom,
                     tileNum_t *tilenum)
{
  unsigned long ntiles;
  double lat_rad;
  ntiles = 1 << zoom;

  if (lat_deg > MAX_LAT_OSM)
  {
    lat_deg = MAX_LAT_OSM;
  }
  else if (lat_deg < (-1.0 * MAX_LAT_OSM))
  {
    lat_deg = -1.0 * MAX_LAT_OSM;
  }

  if (lon_deg > MAX_LON_OSM)
  {
    lon_deg = MAX_LON_OSM;
  }
  else if (lon_deg < (-1.0 * MAX_LON_OSM))
  {
    lon_deg = -1.0 * MAX_LON_OSM;
  }

  lat_rad = (lat_deg * M_PI) / 180.0;

  tilenum->x = ((lon_deg + 180.0) / 360.0) * ntiles;
  tilenum->y = ((1 - (log(tan(lat_rad)
                          + (1.0 / cos(lat_rad))) / M_PI)) / 2.0) * ntiles;

  if (tilenum->y >= ntiles)
  {
    tilenum->y = (ntiles - 1);
  }

  if (tilenum->x >= ntiles)
  {
    tilenum->x = (ntiles - 1);
  }

  return;

} // latLon2tileNum()


/*
 * tile2coord - calculate lat/lon of NW corner of tile
 *
 * Use tilex+1 and tiley+1 to get the coordinates of the SE corner.
 *
 * Based on example code at
 * http://wiki.openstreetmap.org/wiki/Slippy_map_tilenames
 *
 */
void tile2coord (unsigned long tilex,
                 unsigned long tiley,
                 int zoom,
                 coord_t *NWcorner)
{
  unsigned long ntiles;
  double lat_rad;

  ntiles = 1 << zoom;
  lat_rad = M_PI - ((2.0 * M_PI) * ((double)(tiley) / (double)ntiles));
  NWcorner->lat = (180.0 / M_PI) * atan(0.5 * (exp((double)lat_rad) - exp(-1.0 * (double)lat_rad)));

  NWcorner->lon = (((double)(tilex) / (double)ntiles) * 360.0) - 180.0;

  return;

} // tile2coord()

/*
 * calcTileArea - calculate the x and y range of tile numbers
 *
 * Returns the calculated values in the structure pointed to by the
 * tiles argument.
 */
void calcTileArea (double lon_upper_left,
                   double lat_upper_left,
                   double lon_lower_right,
                   double lat_lower_right,
                   int zoom,
                   tileArea_t *tiles)
{
  tileNum_t tilenum;
  latLon2tileNum(lon_upper_left, lat_upper_left, zoom, &tilenum);
  tiles->startx = tilenum.x;
  tiles->starty = tilenum.y;
  latLon2tileNum(lon_lower_right, lat_lower_right, zoom, &tilenum);
  tiles->endx = tilenum.x;
  tiles->endy = tilenum.y;
  return;

} // calcTileArea()


/*
 * tilesMissing - return the count of tiles that are missing from the cache
 */
int tilesMissing (unsigned long startx,
                  unsigned long endx,
                  unsigned long starty,
                  unsigned long endy,
                  int zoom,
                  char *cacheDir,
                  char *tileExt)
{
  struct stat sb;
  char local_filename[1100];
  unsigned long x, y;
  int numMissing = 0;

  for (x = startx; x <= endx; x++)
  {
    for (y = starty; y <= endy; y++)
    {

      xastir_snprintf(local_filename, sizeof(local_filename),
                      "%s/%u/%lu/%lu.%s", cacheDir, zoom, x, y, tileExt);

      if (stat(local_filename, &sb) == -1)
      {
        numMissing++;
      }
    }
  }
  return(numMissing);

}  // end of tilesMissing()


/*
 * getOneTile - get one tile from the web
 *
 * The tile is fetched if it does not exist and 1 is returned.
 *
 * If the tile exists in the cache and it is older than 7 days, then it
 * will be refreshed from the server.
 *
 * Returns 1 if a tile did not exist and download was attempted.
 * Returns 0 if a tile exists in the cache (tile might be updated).
 *
 * Returns libcurl errors as a negative integer.
 *
 * WARNING: Download failures are not reported for wget.
 *
 */
#ifdef HAVE_LIBCURL
int getOneTile (CURL *session,
                char *baseURL,
                unsigned long x,
                unsigned long y,
                int zoom,
                char *baseDir,
                char *tileExt)
{
  CURLcode res;
  time_t cacheTimeout;
#else
int getOneTile (char *baseURL,
                unsigned long x,
                unsigned long y,
                int zoom,
                char *baseDir,
                char *tileExt)
{
#endif // HAVE_LIBCURL

  struct stat sb;
  char url[1100];
  char local_filename[1100];
  int result = 0;

  xastir_snprintf(url, sizeof(url), "%s/%u/%lu/%lu.%s", baseURL, zoom,
                  x, y, tileExt);
  xastir_snprintf(local_filename, sizeof(local_filename),
                  "%s/%u/%lu/%lu.%s", baseDir, zoom, x, y, tileExt);

  if (stat(local_filename, &sb) == -1)
  {
    if (debug_level & 512)
    {
      fprintf(stderr, "Fetching %s\n", url);
    }
    result = 1;  // only count files that do not exist

#ifdef HAVE_LIBCURL
    res = fetch_remote_tile(session, url, local_filename);

  }
  else
  {

    // Check for updated tiles after 7 days, per
    // OSM Tile Usage Policy,
    // http://wiki.openstreetmap.org/wiki/Tile_usage_policy,
    // 2010/07/29
    cacheTimeout = sb.st_mtime + SECONDS_7_DAYS;
    if (cacheTimeout <= time(NULL))
    {
      // tile exists in the cache, but is older than 7 days, so
      // check the server for a newer version.
      if (debug_level & 512)
      {
        fprintf(stderr, "Refreshing %s.\n", url);
      }
      res = fetch_remote_tile(session, url, local_filename);

    }
    else
    {

      if (debug_level & 512)
      {
        fprintf(stderr, "Skipping- %s\n", url);
        fprintf(stderr, "          because cache time has not expired.\n");
        fprintf(stderr, "          cache expires %s",
                ctime(&cacheTimeout));
      }

      res = CURLE_OK;
    }
  }

  if (CURLE_OK != res)
  {
    // return the curl error as a negative value to distinqusih it
    // successful values
    result = -1 * (int)res;
  }

#else  // don't HAVE_LIBCURL
    (void)fetch_remote_file(url, local_filename);
  }
#endif // HAVE_LIBCURL

  return(result);

} // getOneTile()


/*
 * mkpath() - attempt to make each directory in a path
 */
static void mkpath(const char *dir)
{
  char tmp[MAX_FILENAME];
  char *p = NULL;
  size_t len;

  xastir_snprintf(tmp, sizeof(tmp),"%s", dir);
  len = strlen(tmp);
  if(tmp[len - 1] == '/')
  {
    tmp[len - 1] = 0;
  }

  for (p = tmp + 1; *p; p++)
  {
    if(*p == '/')
    {
      *p = 0;
      mkdir(tmp, S_IRWXU);
      *p = '/';
    }
  }
  mkdir(tmp, S_IRWXU);

  return;
}  // end of mkpath()


/*
 * mkOSMmapDirs - try to create all the directories needed for the tiles
 *
 * This is simply a best-effort attempt because there are valid reasons
 * for mkdir() to fail. For example, the tiles could be buffered in a RO
 * structure.
 */
void mkOSMmapDirs (char *baseDir,
                   unsigned long startx,
                   unsigned long endx,
                   int zoom)
{
  char fullPath[MAX_FILENAME];
#ifdef HAVE_POSIX_STRERROR_R
  char errmsg[1024];
#endif
  struct stat sb;
  unsigned long dnum;


  xastir_snprintf(fullPath, sizeof(fullPath), "%s/%u/", baseDir, zoom);
  mkpath(fullPath);

  for (dnum = startx; dnum <= endx; dnum++)
  {
    xastir_snprintf(fullPath, sizeof(fullPath), "%s/%u/%lu/",
                    baseDir, zoom, dnum);
    mkdir(fullPath, S_IRWXU);

    if (debug_level & 512)
    {
      if (stat(fullPath, &sb) == -1)
      {
#ifdef HAVE_POSIX_STRERROR_R
        strerror_r(errno, errmsg, sizeof(errmsg));
        fprintf(stderr, "%s: %s\n", errmsg, fullPath);
#else
        fprintf(stderr, "%s: %s\n",
                strerror(errno), fullPath);
#endif
      }
      else if ((sb.st_mode & S_IWUSR) != S_IWUSR)
      {
        fprintf(stderr, "warning: directory %s is not writable\n", fullPath);
      }
      else if (!S_ISDIR(sb.st_mode))
      {
        fprintf(stderr, "warning: %s is not a directory\n", fullPath);
      }
    }
  }

  return;
} // mkOSMmapDirs()


