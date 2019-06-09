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

//#define FUZZYRASTER

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
#include "map_cache.h"
#include "alert.h"
#include "fetch_remote.h"
#include "util.h"
#include "main.h"
#include "datum.h"
#include "draw_symbols.h"
#include "rotated.h"
#include "color.h"
#include "xa_config.h"

#include "map_OSM.h"

#define CHECKMALLOC(m)  if (!m) { fprintf(stderr, "***** Malloc Failed *****\n"); exit(0); }


// Check for XPM and/or ImageMagick.  We use "NO_GRAPHICS"
// to disable some routines below if the support for them
// is not compiled in.
#if !(defined(HAVE_LIBXPM) || defined(HAVE_LIBXPM_IN_XM) || defined(HAVE_MAGICK))
  #define NO_GRAPHICS 1
#endif  // !(HAVE_LIBXPM || HAVE_LIBXPM_IN_XM || HAVE_MAGICK)

#if defined(HAVE_MAGICK) || !(defined(HAVE_LIBXPM) || defined(HAVE_LIBXPM_IN_XM))
  #define NO_XPM 1
#endif  // HAVE_MAGICK ||  !(HAVE_LIBXPM || HAVE_LIBXPM_IN_XM)


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
    /* Define MAGICK_IMPLEMENTATION to access private interfaces
    * such as DestroyImagePixels(). This may not be a good thing,
    * but DestroyImagePixels() has been in this code for a long
    * time. Defining MAGIC_IMPLEMENTATION eliminates the warning that is
    * now (9/28/2010) being seen on some distros (Ubuntu 10.04 and
    * OpenSuSE-11.3)
    */
    #define MAGICK_IMPLEMENTATION
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


int check_interrupt(
#ifdef HAVE_MAGICK
  Image *image, ImageInfo *image_info, ExceptionInfo *exception,
#else // HAVE_MAGICK
  XImage *xi,
#endif // HAVE_MAGICK
  Widget *da, Pixmap *pixmap,
  GC *gc, unsigned long screen_width, unsigned long screen_height)
{
  HandlePendingEvents(app_context);
  if (interrupt_drawing_now)
  {
#ifdef HAVE_MAGICK
    if (image)
    {
      DestroyImage(image);
    }
    if (image_info)
    {
      DestroyImageInfo(image_info);
    }
#else   // HAVE_MAGICK
    if (xi)
    {
      XDestroyImage (xi);
    }
#endif // HAVE_MAGICK
    // Update to screen
    (void)XCopyArea(XtDisplay(*da),
                    *pixmap,
                    XtWindow(*da),
                    *gc,
                    0,
                    0,
                    (unsigned int)screen_width,
                    (unsigned int)screen_height,
                    0,
                    0);
#ifdef HAVE_MAGICK
    DestroyExceptionInfo(exception);
#endif // HAVE_MAGICK
    return(~0);
  }
  else
  {
    return(0);
  }
}

void draw_geo_image_map (Widget w, char *dir, char *filenm,
                         alert_entry *alert, u_char alert_color, int destination_pixmap,
                         map_draw_flags *mdf);





/*  typedef struct _transparent_color_record{
    unsigned long trans_color;
    struct _transparent_color_record *next;
    } transparent_color_record;
*/
// Pointer to head of transparent color linked list.
transparent_color_record *trans_color_head = NULL;





// Empty out the linked list containing transparent colors
void empty_trans_color_list(void)
{
  transparent_color_record *p;

  while (trans_color_head != NULL)
  {
    p = trans_color_head;
    trans_color_head = p->next;
    free(p);
  }
}





// Add a new transparent color to the linked list
void new_trans_color(unsigned long trans_color)
{
  transparent_color_record *p;

  //fprintf(stderr,"New transparent color: %lx\n", trans_color);

  p = (transparent_color_record *)malloc( sizeof(transparent_color_record) );

  // Fill in value
  p->trans_color = trans_color;

  // Link it to transparent color list
  p->next = trans_color_head;
  trans_color_head = p;
}





/********************(**********************************************
 * check_trans()
 *
 * See if this pixel's color should be transparent
 *
 * We only call this from blocks where ImageMagick is used, so we're
 * ok to use IM calls.
 ******************************************(************************/

int check_trans (XColor c, transparent_color_record *c_trans_color_head)
{
  transparent_color_record *p = c_trans_color_head;

  //    fprintf (stderr, "pix = %li,%lx, chk = %li,%lx.\n",c.pixel,c.pixel,c_trans_color,c_trans_color);
  // A linked list from the geo file of colors to zap.

  //    if ( c.pixel == (unsigned long) 0x000000 ) {
  //    return 1; // black background
  //}
  while (p)
  {
    if ( c.pixel == p->trans_color )
    {
      return 1;
    }
    p = p->next;
  }

  return 0; // everything else is OK to draw
}





