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


#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif  // HAVE_CONFIG_H

#include "snprintf.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>
#include <sys/types.h>
#include <pwd.h>
#include <errno.h>

// Needed for Solaris
#ifdef HAVE_STRINGS_H
  #include <strings.h>
#endif  // HAVE_STRINGS_H

#include <dirent.h>
#include <netinet/in.h>
#include <Xm/XmAll.h>

#ifdef HAVE_X11_XPM_H
  #include <X11/xpm.h>
  #ifdef HAVE_LIBXPM // if we have both, prefer the extra library
    #undef HAVE_XM_XPMI_H
  #endif // HAVE_LIBXPM
#endif // HAVE_X11_XPM_H

#ifdef HAVE_XM_XPMI_H
  #include <Xm/XpmI.h>
#endif // HAVE_XM_XPMI_H

#include <X11/Xlib.h>

#include <math.h>

#include "xastir.h"
#include "maps.h"
#include "alert.h"
#include "fetch_remote.h"
#include "util.h"
#include "main.h"
#include "datum.h"
#include "draw_symbols.h"
#include "rotated.h"
#include "color.h"
#include "xa_config.h"

#include "map_cache.h"

#define CHECKMALLOC(m)  if (!m) { fprintf(stderr, "***** Malloc Failed *****\n"); exit(0); }

#ifdef HAVE_MAGICK
  #if TIME_WITH_SYS_TIME
    #include <sys/time.h>
    #include <time.h>
  #else   // TIME_WITH_SYS_TIME
    #if HAVE_SYS_TIME_H
      #include <sys/time.h>
    #else  // HAVE_SYS_TIME_H
      #include <time.h>
    #endif // HAVE_SYS_TIME_H
  #endif  // TIME_WITH_SYS_TIME
  #undef RETSIGTYPE
  // TVR: "stupid ImageMagick"
  // The problem is that magick/api.h includes Magick's config.h file, and that
  // pulls in all the same autoconf-generated defines that we use.
  // plays those games below, but I don't think in the end that they actually
  // make usable macros with our own data in them.
  // Fortunately, we don't need them, so I'll just undef the ones that are
  // causing problems today.  See main.c for fixes that preserve our values.
  #undef PACKAGE
  #undef VERSION
  /* JMT - stupid ImageMagick */
  #define XASTIR_PACKAGE_BUGREPORT PACKAGE_BUGREPORT
  #undef PACKAGE_BUGREPORT
  #define XASTIR_PACKAGE_NAME PACKAGE_NAME
  #undef PACKAGE_NAME
  #define XASTIR_PACKAGE_STRING PACKAGE_STRING
  #undef PACKAGE_STRING
  #define XASTIR_PACKAGE_TARNAME PACKAGE_TARNAME
  #undef PACKAGE_TARNAME
  #define XASTIR_PACKAGE_VERSION PACKAGE_VERSION
  #undef PACKAGE_VERSION
  #ifdef HAVE_GRAPHICSMAGICK
    /*#include <GraphicsMagick/magick/api.h>*/
    #include <magick/api.h>
  #else   // HAVE_GRAPHICSMAGICK
    #include <magick/api.h>
  #endif  // HAVE_GRAPHICSMAGICK
  #undef PACKAGE_BUGREPORT
  #define PACKAGE_BUGREPORT XASTIR_PACKAGE_BUGREPORT
  #undef XASTIR_PACKAGE_BUGREPORT
  #undef PACKAGE_NAME
  #define PACKAGE_NAME XASTIR_PACKAGE_NAME
  #undef XASTIR_PACKAGE_NAME
  #undef PACKAGE_STRING
  #define PACKAGE_STRING XASTIR_PACKAGE_STRING
  #undef XASTIR_PACKAGE_STRING
  #undef PACKAGE_TARNAME
  #define PACKAGE_TARNAME XASTIR_PACKAGE_TARNAME
  #undef XASTIR_PACKAGE_TARNAME
  #undef PACKAGE_VERSION
  #define PACKAGE_VERSION XASTIR_PACKAGE_VERSION
  #undef XASTIR_PACKAGE_VERSION
#endif // HAVE_MAGICK

// Must be last include file
#include "leak_detection.h"



/**********************************************************
 * draw_WMS_map()
 **********************************************************/