// Regarding MAP CACHING for toporama maps:  Here we are only
// snagging a .geo file for toporama from findu.com:  We send the
// parameters off to findu.com, it computes the .geo file, we
// download it, then we call draw_geo_image_map() with it.  We would
// have to cache the .geo files from findu, we'd have to modify them
// to point to our local cached map file (maybe), and we'd of course
// have to cache that map image as well.  The image filename on
// findu changes each time we call with the same parameters, so we'd
// have to give the image file our own name based on the parameters
// and write that same name into the cached .geo file.  A bit of
// work, but it _could_ be done.  Actually, we should be able to map
// between the original URL we use to request the .GEO file and the
// final image we received.  That way the name would stay the same
// each time we made the request.  There may be other things we need
// from the generated .GEO file though.
//
// For this particular case we need to snag a remote .geo file and
// then start the process all over again.  The URL we'll need to use
// looks something like this:
//
// http://mm.aprs.net/toporama.cgi?set=50|lat=44.59333|lon=-75.72933|width=800|height=600|zoom=1
//
// Where the lat/lon are the center of our view, and the
// width/height depend on our window size.  The "set" parameter
// decides whether we're fetching 50k or 250k maps.
//
void draw_toporama_map (Widget w,
                        char * UNUSED(dir),
                        char *filenm,
                        alert_entry *alert,
                        u_char alert_color,
                        int destination_pixmap,
                        map_draw_flags *mdf,
                        int toporama_flag)      // 50 or 250
{

#ifdef HAVE_MAGICK

  char fileimg[MAX_FILENAME+1];   // Ascii name of image file, read from GEO file
  char map_it[MAX_FILENAME];
  char local_filename[MAX_FILENAME];
  char file[MAX_FILENAME+1];      // Complete path/name of image file
  char short_filenm[MAX_FILENAME+1];
  FILE *f;                        // Filehandle of image file
  double lat_center = 0;
  double long_center = 0;
  double left, right, top, bottom;
  int my_screen_width, my_screen_height;
  float my_zoom = 1.0;
  char temp_file_path[MAX_VALUE];

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


  //fprintf(stderr, "Found TOPORAMA in a .geo file, %dk scale\n", toporama_flag);

  // Check whether we're indexing or drawing the map
  if ( (destination_pixmap == INDEX_CHECK_TIMESTAMPS)
       || (destination_pixmap == INDEX_NO_TIMESTAMPS) )
  {

    // We're indexing only.  Save the extents in the index.
    // Force the extents to the edges of the earth for the index
    // file.
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


  // Compute the parameters we'll need for the URL, fetch the .geo
  // file at that address, then pass that .geo file off to
  // draw_geo_image_map().  This will cause us to fetch the image
  // file corresponding to the .geo file and display it.  We may
  // also need to tweak the zoom parameter for our current zoom
  // level so that things match up properly.


  // Compute the center of our view in decimal lat/long.
  left = (double)((NW_corner_longitude - 64800000l )/360000.0);   // Lat/long Coordinates
  top = (double)(-((NW_corner_latitude - 32400000l )/360000.0));  // Lat/long Coordinates
  right = (double)((SE_corner_longitude - 64800000l)/360000.0);//Lat/long Coordinates
  bottom = (double)(-((SE_corner_latitude - 32400000l)/360000.0));//Lat/long Coordinates

  long_center = (left + right)/2.0l;
  lat_center  = (top + bottom)/2.0l;

  // We now have the center in decimal lat/long.  We also have the
  // screen width and height in pixels, which we can use in the
  // URL as well (depending on the zoom parameter and how to make
  // the finished display look nice).


  // Compute the size of the image we want to snag.  It should be
  // easy to get darn near anything to work, although the size and
  // scale of the image will drastically affect how nicely the
  // finished map display will look.


  // Compute the zoom parameter for the URL.


  // A test URL that works, just to get things going.  This URL
  // requests 1:50k scale maps ("set=50").
  //xastir_snprintf(fileimg, sizeof(fileimg),
  //    "\"http://mm.aprs.net/toporama.cgi?set=50|lat=44.59333|lon=-75.72933|width=800|height=600|zoom=1\"");

  // Compute our custom URL based on our map view and the
  // requested map scale.
  //
  my_screen_width = (int)screen_width;
  my_screen_height = (int)screen_height;

  if (toporama_flag == 50)    // 1:50k
  {

    my_zoom = 32.0 / scale_y;

    if (scale_y <= 16)
    {
      my_zoom = 2.0;
    }
  }
  else    // toporama_flag == 250 (1:250k)
  {

    my_zoom = 128.0 / scale_y;

    if (scale_y <= 64)
    {
      my_zoom = 2.0;
    }
  }

  // Set a max zoom limit so we don't tax the server too much.
  if (my_zoom < 0.02)
  {
    my_zoom = 0.02;
  }

  xastir_snprintf(fileimg, sizeof(fileimg),
                  "http://mm.aprs.net/toporama.cgi?set=%d|lat=%f|lon=%f|width=%d|height=%d|zoom=%0.3f",
                  //        "http://www2.findu.com/toporama.cgi?set=%d|lat=%f|lon=%f|width=%d|height=%d|zoom=%0.3f",
                  toporama_flag,  // Scale, 50 or 250
                  lat_center,
                  long_center,
                  my_screen_width,
                  my_screen_height,
                  my_zoom);

  //fprintf(stderr,"%s\n", fileimg);

  // Create a local filename that we'll save to.
  xastir_snprintf(local_filename,
                  sizeof(local_filename),
                  "%s/map.geo",
                  get_user_base_dir("tmp", temp_file_path, sizeof(temp_file_path)));

  // Erase any previously existing local file by the same
  // name.  This avoids the problem of having an old map image
  // here and the code trying to display it when the download
  // fails.
  unlink( local_filename );


  // Call wget or libcurl to fetch the .geo file.  Best would be
  // to create a generic "fetch" routine which would fetch a
  // remote file, then go back and rework all of the various map
  // routines to use it.

  if (fetch_remote_file(fileimg, local_filename))
  {
    // Had trouble getting the file.  Abort.
    return;
  }

  // Set permissions on the file so that any user can overwrite it.
  chmod(local_filename, 0666);

  // We now re-use the "file" variable.  It'll hold the
  //name of the map file now instead of the .geo file.

  // Tell ImageMagick where to find it
  xastir_snprintf(file,sizeof(file),"%s",local_filename);



  // Check whether we got a reasonable ~/.xastir/tmp/map.geo file from
  // the fetch.  If so, pass it off to the routine which can draw it.

  // We also need to write a valid IMAGESIZE line into the .geo
  // file.  We know these parameters because they should match
  // screen_width/screen_height.
  //
  xastir_snprintf(map_it,
                  sizeof(map_it),
                  "IMAGESIZE\t%d\t%d\n",
                  my_screen_width,
                  my_screen_height);

  // Another thing we might need is a TRANSPARENT line, for the grey
  // color we see crossing over the U.S. border and obscuring maps on
  // this side of the border.

  f = fopen (local_filename, "a");
  if (f != NULL)
  {
    fprintf(f, "%s", map_it);
    (void)fclose (f);
  }
  else
  {
    fprintf(stderr,"Couldn't open file: %s to add IMAGESIZE tag\n", local_filename);
    return;
  }


  // Call draw_geo_image_map() with our newly-fetched .geo file,
  // passing it most of the parameters that we were originally
  // passed in order to effect the map draw.
  draw_geo_image_map (w,
                      get_user_base_dir("tmp", temp_file_path, sizeof(temp_file_path)),
                      "map.geo",
                      alert,
                      alert_color,
                      destination_pixmap,
                      mdf);

#endif  // HAVE_MAGICK
}





/**********************************************************
 * draw_geo_image_map()
 *
 * If we have found a ".geo" file, we read it here and plot
 * the graphic image into the current viewport.
 * We check first to see whether the map should be plotted
 * and skip it if it's not in our viewport.  These images
 * are expected to be aligned in the lat/lon directions
 * (not rotated) and rectangular.
 **********************************************************/

void draw_geo_image_map (Widget w,
                         char *dir,
                         char *filenm,
                         alert_entry *alert,
                         u_char alert_color,
                         int destination_pixmap,
                         map_draw_flags *mdf)
{
#ifdef NO_GRAPHICS
  fprintf(stderr,"XPM and/or ImageMagick support have not been compiled in.\n");
#else   // NO_GRAPHICS
  char file[MAX_FILENAME+1];      // Complete path/name of image file
  char short_filenm[MAX_FILENAME+1];
  FILE *f;                        // Filehandle of image file
  char line[MAX_FILENAME];        // One line from GEO file
  char fileimg[MAX_FILENAME+1];   // Ascii name of image file, read from GEO file
  char tileCache[MAX_FILENAME+1]; // directory for the OSM tile cache, read from GEO file.
  char OSMstyle[MAX_OSMSTYLE];
  char OSMtileExt[MAX_OSMEXT];

  // Start with an empty fileimg[] string so that we can
  // tell if a URL has been specified in the file. Same for OSMstyle.
  fileimg[0] = '\0';
  OSMstyle[0] = '\0';
  tileCache[0] = '\0';
  OSMtileExt[0] = '\0';

  int width,height;
#ifndef NO_XPM
  XpmAttributes atb;              // Map attributes after map's read into an XImage
#endif  // NO_XPM

  tiepoint tp[2];                 // Calibration points for map, read in from .geo file
  int n_tp;                       // Temp counter for number of tiepoints read
  float temp_long, temp_lat;
  long map_c_T, map_c_L;          // map delta NW edge coordinates, DNN: these should be signed
  long tp_c_dx, tp_c_dy;          // tiepoint coordinate differences
  // DK7IN--
  //  int test;                       // temporary debugging

  unsigned long c_x_min,  c_y_min;// top left coordinates of map inside screen
  //  unsigned long c_y_max;          // bottom right coordinates of map inside screen
  double c_x;                     // Xastir coordinates 1/100 sec, 0 = 180°W
  double c_y;                     // Xastir coordinates 1/100 sec, 0 =  90°N
  double c_y_a;                   // coordinates correction for Transverse Mercator

  long map_y_0;                   // map pixel pointer prior to TM adjustment
  long map_x, map_y;              // map pixel pointers, DNN: this was a float, chg to long
  long map_x_min, map_x_max;      // map boundaries for in screen part of map
  long map_y_min, map_y_max;      //
  long map_x_ctr;                 // half map width in pixel
  //  long map_y_ctr;                 // half map height in pixel
  //  long x;
  int map_seen, map_act, map_done;
  double corrfact;

  long map_c_yc;                  // map center, vert coordinate
  long map_c_xc;                  // map center, hor  coordinate
  double map_c_dx, map_c_dy;      // map coordinates increment (pixel width)
  double c_dx;                    // adjusted map pixel width

  long scr_x,  scr_y;             // screen pixel plot positions
  long scr_xp, scr_yp;            // previous screen plot positions
  int  scr_dx, scr_dy;            // increments in screen plot positions
  //  long scr_x_mc;                  // map center in screen units

  long scr_c_xr;

  double dist;                    // distance from equator in nm
  double ew_ofs;                  // distance from map center in nm

  long scale_xa;                  // adjusted for topo maps
  double scale_x_nm;              // nm per Xastir coordinate unit
  long scale_x0;                  // at widest map area

#ifdef HAVE_MAGICK
  char local_filename[MAX_FILENAME];
  ExceptionInfo exception;
  Image *image;
  ImageInfo *image_info;
  PixelPacket *pixel_pack;
  PixelPacket temp_pack;
  IndexPacket *index_pack;
  int l;
  XColor my_colors[256];
  time_t query_start_time, query_end_time;
  char gamma[16];
  struct
  {
    float r_gamma;
    float g_gamma;
    float b_gamma;
    int gamma_flag;
    int contrast;
    int negate;
    int equalize;
    int normalize;
    char level[32];
    char modulate[32];
  } imagemagick_options = { 1.0, 1.0, 1.0, 0, 0, -1, 0, 0, "", "" };
  double left, right, top, bottom;
  //  double map_width, map_height;
  //N0VH
  //    double lat_center  = 0;
  //    double long_center = 0;
  // Terraserver variables
  double top_n=0, left_e=0, bottom_n=0, right_e=0, map_top_n=0, map_left_e=0;
  int z, url_n=0, url_e=0, t_zoom=16, t_scale=12800;
  char zstr0[8];
  char zstr1[8];
#else   // HAVE_MAGICK
  XImage *xi;                 // Temp XImage used for reading in current image
#endif // HAVE_MAGICK

  int terraserver_flag = 0;   // U.S. satellite images/topo/reflectivity/urban
  // areas via terraserver
  int OSMserver_flag = 0;     // OpenStreetMaps server, 1 = static maps, 2 = tiled
  unsigned tmp_zl = 0;

  int toporama_flag = 0;      // Canadian topo's from mm.aprs.net (originally from Toporama)
  int WMSserver_flag = 0;     // WMS server
  char map_it[MAX_FILENAME];
  int geo_image_width = 0;    // Image width  from GEO file
  int geo_image_height = 0;   // Image height from GEO file
  char geo_datum[8+1];        // WGS-84 etc.
  char geo_projection[256+1];   // TM, UTM, GK, LATLON etc.
  int map_proj=0;
  int map_refresh_interval_temp = 0;
#ifdef HAVE_MAGICK
  int nocache = 0;            // Don't cache the file if non-zero
#endif  // HAVE_MAGICK
#ifdef FUZZYRASTER
  int rasterfuzz = 3;    // ratio to skip
#endif //FUZZYRASTER
  unsigned long temp_trans_color;    // what color to zap
  int trans_skip = 0;  // skip transparent pixel
  int crop_x1=0, crop_x2=0, crop_y1=0, crop_y2=0; // pixel crop box
  int do_crop = 0;     // do we crop pixels
  //#define TIMING_DEBUG
#ifdef TIMING_DEBUG
  time_mark(1);
#endif  // TIMING_DEBUG

#ifdef HAVE_MAGICK
#ifdef USE_MAP_CACHE
  int map_cache_return;
  char *cache_file_id;
#endif  // USE_MAP_CACHE
#endif  // HAVE_MAGICK

#ifdef HAVE_MAGICK
  char temp_file_path[MAX_VALUE];
#endif  // HAVE_MAGICK

  KeySym OSM_key = 0;

  xastir_snprintf(file, sizeof(file), "%s/%s", dir, filenm);

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


  // Read the .geo file to find out map filename and tiepoint info

  // Empty the transparent color list before we start reading in a
  // new .geo file.
  empty_trans_color_list();

  n_tp = 0;
  geo_datum[0]      = '\0';
  geo_projection[0] = '\0';
  f = fopen (file, "r");
  if (f != NULL)
  {
    while (!feof (f))
    {
      (void)get_line (f, line, MAX_FILENAME);
      if (strncasecmp (line, "FILENAME", 8) == 0)
      {
        if (1 != sscanf (line + 9, "%s", fileimg))
        {
          fprintf(stderr,"draw_geo_image_map:sscanf parsing error\n");
        }
        if (fileimg[0] != '/' )   // not absolute path
        {
          // make it relative to the .geo file
          char temp[MAX_FILENAME];

          // grab .geo file name
          strcpy(temp, file);
          temp[sizeof(temp)-1] = '\0';  // Terminate line

          (void)get_map_dir(temp);           // leaves just the path and trailing /
          if (strlen(temp) < (MAX_FILENAME - 1 - strlen(fileimg)))
            strncat(temp,
                    fileimg,
                    sizeof(temp) - 1 - strlen(temp));
          xastir_snprintf(fileimg,sizeof(fileimg),"%s",temp);
        }
      }
      if (strncasecmp (line, "URL", 3) == 0)
        if (1 != sscanf (line + 4, "%s", fileimg))
        {
          fprintf(stderr,"draw_geo_image_map:sscanf parsing error\n");
        }

      if (n_tp < 2)       // Only take the first two tiepoints
      {
        if (strncasecmp (line, "TIEPOINT", 8) == 0)
        {
          if (4 != sscanf (line + 9, "%d %d %f %f",
                           &tp[n_tp].img_x,
                           &tp[n_tp].img_y,
                           &temp_long,
                           &temp_lat))
          {
            fprintf(stderr,"draw_geo_image_map:sscanf parsing error\n");
          }
          // Convert tiepoints from lat/lon to Xastir coordinates
          tp[n_tp].x_long = 64800000l + (360000.0 * temp_long);
          tp[n_tp].y_lat  = 32400000l + (360000.0 * (-temp_lat));
          n_tp++;
        }
      }

      if (strncasecmp (line, "IMAGESIZE", 9) == 0)
        if (2 != sscanf (line + 10, "%d %d",&geo_image_width,&geo_image_height))
        {
          fprintf(stderr,"draw_geo_image_map:sscanf parsing error\n");
        }

      if (strncasecmp (line, "DATUM", 5) == 0)
        if (1 != sscanf (line + 6, "%8s",geo_datum))
        {
          fprintf(stderr,"draw_geo_image_map:sscanf parsing error\n");
        }

      if (strncasecmp (line, "PROJECTION", 10) == 0)
        // Ignores leading and trailing space (nice!)
        if (1 != sscanf (line + 11, "%256s",geo_projection))
        {
          fprintf(stderr,"draw_geo_image_map:sscanf parsing error\n");
        }

      if (strncasecmp (line, "TERRASERVER-URBAN", 17) == 0)
      {
        terraserver_flag = 4;
      }

      if (strncasecmp (line, "TERRASERVER-REFLECTIVITY", 24) == 0)
      {
        terraserver_flag = 3;
      }

      if (strncasecmp (line, "TERRASERVER-TOPO", 16) == 0)
      {
        // Set to max brightness as it looks weird when the
        // intensity variable comes into play.
#ifdef HAVE_MAGICK
        // This one causes problems now.  Not sure why.
        //                xastir_snprintf(imagemagick_options.modulate,32,"100 100 100");
#endif  // HAVE_MAGICK
        terraserver_flag = 2;
      }

      if (strncasecmp (line, "TERRASERVER-SATELLITE", 21) == 0)
      {
        terraserver_flag = 1;
      }

      if (strncasecmp (line, "OSMSTATICMAP", 12) == 0)
      {
        OSMserver_flag = 1;
        if (strlen(line) > 13)
        {
          if (1 != sscanf (line + 13, "%s", OSMstyle))
          {
            fprintf(stderr,"draw_geo_image_map:sscanf parsing error for OSM style.\n");
          }
        }
      }

      if (strncasecmp (line, "OSM_TILED_MAP", 13) == 0)
      {
        OSMserver_flag = 2;
        if (strlen(line) > 14)
        {
          if (1 != sscanf (line + 14, "%s", OSMstyle))
          {
            fprintf(stderr,"draw_geo_image_map:sscanf parsing error for OSM style.\n");
          }
        }
      }

      if (OSMserver_flag > 0)    // the following keywords are only valid for OSM maps
      {
        if (strncasecmp (line, "OSM_OPTIMIZE_KEY", 16) == 0)
        {
          if ((destination_pixmap != INDEX_CHECK_TIMESTAMPS)
              && (destination_pixmap != INDEX_NO_TIMESTAMPS))
          {
            if (strlen(line) > 17)
            {
              if (1 != sscanf (line + 17, "%lu", &OSM_key))
              {
                fprintf(stderr,"draw_geo_image_map:sscanf parsing error for OSM_OPTIMIZE_KEY.\n");
              }
              else
              {
                set_OSM_optimize_key(OSM_key);
              }
            }
          }
        }

        if (strncasecmp (line, "OSM_REPORT_SCALE_KEY", 20) == 0)
        {
          if ((destination_pixmap != INDEX_CHECK_TIMESTAMPS)
              && (destination_pixmap != INDEX_NO_TIMESTAMPS))
          {
            if (strlen(line) > 21)
            {
              if (1 != sscanf (line + 21, "%lu", &OSM_key))
              {
                fprintf(stderr,"draw_geo_image_map:sscanf parsing error for OSM_OPTIMIZE_KEY.\n");
              }
              else
              {
                set_OSM_report_scale_key(OSM_key);
              }
            }
          }
        }

        if (strncasecmp (line, "TILE_DIR", 8) == 0)
        {
          if (strlen(line) > 9)
          {
            if (1 != sscanf (line + 9, "%s", tileCache))
            {
              fprintf(stderr,"draw_geo_image_map:sscanf parsing error for TILE_DIR\n");
            }
          }
        }

        if (strncasecmp (line, "TILE_EXT", 8) == 0)
        {
          if (strlen(line) > 9)
          {
            if (1 != sscanf (line + 9, "%s", OSMtileExt))
            {
              fprintf(stderr,"draw_geo_image_map:sscanf parsing error for TILE_EXT\n");
            }
          }
        }

        if (strncasecmp(line, "ZOOM_LEVEL_MIN", 14) == 0)
        {
          if (strlen(line) > 15)
          {
            if (1 != sscanf(line + 15, "%u", &tmp_zl))
            {
              fprintf(stderr, "draw_geo_image_map:sscanf parsing error for ZOOM_LEVEL_MIN\n");
            }
            else
            {
              if (!(osm_zoom_level(scale_x) >= tmp_zl))
              {
                // skip this map because the zoom level
                // is not supported.
                if (debug_level & 512)
                {
                  fprintf(stderr, "Skipping OSM map. zl = %u < %u\n", osm_zoom_level(scale_x), tmp_zl);
                }
                return;
              }
            }
          }
        }
        if (strncasecmp(line, "ZOOM_LEVEL_MAX", 14) == 0)
        {
          if (strlen(line) > 15)
          {
            if (1 != sscanf(line + 15, "%u", &tmp_zl))
            {
              fprintf(stderr, "draw_geo_image_map:sscanf parsing error for ZOOM_LEVEL_MAX\n");
            }
            else
            {
              if (!(tmp_zl >= osm_zoom_level(scale_x)))
              {
                // skip this map because the zoom level
                // is not supported.
                if (debug_level & 512)
                {
                  fprintf(stderr, "Skipping OSM map. zl = %u > %u\n", osm_zoom_level(scale_x), tmp_zl);
                }
                return;
              }
            }
          }
        }
      }

      if (strncasecmp (line, "WMSSERVER", 9) == 0)
      {
        WMSserver_flag = 1;
      }

      // Check for Canadian topo map request
      if (strncasecmp (line, "TOPORAMA-50k", 12) == 0)
      {
        toporama_flag = 50;
      }
      if (strncasecmp (line, "TOPORAMA-250k", 13) == 0)
      {
        toporama_flag = 250;
      }


      // Check whether we're indexing or drawing the map.
      // Exclude setting the map refresh interval from
      // indexing.
      if ( (destination_pixmap != INDEX_CHECK_TIMESTAMPS)
           && (destination_pixmap != INDEX_NO_TIMESTAMPS) )
      {

        if (strncasecmp (line, "REFRESH", 7) == 0)
        {
          if (1 != sscanf (line + 8, "%d", &map_refresh_interval_temp))
          {
            fprintf(stderr,"draw_geo_image_map:sscanf parsing error\n");
          }
          if ( map_refresh_interval_temp > 0 &&
               ( map_refresh_interval == 0 ||
                 map_refresh_interval_temp < map_refresh_interval) )
          {
            map_refresh_interval = (time_t) map_refresh_interval_temp;
            map_refresh_time = sec_now() + map_refresh_interval;

            if (debug_level & 512)
            {
              fprintf(stderr, "Map Refresh set to %d.\n", (int) map_refresh_interval);
            }
          }
#ifdef HAVE_MAGICK
          nocache = map_refresh_interval_temp;
#endif  // HAVE_MAGICK
        }
      }

      if (strncasecmp(line, "TRANSPARENT", 11) == 0)
      {
        // need to make this read a list of colors to zap
        // out.  Use 32-bit unsigned values, so we can
        // handle 32-bit color displays.
        if (1 != sscanf (line + 12, "%lx", &temp_trans_color))
        {
          fprintf(stderr,"draw_geo_image_map:sscanf parsing error\n");
        }

        {
          unsigned short r,g,b;
          // We'll assume the temp_trans_color has been
          // specified as a 24-bit quantity
          r = (temp_trans_color&0xff0000) >> 16;
          g = (temp_trans_color&0x00ff00)>>8;
          b = temp_trans_color&0x0000ff;
          // Now this is an incredible kludge, but seems to be right
          // Apparently, if QuantumDepth is 16 bits, r, g, and b
          // values are duplicated in the high and low byte, which
          // is just bizarre
#ifdef HAVE_MAGICK
          if (QuantumDepth == 16)
          {
            r=r|(r<<8);
            g=g|(g<<8);
            b=b|(b<<8);
          }
#endif  // HAVE_MAGICK
          //fprintf(stderr,"Original Transparent %lx\n",temp_trans_color);
          //fprintf(stderr,"Transparent r,g,b=%x,%x,%x\n",r,g,b);
          if (visual_type == NOT_TRUE_NOR_DIRECT)
          {
            XColor junk;

#ifdef HAVE_MAGICK
            if (QuantumDepth == 16)
            {
              junk.red=r;
              junk.green=g;
              junk.blue=b;
            }
            else
#endif  // HAVE_MAGICK
            {
              junk.red= r<<8;
              junk.green = g<<8;
              junk.blue = b<<8;
            }
            XAllocColor(XtDisplay(w),cmap,&junk);
            temp_trans_color = junk.pixel;
          }
          else
          {
            pack_pixel_bits(r,g,b,&temp_trans_color);
          }
          //fprintf(stderr,"Packed Transparent %lx\n",temp_trans_color);
        }

        //fprintf(stderr,"New Transparent: %lx\n",temp_trans_color);

        // Link color to transparent color list
        new_trans_color(temp_trans_color);
      }
      if (strncasecmp(line, "CROP", 4) == 0)
      {
        if (4 != sscanf (line + 5, "%d %d %d %d",
                         &crop_x1,
                         &crop_y1,
                         &crop_x2,
                         &crop_y2 ))
        {
          fprintf(stderr,"draw_geo_image_map:sscanf parsing error\n");
        }
        if (crop_x1 < 0 )
        {
          crop_x1 = 0;
        }
        if (crop_y1 < 0 )
        {
          crop_y1 = 0;
        }
        if (crop_x2 < 0 )
        {
          crop_x2 = 0;
        }
        if (crop_y2 < 0 )
        {
          crop_y2 = 0;
        }
        if (crop_x2 < crop_x1 )
        {
          // swap
          do_crop = crop_x1;
          crop_x1=crop_x2;
          crop_x2=do_crop;
        }
        if (crop_y2 < crop_y1 )
        {
          // swap
          do_crop = crop_y1;
          crop_y1=crop_y2;
          crop_y2=do_crop;
        }
        do_crop = 1;
      }
#ifdef HAVE_MAGICK
      if (strncasecmp(line, "GAMMA", 5) == 0)
        imagemagick_options.gamma_flag = sscanf(line + 6, "%f,%f,%f",
                                                &imagemagick_options.r_gamma,
                                                &imagemagick_options.g_gamma,
                                                &imagemagick_options.b_gamma);
      if (strncasecmp(line, "CONTRAST", 8) == 0)
      {
        if (1 != sscanf(line + 9, "%d", &imagemagick_options.contrast))
        {
          fprintf(stderr,"draw_geo_image_map:sscanf parsing error\n");
        }
      }
      if (strncasecmp(line, "NEGATE", 6) == 0)
      {
        if (1 != sscanf(line + 7, "%d", &imagemagick_options.negate))
        {
          fprintf(stderr,"draw_geo_image_map:sscanf parsing error\n");
        }
      }
      if (strncasecmp(line, "EQUALIZE", 8) == 0)
      {
        imagemagick_options.equalize = 1;
      }
      if (strncasecmp(line, "NORMALIZE", 9) == 0)
      {
        imagemagick_options.normalize = 1;
      }
#if (MagickLibVersion >= 0x0539)
      if (strncasecmp(line, "LEVEL", 5) == 0)
      {
        xastir_snprintf(imagemagick_options.level,
                        sizeof(imagemagick_options.level),
                        "%s",
                        line+6);
      }
#endif  // MagickLibVersion >= 0x0539
      if (strncasecmp(line, "MODULATE", 8) == 0)
      {
        xastir_snprintf(imagemagick_options.modulate,
                        sizeof(imagemagick_options.modulate),
                        "%s",
                        line+9);
      }
#endif  // HAVE_MAGICK
    }
    (void)fclose (f);
  }
  else
  {
    fprintf(stderr,"Couldn't open file: %s\n", file);
    return;
  }

  //
  // Check whether OpenStreetMap has been selected.  If so, run off to
  // another routine to service this request.
  //
  if (OSMserver_flag == 1)
  {

#ifdef HAVE_MAGICK

    // We need to send the "nocache" parameter to this function
    // for those instances when the received map is bad.
    // Later the GUI can implement a method for refreshing the
    // latest map and replacing the bad map in the cache.
    //
    // fileimg is the server URL, if specified.
    draw_OSM_map(w, filenm, destination_pixmap, fileimg, OSMstyle, nocache);

#endif  // HAVE_MAGICK

    return;
  }
  else if (OSMserver_flag == 2)
  {
#ifdef HAVE_MAGICK

    // fileimg is the server URL, if specified.
    draw_OSM_tiles(w, filenm, destination_pixmap, fileimg, tileCache, OSMstyle, OSMtileExt);

#endif  // HAVE_MAGICK

    return;
  }



  // Check whether a WMS server has been selected.  If so, run off
  // to another routine to service this request.
  //
  if (WMSserver_flag)
  {

#ifdef HAVE_MAGICK
    // Pass the URL in "fileimg"
    draw_WMS_map(w,
                 filenm,
                 destination_pixmap,
                 fileimg,
                 trans_color_head,
                 nocache); // Don't use cached version if non-zero

#endif  // HAVE_MAGICK

    return;
  }


  if (toporama_flag)
  {

#ifdef HAVE_MAGICK
    // Pass all of the parameters to it.  We'll need to use them
    // to call draw_geo_image_map() again shortly, after we
    // fetch the remote .geo file.
    //
    draw_toporama_map(w,
                      dir,
                      filenm,
                      alert,
                      alert_color,
                      destination_pixmap,
                      mdf,
                      toporama_flag);
#endif  // HAVE_MAGICK

    return;
  }



  // DK7IN: I'm experimenting with the adjustment of topo maps with
  // Transverse Mercator projection. Those maps have equal scaling
  // in distance while we use equal scaling in degrees.

  // For now I use the map center as central meridian (I think that
  // is ok for mapblast), that will change with UTM and Gauss-Krueger

  // I have introduced new entries in the geo file for that...
  // I first adjust the x scaling depending on the latitude
  // Then I move points in y direction depending on the offset from
  // the central meridian. I hope I get that right with those
  // approximations. I have the correct formulas, but that will
  // be very computing intensive and result in slow map loading...

  //    if (geo_datum[0] != '\0')
  //        fprintf(stderr,"Map Datum: %s\n",geo_datum);   // not used now...

  if (geo_projection[0] == '\0')
    // default
    xastir_snprintf(geo_projection,
                    sizeof(geo_projection),
                    "LatLon");
  //fprintf(stderr,"Map Projection: %s\n",geo_projection);
  //    (void)to_upper(geo_projection);
  if (strcasecmp(geo_projection,"TM") == 0)
  {
    map_proj = 1;  // Transverse Mercator
  }
  else
  {
    map_proj = 0;  // Lat/Lon, default
  }

#ifdef HAVE_MAGICK
  if (terraserver_flag)
  {
    //http://terraservice.net/download.ashx?t=1&s=10&x=2742&y=26372&z=10&w=820&h=480
    if (scale_y <= 4)
    {
      t_zoom  = 10; // 1m/pixel
      t_scale = 200;
    }
    else if (scale_y <= 8)
    {
      t_zoom  = 11; // 2m/pixel
      t_scale = 400;
    }
    else if (scale_y <= 16)
    {
      t_zoom  = 12; // 4m/pixel
      t_scale = 800;
    }
    else if (scale_y <= 32)
    {
      t_zoom  = 13; // 8m/pixel
      t_scale = 1600;
    }
    else if (scale_y <= 64)
    {
      t_zoom  = 14; // 16m/pixel
      t_scale = 3200;
    }
    else if (scale_y <= 128)
    {
      t_zoom  = 15; // 32m/pixel
      t_scale = 6400;
    }
    else
    {
      t_zoom  = 16; // 64m/pixel
      t_scale = 12800;
    }

    top  = -((NW_corner_latitude - 32400000l) / 360000.0);
    left =  (NW_corner_longitude - 64800000l) / 360000.0;
    ll_to_utm_ups(gDatum[D_NAD_83_CONUS].ellipsoid,
                  top,
                  left,
                  &top_n,
                  &left_e,
                  zstr0,
                  sizeof(zstr0) );
    if (1 != sscanf(zstr0, "%d", &z))
    {
      fprintf(stderr,"draw_geo_image_map:sscanf parsing error\n");
    }

    bottom = -((SE_corner_latitude - 32400000l) / 360000.0);
    right  =   (SE_corner_longitude - 64800000l) / 360000.0;

    ll_to_utm_ups(gDatum[D_NAD_83_CONUS].ellipsoid,
                  bottom,
                  right,
                  &bottom_n,
                  &right_e,
                  zstr1,
                  sizeof(zstr1) );


    //
    // NOTE:
    // POSSIBLE FUTURE ENHANCEMENT:
    // If zstr0 != zstr1, we have a viewscreen that crosses a UTM zone
    // boundary.  Terraserver/Toposerver will only feed us a map for one
    // side of it or the other.  It'd be VERY nice if some day we could
    // check for this condition and do two map loads instead of the one.
    // We'd need to stop drawing right at the boundary for each map
    // also, so that they'd tile together nicely.
    //


    map_top_n  = (int)((top_n  / t_scale) + 1) * t_scale;
    map_left_e = (int)((left_e / t_scale) + 0) * t_scale;
    utm_ups_to_ll(gDatum[D_NAD_83_CONUS].ellipsoid,
                  map_top_n,
                  map_left_e,
                  zstr0,
                  &top,
                  &left);


    // Below here things can get messed up.  We can end up with very
    // large and/or negative values for geo_image_width and/or
    // geo_image_height.  Usually happens around UTM zone boundaries.
    //
    // Terraserver uses UTM coordinates for specifying the maps instead
    // of lat/long.  Note that we're also not supposed to cross UTM
    // zones in our requests.
    //
    // t = 1 - 4, theme.  1=DOQ (aerial photo)
    //                    2=DRG (topo)
    //                    3=shaded relief
    //                    4=Color photos/Urban areas
    // s = 10 - 16, scale.  10=1 meter/pixel.  11=2 meters/pixel.
    // x = UTM easting, center of image
    // y = UTM northing, center of image
    // z = 1 - 60, UTM zone, center of image
    // w = 50 - 2000, width in pixels
    // h = 50 - 2000, height in pixels
    // logo = 0/1, USGS logo in image if 1
    //

    // This number gets messed up if we cross zones.  UTM lines
    // are slanted, so we _can_ cross zones vertically!
    geo_image_height = abs( (((int)map_top_n - (int)bottom_n) * 200 / t_scale) );

    // This number gets messed up if we cross zones
    geo_image_width  = abs( (((int)right_e - (int)map_left_e) * 200 / t_scale) );


    //fprintf(stderr,"\ngeo_image_height:%d\tmap_top_n:%0.1f\tbottom_n:%0.1f\tt_scale:%d\n",
    //geo_image_height,
    //map_top_n,
    //bottom_n,
    //t_scale);
    // map_top_n is the one that goes whacko, throwing off the height.
    // Check whether this is because we're crossing a UTM zone.  We
    // _can_ cross zones vertically because the UTM lines are slanted.

    //fprintf(stderr,"geo_image_width:%d\tright_e:%0.1f\tmap_left_e:%0.1f\tt_scale:%d\n",
    //geo_image_width,
    //right_e,
    //map_left_e,
    //t_scale);
    // right_e is the one that goes whacko, throwing off the width.
    // Check whether this is because we're crossing a UTM zone.


    if (geo_image_height < 50)
    {
      geo_image_height = 50;
    }

    if (geo_image_width < 50)
    {
      geo_image_width = 50;
    }

    if (geo_image_height > 2000)
    {
      geo_image_height = geo_image_width;
    }

    if (geo_image_width > 2000)
    {
      geo_image_width = geo_image_height;
    }

    if (geo_image_height > 2000)
    {
      geo_image_height = geo_image_width;
    }
    if (geo_image_width > 2000)
    {
      geo_image_width = geo_image_height;
    }


    //        map_width  = right - left;
    //        map_height = top - bottom;

    tp[0].img_x = 0;
    tp[0].img_y = 0;
    tp[0].x_long = 64800000l + (360000.0 * left);
    tp[0].y_lat  = 32400000l + (360000.0 * (-top));

    tp[1].img_x = geo_image_width  - 1;
    tp[1].img_y = geo_image_height - 1;
    tp[1].x_long = 64800000l + (360000.0 * right);
    tp[1].y_lat  = 32400000l + (360000.0 * (-bottom));

    url_n = (int)(top_n  / t_scale); // The request URL does not use the
    url_e = (int)(left_e / t_scale); // N/E of the map corner


    xastir_snprintf(fileimg, sizeof(fileimg),
                    "http://terraservice.net/download.ashx?t=%d\046s=%d\046x=%d\046y=%d\046z=%d\046w=%d\046h=%d",
                    //            "http://terraserver-usa.net/download.ashx?t=%d\046s=%d\046x=%d\046y=%d\046z=%d\046w=%d\046h=%d",
                    terraserver_flag,   // 1, 2, 3, or 4
                    t_zoom,
                    url_e,  // easting, center of map
                    url_n,  // northing, center of map
                    z,
                    geo_image_width,
                    geo_image_height);
    //http://terraservice.net/download.ashx?t=1&s=11&x=1384&y=13274&z=10&w=1215&h=560
    //fprintf(stderr,"%s\n",fileimg);

    if (debug_level & 16)
    {
      fprintf(stderr,"URL: %s\n", fileimg);
    }
  }
#endif // HAVE_MAGICK

  //
  // DK7IN: we should check what we got from the geo file
  //   we use geo_image_width, but it might not be initialised...
  //   and it's wrong if the '\n' is missing at the end...

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


  if (debug_level & 512)
  {
    fprintf(stderr,"X tiepoint width: %ld\n", tp_c_dx);
    fprintf(stderr,"Y tiepoint width: %ld\n", tp_c_dy);
  }

  // Calculate step size per pixel
  map_c_dx = ((double) tp_c_dx / abs(tp[1].img_x - tp[0].img_x));
  map_c_dy = ((double) tp_c_dy / abs(tp[1].img_y - tp[0].img_y));

  // Scaled screen step size for use with XFillRectangle below
  scr_dx = (int) (map_c_dx / scale_x) + 1;
  scr_dy = (int) (map_c_dy / scale_y) + 1;

  if (debug_level & 512)
  {
    fprintf(stderr,"\nImage: %s\n", file);
    fprintf(stderr,"Image size %d %d\n", geo_image_width, geo_image_height);
    fprintf(stderr,"XX: %ld YY:%ld Sx %f %d Sy %f %d\n", map_c_L, map_c_T, map_c_dx,(int) (map_c_dx / scale_x), map_c_dy, (int) (map_c_dy / scale_y));
  }

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

  // By this point, geo_image_width & geo_image_height have to
  // have been initialized to something.

  if ( (geo_image_width == 0) || (geo_image_height == 0) )
  {

    if ( (strncasecmp ("http", fileimg, 4) == 0)
         || (strncasecmp ("ftp", fileimg, 3) == 0))
    {
      // what to do for remote files... hmm... -cbell
    }
    else
    {

#ifdef HAVE_MAGICK
      GetExceptionInfo(&exception);
      image_info=CloneImageInfo((ImageInfo *) NULL);
      xastir_snprintf(image_info->filename,
                      sizeof(image_info->filename),
                      "%s",
                      fileimg);
      if (debug_level & 16)
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
        fprintf(stderr,"1 ");
        fprintf(stderr,"File %s could not be read\n",image_info->filename);

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

      image = PingImage(image_info, &exception);

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

      if (debug_level & 16)
      {
        fprintf(stderr,"Color depth is %i \n", (int)image->depth);
      }

      geo_image_width = image->magick_columns;
      geo_image_height = image->magick_rows;

      // close and clean up imagemagick

      if (image)
      {
        DestroyImage(image);
      }
      if (image_info)
      {
        DestroyImageInfo(image_info);
      }
      DestroyExceptionInfo(&exception);
#endif // HAVE_MAGICK
    }
  }

  //    fprintf(stderr, "Geo: %s: size %ux%u.\n",file, geo_image_width, geo_image_height);
  // if that did not generate a valid size, bail out...
  if ( (geo_image_width == 0) || (geo_image_height == 0) )
  {
    fprintf(stderr,"*** Skipping '%s', IMAGESIZE tag missing or incorrect. ***\n",file);
    fprintf(stderr,"Perhaps no XPM/ImageMagick/GraphicsMagick library support is installed?\n");
    return;
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


  // Check whether we're indexing or drawing the map
  if ( (destination_pixmap == INDEX_CHECK_TIMESTAMPS)
       || (destination_pixmap == INDEX_NO_TIMESTAMPS) )
  {

    // We're indexing only.  Save the extents in the index.
    if (terraserver_flag)
    {
      // Force the extents to the edges of the earth for the
      // index file.
      index_update_xastir(filenm, // Filename only
                          64800000l,      // Bottom
                          0l,             // Top
                          0l,             // Left
                          129600000l,     // Right
                          0);             // Default Map Level
    }
    else
    {
      index_update_xastir(filenm, // Filename only
                          tp[1].y_lat,    // Bottom
                          tp[0].y_lat,    // Top
                          tp[0].x_long,   // Left
                          tp[1].x_long,   // Right
                          0);             // Default Map Level
    }

    // Update statusline
    xastir_snprintf(map_it,
                    sizeof(map_it),
                    langcode ("BBARSTA039"),
                    short_filenm);
    statusline(map_it,0);       // Loading/Indexing ...

    return; // Done indexing this file
  }

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

  // Check whether map is inside our current view
  //                bottom        top    left        right
  if (!map_visible (tp[1].y_lat, tp[0].y_lat, tp[0].x_long, tp[1].x_long))
  {
    if (debug_level & 16)
    {
      fprintf(stderr,"Map not in current view, skipping: %s\n", file);
      fprintf(stderr,"\nImage: %s\n", file);
      fprintf(stderr,"Image size %d %d\n", geo_image_width, geo_image_height);
      fprintf(stderr,"   Map:  lat0:%ld\t lon0:%ld\t lat1:%ld\t lon1:%ld\n",
              tp[0].y_lat,
              tp[0].x_long,
              tp[1].y_lat,
              tp[1].x_long);
      fprintf(stderr,"Screen: NWlat:%ld\tNWlon:%ld\tSElat:%ld\tSElon:%ld\n",
              NW_corner_latitude,
              NW_corner_longitude,
              SE_corner_latitude,
              SE_corner_longitude);
      fprintf(stderr,"XX: %ld YY:%ld Sx %f %d Sy %f %d\n", map_c_L, map_c_T, map_c_dx,(int) (map_c_dx / scale_x), map_c_dy, (int) (map_c_dy / scale_y));
    }
    return;            // Skip this map
#ifdef FUZZYRASTER
  }
  else if (((float)(map_c_dx/scale_x) > rasterfuzz) ||
           ((float)(scale_x/map_c_dx) > rasterfuzz) ||
           ((float)(map_c_dy/scale_y) > rasterfuzz) ||
           ((float)(scale_y/map_c_dy) > rasterfuzz))
  {
    fprintf(stderr,"Skipping fuzzy map %s with sx=%f,sy=%f.\n", file,
            (float)(map_c_dx/scale_x),(float)(map_c_dy/scale_y));
    return;
#endif //FUZZYRASTER
  }
  else if (debug_level & 16)
  {
    fprintf(stderr,"Loading imagemap: %s\n", file);
    fprintf(stderr,"\nImage: %s\n", file);
    fprintf(stderr,"Image size %d %d\n", geo_image_width, geo_image_height);
    fprintf(stderr,"XX: %ld YY:%ld Sx %f %d Sy %f %d\n", map_c_L, map_c_T, map_c_dx,(int) (map_c_dx / scale_x), map_c_dy, (int) (map_c_dy / scale_y));
  }

  // Update statusline
  xastir_snprintf(map_it,
                  sizeof(map_it),
                  langcode ("BBARSTA028"),
                  short_filenm);
  statusline(map_it,0);       // Loading/Indexing ...

#ifndef NO_XPM
  atb.valuemask = 0;
#endif  // NO_XPM

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

  // Best here would be to add the process ID or user ID to the filename
  // (to keep the filename distinct for different users), and to check
  // the timestamp on the map file.  If it's older than xx minutes, go
  // get another one.  Make sure to delete the temp files when closing
  // Xastir.  It'd probably be good to check for old files and delete
  // them when starting Xastir as well.

  // Check to see if we have to use "wget" to go get an internet map
  if ( (strncasecmp ("http", fileimg, 4) == 0)
       || (strncasecmp ("ftp", fileimg, 3) == 0)
       || (terraserver_flag) )
  {
#ifdef HAVE_MAGICK
    char *ext;

    if (debug_level & 16)
    {
      fprintf(stderr,"ftp or http file: %s\n", fileimg);
    }

    if (terraserver_flag)
    {
      ext = "jpg";
    }
    else
    {
      ext = get_map_ext(fileimg);  // Use extension to determine image type
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
      // Look for the file in the cache
      map_cache_return = map_cache_get(fileimg,local_filename);
    }

    if (debug_level & 512)
    {
      fprintf(stderr,"map_cache_return: %d\n", map_cache_return);
    }

    // If nocache is non-zero, we're supposed to refresh the map
    // at intervals.  Don't use a cached version of the map in
    // that case.
    //
    if (nocache || map_cache_return != 0 )
    {

      // Caching not requested or cached file not found.  We
      // must snag the remote file via libcurl or wget.

      if (nocache)
      {
        xastir_snprintf(local_filename,
                        sizeof(local_filename),
                        "%s/map.%s",
                        get_user_base_dir("tmp", temp_file_path, sizeof(temp_file_path)),ext);
      }
      else
      {
        cache_file_id = map_cache_fileid();
        xastir_snprintf(local_filename,
                        sizeof(local_filename),
                        "%s/map_%s.%s",
                        get_user_base_dir("map_cache", temp_file_path, sizeof(temp_file_path)),
                        cache_file_id,
                        ext);
        free(cache_file_id);
      }

#else   // USE_MAP_CACHE

    xastir_snprintf(local_filename,
                    sizeof(local_filename),
                    "%s/map.%s",
                    get_user_base_dir("tmp", temp_file_path, sizeof(temp_file_path)),ext);

#endif  // USE_MAP_CACHE


      // Erase any previously existing local file by the same
      // name.  This avoids the problem of having an old map image
      // here and the code trying to display it when the download
      // fails.
      unlink( local_filename );

      if (fetch_remote_file(fileimg, local_filename))
      {
        // Had trouble getting the file.  Abort.
        return;
      }

#ifdef USE_MAP_CACHE

      // Cache the map only if map_refresh_interval_temp is zero
      if (!map_refresh_interval_temp)
      {
        map_cache_put(fileimg,local_filename);
      }

    } // end if is cached
#endif // MAP_CACHE

    if (debug_level & 512)
    {
      fprintf (stderr, "Fetch or query took %d seconds\n",
               (int) (time(&query_end_time) - query_start_time));
    }

    // Set permissions on the file so that any user can overwrite it.
    chmod(local_filename, 0666);

    // We now re-use the "file" variable.  It'll hold the
    //name of the map file now instead of the .geo file.
    // Tell ImageMagick where to find it
    xastir_snprintf(file, sizeof(file), "%s", local_filename);
#endif  // HAVE_MAGICK

  }
  else
  {
    //fprintf(stderr,"Not ftp or http file\n");

    // We now re-use the "file" variable.  It'll hold the
    //name of the map file now instead of the .geo file.
    xastir_snprintf(file, sizeof(file), "%s", fileimg);
  }

  //fprintf(stderr,"File = %s\n",file);

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

  // The status line is not updated yet, probably 'cuz we're too busy
  // getting the map in this thread and aren't redrawing?

#ifdef HAVE_MAGICK
  GetExceptionInfo(&exception);
  image_info=CloneImageInfo((ImageInfo *) NULL);
  xastir_snprintf(image_info->filename,
                  sizeof(image_info->filename),
                  "%s",
                  file);
  if (debug_level & 16)
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
    fprintf(stderr,"2 ");
    fprintf(stderr,"File %s could not be read\n",image_info->filename);
    if (image_info)
    {
      DestroyImageInfo(image_info);
    }
    DestroyExceptionInfo(&exception);
    return;
  }
  (void)fclose (f);

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
    if (image_info)
    {
      DestroyImageInfo(image_info);
    }
    DestroyExceptionInfo(&exception);
    return;
  }

  image = ReadImage(image_info, &exception);

  if (image == (Image *) NULL)
  {
    MagickWarning(exception.severity, exception.reason, exception.description);
    //fprintf(stderr,"MagickWarning\n");
    if (image_info)
    {
      DestroyImageInfo(image_info);
    }
    DestroyExceptionInfo(&exception);
    return;
  }

  if (debug_level & 16)
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

  if (check_interrupt(image, image_info,
                      &exception, &da, &pixmap, &gc, screen_width, screen_height))
  {
    return;
  }

  // gamma setup
  if (imagemagick_options.gamma_flag == 0 ||
      imagemagick_options.gamma_flag == 1)
  {
    if (imagemagick_options.gamma_flag == 0) // if not set in file, set to 1.0
    {
      imagemagick_options.r_gamma = 1.0;
    }

    imagemagick_options.gamma_flag = 1; // set flag to do gamma

    imagemagick_options.r_gamma += imagemagick_gamma_adjust;

    if (imagemagick_options.r_gamma > 0.95 && imagemagick_options.r_gamma < 1.05)
    {
      imagemagick_options.gamma_flag = 0;  // don't bother if near 1.0
    }
    else if (imagemagick_options.r_gamma < 0.1)
    {
      imagemagick_options.r_gamma = 0.1;  // 0.0 is black and negative is really wacky
    }

    xastir_snprintf(gamma, sizeof(gamma), "%.1f", imagemagick_options.r_gamma);
  }
  else if (imagemagick_options.gamma_flag == 3)
  {
    // No checking if you specify 3 channel gamma correction, so you can try negative
    // numbers, etc. if you wish.
    imagemagick_options.gamma_flag = 1; // set flag to do gamma
    imagemagick_options.r_gamma += imagemagick_gamma_adjust;
    imagemagick_options.g_gamma += imagemagick_gamma_adjust;
    imagemagick_options.b_gamma += imagemagick_gamma_adjust;
    xastir_snprintf(gamma, sizeof(gamma), "%.1f,%.1f,%.1f",
                    imagemagick_options.r_gamma,
                    imagemagick_options.g_gamma,
                    imagemagick_options.b_gamma);
  }
  else
  {
    imagemagick_options.gamma_flag = 0;
  }

  if (check_interrupt(image, image_info,
                      &exception, &da, &pixmap, &gc, screen_width, screen_height))
  {
    return;
  }

  if (imagemagick_options.gamma_flag)
  {
    if (debug_level & 16)
    {
      fprintf(stderr,"gamma=%s\n", gamma);
    }
    GammaImage(image, gamma);
  }

  if (check_interrupt(image, image_info,
                      &exception, &da, &pixmap, &gc, screen_width, screen_height))
  {
    return;
  }

  if (imagemagick_options.contrast != 0)
  {
    if (debug_level & 16)
    {
      fprintf(stderr,"contrast=%d\n", imagemagick_options.contrast);
    }
    ContrastImage(image, imagemagick_options.contrast);
  }

  if (check_interrupt(image, image_info,
                      &exception, &da, &pixmap, &gc, screen_width, screen_height))
  {
    return;
  }

  if (imagemagick_options.negate != -1)
  {
    if (debug_level & 16)
    {
      fprintf(stderr,"negate=%d\n", imagemagick_options.negate);
    }
    NegateImage(image, imagemagick_options.negate);
  }

  if (check_interrupt(image, image_info,
                      &exception, &da, &pixmap, &gc, screen_width, screen_height))
  {
    return;
  }

  if (imagemagick_options.equalize)
  {
    if (debug_level & 16)
    {
      fprintf(stderr,"equalize");
    }
    EqualizeImage(image);
  }

  if (check_interrupt(image, image_info,
                      &exception, &da, &pixmap, &gc, screen_width, screen_height))
  {
    return;
  }

  if (imagemagick_options.normalize)
  {
    if (debug_level & 16)
    {
      fprintf(stderr,"normalize");
    }
    NormalizeImage(image);
  }

  if (check_interrupt(image, image_info,
                      &exception, &da, &pixmap, &gc, screen_width, screen_height))
  {
    return;
  }

#if (MagickLibVersion >= 0x0539)
  if (imagemagick_options.level[0] != '\0')
  {
    if (debug_level & 16)
    {
      fprintf(stderr,"level=%s\n", imagemagick_options.level);
    }
    LevelImage(image, imagemagick_options.level);
  }
#endif  // MagickLibVersion >= 0x0539

  if (check_interrupt(image, image_info,
                      &exception, &da, &pixmap, &gc, screen_width, screen_height))
  {
    return;
  }

  if (imagemagick_options.modulate[0] != '\0')
  {
    if (debug_level & 16)
    {
      fprintf(stderr,"modulate=%s\n", imagemagick_options.modulate);
    }
    ModulateImage(image, imagemagick_options.modulate);
  }
  /*
  // Else check the menu option for raster intensity
  else if (raster_map_intensity < 1.0) {
  char tempstr[30];
  int temp_i;

  temp_i = (int)(raster_map_intensity * 100.0);

  xastir_snprintf(tempstr,
  sizeof(tempstr),
  "%d, 100, 100",
  temp_i);

  //fprintf(stderr,"Modulate: %s\n", tempstr);

  ModulateImage(image, tempstr);
  }
  */

  if (check_interrupt(image, image_info,
                      &exception, &da, &pixmap, &gc, screen_width, screen_height))
  {
    return;
  }

  // crop image: if we just use CropImage(), then the tiepoints will be off
  // make border pixels transparent.
  // cbell - this is a first attempt, it will be integrated into the
  //         lower loops to speed them up...
  if ( do_crop)
  {
    int x, y;
    //PixelPacket target;
    PixelPacket *q;

    //        target=GetOnePixel(image,0,0);
    for (y=0; y < (long) image->rows; y++)
    {
      q=GetImagePixels(image,0,y,image->columns,1);
      if (q == (PixelPacket *) NULL)
      {
        fprintf(stderr, "GetImagePixels Failed....\n");
      }
      for (x=0; x < (int) image->columns; x++)
      {
        if ( (x < crop_x1) || (x > crop_x2) ||
             (y < crop_y1) || (y > crop_y2))
        {
          q->opacity=(Quantum) 1;
        }
        q++;
      }
      if (!SyncImagePixels(image))
      {
        fprintf(stderr, "SyncImagePixels Failed....\n");
      }
    }
    DestroyImagePixels(image);
  }

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

  if (check_interrupt(image, image_info,
                      &exception, &da, &pixmap, &gc, screen_width, screen_height))
  {
    return;
  }

  pixel_pack = GetImagePixels(image, 0, 0, image->columns, image->rows);
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

  if (check_interrupt(image, image_info,
                      &exception, &da, &pixmap, &gc, screen_width, screen_height))
  {
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

  if (check_interrupt(image, image_info,
                      &exception, &da, &pixmap, &gc, screen_width, screen_height))
  {
    return;
  }

  if (debug_level & 16)
#ifdef HAVE_GRAPHICSMAGICK
  fprintf(stderr,"Colors = %d\n", (int)image->colors);
#else   // HAVE_GRAPHICSMAGICK
  fprintf(stderr,"Colors = %ld\n", image->colors);
#endif  // HAVE_GRAPHICSMAGICK

    // Set up our own version of the color map.
    if (image->storage_class == PseudoClass && image->colors <= 256)
    {
      for (l = 0; l < (int)image->colors; l++)
      {
        int leave_unchanged = 0;

        // Need to check how to do this for ANY image, as ImageMagick can read in all sorts
        // of image files
        temp_pack = image->colormap[l];
        if (debug_level & 16)
          fprintf(stderr,"Colormap color is %1.2f  %1.2f  %1.2f \n",
                  (float)temp_pack.red, (float)temp_pack.green, (float)temp_pack.blue);

        // Here's a tricky bit:  PixelPacket entries are defined as Quantum's.  Quantum
        // is defined in /usr/include/magick/image.h as either an unsigned short or an
        // unsigned char, depending on what "configure" decided when ImageMagick was installed.
        // We can determine which by looking at MaxRGB or QuantumDepth.
        //

        if (QuantumDepth == 16)     // Defined in /usr/include/magick/magick_config.h
        {
          if (debug_level & 16)
          {
            fprintf(stderr,"Color quantum is [0..65535]\n");
          }

          my_colors[l].red   = temp_pack.red;
          my_colors[l].green = temp_pack.green;
          my_colors[l].blue  = temp_pack.blue;
        }
        else    // QuantumDepth = 8
        {
          if (debug_level & 16)
          {
            fprintf(stderr,"Color quantum is [0..255]\n");
          }

          my_colors[l].red   = temp_pack.red * 256;
          my_colors[l].green = temp_pack.green * 256;
          my_colors[l].blue  = temp_pack.blue * 256;
        }

        // Take care not to screw up the transparency value by
        // the raster_map_intensity multiplication factor.
        if ( trans_color_head )
        {
          //fprintf(stderr,"Checking for transparency\n");

          // Get the color allocated on < 8bpp displays. pixel color is written to my_colors.pixel
          if (visual_type == NOT_TRUE_NOR_DIRECT)
          {
            //                    XFreeColors(XtDisplay(w), cmap, &(my_colors[l].pixel),1,0);
            XAllocColor(XtDisplay(w), cmap, &my_colors[l]);
          }
          else
          {
            pack_pixel_bits(my_colors[l].red, my_colors[l].green, my_colors[l].blue,
                            &my_colors[l].pixel);
          }

          if (check_trans(my_colors[l],trans_color_head) )
          {

            // Found a transparent color.  Leave it alone.
            leave_unchanged++;
            //fprintf(stderr,"Found transparency\n");
            // We never get here!
          }
        }

        // Use the map_intensity value if it's not a transparent
        // color we're dealing with.
        if (!leave_unchanged)
        {
          my_colors[l].red   = my_colors[l].red   * raster_map_intensity;
          my_colors[l].green = my_colors[l].green * raster_map_intensity;
          my_colors[l].blue  = my_colors[l].blue  * raster_map_intensity;
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

        if (debug_level & 16)
          fprintf(stderr,"Color allocated is %li  %i  %i  %i \n", my_colors[l].pixel,
                  my_colors[l].red, my_colors[l].blue, my_colors[l].green);
      }
    }

  if (check_interrupt(image, image_info,
                      &exception, &da, &pixmap, &gc, screen_width, screen_height))
  {
    return;
  }

#ifdef TIMING_DEBUG
  time_mark(0);
#endif  // TIMING_DEBUG

  if (debug_level & 16)
  {
    fprintf(stderr,"Image size %d %d\n", width, height);
#if (MagickLibVersion < 0x0540)
    fprintf(stderr,"Unique colors = %d\n", GetNumberColors(image, NULL));
#else   // MagickLibVersion < 0x0540
    fprintf(stderr,"Unique colors = %ld\n", GetNumberColors(image, NULL, &exception));
#endif  // MagickLibVersion < 0x0540
    fprintf(stderr,"XX: %ld YY:%ld Sx %f %d Sy %f %d\n", map_c_L, map_c_T,
            map_c_dx,(int) (map_c_dx / scale_x), map_c_dy, (int) (map_c_dy / scale_y));

#if (MagickLibVersion < 0x0540)
    fprintf(stderr,"is Gray Image = %i\n", IsGrayImage(image));
    fprintf(stderr,"is Monochrome Image = %i\n", IsMonochromeImage(image));
    //fprintf(stderr,"is Opaque Image = %i\n", IsOpaqueImage(image));
    //fprintf(stderr,"is PseudoClass = %i\n", image->storage_class == PseudoClass);
#else    // MagickLibVersion < 0x0540
    fprintf(stderr,"is Gray Image = %i\n", IsGrayImage( image, &exception ));
    fprintf(stderr,"is Monochrome Image = %i\n", IsMonochromeImage( image, &exception ));
    //fprintf(stderr,"is Opaque Image = %i\n", IsOpaqueImage( image, &exception ));
    //fprintf(stderr,"is PseudoClass = %i\n", image->storage_class == PseudoClass);
#endif   // MagickLibVersion < 0x0540

    fprintf(stderr,"image matte is %i\n", image->matte);
    fprintf(stderr,"Colorspace = %i\n", image->colorspace);
    if (image->colorspace == UndefinedColorspace)
    {
      fprintf(stderr,"Class Type = Undefined\n");
    }
    else if (image->colorspace == RGBColorspace)
    {
      fprintf(stderr,"Class Type = RGBColorspace\n");
    }
    else if (image->colorspace == GRAYColorspace)
    {
      fprintf(stderr,"Class Type = GRAYColorspace\n");
    }
    else if (image->colorspace == sRGBColorspace)
    {
      fprintf(stderr,"Class Type = sRGBColorspace\n");
    }
  }

#else   // HAVE_MAGICK

  // We don't have ImageMagick libs compiled in, so use the
  // XPM library instead, but only if we HAVE XPM.

#ifndef NO_XPM
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

  // Check whether file has .xpm or .xpm.Z or .xpm.gz at the end.
  // If not, don't use the XpmReadFileToImage call below.

  if (       !strstr(filenm,"xpm")
             && !strstr(filenm,"XPM")
             && !strstr(filenm,"Xpm") )
  {
    fprintf(stderr,
            "Error: File format not supported by XPM library: %s\n",
            filenm);
    return;
  }

  /*  XpmReadFileToImage is the call we wish to avoid if at all
   *  possible.  On large images this can take quite a while.  We
   *  check above to see whether the image is inside our viewport,
   *  and if not we skip loading the image.
   */
  if (! XpmReadFileToImage (XtDisplay (w), file, &xi, NULL, &atb) == XpmSuccess)
  {
    fprintf(stderr,"ERROR loading %s\n", file);
    if (xi)
    {
      XDestroyImage (xi);
    }
    return;
  }

  if (debug_level & 16)
  {
    fprintf(stderr,"Image size %d %d\n", (int)atb.width, (int)atb.height);
    fprintf(stderr,"XX: %ld YY:%ld Sx %f %d Sy %f %d\n",
            map_c_L,
            map_c_T,
            map_c_dx,
            (int) (map_c_dx / scale_x),
            map_c_dy,
            (int) (map_c_dy / scale_y));
  }

  HandlePendingEvents(app_context);
  if (interrupt_drawing_now)
  {
    if (xi)
    {
      XDestroyImage (xi);
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

  width  = atb.width;
  height = atb.height;
#else // NO_XPM
  fprintf(stderr,"Xastir was configured with neither XPM library nor (Image/Graphics)Magick, cannot display map %s\n",filenm);
#endif // NO_XPM
#endif  // HAVE_MAGICK

  // draw the image from the file out to the map screen

  // Get the border values for the X and Y for loops used
  // for the XFillRectangle call later.

  map_c_yc = (tp[0].y_lat + tp[1].y_lat) / 2;     // vert center of map as reference
  //  map_y_ctr = (long)(height / 2 +0.499);
  scale_x0 = get_x_scale(0,map_c_yc,scale_y);     // reference scaling at vert map center

  map_c_xc  = (tp[0].x_long + tp[1].x_long) / 2;  // hor center of map as reference
  map_x_ctr = (long)(width  / 2 +0.499);
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
  //    fprintf(stderr,"map top: %ld bottom: %ld\n",tp[0].y_lat,tp[1].y_lat);
  c_y_min = (unsigned long)(tp[0].y_lat + map_y_min * map_c_dy);   // top map inside screen coordinate

  //    // find the y coordinate nearest to the equator
  //    c_y = 90*60*60*100;         // Equator
  //    if ((c_y_min>c_y && c_y_max>c_y) || (c_y_min<c_y && c_y_max<c_y)) {
  //        if (abs(c_y_min-c_y) > abs(c_y_max-c_y))
  //            c_y = c_y_max;      // north
  //        else
  //            c_y = c_y_min;      // south
  //    }
  //    scale_x0 = get_x_scale(0,(long)c_y,scale_y); // calc widest map area in x

  //    if (map_proj != 1) {
  // calculate map pixel range in x direction that falls into screen area
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
  //    }
  //    for (scr_y = scr_y_min; scr_y <= scr_y_max;scr_y++) {       // screen lines
  //    }

  //  test = 1;           // DK7IN: debuging
  scr_yp = -1;
  scr_c_xr = SE_corner_longitude;
  c_dx = map_c_dx;                            // map pixel width
  scale_xa = scale_x0;                        // the compiler likes it ;-)

  //    for (map_y_0 = 0, c_y = tp[0].y_lat; map_y_0 < (long)height; map_y_0++, c_y += map_c_dy) {
  //        scr_y = (c_y - NW_corner_latitude) / scale_y;   // current screen position

  map_done = 0;
  map_act  = 0;
  map_seen = 0;
  scr_y = screen_height - 1;

#ifdef TIMING_DEBUG
  time_mark(0);
#endif  // TIMING_DEBUG


  if (check_interrupt(
#ifdef HAVE_MAGICK
  image, image_info, &exception,
#else  // HAVE_MAGICK
  xi,
#endif // HAVE_MAGICK
        &da, &pixmap, &gc, screen_width, screen_height))
  {
    return;
  }


  // loop over map pixel rows
  for (map_y_0 = map_y_min, c_y = (double)c_y_min;
       (map_y_0 <= map_y_max) || (map_proj == 1 && !map_done && scr_y < screen_height);
       map_y_0++, c_y += map_c_dy)
  {

    if (check_interrupt(
#ifdef HAVE_MAGICK
          image, image_info, &exception,
#else  // HAVE_MAGICK
          xi,
#endif // HAVE_MAGICK
          &da, &pixmap, &gc, screen_width, screen_height))
    {
      return;
    }

    scr_y = (c_y - NW_corner_latitude) / scale_y;
    if (scr_y != scr_yp)                    // don't do a row twice
    {
      scr_yp = scr_y;                     // remember as previous y
      if (map_proj == 1)                  // Transverse Mercator correction in x
      {
        scale_xa = get_x_scale(0,(long)c_y,scale_y); // recalc scale_x for current y
        c_dx = map_c_dx * scale_xa / scale_x0;       // adjusted map pixel width

        map_x_min = map_x_ctr - (map_c_xc - NW_corner_longitude) / c_dx;
        if (map_x_min < 0)
        {
          map_x_min = 0;
        }
        c_x_min = map_c_xc - (map_x_ctr - map_x_min) * c_dx;
        map_x_max = map_x_ctr - (map_c_xc - scr_c_xr) / c_dx;
        if (map_x_max > (long)width)
        {
          map_x_max = width;
        }
        scr_dx = (int) (c_dx / scale_x) + 1;    // at least 1 pixel wide
      }

      //            if (c_y == (double)c_y_min) {  // first call
      //                fprintf(stderr,"map: min %ld ctr %ld max %ld, c_dx %ld, c_x_min %ld, c_y_min %ld\n",map_x_min,map_x_ctr,map_x_max,(long)c_dx,c_x_min,c_y_min);
      //            }
      scr_xp = -1;
      // loop over map pixel columns
      map_act = 0;
      scale_x_nm = calc_dscale_x(0,(long)c_y) / 1852.0;  // nm per Xastir coordinate
      for (map_x = map_x_min, c_x = (double)c_x_min; map_x <= map_x_max; map_x++, c_x += c_dx)
      {
        scr_x = (c_x - NW_corner_longitude) / scale_x;
        if (scr_x != scr_xp)        // don't do a pixel twice
        {
          scr_xp = scr_x;         // remember as previous x
          if (map_proj == 1)      // Transverse Mercator correction in y
          {
            // DK7IN--
            dist = (90*60 - (c_y / 6000.0));   // equator distance in nm
            // ?? 180W discontinuity!  not done yet
            ew_ofs = (c_x - (double)map_c_xc) * scale_x_nm;  // EW offset from center in nm
            //corrfact = (map_y_0 - map_y_ctr)/(2*map_y_ctr);  // 0..50%
            //corrfact = fabs(ew_ofs/dist)*3.0;
            //corrfact = 1.0-1.0*(0.5*map_y_0 / map_y_ctr);
            corrfact = 1.0;
            c_y_a = (fabs(dist) - sqrt((double)(dist*dist - ew_ofs*ew_ofs)))*6000.0; // in Xastir units
            if (dist < 0)           // S
            {
              map_y = map_y_0 + (long)(corrfact*c_y_a / map_c_dy);  // coord per pixel
            }
            else                    // N
            {
              map_y = map_y_0 - (long)(corrfact*c_y_a / map_c_dy);
            }
            //                        if (test < 10) {
            //                            fprintf(stderr,"dist: %ldkm, ew_ofs: %ldkm, dy: %ldkm\n",(long)(1.852*dist),(long)(1.852*ew_ofs),(long)(1.852*c_y_a/6000.0));
            //                            fprintf(stderr,"  corrfact: %f, mapy0: %ld, mapy: %ld\n",corrfact,map_y_0,map_y);
            //                            test++;
            //                        }
          }
          else
          {
            map_y = map_y_0;
          }

          if (map_y >= 0 && map_y <= tp[1].img_y)   // check map boundaries in y direction
          {
            map_seen = 1;
            map_act = 1;    // detects blank screen rows (end of map)

            // DK7IN--

            //----- copy pixel from map to screen ---------------------
            //            if (c_y == (double)c_y_min && (scr_x < 5 || (c_x == (double)c_x_min))) {  // first call
            //                fprintf(stderr,"map: x %ld y %ld scr: x %ld y %ld dx %d, dy %d\n",map_x,map_y,scr_x,scr_y,scr_dx,scr_dy);
            //                fprintf(stderr,"color: %ld\n",XGetPixel (xi, map_x, map_y));
            //                // 65529
            //            }

            // now copy a pixel from the map image to the screen
#ifdef HAVE_MAGICK
            l = map_x + map_y * image->columns;
            trans_skip = 1; // possibly transparent
            if (image->storage_class == PseudoClass)
            {
              if ( trans_color_head &&
                   check_trans(my_colors[(int)index_pack[l]],trans_color_head) )
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
              if ( trans_color_head &&
                   check_trans(my_colors[0],trans_color_head) )
              {
                trans_skip = 1; // skip it
              }
              else
              {
                XSetForeground(XtDisplay(w), gc, my_colors[0].pixel);
                trans_skip = 0; // draw it
              }
            }
#else   // HAVE_MAGICK
            (void)XSetForeground (XtDisplay (w), gc, XGetPixel (xi, map_x, map_y));
#endif  // HAVE_MAGICK


            // Skip drawing if a transparent pixel
#ifdef HAVE_MAGICK
            if ( pixel_pack[l].opacity == 0 && !trans_skip  ) // skip transparent
#else   // HAVE_MAGICK
            if (!trans_skip)    // skip transparent
#endif  // HAVE_MAGICK

              (void)XFillRectangle (XtDisplay (w),pixmap,gc,scr_x,scr_y,scr_dx,scr_dy);
          } // check map boundaries in y direction
        }
      } // loop over map pixel columns
      if (map_seen && !map_act)
      {
        map_done = 1;
      }
    }
  } // loop over map pixel rows

#ifdef HAVE_MAGICK
  if (image)
  {
  DestroyImage(image);
  }
  if (image_info)
  {
  DestroyImageInfo(image_info);
  }
  DestroyExceptionInfo(&exception);
#else   // HAVE_MAGICK
  if (xi)
  {
  XDestroyImage (xi);
  }
#endif // HAVE_MAGICK

#ifdef TIMING_DEBUG
  time_mark(0);
#endif  // TIMING_DEBUG
#endif  // NO_GRAPHICS
}