#ifdef HAVE_MAGICK
void draw_WMS_map (Widget w,
                   char *filenm,
                   int destination_pixmap,
                   char *URL,
                   transparent_color_record *c_trans_color_head,
                   int nocache)    // If non-zero, don't use cached version
{


  char file[MAX_FILENAME];        // Complete path/name of image file
  char short_filenm[MAX_FILENAME];
  FILE *f;                        // Filehandle of image file
  char fileimg[MAX_FILENAME];     // Ascii name of image file, read from GEO file
  char WMStmp[MAX_FILENAME*2];  // Used for putting together the WMS map query
  int width, height;
  tiepoint tp[2];                 // Calibration points for map, read in from .geo file
  long map_c_T, map_c_L;          // map delta NW edge coordinates, DNN: these should be signed
  long tp_c_dx, tp_c_dy;          // tiepoint coordinate differences
  unsigned long c_x_min,  c_y_min;// top left coordinates of map inside screen
//  unsigned long c_y_max;          // bottom right coordinates of map inside screen
  double c_x;                     // Xastir coordinates 1/100 sec, 0 = 180°W
  double c_y;                     // Xastir coordinates 1/100 sec, 0 =  90°N

  long map_y_0;                   // map pixel pointer prior to TM adjustment
  long map_x, map_y;              // map pixel pointers, DNN: this was a float, chg to long
  long map_x_min, map_x_max;      // map boundaries for in screen part of map
  long map_y_min, map_y_max;      //
//  long map_x_ctr;                 // half map width in pixel
//  long map_y_ctr;                 // half map height in pixel
  int map_seen = 0;
  int map_act;
//  int map_done;

//  long map_c_yc;                  // map center, vert coordinate
//  long map_c_xc;                  // map center, hor  coordinate
  double map_c_dx, map_c_dy;      // map coordinates increment (pixel width)
  double c_dx;                    // adjusted map pixel width

  long scr_x,  scr_y;             // screen pixel plot positions
  long scr_xp, scr_yp;            // previous screen plot positions
  int  scr_dx, scr_dy;            // increments in screen plot positions
//  long scr_x_mc;                  // map center in screen units

//  long scr_c_xr;

//  long scale_xa;                  // adjusted for topo maps
//  double scale_x_nm;              // nm per Xastir coordinate unit
//  long scale_x0;                  // at widest map area

  char local_filename[MAX_FILENAME];
  ExceptionInfo exception;
  Image *image;
  ImageInfo *image_info;
  PixelPacket *pixel_pack;
  PixelPacket temp_pack;
  IndexPacket *index_pack;
  int l;
  XColor my_colors[256];
  int trans_skip = 0;             // skip transparent pixel
  double left, right, top, bottom, map_width, map_height;
  double lat_center  = 0;
  double long_center = 0;

  char map_it[MAX_FILENAME];
  char tmpstr[100];
  int geo_image_width;        // Image width  from GEO file
  int geo_image_height;       // Image height from GEO file

  time_t query_start_time, query_end_time;

#ifdef USE_MAP_CACHE
  int map_cache_return;
  char * cache_file_id;
#endif  // USE_MAP_CACHE

  char temp_file_path[MAX_VALUE];

  if (debug_level & 512)
  {
    if (nocache)
    {
      fprintf(stderr,"draw_WMS_map: NOCACHE selected\n");
    }
    else
    {
      fprintf(stderr,"draw_WMS_map: CACHING if enabled\n");
    }
  }

  // Create a shorter filename for display (one that fits the
  // status line more closely).  Subtract the length of the
  // "Indexing " and/or "Loading " strings as well.
  if (strlen(filenm) > (41 - 9))
  {
    int avail = 41 - 11;
    int new_len = strlen(filenm) - avail;

    xastir_snprintf(short_filenm,
                    sizeof(short_filenm),
                    "..%s",
                    &filenm[new_len]);
  }
  else
  {
    xastir_snprintf(short_filenm,
                    sizeof(short_filenm),
                    "%s",
                    filenm);
  }

  xastir_snprintf(map_it,
                  sizeof(map_it),
                  langcode ("BBARSTA028"),
                  short_filenm);
  statusline(map_it,0);       // Loading ...



  // Check whether we're indexing or drawing the map
  if ( (destination_pixmap == INDEX_CHECK_TIMESTAMPS)
       || (destination_pixmap == INDEX_NO_TIMESTAMPS) )
  {

    // We're indexing only.  Save the extents in the index.
    // Force the extents to the edges of the earth for the
    // index file.
    index_update_xastir(filenm, // Filename only
                        64800000l,      // Bottom
                        0l,             // Top
                        0l,             // Left
                        129600000l,     // Right
                        0);             // Default Map Level

    // Update statusline
    xastir_snprintf(map_it,
                    sizeof(map_it),
                    langcode ("BBARSTA039"),
                    short_filenm);
    statusline(map_it,0);       // Loading/Indexing ...

    return; // Done indexing this file
  }


  // Tiepoint for upper left screen corner
  //
  tp[0].img_x = 0;                // Pixel Coordinates
  tp[0].img_y = 0;                // Pixel Coordinates
  tp[0].x_long = NW_corner_longitude;   // Xastir Coordinates
  tp[0].y_lat  = NW_corner_latitude;    // Xastir Coordinates


  // Tiepoint for lower right screen corner
  //
  // Here we must use scale_x for both directions because we have
  // square pixels returned by the WMS server.


// Really what we want to do here is to change our bounding box for
// our request to fit square pixels, using scale_x for both
// dimensions, and to change our tiepoints to match.  WMS servers
// currently feed us back square pixels but the spec says that the
// servers should be capable of sending back rectangular pixels, so
// the images we get back may change if we don't request square
// pixels each time.
//
// TODO:  Change our imagesize, bounding rectangle requested, and
// tiepoints to fit square pixels and to use scale_x for both
// dimensions.
//
// Actually, looking at the changes that were made, it looks like we
// _are_ using square pixels and requesting a bounding box based on
// scale_x for both dimensions, so we might be good to go as-is.


  //
  tp[1].img_x =  screen_width - 1; // Pixel Coordinates
  tp[1].img_y = screen_height - 1; // Pixel Coordinates
  tp[1].x_long = SE_corner_longitude; // Xastir Coordinates

// Modified to use same scale (scale_x) for both dimensions, square
// pixels.  Don't use SE_corner_latitude here as it uses scale_y!
//    tp[1].y_lat  =  NW_corner_latitude + ((screen_height) * scale_y); // Xastir Coordinates
  tp[1].y_lat  =  NW_corner_latitude + ((screen_height) * scale_x); // Xastir Coordinates


  // Again, use scale_x for both directions due to the square
  // pixels returned from the WMS server.
  //
  left = (double)((NW_corner_longitude - 64800000l )/360000.0);   // Lat/long Coordinates
  top = (double)(-((NW_corner_latitude - 32400000l )/360000.0));  // Lat/long Coordinates
  right = (double)((SE_corner_longitude - 64800000l)/360000.0);//Lat/long Coordinates

// Modified to use same scale (scale_x) for both dimensions, square
// pixels.  Don't use SE_corner_latitude here as it uses scale_y!
//    bottom = (double)(-(((NW_corner_latitude + ((screen_height) * scale_y) ) - 32400000l)/360000.0));//Lat/long Coordinates
  bottom = (double)(-(((NW_corner_latitude + ((screen_height) * scale_x) ) - 32400000l)/360000.0));//Lat/long Coordinates


  map_width = right - left;   // Lat/long Coordinates
  map_height = top - bottom;  // Lat/long Coordinates

  geo_image_width  = screen_width;
  geo_image_height = screen_height;

  long_center = (left + right)/2.0l;
  lat_center  = (top + bottom)/2.0l;


//  Example query for a WMS map server....
//  xastir_snprintf(fileimg, sizeof(fileimg),
//  "\'http://mesonet.tamu.edu/cgi-bin/p-warn?SERVICE=WMS&VERSION=1.1.1&REQUEST=getmap&layers=radar&BBOX=-129.000,52.500,-111.000,42.500&HEIGHT=1000&WIDTH=1800&FORMAT=image/png\'");


//    xastir_snprintf(WMStmp, sizeof(WMStmp),
//        "http://mesonet.tamu.edu/cgi-bin/p-warn?SERVICE=WMS&VERSION=1.1.1&REQUEST=getmap");


  xastir_snprintf(WMStmp, sizeof(WMStmp), "%s", URL);
  strncat(WMStmp, "&REQUEST=getmap", sizeof(WMStmp) - 1 - strlen(WMStmp));
  strncat(WMStmp, "&EXCEPTIONS=INIMAGE", sizeof(WMStmp) - 1 - strlen(WMStmp));

// This specifies a bounding box based on square pixels.
  xastir_snprintf(tmpstr, sizeof(tmpstr),
                  "&BBOX=%8.5f,%7.5f,%8.5f,%7.5f",
                  left,   // Lower left
                  bottom, // Lower left
                  right,  // Upper right
                  top);   // Upper right
  strncat (WMStmp, tmpstr, sizeof(WMStmp) - 1 - strlen(WMStmp));

  xastir_snprintf(tmpstr, sizeof(tmpstr), "&HEIGHT=%d", geo_image_height);
  strncat (WMStmp, tmpstr, sizeof(WMStmp) - 1 - strlen(WMStmp));

  xastir_snprintf(tmpstr, sizeof(tmpstr), "&WIDTH=%d", geo_image_width);
  strncat (WMStmp, tmpstr, sizeof(WMStmp) - 1 - strlen(WMStmp));


// These should be specified in the .geo file instead of hard-coded:
//
//    strncat(WMStmp, "&VERSION=1.0.0", sizeof(WMStmp) - 1 - strlen(WMStmp));
//    strncat(WMStmp, "&FORMAT=image/png", sizeof(WMStmp) - 1 - strlen(WMStmp));
//    strncat(WMStmp, "&TRANSPARENT=TRUE", sizeof(WMStmp) - 1 - strlen(WMStmp));
//    strncat(WMStmp, "&BGCOLOR=0xffffff", sizeof(WMStmp) - 1 - strlen(WMStmp));
//    strncat(WMStmp, "&BGCOLOR=0x000000", sizeof(WMStmp) - 1 - strlen(WMStmp));
//    strncat(WMStmp, "&CRS=CRS:84", sizeof(WMStmp) - 1 - strlen(WMStmp));

  memcpy(fileimg, WMStmp, sizeof(fileimg));
  fileimg[sizeof(fileimg)-1] = '\0';  // Terminate string

  if (debug_level & 512)
  {
    fprintf(stderr,"left side is %f\n", left);
    fprintf(stderr,"right side is %f\n", right);
    fprintf(stderr,"top  is %f\n", top);
    fprintf(stderr,"bottom is %f\n", bottom);
    fprintf(stderr,"lat center is %f\n", lat_center);
    fprintf(stderr,"long center is %f\n", long_center);
    fprintf(stderr,"screen width is %li\n", screen_width);
    fprintf(stderr,"screen height is %li\n", screen_height);
    fprintf(stderr,"map width is %f\n", map_width);
    fprintf(stderr,"map height is %f\n", map_height);
    fprintf(stderr,"fileimg is %s\n", fileimg);
    fprintf(stderr,"ftp or http file: %s\n", fileimg);
  }

  if (debug_level & 512)
  {
    query_start_time=time(&query_start_time);
  }


#ifdef USE_MAP_CACHE

  if (nocache || map_cache_fetch_disable)
  {

    // Delete old copy from the cache
    if (map_cache_fetch_disable && fileimg[0] != '\0')
    {
      if (map_cache_del(fileimg))
      {
        if (debug_level & 512)
        {
          fprintf(stderr,"Couldn't delete old map from cache\n");
        }
      }
    }

    // Simulate a cache miss
    map_cache_return = 1;
  }
  else
  {
    // Else look for the file in the cache
    map_cache_return = map_cache_get(fileimg,local_filename);
  }

  if (debug_level & 512)
  {
    fprintf(stderr,"map_cache_return: %d\n", map_cache_return);
  }


  // Don't use cached version if "nocache" is non-zero
  //
  if (nocache || map_cache_return != 0 )
  {
    // Caching has not been requested or cached file not found.
    // We must snag the remote file via libcurl or wget.

    if (nocache)
    {
      xastir_snprintf(local_filename,
                      sizeof(local_filename),
                      "%s/map.%s",
                      get_user_base_dir("tmp", temp_file_path, sizeof(temp_file_path)),
                      "png");
    }
    else
    {
      cache_file_id = map_cache_fileid();
      xastir_snprintf(local_filename,
                      sizeof(local_filename),
                      "%s/map_%s.%s",
                      get_user_base_dir("map_cache", temp_file_path, sizeof(temp_file_path)),
                      cache_file_id,
                      "png");
      free(cache_file_id);
    }

#else   // USE_MAP_CACHE

  xastir_snprintf(local_filename,
                  sizeof(local_filename),
                  "%s/map.%s",
                  get_user_base_dir("tmp", temp_file_path, sizeof(temp_file_path)),
                  "png");

#endif  // USE_MAP_CACHE


    // Erase any previously existing local file by the same name.
    // This avoids the problem of having an old map image here and
    // the code trying to display it when the download fails.

    unlink( local_filename );

    HandlePendingEvents(app_context);
    if (interrupt_drawing_now)
    {
      // Update to screen
      (void)XCopyArea(XtDisplay(da),
                      pixmap,
                      XtWindow(da),
                      gc,
                      0,
                      0,
                      (unsigned int)screen_width,
                      (unsigned int)screen_height,
                      0,
                      0);
      return;
    }

    if (fetch_remote_file(fileimg, local_filename))
    {
      // Had trouble getting the file.  Abort.
      return;
    }

    // For debugging the MagickError/MagickWarning segfaults.
    //system("cat /dev/null >/var/tmp/xastir_hacker_map.png");


#ifdef USE_MAP_CACHE

    // Cache this map only if nocache is zero
    if (!nocache)
    {
      map_cache_put(fileimg,local_filename);
    }

  } // end if is cached  DHBROWN
#endif // USE_MAP_CACHE


  if (debug_level & 512)
  {
    fprintf (stderr, "Fetch or query took %d seconds\n",
             (int) (time(&query_end_time) - query_start_time));
  }

  // Set permissions on the file so that any user can overwrite it.
  chmod(local_filename, 0666);

  // Tell ImageMagick where to find it
  xastir_snprintf(file,
                  sizeof(file),
                  "%s",
                  local_filename);

  GetExceptionInfo(&exception);

  image_info=CloneImageInfo((ImageInfo *) NULL);

  xastir_snprintf(image_info->filename,
                  sizeof(image_info->filename),
                  "%s",
                  file);

  if (debug_level & 512)
  {
    fprintf(stderr,"Copied %s into image info.\n", file);
    fprintf(stderr,"image_info got: %s\n", image_info->filename);
    fprintf(stderr,"Entered ImageMagick code.\n");
    fprintf(stderr,"Attempting to open: %s\n", image_info->filename);
  }

  // We do a test read first to see if the file exists, so we
  // don't kill Xastir in the ReadImage routine.
  f = fopen (image_info->filename, "r");
  if (f == NULL)
  {
    if (debug_level & 512)
    {
      fprintf(stderr,"File could not be read\n");
    }

#ifdef USE_MAP_CACHE
    // clear from cache if bad
    if (map_cache_del(fileimg))
    {
      if (debug_level & 512)
      {
        fprintf(stderr,"Couldn't delete map from cache\n");
      }
    }
#endif

    if (image_info)
    {
      DestroyImageInfo(image_info);
    }
    DestroyExceptionInfo(&exception);
    return;
  }
  (void)fclose (f);


  image = ReadImage(image_info, &exception);

  if (image == (Image *) NULL)
  {
    MagickWarning(exception.severity, exception.reason, exception.description);
    //fprintf(stderr,"MagickWarning\n");

#ifdef USE_MAP_CACHE
    // clear from cache if bad
    if (map_cache_del(fileimg))
    {
      if (debug_level & 512)
      {
        fprintf(stderr,"Couldn't delete map from cache\n");
      }
    }
#endif

    if (image_info)
    {
      DestroyImageInfo(image_info);
    }
    DestroyExceptionInfo(&exception);
    return;
  }


  if (debug_level & 512)
  {
    fprintf(stderr,"Color depth is %i \n", (int)image->depth);
  }

  /*
      if (image->colorspace != RGBColorspace) {
          fprintf(stderr,"TBD: I don't think we can deal with colorspace != RGB");
          if (image)
              DestroyImage(image);
          if (image_info)
              DestroyImageInfo(image_info);
          DestroyExceptionInfo(&exception);
          return;
      }
  */

  width = image->columns;
  height = image->rows;

  //  Code to mute the image so it's not as bright.
  /*    if (raster_map_intensity < 1.0) {
          char tempstr[30];

          if (debug_level & 512)
              fprintf(stderr,"level=%s\n", tempstr);

          xastir_snprintf(tempstr,
              sizeof(tempstr),
              "%d, 100, 100",
              (int)(raster_map_intensity * 100.0));

          ModulateImage(image, tempstr);
      }
  */


  // If were are drawing to a low bpp display (typically < 8bpp)
  // try to reduce the number of colors in an image.
  // This may take some time, so it would be best to do ahead of
  // time if it is a static image.
#if (MagickLibVersion < 0x0540)
  if (visual_type == NOT_TRUE_NOR_DIRECT && GetNumberColors(image, NULL) > 128)
  {
#else   // MagickLib >= 540
  if (visual_type == NOT_TRUE_NOR_DIRECT && GetNumberColors(image, NULL, &exception) > 128)
  {
#endif  // MagickLib Version

    if (image->storage_class == PseudoClass)
    {
#if (MagickLibVersion < 0x0549)
      CompressColormap(image); // Remove duplicate colors
#else // MagickLib >= 0x0549
      CompressImageColormap(image); // Remove duplicate colors
#endif  // MagickLibVersion < 0x0549
    }

    // Quantize down to 128 will go here...
  }


#if defined(HAVE_GRAPHICSMAGICK) || (MagickLibVersion < 0x0669)
  pixel_pack = GetImagePixels(image, 0, 0, image->columns, image->rows);
#else
  pixel_pack = GetAuthenticPixels(image, 0, 0, image->columns, image->rows, &exception);
#endif
  if (!pixel_pack)
  {
    fprintf(stderr,"pixel_pack == NULL!!!");
    if (image)
    {
      DestroyImage(image);
    }
    if (image_info)
    {
      DestroyImageInfo(image_info);
    }
    DestroyExceptionInfo(&exception);
    return;
  }


#if defined(HAVE_GRAPHICSMAGICK)
  #if (MagickLibVersion < 0x201702)
    index_pack = GetIndexes(image);
  #else
    index_pack = AccessMutableIndexes(image);
  #endif
#else
  #if (MagickLibVersion < 0x0669)
    index_pack = GetIndexes(image);
  #else
    index_pack = GetAuthenticIndexQueue(image);
  #endif
#endif
  if (image->storage_class == PseudoClass && !index_pack)
  {
    fprintf(stderr,"PseudoClass && index_pack == NULL!!!");
    if (image)
    {
      DestroyImage(image);
    }
    if (image_info)
    {
      DestroyImageInfo(image_info);
    }
    DestroyExceptionInfo(&exception);
    return;
  }


  if (image->storage_class == PseudoClass && image->colors <= 256)
  {
    for (l = 0; l < (int)image->colors; l++)
    {
      // Need to check how to do this for ANY image, as ImageMagick can read in all sorts
      // of image files
      temp_pack = image->colormap[l];
      if (debug_level & 512)
        fprintf(stderr,"Colormap color is %i  %i  %i \n",
                (int)temp_pack.red, (int)temp_pack.green, (int)temp_pack.blue);

      // Here's a tricky bit:  PixelPacket entries are defined as Quantum's.  Quantum
      // is defined in /usr/include/magick/image.h as either an unsigned short or an
      // unsigned char, depending on what "configure" decided when ImageMagick was installed.
      // We can determine which by looking at MaxRGB or QuantumDepth.
      //
      if (QuantumDepth == 16)     // Defined in /usr/include/magick/image.h
      {
        if (debug_level & 512)
        {
          fprintf(stderr,"Color quantum is [0..65535]\n");
        }
        my_colors[l].red   = temp_pack.red * raster_map_intensity;
        my_colors[l].green = temp_pack.green * raster_map_intensity;
        my_colors[l].blue  = temp_pack.blue * raster_map_intensity;
      }
      else    // QuantumDepth = 8
      {
        if (debug_level & 512)
        {
          fprintf(stderr,"Color quantum is [0..255]\n");
        }
        my_colors[l].red   = (temp_pack.red * 256) * raster_map_intensity;
        my_colors[l].green = (temp_pack.green * 256) * raster_map_intensity;
        my_colors[l].blue  = (temp_pack.blue * 256) * raster_map_intensity;
      }

      // Get the color allocated on < 8bpp displays. pixel color is written to my_colors.pixel
      if (visual_type == NOT_TRUE_NOR_DIRECT)
      {
//                XFreeColors(XtDisplay(w), cmap, &(my_colors[l].pixel),1,0);
        XAllocColor(XtDisplay(w), cmap, &my_colors[l]);
      }
      else
      {
        pack_pixel_bits(my_colors[l].red, my_colors[l].green, my_colors[l].blue,
                        &my_colors[l].pixel);
      }

      if (debug_level & 512)
        fprintf(stderr,"Color allocated is %li  %i  %i  %i \n", my_colors[l].pixel,
                my_colors[l].red, my_colors[l].blue, my_colors[l].green);
    }
  }



  /*
  * Here are the corners of our viewport, using the Xastir
  * coordinate system.  Notice that Y is upside down:
  *
  *   left edge of view = NW_corner_longitude
  *  right edge of view = SE_corner_longitude
  *    top edge of view =  NW_corner_latitude
  * bottom edge of view =  SE_corner_latitude
  *
  * The corners of our map will soon be (after translating the
  * tiepoints to the corners if they're not already there):
  *
  *   left edge of map = tp[0].x_long   in Xastir format
  *  right edge of map = tp[1].x_long
  *    top edge of map = tp[0].y_lat
  * bottom edge of map = tp[1].y_lat
  *
  */
  map_c_L = tp[0].x_long - NW_corner_longitude;     // map left coordinate
  map_c_T = tp[0].y_lat  - NW_corner_latitude;      // map top  coordinate

  tp_c_dx = (long)(tp[1].x_long - tp[0].x_long);//  Width between tiepoints
  tp_c_dy = (long)(tp[1].y_lat  - tp[0].y_lat); // Height between tiepoints


  // Check for tiepoints being in wrong relation to one another
  if (tp_c_dx < 0)
  {
    tp_c_dx = -tp_c_dx;  // New  width between tiepoints
  }
  if (tp_c_dy < 0)
  {
    tp_c_dy = -tp_c_dy;  // New height between tiepoints
  }

  // Calculate step size per pixel
  map_c_dx = ((double) tp_c_dx / abs(tp[1].img_x - tp[0].img_x));
  map_c_dy = ((double) tp_c_dy / abs(tp[1].img_y - tp[0].img_y));

  // Scaled screen step size for use with XFillRectangle below
  scr_dx = (int) (map_c_dx / scale_x) + 1;
  scr_dy = (int) (map_c_dy / scale_y) + 1;

  // calculate top left map corner from tiepoints
  if (tp[0].img_x != 0)
  {
    tp[0].x_long -= (tp[0].img_x * map_c_dx);   // map left edge longitude
    map_c_L = tp[0].x_long - NW_corner_longitude;     // delta ??
    tp[0].img_x = 0;
    if (debug_level & 512)
    {
      fprintf(stderr,"Translated tiepoint_0 x: %d\t%lu\n", tp[0].img_x, tp[0].x_long);
    }
  }
  if (tp[0].img_y != 0)
  {
    tp[0].y_lat -= (tp[0].img_y * map_c_dy);    // map top edge latitude
    map_c_T = tp[0].y_lat - NW_corner_latitude;
    tp[0].img_y = 0;
    if (debug_level & 512)
    {
      fprintf(stderr,"Translated tiepoint_0 y: %d\t%lu\n", tp[0].img_y, tp[0].y_lat);
    }
  }

  // calculate bottom right map corner from tiepoints
  // map size is geo_image_width / geo_image_height
  if (tp[1].img_x != (geo_image_width - 1) )
  {
    tp[1].img_x = geo_image_width - 1;
    tp[1].x_long = tp[0].x_long + (tp[1].img_x * map_c_dx); // right
    if (debug_level & 512)
    {
      fprintf(stderr,"Translated tiepoint_1 x: %d\t%lu\n", tp[1].img_x, tp[1].x_long);
    }
  }
  if (tp[1].img_y != (geo_image_height - 1) )
  {
    tp[1].img_y = geo_image_height - 1;
    tp[1].y_lat = tp[0].y_lat + (tp[1].img_y * map_c_dy);   // bottom
    if (debug_level & 512)
    {
      fprintf(stderr,"Translated tiepoint_1 y: %d\t%lu\n", tp[1].img_y, tp[1].y_lat);
    }
  }

  if (debug_level & 512)
  {
    fprintf(stderr,"X tiepoint width: %ld\n", tp_c_dx);
    fprintf(stderr,"Y tiepoint width: %ld\n", tp_c_dy);
    fprintf(stderr,"Loading imagemap: %s\n", file);
    fprintf(stderr,"\nImage: %s\n", file);
    fprintf(stderr,"Image size %d %d\n", geo_image_width, geo_image_height);
    fprintf(stderr,"XX: %ld YY:%ld Sx %f %d Sy %f %d\n",
            map_c_L, map_c_T, map_c_dx,(int) (map_c_dx / scale_x), map_c_dy, (int) (map_c_dy / scale_y));
    fprintf(stderr,"Image size %d %d\n", width, height);
#if (MagickLibVersion < 0x0540)
    fprintf(stderr,"Unique colors = %d\n", GetNumberColors(image, NULL));
#else // MagickLib < 540
    fprintf(stderr,"Unique colors = %ld\n", GetNumberColors(image, NULL, &exception));
#endif // MagickLib < 540
    fprintf(stderr,"XX: %ld YY:%ld Sx %f %d Sy %f %d\n", map_c_L, map_c_T,
            map_c_dx,(int) (map_c_dx / scale_x), map_c_dy, (int) (map_c_dy / scale_y));
    fprintf(stderr,"image matte is %i\n", image->matte);
  } // debug_level & 512

  // draw the image from the file out to the map screen

  // Get the border values for the X and Y for loops used
  // for the XFillRectangle call later.

//  map_c_yc = (tp[0].y_lat + tp[1].y_lat) / 2;     // vert center of map as reference
//  map_y_ctr = (long)(height / 2 +0.499);
//  scale_x0 = get_x_scale(0,map_c_yc,scale_y);     // reference scaling at vert map center

//  map_c_xc  = (tp[0].x_long + tp[1].x_long) / 2;  // hor center of map as reference
//  map_x_ctr = (long)(width  / 2 +0.499);
//  scr_x_mc  = (map_c_xc - NW_corner_longitude) / scale_x; // screen coordinates of map center

  // calculate map pixel range in y direction that falls into screen area
//  c_y_max = 0ul;
  map_y_min = map_y_max = 0l;
  for (map_y_0 = 0, c_y = tp[0].y_lat; map_y_0 < (long)height; map_y_0++, c_y += map_c_dy)
  {
    scr_y = (c_y - NW_corner_latitude) / scale_y;   // current screen position
    if (scr_y > 0)
    {
      if (scr_y < screen_height)
      {
        map_y_max = map_y_0;          // update last map pixel in y
//              c_y_max = (unsigned long)c_y;// bottom map inside screen coordinate
      }
      else
      {
        break;  // done, reached bottom screen border
      }
    }
    else                                // pixel is above screen
    {
      map_y_min = map_y_0;              // update first map pixel in y
    }
  }
  c_y_min = (unsigned long)(tp[0].y_lat + map_y_min * map_c_dy);   // top map inside screen coordinate

  map_x_min = map_x_max = 0l;
  for (map_x = 0, c_x = tp[0].x_long; map_x < (long)width; map_x++, c_x += map_c_dx)
  {
    scr_x = (c_x - NW_corner_longitude)/ scale_x;  // current screen position
    if (scr_x > 0)
    {
      if (scr_x < screen_width)
      {
        map_x_max = map_x;  // update last map pixel in x
      }
      else
      {
        break;  // done, reached right screen border
      }
    }
    else                                // pixel is left from screen
    {
      map_x_min = map_x;              // update first map pixel in x
    }
  }
  c_x_min = (unsigned long)(tp[0].x_long + map_x_min * map_c_dx);   // left map inside screen coordinate

  scr_yp = -1;
//  scr_c_xr = SE_corner_longitude;
  c_dx = map_c_dx;                            // map pixel width
//  scale_xa = scale_x0;                        // the compiler likes it ;-)

//  map_done = 0;
  map_act  = 0;
  map_seen = 0;
  scr_y = screen_height - 1;


  // loop over map pixel rows
  for (map_y_0 = map_y_min, c_y = (double)c_y_min; (map_y_0 <= map_y_max); map_y_0++, c_y += map_c_dy)
  {

    HandlePendingEvents(app_context);
    if (interrupt_drawing_now)
    {
      if (image)
      {
        DestroyImage(image);
      }
      if (image_info)
      {
        DestroyImageInfo(image_info);
      }
      // Update to screen
      (void)XCopyArea(XtDisplay(da),
                      pixmap,
                      XtWindow(da),
                      gc,
                      0,
                      0,
                      (unsigned int)screen_width,
                      (unsigned int)screen_height,
                      0,
                      0);
      return;
    }

    scr_y = (c_y - NW_corner_latitude) / scale_y;
    if (scr_y != scr_yp)                    // don't do a row twice
    {
      scr_yp = scr_y;                     // remember as previous y
      scr_xp = -1;
      // loop over map pixel columns
      map_act = 0;
//          scale_x_nm = calc_dscale_x(0,(long)c_y) / 1852.0;  // nm per Xastir coordinate
      for (map_x = map_x_min, c_x = (double)c_x_min; map_x <= map_x_max; map_x++, c_x += c_dx)
      {
        scr_x = (c_x - NW_corner_longitude) / scale_x;
        if (scr_x != scr_xp)        // don't do a pixel twice
        {
          scr_xp = scr_x;         // remember as previous x
          map_y = map_y_0;

          if (map_y >= 0 && map_y <= tp[1].img_y)   // check map boundaries in y direction
          {
            map_seen = 1;
            map_act = 1;    // detects blank screen rows (end of map)

            // now copy a pixel from the map image to the screen
            l = map_x + map_y * image->columns;
            trans_skip = 1; // possibly transparent
            if (image->storage_class == PseudoClass)
            {
              if ( c_trans_color_head &&
                   check_trans(my_colors[(int)index_pack[l]],c_trans_color_head))
              {
                trans_skip = 1; // skip it
              }
              else
              {
                XSetForeground(XtDisplay(w), gc, my_colors[(int)index_pack[l]].pixel);
                trans_skip = 0; // draw it
              }
            }
            else
            {
              // It is not safe to assume that the red/green/blue
              // elements of pixel_pack of type Quantum are the
              // same as the red/green/blue of an XColor!
              if (QuantumDepth==16)
              {
                my_colors[0].red=pixel_pack[l].red;
                my_colors[0].green=pixel_pack[l].green;
                my_colors[0].blue=pixel_pack[l].blue;
              }
              else   // QuantumDepth=8
              {
                // shift the bits of the 8-bit quantity so that
                // they become the high bigs of my_colors.*
                my_colors[0].red=pixel_pack[l].red*256;
                my_colors[0].green=pixel_pack[l].green*256;
                my_colors[0].blue=pixel_pack[l].blue*256;
              }
              // NOW my_colors has the right r,g,b range for
              // pack_pixel_bits
              pack_pixel_bits(my_colors[0].red * raster_map_intensity,
                              my_colors[0].green * raster_map_intensity,
                              my_colors[0].blue * raster_map_intensity,
                              &my_colors[0].pixel);
              if ( c_trans_color_head &&
                   check_trans(my_colors[0],c_trans_color_head))
              {
                trans_skip = 1; // skip it
              }
              else
              {
                XSetForeground(XtDisplay(w), gc, my_colors[0].pixel);
                trans_skip = 0; // draw it
              }
            }

            // Skip drawing if a transparent pixel
            if (!trans_skip)
            {
              (void)XFillRectangle (XtDisplay (w),pixmap,gc,scr_x,scr_y,scr_dx,scr_dy);
            }

          } // check map boundaries in y direction
        }
      } // loop over map pixel columns
      if (map_seen && !map_act)
      {
//              map_done = 1;
      }
    }
  } // loop over map pixel rows

  if (image)
  {
    DestroyImage(image);
  }
  if (image_info)
  {
    DestroyImageInfo(image_info);
  }
  DestroyExceptionInfo(&exception);
}
#endif //HAVE_MAGICK
